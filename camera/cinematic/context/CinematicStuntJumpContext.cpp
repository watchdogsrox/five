//
// camera/cinematic/CinematicStuntJumpContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicStuntJumpContext.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "control/stuntjump.h"

INSTANTIATE_RTTI_CLASS(camCinematicStuntJumpContext,0xA11080AE)

CAMERA_OPTIMISATIONS()

camCinematicStuntJumpContext::camCinematicStuntJumpContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicStuntJumpContextMetadata&>(metadata))
, m_IsValid(false)
, m_WasAimingDuringJump(false)
{

}

void camCinematicStuntJumpContext::PreUpdate()
{
	m_IsValid = false;

	//NOTE: We keep track of whether aiming occurred at any point during an individual stunt jump,
	//as we don't want rendering of the cinematic camera to resume after aiming ceases.
	const s32 vehicleEntryExitState			= camInterface::GetGameplayDirector().GetVehicleEntryExitState();

	if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
	{
		const bool isAStuntjumpInProgress = CStuntJumpManager::IsAStuntjumpInProgress();
		if(isAStuntjumpInProgress)
		{
			const bool isStuntCamOptional	  = SStuntJumpManager::GetInstance().IsStuntCamOptional();
			const bool isShowingOptionalCams  = SStuntJumpManager::GetInstance().ShowOptionalStuntCameras();
			if(!isStuntCamOptional || isShowingOptionalCams)
			{
				const bool isAiming = camInterface::GetGameplayDirector().IsAiming();
				if(isAiming)
				{
					m_WasAimingDuringJump = true;
				}
				m_IsValid = !m_WasAimingDuringJump;
			}
			else
			{
				m_WasAimingDuringJump = false;
			}
		}
		else
		{
			m_WasAimingDuringJump = false;
		}
	}
}
