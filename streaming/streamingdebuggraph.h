/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/StreamingDebugGraph.h
// PURPOSE : 
// AUTHOR : 
// CREATED : 
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_STREAMINGMEMORYTRACKER_H_
#define _SCENE_DEBUG_STREAMINGMEMORYTRACKER_H_

#if __BANK

#include "atl/map.h"
#include "renderer/PostScan.h"
#include "vector/vector2.h"
#include "vector/color32.h"
#include "fwscene/lod/LodTypes.h"
#include "fwscene/stores/dwdstore.h"
#include "scene/datafilemgr.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingallocator.h"

#include "phbound/bound.h"

class streamingMemoryResult;
class CStreamGraphExtensionManager;

#define STREAMGRAPH_SCREEN_COORDS(x,y) Vector2( ( static_cast<float>(x) / grcViewport::GetDefaultScreen()->GetWidth() ), ( static_cast<float>(y) / grcViewport::GetDefaultScreen()->GetHeight() ) )

// all these hardcoded values were supposed to be used only with 720 pixel height resolution
// on next gen we aim 1080p so we use new values
// we compare 1042px instead of 1080 to take the window layout into account when windowed on PC
#define STREAMGRAPH_TITLE_HEIGHT()		(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 50 : 70)
#define STREAMGRAPH_TITLE_SPACING()		(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 20 : 35)
#define STREAMGRAPH_BAR_SPACING()		(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 30.0f : 45.0f)
#define STREAMGRAPH_MODULE_SPACING()	(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 30.0f : 45.0f)
#define STREAMGRAPH_BAR_HEIGHT()		(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 15 : 22)
#define STREAMGRAPH_BACKGROUND_HEIGHT()	(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 640 : 960)
#define STREAMGRAPH_FONTSCALE_FACTOR()	(grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 1.0f : 1.5f)
#define STREAMGRAPH_X_BORDER			2

enum
{
	SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE = 0,
	SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER,

	SCENESTREAMINGMEMORYTRACKER_BUFFER_TOTAL
};

namespace rage 
{ 
	class strStreamingModule; 
	class strStreamingInfo;
}

struct GraphEntry
{
	strIndex index;

	int virtualSize;
	int physicalSize;
	char name[128];

	union
	{
		s32 category;
		s32 module;
	};
};

class CStreamGraphModuleBuffer
{
public:
	bool HasNonZeroCategory( bool bIncludeVirtualMemory, bool bIncludePhysicalMemory )
	{
		for ( u32 i = 0; i < CDataFileMgr::CONTENTS_MAX; i++ )
		{
			if ( bIncludeVirtualMemory && m_CostVirt[ i ] > 0 )
				return true;
			if ( bIncludePhysicalMemory && m_CostPhys[ i ] > 0 )
				return true;
		}
		return false;
	}

	u32 m_CostVirt[ CDataFileMgr::CONTENTS_MAX ];
	u32 m_CostPhys[ CDataFileMgr::CONTENTS_MAX ];

	size_t m_TotalCostVirt;
	size_t m_TotalCostPhys;

	size_t m_FilteredTotalCostVirt;
	size_t m_FilteredTotalCostPhys;
};

class CStreamGraphSelectedFileData
{
public:
	void	  CollectDependents(strIndex index);

	static const int MAX_FILE_NAME_LENGTH = 260;

	char	  m_Name[MAX_FILE_NAME_LENGTH];
	u32		  m_PhysicalSize;
	u32		  m_VirtualSize;
	u32		  m_Flags;
	u32		  m_AccumulatedVirtMemoryBeforeSelectedFile;		// This just helps us to draw the currently selected asset into the bar
	u32		  m_AccumulatedPhysMemoryBeforeSelectedFile;		// This just helps us to draw the currently selected asset into the bar

	strIndex  m_Dependents[64];
	int		  m_DependentCount;
};

class CStreamGraphBuffer
{
public:
	static const int MAX_TRACKED_MODULES = 30;
	static const int MAX_LIST_ENTRIES = 35;
	static const int MAX_LIST_ENTRIES_TRANSPOSED = 10;
	static const int MAX_LIST_ENTRY_LENGTH = 128;
	static const int MAX_FILE_INFO_ENTRIES = 64;
	static const int MAX_FILE_INFO_ENTRY_LENGTH = 48;

	CStreamGraphBuffer()
	{
		sysMemSet(this, 0x0, sizeof(*this));
	}
public:
	s32								m_CurrentlySelectedModuleIdx;
	s32								m_CurrentlySelectedCategoryIdx;
	s32								m_CurrentlySelectedFileIdx;
	s32								m_CurrentlySelectedFileObjIdx;
	s32								m_NumFilesInSelectedBar;
	u32								m_NumFilesInSelectedModuleCategory[CDataFileMgr::CONTENTS_MAX];
	u32								m_NumFilesInSelectedCategoryModule[MAX_TRACKED_MODULES];
	bool							m_Transposed;
	CStreamGraphSelectedFileData	m_SelectedFileData;
	CStreamGraphModuleBuffer		m_Modules[MAX_TRACKED_MODULES];

