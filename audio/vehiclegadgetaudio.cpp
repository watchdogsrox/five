#include "vehiclegadgetaudio.h"
#include "northaudioengine.h"
#include "vehicleaudioentity.h"
#include "VehicleAi\Task\TaskVehicleAnimation.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "audiosoundtypes\sound.h"

AUDIO_VEHICLES_OPTIMISATIONS()

#if __BANK
extern bool g_DebugDrawTurret;
extern bool g_DebugDrawTowArm;
extern bool g_DebugDrawGrappling;
extern f32 g_GrapSoundAcceleration;
extern f32 g_GrapIntensity;
#endif

audCategory* audVehicleConvertibleRoof::s_PedRoofCategory = NULL;
audCategory* audVehicleConvertibleRoof::s_PlayerRoofCategory = NULL;
audCurve audVehicleTowTruckArm::sm_VehicleAngleToVolumeCurve;
audCurve audVehicleTowTruckArm::sm_HookSpeedToVolumeCurve;
audCurve audVehicleGrapplingHook::sm_VehicleAngleToVolumeCurve;
audCurve audVehicleGrapplingHook::sm_HookSpeedToVolumeCurve;
audCurve audVehicleGrapplingHook::sm_HookAngleToPitchCurve;

// ----------------------------------------------------------------
// audVehicleGadget constructor
// ----------------------------------------------------------------
audVehicleGadget::audVehicleGadget()
{
	m_Parent = NULL;
}

// ----------------------------------------------------------------
// audVehicleGadget Init
// ----------------------------------------------------------------
void audVehicleGadget::Init(audVehicleAudioEntity* parent)
{
	m_Parent = parent;
}

// ----------------------------------------------------------------
// audVehicleDigger constructor
// ----------------------------------------------------------------
audVehicleDigger::audVehicleDigger() : audVehicleGadget()
{
	for(u32 loop = 0; loop < DIGGER_JOINT_MAX; loop++)
	{
		m_JointSounds[loop] = NULL;
		m_JointSpeed[loop] = 0.0f;
		m_JointSpeedLastFrame[loop] = 0.0f;
		m_JointDesiredAcceleration[loop] = 0.0f;
		m_LastStopTime[loop] = 0;
		m_JointPlayTime[loop] = 0;
	}
}

// ----------------------------------------------------------------
// audVehicleDigger destructor
// ----------------------------------------------------------------
audVehicleDigger::~audVehicleDigger()
{
	if(m_Parent)
	{
		for(u32 loop = 0; loop < DIGGER_JOINT_MAX; loop++)
		{
			m_Parent->StopAndForgetSounds(m_JointSounds[loop]);
		}
	}
}

// ----------------------------------------------------------------
// audVehicleDigger Update
// ----------------------------------------------------------------
void audVehicleDigger::Update()
{
	if(m_Parent)
	{
		if(!m_DiggerSoundSet.IsInitialised())
		{
			m_DiggerSoundSet.Init(m_Parent->GetDiggerSoundSet());
		}

		if(m_DiggerSoundSet.IsInitialised())
		{
			for(u32 loop = 0; loop < DIGGER_JOINT_MAX; loop++)
			{
				audMetadataRef jointSound;
				audMetadataRef jointStopSound;

				switch(loop)
				{
				case DIGGER_JOINT_BASE:
					jointSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerBase", 0x5184E610));
					jointStopSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerBaseStop", 0x6A9D03E2));
					break;
				case DIGGER_JOINT_BOOM:
					jointSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerBoom", 0x9306CFEE));
					jointStopSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerBoomStop", 0xD6BB54F5));
					break;
				case DIGGER_JOINT_STICK:
					jointSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerStick", 0xE95C1C2C));
					jointStopSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerStickStop", 0xCDCE77CC));
					break;
				case DIGGER_JOINT_BUCKET:
					jointSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerBucket", 0x8B8D7DFE));
					jointStopSound = m_DiggerSoundSet.Find(ATSTRINGHASH("DiggerBucketStop", 0x88412F11));
					break;
				default:
					continue;
				}

				bool motorWasPlaying = m_JointSounds[loop] != NULL;

				if(abs(m_JointSpeed[loop]) > 0.0f && abs(m_JointDesiredAcceleration[loop]) > 0.0f)
				{
					if(!m_JointSounds[loop])
					{
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
						initParams.TrackEntityPosition = true;
						initParams.UpdateEntity = true;
						initParams.u32ClientVar = AUD_VEHICLE_SOUND_UNKNOWN;
						m_Parent->CreateAndPlaySound_Persistent(jointSound, &m_JointSounds[loop], &initParams);
						m_JointPlayTime[loop] = fwTimer::GetTimeInMilliseconds();
					}
				}
				else if(m_JointSounds[loop] && m_JointDesiredAcceleration[loop] == 0.0f)
				{
					m_Parent->StopAndForgetSounds(m_JointSounds[loop]);
				}

				// Triggering this manually rather than having an OnStop sound - we want to play the clunk when stopping and also when changing directions, but don't want
				// to spam the sound if both events happen in quick succession
				if((motorWasPlaying && !m_JointSounds[loop]) ||
					(m_JointSounds[loop] && ((m_JointSpeed[loop] > 0.0f && m_JointSpeedLastFrame[loop] < 0.0f) || (m_JointSpeed[loop] < 0.0f && m_JointSpeedLastFrame[loop] > 0.0f))))
				{
					u32 currentTime = fwTimer::GetTimeInMilliseconds();

					if(currentTime- m_LastStopTime[loop] > 250 &&
					   currentTime - m_JointPlayTime[loop] > 250)
					{
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
						initParams.TrackEntityPosition = true;
						m_Parent->CreateAndPlaySound(jointStopSound, &initParams);
						m_LastStopTime[loop] = fwTimer::GetTimeInMilliseconds();
					}
				}

				m_JointSpeedLastFrame[loop] = m_JointSpeed[loop];
				m_JointSpeed[loop] = 0.0f;
			}
		}
	}
}

// ----------------------------------------------------------------
// audVehicleDigger Reset
// ----------------------------------------------------------------
void audVehicleDigger::Reset()
{
	for(u32 loop = 0; loop < DIGGER_JOINT_MAX; loop++)
	{
		m_JointSpeed[loop] = 0.0f;
	}
}

// ----------------------------------------------------------------
// audVehicleDigger StopAllSounds
// ----------------------------------------------------------------
void audVehicleDigger::StopAllSounds()
{
	for(u32 loop = 0; loop < DIGGER_JOINT_MAX; loop++)
	{
		m_Parent->StopAndForgetSounds(m_JointSounds[loop]);
	}
}

// ----------------------------------------------------------------
// audVehicleForks constructor
// ----------------------------------------------------------------
audVehicleForks::audVehicleForks() : audVehicleGadget()
{
	m_MotorSound = NULL;
	m_LastStopTime = 0;
	m_MotorPlayTime = 0;
	m_DesiredAcceleration = 0.0f;
	Reset();
}

