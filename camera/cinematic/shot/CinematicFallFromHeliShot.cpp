//
// camera/cinematic/shot/camCinematicFallFromHeliShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicFallFromHeliShot.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedPartCamera.h"
#include "camera/cinematic/context/CinematicFallFromHeliContext.h"
#include "camera\cinematic\camera\tracking\CinematicVehicleTrackingCamera.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "vehicles/Metadata/VehicleEntryPointInfo.h"
#include "vehicles/trailer.h"
#include "task/Vehicle/TaskExitVehicle.h"


INSTANTIATE_RTTI_CLASS(camCinematicFallFromHeliShot,0x37B90D2F)

CAMERA_OPTIMISATIONS()

camCinematicFallFromHeliShot::camCinematicFallFromHeliShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicFallFromHeliShotMetadata&>(metadata))
{
}

bool camCinematicFallFromHeliShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}
	
	return true; 
}

void camCinematicFallFromHeliShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			camCinematicMountedCamera* pCam = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get()); 
			pCam->ShouldDisableControlHelper(true); 
			pCam->LookAt(m_LookAtEntity); 
			pCam->SetLookAtBehaviour(LOOK_AT_FOLLOW_PED_RELATIVE_POSITION);
			pCam->AttachTo(m_AttachEntity);
		}
	}
}


const CEntity* camCinematicFallFromHeliShot::ComputeAttachEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicFallFromHeliContext::GetStaticClassId()))
	{
		camCinematicFallFromHeliContext* pContext = static_cast<camCinematicFallFromHeliContext*>(m_pContext); 
		return pContext->GetTargetEntity(); 
	}
	return NULL; 
}

bool camCinematicFallFromHeliShot::CanCreate()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		return true; 
	}
	return false;
}

