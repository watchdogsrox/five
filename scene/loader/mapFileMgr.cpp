//
// scene/loader/mapFileMgr.h
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//

#include "mapFileMgr.h"

// rage includes

//fw includes
#include "fwsys/fileexts.h"
#include "fwsys/metadatastore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/stores/psostore.h"
#include "fwscene/stores/staticboundsstore.h"

//game include 
#include "scene/loader/ManagedImapGroup.h"
#include "scene/scene_channel.h"
#include "scene/texLod.h"
#include "file/packfile.h"

//pargen includes
#include "scene/loader/Map_TxdParents.h"
#include "PackFileMetaData.h"

#if !__FINAL
#include "scene/DataFileMgr.h"
#endif

SCENE_OPTIMISATIONS();

XPARAM(PartialDynArch);


CMapFileMgr CMapFileMgr::ms_instance;
bool		CMapFileMgr::ms_bDependencyArraysAllocated = false;

void CMapFileMgr::Init(unsigned UNUSED_PARAM(shutdown))
{
	ms_instance.Reset();
}

void CMapFileMgr::Reset(void) 
{
	if (!ms_bDependencyArraysAllocated)
	{
		m_MapDependencies.Reset(); 

		m_MapDeps.Reset();
		m_packFileNames.Reset();
		m_TypDeps.Reset();
		m_ItypPackFileNames.Reset();
		m_MapDeps.Reserve(g_MapDataStore.GetMaxSize());
		m_packFileNames.Reserve(g_MapDataStore.GetMaxSize());
		m_TypDeps.Reserve(g_MapTypesStore.GetMaxSize());
		m_ItypPackFileNames.Reserve(g_MapTypesStore.GetMaxSize());
		//m_interiorBounds.Reset();

		ms_bDependencyArraysAllocated = true;
	}
}

void CMapFileMgr::Shutdown(unsigned UNUSED_PARAM(shutdown))
{
}

void CMapFileMgr::SetupTxdParent(const CTxdRelationship& txdRelationShip)
{
	strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(txdRelationShip.m_child.c_str()));
	if (txdSlot == -1)
	{
		txdSlot = g_TxdStore.AddDummySlot(txdRelationShip.m_child.c_str());
	}

	strLocalIndex txdParentSlot = strLocalIndex(g_TxdStore.FindSlot(txdRelationShip.m_parent.c_str()));
	if (txdParentSlot == -1)
	{
		txdParentSlot = g_TxdStore.AddDummySlot(txdRelationShip.m_parent.c_str());
	}

	strLocalIndex txdPrevParentSlot = strLocalIndex(g_TxdStore.GetParentTxdSlot(txdSlot));

	if (Verifyf(
		txdPrevParentSlot == -1 ||
		txdPrevParentSlot == txdParentSlot,
		"LoadGlobalTxdParents: tried to set '%s' parent to '%s' but parent was already '%s'",
		g_TxdStore.GetName(txdSlot),
		g_TxdStore.GetName(txdParentSlot),
		g_TxdStore.GetName(txdPrevParentSlot)))
	{
		g_TxdStore.SetParentTxdSlot(txdSlot, txdParentSlot);
	}

	#if __DEV
	if (1) // check txd chain for loop
	{
		atMap<s32,bool> slots;

		for (s32 x = txdSlot.Get(); x != -1; x = g_TxdStore.GetParentTxdSlot(strLocalIndex(x)).Get())
		{
			if (slots.Access(x))
			{
				Quitf("LoadGlobalTxdParents: txd chain from '%s' has a loop", g_TxdStore.GetName(txdSlot));
			}

			slots[x] = true;
		}
	}
	#endif // __DEV
}