// ----------------------------------------------------------------
// audVehicleForks destructor
// ----------------------------------------------------------------
audVehicleForks::~audVehicleForks()
{
	if(m_Parent)
	{
		m_Parent->StopAndForgetSounds(m_MotorSound);
	}
}

// ----------------------------------------------------------------
// audVehicleForks Reset
// ----------------------------------------------------------------
void audVehicleForks::Reset()
{
	m_ForkliftSpeed = 0.0f;
}

// ----------------------------------------------------------------
// audVehicleForks StopAllSounds
// ----------------------------------------------------------------
void audVehicleForks::StopAllSounds()
{
	if(m_Parent)
	{
		m_Parent->StopAndForgetSounds(m_MotorSound);
	}
	
	m_MotorSound = NULL;
}

// ----------------------------------------------------------------
// audVehicleForks InitSoundSet
// ----------------------------------------------------------------
void audVehicleForks::InitSoundSet()
{
	m_SoundSet.Init(m_Parent->GetForkliftSoundSet());
}

// ----------------------------------------------------------------
// audVehicleForks Update
// ----------------------------------------------------------------
void audVehicleForks::Update()
{
	if(m_Parent)
	{
		if(!m_SoundSet.IsInitialised())
		{
			InitSoundSet();
		}

		bool motorWasPlaying = m_MotorSound != NULL;
		audMetadataRef forkliftSound = m_SoundSet.Find(ATSTRINGHASH("MotorLoop", 0xD15B9729));
		f32 minForkliftSpeed = GetMinMovementSpeed();

		if(m_Parent->GetVehicle()->IsEngineOn() && abs(m_ForkliftSpeed) > minForkliftSpeed)
		{
			if(!m_MotorSound)
			{
				if(fwTimer::GetTimeInMilliseconds() - m_LastStopTime > 100)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
					initParams.TrackEntityPosition = true;
					initParams.UpdateEntity = true;
					initParams.u32ClientVar = AUD_VEHICLE_SOUND_UNKNOWN;
					m_Parent->CreateAndPlaySound_Persistent(forkliftSound, &m_MotorSound, &initParams);
					m_MotorPlayTime = fwTimer::GetTimeInMilliseconds();
				}
			}
		}
		else if(m_MotorSound && m_DesiredAcceleration == 0.0f)
		{
			m_Parent->StopAndForgetSounds(m_MotorSound);
		}

		// Triggering this manually rather than having an OnStop sound - we want to play the clunk when stopping and also when changing directions, but don't want
		// to spam the sound if both events happen in quick succession
		if((motorWasPlaying && !m_MotorSound) ||
		   (m_MotorSound && ((m_ForkliftSpeed > minForkliftSpeed && m_ForkliftSpeedLastFrame < -minForkliftSpeed) || (m_ForkliftSpeed < -minForkliftSpeed && m_ForkliftSpeedLastFrame > minForkliftSpeed))))
		{
			u32 currentTime = fwTimer::GetTimeInMilliseconds();

			if(currentTime - m_LastStopTime > 250 &&
			   currentTime - m_MotorPlayTime > 250)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				initParams.TrackEntityPosition = true;
				m_Parent->CreateAndPlaySound(m_SoundSet.Find(ATSTRINGHASH("StopSound", 0x216A24CE)), &initParams);
				m_LastStopTime = fwTimer::GetTimeInMilliseconds();
			}
		}

		m_ForkliftSpeedLastFrame = m_ForkliftSpeed;
		m_ForkliftSpeed = 0.0f;
	}
}

// ----------------------------------------------------------------
// audVehicleTowTruckArm constructor
// ----------------------------------------------------------------
audVehicleTowTruckArm::audVehicleTowTruckArm() :
  audVehicleForks()
, m_LinkStressSound(NULL)
, m_ChainRattleSound(NULL)
, m_TowAngleChangeRate(0.0f)
, m_TowAngleChangeRateLastFrame(0.0f)
, m_TowedVehicleAngle(0.0f)
, m_TowedVehicleAngleLastFrame(0.0f)
, m_HookDistanceLastFrame(0.0f)
, m_FirstUpdate(true)
{
	m_TowAngleChangeRateSmoother.Init(1.0f);
	m_TowAngleVolSmoother.Init(0.1f, 0.1f);
	m_HookRattleVolumeSmoother.Init(0.2f, 0.02f, true);
	m_HookPosition.Zero();
}

// ----------------------------------------------------------------
// Tow truck arm destructor
// ----------------------------------------------------------------
audVehicleTowTruckArm::~audVehicleTowTruckArm()
{
	StopAllSounds();
}

// ----------------------------------------------------------------
// InitClass
// ----------------------------------------------------------------
void audVehicleTowTruckArm::InitClass()
{
	sm_VehicleAngleToVolumeCurve.Init(ATSTRINGHASH("TRAILER_LINKS_ANGLE_CR_TO_VOLUME", 0xB4D01D40));
	sm_HookSpeedToVolumeCurve.Init(ATSTRINGHASH("TOWTRUCK_HOOK_SPEED_TO_VOLUME", 0xD6083F6C));
}

// ----------------------------------------------------------------
// audVehicleTowTruckArm Reset
// ----------------------------------------------------------------
void audVehicleTowTruckArm::Reset()
{
	audVehicleForks::Reset();
	m_TowAngleChangeRate = 0.0f;
	m_TowAngleChangeRateLastFrame = 0.0f;
	m_TowedVehicleAngle = 0.0f;
	m_TowedVehicleAngleLastFrame = 0.0f;
	m_HookDistanceLastFrame = 0.0f;
	m_TowAngleChangeRateSmoother.Reset();
	m_TowAngleChangeRateSmoother.CalculateValue(0.0f);
	m_TowAngleVolSmoother.Reset();
	m_TowAngleVolSmoother.CalculateValue(0.0f);
	m_HookPosition.Zero();
	m_FirstUpdate = true;
}

// ----------------------------------------------------------------
// audVehicleTowTruckArm StopAllSounds
// ----------------------------------------------------------------
void audVehicleTowTruckArm::StopAllSounds()
{
	audVehicleForks::StopAllSounds();
	m_Parent->StopAndForgetSounds(m_LinkStressSound, m_ChainRattleSound);
}