	typedef struct  _listBufferContents
	{
		char m_fileName[MAX_LIST_ENTRY_LENGTH];
		int	 m_pSize;
		int	 m_vSize;
	} listBufferContents;

	listBufferContents				m_ListBuffer[MAX_LIST_ENTRIES];
	s32								m_NumListEntries;
	s32								m_ListOffset;
	s32								m_SelectedListEntry;
	char							m_FileInfoBuffer[MAX_FILE_INFO_ENTRIES][MAX_FILE_INFO_ENTRY_LENGTH];
	s32								m_NumFileInfoEntries;
	size_t							m_UsedByModulesVirt;		// Valid only if m_Tranposed is true
	size_t							m_UsedByModulesPhys;		// Valid only if m_Tranposed is true
};


extern Color32 GetLegendColour( u32 index );

class CStreamGraph
{
	friend class CStreamGraphExtensionManager;
	friend class CStreamGraphExternalAllocations;
	friend class CStreamGraphAllocationsToResGameVirt;

private:

	bool					m_ShowMemoryTracker;
	bool					m_ShowMemoryTrackerToggle;
	bool					m_ShowMemoryTrackerDependentsInScene;
	bool					m_OnlyShowMemoryTrackerDependentsInScene;

	bool					m_TrackTxd;
	bool					m_ShowVirtualMemory;
	bool					m_ShowPhysicalMemory;
	bool					m_ShowCategoryBreakdown;
	bool					m_ShowTemporaryMemory;
	bool					m_ShowLODBreakdown;

	bool					m_IncludeReferencedObjects;
	bool					m_IncludeDontDeleteObjects;
	bool					m_IncludeCachedObjects;
	bool					m_IncludeRequestedObjects;
	bool					m_OnlyIncludeCutsceneRequestedObjects;
	bool					m_ExcludeCutsceneRequestedObjects;
	bool					m_OnlyIncludeScriptRequestedObjects;
	bool					m_TransposeGraph;
	bool					m_ShowFileInfo;
	bool					m_ShowExtendedFileInfo;

	char					m_AssetFilterStr[64];
	bool					m_OrderByName;

	u32						m_nMaxMemMB;

	int						m_MemoryLocationCompareIndex;

	// mouse drag
	bool					m_bMove;
	Vector2					m_vPos;
	Vector2					m_vClickDragDelta;

	atMap<strIndex, bool>		m_CutsceneRequired;
	atArray<strIndex>			m_ScriptDependentOnList;

	CStreamGraphExtensionManager	*m_pStreamGraphExtManager;

	int virtualSize;
	int physicalSize;

	/////////////////////
	// LOD Category stuff

	typedef	struct _MemoryByLOD 
	{
		int phys;
		int virt;
	} MemoryByLOD;

	typedef struct _MemoryByLODStorage
	{
		MemoryByLOD m_TxdStoreMemoryByLod[LODTYPES_DEPTH_TOTAL+1];	// +1 for unknown lod type
		MemoryByLOD m_DwdStoreMemoryByLod[LODTYPES_DEPTH_TOTAL+1];
		MemoryByLOD m_DrawablesStoreMemoryByLod[LODTYPES_DEPTH_TOTAL+1];
	}	MemoryByLODBuffer;

	void ResetMemoryByLODBuffer(MemoryByLODBuffer *pBuffer)
	{
		memset(pBuffer, 0, sizeof(MemoryByLODBuffer));
	}

	void DrawLODStats();
	float DrawLODMemoryBar(int memoryBarIndex, int numDivisions, float x, float y);

	int *m_pTxdLodCounts;
	int *m_pDwdLodCounts;
	int *m_pDrawableLodCounts;

	void	AllocateMemoryForLODDisplay();
	void	FreeMemoryForLODDisplay();

	// Double buffer because calc and render aren't sync'd
	MemoryByLODBuffer m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_TOTAL];

	// LOD Category stuff
	/////////////////////

public:
	static	void			Init(unsigned initMode);
	static	void			Shutdown(unsigned shutdownMode);

	CStreamGraph();
	~CStreamGraph();

	void					AddWidgets(bkBank* pBank);
	void					DrawDebug();
	void					CheckToggleOnOff();
	void					Update();
	void					SwapBuffers();
	void					CollectResultsForAutoCapture( atArray<streamingMemoryResult*> &results );

	void					SetDisplayMemTracking(bool set) { m_ShowMemoryTracker = set; }

	void					IncludeVirtualMemory( bool bInclude )		{ m_ShowVirtualMemory		 = bInclude; }
	void					IncludePhysicalMemory( bool bInclude )		{ m_ShowPhysicalMemory		 = bInclude; }
	void					IncludeReferencedObjects( bool bInclude )	{ m_IncludeReferencedObjects = bInclude; }
	void					IncludeDontDeleteObjects( bool bInclude )	{ m_IncludeDontDeleteObjects = bInclude; }
	void					IncludeCachedObjects( bool bInclude )		{ m_IncludeCachedObjects	 = bInclude; }

	bool					ShouldShowTrackerDependentsInScene()		{ return m_ShowMemoryTrackerDependentsInScene; }
	bool					ShouldOnlyShowTrackerDependentsInScene()	{ return m_OnlyShowMemoryTrackerDependentsInScene; }

	// External access
	u32	  GetMaxMemMB() { return m_nMaxMemMB; }
	float GetWindowWidth() { return GetGraphWidth() + GetFileListWidth() + 20; }
	// originally designed (hardcoded) for 1280x720 res only (so we upscale to 1080p)
	float GetGraphWidth()		{ return grcViewport::GetDefaultScreen()->GetWidth() < 1900 ? 300.0f : 400.0f; }
	float GetFileListWidth()	{ return grcViewport::GetDefaultScreen()->GetWidth() < 1900 ? 500.0f : 800.0f; }

	bool	IsSelectedUsedByCB(CEntity *pEntity);

