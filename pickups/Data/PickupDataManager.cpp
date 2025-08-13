// Class header
#include "pickups/Data/PickupDataManager.h"

// Rage Headers
#include "parser/manager.h"

// Game headers
#include "core/game.h"
#include "pickups/PickupActions.h"
#include "pickups/PickupRewards.h"
#include "pickups/PickupManager.h"
#include "pickups/data/PickupDataManager_parser.h"
#include "scene/dlc_channel.h"

WEAPON_OPTIMISATIONS()

class CPickupDataManagerMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CPickupDataManagerMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		switch(file.m_fileType)
		{
			case CDataFileMgr::DLC_WEAPON_PICKUPS: CPickupDataManager::GetInstance().Append(file.m_filename); break;
			default: return false;
		}

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		dlcDebugf3("CPickupDataManagerMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);

		switch(file.m_fileType)
		{
			case CDataFileMgr::DLC_WEAPON_PICKUPS: CPickupDataManager::GetInstance().Remove(file.m_filename); break;
			default: break;
		}
	}
} g_weaponPickupsMounter;

//////////////////////////////////////////////////////////////////////////
// CPickupDataManager
//////////////////////////////////////////////////////////////////////////

// Static initialisation
CPickupDataManager CPickupDataManager::ms_instance;

CPickupDataManager::CPickupDataManager()
{
}

void CPickupDataManager::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
		CDataFileMount::RegisterMountInterface(CDataFileMgr::DLC_WEAPON_PICKUPS, &g_weaponPickupsMounter);

		RAGE_TRACK(CPickupDataManager);
		CPickupData::InitPool( MEMBUCKET_GAMEPLAY );

		// This is defined in CPickupData::MAX_STORAGE (pickupdata.h), bump this value
#if !__NO_OUTPUT
		if(CPickupData::GetPool())
		{
			CPickupData::GetPool()->SetSizeIsFromConfigFile(false);
		}
#endif //!__NO_OUTPUT

		Verifyf(ms_instance.LoadXmlMeta("common:/data/pickups"), "CPickupDataManager::Init - Load failed.");

		CPickupManager::UpdatePickupTypes();
    }
}

void CPickupDataManager::Shutdown(unsigned /*shutdownMode*/)
{
	ms_instance.m_actionData.Reset();
	ms_instance.m_rewardData.Reset();
	ms_instance.m_pickupData.Reset();

	// Shutdown pools
	CPickupData::ShutdownPool();
}

//
// Xml ParGen based version
//

void CPickupDataManager::Append(const char* filePath)
{
	CPickupDataManager tempMgr = CPickupDataManager();

	if (PARSER.LoadObject(filePath, "meta", tempMgr))
	{
		for (u32 i = 0; i < tempMgr.m_pickupData.GetCount(); i++)
		{
			if (!ms_instance.GetPickupDataExists(tempMgr.m_pickupData[i]->GetHash()))
				ms_instance.m_pickupData.PushAndGrow(tempMgr.m_pickupData[i]);
			else
				Assertf(0, "CPickupDataManager::Append - Duplicate pick up data added! %s", tempMgr.m_pickupData[i]->GetName());
		}

		for (u32 i = 0; i < tempMgr.m_actionData.GetCount(); i++)
		{
			if (!ms_instance.GetPickupActionDataExists(tempMgr.m_actionData[i]->GetHash()))
				ms_instance.m_actionData.PushAndGrow(tempMgr.m_actionData[i]);
			else
				Assertf(0, "CPickupDataManager::Append - Duplicate pick up action data added! %s", tempMgr.m_actionData[i]->GetName());
		}

		for (u32 i = 0; i < tempMgr.m_rewardData.GetCount(); i++)
		{
			if (!ms_instance.GetPickupRewardDataExists(tempMgr.m_rewardData[i]->GetHash()))
				ms_instance.m_rewardData.PushAndGrow(tempMgr.m_rewardData[i]);
			else
				Assertf(0, "CPickupDataManager::Append - Duplicate pick up reward data added! %s", tempMgr.m_rewardData[i]->GetName());
		}
	}

	CPickupManager::UpdatePickupTypes();
}

void CPickupDataManager::Remove(const char* filePath)
{
	CPickupDataManager tempMgr = CPickupDataManager();

	if (PARSER.LoadObject(filePath, "meta", tempMgr))
	{
		CPickupData* currPickupData = NULL;
		CPickupActionData* currActionData = NULL;
		CPickupRewardData* currRewardData = NULL;

		for (u32 i = 0; i < tempMgr.m_pickupData.GetCount(); i++)
		{
			currPickupData = ms_instance.GetPickupDataNonConst(tempMgr.m_pickupData[i]->GetHash());
			
			if (currPickupData)
				ms_instance.m_pickupData.DeleteMatches(currPickupData);
			else
				Assertf(0, "CPickupDataManager::Remove - Unable to find pickup data! %s", tempMgr.m_pickupData[i]->GetName());
		}

		for (u32 i = 0; i < tempMgr.m_actionData.GetCount(); i++)
		{
			currActionData = ms_instance.GetPickupActionDataNonConst(tempMgr.m_actionData[i]->GetHash());
			
			if (currActionData)
				ms_instance.m_actionData.DeleteMatches(currActionData);
			else
				Assertf(0, "CPickupDataManager::Remove - Unable to find pickup action data! %s", tempMgr.m_actionData[i]->GetName());
		}

		for (u32 i = 0; i < tempMgr.m_rewardData.GetCount(); i++)
		{
			currRewardData = ms_instance.GetPickupRewardDataNonConst(tempMgr.m_rewardData[i]->GetHash());
			
			if (currRewardData)
				ms_instance.m_rewardData.DeleteMatches(currRewardData);
			else
				Assertf(0, "CPickupDataManager::Remove - Unable to find pickup reward data! %s", tempMgr.m_rewardData[i]->GetName());
		}
	}

	CPickupManager::UpdatePickupTypes();
}

bool CPickupDataManager::LoadXmlMeta(const char * const pFilename)
{
	return PARSER.LoadObject(pFilename, "meta", *this);
}

#if __DEV

void CPickupDataManager::SaveXmlMeta(const char * const filename)
{
	PARSER.SaveObject(filename, "meta", this);
}

#endif // __DEV

