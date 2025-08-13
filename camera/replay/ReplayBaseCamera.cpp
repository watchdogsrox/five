//
// camera/replay/ReplayBaseCamera.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/CamInterface.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/Collision.h"
#include "camera/replay/ReplayBaseCamera.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"

#include "control/replay/IReplayMarkerStorage.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayMarkerInfo.h"
#include "control/replay/ReplayMarkerContext.h"
#include "control/replay/PacketBasics.h"
#include "Peds/Ped.h"


CAMERA_OPTIMISATIONS();

INSTANTIATE_RTTI_CLASS(camReplayBaseCamera,0x1AE4AC3D)

camReplayBaseCamera::camReplayBaseCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camReplayBaseCameraMetadata&>(metadata))
#if GTA_REPLAY
, m_MarkerIndex(-1)
, m_DofMarkerIndex(-1)
, m_NumReplayUpdatesPerformed(0)
, m_WaitingToInit(true)
, m_ForcingDefaultFrame(false)
#endif // GTA_REPLAY
{
}

bool camReplayBaseCamera::Update()
{
#if GTA_REPLAY
	IReplayMarkerStorage* markerStorage	= CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo		= markerStorage ? markerStorage->TryGetMarker(m_MarkerIndex) : NULL;

	if (!markerInfo)
	{
		return false;
	}

	if (m_WaitingToInit)
	{
		Init(m_MarkerIndex, m_ForcingDefaultFrame);
	}

	if (m_WaitingToInit)
	{
		return false;
	}

	//if setup just finished, or finished some time ago but this is the camera's first frame
	if ((camReplayDirector::WasReplayPlaybackSetupFinishedThisUpdate() || (m_NumReplayUpdatesPerformed == 0 && (camReplayDirector::IsReplayPlaybackSetupFinished() || CReplayMgr::IsFineScrubbing())))
		&& IsClose(markerInfo->GetNonDilatedTimeMs(), (float)CReplayMgr::GetCurrentTimeRelativeMs(), 1.0f - SMALL_FLOAT)) //only validate entities if we're at the start of a marker when loading has finished
	{
		ValidateEntities(*markerInfo);
	}

	return true;
#else
	return false;
#endif // GTA_REPLAY
}

void camReplayBaseCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	camBaseCamera::ComputeDofOverriddenFocusSettings(focusPosition, blendLevel);

#if GTA_REPLAY
	//NOTE: makerStorage and markerInfo have already been verified.
    const s32 markerIndex = m_DofMarkerIndex != -1 ? m_DofMarkerIndex : m_MarkerIndex;
    sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(markerIndex);

	if (markerInfo->GetDofMode() != MARKER_DOF_MODE_CUSTOM)
	{
		return;
	}

	const u8 focusMode = markerInfo->GetFocusMode();
	
	if (focusMode == MARKER_FOCUS_AUTO)
	{
		return;
	}

	if (focusMode == MARKER_FOCUS_MANUAL)
	{
		blendLevel = 1.0f;

		const Vector3& cameraPosition = m_PostEffectFrame.GetPosition();
		const Vector3& cameraFront = m_PostEffectFrame.GetFront();
		focusPosition = cameraPosition + (cameraFront * markerInfo->GetFocalDistance());

		return;
	}

	if (focusMode == MARKER_FOCUS_TARGET)
	{
		const CEntity* focusEntity = CReplayMgr::GetEntity(markerInfo->GetFocusTargetId());

		if(!focusEntity)
		{
			blendLevel = 0.0f;
			return;
		}

		blendLevel = 1.0f;

		const fwTransform& focusEntityTransform	= focusEntity->GetTransform();
		focusPosition							= VEC3V_TO_VECTOR3(focusEntityTransform.GetPosition());

		if(focusEntity->GetIsTypePed())
		{
			//Focus on the ped's head position at the end of the previous update. It would be preferable to focus on the plane of the eyes that will be rendered this update,
			//however the update order of the game can result in a ped's head position being resolved after the camera update, which makes it impractical to query the 'live'
			//eye positions here. Any such queries can give results based upon an un-posed or non-final skeleton.

			Matrix34 focusPedHeadBoneMatrix;
			const CPed* focusPed		= static_cast<const CPed*>(focusEntity);
			const bool isHeadBoneValid	= focusPed->GetBoneMatrixCached(focusPedHeadBoneMatrix, BONETAG_HEAD);
			if(isHeadBoneValid)
			{
				focusPosition.Set(focusPedHeadBoneMatrix.d);
			}
		}
	}
	
#endif // GTA_REPLAY
}

