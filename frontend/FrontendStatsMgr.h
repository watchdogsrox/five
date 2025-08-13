//
// StatsDataMgr.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef FRONTENDSTATSMGR_H
#define FRONTENDSTATSMGR_H

#include "atl/array.h"
#include "Stats/StatsTypes.h"
#include "Stats/StatsInterface.h"
#include "Text/TextFile.h"

// --- Forward Definitions ------------------------------------------------------
namespace rage
{
	class  parTreeNode;
	class  rlProfileStatsValue;
}


enum eUIVisibilityRules
{
	eVisibility_Invalid = -1,
	eVisibility_NeverShown,
	eVisibility_AlwaysShow,
	eVisibility_AfterIncrement,
	eVisibility_Max = eVisibility_AfterIncrement
};

enum eUIStatTypes
{
	eStatType_Invalid = -1,
	eStatType_Int,
	eStatType_Float,
	eStatType_Bool,
	eStatType_Time,
	eStatType_Date,
	eStatType_Distance,
	eStatType_ShortDistance,
	eStatType_Speed,
	eStatType_Percent,
	eStatType_Bar,
	eStatType_Cash,
	eStatType_NameHash,
	eStatType_CarHash,
	eStatType_Fraction,
	eStatType_Checklist,
	eStatType_PlayerID,
	eStatType_MaxTypes = eStatType_PlayerID,
};

struct sUIReplacementTag
{
	sUIReplacementTag()
	{
		m_iMin = 0;
		m_iMax = 0;
		m_uHash = 0;
		m_bUseLabelAndValue = false;
	}

	sUIReplacementTag(int iMin, int iMax, u32 uHash, bool bLabelAndValue)
		: m_iMin(iMin)
		, m_iMax(iMax)
		, m_uHash(uHash)
		, m_bUseLabelAndValue(bLabelAndValue)
	{
		Assertf(iMin <= iMax, "RANGE ERROR: MIN must be less than or equal to MAX");
	}

	int m_iMin;
	int m_iMax;
	u32 m_uHash;
	bool m_bUseLabelAndValue;
};

struct sUIAsyncCacheData
{
	rlGamerHandle m_hGamer;
	char m_UID[128];
	char m_Name[128];

	bool m_bChecking;
	bool m_bReady;
};


struct sUICategoryData 
{
	sUICategoryData()
	{
		m_bSinglePlayer = true;
		m_Name[0] = '0';
	}

	sUICategoryData(const char* szName, bool bSingleplayer)
	{
		strncpy(m_Name,szName,16);

		m_Name[15] = '0';
		m_bSinglePlayer = bSingleplayer;
	}

	char		m_Name[16];
	bool		m_bSinglePlayer;
};

// Defines / Stores the UI properties of a stat.
class sUIStatData
{
	public:
		sUIStatData() {}
		sUIStatData(const char* pName, const char* pDescription, u8 uCategory, int uVisible,  StatType uType);
		virtual ~sUIStatData() {}
		
		// PURPOSE: Returns the stat id as a int hash of the name id.
		int GetBaseHash() const { return m_uHash; }
		virtual int GetStatHash(int) const { return m_uHash; }
		virtual int	GetDescriptionHash() const	{ return m_uLocalizedKeyHash; }
		virtual u32 GetDetailsHash(int) const	{ return 0; }
		virtual u8 GetCategory() const			{ return m_uCategory;}
		virtual StatType GetType() const		{ return m_uStatType; }
		virtual int GetVisibility() const		{ return m_iVisibilityRule; }

	protected:
		int			m_uHash;
		int			m_uLocalizedKeyHash;
		int			m_iVisibilityRule;
		u8			m_uCategory;
		StatType	m_uStatType;
};

class sUIStatDataSP : public sUIStatData
{
public:

	struct statNameList
	{
		
		statNameList()
		{
			sysMemSet(szName[0],0,sizeof(char)*MAX_STAT_LABEL_SIZE * STAT_SP_CATEGORY_CHAR2);
		}

		char szName[STAT_SP_CATEGORY_CHAR2][MAX_STAT_LABEL_SIZE];

	};

	sUIStatDataSP(statNameList* pStatList, const char* pDescription, u8 uCategory, int uVisible,  StatType uType);
	~sUIStatDataSP() {}

	int GetStatHash(int i) const;
 
private:
	u32 m_iStatHash[2];
};

class sUIStatDataSPDetailed : public sUIStatDataSP
{
public:
	sUIStatDataSPDetailed(statNameList* pStatList, const char* pDescription, const char* pDetails, u8 uCategory, int uVisible,  StatType uType);
	~sUIStatDataSPDetailed() {}

	u32 GetDetailsHash(int i) const { return m_uDetailHashes[i]; };

private:
	u32 m_uDetailHashes[3];
};

class sUIStatDataMP : public sUIStatData
{
public:
	struct statNameList
	{

