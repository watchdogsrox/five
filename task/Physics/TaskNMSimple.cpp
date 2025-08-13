// Filename   :	TaskNMSimple.cpp
// Description:	NM Simple behavior

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated\articulatedbody.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "Event/EventDamage.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Physics/TaskNMSimple.h"
#include "Task/Physics/NmDebug.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMSimple /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMSimple::Tunables CTaskNMSimple::sm_Tunables;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMSimple::CTaskNMSimple(const Tunables::Tuning& tuning) :
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour(tuning.m_iMinTime, tuning.m_iMaxTime),
m_Tuning(tuning),
m_bBalanceFailedSent(false),
m_pWritheClipSetRequestHelper(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_SIMPLE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMSimple::CTaskNMSimple(const Tunables::Tuning& tuning, u32 overrideMinTime, u32 overrideMaxTime) :
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour(overrideMinTime, overrideMaxTime),
m_Tuning(tuning),
m_bBalanceFailedSent(false),
m_pWritheClipSetRequestHelper(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_SIMPLE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMSimple::~CTaskNMSimple()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

#if !__FINAL
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
atString CTaskNMSimple::GetName() const
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	atHashString tuningId = sm_Tunables.GetTuningId(m_Tuning);
	atVarString str("NMSimple - TuningId:%s", tuningId.GetCStr());

	return str;
}
#endif // !__FINAL

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMSimple::Tunables::Tunables() : 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTuning("CTaskNMSimple", 0x70788f5e, "Physics Tasks", "Task Tuning")
#if __BANK
, m_pBank(NULL)
, m_pGroup(NULL)
#endif
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
atHashString CTaskNMSimple::Tunables::GetTuningId(const Tunables::Tuning& tuning)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	atHashString uTuningId;
	for (Map::Iterator it = sm_Tunables.m_Tuning.Begin(); it != sm_Tunables.m_Tuning.End(); ++it)
	{
		if ((&(*it)) == &tuning)
		{
			uTuningId = it.GetKey();
			break;
		}
	}

	return uTuningId;
}

