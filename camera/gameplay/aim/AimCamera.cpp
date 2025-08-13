//
// camera/gameplay/aim/AimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/AimCamera.h"

#include "grcore/debugdraw.h"
#include "vector/colors.h"

#include "fwmaths/angle.h"

#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "Peds/ped.h"
#include "scene/Entity.h"
#include "Vehicles/vehicle.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camAimCamera,0xFE1B55FD)

const float g_MaxAbsSafePitch = (89.5f * DtoR);


camAimCamera::camAimCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camAimCameraMetadata&>(metadata))
, m_ControlHelper(NULL)
, m_AttachPosition(VEC3_ZERO)
, m_OverriddenRelativeHeadingLimitsThisUpdate(0.0f, 0.0f)
, m_OverriddenRelativePitchLimitsThisUpdate(0.0f, 0.0f)
, m_MinPitch(m_Metadata.m_MinPitch * DtoR)
, m_MaxPitch(m_Metadata.m_MaxPitch * DtoR)
, m_DesiredMinPitch(m_Metadata.m_MinPitch * DtoR)
, m_DesiredMaxPitch(m_Metadata.m_MaxPitch * DtoR)
, m_OverriddenRelativeHeading(0.0f)		//Start with the camera directly behind the attach parent.
, m_OverriddenRelativePitch(0.0f)		// ...
, m_ZoomFovDelta(0.0f)
, m_OverriddenNearClip(m_Metadata.m_BaseNearClip)
, m_ShouldOverrideRelativeHeading(true)	//Start with the camera directly behind the attach parent.
, m_ShouldOverrideRelativePitch(true)	// ...
, m_ShouldOverrideRelativeHeadingLimitsThisUpdate(false)
, m_ShouldOverrideRelativePitchLimitsThisUpdate(false)
, m_ShouldOverrideNearClipThisUpdate(false)
, m_WasRelativeHeadingLimitsOverriddenThisUpdate(false)
, m_WasRelativePitchLimitsOverriddenThisUpdate(false)
{
	m_ControlHelper = camFactory::CreateObject<camControlHelper>(m_Metadata.m_ControlHelperRef.GetHash());
	cameraAssertf(m_ControlHelper, "An aim camera (name: %s, hash: %u) failed to create a control helper (name: %s, hash: %u)",
		GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_ControlHelperRef.GetCStr()), m_Metadata.m_ControlHelperRef.GetHash());
}

camAimCamera::~camAimCamera()
{
	if(m_ControlHelper)
	{
		delete m_ControlHelper;
	}
}

bool camAimCamera::Update()
{
	if(!m_AttachParent.Get() || !m_ControlHelper)
	{
		return false;
	}

	// Savem to be read by CPed later in update.
	m_WasRelativeHeadingLimitsOverriddenThisUpdate = m_ShouldOverrideRelativeHeadingLimitsThisUpdate;
	m_WasRelativePitchLimitsOverriddenThisUpdate   = m_ShouldOverrideRelativePitchLimitsThisUpdate;

	UpdateControlHelper();
	UpdatePosition();
	UpdateZoom();
	UpdateOrientation();
	UpdateMotionBlur();

	m_Frame.ComputeDofFromFovAndClipRange();

	if(m_ShouldOverrideNearClipThisUpdate)
	{
		m_Frame.SetNearClip(m_OverriddenNearClip);
	}
	else
	{
		m_Frame.SetNearClip(m_Metadata.m_BaseNearClip);
	}

	//Auto-reset overriding of the orientation limits.
	m_ShouldOverrideRelativeHeadingLimitsThisUpdate	= false;
	m_ShouldOverrideRelativePitchLimitsThisUpdate	= false;
	m_ShouldOverrideNearClipThisUpdate				= false; 
	return true;
}

void camAimCamera::ComputeRelativeAttachPosition(const Matrix34& UNUSED_PARAM(targetMatrix), Vector3& relativeAttachPosition)
{
	relativeAttachPosition = m_Metadata.m_AttachRelativeOffset;
}

void camAimCamera::TransformAttachPositionToWorldSpace(const Matrix34& targetMatrix, const Vector3& relativeAttachPosition, Vector3& attachPosition) const
{
	bool shouldApplyRelativeToCamera = ComputeShouldApplyAttachOffsetRelativeToCamera();
	if(shouldApplyRelativeToCamera)
	{
		//Transform the position from parent-local space (to avoid translation feedback) and orientation from camera-space.
		const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();
		worldMatrix.Transform3x3(relativeAttachPosition, attachPosition);
		attachPosition += targetMatrix.d;
	}
	else
	{
		//Transform from parent-local space.
		targetMatrix.Transform(relativeAttachPosition, attachPosition); 
	}
}

