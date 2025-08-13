//
// camera/gameplay/follow/FollowPedCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/follow/FollowPedCamera.h"

#include "profile/group.h"
#include "profile/page.h"

#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PedMotionData.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/Default/TaskPlayer.h"
#include "task/Motion/Locomotion/TaskMotionPed.h"
#include "task/Movement/Climbing/TaskLadderClimb.h"
#include "task/Movement/Climbing/TaskRappel.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "task/Movement/TaskFall.h"
#include "task/Movement/TaskGetUp.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "vehicles/train.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFollowPedCamera,0x5495AE1)

PF_PAGE(camFollowPedCameraPage, "Camera: Follow-ped Camera");

PF_GROUP(camFollowPedCameraMetrics);
PF_LINK(camFollowPedCameraPage, camFollowPedCameraMetrics);

PF_VALUE_FLOAT(ragdollBlendLevel, camFollowPedCameraMetrics);
PF_VALUE_FLOAT(desiredCustomNearToMediumViewModeBlendLevel, camFollowPedCameraMetrics);
PF_VALUE_FLOAT(customNearToMediumViewModeBlendLevel, camFollowPedCameraMetrics);
PF_VALUE_FLOAT(desiredCustomViewModeBlendLevelForCutsceneBlendOut, camFollowPedCameraMetrics);
PF_VALUE_FLOAT(customViewModeBlendLevelForCutsceneBlendOut, camFollowPedCameraMetrics);
PF_VALUE_FLOAT(meleeBlendLevel, camFollowPedCameraMetrics);

const float g_MaxNearClipForAnimals = 0.05f;

camFollowPedCamera::camFollowPedCamera(const camBaseObjectMetadata& metadata)
: camFollowCamera(metadata)
, m_Metadata(static_cast<const camFollowPedCameraMetadata&>(metadata))
, m_RelativeAttachParentMatrixPositionForVehicleEntry(g_InvalidRelativePosition)
, m_RelativeBaseAttachPositionForRagdoll(VEC3_ZERO)
, m_AssistedMovementLookAheadRouteDirection(VEC3_ZERO)
, m_LadderMountTargetPosition(g_InvalidPosition)
, m_AssistedMovementAlignmentEnvelope(NULL)
, m_LadderAlignmentEnvelope(NULL)
, m_RagdollBlendEnvelope(NULL)
, m_CustomNearToMediumViewModeBlendEnvelope(NULL)
, m_VaultBlendEnvelope(NULL)
, m_MeleeBlendEnvelope(NULL)
, m_RagdollBlendLevel(0.0f)
, m_VaultBlendLevel(0.0f)
, m_MeleeBlendLevel(0.0f)
, m_SuperJumpBlendLevel(0.0f)
, m_AssistedMovementAlignmentBlendLevel(0.0f)
, m_AssistedMovementLocalRouteCurvature(0.0f)
, m_LadderAlignmentBlendLevel(0.0f)
, m_LadderHeading(0.0f)
, m_DiveHeight(0.0f)
, m_CustomNearToMediumViewModeBlendLevel(0.0f)
, m_IsAttachPedMountingLadder(false)
, m_WasAttachPedDismountingLadder(false)
, m_ShouldAttachToLadderMountTargetPosition(false)
, m_ShouldInhibitPullAroundOnLadder(false)
, m_ShouldInhibitCustomPullAroundOnLadder(false)
, m_ScriptForceLadder(false)
, m_ShouldUseCustomViewModeSettings(false)
{
	const camEnvelopeMetadata* envelopeMetadata =
		camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_AssistedMovementAlignment.m_AlignmentEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_AssistedMovementAlignmentEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_AssistedMovementAlignmentEnvelope, "A follow-ped camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_AssistedMovementAlignmentEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LadderAlignment.m_AlignmentEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_LadderAlignmentEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_LadderAlignmentEnvelope, "A follow-ped camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_LadderAlignmentEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_RagdollBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_RagdollBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_RagdollBlendEnvelope, "A follow-ped camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_RagdollBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_CustomNearToMediumViewModeBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_CustomNearToMediumViewModeBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		cameraAssertf(m_CustomNearToMediumViewModeBlendEnvelope, "A follow-ped camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash());
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_VaultBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_VaultBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_VaultBlendEnvelope, "A follow-ped camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_VaultBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_MeleeBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_MeleeBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_MeleeBlendEnvelope, "A follow-ped camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_MeleeBlendEnvelope->SetUseGameTime(true);
		}
	}
}

camFollowPedCamera::~camFollowPedCamera()
{
	m_NearbyVehiclesInRagdollList.Reset();

	if(m_AssistedMovementAlignmentEnvelope)
	{
		delete m_AssistedMovementAlignmentEnvelope;
	}

	if(m_LadderAlignmentEnvelope)
	{
		delete m_LadderAlignmentEnvelope;
	}

	if(m_RagdollBlendEnvelope)
	{
		delete m_RagdollBlendEnvelope;
	}

	if(m_CustomNearToMediumViewModeBlendEnvelope)
	{
		delete m_CustomNearToMediumViewModeBlendEnvelope;
	}

	if(m_VaultBlendEnvelope)
	{
		delete m_VaultBlendEnvelope;
	}

	if(m_MeleeBlendEnvelope)
	{
		delete m_MeleeBlendEnvelope;
	}
}

bool camFollowPedCamera::Update()
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypePed()) //Safety check.
	{
		const CEntity* followPed = static_cast<const CEntity*>(camInterface::FindFollowPed());
		if(followPed == NULL)
		{
			return false;
		}

		AttachTo(followPed);
	}

	if(!m_ControlHelper)
	{
		return false;
	}

	//Reset the relative orbit heading limits to the metadata defaults so that we may safely blend to custom limits.
	m_RelativeOrbitHeadingLimits.SetScaled(m_Metadata.m_RelativeOrbitHeadingLimits, DtoR);

	UpdateRagdollBlendLevel();
	UpdateVaultBlendLevel();
	UpdateClimbUpBehaviour();
	UpdateMeleeBlendLevel();
	UpdateSuperJumpBlendLevel();
	UpdateCustomVehicleEntryExitBehaviour();
	UpdateCoverExitBehaviour();
	UpdateAssistedMovementAlignment();
	UpdateLadderAlignment();
	UpdateRappellingAlignment();
	UpdateRunningShake();
	UpdateSwimmingShake();
	UpdateDivingShake();
	UpdateHighFallShake();

	const s32 numTrackedVehicles = m_NearbyVehiclesInRagdollList.GetCount();
	if(numTrackedVehicles > 0)
	{
		//Ignore the movement of the follow ped when avoiding nearby vehicles in ragdoll, as the player can pop around with high velocity.
		m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate = true;

		//Also disable automatic reorientation of the camera due to collision.
		if(m_Collision)
		{
			m_Collision->ForcePopInThisUpdate();
		}
	}

	const bool hasSucceeded = camFollowCamera::Update();

	return hasSucceeded;
}

void camFollowPedCamera::UpdateLensParameters()
{
	camFollowCamera::UpdateLensParameters();

	if(m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
		
		if(attachPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			float nearClip = Min(m_Frame.GetNearClip(), g_MaxNearClipForAnimals);

			m_Frame.SetNearClip(nearClip);			
		}
	}
}

void camFollowPedCamera::UpdateRagdollBlendLevel()
{
	const bool shouldApplyRagdollBehaviour =  m_AttachParent ? ComputeShouldApplyRagdollBehavour(*m_AttachParent) : false;

	if(m_RagdollBlendEnvelope)
	{
		m_RagdollBlendEnvelope->AutoStartStop(shouldApplyRagdollBehaviour);

		m_RagdollBlendLevel	= m_RagdollBlendEnvelope->Update();
		m_RagdollBlendLevel	= Clamp(m_RagdollBlendLevel, 0.0f, 1.0f);
	}
	else
	{
		//We don't have an envelope, so just use the state directly.
		m_RagdollBlendLevel = shouldApplyRagdollBehaviour ? 1.0f : 0.0f;
	}

	PF_SET(ragdollBlendLevel, m_RagdollBlendLevel);
}

bool camFollowPedCamera::ComputeShouldApplyRagdollBehavour(const CEntity &Entity, bool shouldIncludeAllGetUpStates)
{
	if(!Entity.GetIsTypePed())
	{
		return false;
	}

	const CPed* attachPed				= static_cast<const CPed*>(&Entity);
	const bool isAttachPedUsingRagdoll	= attachPed->GetUsingRagdoll();
	if(isAttachPedUsingRagdoll)
	{
		return true;
	}

	const bool isAttachPedDeadOrDying = attachPed->GetIsDeadOrDying();
	if(isAttachPedDeadOrDying)
	{
		return true;
	}

	const CPedIntelligence* pedIntelligence = attachPed->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
	if(!pedQueriableInterface)
	{
		return false;
	}

	//Treat crawling and (non-blended) get-up animations as ragdolling, as the ped capsule is animated away from their transform.
	const bool isAttachPedGettingUp = pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP);
	if(isAttachPedGettingUp)
	{
		if(shouldIncludeAllGetUpStates)
		{
			return true;
		}

		const s32 getUpState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_GET_UP);

		if((getUpState == CTaskGetUp::State_PlayingGetUpClip) || (getUpState == CTaskGetUp::State_Crawl))
		{
			return true;
		}
	}

	return false;
}

void camFollowPedCamera::UpdateVaultBlendLevel()
{
	bool isBonnetSlide = false;

	const bool shouldApplyVaultBehaviour = ComputeShouldApplyVaultBehavour(isBonnetSlide);

	if(m_VaultBlendEnvelope)
	{
		m_VaultBlendEnvelope->AutoStartStop(shouldApplyVaultBehaviour);
		if (isBonnetSlide)
		{
			m_VaultBlendEnvelope->OverrideAttackDuration(m_Metadata.m_VaultSlideAttackDuration);
		}

		m_VaultBlendLevel	= m_VaultBlendEnvelope->Update();
		m_VaultBlendLevel	= Clamp(m_VaultBlendLevel, 0.0f, 1.0f);
	}
	else
	{
		//We don't have an envelope, so interpolate to desired value.
		m_VaultBlendLevel = Lerp(0.25f, m_VaultBlendLevel, shouldApplyVaultBehaviour ? 1.0f : 0.0f);	// TODO: metadata?
	}
}

bool camFollowPedCamera::ComputeShouldApplyVaultBehavour(bool& isBonnetSlide) const
{
	isBonnetSlide = false;

	if(!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return false;
	}

	const CPed* attachPed			= static_cast<const CPed*>(m_AttachParent.Get());
	const bool isAttachPedClimbing	= attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing);
	const bool isAttachPedVaulting	= attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
	bool shouldApplyVaultBehaviour	= false;

	if(isAttachPedClimbing || isAttachPedVaulting)
	{
		const CPedIntelligence* pedIntelligence = attachPed->GetPedIntelligence();
		if(pedIntelligence && pedIntelligence->GetQueriableInterface())
		{
			const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
			if (pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT))
			{
				s32 sVaultState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_VAULT);
				if(sVaultState >= 0)
				{
					if (sVaultState == (s32)CTaskVault::State_Climb ||
						sVaultState == (s32)CTaskVault::State_ClamberVault ||
						sVaultState == (s32)CTaskVault::State_ClamberStand ||
						sVaultState == (s32)CTaskVault::State_Falling ||
						sVaultState == (s32)CTaskVault::State_Slide)
					{
						shouldApplyVaultBehaviour = true;
					}
				}

				const CTaskVault* pVaultTask = static_cast<const CTaskVault*>(attachPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_VAULT));
				if (pVaultTask && pVaultTask->IsVaultExitFlagSet(CTaskVault::eVEC_Slide))
				{
					isBonnetSlide = true;
				}
			}

			if (shouldApplyVaultBehaviour)
			{
				if (pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL))
				{
					s32 sFallState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_FALL);
					if(sFallState >= 0)
					{
						if (sFallState == (s32)CTaskFall::State_Landing ||
							sFallState == (s32)CTaskFall::State_CrashLanding ||
							sFallState == (s32)CTaskFall::State_GetUp)
						{
							shouldApplyVaultBehaviour = false;
						}
					}
				}
			}

			if (!shouldApplyVaultBehaviour)
			{
				if (pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_DROP_DOWN))
				{
					shouldApplyVaultBehaviour = true;
				}
			}
		}
	}

	return shouldApplyVaultBehaviour;
}

