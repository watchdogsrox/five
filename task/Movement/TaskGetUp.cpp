// Filename   :	TaskBlendFromNM.cpp
// Description:	Natural Motion blend from natural motion class (FSM version)
//
// --- Include Files ------------------------------------------------------------

#include "Task/movement/TaskGetUp.h"

// C headers
// Rage headers

#include "crparameterizedmotion/pointcloud.h"
#include "crskeleton/Skeleton.h"
#include "fragment/Cache.h"
#include "fragment/Instance.h"
#include "fragment/Type.h"
#include "fragment/TypeChild.h"
#include "fragmentnm/messageparams.h"
#include "math/angmath.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"

// Game headers
#include "ai/stats.h"
#include "animation/AnimManager.h"
#include "Animation/moveped.h"
#include "Animation/debug/AnimViewer.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/helpers/StickyAimHelper.h"
#include "Event/EventDamage.h"
//#include "ik/ikmanagersolvertypes.h"
#include "ik/solvers/RootSlopeFixupSolver.h"
#include "modelinfo/PedModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Prediction/NetBlenderPed.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "Peds/Ped.h"
#include "PedGroup/PedGroup.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "Physics/RagdollConstraints.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "task/Combat/TaskDamageDeath.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "task/Combat/TaskWrithe.h"
#include "task/default/taskplayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskCrawl.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Combat/Subtasks/TaskAimFromGround.h"
#include "task/Physics/TaskNMHighFall.h"

#include "vehicles/vehicle.h"
#include "vehicles/Bike.h"
#include "Vfx/Misc/Fire.h"

#include "weapons/inventory/PedWeaponManager.h"
#include "weapons/Weapon.h"

#include "Task/Physics/TaskNMRelax.h"

#include "Network/General/NetworkStreamingRequestManager.h"

AI_OPTIMISATIONS()

using namespace AIStats;

//////////////////////////////////////////////////////////////////////////
CTaskGetUp::Tunables CTaskGetUp::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskGetUp, 0xd9477260)
//////////////////////////////////////////////////////////////////////////

bool CTaskGetUp::ms_bEnableBumpedReactionClips = false;
#if DEBUG_DRAW && __DEV
bool CTaskGetUp::ms_bShowSafePositionChecks = false;
#endif // DEBUG_DRAW && __DEV

#if __BANK
static bool sbForceBlockedGetup = false;
#endif

u32  CTaskGetUp::ms_LastAimFromGroundStartTime = 0;
u32  CTaskGetUp::ms_NumPedsAimingFromGround = 0;

dev_float sfRagdollBlendOutDelta = -4.0f;
dev_float sfRagdollTransRelaxation = 60.0f;
// Ok, the chosen position is no good, lets have a quick search around that position instead.
dev_float sfRagdollTransSearchAngle = 0.25f*PI;
dev_float sfRagdollTransSearchRadius = 0.2f;
dev_float sfRagdollTransSearchRadiusPlayer = 1.2f;
dev_float sfRagdollTransSearchRadiusPlayer2 = 2.0f;
dev_float sfRagdollOnCollisionfAllowedPenetration = 0.3f;

bank_float sfMinCrawlDistanceBeforeSafeCheck = 1.0f;

dev_float sfGroundCheckCapsuleRadius = 0.15f;

bank_float sfTestCapsuleRadius = 0.3f;
bank_float sfTestCapsuleBuffer = 0.1f;
bank_float sfCapsuleShortenDist = 0.0f;
#if DEBUG_DRAW
bank_u32 suDrawTestCapsuleTime = 0;
#endif

bool CTaskGetUp::sm_bCloneGetupFix = true;

//////////////////////////////////////////////////////////////////////////
// MoVE network signals
//////////////////////////////////////////////////////////////////////////

fwMvRequestId CTaskGetUp::ms_GetUpClipRequest("GetUpClip",0xB9AEAD6);
fwMvRequestId CTaskGetUp::ms_GetUpBlendRequest("GetUpBlend",0xBA5F330A);
fwMvRequestId CTaskGetUp::ms_ReactionClipRequest("ReactionClip",0x284D3AD);

fwMvBooleanId CTaskGetUp::ms_OnEnterGetUpClip("OnEnterGetUpClip",0xF44FE316);
fwMvBooleanId CTaskGetUp::ms_OnEnterGetUpBlend("OnEnterGetUpBlend",0x53D4BEB9);
fwMvBooleanId CTaskGetUp::ms_OnEnterReactionClip("OnEnterReactionClip",0x8E1C94FD);

fwMvBooleanId CTaskGetUp::ms_CanFireWeapon("CanFireWeapon",0xEB0F32C7);
fwMvBooleanId CTaskGetUp::ms_DropRifle("DropRifle",0x7828D107);

fwMvBooleanId CTaskGetUp::ms_ReactionClipFinished("ReactionClipFinished",0x5798415E);
fwMvBooleanId CTaskGetUp::ms_GetUpBlendFinished("GetUpBlendFinished",0xDD63713B);
fwMvBooleanId CTaskGetUp::ms_GetUpClipFinished("GetUpClipFinished",0x9197959E);

fwMvFloatId		CTaskGetUp::ms_BlendDuration("BlendDuration",0x5E36F77F);
fwMvRequestId	CTaskGetUp::ms_TagSyncBlend("TagSyncBlend",0x69FB1240);

fwMvFloatId		CTaskGetUp::ms_RagdollFrameBlendDuration("RagdollFrameDuration",0x2C215813);
fwMvFrameId	CTaskGetUp::ms_RagdollFrame("RagdollFrame",0x19C78D61);

fwMvClipSetVarId CTaskGetUp::ms_GetUpBlendSet("GetUpSweep",0x2260F08E);
fwMvFloatId	CTaskGetUp::ms_GetUpBlendDirection("GetUpBlendDirection",0xBD1ADA95);
fwMvFlagId CTaskGetUp::ms_No180Blend("No180Blend",0x5C99599D);
fwMvFlagId CTaskGetUp::ms_HasWeaponEquipped("HasWeaponEquipped",0x3ddc6bdf);
fwMvBooleanId CTaskGetUp::ms_OnEnterWeaponArms("OnEnterWeaponArms",0x78f696b4);

fwMvClipId	CTaskGetUp::ms_GetUpClip("GetUpPlaybackClip",0x2581328B);
fwMvFloatId CTaskGetUp::ms_GetUpPhase180("GetUp180Phase",0x1FB2E1AD);
fwMvFloatId CTaskGetUp::ms_GetUpPhase90("GetUp90Phase",0x6F3B5A4);
fwMvFloatId CTaskGetUp::ms_GetUpPhase0("GetUp0Phase",0x7B132374);
fwMvFloatId CTaskGetUp::ms_GetUpPhaseNeg90("GetUp-90Phase",0x726FD77C);
fwMvFloatId CTaskGetUp::ms_GetUpPhaseNeg180("GetUp-180Phase",0xA2F1249);
fwMvFloatId	CTaskGetUp::ms_GetUpRate("GetUpRate",0x467E0B25);
fwMvFloatId	CTaskGetUp::ms_GetUpClip180PhaseOut("GetupClip180PhaseOut",0xB954E98A);
fwMvFloatId	CTaskGetUp::ms_GetUpClip90PhaseOut("GetupClip90PhaseOut",0x20242624);
fwMvFloatId	CTaskGetUp::ms_GetUpClip0PhaseOut("GetupClip0PhaseOut",0x60B16DC0);
fwMvFloatId	CTaskGetUp::ms_GetUpClipNeg90PhaseOut("GetupClip-90PhaseOut",0x24B1381C);
fwMvFloatId	CTaskGetUp::ms_GetUpClipNeg180PhaseOut("GetupClip-180PhaseOut",0xB7DB6F6E);
fwMvBooleanId CTaskGetUp::ms_GetUpLooped("GetUpLooped",0x5F04FFF8);

fwMvClipId	CTaskGetUp::ms_ReactionClip("ReactionPlaybackClip",0xAA7A53C9);
fwMvFloatId CTaskGetUp::ms_ReactionClipPhase("ReactionClipPhase",0xD6FFE7C7);
fwMvFloatId CTaskGetUp::ms_ReactionClipPhaseOut("ReactionClipPhaseOut",0xB8DF09AF);

fwMvClipId	CTaskGetUp::ms_GetUpBlendClipOut("GetUpBlendClipOut",0x8D965D20);

fwMvClipId	CTaskGetUp::ms_GetUpSwimmingUnderwaterBackClip("recover_flip_back_to_front",0xD3760DE0);

fwMvFloatId CTaskGetUp::ms_DefaultArmsBlendInDuration("DefaultArmsBlendInDuration",0xAA6913E3);

#if __ASSERT
bool CTaskGetUp::ms_SpewRagdollTaskInfoOnGetUpSelection = false;
#endif //__ASSERT

