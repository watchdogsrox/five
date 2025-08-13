// File header
#include "Task/Movement/Jumping/TaskInAir.h"

// Game Headers
#include "Animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "ModelInfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Streaming/Streaming.h"		// For CStreaming::HasObjectLoaded(), etc.
#include "System/Control.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Movement/TaskFall.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskComplexStuckInAir
//////////////////////////////////////////////////////////////////////////

CTaskComplexStuckInAir::CTaskComplexStuckInAir()
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_STUCK_IN_AIR);
}

CTaskComplexStuckInAir::~CTaskComplexStuckInAir()
{
}

aiTask* CTaskComplexStuckInAir::CreateNextSubTask(CPed* pPed)
{
	aiTask* pNextSubTask = NULL;

	switch(GetSubTask()->GetTaskType())
	{
	case CTaskTypes::TASK_DO_NOTHING:
		{
			if(!pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
			}
			else if(pPed->GetPlayerInfo())
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
			}
			else
			{
				// Need to work out what direction to face?
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_JUMPVAULT, pPed);
			}
		}
		break;

	case CTaskTypes::TASK_FALL_AND_GET_UP:
		{
			if(!pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
			}
			else
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
			}
		}
		break;

	case CTaskTypes::TASK_JUMPVAULT:
		{
			if(!pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				if(pPed->GetPlayerInfo())
				{
					pNextSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
				}
				else
				{
					pNextSubTask = CreateSubTask(CTaskTypes::TASK_SMART_FLEE, pPed);
				}
			}
			else
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
			}
		}
		break;

	case CTaskTypes::TASK_SMART_FLEE:
		{
			if(!pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
			}
			else
			{
				pNextSubTask = CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
			}
		}
		break;

	default:
		Assert(false);
		break;
	}

	return pNextSubTask;
}

aiTask* CTaskComplexStuckInAir::CreateFirstSubTask(CPed* pPed)
{
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck() == CPedStuckChecker::PED_STUCK_JUMP_FALLOFF_SIDE)
	{
		return CreateSubTask(CTaskTypes::TASK_FALL_AND_GET_UP, pPed);
	}
	else
	{
		return CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
	}
}