#if __BANK
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::Tunables::PreAddWidgets(bkBank& bank)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_pBank = &bank;
	m_pGroup = static_cast<bkGroup*>(bank.GetCurrentGroup());
	bank.PushGroup("Add/Remove Tuning", false);
	bank.AddText("Tuning:", &m_TuningName);
	bank.AddButton("Add Tuning", datCallback(MFA(CTaskNMSimple::Tunables::AddTuning), (datBase*)this));
	bank.AddButton("Remove Tuning", datCallback(MFA(CTaskNMSimple::Tunables::RemoveTuning), (datBase*)this));
	bank.AddButton("Remove All Tuning", datCallback(MFA(CTaskNMSimple::Tunables::RemoveAllTuning), (datBase*)this));
	bank.PopGroup();

	RefreshTuningWidgets();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int SortCompare(const CTaskNMSimple::Tunables::Map::DataPair* const* a, const CTaskNMSimple::Tunables::Map::DataPair* const* b)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return stricmp((*a)->key.GetCStr(), (*b)->key.GetCStr());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::Tunables::RefreshTuningWidgets(bool bSetCurrentGroup)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (nmVerifyf(m_pBank != NULL, "CTaskNMSimple::Tunables::RefreshTuningWidgets: Could not find bank widget for CTaskNMSimple tuning") &&
		nmVerifyf(m_pGroup != NULL, "CTaskNMSimple::Tunables::RefreshTuningWidgets: Could not find group widget for CTaskNMSimple tuning"))
	{
		static const u32 uGroupTitle = ATSTRINGHASH("Tuning", 0x15C82517);
		for (bkWidget* pChild = m_pGroup->GetChild(); pChild != NULL; pChild = pChild->GetNext())
		{
			if (atStringHash(pChild->GetTitle()) == uGroupTitle)
			{
				m_pGroup->Remove(*pChild);
				break;
			}
		}

		if (bSetCurrentGroup)
		{
			m_pBank->SetCurrentGroup(*m_pGroup);
		}

		m_pBank->PushGroup("Tuning", false, "Set of tuning data");

		// Sort the map alphabetically
		atArray<const Map::DataPair*> sortedArray;
		atArray<Map::DataPair>& rawArray = sm_Tunables.m_Tuning.GetRawDataArray();
		sortedArray.Reserve(rawArray.GetCount());
		for (int i = 0; i < rawArray.GetCount(); i++)
		{
			sortedArray.Push(&rawArray[i]);
		}
		sortedArray.QSort(0, -1, SortCompare);

		for (int i = 0; i < sortedArray.GetCount(); i++)
		{
			m_pBank->PushGroup(sortedArray[i]->key.GetCStr(), false);
			PARSER.AddWidgets(*m_pBank, &sortedArray[i]->data);
			m_pBank->PopGroup();
		}

		m_pBank->PopGroup();

		if (bSetCurrentGroup)
		{
			m_pBank->UnSetCurrentGroup(*m_pGroup);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::Tunables::AddTuning()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_TuningName != atHashString::Null())
	{
		Tunables::Tuning tuning;
		PARSER.InitObject(tuning);
		sm_Tunables.m_Tuning.Insert(m_TuningName, tuning);
		sm_Tunables.m_Tuning.FinishInsertion();

		RefreshTuningWidgets(true);
	}
	else
	{
		nmWarningf("CTaskNMSimple::Tunables::AddTuning: Need to specify a tuning name");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::Tunables::RemoveTuning()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_TuningName != atHashString::Null())
	{
		bool bRemoved = false;
		for (int i = 0; i < sm_Tunables.m_Tuning.GetCount(); i++)
		{
			if (*sm_Tunables.m_Tuning.GetKey(i) == m_TuningName)
			{
				sm_Tunables.m_Tuning.Remove(i);
				bRemoved = true;
				break;
			}
		}

		if (bRemoved)
		{
			RefreshTuningWidgets(true);
		}
		else
		{
			nmWarningf("CTaskNMSimple::Tunables::RemoveTuning: Could not find tuning %s", m_TuningName.GetCStr());
		}
	}
	else
	{
		nmWarningf("CTaskNMSimple::Tunables::RemoveTuning: Need to specify a tuning name");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::Tunables::RemoveAllTuning()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (sm_Tunables.m_Tuning.GetCount() > 0)
	{
		sm_Tunables.m_Tuning.Reset();

		RefreshTuningWidgets(true);
	}
}
#endif // __BANK

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMSimple::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Allow dragged tasks to interrupt this task.
	const CEventGivePedTask* pGiveEvent = (pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_GIVE_PED_TASK) ? static_cast<const CEventGivePedTask *>(pEvent) : NULL;
	if(pGiveEvent && pGiveEvent->GetTask() && (pGiveEvent->GetTask()->GetTaskType() == CTaskTypes::TASK_DRAGGED_TO_SAFETY))
	{
		return true;
	}
	
	return CTaskNMBehaviour::ShouldAbort(iPriority, pEvent);
}

bool CTaskNMSimple::Tunables::Append(Tuning* newItem, atHashString key)
{
	bool result = m_Tuning.SafeInsert(key,*newItem);
	return result;
}
void CTaskNMSimple::Tunables::Revert(atHashString key)
{	
	for(int index = 0, count = m_Tuning.GetCount(); index < count; index ++)
	{
		if(m_Tuning.GetRawDataArray()[index].key == key)
		{			
			m_Tuning.Remove(index);
		}
	}
	m_Tuning.FinishInsertion();
		
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&m_Tuning.m_Start);

	CNmMessageList list;
	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMSimple::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour::ControlBehaviour(pPed);

	AddTuningSet(&m_Tuning.m_Update);

	if (!m_bBalanceFailedSent && GetControlTask()->GetFeedbackFlags() & CTaskNMControl::BALANCE_FAILURE)
	{
		AddTuningSet(&m_Tuning.m_OnBalanceFailure);
		m_bBalanceFailedSent = true;
	}

	CNmMessageList list;
	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMSimple::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	static float s_fFallingSpeed = -4.0f;
	if (pPed->GetVelocity().z < s_fFallingSpeed)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
		m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;

		// Ensure that this task stops
		ART::MessageParams msg;
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msg);
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_RELAX, FLAG_VEL_CHECK | FLAG_SKIP_DEAD_CHECK | FLAG_QUIT_IN_WATER, &m_Tuning.m_BlendOutThreshold);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMSimple::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMSimpleInfo(sm_Tunables.GetTuningId(m_Tuning)); 
}

