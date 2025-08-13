//
// weapons/weaponcomponentmanager.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentManager.h"

// Rage headers
#include "bank/bank.h"
#include "parser/manager.h"
#include "parser/restparserservices.h"

// Game headers
#include "Scene/DataFileMgr.h"
#include "Weapons/Components/WeaponComponentData.h"
#include "Weapons/WeaponDebug.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Static initialisation
CWeaponComponentManager CWeaponComponentManager::ms_Instance;


class CWeaponComponentDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CWeaponComponentManager::Load(file.m_filename);
		CWeaponComponentManager::PostLoad();
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & ) {}

} g_WeaponComponentDataFileMounter;


////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeaponComponentInfoBlob::Save()
{
	weaponVerifyf(PARSER.SaveObject(m_Filename.c_str(), "meta", this, parManager::XML), "Failed to save weapon components at %s.meta", m_Filename.c_str());
}

static void SaveBlob(void *blob)
{
	((CWeaponComponentInfoBlob *) blob)->Save();
}
#endif // __BANK




////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::Init()
{
	// Initialise pools
	CWeaponComponentInfo::InitPool(MEMBUCKET_GAMEPLAY);
	CWeaponComponent::InitPool(MEMBUCKET_GAMEPLAY);

	// This is defined in CWeaponComponentInfo::MAX_STORAGE (weaponcomponent.h / weaponcomponentparser.psc), bump those two files
#if !__NO_OUTPUT
	if(CWeaponComponentInfo::GetPool())
	{
		CWeaponComponentInfo::GetPool()->SetSizeIsFromConfigFile(false);
	}
#endif //!__NO_OUTPUT


	CDataFileMount::RegisterMountInterface(CDataFileMgr::WEAPONCOMPONENTSINFO_FILE, &g_WeaponComponentDataFileMounter);

	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::Shutdown()
{
	// Delete any existing data
	Reset();

	// Shutdown pools
	CWeaponComponent::ShutdownPool();
	CWeaponComponentInfo::ShutdownPool();
}

////////////////////////////////////////////////////////////////////////////////

const char* CWeaponComponentManager::GetNameFromData(const CWeaponComponentData* NOTFINAL_ONLY(pData))
{
#if !__FINAL
	if(pData)
	{
		return pData->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponComponentData* CWeaponComponentManager::GetDataFromName(const char* name)
{
	u32 uNameHash = atStringHash(name);

	for (int x=0; x<ms_Instance.m_InfoBlobs.GetCount(); x++)
	{
		CWeaponComponentInfoBlob *blob = ms_Instance.m_InfoBlobs[x];
		int dataCount = blob->m_Data.GetCount();

		for(s32 i = 0; i < dataCount; i++)
		{
			if(blob->m_Data[i]->GetHash() == uNameHash)
			{
				return blob->m_Data[i];
			}
		}
	}
	weaponAssertf(0, "Data [%s] doesn't exist in data", name);
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const char* CWeaponComponentManager::GetNameFromInfo(const CWeaponComponentInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponComponentInfo* CWeaponComponentManager::GetInfoFromName(const char* name)
{
	u32 uNameHash = atStringHash(name);

	for (int x=0; x<ms_Instance.m_InfoBlobs.GetCount(); x++)
	{
		CWeaponComponentInfoBlob *blob = ms_Instance.m_InfoBlobs[x];
		int infoCount = blob->m_Infos.GetCount();

		for(s32 i = 0; i < infoCount; i++)
		{
			if(blob->m_Infos[i]->GetHash() == uNameHash)
			{
				return blob->m_Infos[i];
			}
		}
	}
	weaponAssertf(0, "Info [%s] doesn't exist in data", name);
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

atString CWeaponComponentManager::GetWidgetGroupName()
{
	return atString("Components");
}

void CWeaponComponentManager::AddWidgetButton(bkBank& bank)
{
	bank.PushGroup(GetWidgetGroupName().c_str());

	atString metadataWidgetCreateLabel("");
	metadataWidgetCreateLabel += "Create ";
	metadataWidgetCreateLabel += GetWidgetGroupName();
	metadataWidgetCreateLabel += " widgets";

	bank.AddButton(metadataWidgetCreateLabel.c_str(), datCallback(CFA1(CWeaponComponentManager::AddWidgets), &bank));

	bank.PopGroup();
}

void CWeaponComponentManager::AddWidgets(bkBank& bank)
{
	// Destroy stub group before creating new one
	if (bkWidget* pWidgetGroup = bank.FindChild(GetWidgetGroupName().c_str()))
	{
		pWidgetGroup->Destroy();
	}

	bank.PushGroup(GetWidgetGroupName().c_str());

	// Add info widgets
	bank.PushGroup("Metadata");
	for (int x=0; x<ms_Instance.m_InfoBlobs.GetCount(); x++)
	{
		CWeaponComponentInfoBlob *blob = ms_Instance.m_InfoBlobs[x];

		bank.PushGroup(blob->m_InfoBlobName.c_str());
		blob->AddWidgets(bank);

		// Load/Save
		bank.AddButton("Load", datCallback(LoadBlob, blob, false));
		bank.AddButton("Save", datCallback(SaveBlob, blob, false));

		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PopGroup();
}

void CWeaponComponentInfoBlob::AddWidgets(bkBank& bank)
{
	for (s32 i = 0; i < m_Infos.GetCount(); i++)
	{
		bank.PushGroup(m_Infos[i]->GetName());
		PARSER.AddWidgets(bank, m_Infos[i]);
		bank.PopGroup();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::LoadBlob(void *blob)
{
	CWeaponComponentInfoBlob *blobPtr = (CWeaponComponentInfoBlob *) blob;
	blobPtr->Reset();
	PARSER.LoadObject(blobPtr->m_Filename.c_str(), "meta", *blobPtr);

//	parRestRegisterSingleton("Weapons/WeaponComponents", *blobPtr, ""); // Make this object browsable / editable via rest services (i.e. the web)
	PostLoad();
}

#endif

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentInfoBlob::Reset()
{
	for(s32 i = 0; i < m_Infos.GetCount(); i++)
	{
		delete m_Infos[i];
	}

	for(s32 i = 0; i < m_Data.GetCount(); i++)
	{
		delete m_Data[i];
	}
	
	m_Infos.Reset();
	m_Data.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::Reset()
{
	// Delete the infos
	for (int x=0; x<ms_Instance.m_InfoBlobs.GetCount(); x++)
	{
		ms_Instance.m_InfoBlobs[x]->Reset();
		delete ms_Instance.m_InfoBlobs[x];
	}

	ms_Instance.m_InfoBlobs.Reset();

#if __BANK
	// Empty the names list
	ms_Instance.m_InfoNames.Reset();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::Load(const char *file)
{
	CWeaponComponentInfoBlob *blob = rage_new CWeaponComponentInfoBlob();

	// Load
	ms_Instance.m_InfoBlobs.Grow() = blob;		// We need to add it before we load it so GetDataFromName() and its friends work

	PARSER.LoadObject(file, "meta", *blob);

#if __BANK
	blob->m_Filename = file;
#endif // __BANK

	parRestRegisterSingleton("Weapons/WeaponComponents", *blob, ""); // Make this object browsable / editable via rest services (i.e. the web)
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::Load()
{
	// Delete any existing data
	Reset();

	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WEAPONCOMPONENTSINFO_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			Load(pData->m_filename);
		}

		// Get next file
		pData = DATAFILEMGR.GetNextFile(pData);
	}

	PostLoad();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::PostLoad()
{
	// Update the ptr array for quicker searching
	UpdateInfoPtrs();

#if __BANK
	ms_Instance.m_InfoNames.Resize(0);

	// Construct the set of names
	for (int x=0; x<ms_Instance.m_InfoBlobs.GetCount(); x++)
	{
		CWeaponComponentInfoBlob *blob = ms_Instance.m_InfoBlobs[x];

		for(s32 i = 0; i < blob->m_Infos.GetCount(); i++)
		{
			ms_Instance.m_InfoNames.Push(blob->m_Infos[i]->GetName());
		}
	}

	// Init the widgets
	CWeaponDebug::InitBank();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentManager::UpdateInfoPtrs()
{
	ms_Instance.m_InfoPtrs.Reset();

	// Create a binary searchable list of infos
	for (int x=0; x<ms_Instance.m_InfoBlobs.GetCount(); x++)
	{
		CWeaponComponentInfoBlob *blob = ms_Instance.m_InfoBlobs[x];

		for(s32 i = 0; i < blob->m_Infos.GetCount(); i++)
		{
			ms_Instance.m_InfoPtrs.Push(blob->m_Infos[i]);
		}
	}

	ms_Instance.m_InfoPtrs.QSort(0, ms_Instance.m_InfoPtrs.GetCount(), ComponentSort);
}

////////////////////////////////////////////////////////////////////////////////
