// Title	:	Heli.cpp
// Author	:	Alexander Roger
// Started	:	10/04/2003
//
//
//
// 
// C headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Rage headers
#include "crskeleton/skeleton.h"
#include "math/vecmath.h"
#include "phbound/boundcomposite.h"
#include "pheffects/wind.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "fwpheffects/ropedatamanager.h"
#include "fwpheffects/ropemanager.h"
#include "fwscene/world/WorldLimits.h"

#if !__NO_OUTPUT
#include "fwnet/netchannel.h"
#endif

// Game headers
#include "audio/collisionaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/policescanner.h"
#include "camera/CamInterface.h"
#include "control/gamelogic.h"
#include "control/replay/Misc/RopePacket.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/Task/TaskVehicleMissionBase.h"
#include "vehicleAi/Task/TaskVehicleFlying.h"
#include "vehicleAi/Task/TaskVehicleGotoHelicopter.h"
#include "vehicleAi/FlyingVehicleAvoidance.h"
#include "Vehicles/VehicleGadgets.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "event/ShockingEvents.h"
#include "event/EventDamage.h"
#include "event/EventShocking.h"
#include "game/clock.h"
#include "game/modelIndices.h"
#include "game/weather.h"
#include "Stats/StatsMgr.h"
#include "modelInfo/vehicleModelInfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/Live/NetworkTelemetry.h"
#include "network/NetworkInterface.h"
#include "network/players/NetGamePlayer.h"
#include "peds/pedIntelligence.h"
#include "peds/ped.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/ApplyDamage.h"
#include "renderer/lights/lights.h"
#include "renderer/PostScan.h"
#include "renderer/water.h"
#include "renderer/zoneCull.h"
#include "scene/world/gameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "Scene/portals/Portal.h"
#include "script/script_hud.h"
#include "Stats/StatsInterface.h"
#include "streaming/streaming.h"
#include "streaming/populationstreaming.h"
#include "system/pad.h"
#include "peds/PlayerInfo.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Combat/TaskCombat.h"
#include "TimeCycle/TimeCycle.h"
#include "VehicleAI/Task/TaskVehiclePlayer.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "Vehicles/Boat.h"
#include "vehicles/heli.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/vehicle_channel.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/Systems/VfxBlood.h"
#include "vfx/Systems/VfxMaterial.h"
#include "vfx/Systems/VfxVehicle.h"
#include "vfx/Systems/VfxWater.h"
#include "weapons/explosion.h"
#include "weapons/Projectiles/Projectile.h"
#include "game/wind.h"
#include "audio/heliaudioentity.h"
#include "Task/Vehicle/TaskInVehicle.h"

#if __PPU
#include "system/controlMgr.h"
#endif

ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()
AUDIO_VEHICLES_OPTIMISATIONS()

#if !__NO_OUTPUT
RAGE_DEFINE_SUBCHANNEL(net, damage_heli, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_damage_heli
#endif


#define HELI_ROTOR_ANGULAR_ACCELERATION (0.2f) //(0.001f) // GTA_SA units
#define HELI_ROTOR_ANGULAR_DECCELERATION (0.1f) //(0.00055f) // GTA_SA units

bool CHeli::bPoliceHelisAllowed = true;
bool CHeli::bHeliControlsCheat = false;

#define HELI_MINIMUM_CONTROL_SPEED		(1.0f)	
//
#define HELI_ROTOR_DOTPROD_LIMIT (0.95f)
dev_float sfHeliRearRotorLostYaw = 5.0f;
dev_float sfHeliRearRotorLostRoll = 0.35f;
dev_float sfHeliRearRotorDamageYaw = 0.0f;
dev_float sfHeliRearRotorDamageRoll = 0.01f;
dev_float sfHeliMainRotorLostYaw = 0.15f;
dev_float sfHeliMainRotorLostRoll = 0.01f;
dev_float sfHeliMainRotorDamageYaw = 0.0f;
dev_float sfHeliMainRotorDamageRoll = 0.01f;

//Out of control tuning variables, could do with tidying up.
dev_float sfHeliLostRearRotorThrottle = 0.8f;

dev_float sfHeliOutOfControlMinThrottle = 0.0f;
dev_float sfHeliOutOfControlMaxThrottle = 2.0f;

dev_float sfHeliTailLostYaw = 1.25f;
dev_float sfHeliOutOfControlMinYaw = 0.5f;
dev_float sfHeliTailLostRoll = -1.2f;
dev_float sfHeliOutOfControlMaxRoll = 1.2f;
dev_float sfHeliOutOfControlPitch = 0.9f;

dev_float sfHeliOutOfControlRollAfterRecoveryMult = 2.0f;
dev_float sfHeliOutOfControlThrottleDropAfterRecoveryMin = 0.0f;
dev_float sfHeliOutOfControlThrottleDropAfterRecoveryMax = 1.4f;

dev_float sfHeliOutOfControlMaxYawControl = 8.0f;
dev_float sfHeliOutOfControlYawControlAdjustmentNeg = -1.25f;
dev_float sfHeliOutOfControlYawControlAdjustmentPos = 1.25f;
dev_float sfHeliOutOfControlMinThrottleControl = 0.0f;
dev_float sfHeliOutOfControlThrottleControlAdjustmentNeg = -0.8f;
dev_float sfHeliOutOfControlThrottleControlAdjustmentPos = 0.3f;

int siHeliOutOfControlRecoveryPeriodsMax = 3;

int siHeliOutOfControlInitialFailPeriodsMin = 100;
int siHeliOutOfControlInitialFailPeriodsMax = 400;

int siHeliOutOfControlFailPeriodsMin = 1200;
int siHeliOutOfControlFailPeriodsMax = 3500;

int siHeliOutOfControlRecoverPeriodsMin = 600;
int siHeliOutOfControlRecoverPeriodsMax = 1200;


dev_float sfHeliDrownPull = -10.0f;
dev_float sfInWaterRotorDamage = 300.0f;
dev_float sfMainRotorInWaterSplash = 1.0f;

dev_float sfRollYawSpeedThreshold = 5.0f;
dev_float sfRollYawSpeedBlendRate = 0.5f;
dev_float sfRollYawMult = -1.1f;

dev_float sfAutogyroRotorAccel = 0.04f;
dev_float sfAutogyroRotorDamping = 0.1f;
dev_float sfAutogyroRotorDampingC = 0.01f;

dev_float sfHoverModeYawMult = 10.0f;
dev_float sfHoverModePitchMult = 4.0f;

dev_float sfWantedTargetInHeliOrPlaneSpawnDistance = 400.0f;
dev_float sfMinWantedTargetInHeliOrPlaneVelocity = 25.0f;
dev_float sfMinSpawnOffsetAngleForWantedTarget = EIGHTH_PI;
dev_float sfMaxSpawnOffsetAngleForWantedTarget = EIGHTH_PI * 3.0f;

#if __BANK
static bool sbVisualiseHeliControls = false;
#endif

#if USE_SIXAXIS_GESTURES
bank_float CRotaryWingAircraft::MOTION_CONTROL_PITCH_MIN = -0.5f;
bank_float CRotaryWingAircraft::MOTION_CONTROL_PITCH_MAX = 0.5f;
bank_float CRotaryWingAircraft::MOTION_CONTROL_ROLL_MIN = -0.5f;
bank_float CRotaryWingAircraft::MOTION_CONTROL_ROLL_MAX = 0.5f;
bank_float CRotaryWingAircraft::MOTION_CONTROL_YAW_MULT = 2.5f;
#endif

dev_float HELI_RUDDER_MAX_ANGLE_OF_ATTACK = ( DtoR * 30.0f);

dev_float	HeliLight_FadeDistance = 250.0f;

//////////////////////////////////////////////////////////////////////////
// CRotaryWingAircraft

CRotaryWingAircraft::CRotaryWingAircraft(const eEntityOwnedBy ownedBy, u32 popType, VehicleType veh) : CAutomobile(ownedBy, popType, veh)
{
	m_fYawControl = 0.0f;
	m_fPitchControl = 0.0f;
	m_fRollControl = 0.0f;
	m_fThrottleControl = 0.0f;
	m_fCollectiveControl = 1.0f;

	m_fJoystickPitch = 0.0f;
	m_fJoystickRoll = 0.0f;

	m_fMainRotorSpeed = 0.0f;
	m_fPrevMainRotorSpeed = 0.0f;
	m_vecRearRotorPosition.Zero();

	m_nTailBoomGroup  = -1;

	m_fMainRotorHealth = VEH_DAMAGE_HEALTH_STD;
	m_fRearRotorHealth = VEH_DAMAGE_HEALTH_STD;
	m_fTailBoomHealth  = VEH_DAMAGE_HEALTH_STD;
	m_fMainRotorHealthDamageScale	= 1.0f;
	m_fRearRotorHealthDamageScale	= 1.0f;
	m_fTailBoomHealthDamageScale	= 1.0f;

	m_fMainRotorBrokenOffSmokeLifeTime = 0.0f;
	m_fRearRotorBrokenOffSmokeLifeTime = 0.0f;
	m_bCanBreakOffTailBoom = true;

	m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;

	m_nPhysicalFlags.bFlyer = true;
	m_nVehicleFlags.bNeverUseSmallerRemovalRange = true;

	m_iNumPropellers = 0;

	SetAllowFreezeWaitingOnCollision(false);

    m_bHoverMode = false;
	m_bStrafeMode = false;
	m_fStrafeModeTargetHeight = 0.0f;

    m_bEnableThrustVectoring = true;
    m_vHoverModeDesiredTarget = Vector3(0.0f, 0.0f, 0.0f);

	m_fSearchLightScore = 1.0f;
	m_bHadSearchLightLastFrame = false;
	m_fLastWheelContactTime = 0.0f;

	m_fHeliOutOfControlYaw = sfHeliTailLostYaw;
    m_fHeliOutOfControlPitch = sfHeliOutOfControlPitch;
	m_fHeliOutOfControlRoll = sfHeliTailLostRoll;
	m_fHeliOutOfControlThrottle = sfHeliLostRearRotorThrottle;

    m_uHeliOutOfControlRecoverStart = 0;
    m_iHeliOutOfControlRecoverPeriods = 0;
    m_bHeliOutOfControlRecovering = false;

	m_bHeliRotorDestroyedByPed = false;

	m_bDoBlowUpVehicle = false;

	m_bDisableTurbulanceThisFrame = false;
	m_bIsInAir = false;

	m_pilotSkillNoiseScalar = 1.0f;
	m_fControlLaggingRateMulti = 1.0f;
	m_fJetpackStrafeForceScale = 0.0f;
	m_fJetPackThrusterThrotle  = 0.0f;
	m_fWingAngle[ 0 ] = 0.0f;
	m_fWingAngle[ 1 ] = 0.0f;
	m_uBreakOffTailBoomPending = Break_Off_Tail_Boom_Immediately;

	m_windowBoneCached = false;

	m_windowBoneIndices[0] = 0;
	m_windowBoneIndices[1] = 0;
	m_windowBoneIndices[2] = 0;
	m_windowBoneIndices[3] = 0;

	m_bDisableAutomaticCrashTask = false;

	CFlyingVehicleAvoidanceManager::AddVehicle(RegdVeh(this));
}

CRotaryWingAircraft::~CRotaryWingAircraft()
{
	CFlyingVehicleAvoidanceManager::RemoveVehicle(RegdVeh(this));
}

bool CRotaryWingAircraft::GetSearchLightOn()
{ 
	CSearchLight *pSearchLight = GetSearchLight();

	return (pSearchLight && pSearchLight->GetLightOn()); 
}

void CRotaryWingAircraft::SetSearchLightOn(bool bOn) 
{ 
	CSearchLight *pSearchLight = GetSearchLight();
	if(pSearchLight) 
	{
		pSearchLight->SetLightOn(bOn); 
	}
}

int CRotaryWingAircraft::InitPhys()
{
	CAutomobile::InitPhys();

	fragInstGta* pFragInst = GetVehicleFragInst();
	Assert(pFragInst);

	if(!InheritsFromBlimp() && GetBoneIndex(HELI_TAIL) > -1)
	{
		s16 nGroup = (s16)pFragInst->GetGroupFromBoneIndex(GetBoneIndex(HELI_TAIL));
		if(nGroup > -1)
		{
			m_nTailBoomGroup = (s8)nGroup;
			fragTypeGroup* pTailGroup = pFragInst->GetTypePhysics()->GetAllGroups()[nGroup];

			int nFirstChild = pTailGroup->GetChildFragmentIndex();
			int nNumChildren = pTailGroup->GetNumChildren();

			// make sure this component doesn't break off until we want it to
			for(int nChild=nFirstChild; nChild < nFirstChild + nNumChildren; nChild++)
			{
				((fragInstGta*)pFragInst)->SetDontBreakFlag(BIT(nChild));
			}
		}
	}

	return INIT_OK;
}

void CRotaryWingAircraft::InitDoors()
{
	CVehicle::InitDoors();

	Assert(GetNumDoors()==0);
	m_pDoors = GetCarDoorPointer();
	m_nNumDoors = 0;

	if(GetBoneIndex(VEH_DOOR_DSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_F, -0.4f*PI, 0.0f, CCarDoor::AXIS_Z|CCarDoor::WILL_LOCK_SWINGING);
		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_PSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_F, 0.4f*PI, 0.0f, CCarDoor::AXIS_Z|CCarDoor::WILL_LOCK_SWINGING);
		m_nNumDoors++;
	}

	// heli's have sliding doors on the rear
	if(GetBoneIndex(VEH_DOOR_DSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_R, -1.0f, 0.0f, CCarDoor::AXIS_SLIDE_Y);
		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_PSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_R, -1.0f, 0.0f, CCarDoor::AXIS_SLIDE_Y);
		m_nNumDoors++;
	}

	if(GetBoneIndex(VEH_FOLDING_WING_L) > -1)
	{
		u32 uInitFlags = CCarDoor::AXIS_Y|CCarDoor::DONT_BREAK;

		m_pDoors[m_nNumDoors].Init(this, VEH_FOLDING_WING_L, 0.1f*PI, 0.0f, uInitFlags);
		m_nNumDoors++;
	}	

	if(GetBoneIndex(VEH_FOLDING_WING_R) > -1)
	{
		u32 uInitFlags = CCarDoor::AXIS_Y|CCarDoor::DONT_BREAK;

		m_pDoors[m_nNumDoors].Init(this, VEH_FOLDING_WING_R, 0.1f*PI, 0.0f, uInitFlags);
		m_nNumDoors++;
	}	
}

void CRotaryWingAircraft::UpdateRotorBounds()
{
	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());	
	Assert(pBoundComp);

	if(pBoundComp->GetTypeAndIncludeFlags())
	{
		// turn collision on for rear boom on helicopter 
		if(GetBoneIndex(HELI_TAIL) > -1)
		{		
			int nChildIndex = GetVehicleFragInst()->GetComponentFromBoneIndex(GetBoneIndex(HELI_TAIL));
			if(nChildIndex != -1)
			{
				pBoundComp->SetIncludeFlags(nChildIndex, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
			}
		}

		// Treat the rotor like wheels
		// GTAV - B*1954524 - if the rotor disc is stationary it shouldn't collide with anything.
		u32 nRotorIncludeFlags = 0;

		// don't let rotors hit ped bounds, only ragdoll bounds, since it's a non-physical collision anyway and we don't want peds to stand on rotors
		// Remove this to fix the ped not hit by rotors. The issue that ped able to stand on rotors has been fixed in the other place.
		//nRotorIncludeFlags &= ~ArchetypeFlags::GTA_PED_TYPE;
		u32 nCameraTestFlag = m_fMainRotorSpeed > 0.0f ?  0 : ArchetypeFlags::GTA_CAMERA_TEST;
		if( m_fMainRotorSpeed > 0.0f )
		{
			nRotorIncludeFlags = ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;
			nRotorIncludeFlags &= ~(ArchetypeFlags::GTA_AI_TEST | ArchetypeFlags::GTA_SCRIPT_TEST | ArchetypeFlags::GTA_WHEEL_TEST | nCameraTestFlag);
		}

		u32 nBladeIncludeFlags = ArchetypeFlags::GTA_WEAPON_AND_PROJECTILE_INCLUDE_TYPES | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | nCameraTestFlag;

		if(GetMainRotorGroup() > -1)
		{
			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[GetMainRotorGroup()];
			if(pGroup->GetNumChildren() > 1)
			{
				for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
				{
					pBoundComp->SetIncludeFlags(GetMainRotorChild() + iChild, nBladeIncludeFlags);
				}
			}
		}

		if(GetMainRotorDisc() > 0)	// don't set GTA_ALL_SHAPETEST_TYPES so probes won't hit rotors
		{
			pBoundComp->SetIncludeFlags(GetMainRotorDisc(), nRotorIncludeFlags);
		}

		if(GetRearRotorGroup() > -1)
		{
			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[GetRearRotorGroup()];
			if(pGroup->GetNumChildren() > 1)
			{
				for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
				{
					pBoundComp->SetIncludeFlags(GetRearRotorChild() + iChild, nBladeIncludeFlags);
				}
			}
		}

		if(GetRearRotorDisc() > 0)
		{
			pBoundComp->SetIncludeFlags(GetRearRotorDisc(), nRotorIncludeFlags);
		}

		if( m_iNumPropellers == Num_Drone_Rotors )
		{
			// the drone has 4 rotors so check for those
			if(GetMain2RotorGroup() > -1)
			{
				fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[GetMain2RotorGroup()];
				if(pGroup->GetNumChildren() > 1)
				{
					for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
					{
						pBoundComp->SetIncludeFlags(GetMain2RotorChild() + iChild, nBladeIncludeFlags);
					}
				}
			}

			if(GetRear2RotorGroup() > -1)
			{
				fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[GetRear2RotorGroup()];
				if(pGroup->GetNumChildren() > 1)
				{
					for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
					{
						pBoundComp->SetIncludeFlags(GetRear2RotorChild() + iChild, nBladeIncludeFlags);
					}
				}
			}

			if(GetMain2RotorDisc() > 0)	// don't set GTA_ALL_SHAPETEST_TYPES so probes won't hit rotors
			{
				pBoundComp->SetIncludeFlags(GetMain2RotorDisc(), nRotorIncludeFlags);
			}
			if(GetRear2RotorDisc() > 0)	// don't set GTA_ALL_SHAPETEST_TYPES so probes won't hit rotors
			{
				pBoundComp->SetIncludeFlags(GetRear2RotorDisc(), nRotorIncludeFlags);
			}
		}

		// B*1928675: Allow peds to collide with the skids.
		for(int boneId = VEH_SUSPENSION_LF; boneId <= VEH_SUSPENSION_RF; boneId++)
		{
			int boneIdx = GetBoneIndex((eHierarchyId)boneId);
			if(boneIdx > -1)
			{		
				int nGroupIndex = GetVehicleFragInst()->GetGroupFromBoneIndex(boneIdx);
				if(nGroupIndex != -1)
				{
					fragTypeGroup *pGroup = GetVehicleFragInst()->GetTypePhysics()->GetGroup(nGroupIndex);
					int nChildIndex = pGroup->GetChildFragmentIndex();
					for(int nChild = 0; nChild < pGroup->GetNumChildren(); ++nChild)
					{
						pBoundComp->SetIncludeFlags(nChildIndex + nChild, pBoundComp->GetIncludeFlags(nChildIndex + nChild) | ArchetypeFlags::GTA_PED_TYPE);						
					}
				}
			}
		}
	}	
}

void CRotaryWingAircraft::InitCompositeBound()
{
	CAutomobile::InitCompositeBound();

	UpdateRotorBounds();

	if( GetModelIndex() == MI_HELI_AKULA )
	{
		m_nVehicleFlags.bUseDeformation = false;
	}
}


bool CRotaryWingAircraft::WantsToBeAwake()
{
	if(GetIsAttached() && !GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
		&& GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_BASIC)
	{
		return false;
	}

	if(GetStatus()==STATUS_WRECKED)
		return false;



	if(m_nVehicleFlags.bEngineOn && (Abs(m_fThrottleControl) + Abs(m_fYawControl) + Abs(m_fPitchControl) + Abs(m_fRollControl)) > 0.001f)
	{	
		bool bAllowActivation = !GetIsAttached() || GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
			|| GetAttachmentExtension()->GetAttachState()!=ATTACH_STATE_BASIC;
		return bAllowActivation && !(IsRunningCarRecording() && CanBeInactiveDuringRecording());
	}


	if( HasContactWheels() == false )
	{
		return true;
	}

	if( IsWheelContactPhysicalMoving() )
	{
		return true;
	}

	return false;
}


float CRotaryWingAircraft::ComputeAdditionalYawFromTransform(float fYaw, const fwTransform& transform, float fFlatMoveSpeed)
{
    //add some extra yaw when flying forward and rolling
    if(fFlatMoveSpeed > sfRollYawSpeedThreshold)
    {
        fFlatMoveSpeed -= sfRollYawSpeedThreshold;
        return (fYaw + transform.GetRoll() * rage::Min(1.0f, fFlatMoveSpeed * sfRollYawSpeedBlendRate) * sfRollYawMult);
    }

    return fYaw;
}

CSearchLight* CRotaryWingAircraft::GetSearchLight()
{ 
	CVehicleWeaponMgr* pWeaponMgr = GetVehicleWeaponMgr();

	if(pWeaponMgr)
	{
		for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
		{
			CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
			if(pWeapon && pWeapon->GetType() == VGT_SEARCHLIGHT)
			{
				return static_cast<CSearchLight*>(pWeapon);
			}
		}
	}

	return NULL; 
}

bool CRotaryWingAircraft::GetIsDrone() const
{
	return (GetModelIndex() == MI_HELI_DRONE || GetModelIndex() == MI_HELI_DRONE_2);
}

bool CRotaryWingAircraft::GetIsJetPack() const
{
	return (GetModelIndex() == MI_JETPACK_THRUSTER);
}
//
//
// CRotaryWingAircraft::ProcessControl()
//
//

static dev_float dfJoystickSmoothSpeed = 5.0f;

void CRotaryWingAircraft::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity(), vThisPosition );

// DEBUG!! -AC, This call to process control is either at the wrong place (it should be moved to the bottom of this function)
// or it is at the right place (and all other vehicle::ProcessControl functions should emulate it).
	// then can go ahead and call the parent version of process control
	CAutomobile::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
// END DEBUG!!

	bool bDoSearchLight = false;
	bool bFireSearchLightGun = false;
	CSearchLight* pSearchLight = GetSearchLight();

	if(pSearchLight && !IsNetworkClone()) //Making sure game does not crash if there is no searchlight
	{
#if GTA_REPLAY
		if(!CReplayMgr::IsEditModeActive())
#endif
		{
			// Deal with the searchlight
			if( (GetStatus() != STATUS_WRECKED) &&
				(true == m_nVehicleFlags.bEngineOn) &&
				!CCullZones::PlayerNoRain() &&
				(GetDriver() || IsUsingPretendOccupants()) &&
				!pSearchLight->GetIsDamaged() )
			{
				//if player is controlling the chopper then don't change the state
				if(GetDriver() && GetDriver()->IsPlayer())
				{
					//Skip everything
				}
				else
				{
					CVehicleModelInfo* vmi = GetVehicleModelInfo();
					Assert(vmi);

					CPed* pPlayerPed = NULL;

					// Do some checks to see if we should be a wanted dispatched heli that uses searchlight
					bool bUseWantedSearchLight = false;
					if( !this->m_nVehicleFlags.bMoveAwayFromPlayer &&
						vmi->GetVehicleType() == VEHICLE_TYPE_HELI && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT) )
					{			
						pPlayerPed = !NetworkInterface::IsGameInProgress() ? CGameWorld::FindLocalPlayer() : NetworkInterface::FindClosestWantedPlayer(VEC3V_TO_VECTOR3(GetTransform().GetPosition()),WANTED_LEVEL_HELI_1_DISPATCHED);
					
						CWanted* pPlayerWanted = pPlayerPed ? pPlayerPed->GetPlayerWanted() : NULL;

						//Checking both real and fake wanted level as scripters might set one of these
						if( (pPlayerWanted && pPlayerWanted->GetWantedLevel() >= WANTED_LEVEL_HELI_1_DISPATCHED) ||
							CScriptHud::iFakeWantedLevel >= WANTED_LEVEL_HELI_1_DISPATCHED )
						{
							bUseWantedSearchLight = true;

							if(pPlayerPed)
							{
								// If the target is in a vehicle we need to make sure it's a valid type
								CVehicle* pPlayerVehicle = pPlayerPed->GetIsInVehicle() ? pPlayerPed->GetMyVehicle() : NULL;
								if(pPlayerVehicle)
								{
									VehicleType vehicleType = pPlayerVehicle->GetVehicleType();
									if( vehicleType == VEHICLE_TYPE_HELI ||
										vehicleType == VEHICLE_TYPE_PLANE ||
										(vehicleType == VEHICLE_TYPE_SUBMARINE && pPlayerVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER) )
									{
										bUseWantedSearchLight = false;
									}
								}
							}
						}
					}

					if (bUseWantedSearchLight)
					{
						bDoSearchLight = true;
						bFireSearchLightGun = true;
						SetSearchLightTarget((CPhysical *)pPlayerPed);
					}
					else
					{
						aiTask *pTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority( VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_FOLLOW, VEHICLE_TASK_PRIORITY_PRIMARY);

						CTaskVehicleMissionBase *pCarTask = NULL;

						if(pTask)
						{
							Assert(dynamic_cast<CTaskVehicleMissionBase*>(pTask));
							pCarTask = static_cast<CTaskVehicleMissionBase*>(pTask);
						}

						if(pCarTask && pCarTask->GetTargetEntity())
						{
							bDoSearchLight = true;
							bFireSearchLightGun = false;
							SetSearchLightTarget((CPhysical *)pCarTask->GetTargetEntity());
						}
						else
						{
							SetSearchLightTarget(NULL);
						}
					}

					//This check is here so that the checks below still has a chance to turn the light off
					//GetForceLightOn is set by script, but we would still like it to perform the conditions below
					if(pSearchLight->GetForceLightOn())
					{
						bDoSearchLight = true;
					}

					if (this->GetIsInWater()
						|| (pSearchLight->GetSearchLightTarget() == NULL && !pSearchLight->GetForceLightOn())
						|| (CClock::GetHour() < 19 && CClock::GetHour() >= 7) 
						|| CPortal::IsInteriorScene() 
						)
					{
						bDoSearchLight = false;
						bFireSearchLightGun = false;
					}

					// If we picked a target that is actually above us we don't render the search light.
					if (pSearchLight->GetSearchLightTarget() && pSearchLight->GetSearchLightTarget()->GetTransform().GetPosition().GetZf() > vThisPosition.z && !pSearchLight->GetForceLightOn())
					{
						bDoSearchLight = false;
						bFireSearchLightGun = false;
					}

					// if our driver is dead
					if (!GetDriver() || GetDriver()->IsDead())
					{
						bDoSearchLight = false;
						bFireSearchLightGun = false;
					}
				
					pSearchLight->SetLightOn(bDoSearchLight);
				}        
			}
			else
			{
				pSearchLight->SetLightOn(false);
			}	
		}

		pSearchLight->ProcessSearchLight(this);

		ProcessSearchLightScoring();
	}

	if(g_InterestingEvents.IsActive())
	{
		// chance is once in every 10 secs
		float fTimeStep = fwTimer::GetTimeStep();
		float fChance = fTimeStep / 10.0f;
		if(bFireSearchLightGun)
			fChance *= 2.0f;
		float fRandVal = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		if(fRandVal < fChance)
		{
			//Register this as an 'interesting event'
			g_InterestingEvents.Add(CInterestingEvents::EHelicopterOverhead, this);
		}
	}

	// Add in a shocking event if the helicopter is flying
	if (GetDriver() && GetDriver()->IsPlayer())
	{
		if( m_fMainRotorSpeed > 0.1f && ShouldGenerateEngineShockingEvents() )
		{
			CEventShockingHelicopterOverhead ev(*this);
			CShockingEventsManager::Add(ev);
		}
	}

	// check if tail has broken off, and delete any wheels that may have been attached to it
	if(m_nTailBoomGroup > -1 && GetVehicleFragInst()->GetGroupBroken(m_nTailBoomGroup))
	{
		for(int i=0; i<GetNumWheels(); i++)
		{
			CWheel* pWheel = GetWheel(i);
			int nFragChild = pWheel->GetFragChild();
            if(nFragChild > -1)
            {
			    fragPhysicsLOD* pTypePhysics = GetVehicleFragInst()->GetTypePhysics();
			    fragTypeChild* pChild = pTypePhysics->GetAllChildren()[nFragChild];
			    if(pWheel && pWheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL) && pChild->GetOwnerGroupPointerIndex() > 0)
			    {
				    if(!GetVehicleFragInst()->GetChildBroken(nFragChild))
					    GetVehicleFragInst()->DeleteAbove(nFragChild);
			    }
            }
		}
	}

	// smooth out the joystick control
	if(GetDriver() && GetDriver()->IsInFirstPersonVehicleCamera())
	{
		TUNE_GROUP_FLOAT(JOYSTICK_IK, HeliJoyStickLerp, 0.1f, 0.0f, 1.0f, 0.001f);
		m_fJoystickPitch = rage::Lerp(HeliJoyStickLerp, m_fJoystickPitch, m_fPitchControl);
		m_fJoystickRoll = rage::Lerp(HeliJoyStickLerp, m_fJoystickRoll, m_fRollControl);

		CTaskMotionBase *pCurrentMotionTask = GetDriver()->GetCurrentMotionTask();
		if (pCurrentMotionTask && pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
		{
			const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pCurrentMotionTask);
			float fAnimWeight = pAutoMobileTask->GetSteeringWheelWeight();
			m_fJoystickPitch *= fAnimWeight;
			m_fJoystickRoll *= fAnimWeight;
		}
	}
	else
	{
		float fMaxDelta = fwTimer::GetTimeStep() * dfJoystickSmoothSpeed;
		m_fJoystickPitch += rage::Clamp(m_fPitchControl - m_fJoystickPitch, -fMaxDelta, fMaxDelta);
		m_fJoystickRoll += rage::Clamp(m_fRollControl - m_fJoystickRoll, -fMaxDelta, fMaxDelta);
	}

#if __BANK	

	if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
		&& (GetStatus()==STATUS_PLAYER))
	{
		static int x = 4;
		static int y = 20;

		char debugText[100];
		float fSpeed = GetVelocity().Mag() * 3.6f;
		float fSpeedHoriz = GetVelocity().XYMag() * 3.6f;
		float fSpeedVert = GetVelocity().z * 3.6f;
		sprintf(debugText, "Spd:%3.1f km/h (Horiz:%3.1f, Vert:%3.1f)", fSpeed, fSpeedHoriz, fSpeedVert);
		grcDebugDraw::PrintToScreenCoors(debugText, x,y);

		sprintf(debugText, "Throttle:%1.2f, Engine:%1.2f", m_fThrottleControl, m_fMainRotorSpeed);
		grcDebugDraw::PrintToScreenCoors(debugText, x,y+1);
	}

	if(CVehicle::ms_nVehicleDebug == VEH_DEBUG_DAMAGE && (GetStatus()==STATUS_PLAYER))
	{	
		static int x = 75;
		static int y = 22;

		// Print rotor healths
		char debugText[100];
		formatf(debugText,"Main rotor health: %f", GetMainRotorHealth());
		grcDebugDraw::PrintToScreenCoors(debugText, x,y);

		formatf(debugText,"Rear rotor health: %f", GetRearRotorHealth());
		grcDebugDraw::PrintToScreenCoors(debugText, x,y+1);

		formatf(debugText,"Tail boom health: %f", GetTailBoomHealth());
		grcDebugDraw::PrintToScreenCoors(debugText, x,y+2);
	}
#endif

#if __BANK
	// Do some heli control debugging
	if(CDebugScene::FocusEntities_Get(0) == this)
	{
		VisualisePitchRoll();
	}
#endif	// __BANK
}


float sfHeliAbandonedYawMult = 0.5f;
float sfHeliAbandonedPitchMult = 1.0f;
float HELI_CRASH_THROTTLE	= -0.1f;
float HELI_CRASH_PITCH		= 0.4f;
float HELI_CRASH_YAW		= 0.4f;
float HELI_CRASH_ROLL		= 0.4f;
float HELI_ABANDONED_THROTTLE = 0.5f;

dev_float HELI_THROTTLE_CONTROL_DAMPING = 0.1f;
dev_float HELI_AUTO_THROTTLE_FALLOFF = 0.2f;
dev_float CRotaryWingAircraft::ms_fPreviousSearchLightOwnerBonus = 0.7f;
dev_float CRotaryWingAircraft::ms_fUnblockedSearchLightBonus = 0.5f;

float CRotaryWingAircraft::ms_fBestSearchLightScore = 1.0f;
bool CRotaryWingAircraft::ms_bNeedToResetSearchLightScoring = true;
dev_float dfHeliPushByWindMillSpeed = 5.0f;
dev_float dfCrushPedMinSpeed = 0.2f;

void CRotaryWingAircraft::ProcessPreComputeImpacts(phContactIterator impacts)
{
	static dev_float fMinRotorSpeedForDamage = 0.1f;
	float fSpeedForDamage = m_fMainRotorSpeed < fMinRotorSpeedForDamage ? 0.0f : m_fMainRotorSpeed;

	impacts.Reset();
	while(!impacts.AtEnd())
	{
        phInst* pOtherInstance = impacts.GetOtherInstance();
        CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);

        if( ProcessFoliageImpact( pOtherInstance, impacts.GetMyPosition(), impacts.GetMyComponent(), impacts.GetOtherComponent(), impacts.GetOtherElement() ) )
        {
            // Now that we've noted the collision, disable the impact.
            impacts.DisableImpact();
            ++impacts;
            continue;
        }

		for(int i = 0; i < m_iNumPropellers; i++)
		{
			m_propellerCollisions[i].ProcessPreComputeImpacts(this,impacts,fSpeedForDamage);
		}

		if(!impacts.IsDisabled())
		{
			if(pOtherEntity)
			{
				if(pOtherEntity->GetArchetype() && pOtherEntity->GetModelIndex() == MI_WINDMILL)
				{
					impacts.SetDepth(Min(dfHeliPushByWindMillSpeed * fwTimer::GetTimeStep(), impacts.GetDepth()));
				}

				// Blow up as early as possible when hitting the map while crashing.
				if(pOtherEntity->GetIsTypeBuilding() && !IsPropeller(impacts.GetMyComponent()))
				{
					if(NULL != GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(
						VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH))
					{
						m_bDoBlowUpVehicle = true;
					}
				}

				if(NetworkInterface::IsGameInProgress() && pOtherEntity->GetIsTypePed())
				{
					Vec3V vPedNormal;
					impacts.GetOtherNormal(vPedNormal);
					if(vPedNormal.GetZf() < 0.0f)
					{
						Vec3V vVehicleVelocity(VECTOR3_TO_VEC3V(GetLocalSpeed(VEC3V_TO_VECTOR3(impacts.GetMyPosition()), true, impacts.GetMyComponent())));
						if(vVehicleVelocity.GetZf() * vPedNormal.GetZf() > dfCrushPedMinSpeed
							&& impacts.GetDepth() > CTaskNMBehaviour::sm_Tunables.m_MinContactDepthForContinuousPushActivation)
						{
							static_cast<CPed *>(pOtherEntity)->SetPedResetFlag(CPED_RESET_FLAG_CapsuleBeingPushedByVehicle, true);
						}
					}
				}
			}
			
			if( !m_bIsInAir )
			{
				if( pOtherEntity && pOtherEntity->GetIsTypeVehicle() )
				{
					// GTAV - B*1724742 - cars can pick up helicopters
					// if the helicopter is on the ground, upright, and is colliding with a vehicle
					// test the normal. 
					// if the normal is trying to go under the vehicle flatten it to only be on the x y axis
					Vector3 myNormal;// = VECTOR3_ZERO;
					impacts.GetMyNormal( myNormal );

					if( GetTransform().GetUp().GetZf() > 0.6f &&
						myNormal.GetZ() < 0.95f &&
						myNormal.GetZ() > 0.3f )
					{
						myNormal.SetZ( 0.0f );
						myNormal.Normalize();

						impacts.SetMyNormal( myNormal );
					}
				}
			}
		}
		impacts++;
	}

	impacts.Reset();
	CVehicle::ProcessPreComputeImpacts(impacts);
}

