//
// camera/marketing/MarketingMountedCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingMountedCamera.h"

#include "input/pad.h"

#include "camera/helpers/SpringMount.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "scene/EntityIterator.h"
#include "system/pad.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camMarketingMountedCamera,0x25C0C2A3)


camMarketingMountedCamera::camMarketingMountedCamera(const camBaseObjectMetadata& metadata)
: camMarketingFreeCamera(metadata)
, m_Metadata(static_cast<const camMarketingMountedCameraMetadata&>(metadata))
, m_RelativeAttachPosition(VEC3_ZERO)
, m_RelativeAttachPositionOffset(VEC3_ZERO)
, m_RelativeHeading(0.0f)
, m_RelativePitch(0.0f)
, m_ShouldUseSpringMount(true)
{
	m_SpringMount = camFactory::CreateObject<camSpringMount>(m_Metadata.m_SpringMountRef.GetHash());
	cameraAssertf(m_SpringMount, "A marketing mounted camera (name: %s, hash: %u) failed to create a spring mount (name: %s, hash: %u)",
		GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_SpringMountRef.GetCStr()), m_Metadata.m_SpringMountRef.GetHash());
}

camMarketingMountedCamera::~camMarketingMountedCamera()
{
	if(m_SpringMount)
	{
		delete m_SpringMount;
	}
}

void camMarketingMountedCamera::Reset()
{
	camMarketingFreeCamera::Reset();

	m_RelativeAttachPositionOffset.Zero();
	m_RelativeHeading	= 0.0f;
	m_RelativePitch		= 0.0f;
}

void camMarketingMountedCamera::UpdateMiscControl(CPad& pad)
{
	camMarketingFreeCamera::UpdateMiscControl(pad);

	if(pad.GetPressedDebugButtons() & ioPad::L2)
	{
		//Toggle attachment.
		if(m_AttachParent)
		{
			m_AttachParent = NULL;
		}
		else
		{
			//Mount to the nearest entity and maintain the current relative position.
			const Vector3& worldPosition = m_Frame.GetPosition();
			m_AttachParent = FindNearestEntityToPosition(worldPosition, m_Metadata.m_MaxDistanceToAttachParent);
			if(m_AttachParent)
			{
				m_RelativeAttachPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().UnTransform(VECTOR3_TO_VEC3V(worldPosition)));

				m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
			}

			//Reset the relative orientation.
			m_RelativeHeading	= 0.0f;
			m_RelativePitch		= 0.0f;
		}
	}

	m_RelativeAttachPositionOffset.Zero();
	if (m_AttachParent)
	{
		Vector2 attachOffsetMove(0.0f,0.0f);
		if (pad.GetDebugButtons() & ioPad::LUP)
			// move the attachment point up
			attachOffsetMove.y = 1.0f;
		if (pad.GetDebugButtons() & ioPad::LDOWN)
			// move the attachment point down
			attachOffsetMove.y += -1.0f;
		if (pad.GetDebugButtons() & ioPad::LLEFT)
			// move the attachment point left (relative to camera direction)
			attachOffsetMove.x = -1.0f;
		if (pad.GetDebugButtons() & ioPad::LRIGHT)
			// move the attachment point down (relative to camera direction)
			attachOffsetMove.x += 1.0f;

		//Apply attach offset movement, camera direction relative.
		const float timeStep	= fwTimer::GetNonPausableCamTimeStep();
		m_RelativeAttachPositionOffset += m_Frame.GetUp() * attachOffsetMove.y * timeStep;
		m_RelativeAttachPositionOffset += m_Frame.GetRight() * attachOffsetMove.x * timeStep;
		// Offset is now in world space, transform to the character's space
		m_RelativeAttachPositionOffset = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(m_RelativeAttachPositionOffset)));
	}
}

void camMarketingMountedCamera::ApplyTranslation()
{
	if(m_AttachParent)
	{
		m_RelativeAttachPosition += m_RelativeAttachPositionOffset;

		Vector3 worldPosition;
		m_AttachParent->TransformIntoWorldSpace(worldPosition, m_RelativeAttachPosition);

		m_Frame.SetPosition(worldPosition);
	}
	else
	{
		camMarketingFreeCamera::ApplyTranslation(); //Fall-back to the base class behavior.
	}
}

void camMarketingMountedCamera::ApplyRotation()
{
	if(m_AttachParent)
	{
		Matrix34 worldMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

		if(m_SpringMount)
		{
			if(m_ShouldUseSpringMount && m_AttachParent->GetIsPhysical())
			{
				//Apply the spring mount.
				const CPhysical* parentPhysical = static_cast<const CPhysical*>(m_AttachParent.Get());
				m_SpringMount->Update(worldMatrix, *parentPhysical);
			}
			else
			{
				//Reset the spring mount.
				m_SpringMount->Reset();
			}
		}

		//NOTE: We scale the look-around offsets based upon the current FOV, as this allows the look-around input to be responsive at minimum zoom without
		//being too fast at maximum zoom.
		float fovOrientationScaling = m_Fov / m_Metadata.m_ZoomDefaultFov;

		m_RelativeHeading	+= m_LookAroundHeadingOffset * fovOrientationScaling;
		m_RelativeHeading	= fwAngle::LimitRadianAngle(m_RelativeHeading);

		worldMatrix.RotateLocalZ(m_RelativeHeading);

		m_RelativePitch		+= m_LookAroundPitchOffset * fovOrientationScaling;
		m_RelativePitch		= Clamp(m_RelativePitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);

		worldMatrix.RotateLocalX(m_RelativePitch);

		m_Frame.SetWorldMatrix(worldMatrix);
	}
	else
	{
		camMarketingFreeCamera::ApplyRotation(); //Fall-back to the base class behavior.
	}
}

void camMarketingMountedCamera::AddWidgetsForInstance()
{
	camMarketingFreeCamera::AddWidgetsForInstance();

	if(m_WidgetGroup)
	{
		//Extend the widget group created by the base implementation.
		bkBank* bank = BANKMGR.FindBank("Marketing Tools");
		if(bank)
		{
			bank->SetCurrentGroup(*m_WidgetGroup);
			{
				bank->AddSeparator();
				bank->AddTitle("Mount settings");

				bank->AddToggle("Use spring mount", &m_ShouldUseSpringMount);
			}
			bank->UnSetCurrentGroup(*m_WidgetGroup);
		}
	}
}

#endif // __BANK