void CTaskNMSimple::CreateAnimatedFallbackTask(CPed* pPed)
{
	bool bFromGetUp = false;

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);

	CTaskNMControl* pControlTask = GetControlTask();

	// If the ped is already prone or if this task is starting as the result of a migration then start the writhe from the ground
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf() || (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0))
	{
		bFromGetUp = true;
	}

	fwMvClipSetId loopClipSetId = CTaskWrithe::GetIdleClipSet(pPed);
	s32 loopClipHash;
	CTaskWrithe::GetRandClipFromClipSet(loopClipSetId, loopClipHash);
	s32 startClipHash;
	CTaskWrithe::GetRandClipFromClipSet(CTaskWrithe::m_ClipSetIdToWrithe, startClipHash);
	CClonedWritheInfo::InitParams writheInfoInitParams(CTaskWrithe::State_Loop, NULL, bFromGetUp, false, false, false, 2.0f, NETWORK_INVALID_OBJECT_ID, 0, 
		loopClipSetId, loopClipHash, startClipHash, false);
	CClonedWritheInfo writheInfo(writheInfoInitParams);
	CTaskFSMClone* pNewTask = writheInfo.CreateCloneFSMTask();
	pNewTask->ReadQueriableState(&writheInfo);
	pNewTask->SetRunningLocally(true);

	SetNewTask(pNewTask);
}

bool CTaskNMSimple::ControlAnimatedFallback(CPed* pPed)
{
	if (sm_Tunables.GetTuningId(m_Tuning) == atHashString("HighFall_TimeoutGroundWrithe"))
	{
		nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

		// disable ped capsule control so the blender can set the velocity directly on the ped
		if (pPed && pPed->IsNetworkClone())
		{
			NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
		}

		if (m_pWritheClipSetRequestHelper == NULL)
		{
			m_pWritheClipSetRequestHelper = CWritheClipSetRequestHelperPool::GetNextFreeHelper();
		}

		int iOldHurtEndTime = pPed->GetHurtEndTime();
		if (iOldHurtEndTime <= 0)
		{
			pPed->SetHurtEndTime(1);
		}

		bool bStreamed = false;
		if (m_pWritheClipSetRequestHelper != NULL)
		{
			bStreamed = CTaskWrithe::StreamReqResourcesIn(m_pWritheClipSetRequestHelper, pPed, CTaskWrithe::NEXT_STATE_COLLAPSE | CTaskWrithe::NEXT_STATE_WRITHE | CTaskWrithe::NEXT_STATE_DIE);
		}

		if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			m_nSuggestedNextTask = CTaskTypes::TASK_FINISHED;
			if (!pPed->GetIsDeadOrDying())
			{
				if (pPed->ShouldBeDead())
				{
					CEventDeath deathEvent(false, fwTimer::GetTimeInMilliseconds(), true);
					fwMvClipSetId clipSetId;
					fwMvClipId clipId;
					CTaskWrithe::GetDieClipSetAndRandClip(clipSetId, clipId);
					deathEvent.SetClipSet(clipSetId, clipId);
					pPed->GetPedIntelligence()->AddEvent(deathEvent);
				}
				else
				{
					m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
				}
			}
			return false;
		}
		else if (bStreamed && GetSubTask() == NULL)
		{
			CreateAnimatedFallbackTask(pPed);
		}

		pPed->SetHurtEndTime(iOldHurtEndTime);

		if (GetNewTask() != NULL || GetSubTask() != NULL)
		{
			// disable ped capsule control so the blender can set the velocity directly on the ped
			if (pPed->IsNetworkClone())
			{
				NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
			}
			pPed->OrientBoundToSpine();
		}
	}

	return true;
}

CTaskFSMClone* CClonedNMSimpleInfo::CreateCloneFSMTask()
{
	const CTaskNMSimple::Tunables::Tuning* pTuning = CTaskNMSimple::sm_Tunables.GetTuning(m_uTuningId);
	if (AssertVerify(pTuning != NULL))
	{
		return rage_new CTaskNMSimple(*pTuning);
	}

	return CSerialisedFSMTaskInfo::CreateCloneFSMTask();
}