void CRotaryWingAircraft::ProcessPostPhysics()
{
	if (m_bDoBlowUpVehicle)
	{
		m_bDoBlowUpVehicle = false;
		if(CTaskVehicleCrash* pTask = static_cast<CTaskVehicleCrash*>(GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(
			VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH)))
		{
			pTask->DoBlowUpVehicle(this);
		}
	}

	//crash the helicopter if it's below the minimum height
	if(GetStatus() != STATUS_WRECKED && !m_bDisableAutomaticCrashTask)
	{
		Vec3V vVehiclePos = GetTransform().GetPosition();
		float fMinHeightAtPos = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vVehiclePos.GetXf(), vVehiclePos.GetYf());
		bool bBelowMinHeight = vVehiclePos.GetZf() < fMinHeightAtPos;
		if(bBelowMinHeight)
		{
			StartCrashingBehavior();
			m_bDisableAutomaticCrashTask = true;
		}
	}

	CAutomobile::ProcessPostPhysics();
}

float CRotaryWingAircraft::GetHoverModeYawMult() 
{ 
    return (m_bHoverMode ?  sfHoverModeYawMult : 1.0f);
}

float CRotaryWingAircraft::GetHoverModePitchMult() 
{ 
    return (m_bHoverMode ?  sfHoverModePitchMult : 1.0f);
}

dev_float dfHeliRotorBrokenOffSmokeLifeTime = 8.0f;

void CRotaryWingAircraft::BreakOffMainRotor()
{
	fragInstGta* fragInstVehicle = GetVehicleFragInst();
	int iChild = m_propellerCollisions[Rotor_Main].GetFragChild();

	if (fragInstVehicle && (iChild > -1) && !fragInstVehicle->GetChildBroken(iChild))
	{
		if (CarPartsCanBreakOff())
		{
			// rotor destruction effect
			g_vfxVehicle.TriggerPtFxHeliRotorDestroy(this, false);

			if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
			{
				static_cast<audHeliAudioEntity*>(GetVehicleAudioEntity())->PlayRotorBreakSound();
			}
		}

		GetVehicleFragInst()->DeleteAbove(iChild);

		m_fMainRotorHealth = 0.0f;
		m_fMainRotorSpeed = 0.0f;
		m_fMainRotorBrokenOffSmokeLifeTime = dfHeliRotorBrokenOffSmokeLifeTime;

		if (!IsNetworkClone() && GetWeaponDamageEntity() && GetWeaponDamageEntity()->GetIsTypePed())
		{
			m_bHeliRotorDestroyedByPed = true;
		}
	}

	if( GetIsDrone() )
	{
		int iChild = m_propellerCollisions[Rotor_Main2].GetFragChild();

		if (fragInstVehicle && (iChild > -1) && !fragInstVehicle->GetChildBroken(iChild))
		{
			GetVehicleFragInst()->DeleteAbove(iChild);
		}
	}
}

void CRotaryWingAircraft::BreakOffRearRotor()
{
	fragInstGta* fragInstVehicle = GetVehicleFragInst();
	int iChild = m_propellerCollisions[Rotor_Rear].GetFragChild();

	if (fragInstVehicle && (iChild > -1) && !GetVehicleFragInst()->GetChildBroken(iChild))
	{
		if (CarPartsCanBreakOff())
		{
			// rotor destruction effect
			g_vfxVehicle.TriggerPtFxHeliRotorDestroy(this, true);

			if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
			{
				static_cast<audHeliAudioEntity*>(GetVehicleAudioEntity())->PlayRearRotorBreakSound();
			}
		}

		GetVehicleFragInst()->DeleteAbove(iChild);
		m_fRearRotorHealth = 0.0f;
		m_fRearRotorBrokenOffSmokeLifeTime = dfHeliRotorBrokenOffSmokeLifeTime;
	}

	if( GetIsDrone() )
	{
		int iChild = m_propellerCollisions[Rotor_Rear2].GetFragChild();

		if (fragInstVehicle && (iChild > -1) && !GetVehicleFragInst()->GetChildBroken(iChild))
		{
			GetVehicleFragInst()->DeleteAbove(iChild);
		}
	}
}


dev_float dfHeliBrokenOffTailBoomMinMass = 50.0f;
dev_float dfHeliBrokenOffTailBoomMinHeliMass = 1000.0f;

void CRotaryWingAircraft::BreakOffTailBoom(int eBreakingState)
{
	
	if(eBreakingState != Break_Off_Tail_Boom_Immediately && CApplyDamage::GetNumDamagePending(this) > 0)
	{
		m_uBreakOffTailBoomPending = (u8)eBreakingState;
		return;
	}


	CPropeller& rearPropeller = GetRearPropeller();
	rearPropeller.SetPropellerSpeed(0.0f);
	rearPropeller.PreRender(this); //To set the fast bone to the slow bone

	fragInstGta* fragInstVehicle = GetVehicleFragInst();

	if (fragInstVehicle && (m_nTailBoomGroup > -1) && !fragInstVehicle->GetGroupBroken(m_nTailBoomGroup) && GetCanBreakOffTailBoom())
	{
		if (CarPartsCanBreakOff())
		{
			fragInst* pNewFragInst = fragInstVehicle->BreakOffAboveGroup(m_nTailBoomGroup);
			if (pNewFragInst)
			{
				fragCacheEntry* pCacheEntry = pNewFragInst->GetCacheEntry();

				if( pCacheEntry &&
					pCacheEntry->GetHierInst() &&
					GetMass() > dfHeliBrokenOffTailBoomMinHeliMass &&
					pCacheEntry->GetMass( pCacheEntry->GetHierInst() ) <= dfHeliBrokenOffTailBoomMinMass )
				{
					pCacheEntry->SetMass( dfHeliBrokenOffTailBoomMinMass );
				}

				CEntity* pNewEntity = CPhysics::GetEntityFromInst(pNewFragInst);
				if (pNewEntity)
				{
					g_vfxVehicle.TriggerPtFxVehicleDebris(pNewEntity);

					if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
					{
						static_cast<audHeliAudioEntity*>(GetVehicleAudioEntity())->PlayTailBreakSound();
					}
				}
			}
		}
		else
		{
			fragInstVehicle->DeleteAboveGroup(m_nTailBoomGroup);
		}

		m_fTailBoomHealth = 0.0f;
		m_fRearRotorBrokenOffSmokeLifeTime = 0.0f; // Stop playing rear rotor broken off smoke
	}

	m_uBreakOffTailBoomPending = Break_Off_Tail_Boom_Immediately;
}

void CRotaryWingAircraft::PostBoundDeformationUpdate()
{
	if(m_uBreakOffTailBoomPending == Break_Off_Tail_Boom_Pending_Bound_Update && CApplyDamage::GetNumDamagePending(this) == 0)
	{
		BreakOffTailBoom();
	}

	CVehicle::PostBoundDeformationUpdate();
}

bool CRotaryWingAircraft:: HasBoundUpdatePending() const
{
	return (m_uBreakOffTailBoomPending == Break_Off_Tail_Boom_Pending_Bound_Update) || CVehicle::HasBoundUpdatePending();
}

bool CRotaryWingAircraft::GetIsTailBoomBroken() const
{
	return GetVehicleFragInst() && m_nTailBoomGroup > -1 && GetVehicleFragInst()->GetGroupBroken(m_nTailBoomGroup);
}

bool CRotaryWingAircraft::GetIsMainRotorBroken() const
{
	int iChild = m_propellerCollisions[Rotor_Main].GetFragChild();
	return GetVehicleFragInst() && iChild > -1 && GetVehicleFragInst()->GetChildBroken(iChild);
}

bool CRotaryWingAircraft::GetIsRearRotorBroken() const
{
	int iChild = m_propellerCollisions[Rotor_Rear].GetFragChild();
	return GetVehicleFragInst() && iChild > -1 && GetVehicleFragInst()->GetChildBroken(iChild);
}

bool CRotaryWingAircraft::IsPropeller (int nComponent) const
{
	bool rotorStopped = ( GetMainRotorSpeed() == 0.0f );

	for(int i = 0; i < m_iNumPropellers; i++)
	{
		if( m_propellerCollisions[i].IsPropellerComponent( this, nComponent ) )
		{
			// Allow collision with blade bound when its stationary
			if( !rotorStopped || nComponent == m_propellerCollisions[ i ].GetFragDisc() )
			{
				return true;
			}
		}
	}

	return false;
}

ePrerenderStatus CRotaryWingAircraft::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	for(int i= 0; i < m_iNumPropellers; i++)
	{
		m_propellers[i].PreRender(this);
	}

	if(GetStatus() == STATUS_PLAYER)
	{
        if( !GetIsJetPack() )
        {
		    TUNE_GROUP_FLOAT(JOYSTICK_IK, HeliJoyStickRotX, 4.0f, 0.0f, 20.0f, 0.001f);
		    TUNE_GROUP_FLOAT(JOYSTICK_IK, HeliJoyStickRotY, 6.5f, 0.0f, 20.0f, 0.001f);
		    float fJoystickAdjustAngleX = ( DtoR * HeliJoyStickRotX);
		    float fJoystickAdjustAngleY = ( DtoR * HeliJoyStickRotY);
		    SetComponentRotation(VEH_CAR_STEERING_WHEEL, ROT_AXIS_LOCAL_X, m_fJoystickPitch * fJoystickAdjustAngleX, true);
		    SetComponentRotation(VEH_CAR_STEERING_WHEEL, ROT_AXIS_LOCAL_Y, -m_fJoystickRoll * fJoystickAdjustAngleY, false);
        }
        else
        {
            TUNE_GROUP_FLOAT(JOYSTICK_JETPACK_IK, JetPackJoyStickRotX, 6.5f, 0.0f, 20.0f, 0.001f);
            TUNE_GROUP_FLOAT(JOYSTICK_JETPACK_IK, JetPackJoyStickRotY, 6.5f, 0.0f, 20.0f, 0.001f);
            float fJoystickAdjustAngleX = ( DtoR * JetPackJoyStickRotX);
            float fJoystickAdjustAngleY = ( DtoR * JetPackJoyStickRotY);
            SetComponentRotation(HELI_HBGRIP_LOW_R, ROT_AXIS_LOCAL_X, m_fJoystickPitch * fJoystickAdjustAngleX, true);
            SetComponentRotation(HELI_HBGRIP_LOW_R, ROT_AXIS_LOCAL_Y, -m_fJoystickRoll * fJoystickAdjustAngleY, false);

            SetComponentRotation(HELI_HBGRIP_LOW_L, ROT_AXIS_LOCAL_X, m_fThrottleControl * fJoystickAdjustAngleX, true);
            SetComponentRotation(HELI_HBGRIP_LOW_L, ROT_AXIS_LOCAL_Y, m_fYawControl * fJoystickAdjustAngleY, false);

            GetSkeleton()->PartialUpdate( GetBoneIndex( HELI_HBGRIP_LOW_R ) );
            GetSkeleton()->PartialUpdate( GetBoneIndex( HELI_HBGRIP_LOW_L ) );
        }
	}

	// vfx
	if (!IsDummy() && !m_nVehicleFlags.bIsDrowning && 
		!m_nVehicleFlags.bDisableParticles)
	{
		if(m_fMainRotorBrokenOffSmokeLifeTime > 0.0f && GetIsMainRotorBroken())
		{
			if(GetBoneIndex(HELI_ROTOR_MAIN) > -1)
			{
				Vector3 vPos;
				GetDefaultBonePositionForSetup(HELI_ROTOR_MAIN, vPos);
				g_vfxVehicle.UpdatePtFxAircraftSectionDamageSmoke(this, RCC_VEC3V(vPos), 0);
			}
		}

		if(m_fRearRotorBrokenOffSmokeLifeTime > 0.0f && GetIsRearRotorBroken() && !GetIsTailBoomBroken())
		{
			if(GetBoneIndex(HELI_ROTOR_REAR) > -1)
			{
				Vector3 vPos;
				GetDefaultBonePositionForSetup(HELI_ROTOR_REAR, vPos);
				g_vfxVehicle.UpdatePtFxAircraftSectionDamageSmoke(this, RCC_VEC3V(vPos), 1);
			}
		}

		if ((m_nVehicleFlags.bEngineOn || IsRunningCarRecording()))
		{
			g_vfxVehicle.UpdatePtFxPlaneAfterburner(this, HELI_AFTERBURNER, 0);
			g_vfxVehicle.UpdatePtFxPlaneAfterburner(this, HELI_AFTERBURNER_2, 1);
		}
	}

	return CAutomobile::PreRender(bIsVisibleInMainViewport);
}

void CRotaryWingAircraft::CacheWindowBones()
{
	const crSkeletonData* pSkelData = GetVehicleModelInfo()->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	if (pSkelData)
	{
		u32 index = 0;

		const char * boneNames[] = {	
			"window_lm",
			"window_lr",
			"window_rr",
			"window_rm",
		};

		for (u32 i = 0; i < NELEM(boneNames); i++)
		{
			const crBoneData* boneData = pSkelData->FindBoneData(boneNames[i]);
			if (boneData)
			{
				m_windowBoneIndices[index] = (s16)boneData->GetIndex();
				index++;
			}
		}
	}

	m_windowBoneCached = true;
}


void CRotaryWingAircraft::PreRender2(const bool bIsVisibleInMainViewport)
{

	// deal with searchlights before CAutomobile::PreRender2() call so that we can decide
	// if the light should be added or not. The light is finally added in CSearchLight:PostPreRender call
	if( (GetStatus() != STATUS_WRECKED) && (true == m_nVehicleFlags.bEngineOn) && GetSearchLight() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		bool bHasPriority = HasSearchLightPriority() || (GetDriver() && GetDriver()->IsPlayer());
		GetSearchLight()->SetHasPriority(bHasPriority);

		if (bHasPriority)
		{
			// Make sure no one gets priority after we have taken it.
			ms_fBestSearchLightScore = 1.0f;
			// Note that we had control of the searchlight last frame to help deal with rapid switching.
			m_bHadSearchLightLastFrame = true;
		}
	}
	CAutomobile::PreRender2(bIsVisibleInMainViewport);

	const bool bVehicleSuperVolito = (GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_SUPER_VOLITO.GetName().GetHash() 
								   || GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_SUPER_VOLITO2.GetName().GetHash());
	const bool bVehicleIsSwift2 = (		GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_SWIFT2.GetName().GetHash()
									|| bVehicleSuperVolito 
									|| GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_VOLATUS.GetName().GetHash());

	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	const Vector3& camPos = camInterface::GetPos();
	const float dist = camPos.Dist(vThisPosition);
	float fadeDist = rage::Clamp((dist - GetLightsCutoffDistanceTweak())/HeliLight_FadeDistance,0.0f,1.0f);

	if( (GetStatus() != STATUS_WRECKED) && (true == m_nVehicleFlags.bEngineOn) )
	{
#if RSG_PC
		int frameCount = (int)((float)fwTimer::GetTimeInMilliseconds_ScaledNonClipped()/(1000.0f/30.0f));
#else
		int frameCount = fwTimer::GetFrameCount_ScaledNonClipped();
#endif
		bool blinkLight = ((frameCount + GetRandomSeed()) & 31) <= 0;
		bool blinkLight2 = ((frameCount + GetRandomSeed() + 12) & 31) <= 0;
		bool blinkLight3 = ((frameCount + GetRandomSeed() + 15) & 31) <= 0;
		bool blinkLight4 = ((frameCount + GetRandomSeed() + 24) & 31) <= 0;

		const bool isAnnihilator = (GetModelIndex()==MI_HELI_POLICE_2);

		CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();

		const float fade = (0.5f - fadeDist) * 2.0f;
		const int vehTimeFlags = 0xF8003F;
		const float dayLightFade = Lights::CalculateTimeFade(vehTimeFlags);
		
#if ENABLE_FRAG_OPTIMIZATION
		DoInteriorLightEffect(VEH_INTERIORLIGHT, fade*dayLightFade, pModelInfo, GetInteriorLocation());
		DoInteriorLightEffect(VEH_ENGINE, fade*dayLightFade, pModelInfo, GetInteriorLocation(), true);
#else
		DoInteriorLightEffect(VEH_INTERIORLIGHT, fade*dayLightFade, pModelInfo, GetSkeleton(), GetInteriorLocation());
		DoInteriorLightEffect(VEH_ENGINE, fade*dayLightFade, pModelInfo, GetSkeleton(), GetInteriorLocation(), true);
#endif		

		extern ConfigVehiclePositionLightSettings	g_HeliPosLights;
		extern ConfigVehicleWhiteLightSettings		g_HeliWhiteHeadLights;
		extern ConfigVehicleWhiteLightSettings		g_HeliWhiteTailLights;
		
		bool addBlinkLights = true;
		if (bVehicleIsSwift2) { addBlinkLights = !IsInsideVehicleModeEnabled();  }

		if(bVehicleSuperVolito && (camInterface::IsRenderedCameraInsideVehicle() || (IsEnteringInsideOrExiting() && camInterface::IsRenderingFirstPersonCamera())))
		{
			blinkLight = blinkLight2 = false;
		}

		if (addBlinkLights)
		{
			if( blinkLight )	DoPosLightEffects	(VEH_SIREN_1, g_HeliPosLights, true,isAnnihilator,fadeDist);
			if( blinkLight2 )	DoPosLightEffects	(VEH_SIREN_2, g_HeliPosLights,false,isAnnihilator,fadeDist);

			if( blinkLight3 && ( (m_nTailBoomGroup == -1) || (m_nTailBoomGroup != -1 && false == GetVehicleFragInst()->GetGroupBroken(m_nTailBoomGroup) ) ) )
			{
				DoWhiteLightEffects	(VEH_SIREN_3, g_HeliWhiteTailLights, false,fadeDist);
			}
			if (blinkLight4 )	DoWhiteLightEffects	(VEH_SIREN_4, g_HeliWhiteHeadLights, true,fadeDist);
		}
	}

	// Special swift2 lights
	if ((GetStatus() != STATUS_WRECKED) && bVehicleIsSwift2)
	{
		extern ConfigLightSettings g_HeliSwift2Cabin;
		extern ConfigLightSettings g_PlaneLuxe2CabinTV;
		extern ConfigLightSettings g_PlaneLuxe2CabinWindow;

		CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
		bool enteringInsideExitingAVehicle = IsEnteringInsideOrExiting();
		const bool enableHD = (enteringInsideExitingAVehicle && camInterface::IsRenderingFirstPersonCamera());
		const float distFade =  1.0f - fadeDist;


		if ((camInterface::IsRenderedCameraInsideVehicle() || enableHD) BANK_ONLY( || camInterface::GetDebugDirector().IsFreeCamActive()))
		{
			for (u32 l = VEH_SIREN_6; l <= VEH_SIREN_9; l++ )
			{
				AddLight(LIGHT_TYPE_SPOT, pModelInfo->GetBoneIndex(l), g_HeliSwift2Cabin, distFade);
			}

			AddLight(LIGHT_TYPE_SPOT, pModelInfo->GetBoneIndex(VEH_SIREN_12), g_PlaneLuxe2CabinTV, distFade);

			if (!m_windowBoneCached)
			{
				CacheWindowBones();
			}

			for (u32 l = 0; l < NELEM(m_windowBoneIndices); l++ )
			{
				AddLight(LIGHT_TYPE_SPOT, m_windowBoneIndices[l], g_PlaneLuxe2CabinWindow, distFade);
			}

		}
		else
		{
			for (u32 l = VEH_SIREN_6; l <= VEH_SIREN_9; l++ )
			{
				AddLight(LIGHT_TYPE_SPOT, pModelInfo->GetBoneIndex(l), g_HeliSwift2Cabin, distFade);
			}
		}
	}
}

// Return true if we have the best (valid) helicopter searchlight priority score.
bool CRotaryWingAircraft::HasSearchLightPriority()
{
	//Returning last state if game is paused as no state changes when game is paused
	if(fwTimer::IsUserPaused() || fwTimer::IsGamePaused())
	{
		return m_bHadSearchLightLastFrame; 
	}
	else
	{
		ms_bNeedToResetSearchLightScoring = true;
		if (m_fSearchLightScore > 0.0f)
		{
			// Our score was positive, so invalid.
			return false;
		}
		else
		{
			// If we have the best search light score, then we won.
			return abs(ms_fBestSearchLightScore - m_fSearchLightScore) < VERY_SMALL_FLOAT;
		}

	}
}


// Use a set of heuristics to determine the helicopter searchlight priority score.
void CRotaryWingAircraft::ProcessSearchLightScoring()
{
	// Reset our search light score.  Positive scores are invalid.
	m_fSearchLightScore = 1.0f;
	//If we are the first to ProcessSearchLightScoring, then reset the global score too.
	if (ms_bNeedToResetSearchLightScoring)
	{
		ResetSearchLightScores();
		ms_bNeedToResetSearchLightScoring = false;
	}
	CSearchLight* pSearchLight = GetSearchLight();
	if (pSearchLight && GetSearchLightTarget())
	{
		// Higher negative scores are better.
		float fBaseScore = pSearchLight->GetLightOn() && pSearchLight->GetLightBrightness() > 0.0f ? -1.0f : 1.0f;
		// Give a slight preference to heli's who had search light priority last time.
		if (m_bHadSearchLightLastFrame)
		{
			fBaseScore *= ms_fPreviousSearchLightOwnerBonus;
		}
		// Give a slight preference to heli's whose search light is not blocked by anything
		if(!pSearchLight->GetSearchLightBlocked())
		{
			fBaseScore *= ms_fUnblockedSearchLightBonus;
		}
		// Scale the base score with distance.
		float fDist = Dist(GetSearchLightTarget()->GetTransform().GetPosition(), GetTransform().GetPosition()).Getf();
		m_fSearchLightScore = fBaseScore * fDist;
		if (m_fSearchLightScore < 0.0f && (m_fSearchLightScore >= ms_fBestSearchLightScore || ms_fBestSearchLightScore > 0.0f))
		{
			// We have the best score so far.
			ms_fBestSearchLightScore = m_fSearchLightScore;
		}
	}
	m_bHadSearchLightLastFrame = false;
}

void CRotaryWingAircraft::ApplyDeformationToBones(const void* basePtr)
{
	CAutomobile::ApplyDeformationToBones(basePtr);

	for(int i =0; i< m_iNumPropellers; i++)
	{
		m_propellers[i].ApplyDeformation(this,basePtr);
	}
}


///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Fix
// PURPOSE :  Revert all damage back to OK
///////////////////////////////////////////////////////////////////////////////////
void CRotaryWingAircraft::Fix(bool resetFrag, bool allowNetwork)
{
	CAutomobile::Fix(resetFrag, allowNetwork);

	m_fMainRotorHealth = VEH_DAMAGE_HEALTH_STD;
	m_fRearRotorHealth = VEH_DAMAGE_HEALTH_STD;
	m_fTailBoomHealth = VEH_DAMAGE_HEALTH_STD;

	m_fMainRotorBrokenOffSmokeLifeTime = 0.0f;
	m_fRearRotorBrokenOffSmokeLifeTime = 0.0f;

	fragInst* pFragInst = GetVehicleFragInst();
	if(m_nTailBoomGroup > -1 && pFragInst)
	{
		fragTypeGroup* pTailGroup = pFragInst->GetTypePhysics()->GetAllGroups()[m_nTailBoomGroup];
		int nFirstChild = pTailGroup->GetChildFragmentIndex();
		int nNumChildren = pTailGroup->GetNumChildren();
		// make sure this component doesn't break off until we want it to
		for(int nChild=nFirstChild; nChild < nFirstChild + nNumChildren; nChild++)
		{
			((fragInstGta*)pFragInst)->SetDontBreakFlag(BIT(nChild));
		}
	}

	for(int i =0; i< m_iNumPropellers; i++)
	{
		m_propellers[i].Fix();
	}
}

///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a heli
///////////////////////////////////////////////////////////////////////////////////

void CRotaryWingAircraft::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion )
{
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	/// don't damage if this flag is set (usually during a cutscene)
	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;
	}

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	bool blowUpInstantly = true;
	bool blowUpInstantlyOverride = false;

	// Check if the caller of this function wants the vehicle to instantly blow up, no matter what (for example, network vehicle blow up script)
	if (m_nPhysicalFlags.bExplodeInstantlyWhenChecked == true)
	{
		blowUpInstantlyOverride = true;
	}
	// set the plane/heli to go out of control if not already
	if (GetStatus()==STATUS_ABANDONED && !blowUpInstantlyOverride)
	{	
		blowUpInstantly = false;
	}
	// if heli is in the air, crash and burn, otherwise just blow up now
	else if(HasContactWheels()==false && fwRandom::GetRandomNumberInRange(0.0f, 100.0f)<50.0f && !blowUpInstantlyOverride)
	{	
		blowUpInstantly = false;
	}

	if(blowUpInstantly && GetStatus() == STATUS_PLAYER && GetDriver() && GetDriver()->IsLocalPlayer())
	{
		audNorthAudioEngine::NotifyLocalPlayerPlaneCrashed();
	}
        
    if ( m_fRearRotorHealth <= 0.0f && m_fTailBoomHealth <= 0.0f )// If the rear rotors are broken off then blow up instantly
    {
		blowUpInstantly = true;
    }

	if(weaponHash == WEAPONTYPE_EXPLOSION)
	{
		blowUpInstantly = true;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	if(pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
	{
		blowUpInstantly = true;
	}

	if(!bAddExplosion && IsInAir() && !blowUpInstantly)
	{
		audHeliAudioEntity * audio ( (audHeliAudioEntity*)GetVehicleAudioEntity());
		if(audio)
		{
			audio->TriggerGoingDownSound();
		}
	}
	else if(blowUpInstantly)
	{
		// Heli is completely blown up, stop the going down sound
		audHeliAudioEntity * audio ( (audHeliAudioEntity*)GetVehicleAudioEntity());
		if(audio)
		{
			audio->StopGoingDownSound();
		}
	}
	
	// set the engine temp super high so we get nice sounds as the chassis cools down
	m_EngineTemperature = MAX_ENGINE_TEMPERATURE;
	// Everything now goes through a crash task
	// Create a crash task if one doesn't already exist
	if (!IsNetworkClone())
	{
#if !__NO_OUTPUT
		if (NetworkInterface::IsGameInProgress() && GetNetworkObject())
		{
			netDebug1("**************************************************************************************************");
			netDebug1("HELI BLOWN UP: %s", GetNetworkObject()->GetLogName());
			sysStack::PrintStackTrace();
			netDebug1("**************************************************************************************************");
		}
#endif //  !__NO_OUTPUT

		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);

		aiTask *pActiveTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH);
		if(!pActiveTask)
		{
			CTaskVehicleCrash *pCarTask = rage_new CTaskVehicleCrash( pCulprit, 0, weaponHash );

			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_BlowUpInstantly, blowUpInstantly);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_InACutscene, bInACutscene);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_AddExplosion, bAddExplosion);

			GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_CRASH);
			return;
		}
		else if(blowUpInstantly)
		{
			// Blow up heli if we've been told to blow up again instantly
			CTaskVehicleCrash *pCrashTask = static_cast<CTaskVehicleCrash *>(pActiveTask);
			pCrashTask->SetCrashFlag(CTaskVehicleCrash::CF_HitByConsecutiveExplosion, true);
		}
	}
	//Because this task is not run in Clones we need to Finish Blowing Up the Vehicle.
	else
	{
		FinishBlowingUpVehicle(pCulprit, bInACutscene, bAddExplosion, bNetCall, weaponHash, bDelayedExplosion);
	}
}

//////////////////////////////////////////////////////////////////////////
// Damage the vehicle even more, close to these bones during BlowUpCar
//////////////////////////////////////////////////////////////////////////
const int HELI_BONE_COUNT_TO_DEFORM = 30;
const eHierarchyId ExtraHeliBones[HELI_BONE_COUNT_TO_DEFORM] = 
{
	VEH_CHASSIS, VEH_CHASSIS, HELI_TAIL, HELI_TAIL, //default CPU code path has 4 max impacts, do the most important ones first
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS,
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, HELI_TAIL, HELI_TAIL, HELI_TAIL, HELI_TAIL, HELI_TAIL, HELI_TAIL, 
	HELI_ROTOR_MAIN, HELI_ROTOR_MAIN, HELI_ROTOR_MAIN, HELI_ROTOR_REAR, HELI_RUDDER, VEH_CHASSIS, HELI_ELEVATORS
};

const eHierarchyId* CRotaryWingAircraft::GetExtraBonesToDeform(int& extraBoneCount)
{
	extraBoneCount = HELI_BONE_COUNT_TO_DEFORM;
	return ExtraHeliBones;
}

//////////////////////////////////////////////////////////////////////////
// Return true if the props are hit
//////////////////////////////////////////////////////////////////////////
bool CRotaryWingAircraft::ApplyDamageToPropellers( CEntity* inflictor, float fApplyDamage, int nComponent )
{
    // See if the main or rear rotors are damaged.
    if(GetVehicleFragInst() && m_nVehicleFlags.bCanBeVisiblyDamaged)
    {
		bool bHitPropeller = false;;
        if(m_propellerCollisions[Rotor_Rear].GetFragChild() == nComponent)
        {
			bHitPropeller = true;
		}
		else if(m_propellerCollisions[Rotor_Rear].GetFragGroup() > -1)
		{
			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[m_propellerCollisions[Rotor_Rear].GetFragGroup()];
			if(pGroup->GetNumChildren() > 1)
			{
				for(int iChild = 1; iChild < pGroup->GetNumChildren(); iChild++)
				{
					if(m_propellerCollisions[Rotor_Rear].GetFragChild() + iChild == nComponent)
					{
						bHitPropeller = true;
						break;
					}
				}
			}
		}

		if(bHitPropeller)
		{
            static dev_float sfRearRotorDamageMult = 7.5f;
            m_fRearRotorHealth -= (fApplyDamage * sfRearRotorDamageMult * m_fRearRotorHealthDamageScale);
            if(m_fRearRotorHealth <= 0.0f)
            {
                m_fRearRotorHealth = 0.0f;
                BreakOffRearRotor();
				StartCrashingBehavior(inflictor);

				if (inflictor && inflictor->GetIsTypePed())
				{
					m_bHeliRotorDestroyedByPed = true;
				}
            }
            return TRUE;
        }
        
		if(m_propellerCollisions[Rotor_Main].GetFragChild() == nComponent)
        { 
			bHitPropeller = true;
		}
		else if(m_propellerCollisions[Rotor_Main].GetFragGroup() > -1)
		{
			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[m_propellerCollisions[Rotor_Main].GetFragGroup()];
			if(pGroup->GetNumChildren() > 1)
			{
				for(int iChild = 1; iChild < pGroup->GetNumChildren(); iChild++)
				{
					if(m_propellerCollisions[Rotor_Main].GetFragChild() + iChild == nComponent)
					{
						bHitPropeller = true;
						break;
					}
				}
			}
		}

		if(bHitPropeller)
		{
            m_fMainRotorHealth -= fApplyDamage * m_fMainRotorHealthDamageScale;
            if(m_fMainRotorHealth <= 0.0f)
            {
                m_fMainRotorHealth = 0.0f;
                BreakOffMainRotor(); 

				if (inflictor && inflictor->GetIsTypePed())
				{
					m_bHeliRotorDestroyedByPed = true;
				}
			}
            return TRUE;
        }
    }

    return FALSE;
}

bool CRotaryWingAircraft::GetHeliRotorDestroyedByPed( ) 
{ 
	if (m_bHeliRotorDestroyedByPed)
		return true;

	static const u32 LAST_DAMAGE_TRESHOLD = 3*1000;
	const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - GetWeaponDamagedTime();
	CEntity* lastdamager = GetWeaponDamageEntity();
	if (lastdamager && timeSinceLastDamage <= LAST_DAMAGE_TRESHOLD)
	{
		if (lastdamager->GetIsTypePed())
			return true;

		if (lastdamager->GetIsTypeVehicle() && static_cast<CVehicle*>(lastdamager)->GetDriver())
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
//This is called by the crash task
//////////////////////////////////////////////////////////////////////////
void CRotaryWingAircraft::FinishBlowingUpVehicle(CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion)
{
	//This should only be called from the crash task
	Assert(IsNetworkClone() || GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH));

	CVehicle::BlowUpCar(pCulprit);

#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	/// don't damage if this flag is set (usually during a cutscene)
	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;
	}

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	audVehicleCollisionAudio * collisionAudio = GetAudioEntity() ? &(((audVehicleAudioEntity*)GetAudioEntity())->GetCollisionAudio()) : NULL;

	if(collisionAudio)
	{
		collisionAudio->WreckVehicle();
	}

	// we can't blow up helis controlled by another machine
	// but we still have to change their status to wrecked
	// so the car doesn't blow up if we take control of an
	// already blown up car
	if (IsNetworkClone())
	{
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);

		// break the tail and main rotor off heli's (do this before BlowUpCarParts because we want the main rotor to disapear, not break off)
		if (m_fRearRotorHealth > 0.0f)
		{
			BreakOffRearRotor();
		}

		BreakOffTailBoom();

		if(m_fMainRotorHealth > 0.0f)
		{
			BreakOffMainRotor();
		}

		m_nPhysicalFlags.bRenderScorched = TRUE;
		SetTimeOfDestruction();

		SetIsWrecked();

		// knock bits off the car
		GetVehicleDamage()->BlowUpCarParts(pCulprit);
		// Break lights, windows and sirens
		GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

		// Switch off the engine. (For sound purposes)
		SwitchEngineOff(false);
		m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
		m_nVehicleFlags.bLightsOn = FALSE;
		TurnSirenOn(FALSE);
		m_nAutomobileFlags.bTaxiLight = FALSE;

		g_decalMan.Remove(this);

		//Check to see that it is the player
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
			CCrime::ReportDestroyVehicle(this, static_cast<CPed*>(pCulprit));
		}

		return;
	}

	if (NetworkUtils::IsNetworkCloneOrMigrating(this))
	{
		// the vehicle is migrating. Create a weapon damage event to blow up the vehicle, which will be sent to the new owner. If the migration fails
		// then the vehicle will be blown up a little later.
		CBlowUpVehicleEvent::Trigger(*this, pCulprit, bAddExplosion, weaponHash, bDelayedExplosion);
		return;
	}

	//Total damage done for the damage trackers
	float totalDamage = GetHealth() + m_VehicleDamage.GetEngineHealth() + m_VehicleDamage.GetPetrolTankHealth();
	for(s32 i=0; i<GetNumWheels(); i++)
	{
		totalDamage += m_VehicleDamage.GetTyreHealth(i);
		totalDamage += m_VehicleDamage.GetSuspensionHealth(i);
	}
	totalDamage = totalDamage > 0.0f ? totalDamage : 1000.0f;

	SetIsWrecked();

	// increment player stats
	if ( ( pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer() ) || pCulprit == FindPlayerVehicle() )
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->HavocCaused += HAVOC_BLOWUPCAR;
	}

	//Set the destruction information.
	SetDestructionInfo(pCulprit, weaponHash);
	
	m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off

	SetHealth(0.0f);			// Make sure this happens before AddExplosion or it will blow up twice

