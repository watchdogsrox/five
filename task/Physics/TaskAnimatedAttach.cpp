// Filename   :	TaskAnimatedAttach.cpp
// Description:	Handle attaching a ped to a CPhysical with anywhere from one limb to all four. This task
//              manages the constraints and, once attached, starts an NM task to control the ragdoll
//              beyond the simple clip.

// --- Include Files ------------------------------------------------------------

#include "animation/MovePed.h"
#include "debug/DebugScene.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/physics.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Physics/AttachTasks/TaskNMGenericAttach.h"
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Task/System/TaskTypes.h"
#include "text/TextConversion.h"

// Framework headers:
#include "fwanimation/animdirector.h"
#include "fwsys\timer.h"

// ------------------------------------------------------------------------------

AI_OPTIMISATIONS()

// Tunable parameters ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CTaskAnimatedAttach::ms_fInvMassMult = 1.0f; // 0 gives attach parent infinite mass, 1 gives it its usual mass.
bool CTaskAnimatedAttach::m_bEnableDebugDraw = false;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskAnimatedAttach::CTaskAnimatedAttach(CPhysical* pParent, s32 nParentBoneIndex, const Vector3& vOffset, const Quaternion& RotationQuaternion, u32 iAttachMask,
										 s32 ClipDict, u32 ApprochClip, u32 IdleClip,  bool bStreamAssets)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 : m_pAttachParent(RegdPhy(pParent)),
m_nBoneIndex(nParentBoneIndex),
m_vAttachOffset(vOffset),
m_RotationQuaternion(RotationQuaternion), 
m_nClipDict(ClipDict),
m_nClip(ApprochClip), 
m_nIdleClip(IdleClip), 
m_nAttachFlags(iAttachMask),
m_bStreamAssets(bStreamAssets),
m_bPosedFromIdle(false),
m_bAttachToVehicleDoor(false),
m_parentNetObjId(NETWORK_INVALID_OBJECT_ID),
m_bQuitCloneTask(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ANIMATED_ATTACH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskAnimatedAttach::CTaskAnimatedAttach(CPhysical* pParent, s32 nParentBoneIndex, u32 iAttachFlags, s32 ClipDict, u32 IdleClip)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_pAttachParent(RegdPhy(pParent)),
m_nBoneIndex(nParentBoneIndex),
m_vAttachOffset(Vector3::ZeroType),
m_RotationQuaternion(Quaternion::IdentityType), 
m_nClipDict(ClipDict),
m_nClip(0), 
m_nIdleClip(IdleClip), 
m_nAttachFlags(iAttachFlags),
m_bStreamAssets(false),
m_bPosedFromIdle(false),
m_bAttachToVehicleDoor(false),
m_parentNetObjId(NETWORK_INVALID_OBJECT_ID),
m_bQuitCloneTask(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ANIMATED_ATTACH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskAnimatedAttach::~CTaskAnimatedAttach()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskAnimatedAttach::Copy() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskAnimatedAttach* pNewTask = rage_new CTaskAnimatedAttach(m_pAttachParent, m_nBoneIndex, m_vAttachOffset, m_RotationQuaternion, m_nAttachFlags, m_nClipDict,
		m_nClip, m_nIdleClip, m_bStreamAssets);
	pNewTask->SetAttachToVehicleDoor(m_bAttachToVehicleDoor);
	return pNewTask;
}

CTaskInfo*	CTaskAnimatedAttach::CreateQueriableState() const
{
	return rage_new CClonedAnimatedAttachInfo(m_pAttachParent, m_nBoneIndex, m_nAttachFlags, m_nClipDict, m_nIdleClip);
}

CTask::FSM_Return CTaskAnimatedAttach::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	if (m_bQuitCloneTask && iEvent == OnUpdate)
	{
		SetState(State_Finish);
	}

	return UpdateFSM(iState, iEvent);
}

void CTaskAnimatedAttach::OnCloneTaskNoLongerRunningOnOwner()
{
	m_bQuitCloneTask = true;
}

