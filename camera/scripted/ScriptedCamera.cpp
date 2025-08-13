//
// camera/scripted/ScriptedCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/ScriptedCamera.h"

#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"

#if __BANK
#include "grcore/debugdraw.h"
#endif // __BANK

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camScriptedCamera,0x38E933FA)

const float g_MaxAbsSafePitchForLookAt	= (89.9f * DtoR);
const float g_InvalidRoll				= 9999.0f;


camScriptedCamera::camScriptedCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camScriptedCameraMetadata&>(metadata))
, m_LookAtPosition(VEC3_ZERO)
, m_CachedLookAtPosition(VEC3_ZERO)
, m_AttachOffset(VEC3_ZERO)
, m_AttachRotationOffset(VEC3_ZERO)
, m_LookAtOffset(VEC3_ZERO)
, m_RollEntity(NULL)
, m_OverriddenDofSettings(NULL)
, m_LookAtMode(LOOK_AT_NOTHING)
, m_AttachBoneTag(-1)
, m_LookAtBoneTag(-1)
, m_OverriddenRoll(g_InvalidRoll)
, m_CustomMaxNearClip(0.0f)
, m_OverriddenDofFocusDistance(0.0f)
, m_OverriddenDofFocusDistanceBlendLevel(0.0f)
, m_IsAttachOffsetRelativeToMatrix(false)
, m_IsLookAtOffsetRelativeToMatrix(false)
, m_ShouldAffectAiming(true) //TODO: Default to false as soon as the scripts that require aiming through scripted cameras opt-in using SET_CAM_AFFECTS_AIMING.
, m_ShouldControlMiniMapHeading(false)
, m_IsInsideVehicle(false)
, m_HardAttachment(false)
, m_ShouldOverrideFarClip(false)
{
	m_Frame.SetFov(m_Metadata.m_DefaultFov);

	m_OverriddenDofSettings = rage_new camDepthOfFieldSettingsMetadata;
	if(m_OverriddenDofSettings)
	{
		//Clone the DOF settings that were pulled from the camera metadata and then redirect to the unique settings for this camera, so that they may be overridden.
		if(m_DofSettings)
		{
			*m_OverriddenDofSettings = *m_DofSettings;
		}

		m_DofSettings = m_OverriddenDofSettings;
	}

#if __BANK
	m_szDebugName[0] = 0;
#endif // __BANK
}

camScriptedCamera::~camScriptedCamera()
{
	if(m_OverriddenDofSettings)
	{
		delete m_OverriddenDofSettings;
	}
}

bool camScriptedCamera::Update()
{
	//1) Move camera:
	if(m_AttachParent)
	{
		Matrix34 attachMatrix(Matrix34::IdentityType);
		if((m_AttachBoneTag >= 0) && (m_AttachParent->GetIsTypePed() || m_AttachParent->GetIsTypeVehicle()))
		{
			if(m_AttachParent->GetIsTypePed())
			{
				const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
				attachPed->GetBoneMatrix(attachMatrix, (eAnimBoneTag)m_AttachBoneTag);
			}
			else
			{
				m_AttachParent->GetGlobalMtx(m_AttachBoneTag, attachMatrix);
			}
		}
		else if (m_HardAttachment)
		{
			//grab the full matrix from the entity
			attachMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
		}
		else
		{
			attachMatrix.d = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		}

		if(m_IsAttachOffsetRelativeToMatrix)
		{
			if(m_AttachOffset.IsNonZero())
			{
				attachMatrix.d += VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_AttachOffset)));
			}
		}
		else
		{
			attachMatrix.d += m_AttachOffset;
		}

		if(m_HardAttachment)
		{
			if(m_AttachRotationOffset.Mag2() > SMALL_FLOAT)
			{
				attachMatrix.RotateLocalZ(m_AttachRotationOffset.z);
				attachMatrix.RotateLocalX(m_AttachRotationOffset.x);
				attachMatrix.RotateLocalY(m_AttachRotationOffset.y);
			}

			m_Frame.SetWorldMatrix(attachMatrix);
		}

		m_Frame.SetPosition(attachMatrix.d);
	}

	//2) Look at target:
	Vector3 lookAtPosition;

	switch(m_LookAtMode)
	{
	case LOOK_AT_POSITION:
		if(m_LookAtPosition.IsZero())
		{
			cameraWarningf("A scripted camera is looking at the origin - this is likely to be a bug");
		}

		lookAtPosition = m_LookAtPosition + m_LookAtOffset;
		break;

	case LOOK_AT_ENTITY:
		//It may be valid for the camera to handle entity deletion this way:
		if(m_LookAtTarget)
		{
			if((m_LookAtBoneTag >= 0) && m_LookAtTarget->GetIsTypePed())
			{
				const CPed* lookAtPed = static_cast<const CPed*>(m_LookAtTarget.Get());
				lookAtPed->GetBonePosition(lookAtPosition, (eAnimBoneTag)m_LookAtBoneTag);
			}
			else
			{
				lookAtPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
			}

			if(m_IsLookAtOffsetRelativeToMatrix)
			{
				if(m_LookAtOffset.IsNonZero())
				{
					lookAtPosition += VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_LookAtOffset)));
				}
			}
			else
			{
				lookAtPosition += m_LookAtOffset;
			}
		}
		else
		{
			ClearLookAtTarget();
		}
		break;

	case LOOK_AT_CAMERA:
		if(cameraVerifyf(m_LookAtCamera, "A scripted camera is looking at a NULL camera."))
		{
			lookAtPosition = m_LookAtCamera->GetFrame().GetPosition();

			if(m_IsLookAtOffsetRelativeToMatrix)
			{
				if (m_LookAtOffset.IsNonZero())
				{
					Vector3 offset;
					m_LookAtCamera->GetFrame().GetWorldMatrix().Transform3x3(m_LookAtOffset, offset);
					lookAtPosition += offset;
				}
			}
			else
			{
				lookAtPosition += m_LookAtOffset;
			}
		}
		else
		{
			ClearLookAtTarget();
		}
		break;

	case LOOK_AT_NOTHING:
		break;

	default:
		ClearLookAtTarget(); //Just for safety.
		break;
	}

	if(m_LookAtMode != LOOK_AT_NOTHING)
	{
		//Make the camera matrix look at the target position.
		Vector3 front = lookAtPosition - m_Frame.GetPosition();
		if(front.Mag2() >= VERY_SMALL_FLOAT)
		{
			front.Normalize();

			float heading;
			float pitch;
			camFrame::ComputeHeadingAndPitchFromFront(front, heading, pitch);

			//Limit the pitch to avoid looking along world-up/down, as that would result in aliasing between heading and roll,
			//which can cause confusion in script.
			pitch = Clamp(pitch, -g_MaxAbsSafePitchForLookAt, g_MaxAbsSafePitchForLookAt);

			m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, 0.0f);
		}

		m_CachedLookAtPosition = lookAtPosition;
	}

	if(m_OverriddenRoll != g_InvalidRoll)
	{
		float heading;
		float pitch;
		float roll;
		m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);
		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, m_OverriddenRoll);
	}

	if(m_ShouldOverrideFarClip)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldIgnoreTimeCycleFarClip);
	}

	return true;
}

