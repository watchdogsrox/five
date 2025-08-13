//
// camera/marketing/MarketingOrbitCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingOrbitCamera.h"

#include "input/pad.h"

#include "camera/helpers/SpringMount.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "scene/EntityIterator.h"
#include "system/pad.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camMarketingOrbitCamera,0xAC18397A)


camMarketingOrbitCamera::camMarketingOrbitCamera(const camBaseObjectMetadata& metadata)
: camMarketingFreeCamera(metadata)
, m_Metadata(static_cast<const camMarketingOrbitCameraMetadata&>(metadata))
, m_RelativeLookatPosition(VEC3_ZERO)
, m_RelativeLookatPositionOffset(VEC3_ZERO)
, m_ShouldUseSpringMount(true)
{
	m_SpringMount = camFactory::CreateObject<camSpringMount>(m_Metadata.m_SpringMountRef.GetHash());
	cameraAssertf(m_SpringMount, "A marketing orbit camera (name: %s, hash: %u) failed to create a spring mount (name: %s, hash: %u)",
		GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_SpringMountRef.GetCStr()), m_Metadata.m_SpringMountRef.GetHash());
}

camMarketingOrbitCamera::~camMarketingOrbitCamera()
{
	if(m_SpringMount)
	{
		delete m_SpringMount;
	}
}

void camMarketingOrbitCamera::Reset()
{
	camMarketingFreeCamera::Reset();

	m_RelativeLookatPositionOffset.Zero();
}

void camMarketingOrbitCamera::UpdateMiscControl(CPad& pad)
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
				Vector3 cameraVector = worldPosition;
				Vector3 lookatPoint;
				m_AttachParent->TransformIntoWorldSpace(lookatPoint, m_RelativeLookatPosition);
				cameraVector -= lookatPoint;
				m_CameraDistance = cameraVector.Mag();

				cameraVector.Normalize();
				m_Heading = Atan2f(-cameraVector.x, cameraVector.y);
				m_Pitch = Atan2f( cameraVector.z, sqrtf(cameraVector.y*cameraVector.y+cameraVector.x*cameraVector.x) );

				m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
			}
		}
	}

	m_RelativeLookatPositionOffset.Zero();
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
		m_RelativeLookatPositionOffset += m_Frame.GetUp() * attachOffsetMove.y * timeStep;
		m_RelativeLookatPositionOffset += m_Frame.GetRight() * attachOffsetMove.x * timeStep;
		// Offset is now in world space, transform to the character's space
		m_RelativeLookatPositionOffset = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(m_RelativeLookatPositionOffset)));
	}
}

void camMarketingOrbitCamera::ApplyTranslation()
{
	if(m_AttachParent)
	{
		m_RelativeLookatPosition += m_RelativeLookatPositionOffset;

		Vector3 worldLookatPosition;
		m_AttachParent->TransformIntoWorldSpace(worldLookatPosition, m_RelativeLookatPosition);

		m_Heading += m_LookAroundHeadingOffset;
		m_Pitch += m_LookAroundPitchOffset;
		m_Pitch = Clamp(m_Pitch, -(HALF_PI-0.1f), HALF_PI-0.1f);

		m_RelativeCameraPosition = Vector3(0.0f, m_CameraDistance, 0.0f);
		m_RelativeCameraPosition.RotateX(m_Pitch);
		m_RelativeCameraPosition.RotateZ(m_Heading);

		m_Frame.SetPosition(worldLookatPosition + m_RelativeCameraPosition);
	}
	else
	{
		camMarketingFreeCamera::ApplyTranslation(); //Fall-back to the base class behavior.
	}
}

void camMarketingOrbitCamera::ApplyRotation()
{
	if(m_AttachParent)
	{
		Matrix34 worldMatrix;
		worldMatrix.d = VEC3_ZERO;
		camFrame::ComputeWorldMatrixFromFront( -m_RelativeCameraPosition, worldMatrix );

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

		m_Frame.SetWorldMatrix(worldMatrix, true/*only set the rotation 3x3*/);
	}
	else
	{
		camMarketingFreeCamera::ApplyRotation(); //Fall-back to the base class behavior.
	}
}

void camMarketingOrbitCamera::AddWidgetsForInstance()
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
