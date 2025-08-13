// FILE :    TaskTakeOffPedVariation.h

// File header
#include "Task/Movement/TaskTakeOffPedVariation.h"

// Game headers
#include "animation/EventTags.h"
#include "animation/MoveObject.h"
#include "fwanimation/animhelpers.h"
#include "modelinfo/ModelInfo.h"
#include "Objects/ObjectIntelligence.h"
#include "Objects/objectpopulation.h"
#include "Peds/ped.h"
#include "Peds/rendering/PedVariationPack.h"
#include "scene/world/GameWorld.h"
#include "scene/RegdRefTypes.h"
#include "shaders/CustomShaderEffectTint.h"
#include "shaders/CustomShaderEffectProp.h"
#include "task/Animation/TaskAnims.h"
#include "weapons/inventory/PedWeaponManager.h"
#include "Task/Movement/TaskParachute.h"
#include "control/replay/replay.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"

AI_OPTIMISATIONS()

fwMvClipId ClonedTakeOffPedVariationInfo::m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;

////////////////////////////////////////////////////////////////////////////////
// CTaskTakeOffPedVariation
////////////////////////////////////////////////////////////////////////////////

CTaskTakeOffPedVariation::Tunables CTaskTakeOffPedVariation::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskTakeOffPedVariation, 0x8640928c);

CTaskTakeOffPedVariation::CTaskTakeOffPedVariation(fwMvClipSetId clipSetId, fwMvClipId clipIdForPed, fwMvClipId clipIdForProp,
	ePedVarComp nVariationComponent, u8 uVariationDrawableId, u8 uVariationDrawableAltId, u8 uVariationTexId, atHashWithStringNotFinal hProp,
	eAnimBoneTag nAttachBone, CObject *pProp, bool bDestroyProp)
: m_vAttachOffset(V_ZERO)
, m_qAttachOrientation(V_IDENTITY)
, m_ClipSetId(clipSetId)
, m_ClipIdForPed(clipIdForPed)
, m_ClipIdForProp(clipIdForProp)
, m_pProp(NULL)
, m_fBlendInDeltaForPed(NORMAL_BLEND_IN_DELTA)
, m_fBlendInDeltaForProp(NORMAL_BLEND_IN_DELTA)
, m_fPhaseToBlendOut(-1.0f)
, m_fBlendOutDelta(SLOW_BLEND_OUT_DELTA)
, m_fForceToApplyAfterInterrupt(0.0f)
, m_hProp(hProp)
, m_nVariationComponent(nVariationComponent)
, m_nAttachBone(nAttachBone)
, m_uRunningFlags(0)
, m_uVariationDrawableId(uVariationDrawableId)
, m_uVariationDrawableAltId(uVariationDrawableAltId)
, m_uVariationTexId(uVariationTexId)
{
	SetInternalTaskType(CTaskTypes::TASK_TAKE_OFF_PED_VARIATION);

	//! we can pass in prop so that we don't have to create a new one. This is used for the jetpack.
	m_pExistingProp = pProp;

	if(bDestroyProp)
	{
		m_uRunningFlags.SetFlag(RF_DestroyProp);
	}
}

CTaskTakeOffPedVariation::~CTaskTakeOffPedVariation()
{

}

aiTask* CTaskTakeOffPedVariation::Copy() const
{
	//Create the task.
	CTaskTakeOffPedVariation* pTask = rage_new CTaskTakeOffPedVariation(m_ClipSetId, m_ClipIdForPed, m_ClipIdForProp,
		m_nVariationComponent, m_uVariationDrawableId, m_uVariationDrawableAltId, m_uVariationTexId, m_hProp, m_nAttachBone, m_pExistingProp);
	pTask->SetAttachOffset(m_vAttachOffset);
	pTask->SetAttachOrientation(m_qAttachOrientation);
	pTask->SetBlendInDeltaForPed(m_fBlendInDeltaForPed);
	pTask->SetBlendInDeltaForProp(m_fBlendInDeltaForProp);
	pTask->SetBlendOutDelta(m_fBlendOutDelta);
	pTask->SetPhaseToBlendOut(m_fPhaseToBlendOut);
	pTask->SetForceToApplyAfterInterrupt(m_fForceToApplyAfterInterrupt);

	if(m_uRunningFlags.IsFlagSet(RF_HasVelocityInheritance))
	{
		pTask->SetVelocityInheritance(m_vVelocityInheritance);
	}
	
	return pTask;
}