void camScriptedCamera::AttachTo(const CEntity* parent)
{
	if(m_AttachParent != parent)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);

		if(m_LookAtMode != LOOK_AT_NOTHING)
		{
			//This attachment change will result in an orientation change.
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}

	camBaseCamera::AttachTo(parent);
}

void camScriptedCamera::SetAttachBoneTag(s32 boneTag, bool hardAttachment /* = false */)
{
	if(m_AttachParent && (m_AttachBoneTag != boneTag || hardAttachment != m_HardAttachment))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);

		if(m_LookAtMode != LOOK_AT_NOTHING)
		{
			//This attachment change will result in an orientation change.
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}

	m_AttachBoneTag = boneTag;
	m_HardAttachment = hardAttachment;
}

void camScriptedCamera::SetAttachOffset(const Vector3& offset)
{
	if(m_AttachParent && !m_AttachOffset.IsClose(offset, SMALL_FLOAT))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);

		if(m_LookAtMode != LOOK_AT_NOTHING)
		{
			//This attachment change will result in an orientation change.
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}

	m_AttachOffset = offset;
}

void camScriptedCamera::SetAttachRotationOffset(const Vector3& offset)
{
	if(m_AttachParent && !m_AttachRotationOffset.IsClose(offset, SMALL_FLOAT))
	{
		//This attachment change will result in an orientation change.
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_AttachRotationOffset = DEG2RAD(offset);
}

void camScriptedCamera::SetAttachOffsetRelativeToMatrixState(bool b)
{
	if(m_AttachParent && (m_IsAttachOffsetRelativeToMatrix != b))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);

		if(m_LookAtMode != LOOK_AT_NOTHING)
		{
			//This attachment change will result in an orientation change.
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}
	}

	m_IsAttachOffsetRelativeToMatrix = b;
}

void camScriptedCamera::LookAt(const Vector3& position)			
{
	if((m_LookAtMode != LOOK_AT_POSITION) || !m_LookAtPosition.IsClose(position, SMALL_FLOAT))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_LookAtPosition = position;
	m_LookAtCamera = NULL;
	m_LookAtTarget = NULL;
	m_LookAtMode = LOOK_AT_POSITION;
}

void camScriptedCamera::LookAt(camBaseCamera* camera)
{
	if(!cameraVerifyf(camera != this, "A scripted camera was told to look at itself"))
	{
		return;
	}

	if(camera)
	{
		if((m_LookAtMode != LOOK_AT_CAMERA) || (m_LookAtCamera != camera))
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}

		m_LookAtPosition.Zero();
		m_LookAtCamera = camera;
		m_LookAtTarget = NULL;
		m_LookAtMode = LOOK_AT_CAMERA;
	}
}