		statNameList()
		{
			sysMemSet(szName[0], 0, sizeof(char)*MAX_STAT_LABEL_SIZE * MAX_NUM_MP_ACTIVE_CHARS);
		}

		char szName[MAX_NUM_MP_ACTIVE_CHARS][MAX_STAT_LABEL_SIZE];
	};

	sUIStatDataMP();
	sUIStatDataMP(statNameList* pNames, const char* pDescription, u8 uCategory, int uVisible,  StatType uType);
	~sUIStatDataMP() {}

	int GetStatHash(int i) const;

private:
	u32 m_iStatHash[MAX_NUM_MP_ACTIVE_CHARS];
};

class CFrontendStatsMgr 
{
private:
	static int GetCategoryFromName(const char* name);
	static int GetVisibilityFromXML(const char* pVisibility);
	static StatType GetStatTypeFromXML(const char* pStatType);

	static sUIStatData* RegisterNewStat(const char* pName
		,const char* pDescription
		,const char* pDetails
		,u8 uCategory
		,int uVisible
		,StatType uStatType
		,atArray<sUIReplacementTag>& tags
		,bool bSinglePlayer);

	static void RegisterNewStatForEachCharacter(const char* pName
		,const char* pDescription
		,const char* pDetails
		,u8 uCategory
		,int uVisible
		,StatType uStatType
		,atArray<sUIReplacementTag>& tags
		,bool bSinglePlayer);


public: 


	CFrontendStatsMgr();
	~CFrontendStatsMgr();

	static void Update();
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void LoadDataXMLFile(const unsigned initMode);
	static void LoadDataXMLFile(const char* cFileName, bool bSinglePlayer);
	static void LoadStatsData(parTreeNode* pNode, bool bSinglePlayer);

	//All stats
	typedef atArray<sUIStatData*> UIStatsList;
	typedef atArray<sUICategoryData*> UICategoryList;
	typedef atMap< u32, atArray<sUIReplacementTag> > UIReplacementLabelMap;
	typedef atMap<u32, sUIAsyncCacheData> UIStatNameCache;
	typedef atArray<u32> MountedFilesHashList;

	static UICategoryList::const_iterator CategoryConstIterBegin();
	static UICategoryList::const_iterator CategoryConstIterEnd();

	static UIStatsList::const_iterator SPStatsUiConstIterBegin();
	static UIStatsList::const_iterator SPStatsUiConstIterEnd();

	static UIStatsList::const_iterator MPStatsUiConstIterBegin();
	static UIStatsList::const_iterator MPStatsUiConstIterEnd();

	static void GetDataAsString(const sStatDataPtr* ppStat, const sUIStatData* pStatUI, char* pOutString, u32 bufferSize);
	static float GetDataAsPercent(const sStatDataPtr* ppStat, const sUIStatData* pStatUI);
	static void FormatIntToCash(int cash, char* cashString, int cashStringSize, bool bUseCommas = true);
	static void FormatInt64ToCash(s64 cash, char* cashString, int cashStringSize, bool bUseCommas = true);
	static bool GetVisibilityFromRule(const sUIStatData* pStat, const sStatDataPtr* pActualStat);
	static UIStatNameCache& GetAsyncCache() { return m_AsyncStatCache; }
	static MountedFilesHashList& GetMountedFileList() { return m_MountedFiles; }

	static bool ShouldUseMetric();

	static UIStatsList& GetSkillsStats() { return m_aSPSkillStats; }

#if RSG_DURANGO || RSG_ORBIS
	static void RequestDisplayNameLookup();
	static void CancelDisplayNameLookup();
#endif // RSG_DURANGO

private:

	static bool GetUsesReplacementLabel(u32 statHash) { return m_ReplacementHashes.Access(statHash) != NULL; }
	static u32 GetReplacementLocalizedHash(u32 statHash, int iValue);
	static const char* GetVehicleNameFromHashKey(u32 uVehicleModelHashKey);

	static void FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, s32 sData);
	static void FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, u32 sData);
	static void FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, u64 sData);
	static void FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, s64 sData);
	static void FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, float sData);
	static void FormatDataAsType(char* outBuffer, u32 bufferSize, char* pData);
	static void FormatDataAsType(const sUIStatData* pStatUI, char* outBuffer, u32 bufferSize, bool bData);

	//Store data for all stats
	static UIStatsList m_aSPStatsData;
	static UIStatsList m_aMPStatsData;
	static UIStatsList m_aSPSkillStats;
	static UICategoryList m_aCategoryList;

	static UIReplacementLabelMap m_ReplacementHashes;

	// Cache for stats that require async lookups
	static UIStatNameCache m_AsyncStatCache;
#if RSG_DURANGO || RSG_ORBIS
	static bool m_bLookupRequired;
	static int m_iLookupId;
	static int m_iNumRequests;
#endif // RSG_DURANGO

	// For DLC ensure that we only mount additional stats data once
	static MountedFilesHashList m_MountedFiles;
};
#endif // FRONTENDSTATSMGR_H

// EOF