private:

	u32						GetBoundSize(phBound *pBound);

	typedef int (*CompareFunc)(const void *, const void *);

	bool		ObjectPassesFilter(strStreamingModule* pModule, strLocalIndex objIdx);
	bool		ObjectPassesCategoryFilter(strStreamingModule* pModule, s32 categoryIdx, strLocalIndex objIdx);
	void					UpdateCategoryStats(s32 categoryId, CompareFunc compareFunc);
	void					UpdateModuleStats(s32 moduleId, CompareFunc compareFunc);

	void					UpdateLodStats();
	int						GetLodFromCountArray(int idx, int *pLodCounts);
	void					BuildLODData(int *pLodCounts);

	void					BuildTxdLODCounts(CEntity *pEntity, int *pTxdLodCounts);
	void					BuildTxdLODData(int *pLodCounts);

	void					BuildDwdLODCounts(CEntity *pEntity, int *pDwdLodCounts);
	void					BuildDwdLODData(int *pLodCounts);

	void					BuildDrawableLODCounts(CEntity *pEntity, int *pDrawableLodCounts);
	void					BuildDrawableLODData(int *pLodCounts);

	void					ResetFileList();

	void					BuildDependentOnList();
	void					AddDependenciesToMap(strIndex index, atMap<strIndex, bool> &outMap);
	bool					CheckIfDependency(atArray<strIndex> &dependentList, strIndex streamIndex);
	bool					CheckChildren(strIndex parentStrIdx, strIndex thisStrIdx);

	void					UpdateFileList(const GraphEntry * sortedEntries, const int entryCount);
	void					UpdateFileInfo(const GraphEntry & file, strStreamingModule* pModule);

	// Sets the first valid bar in the streamgraph (excluding extensions)
	void					SetFirstSelected();
	// Sets the last valid bar in the streamgraph (excluding extensions)
	void					SetLastSelected();

	void					UpdateMemoryBarSelection();
	void					UpdateRepositionWindow();
	void					SetSelectedFileName(CStreamGraphBuffer & buffer, const char * name);

	void  DrawTitle();
	void  DrawBackground();
	float DrawMemoryBar(int memoryBarIndex, int numDivisions, float x, float y);
	float DrawMasterMemoryBar(float x, float y);
	void  DrawMemoryMarkers(float x, float y);
	float DrawLegend(float x, float y);
	void  DrawFileList(float x, float y);
	void  DrawFileInfo(float x, float y);
	
	int GetNumTrackedCategories();
	int GetNumTrackedModules();
	CompareFunc GetCompareFunc();
	strStreamingModule * GetSelectedModule(const CStreamGraphBuffer & buffer);
	void GetDrawableInfo(const Drawable & drawable, u32 & vertexCount, int & textureCount);

	static int CompareStreamingInfoByPhysSize(const void* pVoidA, const void* pVoidB);
	static int CompareStreamingInfoByVirtSize(const void* pVoidA, const void* pVoidB);
	static int CompareStreamingInfoByTotalSize(const void* pVoidA, const void* pVoidB);
	static int CompareStreamingInfoByCategoryPhysSize(const void* pVoidA, const void* pVoidB);
	static int CompareStreamingInfoByCategoryVirtSize(const void* pVoidA, const void* pVoidB);
	static int CompareStreamingInfoByCategoryTotalSize(const void* pVoidA, const void* pVoidB);
	static int CompareStreamingInfoByName(const void* pVoidA, const void* pVoidB);
	static u32 GetStreamingCategory( strStreamingInfo *info );
	static void CreateAssetInfoString(strIndex index, char *outBuffer, size_t bufferSize);


	CStreamGraphBuffer m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_TOTAL ];
};



typedef atSingleton<CStreamGraph> CStreamGraphSingleton;
#define DEBUGSTREAMGRAPH CStreamGraphSingleton::InstanceRef()
#define INIT_DEBUGSTREAMGRAPH									\
	do {														\
		if(!CStreamGraphSingleton::IsInstantiated())			\
		{														\
			CStreamGraphSingleton::Instantiate();				\
		}														\
	} while(0)													\
	//END
#define SHUTDOWN_DEBUGSTREAMGRAPH CStreamGraphSingleton::Destroy()

//extern CStreamGraph g_DebugStreamGraph;

#endif //__BANK

#endif //_SCENE_DEBUG_STREAMINGMEMORYTRACKER_H_