void camFollowPedCamera::UpdateClimbUpBehaviour()
{
	const bool shouldApplyClimbUpBehaviour = ComputeShouldApplyClimbUpBehaviour();

	if(shouldApplyClimbUpBehaviour)
	{
		//Inhibit look-behind in order to avoid clipping into the attach ped.
		m_ControlHelper->SetLookBehindState(false);
		m_ControlHelper->IgnoreLookBehindInputThisUpdate();
	}

	//Blend to custom relative heading limits to avoid clipping into the attach ped.

	const float desiredBlendLevel	= shouldApplyClimbUpBehaviour ? 1.0f : 0.0f;
	//NOTE: We cut, rather than blend, out of the custom relative heading limits in order to avoid interacting with any reactivation of look-behind.
	const bool shouldCutSpring		= (!shouldApplyClimbUpBehaviour || (m_NumUpdatesPerformed == 0) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_ClimbUpBlendSpringConstant;
	const float blendLevelToApply	= m_ClimbUpBlendSpring.Update(desiredBlendLevel, springConstant, m_Metadata.m_ClimbUpBlendSpringDampingRatio);

	Vector2 relativeOrbitHeadingLimitsForClimbUp;
	relativeOrbitHeadingLimitsForClimbUp.SetScaled(m_Metadata.m_RelativeOrbitHeadingLimitsForClimbUp, DtoR);

	m_RelativeOrbitHeadingLimits.Lerp(blendLevelToApply, m_RelativeOrbitHeadingLimits, relativeOrbitHeadingLimitsForClimbUp);
}

bool camFollowPedCamera::ComputeShouldApplyClimbUpBehaviour() const
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return false;
	}

	const CPed* attachPed				= static_cast<const CPed*>(m_AttachParent.Get());
	const bool isAttachPedClimbing		= attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing);
	const bool isAttachPedVaulting		= attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
	bool shouldApplyClimbUpBehaviour	= false;

	if(isAttachPedClimbing || isAttachPedVaulting)
	{
		const CPedIntelligence* pedIntelligence = attachPed->GetPedIntelligence();
		if(pedIntelligence)
		{
			const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
			if(pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT))
			{
				const s32 vaultState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_VAULT);
				if(vaultState == (s32)CTaskVault::State_Climb)
				{
					const CTaskVault* vaultTask = static_cast<const CTaskVault*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_VAULT));
					if(vaultTask)
					{
						const CClimbHandHoldDetected& handHold	= vaultTask->GetHandHold();
						const float handHoldHeight				= handHold.GetHeight();
						shouldApplyClimbUpBehaviour				= (handHoldHeight >= m_Metadata.m_MinHandHoldHeightForClimbUpBehaviour);
					}
				}
			}
		}
	}

	return shouldApplyClimbUpBehaviour;
}

void camFollowPedCamera::UpdateMeleeBlendLevel()
{
	bool isAttachPedPerformingMeleeAction = false;

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed					= static_cast<const CPed*>(m_AttachParent.Get());
	const CPedIntelligence* pedIntelligence	= attachPed->GetPedIntelligence();
	if(pedIntelligence)
	{
		const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
		if(pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE))
		{
			const s32 meleeTaskState			= pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_MELEE);
			isAttachPedPerformingMeleeAction	= (meleeTaskState == CTaskMelee::State_MeleeAction);
		}
	}

	if(m_MeleeBlendEnvelope)
	{
		m_MeleeBlendEnvelope->AutoStartStop(isAttachPedPerformingMeleeAction);

		m_MeleeBlendLevel	= m_MeleeBlendEnvelope->Update();
		m_MeleeBlendLevel	= Clamp(m_MeleeBlendLevel, 0.0f, 1.0f);
	}
	else
	{
		//We don't have an envelope, so just use the state directly.
		m_MeleeBlendLevel = isAttachPedPerformingMeleeAction ? 1.0f : 0.0f;
	}

	PF_SET(meleeBlendLevel, m_MeleeBlendLevel);
}

void camFollowPedCamera::UpdateSuperJumpBlendLevel()
{
	bool isAttachPedPerformingSuperJump = false;

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed					= static_cast<const CPed*>(m_AttachParent.Get());
	const CPedIntelligence* pedIntelligence	= attachPed->GetPedIntelligence();
	if(pedIntelligence)
	{
		const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
		if(pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_JUMP))
		{
			const CTaskJump* jumpTask = static_cast<const CTaskJump*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_JUMP));
			if (jumpTask && jumpTask->GetIsDoingSuperJump())
			{
				isAttachPedPerformingSuperJump = true;
			}
		}
	}

	m_SuperJumpBlendLevel = isAttachPedPerformingSuperJump ? 1.0f : 0.0f;
}

void camFollowPedCamera::UpdateCustomVehicleEntryExitBehaviour()
{
	const camGameplayDirector& gameplayDirector		= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState					= gameplayDirector.GetVehicleEntryExitState();
	const s32 vehicleEntryExitStateOnPreviousUpdate	= gameplayDirector.GetVehicleEntryExitStateOnPreviousUpdate();

	if((vehicleEntryExitState == camGameplayDirector::EXITING_VEHICLE) && (vehicleEntryExitStateOnPreviousUpdate != camGameplayDirector::EXITING_VEHICLE) && !m_HintHelper)
	{
		//Auto-level the follow pitch on starting to exit a vehicle.
		float fEntryExitPitch = (m_ControlHelper == NULL || m_ControlHelper->GetViewMode() < camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM) ? m_Metadata.m_VehicleEntryExitPitchDegrees * DtoR : 0.0f;
		float fEntryExitSmoothRate = m_Metadata.m_VehicleEntryExitPitchLevelSmoothRate;
		if(fEntryExitPitch != 0.0f && m_NumUpdatesPerformed == 0)
		{
			fEntryExitSmoothRate = 1.0f;
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
		}
		SetRelativePitch(fEntryExitPitch, m_Metadata.m_VehicleEntryExitPitchLevelSmoothRate, false);
	}

	if(m_Metadata.m_ShouldIgnoreCollisionWithFollowVehicle && (vehicleEntryExitState == camGameplayDirector::OUTSIDE_VEHICLE))
	{
		//If the follow ped is outside a vehicle as far as our entry/exit state is concerned, double-check if the ped is jacking a ped from outside or
		//picking or pulling up a bike.
		//If so, we should ignore occlusion with and push beyond the vehicle.

		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		const CPed* attachPed							= (CPed*)m_AttachParent.Get();
		const CPedIntelligence* attachPedIntelligence	= attachPed->GetPedIntelligence();
		if(attachPedIntelligence)
		{
			const CQueriableInterface* attachPedQueriableInterface = attachPedIntelligence->GetQueriableInterface();
			if(attachPedQueriableInterface)
			{
				const bool isAttachPedEnteringVehicle = attachPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true);
				if(isAttachPedEnteringVehicle)
				{
					const s32 enterVehicleState = attachPedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);

					if((enterVehicleState == CTaskEnterVehicle::State_JackPedFromOutside) || (enterVehicleState == CTaskEnterVehicle::State_PickUpBike) ||
						(enterVehicleState == CTaskEnterVehicle::State_PullUpBike))
					{
						const CVehicle* attachPedVehicle = attachPed->GetVehiclePedEntering();
						if(attachPedVehicle)
						{
							m_Collision->IgnoreEntityThisUpdate(*attachPedVehicle);
							m_Collision->PushBeyondEntityIfClippingThisUpdate(*attachPedVehicle);
						}
					}
				}
			}
		}
	}
}

void camFollowPedCamera::UpdateCoverExitBehaviour()
{
	//Ignore the movement of the attach ped when exiting cover, persisting the orientation of the cover camera by default. 

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed		= (CPed*)m_AttachParent.Get();
	const bool isExitingCover	= attachPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_COVER);
	if(isExitingCover)
	{
		m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate = true;
	}
}

void camFollowPedCamera::UpdateAssistedMovementAlignment()
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed			= (CPed*)m_AttachParent.Get();
	const CTaskMovePlayer* moveTask	= (const CTaskMovePlayer*)attachPed->GetPedIntelligence()->FindTaskActiveMovementByType(
										CTaskTypes::TASK_MOVE_PLAYER);

	const CPlayerAssistedMovementControl* assistedMovementControl	= moveTask ? moveTask->GetAssistedMovementControl() : NULL;
	const Vector3& lookAheadRouteDirection							= assistedMovementControl ?
																		assistedMovementControl->GetLookAheadRouteDirection() : VEC3_ZERO;
	const bool shouldPullAroundForAssistedMovement					= m_Metadata.m_AssistedMovementAlignment.m_ShouldAlign &&
																		assistedMovementControl && assistedMovementControl->GetIsUsingRoute() &&
																		(lookAheadRouteDirection.Mag2() >= VERY_SMALL_FLOAT);
	if(shouldPullAroundForAssistedMovement)
	{
		m_AssistedMovementLookAheadRouteDirection	= lookAheadRouteDirection;
 		m_AssistedMovementLocalRouteCurvature		= Abs(assistedMovementControl->GetLocalRouteCurvature());
	}

	if(m_AssistedMovementAlignmentEnvelope)
	{
		m_AssistedMovementAlignmentEnvelope->AutoStartStop(shouldPullAroundForAssistedMovement);
		m_AssistedMovementAlignmentBlendLevel = m_AssistedMovementAlignmentEnvelope->Update();
	}
	else
	{
		//We don't have an envelope, so just use the state directly.
		m_AssistedMovementAlignmentBlendLevel = shouldPullAroundForAssistedMovement ? 1.0f : 0.0f;
	}

	//NOTE: We scale the blend level based upon the local route curvature, as we want to pull-around strongly to sharp turns and largely
	//ignore straight routes.
	const float routeCurvatureForMaxPullAround	= (m_Metadata.m_AssistedMovementAlignment.m_RouteCurvatureForMaxPullAround * DtoR);
	const float routeCurvatureScaling			= (routeCurvatureForMaxPullAround < SMALL_FLOAT) ? 1.0f :
													Min(m_AssistedMovementLocalRouteCurvature / routeCurvatureForMaxPullAround, 1.0f);
	m_AssistedMovementAlignmentBlendLevel		*= routeCurvatureScaling;

	//We must disable reorientation of the camera due to collision when we are aligning to an assisted movement route, in order to avoid fighting.
	if(m_Collision && (m_AssistedMovementAlignmentBlendLevel >= m_Metadata.m_AssistedMovementAlignment.m_MinBlendLevelToForceCollisionPopIn))
	{
		m_Collision->ForcePopInThisUpdate();
	}
}