// ----------------------------------------------------------------
// audVehicleTowTruckArm Update
// ----------------------------------------------------------------
void audVehicleTowTruckArm::Update()
{
	audVehicleForks::Update();

	f32 towedVehicleAngleDegrees = RtoD * m_TowedVehicleAngle;
	m_TowAngleChangeRate = Abs(towedVehicleAngleDegrees - m_TowedVehicleAngleLastFrame) * fwTimer::GetInvTimeStep();
	f32 towAngleChangeRateChangeRate = Abs(m_TowAngleChangeRate - m_TowAngleChangeRateLastFrame) * fwTimer::GetInvTimeStep();

	f32 smoothedChangeRate = m_TowAngleChangeRateSmoother.CalculateValue(m_TowAngleChangeRate);
	f32 linkVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_TowAngleVolSmoother.CalculateValue(sm_VehicleAngleToVolumeCurve.CalculateValue(towAngleChangeRateChangeRate)));

	if(!m_LinkStressSound)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
		initParams.UpdateEntity = true;
		initParams.TrackEntityPosition = true;
		initParams.Volume = linkVolume;
		m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("VehicleSwingAngleLoop", 0xC87B21EA)), &m_LinkStressSound, &initParams);
	}

	if(m_LinkStressSound)
	{
		m_LinkStressSound->FindAndSetVariableValue(ATSTRINGHASH("AngleChangeRate", 0x6952443F), smoothedChangeRate);
		m_LinkStressSound->SetRequestedVolume(linkVolume);
	}


	m_TowAngleChangeRateLastFrame = m_TowAngleChangeRate;
	m_TowedVehicleAngleLastFrame = towedVehicleAngleDegrees;

	Vec3V localHookPosition = m_Parent->GetVehicle()->GetTransform().UnTransform(VECTOR3_TO_VEC3V(m_HookPosition));
	f32 hookDistanceThisFrame = localHookPosition.GetXf();
	f32 hookSpeed = m_FirstUpdate? 0.0f : Abs(hookDistanceThisFrame - m_HookDistanceLastFrame) * fwTimer::GetInvTimeStep();
	f32 volumeLin = m_HookRattleVolumeSmoother.CalculateValue(sm_HookSpeedToVolumeCurve.CalculateValue(hookSpeed));

#if __BANK
	if(g_DebugDrawTowArm && m_Parent->IsPlayerVehicle())
	{
		char tempString[128];
		sprintf(tempString, "Hook Speed: %.02f", hookSpeed);
		grcDebugDraw::Text(Vector2(0.08f, 0.1f), Color32(255,255,255), tempString);

		sprintf(tempString, "Hook Volume: %.02f", volumeLin);
		grcDebugDraw::Text(Vector2(0.08f, 0.12f), Color32(255,255,255), tempString);
	}
#endif

	if(volumeLin > g_SilenceVolumeLin)
	{
		if(!m_ChainRattleSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("ChainRattleLoop", 0xB013A7C)), &m_ChainRattleSound, &initParams);
		}

		if(m_ChainRattleSound)
		{
			m_ChainRattleSound->SetRequestedPosition(m_HookPosition);
			m_ChainRattleSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volumeLin));
		}
	}
	else if(m_ChainRattleSound)
	{
		m_ChainRattleSound->StopAndForget();
	}

	m_HookDistanceLastFrame = hookDistanceThisFrame;
	m_FirstUpdate = false;
}

// ----------------------------------------------------------------
// audVehicleTowTruckArm InitSoundSet
// ----------------------------------------------------------------
void audVehicleTowTruckArm::InitSoundSet()
{
	m_SoundSet.Init(m_Parent->GetTowTruckSoundSet());
}

// ----------------------------------------------------------------
// audVehicleTurret constructor
// ----------------------------------------------------------------
audVehicleTurret::audVehicleTurret() : 
	  audVehicleGadget()
	, m_AimAngle(Quaternion::IdentityType)
	, m_PrevAimAngle(Quaternion::IdentityType)
{
	m_MotorSound = NULL;
	m_HorizontalSmoother.Init(0.1f, 0.1f);
	m_VerticalSmoother.Init(0.1f, 0.1f);
	m_SmoothersEnabled = true;
	Reset();
}

// ----------------------------------------------------------------
// audVehicleTurret destructor
// ----------------------------------------------------------------
audVehicleTurret::~audVehicleTurret()
{
	if(m_Parent)
	{
		m_Parent->StopAndForgetSounds(m_MotorSound);
	}
}

// ----------------------------------------------------------------
// audVehicleTurret InitSoundSet
// ----------------------------------------------------------------
void audVehicleTurret::Init(audVehicleAudioEntity* parent)
{
	audVehicleGadget::Init(parent);

	if(parent)
	{
		const u32 modelNameHash = parent->GetVehicleModelNameHash();

		if(modelNameHash == ATSTRINGHASH("BARRAGE", 0xF34DFB25) ||
		   modelNameHash == ATSTRINGHASH("CHERNOBOG", 0xD6BC7523) ||
		   modelNameHash == ATSTRINGHASH("LIMO2", 0xF92AEC4D) ||
		   modelNameHash == ATSTRINGHASH("MINITANK", 0xB53C6C52))
		{
			m_SmoothersEnabled = false;
		}
	}
}

// ----------------------------------------------------------------
// audVehicleTurret Reset
// ----------------------------------------------------------------
void audVehicleTurret::Reset()
{
	m_TurretAngularVelocity.Zero();
	m_TurretLocalPosition.Zero();
	m_AimAngle = Quaternion(Quaternion::IdentityType);
	m_PrevAimAngle = Quaternion(Quaternion::IdentityType);
	m_HorizontalSmoother.Reset();
	m_VerticalSmoother.Reset();
	m_HorizontalSmoother.CalculateValue(0.f);
	m_VerticalSmoother.CalculateValue(0.f);
}

// ----------------------------------------------------------------
// audVehicleForks StopAllSounds
// ----------------------------------------------------------------
void audVehicleTurret::StopAllSounds()
{
	if(m_Parent)
	{
		m_Parent->StopAndForgetSounds(m_MotorSound);
	}

	m_MotorSound = NULL;
}

// ----------------------------------------------------------------
// audVehicleTurret SetTurretLocalPosition
// ----------------------------------------------------------------
void audVehicleTurret::SetTurretLocalPosition(const Vector3 localPosition)
{
	m_TurretLocalPosition = localPosition;

	// Hacky fix for badly positioned turret bones on Barrage
	if(m_Parent && m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("BARRAGE", 0xF34DFB25))
	{
		m_TurretLocalPosition.SetY(0.f);
	}
}

