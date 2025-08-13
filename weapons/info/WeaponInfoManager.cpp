//
// weapons/weaponinfomanager.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// Class header
#include "Weapons/Info/WeaponInfoManager.h"

// Parser files
#include "WeaponInfoManager_parser.h"

// Rage headers
#include "parser/manager.h"
#include "parser/restparserservices.h"

// Game headers
#include "Peds/ped.h"
#include "Scene/DataFileMgr.h"
#include "Scene/DynamicEntity.h"
#include "scene/ExtraContent.h"
#include "Weapons/WeaponDebug.h"
#include "Weapons/WeaponManager.h"
#include "Weapons/WeaponTypes.h"

#include "network/General/NetworkHasherUtil.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

// Max number of weapon blobs (number of individual files)
#define MAX_WEAPON_INFO_BLOBS		(140)

//////////////////////////////////////////////////////////////////////////
// CWeaponInfoManager
//////////////////////////////////////////////////////////////////////////

// Static initialisation
CWeaponInfoManager CWeaponInfoManager::ms_Instance;

class CWeaponInfoDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType) 
		{
			case CDataFileMgr::WEAPONINFO_FILE:
				Assertf(!CWeaponInfoManager::FindWeaponInfoBlobByFileName(file.m_filename), 
					"Found weapon info blob with the same filename while loading %s. Please make sure all DLC weapon info blobs have unique names", 
					file.m_filename);
				CWeaponInfoManager::Load(file);
				CWeaponInfoManager::PostLoad();
				return true;

			case CDataFileMgr::WEAPONINFO_FILE_PATCH:
				CWeaponInfoManager::LoadPatch(file);
				return true;

			default:
				return false;
		}
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file ) 
	{
		switch(file.m_fileType) 
		{
			case CDataFileMgr::WEAPONINFO_FILE:
				CWeaponInfoManager::Unload(file);
				break;

			case CDataFileMgr::WEAPONINFO_FILE_PATCH:
				CWeaponInfoManager::UnloadPatch(file);
				break;

			default:
				break;
		}
	}

} g_WeaponInfoDataFileMounter;