//	Vector3	Temp = GetPosition();

	KillPedsInVehicle(pCulprit, weaponHash);
	KillPedsGettingInVehicle(pCulprit);

	// break the tail and main rotor off heli's (do this before BlowUpCarParts because we want the main rotor to disapear, not break off)
	if(m_fRearRotorHealth > 0.0f)
	{
		BreakOffRearRotor();
	}

	BreakOffTailBoom(GPU_VEHICLE_DAMAGE_ONLY(Break_Off_Tail_Boom_Pending_Bound_Update));

	if(m_fMainRotorHealth > 0.0f)
	{
		BreakOffMainRotor();
	}


	// knock bits off the car
	GetVehicleDamage()->BlowUpCarParts(pCulprit, CVehicleDamage::Break_Off_Car_Parts_Pending_Bound_Update);
	// Break lights, windows and sirens
	GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;
	this->TurnSirenOn(FALSE);
	this->m_nAutomobileFlags.bTaxiLight = FALSE;

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);

		CPed* pInflictorPed = pCulprit->GetIsTypeVehicle() ? static_cast<CVehicle*>(pCulprit)->GetDriver() : static_cast<CPed*>(pCulprit);
		CCrime::ReportDestroyVehicle(this, pInflictorPed);
	}

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);
}



void CRotaryWingAircraft::Teleport(const Vector3& vecSetCoors, float fSetHeading/* =-10.0f */, bool bCalledByPedTask/* =false */, bool bTriggerPortalRescan, bool bCalledByPedTask2/* =false */, bool bWarp/* =true */, bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants))
{
    // Not sure why we need to reset the rotor blades but this might need to be re-added.
	// m_fMainRotorSpeed = 0.0f;
	CAutomobile::Teleport(vecSetCoors,fSetHeading,bCalledByPedTask,bTriggerPortalRescan,bCalledByPedTask2, bWarp);
}

void CRotaryWingAircraft::SetDampingForFlight(float fLinearDampingMult)
{
    CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
    if(pFlyingHandling == NULL)
		return;

	phArchetypeDamp* pArchetypeDamp = static_cast<phArchetypeDamp*>(GetVehicleFragInst()->GetArchetype());

    ScalarV dampingMultiplier = ScalarV(V_ONE);
    if(const CPed* pDriver = GetDriver())
    {
		if(pDriver->IsPlayer())
		{
			StatId stat = STAT_FLYING_ABILITY.GetStatId();
			float fFlyingStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
			const CPlayerInfo::sPlayerStatInfo& driverStatInfo = CPlayerInfo::GetPlayerStatInfoForPed(*pDriver);
			float fMinHeliDamping = driverStatInfo.m_MinHeliDamping;
			float fMaxHeliDamping = driverStatInfo.m_MaxHeliDamping;

			// Swapped the max damping and min damping, as better skills should drive faster, which means less damping
			dampingMultiplier = ScalarVFromF32(((1.0f - fFlyingStatValue) * fMaxHeliDamping + fFlyingStatValue * fMinHeliDamping)/100.0f);

            float airDragMult = pDriver->GetPlayerInfo()->m_fForceAirDragMult;

            if( airDragMult > 0.0f)
            {
                dampingMultiplier *= ScalarV( airDragMult );
            }
		}
	}

	ScalarV linearDampingMultiplier = Scale(dampingMultiplier,ScalarVFromF32(fLinearDampingMult));
	if(const phCollider* pCollider = GetCollider())
	{
		if(pCollider->IsArticulated())
		{
			linearDampingMultiplier = Scale(linearDampingMultiplier,ScalarV(V_HALF));
		}
	}

	const Vec3V handlingLinearVelocityDamping = Vec3VFromF32(pFlyingHandling->m_fMoveRes);
	Vec3V linearVelocityDamping = Scale(handlingLinearVelocityDamping,linearDampingMultiplier);


	if( GetDriver() &&
		GetDriver()->IsAPlayerPed() )
	{
		float fSlipStreamEffect = GetSlipStreamEffect();
		linearVelocityDamping -= (linearVelocityDamping * ScalarV( fSlipStreamEffect ) );
	}

    pArchetypeDamp->ActivateDamping(phArchetypeDamp::LINEAR_V, RCC_VECTOR3(linearVelocityDamping));

	// Only modify the angular tuning if it's non-zero
	const Vec3V handlingLinearAngularVelocityDamping = pFlyingHandling->m_vecTurnRes;
	const Vec3V linearAngularVelocityDamping = SelectFT(IsZero(handlingLinearAngularVelocityDamping), Scale(handlingLinearAngularVelocityDamping,linearDampingMultiplier), RCC_VEC3V(pArchetypeDamp->GetDampingConstant(phArchetypeDamp::ANGULAR_V)));
	pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, RCC_VECTOR3(linearAngularVelocityDamping));

	const Vec3V handlingQuadraticAngularVelocityDamping = pFlyingHandling->m_vecSpeedRes;
	const Vec3V quadraticAngularVelocityDamping = SelectFT(IsZero(handlingQuadraticAngularVelocityDamping), Scale(handlingQuadraticAngularVelocityDamping,dampingMultiplier), RCC_VEC3V(pArchetypeDamp->GetDampingConstant(phArchetypeDamp::ANGULAR_V2)));
	pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V2, RCC_VECTOR3(quadraticAngularVelocityDamping));
}

dev_float dfOutOfControlRearRotorHealthRatio = 0.5f;

void CRotaryWingAircraft::ProcessFlightHandling(float fTimeStep)
{
	if (IsFullThrottleActive())
	{
		SetThrottleControl(5);
		SetMainRotorSpeed(MAX_ROT_SPEED_HELI_BLADES);
		if (!GetDriver() || !GetDriver()->IsPlayer())
		{
			SetYawControl(0);
			SetPitchControl(0);
			SetRollControl(0);
		}
	}
	m_bIsInAir = IsInAir(false);

    SetDampingForFlight();

	if(!m_bIsInAir && HasContactWheels())
	{
		m_fLastWheelContactTime = fwTimer::GetTimeInMilliseconds() * 0.0001f;
	}

	if(m_fMainRotorSpeed > 0.0f && m_fMainRotorHealth > 0.0f)
	{
        float fOldYawControl      = m_fYawControl;
	    float fOldPitchControl    = m_fPitchControl;
	    float fOldRollControl     = m_fRollControl;
	    float fOldThrottleControl = m_fThrottleControl;

		// Make sure the possbilyTouchesWater flag is up to date before acting on it...
		if(	m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate &&
			m_nFlags.bPossiblyTouchesWater &&
			(GetTransform().GetPosition().GetZf() < 10.0f || GetIsInWater()))
		{
			// test centre of heli, and pull down into water
			float fWaterLevel;
			const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			Vector3 vecTestPoint = vThisPosition;

			Assert(pHandling);
			Assert(pHandling->GetFlyingHandlingData());
			CFlyingHandlingData* pFlightHandling = pHandling->GetFlyingHandlingData();

			if(m_Buoyancy.GetWaterLevelIncludingRivers(vecTestPoint, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL)
				&& m_Buoyancy.GetSubmergedLevel()>pFlightHandling->m_fSubmergeLevelToPullHeliUnderwater)
			{
				// pull heli down into the water
				float fPullDown = fWaterLevel - (vThisPosition.z + GetBoundingBoxMin().z);
				fPullDown = rage::Min(fPullDown / rage::Abs(GetBoundingBoxMin().z), 1.5f);

				ApplyInternalForceCg(fPullDown * sfHeliDrownPull * GetMass() * ZAXIS);
			}

			// test if the main rotor is hitting the water
			if(m_propellerCollisions[Rotor_Main].GetFragChild() > -1)
			{
				phBound* pBoundRotor = ((phBoundComposite*)GetVehicleFragInst()->GetArchetype()->GetBound())->GetBound(m_propellerCollisions[Rotor_Main].GetFragChild());
				 if(pBoundRotor)
				{
					Matrix34 matRotor = RCC_MATRIX34(((phBoundComposite*)GetVehicleFragInst()->GetArchetype()->GetBound())->GetCurrentMatrix(m_propellerCollisions[Rotor_Main].GetFragChild()));
					matRotor.RotateLocalZ(fwRandom::GetRandomNumberInRange(0.0f, HALF_PI));
					Matrix34 m = MAT34V_TO_MATRIX34(GetMatrix());
					matRotor.Dot(m);

					Vector3 vecSamplePoints[4];
					Vector3 vecSplashDirn[4];
					vecSamplePoints[0].Set(0.0f, pBoundRotor->GetBoundingBoxMax().GetYf(), 0.0f);
					vecSamplePoints[1].Set(-pBoundRotor->GetBoundingBoxMax().GetXf(), 0.0f, 0.0f);
					vecSamplePoints[2].Set(0.0f, -pBoundRotor->GetBoundingBoxMax().GetYf(), 0.0f);
					vecSamplePoints[3].Set(pBoundRotor->GetBoundingBoxMax().GetXf(), 0.0f, 0.0f);
					Vector3 vecRight(VEC3V_TO_VECTOR3(GetTransform().GetA()));
					Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
					vecSplashDirn[0].Set(-vecRight + ZAXIS);
					vecSplashDirn[1].Set(-vecForward + ZAXIS);
					vecSplashDirn[2].Set(vecRight + ZAXIS);
					vecSplashDirn[3].Set(vecForward + ZAXIS);

					for(int nSample=0; nSample < 4; nSample++)
					{
						matRotor.Transform(vecSamplePoints[nSample], vecTestPoint);
						if(m_Buoyancy.GetWaterLevelIncludingRivers(vecTestPoint, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL) && fWaterLevel > vecTestPoint.z)
						{
							vecTestPoint.z = fWaterLevel;

							if ((fwTimer::GetSystemFrameCount()&3)==0)
							{
								g_vfxWater.TriggerPtFxSplashHeliBlade(this, RCC_VEC3V(vecTestPoint));
							}

							GetVehicleAudioEntity()->TriggerHeliRotorSplash();

							// rotor take damage
							if(m_fMainRotorHealth > 0.0f && m_nVehicleFlags.bCanBeVisiblyDamaged)
							{
								if (!IsNetworkClone())
								{
									float fDamage = sfInWaterRotorDamage * fwTimer::GetTimeStep();
									m_fMainRotorHealth -= fDamage * m_fMainRotorHealthDamageScale;

									if(m_fMainRotorHealth <= 0.0f)
									{
										BreakOffMainRotor();
									}
								}

								if(m_fMainRotorHealth > 0.0f)
									m_fMainRotorSpeed = rage::Min(MIN_ROT_SPEED_HELI_CONTROL + 0.01f, m_fMainRotorSpeed);
							}
						}
					}
				}
			}

			if(m_propellerCollisions[Rotor_Rear].GetFragChild() > -1 && m_fRearRotorHealth > 0.0f)
			{
				Matrix34 matRearRotor = RCC_MATRIX34(((phBoundComposite*)GetVehicleFragInst()->GetArchetype()->GetBound())->GetCurrentMatrix(m_propellerCollisions[Rotor_Rear].GetFragChild()));
				if(matRearRotor.a.IsNonZero())
				{
					vecTestPoint = matRearRotor.d;
					vecTestPoint = TransformIntoWorldSpace(vecTestPoint);
					if(m_Buoyancy.GetWaterLevelIncludingRivers(vecTestPoint, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL) && fWaterLevel > vecTestPoint.z)
					{
						// rotor take damage
						float fDamage = sfInWaterRotorDamage * fwTimer::GetTimeStep();
						m_fRearRotorHealth -= fDamage * m_fRearRotorHealthDamageScale;
						if(m_fRearRotorHealth <= 0.0f)
						{
							// rotor destruction effect
							BreakOffRearRotor();
							StartCrashingBehavior();
						}
					}
				}
			}
		}

		static dev_float sfLeaveGroundMaxTime = 5.0f;
		float fCurrentTime = fwTimer::GetTimeInMilliseconds() * 0.0001f;
		float fLeaveGroundTimeRatio = (fCurrentTime - m_fLastWheelContactTime) / sfLeaveGroundMaxTime;
		fLeaveGroundTimeRatio = Clamp(fLeaveGroundTimeRatio, 0.0f, 1.0f);
		
		bool bProcessDamageControl = true;
		// If we've lost the backend start spinning round
		if((m_nTailBoomGroup > -1 && GetVehicleFragInst()->GetGroupBroken(m_nTailBoomGroup)) || GetStatus() == STATUS_OUT_OF_CONTROL)
		{
			bProcessDamageControl = false;
            if(fwTimer::GetTimeInMilliseconds() > m_uHeliOutOfControlRecoverStart)
            {
			
                if(m_bHeliOutOfControlRecovering)
                {
                    m_bHeliOutOfControlRecovering = false;

					if(m_iHeliOutOfControlRecoverPeriods < siHeliOutOfControlRecoveryPeriodsMax-1)
					{
						m_fHeliOutOfControlRoll = fwRandom::GetRandomNumberInRange(sfHeliTailLostRoll, sfHeliOutOfControlMaxRoll) * sfHeliOutOfControlRollAfterRecoveryMult;
						m_fHeliOutOfControlThrottle = fwRandom::GetRandomNumberInRange(sfHeliOutOfControlThrottleDropAfterRecoveryMin, sfHeliOutOfControlThrottleDropAfterRecoveryMax);
						m_uHeliOutOfControlRecoverStart = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(siHeliOutOfControlFailPeriodsMin, siHeliOutOfControlFailPeriodsMax);
					}
				}
                else
                {
                    if(m_iHeliOutOfControlRecoverPeriods > 0)
                    {
                        m_uHeliOutOfControlRecoverStart = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(siHeliOutOfControlRecoverPeriodsMin, siHeliOutOfControlRecoverPeriodsMax);
                        
                        m_bHeliOutOfControlRecovering = true;
                        
                        m_iHeliOutOfControlRecoverPeriods--;
                        if(m_iHeliOutOfControlRecoverPeriods < 0)
                            m_iHeliOutOfControlRecoverPeriods = 0;
                    }
                }
            }
 
            if(m_iHeliOutOfControlRecoverPeriods < siHeliOutOfControlRecoveryPeriodsMax && !m_bHeliOutOfControlRecovering)// first recovering period do nothing.
            {    
                m_fHeliOutOfControlYaw += Sign(m_fHeliOutOfControlYaw) * fwRandom::GetRandomNumberInRange(sfHeliOutOfControlYawControlAdjustmentNeg, sfHeliOutOfControlYawControlAdjustmentPos) * fTimeStep;
                m_fHeliOutOfControlYaw = Clamp(m_fHeliOutOfControlYaw, -sfHeliOutOfControlMaxYawControl, sfHeliOutOfControlMaxYawControl);

				if(m_bIsInAir)
				{
	                m_fYawControl = m_fHeliOutOfControlYaw * fLeaveGroundTimeRatio;

					m_fPitchControl = m_fHeliOutOfControlPitch * fLeaveGroundTimeRatio;

					m_fRollControl = m_fHeliOutOfControlRoll * fLeaveGroundTimeRatio;
					bProcessDamageControl = true;
				}

				// Make it spin by a little amount
                m_fRearRotorHealth = Min(m_fRearRotorHealth, VEH_DAMAGE_HEALTH_STD * dfOutOfControlRearRotorHealthRatio);

                m_fHeliOutOfControlThrottle += fwRandom::GetRandomNumberInRange(sfHeliOutOfControlThrottleControlAdjustmentNeg, sfHeliOutOfControlThrottleControlAdjustmentPos) * fTimeStep;
                m_fHeliOutOfControlThrottle = Clamp(m_fHeliOutOfControlThrottle, sfHeliOutOfControlMinThrottleControl, 2.0f);

                SetThrottleControl(m_fHeliOutOfControlThrottle);
            }
           
		}
		
		if(m_bIsInAir && bProcessDamageControl) // helicopter is under control, but still might buffeting due to the damage
		{
			// Early out in the common case where the engine has taken no damage
			if(m_fRearRotorHealth < VEH_DAMAGE_HEALTH_STD)
			{
				float fHeliRearRotorYaw = m_fRearRotorHealth <= 0.0f ? sfHeliRearRotorLostYaw : sfHeliRearRotorDamageYaw;
				float fHeliRearRotorRoll = m_fRearRotorHealth <= 0.0f ? sfHeliRearRotorLostRoll : sfHeliRearRotorDamageRoll;

				float fHeliRearRotorDamageRate = 1.0f - m_fRearRotorHealth / VEH_DAMAGE_HEALTH_STD;
				fHeliRearRotorDamageRate = Clamp(fHeliRearRotorDamageRate, 0.0f, 1.0f);

				if(m_fRollControl < 2.0f)
					m_fRollControl += m_fMainRotorSpeed*fHeliRearRotorRoll*fHeliRearRotorDamageRate * fLeaveGroundTimeRatio;

				m_fYawControl += m_fMainRotorSpeed*fHeliRearRotorYaw*fHeliRearRotorDamageRate * fLeaveGroundTimeRatio;

				if(m_fRearRotorHealth <= 0.0f)
				{
					if(GetStatus() != STATUS_OUT_OF_CONTROL)
					{
						SetThrottleControl(sfHeliLostRearRotorThrottle);
					}

					audHeliAudioEntity * audio ( (audHeliAudioEntity*)GetVehicleAudioEntity());
					if(audio)
					{
						audio->TriggerGoingDownSound();
					}
				}
			}

			if(m_fMainRotorHealth < VEH_DAMAGE_HEALTH_STD)
			{
				float fHeliMainRotorYaw = m_fMainRotorHealth <= 0.0f ? sfHeliMainRotorLostYaw : sfHeliMainRotorDamageYaw;
				float fHeliMainRotorRoll = m_fMainRotorHealth <= 0.0f ? sfHeliMainRotorLostRoll : sfHeliMainRotorDamageRoll;

				float fHeliMainRotorDamageRate = 1.0f - m_fMainRotorHealth / VEH_DAMAGE_HEALTH_STD;
				fHeliMainRotorDamageRate = Clamp(fHeliMainRotorDamageRate, 0.0f, 1.0f);

				if(m_fRollControl < 2.0f)
					m_fRollControl += m_fMainRotorSpeed*fHeliMainRotorRoll*fHeliMainRotorDamageRate * fLeaveGroundTimeRatio;

				m_fYawControl += m_fMainRotorSpeed*fHeliMainRotorYaw*fHeliMainRotorDamageRate * fLeaveGroundTimeRatio;
			}
		}

        // reset control variables for clones - they should use values synced over the network
        if(IsNetworkClone())
        {
            m_fYawControl      = fOldYawControl;
	        m_fPitchControl    = fOldPitchControl;
	        m_fRollControl     = fOldRollControl;
	        m_fThrottleControl = fOldThrottleControl;
        }

		ProcessFlightModel(fTimeStep);
	}

	m_bDisableTurbulanceThisFrame = false;

}

void CRotaryWingAircraft::ModifyControlsBasedOnFlyingStats( CPed* pPilot, CFlyingHandlingData* pFlyingHandling, float &fYawControl, float &fPitchControl, float &fRollControl, float &fThrottleControl, float fTimeStep )
{
	Assert( pFlyingHandling );

	StatId stat = STAT_FLYING_ABILITY.GetStatId();
	float fFlyingStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
	float fMinPlaneControlAbility = CPlayerInfo::GetPlayerStatInfoForPed(*pPilot).m_MinPlaneControlAbility;
	float fMaxPlaneControlAbility = CPlayerInfo::GetPlayerStatInfoForPed(*pPilot).m_MaxPlaneControlAbility;

	float fHeliControlAbilityMult = ((1.0f - fFlyingStatValue) * fMinPlaneControlAbility + fFlyingStatValue * fMaxPlaneControlAbility)/100.0f;
	fHeliControlAbilityMult = Clamp(fHeliControlAbilityMult/ms_fMaxAbilityToAdjustDifficulty, 0.0f, 1.0f);

	// Scale by the vehicle modifier
	float fHeliDifficulty = pFlyingHandling->m_fInputSensitivityForDifficulty * fTimeStep;

	if( fHeliControlAbilityMult < 1.0f ||  m_fControlLaggingRateMulti < 1.0f )
	{
		CPlayerInfo* pPlayerInfo = pPilot->GetPlayerInfo();
		physicsFatalAssertf( pPlayerInfo, "Expected a player info!" ); // Assumed to be a player

		// Limited controls for inexperienced pilots
		float fAbilityMult = Clamp(ms_fHeliControlAbilityControlDampMin + ((ms_fHeliControlAbilityControlDampMax - ms_fHeliControlAbilityControlDampMin) * fHeliControlAbilityMult), 0.0f, 1.0f);
		float fYawControlModified = fYawControl;
		float fPitchControlModified = fPitchControl;
		float fRollControlModified = fRollControl;
		float fThrottleControlModified = fThrottleControl;
		fYawControlModified *= fAbilityMult;
		fPitchControlModified *= fAbilityMult;
		fRollControlModified *= fAbilityMult;

		// Input lagging for inexperienced pilots
		const float fLaggedYawControl = pPlayerInfo->GetLaggedYawControl();
		const float fLaggedPitchControl = pPlayerInfo->GetLaggedPitchControl();
		const float fLaggedRollControl = pPlayerInfo->GetLaggedRollControl();

		float fControlLaggingBlendingRate = (1.0f - fHeliControlAbilityMult) * ms_fHeliControlLaggingMinBlendingRate + fHeliControlAbilityMult * ms_fHeliControlLaggingMaxBlendingRate;
		fControlLaggingBlendingRate *= m_fControlLaggingRateMulti;
		fYawControlModified = Clamp(fYawControlModified, fLaggedYawControl - fControlLaggingBlendingRate * fTimeStep, fLaggedYawControl + fControlLaggingBlendingRate * fTimeStep);
		fPitchControlModified = Clamp(fPitchControlModified, fLaggedPitchControl - fControlLaggingBlendingRate * fTimeStep, fLaggedPitchControl + fControlLaggingBlendingRate * fTimeStep);
		fRollControlModified = Clamp(fRollControlModified, fLaggedRollControl - fControlLaggingBlendingRate * fTimeStep, fLaggedRollControl + fControlLaggingBlendingRate * fTimeStep);

		pPlayerInfo->SetLaggedYawControl(fYawControlModified);
		pPlayerInfo->SetLaggedPitchControl(fPitchControlModified);
		pPlayerInfo->SetLaggedRollControl(fRollControlModified);

		// Random control inputs for inexperienced pilots
		float fTimeBetweenRandomControlInputs = pPlayerInfo->GetTimeBetweenRandomControlInputs();
		float fRandomControlYaw = pPlayerInfo->GetRandomControlYaw();
		float fRandomControlPitch = pPlayerInfo->GetRandomControlPitch();
		float fRandomControlRoll = pPlayerInfo->GetRandomControlRoll();
		float fRandomControlThrottle = pPlayerInfo->GetRandomControlThrottle();

		fTimeBetweenRandomControlInputs -= fTimeStep;
		if( fTimeBetweenRandomControlInputs <= 0.0f )
		{
			float fAbilityRandomMult = Clamp(ms_fHeliControlAbilityControlRandomMin + ((ms_fHeliControlAbilityControlRandomMax - ms_fHeliControlAbilityControlRandomMin) * (1.0f - fHeliControlAbilityMult)), 0.0f, 1.0f);
			fRandomControlYaw = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult, fAbilityRandomMult);
			fRandomControlPitch = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult, fAbilityRandomMult);
			fRandomControlRoll = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult, fAbilityRandomMult);
			fRandomControlThrottle = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult, 0.0f);
			fTimeBetweenRandomControlInputs = Clamp(ms_fTimeBetweenRandomControlInputsMin + ((ms_fTimeBetweenRandomControlInputsMax - ms_fTimeBetweenRandomControlInputsMin) * (1.0f - fHeliControlAbilityMult)), 0.0f, 1.0f);
		}
		fYawControlModified += fRandomControlYaw;
		fPitchControlModified += fRandomControlPitch;
		fRollControlModified += fRandomControlRoll;
		fThrottleControlModified += fRandomControlThrottle;

		fYawControl += (fYawControlModified - fYawControl) * m_pilotSkillNoiseScalar;
		fPitchControl += (fPitchControlModified - fPitchControl) * m_pilotSkillNoiseScalar;
		fRollControl += (fRollControlModified - fRollControl) * m_pilotSkillNoiseScalar;
		fThrottleControl += (fThrottleControlModified - fThrottleControl) * m_pilotSkillNoiseScalar;

		fRandomControlYaw = Lerp(fHeliDifficulty, fRandomControlYaw, 0.0f);
		fRandomControlPitch = Lerp(fHeliDifficulty, fRandomControlPitch, 0.0f);
		fRandomControlRoll = Lerp(fHeliDifficulty, fRandomControlRoll, 0.0f);
		fRandomControlThrottle = Lerp(fHeliDifficulty, fRandomControlThrottle, 0.0f);

		// Store the values back on the player info
		pPlayerInfo->SetTimeBetweenRandomControlInputs(fTimeBetweenRandomControlInputs);
		pPlayerInfo->SetRandomControlYaw(fRandomControlYaw);
		pPlayerInfo->SetRandomControlPitch(fRandomControlPitch);
		pPlayerInfo->SetRandomControlRoll(fRandomControlRoll);
		pPlayerInfo->SetRandomControlThrottle(fRandomControlThrottle);
	}
}

u32 uAbandonedTimeForCrashing = (u32)(fwTimer::GetMaximumFrameTime() * 2000.0f); // Two frames
float fDeadDriverCrashingHeight = 3.0f;
float fAbandonedCrashingHeight = 5.0f;
void CRotaryWingAircraft::SetIsAbandoned(const CPed *pOriginalDriver)
{
	if(GetStatus()==STATUS_WRECKED)
		return;

	if(GetStatus() != STATUS_ABANDONED)
	{
		if (!IsNetworkClone())
		{
			bool bStartCrashing = false;
			if( GetSeatManager()->GetNumPlayers() <= 1 && // the SetIsAbandoned is called before removing the driver from the vehicle
				pOriginalDriver && 
				pOriginalDriver->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_EXIT_VEHICLE_SEAT))
			{
				CEntity* lastDriver = const_cast<CPed*>(pOriginalDriver);
				bStartCrashing = StartCrashingBehavior(lastDriver, true);
			}

			if(!bStartCrashing)
			{
				aiTask *pActiveTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_NO_DRIVER, VEHICLE_TASK_PRIORITY_PRIMARY);
				if(!pActiveTask)
				{
					GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, 
						rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_ABANDONED), 
						VEHICLE_TASK_PRIORITY_PRIMARY, 
						false);
				}
			}
		}
	}

	SetStatus(STATUS_ABANDONED);
}

void CRotaryWingAircraft::SetIsOutOfControl()
{
	if(GetStatus()==STATUS_WRECKED)
		return;

	// Use some randomness if the vehicle is set out of control, used when the driver is killed, etc.
	if(GetStatus() != STATUS_OUT_OF_CONTROL)
	{
		m_fHeliOutOfControlYaw = fwRandom::GetRandomNumberInRange(-sfHeliTailLostYaw, sfHeliTailLostYaw);
		m_fHeliOutOfControlRoll = fwRandom::GetRandomNumberInRange(sfHeliTailLostRoll, sfHeliOutOfControlMaxRoll);
        m_fHeliOutOfControlPitch = fwRandom::GetRandomNumberInRange(-sfHeliOutOfControlPitch, sfHeliOutOfControlPitch);

        static dev_float sfRandomMinThrottleWeighting = 0.2f;
        m_fHeliOutOfControlThrottle = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sfRandomMinThrottleWeighting ? sfHeliOutOfControlMinThrottle : sfHeliOutOfControlMaxThrottle;

        m_iHeliOutOfControlRecoverPeriods = siHeliOutOfControlRecoveryPeriodsMax;

        m_uHeliOutOfControlRecoverStart = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(siHeliOutOfControlInitialFailPeriodsMin, siHeliOutOfControlInitialFailPeriodsMax);

        m_bHeliOutOfControlRecovering = false;

		if(m_fRearRotorHealth <= 0.0f) // If the rear rotor broken off, vehicle will be out of control right away
		{
			m_iHeliOutOfControlRecoverPeriods--;
		}

		//don't override crash task if we're already crashing
		aiTask* pCrashTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH);
        if (!IsNetworkClone() && !pCrashTask)
        {  
            sVehicleMissionParams params;

            // Just fly to a far away position because we are trying to get away. We should stream out before reaching it
			Vector3 vTargetPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

            static dev_float sfTargetOffset = 100.0f;
			vTargetPos.x += fwRandom::GetRandomNumberInRange(-sfTargetOffset, sfTargetOffset);
            vTargetPos.y += fwRandom::GetRandomNumberInRange(-sfTargetOffset, sfTargetOffset);

            params.SetTargetPosition(vTargetPos);

            params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
            CTaskVehicleGoToHelicopter *pHeliTask = rage_new CTaskVehicleGoToHelicopter(params, 0, -1.0f, 40);

            GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pHeliTask, VEHICLE_TASK_PRIORITY_PRIMARY, false );
        }
	}

    SetStatus(STATUS_OUT_OF_CONTROL);
	if(IsInAir())
	{
		audHeliAudioEntity * audio ( (audHeliAudioEntity*)GetVehicleAudioEntity());
		if(audio)
		{
			audio->TriggerGoingDownSound();
		}
	}
}

bool CRotaryWingAircraft::StartCrashingBehavior(CEntity *pCulprit, bool bIsAbandend)
{
	if (!IsNetworkClone())
	{
		//already crashing
		aiTask *pActiveTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH);
		if(pActiveTask)
		{
			return true;
		}

		bool bShouldCrash = true;
		//always crash if no collision, otherwise we'll fall through world
		//or no tail boom as then we can't fly anyway
		if(IsCollisionLoadedAroundPosition() && !GetIsTailBoomBroken())
		{
			if(!IsInAir() ||
				( pHandling->GetSeaPlaneHandlingData() &&
				  m_nFlags.bPossiblyTouchesWater ) )
			{
				bShouldCrash = false;
			}

			if(GetMainRotorSpeed() < MIN_ROT_SPEED_HELI_CONTROL)
			{
				bShouldCrash = false;
			}

			if(bShouldCrash)
			{
				float fCrashingHeightThreshold = bIsAbandend? fAbandonedCrashingHeight : fDeadDriverCrashingHeight;
				Vector3 vStart = VEC3V_TO_VECTOR3(GetVehiclePosition());
				Vector3 vEnd = vStart;
				vEnd.z += GetBoundingBoxMin().z - fCrashingHeightThreshold;
				WorldProbe::CShapeTestHitPoint probeHitPoint;
				WorldProbe::CShapeTestResults probeResults(probeHitPoint);
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(vStart, vEnd);
				probeDesc.SetResultsStructure(&probeResults);
				probeDesc.SetExcludeEntity(this);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
				probeDesc.SetIsDirected(true);

				bShouldCrash = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			}
		}

		if(bShouldCrash)
		{
			CTaskVehicleCrash *pCarTask = rage_new CTaskVehicleCrash(pCulprit);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_BlowUpInstantly, false);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_InACutscene, false);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_AddExplosion, true);

			GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_CRASH);

			return true;
		}
	}

	return false;
}



float CRotaryWingAircraft::ms_fHeliControlLaggingMinBlendingRate = 1.0f;
float CRotaryWingAircraft::ms_fHeliControlLaggingMaxBlendingRate = 10.0f;
float CRotaryWingAircraft::ms_fHeliControlAbilityControlDampMin = 0.7f;
float CRotaryWingAircraft::ms_fHeliControlAbilityControlDampMax = 1.0f;
float CRotaryWingAircraft::ms_fHeliControlAbilityControlRandomMin = 0.0f;
#if RSG_PC
float CRotaryWingAircraft::ms_fHeliControlAbilityControlRandomMax = 0.28f;
#else
float CRotaryWingAircraft::ms_fHeliControlAbilityControlRandomMax = 0.56f;
#endif
float CRotaryWingAircraft::ms_fTimeBetweenRandomControlInputsMin = 3.0f;
float CRotaryWingAircraft::ms_fTimeBetweenRandomControlInputsMax = 6.0f;
float CRotaryWingAircraft::ms_fRandomControlLerpDown			= 0.995f;
float CRotaryWingAircraft::ms_fMaxAbilityToAdjustDifficulty		= 0.6f;

