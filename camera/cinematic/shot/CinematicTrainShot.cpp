//
// camera/cinematic/shot/camInVehicleShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicTrainShot.h"
#include "camera/system/CameraMetadata.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"
#include "camera/cinematic/context/CinematicInVehicleContext.h"

#include "vehicles/train.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicTrainStationShot,0xAD1A194F)

camCinematicTrainStationShot::camCinematicTrainStationShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicTrainStationShotMetadata&>(metadata))
{

}

void camCinematicTrainStationShot::InitCamera()
{
	if(m_AttachEntity)
	{
		CTrain* caboose	= CTrain::FindCaboose(static_cast<const CTrain*>(m_AttachEntity.Get()));
		if(caboose)
		{
			m_UpdatedCamera->AttachTo(caboose);
		}
	}
}

bool camCinematicTrainStationShot::CanCreate()
{
	const camBaseCinematicContext* pContext = camInterface::GetCinematicDirector().GetCurrentContext(); 

	if(pContext)
	{
		const camBaseCinematicShot* pShot = pContext->GetCurrentShot(); 

		if(pShot)
		{
			if(pShot->GetIsClassId(camCinematicTrainRoofShot::GetStaticClassId()))
			{
				return false; 
			}
		}
	}
	return true; 
}

const CEntity* camCinematicTrainStationShot::ComputeAttachEntity() const
{
	if(m_pContext && ((m_pContext->GetClassId() == camCinematicInTrainContext::GetStaticClassId()) || 
		(m_pContext->GetClassId() == camCinematicInTrainAtStationContext::GetStaticClassId())))
	{
		camCinematicInTrainContext* pTrainContext = static_cast<camCinematicInTrainContext*>(m_pContext); 
		return 	pTrainContext->GetTrainPlayerIsIn();
	}
	
	return NULL; 
}

INSTANTIATE_RTTI_CLASS(camCinematicTrainRoofShot,0x98F67DDE)

camCinematicTrainRoofShot::camCinematicTrainRoofShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicTrainRoofMountedShotMetadata&>(metadata))
{

}

bool camCinematicTrainRoofShot::CanCreate()
{
	const camBaseCinematicContext* pContext = camInterface::GetCinematicDirector().GetCurrentContext(); 

	if(pContext)
	{
		const camBaseCinematicShot* pShot = pContext->GetCurrentShot(); 

		if(pShot)
		{
			if(pShot->GetIsClassId(camCinematicTrainStationShot::GetStaticClassId()))
			{
				return false; 
			}
		}
	}
	return true; 
}


const CEntity* camCinematicTrainRoofShot::ComputeAttachEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicInTrainContext::GetStaticClassId()))
	{
		camCinematicInTrainContext* pTrainContext = static_cast<camCinematicInTrainContext*>(m_pContext); 
		return 	pTrainContext->GetTrainPlayerIsIn();
	}

	return NULL; 
}


void camCinematicTrainRoofShot::InitCamera()
{
	if(m_AttachEntity)	
	{
		CTrain* engine	= CTrain::FindEngine(static_cast<const CTrain*>(m_AttachEntity.Get()));
		{
			m_UpdatedCamera->AttachTo(engine);
		}
	}
}



INSTANTIATE_RTTI_CLASS(camCinematicPassengerShot,0x436DFA18)

camCinematicPassengerShot::camCinematicPassengerShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicTrainPassengerShotMetadata&>(metadata))
{

}

void camCinematicPassengerShot::InitCamera()
{
	if(m_AttachEntity)
	{
		CTrain* engine	= CTrain::FindEngine(static_cast<const CTrain*>(m_AttachEntity.Get()));
		{
			m_UpdatedCamera->AttachTo(engine);
		}
	}
}

const CEntity* camCinematicPassengerShot::ComputeAttachEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicInTrainContext::GetStaticClassId()))
	{
		camCinematicInTrainContext* pTrainContext = static_cast<camCinematicInTrainContext*>(m_pContext); 
		return 	pTrainContext->GetTrainPlayerIsIn();
	}

	return NULL; 
}


INSTANTIATE_RTTI_CLASS(camCinematicTrainTrackShot,0x5A529149)

camCinematicTrainTrackShot::camCinematicTrainTrackShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicTrainTrackShotMetadata&>(metadata))
{

}

void camCinematicTrainTrackShot::InitCamera()
{
	if(m_LookAtEntity)
	{
		m_UpdatedCamera->LookAt(m_LookAtEntity);

		((camCinematicTrainTrackCamera*)m_UpdatedCamera.Get())->SetMode(m_CurrentMode);
	}
}

const CEntity* camCinematicTrainTrackShot::ComputeLookAtEntity() const
{
	if(m_pContext && (m_pContext->GetClassId() == camCinematicInTrainContext::GetStaticClassId()))
	{
		camCinematicInTrainContext* pTrainContext = static_cast<camCinematicInTrainContext*>(m_pContext); 
		return 	pTrainContext->GetTrainPlayerIsIn();
	}

	return NULL; 
}

