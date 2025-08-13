//
// Task/Default/TaskArrest.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// Class header 
#include "Task/Default/TaskArrest.h"

// Game headers
#include "Animation/Move.h"
#include "Animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Scripted/ScriptDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "Debug/DebugScene.h"
#include "Math/angmath.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Network.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/gtaInst.h"
#include "Pickups/PickupManager.h"
#include "Scene/World/GameWorld.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Weapons/TaskWeapon.h"
#include "Weapons/Weapon.h"
#include "Task/Default/ArrestHelpers.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "camera/base/BaseCamera.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

#if ENABLE_TASKS_ARREST_CUFFED

const fwMvFloatId CTaskArrest::ms_HeadingDelta("HeadingDelta",0x232C8422);
const fwMvFloatId CTaskArrest::ms_Distance("Distance",0xD6FF9582);
const fwMvFloatId CTaskArrest::ms_PairedAnimAngle("PairedAnimAngle",0x89c3c86c);
const fwMvFlagId CTaskArrest::ms_MoveRight("MoveRight",0x8CAB3862);
const fwMvFlagId CTaskArrest::ms_UseFallback("UseFallback",0x9ED7F1F7);
const fwMvFlagId CTaskArrest::ms_UseUncuff("UseUncuff",0x679B9D0B);
const fwMvFlagId CTaskArrest::ms_UseNewAnims("UseNewAnims",0x53de5b58);
const fwMvBooleanId CTaskArrest::ms_Phase1Complete("Phase1Complete",0xB031A936);
const fwMvBooleanId CTaskArrest::ms_Phase2Complete("Phase2Complete",0x3940C543);
const fwMvBooleanId CTaskArrest::ms_Ended("Ended",0x292A3069);
const fwMvPropertyId CTaskArrest::ms_Shove("Shove",0xEB17960E);
const fwMvPropertyId CTaskArrest::ms_Handcuffed("Handcuff",0xC5C4CAC3);
const fwMvPropertyId CTaskArrest::ms_Interruptible("Interruptible",0x550A14CF);
const fwMvFloatId CTaskArrest::ms_DistanceWeightId("DistanceWeight",0x48810475);
const fwMvFloatId CTaskArrest::ms_DistanceDirectionWeightId("DistanceWeightDirection",0x7F8595AA);
const fwMvFloatId CTaskArrest::ms_AngleWeightId("AngleWeight",0x252D774A);
const fwMvFloatId CTaskArrest::ms_ShortPhase("ShortPhase",0xB3E0AFD0);
const fwMvFloatId CTaskArrest::ms_LongPhase("LongPhase",0x1E232283);
const fwMvFloatId CTaskArrest::ms_ShortDirectionPhase("ShortPhaseDirection",0x857C3C06);
const fwMvFloatId CTaskArrest::ms_LongDirectionPhase("LongPhaseDirection",0x211EDD2E);
const fwMvClipId CTaskArrest::ms_ShortClipId("ShortClip",0x7DC8A24);
const fwMvClipId CTaskArrest::ms_LongClipId("LongClip",0xDAAA9881);
const fwMvClipId CTaskArrest::ms_ShortDirectionClipId("ShortClipDirection",0x70B155EC);
const fwMvClipId CTaskArrest::ms_LongDirectionClipId("LongClipDirection",0xBFF843AD);

const strStreamingObjectName CTaskArrest::ms_ArrestDict("mp_arresting",0x1532946A);
const strStreamingObjectName CTaskArrest::ms_FullArrestDict("mp_arrest_paired",0x9BC7271C);
const strStreamingObjectName CTaskArrest::ms_FullUncuffDict("mp_uncuff_paired",0x3C055F2B);

tArrestSyncSceneClips CTaskArrest::ms_SyncSceneOptions[SSO_Max] = 
{
	//! arrester				arrestee				camera
	{	"a_arrest_on_floor",	"b_arrest_on_floor",	"cam_b_arrest_on_floor" },		//SSO_ArrestOnFloor
	{	"A_UNCUFF",				"B_UNCUFF",				"CAM_UNCUFF" },					//SSO_Uncuff
};

//////////////////////////////////////////////////////////////////////////
// CTaskArrest
//////////////////////////////////////////////////////////////////////////

static dev_float s_fTakeCustodyTime = 1.0f;

static dev_float fFullArrestMinDistance_NewAnims = 1.16f;
static dev_float fFullArrestMinDistance = 0.75f;
static dev_float fFullArrestMaxDistance = 2.4f;

static dev_float fFullUncuffMinDistance = 0.65f;
static dev_float fFullUncuffMaxDistance = 2.4f;

//! Debug - Don't turn it on by default as it conflicts with script arrest.
bool CTaskArrest::IsEnabled()
{
	return true;
}