SearchLightInfo CRotaryWingAircraft::ms_SearchLightInfo;
#if __BANK
void CRotaryWingAircraft::InitWidgets()
{
	rage::bkBank* bank = BANKMGR.FindBank("Input");
	if(AssertVerify(bank))
	{
		bank->PushGroup("Heli controls");
			bank->AddToggle("Visualise Heli Controls",&sbVisualiseHeliControls);
			bank->AddSlider("Lagging control min blending rate", &CRotaryWingAircraft::ms_fHeliControlLaggingMinBlendingRate, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("Lagging control max blending rate", &CRotaryWingAircraft::ms_fHeliControlLaggingMaxBlendingRate, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fHeliControlAbilityControlDampMin", &CRotaryWingAircraft::ms_fHeliControlAbilityControlDampMin, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fHeliControlAbilityControlDampMax", &CRotaryWingAircraft::ms_fHeliControlAbilityControlDampMax, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fHeliControlAbilityControlRandomMin", &CRotaryWingAircraft::ms_fHeliControlAbilityControlRandomMin, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fHeliControlAbilityControlRandomMax", &CRotaryWingAircraft::ms_fHeliControlAbilityControlRandomMax, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fTimeBetweenRandomControlInputsMin", &CRotaryWingAircraft::ms_fTimeBetweenRandomControlInputsMin, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fTimeBetweenRandomControlInputsMax", &CRotaryWingAircraft::ms_fTimeBetweenRandomControlInputsMax, 0.0f, 100.0f, 0.1f);
			bank->AddSlider("fRandomControlLerpDown", &CRotaryWingAircraft::ms_fRandomControlLerpDown, 0.0f, 100.0f, 0.1f);
		bank->PopGroup();
	}
}

void CRotaryWingAircraft::VisualisePitchRoll()
{
	if(sbVisualiseHeliControls)
	{
		static Vector2 vRollDebugPos(0.65f,0.25f);
		static Vector2 vPitchDebugPos(0.75f,0.15f);
		static Vector2 vYawDebugPos(0.65f,0.5f);
		static Vector2 vThrottleDebugPos(0.5f,0.15f);
		float fScale = 0.2f;
		float fEndWidth = 0.01f;

		grcDebugDraw::Meter(vPitchDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Pitch");
		grcDebugDraw::MeterValue(vPitchDebugPos, Vector2(0.0f,1.0f), fScale, m_fPitchControl, fEndWidth, Color32(255,0,0));	

		grcDebugDraw::Meter(vRollDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Roll");
		grcDebugDraw::MeterValue(vRollDebugPos, Vector2(1.0f,0.0f), fScale, m_fRollControl, fEndWidth, Color32(255,0,0));	

		grcDebugDraw::Meter(vYawDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Yaw");
		grcDebugDraw::MeterValue(vYawDebugPos, Vector2(1.0f,0.0f), fScale, m_fYawControl, fEndWidth, Color32(255,0,0));	

		grcDebugDraw::Meter(vThrottleDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Throttle");
		grcDebugDraw::MeterValue(vThrottleDebugPos, Vector2(0.0f,1.0f), fScale, 2.0f*(0.5f-m_fThrottleControl), fEndWidth, Color32(255,0,0));	
		/*
		static const float fWidth = 1.0f;
		static const float fHeight = 1.0f;
		static const float fDebugAxisLength = 100.0f/320.0f;


		// Visualise roll
		Vector2 vMin, vMax, vValue;
		vMin.x = (fWidth / 2.0f) - (fDebugAxisLength/2.0f);
		vMin.y = (fHeight / 3.0f) - 20.0f/240.0f;

		vMax.x = vMin.x + fDebugAxisLength;
		vMax.y = vMin.y;

		vValue.y = vMin.y;
		vValue.x = (vMax.x-vMin.x) *(0.5f*-m_fRollControl+0.5f) + vMin.x;

		grcDebugDraw::Line(vMin,vMax,Color32(255,255,255));
		grcDebugDraw::Line(vMin - Vector2(0.0f,5.0f/240.0f),vMin+ Vector2(0.0f,5.0f/240.0f),Color32(255,255,255));
		grcDebugDraw::Line(vMax - Vector2(0.0f,5.0f/240.0f),vMax+ Vector2(0.0f,5.0f/240.0f),Color32(255,255,255));
		grcDebugDraw::Line(vValue - Vector2(0.0f,5.0f/240.0f),vValue+ Vector2(0.0f,5.0f/240.0f),Color32(255,0,0));

		// Visualise pitch
		vMin.x = (4.0f*fWidth / 5.0f) + 20.0f/320.0f;
		vMin.y = (fHeight / 3.0f);

		vMax.x = vMin.x;
		vMax.y = vMin.y + fDebugAxisLength;

		// Y axis goes from top to bottom so need to multiply by -ve 1
		vValue.y = (vMax.y - vMin.y) *(-0.5f*m_fPitchControl+0.5f) + vMin.y;
		vValue.x = vMin.x;

		grcDebugDraw::Line(vMin,vMax,Color32(255,255,255));
		grcDebugDraw::Line(vMin - Vector2(5.0f/320.0f,0.0f),vMin+ Vector2(5.0f/320.0f,0.0f),Color32(255,255,255));
		grcDebugDraw::Line(vMax - Vector2(5.0f/320.0f,0.0f),vMax+ Vector2(5.0f/320.0f,0.0f),Color32(255,255,255));
		grcDebugDraw::Line(vValue - Vector2(5.0f/320.0f,0.0f),vValue+ Vector2(5.0f/320.0f,0.0f),Color32(255,0,0));
		*/
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
// CHeli
//
//
//
bank_float CHeli::ms_fRotorSpeedMults[] = 
{
	25.2f,	// Main
	59.6f,	// Rear

	25.2f,	// Main_2
	45.0f	// Rear_2 - used for the drone rotor speed
};
CompileTimeAssert(CRotaryWingAircraft::Num_Heli_Rotors == 2);

dev_float s_minTurbulenceScalar = 0.1f;
dev_float s_maxTurbulenceScalar = 1.0f;
dev_float s_massForMaxTurbulenceScalar = 4000.0f;
dev_float s_massForMinTurbulenceScalar = 20000.0f;


CHeli::CHeli(const eEntityOwnedBy ownedBy, u32 popType, VehicleType veh) 
: CRotaryWingAircraft(ownedBy, popType, veh)
, m_InitialRopeLength(0.0f)
, m_pAntiSwayEntity(NULL)
, m_vLastFramesAntiSwayEntityDistance(0.0f, 0.0f, 0.0f)
, m_AntiSwaySpringDistance(-8.0f)
, m_fEngineDegradeTimer(0.0f)
, m_turbulenceTimer(0.0f)
, m_turbulenceRecoverTimer(0.0f)
, m_turbulenceRoll(0.0f)
, m_turbulencePitch(0.0f)
, m_turbulenceDrop(0.0f)
, m_turbulenceScalar(1.0f)
, m_fTimeSpentLanded(0.0f)
, m_fTimeSpentCollidingNotLanded(0.0f)
, m_fJetPackGroundHeight(0.0f)
, m_vHeliSteeringBias(VEC3_ZERO)
, m_bPoliceDispatched(false)
, m_bSwatDispatched(false)
, m_nPickupRopeType(PICKUP_HOOK)
{
#if REGREF_VALIDATE_CDCDCDCD
	fwRefAwareBaseImpl_AddInterestingObject(this);
#endif
	m_pOwnerPlayer		= NULL;
	m_bHasLandingGear	= false;
	m_bDisableExplodeFromBodyDamage = false;
	m_bCanPickupEntitiesThatHavePickupDisabled = false;
	m_bDisableSelfRighting = false;

	if(veh == VEHICLE_TYPE_HELI)
	{
		gPostScan.AddToAlwaysPreRenderList(this);
	}

	for(int i = 0; i < eNumRopeIds; ++i)
	{
		m_Ropes[i]=NULL;
	}
}


//
//
//
CHeli::~CHeli()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

#if GTA_REPLAY 
	//Don't allow heli to delete rope that it does not own. Let CPacketDeleteRope handle it during replay.
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		for(int i = 0; i < eNumRopeIds; ++i)
		{
			// Remove any existing ropes
			if(m_Ropes[i])
			{
#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(m_Ropes[i]->GetUniqueID());
					CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(m_Ropes[i]->GetUniqueID());
					CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(m_Ropes[i]->GetUniqueID()),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_Ropes[i]), pAttachedA, pAttachedB, false);
				}
#endif //GTA_REPLAY

				CPhysics::GetRopeManager()->RemoveRope(m_Ropes[i]);
				m_Ropes[i]=NULL;
			}
		}
	}
}

bool CHeli::ShouldCreateLandingGear()
{
	CVehicleModelInfo* pModelInfo = GetVehicleModelInfo();
	return (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HELICOPTER_WITH_LANDING_GEAR) && GetBoneIndex(FIRST_LANDING_GEAR) > -1);
}

/////////////////////////////////////////////////////////////////////////////////////////
int CHeli::InitPhys()
{
	int result = CRotaryWingAircraft::InitPhys();

	if( ShouldCreateLandingGear() )
	{
		m_landingGear.InitPhys( this, LANDING_GEAR_F, LANDING_GEAR_RM1, LANDING_GEAR_LM1, LANDING_GEAR_RR, LANDING_GEAR_RL, LANDING_GEAR_RM );
		m_bHasLandingGear = true;
	}

	float mass = Clamp( GetMass(), s_massForMaxTurbulenceScalar, s_massForMinTurbulenceScalar );
	mass -= s_massForMaxTurbulenceScalar;

	if( mass > 0.0f )
	{
		mass /= ( s_massForMinTurbulenceScalar - s_massForMaxTurbulenceScalar );
	}

	m_turbulenceScalar = s_maxTurbulenceScalar - ( mass * ( s_maxTurbulenceScalar - s_minTurbulenceScalar ) );

	return result;
}

void CHeli::InitWheels() 
{ 
	CAutomobile::InitWheels(); 

	for(int i = 0; i < GetNumWheels(); i++)
	{
		Assert(GetWheel(i));
		GetWheel(i)->GetConfigFlags().SetFlag(WCF_UPDATE_SUSPENSION);
	}

	if( m_bHasLandingGear ) 
	{ 
		m_landingGear.InitWheels(this); 
	}
}

CHeliIntelligence *CHeli::GetHeliIntelligence() const
{
	Assert(dynamic_cast<CHeliIntelligence*>(m_pIntelligence));

	return static_cast<CHeliIntelligence*>(m_pIntelligence);
}

void CHeli::SetModelId(fwModelId modelId)
{
	CRotaryWingAircraft::SetModelId(modelId);

#if __ASSERT
	const char* strDebugPropNames[] = 
	{
		"Main",
		"Rear",
		"Main_2",
		"Rear_2"
	};
#endif

	// Figure out which propellers are valid, and set them up
	for(int nPropIndex = 0 ; nPropIndex < HELI_NUM_ROTORS; nPropIndex++ )
	{
		CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();

		eRotationAxis nAxis = ROT_AXIS_LOCAL_Z;
		if( (nPropIndex != Rotor_Main) && (!pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_ROTATE_TAIL_ROTOR)) )//if were not the main rotor and we don't have "dont rotate" flag set, rotate on the x axis
			nAxis = ROT_AXIS_LOCAL_X;

		if( GetIsDrone() )
		{
			nAxis = ROT_AXIS_LOCAL_Z;
		}
		if( GetModelIndex() == MI_JETPACK_THRUSTER )
		{
			nAxis = ROT_AXIS_LOCAL_Z;
		}

		eHierarchyId nId = (eHierarchyId)(HELI_ROTOR_MAIN + nPropIndex);
		if(vehicleVerifyf(GetBoneIndex(nId) > -1,"Vehicle %s is missing a propeller: %s",GetModelName(),strDebugPropNames[nPropIndex])
			&& vehicleVerifyf(m_iNumPropellers < Num_Heli_Rotors,"Out of room for plane propellers"))
		{
			// Found a valid propeller
			m_propellers[m_iNumPropellers].Init(nId,nAxis, this);
			m_propellerCollisions[m_iNumPropellers].Init(nId,this);
			m_iNumPropellers++;
		}
	}

	if( GetIsDrone() )
	{
		for(int nPropIndex = HELI_NUM_ROTORS ; nPropIndex < HELI_NUM_DRONE_ROTORS; nPropIndex++ )
		{
			eRotationAxis nAxis = ROT_AXIS_LOCAL_Z;
			eHierarchyId nId = (eHierarchyId)(HELI_ROTOR_MAIN + nPropIndex);
			if(vehicleVerifyf(GetBoneIndex(nId) > -1,"Vehicle %s is missing a propeller: %s",GetModelName(),strDebugPropNames[nPropIndex])
				&& vehicleVerifyf(m_iNumPropellers < Num_Drone_Rotors,"Out of room for plane propellers"))
			{
				// Found a valid propeller
				m_propellers[m_iNumPropellers].Init(nId,nAxis, this);
				m_propellerCollisions[m_iNumPropellers].Init(nId,this);
				m_iNumPropellers++;
			}
		}
	}

	if( m_bHasLandingGear )
	{
		// Turn collision on for landing gear
		m_landingGear.InitCompositeBound( this );
	}

	// Call InitCompositeBound again, as it has dependency with propellers
	InitCompositeBound();

	// cargobob can survive from on rocket explosion
	if(GetIsCargobob())
	{
		m_nVehicleFlags.bExplodesOnHighExplosionDamage = false;
	}

	// Cache off the initial rotor position so we don't need to compute it at runtime
	if(GetSkeleton() && GetBoneIndex(HELI_ROTOR_REAR) > -1 && !GetIsJetPack())
	{
		GetDefaultBonePositionForSetup(HELI_ROTOR_REAR,m_vecRearRotorPosition);
	}
	else
	{
		m_vecRearRotorPosition = Vector3(0.0f, 0.0f, GetBoundingBoxMax().z);
	}

	// If this is a seaplane, set up the anchor helper and the extension class for instance variables.
	if( pHandling->GetSeaPlaneHandlingData() )
	{
		CSeaPlaneExtension* pExtension = GetExtension<CSeaPlaneExtension>();
		if( !pExtension )
		{
			pExtension = rage_new CSeaPlaneExtension();
			Assert( pExtension );
			GetExtensionList().Add( *pExtension );
		}

		if( pExtension )
		{
			pExtension->GetAnchorHelper().SetParent( this );
		}
	}
}

void CHeli::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
// DEBUG!! -AC, This call to process control is either at the wrong place (it should be moved to the bottom of this function)
// or it is at the right place (and all other vehicle::ProcessControl functions should emulate it).
// then can go ahead and call the parent version of process control
	CRotaryWingAircraft::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
// END DEBUG!!

	if (!m_nVehicleFlags.bAnimatePropellers)
	{
		UpdateRotorSpeed();
	}

	UpdateRopes();
	ProcessLanded(fFullUpdateTimeStep);

	if( HasLandingGear() )
	{
		m_landingGear.ProcessControl( this );
	}

	ProcessWings();
}

// void CHeli::ProcessDriverInputsForPlayer(CPed *pPlayerPed)
// {
// 	Assert(pPlayerPed && (pPlayerPed->IsControlledByLocalPlayer()));
// 	CControl *pControl = pPlayerPed->GetControlFromPlayer();
// 
// 	// auto throttle will keep us hovering, but won't stop us stopping from moving up, if that makes any sense?
// 	float fDesiredThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();
// 
// 	float fAutoThrottle = 0.0f;
// 	{
// 		if(!GetNumContactWheels())
// 			fAutoThrottle = 0.98f * rage::Clamp(1.0f - GetVelocity().z * HELI_AUTO_THROTTLE_FALLOFF, 0.0f, 1.0f);
// 
// 		// need to combine desired throttle and auto throttle, somehow
// 		if(fDesiredThrottleControl > 0.01f)
// 		{
// 			fDesiredThrottleControl += 1.0f;
// 		}
// 		else if(fDesiredThrottleControl < -0.01f)
// 		{
// 			fDesiredThrottleControl = rage::Max(-0.1f, fDesiredThrottleControl + 0.5f);
// 		}
// 		else
// 		{
// 			fDesiredThrottleControl = fAutoThrottle;
// 		}
// 	}
// 
// 
// 	float fThrottleChangeMult = rage::Powf(HELI_THROTTLE_CONTROL_DAMPING, fwTimer::GetTimeStep());
// 
// 	// Store this here since it will be overridden by superclass process control inputs
// 	fDesiredThrottleControl = fThrottleChangeMult*m_fThrottleControl + (1.0f - fThrottleChangeMult)*fDesiredThrottleControl;
// 
// 	CRotaryWingAircraft::ProcessDriverInputsForPlayer(pPlayerPed);
// 
// 	SetThrottleControl(fDesiredThrottleControl);
// 	
// 
// }

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
void CHeli::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion )
{
	CRotaryWingAircraft::BlowUpCar(pCulprit,bInACutscene,bAddExplosion,bNetCall,weaponHash,bDelayedExplosion);
}

//////////////////////////////////////////////////////////////////////////
//This should only be called from the crash task
//////////////////////////////////////////////////////////////////////////
void CHeli::FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion )
{
	//This should only be called from the crash task
	Assert(IsNetworkClone() || GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH));

	// If this heli was chasing a player we will make it so that the player doesn't get any new helis created for a while.
	if (GetOwnerPlayer())
	{
		((CNetGamePlayer*)GetOwnerPlayer())->GetPlayerPed()->GetPlayerInfo()->m_LastTimeHeliWasDestroyed = fwTimer::GetTimeInMilliseconds();
		SetOwnerPlayer(NULL);
	}
		
	CRotaryWingAircraft::FinishBlowingUpVehicle(pCulprit,bInACutscene,bAddExplosion, bNetCall, weaponHash, bDelayedExplosion);

	if( GetIsCargobob() )
	{
		RemoveRopesAndHook();
	}
}
void CHeli::ProcessControlInputsInactive(CPed *)
{
	if(HasContactWheels() && GetVelocity().Mag2() < 16.0f)
	{
		m_fThrottleControl = 0.0f;
		m_fYawControl = 0.0f;
		m_fPitchControl = 0.0f;
		m_fRollControl = 0.0f;
	}
}

static dev_float sfSpringConstant = -3.3f;
static dev_float sfDampingConstant = 1.7f;
/*
static dev_float sfHeliControlMichealMulti = 0.7f;
static dev_float sfHeliControlFrankMulti = 0.5f;
static dev_float sfHeliControlTrevorMulti = 0.9f;
*/
extern float TurbulenceSideWindThreshold; // from Planes.cp
extern float FullRumbleSideWindSpeed;	// from Planes.cpp
static dev_float sfLiftForceMaxMultiWhenMissingFiring = 0.1f;

// Turbulence related tunings
bank_float sfRandomRollNoiseLower = -1.5f;//prefer the random noise moving the plane forward then back
bank_float sfRandomRollNoiseUpper = 1.5f; 
bank_float sfRandomPitchNoiseLower = -1.0f;
bank_float sfRandomPitchNoiseUpper = 1.0f; 
bank_float sfRandomDropNoiseLower = -1.0f;
bank_float sfRandomDropNoiseUpper = 0.2f; 
bank_float sfRandomNoiseSpeedMax = 30.0f;

#if RSG_PC
bank_float sfRandomNoiseIdleMultForPlayer = 0.05f;
bank_float sfRandomNoiseThrottleMultForPlayer = 0.025f;
bank_float sfRandomNoiseSpeedMultForPlayer = 0.125f;
#else
bank_float sfRandomNoiseIdleMultForPlayer = 0.1f;
bank_float sfRandomNoiseThrottleMultForPlayer = 0.05f;
bank_float sfRandomNoiseSpeedMultForPlayer = 0.25f;
#endif

bank_float sfRandomNoiseIdleMultForAI = 0.2f;
bank_float sfRandomNoiseThrottleMultForAI = 0.15f;
bank_float sfRandomNoiseSpeedMultForAI = 0.25f;
bank_float sfTurbulenceTimeMin = 0.5f;
bank_float sfTurbulenceTimeMax = 1.0f;
bank_float sfTurbulenceRecoverTimeMin = 1.5f;
bank_float sfTurbulenceRecoverTimeMax = 2.5f;
bank_float sfHeliPhysicalTurbulencePedVibMult = 0.0f;
dev_float sfHeliSteeringBiasFadeOutRate = 1.0f;
dev_float sfHeliSteeringBiasMag = 1.0f;

void CHeli::ProcessAntiSway(float fTimeStep)
{
	// If we have an entity that we don't want to sway too much, add a spring and damper
	if(m_pAntiSwayEntity && m_pAntiSwayEntity->GetCurrentPhysicsInst() && m_pAntiSwayEntity->GetCurrentPhysicsInst()->IsInLevel())
	{
		if( !m_pAntiSwayEntity->GetFrameCollisionHistory()->GetNumCollidedEntities() )
		{
			Vector3 vPosition = VEC3V_TO_VECTOR3(GetVehiclePosition());
			vPosition.z += m_AntiSwaySpringDistance;

			Vector3 vEntityDistance(VEC3V_TO_VECTOR3(m_pAntiSwayEntity->GetMatrix().d()) - vPosition);

			static dev_float sfMaxDistanceToDampSq = 100.0f*100.0f;
			if(vEntityDistance.Mag2() < sfMaxDistanceToDampSq)
			{
				Vector3 dampingToApply(VEC3_ZERO);

				Vector3 vDampingDistance = vEntityDistance-m_vLastFramesAntiSwayEntityDistance;

				if(fTimeStep > SMALL_FLOAT && vDampingDistance.Mag2() < sfMaxDistanceToDampSq)
				{
					dampingToApply = ((vDampingDistance) / fTimeStep);
				}

				Vector3 forceToApply = ((vEntityDistance * sfSpringConstant) - (dampingToApply * sfDampingConstant)) * m_pAntiSwayEntity->GetMass();// scale the torque by mass

				float fForceLimit = DEFAULT_ACCEL_LIMIT * Max(1.0f, m_pAntiSwayEntity->GetMass());
				float fForceMag = forceToApply.Mag();
				if(fForceMag > fForceLimit)
				{
					forceToApply *= (fForceLimit - 0.1f)/fForceMag;
				}

				m_pAntiSwayEntity->ApplyForceCg(forceToApply);
			}

			m_vLastFramesAntiSwayEntityDistance = vEntityDistance;
		}
	}
}

void CHeli::ProcessTurbulence(float fThrottleControl, float fTimeStep)
{
	const Mat34V& matrix = GetMatrixRef();
	const Vec3V vecRight = matrix.GetCol0();
	const Vec3V vecForward = matrix.GetCol1();
	Vector3 vecAngInertia = GetAngInertia();
	float fMass = GetMass();
	CPed* pDriver = GetDriver();
	bool bDrivenByPlayer = pDriver && pDriver->IsPlayer();
	CControl *pControl = NULL;
	if(bDrivenByPlayer && GetStatus()==STATUS_PLAYER)
	{
		pControl = pDriver->GetControlFromPlayer();
	}
	bool bControlInactive = pControl && CTaskVehiclePlayerDrive::IsThePlayerControlInactive(pControl);
	const Vec3V velocity = VECTOR3_TO_VEC3V(GetVelocity());
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();


	if(m_turbulenceRecoverTimer <= 0.0f)
	{
		if(m_turbulenceTimer > 0.0f)
		{
			// Apply pitch noise
			ApplyInternalTorque(m_turbulencePitch*ZAXIS*vecAngInertia.x, RCC_VECTOR3(vecForward));
			// Apply roll noise
			ApplyInternalTorque(m_turbulenceRoll*ZAXIS*vecAngInertia.y, RCC_VECTOR3(vecRight));
			// Apply drop noise
			ApplyInternalForceCg(m_turbulenceDrop*ZAXIS*fMass);

			m_turbulenceTimer -= fTimeStep;
		}
		else
		{
			// Recompute next turbulence values
			float fRandomNoiseIdleMult = sfRandomNoiseIdleMultForAI;
			float fRandomNoiseThrottleMult = sfRandomNoiseThrottleMultForAI;
			float fRandomNoiseSpeedMult = sfRandomNoiseSpeedMultForAI;

			if(bDrivenByPlayer)
			{
				fRandomNoiseIdleMult = sfRandomNoiseIdleMultForPlayer;
				fRandomNoiseThrottleMult = sfRandomNoiseThrottleMultForPlayer;
				fRandomNoiseSpeedMult = sfRandomNoiseSpeedMultForPlayer;
			}

			float fDamageRate = 1.0f - Min(GetHealth()/GetMaxHealth(), GetVehicleDamage()->GetEngineHealth()/ENGINE_HEALTH_MAX);
			fDamageRate = Clamp(fDamageRate, 0.0f, 1.0f);

			float fSpeedRate = RCC_VECTOR3(velocity).XYMag() / sfRandomNoiseSpeedMax;
			fSpeedRate = Clamp(fSpeedRate, 0.0f, 1.0f);

			float fRandomNoiseMult = fRandomNoiseIdleMult + fThrottleControl * fRandomNoiseThrottleMult + fDamageRate * pFlyingHandling->m_fBodyDamageControlEffectMult + fSpeedRate * fRandomNoiseSpeedMult + WeatherTurbulenceMult();

			m_turbulenceRoll = fwRandom::GetRandomNumberInRange( sfRandomRollNoiseLower*fRandomNoiseMult, sfRandomRollNoiseUpper*fRandomNoiseMult) * m_turbulenceScalar;
			m_turbulencePitch = fwRandom::GetRandomNumberInRange( sfRandomPitchNoiseLower*fRandomNoiseMult, sfRandomPitchNoiseUpper*fRandomNoiseMult) * m_turbulenceScalar;
			m_turbulenceDrop = fwRandom::GetRandomNumberInRange( sfRandomDropNoiseLower*fRandomNoiseMult, sfRandomDropNoiseUpper*fRandomNoiseMult) * m_turbulenceScalar;
			m_turbulenceTimer = fwRandom::GetRandomNumberInRange(sfTurbulenceTimeMin, sfTurbulenceTimeMax);
			m_turbulenceRecoverTimer = fwRandom::GetRandomNumberInRange(sfTurbulenceRecoverTimeMin, sfTurbulenceRecoverTimeMax);
		}
	}
	else
	{
		m_turbulenceRecoverTimer -= fTimeStep;

		if(bDrivenByPlayer && sfHeliPhysicalTurbulencePedVibMult > 0.0f && m_turbulenceRecoverTimer <= 0.0f && !bControlInactive)
		{
			u32 uTurbulenceTime = (u32)(m_turbulenceTimer * 1000.0f);
			CControlMgr::StartPlayerPadShakeByIntensity(uTurbulenceTime, sfHeliPhysicalTurbulencePedVibMult);
		}
	}
}

void CHeli::ProcessSteeringBias(float fTimeStep, float &fRollControl, float &fPitchControl)
{
	const Mat34V& matrix = GetMatrixRef();
	// Fades out the steering bias
	m_vHeliSteeringBias *= Max(1.0f - sfHeliSteeringBiasFadeOutRate * fTimeStep, 0.0f);

	// Update the steering bias
	if(GetMainRotorSpeed() > 0.0f)
	{
		if(const CCollisionHistory* pCollisionHistory = GetFrameCollisionHistory())
		{
			if(const CCollisionRecord* pCollisionRecord = pCollisionHistory->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_BUILDING))
			{
				if(pCollisionRecord->m_fCollisionImpulseMag > 0.0f)
				{
					Vector3 vAccumulatedContactToCenter = VEC3_ZERO;
					for(const CCollisionRecord* pColRecord = pCollisionHistory->GetFirstBuildingCollisionRecord();
						pColRecord != NULL ; pColRecord = pColRecord->GetNext())
					{
						if(pColRecord->m_MyCollisionComponent == GetMainRotorDisc() && pColRecord->m_fCollisionImpulseMag > 0.0f)
						{
							vAccumulatedContactToCenter += VEC3V_TO_VECTOR3(GetVehiclePosition()) - pColRecord->m_MyCollisionPos;
						}
					}

					if(!vAccumulatedContactToCenter.IsZero())
					{
						vAccumulatedContactToCenter.z = 0.0f;
						vAccumulatedContactToCenter.NormalizeSafe();
						vAccumulatedContactToCenter *= sfHeliSteeringBiasMag;
						m_vHeliSteeringBias += vAccumulatedContactToCenter;
						m_vHeliSteeringBias.NormalizeSafe();
						m_vHeliSteeringBias *= sfHeliSteeringBiasMag;
					}
				}
			}
		}
	}

	// Decompose the steering bias
	Vector3 vStteringBiasLocal = VEC3V_TO_VECTOR3(UnTransform3x3Ortho(matrix,RCC_VEC3V(m_vHeliSteeringBias)));
	fRollControl -= vStteringBiasLocal.x;
	fPitchControl -= vStteringBiasLocal.y;
}

void CHeli::ProcessFlightModel(float fTimeStep)
{
	const Vec3V velocity = VECTOR3_TO_VEC3V(GetVelocity());
	if(!m_bIsInAir && GetNumContactWheels() == 4 && m_fThrottleControl == 0.0f
		&& IsLessThanOrEqualAll(Abs(velocity), Vec3VFromF32(HELI_MINIMUM_CONTROL_SPEED)))
	{
		return;
	}

	// better make sure we've got some handling data, otherwise we're screwed for flying
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	if(pFlyingHandling == NULL)
		return;

	Assert(fTimeStep > 0.0f);

	const Mat34V& matrix = GetMatrixRef();
	const Vec3V vecRight = matrix.GetCol0();
	const Vec3V vecForward = matrix.GetCol1();
	const Vec3V vecUp = matrix.GetCol2();
	const Vec3V vecPosition = matrix.GetCol3();

	CControl *pControl = NULL;
	CPed* pDriver = GetDriver();
	bool bDrivenByPlayer = pDriver && pDriver->IsPlayer();
	if(bDrivenByPlayer && GetStatus()==STATUS_PLAYER)
		pControl = pDriver->GetControlFromPlayer();
	bool bControlInactive = pControl && CTaskVehiclePlayerDrive::IsThePlayerControlInactive(pControl);

	Vector3 vecAirSpeed = RCC_VECTOR3(velocity);
	Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
	// Exclude non-players and blimps from wind purely for optimization purposes
	if (bDrivenByPlayer && !InheritsFromBlimp() && !PopTypeIsMission() && !GetIsDrone() )
	{
		WIND.GetLocalVelocity(vecPosition, RC_VEC3V(vecWindSpeed), false, false);
		vecWindSpeed *= ComputeWindMult();
		vecAirSpeed -= vecWindSpeed;
	}

	float fMass = GetMass();
	Vector3 vecAngInertia = GetAngInertia();

#if __BANK	
	if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING && GetStatus()==STATUS_PLAYER)
	{
		Vector3 vecWindSocPos = VEC3V_TO_VECTOR3(AddScaled(vecPosition,vecUp,ScalarVFromF32(GetBoundingBoxMax().z)));
		grcDebugDraw::Line(vecWindSocPos, vecWindSocPos + 10.0f*vecWindSpeed, Color32(0,0,255));
		grcDebugDraw::Line(vecWindSocPos, vecWindSocPos - 10.0f*vecAirSpeed, Color32(0,255,0));
	}