//////////////////////////////////////////////////////////////////////////
// CClonedGetUpInfo
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedGetUpInfo::CClonedGetUpInfo(InitParams const& initParams)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_weaponTargetHelper(initParams.m_target)
{
	ClearBlendOutSetArray();

	SetStatusFromMainTaskState(initParams.m_state);	

	if(initParams.m_blendOutSetList)
	{
		BuildBlendOutSetArray(initParams.m_blendOutSetList);
		Assert(0 != m_numNmBlendOutSets);
	}

	m_forceTreatAsPlayer = initParams.m_forceTreatAsPlayer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedGetUpInfo::CClonedGetUpInfo() :
m_forceTreatAsPlayer(false)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	ClearBlendOutSetArray();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClonedGetUpInfo::BuildBlendOutSetArray(CNmBlendOutSetList const * blendOutSetList)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	ClearBlendOutSetArray();

	if(blendOutSetList)
	{
		m_numNmBlendOutSets = blendOutSetList->GetNumBlendOutSets();
		for(int s = 0; s < m_numNmBlendOutSets; ++s)
		{
			Assert(NMBS_INVALID != blendOutSetList->GetBlendOutSet(s));

			eNmBlendOutSet const& blendOutSet	= blendOutSetList->GetBlendOutSet(s);
			m_NmBlendOutSets[s]					= blendOutSet.GetHash();

			Assert(m_NmBlendOutSets[s] != 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void  CClonedGetUpInfo::ClearBlendOutSetArray(void)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_numNmBlendOutSets = 0;
	memset(m_NmBlendOutSets, 0, sizeof(int) * MAX_NUM_BLEND_OUT_SETS);	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClonedGetUpInfo::AutoCreateCloneTask(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// if a fall and get up clone task is running, it will create the getup clone task
	return !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_AND_GET_UP);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedGetUpInfo::CreateCloneFSMTask()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// clones always do a fast get up, so they don't lag behind the master ped too much
	CTaskGetUp* taskGetUp = rage_new CTaskGetUp();

	if(taskGetUp)
	{
		if(m_weaponTargetHelper.HasTargetEntity())
		{
			taskGetUp->SetTargetOutOfScopeID(m_weaponTargetHelper.GetTargetEntityId());
		}
	}

	return taskGetUp;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskGetUp::CTaskGetUp(BANK_ONLY(bool bForceEnableCollisionOnGetup))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Init();
#if __BANK
	m_bForceEnableCollisionOnGetup = bForceEnableCollisionOnGetup;
#endif
	SetInternalTaskType(CTaskTypes::TASK_GET_UP);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskGetUp::CTaskGetUp(eNmBlendOutSet forceBlendOutSet BANK_ONLY(, bool bForceEnableCollisionOnGetup))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Init();
	m_forcedSet = forceBlendOutSet;
#if __BANK
	m_bForceEnableCollisionOnGetup = bForceEnableCollisionOnGetup;
#endif
	SetInternalTaskType(CTaskTypes::TASK_GET_UP);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskGetUp::~CTaskGetUp()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::Init()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_bRunningAsMotionTask = false;
	m_bInstantBlendInNetwork = false;
	m_bInsertRagdollFrame = false;
	m_bBlendedInDefaultArms = false;

	m_clipSetId = CLIP_SET_ID_INVALID;
	m_clipId = CLIP_ID_INVALID;
	//m_pAttachToPhysical = NULL;
	//m_vecAttachOffset.Zero();

	m_pBumpedByEntity = NULL;
	m_iBumpedByComponent = -1;
	m_pExclusionEntity = NULL;
	m_bAllowBumpReaction = false;

	m_fBlendOutRootIkSlopeFixupPhase = -1.0f;

	m_fBlendDuration = SLOW_BLEND_DURATION;
	m_fGetupBlendReferenceHeading = 0.0f;
	m_fGetupBlendCurrentHeading = 0.0f;
	m_fGetupBlendInitialTargetHeading = 0.0f;
	m_bTagSyncBlend = false;
	m_bDroppingRifle = false;
	m_bDisableVehicleImpacts = false;
	m_bForceUpperBodyAim = false;
	m_bPressedToGoIntoCover = false;
	m_bDisableHighFallAbort = false;

#if __BANK
	m_bForceEnableCollisionOnGetup = false;
#endif

	m_forcedSet = NMBS_INVALID;
	m_matchedTime = 0.0f;
	m_matchedSet = NMBS_INVALID;
	m_pMatchedItem = NULL;

	m_NextEarlyFireTime = (u32)-1;

	m_pMoveTask = NULL;

	m_nStuckCounter = 0;
	m_nLastStandingTime = fwTimer::GetTimeInMilliseconds();
	pWritheClipSetRequestHelper = NULL;

	m_vCrawlStartPosition.Zero();

	m_TargetOutOfScopeId_Clone = NETWORK_INVALID_OBJECT_ID;
	m_bCloneTaskLocallyCreated = false;

	m_ProbeHelper = NULL;

	m_bLookingAtCarJacker = false;

	m_defaultArmsBlendDuration = 0.25f;

	m_bBranchTagsDirty = false;
	m_bSearchedFromClipPos = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::CleanUp()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed();

	// make sure we can't leave the ragdoll frame on when the task aborts
	if (pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame)
	{
		pPed->GetMovePed().SwitchToAnimated(0);
	}

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

			// If stationary, blend out ignore mover rotation, incase we want to walk/run start
			if(pPed->GetMotionData()->GetIsStill())
			{
				blendOutFlags |= CMovePed::Task_IgnoreMoverBlendRotation;
			}

			float fBlendDuration = m_fBlendDuration;
#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fMaxGetUpBlendOutDuration, 0.4f, 0.0f, 2.0f, 0.01f);
				fBlendDuration = Clamp(fBlendDuration, 0.0f, fMaxGetUpBlendOutDuration);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup, true);
			}
#endif // FPS_MODE_SUPPORTED


			pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, fBlendDuration, blendOutFlags);
		}
	}
	
	// Ensure that the slope fixup IK solver is cleaned up!
	CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pSolver != NULL)
	{
		pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
	}

	if (pPed->GetAnimatedInst() != NULL && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
	{
		CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex(), CPhysics::GetLevel()->GetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex()) & (~ArchetypeFlags::GTA_DEEP_SURFACE_TYPE));
	}

	m_matchedSet.SetHash(0);

	// if we're a clone, free the get up animations we've loaded up....
	if(pPed->IsNetworkClone())
	{
		u32 numSets = m_pointCloudPoseSets.GetNumBlendOutSets();
		for(u32 i = 0; i < numSets; ++i)
		{
			m_cloneSetRequestHelpers[i].Release();
		}
	}

	// Reset any bound sizing/orientation/offset
	pPed->ClearBound();
	pPed->SetBoundPitch(0.0f);
	pPed->SetBoundHeading(0.0f);
	pPed->SetBoundOffset(VEC3_ZERO);
	pPed->SetTaskCapsuleRadius(0.0f);

	if (pPed->GetPedIntelligence())
	{
		pPed->GetPedIntelligence()->SetLastGetUpTime();
		// Grab the locomotion task
		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		if( pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION )
		{
			static_cast<CTaskHumanLocomotion*>(pTask)->SetCheckForAmbientIdle();
		}
	}

	m_pointCloudPoseSets.Reset();

	if (pWritheClipSetRequestHelper)
		CWritheClipSetRequestHelperPool::ReleaseClipSetRequestHelper(pWritheClipSetRequestHelper);

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PointGunLeftHandSupporting, false);
	pPed->GetIkManager().ClearFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);

	if (m_ProbeHelper != NULL)
	{
		delete m_ProbeHelper;
		m_ProbeHelper = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(iPriority == ABORT_PRIORITY_IMMEDIATE)
	{
		animTaskDebugf(this, "TaskGetUp Aborting for ABORT_PRIORITY_IMMEDIATE, event '%s'", pEvent ? pEvent->GetName() : "none");
		return CTask::ShouldAbort(iPriority, pEvent);
	}
	
	if(ShouldAbortExternal(pEvent))
	{
		animTaskDebugf(this, "TaskGetUp Aborting for event '%s'", pEvent ? pEvent->GetName() : "none");
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}

bool CTaskGetUp::ShouldAbortExternal(const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	bool bAllowAbort = false;

	// always abort here if the event requires abort for ragdoll
	if (pEvent && (((const CEvent*)pEvent)->RequiresAbortForRagdoll() || ((const CEvent*)pEvent)->RequiresAbortForMelee(pPed)))
	{
		bAllowAbort = true;
	}

	int nEventType = pEvent ? ((CEvent*)pEvent)->GetEventType() : -1;
	if( nEventType==EVENT_NEW_TASK || ( nEventType==EVENT_DAMAGE && GetState()==State_PlayReactionClip ) )
		bAllowAbort = true;

	if( !bAllowAbort )
	{
		const aiTask* pTaskResponse = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();
		if(pTaskResponse==NULL)
			pTaskResponse = pPed->GetPedIntelligence()->GetEventResponseTask();

		if(pTaskResponse)                                                                                                                                                                                 
		{
			if(CTaskNMBehaviour::DoesResponseHandleRagdoll(pPed, pTaskResponse))
				bAllowAbort = true;
		}
	}

	// Don't allow on-fire events to abort a getup that is between 20% and 80% complete as blending could be messy if there are
	// no ragdolls available
	if( bAllowAbort && nEventType==EVENT_ON_FIRE && ( GetPhase() > 0.2f && GetPhase() < 0.8f ) )
	{
		bAllowAbort = false;
	}

	return bAllowAbort;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTask::DoAbort(iPriority, pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CTaskGetUp::GetPhase() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//Check the state.
	switch(GetState())
	{
		case State_PlayingGetUpClip:
		{
			return m_networkHelper.GetFloat(ms_GetUpClip0PhaseOut);
		}
		default:
		{
			break;
		}
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo *CTaskGetUp::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CClonedGetUpInfo::InitParams initParams
	(
		GetState(), 
		&m_Target,
		0 < m_pointCloudPoseSets.GetNumBlendOutSets() ? &m_pointCloudPoseSets : NULL,
		GetPed() ? GetTreatAsPlayer(*GetPed()) : false
	);
	
	return rage_new CClonedGetUpInfo(initParams);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Assert( pTaskInfo && pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_GET_UP );
    CClonedGetUpInfo *getupInfo = static_cast<CClonedGetUpInfo*>(pTaskInfo);

	//---

	getupInfo->GetWeaponTargetHelper().ReadQueriableState(m_Target);
	if(getupInfo->GetWeaponTargetHelper().HasTargetEntity())
	{
		SetTargetOutOfScopeID(getupInfo->GetWeaponTargetHelper().GetTargetEntityId());
	}

	//---

	m_pointCloudPoseSets.Reset();
	s32 numBlendOutSets = getupInfo->GetNumNmBlendOutSets();
	for(s32 b = 0; b < numBlendOutSets; ++b)
	{
		Assert(NMBS_INVALID != (u32)getupInfo->GetNmBlendOutSet(b));

		eNmBlendOutSet getupSet = eNmBlendOutSet(getupInfo->GetNmBlendOutSet(b));

		// convert local getup set to clone one
		if (getupSet == NMBS_STANDARD_GETUPS)
		{
			getupSet = NMBS_STANDARD_CLONE_GETUPS;
		}
		else if (getupSet == NMBS_STANDARD_GETUPS_FEMALE)
		{
			getupSet = NMBS_STANDARD_CLONE_GETUPS_FEMALE;
		}
		else if (getupSet == NMBS_PLAYER_MP_GETUPS)
		{
			getupSet = NMBS_PLAYER_MP_CLONE_GETUPS;
		}
		else if (getupSet == NMBS_FAST_GETUPS)
		{
			getupSet = NMBS_FAST_CLONE_GETUPS;
		}
		else if (getupSet == NMBS_FAST_GETUPS_FEMALE)
		{
			getupSet = NMBS_FAST_CLONE_GETUPS_FEMALE;
		}
		else if (getupSet == NMBS_ACTION_MODE_GETUPS)
		{
			getupSet = NMBS_ACTION_MODE_CLONE_GETUPS;
		}
		else if (getupSet == NMBS_PLAYER_MP_ACTION_MODE_GETUPS)
		{
			getupSet = NMBS_PLAYER_MP_ACTION_MODE_CLONE_GETUPS;
		}
		else if (getupSet == NMBS_INJURED_PLAYER_MP_GETUPS)
		{
			getupSet = NMBS_INJURED_PLAYER_MP_CLONE_GETUPS;
		}

		m_pointCloudPoseSets.Add(getupSet);
	}

	//---

	m_bForceTreatAsPlayer = getupInfo->GetForceTreatAsPlayer();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

bool CTaskGetUp::OverridesNetworkBlender(CPed* pPed)
{
	if(pPed)
	{
		CTaskParachute* taskParachute = static_cast<CTaskParachute*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
		if(taskParachute && taskParachute->GetState() == CTaskParachute::State_CrashLand)
		{
			return false;
		}
	}

	return true; 
}

bool CTaskGetUp::OverridesNetworkHeadingBlender(CPed* pPed)
{
	if(pPed)
	{
		CTaskParachute* taskParachute = static_cast<CTaskParachute*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
		if(taskParachute)
		{
			return (taskParachute->GetState() == CTaskParachute::State_CrashLand);
		}
	}	

	return true;
}

bool CTaskGetUp::IsInScope(const CPed* pPed)
{
	// We must keep the getup task running if it is a subtask of dying dead, otherwise the ped can end up posed standing
	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_DYING_DEAD)
	{
		return true;
	}

	// Players' don't send over target data....
	if(!pPed->IsPlayer())
	{
		// Update the target...
		ObjectId targetId = GetTargetOutOfScopeID();
		CNetObjPed::UpdateNonPlayerCloneTaskTarget(pPed, m_Target, targetId);
		SetTargetOutOfScopeID(targetId);
	}
	else
	{
		return CTaskFSMClone::IsInScope(pPed);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::IsTaskRunningOverNetwork_Clone(CTaskTypes::eTaskType const taskType) const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed const* pPed = GetPed();

	if(taskVerifyf(pPed && pPed->GetPedIntelligence(), "Invalid Ped Intelligence?!"))
	{
		CQueriableInterface const* qi = pPed->GetPedIntelligence()->GetQueriableInterface();

		if(taskVerifyf(qi, "Invalid Ped Queriable Interface?!"))
		{
			CTaskInfo* taskInfo = qi->GetTaskInfoForTaskType(taskType, PED_TASK_PRIORITY_MAX, false);

			if(taskInfo)
			{
				return true;
			}
		}
	}

	return false;
}

void CTaskGetUp::TaskSetState(u32 const iState)
{
	aiTask::SetState(iState);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskGetUp::FSM_Return CTaskGetUp::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// and updating...
	if(iEvent == OnUpdate)
	{
		CPed* pPed = GetPed();

		// if the task is actually running over the network (sometimes it be set off locally with 
		// no corresponding task from the network so we can't get the data we need so we have to 
		// run it locally and generate the data ourselves.
		if(IsTaskRunningOverNetwork_Clone(CTaskTypes::TASK_GET_UP) && !m_bCloneTaskLocallyCreated)
		{
			u32 stateFromNetwork = GetStateFromNetwork();

			// if we're in start state
			if(iState == State_Start)
			{
				Vector3 vecHeadPos(0.0f, 0.0f, 0.0f);
				pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
				// if the ped is already animated and standing, just quit
				if (!pPed->GetUsingRagdoll() && vecHeadPos.z >= pPed->GetTransform().GetPosition().GetZf())
				{
#if __DEV
					taskDebugf2("Frame %d : %s : FSM_Quit: m_bCloneTaskLocallyCreated %s, ped %s [0x%p] : State %d,  vecHeadPos.z %.2f,  pPed->GetTransform().GetPosition().GetZf() %.2f.", 
						fwTimer::GetFrameCount(),
						__FUNCTION__,
						m_bCloneTaskLocallyCreated?"TRUE":"FALSE",
						pPed->GetDebugName(),
						pPed,
						GetState(),
						vecHeadPos.z,
						pPed->GetTransform().GetPosition().GetZf() );
#endif
					return FSM_Quit;
				}
				else if(stateFromNetwork == State_Start)
				{
					// need to keep the current pose whilst we wait for a valid clip matrix
					if (pPed->GetMovePed().GetState()==CMovePed::kStateAnimated)
					{
						pPed->SwitchToStaticFrame(INSTANT_BLEND_DURATION);
					}

					// don't move on until the owner has moved on and we know which nm blend out sets have been chosen....
					return FSM_Continue;
				}
				// if the owner has chosen what it wants to do / which anims to use or the task isn't running on the network...
				else if((m_pointCloudPoseSets.GetNumBlendOutSets() > 0) && (GetStateFromNetwork() > State_ChooseGetupSet))
				{
					// we need to stream the get up anims as they haven' been streamed in by the ::RequestGetupAssets calls made by the NM task...
					TaskSetState(State_StreamBlendOutClip_Clone);
					return FSM_Continue;
				}

				// need to keep the current pose whilst we wait for a valid clip matrix
				if (pPed->GetMovePed().GetState()==CMovePed::kStateAnimated)
				{
					pPed->SwitchToStaticFrame(INSTANT_BLEND_DURATION);
				}
			}
			// else we've finished streaming and we're waiting for the owner to move on...
			else if(iState == State_StreamBlendOutClipComplete_Clone)
			{
				Assert(m_pointCloudPoseSets.GetNumBlendOutSets() > 0);
				Assert(GetStateFromNetwork() > State_ChooseGetupSet);

				// lets use it...
				TaskSetState(State_ChooseGetupSet);
				return FSM_Continue;
			}
		}
		else
		{
			if(iState == State_Start)
			{
				// no network counterpart, just move straight onto the waiting state....
				TaskSetState(State_ChooseGetupSet);
				return FSM_Continue;
			}
			// sometimes the owner can finish before the clone has completed streaming so it gets stuck on the streaming complete state so if so, we nudge it along.
			else if(iState == State_StreamBlendOutClipComplete_Clone)
			{
				// We should still have anim data by now if we've got this far....
				Assert(m_pointCloudPoseSets.GetNumBlendOutSets() > 0);

				// move onto the right state (at this point we've either got anim data or we don't)...
				TaskSetState(State_ChooseGetupSet);
				return FSM_Continue;
			}
		}
	}

	//use the standard state machine
	return UpdateFSM(iState, iEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask* CTaskGetUp::FindGetupControllingTask(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTask* pTask = NULL;

	if (pPed)
	{
		pTask = FindGetupControllingChild(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP));

		if (!pTask)
		{
			pTask = FindGetupControllingChild(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP));
		}
		if (!pTask)
		{
			pTask = FindGetupControllingChild(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY));
		}
		if (!pTask)
		{
			pTask = FindGetupControllingChild(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_DEFAULT));
		}
	}

	return pTask;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask* CTaskGetUp::FindGetupControllingChild(CTask* pTask)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	while (pTask)
	{
		if (pTask->UseCustomGetup())
			return pTask;

		pTask = pTask->GetSubTask();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ProcessUpperBodyAim(CPed* pPed, const crClip* pClip, float fPhase)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
	{
		if (!m_UpperBodyAimPitchHelper.IsBlendedIn())
		{
			bool bBlendInAim = m_bForceUpperBodyAim;
			float fBlendInDuration = m_fBlendDuration;

			if (!bBlendInAim)
			{
				float fStartPhase = 0.0f;
				float fEndPhase = 1.0f;
				if (CClipEventTags::FindBlendInUpperBodyAimStartEndPhases(pClip, fStartPhase, fEndPhase) && fPhase >= fStartPhase && taskVerifyf(fEndPhase >= fStartPhase, "Start blend in phase was greater than end blend in phase"))		
				{
					bBlendInAim = true;
					fBlendInDuration = pClip->ConvertPhaseToTime(fEndPhase - fStartPhase);
				}
			}

			if (bBlendInAim)
			{
				m_UpperBodyAimPitchHelper.BlendInUpperBodyAim(m_networkHelper, *pPed, fBlendInDuration);
			}
		}
		else
		{
			m_UpperBodyAimPitchHelper.Update(*pPed);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ProcessEarlyFiring(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	// Is the anim ready to early fire? TODO - get this from the move network / clips
	bool bAnimReady = m_networkHelper.IsNetworkActive() && m_networkHelper.GetBoolean(ms_CanFireWeapon);

	if (!bAnimReady)
	{
		m_NextEarlyFireTime = (u32)-1;
		return;
	}

	// Is the ped armed
	if (!pPed->GetWeaponManager() || !pPed->GetWeaponManager()->GetEquippedWeapon() || !pPed->GetWeaponManager()->GetEquippedWeaponObject())
		return;

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapon->GetWeaponHash());

	//  Is the weapon ready to fire
	if (!pWeaponInfo || !pWeapon->GetWeaponInfo()->GetIsGun() || pWeapon->GetState() != CWeapon::STATE_READY)
		return;

	if (m_NextEarlyFireTime==(u32)-1)
	{
		m_NextEarlyFireTime=fwTimer::GetTimeInMilliseconds()+static_cast<u32>(1000.0f * fwRandom::GetRandomNumberInRange(0.0f, pWeaponInfo->GetTimeBetweenShots()));
	}

	if (m_NextEarlyFireTime > fwTimer::GetTimeInMilliseconds())
		return;

	// Is the target valid?
	if (!m_Target.GetIsValid())
		return;

	// Is the target in range?
	Vector3 targetPosition(VEC3_ZERO);
	m_Target.GetPosition(targetPosition);
	Mat34V weaponMatrix = pWeaponObject->GetMatrix();

	Vector3 muzzlePosition(VEC3_ZERO); 
	pWeapon->GetMuzzlePosition(RC_MATRIX34(weaponMatrix), muzzlePosition);

	if (pWeapon->GetRange()*pWeapon->GetRange() < (targetPosition-muzzlePosition).Mag2())
		return;

	// Is the weapon pointing at the target?
	Vector3 vecToTarget(targetPosition);
	vecToTarget-=muzzlePosition;
	vecToTarget.Normalize();

	Vector3 vecStart(VEC3_ZERO);
	Vector3 vecEnd(VEC3_ZERO);

	pWeapon->CalcFireVecFromGunOrientation(pPed, RC_MATRIX34(weaponMatrix), vecStart, vecEnd);
	Vector3 vecMuzzle(vecEnd);
	vecMuzzle-=vecStart;
	vecMuzzle.Normalize();

	if (vecMuzzle.Dot(vecToTarget) < 0.9f)
		return;

	// Is the ped looking in the general direction of the target?

	Matrix34 headMatrix(M34_IDENTITY);
	pPed->GetBoneMatrix(headMatrix, BONETAG_HEAD);
	vecToTarget=targetPosition;
	vecToTarget-=headMatrix.d;
	vecToTarget.Normalize();

	if (headMatrix.b.Dot(vecToTarget) < 0.2f)
		return;

	CWeapon::sFireParams fireParams(pPed, RC_MATRIX34(weaponMatrix));

	pWeapon->Fire(fireParams);
	m_NextEarlyFireTime=fwTimer::GetTimeInMilliseconds()+static_cast<u32>(1000.0f * pWeaponInfo->GetTimeBetweenShots());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ProcessCoverInterrupt(CPed& rPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (rPed.IsLocalPlayer() && !m_bPressedToGoIntoCover && GetTimeRunning() > sm_Tunables.m_fMinTimeInGetUpToAllowCover)
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl && pControl->GetPedCover().IsPressed())
		{
			m_bPressedToGoIntoCover = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ProcessDropDown(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsLocalPlayer())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDropDown, true);

		CDropDownDetector::CDropDownDetectorParams dropDownDetectorParams;

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownDepthRadius, 0.5f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownMinHeight, 0.7f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fHeightToDropDownCutoff, 0.5f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownHeight, 0.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownStartZOffset, 1.0f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownStartForwardOffset, 0.35f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownSlopeDotMax, 0.4f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownSlopeDotMinDirectionCutoff, -0.65f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownPointMinZDot, 0.4f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fTestHeightDifference, 0.25f, 0.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSlantedSlopeDot, 0.875f, 0.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownEdgeTestRadius, 0.2f, 0.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownBestDistanceDotMax, 0.95f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownBestDistanceDotMin, 0.65f, 0.0f, 10.0f, 0.1f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownRunMaxDistance, 2.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownRunMinDistance, 0.75f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownRunBestDistanceMax, 0.25f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownRunBestDistanceMin, 0.125f, 0.0f, 10.0f, 0.1f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownWalkMaxDistance, 0.75f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownWalkMinDistance, 0.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownWalkBestDistanceMax, 0.15f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownWalkBestDistanceMin, 0.05f, 0.0f, 10.0f, 0.1f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownSlopeTolerance, 0.3f, 0.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownClearanceTestZOffset, 0.35f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownClearanceTestRadius, 0.25f, 0.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fDropDownEdgeThresholdDot, 0.2f, 0.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fAutoJumpMaxDistance, 2.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fAutoJumpMinDistance, 0.4f, 0.0f, 10.0f, 0.1f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownWalkMaxDistance, 0.75f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownWalkMinDistance, 0.4f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownWalkBestDistanceMax, 0.2f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownWalkBestDistanceMin, 0.1f, 0.0f, 10.0f, 0.1f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownRunMaxDistance, 0.75f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownRunMinDistance, 0.4f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownRunBestDistanceMax, 0.2f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fSmallDropDownRunBestDistanceMin, 0.1f, 0.0f, 10.0f, 0.1f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fMidpointTestRadius, 0.1f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fMidpointTestCutoff, 0.33f, 0.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fBiasRightSideHeading, -0.1f, -1.0f, 1.0f, 0.01f);

		TUNE_GROUP_FLOAT(GETUP_DROPDOWN, fGapDetectionDistance, 8.0f, 0.0f, 20.0f, 0.01f);

		dropDownDetectorParams.m_fDropDownDepthRadius = fDropDownDepthRadius;
		dropDownDetectorParams.m_fDropDownMinHeight = fDropDownMinHeight;
		dropDownDetectorParams.m_fHeightToDropDownCutoff = fHeightToDropDownCutoff;
		dropDownDetectorParams.m_fSmallDropDownHeight = fSmallDropDownHeight;
		dropDownDetectorParams.m_fDropDownStartZOffset = fDropDownStartZOffset;
		dropDownDetectorParams.m_fDropDownStartForwardOffset = fDropDownStartForwardOffset;
		dropDownDetectorParams.m_fDropDownSlopeDotMax = fDropDownSlopeDotMax;
		dropDownDetectorParams.m_fDropDownSlopeDotMinDirectionCutoff = fDropDownSlopeDotMinDirectionCutoff;
		dropDownDetectorParams.m_fDropDownPointMinZDot = fDropDownPointMinZDot;
		dropDownDetectorParams.m_fTestHeightDifference = fTestHeightDifference;
		dropDownDetectorParams.m_fSlantedSlopeDot = fSlantedSlopeDot;
		dropDownDetectorParams.m_bPerformControlTest = false;

		dropDownDetectorParams.m_fDropDownEdgeTestRadius = fDropDownEdgeTestRadius;

		dropDownDetectorParams.m_fDropDownBestDistanceDotMax = fDropDownBestDistanceDotMax;
		dropDownDetectorParams.m_fDropDownBestDistanceDotMin = fDropDownBestDistanceDotMin;

		dropDownDetectorParams.m_fDropDownRunMaxDistance = fDropDownRunMaxDistance;
		dropDownDetectorParams.m_fDropDownRunMinDistance = fDropDownRunMinDistance;
		dropDownDetectorParams.m_fDropDownRunBestDistanceMax = fDropDownRunBestDistanceMax;
		dropDownDetectorParams.m_fDropDownRunBestDistanceMin = fDropDownRunBestDistanceMin;

		dropDownDetectorParams.m_fDropDownWalkMaxDistance = fDropDownWalkMaxDistance;
		dropDownDetectorParams.m_fDropDownWalkMinDistance = fDropDownWalkMinDistance;
		dropDownDetectorParams.m_fDropDownWalkBestDistanceMax = fDropDownWalkBestDistanceMax;
		dropDownDetectorParams.m_fDropDownWalkBestDistanceMin = fDropDownWalkBestDistanceMin;

		dropDownDetectorParams.m_fDropDownSlopeTolerance = fDropDownSlopeTolerance;

		dropDownDetectorParams.m_fDropDownClearanceTestZOffset = fDropDownClearanceTestZOffset;
		dropDownDetectorParams.m_fDropDownClearanceTestRadius = fDropDownClearanceTestRadius;

		dropDownDetectorParams.m_fDropDownEdgeThresholdDot = fDropDownEdgeThresholdDot;

		dropDownDetectorParams.m_fAutoJumpMaxDistance = fAutoJumpMaxDistance;
		dropDownDetectorParams.m_fAutoJumpMinDistance = fAutoJumpMinDistance;

		dropDownDetectorParams.m_fSmallDropDownWalkMaxDistance = fSmallDropDownWalkMaxDistance;
		dropDownDetectorParams.m_fSmallDropDownWalkMinDistance = fSmallDropDownWalkMinDistance;
		dropDownDetectorParams.m_fSmallDropDownWalkBestDistanceMax = fSmallDropDownWalkBestDistanceMax;
		dropDownDetectorParams.m_fSmallDropDownWalkBestDistanceMin = fSmallDropDownWalkBestDistanceMin;

		dropDownDetectorParams.m_fSmallDropDownRunMaxDistance = fSmallDropDownRunMaxDistance;
		dropDownDetectorParams.m_fSmallDropDownRunMinDistance = fSmallDropDownRunMinDistance;
		dropDownDetectorParams.m_fSmallDropDownRunBestDistanceMax = fSmallDropDownRunBestDistanceMax;
		dropDownDetectorParams.m_fSmallDropDownRunBestDistanceMin = fSmallDropDownRunBestDistanceMin;

		dropDownDetectorParams.m_fMidpointTestRadius = fMidpointTestRadius;
		dropDownDetectorParams.m_fMidpointTestCutoff = fMidpointTestCutoff;

		dropDownDetectorParams.m_fBiasRightSideHeading = fBiasRightSideHeading;

		dropDownDetectorParams.m_fGapDetectionDistance = fGapDetectionDistance;

		Matrix34 xTransform = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		pPed->GetPedIntelligence()->GetDropDownDetector().Process(&xTransform, &dropDownDetectorParams);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::IsAllowedWhenDead(eNmBlendOutSet blendOutSet)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CNmBlendOutSet* pSet = CNmBlendOutSetManager::GetBlendOutSet(blendOutSet);
	if (pSet)
	{
		return pSet->IsFlagSet(CNmBlendOutSet::BOSF_AllowWhenDead);
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::ProcessPreFSM()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Very rarely the clone ped will get hit again after TaskGetUp (via parent TaskFallAndGetUp) has started so the 
	// physics will change the ped back to ragdoll. This wipes the TaskNetwork from the ped's motion tree so the ped 
	// will be locked in TaskGetUp forever as it's MoveNetworkHelper will never get into the target state it's looking for. 
	// If we detect that we've changed to ragdoll (after TaskGetUp has already changed us to animated in State_ChooseGetUpSet
	// - we can start TaskGetUp in ragdoll and the task changes us to animated using ActivateAnimatedInstance( ), just quit the task.
	// (The owner ped's tasks are automatically wiped using events which the clone doesn't respond to)
	if(pPed->IsNetworkClone())
	{
		if(GetState() > State_ChooseGetupSet)
		{
			if(pPed->GetUsingRagdoll())
			{
				return FSM_Quit;
			}
		}
	}

	if(GetState() > State_ChooseGetupSet)
	{
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding))
		{
			m_nLastStandingTime = fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			if (GetState() != State_DropDown
				&& !m_bDisableHighFallAbort
				&& (pPed->IsPlayer() || sm_Tunables.m_AllowNonPlayerHighFallAbort)
				&& !pPed->IsNetworkClone()
				&& (pPed->GetIsVisibleInSomeViewportThisFrame() || sm_Tunables.m_AllowOffScreenHighFallAbort) 
				&& (fwTimer::GetTimeInMilliseconds() - m_nLastStandingTime)>sm_Tunables.m_FallTimeBeforeHighFallAbort 
				&& CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL)
				)
			{
				const Vector3& vel = pPed->GetVelocity();
				const float heightAboveGround = pPed->GetGroundZFromImpact() == PED_GROUNDPOS_RESET_Z ? -PED_GROUNDPOS_RESET_Z : pPed->GetTransform().GetPosition().GetZf() - pPed->GetGroundZFromImpact() - pPed->GetCapsuleInfo()->GetGroundToRootOffset();
				if (vel.z<sm_Tunables.m_MinFallSpeedForHighFallAbort && heightAboveGround>sm_Tunables.m_MinHeightAboveGroundForHighFallAbort)
				{
					// check if the matched blend out set allows falling
					CNmBlendOutSet* pMatchedSet = CNmBlendOutSetManager::GetBlendOutSet(m_matchedSet);

					if (pMatchedSet && !pMatchedSet->IsFlagSet(CNmBlendOutSet::BOSF_DontAbortOnFall))
					{
						CTaskNMHighFall* pNewTask = rage_new CTaskNMHighFall(100, NULL, CTaskNMHighFall::HIGHFALL_IN_AIR);
						CTaskNMControl* pNMControlTask = static_cast<CTaskNMControl*>(FindParentTaskOfType(CTaskTypes::TASK_NM_CONTROL));
						if (pNMControlTask != NULL)
						{
							// set the next task on nm control
							pNMControlTask->ForceNewSubTask(pNewTask);
							pPed->SwitchToRagdoll(*this);
							return FSM_Quit;
						}
						else
						{
							CEventSwitch2NM event(10000, pNewTask);
							pPed->SwitchToRagdoll(event);
							return FSM_Quit;
						}
					}
				}				

				
			}
		}
	}


	pPed->SetPedResetFlag( CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false );

	// Allow Leg IK if enabled in anim tags.
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);

	// Force disable leg/look ik.
	if (GetState()!= State_PlayReactionClip)
	{
		if (!m_bLookingAtCarJacker && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL))
		{
			pPed->SetCodeHeadIkBlocked();
		}

		if ( !m_bRunningAsMotionTask && !pPed->GetIsSwimming() && !pPed->GetIsParachuting())
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceMotionStateLeaveDesiredMBR, true);
		}
	}

	if (m_pMoveTask || pPed->IsLocalPlayer())
	{
		CTask* pTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
		if (pTask && (pTask==m_pMoveTask || pTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER))
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
	
	ProcessEarlyFiring(pPed);

	// If we got into this task, we consider this ped as having done an NM getup.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasPlayedNMGetup, true);

	if(PlayerWantsToAimOrFire(pPed))
	{
		if(!pPed->GetUsingRagdoll() && !pPed->GetPlayerInfo()->AreControlsDisabled())
		{
			const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();

			const bool bIsAssistedAiming = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

			// Update the ped's (desired) heading
			CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());
			Vector3 vTargetPos;
			if (bIsAssistedAiming)
			{
				vTargetPos = targeting.GetLockonTargetPos();
				pPed->SetDesiredHeading(vTargetPos);
			}
			else
			{
				float aimHeading = aimCameraFrame.ComputeHeading();
				pPed->SetDesiredHeading(aimHeading);
			}
		}
	}

	// set the weapon clip set
	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	if (m_networkHelper.IsNetworkActive() && pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
	{
		CTaskMotionPed* pPedTask = static_cast<CTaskMotionPed*>(pTask);

		const CWeaponInfo* pWeaponInfo = NULL;
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon))
		{
			// If swapping weapon, use the weapon we are swapping to
			pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		}
		else
		{
			pWeaponInfo = pPed->GetWeaponManager() ? CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash()) : NULL;
		}

		static fwMvFilterId s_filter("BothArms_filter",0x16F1D420);
		static fwMvFilterId s_Gripfilter("Grip_R_Only_filter",0xB455BA3A);
		fwMvFilterId filterId = s_filter;
		fwMvClipSetId clipsetId = CLIP_SET_ID_INVALID;

		if(pPedTask->GetOverrideWeaponClipSet() != CLIP_SET_ID_INVALID)
		{
			if(pPedTask->GetOverrideWeaponClipSetFilter() != FILTER_ID_INVALID)
			{
				filterId = pPedTask->GetOverrideWeaponClipSetFilter();
			}
			clipsetId = pPedTask->GetOverrideWeaponClipSet();
		}
		else
		{
			if(pWeaponInfo)
			{
				clipsetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);
				filterId = pWeaponInfo->GetPedMotionFilterId(*pPed);
			}
		}

		if (clipsetId != CLIP_SET_ID_INVALID && fwClipSetManager::IsStreamedIn_DEPRECATED(clipsetId))
		{
			crFrameFilter* pWeaponFilter = CGtaAnimManager::FindFrameFilter(filterId.GetHash(), GetPed());

			m_networkHelper.SetFlag(clipsetId!=CLIP_SET_ID_INVALID, ms_HasWeaponEquipped);
			m_networkHelper.SetClipSet(clipsetId, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);
			m_networkHelper.SetFilter(pWeaponFilter, CTaskHumanLocomotion::ms_WeaponHoldingFilterId);
			m_networkHelper.SetFloat(ms_DefaultArmsBlendInDuration, m_defaultArmsBlendDuration);
		}
	}

	if (m_networkHelper.IsNetworkActive() && m_networkHelper.GetBoolean(ms_OnEnterWeaponArms))
	{
		m_bBlendedInDefaultArms = true;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::UpdateFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate(pPed);

	FSM_State(State_ChooseGetupSet)
		FSM_OnEnter
		ChooseGetupSet_OnEnter(pPed);
	FSM_OnUpdate
		return ChooseGetupSet_OnUpdate(pPed);
	FSM_OnExit
		ChooseGetupSet_OnExit(pPed);

	FSM_State(State_PlayingGetUpClip)
		FSM_OnEnter
		PlayingGetUpClip_OnEnter(pPed);
	FSM_OnUpdate
		return PlayingGetUpClip_OnUpdate(pPed);
	FSM_OnExit
		PlayingGetUpClip_OnExit(pPed);

	FSM_State(State_PlayReactionClip)
		FSM_OnEnter
		PlayingReactionClip_OnEnter(pPed);
	FSM_OnUpdate
		return PlayingReactionClip_OnUpdate(pPed);

	FSM_State(State_PlayingGetUpBlend)
		FSM_OnEnter
		PlayingGetUpBlend_OnEnter(pPed);
	FSM_OnUpdate
		return PlayingGetUpBlend_OnUpdate(pPed);

	FSM_State(State_AimingFromGround)
		FSM_OnEnter
		AimingFromGround_OnEnter(pPed);
	FSM_OnUpdate
		return AimingFromGround_OnUpdate(pPed);
	FSM_OnExit
		AimingFromGround_OnExit();

	FSM_State(State_ForceMotionState)
		FSM_OnEnter
		ForceMotionState_OnEnter(pPed);
	FSM_OnUpdate
		return ForceMotionState_OnUpdate(pPed);

	FSM_State(State_SetBlendDuration)
		FSM_OnEnter
		SetBlendDuration_OnEnter(pPed);
	FSM_OnUpdate
		return SetBlendDuration_OnUpdate(pPed);

	FSM_State(State_DirectBlendOut)
		FSM_OnEnter
		DirectBlendOut_OnEnter(pPed);
	FSM_OnUpdate
		return DirectBlendOut_OnUpdate(pPed);

	FSM_State(State_StreamBlendOutClip_Clone)
		FSM_OnUpdate
		return StreamBlendOutClip_Clone_OnUpdate(pPed);

	FSM_State(State_Crawl)
		FSM_OnEnter
			Crawl_OnEnter(pPed);
		FSM_OnUpdate
			return Crawl_OnUpdate(pPed);
		FSM_OnExit
			Crawl_OnExit(pPed);

	FSM_State(State_DropDown)
		FSM_OnEnter
			DropDown_OnEnter(pPed);
		FSM_OnUpdate
			return DropDown_OnUpdate(pPed);
		FSM_OnExit
			DropDown_OnExit(pPed);

	FSM_State(State_Finish)
		FSM_OnUpdate
		return Finish_OnUpdate();

	FSM_State(State_StreamBlendOutClipComplete_Clone)

	FSM_End
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::Start_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(pPed && !pPed->IsNetworkClone())
	{
		TaskSetState(State_ChooseGetupSet);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ChooseGetupSet_OnEnter(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_nStartClipCounter = sm_Tunables.m_StartClipWaitTime;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ProcessBranchTags(atHashString& nextItemId)
//////////////////////////////////////////////////////////////////////////
{
	static atHashString s_branchAlways("Always",0x2752BB0F);
	static atHashString s_branchIdle("Idle",0x71C21326);
	static atHashString s_branchIdleTurn("IdleTurn",0xFA5D76E5);
	static atHashString s_branchWalk("Walk",0x83504C9C);
	static atHashString s_branchRun("Run",0x1109B569);
	static atHashString s_branchAim("Aim",0xB01E2F36);

	CPed* pPed = GetPed();

	// NOTE - we never detect branch tags on the first frame (as the tags are from the previous state in the move network)
	if (pPed && m_networkHelper.GetNetworkPlayer() && !m_bBranchTagsDirty)
	{
		// grab the desired move blend ratio
		float desiredMbrX = abs(pPed->GetMotionData()->GetDesiredMbrX());
		float desiredMbrY = abs(pPed->GetMotionData()->GetDesiredMbrY());
		bool bWantsToMove = desiredMbrY>0.001f || desiredMbrX>0.001f;
		bool bWantsToTurn = !AreNearlyEqual(pPed->GetCurrentHeading(), pPed->GetDesiredHeading(), 0.001f);
		bool bPlayerWantsToAimOrFire = PlayerWantsToAimOrFire(pPed);
		
		//search for a branch tag from the move network
		mvParameterBuffer::Iterator it;
		for(mvKeyedParameter* kp = it.Begin(m_networkHelper.GetNetworkPlayer()->GetOutputBuffer(), mvParameter::kParameterProperty, CClipEventTags::BranchGetup); kp != NULL; kp = it.Next())
		{
			const CClipEventTags::CBranchGetupTag* pBranchTag = static_cast<const CClipEventTags::CBranchGetupTag*>(kp->Parameter.GetProperty());
			if (pBranchTag)
			{
				bool bShouldBranch = false;

				if (pBranchTag->GetTrigger()==s_branchAlways)
				{
					bShouldBranch = true;
				}
				else if (pBranchTag->GetTrigger()==s_branchWalk)
				{
					bShouldBranch = CPedMotionData::GetIsWalking(desiredMbrY);
				}
				else if (pBranchTag->GetTrigger()==s_branchRun)
				{
					bShouldBranch = CPedMotionData::GetIsRunning(desiredMbrY) || CPedMotionData::GetIsSprinting(desiredMbrY);
				}
				else if (pBranchTag->GetTrigger()==s_branchIdle)
				{
					bShouldBranch = !bWantsToMove && !bWantsToTurn;
				}
				else if (pBranchTag->GetTrigger()==s_branchIdleTurn)
				{
					bShouldBranch = !bWantsToMove && bWantsToTurn;
				}
				else if (pBranchTag->GetTrigger()==s_branchAim)
				{
					bShouldBranch = bPlayerWantsToAimOrFire;
				}

				if (bShouldBranch)
				{
					animTaskDebugf(this, "Branch tag hit - blend out set: %s, currentItemId:%s, trigger: %s, nextItemId: %s", m_matchedSet.TryGetCStr(), m_pMatchedItem ? m_pMatchedItem->m_id.TryGetCStr() : "none!", pBranchTag->GetTrigger().TryGetCStr(), pBranchTag->GetBlendOutItemId().TryGetCStr());
					nextItemId = pBranchTag->GetBlendOutItemId();
					return true;
				}
			}
		}
	}

	m_bBranchTagsDirty = false;

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::ChooseGetupSet_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(pPed && pPed->IsNetworkClone())
	{
		if(IsTaskRunningOverNetwork_Clone(CTaskTypes::TASK_GET_UP))
		{
			// it's possible the clone task was set off locally without the owner task running
			// this puts the task immediately into ChooseGetupSet - however mid UpdateClonedFSM the task can detect
			// the owner task has appeared in the QI so we need to check here and hang on if that's the case
			// so we get the data over the network.
			if((m_pointCloudPoseSets.GetNumBlendOutSets() == 0) || (GetStateFromNetwork() <= State_ChooseGetupSet))
			{
				// Need to keep the current pose whilst we wait for a valid clip matrix
				if(pPed->GetMovePed().GetState() == CMovePed::kStateAnimated)
				{
					pPed->SwitchToStaticFrame(INSTANT_BLEND_DURATION);
				}
				return FSM_Continue;
			}
		}
	}

	// If the ped is stuck underneath something it could be a few seconds before we either kill them or they are able to get free
	// Need to maintain any bound sizing/offset/orientation while we choose a get-up set
	pPed->SetDesiredBoundHeading(pPed->GetBoundHeading());
	pPed->SetDesiredBoundPitch(pPed->GetBoundPitch());
	pPed->SetDesiredBoundOffset(pPed->GetBoundOffset());
	pPed->SetTaskCapsuleRadius(pPed->GetCurrentMainMoverCapsuleRadius());

	// Don't attempt to get up if the ped is should die.
	if(pPed->GetIsDeadOrDying() && !IsAllowedWhenDead(m_forcedSet))
	{
		TaskSetState(State_Finish);
		return FSM_Continue;
	}

	if (m_bDisableVehicleImpacts)
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableVehicleImpacts, true);

	m_nStartClipCounter += GetTimeStepInMilliseconds();
	u32 nMaxWaitTime = sm_Tunables.GetStartClipWaitTime(pPed);

	if(m_matchedSet!=NMBS_INVALID)
		m_GetupRequestHelper.Request(m_matchedSet);

	// Have we been waiting long enough?
	// Or have we requested an async probe that is now ready?
	// Or am I a clone? (I should have blend out anim set data from the 
	// network by now as I've been delayed in State_StreamBlendOutClipComplete_Clone until it comes over)
	if((m_nStartClipCounter > nMaxWaitTime) || (m_ProbeHelper != NULL && m_ProbeHelper->GetAllResultsReady()) || pPed->IsNetworkClone())
	{
		// we've waiting long enough...
		m_nStartClipCounter = 0;

		Matrix34 animMatrix;
		CNmBlendOutItem* pBlendOutItem = NULL;
		eNmBlendOutSet eBlendOutSet;
		float fBlendOutTime = 0.0f;

		// Search for anims which match the current ragdoll pose.
		bool bAnimMatchFound = PickClipUsingPointCloud(pPed, &animMatrix, pBlendOutItem, eBlendOutSet, fBlendOutTime);

		// If the matched item doesn't allow us to blend out then we kill the ped...
		if(pBlendOutItem != NULL && pBlendOutItem->m_type == NMBT_DISALLOWBLENDOUT && 
			!pPed->IsPlayer() && !pPed->IsNetworkClone())
		{
			CTask* pTaskNM = rage_new CTaskNMRelax(1000, 2000);
			CEventSwitch2NM event(3000, pTaskNM);

			// If we're already ragdolled then force the ped to die...
			if(pPed->GetUsingRagdoll())
			{
				pPed->ForceDeath(true);
				pPed->GetPedIntelligence()->AddEvent(event);
			}
			// ...Otherwise mark the ped to die when ragdolled and then force the ped into ragdoll
			else
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, true);
				pPed->SwitchToRagdoll(event);
			}
		}
		else
		{
			Matrix34 rootMatrix;
			pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);
			if(!bAnimMatchFound)
			{
				// Use the root bone position as a best guess.
				animMatrix.d = rootMatrix.d;

				// Copy orientation from the root matrix too.
				float fSwitchHeading = rage::Atan2f(-rootMatrix.b.x, rootMatrix.b.y);
				//fSwitchHeading -= HALF_PI;
				animMatrix.MakeRotateZ(fSwitchHeading);
			}
			else if(pBlendOutItem != NULL && pBlendOutItem->GetClip() == ms_GetUpSwimmingUnderwaterBackClip)
			{
				// If we end up underwater on our back, rather than create a more elaborate animation to both get us onto our front and rotate us 180 degrees about our z-axis
				// we just have an animation that gets us onto our front and we ensure here that our new heading doesn't require a 180 degree z-axis rotation
				animMatrix.RotateLocalZ(PI);
			}

			// If this is running on a network clone (and it's not running locally) and we're treating this ped as a player and we are stuck
			// then try and have the clone get up at the same position as the owner. Should result in much quicker getups on the clone since
			// it should save us the 2-3 seconds to find a suitable position to get up
			if(pPed->IsNetworkClone() && GetRunAsAClone() && GetTreatAsPlayer(*pPed) && m_nStuckCounter > 0)
			{
				CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
				if (pPedObj && pPedObj->GetNetBlender())
				{
					animMatrix.d = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
				}
			}

			// We need to check that the calculated matrix position is safe to place the ped. 
			// If FixupClipMatrix() returns false, there is something in the
			// ped's way so we won't return to animation this time round.
			if(!FixupClipMatrix(pPed, &animMatrix, pBlendOutItem, eBlendOutSet, fBlendOutTime))
			{
				// this has failed for whatever reason so we're starting again...
				if(pPed->IsNetworkClone())
				{
					// bail out of the task if we are stuck here and the main ped is not running the task anymore
					if (GetTimeInState()>2.0f && GetStateFromNetwork() == -1)
					{
#if __DEV
						taskDebugf2("Frame %d : %s : FSM_Quit m_bCloneTaskLocallyCreated %s.  ped %s [0x%p] : State %d,  GetTimeInState() %.2f.", 
							fwTimer::GetFrameCount(),
							__FUNCTION__,
							m_bCloneTaskLocallyCreated?"TRUE":"FALSE",
							pPed->GetDebugName(),
							pPed,
							GetState(),
							GetTimeInState());
#endif
						return FSM_Quit;
					}
				}
				else
				{
					// wipe any data we may have generated in the call to PickClipUsingPointCloud so it's not passed over the network....
					m_pointCloudPoseSets.Reset();
				}

				if(FindSubTaskOfType(CTaskTypes::TASK_CRAWL) != NULL)
				{
					if(m_forcedSet == NMBS_CRAWL_GETUPS)
					{
						TaskSetState(State_Crawl);
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					}
				}
				else if(m_forcedSet == NMBS_CRAWL_GETUPS && m_ProbeHelper == NULL)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
					SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				}
				// Need to keep the current pose whilst we wait for a valid clip matrix
				else if(pPed->GetMovePed().GetState() == CMovePed::kStateAnimated)
				{
					pPed->SwitchToStaticFrame(INSTANT_BLEND_DURATION);
					ActivateSlopeFixup(pPed, INSTANT_BLEND_IN_DELTA);
				}

				return FSM_Continue;
			}

			// Don't set the member variables until after we've determined we have a safe position to put our capsule
			m_pMatchedItem = pBlendOutItem;
			m_matchedSet = eBlendOutSet;
			m_matchedTime = fBlendOutTime;
			
			// If we don't have the getup set in memory yet, stay down and try to stream it in.
			if(m_matchedSet!=NMBS_INVALID && !m_GetupRequestHelper.Request(m_matchedSet))
			{
				// Need to keep the current pose whilst we wait for the anim to stream in
				// use the static frame while we wait
				if (pPed->GetMovePed().GetState()==CMovePed::kStateAnimated)
				{
					pPed->SwitchToStaticFrame(INSTANT_BLEND_DURATION);
					ActivateSlopeFixup(pPed, INSTANT_BLEND_IN_DELTA);
				}

				return FSM_Continue;
			}

#if __BANK
			// Used by the AnimViewer when forcing a ped to getup from a given pose.  Keep the ped in place by disabling collision and setting to fixed
			// Need to undo that here
			// Might be safe to always do this as we really shouldn't ever have peds that are getting up that have collision disabled and/or are fixed
			// but figured it would be safer to wrap with a flag
			if (m_bForceEnableCollisionOnGetup)
			{
				pPed->EnableCollision();
				pPed->SetFixedPhysics(false);
			}
#endif

			const Vector3 vPrevPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetMatrix().GetCol3());

			// If we're currently in ragdoll, we should make the switch from ragdoll to animation now.
			if (pPed->GetUsingRagdoll())
			{
				pPed->SetMatrix(animMatrix, false, false, false);
				ActivateAnimatedInstance(pPed, &animMatrix);
				m_bInstantBlendInNetwork = true;
			}
			else
			{
				// The point cloud match will likely require a change of space for the cross blend to work
				// (the matched anim probably doesn't have the same offset from the root to the mover as the current one.
				// Work out a transformation matrix from the old anim to the new one, and use the ragdoll frame to
				// blend between them (much as we do when blending from nm)
				Matrix34 poseFrametoAnimMtx;
				Matrix34 pedMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
				poseFrametoAnimMtx.FastInverse(animMatrix);
				poseFrametoAnimMtx.DotFromLeft(pedMatrix);

				pPed->GetSkeleton()->Transform(RCC_MAT34V(poseFrametoAnimMtx));
				// Need to pose the local matrices of skeleton because the above transform only updates the object space ones.
				pPed->InverseUpdateSkeleton();

				// Our last ragdoll bounds matrix will no longer be valid so need to consider this as a 'warp'...
				pPed->SetMatrix(animMatrix, true, true, true);

				// Now need to make sure that the ragdoll bounds match the skeleton otherwise if our ragdoll activates on this same frame the
				// bounds will be out-of-date!
				pPed->GetRagdollInst()->PoseBoundsFromSkeleton(true, true);
				pPed->GetRagdollInst()->PoseBoundsFromSkeleton(true, true);

				float ragdollFrameBlendDuration = SLOW_BLEND_DURATION;
				if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithDeepSurface))
				{
					ragdollFrameBlendDuration = REALLY_SLOW_BLEND_DURATION;
				}
				else if (m_pMatchedItem && m_pMatchedItem->IsPoseItem())
				{
					CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_pMatchedItem);
					ragdollFrameBlendDuration = pPoseItem->m_ragdollFrameBlendDuration;
					m_fBlendDuration = pPoseItem->m_ragdollFrameBlendDuration;
				}

				if (pPed->GetAnimDirector())
				{
					fwAnimDirectorComponentRagDoll* componentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();
					if(componentRagDoll)
					{
						// Actually make the switch to the animated instance.
						componentRagDoll->PoseFromRagDoll(*pPed->GetAnimDirector()->GetCreature());

						// if we're coming from the static frame state, switch to animated
						if (pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame)
						{
							pPed->GetMovePed().SwitchToAnimated(m_fBlendDuration, true);
							if (!m_bRunningAsMotionTask)
							{
								// force the motion state to restart the ped motion move network
								pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
							}
							m_bInstantBlendInNetwork = true;
						}
						else
						{
							// if we're already in animated, make sure we insert the static frame into our
							// move network to blend with.
							m_bInsertRagdollFrame = true;
						}
					}
				}

				// We may have been running a ragdoll response task while not necessarily ragdolling - in which case we need to clear it
				pPed->GetPedIntelligence()->ClearRagdollEventResponse();
				
				ActivateSlopeFixup(pPed, ragdollFrameBlendDuration > 0.0f ? 1.0f / ragdollFrameBlendDuration : INSTANT_BLEND_IN_DELTA);
			}

			// Need to recompute cached local positions for sticky aim helpers (third/first person) if player is aiming at this ped.
			// (Same as code in CPedRagdollComponent::FixupSkeleton on RDR).
			camStickyAimHelper* pStickyAimHelper = nullptr;
			if (camThirdPersonAimCamera* pThirdPersonAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera())
			{
				pStickyAimHelper = pThirdPersonAimCamera->GetStickyAimHelper();					
			}
			else if (camFirstPersonAimCamera* pFirstPersonAimCamera = camInterface::GetGameplayDirector().GetFirstPersonAimCamera())
			{
				pStickyAimHelper = pFirstPersonAimCamera->GetStickyAimHelper();
			}

			if (pStickyAimHelper)
			{
				pStickyAimHelper->FixupPositionOffsetsFromRagdoll(pPed, vPrevPosition, VEC3V_TO_VECTOR3(pPed->GetMatrix().GetCol3()));
			}
		}

		// Decide on which state to go to next depending on the choice of anim.
		if (m_pMatchedItem)
		{
			// if a move task hasn't been specified by the controlling
			// task, and we;re the player, add a move player task
			// to test movement intentions
			if (pPed->IsLocalPlayer() && !m_pMoveTask)
			{
				CTaskComplexControlMovement* pTaskControlMovement = static_cast<CTaskComplexControlMovement*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));

				if (pTaskControlMovement)
				{
					if (pTaskControlMovement->GetMoveTaskType()!=CTaskTypes::TASK_MOVE_PLAYER)
					{
						pTaskControlMovement->SetNewMoveTask(rage_new CTaskMovePlayer());
					}
				}
				else
				{
					CTask* pTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
					if (!pTask || pTask->GetTaskType()!=CTaskTypes::TASK_MOVE_PLAYER)
					{
						pPed->GetPedIntelligence()->AddTaskMovement( rage_new CTaskMovePlayer());
					}
				}
			}

			if ( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PointGunLeftHandSupporting) &&
				m_pMatchedItem->IsPoseItem() && ((CNmBlendOutPoseItem*)(m_pMatchedItem))->m_allowInstantBlendToAim )
			{
				m_bForceUpperBodyAim = true;
			}

			SetStateFromBlendOutItem(m_pMatchedItem);
		}
		else
		{
			DoDirectBlendOut();
			TaskSetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskGetUp::ChooseGetupSet_OnExit(CPed* pPed)
{
	const bool bIsLocalPlayerInControl = pPed->IsLocalPlayer() && CGameWorld::GetMainPlayerInfo() &&
		!CGameWorld::GetMainPlayerInfo()->AreControlsDisabled();
	if( bIsLocalPlayerInControl && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup))
	{
		pPed->NewSay("GET_UP_FROM_FALL");
	}

	// If we're not restarting the current state or going into the crawl state then reset any bound changes
	if(!m_Flags.IsFlagSet(aiTaskFlags::RestartCurrentState) && GetState() != State_Crawl)
	{
		// Reset any bound sizing/offset/orientation
		pPed->ClearBound();
		pPed->SetBoundPitch(0.0f);
		pPed->SetBoundHeading(0.0f);
		pPed->SetBoundOffset(VEC3_ZERO);
	}
}