void camAimCamera::TransformAttachPositionToLocalSpace(const Matrix34& targetMatrix, const Vector3& attachPosition, Vector3& relativeAttachPosition) const
{
	bool shouldApplyRelativeToCamera = ComputeShouldApplyAttachOffsetRelativeToCamera();
	if(shouldApplyRelativeToCamera)
	{
		//Transform the position to be target-local (to avoid translation feedback) and orientation to be camera-local.
		relativeAttachPosition = attachPosition - targetMatrix.d;
		const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();
		worldMatrix.UnTransform3x3(relativeAttachPosition);
	}
	else
	{
		//Transform to parent-local space.
		targetMatrix.UnTransform(attachPosition, relativeAttachPosition);
	}
}

void camAimCamera::TransformAttachOffsetToWorldSpace(const Matrix34& targetMatrix, const Vector3& relativeAttachOffset, Vector3& attachOffset) const
{
	bool shouldApplyRelativeToCamera = ComputeShouldApplyAttachOffsetRelativeToCamera();
	if(shouldApplyRelativeToCamera)
	{
		//Transform the orientation from camera-space.
		const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();
		worldMatrix.Transform3x3(relativeAttachOffset, attachOffset);
	}
	else
	{
		//Transform from parent-local space.
		targetMatrix.Transform3x3(relativeAttachOffset, attachOffset); 
	}
}

void camAimCamera::TransformAttachOffsetToLocalSpace(const Matrix34& targetMatrix, const Vector3& attachOffset, Vector3& relativeAttachOffset) const
{
	bool shouldApplyRelativeToCamera = ComputeShouldApplyAttachOffsetRelativeToCamera();
	if(shouldApplyRelativeToCamera)
	{
		//Transform the orientation to be camera-local.
		const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();
		worldMatrix.UnTransform3x3(attachOffset, relativeAttachOffset);
	}
	else
	{
		//Transform to parent-local space.
		targetMatrix.UnTransform3x3(attachOffset, relativeAttachOffset);
	}
}

bool camAimCamera::ComputeShouldApplyAttachOffsetRelativeToCamera() const
{
	return m_Metadata.m_ShouldApplyAttachOffsetRelativeToCamera;
}

void camAimCamera::UpdateControlHelper()
{
	m_ControlHelper->Update(*m_AttachParent);
}

void camAimCamera::UpdateZoom()
{
	//NOTE: m_ControlHelper has already been validated.
	const bool isUsingZoomInput	= m_ControlHelper->IsUsingZoomInput();
	const float fov				= isUsingZoomInput ? m_ControlHelper->GetZoomFov() : m_Metadata.m_BaseFov;

	m_Frame.SetFov(fov);
}

void camAimCamera::ApplyOverriddenOrientation(float& heading, float& pitch)
{
	//NOTE: m_AttachParent has already been validated.
	if(m_ShouldOverrideRelativeHeading)
	{
		float targetHeading				= m_AttachParent->GetTransform().GetHeading();
		heading							= fwAngle::LimitRadianAngleSafe(targetHeading + m_OverriddenRelativeHeading); //make sure we ends up between -pi and pi
		m_ShouldOverrideRelativeHeading	= false;

		//NOTE: We do not report an orientation cut on the first update, as this interface is used to clone base orientation.
		if(m_NumUpdatesPerformed > 0)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}

	if(m_ShouldOverrideRelativePitch)
	{
		float targetPitch				= m_AttachParent->GetTransform().GetPitch();
		pitch							= targetPitch + m_OverriddenRelativePitch;
		m_ShouldOverrideRelativePitch		= false;

		//NOTE: We do not report an orientation cut on the first update, as this interface is used to clone base orientation.
		if(m_NumUpdatesPerformed > 0)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}
}

void camAimCamera::ApplyOrientationLimits(const Matrix34& targetMatrix, float& heading, float& pitch) const
{
	Vector2 headingLimits;
	if(m_ShouldOverrideRelativeHeadingLimitsThisUpdate)
	{
		headingLimits.Set(m_OverriddenRelativeHeadingLimitsThisUpdate);
	}
	else
	{
		headingLimits.Set(m_Metadata.m_MinRelativeHeading, m_Metadata.m_MaxRelativeHeading);
		headingLimits.Scale(DtoR);
	}

	const float thresholdForHeadingLimiting = PI - SMALL_FLOAT;
	if((headingLimits.x >= -thresholdForHeadingLimiting) || (headingLimits.y <= thresholdForHeadingLimiting))
	{
		//Limit the target-relative heading.
		float targetHeading			= camFrame::ComputeHeadingFromMatrix(targetMatrix);
		float relativeOrbitHeading	= heading - targetHeading;
		relativeOrbitHeading		= fwAngle::LimitRadianAngle(relativeOrbitHeading);

		relativeOrbitHeading		= Clamp(relativeOrbitHeading, headingLimits.x, headingLimits.y);

		heading						= targetHeading + relativeOrbitHeading;
	}

	heading	= fwAngle::LimitRadianAngle(heading);

	if(m_ShouldOverrideRelativePitchLimitsThisUpdate)
	{
		//Limit the target-relative pitch.
		float targetPitch			= camFrame::ComputePitchFromMatrix(targetMatrix);
		float relativeOrbitPitch	= pitch - targetPitch;

		relativeOrbitPitch			= Clamp(relativeOrbitPitch, m_OverriddenRelativePitchLimitsThisUpdate.x, m_OverriddenRelativePitchLimitsThisUpdate.y);

		pitch						= targetPitch + relativeOrbitPitch;
	}
	else
	{
		pitch = Clamp(pitch, m_MinPitch, m_MaxPitch);
	}

	//Limit to just short of +/-90 degrees for safety.
	pitch = Clamp(pitch, -g_MaxAbsSafePitch, g_MaxAbsSafePitch);
}