#endif

	// Get local copies so we can mess about with them
	float fPitchControl = GetPitchControl();
	float fYawControl = GetYawControl();
	float fRollControl = GetRollControl();
	float fThrottleControl = GetThrottleControl();
	float fEngineSpeed = GetMainRotorSpeed();
	float fCollectiveControl = GetCollectiveControl();

	SetCollectiveControl(1.0f);
	Assert(FPIsFinite(fPitchControl));
	Assert(FPIsFinite(fYawControl));
	Assert(FPIsFinite(fRollControl));
	Assert(FPIsFinite(fThrottleControl));
	Assert(FPIsFinite(fEngineSpeed));
	Assert(FPIsFinite(fCollectiveControl));

	{
		// CONTROLS
		if(fPitchControl==FLY_INPUT_NULL)
		{
			fPitchControl = 0.0f;
			if(pControl)
				fPitchControl = pControl->GetVehicleFlyPitchUpDown().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
		}
		if(fRollControl==FLY_INPUT_NULL)
		{
			fRollControl = 0.0f;
			if(pControl)
				fRollControl = -pControl->GetVehicleFlyRollLeftRight().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
		}
		if(fYawControl==FLY_INPUT_NULL)
		{
			fYawControl = 0.0f;
			if(pControl)
			{
				// commented out as there are no mappings to these functions but I have left them here so they are easy to add again.
// 				if(pControl->GetVehicleLookRight().IsDown() && !pControl->GetVehicleLookLeft().IsDown())
// 					fYawControl = 1.0f;
// 				if(pControl->GetVehicleLookLeft().IsDown() && !pControl->GetVehicleLookRight().IsDown())	
// 					fYawControl = -1.0f; 
				// 2nd stick controls option for chris
				if(ABS(pControl->GetVehicleGunLeftRight().GetNorm()) > 0.008f)
					fYawControl = pControl->GetVehicleGunLeftRight().GetNorm();
			}
		}
		if(fThrottleControl==FLY_INPUT_NULL)
		{
			fThrottleControl = 0.0f;
			if(pControl)
				fThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();
		}

		static bool disableHeliTurbulence = false;
		if( CGameLogic::GetCurrentLevelIndex() == 2 )
		{
			TUNE_GROUP_BOOL( VEHICLE_TURBULENCE, DISABLE_HELI_TURBULENCE, true );
			disableHeliTurbulence = DISABLE_HELI_TURBULENCE;
		}
		else
		{
			TUNE_GROUP_BOOL( VEHICLE_TURBULENCE, DISABLE_HELI_TURBULENCE, false );
			disableHeliTurbulence = DISABLE_HELI_TURBULENCE;
		}


		// Apply the driver influence to controls
		if( !GetIsDrone() && bDrivenByPlayer && m_bIsInAir && !bControlInactive && !m_nVehicleFlags.bUsedForPilotSchool BANK_ONLY(&& !CVehicleFactory::ms_bflyingAce) && !disableHeliTurbulence )
		{
			ModifyControlsBasedOnFlyingStats(pDriver, pFlyingHandling, fYawControl, fPitchControl, fRollControl, fThrottleControl, fTimeStep);
		}

		if(GetMainRotorDisc() > -1 && m_bIsInAir && pDriver && !bDrivenByPlayer)
		{
			ProcessSteeringBias(fTimeStep, fRollControl, fPitchControl);
		}
        ////////////////////////
        // ADDED NOISE
        if(m_bIsInAir && !m_bDisableTurbulanceThisFrame && !bControlInactive && !InheritsFromBlimp() && !GetIsDrone() && !disableHeliTurbulence )
        {
			ProcessTurbulence(fThrottleControl, fTimeStep);
        }

		///////////////////////
		// LIFT&THRUST
		Vector3 vecLift(RCC_VECTOR3(vecUp));

		float fSpeedThroRotor = vecAirSpeed.Dot(vecLift);
		if(fSpeedThroRotor < 0.0f)
			fSpeedThroRotor *= 2.0f;

		// copy input argument to local var
		float fThrottleMult = fThrottleControl;
		if(fThrottleMult > 1.0f)
			fThrottleMult = 1.0f + (fThrottleMult - 1.0f)*pFlyingHandling->m_fThrust;

		Assert(IsEqualAll(GetTransform().GetPosition(),vecPosition));
		if(fThrottleMult > 0.0f)
		{
			float heightAboveCeiling = HeightAboveCeiling(vecPosition.GetZf());
			if(heightAboveCeiling > 0.0f)
			{
				fThrottleMult *= 10.0f/(heightAboveCeiling + 10.0f);
			}
		}

        if(m_bHoverMode)
        {
            vecLift = Vector3(0.0f, 0.0f, 1.0f);
            static dev_float sfLinearDampingForHoverMult = 10.0f;
            SetDampingForFlight(sfLinearDampingForHoverMult);
        }

		fThrottleMult -= pFlyingHandling->m_fThrustFallOff*fSpeedThroRotor;

		// Cut out the lift force if the engine is missing firing
		if(IsEngineOn() && m_Transmission.GetCurrentlyMissFiring())
		{
			fThrottleMult = Min(fThrottleMult, sfLiftForceMaxMultiWhenMissingFiring);
		}

        //Normal lift force
		Vector3 liftForce = vecLift;
        if(m_bEnableThrustVectoring)
        {
            // Additional force when pitching or rolling
			Vector3 negAdditionalThrustForce = pFlyingHandling->m_fThrustVectoring * (fPitchControl*RCC_VECTOR3(vecForward) + fRollControl*RCC_VECTOR3(vecRight));
			negAdditionalThrustForce.z = 0.0f;
			liftForce -= negAdditionalThrustForce;
		}

        if( GetIsJetPack() &&
            pControl && 
            m_nVehicleFlags.bEngineOn )
        {
            liftForce *= Max( pControl->GetVehicleFlyThrottleUp().GetNorm01(), liftForce.Dot( Vector3( 0.0f, 0.0f, 1.0f ) ) );
        }
		ApplyInternalForceCg(liftForce*(-GRAVITY*fThrottleMult*fEngineSpeed*fMass*fCollectiveControl));


		// only apply stabilising force when heli is right way up (to enable recovery from upside down)
		if(vecUp.GetZf() > 0.0f)
		{
			Vector3 vecTempUp(0.0f,0.0f,1.0f);
			vecTempUp += vecWindSpeed;
			vecTempUp.Normalize();

			float fForceOffset = -Clamp(RCC_VECTOR3(vecRight).Dot(vecTempUp), -pFlyingHandling->m_fFormLiftMult, pFlyingHandling->m_fFormLiftMult);
			ApplyInternalTorque((fEngineSpeed*pFlyingHandling->m_fAttackLiftMult*fForceOffset*vecAngInertia.y)*RCC_VECTOR3(vecUp), RCC_VECTOR3(vecRight));

			fForceOffset = -Clamp(RCC_VECTOR3(vecForward).Dot(vecTempUp), -pFlyingHandling->m_fFormLiftMult, pFlyingHandling->m_fFormLiftMult);
			ApplyInternalTorque((fEngineSpeed*pFlyingHandling->m_fAttackLiftMult*fForceOffset*vecAngInertia.x)*RCC_VECTOR3(vecUp), RCC_VECTOR3(vecForward));
		}
		else
		{
			float fForceOffset;
			if(vecRight.GetZf() < 0.0f)	fForceOffset = pFlyingHandling->m_fFormLiftMult;
			else	fForceOffset = -pFlyingHandling->m_fFormLiftMult;
			ApplyInternalTorque((fEngineSpeed*pFlyingHandling->m_fAttackLiftMult*fForceOffset*vecAngInertia.y)*RCC_VECTOR3(vecUp), RCC_VECTOR3(vecRight));

			if(vecForward.GetZf() < 0.0f)	fForceOffset = pFlyingHandling->m_fFormLiftMult;
			else	fForceOffset = -pFlyingHandling->m_fFormLiftMult;
			ApplyInternalTorque((fEngineSpeed*pFlyingHandling->m_fAttackLiftMult*fForceOffset*vecAngInertia.x)*RCC_VECTOR3(vecUp), RCC_VECTOR3(vecForward));
		}

		static dev_float sfOnGroundMult = 0.1f;
		float fOnGroundMult = m_bIsInAir ? 1.0f : sfOnGroundMult;

		if (!IsNetworkClone())
		{
			m_fJetpackStrafeForceScale = 0.0f;
		}		

		static dev_float strafeModeStrafeVelocityAccelerationMin = 10.0f;
		static dev_float strafeModeStrafeVelocityAccelerationMax = 15.0f;

		static dev_float sfJetpackMaxLateralAccelerationInv = 1.0f / ( strafeModeStrafeVelocityAccelerationMin + strafeModeStrafeVelocityAccelerationMax );
		
		if( !m_bStrafeMode )
		{
			ApplyInternalTorque(RCC_VECTOR3(vecUp)*(fPitchControl*fOnGroundMult*pFlyingHandling->m_fPitchMult*GetHoverModePitchMult()*vecAngInertia.x*fEngineSpeed), RCC_VECTOR3(vecForward));
			ApplyInternalTorque(RCC_VECTOR3(vecUp)*(fRollControl*fOnGroundMult*pFlyingHandling->m_fRollMult*vecAngInertia.y*fEngineSpeed), RCC_VECTOR3(vecRight));
			ProcessTail(vecAirSpeed, pFlyingHandling, fYawControl, fEngineSpeed);

			m_fStrafeModeTargetHeight = GetTransform().GetPosition().GetZf();

			if( GetIsJetPack() &&
                m_nVehicleFlags.bEngineOn )
			{
				// apply extra lateral drag if no stick input
				float invTimeStep = 1.0f / fTimeStep;

				static dev_float sfJetpackExtraLateralDragAcceleration = 5.0f;
				static dev_float sfExtraDragMaxRoll = 0.3f;
				static dev_float sfMaxLateralDragRollScale = 0.015f;
				static dev_float sfLaterDragTorqueScale = -1.5f;

				if( fRollControl == 0.0f )
				{
					float currentSpeed = GetVelocity().Dot( VEC3V_TO_VECTOR3( vecRight ) );
					float maxAcceleration = sfJetpackExtraLateralDragAcceleration * fEngineSpeed;
					float acceleration = Clamp( -currentSpeed * invTimeStep, -maxAcceleration, maxAcceleration );

					if (!IsNetworkClone())
					{
						m_fJetpackStrafeForceScale += acceleration * sfJetpackMaxLateralAccelerationInv;  
					}

					acceleration *= fMass;

					ApplyInternalForceCg( RCC_VECTOR3( vecRight ) * acceleration );

					float targetRoll = Clamp( currentSpeed * sfMaxLateralDragRollScale, -sfExtraDragMaxRoll, sfExtraDragMaxRoll );
					float torque = targetRoll - vecRight.GetZf();
					torque *= sfLaterDragTorqueScale * GetAngInertia().GetY();
					Vector3 vecTorque = VEC3V_TO_VECTOR3( vecForward ) * torque;

					ApplyInternalTorque( vecTorque );
				}
				if( fPitchControl == 0.0f )
				{
					float currentSpeed = GetVelocity().Dot( VEC3V_TO_VECTOR3( vecForward ) );
					float maxAcceleration = sfJetpackExtraLateralDragAcceleration * fEngineSpeed;
					float acceleration = Clamp( -currentSpeed * invTimeStep, -maxAcceleration, maxAcceleration );
					acceleration *= fMass;

					ApplyInternalForceCg( RCC_VECTOR3( vecForward ) * acceleration );

					float targetPitch = Clamp( -currentSpeed * sfMaxLateralDragRollScale, -sfExtraDragMaxRoll, sfExtraDragMaxRoll );
					float torque = targetPitch - vecForward.GetZf();
					torque *= sfLaterDragTorqueScale * GetAngInertia().GetX();
					Vector3 vecTorque = VEC3V_TO_VECTOR3( vecRight ) * torque;

					ApplyInternalTorque( vecTorque );
				}

				ApplyInternalForceCg( CalculateHoverForce( invTimeStep ) );
			}
		}
		else
		{
			float invTimeStep = 1.0f / fTimeStep;

			static dev_float strafeModeStrafeVelocityScale = 0.25f;

			float targetSpeed = pHandling->m_fEstimatedMaxFlatVel * strafeModeStrafeVelocityScale * -fRollControl;
			float currentSpeed = GetVelocity().Dot( VEC3V_TO_VECTOR3( vecRight ) );
			float maxAcceleration = strafeModeStrafeVelocityAccelerationMin + ( strafeModeStrafeVelocityAccelerationMax * fEngineSpeed * Abs( fRollControl ) );
			float acceleration = Clamp( ( targetSpeed - currentSpeed ) * invTimeStep, -maxAcceleration, maxAcceleration );

			if (!IsNetworkClone())
			{
				m_fJetpackStrafeForceScale += ( -fRollControl * 0.5f ) + ( 0.5f * acceleration * sfJetpackMaxLateralAccelerationInv );  
			}

			Vector3 cumulativeForce = RCC_VECTOR3( vecRight ) * acceleration;

			static dev_float strafeModeMaxRoll = 0.3f;
			static dev_float strafeModeMaxTorque = -2.0f;
			static dev_float strafeModeScaleOpposingTorque = 1.5f;
			float targetRoll = fRollControl * strafeModeMaxRoll;
			float torque = targetRoll - vecRight.GetZf();
			torque *= strafeModeMaxTorque * GetAngInertia().GetY();
			if( targetRoll == 0.0f )
			{
				torque *= strafeModeScaleOpposingTorque;
			}
			Vector3 vecTorque = VEC3V_TO_VECTOR3( vecForward ) * torque;

			ApplyInternalTorque( vecTorque );

			static dev_float strafeModeMaxPitch = 0.3f;
			static dev_float strafeModeMaxPitchTorque = 2.0f;
			float targetPitch = fPitchControl * strafeModeMaxPitch;
			torque = targetPitch - vecForward.GetZf();
			torque *= strafeModeMaxPitchTorque * GetAngInertia().GetX();
			if( targetRoll == 0.0f )
			{
				torque *= strafeModeScaleOpposingTorque;
			}
			vecTorque = VEC3V_TO_VECTOR3( vecRight ) * torque;

			ApplyInternalTorque( vecTorque );

			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
			float dirToCam = aimFrame.GetFront().Dot( VEC3V_TO_VECTOR3( vecForward ) );

			// only rotate towards the camera position if we are reasonable close to it
			if( dirToCam > 0.0f &&
				vecUp.GetZf() > 0.75f )
			{
				Vector3 aimPos = aimFrame.GetPosition() + ( aimFrame.GetFront() * 30.0f );
				aimPos = VEC3V_TO_VECTOR3( GetTransform().UnTransform( VECTOR3_TO_VEC3V( aimPos ) ) );
				aimPos.Normalize();
				static float sfYawTorqueScale = 15.0f;

				float targetYaw = -aimPos.x * Abs( aimPos.x );
				torque = targetYaw;
				torque *= sfYawTorqueScale;
				torque -= GetAngVelocity().GetZ();
				torque *= GetAngInertia().GetZ();
				vecTorque = Vector3( 0.0f, 0.0f, 1.0f ) * torque;

				ApplyInternalTorque( vecTorque );
			}

			static dev_float forwardSpeedScale = 1.25f;
			targetSpeed = pHandling->m_fEstimatedMaxFlatVel * strafeModeStrafeVelocityScale * -fPitchControl;
			if( targetSpeed > 0.0f )
			{
				targetSpeed *= forwardSpeedScale;
			}

			currentSpeed = GetVelocity().Dot( VEC3V_TO_VECTOR3( vecForward ) );
			maxAcceleration = strafeModeStrafeVelocityAccelerationMin + ( strafeModeStrafeVelocityAccelerationMax * fEngineSpeed * Abs( fPitchControl ) );
			acceleration = Clamp( ( targetSpeed - currentSpeed ) * invTimeStep, -maxAcceleration, maxAcceleration );

			cumulativeForce += RCC_VECTOR3( vecForward ) * acceleration;
			maxAcceleration = strafeModeStrafeVelocityAccelerationMin + ( strafeModeStrafeVelocityAccelerationMax * fEngineSpeed );

			float throttleControl = pControl ? pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01() : 0.0f;
			static dev_float sfVerticalAccelerationScale = 0.75f;
			static dev_float sfVerticalMaxSpeedScale = 0.5f;

			if( vecUp.GetZf() > 0.5f )
			{
				if( throttleControl != 0.0f )
				{
					targetSpeed = pHandling->m_fEstimatedMaxFlatVel * strafeModeStrafeVelocityScale * throttleControl * sfVerticalMaxSpeedScale;
					currentSpeed = GetVelocity().Dot( VEC3V_TO_VECTOR3( vecUp ) );
					maxAcceleration = strafeModeStrafeVelocityAccelerationMin + ( strafeModeStrafeVelocityAccelerationMax * fEngineSpeed * Abs( throttleControl ) ) * sfVerticalAccelerationScale;
					acceleration = Clamp( ( targetSpeed - currentSpeed ) * invTimeStep, -maxAcceleration, maxAcceleration );
					cumulativeForce += RCC_VECTOR3( vecUp ) * acceleration;
				}
				else
				{
					static dev_float sfStrafeModeHeightForce = 3.0f;
					float maxAcceleration = sfStrafeModeHeightForce * fEngineSpeed;
					float acceleration = Clamp( ( m_fStrafeModeTargetHeight - GetTransform().GetPosition().GetZf() ) * invTimeStep, -maxAcceleration, maxAcceleration );

					cumulativeForce += Vector3( 0.0f, 0.0f, 1.0f ) * acceleration;
				}

				cumulativeForce += CalculateHoverForce( invTimeStep );
			}

			if( cumulativeForce.Mag2() > maxAcceleration * maxAcceleration )
			{
				cumulativeForce.Normalize();
				cumulativeForce *= maxAcceleration;
			}
			cumulativeForce *= fMass;
			ApplyInternalForceCg( cumulativeForce );

			if( throttleControl != 0.0f )
			{
				m_fStrafeModeTargetHeight = GetTransform().GetPosition().GetZf();
			}
		}
	}

	// If the wind is strong enough, shake the control pad
	if(bDrivenByPlayer && m_bIsInAir)
	{
		float fSideWindSpeed = DotProduct(RCC_VECTOR3(vecRight), vecWindSpeed);
		if(Abs(fSideWindSpeed) > TurbulenceSideWindThreshold)
		{
			CControlMgr::StartPlayerPadShakeByIntensity(400,Abs(fSideWindSpeed) / FullRumbleSideWindSpeed);
		}
	}
}

void CHeli::ProcessTail(Vector3& vecAirSpeed, CFlyingHandlingData* pFlyingHandling, float fYawControl, float fEngineSpeed)
{
	const Mat34V& matrix = GetMatrixRef();
	const Vec3V vecRight = matrix.GetCol0();
	const Vec3V vecForward = matrix.GetCol1();
	const Vec3V vecUp = matrix.GetCol2();
	const Vec3V velocity = VECTOR3_TO_VEC3V(GetVelocity());

	// get tail position and add contribution from rotational velocity
	Vector3 vecTailAirSpeed = VEC3V_TO_VECTOR3(Transform3x3(matrix,RCC_VEC3V(m_vecRearRotorPosition)));
	vecTailAirSpeed.CrossNegate(GetAngVelocity());
	vecTailAirSpeed.Add(vecAirSpeed);

	float fSideSpeed = DotProduct(vecTailAirSpeed, RCC_VECTOR3(vecRight));
	float fFwdSpeed = DotProduct(vecTailAirSpeed, RCC_VECTOR3(vecForward));
	float fAirSpeedSqr = vecAirSpeed.Mag2();

	float fSideSlipAngle = -rage::Atan2f(fSideSpeed, fFwdSpeed);
	fSideSlipAngle = rage::Clamp(fSideSlipAngle, -HELI_RUDDER_MAX_ANGLE_OF_ATTACK, HELI_RUDDER_MAX_ANGLE_OF_ATTACK);

	float fMass = GetMass();
	Vector3 vecAngInertia = GetAngInertia();

	// doing sideways stuff
	Vector3 vecRudderForce(RCC_VECTOR3(vecRight));
	// sideways force from sidesliping
	vecRudderForce *= rage::Clamp(pFlyingHandling->m_fSideSlipMult * fSideSlipAngle * fAirSpeedSqr, -100.0f, 100.0f);
	ApplyInternalForceCg(vecRudderForce*fMass);

	// control force from steering and stabilising force from rudder
	vecRudderForce = RCC_VECTOR3(vecRight);

    if(m_bEnableThrustVectoring)
    {
        fYawControl = ComputeAdditionalYawFromTransform(fYawControl, GetTransform(), RCC_VECTOR3(velocity).XYMag());
    }

	vecRudderForce *= (pFlyingHandling->m_fYawMult*GetHoverModeYawMult()*fYawControl*fEngineSpeed + pFlyingHandling->m_fYawStabilise*fSideSlipAngle*fAirSpeedSqr)*vecAngInertia.z;
	ApplyInternalTorque(vecRudderForce, -RCC_VECTOR3(vecForward));

	// PITCH STABILISATION
	fSideSpeed = DotProduct(vecTailAirSpeed, RCC_VECTOR3(vecUp));
	fSideSlipAngle = -rage::Atan2f(fSideSpeed, fFwdSpeed);
	fSideSlipAngle = rage::Clamp(fSideSlipAngle, -HELI_RUDDER_MAX_ANGLE_OF_ATTACK, HELI_RUDDER_MAX_ANGLE_OF_ATTACK);

	vecRudderForce = RCC_VECTOR3(vecUp);
	//vecRudderForce *= pFlyingHandling->m_fPitchStabilise*fSideSlipAngle*rage::Abs(fSideSlipAngle)*vecAngInertia.x;
	vecRudderForce *= pFlyingHandling->m_fPitchStabilise*fSideSlipAngle*fAirSpeedSqr*vecAngInertia.x;
	ApplyInternalTorque(vecRudderForce, -RCC_VECTOR3(vecForward));
}

///////////////////////////////////////////////////////////////////////
//
// FUNCTION: DoHeliGenerationAndRemoval
// PURPOSE:  Possibly trigger helis
//
///////////////////////////////////////////////////////////////////////
 //float fRotRamp = 1.03f;
 #define MAX_NUM_ACTIVE_HELIS_ON_ONE_MACHINE	(3)

void CHeli::DoHeliGenerationAndRemoval()
{ 
	// This doesn't need to be done every frame.
	if ((fwTimer::GetSystemFrameCount() & 15) != 5)
	{
		return;
	}

	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) VehiclePool->GetSize();
	
	while(i--)
	{
		bool bHeliHasOrders = false;
		pVehicle = VehiclePool->GetSlot(i);
		
		// bMoveAwayFromPlayer is synced, don't set for clones otherwise helicopters will fly off when they migrate...
		if(pVehicle && !pVehicle->IsNetworkClone() && pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI && pVehicle->PopTypeIsRandom() && pVehicle->IsLawEnforcementVehicle())
		{
			for (s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
			{
				CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
				if (pPedInSeat && pPedInSeat->GetPedIntelligence()->GetOrder())
				{
					bHeliHasOrders = true;
				}
			}

			if(!bHeliHasOrders)
			{
				pVehicle->m_nVehicleFlags.bMoveAwayFromPlayer = true;
			}
		}
	}

 //	s32	ReqNumHelis, /*Allowed,*/ NumRandomHelis = 0;
 //
 //	ReqNumHelis = CGameWorld::FindLocalPlayerWanted()->NumOfHelisRequired(CGameWorld::FindLocalPlayerWanted()->GetWantedLevel());
 //
 //	#define MAXNUMHELISFOR1PLAYER (10)		// The number of police helis we can have chasing one player. (Should never reach this)
 //
 //	CHeli	*apHelis[MAXNUMHELISFOR1PLAYER];
 //	CHeli	*apHelis_Network[NUM_RAISED_HEIGHT_LEVELS_NETWORK];			// 1 for the each of the different heights
 //	s32	numHelisFound = 0;
 //	s32	totalNumActiveHelis = 0;
 //
 //	for (s32 n = 0; n < NUM_RAISED_HEIGHT_LEVELS_NETWORK; n++)
 //	{
 //		apHelis_Network[n] = NULL;
	//}

	//for (s32 n = 0; n < MAXNUMHELISFOR1PLAYER; n++)
	//{
	//	apHelis[n] = NULL;
	//}
 //
 //
 //	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
 //	CVehicle* pVehicle;
 //	s32 i=VehiclePool->GetSize();
 //
 //	while(i--)
 //	{
 //		pVehicle = VehiclePool->GetSlot(i);
 //		if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
 //		{
 //			CHeli *pHeli = (CHeli *)pVehicle;
 //
 //			if ( pVehicle->GetStatus() != STATUS_WRECKED &&
 //					(pVehicle->IsUsingPretendOccupants() || (pVehicle->GetDriver() && !pVehicle->GetDriver()->IsPlayer())))
 //			{
 //				totalNumActiveHelis++;
 //			}
 //
 //
 //			if (pHeli->GetOwnerPlayer() && pHeli->GetOwnerPlayer()->IsMyPlayer())
 //			{
 //				// If we have already found a heli we will avoid that one (don't get too close to it)
 //				if (!NetworkInterface::IsGameInProgress())
 //				{
 //					if (numHelisFound >= 1)
 //					{
 //						if (pHeli->GetHeliIntelligence()->GetHeliToAvoid() != apHelis[0])
 //						{
 //							pHeli->GetHeliIntelligence()->SetHeliToAvoid(apHelis[0]);
 //						}
 //					}
 //				}
 //				else
 //				{
 //					if (pHeli->GetOwnerPlayer())
 //					{
 //						Assert(pHeli->GetHeliIntelligence()->GetRaisedHeight() >= 0 && pHeli->GetHeliIntelligence()->GetRaisedHeight() < NUM_RAISED_HEIGHT_LEVELS_NETWORK);
 //
 //						if (apHelis_Network[pHeli->GetHeliIntelligence()->GetRaisedHeight()] == NULL)
 //						{
 //							apHelis_Network[pHeli->GetHeliIntelligence()->GetRaisedHeight()] = pHeli;
 //						}
 //						else
 //						{
 //							CHeli *pHeli1 = apHelis_Network[pHeli->GetHeliIntelligence()->GetRaisedHeight()];
 //							CHeli *pHeli2 = pHeli;
 //
 //							if (pHeli2->GetOwnerPlayer() && pHeli1->GetOwnerPlayer()->GetRlGamerId() < pHeli2->GetOwnerPlayer()->GetRlGamerId())		// Make sure it's always the same heli avoiding to other.
 //							{
 //								CHeli *pTemp = pHeli1;
 //								pHeli1 = pHeli2;
 //								pHeli2 = pTemp;
 //							}
 //							pHeli1->GetHeliIntelligence()->SetHeliToAvoid(pHeli2);
 //						}
 //					}
 //				}
 //
 //				Assert(numHelisFound < MAXNUMHELISFOR1PLAYER);
 //				if (numHelisFound < MAXNUMHELISFOR1PLAYER)
 //				{
 //					apHelis[numHelisFound] = pHeli;
 //					numHelisFound++;
 //				}
 //			}
 //		}
 //	}
 //
 //	// The script can switch off the police helis
 //	if (!bPoliceHelisAllowed) ReqNumHelis = 0;
 //
 //	if (CGameWorld::FindLocalPlayerWanted()->m_DontDispatchCopsForThisPlayer) ReqNumHelis = 0;
 // if (!NetworkInterface::IsGameInProgress()) numHelisFound = totalNumActiveHelis;

 //	CHeli	*pNewHeli = NULL;
 //
 //	if (ReqNumHelis > numHelisFound && CVehicle::GetPool()->GetNoOfFreeSpaces() > 4 && CPed::GetPool()->GetNoOfFreeSpaces() > 4 &&
 //		(!NetworkInterface::IsGameInProgress() || totalNumActiveHelis < MAX_NUM_ACTIVE_HELIS_ON_ONE_MACHINE) )
 //	{
 //			// Make sure we don't create a fresh heli right after one was destroyed.
 //		if ( (!CGameWorld::GetMainPlayerInfo()) || (CGameWorld::GetMainPlayerInfo()->m_LastTimeHeliWasDestroyed==0) || (fwTimer::GetTimeInMilliseconds() > CGameWorld::GetMainPlayerInfo()->m_LastTimeHeliWasDestroyed + 30000) )
 //		{
 //			if (CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL_HELI_2_DISPATCHED || NetworkInterface::IsGameInProgress())
 //			{
 //				if (CStreaming::HasObjectLoaded(MI_HELI_POLICE_2, CModelInfo::GetStreaming ModuleId()))
 //				{
 //					pNewHeli = CHeli::GenerateHeli(FindPlayerPed(), MI_HELI_POLICE_2);
 //				}
 //			}
 //			else
 //			{
 //				if (CStreaming::HasObjectLoaded(MI_HELI_POLICE_1, CModelInfo::GetStreaming ModuleId()))
 //				{
 //					pNewHeli = CHeli::GenerateHeli(FindPlayerPed(), MI_HELI_POLICE_1);
 //				}
 //			}
 //
 //			if (pNewHeli)
 //			{
 //				// TODO: ** THIS CODE WILL NOT WORK ANYMORE AS PLAYER INDICIES ARE NOT CONSISTENT ACROSS THE NETWORK **
 //
 //				pNewHeli->SetOwnerPlayer(NULL);
 //
 //				if (NetworkInterface::IsGameInProgress())
 //				{		// Network games: Half the helis fly at elevated height (based on player index)
 //					pNewHeli->GetHeliIntelligence()->SetRaisedHeight(0);//(s8)(localPlayer % NUM_RAISED_HEIGHT_LEVELS_NETWORK);
 //				}
 //				else
 //				{		// In single player: If there are 2 helis one of them should fly higher.
 //					if (numHelisFound >= 1 && apHelis[0] && apHelis[0]->GetHeliIntelligence()->GetRaisedHeight() == 0)
 //					{
 //						pNewHeli->GetHeliIntelligence()->SetRaisedHeight(1);
 //					}
 //
 //					if (pNewHeli->GetModelIndex() == MI_HELI_POLICE_1)
 //					{
 //						CNetworkTelemetry::EmergencySvcsCalled(MI_HELI_POLICE_1.ConvertToStreamingIndex(), CGameWorld::FindLocalPlayerCoors());
 //					}
 //					else if (pNewHeli->GetModelIndex() == MI_HELI_POLICE_2)
 //					{
 //						CNetworkTelemetry::EmergencySvcsCalled(MI_HELI_POLICE_2.ConvertToStreamingIndex(), CGameWorld::FindLocalPlayerCoors());
 //					}
 //				}
 //
 //				Assert(numHelisFound < MAXNUMHELISFOR1PLAYER);
 //				if (numHelisFound < MAXNUMHELISFOR1PLAYER)
 //				{
 //					apHelis[numHelisFound] = pNewHeli;
 //					numHelisFound++;
 //				}
 //
 //				NumRandomHelis++;
 //				g_PoliceScanner.ReportDispatch(1, AUD_UNIT_AIR, VEC3V_TO_VECTOR3(pNewHeli->GetTransform().GetPosition()));
 //			}
 //		}
 //	}
 //
 //
 //
 //
 //	// If we have more random helis than we need we tell them to go away
 //	/*Allowed = ReqNumHelis;
 //	for(s32 C=0; C<numHelisFound; C++)
 //	{
 //		if (apHelis[C])
 //		{
 //			// Make sure at least one passenger is alive
 //			bool bPassengerAlive = false;
 //			for( s32 i = Seat_backLeft; i <= Seat_backRight; i++ )
 //			{
 //				if( apHelis[C]->GetOccupierOfSeat((Seat) i) && !apHelis[C]->GetOccupierOfSeat((Seat) i)->IsInjured() )
 //				{
 //					bPassengerAlive = true;
 //				}
 //			}
 //			if(Allowed > 0 && bPassengerAlive)
 //			{
 //				Allowed--;
 //			}
 //			else
 //			{	// This one needs to go away
 //				Assert(apHelis[C]->GetVehicleType() == VEHICLE_TYPE_HELI);
 //				CVehMission *pMission = CCarIntelligence::FindUberMissionForCar(apHelis[C]);
 //				Vector3	targetCoors;
 //
 //				if (apHelis[C]->GetPosition().x > 0.0f)
 //				{
 //					targetCoors.x = WORLDLIMITS_REP_XMIN;
 //				}
 //				else
 //				{
 //					targetCoors.x = WORLDLIMITS_REP_XMAX;
 //				}
 //
 //				if (apHelis[C]->GetPosition().y > 0.0f)
 //				{
 //					targetCoors.y = WORLDLIMITS_REP_YMIN;
 //				}
 //				else
 //				{
 //					targetCoors.y = WORLDLIMITS_REP_YMAX;
 //				}
 //				targetCoors.z = 400.0f;
 //
 //				if (pMission)
 //				{
 //					pMission->m_missionType = MISSION_GOTO;
 //
 //					pMission->m_TargetCoors = targetCoors;
 //					pMission->m_FlightHeight = pMission->m_MinHeightAboveTerrain = 400;
 //				}
 //				apHelis[C]->GetHeliIntelligence()->GetMission()->m_missionType = MISSION_FLEE;
 //				apHelis[C]->GetIntelligence()->GetMission()->m_FlightHeight = apHelis[C]->GetIntelligence()->GetMission()->m_MinHeightAboveTerrain = 400;
 //				apHelis[C]->GetIntelligence()->GetMission()->m_TargetCoors = targetCoors;
 //
 //				apHelis[C]->m_nVehicleFlags.bMoveAwayFromPlayer = true;
 //				apHelis[C]->PopTypeSet(POPTYPE_RANDOM_AMBIENT);				// Heli can now be removed by population code.
 //
 //				apHelis[C]->m_ExtendedRemovalRange = 0;
 //
 //				CGameWorld::GetMainPlayerInfo()->m_LastTimeHeliWasDestroyed = fwTimer::GetTimeInMilliseconds();
 //			}
 //		}
 //	}*/
}

////////////////////////////////////////////////////////////////////////////////
// FUNCTION:  RemovePoliceHelisUponDeathArrest
// FUNCTION:  When player dies or gets arrested this function is called to remove active helis.
//			  As you can hear helis miles away it is odd for the helis to still be there
////////////////////////////////////////////////////////////////////////////////

void CHeli::RemovePoliceHelisUponDeathArrest()
{
	// Also remove the ones that are in the pool that we don't have pointers to.
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) VehiclePool->GetSize();

	while(i--)
	{
		pVehicle = VehiclePool->GetSlot(i);
        if(pVehicle)
        {
            CVehicleModelInfo* vmi = pVehicle->GetVehicleModelInfo();
            Assert(vmi);

            if(vmi->GetVehicleType() == VEHICLE_TYPE_HELI && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT))
            {
                if (!pVehicle->PopTypeIsMission())
                {
                    CVehicleFactory::GetFactory()->Destroy(pVehicle);
                }
            }
        }
	}
}

///////////////////////////////////////////////////////////////////////
//
// FUNCTION: GenerateHeli
// PURPOSE:  Generates one heli
//
///////////////////////////////////////////////////////////////////////

CHeli* CHeli::GenerateHeli(CPed* pTargetPed, u32 mi, u32 driverPedMi, bool bFadeIn)
{
	Assert(pTargetPed);

	Vector3 vTargetPedPosition;
	bool bShouldSpawnWithinLastSpottedLocation = pTargetPed->GetPlayerWanted() && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwatHeliSpawnWithinLastSpottedLocation);
	if (bShouldSpawnWithinLastSpottedLocation)
	{
		vTargetPedPosition = pTargetPed->GetPlayerWanted()->m_LastSpottedByPolice;
	}
	else
	{
		vTargetPedPosition = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition());
	}

	Vector3 Coors = CHeli::FindSpawnCoordinatesForHeli(pTargetPed);

    bool   canCreateHeli = true;
    CHeli *pNewHeli      = 0;

    if(NetworkInterface::IsGameInProgress())
    {
		const u32 numRequiredPeds    = 3;
		const u32 numRequiredCars    = 1;
		const u32 numRequiredObjects = 0;

        canCreateHeli = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);
    }

    if(canCreateHeli)
	{
		Matrix34 mat;
		mat.c = Vector3(0.0f, 0.0f, 1.0f);
		mat.a = vTargetPedPosition - Coors;
		mat.a.z = 0.0f;
		mat.a.Normalize();
		mat.b.Cross(mat.c, mat.a);
		mat.d = Coors;

		fwModelId modelId((strLocalIndex(mi)));
	    pNewHeli = (CHeli *)CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_AMBIENT, &mat);		// Was permanent vehicle but now allow helis to be removed.
    }

    if(pNewHeli)
    {
		CGameWorld::Add(pNewHeli, CGameWorld::OUTSIDE );

		pNewHeli->SetHeading(fwAngle::GetRadianAngleBetweenPoints(vTargetPedPosition.x, vTargetPedPosition.y, Coors.x, Coors.y));

	    pNewHeli->SetUpDriver(false, false, driverPedMi);

		// Don't create Passengers here, schedule them after the helicopter is created - they need access to ped group info
		// Don't create cops in the front passenger seat.		
		//pNewHeli->SetupPassenger(2, false);
		//pNewHeli->SetupPassenger(3, false);

// 	    pNewHeli->m_nHeliFlags.bUseSearchLightOnTarget = true;
//
// 	    CVehMission *pMission = CCarIntelligence::FindUberMissionForCar(pNewHeli);
//
// 		pMission->m_missionType = MISSION_POLICE_BEHAVIOUR;
// 		pMission->m_pTargetEntity = pTargetPed;
// 		pMission->m_CruiseSpeed = 70;
// 		pMission->m_FlightHeight = 40;
// 		pMission->m_MinHeightAboveTerrain = 20;
//
		// make the audio skip the startup sequence
		if(pNewHeli->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
		{
			static_cast<audHeliAudioEntity*>(pNewHeli->GetVehicleAudioEntity())->SetHeliShouldSkipStartup(true);
		}

		pNewHeli->SwitchEngineOn(true);
	    pNewHeli->m_fMainRotorSpeed = MIN_ROT_SPEED_HELI_CONTROL * 1.25f;	// Make sure we're off to a flying start.
		pNewHeli->DelayedRemovalTimeReset(30000);

		if(bFadeIn)
		{
			pNewHeli->GetLodData().SetResetDisabled(false);
			pNewHeli->GetLodData().SetResetAlpha(true);
		}
   }

	return (pNewHeli);
}

Vector3 CHeli::FindSpawnCoordinatesForHeli(const CPed* pTargetPed, bool bReturnFirstValid)
{
	float fAngle = 0.0f;
	Vector3 vTargetPedPosition;
	bool bShouldSpawnWithinLastSpottedLocation = pTargetPed->GetPlayerWanted() && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwatHeliSpawnWithinLastSpottedLocation);
	if (bShouldSpawnWithinLastSpottedLocation)
	{
		vTargetPedPosition = pTargetPed->GetPlayerWanted()->m_LastSpottedByPolice;
	}
	else
	{
		vTargetPedPosition = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition());
	}

	// Check if our target is wanted and in a plane or heli
	bool bWantedTargetInHeliOrPlane = false;
	const CVehicle* pTargetPedVehicle = pTargetPed->GetVehiclePedInside();
	if( pTargetPedVehicle && (pTargetPedVehicle->InheritsFromHeli() || pTargetPedVehicle->InheritsFromPlane()) &&
		pTargetPed->GetPlayerWanted() && pTargetPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN )
	{
		// See how fast their plane or heli is moving
		ScalarV scVehicleVelocitySq = ScalarVFromF32(pTargetPedVehicle->GetVelocity().Mag2());
		ScalarV scMinVehicleVelocitySq = ScalarVFromF32(sfMinWantedTargetInHeliOrPlaneVelocity * sfMinWantedTargetInHeliOrPlaneVelocity);
		if(IsGreaterThanAll(scVehicleVelocitySq, scMinVehicleVelocitySq))
		{
			// If moving fast enough we want to specify a range for the angle that they spawn at to be in front but off to the side a bit
			bWantedTargetInHeliOrPlane = true;
		}
	}

	// Find start coordinates for heli.
	float fRange = bWantedTargetInHeliOrPlane ? sfWantedTargetInHeliOrPlaneSpawnDistance : 250.0f;
	Vector3 Coors;

	static dev_s32 s_iMaxTries = 5;
	for(int i = 0; i < s_iMaxTries; ++i)
	{
		Coors = vTargetPedPosition;

		if(!bWantedTargetInHeliOrPlane)
		{
			fAngle = fwRandom::GetRandomNumberInRange(0.0f, TWO_PI);
		}
		else
		{
			float fOffsetAngle = fwRandom::GetRandomNumberInRange(sfMinSpawnOffsetAngleForWantedTarget, sfMaxSpawnOffsetAngleForWantedTarget);
			fOffsetAngle = fwRandom::GetRandomTrueFalse() ? fOffsetAngle : -fOffsetAngle;
			fAngle = fwAngle::LimitRadianAngleSafe(pTargetPedVehicle->GetTransform().GetHeading() + fOffsetAngle);
		}

		Coors.x -= fRange * rage::Sinf(fAngle);
		Coors.y += fRange * rage::Cosf(fAngle);

		// Off the map. We're going to have to pick a different coordinate
		if (Coors.x < WORLDLIMITS_REP_XMIN || Coors.x > WORLDLIMITS_REP_XMAX ||
			Coors.y < WORLDLIMITS_REP_YMIN || Coors.y > WORLDLIMITS_REP_YMAX )
		{
			fAngle += PI;
			Coors = vTargetPedPosition;
			Coors.x += fRange * rage::Cosf(fAngle);
			Coors.y += fRange * rage::Sinf(fAngle);
		}

		float fMinZ = (Coors.z + 40.0f);
#if __BANK
		TUNE_BOOL(IGNORE_HEIGHT_MAP_FOR_HELI_SPAWN, false);
		if (IGNORE_HEIGHT_MAP_FOR_HELI_SPAWN)
		{
			// Heli should be quite high
			Coors.z = fMinZ;

			// Do a test to make sure the heli is above the map collision.
			// This might not work as the collision might not be streamed in.
			Coors.z = rage::Max(Coors.z, WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE,Coors.x, Coors.y) + 20.0f);
		}
		else