void CTaskTakeOffPedVariation::CleanUp()
{
	//Detach the prop.
	DetachProp(false);

	//Destroy the prop.
	DestroyProp();

	// Make sure previous variation is restored
	if (!m_uRunningFlags.IsFlagSet(RF_HasRestoredVariation))
		ClearVariation(GetPed(), m_nVariationComponent);
}

CTask::FSM_Return CTaskTakeOffPedVariation::ProcessPreFSM()
{
	//Process the streaming.
	ProcessStreaming();

	//Check if we should process physics.
	if(ShouldProcessPhysics())
	{
		//Set the reset flag.
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
	}

#if FPS_MODE_SUPPORTED
	
	//! Block view mode switch.
	if(GetPed()->IsLocalPlayer() && GetPed()->GetPlayerInfo())
	{
		GetPed()->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}

	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		// Flag us aiming and strafing so we can use TaskMotionAiming movement
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);	// Skip the idle intro when pulling out the phone (B*2027640).
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsAiming, true);
		GetPed()->SetIsStrafing(true);

		const camFirstPersonShooterCamera* pFpsCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		bool bLookingBehind = pFpsCam && (pFpsCam->IsDoingLookBehindTransition() || pFpsCam->IsLookingBehind());
		if (!bLookingBehind)
		{
			float aimHeading = fwAngle::LimitRadianAngleSafe(camInterface::GetPlayerControlCamHeading(*GetPed()));
			GetPed()->SetDesiredHeading(aimHeading);
		}
	}
#endif

	return FSM_Continue;
}

CTask::FSM_Return CTaskTakeOffPedVariation::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Stream)
			FSM_OnUpdate
				return Stream_OnUpdate();

		FSM_State(State_TakeOffPedVariation)
			FSM_OnEnter
				TakeOffPedVariation_OnEnter();
			FSM_OnUpdate
				return TakeOffPedVariation_OnUpdate();
			FSM_OnExit
				TakeOffPedVariation_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskInfo* CTaskTakeOffPedVariation::CreateQueriableState() const
{
	ClonedTakeOffPedVariationInfo::InitParams params
	(
		m_ClipSetId, 
		m_ClipIdForPed, 
		m_ClipIdForProp, 
		m_nVariationComponent, 
		m_uVariationDrawableId, 
		m_uVariationDrawableAltId,
		m_uVariationTexId,
		m_hProp, 
		m_nAttachBone,
		m_vVelocityInheritance,
		m_vAttachOffset,
		m_qAttachOrientation,
		m_fBlendInDeltaForPed,
		m_fBlendInDeltaForProp,
		m_fPhaseToBlendOut,
		m_fBlendOutDelta,
		m_fForceToApplyAfterInterrupt,
		m_uRunningFlags.GetAllFlags()
	);

	return rage_new ClonedTakeOffPedVariationInfo(params);
}

void CTaskTakeOffPedVariation::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_TAKE_OFF_PED_VARIATION );
    ClonedTakeOffPedVariationInfo *info = static_cast<ClonedTakeOffPedVariationInfo*>(pTaskInfo);

	m_ClipSetId						= info->GetClipSetId();
	m_ClipIdForPed					= info->GetClipIdForPed();
	m_ClipIdForProp					= info->GetClipIdForProp();
	m_nVariationComponent			= info->GetVariationComponent();
	m_uVariationDrawableId			= info->GetVariationDrawableId();
	m_uVariationDrawableAltId		= info->GetVariationDrawableAltId();
	m_uVariationTexId				= info->GetVariationTexId();
	m_hProp							= info->GetPropHash();
	m_nAttachBone					= info->GetAttachBone();
	m_vAttachOffset					= info->GetAttachOffset();
	m_qAttachOrientation			= info->GetAttachOrientation();
	m_fBlendInDeltaForPed			= info->GetBlendInDeltaForPed();
	m_fBlendInDeltaForProp  		= info->GetBlendInDeltaForProp();
	m_fBlendOutDelta				= info->GetBlendOutDelta();
	m_fForceToApplyAfterInterrupt	= info->GetForceToApply();
	m_vVelocityInheritance			= info->GetVelocityInheritance();
	m_uRunningFlags					= info->GetRunningFlags();
	m_fPhaseToBlendOut				= info->GetPhaseToBlendOut();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