// ----------------------------------------------------------------
// audVehicleTurret Update
// ----------------------------------------------------------------
void audVehicleTurret::Update()
{
	if(m_Parent)
	{
		if(!m_TurretSoundSet.IsInitialised())
		{
			m_TurretSoundSet.Init(m_Parent->GetTurretSoundSet());
		}

#if GTA_REPLAY
		// If we're in replay, we get angular velocity from replay packets
		if(!CReplayMgr::IsEditModeActive())
#endif
		{
			QuatV qOrientationOld = QUATERNION_TO_QUATV(m_PrevAimAngle);
			QuatV qOrientationNew = QUATERNION_TO_QUATV(m_AimAngle);
			Vec3V vAngularVelocity;

			if(!IsCloseAll(qOrientationOld, qOrientationNew, ScalarV(V_FLT_SMALL_6)))
			{
				QuatV qOrientationChange = Multiply(qOrientationNew, Invert(qOrientationOld));
				Vec3V vAxis = GetUnitDirectionSafe(qOrientationChange);
				ScalarV scAngle = CanonicalizeAngle(GetAngle(qOrientationChange));
				vAngularVelocity = Scale(vAxis, scAngle * ScalarV(fwTimer::GetInvTimeStep()));
			}
			else
			{
				//Clear the angular velocity.
				vAngularVelocity = Vec3V(V_ZERO);
			}

			m_TurretAngularVelocity = VEC3V_TO_VECTOR3(vAngularVelocity);
		}	

		rage::Vector3 horizVelocity = m_TurretAngularVelocity;
		rage::Vector3 vertVelocity = m_TurretAngularVelocity;

		vertVelocity.SetZ(0.0f);
		horizVelocity.SetX(0.0f);
		horizVelocity.SetY(0.0f);

		f32 horizontalSpeed = m_SmoothersEnabled? m_HorizontalSmoother.CalculateValue(horizVelocity.Mag()) : horizVelocity.Mag();
		f32 verticalSpeed = m_SmoothersEnabled? m_VerticalSmoother.CalculateValue(vertVelocity.Mag()) : vertVelocity.Mag();

#if __BANK
		if(g_DebugDrawTurret && m_Parent->IsPlayerVehicle())
		{
			char tempString[128];
			sprintf(tempString, "Horizontal Velocity: %.02f", horizontalSpeed);
			grcDebugDraw::Text(Vector2(0.08f, 0.1f), Color32(255,255,255), tempString);			
			sprintf(tempString, "Vertical Velocity: %.02f", verticalSpeed);
			grcDebugDraw::Text(Vector2(0.08f, 0.12f), Color32(255,255,255), tempString);
		}
#endif

		audMetadataRef turretMotorSound = m_TurretSoundSet.Find(ATSTRINGHASH("MotorLoop", 0xD15B9729));

		if(horizontalSpeed > 0.0f || verticalSpeed > 0.0f)
		{
			if(!m_MotorSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				m_Parent->CreateAndPlaySound_Persistent(turretMotorSound, &m_MotorSound, &initParams);
			}

			if(m_MotorSound)
			{
				m_MotorSound->SetRequestedPosition(m_Parent->GetVehicle()->TransformIntoWorldSpace(m_TurretLocalPosition));
				m_MotorSound->SetRequestedDopplerFactor(m_Parent->IsFocusVehicle()? 0.f : 1.f); // High pitched turret servos sound weird with doppler (eg. camera moving close to vehicle)

				if(m_MotorSound->GetPlayState() == AUD_SOUND_PLAYING)
				{
					m_MotorSound->FindAndSetVariableValue(ATSTRINGHASH("HorizontalVelocity", 0x8378EC7C), horizontalSpeed);
					m_MotorSound->FindAndSetVariableValue(ATSTRINGHASH("VerticalVelocity", 0xF2FDB384), verticalSpeed);
				}
			}
		}
		else if(m_MotorSound)
		{
			m_MotorSound->StopAndForget();
		}

		m_PrevAimAngle = m_AimAngle;
	}
}

// ----------------------------------------------------------------
// audVehicleConvertibleRoof constructor
// ----------------------------------------------------------------
audVehicleConvertibleRoof::audVehicleConvertibleRoof() : audVehicleGadget()
{
	for(u32 component = 0; component < AUD_ROOF_COMPONENT_MAX; component++)
	{
		for(u32 soundType = 0; soundType < AUD_ROOF_SOUND_TYPE_MAX; soundType++)
		{
			m_RoofSounds[component][soundType][AUD_ROOF_INTERIOR] = NULL;
			m_RoofSounds[component][soundType][AUD_ROOF_EXTERIOR] = NULL;
		}
	}

	m_PlayerNPCVolumeSmoother.Init(0.01f, true);
	m_LastRoofActiveTime = 0;
	m_RoofWasActive = false;
}

// ----------------------------------------------------------------
// audVehicleConvertibleRoof destructor
// ----------------------------------------------------------------
audVehicleConvertibleRoof::~audVehicleConvertibleRoof()
{
	StopAllSounds();
}

