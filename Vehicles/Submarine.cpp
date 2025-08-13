#include "vehicles/Submarine.h"

#include "Peds/Ped.h"
#include "Physics/Floater.h"
#include "Physics/physics.h"
#include "Physics/PhysicsHelpers.h"
#include "Renderer/Water.h"
#include "renderer/River.h"
#include "system/control.h"
#include "System/controlMgr.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Vfx/Systems/VfxWater.h"
#include "debug/DebugScene.h"
#include "camera/CamInterface.h"
#include "Stats/StatsMgr.h"
#include "Vfx/Decals/DecalManager.h"
#include "scene/world/gameWorld.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/Systems/VfxVehicle.h"
#include "debug/debugglobals.h"
#include "network/Events/NetworkEventTypes.h"
#include "audio/boataudioentity.h"
#include "audio/caraudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/radioslot.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"

ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

dev_float CSubmarineHandling::ms_fControlSmoothSpeed = 2.0f;
dev_float CSubmarineHandling::ms_fSubSinkForceMultStep = 0.002f;

// Below "design depth" we start applying damage to the sub. If the player doesn't heed this warning and head back towards the surface
// we then start to deplete oxygen halfway between "design depth" and "crush depth".
// At "crush depth" we instantly kill the occupants.
const float kfGlobalDesignDepth = -150.0f;
const float kfGlobalAirLeakDepth = -170.0f;
const float kfGlobalCrushDepth = -185.0f;
dev_float CSubmarineHandling::ms_fCrushStepIncrementSize = 1.0f;
dev_float CSubmarineHandling::ms_fImplosionDamageAmount = 1000000.0f; // 100000 N/M^2 = 14.5 psi
dev_u32 CSubmarineHandling::ms_nNumberOfImplosionVectors = 10;
dev_u32 CSubmarineHandling::ms_nMinIntervalBeforeNextImplisionEvent = 100;
dev_u32 CSubmarineHandling::ms_nMaxIntervalBeforeNextImplisionEvent = 2500;

dev_float CSubmarineHandling::ms_fSubCarPerImplosionDamageAmount = ( ms_fImplosionDamageAmount / (float)ms_nNumberOfImplosionVectors ) / 20.0f;

float CSubmarineCar::ms_wheelUpOffset			= 0.126f;
float CSubmarineCar::ms_wheelInset				= 0.2772f;
float CSubmarineCar::ms_wheelCoverInset			= -0.32f;
float CSubmarineCar::ms_wheelCoverZOffset		= -0.3f;
float CSubmarineCar::ms_wheelCoverRotation		= 0.3f;
float CSubmarineCar::ms_wheelSuspensionOffset	= 0.35f;
float CSubmarineCar::ms_rearWheelUpFactor		= 1.2f;
float CSubmarineCar::ms_animationSpeed	= 1.5f;

//////////////////////////////////////////////////////////////////////////
// Class CSubmarineHandling

CSubmarineHandling::CSubmarineHandling()
{
	m_fPitchControl = 0.0f;
	m_fYawControl = 0.0f;
	m_fDiveControl = 0.0f;
	m_fMaxDepthReached = 0.0f;
	m_nAirLeaks = 0;

    m_fRudderAngle = 0.0f;
    m_fElevatorAngle = 0.0f;

	m_fMinWreckDepth = 0.8f;

	m_iNumPropellers = 0;

	m_pBuoyancyOverride = NULL;

	m_bEnableCrushDamage = true;
	m_bCrushDepthsModifiedThisFrame = false;

	m_nSubmarineFlags.bSinkWhenWrecked = 1;
	m_nSubmarineFlags.bForceSurfaceMode = 0;
	m_nSubmarineFlags.bDisplayingCrushWarning = 0;
	m_nSubmarineFlags.bPropellerSubmarged = 0;

	m_iForcingNeutralBuoyancyTime = 0;

	m_fForceThrottleValue = 0.0f;
	m_fForceThrottleSeconds = 0.0f;

	ComputeTimeForNextImplosionEvent();
}

CSubmarineHandling::~CSubmarineHandling()
{
	if(m_pBuoyancyOverride)
	{
		delete m_pBuoyancyOverride;
	}
}

void CSubmarineHandling::SetModelId(CVehicle *pVehicle, fwModelId UNUSED_PARAM(modelId))
{
	// Copy the buoyancy info for this submarine class so that we can fiddle with it later.
	m_pBuoyancyOverride = pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle)->Clone();
	pVehicle->m_Buoyancy.SetBuoyancyInfoOverride(m_pBuoyancyOverride);

	// Figure out which propellers are valid, and set them up
	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		for(int nPropIndex = 0 ; nPropIndex < SUBMARINECAR_NUM_PROPELLERS; nPropIndex++ )
		{
			eHierarchyId nId = (eHierarchyId)(SUBMARINECAR_PROPELLER_1 + nPropIndex);
			if(pVehicle->GetBoneIndex(nId) > -1 && physicsVerifyf(m_iNumPropellers < SUB_NUM_PROPELLERS,"Out of room for sub propellers"))
			{
				// Found a valid propeller
				m_propellers[m_iNumPropellers].Init(nId,ROT_AXIS_LOCAL_Y,pVehicle);
				m_iNumPropellers ++;
			}
		}
	}
	else
	{
		for(int nPropIndex = 0 ; nPropIndex < SUB_NUM_PROPELLERS; nPropIndex++ )
		{
			eHierarchyId nId = (eHierarchyId)(SUB_PROPELLER_1 + nPropIndex);
			if(pVehicle->GetBoneIndex(nId) > -1 && physicsVerifyf(m_iNumPropellers < SUB_NUM_PROPELLERS,"Out of room for sub propellers"))
			{
				// Found a valid propeller
				m_propellers[m_iNumPropellers].Init(nId,ROT_AXIS_LOCAL_Y,pVehicle);
				m_iNumPropellers ++;
			}
		}
	}

	m_AnchorHelper.SetParent(pVehicle);
}
//////////////////////////////////////////////////////////////////////////
// Damage the vehicle even more, close to these bones during BlowUpCar
//////////////////////////////////////////////////////////////////////////
const int SUB_BONE_COUNT_TO_DEFORM = 15;
const eHierarchyId ExtraSubBones[SUB_BONE_COUNT_TO_DEFORM] = 
{ 
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, 
	SUB_PROPELLER_1, SUB_PROPELLER_2, SUB_PROPELLER_3, SUB_PROPELLER_4, SUB_PROPELLER_LEFT, SUB_PROPELLER_RIGHT, SUB_RUDDER, SUB_RUDDER2, SUB_ELEVATOR_L, SUB_ELEVATOR_R };

const eHierarchyId* CSubmarineHandling::GetExtraBonesToDeform(int& extraBoneCount)
{
	extraBoneCount = SUB_BONE_COUNT_TO_DEFORM;
	return ExtraSubBones;
}

void CSubmarineHandling::ForceThrottleForTime( float in_fThrottle, float in_fSeconds )
{
	m_fForceThrottleValue = in_fThrottle;
	m_fForceThrottleSeconds = in_fSeconds;
}

float CSubmarineHandling::GetCurrentDepth(CVehicle* pVehicle) const
{
	Vec3V vSubPos = pVehicle->GetTransform().GetPosition();

	// Subtract the submarine's Z coord from the water height at its location to get the depth.
	float fWaterLevel = 0.0f;
	pVehicle->m_Buoyancy.GetWaterLevelIncludingRiversNoWaves(RC_VECTOR3(vSubPos), &fWaterLevel, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

	return vSubPos.GetZf() - fWaterLevel;
}

void CSubmarineHandling::Implode(CVehicle *pVehicle, bool fullyImplode)
{
	if(IsUnderAirLeakDepth(pVehicle))
	{
		m_nAirLeaks++;
	}

	float fDamagePerImplosion = 0.0f;
	
	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		fDamagePerImplosion = ms_fSubCarPerImplosionDamageAmount;
	}
	else
	{
		fDamagePerImplosion = ms_fImplosionDamageAmount / (float)ms_nNumberOfImplosionVectors;
	}

	for (int i = 0; i < ms_nNumberOfImplosionVectors; ++i)
	{
		float randomAlpha = DtoR * fwRandom::GetRandomNumberInRange(0.0f, 180.0f);
		float randomTheta = DtoR * fwRandom::GetRandomNumberInRange(0.0f, 360.0f);

		Vector3 damagePosLocal = Vector3(cos(randomAlpha) * 2.0f, sin(randomAlpha) * 2.0f - 1.5f, cos(randomTheta) * 2.0f);
		Vector3 impulseLocal = -damagePosLocal;

		CVehicleDamage::DamageVehicleByDriver(impulseLocal, damagePosLocal, fDamagePerImplosion, DAMAGE_TYPE_WATER_CANNON, pVehicle, fullyImplode);	
	}

	if (fullyImplode)
	{
		// B*1929776: Don't modify the health for clones.
		if(!pVehicle->IsNetworkClone())
		{
			// It's game over.
			pVehicle->SetHealth(0.0f);
			pVehicle->BlowUpCar(NULL, false, false, false, WEAPONTYPE_DROWNINGINVEHICLE, false);

			pVehicle->m_nVehicleFlags.bIsDrowning = true;
		}

		g_vfxWater.TriggerPtFxSubmarineCrush(static_cast<CSubmarine*>(pVehicle));

		// More pad shake if we are destroying the submarine.
		dev_u32 snRumbleDuration = 750;
		dev_float sfRumbleIntensity = 1.0f; // Max intensity.
		CControlMgr::StartPlayerPadShakeByIntensity(snRumbleDuration, sfRumbleIntensity);
	}
	else
	{
		dev_u32 snRumbleDuration = 100;
		dev_float sfRumbleIntensityScale = 0.3f;
		CControlMgr::StartPlayerPadShakeByIntensity(snRumbleDuration, fDamagePerImplosion*sfRumbleIntensityScale);
	}

	if(pVehicle->GetVehicleAudioEntity())
	{
		if(IsUnderAirLeakDepth(pVehicle))
		{
			pVehicle->GetVehicleAudioEntity()->TriggerSubmarineImplodeWarning(fullyImplode);
		}

		pVehicle->GetVehicleAudioEntity()->TriggerSubmarineCrushSound();
		pVehicle->GetVehicleAudioEntity()->TriggerDynamicExplosionSounds(true);
	}
}

// Needs to be called every frame or it will reset.
void CSubmarineHandling::SetCrushDepths(bool bEnableCrushDamage, float fDesignDamageDepth, float fAirLeakDepth, float fCrushDepth)
{
	m_bEnableCrushDamage = bEnableCrushDamage;

	m_fDesignDepth = fDesignDamageDepth;
	m_fAirLeakDepth = fAirLeakDepth;
	m_fCrushDepth = fCrushDepth;

	m_bCrushDepthsModifiedThisFrame = true;
}

