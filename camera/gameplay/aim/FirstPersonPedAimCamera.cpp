//
// camera/gameplay/aim/FirstPersonPedAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/system/CameraMetadata.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "scene/portals/Portal.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFirstPersonPedAimCamera,0xC6A2F0FA)


camFirstPersonPedAimCamera::camFirstPersonPedAimCamera(const camBaseObjectMetadata& metadata)
: camFirstPersonAimCamera(metadata)
, m_Metadata(static_cast<const camFirstPersonPedAimCameraMetadata&>(metadata))
, m_PreviousRelativeAttachPosition(g_InvalidRelativePosition)
{
}

camFirstPersonPedAimCamera::~camFirstPersonPedAimCamera()
{
	// When destroying this camera, force an update of the scope component (otherwise PostFX will disable a frame late)
	CPed* attachedPed = const_cast<CPed*>(static_cast<const CPed*>(m_AttachParent.Get()));
	if (attachedPed)
	{
		CWeapon* pWeapon = attachedPed->GetWeaponManager() ? attachedPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if (pWeapon && pWeapon->GetScopeComponent())
		{
			pWeapon->GetScopeComponent()->SetRenderingFirstPersonScope(false);
			pWeapon->GetScopeComponent()->UpdateSpecialScope(attachedPed);
		}
	}
}

bool camFirstPersonPedAimCamera::Update()
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypePed()) //Safety check.
	{
		const CPed* attachedPed = camInterface::FindFollowPed();
		if(!attachedPed)
		{
			return false;
		}

		AttachTo(attachedPed);
	}

	CPed* attachedPed = const_cast<CPed*>(static_cast<const CPed*>(m_AttachParent.Get()));
	if(m_Metadata.m_ShouldTorsoIkLimitsOverrideOrbitPitchLimits && !attachedPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
	{
		//Allow the IK pitch limits to override the defaults defined in metadata.
		//NOTE: We do not currently do this during climbing, as the limits are zeroed, which would not allow any pitching.
		m_MinPitch	= attachedPed->GetIkManager().GetTorsoMinPitch();
		m_MaxPitch	= attachedPed->GetIkManager().GetTorsoMaxPitch();
	}

	CVehicle* pVeh = attachedPed->GetVehiclePedInside();
	if (pVeh && pVeh->InheritsFromHeli() && m_Metadata.m_ShouldUseHeliPitchLimits)
	{
		m_MinPitch = m_Metadata.m_MinPitchInHeli * DtoR;
		m_MaxPitch = m_Metadata.m_MaxPitchInHeli * DtoR;
	}

	// Update the scope component directly from here to prevent delays
	CWeapon* pWeapon = attachedPed->GetWeaponManager() ? attachedPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if (pWeapon && pWeapon->GetScopeComponent())
	{
		pWeapon->GetScopeComponent()->SetRenderingFirstPersonScope(true);
		
		// On the first update of this camera, force an update (otherwise PostFX will activate a frame late)
		if (GetNumUpdatesPerformed() == 0)
		{
			pWeapon->GetScopeComponent()->UpdateSpecialScope(attachedPed);
		}
	}

	bool hasSucceeded	= camFirstPersonAimCamera::Update();


#if RSG_PC
	if(camInterface::IsTripleHeadDisplay() && !m_ShouldOverrideNearClipThisUpdate && m_Metadata.m_TripleHeadNearClip > SMALL_FLOAT)
	{
		m_Frame.SetNearClip(m_Metadata.m_TripleHeadNearClip);
	}

#endif

	return hasSucceeded;
}

