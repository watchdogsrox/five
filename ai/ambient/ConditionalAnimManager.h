#ifndef INC_CONDITIONAL_ANIM_MANAGER_H
#define INC_CONDITIONAL_ANIM_MANAGER_H

// Rage headers
#include "atl/singleton.h"

// Game headers
#include "ConditionalAnims.h"



////////////////////////////////////////////////////////////////////////////////
// CConditionalAnimManager
////////////////////////////////////////////////////////////////////////////////

class CConditionalAnimManager : public datBase
{
public:

	CConditionalAnimManager();
	~CConditionalAnimManager();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	void ClearDLCEntries();
	const CConditionalAnimsGroup * GetConditionalAnimsGroup(const char * pName) const
	{
		const CConditionalAnimsGroup * pGroup = GetConditionalAnimsGroup(atStringHash(pName));
		aiAssertf(pGroup,"'%s' not defined in conditionalAnims.meta",pName);
		return pGroup;
	}

	const CConditionalAnimsGroup * GetConditionalAnimsGroup(atHashWithStringNotFinal hash) const
	{
		for(s32 i = 0; i < m_ConditionalAnimsGroup.GetCount(); i++)
		{
			if(m_ConditionalAnimsGroup[i]->GetHash() == hash.GetHash())
			{
				return m_ConditionalAnimsGroup[i];
			}
		}
		return NULL;
	}

	int GetNumConditionalAnimsGroups() const
	{
		return m_ConditionalAnimsGroup.GetCount();
	}

	const CConditionalAnimsGroup& GetConditionalAnimsGroupByIndex(int index) const
	{
		return (*m_ConditionalAnimsGroup[index]);
	}

	// Append extra data
	void Append(const char *fname, bool isDlc=false);

	// Remove extra data
	void Remove(const char *fname);

#if __BANK
	// Add widgets
	void AddWidgets(bkBank& bank);
#endif // __BANK
	
private:

	// Delete the data
	void Reset();

	// Load the data
	void Load();

#if __BANK
	// Save the data
	void Save();

	// Reload the data
	void BankLoad();

	// Add widgets
	void AddParserWidgets(bkBank& bank);

	// Clear widgets
	void ClearWidgets();
#endif // __BANK

	//
	// Members
	//

#if __BANK
	atArray<bkBank*> m_CAWidgetBanks;
#endif
	struct sCASourceInfo
	{
		sCASourceInfo(){};
		sCASourceInfo(int _CAStartIndex,int _CACount, const char* _source, bool _isDLC=false)
			:CAStartIndex(_CAStartIndex),CACount(_CACount),source(_source),isDLC(_isDLC){}
		int CAStartIndex;
		int CACount;
		atFinalHashString source;
		bool isDLC;
	};
	atArray<sCASourceInfo> m_dCASourceInfo;
	atArray<CConditionalAnimsGroup*> m_ConditionalAnimsGroup;

	PAR_SIMPLE_PARSABLE;
};

typedef atSingleton<CConditionalAnimManager> CConditionalAnimManagerSingleton;
#define CONDITIONALANIMSMGR CConditionalAnimManagerSingleton::InstanceRef()
#define INIT_CONDITIONALANIMSMGR											\
	do {																\
		if(!CConditionalAnimManagerSingleton::IsInstantiated()) {			\
			CConditionalAnimManagerSingleton::Instantiate();				\
		}																\
	} while(0)															\
	//END
#define SHUTDOWN_CONDITIONALANIMSMGR CConditionalAnimManagerSingleton::Destroy()

////////////////////////////////////////////////////////////////////////////////
// CConditionalAnimStreamingVfxManager
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	Simple manager for assets in ptfxAssetStore, streamed in for use in scenarios and other
	conditional animations.
*/
class CConditionalAnimStreamingVfxManager
{
public:
	static void InitClass() { Assert(!sm_Instance); sm_Instance = rage_new CConditionalAnimStreamingVfxManager; }
	static void ShutdownClass() { delete sm_Instance; sm_Instance = NULL; }
	static CConditionalAnimStreamingVfxManager& GetInstance() { aiAssert(sm_Instance); return *sm_Instance; }

	// PURPOSE:	Clear out the request array, releasing any references to streamable
	//			assets.
	// NOTES:	When this is called, it's expected that any users have already been destroyed
	//			and released their reference counts.
	void Reset();

	// PURPOSE:	Update, handling all streaming requests, etc.
	void Update();

	// PURPOSE:	Request a streamable VFX asset, by hash.
	// RETURNS:	Streaming slot index in ptfxAssetStore, or a negative number if not found.
	int RequestVfxAssetSlot(u32 fxAssetHashName);

	// PURPOSE:	Unrequest a streamable VFX asset, passing in the return value
	//			from a previous call to RequestVfxAssetSlot().
	// NOTES:	This has to be matched perfectly with a call to RequestVfxAssetSlot().
	void UnrequestVfxAssetSlot(int assetStoreSlot);

protected:
	// PURPOSE:	Locate an entry in m_Entries, matching a specific asset slot.
	int FindEntryForSlot(int assetStoreSlot) const;

	// PURPOSE:	Entry keeping track of a streamable asset that has been requested by
	//			one or more users, at some point.
	struct StreamingEntry
	{
		// PURPOSE:	Slot index in ptfxAssetStore.
		s16		m_AssetStoreSlot;

		// PURPOSE:	Number of current users of this asset.
		u16		m_NumUsers:15;

		// PURPOSE:	True if we have streamed in this asset and added our own reference
		//			to it, thus locking it in memory.
		u16		m_RefCountedAsset:1;
	};

	// PURPOSE:	Maximum number of streamable assets we may care about at any given time.
	static const int kMaxStreamingEntries = 8;

	// PURPOSE:	Array to keep track of the current asset needs.
	atFixedArray<StreamingEntry, kMaxStreamingEntries> m_Entries;

	// PURPOSE:	Pointer to the instance of this class.
	static CConditionalAnimStreamingVfxManager*	sm_Instance;
};

#endif // INC_CONDITIONAL_ANIM_MANAGER_H
