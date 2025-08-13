//
// script/ScriptMetadataManager.cpp
//
// Copyright (C) 1999-2015 Rockstar Games. All Rights Reserved.
//

#include "script/script_channel.h"
#include "script/ScriptMetadataManager.h"
#include "parser/macros.h"
#include "parser/manager.h"

const char* CScriptMetadataManager::sm_scriptMetadataPath = "common:/data/scriptMetadata";
CScriptMetadata* CScriptMetadataManager::m_ScriptMetadata = NULL;

CScriptMetadataManager::CScriptMetadataManager()
{
	m_ScriptMetadata = new CScriptMetadata();
}

CScriptMetadataManager::~CScriptMetadataManager()
{
	Shutdown();
}

void CScriptMetadataManager::Init()
{
	PARSER.LoadObjectPtr(sm_scriptMetadataPath, "meta", m_ScriptMetadata);
	Assertf(m_ScriptMetadata, "CScriptMetadataManager::Init: Failed to load %s", sm_scriptMetadataPath);

}

void CScriptMetadataManager::Shutdown()
{
	m_ScriptMetadata->~CScriptMetadata();
}

bool CScriptMetadataManager::GetMPOutfitData(CMPOutfitsData** outfit, int id, bool bMale)
{
	CMPOutfitsMap* outfitsDataMap = NULL;
	if(bMale)
		outfitsDataMap = &(m_ScriptMetadata->m_MPOutfits.m_MPOutfitsDataMale);
	else
		outfitsDataMap = &(m_ScriptMetadata->m_MPOutfits.m_MPOutfitsDataFemale);

	if(outfitsDataMap->m_MPOutfitsData.GetCount() > id && id >= 0)
	{
		*outfit = &outfitsDataMap->m_MPOutfitsData[id];
		return true;
	}
	
	return false;
}

bool CScriptMetadataManager::GetBaseElementLocation(CBaseElementLocation** outLocation, int element, bool bHighApt)
{
	CBaseElementLocationsMap* locationsMap = NULL;
	if(bHighApt)
		locationsMap = &(m_ScriptMetadata->m_BaseElements.m_BaseElementLocationsMap_HighApt);
	else
		locationsMap = &(m_ScriptMetadata->m_BaseElements.m_BaseElementLocationsMap);

	if(locationsMap->m_BaseElementLocations.GetCount() > element && element >= 0)
	{
		*outLocation = &locationsMap->m_BaseElementLocations[element];
		return true;
	}

	return false;
}

bool CScriptMetadataManager::GetBaseElementLocationFromBlock(CBaseElementLocation** outLocation, int element, int dataBlock)
{
	CBaseElementLocationsMap* locationsMap = NULL;
	switch(dataBlock)
	{
		case 0: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_BaseElementLocationsMap);
			break;
		case 1: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_BaseElementLocationsMap_HighApt);
			break;
		case 2: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap1);
			break;
		case 3: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap2);
			break;
		case 4: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap3);
			break;
		case 5: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap4);
			break;
		case 6: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap5);
			break;
		case 7: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap6);
			break;
		case 8: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap7);
			break;
		case 9: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap8);
			break;
		case 10: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap9);
			break;
		case 11: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap10);
			break;
		case 12: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap11);
			break;
		case 13: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap12);
			break;
		case 14: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap13);
			break;
		case 15: 
			locationsMap = &(m_ScriptMetadata->m_BaseElements.m_ExtraBaseElementLocMap14);
			break;
		default:
			scriptAssertf(false, "Unrecognised BaseElementLocation : %d", dataBlock);
			return(false);
	}


	if(locationsMap->m_BaseElementLocations.GetCount() > element && element >= 0)
	{
		*outLocation = &locationsMap->m_BaseElementLocations[element];
		return true;
	}

	return false;
}

int CScriptMetadataManager::GetMPApparelMale(u32 hash)
{
	const int* pValue = m_ScriptMetadata->m_MPApparelData.m_MPApparelDataMale.Access(hash);
	if(pValue)
	{
		return *pValue;
	}

	return -1;
}

int CScriptMetadataManager::GetMPApparelFemale(u32 hash)
{
	const int* pValue = m_ScriptMetadata->m_MPApparelData.m_MPApparelDataFemale.Access(hash);
	if(pValue)
	{
		return *pValue;
	}

	return -1;
}