// the DLC global .txd meta files (common/data/gtxd.meta) are specified in the contents.xml for the DLC pack
void CMapFileMgr::ProcessGlobalTxdParentsFile(const char* pFileName)
{
	Displayf("I want to process %s", pFileName);

	CMapParentTxds gtxdParentsFile;
	if(PARSER.LoadObject(pFileName,"",gtxdParentsFile))
	{
		for(int idx=0;idx<gtxdParentsFile.m_txdRelationships.GetCount();idx++)
		{
			//Displayf("%d : %s :  %s", idx,  gtxdParentsFile.m_txdRelationships[idx].m_parent.c_str(), gtxdParentsFile.m_txdRelationships[idx].m_child.c_str());
			
			const CTxdRelationship& txdRelationShip = gtxdParentsFile.m_txdRelationships[idx];
			SetupTxdParent(txdRelationShip);		
		}
	}
}

void CMapFileMgr::LoadGlobalTxdParents(const char* slotName)
{
	CMapParentTxds *pMapParentsTxds = NULL;
	strLocalIndex parentTxdMetaFileIndex = strLocalIndex(g_fwMetaDataStore.FindSlotFromHashKey(atStringHash(slotName)));
	Assertf(parentTxdMetaFileIndex != -1, "Unable to find %s to setup global txd parents", slotName);
	if (g_fwMetaDataStore.IsValidSlot(parentTxdMetaFileIndex))
	{
		g_fwMetaDataStore.StreamingRequest(parentTxdMetaFileIndex, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);

		if (g_fwMetaDataStore.GetStreamingInfo(parentTxdMetaFileIndex)->GetStatus() != STRINFO_LOADED)
		{
			strStreamingEngine::GetLoader().LoadAllRequestedObjects();
		}

		Assertf(g_fwMetaDataStore.HasObjectLoaded(parentTxdMetaFileIndex), "metadata file not loaded");
		if (g_fwMetaDataStore.Get(parentTxdMetaFileIndex))
		{
			pMapParentsTxds = g_fwMetaDataStore.Get(parentTxdMetaFileIndex)->GetObject< CMapParentTxds >();
			Assertf(pMapParentsTxds, "Unable to extract data from gtxd.meta to setup global txd parents");
			if (pMapParentsTxds)
			{
				for(int i = 0; i < pMapParentsTxds->m_txdRelationships.GetCount(); i ++)
				{
					const CTxdRelationship& txdRelationShip = pMapParentsTxds->m_txdRelationships[i];
					SetupTxdParent(txdRelationShip);			
				}
			}
		}

		g_fwMetaDataStore.ClearRequiredFlag(parentTxdMetaFileIndex.Get(), STRFLAG_DONTDELETE);
	}
}


void CMapFileMgr::LoadGlobalTxdParents()
{
	// the original global .txd meta file file is hardwired in
	LoadGlobalTxdParents("gtxd");
}

// Ick.
namespace rage {
extern fiStream *s_CachedResults;
extern bool s_WriteCacheFile;
}

#if !__FINAL
void ValidateRPF(const fiPackfile* pDevice, const char* pPackFileName)
{
	CDataFileMgr::DataFile* dataFile = DATAFILEMGR.FindDataFile(pPackFileName);

	// Perform some validation steps on this RPF...
	if (dataFile && pDevice && pDevice->IsInitialized() && strncmp(pPackFileName, "dlc", 3) == 0)
	{
		u32 entryCount = pDevice->GetEntryCount();
		const fiPackEntry* entries = pDevice->GetEntries();
		char fullName[STORE_NAME_LENGTH] = { 0 };
		char fullNameNoExt[STORE_NAME_LENGTH] = { 0 };
		u32 entryHandle = 0;
		strStreamingModule* pModule = NULL;	
		const char* extension = NULL;
		strLocalIndex slotIndex;
		strIndex currIndex;

		for (u32 i = 0; i < entryCount; ++i)
		{
			const fiPackEntry& currEntry = entries[i];

			if (!currEntry.IsDir())
			{
				entryHandle = fiCollection::MakeHandle(pDevice->GetPackfileIndex(), ptrdiff_t_to_int(&currEntry - entries));
				pDevice->GetEntryFullName(entryHandle, fullName, STORE_NAME_LENGTH);
				extension = ASSET.FindExtensionInPath(fullName);
				pModule = extension ? strStreamingEngine::GetInfo().GetModuleMgr().GetModuleFromFileExtension(++extension) : NULL;

				if (pModule)
				{
					ASSET.RemoveExtensionFromPath(fullNameNoExt, sizeof(fullNameNoExt), fullName);
					slotIndex = pModule->FindSlot(fullNameNoExt);

					if (slotIndex != -1)
					{
						currIndex = pModule->GetStreamingIndex(slotIndex);

						// If we have any imaps, make sure we have this tag
						if (pModule == INSTANCE_STORE.GetStreamingModule())
						{
							Assertf(dataFile->m_contents == CDataFileMgr::CONTENTS_DLC_MAP_DATA, 
								"%s contains DLC map data but is missing the CONTENTS_DLC_MAP_DATA tag.", 
								dataFile->m_filename);

							ASSERT_ONLY(
							if (strstr(pPackFileName, "_metadata") != NULL || strstr(pPackFileName, "occl") != NULL)
								Assertf(dataFile->m_loadCompletely, "%s isn't set to loadCompletely, it's likely that we need this", dataFile->m_filename);
							)

							break;
						}
					}
				}
			}
		}
	}
}
#endif