void CTaskTakeOffPedVariation::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTask::FSM_Return CTaskTakeOffPedVariation::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone* CTaskTakeOffPedVariation::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskTakeOffPedVariation(m_ClipSetId, m_ClipIdForPed, m_ClipIdForProp, m_nVariationComponent, m_uVariationDrawableId, m_uVariationDrawableAltId, m_uVariationTexId, m_hProp, m_nAttachBone);
}

CTaskFSMClone* CTaskTakeOffPedVariation::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskTakeOffPedVariation(m_ClipSetId, m_ClipIdForPed, m_ClipIdForProp, m_nVariationComponent, m_uVariationDrawableId, m_uVariationDrawableAltId, m_uVariationTexId, m_hProp, m_nAttachBone);
}

bool CTaskTakeOffPedVariation::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	return true;
}

bool CTaskTakeOffPedVariation::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	//Update the prop attachment.
	UpdatePropAttachment();

	return false;
}

#if !__FINAL
void CTaskTakeOffPedVariation::Debug() const
{
#if DEBUG_DRAW
#endif

	CTask::Debug();
}

const char* CTaskTakeOffPedVariation::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Stream",
		"State_TakeOffPedVariation",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskTakeOffPedVariation::Start_OnUpdate()
{
	// in rare cases the ped appearance node can remove the variation before this task runs but
	// we should still run it to create the prop and drop it on the floor...
	if(GetPed() && !GetPed()->IsNetworkClone() && !m_pExistingProp)
	{
		//Assert that the variation is active.
		taskAssert(IsVariationActive());
	}

	//Move to the stream state.
	SetState(State_Stream);

	return FSM_Continue;
}

CTask::FSM_Return CTaskTakeOffPedVariation::Stream_OnUpdate()
{
	//Check if everything has streamed in.
	if(m_ClipSetRequestHelper.IsLoaded() && m_ModelRequestHelper.HasLoaded())
	{
		//Take off the ped variation.
		SetState(State_TakeOffPedVariation);
	}

	return FSM_Continue;
}

void CTaskTakeOffPedVariation::TakeOffPedVariation_OnEnter()
{
	//Create the prop.
	CreateProp_Internal();

	//Attach the prop.
	AttachProp_Internal();

	//Clear the variation.
	ClearVariation_Internal();

	//Start the animations.
	StartAnimations();
}

CTask::FSM_Return CTaskTakeOffPedVariation::TakeOffPedVariation_OnUpdate()
{
	//Check if we failed to play the clip, or the clip has ended.
	if(!GetClipHelper() || GetClipHelper()->IsHeldAtEnd() ||
		((m_fPhaseToBlendOut >= 0.0f) && (GetClipHelper()->GetPhase() >= m_fPhaseToBlendOut)))
	{
		// We delayed this earlier, restore here
		if (!m_uRunningFlags.IsFlagSet(RF_HasRestoredVariation))
		{
			ClearVariation(GetPed(), m_nVariationComponent);
			m_uRunningFlags.SetFlag(RF_HasRestoredVariation);
		}

		//Finish the task.
		SetState(State_Finish);
	}
	//Check if an interrupt has been processed.
	else if(UpdateInterrupts())
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_WasInterrupted);

		// We delayed this earlier, restore here
		if (!m_uRunningFlags.IsFlagSet(RF_HasRestoredVariation))
		{
			ClearVariation(GetPed(), m_nVariationComponent);
			m_uRunningFlags.SetFlag(RF_HasRestoredVariation);
		}

		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we should detach the prop.
	else if(ShouldDetachProp())
	{
		//Detach the prop.
		DetachProp(true);

		//Destroy the prop.
		DestroyProp();
	}

	return FSM_Continue;
}

