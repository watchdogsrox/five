// Class header
#include "AI/Ambient/ConditionalAnimManager.h"

// Rage headers
#include "bank/bkmgr.h"
#include "parser/manager.h"

// Game headers
#include "ConditionalAnimManager.h"
#include "Peds/ped.h"
#include "Peds/pedpopulation.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "vfx/ptfx/ptfxmanager.h"

// Parser files
#include "ConditionalAnimManager_parser.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()



class ConditionalAnimationsMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CONDITIONALANIMSMGR.Append(file.m_filename,true);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file) 
	{
		CONDITIONALANIMSMGR.Remove(file.m_filename);
	}

} g_ConditionalAnimationsMounter;

////////////////////////////////////////////////////////////////////////////////
// CConditionalAnimManager
////////////////////////////////////////////////////////////////////////////////

CConditionalAnimManager::CConditionalAnimManager()
{
}

////////////////////////////////////////////////////////////////////////////////

CConditionalAnimManager::~CConditionalAnimManager()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CConditionalAnimManager::Init(unsigned initMode)
{
	switch(initMode)
	{
		case INIT_BEFORE_MAP_LOADED:
			CDataFileMount::RegisterMountInterface(CDataFileMgr::CONDITIONAL_ANIMS_FILE, &g_ConditionalAnimationsMounter);
			INIT_CONDITIONALANIMSMGR;
			CONDITIONALANIMSMGR.Load();
		break;
		case INIT_SESSION:
		default:
		break;
	}

}

void CConditionalAnimManager::Shutdown(unsigned shutdownMode)
{
	switch(shutdownMode)
	{
		case SHUTDOWN_WITH_MAP_LOADED:
			CONDITIONALANIMSMGR.Reset();
			SHUTDOWN_CONDITIONALANIMSMGR;
		break;
		case SHUTDOWN_SESSION:
			CONDITIONALANIMSMGR.ClearDLCEntries();
		default:
		break;
	}
}