void camFirstPersonPedAimCamera::CloneBaseOrientation(const camBaseCamera& sourceCamera)
{
	camFirstPersonAimCamera::CloneBaseOrientation(sourceCamera);

	if (sourceCamera.GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
	{
		m_RelativeCoverHeadingSpring.OverrideVelocity(static_cast<const camFirstPersonAimCamera&>(sourceCamera).GetRelativeCoverHeadingSpringVelocity());
	}
}

void camFirstPersonPedAimCamera::UpdateOrientation()
{
	camFirstPersonAimCamera::UpdateOrientation();

	ReorientCameraToPreviousAimDirection();

	float heading;
	float pitch;
	m_Frame.ComputeHeadingAndPitch(heading, pitch);

	CPed& rPed = const_cast<CPed&>(static_cast<const CPed&>(*(m_AttachParent.Get())));

	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint))
	{
		const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
		if (pCoverTask != NULL)
		{
			ProcessCoverAutoHeadingCorrection(rPed, heading, pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft), pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint), 
				CTaskInCover::CanPedFireRoundCorner(rPed, pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft)), pCoverTask->IsCoverFlagSet(CTaskCover::CF_AimDirectly));
		}
	}

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch);
}

void camFirstPersonPedAimCamera::ProcessCoverAutoHeadingCorrection(CPed& rPed, float& heading, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, bool bAimDirectly)
{
	const bool bIsUsingFirstPersonMode = camInterface::GetGameplayDirector().IsFirstPersonModeAllowed() && !CTaskCover::CanUseThirdPersonCoverInFirstPerson(rPed);
	if (!bIsUsingFirstPersonMode)
	{
		if (rPed.GetPlayerInfo() != NULL)
		{
			rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
		}
		m_RelativeCoverHeadingSpring.Reset();
		return;
	}

	float fDesiredHeading = heading;
	
	Vector3 vCoverDir(Vector3::ZeroType);
	if (!bAimDirectly && CCover::FindCoverDirectionForPed(rPed, vCoverDir, VEC3_ZERO))
	{
		if ((rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) || rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAiming)) && (!bInHighCover || bAtCorner))
		{
			// Don't correct our aim if we're ignoring cover auto heading correction or have look-around input
			if (!m_ControlHelper->IsLookAroundInputPresent())
			{
				fDesiredHeading = rPed.GetPlayerInfo() != NULL ? rPed.GetPlayerInfo()->GetCoverHeadingCorrectionAimHeading() : FLT_MAX;

				if (fDesiredHeading == FLT_MAX)
				{
					// Adpot the cover direction by default when we don't have a previous heading to correct to
					fDesiredHeading = rage::Atan2f(-vCoverDir.x, vCoverDir.y);

					// If in high cover at a corner with an angle greater than 90 degrees we need to adjust our desired heading so that
					// we don't end up aiming at the corner we're going around
					if (bInHighCover && bAtCorner && rPed.GetPlayerInfo() != NULL)
					{
						bool bSaveDynamicCoverInsideCorner = rPed.GetPlayerInfo()->DynamicCoverInsideCorner();
						bool bSaveDynamicCoverCanMoveRight = rPed.GetPlayerInfo()->DynamicCoverCanMoveRight();
						bool bSaveDynamicCoverCanMoveLeft = rPed.GetPlayerInfo()->DynamicCoverCanMoveLeft();
						bool bSaveDynamicCoverHitPed = rPed.GetPlayerInfo()->DynamicCoverHitPed();
						bool bSaveDynamicCoverGeneratedByDynamicEntity = rPed.GetPlayerInfo()->IsCoverGeneratedByDynamicEntity();

						CCoverPoint cornerCoverPoint;
						if (CPlayerInfo::ms_DynamicCoverHelper.FindNewCoverPointAroundCorner(&cornerCoverPoint, &rPed, bFacingLeftInCover, !bInHighCover, true))
						{
							Vector3 vCornerCoverHeading = VEC3V_TO_VECTOR3(cornerCoverPoint.GetCoverDirectionVector(NULL));

							float fDotCornerCover = vCoverDir.Dot(vCornerCoverHeading);
							if (fDotCornerCover > 0.0f)
							{
								float fAngleToAdjust = HALF_PI - Acosf(fDotCornerCover);
								fDesiredHeading += bFacingLeftInCover ? fAngleToAdjust : -fAngleToAdjust;
								fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
							}
						}

						rPed.GetPlayerInfo()->SetDynamicCoverInsideCorner(bSaveDynamicCoverInsideCorner);
						rPed.GetPlayerInfo()->SetDynamicCoverCanMoveRight(bSaveDynamicCoverCanMoveRight);
						rPed.GetPlayerInfo()->SetDynamicCoverCanMoveLeft(bSaveDynamicCoverCanMoveLeft);
						rPed.GetPlayerInfo()->SetDynamicCoverHitPed(bSaveDynamicCoverHitPed);
						rPed.GetPlayerInfo()->SetCoverGeneratedByDynamicEntity(bSaveDynamicCoverGeneratedByDynamicEntity);

						fDesiredHeading += bFacingLeftInCover ? m_Metadata.m_HighCoverEdgeCameraAngleAimOffsetDegrees * DtoR : -m_Metadata.m_HighCoverEdgeCameraAngleAimOffsetDegrees * DtoR;
						fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
					}
				}
			}

			if (rPed.GetPlayerInfo() != NULL)
			{
				rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(fDesiredHeading);
			}
		}
	}
	else if (rPed.GetPlayerInfo() != NULL)
	{
		// Reset the stored cover heading so that when we start aiming again we'll just set the cover heading to the current heading
		rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
	}

	// Update the heading value based on the spring model
	// Figure out if we've finished the correction behaviour
	if (Abs(heading - fDesiredHeading) < m_Metadata.m_AimHeadingCorrection.m_DeltaTolerance)
	{
		m_RelativeCoverHeadingSpring.Reset();
	}
	else
	{
		m_RelativeCoverHeadingSpring.OverrideResult(0.0f);
		heading	+= m_RelativeCoverHeadingSpring.Update(SubtractAngleShorter(fDesiredHeading, heading), m_Metadata.m_AimHeadingCorrection.m_SpringConstant, m_Metadata.m_AimHeadingCorrection.m_AwayDampingRatio);

		// Translate back to between -PI and PI to compare the two angles below
		heading = fwAngle::LimitRadianAngle(heading);
	}
}