// load the per .rpf meta data. HDTxd mappings for now, could include stuff like per .imap bounds to avoid having to open each .imap file
void CMapFileMgr::LoadPackFileMetaData(const fiPackfile* pDevice, const char* pPackFileName){

	if ( (pPackFileName==NULL) || (pDevice==NULL)){
		return;
	}

#if !__FINAL
	ValidateRPF(pDevice, pPackFileName);
#endif

	char *metadataBlob = NULL;
	int metadataBlobSize = 0;
	// If there's a cache (and we're not creating one), pull the metadata blob (if any) from the cache
	if (s_CachedResults && !s_WriteCacheFile){
		s_CachedResults->Read(&metadataBlobSize,sizeof(metadataBlobSize));
		if (metadataBlobSize){
			metadataBlob = rage_new char[metadataBlobSize];
			s_CachedResults->Read(metadataBlob,metadataBlobSize);
		}
	}
	else {
		// Read metadata from original archive
		fiStream *S = ASSET.Open("localPack:/_manifest." MANIFEST_FILE_EXT,"");
		if (S){
			metadataBlobSize = S->Size();
			metadataBlob = rage_new char[metadataBlobSize];
			S->Read(metadataBlob, metadataBlobSize);
			S->Close();
		}
	} 

	// If we're writing a cache file, write either a zero size marker, or the actual data now.
	if (s_CachedResults && s_WriteCacheFile){
		s_CachedResults->Write(&metadataBlobSize,sizeof(metadataBlobSize));
		if (metadataBlobSize)
			s_CachedResults->Write(metadataBlob,metadataBlobSize);
	}

	if (metadataBlobSize){			
		fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CPackFileMetaData>();
		loader.GetFlagsRef().Set(fwPsoStoreLoader::LOAD_IN_PLACE);
		loader.SetInPlaceHeap(PSO_HEAP_CURRENT);
		fwPsoStoreLoadInstance inst;
		// Construct a fake file name based on the metadata blob buffer
		char fileName[64];
		fiDevice::MakeMemoryFileName(fileName,sizeof(fileName),metadataBlob,metadataBlobSize,false,"metadata");
		// Load the data
		loader.Load(fileName, inst);
		// Free the input buffer
		delete[] metadataBlob;

		if (!Verifyf(inst.IsLoaded(), "Couldn't load manifest file "))
			return;

		CPackFileMetaData& packFileMetaData = *reinterpret_cast<CPackFileMetaData*>(inst.GetInstance());

		LoadPackFileMetaDataCommon(packFileMetaData, pPackFileName);
		loader.Unload(inst);
	}
}


