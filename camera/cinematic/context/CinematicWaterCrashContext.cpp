//
// camera/cinematic/CinematicWaterCrashContext.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicWaterCrashContext.h"
#include "camera/system/CameraMetadata.h"
#include "control/gamelogic.h"
#include "Vehicles/vehicle.h"
#include "peds/ped.h"
#include "scene/Physical.h"
#include "fwsys/timer.h"

INSTANTIATE_RTTI_CLASS(camCinematicWaterCrashContext,0x82742D3D)

CAMERA_OPTIMISATIONS()

camCinematicWaterCrashContext::camCinematicWaterCrashContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicWaterCrashContextMetadata&>(metadata))
, m_InvalidEntity(NULL)
, m_StartTime(0)
{
}

bool camCinematicWaterCrashContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	// TODO: restrict to menu option?
	////if( CPauseMenu::GetMenuPreference(PREF_CINEMATIC_SHOOTING) )
	{
		if(CGameLogic::IsTeleportActive())
		{
			return false;
		}

		if(m_TargetEntity)// && m_StartTime + m_Metadata.m_ContextDuration > fwTimer::GetCamTimeInMilliseconds())
		{
			if( m_TargetEntity->GetIsTypeVehicle() &&
				static_cast<const CVehicle*>(m_TargetEntity.Get())->GetDriver() != NULL )
			{
				// If player is teleported during camera, the vehicle we are tracking won't have a driver.
				return true;
			}
		}
	}
	return false; 
}

void camCinematicWaterCrashContext::PreUpdate()
{
	if(m_TargetEntity == NULL)
	{
		camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState	= gameplayDirector.GetVehicleEntryExitState();
		if (vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
		{
			const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();
			if (followVehicleCamera)
			{
				const CEntity* followVehicle = followVehicleCamera->GetAttachParent();
				if (!followVehicle || !followVehicle->GetIsTypeVehicle())
				{
					return;
				}

				const CVehicle* pVehicle = static_cast<const CVehicle*>(followVehicle); 
				{
					if (pVehicle->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && pVehicle->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame())
					{
						bool isWaterCrash = false;
						if ( m_InvalidEntity.Get() != NULL )
						{
							int buoyancyStatus = pVehicle->m_Buoyancy.GetStatus();
							if(buoyancyStatus != NOT_IN_WATER)
							{
								if (pVehicle->InheritsFromPlane() &&
									pVehicle->m_nFlags.bPossiblyTouchesWater)
								{
									// Pitch limit is specified as a positive angle from straight down, so have to
									// convert relative to vehicle's pitch. (straight down for vehicle is -90.0f degrees)
									const float fMinPitchLimit = (m_Metadata.m_PitchLimit - 90.0f)*DtoR;
									const float fMaxPitchLimit = (-180.0f - fMinPitchLimit)*DtoR;
									float fPitch = pVehicle->GetTransform().GetPitch();
									if (fPitch <= fMinPitchLimit && fPitch >= fMaxPitchLimit)
									{
										isWaterCrash = true;
									}
								}
								else
								{
									//NOTE: The average submerged level is only updated if the buoyancy helper detects water.
									const float submergedLevel		= pVehicle->m_Buoyancy.GetSubmergedLevel();
									const float submergedLevelLimit	= pVehicle->InheritsFromHeli() ? m_Metadata.m_fHeliSubmergedLimit :
																		(pVehicle->InheritsFromPlane() ? m_Metadata.m_fPlaneSubmergedLimit :
																		m_Metadata.m_fDefaultSubmergedLimit);
									if(submergedLevel >= submergedLevelLimit)
									{
										isWaterCrash = true;
									}
								}
							}
						}

						if (isWaterCrash)
						{
							if (m_InvalidEntity.Get() == pVehicle)
							{
								// Valid vehicle that just entered the water.
								m_StartTime = fwTimer::GetCamTimeInMilliseconds();
								m_TargetEntity = pVehicle;
								m_InvalidEntity = NULL;
							}
						}
						else if (!pVehicle->m_nFlags.bPossiblyTouchesWater)
						{
							// for some reason, IsInAir is not const
							if (const_cast<CVehicle*>(pVehicle)->IsInAir())
							{
								// Cannot drive into water and trigger water crash camera, must be in Air before entering water.
								m_InvalidEntity = pVehicle;
							}
							else
							{
								m_InvalidEntity = NULL;
							}
						}
					}
				}
			}
			else
			{
				m_InvalidEntity = NULL;
			}
		}
		else
		{
			m_InvalidEntity = NULL;
		}
	}
}

bool camCinematicWaterCrashContext::Update()
{
	if(IsValid())
	{
		if(UpdateContext())
		{
			return true;
		}
	}

	m_TargetEntity = NULL;
	return false;
}