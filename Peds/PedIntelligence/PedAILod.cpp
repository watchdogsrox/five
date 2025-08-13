// FILE :    PedAILod.cpp
// PURPOSE : Controls the LOD of the peds AI update.
// CREATED : 30-09-2009

// class header
#include "Peds/PedIntelligence/PedAiLod.h"

// Rage headers
#include "grcore/debugdraw.h"

// Game headers
#include "ai/stats.h"
#include "Peds/Ped.h"
#include "Peds/ped_channel.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Task/Motion/TaskMotionBase.h"
#include "task/Vehicle/TaskRideTrain.h"
#include "Vehicles/train.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

using namespace AIStats;

#if __BANK
const char* CPedAILod::ms_aLodFlagNames[] =
{
	"EventScanning",				// AL_LodEventScanning
	"MotionTask",					// AL_LodMotionTask
	"Physics",						// AL_LodPhysics
	"Navigation",					// AL_LodNavigation
	"EntityScanning",				// AL_LodEntityScanning
	"RagdollInVehicle",				// AL_LodRagdollInVehicle
	"TimesliceIntelligenceUpdate",	// AL_LodTimesliceIntelligenceUpdate
	"TimesliceAnimUpdate"			// AL_LodTimesliceAnimUpdate
};

s32 CPedAILod::ms_iDebugFlagIndex = 0;
#endif // __BANK

CPedAILod::CPedAILod()
: m_pOwner(NULL)
, m_LodFlags(AL_DefaultLodFlags)
, m_BlockedLodFlags(AL_DefaultMissionBlockedLodFlags)
, m_fBlockMotionLodChangeTimer(0.f)
, m_fTimeSinceLastAnimUpdate(0.0f)
, m_uNextTimeToCheckSwitchToFullPhysicsMS(0)
, m_AiScoreOrder(255)
, m_AnimUpdatePeriod(1)
, m_bMaySkipVisiblePositionUpdatesInLowLod(false)
, m_bTaskSetNeedIntelligenceUpdate(false)
, m_AnimUpdatePhase(0)
, m_NavmeshTrackerUpdate_OnScreenSkipCount(0)
, m_NavmeshTrackerUpdate_OffScreenSkipCount(0)
, m_bForceNoTimesliceIntelligenceUpdate(false)
, m_bUnconditionalTimesliceIntelligenceUpdate(false)
{
}

CPedAILod::~CPedAILod()
{
}

void CPedAILod::ResetAllLodFlags()
{
	SetLodFlags(CPedAILod::AL_DefaultLodFlags);
	SetBlockedLodFlags(CPedAILod::AL_DefaultMissionBlockedLodFlags);
	m_NavmeshTrackerUpdate_OnScreenSkipCount = 0;
	m_NavmeshTrackerUpdate_OffScreenSkipCount = 0;
	m_bForceNoTimesliceIntelligenceUpdate = false;
	m_bUnconditionalTimesliceIntelligenceUpdate = false;
	m_bTaskSetNeedIntelligenceUpdate = false;
}

// Process a change in the flags
void CPedAILod::ProcessLodChange(const s32 iOldFlags, const s32 iNewFlags)
{
	pedFatalAssertf(m_pOwner, "Ped AI lod owner not set!");

	// Toggle the ped using physics
	if( (iOldFlags & AL_LodPhysics) != (iNewFlags & AL_LodPhysics) )
	{
		m_pOwner->SetUsingLowLodPhysics((iNewFlags & AL_LodPhysics) != 0);
	}

	// Toggle the ped using the low lod navigation
	if( (iOldFlags & AL_LodNavigation) != (iNewFlags & AL_LodNavigation) )
	{
		//m_pOwner->SetUsingLowLodNavigation((iNewFlags & AL_LodNavigation) != 0);
	}

	// Toggle the motion task
	if( (iOldFlags & AL_LodMotionTask) != (iNewFlags & AL_LodMotionTask) )
	{
		m_pOwner->SetUsingLowLodMotionTask((iNewFlags & AL_LodMotionTask) != 0);

		// Changed state, reset timer
		static dev_float BLOCK_MOTION_TASK_LOD_CHANGE = 0.5f;
		m_fBlockMotionLodChangeTimer = BLOCK_MOTION_TASK_LOD_CHANGE;
	}

	// Toggle the motion task
	if( (iOldFlags & AL_LodEventScanning) != (iNewFlags & AL_LodEventScanning) )
	{
		// Don't want stale list of nearby fires
		m_pOwner->GetPedIntelligence()->GetEventScanner()->GetFireScanner().ClearNearbyFires();
	}
}

