// Filename   :	TaskNMRollUpAndRelax.cpp
// Description:	Natural Motion roll up and relax class (FSM version)

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
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMRollUpAndRelax.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMRollUpAndRelaxInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMRollUpAndRelaxInfo::CClonedNMRollUpAndRelaxInfo(u32 nRollUpTime, float fStiffness, float fDamping)
: m_nRollUpTime(nRollUpTime)
, m_fStiffness(fStiffness)
, m_fDamping(fDamping)
, m_bHasStiffness(fStiffness > 0.0f)
, m_bHasDamping(fDamping > 0.0f)
{
}

CClonedNMRollUpAndRelaxInfo::CClonedNMRollUpAndRelaxInfo()
: m_nRollUpTime(0)
, m_fStiffness(-1.0f)
, m_fDamping(-1.0f)
, m_bHasStiffness(false)
, m_bHasDamping(false)
{
}

CTaskFSMClone *CClonedNMRollUpAndRelaxInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMRollUpAndRelax(10000, 10000, m_nRollUpTime, m_fStiffness, m_fDamping);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMRollUpAndRelax
//////////////////////////////////////////////////////////////////////////

CTaskNMRollUpAndRelax::CTaskNMRollUpAndRelax(u32 nMinTime, u32 nMaxTime, u32 nRollUpTime, float fStiffness, float fDamping)
:	CTaskNMRelax(nMinTime, nMaxTime, fStiffness, fDamping),
m_nRollUpTime(nRollUpTime),
bIsRelaxing(false)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_ROLL_UP_AND_RELAX);
}

CTaskNMRollUpAndRelax::~CTaskNMRollUpAndRelax()
{
}

void CTaskNMRollUpAndRelax::StartBehaviour(CPed* pPed)
{
	ART::MessageParams msgRollUp;
	msgRollUp.addBool(NMSTR_PARAM(NM_START), true);

	msgRollUp.addString(NMSTR_PARAM(NM_ROLLUP_MASK), "fb");
	msgRollUp.addFloat(NMSTR_PARAM(NM_ROLLUP_STIFFNESS), fwRandom::GetRandomNumberInRange(10.0f, 12.0f));
	msgRollUp.addFloat(NMSTR_PARAM(NM_ROLLUP_LEG_PUSH), 0.5f);
	msgRollUp.addFloat(NMSTR_PARAM(NM_ROLLUP_ARM_REACH_AMOUNT), 0.5f);
	msgRollUp.addFloat(NMSTR_PARAM(NM_ROLLUP_USE_ARM_TO_SLOW_DOWN), -0.5f);
	msgRollUp.addFloat(NMSTR_PARAM(NM_ROLLUP_ASYMMETRICAL_LEGS), fwRandom::GetRandomNumberInRange(0.1f, 0.4f));

	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ROLLUP_MSG), &msgRollUp);
}

void CTaskNMRollUpAndRelax::ControlBehaviour(CPed* pPed)
{
	if(bIsRelaxing)
		SendRelaxMessage(pPed, false);

	if(!bIsRelaxing && fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nRollUpTime)
	{
		ART::MessageParams msgRollUp;
		msgRollUp.addBool(NMSTR_PARAM(NM_START), false);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ROLLUP_MSG), &msgRollUp);

		SendRelaxMessage(pPed, true);
		bIsRelaxing = true;
	}
}

bool CTaskNMRollUpAndRelax::FinishConditions(CPed* pPed)
{
	return ProcessFinishConditionsBase(pPed, MONITOR_RELAX, FLAG_VEL_CHECK|FLAG_SKIP_DEAD_CHECK);
}