// ----------------------------------------------------------------
// Handle an animation trigger
// ----------------------------------------------------------------
bool audVehicleConvertibleRoof::HandleAnimationTrigger(u32 trigger)
{
	audConvertibleRoofComponent roofComponent = AUD_ROOF_COMPONENT_MAX;
	u32 loopSound, startSound, stopSound;
	loopSound = startSound = stopSound = g_NullSoundHash;

	bool startTrigger = true;
	bool handled = true;

	switch(trigger)
	{
	case 1287555492u: //WINDOW_START
	case 1540290171u: //WINDOW_STOP
		roofComponent = AUD_ROOF_WINDOWS;
		loopSound = ATSTRINGHASH("WINDOW_LOOP", 0x942D5BE8);
		startSound = ATSTRINGHASH("WINDOW_START", 0x4CBE89A4);
		stopSound = ATSTRINGHASH("WINDOW_STOP", 0x5BCEF67B);

		if(trigger == 1540290171)//WINDOW_STOP
		{
			startTrigger = false;
		}
		break;
	case 3854694261u: //FLAPS_START
	case 225565905u: //FLAPS_STOP
		roofComponent = AUD_ROOF_FLAPS;
		loopSound = ATSTRINGHASH("FLAPS_LOOP", 0xE6925113);
		startSound = ATSTRINGHASH("FLAPS_START", 0xE5C1F775);
		stopSound = ATSTRINGHASH("FLAPS_STOP", 0xD71DCD1);

		if(trigger == 225565905)//FLAPS_STOP
		{
			startTrigger = false;
		}
		break;
	case 625550937u: //BOOT_START
	case 2604331104u: //BOOT_STOP
		roofComponent = AUD_ROOF_BOOT_OPEN;
		loopSound = ATSTRINGHASH("BOOT_OPEN_LOOP", 0x9382C6E4);
		startSound = ATSTRINGHASH("BOOT_OPEN_START", 0x4171726);
		stopSound = ATSTRINGHASH("BOOT_OPEN_STOP", 0x6BE75653);

		if(trigger == 2604331104)//BOOT_STOP
		{
			startTrigger = false;
		}
		break;
	case 3242699650u: //ROOF_START
	case 3397943600u: //ROOF_STOP
		roofComponent = AUD_ROOF_MAIN;
		loopSound = ATSTRINGHASH("ROOF_LOOP", 0x2B10F3AA);
		startSound = ATSTRINGHASH("ROOF_START", 0xC147AB82);
		stopSound = ATSTRINGHASH("ROOF_STOP", 0xCA888130);

		if(trigger == 3397943600)//ROOF_STOP
		{
			startTrigger = false;
		}
		break;
	case 2254089658u: //BOOT_CLOSE_START
	case 2855817943u: //BOOT_CLOSE_STOP
		roofComponent = AUD_ROOF_BOOT_CLOSE;
		loopSound = ATSTRINGHASH("BOOT_CLOSE_LOOP", 0x52E894EA);
		startSound = ATSTRINGHASH("BOOT_CLOSE_START", 0x865AADBA);
		stopSound = ATSTRINGHASH("BOOT_CLOSE_STOP", 0xAA3852D7);

		if(trigger == 2855817943)//BOOT_CLOSE_STOP
		{
			startTrigger = false;
		}
		break;
	default:
		handled = false;
		break;
	}

	if(handled)
	{
		if(!m_ConvertibleRoofSoundSet.IsInitialised())
		{
			m_ConvertibleRoofSoundSet.Init(m_Parent->GetConvertibleRoofSoundSet());
		}

		if(!m_ConvertibleRoofSoundSetInterior.IsInitialised())
		{
			m_ConvertibleRoofSoundSetInterior.Init(m_Parent->GetConvertibleRoofInteriorSoundSet());
		}

		s32 roofState = m_Parent->GetVehicle()->GetConvertibleRoofState();

		// Roof stuck sounds are handled separately, so ignore any triggers if we're in this state
		if(roofState != CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_RAISED && roofState != CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_LOWERED)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();

			// Anims are tagged based on the lowering roof sequence, so reverse the start/stop tags if we're playing backwards
			if(roofState == CTaskVehicleConvertibleRoof::STATE_RAISING)
			{
				startTrigger = !startTrigger;
			}

			if(!startTrigger)
			{
				if(!m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_STOP][AUD_ROOF_INTERIOR])
				{
					m_Parent->CreateAndPlaySound_Persistent(GetRoofSound(stopSound, AUD_ROOF_INTERIOR), &m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_STOP][0], &initParams);
				}
				
				if(!m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_STOP][AUD_ROOF_EXTERIOR])
				{
					m_Parent->CreateAndPlaySound_Persistent(GetRoofSound(stopSound, AUD_ROOF_EXTERIOR), &m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_STOP][1], &initParams);
				}
				
				m_Parent->StopAndForgetSounds(m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_INTERIOR], m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_EXTERIOR]);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_Parent->GetConvertibleRoofSoundSet(), stopSound, &initParams, m_Parent->GetOwningEntity()));
				UpdateVolumes();
			}
			else
			{
				if(!m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_INTERIOR])
				{
					m_Parent->CreateAndPlaySound_Persistent(GetRoofSound(loopSound, AUD_ROOF_INTERIOR), &m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_INTERIOR], &initParams);
				}

				if(!m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_EXTERIOR])
				{
					m_Parent->CreateAndPlaySound_Persistent(GetRoofSound(loopSound, AUD_ROOF_EXTERIOR), &m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_EXTERIOR], &initParams);
				}

				if(!m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_START][AUD_ROOF_INTERIOR])
				{
					m_Parent->CreateAndPlaySound_Persistent(GetRoofSound(startSound, AUD_ROOF_INTERIOR), &m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_START][AUD_ROOF_INTERIOR], &initParams);
				}

				if(!m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_START][AUD_ROOF_EXTERIOR])
				{
					m_Parent->CreateAndPlaySound_Persistent(GetRoofSound(startSound, AUD_ROOF_EXTERIOR), &m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_START][AUD_ROOF_EXTERIOR], &initParams);
				}					

				REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_Parent->GetConvertibleRoofSoundSet(), loopSound, &initParams, m_RoofSounds[roofComponent][AUD_ROOF_SOUND_TYPE_LOOP][AUD_ROOF_EXTERIOR], m_Parent->GetOwningEntity(), eNoGlobalSoundEntity);)
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_Parent->GetConvertibleRoofSoundSet(), startSound, &initParams, m_Parent->GetOwningEntity());)
				m_RoofWasActive = true;
				UpdateVolumes();
			}
		}

		return true;
	}

	return false;
}

// ----------------------------------------------------------------
// Get a roof sound (may be different depending on whether we're 
// in first person mode or not)
// ----------------------------------------------------------------
audMetadataRef audVehicleConvertibleRoof::GetRoofSound(u32 soundNameHash, audConvertibleRoofCameraType interiorType)
{	
	if(interiorType == AUD_ROOF_INTERIOR)
	{
		return m_ConvertibleRoofSoundSetInterior.Find(soundNameHash);
	}
	else
	{
		return m_ConvertibleRoofSoundSet.Find(soundNameHash);
	}
}

// ----------------------------------------------------------------
// Stop all roof sounds
// ----------------------------------------------------------------
void audVehicleConvertibleRoof::StopAllSounds()
{
	if(m_Parent)
	{
		for(u32 component = 0; component < AUD_ROOF_COMPONENT_MAX; component++)
		{
			for(u32 soundType = 0 ; soundType < AUD_ROOF_SOUND_TYPE_MAX; soundType++)
			{
				m_Parent->StopAndForgetSounds(m_RoofSounds[component][soundType][AUD_ROOF_INTERIOR], m_RoofSounds[component][soundType][AUD_ROOF_EXTERIOR]);
			}			
		}
	}
}

// ----------------------------------------------------------------
// InitClass
// ----------------------------------------------------------------
void audVehicleConvertibleRoof::InitClass()
{
	s_PlayerRoofCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PLAYER_VEHICLE_CONVERTIBLE_ROOF", 0xED39E59D));
	s_PedRoofCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PED_VEHICLE_CONVERTIBLE_ROOF", 0x11F1827));
}

f32 g_ConvertibleRoofForwardOffset = 2.5f;
f32 g_ConvertibleRoofForwardScale = 4.f;
f32 g_ConvertibleRoofUpScale = 1.f;

