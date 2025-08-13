#include "dooraudioentity.h"
#include "dynamicmixer.h"
#include "gameobjects.h"
#include "northaudioengine.h"
#include "objects/Door.h"
#include "Objects/DoorTuning.h"
#include "Peds/ped.h"

#include "audioengine/controller.h"
#include "audioengine/engine.h"
#include "audioengine/soundfactory.h"
#include "audioengine/soundmanager.h"
#include "audioengine/soundset.h"
#include "debugaudio.h"
#include "debug/DebugScene.h"

#include "network/Network.h"

AUDIO_OPTIMISATIONS()

#if __BANK
namespace rage
{
	XPARAM(audiowidgets);
} 
bool audDoorAudioEntity::sm_NeedToInitialize = false;
bool audDoorAudioEntity::sm_PauseOnFullyOpen = false;
bool audDoorAudioEntity::sm_PauseOnFullyClose = false;
bool audDoorAudioEntity::sm_PauseOnChange = false;
bool audDoorAudioEntity::sm_PauseOnClosing = false;
bool audDoorAudioEntity::sm_PauseOnOpening = false;
bool audDoorAudioEntity::sm_PauseOnBrush = false;
bool audDoorAudioEntity::sm_FindDoor = false;
bool audDoorAudioEntity::sm_IsMovingForAudio = false;
bool audDoorAudioEntity::sm_ShowDoorsUpdating = false;
bool audDoorAudioEntity::sm_ShowCurrentDoorVelocity = false;


audDoorAudioEntity::DoorAudioSettingsSetup audDoorAudioEntity::sm_DoorSettingsCreator;
rage::bkGroup*	audDoorAudioEntity::sm_WidgetGroup(NULL);
static unsigned char s_DoorTuningParamsName;
static unsigned char s_SoundSetName;
#endif

u32 audDoorAudioEntity::sm_DoorImpactTimeFilter = 500;


f32 audDoorAudioEntity::sm_PedHandPushDistance = 0.5f;


audCurve audDoorAudioEntity::sm_DoorCollisionPlayerCurve;
audCurve audDoorAudioEntity::sm_DoorCollisionNpcCurve;
audCurve audDoorAudioEntity::sm_DoorCollisionVehicleCurve;

f32 audDoorAudioEntity::sm_ChangeDirThreshold = 0.05f;
f32 audDoorAudioEntity::sm_DeltaOpeningThreshold = 0.08f;
f32 audDoorAudioEntity::sm_ImpulseThreshold = 1.5f;
f32 audDoorAudioEntity::sm_HeadingThreshold = 1.25f;
f32 audDoorAudioEntity::sm_PunchCollisionScale = 2.f;

//------------------------------------------------------------------------------------------------------------------------------
audDoorAudioEntity::audDoorAudioEntity() :
m_Door(NULL),
m_DoorSettings(NULL),
m_DoorTuningParams(NULL),
m_DoorOpening(NULL),
m_DoorClosing(NULL),
m_WasAtLimit(false),
m_WasOpening(false),
m_WasShutting(false),
m_WasShut(true),
m_WasOpen(false),
m_StopForced(false),
m_ScriptUpdate(false),
m_CurrentVelocity(0.f),
m_Heading(0.f),
m_OriginalHeading(0.f),
m_OldOpenRatio(0.f),
m_LastDoorImpactTime(0)
{

}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::InitClass()
{
#if __BANK
	if(PARAM_audiowidgets.Get())
	{
		AddDoorTuningParamsWidgets();
		AddDoorSoundSetWidgets();
	}
#endif

	sm_DoorCollisionPlayerCurve.Init(ATSTRINGHASH("PLAYER_DOOR_VELOCITY_TO_IMPACT", 0x52E5F893));
	sm_DoorCollisionNpcCurve.Init(ATSTRINGHASH("NPC_DOOR_VELOCITY_TO_IMPACT", 0xCD4B9C2C));
	sm_DoorCollisionVehicleCurve.Init(ATSTRINGHASH("VEHICLE_DOOR_VELOCITY_TO_IMPACT", 0xA8CCDF6C));
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::InitDoor(CDoor *door)
{
	if(GetControllerId() == AUD_INVALID_CONTROLLER_ENTITY_ID)
	{
		naAudioEntity::Init(); 
	}
	naAssertf(door,"Null door when initializing the audio.");
	m_Door = door;
	m_DoorOpening = NULL;
	m_DoorClosing = NULL;
	m_WasShut = true;
	m_WasOpen = false;
	m_WasOpening = false;
	m_StopForced = false;
	REPLAY_ONLY(m_ReplayDoorUpdating = false;)
	BANK_ONLY(m_UsingDefaultSettings = false;)
	m_WasShutting = false;
	m_CurrentVelocity = 0.f;
	m_Heading = 0.f;
	m_OriginalHeading = 0.f;
	m_DoorPosition = VEC3_ZERO;
	
	m_PosForPushSound = VEC3_ZERO;
	m_EnvironmentGroup = NULL;
	m_ScriptUpdate = false;
	m_DoorLastPosition = m_Door->GetDoorStartPosition();

	char doorName[16]; 
	formatf(doorName, "DASL_%08X",door->GetBaseModelInfo()->GetModelNameHash());
	DoorAudioSettingsLink *doorLink = audNorthAudioEngine::GetObject<DoorAudioSettingsLink>(atStringHash(doorName));
	DoorAudioSettings *doorSettings  = NULL;
	if(doorLink)
	{
		doorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(doorLink->DoorAudioSettings);
	}

	if(doorSettings)
	{
		m_DoorSettings = doorSettings;
	}
	else 
	{
		BANK_ONLY(m_UsingDefaultSettings = true;)
		switch(door->GetDoorType())
		{
		case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL:
		case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL_SC:
			{
				m_DoorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(ATSTRINGHASH("DEFAULT_DOORS_SLIDING_HORIZONTAL", 0x05d820e00));
				naAssertf(m_DoorSettings,"Error getting default door type for SLIDING_HORIZONTAL");
				break;
			}
		case CDoor::DOOR_TYPE_SLIDING_VERTICAL:
		case CDoor::DOOR_TYPE_SLIDING_VERTICAL_SC:
			{
				m_DoorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(ATSTRINGHASH("DEFAULT_DOORS_SLIDING_VERTICAL", 0x0cbeecbc1));
				naAssertf(m_DoorSettings,"Error getting default door type for SLIDING_VERTICAL");
				break;
			}
		case CDoor::DOOR_TYPE_GARAGE:
		case CDoor::DOOR_TYPE_GARAGE_SC:
			{
				m_DoorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(ATSTRINGHASH("DEFAULT_DOORS_GARAGE", 0x03719fce4));
				naAssertf(m_DoorSettings,"Error getting default door type for GARAGE");
				break;
			}
		case CDoor::DOOR_TYPE_BARRIER_ARM:
		case CDoor::DOOR_TYPE_BARRIER_ARM_SC:
		case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER:
		case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
			{
				m_DoorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(ATSTRINGHASH("DEFAULT_DOORS_BARRIER_ARM", 0x0be4e17ad));
				naAssertf(m_DoorSettings,"Error getting default door type for BARRIER_ARM");
				break;
			}
		case CDoor::DOOR_TYPE_STD:
		case CDoor::DOOR_TYPE_STD_SC:
			{
				m_DoorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(ATSTRINGHASH("DEFAULT_DOORS_STD", 0x039a68faa));
				naAssertf(m_DoorSettings,"Error getting default door type for STD");
				break;
			}
		default:
			Assertf(0, "Unhandled doortype");
			break;
		}
	}
	if(!naVerifyf(m_DoorSettings,"Failed initializing door with name %s and type %d ", m_Door->GetBaseModelInfo()->GetModelName(),m_Door->GetType()))
	{
		m_DoorSounds.Init(g_NullSoundHash);
		m_PreviousHeadingOffset = 0.f;
		return;
	}

	m_DoorSounds.Init(m_DoorSettings->Sounds);
	DoorTuningParams *tuningParams = audNorthAudioEngine::GetObject<DoorTuningParams>(m_DoorSettings->TuningParams);
	if(tuningParams)
	{
		m_DoorTuningParams = tuningParams;
		m_PreviousHeadingOffset = tuningParams->HeadingThresh;
	}
	else 
	{
		m_DoorTuningParams = NULL;
		m_PreviousHeadingOffset = 0.f;
	}
}

#if GTA_REPLAY
bool audDoorAudioEntity::IsDoorUpdating()
{
	if(m_Door && m_DoorSettings && m_DoorTuningParams)
	{
		bool keepUpdating = ((AUD_GET_TRISTATE_VALUE(m_DoorTuningParams->Flags, FLAG_ID_DOORTUNINGPARAMS_ISRAILLEVELCROSSING) == AUD_TRISTATE_TRUE) 
			&& (m_DoorOpening || m_DoorClosing));

		phInst * pInst = m_Door->GetCurrentPhysicsInst();
		if(keepUpdating || m_ScriptUpdate || (pInst && pInst->GetLevelIndex() != phInst::INVALID_INDEX && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex())))
		{
			return true;
		}
	}
	return false;
}
#endif