bool CPedAILod::CanChangeLod() const
{
	pedFatalAssertf( m_pOwner, "Ped AI lod owner not set!" );
	if( m_pOwner->IsLocalPlayer() )
	{
		return false;
	}

	return true;
}

// NAME : IsSafeToSwitchToFullPhysics
// PURPOSE : Detect whether ped in intersecting any nearby objects or vehicles
bool CPedAILod::IsSafeToSwitchToFullPhysics()
{
	PF_FUNC(IsSafeToSwitchToFullPhysics);

	// We currently only test for intersection vs the closest object/vehicle.
	// However - a ped may feasibly be contained with some large entity B, whilst actually
	// being closer to the origin of entity A.  These rare cases will be missed.  If bugs
	// are still found with peds converting to real inside objects/vehicles, then we can
	// try setting 'bOnlyClosest' to false, to test intersection with all nearby entities.

	static dev_bool bOnlyClosest = true;

	// We don't want to leave a ragdolling ped in low lod physics mode.
	if (m_pOwner && m_pOwner->GetUsingRagdoll())
		return true;

	// Check if we are not yet permitted to repeat these checks for performance
	if( fwTimer::GetTimeInMilliseconds() < m_uNextTimeToCheckSwitchToFullPhysicsMS )
	{
		return false;
	}

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pOwner->GetTransform().GetPosition());
	const CBaseCapsuleInfo * pCapsule = m_pOwner->GetCapsuleInfo();
	const float fPedBase = vPedPos.z - pCapsule->GetGroundToRootOffset();
	const float fPedTop = vPedPos.z + pCapsule->GetGroundToRootOffset();
	const float fPedRadius = pCapsule->GetHalfWidth();

	CEntityScannerIterator objects = m_pOwner->GetPedIntelligence()->GetNearbyObjects();
	CObject * pObject = (CObject*)objects.GetFirst();

	while(pObject)
	{
		const bool bIgnore =
			pObject->m_nObjectFlags.bAmbientProp ||
			pObject->m_nObjectFlags.bDetachedPedProp ||
			pObject->m_nObjectFlags.bIsPickUp ||
			pObject->m_nObjectFlags.bCanBePickedUp ||
			pObject->m_nObjectFlags.bIsHelmet ||
			(pObject->GetCurrentPhysicsInst()==NULL) ||
            (NetworkInterface::IsGameInProgress() && NetworkInterface::IsRemotePlayerStandingOnObject(*m_pOwner, *pObject));
			
		if(!bIgnore)
		{
			CEntityBoundAI boundAI(*pObject, vPedPos.z, fPedRadius, true);

			if((fPedBase > boundAI.GetTopZ() || boundAI.GetBottomZ() > fPedTop)==false)
			{
				if(boundAI.LiesInside(vPedPos))
				{
					// set next time we may repeat checks, for performance
					m_uNextTimeToCheckSwitchToFullPhysicsMS = fwTimer::GetTimeInMilliseconds() + kRepeatSafeToSwitchCheckDelayMS;
					return false;
				}
			}
		}

		pObject = bOnlyClosest ? NULL : (CObject*) objects.GetNext();
	}

	//Exclude trains we are riding in.
	const CVehicle* pExcludeVehicle = NULL;
	const CTask* pTaskActive = m_pOwner->GetPedIntelligence()->GetTaskActive();
	if(pTaskActive && (pTaskActive->GetTaskType() == CTaskTypes::TASK_RIDE_TRAIN))
	{
		pExcludeVehicle = static_cast<const CTaskRideTrain *>(pTaskActive)->GetTrain();
	}

	CEntityScannerIterator vehicles = m_pOwner->GetPedIntelligence()->GetNearbyVehicles();
	CVehicle * pVehicle = (CVehicle*)vehicles.GetFirst();

	while(pVehicle)
	{
        bool bExcludeVehicle = (pVehicle == pExcludeVehicle) || (m_pOwner->GetIsInVehicle() && m_pOwner->GetMyVehicle() == pVehicle);

        if(!bExcludeVehicle && NetworkInterface::IsGameInProgress())
        {
            bExcludeVehicle = NetworkInterface::IsRemotePlayerStandingOnObject(*m_pOwner, *pVehicle);
        }

        if(!bExcludeVehicle)
        {
		    CEntityBoundAI boundAI(*pVehicle, vPedPos.z, fPedRadius, true);

            // adjust the top of the bounding box to prevent false hits with peds standing on top of objects
            float fAdjustedTop = boundAI.GetTopZ() - 0.5f;

		    if((fPedBase > fAdjustedTop || boundAI.GetBottomZ() > fPedTop)==false)
		    {
			    if(boundAI.LiesInside(vPedPos))
			    {
					// set next time we may repeat checks, for performance
					m_uNextTimeToCheckSwitchToFullPhysicsMS = fwTimer::GetTimeInMilliseconds() + kRepeatSafeToSwitchCheckDelayMS;
				    return false;
			    }
		    }
        }

		pVehicle = bOnlyClosest ? NULL : (CVehicle*) vehicles.GetNext();
	}

	CEntityScannerIterator peds = m_pOwner->GetPedIntelligence()->GetNearbyPeds();
	CPed * pOtherPed = (CPed*)peds.GetFirst();

	while(pOtherPed)
	{
        bool bIgnorePed = NetworkInterface::IsASpectatingPlayer(pOtherPed);

        if(!bIgnorePed)
        {
		    const Vector3 vOtherPedPos = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());
		    const Vector3 vDiff = vOtherPedPos - vPedPos;
		    const float fOtherPedRadius = pOtherPed->GetCapsuleInfo()->GetHalfWidth();
		    if( Abs(vDiff.z) < 3.0f && vDiff.XYMag2() < square(fPedRadius+fOtherPedRadius) )
		    {
				// set next time we may repeat checks, for performance
				m_uNextTimeToCheckSwitchToFullPhysicsMS = fwTimer::GetTimeInMilliseconds() + kRepeatSafeToSwitchCheckDelayMS;
			    return false;
		    }
        }

		pOtherPed = bOnlyClosest ? NULL : (CPed*) peds.GetNext();
	}

	//--------------------------------------------------------------------------------------------------------
	// We need to test again the physics world as well, to fix issues where a ped has strayed into collision
	// whilst in low LOD.
	// See (url:bugstar:1074544) - it seems to be possible that peds with large turning circles will stray
	// from their routes and possibly end up intersecting map collision.
	// I would like to enable this test across the board for all peds, but I don't think we can justify the
	// cost - so I am activating it only for the quad animals.

	if( m_pOwner->GetPedType()==PEDTYPE_ANIMAL && m_pOwner->GetPrimaryMotionTask() &&
		(m_pOwner->GetPrimaryMotionTask()->GetTaskType()==CTaskTypes::TASK_ON_FOOT_QUAD ||
		m_pOwner->GetPrimaryMotionTask()->GetTaskType()==CTaskTypes::TASK_ON_FOOT_HORSE ||
		m_pOwner->GetPrimaryMotionTask()->GetTaskType()==CTaskTypes::TASK_ON_FOOT_DEER) )
	{
		WorldProbe::CShapeTestSphereDesc sphereDesc;
		sphereDesc.SetSphere(vPedPos, m_pOwner->GetCapsuleInfo()->GetHalfWidth());
		sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
		sphereDesc.SetContext(WorldProbe::LOS_GeneralAI);

		WorldProbe::CShapeTestFixedResults<> sphereResult;
		sphereDesc.SetResultsStructure(&sphereResult);

		if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
		{
			// set next time we may repeat checks, for performance
			m_uNextTimeToCheckSwitchToFullPhysicsMS = fwTimer::GetTimeInMilliseconds() + kRepeatSafeToSwitchCheckDelayMS;
			return false;
		}
	}

	return true;
}

