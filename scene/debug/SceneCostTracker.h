/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneCostTracker.h
// PURPOSE : debug widgets for tracking cost of a scene
// AUTHOR :  Ian Kiigan
// CREATED : 08/06/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_SCENECOSTTRACKER_H_
#define _SCENE_DEBUG_SCENECOSTTRACKER_H_

#if __BANK

#include "atl/map.h"
#include "renderer/PostScan.h"
#include "vector/vector2.h"
#include "vector/color32.h"
#include "fwscene/lod/LodTypes.h"
#include "streaming/streamingdefs.h"

class CEntity;

namespace rage {
class fwEntityDesc;
class rmcDrawable;
}

// choice of source list types
enum
{
	SCENECOSTTRACKER_LIST_PVS = 0,
	SCENECOSTTRACKER_LIST_SPHERE,
	SCENECOSTTRACKER_LIST_PSS_ALL,
	SCENECOSTTRACKER_LIST_PSS_NOTLOADED,

	SCENECOSTTRACKER_LIST_TOTAL
};

enum
{
	SCENECOSTTRACKER_MEMTYPE_PHYS,
	SCENECOSTTRACKER_MEMTYPE_VIRT	
};

enum
{
	SCENECOSTTRACKER_BUFFER_UPDATE = 0,
	SCENECOSTTRACKER_BUFFER_RENDER,

	SCENECOSTTRACKER_BUFFER_TOTAL
};

enum
{
	SCENECOSTTRACKER_COST_LIST_TXD = 0,
	SCENECOSTTRACKER_COST_LIST_DWD,
	SCENECOSTTRACKER_COST_LIST_DRAWABLE,
	SCENECOSTTRACKER_COST_LIST_FRAG,
	SCENECOSTTRACKER_COST_LIST_PTFX,
	SCENECOSTTRACKER_COST_LIST_TOTAL,

	SCENECOSTTRACKER_COST_TOTAL
};

enum
{
	SCENECOSTTRACKER_COUNTS_SHADERS = 0,

	SCENECOSTTRACKER_COUNTS_TOTAL
};

enum
{
	SCENECOSTTRACKER_BARTYPE_IMPORTANT = 0,
	SCENECOSTTRACKER_BARTYPE_REGULAR,
};

class CSceneCostTrackerStatsBuffer
{
public:
	void					Reset();
	void					IncrementCost(u32 lodType, u32 eCost, u32 nPhysical, u32 nVirtual, bool bFiltered);
	void					IncrementCount(u32 eCount, u32 nNum, bool bFiltered);
	inline u32				GetVirtCost(u32 eCost);
	inline u32				GetPhysCost(u32 eCost);
	inline u32				GetVirtCostPeak(u32 eCost);
	inline u32				GetPhysCostPeak(u32 eCost);
	inline u32				GetCount(u32 eCount);
	inline u32				GetCountPeak(u32 eCount);
	inline u32				GetVirtCostFiltered(u32 eCost);
	inline u32				GetPhysCostFiltered(u32 eCost);
	inline u32				GetCountFiltered(u32 eCount);

private:
	u32						m_anCostVirt[SCENECOSTTRACKER_COST_TOTAL];
	u32						m_anCostPhys[SCENECOSTTRACKER_COST_TOTAL];
	u32						m_anCostVirtPeak[SCENECOSTTRACKER_COST_TOTAL];
	u32						m_anCostPhysPeak[SCENECOSTTRACKER_COST_TOTAL];
	u32						m_anCount[SCENECOSTTRACKER_COUNTS_TOTAL];
	u32						m_anCountPeak[SCENECOSTTRACKER_COUNTS_TOTAL];
	u32						m_anCostVirtFiltered[SCENECOSTTRACKER_COST_TOTAL];
	u32						m_anCostPhysFiltered[SCENECOSTTRACKER_COST_TOTAL];
	u32						m_anCountFiltered[SCENECOSTTRACKER_COUNTS_TOTAL];

public:
	u32						m_anPerLodTotalVirt[LODTYPES_MAX_NUM_LEVELS+1];
	u32						m_anPerLodTotalPhys[LODTYPES_MAX_NUM_LEVELS+1];
};

