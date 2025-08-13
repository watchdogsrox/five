//
// camera/cinematic/CinematicOnFootAssistedAimingContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicOnFootAssistedAimingContext.h"
#include "camera/system/CameraMetadata.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "peds/ped.h"
#include "weapons/info/WeaponInfo.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootAssistedAimingContext,0xEBC19D36)

CAMERA_OPTIMISATIONS()

camCinematicOnFootAssistedAimingContext::camCinematicOnFootAssistedAimingContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicOnFootAssistedAimingContextMetadata&>(metadata))
, m_IsCinematicKillShotRunning(false)
{

}

void camCinematicOnFootAssistedAimingContext::PreUpdate()
{
	const camThirdPersonAimCamera* thirdPersonAimCamera	= camInterface::GetGameplayDirector().GetThirdPersonAimCamera();	

	if (!thirdPersonAimCamera || !thirdPersonAimCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()))
	{
		m_IsCinematicKillShotRunning = false;
		return;
	}

	const camThirdPersonPedAssistedAimCamera* assistedAimCamera = static_cast<const camThirdPersonPedAssistedAimCamera*>(thirdPersonAimCamera);
	m_IsCinematicKillShotRunning = assistedAimCamera->GetIsCinematicKillShotRunning();
}

bool camCinematicOnFootAssistedAimingContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	return m_IsCinematicKillShotRunning;
}