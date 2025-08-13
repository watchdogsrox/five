// Filename   :	TaskCrawl.cpp
// Description:	Crawl task
//
// --- Include Files ------------------------------------------------------------

#include "Task/movement/TaskCrawl.h"

// C headers
// Rage headers

// Framework headers

// Game headers
#include "Animation/MovePed.h"
#include "Event/Event.h"
#include "IK/Solvers/RootSlopeFixupSolver.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Physics/TaskNM.h"

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// MoVE network signals
//////////////////////////////////////////////////////////////////////////

fwMvRequestId CTaskCrawl::ms_GetUpClipRequest("GetUpClip",0xB9AEAD6);

fwMvBooleanId CTaskCrawl::ms_OnEnterGetUpClip("OnEnterGetUpClip",0xF44FE316);

fwMvBooleanId CTaskCrawl::ms_GetUpClipFinished("GetUpClipFinished",0x9197959E);

fwMvFloatId CTaskCrawl::ms_BlendDuration("BlendDuration",0x5E36F77F);
fwMvRequestId CTaskCrawl::ms_TagSyncBlend("TagSyncBlend",0x69FB1240);

fwMvClipId CTaskCrawl::ms_GetUpClip("GetUpPlaybackClip",0x2581328B);
fwMvFloatId CTaskCrawl::ms_GetUpPhase0("GetUp0Phase",0x7B132374);
fwMvFloatId	CTaskCrawl::ms_GetUpRate("GetUpRate",0x467E0B25);
fwMvFloatId	CTaskCrawl::ms_GetUpClip0PhaseOut("GetupClip0PhaseOut",0x60B16DC0);
fwMvBooleanId CTaskCrawl::ms_GetUpLooped("GetUpLooped",0x5F04FFF8);

fwMvClipId CTaskCrawl::ms_CrawlForwardClipId("ONFRONT_FWD",0x40CC0C49);
fwMvClipId CTaskCrawl::ms_CrawlBackwardClipId("ONFRONT_BWD",0xB3EE208A);
fwMvClipSetId CTaskCrawl::ms_CrawlClipSetId("move_crawl",0xA2CEC2FA);

//////////////////////////////////////////////////////////////////////////
// CClonedCrawlInfo
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedCrawlInfo::CClonedCrawlInfo(u32 uState, const Vector3& vSafePosition)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	SetStatusFromMainTaskState(uState);
	m_vSafePosition = vSafePosition;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedCrawlInfo::CClonedCrawlInfo()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone* CClonedCrawlInfo::CreateCloneFSMTask()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CTaskCrawl(m_vSafePosition);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskCrawl::CTaskCrawl(const Vector3& vSafePosition) :