void CMapFileMgr::LoadPackFileMetaDataCommon(CPackFileMetaData &packFileMetaData, const char* pPackFileName){

		//if (!ms_bDependenciesSet)
		{
			// ----
			for (s32 i=0; i<packFileMetaData.m_imapDependencies.GetCount(); i++)
			{
				CImapDependency& dep = packFileMetaData.m_imapDependencies[i];

				if ((dep.m_imapName.GetHash() != 0) && (dep.m_itypName.GetHash() != 0))
				{
					dep.m_packFileName.SetFromString(pPackFileName);
					m_MapDependencies.PushAndGrow(dep);
				} else {
					Assertf(false, "Malformed .imap / .ityp dependency in %s ( %s.imap -> %s.ityp )", pPackFileName,
						(dep.m_imapName.GetHash() == 0) ? "NULL" : dep.m_imapName.GetCStr(),
						(dep.m_itypName.GetHash() == 0) ? "NULL" : dep.m_itypName.GetCStr());
				}
			}
			// ---
			// .imap to .ityp dependency data
			u32 numDeps = packFileMetaData.m_imapDependencies_2.GetCount();
			if (numDeps > 0)
			{
				for (u32 i=0; i<numDeps; i++)
				{
					const CImapDependencies& deps = packFileMetaData.m_imapDependencies_2[i];
					Assert(deps.m_imapName.GetHash() != 0);
					if (deps.m_imapName.GetHash() != 0)
					{
						m_packFileNames.Append().SetFromString(pPackFileName);
						CImapDependencies& newDep = m_MapDeps.Append();

						newDep.m_imapName = deps.m_imapName;
						newDep.m_manifestFlags = deps.m_manifestFlags;

						u32 numItyps = deps.m_itypDepArray.GetCount();
						newDep.m_itypDepArray.Resize(numItyps);
						for (u32 j=0; j<numItyps; j++)
						{
							newDep.m_itypDepArray[j] = deps.m_itypDepArray[j];
						}
					}
				}
			}

			// ityp to .ityp dependency data
			numDeps = packFileMetaData.m_itypDependencies_2.GetCount();
			if (numDeps > 0)
			{
				for (u32 i=0; i<numDeps; i++)
				{
					const CItypDependencies& deps = packFileMetaData.m_itypDependencies_2[i];
					Assert(deps.m_itypName.GetHash() != 0);
					if (deps.m_itypName.GetHash() != 0)
					{
						m_ItypPackFileNames.Append().SetFromString(pPackFileName);
						CItypDependencies& newDep = m_TypDeps.Append();

						newDep.m_itypName = deps.m_itypName;
						newDep.m_manifestFlags = deps.m_manifestFlags;

						u32 numItyps = deps.m_itypDepArray.GetCount();
						newDep.m_itypDepArray.Resize(numItyps);
						for (u32 j=0; j<numItyps; j++)
						{
							newDep.m_itypDepArray[j] = deps.m_itypDepArray[j];
						}
					}
				}
			}


			u32 numInteriors = packFileMetaData.m_Interiors.GetCount();
			if (numInteriors > 0)
			{
				for(u32 i = 0; i< numInteriors; i++)
				{
					u32 numInteriorBounds = packFileMetaData.m_Interiors[i].m_Bounds.GetCount();
					Assertf((numInteriorBounds == 2) || (numInteriorBounds == 1), "Unexpected number of interior bounds : %s", packFileMetaData.m_Interiors[i].m_Name.GetCStr());

					CInteriorBoundsFiles& entry = packFileMetaData.m_Interiors[i];

					if (m_interiorBounds.Access(entry.m_Name) == NULL)
					{
						if (numInteriorBounds == 1)
						{
							// check that 1st bound (mover) matches the archetype name
							Assertf( entry.m_Bounds[0] == entry.m_Name, "interior %s : mover bounds %s doesn't match name",entry.m_Name.GetCStr(), entry.m_Bounds[0].GetCStr());
						} 
						else if (numInteriorBounds == 2)
						{
							// store the second bound (weapon)
							if (entry.m_Bounds[0] == entry.m_Name) {	
								m_interiorBounds.Insert(entry.m_Name, entry.m_Bounds[1]);
							} 
							else if (entry.m_Bounds[1] == entry.m_Name) {
								m_interiorBounds.Insert(entry.m_Name, entry.m_Bounds[0]);
							}
							else {
								Assertf( false, "interior %s has no matching bounds",entry.m_Name.GetCStr());
							}
						} 
						else 
						{
							Assertf(false, "Unexpected number of interior bounds : %s", packFileMetaData.m_Interiors[i].m_Name.GetCStr());
						}

						for(u32 j = 0; j< numInteriorBounds; j++)
						{
							// disable both entries in the box streamer
							atHashString boundsFile = packFileMetaData.m_Interiors[i].m_Bounds[j];
							s32 slotIdx = g_StaticBoundsStore.FindSlotFromHashKey(boundsFile).Get();
							if (slotIdx != -1)
							{
								g_StaticBoundsStore.GetBoxStreamer().SetIsIgnored(slotIdx, true);
							}
						}
					}
				}
			}
		}

		// *** HACK HACK HACK ***
		// For the heist island terrain force the offending underwater collision bounds into the mph4_terrain_03 list
		bool hackDone = false;
		if(packFileMetaData.m_MapDataGroups.GetCount() && packFileMetaData.m_MapDataGroups[0].m_Name == ATSTRINGHASH("H4_islandx_terrain_03", 0x3c10464e))
		//if(strstr(pPackFileName, "mph4_Terrain_03")!= nullptr)
		{
			hackDone = true;
			for (s32 i=0; i<packFileMetaData.m_MapDataGroups.GetCount(); i++)
			{
				const CMapDataGroup& mapDataGroup = packFileMetaData.m_MapDataGroups[i];
				CMapDataGroup tempGroup;
				CMapDataGroup const* pGroupToUse = &mapDataGroup;
				if(i == 0) // Just add them to the first group
				{
					tempGroup = mapDataGroup;
					for(int i = 0; i < 36; ++i)
					{
						char name[128] = {0};
						formatf(name, "ma@h4_mph4_terrain_03_%d", i);
						tempGroup.m_Bounds.PushAndGrow(name);
					}
					pGroupToUse = &tempGroup;
				}

				if (pGroupToUse->m_Bounds.GetCount())
					g_StaticBoundsStore.StoreImapGroupBoundsList(pGroupToUse->m_Name, pGroupToUse->m_Bounds);

				if (pGroupToUse->m_Flags.IsSet(TIME_DEPENDENT) || pGroupToUse->m_Flags.IsSet(WEATHER_DEPENDENT))
				{
					CManagedImapGroupMgr::Add(*pGroupToUse);
				}
			}
		}
		
		if(!hackDone)
		{
			// imap group metadata - bounds, weather and time info etc
			for (s32 i=0; i<packFileMetaData.m_MapDataGroups.GetCount(); i++)
			{
				const CMapDataGroup& mapDataGroup = packFileMetaData.m_MapDataGroups[i];
				if (mapDataGroup.m_Bounds.GetCount())
					g_StaticBoundsStore.StoreImapGroupBoundsList(mapDataGroup.m_Name, mapDataGroup.m_Bounds);

				if (mapDataGroup.m_Flags.IsSet(TIME_DEPENDENT) || mapDataGroup.m_Flags.IsSet(WEATHER_DEPENDENT))
				{
					CManagedImapGroupMgr::Add(mapDataGroup);
				}
			}
		}

		for(u32 i=0; i<packFileMetaData.m_HDTxdBindingArray.GetCount(); i++)
		{
			const CHDTxdAssetBinding& newBinding = packFileMetaData.m_HDTxdBindingArray[i];

			eStoreType type = STORE_ASSET_INVALID;
			s32 slot = -1;

			// select store based on asset type
			if (newBinding.m_assetType == AT_TXD){
				type = STORE_ASSET_TXD;
				slot = g_TxdStore.FindSlot(newBinding.m_targetAsset).Get();
				if(slot == -1) {
					slot = g_TxdStore.AddSlot(newBinding.m_targetAsset).Get();
				}
			} else if (newBinding.m_assetType == AT_DRB){
				type = STORE_ASSET_DRB;
				slot = g_DrawableStore.FindSlot(newBinding.m_targetAsset).Get();
				if(slot == -1) {
					slot = g_DrawableStore.AddSlot(newBinding.m_targetAsset).Get();
				}
			} else if (newBinding.m_assetType == AT_FRG){
				type = STORE_ASSET_FRG;
				slot = g_FragmentStore.FindSlot(newBinding.m_targetAsset).Get();
				if(slot == -1) {
					slot = g_FragmentStore.AddSlot(newBinding.m_targetAsset).Get();
				}
			} else if (newBinding.m_assetType == AT_DWD){
				type = STORE_ASSET_DWD;
				slot = g_DwdStore.FindSlot(newBinding.m_targetAsset).Get();
				if(slot == -1) {
					slot = g_DwdStore.AddSlot(newBinding.m_targetAsset).Get();
				}
			} else {
				Assert(false);
				return;
			}

			s32 txdHDSlot = g_TxdStore.FindSlot(newBinding.m_HDTxd).Get();
			if(txdHDSlot != -1)
			{
				CTexLod::StoreHDMapping(type, slot, txdHDSlot);
			}
		}
}