#if __BANK
void CPedAILod::DebugRender() const
{
	char debugText[50];
	Vector3 vTextPos = VEC3V_TO_VECTOR3(m_pOwner->GetTransform().GetPosition()) + ZAXIS;
	s32 iNoOfTexts = 0;

	for( s32 i = 0; i < ms_iMaxFlagNames; i++ )
	{
		if(ms_aLodFlagNames[i])
		{
			formatf( debugText, "%s : %s%s", ms_aLodFlagNames[i], IsLodFlagSet(1<<i) ? "Low LOD" : "High LOD", IsBlockedFlagSet(1<<i) ? " (blocked)" : "");
			grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts * grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;
		}
	}

	// Debug the navmesh position tracking
#if __TRACK_PEDS_IN_NAVMESH
	if( m_pOwner->GetNavMeshTracker().GetIsValid() )
	{
		Vec3V vTrackedPos = RCC_VEC3V(m_pOwner->GetNavMeshTracker().GetLastNavMeshIntersection());
		const Color32 iCol = m_pOwner->GetNavMeshTracker().IsUpToDate( VEC3V_TO_VECTOR3(m_pOwner->GetTransform().GetPosition()) ) ? Color_green : Color_red;
		grcDebugDraw::Cross( vTrackedPos, 0.5f, iCol );
		grcDebugDraw::Line( vTrackedPos, RCC_VEC3V(m_pOwner->GetNavMeshTracker().GetLastNavMeshIntersection()), iCol );
	}
#endif // __TRACK_PEDS_IN_NAVMESH
}