CTaskArrest::CTaskArrest(CPed* pTargetPed, fwFlags8 iFlags)
: m_pTargetPed(pTargetPed)
, m_SyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_iFlags(iFlags)
, m_iSyncSceneOption(SSO_Max)
, m_BlockingBoundID(DYNAMIC_OBJECT_INDEX_NONE)
, m_fFullArrestPedHeading(0.0f)
, m_fDesiredCuffedPedGetupHeading(0.0f)
, m_fPairedAnimAngle(0.0f)
, m_vFullArrestTargetDirection(Vector3::ZeroType)
, m_vFullArrestTargetPos(Vector3::ZeroType)
, m_qFullArrestTargetOrientation(0.0f,0.0f,0.0f,1.0f)
, m_bStartedTargetAttachment(false)
, m_fStartAttachmentPhase(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_ARREST);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::CleanUp()
{
	// Clear any forced aim
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim, false);

	// Clear any lockon target
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ClearLockonTarget, true);

	// Blend out of move network
	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}

	m_ClipRequestHelper.ReleaseClips();
	m_ClipRequestHelperFull.ReleaseClips();

	// Clean up the synced scene
	DestroySyncedSceneCamera();

	// Release sync scene (if valid).
	ReleaseSyncedScene();

	if(GetPed()->IsLocalPlayer() && GetState() == State_SyncedScene_Playing)
	{
		camInterface::GetGameplayDirector().SetRelativeCameraHeading(0.0f);
		camInterface::GetGameplayDirector().SetRelativeCameraPitch(0.0f);
	}

	if(m_BlockingBoundID != DYNAMIC_OBJECT_INDEX_NONE)
	{
		CPathServerGta::RemoveBlockingObject(m_BlockingBoundID);
		m_BlockingBoundID = DYNAMIC_OBJECT_INDEX_NONE;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (iPriority != CTask::ABORT_PRIORITY_IMMEDIATE
		&& pEvent 
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_IN_AIR)
	{
		return false;
	}

	ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ShouldAbort - aborting arrest task! \n");

	if(pEvent)
	{
		ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ShouldAbort. Type = %d, Priority = %d, Task = %s\n", 
			pEvent->GetEventType(), 
			pEvent->GetEventPriority(), 
			pEvent->GetName().c_str());
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::ProcessPreFSM()
{
	if (!m_pTargetPed)
	{
		// Can't do anything with no target
		ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ProcessPreFSM - m_pTargetPed is NULL\n");
		return FSM_Quit;
	}

	if (!GetPed()->IsNetworkClone())
	{
		//! Sync scene is set to not abort, so we need to specifically check we are dead here to exit the task.
		if(GetPed()->IsDead())
		{
			if(GetPed()->GetPedAudioEntity())
			{
				GetPed()->GetPedAudioEntity()->StopArrestingLoop();
			}
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ProcessPreFSM - ped is dead!\n");
			return FSM_Quit;
		}
		//! Arrest ped has died. Note: we wouldn't ordinarily exit the sync scene after we have been cuffed, but this is the exception.
		else if(m_pTargetPed && m_pTargetPed->IsDead())
		{
			if(GetPed()->GetPedAudioEntity())
			{
				GetPed()->GetPedAudioEntity()->StopArrestingLoop();
			}
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ProcessPreFSM - target ped is dead!\n");
			return FSM_Quit;
		}

		if(GetState() > State_Start)
		{
			// Ensure ped hasn't switched tasks for some reason
			if(!m_iFlags.IsFlagSet(AF_TakeCustody))
			{
				if (!IsTargetPedRunningCuffedTask())
				{
					if(GetPed()->GetPedAudioEntity())
					{
						GetPed()->GetPedAudioEntity()->StopArrestingLoop();
					}
					ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ProcessPreFSM - Target ped not running cuffed task\n");
					return FSM_Quit;
				}
			}

			// Force the lock-on target to be the target ped
			if(CPlayerInfo *pPlayerInfo = GetPed()->GetPlayerInfo())
			{
				if(IsPerformingFallbackArrest())
				{	
					pPlayerInfo->GetTargeting().SetLockOnTarget(m_pTargetPed);
				}
			}
		}
	}

	if (GetPed()->IsLocalPlayer() && CanCancel())
	{
		// Release the arrest button to bail out
		if (GetPed()->GetControlFromPlayer()->GetPedArrest().IsUp())
		{
			if(GetPed()->GetPedAudioEntity())
			{
				GetPed()->GetPedAudioEntity()->StopArrestingLoop();
			}
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::ProcessPreFSM - arrest cancelled!\n");
			return FSM_Quit;
		}
	}

	//! Stop ped being pushed about.
	if(IsArresting())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);
	}

	if(GetState() == State_Full_Phase1)
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskArrest::FSM_Return CTaskArrest::ProcessPostFSM()
{
	//! Send constant MoVE signals.
	UpdateMoVE();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::UpdateMoVE()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		m_moveNetworkHelper.SetFloat(ms_PairedAnimAngle, m_fPairedAnimAngle);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	switch(GetState())
	{
	case(State_Full_Phase1):
		{
			if(m_moveNetworkHelper.IsNetworkActive() && m_pTargetPed)
			{
				TUNE_GROUP_FLOAT(ARREST, fFixUp_StartPhase, 0.2f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(ARREST, fFixUp_EndPhase, 0.9f, 0.0f, 1.0f, 0.01f);

				m_vFullArrestTargetPos = CalculateFullArrestTargetPosition();
				m_PlayAttachedClipHelper.SetTarget(m_vFullArrestTargetPos, m_qFullArrestTargetOrientation);

				float fCurrentPhase = Clamp(m_moveNetworkHelper.GetFloat(ms_ShortPhase), 0.0f, 1.0f);
				if(!m_pTargetPed->GetUsingRagdoll())
				{
					if(!m_bStartedTargetAttachment)
					{
						m_bStartedTargetAttachment = true;
						m_fStartAttachmentPhase = fCurrentPhase;
						m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());
					}
				}

				if(m_bStartedTargetAttachment)
				{
					Vector3 vNewPedPosition(Vector3::ZeroType);
					Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);
					m_PlayAttachedClipHelper.ComputeInitialSituation(vNewPedPosition, qNewPedOrientation);

					Vector3 vDisplacementFromInitial(Vector3::ZeroType);
					Quaternion qOrientationFromInitial(0.0f,0.0f,0.0f,1.0f);
					GetFullArrestDisplacementAndOrientation(vDisplacementFromInitial, qOrientationFromInitial, false, m_fStartAttachmentPhase);

					qNewPedOrientation.Transform(vDisplacementFromInitial);
					vNewPedPosition += vDisplacementFromInitial;

					Vector3 vDisplacementRemaining(Vector3::ZeroType);
					Quaternion qOrientationRemaining(0.0f,0.0f,0.0f,1.0f);
					GetFullArrestDisplacementAndOrientation(vDisplacementRemaining, qOrientationRemaining, true);

					// Rotate the translation relative to the initial rotation
					Vector3 vPosIncludingDisplacement = vDisplacementRemaining;
					qNewPedOrientation.Transform(vPosIncludingDisplacement);
					vPosIncludingDisplacement += vNewPedPosition;

					// Compute the difference to the target position, this is the total translational fixup we need to apply
					Vector3 vOffset = m_vFullArrestTargetPos - vPosIncludingDisplacement;

					// Apply fix up
					vNewPedPosition.x += m_PlayAttachedClipHelper.ComputeFixUpThisFrame(vOffset.x, fCurrentPhase, fFixUp_StartPhase, fFixUp_EndPhase);
					vNewPedPosition.y += m_PlayAttachedClipHelper.ComputeFixUpThisFrame(vOffset.y, fCurrentPhase, fFixUp_StartPhase, fFixUp_EndPhase);
					vNewPedPosition.z += m_PlayAttachedClipHelper.ComputeFixUpThisFrame(vOffset.z, fCurrentPhase, fFixUp_StartPhase, fFixUp_EndPhase);

#if __BANK
					m_vFullArrestDebugPosition = vNewPedPosition;

					if(!g_WorldLimits_AABB.ContainsPoint( VECTOR3_TO_VEC3V(vNewPedPosition) ))
					{
						taskDisplayf("--------------------------------------------------------");
						taskDisplayf("INVALID ARREST DETECTED!!!");

						taskDisplayf("vNewPedPosition = %f:%f:%f", vNewPedPosition.x, vNewPedPosition.y, vNewPedPosition.z);
						taskDisplayf("m_vFullArrestTargetPos = %f:%f:%f", m_vFullArrestTargetPos.x, m_vFullArrestTargetPos.y, m_vFullArrestTargetPos.z);
						taskDisplayf("vOffset = %f:%f:%f", vOffset.x, vOffset.y, vOffset.z);
						taskDisplayf("vDisplacementRemaining = %f:%f:%f", vDisplacementRemaining.x, vDisplacementRemaining.y, vDisplacementRemaining.z);
						taskDisplayf("vPosIncludingDisplacement = %f:%f:%f", vPosIncludingDisplacement.x, vPosIncludingDisplacement.y, vPosIncludingDisplacement.z);

						taskDisplayf("--------------------------------------------------------");
						
						taskAssertf(0, "Invalid arrest target position calculated! See DMKH");
					}
#endif
					GetPed()->SetPosition(vNewPedPosition);
				}
			}
		}
		break;
	default:
		break;
	}

	// Base class
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_SyncedScene_Prepare)
			FSM_OnEnter
				return SyncedScenePrepare_OnEnter();
			FSM_OnUpdate
				return SyncedScenePrepare_OnUpdate();

		FSM_State(State_SyncedScene_Playing)
			FSM_OnEnter
				return SyncedScenePlaying_OnEnter();
			FSM_OnUpdate
				return SyncedScenePlaying_OnUpdate();
			FSM_OnExit
				return SyncedScenePlaying_OnExit();

		FSM_State(State_Full_Prepare)
			FSM_OnEnter
				return FullPrepare_OnEnter();
			FSM_OnUpdate
				return FullPrepare_OnUpdate();

		FSM_State(State_Full_Phase1)
			FSM_OnEnter
				return FullPhase1_OnEnter();
			FSM_OnUpdate
				return FullPhase1_OnUpdate();

		FSM_State(State_Full_Phase2)
			FSM_OnUpdate
				return FullPhase2_OnUpdate();

		FSM_State(State_Full_Phase3)
			FSM_OnUpdate
				return FullPhase3_OnUpdate();

		FSM_State(State_Fallback_MoveToTarget)
			FSM_OnEnter
				return FallbackMoveToTarget_OnEnter();
			FSM_OnUpdate
				return FallbackMoveToTarget_OnUpdate();

		FSM_State(State_Fallback_FaceTarget)
			FSM_OnEnter
				return FallbackFaceTarget_OnEnter();
			FSM_OnUpdate
				return FallbackFaceTarget_OnUpdate();

		FSM_State(State_Fallback_PreparingToShove)
			FSM_OnEnter
				return FallbackPreparingToShove_OnEnter();
			FSM_OnUpdate
				return FallbackPreparingToShove_OnUpdate();

		FSM_State(State_Fallback_Shoving)
			FSM_OnUpdate
				return FallbackShoving_OnUpdate();

		FSM_State(State_Fallback_Cuffing)
			FSM_OnUpdate
				return FallbackCuffing_OnUpdate();
			FSM_OnExit
				return FallbackCuffing_OnExit();

		FSM_State(State_TakeCustody)
			FSM_OnEnter
				return TakeCustody_OnEnter();
			FSM_OnUpdate
				return TakeCustody_OnUpdate();
			FSM_OnExit
				return TakeCustody_OnExit();

		FSM_State(State_FinishedHandshake)
			FSM_OnEnter
				return FinishedHandShake_OnEnter();
			FSM_OnUpdate
				return FinishedHandShake_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				return Finished_OnEnter();
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskArrest::FSM_Return	CTaskArrest::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iEvent == OnUpdate && GetStateFromNetwork() != GetState())
	{
		// Sync with network
		switch (GetStateFromNetwork())
		{
		case State_Finish:
			SetState(State_Finish);
			break;
		//! Host is in sync scene.
		case(State_SyncedScene_Prepare):
		case(State_SyncedScene_Playing):
			if(GetState() == State_Start)
			{
				SetState(State_SyncedScene_Prepare);
			}
			break;
		//! Host is in new arrest.
		case(State_Full_Prepare):
		case(State_Full_Phase1):
		case(State_Full_Phase2):
		case(State_Full_Phase3):
			if(GetState() == State_Start)
			{
				SetState(State_Full_Prepare);
			}
			break;
		//! host is in fallback.
		case(State_Fallback_MoveToTarget):
		case(State_Fallback_FaceTarget):
		case(State_Fallback_PreparingToShove):
		case(State_Fallback_Shoving):
		case(State_Fallback_Cuffing):
			if(GetState() == State_Start)
			{
				SetState(State_Fallback_MoveToTarget);
			}
			break;
		case(State_TakeCustody):
			SetState(State_TakeCustody);
			break;
		case(State_FinishedHandshake):
			//! Note: This will ensure we finish on the clone side when the authoritative ped does. Not sure this the best thing to do, 
			//! so I'll see how it goes.
			SetState(State_FinishedHandshake);
			break;
		default:
			//! just simulate from here.
			break;
		}
	}

	return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskArrest::CreateQueriableState() const
{
	return rage_new CClonedTaskArrestInfo(GetState(), m_pTargetPed, (u8)m_iSyncSceneOption, m_iFlags);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_ARREST);
	CClonedTaskArrestInfo* pArrestInfo = static_cast<CClonedTaskArrestInfo*>(pTaskInfo);

	m_pTargetPed = pArrestInfo->GetTargetPed();
	m_iFlags = pArrestInfo->GetFlags();
	m_iSyncSceneOption = static_cast<SyncSceneOption>(pArrestInfo->GetSyncSceneOption());

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed))
{
	//! Don't move ped until we have selected arrest type.
	if(GetState() == State_Start)
	{
		return true;
	}

	//! If receiving new full arrest anims, override, as we don't sync the paired arrest anims. They are allowed to run locally 
	//! (as it doesn't really matter about angle of arrest etc).
	if(CArrestHelpers::UsingNewArrestAnims())
	{
		return IsPerformingFullArrest();
	}

	return GetState() == State_Full_Phase1;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::Start_OnEnter()
{	
	// Request animations ASAP.
	s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(ms_ArrestDict).Get();
	m_ClipRequestHelper.RequestClipsByIndex(iDictIndex);

	s32 iFullDictIndex = IsUnCuffTask() ? fwAnimManager::FindSlotFromHashKey(ms_FullUncuffDict).Get() : fwAnimManager::FindSlotFromHashKey(ms_FullArrestDict).Get() ;
	m_ClipRequestHelperFull.RequestClipsByIndex(iFullDictIndex);

	if (!IsEnabled())
	{
		ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::Start_OnEnter - Code Arrest not enabled!\n");
		return FSM_Quit;
	}

	if (!GetPed()->IsNetworkClone())
	{
		fwFlags8 cuffedFlags;
		if(IsUnCuffTask())
		{
			cuffedFlags.SetFlag(CF_UnCuff);
		}

		if(m_iFlags.IsFlagSet(AF_TakeCustody))
		{
			cuffedFlags.SetFlag(CF_TakeCustody);
		}

		if (m_pTargetPed->IsNetworkClone())
		{
			// Start cuffed task remotely
			CStartNetworkPedArrestEvent::Trigger(GetPed(), m_pTargetPed, cuffedFlags);
		}
		else
		{
			CTaskCuffed::CreateTask(m_pTargetPed, GetPed(), cuffedFlags);
		}
	}

	//! Create a blocking bound, so peds don't wander into our shot.
	if(!m_iFlags.IsFlagSet(AF_TakeCustody))
	{
		Vector3 vBlockingBoundPos = GetTargetPedPosition();
		const Vector3 vBlockingBounds(2.0f, 2.0f, 2.0f);
		Vector3 vOrientation = GetTargetPedPosition() - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		const float fHeading = rage::Atan2f(-vOrientation.x, vOrientation.y);
		m_BlockingBoundID = CPathServerGta::AddBlockingObject(vBlockingBoundPos, vBlockingBounds, fHeading, TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::Start_OnUpdate()
{
	if(m_iFlags.IsFlagSet(AF_TakeCustody))
	{
		SetState(State_TakeCustody);
	}
	// Wait for target to start the cuffed task
	else if (IsTargetPedRunningCuffedTask())
	{
		if(IsUnCuffTask())
		{
			SelectUnCuffState();
		}
		else
		{
			s32 iTargetPedState = GetTargetPedCuffedState();
			switch(iTargetPedState)
			{
				case(CTaskCuffed::State_WaitOnArrester):
					{
						SelectArrestState();
					}
					break;
				default:
					{
						// Target isn't in a valid state to be arrested or taken custody of. Just wait until they are. This state
						// will time out if they never get to a valid state.
					}
					break;
			}
		}
	}

	if (!GetPed()->IsNetworkClone())
	{
		// Time out check - target not able to be cuffed/put in custody
		if (GetTimeInState() > CArrestHelpers::GetArrestNetworkTimeout())
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::Start_OnUpdate - Time In State > Network Timeout (%f)\n", CArrestHelpers::GetArrestNetworkTimeout());
			if(GetPed()->GetPedAudioEntity())
			{
				GetPed()->GetPedAudioEntity()->StopArrestingLoop();
			}
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::SyncedScenePrepare_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::SyncedScenePrepare_OnUpdate()
{
	if (m_ClipRequestHelper.HaveClipsBeenLoaded())
	{
		SetState(State_SyncedScene_Playing);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::SyncedScenePlaying_OnEnter()
{
	// Start a new synced scene
	m_SyncedSceneId = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();
	if (!taskVerifyf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneId), "Failed to create synced scene!"))
	{
		if(GetPed()->GetPedAudioEntity())
		{
			GetPed()->GetPedAudioEntity()->StopArrestingLoop();
		}
		ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::PlayingSyncedScene_OnEnter - m_SyncedSceneId is invalid %d\n", m_SyncedSceneId);
		return FSM_Quit;
	}

	// Setup the scene origin. Need ground at ped position.

	Vector3 vScenePos = GetTargetPedPosition();
	bool bFound;
	float fGroundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE,vScenePos,&bFound);
	if(bFound)
	{
		vScenePos.z = fGroundZ;
	}

	Vector3 vOrientation = GetTargetPedPosition() - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	const float fHeading = rage::Atan2f(-vOrientation.x, vOrientation.y);

	Vector3 vHeading(0.0f, 0.0f, fHeading);
	Quaternion vSceneRot;
	CScriptEulers::QuaternionFromEulers(vSceneRot, vHeading, EULER_XYZ);

	fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(m_SyncedSceneId, vScenePos, vSceneRot);

	// Increment ref count
	fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(m_SyncedSceneId);

	// Set it up to play once and then end.
	fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(m_SyncedSceneId, false);
	fwAnimDirectorComponentSyncedScene::SetSyncedSceneHoldLastFrame(m_SyncedSceneId, false);

	eSyncedSceneFlagsBitSet flags;
	flags.BitSet().Set(SYNCED_SCENE_USE_KINEMATIC_PHYSICS);
	flags.BitSet().Set(SYNCED_SCENE_DONT_INTERRUPT);

	// Start the synced scene task
	taskAssert(m_iSyncSceneOption < SSO_Max);
	CTaskSynchronizedScene* pSyncAnimTask = 
		rage_new CTaskSynchronizedScene(m_SyncedSceneId, ms_SyncSceneOptions[m_iSyncSceneOption].m_ArresterClipId, ms_ArrestDict, INSTANT_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, flags);
	SetNewTask(pSyncAnimTask);

	// Force an anim update
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );

	// Setup the animated camera
	CreateSyncedSceneCamera();

	//TODO: Checked if the not sync-scene will use the same sounds.
	if(GetPed()->GetPedAudioEntity())
	{
		GetPed()->GetPedAudioEntity()->PlayArrestingLoop();
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::SyncedScenePlaying_OnUpdate()
{
	if (!fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneId))
	{
		SetState(State_Finish);
	}
	else
	{
		if(!GetPed()->IsNetworkClone())
		{
			const float phase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_SyncedSceneId);

			if (phase >= 1.0f)
			{
				SetState(State_FinishedHandshake);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::SyncedScenePlaying_OnExit()
{
	DestroySyncedSceneCamera();

	ReleaseSyncedScene();

	if (GetPed()->IsLocalPlayer())
	{
		camInterface::GetGameplayDirector().SetRelativeCameraHeading();
		camInterface::GetGameplayDirector().SetRelativeCameraPitch();
	}

	if(GetPed()->GetPedAudioEntity())
	{
		GetPed()->GetPedAudioEntity()->PlayCuffingSounds();
		GetPed()->GetPedAudioEntity()->StopArrestingLoop();
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FullPrepare_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FullPrepare_OnUpdate()
{
	// Load the move network to cuff the target if they're not cuffed already
	if (!m_moveNetworkHelper.GetNetworkPlayer())
	{
		if (m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskArrest) && m_ClipRequestHelperFull.HaveClipsBeenLoaded() )
		{
			m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskArrest);
			
			static dev_float fMaxRotAngle = (PI*0.5f); //! 90 degrees

			static dev_float fUseUpForHeadingThrehold = 0.5f;

			Vector3 vTargetPedPosition = GetTargetPedPosition();
			Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());

			Vector3 vToTargetPed = vTargetPedPosition - vPedPosition;
			vToTargetPed.z = 0.0f;

			float fMinDistance;
			float fMaxDistance;
			if(IsUnCuffTask())
			{
				fMinDistance = fFullUncuffMinDistance;
				fMaxDistance = fFullUncuffMaxDistance;
			}
			else
			{
				fMinDistance = CArrestHelpers::UsingNewArrestAnims() ? fFullArrestMinDistance_NewAnims : fFullArrestMinDistance;
				fMaxDistance = fFullArrestMaxDistance;	
			}

			const float fDistanceRatio = ClampRange(vToTargetPed.Mag(), fMinDistance, fMaxDistance);

			Vector3 vTargetPedDirection;

			//! Target heading isn't valid during ragdoll phase. pick most horizontal component of 
			//! transform to align to.
			Vector3 vTargetUpDir = VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetUp());
			Vector3 vTargetForwardDir = VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetForward());

			if(vTargetUpDir.Dot(ZAXIS) < fUseUpForHeadingThrehold)
			{
				vTargetPedDirection = -vTargetUpDir;
			}
			else
			{
				vTargetPedDirection = -vTargetForwardDir;
			}

			vTargetPedDirection.z = 0.0f;
			vTargetPedDirection.NormalizeFast();

			float fCuffedPedHeading = fwAngle::LimitRadianAngle(rage::Atan2f(vTargetPedDirection.x, -vTargetPedDirection.y));

			if(CArrestHelpers::UsingNewArrestAnims())
			{
				//! New system has no step around, the cuffed ped rotates during the getup.
				m_fFullArrestPedHeading = GetPed()->GetCurrentHeading();
				CalculateCuffedPedGetupHeading(GetPed()->GetCurrentHeading(), fCuffedPedHeading);
			}
			else
			{
				//! Old system requires arresting ped to step around to cuffed ped orientation.
				m_fFullArrestPedHeading = fCuffedPedHeading;
			}

			const float fHeadingDelta = SubtractAngleShorter(GetPed()->GetCurrentHeading(), m_fFullArrestPedHeading);
			const bool bOnRight = fHeadingDelta <= 0.0f;
			float fHeadingRatio = fabs(fHeadingDelta) / fMaxRotAngle;

			//! Check if it is too far for us to rotate. we can only rotate "fMaxRotAngle" degrees round target. 
			//! In this case, we need to recalc target heading, so that they share the workload.
			if(fHeadingRatio > 1.0f)
			{
				float fExcessAngle = fMaxRotAngle * (bOnRight ? 1.0f : -1.0f);
				m_fFullArrestPedHeading = fwAngle::LimitRadianAngle(GetPed()->GetCurrentHeading() + fExcessAngle);

				fHeadingRatio = 1.0f;
			}

			Matrix34 m;
			m.MakeRotateZ(m_fFullArrestPedHeading);
			m_vFullArrestTargetDirection = -m.b;
			m.ToQuaternion(m_qFullArrestTargetOrientation);

			// Setup move network parameters
			m_moveNetworkHelper.SetFloat(ms_HeadingDelta, fHeadingRatio);
			m_moveNetworkHelper.SetFloat(ms_Distance, fDistanceRatio);
			m_moveNetworkHelper.SetFlag(bOnRight, ms_MoveRight);
			m_moveNetworkHelper.SetFlag(IsUnCuffTask(), ms_UseUncuff);

			m_moveNetworkHelper.SetFlag(CArrestHelpers::UsingNewArrestAnims(), ms_UseNewAnims);

			// Start the animation
			GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION, CMovePed::Task_TagSyncTransition);

			SetState(State_Full_Phase1);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FullPhase1_OnEnter()
{
	// Force an anim update
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );

	// Store the offsets to the seat situation so we can recalculate the starting situation each frame relative to the seat
	m_vFullArrestTargetPos = CalculateFullArrestTargetPosition();
	m_PlayAttachedClipHelper.SetTarget(m_vFullArrestTargetPos, m_qFullArrestTargetOrientation);
	m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());

	m_uFullStartTimeMs = fwTimer::GetTimeInMilliseconds();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FullPhase1_OnUpdate()
{
	if (m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean(ms_Phase1Complete))
	{
		SetState(State_Full_Phase2);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FullPhase2_OnUpdate()
{
	if (m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean(ms_Phase2Complete))
	{
		SetState(State_Full_Phase3);
	}

	if(CanMoveToFinishHandshake())
	{
		SetState(State_FinishedHandshake);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FullPhase3_OnUpdate()
{
	if(CanMoveToFinishHandshake())
	{
		SetState(State_FinishedHandshake);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FinishedHandShake_OnEnter()
{
	if(m_iFlags.IsFlagSet(AF_TakeCustody))
	{
		if(m_pTargetPed && !m_pTargetPed->IsNetworkClone())
		{
			m_pTargetPed->SetInCustody(true, GetPed());
		}
	}
	else
	{
		CTaskCuffed *pCuffedTask = GetTargetPedCuffedTask();
		if(pCuffedTask)
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::FinishedHandShake_OnEnter - OnArrestPedFinished!\n");
			pCuffedTask->OnArrestPedFinishedHandshake();
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FinishedHandShake_OnUpdate()
{
	if(!GetPed()->IsNetworkClone())
	{
		if(m_iFlags.IsFlagSet(AF_TakeCustody))
		{
			if(m_pTargetPed && m_pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
			{
				ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::FinishedHandShake_OnUpdate - Custody ped is now in custody!\n");
				SetState(State_Finish);
			}
			else if (GetTimeInState() > CArrestHelpers::GetArrestNetworkTimeout())
			{
				ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::FinishedHandShake_OnUpdate - Time In State > Network Timeout (%f)\n", CArrestHelpers::GetArrestNetworkTimeout());
				SetState(State_Finish);
			}
		}
		else
		{
			//! If we are arresting/uncuffing a clone ped, then just wait for that task to finish. It'll just exit, which will 
			//! clean this task up in ProcessPreFSM().
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::Finished_OnEnter()
{
	if(GetPed()->IsNetworkClone())
	{
		CTaskCuffed *pTaskCuffed = GetTargetPedCuffedTask();
		if(pTaskCuffed)
		{
			pTaskCuffed->OnArrestPedQuit();
		}
		CTaskInCustody *pTaskCustody = GetTargetPedInCustodyTask();
		if(pTaskCustody)
		{
			pTaskCustody->OnCustodianPedQuit();
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackMoveToTarget_OnEnter()
{
	// Ensure we're locked on to the target
	if (CPlayerInfo *pPlayerInfo = GetPed()->GetPlayerInfo())
	{
		pPlayerInfo->GetTargeting().SetLockOnTarget(m_pTargetPed);
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim, true);
	}

	if (!GetPed()->IsNetworkClone())
	{
		static dev_float fTargetRadius = 0.1f;
		static dev_float fApproachDistanceMin = 1.0f;
		static dev_float fApproachDistanceMax = 1.5f;

		const Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		const Vector3 vTargetPos = GetTargetPedPosition();
		const Vector3 vToTarget = vTargetPos - vPedPos;
		const float fTargetDist = vToTarget.Mag();

		// Abort if right on top of target somehow
		if (fTargetDist <= FLOAT_EPSILON)
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::MoveToTarget_OnEnter - on top of target!\n");
			return FSM_Quit;
		}

		// Approach the target to be within the min and max distances
		Vector3 vMovePos = vPedPos;
		if (fTargetDist < fApproachDistanceMin)
		{
			// Move away from target
			vMovePos = vTargetPos - (vToTarget * (fApproachDistanceMin  / fTargetDist));
		}
		else if (fTargetDist > fApproachDistanceMax)
		{
			// Move towards target
			vMovePos = vTargetPos - (vToTarget * (fApproachDistanceMax  / fTargetDist));
		}

		// Create the movement task
		CTask* pTaskGoToPoint = rage_new CTaskMoveGoToPointAndStandStill(
			MOVEBLENDRATIO_WALK, 
			vMovePos, 
			fTargetRadius,
			CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance,
			false, true);

		// Aim at the target with the held weapon while approaching
		const s32 iWeaponFlags = GF_ForceAimState | GF_SkipIdleTransitions | GF_UpdateTarget;
		CTaskWeapon* pTaskWeapon = rage_new CTaskWeapon(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, iWeaponFlags, 0, CWeaponTarget(m_pTargetPed));

		SetNewTask(rage_new CTaskComplexControlMovement(pTaskGoToPoint, pTaskWeapon));
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackMoveToTarget_OnUpdate()
{
	if (!GetPed()->IsNetworkClone())
	{
		// Time out check - took too long to move to target (obstructed?)
		const float fMoveToTargetTimeout = 3.0f;
		if (GetTimeInState() > fMoveToTargetTimeout)
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::MoveToTarget_OnUpdate - Time In State > Network Timeout (%f)\n", fMoveToTargetTimeout);
			return FSM_Quit;
		}

		if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			// Keep the weapon aiming subtask active
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

			// Make sure we're facing the target
			SetState(State_Fallback_FaceTarget);
		}
	}
	else 
	{
		// Sync with network
		if (GetStateFromNetwork() >= State_Fallback_FaceTarget)
		{
			SetState(State_Fallback_FaceTarget);
		}
	}


	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackFaceTarget_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackFaceTarget_OnUpdate()
{
	if (!GetPed()->IsNetworkClone())
	{
		static dev_float fHeadingDeltaThreshold = 5.0f * DtoR;

		const Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		const Vector3 vTargetPos = GetTargetPedPosition();
		//const Vector3 vToTarget = vTargetPos - vPedPos;
		const float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPedPos.x, vPedPos.y);
		GetPed()->SetDesiredHeading(fDesiredHeading);

		// The aim motion will prevent the feet from turning if the target is within a threshold greater than we'd like for the synced anims,
		// so force the heading to turn towards the desired
		const float fHeadingDiff = SubtractAngleShorter(fDesiredHeading, GetPed()->GetCurrentHeading());
		const float fHeadingDelta = Min(Abs(fHeadingDiff), GetPed()->GetHeadingChangeRate() * fwTimer::GetTimeStep());
		if (fHeadingDelta <= fHeadingDeltaThreshold)
		{
			// Shove 'em and cuff 'em
			SetState(State_Fallback_PreparingToShove);
		}
		else
		{
			GetPed()->GetMotionData()->SetExtraHeadingChangeThisFrame(Sign(fHeadingDiff) * fHeadingDelta);
		}		
	}
	else 
	{
		// Sync with network
		if (GetStateFromNetwork() >= State_Fallback_PreparingToShove)
		{
			SetState(State_Fallback_PreparingToShove);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackPreparingToShove_OnEnter()
{
	// Snap heading
	GetPed()->SetHeading(GetPed()->GetDesiredHeading());
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackPreparingToShove_OnUpdate()
{
	// Load the move network to cuff the target if they're not cuffed already
	if (!m_moveNetworkHelper.GetNetworkPlayer())
	{
		if (m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskArrest) && m_ClipRequestHelper.HaveClipsBeenLoaded() )
		{
			m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskArrest);

			const float fPedHeading = GetPed()->GetCurrentHeading();
			const float fTargetPedHeading = m_pTargetPed->GetCurrentHeading();
			const float fHeadingDelta = SubtractAngleShorter(fPedHeading, fTargetPedHeading);
			const bool bShoveRight = fHeadingDelta >= 0.0f;
			const float fHeadingDeltaAbs = fabs(fHeadingDelta);

			static dev_float fMinDistance = 1.0f;
			static dev_float fMaxDistance = 1.5f;
			Vector3 vTargetPedPos = GetTargetPedPosition();
			const float fDistance = Dist(GetPed()->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vTargetPedPos)).Getf();
			const float fDistanceRatio = ClampRange(fDistance, fMinDistance, fMaxDistance);

			// Setup move network parameters
			m_moveNetworkHelper.SetFloat(ms_HeadingDelta, fHeadingDeltaAbs / PI);
			m_moveNetworkHelper.SetFloat(ms_Distance, fDistanceRatio);
			m_moveNetworkHelper.SetFlag(bShoveRight, ms_MoveRight);
			m_moveNetworkHelper.SetFlag(true, ms_UseFallback);

			// Start the animation
			GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		}
	}

	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		// Wait for shove tag
		if (m_moveNetworkHelper.GetProperty(ms_Shove))
		{
			SetState(State_Fallback_Shoving);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackShoving_OnUpdate()
{
	// Wait for animation
	if (m_moveNetworkHelper.GetProperty(ms_Handcuffed))
	{
		if(GetPed()->GetPedAudioEntity())
		{
			GetPed()->GetPedAudioEntity()->PlayCuffingSounds();
		}
		SetState(State_Fallback_Cuffing);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackCuffing_OnUpdate()
{
	// Wait for animation to end
	if(!GetPed()->IsNetworkClone())
	{
		if (!m_moveNetworkHelper.IsNetworkActive()
			|| m_moveNetworkHelper.GetBoolean(ms_Ended)
			|| GetFallbackPhase() >= 1.0f)
		{
			SetState(State_FinishedHandshake);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::FallbackCuffing_OnExit()
{
	GetPed()->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::TakeCustody_OnEnter()
{
	//! Tell the ped that they are being taken into custody. This allows us to determine that the ped is being taken into 
	//! custody even if they can't run the custody task (e.g. it's blocked due to ragdoll).
	if(!m_pTargetPed->IsNetworkClone())
	{
		m_pTargetPed->SetCustodian(GetPed()); 
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::TakeCustody_OnUpdate()
{
	if (!GetPed()->IsNetworkClone() && (GetTimeInState() > s_fTakeCustodyTime) )
	{
		SetState(State_FinishedHandshake);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskArrest::TakeCustody_OnExit()
{
	if(!m_pTargetPed->IsNetworkClone())
	{
		//! Unset custodian if we have failed.
		if(GetState() != State_FinishedHandshake && 
			!m_pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody) && 
			m_pTargetPed->GetCustodian() == GetPed())
		{
			m_pTargetPed->SetCustodian(NULL); 
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsTargetPedRunningCuffedTask() const
{
	const CTaskCuffed* pTaskCuffed = static_cast<const CTaskCuffed*>(m_pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CUFFED));
	if (pTaskCuffed)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsTargetPedRunningInCustodyTask() const
{
	const CTaskInCustody* pTaskInCustody = static_cast<const CTaskInCustody*>(m_pTargetPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_IN_CUSTODY));
	if (pTaskInCustody)
	{
		return true;
	}

	return GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_CUSTODY);
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskArrest::GetTargetPedCuffedState() const
{
	const CTaskCuffed* pTaskCuffed = GetTargetPedCuffedTask();
	if (pTaskCuffed)
	{
		return pTaskCuffed->GetState();
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////

CTaskCuffed *CTaskArrest::GetTargetPedCuffedTask() const
{
	return static_cast<CTaskCuffed*>(m_pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CUFFED));
}

////////////////////////////////////////////////////////////////////////////////

CTaskInCustody *CTaskArrest::GetTargetPedInCustodyTask() const
{
	return static_cast<CTaskInCustody*>(m_pTargetPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_IN_CUSTODY));
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::SelectUnCuffState()
{
	TUNE_GROUP_BOOL(ARREST,bForceNewUncuff,true);

	if(!GetPed()->IsNetworkClone())
	{
		// Don't move. And Aim.
		CTaskMoveStandStill* pTaskMoveStandStill = rage_new CTaskMoveStandStill();

		SetNewTask(rage_new CTaskComplexControlMovement(pTaskMoveStandStill, NULL));

		if(bForceNewUncuff)
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::SelectUnCuffState - New Uncuff! \n");
			m_iFlags.SetFlag(AF_FullSelected);
			SetState(State_Full_Prepare);
		}
		else if (CArrestHelpers::IsArrestTypeValid(GetPed(), m_pTargetPed, CArrestHelpers::ARREST_UNCUFFING))
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::SelectUnCuffState - sync scene uncuff! \n");
			m_iSyncSceneOption = SSO_Uncuff;
			SetState(State_SyncedScene_Prepare);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::SelectArrestState()
{
	TUNE_GROUP_BOOL(ARREST,bForceFullArrest,true);
	TUNE_GROUP_BOOL(ARREST,bForceFallback,false);

	if(!GetPed()->IsNetworkClone())
	{
		// Don't move. And Aim.
		CTaskMoveStandStill* pTaskMoveStandStill = rage_new CTaskMoveStandStill();

		const s32 iWeaponFlags = GF_ForceAimState | GF_SkipIdleTransitions | GF_UpdateTarget;
		CTaskWeapon* pTaskWeapon = rage_new CTaskWeapon(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, iWeaponFlags, 0, CWeaponTarget(m_pTargetPed));
		SetNewTask(rage_new CTaskComplexControlMovement(pTaskMoveStandStill, pTaskWeapon));

		if(bForceFullArrest)
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::SelectArrestState - New Arrest! \n");
			m_iFlags.SetFlag(AF_FullSelected);
			SetState(State_Full_Prepare);
		}
		else if(bForceFallback || !CArrestHelpers::IsArrestTypeValid(GetPed(), m_pTargetPed, CArrestHelpers::ARREST_CUFFING))
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::SelectArrestState - Fallback Arrest! \n");
			m_iFlags.SetFlag(AF_FallbackSelected);
			SetState(State_Fallback_MoveToTarget);	
		}
		else
		{
			ARREST_DISPLAYF(GetPed(), m_pTargetPed, "CTaskArrest::SelectArrestState - Sync Scene Arrest! \n");
			m_iSyncSceneOption = SSO_ArrestOnFloor;
			SetState(State_SyncedScene_Prepare);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::CreateSyncedSceneCamera()
{
	bool bLocalPlayerInvolved = GetPed()->IsLocalPlayer() || m_pTargetPed->IsLocalPlayer();

	if(bLocalPlayerInvolved)
	{
		const s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(ms_ArrestDict).Get();

		taskAssert(m_iSyncSceneOption < SSO_Max);
		u32 iCamHash = ms_SyncSceneOptions[m_iSyncSceneOption].m_CameraClipId.GetHash();
		const crClip* clip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex, iCamHash);

		if (taskVerifyf(clip, "Couldn't find camera animation for arrest synced scene"))
		{
			Matrix34 sceneOrigin;
			fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(m_SyncedSceneId, sceneOrigin);
			camInterface::GetSyncedSceneDirector().AnimateCamera(ms_ArrestDict, *clip, sceneOrigin, m_SyncedSceneId, 0 , camSyncedSceneDirector::TASK_ARREST_CLIENT); 
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::DestroySyncedSceneCamera()
{
	camInterface::GetSyncedSceneDirector().StopAnimatingCamera(camSyncedSceneDirector::TASK_ARREST_CLIENT); 
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::ReleaseSyncedScene()
{
	if(m_SyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		if(taskVerifyf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneId),
			"Expected the synced scene to be valid since we have a ref-count on it, but something must have destroyed it."))
		{
			fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedSceneId);
		}
		m_SyncedSceneId = INVALID_SYNCED_SCENE_ID;
	}
}

////////////////////////////////////////////////////////////////////////////////

const strStreamingObjectName* CTaskArrest::GetArresteeClip() const 
{
	if(m_iSyncSceneOption < SSO_Max)
	{
		return &ms_SyncSceneOptions[m_iSyncSceneOption].m_ArresteeClipId;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskArrest::GetPhase() const
{
	if(GetState() == State_FinishedHandshake)
	{
		return 1.0f;
	}
	else if(GetState() == State_TakeCustody)
	{
		return Min(1.0f, (GetTimeInState()/s_fTakeCustodyTime));
	}
	else if(IsPerformingSyncedScene() && fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneId))
	{
		return fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_SyncedSceneId);
	}
	else if(IsPerformingFallbackArrest())
	{
		return GetFallbackPhase();
	}
	else if(IsPerformingFullArrest() || IsPerformingFullUncuff())
	{
		return GetFullPhase();
	}

	return 0.0f;
}

float CTaskArrest::GetFallbackPhase() const
{
	float fInterruptPhase = 0.0f;
	if(m_moveNetworkHelper.GetNetworkPlayer())
	{
		float fShortPhase = m_moveNetworkHelper.GetFloat(ms_ShortPhase);
		float fLongPhase = m_moveNetworkHelper.GetFloat(ms_LongPhase);
		float fWeight = m_moveNetworkHelper.GetFloat(ms_DistanceWeightId);

		const crClip *pClipShort = m_moveNetworkHelper.GetClip(ms_ShortClipId);
		if(pClipShort)
		{
			float fShortInterruptPhase = 1.0f;
			if(CClipEventTags::FindEventPhase(pClipShort, ms_Interruptible, fShortInterruptPhase))
			{
				fInterruptPhase += (fShortInterruptPhase * (1.0f-fWeight));
			}
		}
		const crClip *pClipLong = m_moveNetworkHelper.GetClip(ms_LongClipId);
		if(pClipLong)
		{
			float fLongInterruptPhase = 1.0f;
			if(CClipEventTags::FindEventPhase(pClipLong, ms_Interruptible, fLongInterruptPhase))
			{
				fInterruptPhase += (fLongInterruptPhase * fWeight);
			}
		}

		float fCurrentPhase = (fShortPhase * (1.0f - fWeight)) + (fLongPhase * fWeight);
		float fEndPhase = fInterruptPhase > 0.0f ? fInterruptPhase : 1.0f;

		return fCurrentPhase/fEndPhase;
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskArrest::GetFullPhase() const
{
	//! Grab Clip ID Names.
	
	const crClip* pClipP1 = NULL;
	const crClip* pClipP2 = NULL;
	const crClip* pClipP3 = NULL;

	if(const_cast<CTaskArrest*>(this)->m_ClipRequestHelperFull.HaveClipsBeenLoaded())
	{
		if(IsUnCuffTask())
		{
			static const fwMvClipId s_UncuffClipIds[3] = { fwMvClipId("crook_01_p1_lf_fwd_0",0x25C232A8),fwMvClipId("crook_01_p2_fwd",0xCBE405B8), fwMvClipId("crook_01_p3_fwd",0x22D55DC5) };

			s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(ms_FullUncuffDict).Get();

			pClipP1 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_UncuffClipIds[0].GetHash());
			pClipP2 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_UncuffClipIds[1].GetHash());
			pClipP3 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_UncuffClipIds[2].GetHash());
		}
		else
		{
			static const fwMvClipId s_ArrestClipP1("cop_p1_lf_fwd_0",0xA9D47D53);
			static const fwMvClipId s_ArrestClipP2("cop_p2_fwd",0xC977DAB);
			static const fwMvClipId s_ArrestClipP2_New("cop_p2_fwd_right",0x7c654bf5);
			static const fwMvClipId s_ArrestClipP3("cop_p3_fwd",0x721B6EC5);

			s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(ms_FullArrestDict).Get();

			pClipP1 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_ArrestClipP1.GetHash());

			if(CArrestHelpers::UsingNewArrestAnims())
			{
				pClipP2 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_ArrestClipP2_New.GetHash());
			}
			else
			{
				pClipP2 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_ArrestClipP2.GetHash());
				pClipP3 = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, s_ArrestClipP3.GetHash());
			}
		}
	}

	if(pClipP1 || pClipP2 || pClipP3)
	{
		u32 uCurrentTimeMS = fwTimer::GetTimeInMilliseconds() - m_uFullStartTimeMs;
		float fCurrentTime = uCurrentTimeMS * 0.001f;

		float fClip1Duration = pClipP1 ? pClipP1->GetDuration() : 0.0f;
		float fClip2Duration = pClipP2 ? pClipP2->GetDuration() : 0.0f;
		float fClip3Duration = pClipP3 ? pClipP3->GetDuration() : 0.0f;

		float fDuration = fClip1Duration + fClip2Duration + fClip3Duration;
		return fCurrentTime/fDuration;
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::CalculateCuffedPedGetupHeading(float fArrestPedHeading, float fCuffedPedHeading)
{
	//! Calculate heading difference to nearest 45 degree boundary, and set that to be the get up heading.

	static dev_float s_fAngles[4] = 
	{	(-45.0f * DtoR),
	( 45.0f * DtoR),
	( 135.0f * DtoR),
	(-135.0f * DtoR),
	};

	//! Loop through and see which boundary we are closest to.

	int bestIndex = 0;
	float fBestHeadingDiff = 9999.0f;
	float fBestHeading = fCuffedPedHeading;

	for(int i = 0; i < 4; ++i)
	{
		f32 fTestHeading = fwAngle::LimitRadianAngle(fArrestPedHeading + s_fAngles[i]);

		float fHeadingDiff = SubtractAngleShorter(fCuffedPedHeading, fTestHeading);

		if(fabs(fHeadingDiff) < fabs(fBestHeadingDiff))
		{
			fBestHeadingDiff = fHeadingDiff;
			fBestHeading = fTestHeading;
			bestIndex = i;
		}
	}

	m_fDesiredCuffedPedGetupHeading = fBestHeading;

	m_fPairedAnimAngle = (float)bestIndex;

#if __BANK
	if(CArrestDebug::GetPairedAnimOverride() > 0.0f)
	{
		m_fPairedAnimAngle = CArrestDebug::GetPairedAnimOverride();
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::CanMoveToFinishHandshake()
{
	if(!GetPed()->IsNetworkClone())
	{
		if (!m_moveNetworkHelper.IsNetworkActive()
			|| m_moveNetworkHelper.GetBoolean(ms_Ended)
			|| GetPhase() >= 1.0f)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskArrest::GetDesiredFullArrestHeading() const
{
	return m_fFullArrestPedHeading;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskArrest::GetDesiredCuffedPedGetupHeading() const
{
	if(CArrestHelpers::UsingNewArrestAnims())
	{
		return m_fDesiredCuffedPedGetupHeading;
	}

	//! same as arrest ped (we try and line them up the same).
	return m_fFullArrestPedHeading;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskArrest::GetPairedAnimAngle() const
{
	return m_fPairedAnimAngle;
}

////////////////////////////////////////////////////////////////////////////////


bool CTaskArrest::IsPerformingSyncedScene() const 
{ 
	return GetState() >= State_SyncedScene_Prepare && GetState() <= State_SyncedScene_Playing; 
}

////////////////////////////////////////////////////////////////////////////////


bool CTaskArrest::IsPerformingFullArrest() const 
{ 
	if(!IsUnCuffTask())
	{
		return GetState() >= State_Full_Prepare && GetState() <= State_Full_Phase3; 
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////


bool CTaskArrest::IsPerformingFullUncuff() const 
{ 
	if(IsUnCuffTask())
	{
		return GetState() >= State_Full_Prepare && GetState() <= State_Full_Phase3; 
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////


bool CTaskArrest::IsPerformingSyncedSceneArrest() const 
{ 
	return IsPerformingSyncedScene() && !IsUnCuffTask();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsPerformingSyncedSceneUnCuff() const 
{ 
	return IsPerformingSyncedScene() && IsUnCuffTask();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsPerformingFallbackArrest() const 
{ 
	return GetState() >= State_Fallback_MoveToTarget && GetState() <= State_Fallback_Cuffing; 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsArresting() const 
{ 
	return IsPerformingSyncedSceneArrest() || IsPerformingFallbackArrest() || IsPerformingFullArrest(); 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsUnCuffing() const 
{ 
	return IsPerformingSyncedSceneUnCuff() || IsPerformingFullUncuff(); 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::CanCancel() const 
{ 
	if(!m_iFlags.IsFlagSet(AF_TakeCustody))
	{
		//! Check if our target has set his handcuffed flag. If so, then we can't cancel (it's too late!).
		if(IsUnCuffTask() && !m_pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		{
			return false;
		}
		if(!IsUnCuffTask() && m_pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		{
			return false;
		}

		//! Check if our target is allowing an interrupt.
		if(IsPerformingSyncedScene() && !GetTargetPedCuffedTask()->CanInterruptSyncScene())
		{
			return false;
		}
	}

	//! Allows the arrest to be synced to clone ped. This prevents clone ped sitting about waiting 
	//! for a task that will never arrive.
	static dev_float s_fMinCancelTime = 0.2f;
	if(GetTimeRunning() < s_fMinCancelTime)
	{
		return false;
	}

	return GetState() >= State_Start && GetState() <= State_TakeCustody; 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskArrest::IsUnCuffTask() const
{
	return m_iFlags.IsFlagSet(AF_UnCuff);
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskArrest::CalculateFullArrestTargetPosition()
{
	float fMinDistance;
	if(IsUnCuffTask())
	{
		fMinDistance = fFullUncuffMinDistance;
	}
	else
	{
		fMinDistance = CArrestHelpers::UsingNewArrestAnims() ? fFullArrestMinDistance_NewAnims : fFullArrestMinDistance;
	}

	Vector3 vTargetPedPosition = GetTargetPedPosition();
	Vector3 vFullArrestTargetPos = vTargetPedPosition + (m_vFullArrestTargetDirection * fMinDistance);
	m_PlayAttachedClipHelper.FindGroundPos(m_pTargetPed, vFullArrestTargetPos, 2.0f, vFullArrestTargetPos);
	return vFullArrestTargetPos;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskArrest::GetFullArrestDisplacementAndOrientation(Vector3 &vDisplacementOut, Quaternion &qOrientationOut, bool bCalcRemaining, float fStartPhaseOverride) const
{
	const crClip *pClipShort = m_moveNetworkHelper.GetClip(ms_ShortClipId);
	const crClip *pClipLong = m_moveNetworkHelper.GetClip(ms_LongClipId);
	const crClip *pClipShortDir = m_moveNetworkHelper.GetClip(ms_ShortDirectionClipId);
	const crClip *pClipLongDir = m_moveNetworkHelper.GetClip(ms_LongDirectionClipId);

	//! Note: phase is consistent across all clips, so only need to get 1 of them.
	float fCurrentPhase = Clamp(m_moveNetworkHelper.GetFloat(ms_ShortPhase), 0.0f, 1.0f);
	float fStartPhase = bCalcRemaining ? fCurrentPhase : fStartPhaseOverride;
	float fEndPhase = bCalcRemaining ? 1.0f : fCurrentPhase;

	Vector3 vShortDisplacement(Vector3::ZeroType);
	if(pClipShort)
	{
		vShortDisplacement = fwAnimHelpers::GetMoverTrackDisplacement(*pClipShort, fStartPhase, fEndPhase);
	}

	Vector3 vLongDisplacement(Vector3::ZeroType);
	if(pClipLong)
	{
		vLongDisplacement = fwAnimHelpers::GetMoverTrackDisplacement(*pClipLong, fStartPhase, fEndPhase);
	}

	Vector3 vShortDirDisplacement(Vector3::ZeroType);
	if(pClipShortDir)
	{
		vShortDirDisplacement = fwAnimHelpers::GetMoverTrackDisplacement(*pClipShortDir, fStartPhase, fEndPhase);
	}

	Vector3 vLongDirDisplacement(Vector3::ZeroType);
	if(pClipLongDir)
	{
		vLongDirDisplacement = fwAnimHelpers::GetMoverTrackDisplacement(*pClipLongDir, fStartPhase, fEndPhase);
	}

	float fDistanceWeight = Clamp(m_moveNetworkHelper.GetFloat(ms_DistanceWeightId), 0.0f, 1.0f);
	float fDistanceDirectionWeight = Clamp(m_moveNetworkHelper.GetFloat(ms_DistanceDirectionWeightId), 0.0f, 1.0f);

	Vector3 vDistanceDisplacement = vShortDisplacement * (1.0f-fDistanceWeight) + (vLongDisplacement * fDistanceWeight);
	Vector3 vDistanceDirectionDisplacement = vShortDirDisplacement * (1.0f-fDistanceDirectionWeight) + (vLongDirDisplacement * fDistanceDirectionWeight);

	float fAngleWeight = Clamp(m_moveNetworkHelper.GetFloat(ms_AngleWeightId), 0.0f, 1.0f);
	vDisplacementOut = vDistanceDisplacement * (1.0f-fAngleWeight) + (vDistanceDirectionDisplacement * fAngleWeight);

	qOrientationOut = Quaternion(0.0f,0.0f,0.0f,1.0f);
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskArrest::GetTargetPedPosition() const
{
	Vector3 vTargetPosition(Vector3::ZeroType);

	if(m_pTargetPed)
	{
		if(m_pTargetPed->GetUsingRagdoll())
		{
			Matrix34 targetMatrix;
			m_pTargetPed->GetBoneMatrix(targetMatrix, BONETAG_ROOT); 
			vTargetPosition = targetMatrix.d;
		}
		else
		{
			vTargetPosition = VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition());
		}
	}

	return vTargetPosition;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskArrest::Debug() const
{
#if __BANK
	Vector3 vTargetHeading = m_vFullArrestTargetPos + (-m_vFullArrestTargetDirection * 5.0f);
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(m_vFullArrestTargetPos, vTargetHeading, Color_red));
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vFullArrestTargetPos, 0.2f, Color_green, true));
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vFullArrestDebugPosition, 0.15f, Color_blue, true));
#endif
}

const char * CTaskArrest::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:						return "State_Start";
		case State_SyncedScene_Prepare:			return "State_SyncedScene_Prepare";
		case State_SyncedScene_Playing:			return "State_SyncedScene_Playing";
		case State_Full_Prepare:				return "State_Full_Prepare";
		case State_Full_Phase1:					return "State_Full_Phase1";
		case State_Full_Phase2:					return "State_Full_Phase2";
		case State_Full_Phase3:					return "State_Full_Phase3";
		case State_Fallback_MoveToTarget:		return "State_Fallback_MoveToTarget";
		case State_Fallback_FaceTarget:			return "State_Fallback_FaceTarget";
		case State_Fallback_PreparingToShove:	return "State_Fallback_PreparingToShove";
		case State_Fallback_Shoving:			return "State_Fallback_Shoving";
		case State_Fallback_Cuffing:			return "State_Fallback_Cuffing";
		case State_TakeCustody:					return "State_TakeCustody";
		case State_FinishedHandshake:			return "State_FinishedHandshake";
		case State_Finish:						return "State_Finish";
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CClonedTaskArrestInfo
//////////////////////////////////////////////////////////////////////////

CClonedTaskArrestInfo::CClonedTaskArrestInfo(s32 iState, CPed* pTargetPed, u8 iSyncSceneOption, u8 iFlags)
: m_TargetPedId((pTargetPed && pTargetPed->GetNetworkObject()) ? pTargetPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID)
, m_iSyncSceneOption(iSyncSceneOption)
, m_iFlags(iFlags)
{
	CClonedFSMTaskInfo::SetStatusFromMainTaskState(iState);
}

CClonedTaskArrestInfo::CClonedTaskArrestInfo()
: m_iFlags(0)
, m_iSyncSceneOption(SSO_Max)
{}

CClonedTaskArrestInfo::~CClonedTaskArrestInfo()
{}

CPed* CClonedTaskArrestInfo::GetTargetPed()
{
	return NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_TargetPedId));
}

#endif // ENABLE_TASKS_ARREST_CUFFED