bool CTaskAnimatedAttach::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	// wait for the entity the ped is attached to before continuing
	if (!m_pAttachParent)
	{
		netObject* pNetObj = NetworkInterface::GetNetworkObject(m_parentNetObjId);

		if (pNetObj)
		{
			m_pAttachParent = SafeCast(CPhysical, pNetObj->GetEntity());
		}
	}

	if (!m_bAttachToVehicleDoor && !fwAnimManager::GetClipIfExistsByDictIndex(m_nClipDict, m_nIdleClip))
	{
		return false;
	}

	return (m_pAttachParent != NULL);
}

CTaskFSMClone* CTaskAnimatedAttach::CreateTaskForClonePed(CPed *UNUSED_PARAM(pPed))
{
	return rage_new CTaskAnimatedAttach(m_pAttachParent, m_nBoneIndex, m_nAttachFlags, m_nClipDict, m_nIdleClip);
}

CTaskFSMClone*	CTaskAnimatedAttach::CreateTaskForLocalPed(CPed *pPed)
{
	return CreateTaskForClonePed(pPed);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAnimatedAttach::UpdateFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);
		
		FSM_State(State_StreamAssets)
			FSM_OnUpdate
			return StreamAssets_OnUpdate(pPed);
		
		FSM_State(State_PosingPed)
			FSM_OnEnter
				PosingPed_OnEnter(pPed);
			FSM_OnUpdate
				return PosingPed_OnUpdate(pPed);
			FSM_OnExit
				PosingPed_OnExit(pPed);

		FSM_State(State_AttachingPed)
			FSM_OnEnter
				AttachingPed_OnEnter(pPed);
			FSM_OnUpdate
				return AttachingPed_OnUpdate(pPed);
			FSM_OnExit
				AttachingPed_OnExit(pPed);

		FSM_State(State_PedAttached)
			FSM_OnEnter
				PedAttached_OnEnter(pPed);
			FSM_OnUpdate
				return PedAttached_OnUpdate(pPed);
			FSM_OnExit
				PedAttached_OnExit(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::Start_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsNetworkClone())
	{
		if (pPed->GetRagdollState() != RAGDOLL_STATE_ANIM)
		{
			nmTaskDebugf(this, "CTaskAnimatedAttach::Start_OnEnter switching to animated");
			pPed->SwitchToAnimated();
		}
	}
	else
	{
		// We can't have the ragdoll active when we are going to diddle the ped's matrix.
		taskAssert(pPed->GetRagdollState() == RAGDOLL_STATE_ANIM);
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_AttachedToVehicle, true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAnimatedAttach::Start_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Ensure Idle Clip Is Valid
#if __ASSERT
	if (!m_bAttachToVehicleDoor && !taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_nClipDict, m_nIdleClip),"NULL Idle Clip Does Not Exist"))
	{
		return FSM_Quit;
	}
