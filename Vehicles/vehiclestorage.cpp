/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    vehiclestorage.cpp
// PURPOSE : Stuff having to do with taking vehicles off the map and storing them in a small structure.
//			 Later on the vehicle can be recreated from this structure.
// AUTHOR :  Obbe.
// CREATED : 30-9-05
//
/////////////////////////////////////////////////////////////////////////////////

#include "vehicles\vehiclestorage.h"
#include "vehicles\vehicle.h"
#include "scene\world\gameWorld.h"
#include "modelinfo\vehiclemodelinfo.h"
#include "network\live\NetworkTelemetry.h"
#include "stats\MoneyInterface.h"
#include "streaming\streaming.h"
#include "vehicles/Automobile.h"
#include "vehicles\vehiclefactory.h"
#include "VehicleAi\task\TaskVehiclePlayer.h"
#include "fwscene\world\WorldLimits.h"

#define FRONTCOMPR (100.0f)


CStoredVehicleVariations::CStoredVehicleVariations() : 
m_color1(0),
m_color2(0),
m_color3(0),
m_color4(0),
m_smokeColR(0),
m_smokeColG(0),
m_smokeColB(0),
m_neonColR(0),
m_neonColG(0),
m_neonColB(0),
m_neonFlags(0),
m_windowTint(0),
m_wheelType(VWT_SPORT),
m_kitIdx(INVALID_VEHICLE_KIT_INDEX)
{
	for (s32 i = 0; i < VMT_MAX; ++i)
	{
		m_mods[i] = INVALID_MOD;
	}

    m_modVariation[0] = false;
    m_modVariation[1] = false;
}


void CStoredVehicleVariations::StoreVariations(const CVehicle* pVeh)
{
	if (!pVeh)
		return;

	const CVehicleVariationInstance& VehicleVariationInstance = pVeh->GetVariationInstance();

	m_kitIdx = VehicleVariationInstance.GetKitIndex();

	for (u32 mod_loop = 0; mod_loop < VMT_MAX; mod_loop++)
	{
		m_mods[mod_loop] = VehicleVariationInstance.GetModIndex(static_cast<eVehicleModType>(mod_loop));
	}

	m_color1 = VehicleVariationInstance.GetColor1();
	m_color2 = VehicleVariationInstance.GetColor2();
	m_color3 = VehicleVariationInstance.GetColor3();
	m_color4 = VehicleVariationInstance.GetColor4();
	m_color5 = VehicleVariationInstance.GetColor5();
	m_color6 = VehicleVariationInstance.GetColor6();

	m_smokeColR = VehicleVariationInstance.GetSmokeColorR();
	m_smokeColG = VehicleVariationInstance.GetSmokeColorG();
	m_smokeColB = VehicleVariationInstance.GetSmokeColorB();

	m_neonColR = VehicleVariationInstance.GetNeonColour().GetRed();
	m_neonColG = VehicleVariationInstance.GetNeonColour().GetGreen();
	m_neonColB = VehicleVariationInstance.GetNeonColour().GetBlue();
	m_neonFlags = 0;
	if(VehicleVariationInstance.IsNeonLOn())
		m_neonFlags |= 1;
	if(VehicleVariationInstance.IsNeonROn())
		m_neonFlags |= 2;
	if(VehicleVariationInstance.IsNeonFOn())
		m_neonFlags |= 4;
	if(VehicleVariationInstance.IsNeonBOn())
		m_neonFlags |= 8;

	m_windowTint = VehicleVariationInstance.GetWindowTint();

	m_wheelType = (u8)VehicleVariationInstance.GetWheelType(pVeh);

    m_modVariation[0] = VehicleVariationInstance.HasVariation(0);
    m_modVariation[1] = VehicleVariationInstance.HasVariation(1);
}

