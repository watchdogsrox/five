// Filename   :	TaskNMScriptControl.cpp
// Description:	Natural Motion script control class (FSM version)

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
#include "Task/Physics/TaskNMScriptControl.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()


CTaskNMScriptControl::CTaskNMScriptControl(u32 nMaxTime, /*bool bAbortIfInjured, bool bAbortIfDead,*/ bool bForceControl)
: CTaskNMBehaviour(nMaxTime, nMaxTime)
{
	//	m_bAbortIfInjured = bAbortIfInjured;
	//	m_bAbortIfDead = bAbortIfDead;
	m_bForceControl = bForceControl;

	for(int i = 0; i < NM_NUM_MESSAGES/32; i++)
	{
		m_nSuccessState[i] = 0;
		m_nFailureState[i] = 0;
		m_nFinishState[i] = 0;
	}
	SetInternalTaskType(CTaskTypes::TASK_NM_SCRIPT_CONTROL);
}

CTaskNMScriptControl::~CTaskNMScriptControl()
{
}

CTaskInfo* CTaskNMScriptControl::CreateQueriableState() const 
{ 
	// this task is not networked yet
	Assert(!NetworkInterface::IsGameInProgress()); 
	return rage_new CTaskInfo(); 
}

void CTaskNMScriptControl::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	for(int i=0; i<NM_NUM_MESSAGES; i++)
	{
		const char* pFeedBackString = NMSTR_PARAM(eNMStrings(CTaskNMBehaviour::GetMessageFromIndex(i) + 1));
		if(*pFeedBackString!='\0' && !strcmp(pFeedBackString, pFeedbackInterface->m_behaviourName))
		{
			// The logic behind this rather terse statement is to turn the correct flag on when, instead of an
			// intrinsic type, we store the bit-field over a u32 array.
			m_nFailureState[i>>5] |= BIT(i%32);
		}
	}
}
void CTaskNMScriptControl::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	for(int i=0; i<NM_NUM_MESSAGES; i++)
	{
		const char* pFeedBackString = NMSTR_PARAM(eNMStrings(CTaskNMBehaviour::GetMessageFromIndex(i) + 1));
		if(*pFeedBackString!='\0' && !strcmp(pFeedBackString, pFeedbackInterface->m_behaviourName))
		{
			// The logic behind this rather terse statement is to turn the correct flag on when, instead of an
			// intrinsic type, we store the bit-field over a u32 array.
			m_nSuccessState[i>>5] |= BIT(i%32);
		}
	}
}
void CTaskNMScriptControl::BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFinish(pPed, pFeedbackInterface);

	for(int i=0; i<NM_NUM_MESSAGES; i++)
	{
		const char* pFeedBackString = NMSTR_PARAM(eNMStrings(CTaskNMBehaviour::GetMessageFromIndex(i) + 1));
		if(*pFeedBackString!='\0' && !strcmp(pFeedBackString, pFeedbackInterface->m_behaviourName))
		{
			// The logic behind this rather terse statement is to turn the correct flag on when, instead of an
			// intrinsic type, we store the bit-field over a u32 array.
			m_nFinishState[i>>5] |= BIT(i%32);
		}
	}
}

bool CTaskNMScriptControl::HasSucceeded(eNMStrings nFeedback)
{
	taskAssertf(CTaskNMBehaviour::IsMessageString(eNMStrings(nFeedback - 1)), "HasSucceeded: String is not a feedback (FB) string. eNMStrings: %d.", nFeedback);
	int nMessageIndex = CTaskNMBehaviour::GetMessageStringIndex(eNMStrings(nFeedback - 1));

	// The logic behind this rather terse statement is to check the correct flag when, instead of an
	// intrinsic type, we store the bit-field over a u32 array.
	if(m_nSuccessState[nMessageIndex>>5] &BIT(nMessageIndex%32))
		return true;

	return false;
}
bool CTaskNMScriptControl::HasFailed(eNMStrings nFeedback)
{
	taskAssertf(CTaskNMBehaviour::IsMessageString(eNMStrings(nFeedback - 1)), "HasFailed: String is not a feedback (FB) string. eNMStrings: %d.", nFeedback);
	int nMessageIndex = CTaskNMBehaviour::GetMessageStringIndex(eNMStrings(nFeedback - 1));
	
	// The logic behind this rather terse statement is to check the correct flag when, instead of an
	// intrinsic type, we store the bit-field over a u32 array.
	if(m_nFailureState[nMessageIndex>>5] &BIT(nMessageIndex%32))
		return true;

	return false;

}
bool CTaskNMScriptControl::HasFinished(eNMStrings nFeedback)
{
	taskAssertf(CTaskNMBehaviour::IsMessageString(eNMStrings(nFeedback - 1)), "HasFinished: String is not a feedback (FB) string. eNMStrings: %d.", nFeedback);
	int nMessageIndex = CTaskNMBehaviour::GetMessageStringIndex(eNMStrings(nFeedback - 1));

	// The logic behind this rather terse statement is to check the correct flag when, instead of an
	// intrinsic type, we store the bit-field over a u32 array.
	if(m_nFinishState[nMessageIndex>>5] &BIT(nMessageIndex%32))
		return true;

	return false;
}


void CTaskNMScriptControl::StartBehaviour(CPed* pPed)
{
	if(m_bForceControl)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll, true );
}
bool CTaskNMScriptControl::FinishConditions(CPed* pPed)
{
	if(m_bForceControl)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll, true );

	if(m_nMaxTime > 0 && fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMaxTime)
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
		return true;
	}
	return false;
}