aiTask* CTaskComplexStuckInAir::ControlSubTask(CPed* pPed)
{
	aiTask* pNewSubTask = GetSubTask();

	switch(GetSubTask()->GetTaskType())
	{
	case CTaskTypes::TASK_DO_NOTHING:
		{
			// If not stuck any more
			if(!pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
			}
			else
			{
				// If player let them rotate and try to jump out of collision
				if(pPed->GetPlayerInfo())
				{
					CControl* pControl = pPed->GetControlFromPlayer();
					if(pControl)
					{
						float fStickX = pControl->GetPedWalkLeftRight().GetNorm();
						float fStickY = pControl->GetPedWalkUpDown().GetNorm();
						if(Abs(fStickX) > 0.0f || Abs(fStickY) > 0.0f)
						{
							float fNewHeading = rage::Atan2f(-fStickX, fStickY);
							fNewHeading += camInterface::GetHeading(); // // careful.. this looks dodgy to me - and could be ANY camera - DW
							fNewHeading = fwAngle::LimitRadianAngle(fNewHeading);

							pPed->SetDesiredHeading(fNewHeading);
						}

						if(pControl->GetPedJump().IsPressed())
						{
							pNewSubTask = CreateSubTask(CTaskTypes::TASK_JUMPVAULT, pPed);
						}
					}
				}

				pPed->SetIsStanding(true);

				// actually lets just get rid of ALL higher priority clips - MEDIUM first
				CSimpleBlendHelper& helper = pPed->GetMovePed().GetBlendHelper();

				helper.BlendOutClip(AP_MEDIUM, NORMAL_BLEND_OUT_DELTA);
				helper.BlendOutClip(AP_HIGH, NORMAL_BLEND_OUT_DELTA);
			}
		}
		break;

#if 0 // CS
	case CTaskTypes::TASK_FALL_AND_GET_UP:
		{
			if(GetSubTask()->GetSubTask() && 
				GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL_OVER && 
				static_cast<CTaskFallOver*>(GetSubTask()->GetSubTask())->IsFinished() && 
				pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				pPed->SetIsStanding(true);
			}
		}
		break;
#endif // 0

	case CTaskTypes::TASK_JUMPVAULT:
		{
			if(pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck() && 
				GetSubTask()->GetSubTask() && 
				GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
			{
				pNewSubTask = CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
			}
		}
		break;

	case CTaskTypes::TASK_SMART_FLEE:
		{
			if(pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
			{
				pNewSubTask = CreateSubTask(CTaskTypes::TASK_DO_NOTHING, pPed);
			}
		}
		break;

	default:
		Assert(false);
		break;	
	}

	if(pNewSubTask != GetSubTask())
	{
		GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL);
	}

	return pNewSubTask;
}

aiTask* CTaskComplexStuckInAir::CreateSubTask(const int iSubTaskType, CPed* pPed)
{
	Assert(pPed);

	aiTask* pSubTask = NULL;

	switch(iSubTaskType)
	{
	case CTaskTypes::TASK_DO_NOTHING:
		{
			pPed->SetIsStanding(true);
			pSubTask = rage_new CTaskDoNothing(5000);
		}
		break;

	case CTaskTypes::TASK_FALL_AND_GET_UP:
		{
			pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().ClearPedStuck();
			fwMvClipSetId clipSetId;
			fwMvClipId clipId;
			CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextFallImpact, CTaskFallOver::kDirFront, clipSetId, clipId);
			pSubTask = rage_new CTaskFallAndGetUp(clipSetId, clipId, 1.0f);
		}
		break;

	case CTaskTypes::TASK_JUMPVAULT:
		{
			pPed->SetIsStanding(true);
			pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().ClearPedStuck();
			pSubTask = rage_new CTaskFall();
		}
		break;

	case CTaskTypes::TASK_JUMP:
		{
			pPed->SetIsStanding(true);
			pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().ClearPedStuck();
			pSubTask = rage_new CTaskFall();
		}
		break;

	case CTaskTypes::TASK_SMART_FLEE:
		{
			pPed->SetIsStanding(true);
			Vector3 vFleePos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - 0.5f * VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			if(fwRandom::GetRandomNumber() & 1)
			{
				vFleePos += 0.5f * VEC3V_TO_VECTOR3(pPed->GetTransform().GetA());
			}
			else
			{
				vFleePos -= 0.5f * VEC3V_TO_VECTOR3(pPed->GetTransform().GetA());
			}

			pSubTask = rage_new CTaskSmartFlee(CAITarget(vFleePos), 5.0f, 10000);
		}
		break;

	case CTaskTypes::TASK_FINISHED:
		{
			pSubTask = NULL;
		}
		break;

	default:
		Assert(false);
		break;
	}

	return pSubTask;
}

//*********************************************************************************
//	CTaskDropOverEdge

