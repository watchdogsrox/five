//
// StatsDataMgr.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "parser/tree.h"
#include "parser/treenode.h"
#include "parser/manager.h"


// Game Headers
#include "FrontendStatsMgr.h"
#include "frontend/ui_channel.h"
#include "Scene/DataFileMgr.h"
#include "scene/extracontent.h"

// Framework headers
#include "fwsys/timer.h"
#include "fwsys/gameskeleton.h"

// Stats headers
#include "Stats/StatsMgr.h"
#include "Stats/stats_channel.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsUtils.h"

#include "Text/TextConversion.h"
#include "Network/Live/livemanager.h"
#include "modelinfo/vehiclemodelinfo.h"

#if __PPU
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#endif

#if RSG_DURANGO || RSG_ORBIS
#define MAX_ASYNC_NAME_LOOKUPS 2

//OPTIMISATIONS_OFF()

bool CFrontendStatsMgr::m_bLookupRequired = false;
int CFrontendStatsMgr::m_iLookupId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
int CFrontendStatsMgr::m_iNumRequests = 0;
#endif // RSG_DURANGO

CFrontendStatsMgr::UIStatsList CFrontendStatsMgr::m_aSPStatsData;
CFrontendStatsMgr::UIStatsList CFrontendStatsMgr::m_aMPStatsData;
CFrontendStatsMgr::UIStatsList CFrontendStatsMgr::m_aSPSkillStats;
CFrontendStatsMgr::UICategoryList CFrontendStatsMgr::m_aCategoryList;
CFrontendStatsMgr::UIReplacementLabelMap CFrontendStatsMgr::m_ReplacementHashes;
CFrontendStatsMgr::UIStatNameCache CFrontendStatsMgr::m_AsyncStatCache;
CFrontendStatsMgr::MountedFilesHashList CFrontendStatsMgr::m_MountedFiles;

CFrontendStatsMgr::CFrontendStatsMgr() {}
CFrontendStatsMgr::~CFrontendStatsMgr() {}

int statSort(sUIStatData* const* a, sUIStatData* const* b);

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
sUIStatData::sUIStatData(const char* pName, const char* pDescription, u8 uCategory, int uVisible,  StatType uType)
{
	m_uHash =  HASH_STAT_ID(pName);
	m_uLocalizedKeyHash =  HASH_STAT_ID(pDescription);
	m_uCategory = uCategory;
	m_iVisibilityRule = uVisible;
	m_uStatType = uType;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
sUIStatDataSP::sUIStatDataSP(sUIStatDataSP::statNameList* pStatList, const char* pDescription, u8 uCategory, int uVisible,  StatType uType)
{
	m_uHash =  HASH_STAT_ID(pStatList->szName[0]);
	m_iStatHash[0] =  HASH_STAT_ID(pStatList->szName[1]);
	m_iStatHash[1] =  HASH_STAT_ID(pStatList->szName[2]);
	m_uLocalizedKeyHash =  HASH_STAT_ID(pDescription);
	m_uCategory = uCategory;
	m_iVisibilityRule = uVisible;
	m_uStatType = uType;
}

sUIStatDataSPDetailed::sUIStatDataSPDetailed(sUIStatDataSP::statNameList* pStatList, const char* pDescription, const char* pDetails, u8 uCategory, int uVisible,  StatType uType)
	: sUIStatDataSP(pStatList, pDescription, uCategory, uVisible, uType)
{
	char buff[32];
	sprintf(buff, "SP0_%s", pDetails);
	m_uDetailHashes[0] = atStringHash(buff);
	
	sprintf(buff, "SP1_%s", pDetails);
	m_uDetailHashes[1] = atStringHash(buff);

	sprintf(buff, "SP2_%s", pDetails);
	m_uDetailHashes[2] = atStringHash(buff);
}

int sUIStatDataSP::GetStatHash(int i) const	
{
	int iResult = 0;

	if (i > STAT_MP_CATEGORY_CHAR1)
		return iResult;

	if (i ==0)
		iResult = m_uHash;
	else 
	{
		iResult = m_iStatHash[i-1];
	}

	return iResult;
}
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
sUIStatDataMP::sUIStatDataMP(sUIStatDataMP::statNameList* pStatList, const char* pDescription, u8 uCategory, int uVisible,  StatType uType)
{
	m_uHash =  HASH_STAT_ID(pStatList->szName[0]);

	for (int i=1; i<MAX_NUM_MP_ACTIVE_CHARS; i++)
	{
		m_iStatHash[i-1] = HASH_STAT_ID(pStatList->szName[i]);
	}

	m_uLocalizedKeyHash =  HASH_STAT_ID(pDescription);
	m_uCategory = uCategory;
	m_iVisibilityRule = uVisible;
	m_uStatType = uType;
}

int sUIStatDataMP::GetStatHash(int i) const	
{
	int iResult = 0;

	if (i>=MAX_NUM_MP_CHARS)
		return iResult;

	if (i ==0)
		iResult = m_uHash;
	else 
	{
		iResult = m_iStatHash[i-1];
	}

	return iResult;
}

class CStatsUIListFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		u32 uFileHash = atStringHash(file.m_filename);
		if(CFrontendStatsMgr::GetMountedFileList().Find(uFileHash) != -1)
			return false;

		switch(file.m_fileType)
		{
			case CDataFileMgr::SP_STATS_UI_LIST_FILE:
				CFrontendStatsMgr::LoadDataXMLFile(file.m_filename,true);
				CFrontendStatsMgr::GetMountedFileList().PushAndGrow(uFileHash);
				return true;
			case CDataFileMgr::MP_STATS_UI_LIST_FILE:
				CFrontendStatsMgr::LoadDataXMLFile(file.m_filename,false);
				CFrontendStatsMgr::GetMountedFileList().PushAndGrow(uFileHash);
				return true;
			default:
				return false;
		}
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & ) {}
} g_StatsUIListDataFileMounter;

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void CFrontendStatsMgr::Update()
{
#if __XENON
	bool bChecking = false;
	for(CFrontendStatsMgr::UIStatNameCache::Iterator iter = m_AsyncStatCache.CreateIterator(); !iter.AtEnd(); iter.Next())
	{
		sUIAsyncCacheData &data = iter.GetData();
		bChecking |= data.m_bChecking;
		if(!data.m_bReady)
		{
			if(!CLiveManager::GetFindGamerTag().Pending() && !bChecking)
			{
				CLiveManager::GetFindGamerTag().Start(data.m_hGamer);
				data.m_bChecking = true;
				bChecking = true;
			}

			if(CLiveManager::GetFindGamerTag().Succeeded())
			{
				sprintf(data.m_Name, "%s", CLiveManager::GetFindGamerTag().GetGamerTag(data.m_hGamer));
				if(strcmp(data.m_Name, "") != 0)
				{
					data.m_bReady = true;
					data.m_bChecking = false;
				}
			}
		}
	}
#elif RSG_DURANGO || RSG_ORBIS
	if(m_iLookupId == CDisplayNamesFromHandles::INVALID_REQUEST_ID && m_bLookupRequired)
	{
		int iCount = 0;
		rlGamerHandle gamerHandles[MAX_ASYNC_NAME_LOOKUPS];
		for(CFrontendStatsMgr::UIStatNameCache::Iterator iter = m_AsyncStatCache.CreateIterator(); !iter.AtEnd(); iter.Next())
		{
			sUIAsyncCacheData &data = iter.GetData();
			if(data.m_hGamer.IsValid())
			{
				gamerHandles[iCount] = data.m_hGamer;
				data.m_bReady = false;
				data.m_bChecking = true;
				iCount++;
			}
		}
		m_iNumRequests = iCount;
		
		if(m_iNumRequests > 0)
		{
			m_iLookupId = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), gamerHandles, m_iNumRequests);
			if(m_iLookupId == CDisplayNamesFromHandles::INVALID_REQUEST_ID)
			{
				CLiveManager::GetFindDisplayName().CancelRequest(m_iLookupId);
				m_iLookupId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			}
		}
	}
	
	if(m_iLookupId != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
	{
		rlDisplayName displayNames[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST];
		int iStatus = CLiveManager::GetFindDisplayName().GetDisplayNames(m_iLookupId, displayNames, m_iNumRequests);
		int iCount = 0;
		switch(iStatus)
		{
			case CDisplayNamesFromHandles::DISPLAY_NAMES_SUCCEEDED:
				for(CFrontendStatsMgr::UIStatNameCache::Iterator iter = m_AsyncStatCache.CreateIterator(); !iter.AtEnd(); iter.Next())
				{
					sUIAsyncCacheData &data = iter.GetData();
					sprintf(data.m_Name, "%s", displayNames[iCount]);
					if(strcmp(data.m_Name, "") != 0)
					{
						data.m_bReady = true;
						data.m_bChecking = false;
					}
					iCount++;
				}
			case CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED:
				m_iLookupId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
				m_bLookupRequired = false;
				m_iNumRequests = 0;
			default:
				break;
		}
	}
#endif // RSG_DURANGO || RSG_ORBIS
}