void CSubmarineHandling::ProcessCrushDepth(CVehicle *pVehicle)
{
	float fCurrentDepth = GetCurrentDepth(pVehicle);
	if(fCurrentDepth < m_fMaxDepthReached - ms_fCrushStepIncrementSize)
	{
		m_fMaxDepthReached = fCurrentDepth;
	}

	// Has someone modified the crush depths for this frame?
	if(!m_bCrushDepthsModifiedThisFrame)
	{
		m_bEnableCrushDamage = true;

		m_fDesignDepth = kfGlobalDesignDepth;
		m_fAirLeakDepth = kfGlobalAirLeakDepth;
		m_fCrushDepth = kfGlobalCrushDepth;
	}

	#if DISPLAY_CRUSH_DEPTH_HELP_TEXT
	if(m_bEnableCrushDamage && pVehicle->ContainsLocalPlayer())
	{
		DisplayCrushDepthMessages(pVehicle);
	}
	#endif // DISPLAY_CRUSH_DEPTH_HELP_TEXT

	// Handle applying damage from being too deep.
	if(m_bEnableCrushDamage && IsUnderDesignDepth(pVehicle) && pVehicle->GetStatus()!=STATUS_WRECKED)
	{
		if(IsUnderCrushDepth(pVehicle))
		{
			// If we are at the absolute limit we wish players to explore, kill them instantly.
			Implode(pVehicle, true);
		}
		else if(fwTimer::GetTimeInMilliseconds() > GetTimeForNextImplosionEvent())
		{
			// While below the design depth we continually apply implosion damage after random intervals of time.
			Implode(pVehicle, false);
			ComputeTimeForNextImplosionEvent();
		}

		if(IsUnderAirLeakDepth(pVehicle))
		{
			if(pVehicle && pVehicle->GetDriver() && pVehicle->GetDriver()->IsLocalPlayer())
			{
				// Add a constant low-intensity pad shake when sub is permanently broken and leaking air.
				dev_u32 snRumbleDuration = 1000;
				dev_float sfRumbleIntensity = 0.1f;
				CControlMgr::StartPlayerPadShakeByIntensity(snRumbleDuration, sfRumbleIntensity);
			}

		}
	}
	if(CPhysics::GetIsLastTimeSlice(CPhysics::GetCurrentTimeSlice()))
	{
		m_bCrushDepthsModifiedThisFrame = false;
	}
}


ePhysicsResult CSubmarineHandling::ProcessPhysics(CVehicle *pVehicle, float fTimeStep, bool UNUSED_PARAM(bCanPostpone), int nTimeSlice)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), pVehicle );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) );

	// No point processing submarine physics if we have a basic attachment to something.
	if(pVehicle->GetIsAttached() && !pVehicle->GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
		&& (pVehicle->GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_BASIC||pVehicle->GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD))
	{
		return PHYSICS_DONE;
	}

    // process doors (doors can keep us awake?)
    for(int i=0; i<pVehicle->GetNumDoors(); i++)
	{
        pVehicle->GetDoor(i)->ProcessPhysics(pVehicle, fTimeStep, nTimeSlice);
	}

	ProcessCrushDepth(pVehicle);

	// see if the prop / rudder is actually in the water
	float fDriveInWater = 0.0f;

	// Find propeller position
	for(int i = 0; i < m_iNumPropellers; i++)
	{
		fDriveInWater += m_propellers[i].GetSubmergeFraction(pVehicle);
	}

	if(m_iNumPropellers > 0)
	{
		fDriveInWater /= (float) m_iNumPropellers;
	}

	m_nSubmarineFlags.bPropellerSubmarged = fDriveInWater > 0.0f ? 1 : 0;

	// Do any processing necessary when asleep here, then quit.
	if(pVehicle->ProcessIsAsleep())
	{
		// Need to always keep the transmission updated, else the sub can become asleep whilst the engine revs are still high, leading them to get audibly stuck on
		ProcessProps(pVehicle, fTimeStep, fDriveInWater);

		return PHYSICS_DONE;
	}

	ProcessSubmarineHandling(pVehicle, fTimeStep, fDriveInWater);

	ProcessDepthLimit(pVehicle);

	// Make sure the possbilyTouchesWater flag is up to date before acting on it...
	if(pVehicle->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
	{
		// If bPossiblyTouchesWater is false we are either well above the surface of the water or in a tunnel.
		if (!pVehicle->m_nFlags.bPossiblyTouchesWater)
		{
			// we can't set these flags for network clones of script objects directly as they are synced
			if(!pVehicle->IsNetworkClone() || !(pVehicle->GetNetworkObject() && pVehicle->GetNetworkObject()->IsScriptObject(true)))
			{
				pVehicle->SetIsInWater( false );
			}
			pVehicle->m_Buoyancy.ResetBuoyancy();
		}
		// else process the buoyancy class
		else 
		{
			float fBuoyancyAccel = 0.0f;
			pVehicle->m_Buoyancy.Process(pVehicle, fTimeStep, false, CPhysics::GetIsLastTimeSlice(nTimeSlice), &fBuoyancyAccel);
			pVehicle->SetIsInWater(pVehicle->m_Buoyancy.GetStatus() > NOT_IN_WATER);
		}
	}

	// if it's a submarine car apply some lateral damping to make it easier to control.
	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		Matrix34 vehicleMat		= MAT34V_TO_MATRIX34( pVehicle->GetMatrix() );
		Vector3  rightVector	= vehicleMat.GetVector( 0 );

		float sideVelocity = rightVector.Dot( pVehicle->GetVelocity() );
		static float sideVelocityDamping = -0.25f;
		sideVelocity *= pVehicle->GetMass() * sideVelocityDamping;
		rightVector.Scale( sideVelocity );
		
		pVehicle->ApplyForceCg( rightVector );
	}


	for( int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		pVehicle->GetVehicleGadget(i)->ProcessPhysics(pVehicle,fTimeStep,nTimeSlice);
	}

	return PHYSICS_DONE;
}

void CSubmarineHandling::ProcessDepthLimit(CVehicle *pVehicle)
{
	// Apply extra floating force if submarine approaches the world depth limit.
	dev_float dfMaxWorldDepth = -250.0f;
	if(pVehicle->GetTransform().GetPosition().GetZf() < dfMaxWorldDepth)
	{
		float fDesiredFloatingSpeed = dfMaxWorldDepth - pVehicle->GetTransform().GetPosition().GetZf();
		float fDesiredFloatingAcceleration = Max(fDesiredFloatingSpeed - pVehicle->GetVelocity().z, 0.0f);
		Vector3 vTorque = fDesiredFloatingAcceleration*pVehicle->GetMass() * ZAXIS;
		pVehicle->ApplyForceCg(vTorque);
	}
}

void CSubmarineHandling::ComputeTimeForNextImplosionEvent()
{
	u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();
	u32 nRandInterval = fwRandom::GetRandomNumberInRange((int)ms_nMinIntervalBeforeNextImplisionEvent, (int)ms_nMaxIntervalBeforeNextImplisionEvent);
	m_nNextImplosionForceTime = nCurrentTime + nRandInterval;
}

void CSubmarineHandling::DoProcessControl(CVehicle *pVehicle, bool fullUpdate, float fFullUpdateTimeStep)
{
	physicsAssertf(pVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINE || pVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR, "submarine doesn't have correct vehicle type (%d)", pVehicle->GetVehicleType());

	pVehicle->ProcessIntelligence(fullUpdate, fFullUpdateTimeStep);

	if(pVehicle->IsInDriveableState())
	{
		if ( m_fForceThrottleSeconds > 0.0f )
		{
			pVehicle->SetThrottle(m_fForceThrottleValue);
			m_fForceThrottleSeconds -= fFullUpdateTimeStep;
		}

		//update the rudder angle
		UpdateRudder(pVehicle);

		//update the elevator angle
		UpdateElevators(pVehicle);
	}

	if(pVehicle->GetIsInWater())
	{
		ProcessBuoyancy(pVehicle);
	}

	//check for and apply vehicle damage
	pVehicle->GetVehicleDamage()->Process(fwTimer::GetTimeStep());

}