#endif
		{
			// Heli should be quite high (above everything else)
			Coors.z = Max(fMinZ, CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(Coors.x, Coors.y));
		}

		if (camInterface::IsSphereVisibleInGameViewport(Coors, 7.5f))
		{
			continue;
		}
		else if(NetworkInterface::IsGameInProgress() && NetworkInterface::IsVisibleToAnyRemotePlayer(Coors, 7.5f, fRange))
		{
			continue;
		}

		if(!bWantedTargetInHeliOrPlane)
		{
			static dev_float s_fCloserRange = 75.0f;

			// Store off this position and calculate a closer one
			Vector3 vFarCoords = Coors;
			Coors.x -= s_fCloserRange * rage::Cosf(fAngle);
			Coors.y -= s_fCloserRange * rage::Sinf(fAngle);

#if __BANK
			if(!IGNORE_HEIGHT_MAP_FOR_HELI_SPAWN)
#endif
			{
				Coors.z = Max(fMinZ, CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(Coors.x, Coors.y));
			}

			// Check if the closer position is visible. If it is revert to the further position
			if(camInterface::IsSphereVisibleInGameViewport(Coors, 7.5f))
			{
				Coors = vFarCoords;
			}
			else if(NetworkInterface::IsGameInProgress() && NetworkInterface::IsVisibleToAnyRemotePlayer(Coors, 7.5f, s_fCloserRange))
			{
				Coors = vFarCoords;
			}
		}

		if (bReturnFirstValid)
			break;
	}

	return Coors;
}

// static bank_float s_fRopeAdjustmentZ = 0.85f;
// static bank_float s_fRopeAdjustmentX = 1.2f;
// static bank_float s_fRopeAdjustmentY = 0.85f;

//////////////////////////////////////////////////////////////////////////
// Get the position we want to attach to the rope to on the heli
//////////////////////////////////////////////////////////////////////////
void CHeli::GetRopeAttachPosition(Vector3& vAttachPos, bool bDriverSide, s32 iSeatId)
{
	// Just some sensible initial position in case bone is not found
	vAttachPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	float fAttachOffsetX = 0.0f;
	float fAttachOffsetY = 0.0f;
	float fAttachOffsetZ = 0.0f;

	eHierarchyId leftAttachBone = HELI_ROPE_ATTACH_A;
	eHierarchyId rightAttachBone = HELI_ROPE_ATTACH_B;
	Matrix34 matRopeAttach;

	bool bIsAnnihilator = MI_HELI_POLICE_2.IsValid() && GetModelIndex() == MI_HELI_POLICE_2;
	bool bIsAnnihilator2 = MI_HELI_ANNIHILATOR2.IsValid() && GetModelIndex() == MI_HELI_ANNIHILATOR2;

	// Provide some bone overrides for certain helicopters, as we can't add new bones
	if (bIsAnnihilator || bIsAnnihilator2)
	{
		Matrix34 matLocalRopeAttach;

		leftAttachBone = VEH_SIREN_2;
		rightAttachBone = VEH_SIREN_1;
		fAttachOffsetZ = -0.8f;

		if (bDriverSide)
		{
			// Rear Left
			fAttachOffsetX = -0.5f;

			if (!HasComponent(leftAttachBone))
			{
				aiAssertf(0, "Trying to create rope for the vehicle: %s when it doesn't have bone to attach it to!", GetVehicleModelInfo()->GetModelName());
				return;
			}
			GetGlobalMtx(GetBoneIndex(leftAttachBone), matRopeAttach);

			if (iSeatId == 2)
			{
				fAttachOffsetY = 0.5f;
			}

			else if (iSeatId == 4)
			{
				fAttachOffsetY = -0.3f;
			}
		}
		else
		{
			// Rear Right
			fAttachOffsetX = 0.5f;

			if (!HasComponent(rightAttachBone))
			{
				aiAssertf(0, "Trying to create rope for the vehicle: %s when it doesn't have bone to attach it to!", GetVehicleModelInfo()->GetModelName());
				return;
			}
			GetGlobalMtx(GetBoneIndex(rightAttachBone), matRopeAttach);
			
			if (iSeatId == 3)
			{
				fAttachOffsetY = 0.5f;
			}

			else if (iSeatId == 5)
			{
				fAttachOffsetY = -0.3f;
			}
		}

		Vector3 vLocalRopeAttachPosition = VEC3_ZERO;
		if (bIsAnnihilator)
			vLocalRopeAttachPosition = Vector3(0.0f, fAttachOffsetY, fAttachOffsetZ);
		else 
			vLocalRopeAttachPosition = Vector3(fAttachOffsetX, fAttachOffsetY, fAttachOffsetZ);
	
		matRopeAttach.Transform(vLocalRopeAttachPosition, vAttachPos);
	}

	//Find rappel attach location for the Buzzards
	else if ((MI_HELI_BUZZARD.IsValid() && GetModelIndex() == MI_HELI_BUZZARD) || (MI_HELI_BUZZARD2.IsValid() && GetModelIndex() == MI_HELI_BUZZARD2))
	{
		Matrix34 matLocalRopeAttach;
		if (bDriverSide)
		{
			// BUZZARD LEFT
			leftAttachBone = VEH_ENGINE;
			fAttachOffsetX = -0.5f;
			fAttachOffsetZ = -0.2f;

			if (!HasComponent(leftAttachBone))
			{
				aiAssertf(0, "Trying to create rope for the vehicle: %s when it doesn't have bone to attach it to!", GetVehicleModelInfo()->GetModelName());
				return;
			}

			GetGlobalMtx(GetBoneIndex(leftAttachBone), matRopeAttach);
		}
		else
		{
			// BUZZARD RIGHT
			rightAttachBone = VEH_ENGINE;
			fAttachOffsetX = 0.5f;
			fAttachOffsetZ = -0.2f;

			if (!HasComponent(rightAttachBone))
			{
				aiAssertf(0, "Trying to create rope for the vehicle: %s when it doesn't have bone to attach it to!", GetVehicleModelInfo()->GetModelName());
				return;
			}

			GetGlobalMtx(GetBoneIndex(rightAttachBone), matRopeAttach);
		}

		Vector3 vLocalRopeAttachPosition = Vector3(fAttachOffsetX, 0.0f, fAttachOffsetZ);
		matRopeAttach.Transform(vLocalRopeAttachPosition, vAttachPos);
	}
	else
	{
		if (bDriverSide)
		{
			if (!HasComponent(leftAttachBone))
			{
				aiAssertf(0, "Trying to create rope for the vehicle: %s when it doesn't have bone to attach it to!", GetVehicleModelInfo()->GetModelName());
				return;
			}

			GetGlobalMtx(GetBoneIndex(leftAttachBone), matRopeAttach);
		}
		else
		{
			if (!HasComponent(rightAttachBone))
			{
				aiAssertf(0, "Trying to create rope for the vehicle: %s when it doesn't have bone to attach it to!", GetVehicleModelInfo()->GetModelName());
				return;
			}

			GetGlobalMtx(GetBoneIndex(rightAttachBone), matRopeAttach);
		}

		vAttachPos = matRopeAttach.d;
		vAttachPos.z += fAttachOffsetZ;
	}
}

#define		ROPE_WEIGHT_SCALE		8.0f			// technically is time scale , not weight

//////////////////////////////////////////////////////////////////////////
// PURPOSE : Creates a rope for the desired side (if it doesn't exist already)
//////////////////////////////////////////////////////////////////////////
ropeInstance* CHeli::CreateAndAttachRope(bool bDriverSide, s32 iSeatId, float fLength, float fMinLength, float fMaxLength, float fLengthChangeRate, int ropeType, int numSections, bool lockFromFront)
{
	// Get the matrix of the proper bone.
	Vector3 vAttachPosition;
	GetRopeAttachPosition(vAttachPosition, bDriverSide, iSeatId);

	eRopeID eSeatRopeID = GetRopeIdFromSeat(bDriverSide, iSeatId);
	ropeInstance* pRopeForSeat = GetRope(eSeatRopeID);

	// Either get the existing rope or create a new one
	ropeInstance* pNewRope = NULL;
	if(pRopeForSeat)
	{
		pNewRope = pRopeForSeat;
	}
	else
	{
		Vec3V rot(0.0f, 0.0f, -1.0f);
		ropeManager* pRopeManager = CPhysics::GetRopeManager();
		Assert( pRopeManager );
		pNewRope = pRopeManager->AddRope(VECTOR3_TO_VEC3V(vAttachPosition), rot, fLength, fMinLength, fMaxLength, fLengthChangeRate, ropeType, numSections, true, lockFromFront, ROPE_WEIGHT_SCALE, false, true );

		Assert( pNewRope );
		if( !pNewRope )
			return NULL;
		pNewRope->SetNewUniqueId();
		pNewRope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );

		// Set the proper rope
		Assertf( !m_Ropes[eSeatRopeID], "Stomping memory! There is already rope created for seat id %d", (int)eSeatRopeID);
		m_Ropes[eSeatRopeID]=pNewRope;

#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAddRope>(
				CPacketAddRope(pNewRope->GetUniqueID(), VECTOR3_TO_VEC3V(vAttachPosition), rot, fLength, fMinLength, fMaxLength, fLengthChangeRate, ropeType, numSections, true, lockFromFront, ROPE_WEIGHT_SCALE, false, true),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pNewRope),
				NULL, 
				true);
#endif
	}

	// Attach to the heli if not already attached
	if(!pNewRope->IsAttached(GetCurrentPhysicsInst()))
	{
		pNewRope->AttachToObject(VECTOR3_TO_VEC3V(vAttachPosition), GetCurrentPhysicsInst(), 0, NULL, NULL);
		
#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAttachRopeToEntity>(
				CPacketAttachRopeToEntity(pNewRope->GetUniqueID(), VECTOR3_TO_VEC3V(vAttachPosition), 0, eSeatRopeID, pNewRope->GetLocalOffset() ),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pNewRope),
				this,
				false);
#endif
	}

#if __ASSERT
	strLocalIndex txdSlot = g_TxdStore.FindSlot(ropeDataManager::GetRopeTxdName());
	if (txdSlot != -1)
	{
		Assertf(CStreaming::HasObjectLoaded(txdSlot, g_TxdStore.GetStreamingModuleId()), "Rope txd not streamed in for vehicle %s", GetModelName());
	}
#endif // __ASSERT

	return pNewRope;
}

ropeAttachment* CHeli::AttachObjectToRope(bool bDriverSide, s32 iSeatId, CEntity* pAttachToEntity, float fEntityMoveSpeed)
{
	eRopeID eSeatRopeID = GetRopeIdFromSeat(bDriverSide, iSeatId);
	ropeInstance* pRope = GetRope(eSeatRopeID);

	if(!pRope)
	{
		return NULL;
	}

	Vector3 vAttachPosition;
	GetRopeAttachPosition(vAttachPosition, bDriverSide, iSeatId);

	static bank_float fAttachDist = 1.5f;

#if GTA_REPLAY
		CReplayMgr::RecordPersistantFx<CPacketAttachObjectsToRopeArray>(
			CPacketAttachObjectsToRopeArray((int)pRope->GetUniqueID(), VECTOR3_TO_VEC3V(vAttachPosition),  pAttachToEntity->GetTransform().GetPosition(), fAttachDist, fEntityMoveSpeed),
			CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
			this,
			pAttachToEntity,
			false);
#endif

	return &pRope->AttachObjectsToConstraintArray(VECTOR3_TO_VEC3V(vAttachPosition), pAttachToEntity->GetTransform().GetPosition(), GetCurrentPhysicsInst(), pAttachToEntity->GetCurrentPhysicsInst(), 0, 0, fAttachDist, fEntityMoveSpeed);
}

bool CHeli::IsRopeAttachedToNonHeliEntity(const ropeInstance* pRopeInstance) const
{
	if(pRopeInstance)
	{
		phInst* pHeliInst = GetCurrentPhysicsInst();
		if(pRopeInstance->HasAttachmentsInArray())
		{
			return true;
		}
		else if(pRopeInstance->GetInstanceA() && pRopeInstance->GetInstanceB())
		{
			return (pRopeInstance->GetInstanceA() != pHeliInst || pRopeInstance->GetInstanceB() != pHeliInst);
		}
		else if(pRopeInstance->GetAttachedTo())
		{
			return pRopeInstance->GetAttachedTo() != pHeliInst;
		}
	}

	return false;
}

bool CHeli::AreRopesAttachedToNonHeliEntity() const
{
	for(int i = 0; i < eNumRopeIds; ++i)
	{
		if(IsRopeAttachedToNonHeliEntity(m_Ropes[i]))
		{
			return true;
		}
	}

	return false;
}

bool CHeli::CanEntitiesAttachToRope(bool bDriverSide, s32 iSeatId)
{
	eRopeID eSeatRopeID = GetRopeIdFromSeat(bDriverSide, iSeatId);
	ropeInstance* pRopeForSeat = GetRope(eSeatRopeID);
	return pRopeForSeat && !pRopeForSeat->GetIsUnwindingFront();
}

void CHeli::AttachAntiSwayToEntity(CPhysical* pEntity, float fSpringDistance)
{
    m_pAntiSwayEntity = pEntity;
    m_AntiSwaySpringDistance = fSpringDistance;
}

CVehicleGadgetPickUpRope *CHeli::AddPickupRope()
{
    s32 iMountAAttachHelperBoneIndex = GetBoneIndex(VEH_TOW_MOUNT_A);
	s32 iMountBAttachHelperBoneIndex = GetBoneIndex(VEH_TOW_MOUNT_B);
	if(iMountAAttachHelperBoneIndex > -1 && iMountBAttachHelperBoneIndex > -1)
	{
		//Make sure the bones are valid
		Assert(GetSkeleton() && iMountAAttachHelperBoneIndex < GetSkeleton()->GetBoneCount() && iMountBAttachHelperBoneIndex < GetSkeleton()->GetBoneCount());
			
		CVehicleGadgetPickUpRope* pPickUpRope = NULL;

		switch(m_nPickupRopeType)
		{
		case PICKUP_HOOK:
			pPickUpRope = rage_new CVehicleGadgetPickUpRopeWithHook(iMountAAttachHelperBoneIndex, iMountBAttachHelperBoneIndex);
			break;
		case PICKUP_MAGNET:		
			pPickUpRope = rage_new CVehicleGadgetPickUpRopeWithMagnet(iMountAAttachHelperBoneIndex, iMountBAttachHelperBoneIndex);
			((CVehicleGadgetPickUpRopeWithMagnet*)pPickUpRope)->InitAudio(this);
			break;
		}		

		pPickUpRope->Init(this);

		AddVehicleGadget(pPickUpRope);
		static_cast<audHeliAudioEntity*>(GetVehicleAudioEntity())->PlayHookDeploySound();

		if( m_InitialRopeLength > 0.0f )
		{
			pPickUpRope->SetRopeLength(m_InitialRopeLength, m_InitialRopeLength, false);
		}
		return pPickUpRope;
	}
	return NULL;
}

void CHeli::RemoveRopesAndHook()
{
	for(int i = 0; i < GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = GetVehicleGadget(i);

		if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
		{
			CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
			pPickUpRope->DeleteRopesPropAndConstraints(this);
			return;
		}
	}
}

bool CHeli::GetIsCargobob() const
{
	if(GetModelIndex() == MI_HELI_CARGOBOB || GetModelIndex() == MI_HELI_CARGOBOB2 || GetModelIndex() == MI_HELI_CARGOBOB3 || GetModelIndex() == MI_HELI_CARGOBOB4 || GetModelIndex() == MI_HELI_CARGOBOB5)
	{
		return true;
	}
	return false;
}

bool CHeli::GetWheelDeformation(const CWheel *pWheel, Vector3 &vDeformation) const
{
	return m_bHasLandingGear && m_landingGear.GetWheelDeformation( this, pWheel, vDeformation );
}

bool CHeli::DoesBulletHitPropellerBound(int iComponent) const
{
	bool bIsDiscBound;
	int iPropellerIndex;
	return DoesBulletHitPropellerBound(iComponent, bIsDiscBound, iPropellerIndex);
}

bool CHeli::DoesBulletHitPropellerBound(int iComponent, bool &bHitDiscBound, int &iPropellerIndex) const
{
	for(int i =0; i < m_iNumPropellers; i++)
	{	
		if(m_propellerCollisions[i].GetFragDisc() == iComponent)
		{
			bHitDiscBound = true;
			iPropellerIndex = i;
			return true;
		}
		else if(m_propellerCollisions[i].GetFragGroup() > -1)
		{
			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[m_propellerCollisions[i].GetFragGroup()];
			for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
			{
				if(iComponent == m_propellerCollisions[i].GetFragChild() + iChild)
				{
					bHitDiscBound = false;
					iPropellerIndex = i;
					return true;
				}
			}
		}
	}
	return false;
}

static dev_float ms_fBlockBulletSpeedRatioThreshold = 0.1f;

bool CHeli::DoesBulletHitPropeller(int iEntryComponent, const Vector3 &UNUSED_PARAM(vEntryPos), int iExitComponent, const Vector3 &UNUSED_PARAM(vExitPos)) const
{
	if(iEntryComponent != iExitComponent)
	{
		return false;
	}
	
	bool bHitDiscBound;
	int iPropellerIndex;

	if(DoesBulletHitPropellerBound(iEntryComponent, bHitDiscBound, iPropellerIndex))
	{
		float fSpeedRatio = m_propellers[iPropellerIndex].GetSpeed()/ms_fRotorSpeedMults[iPropellerIndex];
		if(bHitDiscBound)
		{
			return iPropellerIndex != Rotor_Main // Don't let bullet hit the main rotor disc bound, B*1450124
				&& fSpeedRatio > ms_fBlockBulletSpeedRatioThreshold
				&& fSpeedRatio > fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		}
		else
		{
			return fSpeedRatio <= ms_fBlockBulletSpeedRatioThreshold;
		}
	}

	return false;
}

bool CHeli::DoesProjectileHitPropeller(int iComponent) const
{
	bool bHitDiscBound;
	int iPropellerIndex;

	if(DoesBulletHitPropellerBound(iComponent, bHitDiscBound, iPropellerIndex))
	{
		float fSpeedRatio = m_propellers[iPropellerIndex].GetSpeed()/ms_fRotorSpeedMults[iPropellerIndex];
		if(bHitDiscBound)
		{
			return fSpeedRatio > ms_fBlockBulletSpeedRatioThreshold
				&& fSpeedRatio > fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		}
		else
		{
			return fSpeedRatio <= ms_fBlockBulletSpeedRatioThreshold;
		}
	}

	return false;
}

bool CHeli::NeedUpdatePropellerBound(int iPropellerIndex) const
{
	float fSpeedRatio = m_propellers[iPropellerIndex].GetSpeed()/ms_fRotorSpeedMults[iPropellerIndex];
	return fSpeedRatio <= ms_fBlockBulletSpeedRatioThreshold;
}

///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SwitchPoliceHelis
// PURPOSE :  For the script to decide whether the police helis are allowed.
///////////////////////////////////////////////////////////////////////////////////

void CHeli::SwitchPoliceHelis(bool bSwitch)
{
	bPoliceHelisAllowed = bSwitch;
}

bank_float bfWindyWindMultForHeli = 2.0f;
bank_float bfRainingWindMultForHeli = 1.5f;
bank_float bfSnowingWindMultForHeli = 1.5f;
bank_float bfSandStormWindMultForHeli = 1.5f;
float CHeli::ComputeWindMult()
{
	// Disable wind effect when heli is landed
	if(!m_bIsInAir)
	{
		return 0.0f;
	}

	float fMult = 1.0f;
	if(pHandling && pHandling->GetFlyingHandlingData())
	{
		fMult *= pHandling->GetFlyingHandlingData()->m_fWindMult;
	}
	if(g_weather.IsWindy())
	{
		fMult *= bfWindyWindMultForHeli;
	}
	if(g_weather.IsRaining())
	{
		fMult *= bfRainingWindMultForHeli;
	}
	if(g_weather.IsSnowing())
	{
		fMult *= bfSnowingWindMultForHeli;
	}
	if(g_weather.GetSandstorm() > 0.2f)
	{
		fMult *= bfSandStormWindMultForHeli;
	}
	return fMult;
}

bank_float bfWindyTurbulenceMultForHeli = 0.2f;
bank_float bfRainingTurbulenceMultForHeli = 0.1f;
bank_float bfSnowingTurbulenceMultForHeli = 0.1f;
bank_float bfSandStormTurbulenceMultForHeli = 0.1f;
float CHeli::WeatherTurbulenceMult()
{
	float fMult = 0.0f;
	if(g_weather.IsWindy())
	{
		fMult += bfWindyTurbulenceMultForHeli;
	}
	if(g_weather.IsRaining())
	{
		fMult += bfRainingTurbulenceMultForHeli;
	}
	if(g_weather.IsSnowing())
	{
		fMult += bfSnowingTurbulenceMultForHeli;
	}
	if(g_weather.GetSandstorm() > 0.2f)
	{
		fMult += bfSandStormTurbulenceMultForHeli;
	}
	return fMult;
}


#if __BANK
void CHeli::InitRotorWidgets(bkBank& bank)
{
	const char* strPropNames[CRotaryWingAircraft::Num_Heli_Rotors] = 
	{
		"Main",
		"Rear"
	};
	CompileTimeAssert(CRotaryWingAircraft::Num_Heli_Rotors == 2);

	bank.PushGroup("Heli rotors");
	for(int i =0; i < Num_Heli_Rotors; i++)
	{
		bank.AddSlider(strPropNames[i],&ms_fRotorSpeedMults[i],0.0f,100.0f,0.01f);
	}
	bank.PopGroup();

}

void CHeli::InitTurbulenceWidgets(bkBank& bank)
{
	bank.PushGroup("Heli turbulence");
		bank.AddSlider("Turbulence roll noise lower limit",&sfRandomRollNoiseLower,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence roll noise upper limit",&sfRandomRollNoiseUpper,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence pitch noise lower limit",&sfRandomPitchNoiseLower,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence pitch noise upper limit",&sfRandomPitchNoiseUpper,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence drop noise lower limit",&sfRandomDropNoiseLower,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence drop noise upper limit",&sfRandomDropNoiseUpper,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence noise speed limit",&sfRandomNoiseSpeedMax,0.0f,100.0f,1.0f);
		bank.AddSlider("Turbulence idle multiplier for player",&sfRandomNoiseIdleMultForPlayer,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence throttle multiplier for player",&sfRandomNoiseThrottleMultForPlayer,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence speed multiplier for player",&sfRandomNoiseSpeedMultForPlayer,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence idle multiplier for AI",&sfRandomNoiseIdleMultForAI,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence throttle multiplier for AI",&sfRandomNoiseThrottleMultForAI,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence speed multiplier for AI",&sfRandomNoiseSpeedMultForAI,-10.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence min during",&sfTurbulenceTimeMin,0.0f,20.0f,0.01f);
		bank.AddSlider("Turbulence max during",&sfTurbulenceTimeMax,0.0f,20.0f,0.01f);
		bank.AddSlider("Turbulence min recover time",&sfTurbulenceRecoverTimeMin,0.0f,20.0f,0.01f);
		bank.AddSlider("Turbulence max recover time",&sfTurbulenceRecoverTimeMax,0.0f,20.0f,0.01f);
		bank.AddSlider("Wind multiplier in windy condition",&bfWindyWindMultForHeli,0.0f,10.0f,0.01f);
		bank.AddSlider("Wind multiplier in raining condition",&bfRainingWindMultForHeli,0.0f,10.0f,0.01f);
		bank.AddSlider("Wind multiplier in snow condition",&bfSnowingWindMultForHeli,0.0f,10.0f,0.01f);
		bank.AddSlider("Wind multiplier in sand storm condition",&bfSandStormWindMultForHeli,0.0f,10.0f,0.01f);
		bank.AddSlider("Turbulence multiplier in windy condition",&bfWindyTurbulenceMultForHeli,0.0f,1.0f,0.01f);
		bank.AddSlider("Turbulence multiplier in raining condition",&bfRainingTurbulenceMultForHeli,0.0f,1.0f,0.01f);
		bank.AddSlider("Turbulence multiplier in snow condition",&bfSnowingTurbulenceMultForHeli,0.0f,1.0f,0.01f);
		bank.AddSlider("Turbulence multiplier in sand storm condition",&bfSandStormTurbulenceMultForHeli,0.0f,1.0f,0.01f);
		bank.AddSlider("Controller vibration scale when turbulence",&sfHeliPhysicalTurbulencePedVibMult,0.0f,1.0f,0.01f);
	bank.PopGroup();
}
#endif




///////////////////////////////////////////////////////////////////////////////
//  UpdateEngineSpeed
///////////////////////////////////////////////////////////////////////////////
float ms_fHeliEngineSpeedDropRateWhenMissFiring = 1.0f;
float dfHeliEngineMissFireStartingHealth = 600.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float sfHeliEngineBreakDownHealth = 200.0f; // Also referenced in CVehicleDamage::ProcessPetrolTankDamage;
float bfHeliEngineMissFireMinTime = 0.5f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfHeliEngineMissFireMaxTime = 1.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfHeliEngineMissFireMinRecoverTime = 10.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfHeliEngineMissFireMaxRecoverTime = 20.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float dfHeliEngineDegradeStartingHealth = ENGINE_DAMAGE_PLANE_DAMAGE_START;
float dfHeliEngineDegradeMinDamage = 2.0f;
float dfHeliEngineDegradeMaxDamage = 5.0f;

void CHeli::UpdateRotorSpeed()
{
	int iDriverSeatIndex = GetVehicleModelInfo()->GetModelSeatInfo()->GetDriverSeat();
	int iDriverSeatBoneIndex = GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iDriverSeatIndex);

	// Standard heli
	float fOldRotorSpeed = m_fPrevMainRotorSpeed;
	if(m_nVehicleFlags.bEngineOn && (GetStatus()==STATUS_PLAYER || GetStatus()==STATUS_PHYSICS || GetStatus()==STATUS_OUT_OF_CONTROL) && !m_Transmission.GetCurrentlyMissFiring())
	{
		m_fMainRotorSpeed += HELI_ROTOR_ANGULAR_ACCELERATION * fwTimer::GetTimeStep();
		if(m_fMainRotorSpeed > MAX_ROT_SPEED_HELI_BLADES)
			m_fMainRotorSpeed = MAX_ROT_SPEED_HELI_BLADES;
	}
	else
	{
		if(!m_nVehicleFlags.bEngineStarting)
		{
			bool bAnyAlivePassenger = false;
			for(s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); ++iSeat)
			{
				CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat);
				if(pPassenger && !pPassenger->IsInjured())
				{
					bAnyAlivePassenger = true;
					break;
				}
			}
			// only switch the engine off is no-one is on board and no-one else is about to enter the seat
			CComponentReservation* pComponentReservation = m_ComponentReservationMgr.FindComponentReservation(iDriverSeatBoneIndex, false);
			if (pComponentReservation && pComponentReservation->GetPedUsingComponent() == NULL && !IsNetworkClone() && !bAnyAlivePassenger && !IsFullThrottleActive())
			{
				SwitchEngineOff();
			}
		}

		// stop engine suddenly if fallen in water
		if(GetIsInWater() && m_fTimeInWater > 0.0f)
		{
			if(m_Buoyancy.GetSubmergedLevel() > 0.99f)
			{
				m_fMainRotorSpeed = 0.0f;
			}
			else if (!IsNetworkClone())
			{
				SwitchEngineOff();
			}
		}

		if(m_Transmission.GetCurrentlyMissFiring() && GetVehicleDamage()->GetEngineHealth() > (sfHeliEngineBreakDownHealth + dfHeliEngineDegradeMaxDamage))
		{
			m_fMainRotorSpeed -= ms_fHeliEngineSpeedDropRateWhenMissFiring * fwTimer::GetTimeStep();
		}
		else
		{
			m_fMainRotorSpeed -= HELI_ROTOR_ANGULAR_DECCELERATION * fwTimer::GetTimeStep();
		}
		if(m_fMainRotorSpeed < 0.0f)
			m_fMainRotorSpeed = 0.0f;

		float fFallSpeed = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetC()), GetVelocity());
		if(fFallSpeed < -2.0f && m_fMainRotorSpeed > 0.0f)
		{
			if(m_fMainRotorSpeed < 0.5f)
			{
				m_fMainRotorSpeed += 20.f*HELI_ROTOR_ANGULAR_DECCELERATION * fwTimer::GetTimeStep();
				if(m_fMainRotorSpeed > 0.5f)
					m_fMainRotorSpeed = 0.5f;
			}
		}
	}
	
	if(fOldRotorSpeed != m_fMainRotorSpeed && (fOldRotorSpeed == 0.0f || m_fMainRotorSpeed == 0.0f))
	{
		UpdateRotorBounds();
	}

	vehicleAssert(m_iNumPropellers <= Max_Num_Rotors);

	// Update the propellers from the rotor speed variables
	if(GetIsCargobob() || GetIsDrone() || GetIsJetPack() )
	{
		// The rotor speeds are the same for cargobob
		for(int i =0; i < m_iNumPropellers; i++)
		{	
			float fRotorDirection = (i == Rotor_Rear || i == Rotor_Main2) ? -1.0f : 1.0f;
			float fRotorSpeedMult = ( GetIsDrone() ) ? ms_fRotorSpeedMults[ Rotor_Rear2 ] : ms_fRotorSpeedMults[ Rotor_Main ];

			m_propellers[i].UpdatePropeller(fRotorDirection * m_fMainRotorSpeed * fRotorSpeedMult,fwTimer::GetTimeStep());
		}
	}
	else
	{
		for(int i = 0; i < m_iNumPropellers; i++)
		{	
			float fRotorSpeed = m_fMainRotorSpeed;

			if ((i == Rotor_Rear) && GetIsTailBoomBroken())
			{
				fRotorSpeed = 0.0f;
			}

			m_propellers[i].UpdatePropeller(fRotorSpeed*ms_fRotorSpeedMults[i],fwTimer::GetTimeStep());
		}
	}

	// calc the ground position intersection of the heli (directly down in the -z direction)
	if (m_fMainRotorSpeed>0.3f && m_fMainRotorHealth>0.0f)
	{
		g_vfxVehicle.ProcessHeliDownwash(this);
	}

	// B*1991050: Now storing the last known rotor speed explcitly so we can detect changes to rotor speed across complete frames rather than just changes that were done inside this method.
	// This ensures the heli rotor disc bounds are correctly updated even if the rotor speed is changed directly outside of this method (e.g. Police heli generation or via script).
	m_fPrevMainRotorSpeed = m_fMainRotorSpeed;
}


void CHeli::SetMainRotorSpeed(float fMainRotorSpeed)
{
	CRotaryWingAircraft::SetMainRotorSpeed(fMainRotorSpeed);

	// Update the propellers from the rotor speed variables
	if(GetIsCargobob() || GetIsDrone() || GetIsJetPack() )
	{
		// The rotor speeds are the same for cargobob
		for(int i =0; i < m_iNumPropellers; i++)
		{	
			float fRotorDirection = (i == Rotor_Rear || i == Rotor_Main2) ? -1.0f : 1.0f;
			m_propellers[i].SetPropellerSpeed(fRotorDirection * m_fMainRotorSpeed * ms_fRotorSpeedMults[0]);
		}
	}
	else
	{
		for(int i =0; i < m_iNumPropellers; i++)
		{	
			m_propellers[i].SetPropellerSpeed(m_fMainRotorSpeed*ms_fRotorSpeedMults[i]);
		}
	}
}

void CHeli::PreRender2( const bool bIsVisibleInMainViewport ) 
{
#if GTA_REPLAY
	if (!CReplayMgr::IsEditModeActive())
#endif
	{ 
		if( m_bHasLandingGear )
		{
			m_landingGear.PreRenderDoors( this );
		}
	}
	CRotaryWingAircraft::PreRender2( bIsVisibleInMainViewport );

	// jetpack vfx
	if (GetIsJetPack())
	{
		if (!IsDummy() && !m_nVehicleFlags.bIsDrowning && 
			!m_nVehicleFlags.bDisableParticles)
		{
			if ((m_nVehicleFlags.bEngineOn || IsRunningCarRecording()))
			{
				s32 airBrakeBoneIndex = GetBoneIndex(HELI_AIRBRAKE_L);
				if (airBrakeBoneIndex>-1)
				{
					float vfxScale = Clamp( m_fJetpackStrafeForceScale, 0.0f, 1.0f );
					g_vfxVehicle.UpdatePtFxThrusterJet(this, airBrakeBoneIndex, 0, vfxScale);
				}

				airBrakeBoneIndex = GetBoneIndex(HELI_AIRBRAKE_R);
				if (airBrakeBoneIndex>-1)
				{
					float vfxScale = Clamp( -m_fJetpackStrafeForceScale, 0.0f, 1.0f );
					g_vfxVehicle.UpdatePtFxThrusterJet(this, airBrakeBoneIndex, 1, vfxScale);
				}
			}
		}

        if( GetIsJetPack() &&
            GetSkeleton() &&
            !IsAnimated() )
        {
            if( !GetDriver() )
            {
                GetLandingGear().ControlLandingGear( this, CLandingGear::COMMAND_DEPLOY );
            }

            static dev_float sfMaxSecondaryGearRotationAmount = DtoR * -95.0f;
            float rotationAmount = ( 1.0f -  GetLandingGear().GetGearDeployRatio() ) * sfMaxSecondaryGearRotationAmount;

            int boneIndex = GetBoneIndex( VEH_MISC_B );
            SetBoneRotation( boneIndex, ROT_AXIS_LOCAL_X, rotationAmount );
            GetSkeleton()->PartialUpdate( boneIndex );
        }
	}
}

void CHeli::ApplyDeformationToBones( const void* basePtr )
{
	CRotaryWingAircraft::ApplyDeformationToBones( basePtr );

	if( m_bHasLandingGear )
	{
		m_landingGear.ApplyDeformation( this, basePtr );
	}
}

void CHeli::Fix( bool resetFrag, bool allowNetwork )
{
	CRotaryWingAircraft::Fix( resetFrag, allowNetwork );

	if( m_bHasLandingGear )
	{
		m_landingGear.Fix( this );
	}
}

//////////////////////////////////////////////
// Update our ropes and wind them if needed //
//////////////////////////////////////////////