void camFollowPedCamera::UpdateLadderAlignment()
{
 	const bool wasAttachedToLadderMountTargetPositionOnPreviousUpdate	= m_ShouldAttachToLadderMountTargetPosition;
	const bool wasAttachPedDismountingLadderOnPreviousUpdate			= m_WasAttachPedDismountingLadder;

	m_IsAttachPedMountingLadder					= false;
	m_ShouldAttachToLadderMountTargetPosition	= false;

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed				= (CPed*)m_AttachParent.Get();
	const CTaskClimbLadder* ladderTask	= static_cast<CTaskClimbLadder*>(attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER));
	const bool shouldAlign				= m_Metadata.m_LadderAlignment.m_ShouldAlign &&
											( ( ( ladderTask ) && ( ladderTask->GetState() != CTaskClimbLadder::State_Initial ) && ( ladderTask->GetState() != CTaskClimbLadder::State_Blocked ) && ( ladderTask->GetState() != CTaskClimbLadder::State_WaitForNetworkAfterInitialising ) ) || 
											m_ScriptForceLadder);
	
	// Flag has to be set each frame by script.
	m_ScriptForceLadder = false;

	if(shouldAlign)
	{
		if(ladderTask)
		{
			m_LadderHeading					= ladderTask->GetLadderHeading();
			m_LadderMountTargetPosition		= ladderTask->GetPositionToEmbarkLadder();

			const s32 ladderTaskState		= ladderTask->GetState();
			m_IsAttachPedMountingLadder		= (ladderTaskState == CTaskClimbLadder::State_MountingAtTop) ||
												(ladderTaskState == CTaskClimbLadder::State_MountingAtBottom);
			if(ladderTaskState == CTaskClimbLadder::State_MountingAtBottom)
			{
				m_ShouldAttachToLadderMountTargetPosition = ladderTask->WasGettingOnBehind();
			}

			m_WasAttachPedDismountingLadder	= (ladderTaskState == CTaskClimbLadder::State_DismountAtTop) ||
												(ladderTaskState == CTaskClimbLadder::State_DismountAtBottom) ||
												(ladderTaskState == CTaskClimbLadder::State_SlideDismount);
		}
		else
		{
			//Fall-back to the attach ped's heading if we can't access the ladder task, which might occur when spectating in a network game.
			m_LadderHeading = attachPed->GetCurrentHeading();
		}
	}

	if(m_LadderAlignmentEnvelope)
	{
		m_LadderAlignmentEnvelope->AutoStartStop(shouldAlign);
		m_LadderAlignmentBlendLevel = m_LadderAlignmentEnvelope->Update();
		m_LadderAlignmentBlendLevel = SlowInOut(m_LadderAlignmentBlendLevel);
	}
	else
	{
		//We don't have an envelope, so just use the state directly.
		m_LadderAlignmentBlendLevel = shouldAlign ? 1.0f : 0.0f;
	}

	if(m_LadderAlignmentBlendLevel >= SMALL_FLOAT)
	{
		if(m_WasAttachPedDismountingLadder && !wasAttachPedDismountingLadderOnPreviousUpdate)
		{
			//Auto-level the follow pitch on dismounting a ladder.
			SetRelativePitch(0.0f, m_Metadata.m_LadderAlignment.m_PitchLevelSmoothRate, false);
		}

		bool shouldInhibitCustomPullAroundDueToCollision = false;

		if(m_Collision)
		{
			//We must disable reorientation of the camera due to collision when we are aligning to a ladder, in order to avoid fighting.
			m_Collision->ForcePopInThisUpdate();

			//Block pull-around to a custom orientation when the camera is constrained too closely to the root.
			const float orbitDistanceRatioAfterCollision	= m_Collision->GetOrbitDistanceRatioOnPreviousUpdate();
			shouldInhibitCustomPullAroundDueToCollision		= (orbitDistanceRatioAfterCollision <
																m_Metadata.m_LadderAlignment.m_MinOrbitDistanceRatioAfterCollisionForCustomPullAround);
		}

		if(shouldInhibitCustomPullAroundDueToCollision)
		{
			//Inhibit custom pull-around and instead pull-around to the ladder heading, in the hope of rotating away from collision.
			m_ShouldInhibitCustomPullAroundOnLadder	= true;
			m_ShouldInhibitPullAroundOnLadder		= false;
		}
		else if(m_ShouldInhibitCustomPullAroundOnLadder)
		{
			//NOTE: We allow pull-around to resume if the user looks around. We do not revert to this behaviour automatically though,
			//as that could result in oscillation.
			const bool isLookingAround = m_ControlHelper && m_ControlHelper->IsLookAroundInputPresent();
			if(isLookingAround)
			{
				m_ShouldInhibitCustomPullAroundOnLadder	= false;
				m_ShouldInhibitPullAroundOnLadder		= false;
			}
			else
			{
				//The camera is no longer too close to collision, but we need to avoid oscillating back and forth, so disable all pull-around for now.
				m_ShouldInhibitPullAroundOnLadder = true;
			}
		}

		//Inhibit look-behind when aligning to a ladder.
		//NOTE: m_ControlHelper has already been validated.
		m_ControlHelper->SetLookBehindState(false);
		m_ControlHelper->IgnoreLookBehindInputThisUpdate();
	}
	else
	{
		m_ShouldInhibitCustomPullAroundOnLadder = false;
		m_ShouldInhibitPullAroundOnLadder		= false;
	}

	if(m_ShouldAttachToLadderMountTargetPosition && !wasAttachedToLadderMountTargetPositionOnPreviousUpdate)
	{
		//Cut to the ideal (ladder) heading when initially attaching to the ladder mount position. This is used to avoid issues with the camera
		//rotating around the side of the ladder and constraining badly when mounting from behind.
		m_RelativeOrbitHeadingLimits.Zero();

		//Flag the cut.
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

		//Ensure that any interpolation is stopped immediately, to ensure a clean cut.
		StopInterpolating();
	}
	else
	{
		//Blend to custom relative heading limits when aligning to a ladder. This prevents the camera passing through the ladder and follow ped.

		Vector2 relativeOrbitHeadingLimitsForLadder;
		relativeOrbitHeadingLimitsForLadder.SetScaled(m_Metadata.m_LadderAlignment.m_RelativeOrbitHeadingLimits, DtoR);

		m_RelativeOrbitHeadingLimits.Lerp(m_LadderAlignmentBlendLevel, m_RelativeOrbitHeadingLimits, relativeOrbitHeadingLimitsForLadder);
	}
}

void camFollowPedCamera::UpdateRappellingAlignment()
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	CPed* attachPed							= (CPed*)m_AttachParent.Get();
	CQueriableInterface* queriableInterface	= attachPed->GetPedIntelligence()->GetQueriableInterface();
	const bool shouldAlign					= m_Metadata.m_RappellingAlignment.m_ShouldAlign && queriableInterface &&
												queriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL);
	if(!shouldAlign)
	{
		return;
	}

	//Auto-level the camera upon landing/detaching from rope.
	const s32 taskState		= queriableInterface->GetStateForTaskType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL);
	const bool hasLanded	= (taskState >= CTaskHeliPassengerRappel::State_DetachFromRope);
	if(hasLanded)
	{
		SetRelativePitch(0.0f, m_Metadata.m_RappellingAlignment.m_PitchLevelSmoothRate, false);
	}
}

void camFollowPedCamera::UpdateRunningShake()
{
	const camFollowPedCameraMetadataRunningShakeSettings& settings = m_Metadata.m_RunningShakeSettings;
	if (!settings.m_ShakeRef)
	{
		return;
	}

	float fMinRunShakeAmp = settings.m_MinAmplitude;
	float fMaxRunShakeAmp = settings.m_MaxAmplitude;
	if (m_AttachParent.Get() && fMinRunShakeAmp > 0.0f && fMaxRunShakeAmp > fMinRunShakeAmp)
	{
		const CPed* attachPed = (const CPed*)m_AttachParent.Get();
		const CPedMotionData* attachPedMotionData = attachPed->GetMotionData();
		if (attachPedMotionData)
		{
			const float c_fShakeBlendStart = MBR_SPRINT_BOUNDARY;
			const float c_fShakeBlendEnd   = MBR_SPRINT_BOUNDARY + 0.75f;
			cameraAssertf(c_fShakeBlendEnd > c_fShakeBlendStart, "Invalid run shake boundaries");

			float moveBlendRatio = attachPedMotionData->GetCurrentMoveBlendRatio().Mag();
			float fRunRatio = -1.0f;

			const float attachPedSpeed2	= attachPed->GetVelocity().Mag2();
			const bool isPhysicallyRunning = (attachPedSpeed2 >= (MBR_RUN_BOUNDARY * MBR_RUN_BOUNDARY));
			const bool isJumping = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping);
			const bool isInAir   = attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);
			const bool isFalling = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling);
			const bool bIsShakeValid = isPhysicallyRunning && !attachPed->GetIsSwimming() && !attachPed->GetIsParachuting() && !isJumping && !isInAir && !isFalling;
			if (bIsShakeValid)
			{
				if (moveBlendRatio >= MBR_RUN_BOUNDARY && moveBlendRatio <= c_fShakeBlendStart)
				{
					// Do run shake.
					fRunRatio = 0.0f;
				}
				else if (moveBlendRatio > c_fShakeBlendStart)
				{
					fRunRatio = Min((moveBlendRatio - c_fShakeBlendStart) / (c_fShakeBlendEnd - c_fShakeBlendStart), 1.0f);
				}
			}

			camBaseFrameShaker* pRunShaker = FindFrameShaker(settings.m_ShakeRef);
			if (!pRunShaker && fRunRatio >= 0.0f)
			{
				pRunShaker = Shake(settings.m_ShakeRef, 1.0f);
			}

			if (pRunShaker)
			{
				float fDesiredFrequencyScale = 1.0f;
				if (fRunRatio >= 0.0f)
				{
					const bool shouldCutSpring	= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
													m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
					fRunRatio = m_RunRatioSpring.Update(fRunRatio, shouldCutSpring ? 0.0f : settings.m_RunRatioSpringConstant);

					float fAmplitude = Lerp(fRunRatio, fMinRunShakeAmp, fMaxRunShakeAmp);
					pRunShaker->SetAmplitude( fAmplitude );

					if(pRunShaker->GetIsClassId(camOscillatingFrameShaker::GetStaticClassId()))
					{
						fDesiredFrequencyScale = 1.0f + fRunRatio * settings.m_MaxFrequencyScale;
						static_cast<camOscillatingFrameShaker*>(pRunShaker)->SetFreqScale( fDesiredFrequencyScale );
					}
				}
				else
				{
					pRunShaker->Release();

					float fAmplitude = Lerp(pRunShaker->GetReleaseLevel(), pRunShaker->GetAmplitude(), fMinRunShakeAmp);
					pRunShaker->SetAmplitude( fAmplitude );
				}
			}
		}
	}
}

void camFollowPedCamera::UpdateSwimmingShake()
{
	const camFollowPedCameraMetadataSwimmingShakeSettings& settings = m_Metadata.m_SwimmingShakeSettings;
	if (!settings.m_ShakeRef)
	{
		return;
	}

	float fMinSwimShakeAmp = settings.m_MinAmplitude;
	float fMaxSwimShakeAmp = settings.m_MaxAmplitude;
	if (m_AttachParent.Get() && fMinSwimShakeAmp > 0.0f && fMaxSwimShakeAmp > fMinSwimShakeAmp)
	{
		const CPed* attachPed = (const CPed*)m_AttachParent.Get();
		const CPedMotionData* attachPedMotionData = attachPed->GetMotionData();
		if (attachPedMotionData)
		{
			const float c_fShakeBlendStart = Max(MBR_WALK_BOUNDARY, 0.95f);
			const float c_fShakeBlendEnd   = MBR_SPRINT_BOUNDARY;
			cameraAssertf(c_fShakeBlendEnd > c_fShakeBlendStart, "Invalid swim shake boundaries");

			float fMoveRatio = -1.0f;

			const bool isPhysicallySwimming = attachPed->GetIsSwimming();
			if (isPhysicallySwimming)
			{
				// If swimming and above a threshold speed, then apply swim shake.
				const float attachPedSpeed2dSq= attachPed->GetVelocity().XYMag2();
				if (attachPedSpeed2dSq >= (MBR_WALK_BOUNDARY * MBR_WALK_BOUNDARY))
				{
					float moveBlendRatio = attachPedMotionData->GetCurrentMoveBlendRatio().Mag();
					if (moveBlendRatio >= c_fShakeBlendStart)
					{
						fMoveRatio = Min((moveBlendRatio - c_fShakeBlendStart) / (c_fShakeBlendEnd - c_fShakeBlendStart), 1.0f);
					}
				}
			}

			camBaseFrameShaker* pSwimShaker = FindFrameShaker(settings.m_ShakeRef);
			if (!pSwimShaker && fMoveRatio >= 0.0f)
			{
				pSwimShaker = Shake(settings.m_ShakeRef, 1.0f);
			}

			if (pSwimShaker)
			{
				float fDesiredFrequencyScale = 1.0f;
				if (fMoveRatio >= 0.0f)
				{
					const bool shouldCutSpring	= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
													m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
					fMoveRatio = m_RunRatioSpring.Update(fMoveRatio, shouldCutSpring ? 0.0f : settings.m_SwimRatioSpringConstant);

					float fAmplitude = Lerp(fMoveRatio, fMinSwimShakeAmp, fMaxSwimShakeAmp);
					pSwimShaker->SetAmplitude( fAmplitude );

					if(pSwimShaker->GetIsClassId(camOscillatingFrameShaker::GetStaticClassId()))
					{
						fDesiredFrequencyScale = 1.0f + fMoveRatio * settings.m_MaxFrequencyScale;
						CTaskMotionBase* motionTask	= attachPed->GetCurrentMotionTask();
						if (!attachPed->GetUsingRagdoll() && motionTask && motionTask->IsUnderWater())
						{
							fDesiredFrequencyScale *= settings.m_UnderwaterFrequencyScale;
						}

						static_cast<camOscillatingFrameShaker*>(pSwimShaker)->SetFreqScale( fDesiredFrequencyScale );
					}
				}
				else
				{
					pSwimShaker->Release();

					float fAmplitude = Lerp(pSwimShaker->GetReleaseLevel(), pSwimShaker->GetAmplitude(), fMinSwimShakeAmp);
					pSwimShaker->SetAmplitude( fAmplitude );
				}
			}
		}
	}
}

