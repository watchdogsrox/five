/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/ipl/IplCullBox.cpp
// PURPOSE : management of artist-placed aabbs to prevent specified map data files from loading
// AUTHOR :  Ian Kiigan
// CREATED : 31/03/2011
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/ipl/IplCullBox.h"
#include "grcore/debugdraw.h"
#include "parser/manager.h"
#include "IplCullBox_parser.h"
#include "file/default_paths.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/psostore.h"
#include "fwsys/fileExts.h"
#include "vector/colors.h"
#include "system/param.h"
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"

SCENE_OPTIMISATIONS();

PARAM(newcullbox, "[scene] new cull box behaviour");

#define CULLBOX_FILE "platform:/data/mapdatacullboxes"

CIplCullBoxFile CIplCullBox::ms_boxList;
atString CIplCullBox::m_dlcFilePath;
atArray<CIplCullBoxContainer> CIplCullBox::ms_containerList;
bool CIplCullBox::ms_bCullEverything = false;
bool CIplCullBox::ms_bLoaded = false;

#if __BANK
#include "camera/CamInterface.h"

bool CIplCullBox::ms_bCheckForLeaks = false;
bool CIplCullBox::ms_bTestAgainstCameraPos = false;
#endif	//__BANK
bool CIplCullBox::ms_bEnabled = true;

class CIplCullboxFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		dlcDebugf3("CIplCullboxFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		CIplCullBox::LoadDLCFile(file.m_filename);

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file) 
	{
		dlcDebugf3("CIplCullboxFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);

		CIplCullBox::UnloadDLCFile(file.m_filename);
	}

} g_IplCullboxFileMounter;

