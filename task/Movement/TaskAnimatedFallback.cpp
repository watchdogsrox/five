// Filename   :	TaskAnimatedFallback.cpp
// Description:	Plays a given animation, altering the ped's orientation to match its current pose
//
// --- Include Files ------------------------------------------------------------

#include "Task/Movement/TaskAnimatedFallback.h"

// C headers
// Rage headers

#include "crextra/posematcher.h"
#include "cranimation/weightset.h"
#include "crparameterizedmotion/pointcloud.h"
#include "crskeleton/skeleton.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentragdoll.h"

// Game headers
#include "ik/solvers/RootSlopeFixupSolver.h"
#include "Event/EventDamage.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/TaskGetUp.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskAnimatedFallback
//////////////////////////////////////////////////////////////////////////

CTaskAnimatedFallback::CTaskAnimatedFallback(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fStartPhase, float fRootSlopeFixupPhase, bool bHoldAtEnd)
	: m_clipId(clipId)
	, m_fStartPhase(fStartPhase)
	, m_fRootSlopeFixupPhase(fRootSlopeFixupPhase)
	, m_bHoldAtEnd(bHoldAtEnd)
{
	SetInternalTaskType(CTaskTypes::TASK_ANIMATED_FALLBACK);

	m_clipSetHelper.Request(clipSetId);
}

CTaskAnimatedFallback::CTaskAnimatedFallback(const CTaskAnimatedFallback& other)
	: m_clipSetHelper(other.m_clipSetHelper)
 	, m_clipId(other.m_clipId)
	, m_fStartPhase(other.m_fStartPhase)
	, m_fRootSlopeFixupPhase(other.m_fRootSlopeFixupPhase)
	, m_bHoldAtEnd(other.m_bHoldAtEnd)
{
	SetInternalTaskType(CTaskTypes::TASK_ANIMATED_FALLBACK);
}

float CTaskAnimatedFallback::GetClipPhase() const
{
	return (m_networkHelper.IsNetworkActive() && m_networkHelper.IsNetworkAttached()) ? m_networkHelper.GetFloat(CTaskGetUp::ms_GetUpClip0PhaseOut) : m_fStartPhase;
}

void CTaskAnimatedFallback::CleanUp()
{
	CPed* pPed = GetPed();

	if (m_networkHelper.IsNetworkActive())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, NORMAL_BLEND_DURATION);
	}

	// Ensure that the slope fixup IK solver is cleaned up!
	CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pSolver != NULL)
	{
		pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
	}

	pPed->SetTaskCapsuleRadius(0.0f);
	pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);
	pPed->ClearBound();
}

CTask::FSM_Return CTaskAnimatedFallback::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return StateStart_OnUpdate(pPed);
		FSM_State(State_PlayingClip)
			FSM_OnEnter
				StatePlayingClip_OnEnter(pPed);
			FSM_OnUpdate
				return StatePlayingClip_OnUpdate(pPed);
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskAnimatedFallback::StateStart_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (m_clipSetHelper.GetClipSetId() == CLIP_SET_ID_INVALID)
	{
		SetState(State_Finish);
	}
	else if (m_clipSetHelper.Request())
	{
		SetState(State_PlayingClip);
	}

	return FSM_Continue;
}