void CTaskGetUp::DecideOnNextAction(atHashString nextItemId)
{
	// New metadata system - check the metadata item for the next anim to play (if any)
	if(nextItemId.GetHash()!=0)
	{
		CNmBlendOutItem* pItem = CNmBlendOutSetManager::FindItem(m_matchedSet, nextItemId);
		if (pItem)
		{
			// Matched time is only used for the first 'action' - now that we're deciding on the *next* action
			// the matched time needs to be cleared
			m_matchedTime = 0.0f;
			SetStateFromBlendOutItem(pItem);
		}
		else
		{
#if !__FINAL
			taskWarningf("Unable to find blend out item for hash %u (%s) in blend out metadata set '%s'. Ending task!", nextItemId.GetHash(), nextItemId.GetCStr(), CNmBlendOutSetManager::GetBlendOutSetName(m_matchedSet));
#endif
			TaskSetState(State_Finish);
		}
	}
	else
	{
		if (m_bPressedToGoIntoCover)
		{
			CPed& rPed = *GetPed();
			rPed.SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover, true);
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAIUpdate, true);
			aiDebugf1("Frame : %i, leaving get up task, wants to enter cover", fwTimer::GetFrameCount());
		}
		animTaskDebugf(this, "Ending getup - blend out set: %s, currentItemId:%s, no next item specified", m_matchedSet.TryGetCStr(), m_pMatchedItem ? m_pMatchedItem->m_id.TryGetCStr() : "none!");
		TaskSetState(State_Finish);
	}
}

void CTaskGetUp::SetStateFromBlendOutItem(CNmBlendOutItem* pItem)
{
	if (pItem)
	{
		s32 currentState = GetState();

		if (pItem->HasClipSet())
		{
			m_clipSetId.SetHash(pItem->GetClipSet().GetHash());
			m_clipId.SetHash(pItem->GetClip().GetHash());
		}

		m_bFailedToLoadAssets = false;

		animTaskDebugf(this, "Starting next blend out item - blend out set: %s, currentItemId:%s, newItemId: %s", m_matchedSet.TryGetCStr(), m_pMatchedItem ? m_pMatchedItem->m_id.TryGetCStr() : "none!", pItem->m_id.TryGetCStr());

		m_pMatchedItem = pItem;
		switch (pItem->m_type)
		{
		case NMBT_DIRECTIONALBLEND:
			{
				TaskSetState(State_PlayingGetUpBlend);
				break;
			}
		case NMBT_SINGLECLIP:
			{
				fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
				if (Verifyf(pSet && pSet->IsStreamedIn_DEPRECATED(), "The clipset for the single clip NMBT was not streamed in yet!  Doing direct transition. blend out set %s, clip set %s", CNmBlendOutSetManager::GetBlendOutSetName(m_matchedSet), m_clipSetId.TryGetCStr() ? m_clipSetId.GetCStr() : "UNKNOWN" ))
				{
					TaskSetState(State_PlayingGetUpClip);
				}
				else
				{
					DoDirectBlendOut();
					TaskSetState(State_Finish);
				}
				break;
			}
		case NMBT_AIMFROMGROUND:
			{
				TaskSetState(State_AimingFromGround);
				break;
			}
		case NMBT_FORCEMOTIONSTATE:
			{
				TaskSetState(State_ForceMotionState);
				break;
			}
		case NMBT_SETBLEND:
			{
				TaskSetState(State_SetBlendDuration);
				break;
			}
		case NMBT_DIRECTBLENDOUT:
			{
				TaskSetState(State_DirectBlendOut);
				break;
			}
		case NMBT_UPPERBODYREACTION:
			{
				TaskSetState(State_PlayReactionClip);
				break;
			}
		case NMBT_DISALLOWBLENDOUT:
			{
				TaskSetState(State_Finish);
				break;
			}
		case NMBT_CRAWL:
			{
				TaskSetState(State_Crawl);
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				break;
			}
		default:
			{
				taskAssertf(false, "Unexpected nm blend out type in metadata!");
				TaskSetState(State_Finish);
				break;
			}
		}

		if (currentState == GetState())
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}

		m_bBranchTagsDirty = true;
	}
	else
	{
		// blend out item is null, time to exit...
		animTaskDebugf(this, "Ending getup - blend out set: %s, currentItemId:%s, no next item specified", m_matchedSet.TryGetCStr(), m_pMatchedItem ? m_pMatchedItem->m_id.TryGetCStr() : "none!");
		TaskSetState(State_Finish);
	}
}

bool CTaskGetUp::TestWorldForFlatnessAndClearanceForLyingDownPed(CPed* pPed) const
{
	// Assumes the ped is currently lying down, and tests to see if the ground 
	// is generally flat and free of obstructions around lying-down ped.
	//
	// To test for "flat" ground, we'll probe around the ped in a roughly kite-shape (one at the head,
	// one below the root, and two near the shoulders), and confirm the deviation isn't too high.
	//
	// To test for obstructing geometry, we'll just use a capsule approximately where the ped will be.

	// p0 starts at the head, then is pushed 1.1t from p1 to p0
	// p1 is 2.0t from head to root
	// p2 is 0.4t from head to root + 0.25m off to the right
	// p3 is 0.4t from head to root + 0.25m off to the left
	Vector3 p[4];
	// temp vectors
	Vector3 t0;
	Vector3 t1;
	pPed->GetBonePosition(p[0], BONETAG_HEAD);
	pPed->GetBonePosition(t0, BONETAG_PELVIS);
	p[0].z += 0.5f;
	t0.z += 0.5f;
	p[1].Lerp(2.0f, p[0], t0);
	p[2].Lerp(0.4f, p[0], t0);
	p[3].Lerp(0.4f, p[0], t0);
	t1.Subtract(p[0], t0);
	t0.Cross(t1, Vector3(0.0f, 0.0f, 1.0f));
	t0.Normalize();
	p[2].AddScaled(t0, 0.25f);
	p[3].AddScaled(t0, -0.25f);
	p[0].Lerp(1.1f, p[1], p[0]);

	// Ground heights at p[i]
	float h[4];

	int nTestTypes = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;

	// Flat ground test
	{
		float heightMax = -LARGE_FLOAT;
		float heightMin = LARGE_FLOAT;

		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<1> probeResults;
		probeDesc.SetIncludeFlags(nTestTypes);
		probeDesc.SetExcludeEntity(pPed);
		probeDesc.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
		probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
		probeDesc.SetResultsStructure(&probeResults);

		for(int i = 0; i < 4; i++)
		{
			Vector3 end = p[i];
			end.z -= 2.5f;
			probeDesc.SetStartAndEnd(p[i], end);
			if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				return false;
			}
			h[i] = probeResults[0].GetHitPosition().z;
			heightMax = (h[i] > heightMax) ? h[i] : heightMax;
			heightMin = (h[i] < heightMin) ? h[i] : heightMin;

			probeResults.Reset();
		}

		const float kfMaxDeviation = 0.15f;
		if(heightMax - heightMin > kfMaxDeviation)
		{
			return false;
		}
	}

	// Obstructing geometry test
	{
		// Create a capsule just above the ground with interior segment p0,p1 (plus a little vertical to float 0.1m above the ground)
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestFixedResults<1> capsuleResults;
		capsuleDesc.SetResultsStructure(&capsuleResults);

		// Make a capsule that extends from p[1] to almost p[0]
		// It's ok to not test all the way to the head since the ped
		// getup bound will protect during the transition.
		Vector3 v0;
		Vector3 v1(p[1]);
		float fDistP0P1 = p[0].Dist(p[1]);
		v0.Lerp((fDistP0P1-sfCapsuleShortenDist)/fDistP0P1,p[1],p[0]);
		v0.z = h[0] + sfTestCapsuleRadius + sfTestCapsuleBuffer;
		v1.z = h[1] + sfTestCapsuleRadius + sfTestCapsuleBuffer;

		capsuleDesc.SetCapsule(v0, v1, sfTestCapsuleRadius);
		capsuleDesc.SetIsDirected(false);
		capsuleDesc.SetExcludeEntity(pPed);
		capsuleDesc.SetIncludeFlags(nTestTypes);
		capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);

#if DEBUG_DRAW && __DEV && !__SPU
		if(suDrawTestCapsuleTime > 0)
		{
			CPhysics::ms_debugDrawStore.AddCapsule(VECTOR3_TO_VEC3V(v0), VECTOR3_TO_VEC3V(v1), sfTestCapsuleRadius, Color_purple, suDrawTestCapsuleTime);
		}
#endif

		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			return false;
		}
	}

	return true;
}

float CTaskGetUp::GetPlaybackRateModifier() const
{
	float fModifier = 1.0f;

	const CPed* pPed = GetPed();
	if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
	{
		TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fFasterGetUpRateModifier, 1.5f, 1.0f, 10.0f, 0.01f);
		fModifier = fFasterGetUpRateModifier;
	}

	return fModifier;
}

#if !__FINAL
//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::Debug() const
//////////////////////////////////////////////////////////////////////////
{
#if __ASSERT

	if(GetPed())
	{
		char buffer[2560] = "\0";
		char temp[256];
		sprintf(temp, "Target = %p\n", m_Target.GetIsValid() ? m_Target.GetEntity() : 0x0);
		strcat(buffer, temp);

		if(GetPed()->IsNetworkClone())
		{
			sprintf(temp, "Target Out Of Scope ID = %d\n", m_TargetOutOfScopeId_Clone);
			strcat(buffer, temp);
		}
		
		atString poseSets = m_pointCloudPoseSets.DebugDump();
		strcat(buffer, poseSets.c_str());

		Vector3 pedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		grcDebugDraw::Text(pedPos, Color_white, buffer, true);
	}
#endif /*__ASSERT*/

#if DEBUG_DRAW
	if(GetState()==State_PlayingGetUpBlend)
	{
		// render the stored start heading
		const CPed* pPed = GetPed();
		Vector3 temp(0.0f, 1.0f, 0.0f);
		temp.RotateZ(m_fGetupBlendReferenceHeading);

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		temp+= vPedPosition;

		grcDebugDraw::Line( vPedPosition, temp, Color_purple );

		// render the target heading for mover fixup
		temp.Set(0.0f, 1.0f, 0.0f);
		temp.RotateZ(m_fGetupBlendInitialTargetHeading);
		temp+= vPedPosition;

		grcDebugDraw::Line( vPedPosition, temp, Color_green );
	}
#endif //DEBUG_DRAW
}
#endif //!__FINAL

#if DEBUG_DRAW && __DEV
void CTaskGetUp::DebugDrawPedCapsule(const Matrix34& transform, const Color32& color, u32 uExpiryTime, u32 uKey, float fCapsuleRadiusMult, float fBoundHeading, float fBoundPitch, const Vector3& vBoundOffset)
{
	CPed* pPed = GetPed();

	//Get biped information back from the ped
	const CBaseCapsuleInfo* pBaseCapsuleInfo = pPed->GetCapsuleInfo();
	if (pBaseCapsuleInfo == NULL)
	{
		return;
	}

	float fPedCapsuleRadius = pBaseCapsuleInfo->GetHalfWidth();

	// Maybe we always want to do this due to dynamic capsule size
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
	{
		fPedCapsuleRadius = pPed->GetCurrentMainMoverCapsuleRadius();
	}

	fPedCapsuleRadius *= fCapsuleRadiusMult;

	Matrix34 xTransform = transform;
	xTransform.RotateLocalZ(fBoundHeading);
	xTransform.RotateLocalX(fBoundPitch);
	xTransform.Translate(vBoundOffset);

	Vector3 vStart = xTransform.d;
	Vector3 vEnd = vStart;

	const CBipedCapsuleInfo* pBipedCapsuleInfo = pBaseCapsuleInfo->GetBipedCapsuleInfo();
	if (pBipedCapsuleInfo)
	{
		vStart += xTransform.c * (pBipedCapsuleInfo->GetHeadHeight() - fPedCapsuleRadius);
		vEnd += xTransform.c * (pBipedCapsuleInfo->GetCapsuleZOffset() + fPedCapsuleRadius);
	}
	else
	{
		const CQuadrupedCapsuleInfo *pQuadrupedCapsuleInfo = pBaseCapsuleInfo->GetQuadrupedCapsuleInfo();
		if (pQuadrupedCapsuleInfo)
		{
			float fPedHalfCapsuleLength = pQuadrupedCapsuleInfo->GetMainBoundLength() * 0.5f;
			vStart += xTransform.b * fPedHalfCapsuleLength;
			vEnd -= xTransform.b * fPedHalfCapsuleLength;
		}
		else
		{
			const CFishCapsuleInfo *pFishCapsuleInfo = pBaseCapsuleInfo->GetFishCapsuleInfo();
			if (pFishCapsuleInfo)
			{
				float fPedHalfCapsuleLength = pFishCapsuleInfo->GetMainBoundLength() * 0.5f;
				vStart += xTransform.b * fPedHalfCapsuleLength;
				vEnd -= xTransform.b * fPedHalfCapsuleLength;
			}
			else
			{
				return;
			}
		}
	}

	CPhysics::ms_debugDrawStore.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), fPedCapsuleRadius, color, uExpiryTime, uKey, false);
}
#endif // DEBUG_DRAW && __DEV