#if __BANK
// Strings corresponding to the enum InfoType - ensure this matches with InfoType enum
const char* CWeaponInfoManager::ms_InfoTypeNames[CWeaponInfoBlob::IT_Max] =
{
	"Ammo Infos",
	"Weapon Infos",
	"Vehicle Weapon Infos",
	"Damage Infos",
};
// Force the shader update when set
bank_bool CWeaponInfoManager::ms_bForceShaderUpdate = false;
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CWeaponInfoBlob::SlotEntry::Validate()
{
	Assertf(m_Entry.GetHash(), "Slot with order number %d has invalid hash - check the XML file", m_OrderNumber);
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CWeaponInfoBlob::sWeaponSlotList::Validate()
{
	for (int x=0; x<WeaponSlots.GetCount(); x++)
	{
		WeaponSlots[x].Validate();
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

CWeaponInfoManager::CWeaponInfoManager()
{
	m_WeaponInfoBlobs.Reserve(MAX_WEAPON_INFO_BLOBS);
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::Init()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::WEAPONINFO_FILE, &g_WeaponInfoDataFileMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::WEAPONINFO_FILE_PATCH, &g_WeaponInfoDataFileMounter);

	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::Shutdown()
{
	// Delete any existing data
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfoManager::CheckDamagedGeneralWeaponType(atHashWithStringNotFinal uDamagedByHash, s32 iGeneralWeaponType)
{
	if (uDamagedByHash.GetHash() == 0)
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uDamagedByHash);
	weaponAssert(pWeaponInfo);
	eDamageType damageType = pWeaponInfo->GetDamageType();

	if (iGeneralWeaponType == GENERALWEAPON_TYPE_ANYMELEE)
	{
		return damageType == DAMAGE_TYPE_MELEE;	
	}
	else if (iGeneralWeaponType == GENERALWEAPON_TYPE_ANYWEAPON)
	{
		return damageType == DAMAGE_TYPE_MELEE || 
			damageType == DAMAGE_TYPE_BULLET || 
			damageType == DAMAGE_TYPE_EXPLOSIVE || 
			damageType == DAMAGE_TYPE_ELECTRIC ||
			damageType == DAMAGE_TYPE_FIRE ||
			damageType == DAMAGE_TYPE_TRANQUILIZER;
	}
	else
	{
		weaponAssertf(0, "CWeaponInfoManager::CheckDamagedWeaponType - Unknown general weapon type [%d]", iGeneralWeaponType);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::UpdateInfoPtrs()
{
	ms_Instance.m_InfoPtrs.Infos.Reset();

	// Create a binary searchable list of infos
	for (s32 k=0; k < ms_Instance.m_WeaponInfoBlobs.GetCount(); k++)
	{
		CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs[k];

		for(s32 i = 0; i < CWeaponInfoBlob::IT_Max; i++)
		{
			sWeaponInfoList &infos = blob.GetInfos((CWeaponInfoBlob::InfoType) i);
			for(s32 j = 0; j < infos.Infos.GetCount(); j++)
			{
				ms_Instance.m_InfoPtrs.Infos.PushAndGrow(infos.Infos[j]);
			}
		}
	}
	ms_Instance.m_InfoPtrs.Infos.QSort(0, ms_Instance.m_InfoPtrs.Infos.GetCount(), CItemInfo::PairSort);
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoBlob::Validate()
{
#if __ASSERT
	m_SlotBestOrder.Validate();

	for (int x = 0; x < m_SlotNavigateOrder.GetCount(); x++)
	{
		m_SlotNavigateOrder[x].Validate();
	}
#endif // __ASSERT
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoBlob::Reset()
{
	// Delete the infos
	for(s32 i = 0; i < IT_Max; i++)
	{
		for(s32 j = 0; j < m_Infos[i].Infos.GetCount(); j++)
		{
			delete m_Infos[i].Infos[j];
		}
		m_Infos[i].Infos.Reset();
	}

	// Delete the gun task infos
	for(s32 i = 0; i < m_AimingInfos.GetCount(); i++)
	{
		delete m_AimingInfos[i];
	}
	m_AimingInfos.Reset();

	// Delete vehicle weapon infos
	for(s32 i = 0; i < m_VehicleWeaponInfos.GetCount(); i++)
	{
		delete m_VehicleWeaponInfos[i];
	}
	m_VehicleWeaponInfos.Reset();
	m_TintSpecValues.Reset();
	m_FiringPatternAliases.Reset();
	m_UpperBodyFixupExpressionData.Reset();
	m_SlotNavigateOrder.Reset();
	m_patchHash = 0;
	m_Filename = NULL;
	m_Name = NULL;
}

////////////////////////////////////////////////////////////////////////////////

CWeaponInfoBlob *CWeaponInfoManager::FindWeaponInfoBlobByFileName(const char *fname)
{
	const char *patchFileName = ASSET.FileName(fname);
	Assert(patchFileName);

	for(int i=0; i<ms_Instance.m_WeaponInfoBlobs.GetCount(); ++i)
	{
		CWeaponInfoBlob *pBlob = &ms_Instance.m_WeaponInfoBlobs[i];

		if(!pBlob->GetFilename())
			continue;

		const char *blobFileName = ASSET.FileName(pBlob->GetFilename());

		if(strnicmp(patchFileName, blobFileName, RAGE_MAX_PATH) == 0)
		{
			return pBlob;
		}
	}	

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfoManager::CalculateWeaponPatchCRC(u32 initValue)
{
	for(int i=0; i<ms_Instance.m_WeaponInfoBlobs.GetCount(); ++i)
	{
		const CWeaponInfoBlob *pBlob = &ms_Instance.m_WeaponInfoBlobs[i];

		if(pBlob->GetPatchHash())
		{
			u32 hash = pBlob->GetPatchHash();
			initValue = atDataHash((const unsigned int*)&hash, (s32)sizeof(hash), initValue);
		}
	}

	return initValue;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::Reset()
{
	EXTRACONTENT.ExecuteTitleUpdateDataPatch((u32)CCS_TITLE_UPDATE_WEAPON_PATCH, false);
	EXTRACONTENT.ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_WEAPON_PATCH, false);

	ms_Instance.m_InfoPtrs.Infos.Reset();

	// We need to Reset() the blobs even though we empty the array later on -
	// just calling Resize() won't empty the blobs' arrays, so their data would
	// remain in memory.
	for (int x=0; x<ms_Instance.m_WeaponInfoBlobs.GetCount(); x++)
	{
		ms_Instance.m_WeaponInfoBlobs[x].Reset();
	}

	ms_Instance.m_WeaponInfoBlobs.Resize(0);

#if __BANK
	// Empty the names list
	for(s32 i = 0; i < CWeaponInfoBlob::IT_Max; i++)
	{
		ms_Instance.m_InfoNames[i].Reset();
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::Load()
{
	// As peds own references to weapon infos, we need to delete them all
	BANK_ONLY(RemoveAllWeaponInfoReferences());

	// Delete any existing data
	Reset();

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WEAPONINFO_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			Load(*pData);
		}

		// Get next file
		pData = DATAFILEMGR.GetNextFile(pData);
	}

	PostLoad();

	EXTRACONTENT.ExecuteTitleUpdateDataPatch((u32)CCS_TITLE_UPDATE_WEAPON_PATCH, true);
	EXTRACONTENT.ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_WEAPON_PATCH, true);
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::Load(const CDataFileMgr::DataFile & file)
{
	DIAG_CONTEXT_MESSAGE("WeaponInfo(%s)", file.m_filename);

	// Load
	weaponAssertf(ms_Instance.m_WeaponInfoBlobs.GetCount() < ms_Instance.m_WeaponInfoBlobs.GetCapacity(), "Ran out of weaponinfo storage (size %i), ask gameplay code to bump MAX_WEAPON_INFO_BLOBS", ms_Instance.m_WeaponInfoBlobs.GetCapacity());
	CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs.Append();

	// Validate the blob.
	if (Verifyf(PARSER.LoadObject(file.m_filename, "meta", blob), "Weapon info blob %s is broken", file.m_filename))
	{
		parRestRegisterSingleton("Weapons/WeaponInfo", blob, ""); // Make this object browsable / editable via rest services (i.e. the web)
		blob.SetFilename(file.m_filename);
	}
	else
	{
		ms_Instance.m_WeaponInfoBlobs.Pop();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::LoadPatch(const CDataFileMgr::DataFile & file)
{
	CWeaponInfoBlob *blobToPatch = FindWeaponInfoBlobByFileName(file.m_filename);

	if(Verifyf(blobToPatch, "Couldn't find weapon info blob to patch for file %s", file.m_filename))
	{
		parSettings settings;
		settings.SetFlag(parSettings::INITIALIZE_MISSING_DATA, false);

#if !__NO_OUTPUT
		u32 originalHash = GetParsableStructHash(*blobToPatch, 0);			
#endif// !__NO_OUTPUT
		
		if(Verifyf(PARSER.LoadObject(file.m_filename, "meta", *blobToPatch, &settings), 
			"CWeaponInfoManager::LoadPatch couldn't load %s for patching", file.m_filename))
		{
			u32 updatedHash = GetParsableStructHash(*blobToPatch, 0);			
			blobToPatch->SetPatchHash(updatedHash);

#if !__NO_OUTPUT
			Displayf("Applied weapon patch from %s, replacing %s, originalHash = 0x%X, updatedHash = 0x%X", file.m_filename, blobToPatch->GetFilename(), originalHash, updatedHash);
#endif// !__NO_OUTPUT

		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::Unload(const CDataFileMgr::DataFile & file)
{
	CWeaponInfoBlob* pBlob = rage_new CWeaponInfoBlob;
	if(PARSER.LoadObject(file.m_filename, "meta", *pBlob))
	{
		atLiteralHashValue blobToRemoveHash(pBlob->GetName());

		Displayf("Rem blob name: %s hash: %u blobcount: %u", pBlob->GetName().c_str(), blobToRemoveHash.GetHash(), ms_Instance.m_WeaponInfoBlobs.GetCount());

		for(int i=0; i<ms_Instance.m_WeaponInfoBlobs.GetCount(); ++i)
		{
			atLiteralHashValue currentBlobHash(ms_Instance.m_WeaponInfoBlobs[i].GetName());
			Displayf("In blob name: %s hash %u", ms_Instance.m_WeaponInfoBlobs[i].GetName().c_str(), currentBlobHash.GetHash());

			if(blobToRemoveHash == currentBlobHash)
			{
				ms_Instance.m_WeaponInfoBlobs[i].Reset();
				ms_Instance.m_WeaponInfoBlobs.Delete(i);
				i--;
			}
			//ms_Instance.m_WeaponInfoBlobs.Pop();
		}
		Displayf("Blobcount %u", ms_Instance.m_WeaponInfoBlobs.GetCount());
	}
	delete pBlob;
	UpdateInfoPtrs();
	PostLoad();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::UnloadPatch(const CDataFileMgr::DataFile & file)
{
	CWeaponInfoBlob *blobToUnPatch = FindWeaponInfoBlobByFileName(file.m_filename);

	if(Verifyf(blobToUnPatch, "Couldn't find weapon info blob to unpatch for file %s", file.m_filename))
	{
		if(Verifyf(PARSER.LoadObject(blobToUnPatch->GetFilename(), "meta", *blobToUnPatch), 
			"CWeaponInfoManager::LoadPatch couldn't load %s for unpatching", blobToUnPatch->GetFilename()))
		{
			Displayf("Reverted weapon patch for %s -> loaded back %s", file.m_filename, blobToUnPatch->GetFilename());
			blobToUnPatch->SetPatchHash(0);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::PostLoad()
{
	CollateInfoBlobs();

#if !__FINAL
	// Validate the data
	Validate();
#endif // !__FINAL

	// Create the unarmed weapon
	CWeaponManager::CreateWeaponUnarmed(WEAPONTYPE_UNARMED);
}

////////////////////////////////////////////////////////////////////////////////
void CWeaponInfoManager::CollateInfoBlobs()
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();

	int slotNavigateOrderCount[CWeaponInfoBlob::WS_Max] = {0};
	int infosCount[CWeaponInfoBlob::IT_Max] = {0};
	int slotBestOrderCount = 0;
	int aimingInfoCount = 0;


	// First pass - get the size of each collated array.
	for (int x=0; x<count; x++)
	{
		CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs[x];

		for (int y=0; y<blob.GetSlotNavigateOrderCount(); y++)
		{
			slotNavigateOrderCount[y] += blob.GetSlotNavigateOrder((CWeaponInfoBlob::WeaponSlots) y).WeaponSlots.GetCount();
		}

		for (int y=0; y<CWeaponInfoBlob::IT_Max; y++)
		{
			infosCount[y] += blob.GetInfos((CWeaponInfoBlob::InfoType) y).Infos.GetCount();
		}

		slotBestOrderCount += blob.GetSlotBestOrder().WeaponSlots.GetCount();
		aimingInfoCount += blob.GetAimingInfoCount();
	}

	ms_Instance.m_SlotNavigateOrder.Reset();
	ms_Instance.m_SlotNavigateOrder.Resize(CWeaponInfoBlob::WS_Max);

	// Second pass - create the arrays and collate the data. This is for the per-slot ones.
	for (int y=0; y<CWeaponInfoBlob::WS_Max; y++)
	{
		ms_Instance.m_SlotNavigateOrder[y].WeaponSlots.Reset();
		ms_Instance.m_SlotNavigateOrder[y].WeaponSlots.Resize(slotNavigateOrderCount[y]);

		int snoIndex = 0;

		for (int x=0; x<count; x++)
		{
			CWeaponInfoBlob::WeaponSlots slot = (CWeaponInfoBlob::WeaponSlots) y;
			CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs[x];

			// These are POD, so we can just copy them.
			if(slot < blob.GetSlotNavigateOrderCount())
			{
				int snoEntryCount = blob.GetSlotNavigateOrder(slot).WeaponSlots.GetCount();
				if (snoEntryCount)
				{
					sysMemCpy(&ms_Instance.m_SlotNavigateOrder[y].WeaponSlots[snoIndex], blob.GetSlotNavigateOrder(slot).WeaponSlots.GetElements(), sizeof(CWeaponInfoBlob::SlotEntry) * snoEntryCount);
					snoIndex += snoEntryCount;
				}
			}
		}

		std::sort(ms_Instance.m_SlotNavigateOrder[y].WeaponSlots.begin(), ms_Instance.m_SlotNavigateOrder[y].WeaponSlots.end(), CWeaponInfoBlob::SlotEntry::SortFunc);

#if __ASSERT
		for (int x=0; x<ms_Instance.m_SlotNavigateOrder[y].WeaponSlots.GetCount() - 1; x++)
		{
			Assertf(ms_Instance.m_SlotNavigateOrder[y].WeaponSlots[x].m_OrderNumber != ms_Instance.m_SlotNavigateOrder[y].WeaponSlots[x+1].m_OrderNumber,
				"%s has the same slot navigate order number as %s (%d). You should give them unique values so the order is not ambiguous.",
				ms_Instance.m_SlotNavigateOrder[y].WeaponSlots[x].m_Entry.GetCStr(), ms_Instance.m_SlotNavigateOrder[y].WeaponSlots[x+1].m_Entry.GetCStr(),
				ms_Instance.m_SlotNavigateOrder[y].WeaponSlots[x].m_OrderNumber);
		}
#endif // __ASSERT
	}

	// And the per-type ones.
	// Let's do the per-type ones here.
	for (int it=0; it<CWeaponInfoBlob::IT_Max; it++)
	{
		ms_Instance.m_Infos[it].Infos.Reset();
		ms_Instance.m_Infos[it].Infos.Resize(infosCount[it]);

		int infoIndex = 0;

		for (int x=0; x<count; x++)
		{
			CWeaponInfoBlob::InfoType infoType = (CWeaponInfoBlob::InfoType) it;
			CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs[x];

			int infoEntryCount = blob.GetInfos(infoType).Infos.GetCount();

			if (infoEntryCount)
			{
				sysMemCpy(&ms_Instance.m_Infos[it].Infos[infoIndex], blob.GetInfos(infoType).Infos.GetElements(), sizeof(CItemInfo *) * infoEntryCount);
				infoIndex += infoEntryCount;
			}
		}
	}

	// And now for the per-blob ones.
	int index = 0;
	ms_Instance.m_SlotBestOrder.WeaponSlots.Reset();
	ms_Instance.m_SlotBestOrder.WeaponSlots.Resize(slotBestOrderCount);

#if __BANK
	ms_Instance.m_AimingInfoNames.Reset();
	ms_Instance.m_AimingInfoNames.Reserve(aimingInfoCount);
#endif // __BANK

	for (int x=0; x<count; x++)
	{
		CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs[x];

		int entryCount = blob.GetSlotBestOrder().WeaponSlots.GetCount();

		if (entryCount)
		{
			sysMemCpy(&ms_Instance.m_SlotBestOrder.WeaponSlots[index], blob.GetSlotBestOrder().WeaponSlots.GetElements(), sizeof(CWeaponInfoBlob::SlotEntry) * entryCount);
			index += entryCount;
		}

#if __BANK
		for(s32 i = 0; i < blob.GetAimingInfoCount(); i++)
		{
			ms_Instance.m_AimingInfoNames.PushAndGrow(blob.GetAimingInfoByIndex(i)->GetName());
		}
#endif // __BANK
	}

	// And now sort the entries.
	std::sort(ms_Instance.m_SlotBestOrder.WeaponSlots.begin(), ms_Instance.m_SlotBestOrder.WeaponSlots.end(), CWeaponInfoBlob::SlotEntry::SortFunc);

	// Look for identical values.
#if __ASSERT
	for (int x=0; x<ms_Instance.m_SlotBestOrder.WeaponSlots.GetCount() - 1; x++)
	{
		Assertf(ms_Instance.m_SlotBestOrder.WeaponSlots[x].m_OrderNumber != ms_Instance.m_SlotBestOrder.WeaponSlots[x+1].m_OrderNumber,
			"%s has the same slot best order number as %s (%d). You should give them unique values so the order is not ambiguous.",
			ms_Instance.m_SlotBestOrder.WeaponSlots[x].m_Entry.GetCStr(), ms_Instance.m_SlotBestOrder.WeaponSlots[x+1].m_Entry.GetCStr(),
			ms_Instance.m_SlotBestOrder.WeaponSlots[x].m_OrderNumber);
	}
#endif // __ASSERT

//	m_InfoNames[i].PushAndGrow(m_Infos[i].Infos[j]->GetName());


#if __BANK
	for(s32 i = 0; i < CWeaponInfoBlob::IT_Max; i++)
	{
		ms_Instance.m_InfoNames[i].Reset();
	}
	for (int x=0; x<count; x++)
	{
		CWeaponInfoBlob &blob = ms_Instance.m_WeaponInfoBlobs[x];

		for(s32 i = 0; i < CWeaponInfoBlob::IT_Max; i++)
		{
			sWeaponInfoList &infos = blob.GetInfos((CWeaponInfoBlob::InfoType) i);
			for(s32 j = 0; j < infos.Infos.GetCount(); j++)
			{
				ms_Instance.m_InfoNames[i].PushAndGrow(infos.Infos[j]->GetName());
			}
		}
	}

	// Set any existing peds to default load outs
	//GiveAllPedsDefaultItems();

	// Init the widgets
	CWeaponDebug::InitBank();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CWeaponInfoManager::Validate()
{
	// Validate the loaded data
	for(s32 i = 0; i < ms_Instance.m_InfoPtrs.Infos.GetCount(); i++)
	{
		ms_Instance.m_InfoPtrs.Infos[i]->Validate();
	}
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeaponInfoBlob::Save()
{
	//SaveExamples();

	weaponVerifyf(PARSER.SaveObject(m_Filename.c_str(), "meta", this, parManager::XML), "Failed to save weapons");
}

void CWeaponInfoBlob::Load()
{
	PARSER.LoadObject(m_Filename.c_str(), "meta", *this);

	// Perform some sanity checking.
	Assertf(m_SlotNavigateOrder.GetCount() == m_SlotNavigateOrder.GetMaxCount(), "Weapon info for '%s' has the incorrect number of slot navigation order entries - should be %d but is %d", m_Filename.c_str(), m_SlotNavigateOrder.GetCount(), m_SlotNavigateOrder.GetMaxCount());

	// Merge all the information in the master info manager
	CWeaponInfoManager::CollateInfoBlobs();
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

const CWeaponInfoBlob::SlotList& CWeaponInfoManager::GetSlotListNavigate(const CPed* UNUSED_PARAM(pPed))
{
	weaponFatalAssertf(ms_Instance.m_SlotNavigateOrder.size() == CWeaponInfoBlob::WS_Max, "Missing weapon data from weapons.meta!");

	CWeaponInfoBlob::WeaponSlots slot = CWeaponInfoBlob::WS_Default;

	// url:bugstar:2557284 - eTEAM_COP isn't used any more, script use teams 0-3 freely with assuming certain team types. Removing below code.

	//If the ped is CnC cop use the WS_CnCCop weapon layout.
	//if(pPed && pPed->IsPlayer() && pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner() && pPed->GetNetworkObject()->GetPlayerOwner()->GetTeam() == eTEAM_COP)
	//{
	//	slot = CWeaponInfoBlob::WS_CnCCop;
	//}

	return ms_Instance.m_SlotNavigateOrder[slot].WeaponSlots;
}

////////////////////////////////////////////////////////////////////////////////

// Access a tint spec value
const CWeaponTintSpecValues* CWeaponInfoManager::GetWeaponTintSpecValue(atHashWithStringNotFinal uNameHash)
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();
	const CWeaponTintSpecValues *result = NULL;

	for (int x=0; x<count; x++)
	{
		result = ms_Instance.m_WeaponInfoBlobs[x].GetWeaponTintSpecValue(uNameHash);

		if (result)
		{
			break;
		}
	}

	return result;
}

// Access a firing pattern alias
const CWeaponFiringPatternAliases* CWeaponInfoManager::GetWeaponFiringPatternAlias(atHashWithStringNotFinal uNameHash)
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();
	const CWeaponFiringPatternAliases *result = NULL;

	for (int x=0; x<count; x++)
	{
		result = ms_Instance.m_WeaponInfoBlobs[x].GetWeaponFiringPatternAlias(uNameHash);

		if (result)
		{
			break;
		}
	}

	return result;
}

// Access a upper body expression data
const CWeaponUpperBodyFixupExpressionData* CWeaponInfoManager::GetWeaponUpperBodyFixupExpressionData(atHashWithStringNotFinal uNameHash)
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();
	const CWeaponUpperBodyFixupExpressionData *result = NULL;

	for (int x=0; x<count; x++)
	{
		result = ms_Instance.m_WeaponInfoBlobs[x].GetWeaponUpperBodyFixupExpressionData(uNameHash);

		if (result)
		{
			break;
		}
	}

	return result;
}

// Access a aiming info
const CAimingInfo* CWeaponInfoManager::GetAimingInfo(atHashWithStringNotFinal uNameHash)
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();
	const CAimingInfo *result = NULL;

	for (int x=0; x<count; x++)
	{
		result = ms_Instance.m_WeaponInfoBlobs[x].GetAimingInfo(uNameHash);

		if (result)
		{
			break;
		}
	}

	return result;
}

// Access a vehicle weapon info
const CVehicleWeaponInfo* CWeaponInfoManager::GetVehicleWeaponInfo(atHashWithStringNotFinal uNameHash)
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();
	const CVehicleWeaponInfo *result = NULL;

	for (int x=0; x<count; x++)
	{
		result = ms_Instance.m_WeaponInfoBlobs[x].GetVehicleWeaponInfo(uNameHash);

		if (result)
		{
			break;
		}
	}

	return result;
}

// Access armoured glass damage
const float CWeaponInfoManager::GetArmouredGlassDamageForWeaponGroup(atHashWithStringNotFinal uGroupHash)
{
	int count = ms_Instance.m_WeaponInfoBlobs.GetCount();
	float fDamage = 10.0f;

	// Try and find damage value for specified group hash.
	for (int x=0; x<count; x++)
	{
		fDamage = ms_Instance.m_WeaponInfoBlobs[x].GetArmouredGlassDamageForWeaponGroup(uGroupHash);

		if (fDamage != -1.0f)
		{
			return fDamage;
		}
	}

	// No damage found for this group, try and find the fallback ("GROUP_UNDEFINED").
	u32 uUndefinedGroupHash = ATSTRINGHASH("GROUP_UNDEFINED", 0xf30af33b);
	for (int x=0; x<count; x++)
	{
		fDamage = ms_Instance.m_WeaponInfoBlobs[x].GetArmouredGlassDamageForWeaponGroup(uUndefinedGroupHash);

		if (fDamage != -1.0f)
		{
			return fDamage;
		}
	}

	// Overall fallback; return 10.0f.
	return fDamage;
}