void audDoorAudioEntity::ProcessAnimEvents()
{
	for(int i=0; i< AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		if(m_AnimEvents[i])
		{
			audSoundInitParams initParams; 
			initParams.AllowLoad = false;
			initParams.TrackEntityPosition = true;
			CreateAndPlaySound(m_AnimEvents[i], &initParams);
			m_AnimEvents[i] = 0;
		}
	}


	if(m_AnimLoopWasStarted && !m_AnimLoopWasStopped)
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.AllowLoad = false; 
		CreateAndPlaySound_Persistent(m_AnimLoopHash, &m_AnimLoop, &initParams);
	}
	else if(m_AnimLoop && (m_AnimLoopWasStopped || g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) > (m_AnimLoopTriggerTime +100))) 
	{
		m_AnimLoop->StopAndForget();
		m_AnimLoopHash = 0;
	}

	m_AnimLoopWasStarted = false;
	m_AnimLoopWasStopped = false;
}

//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::PreUpdateService(u32 UNUSED_PARAM(timeInMs))
{
	if(m_DoorSounds.IsInitialised() && g_AudioEngine.GetSoundManager().IsMetadataDisabled(m_DoorSounds.GetMetadata()))
	{
		m_DoorSounds.Reset();
	}

	ProcessAnimEvents();

#if __BANK
	if (sm_NeedToInitialize)
	{
		sm_NeedToInitialize = false;
		if(!naVerifyf(m_DoorSettings,"Failed getting door settings"))
		{
			m_DoorSounds.Init(g_NullSoundHash);
			m_PreviousHeadingOffset = 0.f;
			return;
		}
		m_DoorSounds.Init(m_DoorSettings->Sounds);
		DoorTuningParams *tuningParams = audNorthAudioEngine::GetObject<DoorTuningParams>(m_DoorSettings->TuningParams);
		if(tuningParams)
		{
			m_PreviousHeadingOffset = tuningParams->HeadingThresh;
		}
		else 
		{
			m_PreviousHeadingOffset = 0.f;
		}
	}
	if(sm_FindDoor)
	{ 
		if(m_Door)
		{
			switch(m_Door->GetDoorType())
			{
			case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL:
			case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL_SC:
				{
					if(m_UsingDefaultSettings)
						grcDebugDraw::Capsule(m_Door->GetTransform().GetPosition()
						,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Door->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,Color_red);
					break;
				}
			case CDoor::DOOR_TYPE_SLIDING_VERTICAL:
			case CDoor::DOOR_TYPE_SLIDING_VERTICAL_SC:
				{
					if(m_UsingDefaultSettings)
						grcDebugDraw::Capsule(m_Door->GetTransform().GetPosition()
						,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Door->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,Color_blue);
					break;
				}
			case CDoor::DOOR_TYPE_GARAGE:
			case CDoor::DOOR_TYPE_GARAGE_SC:
				{
					if(m_UsingDefaultSettings)
						grcDebugDraw::Capsule(m_Door->GetTransform().GetPosition()
						,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Door->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,Color_yellow);
					break;
				}
			case CDoor::DOOR_TYPE_BARRIER_ARM:
			case CDoor::DOOR_TYPE_BARRIER_ARM_SC:
			case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER:
			case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
				{
					if(m_UsingDefaultSettings)
						grcDebugDraw::Capsule(m_Door->GetTransform().GetPosition()
						,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Door->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,Color_green);
					break;
				}
			case CDoor::DOOR_TYPE_STD:
			case CDoor::DOOR_TYPE_STD_SC:
				{
					if(m_UsingDefaultSettings)
						grcDebugDraw::Capsule(m_Door->GetTransform().GetPosition()
						,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Door->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,Color_orange);
					break;
				}
			default:
				break;
			}
		}
	}
#endif
	bool shouldStop = false;
	if(m_Door && m_DoorSettings && m_DoorTuningParams)
	{
		bool keepUpdating = ((AUD_GET_TRISTATE_VALUE(m_DoorTuningParams->Flags, FLAG_ID_DOORTUNINGPARAMS_ISRAILLEVELCROSSING) == AUD_TRISTATE_TRUE) 
			&& (m_DoorOpening || m_DoorClosing));

		phInst * pInst = m_Door->GetCurrentPhysicsInst();
		if(REPLAY_ONLY((CReplayMgr::IsEditModeActive() && m_ReplayDoorUpdating) ||) keepUpdating || m_ScriptUpdate || (pInst && pInst->GetLevelIndex() != phInst::INVALID_INDEX && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex())))
		{
#if __BANK
				if(sm_ShowDoorsUpdating)
				{
					grcDebugDraw::Sphere(m_Door->GetTransform().GetPosition(),0.2f,Color_green);
				}
#endif
#if !__FINAL
				CBaseModelInfo * model = m_Door->GetBaseModelInfo();
				if(model) 
				{
#endif
					f32 openRatio = fabs(m_Door->GetDoorOpenRatio());
					switch(m_Door->GetDoorType())
					{
					case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL:
					case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL_SC:
						{
							if(naVerifyf(m_DoorTuningParams->DoorType == AUD_DOOR_TYPE_SLIDING_HORIZONTAL, "Wrong door type for door D_%s, the current door type is SLIDING_HORIZONTAL",model->GetModelName()))
							{
								UpdateNonStdDoor();
								ProcessNonStdDoor(openRatio);
							}
							break;
						}
					case CDoor::DOOR_TYPE_SLIDING_VERTICAL:
					case CDoor::DOOR_TYPE_SLIDING_VERTICAL_SC:
						{
							if(naVerifyf(m_DoorTuningParams->DoorType == AUD_DOOR_TYPE_SLIDING_VERTICAL, "Wrong door type for door D_%s, the current door type is SLIDING_VERTICAL",model->GetModelName()))
							{
								UpdateNonStdDoor();
								ProcessNonStdDoor(openRatio);
							}
							break;
						}
					case CDoor::DOOR_TYPE_GARAGE:
					case CDoor::DOOR_TYPE_GARAGE_SC:
						{
							if(naVerifyf(m_DoorTuningParams->DoorType == AUD_DOOR_TYPE_GARAGE, "Wrong door type for door D_%s, the current door type is TYPE_GARAGE",model->GetModelName()))
							{
								UpdateNonStdDoor();
								ProcessNonStdDoor(openRatio);
							}
							break;
						}
					case CDoor::DOOR_TYPE_BARRIER_ARM:
					case CDoor::DOOR_TYPE_BARRIER_ARM_SC:
					case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER:
					case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
						{
							if(naVerifyf(m_DoorTuningParams->DoorType == AUD_DOOR_TYPE_BARRIER_ARM, "Wrong door type for door D_%s, the current door type is BARRIER_ARM",model->GetModelName()))
							{
								UpdateNonStdDoor();
								ProcessNonStdDoor(openRatio);
							}
							break;
						}
					case CDoor::DOOR_TYPE_STD:
					case CDoor::DOOR_TYPE_STD_SC:
						{
							if(naVerifyf(m_DoorTuningParams->DoorType == AUD_DOOR_TYPE_STD, "Wrong door type for door D_%s, the current door type is STANDARD",model->GetModelName()))
							{
								UpdateStdDoor(m_DoorTuningParams->SpeedScale);
								ProcessStdDoor(openRatio);
							}
							break;
						}
					default:
						break;
					}
#if !__FINAL
				}
#endif
		}
		else 
		{
			shouldStop = true;
		}
	}
	else 
	{
		shouldStop = true;
	}
	if(shouldStop)
	{
		if(m_DoorOpening)
		{
			m_DoorOpening->StopAndForget();
		}
		if(m_DoorClosing)
		{
			m_DoorClosing->StopAndForget();
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::UpdateNonStdDoor()
{
	if(m_DoorOpening)
	{
		m_DoorOpening->SetVariableValueDownHierarchyFromName("TargetRatio",fabs(m_Door->GetDoorOpenRatio()));
	}
	if(m_DoorClosing)
	{
		m_DoorClosing->SetVariableValueDownHierarchyFromName("TargetRatio",fabs(m_Door->GetDoorOpenRatio()));
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::UpdateStdDoor(f32 speedScale)
{
	if(m_DoorOpening)
	{
		m_DoorOpening->SetVariableValueDownHierarchyFromName("Velocity",fabs(m_CurrentVelocity) * speedScale);
	}
	if(m_DoorClosing)
	{
		m_DoorClosing->SetVariableValueDownHierarchyFromName("Velocity",fabs(m_CurrentVelocity) * speedScale);
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::ProcessAudio()
{
	if(m_Door)
	{
		m_DoorPosition = VEC3V_TO_VECTOR3(m_Door->GetTransform().GetPosition());
		m_CurrentVelocity = (m_DoorLastPosition.Mag2() == 0.f) ? 0.f :(m_DoorPosition - m_DoorLastPosition).Mag2() / fwTimer::GetTimeStep();
	}
} 
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::ProcessAudio(f32 velocity,f32 heading,f32 deltaHeading)
{
	m_CurrentVelocity = velocity;
	m_Heading = heading;
	m_OriginalHeading = deltaHeading;
}

void audDoorAudioEntity::EntityWantsToOpenDoor(Vector3::Param position, f32 UNUSED_PARAM(speed))
{
	m_PosForPushSound = position;
}

void audDoorAudioEntity::TriggerDoorImpact(const CPed * ped, Vec3V_In pos, bool UNUSED_PARAM(isPush)) 
{
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(now < m_LastDoorImpactTime + sm_DoorImpactTimeFilter)
	{
		m_LastDoorImpactTime = now;
		return;
	}
	m_LastDoorImpactTime = now;

	if(ped->GetMotionData()->GetIsStill())
	{
		return;
	}

	f32 impactMag = 0.f;

	if(ped->GetMotionData()->GetIsSprinting() || ped->GetMotionData()->GetIsRunning())
	{
		impactMag = audPedAudioEntity::GetPedRunningImpactFactor();
	}
	else
	{
		impactMag = audPedAudioEntity::GetPedWalkingImpactFactor();
	}

	audSoundInitParams initParams;
	initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(impactMag);
	initParams.Position = VEC3V_TO_VECTOR3(pos);
	//initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), m_EntitySpeed);

	const u32 hitSoundHash = ATSTRINGHASH("COLLISION", 0x86618E11);
	audMetadataRef hitSoundRef = GetSoundSet().Find(hitSoundHash);

	if(hitSoundRef.IsValid())
	{
		CreateDeferredSound(hitSoundRef, ped, &initParams, true);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_DoorSounds.GetNameHash(), hitSoundHash, &initParams, ped));
	}	
}


//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::ProcessStdDoor(f32 openRatio)
{
	if(!FPIsFinite(openRatio))
	{
		return;
	}
	
	const bool bMovingForAudio = (m_OldOpenRatio != openRatio);

#if __BANK
	if (CDebugScene::FocusEntities_Get(0) == m_Door && bMovingForAudio && sm_IsMovingForAudio)
	{
		naDisplayf("MOVING FOR AUDIO [CurrentVel %f]",fabs(m_CurrentVelocity));
	}
#endif	
	bool slamShut = m_Door->GetTuning() && m_Door->GetTuning()->m_ShouldLatchShut;
	float fHeadingOffset = m_Heading - m_OriginalHeading;
	fHeadingOffset = fwAngle::LimitRadianAngleFast(fHeadingOffset);

	if(m_WasShut && (openRatio > m_DoorTuningParams->ClosedThresh) && (fabs(m_CurrentVelocity) > m_DoorTuningParams->SpeedThresh))
	{
#if __BANK
		if(sm_PauseOnOpening && (CDebugScene::FocusEntities_Get(0) == m_Door))
		{
			naDisplayf("Push [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
			fwTimer::StartUserPause();
		}
#endif	
		m_WasShut = false;
		m_WasOpening = true;
		audSoundInitParams initParams;

		initParams.Position = m_PosForPushSound;

		initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
		initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
		CreateAndPlaySound(FindSound(ATSTRINGHASH("PUSH", 0x74D4F959)), &initParams);
		

	}
	//duplicate it on CDOOR to move the call slam door there...
	else if(m_WasShutting )
	{
		if((fHeadingOffset > 0.f && m_PreviousHeadingOffset < 0.f) || (fHeadingOffset < 0.f && m_PreviousHeadingOffset > 0.f))
		{
			if(!slamShut) 
			{
#if __BANK
				if(sm_PauseOnBrush && (CDebugScene::FocusEntities_Get(0) == m_Door))
				{
					naDisplayf("Brush [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
					fwTimer::StartUserPause();
				}
#endif
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true; 
				initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
				initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
				CreateAndPlaySound(FindSound(ATSTRINGHASH("BRUSH", 0x702E09E4)), &initParams);
			}
		}

		// if its pretty close to 0 degrees and moving slowly then mark as closed
		if((openRatio <= m_DoorTuningParams->ClosedThresh) && (slamShut || fabs(m_CurrentVelocity) <= m_DoorTuningParams->SpeedThresh))
		{
#if __BANK
			if(sm_PauseOnFullyClose && (CDebugScene::FocusEntities_Get(0) == m_Door))
			{
				naDisplayf("Close [CurrentVel %f] [fHeadingOffset %f] %f, %f ",fabs(m_CurrentVelocity),fHeadingOffset,DtoR*sm_HeadingThreshold,m_Heading);
				fwTimer::StartUserPause();
			}
#endif

			m_WasShut = true;
			m_WasShutting = false;
			m_WasOpening = false;
			if(m_DoorClosing)
			{
				m_DoorClosing->StopAndForget();
			}
			if(m_DoorOpening)
			{
				m_DoorOpening->StopAndForget();
			}
			if(slamShut)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true; 
				initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
				initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
				CreateAndPlaySound(FindSound(ATSTRINGHASH("CLOSED", 0x69D3EE70)), &initParams);
			}
		}
	}
	// door counts as at its limit if within 3 degrees of rotational limit
	if(!m_WasAtLimit && (openRatio > m_DoorTuningParams->OpenThresh))
	{
#if __BANK
		if(sm_PauseOnFullyOpen && (CDebugScene::FocusEntities_Get(0) == m_Door))
		{
			naDisplayf("Limit [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
			fwTimer::StartUserPause();
		}
#endif	
		m_WasAtLimit = true;
		m_WasOpening = false;
		if(m_DoorOpening)
		{
			m_DoorOpening->StopAndForget();
		}		
		m_WasShutting = false;
		if(m_DoorClosing)
		{
			m_DoorClosing->StopAndForget();
		}
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
		initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
		CreateAndPlaySound(FindSound(ATSTRINGHASH("LIMIT", 0xD92CB76D)), &initParams);

	}
	if(m_WasAtLimit && (openRatio < m_DoorTuningParams->OpenThresh))
	{
		m_WasAtLimit = false;
		m_WasShutting = true;
	}

	if(bMovingForAudio)
	{
		if(m_WasOpening && (m_OldOpenRatio < openRatio ))
		{
			if( !m_DoorOpening )
			{
#if __BANK
				if(sm_PauseOnOpening && (CDebugScene::FocusEntities_Get(0) == m_Door))
				{
					naDisplayf("Opening [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
					fwTimer::StartUserPause();
				}
#endif	
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true; 
				initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
				initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
				CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("OPENING", 0x77D37405)), &m_DoorOpening, &initParams);
			}
		}
		else if (m_WasShutting && (m_OldOpenRatio > openRatio))
		{
			if(!m_DoorClosing)
			{
#if __BANK
				if(sm_PauseOnClosing && (CDebugScene::FocusEntities_Get(0) == m_Door))
				{
					naDisplayf("Closing [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
					fwTimer::StartUserPause();
				}
#endif
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
				initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
				CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("CLOSING", 0x9307B42F)),&m_DoorClosing,&initParams);
			}
		}
		else if((m_WasOpening && (m_OldOpenRatio >= (openRatio + sm_DeltaOpeningThreshold)))||(m_WasShutting && (m_OldOpenRatio <= (openRatio - sm_DeltaOpeningThreshold))))
		{
			if(m_WasOpening)
			{
				if(m_DoorOpening)
				{
					m_DoorOpening->StopAndForget();
				}
				m_WasOpening = false;
				m_WasShutting = true;
				if(!m_DoorClosing)
				{
#if __BANK
					if(sm_PauseOnClosing && (CDebugScene::FocusEntities_Get(0) == m_Door))
					{
						naDisplayf("Was opening now closing [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
						fwTimer::StartUserPause();
					}
#endif
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true; 
					initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
					initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
					CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("CLOSING", 0x9307B42F)),&m_DoorClosing,&initParams);
				}

			}
			else if (m_WasShutting)
			{
				if(m_DoorClosing)
				{
					m_DoorClosing->StopAndForget();
				}
				m_WasShutting = false;
				m_WasOpening = true;
				if(!m_DoorOpening)
				{
#if __BANK
					if(sm_PauseOnOpening && (CDebugScene::FocusEntities_Get(0) == m_Door))
					{
						naDisplayf("Was closing now opening [CurrentVel %f] [openRatio %f] [OldOpenRatio %f] ",fabs(m_CurrentVelocity),openRatio,m_OldOpenRatio);
						fwTimer::StartUserPause();
					}
#endif
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true; 
					initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
					initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), fabs(m_CurrentVelocity) * m_DoorTuningParams->SpeedScale);
					CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("OPENING", 0x77D37405)),&m_DoorOpening,&initParams);
				}		
			}
		}
	}
	else 
	{
		if(m_DoorOpening)
		{
			m_DoorOpening->StopAndForget();
		}
		if(m_DoorClosing)
		{
			m_DoorClosing->StopAndForget();
		}
	}

	m_OldOpenRatio = fabs(openRatio);
	m_PreviousHeadingOffset = fHeadingOffset;
}