//////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::TestLOS(const Vector3& vStart, const Vector3& vEnd) const
//////////////////////////////////////////////////////////////////////////
{
	// Also do a line test from the ragdoll root position to the new position to make sure it's not on the other side of a wall.
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
	if (m_pExclusionEntity)
	{
		probeDesc.SetExcludeEntity(m_pExclusionEntity);
	}
	return WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
}

//////////////////////////////////////////////////////////////////////////
const crClip* CTaskGetUp::GetClip() const
//////////////////////////////////////////////////////////////////////////
{
	if(m_clipId!=CLIP_ID_INVALID && m_clipSetId!=CLIP_SET_ID_INVALID)
	{
		return fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId);
	}
	else
	{
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::PlayingGetUpClip_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Get the clip and pass it in
	const crClip* pClip = GetClip();
	//@@: location CTASKGETUP_PLAYINGGETUPCLIP_ONENTER
	taskAssertf(pClip, "Unable to load get up clip in task blend from nm! blend out set:%s, %s", m_matchedSet.TryGetCStr() ? m_matchedSet.GetCStr(): "NULL", fwClipSetManager::GetRequestFailReason(m_clipSetId, m_clipId).c_str() );

	if (GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_DYING_DEAD)
	{
		CTaskDyingDead* pTask = static_cast<CTaskDyingDead*>(GetParent());
		pTask->SetDeathAnimationBySet(m_clipSetId, m_clipId);
	}

	if (m_matchedSet == NMBS_WRITHE)
	{
#if !__FINAL
		if (CPed::ms_bActivateRagdollOnCollision)
#endif
		{
			CEventSwitch2NM event(10000, rage_new CTaskNMRelax(1000, 10000));
			pPed->SetActivateRagdollOnCollisionEvent(event);
			pPed->SetActivateRagdollOnCollision(true);

			// See if there's enough room to continue writhing
			if (TestWorldForFlatnessAndClearanceForLyingDownPed(pPed))
			{
				pPed->SetRagdollOnCollisionAllowedPenetration(sfRagdollOnCollisionfAllowedPenetration);
			}
			else
			{
				pPed->SetRagdollOnCollisionAllowedPenetration(0.0f);
			}

			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, true);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds, true);
		}

		if (pPed->IsNetworkClone())
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	
	}
	else if (m_matchedSet == NMBS_CRAWL_GETUPS)
	{
		float fNewCapsuleRadius = pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult;
		pPed->SetTaskCapsuleRadius(fNewCapsuleRadius);
		pPed->ResizePlayerLegBound(true);
		pPed->OverrideCapsuleRadiusGrowSpeed(INSTANT_BLEND_IN_DELTA);
	}

	m_fBlendOutRootIkSlopeFixupPhase = -1.0f;

	if (pClip)
	{
		if (!m_networkHelper.IsNetworkActive() || !m_networkHelper.IsNetworkAttached())
			StartMoveNetwork();

		// Transition to the correct state
		m_networkHelper.SendRequest(ms_GetUpClipRequest);
		m_networkHelper.WaitForTargetState(ms_OnEnterGetUpClip);

		GetDefaultArmsDuration(pClip);

		if (m_bTagSyncBlend)
		{
			m_networkHelper.SendRequest(ms_TagSyncBlend);
		}
		m_networkHelper.SetFloat(ms_BlendDuration, m_fBlendDuration);
		m_networkHelper.SetFloat(ms_GetUpPhase0, pClip->ConvertTimeToPhase(m_matchedTime));
		m_networkHelper.SetClip(pClip, ms_GetUpClip);

		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

		if (m_pMatchedItem)
		{
			taskAssert(m_pMatchedItem->IsPoseItem());
			CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_pMatchedItem);
			float playbackRate = pPoseItem->GetPlaybackRate();
			playbackRate *= GetPlaybackRateModifier();
			m_networkHelper.SetFloat(ms_GetUpRate, playbackRate);
			m_networkHelper.SetBoolean(ms_GetUpLooped, pPoseItem->IsLooping());
		}	

		// Until we got the rifle writhe we must switch to pistol here, we must also set so we consider hurt mode started
		if (m_matchedSet == NMBS_WRITHE ||
			m_matchedSet==NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT ||
			m_matchedSet==NMBS_STANDING_AIMING_LEFT_PISTOL_HURT ||
			m_matchedSet==NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT ||
			m_matchedSet==NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT ||
			m_matchedSet==NMBS_STANDING_AIMING_LEFT_RIFLE_HURT ||
			m_matchedSet==NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT)
		{
			if(!pPed->IsNetworkClone())
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, true);
			}
		}

		m_fLastExtraTurnAmount = 0.0f;

#if !__NO_OUTPUT
		bool bHasMoverFixup = false;
		float startPhase = -1.0f;
		float endPhase = -1.0f;
		bHasMoverFixup = CClipEventTags::FindMoverFixUpStartEndPhases(pClip, startPhase, endPhase, false);
		animTaskDebugf(this, "Starting getup clip: %s / %s. Mover fixup: %s Phases: %.3f - %.3f, target: %s(%p), moveTask:%s", m_clipSetId.TryGetCStr(), m_clipId.TryGetCStr(), bHasMoverFixup? "yes" : "no", startPhase, endPhase, m_Target.GetEntity() ? m_Target.GetEntity()->GetModelName() : "none", m_Target.GetEntity(), m_pMoveTask.Get() ? m_pMoveTask->GetName() : "none");
#endif // !__NO_OUTPUT
	}
	else
	{
		m_bFailedToLoadAssets = true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::PlayingGetUpClip_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_bFailedToLoadAssets)
	{
		animTaskDebugf(this, "Failed to load assets - moving to next state");
		DecideOnNextAction(GetNextItem());
		return FSM_Continue;
	}

	const crClip* pClip = GetClip();
	float currentPhase = m_networkHelper.IsInTargetState() ? m_networkHelper.GetFloat(ms_GetUpClip0PhaseOut) : 0.0f;
	
	if (m_matchedSet == NMBS_WRITHE)
	{
		if (pPed->GetHurtEndTime() <= 0)
			pPed->SetHurtEndTime(fwRandom::GetRandomNumberInRange(1, 10));

		if (!pWritheClipSetRequestHelper)
			pWritheClipSetRequestHelper = CWritheClipSetRequestHelperPool::GetNextFreeHelper();

		if (pWritheClipSetRequestHelper)
			CTaskWrithe::StreamReqResourcesIn(pWritheClipSetRequestHelper, pPed, CTaskWrithe::NEXT_STATE_WRITHE);

		if (pPed->IsNetworkClone())
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	}
	else if (m_matchedSet == NMBS_CRAWL_GETUPS)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ResetMovementStaticCounter, true);

		if (pClip != NULL)
		{
			Vector3 vEulers;
			fwAnimHelpers::GetMoverTrackRotation(*pClip, currentPhase).ToEulers(vEulers);
			pPed->SetBoundHeading(-vEulers.z);
		}

		// Always force pitch/offset to the maximum
		float fBoundPitch = -HALF_PI;

		// Use the root slope fixup solver to determine the amount to pitch the bound
		CRootSlopeFixupIkSolver* pRootSlopeFixupIkSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (Verifyf(pRootSlopeFixupIkSolver != NULL, "CTaskGetUp::PlayingGetUpClip_OnUpdate: Root slope fixup has not been activated - won't be able to orient bound to slope for crawl"))
		{
			ScalarV fPitch;
			ScalarV fRoll;
			Mat34V xParentMatrix = pPed->GetTransform().GetMatrix();
			Mat34VRotateLocalZ(xParentMatrix, ScalarVFromF32(pPed->GetBoundHeading()));

			if (pRootSlopeFixupIkSolver->CalculatePitchAndRollFromGroundNormal(fPitch, fRoll, xParentMatrix.GetMat33ConstRef()))
			{
				fBoundPitch += fPitch.Getf();
			}
		}
		
		pPed->SetBoundPitch(fBoundPitch);

		if (pPed->GetCapsuleInfo()->GetBipedCapsuleInfo())
		{
			float fCapsuleRadiusDiff = pPed->GetCapsuleInfo()->GetHalfWidth() - (pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);
			Vector3 vBoundOffset(0.0f, 0.0f, pPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetCapsuleZOffset() - (fCapsuleRadiusDiff * 0.5f));
			pPed->SetBoundOffset(vBoundOffset);
		}
		pPed->ResetProcessBoundsCountdown();
	}

	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	ProcessCoverInterrupt(*pPed);

	ProcessDropDown(pPed);

	CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
	const CWeaponInfo* pWeaponInfo = pWeapMgr != NULL ? pWeapMgr->GetEquippedWeaponInfo() : NULL;

	// Until we got the rifle writhe we must switch to pistol here, we must also set so we consider hurt mode started
	if (m_matchedSet==NMBS_WRITHE ||
		m_matchedSet==NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT ||
		m_matchedSet==NMBS_STANDING_AIMING_LEFT_PISTOL_HURT ||
		m_matchedSet==NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT ||
		m_matchedSet==NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT ||
		m_matchedSet==NMBS_STANDING_AIMING_LEFT_RIFLE_HURT ||
		m_matchedSet==NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT)
	{
		if (m_networkHelper.GetBoolean(ms_DropRifle) || m_matchedSet==NMBS_WRITHE)
		{
			if (pWeaponInfo && pWeaponInfo->GetGroup() != WEAPONGROUP_UNARMED && pWeaponInfo->GetGroup() != WEAPONGROUP_PISTOL)
			{
				if(!NetworkInterface::IsGameInProgress())
				{
					pWeapMgr->DropWeapon(pWeapMgr->GetEquippedWeaponHash(), true);
				}
				else // simply delete the weapon in MP
				{
					pWeapMgr->DestroyEquippedWeaponObject(false);
					pPed->GetInventory()->RemoveWeaponAndAmmo(pWeapMgr->GetEquippedWeaponHash());
				}
			}
		}

		// There is an object tag for this also but if so, we kinda need to verify that etc...
		// This is more solid and easier to check if we "stopped" dropping that rifle
		if (!m_networkHelper.GetBoolean(ms_DropRifle) && 
			!pPed->GetPedResetFlag(CPED_RESET_FLAG_BeingElectrocuted) &&
			(!pPed->IsCivilianPed() || (pPed->PopTypeIsMission() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_GiveWeaponOnGetup))) &&
			!CPedType::IsEmergencyType(pPed->GetPedType()) &&
			(m_bDroppingRifle || m_matchedSet==NMBS_WRITHE))
		{
			CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
			{
				const CWeaponInfo* pBestPistol = pWeapMgr->GetBestWeaponInfoByGroup(WEAPONGROUP_PISTOL);
				if (pBestPistol)
				{
					pWeapMgr->EquipWeapon(pBestPistol->GetHash(), -1, true);
				}
				else
				{
					pPed->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, 100);
					pWeapMgr->EquipWeapon(WEAPONTYPE_PISTOL, -1, true);
				}
			}
		}

		m_bDroppingRifle = m_networkHelper.GetBoolean(ms_DropRifle);
	}

	taskAssert(m_pMatchedItem);
	taskAssert(m_pMatchedItem->m_type==NMBT_SINGLECLIP);
	CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_pMatchedItem);

	bool bWantsToMove = abs(pPed->GetMotionData()->GetDesiredMbrY())>0.001f || abs(pPed->GetMotionData()->GetDesiredMbrX())>0.001f;
	bool bWantsToTurn = !AreNearlyEqual(pPed->GetCurrentHeading(), pPed->GetDesiredHeading(), 0.001f);
	bool bPlayerWantsToAimOrFire = PlayerWantsToAimOrFire(pPed);

	float earlyOutPhase = pPoseItem->m_earlyOutPhase;
	if (!pPed->IsAPlayerPed() && pPed->GetWeaponManager() != NULL && pPed->GetWeaponManager()->GetIsArmed())
	{
		earlyOutPhase = Min(earlyOutPhase, pPoseItem->m_armedAIEarlyOutPhase);
	}

	ProcessUpperBodyAim(pPed, pClip, currentPhase);

	if (pClip != NULL && currentPhase >= 0.0f)
	{
		static const atHashString s_BlendOutRootIkSlopeFixup("BlendOutRootIkSlopeFixup",0xA5690A9E);
		if (m_fBlendOutRootIkSlopeFixupPhase < 0.0f && !CClipEventTags::FindEventPhase<crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlendOutRootIkSlopeFixup, m_fBlendOutRootIkSlopeFixupPhase))
		{
			// If we couldn't find an event then it means we blend out when the animation ends...
			m_fBlendOutRootIkSlopeFixupPhase = 1.0f;
		}

		static const atHashString s_BlockSecondaryAnim("BlockSecondaryAnim",0xD69F5E8E);
		const crTag* pTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlockSecondaryAnim, currentPhase);
		if (pTag != NULL && pTag->GetStart() <= currentPhase && pTag->GetEnd() >= currentPhase)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockSecondaryAnim, true);
		}

		ProcessMoverFixup(pPed, pClip, currentPhase, pPed->GetCurrentHeading());
	}

	atHashString nextItemId = GetNextItem();

	// Wait until the get-up anim finishes playing
	if (
		ProcessBranchTags(nextItemId)
		|| m_networkHelper.GetBoolean(ms_GetUpClipFinished) 
		//////////////////////////////////////////////////////////////////////////
		// remove these once all the data has been converted to BranchGetup tags in the anims.
		|| (pPoseItem != NULL && ((currentPhase > earlyOutPhase)
		|| (bWantsToTurn && currentPhase > pPoseItem->m_turnBreakOutPhase)
		|| (bWantsToMove && currentPhase > pPoseItem->m_movementBreakOutPhase)
		|| (bPlayerWantsToAimOrFire && currentPhase > pPoseItem->m_playerAimOrFireBreakOutPhase)
		//////////////////////////////////////////////////////////////////////////
		|| ((pPoseItem->m_duration>=0.0f) && GetTimeInState()>pPoseItem->m_duration)))
		)
	{
		// Audio for the player after extreme ragdoll injuries - he swears. Only do it when we have
		// a wanted level; the player seems like a bit of a moody bastard otherwise.
		const bool bIsLocalPlayerInControl = pPed->IsLocalPlayer() && CGameWorld::GetMainPlayerInfo() &&
			!CGameWorld::GetMainPlayerInfo()->AreControlsDisabled();
		if( bIsLocalPlayerInControl && pPed->GetPlayerWanted()->GetWantedLevel()>=WANTED_LEVEL1)
		{
			pPed->NewSay("GENERIC_CURSE");
		}

		DecideOnNextAction(nextItemId);
	}
	else if (pPoseItem != NULL && currentPhase > pPoseItem->m_dropDownPhase)
	{
        bool bDetectedDropDown = false;

        if(pPed->IsNetworkClone())
        {
            bDetectedDropDown = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DROP_DOWN);
        }
        else
        {
            bDetectedDropDown = pPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDropDown();
        }

        if(bDetectedDropDown)
        {
		    SetState(State_DropDown);
        }
	}

	// Once the feet have been planted we no longer need to do root slope fixup
	if (m_fBlendOutRootIkSlopeFixupPhase >= 0.0f && currentPhase > m_fBlendOutRootIkSlopeFixupPhase)
	{
		CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pSolver != NULL)
		{
			pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
		}

		if (pPed->GetAnimatedInst() != NULL && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
		{
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex(), CPhysics::GetLevel()->GetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex()) & (~ArchetypeFlags::GTA_DEEP_SURFACE_TYPE));
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::PlayingGetUpClip_OnExit(CPed* pPed)
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_matchedSet == NMBS_CRAWL_GETUPS)
	{
		// Reset the ped bound size to defaults
		pPed->SetTaskCapsuleRadius(0.0f);
		pPed->ResizePlayerLegBound(false);
		pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);

		// Reset the ped bound orientation to defaults
		pPed->ClearBound();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::PlayingReactionClip_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//pick an appropriate reaction anim for the ped, and play it
	fwMvClipSetId clipSet;
	fwMvClipId anim;
	const crClip* pClip = NULL;

	m_bDoReactionClip = ShouldPlayReactionClip(pPed);

	if (PickReactionClip(pPed, clipSet, anim))
	{
		// Get the clip to be used
		//@@: location CTASKGETUP_PLAYINGREACTIONCLIP_ONENTER_GET_CLIP
		pClip = fwAnimManager::GetClipIfExistsBySetId(clipSet, anim);
		taskAssertf(pClip, "No reaction clip found in task blend from nm!, Blend out set: %s, %s", m_matchedSet.TryGetCStr() ? m_matchedSet.GetCStr() : "NULL", fwClipSetManager::GetRequestFailReason(m_clipSetId, m_clipId).c_str());

		if (!pClip)
		{
			m_bFailedToLoadAssets = true;
		}
	}

	if (m_bDoReactionClip && !m_bFailedToLoadAssets)
	{
		if (!m_networkHelper.IsNetworkActive())
			StartMoveNetwork();

		// Transition to the correct state
		m_networkHelper.SendRequest(ms_ReactionClipRequest);
		m_networkHelper.WaitForTargetState(ms_OnEnterReactionClip);

		m_networkHelper.SetClip(pClip, ms_ReactionClip);

		if (!m_bRunningAsMotionTask)
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::PlayingReactionClip_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

	// if we're not allowed to do the reaction clip, or we couldn't get the asset, bail immediately.
	if (!m_bDoReactionClip || m_bFailedToLoadAssets)
	{
		DecideOnNextAction(GetNextItem());
		return FSM_Continue;
	}

	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	// HACK - Remove this once we have proper assets for bump reactions
	//static u32 tempClipHash = ATSTRINGHASH("excited_idle_a", 0x4A7793E9);

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceImprovedIdleTurns, true);

	//wait till the anim ends, then bail (should be fully back in animation at this point)
	if (m_networkHelper.GetBoolean(ms_ReactionClipFinished)) 
	{
		DecideOnNextAction(GetNextItem());
		return FSM_Continue;
	}
	else
	{
		//update the desired heading and head look at to track the responsible entity
		if (m_pBumpedByEntity)
		{
			Vec3V pos = m_pBumpedByEntity->GetTransform().GetPosition();

			const crClip* pClip = GetClip();


			pPed->SetDesiredHeading(RCC_VECTOR3(pos));

			if (pClip)
			{
				pPed->GetIkManager().LookAt(
					0, 
					m_pBumpedByEntity, 
					(s32)((pClip->GetDuration() - pClip->ConvertPhaseToTime(m_networkHelper.GetFloat(ms_ReactionClipPhaseOut)))*1000.0f), 
					m_pBumpedByEntity->GetIsTypePed() ? BONETAG_HEAD : BONETAG_ROOT, &VEC3_ZERO
					);
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::PickReactionClip(CPed* UNUSED_PARAM(pPed), fwMvClipSetId &clipSetId, fwMvClipId &clipId)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	static fwMvClipSetId reactionClipSet("bump_reactions",0x36BD0D49);
	// TODO - work out an appropriate anim based on ped gender / type / etc
	// currently just picks a random anim from the set

	fwMvClipSetId clipset = reactionClipSet;

	if (m_pMatchedItem && m_pMatchedItem->m_type==NMBT_UPPERBODYREACTION)
	{
		clipset.SetHash(static_cast<CNmBlendOutReactionItem*>(m_pMatchedItem)->m_clipSet.GetHash());	 
	}

	if (fwClipSetManager::Request_DEPRECATED(reactionClipSet))
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(reactionClipSet);
		if (taskVerifyf(pClipSet, "Unable to find bump reaction anim set!"))
		{
			s32 numAnims = pClipSet->GetClipItemCount();

			if (numAnims>0)
			{
				s32 random = fwRandom::GetRandomNumberInRange(1, numAnims);

				clipSetId.SetHash(reactionClipSet.GetHash());
				m_clipSetId = clipSetId;
				clipId.SetHash(pClipSet->GetClipItemIdByIndex(random-1).GetHash());
				m_clipId = clipId;
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ShouldPlayReactionClip(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (ms_bEnableBumpedReactionClips)
	{
		// only do bumped reactions if the ped is running an the ambient task
		// (don't want to do this for in combat and mission peds for example)
		// Also never want to do it for the local player
		if (
			!pPed->IsLocalPlayer()
			&& !pPed->PopTypeIsMission()
			&& m_bAllowBumpReaction
			&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed)
			&& !pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_THREAT_RESPONSE)
			&& !pPed->GetWeaponManager()->GetIsArmed()
			&& !CTaskAgitated::FindAgitatedTaskForPed(*pPed)
			)
		{
			// make sure the bumped by entity exists, and is a ped
			if (m_pBumpedByEntity && m_pBumpedByEntity->GetIsTypePed())
			{
				// check we've got the correct type of item
				if (m_pMatchedItem && m_pMatchedItem->m_type==NMBT_UPPERBODYREACTION)
				{
					// use the item probability value
					CNmBlendOutReactionItem* pItem = static_cast<CNmBlendOutReactionItem*>(m_pMatchedItem);
					return fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= pItem->m_doReactionChance;
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::PlayingGetUpBlend_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPlayerSpecialAbilityManager::ChargeEvent(ACET_FALL_OVER, pPed);

	if (!m_networkHelper.IsNetworkActive() || m_networkHelper.HasNetworkExpired())
		StartMoveNetwork();

	// Transition to the correct state
	m_networkHelper.SendRequest(ms_GetUpBlendRequest);
	m_networkHelper.WaitForTargetState(ms_OnEnterGetUpBlend);

	if (m_bTagSyncBlend)
	{
		m_networkHelper.SendRequest(ms_TagSyncBlend);
	}
	m_networkHelper.SetFloat(ms_BlendDuration, m_fBlendDuration);
	const crClip* pClip = GetClip();
	if (pClip != NULL)
	{
		m_networkHelper.SetFloat(ms_GetUpPhase0, pClip->ConvertTimeToPhase(m_matchedTime));
	}

	// Set the correct anim set to use for the get up blend
	m_networkHelper.SetClipSet(m_clipSetId, ms_GetUpBlendSet);

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	Matrix34 pedMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());

	m_fGetupBlendReferenceHeading = rage::Atan2f(-pedMatrix.b.x, pedMatrix.b.y);

	m_fGetupBlendCurrentHeading = CalcDesiredDirection();
	m_fGetupBlendInitialTargetHeading = fwAngle::LimitRadianAngleSafe(m_fGetupBlendCurrentHeading+m_fGetupBlendReferenceHeading);
	m_fLastExtraTurnAmount = 0.0f;

	m_fBlendOutRootIkSlopeFixupPhase = -1.0f;

	if (m_pMatchedItem)
	{
		taskAssert(m_pMatchedItem->m_type==NMBT_DIRECTIONALBLEND);
		CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_pMatchedItem);
		float playbackRate = pPoseItem->GetPlaybackRate();
		playbackRate *= GetPlaybackRateModifier();
		m_networkHelper.SetFloat(ms_GetUpRate, playbackRate);
		m_networkHelper.SetBoolean(ms_GetUpLooped, pPoseItem->IsLooping()); 
	}

	m_bBlendedInDefaultArms = false;
}

//////////////////////////////////////////////////////////////////////////
float CTaskGetUp::ProcessMoverFixup(CPed* pPed, const crClip* pClip, float fPhase, float fReferenceHeading)
//////////////////////////////////////////////////////////////////////////
{
	float moverFixupStartPhase = 0.0f;
	float moverFixupEndPhase = 1.0f;

	if ((CClipEventTags::FindMoverFixUpStartEndPhases(pClip, moverFixupStartPhase, moverFixupEndPhase, false) && fPhase>moverFixupStartPhase))
	{
		float turnTimeLeft = pClip->ConvertPhaseToTime(moverFixupEndPhase - fPhase);
		static float minTimeLeftForFixup = 0.0001f;
		float turnRequired = SubtractAngleShorter(fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading()), fReferenceHeading);

		if (turnTimeLeft>minTimeLeftForFixup)
		{
			float turnAmount = (turnRequired/turnTimeLeft)*fwTimer::GetTimeStep();

			if (turnAmount!=0.0f)
			{
				float turnLerpRate = pPed->IsLocalPlayer() ? sm_Tunables.m_PlayerMoverFixupMaxExtraHeadingChange : sm_Tunables.m_AiMoverFixupMaxExtraHeadingChange;
				turnLerpRate*=fwTimer::GetTimeStep();

				// approach m_fLastTurnAmount towards the new required turn amount
				if (turnAmount<m_fLastExtraTurnAmount)
				{
					m_fLastExtraTurnAmount = Max(m_fLastExtraTurnAmount - turnLerpRate, turnAmount);
				}
				else
				{
					m_fLastExtraTurnAmount = Min(m_fLastExtraTurnAmount + turnLerpRate, turnAmount);
				}

				// prevent overall overshoot
				if ((turnRequired<0.0f && m_fLastExtraTurnAmount<turnRequired)
					|| (turnRequired>0.0f && m_fLastExtraTurnAmount>turnRequired))
				{
					m_fLastExtraTurnAmount = turnRequired;
				}

				pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(m_fLastExtraTurnAmount);
				return m_fLastExtraTurnAmount;
			}			
		}
	}
	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::PlayingGetUpBlend_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	animAssertf(m_pMatchedItem, "Playing a directional blend, but no blend out item found!");
	animAssertf(m_pMatchedItem->m_type==NMBT_DIRECTIONALBLEND, "Playing a directional blend, but blend out item is not a directional blend!");

	CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_pMatchedItem);

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	UpdateBlendDirectionSignal();

	m_networkHelper.SetFlag(pPoseItem->HasNo180Blend(), ms_No180Blend);

	// Calc Max Angle to pass as a signal. If we aren't using a 180 blend, then max angle is 90.
	float fMaxAngle = pPoseItem->HasNo180Blend() ? (PI*0.5f) : PI;

	m_networkHelper.SetFloat(ms_GetUpBlendDirection, ConvertRadianToSignal(m_fGetupBlendCurrentHeading, fMaxAngle));

	if (!m_networkHelper.IsInTargetState())
		return FSM_Continue;

	const float fPhase = m_networkHelper.GetFloat(ms_GetUpClip0PhaseOut);
	const crClip* pClip = m_networkHelper.GetClip(ms_GetUpBlendClipOut);

	ProcessCoverInterrupt(*pPed);

	ProcessDropDown(pPed);

	if (pClip)
	{

		GetDefaultArmsDuration(pClip);
		static const atHashString s_BlendOutRootIkSlopeFixup("BlendOutRootIkSlopeFixup",0xA5690A9E);
		if (m_fBlendOutRootIkSlopeFixupPhase < 0.0f && !CClipEventTags::FindEventPhase<crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlendOutRootIkSlopeFixup, m_fBlendOutRootIkSlopeFixupPhase))
		{
			m_fBlendOutRootIkSlopeFixupPhase = 1.0f;
		}

		static const atHashString s_BlockSecondaryAnim("BlockSecondaryAnim",0xD69F5E8E);
		const crTag* pTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlockSecondaryAnim, fPhase);
		if (pTag != NULL && pTag->GetStart() <= fPhase && pTag->GetEnd() >= fPhase)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockSecondaryAnim, true);
		}

		ProcessUpperBodyAim(pPed, pClip, fPhase);
	}

	// Wait until the get-up anim finishes playing and then choose the anim to use to transition back
	// to a standing pose.

	bool bWantsToMove = abs(pPed->GetMotionData()->GetDesiredMbrY())>0.001f || abs(pPed->GetMotionData()->GetDesiredMbrX())>0.001f;
	bool bWantsToTurn = !AreNearlyEqual(pPed->GetCurrentHeading(), pPed->GetDesiredHeading(), 0.001f);
	bool bPlayerWantsToAimOrFireOrCover = PlayerWantsToAimOrFire(pPed) || m_bPressedToGoIntoCover;
	
	float fEarlyOutPhase = pPoseItem->m_earlyOutPhase;
	if (!pPed->IsAPlayerPed() && pPed->GetWeaponManager() != NULL && pPed->GetWeaponManager()->GetIsArmed())
	{
		fEarlyOutPhase = Min(fEarlyOutPhase, pPoseItem->m_armedAIEarlyOutPhase);
	}

	float extraHeading = ProcessMoverFixup(pPed, pClip, fPhase, m_fGetupBlendInitialTargetHeading);
	m_fGetupBlendInitialTargetHeading = fwAngle::LimitRadianAngleSafe(m_fGetupBlendInitialTargetHeading + extraHeading);

	atHashString nextItemId = GetNextItem();

	if(
		ProcessBranchTags(nextItemId)
		||  m_networkHelper.GetBoolean(ms_GetUpBlendFinished)
		//////////////////////////////////////////////////////////////////////////
		// Remove these once all the data is converted to BranchGetup tags in the anims
		|| (pPoseItem != NULL && ((fPhase > fEarlyOutPhase)
		|| (bWantsToTurn && fPhase > pPoseItem->m_turnBreakOutPhase)
		|| (bWantsToMove && fPhase > pPoseItem->m_movementBreakOutPhase)
		|| (bPlayerWantsToAimOrFireOrCover && fPhase > pPoseItem->m_playerAimOrFireBreakOutPhase)
		//////////////////////////////////////////////////////////////////////////
		|| ((pPoseItem->m_duration>=0.0f) && GetTimeInState()>pPoseItem->m_duration)))
		)
	{
		DecideOnNextAction(nextItemId);
	}
	else if (pPoseItem != NULL && fPhase > pPoseItem->m_dropDownPhase && pPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDropDown())
	{
		SetState(State_DropDown);
	}

	// Once the feet have been planted we no longer need to do root slope fixup
	if (m_fBlendOutRootIkSlopeFixupPhase >= 0.0f && fPhase > m_fBlendOutRootIkSlopeFixupPhase)
	{
		CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pSolver != NULL)
		{
			pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
		}

		if (pPed->GetAnimatedInst() != NULL && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
		{
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex(), CPhysics::GetLevel()->GetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex()) & (~ArchetypeFlags::GTA_DEEP_SURFACE_TYPE));
		}
	}

	if(!pPed->GetIkManager().IsLooking())
	{
		CPed* pCarJacker = pPed->GetCarJacker();
		if(pCarJacker)
		{
			const u32 uLookAtJackerHash = ATSTRINGHASH("LookAtJackerHash", 0x4E41E7BC);
			pPed->GetIkManager().LookAt(
				uLookAtJackerHash,
				pCarJacker,
				5000,
				BONETAG_HEAD,
				NULL,
				LF_WHILE_NOT_IN_FOV|LF_WIDE_PITCH_LIMIT|LF_FAST_TURN_RATE);

			m_bLookingAtCarJacker = true;
		}
	}

	return FSM_Continue;
}