void camScriptedCamera::LookAt(const CEntity* target)
{ 
	if(target)
	{
		if((m_LookAtMode != LOOK_AT_ENTITY) || (m_LookAtTarget != target))
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}

		camBaseCamera::LookAt(target);

		m_LookAtPosition.Zero();
		m_LookAtCamera = NULL;
		m_LookAtMode = LOOK_AT_ENTITY;
	}
}

void camScriptedCamera::ClearLookAtTarget()
{
	if(m_LookAtMode != LOOK_AT_NOTHING)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_LookAtPosition.Zero();
	m_LookAtCamera = NULL;
	m_LookAtTarget = NULL;
	m_LookAtMode = LOOK_AT_NOTHING;
}

void camScriptedCamera::SetLookAtBoneTag(s32 boneTag)
{
	if((m_LookAtMode == LOOK_AT_ENTITY) && (m_LookAtBoneTag != boneTag))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_LookAtBoneTag = boneTag;
}

void camScriptedCamera::SetLookAtOffset(const Vector3& offset)
{
	if((m_LookAtMode != LOOK_AT_NOTHING) && !m_LookAtOffset.IsClose(offset, SMALL_FLOAT))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_LookAtOffset = offset;
}

void camScriptedCamera::SetLookAtOffsetRelativeToMatrixState(bool b)
{
	if(((m_LookAtMode == LOOK_AT_ENTITY) || (m_LookAtMode == LOOK_AT_CAMERA)) && (m_IsLookAtOffsetRelativeToMatrix != b))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_IsLookAtOffsetRelativeToMatrix = b;
}

void camScriptedCamera::SetRollTarget(const CEntity* entity)		
{ 
	if(entity)
	{
		if(m_RollEntity != entity)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		}

		m_RollEntity = entity;	
	}
}

void camScriptedCamera::ClearRollTarget()
{
	if(m_RollEntity)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_RollEntity = NULL;
}

void camScriptedCamera::SetRoll(float roll)
{
	if(!IsClose(m_OverriddenRoll, roll))
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_OverriddenRoll = roll;
}

//Clones ... turns THIS camera into a clone of the camera passed in.
void camScriptedCamera::CloneFrom(const camBaseCamera* const sourceCam)
{
	if(sourceCam)
	{
		if(cameraVerifyf(sourceCam->GetIsClassId(camScriptedCamera::GetStaticClassId()), "A scripted camera has been instructed to clone an incompatible camera type"))
		{
			camBaseCamera::CloneFrom(sourceCam);

			const camScriptedCamera* const sourceScriptedCam = static_cast<const camScriptedCamera* const>(sourceCam);

			switch(sourceScriptedCam->m_LookAtMode)
			{
			case LOOK_AT_POSITION:
				LookAt(sourceScriptedCam->m_LookAtPosition);
				break;

			case LOOK_AT_ENTITY:
				LookAt(sourceScriptedCam->GetLookAtTarget());
				break;

			case LOOK_AT_CAMERA:
				LookAt(sourceScriptedCam->m_LookAtCamera);
				break;

			default:
				ClearLookAtTarget();
				break;
			}

			AttachTo(sourceScriptedCam->GetAttachParent());

			if(sourceScriptedCam->m_RollEntity)
			{
				SetRollTarget(sourceScriptedCam->m_RollEntity);
			}
			else
			{
				ClearRollTarget();
			}

			m_AttachOffset = sourceScriptedCam->m_AttachOffset;
			m_LookAtOffset = sourceScriptedCam->m_LookAtOffset;
			m_IsAttachOffsetRelativeToMatrix = sourceScriptedCam->m_IsAttachOffsetRelativeToMatrix;
			m_IsLookAtOffsetRelativeToMatrix = sourceScriptedCam->m_IsLookAtOffsetRelativeToMatrix;

			//NOTE: This interface is used to implement interpolation of frame parameters, so we should not report a cut on the first update.
			if(m_NumUpdatesPerformed > 0)
			{
				m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
			}

			//NOTE: Multiplayer-only, as the single player game is now locked.
			if (NetworkInterface::IsGameInProgress())
			{
				SetIsInsideVehicle(sourceScriptedCam->IsInsideVehicle());
			}
		}
	}
}

void camScriptedCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	camBaseCamera::ComputeDofOverriddenFocusSettings(focusPosition, blendLevel);

	if(m_OverriddenDofFocusDistanceBlendLevel < SMALL_FLOAT)
	{
		return;
	}

	//NOTE: We cannot seamlessly blend from one custom focus position and blend level to another without an additional overall blend level, so we just apply any
	//script override explicitly for simplicity. This does not allow us to support blending between focusing on a look-at or attach entity and an overridden focus distance.

	Vector3 localFront = YAXIS * m_OverriddenDofFocusDistance;
	m_PostEffectFrame.GetWorldMatrix().Transform(localFront, focusPosition); 

	blendLevel = m_OverriddenDofFocusDistanceBlendLevel;
}