/*
bank_float CTaskDropOverEdge::ms_fMinDropHeight					= 2.0f + 1.0f;
bank_float CTaskDropOverEdge::ms_fMaxDropHeight					= 50.0f;

bank_float CTaskDropOverEdge::ms_fDropDeltaLow					= 4.0f;
bank_float CTaskDropOverEdge::ms_fDropDeltaMedium				= 8.0f;
bank_float CTaskDropOverEdge::ms_fDropDeltaHigh					= 12.0f;

bank_bool CTaskDropOverEdge::ms_bDisableCollisionForDropStart	= true;
bank_bool CTaskDropOverEdge::ms_bDisableGravityForDropStart		= true;

const fwMvClipId CTaskDropOverEdge::ms_iDropIntroIDs[3] = { CLIP_AUTODROP_25CM_INTRO, CLIP_AUTODROP_50CM_INTRO, CLIP_AUTODROP_75CM_INTRO };
const fwMvClipId CTaskDropOverEdge::ms_iDropLoopIDs[3] = { CLIP_AUTODROP_25CM_LOOP, CLIP_AUTODROP_50CM_LOOP, CLIP_AUTODROP_75CM_LOOP };
const fwMvClipId CTaskDropOverEdge::ms_iDropLandIDs[3] = { CLIP_AUTODROP_25CM_LAND, CLIP_AUTODROP_50CM_LAND, CLIP_AUTODROP_75CM_LAND };

CTaskDropOverEdge::CTaskDropOverEdge(const Vector3 & vStartPos, const float fHeading, const int iDropType)
	: m_vStartPos(vStartPos),
	m_vDropPos(vStartPos),
	m_fDropEndZ(vStartPos.z),
	m_fHeading(fHeading),
	m_iDropType(iDropType),
	m_fDropClipVelScale(1.0f),
	m_vDropClipEndingVel(Vector3(0.0f,0.0f,0.0f))
{
	SetInternalTaskType(CTaskTypes::TASK_DROP_OVER_EDGE);
}
CTaskDropOverEdge::~CTaskDropOverEdge()
{

}

#if !__FINAL
void CTaskDropOverEdge::Debug() const
{
#if DEBUG_DRAW
	if(m_iDropType != DropType_Unknown && m_iDropType != DropType_None)
	{
		grcDebugDraw::Cross(RCC_VEC3V(m_vStartPos), 0.5f, Color_green1);
		grcDebugDraw::Cross(RCC_VEC3V(m_vDropPos), 0.5f, Color_green3);
		Vec3V vLandPos(m_vDropPos.x, m_vDropPos.y, m_fDropEndZ);
		grcDebugDraw::Cross(vLandPos, 0.5f, Color_red2);

		grcDebugDraw::Line(RCC_VEC3V(m_vStartPos), RCC_VEC3V(m_vDropPos), Color_green1, Color_green3);
		grcDebugDraw::Line(RCC_VEC3V(m_vDropPos), vLandPos, Color_green3, Color_red2);
	}
	const Vector3 vHdg(-rage::Sinf(m_fHeading), rage::Cosf(m_fHeading), 0.0f);
	grcDebugDraw::Line(RCC_VEC3V(m_vStartPos), VECTOR3_TO_VEC3V(m_vStartPos+vHdg), Color_purple, Color_white);
#endif

	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

bool CTaskDropOverEdge::ShouldAbort( const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_IN_AIR)
	{
		if(GetState()==State_DropLoop || GetState()==State_DropStart || GetState()==State_DropLand)
		{
			return false;
		}
	}

	return true;
}

void CTaskDropOverEdge::CleanUp()
{
	GetPed()->SetUseExtractedZ(false);
	if(!GetPed()->IsCollisionEnabled())
		GetPed()->EnableCollision();
}

CTask::FSM_Return CTaskDropOverEdge::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(State_SlideToCoord)
			FSM_OnEnter
				return StateSlideToCoord_OnEnter(pPed);
			FSM_OnUpdate
				return StateSlideToCoord_OnUpdate(pPed);

		FSM_State(State_WaitForClipsToStream)
			FSM_OnEnter
				return StateWaitForClipsToStream_OnEnter(pPed);
			FSM_OnUpdate
				return StateWaitForClipsToStream_OnUpdate(pPed);

		FSM_State(State_DropStart)
			FSM_OnEnter
				return StateDropStart_OnEnter(pPed);
			FSM_OnUpdate
				return StateDropStart_OnUpdate(pPed);

		FSM_State(State_DropLoop)
			FSM_OnEnter
				return StateDropLoop_OnEnter(pPed);
			FSM_OnUpdate
				return StateDropLoop_OnUpdate(pPed);

		FSM_State(State_DropLand)
			FSM_OnEnter
				return StateDropLand_OnEnter(pPed);
			FSM_OnUpdate
				return StateDropLand_OnUpdate(pPed);

	FSM_End
}

CTask::FSM_Return CTaskDropOverEdge::StateInitial_OnEnter(CPed* pPed)
{
	CalculateDropType(pPed);

	aiAssertf(0, "CTaskDropOverEdge is not currently supported without moveblenders. Needs reimplementing");

	return FSM_Continue;
}
CTask::FSM_Return CTaskDropOverEdge::StateInitial_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_SlideToCoord);
	return FSM_Continue;
}

CTask::FSM_Return CTaskDropOverEdge::StateSlideToCoord_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskSlideToCoord * pSlide = rage_new CTaskSlideToCoord(m_vStartPos, m_fHeading, 10.0f);
	SetNewTask(pSlide);
	return FSM_Continue;
}
CTask::FSM_Return CTaskDropOverEdge::StateSlideToCoord_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_SLIDE_TO_COORD);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_WaitForClipsToStream);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskDropOverEdge::StateWaitForClipsToStream_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskDropOverEdge::StateWaitForClipsToStream_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	const s32 iClipBlockIndex = fwClipSetManager::GetClipDictionaryIndex(CLIP_SET_AUTODROP);
	Assertf(iClipBlockIndex != -1, "An clip set %i is being requested which doesn't exist", CLIP_SET_AUTODROP.GetHash());
	if(iClipBlockIndex != -1 && !CStreaming::HasObjectLoaded(iClipBlockIndex, fwAnimManager::GetStreamingModuleId()))
	{
		CStreaming::RequestObject(iClipBlockIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
	}
	else
	{
		SetState(State_DropStart);		
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskDropOverEdge::StateDropStart_OnEnter(CPed* pPed)
{
	Assert(m_iDropType>=0 && m_iDropType<=2);
	StartClipBySetAndClip(pPed, CLIP_SET_AUTODROP, ms_iDropIntroIDs[m_iDropType], 4.0f);

	// Find out how far this clip will translate us forwards.
	// We shall scale the animated velocity so that it takes us exactly over the m_vDropPos

	if(GetClipHelper())
	{
		const float fClipTranslation = fwAnimHelpers::GetMoverTrackTranslationDiff(CLIP_SET_AUTODROP, ms_iDropIntroIDs[m_iDropType], 0.f, 1.f).y;
		const Vector3 vDiff = m_vDropPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fDistXY = vDiff.XYMag();

		if(fClipTranslation > 0.0001f)
			m_fDropClipVelScale = fDistXY / fClipTranslation;
		else
			m_fDropClipVelScale = 1.0f;

		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

		if(ms_bDisableCollisionForDropStart)
			pPed->DisableCollision();
		if(ms_bDisableGravityForDropStart)
			pPed->SetUseExtractedZ(true);

		m_vDropClipEndingVel = fwAnimHelpers::GetMoverTrackVelocity(CLIP_SET_AUTODROP, ms_iDropIntroIDs[m_iDropType]);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskDropOverEdge::StateDropStart_OnUpdate(CPed* pPed)
{
	CClipHelper * pClip = GetClipHelper();
	Assert(!pClip || pClip->GetClipId()==ms_iDropIntroIDs[m_iDropType]);
	if(!pClip || pClip->IsHeldAtEnd())
	{
		if(!pPed->IsCollisionEnabled())
			pPed->EnableCollision();
		if(pPed->GetUseExtractedZ())
			pPed->SetUseExtractedZ(false);
		pPed->SetVelocity(m_vDropClipEndingVel);
		SetState(State_DropLoop);
	}
	else
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskDropOverEdge::StateDropLoop_OnEnter(CPed* pPed)
{
	Assert(m_iDropType>=0 && m_iDropType<=2);
	StartClipBySetAndClip(pPed, CLIP_SET_AUTODROP, ms_iDropLoopIDs[m_iDropType], 4.0f);
	return FSM_Continue;
}
CTask::FSM_Return CTaskDropOverEdge::StateDropLoop_OnUpdate(CPed* pPed)
{
	Assert(!GetClipHelper() || GetClipHelper()->GetClipId()==ms_iDropLoopIDs[m_iDropType]);

	const float fGroundDiff = pPed->GetGroundZFromImpact() - (pPed->GetTransform().GetPosition().GetZf()-1.0f);
	static dev_float fEps = 0.25f;
	if(IsNearZero(fGroundDiff, fEps))
	{
		SetState(State_DropLand);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskDropOverEdge::StateDropLand_OnEnter(CPed* pPed)
{
	Assert(m_iDropType>=0 && m_iDropType<=2);
	StartClipBySetAndClip(pPed, CLIP_SET_AUTODROP, ms_iDropLandIDs[m_iDropType], 4.0f);
	return FSM_Continue;
}
CTask::FSM_Return CTaskDropOverEdge::StateDropLand_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	CClipHelper * pClip = GetClipHelper();
	Assert(!pClip || pClip->GetClipId()==ms_iDropLandIDs[m_iDropType]);

	if(!pClip || pClip->IsHeldAtEnd())
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

bool CTaskDropOverEdge::ProcessPhysics(float UNUSED_PARAM(fTimestep), int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(GetState()==State_DropStart)
	{
		Vector3 vAnimatedVel = pPed->GetAnimatedVelocity();
		vAnimatedVel.Scale(m_fDropClipVelScale);
		pPed->SetAnimatedVelocity(vAnimatedVel);
		return true;
	}
	return false;
}


bool CTaskDropOverEdge::CalculateDropType(CPed * pPed)
{
	// The initial test needs to be far enough away that the ped may clear any edge.
	// TODO: we also need some obstruction tests..
	static dev_float fStartStepMult = 2.0f;

	const float fMaxTestForwardsDist = 4.0f;
	const float fHalfWidth = pPed->GetCapsuleInfo()->GetHalfWidth();
	const float fTestStepDist = fHalfWidth;
	float fCurrDist = fTestStepDist*fStartStepMult;

	const Vector3 vHdg(-rage::Sinf(m_fHeading), rage::Cosf(m_fHeading), 0.0f);

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> capsuleResult;
	capsuleDesc.SetResultsStructure(&capsuleResult);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	capsuleDesc.SetExcludeEntity(pPed);

	while(fCurrDist < fMaxTestForwardsDist)
	{
		capsuleResult.Reset();

		const Vector3 vPos = m_vStartPos + (vHdg * fCurrDist);
		capsuleDesc.SetCapsule(vPos, vPos-Vector3(0.0f,0.0f,ms_fMaxDropHeight), fHalfWidth+0.1f);
		WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

		if(capsuleResult[0].GetHitDetected())
		{
			const Vector3 vDiff = m_vStartPos - capsuleResult[0].GetHitPosition();
			Assert(vDiff.z > 0.0f);

			if(vDiff.z > ms_fMinDropHeight && vDiff.z < ms_fMaxDropHeight)
			{
				m_vDropPos = vPos;
				m_fDropEndZ = capsuleResult[0].GetHitPosition().z;

				// TODO: now categorise the drop..
				if(vDiff.z < ms_fDropDeltaLow)
				{
					m_iDropType = DropType_Low;
				}
				else if(vDiff.z < ms_fDropDeltaMedium)
				{
					m_iDropType = DropType_Medium;
				}
				else
				{
					m_iDropType = DropType_Medium;
				}
				return true;
			}
		}

		fCurrDist += fTestStepDist;
	}

	m_iDropType = DropType_None;
	return false;
}
*/