void CFrontendStatsMgr::Init(unsigned initMode)
{
	m_aSPStatsData.clear();
	m_aSPSkillStats.clear();
	m_aMPStatsData.clear();
	m_aCategoryList.Reset();
	m_aCategoryList.Grow(16);
	m_ReplacementHashes.Reset();
	m_AsyncStatCache.Reset();
	m_MountedFiles.Reset();
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SP_STATS_UI_LIST_FILE, &g_StatsUIListDataFileMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::MP_STATS_UI_LIST_FILE, &g_StatsUIListDataFileMounter);
	LoadDataXMLFile(initMode);

#if RSG_DURANGO || RSG_ORBIS
	CancelDisplayNameLookup();
#endif // RSG_DURANGO
}

void CFrontendStatsMgr::Shutdown(unsigned UNUSED_PARAM(shutdownMode) )
{
	m_aSPStatsData.clear();
	m_aSPSkillStats.clear();
	m_aMPStatsData.clear();
	m_aCategoryList.Reset();
	m_ReplacementHashes.Reset();
	m_AsyncStatCache.Reset();
	m_MountedFiles.Reset();

#if RSG_DURANGO || RSG_ORBIS
	CancelDisplayNameLookup();
#endif // RSG_DURANGO || RSG_ORBIS
}

void CFrontendStatsMgr::LoadDataXMLFile(const unsigned initMode)
{
	if (INIT_CORE == initMode)
	{
		Shutdown(SHUTDOWN_CORE);

		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::SP_STATS_UI_LIST_FILE);
		if (DATAFILEMGR.IsValid(pData))
		{
			// Read in this particular file
			LoadDataXMLFile(pData->m_filename, true);

			// Get next file
			pData = DATAFILEMGR.GetPrevFile(pData);

			DEV_ONLY(if (m_aSPStatsData.empty()) statWarningf("Single Player stats are empty");)
		}

		const CDataFileMgr::DataFile* pMPData = DATAFILEMGR.GetLastFile(CDataFileMgr::MP_STATS_UI_LIST_FILE);
		if (DATAFILEMGR.IsValid(pMPData))
		{
			// Read in this particular file
			LoadDataXMLFile(pMPData->m_filename, false);

			// Get next file
			pData = DATAFILEMGR.GetPrevFile(pMPData);

			DEV_ONLY(if (m_aMPStatsData.empty()) statWarningf("Multiplayer stats are empty");)
		}

	
		statDebugf1("-----------------------------------------------------------------");
		statDebugf1("             Number of UI stats = \"%d\"", m_aSPStatsData.size());
		statDebugf1("             Sizeof UI Stats = \"%" SIZETFMT "d\"", m_aSPStatsData.size()*sizeof(sUIStatData));
		statDebugf1("-----------------------------------------------------------------");
	}
}

void CFrontendStatsMgr::LoadDataXMLFile(const char* pFileName, bool bSinglePlayer)
{

	if (AssertVerify(pFileName))
	{
		parTree* pTree = PARSER.LoadTree(pFileName, "xml");
		if(pTree)
		{
			parTreeNode* pRootNode = pTree->GetRoot();
			if(pRootNode)
			{
				parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
				for(; it != pRootNode->EndChildren(); ++it)
				{
					if(stricmp((*it)->GetElement().GetName(), "stats")    == 0)
					{
						LoadStatsData((*it), bSinglePlayer);
					}
				}

				// Old stat sort. Uncomment if design wants stats sorted again.
// 				if (bSinglePlayer)
// 				{
// 					m_aSPStatsData.QSort(0,m_aSPStatsData.GetCount(), statSort);
// 				}
// 				else
// 				{
// 					m_aMPStatsData.QSort(0,m_aMPStatsData.GetCount(), statSort);
// 				}
			}

			delete pTree;
		}
	}
}