#endif // __ASSERT

	//lets decide if we need to stream the clip assets, depending if this task has been called directly by script or from another task
	if(m_bStreamAssets)
	{
		SetState(State_StreamAssets);
	}
	else
	{
		if (m_nClip != 0)
			SetState(State_PosingPed);
		else 
		{
			if (!m_bPosedFromIdle)
			{
				// Force an instant clip update the first frame to ensure ped is posed correctly, regardless of if they are on screen
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
				m_bPosedFromIdle = true;
			}
			else
			{
				// Ensure the ped's in vehicle clips have fully blended in
				CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
				if (m_bAttachToVehicleDoor || (pMotionTask && pMotionTask->IsFullyInVehicle()))
				{
					SetState(State_AttachingPed);
				}
			}
		}
	}
	
	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAnimatedAttach::StreamAssets_OnUpdate(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	const bool bClipsLoaded = m_ClipSetRequestHelper.Request(m_clipSetId);

	if(bClipsLoaded)
	{
		SetState(State_PosingPed);
	}

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::PosingPed_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed)
	{
		taskAssert(m_pAttachParent.Get());
		pPed->AttachPedToPhysical(m_pAttachParent.Get(), (s16)m_nBoneIndex, ATTACH_STATE_PED|ATTACH_FLAG_INITIAL_WARP, &m_vAttachOffset, NULL, 0.0f, 0.0f);
		pPed->ProcessAllAttachments();
		pPed->GetAnimDirector()->SetPosed(false);
		int nClipFlags = APF_ISFINISHAUTOREMOVE;
		StartClipByDictAndHash( pPed, m_nClipDict, m_nClip, nClipFlags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAnimatedAttach::PosingPed_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Wait for the ped's skeleton to be updated by the playing clip.
	CSimpleBlendHelper& helper = pPed->GetMovePed().GetBlendHelper();
	CClipHelper* pPlayer = helper.FindClipByIndexAndHash(m_nClipDict, m_nClip); 
	
	if(pPlayer && pPlayer->GetPhase() < 1.0)
	{
		//lets calculate the displacement so that we can up date the attachment 
		Vector3 vAttachDisplacment = pPed->GetAnimatedVelocity() * fwTimer::GetTimeStep(); 
		m_vAttachOffset = m_vAttachOffset + vAttachDisplacment; 
		taskAssert(m_pAttachParent.Get());
		pPed->AttachPedToPhysical(m_pAttachParent.Get(), (s16)m_nBoneIndex, ATTACH_STATE_PED|ATTACH_FLAG_INITIAL_WARP, &m_vAttachOffset, NULL, 0.0f, 0.0f);
		pPed->ProcessAllAttachments();
	}
	else
	{
		SetState(State_AttachingPed);
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::PosingPed_OnExit(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::CalculateAttachPoints(atArray<sAttachedNMComponents> &sAttachComponent, CPed *pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	const crSkeletonData &pedSkelData = pPed->GetSkeletonData();
	int BoneIdx = 0; 
	Matrix34 mMatBone(M34_IDENTITY);
	sAttachedNMComponents Attach; 

	//calculate the peds bone positions in global space
	if(IsAttachFlagSet(NMCF_LEFT_HAND))
	{
		Attach.iRagdollComponent = RAGDOLL_HAND_LEFT;

		mMatBone.Identity();
		pedSkelData.ConvertBoneIdToIndex((u16)BONETAG_L_IK_HAND, BoneIdx);
		Attach.iPedBoneIndex = BoneIdx;
		pPed->GetGlobalMtx(BoneIdx, mMatBone);
		Attach.vPos = mMatBone.d;
		sAttachComponent.PushAndGrow(Attach);
	}

	if(IsAttachFlagSet(NMCF_RIGHT_HAND))
	{
		Attach.iRagdollComponent = RAGDOLL_HAND_RIGHT;

		mMatBone.Identity();
		pedSkelData.ConvertBoneIdToIndex((u16)BONETAG_R_IK_HAND, BoneIdx);
		Attach.iPedBoneIndex = BoneIdx;
		pPed->GetGlobalMtx(BoneIdx, mMatBone);
		Attach.vPos = mMatBone.d;
		sAttachComponent.PushAndGrow(Attach);
	}

	// TODO RA - revisit the foot attach points when point helpers have been added to the bone tags.
	if(IsAttachFlagSet(NMCF_RIGHT_FOOT))
	{
		Attach.iRagdollComponent = RAGDOLL_FOOT_RIGHT;

		mMatBone.Identity();
		pedSkelData.ConvertBoneIdToIndex((u16)BONETAG_R_FOOT, BoneIdx);
		Attach.iPedBoneIndex = BoneIdx;
		pPed->GetGlobalMtx(BoneIdx, mMatBone);
		Attach.vPos = mMatBone.d;
		sAttachComponent.PushAndGrow(Attach);
	}

	if(IsAttachFlagSet(NMCF_LEFT_FOOT))
	{
		Attach.iRagdollComponent = RAGDOLL_FOOT_LEFT;

		mMatBone.Identity();
		pedSkelData.ConvertBoneIdToIndex((u16)BONETAG_L_FOOT, BoneIdx);
		Attach.iPedBoneIndex = BoneIdx;
		pPed->GetGlobalMtx(BoneIdx, mMatBone);
		Attach.vPos = mMatBone.d;
		sAttachComponent.PushAndGrow(Attach);
	}
}	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::AttachingPed_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_nClip != 0)
		pPed->DetachFromParent(0); 
	
	// Remove any previous constraints on both the ped and the current attach parent.
	DetachPed(pPed, true);

	ConfigureRagdollCollision(pPed, false);
	atArray<sAttachedNMComponents> sAttachedComponents; 

	CalculateAttachPoints(sAttachedComponents, pPed); 

	// Update Ragdoll phInst's matrix to match the new ped's CEntity matrix.
	pPed->GetRagdollInst()->SetMatrix(pPed->GetMatrix());

	if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_ANIMATED_ATTACH))
	{
		pPed->SwitchToRagdoll(*this);
		
		/////////////////////////////////////////////// CREATE THE CONSTRAINTS //////////////////////////////////////////////////

		// Just make all attachments infinite strength for now.
		float fPhysicalStrength = 0.0f;

		// Now create the new constraints between each requested ragdoll component and the attach parent. We create two RAGE
		// constraints per ragdoll component - one for translation and the other for rotation.
		for(int i = 0; i < sAttachedComponents.GetCount(); i++)
		{
			if(physicsVerifyf(m_pAttachParent, "No parent entity has been defined to attach this ped (0x%p) to.", pPed))
			{
	#if __BANK
				ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(sAttachedComponents[i].vPos), 0.025f, Color_red, 5000);
	#endif 

				// Configure the attachment behaviour - code in CPhysical will manage all this for us, yay!
				u32 nPhysicalAttachFlags = 0;
				nPhysicalAttachFlags |= ATTACH_STATE_RAGDOLL;
				nPhysicalAttachFlags |= ATTACH_FLAG_POS_CONSTRAINT;
				nPhysicalAttachFlags |= ATTACH_FLAG_ROT_CONSTRAINT; // To Constrain the rotation as well as translation.
				nPhysicalAttachFlags |= ATTACH_FLAG_DO_PAIRED_COLL;
				nPhysicalAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_DEATH;
				nPhysicalAttachFlags |= ATTACH_FLAG_COL_ON;
				nPhysicalAttachFlags |= ATTACH_FLAG_DELETE_WITH_PARENT;

				taskAssert(m_pAttachParent.Get());
				pPed->AttachToPhysicalUsingPhysics(m_pAttachParent.Get(), (s16)m_nBoneIndex, (s16)sAttachedComponents[i].iPedBoneIndex,
					nPhysicalAttachFlags, sAttachedComponents[i].vPos, fPhysicalStrength);
				physicsAssert(pPed->GetAttachState()==ATTACH_STATE_RAGDOLL);
			}
		}
	}

	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAnimatedAttach::AttachingPed_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If ped is attached successfully, switch states here.

	if(pPed->GetRagdollState()==RAGDOLL_STATE_ANIM)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Inform the task system that we have control of the ragdoll from the ragdoll
	// until the corresponding NM task assumes control.
	pPed->TickRagdollStateFromTask(*this);