void CTaskGetUp::DoDirectBlendOut()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	// do a quick blend out to the appropriate a.i. behaviour
	// Tell the a.i. system that we're doing a direct blend from nm
	pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ForceMotionState_OnEnter(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::ForceMotionState_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

	animAssertf(m_pMatchedItem, "Forcing a motion state, but no blend out item found!");
	animAssertf(m_pMatchedItem->m_type==NMBT_FORCEMOTIONSTATE, "Forcing the motion state, but blend out item is not a force motion state!");

	// Wait until the get-up anim finishes playing and then choose the anim to use to transition back
	// to a standing pose.

	CNmBlendOutMotionStateItem* pMotionStateItem = static_cast<CNmBlendOutMotionStateItem*>(m_pMatchedItem);

	if ( !m_bRunningAsMotionTask && pMotionStateItem->m_motionState!=CPedMotionStates::MotionState_None  && !pPed->GetIsSwimming() ) //pedmotion already handling swimming
	{
		pPed->ForceMotionStateThisFrame(pMotionStateItem->m_motionState, pMotionStateItem->m_forceRestart);
		if (pMotionStateItem->m_motionStartPhase>0.0f)
		{
			CTaskMotionBase* pTask = pPed->GetPrimaryMotionTaskIfExists();
			if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
			{
				((CTaskMotionPed*)pTask)->SetBeginMoveStartPhase(pMotionStateItem->m_motionStartPhase);
			}
		}
	}

	// New metadata system - check the metadata item for the next anim to play (if any)
	DecideOnNextAction(GetNextItem());

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::SetBlendDuration_OnEnter(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::SetBlendDuration_OnUpdate(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{ 

	animAssertf(m_pMatchedItem, "Setting a blend duration, but no blend out item found!");
	animAssertf(m_pMatchedItem->m_type==NMBT_SETBLEND, "Setting a blend duratio, but blend out item is not a blend item!");

	// Wait until the get-up anim finishes playing and then choose the anim to use to transition back
	// to a standing pose.

	CNmBlendOutBlendItem* pBlendItem = static_cast<CNmBlendOutBlendItem*>(m_pMatchedItem);

	m_fBlendDuration = pBlendItem->m_blendDuration;
	m_bTagSyncBlend = pBlendItem->m_tagSync;

	// New metadata system - check the metadata item for the next anim to play (if any)
	DecideOnNextAction(GetNextItem());

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::DirectBlendOut_OnEnter(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::DirectBlendOut_OnUpdate(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{ 

	animAssertf(m_pMatchedItem, "DirectBlendOut, but no blend out item found!");
	animAssertf(m_pMatchedItem->m_type==NMBT_DIRECTBLENDOUT, "direct blend out, but blend out item is not a direct blend out item!");

	DoDirectBlendOut();

	DecideOnNextAction(GetNextItem());

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::StreamBlendOutClip_Clone_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Assertf(m_pointCloudPoseSets.GetNumBlendOutSets() != 0, "ERROR : CTaskGetUp::StreamBlendOutClip_Clone_OnUpdate : Cannot be streaming - no streaming requests?!");

	u32 numSets = m_pointCloudPoseSets.GetNumBlendOutSets();
	Assert(numSets < ms_iMaxCombatGetupHelpers);

	for(u32 i = 0; i < numSets; ++i)
	{
		eNmBlendOutSet set = m_pointCloudPoseSets.GetBlendOutSet(i);
		Assert(set != NMBS_INVALID);

		m_cloneSetRequestHelpers[i].Request(set);

		if(!m_cloneSetRequestHelpers[i].IsLoaded())
		{
			// need to keep the current pose whilst we wait for a valid clip matrix
			if (pPed->GetMovePed().GetState()==CMovePed::kStateAnimated)
			{
				pPed->SwitchToStaticFrame(INSTANT_BLEND_DURATION);
			}

			return FSM_Continue;
		}
	}

	TaskSetState(State_StreamBlendOutClipComplete_Clone);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::Crawl_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_forcedSet = NMBS_INVALID;

	m_vCrawlStartPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + pPed->GetBoundOffset();

	// Release our network player so that if the crawl task quits and the get up task needs to complete the get up we'll properly restart our network player
	if (m_networkHelper.IsNetworkActive())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}

	if (m_ProbeHelper == NULL)
	{
		float fBoundOffset = FLT_MAX;
		if (pPed->GetCapsuleInfo()->GetBipedCapsuleInfo())
		{
			float fCapsuleRadiusDiff = pPed->GetCapsuleInfo()->GetHalfWidth() - (pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);
			fBoundOffset = pPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetCapsuleZOffset() - (fCapsuleRadiusDiff * 0.5f);
		}

		m_ProbeHelper = rage_new CGetupProbeHelper(fBoundOffset + pPed->GetCapsuleInfo()->GetGroundToRootOffset());
		int iIncludeFlags = ArchetypeFlags::GTA_PED_INCLUDE_TYPES;
		// Don't include the following types:
		iIncludeFlags &= ~(ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PICKUP_TYPE);
		m_ProbeHelper->SetIncludeFlags(iIncludeFlags);

		const int iMaxNumEntitiesToExclude = 2;
		int iNumEntitiesToExclude = 0;
		const CEntity* ppExcludeEntities[iMaxNumEntitiesToExclude];
		if (m_pExclusionEntity != NULL)
		{
			ppExcludeEntities[iNumEntitiesToExclude++] = m_pExclusionEntity;
		}

		CEntity* pEntityException = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if (pEntityException != NULL)
		{
			ppExcludeEntities[iNumEntitiesToExclude++] = pEntityException;
		}

		if (iNumEntitiesToExclude > 0)
		{
			m_ProbeHelper->SetExcludeEntities(ppExcludeEntities, iNumEntitiesToExclude);
		}
		m_ProbeHelper->SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
		m_ProbeHelper->SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
		m_ProbeHelper->SetIsDirected(true);
		m_ProbeHelper->SetCapsule(m_vCrawlStartPosition, VEC3_ZERO, pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);

		// We now do line of sight tests to each safe position found so no need to limit our search to only interiors if inside or only exteriors if outside...
		u32 iFlags = CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects | CNavmeshClosestPositionHelper::Flag_ConsiderOnlyNonIsolated | 
			CNavmeshClosestPositionHelper::Flag_ConsiderExterior | CNavmeshClosestPositionHelper::Flag_ConsiderInterior;

		if(!pPed->GetIsInWater())
		{
			iFlags |= CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand;
		}

		static float fSpacing = 1.0f;
		static float fZWeightingAbove = 2.0f;
		static float fZWeightingAtOrBelow = 0.75f;
		// Set z-weighting to be 0 so that we can do our own weighting based on the closest positions in the X/Y plane
		m_ProbeHelper->StartClosestPositionSearch(50.0f, iFlags, fZWeightingAbove, fZWeightingAtOrBelow, fSpacing);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::Crawl_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskCrawl* pCrawlTask = static_cast<CTaskCrawl*>(FindSubTaskOfType(CTaskTypes::TASK_CRAWL));
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ResetMovementStaticCounter, true);

	if (m_ProbeHelper != NULL && m_ProbeHelper->GetAllResultsReady())
	{
		Vector3 vSafePosition;
		float fClosestScore = FLT_MAX;
		float fClosestDistZ = FLT_MAX;
		for (int i = 0; i < m_ProbeHelper->GetNumResult(); i++)
		{
#if DEBUG_DRAW && __DEV
			if(ms_bShowSafePositionChecks)
			{
				Vector3 vEndPosition(m_ProbeHelper->GetEndPosition(i));
				vEndPosition.z = m_vCrawlStartPosition.z;
				CPhysics::ms_debugDrawStore.AddCapsule(VECTOR3_TO_VEC3V(m_vCrawlStartPosition), VECTOR3_TO_VEC3V(vEndPosition), pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult,
					m_ProbeHelper->GetResult(i).GetHitDetected() ? Color_maroon : Color_DarkGreen, 10000, 0, false);
			}
#endif // DEBUG_DRAW && __DEV

			Vector3 vDiff(m_ProbeHelper->GetEndPosition(i) - m_ProbeHelper->GetStart());
			float fDist = vDiff.Mag2() / square(sfMinCrawlDistanceBeforeSafeCheck * 2.0f);

			float fDotProduct = 0.0f;
			if (fDist > SMALL_FLOAT)
			{
				vDiff.NormalizeSafe();
				fDotProduct = 1.0f - Abs(vDiff.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward())));
			}

			static bank_float s_fDistanceWeight = 0.5f;
			static bank_float s_fAngleWeight = 0.5f;
			float fScore = fDist * s_fDistanceWeight + fDotProduct * s_fAngleWeight;

			// Try to prefer positions that have line-of-sight and are lower than the ped (i.e. positions on the floor below us rather than a floor above us)
			// If this position is closer and is below the ped or our current closest position is above the ped
			// Or if this position is below the ped and our current closest position is above the ped
			// Then we've found a new closest position
			if (!m_ProbeHelper->GetResult(i).GetHitDetected() && ((fScore < fClosestScore && (vDiff.z <= 0.0f || fClosestDistZ > 0.0f)) || (vDiff.z <= 0.0f && fClosestDistZ > 0.0f)))
			{
				vSafePosition = m_ProbeHelper->GetEndPosition(i);
				fClosestScore = fScore;
				fClosestDistZ = vDiff.z;
			}
		}

		if (fClosestScore < FLT_MAX)
		{
#if DEBUG_DRAW && __DEV
			if(ms_bShowSafePositionChecks)
			{
				CPhysics::ms_debugDrawStore.AddSphere(VECTOR3_TO_VEC3V(vSafePosition), pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult,
					Color_DarkGreen, 10000);
			}
#endif // DEBUG_DRAW && __DEV

			// If we've already got a crawl sub-task then just update it - otherwise create a new one
			if (pCrawlTask != NULL)
			{
				pCrawlTask->SetSafePosition(vSafePosition);
			}
			else
			{
				pCrawlTask = rage_new CTaskCrawl(vSafePosition);
				SetNewTask(pCrawlTask);
			}
		}

		// Once done with the probe helper reset it so that it can be used again
		delete m_ProbeHelper;
		m_ProbeHelper = NULL;
	}
	else
	{
		static dev_float s_fTimeoutThreshold = 10.0f;
		if (!m_Flags.IsFlagSet(aiTaskFlags::SubTaskFinished) && pCrawlTask != NULL && pCrawlTask->GetTimeRunning() < s_fTimeoutThreshold)
		{
			Vector3 vSafePosition = pCrawlTask->GetSafePosition();

			// If we've moved far enough from our start position or close enough to the safe position then re-evaluate whether we can get up
			Vector3 vCurrentPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vCurrentPosition.z = m_vCrawlStartPosition.z;

			int nTestTypes = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
			static const int nNumTest = 2;
			static const eAnimBoneTag eBoneTag[nNumTest][2] = { { BONETAG_R_CLAVICLE, BONETAG_L_CLAVICLE }, { BONETAG_R_THIGH, BONETAG_L_THIGH } };

			WorldProbe::CShapeTestFixedResults<nNumTest> shapeResult;

			// Fill in the descriptor for the batch test.
			WorldProbe::CShapeTestBatchDesc batchDesc;
			batchDesc.SetOptions(0);
			batchDesc.SetIncludeFlags(nTestTypes);
			batchDesc.SetExcludeEntity(NULL);
			ALLOC_AND_SET_CAPSULE_DESCRIPTORS(batchDesc, nNumTest);

			WorldProbe::CShapeTestCapsuleDesc shapeDesc;
			shapeDesc.SetResultsStructure(&shapeResult);
			shapeDesc.SetMaxNumResultsToUse(1);
			shapeDesc.SetIncludeFlags(nTestTypes);
			shapeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
			shapeDesc.SetExcludeEntity(NULL);
			shapeDesc.SetIsDirected(true);

			static dev_float sfCrawlSupportHeight = 0.25f;

			Vector3 vRight;
			Vector3 vLeft;
			Vector3 vStart;
			Vector3 vEnd;

			for (int i = 0; i < nNumTest; i++)
			{
				pPed->GetBonePosition(vRight, eBoneTag[i][0]);
				pPed->GetBonePosition(vLeft, eBoneTag[i][1]);
				vStart.Average(vRight, vLeft);
				vEnd = vStart;
				vEnd.z -= sfCrawlSupportHeight;
				shapeDesc.SetCapsule(vStart, vEnd, sfGroundCheckCapsuleRadius);
				shapeDesc.SetFirstFreeResultOffset(batchDesc.GetNumShapeTests());
				batchDesc.AddCapsuleTest(shapeDesc);

#if DEBUG_DRAW && __DEV
				if (ms_bShowSafePositionChecks)
				{
					CPhysics::ms_debugDrawStore.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), sfGroundCheckCapsuleRadius, Color_yellow, sm_Tunables.GetStartClipWaitTime(pPed));
				}
#endif // DEBUG_DRAW && __DEV
			}

			float fMinCrawlDistanceBeforeSafeCheck = sfMinCrawlDistanceBeforeSafeCheck;

			static dev_float sfMinCrawlDistanceBeforeSafeCheckNoSupportMultiplier = 0.25f;
			// If there's nothing supporting us at the thigh or clavicle position then start looking for a safe position to start getting up sooner...
			if (!WorldProbe::GetShapeTestManager()->SubmitTest(batchDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST) || !shapeResult[0].GetHitDetected() || !shapeResult[1].GetHitDetected())
			{
				fMinCrawlDistanceBeforeSafeCheck *= sfMinCrawlDistanceBeforeSafeCheckNoSupportMultiplier;
			}

			// Don't check how close we are to the safe position while in the middle of processing new probe results as our previous safe position is not very valid at that point
			if (m_vCrawlStartPosition.Dist2(vCurrentPosition) > square(fMinCrawlDistanceBeforeSafeCheck) || (m_ProbeHelper == NULL && vSafePosition.Dist2(vCurrentPosition) < square(sfMinCrawlDistanceBeforeSafeCheck * 0.5f)))
			{
				m_nStuckCounter = 0;
				TaskSetState(State_ChooseGetupSet);
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			}
		}
		else
		{
			m_nStuckCounter = sm_Tunables.m_StuckWaitTime;
			TaskSetState(State_ChooseGetupSet);
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::Crawl_OnExit(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Ensure the probe helper is removed when leaving the state (we'll generate a new one if we ever come back into the crawl state!)
	if (m_ProbeHelper != NULL)
	{
		delete m_ProbeHelper;
		m_ProbeHelper = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::DropDown_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Release our network player so that if the drop-down task quits and the get up task needs to complete the get up we'll properly restart our network player
	if (m_networkHelper.IsNetworkActive())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}

	// Ensure the root slope solver is blended out
	CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pSolver != NULL)
	{
		pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
	}

	if (pPed->GetAnimatedInst() != NULL && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
	{
		CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex(), CPhysics::GetLevel()->GetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex()) & (~ArchetypeFlags::GTA_DEEP_SURFACE_TYPE));
	}

	static bank_float sfBlendInTime = SLOW_BLEND_DURATION;

	CDropDown dropDown = pPed->GetPedIntelligence()->GetDropDownDetector().GetDetectedDropDown();

	Vector3 vLeftHandPosition;
	Vector3 vRightHandPosition;
	pPed->GetBonePosition(vLeftHandPosition, BONETAG_L_HAND);
	pPed->GetBonePosition(vRightHandPosition, BONETAG_R_HAND);

	// Use the lower hand to determine the type of drop...
	dropDown.m_bLeftHandDrop = vLeftHandPosition.z < vRightHandPosition.z;

    CTaskFSMClone *pDropDownSubTask = 0;

    if(pPed->IsNetworkClone())
    {
        pDropDownSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_DROP_DOWN);
    }
    else
    {
        pDropDownSubTask = rage_new CTaskDropDown(dropDown, sfBlendInTime);
    }

	SetNewTask(pDropDownSubTask);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::DropDown_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding))
	{
		static bank_float sfHeadingChangeRate = DtoR * 360.0f * 0.15f;
		pPed->SetHeadingChangeRate(sfHeadingChangeRate);
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_DROP_DOWN))
	{
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::DropDown_OnExit(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	pPed->RestoreHeadingChangeRate();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::Finish_OnUpdate()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed(); 

	if (pPed)
	{
		// Clones cannot be allowed to finish before owners when getting up 
		// as they will be left hanging about which task to switch to.
		// We stall the ped until we taskGetUp is no longer part of the qi.
		if(pPed->IsNetworkClone())
		{
			if(IsTaskRunningOverNetwork_Clone(CTaskTypes::TASK_GET_UP))
			{
				return FSM_Continue;
			}
		}

		pPed->SetClothPinRadiusScale(1.0f);

		// message the controlling a.i. to tell it which blend out set we used
		// This gives it an opportunity to resume in the correct state.
		NotifyGetUpComplete(m_matchedSet, m_pMatchedItem, static_cast<CTaskMove*>(m_pMoveTask.Get()), pPed);

		if (m_matchedSet == NMBS_WRITHE)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);

			// Make sure we don't timeslice the ai / animation this frame.
			pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

			if (!pWritheClipSetRequestHelper)
				pWritheClipSetRequestHelper = CWritheClipSetRequestHelperPool::GetNextFreeHelper();

			if (pWritheClipSetRequestHelper && !CTaskWrithe::StreamReqResourcesIn(pWritheClipSetRequestHelper, pPed, CTaskWrithe::NEXT_STATE_WRITHE))
				return FSM_Continue;

			if(!pPed->IsNetworkClone())
			{	
				//Disable blood pools if this was caused by an electric weapon.				
				if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DAMAGE_ELECTRIC))
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableBloodPoolCreation, true);
				}

				CEventWrithe event(CWeaponTarget(m_Target), true);
				if (!pPed->GetPedIntelligence()->HasEventOfType(&event) && !pPed->GetPedIntelligence()->GetTaskWrithe())
					pPed->GetPedIntelligence()->AddEvent(event);
			}

			return FSM_Quit;
		}
	}

	return FSM_Quit;
}

//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::UpdateBlendDirectionSignal()
//////////////////////////////////////////////////////////////////////////
{
	taskAssert(m_pMatchedItem && m_pMatchedItem->IsPoseItem());
	const CNmBlendOutPoseItem* pPoseItem = static_cast<const CNmBlendOutPoseItem*>(m_pMatchedItem);

	float interpRate = pPoseItem->m_fullBlendHeadingInterpRate + RampValueSafe(GetTimeInState(), 0.0f, m_fBlendDuration, pPoseItem->m_zeroBlendHeadingInterpRate, 0.0f);

	float current = m_fGetupBlendCurrentHeading;
	float target = CalcDesiredDirection();

	target = current + SubtractAngleShorter(target, current);

	// don't allow wrapping
	// TODO - add some extra heading change here to deal with missing turn?
	if (target>PI)
	{
		target=PI;
	}
	else if (target<-PI)
	{
		target=-PI;
	}

	// don't overshoot
	if (target<current)
	{
		current-=(interpRate*fwTimer::GetTimeStep());
		current = Max(current, target);
	}
	else
	{
		current+=(interpRate*fwTimer::GetTimeStep());
		current = Min(current, target);
	}

	m_fGetupBlendCurrentHeading = current;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::AimingFromGround_OnEnter(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	taskAssert(m_pMatchedItem);
	taskAssert(m_pMatchedItem->m_type==NMBT_AIMFROMGROUND);

	if (m_networkHelper.IsNetworkActive())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}

	ms_LastAimFromGroundStartTime = fwTimer::GetTimeInMilliseconds();
	ms_NumPedsAimingFromGround++;

	//start the new subtask
	CTaskAimFromGround* pTask = rage_new CTaskAimFromGround(m_pMatchedItem->GetClipSet().GetHash(), 0, CWeaponTarget(m_Target));
	SetNewTask(pTask);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskGetUp::AimingFromGround_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	taskAssert(m_pMatchedItem);
	taskAssert(m_pMatchedItem->m_type==NMBT_AIMFROMGROUND);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pSolver != NULL)
		{
			pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
		}

		if (pPed->GetAnimatedInst() != NULL && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
		{
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex(), CPhysics::GetLevel()->GetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex()) & (~ArchetypeFlags::GTA_DEEP_SURFACE_TYPE));
		}

		DecideOnNextAction(GetNextItem());
	}

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::AimingFromGround_OnExit()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	ms_NumPedsAimingFromGround--;
}

//////////////////////////////////////////////////////////////////////////
float CTaskGetUp::CalcDesiredDirection()
//////////////////////////////////////////////////////////////////////////
{
	CPed * pPed = GetPed();
	float fDesiredHeading = 0.0f;

	if (pPed->IsNetworkClone() && sm_bCloneGetupFix)
	{
		fDesiredHeading = static_cast<CNetObjPed*>(pPed->GetNetworkObject())->GetDesiredHeading();
	}
	else
	{
		fDesiredHeading = pPed->GetDesiredHeading();
	}
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(m_fGetupBlendReferenceHeading);
	fDesiredHeading = fwAngle::LimitRadianAngleSafe(fDesiredHeading);
	return SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
}

//////////////////////////////////////////////////////////////////////////
float CTaskGetUp::ConvertRadianToSignal(float angle, float fMaxAngle /*= PI */)
//////////////////////////////////////////////////////////////////////////
{
	angle = Clamp( angle/fMaxAngle, -1.0f, 1.0f);

	return ((-angle * 0.5f) + 0.5f);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ShouldDoDirectionalGetup()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_pMatchedItem && m_pMatchedItem->m_type == NMBT_DIRECTIONALBLEND)
		return true;
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ShouldBlendOutDirectly()
//////////////////////////////////////////////////////////////////////////
{
	// Do a direct blend out of there's no matched item (we've matched to the ped's current idle anim)
	// or if the blend out item metadata tells us to.
	return !m_pMatchedItem || m_pMatchedItem->m_type==NMBT_DIRECTBLENDOUT;
}

