// Filename   :	TaskNMJumpRollFromRoadVehicle.cpp
// Description:	Natural Motion jump and roll from a road vehicle class (FSM version)
 
// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "audio/scriptaudioentity.h"
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "vehicles/vehicle.h"
#include "Vehicles\Metadata\VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMJumpRollFromRoadVehicle::Tunables CTaskNMJumpRollFromRoadVehicle::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMJumpRollFromRoadVehicle, 0x0c26ef4c);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CClonedNMJumpRollInfo
//////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedNMJumpRollInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMJumpRollFromRoadVehicle(10000, 10000, m_bExtraGravity);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMJumpRollFromRoadVehicle
//////////////////////////////////////////////////////////////////////////



CTaskNMJumpRollFromRoadVehicle::CTaskNMJumpRollFromRoadVehicle(u32 nMinTime, u32 nMaxTime, bool extraGravity, bool blendOutQuickly, atHashString entryPointAnimInfo, CVehicle* pVehicle)
: CTaskNMBehaviour(nMinTime, nMaxTime)
, m_EntryPointAnimInfoHash(entryPointAnimInfo)
, m_pVehicle(pVehicle)
, m_DisableZBlending(false)
{
	m_ExtraGravity = extraGravity;
	m_BlendOutQuickly = blendOutQuickly;

	m_vecInitialVelocity.Zero();
	SetInternalTaskType(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE);
}

CTaskNMJumpRollFromRoadVehicle::~CTaskNMJumpRollFromRoadVehicle()
{
}

bool CTaskNMJumpRollFromRoadVehicle::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	return true;
}

void CTaskNMJumpRollFromRoadVehicle::StartBehaviour(CPed* pPed)
{
	// we have to use the root rather than the entity position here, because the entity matrix can be offset significantly from the 
	// ped while the ragdoll is simulating, depending on the animation at the point when it first activated.
	pPed->GetBonePosition(m_vecInitialPosition, BONETAG_ROOT);
	
	// prefer the injured getup when jumping from vehicles
	if (!m_BlendOutQuickly)
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup, true);

	g_ScriptAudioEntity.RegisterThrownFromVehicle(pPed);

	CStatsMgr::StartBailVehicle(pPed);
}

void CTaskNMJumpRollFromRoadVehicle::AddStartMessages(CPed* UNUSED_PARAM(pPed), CNmMessageList& UNUSED_PARAM(list))
{
	AddTuningSet(&sm_Tunables.m_Start);

	if (m_EntryPointAnimInfoHash.GetHash()!=0)
	{
		const CVehicleEntryPointAnimInfo* pInfo = CVehicleMetadataMgr::GetVehicleEntryPointAnimInfo(m_EntryPointAnimInfoHash);

		if (pInfo)
		{
			if (pInfo->GetNMJumpFromVehicleTuningSet().GetHash()!=0)
			{
				CNmTuningSet* pEntryPointSet = sm_Tunables.m_EntryPointSets.Get(pInfo->GetNMJumpFromVehicleTuningSet());
				taskAssertf(pEntryPointSet, "Nm jump from vehicle tuning set does not exist! VehicleEntryPointAnimInfo:%s, Set:%s", pInfo->GetName().GetCStr(), pInfo->GetNMJumpFromVehicleTuningSet().TryGetCStr() ? pInfo->GetNMJumpFromVehicleTuningSet().GetCStr() : "UNKNOWN");

				if(pEntryPointSet)
				{
					AddTuningSet(pEntryPointSet);
				}
			}
		}
	}
}

void CTaskNMJumpRollFromRoadVehicle::ControlBehaviour(CPed* pPed)
{
	// Ensure that the ped doesn't bounce into the air (like when bailing from very fast vehicles)
	phArticulatedCollider* pCollider = dynamic_cast<phArticulatedCollider*>(CPhysics::GetSimulator()->GetCollider(pPed->GetRagdollInst()));
	static float fMinSpeedForExtraGravity = 3.0f;
	if(m_ExtraGravity && pCollider && pPed->GetVelocity().Mag2() > fMinSpeedForExtraGravity*fMinSpeedForExtraGravity)
	{
		// Find the distance above the initial jump "plane"
		Vector3 toPed;
		pPed->GetBonePosition(toPed, BONETAG_ROOT);
		toPed -= m_vecInitialPosition;
		Vector3 vUp = Vector3(0.0f,0.0f,1.0f);
		float distAbovePlane = toPed.Dot(vUp);
		if(distAbovePlane > sm_Tunables.m_StartForceDownHeight)
		{
			pCollider->GetBody()->ApplyGravityToLinks(vUp * (-sm_Tunables.m_GravityScale) * distAbovePlane, ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), pCollider->GetGravityFactor());
		}
	}
}

bool CTaskNMJumpRollFromRoadVehicle::HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId)
{
	// Handle impacts with the vehicle we're ejecting from
	if (pEntity != NULL && pEntity == m_pVehicle)
	{
		return true;
	}

	return CTaskNMBehaviour::HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId);
}

bool CTaskNMJumpRollFromRoadVehicle::FinishConditions(CPed* pPed)
{
	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_VEL_CHECK | FLAG_RELAX_AP_LOW_HEALTH, m_BlendOutQuickly ? &sm_Tunables.m_QuickBlendOut.PickBlendOut(pPed) : &sm_Tunables.m_BlendOut.PickBlendOut(pPed));
}