void CIplCullBox::Initialise(unsigned)
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::IPLCULLBOX_METAFILE, &g_IplCullboxFileMounter);

	ms_bLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:		reset box list, flush cull list, flush container list
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::Reset()
{
	ResetCulled();
	ms_boxList.Reset();
	ms_containerList.Reset();
	m_dlcFilePath.Reset();
	ms_bLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddNewBox
// PURPOSE:		adds new cull box to list
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::AddNewBox(CIplCullBoxEntry& newBox)
{
	Assertf(newBox.m_aabb.IsValid(), "Invalid cull box");
	ms_boxList.Add(newBox);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ResetCulled
// PURPOSE:		clear any cull flags for currently culled list
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::ResetCulled()
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& entry = ms_boxList.Get(i);
		if (entry.m_bActive)
		{
			entry.SetActive(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		update which IPLs are to be culled based on player position
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::Update(const Vector3& vPlayerPos)
{
#if __BANK
	if (ms_bCullEverything)
	{
		grcDebugDraw::AddDebugOutput(Color_red, "!! Entire map has been culled by script !!");
	}
#endif	//__BANK

	if (ms_bCullEverything) { return; }

	Vector3 vTestPos = vPlayerPos;

	if (!ms_bEnabled) { return; }
#if __BANK
	if (ms_bTestAgainstCameraPos) { vTestPos = camInterface::GetPos(); }
	if (ms_bCheckForLeaks)
	{
		CheckForLeaks();
	}
#endif	//__BANK

	// check box list for any changes
	atArray<s32> clearList;
	atArray<s32> setList;
	atArray<s32> rqdList;

	for (s32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& entry = ms_boxList.Get(i);
		bool bContainsPlayer = entry.m_aabb.ContainsPoint(RCC_VEC3V(vTestPos));

		//////////////////////////////////////////////////////////////////////////
		// add to required list
		if (entry.m_bEnabled && bContainsPlayer)
		{
			rqdList.PushAndGrow(i);
		}

		//////////////////////////////////////////////////////////////////////////
		// check for transitions from required to unrequired and vice versa
		if (entry.m_bActive)
		{
			if (!entry.m_bEnabled || !bContainsPlayer)
			{
				entry.m_bActive = false;
				clearList.PushAndGrow(i);
			}
		}
		else
		{
			if (entry.m_bEnabled && bContainsPlayer)
			{
				entry.m_bActive = true;
				setList.PushAndGrow(i);
			}
		}
		//////////////////////////////////////////////////////////////////////////
	}

	const bool bHaveDisabledBoxes = (clearList.GetCount() > 0);

	//
	// clear cull flags for boxes which are no longer required
	for (s32 i=0; i<clearList.size(); i++)
	{
		CIplCullBoxEntry& entry = ms_boxList.Get(clearList[i]);
		entry.SetActive(false);
	}

	if (bHaveDisabledBoxes)
	{
		//
		// re-apply cull flags for all required boxes
		for (s32 i=0; i<rqdList.size(); i++)
		{
			CIplCullBoxEntry& entry = ms_boxList.Get(rqdList[i]);
			entry.SetActive(true);
		}
	}
	else
	{
		//
		// apply cull flags only for newly-required boxes
		for (s32 i=0; i<setList.size(); i++)
		{
			CIplCullBoxEntry& entry = ms_boxList.Get(setList[i]);
			entry.SetActive(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegisterContainers
// PURPOSE:		registers all mapdata by name and slot
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::RegisterContainers()
{
#if __BANK
	ms_containerList.Reset();
	// array of containers only used for Rag cull box editor widgets

	for (s32 i=0; i<INSTANCE_STORE.GetCount(); i++)
	{
		const fwMapDataDef* pDef = INSTANCE_STORE.GetSlot(strLocalIndex(i));
		if (pDef && pDef->GetIsValid() && !pDef->GetIsScriptManaged())
		{
			Assert(pDef->GetInitState()==fwMapDataDef::FULLY_INITIALISED);

			if (  ((pDef->GetIsParent() && (pDef->GetContentFlags()&fwMapData::CONTENTFLAG_MASK_ENTITIES)!=0))
				|| (pDef->GetContentFlags()&fwMapData::CONTENTFLAG_INSTANCE_LIST)!=0 )
			{
				// typically map artists refer to sections by their "container name" - so just take names from IMAP files
				// containing low-detail model
				RegisterContainerName(pDef->m_name.GetCStr(), i);
			}
		}
	}

#endif	//__BANK
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegisterContainerName
// PURPOSE:		register w/ temporary mapping fn from container base name to slot index
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::RegisterContainerName(const char* pszContainerName, s32 iplSlotIndex)
{
	CIplCullBoxContainer newContainer;
	newContainer.m_name.SetFromString(pszContainerName);
	newContainer.m_iplSlotIndex = iplSlotIndex;
	ms_containerList.PushAndGrow(newContainer);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateCullList
// PURPOSE:		turns array of container hashes and generates IPL cull list
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEntry::GenerateCullList()
{
	m_cullList.Init( g_MapDataStore.GetCount() );

	INSTANCE_STORE.CreateChildrenCache();
	for (u32 i=0; i<m_culledContainerHashes.size(); i++)
	{
		// for each container, gets its IPL slot and set the bit for that index
		// (and any dependent children IPLs, recursively)
		s32 iplSlot = g_MapDataStore.FindSlotFromHashKey( m_culledContainerHashes[i] ).Get();
		RecursivelyAddCulledDeps(iplSlot);
	}
	INSTANCE_STORE.DestroyChildrenCache();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RecursivelyAddCulledDeps
// PURPOSE:		adds specified ipl to cull list, and recursively add any dependent slots
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEntry::RecursivelyAddCulledDeps(s32 iplSlot)
{
	if (iplSlot != -1)
	{
		m_cullList.MarkIplSlot(iplSlot);

		atArray<s32> children;
		INSTANCE_STORE.GetChildren(strLocalIndex(iplSlot), children);
		for (u32 i=0; i<children.size(); i++)
		{
			if (children[i] != -1)
			{
				RecursivelyAddCulledDeps(children[i]);
			}
		}
	}
}

void CIplCullBox::LoadDLCFile(const char* filePath)
{
	m_dlcFilePath = filePath;

	// Already loaded a file, reload now
	if (ms_bLoaded)
		ReadFile(m_dlcFilePath.c_str(), NULL);
}

void CIplCullBox::UnloadDLCFile(const char* /*filePath*/)
{
	m_dlcFilePath.Reset();

	// Already loaded a file, reload now
	if (ms_bLoaded)
		ReadFile(CULLBOX_FILE, META_FILE_EXT);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Load
// PURPOSE:		loads xml file containing box list
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::Load()
{
	if (m_dlcFilePath.length() > 0)
	{
		ReadFile(m_dlcFilePath.c_str(), NULL);
	}
	else
	{
		ReadFile(CULLBOX_FILE, META_FILE_EXT);
	}
}

void CIplCullBox::ReadFile(const char* filePath, const char* extension)
{
	INSTANCE_STORE.CreateChildrenCache();

	ResetCulled();
	ms_boxList.Reset();

	char fullFileName[RAGE_MAX_PATH] = { 0 };
	ASSET.AddExtension(fullFileName, RAGE_MAX_PATH, filePath, extension);

	fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CIplCullBoxFile>();
	fwPsoStoreLoadInstance inst(&ms_boxList);
	loader.Load(fullFileName, inst);

	for (u32 i = 0; i < ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& fileEntry = ms_boxList.Get(i);
		fileEntry.GenerateCullList();
	}

	ms_bLoaded = true;

#if !__BANK
	INSTANCE_STORE.DestroyChildrenCache();	// used by cull box editor
#endif
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Save
// PURPOSE:		writes out box list as XML format
//////////////////////////////////////////////////////////////////////////
#if __BANK
void CIplCullBox::Save()
{
	Verifyf(PARSER.SaveObject(RS_ASSETS "/titleupdate/dev_ng/data/mapdatacullboxes.pso.meta", "", &ms_boxList, parManager::XML), "Failed to save IPL cull box data");
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetBoxEnabled
// PURPOSE:		used by script commands to enable / disable cull box by name
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::SetBoxEnabled(u32 cullBoxNameHash, bool bEnabled)
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& cullBox = ms_boxList.Get(i);
		if (cullBox.m_name.GetHash()==cullBoxNameHash && cullBox.m_bEnabled!=bEnabled)
		{
			cullBox.m_bEnabled = bEnabled;
			return;
		}
	}
}


bool CIplCullBox::IsBoxEnabled(u32 cullBoxNameHash)
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& cullBox = ms_boxList.Get(i);
		if (cullBox.m_name.GetHash()==cullBoxNameHash)
		{
			return cullBox.m_bEnabled;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateCullListForBox
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::GenerateCullListForBox(u32 cullBoxNameHash)
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& cullBox = ms_boxList.Get(i);
		if (cullBox.m_name.GetHash()==cullBoxNameHash)
		{
			INSTANCE_STORE.CreateChildrenCache();
			cullBox.GenerateCullList();
			INSTANCE_STORE.DestroyChildrenCache();

			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegenerateAllCullBoxLists
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::RegenerateAllCullBoxLists()
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& cullBox = ms_boxList.Get(i);
		cullBox.GenerateCullList();
	}
}


CIplCullBoxEntry const* CIplCullBox::GetCullboxEntry(u32 cullBoxNameHash)
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& cullBox = ms_boxList.Get(i);
		if (cullBox.m_name.GetHash()==cullBoxNameHash)
			return &cullBox;
	}

	return nullptr;
}


void CIplCullBox::RemoveCulledAtPosition(Vec3V_In vPos, atArray<u32>& slotList)
{
	atArray<CIplCullBoxEntry*> boxList(0, 16);
	for (s32 i = 0; i < ms_boxList.Size(); ++i)
	{
		CIplCullBoxEntry& cullBox = ms_boxList.Get(i);
		if (cullBox.m_aabb.ContainsPoint(vPos) && cullBox.m_bEnabled)
		{
			boxList.Grow() = &cullBox;
		}
	}

	for (s32 i = slotList.GetCount() - 1; i >= 0; --i)
	{
		u32 mapDataSlot = slotList[i];
		for (u32 j = 0; j < boxList.GetCount(); ++j)
		{
			CIplCullBoxEntry* cullBox = boxList[j];
			if (cullBox->m_cullList.IsMapDataSlotMarked(mapDataSlot))
			{
				slotList.DeleteFast(i);
				break;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsCullingContainer
// PURPOSE:		returns true if entry has specified container in cull list
//////////////////////////////////////////////////////////////////////////
bool CIplCullBoxEntry::IsCullingContainer(u32 containerHash)
{
	for(s32 i=0; i<m_culledContainerHashes.size(); i++)
	{
		if (m_culledContainerHashes[i] == containerHash) { return true; }
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddContainer
// PURPOSE:		add container name hash to linked list, debug only
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEntry::AddContainer(u32 containerHash)
{
	if (!IsCullingContainer(containerHash))
	{
		m_culledContainerHashes.PushAndGrow(containerHash);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RemoveContainer
// PURPOSE:		remove container name hash from linked list
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEntry::RemoveContainer(u32 containerHash)
{
	for (u32 i=0; i<m_culledContainerHashes.size(); i++)
	{
		if (m_culledContainerHashes[i] == containerHash)
		{
			m_culledContainerHashes.Delete(i);
			break;
		}
	}
	Assert(!IsCullingContainer(containerHash));
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:		clears list of ipl slots to be culled
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxCullList::Init(s32 numIplSlots)
{
	const s32 numCullListEntries = ( numIplSlots / BITS_PER_CULL_SLOT ) + 1;
	m_bitSet.ResizeGrow( numCullListEntries );

	// zero all bits
	for (s32 i=0; i<m_bitSet.GetCount(); i++)
	{
		m_bitSet[i] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetActive
// PURPOSE:		use cull list bits to set cull flags in box streamer entries
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxCullList::SetActive(bool bActive)
{
	fwBoxStreamerVariable& boxStreamer = g_MapDataStore.GetBoxStreamer();

	for (s32 i=0; i<m_bitSet.GetCount(); i++)
	{
		if (m_bitSet[i])
		{
			for (s32 j=0; j<BITS_PER_CULL_SLOT; j++)
			{
				if ( m_bitSet[i] & (1 << j) )
				{

					if (!PARAM_newcullbox.Get())
					{
						// cull by streaming
						boxStreamer.SetIsCulled( GetIplSlot(i, j), bActive );
					}
					else
					{
						// cull by visibility
						fwMapDataDef* pDef = g_MapDataStore.GetSlot( strLocalIndex(GetIplSlot(i, j)) );
						if (pDef)
						{
							pDef->SetIsCulled(bActive);
						}
					}

					
				}
			}
		}
	}
}

#if __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetSlotList
// PURPOSE:		build up array of slots culled by this list
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxCullList::GetSlotList(atArray<s32>& iplSlotList)
{
	for (s32 i=0; i<m_bitSet.GetCount(); i++)
	{
		if (m_bitSet[i])
		{
			for (s32 j=0; j<BITS_PER_CULL_SLOT; j++)
			{
				if (m_bitSet[i] & (1 << j))
				{
					iplSlotList.Grow() = GetIplSlot(i, j);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CheckForLeaks
// PURPOSE:		simple sanity check for debug purposes. clear all currently
//				active cull boxes, and see if there are any IPLs still culled!
//////////////////////////////////////////////////////////////////////////
void CIplCullBox::CheckForLeaks()
{
	for (u32 i=0; i<ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& entry = ms_boxList.Get(i);
		if (entry.m_bActive && entry.m_bEnabled)
		{
			entry.SetActive(false);
		}
	}

#if __ASSERT
	fwBoxStreamerVariable& streamer = INSTANCE_STORE.GetBoxStreamer();
	for (u32 i=0; i<INSTANCE_STORE.GetCount(); i++)
	{
		Assertf(
			!streamer.GetIsCulled(i),
			"Cull box leak - IPL %s is being culled despite not being present in any active cull box",
			INSTANCE_STORE.GetSlot(strLocalIndex(i))->m_name.GetCStr()			
		);
	}
#endif	//__ASSERT
}
#endif	//__BANK