void camFollowPedCamera::UpdateDivingShake()
{
	const camFollowPedCameraMetadataDivingShakeSettings& settings = m_Metadata.m_DivingShakeSettings;
	if (!settings.m_ShakeRef)
	{
		return;
	}

	bool bWasDiving = (m_DiveHeight > 0.0f);
	bool bIsDiving  = false;
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	const CTask* pTask = attachPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_FALL);
	float fTaskFallHeight = 0.0f;
	if(pTask)
	{
		const CTaskFall* pFallTask = static_cast<const CTaskFall*>(pTask);
		bIsDiving = (pFallTask->GetState() == CTaskFall::State_Dive);
		fTaskFallHeight = pFallTask->GetMaxFallDistance();
	}

	camBaseFrameShaker* pDiveShaker = FindFrameShaker(settings.m_ShakeRef);
	if (bIsDiving)
	{
		if (!bWasDiving)
		{
			// Get dive height.
			m_DiveHeight = fTaskFallHeight;
		}

		float fHeightInterp = (m_DiveHeight - settings.m_MinHeight) / (settings.m_MaxHeight - settings.m_MinHeight);
		fHeightInterp = Clamp(fHeightInterp, 0.0f, 1.0f);
		float fHeightScale = Lerp(fHeightInterp, settings.m_MinAmplitude, settings.m_MaxAmplitude);
		if (!pDiveShaker)
		{
			pDiveShaker = Shake(settings.m_ShakeRef, fHeightScale);
		}
	}
	else
	{
		m_DiveHeight = 0.0f;
		if (pDiveShaker)
		{
			pDiveShaker->Release();
		}
	}
}

void camFollowPedCamera::UpdateHighFallShake()
{
	const camFollowPedCameraMetadataHighFallShakeSettings& settings = m_Metadata.m_HighFallShakeSettings;
	if (!settings.m_ShakeRef)
	{
		return;
	}

	camBaseFrameShaker* pHighFallShaker = FindFrameShaker(settings.m_ShakeRef);

	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	if (attachPed && m_RagdollBlendLevel >= SMALL_FLOAT && ComputeIsAttachParentInAir())
	{
		bool isGetOutTaskActive	= false;
		CQueriableInterface* queriableInterface = (attachPed->GetPedIntelligence()) ? attachPed->GetPedIntelligence()->GetQueriableInterface() : NULL;
		if (queriableInterface)
		{
			// Do not apply shake while ejecting/exiting vehicle.
			isGetOutTaskActive = queriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE);
		}

		float fSpeedSq = attachPed->GetVelocity().Mag2();
		if (!isGetOutTaskActive && fSpeedSq > settings.m_MinSpeed*settings.m_MinSpeed)
		{
			float fSpeed = Sqrtf(fSpeedSq);

			float fSpeedInterp = (fSpeed - settings.m_MinSpeed) / (settings.m_MaxSpeed - settings.m_MinSpeed);
			fSpeedInterp = Clamp(fSpeedInterp, 0.0f, 1.0f);
			float fSpeedScale = Lerp(fSpeedInterp, 0.0f, settings.m_MaxAmplitude);
			if (!pHighFallShaker)
			{
				pHighFallShaker = Shake(settings.m_ShakeRef, fSpeedScale);
			}
			else
			{
				pHighFallShaker->SetAmplitude(fSpeedScale);
			}
		}
		else if (pHighFallShaker)
		{
			pHighFallShaker->Release();
		}
	}
	else if (pHighFallShaker)
	{
		pHighFallShaker->Release();
	}
}

void camFollowPedCamera::UpdateControlHelper()
{
	camFollowCamera::UpdateControlHelper();

	const bool isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch	= camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch();
	m_ShouldUseCustomViewModeSettings										= m_Metadata.m_ShouldUseCustomFramingForViewModes && !isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch;
	if(!m_ShouldUseCustomViewModeSettings)
	{
		return;
	}

	//Blend between the custom settings specified for the view modes.

	//NOTE: m_ControlHelper has already been validated.
	const float* viewModeBlendLevels = m_ControlHelper->GetViewModeBlendLevels();
	if(!viewModeBlendLevels)
	{
		m_ShouldUseCustomViewModeSettings = false;
		return;
	}

	//Apply any custom modification to the blend levels.

	float viewModeBlendLevelsToApply[camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS];

	sysMemCpy(viewModeBlendLevelsToApply, viewModeBlendLevels, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));

	UpdateCustomViewModeBlendLevels(viewModeBlendLevelsToApply);

	ComputeBlendedCustomViewModeSettings(m_Metadata.m_CustomFramingForViewModes, viewModeBlendLevelsToApply, m_CustomViewModeSettings);
	ComputeBlendedCustomViewModeSettings(m_Metadata.m_CustomFramingForViewModesInTightSpace, viewModeBlendLevelsToApply, m_CustomViewModeSettingsInTightSpace);
	ComputeBlendedCustomViewModeSettings(m_Metadata.m_CustomFramingForViewModesAfterAiming, viewModeBlendLevelsToApply, m_CustomViewModeSettingsAfterAiming);
	ComputeBlendedCustomViewModeSettings(m_Metadata.m_CustomFramingForViewModesAfterAimingInTightSpace, viewModeBlendLevelsToApply, m_CustomViewModeSettingsAfterAimingInTightSpace);
}

void camFollowPedCamera::UpdateCustomViewModeBlendLevels(float* viewModeBlendLevels)
{
	const float nearBlendLevel							= viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_NEAR];
	const bool wasRenderingThisCameraOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldCutSpring							= (!wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const bool shouldCutNearBlend						= ((nearBlendLevel < SMALL_FLOAT) || shouldCutSpring);

	const float desiredNearBlendLevel	= ComputeDesiredCustomNearToMediumViewModeBlendLevel(shouldCutNearBlend);

	const float baseAttachSpeed2		= m_BaseAttachVelocityToConsider.Mag2();
	const bool shouldUpdateNearBlend	= (shouldCutNearBlend || (baseAttachSpeed2 >= (m_Metadata.m_MinAttachSpeedToUpdateCustomNearToMediumViewModeBlendLevel *
											m_Metadata.m_MinAttachSpeedToUpdateCustomNearToMediumViewModeBlendLevel)));

	float springConstant;
	if(shouldCutNearBlend)
	{
		springConstant = 0.0f;
	}
	else
	{
		const float blendLevelDelta	= (desiredNearBlendLevel - m_CustomNearToMediumViewModeBlendLevel);
		springConstant				= (blendLevelDelta >= 0.0f) ? m_Metadata.m_CustomNearToMediumViewModeBlendInSpringConstant :
										m_Metadata.m_CustomNearToMediumViewModeBlendOutSpringConstant;
	}

	m_CustomNearToMediumViewModeBlendLevel	= m_CustomNearToMediumViewModeBlendSpring.Update(shouldUpdateNearBlend ?
												desiredNearBlendLevel : m_CustomNearToMediumViewModeBlendLevel,
												springConstant, m_Metadata.m_CustomNearToMediumViewModeBlendSpringDampingRatio, true);

	if((m_CustomNearToMediumViewModeBlendLevel >= SMALL_FLOAT) && (nearBlendLevel >= SMALL_FLOAT))
	{
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_NEAR]	= (nearBlendLevel * (1.0f - m_CustomNearToMediumViewModeBlendLevel));
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM]	+= (nearBlendLevel * m_CustomNearToMediumViewModeBlendLevel);
	}

	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	if(m_HintHelper && gameplayDirector.IsScriptForcingHintCameraBlendToFollowPedMediumViewMode())
	{
		const float hintBlendLevel = gameplayDirector.GetHintBlend();

		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_NEAR]	*= (1.0f - hintBlendLevel);
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM]	= Lerp(hintBlendLevel,
																						viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM], 1.0f);
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_FAR]		*= (1.0f - hintBlendLevel);
#if FPS_MODE_SUPPORTED
		viewModeBlendLevels[camControlHelperMetadataViewMode::FIRST_PERSON]			*= (1.0f - hintBlendLevel);
#endif // FPS_MODE_SUPPORTED
	}

	const camBaseDirector::eRenderStates cutsceneRenderState	= camInterface::GetCutsceneDirector().GetRenderState();
	const bool isBlendingOutOfCutscene							= (cutsceneRenderState == camBaseDirector::RS_INTERPOLATING_OUT);
	const float desiredCutsceneBlendLevel						= isBlendingOutOfCutscene ? 1.0f : 0.0f;
	//NOTE: We need only blend out.
	//NOTE: We intentionally avoid cutting the cutscene blend based upon the camera cut flags, as we need to be extra careful to avoid popping the view mode if
	//a script happens to set the relative orientation of this camera immediately after the cutscene blend out has finished.
	const bool shouldCutCutsceneBlend							= (isBlendingOutOfCutscene || !wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0));

	const bool shouldUpdateCutsceneBlend	= (shouldCutCutsceneBlend || (baseAttachSpeed2 >= (m_Metadata.m_MinAttachSpeedToUpdateCustomViewModeBlendForCutsceneBlendOut *
												m_Metadata.m_MinAttachSpeedToUpdateCustomViewModeBlendForCutsceneBlendOut)));

	springConstant = shouldCutCutsceneBlend ? 0.0f : m_Metadata.m_CustomViewModeBlendForCutsceneBlendOutSpringConstant;

	const float cutsceneBlendLevelOnPreviousUpdate = m_CustomViewModeBlendForCutsceneBlendOutSpring.GetResult();

	const float cutsceneBlendLevel	= m_CustomViewModeBlendForCutsceneBlendOutSpring.Update(shouldUpdateCutsceneBlend ? desiredCutsceneBlendLevel :
										cutsceneBlendLevelOnPreviousUpdate, springConstant, m_Metadata.m_CustomViewModeBlendForCutsceneBlendOutSpringDampingRatio, true);

	if(cutsceneBlendLevel >= SMALL_FLOAT)
	{
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_NEAR]	*= (1.0f - cutsceneBlendLevel);
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM]	= Lerp(cutsceneBlendLevel,
																						viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM], 1.0f);
		viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_FAR]		*= (1.0f - cutsceneBlendLevel);
#if FPS_MODE_SUPPORTED
		viewModeBlendLevels[camControlHelperMetadataViewMode::FIRST_PERSON]			*= (1.0f - cutsceneBlendLevel);
#endif // FPS_MODE_SUPPORTED
	}

	PF_SET(desiredCustomNearToMediumViewModeBlendLevel, desiredNearBlendLevel);
	PF_SET(customNearToMediumViewModeBlendLevel, m_CustomNearToMediumViewModeBlendLevel);
	PF_SET(desiredCustomViewModeBlendLevelForCutsceneBlendOut, desiredCutsceneBlendLevel);
	PF_SET(customViewModeBlendLevelForCutsceneBlendOut, cutsceneBlendLevel);
}