// ----------------------------------------------------------------
// audVehicleConvertibleRoof UpdateVolumes
// ----------------------------------------------------------------
void audVehicleConvertibleRoof::UpdateVolumes()
{
	if(m_Parent && s_PedRoofCategory && s_PlayerRoofCategory)
	{
		bool interiorSoundsActive = false;

		if(m_Parent && 
			m_Parent->IsPlayerVehicle() && 
			m_ConvertibleRoofSoundSetInterior.IsInitialised() && 
			audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
		{
			interiorSoundsActive = true;
		}

		f32 playerNPCBalance = m_PlayerNPCVolumeSmoother.CalculateValue(m_Parent->IsPlayerVehicle()? 1.0f : 0.0f);
		f32 volume = s_PedRoofCategory->GetVolume() + (playerNPCBalance * (s_PlayerRoofCategory->GetVolume() - s_PedRoofCategory->GetVolume()));
		f32 volumeExterior = interiorSoundsActive? g_SilenceVolume : volume;
		f32 volumeInterior = interiorSoundsActive? volume : g_SilenceVolume;

		for(u32 component = 0; component < AUD_ROOF_COMPONENT_MAX; component++)
		{
			for(u32 soundType = 0; soundType < AUD_ROOF_SOUND_TYPE_MAX; soundType++)
			{
				Vec3V position = Vec3V(V_ZERO);

				switch(component)
				{
				case AUD_ROOF_MAIN:
					{
						f32 openness = m_Parent->GetVehicle()->GetMoveVehicle().GetMechanismPhase();
						position.SetYf(g_ConvertibleRoofForwardOffset + (-Cosf(HALF_PI * (1.f - openness)) * g_ConvertibleRoofForwardScale));
						position.SetZf(Sinf(HALF_PI * (1.f - openness)) * g_ConvertibleRoofUpScale);
					}					
					break;
				
				case AUD_ROOF_BOOT_OPEN:
				case AUD_ROOF_BOOT_CLOSE:
					position.SetYf(-1.f);
					break;
				case AUD_ROOF_FLAPS:
					position.SetYf(-0.25f);
					break;
				case AUD_ROOF_WINDOWS:
				default:
					break;
				};


				position = m_Parent->GetVehicle()->TransformIntoWorldSpace(position);

				if(m_RoofSounds[component][soundType][AUD_ROOF_INTERIOR])
				{
					m_RoofSounds[component][soundType][AUD_ROOF_INTERIOR]->SetRequestedVolume(volumeInterior);
					m_RoofSounds[component][soundType][AUD_ROOF_INTERIOR]->SetRequestedPosition(position);
				}

				if(m_RoofSounds[component][soundType][AUD_ROOF_EXTERIOR])
				{
					m_RoofSounds[component][soundType][AUD_ROOF_EXTERIOR]->SetRequestedVolume(volumeExterior);
					m_RoofSounds[component][soundType][AUD_ROOF_EXTERIOR]->SetRequestedPosition(position);
				}
			}
		}
	}	
}

// ----------------------------------------------------------------
// audVehicleConvertibleRoof Update
// ----------------------------------------------------------------
void audVehicleConvertibleRoof::Update()
{	
	// In the case that the roof finishes its animation without giving us a final stop trigger, time out after a short period
	s32 roofState = m_Parent->GetVehicle()->GetConvertibleRoofState();

	if(roofState != CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_RAISED && 
	   roofState != CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_LOWERED &&
	   roofState != CTaskVehicleConvertibleRoof::STATE_RAISED &&
	   roofState != CTaskVehicleConvertibleRoof::STATE_LOWERED)
	{		
		m_LastRoofActiveTime = fwTimer::GetTimeInMilliseconds();
		m_RoofWasActive = true;
		UpdateVolumes();
	}
	else if(m_RoofWasActive)
	{
		if(fwTimer::GetTimeInMilliseconds() - m_LastRoofActiveTime > 100)
		{
			StopAllSounds();
		}
	}
}

// ----------------------------------------------------------------
// audHeliGrapplingHook constructor
// ----------------------------------------------------------------
audVehicleGrapplingHook::audVehicleGrapplingHook() :
audVehicleGadget()
	, m_LinkStressSound(NULL)
	, m_ChainRattleSound(NULL)
	, m_TowAngleChangeRate(0.0f)
	, m_TowAngleChangeRateLastFrame(0.0f)
	, m_TowedVehicleAngle(0.0f)
	, m_TowedVehicleAngleLastFrame(55.0f)
	, m_HookDistanceLastFrame(0.0f)
	, m_FirstUpdate(true)
	, m_EntityAttached(false)
{
	m_TowAngleChangeRateSmoother.Init(0.5f, 0.5f, 0.0f, 100.0f);
	m_HookRattleVolumeSmoother.Init(0.2f, 0.02f, true);
	m_HookPosition.Zero();
}

// ----------------------------------------------------------------
// Tow truck arm destructor
// ----------------------------------------------------------------
audVehicleGrapplingHook::~audVehicleGrapplingHook()
{
	StopAllSounds();
}

// ----------------------------------------------------------------
// InitClass
// ----------------------------------------------------------------
void audVehicleGrapplingHook::InitClass()
{
	sm_VehicleAngleToVolumeCurve.Init(ATSTRINGHASH("HELI_HOOK_ANGLE_TO_VOLUME", 0xF6A1AFFD));
	sm_HookSpeedToVolumeCurve.Init(ATSTRINGHASH("HELI_HOOK_SPEED_TO_VOLUME", 0x707ACDBC));
	sm_HookAngleToPitchCurve.Init(ATSTRINGHASH("HELI_HOOK_ANGLE_TO_PITCH", 0x3859041));
}

// ----------------------------------------------------------------
// audHeliGrapplingHook Reset
// ----------------------------------------------------------------
void audVehicleGrapplingHook::Reset()
{
	m_TowAngleChangeRate = 0.0f;
	m_TowAngleChangeRateLastFrame = 0.0f;
	m_TowedVehicleAngle = 0.0f;
	m_TowedVehicleAngleLastFrame = 55.0f;
	m_HookDistanceLastFrame = 0.0f;
	m_TowAngleChangeRateSmoother.Reset();
	m_TowAngleChangeRateSmoother.CalculateValue(0.0f);
	m_HookPosition.Zero();
	m_FirstUpdate = true;
}

// ----------------------------------------------------------------
// audHeliGrapplingHook StopAllSounds
// ----------------------------------------------------------------
void audVehicleGrapplingHook::StopAllSounds()
{
	if(m_Parent)
		m_Parent->StopAndForgetSounds(m_LinkStressSound, m_ChainRattleSound);
}

// ----------------------------------------------------------------
// audHeliGrapplingHook Update
// ----------------------------------------------------------------
void audVehicleGrapplingHook::Update()
{
	if(!m_SoundSet.IsInitialised() BANK_ONLY(|| m_FirstUpdate))
	{
		InitSoundSet();
	}
	
	if(!m_Parent)
		return;

	f32 towedVehicleAngleDegrees = RtoD * m_TowedVehicleAngle;
	m_TowAngleChangeRate = Abs(towedVehicleAngleDegrees - m_TowedVehicleAngleLastFrame) * fwTimer::GetInvTimeStep();
	m_TowAngleChangeRate = Clamp(m_TowAngleChangeRate, 0.0f, 40.0f);
	f32 smoothedChangeRate = m_TowAngleChangeRateSmoother.CalculateValue(m_TowAngleChangeRate);
	f32 linkVolume = audDriverUtil::ComputeDbVolumeFromLinear(sm_VehicleAngleToVolumeCurve.CalculateValue(smoothedChangeRate));
	s32 pitch = (s32)sm_HookAngleToPitchCurve.CalculateValue(smoothedChangeRate);

	if(m_EntityAttached)
	{
		if(!m_LinkStressSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			initParams.TrackEntityPosition = true;
			initParams.Volume = linkVolume;
			m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("VehicleSwingAngleLoop", 0xC87B21EA)), &m_LinkStressSound, &initParams);
		}

		if(m_LinkStressSound)
		{
			m_LinkStressSound->SetRequestedVolume(linkVolume);
			m_LinkStressSound->SetRequestedPitch(pitch);
		}
	}
	else if(m_LinkStressSound)
	{
		m_LinkStressSound->StopAndForget();
	}

	m_TowAngleChangeRateLastFrame = m_TowAngleChangeRate;
	m_TowedVehicleAngleLastFrame = towedVehicleAngleDegrees;

	Vec3V localHookPosition = m_Parent->GetVehicle()->GetTransform().UnTransform(VECTOR3_TO_VEC3V(m_HookPosition));
	f32 hookDistanceThisFrame = localHookPosition.GetXf();
	f32 hookSpeed = m_FirstUpdate? 0.0f : Abs(hookDistanceThisFrame - m_HookDistanceLastFrame) * fwTimer::GetInvTimeStep();
	f32 volumeLin = m_HookRattleVolumeSmoother.CalculateValue(sm_HookSpeedToVolumeCurve.CalculateValue(hookSpeed));