void CTaskTakeOffPedVariation::TakeOffPedVariation_OnExit()
{
	//Stop the clip.
	StopClip(m_fBlendOutDelta);
}

bool CTaskTakeOffPedVariation::ArePlayerControlsDisabled() const
{
	//Ensure the ped is a player.
	if(!GetPed()->IsPlayer())
	{
		return false;
	}

	return (GetPed()->GetPlayerInfo()->AreControlsDisabled());
}

void CTaskTakeOffPedVariation::AttachProp_Internal()
{
	//Attach the prop.
	Vector3 vAdditionalOffset(Vector3::ZeroType);
	Quaternion qAdditionalRotation(Quaternion::IdentityType);
	AttachProp(m_pProp, GetPed(), m_nAttachBone, m_vAttachOffset, m_qAttachOrientation, vAdditionalOffset, qAdditionalRotation);
}

void CTaskTakeOffPedVariation::AttachProp(CObject *pProp,
										  CPed *pPed, 
										  eAnimBoneTag nAttachBone, 
										  const Vec3V& vAttachOffset, 
										  const QuatV& qAttachOrientation,
										  const Vector3& vAdditionalOffset, 
										  const Quaternion& qAdditionalRotation)
{
	//Ensure the prop is valid.
	if(!pProp)
	{
		return;
	}

	if(pProp->GetIsAttached())
	{
		return;
	}

	//Get the bone index.
	s32 iBoneIndex = pPed->GetBoneIndexFromBoneTag(nAttachBone);

	//Calculate the offset.
	Vector3 vOffset(VEC3V_TO_VECTOR3(vAttachOffset));

	//Calculate the orientation.
	Quaternion qOrientation(QUATV_TO_QUATERNION(qAttachOrientation));

	//Transform the additional offset by the original orientation.
	Vector3 vAdditionalRotatedOffset = vAdditionalOffset;
	qOrientation.Transform(vAdditionalRotatedOffset);

	//Apply the offset.
	vOffset += vAdditionalRotatedOffset;

	//Apply the additional rotation.
	qOrientation.Multiply(qAdditionalRotation);

	//Attach the prop.
	pProp->AttachToPhysicalBasic(pPed, (s16)iBoneIndex, ATTACH_STATE_BASIC | ATTACH_FLAG_INITIAL_WARP, &vOffset, &qOrientation);
}

void CTaskTakeOffPedVariation::ClearVariation_Internal()
{
	ClearVariation(GetPed(), m_nVariationComponent, !ShouldDelayVariationRestore());
	
	if (!ShouldDelayVariationRestore())
		m_uRunningFlags.SetFlag(RF_HasRestoredVariation);
}

void CTaskTakeOffPedVariation::ClearVariation(CPed* pPed, ePedVarComp nVariationComponent, bool bShouldRestorePreviousVariation)
{
	// Grab the previous variation information if it was for this component
	u32 uVariationDrawableId = 0;
	u32 uVariationDrawableAltId = 0;
	u32 uVariationTexId = 0;
	u32 uVariationPaletteId = 0;

	if (bShouldRestorePreviousVariation)
	{
		CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if(pPlayerInfo)
		{
			if(pPlayerInfo->GetPreviousVariationComponent() == nVariationComponent)
			{
				pPlayerInfo->GetPreviousVariationInfo(uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId);
			}

			pPlayerInfo->ClearPreviousVariationData();
		}
	}

	pPed->SetVariation(nVariationComponent, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId, 0, false);
    pPed->FinalizeHeadBlend();
}

bool CTaskTakeOffPedVariation::ShouldDelayVariationRestore() const
{
	CPlayerInfo* pPlayerInfo = GetPed()->GetPlayerInfo();
	if(pPlayerInfo)
	{
		if(pPlayerInfo->GetPreviousVariationComponent() == m_nVariationComponent)
		{
			u32 uVariationDrawableId = 0, uVariationDrawableAltId = 0, uVariationTexId = 0, uVariationPaletteId = 0;
			pPlayerInfo->GetPreviousVariationInfo(uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId);

			// Delay 'hand' variation restore (bags, parachutes, etc) until animation has finished or we're interrupted
			if (m_nVariationComponent == PV_COMP_HAND && GetPed()->GetPedModelInfo()->GetVarInfo()->HasComponentFlagsSet(m_nVariationComponent, uVariationDrawableId, PV_FLAG_NOT_IN_CAR))
			{
				return true;
			}
		}
	}
	return false;
}