float camFollowPedCamera::ComputeDesiredCustomNearToMediumViewModeBlendLevel(bool shouldCut)
{
	float desiredCustomNearToMediumViewModeBlendLevel;

	const bool shouldBlendNearToMediumViewModes = ComputeShouldBlendNearToMediumViewModes();

	if(m_CustomNearToMediumViewModeBlendEnvelope)
	{
		if(shouldCut)
		{
			if(shouldBlendNearToMediumViewModes)
			{
				m_CustomNearToMediumViewModeBlendEnvelope->Start(1.0f);
			}
			else
			{
				m_CustomNearToMediumViewModeBlendEnvelope->Stop();
			}
		}
		else
		{
			m_CustomNearToMediumViewModeBlendEnvelope->AutoStartStop(shouldBlendNearToMediumViewModes);
		}

		desiredCustomNearToMediumViewModeBlendLevel = m_CustomNearToMediumViewModeBlendEnvelope->Update();
	}
	else
	{
		//We don't have an envelope, so just use the state directly.
		desiredCustomNearToMediumViewModeBlendLevel = shouldBlendNearToMediumViewModes ? 1.0f : 0.0f;
	}

	//Modify the blend directly based upon some other blend levels, for better synchronization.
	desiredCustomNearToMediumViewModeBlendLevel = Lerp(m_RagdollBlendLevel, desiredCustomNearToMediumViewModeBlendLevel, 1.0f);
	desiredCustomNearToMediumViewModeBlendLevel = Lerp(m_LadderAlignmentBlendLevel, desiredCustomNearToMediumViewModeBlendLevel, 1.0f);

	desiredCustomNearToMediumViewModeBlendLevel *= m_Metadata.m_MaxCustomNearToMediumViewModeBlendLevel;

	return desiredCustomNearToMediumViewModeBlendLevel;
}

bool camFollowPedCamera::ComputeShouldBlendNearToMediumViewModes() const
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());

	const bool isAttachPedUsingActionMode = attachPed->IsUsingActionMode();
	if(isAttachPedUsingActionMode)
	{
		return true;
	}

	//NOTE: We ignore the in-air and falling states if the attach ped is ragdolling, as we need to defer to the ragdoll blend.
	if(m_RagdollBlendLevel < SMALL_FLOAT)
	{
		const bool isAttachPedInAir = attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);
		if(isAttachPedInAir)
		{
			return true;
		}

		const bool isAttachPedFalling = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling);
		if(isAttachPedFalling)
		{
			return true;
		}
	}

	//NOTE: We ignore the climbing and vaulting states if the attach ped is on a ladder, as we need to defer to the ladder alignment blend.
	if(m_LadderAlignmentBlendLevel < SMALL_FLOAT)
	{
		const bool isAttachPedClimbing = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing);
		if(isAttachPedClimbing)
		{
			return true;
		}

		const bool isAttachPedVaulting = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
		if(isAttachPedVaulting)
		{
			return true;
		}
	}

	const CPedIntelligence* attachPedIntelligence = attachPed->GetPedIntelligence();
	if(attachPedIntelligence)
	{
		const CQueriableInterface* attachPedQueriableInterface = attachPedIntelligence->GetQueriableInterface();
		if(attachPedQueriableInterface)
		{
			const bool isAttachPedInMeleeCombat = attachPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE, true);
			if(isAttachPedInMeleeCombat)
			{
				return true;
			}
		}
	}

	const CPedMotionData* attachPedMotionData = attachPed->GetMotionData();
	if(attachPedMotionData)
	{
		const bool isAttachPedRunning = attachPedMotionData->GetIsRunning() || attachPedMotionData->GetIsSprinting();
		if(isAttachPedRunning)
		{
			return true;
		}
	}

	return false;
}

void camFollowPedCamera::ComputeBlendedCustomViewModeSettings(const camFollowPedCameraMetadataCustomViewModeSettings* customFramingForViewModes,
	const float* viewModeBlendLevels, camFollowPedCameraMetadataCustomViewModeSettings& blendedCustomViewModeSettings) const
{
	blendedCustomViewModeSettings.m_AttachParentHeightRatioToAttainForBasePivotPosition	= 0.0f;
	blendedCustomViewModeSettings.m_ScreenRatioForMinFootRoom							= 0.0f;
	blendedCustomViewModeSettings.m_ScreenRatioForMaxFootRoom							= 0.0f;
	blendedCustomViewModeSettings.m_BaseOrbitPitchOffset								= 0.0f;

	for(int viewModeIndex=0; viewModeIndex<camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS; viewModeIndex++)
	{
		const float blendLevel = viewModeBlendLevels[viewModeIndex];

		const camFollowPedCameraMetadataCustomViewModeSettings& customSettingsForViewMode	= customFramingForViewModes[viewModeIndex];
		blendedCustomViewModeSettings.m_AttachParentHeightRatioToAttainForBasePivotPosition	+= blendLevel * customSettingsForViewMode.m_AttachParentHeightRatioToAttainForBasePivotPosition;
		blendedCustomViewModeSettings.m_ScreenRatioForMinFootRoom							+= blendLevel * customSettingsForViewMode.m_ScreenRatioForMinFootRoom;
		blendedCustomViewModeSettings.m_ScreenRatioForMaxFootRoom							+= blendLevel * customSettingsForViewMode.m_ScreenRatioForMaxFootRoom;
		blendedCustomViewModeSettings.m_BaseOrbitPitchOffset								+= blendLevel * customSettingsForViewMode.m_BaseOrbitPitchOffset;
	}
}

void camFollowPedCamera::UpdateAttachParentMatrix()
{
	camFollowCamera::UpdateAttachParentMatrix();

	//Blend to the matrix of the root bone if ped is vaulting.
	if(m_VaultBlendLevel >= SMALL_FLOAT)
	{
		Matrix34 rootBoneMatrix;
		const bool isRootBoneValid = ComputeAttachPedRootBoneMatrix(rootBoneMatrix);
		if(isRootBoneValid)
		{
			m_AttachParentMatrix.d.Lerp(m_VaultBlendLevel, rootBoneMatrix.d);
		}
	}

	//Blend to the matrix of the root bone rather than the ped transform if the attach ped is ragdolling.
	if(m_RagdollBlendLevel >= SMALL_FLOAT)
	{
		Matrix34 rootBoneMatrix;
		const bool isRootBoneValid = ComputeAttachPedRootBoneMatrix(rootBoneMatrix);
		if(isRootBoneValid)
		{
			Matrix34 blendedMatrix;
			camFrame::SlerpOrientation(m_RagdollBlendLevel, m_AttachParentMatrix, rootBoneMatrix, blendedMatrix);

			blendedMatrix.d.Lerp(m_RagdollBlendLevel, m_AttachParentMatrix.d, rootBoneMatrix.d);

			m_AttachParentMatrix.Set(blendedMatrix);
		}
	}

	//Snap the attach parent matrix to the target position when mounting a ladder from behind, in order to avoid various framing and occlusion issues.
	//NOTE: We can still utilise the z component of the attach parent position safely, which feels more natural.
	if(m_ShouldAttachToLadderMountTargetPosition && !m_LadderMountTargetPosition.IsClose(g_InvalidPosition, SMALL_FLOAT))
	{
		m_AttachParentMatrix.d.x = m_LadderMountTargetPosition.x;
		m_AttachParentMatrix.d.y = m_LadderMountTargetPosition.y;
	}

	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	if(gameplayDirector.IsFollowingVehicle() && gameplayDirector.IsCameraInterpolating(this))
	{
		//Lock the attach parent matrix position w.r.t. the follow vehicle when interpolating into a vehicle camera, for smoother interpolation.

		const CVehicle* followVehicle = gameplayDirector.GetFollowVehicle();
		if(followVehicle)
		{
			const Matrix34 followVehicleMatrix = MAT34V_TO_MATRIX34(followVehicle->GetMatrix());

			if(m_RelativeAttachParentMatrixPositionForVehicleEntry.IsClose(g_InvalidRelativePosition, SMALL_FLOAT))
			{
				followVehicleMatrix.UnTransform(m_AttachParentMatrix.d, m_RelativeAttachParentMatrixPositionForVehicleEntry);
			}
			else
			{
				followVehicleMatrix.Transform(m_RelativeAttachParentMatrixPositionForVehicleEntry, m_AttachParentMatrix.d);
			}
		}
	}
}

bool camFollowPedCamera::ComputeAttachPedRootBoneMatrix(Matrix34& matrix) const
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return false;
	}

	const CPed* attachPed				= static_cast<const CPed*>(m_AttachParent.Get());
	const crSkeleton* attachPedSkeleton	= attachPed->GetSkeleton();
	if(!attachPedSkeleton)
	{
		return false;
	}

	const s32 rootBoneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
	if(rootBoneIndex == -1)
	{
		return false;
	}

	//NOTE: We manually transform the local bone matrix into world space here, as the attach ped's skeleton may
	//not yet have been updated.
	const Matrix34& localBoneMatrix	= RCC_MATRIX34(attachPedSkeleton->GetLocalMtx(rootBoneIndex));
	const Matrix34 attachPedMatrix	= MAT34V_TO_MATRIX34(attachPed->GetTransform().GetMatrix());

	matrix.Dot(localBoneMatrix, attachPedMatrix);

	return true;
}

void camFollowPedCamera::UpdateBaseAttachPosition()
{
	camFollowCamera::UpdateBaseAttachPosition();

	//Blend to a custom base attach position if the attach ped is ragdolling.
	if(m_RagdollBlendLevel >= SMALL_FLOAT)
	{
		Vector3 baseAttachPositionForRagdoll(g_InvalidPosition);

		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		const CPed* attachPed				= static_cast<const CPed*>(m_AttachParent.Get());
		const bool isAttachPedUsingRagdoll	= attachPed->GetUsingRagdoll();
		if(isAttachPedUsingRagdoll)
		{
			attachPed->CalcCameraAttachPositionForRagdoll(baseAttachPositionForRagdoll);

			//NOTE: We cache the base attach position to be applied when ragdolling local to the attach parent matrix,
			//so that it can be restored if the reported position is no longer valid when blending out of ragdolling.
			if(g_CameraWorldLimits_AABB.ContainsPoint(VECTOR3_TO_VEC3V(baseAttachPositionForRagdoll)))
			{
				m_AttachParentMatrix.UnTransform(baseAttachPositionForRagdoll, m_RelativeBaseAttachPositionForRagdoll);
			}
			else
			{
				m_AttachParentMatrix.Transform(m_RelativeBaseAttachPositionForRagdoll, baseAttachPositionForRagdoll);
			}
		}
		else
		{
			m_AttachParentMatrix.Transform(m_RelativeBaseAttachPositionForRagdoll, baseAttachPositionForRagdoll);
		}

		m_BaseAttachPosition.Lerp(m_RagdollBlendLevel, baseAttachPositionForRagdoll);
	}
}

float camFollowPedCamera::ComputeAttachParentHeightRatioToAttainForBasePivotPosition() const
{
	float attachParentHeightRatioToAttain;

	if(m_ShouldUseCustomViewModeSettings)
	{
		float customViewModeAttachParentHeightRatioToAttainAfterAiming = m_CustomViewModeSettingsAfterAiming.m_AttachParentHeightRatioToAttainForBasePivotPosition;
		if(ShouldUseTightSpaceCustomFraming())
		{
			//Apply any scaling for tight spaces.
			const float blendLevel			= m_TightSpaceHeightSpring.GetResult();
			attachParentHeightRatioToAttain	= Lerp(blendLevel, m_CustomViewModeSettings.m_AttachParentHeightRatioToAttainForBasePivotPosition,
												m_CustomViewModeSettingsInTightSpace.m_AttachParentHeightRatioToAttainForBasePivotPosition);
			customViewModeAttachParentHeightRatioToAttainAfterAiming = Lerp(blendLevel, customViewModeAttachParentHeightRatioToAttainAfterAiming,
												m_CustomViewModeSettingsAfterAimingInTightSpace.m_AttachParentHeightRatioToAttainForBasePivotPosition);
		}
		else
		{
			attachParentHeightRatioToAttain = m_CustomViewModeSettings.m_AttachParentHeightRatioToAttainForBasePivotPosition;
		}

		if( m_Metadata.m_ShouldUseCustomFramingAfterAiming )
		{
			if(m_AttachParent && m_AttachParent->GetIsTypePed())
			{
				const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
				if( attachPed && attachPed->IsLocalPlayer() && attachPed->GetPlayerInfo() )
				{
					if(m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax > 0.0f)
					{
						if(m_DistanceTravelled < m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax)
						{
							float aimBlend = ComputeAimBlendBasedOnDistanceTravelled(); //RampValueSafe(m_DistanceTravelled, 0.0f, m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax, 0.0f, 1.0f);
							attachParentHeightRatioToAttain	= Lerp(SlowInOut(aimBlend),  customViewModeAttachParentHeightRatioToAttainAfterAiming, attachParentHeightRatioToAttain);
						}
					}
					else
					{
						float timeSinceLastAimed = (attachPed->GetPlayerInfo()->GetTimeSinceLastAimedMS()/1000.0f);
						if( timeSinceLastAimed < m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax)
						{
							float aimBlendTimeMin = m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMin;
							float aimBlendTimeMax = m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax;
							float aimBlend = Clamp( (timeSinceLastAimed-aimBlendTimeMin)/(aimBlendTimeMax-aimBlendTimeMin), 0.0f, 1.0f);
							attachParentHeightRatioToAttain	= Lerp(SlowInOut(aimBlend),  customViewModeAttachParentHeightRatioToAttainAfterAiming, attachParentHeightRatioToAttain);
						}
					}
				}
			}
		}
	}
	else
	{
		attachParentHeightRatioToAttain	= camFollowCamera::ComputeAttachParentHeightRatioToAttainForBasePivotPosition();
	}

	return attachParentHeightRatioToAttain;
}