void CConditionalAnimManager::ClearDLCEntries()
{
	for(int i = m_dCASourceInfo.GetCount()-1; i >= 0; i--)
	{
		sCASourceInfo& current = m_dCASourceInfo[i];
		if(current.isDLC)
		{
			int toDelete = current.CACount;
			for (int j=0;j<toDelete;j++)
			{
				delete m_ConditionalAnimsGroup[current.CAStartIndex+j];
				m_ConditionalAnimsGroup.Delete(current.CAStartIndex+j--);
				toDelete--;				
			}
			m_dCASourceInfo.Delete(i);
		}	
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CConditionalAnimManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Conditional Anims");
	
	bank.AddButton("Load", datCallback(MFA(CConditionalAnimManager::BankLoad), this));
	bank.AddButton("Save", datCallback(MFA(CConditionalAnimManager::Save), this));
	
	AddParserWidgets(bank);
	
	bank.PopGroup();

	//storing the bkBanks we are part of so if we reload we can properly reset the rag widgets
	Assertf(m_CAWidgetBanks.Find(&bank) == -1, "Adding CConditionalAnimManager widgets to bank [%s] multiple times. Is this intended?", bank.GetTitle());
	m_CAWidgetBanks.PushAndGrow(&bank);
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CConditionalAnimManager::Reset()
{
	for(int i=0;i<m_ConditionalAnimsGroup.GetCount();i++)
	{
		delete m_ConditionalAnimsGroup[i];
	}
	m_ConditionalAnimsGroup.Reset();
	m_dCASourceInfo.Reset();
#if __BANK
	ClearWidgets();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CConditionalAnimManager::Load()
{
#if SCENARIO_DEBUG
	//make sure we abort the current tasks because they can have pointers to the data we are about to delete.
	CScenarioInfoManager::ClearPedUsedScenarios();
	ClearWidgets();
#endif // SCENARIO_DEBUG

	// Delete any existing data
	Reset();
	Append("common:/data/ai/conditionalanims.meta");
}

////////////////////////////////////////////////////////////////////////////////

void CConditionalAnimManager::Append(const char *fname,bool isDlc)
{
	CConditionalAnimManager tempInst;

 	if(AssertVerify(PARSER.LoadObject(fname, NULL, tempInst)))
 	{
 		for(int i=0;i<tempInst.m_ConditionalAnimsGroup.GetCount();++i)
 		{
 			m_ConditionalAnimsGroup.PushAndGrow((tempInst.m_ConditionalAnimsGroup[i]));
			tempInst.m_ConditionalAnimsGroup[i] = NULL;
 		}
		int count = tempInst.m_ConditionalAnimsGroup.GetCount();
		m_dCASourceInfo.PushAndGrow(sCASourceInfo(m_ConditionalAnimsGroup.GetCount()-count,count,fname,isDlc));
#if __BANK	
		ClearWidgets();
		for(int i = 0; i < m_CAWidgetBanks.GetCount(); i++)
		{
			Assert(m_CAWidgetBanks[i]);
			AddParserWidgets(*m_CAWidgetBanks[i]);
		}
#endif //__BANK
	}
}

////////////////////////////////////////////////////////////////////////////////

void CConditionalAnimManager::Remove(const char * /*fname*/)
{
}

////////////////////////////////////////////////////////////////////////////////


#if __BANK
void CConditionalAnimManager::BankLoad()
{
	CPedPopulation::RemoveAllPedsHardNotPlayer();
	atArray<atFinalHashString> dlcSources;

	for(int i=0;i<m_dCASourceInfo.GetCount();i++)
	{
		if(m_dCASourceInfo[i].isDLC)
		{
			dlcSources.PushAndGrow(m_dCASourceInfo[i].source);
		}
	}
	Load();

	for(int i=0;i<dlcSources.GetCount();i++)
	{
		Append(dlcSources[i].TryGetCStr(),true);
	}
}

void CConditionalAnimManager::Save()
{
	int curGroupIndex=0;
	for(int i=0;i<m_dCASourceInfo.GetCount();i++)
	{
		CConditionalAnimManager tempData;
		sCASourceInfo& currentSourceInfo = m_dCASourceInfo[i];
		for(int j=curGroupIndex;j<curGroupIndex+currentSourceInfo.CACount;j++)
		{
			tempData.m_ConditionalAnimsGroup.PushAndGrow(m_ConditionalAnimsGroup[j]);
		}
		curGroupIndex += currentSourceInfo.CACount;
		Verifyf(PARSER.SaveObject(currentSourceInfo.source.TryGetCStr(), "", &tempData, parManager::XML), "File is probably write protected");
 		for(int j=0;j<tempData.m_ConditionalAnimsGroup.GetCount();j++)
 		{
 			tempData.m_ConditionalAnimsGroup[j]=NULL;
 		}
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CConditionalAnimManager::AddParserWidgets(bkBank& myBank)
{
	bkGroup* pGroup = NULL;
	char text[128];
	formatf(text, "%s/Conditional Anims", myBank.GetTitle());
	bkWidget* pWidget = BANKMGR.FindWidget(text);
	if(pWidget)
	{
		pGroup = dynamic_cast<bkGroup*>(pWidget);
		if(pGroup && myBank.GetCurrentGroup() != pGroup)
		{
			myBank.SetCurrentGroup(*pGroup);
		}
		else
		{
			pGroup = NULL;// reset it so we dont try to unset the current group later.
		}
	}

	myBank.PushGroup("Conditional Anim Groups");
	for(s32 i = 0; i < m_ConditionalAnimsGroup.GetCount(); i++)
	{
		myBank.PushGroup(m_ConditionalAnimsGroup[i]->GetName());
		PARSER.AddWidgets(myBank, m_ConditionalAnimsGroup[i]);
		myBank.PopGroup();
	}
	myBank.PopGroup();

	if(pGroup)
	{
		myBank.UnSetCurrentGroup(*pGroup);
	}
}

void CConditionalAnimManager::ClearWidgets()
{
	//For all the widget banks remove all the widgets ... 
	char text[128];
	for (int i = 0; i < m_CAWidgetBanks.GetCount(); i++)
	{
		bkBank* bank = m_CAWidgetBanks[i];
		Assert(bank);
		formatf(text, "%s/Conditional Anims/Conditional Anim Groups", bank->GetTitle());
		bkWidget* widget = BANKMGR.FindWidget(text);
		if (widget)
		{
			Assertf(dynamic_cast<bkGroup*>(widget), "widget [Conditional Anim Groups] is not the expected widget type {bkGroup}.");
			widget->Destroy();
		}
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
// CConditionalAnimStreamingVfxManager
////////////////////////////////////////////////////////////////////////////////

CConditionalAnimStreamingVfxManager* CConditionalAnimStreamingVfxManager::sm_Instance;

void CConditionalAnimStreamingVfxManager::Reset()
{
	ptfxAssetStore& store = ptfxManager::GetAssetStore();

	// Clear out any references or DONTDELETE flags on the assets. It's expected
	// at this point that any peds, and their tasks, which are the users of this
	// stuff, would have already been destroyed.
	int numRequests = m_Entries.GetCount();
	for(int i = 0; i < numRequests; i++)
	{
		StreamingEntry& r = m_Entries[i];

		aiAssertf(!r.m_NumUsers,
				"Shutting down scenario VFX streaming, but there are still %d references (and %d peds in use).",
				r.m_NumUsers, CPed::GetPool() ? CPed::GetPool()->GetNoOfUsedSpaces() : 0);

		const strLocalIndex slot = strLocalIndex(r.m_AssetStoreSlot);
		if(r.m_RefCountedAsset)
		{
			store.RemoveRef(slot, REF_OTHER);
		}
		else
		{
			store.ClearRequiredFlag(slot.Get(), STRFLAG_DONTDELETE);
		}
	}

	m_Entries.Reset();
}


void CConditionalAnimStreamingVfxManager::Update()
{
	ptfxAssetStore& store = ptfxManager::GetAssetStore();

	// Go through the entries and make streaming requests, etc.
	int numRequests = m_Entries.GetCount();
	for(int i = 0; i < numRequests; i++)
	{
		bool removeEntry = false;

		StreamingEntry& r = m_Entries[i];
		if(r.m_RefCountedAsset)
		{
			aiAssert(store.HasObjectLoaded(strLocalIndex(r.m_AssetStoreSlot)));

			if(!r.m_NumUsers)
			{
				const int slot = r.m_AssetStoreSlot;
				store.RemoveRef(strLocalIndex(slot), REF_OTHER);

				removeEntry = true;
			}
		}
		else
		{
			const strLocalIndex slot = strLocalIndex(r.m_AssetStoreSlot);
			if(store.HasObjectLoaded(slot))
			{
				store.AddRef(slot, REF_OTHER);
				store.ClearRequiredFlag(slot.Get(), STRFLAG_DONTDELETE);
				r.m_RefCountedAsset = true;
			}
			// May want to do something like this here, deleting a pending request if no longer
			// desired. But, that's probably the most fragile state where something might go wrong
			// - could potentially remove some other system's request to the same resource, for example.
			//		else if(!r.m_NumUsers)
			//		{
			//			if(!strStreamingEngine::GetInfo().IsObjectInUse(store.GetStreamingIndex(slot)))
			//			{
			//				store.ClearRequiredFlag(r.m_AssetStoreSlot, STRFLAG_DONTDELETE);
			//				store.StreamingRemove(slot);
			//				removeEntry = true;
			//			}
			//		}
			else if(!store.IsObjectRequested(slot) && !store.IsObjectLoading(slot))
			{
				if(r.m_NumUsers)
				{
					store.StreamingRequest(slot, STRFLAG_DONTDELETE);
				}
				else
				{
					removeEntry = true;
				}
			}
		}

		if(removeEntry)
		{
			m_Entries.DeleteFast(i--);
			numRequests--;
			continue;
		}
	}
}


int CConditionalAnimStreamingVfxManager::RequestVfxAssetSlot(u32 fxAssetHashName)
{
	int slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(fxAssetHashName).Get();
	if(slot >= 0)
	{
		int requestIndex = FindEntryForSlot(slot);
		if(requestIndex < 0)
		{
			if(aiVerifyf(!m_Entries.IsFull(), "Too many VFX streaming requests."))
			{
				requestIndex = m_Entries.GetCount();
				StreamingEntry& r = m_Entries.Append();
				aiAssert(slot <= 0x7fff);
				r.m_AssetStoreSlot = (s16)slot;
				r.m_NumUsers = 0;
				r.m_RefCountedAsset = false;
			}
		}

		if(requestIndex >= 0)
		{
			aiAssert(m_Entries[requestIndex].m_NumUsers < 200);
			m_Entries[requestIndex].m_NumUsers++;
			aiAssert(m_Entries[requestIndex].m_NumUsers > 0);	// Check for overflow.
		}
		else
		{
			slot = -1;
		}
	}
	return slot;
}


void CConditionalAnimStreamingVfxManager::UnrequestVfxAssetSlot(int assetStoreSlot)
{
	if(assetStoreSlot >= 0)
	{
		const int entryIndex = FindEntryForSlot(assetStoreSlot);
		if(aiVerifyf(entryIndex >= 0, "Failed to find expected request for slot %d.", assetStoreSlot))
		{
			StreamingEntry& r = m_Entries[entryIndex];
			aiAssert(r.m_NumUsers > 0);
			r.m_NumUsers--;;
		}
	}
}


int CConditionalAnimStreamingVfxManager::FindEntryForSlot(int assetStoreSlot) const
{
	const int num = m_Entries.GetCount();
	for(int i = 0; i < num; i++)
	{
		if(m_Entries[i].m_AssetStoreSlot == assetStoreSlot)
		{
			return i;
		}
	}
	return -1;
}