class CSceneCostTracker
{
public:
	void					AddWidgets(bkBank* pBank);
	void					AddCostsForPvsList(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop);
	void					AddCostsForSphere();
	void					Update();
	void					DrawDebug();
	static inline bool		GetShowBudgets(u32 eListType);
	s32						GetCostTitleStr(char* pszResult, const char* pszTitle, u32 eStat, u32 eMemType);
	void					SetJunctionCaptureMode(bool bEnabled=true);
	void					WriteToLog(const char* pszFileName);
	u32						GetTotalCost(bool bVram);

private:
	void					ResetThisFrame();
	static inline bool		FilteringResults();
	bool					IsFilteredEntity(CEntity* pEntity);
	void					DrawTitle(u32* x, u32* y, const char* pszText, u32 width);
	void					DrawFilteredByMsg(u32 x, u32 y);
	void					DrawMemBar(u32& x, u32& y, const u32 nStat, const u32 nPeak, const u32 nFilteredStat, const u32 nMax, const u32 eBarType);
	void					DrawCountBar(u32& x, u32& y, const u32 nStat, const u32 nFilteredStat, const u32 nMax, const u32 eBarType);
	void					DrawLodBreakdown(u32 x, u32 y, u32* pData, float fMaxMem);
	void					DrawLegend(u32 x, u32 y);
	void					AddCostsForPvsEntry(CGtaPvsEntry* pPvsEntry);
	void					AddCostsForEntity(CEntity* pEntity);
	void					AddCostsForZonedAsset(strIndex modelIndex);
	u32						GetShaderCount(CEntity* pEntity,rmcDrawable *pDrawable);
	
	static bool				AppendToListCB(CEntity* pEntity, void* pData);
	static void				SaveToLogCB();
	static void				SelectLogFileCB();
	static s32				SortByNameCB(const strIndex* pIndex1, const strIndex* pIndex2);

	static void				StartMemCheckCB();
	static void				StopMemCheckCB();

	CSceneCostTrackerStatsBuffer	m_sStatBuffers[SCENECOSTTRACKER_BUFFER_TOTAL];
	atMap<s32, u32>			m_txdMap;
	atMap<s32, u32>			m_dwdMap;
	atMap<s32, u32>			m_drawableMap;
	atMap<s32, u32>			m_fragMap;
	atMap<s32, u32>			m_ptfxMap;

	atArray<strIndex>		m_streamingIndices;

	static char				ms_achLogFileName[RAGE_MAX_PATH];
	static bool				ms_bShowBudgets;
	static bool				ms_bVirtual;
	static bool				ms_bPhysical;
	static bool				ms_bShowSceneCostFromList_Drawables;
	static bool				ms_bShowSceneCostFromList_Dwds;
	static bool 			ms_bShowSceneCostFromList_Txds;
	static bool				ms_bShowSceneCostFromList_Frags;
	static bool				ms_bShowSceneCostFromList_Ptfx;
	static bool 			ms_bShowSceneCostFromList_Total;
	static bool				ms_bShowSceneNumShaders;
	static bool				ms_bOnlyTrackGbuf;
	static bool				ms_bOnlyTrackNonzeroAlpha;
	static bool				ms_bOnlyTrackMapData;
	static bool				ms_bFilterResults;
	static bool				ms_bTrackParentTxds;
	static s32				ms_nSelectedFilter;
	static s32				ms_nSelectedList;
	static bool				ms_bShowLodBreakdown;
	static bool				ms_bIncludeHdTxdPromotion;
	static bool				ms_bWriteLog;
	static bool				ms_bIncludeZoneAssets;

	static u32				ms_nWidth;
	static u32				ms_nHeight;

	static u32				ms_nMaxMemPhysMB;
	static u32				ms_nMaxMemVirtMB;
	static u32				ms_nMaxNumShaders;

	static Vec3V			ms_vSearchPos;

	// mouse drag
	static bool				ms_bMove;
	static Vector2			ms_vPos;
	static Vector2			ms_vClickDragDelta;
	static u32				ms_nNumEntries;

	// world search
	static s32				ms_nSearchRadius;
};

inline bool CSceneCostTracker::GetShowBudgets(u32 eListType)
{
	return (ms_bShowBudgets && ms_nSelectedList==(s32)eListType);
}

inline u32 CSceneCostTrackerStatsBuffer::GetVirtCost(u32 eCost)
{
	return m_anCostVirt[eCost];
}

inline u32 CSceneCostTrackerStatsBuffer::GetPhysCost(u32 eCost)
{
	return m_anCostPhys[eCost];
}

inline u32 CSceneCostTrackerStatsBuffer::GetVirtCostPeak(u32 eCost)
{
	return m_anCostVirtPeak[eCost];
}

inline u32 CSceneCostTrackerStatsBuffer::GetPhysCostPeak(u32 eCost)
{
	return m_anCostPhysPeak[eCost];
}

inline u32 CSceneCostTrackerStatsBuffer::GetCount(u32 eCount)
{
	return m_anCount[eCount];
}

inline u32 CSceneCostTrackerStatsBuffer::GetCountPeak(u32 eCount)
{
	return m_anCountPeak[eCount];
}

