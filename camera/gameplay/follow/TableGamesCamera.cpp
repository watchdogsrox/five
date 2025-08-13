//
// camera/gameplay/follow/TableGamesCamera.h
// 
// Copyright (C) 1999-2019 Rockstar Games.  All Rights Reserved.
//
#include "camera/gameplay/follow/TableGamesCamera.h"
#include "camera/system/CameraMetadata.h"
#include "fwmaths/angle.h"

CAMERA_OPTIMISATIONS();

INSTANTIATE_RTTI_CLASS(camTableGamesCamera, 0x5B78E32D);

camTableGamesCamera::camTableGamesCamera(const camBaseObjectMetadata& metadata)
	: camThirdPersonCamera(static_cast<const camTableGamesCameraMetadata&>(metadata).m_ThirdPersonCameraSettings)
	, m_Metadata(static_cast<const camTableGamesCameraMetadata&>(metadata))
{

}

camTableGamesCamera::~camTableGamesCamera()
{

}

bool camTableGamesCamera::Update()
{
	m_RelativeOrbitHeadingLimits = m_Metadata.m_TableGamesSettings.m_HeadingLimits * DtoR;

	return camThirdPersonCamera::Update();
}

void camTableGamesCamera::UpdateBasePivotPosition()
{
	camThirdPersonCamera::UpdateBasePivotPosition();

	Vector3 offset;
	m_AttachParentMatrix.Transform3x3(m_Metadata.m_TableGamesSettings.m_RelativePivotOffset, offset);

	m_BasePivotPosition += offset;
}

float camTableGamesCamera::UpdateOrbitDistance()
{
	float orbitDistanceToApply = camThirdPersonCamera::UpdateOrbitDistance();

	orbitDistanceToApply *= m_Metadata.m_TableGamesSettings.m_OrbitDistanceScalar;

	return orbitDistanceToApply;
}

void camTableGamesCamera::UpdateOrbitPitchLimits()
{
	camThirdPersonCamera::UpdateOrbitPitchLimits();

	m_OrbitPitchLimits = m_Metadata.m_TableGamesSettings.m_PitchLimits * DtoR;
}

void camTableGamesCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	camThirdPersonCamera::ComputeOrbitOrientation(orbitHeading, orbitPitch);

	if (m_NumUpdatesPerformed == 0 && m_Metadata.m_TableGamesSettings.m_ShouldApplyInitialRotation)
	{
		//initialize heading and pitch to a certain angle relative to the attach parent matrix.
		float attachParentHeading, attachParentPitch;
		camFrame::ComputeHeadingAndPitchFromMatrix(m_AttachParentMatrix, attachParentHeading, attachParentPitch);

		if (ShouldUseSyncSceneAttachParentMatrix())
		{
			Matrix34 sceneMatrix;
			ComputeAttachParentMatrixFromSnycScene(sceneMatrix);

			if (!sceneMatrix.IsClose(M34_IDENTITY))
			{
				camFrame::ComputeHeadingAndPitchFromMatrix(sceneMatrix, attachParentHeading, attachParentPitch);
			}
		}

		orbitHeading	= fwAngle::LimitRadianAngle(m_Metadata.m_TableGamesSettings.m_InitialRelativeHeading*DtoR + attachParentHeading);
		orbitPitch		= fwAngle::LimitRadianAngleForPitch(m_Metadata.m_TableGamesSettings.m_InitialRelativePitch*DtoR + attachParentPitch);

		//Clear any existing orientation override requests.
		m_ShouldApplyRelativeHeading = false;
		m_ShouldApplyRelativePitch = false;
	}
}

bool camTableGamesCamera::ShouldUseSyncSceneAttachParentMatrix() const
{
	static const u32 slotMachineCameraName	= ATSTRINGHASH("CASINO_SLOT_MACHINE_CAMERA", 0x1EE8CB4C);
	const bool isSlotMachineCamera			= (m_Metadata.m_Name.GetHash() == slotMachineCameraName);
	return isSlotMachineCamera;
}

void camTableGamesCamera::ComputeAttachParentMatrixFromSnycScene(Matrix34& sceneMatrix) const
{
	sceneMatrix.Identity();

	if (!m_AttachParent || !m_AttachParent->GetIsTypePed())
	{
		return;
	}

	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	const CTaskSynchronizedScene* syncSceneTask = attachPed->GetPedIntelligence() ? static_cast<const CTaskSynchronizedScene*>(attachPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE)) : NULL;
	if (!syncSceneTask)
	{
		return;
	}

	const int dictionaryIndex = fwAnimManager::FindSlotFromHashKey(syncSceneTask->GetDictionary().GetHash()).Get();
	const atHashString clip(atFinalizeHash(syncSceneTask->GetClipPartialHash()));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictionaryIndex, clip.GetHash());

	fwAnimDirectorComponentSyncedScene::CalcEntityMatrix(syncSceneTask->GetScene(), pClip, syncSceneTask->IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE), sceneMatrix);
}