void CTaskAnimatedFallback::StatePlayingClip_OnEnter(CPed* pPed)
{
	fwClipSet* clipSet = m_clipSetHelper.GetClipSet();
	if (animVerifyf(clipSet != NULL, "CTaskAnimatedFallback::StatePlayingClip_OnEnter: Could not find clip set %s", m_clipSetHelper.GetClipSetId().GetCStr()))
	{
		if ((!m_networkHelper.IsNetworkActive() || !m_networkHelper.IsNetworkAttached()) &&
			animVerifyf(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM), 
			"CTaskAnimatedFallback::StatePlayingClip_OnEnter: Blend from NM network is not streamed in!"))
		{
			static const int s_iMaxNumWeightSetBones = 5;
			static const eAnimBoneTag s_WeightSetBoneTag[s_iMaxNumWeightSetBones] =
			{
				BONETAG_L_CALF,
				BONETAG_R_CALF,
				BONETAG_L_UPPERARM,
				BONETAG_R_UPPERARM,
				BONETAG_HEAD
			};
			crWeightSet weightSet(pPed->GetSkeletonData());
			for (int i = 0; i < s_iMaxNumWeightSetBones; i++)
			{
				s32 boneIndex = pPed->GetBoneIndexFromBoneTag(s_WeightSetBoneTag[i]);
				if (boneIndex !=-1)
				{
					weightSet.SetAnimWeight(boneIndex, 1.0f);
				}
			}
			crPoseMatcher poseMatcher;
			poseMatcher.Init(pPed->GetSkeletonData(), &weightSet);

			const crClip* clip = NULL;

			if (m_clipId != CLIP_ID_INVALID)
			{
				clip = clipSet->GetClip(m_clipId);
			}

			if (clip != NULL)
			{
				poseMatcher.AddSample(*clip, fwPoseMatcher::CalcKey(m_clipSetHelper.GetClipSetId(), m_clipId), clip->ConvertPhaseToTime(m_fStartPhase));
			}
			else
			{
				crFrame frame;
				frame.InitCreateBoneAndMoverDofs(pPed->GetSkeletonData(), false);
				frame.IdentityFromSkel(pPed->GetSkeletonData());

				if (clipSet->GetClipItemCount() > 0)
				{
					for (int i = 0; i < clipSet->GetClipItemCount(); i++)
					{
						fwMvClipId clipId = clipSet->GetClipItemIdByIndex(i);
						clip = clipSet->GetClip(clipId);
						if (clip != NULL)
						{
							float time = clip->ConvertPhaseToTime(m_fStartPhase);
							clip->Composite(frame, time);
							crpmPointCloud* pointCloud = rage_new crpmPointCloud;
							pointCloud->AddFrame(frame, pPed->GetSkeletonData(), &weightSet);
							poseMatcher.AddPointCloud(pointCloud, fwPoseMatcher::CalcKey(m_clipSetHelper.GetClipSetId(), clipId), time);
						}
					}
				}
				else
				{
					const crClipDictionary* clipDictionary = clipSet->GetClipDictionary();
					if (clipDictionary != NULL && clipDictionary->GetNumClips() > 0)
					{
						for (int i = 0; i < clipDictionary->GetNumClips(); i++)
						{
							fwMvClipId clipId = fwMvClipId(clipDictionary->FindKeyByIndex(i));
							clip = clipDictionary->FindClipByIndex(i);
							if (clip != NULL)
							{
								float time = clip->ConvertPhaseToTime(m_fStartPhase);
								clip->Composite(frame, time);
								crpmPointCloud* pointCloud = rage_new crpmPointCloud;
								pointCloud->AddFrame(frame, pPed->GetSkeletonData(), &weightSet);
								poseMatcher.AddPointCloud(pointCloud, fwPoseMatcher::CalcKey(m_clipSetHelper.GetClipSetId(), clipId), time);
							}
						}
					}
				}
			}

			u64 outKey;
			float outTime;
			Mat34V outTransform;
			if (animVerifyf(poseMatcher.FindBestMatch(*pPed->GetSkeleton(), outKey, outTime, outTransform), 
				"CTaskAnimatedFallback::StatePlayingClip_OnEnter: Could not find pose match for clipSet %s", m_clipSetHelper.GetClipSetId().GetCStr()))
			{
				fwMvClipSetId clipSetId = fwMvClipSetId(outKey >> 32);
				fwMvClipId clipId = fwMvClipId(outKey & 0xFFFFFFFF);

				nmDebugf3("CTaskAnimatedFallback::StatePlayingClip_OnEnter: Playing clip %s/%s @ time %.4f", clipSetId.GetCStr(), clipId.GetCStr(), outTime);

				clip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
				if (animVerifyf(clip != NULL, "CTaskAnimatedFallback::StatePlayingClip_OnEnter: Clip %s/%s does not exist", clipSetId.GetCStr(), clipId.GetCStr()))
				{
					outTransform.Setd(pPed->GetTransform().GetPosition());

					// The point cloud match will likely require a change of space for the cross blend to work
					// (the matched anim probably doesn't have the same offset from the root to the mover as the current one.
					// Work out a transformation matrix from the old anim to the new one, and use the ragdoll frame to
					// blend between them (much as we do when blending from nm)
					Matrix34 poseFrametoAnimMtx;
					Matrix34 pedMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
					poseFrametoAnimMtx.FastInverse(RCC_MATRIX34(outTransform));
					poseFrametoAnimMtx.DotFromLeft(pedMatrix);

					pPed->GetSkeleton()->Transform(RCC_MAT34V(poseFrametoAnimMtx));
					// Need to pose the local matrices of skeleton because the above transform only updates the object space ones.
					pPed->InverseUpdateSkeleton();

					// Our last ragdoll bounds matrix will no longer be valid so need to consider this as a 'warp'...
					pPed->SetMatrix(RCC_MATRIX34(outTransform), true, true, true);

					// Now need to make sure that the ragdoll bounds match the skeleton otherwise if our ragdoll activates on this same frame the
					// bounds will be out-of-date!
					pPed->GetRagdollInst()->PoseBoundsFromSkeleton(true, true);
					pPed->GetRagdollInst()->PoseBoundsFromSkeleton(true, true);

					float ragdollFrameBlendDuration = REALLY_SLOW_BLEND_DURATION;

					crFrame* pFrame = NULL;
					if (pPed->GetAnimDirector())
					{
						fwAnimDirectorComponentRagDoll* componentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();
						if(componentRagDoll)
						{
							// Actually make the switch to the animated instance.
							componentRagDoll->PoseFromRagDoll(*pPed->GetAnimDirector()->GetCreature());

							// Actually make the switch to the animated instance.
							pFrame = componentRagDoll->GetFrame();

							// if we're coming from the static frame state, switch to animated
							if (pPed->GetMovePed().GetState() == CMovePed::kStateStaticFrame)
							{
								pPed->GetMovePed().SwitchToAnimated(ragdollFrameBlendDuration, true);
								// force the motion state to restart the ped motion move network
								pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
							}
						}
					}

					m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM);

					pPed->GetMovePed().SetTaskNetwork(m_networkHelper, 0.0f);

					// Transition to the correct state
					m_networkHelper.SendRequest(CTaskGetUp::ms_GetUpClipRequest);
					m_networkHelper.WaitForTargetState(CTaskGetUp::ms_OnEnterGetUpClip);

					m_networkHelper.SetFloat(CTaskGetUp::ms_BlendDuration, ragdollFrameBlendDuration);
					m_networkHelper.SetClip(clip, CTaskGetUp::ms_GetUpClip);
					m_networkHelper.SetFloat(CTaskGetUp::ms_GetUpRate, 1.0f);
					m_networkHelper.SetFloat(CTaskGetUp::ms_GetUpPhase0, clip->ConvertTimeToPhase(outTime));
					m_networkHelper.SetBoolean(CTaskGetUp::ms_GetUpLooped, false);
					// Pass in the ragdoll frame and blend duration
					m_networkHelper.SetFrame(pFrame, CTaskGetUp::ms_RagdollFrame);
					m_networkHelper.SetFloat(CTaskGetUp::ms_RagdollFrameBlendDuration, ragdollFrameBlendDuration);

					if (m_fRootSlopeFixupPhase < 0.0f)
					{
						static const atHashString ms_BlendInRootIkSlopeFixup("BlendInRootIkSlopeFixup", 0xDBFC38CA);
						CClipEventTags::FindEventPhase<crPropertyAttributeHashString>(clip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_BlendInRootIkSlopeFixup, m_fRootSlopeFixupPhase);
					}
				}
			}
		}
	}
}