const CEntity* camReplayBaseCamera::ComputeDofOverriddenFocusEntity() const
{
	const CEntity* focusEntity = camBaseCamera::ComputeDofOverriddenFocusEntity();

#if GTA_REPLAY
	//NOTE: makerStorage and markerInfo have already been verified.
    const s32 markerIndex = m_DofMarkerIndex != -1 ? m_DofMarkerIndex : m_MarkerIndex;
	sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(markerIndex);

	if (markerInfo->GetDofMode() != MARKER_DOF_MODE_CUSTOM)
	{
		return NULL;
	}

	const u8 focusMode = markerInfo->GetFocusMode();
	switch (focusMode)
	{
	case MARKER_FOCUS_TARGET:
		focusEntity = CReplayMgr::GetEntity(markerInfo->GetFocusTargetId());
		break;

	case MARKER_FOCUS_AUTO:
		// fall through
	case MARKER_FOCUS_MANUAL:
		// fall through
	default:
		focusEntity = NULL;
		break;
	}
#endif // GTA_REPLAY

	return focusEntity;
}

void camReplayBaseCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
#if GTA_REPLAY
	//NOTE: makerStorage and markerInfo have already been verified.
    const s32 markerIndex = m_DofMarkerIndex != -1 ? m_DofMarkerIndex : m_MarkerIndex;
    sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(markerIndex);

	if (markerInfo->GetDofMode() == MARKER_DOF_MODE_NONE)
	{
		return;
	}
#endif // GTA_REPLAY

	camBaseCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

#if GTA_REPLAY
	if (markerInfo->GetDofMode() == MARKER_DOF_MODE_DEFAULT)
	{
		return;
	}

	u32 dofSettingsHash = 0;
	camInterface::GetReplayDirector().GetDofSettingsHash(dofSettingsHash);

	const camDepthOfFieldSettingsMetadata* dofSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(dofSettingsHash);

	if(!dofSettings)
	{
		return;
	}
	
	settings = *dofSettings;
	const float dofIntensityLevel		= markerInfo->GetDofIntensity() / 100.f;

	float minFNumber, maxFNumber, fNumberExponent, minFocalLengthMultiplier, maxFocalLengthMultiplier;
	camInterface::GetReplayDirector().GetDofMinMaxFNumberAndFocalLengthMultiplier(minFNumber, maxFNumber, fNumberExponent, minFocalLengthMultiplier, maxFocalLengthMultiplier);

	//only use fNumberExponent if we're in manual focus mode
	if (markerInfo->GetFocusMode() != MARKER_FOCUS_MANUAL)
	{
		fNumberExponent = 1.0f;
	}
	
	const float dofIntensityLevelForFNumber = 1.0f - powf(1.0f - dofIntensityLevel, fNumberExponent);

	settings.m_FNumberOfLens			= RampValueSafe(dofIntensityLevelForFNumber, 0.0f, 1.0f, minFNumber, maxFNumber);
	settings.m_FocalLengthMultiplier	= RampValueSafe(dofIntensityLevel, 0.0f, 1.0f, minFocalLengthMultiplier, maxFocalLengthMultiplier);
#endif // GTA_REPLAY
}

void camReplayBaseCamera::PostUpdate()
{
	camBaseCamera::PostUpdate();

#if GTA_REPLAY
	sReplayMarkerInfo* markerInfo			= CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);
	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	const bool isShiftingThisMarker			= CReplayMarkerContext::GetCurrentMarkerIndex() == m_MarkerIndex && CReplayMarkerContext::IsShiftingCurrentMarker();
	const bool isEditableCamera				= markerInfo && markerInfo->IsEditableCamera();

	if (isEditableCamera && (isCurrentlyEditingThisMarker || isShiftingThisMarker))
	{
		SaveMarkerCameraInfo(*markerInfo, m_Frame);
	}

	++m_NumReplayUpdatesPerformed;

	m_ForcingDefaultFrame = false;
#endif // GTA_REPLAY
}

#if GTA_REPLAY
bool camReplayBaseCamera::Init(const s32 markerIndex, const bool shouldForceDefaultFrame)
{
	m_MarkerIndex			= markerIndex;
	m_ForcingDefaultFrame	= shouldForceDefaultFrame;

	IReplayMarkerStorage* markerStorage	= CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo		= markerStorage ? markerStorage->TryGetMarker(m_MarkerIndex) : NULL;

	if (!markerInfo)
	{
		return false;
	}

	if (!markerInfo->IsCamMatrixValid())
	{
		//if we're a brand new marker but we're falling back to the recorded cam, then force default framing
		m_ForcingDefaultFrame |= camInterface::GetReplayDirector().IsFallingBackToRecordedCamera();
	}

	if (!(camInterface::GetReplayDirector().IsReplayPlaybackSetupFinished() || CReplayMgr::IsFineScrubbing()))
	{
		return false;
	}

	camFrame previousFrame;
	GetDefaultFrame(previousFrame);

	if (!shouldForceDefaultFrame)
	{
		LoadMarkerCameraInfo(*markerInfo, previousFrame);
	}
	
	if (!markerInfo->IsCamMatrixValid() || shouldForceDefaultFrame)
	{
		//if the marker didn't have any saved camera info (or we're forcing a default), save out the frame
		SaveMarkerCameraInfo(*markerInfo, previousFrame);
	}

	m_Frame.CloneFrom(previousFrame);
	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

	m_NumReplayUpdatesPerformed = 0;

	m_WaitingToInit = false;

	return true;
}