void CSubmarineHandling::ProcessBuoyancy(CVehicle *pVehicle)
{
	const CSubmarineHandlingData* pSubHandling = pVehicle->pHandling->GetSubmarineHandlingData();
	if(physicsVerifyf(pSubHandling,"Can't find submarine handling"))
	{
		physicsAssertf(pSubHandling->GetDiveSpeed() <= 1.0f, "Submarine dive speed should be between 0 and 1");
		Assert(pVehicle->GetDoor(0));
		CCarDoor* pHatch = pVehicle->GetDoor(0);

		// If the sub is missing its hatch and is submerged enough, mark it as wrecked so that peds within it can drown.
		if( pHatch->GetFragChild() != -1 &&
			pVehicle->GetFragInst()->GetChildBroken(pHatch->GetFragChild()) && 
			pVehicle->m_Buoyancy.GetSubmergedLevel() > m_fMinWreckDepth )
		{
			pVehicle->SetIsWrecked();
		}

		// B*1974048 - Spy car is no longer wrecked if the door is opened in water.
		//if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR &&
		//	pVehicle->m_Buoyancy.GetSubmergedLevel() > m_fMinWreckDepth )
		//{
		//	int numDoors = pVehicle->GetNumDoors();

		//	for( int i = 0; i < numDoors; i++ )
		//	{
		//		if( pVehicle->GetDoor( i )->GetDoorRatio() > 0.01f )
		//		{
		//			pVehicle->SetIsWrecked();
		//			pVehicle->SwitchEngineOff();
		//			break;
		//		}
		//	}
		//}

		// Handle sinking when wrecked.
		if(pVehicle->GetStatus()==STATUS_WRECKED && GetSinksWhenWrecked())
		{
			// If we are anchored then release anchor.
			if(GetAnchorHelper().IsAnchored())
			{
				GetAnchorHelper().Anchor(false);
			}

			pVehicle->m_Buoyancy.m_fForceMult = rage::Max(0.0f, pVehicle->m_Buoyancy.m_fForceMult - ms_fSubSinkForceMultStep);
			return;
		}
		else if( pVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINECAR )
		{
			pVehicle->m_Buoyancy.m_fForceMult = 1.0f + (pSubHandling->GetDiveSpeed() * m_fDiveControl);
		}

		// While the sub has no driver we want it to surface and have greater buoyancy than when being driven so that it
		// doesn't look weird that peds can enter it.
		bool bForcingNeutralBuoyancy = fwTimer::GetTimeInMilliseconds() < m_iForcingNeutralBuoyancyTime;
		bool bSurfaceMode = !(!GetForceSurfaceMode() && (bForcingNeutralBuoyancy || ((pVehicle->GetDriver() || pVehicle->m_nVehicleFlags.bUsingPretendOccupants) && pHatch->GetIsClosed())));

        TUNE_GROUP_FLOAT( SUBMARINE_TUNE, BIG_SUB_BUOYANCY_MULT, 2.25f, 0.0f, 10.0f, 0.1f );

        const float fSubmergeLevel = pVehicle->m_Buoyancy.GetSubmergedLevel();

		if(!bSurfaceMode)
		{
			if(pVehicle->GetBaseModelInfo() && pVehicle->GetBaseModelInfo()->GetBuoyancyInfo())
			{
				// The sub has neutral buoyancy while there is a driver at the controls.
				// Work out fSizeTotal from the current buoyancy data and use that to compute what the new buoyancy constant should be.
				pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle)->m_fBuoyancyConstant =
					pVehicle->GetVehicleModelInfo()->ComputeBuoyancyConstantFromSubmergeValue(1.0f, 0.0f, true);

                if( MI_SUB_KOSATKA.IsValid() && 
					pVehicle->GetModelIndex() == MI_SUB_KOSATKA &&
                    fSubmergeLevel < 0.9f &&
					m_fDiveControl >= 0.0f )
				{
					pVehicle->m_Buoyancy.GetBuoyancyInfo( pVehicle )->m_fBuoyancyConstant *= BIG_SUB_BUOYANCY_MULT;
				}
			}
		}
		else
		{
			if(pVehicle->GetBaseModelInfo() && pVehicle->GetBaseModelInfo()->GetBuoyancyInfo())
			{
				pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle)->m_fBuoyancyConstant =
					pVehicle->GetVehicleModelInfo()->ComputeBuoyancyConstantFromSubmergeValue(pVehicle->pHandling->m_fBuoyancyConstant, 0.0f, true);
			}
		}

        TUNE_GROUP_FLOAT( SUBMARINE_TUNE, BIG_SUB_BUOYANCY_EXTRA_MULT, 1.15f, 0.0f, 10.0f, 0.1f );
        TUNE_GROUP_FLOAT( SUBMARINE_TUNE, BIG_SUB_SUBMERGE_LEVEL_FOR_EXTRA_MULT, 0.6f, 0.0f, 10.0f, 0.1f );

        if( MI_SUB_KOSATKA.IsValid() &&
            pVehicle->GetModelIndex() == MI_SUB_KOSATKA &&
            fSubmergeLevel < BIG_SUB_SUBMERGE_LEVEL_FOR_EXTRA_MULT &&
            m_fDiveControl >= 0.0f )
        {
            pVehicle->m_Buoyancy.GetBuoyancyInfo( pVehicle )->m_fBuoyancyConstant *= BIG_SUB_BUOYANCY_EXTRA_MULT;
        }

        if( MI_SUB_KOSATKA.IsValid() &&
            pVehicle->GetModelIndex() == MI_SUB_KOSATKA )
        {
            TUNE_GROUP_FLOAT( SUBMARINE_TUNE, BIG_SUB_SUBMERGE_LEVEL_FOR_PITCH, 0.3f, 0.0f, 1.0f, 0.1f );
            m_fPitchControl *= Max( 0.0f, ( fSubmergeLevel - BIG_SUB_SUBMERGE_LEVEL_FOR_PITCH ) / ( 1.0f - BIG_SUB_SUBMERGE_LEVEL_FOR_PITCH ) );
        }

		// If we are breaking the surface, we apply some forces here to avoid the bounce back from the sudden drop in
		// buoyancy. This makes it easier to pilot the sub up to the surface and skim along it.
		TUNE_FLOAT(sfSubStickToSurfaceAtSubmergeFactor, 0.5f, 0.1f, 1.0f, 0.1f);
		TUNE_FLOAT(sfSubStickToSurfaceAtSubmergeFactorSurfaceMode, 0.2f, 0.1f, 1.0f, 0.1f);

		if(fSubmergeLevel < 1.0f)
		{
			// Whether we apply forces or not depends on the player control inputs.
			dev_float sfPitchThreshold = -0.1f;
			if(!(GetDiveControl() < 0.0f || GetPitchControl() < 0.0f || pVehicle->GetVehicleForwardDirection().GetZf() < sfPitchThreshold))
			{
				Vector3 vSubVelocity = pVehicle->GetVelocity();
				float fSubStickToSurfaceAtSubmergeFactor = bSurfaceMode ? sfSubStickToSurfaceAtSubmergeFactorSurfaceMode : sfSubStickToSurfaceAtSubmergeFactor;
				if(vSubVelocity.z < 0.0f && fSubmergeLevel > fSubStickToSurfaceAtSubmergeFactor)
				{
					vSubVelocity.x = vSubVelocity.y = 0.0f;
					vSubVelocity.z *= -1.0f*pVehicle->GetMass();
					// Scale quadratically based on submerge level to avoid sudden bump with stick to surface force.
					float temp = 1.0f/sfSubStickToSurfaceAtSubmergeFactor;
					float x = rage::Clamp(fSubmergeLevel*temp - 0.5f*temp, 0.0f, 1.0f);
					vSubVelocity.z *= x*x;
					pVehicle->ApplyImpulseCg(vSubVelocity);
				}
			}

            TUNE_GROUP_FLOAT( SUBMARINE_TUNE, AUTO_ANCHOR_DEPTH, 0.5f, 0.0f, 1.0f, 0.1f );

            if( MI_SUB_KOSATKA.IsValid() && 
				pVehicle->GetModelIndex() == MI_SUB_KOSATKA &&
                !pVehicle->GetDriver() && 
                fSubmergeLevel < AUTO_ANCHOR_DEPTH &&
                !GetAnchorHelper().IsAnchored() )
            {
                if( GetAnchorHelper().CanAnchorHere( true ) )
                {
                    GetAnchorHelper().Anchor( true );
                }
            }
        }
	}
}

float CSubmarineHandling::ProcessProps(CVehicle *pVehicle, float fTimeStep, float fDriveInWater)
{
	bool isSubmarineCar = ( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR );
	if( isSubmarineCar )
	{
		if( !static_cast< CSubmarineCar* >( pVehicle )->AreWheelsActive() )
		{
			pVehicle->m_Transmission.SelectAppropriateGearForSpeed();
		}
	}

	// Thrust
	// Want smooth engine changes
	float fDriveForce = pVehicle->m_Transmission.ProcessSubmarine(pVehicle, fDriveInWater, fTimeStep);

	// Now update propellers with drive force (for rotation)
	// Todo: transmission should handle idle revs
	// Clutch should be down at idle?
	float fPropSpeed = rage::Clamp(pVehicle->m_Transmission.GetRevRatio() - CTransmission::ms_fIdleRevs / (1.0f - CTransmission::ms_fIdleRevs),0.0f,1.0f);
	TUNE_FLOAT(sfDefaultPropSpeed, 24.0f, 0.0f, 100.0f, 0.1f);
	float fMainPropellerSpeed = sfDefaultPropSpeed;
	float fTurnPropellerSpeed = sfDefaultPropSpeed;

	if( isSubmarineCar )
	{
		fPropSpeed *= 10.0f;
	}

	if( MI_SUB_KOSATKA.IsValid() && pVehicle->GetModelIndex() == MI_SUB_KOSATKA )
	{
		TUNE_FLOAT(sfKosatkaPropSpeed, 10.0f, 0.0f, 100.0f, 0.1f);
		fMainPropellerSpeed = sfKosatkaPropSpeed;
		fTurnPropellerSpeed = sfKosatkaPropSpeed;
	}

	fPropSpeed *= fMainPropellerSpeed / pVehicle->m_Transmission.GetGearRatio(pVehicle->m_Transmission.GetGear());

	if( MI_SUB_AVISA.IsValid() && pVehicle->GetModelIndex() == MI_SUB_AVISA )
	{
		for(int i = 0; i < m_iNumPropellers; i++)
		{
			// Left and right props help turn us
			float fFinalPropSpeed = 0.0f;

			int iLeftBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_LEFT);
			int iRightBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_RIGHT);

			if(iLeftBoneIndex > -1 && iLeftBoneIndex == m_propellers[i].GetBoneIndex())
			{
				fFinalPropSpeed = (fTurnPropellerSpeed*(-m_fYawControl));
			}
			else if(iRightBoneIndex > -1 && iRightBoneIndex == m_propellers[i].GetBoneIndex())
			{
				fFinalPropSpeed = (fTurnPropellerSpeed*(-m_fYawControl));
			}
			else if(m_propellers[i].GetBoneIndex() == pVehicle->GetBoneIndex(SUB_PROPELLER_3) || m_propellers[i].GetBoneIndex() == pVehicle->GetBoneIndex(SUB_PROPELLER_4))
			{
				fFinalPropSpeed = (fTurnPropellerSpeed*(-m_fDiveControl));
			}
			else
			{
				fFinalPropSpeed = fPropSpeed;
			}

			fFinalPropSpeed = rage::Clamp(fFinalPropSpeed,-fMainPropellerSpeed,fMainPropellerSpeed);

			m_propellers[i].UpdatePropeller(fFinalPropSpeed,fTimeStep);

			fDriveInWater += m_propellers[i].GetSubmergeFraction(pVehicle);
		}
	}
	else if( MI_SUB_KOSATKA.IsValid() && pVehicle->GetModelIndex() == MI_SUB_KOSATKA )
	{
		for(int i = 0; i < m_iNumPropellers; i++)
		{
			// Left and right props help turn us
			float fTurnMult = 0.0f;
			int iLeftBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_1);
			int iRightBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_2);

			if(iLeftBoneIndex > -1 && iLeftBoneIndex == m_propellers[i].GetBoneIndex())
			{
				fTurnMult = -1.0f;
			}
			else if(iRightBoneIndex > -1 && iRightBoneIndex == m_propellers[i].GetBoneIndex())
			{
				fTurnMult = 1.0f;
			}

			// Can't go faster than main propeller speed
			float fDesiredPropSpeed = rage::Clamp(fPropSpeed + (fTurnMult*fTurnPropellerSpeed*m_fYawControl),-fMainPropellerSpeed,fMainPropellerSpeed);
			fDesiredPropSpeed *= -1.0f; // Flip the rotation direction as the propeller is modeled backwards.

			float fCurrentPropSpeed = m_propellers[i].GetSpeed();

			TUNE_FLOAT(sfPropChangeTimeFactor, 3.0f, -2.0f, 10.0f, 0.1f);

			//Lerp towards new prop speed.
			float fNewPropSpeed = fCurrentPropSpeed + (Clamp(fDesiredPropSpeed - fCurrentPropSpeed, -1.0f, 1.0f) * sfPropChangeTimeFactor * fTimeStep);

			m_propellers[i].UpdatePropeller(fNewPropSpeed,fTimeStep);

			fDriveInWater += m_propellers[i].GetSubmergeFraction(pVehicle);
		}
	}
	else
	{
		for(int i = 0; i < m_iNumPropellers; i++)
		{
			// Left and right props help turn us
			float fTurnMult = 0.0f;
			int iLeftBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_LEFT);

			if( isSubmarineCar )
			{
				iLeftBoneIndex = pVehicle->GetBoneIndex( SUBMARINECAR_PROPELLER_1 );
			}

			int iRightBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_RIGHT);

			if( isSubmarineCar )
			{
				iRightBoneIndex = pVehicle->GetBoneIndex( SUBMARINECAR_PROPELLER_2 );
			}

			if(iRightBoneIndex < 0)
			{
				iRightBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_2);
			}
			// Only do this for subs with two main propellers like "submersible2" so as not to turn the main prop on "submersible" when
			// just under yaw control.
			if(iRightBoneIndex > -1 && iLeftBoneIndex < 0)
			{
				iLeftBoneIndex = pVehicle->GetBoneIndex(SUB_PROPELLER_1);
			}

			if(iLeftBoneIndex > -1 && iLeftBoneIndex == m_propellers[i].GetBoneIndex())
			{
				fTurnMult = -1.0f;
			}
			else if(iRightBoneIndex > -1 && iRightBoneIndex == m_propellers[i].GetBoneIndex())
			{
				fTurnMult = 1.0f;
			}

			// Can't go faster than main propeller speed
			fTurnMult = rage::Clamp(fPropSpeed + (fTurnMult*fTurnPropellerSpeed*m_fYawControl),-fMainPropellerSpeed,fMainPropellerSpeed);
			fTurnMult *= -1.0f; // Flip the rotation direction as the propeller is modeled backwards.

			m_propellers[i].UpdatePropeller(fTurnMult,fTimeStep);

			fDriveInWater += m_propellers[i].GetSubmergeFraction(pVehicle);
		}
	}

	return fDriveForce;
}

