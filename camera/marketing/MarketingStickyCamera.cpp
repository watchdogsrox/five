//
// camera/marketing/MarketingStickyCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingStickyCamera.h"

#include "input/pad.h"

#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "system/pad.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camMarketingStickyCamera,0xBAC8BDA9)


camMarketingStickyCamera::camMarketingStickyCamera(const camBaseObjectMetadata& metadata)
: camMarketingFreeCamera(metadata)
, m_Metadata(static_cast<const camMarketingStickyCameraMetadata&>(metadata))
, m_RelativeLookAtPosition(VEC3_ZERO)
, m_RelativeLookAtPositionOffset(VEC3_ZERO)
, m_WorldLookAtPosition(VEC3_ZERO)
, m_ShouldUseStickyMode(false)
{
}

void camMarketingStickyCamera::Reset()
{
	camMarketingFreeCamera::Reset();
	m_RelativeLookAtPositionOffset.Zero();

	UpdateLookAt();
}

bool camMarketingStickyCamera::Update()
{
	bool hasSucceeded = UpdateLookAt();
	if(hasSucceeded)
	{
		hasSucceeded = camMarketingFreeCamera::Update();
	}

	return hasSucceeded;
}

bool camMarketingStickyCamera::UpdateLookAt()
{
	m_LookAtTarget				= static_cast<const CEntity*>(camInterface::FindFollowPed());
	const bool hasLookAtTarget	= (m_LookAtTarget.Get() != NULL);
	if(hasLookAtTarget)
	{
		m_LookAtTarget->TransformIntoWorldSpace(m_WorldLookAtPosition, m_RelativeLookAtPosition);
	}

	return hasLookAtTarget;
}

void camMarketingStickyCamera::UpdateMiscControl(CPad& pad)
{
	camMarketingFreeCamera::UpdateMiscControl(pad);

	if(pad.GetPressedDebugButtons() & ioPad::L2)
	{
		//Toggle sticky look-at behavior.
		m_ShouldUseStickyMode = !m_ShouldUseStickyMode;

		if(m_ShouldUseStickyMode)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}

	m_RelativeLookAtPositionOffset.Zero();
	if (m_LookAtTarget)
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
		m_RelativeLookAtPositionOffset += m_Frame.GetUp() * attachOffsetMove.y * timeStep;
		m_RelativeLookAtPositionOffset += m_Frame.GetRight() * attachOffsetMove.x * timeStep;
		// Offset is now in world space, transform to the character's space
		m_RelativeLookAtPositionOffset = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(m_RelativeLookAtPositionOffset)));
	}
}

void camMarketingStickyCamera::ApplyTranslation()
{
	//Only support the base class (free) behavior outside of sticky mode.
	if(!m_ShouldUseStickyMode)
	{
		camMarketingFreeCamera::ApplyTranslation();
	}
	m_RelativeLookAtPosition += m_RelativeLookAtPositionOffset;
}

void camMarketingStickyCamera::ApplyRotation()
{
	if(m_ShouldUseStickyMode)
	{
		const Vector3& worldPosition	= m_Frame.GetPosition();
		const Vector3 positionDelta		= m_WorldLookAtPosition - worldPosition;
		if(positionDelta.Mag2() > SMALL_FLOAT)
		{
			Vector3 front(positionDelta);
			front.Normalize();

			float heading;
			float pitch;
			float roll;
			m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

			camFrame::ComputeHeadingAndPitchFromFront(front, heading, pitch);

			//Apply roll.
			if(m_ShouldResetRoll)
			{
				roll = 0.0f;
			}

			roll	+= m_RollOffset;
			roll	= Clamp(roll, -m_Metadata.m_MaxRoll * DtoR, m_Metadata.m_MaxRoll * DtoR);

			m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
		}
	}
	else
	{
		camMarketingFreeCamera::ApplyRotation(); //Fall-back to the base class behavior.
	}
}

#endif // __BANK