float camFollowPedCamera::ComputeAttachPedPelvisOffsetForShoes() const
{
#if RSG_PC
	if(camInterface::IsTripleHeadDisplay())
	{
		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		const CPed* attachedPed	= static_cast<const CPed*>(m_AttachParent.Get());
		const bool hasHighHeels	= attachedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHighHeels) BANK_ONLY(|| g_DebugShouldUseHighHeels);
		const bool hasBareFeet  = attachedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBareFeet)  BANK_ONLY(|| g_DebugShouldUseBareFeet);

		float offset = hasHighHeels ? g_AttachPedPelvisOffsetForHighHeels : 0.0f;
		offset -= hasBareFeet ? g_AttachPedPelvisOffsetForBareFeet : 0.0f;

		return offset;
	}
#endif

	return 0.0f;
}

float camFollowPedCamera::ComputeExtraOffsetForHatsAndHelmets() const 
{
#if RSG_PC
	if(camInterface::IsTripleHeadDisplay())
	{
		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		const CPed* attachedPed		= static_cast<const CPed*>(m_AttachParent.Get());
		if(CPedPropsMgr::GetPedPropIdx(attachedPed, ANCHOR_HEAD) != -1)
		{						
			return m_Metadata.m_ExtraOffsetForHatsAndHelmetsForTripleHead;
		}
	}
#endif

	return 0.0f;
}

void camFollowPedCamera::UpdateOrbitPitchLimits()
{
	camFollowCamera::UpdateOrbitPitchLimits();

	//Blend to custom limits if the attach ped is on a ladder.

	Vector2 orbitPitchLimitsForLadder;
	orbitPitchLimitsForLadder.SetScaled(m_Metadata.m_LadderAlignment.m_OrbitPitchLimits, DtoR);

	m_OrbitPitchLimits.Lerp(m_LadderAlignmentBlendLevel, m_OrbitPitchLimits, orbitPitchLimitsForLadder);

	//Blend to custom limits if the attach ped is ragdolling.

	Vector2 orbitPitchLimitsForRagdoll;

	if(camInterface::GetGameplayDirector().IsHintActive())
	{
		float HintBlendLevel = camInterface::GetGameplayDirector().GetHintBlend(); 
		orbitPitchLimitsForRagdoll.Lerp(HintBlendLevel, m_Metadata.m_OrbitPitchLimitsForRagdoll, m_Metadata.m_HintOrbitPitchLimitsForRagdoll);
		orbitPitchLimitsForRagdoll.Scale(DtoR);
	}
	else
	{
		orbitPitchLimitsForRagdoll.SetScaled(m_Metadata.m_OrbitPitchLimitsForRagdoll, DtoR);
	}
	
	
	m_OrbitPitchLimits.Lerp(m_RagdollBlendLevel, m_OrbitPitchLimits, orbitPitchLimitsForRagdoll);

	if(m_Metadata.m_ShouldApplyCustomOrbitPitchLimitsWhenBuoyant && m_IsBuoyant)
	{
		//Apply custom limits when buoyant, but only if they are more restrictive.
		Vector2 orbitPitchLimitsWhenBuoyant;
		orbitPitchLimitsWhenBuoyant.SetScaled(m_Metadata.m_OrbitPitchLimitsWhenBuoyant, DtoR);

		m_OrbitPitchLimits.x = Max(m_OrbitPitchLimits.x, orbitPitchLimitsWhenBuoyant.x);
		m_OrbitPitchLimits.y = Min(m_OrbitPitchLimits.y, orbitPitchLimitsWhenBuoyant.y);
	}

	//Apply a custom minimum orbit pitch limit in the presence of low overhead collision.
	if(m_NumUpdatesPerformed > 0)
	{
		const camFollowPedCameraMetadataOrbitPitchLimitsForOverheadCollision& overheadCollisionSettings = m_Metadata.m_OrbitPitchLimitsForOverheadCollision;

		Vector2 minOrbitPitchLimitsForOverheadCollision;
		minOrbitPitchLimitsForOverheadCollision.SetScaled(overheadCollisionSettings.m_MinOrbitPitchLimits, DtoR);

		const float collisionFallBackBlendLevelOnPreviousUpdate	= m_CollisionFallBackSpring.GetResult();
		const float desiredBlendLevelForOverheadCollision		= RampValueSafe(collisionFallBackBlendLevelOnPreviousUpdate,
																	overheadCollisionSettings.m_CollisionFallBackBlendLevelForFullLimit, 1.0f, 1.0f, 0.0f);

		const bool shouldCut		= (m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
		const float springConstant	= shouldCut ? 0.0f : overheadCollisionSettings.m_SpringConstant;

		const float blendLevelForOverheadCollisionToApply	= m_OrbitPitchLimitsForOverheadCollisionSpring.Update(desiredBlendLevelForOverheadCollision,
																springConstant, overheadCollisionSettings.m_SpringDampingRatio);

		const float minOrbitPitchForOverheadCollision		= Lerp(blendLevelForOverheadCollisionToApply, minOrbitPitchLimitsForOverheadCollision.x,
																minOrbitPitchLimitsForOverheadCollision.y);
		m_OrbitPitchLimits.x								= Max(m_OrbitPitchLimits.x, minOrbitPitchForOverheadCollision);
	}
}

void camFollowPedCamera::ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const
{
	if(m_ShouldUseCustomViewModeSettings)
	{
		float customViewModeScreenRatioForMinFootRoomAfterAiming = m_CustomViewModeSettingsAfterAiming.m_ScreenRatioForMinFootRoom;
		float customViewModeScreenRatioForMaxFootRoomAfterAiming = m_CustomViewModeSettingsAfterAiming.m_ScreenRatioForMaxFootRoom;
		if(ShouldUseTightSpaceCustomFraming())
		{
			//Apply any scaling for tight spaces.
			const float blendLevel		= m_TightSpaceSpring.GetResult();
			screenRatioForMinFootRoom	= Lerp(blendLevel, m_CustomViewModeSettings.m_ScreenRatioForMinFootRoom,
											m_CustomViewModeSettingsInTightSpace.m_ScreenRatioForMinFootRoom);
			screenRatioForMaxFootRoom	= Lerp(blendLevel, m_CustomViewModeSettings.m_ScreenRatioForMaxFootRoom,
											m_CustomViewModeSettingsInTightSpace.m_ScreenRatioForMaxFootRoom);
			customViewModeScreenRatioForMinFootRoomAfterAiming = Lerp(blendLevel, customViewModeScreenRatioForMinFootRoomAfterAiming,
											m_CustomViewModeSettingsAfterAimingInTightSpace.m_ScreenRatioForMinFootRoom);
			customViewModeScreenRatioForMaxFootRoomAfterAiming = Lerp(blendLevel, customViewModeScreenRatioForMaxFootRoomAfterAiming,
											m_CustomViewModeSettingsAfterAimingInTightSpace.m_ScreenRatioForMaxFootRoom);
		}
		else
		{
			screenRatioForMinFootRoom = m_CustomViewModeSettings.m_ScreenRatioForMinFootRoom;
			screenRatioForMaxFootRoom = m_CustomViewModeSettings.m_ScreenRatioForMaxFootRoom;
		}

		if( m_Metadata.m_ShouldUseCustomFramingAfterAiming )
		{
			if(m_AttachParent && m_AttachParent->GetIsTypePed())
			{
				const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
				if( attachPed && attachPed->IsLocalPlayer() && attachPed->GetPlayerInfo() )
				{
					if(m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax > 0.0f)
					{
						if(m_DistanceTravelled < m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax)
						{
							float aimBlend = ComputeAimBlendBasedOnDistanceTravelled(); //RampValueSafe(m_DistanceTravelled, 0.0f, m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax, 0.0f, 1.0f);
							screenRatioForMinFootRoom	= Lerp(SlowInOut(aimBlend),  customViewModeScreenRatioForMinFootRoomAfterAiming, screenRatioForMinFootRoom);
							screenRatioForMaxFootRoom	= Lerp(SlowInOut(aimBlend),  customViewModeScreenRatioForMaxFootRoomAfterAiming, screenRatioForMaxFootRoom);
						}
					}
					else
					{
						float timeSinceLastAimed = (attachPed->GetPlayerInfo()->GetTimeSinceLastAimedMS()/1000.0f);
						if( timeSinceLastAimed < m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax)
						{
							float aimBlendTimeMin = m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMin;
							float aimBlendTimeMax = m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax;
							float aimBlend = Clamp( (timeSinceLastAimed-aimBlendTimeMin)/(aimBlendTimeMax-aimBlendTimeMin), 0.0f, 1.0f);
							screenRatioForMinFootRoom	= Lerp(SlowInOut(aimBlend),  customViewModeScreenRatioForMinFootRoomAfterAiming, screenRatioForMinFootRoom);
							screenRatioForMaxFootRoom	= Lerp(SlowInOut(aimBlend),  customViewModeScreenRatioForMaxFootRoomAfterAiming, screenRatioForMaxFootRoom);
						}
					}
				}
			}
		}
	}
	else
	{
		camFollowCamera::ComputeScreenRatiosForFootRoomLimits(screenRatioForMinFootRoom, screenRatioForMaxFootRoom);
	}

	//Push the minimum screen ratio out to the maximum when aligning to a ladder, as this gives more consistent framing.
	screenRatioForMinFootRoom = Lerp(m_LadderAlignmentBlendLevel, screenRatioForMinFootRoom, screenRatioForMaxFootRoom);
}

float camFollowPedCamera::ComputeBaseOrbitPitchOffset() const
{
	float pitchOffset;

	if(m_ShouldUseCustomViewModeSettings)
	{
		float customViewModePitchOffsetAfterAiming = m_CustomViewModeSettingsAfterAiming.m_BaseOrbitPitchOffset;
		if(ShouldUseTightSpaceCustomFraming())
		{
			//Apply any scaling for tight spaces.
			const float blendLevel	= m_TightSpaceSpring.GetResult();
			pitchOffset				= Lerp(blendLevel, m_CustomViewModeSettings.m_BaseOrbitPitchOffset,
										m_CustomViewModeSettingsInTightSpace.m_BaseOrbitPitchOffset);
			customViewModePitchOffsetAfterAiming = Lerp(blendLevel, customViewModePitchOffsetAfterAiming,
										m_CustomViewModeSettingsAfterAimingInTightSpace.m_BaseOrbitPitchOffset);
		}
		else
		{
			pitchOffset				= m_CustomViewModeSettings.m_BaseOrbitPitchOffset;
		}

		if( m_Metadata.m_ShouldUseCustomFramingAfterAiming )
		{
			if(m_AttachParent && m_AttachParent->GetIsTypePed())
			{
				const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
				if( attachPed && attachPed->IsLocalPlayer() && attachPed->GetPlayerInfo() )
				{
					if(m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax > 0.0f)
					{
						if(m_DistanceTravelled < m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax)
						{
							float aimBlend = ComputeAimBlendBasedOnDistanceTravelled(); //RampValueSafe(m_DistanceTravelled, 0.0f, m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax, 0.0f, 1.0f);
							pitchOffset	= Lerp(SlowInOut(aimBlend), customViewModePitchOffsetAfterAiming, pitchOffset);
						}
					}
					else
					{
						float timeSinceLastAimed = (attachPed->GetPlayerInfo()->GetTimeSinceLastAimedMS()/1000.0f);
						if( timeSinceLastAimed < m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax)
						{
							float aimBlendTimeMin = m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMin;
							float aimBlendTimeMax = m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax;
							float aimBlend = Clamp( (timeSinceLastAimed-aimBlendTimeMin)/(aimBlendTimeMax-aimBlendTimeMin), 0.0f, 1.0f);
							pitchOffset	= Lerp(SlowInOut(aimBlend), customViewModePitchOffsetAfterAiming, pitchOffset);
						}
					}
				}
			}
		}
		pitchOffset *= DtoR;
	}
	else
	{
		pitchOffset = camFollowCamera::ComputeBaseOrbitPitchOffset();
	}

	return pitchOffset;
}

bool camFollowPedCamera::ComputeIsAttachParentInAir() const
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachPed		= static_cast<const CPed*>(m_AttachParent.Get());
	bool isAttachParentInAir	= attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);
	if(!isAttachParentInAir)
	{
		//Double-check for the NM high-fall task, as this may not always manage the IsInTheAir flag correctly.
		const CPedIntelligence* pedIntelligence = attachPed->GetPedIntelligence();
		if(pedIntelligence)
		{
			const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();

			isAttachParentInAir = (pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_HIGH_FALL));
		}
	}

	return isAttachParentInAir;
}