void CSubmarineHandling::ProcessSubmarineHandling(CVehicle *pVehicle, float fTimeStep, float fDriveInWater)
{
	const CSubmarineHandlingData* pSubHandling = pVehicle->pHandling->GetSubmarineHandlingData();
	if(!physicsVerifyf(pSubHandling,"Can't find submarine handling"))
	{
		return;
	}

	// Submarine physics sim
	// pitch and yaw
	Vector3 vTorque;

	if(fDriveInWater > 0.0f && pVehicle->IsInDriveableState())
	{
		if( (pVehicle->GetBoneIndex(SUB_RUDDER) > -1 && pVehicle->GetBoneIndex(SUB_ELEVATOR_L) > -1 ) ||
			(pVehicle->GetBoneIndex(SUBMARINECAR_RUDDER_1) > -1 && pVehicle->GetBoneIndex(SUBMARINECAR_ELEVATOR_L) > -1 ) )
		{
			// New sub handling
			dev_float SUB_FORCE_CLAMP = 20.0f;
			// get propeller position (will get from bone)
			Vector3 vecThrustPos = pVehicle->GetBoundingBoxMin();
			vecThrustPos.x = 0.0f;
			vecThrustPos.y = 0.0f;
			vecThrustPos.z *= 0.9f;
			vecThrustPos = pVehicle->TransformIntoWorldSpace(vecThrustPos);

			Vector3 vecLocalSpeed;
			vecLocalSpeed = pVehicle->GetLocalSpeed(vecThrustPos, true);

			float fFwdDot = vecLocalSpeed.Dot( VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()) );
			//float fFwdDotSq = fFwdDot * fFwdDot;
			//fFwdDot = Min(fFwdDot, fFwdDotSq);
			//fFwdDot = Min(fFwdDot, SUB_FORCE_CLAMP);

			fFwdDot = Clamp( fFwdDot, -SUB_FORCE_CLAMP, SUB_FORCE_CLAMP );
			float increasePitchUpForce = 1.0f;

			if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
			{
				// even if the submarine car is stationary apply a little bit of torque as the props still spin and it looks a 
				// bit weird that it doesn't turn.
				dev_float SUB_CAR_FORCE_CLAMP_MIN = 1.0f;
				if( Abs( fFwdDot ) < SUB_CAR_FORCE_CLAMP_MIN )
				{
					static dev_float SUB_CAR_INCREASE_STATIONARY_PITCH_UP_FORCE = 3.0f;
					increasePitchUpForce = SUB_CAR_INCREASE_STATIONARY_PITCH_UP_FORCE;

					if( fFwdDot < 0.0f )
					{
						fFwdDot = -SUB_CAR_FORCE_CLAMP_MIN;
					}
					else
					{
						fFwdDot = SUB_CAR_FORCE_CLAMP_MIN;
					}
				}

			}

			// Calculate rudder force
			float fRudderForce = fFwdDot * pSubHandling->GetYawMult() * -m_fRudderAngle;
			bool bGoingForward = fFwdDot > 0.0f;

			if( bGoingForward )
			{
				fRudderForce *= -1.0f;
			}

			vTorque = fRudderForce * pVehicle->GetAngInertia().z * VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC()) * fDriveInWater;
			pVehicle->ApplyTorque(vTorque);

			Vector3 angularInertia	= pVehicle->GetAngInertia();
			const Vector3 vecRight	= VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());
			const Vector3 vecUp		= VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC());

			if(MI_SUB_KOSATKA.IsValid() && pVehicle->GetModelIndex()==MI_SUB_KOSATKA)
			{
				// Yaw from bow thruster
				Vector3 vBowThrusterTorque = m_fYawControl * pVehicle->GetAngInertia().z * VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC()) * pSubHandling->GetYawMult() * fDriveInWater;
				pVehicle->ApplyTorque(vBowThrusterTorque);

				// pitch between some min and max pitch angle, we'll base this on the elevator angle
				float fPitchAngle = bGoingForward ? m_fElevatorAngle : -m_fElevatorAngle;

				float fPitch = bGoingForward ? pVehicle->GetTransform().GetPitch() : -pVehicle->GetTransform().GetPitch();

				float fPitchDiff = fPitchAngle - fPitch;
				fPitchDiff = fwAngle::LimitRadianAngleForPitch(fPitchDiff);

				// Give us a pitch input from -1 -> 1. We want to scale the upward pitch such that we don't pitch up at all when on the surface.
				Vector3 vObjectCentre;
				pVehicle->GetBoundCentre(vObjectCentre);
				float fDepth = pVehicle->m_Buoyancy.GetAvgAbsWaterLevel()-vObjectCentre.z;

				fPitchDiff = rage::Clamp(fPitchDiff, -1.0f, 1.0f);

				TUNE_FLOAT(sfDepthAboveWhichNoPitch, 1.2f, -2.0f, 10.0f, 0.1f);
				TUNE_FLOAT(sfDepthBelowWhichFullPitch, 2.5f, -2.0f, 10.0f, 0.1f);
				float fLerpRange = sfDepthBelowWhichFullPitch - sfDepthAboveWhichNoPitch;
				float fLerpFraction = rage::Clamp(fDepth, sfDepthAboveWhichNoPitch, sfDepthBelowWhichFullPitch);
				fLerpFraction = (fLerpFraction-sfDepthAboveWhichNoPitch)/fLerpRange;
				fPitchDiff *= fLerpFraction;

				//if any of the sub is above the surface reduce the force from the elevator even further
				if(pVehicle->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER)
				{
					dev_float PARTIALLY_SURFACED_MULT = 0.6f;
					if(fPitchDiff > 0.0f)
					{
						fPitchDiff *= PARTIALLY_SURFACED_MULT;
					}
				}

				// Apply pitch.
				vTorque = fFwdDot * vecRight * angularInertia.x * fPitchDiff * pSubHandling->GetPitchMult() * fDriveInWater;
				pVehicle->ApplyTorque(vTorque);
			}
			else
			{
				// Calculate elevator force
				float fElevatorForce = fFwdDot *pSubHandling->GetPitchMult() * -m_fElevatorAngle;

				if(bGoingForward)
				{
					fElevatorForce *= -1.0f;
				}
				if( m_fElevatorAngle > 0.0f )
				{
					fElevatorForce *= increasePitchUpForce;
				}

				// If we're coming out of the water massively reduce the amount of torque we can apply
				if(pVehicle->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER)
				{
					dev_float PARTIALLY_SURFACED_MULT = 0.6f;
					if(fElevatorForce > 0.0f)//if we're trying to rise above the surface reduce the force from the elevator significantly
					{
						fElevatorForce *= PARTIALLY_SURFACED_MULT;
					}
				}

				vTorque = fElevatorForce * angularInertia.x * vecRight * fDriveInWater;
				pVehicle->ApplyTorque(vTorque);
			}

			// Roll slightly when yawing as it looks nice.
			Vector3 vecRudderForce = vecRight;
			vecRudderForce *= pSubHandling->GetRollMult() * -m_fRudderAngle * fFwdDot * angularInertia.y * -GRAVITY;

			pVehicle->ApplyTorque( vecRudderForce, vecUp );

			// also need to add some stabilization for roll (so sub naturally levels out)
			//Vector3 torque = vecRight;
			//torque *= pSubHandling->GetRollStab() * angularInertia.y * 0.5f * -GRAVITY;
			//pVehicle->ApplyTorque( torque, vecUp );
			float torqueMag = pSubHandling->GetRollStab() * angularInertia.y * 0.5f * -GRAVITY * vecRight.GetZ();
			Vector3 torque = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()) * torqueMag;
			pVehicle->ApplyTorque( torque );
		}
		else //Old sub steering still used for submersible
		{
			// pitch between some min and max pitch angle
			float fPitchAngle = PI * pSubHandling->GetPitchAngle() / 180.0f;

			float fPitchDiff = (m_fPitchControl * fPitchAngle) - pVehicle->GetTransform().GetPitch();
			fPitchDiff = fwAngle::LimitRadianAngleForPitch(fPitchDiff);

			// How fast does input saturate?
			// Shouldn't need to tweak per vehicle
			static dev_float fPitchSensitivity = 4.0f;
			fPitchDiff *= fPitchSensitivity;

			// Give us a pitch input from -1 -> 1. We want to scale the upward pitch such that we don't pitch up at all when on the surface.
			Vector3 vObjectCentre;
			pVehicle->GetBoundCentre(vObjectCentre);
			float fDepth = pVehicle->m_Buoyancy.GetAvgAbsWaterLevel()-vObjectCentre.z;
#if __BANK
			TUNE_BOOL(DEBUG_SUBMARINE_DEPTH, false);
			if(DEBUG_SUBMARINE_DEPTH)
			{
				char zText[100];
				sprintf(zText, "Depth: %5.3f", fDepth);
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), Color_white, zText);
			}
#endif // __BANK
			fPitchDiff = rage::Clamp(fPitchDiff, -1.0f, 1.0f);
			if(fPitchDiff > 0.0f)
			{
				TUNE_FLOAT(sfDepthAboveWhichNoPitch, 1.2f, -2.0f, 10.0f, 0.1f);
				TUNE_FLOAT(sfDepthBelowWhichFullPitch, 2.5f, -2.0f, 10.0f, 0.1f);
				float fLerpRange = sfDepthBelowWhichFullPitch - sfDepthAboveWhichNoPitch;
				float fLerpFraction = rage::Clamp(fDepth, sfDepthAboveWhichNoPitch, sfDepthBelowWhichFullPitch);
				fLerpFraction = (fLerpFraction-sfDepthAboveWhichNoPitch)/fLerpRange;
				fPitchDiff *= fLerpFraction;
			}

			// Apply pitch.
			vTorque = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()) * pVehicle->GetAngInertia().x * fPitchDiff * pSubHandling->GetPitchMult() * fDriveInWater;
			pVehicle->ApplyTorque(vTorque);

			// Yaw
			vTorque = m_fYawControl * pVehicle->GetAngInertia().z * VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC()) * pSubHandling->GetYawMult() * fDriveInWater;
			pVehicle->ApplyTorque(vTorque);
		}
	}

	// Thrust
	// Want smooth engine changes
	float fDriveForce = ProcessProps(pVehicle, fTimeStep, fDriveInWater);

	if(fDriveForce != 0.0f)
	{
		vTorque = fDriveForce*pVehicle->GetMass()*VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
		pVehicle->ApplyForceCg(vTorque);
	}
}