//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::StartMoveNetwork()
//////////////////////////////////////////////////////////////////////////
{
	taskAssertf(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM), "Blend from nm network is not streamed in!");

	CPed* pPed = GetPed();
	m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM);

	if (m_bRunningAsMotionTask)
	{
#if ENABLE_DRUNK
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (taskVerifyf(pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED, "Missing or invalid motion task for TaskBlendFromNM: %s", pTask==NULL ? "(NULL)": pTask->GetName() ))
		{
			CTaskMotionPed* pPedTask = static_cast<CTaskMotionPed*>(pTask);
			pPedTask->InsertDrunkNetwork(m_networkHelper);
		}
#endif // ENABLE_DRUNK
	}
	else
	{
		pPed->GetMovePed().SetTaskNetwork(m_networkHelper, m_bInstantBlendInNetwork || m_bInsertRagdollFrame ? 0.0f : m_fBlendDuration);
		m_bInstantBlendInNetwork = false;
	}

	if (m_bInsertRagdollFrame)
	{
		//pass in the ragdoll frame and blend duration
		crFrame* pFrame = NULL;
		if (pPed->GetAnimDirector())
		{
			fwAnimDirectorComponentRagDoll* componentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();
			if(componentRagDoll)
			{
				// Actually make the switch to the animated instance.
				pFrame = componentRagDoll->GetFrame();
			}
		}
		m_networkHelper.SetFrame(pFrame, ms_RagdollFrame);
		m_networkHelper.SetFloat(ms_RagdollFrameBlendDuration, m_fBlendDuration);
		m_bInsertRagdollFrame = false;
	}

	if (m_bForceUpperBodyAim)
	{
		m_UpperBodyAimPitchHelper.BlendInUpperBodyAim(m_networkHelper, *pPed, 0.0f);
	}
}

void CTaskGetUp::GetDefaultArmsDuration(const crClip* pClip)
{
	static const atHashString s_BlendInDefaultArms("BlendInDefaultArms",0x3FF41EBE);
	const crTag* pTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, s_BlendInDefaultArms, 0.0f);
	if (!m_bBlendedInDefaultArms && pTag != NULL && taskVerifyf(pTag->GetEnd() >= pTag->GetStart(), "CTaskGetUp::GetDefaultArmsDuration: Start blend in phase for default arms was greater than end blend in phase"))
	{
		m_defaultArmsBlendDuration = pClip->ConvertPhaseToTime(pTag->GetEnd() - pTag->GetStart());
	}
}

bool CTaskGetUp::GetTreatAsPlayer(const CPed& ped) const
{
	bool bTreatAsPlayer = ped.IsAPlayerPed();

	if(ped.IsNetworkClone() && m_bForceTreatAsPlayer)
	{
		bTreatAsPlayer = true;
	}
	else
	{
		// Treat peds in the players group and mission chars friendly to the player as players - makes sure they stand up.
		if(ped.GetPedsGroup() && ped.GetPedsGroup()->GetGroupMembership()->GetLeader() &&
			ped.GetPedsGroup()->GetGroupMembership()->GetLeader()->IsPlayer())
		{
			bTreatAsPlayer = true;
		}
		else if(ped.PopTypeIsMission()&&CGameWorld::FindLocalPlayer()&&ped.GetPedIntelligence()->IsFriendlyWith(*CGameWorld::FindLocalPlayer()))
		{
			bTreatAsPlayer = true;
		}

	}

	return bTreatAsPlayer;
}

int CTaskGetUp::ExcludeUnderwaterBikes(CPed* pPed, const CEntity** const ppEntitiesToExclude, int iMaxEntitiesToExclude)
{
	// Search for any underwater bikes nearby to the ped to exclude from getup probes
	static dev_float s_MaxDistanceToExcludeSquared = square(5.0f);

	int iNumEntitiesExcluded = 0;

	CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pVehiclePool->GetSize();
	while(i-- && iNumEntitiesExcluded < iMaxEntitiesToExclude)
	{
		pVehicle = pVehiclePool->GetSlot(i);
		if(pVehicle != NULL && DistSquared(pPed->GetTransform().GetPosition(), pVehicle->GetTransform().GetPosition()).Getf() < s_MaxDistanceToExcludeSquared && 
			pVehicle->InheritsFromBike() && static_cast<CBike*>(pVehicle)->IsUnderwater())
		{
			ppEntitiesToExclude[iNumEntitiesExcluded++] = pVehicle;
		}
	}

	return iNumEntitiesExcluded;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::FindSafePosition(CPed* pPed, const Vector3& vNearThisPosition, Vector3& vOut)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	const float fNavMeshSearchRadius = 50.0f;
	u32 iFlags = GetClosestPos_ClearOfObjects | GetClosestPos_OnlyNonIsolated | GetClosestPos_PreferSameHeight | GetClosestPos_ExtraThorough | GetClosestPos_UseFloodFill;

	// If we're not in an interior, exclude interiors from the position search.
	if(!pPed->GetPortalTracker()->IsInsideInterior() && !pPed->GetPortalTracker()->IsInsideTunnel())
	{
		iFlags |= GetClosestPos_NotInteriors;

		// If we aren't already very close to navmesh flagged as sheltered then exclude shleterd navmesh from the position search
		if (CPathServer::GetClosestPositionForPed(vNearThisPosition, vOut, 2.0f, iFlags | GetClosestPos_OnlySheltered) == ENoPositionFound)
		{
			iFlags |= GetClosestPos_NotSheltered;
		}
	}
	// If we're in an interior, exclude non-interiors from the position search.
	else
	{
		iFlags |= GetClosestPos_OnlyInteriors;
	}

	if(!pPed->GetIsInWater())
	{
		iFlags |= GetClosestPos_NotWater;
	}

	bool bFound = true;

	if (CPathServer::GetClosestPositionForPed(vNearThisPosition, vOut, fNavMeshSearchRadius, iFlags) == ENoPositionFound)
	{
		bFound = CPathServer::GetClosestClearCarNodeForPed(vNearThisPosition, vOut, fNavMeshSearchRadius);
	}

	return bFound;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::IsValidBlockingEntity(CPed* pPed, CEntity* pEntity, bool bCapsuleTest) const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	bool bValidBlockingEntity = false;
	if (pEntity != NULL)
	{
		if (pEntity->GetIsFixedFlagSet())
		{
			bValidBlockingEntity = true;
		}
		else if (pEntity->GetIsPhysical())
		{
			if (pEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pEntity);
				// Attached doors are always considered blocking entities - regardless of mass
				if (pObject->IsADoor() && pObject->GetIsAttached())
				{
					bValidBlockingEntity = true;
				}
				else if (pObject->GetMass() > (pPed->GetMass() * 3.0f))
				{
					bValidBlockingEntity = true;
				}
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
				// If this is a bike and if the bike has a driver, is on its side-stand or is getting picked up then it is considered a valid blocking entity - regardless of mass
				if (pVehicle->InheritsFromBike() && (pVehicle->GetDriver() != NULL || static_cast<CBike*>(pVehicle)->m_nBikeFlags.bOnSideStand || static_cast<CBike*>(pVehicle)->m_nBikeFlags.bGettingPickedUp || bCapsuleTest))
				{
					bValidBlockingEntity = true;
				}
				else if (pVehicle->GetMass() > (pPed->GetMass() * 3.0f))
				{
					bValidBlockingEntity = true;
				}
			}
		}
	}

	return bValidBlockingEntity;
}

bool CTaskGetUp::CanCrawl() const
{
	bool bCanCrawl = false;
	const CPed* pPed = GetPed();
	if (!NetworkInterface::IsGameInProgress() && pPed != NULL && pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsBiped() &&
		!pPed->GetIsSwimming() && m_forcedSet != NMBS_CRAWL_GETUPS && m_pBumpedByEntity != NULL)
	{
		bCanCrawl = true;

		if (m_pBumpedByEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pBumpedByEntity.Get());
			// Don't let the ped crawl out from underneath a vehicle if we're blocked by a door
			eHierarchyId eDoorId = CVehicle::GetDoorHierachyIdFromImpact(*pVehicle, m_iBumpedByComponent);
			if (eDoorId != VEH_INVALID_ID)
			{
				bCanCrawl = false;
			}
			else
			{
				// Don't let the ped crawl out from underneath a vehicle if one of the wheels is in contact
				s32 iNumWheels = pVehicle->GetNumWheels();
				for (s32 i = 0; i < iNumWheels; ++i)
				{
					const CWheel* pWheel = pVehicle->GetWheel(i);
					if (pWheel != NULL && (pWheel->GetHitPhysical() == pPed || pWheel->GetPrevHitPhysical() == pPed))
					{
						bCanCrawl = false;
					}
				}
			}
		}
	}

	return bCanCrawl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ShouldBlockingEntityKillPlayer(const CEntity* pEnt)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (NetworkInterface::IsGameInProgress() && pEnt && pEnt->GetIsTypeVehicle())
	{
		const CVehicle* pVeh = static_cast<const CVehicle*>(pEnt);

		// B*2982720 - this ped can be AI if TreatAsPlayer is true
		bool bFriendlyWith = false;
		const CPed* pDriver = pVeh->GetDriver() ? pVeh->GetDriver() : pVeh->GetLastDriver();
		if(pDriver && GetPed()->PopTypeIsMission() && GetPed()->GetPedIntelligence()->IsFriendlyWith(*pDriver))
		{
			bFriendlyWith = true;
		}

		return !bFriendlyWith && pVeh->IsTank();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::FixupClipMatrix(CPed* pPed, Matrix34* pAnimMatrix, CNmBlendOutItem* pBlendOutItem, eNmBlendOutSet eBlendOutSet, float fBlendOutTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If we didn't find any anims to match, we're going to have to workout an anim matrix for ourselves.
	bool bClearToGetup = true;
	Matrix34 rootMatrix;
	pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);

	//Orient the qpeds to the ground
	if (pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped())
	{	
		Vector3 vNormal;
		bool bReturn;
		float fGroundZ =  WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, pAnimMatrix->d, &bReturn, &vNormal);
		if (bReturn && Abs(fGroundZ - pAnimMatrix->d.z) < 4.0f)	//Teleported near ground, align
		{
			Vector3 vSlope;
			vSlope.Cross(vNormal, pAnimMatrix->a);
			//TMS: Going to ignore really steep stuff!
			if (vSlope.Mag2() > 0.1f*0.1f)
			{
				pAnimMatrix->c.Cross(pAnimMatrix->a, vSlope);
				pAnimMatrix->Normalize();
			}
		}
	}

	bool bTreatAsPlayer = GetTreatAsPlayer(*pPed);
	
	// Try to find an accurate ground position for the capsule.
	// Check for ground against GTA_DEEP_SURFACE_TYPE as we want to try and getup on top of deep surfaces.  This doesn't always look great
	// but the alternative is potentially having the first-person-camera underneath the ground which is not acceptable
	u32 nTestIncludeTypes = 0
		|ArchetypeFlags::GTA_VEHICLE_TYPE
		|ArchetypeFlags::GTA_OBJECT_TYPE
		|ArchetypeFlags::GTA_ALL_MAP_TYPES
		|(u32)ArchetypeFlags::GTA_DEEP_SURFACE_TYPE;

	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();
	const float kfGroundToRootOffset = pCapsuleInfo->GetGroundToRootOffset();
	CEntity* pBlockingEntity = NULL;
	s32 iBlockingComponent = -1;
	WorldProbe::CShapeTestFixedResults<> shapeTestResult;
	WorldProbe::CShapeTestCapsuleDesc shapeTestDesc;

	// When searching for ground it often makes more sense to use the average of the left and right thigh bone positions.  
	// If the ped is leaning over something and we're doing a standing recovery (not allowing slope fixup) then often probing down 
	// from the root bone will hit whatever we're leaning over so the ped will end up getting up on-top of whatever we're leaning over
	Vector3 vStart;
	Vector3 vEnd;
	CNmBlendOutSet* pBlendOutSet = CNmBlendOutSetManager::GetBlendOutSet(eBlendOutSet);
	bool bAllowRootSlopeFixup = pBlendOutSet == NULL || pBlendOutSet->IsFlagSet(CNmBlendOutSet::BOSF_AllowRootSlopeFixup);
	if (!bAllowRootSlopeFixup)
	{
		Vector3 vRightThigh;
		Vector3 vLeftThigh;
		pPed->GetBonePosition(vRightThigh, BONETAG_L_THIGH);
		pPed->GetBonePosition(vLeftThigh, BONETAG_L_THIGH);
		vStart.Average(vRightThigh, vLeftThigh);
		vStart.z = pAnimMatrix->d.z;
	}
	else
	{
		vStart = pAnimMatrix->d;
	}
	vEnd = vStart;
	vEnd.z -= 1.2f;

	shapeTestDesc.SetCapsule(vStart, vEnd, pCapsuleInfo->GetHalfWidth());
	shapeTestDesc.SetIsDirected(true);
	shapeTestDesc.SetDoInitialSphereCheck(true);

#if DEBUG_DRAW && __DEV
	if(ms_bShowSafePositionChecks)
	{
		CPhysics::ms_debugDrawStore.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), pCapsuleInfo->GetHalfWidth(), Color_yellow, sm_Tunables.GetStartClipWaitTime(pPed), 0, false);
		CPhysics::ms_debugDrawStore.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), pCapsuleInfo->GetProbeRadius(), Color_white, sm_Tunables.GetStartClipWaitTime(pPed), 0, true);
	}
#endif // DEBUG_DRAW && __DEV

	shapeTestDesc.SetResultsStructure(&shapeTestResult);
	shapeTestDesc.SetIncludeFlags(nTestIncludeTypes);
	shapeTestDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
	const int iMaxNumEntitiesToExclude = 4;
	const CEntity* ppExcludeEntities[iMaxNumEntitiesToExclude];
	int iNumEntitiesToExclude = ExcludeUnderwaterBikes(pPed, ppExcludeEntities, iMaxNumEntitiesToExclude - 1);
	ppExcludeEntities[iNumEntitiesToExclude++] = pPed;
	shapeTestDesc.SetExcludeEntities(ppExcludeEntities, iNumEntitiesToExclude);

	nmDebugf2("[%u] CTaskGetUp::FixupClipMatrix:%s(%p) Doing capsule test from (%.4f %.4f %.4f) to (%.4f %.4f %.4f)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed,
		vStart.x, vStart.y, vStart.z, vEnd.x, vEnd.y, vEnd.z);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		// Go through probe results to find the first valid blocking entity
		for (int i = shapeTestResult.GetSize() - 1; i >= 0; i--)
		{
			if (shapeTestResult[i].GetHitDetected())
			{
				Vector3 vHitPosition = shapeTestResult[i].GetHitPosition();

				float fDistSquaredXY = Vector3(vHitPosition - vStart).XYMag2();

				// Cache off the blocking entity
				pBlockingEntity = CPhysics::GetEntityFromInst(shapeTestResult[i].GetHitInst());
				iBlockingComponent = shapeTestResult[i].GetHitComponent();
				bool bValidBlockingEntity = IsValidBlockingEntity(pPed, pBlockingEntity);

				// If we're not being blocked by a valid entity (i.e. a very light CObject or CVehicle) and our hit point is equal to or within our ground probe radius...
				// Or our hit point height is less than or equal to our root bone height...
				if ((!bValidBlockingEntity && fDistSquaredXY <= square(pCapsuleInfo->GetProbeRadius())) || (vHitPosition.z <= rootMatrix.d.z && (bAllowRootSlopeFixup || fDistSquaredXY <= square(pCapsuleInfo->GetProbeRadius()))))
				{
					nmDebugf2("[%u] CTaskGetUp::FixupClipMatrix:%s(%p) Capsule test hit (%.4f %.4f %.4f)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, 
						vHitPosition.x, vHitPosition.y, vHitPosition.z);

					// Clear the blocking entity if we found a valid position...
					pBlockingEntity = NULL;
					iBlockingComponent = -1;

#if DEBUG_DRAW && __DEV
					if(ms_bShowSafePositionChecks)
					{
						CPhysics::ms_debugDrawStore.AddSphere(shapeTestResult[i].GetHitPositionV(), 0.05f, Color_orange, sm_Tunables.GetStartClipWaitTime(pPed));
					}
#endif // DEBUG_DRAW && __DEV

					pAnimMatrix->d.z = vHitPosition.z + kfGroundToRootOffset;

					// If the hit position is sufficiently far in the XY direction from the clip matrix (but not too far) then adjust it to match the hit position
					// If we don't do this then it could be that we're on a ledge and the ground probe might miss colliding with the ledge and we'd fall as we were getting up
					if (fDistSquaredXY > square(pCapsuleInfo->GetProbeRadius() * 0.5f) && fDistSquaredXY < square(pCapsuleInfo->GetHalfWidth()))
					{
						pAnimMatrix->d.x = vHitPosition.x;
						pAnimMatrix->d.y = vHitPosition.y;

						nmDebugf2("[%u] CTaskGetUp::FixupClipMatrix:%s(%p) Hit position is %.4f away from clip matrix in XY plane - moving clip matrix to match hit point", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, Sqrtf(fDistSquaredXY));
					}
				}
				else if (bValidBlockingEntity)
				{
					break;
				}
			}
		}

		if (((rootMatrix.d.z - pAnimMatrix->d.z - kfGroundToRootOffset) > kfGroundToRootOffset * 0.6f) && rootMatrix.c.z > 0.5f && !pPed->GetIsInWater())
		{
			//*Might* be safe to return to anim now since we're already standing up
			//But, our capsule could still end up deeply penetrating and push us through the world
			if(!bTreatAsPlayer || !CPedGeometryAnalyser::TestPedCapsule(pPed, pAnimMatrix, (const CEntity**)&pPed, 1, EXCLUDE_ENTITY_OPTIONS_NONE, nTestIncludeTypes, ArchetypeFlags::GTA_PED_TYPE, ArchetypeFlags::GTA_FOLIAGE_TYPE, 
				!IsValidBlockingEntity(pPed, pBlockingEntity, true) ? &pBlockingEntity : NULL, &iBlockingComponent))
			{
				return true;
			}
		}
	}

	float fBoundRadiusMult = 1.0f;
	float fBoundHeading = 0.0f;
	float fBoundPitch = 0.0f;
	Vector3 vBoundOffset(Vector3::ZeroType);
	if (m_forcedSet == NMBS_CRAWL_GETUPS)
	{
		fBoundRadiusMult = CTaskFallOver::ms_fCapsuleRadiusMult;
		Vector3 vNeckPosition;
		if (pPed->GetBonePosition(vNeckPosition, BONETAG_NECK))
		{
			Vector3 vPelvisToHead = vNeckPosition - rootMatrix.d;
			vPelvisToHead.Normalize();
			fBoundHeading = vPelvisToHead.AngleZ(pAnimMatrix->b);
		}
		fBoundPitch = -HALF_PI;
	
		// Use the root slope fixup solver to determine the amount to pitch the bound
		ActivateSlopeFixup(pPed, 0.0f);
		CRootSlopeFixupIkSolver* pRootSlopeFixupIkSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pRootSlopeFixupIkSolver != NULL)
		{
			ScalarV fPitch;
			ScalarV fRoll;
			if (pRootSlopeFixupIkSolver->CalculatePitchAndRollFromGroundNormal(fPitch, fRoll, RCC_MAT34V(*pAnimMatrix).GetMat33ConstRef()))
			{
				fBoundPitch += fPitch.Getf();
			}
		}

		if (pPed->GetCapsuleInfo()->GetBipedCapsuleInfo())
		{
			float fCapsuleRadiusDiff = pPed->GetCapsuleInfo()->GetHalfWidth() - (pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult);
			vBoundOffset.z = pPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetCapsuleZOffset() + (fCapsuleRadiusDiff * 0.5f);
		}
	}

	// Make sure that we don't make the ped stand up inside something.
	nTestIncludeTypes |= ArchetypeFlags::GTA_PED_TYPE;
	if(bClearToGetup && CPedGeometryAnalyser::TestPedCapsule(pPed, pAnimMatrix, (const CEntity**)&pPed, 1, EXCLUDE_ENTITY_OPTIONS_NONE, nTestIncludeTypes, ArchetypeFlags::GTA_PED_TYPE, ArchetypeFlags::GTA_FOLIAGE_TYPE,
		!IsValidBlockingEntity(pPed, pBlockingEntity, true) ? &pBlockingEntity : NULL, &iBlockingComponent, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset))
	{
		nmDebugf2("[%u] CTaskGetUp::FixupClipMatrix:%s(%p) Ground position blocked by %s(%p) component %d", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed,
			pBlockingEntity != NULL ? pBlockingEntity->GetModelName() : "none", pBlockingEntity, iBlockingComponent);

#if DEBUG_DRAW && __DEV
		if(ms_bShowSafePositionChecks)
		{
			DebugDrawPedCapsule(*pAnimMatrix, Color_red, sm_Tunables.GetStartClipWaitTime(pPed), 0, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset);
		}
#endif // DEBUG_DRAW && __DEV

		// Calculate the object-space matrix for the root bone using the get-up clip
		Vector3 newRootTranslation(Vector3::ZeroType);
		//fwAnimManager::GetLocalTranslation(&pPed->GetSkeletonData(), GetClip(), 0.0f, (u16)BONETAG_ROOT, newRootTranslation);
		// Clip often isn't streamed in yet at this point and rather than waiting for it to be streamed in we can just use the first point in the point cloud for the
		// matched pose - it should be identical to the object space root bone translation
		if (pBlendOutItem != NULL)
		{
			const crPoseMatcher::MatchSample* pMatchSample = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().FindSample(pBlendOutItem->GetClipSet(), pBlendOutItem->GetClip(), fBlendOutTime);
			if (pMatchSample != NULL)
			{
				newRootTranslation = VEC3V_TO_VECTOR3(pMatchSample->m_PointCloud->GetPoint(0));
			}
		}

		// Search around in a circle to find a safe position to place the ped.
		float fSearchRadius = pCapsuleInfo ? pCapsuleInfo->GetHalfWidth() : sfRagdollTransSearchRadius;

		if(bTreatAsPlayer)
			fSearchRadius = m_nStuckCounter > 0 ? fSearchRadius*=sfRagdollTransSearchRadiusPlayer2: fSearchRadius*=sfRagdollTransSearchRadiusPlayer;
		//else if(m_clipId==CLIP_TRANS_SEATED)
		//	fSearchRadius = m_nStuckCounter > 0 ? ms_fRagdollTransSearchRadiusSeated2 : ms_fRagdollTransSearchRadiusSeated;
		float fAngle = 0.0f;
		Matrix34 testMat = *pAnimMatrix;
		while(fAngle < TWO_PI)
		{
			testMat.d = pAnimMatrix->d + fSearchRadius * Vector3(-rage::Sinf(fAngle), rage::Cosf(fAngle), 0.0f);

			// Test a ped shaped capsule at the prospective position.
			if(!CPedGeometryAnalyser::TestPedCapsule(pPed, &testMat, (const CEntity**)&pPed, 1, EXCLUDE_ENTITY_OPTIONS_NONE, nTestIncludeTypes, ArchetypeFlags::GTA_PED_TYPE, ArchetypeFlags::GTA_FOLIAGE_TYPE, 
				!IsValidBlockingEntity(pPed, pBlockingEntity, true) ? &pBlockingEntity : NULL, &iBlockingComponent, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset))
			{
#if DEBUG_DRAW && __DEV
				if(ms_bShowSafePositionChecks)
				{
					DebugDrawPedCapsule(testMat, Color_green, sm_Tunables.GetStartClipWaitTime(pPed), 0, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset);
				}
#endif // DEBUG_DRAW && __DEV
				
				// Ensure that there is nothing blocking the line-of-sight from the current root bone position to the new root bone position
				Vector3 newRootPosition = newRootTranslation;
				testMat.Transform(newRootPosition);
				if(!TestLOS(rootMatrix.d, newRootPosition))
				{
#if DEBUG_DRAW && __DEV
					if(ms_bShowSafePositionChecks)
					{
						CPhysics::ms_debugDrawStore.AddLine(RCC_VEC3V(rootMatrix.d), RCC_VEC3V(newRootPosition), Color_green, sm_Tunables.GetStartClipWaitTime(pPed));
					}
#endif // DEBUG_DRAW && __DEV

					// Ensure that there is nothing blocking the line-of-sight from the new root bone position to the new mover capsule position
					if(!TestLOS(newRootPosition, testMat.d + vBoundOffset))
					{
#if DEBUG_DRAW && __DEV
						if(ms_bShowSafePositionChecks)
						{
							CPhysics::ms_debugDrawStore.AddLine(RCC_VEC3V(newRootPosition), RCC_VEC3V(testMat.d), Color_green, sm_Tunables.GetStartClipWaitTime(pPed));
						}
#endif // DEBUG_DRAW && __DEV
						break;
					}
#if DEBUG_DRAW && __DEV
					else
					{
						if(ms_bShowSafePositionChecks)
						{
							CPhysics::ms_debugDrawStore.AddLine(RCC_VEC3V(newRootPosition), RCC_VEC3V(testMat.d), Color_red, sm_Tunables.GetStartClipWaitTime(pPed));
						}
					}
#endif // DEBUG_DRAW && __DEV
				}
#if DEBUG_DRAW && __DEV
				else
				{
					if(ms_bShowSafePositionChecks)
					{
						CPhysics::ms_debugDrawStore.AddLine(RCC_VEC3V(rootMatrix.d), RCC_VEC3V(newRootPosition), Color_red, sm_Tunables.GetStartClipWaitTime(pPed));
					}
				}
#endif // DEBUG_DRAW && __DEV
			}
#if DEBUG_DRAW && __DEV
			else
			{
				nmDebugf2("[%u] CTaskGetUp::FixupClipMatrix:%s(%p) Ground position blocked by %s(%p) component %d", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed,
					pBlockingEntity != NULL ? pBlockingEntity->GetModelName() : "none", pBlockingEntity, iBlockingComponent);

				if(ms_bShowSafePositionChecks)
				{
					DebugDrawPedCapsule(testMat, Color_red, sm_Tunables.GetStartClipWaitTime(pPed), 0, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset);
				}
			}
#endif // DEBUG_DRAW && __DEV
			fAngle += sfRagdollTransSearchAngle;
		}

		// Found a safe spot, so update the anim matrix and continue.
		if(fAngle < TWO_PI)
		{
			*pAnimMatrix = testMat;
		}
		else
		{
			bClearToGetup = false;
		}
	}
#if DEBUG_DRAW && __DEV
	else if (bClearToGetup)
	{
		if(ms_bShowSafePositionChecks)
		{
			DebugDrawPedCapsule(*pAnimMatrix, Color_green, sm_Tunables.GetStartClipWaitTime(pPed), 0, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset);
		}
	}
#endif // DEBUG_DRAW && __DEV

#if __BANK
	// Force the getup to be blocked (useful for testing)
	if (sbForceBlockedGetup)
	{
		bClearToGetup = false;
	}
