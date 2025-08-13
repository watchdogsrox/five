
#include "savegame_data_garages.h"
#include "savegame_data_garages_parser.h"


// Rage headers
#include "system/nelem.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"



void CSaveGarages::CSavedVehicleVariationInstance::CopyVehicleVariationBeforeSaving(const CStoredVehicleVariations *pVehicleVariationToSave)
{
	//	m_numMods = pVehicleVariationToSave->GetNumMods();

	m_kitIdx = INVALID_KIT_U8;
	m_kitID_U16 = INVALID_VEHICLE_KIT_ID;

	if((pVehicleVariationToSave->GetKitIndex() < CVehicleModelInfo::GetVehicleColours()->m_Kits.GetCount()) &&
		pVehicleVariationToSave->GetKitIndex() != INVALID_VEHICLE_KIT_INDEX)
	{
		m_kitID_U16 = CVehicleModelInfo::GetVehicleColours()->m_Kits[pVehicleVariationToSave->GetKitIndex()].GetId();
		savegameDebugf1("Store kit ID (%d) retrieved through valid index (%d)", m_kitID_U16, pVehicleVariationToSave->GetKitIndex());
	}

	atHashString HashStringOfVehicleModType;
	m_mods.Reset();
	for (u32 modLoop = 0; modLoop < VMT_MAX; modLoop++)
	{
		if (savegameVerifyf(CSaveGameMappingEnumsToHashStrings::GetHashString_VehicleModType(modLoop, HashStringOfVehicleModType), "CPlayerPedSaveStructure::PreSave - failed to find a hash string for vehicle mod type %d", modLoop))
		{
			m_mods.Insert(HashStringOfVehicleModType, pVehicleVariationToSave->GetModIndex(static_cast<eVehicleModType> (modLoop)));
		}
	}
	m_mods.FinishInsertion();

	m_color1 = pVehicleVariationToSave->GetColor1();
	m_color2 = pVehicleVariationToSave->GetColor2();
	m_color3 = pVehicleVariationToSave->GetColor3();
	m_color4 = pVehicleVariationToSave->GetColor4();
	m_color5 = pVehicleVariationToSave->GetColor5();
	m_color6 = pVehicleVariationToSave->GetColor6();

	m_smokeColR = pVehicleVariationToSave->GetSmokeColorR();
	m_smokeColG = pVehicleVariationToSave->GetSmokeColorG();
	m_smokeColB = pVehicleVariationToSave->GetSmokeColorB();

	m_neonColR = pVehicleVariationToSave->GetNeonColorR();
	m_neonColG = pVehicleVariationToSave->GetNeonColorG();
	m_neonColB = pVehicleVariationToSave->GetNeonColorB();
	m_neonFlags = pVehicleVariationToSave->GetNeonFlags();

	m_windowTint = pVehicleVariationToSave->GetWindowTint();

	m_wheelType = pVehicleVariationToSave->GetWheelType();

	m_modVariation[0] = pVehicleVariationToSave->HasModVariation(0);
	m_modVariation[1] = pVehicleVariationToSave->HasModVariation(1);
}