inline u32 CSceneCostTrackerStatsBuffer::GetVirtCostFiltered(u32 eCost)
{
	return m_anCostVirtFiltered[eCost];
}

inline u32 CSceneCostTrackerStatsBuffer::GetPhysCostFiltered(u32 eCost)
{
	return m_anCostPhysFiltered[eCost];
}

inline u32 CSceneCostTrackerStatsBuffer::GetCountFiltered(u32 eCount)
{
	return m_anCountFiltered[eCount];
}

inline bool CSceneCostTracker::FilteringResults()
{
	return ms_bFilterResults;
}

class CSceneMemoryCheckResult
{
public:
	enum eStatType
	{
		STAT_VRAM_FREE_AFTER_FLUSH = 0,
		STAT_MAIN_FREE_AFTER_FLUSH,
		STAT_VRAM_SCENE_REQUIRED,
		STAT_MAIN_SCENE_REQUIRED,

		STAT_VRAM_USED_AFTER_FLUSH,
		STAT_MAIN_USED_AFTER_FLUSH,

		STAT_TOTAL
	};

	void	Reset()									{ SetIsValid(false); for (s32 i=0; i<STAT_TOTAL; i++) { m_aStats[i]=0; } }
	u32		GetStat(eStatType stat) const			{ return m_aStats[stat]; }
	void	SetStat(eStatType stat, u32 val)		{ m_aStats[stat] = val; }
	bool	SceneFits() const						{ return VramSceneFits() && MainSceneFits(); }
	u32		GetMainShortfall() const				{ return ( MainSceneFits() ? 0 : (GetStat(STAT_MAIN_SCENE_REQUIRED)-GetStat(STAT_MAIN_FREE_AFTER_FLUSH)) ); }
	u32		GetVramShortfall() const				{ return ( VramSceneFits() ? 0 : (GetStat(STAT_VRAM_SCENE_REQUIRED)-GetStat(STAT_VRAM_FREE_AFTER_FLUSH)) ); }
	void	SetIsValid(bool bValid)					{ m_bIsValid = bValid; }
	bool	GetIsValid() const						{ return m_bIsValid; }

	s32		GetMainOverOrUnder() const				{ return ( ((s32)GetStat(STAT_MAIN_SCENE_REQUIRED)) - ((s32)GetStat(STAT_MAIN_FREE_AFTER_FLUSH)) ); }
	s32		GetVramOverOrUnder() const				{ return ( ((s32)GetStat(STAT_VRAM_SCENE_REQUIRED)) - ((s32)GetStat(STAT_VRAM_FREE_AFTER_FLUSH)) ); }

private:
	bool	VramSceneFits() const					{ return GetStat(STAT_VRAM_SCENE_REQUIRED) <= GetStat(STAT_VRAM_FREE_AFTER_FLUSH); }
	bool	MainSceneFits() const					{ return GetStat(STAT_MAIN_SCENE_REQUIRED) <= GetStat(STAT_MAIN_FREE_AFTER_FLUSH); }

	u32		m_aStats[STAT_TOTAL];
	bool	m_bIsValid;
};

class CSceneMemoryCheck
{
public:
	CSceneMemoryCheck()						{ SetState(STATE_IDLE); }

	void Start()							{ SetState(STATE_FLUSHING); }
	void Stop()								{ SetState(STATE_IDLE); }
	void Update();

	bool IsFinished() const					{ return ( m_eState==STATE_FINISHED && m_results.GetIsValid() ); }
	CSceneMemoryCheckResult& GetResult()	{ return m_results; }

private:

	enum eState
	{
		STATE_IDLE,
		STATE_FLUSHING,
		STATE_FLUSHING_AGAIN,
		STATE_LOADING_MAP,
		STATE_SAMPLING_COSTTRACKER,
		STATE_FINISHED
	};

	enum
	{
		NUM_TRACKER_SAMPLES = 30
	};

	void SetState(eState newState);
	void DisplayResults() const;
	void PrintResults() const;
	void CalcAndStoreSceneCosts();
	void CalcAndStoreMemAvailable();
	void FreezePlayer(bool bFrozen);
	void SampleCostTracker();

	CSceneMemoryCheckResult m_results;
	eState m_eState;
	bool m_bActive;
	Vec3V m_vPos;
	u32 m_numSamples;
	u32 m_maxTrackerMain;
	u32 m_maxTrackerVram;
};

extern CSceneCostTracker g_SceneCostTracker;
extern CSceneMemoryCheck g_SceneMemoryCheck;

#endif //__BANK

#endif //_SCENE_DEBUG_SCENECOSTTRACKER_H_