#endif

	if (bClearToGetup && m_forcedSet == NMBS_CRAWL_GETUPS)
	{
		Vector3 vCrawlStartPosition = pAnimMatrix->d + vBoundOffset;
		float fNewCapsuleRadius = pPed->GetCapsuleInfo()->GetHalfWidth() * CTaskFallOver::ms_fCapsuleRadiusMult;
		if (m_ProbeHelper == NULL)
		{
			m_ProbeHelper = rage_new CGetupProbeHelper(vBoundOffset.z + kfGroundToRootOffset);
			int iIncludeFlags = ArchetypeFlags::GTA_PED_INCLUDE_TYPES | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE;
			// Don't include the following types:
			iIncludeFlags &= ~(ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PICKUP_TYPE);
			m_ProbeHelper->SetIncludeFlags(iIncludeFlags);

			const int iMaxNumEntitiesToExclude = 2;
			int iNumEntitiesToExclude = 0;
			const CEntity* ppExcludeEntities[iMaxNumEntitiesToExclude];
			if (m_pExclusionEntity != NULL)
			{
				ppExcludeEntities[iNumEntitiesToExclude++] = m_pExclusionEntity;
			}

			CEntity* pEntityException = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
			if (pEntityException != NULL)
			{
				ppExcludeEntities[iNumEntitiesToExclude++] = pEntityException;
			}

			if (iNumEntitiesToExclude > 0)
			{
				m_ProbeHelper->SetExcludeEntities(ppExcludeEntities, iNumEntitiesToExclude);
			}

			m_ProbeHelper->SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
			m_ProbeHelper->SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
			m_ProbeHelper->SetIsDirected(true);
			m_ProbeHelper->SetCapsule(vCrawlStartPosition, VEC3_ZERO, fNewCapsuleRadius);

			// We now do line of sight tests to each safe position found so no need to limit our search to only interiors if inside or only exteriors if outside...
			u32 iFlags = CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects | CNavmeshClosestPositionHelper::Flag_ConsiderOnlyNonIsolated |
				CNavmeshClosestPositionHelper::Flag_ConsiderExterior | CNavmeshClosestPositionHelper::Flag_ConsiderInterior;

			if(!pPed->GetIsInWater())
			{
				iFlags |= CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand;
			}

			static float fSpacing = 1.0f;
			static float fZWeightingAbove = 2.0f;
			static float fZWeightingAtOrBelow = 0.75f;
			// Set z-weighting to be 0 so that we can do our own weighting based on the closest positions in the X/Y plane
			m_ProbeHelper->StartClosestPositionSearch(50.0f, iFlags, fZWeightingAbove, fZWeightingAtOrBelow, fSpacing);

			animTaskDebugf(this, "TRAPPED: Crawl position search: Testing from default capsule position...");
		}

		if (!m_ProbeHelper->GetAllResultsReady())
		{
			return false;
		}
		else
		{
#if DEBUG_DRAW && __DEV
			if(ms_bShowSafePositionChecks)
			{
				for (int i = 0; i < m_ProbeHelper->GetNumResult(); i++)
				{
					Matrix34 testMat(*pAnimMatrix);
					testMat.d = m_ProbeHelper->GetEndPosition(i);
					testMat.d.z += kfGroundToRootOffset;
					DebugDrawPedCapsule(testMat, m_ProbeHelper->GetResult(i).GetHitDetected() ? Color_red : Color_green, sm_Tunables.GetStartClipWaitTime(pPed));

					Vector3 vProbePosition(testMat.d);
					vProbePosition.z += vBoundOffset.z;
					CPhysics::ms_debugDrawStore.AddCapsule(VECTOR3_TO_VEC3V(m_ProbeHelper->GetStart()), VECTOR3_TO_VEC3V(vProbePosition), m_ProbeHelper->GetRadius(), Color_blue, sm_Tunables.GetStartClipWaitTime(pPed), 0, false);
				}
			}
#endif
			
			Vector3 vSafePosition;
			if (!m_ProbeHelper->GetClosestPosition(vSafePosition, true))
			{
				// Re-do the probes without the custom Z probe end offset
				if (m_ProbeHelper->GetNumResult() > 0 && m_ProbeHelper->GetCustomZProbeEndOffset() != FLT_MAX && !TestLOS(m_ProbeHelper->GetStart(), pAnimMatrix->d))
				{
					animTaskDebugf(this, "TRAPPED: Crawl position search: Testing from raised capsule position...");
					m_ProbeHelper->SetCustomZProbeEndOffset(FLT_MAX);
					m_ProbeHelper->SubmitProbes();
					return false;
				}

				// Get rid of the probe helper since we don't need the results any longer
				delete m_ProbeHelper;
				m_ProbeHelper = NULL;

				bClearToGetup = false;
			}
		}
	}

	if(bClearToGetup)
	{
		int nIncludeFlags = ArchetypeFlags::GTA_PED_INCLUDE_TYPES | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE;
		// Exclude the following types:
		nIncludeFlags &= ~(ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PICKUP_TYPE);
		int nTypeFlags = ArchetypeFlags::GTA_PED_TYPE;
		CEntity* pEntityException = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;

		float fPedProbeBottom = (-kfGroundToRootOffset);
		const CBipedCapsuleInfo *pBipedInfo = pCapsuleInfo->GetBipedCapsuleInfo();
		if (pBipedInfo)
		{
			fPedProbeBottom -= pBipedInfo->GetGroundOffsetExtend();
		}

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestFixedResults<> capsuleResult;
		capsuleDesc.SetResultsStructure(&capsuleResult);
		capsuleDesc.SetCapsule(pAnimMatrix->d, pAnimMatrix->d + Vector3(0.0f, 0.0f, fPedProbeBottom), pCapsuleInfo->GetProbeRadius());
		capsuleDesc.SetIncludeFlags(nIncludeFlags);
		capsuleDesc.SetTypeFlags(nTypeFlags);
		capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
		capsuleDesc.SetExcludeEntity(pEntityException);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(false);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
#if DEBUG_DRAW && __DEV
			if(ms_bShowSafePositionChecks)
			{
				CPhysics::ms_debugDrawStore.AddCapsule(VECTOR3_TO_VEC3V(pAnimMatrix->d), VECTOR3_TO_VEC3V(pAnimMatrix->d + Vector3(0.0f, 0.0f, fPedProbeBottom)), pCapsuleInfo->GetProbeRadius(),
					Color_pink, sm_Tunables.GetStartClipWaitTime(pPed));
			}
#endif // DEBUG_DRAW && __DEV

			int i = capsuleResult.GetSize() - 1;
			for (; i >= 0; i--)
			{
				if (capsuleResult[i].GetHitDetected())
				{
					Vector3 vHitPosition = capsuleResult[i].GetHitPosition();

					// Cache off the blocking entity
					pBlockingEntity = CPhysics::GetEntityFromInst(capsuleResult[i].GetHitInst());
					iBlockingComponent = capsuleResult[i].GetHitComponent();
					bool bValidBlockingEntity = IsValidBlockingEntity(pPed, pBlockingEntity);

					// The returned probe result should never be higher than the current root bone position
					// If it is, it means that there is an object between the animated start position of the capsule and the ragdoll 
					// Note: this only works because our getup assets are authored so that the entity matrix is roughly directly above the root in the animation

					// We know that we should be clear of any blocking object but we also can't use a position above our root if the entity blocks standing
					// So just keep iterating through the hits until we find one below our root...
					if (!bValidBlockingEntity || vHitPosition.z <= rootMatrix.d.z)
					{
						nmDebugf2("[%u] CTaskGetUp::FixupClipMatrix:%s(%p) Found ground position (%.4f %.4f %.4f)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed,
							vHitPosition.x, vHitPosition.y, vHitPosition.z);

						// Clear the blocking entity if we found a valid position...
						pBlockingEntity = NULL;
						iBlockingComponent = -1;

#if DEBUG_DRAW && __DEV
						if(ms_bShowSafePositionChecks)
						{
							CPhysics::ms_debugDrawStore.AddSphere(RCC_VEC3V(vHitPosition), 0.05f, Color_red, sm_Tunables.GetStartClipWaitTime(pPed));
						}
#endif // DEBUG_DRAW && __DEV
						pAnimMatrix->d.z = vHitPosition.z + kfGroundToRootOffset;
						pPed->SetIsStanding(true);
						pPed->SetWasStanding(true);
						pPed->SetGroundOffsetForPhysics(-kfGroundToRootOffset);
					}
					else if (bValidBlockingEntity)
					{
						break;
					}
				}
			}

#if !__NO_OUTPUT
			if (i < 0)
			{
				taskWarningf("CTaskGetUp::FixupClipMatrix: Could not find the ground - the must be something between our root bone and our animated start position");
			}
#endif
		}
		else
		{
#if DEBUG_DRAW && __DEV
			if(ms_bShowSafePositionChecks)
			{
				CPhysics::ms_debugDrawStore.AddCapsule(VECTOR3_TO_VEC3V(pAnimMatrix->d), VECTOR3_TO_VEC3V(pAnimMatrix->d + Vector3(0.0f, 0.0f, fPedProbeBottom)), pCapsuleInfo->GetProbeRadius(),
					Color_LightSeaGreen, sm_Tunables.GetStartClipWaitTime(pPed));
			}
#endif // DEBUG_DRAW && __DEV
		}

		// Safe to return to animation:
		return true;
	}
	else
	{
		if(IsValidBlockingEntity(pPed, pBlockingEntity))
		{
			m_pBumpedByEntity = pBlockingEntity;
			m_iBumpedByComponent = iBlockingComponent;
		}

		if(m_forcedSet == NMBS_CRAWL_GETUPS)
		{
			m_forcedSet = NMBS_INVALID;
		}
		else if(m_nStuckCounter > 0 && CanCrawl())
		{
			m_forcedSet = NMBS_CRAWL_GETUPS;
		}

		// Couldn't find anywhere safe, need to do some tidy up and then wait a while.
		// If player has been stuck for too long, try and rescue him to a safe position and continue.
		m_nStuckCounter += sm_Tunables.GetStartClipWaitTime(pPed);
		u32 nStuckCheckFailTime = sm_Tunables.GetStuckWaitTime(pPed);

		if(m_nStuckCounter >= nStuckCheckFailTime)
		{
			m_forcedSet = NMBS_INVALID;

			if( bTreatAsPlayer && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KillWhenTrapped) && !ShouldBlockingEntityKillPlayer(pBlockingEntity) )
			{
				if (m_ProbeHelper == NULL)
				{
					m_ProbeHelper = rage_new CGetupProbeHelper();
					m_ProbeHelper->SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE);
					if (m_pExclusionEntity)
					{
						m_ProbeHelper->SetExcludeEntity(m_pExclusionEntity);
					}
					m_ProbeHelper->SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
					m_ProbeHelper->SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
					m_ProbeHelper->SetIsDirected(true);
					m_ProbeHelper->SetCapsule(rootMatrix.d, VEC3_ZERO, 0.05f);

					// We now do line of sight tests to each safe position found so no need to limit our search to only interiors if inside or only exteriors if outside...
					u32 iFlags = CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects | CNavmeshClosestPositionHelper::Flag_ConsiderOnlyNonIsolated |
						CNavmeshClosestPositionHelper::Flag_ConsiderExterior | CNavmeshClosestPositionHelper::Flag_ConsiderInterior;

					if(!pPed->GetIsInWater())
					{
						iFlags |= CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand;
					}

					static float fSpacing = 1.0f;
					static float fZWeightingAbove = 2.0f;
					static float fZWeightingAtOrBelow = 0.75f;
					// Set z-weighting to be 0 so that we can do our own weighting based on the closest positions in the X/Y plane
					m_ProbeHelper->StartClosestPositionSearch(50.0f, iFlags, fZWeightingAbove, fZWeightingAtOrBelow, fSpacing);
					animTaskDebugf(this, "TRAPPED: Start closest position search");
				}
				
				if (!m_ProbeHelper->GetAllResultsReady())
				{
					return false;
				}
				else
				{
#if DEBUG_DRAW && __DEV
					if(ms_bShowSafePositionChecks)
					{
						static const u32 suKeySafePositionPedCapsule = ATSTRINGHASH("safe_position_ped_capsule", 0x5FA345E6);
						for (int i = 0; i < m_ProbeHelper->GetNumResult(); i++)
						{
							Matrix34 testMat(*pAnimMatrix);
							testMat.d = m_ProbeHelper->GetEndPosition(i);
							testMat.d.z += pCapsuleInfo->GetGroundToRootOffset();
							DebugDrawPedCapsule(testMat, m_ProbeHelper->GetResult(i).GetHitDetected() ? Color_red : Color_green, 10000, suKeySafePositionPedCapsule + i);
							CPhysics::ms_debugDrawStore.AddCapsule(VECTOR3_TO_VEC3V(m_ProbeHelper->GetStart()), VECTOR3_TO_VEC3V(m_ProbeHelper->GetProbeEndPosition(i)), m_ProbeHelper->GetRadius(), Color_blue, 10000, 0, false);
						}
					}
#endif // DEBUG_DRAW && __DEV

					// Try and find a position using the LOS tests
					if (m_ProbeHelper->GetClosestPosition(pAnimMatrix->d, true, m_ProbeHelper->GetStateIncludeFlags()==phLevelBase::STATE_FLAG_FIXED))
					{
						pAnimMatrix->d.z += pCapsuleInfo->GetGroundToRootOffset();

						animTaskDebugf(this, "TRAPPED: Found closest safe position: (%.3f, %.3f, %.3f)", pAnimMatrix->d.x, pAnimMatrix->d.y, pAnimMatrix->d.z);
						
						// Done with the probe helper - get rid of it!
						delete m_ProbeHelper;
						m_ProbeHelper = NULL;

						pPed->GetPortalTracker()->RequestRescanNextUpdate();
						return true;
					}
					else
					{
						// Re-do the probes against fixed objects only
						if (m_ProbeHelper->GetNumResult() > 0 && m_ProbeHelper->GetStateIncludeFlags() != phLevelBase::STATE_FLAG_FIXED)
						{
							animTaskDebugf(this, "TRAPPED: Safe position search: Testing against fixed geometry only...");
							m_ProbeHelper->SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);
							m_ProbeHelper->SubmitProbes();
							return false;
						}
						// Re-do the probes from our clip matrix (which should include our ground-to-root-offset)
						else if (m_ProbeHelper->GetNumResult() > 0 && !m_bSearchedFromClipPos && !TestLOS(m_ProbeHelper->GetStart(), pAnimMatrix->d))
						{
							animTaskDebugf(this, "TRAPPED: Safe position search: Testing from raised capsule position...");
							// By default the probe helper probes from the start position to the returned path-server query positions using the start position height
							m_ProbeHelper->SetCapsule(pAnimMatrix->d, m_ProbeHelper->GetEnd(), m_ProbeHelper->GetRadius());
							m_ProbeHelper->SubmitProbes();
							m_bSearchedFromClipPos = true;
							return false;
						}
						// If we've already re-done the probe against fixed objects and from our clip matrix position and still can't find a clear path...
						else
						{
							bool bCanStandOnBlockingEntity = false;
							// Couldn't find any safe position (with LOS) - see if we can stand on our blocking physical entity
							if (pBlockingEntity != NULL && pBlockingEntity->GetIsPhysical())
							{
								static const Vector3 svShapeTestOffset(0.0f, 0.0f, 1000.0f);
								vStart = shapeTestDesc.GetStart();
								Vector3 vEnd(vStart);
								vStart += svShapeTestOffset;
								vEnd -= svShapeTestOffset;
								shapeTestDesc.SetCapsule(vStart, vEnd, shapeTestDesc.GetRadius());
#if DEBUG_DRAW && __DEV
								if(ms_bShowSafePositionChecks)
								{
									CPhysics::ms_debugDrawStore.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), sfGroundCheckCapsuleRadius, Color_yellow, sm_Tunables.GetStartClipWaitTime(pPed));
								}
#endif // DEBUG_DRAW && __DEV

								shapeTestDesc.SetIncludeEntity(pBlockingEntity, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS | WorldProbe::EIEO_DONT_ADD_WEAPONS | WorldProbe::EIEO_DONT_ADD_PED_ATTACHMENTS);
								shapeTestDesc.SetExcludeEntities(NULL, 0);
								shapeTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
								if (WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
								{
									for (int i = 0; i < shapeTestResult.GetSize(); i++)
									{
										if (pBlockingEntity == CPhysics::GetEntityFromInst(shapeTestResult[i].GetHitInst()))
										{
											Matrix34 testMat(*pAnimMatrix);
											testMat.d.z = shapeTestResult[i].GetHitPosition().z + pCapsuleInfo->GetGroundToRootOffset();

											if (!CPedGeometryAnalyser::TestPedCapsule(pPed, &testMat, (const CEntity**)&pPed, 1, EXCLUDE_ENTITY_OPTIONS_NONE, nTestIncludeTypes, ArchetypeFlags::GTA_PED_TYPE, ArchetypeFlags::GTA_FOLIAGE_TYPE,
												NULL, NULL, fBoundRadiusMult, fBoundHeading, fBoundPitch, vBoundOffset))
											{
												pAnimMatrix->d.z = testMat.d.z;
												bCanStandOnBlockingEntity = true;
		#if DEBUG_DRAW && __DEV
												if(ms_bShowSafePositionChecks)
												{
													DebugDrawPedCapsule(testMat, Color_green, sm_Tunables.GetStartClipWaitTime(pPed));
												}
		#endif // DEBUG_DRAW && __DEV
											}
											else
											{
		#if DEBUG_DRAW && __DEV
												if(ms_bShowSafePositionChecks)
												{
													DebugDrawPedCapsule(testMat, Color_red, sm_Tunables.GetStartClipWaitTime(pPed));
												}
		#endif // DEBUG_DRAW && __DEV
											}

											break;
										}
									}
								}
							}

							// If we don't have a valid blocking entity or can't stand up on top of it then we fall-back to our safe position
							if (!bCanStandOnBlockingEntity)
							{
								Vector3 vecSafePos = pAnimMatrix->d;
								if (taskVerifyf(FindSafePosition(pPed, pAnimMatrix->d, vecSafePos), "CTaskGetUp::FixupClipMatrix: Couldn't find safe position to rescue player to."))
								{
									animTaskDebugf(this, "TRAPPED: All probes failed, rescuing to safe car node location");
									pAnimMatrix->d = vecSafePos;
								}
							}
							else
							{
								animTaskDebugf(this, "TRAPPED: All probes failed, standing up on blocking entity");
							}
							
							// Done with the probe helper - get rid of it!
							delete m_ProbeHelper;
							m_ProbeHelper = NULL;

							// need to disable the high fall abort in these extreme cases, as the capsule can be left in the air,
							// leading to infinite ragdoll activation -> trapped loops.
							m_bDisableHighFallAbort = true;

							pPed->GetPortalTracker()->RequestRescanNextUpdate();
							return true;
						}
					}
				}
			}
			else
			{
				if (m_pBumpedByEntity != NULL && !pPed->IsNetworkClone() && !pPed->m_nPhysicalFlags.bNotDamagedByCollisions)
				{
					// We don't want a death animation to play, so let's just pretend we were dead all along...
					CEventDeath deathEvent(false, true);
					pPed->GetPedIntelligence()->AddEvent(deathEvent);
				}
			}

			return false;
		}
		else
		{
			// It's not safe to return to animation this time.
			return false;
		}	
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ActivateAnimatedInstance(CPed* pPed, Matrix34* pAnimMatrix)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Ready the ped's skeleton for the return to animation. Start by working out a transformation matrix from ragdoll to animation space.
	Matrix34 ragdollToAnimMtx;
	ragdollToAnimMtx.FastInverse(*pAnimMatrix);
	ragdollToAnimMtx.DotFromLeft(RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
	pPed->GetSkeleton()->Transform(RCC_MAT34V(ragdollToAnimMtx));

	pPed->GetRagdollInst()->SetMatrix(RCC_MAT34V(*pAnimMatrix));
	CPhysics::GetLevel()->UpdateObjectLocation(pPed->GetRagdollInst()->GetLevelIndex());

	// Need to pose local matrices of skeleton because ragdoll has only been updating the global matrices.
	pPed->InverseUpdateSkeleton();

	// Blend in idle to stop any movement.
	pPed->StopAllMotion(true);

	float ragdollFrameBlendDuration = SLOW_BLEND_DURATION;
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithDeepSurface))
	{
		ragdollFrameBlendDuration = REALLY_SLOW_BLEND_DURATION;
	}
	else if (m_pMatchedItem && m_pMatchedItem->IsPoseItem())
	{
		CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_pMatchedItem);
		ragdollFrameBlendDuration = pPoseItem->m_ragdollFrameBlendDuration;
	}

	if (pPed->GetAnimDirector())
	{
		fwAnimDirectorComponentRagDoll* componentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();
		if(componentRagDoll)
		{
			// Actually make the switch to the animated instance.
			componentRagDoll->PoseFromRagDoll(*pPed->GetAnimDirector()->GetCreature());

			bool extrapolate = FPS_MODE_SUPPORTED_ONLY(pPed->IsLocalPlayer() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) ? false :) true;
			pPed->GetMovePed().SwitchToAnimated(ragdollFrameBlendDuration, extrapolate);
		}
	}

	Vector3 vecRootVelocity(VEC3_ZERO);
	if(pPed->GetCollider())
	{
		vecRootVelocity.Set(RCC_VECTOR3(pPed->GetCollider()->GetVelocity()));
	}

	// Actually make the switch to the animated instance.
	pPed->SwitchToAnimated(true, true, false, false, false, !m_bRunningAsMotionTask);

	// Allow the ped to collide with the deep surface type until we're done getting up
	if (pPed->GetAnimatedInst() != NULL && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
	{
		CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex(), CPhysics::GetLevel()->GetInstanceIncludeFlags(pPed->GetAnimatedInst()->GetLevelIndex()) | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);
	}

	// Blend in the ik slope fixup
	ActivateSlopeFixup(pPed, ragdollFrameBlendDuration > 0.0f ? 1.0f / ragdollFrameBlendDuration : INSTANT_BLEND_IN_DELTA);

	// Try and maintain velocity in the animated ped.
	pPed->SetVelocity(vecRootVelocity);
}

//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::ActivateSlopeFixup(CPed* pPed, float blendRate)
//////////////////////////////////////////////////////////////////////////
{
	// Don't need to do slope fixup for standing, swimming or diving getups
	CNmBlendOutSet* pBlendOutSet = CNmBlendOutSetManager::GetBlendOutSet(m_matchedSet);
	if(pBlendOutSet == NULL || pBlendOutSet->IsFlagSet(CNmBlendOutSet::BOSF_AllowRootSlopeFixup))
	{
		pPed->GetIkManager().AddRootSlopeFixup(blendRate);
	}
}

bool CTaskGetUp::IsSlopeFixupActive(const CPed* pPed)
{
	const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pSolver)
	{
		return pSolver->IsActive();
	}

	return false; //assume true?
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::PickClipUsingPointCloud(CPed* pPed, Matrix34* pAnimMatrix, CNmBlendOutItem*& outItem, eNmBlendOutSet& outSet, float& outTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	PF_FUNC(PickClipUsingPointCloud);

	if(!pPed->IsNetworkClone())
	{
		// wipe any data we may have generated in the previous call of this function...
		m_pointCloudPoseSets.Reset();
	}
	else if(IsTaskRunningOverNetwork_Clone(CTaskTypes::TASK_GET_UP))
	{
		// we should have this information by now...
		Assertf(m_pointCloudPoseSets.GetNumBlendOutSets(), "ERROR : CTaskGetUp::PickClipUsingPointCloud : Clone : point cloud pose sets empty?!");
	}

	// calc which sets we need (on owner only) or (when we don't have a corresponding owner task and we haven't recieved the anim sets already)...
	if( (!pPed->IsNetworkClone()) || (!IsTaskRunningOverNetwork_Clone(CTaskTypes::TASK_GET_UP) && !m_pointCloudPoseSets.GetNumBlendOutSets()) )
	{
		if (pPed->GetUsingRagdoll())
		{
			// Get the root matrix which should give us the best estimate of where to place the animated ped.
			Matrix34 rootMatrix;
			pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);

			// Copy the position from the root matrix.
			pAnimMatrix->Identity();
			pAnimMatrix->d = rootMatrix.d;
		}
		else
		{
			Matrix34 pedMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			pAnimMatrix->Set(pedMatrix);
		}

		SelectBlendOutSets(pPed, m_pointCloudPoseSets, pAnimMatrix, this);
	}

	bool bMatchResult = false;

	atHashString blendOutSetHashString = pPed->GetPedModelInfo()->GetGetupSet();
	CNmBlendOutSet* pBlendOutSet = CNmBlendOutSetManager::GetBlendOutSet(blendOutSetHashString);
	if (pBlendOutSet == NULL || !pBlendOutSet->IsFlagSet(CNmBlendOutSet::BOSF_ForceNoBlendOutSet))
	{
		fwMvClipSetId clipSetId;
		fwMvClipId clipId;
		
		if (m_pointCloudPoseSets.GetFilter().GetKeyCount() > 0)
		{
			bMatchResult = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().FindBestMatch(*pPed->GetSkeleton(), clipSetId, clipId, outTime, *pAnimMatrix, &m_pointCloudPoseSets.GetFilter());
		}

		if (bMatchResult)
		{
			outItem = m_pointCloudPoseSets.FindPoseItem(clipSetId, clipId, outSet);

#if __BANK
			if (outItem != NULL)
			{
				CAnimViewer::BlendOutCapture(pPed, m_pointCloudPoseSets, outSet, outItem, "Getup ");
			}
#endif
		}

		taskAssertf(bMatchResult && outItem != NULL, "Unable to match nm blend out set using point cloud: %s - Result=%s, m_pMatchedItem=%s, clipset=%s(%u), clip=%s(%u), Controlling task: %s, Ped: %s, Posematcher:%s", 
			m_pointCloudPoseSets.DebugDump().c_str(),
			bMatchResult ? "TRUE" : "FALSE",
			outItem != NULL ? "VALID" : "NULL",
			clipSetId.TryGetCStr() ? clipSetId.GetCStr() : "UNKNOWN", clipSetId.GetHash(),
			clipId.TryGetCStr() ? clipId.GetCStr() : "UNKNOWN", clipId.GetHash(),
			FindGetupControllingTask(pPed) ? FindGetupControllingTask(pPed)->GetName().c_str() : "None", 
			pPed->GetModelName(), 
			pPed->GetPedModelInfo()->GetPoseMatcherFileIndex().Get()>-1 ? g_PoseMatcherStore.GetName(pPed->GetPedModelInfo()->GetPoseMatcherFileIndex()) : "Pose matcher file not found"
			);
	}
	return bMatchResult;
}