bool CSubmarineHandling::WantsToBeAwake(CVehicle *pVehicle)
{
	if(pVehicle->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame())
	{
		return pVehicle->m_Buoyancy.GetSubmergedLevel() > 0.0f;
	}
	else
	{
		if(pVehicle->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
		{
			return pVehicle->m_nFlags.bPossiblyTouchesWater;
		}
		else
		{
			return pVehicle->GetIsInWater();
		}
	}
}

void CSubmarineHandling::PreRender(CVehicle *pVehicle, const bool UNUSED_PARAM(bIsVisibleInMainViewport))
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), pVehicle );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) );

	if(!pVehicle->IsDummy())
	{
		// vehicle damage fx
		pVehicle->GetVehicleDamage()->PreRender();
	}

	pVehicle->SetComponentRotation(SUB_RUDDER, ROT_AXIS_Z, -m_fRudderAngle, true);
	pVehicle->SetComponentRotation(SUB_RUDDER2, ROT_AXIS_Z, -m_fRudderAngle, true);
	pVehicle->SetComponentRotation(SUB_ELEVATOR_L, ROT_AXIS_X, -m_fElevatorAngle, true);
	pVehicle->SetComponentRotation(SUB_ELEVATOR_R, ROT_AXIS_X, -m_fElevatorAngle, true);

    for(int i=0; i<pVehicle->GetNumDoors(); i++)
	{
        pVehicle->GetDoor(i)->ProcessPreRender(pVehicle);
	}

	PreRenderPropellers( pVehicle );
	
	if (IsSubLeakingAir())
	{
		float damageEvo = Min(1.0f, GetNumberOfAirLeaks()/20.0f);
		g_vfxWater.UpdatePtFxSubmarineAirLeak(static_cast<CSubmarine*>(pVehicle), damageEvo);
	}
}

void CSubmarineHandling::PreRender2(CVehicle *pVehicle, const bool UNUSED_PARAM(bIsVisibleInMainViewport))
{
	//Check to see if light and camera are underwater together or not.
	//If not then we reduce the intensity and size of headlight Corona
	CVehicleModelInfo* pModelInfo=(CVehicleModelInfo*)pVehicle->GetBaseModelInfo();
	Vector3 leftBonePos; 
	Vector3 rightBonePos; 
	s32 boneIdLeft = VEH_HEADLIGHT_L;
	s32 boneIdRight = VEH_HEADLIGHT_R;
	s32		boneIdxLeft = pModelInfo->GetBoneIndex(boneIdLeft);
	s32		boneIdxRight = pModelInfo->GetBoneIndex(boneIdRight);
	pVehicle->GetDefaultBonePositionSimple(boneIdxLeft,leftBonePos);
	pVehicle->GetDefaultBonePositionSimple(boneIdxRight,rightBonePos);
	
	//Using only mid position between lights as its cheaper 
	Vector3 lightPos = (leftBonePos + rightBonePos) *0.5f;
	lightPos = pVehicle->TransformIntoWorldSpace(lightPos);

	float oceanWaterZ = 0.0f;
	bool isLightUnderwater = false;
	bool foundOceanWater = CVfxHelper::GetOceanWaterZ(RCC_VEC3V(lightPos), oceanWaterZ);
	if (foundOceanWater)
	{
		float oceanWaterDepth = Max(0.0f, oceanWaterZ-lightPos.z);
		isLightUnderwater = oceanWaterDepth>0.0f;
	}


	// Kosatka: keep external lights always on (when above water surface): B*6786266 - Kosatka - Headlight on top of the tower to be on all the time during the night
	if(pVehicle->GetModelNameHash() == MID_KOSATKA)
	{
		pVehicle->m_OverrideLights = isLightUnderwater? NO_CAR_LIGHT_OVERRIDE : FORCE_CAR_LIGHTS_ON;
	}

	// don't need to update the lights here for submarine car as the automobile will do them
	if( pVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINECAR )
	{
		u32 nLightFlags = LIGHTS_USE_EXTRA_AS_HEADLIGHTS;
		if( camInterface::IsDominantRenderedCameraAnyFirstPersonCamera() && pVehicle->ContainsLocalPlayer() )
		{
			nLightFlags |= LIGHTS_IGNORE_INTERIOR_LIGHT;
		}
		else
		{
			nLightFlags |= LIGHTS_FORCE_INTERIOR_LIGHT;
		}

		if(Water::IsCameraUnderwater())
		{
			nLightFlags |= (LIGHTS_USE_VOLUMETRIC_LIGHTS | LIGHTS_FORCE_LOWRES_VOLUME);
		}

		if(Water::IsCameraUnderwater() != isLightUnderwater)
		{
			nLightFlags |= LIGHTS_UNDERWATER_CORONA_FADE;
		}
		
		pVehicle->DoVehicleLights(nLightFlags);
	}

	// vfx
	if( m_fDiveControl<0.0f &&
		pVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINECAR )
	{
		g_vfxWater.UpdatePtFxSubmarineDive(static_cast<CSubmarine*>(pVehicle), -m_fDiveControl);
	}
}

void CSubmarineHandling::PreRenderPropellers( CVehicle* pVehicle )
{
	for( int i = 0; i < m_iNumPropellers; i++ )
	{
		m_propellers[i].PreRender( pVehicle );

		// process propeller vfx
		s32 propBoneIndex = m_propellers[i].GetBoneIndex();
		g_vfxWater.UpdatePtFxPropeller(pVehicle, i, propBoneIndex, m_propellers[i].GetSpeed());
	}
}


//
//
//
static float s_subCarSteerModMult = 1.75f;

void CSubmarineHandling::UpdateRudder(CVehicle *pVehicle)
{
    float fSpeedSteerMod = 1.0f;
    float fSpeeedSteerModMult = 0.5f*pVehicle->pHandling->m_fTractionBiasFront;
    float fSteerAngle = m_fYawControl * pVehicle->pHandling->m_fSteeringLock;

	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		fSteerAngle = Clamp( fSteerAngle, -pVehicle->pHandling->m_fSteeringLock, pVehicle->pHandling->m_fSteeringLock );
		fSpeeedSteerModMult *= s_subCarSteerModMult;
	}

    Assert(fSpeeedSteerModMult < 1.0f && fSpeeedSteerModMult > 0.0f);
    fSpeedSteerMod = DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));

    if(fSpeedSteerMod > 1.0f)
    {
        fSpeedSteerMod = rage::Powf(fSpeeedSteerModMult, fSpeedSteerMod);
    }
    else
	{
        fSpeedSteerMod = 1.0f;
	}

    // rudder angle 
    float fDesiredRudderAngle = fSteerAngle * fSpeedSteerMod;
    m_fRudderAngle += rage::Clamp(fDesiredRudderAngle - m_fRudderAngle, -fwTimer::GetTimeStep() * ms_fControlSmoothSpeed, fwTimer::GetTimeStep() * ms_fControlSmoothSpeed);
}


//
//
//
void CSubmarineHandling::UpdateElevators(CVehicle *pVehicle)
{
    float fSpeedSteerMod = 1.0f;
    float fSpeeedSteerModMult = 0.5f*pVehicle->pHandling->m_fTractionBiasFront;
    float fSteerAngle = m_fPitchControl * pVehicle->pHandling->m_fSteeringLock;

	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		fSpeeedSteerModMult *= s_subCarSteerModMult;
	}

    Assert(fSpeeedSteerModMult < 1.0f && fSpeeedSteerModMult > 0.0f);
    fSpeedSteerMod = DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));

    if(fSpeedSteerMod > 1.0f)
    {
        fSpeedSteerMod = rage::Powf(fSpeeedSteerModMult, fSpeedSteerMod);
    }
    else
	{
        fSpeedSteerMod = 1.0f;
	}

    // elevator angle 
    float fDesiredElevatorAngle = fSteerAngle * fSpeedSteerMod;
    m_fElevatorAngle += rage::Clamp(fDesiredElevatorAngle - m_fElevatorAngle, -fwTimer::GetTimeStep() * ms_fControlSmoothSpeed, fwTimer::GetTimeStep() * ms_fControlSmoothSpeed);

}

void CSubmarineHandling::ApplyDeformationToBones(CVehicle *pVehicle, const void* basePtr)
{
	for(int i =0; i< m_iNumPropellers; i++)
	{
		m_propellers[i].ApplyDeformation(pVehicle ,basePtr);
	}
}

void CSubmarineHandling::Fix(CVehicle *pVehicle, bool UNUSED_PARAM(resetFrag))
{
	for(int i =0; i< m_iNumPropellers; i++)
	{
		m_propellers[i].Fix();
	}

	m_nAirLeaks = 0;
	pVehicle->m_nVehicleFlags.bIsDrowning = false;
	m_fMaxDepthReached = 0.0f;
}

#if DISPLAY_CRUSH_DEPTH_HELP_TEXT
void CSubmarineHandling::DisplayCrushDepthMessages(CVehicle *pVehicle)
{
	if(IsUnderCrushDepth(pVehicle))
	{
		CHelpMessage::SetMessageTextAndAddToBrief(HELP_TEXT_SLOT_STANDARD, "SUB_CRUSH_WARN_3");
		SetDisplayingCrushWarningFlag(true);
	}
	else if(IsUnderAirLeakDepth(pVehicle))
	{
		CHelpMessage::SetMessageTextAndAddToBrief(HELP_TEXT_SLOT_STANDARD, "SUB_CRUSH_WARN_2");
		SetDisplayingCrushWarningFlag(true);
	}
	else if(IsUnderDesignDepth(pVehicle))
	{
		CHelpMessage::SetMessageTextAndAddToBrief(HELP_TEXT_SLOT_STANDARD, "SUB_CRUSH_WARN_1");
		SetDisplayingCrushWarningFlag(true);
	}
	else
	{
		if(GetIsDisplayingCrushWarning())
		{
			CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, true);
			SetDisplayingCrushWarningFlag(false);
		}
	}
}
#endif // DISPLAY_CRUSH_DEPTH_HELP_TEXT

//////////////////////////////////////////////////////////////////////////
// Class CSubmarine
//
CSubmarine::CSubmarine(const eEntityOwnedBy ownedBy, u32 popType) : CVehicle(ownedBy, popType, VEHICLE_TYPE_SUBMARINE)
{
	m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;
	m_nVehicleFlags.bUnbreakableLights = true;
	m_nVehicleFlags.bUseDeformation = true;

	m_SubHandling.SetMinDepthToWreck( 0.8f );
}