void CPedAILod::AddDebugWidgets(bkBank& bank)
{
	bank.PushGroup( "Manual control", false );

	// Work out how many consecutive strings there are
	s32 iFlagNames = 0;
	for( s32 i = 0; i < ms_iMaxFlagNames; i++ )
	{
		if( !ms_aLodFlagNames[i] )
		{
			break;
		}
		iFlagNames++;
	}

	bank.AddCombo( "Flag:", &ms_iDebugFlagIndex, iFlagNames, ms_aLodFlagNames );
	bank.AddButton( "Toggle focus ped flag", ToggleFocusPedDebugFlag );
	bank.AddButton( "Clear focus ped flag", ClearFocusPedDebugFlag );
	bank.AddButton( "Set all peds to low LOD flag", SetAllPedsToLowLodDebugFlag );
	bank.AddButton( "Set all peds to high LOD flag", SetAllPedsToHighLodDebugFlag );
	bank.AddButton( "Clear all ped flag overrides", ClearAllPedsDebugFlags );

	bank.PopGroup(); // "Manual control"
}
#endif // __BANK

#if __BANK
void CPedAILod::ToggleFocusPedDebugFlag()
{
	CPed* pPed = CPedDebugVisualiserMenu::GetFocusPed();
	if( pPed )
	{
		s32 iFlag = (1<<ms_iDebugFlagIndex);
		if( pPed->GetPedAiLod().m_DebugLowLodFlags.IsFlagSet( iFlag ) )
		{
			pPed->GetPedAiLod().m_DebugLowLodFlags.ClearFlag( iFlag );
			pPed->GetPedAiLod().m_DebugHighLodFlags.SetFlag( iFlag );
		}
		else
		{
			pPed->GetPedAiLod().m_DebugLowLodFlags.SetFlag( iFlag );
			pPed->GetPedAiLod().m_DebugHighLodFlags.ClearFlag( iFlag );
		}
	}
}

void CPedAILod::ClearFocusPedDebugFlag()
{
	CPed* pPed = CPedDebugVisualiserMenu::GetFocusPed();
	if( pPed )
	{
		s32 iFlag = (1<<ms_iDebugFlagIndex);
		pPed->GetPedAiLod().m_DebugLowLodFlags.ClearFlag( iFlag );
		pPed->GetPedAiLod().m_DebugHighLodFlags.ClearFlag( iFlag );
	}
}

void CPedAILod::SetAllPedsToLowLodDebugFlag()
{
	CPed::Pool* pPool = CPed::GetPool();
	for( s32 i = 0; i < pPool->GetSize(); i++ )
	{
		CPed* pPed = pPool->GetSlot(i);
		if( pPed )
		{
			pPed->GetPedAiLod().m_DebugLowLodFlags.SetFlag( (1<<ms_iDebugFlagIndex) );
			pPed->GetPedAiLod().m_DebugHighLodFlags.ClearFlag( (1<<ms_iDebugFlagIndex) );
		}
	}
}

void CPedAILod::SetAllPedsToHighLodDebugFlag()
{
	CPed::Pool* pPool = CPed::GetPool();
	for( s32 i = 0; i < pPool->GetSize(); i++ )
	{
		CPed* pPed = pPool->GetSlot(i);
		if(pPed)
		{
			pPed->GetPedAiLod().m_DebugLowLodFlags.ClearFlag( (1<<ms_iDebugFlagIndex) );
			pPed->GetPedAiLod().m_DebugHighLodFlags.SetFlag( (1<<ms_iDebugFlagIndex) );
		}
	}
}

void CPedAILod::ClearAllPedsDebugFlags()
{
	CPed::Pool* pPool = CPed::GetPool();
	for( s32 i = 0; i < pPool->GetSize(); i++ )
	{
		CPed* pPed = pPool->GetSlot(i);
		if(pPed)
		{
			pPed->GetPedAiLod().m_DebugLowLodFlags.ClearAllFlags();
			pPed->GetPedAiLod().m_DebugHighLodFlags.ClearAllFlags();
		}
	}
}
#endif // __BANK