void CHeli::UpdateRopes()
{
#if GTA_REPLAY 
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

	const CSeatManager* pSeatManager = GetSeatManager();
	const bool bIsAnnihilator = (MI_HELI_POLICE_2.IsValid() && GetModelIndex() == MI_HELI_POLICE_2) || (MI_HELI_ANNIHILATOR2.IsValid() && GetModelIndex() == MI_HELI_ANNIHILATOR2);
	bool bAllRearSeatsFree = !pSeatManager->GetPedInSeat(Seat_backLeft) && !pSeatManager->GetPedInSeat(Seat_backRight);
	if (bIsAnnihilator)
	{
		bAllRearSeatsFree &= pSeatManager->GetPedInSeat(4) == nullptr && pSeatManager->GetPedInSeat(5) == nullptr;
	}

	// If we don't have any peds in our heli anymore (excluding the driver) and no-one attached to our ropes then wind them
	if(bAllRearSeatsFree)
	{
		// Don't start winding the ropes if we still have peds attached to the heli directly
		CPed* pDriver = GetDriver();
		if(pDriver)
		{
			CPedGroup* pMyGroup = pDriver->GetPedsGroup();
			const CPedGroupMembership* pGroupMembership = pMyGroup ? pMyGroup->GetGroupMembership() : NULL;
			if(pGroupMembership)
			{
				for (int i = 0; i < CPedGroupMembership::MAX_NUM_MEMBERS; i++)
				{
					const CPed* pPedMember = pGroupMembership->GetMember(i);
					if (pPedMember && pPedMember != pDriver && pPedMember->GetAttachParent() == this)
					{
						return;
					}
				}
			}
		}

		for(int i = 0; i < eNumRopeIds; ++i)
		{
			if(m_Ropes[i] && !IsRopeAttachedToNonHeliEntity(m_Ropes[i]) && !HasScheduledOccupants())
			{
				s32 sSeatId = i+2; //skip front seats - we start from 1st passenger.

				CPed* pLastPedInSeat = pSeatManager->GetLastPedInSeat(sSeatId);
				bool bBackSeatFree = !pLastPedInSeat || !pLastPedInSeat->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling) || pLastPedInSeat->GetMyVehicle() != this;

				bool bSeatFree = bBackSeatFree;
				if (bIsAnnihilator)
				{
					CPed* pPedInExtraRearSeat = pSeatManager->GetLastPedInSeat(sSeatId+2);
					bool bExtraBackSeatFree = !pPedInExtraRearSeat || !pPedInExtraRearSeat->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling) || pPedInExtraRearSeat->GetMyVehicle() != this;
					bSeatFree &= bExtraBackSeatFree;
				}

				if(bSeatFree)
				{
					m_Ropes[i]->StopUnwindingFront();
					m_Ropes[i]->StartWindingFront(7.0f);

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						ropeInstance* r = m_Ropes[i];
						CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
							CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
							CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r),
							NULL, 
							false);
					}
#endif
				}
			}
		}
	}
	else
	{
		for(int i = 0; i < eNumRopeIds; ++i)
		{
			if(m_Ropes[i] && m_Ropes[i]->GetIsUnwindingFront() && m_Ropes[i]->GetLength() >= m_Ropes[i]->GetMaxLength())
			{
				m_Ropes[i]->StopUnwindingFront();
#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					ropeInstance* r = m_Ropes[i];
					CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
						CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
						CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r),
						NULL, 
						false);
				}
#endif
			}
		}
	}
}

void CHeli::ProcessLanded(float fTimeStep)
{
	//Ensure the heli is not in the air.
	if(!HasContactWheels())
	{
		//Clear the time the heli has spent landed.
		m_fTimeSpentLanded = 0.0f;

		if( GetFrameCollisionHistory()->GetMostRecentCollisionTime() <= fwTimer::GetTimeInMilliseconds() &&
			GetFrameCollisionHistory()->GetMostRecentCollisionTime() > (fwTimer::GetTimeInMilliseconds() - 100) )
		{
			m_fTimeSpentCollidingNotLanded += fTimeStep;
		}
		else
		{
			m_fTimeSpentCollidingNotLanded = 0.0f;
		}
	}
	else
	{
		//Increase the time the heli has spent landed.
		m_fTimeSpentLanded += fTimeStep;
	}
}

dev_float sfHeliDamageStrongMult = 0.4f;
dev_float sfHeliCrashingMult = 10.0f;
dev_float sfHeliDamageNoDriverMult = 3.0f;
dev_float dfHeliDamageUpsideDownMult = 10.0f;

dev_float sfMainRotorDamagePerSec = 10.0f;
dev_float sfMainRotorDamageFromImpactMag = 10.0f;

dev_float sfRearRotorDamagePerSec = 60.0f;
dev_float sfRearRotorDamageFromImpactMag = 60.0f;

dev_float sfTailDamageFromImpact = 12.0f;
// Use collision information to damage rotors
void CHeli::ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const * hitInst, const Vector3& vMyHitPos, const Vector3& vOtherHitPos,
							 float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
							 phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact)
{
	fragInstGta* pFragInst = GetVehicleFragInst();
	vehicleAssert(pFragInst);

	float fDamageMult = 1.0f;
	if(m_fMainRotorHealth <= 0.0f || m_fRearRotorHealth <= 0.0f || (m_nTailBoomGroup > -1 && pFragInst->GetGroupBroken(m_nTailBoomGroup)))
		fDamageMult = sfHeliCrashingMult;
	else if(m_nPhysicalFlags.bNotDamagedByCollisions)
		fDamageMult = 0.0f;
	else if(m_nVehicleFlags.bTakeLessDamage)
		fDamageMult = sfHeliDamageStrongMult;
	else if(!GetDriver())
		fDamageMult = sfHeliDamageNoDriverMult;
	else if(IsUpsideDown())
		fDamageMult = dfHeliDamageUpsideDownMult;

	bool bPassDamageToTail = false;
	bool bDamageDone = false;
	bool bNetworkClone = IsNetworkClone();


	// check for main rotor impacts
	int nMainRotorChild = m_propellerCollisions[Rotor_Main].GetFragChild();
	int nRearRotorChild = m_propellerCollisions[Rotor_Rear].GetFragChild();
	if(GetIsVisible() && iMyComponent==nMainRotorChild && m_fMainRotorSpeed > 0.0f && !pFragInst->GetChildBroken(nMainRotorChild))
	{
		if (pHitEnt)// && CPhysics::GetIsLastTimeSlice(CPhysics::GetCurrentTimeSlice()))// && (fwTimer::GetSystemFrameCount()%4)==0)
		{
			// do collision effects
			Vector3 collPos = vMyHitPos;
			Vector3 collNormal = vMyNormal;
			//Vector3 collNormalNeg = -vMyNormal;
			//		Vector3 collVel = vecImpulse;
			Vector3 collDir(0.0f, 0.0f, 0.0f);

			Vector3 collPtToVehCentre = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) - collPos;
			collPtToVehCentre.Normalize();
			Vector3 collVel = CrossProduct(collPtToVehCentre, VEC3V_TO_VECTOR3(GetTransform().GetC()));
			collVel.Normalize();

			float scrapeMag = 100000.0f;
			float accumImpulse = 100000.0f;

			g_vfxMaterial.DoMtlScrapeFx(this, 0, pHitEnt, 0, RCC_VEC3V(collPos), RCC_VEC3V(collNormal), RCC_VEC3V(collVel), PGTAMATERIALMGR->g_idCarMetal, iOtherMaterial, RCC_VEC3V(collDir), scrapeMag, accumImpulse, VFXMATERIAL_LOD_RANGE_SCALE_HELI, 1.0f, 1.0);
			g_vfxMaterial.DoMtlScrapeFx(pHitEnt, 0, this, 0, RCC_VEC3V(collPos), RCC_VEC3V(collNormal), RCC_VEC3V(collVel), iOtherMaterial, PGTAMATERIALMGR->g_idCarMetal, RCC_VEC3V(collDir), scrapeMag, accumImpulse, VFXMATERIAL_LOD_RANGE_SCALE_HELI, 1.0f, 1.0);

			if (pHitEnt->GetIsTypePed())
			{
				g_vfxBlood.UpdatePtFxBloodMist(static_cast<CPed*>(pHitEnt));
			}

			pHitEnt->ProcessFxEntityCollision(collPos, collNormal, 0, accumImpulse);
		}
		// rotor take damage
		if(m_nVehicleFlags.bCanBeVisiblyDamaged && !bNetworkClone)
		{
			float fDamage = sfMainRotorDamagePerSec * fwTimer::GetTimeStep();
			fDamage += fImpulseMag * GetInvMass() * sfMainRotorDamageFromImpactMag;
			fDamage *= fDamageMult;

			m_fMainRotorHealth -= fDamage * m_fMainRotorHealthDamageScale;
			bDamageDone = true;

			if(m_fMainRotorHealth <= 0.0f)
			{
				BreakOffMainRotor();
			}
		}

		physicsAssert(hitInst);
		u16 nGenerationId = 0;
#if LEVELNEW_GENERATION_IDS
		if(hitInst && hitInst->IsInLevel())
		{
			nGenerationId = PHLEVEL->GetGenerationID(hitInst->GetLevelIndex());
		}
#endif // LEVELNEW_GENERATION_IDS
		WorldProbe::CShapeTestFixedResults<> tempResults;
		tempResults[0].SetHitComponent((u16)iOtherComponent);
		tempResults[0].SetHitInst(
			hitInst->GetLevelIndex(),
			nGenerationId
			);
		tempResults[0].SetHitMaterialId(iOtherMaterial);
		tempResults[0].SetHitPosition(vMyHitPos);
		g_CollisionAudioEntity.ReportHeliBladeCollision(&tempResults[0], (CVehicle*)this);
	}
	// check for tail rotor impacts
	else if(iMyComponent==nRearRotorChild && m_fMainRotorSpeed > 0.0f && !pFragInst->GetChildBroken(nRearRotorChild))
	{
		// pass impacts to the rear rotor on to the tail
		bPassDamageToTail = true;

		// do collision effects

		// rear rotor take damage
		if(m_nVehicleFlags.bCanBeVisiblyDamaged && !bNetworkClone)
		{
			float fDamage = sfRearRotorDamagePerSec * fwTimer::GetTimeStep();
			fDamage += fImpulseMag * GetInvMass() * sfRearRotorDamageFromImpactMag;
			fDamage *= fDamageMult;

			m_fRearRotorHealth -= fDamage * m_fRearRotorHealthDamageScale;
			bDamageDone = true;

			if(m_fRearRotorHealth <= 0.0f)
			{
				BreakOffRearRotor();
				StartCrashingBehavior(pHitEnt);
			}
		}
	}

	// if parent of our collision is the tail rotor then apply damage to it too
	int nParentGroup = pFragInst->GetTypePhysics()->GetAllChildren()[iMyComponent]->GetOwnerGroupPointerIndex();
	if(m_nTailBoomGroup > -1 && (nParentGroup==m_nTailBoomGroup || bPassDamageToTail) && !pFragInst->GetGroupBroken(m_nTailBoomGroup) && m_nVehicleFlags.bCanBeVisiblyDamaged && !bNetworkClone)
	{
		float fDamage = fImpulseMag * GetInvMass() * sfTailDamageFromImpact;
		fDamage *= fDamageMult;

		m_fTailBoomHealth -= fDamage * m_fTailBoomHealthDamageScale;
		bDamageDone = true;

		// once the tail's health reaches zero, let it break off if hit hard enough again
		if(m_fTailBoomHealth <= 0.0)
		{
			fragTypeGroup* pTailGroup = pFragInst->GetTypePhysics()->GetAllGroups()[m_nTailBoomGroup];
			int nFirstChild = pTailGroup->GetChildFragmentIndex();
			int nNumChildren = pTailGroup->GetNumChildren();

			// make sure this component doesn't break off until we want it to
			for(int nChild=nFirstChild; nChild < nFirstChild + nNumChildren; nChild++)
			{
				((fragInstGta*)pFragInst)->ClearDontBreakFlag(BIT(nChild));
			}
		}
	}

	// make sure overall health is less than full if any helicopter components are damaged
	if(bDamageDone && GetHealth() >= CREATED_VEHICLE_HEALTH)
	{
		if (!IsNetworkClone())
		{
			ChangeHealth(-1.0f);
		}
	}

	return CRotaryWingAircraft::ProcessCollision(myInst, pHitEnt, hitInst, vMyHitPos,vOtherHitPos,fImpulseMag,vMyNormal,iMyComponent,iOtherComponent,
		iOtherMaterial,bIsPositiveDepth,bIsNewContact);
}

dev_float dfHeliPushAttachedEntitySpeed = 1.0f;
dev_float dfHeliContactDepthToDetachCargo = 1.0f;

void CHeli::ProcessPreComputeImpacts(phContactIterator impacts)
{
	// Disable push between cargobob and its attached entity
	if(GetIsCargobob() && GetNumberOfVehicleGadgets() > 0)
	{
		const CPhysical *pAttachedEntity = NULL;
		CVehicleGadgetPickUpRope *pPickUpRope = NULL;

		for(int i = 0; i < GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);

				pAttachedEntity = pPickUpRope->GetAttachedEntity();
				break;
			}
		}

		if(pPickUpRope && pAttachedEntity)
		{
			impacts.Reset();
			while(!impacts.AtEnd())
			{
				if(!impacts.IsDisabled())
				{
					phInst* pOtherInstance = impacts.GetOtherInstance();
					CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);
					if(pOtherEntity == pAttachedEntity)
					{
						if(!impacts.IsConstraint())
						{
							if(impacts.GetDepth() > dfHeliContactDepthToDetachCargo)
							{
								if(NetworkInterface::IsGameInProgress())
								{
									if(!NetworkUtils::IsNetworkCloneOrMigrating(pAttachedEntity))
									{
										pPickUpRope->DetachEntity(this);
									}
									else if(!impacts.IsConstraint())
									{
										// send an event to the owner of the attached entity requesting they detach
										//CRequestDetachmentEvent::Trigger(*pAttachedEntity);
										impacts.DisableImpact();
									}
								}
								else if(!impacts.IsConstraint())
								{
									pPickUpRope->DetachEntity(this);
								}
							}
						}

						if(IsNetworkClone() && !impacts.IsConstraint())
						{
							impacts.SetDepth(Min(dfHeliPushAttachedEntitySpeed * fwTimer::GetTimeStep(), impacts.GetDepth()));
						}
					}
				}
				impacts++;
			}
		}
	}

	if( HasLandingGear() )
	{
		impacts.Reset();
		while(!impacts.AtEnd())
		{
			GetLandingGear().ProcessPreComputeImpacts( this, impacts );
			impacts++;
		}
	}

	static dev_float sfThresholdForPontoonImpacts = 5.0f;
	CSeaPlaneHandlingData* pSeaPlaneHandling = pHandling->GetSeaPlaneHandlingData();

	if( pSeaPlaneHandling &&
		GetVelocity().Dot( VEC3V_TO_VECTOR3( GetTransform().GetUp() ) ) < sfThresholdForPontoonImpacts )
	{
		impacts.Reset();
		while( !impacts.AtEnd() )
		{
			if( !impacts.IsDisabled() )
			{
				if( (u32)impacts.GetMyComponent() == pSeaPlaneHandling->m_fLeftPontoonComponentId ||
					(u32)impacts.GetMyComponent() == pSeaPlaneHandling->m_fRightPontoonComponentId )
				{
					Vec3V vecNorm;
					impacts.GetMyNormal( vecNorm );

					float normalDotUp = Dot( vecNorm, GetTransform().GetC() ).Getf();
					if( normalDotUp > 0.6f )
					{
						impacts.DisableImpact();
						impacts++;
						continue;
					}
				}
			}
			impacts++;
		}
	}

	bool isValkyrie = ( MI_HELI_VALKYRIE.IsValid() && 
						( GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_VALKYRIE.GetName().GetHash() ) ) ||
					  ( MI_HELI_VALKYRIE2.IsValid() && 
						( GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_VALKYRIE2.GetName().GetHash() ) );

	if( IsNetworkClone() &&
		isValkyrie )
	{
		impacts.Reset();
		while(!impacts.AtEnd())
		{
			if(!impacts.IsDisabled())
			{
				phInst* pOtherInstance	= impacts.GetOtherInstance();
				CEntity* pOtherEntity	= CPhysics::GetEntityFromInst( pOtherInstance );

				if( pOtherEntity &&
					pOtherEntity->GetIsTypePed() )
				{
					int myComponent = impacts.GetMyComponent();

					// Hack to fix GTAV - B*1978367 - If a ped hits one of the side weapons on a network clone
					// Valkyrie don't move the weapons as they get forced back into position and cause the ped to be
					// launched.
					int valkyrieWeapon2Component = GetBoneIndex( VEH_WEAPON_2A );
					int valkyrieWeapon3Component = GetBoneIndex( VEH_WEAPON_3A );

					if( valkyrieWeapon2Component > -1 )
					{
						valkyrieWeapon2Component = GetVehicleFragInst()->GetComponentFromBoneIndex( valkyrieWeapon2Component );
					}
					if( valkyrieWeapon3Component > -1 )
					{
						valkyrieWeapon3Component = GetVehicleFragInst()->GetComponentFromBoneIndex( valkyrieWeapon3Component );
					}

					if( myComponent == valkyrieWeapon2Component ||
						myComponent == valkyrieWeapon3Component )
					{
						impacts.SetMassInvScales( 0.0f, 1.0f );
					}
				}
			}
			impacts++;
		}
	}

	impacts.Reset();
	CRotaryWingAircraft::ProcessPreComputeImpacts(impacts);

	// for the jet packs we want to apply self righting forces but we don't want to do that if
	// there are any contacts that would oppose this
	if( GetIsJetPack() )
	{
		impacts.Reset();
		while(!impacts.AtEnd())
		{
			if(!impacts.IsDisabled())
			{
                phInst* pOtherInstance	= impacts.GetOtherInstance();
                CEntity* pOtherEntity	= CPhysics::GetEntityFromInst( pOtherInstance );

                if( !pOtherEntity ||
                    !pOtherEntity->GetIsTypePed() )
                {
				    Vec3V myNormal;
				    impacts.GetMyNormal( myNormal );
				    static ScalarV minNormalToDisableSelfRighting( -0.2f );

				    if( IsLessThan( myNormal.GetZ(), minNormalToDisableSelfRighting ).Getb() )
				    {
					    m_bDisableSelfRighting = true;
				    }
                }
			}
			impacts++;
		}
	}

}

void CHeli::ProcessPrePhysics()
{
	CAutomobile::ProcessPrePhysics();
	static dev_bool sbForceMagicForce = false;

	m_fJetPackGroundHeight = -FLT_MAX;
    m_bDisableSelfRighting = GetVehicleDamage()->GetEngineHealth() <= 0.0f;

	if( GetIsJetPack() &&
        m_nVehicleFlags.bEngineOn )
	{
		bool disableWheelForces = GetLandingGear().GetPublicState() == CLandingGear::STATE_LOCKED_UP;

		for( int i = 0; i < m_nNumWheels; i++ )
		{
			CWheel* pWheel = GetWheel( i );

			if( pWheel )
			{
				pWheel->SetDisableWheelForces( disableWheelForces );
			}
		}
	
		if( m_nVehicleFlags.bEngineOn &&
			( GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN ||
			  sbForceMagicForce ) )
		{
			// when the landing gear is retracted check to see if we are close to the ground so we can apply a force to keep us above it
			Vector3 vProbeDirection( 0.0f, 0.0f, -4.0f );
			Matrix34 matGlobal = MAT34V_TO_MATRIX34( GetTransform().GetMatrix() );
			// add the distance we expect to travel in a frame to the position
			matGlobal.d += GetVelocity() * fwTimer::GetTimeStep() * 2.0f;
			matGlobal.d.z += CPhysics::ms_bInStuntMode ? 0.0f : 0.5f;

			//setup the capsule test.
			WorldProbe::CShapeTestHitPoint testHitPoints[ 10 ];
			WorldProbe::CShapeTestResults capsuleResults( testHitPoints, 10 );
			const u32 iTestFlags = (ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetResultsStructure(&capsuleResults);
			capsuleDesc.SetIncludeFlags(iTestFlags);
			capsuleDesc.SetExcludeEntity(this);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetCapsule( matGlobal.d, matGlobal.d + vProbeDirection, 0.75f );
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

			for(WorldProbe::ResultIterator it = capsuleResults.begin(); it < capsuleResults.last_result(); ++it)
			{
				if(it->IsAHit())
				{
					bool inGhostCollision = false;
					bool beingRespotted = false;

					bool useThisHit = true;
					CEntity* hitEntity = it->GetHitEntity();
					if(hitEntity && hitEntity->GetIsPhysical())
					{
						CPhysical* hitPhysical = SafeCast(CPhysical, hitEntity);
						if(hitPhysical->GetIsTypeVehicle())
						{
							CVehicle* hitVehicle = SafeCast(CVehicle, hitEntity);
							if(hitVehicle->IsBeingRespotted() ||
								IsBeingRespotted() )
							{
								useThisHit = false;
								beingRespotted = true;
							}
						}

						if(useThisHit)
						{
							netObject* netEntity = hitPhysical->GetNetworkObject();
							if(netEntity)
							{
								CNetObjPhysical* netPhysicalEntity = SafeCast(CNetObjPhysical, netEntity);
								if( netPhysicalEntity->IsInGhostCollision() ||
									( GetNetworkObject() &&
									  static_cast<CNetObjPhysical*>( GetNetworkObject() )->IsInGhostCollision() ) )
								{
									useThisHit = false;
									inGhostCollision = true;
								}
							}
						}
					}

					if( useThisHit && it->GetHitPosition().z > m_fJetPackGroundHeight &&
						it->GetHitNormal().z > 0.5f )
					{
						if( NetworkInterface::IsGameInProgress() &&
							hitEntity &&
							GetNetworkObject() &&
							hitEntity->GetIsPhysical() )
						{
							CNetObjPhysical* netPhysicalEntity = SafeCast( CNetObjPhysical, GetNetworkObject() );

							if( netPhysicalEntity->IsInGhostCollision() ||
								IsBeingRespotted() )
							{
								vehicleDebugf1( "CHeli::ProcessPrePhysics: jetpack: %s: hitting: %s with hover probe, being respotted: %d, In ghost collision: %d", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "", static_cast< CDynamicEntity* >( hitEntity )->GetNetworkObject() ? static_cast< CDynamicEntity* >( hitEntity )->GetNetworkObject()->GetLogName() : "", (int)beingRespotted, (int)inGhostCollision );
							}
						}
						m_fJetPackGroundHeight = it->GetHitPosition().z;
					}
				}
			}
		}
	}
}


ePhysicsResult CHeli::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	float planeDamageThreshold = m_Transmission.GetPlaneDamageThresholdOverride();
	if (planeDamageThreshold==0.0f)
	{
		planeDamageThreshold = dfHeliEngineDegradeStartingHealth;
	}

	// Process engine degrade
	if(CanEngineDegrade() && m_nVehicleFlags.bEngineOn
		&& GetVehicleDamage()->GetEngineHealth() > sfHeliEngineBreakDownHealth
		&& GetVehicleDamage()->GetEngineHealth() < planeDamageThreshold)
	{
		if(m_fEngineDegradeTimer > 1.0f)
		{
			Vector3 vEnginePosLocal = VEC3_ZERO;
			if(GetBoneIndex(VEH_ENGINE) >= 0)
			{
				Matrix34 matEngineLocal = GetLocalMtx(GetBoneIndex(VEH_ENGINE));
				vEnginePosLocal = matEngineLocal.d;
			}

			float fEngineDegradeDamage = dfHeliEngineDegradeMinDamage + (dfHeliEngineDegradeMaxDamage - dfHeliEngineDegradeMinDamage) 
				* ((GetVehicleDamage()->GetEngineHealth() - sfHeliEngineBreakDownHealth) / (planeDamageThreshold - sfHeliEngineBreakDownHealth));
			fEngineDegradeDamage = Clamp(fEngineDegradeDamage, dfHeliEngineDegradeMinDamage, dfHeliEngineDegradeMaxDamage);

			GetVehicleDamage()->ApplyDamageToEngine(this, DAMAGE_TYPE_COLLISION, fEngineDegradeDamage, vEnginePosLocal, -ZAXIS, ZAXIS, false, true, 0.0f);
			m_fEngineDegradeTimer = 0.0f;
		}

		m_fEngineDegradeTimer += fTimeStep;
	}

	// Seaplanes need to be forced to sink if required when destroyed.
	if( pHandling->GetSeaPlaneHandlingData() )
	{
		CSeaPlaneExtension* pSeaPlaneExtension = this->GetExtension<CSeaPlaneExtension>();
		Assert( pSeaPlaneExtension );
		if( pSeaPlaneExtension && m_nVehicleFlags.bBlownUp && pSeaPlaneExtension->m_nFlags.bSinksWhenDestroyed )
		{
			// If we are anchored then release anchor.
			if( pSeaPlaneExtension->GetAnchorHelper().IsAnchored() )
			{
				pSeaPlaneExtension->GetAnchorHelper().Anchor( false );
			}

			if( m_Buoyancy.m_fForceMult > 0.0f )
			{
				const float kfSinkForceMultStep = 0.002f;
				m_Buoyancy.m_fForceMult -= kfSinkForceMultStep;
			}
			else
			{
				m_Buoyancy.m_fForceMult = 0.0f;
			}
		}

		if( pSeaPlaneExtension->GetAnchorHelper().IsAnchored() )
		{
			CBoat::UpdateBuoyancyLodMode( this, pSeaPlaneExtension->GetAnchorHelper() );

			if( pSeaPlaneExtension->GetAnchorHelper().UsingLowLodMode() )
			{
				return PHYSICS_DONE;
			}
		}
	}

	// Script sometimes want the helicopter to keep flying until it explodes from damage
	// so if they're blocking missfiring, block switching the engine off, unless the engine is dead
	if( ( m_nVehicleFlags.bCanEngineMissFire || GetVehicleDamage()->GetEngineHealth() <= 0.0f ) 
		&& m_nVehicleFlags.bEngineOn
		&& GetVehicleDamage()->GetEngineHealth() < sfHeliEngineBreakDownHealth
		&& !IsNetworkClone())
	{
		SwitchEngineOff();
	}

	m_fMainRotorBrokenOffSmokeLifeTime -= fTimeStep;
	m_fMainRotorBrokenOffSmokeLifeTime = Max(m_fMainRotorBrokenOffSmokeLifeTime, 0.0f);
	m_fRearRotorBrokenOffSmokeLifeTime -= fTimeStep;
	m_fRearRotorBrokenOffSmokeLifeTime = Max(m_fRearRotorBrokenOffSmokeLifeTime, 0.0f);

	if( m_bHasLandingGear )
	{
		m_landingGear.ProcessPhysics( this );
    }

	static dev_float sfVelocityThresholdForSelfLeveling = 1.0f;
    static dev_float sfTimeThresholdForSelfLeveling = 0.25f;
    static dev_float sfUprightThreshold = 0.97f;
    Vec3V groundNormal( 0.0f, 0.0f, 1.0f );

    bool inAir = IsInAir( false );

    if( !inAir &&
        GetWheel(0) &&
        GetWheel(2) )
    {
        groundNormal = VECTOR3_TO_VEC3V( GetWheel(0)->GetHitNormal() + GetWheel(2)->GetHitNormal() ) * ScalarV( 0.5f );
    }

    float fCurrentTime = fwTimer::GetTimeInMilliseconds() * 0.0001f;
    float fLeaveGroundTimeRatio = (fCurrentTime - m_fLastWheelContactTime) / sfTimeThresholdForSelfLeveling;
    float fAngle = Dot( GetTransform().GetUp(), groundNormal ).Getf();
    bool upRight = fAngle > sfUprightThreshold;
    bool aboveVelocityThreshold = GetVelocity().Mag2() > sfVelocityThresholdForSelfLeveling;
    bool hasDriver = GetDriver() != NULL;

	if( !m_bDisableSelfRighting &&
		GetIsJetPack() &&
        ( !hasDriver ||
          fLeaveGroundTimeRatio < 1.0f ) &&
		( m_nVehicleFlags.bEngineOn ||
		  aboveVelocityThreshold ||
          inAir ||
          !upRight ) )
	{
        fAngle = AcosfSafe( fAngle );

        if( Abs( fAngle ) > 0.001f )
        {
		    static dev_float selfLevelingPitchTorqueScale = 30.0f;
            static dev_float selfLevelingRollTorqueScale = 20.0f;
        
            // Calculate the rotation axis by crossing the current and desired up vectors
            Vec3V vAxisToRotateOn = Cross( GetTransform().GetUp(), groundNormal );

            // Apply a pitch stabilisation torque so that we roll towards the desired rotation
            Vec3V vRightAxis = GetTransform().GetA();
            Vec3V vRightTorque = Dot( vAxisToRotateOn, vRightAxis ) * vRightAxis * ScalarV( fAngle * selfLevelingPitchTorqueScale );

            // Apply a roll stabilisation force so that we roll towards the desired rotation
            Vec3V vFwdAxis = GetTransform().GetB();
            Vec3V vFwdTorque = Dot( vAxisToRotateOn, vFwdAxis ) * vFwdAxis * ScalarV( fAngle * selfLevelingRollTorqueScale );

            Vector3 vecTorque = VEC3V_TO_VECTOR3( vRightTorque + vFwdTorque );
            vecTorque *= GetAngInertia();

            if( hasDriver )
            {
                vecTorque *= 1.0f - fLeaveGroundTimeRatio;
            }

		    ApplyInternalTorque( vecTorque );
        }
	}

	if( GetIsJetPack() &&
		CPhysics::GetIsLastTimeSlice( nTimeSlice ) &&
        m_nVehicleFlags.bEngineOn )
	{
		for( int i = 0; i < NUM_HELI_WINGS_MAX; i++ )
		{
			int iBoneIndex = GetBoneIndex( (eHierarchyId)( (int)HELI_WING_L + i ) );

			if(iBoneIndex > -1)
			{
				static dev_float sfMaxRotationAngle = DtoR * 25.0f;
				static dev_float sfMaxRotationAngleInStrafeMode = 0.7f;
				static dev_float sfMaxRotationRate  = 1.0f;
				static dev_float sfMaxRotationRateInStrafeMode = sfMaxRotationRate * 2.0f;
				static dev_float sfMaxSpeedForTargetAngle = 20.0f;
				static dev_float sfMaxWingAngleDeltaBrakingScale = 1.5f;
				float maxWingAngleDelta = ( m_bStrafeMode ? sfMaxRotationRateInStrafeMode : sfMaxRotationRate ) * fTimeStep;
				float targetAngle = m_fPitchControl;
				float forwardVelocity = GetVelocity().Dot( VEC3V_TO_VECTOR3( GetTransform().GetForward() ) ) / sfMaxSpeedForTargetAngle;
				forwardVelocity = Clamp( forwardVelocity, -1.0f, 1.0f );

				if( forwardVelocity * targetAngle >= 0.0f )
				{
					maxWingAngleDelta *= sfMaxWingAngleDeltaBrakingScale;
					targetAngle += forwardVelocity;
				}

				if( !m_bStrafeMode )
				{
					targetAngle += ( m_fYawControl * ( (float)i - 0.5f ) );
				}
				else
				{
					targetAngle *= sfMaxRotationAngleInStrafeMode;
				}

                if( m_fWingAngle[ i ] != targetAngle )
                {
				    m_fWingAngle[ i ] += Clamp( targetAngle - m_fWingAngle[ i ], -maxWingAngleDelta, maxWingAngleDelta );
				    m_fWingAngle[ i ] = Clamp( m_fWingAngle[ i ], -1.0f, 1.0f );

				    SetBoneRotation( iBoneIndex, ROT_AXIS_LOCAL_X, m_fWingAngle[ i ] * sfMaxRotationAngle, true, NULL, NULL );
				    GetSkeleton()->PartialUpdate( iBoneIndex );

				    int fragGroup = GetVehicleFragInst()->GetGroupFromBoneIndex( iBoneIndex );
				    if( fragGroup > -1 &&
					    GetVehicleFragInst()->GetArchetype() &&
					    GetVehicleFragInst()->GetArchetype()->GetBound() &&
					    phBound::IsTypeComposite( GetVehicleFragInst()->GetArchetype()->GetBound()->GetType() ) )
				    {
					    fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[ fragGroup ];
					    phBoundComposite *pBound = (phBoundComposite *)GetVehicleFragInst()->GetArchetype()->GetBound();

					    UpdateLinkAttachmentMatricesRecursive( pBound, pGroup );
				    }
                }
			}
		}
	}

	ePhysicsResult result = CAutomobile::ProcessPhysics(fTimeStep,bCanPostpone,nTimeSlice);

	for(int i = 0; i < m_iNumPropellers; i++)
	{	
		// B*1855069: This is now called every frame to keep the rotor current and last matrices in sync and to update articulated link attachment matrices.
		// Not having the current and last matrices being equal allows the solver to see the large rotation due to very high rotor speed.
		// Not updating the link attachment matrices causes a discrepancy between the bounds and the articulated body.
		// Both of these things can cause the vehicle's swept bounding box to become very large.
		// The existing NeedUpdatePropellerBound() call is now used to determine when the composite extents and BVH are updated, so that isn't being done any more often than it was previously.
		// The same is done for all vehicles that use this propeller collision class.
		m_propellerCollisions[i].UpdateBound(this, m_propellers[i].GetBoneIndex(), m_propellers[i].GetAngle(), m_propellers[i].GetAxis(), NeedUpdatePropellerBound(i));			
	}

	//if( GetIsJetPack() )
	//{
	//	if( !IsInAir() )
	//	{
	//		Vec3V groundNormal = VECTOR3_TO_VEC3V( GetWheel(0)->GetHitNormal() + GetWheel(2)->GetHitNormal() ) * ScalarV( 0.5f );
	//		if( Dot( GetTransform().GetC(), groundNormal ).Getf() > 0.3f )
	//		{
	//			static dev_float sfTorqueScale = 50.0f;
	//			Vec3V forwardVector = Cross( GetTransform().GetA(), groundNormal );
	//			float torque = sfTorqueScale * GetAngInertia().y * ( GetTransform().GetB().GetZf() - forwardVector.GetZf() );

	//			Vector3 torqueVector = VEC3V_TO_VECTOR3( GetTransform().GetA() ) * torque;

	//			ApplyInternalTorque( torqueVector );
	//		}
	//	}
	//}

	return result;
}

void CHeli::ProcessPostPhysics()
{
	CRotaryWingAircraft::ProcessPostPhysics();
	
	ProcessAntiSway(fwTimer::GetTimeStep());
}

void CHeli::ProcessWings()
{
	CPed* pDriver = GetDriver();

	if( pDriver )
	{
		if( pDriver && pDriver->IsLocalPlayer() )
		{
			CControl *pControl = pDriver->GetControlFromPlayer();

			static bool sbHasButtonBeenUp = true;
			sbHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();

			bool bIsAkula = MI_HELI_AKULA.IsValid() && GetModelIndex() == MI_HELI_AKULA;
			bool bIsAnnihilator2 = MI_HELI_ANNIHILATOR2.IsValid() && GetModelIndex() == MI_HELI_ANNIHILATOR2;

			bool bInputHeld = pControl->GetVehicleRoof().HistoryHeldDown( CTaskVehicleConvertibleRoof::ms_uTimeToHoldButtonDown );
			bool bInputTapped = pControl->GetVehicleRoof().IsReleased() && !pControl->GetVehicleRoof().IsReleasedAfterHistoryHeldDown(ms_uTimeToIgnoreButtonHeldDown);

			bool bInputActivated = (bIsAkula || bIsAnnihilator2) ? bInputTapped : bInputHeld;

			if( sbHasButtonBeenUp && 
				 bInputActivated &&
				( GetAreWingsFullyDeployed() || // DON'T TOGGLE THE MODE IF IT IS STILL TRANSITIONING 
				  GetAreWingsFullyRetracted() ) )
			{ 
				ToggleWings( !GetAreWingsDeployed() );
				sbHasButtonBeenUp = false;
			}
		}
	}
}


