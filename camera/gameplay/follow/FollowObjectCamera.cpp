//
// camera/gameplay/follow/FollowObjectCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/follow/FollowObjectCamera.h"

#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFollowObjectCamera,0x3E9DDE23)


camFollowObjectCamera::camFollowObjectCamera(const camBaseObjectMetadata& metadata)
: camFollowCamera(metadata)
, m_Metadata(static_cast<const camFollowObjectCameraMetadata&>(metadata))
{
}

bool camFollowObjectCamera::Update()
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypeObject()) //Safety check.
	{
		//Default to attaching to any object that the follow ped is using/attached to.
		const CPhysical* followPed = static_cast<const CPhysical*>(camInterface::FindFollowPed());
		if(followPed)
		{
			CPhysical* followPedAttachment = (CPhysical*)followPed->GetAttachParent();
			if(followPedAttachment && followPedAttachment->GetIsTypeObject())
			{
				AttachTo(followPedAttachment);
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	bool hasSucceeded = camFollowCamera::Update();
	return hasSucceeded;
}