float camFollowPedCamera::ComputeVerticalMoveSpeedScaling(const Vector3& attachParentVelocity) const
{
	float verticalMoveSpeedScaling	= camFollowCamera::ComputeVerticalMoveSpeedScaling(attachParentVelocity);
	verticalMoveSpeedScaling		= Lerp(m_LadderAlignmentBlendLevel, verticalMoveSpeedScaling,
										m_Metadata.m_LadderAlignment.m_VerticalMoveSpeedScaling);

	return verticalMoveSpeedScaling;
}

float camFollowPedCamera::ComputeCustomFollowBlendLevel() const
{
	//Blend out the follow behaviour when the attach ped is performing a melee action or a super jump.
	float customFollowBlendLevel	= camFollowCamera::ComputeCustomFollowBlendLevel();
	customFollowBlendLevel			*= (1.0f - m_MeleeBlendLevel);
	customFollowBlendLevel			*= (1.0f - m_SuperJumpBlendLevel);

	return customFollowBlendLevel;
}

void camFollowPedCamera::ComputeBasePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const
{
	camFollowCamera::ComputeBasePullAroundSettings(settings);

	//Optionally blend to custom default settings in a tight space.
	if(m_Metadata.m_ShouldUseCustomPullAroundSettingsInTightSpace)
	{
		const float blendLevel = m_TightSpaceSpring.GetResult();

		camFollowCameraMetadataPullAroundSettings blendedSettings;
		BlendPullAroundSettings(blendLevel, settings, m_Metadata.m_PullAroundSettingsInTightSpace, blendedSettings);

		settings = blendedSettings;
	}

	if(m_AssistedMovementAlignmentBlendLevel >= SMALL_FLOAT)
	{
		//Blend to custom settings when aligning to an assisted movement route.
		camFollowCameraMetadataPullAroundSettings blendedSettings;
		BlendPullAroundSettings(m_AssistedMovementAlignmentBlendLevel, settings, m_Metadata.m_AssistedMovementAlignment.m_PullAroundSettings,
			blendedSettings);

		settings = blendedSettings;
	}
	else if(m_LadderAlignmentBlendLevel >= SMALL_FLOAT)
	{
		const camFollowCameraMetadataPullAroundSettings& ladderSettings	= (m_IsAttachPedMountingLadder || m_WasAttachPedDismountingLadder) ?
																			m_Metadata.m_LadderAlignment.m_PullAroundSettingsForMountAndDismount :
																			m_Metadata.m_LadderAlignment.m_PullAroundSettings;

		settings.m_ShouldBlendOutWhenAttachParentIsInAir	|= ladderSettings.m_ShouldBlendOutWhenAttachParentIsInAir;
		settings.m_ShouldBlendOutWhenAttachParentIsOnGround	|= ladderSettings.m_ShouldBlendOutWhenAttachParentIsOnGround;
		settings.m_HeadingPullAroundMinMoveSpeed			= Lerp(m_LadderAlignmentBlendLevel, settings.m_HeadingPullAroundMinMoveSpeed,
																ladderSettings.m_HeadingPullAroundMinMoveSpeed);
		settings.m_HeadingPullAroundMaxMoveSpeed			= Lerp(m_LadderAlignmentBlendLevel, settings.m_HeadingPullAroundMaxMoveSpeed,
																ladderSettings.m_HeadingPullAroundMaxMoveSpeed);
		settings.m_HeadingPullAroundSpeedAtMaxMoveSpeed		= Lerp(m_LadderAlignmentBlendLevel, settings.m_HeadingPullAroundSpeedAtMaxMoveSpeed,
																ladderSettings.m_HeadingPullAroundSpeedAtMaxMoveSpeed);
		settings.m_HeadingPullAroundErrorScalingBlendLevel	= Lerp(m_AssistedMovementAlignmentBlendLevel, settings.m_HeadingPullAroundErrorScalingBlendLevel,
																ladderSettings.m_HeadingPullAroundErrorScalingBlendLevel);
		settings.m_HeadingPullAroundSpringConstant			= Lerp(m_LadderAlignmentBlendLevel, settings.m_HeadingPullAroundSpringConstant,
																ladderSettings.m_HeadingPullAroundSpringConstant);
		settings.m_HeadingPullAroundSpringDampingRatio		= Lerp(m_LadderAlignmentBlendLevel, settings.m_HeadingPullAroundSpringDampingRatio,
																ladderSettings.m_HeadingPullAroundSpringDampingRatio);
	}
}

void camFollowPedCamera::ComputeAttachParentVelocityToConsiderForPullAround(Vector3& attachParentVelocityToConsider) const
{
	camFollowCamera::ComputeAttachParentVelocityToConsiderForPullAround(attachParentVelocityToConsider);

	//NOTE: We always pull-around on mounting and dismounting, irrespective of nearby collision.
	const bool shouldPullAroundOnLadder		= (m_IsAttachPedMountingLadder || m_WasAttachPedDismountingLadder || !m_ShouldInhibitPullAroundOnLadder);
	//NOTE: We want the pull-around effect be based upon move speed including any (locally) vertical component when aligning to a ladder.
	const Vector3 velocityToConsideOnLadder	= shouldPullAroundOnLadder ? m_BaseAttachVelocityToConsider : VEC3_ZERO;

	attachParentVelocityToConsider.Lerp(m_LadderAlignmentBlendLevel, velocityToConsideOnLadder);
}

void camFollowPedCamera::ComputeDesiredPullAroundOrientation(const Vector3& attachParentVelocityToConsider, float orbitHeading, float orbitPitch,
	float& desiredHeading, float& desiredPitch) const
{
	camFollowCamera::ComputeDesiredPullAroundOrientation(attachParentVelocityToConsider, orbitHeading, orbitPitch, desiredHeading, desiredPitch);

	if(m_AssistedMovementAlignmentBlendLevel >= SMALL_FLOAT)
	{
		float desiredHeadingForAssistedMovement;
		float desiredPitchForAssistedMovement;
		camFrame::ComputeHeadingAndPitchFromFront(m_AssistedMovementLookAheadRouteDirection, desiredHeadingForAssistedMovement,
			desiredPitchForAssistedMovement);

		desiredHeading = fwAngle::LerpTowards(desiredHeading, desiredHeadingForAssistedMovement, m_AssistedMovementAlignmentBlendLevel);

		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		CPed* attachPed = (CPed*)m_AttachParent.Get();
		if(attachPed->GetPrimaryMotionTask()->IsUnderWater())
		{
			//The attach ped is on a 3D assisted movement route, so we must also blend to a custom desired pitch.
			desiredPitch = fwAngle::LerpTowards(desiredPitch, desiredPitchForAssistedMovement, m_AssistedMovementAlignmentBlendLevel);
		}
	}
	else if(m_LadderAlignmentBlendLevel >= SMALL_FLOAT)
	{
		//Pull-around to align head-on with the ladder by default, which covers mounting and dismounting.
		float desiredHeadingForLadder = m_LadderHeading;

		if(!m_ShouldInhibitCustomPullAroundOnLadder && !m_IsAttachPedMountingLadder && !m_WasAttachPedDismountingLadder)
		{
			//Pull-around towards the nearest side of the ladder.
			const float ladderToOrbitHeadingDelta		= fwAngle::LimitRadianAngleSafe(orbitHeading - m_LadderHeading);
			const float ladderToOrbitHeadingDeltaSign	= (ladderToOrbitHeadingDelta >= 0.0f) ? 1.0f : -1.0f;
			desiredHeadingForLadder						= fwAngle::LimitRadianAngle(m_LadderHeading + (ladderToOrbitHeadingDeltaSign *
															m_Metadata.m_LadderAlignment.m_DesiredPullAroundHeadingOffset * DtoR));
		}

		desiredHeading = fwAngle::LerpTowards(desiredHeading, desiredHeadingForLadder, m_LadderAlignmentBlendLevel);
	}
}

float camFollowPedCamera::ComputeIdealOrbitHeadingForLimiting() const
{
	float idealHeading =  camFollowCamera::ComputeIdealOrbitHeadingForLimiting();

	if((m_LadderAlignmentBlendLevel >= SMALL_FLOAT) || m_ShouldAttachToLadderMountTargetPosition)
	{
		//Limit against the heading of the ladder for the duration of any alignment. Limiting against the heading of the attach ped would result
		//in undesirable behaviour when turning away from the ladder during the blend out.
		idealHeading = m_LadderHeading;
	}

	return idealHeading;
}

void camFollowPedCamera::ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const
{
	camFollowCamera::ComputeOrbitRelativePivotOffsetInternal(orbitRelativeOffset);

	//Clear any default side offset when following an animal.
	if(m_AttachParent && m_AttachParent->GetIsTypePed() && (static_cast<const CPed*>(m_AttachParent.Get())->GetPedType() == PEDTYPE_ANIMAL))
	{
		orbitRelativeOffset.x = 0.0f;
	}
}

bool camFollowPedCamera::ComputeShouldPushBeyondAttachParentIfClipping() const
{
	bool shouldPushBeyondAttachParentIfClipping = camFollowCamera::ComputeShouldPushBeyondAttachParentIfClipping();
	if(!shouldPushBeyondAttachParentIfClipping)
	{
		shouldPushBeyondAttachParentIfClipping = (m_Metadata.m_ShouldPushBeyondAttachParentIfClippingInRagdoll && (m_RagdollBlendLevel > SMALL_FLOAT));
	}

	return shouldPushBeyondAttachParentIfClipping;
}

float camFollowPedCamera::ComputeMaxSafeCollisionRadiusForAttachParent() const
{
	float maxSafeCollisionRadiusForAttachParent = camFollowCamera::ComputeMaxSafeCollisionRadiusForAttachParent();

	if(m_Metadata.m_MaxCollisionTestRadiusForRagdoll <= (maxSafeCollisionRadiusForAttachParent - SMALL_FLOAT))
	{
		//NOTE: We cut to the custom radius limit upon entering ragdoll but we blend out, as this reduces the scope for problems arising from the
		//ped capsule penetrating collision.
		const bool shouldApplyRagdollBehaviour	= m_AttachParent ? ComputeShouldApplyRagdollBehavour(*m_AttachParent) : false;
		const float blendLevel					= shouldApplyRagdollBehaviour ? 1.0f : m_RagdollBlendLevel;
		maxSafeCollisionRadiusForAttachParent	= Lerp(blendLevel, maxSafeCollisionRadiusForAttachParent, m_Metadata.m_MaxCollisionTestRadiusForRagdoll);
	}

	if(m_Metadata.m_MaxCollisionTestRadiusForVault <= (maxSafeCollisionRadiusForAttachParent - SMALL_FLOAT))
	{
		//NOTE: We cut to the custom radius limit upon starting to climb/vault but we blend out, as this reduces the scope for problems arising from the
		//ped capsule penetrating collision.
		bool dummy;
		const bool shouldApplyVaultBehaviour	= ComputeShouldApplyVaultBehavour(dummy);
		const float blendLevel					= shouldApplyVaultBehaviour ? 1.0f : m_VaultBlendLevel;
		maxSafeCollisionRadiusForAttachParent	= Lerp(blendLevel, maxSafeCollisionRadiusForAttachParent, m_Metadata.m_MaxCollisionTestRadiusForVault);
	}

	return maxSafeCollisionRadiusForAttachParent;
}