CObject *CTaskTakeOffPedVariation::CreateProp(CPed *pPed, ePedVarComp nVariationComponent, const fwModelId &modelID)
{
	//Create the input.
	CObjectPopulation::CreateObjectInput input(modelID, ENTITY_OWNEDBY_GAME, true);

	CObject *pProp = NULL;

	//Create the prop.
	taskAssert(!pProp);
	pProp = CObjectPopulation::CreateObject(input);
	taskAssertf(pProp, "Failed to create prop.");

	//Don't fade in the prop.
	pProp->GetLodData().SetResetDisabled(true);

	if(pPed->IsAPlayerPed())
	{
		//Prevent the camera shapetests from detecting this object, as we don't want the camera to pop if it passes near to the camera
		//or collision root position.
		phInst* pPropPhysicsInst = pProp->GetCurrentPhysicsInst();
		if(pPropPhysicsInst)
		{
			phArchetype* pPropArchetype = pPropPhysicsInst->GetArchetype();
			if(pPropArchetype)
			{
				//NOTE: This include flag change will apply to all new instances that use this archetype until it is streamed out.
				//We can live with this for camera purposes, but it's worth noting in case it causes a problem.
				pPropArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
			}
		}
	}

	// Determine if this is a the MP scuba prop, as we'll have to do some custom tint/texture mapping
	static atHashWithStringNotFinal s_hMPScubaProp("xm_prop_x17_scuba_tank", 0xC7530EE8);
	const fwModelId mpScubaPropModelId = GetModelIdForProp(s_hMPScubaProp);
	const bool bIsMPScubaProp = modelID == mpScubaPropModelId;

	//Check if the tint index is valid.
	u32 uTintIndex = GetTintIndexFromVariation(pPed, nVariationComponent);
	u32 uTextureIndex = GetTextureIndexFromVariation(pPed, nVariationComponent);

	// For the MP scuba prop, map the texture index to the prop tint index with an offset (due to the way the prop has been created)
	if (bIsMPScubaProp)
	{
		uTintIndex = uTextureIndex + 1;
	}

	if(uTintIndex != UINT_MAX)
	{
		// ped component palette supports 16 entries, prop palette supports 8 but it uses double height entries
		// to avoid dxt comrpession artifacts
		if (!bIsMPScubaProp)
		{
			uTintIndex = uTintIndex >> 1;
		}

		//Set the tint index.
		pProp->SetTintIndex(uTintIndex);

		fwCustomShaderEffect* pCSE = pProp->GetDrawHandler().GetShaderEffect();
		if (pCSE)
		{
			if (pCSE->GetType() == CSE_TINT)
			{
				CCustomShaderEffectTint *pCSETint = (CCustomShaderEffectTint*)pCSE;
				pCSETint->SelectTintPalette((u8)uTintIndex, pProp);
			}
			else if(pCSE->GetType() == CSE_PROP)
			{
				CCustomShaderEffectProp *pCSEProp = (CCustomShaderEffectProp*)pCSE;
				pCSEProp->SelectTintPalette((u8)uTintIndex, pProp);
				pCSEProp->SetDiffuseTexIdx((u8)uTextureIndex);
			}
		}
	}

	//Add the prop to the world.
	CGameWorld::Add(pProp, pPed->GetInteriorLocation());

	// And make sure we register it for recording
	REPLAY_ONLY( CReplayMgr::RecordObject(pProp) );

	return pProp;
}

void CTaskTakeOffPedVariation::CreateProp_Internal()
{
	//! if we have already created prop, just use this one.
	if(m_pExistingProp)
	{
		m_pProp = m_pExistingProp;
		return;
	}

	m_pProp = CreateProp(GetPed(), m_nVariationComponent, GetModelIdForProp(m_hProp));
}

