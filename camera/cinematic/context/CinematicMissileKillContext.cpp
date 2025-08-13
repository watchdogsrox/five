//
// camera/cinematic/CinematicMissileKillContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicMissileKillContext.h"
#include "camera/system/CameraMetadata.h"
#include "Vehicles/vehicle.h"
#include "peds/ped.h"
#include "scene/Physical.h"

INSTANTIATE_RTTI_CLASS(camCinematicMissileKillContext,0x5AE4C378)

CAMERA_OPTIMISATIONS()

camCinematicMissileKillContext::camCinematicMissileKillContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicMissileKillContextMetadata&>(metadata))
, m_haveActiveShot(false)
{
}

bool camCinematicMissileKillContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	if( CPauseMenu::GetMenuPreference(PREF_CINEMATIC_SHOOTING) )
	{
		if(m_TargetEntity || m_haveActiveShot)
		{
			return true; 
		}
	}
	return false; 
}

void camCinematicMissileKillContext::PreUpdate()
{
	if(m_TargetEntity == NULL && m_haveActiveShot == false)
	{
		camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState	= gameplayDirector.GetVehicleEntryExitState();
		if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
		{
			const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();

			if(followVehicleCamera)
			{
				//the follow vehicle is also not valid
				const CEntity* followVehicle = followVehicleCamera->GetAttachParent();

				if(!followVehicle || !followVehicle->GetIsTypeVehicle())
				{
					return;
				}
				const CVehicle* pVehicle = static_cast<const CVehicle*>(followVehicle); 

				if(pVehicle->InheritsFromPlane())
				{
					if(pVehicle->GetLockOnTarget())
					{
						CPhysical* pPhys = pVehicle->GetLockOnTarget(); 
						CVehicle* TargetVehicle = NULL; 

						if(pPhys && pPhys->GetIsTypePed())
						{
							CPed* pTargetPed = static_cast<CPed*>(pPhys); 

							TargetVehicle = pTargetPed->GetVehiclePedInside(); 

						}
						else
						{
							if(pPhys && pPhys->GetIsTypeVehicle())
							{
								TargetVehicle = static_cast<CVehicle*>(pPhys); 
							}
						}

						if(TargetVehicle)
						{
							float distanceToTarget = TargetVehicle->GetHomingProjectileDistance(); 
							
							if( distanceToTarget > -1.0f && distanceToTarget < m_Metadata.MissileCollisionRadius)
							{
								m_TargetEntity = TargetVehicle; 
								//m_Shots[0]->SetTargetEntity(); 
								return; 
							}
						}
					}
				}
			}
		}
	}
}

bool camCinematicMissileKillContext::Update()
{
	if(IsValid())
	{
		if(UpdateContext())
		{
			m_haveActiveShot = true; 
			m_TargetEntity = NULL; 
			return true; 
		}
		else
		{
			m_haveActiveShot = false; 
			m_TargetEntity = NULL; 
			return false; 
		}
	}
	return false; 
}