void CHeli::ToggleWings( bool deploy )
{
	for( int i = (int)VEH_FOLDING_WING_L; i <= (int)VEH_FOLDING_WING_R; i++ )
	{
		eHierarchyId nDoorId = (eHierarchyId)i;
		CCarDoor* pDoor = GetDoorFromId( nDoorId );

		if( pDoor )
		{
			// Clear flags before setting them, avoid any flag conflicts
			pDoor->ClearFlag(CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_SWINGING|CCarDoor::WILL_LOCK_DRIVEN|CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
			pDoor->SetTargetDoorOpenRatio( deploy ? 1.0f : 0.0f,  deploy ? (CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL) : (CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL|CCarDoor::WILL_LOCK_DRIVEN), this );
		}
	}

	audVehicleAudioEntity* vehicleAudioEntity = GetVehicleAudioEntity();

	if(vehicleAudioEntity)
	{
		vehicleAudioEntity->ToggleHeliWings(deploy);
	}
}

bool CHeli::GetAreWingsDeployed()
{
	for( int i = (int)VEH_FOLDING_WING_L; i <= (int)VEH_FOLDING_WING_R; i++ )
	{
		eHierarchyId nDoorId = (eHierarchyId)i;
		CCarDoor* pDoor = GetDoorFromId( nDoorId );

		if( pDoor )
		{
			if( pDoor->GetDoorRatio() < 0.7f )
			{
				return false;
			}
		}
	}
	return true;
}

bool CHeli::GetAreWingsFullyDeployed()
{
	for( int i = (int)VEH_FOLDING_WING_L; i <= (int)VEH_FOLDING_WING_R; i++ )
	{
		eHierarchyId nDoorId = (eHierarchyId)i;
		CCarDoor* pDoor = GetDoorFromId( nDoorId );

		if( pDoor )
		{
			if( pDoor->GetDoorRatio() < 0.9f )
			{
				return false;
			}
		}
	}
	return true;
}

bool CHeli::GetAreWingsFullyRetracted()
{
	for( int i = (int)VEH_FOLDING_WING_L; i <= (int)VEH_FOLDING_WING_R; i++ )
	{
		eHierarchyId nDoorId = (eHierarchyId)i;
		CCarDoor* pDoor = GetDoorFromId( nDoorId );

		if( pDoor )
		{
			if( !pDoor->GetIsLatched( this ) )
			{
				return false;
			}
		}
	}
	return true;
}

Vector3 CHeli::CalculateHoverForce( float invTimeStep )
{
	static dev_float sfJetpackMinHoverHeight = 1.1f;
	static dev_float sfJetpackMaxHoverHeight = 1.5f;
	static dev_float sfJetpackDistanceFromTargetToReduceAccelerationInv = 1.0f / 0.5f;
	static dev_float sfJetpackHoverAccelerationMax = 15.0f;
	static dev_float sfJetpackHoverAccelerationWhenStationary = 10.0f;
	static dev_float sfJetpackMaxSpeedForFullAcceleration = 10.0f;

	Vector3 result( 0.0f, 0.0f, 0.0f );

	float gearRetractRatio = 1.0f - GetLandingGear().GetGearDeployRatio();
	float targetHeight = sfJetpackMinHoverHeight + ( ( sfJetpackMaxHoverHeight - sfJetpackMinHoverHeight ) * gearRetractRatio );

	if( //m_fJetPackGroundHeight < GetTransform().GetPosition().GetZf()  &&
		m_fJetPackGroundHeight + targetHeight > GetTransform().GetPosition().GetZf() )
	{
		float targetDistance = ( m_fJetPackGroundHeight + targetHeight - GetTransform().GetPosition().GetZf() );
		float currentSpeed = GetVelocity().Dot( Vector3( 0.0f, 0.0f, 1.0f ) );
		float acceleration = 2.0f * ( ( ( targetDistance * invTimeStep ) - currentSpeed ) * invTimeStep );

		float maxAcceleration = sfJetpackHoverAccelerationWhenStationary + ( sfJetpackHoverAccelerationMax * Min( 1.0f, GetVelocity().Mag2() / sfJetpackMaxSpeedForFullAcceleration ) );

		maxAcceleration = Max( maxAcceleration, Abs( Min( 0.0f, currentSpeed * invTimeStep ) ) );
		maxAcceleration = Min( maxAcceleration, 149.9f );

		acceleration *= Min( 1.0f, targetDistance * sfJetpackDistanceFromTargetToReduceAccelerationInv );

		acceleration = Clamp( acceleration, 0.0f, maxAcceleration );

		acceleration *= GetMass();

		result = Vector3( 0.0f, 0.0f, 1.0f ) * acceleration;

		m_fStrafeModeTargetHeight = GetTransform().GetPosition().GetZf() + targetHeight;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Class CAutogyro
bank_float CAutogyro::ms_fRotorSpeedMults[] = 
{
	21.0f,	// Main
	25.0f	// Rear
};
CompileTimeAssert(CRotaryWingAircraft::Num_Heli_Rotors == 2);
void CAutogyro::SetModelId(fwModelId modelId)
{
	CRotaryWingAircraft::SetModelId(modelId);

#if __ASSERT
	const char* strDebugPropNames[] = 
	{
		"Main",
		"Rear"
	};
#endif

	// Figure out which propellers are valid, and set them up
	for(int nPropIndex = 0 ; nPropIndex < HELI_NUM_ROTORS; nPropIndex++ )
	{
		eRotationAxis nAxis = nPropIndex == Rotor_Main ?  ROT_AXIS_LOCAL_Z : ROT_AXIS_LOCAL_Y;
		eHierarchyId nId = (eHierarchyId)(HELI_ROTOR_MAIN + nPropIndex);
		if(vehicleVerifyf(GetBoneIndex(nId) > -1,"Vehicle %s is missing a propeller: %s",GetModelName(),strDebugPropNames[nPropIndex])
			&& vehicleVerifyf(m_iNumPropellers < Num_Heli_Rotors,"Out of room for plane propellers"))
		{
			// Found a valid propeller
			m_propellers[m_iNumPropellers].Init(nId,nAxis, this);
			m_propellerCollisions[m_iNumPropellers].Init(nId,this);
			m_iNumPropellers++;
		}
	}	
}


static dev_float fAngleOfAttackDropOffMult = 20.0f;	// Roughly related to rotor span... converts rpm into velocity at wing
bank_float CAutogyro::ms_fCeilingLiftCutoffRange = 25.0f;	// Over what range do we scale down lift?
void CAutogyro::ProcessFlightHandling(float fTimeStep)
{
	Assert(pHandling);
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	Assert(pFlyingHandling);

	Vector3 vecAirSpeed(0.0f, 0.0f, 0.0f);
	Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
	if (!PopTypeIsMission())
	{
		WIND.GetLocalVelocity(GetTransform().GetPosition(), RC_VEC3V(vecWindSpeed), false, false);
		vecAirSpeed -= pFlyingHandling->m_fWindMult*vecWindSpeed;
	}
	vecAirSpeed += GetVelocity();

	// need to see if rotors are angled away from vertical (probably the case with a gyro)



	// effective airspeed at wing
	// this changes with the speed the rotors spin at
	// effectively increasing wind speed in plane of rotors
	Matrix34 gyroRotorMat;
	gyroRotorMat.Identity();

	// Hijack m_fGearDownDragV for this
	gyroRotorMat.RotateLocalX(pFlyingHandling->m_fGearDownDragV);
	Matrix34 m = MAT34V_TO_MATRIX34(GetMatrix());
	gyroRotorMat.Dot3x3(m);

	Vector3 vecLift = gyroRotorMat.c;

	
	vecAirSpeed += (gyroRotorMat.b + gyroRotorMat.a) * m_fMainRotorSpeed * fAngleOfAttackDropOffMult;

	float fSpeedThroughRotor = -vecLift.Dot(vecAirSpeed);

	// Don't allow speed through rotor to reverse rotor direction
	if(fSpeedThroughRotor < 0.0f)
	{
		fSpeedThroughRotor = 0.0f;
	}
	else
	{
		// Check for stall

		static dev_bool bStall = false;
		if(bStall)
		{
			static dev_float fSinStallAngleStart = rage::Sinf(PI / 12.0f);	// 15 deg ~ critical angle

		Vector3 vecWindDir = -vecAirSpeed;
		vecWindDir.Normalize();
		float fSinStallAngle = vecWindDir.Dot(vecLift);
		if(fSinStallAngle > fSinStallAngleStart)
		{
			float fAmountOverStallAngle = fSinStallAngle - fSinStallAngleStart;
			static dev_float fRotorAccelStallMult = 2.0f;	// How quickly does accel drop off over stall angle?
			float fStallMult = rage::Max(0.0f, 1.0f - fAmountOverStallAngle*fRotorAccelStallMult);
			fSpeedThroughRotor *= fStallMult;

#if __BANK	
				if(GetStatus()==STATUS_PLAYER && CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
				{
					grcDebugDraw::AddDebugOutput("STALLED!\n");
				}
#endif
			}
		}
	}

#if __BANK	
	if(GetStatus()==STATUS_PLAYER && CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
	{
		grcDebugDraw::AddDebugOutput("Speed through rotor %.2f\n",fSpeedThroughRotor);
	}
#endif
	m_fSpeedThroughMainRotor = fSpeedThroughRotor;	

	CRotaryWingAircraft::ProcessFlightHandling(fTimeStep);
}

void CAutogyro::ProcessFlightModel(float fTimeStep)
{
	// better make sure we've got some handling data, otherwise we're screwed for flying
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	if(pFlyingHandling == NULL)
		return;

	if(fTimeStep <= 0.0f)
		return;

	CControl *pControl = NULL;
	if(GetStatus()==STATUS_PLAYER && GetDriver() && GetDriver()->IsPlayer())
		pControl = GetDriver()->GetControlFromPlayer();

	Vector3 vecAirSpeed(0.0f, 0.0f, 0.0f);
	Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
	if (!PopTypeIsMission())
	{
		WIND.GetLocalVelocity(GetTransform().GetPosition(), RC_VEC3V(vecWindSpeed), false, false);
		vecAirSpeed -= pFlyingHandling->m_fWindMult*vecWindSpeed;
	}
	vecAirSpeed += GetVelocity();

	float fMass = GetMass();
	Vector3 vecAngInertia = GetAngInertia();

#if __BANK	
	if(GetStatus()==STATUS_PLAYER && CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
	{
		Vector3 vecWindSocPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(GetTransform().GetC()) * GetBoundingBoxMax().z;
		grcDebugDraw::Line(vecWindSocPos, vecWindSocPos + 10.0f*vecWindSpeed, Color32(0,0,255));
		grcDebugDraw::Line(vecWindSocPos, vecWindSocPos - 10.0f*vecAirSpeed, Color32(0,255,0));
	}
#endif

	// Get local copies so we can mess about with them
	float fPitchControl = GetPitchControl();
	float fYawControl = GetYawControl();
	float fRollControl = GetRollControl();
	float fThrottleControl = GetThrottleControl();
	float fEngineSpeed = GetMainRotorSpeed();

	{
		// For gyro throttle variable gives us the rear engine speed
		// and 'engineSpeed' gives us the current speed of the main rotor

		// CONTROLS
		if(fPitchControl==FLY_INPUT_NULL)
		{
			fPitchControl = 0.0f;
			if(pControl)
				fPitchControl = pControl->GetVehicleFlyPitchUpDown().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
		}
		if(fRollControl==FLY_INPUT_NULL)
		{
			fRollControl = 0.0f;
			if(pControl)
				fRollControl = -pControl->GetVehicleFlyRollLeftRight().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
		}
		if(fYawControl==FLY_INPUT_NULL)
		{
			fYawControl = 0.0f;
			if(pControl)
			{
				// commented out as there are no mappings to these functions but I have left them here so they are easy to add again.
// 				if(pControl->GetVehicleLookRight().IsDown() && !pControl->GetVehicleLookLeft().IsDown())
// 					fYawControl = 1.0f;
// 				if(pControl->GetVehicleLookLeft().IsDown() && !pControl->GetVehicleLookRight().IsDown())	
// 					fYawControl = -1.0f; 
				// 2nd stick controls option for chris
				if(ABS(pControl->GetVehicleGunLeftRight().GetNorm()) > 0.008f)
					fYawControl = pControl->GetVehicleGunLeftRight().GetNorm();
			}
		}
		if(fThrottleControl==FLY_INPUT_NULL)
		{
			fThrottleControl = 0.0f;
			if(pControl)
				fThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();
		}

		// MISC STUFF

		Vector3 vecRudderArm(VEC3V_TO_VECTOR3(GetTransform().GetB()));
		Vector3 vecTailOffset = vecRudderArm * GetBoundingBoxMin().y;
		float fForwardSpd = vecAirSpeed.Dot(vecRudderArm);
		float fForwardSpdSqr = fForwardSpd*fForwardSpd;

		Matrix34 gyroRotorMat;
		gyroRotorMat.Identity();

		gyroRotorMat.RotateLocalX(pFlyingHandling->m_fGearDownDragV);
		Matrix34 m = MAT34V_TO_MATRIX34(GetMatrix());
		gyroRotorMat.Dot3x3(m);

		// Pitch and roll
		Vector3 vecUp(VEC3V_TO_VECTOR3(GetTransform().GetC()));
		Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
		ApplyInternalTorque(vecUp*fPitchControl*pFlyingHandling->m_fPitchMult*GetHoverModePitchMult()*vecAngInertia.x*fEngineSpeed, vecForward);

		vecUp = VEC3V_TO_VECTOR3(GetTransform().GetC());  // not sure if ApplyInternalTorque could have changed GetC()
		Vector3 vecRight(VEC3V_TO_VECTOR3(GetTransform().GetA()));
		ApplyInternalTorque(vecUp*fRollControl*pFlyingHandling->m_fRollMult*vecAngInertia.y*fEngineSpeed, vecRight);


		{
			Vector3 vecTailAirSpeed(VEC3_ZERO);
			// get tail position and add contribution from rotational velocity
			{
				vecTailAirSpeed = m_vecRearRotorPosition;
				vecTailAirSpeed = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vecTailAirSpeed)));
				vecTailAirSpeed.CrossNegate(GetAngVelocity());
			}
			vecTailAirSpeed.Add(vecAirSpeed);

			float fSideSpeed = DotProduct(vecTailAirSpeed, VEC3V_TO_VECTOR3(GetTransform().GetA()));
			float fFwdSpeed = DotProduct(vecTailAirSpeed, VEC3V_TO_VECTOR3(GetTransform().GetB()));
			float fAirSpeedSqr = vecAirSpeed.Mag2();

			float fSideSlipAngle = -rage::Atan2f(fSideSpeed, fFwdSpeed);
			fSideSlipAngle = rage::Clamp(fSideSlipAngle, -HELI_RUDDER_MAX_ANGLE_OF_ATTACK, HELI_RUDDER_MAX_ANGLE_OF_ATTACK);

			// doing sideways stuff
			Vector3 vecRudderForce(VEC3V_TO_VECTOR3(GetTransform().GetA()));
			// sideways force from sidesliping
			vecRudderForce *= rage::Clamp(pFlyingHandling->m_fSideSlipMult * fSideSlipAngle * fAirSpeedSqr, -100.0f, 100.0f);
			ApplyInternalForceCg(vecRudderForce*fMass);

			// YAW
			// This is proportional to forwards speed (like in plane) since rudder is fixed
			vecRudderForce = VEC3V_TO_VECTOR3(GetTransform().GetA());
			fSideSlipAngle = -1.0f*GetLocalSpeed(vecTailOffset).Dot(VEC3V_TO_VECTOR3(GetTransform().GetA()));

            // Add some extra yaw when moving and rolling
			if(m_bEnableThrustVectoring)
			{
				Vector3 vFlatVelocity = GetVelocity();
				vFlatVelocity.z = 0.0f;
				fYawControl = ComputeAdditionalYawFromTransform(fYawControl, GetTransform(), vFlatVelocity.Mag());
			}

			// control force from steering and stabilising force from rudder
			vecRudderForce *= (pFlyingHandling->m_fYawMult*GetHoverModeYawMult()*fYawControl*fForwardSpd + pFlyingHandling->m_fYawStabilise*fSideSlipAngle*fAirSpeedSqr)*vecAngInertia.z;
			ApplyInternalTorque(vecRudderForce, -VEC3V_TO_VECTOR3(GetTransform().GetB()));

			// PITCH STABILISATION
			// calculate tailplane angle of attack
			// want autogyro blades to remain flat relative to velocity
			vecRudderForce = VEC3V_TO_VECTOR3(GetTransform().GetC());
			Matrix34 matPitchStable = gyroRotorMat;
			// Again, hijack this param cos its not needed for autogyros
			matPitchStable.RotateLocalX(pFlyingHandling->m_fGearDownLiftMult);
			float fAngleOfAttackForPitch = -1.0f*GetLocalSpeed(vecTailOffset).Dot(matPitchStable.c);

			vecRudderForce *= (pFlyingHandling->m_fPitchStabilise*fAngleOfAttackForPitch*rage::Abs(fAngleOfAttackForPitch))*vecAngInertia.x*-GRAVITY;
			ApplyInternalTorque(vecRudderForce, vecTailOffset);

			// ROLL
			vecRudderForce = VEC3V_TO_VECTOR3(GetTransform().GetA());

			// also need to add some stabilisation for roll (so plane naturally levels out)
			vecRudderForce.Cross(VEC3V_TO_VECTOR3(GetTransform().GetB()), Vector3(0.0f,0.0f,1.0f));
			float fRollOffset = 1.0f;
			if(GetTransform().GetC().GetZf() > 0.0f)
			{
				if(GetTransform().GetA().GetZf() > 0.0f)
					fRollOffset = -1.0f;
			}
			else
			{
				vecRudderForce *= -1.0f;
				if(GetTransform().GetA().GetZf() > 0.0f)
					fRollOffset = -1.0f;
			}
			fRollOffset *= 1.0f - DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetA()), vecRudderForce);
			// roll stabilisation recedes as we go vertical
			fRollOffset *= 1.0f - rage::Abs(GetTransform().GetB().GetZf());
			ApplyInternalTorque(pFlyingHandling->m_fRollStabilise*fRollOffset*vecAngInertia.y*0.5f*-GRAVITY*VEC3V_TO_VECTOR3(GetTransform().GetA()), VEC3V_TO_VECTOR3(GetTransform().GetC()));
		}


		///////////////////////
		// LIFT&THRUST


		

		static dev_bool bMethod1 = true;
		if(bMethod1)
		{

			// effective airspeed at wing
			// airspeed in plane of rotors does not do anything
			// since receding blade forces cancel out proceding blade
			// so airspeed only acts along .c
			// The .b and .a forces come from the rotor rpm			

			// The components in the direction of the plane of rotation don't really mean anything geometrically
			Vector3 vVelAtWing = vecAirSpeed.Dot(gyroRotorMat.c)*gyroRotorMat.c;
			vVelAtWing += (gyroRotorMat.b + gyroRotorMat.a) * fEngineSpeed * fAngleOfAttackDropOffMult;

			float fAngleOfAttack = vVelAtWing.Dot(gyroRotorMat.c) / rage::Max(0.01f, vVelAtWing.Mag());
			fAngleOfAttack = -1.0f*rage::Asinf(Clamp(fAngleOfAttack, -1.0f, 1.0f));
			

			// Lift....
			// 1: From gyro blades spinning
			// 2: From gyro forwards speed
			// Note attackLift is being used for something different here


			// Past a certain angle we get no more lift
			static dev_float fCrititicalAngleOfAttack = PI/2.0f;
			if(fAngleOfAttack > fCrititicalAngleOfAttack)
			{
				fAngleOfAttack = 2.0f*fCrititicalAngleOfAttack - fAngleOfAttack;
				fAngleOfAttack = rage::Max(0.0f,fAngleOfAttack);
			}

			// adjust fForwardsSpeed as well
			float fForwardsSpdForLift = vVelAtWing.Dot(gyroRotorMat.b);
			float fForwardsSpdForLiftSq = fForwardsSpdForLift*fForwardsSpdForLift;
			

			float fRotorLift = pFlyingHandling->m_fFormLiftMult;
			float fAttackLift = pFlyingHandling->m_fAttackLiftMult*fAngleOfAttack;
			

			Vector3 vecLift = gyroRotorMat.c;
			float fTotalLiftForce = (fRotorLift+fAttackLift)*-GRAVITY*fMass*fForwardsSpdForLiftSq;
			

			// Lift needs to drop above pre determined flight ceiling

			const float ThisZ = GetTransform().GetPosition().GetZf();
			if(fTotalLiftForce > fMass*-GRAVITY
				&& HeightAboveCeiling(ThisZ) > 0.0f)
			{
				fTotalLiftForce = MAX(0.0f, 1.0f - HeightAboveCeiling(ThisZ)/ms_fCeilingLiftCutoffRange)*fMass*-GRAVITY;
			}

			vecLift *= fTotalLiftForce;

			ApplyInternalForceCg(vecLift);	

#if __BANK	
			if(GetStatus()==STATUS_PLAYER && CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
			{
				// Would be useful to see the wind speed and propeller matrix
				static dev_bool bDebugDraw = false;
				if(bDebugDraw)
				{
					static dev_float fAxisScale = 2.0f;
					Matrix34 debugMat = gyroRotorMat;
					debugMat.d = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

					static dev_float fLiftScale = 0.005f;
					static dev_float fVelScale = 1.0f;
							
					grcDebugDraw::Line(debugMat.d,(vVelAtWing*fVelScale)+debugMat.d,Color_green,Color_blue);
					grcDebugDraw::Line(debugMat.d,((vecLift*fLiftScale/(-GRAVITY*fMass))+debugMat.d),Color_red);

					grcDebugDraw::Axis(debugMat,fAxisScale);

				}			
			grcDebugDraw::AddDebugOutput("Angle of attack %.3f\n",fAngleOfAttack);
			}
#endif
		}
		else
		{
			Vector3 vLift = ZAXIS;
			vLift.RotateX(pFlyingHandling->m_fGearDownDragV);
			vLift = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vLift)));
			
			//float fAngleOfAttack = vecAirSpeed.Dot(vLift) / rage::Max(0.01f, vecAirSpeed.Mag());
			//fAngleOfAttack = -1.0f*rage::Asinf(Clamp(fAngleOfAttack, -1.0f, 1.0f));

			float fSpeedThroughRotor = m_fSpeedThroughMainRotor;
			static dev_bool bClampRotorSpeed = true;
			if(bClampRotorSpeed)
			{
				static dev_float fMaxSpeedThroughRotor = 3.0f;
				fSpeedThroughRotor = rage::Clamp(m_fSpeedThroughMainRotor,0.0f,fMaxSpeedThroughRotor);

			}
			
			float fAircraftFormLift = pFlyingHandling->m_fFormLiftMult * fForwardSpdSqr;
			// If we mult by fForward speed then this will never push autogyro backwards
			float fAutogyroLift = pFlyingHandling->m_fAttackLiftMult* fSpeedThroughRotor; 

			

			vLift *= (fAircraftFormLift+fAutogyroLift)*-GRAVITY*fMass;
			ApplyInternalForceCg(vLift);	
		}

		// Thrust
		// only let pilot backwards thrust when on the ground
		if(IsInAir() || fThrottleControl > 0.01f)
		{

			// Rescales throttle so full brake = 0.0f throttle
			fThrottleControl = pFlyingHandling->m_fThrust*0.5f*(fThrottleControl + 1.0f);


			// Make throttle fall off at high speed
			float fFallOff = pFlyingHandling->m_fThrustFallOff*fForwardSpdSqr;
			fThrottleControl *= (1.0f -fFallOff);
		}
		// need to do another dotProduct here rather than use fForwardSpd because we don't want the influence of the windspeed
		else if(fThrottleControl < 0.0f && DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetB()), GetVelocity()) < 0.02f)
			fThrottleControl = pFlyingHandling->m_fThrust*MIN(0.0f, (fThrottleControl - 8.0f*0.97f*fForwardSpd));
		else
			fThrottleControl = 0.0f;

		//grcDebugDraw::AddDebugOutput("Thrust: %.2f\n",fThrottleControl);
		ApplyInternalForceCg(vecRudderArm*-GRAVITY*fThrottleControl*fMass);

	}
}

// void CAutogyro::ProcessDriverInputsForPlayer(CPed *pPlayerPed)
// {
// 	CRotaryWingAircraft::ProcessDriverInputsForPlayer(pPlayerPed);
// 	
// 	ProcessPlayerControlInputsForBrakeAndGasPedal(pPlayerPed->GetControlFromPlayer(),false);
// 
// 	if(GetNumContactWheels() > 0)
// 	{
// 		SetSteerAngle(-m_fYawControl*pHandling->m_fSteeringLock);
// 	}	
// }

void CAutogyro::Teleport(const Vector3& vecSetCoors, float fSetHeading , bool bCalledByPedTask, bool bTriggerPortalRescan, bool bCalledByPedTask2, bool bWarp, bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants))
{
    // Not sure why we need to reset the rotor blades but this might need to be re-added.
	//m_fRearPropellerSpeed = 0.0f;
	//m_fSpeedThroughMainRotor = 0.0f;
	CRotaryWingAircraft::Teleport(vecSetCoors,fSetHeading,bCalledByPedTask,bTriggerPortalRescan,bCalledByPedTask2,bWarp);
}

CAutogyro::CAutogyro(const eEntityOwnedBy ownedBy, const u32 popType) : CRotaryWingAircraft(ownedBy, popType, VEHICLE_TYPE_AUTOGYRO)
{
	m_fRearPropellerSpeed = 0.0f;
	m_fSpeedThroughMainRotor = 0.0f;
}

static dev_float sfAutogyroPropellerAccel = 0.1f;
static dev_float sfAutogyroPropellerDeccel = -0.05f;
static dev_float sfAutogyroPropellerDeccelWater = -0.5f;
static dev_float sfMaxPropellerSpeed = 1.0f;
void CAutogyro::UpdatePropellerSpeed()
{
	if(m_nVehicleFlags.bEngineOn && (GetStatus()==STATUS_PLAYER || GetStatus()==STATUS_PHYSICS))
	{
		m_fRearPropellerSpeed += sfAutogyroPropellerAccel * fwTimer::GetTimeStep();
		if(m_fRearPropellerSpeed > sfMaxPropellerSpeed)
			m_fRearPropellerSpeed = sfMaxPropellerSpeed;		
	}
	else
	{

		// stop engine suddenly if fallen in water
		if(GetIsInWater() && m_fTimeInWater > 0.0f)
		{
			m_fRearPropellerSpeed += sfAutogyroPropellerDeccelWater * fwTimer::GetTimeStep();
		}
		else
		{
			m_fRearPropellerSpeed += sfAutogyroPropellerDeccel * fwTimer::GetTimeStep();
		}
		
		if(m_fRearPropellerSpeed < 0.0f)
			m_fRearPropellerSpeed = 0.0f;
	}

	// Rotor speed should be driven by wind speed through rotors
	float fRotorAccel = m_fSpeedThroughMainRotor* sfAutogyroRotorAccel - m_fMainRotorSpeed*sfAutogyroRotorDamping - sfAutogyroRotorDampingC;
	m_fMainRotorSpeed += fRotorAccel * fwTimer::GetTimeStep();

	static dev_float sfMinRotorSpeedEngineOn = 0.1f;
	static dev_float sfMaxRotorSpeed = 1.0f;

	float fMinRotorSpeed = m_nVehicleFlags.bEngineOn ? sfMinRotorSpeedEngineOn : 0.0f;

	static dev_bool bClamp = true;
	if(bClamp)
		m_fMainRotorSpeed = rage::Clamp(m_fMainRotorSpeed,fMinRotorSpeed,sfMaxRotorSpeed);	

	GetMainPropeller().UpdatePropeller(m_fMainRotorSpeed*ms_fRotorSpeedMults[Rotor_Main],fwTimer::GetTimeStep());
	GetRearPropeller().UpdatePropeller(m_fRearPropellerSpeed*ms_fRotorSpeedMults[Rotor_Rear] ,fwTimer::GetTimeStep());
}

void CAutogyro::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	UpdatePropellerSpeed();
	CRotaryWingAircraft::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
}

#if __BANK
void CAutogyro::InitRotorWidgets(bkBank& bank)
{
	const char* strPropNames[CRotaryWingAircraft::Num_Heli_Rotors] = 
	{
		"Main",
		"Rear"
	};
	CompileTimeAssert(Num_Heli_Rotors == 2);

	bank.PushGroup("Autogyro rotors");
	for(int i =0; i < Num_Heli_Rotors; i++)
	{
		bank.AddSlider(strPropNames[i],&ms_fRotorSpeedMults[i],0.0f,100.0f,0.01f);
	}
	bank.PopGroup();

}
#endif

CBlimp::CBlimp(const eEntityOwnedBy ownedBy, const u32 popType)
: CHeli(ownedBy, popType, VEHICLE_TYPE_BLIMP)
, m_isBlimpBroken(false)
{
	m_nVehicleFlags.bCanEngineDegrade = false;
	m_nVehicleFlags.bCanPlayerAircraftEngineDegrade = false;
}

void CBlimp::SetModelId(fwModelId modelId)
{
	CRotaryWingAircraft::SetModelId(modelId);


	// Figure out which propellers are valid, and set them up
	for(int nPropIndex = 0 ; nPropIndex < Num_Heli_Rotors; nPropIndex++ )// use num heli rotors as we are reusing the heli rotor array
	{
		eHierarchyId nId = (eHierarchyId)(BLIMP_PROP_1 + nPropIndex);
		if(GetBoneIndex(nId) > -1 && physicsVerifyf(m_iNumPropellers < Num_Heli_Rotors,"Out of room for propellers"))
		{
			// Found a valid propeller
			m_propellers[m_iNumPropellers].Init(nId,ROT_AXIS_LOCAL_Y, this);
			m_propellerCollisions[m_iNumPropellers].Init(nId,this);
			m_iNumPropellers++;
		}
	}

	// Call InitCompositeBound again, as it has dependency with propellers
	InitCompositeBound();
}

static dev_float sfBlimpRudderAdjustAngle = ( DtoR * 10.0f);
static dev_float sfBlimpElevatorAdjustAngle = ( DtoR * 20.0f);

ePrerenderStatus CBlimp::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	for(int i= 0; i < m_iNumPropellers; i++)
	{
		m_propellers[i].PreRender(this);
	}

	// Adjust rudder (yaw)
	SetComponentRotation(BLIMP_RUDDER_UPPER,ROT_AXIS_LOCAL_Z,m_fYawControl*sfBlimpRudderAdjustAngle,true);
	SetComponentRotation(BLIMP_RUDDER_LOWER,ROT_AXIS_LOCAL_Z,m_fYawControl*sfBlimpRudderAdjustAngle,true);

	// Elevators (pitch)
	SetComponentRotation(BLIMP_ELEVATOR_L,ROT_AXIS_LOCAL_X,-m_fPitchControl*sfBlimpElevatorAdjustAngle,true);
	SetComponentRotation(BLIMP_ELEVATOR_R,ROT_AXIS_LOCAL_X,-m_fPitchControl*sfBlimpElevatorAdjustAngle,true);

	return CAutomobile::PreRender(bIsVisibleInMainViewport);
}

//
void CBlimp::UpdateRotorSpeed()
{
	// Standard heli
	float fOldRotorSpeed = m_fPrevMainRotorSpeed;

	if(m_nVehicleFlags.bEngineOn && (GetStatus()==STATUS_PLAYER || GetStatus()==STATUS_PHYSICS || GetStatus()==STATUS_OUT_OF_CONTROL) && !m_Transmission.GetCurrentlyMissFiring())
	{
		m_fMainRotorSpeed += HELI_ROTOR_ANGULAR_ACCELERATION * fwTimer::GetTimeStep();
		if(m_fMainRotorSpeed > MAX_ROT_SPEED_HELI_BLADES)
			m_fMainRotorSpeed = MAX_ROT_SPEED_HELI_BLADES;
	}
	else
	{
		if(!m_nVehicleFlags.bEngineStarting)
		{
			int iDriverSeatIndex = GetVehicleModelInfo()->GetModelSeatInfo()->GetDriverSeat();
			int iDriverSeatBoneIndex = GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iDriverSeatIndex);

			bool bAnyAlivePassenger = false;
			for(s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); ++iSeat)
			{
				CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat);
				if(pPassenger && !pPassenger->IsInjured())
				{
					bAnyAlivePassenger = true;
					break;
				}
			}
			// only switch the engine off is no-one is on board and no-one else is about to enter the seat
			CComponentReservation* pComponentReservation = m_ComponentReservationMgr.FindComponentReservation(iDriverSeatBoneIndex, false);
			if (pComponentReservation && pComponentReservation->GetPedUsingComponent() == NULL && !IsNetworkClone() && !bAnyAlivePassenger)
			{
				SwitchEngineOff();
			}
		}

		m_fMainRotorSpeed -= HELI_ROTOR_ANGULAR_ACCELERATION * fwTimer::GetTimeStep();
		if(m_fMainRotorSpeed < 0.0f)
			m_fMainRotorSpeed = 0.0f;
	}

	if(fOldRotorSpeed != m_fMainRotorSpeed && (fOldRotorSpeed == 0.0f || m_fMainRotorSpeed == 0.0f))
	{
		UpdateRotorBounds();
	}

	//
	for(int i =0; i < m_iNumPropellers; i++)
	{	
		float fRotorDirection = (i == Rotor_Rear) ? -1.0f : 1.0f;
		m_propellers[i].UpdatePropeller(fRotorDirection * m_fMainRotorSpeed * ms_fRotorSpeedMults[0],fwTimer::GetTimeStep());		
	}

	m_fPrevMainRotorSpeed = m_fMainRotorSpeed;
}

void CBlimp::FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion)
{
	// go thru all the fragment children (there's more of them)
	BreakBlimp();

	CHeli::FinishBlowingUpVehicle(pCulprit, bInACutscene, bAddExplosion, bNetCall, weaponHash, bDelayedExplosion);

	if (m_fMainRotorHealth > 0.0f)
	{
		m_fMainRotorHealth = 0.0f;
		m_fMainRotorSpeed = 0.0f;
	}

	if(m_fRearRotorHealth > 0.0f)
	{
		m_fRearRotorHealth = 0.0f;
	}
}

void CBlimp::BreakBlimp()
{
	m_isBlimpBroken = true;

	fragInst* pFragInst = GetVehicleFragInst();
	for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
	{
		if(pFragInst->GetChildBroken(nChild))
			continue;

		int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());

		// go through each of the possible extras
		if(!GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_STRONG))
		{
			for(int nPart=BLIMP_PROP_1; nPart<=BLIMP_PROP_1 + BLIMP_NUM_OVERRIDDEN_NODES; nPart++)
			{
				// if the bone index of this child matches the bone index of the extra that's turned off then delete this component
				if(GetBoneIndex((eHierarchyId)nPart) == boneIndex)
				{
					PartHasBrokenOff((eHierarchyId)nPart);

					// depending on distance, just delete component instead of having it come flying off
					if(nPart == BLIMP_SHELL)
						pFragInst->DeleteAbove(nChild);
					else
					{
						pFragInst->BreakOffAbove(nChild);
					}

					CVehicle::ClearLastBrokenOffPart();
					break;
				}
			}
		}
	}
}