void CTaskTakeOffPedVariation::DestroyProp()
{
	//Ensure the prop is valid.
	if(!m_pProp || !m_uRunningFlags.IsFlagSet(RF_DestroyProp))
	{
		return;
	}

	//Set the prop as owned by the population.
	m_pProp->SetOwnedBy(ENTITY_OWNEDBY_TEMP);

	//Set the end of life timer.
	m_pProp->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() + 30000;

	//Don't let the object be picked up.
	m_pProp->m_nObjectFlags.bCanBePickedUp = false;

	// Remove motion blur mask
	m_pProp->m_nFlags.bAddtoMotionBlurMask = false;
}

void CTaskTakeOffPedVariation::DetachProp(bool bUseVelocityInheritance)
{
	//Ensure the prop is valid.
	if(!m_pProp)
	{
		return;
	}

	//Ensure the prop is attached.
	if(!m_pProp->GetIsAttached())
	{
		m_pProp->m_nFlags.bAddtoMotionBlurMask = false;
		return;
	}

	//Detach the prop.
	m_pProp->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY|DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR);

	//Check if we should use velocity inheritance.
	if(bUseVelocityInheritance && m_uRunningFlags.IsFlagSet(RF_HasVelocityInheritance))
	{
		//Transform the velocity into world coordinates.  Note that I'm using the ped
		//transform and not the prop, since the prop transform is funky in some cases.
		Vec3V vVelocity = GetPed()->GetTransform().Transform3x3(m_vVelocityInheritance);

		//Include the ped's velocity.
		vVelocity = Add(vVelocity, VECTOR3_TO_VEC3V(GetPed()->GetVelocity()));

		//Set the velocity.
		m_pProp->SetVelocity(VEC3V_TO_VECTOR3(vVelocity));
	}
	//Check if we were interrupted.
	else if(m_uRunningFlags.IsFlagSet(RF_WasInterrupted))
	{
		//Check if the force to use after an interrupt is valid.
		if(m_fForceToApplyAfterInterrupt != 0.0f)
		{
			//Apply the force.
			Vec3V vDirection = Negate(GetPed()->GetTransform().GetForward());
			Vec3V vForce = Scale(vDirection, ScalarVFromF32(-GRAVITY));
			vForce = Scale(vForce, ScalarVFromF32(m_fForceToApplyAfterInterrupt));
			m_pProp->ApplyForceCg(VEC3V_TO_VECTOR3(vForce));
		}
	}
}

fwModelId CTaskTakeOffPedVariation::GetModelIdForProp(const atHashWithStringNotFinal &hProp)
{
	//Get the model id from the hash.
	fwModelId iModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(hProp, &iModelId);
	taskAssertf(iModelId.IsValid(), "The model id for the prop: %s is invalid.", hProp.GetCStr());
	return iModelId;
}

u32 CTaskTakeOffPedVariation::GetTintIndexFromVariation(CPed *pPed, ePedVarComp nVariationComponent)
{
	return CPedVariationPack::GetPaletteVar(pPed, nVariationComponent);
}

u32 CTaskTakeOffPedVariation::GetTextureIndexFromVariation(CPed *pPed, ePedVarComp nVariationComponent)
{	
	return CPedVariationPack::GetTexVar(pPed, nVariationComponent);
}

bool CTaskTakeOffPedVariation::IsPlayerTryingToAimOrFire() const
{
	taskAssert(!ArePlayerControlsDisabled());

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is a player.
	if(!pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!taskVerifyf(pPlayerInfo, "The player info is invalid."))
	{
		return false;
	}

	//Check if the player is aiming.
	if(pPlayerInfo->IsAiming())
	{
		return true;
	}

	//Check if the player is firing.
	if(pPlayerInfo->IsFiring())
	{
		return true;
	}

	return false;
}