void CMapFileMgr::SetMapFileDependencies_old(void){

	for(u32 i=0; i<m_MapDependencies.GetCount(); i++)
	{
		atHashString imapFile = m_MapDependencies[i].m_imapName;
		atHashString itypFile = m_MapDependencies[i].m_itypName;
		ASSERT_ONLY(atHashString packFileName = m_MapDependencies[i].m_packFileName;)

		if (imapFile.GetHash() == 0){
			continue;
		}

		if (itypFile.GetHash() == 0){
			continue;
		}
 
		strLocalIndex itypSlotIndex = strLocalIndex(g_MapTypesStore.FindSlotFromHashKey(itypFile.GetHash()));
		Assertf(itypSlotIndex != -1, "%s does not exist in the map types store! (in %s)", itypFile.GetCStr(), packFileName.GetCStr());

		// if it is a dependency, then delay loading anyway.
		if (itypSlotIndex != -1)
		{
			fwMapTypesDef* pItypDataDef = g_MapTypesStore.GetSlot(itypSlotIndex);
			Assert(pItypDataDef);
			pItypDataDef->SetIsDelayLoading(true);
		}

		strLocalIndex imapSlotIndex = strLocalIndex(INSTANCE_STORE.FindSlotFromHashKey(imapFile.GetHash()));
		Assertf(imapSlotIndex != -1, "%s does not exist in the map data store! (in %s)", imapFile.GetCStr(), packFileName.GetCStr());

		if ((imapSlotIndex != -1) && (itypSlotIndex != -1))
		{
			fwMapDataDef* pImapDataDef = INSTANCE_STORE.GetSlot(imapSlotIndex);
			Assert(pImapDataDef);
			fwMapTypesDef* pItypDataDef = g_MapTypesStore.GetSlot(itypSlotIndex);
			Assert(pItypDataDef);

			if (pImapDataDef!=NULL && pItypDataDef!=NULL){
				if (!pImapDataDef->IsSetAsParentTypeDef(itypSlotIndex.Get()))
				{
					if (!pImapDataDef->PushParentTypeDef(itypSlotIndex.Get()))	
					{
						sceneAssertf(false, "Too many dependencies. Not adding %s.ityp to %s.imap", g_MapTypesStore.GetName(itypSlotIndex), INSTANCE_STORE.GetName(imapSlotIndex));
					}
				}
				pItypDataDef->SetIsPermanent(false);
			}

			//sceneDisplayf(".ityp (%s) parented to .imap (%s)\n",itypFile.GetCStr(), imapFile.GetCStr());
		}
	}

	// flag interiors which are known to contain MLO data 
	for(u32 imapFileIndex=0; imapFileIndex<m_MapDeps.GetCount(); imapFileIndex++)
	{
		atHashString imapFile = m_MapDeps[imapFileIndex].m_imapName;
		ASSERT_ONLY(atHashString packFileName = m_packFileNames[imapFileIndex];)

		// validate .imap
		strLocalIndex imapSlotIndex = strLocalIndex(INSTANCE_STORE.FindSlotFromHashKey(imapFile.GetHash()));
		Assertf(imapSlotIndex != -1, "%s does not exist in the map data store! (in %s)", imapFile.GetCStr(), packFileName.GetCStr());

		if (imapSlotIndex != -1)
		{
			fwMapDataDef* pImapDataDef = INSTANCE_STORE.GetSlot(imapSlotIndex);
			Assert(pImapDataDef);

			if (m_MapDeps[imapFileIndex].m_manifestFlags.IsSet(INTERIOR_DATA))
			{
				pImapDataDef->SetIsMLOInstanceDef(true);
			}
		}
	}

	Cleanup();
}