void CFrontendStatsMgr::LoadStatsData(parTreeNode* pNode, bool bSinglePlayer)
{
	if(pNode)
	{
		parTreeNode::ChildNodeIterator it = pNode->BeginChildren();
		for(; it != pNode->EndChildren(); ++it)
		{
			u32	uCategory= 0;
			// Verify Categories and add them to the list of categories.
			if(stricmp((*it)->GetElement().GetName(), "Category") == 0)
			{
				Assertf((*it)->GetElement().FindAttribute("label"), "Missing attribute \"label\"");

				if ((*it)->GetElement().FindAttribute("label"))
				{
					const char* pStatIdCategory = (*it)->GetElement().FindAttributeAnyCase("label")->GetStringValue();
					
					if (AssertVerify(pStatIdCategory) )
					{
						sUICategoryData* pCategory = NULL;	
						for(u32 i = 0; i< m_aCategoryList.GetCount(); ++i)
						{
							if(strcmp(m_aCategoryList[i]->m_Name,pStatIdCategory)==0)
							{
								pCategory = m_aCategoryList[i];
								break;
							}
						}
						if(!pCategory)
						{
							pCategory = rage_new sUICategoryData(pStatIdCategory, bSinglePlayer);
							m_aCategoryList.PushAndGrow(pCategory);
						}
						for(u32 i=0;i<m_aCategoryList.GetCount();i++)
						{
							if(pCategory == m_aCategoryList[i])
							{
								break;
							}
							if(m_aCategoryList[i]->m_bSinglePlayer == bSinglePlayer)
								uCategory++;							
						}
						parTreeNode::ChildNodeIterator statIterator = (*it)->BeginChildren();
						for(; statIterator != (*statIterator)->EndChildren(); ++statIterator )
						{
							// Check for individual stats.
							if (stricmp((*statIterator)->GetElement().GetName(), "Stat") == 0)
							{
								Assertf((*statIterator)->GetElement().FindAttribute("name"), "Missing attribute \"name\"");
								Assertf((*statIterator)->GetElement().FindAttribute("visible"), "Missing attribute \"visible\"");
								Assertf((*statIterator)->GetElement().FindAttribute("type"), "Missing attribute \"type\"");
								
								if (  (*statIterator)->GetElement().FindAttribute("name") 
								   && (*statIterator)->GetElement().FindAttribute("visible") 
								   && (*statIterator)->GetElement().FindAttribute("type") )
								{
									atArray<sUIReplacementTag> tags;
									const char* pStatIdName         = (*statIterator)->GetElement().FindAttributeAnyCase("name")->GetStringValue();
									const char* pStatVisibility     = (*statIterator)->GetElement().FindAttributeAnyCase("visible")->GetStringValue();
									const char* pStatType			= (*statIterator)->GetElement().FindAttributeAnyCase("type")->GetStringValue();
									const char* pDetails			= NULL;
									const char* pDescription		= NULL;
									bool bCharacterStat				= (*statIterator)->GetElement().FindAttributeBoolValue("characterStat",true);
									bool bLabelAndValue				= (*statIterator)->GetElement().FindAttributeBoolValue("labelAndValue",false);
									
									if ((*statIterator)->GetElement().FindAttributeAnyCase("label"))
									{
										pDescription = (*statIterator)->GetElement().FindAttributeAnyCase("label")->GetStringValue();
									}

									if ((*statIterator)->GetElement().FindAttributeAnyCase("details"))
									{
										pDetails = (*statIterator)->GetElement().FindAttributeAnyCase("details")->GetStringValue();
									}

									parTreeNode::ChildNodeIterator replacementTagIter = (*statIterator)->BeginChildren();
									for(; replacementTagIter != (*replacementTagIter)->EndChildren(); ++replacementTagIter)
									{
										Assertf((*replacementTagIter)->GetElement().FindAttribute("minValue"), "Missing attribute \"minValue\"");
										Assertf((*replacementTagIter)->GetElement().FindAttribute("maxValue"), "Missing attribute \"maxValue\"");
										Assertf((*replacementTagIter)->GetElement().FindAttribute("label"), "Missing attribute \"label\"");
										
										if (  (*replacementTagIter)->GetElement().FindAttribute("minValue") 
											&& (*replacementTagIter)->GetElement().FindAttribute("maxValue") 
											&& (*replacementTagIter)->GetElement().FindAttribute("label") )
										{
											int minValue = (*replacementTagIter)->GetElement().FindAttributeIntValue("minValue", 0);
											int maxValue = (*replacementTagIter)->GetElement().FindAttributeIntValue("maxValue", 0);
											bool rawNumber = (*replacementTagIter)->GetElement().FindAttributeBoolValue("rawNumber", false);
											char strLabel[32];
											sprintf(strLabel, "%s", (*replacementTagIter)->GetElement().FindAttributeAnyCase("label")->GetStringValue());
											u32 nameHash = rawNumber ? atoi(strLabel) : atStringHash(strLabel);

											sUIReplacementTag replacementTag(minValue,maxValue,nameHash,bLabelAndValue);
											tags.PushAndGrow(replacementTag);
										}
									}
									
									if (bCharacterStat)
									{
										RegisterNewStatForEachCharacter(pStatIdName
											, pDescription
											, pDetails
											, (u8)uCategory
											, GetVisibilityFromXML(pStatVisibility)
											, GetStatTypeFromXML(pStatType)
											, tags
											, bSinglePlayer );
									}
									else
									{
										RegisterNewStat( pStatIdName
											, pDescription 
											, pDetails
											, (u8)uCategory
											, GetVisibilityFromXML(pStatVisibility)
											, GetStatTypeFromXML(pStatType)
											, tags
											, bSinglePlayer );
									}
									
								}
							}
						}
					}
				}
			}
		}
	}
}


void CFrontendStatsMgr::RegisterNewStatForEachCharacter(const char* pName
								   ,const char* pDescription
								   ,const char* pDetails
								   ,u8 uCategory
								   ,int bVisible
								   ,StatType uType
								   ,atArray<sUIReplacementTag>& tags
								   ,bool bSinglePlayer)
{

	if (bSinglePlayer)
	{
		sUIStatDataSP::statNameList oNames;

		for (int i= 0; i < STAT_SP_CATEGORY_CHAR2; i++)
		{
			snprintf(oNames.szName[i], MAX_STAT_LABEL_SIZE, "SP%d_%s", i, pName);
			oNames.szName[i][MAX_STAT_LABEL_SIZE-1] = '\0';
			
#if __BANK
			const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
			const sStatData* stat = statsDataMgr.GetStat(atHashValue::ComputeHash(oNames.szName[i]));
			if (!stat)
			{
				char szErrorMsg[128];
				formatf(szErrorMsg,128,"FrontendStatManager - Stat %s doesn't exist.",oNames.szName[i]);
				AssertMsg(0,szErrorMsg);
			}
#endif
		}

		if(pDetails != NULL)
		{
			sUIStatDataSPDetailed* pStatSlot = rage_new sUIStatDataSPDetailed(&oNames
				,pDescription ? pDescription : pName
				,pDetails
				,uCategory
				,bVisible
				,uType);

			//Insert stat in StatsMap
			if (pStatSlot)
			{
				m_aSPStatsData.PushAndGrow(pStatSlot);
				m_aSPSkillStats.PushAndGrow(pStatSlot);

				if(tags.GetCount() > 0)
				{
					m_ReplacementHashes.Insert(pStatSlot->GetStatHash(0), tags);
				}
			}
		}
		else
		{
			sUIStatDataSP* pStatSlot = rage_new sUIStatDataSP(&oNames
				,pDescription ? pDescription : pName
				,uCategory
				,bVisible
				,uType);

			//Insert stat in StatsMap
			if (pStatSlot)
			{
				m_aSPStatsData.PushAndGrow(pStatSlot);

				if(tags.GetCount() > 0)
				{
					m_ReplacementHashes.Insert(pStatSlot->GetStatHash(0), tags);
				}
			}
		}	
	}
	else
	{
		sUIStatDataMP::statNameList oNames;

		for (int i=0; i<MAX_NUM_MP_ACTIVE_CHARS; i++)
		{
			snprintf(oNames.szName[i], MAX_STAT_LABEL_SIZE, "MP%d_%s", i, pName);
			oNames.szName[i][MAX_STAT_LABEL_SIZE-1] = '\0';

#if __BANK
			const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
			const sStatData* stat = statsDataMgr.GetStat(atHashValue::ComputeHash(oNames.szName[i]));
			if (!stat)
			{
				char szErrorMsg[128];
				formatf(szErrorMsg,128,"FrontendStatManager - Stat %s doesn't exist.",oNames.szName[i]);
				AssertMsg(0,szErrorMsg);
			}
#endif
		}

		sUIStatDataMP* pStatSlot = rage_new sUIStatDataMP(&oNames
				,pDescription ?  pDescription : pName
				,uCategory
				,bVisible
				,uType);

		//Insert stat in StatsMap
		if (pStatSlot)
		{
			m_aMPStatsData.PushAndGrow(pStatSlot);

			if(tags.GetCount() > 0)
			{
				m_ReplacementHashes.Insert(pStatSlot->GetStatHash(0), tags);
			}
		}
	}
}


sUIStatData*  
CFrontendStatsMgr::RegisterNewStat(const char* pName
							   ,const char* pDescription
							   ,const char* UNUSED_PARAM(pDetails)
							   ,u8 uCategory
							   ,int bVisible
							   ,StatType uType
							   ,atArray<sUIReplacementTag>& tags
							   ,bool bSinglePlayer)
{

#if __BANK
	const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
	const sStatData* stat = statsDataMgr.GetStat(atHashValue::ComputeHash(pName));
	if (!stat)
	{
		char szErrorMsg[128];
		formatf(szErrorMsg,128,"FrontendStatManager - Stat %s doesn't exist.",pName);
		AssertMsg(0,szErrorMsg);
	}
#endif

	sUIStatData* pStatSlot = rage_new sUIStatData(pName
												,pDescription ?  pDescription : pName
												,uCategory
												,bVisible
												,uType);

	statAssert(pStatSlot);
	if (!pStatSlot)
		return NULL;

	if(tags.GetCount() > 0)
	{
		m_ReplacementHashes.Insert(pStatSlot->GetStatHash(0), tags);
	}

	//Insert stat in StatsMap
	if (bSinglePlayer)
	{
		m_aSPStatsData.PushAndGrow(pStatSlot);
	}
	else
	{
		m_aMPStatsData.PushAndGrow(pStatSlot);
	}

	return pStatSlot;
}