void camFirstPersonPedAimCamera::ComputeRelativeAttachPosition(const Matrix34& targetMatrix, Vector3& relativeAttachPosition)
{
	//Attach to an offset from a ped bone.

	//Compute the basic relative offset, which is derived from metadata settings.
	camFirstPersonAimCamera::ComputeRelativeAttachPosition(targetMatrix, relativeAttachPosition);

	if(m_Metadata.m_AttachBoneTag >= 0)	//A negative bone tag indicates that the ped matrix should be used.
	{
		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		CPed* attachedPed = (CPed*)m_AttachParent.Get();

		const eAnimBoneTag boneTag = (eAnimBoneTag)m_Metadata.m_AttachBoneTag;

		Matrix34 boneMatrix;
		attachedPed->GetBoneMatrix(boneMatrix, boneTag);

		//NOTE: Only use the z-component of the ped-relative bone position, as we don't want the camera to pick up any other movement.
		Vector3 attachedPedRelativeBonePosition;
		targetMatrix.UnTransform(boneMatrix.d, attachedPedRelativeBonePosition);
		attachedPedRelativeBonePosition.x = 0.0f;
		attachedPedRelativeBonePosition.y = 0.0f;
		Vector3 bonePosition;
		targetMatrix.Transform(attachedPedRelativeBonePosition, bonePosition);

		Vector3 relativeBonePosition;
		TransformAttachPositionToLocalSpace(targetMatrix, bonePosition, relativeBonePosition);

		relativeAttachPosition += relativeBonePosition;
	}

	if(m_PreviousRelativeAttachPosition != g_InvalidRelativePosition)
	{
		//Smooth the target-relative attach position to reduce movement 'noise'.
		//Cheaply make the smoothing frame-rate independent.
		float smoothRate	= m_Metadata.m_RelativeAttachPositionSmoothRate * 30.0f * fwTimer::GetCamTimeStep();
		smoothRate			= Min(smoothRate, 1.0f);
		relativeAttachPosition.Lerp(smoothRate, m_PreviousRelativeAttachPosition, relativeAttachPosition);
	}

	m_PreviousRelativeAttachPosition = relativeAttachPosition;
}