bool CStoredVehicleVariations::IsToggleModOn(eVehicleModType modSlot) const
{
	if (Verifyf(modSlot >= VMT_TOGGLE_MODS && modSlot < VMT_MISC_MODS, "CStoredVehicleVariations::IsToggleModOn - Mod type %d is not a toggle mod", modSlot))
	{
		return m_mods[modSlot] != INVALID_MOD;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StoreVehicle
// PURPOSE :  Stores one vehicle into a structure (all the info needed to recreate the vehicle)
/////////////////////////////////////////////////////////////////////////////////

void CStoredVehicle::StoreVehicle(CVehicle *pVeh)
{
	if (Verifyf(pVeh, "CStoredVehicle::StoreVehicle - vehicle pointer is NULL"))
	{
		const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
		// Make sure coordinates are on the map. (We got a bug with nan coordinates being saved)
		Assert(vVehPosition.x > WORLDLIMITS_XMIN);
		Assert(vVehPosition.x < WORLDLIMITS_XMAX);
		Assert(vVehPosition.y > WORLDLIMITS_YMIN);
		Assert(vVehPosition.y < WORLDLIMITS_YMAX);

		m_variation.StoreVariations(pVeh);
//		m_ModelIndex = static_cast<u16>(pVeh->GetModelIndex());
		if (Verifyf(pVeh->GetBaseModelInfo(), "CStoredVehicle::StoreVehicle - failed to get BaseModelInfo for vehicle"))
		{
			SetModelHashKey(pVeh->GetBaseModelInfo()->GetModelNameHash());
		}
		else
		{
			m_ModelHashKey = 0;
		}
		m_CoorX = vVehPosition.x;
		m_CoorY = vVehPosition.y;
		m_CoorZ = vVehPosition.z;
		Vector3 vecForward(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetB()));
		m_iFrontX = (s8)(vecForward.x * FRONTCOMPR);
		m_iFrontY = (s8)(vecForward.y * FRONTCOMPR);
		m_iFrontZ = (s8)(vecForward.z * FRONTCOMPR);
		m_LiveryId = pVeh->GetLiveryId();
		m_Livery2Id= pVeh->GetLivery2Id();
		m_nDisableExtras = pVeh->m_nDisableExtras;

		m_bInInterior = pVeh->m_nFlags.bInMloRoom;
		m_bNotDamagedByBullets = pVeh->m_nPhysicalFlags.bNotDamagedByBullets;
		m_bNotDamagedByFlames = pVeh->m_nPhysicalFlags.bNotDamagedByFlames;
		m_bIgnoresExplosions = pVeh->m_nPhysicalFlags.bIgnoresExplosions;
		m_bNotDamagedByCollisions = pVeh->m_nPhysicalFlags.bNotDamagedByCollisions;
		m_bNotDamagedByMelee = pVeh->m_nPhysicalFlags.bNotDamagedByMelee;

		m_bTyresDontBurst = pVeh->m_nVehicleFlags.bTyresDontBurst;

		if(pVeh->GetVehicleAudioEntity())
		{
			m_HornSoundIndex = pVeh->GetVehicleAudioEntity()->GetHornSoundIdx();
			m_AudioEngineHealth = pVeh->GetVehicleAudioEntity()->GetFakeEngineHealth();
			m_AudioBodyHealth = pVeh->GetVehicleAudioEntity()->GetFakeBodyHealth();
		}
		else
		{
			m_HornSoundIndex = -1; 
			m_AudioEngineHealth = -1;
			m_AudioBodyHealth = -1;
		}

		// License plate
		CCustomShaderEffectVehicle *pShaderEffectVehicle = static_cast<CCustomShaderEffectVehicle*>(pVeh->GetDrawHandler().GetShaderEffect());
		memcpy(m_LicencePlateText, pShaderEffectVehicle->GetLicensePlateText(), CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX);

		Assertf( (pShaderEffectVehicle->GetLicensePlateTexIndex() >= MIN_INT8) && (pShaderEffectVehicle->GetLicensePlateTexIndex() <= MAX_INT8), "CStoredVehicle::StoreVehicle - expected LicensePlateTexIndex to fit within an int8");
		m_plateTexIndex = (s8) pShaderEffectVehicle->GetLicensePlateTexIndex();

		m_bUsed = true;
		m_collisionTestResult.Reset();
	}
}

void CStoredVehicle::SetModelHashKey(u32 modelHashKey)
{
	m_ModelHashKey = modelHashKey;
	m_hashedModelHashKey = atDataHash(&m_ModelHashKey, sizeof(m_ModelHashKey));
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RestoreVehicle
// PURPOSE :  Restores one vehicle from a little struct
/////////////////////////////////////////////////////////////////////////////////

CVehicle *CStoredVehicle::RestoreVehicle()
{
	CVehicle *pNewVehicle;

	fwModelId vehicleModelId;

	if (atDataHash(&m_ModelHashKey,sizeof(m_ModelHashKey)) != m_hashedModelHashKey)
	{
		vehicleDisplayf("* restore vehicle check failed *");

		MoneyInterface::MemoryTampering( true );
		CNetworkTelemetry::AppendGarageTamper(m_ModelHashKey);
		Assertf(false, "Stored vehicle was not set using SetModelHashKey() call");
		return NULL;
	}

	CModelInfo::GetBaseModelInfoFromHashKey(m_ModelHashKey, &vehicleModelId);		//	ignores return value

//	fwModelId modelId((u32)m_ModelIndex);

	Assert(CModelInfo::HaveAssetsLoaded(vehicleModelId));

	if (CModelInfo::HaveAssetsLoaded(vehicleModelId))
	{
		// Make sure coordinates are on the map. (We got a bug with nan coordinates being saved)
		Assert(m_CoorX > WORLDLIMITS_XMIN);
		Assert(m_CoorX < WORLDLIMITS_XMAX);
		Assert(m_CoorY > WORLDLIMITS_YMIN);
		Assert(m_CoorY < WORLDLIMITS_YMAX);

		Matrix34 tempMat;
		tempMat.Identity();
		tempMat.d = Vector3(m_CoorX, m_CoorY, m_CoorZ);

		float	FrontX = m_iFrontX / FRONTCOMPR;
		float	FrontY = m_iFrontY / FRONTCOMPR;
		float	FrontZ = m_iFrontZ / FRONTCOMPR;
		tempMat.b.x = FrontX;	// Forward
		tempMat.b.y = FrontY;
		tempMat.b.z = FrontZ;
		tempMat.b.Normalize();

		tempMat.a.x = FrontY;	// Side
		tempMat.a.y = -FrontX;
		tempMat.a.z = 0.0f;
		tempMat.a.Normalize();

		tempMat.c.Cross(tempMat.a, tempMat.b); // Up
		tempMat.c.Normalize();

		pNewVehicle = CVehicleFactory::GetFactory()->Create(vehicleModelId, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_AMBIENT, &tempMat);
		
        if(pNewVehicle)
        {
		    pNewVehicle->SetIsAbandoned();

		    pNewVehicle->m_nDisableExtras = m_nDisableExtras;
		    // probably need to call some code to switch off extra fragment groups

		    pNewVehicle->m_nVehicleFlags.bFreebies = false;			// no freebies left in here
		    pNewVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = true;	// So that player can take it without commiting crime
		    pNewVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
		    pNewVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);// Make sure the car isn't locked (like police cars)

			pNewVehicle->m_nVehicleFlags.bTyresDontBurst = m_bTyresDontBurst;

		    pNewVehicle->m_nPhysicalFlags.bNotDamagedByBullets = m_bNotDamagedByBullets;
		    pNewVehicle->m_nPhysicalFlags.bNotDamagedByFlames = m_bNotDamagedByFlames;
		    pNewVehicle->m_nPhysicalFlags.bIgnoresExplosions = m_bIgnoresExplosions;
		    pNewVehicle->m_nPhysicalFlags.bNotDamagedByCollisions = m_bNotDamagedByCollisions;
		    pNewVehicle->m_nPhysicalFlags.bNotDamagedByMelee = m_bNotDamagedByMelee;

			CCustomShaderEffectVehicle *pShaderEffectVehicle = static_cast<CCustomShaderEffectVehicle*>(pNewVehicle->GetDrawHandler().GetShaderEffect());
			pShaderEffectVehicle->SetLicensePlateText( (const char*)m_LicencePlateText );

			pShaderEffectVehicle->SetLicensePlateTexIndex(m_plateTexIndex);

		    pNewVehicle->SwitchEngineOff();

			pNewVehicle->SetVariationInstance(m_variation);
    	
			pNewVehicle->SetLiveryId(m_LiveryId);

			pNewVehicle->SetLivery2Id(m_Livery2Id);

		    pNewVehicle->UpdateBodyColourRemapping();		// let shaders know, that body colours changed

			CInteriorInst* pDestInteriorInst = 0;		
			s32 destRoomIdx = -1;
			bool setWaitForAllCollisionsBeforeProbe = false;
			CAutomobile::PlaceOnRoadProperly((CAutomobile*)pNewVehicle, &tempMat, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, vehicleModelId.GetModelIndex(), NULL, false, NULL, NULL, 0);

			fwInteriorLocation loc = CGameWorld::OUTSIDE;
			if (pDestInteriorInst && pDestInteriorInst->CanReceiveObjects()){
				loc = CInteriorInst::CreateLocation(pDestInteriorInst, destRoomIdx);
			}

			CGameWorld::Add(pNewVehicle, loc);
			if (m_bInInterior){
				pNewVehicle->GetPortalTracker()->ScanUntilProbeTrue();
			}

			pNewVehicle->Fix();
			pNewVehicle->SetIsFixedWaitingForCollision(true);
			if(pNewVehicle->GetVehicleAudioEntity())
			{
				pNewVehicle->GetVehicleAudioEntity()->SetHornSoundIdx(m_HornSoundIndex);
				pNewVehicle->GetVehicleAudioEntity()->SetFakeEngineHealth(m_AudioEngineHealth);
				pNewVehicle->GetVehicleAudioEntity()->SetFakeBodyHealth(m_AudioBodyHealth);
			}
        }

		return (pNewVehicle);
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Clear
// PURPOSE :  Empties this stored vehicle
/////////////////////////////////////////////////////////////////////////////////

void CStoredVehicle::Clear()
{
	m_bUsed = false;
	m_bInteriorRequired = false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsUsed
// PURPOSE :  Returns whether there is a vehicle in this storedvehicle
/////////////////////////////////////////////////////////////////////////////////

bool CStoredVehicle::IsUsed()
{
	return m_bUsed;
}