void CFrontendStatsMgr::GetDataAsString(const sStatDataPtr* ppStat, const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize)
{
	if (!pStatUI || !outBuffer )
		return;

	const sStatData* pStat = 0;
	if (ppStat && ppStat->KeyIsvalid())
	{
		pStat = *ppStat;
	}

	if (!pStat)
		return;

	switch (pStat->GetType())
	{
	case STAT_TYPE_INT:
		{ 
			s32 sData = (s32)StatsInterface::GetIntStat(ppStat->GetKey());
			FormatDataAsType(pStatUI, outBuffer, bufferSize, sData);
		}
		break;
	case STAT_TYPE_USERID:
		{
			if(pStatUI->GetType() == eStatType_PlayerID)
			{
				char pStatText[128];
				bool bSuccess = pStat->GetUserId(pStatText, 128);

				if(!bSuccess || strcmp(pStatText, "0") == 0)
				{
					char* str = TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "STAT_TYPE_PLAYERID");
					CText::FilterOutTokensFromString(str); // remove any markers
					FormatDataAsType(outBuffer, bufferSize, str);
				}
				else
				{
#if __PS3 || RSG_PC
					::sprintf(outBuffer, "%s", pStatText);
#elif __XENON || RSG_DURANGO || RSG_ORBIS
#if RSG_ORBIS		
					if (!rlGamerHandle::IsUsingAccountIdAsKey())
					{
						::sprintf(outBuffer, "%s", pStatText);
					}
					else
#endif
					{
						sUIAsyncCacheData* pCachedName = m_AsyncStatCache.Access(pStatUI->GetStatHash(0));
						if (pCachedName)
						{
							if (strcmp(pCachedName->m_UID, pStatText) == 0)
							{
								::sprintf(outBuffer, "%s", pCachedName->m_Name);
							}
							else
							{
								sprintf(pCachedName->m_UID, "%s", pStatText);
								pCachedName->m_hGamer.FromUserId(pStatText);

								if (!pCachedName->m_hGamer.IsValid())
								{
									sprintf(pCachedName->m_Name, "%s", TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "STAT_TYPE_PLAYERID"));
									::sprintf(outBuffer, "%s", pCachedName->m_Name);
								}
								else
								{
									pCachedName->m_bReady = false;
								}
							}
						}
						else
						{
							sUIAsyncCacheData cacheData;
							cacheData.m_bReady = false;
							cacheData.m_hGamer.FromUserId(pStatText);
							sprintf(cacheData.m_UID, "%s", pStatText);
							sprintf(cacheData.m_Name, "%s", TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "STAT_TYPE_PLAYERID"));

							m_AsyncStatCache.Insert(pStatUI->GetStatHash(0), cacheData);
							::sprintf(outBuffer, "%s", TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "STAT_TYPE_PLAYERID"));
						}
					}
#else
					// For when we have to handle this on other platforms
					::sprintf(outBuffer, "%s", pStatText);
#endif
				}
			}
		}
		break;
	case STAT_TYPE_PERCENT:
	case STAT_TYPE_UINT32:
		{
			u32 uData = (u32)StatsInterface::GetUInt32Stat(ppStat->GetKey());
			FormatDataAsType(pStatUI, outBuffer, bufferSize, uData);
		}
		break;
	case STAT_TYPE_PACKED:
	case STAT_TYPE_UINT64:
	case STAT_TYPE_DATE:
		{
			u64 uData = (u64)StatsInterface::GetUInt64Stat(ppStat->GetKey());
			FormatDataAsType(pStatUI, outBuffer, bufferSize, uData);
		}
		break;
	case STAT_TYPE_INT64:
		{
			s64 uData = (s64)StatsInterface::GetInt64Stat(ppStat->GetKey());
			FormatDataAsType(pStatUI, outBuffer, bufferSize, uData);
		}
		break;
	case STAT_TYPE_FLOAT:
		{ 
			float fData  = StatsInterface::GetFloatStat(ppStat->GetKey());
			FormatDataAsType(pStatUI, outBuffer, bufferSize, fData);
		}
		break;
	case STAT_TYPE_TEXTLABEL:
		{ // number is string:

			int iStatHash = ppStat->GetKey();
			StatId statMissionName(iStatHash);
			int iStatValue = StatsInterface::GetIntStat(statMissionName);
			char* pStatText = NULL;

			if (iStatValue)
			{
				pStatText = TheText.Get(iStatValue, "STAT_TYPE_TEXTLABEL");
				CText::FilterOutTokensFromString(pStatText); // remove any markers
				FormatDataAsType(outBuffer, bufferSize, pStatText);
			}
			else
			{
				iStatValue = ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
				pStatText = TheText.Get(iStatValue, "STAT_TYPE_TEXTLABEL");
				CText::FilterOutTokensFromString(pStatText); // remove any markers
				FormatDataAsType(outBuffer, bufferSize, pStatText);
			}
		}
		break;
	case STAT_TYPE_BOOLEAN:
		{
			bool bData  = StatsInterface::GetBooleanStat(ppStat->GetKey());
			FormatDataAsType(pStatUI, outBuffer, bufferSize, bData);
		}
		break;
	case STAT_TYPE_STRING:
		{
			if(strcmp(pStat->GetString(), "") == 0)
			{
				char* str = TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "STAT_TYPE_PLAYERID");
				CText::FilterOutTokensFromString(str); // remove any markers
				FormatDataAsType(outBuffer, bufferSize, str);
			}
			else
			{
				formatf(outBuffer, bufferSize, "%s",pStat->GetString());
			}
		}
		break;
	case STAT_TYPE_DEGREES:
	case STAT_TYPE_WEIGHT:
	case STAT_TYPE_CASH:
	case STAT_TYPE_METERS:
	case STAT_TYPE_TIME:
	case STAT_TYPE_FEET:
	case STAT_TYPE_MILES:
	case STAT_TYPE_VELOCITY:
	case STAT_TYPE_SECONDS:
		{
			AssertMsg(0,"STAT_TYPE unhandled.  Please add to this switch statement or contact cconlon.");
		}
		break;
	
	default:
		{
			::sprintf(outBuffer, "NOTYPE:%d", StatsInterface::GetStatType(ppStat->GetKey()));
		}
		break;
	}

}

