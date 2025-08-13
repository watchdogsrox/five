#ifndef PICKUP_DATA_MANAGER_H
#define PICKUP_DATA_MANAGER_H

// Game headers
#include "pickups/Data/ActionData.h"
#include "pickups/Data/PickupData.h"
#include "pickups/Data/PickupIds.h"
#include "pickups/Data/RewardData.h"

//////////////////////////////////////////////////////////////////////////
// CPickupDataManager
//////////////////////////////////////////////////////////////////////////

class CPickupDataManager
{
public:
	// constructor
	CPickupDataManager();

	// Initialise
	static void Init(unsigned initMode);

	// Shutdown
	static void Shutdown(unsigned shutdown);

	void Append(const char* filePath);
	void Remove(const char* filePath);

	static bool GetPickupDataExists(atHashWithStringNotFinal pickupHash)
	{
		for(int i=0;i<ms_instance.m_pickupData.GetCount();i++)
			if (ms_instance.m_pickupData[i]->GetHash() == pickupHash.GetHash())
				return true;

		return false;
	}

	static bool GetPickupActionDataExists(atHashWithStringNotFinal actionHash)
	{
		for(int i=0;i<ms_instance.m_actionData.GetCount();i++)
			if (ms_instance.m_actionData[i]->GetHash() == actionHash.GetHash())
				return true;

		return false;
	}

	static bool GetPickupRewardDataExists(atHashWithStringNotFinal actionHash)
	{
		for(int i=0;i<ms_instance.m_rewardData.GetCount();i++)
			if (ms_instance.m_rewardData[i]->GetHash() == actionHash.GetHash())
				return true;

		return false;
	}

	static const CPickupData* GetPickupData(atHashWithStringNotFinal pickupHash)
	{
		for(int i=0;i<ms_instance.m_pickupData.GetCount();i++)
			if (ms_instance.m_pickupData[i]->GetHash() == pickupHash.GetHash())
				return ms_instance.m_pickupData[i];
		Assertf(0, "Not found pickup [%s][%d]", pickupHash.TryGetCStr(), pickupHash.GetHash());
		return NULL;
	}

	// Access to the action data
	static const CPickupActionData* GetPickupActionData(atHashWithStringNotFinal actionHash)
	{
		for(int i=0;i<ms_instance.m_actionData.GetCount();i++)
			if (ms_instance.m_actionData[i]->GetHash() == actionHash.GetHash())
				return ms_instance.m_actionData[i];
		Assertf(0, "Not found pickup action [%s][%d]", actionHash.TryGetCStr(), actionHash.GetHash());
		return NULL;
	}

	// Access to the reward data
	static const CPickupRewardData* GetPickupRewardData(atHashWithStringNotFinal actionHash)
	{
		for(int i=0;i<ms_instance.m_rewardData.GetCount();i++)
			if (ms_instance.m_rewardData[i]->GetHash() == actionHash.GetHash())
				return ms_instance.m_rewardData[i];
		Assertf(0, "Not found pickup reward [%s][%d]", actionHash.TryGetCStr(), actionHash.GetHash());
		return NULL;
	}

	// Access to the static manager
	static CPickupDataManager& GetInstance() { return ms_instance; }

	static u32 GetNumPickupTypes() 
	{ 
		return ms_instance.m_pickupData.GetCount(); 
	}

	static CPickupData* GetPickupDataInSlot(u32 slot) 
	{ 
		CPickupData* pData = NULL;

		if (AssertVerify(slot < ms_instance.m_pickupData.GetCount()))
		{
			pData = ms_instance.m_pickupData[slot]; 
		}

		return pData;
	}

private:

	static CPickupData* GetPickupDataNonConst(atHashWithStringNotFinal pickupHash)
	{
		for(int i=0;i<ms_instance.m_pickupData.GetCount();i++)
			if (ms_instance.m_pickupData[i]->GetHash() == pickupHash.GetHash())
				return ms_instance.m_pickupData[i];

		return NULL;
	}

	static CPickupActionData* GetPickupActionDataNonConst(atHashWithStringNotFinal actionHash)
	{
		for(int i=0;i<ms_instance.m_actionData.GetCount();i++)
			if (ms_instance.m_actionData[i]->GetHash() == actionHash.GetHash())
				return ms_instance.m_actionData[i];

		return NULL;
	}

	static CPickupRewardData* GetPickupRewardDataNonConst(atHashWithStringNotFinal actionHash)
	{
		for(int i=0;i<ms_instance.m_rewardData.GetCount();i++)
			if (ms_instance.m_rewardData[i]->GetHash() == actionHash.GetHash())
				return ms_instance.m_rewardData[i];

		return NULL;
	}

	//
	// Pargen (xml) support
	bool LoadXmlMeta(const char * const pFilename);
#if __DEV
	void SaveXmlMeta(const char * const pFilename);
#endif // __DEV

	rage::atArray<CPickupData*>			m_pickupData;
	rage::atArray<CPickupActionData*>	m_actionData;
	rage::atArray<CPickupRewardData*>	m_rewardData;

	//
	// Static manager object
	static CPickupDataManager ms_instance;

	PAR_SIMPLE_PARSABLE;
};

#endif // PICKUP_DATA_MANAGER_H
