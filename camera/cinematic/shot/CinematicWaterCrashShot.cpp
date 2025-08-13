//
// camera/cinematic/shot/camCinematicWaterCrashShot.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicWaterCrashContext.h"
#include "camera/cinematic/shot/CinematicWaterCrashShot.h"
//#include "camera/cinematic/camera/tracking/CinematicPositionCamera.h"
#include "camera/cinematic/camera/tracking/CinematicWaterCrashCamera.h"
#include "fwsys/timer.h"

INSTANTIATE_RTTI_CLASS(camCinematicWaterCrashShot,0xD2066CFF)

CAMERA_OPTIMISATIONS()

camCinematicWaterCrashShot::camCinematicWaterCrashShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicWaterCrashShotMetadata&>(metadata))
{
}
bool camCinematicWaterCrashShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}
	
	if (m_AttachEntity && m_LookAtEntity)
	{
		return camBaseCinematicShot::IsValid();
	}

	return false; 
}

void camCinematicWaterCrashShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)	
	{
		//if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
		if(m_UpdatedCamera->GetIsClassId(camCinematicWaterCrashCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			//((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetShouldStartZoomed(false); 
			//((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetShouldZoom(false); 
			//((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetMode(m_CurrentMode);
			//((camCinematicPositionCamera*)m_UpdatedCamera.Get())->Init();
			((camCinematicWaterCrashCamera*)m_UpdatedCamera.Get())->Init();
			((camCinematicWaterCrashCamera*)m_UpdatedCamera.Get())->SetContext(m_pContext);
		}
	}
}

const CEntity* camCinematicWaterCrashShot::ComputeAttachEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicWaterCrashContext::GetStaticClassId()))
	{
		camCinematicWaterCrashContext* pContext = static_cast<camCinematicWaterCrashContext*>(m_pContext); 
		return pContext->GetTargetEntity(); 
	}
	return NULL; 
}

const CEntity* camCinematicWaterCrashShot::ComputeLookAtEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicWaterCrashContext::GetStaticClassId()))
	{
		camCinematicWaterCrashContext* pContext = static_cast<camCinematicWaterCrashContext*>(m_pContext); 
		return pContext->GetTargetEntity(); 
	}
	return NULL;
}