float CFrontendStatsMgr::GetDataAsPercent(const sStatDataPtr* ppStat, const sUIStatData* pStatUI)
{
	if (!pStatUI)
		return 0.0f;

	const sStatData* pStat = 0;
	if (ppStat && ppStat->KeyIsvalid())
	{
		pStat = *ppStat;
	}

	if (!pStat)
		return 0.0f;

	float percent = 0.0f;
	switch (pStat->GetType())
	{
	case STAT_TYPE_INT:
		{ 
			if(pStatUI->GetType() != eStatType_Checklist)
			{
				s32 sData = (s32)StatsInterface::GetIntStat(ppStat->GetKey());
				if(pStatUI->GetType() == eStatType_Percent)
				{
					percent = sData / 100.0f;
				}
				else if(pStatUI->GetType() == eStatType_Fraction)
				{
					u32 statHash = pStatUI->GetStatHash(0);
					atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
					if(tagData)
					{
						float divisor = (float)GetReplacementLocalizedHash(statHash, sData);
						percent = sData / divisor;
					}
				}
			}
			else
			{
				u32 statHash = pStatUI->GetStatHash(0);
				atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
				if(tagData && tagData->GetCount() == 2)
				{
					u32 uNumeratorHash = (*tagData)[0].m_uHash;
					u32 uDenominatorHash = (*tagData)[1].m_uHash;

					float fNumerator = (float)StatsInterface::GetIntStat(uNumeratorHash);
					float fDenominator = (float)StatsInterface::GetIntStat(uDenominatorHash);

					if(fDenominator != 0)
					{
						percent = fNumerator / fDenominator;
					}
					else
					{
						percent = 0.0f;
					}
				}
			}
		}
		break;
	case STAT_TYPE_PERCENT:
	case STAT_TYPE_UINT32:
		{
			if(pStatUI->GetType() != eStatType_Checklist)
			{
				u32 uData = (u32)StatsInterface::GetUInt32Stat(ppStat->GetKey());
				if(pStatUI->GetType() == eStatType_Percent)
				{
					percent = uData / 100.0f;
				}
				else if(pStatUI->GetType() == eStatType_Fraction)
				{
					u32 statHash = pStatUI->GetStatHash(0);
					atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
					if(tagData)
					{
						float divisor = (float)GetReplacementLocalizedHash(statHash, uData);
						percent = uData / divisor;
					}
				}
			}
			else
			{
				u32 statHash = pStatUI->GetStatHash(0);
				atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
				if(tagData && tagData->GetCount() == 2)
				{
					u32 uNumeratorHash = (*tagData)[0].m_uHash;
					u32 uDenominatorHash = (*tagData)[1].m_uHash;

					float fNumerator = (float)StatsInterface::GetIntStat(uNumeratorHash);
					float fDenominator = (float)StatsInterface::GetIntStat(uDenominatorHash);

					if(fDenominator != 0)
					{
						percent = fNumerator / fDenominator;
					}
					else
					{
						percent = 0.0f;
					}
				}
			}
		}
		break;
	case STAT_TYPE_FLOAT:
		{ 
			if(pStatUI->GetType() != eStatType_Checklist)
			{
				float fData  = StatsInterface::GetFloatStat(ppStat->GetKey());
				if(pStatUI->GetType() == eStatType_Percent)
				{
					percent = fData / 100.0f;
				}
				else if(pStatUI->GetType() == eStatType_Fraction)
				{
					u32 statHash = pStatUI->GetStatHash(0);
					atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
					if(tagData)
					{
						float divisor = (float)GetReplacementLocalizedHash(statHash, (int)fData);
						percent = fData / divisor;
					}
				}
			}
			else
			{
				u32 statHash = pStatUI->GetStatHash(0);
				atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
				if(tagData && tagData->GetCount() == 2)
				{
					u32 uNumeratorHash = (*tagData)[0].m_uHash;
					u32 uDenominatorHash = (*tagData)[1].m_uHash;

					float fNumerator = (float)StatsInterface::GetIntStat(uNumeratorHash);
					float fDenominator = (float)StatsInterface::GetIntStat(uDenominatorHash);

					if(fDenominator != 0)
					{
						percent = fNumerator / fDenominator;
					}
					else
					{
						percent = 0.0f;
					}
				}
			}
		}
		break;
	case STAT_TYPE_TEXTLABEL:
	case STAT_TYPE_USERID:
	case STAT_TYPE_PACKED:
	case STAT_TYPE_UINT64:
	case STAT_TYPE_INT64:
	case STAT_TYPE_DATE:
	case STAT_TYPE_BOOLEAN:
	case STAT_TYPE_DEGREES:
	case STAT_TYPE_WEIGHT:
	case STAT_TYPE_CASH:
	case STAT_TYPE_METERS:
	case STAT_TYPE_TIME:
	case STAT_TYPE_FEET:
	case STAT_TYPE_MILES:
	case STAT_TYPE_VELOCITY:
	case STAT_TYPE_SECONDS:
	case STAT_TYPE_STRING:
	default:
		{
			AssertMsg(0,"STAT_TYPE unhandled.  Please add to this switch statement or contact cconlon.");
		}
		break;
	}

	return percent;
}

