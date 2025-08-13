//
// camera/cinematic/shot/camInVehicleShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicInVehicleGroupShot.h"
#include "camera/cinematic/camera/tracking/CinematicGroupCamera.h"
#include "fwnet/netinterface.h"
#include "peds/ped.h"

INSTANTIATE_RTTI_CLASS(camCinematicVehicleGroupShot,0xE1344CF8)

CAMERA_OPTIMISATIONS()

camCinematicVehicleGroupShot::camCinematicVehicleGroupShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicVehicleGroupShotMetadata&>(metadata))
{
}

void camCinematicVehicleGroupShot::InitCamera()
{
	if(m_AttachEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicGroupCamera::GetStaticClassId()))
		{	
			GetPlayerPositions(); 
			
			camCinematicGroupCamera* pCam = static_cast<camCinematicGroupCamera*>(m_UpdatedCamera.Get()); 

			pCam->Init(m_TargetEntities); 
			pCam->LookAt(m_LookAtEntity); 
		}
	}
}

bool camCinematicVehicleGroupShot::CanCreate()
{
	GetPlayerPositions(); 

	return HaveEnoughEntitiesInGroup();
}

void camCinematicVehicleGroupShot::PreUpdate(bool IsCurrentShot)
{
	if(IsCurrentShot)	
	{
		GetPlayerPositions(); 
	}
}

bool camCinematicVehicleGroupShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}
	
	return HaveEnoughEntitiesInGroup();
}

void camCinematicVehicleGroupShot::Update()
{
	if(m_UpdatedCamera)
	{
		camCinematicGroupCamera* pCam = static_cast<camCinematicGroupCamera*>(m_UpdatedCamera.Get()); 
		pCam->SetEntityList(m_TargetEntities); 
	}
}

void camCinematicVehicleGroupShot::GetPlayerPositions()
{
	m_TargetEntities.Reset(); 

	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();
	const CPed* pLocalPlayer = camInterface::FindFollowPed(); 
	
	if(pLocalPlayer)
	{
		// Vector3 vLocalPlayerPositon = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()); 
		// Vector3 vLocalFront = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetForward());

		RegdConstEnt& PlayerEntity = m_TargetEntities.Grow(); 
		PlayerEntity = pLocalPlayer; 
		
		for(unsigned index = 0; index < numPhysicalPlayers; index++)
		{
			const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			if(player && player->GetPlayerPed() && player->GetPlayerPed() != pLocalPlayer)
			{
				if(IsPedValidForGroup(player->GetPlayerPed()))
				{
					RegdConstEnt& Entity = m_TargetEntities.Grow(); 
					Entity = player->GetPlayerPed(); 
				}
			}
		}

		//const CEntityScannerIterator entityList = pLocalPlayer->GetPedIntelligence()->GetNearbyPeds();
		//for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
		//{
		//	const CPed* pThisPed = static_cast<const CPed*>(pEnt);
		//	
		//	if(IsPedValidForGroup(pThisPed))
		//	{
		//		RegdConstEnt& Entity = m_TargetEntities.Grow(); 
		//		Entity = pEnt; 
		//	}
		//}
	}
}

//tidy up
bool camCinematicVehicleGroupShot::IsPedValidForGroup(const CPed* pPed) const
{
	const CPed* pLocalPlayer = camInterface::FindFollowPed(); 

	if(pLocalPlayer)
	{	
		Vector3 vLocalPlayerPositon = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()); 
		Vector3 vLocalFront = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetForward());

		Vector3 vNonLocalPlayerPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); 
		Vector3 vNonLocalFront = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()); 

		CVehicle* pVehicle = pPed->GetVehiclePedInside(); 

		if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE && pPed->GetIsDrivingVehicle())
		{
			float distToPlayer = vLocalPlayerPositon.Dist2(vNonLocalPlayerPosition); 
			if( distToPlayer < m_Metadata.m_GroupSearchRadius* m_Metadata.m_GroupSearchRadius)
			{
				if(vLocalFront.Dot(vNonLocalFront) > m_Metadata.m_DotOfEntityFrontAndPlayer) //add to meta data
				{
					return true; 
				}
			}
		}
	}
	return false;  
}

bool camCinematicVehicleGroupShot::HaveEnoughEntitiesInGroup() const
{
	if(m_TargetEntities.GetCount() >= m_Metadata.m_MinNumberForValidGroup )
	{
		return true; 
	}

	return false; 
}