#if DEBUG_DRAW
	if(m_bEnableDebugDraw)
	{
		DebugVisualise(pPed);
	}
#endif //DEBUG_DRAW

	SetState(State_PedAttached);

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::AttachingPed_OnExit(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::PedAttached_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Setup NM behaviour.
	
	//Here we can kick off our attached NM behaviour. This could take the form of an enum in the constructor of the task
	CTaskNMGenericAttach* pNmTask = rage_new CTaskNMGenericAttach(m_nAttachFlags);

	CTaskNMControl* pNmControlTask = rage_new CTaskNMControl(pNmTask->GetMinTime(), pNmTask->GetMaxTime(), pNmTask, CTaskNMControl::DO_BLEND_FROM_NM);
	pNmControlTask->SetFlag(CTaskNMControl::DO_CLIP_POSE);
	pNmControlTask->GetClipPoseHelper().SetAnimPoseType(CTaskNMBehaviour::ANIM_POSE_ATTACH_TO_VEHICLE);
	pNmControlTask->GetClipPoseHelper().SetAttachParent(m_pAttachParent);

	pNmControlTask->GetClipPoseHelper().TaskSetsStiffnessValues(true);
	pNmTask->SetClipPoseHelperClip(m_nClipDict, m_nIdleClip);
	if (m_bAttachToVehicleDoor)
		pNmControlTask->GetClipPoseHelper().SelectClip(m_nClipDict, m_nIdleClip);

	if (pPed->IsNetworkClone())
	{
		pNmControlTask->SetRunningLocally(true);
		pNmControlTask->AlwaysAllowControlPassing();
	}

	SetNewTask(pNmControlTask);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAnimatedAttach::PedAttached_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Monitor for the NM subtask finishing and switch to the finish state if it does.
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodRagdollInVehicle) || m_bQuitCloneTask)
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::PedAttached_OnExit(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::DetachPed(CPed* pPed, bool bIgnoreAttachState)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	u16 nDetachFlags = 0;
	nDetachFlags |= DETACH_FLAG_ACTIVATE_PHYSICS;
	nDetachFlags |= DETACH_FLAG_APPLY_VELOCITY;
	//nDetachFlags |= DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR; // Do not collide with object until you are clear of its bounds

	if (bIgnoreAttachState)
		nDetachFlags |= DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK;
	//nDetachFlags |= DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK;

	if(pPed->GetIsAttached() && (bIgnoreAttachState || pPed->GetAttachState() != ATTACH_STATE_PED_IN_CAR))
	{
		pPed->DetachFromParent(nDetachFlags);
	}

	ConfigureRagdollCollision(pPed, true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::ConfigureRagdollCollision(CPed* pPed, bool bCollisionEnabled)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// We don't want collision between constrained limbs.

	fragInstNMGta* pFragInst = pPed->GetRagdollInst();
	u32 nCollisionMask = 0;

	if(!bCollisionEnabled)
	{
		
		if(IsAttachFlagSet(NMCF_LEFT_HAND)) nCollisionMask |= BIT(RAGDOLL_HAND_LEFT);
		if(IsAttachFlagSet(NMCF_RIGHT_HAND)) nCollisionMask |= BIT(RAGDOLL_HAND_RIGHT);
		if(IsAttachFlagSet(NMCF_LEFT_FOOT)) nCollisionMask |= BIT(RAGDOLL_FOOT_LEFT);
		if(IsAttachFlagSet(NMCF_RIGHT_FOOT)) nCollisionMask |= BIT(RAGDOLL_FOOT_RIGHT);

		if(IsAttachFlagSet(NMCF_LEFT_FOREARM)) nCollisionMask |= BIT(RAGDOLL_LOWER_ARM_LEFT);
		
		if(IsAttachFlagSet(NMCF_RIGHT_FOREARM)) nCollisionMask |= BIT(RAGDOLL_LOWER_ARM_RIGHT);
		if(IsAttachFlagSet(NMCF_LEFT_SHIN)) nCollisionMask |= BIT(RAGDOLL_SHIN_LEFT);
		if(IsAttachFlagSet(NMCF_RIGHT_SHIN)) nCollisionMask |= BIT(RAGDOLL_SHIN_RIGHT);

		if(IsAttachFlagSet(NMCF_LEFT_THIGH)) nCollisionMask |= BIT(RAGDOLL_THIGH_LEFT);
		if(IsAttachFlagSet(NMCF_RIGHT_THIGH)) nCollisionMask |= BIT(RAGDOLL_THIGH_RIGHT);
	}

	pFragInst->SetDisableCollisionMask(nCollisionMask);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::CleanUp()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed();
	
	// added this in to re-pose the character before the render to get around the nm matrix rotation problem, this should be removed
	// when this is resolved as this is expensive to do!
	TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, forceFullClipUpdate, true);
	if (forceFullClipUpdate)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		pPed->GetAnimDirector()->SetPosed(false);
	}

	if (pPed->IsNetworkClone())
	{
		if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			pPed->SetPedOutOfVehicle(CPed::PVF_DontResetDefaultTasks);	
		}
	}
	else
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_AttachedToVehicle, false);

		//if (m_nClip != 0)
		{
			DetachPed(pPed);
		}
	}

	// Tried to force a update of the local skel matrices instead of doing the full update above, but ped seems to slip forwards
	// See TaskBlendFromNM - this uses point clouds to get the clip matrix.
	// Get the root matrix which should give us the best estimate of where to place the animated ped.
