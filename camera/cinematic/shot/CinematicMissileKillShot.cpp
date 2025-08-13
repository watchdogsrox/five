//
// camera/cinematic/shot/camCinematicMissileKillShot.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicMissileKillContext.h"
#include "camera/cinematic/shot/CinematicMissileKillShot.h"
#include "camera/cinematic/camera/tracking/CinematicPositionCamera.h"

INSTANTIATE_RTTI_CLASS(camCinematicMissileKillShot,0x3CCF1EA4)

CAMERA_OPTIMISATIONS()

camCinematicMissileKillShot::camCinematicMissileKillShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicMissileKillShotMetadata&>(metadata))
{
}
bool camCinematicMissileKillShot::IsValid() const
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		return true; 
	}

	return false; 
}

void camCinematicMissileKillShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetShouldStartZoomed(false); 
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetShouldZoom(false); 
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetMode(m_CurrentMode);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->ComputeCamPositionRelativeToBaseOrbitPosition(); 
			
		}
	}
}

const CEntity* camCinematicMissileKillShot::ComputeAttachEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicMissileKillContext::GetStaticClassId()))
	{
		camCinematicMissileKillContext* pContext = static_cast<camCinematicMissileKillContext*>(m_pContext); 
		return pContext->GetTargetEntity(); 
	}
	return NULL; 
}

const CEntity* camCinematicMissileKillShot::ComputeLookAtEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicMissileKillContext::GetStaticClassId()))
	{
		camCinematicMissileKillContext* pContext = static_cast<camCinematicMissileKillContext*>(m_pContext); 
		return pContext->GetTargetEntity(); 
	}
	return NULL; 
}