//
// camera/gameplay/aim/FirstPersonHeadTrackingAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/FirstPersonHeadTrackingAimCamera.h"

#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/system/CameraMetadata.h"
#include "scene/portals/Portal.h"
#include "stats/StatsInterface.h"
#include "peds/ped.h"
#include "input/headtracking.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFirstPersonHeadTrackingAimCamera,0x539D384C)

camFirstPersonHeadTrackingAimCamera::camFirstPersonHeadTrackingAimCamera(const camBaseObjectMetadata& metadata)
: camAimCamera(metadata)
, m_Metadata(static_cast<const camFirstPersonHeadTrackingAimCameraMetadata&>(metadata))
{
}

bool camFirstPersonHeadTrackingAimCamera::ShouldDisplayReticule() const
{
	return m_Metadata.m_ShouldDisplayReticule;
}

bool camFirstPersonHeadTrackingAimCamera::Update()
{
	if(!m_AttachParent.Get() || !m_ControlHelper)
	{
		return false;
	}

	// Now done in camInterface::ComputeShouldMakePedHeadInvisible()
	//CPed* attachPed = (CPed*)m_AttachParent.Get();
	//attachPed->SetPedResetFlag(CPED_RESET_FLAG_MakeHeadInvisible, true);

	UpdatePosition();
	UpdateOrientation();
	return true;
}

void camFirstPersonHeadTrackingAimCamera::UpdatePosition()
{
	// Head bone position
	CPed* attachPed = (CPed*)m_AttachParent.Get();

	if (attachPed->GetIsInVehicle() || attachPed->GetIsParachuting())
	{
		Vector3 vPos;
		int boneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
		Matrix34 matHeadPosition;
		m_AttachParent->GetGlobalMtx(boneIndex, matHeadPosition);
		vPos = matHeadPosition.GetVector(3);
		// Apply a head forward/up offset to position
		vPos = m_Metadata.m_AttachRelativeOffset;
		matHeadPosition.Transform(vPos);
		m_Frame.SetPosition(vPos);
	}
	else
	{
		Mat34V mtxReferenceDir;
		s32 iFacingBoneIdx = attachPed->GetBoneIndexFromBoneTag(BONETAG_FACING_DIR);

		mtxReferenceDir = attachPed->GetTransform().GetMatrix();
		Transform(mtxReferenceDir, mtxReferenceDir, attachPed->GetSkeleton()->GetLocalMtx(iFacingBoneIdx));

		//Vector3 vRootPosition, vHeadPosition;
		//attachPed->GetBonePosition(vRootPosition, BONETAG_ROOT);
		//attachPed->GetBonePosition(vHeadPosition, BONETAG_HEAD);
		//Vec3V vPos = Vec3V(0.0f, 0.0f, vHeadPosition.GetZ() - vRootPosition.GetZ()) + Vec3V(m_Metadata.m_AttachRelativeOffset);

		Vec3V vPos = Vec3V(0.0f, 0.0f, rage::ioHeadTracking::GetPlayerHeight() * 0.42f) + Vec3V(m_Metadata.m_AttachRelativeOffset);
		vPos = Transform(mtxReferenceDir, vPos);
		m_Frame.SetPosition(VEC3V_TO_VECTOR3(vPos));
	}
}

void camFirstPersonHeadTrackingAimCamera::UpdateOrientation()
{
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	Mat34V mtxReferenceDir;
	s32 iFacingBoneIdx = attachPed->GetBoneIndexFromBoneTag(BONETAG_FACING_DIR);

	mtxReferenceDir = attachPed->GetTransform().GetMatrix();
	Transform(mtxReferenceDir, mtxReferenceDir, attachPed->GetSkeleton()->GetLocalMtx(iFacingBoneIdx));

	m_Frame.SetWorldMatrix(MAT34V_TO_MATRIX34(mtxReferenceDir), true);
}

void camFirstPersonHeadTrackingAimCamera::UpdateShake()
{
}