bool CTaskTakeOffPedVariation::IsPlayerTryingToSprint() const
{
	taskAssert(!ArePlayerControlsDisabled());

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is a player.
	if(!pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!taskVerifyf(pPlayerInfo, "The player info is invalid."))
	{
		return false;
	}

	//Ensure the player is trying to sprint.
	float fSprintResult = pPlayerInfo->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT);
	if(fSprintResult <= 1.0f)
	{
		return false;
	}

	//Ensure the player is running.
	if(pPed->GetMotionData()->GetIsLessThanRun())
	{
		return false;
	}

	return true;
}

bool CTaskTakeOffPedVariation::IsPropAttached() const
{
	return (m_pProp && m_pProp->GetIsAttached());
}

bool CTaskTakeOffPedVariation::IsVariationActive() const
{
	//Ensure the variation is active.
	u32 uDrawableId = GetPed()->GetPedDrawHandler().GetVarData().GetPedCompIdx(m_nVariationComponent);
	if(uDrawableId != m_uVariationDrawableId)
	{
		return false;
	}
	u8 uDrawableAltId = GetPed()->GetPedDrawHandler().GetVarData().GetPedCompAltIdx(m_nVariationComponent);
	if(uDrawableAltId != m_uVariationDrawableAltId)
	{
		return false;
	}
	u8 uTexId = GetPed()->GetPedDrawHandler().GetVarData().GetPedTexIdx(m_nVariationComponent);
	if(uTexId != m_uVariationTexId)
	{
		return false;
	}

	return true;
}

void CTaskTakeOffPedVariation::ProcessStreaming()
{
	//Process the streaming for the model.
	ProcessStreamingForModel();

	//Process the streaming for the clip set.
	ProcessStreamingForClipSet();
}

void CTaskTakeOffPedVariation::ProcessStreamingForClipSet()
{
	//Request the animations.
	m_ClipSetRequestHelper.Request(m_ClipSetId);
}

void CTaskTakeOffPedVariation::ProcessStreamingForModel()
{
	//Ensure the model has not been requested.
	if(m_ModelRequestHelper.IsValid())
	{
		return;
	}

	//Ensure the model id is valid.
	fwModelId iModelId = GetModelIdForProp(m_hProp);
	if(!iModelId.IsValid())
	{
		return;
	}

	//Stream the model.
	strLocalIndex iModelRequestId = CModelInfo::AssignLocalIndexToModelInfo(iModelId);
	m_ModelRequestHelper.Request(iModelRequestId, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
}

bool CTaskTakeOffPedVariation::ShouldDetachProp() const
{
	//Assert that the clip helper is valid.
	taskAssert(GetClipHelper());
	
	//Check if a move event has come through.
	if(GetClipHelper()->IsEvent(CClipEventTags::MoveEvent))
	{
		return true;
	}

	//Check if the prop is valid.
	if(m_pProp)
	{
		//Check if a move event has come through.
		CGenericClipHelper& clipHelper = m_pProp->GetMoveObject().GetClipHelper();
		if(clipHelper.IsEvent(CClipEventTags::MoveEvent))
		{
			return true;
		}

		if(m_hProp == CTaskParachute::GetModelForParachutePack(GetPed()))
		{
			const crClip* pClip = clipHelper.GetClip();
			if(pClip && clipHelper.GetPhase() > 0.675f)
			{
				return true;
			}
		}
	}
	
	return false;
}

bool CTaskTakeOffPedVariation::ShouldProcessPhysics() const
{
	//Ensure the prop is attached.
	if(!IsPropAttached())
	{
		return false;
	}

	return true;
}

void CTaskTakeOffPedVariation::StartAnimationForPed()
{
	//Start the animation.
	StartClipBySetAndClip(GetPed(), m_ClipSetRequestHelper.GetClipSetId(), m_ClipIdForPed,
		m_fBlendInDeltaForPed, CClipHelper::TerminationType_OnFinish);

	//Force an AI update this frame.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
}

void CTaskTakeOffPedVariation::StartAnimationForProp()
{
	//Ensure the prop is valid.
	if(!m_pProp)
	{
		return;
	}

	if(m_ClipIdForProp == CLIP_ID_INVALID)
	{
		return;
	}

	//Initialize the prop's ability to animate.
	if(!m_pProp->InitAnimLazy())
	{
		return;
	}

	//Start the animation.
	m_pProp->GetMoveObject().GetClipHelper().BlendInClipBySetAndClip(m_pProp, m_ClipSetRequestHelper.GetClipSetId(),
		m_ClipIdForProp, m_fBlendInDeltaForProp, 0.0f, false, true);
}

void CTaskTakeOffPedVariation::StartAnimations()
{
	//Start the animation for the ped.
	StartAnimationForPed();

	//Start the animation for the prop.
	StartAnimationForProp();
}

bool CTaskTakeOffPedVariation::UpdateInterrupts()
{
	//Check if the player controls are disabled.
	if(ArePlayerControlsDisabled())
	{
		return false;
	}

	//Check if the player is trying to aim or fire.
	if(IsPlayerTryingToAimOrFire())
	{
		//Equip the best weapon immediately.
		CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
		if(pWeaponManager && pWeaponManager->GetRequiresWeaponSwitch())
		{
			pWeaponManager->CreateEquippedWeaponObject();
		}

		return true;
	}
	//Check if the player is trying to sprint.
	else if(IsPlayerTryingToSprint())
	{
		return true;
	}

	return false;
}

void CTaskTakeOffPedVariation::UpdatePropAttachment()
{
	//Ensure the prop is attached.
	if(!IsPropAttached())
	{
		return;
	}

	CGenericClipHelper& clipHelper = m_pProp->GetMoveObject().GetClipHelper();
	//Ensure the clip is valid.
	const crClip* pClip = clipHelper.GetClip();
	if(!pClip)
	{
		return;
	}

	//Grab the phase.
	float fPhase = clipHelper.GetPhase();

	//Calculate the additional offset.
	Vector3 vAdditionalOffset = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, 0.f, fPhase);

	//Calculate the additional rotation.
	Quaternion qAdditionalRotation = fwAnimHelpers::GetMoverTrackRotationDelta(*pClip, 0.f, fPhase);

	//Attach the prop.
	AttachProp(m_pProp, GetPed(), m_nAttachBone, m_vAttachOffset, m_qAttachOrientation, vAdditionalOffset, qAdditionalRotation);
}

