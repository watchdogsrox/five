// Filename   :	TaskNMBuoyancy.cpp
// Description:	GTA-side of the NM Buoyancy behavior

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragmentnm\messageparams.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "Peds/ped.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Physics/TaskNMBuoyancy.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/NmDebug.h"
#include "Renderer/Water.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMBuoyancy //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBuoyancy::Tunables CTaskNMBuoyancy::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMBuoyancy, 0x427acce5);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBuoyancy::CTaskNMBuoyancy(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime)
{
#if !__FINAL
	m_strTaskName = "NMBuoyancy";
#endif // !__FINAL
	SetInternalTaskType(CTaskTypes::TASK_NM_BUOYANCY);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBuoyancy::~CTaskNMBuoyancy()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBuoyancy::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&CTaskNMShot::sm_Tunables.m_ParamSets.m_Underwater);

	if (pPed->GetPrimaryWoundData()->valid && (pPed->GetPrimaryWoundData()->component == RAGDOLL_HEAD || pPed->GetPrimaryWoundData()->numHits > 5))
	{
		AddTuningSet(&CTaskNMShot::sm_Tunables.m_ParamSets.m_UnderwaterRelax);
	}

	CNmMessageList list;
	ApplyTuningSetsToList(list);

	// BULLET PARAMETERS MESSAGE:
	if (pPed->GetPrimaryWoundData()->valid)
	{
		ART::MessageParams& msg = list.GetMessage(NMSTR_MSG(NM_SHOTNEWBULLET_MSG));
		msg.addBool(NMSTR_PARAM(NM_START), true);
		msg.addInt(NMSTR_PARAM(NM_SHOTNEWBULLET_BODYPART), pPed->GetPrimaryWoundData()->component);
		msg.addBool(NMSTR_PARAM(NM_SHOTNEWBULLET_LOCAL_HIT_POINT_INFO), true);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_POS_VEC3), pPed->GetPrimaryWoundData()->localHitLocation.x, pPed->GetPrimaryWoundData()->localHitLocation.y, pPed->GetPrimaryWoundData()->localHitLocation.z);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_VEL_VEC3), pPed->GetPrimaryWoundData()->localHitNormal.x, pPed->GetPrimaryWoundData()->localHitNormal.y, pPed->GetPrimaryWoundData()->localHitNormal.z);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_NORMAL_VEC3), 1.0f, 0.0f, 0.0f);
	}

	list.Post(pPed->GetRagdollInst());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBuoyancy::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour::ControlBehaviour(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBuoyancy::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (CTaskMotionSwimming::CheckForRapids(pPed) && !pPed->ShouldBeDead() && !pPed->GetIsDeadOrDying())
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_RIVER_RAPIDS;

		nmDebugf3("[%u]Finish %s on %s - river flow too fast. Next task: %s", fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));

		return true;
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_SKIP_DEAD_CHECK | FLAG_SKIP_WATER_CHECK, &sm_Tunables.m_BlendOutThreshold);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMBuoyancy::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMBuoyancyInfo(); 
}


CTaskFSMClone *CClonedNMBuoyancyInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMBuoyancy(4000, 30000);
}