//////////////////////////////////////////////////////////////////////////
void CTaskGetUp::SelectBlendOutSets(CPed* pPed, CNmBlendOutSetList& poseSets, Matrix34* pAnimMatrix, CTaskGetUp* pGetUpTask)
//////////////////////////////////////////////////////////////////////////
{
	bool bUseDivingSet = false;
	bool bUseSwimmingSet = false;
	float fUnderwaterBias = 0.0f;

#if __BANK
	if (CAnimViewer::m_bForceBlendoutSet)
	{
		poseSets.Add(CAnimViewer::m_ForcedBlendOutSet);
		return;
	}
#endif //__BANK

	// check if the passed in override set is high priority. If so, use that.
	if (pGetUpTask && pGetUpTask->m_forcedSet!=NMBS_INVALID)
	{
		CNmBlendOutSet* pGetupSet =CNmBlendOutSetManager::GetBlendOutSet(pGetUpTask->m_forcedSet);
		if (pGetupSet && pGetupSet->IsFlagSet(CNmBlendOutSet::BOSF_ForceDefaultBlendOutSet))
		{
			poseSets.Add(pGetUpTask->m_forcedSet);
			return;
		}
	}

	// check for forced movement set override
	const fwClipSetWithGetup* pSet = FindClipSetWithGetup(pPed);
	if (pSet)
	{
		CNmBlendOutSet* pGetupSet = pSet->GetGetupSet()!=NMBS_INVALID ? CNmBlendOutSetManager::GetBlendOutSet(pSet->GetGetupSet()) : NULL;
		if (pGetupSet && pGetupSet->IsFlagSet(CNmBlendOutSet::BOSF_ForceDefaultBlendOutSet))
		{
			poseSets.Add(pSet->GetGetupSet());
			return;
		}
	}

	Assert(pPed->GetCapsuleInfo());
	bool bIsQuadruped = pPed->GetCapsuleInfo()->IsQuadruped();

	//are we swimming?
	if (pPed->GetIsSwimming() && !bIsQuadruped)
	{
		// work out if the water is shallow enough to stand in. If so, we should
		// use the standard getup behaviour instead of the swimming one.

		// use the root matrix as the basis of our water probe. the entity matrix can
		// be significantly offset, depending on how the ped was animating on ragdoll activation
		Vector3 vPedPos(VEC3_ZERO);
		Matrix34 rootMatrix;
		pPed->GetGlobalMtx(BONETAG_ROOT, rootMatrix);
		vPedPos = rootMatrix.d;
		
		float fWaterLevel;
		Vector3 vGroundIntersect(VEC3_ZERO);
		if(pPed->m_Buoyancy.GetWaterLevelIncludingRivers(vPedPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL, pPed))
		{
			// Try to find an accurate ground position for the capsule.
			const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();
			WorldProbe::CShapeTestFixedResults<> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vPedPos, vPedPos - Vector3(0.0f, 0.0f, pCapsuleInfo->GetGroundToRootOffset()*1.2f));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);
			probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
			const int iMaxNumEntitiesToExclude = 4;
			const CEntity* ppExcludeEntities[iMaxNumEntitiesToExclude];
			int iNumEntitiesToExclude = ExcludeUnderwaterBikes(pPed, ppExcludeEntities, iMaxNumEntitiesToExclude - 1);
			ppExcludeEntities[iNumEntitiesToExclude++] = pPed;
			probeDesc.SetExcludeEntities(ppExcludeEntities, iNumEntitiesToExclude);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				vGroundIntersect = probeResult[0].GetHitPosition();
			}
			else
			{
				vGroundIntersect = probeDesc.GetEnd();
			}
			
			vGroundIntersect.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

			// This extra water height check forces the ped to do an on-ground blend out instead of a swimming blend out in water deeper than the ground-to-root offset.
			// This is necessary as there are places where the water height can vary quite drastically while in the middle of a getup. When initially detecting the type of blend out
			// the water height might have indicated a swimming blend out would be best but as the water recedes that might not hold true by the time the blend out is half-way through.
			// It looks much better to perform an on-ground blend out in slightly deeper water than necessary than it does perform a swimming blend out in slightly
			// shallower water than necessary.  The extra water height check allows us to bias the blend out between on-ground and swimming.
			// This is for bug #1193149 (NM GETUP WATER: Bad on land swimming blend out in shallow water, did a run start too but I wasn't holding input)
			static bank_float sfExtraWaterHeight = 0.3f;
			// Since water height can't change dynamically in a river we are safe having a lower extra water height...
			static bank_float sfExtraWaterHeightRiver = 0.2f;
			Vector3 vRiverHitPosition;
			Vector3 vRiverHitNormal;
			float fExtraWaterHeight = pPed->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPosition, vRiverHitNormal) ? sfExtraWaterHeightRiver : sfExtraWaterHeight;
			if(vGroundIntersect.z < (fWaterLevel - fExtraWaterHeight))
			{
				if (pPed->GetWaterLevelOnPed() > 0.75f)
				{
					if ((fWaterLevel - pAnimMatrix->d.z) > 1.0f)
					{
						fUnderwaterBias = 1.0f;
					}
					// Bias the underwater animations so they are less likely to get chosen when closer to the surface...
					else
					{
						fUnderwaterBias = 2.0;
					}
				}


				if (pPed->GetPrimaryMotionTask() && pPed->GetPrimaryMotionTask()->IsDiving())
				{
					bUseDivingSet = true;
				}
				else
				{
					bUseSwimmingSet = true;
				}
			}
			else 
			{
				pAnimMatrix->d = vGroundIntersect;
			}
		}
	}

	// Are we underwater?
	if (fUnderwaterBias > SMALL_FLOAT)
	{
		poseSets.Add(NMBS_UNDERWATER_GETUPS, fUnderwaterBias);
	}
	
	// *Allow diving and swimming pose sets even when an underwater set has been chosen since it might still be better
	// to dive or swim*
	// Should we use a diving blendoutset
	if (bUseDivingSet)
	{
		poseSets.Add(NMBS_DIVING_GETUPS);
	}
	// Should we use a swimming blendoutset
	else if (bUseSwimmingSet)
	{
		poseSets.Add(NMBS_SWIMMING_GETUPS);
	}
	// Does the ped have four legs?
	else if (bIsQuadruped)
	{
		poseSets.Add(pPed->GetPedModelInfo()->GetGetupSet());
	}
	// Are we hand cuffed?
	else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		// add the cuffed group from the metadata
		poseSets.Add(NMBS_CUFFED_GETUPS);
		poseSets.Add(NMBS_STANDING);
	}
#if ENABLE_DRUNK
	//are we drunk
	else if (pPed->IsDrunk())
	{
		// add the drunk group from the metadata
		poseSets.Add(CNmBlendOutSetManager::IsBlendOutSetStreamedIn(NMBS_DRUNK_GETUPS) ? NMBS_DRUNK_GETUPS : pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE);
		poseSets.Add( NMBS_STANDING );
	}
#endif // ENABLE_DRUNK
	else
	{
		// Has the task passed in a specific blendoutset
		if (pGetUpTask && pGetUpTask->m_forcedSet)
		{
			poseSets.Add(pGetUpTask->m_forcedSet);

			if(pGetUpTask->m_forcedSet == NMBS_WRITHE)
			{
				// at this point, we've been told to use the writhe set but TaskWrithe needs to know a target too
				// We get the target from the controlling task (TaskGun) so create a temp BlendOutSetList and get the info
				CNmBlendOutSetList targetFinder;
				bool bBlendOutSetSpecified = GetRequestedBlendOutSets(targetFinder, pPed);

				if (pGetUpTask && bBlendOutSetSpecified)
				{
					pGetUpTask->m_Target = targetFinder.GetTarget();
				}
			}
		}
		else
		{
			// Ask what blendoutset the controlling ai wants to use
			bool bBlendOutSetSpecified = GetRequestedBlendOutSets(poseSets, pPed);

			if (bBlendOutSetSpecified)
			{
				if (pGetUpTask)
				{
					if (pGetUpTask->m_pMoveTask)
						delete pGetUpTask->m_pMoveTask;

					pGetUpTask->m_pMoveTask = poseSets.RelinquishMoveTask();
					pGetUpTask->m_Target = poseSets.GetTarget();
				}				
			}
			// Controlling task has no blendoutset preference
			else
			{
				// Use default blendout sets

				// Are we standing?
				if (ShouldUseStandingBlendOutSet(pPed))
				{
					// Are we injured?
					if (ShouldUseInjuredGetup(pPed))
					{
						// The ped is using an injured movement set - use the blend to injured standing versions.
						poseSets.Add( NMBS_INJURED_STANDING );
					}
					else
					{
						poseSets.Add( NMBS_STANDING );
					}
				}
				else
				{
					// Add the standard blendoutsets
					poseSets.Add( PickStandardGetupSet(pPed) );
				}

				// ?
				if (pPed->ShouldDoHurtTransition() && pGetUpTask && pGetUpTask->m_Target.GetIsValid())
					poseSets.SetTarget(pGetUpTask->m_Target);
			}
		}
	}

	if ( pGetUpTask && pGetUpTask->m_pMoveTask)
	{
		pPed->GetPedIntelligence()->AddTaskMovement( pGetUpTask->m_pMoveTask );
		taskAssert(pPed->GetPedIntelligence()->GetGeneralMovementTask() );
		taskAssert(pPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == pGetUpTask->m_pMoveTask->GetTaskType() );

		// Mark the movement task as still in use this frame
		CTaskMoveInterface* pMoveInterface = pPed->GetPedIntelligence()->GetGeneralMovementTask()->GetMoveInterface();
		pMoveInterface->SetCheckedThisFrame(true);

#if !__FINAL
		pMoveInterface->SetOwner(pGetUpTask);
#endif /* !__FINAL */
	}
}

bool CTaskGetUp::GetRequestedBlendOutSets(CNmBlendOutSetList& sets, CPed* pPed)
{

	CTask* pTask = NULL;
	bool bBlendOutSetSpecified = false;
	CEvent* pEvent = NULL;

	// No currently running tasks are requesting a specific blend out.
	// Check the highest priority event waiting in the queue
	aiEvent::BeginEventQuery();
	pEvent = pPed->GetPedIntelligence()->GetEventGroup().GetHighestPriorityEvent();
	aiEvent::EndEventQuery();

	if (pEvent)
	{
		switch (pEvent->GetEventType())
		{
		case EVENT_SCRIPT_COMMAND:
			{
				CEventScriptCommand* pScriptEvent = static_cast<CEventScriptCommand*>(pEvent);
				pTask = static_cast<CTask*>(pScriptEvent->GetTask());
				bBlendOutSetSpecified = pTask ? pTask->AddGetUpSets(sets, pPed) : false;
				nmEntityDebugf(pPed, "GetRequestedBlendOutSets - incoming script event - getup controlling task: %s. Blend Out sets specified: %s", pTask ? pTask->GetName() : "none", bBlendOutSetSpecified ? "yes" : "no");
			}
			break;
		}
	}

	if (!bBlendOutSetSpecified)
	{
		// Get the task controlling the getup
		pTask = FindGetupControllingTask(pPed);
		bBlendOutSetSpecified = pTask ? pTask->AddGetUpSets(sets, pPed) : false;
		animEntityDebugf(pPed, "GetRequestedBlendOutSets - getup controlling task: %s. blend out sets specified: %s", pTask ? pTask->GetName() : "none", bBlendOutSetSpecified ? "yes" : "no");
	}

#if __ASSERT
	if (ms_SpewRagdollTaskInfoOnGetUpSelection)
	{
		pPed->SpewRagdollTaskInfo();
	}
#endif //#if __ASSERT

	return bBlendOutSetSpecified;
}

void CTaskGetUp::NotifyGetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed)
{
	// Get the task controlling the getup
	CTask* pTask = FindGetupControllingTask(pPed);
	if (pTask)
		pTask->GetUpComplete(setMatched, lastItem, pMoveTask, pPed);

	// also notify the highest priority event if appropriate
	aiEvent::BeginEventQuery();
	CEvent* pEvent = pPed->GetPedIntelligence()->GetEventGroup().GetHighestPriorityEvent();
	aiEvent::EndEventQuery();

	if (pEvent)
	{
		switch (pEvent->GetEventType())
		{
		case EVENT_SCRIPT_COMMAND:
			{
				CEventScriptCommand* pScriptEvent = static_cast<CEventScriptCommand*>(pEvent);
				pTask = static_cast<CTask*>(pScriptEvent->GetTask());
				if (pTask)
					pTask->GetUpComplete(setMatched, lastItem, pMoveTask, pPed);
			}
			break;
		}
	}
}

void CTaskGetUp::RequestAssetsForGetup(CPed* pPed)
{
	// Get the task controlling the getup
	CTask* pTask = FindGetupControllingTask(pPed);
	if (pTask)
		pTask->RequestGetupAssets(pPed);

	// Also check the highest priority event
	aiEvent::BeginEventQuery();
	CEvent* pEvent = pPed->GetPedIntelligence()->GetEventGroup().GetHighestPriorityEvent();
	aiEvent::EndEventQuery();

	if (pEvent)
	{
		switch (pEvent->GetEventType())
		{
		case EVENT_SCRIPT_COMMAND:
			{
				CEventScriptCommand* pScriptEvent = static_cast<CEventScriptCommand*>(pEvent);
				pTask = static_cast<CTask*>(pScriptEvent->GetTask());
				if (pTask)
					pTask->RequestGetupAssets(pPed);
			}
			break;
		}
	}
}

bool CTaskGetUp::ShouldUseStandingBlendOutSet(CPed* pPed)
{
	if (pPed)
	{
		// get the ragdoll root bone position
		Matrix34 rootMatrix;
		pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);

		// Don't include standing anims in search if root is too close to the ground below.
		static bank_float sfClavicleHeightForStandingExit = 0.65f;
		// A height of 40cm allows a kneeling ped to use standing exits which is preferable
		static bank_float sfThighHeightForStandingExit = 0.4f;
		static bank_float sfTestHeight = 1.25f;
		static bank_float sfGroundRejection = 0.6f;

		WorldProbe::CShapeTestFixedResults<2> probeResult;
		int nTestTypes = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

		// Fill in the descriptor for the batch test.
		WorldProbe::CShapeTestBatchDesc batchDesc;
		batchDesc.SetOptions(0);
		batchDesc.SetIncludeFlags(nTestTypes);
		batchDesc.SetExcludeEntity(NULL);
		ALLOC_AND_SET_CAPSULE_DESCRIPTORS(batchDesc, 2);

		WorldProbe::CShapeTestCapsuleDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetMaxNumResultsToUse(1);
		probeDesc.SetIncludeFlags(nTestTypes);
		probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_FOLIAGE_TYPE);
		probeDesc.SetExcludeEntity(NULL);
		probeDesc.SetDoInitialSphereCheck(true);
		probeDesc.SetIsDirected(true);

		Vector3 vRight;
		Vector3 vLeft;
		pPed->GetBonePosition(vRight, BONETAG_R_CLAVICLE);
		pPed->GetBonePosition(vLeft, BONETAG_L_CLAVICLE);
		Vector3 vAverage;
		vAverage.Average(vRight, vLeft);
		probeDesc.SetCapsule(vAverage, vAverage - Vector3(0.0f, 0.0f, sfTestHeight), pPed->GetCapsuleInfo()->GetProbeRadius());
		probeDesc.SetFirstFreeResultOffset(0);
		batchDesc.AddCapsuleTest(probeDesc);
		
		pPed->GetBonePosition(vRight, BONETAG_R_THIGH);
		pPed->GetBonePosition(vLeft, BONETAG_L_THIGH);
		vAverage.Average(vRight, vLeft);
		probeDesc.SetCapsule(vAverage, vAverage - Vector3(0.0f, 0.0f, sfTestHeight), pPed->GetCapsuleInfo()->GetProbeRadius());
		probeDesc.SetFirstFreeResultOffset(1);
		batchDesc.AddCapsuleTest(probeDesc);

		// If neither probe hit anything then we always use a standing blend out set
		if (!WorldProbe::GetShapeTestManager()->SubmitTest(batchDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			return true;
		}

		// If both probes hit and we need to check the height difference between the hit positions to determine if we should use a standing blend out set
		if (probeResult[0].IsAHit() && probeResult[1].IsAHit())
		{
			float fHeightDifference = probeResult[0].GetHitPosition().z - probeResult[1].GetHitPosition().z;
			if (fHeightDifference > sfClavicleHeightForStandingExit)
			{
				return true;
			}
		}

		// If both probes are invalid (an invalid probe is one that didn't hit, has an invalid hit normal or hit too far below the start position) then we should use a standing blend out set
		if ((!probeResult[0].IsAHit() || (probeResult[0].GetHitNormal().z < sfGroundRejection) || ((probeResult[0].GetT() * sfTestHeight) > sfClavicleHeightForStandingExit)) && 
			(!probeResult[1].IsAHit() || (probeResult[1].GetHitNormal().z < sfGroundRejection) || ((probeResult[1].GetT() * sfTestHeight) > sfThighHeightForStandingExit)))
		{
			return true;
		}
	}

	return false;
}

eNmBlendOutSet CTaskGetUp::PickStandardGetupSet(CPed* pPed)
{
	if (pPed->IsPlayer())
	{
		// player get up sets
		if (NetworkInterface::IsGameInProgress())
		{
#if FPS_MODE_SUPPORTED
			if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				if (pPed->IsUsingActionMode())
				{
					return NMBS_FIRST_PERSON_MP_ACTION_MODE_GETUPS;
				}
				else if (ShouldUseInjuredGetup(pPed))
				{
					return NMBS_INJURED_FIRST_PERSON_MP_GETUPS;
				}
				else
				{
					return NMBS_FIRST_PERSON_MP_GETUPS;
				}
			}
			else
#endif
			{
				if (pPed->IsUsingActionMode())
				{
					return NMBS_PLAYER_MP_ACTION_MODE_GETUPS;
				}
				else if (ShouldUseInjuredGetup(pPed))
				{
					return NMBS_INJURED_PLAYER_MP_GETUPS;
				}
				else
				{
					// check for movement set override
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
					if (pTask)
					{
						fwMvClipSetId setId = pTask->GetDefaultOnFootClipSet();
						fwClipSet* pSet = fwClipSetManager::GetClipSet(setId);
						if (pSet && pSet->GetClipSetType()==fwClipSet::kTypeClipSetWithGetUp)
						{
							return (eNmBlendOutSet)static_cast<fwClipSetWithGetup*>(pSet)->GetGetupSet();
						}
					}

					return NMBS_PLAYER_MP_GETUPS;
				}
			}
		}
		else
		{
#if FPS_MODE_SUPPORTED
			if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				if (pPed->IsUsingActionMode())
				{
					return NMBS_FIRST_PERSON_ACTION_MODE_GETUPS;
				}
				else if (ShouldUseInjuredGetup(pPed))
				{
					return NMBS_INJURED_FIRST_PERSON_GETUPS;
				}
				else
				{
					return NMBS_FIRST_PERSON_GETUPS;
				}
			}
			else
#endif
			{
				if (pPed->IsUsingActionMode())
				{
					return NMBS_PLAYER_ACTION_MODE_GETUPS;
				}
				else if (ShouldUseInjuredGetup(pPed))
				{
					return NMBS_INJURED_PLAYER_GETUPS;
				}
				else
				{
					// check for movement set override
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
					if (pTask)
					{
						fwMvClipSetId setId = pTask->GetDefaultOnFootClipSet();
						fwClipSet* pSet = fwClipSetManager::GetClipSet(setId);
						if (pSet && pSet->GetClipSetType()==fwClipSet::kTypeClipSetWithGetUp)
						{
							return (eNmBlendOutSet)static_cast<fwClipSetWithGetup*>(pSet)->GetGetupSet();
						}
					}

					return NMBS_PLAYER_GETUPS;
				}
			}
		}
	}
	else
	{
		// a.i. get up sets
		// If ped is injured or in combat, modify the anim choice for better contextual return to animation.
		// CNC: peds shouldn't get up into writhe. 
		if (pPed->ShouldDoHurtTransition() && CNmBlendOutSetManager::IsBlendOutSetStreamedIn(NMBS_WRITHE) && !NetworkInterface::IsInCopsAndCrooks())
		{
			return NMBS_WRITHE;
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
		{
			return pPed->IsMale() ? NMBS_FAST_GETUPS : NMBS_FAST_GETUPS_FEMALE;
		}
		else if(ShouldUseInjuredGetup(pPed))
		{
			if (ShouldUseArmedGetup(pPed) && CNmBlendOutSetManager::IsBlendOutSetStreamedIn(NMBS_INJURED_ARMED_GETUPS))
			{
				return NMBS_INJURED_ARMED_GETUPS;
			}
			else if (CNmBlendOutSetManager::IsBlendOutSetStreamedIn(NMBS_INJURED_GETUPS))
			{
				return NMBS_INJURED_GETUPS;
			}
		}

		if (pPed->IsUsingActionMode())
		{
			return NMBS_ACTION_MODE_GETUPS;
		}
		else
		{
			// check for movement set override
			const fwClipSetWithGetup* pSet = FindClipSetWithGetup(pPed);
			if (pSet)
			{
				return pSet->GetGetupSet();
			}
			else
			{
				return pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const fwClipSetWithGetup* CTaskGetUp::FindClipSetWithGetup(const CPed* pPed)
{
	const CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	if (pTask)
	{
		fwMvClipSetId setId = pTask->GetDefaultOnFootClipSet();
		const fwClipSet* pSet = fwClipSetManager::GetClipSet(setId);
		if (pSet && pSet->GetClipSetType()==fwClipSet::kTypeClipSetWithGetUp)
		{
			return static_cast<const fwClipSetWithGetup*>(pSet);
		}
	}

	// if the on foot clipset didn't give us a clip set to use, try the default from peds.meta
	if (pPed->GetPedModelInfo())
	{
		fwMvClipSetId setId = pPed->GetPedModelInfo()->GetMovementClipSet();
		if (setId!=CLIP_SET_ID_INVALID)
		{
			const fwClipSet* pSet = fwClipSetManager::GetClipSet(setId);
			if (pSet && pSet->GetClipSetType()==fwClipSet::kTypeClipSetWithGetUp)
			{
				return static_cast<const fwClipSetWithGetup*>(pSet);
			}
		}
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////

bool CTaskGetUp::ShouldUseInjuredGetup(CPed* pPed)
{
	if (pPed->IsPlayer())
	{
			// reasons we should never use n injured getup
			if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed)
#if ENABLE_DRUNK
				&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDrunk)
#endif // ENABLE_DRUNK
				&& (!pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning))
			)
			{
				if (pPed->GetHealth() < CTaskGetUp::sm_Tunables.m_fPreferInjuredGetupPlayerHealthThreshold || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup) )
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
	}
	else
	{
		return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTaskGetUp::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed* pPed = GetPed();

	if (pPed && pPed->IsNetworkClone() && m_matchedSet==NMBS_WRITHE && GetState() == State_PlayingGetUpClip && GetStateFromNetwork() == State_PlayingGetUpClip)
	{
		Vector3 vUpVec = VEC3V_TO_VECTOR3(pPed->GetTransform().GetUp());
		if (vUpVec.Dot(ZAXIS) > 0.95f)
		{
			float fCurrentHeading = pPed->GetTransform().GetHeading();
			float fNetworkHeading = fwAngle::LimitRadianAngleSafe(NetworkInterface::GetLastHeadingReceivedOverNetwork(pPed));

			// Use the animated velocity to linearly interpolate the heading.
			float fDesiredHeadingChange = SubtractAngleShorter(fNetworkHeading, fCurrentHeading);

			// Don't spin too fast
			fDesiredHeadingChange = Clamp(fDesiredHeadingChange, -1.75f * fTimeStep, 1.75f * fTimeStep);

			// Interpolate towards desired heading smoothly
			pPed->SetHeading(fwAngle::LimitRadianAngleSafe(fCurrentHeading + fDesiredHeadingChange));
			pPed->SetDesiredHeading(pPed->GetCurrentHeading());
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskGetUp::ShouldUseArmedGetup(CPed* pPed)
{
	if (
		pPed->GetWeaponManager() 
		&& pPed->GetWeaponManager()->GetIsArmed() 
		&& pPed->GetWeaponManager()->GetEquippedWeapon()
		)
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskGetUp::PlayerWantsToAimOrFire(CPed* pPed)
{
	if (pPed->IsAPlayerPed() && pPed->IsLocalPlayer() && pPed->GetWeaponManager() != NULL)
	{
		const CWeapon* pWeaponUsable = pPed->GetWeaponManager()->GetEquippedWeapon();
		const CWeaponInfo* pWeaponInfo = pWeaponUsable ? pWeaponUsable->GetWeaponInfo() : NULL;

		// Can the player fire or has the player hit the input for firing?
		bool bCanPlayerFire = pPed->GetWeaponManager()->GetIsArmed() && !pPed->GetWeaponManager()->GetIsArmedMelee() && !pPed->GetWeaponManager()->GetIsNewEquippableWeaponSelected();
		bool bConsiderAttackTriggerAiming = pWeaponInfo == NULL || !pWeaponInfo->GetIsThrownWeapon();
		bool bCanLockonOnFoot = pWeaponUsable ? pWeaponUsable->GetCanLockonOnFoot() : false;
		bool bAimPressed = (pPed->GetPlayerInfo()->IsAiming(bConsiderAttackTriggerAiming) && (!pWeaponInfo || bCanLockonOnFoot || pWeaponInfo->GetCanFreeAim()));
		bool bFirePressed = pPed->GetPlayerInfo()->IsFiring();

		return (bCanPlayerFire && (bAimPressed || bFirePressed));
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskGetUp::InitMetadata( u32 mode )
{
	if(mode == INIT_SESSION)
	{
		if (CStreaming::ShouldLoadStaticData())
		{
			CNmBlendOutSetManager::Load();
			ASSERT_ONLY(CNmBlendOutSetManager::ValidateBlendOutSets());
		}
	}
}

void CTaskGetUp::ShutdownMetadata( u32 mode )
{
	if (mode == SHUTDOWN_SESSION)
	{
		if (CStreaming::GetReloadPackfilesOnRestart())
		{
			CNmBlendOutSetManager::Shutdown();
		}
	}

	if (mode == SHUTDOWN_CORE)
	{
		CNmBlendOutSetManager::Shutdown();
	}
}

u32 CTaskGetUp::Tunables::GetStartClipWaitTime(const CPed* pPed)
{ 
	return pPed && pPed->IsPlayer() ?  m_StartClipWaitTimePlayer : m_StartClipWaitTime; 
}

u32 CTaskGetUp::Tunables::GetStuckWaitTime(const CPed* UNUSED_PARAM(pPed))
{ 
	return NetworkInterface::IsGameInProgress() ? m_StuckWaitTimeMp : m_StuckWaitTime;
}

void CTaskGetUp::InitTunables()
{
	sm_bCloneGetupFix = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CLONE_GETUP_FIX", 0x63e63561), sm_bCloneGetupFix);
}