//
//
//
void CSubmarine::InitCompositeBound()
{
	CVehicle::InitCompositeBound();

	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());

	// B*2009160: Allow the windscreen to have normal vehicle collision on subs. 
	// submersible2 has a glass bubble that is used as a windscreen and it sticks out from the main chassis quite a lot.
	if(GetBoneIndex(VEH_WINDSCREEN) > -1)
	{		
		int nChildIndex = GetVehicleFragInst()->GetComponentFromBoneIndex(GetBoneIndex(VEH_WINDSCREEN));
		if(nChildIndex != -1)
		{
			pBoundComp->SetIncludeFlags(nChildIndex, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
		}
	}
	//Make sure the exposed rudder on the Kosatka collides against stuff.
	if(GetBoneIndex(SUB_RUDDER2) > -1)
	{		
		int nChildIndex = GetVehicleFragInst()->GetComponentFromBoneIndex(GetBoneIndex(SUB_RUDDER2));
		if(nChildIndex != -1)
		{
			pBoundComp->SetIncludeFlags(nChildIndex, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
		}
	}
}

//
//
//
void CSubmarine::InitDoors()
{
	CVehicle::InitDoors();

	Assert(GetNumDoors()==0);
	m_pDoors = m_aSubDoors;
	m_nNumDoors = 0;

	float fOpenDoorAngle = 1.0f*PI;

	u32 uInitFlags = CCarDoor::AXIS_Y|CCarDoor::WILL_LOCK_SWINGING;

	//Apparently we don't want to break off doors on this massive sub  url:bugstar:6715420
	if( MI_SUB_KOSATKA.IsValid() && GetModelIndex() == MI_SUB_KOSATKA)
	{
		uInitFlags |= CCarDoor::DONT_BREAK;
	}

	if(GetBoneIndex(VEH_DOOR_DSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_F, 0.0f, fOpenDoorAngle, uInitFlags);
		m_nNumDoors++;
	}

	if(GetBoneIndex(VEH_DOOR_PSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_F, 0.0f, -fOpenDoorAngle, uInitFlags);
		m_nNumDoors++;
	}

	if(GetBoneIndex(VEH_DOOR_DSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_R, 0.0f, fOpenDoorAngle, uInitFlags);
		m_nNumDoors++;
	}

	if(GetBoneIndex(VEH_DOOR_PSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_R, 0.0f, -fOpenDoorAngle, uInitFlags);
		m_nNumDoors++;
	}

	if(GetBoneIndex(VEH_BOOT) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_BOOT, 0.0f, fOpenDoorAngle, uInitFlags);
		m_nNumDoors++;
	}
}

//
//
//
void CSubmarine::BlowUpCar(CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion)
{
	CVehicle::BlowUpCar(pCulprit);
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	// we can't blow up boats controlled by another machine
	// but we still have to change their status to wrecked
	// so the boat doesn't blow up if we take control of an
	// already blown up car
	if (IsNetworkClone())
	{
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);

		m_nPhysicalFlags.bRenderScorched = TRUE;  
		SetTimeOfDestruction();

		SetIsWrecked();

		// Switch off the engine. (For sound purposes)
		this->SwitchEngineOff(false);
		this->m_nVehicleFlags.bLightsOn = FALSE;

		g_decalMan.Remove(this);

		//Check to see that it is the player
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
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

	//	AddToMoveSpeedZ(0.13f);
	ApplyImpulseCg(Vector3(0.0f, 0.0f, 6.5f));

	SetWeaponDamageInfo(pCulprit, weaponHash, fwTimer::GetTimeInMilliseconds());

	//Set the destruction information.
	SetDestructionInfo(pCulprit, weaponHash);
	SetIsWrecked();

	this->m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off


	// do it before SpawnFlyingComponent() so this bit is propagated to all vehicle parts before they go flying:
	// let know pipeline, that we don't want any extra passes visible for this clump anymore:
	//	CVisibilityPlugins::SetClumpForAllAtomicsFlag((RpClump*)this->m_pRwObject, VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES); rage

	SetHealth(0.0f);	// Make sure this happens before AddExplosion or it will blow up twice

	KillPedsInVehicle(pCulprit, weaponHash);

	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_nVehicleFlags.bLightsOn = FALSE;

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);
	}

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);
}

//
//
//
bool CSubmarine::PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool UNUSED_PARAM(bJustSetCompression))
{
	Matrix34 matrix = MAT34V_TO_MATRIX34(GetMatrix());
	const int nInitialNumResults = 8;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<nInitialNumResults> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(matrix.d + HeightSampleRangeUp*ZAXIS, matrix.d - HeightSampleRangeDown*ZAXIS);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
	int nBestHit = -1;
	float fBestDeltaZ = LARGE_FLOAT;

	WorldProbe::ResultIterator it;
	int i = 0;
	for(it = probeResults.begin(); it < probeResults.end(); ++it, ++i)
	{
		if(it->GetHitDetected() && rage::Abs((it->GetHitPosition() - matrix.d).z) < fBestDeltaZ)
		{
			nBestHit = i;
			fBestDeltaZ = rage::Abs((it->GetHitPosition() - matrix.d).z);
		}
	}

	if(nBestHit > -1)
	{
		float fHeightAboveRoad = -GetBaseModelInfo()->GetBoundingBoxMin().z;
		if(GetVehicleFragInst() && GetFragmentComponentIndex(VEH_CHASSIS) > -1)
		{
			const phBoundComposite* pBoundComp = GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds();
			fHeightAboveRoad = -pBoundComp->GetBound(GetFragmentComponentIndex(VEH_CHASSIS))->GetBoundingBoxMax().GetZf();
		}
		matrix.d.z = probeResults[nBestHit].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN + GetHeightAboveRoad();
		SetMatrix(matrix, true, true, true);

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// Class CSubmarineCar
//
CSubmarineCar::CSubmarineCar(const eEntityOwnedBy ownedBy, u32 popType)	: CAutomobile(ownedBy, popType, VEHICLE_TYPE_SUBMARINECAR)
{
	m_InSubmarineMode	= false;
	m_AnimationPhase	= 0.0f;
	m_AnimationState	= State_Finished;
	m_PrevElevatorRotation = 0.0f;
	m_PrevYawControl = 0.0f;	
	m_SubHandling.SetMinDepthToWreck( 0.8f );
	m_LastEngineModeSwitchTime = 0u;
	m_RudderAngleClunkRetriggerValid = true;
	m_SetWheelsDown = true;
	m_SetWheelsUp	= false;
    m_UpdateCollision = false;
}

//
//
//
CSubmarineCar::~CSubmarineCar()
{	
}

ePrerenderStatus CSubmarineCar::PreRender( const bool bIsVisibleInMainViewport )
{ 
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(GetTransform().GetPosition()) );

	if( m_SetWheelsDown )
	{
        m_AnimationPhase = 0.0f;
		UpdateWheels( 0.0f );
		UpdateWheelCovers( 1.0f );
        UpdateSuspensionPositions();

        m_UpdateCollision = true;
        m_SetWheelsDown = false;
	}
	else if( m_SetWheelsUp )
	{
        m_AnimationPhase = 1.0f;
		UpdateWheels( 1.0f );
		UpdateWheelCovers( 0.0f );
        UpdateSuspensionPositions();

        m_UpdateCollision = true;
        m_SetWheelsUp = false;
	}

	if(!IsDummy())
	{
		// vehicle damage fx
		GetVehicleDamage()->PreRender();
	}

	float rudderAngle	= m_SubHandling.GetRudderAngle();
	float elevatorAngle = m_SubHandling.GetElevatorAngle();

	SetComponentRotation( SUBMARINECAR_RUDDER_1, ROT_AXIS_Z, rudderAngle, true );
	SetComponentRotation( SUBMARINECAR_RUDDER_2, ROT_AXIS_Z, rudderAngle, true );

	f32 yawControl = m_SubHandling.GetYawControl();	

	// If we're applying directional control and a) we weren't before or b) we changed direction, allow for a thunk sound
	if((Abs(yawControl) > 0.f && (m_PrevYawControl == 0 || (m_PrevYawControl != 0 && Sign(yawControl) != Sign(m_PrevYawControl)))))
	{
		m_RudderAngleClunkRetriggerValid = true;
	}

	// If we're waiting on a thunk and the yaw control is being applied, or we've released the yaw control after having it heavily applied, actually  play the thunk
	if((yawControl == 0 && Abs(m_PrevYawControl) > 0.f && Abs(rudderAngle) > 0.4f) || (Abs(yawControl) > 0.01f && m_RudderAngleClunkRetriggerValid))
	{		
		if(m_VehicleAudioEntity)
		{
			m_VehicleAudioEntity->TriggerSubmarineCarRudderTurnStart();
		}

		m_RudderAngleClunkRetriggerValid = false;
	}

	m_PrevYawControl = yawControl;

	float finalElevatorAngle = elevatorAngle - ( rudderAngle * 0.3f );
	const float maxElevatorAngle = pHandling->m_fSteeringLock;
	finalElevatorAngle = Clamp( finalElevatorAngle, -maxElevatorAngle, maxElevatorAngle );
	SetComponentRotation( SUBMARINECAR_ELEVATOR_L, ROT_AXIS_X, finalElevatorAngle, true );

	finalElevatorAngle = elevatorAngle + ( rudderAngle * 0.3f );
	finalElevatorAngle = Clamp( finalElevatorAngle, -maxElevatorAngle, maxElevatorAngle );
	SetComponentRotation( SUBMARINECAR_ELEVATOR_R, ROT_AXIS_X, finalElevatorAngle, true );
	
	if(Abs(m_PrevElevatorRotation) < (maxElevatorAngle * 0.95f) && Abs(finalElevatorAngle) >= (maxElevatorAngle * 0.95f))
	{
		if(m_VehicleAudioEntity)
		{
			m_VehicleAudioEntity->TriggerSubmarineCarWingFlapSound();
		}
	}

	m_PrevElevatorRotation = finalElevatorAngle;

	if( m_InSubmarineMode )
	{
		m_SubHandling.PreRenderPropellers( this );
	}

	switch( m_AnimationState )
	{
		// retract / extend the suspension if we're transforming
		case State_MoveWheelsUp:
		case State_MoveWheelsDown:
		{
			UpdateSuspensionPositions();
			// fall through to also update the steering angle
		}
		case State_MoveWheelCoversIn:
		case State_MoveWheelCoversOut:
		{
			UpdateSteeringAngle();
			break;
		}
		default:
		break;
	}

	return CAutomobile::PreRender( bIsVisibleInMainViewport );
}

void CSubmarineCar::PreRender2(const bool bIsVisibleInMainViewport)
{	
	// if we're animating the wheels we need to update their positions after the automobile prerender to made sure they're where we want them
	//if( ( m_wheelOffset > 0.0f && m_wheelOffset < ms_wheelUpOffset ) ||
	//	( !m_InSubmarineMode && m_wheelOffset != 0.0f ) )

	Vector3 bonePosition = VEC3_ZERO;
	float animationPhase = m_AnimationPhase;

	switch( m_AnimationState )
	{
		// retract / extend the suspension if we're transforming
		case State_MoveWheelCoversOut:
		{
			UpdateWheelCovers( 1.0f - animationPhase );
			
			// fall through so that the wheels are kept in the correct position
			animationPhase = 1.0f;
		}
		case State_MoveWheelsUp:
		{
			UpdateWheels( animationPhase );
			break;
		}

		case State_MoveWheelCoversIn:
		{
			UpdateWheelCovers( 1.0f - animationPhase );
			
			// fall through so that the wheels are kept in the correct position
			animationPhase = 1.0f;
		}
		case State_MoveWheelsDown:
		{
			UpdateWheels( animationPhase );
			break;
		}
		case State_Finished:
		{
			if( m_InSubmarineMode )
			{
				UpdateWheels( animationPhase );
				break;
			}
		}
		default:
			break;
	}

	m_SubHandling.PreRender2(this, bIsVisibleInMainViewport); 
	CAutomobile::PreRender2(bIsVisibleInMainViewport);
}

void CSubmarineCar::CheckForAudioModeSwitch(bool UNUSED_PARAM(isFocusVehicle) BANK_ONLY(, bool forceBoat))
{
	audVehicleAudioEntity* vehicleAudioEntity = m_VehicleAudioEntity;

	if(vehicleAudioEntity && vehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		audCarAudioEntity* carAudioEntity = (audCarAudioEntity*)vehicleAudioEntity;
		const bool inBoatMode = carAudioEntity->IsInAmphibiousBoatMode();
		const bool shouldBeInBoatMode = m_InSubmarineMode BANK_ONLY(|| forceBoat);
		bool modeSwitchValid = true;

		if(inBoatMode && !shouldBeInBoatMode)
		{
			modeSwitchValid = !IsInAir(true);
		}

		if(modeSwitchValid && inBoatMode != shouldBeInBoatMode)
		{
			if(fwTimer::GetTimeInMilliseconds() - m_LastEngineModeSwitchTime > 1000)
			{
				m_LastEngineModeSwitchTime = fwTimer::GetTimeInMilliseconds();
				carAudioEntity->SetInAmphibiousBoatMode(shouldBeInBoatMode);
			}
		}
	}
}

//
//
// physics
ePhysicsResult CSubmarineCar::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	if( CPhysics::GetIsLastTimeSlice( nTimeSlice ) )
	{
		UpdateAnimationPhase();
	}

    ePhysicsResult result = PHYSICS_DONE;

	if( !m_InSubmarineMode )
	{
		if( GetCollider() )
		{
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, true);
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
		}

		result = CAutomobile::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);
	}
	else
	{
		if( GetCollider() )
		{
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, false);
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
		}

        if( CPhysics::GetIsLastTimeSlice( nTimeSlice ) )
        {
            if( GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && (m_nVehicleFlags.bAnimateWheels==0) )
            {
                float fAnimWeight = 1.0f;
                float fSteerAngle = m_SubHandling.GetYawControl();

                if( GetDriver() )
                {
                    CTaskMotionBase *pCurrentMotionTask = GetDriver()->GetCurrentMotionTask();
                    if (pCurrentMotionTask && pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
                    {
                        const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pCurrentMotionTask);
                        fAnimWeight = pAutoMobileTask->GetSteeringWheelWeight();
                        fSteerAngle = pAutoMobileTask->GetSteeringWheelAngle();
                    }
                }

                SetComponentRotation( VEH_CAR_STEERING_WHEEL, ROT_AXIS_LOCAL_Y, -fSteerAngle * (GetVehicleModelInfo()->GetMaxSteeringWheelAngle() * DtoR) * fAnimWeight, true );
            }
        }

		if( HasRocketBoost() )
		{
			ProcessRocketBoost();
		}

        ProcessBoostPhysics( fTimeStep );
        result = m_SubHandling.ProcessPhysics(this, fTimeStep, bCanPostpone, nTimeSlice);

		//For the Toreador we need extra stabilization even after the rocket has ended to make sure the vehicle doesnt pitch up
		TUNE_INT(siMinTimeToKeepStabilisingPostRocketBoost, 1250, 0, 200000, 1);
		bool bPostBoostStabilisation = IsRocketBoosting() || (fwTimer::GetTimeInMilliseconds() < ( GetBoostToggleTime() + siMinTimeToKeepStabilisingPostRocketBoost ));

        if( GetIsInWater() && (CPhysics::ms_bInStuntMode || bPostBoostStabilisation) )
        {
            static dev_float sfExtraStablisationTorque = 2.0f;
            static dev_float sfMinVelocityForExtraStablisation = 100.0f;
            static dev_float sfMaxVelocityForExtraStablisation = 1600.0f;

            float velocity2 = GetVelocity().Mag2();

            if( velocity2 > sfMinVelocityForExtraStablisation )
            {
                float scaleForInput = 1.0f - Abs( m_SubHandling.GetPitchControl() );
                float extraTorque = sfExtraStablisationTorque * ( ( velocity2 - sfMinVelocityForExtraStablisation ) / ( sfMaxVelocityForExtraStablisation - sfMinVelocityForExtraStablisation ) );
                float pitchAmount = -GetTransform().GetB().GetZf();
                if( pitchAmount * m_SubHandling.GetPitchControl() < 0.0f )
                {
                    extraTorque *= scaleForInput;
                }
                extraTorque *= GetAngInertia().x;
                extraTorque *= pitchAmount;
                extraTorque /= fTimeStep;

                float currentMomentum = GetAngInertia().x * GetAngVelocity().Dot( VEC3V_TO_VECTOR3( GetTransform().GetRight() ) );
                extraTorque -= currentMomentum;

                Vector3 torque = VEC3V_TO_VECTOR3( GetTransform().GetA() ) * extraTorque;

                ApplyTorque( torque );
            }
        }
	}

    if( m_AnimationState != State_Finished ||
        m_UpdateCollision )
    {
        UpdateExtraCollision();
    }

    return result;
}