void camReplayBaseCamera::GetDefaultFrame(camFrame& frame) const
{
	const camFrame& renderedFrame = camInterface::GetReplayDirector().GetFrameNoPostEffects();
	frame.CloneFrom(renderedFrame);
}

void camReplayBaseCamera::SaveMarkerCameraInfo(sReplayMarkerInfo& markerInfo, const camFrame& frame) const
{
	Quaternion quat;
	frame.GetWorldMatrix().ToQuaternion(quat);

	markerInfo.SetFreeCamQuaternion( QuantizeQuaternionS3_20(quat) );

	float (&position)[ 3 ] = markerInfo.GetFreeCamPosition();

	position[0]	= frame.GetPosition().x;
	position[1]	= frame.GetPosition().y;
	position[2]	= frame.GetPosition().z;

	markerInfo.SetFOV( static_cast<u8>(rage::round(frame.GetFov())) );
	markerInfo.SetCamMatrixValid( true );
}

void camReplayBaseCamera::LoadMarkerCameraInfo(sReplayMarkerInfo& markerInfo, camFrame& frame) /*non-const because validate entities may need to change targets*/
{
	if (markerInfo.IsCamMatrixValid())
	{
		camFrame newFrame;

		Quaternion orientation;
		DeQuantizeQuaternionS3_20(orientation, markerInfo.GetFreeCamQuaternion() );

		Matrix34 worldMatrix;
		worldMatrix.FromQuaternion(orientation);
		Assert(worldMatrix.IsOrthonormal());
		newFrame.SetWorldMatrix(worldMatrix);

		Vector3 position;
		float const (&positionRaw)[ 3 ] = markerInfo.GetFreeCamPosition();

		position.Set(positionRaw[0], positionRaw[1], positionRaw[2]);
		newFrame.SetPosition(position);

		newFrame.SetFov(static_cast<float>(markerInfo.GetFOV()));

		frame.CloneFrom(newFrame);
	}
}

void camReplayBaseCamera::ValidateEntities(sReplayMarkerInfo& markerInfo)
{
	const bool shouldForceReset = !markerInfo.IsCamMatrixValid() && camInterface::GetReplayDirector().IsFallingBackToRecordedCamera();

	if(shouldForceReset || !CReplayMgr::GetEntity(markerInfo.GetAttachTargetId()))
	{
		camInterface::GetReplayDirector().DisableForceSmoothing();
		markerInfo.SetAttachTargetId( DEFAULT_ATTACH_TARGET_ID );
		markerInfo.SetAttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX );
	}

	if(shouldForceReset || !CReplayMgr::GetEntity(markerInfo.GetLookAtTargetId()))
	{
		markerInfo.SetLookAtTargetId( DEFAULT_LOOKAT_TARGET_ID );
		markerInfo.SetLookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX );
	}

	if(!CReplayMgr::GetEntity(markerInfo.GetFocusTargetId()))
	{
		const s32 focusTargetId = camInterface::GetReplayDirector().GetTargetIdFromIndex(DEFAULT_MARKER_DOF_TARGETINDEX);
		if (CReplayMgr::GetEntity(focusTargetId))
		{
			markerInfo.SetFocusTargetId( focusTargetId );
		}
		else
		{
			markerInfo.SetFocusTargetId( DEFAULT_DOF_FOCUS_ID );
		}

		markerInfo.SetFocusTargetIndex( DEFAULT_MARKER_DOF_TARGETINDEX );
	}
}

bool camReplayBaseCamera::GetClosestParachuteCB(CEntity* pEntity, void* data)
{
    Assert(pEntity);
    Assert(data);

    //cast arguments into more significant types.
    camCollision* pCollision = reinterpret_cast<camCollision*>(data);
    CObject* pObject         = static_cast<CObject*>(pEntity);

    //Check if the closest object is a parachute.
    if(pCollision && pEntity && pEntity->GetIsTypeObject() && pObject->GetIsParachute())
    {
        pCollision->IgnoreEntityThisUpdate(*pEntity);
        return true;
    }          

    return false;
}


void camReplayBaseCamera::AvoidCollisionWithParachutes(const Vector3& testPoint, const camCollision* collision)
{
    Assert(collision);

    //return immediately if the camCollision pointer is null.
    if( ! collision )
    {
        return;
    }

    //The sphere we shall be testing against objects in the world
    Vec3V vPos = VECTOR3_TO_VEC3V(testPoint);

    spdSphere testSphere(vPos, ScalarV(2.0f));
    fwIsSphereIntersecting searchSphere(testSphere);
    CGameWorld::ForAllEntitiesIntersecting(&searchSphere, camReplayBaseCamera::GetClosestParachuteCB, (void*) collision, ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS);
}


#endif // GTA_REPLAY