#ifndef __FINAL
void CTaskTakeOffPedVariation::DebugRenderClonedInformation(void)
{
	return;
/*
	if(GetEntity())
	{
		if(GetPed())
		{
			char buffer[1024];
			sprintf(buffer, "ClipSetId = %s\nClipIdForPed = %s\nClipIdForProp = %s\nVariationComponent = %d\nVariationDrawableId = %d\nPropHash = %s\nAttachBone = %d\nAttachOffset %f %f %f\nAttach Orientation %f %f %f %f\nBlendInDeltaForPed %f\nBlendInDeltaForProp %f\nBlendOutDelta %f\nForceToBeApplied %f\n",
			m_ClipSetId.GetCStr(),
			m_ClipIdForPed.GetCStr(),
			m_ClipIdForProp.GetCStr(),
			m_nVariationComponent,
			m_uVariationDrawableId,
			m_hProp.GetCStr(),
			m_nAttachBone,
			m_vAttachOffset.GetXf(), m_vAttachOffset.GetYf(), m_vAttachOffset.GetZf(),
			m_qAttachOrientation.GetXf(), m_qAttachOrientation.GetYf(), m_qAttachOrientation.GetZf(), m_qAttachOrientation.GetWf(), 
			m_fBlendInDeltaForPed,
			m_fBlendInDeltaForProp,
			m_fBlendOutDelta,
			m_fForceToApplyAfterInterrupt);

			grcDebugDraw::Text(GetPed()->GetTransform().GetPosition(), Color_white, buffer);
		}
	}	
*/
}
#endif /* __FINAL */

CTaskFSMClone* ClonedTakeOffPedVariationInfo::CreateCloneFSMTask()
{
	return rage_new CTaskTakeOffPedVariation(m_clipSetId, m_clipIdForPed, m_clipIdForProp, m_nVariationComponent, m_uVariationDrawableId, m_uVariationDrawableAltId, m_uVariationTexId, m_hProp, m_nAttachBone );
}