CTask::FSM_Return CTaskAnimatedFallback::StatePlayingClip_OnUpdate(CPed* pPed)
{
	if (!m_networkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if (!m_bHoldAtEnd && (!pPed->IsNetworkClone() || (FindParentTaskOfType(CTaskTypes::TASK_DYING_DEAD) && pPed->IsDead())) && 
		(m_networkHelper.GetBoolean(CTaskGetUp::ms_GetUpClipFinished) || GetClipPhase() >= 1.0f))
	{
		SetState(State_Finish);
	}
	else
	{
		if (m_fRootSlopeFixupPhase >= 0.0f && GetClipPhase() >= m_fRootSlopeFixupPhase)
		{
			// If we haven't yet added IK fixup...
			const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
			if (pSolver == NULL || pSolver->GetBlendRate() <= 0.0f)
			{
				// Start aligning the ped to the slope...
				pPed->GetIkManager().AddRootSlopeFixup(SLOW_BLEND_IN_DELTA);
			}
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);
	}

	return FSM_Continue;
}

bool CTaskAnimatedFallback::ProcessPostPreRender()
{
	CPed *pPed = GetPed();

	const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo(); 
	if (pSolver != NULL && pSolver->GetBlendRate() >= 0.0f && pCapsuleInfo != NULL)
	{
		pPed->OrientBoundToSpine();
		pPed->SetTaskCapsuleRadius(pCapsuleInfo->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);
		if (m_fRootSlopeFixupPhase == 0.0f || GetClipPhase() == 1.0f)
		{
			pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);

			// Always force pitch/offset to the maximum
			pPed->SetBoundPitch(-HALF_PI);
			pPed->SetDesiredBoundPitch(pPed->GetBoundPitch());
			if (pCapsuleInfo->GetBipedCapsuleInfo())
			{
				float fCapsuleRadiusDiff = pCapsuleInfo->GetHalfWidth() - (pCapsuleInfo->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);
				Vector3 vBoundOffset(0.0f, 0.0f, pCapsuleInfo->GetBipedCapsuleInfo()->GetCapsuleZOffset() - (fCapsuleRadiusDiff * 0.5f));
				pPed->SetBoundOffset(vBoundOffset);
				pPed->SetDesiredBoundOffset(pPed->GetBoundOffset());
			}
		}
		else
		{
			if (pCapsuleInfo->GetBipedCapsuleInfo())
			{
				pPed->OverrideCapsuleRadiusGrowSpeed(pCapsuleInfo->GetBipedCapsuleInfo()->GetRadiusGrowSpeed() * CTaskFallOver::ms_fCapsuleRadiusGrowSpeedMult);
			}
		}
	}
	else
	{
		pPed->SetTaskCapsuleRadius(0.0f);
		pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);
		pPed->ClearBound();
	}

	return true;
}