void CFrontendStatsMgr::FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, s32 sData)
{
	switch(pStatUI->GetType())
	{
		case eStatType_Int:
		{
			u32 statHash = pStatUI->GetStatHash(0);
			atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
			if(tagData)
			{
				char* gxt = 0;
				gxt = TheText.Get(GetReplacementLocalizedHash(statHash, sData), "UNKNOWN");

				if((*tagData)[0].m_bUseLabelAndValue)
				{
					::sprintf(outBuffer,"%s (%d)", gxt, sData);
				}
				else
				{
					::sprintf(outBuffer,"%s", gxt);
				}
			}
			else
			{
				int iData = (int)sData;
				char humanIntBuff[32];
				int stringSize = sizeof(humanIntBuff)/sizeof(humanIntBuff[0]);
				CTextConversion::FormatIntForHumans(iData, humanIntBuff, stringSize, "%s");
				::sprintf(outBuffer,"%s",humanIntBuff);
			}
			break;
		}
		case eStatType_Float:
		{
			float fData = (float)sData;
			::sprintf(outBuffer,"%.2f",fData);
			break;
		}
		case eStatType_Percent:
		{
			float fData = (float)sData;
			::sprintf(outBuffer,"%.2f%%",fData);
			break;
		}
		case eStatType_Distance:
		{
			float fData = (float)sData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? METERS_TO_KILOMETERS(fData) : METERS_TO_MILES(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KILO") : TheText.Get("UNIT_MILES"));
			break;
		}
		case eStatType_ShortDistance:
		{
			float fData = (float)sData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? fData : METERS_TO_FEET(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_METERS") : TheText.Get("UNIT_FEET"));
			break;
		}
		case eStatType_Speed:
		{
			float fData = (float)sData;
			::sprintf(outBuffer,"%.2f%s",
				ShouldUseMetric() ? fData : KPH_TO_MPH(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KPH") : TheText.Get("UNIT_MPH"));
			break;
		}
		case eStatType_Time:
		{
			CStatsUtils::GetTime(sData, outBuffer, bufferSize, true);
			break;
		}
		case eStatType_Cash:
		{
			s64 iData = (int)sData;
			char cashBuff[32];
			FormatInt64ToCash(iData, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));
			::sprintf(outBuffer,"%s",cashBuff);
			break;
		}
		case eStatType_NameHash:
		{
			u32 uData = sData ? (u32)sData : ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
			::sprintf(outBuffer, "%s", TheText.Get(uData, "NAME_HASH"));
			break;
		}
		case eStatType_CarHash:
		{
			u32 uData = (u32)sData;
			::sprintf(outBuffer, "%s", GetVehicleNameFromHashKey(uData));
			break;
		}
		case eStatType_Bool:
		{
			bool bData = sData ? true : false ;
			
			if (bData)
				::sprintf(outBuffer,"%s",TheText.Get("MO_YES"));
			else
				::sprintf(outBuffer,"%s",TheText.Get("MO_NO"));

			break;
		}
		case eStatType_Date:
		{
			u64 uData = (u64)sData;
			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate(uData, year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(outBuffer, "%04d / %02d / %02d",year,month,day);

			break;
		}
		case eStatType_Bar:
		{
			::sprintf(outBuffer,"");
			break;
		}
		case eStatType_Fraction:
			{
				u32 statHash = pStatUI->GetStatHash(0);
				atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
				if(tagData)
				{
					int divisor = GetReplacementLocalizedHash(statHash, sData);
					::sprintf(outBuffer,"%d / %d", sData, divisor);
				}
				else
				{
					::sprintf(outBuffer,"%d", sData);
				}
				break;
			}
		case eStatType_Checklist:
			{
				u32 statHash = pStatUI->GetStatHash(0);
				int iNumerator = 0;
				int iDenominator = 0;
				atArray<sUIReplacementTag>* tagData = m_ReplacementHashes.Access(statHash);
				if(tagData && tagData->GetCount() == 2)
				{
					u32 uNumeratorHash = (*tagData)[0].m_uHash;
					u32 uDenominatorHash = (*tagData)[1].m_uHash;

					iNumerator = StatsInterface::GetIntStat(uNumeratorHash);
					iDenominator = StatsInterface::GetIntStat(uDenominatorHash);
				}

				::sprintf(outBuffer,"%d / %d", iNumerator, iDenominator);
				break;
			}
		default:
		{
			AssertMsg(0,"Unhandled Type.");
			break;
		}
	}
		
}

void CFrontendStatsMgr::FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, u32 uData)
{
	switch(pStatUI->GetType())
	{
		case eStatType_Int:
		{
			int iData = (int)uData;
			char humanIntBuff[32];
			int stringSize = sizeof(humanIntBuff)/sizeof(humanIntBuff[0]);
			CTextConversion::FormatIntForHumans(iData, humanIntBuff, stringSize, "%s");
			::sprintf(outBuffer,"%s",humanIntBuff);
			break;
		}
		case eStatType_Float:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f",fData);
			break;
		}
		case eStatType_Percent:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f%%",fData);
			break;
		}
		case eStatType_Distance:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? METERS_TO_KILOMETERS(fData) : METERS_TO_MILES(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KILO") : TheText.Get("UNIT_MILES"));
			break;
		}
		case eStatType_ShortDistance:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? fData : METERS_TO_FEET(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_METERS") : TheText.Get("UNIT_FEET"));
			break;
		}
		case eStatType_Speed:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f%s",
				ShouldUseMetric() ? fData : KPH_TO_MPH(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KPH") : TheText.Get("UNIT_MPH"));
			break;
		}
		case eStatType_Time:
		{	
			CStatsUtils::GetTime((s32)uData, outBuffer, bufferSize, true);
			break;
		}
		case eStatType_Cash:
		{
			s64 iData = (int)uData;
			char cashBuff[32];
			FormatInt64ToCash(iData, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));
			::sprintf(outBuffer,"%s",cashBuff);
			break;
		}
		case eStatType_NameHash:
		{
			u32 hashData = uData ? uData : ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
			::sprintf(outBuffer, "%s", TheText.Get(hashData, "NAME_HASH"));
			break;
		}
		case eStatType_CarHash:
		{
			::sprintf(outBuffer, "%s", GetVehicleNameFromHashKey(uData));
			break;
		}
		case eStatType_Bool:
		{
			bool bData = uData ? true : false;
			if (bData)
				::sprintf(outBuffer,"%s",TheText.Get("MO_YES"));
			else
				::sprintf(outBuffer,"%s",TheText.Get("MO_NO"));

			break;
		}
		case eStatType_Date:
		{

			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate((u64)uData, year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(outBuffer, "%04d / %02d / %02d",year,month,day);

			break;
		}
		case eStatType_Bar:
		{
			::sprintf(outBuffer,"");
			break;
		}

		default:
		{
			AssertMsg(0,"Unhandled Type.");
			break;
		}
	}
}

void CFrontendStatsMgr::FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, bool bData)
{
	switch(pStatUI->GetType())
	{
	case eStatType_Int:
		{
			int iData = bData ? 1 : 0;

			::sprintf(outBuffer,"%d",iData);
			break;
		}
	case eStatType_Float:
		{
			float fData = bData ? 1.0f : 0.0f;
			::sprintf(outBuffer,"%.2f",fData);
			break;
		}
	case eStatType_Percent:
		{
			float fData = bData ? 1.0f : 0.0f;
			::sprintf(outBuffer,"%.2f%%",fData);
			break;
		}
	case eStatType_Distance:
		{
			float fData = bData ? 1.0f : 0.0f;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? METERS_TO_KILOMETERS(fData) : METERS_TO_MILES(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KILO") : TheText.Get("UNIT_MILES"));
			break;
		}
	case eStatType_ShortDistance:
		{
			float fData = bData ? 1.0f : 0.0f;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? fData : METERS_TO_FEET(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_METERS") : TheText.Get("UNIT_FEET"));
			break;
		}
	case eStatType_Speed:
		{
			float fData = bData ? 1.0f : 0.0f;
			::sprintf(outBuffer,"%.2f%s",
				ShouldUseMetric() ? fData : KPH_TO_MPH(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KPH") : TheText.Get("UNIT_MPH"));
			break;
		}
	case eStatType_Time:
		{	
			s32 sData = bData ? 1 : 0;
			CStatsUtils::GetTime(sData, outBuffer, bufferSize, true);
			break;
		}
	case eStatType_Cash:
		{
			s64 iData = bData ? 1 : 0;
			char cashBuff[32];
			FormatInt64ToCash(iData, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));
			::sprintf(outBuffer,"%s",cashBuff);
			break;
		}
	case eStatType_NameHash:
		{
			u32 uData = bData ? (u32)bData : ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
			::sprintf(outBuffer, "%s", TheText.Get(uData, "NAME_HASH"));
			break;
		}
	case eStatType_CarHash:
		{
			u32 uData = (u32)bData;
			::sprintf(outBuffer, "%s", GetVehicleNameFromHashKey(uData));
			break;
		}
	case eStatType_Date:
		{
			u64 uData = (u64)bData;
			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate(uData, year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(outBuffer, "%04d / %02d / %02d",year,month,day);

			break;
		}
	case eStatType_Bool:
		{
			if (bData)
				::sprintf(outBuffer,"%s",TheText.Get("MO_YES"));
			else
				::sprintf(outBuffer,"%s",TheText.Get("MO_NO"));

			break;
		}
	case eStatType_Bar:
	{
		::sprintf(outBuffer,"");
		break;
	}
	default:
		{
			AssertMsg(0,"Unhandled Type.");
			break;
		}
	}
}

void CFrontendStatsMgr::FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, u64 uData)
{
	switch(pStatUI->GetType())
	{
	case eStatType_Int:
		{
			int iData = (int)uData;
			char humanIntBuff[32];
			int stringSize = sizeof(humanIntBuff)/sizeof(humanIntBuff[0]);
			CTextConversion::FormatIntForHumans(iData, humanIntBuff, stringSize, "%s");
			::sprintf(outBuffer,"%s",humanIntBuff);
			break;
		}
	case eStatType_Float:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f",fData);
			break;
		}
	case eStatType_Percent:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f%%",fData);
			break;
		}
	case eStatType_Distance:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? METERS_TO_KILOMETERS(fData) : METERS_TO_MILES(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KILO") : TheText.Get("UNIT_MILES"));
			break;
		}
	case eStatType_ShortDistance:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? fData : METERS_TO_FEET(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_METERS") : TheText.Get("UNIT_FEET"));
			break;
		}
	case eStatType_Speed:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f%s",
				ShouldUseMetric() ? fData : KPH_TO_MPH(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KPH") : TheText.Get("UNIT_MPH"));
			break;
		}
	case eStatType_Time:
		{	
			CStatsUtils::GetTime(uData, outBuffer, bufferSize, true);
			break;
		}
	case eStatType_Cash:
		{
			s64 iData = (int)uData;
			char cashBuff[32];
			FormatInt64ToCash(iData, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));
			::sprintf(outBuffer,"%s",cashBuff);
			break;
		}
	case eStatType_NameHash:
		{
			u32 hashData = uData ? (u32)uData : ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
			::sprintf(outBuffer, "%s", TheText.Get(hashData, "NAME_HASH"));
			break;
		}
	case eStatType_CarHash:
		{
			::sprintf(outBuffer, "%s", GetVehicleNameFromHashKey((u32)uData));
			break;
		}
	case eStatType_Bool:
		{
			bool bData = uData ? true : false;
			if (bData)
				::sprintf(outBuffer,"%s",TheText.Get("MO_YES"));
			else
				::sprintf(outBuffer,"%s",TheText.Get("MO_NO"));

			break;
		}
	case eStatType_Date:
		{
			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate(uData, year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(outBuffer, "%04d / %02d / %02d",year,month,day);

			break;
		}
	case eStatType_Bar:
		{
			::sprintf(outBuffer,"");
			break;
		}
	default:
		{
			AssertMsg(0,"Unhandled Type.");
			break;
		}
	}
}

void CFrontendStatsMgr::FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, s64 uData)
{
	switch(pStatUI->GetType())
	{
	case eStatType_Int:
		{
			int iData = (int)uData;
			char humanIntBuff[32];
			int stringSize = sizeof(humanIntBuff)/sizeof(humanIntBuff[0]);
			CTextConversion::FormatIntForHumans(iData, humanIntBuff, stringSize, "%s");
			::sprintf(outBuffer,"%s",humanIntBuff);
			break;
		}
	case eStatType_Float:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f",fData);
			break;
		}
	case eStatType_Percent:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f%%",fData);
			break;
		}
	case eStatType_Distance:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? METERS_TO_KILOMETERS(fData) : METERS_TO_MILES(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KILO") : TheText.Get("UNIT_MILES"));
			break;
		}
	case eStatType_ShortDistance:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? fData : METERS_TO_FEET(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_METERS") : TheText.Get("UNIT_FEET"));
			break;
		}
	case eStatType_Speed:
		{
			float fData = (float)uData;
			::sprintf(outBuffer,"%.2f%s",
				ShouldUseMetric() ? fData : KPH_TO_MPH(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KPH") : TheText.Get("UNIT_MPH"));
			break;
		}
	case eStatType_Time:
		{	
			CStatsUtils::GetTime((u64)uData, outBuffer, bufferSize, true);
			break;
		}
	case eStatType_Cash:
		{
			s64 iData = (int)uData;
			char cashBuff[32];
			FormatInt64ToCash(iData, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));
			::sprintf(outBuffer,"%s",cashBuff);
			break;
		}
	case eStatType_NameHash:
		{
			u32 hashData = uData ? (u32)uData : ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
			::sprintf(outBuffer, "%s", TheText.Get(hashData, "NAME_HASH"));
			break;
		}
	case eStatType_CarHash:
		{
			::sprintf(outBuffer, "%s", GetVehicleNameFromHashKey((u32)uData));
			break;
		}
	case eStatType_Bool:
		{
			bool bData = uData ? true : false;
			if (bData)
				::sprintf(outBuffer,"%s",TheText.Get("MO_YES"));
			else
				::sprintf(outBuffer,"%s",TheText.Get("MO_NO"));

			break;
		}
	case eStatType_Date:
		{
			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate(uData, year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(outBuffer, "%04d / %02d / %02d",year,month,day);

			break;
		}
	case eStatType_Bar:
		{
			::sprintf(outBuffer,"");
			break;
		}
	default:
		{
			AssertMsg(0,"Unhandled Type.");
			break;
		}
	}
}