void camAimCamera::UpdateMotionBlur()
{
	float motionBlurStrength		= (m_NumUpdatesPerformed >= m_Metadata.m_MinUpdatesBeforeApplyingMotionBlur) ?
										m_Metadata.m_BaseMotionBlurStrength : 0.0f;

	float zoomMotionBlurStrength	= RampValueSafe(m_ZoomFovDelta, m_Metadata.m_ZoomMotionBlurMinFovDelta, m_Metadata.m_ZoomMotionBlurMaxFovDelta,
										0.0f, m_Metadata.m_ZoomMotionBlurMaxStrengthForFov);
	motionBlurStrength				+= zoomMotionBlurStrength;

	m_Frame.SetMotionBlurStrength(motionBlurStrength);
}

void camAimCamera::CloneBaseOrientation(const camBaseCamera& sourceCamera)
{
	if(sourceCamera.ShouldPreventNextCameraCloningOrientation())
	{
		return;
	}

	//Clone the 'base' front of the source camera, which is independent of any non-propagating offsets.
	Vector3 baseFront = sourceCamera.GetBaseFront();

	const camControlHelper* sourceControlHelper = NULL;
	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camThirdPersonCamera&>(sourceCamera).GetControlHelper();
	}
	else if(sourceCamera.GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camCinematicMountedCamera&>(sourceCamera).GetControlHelper();
		if(sourceControlHelper && sourceControlHelper->IsLookingBehind())
		{
			//Compensate for the look-behind flip, as the base front should not contain this.
			baseFront = -baseFront;
		}
	}

	if(sourceControlHelper && sourceControlHelper->IsLookingBehind())
	{
		//NOTE: We need to manually flip the front for look-behind as the base front ignores this.
		baseFront = -baseFront;
	}

	m_Frame.SetWorldMatrixFromFront(baseFront);

	//Ignore any pending orientation override requests and respect the cloned orientation.
	m_ShouldOverrideRelativeHeading	= false;
	m_ShouldOverrideRelativePitch	= false;
}

void camAimCamera::CloneBaseOrientationFromSeat(const camBaseCamera& sourceCamera)
{
	if(sourceCamera.ShouldPreventNextCameraCloningOrientation())
	{
		return;
	}

	bool bOrientationSet = false;
	if(m_AttachParent.Get() && m_AttachParent->GetIsTypePed())
	{
		const CPed* pFollowPed = (const CPed*)m_AttachParent.Get();
		const CVehicle* pAttachVehicle = pFollowPed->GetVehiclePedInside();
		if(pAttachVehicle && pAttachVehicle->GetVehicleModelInfo() )
		{
			s32 attachSeatIndex = pFollowPed->GetAttachCarSeatIndex();
			const CModelSeatInfo* pModelSeatInfo = pAttachVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
			if(attachSeatIndex >= 0 && pModelSeatInfo)
			{
				const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( attachSeatIndex );
				if(iSeatBoneIndex > -1) 
				{ 
					Matrix34 seatMtx;
					pAttachVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);
					m_Frame.SetWorldMatrix(seatMtx, true);
					bOrientationSet = true;
				}
			}
		}
	}

	if(!bOrientationSet)
	{
		CloneBaseOrientation(sourceCamera);
		return;
	}

	//Ignore any pending orientation override requests and respect the cloned orientation.
	m_ShouldOverrideRelativeHeading	= false;
	m_ShouldOverrideRelativePitch	= false;
}

void camAimCamera::CloneControlSpeeds(const camBaseCamera& sourceCamera)
{
	if(!m_ControlHelper)
	{
		return;
	}

	const camControlHelper* sourceControlHelper	= NULL;

	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camThirdPersonCamera&>(sourceCamera).GetControlHelper();
	}
	else if(sourceCamera.GetIsClassId(camAimCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camAimCamera&>(sourceCamera).GetControlHelper();
	}

	if(sourceControlHelper)
	{
		m_ControlHelper->CloneSpeeds(*sourceControlHelper);
	}
}

#if __BANK
//Render the camera so we can see it.
void camAimCamera::DebugRender()
{
	camBaseCamera::DebugRender();

	//NOTE: These elements are drawn even if this is the active/rendering camera. This is necessary in order to debug the camera when active.
	static float radius = 0.125f;
	grcDebugDraw::Sphere(RCC_VEC3V(m_AttachPosition), radius, Color_blue);
}
#endif // __BANK