void camFollowPedCamera::UpdateCollisionOrigin(float& collisionTestRadius)
{
	camFollowCamera::UpdateCollisionOrigin(collisionTestRadius);

	const camFollowPedCameraMetadataPushBeyondNearbyVehiclesInRagdollSettings& settings = m_Metadata.m_PushBeyondNearbyVehiclesInRagdollSettings;

	const bool isInRagdollReleasePhase			= m_RagdollBlendEnvelope && m_RagdollBlendEnvelope->IsInReleasePhase();
	const float minRagdollBlendLevelToConsider	= isInRagdollReleasePhase ? settings.m_MinRagdollBlendLevelDuringRelease : settings.m_MinRagdollBlendLevel;
	if(m_RagdollBlendLevel < minRagdollBlendLevelToConsider)
	{
		m_NearbyVehiclesInRagdollList.Reset();
		return;
	}

	//Ignore and push beyond any vehicles that are detected close to the collision origin that are not also found when testing downwards.
	// - Track any valid nearby vehicles for a set duration to avoid rapid changes in behaviour.

	const u32 gameTime = fwTimer::GetTimeInMilliseconds();

	//Validate all existing tracked vehicles, ensuring that they have not expired and are still nearby.
	s32 vehicleIndex;
	for(vehicleIndex=0; vehicleIndex<m_NearbyVehiclesInRagdollList.GetCount(); )
	{
		tNearbyVehicleSettings& vehicleSettings = m_NearbyVehiclesInRagdollList[vehicleIndex];

		bool isValid = ((vehicleSettings.m_Vehicle != NULL) && (gameTime <= (vehicleSettings.m_DetectionTime + settings.m_MaxDurationToTrackVehicles)));
		if(isValid)
		{
			spdAABB vehicleAabb;
			vehicleSettings.m_Vehicle->GetAABB(vehicleAabb);

			const float distanceToVehicleAabb	= vehicleAabb.DistanceToPoint(VECTOR3_TO_VEC3V(m_CollisionOrigin)).Getf();
			isValid								= (distanceToVehicleAabb <= settings.m_MaxAabbDistanceToTrackVehicles);
		}

		if(isValid)
		{
			vehicleIndex++;
		}
		else
		{
			//NOTE: We explicitly clear the registered reference for safety.
			vehicleSettings.m_Vehicle = NULL;
			m_NearbyVehiclesInRagdollList.Delete(vehicleIndex);
		}
	}

	atArray<const CEntity*> nearbyVehicleList;

	ComputeValidNearbyVehiclesInRagdoll(collisionTestRadius, nearbyVehicleList);

	const s32 numNearbyVehicles = nearbyVehicleList.GetCount();
	for(s32 nearbyVehicleIndex=0; nearbyVehicleIndex<numNearbyVehicles; nearbyVehicleIndex++)
	{
		const CVehicle* nearbyVehicle = static_cast<const CVehicle*>(nearbyVehicleList[nearbyVehicleIndex]);
		if(!nearbyVehicle)
		{
			continue;
		}

		//Add this vehicle to our list of tracked vehicles, or bump the detection time if we're already tracking it.

		bool isTrackingVehicle = false;

		const s32 numTrackedVehicles = m_NearbyVehiclesInRagdollList.GetCount();
		for(vehicleIndex=0; vehicleIndex<numTrackedVehicles; vehicleIndex++)
		{
			tNearbyVehicleSettings& vehicleSettings	= m_NearbyVehiclesInRagdollList[vehicleIndex];
			isTrackingVehicle						= (vehicleSettings.m_Vehicle == nearbyVehicle);
			if(isTrackingVehicle)
			{
				vehicleSettings.m_DetectionTime = gameTime;
				break;
			}
		}

		if(!isTrackingVehicle)
		{
			tNearbyVehicleSettings& vehicleSettings	= m_NearbyVehiclesInRagdollList.Grow();
			vehicleSettings.m_Vehicle				= nearbyVehicle;
			vehicleSettings.m_DetectionTime			= gameTime;
		}
	}

	//Finally, ignore and push beyond all tracked nearby vehicles.
	const s32 numTrackedVehicles = m_NearbyVehiclesInRagdollList.GetCount();
	for(vehicleIndex=0; vehicleIndex<numTrackedVehicles; vehicleIndex++)
	{
		tNearbyVehicleSettings& vehicleSettings	= m_NearbyVehiclesInRagdollList[vehicleIndex];
		const CVehicle* nearbyVehicle			= vehicleSettings.m_Vehicle;
		if(!nearbyVehicle)
		{
			continue;
		}

		m_Collision->IgnoreEntityThisUpdate(*nearbyVehicle);
		m_Collision->PushBeyondEntityIfClippingThisUpdate(*nearbyVehicle);

		const bool isTrain = nearbyVehicle->InheritsFromTrain();
		if(isTrain)
		{
			//Also consider any train carriage that is directly linked to either side of this train/carriage.

			const CTrain* nearbyTrain	= static_cast<const CTrain*>(nearbyVehicle);
			const CTrain* backwardTrain	= nearbyTrain->GetLinkedToBackward();
			const CTrain* forwardTrain	= nearbyTrain->GetLinkedToForward();

			if(backwardTrain)
			{
				m_Collision->IgnoreEntityThisUpdate(*backwardTrain);
				m_Collision->PushBeyondEntityIfClippingThisUpdate(*backwardTrain);
			}

			if(forwardTrain)
			{
				m_Collision->IgnoreEntityThisUpdate(*forwardTrain);
				m_Collision->PushBeyondEntityIfClippingThisUpdate(*forwardTrain);
			}
		}
	}
}

void camFollowPedCamera::ComputeFallBackForCollisionOrigin(Vector3& fallBackPosition) const
{
	camFollowCamera::ComputeFallBackForCollisionOrigin(fallBackPosition);

	//NOTE: We must fall back to the position of the attach ped's root bone when ragdolling, as the mover and ped transform are stale
	//and may penetrate collision.
	const bool shouldApplyRagdollBehaviour =  m_AttachParent ? ComputeShouldApplyRagdollBehavour(*m_AttachParent) : false;
	if(shouldApplyRagdollBehaviour)
	{
		Matrix34 rootBoneMatrix;
		const bool isRootBoneValid = ComputeAttachPedRootBoneMatrix(rootBoneMatrix);
		if(isRootBoneValid)
		{
			fallBackPosition.Set(rootBoneMatrix.d);
		}
	}
}

void camFollowPedCamera::ComputeValidNearbyVehiclesInRagdoll(float collisionTestRadius, atArray<const CEntity*>& nearbyVehicleList) const
{
	const camFollowPedCameraMetadataPushBeyondNearbyVehiclesInRagdollSettings& settings = m_Metadata.m_PushBeyondNearbyVehiclesInRagdollSettings;

	//Perform a capsule test upwards from a position that is offset vertically to align with the collision origin less the collision test radius.

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> capsuleTestResults;
	capsuleTest.SetResultsStructure(&capsuleTestResults);
	capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	//NOTE: m_Collision has already been validated.
	const CEntity** entitiesToIgnore	= const_cast<const CEntity**>(m_Collision->GetEntitiesToIgnoreThisUpdate());
	const s32 numEntitiesToIgnore		= m_Collision->GetNumEntitiesToIgnoreThisUpdate();
	if(numEntitiesToIgnore > 0)
	{
		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}

	const float startZOffset = settings.m_DetectionRadius - collisionTestRadius;

	Vector3 startPosition(m_CollisionOrigin);
	startPosition.AddScaled(ZAXIS, startZOffset);

	Vector3 endPosition(startPosition);
	endPosition.AddScaled(ZAXIS, settings.m_DistanceToTestUpForVehicles);

	capsuleTest.SetCapsule(startPosition, endPosition, settings.m_DetectionRadius);

	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	if(!hasHit)
	{
		return;
	}

	for(WorldProbe::ResultIterator upwardResultsIterator=capsuleTestResults.begin(); upwardResultsIterator<capsuleTestResults.last_result(); upwardResultsIterator++)
	{
		if(!upwardResultsIterator->GetHitDetected())
		{
			continue;
		}

		const CEntity* hitEntity = upwardResultsIterator->GetHitEntity();
		if(!hitEntity || !hitEntity->GetIsTypeVehicle())
		{
			continue;
		}

		//Exclude all vehicles, aside from boats, that have BVH bounds, as the follow ped could ragdolling inside these vehicles.
		//NOTE: The downward probe (below) should provide a means to reject vehicles that the follow ped is inside, but it does not appear to be 100% reliable.
		const CVehicle* hitVehicle = static_cast<const CVehicle*>(hitEntity);
		if(!hitVehicle->InheritsFromBoat())
		{
			const fragInstGta* vehicleFragInst = hitVehicle->GetVehicleFragInst();
			if(vehicleFragInst)
			{
				const phArchetype* archetype = vehicleFragInst->GetArchetype();
				if(archetype)
				{
					const phBound* bound = archetype->GetBound();
					if(bound && (bound->GetType() == phBound::COMPOSITE))
					{
						const bool hasBvhBound = static_cast<const phBoundComposite*>(bound)->GetContainsBVH();
						if(hasBvhBound)
						{
							continue;
						}
					}
				}
			}
		}

		nearbyVehicleList.Grow() = hitEntity;
	}

	const s32 numNearbyVehicles = nearbyVehicleList.GetCount();
	if(numNearbyVehicles < 1)
	{
		return;
	}

	const CEntity** nearbyVehicles = nearbyVehicleList.GetElements();

	//Now check below the collision origin for any of the nearby vehicles we detected above,
	//as we don't not wish to take into account any vehicles that are also below.

	WorldProbe::CShapeTestProbeDesc probeTest;
	WorldProbe::CShapeTestFixedResults<> probeTestResults;
	probeTest.SetResultsStructure(&probeTestResults);
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	probeTest.SetContext(WorldProbe::LOS_Camera);
	probeTest.SetIsDirected(true);

	probeTest.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	probeTest.SetIncludeEntities(nearbyVehicles, numNearbyVehicles, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

	Vector3 downwardEndPosition(m_CollisionOrigin);
	downwardEndPosition.AddScaled(ZAXIS, -settings.m_DistanceToTestDownForVehiclesToReject);

	probeTest.SetStartAndEnd(m_CollisionOrigin, downwardEndPosition);

	const bool hasHitVehicleBelow = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
	if(!hasHitVehicleBelow)
	{
		return;
	}

	for(WorldProbe::ResultIterator downwardResultsIterator=probeTestResults.begin(); downwardResultsIterator<probeTestResults.last_result(); downwardResultsIterator++)
	{
		if(!downwardResultsIterator->GetHitDetected())
		{
			continue;
		}

		const CEntity* hitEntity = downwardResultsIterator->GetHitEntity();
		if(!hitEntity)
		{
			continue;
		}

		//Explicitly ignore the downward check for bikes and quad bikes, as we know that they are convex and we do not want to run the risk of
		//invalidation during ragdoll penetration.
		const CVehicle* hitVehicle = static_cast<const CVehicle*>(hitEntity);
		if(hitVehicle->InheritsFromBike() || hitVehicle->InheritsFromQuadBike() || hitVehicle->InheritsFromAmphibiousQuadBike())
		{
			continue;
		}

		nearbyVehicleList.DeleteMatches(hitEntity);
	}
}