#if __BANK
	if(g_DebugDrawGrappling && m_Parent->IsPlayerVehicle())
	{
		char tempString[128];
		sprintf(tempString, "Volume           : %.02f", volumeLin);
		grcDebugDraw::Text(Vector2(0.08f, 0.10f), Color32(255,255,255), tempString);

		sprintf(tempString, "Pitch            : %d", pitch);
		grcDebugDraw::Text(Vector2(0.08f, 0.12f), Color32(255,255,255), tempString);

		sprintf(tempString, "AngleChangeRate    : %.02f", smoothedChangeRate);
		grcDebugDraw::Text(Vector2(0.08f, 0.14f), Color32(255,255,255), tempString);

		sprintf(tempString, "hookSpeed          : %.02f", hookSpeed);
		grcDebugDraw::Text(Vector2(0.08f, 0.16f), Color32(255,255,255), tempString);

		Displayf("AngleChangeRate    %.02f     Angle                    %.02f", smoothedChangeRate, towedVehicleAngleDegrees);
	}
#endif
	
	if(volumeLin > g_SilenceVolumeLin && !m_EntityAttached)
	{
		if(!m_ChainRattleSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("ChainRattleLoop", 0xB013A7C)), &m_ChainRattleSound, &initParams);
		}

		if(m_ChainRattleSound)
		{
			m_ChainRattleSound->SetRequestedPosition(m_HookPosition);
			m_ChainRattleSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volumeLin));
		}
	}
	else if(m_ChainRattleSound)
	{
		m_ChainRattleSound->StopAndForget();
	}

	m_HookDistanceLastFrame = hookDistanceThisFrame;
	m_FirstUpdate = false;
}