audMetadataRef audDoorAudioEntity::FindSound(u32 soundName)
{
	if(CNetwork::IsNetworkOpen())
	{
		// Workaround for garage doors being audible inside the MP apartments.
		static const SoundSet *s_GarageDoorSoundSet = NULL;
		if(!s_GarageDoorSoundSet)
		{
			s_GarageDoorSoundSet = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObject<SoundSet>(ATSTRINGHASH("DOOR_GARAGE_LS_CUSTOMS", 0xCA57A12));
		}

		static const MixerScene *s_MpApartmentScene = NULL;
		if(!s_MpApartmentScene)
		{
			s_MpApartmentScene = g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObject<MixerScene>(ATSTRINGHASH("MP_APARTMENT_SCENE", 0x2F4D80C7));
		}

		if(DYNAMICMIXER.GetActiveSceneFromMixerScene(s_MpApartmentScene))
		{
			if(m_DoorSounds.GetMetadata() == s_GarageDoorSoundSet)
			{
				return audMetadataRef();
			}
		}
	}
	return m_DoorSounds.Find(soundName);
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::ProcessNonStdDoor(f32 openRatio)
{
	if(!FPIsFinite(openRatio))
	{
		return;
	}
#if __BANK
	if(sm_ShowCurrentDoorVelocity)
	{
		char txt[128];
		formatf(txt,"Current Speed %f", fabs(m_CurrentVelocity));
		grcDebugDraw::Text(m_Door->GetTransform().GetPosition(),Color_white,txt);
	}
#endif

	if(fabs(m_CurrentVelocity) < m_DoorTuningParams->SpeedThresh)
	{
		m_StopForced = true;
		if(m_DoorClosing)
		{
			m_DoorClosing->StopAndForget();
		}
		if(m_DoorOpening)
		{
			m_DoorOpening->StopAndForget();
		}
	}
	if(!m_WasShut &&  (openRatio <= m_DoorTuningParams->ClosedThresh))
	{
#if __BANK
		if(sm_PauseOnFullyClose && (CDebugScene::FocusEntities_Get(0) == m_Door))
		{
			naDisplayf("Fully close [TargetRatio %f] [CurrentVel %f]",openRatio,fabs(m_CurrentVelocity));
			fwTimer::StartUserPause();
		}
#endif		

		m_WasOpen = false;
		m_WasShut = true;
		m_WasShutting = false;
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
		CreateAndPlaySound(FindSound(ATSTRINGHASH("CLOSED", 0x69D3EE70)),&initParams);
		if(m_DoorClosing)
		{
			m_DoorClosing->StopAndForget();
		}
		if(m_DoorOpening)
		{
			m_DoorOpening->StopAndForget();
		}
	}
	else if(!m_WasOpen && (openRatio >= m_DoorTuningParams->OpenThresh))
	{
#if __BANK
		if(sm_PauseOnFullyOpen && (CDebugScene::FocusEntities_Get(0) == m_Door))
		{
			naDisplayf("Fully open [TargetRatio %f] [CurrentVel %f]",openRatio,fabs(m_CurrentVelocity));
			fwTimer::StartUserPause();
		}
#endif					

		m_WasShut = false;
		m_WasOpen = true;
		m_WasOpening = false;
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
		CreateAndPlaySound(FindSound(ATSTRINGHASH("OPENED", 0x2C29359A)),&initParams);
		if(m_DoorOpening && AUD_GET_TRISTATE_VALUE(m_DoorTuningParams->Flags, FLAG_ID_DOORTUNINGPARAMS_ISRAILLEVELCROSSING) != AUD_TRISTATE_TRUE)
		{
			m_DoorOpening->StopAndForget();
		}
		if(m_DoorClosing)
		{
			m_DoorClosing->StopAndForget();
		}
	}
	else
	{
		// Opening
		if(m_WasShut && (openRatio > m_DoorTuningParams->ClosedThresh) && (m_OldOpenRatio < openRatio))
		{
#if __BANK
			if(sm_PauseOnOpening && (CDebugScene::FocusEntities_Get(0) == m_Door))
			{
				naDisplayf("Opening [TargetRatio %f] [CurrentVel %f]",openRatio,fabs(m_CurrentVelocity));
				fwTimer::StartUserPause();
			}
#endif	
			m_WasShut = false;
			m_WasOpening = true;
			if(!m_DoorOpening)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
				initParams.SetVariableValue(ATSTRINGHASH("TargetRatio",0x9f89d6e6), openRatio);
				CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("OPENING", 0x77D37405)),&m_DoorOpening,&initParams);
			}
			if(m_DoorClosing)
			{
				m_DoorClosing->StopAndForget();
			}
		}
		else if(m_WasOpen && (openRatio < m_DoorTuningParams->OpenThresh) && (m_OldOpenRatio > openRatio))
		{
#if __BANK
			if(sm_PauseOnClosing && (CDebugScene::FocusEntities_Get(0) == m_Door))
			{
				naDisplayf("Closing [TargetRatio %f] [CurrentVel %f]",openRatio,fabs(m_CurrentVelocity));
				fwTimer::StartUserPause();
			}
#endif	
			m_WasOpen = false;
			m_WasShutting = true;
			if (AUD_GET_TRISTATE_VALUE(m_DoorTuningParams->Flags, FLAG_ID_DOORTUNINGPARAMS_ISRAILLEVELCROSSING) != AUD_TRISTATE_TRUE)
			{
				if(!m_DoorClosing)
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
					initParams.SetVariableValue(ATSTRINGHASH("TargetRatio",0x9f89d6e6), openRatio);
					CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("CLOSING", 0x9307B42F)),&m_DoorClosing,&initParams);
				}
				if(m_DoorOpening)
				{
					m_DoorOpening->StopAndForget();
				}
			}
		}
		else if(m_StopForced || (m_WasOpening && (m_OldOpenRatio > openRatio))||(m_WasShutting && (m_OldOpenRatio < openRatio)))
		{
			if(fabs(m_CurrentVelocity) > m_DoorTuningParams->SpeedThresh)
			{
				m_StopForced = false;
#if __BANK
				if(sm_PauseOnChange && (CDebugScene::FocusEntities_Get(0) == m_Door))
				{
					naDisplayf("Change state [TargetRatio %f] [CurrentVel %f]",openRatio,fabs(m_CurrentVelocity));
					fwTimer::StartUserPause();
				}
#endif		
				//Opening
				if(m_WasOpening)
				{
					m_WasOpening = false;
					m_WasShutting = true;
					if(m_DoorOpening)
					{
						m_DoorOpening->StopAndForget();
					}
					if(!m_DoorClosing)
					{
						audSoundInitParams initParams;
						initParams.TrackEntityPosition = true;
						initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
						initParams.SetVariableValue(ATSTRINGHASH("TargetRatio",0x9f89d6e6), openRatio);
						CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("CLOSING", 0x9307B42F)),&m_DoorClosing,&initParams);
					}
				}
				//Closing
				else if (m_WasShutting)
				{
					m_WasOpening = true;
					m_WasShutting = false;
					if(m_DoorClosing)
					{
						m_DoorClosing->StopAndForget();
					}
					if(!m_DoorOpening)
					{
						audSoundInitParams initParams;
						initParams.TrackEntityPosition = true;
						initParams.EnvironmentGroup = CreateDoorEnvironmentGroup(m_Door);
						initParams.SetVariableValue(ATSTRINGHASH("TargetRatio",0x9f89d6e6), openRatio);
						CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("OPENING", 0x77D37405)),&m_DoorOpening,&initParams);
					}
				}
			}
		}
	}
	m_OldOpenRatio = fabs(openRatio);
	m_DoorLastPosition = m_DoorPosition;
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::UpdateClass()
{
#if __BANK
	if (CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypeObject())
	{
		CObject* pDoor = static_cast<CObject*>(CDebugScene::FocusEntities_Get(0));
		if(pDoor)
		{
			if(pDoor->IsADoor())
			{
				CBaseModelInfo * model = pDoor->GetBaseModelInfo();
				if(model) 
				{
					formatf(sm_DoorSettingsCreator.doorAudioSettingsName,"D_%s",model->GetModelName());
					if(sm_DoorSettingsCreator.hasToCreateDTP)
					{
						formatf(sm_DoorSettingsCreator.doorTuningParamsName,"DTP_%s",model->GetModelName());
					}
				}
			}
		}
	}
#endif
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::ProcessCollisionSound(f32 impulseMagnitude, bool shouldPlayIt, const CEntity * otherEntity)
{
	if(shouldPlayIt || (!m_WasShut && 
		// closing or opening with a big hit
		((m_WasShutting || (m_WasOpening && impulseMagnitude >= sm_ImpulseThreshold)) 
		// Brushing.
		|| (!m_WasShutting && !m_WasOpening))))
	{
		audSoundInitParams initParams;
		initParams.SetVariableValue(ATSTRINGHASH("Velocity",0x8FF7E5A7), (shouldPlayIt)?(sm_PunchCollisionScale):fabs(m_CurrentVelocity));

		const u32 hitSoundHash = ATSTRINGHASH("COLLISION", 0x86618E11);
		audMetadataRef hitSoundRef = GetSoundSet().Find(hitSoundHash);

		if(hitSoundRef.IsValid())
		{
			if(otherEntity)
			{
				initParams.Position = VEC3V_TO_VECTOR3(otherEntity->GetTransform().GetPosition());
				CreateDeferredSound(hitSoundRef,otherEntity, &initParams,true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_DoorSounds.GetNameHash(), hitSoundHash, &initParams, otherEntity));
			}
			else
			{
				CPed *pPlayer = CGameWorld::FindLocalPlayer();
				naAssertf(pPlayer,"Error getting the player.");
				initParams.Position = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
				CreateDeferredSound(hitSoundRef,pPlayer, &initParams,true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_DoorSounds.GetNameHash(), hitSoundHash, &initParams, pPlayer));
			}
		}

	}
}