//
//
//
void CSubmarineCar::ProcessPostPhysics()
{
	CAutomobile::ProcessPostPhysics();
}

void CSubmarineCar::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{	
	m_SubHandling.DoProcessControl(this, fullUpdate, fFullUpdateTimeStep); 
	CAutomobile::DoProcessControl(fullUpdate, fFullUpdateTimeStep);			

	if( m_AnimationState != State_Finished )
	{
		float throttleRatio = 1.0f;

		if( m_AnimationState == State_MoveWheelsDown ||
			m_AnimationState == State_MoveWheelsUp )
		{
			throttleRatio = 1.0f - ( m_AnimationPhase );
		}

		float throttle = GetThrottle();
		throttle *= ( throttleRatio * throttleRatio );
		SetThrottle( throttle );
	}
}

//
//
//
bool CSubmarineCar::WantsToBeAwake()
{ 
	if( m_AnimationPhase > 0.0f )
	{
		return true;
	}

	if(GetIsInWater() || m_nFlags.bPossiblyTouchesWater)
	{
		return m_SubHandling.WantsToBeAwake(this); 
	}
	else
	{
		return CAutomobile::WantsToBeAwake(); 
	}
}

void CSubmarineCar::Fix( bool resetFrag, bool allowNetwork )
{	
	m_SubHandling.Fix(this, resetFrag);
	CAutomobile::Fix(resetFrag,allowNetwork);				

	m_SetWheelsUp = m_InSubmarineMode;
	m_SetWheelsDown = !m_SetWheelsUp;
}

bool CSubmarineCar::AreWheelsActive()
{
	// the wheels aren't active if we're in submarine mode
	if( m_InSubmarineMode )
	{
		return false;
	}

	// if we're not in submarine mode but the animation is complete 
	// then we must be fully in car mode and the wheels are active
	if( m_AnimationState == State_Finished )
	{
		return true;
	}

	// if we're moving the wheel covers in or out then the wheels can't be active.
	if( m_AnimationState == State_MoveWheelCoversIn )
	{
		return false;
	}
	if( m_AnimationState == State_MoveWheelCoversOut )
	{
		return false;
	}

	// if we're raising / lowering the wheels and they're nearly all the way up
	// then they can't be active
	if( m_AnimationPhase > 0.9f )
	{
		return false;
	}

	return true;
}

void CSubmarineCar::SetSubmarineMode( bool submarine, bool forceWheelPosition ) 
{ 
	if( forceWheelPosition ) 
	{
		m_SetWheelsDown = !submarine;
		m_SetWheelsUp	= submarine;

		SetAnimationState(State_Finished);
	}
	m_InSubmarineMode = submarine; 
}

void CSubmarineCar::SetTransformingToSubmarine(bool instantTransform) 
{ 
	if( m_AnimationState == State_MoveWheelCoversIn )
	{
		SetAnimationState(State_MoveWheelCoversOut, instantTransform); 
	}
	else if( m_AnimationState == State_MoveWheelsDown )
	{
		SetAnimationState(State_MoveWheelsUp, instantTransform); 
	}
	else
	{
		m_AnimationPhase = 0.0f;
		SetAnimationState(State_StartToSubmarine, instantTransform);
	}
}
void CSubmarineCar::SetTransformingToCar(bool instantTransform) 
{ 
	if( m_AnimationState == State_MoveWheelCoversOut )
	{
		SetAnimationState(State_MoveWheelCoversIn, instantTransform); 
	}
	else if( m_AnimationState == State_MoveWheelsUp )
	{
		SetAnimationState(State_MoveWheelsDown, instantTransform); 
	}
	else
	{
		m_AnimationPhase = 1.0f;
		SetAnimationState(State_StartToCar, instantTransform); 
	}
}

void CSubmarineCar::SetAnimationState(AnimationState newState, bool instantTransform)
{
	if(m_AnimationState != newState)
	{
		m_AnimationState = newState;

		if(m_VehicleAudioEntity && !instantTransform)
		{
			m_VehicleAudioEntity->OnSubmarineCarAnimationStateChanged(newState, m_AnimationPhase);
		}
	}	
}

static float s_moveCoverAnimSpeedModifier = 1.5f;

void CSubmarineCar::UpdateAnimationPhase()
{
	float maximumChange = fwTimer::GetTimeStep() * ms_animationSpeed;
	static dev_float wheelMovementScale = 1.0f;

	switch( m_AnimationState )
	{
		case State_MoveWheelsUp:
		{
			if( m_AnimationPhase >= 1.0f )
			{
				m_AnimationPhase -= 1.0f;
				SetAnimationState(State_MoveWheelCoversOut);
			}
			m_AnimationPhase += maximumChange * wheelMovementScale;
			break;
		}
		case State_MoveWheelCoversOut:
		{
			m_AnimationPhase += ( maximumChange * s_moveCoverAnimSpeedModifier );
			break;
		}
		case State_MoveWheelCoversIn:
		{
			if( m_AnimationPhase <= 0.0f )
			{
				m_AnimationPhase += 1.0f;
				SetAnimationState(State_MoveWheelsDown);
			}
			m_AnimationPhase -= ( maximumChange * s_moveCoverAnimSpeedModifier );
			break;
		}
		case State_MoveWheelsDown:
		{
			m_AnimationPhase -= maximumChange * wheelMovementScale;
			break;
		}
		default:
		{
			SetAnimationState(State_Finished);
		}
	}

	m_AnimationPhase = rage::Clamp( m_AnimationPhase, 0.0f, 1.0f );
}