void CSaveGarages::CSavedVehicleVariationInstance::CopyVehicleVariationAfterLoading(CStoredVehicleVariations *pVehicleVariationToLoad)
{
	//	u8 m_numMods;

	// if the legacy kit index looks valid, then use it. Else use the new kit index instead. Next time it's saved, the legacy value will get invalidated.
	if (m_kitIdx != INVALID_KIT_U8)
	{
		pVehicleVariationToLoad->SetKitIndex(CVehicleModelInfo::GetVehicleColours()->GetKitIndexById(m_kitIdx));
		savegameDebugf1("Set kit index using m_kitIdx : %d", m_kitIdx);
	} else 
	{
		pVehicleVariationToLoad->SetKitIndex(CVehicleModelInfo::GetVehicleColours()->GetKitIndexById(m_kitID_U16));
		savegameDebugf1("Set kit index using m_kitID_U16 : %d", m_kitID_U16);
	}

	u32 modLoop = 0;
	for (modLoop = 0; modLoop < VMT_MAX; modLoop++)
	{
		pVehicleVariationToLoad->SetModIndex(static_cast<eVehicleModType> (modLoop), INVALID_MOD);
	}

	atBinaryMap<u8, atHashString>::Iterator VehicleModIterator = m_mods.Begin();
	while (VehicleModIterator != m_mods.End())
	{
		const atHashString HashStringOfVehicleModType = VehicleModIterator.GetKey();
		s32 VehicleModTypeIndex = CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString_VehicleModType(HashStringOfVehicleModType);
		if (savegameVerifyf(VehicleModTypeIndex >= 0, "CSaveGarages::CSavedVehicleVariationInstance::CopyVehicleVariationAfterLoading - couldn't find vehicle mod index for hash string %s", HashStringOfVehicleModType.GetCStr()))
		{
			pVehicleVariationToLoad->SetModIndex(static_cast<eVehicleModType> (VehicleModTypeIndex), *VehicleModIterator);
		}
		++VehicleModIterator;
	}

	pVehicleVariationToLoad->SetColor1(m_color1);
	pVehicleVariationToLoad->SetColor2(m_color2);
	pVehicleVariationToLoad->SetColor3(m_color3);
	pVehicleVariationToLoad->SetColor4(m_color4);
	pVehicleVariationToLoad->SetColor5(m_color5);
	pVehicleVariationToLoad->SetColor6(m_color6);

	pVehicleVariationToLoad->SetSmokeColorR(m_smokeColR);
	pVehicleVariationToLoad->SetSmokeColorG(m_smokeColG);
	pVehicleVariationToLoad->SetSmokeColorB(m_smokeColB);

	pVehicleVariationToLoad->SetNeonColorR(m_neonColR);
	pVehicleVariationToLoad->SetNeonColorG(m_neonColG);
	pVehicleVariationToLoad->SetNeonColorB(m_neonColB);
	pVehicleVariationToLoad->SetNeonFlags(m_neonFlags);

	pVehicleVariationToLoad->SetWindowTint(m_windowTint);
	pVehicleVariationToLoad->SetWheelType(m_wheelType);

	pVehicleVariationToLoad->SetModVariation(0, m_modVariation[0]);
	pVehicleVariationToLoad->SetModVariation(1, m_modVariation[1]);
}

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CSaveGarages::CSavedVehicleVariationInstance::Initialize(CSavedVehicleVariationInstance_Migration &copyFrom)
{
	// Copy all the members that don't need to change
	for (u32 loop = 0; loop < NELEM(m_modVariation); loop++)
	{
		m_modVariation[loop] = copyFrom.m_modVariation[loop];
	}

	m_kitIdx = copyFrom.m_kitIdx;

	m_color1 = copyFrom.m_color1;
	m_color2 = copyFrom.m_color2;
	m_color3 = copyFrom.m_color3;
	m_color4 = copyFrom.m_color4;
	m_color5 = copyFrom.m_color5;
	m_color6 = copyFrom.m_color6;

	m_smokeColR = copyFrom.m_smokeColR;
	m_smokeColG = copyFrom.m_smokeColG;
	m_smokeColB = copyFrom.m_smokeColB;

	m_neonColR = copyFrom.m_neonColR;
	m_neonColG = copyFrom.m_neonColG;
	m_neonColB = copyFrom.m_neonColB;
	m_neonFlags = copyFrom.m_neonFlags;

	m_windowTint = copyFrom.m_windowTint;
	m_wheelType = copyFrom.m_wheelType;
	m_kitID_U16 = copyFrom.m_kitID_U16;

	// Now copy the contents of the m_mods Binary Map
	// The source binary maps use u32 for the key.
	// The binary maps in this class use atHashString for the key.
	// Hopefully the game already knows what the strings are for these atHashStrings.
	m_mods.Reset();
	m_mods.Reserve(copyFrom.m_mods.GetCount());

	atBinaryMap<u8, u32>::Iterator modsIterator = copyFrom.m_mods.Begin();
	while (modsIterator != copyFrom.m_mods.End())
	{
		u32 keyAsU32 = modsIterator.GetKey();
		atHashString keyAsHashString(keyAsU32);
		u8 valueToInsert = *modsIterator;
		m_mods.Insert(keyAsHashString, valueToInsert);

		modsIterator++;
	}
	m_mods.FinishInsertion();
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES



void CSaveGarages::CSavedVehicle::CopyVehicleIntoSavedVehicle(CStoredVehicle *pVehicle)
{
	m_CoorX = pVehicle->m_CoorX;
	m_CoorY = pVehicle->m_CoorY;
	m_CoorZ = pVehicle->m_CoorZ;
	m_FlagsLocal = pVehicle->m_FlagsLocal;

	//	fwModelId vehicleModelId;
	//	vehicleModelId.SetModelIndex(pVehicle->m_ModelIndex);
	//	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(vehicleModelId);

	//	if(savegameVerifyf(pModelInfo, "CSavedVehicle::CopyVehicleIntoSavedVehicle - couldn't get modelinfo from modelindex"))
	//	{
	//		m_ModelHashKey = pModelInfo->GetHashKey();
	//	}

	m_ModelHashKey = pVehicle->GetModelHashKey();

	savegameDebugf1("Saving vehicle <%s> at (%.1f,%.1f,%.1f)", CModelInfo::GetBaseModelInfoName(CModelInfo::GetModelIdFromName(m_ModelHashKey)), m_CoorX, m_CoorY, m_CoorZ);

	m_variation.CopyVehicleVariationBeforeSaving(&pVehicle->m_variation);

	m_nDisableExtras = pVehicle->m_nDisableExtras;
	m_LiveryId = pVehicle->m_LiveryId;
	m_Livery2Id= pVehicle->m_Livery2Id;
	m_iFrontX = pVehicle->m_iFrontX;
	m_iFrontY = pVehicle->m_iFrontY;
	m_iFrontZ = pVehicle->m_iFrontZ;

	m_bUsed = pVehicle->m_bUsed;
	m_bInInterior = pVehicle->m_bInInterior;
	m_bNotDamagedByBullets = pVehicle->m_bNotDamagedByBullets;
	m_bNotDamagedByFlames = pVehicle->m_bNotDamagedByFlames;
	m_bIgnoresExplosions = pVehicle->m_bIgnoresExplosions;
	m_bNotDamagedByCollisions = pVehicle->m_bNotDamagedByCollisions;
	m_bNotDamagedByMelee = pVehicle->m_bNotDamagedByMelee;
	m_HornSoundIndex = pVehicle->m_HornSoundIndex;
	m_AudioEngineHealth = pVehicle->m_AudioEngineHealth;
	m_AudioBodyHealth = pVehicle->m_AudioBodyHealth;
	m_bTyresDontBurst = pVehicle->m_bTyresDontBurst;

	m_LicensePlateTexIndex = pVehicle->m_plateTexIndex;

	memcpy(m_LicencePlateText, pVehicle->m_LicencePlateText, CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX);

}


void CSaveGarages::CSavedVehicle::CopyVehicleOutOfSavedVehicle(CStoredVehicle *pVehicle)
{
	pVehicle->m_CoorX = m_CoorX;
	pVehicle->m_CoorY = m_CoorY;
	pVehicle->m_CoorZ = m_CoorZ;
	pVehicle->m_FlagsLocal = m_FlagsLocal;

	//	pVehicle->m_ModelIndex = (u16) CModelInfo::GetModelIdFromName(m_ModelHashKey).GetModelIndex();		// should this be (u16) ?
	pVehicle->SetModelHashKey(m_ModelHashKey);

	savegameDebugf1("Restoring vehicle <%s> at (%.1f,%.1f,%.1f)", CModelInfo::GetBaseModelInfoName(CModelInfo::GetModelIdFromName(m_ModelHashKey)), m_CoorX, m_CoorY, m_CoorZ);

	m_variation.CopyVehicleVariationAfterLoading(&pVehicle->m_variation);

	pVehicle->m_nDisableExtras = m_nDisableExtras;
	pVehicle->m_LiveryId = m_LiveryId;
	pVehicle->m_Livery2Id = m_Livery2Id;
	pVehicle->m_iFrontX = m_iFrontX;
	pVehicle->m_iFrontY = m_iFrontY;
	pVehicle->m_iFrontZ = m_iFrontZ;

	pVehicle->m_bUsed = m_bUsed;
	pVehicle->m_bInInterior = m_bInInterior;
	pVehicle->m_bNotDamagedByBullets = m_bNotDamagedByBullets;
	pVehicle->m_bNotDamagedByFlames = m_bNotDamagedByFlames;
	pVehicle->m_bIgnoresExplosions = m_bIgnoresExplosions;
	pVehicle->m_bNotDamagedByCollisions = m_bNotDamagedByCollisions;
	pVehicle->m_bNotDamagedByMelee = m_bNotDamagedByMelee;
	pVehicle->m_HornSoundIndex = m_HornSoundIndex;
	pVehicle->m_AudioEngineHealth = m_AudioEngineHealth;
	pVehicle->m_AudioBodyHealth = m_AudioBodyHealth;
	pVehicle->m_bTyresDontBurst = m_bTyresDontBurst;

	pVehicle->m_plateTexIndex = m_LicensePlateTexIndex;
	memcpy(pVehicle->m_LicencePlateText, m_LicencePlateText, CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX);
}

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CSaveGarages::CSavedVehicle::Initialize(CSavedVehicle_Migration &copyFrom)
{
	m_variation.Initialize(copyFrom.m_variation);
	m_CoorX = copyFrom.m_CoorX;
	m_CoorY = copyFrom.m_CoorY;
	m_CoorZ = copyFrom.m_CoorZ;
	m_FlagsLocal = copyFrom.m_FlagsLocal;
	m_ModelHashKey = copyFrom.m_ModelHashKey;
	m_nDisableExtras = copyFrom.m_nDisableExtras;
	m_LiveryId = copyFrom.m_LiveryId;
	m_Livery2Id = copyFrom.m_Livery2Id;
	m_HornSoundIndex = copyFrom.m_HornSoundIndex;
	m_AudioEngineHealth = copyFrom.m_AudioEngineHealth;
	m_AudioBodyHealth = copyFrom.m_AudioBodyHealth;
	m_iFrontX = copyFrom.m_iFrontX;
	m_iFrontY = copyFrom.m_iFrontY;
	m_iFrontZ = copyFrom.m_iFrontZ;
	m_bUsed = copyFrom.m_bUsed;
	m_bInInterior = copyFrom.m_bInInterior;
	m_bNotDamagedByBullets = copyFrom.m_bNotDamagedByBullets;
	m_bNotDamagedByFlames = copyFrom.m_bNotDamagedByFlames;
	m_bIgnoresExplosions = copyFrom.m_bIgnoresExplosions;
	m_bNotDamagedByCollisions = copyFrom.m_bNotDamagedByCollisions;
	m_bNotDamagedByMelee = copyFrom.m_bNotDamagedByMelee;
	m_bTyresDontBurst = copyFrom.m_bTyresDontBurst;

	m_LicensePlateTexIndex = copyFrom.m_LicensePlateTexIndex;

	for (u32 loop = 0; loop < NELEM(m_LicencePlateText); loop++)
	{
		m_LicencePlateText[loop] = copyFrom.m_LicencePlateText[loop];
	}
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CSaveGarages::CSavedVehiclesInSafeHouse::Initialize(CSavedVehiclesInSafeHouse_Migration &copyFrom)
{
	m_NameHashOfGarage = copyFrom.m_NameHashOfGarage;

	const s32 numberOfVehiclesToCopy = copyFrom.m_VehiclesSavedInThisSafeHouse.GetCount();

	m_VehiclesSavedInThisSafeHouse.Reset();
	m_VehiclesSavedInThisSafeHouse.Reserve(numberOfVehiclesToCopy);
	for (s32 loop = 0; loop < numberOfVehiclesToCopy; loop++)
	{
		CSavedVehicle &newEntry = m_VehiclesSavedInThisSafeHouse.Append();
		newEntry.Initialize(copyFrom.m_VehiclesSavedInThisSafeHouse[loop]);
	}
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


void CSaveGarages::CSaveGarage::CopyGarageIntoSaveGarage(CGarage *pGarage)
{
	m_NameHash = pGarage->name.GetHash();
	m_Type = (u8)pGarage->type;
	m_bLeaveCameraAlone = pGarage->Flags.bLeaveCameraAlone;
	m_bSavingVehilclesEnabled = pGarage->Flags.bSavingVehiclesEnabled;
}


void CSaveGarages::CSaveGarage::CopyGarageOutOfSaveGarage(CGarage *pGarage)
{
	//	pGarage->name = m_NameHash;
	pGarage->type = (GarageType)m_Type;
	pGarage->Flags.bLeaveCameraAlone = m_bLeaveCameraAlone;
	pGarage->Flags.bSavingVehiclesEnabled = m_bSavingVehilclesEnabled;
}


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CSaveGarages::CSaveGarage::Initialize(CSaveGarage_Migration &copyFrom)
{
	m_NameHash = copyFrom.m_NameHash;

	m_Type = copyFrom.m_Type;
	m_bLeaveCameraAlone = copyFrom.m_bLeaveCameraAlone;
	m_bSavingVehilclesEnabled = copyFrom.m_bSavingVehilclesEnabled;
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CSaveGarages::Initialize(CSaveGarages_Migration &copyFrom)
{
	m_RespraysAreFree = copyFrom.m_RespraysAreFree;
	m_NoResprays = copyFrom.m_NoResprays;
	//	aCarsInSafeHouse

	const s32 numberOfSafeHousesToCopy = copyFrom.m_SafeHouses.GetCount();
	m_SafeHouses.Reset();
	m_SafeHouses.Reserve(numberOfSafeHousesToCopy);
	for (s32 safeHouseLoop = 0; safeHouseLoop < numberOfSafeHousesToCopy; safeHouseLoop++)
	{
		CSavedVehiclesInSafeHouse &newEntry = m_SafeHouses.Append();
		newEntry.Initialize(copyFrom.m_SafeHouses[safeHouseLoop]);
	}

	const s32 numberOfGaragesToCopy = copyFrom.m_SavedGarages.GetCount();
	m_SavedGarages.Reset();
	m_SavedGarages.Reserve(numberOfGaragesToCopy);
	for (s32 garageLoop = 0; garageLoop < numberOfGaragesToCopy; garageLoop++)
	{
		CSaveGarage &newEntry = m_SavedGarages.Append();
		newEntry.Initialize(copyFrom.m_SavedGarages[garageLoop]);
	}
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


void CSaveGarages::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSaveGarages::PreSave"))
	{
		return;
	}

	//	if (CGenericGameStorage::IsCalculatingBufferSize() == false)
	{
		CGarages::CloseHideOutGaragesBeforeSave();
	}

	m_RespraysAreFree = CGarages::RespraysAreFree;
	m_NoResprays = CGarages::NoResprays;

	m_SafeHouses.Resize(CGarages::NumSafeHousesRegistered);
	for (u32 safeHouseIndex = 0; safeHouseIndex < CGarages::NumSafeHousesRegistered; safeHouseIndex++)
	{
		s32 GarageIndex = CGarages::FindGarageWithSafeHouseIndex(safeHouseIndex);
		if (Verifyf(GarageIndex >= 0, "CSaveGarages::PreSave - didn't find garage for safe house index %d", safeHouseIndex))
		{
			m_SafeHouses[safeHouseIndex].m_NameHashOfGarage = CGarages::aGarages[GarageIndex].name.GetHash();
		}
		else
		{
			m_SafeHouses[safeHouseIndex].m_NameHashOfGarage = atHashString::Null();
		}

		m_SafeHouses[safeHouseIndex].m_VehiclesSavedInThisSafeHouse.Resize(MAX_NUM_STORED_CARS_IN_SAFEHOUSE);
		for (u32 C = 0; C < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; C++)
		{
			m_SafeHouses[safeHouseIndex].m_VehiclesSavedInThisSafeHouse[C].CopyVehicleIntoSavedVehicle(&CGarages::aCarsInSafeHouse[safeHouseIndex][C]);
		}
	}

	m_SavedGarages.Resize(CGarages::aGarages.GetCount());
	for (u32 Garage = 0; Garage < CGarages::aGarages.GetCount(); Garage++)
	{
		m_SavedGarages[Garage].CopyGarageIntoSaveGarage(&CGarages::aGarages[Garage]);
	}

	//	if (CGenericGameStorage::IsCalculatingBufferSize() == false)
	{
		CGarages::OpenHideOutGaragesAfterSave();
	}
}

void CSaveGarages::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSaveGarages::PostLoad"))
	{
		return;
	}

	//	CGarages::CloseHideOutGaragesBeforeSave();	I don't think this should be here

	CGarages::RespraysAreFree = m_RespraysAreFree;
	CGarages::NoResprays = m_NoResprays;

	for (u32 HouseLoop = 0; HouseLoop < m_SafeHouses.GetCount(); HouseLoop++)
	{
		s32 GarageIndex = CGarages::FindGarageIndexFromNameHash(m_SafeHouses[HouseLoop].m_NameHashOfGarage.GetHash());
		if (savegameVerifyf(GarageIndex >= 0, "CSaveGarages::PostLoad - couldn't find SafeHouse garage with nameHash %u", m_SafeHouses[HouseLoop].m_NameHashOfGarage.GetHash()))
		{
			if (Verifyf(CGarages::aGarages[GarageIndex].type == GARAGE_SAFEHOUSE_PARKING_ZONE, "CSaveGarages::PostLoad - garage is not a GARAGE_SAFEHOUSE_PARKING_ZONE"))
			{
				const s32 safeHouseIndex = CGarages::aGarages[GarageIndex].safeHouseIndex;
				if (Verifyf(safeHouseIndex >= 0 && safeHouseIndex < NUM_SAFE_HOUSES, "CSaveGarages::PostLoad - safeHouseIndex %d is out of range", safeHouseIndex))
				{
					for (u32 C = 0; C < m_SafeHouses[HouseLoop].m_VehiclesSavedInThisSafeHouse.GetCount(); C++)
					{
						m_SafeHouses[HouseLoop].m_VehiclesSavedInThisSafeHouse[C].CopyVehicleOutOfSavedVehicle(&CGarages::aCarsInSafeHouse[safeHouseIndex][C]);
					}
				}
			}
		}
	}

	for (u32 SavedGarageIndex = 0; SavedGarageIndex < m_SavedGarages.GetCount(); SavedGarageIndex++)
	{
		s32 RealGarageIndex = CGarages::FindGarageIndexFromNameHash(m_SavedGarages[SavedGarageIndex].m_NameHash);
		if (savegameVerifyf(RealGarageIndex >= 0, "CSaveGarages::PostLoad - couldn't find garage with nameHash %u", m_SavedGarages[SavedGarageIndex].m_NameHash.GetHash()))
		{
			m_SavedGarages[SavedGarageIndex].CopyGarageOutOfSaveGarage(&CGarages::aGarages[RealGarageIndex]);
		}
	}

	// Clear the messages
	CGarages::pGarageCamShouldBeOutside = NULL;
}



//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
// ******************* CSaveGarages_Migration *******************
void CSavedVehicleVariationInstance_Migration::Initialize(CSaveGarages::CSavedVehicleVariationInstance &copyFrom)
{
// Copy all the members that don't need to change
	for (u32 loop = 0; loop < NELEM(m_modVariation); loop++)
	{
		m_modVariation[loop] = copyFrom.m_modVariation[loop];
	}

	m_kitIdx = copyFrom.m_kitIdx;

	m_color1 = copyFrom.m_color1;
	m_color2 = copyFrom.m_color2;
	m_color3 = copyFrom.m_color3;
	m_color4 = copyFrom.m_color4;
	m_color5 = copyFrom.m_color5;
	m_color6 = copyFrom.m_color6;

	m_smokeColR = copyFrom.m_smokeColR;
	m_smokeColG = copyFrom.m_smokeColG;
	m_smokeColB = copyFrom.m_smokeColB;

	m_neonColR = copyFrom.m_neonColR;
	m_neonColG = copyFrom.m_neonColG;
	m_neonColB = copyFrom.m_neonColB;
	m_neonFlags = copyFrom.m_neonFlags;

	m_windowTint = copyFrom.m_windowTint;
	m_wheelType = copyFrom.m_wheelType;
	m_kitID_U16 = copyFrom.m_kitID_U16;

// Now copy the contents of the m_mods Binary Map
// The original binary map used atHashString for the key.
// The string for an atHashString is stripped out of Final executables.
// The binary map in this class uses u32 for the key so that it can be written to an XML file by a Final executable.
	m_mods.Reset();
	m_mods.Reserve(copyFrom.m_mods.GetCount());

	atBinaryMap<u8, atHashString>::Iterator modsIterator = copyFrom.m_mods.Begin();
	while (modsIterator != copyFrom.m_mods.End())
	{
		u32 keyAsU32 = modsIterator.GetKey().GetHash();
		u8 valueToInsert = *modsIterator;
		m_mods.Insert(keyAsU32, valueToInsert);

		modsIterator++;
	}
	m_mods.FinishInsertion();
}



void CSavedVehicle_Migration::Initialize(CSaveGarages::CSavedVehicle &copyFrom)
{
	m_variation.Initialize(copyFrom.m_variation);
	m_CoorX = copyFrom.m_CoorX;
	m_CoorY = copyFrom.m_CoorY;
	m_CoorZ = copyFrom.m_CoorZ;
	m_FlagsLocal = copyFrom.m_FlagsLocal;
	m_ModelHashKey = copyFrom.m_ModelHashKey;
	m_nDisableExtras = copyFrom.m_nDisableExtras;
	m_LiveryId = copyFrom.m_LiveryId;
	m_Livery2Id = copyFrom.m_Livery2Id;
	m_HornSoundIndex = copyFrom.m_HornSoundIndex;
	m_AudioEngineHealth = copyFrom.m_AudioEngineHealth;
	m_AudioBodyHealth = copyFrom.m_AudioBodyHealth;
	m_iFrontX = copyFrom.m_iFrontX;
	m_iFrontY = copyFrom.m_iFrontY;
	m_iFrontZ = copyFrom.m_iFrontZ;
	m_bUsed = copyFrom.m_bUsed;
	m_bInInterior = copyFrom.m_bInInterior;
	m_bNotDamagedByBullets = copyFrom.m_bNotDamagedByBullets;
	m_bNotDamagedByFlames = copyFrom.m_bNotDamagedByFlames;
	m_bIgnoresExplosions = copyFrom.m_bIgnoresExplosions;
	m_bNotDamagedByCollisions = copyFrom.m_bNotDamagedByCollisions;
	m_bNotDamagedByMelee = copyFrom.m_bNotDamagedByMelee;
	m_bTyresDontBurst = copyFrom.m_bTyresDontBurst;

	m_LicensePlateTexIndex = copyFrom.m_LicensePlateTexIndex;

	for (u32 loop = 0; loop < NELEM(m_LicencePlateText); loop++)
	{
		m_LicencePlateText[loop] = copyFrom.m_LicencePlateText[loop];
	}	
}




void CSavedVehiclesInSafeHouse_Migration::Initialize(CSaveGarages::CSavedVehiclesInSafeHouse &copyFrom)
{
	m_NameHashOfGarage = copyFrom.m_NameHashOfGarage;

	const s32 numberOfVehiclesToCopy = copyFrom.m_VehiclesSavedInThisSafeHouse.GetCount();

	m_VehiclesSavedInThisSafeHouse.Reset();
	m_VehiclesSavedInThisSafeHouse.Reserve(numberOfVehiclesToCopy);
	for (s32 loop = 0; loop < numberOfVehiclesToCopy; loop++)
	{
		CSavedVehicle_Migration &newEntry = m_VehiclesSavedInThisSafeHouse.Append();
		newEntry.Initialize(copyFrom.m_VehiclesSavedInThisSafeHouse[loop]);
	}
}



void CSaveGarage_Migration::Initialize(CSaveGarages::CSaveGarage &copyFrom)
{
	m_NameHash = copyFrom.m_NameHash;

	m_Type = copyFrom.m_Type;
	m_bLeaveCameraAlone = copyFrom.m_bLeaveCameraAlone;
	m_bSavingVehilclesEnabled = copyFrom.m_bSavingVehilclesEnabled;
}


void CSaveGarages_Migration::Initialize(CSaveGarages &copyFrom)
{
	m_RespraysAreFree = copyFrom.m_RespraysAreFree;
	m_NoResprays = copyFrom.m_NoResprays;
	//	aCarsInSafeHouse

	const s32 numberOfSafeHousesToCopy = copyFrom.m_SafeHouses.GetCount();
	m_SafeHouses.Reset();
	m_SafeHouses.Reserve(numberOfSafeHousesToCopy);
	for (s32 safeHouseLoop = 0; safeHouseLoop < numberOfSafeHousesToCopy; safeHouseLoop++)
	{
		CSavedVehiclesInSafeHouse_Migration &newEntry = m_SafeHouses.Append();
		newEntry.Initialize(copyFrom.m_SafeHouses[safeHouseLoop]);
	}

	const s32 numberOfGaragesToCopy = copyFrom.m_SavedGarages.GetCount();
	m_SavedGarages.Reset();
	m_SavedGarages.Reserve(numberOfGaragesToCopy);
	for (s32 garageLoop = 0; garageLoop < numberOfGaragesToCopy; garageLoop++)
	{
		CSaveGarage_Migration &newEntry = m_SavedGarages.Append();
		newEntry.Initialize(copyFrom.m_SavedGarages[garageLoop]);
	}
}

	
void CSaveGarages_Migration::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSaveGarages_Migration::PreSave"))
	{
		return;
	}
}

void CSaveGarages_Migration::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSaveGarages_Migration::PostLoad"))
	{
		return;
	}
}

//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