// process all of the per .rpf data which we loaded in for all .rpfs earlier
void CMapFileMgr::SetMapFileDependencies(void){

	if (PARAM_PartialDynArch.Get())
	{
		Assertf(false,"-PartialDynArch is no longer supported. Contact John Whyte");
		SetMapFileDependencies_old();
		return;
	}

	// .imap deps 
	for(u32 imapFileIndex=0; imapFileIndex<m_MapDeps.GetCount(); imapFileIndex++)
	{
		atHashString imapFile = m_MapDeps[imapFileIndex].m_imapName;
		atArray<atHashString>& itypFiles = m_MapDeps[imapFileIndex].m_itypDepArray;
		ASSERT_ONLY(atHashString packFileName = m_packFileNames[imapFileIndex];)

		// validate .imap
		strLocalIndex imapSlotIndex = strLocalIndex(INSTANCE_STORE.FindSlotFromHashKey(imapFile.GetHash()));
		Assertf(imapSlotIndex != -1, "%s does not exist in the map data store! (in %s)", imapFile.GetCStr(), packFileName.GetCStr());

		if (imapSlotIndex != -1)
		{
			fwMapDataDef* pImapDataDef = INSTANCE_STORE.GetSlot(imapSlotIndex);
			Assert(pImapDataDef);

			if (m_MapDeps[imapFileIndex].m_manifestFlags.IsSet(INTERIOR_DATA))
			{
				pImapDataDef->SetIsMLOInstanceDef(true);
			}

			for(u32 itypFileIndex=0; itypFileIndex < itypFiles.GetCount(); itypFileIndex++)
			{
				atHashString itypFile = itypFiles[itypFileIndex];

				// validate .ityp 
				strLocalIndex itypSlotIndex = strLocalIndex(g_MapTypesStore.FindSlotFromHashKey(itypFile.GetHash()));
				Assertf(itypSlotIndex != -1, "%s.ityp does not exist in the map types store! (in %s)", itypFile.GetCStr(), packFileName.GetCStr());

				if (itypSlotIndex != -1)
				{

					fwMapTypesDef* pItypDataDef = g_MapTypesStore.GetSlot(itypSlotIndex);
					Assert(pItypDataDef);

					if (pImapDataDef!=NULL && pItypDataDef!=NULL && !(pItypDataDef->GetIsPermanent() || pItypDataDef->GetIsPermanentDLC())){
						//sceneDisplayf("		.ityp (%s)",itypFile.GetCStr());
						if (!pImapDataDef->IsSetAsParentTypeDef(itypSlotIndex.Get()))
						{
							if (!pImapDataDef->PushParentTypeDef(itypSlotIndex.Get()))	
							{
								sceneAssertf(false, "Too many dependencies. Not adding %s.ityp to %s.imap", g_MapTypesStore.GetName(itypSlotIndex), INSTANCE_STORE.GetName(imapSlotIndex));
							
#if __DEV
								sceneDisplayf("Dump ityps already parented to %s.imap (failed to add %s.ityp)", INSTANCE_STORE.GetName(imapSlotIndex), g_MapTypesStore.GetName(itypSlotIndex));
								for(u32 i = 0; i< pImapDataDef->GetNumParentTypes(); i++)
								{
									strLocalIndex itypIdx = pImapDataDef->GetParentTypeIndex(i);
									sceneDisplayf("   - %s.ityp", g_MapTypesStore.GetName(itypIdx));
								}
#endif //__DEV
							}
						}
						pItypDataDef->SetIsDependency(true);
					}					
				}
			}
		}
	}

	// .ityp deps
	for(u32 itypFileIndex=0; itypFileIndex<m_TypDeps.GetCount(); itypFileIndex++)
	{
		atHashString itypFile = m_TypDeps[itypFileIndex].m_itypName;
		atArray<atHashString>& itypFiles = m_TypDeps[itypFileIndex].m_itypDepArray;
		ASSERT_ONLY(atHashString packFileName = m_ItypPackFileNames[itypFileIndex];)

		// validate .ityp
		strLocalIndex itypeSlotIndex = strLocalIndex(g_MapTypesStore.FindSlotFromHashKey(itypFile.GetHash()));
		Assertf(itypeSlotIndex != -1, "%s does not exist in the map type store! (in %s)", itypFile.GetCStr(), packFileName.GetCStr());

		if (itypeSlotIndex != -1)
		{
			fwMapTypesDef* pItypDataDef = g_MapTypesStore.GetSlot(itypeSlotIndex);
			Assert(pItypDataDef);

			if (m_TypDeps[itypFileIndex].m_manifestFlags.IsSet(INTERIOR_DATA))
			{
				pItypDataDef->SetIsMLOType(true);
			}

			for(s32 parentIndex=0; parentIndex < itypFiles.GetCount(); parentIndex++)
			{
				atHashString itypFile = itypFiles[parentIndex];

				// validate parent .ityp 
				strLocalIndex parentSlotIndex = strLocalIndex(g_MapTypesStore.FindSlotFromHashKey(itypFile.GetHash()));
				Assertf(parentIndex != -1, "%s does not exist in the map types store! (in %s)", itypFile.GetCStr(), packFileName.GetCStr());

				if (parentSlotIndex != -1)
				{

					fwMapTypesDef* pParentDef = g_MapTypesStore.GetSlot(parentSlotIndex);
					Assert(pParentDef);

					if (pItypDataDef!=NULL && pParentDef!=NULL && !(pParentDef->GetIsPermanent() || pParentDef->GetIsPermanentDLC())){
						//sceneDisplayf("		.ityp (%s)",itypFile.GetCStr());
						if (!pItypDataDef->IsSetAsParentTypeDef(parentSlotIndex.Get()))
						{
							if (!pItypDataDef->PushParentTypeDef(parentSlotIndex.Get()))	
							{
								sceneAssertf(false, "Too many dependencies. Not adding %s.ityp to %s.ityp", g_MapTypesStore.GetName(parentSlotIndex), g_MapTypesStore.GetName(itypeSlotIndex));
							}
						}
						pParentDef->SetIsDependency(true);
					}					
				}
			}
		}
	}
}

void CMapFileMgr::Cleanup(void){  

#if 1 || !USE_PAGED_POOLS_FOR_STREAMING
	m_MapDependencies.Reset();		// still need to clear this until it is removed!

	m_MapDeps.Reset();
	m_TypDeps.Reset();
	m_packFileNames.Reset();
	m_ItypPackFileNames.Reset();

	//m_interiorBounds.Kill();

	Displayf("num m_interiorBounds : %d", m_interiorBounds.GetNumUsed());


	ms_bDependencyArraysAllocated = false;
#endif // !USE_PAGED_POOLS_FOR_STREAMING
}