void CSubmarineCar::UpdateSuspensionPositions()
{
	int	numberOfWheels = GetNumWheels();

	for( int i = 0; i < numberOfWheels; i++ )
	{
		CWheel* wheel = GetWheel( i );
		float offset = m_AnimationPhase * ms_wheelSuspensionOffset;
		if( wheel->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
		{
			offset *= ms_rearWheelUpFactor;
		}
		wheel->SetSuspensionRaiseAmount( -offset );
		wheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
		
		if( m_AnimationPhase > 0.0f )
		{
			wheel->SetDisableBottomingOut( true );
		}
		else
		{
			wheel->SetDisableBottomingOut( false );
		}
	}
}

void CSubmarineCar::UpdateSteeringAngle()
{
	// if we're in submarine mode, or transforming to it, we need to reduce the steering angle
	// so that in submarine mode the wheels are straight and don't clip through the car
	if( m_AnimationPhase != 0.0f )
	{
		int		numberOfWheels = GetNumWheels();
		float	steeringFactor = 1.0f - m_AnimationPhase;

		if( m_AnimationState == State_MoveWheelCoversOut || 
			m_AnimationState == State_MoveWheelCoversIn )
		{
			steeringFactor = 0.0f;
		}

		for( int i = 0; i < numberOfWheels; i++ )
		{			
			CWheel* wheel = GetWheel( i );

			if( wheel->GetConfigFlags().IsFlagSet(WCF_STEER) )
			{
				float steeringAngle	= wheel->GetSteeringAngle();

				if( steeringAngle != 0.0f )
				{
					steeringAngle *= steeringFactor;
					wheel->SetSteerAngle( steeringAngle );
				}
			}
		}
	}
}

static float s_animStageForCoverZOffset = 0.3f;
void CSubmarineCar::UpdateWheelCovers( float animationPhase )
{
	Vector3 bonePosition = VEC3_ZERO;
	float wheelInset	= animationPhase * ms_wheelCoverInset;
	float zOffset		= 0.0f;
	float coverAngle	= ( animationPhase * 3.0f ) * ms_wheelCoverRotation;

	coverAngle = Min( coverAngle, ms_wheelCoverRotation );

	if( animationPhase > s_animStageForCoverZOffset )
	{
		zOffset = ( ( animationPhase - s_animStageForCoverZOffset ) / ( 1.0f - s_animStageForCoverZOffset ) ) * ms_wheelCoverZOffset;
	}

	for( int i = 0; i < 2; i++ )
	{
		eHierarchyId compId	= (eHierarchyId)( (int)SUBMARINECAR_WHEELCOVER_L + i );
		int	nBoneIndex		= GetBoneIndex((eHierarchyId)compId);
		Matrix34& boneMat	= GetLocalMtxNonConst( nBoneIndex );

		const crBoneData* pBoneData = GetSkeletonData().GetBoneData(nBoneIndex);
		Vec3V defaultTranslation	= pBoneData->GetDefaultTranslation();

		bonePosition = VEC3V_TO_VECTOR3( defaultTranslation );

		if( i == 1 )
		{
			bonePosition.SetX( bonePosition.GetX() + wheelInset );
		}
		else
		{
			bonePosition.SetX( bonePosition.GetX() - wheelInset );
		}

		bonePosition.SetZ( bonePosition.GetZ() - zOffset );//( animationPhase * ms_wheelCoverZOffset ) );

		boneMat.FromQuaternion( RCC_QUATERNION( pBoneData->GetDefaultRotation() ) );
		
		boneMat.d.Set( bonePosition );

		if( i == 1 )
		{
			boneMat.RotateY( coverAngle );
		}
		else
		{
			boneMat.RotateY( -coverAngle );
		}

		GetSkeleton()->PartialUpdate( nBoneIndex );
	}
}

void CSubmarineCar::UpdateWheels( float animationPhase )
{
	Matrix34 newBoneMatrix;

	for( int i = 0; i < GetNumWheels(); i++ )
	{
		eHierarchyId compId	= (eHierarchyId)( (int)VEH_WHEEL_LF + i );
		CWheel*	wheel		= GetWheel( i );
		int	nBoneIndex		= GetBoneIndex((eHierarchyId)compId);
		Matrix34& boneMat	= GetLocalMtxNonConst( nBoneIndex );

		const crBoneData* pBoneData = GetSkeletonData().GetBoneData(nBoneIndex);
	
		// Get default matrix
		newBoneMatrix.FromQuaternion( RCC_QUATERNION( pBoneData->GetDefaultRotation() ) );
		Vec3V defaultTranslation	= pBoneData->GetDefaultTranslation();
		newBoneMatrix.d = VEC3V_TO_VECTOR3(defaultTranslation);

		if( wheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) )
		{
			newBoneMatrix.d.SetX( newBoneMatrix.d.GetX() + ( animationPhase * ms_wheelInset ) );
		}
		else
		{
			newBoneMatrix.d.SetX( 	newBoneMatrix.d.GetX() + ( animationPhase * -ms_wheelInset ) );
		}

		float minOffset = animationPhase * ms_wheelUpOffset;
		float maxOffset = ms_wheelUpOffset;

        if( m_SetWheelsDown )
        {
            maxOffset *= animationPhase;
        }

		if( wheel->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
		{
			maxOffset *= ms_rearWheelUpFactor;
			minOffset *= ms_rearWheelUpFactor;
		}

		float currentOffset = boneMat.d.GetZ() - defaultTranslation.GetZf();

		if( minOffset > currentOffset )
		{
			minOffset -= ( minOffset - currentOffset ) * ( 1.0f - animationPhase );
		}

		currentOffset = Clamp( currentOffset, minOffset, maxOffset );
		newBoneMatrix.d.SetZ( defaultTranslation.GetZf() + currentOffset );

		if(MI_CAR_TOREADOR.IsValid() && GetModelIndex() == MI_CAR_TOREADOR)
		{	
			TUNE_FLOAT(sfToreadorSubmarineCarWheelRotation, 90.0f, 0.0f, 100.0f,0.1f)
			float fWheelTiltAmount = animationPhase * sfToreadorSubmarineCarWheelRotation;

			float fTransformDirection = ( wheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) ? 1.0f : -1.0f );
			newBoneMatrix.RotateLocalY(fWheelTiltAmount * fTransformDirection * DtoR);
		}

		boneMat.Set(newBoneMatrix);

		GetSkeleton()->PartialUpdate( nBoneIndex );

		//Just hide the hub when transforming, otherwise it clips.
		TUNE_FLOAT(sfSubmarineCarAnimationPhaseToHideHub, 0.05f, 0.0f, 1.0f,0.01f)
		if(animationPhase <= sfSubmarineCarAnimationPhaseToHideHub)
		{
			wheel->GetConfigFlags().ClearFlag(WCF_DONT_RENDER_HUB);
		}
		else
		{
			wheel->GetConfigFlags().SetFlag(WCF_DONT_RENDER_HUB);
		}

	}
}

const int SUBMARINECAR_BONE_COUNT_TO_DEFORM = 8;
const eHierarchyId ExtraSubmarineCarBones[SUBMARINECAR_BONE_COUNT_TO_DEFORM] = { SUBMARINECAR_PROPELLER_1, SUBMARINECAR_PROPELLER_2, SUBMARINECAR_RUDDER_1, SUBMARINECAR_RUDDER_2, SUBMARINECAR_ELEVATOR_L, SUBMARINECAR_ELEVATOR_R, };

const eHierarchyId* CSubmarineCar::GetExtraBonesToDeform(int& extraBoneCount)
{
	extraBoneCount = SUBMARINECAR_BONE_COUNT_TO_DEFORM;
	return ExtraSubmarineCarBones;
}

void CSubmarineCar::ProcessPreComputeImpacts(phContactIterator impacts)
{
	if( GetIsInWater() &&
		m_InSubmarineMode )
	{
		impacts.Reset();

		while(!impacts.AtEnd())
		{
			phInst* pOtherInst = impacts.GetOtherInstance();
			CEntity* pOtherEntity = CPhysics::GetEntityFromInst( pOtherInst );
		
			if( !pOtherEntity ||
				pOtherEntity->GetIsTypeBuilding() )
			{
				static dev_float sfFrictionMultiplier = 0.1f;

				float friction = impacts.GetFriction();
				impacts.SetFriction( friction * sfFrictionMultiplier );
			}

			impacts++;
		}
	}

	impacts.Reset();
	CVehicle::ProcessPreComputeImpacts(impacts);

}

void CSubmarineCar::UpdateExtraCollision()
{
    const s32 EXTRA_SUBMARINE_CAR_BONES = 3;
    static eHierarchyId aSubmarineCarBones[ EXTRA_SUBMARINE_CAR_BONES ] = 
    {
        SUBMARINECAR_FIN_MOUNT,
        SUBMARINECAR_DRIVE_PLANE_L,
        SUBMARINECAR_DRIVE_PLANE_R
    };

    if( fragInstGta* pFragInst = GetVehicleFragInst() )
    {
        m_UpdateCollision = false;

        if( m_AnimationState == State_StartToSubmarine &&
           m_AnimationPhase == 0.0f )
        {
            pFragInst->ForceArticulatedColliderMode();
        }

        if( fragPhysicsLOD* physicsLOD = pFragInst->GetTypePhysics() )
        {
            phArticulatedCollider* pCollider = nullptr;

            if( pFragInst->GetCacheEntry() &&
                pFragInst->GetCacheEntry()->GetHierInst() )
            {
                pCollider = pFragInst->GetCacheEntry()->GetHierInst()->articulatedCollider;	
            }

            if( pCollider )
            {
                for( int i = 0; i < EXTRA_SUBMARINE_CAR_BONES; i++ )
                {
                    int boneIndex = GetBoneIndex( aSubmarineCarBones[ i ] );

                    if( physicsLOD &&
                        boneIndex != -1 )
                    {
                        int group = pFragInst->GetGroupFromBoneIndex( boneIndex );

                        if( group > -1 )
                        {
                            fragTypeGroup* pGroup = physicsLOD->GetGroup( group );

                            phBoundComposite* pBound = (phBoundComposite*)pFragInst->GetArchetype()->GetBound();
                            int childFragmentIndex = pGroup->GetChildFragmentIndex();

                            const Matrix34 &matrixParentBone = GetObjectMtx( boneIndex );
                            Matrix34 matLink = matrixParentBone;
                            matLink.d -= VEC3V_TO_VECTOR3( pBound->GetCGOffset() );

                            for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
                            {	
                                pBound->SetCurrentMatrix( childFragmentIndex + iChild, MATRIX34_TO_MAT34V( matrixParentBone ) );
                                // pBound->SetLastMatrix( childFragmentIndex + iChild, MATRIX34_TO_MAT34V( matrixParentBone ) );

                                // Keep articulated collider link attachments in sync.
                                if( pCollider )
                                {
                                    if( Matrix34 *pMat = (Matrix34 *)pCollider->GetLinkAttachmentMatrices() )
                                    {
                                        pMat[ childFragmentIndex + iChild ] = matLink;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