void CFrontendStatsMgr::FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, float fData)
{
	switch(pStatUI->GetType())
	{
		case eStatType_Int:
		{
			int iData = (int)fData;
			char humanIntBuff[32];
			int stringSize = sizeof(humanIntBuff)/sizeof(humanIntBuff[0]);
			CTextConversion::FormatIntForHumans(iData, humanIntBuff, stringSize, "%s");
			::sprintf(outBuffer,"%s",humanIntBuff);
			break;
		}
		case eStatType_Float:
		{
			::sprintf(outBuffer,"%.2f",fData);
			break;
		}
		case eStatType_Percent:
			{
				::sprintf(outBuffer,"%.2f%%",fData);
				break;
			}
		case eStatType_Distance:
		{
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? METERS_TO_KILOMETERS(fData) : METERS_TO_MILES(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KILO") : TheText.Get("UNIT_MILES"));
			break;
		}
		case eStatType_ShortDistance:
		{
			::sprintf(outBuffer,"%.2f %s",
				ShouldUseMetric() ? fData : METERS_TO_FEET(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_METERS") : TheText.Get("UNIT_FEET"));
			break;
		}
		case eStatType_Speed:
		{
			::sprintf(outBuffer,"%.2f%s",
				ShouldUseMetric() ? fData : KPH_TO_MPH(fData),
				ShouldUseMetric() ? TheText.Get("UNIT_KPH") : TheText.Get("UNIT_MPH"));
			break;
		}
		case eStatType_Time:
		{
			s32 sData = (s32)fData;
			CStatsUtils::GetTime(sData, outBuffer, bufferSize, true);
			break;
		}
		case eStatType_Cash:
		{
			s64 iData = (int)fData;
			char cashBuff[32];
			FormatInt64ToCash(iData, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));
			::sprintf(outBuffer,"%s",cashBuff);
			break;
		}
		case eStatType_NameHash:
		{
			u32 uData = fData ? (u32)fData : ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A);
			::sprintf(outBuffer, "%s", TheText.Get(uData, "NAME_HASH"));
			break;
		}
		case eStatType_CarHash:
		{
			u32 uData = (u32)fData;
			::sprintf(outBuffer, "%s", GetVehicleNameFromHashKey(uData));
			break;
		}
		case eStatType_Bool:
		{
			bool bData = (fData >= 1.0f)? true : false;

			if (bData)
				::sprintf(outBuffer,"%s",TheText.Get("MO_YES"));
			else
				::sprintf(outBuffer,"%s",TheText.Get("MO_NO"));

			break;
		}
		case eStatType_Date:
		{
			u64 uData = (u64)fData;
			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate(uData, year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(outBuffer, "%04d / %02d / %02d",year,month,day);

			break;
		}
		case eStatType_Bar:
		{
			::sprintf(outBuffer,"");
			break;
		}
		default:
		{
			AssertMsg(0,"Unhandled Type.");
			break;
		}
	}
}

void CFrontendStatsMgr::FormatDataAsType( char* outBuffer, u32 bufferSize, char* pData)
{
	formatf(outBuffer, bufferSize, "%s", pData);
}

StatType CFrontendStatsMgr::GetStatTypeFromXML(const char* name)
{

	int tagType = eStatType_Invalid;

	if (AssertVerify(name))
	{
		if( stricmp(name, "int") == 0 )
		{
			tagType = eStatType_Int;
		}
		else if( stricmp(name, "float") == 0 )
		{
			tagType = eStatType_Float;
		}
		else if( stricmp(name, "percent") == 0 )
		{
			tagType = eStatType_Percent;
		}
		else if( stricmp(name, "time") == 0 )
		{
			tagType = eStatType_Time;
		}
		else if( stricmp(name, "date") == 0 )
		{
			tagType = eStatType_Date;
		}
		else if( stricmp(name, "distance") == 0 )
		{
			tagType = eStatType_Distance;
		}
		else if (stricmp(name, "short_distance") == 0)
		{
			tagType = eStatType_ShortDistance;
		}
		else if( stricmp(name, "speed") == 0 )
		{
			tagType = eStatType_Speed;
		}
		else if( stricmp(name, "bool") == 0 )
		{
			tagType = eStatType_Bool;
		}
		else if( stricmp(name, "bar") == 0 )
		{
			tagType = eStatType_Bar;
		}
		else if( stricmp(name, "playerid") == 0 )
		{
			tagType = eStatType_PlayerID;
		}
		else if (stricmp(name, "cash") == 0)
		{
			tagType = eStatType_Cash;
		}
		else if (stricmp(name, "namehash") == 0)
		{
			tagType = eStatType_NameHash;
		}
		else if (stricmp(name, "carhash") == 0)
		{
			tagType = eStatType_CarHash;
		}
		else if (stricmp(name, "fraction") == 0)
		{
			tagType = eStatType_Fraction;
		}
		else if (stricmp(name, "checklist") == 0)
		{
			tagType = eStatType_Checklist;
		}
	}

	return tagType;
}

void CFrontendStatsMgr::FormatIntToCash(int cash, char* cashString, int cashStringSize, bool bUseCommas)
{
	CTextConversion::FormatIntForHumans(cash, cashString, cashStringSize, "$%s", bUseCommas);
}

void CFrontendStatsMgr::FormatInt64ToCash(s64 cash, char* cashString, int cashStringSize, bool bUseCommas)
{
	CTextConversion::FormatIntForHumans(cash, cashString, cashStringSize, "$%s", bUseCommas);
}

bool CFrontendStatsMgr::GetVisibilityFromRule(const sUIStatData* pUIStat, const sStatDataPtr* pActualStat)
{
	const sStatData* pStat = *pActualStat;
	if(!pUIStat || !pActualStat || !pStat)
	{
		Assertf(false, "Bad stat type exists in the frontend stats manager.");
		return false;
	}

	int iVisibilityRule = pUIStat->GetVisibility();
	switch (iVisibilityRule)
	{
	case eVisibility_AlwaysShow:
		return true;
	case eVisibility_NeverShown:
		return false;
	case eVisibility_AfterIncrement:
		switch (pStat->GetType())
		{
		case STAT_TYPE_INT:
			return StatsInterface::GetIntStat(pActualStat->GetKey()) > 0;
		case STAT_TYPE_BOOLEAN:
			return StatsInterface::GetBooleanStat(pActualStat->GetKey()) != false;
		case STAT_TYPE_PERCENT:
		case STAT_TYPE_UINT32:
			return StatsInterface::GetUInt32Stat(pActualStat->GetKey()) > 0;
		case STAT_TYPE_PACKED:
		case STAT_TYPE_UINT64:
		case STAT_TYPE_DATE:
			return StatsInterface::GetUInt64Stat(pActualStat->GetKey()) > 0;
		case STAT_TYPE_INT64:
			return StatsInterface::GetInt64Stat(pActualStat->GetKey()) > 0;
		case STAT_TYPE_FLOAT:
			return StatsInterface::GetFloatStat(pActualStat->GetKey()) > 0.0f;
		default:
			return false;
		}
	default:
		return EXTRACONTENT.IsContentPresent((u32)iVisibilityRule);
	}
}

bool CFrontendStatsMgr::ShouldUseMetric()
{	
	const int METRIC_SYSTEM = 1;
	return CPauseMenu::GetMenuPreference(PREF_MEASUREMENT_SYSTEM) == METRIC_SYSTEM;
}

u32 CFrontendStatsMgr::GetReplacementLocalizedHash(u32 statHash, int iValue)
{
	u32 uHash = 0;
	
	if(m_ReplacementHashes.Access(statHash))
	{
		atArray<sUIReplacementTag>* tags = m_ReplacementHashes.Access(statHash);
#if __ASSERT
		bool bFoundReplacement = false;
#endif	//	__ASSERT
		for(int i = 0; i < tags->GetCount(); ++i)
		{
			if( iValue >= (*tags)[i].m_iMin &&
				iValue <= (*tags)[i].m_iMax )
			{
				uHash = (*tags)[i].m_uHash;
#if __ASSERT
				bFoundReplacement = true;
#endif	//	__ASSERT
			}
		}
		Assertf(bFoundReplacement, "CFrontendStatsMgr::GetReplacementLocalizedHash - failed to find a replacement for stat with hash %u. value = %d", statHash, iValue);
	}
	else
	{
		Assertf(false, "statHash is invalid. Probably because this stat does not use replacement labels.");
	}

	return uHash;
}

const char* CFrontendStatsMgr::GetVehicleNameFromHashKey(u32 uVehicleModelHashKey)
{
	if(uVehicleModelHashKey == 0)
	{
		return TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "VEHICLEN_NAME");
	}

	CBaseModelInfo* mi = CModelInfo::GetBaseModelInfoFromHashKey(uVehicleModelHashKey, NULL);
	if(uiVerifyf(mi, "Invalid Vehicle Model Hash %d", uVehicleModelHashKey))
	{
		if(AssertVerify(mi->GetModelType() == MI_TYPE_VEHICLE))
		{
			return TheText.Get(atStringHash(((CVehicleModelInfo*)mi)->GetGameName()), "VEHICLEN_NAME");
		}
	}

	return TheText.Get(ATSTRINGHASH("SM_LCNONE", 0xD7DE0F3A), "VEHICLEN_NAME");
}

int CFrontendStatsMgr::GetVisibilityFromXML(const char* name)
{

	int tagType = eVisibility_AlwaysShow;

	if (AssertVerify(name))
	{
		if( stricmp(name, "ALWAYS_VISIBLE") == 0 )
		{
			tagType = eVisibility_AlwaysShow;
		}
		else if( stricmp(name, "NEVER_SHOW") == 0 )
		{
			tagType = eVisibility_NeverShown;
		}
		else if( stricmp(name, "AFTER_INCREMENT") == 0 )
		{
			tagType = eVisibility_AfterIncrement;
		}
		else if( stricmp(name, "PC_ONLY") == 0 )
		{
#if RSG_PC
			tagType = eVisibility_AlwaysShow;
#else
			tagType = eVisibility_NeverShown;
#endif // RSG_PC
		}
		else if( stricmp(name, "CONSOLE_ONLY") == 0 )
		{
#if RSG_PC
			tagType = eVisibility_NeverShown;
#else
			tagType = eVisibility_AlwaysShow;
#endif // RSG_PC
		}
		else
		{
			tagType = atStringHash(name);
		}
	}

	return tagType;
}

#if RSG_DURANGO || RSG_ORBIS
void CFrontendStatsMgr::RequestDisplayNameLookup()
{
	CancelDisplayNameLookup();
	m_bLookupRequired = true;
}

void CFrontendStatsMgr::CancelDisplayNameLookup()
{
	// Handle Canceled Requests
	if (m_iLookupId != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
	{
		CLiveManager::GetFindDisplayName().CancelRequest(m_iLookupId);
		m_iLookupId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	}

	m_bLookupRequired = false;
	m_iLookupId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	m_iNumRequests = 0;
}
#endif // RSG_DURANGO || RSG_ORBIS

CFrontendStatsMgr::UIStatsList::const_iterator
CFrontendStatsMgr::SPStatsUiConstIterBegin()
{
	return m_aSPStatsData.begin();
}

CFrontendStatsMgr::UIStatsList::const_iterator
CFrontendStatsMgr::SPStatsUiConstIterEnd()
{
	return m_aSPStatsData.end();
}

CFrontendStatsMgr::UIStatsList::const_iterator
CFrontendStatsMgr::MPStatsUiConstIterBegin()
{
	return m_aMPStatsData.begin();
}

CFrontendStatsMgr::UIStatsList::const_iterator
CFrontendStatsMgr::MPStatsUiConstIterEnd()
{
	return m_aMPStatsData.end();
}

CFrontendStatsMgr::UICategoryList::const_iterator
CFrontendStatsMgr::CategoryConstIterBegin()
{
	return m_aCategoryList.begin();
}

CFrontendStatsMgr::UICategoryList::const_iterator
CFrontendStatsMgr::CategoryConstIterEnd()
{
	return m_aCategoryList.end();
}

int statSort(sUIStatData* const* a, sUIStatData* const* b)
{
	statAssertf(TheText.DoesTextLabelExist((*a)->GetDescriptionHash()), "Stat Text label is missing \"%d:%s\"", (*a)->GetDescriptionHash(), StatsInterface::GetKeyName((*a)->GetStatHash(0)));
	statAssertf(TheText.DoesTextLabelExist((*b)->GetDescriptionHash()), "Stat Text label is missing \"%d:%s\"", (*b)->GetDescriptionHash(), StatsInterface::GetKeyName((*b)->GetStatHash(0)));

	const char* szLocalizedString1 = TheText.Get((*a)->GetDescriptionHash(), "HASH");
	const char* szLocalizedString2 = TheText.Get((*b)->GetDescriptionHash(), "HASH");

	int iMaxLength = Min(CTextConversion::GetCharacterCount(szLocalizedString1), CTextConversion::GetCharacterCount(szLocalizedString2));
	
	for (int i = 0; i < iMaxLength; i++)
	{
		if (szLocalizedString1[i] > szLocalizedString2[i] )
			return 1;
		else if (szLocalizedString1[i] < szLocalizedString2[i] )
			return -1;
	}

	return -1;
}

