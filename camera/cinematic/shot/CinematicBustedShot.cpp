//
// camera/cinematic/shot/CinematicBustedShot.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicBustedShot.h"

#include "camera/cinematic/context/CinematicBustedContext.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"

INSTANTIATE_RTTI_CLASS(camCinematicBustedShot,0x3B05DD63)

CAMERA_OPTIMISATIONS()

camCinematicBustedShot::camCinematicBustedShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicBustedShotMetadata&>(metadata))
{
}

void camCinematicBustedShot::InitCamera()
{
	if(!m_UpdatedCamera)
	{
		return;
	}

	if(m_AttachEntity && m_LookAtEntity)
	{
		m_UpdatedCamera->AttachTo(m_AttachEntity);
		m_UpdatedCamera->LookAt(m_LookAtEntity);
	}
	
}

bool camCinematicBustedShot::CanCreate()
{
	if(!camBaseCinematicShot::CanCreate())
	{
		return false;
	}

	const CPed* arrestingPed = ComputeArrestingPed();
	if(!arrestingPed || !m_LookAtEntity)
	{
		return false;
	}

	//Wait for the arresting ped to stop moving before creating the shot.
	const Vector3 arrestingPedVelocity	= arrestingPed->GetVelocity();
	const float arrestingPedSpeed2		= arrestingPedVelocity.Mag2();
	const bool canCreate				= (arrestingPedSpeed2 <= (m_Metadata.m_MaxArrestingPedSpeedToActivate *
											m_Metadata.m_MaxArrestingPedSpeedToActivate));

	return canCreate;
}

const CEntity* camCinematicBustedShot::ComputeAttachEntity() const 
{
	return ComputeArrestingPed(); 
}

const CPed* camCinematicBustedShot::ComputeArrestingPed() const
{
	const CPed* arrestingPed = NULL;

	if(m_pContext && (m_pContext->GetClassId() == camCinematicBustedContext::GetStaticClassId()))
	{
		const camCinematicBustedContext* bustedContext	= static_cast<const camCinematicBustedContext*>(m_pContext); 
		arrestingPed									= bustedContext->GetArrestingPed();
	}

	return arrestingPed;
}