// ----------------------------------------------------------------
// audHeliGrapplingHook InitSoundSet
// ----------------------------------------------------------------
void audVehicleGrapplingHook::InitSoundSet()
{
	m_SoundSet.Init(ATSTRINGHASH("HeliGrapplingHook", 0x162BBD9));
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet constructor
// ----------------------------------------------------------------
audVehicleGadgetMagnet::audVehicleGadgetMagnet() : audVehicleGadget()
{
	m_MagnetHumSound = NULL;
	m_IsMagnetActive = false;
	m_MagnetPosition = Vec3V(V_ZERO);
	m_MagnetTargetDistance = 0.0f;
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet destructor
// ----------------------------------------------------------------
audVehicleGadgetMagnet::~audVehicleGadgetMagnet()
{
	StopAllSounds();
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet StopAllSounds
// ----------------------------------------------------------------
void audVehicleGadgetMagnet::StopAllSounds()
{
	m_Parent->StopAndForgetSounds(m_MagnetHumSound);
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet InitSoundSet
// ----------------------------------------------------------------
void audVehicleGadgetMagnet::InitSoundSet()
{
	m_SoundSet.Init(ATSTRINGHASH("DLC_HEIST_FLEECA_SOUNDSET", 0xD4617EF5));
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet Update
// ----------------------------------------------------------------
void audVehicleGadgetMagnet::Update()
{
	if(!m_SoundSet.IsInitialised())
	{
		InitSoundSet();
	}

	if(m_IsMagnetActive)
	{
		if(!m_MagnetHumSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("Magnet_Hum", 0xE55A5C8F)), &m_MagnetHumSound, &initParams);
		}

		if(m_MagnetHumSound)
		{
			m_MagnetHumSound->SetRequestedPosition(m_MagnetPosition);
		}
	}
	else if(m_MagnetHumSound)
	{
		m_MagnetHumSound->StopAndForget();
	}
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet UpdateMagnetStatus
// ----------------------------------------------------------------
void audVehicleGadgetMagnet::UpdateMagnetStatus(Vec3V_In position, bool magnetOn, f32 targetDistance) 
{ 
	if(magnetOn != m_IsMagnetActive)
	{
		audDisplayf("Switching vehicle magnet %s, position: %.02f, %.02f, %.02f, targetDistance: %.02f", magnetOn? "ON" : "OFF", position.GetXf(), position.GetYf(), position.GetZf(), targetDistance);
	}

	m_MagnetPosition = position; 
	m_IsMagnetActive = magnetOn; 
	m_MagnetTargetDistance = targetDistance; 
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet Attach
// ----------------------------------------------------------------
void audVehicleGadgetMagnet::TriggerMagnetAttach(CEntity* attachedEntity)
{
	if(!m_SoundSet.IsInitialised())
	{
		InitSoundSet();
	}

	audDisplayf("Triggering magnet attach");
	m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("Magnet_Attach", 0xCB4E4E8B)), attachedEntity, NULL, true, true);
}

// ----------------------------------------------------------------
// audVehicleGadgetMagnet Detach
// ----------------------------------------------------------------
void audVehicleGadgetMagnet::TriggerMagnetDetach(CEntity* attachedEntity)
{
	if(!m_SoundSet.IsInitialised())
	{
		InitSoundSet();
	}

	audDisplayf("Triggering magnet detach");
	m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("Magnet_Detach", 0x5B0F0663)), attachedEntity, NULL, true, true);
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler constructor
// ----------------------------------------------------------------
audVehicleGadgetDynamicSpoiler::audVehicleGadgetDynamicSpoiler() : 
	  m_RaiseSound(NULL)
	, m_RotateSound(NULL)
	, m_StrutState(AUD_STRUT_LOWERED)
	, m_AngleState(AUD_SPOILER_ROTATED_BACKWARDS)
	, m_Frozen(false)
{	
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler destructor
// ----------------------------------------------------------------
audVehicleGadgetDynamicSpoiler::~audVehicleGadgetDynamicSpoiler()
{
	StopAllSounds();
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler InitSoundSet
// ----------------------------------------------------------------
void audVehicleGadgetDynamicSpoiler::Init(audVehicleAudioEntity* parent)
{
	audVehicleGadget::Init(parent);
	InitSoundSet();
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler InitSoundSet
// ----------------------------------------------------------------
void audVehicleGadgetDynamicSpoiler::InitSoundSet()
{
	const u32 modelNameHash = m_Parent->GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("Nero", 0x3DA47243) || modelNameHash == ATSTRINGHASH("Nero2", 0x4131F378))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_ImportExport_Nero_Spoiler_Sounds", 0x75089C79));
	}
	else if(modelNameHash == ATSTRINGHASH("XA21", 0x36B4A8A9))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_GR_XA21_Spoiler_Sounds", 0xF9C0B635));
	}
	else if (modelNameHash == ATSTRINGHASH("Ignus2", 0x39085F47))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_SECURITY_ignus_Spoiler_Sounds", 0x8044B4B4));
	}
	else if (modelNameHash == ATSTRINGHASH("Ignus", 0xA9EC907B))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_SECURITY_ignus_Spoiler_Sounds", 0x8044B4B4));
	}	
	else if (modelNameHash == ATSTRINGHASH("FURIA", 0x3944D5A0))
	{
		m_SoundSet.Init(ATSTRINGHASH("dlc_ch_furia_spoiler_sounds", 0xC381E826));
	}
	else if (modelNameHash == ATSTRINGHASH("italirsx", 0xBB78956A))
	{
		m_SoundSet.Init(ATSTRINGHASH("dlc_hei4_italirsx_spoiler_sounds", 0x4024E30));
	}
	else if(modelNameHash == ATSTRINGHASH("COMET6", 0x991EFC04) || modelNameHash == ATSTRINGHASH("comet7", 0x440851D8) )
	{
		m_SoundSet.Init(ATSTRINGHASH("dlc_tuner_comet6_spoiler_sounds", 0x7F084F8B));
	}
	else if(modelNameHash == ATSTRINGHASH("COMET7", 0x440851D8))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_SECURITY_comet7_spoiler_sounds", 0x63B65786));
	}
	else if (modelNameHash == ATSTRINGHASH("tenf", 0xCAB6E261))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_SUM2_tenf_spoiler_sounds", 0xDC281212));
	}
	else if (modelNameHash == ATSTRINGHASH("tenf2", 0x10635A0E))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_SUM2_tenf2_spoiler_sounds", 0x96B9FD92));
	}
	else if (modelNameHash == ATSTRINGHASH("torero2", 0xF62446BA))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_MPSUM2_torero2_Spoiler_Sounds", 0xBAF861DE));
	}
	else if(modelNameHash == ATSTRINGHASH("omnisegt", 0xE1E2E6D7))
	{
		m_SoundSet.Init(ATSTRINGHASH("DLC_SUM2_omnisegt_spoiler_sounds", 0xCAD64483));
	}
	else
	{
		m_SoundSet.Init(ATSTRINGHASH("T20_SPOILER_SOUNDSET", 0x14AF4F68));
	}
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler Update
// ----------------------------------------------------------------
void audVehicleGadgetDynamicSpoiler::Update() 
{
	if(!m_Frozen && (m_StrutState == AUD_STRUT_LOWERING || m_StrutState == AUD_STRUT_RAISING))
	{
		if(!m_RaiseSound && !m_Parent->IsDisabled())
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_SPOILER_STRUTS;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("STRUT_HEIGHT_ADJUST_LOOP", 0xFDE0B78A)), &m_RaiseSound, &initParams);
		}
	}
	else if(m_RaiseSound)
	{
		m_RaiseSound->StopAndForget(true);
	}

	if(!m_Frozen && (m_AngleState == AUD_SPOILER_ROTATING_FORWARDS || m_AngleState == AUD_SPOILER_ROTATING_BACKWARDS))
	{
		if(!m_RotateSound && !m_Parent->IsDisabled())
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_SPOILER;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			m_Parent->CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("STRUT_ANGLE_ADJUST_LOOP", 0x8DAE093F)), &m_RotateSound, &initParams);
		}
	}
	else if(m_RotateSound)
	{
		m_RotateSound->StopAndForget(true);
	}
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler StopAllSounds
// ----------------------------------------------------------------
void audVehicleGadgetDynamicSpoiler::StopAllSounds() 
{
	m_Parent->StopAndForgetSounds(m_RaiseSound, m_RotateSound);
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler SetSpoilerRiseState
// ----------------------------------------------------------------
void audVehicleGadgetDynamicSpoiler::SetSpoilerStrutState(audDynamicSpoilerStrutState strutState)
{
	if(strutState != m_StrutState)
	{
		if(!m_Parent->IsDisabled())
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_SPOILER_STRUTS;	
			
			switch(strutState)
			{
			case AUD_STRUT_RAISING:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_STRUT_START_RAISE", 0x70393FD9)), m_Parent->GetVehicle(), &initParams, true);
				break;

			case AUD_STRUT_RAISED:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_STRUT_RAISED", 0xDCF8CE22)), m_Parent->GetVehicle(), &initParams, true);
				break;

			case AUD_STRUT_LOWERING:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_STRUT_START_LOWER", 0x64691A22)), m_Parent->GetVehicle(), &initParams, true);
				break;

			case AUD_STRUT_LOWERED:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_STRUT_LOWERED", 0x4069E65F)), m_Parent->GetVehicle(), &initParams, true);
				break;				
			}
		}		

		m_StrutState = strutState;
	}	
}

// ----------------------------------------------------------------
// audVehicleGadgetDynamicSpoiler SetSpoilerAngleState
// ----------------------------------------------------------------
void audVehicleGadgetDynamicSpoiler::SetSpoilerAngleState(audDynamicSpoilerAngleState angleState)
{
	if(angleState != m_AngleState)
	{
		if(!m_Parent->IsDisabled())
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_SPOILER;

			switch(angleState)
			{
			case AUD_SPOILER_ROTATING_FORWARDS:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_ROTATE_FORWARDS_START", 0x730F9ED6)), m_Parent->GetVehicle(), &initParams, true);
				break;

			case AUD_SPOILER_ROTATED_FORWARDS:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_ROTATE_FORWARDS_STOP", 0xD6B1A8D8)), m_Parent->GetVehicle(), &initParams, true);
				break;

			case AUD_SPOILER_ROTATING_BACKWARDS:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_ROTATE_BACKWARDS_START", 0x1151CA16)), m_Parent->GetVehicle(), &initParams, true);
				break;

			case AUD_SPOILER_ROTATED_BACKWARDS:
				m_Parent->CreateDeferredSound(m_SoundSet.Find(ATSTRINGHASH("SPOILER_ROTATE_BACKWARDS_STOP", 0x333C265E)), m_Parent->GetVehicle(), &initParams, true);
				break;				
			}
		}		

		m_AngleState = angleState;
	}	
}