//------------------------------------------------------------------------------------------------------------------------------
Vec3V_Out audDoorAudioEntity::GetPosition() const 
{ 
	Assert(m_Door);
	return m_Door->GetTransform().GetPosition(); 
}

//-------------------------------------------------------------------------------------------------------------------------------
audCompressedQuat audDoorAudioEntity::GetOrientation() const 
{
	Assert(m_Door); 
	Assert(m_Door->GetPlaceableTracker()); 
	return m_Door->GetPlaceableTracker()->GetOrientation(); 

}

naEnvironmentGroup* audDoorAudioEntity::CreateDoorEnvironmentGroup(const CDoor* door)
{
	naEnvironmentGroup* environmentGroup = NULL;
	if(naVerifyf(door, "Passed NULL CDoor* when creating a door environmentGroup"))
	{
		environmentGroup = naEnvironmentGroup::Allocate("Door");
		if(environmentGroup)
		{
			environmentGroup->Init(NULL, 0, 0);
			environmentGroup->SetSource(door->GetTransform().GetPosition());
			environmentGroup->SetUsePortalOcclusion(true);
			environmentGroup->SetMaxPathDepth(audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());
			environmentGroup->SetInteriorInfoWithEntity(door);
		}
	}
	return environmentGroup;
}

//------------------------------------------------------------------------------------------------------------------------------
#if __BANK
void audDoorAudioEntity::CreateDoorAudioSettings()
{
	if(strlen(sm_DoorSettingsCreator.doorAudioSettingsName) == 0 
		||(sm_DoorSettingsCreator.hasToCreateDTP ? strlen(sm_DoorSettingsCreator.doorTuningParamsName) == 0 : false))
	{
		return; //No model selected, nothing to create
	}
	if(sm_DoorSettingsCreator.hasToCreateDTP)
	{
		char xmlMsg[4092];
		FormatDoorsTuningParamsXml(xmlMsg, sm_DoorSettingsCreator);
		naAssertf(g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg)), "Failed to send xml message to rave");
	}
	char xmlMsg[4092];
	char m_DoorTuningParams[64];
	DoorList *tuningParamsList = audNorthAudioEngine::GetObject<DoorList>(ATSTRINGHASH("DoorList", 0x51631452));
	if(naVerifyf(tuningParamsList,"Failed getting door tuning params list"))
	{
		DoorAudioSettings *doorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(tuningParamsList->Door[s_DoorTuningParamsName].DoorAudioSettings);

		if(doorSettings)
		{
			formatf(m_DoorTuningParams,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(doorSettings->TuningParams));
		}
	}
	char soundSet[64];
	SoundSetList *soundSetList = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObject<SoundSetList>(audStringHash("SoundSetList"));
	if(naVerifyf(soundSetList,"Failed getting door soundset list"))
	{
		formatf(soundSet,"%s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(soundSetList->SoundSet[s_SoundSetName].SoundSetHash));
	}
	FormatDoorsAudioSettingsXml(xmlMsg, sm_DoorSettingsCreator.doorAudioSettingsName,soundSet,(sm_DoorSettingsCreator.hasToCreateDTP ? sm_DoorSettingsCreator.doorTuningParamsName : m_DoorTuningParams));
	naAssertf(g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg)), "Failed to send xml message to rave");
	sm_NeedToInitialize = true;
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::FormatDoorsAudioSettingsXml(char * xmlMsg, const char * settingsName,const char * soundsRef,const char * tuningParamsRef)
{
	sprintf(xmlMsg, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};
	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	//DoorAudioSettings
	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", (u32)audStringHash("BASE"));
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<DoorAudioSettings name=\"%s\">\n", &upperSettings[0]);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<Sounds>%s</Sounds>\n", soundsRef);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<TunningParams>%s</TunningParams>\n", tuningParamsRef);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		</DoorAudioSettings>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::FormatDoorsTuningParamsXml(char * xmlMsg, DoorAudioSettingsSetup settings)
{
	atString upperParamsName(settings.doorTuningParamsName);
	upperParamsName.Uppercase();

	sprintf(xmlMsg, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};
	//Door Tuning Params
	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", (u32)audStringHash("BASE"));
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<DoorTuningParams name=\"%s\">\n", &upperParamsName[0]);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<OpenThresh>%f</OpenThresh>\n", settings.openThresh);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<HeadingThresh>%f</HeadingThresh>\n", settings.headingThresh);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<ClosedThresh>%f</ClosedThresh>\n", settings.closedThresh);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<SpeedThresh>%f</SpeedThresh>\n", settings.speedThresh);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<SpeedScale>%f</SpeedScale>\n", settings.speedScale);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<HeadingDeltaThreshold>%f</HeadingDeltaThreshold>\n", settings.headingDeltaThreshold);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<AngularVelocityThreshold>%f</AngularVelocityThreshold>\n", settings.angularVelThreshold);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		</DoorTuningParams>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::AddDoorTuningParamsWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");
	bank->SetCurrentGroup(*audDoorAudioEntity::sm_WidgetGroup);
	bkWidget* tuninParamsWidget =  BANKMGR.FindWidget("Audio/Door editor/Door tuning params" ) ;
	if(!tuninParamsWidget)
	{
		DoorList *tuningParamsList = audNorthAudioEngine::GetObject<DoorList>(ATSTRINGHASH("DoorList", 0x51631452));
		if(naVerifyf(tuningParamsList,"Failed getting door tuning params list"))
		{
			rage::bkCombo* pTuningParamsCombo = bank->AddCombo("Door tuning param", &s_DoorTuningParamsName, tuningParamsList->numDoors, NULL);
			if (pTuningParamsCombo != NULL)
			{
				for(int i=0; i < tuningParamsList->numDoors; i++)
				{
					DoorAudioSettings *doorSettings = audNorthAudioEngine::GetObject<DoorAudioSettings>(tuningParamsList->Door[i].DoorAudioSettings);

					if(doorSettings)
					{
						pTuningParamsCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(doorSettings->TuningParams));
					}
				}
			}
		}
	}
	bank->UnSetCurrentGroup(*audDoorAudioEntity::sm_WidgetGroup);
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::AddDoorSoundSetWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");
	bank->SetCurrentGroup(*audDoorAudioEntity::sm_WidgetGroup);
	bkWidget* tuninParamsWidget =  BANKMGR.FindWidget("Audio/audDoorAudioEntity/Door editor/Door soundset list" ) ;
	if(!tuninParamsWidget)
	{
		SoundSetList *soundSetList = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObject<SoundSetList>(audStringHash("SoundSetList"));
		if(naVerifyf(soundSetList,"Failed getting door soundset list"))
		{
			rage::bkCombo* pSoundSetCombo = bank->AddCombo("Door sound sets", &s_SoundSetName, soundSetList->numSoundSets, NULL);
			if (pSoundSetCombo != NULL)
			{
				for(int i=0; i < soundSetList->numSoundSets; i++)
				{
					pSoundSetCombo->SetString(i, g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(soundSetList->SoundSet[i].SoundSetHash));
				}
			}
		}
		bank->UnSetCurrentGroup(*audDoorAudioEntity::sm_WidgetGroup);
	}
}
void Continue()
{
	fwTimer::EndUserPause();
}
//------------------------------------------------------------------------------------------------------------------------------
void audDoorAudioEntity::AddWidgets(bkBank &bank)
{
	bool test = false;
	bank.PushGroup("audDoorAudioEntity");
		bank.AddToggle("Show door current vel",&sm_ShowCurrentDoorVelocity);
		bank.AddSlider("Change dir threshold", &sm_ChangeDirThreshold, 0.f, 1.f, 0.001f);
		bank.AddSlider("Opening impulse threshold", &sm_ImpulseThreshold, 0.f, 10.f, 0.001f);
		bank.AddSlider("Heading threshold in degrees for fully close doors.", &sm_HeadingThreshold, 0.f, 10.f, 0.001f);
		bank.AddSlider("Punch collision impact magnitude", &sm_PunchCollisionScale, 0.f, 10.f, 0.001f);
		bank.AddSlider("Opening/Closing delta threshold.", &sm_DeltaOpeningThreshold, 0.f, 10.f, 0.001f);
		bank.AddSlider("Ped hand push distance threshold", &sm_PedHandPushDistance, 0.f, 2.f, 0.1f);
		bank.AddSlider("sm_DoorImpactTimeFilter", &sm_DoorImpactTimeFilter, 0, 100000, 100);
		bank.AddSeparator();
		bank.AddToggle("sm_ShowDoorsUpdating",&sm_ShowDoorsUpdating);
		bank.AddToggle("Is door moving for audio?",&sm_IsMovingForAudio);
		bank.AddToggle("Find door",&sm_FindDoor);
		bank.AddText("Red	 -> Sliding horizontal",&test);
		bank.AddText("Blue	 -> Sliding vertical",&test);
		bank.AddText("Yellow -> Garage",&test);
		bank.AddText("Green  -> Barrier",&test);
		bank.AddText("Orange -> Standard",&test);
		bank.AddSeparator();
		bank.AddToggle("Pause on open/limit event",&sm_PauseOnFullyOpen);
		bank.AddToggle("Pause on close event",&sm_PauseOnFullyClose);
		bank.AddToggle("Pause when changing dir",&sm_PauseOnChange);
		bank.AddToggle("Pause when opening",&sm_PauseOnOpening);
		bank.AddToggle("Pause when closing",&sm_PauseOnClosing);
		bank.AddToggle("Pause on brush event",&sm_PauseOnBrush);
		bank.AddButton("Continue", datCallback(CFA(Continue)));
		bank.AddSeparator();
		bank.PushGroup("Door editor", true);
			bank.AddText("Current Door", sm_DoorSettingsCreator.doorAudioSettingsName, sizeof(sm_DoorSettingsCreator.doorAudioSettingsName), false);
			bank.AddButton("Create current door", CFA(CreateDoorAudioSettings));
			bank.AddToggle("Also create tunning params GO",&sm_DoorSettingsCreator.hasToCreateDTP);
			bank.AddSlider("Open Threshold", &sm_DoorSettingsCreator.openThresh, 0.f, 1.f, 0.001f);
			bank.AddSlider("Close Threshold", &sm_DoorSettingsCreator.closedThresh, 0.f, 1.f, 0.001f);
			bank.AddSlider("Heading Threshold", &sm_DoorSettingsCreator.headingThresh, 0.f, 1.f, 0.001f);
			bank.AddSlider("Speed Threshold", &sm_DoorSettingsCreator.speedThresh, 0.f, 1.f, 0.001f);
			bank.AddSlider("Speed scale", &sm_DoorSettingsCreator.speedScale, 0.f, 1.f, 0.001f);
			bank.AddSlider("Heading Delta Threshold", &sm_DoorSettingsCreator.headingDeltaThreshold, 0.f, 1.f, 0.00001f);
			bank.AddSlider("Angular Vel Threshold", &sm_DoorSettingsCreator.angularVelThreshold, 0.f, 1.f, 0.00001f);
			sm_WidgetGroup = smart_cast<bkGroup*>(bank.GetCurrentGroup());
			naAssert(sm_WidgetGroup);
		bank.PopGroup();
	bank.PopGroup();
}
#endif
//
//void audDoorAudioEntity::StartDoorOpening(CGarage *garage)
//{
//	if(m_DoorLoop)
//	{
//		naErrorf("Starting a door opening loop but there's already one playing");
//		m_DoorLoop->StopAndForget();
//	}
//	audSoundInitParams initParams;
//	initParams.Tracker = garage->m_pDoorObject->GetPlaceableTracker();
//	initParams.EnvironmentGroup = garage->m_pDoorObject->GetAudioEnvironmentGroup();
//	CreateAndPlaySound_Persistent("PAYANDSPRAY_DOOR", &m_DoorLoop, &initParams);
//}
//
//void audDoorAudioEntity::StopDoorOpening()
//{
//	if(m_DoorLoop)
//	{
//		m_DoorLoop->StopAndForget();
//	}
//}
//
//void audDoorAudioEntity::StartRespray()
//{
//	if(m_ResprayLoop)
//	{
//		naErrorf("Trying to start a respray loop but there's already one playing");
//		m_ResprayLoop->StopAndForget();
//	}
//	CreateAndPlaySound_Persistent("PAYANDSPRAY_COMPRESSOR", &m_ResprayLoop);
//}
//
//void audDoorAudioEntity::StopRespray()
//{
//	if(m_ResprayLoop)
//	{
//		m_ResprayLoop->StopAndForget();
//	}
//}