// 	Matrix34 rootMatrix;
// 	pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);
// 
// 	// Copy the position from the root matrix.
// 	Matrix34 mClipMatrix(Matrix34::IdentityType);
// 	mClipMatrix.d = rootMatrix.d;
// 
// 	TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, forceLocalMatricesUpdate, false);
// 	if (forceLocalMatricesUpdate)
// 	{
// 		float fSwitchHeading =0.0f; 
// 
// 		// Get the root matrix which should give us the best estimate of where to place the animated ped.
// 		//Matrix34 rootMatrix;
// 		//pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);
// 
// 		Matrix34 rootMatrix;
// 		pPed->GetMatrixCopy(rootMatrix);
// 
// #if DEBUG_DRAW
// 		ms_debugDraw.AddLine(rootMatrix.d, rootMatrix.d + rootMatrix.a, Color_red, 3000);
// 		ms_debugDraw.AddSphere(rootMatrix.d + rootMatrix.a, 0.025f, Color_red, 3000);
// 
// 		ms_debugDraw.AddLine(rootMatrix.d, rootMatrix.d + rootMatrix.b, Color_green, 3000);
// 		ms_debugDraw.AddSphere(rootMatrix.d + rootMatrix.b, 0.025f, Color_green, 3000);
// 
// 		ms_debugDraw.AddLine(rootMatrix.d, rootMatrix.d + rootMatrix.c, Color_blue, 3000);
// 		ms_debugDraw.AddSphere(rootMatrix.d + rootMatrix.c, 0.025f, Color_blue, 3000);
// #endif 
// 
// 		TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, rotateX, false);
// 		if (rotateX)
// 		{
// 			TUNE_GROUP_FLOAT(FIRETRUCK_DEBUG, rx, HALF_PI, -PI, PI, 0.01f);
// 			rootMatrix.RotateLocalX(rx);
// 		}
// 
// 		// Copy the position from the root matrix.
// 		Matrix34 mClipMatrix = rootMatrix; 
// 
// 		TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, setClipInstance, false);
// 		if (setClipInstance)
// 		{
// 			// Set the animated instance matrix based on our findings above.
// 			pPed->SetMatrix(mClipMatrix, false, false, false);
// 			pPed->SetDesiredHeading(fSwitchHeading);
// 		}
// 
// 		// Ready the ped's skeleton for the return to clip. Start by working out a transformation matrix from ragdoll to clip space.
// 		Matrix34 ragdollToClipMtx;
// 		ragdollToClipMtx.FastInverse(mClipMatrix);
// 		ragdollToClipMtx.DotFromLeft(RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
// 		pPed->GetSkeleton()->Transform(ragdollToClipMtx);
// 
// 		pPed->GetRagdollInst()->SetMatrix(RCC_MAT34V(mClipMatrix));
// 		CPhysics::GetLevel()->UpdateObjectLocation(pPed->GetRagdollInst()->GetLevelIndex());
// 
// 		// Need to pose local matrices of skeleton because ragdoll has only been updating the global matrices.
// 		pPed->InverseUpdateSkeleton();
// 
// 		// Blend in idle to stop any movement.
// 		TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, doThis, false);
// 		if (doThis)
// 		{
// 			pPed->StopAllMotion(true);
// 			pPed->GetClipBlender()->GrabAnimFrameFromRagdoll(*pPed->GetSkeleton(), 1.0f, SLOW_BLEND_OUT_DELTA);
// 		}
// 
// 		Vector3 vecRootVelocity(VEC3_ZERO);
// 		if(pPed->GetCollider())
// 		{
// 			vecRootVelocity.Set(pPed->GetCollider()->GetVelocity());
// 		}
// 
// 		TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, doThis2, false);
// 		if (doThis2)
// 		{
// #if EXTRAPOLATE_RAGDOLL
// 			const float fDeltaTime = fwTimer::GetTimeStep() / (float)CPhysics::GetNumTimeSlices();
// 
// 			pPed->GetRagdollInst()->PoseSkeletonFromLastFrame();
// 			pPed->GetSkeleton()->Transform(ragdollToClipMtx);
// 			pPed->InverseUpdateSkeleton();
// 			pPed->GetClipBlender()->GrabPrevClipFrameFromRagdoll(*pPed->GetSkeleton(), fDeltaTime, mClipMatrix.a.Dot(vecRootVelocity),
// 				mClipMatrix.b.Dot(vecRootVelocity));
// #endif // EXTRAPOLATE_RAGDOLL
// 		}
// 
// 		// Actually make the switch to the animated instance.
// 		pPed->SwitchToAnimated(true, true, false);
// 
// 		// Try and maintain velocity in the animated ped.
// 		TUNE_GROUP_BOOL(FIRETRUCK_DEBUG, setVelocity, false);
// 		if (setVelocity)
// 			pPed->SetVelocity(vecRootVelocity);
// 
// 		pPed->GetAnimDirector()->SetPosed(false);
// 	}

}
#if DEBUG_DRAW
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::Debug() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAnimatedAttach::DebugVisualise(CPed *pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Visualise the attachment bone(s).
	static bool bDrawBones = true;
	if(bDrawBones)
	{
		const crSkeletonData &pedSkelData = pPed->GetSkeletonData();
		int rightHandBoneIdx;
		int rightFootBoneIdx; 
		pedSkelData.ConvertBoneIdToIndex((u16)BONETAG_R_IK_HAND, rightHandBoneIdx);
		pedSkelData.ConvertBoneIdToIndex((u16)BONETAG_R_FOOT, rightFootBoneIdx);
		Matrix34 handBoneMat;
		pPed->GetGlobalMtx(rightHandBoneIdx, handBoneMat);
		Matrix34 footBoneMat;
		pPed->GetGlobalMtx(rightFootBoneIdx, footBoneMat);
		ms_debugDraw.AddSphere(RCC_VEC3V(handBoneMat.d), 0.1f, Color32(0xff,0xff,0x00,0x60));
		ms_debugDraw.AddSphere(RCC_VEC3V(footBoneMat.d), 0.1f, Color32(0xff,0xff,0x00,0x60));
	}

	// Compute attach positions for the ped on the parent. Use the left hand as a guide and compute the left foot
	// relative to that.
	Vector3 vRightHandAttachPos;
	Vector3 vRightFootAttachPos;
	const crSkeletonData &skelData = m_pAttachParent->GetSkeletonData();
	crSkeleton *pSkel = m_pAttachParent->GetSkeleton();
	
	Matrix34 rootMatrix;
	pSkel->GetGlobalMtx(0, RC_MAT34V(rootMatrix));

	for(int i = 0; i < skelData.GetNumBones(); i++)
	{
		Matrix34 boneMat;
		pSkel->GetGlobalMtx(i, RC_MAT34V(boneMat));

		if(m_nBoneIndex == i)
		{
			ms_debugDraw.AddSphere(RCC_VEC3V(boneMat.d), 0.15f, Color32(200,0,0,0x80));
			vRightHandAttachPos = boneMat.d;
		}
		else
		{
			ms_debugDraw.AddSphere(RCC_VEC3V(boneMat.d), 0.15f, Color32(100,100,100,0x80));
		}
		ms_debugDraw.AddLine(RCC_VEC3V(boneMat.d), RCC_VEC3V(rootMatrix.d), Color32(200,200,100));
	}
	
	// Display a colour coded banner to show how well the ClipPose helper is matching the ragdoll to the clip.
	if(CClipPoseHelper::ms_bDebugClipMatch)
	{
		Color32 colour;
		CTextLayout ClipMatchDebugText;
		float fClipAccuracy = CClipPoseHelper::ms_fClipMatchAcc;
		float fAccThreshold = 0.05f;

		ClipMatchDebugText.SetScale(Vector2(0.5f, 1.0f));
		ClipMatchDebugText.SetBackgroundColor(CRGBA(0,0,0,128));
		if(fClipAccuracy <= fAccThreshold*0.5f)
		{
			u8 red = u8((fClipAccuracy*2.0f/fAccThreshold)*255);
			colour = Color32(red,255,0,255);
		}
		else if(fClipAccuracy <= fAccThreshold)
		{
			u8 green = u8((1.0f - (fClipAccuracy*2.0f/fAccThreshold - 1.0f))*255);
			colour = Color32(255,green,0,255);
		}
		else
		{
			colour = Color32(255,0,0,255);
		}
		char debugString[100];
		sprintf(debugString, "Ragdoll match to clip: %5.3f", fClipAccuracy);
		static s32 textX = 0, textY = 0;
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), colour, textX, textY, debugString, false);
	}
}
#endif // DEBUG_DRAW
