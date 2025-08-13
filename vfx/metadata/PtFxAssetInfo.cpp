///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxAssetInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	08 November 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "PtFxAssetInfo.h"
#include "PtFxAssetInfo_parser.h"

// rage
#include "parser/manager.h"
#include "parser/restparserservices.h"

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "vfx/channel.h"

// game
#include "Scene/DataFileMgr.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CPtFxAssetInfoMgr g_ptfxAssetInfoMgr;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxAssetInfoMgr
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void CPtFxAssetInfoMgr::LoadData(ptfxAssetStore& assetStore)
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PTFXASSETINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "ptfx asset info file is not available");
	while (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/AssetInfo", *this, pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}

	for (int i=0; i<m_ptfxAssetDependencyInfos.GetCount(); i++)
	{
		int childSlot = assetStore.FindSlot(m_ptfxAssetDependencyInfos[i]->GetChildName()).Get();
		int parentSlot = assetStore.FindSlot(m_ptfxAssetDependencyInfos[i]->GetParentName()).Get();
		if (ptxVerifyf(childSlot>-1, "ptfx child asset %s not found", m_ptfxAssetDependencyInfos[i]->GetChildName().GetCStr()))
		{
			if (ptxVerifyf(parentSlot>-1, "ptfx parent asset %s not found", m_ptfxAssetDependencyInfos[i]->GetParentName().GetCStr()))
			{
				strLocalIndex currentParentSlot = assetStore.GetParentIndex(strLocalIndex(childSlot));
				if (ptxVerifyf(currentParentSlot==-1 || currentParentSlot==parentSlot, "%s already has a parent ptfx asset set", m_ptfxAssetDependencyInfos[i]->GetChildName().GetCStr()))
				{
					assetStore.SetParentIndex(strLocalIndex(childSlot), parentSlot);
				}
			}
		}
	}

	m_ptfxAssetDependencyInfos.Reset();
}

void CPtFxAssetInfoMgr::AppendData(ptfxAssetStore& assetStore, const char* file)
{
	if (PARSER.LoadObject(file,"meta",*this))
	{
		// all non-core assets have had a dependency set to the core asset(in ptfxAssetStore::InitLevel)
		// detect any of these and remove the dependency whilst we append the new data
		int coreIndex = assetStore.FindSlotFromHashKey(atHashValue("core")).Get();
		for (int index=0; index<assetStore.GetSize(); index++)
		{
			if (assetStore.IsValidSlot(strLocalIndex(index)) && index!=coreIndex)
			{
				// only set the parent of assets that don't have a parent already
				if (assetStore.GetParentIndex(strLocalIndex(index))==coreIndex)
				{
					assetStore.SetParentIndex(strLocalIndex(index), -1);
				}
			}
		}

		for (int i=0; i<m_ptfxAssetDependencyInfos.GetCount(); i++)
		{
			int childSlot = assetStore.FindSlot(m_ptfxAssetDependencyInfos[i]->GetChildName()).Get();
			int parentSlot = assetStore.FindSlot(m_ptfxAssetDependencyInfos[i]->GetParentName()).Get();
			if (ptxVerifyf(childSlot>-1, "ptfx child asset %s not found", m_ptfxAssetDependencyInfos[i]->GetChildName().GetCStr()))
			{
				if (ptxVerifyf(parentSlot>-1, "ptfx parent asset %s not found", m_ptfxAssetDependencyInfos[i]->GetParentName().GetCStr()))
				{
					strLocalIndex currentParentSlot = assetStore.GetParentIndex(strLocalIndex(childSlot));
					if (ptxVerifyf(currentParentSlot==-1 || currentParentSlot==parentSlot, "%s already has a parent ptfx asset set", m_ptfxAssetDependencyInfos[i]->GetChildName().GetCStr()))
					{
						assetStore.SetParentIndex(strLocalIndex(childSlot), parentSlot);
					}
				}
			}
		}

		m_ptfxAssetDependencyInfos.Reset();

		// make sure all non-core assets have a dependency to the core asset
		// they may be sharing textures from the core asset so we don't want to be able to delete the core before the rest
		coreIndex = assetStore.FindSlotFromHashKey(atHashValue("core")).Get();
		for (int index=0; index<assetStore.GetSize(); index++)
		{
			if (assetStore.IsValidSlot(strLocalIndex(index)) && index!=coreIndex)
			{
				// only set the parent of assets that don't have a parent already
				if (assetStore.GetParentIndex(strLocalIndex(index))==-1)
				{
					assetStore.SetParentIndex(strLocalIndex(index), coreIndex);
				}
			}
		}
	}
}