m_vSafePosition(vSafePosition),
m_clipId(ms_CrawlForwardClipId),
m_fBlendOutRootIkSlopeFixupPhase(-1.0f),
m_fBlendDuration(SLOW_BLEND_DURATION),
m_bTagSyncBlend(false),
m_bRunningAsMotionTask(false)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	SetInternalTaskType(CTaskTypes::TASK_CRAWL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskCrawl::~CTaskCrawl()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::CleanUp()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed();

	// we should be done with this now so we can remove any streaming requests that have been made.....
	if(NetworkInterface::IsGameInProgress() && pPed && !pPed->IsNetworkClone() && pPed->GetNetworkObject())
	{
		NetworkInterface::RemoveAllNetworkAnimRequests(GetPed());
	}

	if(pPed && m_networkHelper.IsNetworkActive())
	{
		if (m_bRunningAsMotionTask)
		{
#if ENABLE_DRUNK
			CTaskMotionBase* pTask = pPed->GetPrimaryMotionTaskIfExists();

			if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
			{
				CTaskMotionPed* pPedTask = static_cast<CTaskMotionPed*>(pTask);
				pPedTask->ClearDrunkNetwork(m_fBlendDuration);
			}
#endif // ENABLE_DRUNK
		}
		else
		{
			u32 blendOutFlags = CMovePed::Task_None;

			if (m_bTagSyncBlend)
			{
				blendOutFlags |= CMovePed::Task_TagSyncTransition;
			}

			pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, m_fBlendDuration, blendOutFlags);
		}
	}

	// Reset any bound sizing/orientation/offset
	pPed->ClearBound();
	pPed->SetBoundPitch(0.0f);
	pPed->SetBoundHeading(0.0f);
	pPed->SetBoundOffset(VEC3_ZERO);
	pPed->SetTaskCapsuleRadius(0.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCrawl::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (iPriority == ABORT_PRIORITY_IMMEDIATE)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	bool bAllowAbort = false;

	// Always abort here if the event requires abort for ragdoll
	if (pEvent != NULL && (static_cast<const CEvent*>(pEvent)->RequiresAbortForRagdoll() || static_cast<const CEvent*>(pEvent)->GetEventType() == EVENT_NEW_TASK))
	{
		bAllowAbort = true;
	}

	if (!bAllowAbort)
	{
		const aiTask* pTaskResponse = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();
		if (pTaskResponse == NULL)
		{
			pTaskResponse = pPed->GetPedIntelligence()->GetEventResponseTask();
		}

		if (pTaskResponse != NULL)
		{
			if (CTaskNMBehaviour::DoesResponseHandleRagdoll(pPed, pTaskResponse))
			{
				bAllowAbort = true;
			}
		}
	}

	if (bAllowAbort)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTask::DoAbort(iPriority, pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskCrawl::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedCrawlInfo(GetState(), m_vSafePosition);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Assert(pTaskInfo != NULL && pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_CRAWL);
    CClonedCrawlInfo* pCrawlInfo = static_cast<CClonedCrawlInfo*>(pTaskInfo);

	m_vSafePosition = pCrawlInfo->GetSafePosition();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskCrawl::FSM_Return CTaskCrawl::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Use the standard state machine
	return UpdateFSM(iState, iEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskCrawl::ProcessPreFSM()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false);

	// Force disable leg ik
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_OFF);

	if (!m_bRunningAsMotionTask && !pPed->GetIsSwimming() && !pPed->GetIsParachuting())
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
	}

	if (pPed->IsLocalPlayer())
	{
		CTask* pTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
		if (pTask != NULL && pTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER)
		{
			// Mark the movement task as still in use this frame
			CTaskMoveInterface* pMoveInterface = pTask->GetMoveInterface();
			if (pMoveInterface)
			{
				pMoveInterface->SetCheckedThisFrame(true);
#if !__FINAL
				pMoveInterface->SetOwner(this);
#endif
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskCrawl::UpdateFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Start)
		FSM_OnEnter
			return Start_OnEnter(pPed);
		FSM_OnUpdate
			return Start_OnUpdate(pPed);

	FSM_State(State_Crawl)
		FSM_OnEnter
			return Crawl_OnEnter(pPed);
		FSM_OnUpdate
			return Crawl_OnUpdate(pPed);
		FSM_OnExit
			Crawl_OnExit(pPed);

	FSM_State(State_Finish)
		FSM_OnUpdate
			return Finish_OnUpdate();

FSM_End
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskCrawl::Start_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Need this to happen immediately - don't want to wait for animations or move network to stream in...
	float fNewCapsuleRadius = pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult;
	pPed->SetTaskCapsuleRadius(fNewCapsuleRadius);
	pPed->ResizePlayerLegBound(true);
	pPed->OverrideCapsuleRadiusGrowSpeed(INSTANT_BLEND_IN_DELTA);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskCrawl::Start_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_CrawlRequestHelper.Request(ms_CrawlClipSetId))
	{
		// Calculate a new heading and rate (to determine if the ped should be playing the animation forwards or backwards) as we've just started the crawl
		CalculateDesiredHeadingAndPlaybackRate(pPed);

		SetState(State_Crawl);
	}
	else
	{
		OrientBounds(pPed);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const crClip* CTaskCrawl::GetClip() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_clipId != CLIP_ID_INVALID && ms_CrawlClipSetId != CLIP_SET_ID_INVALID)
	{
		return fwAnimManager::GetClipIfExistsBySetId(ms_CrawlClipSetId, m_clipId);
	}
	else
	{
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::OrientBounds(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	float fBoundPitch = -HALF_PI;

	// Use the root slope fixup solver to determine the amount to pitch the bound
	CRootSlopeFixupIkSolver* pRootSlopeFixupIkSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (Verifyf(pRootSlopeFixupIkSolver != NULL, "CTaskGetUp::PlayingGetUpClip_OnUpdate: Root slope fixup has not been activated - won't be able to orient bound to slope for crawl"))
	{
		ScalarV fPitch;
		ScalarV fRoll;
		Mat34V xParentMatrix = pPed->GetTransform().GetMatrix();
		if (pRootSlopeFixupIkSolver->CalculatePitchAndRollFromGroundNormal(fPitch, fRoll, xParentMatrix.GetMat33ConstRef()))
		{
			fBoundPitch += fPitch.Getf();
		}
	}

	// Always force pitch/offset to the maximum
	pPed->SetBoundPitch(fBoundPitch);
	if (pPed->GetCapsuleInfo()->GetBipedCapsuleInfo())
	{
		float fCapsuleRadiusDiff = pPed->GetCapsuleInfo()->GetHalfWidth() - (pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);
		Vector3 vBoundOffset(0.0f, 0.0f, pPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetCapsuleZOffset() - (fCapsuleRadiusDiff * 0.5f));
		pPed->SetBoundOffset(vBoundOffset);
	}
	pPed->ResetProcessBoundsCountdown();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::SetSafePosition(const Vector3& vSafePosition)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_vSafePosition = vSafePosition;

	if (CalculateDesiredHeadingAndPlaybackRate(GetPed()) && GetState() == State_Crawl)
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskCrawl::FSM_Return CTaskCrawl::Crawl_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_fBlendOutRootIkSlopeFixupPhase = -1.0f;

	// Get the clip and pass it in
	const crClip* pClip = GetClip();

	if (!taskVerifyf(pClip != NULL, "CTaskCrawl::Crawl_OnEnter: Unable to load get up clip %s", fwClipSetManager::GetRequestFailReason(ms_CrawlClipSetId, m_clipId).c_str()))
	{
		return FSM_Quit;
	}

	if (!m_networkHelper.IsNetworkActive() || !m_networkHelper.IsNetworkAttached())
	{
		StartMoveNetwork();
	}

	// If the animation is moving backwards relative to the mover direction then we need to reverse playback of the clip so that movement is always
	// in the mover direction!
	//m_bReversePlaybackRate = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.0f, 1.0f).y < 0.0f;

	// Transition to the correct state
	m_networkHelper.SendRequest(ms_GetUpClipRequest);
	m_networkHelper.WaitForTargetState(ms_OnEnterGetUpClip);

	if (m_bTagSyncBlend)
	{
		m_networkHelper.SendRequest(ms_TagSyncBlend);
	}
	m_networkHelper.SetFloat(ms_BlendDuration, m_fBlendDuration);
	m_networkHelper.SetClip(pClip, ms_GetUpClip);

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	// Crawling animations should always be looping
	m_networkHelper.SetBoolean(ms_GetUpLooped, true);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskCrawl::Crawl_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	OrientBounds(pPed);

	if (!m_networkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// If a mover fixup rotation tag has been added to the anim, handle rotating the current heading towards the desired over the duration of the tag.
	//float fMoverFixupStartPhase = 0.0f;
	//float fMoverFixupEndPhase = 1.0f;
	float fCurrentPhase = m_networkHelper.GetFloat(ms_GetUpClip0PhaseOut);

	const crClip* pClip = GetClip();
	if (pClip != NULL && fCurrentPhase >= 0.0f)
	{
		static const atHashString s_BlendOutRootIkSlopeFixup("BlendOutRootIkSlopeFixup",0xA5690A9E);
		if (m_fBlendOutRootIkSlopeFixupPhase < 0.0f && !CClipEventTags::FindEventPhase<crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlendOutRootIkSlopeFixup, m_fBlendOutRootIkSlopeFixupPhase))
		{
			// If we couldn't find an event then it means we blend out when the animation ends...
			m_fBlendOutRootIkSlopeFixupPhase = 1.0f;
		}

		static const atHashString s_BlockSecondaryAnim("BlockSecondaryAnim",0xD69F5E8E);
		const crTag* pTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlockSecondaryAnim, fCurrentPhase);
		if (pTag != NULL && pTag->GetStart() <= fCurrentPhase && pTag->GetEnd() >= fCurrentPhase)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockSecondaryAnim, true);
		}

		if (CalculateDesiredHeadingAndPlaybackRate(pPed))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}

		static dev_bool s_bAllowPlayerInput = false;
		static dev_float s_fTurnSpeed = 1.0f;
		float fCurrentHeadingDelta = 0.0f;

		// Allow the player to affect the turn should they choose to
		if (s_bAllowPlayerInput && pPed->GetMotionData()->GetDesiredMbrX() > 0.001f)
		{
			fCurrentHeadingDelta = s_fTurnSpeed * fwTimer::GetTimeStep();
		}
		else if (s_bAllowPlayerInput && pPed->GetMotionData()->GetDesiredMbrX() < -0.001f)
		{
			fCurrentHeadingDelta = -s_fTurnSpeed * fwTimer::GetTimeStep();
		}
		else
		{
			float fDesiredHeadingDelta = SubtractAngleShorterFast(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
			Approach(fCurrentHeadingDelta, fDesiredHeadingDelta, s_fTurnSpeed, fwTimer::GetTimeStep());
		}

		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fCurrentHeadingDelta);
	}

	if (m_networkHelper.GetBoolean(ms_GetUpClipFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::Crawl_OnExit(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());

	// Reset the ped bound size to defaults
	pPed->SetTaskCapsuleRadius(0.0f);
	pPed->ResizePlayerLegBound(false);
	pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);

	// Reset the ped bound orientation to defaults
	pPed->ClearBound();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskCrawl::Finish_OnUpdate()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return FSM_Quit;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCrawl::StartMoveNetwork()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	taskAssertf(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM), "CTaskCrawl::StartMoveNetwork: %s network is not streamed in!", CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM.TryGetCStr());

	CPed* pPed = GetPed();
	m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM);

	pPed->GetMovePed().SetTaskNetwork(m_networkHelper, NORMAL_BLEND_DURATION);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCrawl::CalculateDesiredHeadingAndPlaybackRate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	pPed->SetDesiredHeading(m_vSafePosition);

	fwMvClipId clipId = ms_CrawlForwardClipId;

	// If we're required to rotate more than 112.5 (90 + fudge factor to avoid thrashing) degrees then just play the animation backwards and
	// adjust the desired heading so the ped doesn't try and do a 180 to get to the safe position but instead just starts moving in the opposite direction
	static dev_float fAngleThreshold = 112.5f * DtoR;
	if (Abs(SubtractAngleShorterFast(pPed->GetDesiredHeading(), pPed->GetCurrentHeading())) > fAngleThreshold)
	{
		clipId = ms_CrawlBackwardClipId;
		pPed->SetDesiredHeading(fwAngle::LimitRadianAngleFast(pPed->GetDesiredHeading() + PI));
	}

	if (clipId != m_clipId)
	{
		m_clipId = clipId;
			
		return true;
	}

	return false;
}