void camFirstPersonPedAimCamera::UpdateInteriorCollision()
{
	// Only needed for sniper camera.
	if( !m_Metadata.m_IsOrientationRelativeToAttached )
	{
		// Generate a line of length 200m down the camera view vector & test for any interiors which
		// intersect it, as these need to have their collisions activated.
		Vector3 cameraPosition			= m_Frame.GetPosition();
		Vector3 mloActivateDirection	= m_Frame.GetFront();
		mloActivateDirection.Normalize();
		mloActivateDirection			*= 200.0f;
		CPortal::ActivateCollisions(cameraPosition, mloActivateDirection);
	}
}

void camFirstPersonPedAimCamera::UpdateDof()
{
	camFirstPersonAimCamera::UpdateDof();

	if(m_Metadata.m_ShouldApplyFocusLock)
	{
		const CControl* activeControl = camInterface::GetGameplayDirector().GetActiveControl();
		if(activeControl)
		{
			const bool isFocusLockActive = activeControl->GetCellphoneCameraFocusLock().IsDown();
			if(isFocusLockActive)
			{
				m_PostEffectFrame.GetFlags().SetFlag(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);
			}
		}
	}
}

void camFirstPersonPedAimCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camFirstPersonAimCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	//NOTE: We only actually measure post-alpha pixel depth when flagged *and* when aiming at a vehicle.
	//- This allows us to avoid blurring out a vehicle when aiming through its windows without introducing more general problems with focusing on things like the windows of buildings.
	if(settings.m_ShouldMeasurePostAlphaPixelDepth)
	{
		bool isAimingAtVehicle = false;
		if(m_AttachParent && m_AttachParent->GetIsTypePed())
		{
			const CPed* attachPed			= static_cast<const CPed*>(m_AttachParent.Get());
			const CPlayerInfo* playerInfo	= attachPed->GetPlayerInfo();
			isAimingAtVehicle				= playerInfo && playerInfo->GetTargeting().IsFreeAimTargetingVehicleForCamera();
		}

		settings.m_ShouldMeasurePostAlphaPixelDepth = isAimingAtVehicle;
	}
}

void camFirstPersonPedAimCamera::ReorientCameraToPreviousAimDirection()
{
	// UpdateOrientation is called after UpdatePosition and UpdateZoom.
	// so, at this point, the camera position and m_IsAiming are valid.
	if( m_NumUpdatesPerformed == 0 &&
		m_AttachParent.Get() && m_AttachParent->GetIsTypePed() &&
		!camInterface::GetGameplayDirector().GetPreviousAimCameraPosition().IsClose(g_InvalidPosition, SMALL_FLOAT) )
	{
		WorldProbe::CShapeTestProbeDesc lineTest;
		WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
		lineTest.SetResultsStructure(&shapeTestResults);
		lineTest.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
		lineTest.SetContext(WorldProbe::LOS_Camera);
		lineTest.SetIsDirected(true);
		const u32 collisionIncludeFlags =	(u32)ArchetypeFlags::GTA_ALL_TYPES_WEAPON |
											(u32)ArchetypeFlags::GTA_RAGDOLL_TYPE |
											(u32)ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
		lineTest.SetIncludeFlags(collisionIncludeFlags);
		lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		const Vector3 playerPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		const Vector3 previousCameraPosToPlayer = playerPosition - camInterface::GetGameplayDirector().GetPreviousAimCameraPosition();
		float fDotProjection = Max(previousCameraPosToPlayer.Dot(camInterface::GetGameplayDirector().GetPreviousAimCameraForward()), 0.0f);
		const Vector3 vProbeStartPosition = camInterface::GetGameplayDirector().GetPreviousAimCameraPosition() + camInterface::GetGameplayDirector().GetPreviousAimCameraForward()*fDotProjection;

		const float maxProbeDistance = 50.0f;
		Vector3 vTargetPoint = vProbeStartPosition+camInterface::GetGameplayDirector().GetPreviousAimCameraForward()*maxProbeDistance;
		lineTest.SetStartAndEnd(vProbeStartPosition, vTargetPoint);
		lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		if( hasHit )
		{
			vTargetPoint = shapeTestResults[0].GetHitPosition();
		}

		Vector3 vNewForward  = vTargetPoint - m_Frame.GetPosition();
		vNewForward.NormalizeSafe();

		m_Frame.SetWorldMatrixFromFront( vNewForward );
	}
}
