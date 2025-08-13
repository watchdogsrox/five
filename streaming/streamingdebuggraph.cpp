/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/StreamingDebugGraph.cpp
// PURPOSE : 
// AUTHOR :  
// CREATED : 
//
/////////////////////////////////////////////////////////////////////////////////

#include "streaming/streamingdebuggraph.h"
#include "streaming/streaming_channel.h"
#include "system/stack.h"

STREAMING_OPTIMISATIONS();
SCENE_OPTIMISATIONS();

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "debug/debugscene.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerSearch.h"
#include "fwdebug/debugdraw.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "input/mouse.h"

#include "text/text.h"
#include "text/textconversion.h"

#include "grcore/viewport.h"
#include "fwmaths/Rect.h"
#include "vector/color32.h"

#include "streaming/packfilemanager.h"
#include "system/controlmgr.h"
#include "system/AutoGPUCapture.h"
#include "fwsys/timer.h"

#include "streaming/streamingdebug.h"

#include "entity/archetype.h"
#include "objects/DummyObject.h"
#include "scene/Building.h"

#include "script/script.h"
#include "script/streamedscripts.h"

#include "frontend/Scaleform/ScaleFormStore.h"
#include "streamingdebuggraphextensions.h"
// Bounds
#include "phbound/support.h"
#include "phbound/boundgrid.h"
#include "phbound/boundribbon.h"
#include "phbound/boundsurface.h"
#include "phbound/OptimizedBvh.h"

//CStreamGraph g_DebugStreamGraph;

extern void GetSizesSafe(strIndex, strStreamingInfo &, int &, int &, bool);
extern void GetActiveEntityAssets(atMap<strIndex, bool> &outAssetList);
extern const char *CreateFlagString(u32 flags, char *outBuffer, size_t bufferSize);

extern atMap<strIndex, bool> s_SceneMap;
extern bool s_DumpExcludeSceneAssetsInGraph;
extern bool s_ExcludeSceneAssetsForRendering;
extern bool s_ExcludeSceneAssetsForNonRendering;


void GetLoadedSizes(strStreamingModule *pModule, strLocalIndex objIndex, int &virt, int &phys)
{
	virt += (int) pModule->GetVirtualMemoryOfLoadedObj(objIndex, false);
	phys += (int) pModule->GetPhysicalMemoryOfLoadedObj(objIndex, false);
}


void CStreamGraphSelectedFileData::CollectDependents(strIndex index)
{
	int finalCount = 0;
	int maxCount = NELEM(m_Dependents);

	strLoadedInfoIterator it;
	strIndex loadIndex = it.GetNextIndex();

	while (loadIndex.IsValid())
	{
		strIndex deps[STREAMING_MAX_DEPENDENCIES];
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(loadIndex);
		strLocalIndex objectIdx = pModule->GetObjectIndex(loadIndex);
		s32 numDeps = pModule->GetDependencies(objectIdx, &deps[0], STREAMING_MAX_DEPENDENCIES);

		bool hasDeps = false;

		while (numDeps--)
		{
			if (deps[numDeps] == index)
			{
				hasDeps = true;
				break;
			}
		}

		if( hasDeps )
		{
			m_Dependents[finalCount++] = loadIndex;

			// Don't write outside the array bounds!
			if (finalCount == maxCount)
			{
				break;
			}
		}

		loadIndex = it.GetNextIndex();
	}

	m_DependentCount = finalCount;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	
// PURPOSE:		 
//////////////////////////////////////////////////////////////////////////

void	CStreamGraph::Init(unsigned /*initMode*/)
{
	USE_DEBUG_MEMORY();

	INIT_DEBUGSTREAMGRAPH;
}

void	CStreamGraph::Shutdown(unsigned /*shutdownMode*/)
{
	USE_DEBUG_MEMORY();

	SHUTDOWN_DEBUGSTREAMGRAPH;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	
// PURPOSE:		 
//////////////////////////////////////////////////////////////////////////
CStreamGraph::CStreamGraph()
{
	m_ShowMemoryTracker = false;
	m_ShowMemoryTrackerToggle = false;

	m_ShowMemoryTrackerDependentsInScene = false;
	m_OnlyShowMemoryTrackerDependentsInScene = false;

	m_TrackTxd = false;
	m_ShowVirtualMemory = true;
	m_ShowPhysicalMemory = true;
	m_ShowTemporaryMemory = false;
	m_IncludeReferencedObjects = true;
	m_IncludeDontDeleteObjects = true;
	m_IncludeCachedObjects = false;
	m_IncludeRequestedObjects = false;
	m_IncludeDontDeleteObjects = true;
	m_OnlyIncludeCutsceneRequestedObjects	= false;
	m_ExcludeCutsceneRequestedObjects		= false;
	m_OnlyIncludeScriptRequestedObjects		= false;
	m_ShowCategoryBreakdown = false;
	m_ShowLODBreakdown = false;
	m_TransposeGraph = false;
	m_ShowFileInfo = true;
	m_ShowExtendedFileInfo = true;
	m_bMove = false;
	m_OrderByName = false;

	m_MemoryLocationCompareIndex = 0;

	m_vPos.Set(500, 40);
	m_nMaxMemMB = 75;
	m_AssetFilterStr[0] = '\0';

	m_pTxdLodCounts = NULL;
	m_pDwdLodCounts = NULL;
	m_pDrawableLodCounts = NULL;

	// Create the extensions manager
	m_pStreamGraphExtManager = rage_new CStreamGraphExtensionManager;


}

CStreamGraph::~CStreamGraph()
{
	delete m_pStreamGraphExtManager;
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	
// PURPOSE:		 
//////////////////////////////////////////////////////////////////////////
void CStreamGraph::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("Stream Graph", false);
	pBank->AddToggle("Show memory usage", &m_ShowMemoryTracker);

	pBank->AddText("Filter Asset Name Containing", m_AssetFilterStr, 64);

	pBank->AddToggle("Show Dependents In Scene", &m_ShowMemoryTrackerDependentsInScene);
	pBank->AddToggle("Show ONLY Dependents In Scene", &m_OnlyShowMemoryTrackerDependentsInScene);

	pBank->AddToggle("Track TXD in Texture Viewer", &m_TrackTxd);

	// Make sure the list of debug locations is resident.
	if (CStreamingDebug::GetMemoryProfileLocationNames() == NULL)
	{
		CStreamingDebug::LoadMemoryProfileLocations();
	}

	int locationCount = CStreamingDebug::GetMemoryProfileLocationCount();
	pBank->AddCombo("Compare Against", &m_MemoryLocationCompareIndex, locationCount, CStreamingDebug::GetMemoryProfileLocationNames());

	pBank->AddTitle("Types of memory to include");
	pBank->AddToggle("Main memory", &m_ShowVirtualMemory);
	pBank->AddToggle("Vram memory", &m_ShowPhysicalMemory);
	pBank->AddToggle("Temporary memory", &m_ShowTemporaryMemory, NullCB, "Show temporary memory as well");
	pBank->AddToggle("Break down by category", &m_ShowCategoryBreakdown);
	pBank->AddToggle("Transpose", &m_TransposeGraph );
	pBank->AddToggle("Show file info", &m_ShowFileInfo);
	pBank->AddToggle("Show extended file info", &m_ShowExtendedFileInfo);
	pBank->AddToggle("Order by name", &m_OrderByName);

	pBank->AddToggle("Break down by LOD", &m_ShowLODBreakdown );

	pBank->AddTitle("Types of streamed objects to include");
	pBank->AddToggle("REFERENCED objects", &m_IncludeReferencedObjects);
	pBank->AddToggle("DONTDELETE objects", &m_IncludeDontDeleteObjects);
	pBank->AddToggle("CACHED objects", &m_IncludeCachedObjects);
	pBank->AddToggle("REQUESTED objects", &m_IncludeRequestedObjects);
	pBank->AddToggle("Exclude objects in scene", &s_DumpExcludeSceneAssetsInGraph);
	pBank->AddToggle("Exclude objects for rendering only", &s_ExcludeSceneAssetsForRendering);
	pBank->AddToggle("Exclude objects for non-rendering only", &s_ExcludeSceneAssetsForNonRendering);
	pBank->AddToggle("(only) CUTSCENE REQUESTED objects", &m_OnlyIncludeCutsceneRequestedObjects);
	pBank->AddToggle("Exclude CUTSCENE REQUESTED objects", &m_ExcludeCutsceneRequestedObjects);
	pBank->AddToggle("(only) SCRIPT REQUESTED objects", &m_OnlyIncludeScriptRequestedObjects);

	pBank->AddSlider("Max Mem (mb)", &m_nMaxMemMB, 1, 512, 1);

	// Add widgets for extensions
	m_pStreamGraphExtManager->AddWidgets(pBank);

	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
static const struct  
{
	u32			category;
	const char *label;
} sAssetCategoryLegend[] =
{
	{ CDataFileMgr::CONTENTS_DEFAULT,	"Default"	},
	{ CDataFileMgr::CONTENTS_PROPS,		"Props"		},
	{ CDataFileMgr::CONTENTS_MAP,		"Map"		},
	{ CDataFileMgr::CONTENTS_LODS,		"Lods"		},
	{ CDataFileMgr::CONTENTS_PEDS,		"Peds"		},
	{ CDataFileMgr::CONTENTS_VEHICLES,	"Vehicles"  },
	{ CDataFileMgr::CONTENTS_ANIMATION, "Animation" },
	{ CDataFileMgr::CONTENTS_CUTSCENE,	"Cutscene"	},
	{ CDataFileMgr::CONTENTS_DLC_MAP_DATA, "DLC"    },
	{ CDataFileMgr::CONTENTS_DEBUG_ONLY, "DebugOnly"}
};

static const struct  
{
	u32			lod;
	const char *label;
} sAssetLODLegend[] =
{
	{ LODTYPES_DEPTH_HD,		"HD"		},
	{ LODTYPES_DEPTH_LOD,		"LOD"		},
	{ LODTYPES_DEPTH_SLOD1,		"SLOD1"		},
	{ LODTYPES_DEPTH_SLOD2,		"SLOD2"		},
	{ LODTYPES_DEPTH_SLOD3,		"SLOD3"		},
	{ LODTYPES_DEPTH_ORPHANHD,	"ORPHANHD"  },
	{ LODTYPES_DEPTH_SLOD4,		"SLOD4"		},
	{ LODTYPES_DEPTH_TOTAL,		"Unknown"	},
};


static const Color32 sLegendColours[] =
{
	Color32(0,255,255),
	Color32(255,255,0),
	Color32(0,0,255),
	Color32(255,0,255),
	Color32(255,0,0),
	Color32(0,255,0),
	Color32(255,255,255),
	Color32(128,255,255),
	Color32(128,0,255),
	Color32(255,128,255),
	Color32(255,128,0),
	Color32(128,255,0),
	Color32(128,128,255),
	Color32(255,128,255),
	Color32(255,128,128),
	Color32(128,255,128),
	Color32(128,64,64),
	Color32(255,128,64),
	Color32(64,128,0),
	Color32(128,64,0),
	Color32(128,128,64),
	Color32(64,128,255),
	Color32(255,128,64),
	Color32(128,64,128),
	Color32(128,64,64),
	Color32(64,128,64),
	Color32(64,128,64),
	Color32(128,64,64),
	Color32(128,64,64),
	Color32(64,128,64),
	Color32(64,128,64),
	Color32(64,64,128)
};

Color32 GetLegendColour( u32 index )
{
	index = index % ( sizeof(sLegendColours)/sizeof(sLegendColours[0]) );
	return sLegendColours[ index ];
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CStreamGraph::DrawTitle()
{
	char titleText[200];
	sprintf(titleText, m_ShowVirtualMemory ? (m_ShowPhysicalMemory ? "Main + Vram Memory" : "Main Memory") : "Vram Memory");

	u32 x = (u32)m_vPos.x;
	u32 y = (u32)m_vPos.y;

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.4f, 0.4f));
	DebugTextLayout.SetColor(Color32(0, 255, 255, 255));
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y-STREAMGRAPH_TITLE_SPACING()), titleText);
}

void CStreamGraph::DrawBackground()
{
	float x = m_vPos.x;
	float y = m_vPos.y;
	fwRect bgRect(x, y + STREAMGRAPH_BACKGROUND_HEIGHT(), x + GetWindowWidth(), y);

	Color32 bgColour(0, 0, 0, 128);
	CSprite2d::DrawRectSlow(bgRect, bgColour );
}

float CStreamGraph::DrawMasterMemoryBar(float x, float y)
{
	CTextLayout DebugTextLayout;


	DrawMemoryMarkers(x, y);

	// Get some numbers.
	ptrdiff_t totalMemory = 0;
	ptrdiff_t usedByModules = 0;
	ptrdiff_t usedByExternalAlloc = 0;
	ptrdiff_t unaccountedFor = 0;
	ptrdiff_t freeMemory = 0;
	CStreamGraphBuffer & buffer = m_statBuffers[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER];

	ptrdiff_t usedByModulesVirt = 0;
	ptrdiff_t usedByModulesPhys = 0;

	ptrdiff_t unfilteredUsedByModulesVirt = 0;
	ptrdiff_t unfilteredUsedByModulesPhys = 0;

	if ( buffer.m_Transposed )
	{
		usedByModulesVirt = (ptrdiff_t) buffer.m_UsedByModulesVirt;
		usedByModulesPhys = (ptrdiff_t) buffer.m_UsedByModulesPhys;
	}
	else
	{
		for (int x=0; x<GetNumTrackedModules(); x++)
		{
			usedByModulesVirt += (ptrdiff_t) buffer.m_Modules[x].m_FilteredTotalCostVirt;
			usedByModulesPhys += (ptrdiff_t) buffer.m_Modules[x].m_FilteredTotalCostPhys;

			unfilteredUsedByModulesVirt += (ptrdiff_t) buffer.m_Modules[x].m_TotalCostVirt;
			unfilteredUsedByModulesPhys += (ptrdiff_t) buffer.m_Modules[x].m_TotalCostPhys;
		}
	}

	if (m_ShowVirtualMemory)
	{
		totalMemory += (ptrdiff_t) strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable();
		freeMemory += (ptrdiff_t) strStreamingEngine::GetAllocator().GetVirtualMemoryFree() + unfilteredUsedByModulesVirt - usedByModulesVirt;
		usedByExternalAlloc += (ptrdiff_t) strStreamingEngine::GetAllocator().GetExternalVirtualMemoryUsed();
		usedByModules += (ptrdiff_t) usedByModulesVirt;

		unaccountedFor += (ptrdiff_t) (strStreamingEngine::GetAllocator().GetVirtualMemoryUsed() - strStreamingEngine::GetAllocator().GetExternalVirtualMemoryUsed());
		unaccountedFor -= (ptrdiff_t) (unfilteredUsedByModulesVirt);
	}

	if (m_ShowPhysicalMemory)
	{
		totalMemory += (ptrdiff_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
		freeMemory += (ptrdiff_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryFree() + unfilteredUsedByModulesPhys - usedByModulesPhys;
		usedByExternalAlloc += (ptrdiff_t) strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed();
		usedByModules += (ptrdiff_t) usedByModulesPhys;

		unaccountedFor += (ptrdiff_t) (strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed() - strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed());
		unaccountedFor -= (ptrdiff_t) (unfilteredUsedByModulesPhys);
	}


	usedByModules /= 1024;
	totalMemory /= 1024;
	usedByExternalAlloc /= 1024;
	unaccountedFor /= 1024;
	freeMemory /= 1024;

	float totalMemoryf = (float) totalMemory;

	// The left bar is what's used by modules.
	float x1 = x + float( usedByModules ) / totalMemoryf * GetGraphWidth();
	float x2 = x + float(usedByModules+usedByExternalAlloc) / totalMemoryf * GetGraphWidth();
	float x3 = x2 + float(Max(unaccountedFor, (ptrdiff_t) 0)) / totalMemoryf * GetGraphWidth();

	char line[256];

	formatf(line, "Modules: %.2fMB, Ext: %.2fMB, ?: %.2fMB, Free: %.2fMB", float(usedByModules) / 1024.0f,
		float(usedByExternalAlloc) / 1024.0f, float(unaccountedFor) / 1024.0f, float(freeMemory) / 1024.0f);

	float fScreenX = (float)x;
	float fScreenY = y;

	float y1 = y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT();
	float y2 = y + STREAMGRAPH_BAR_SPACING();

	fwRect modMemRect(x, y1, x1, y2);
	CSprite2d::DrawRectSlow(modMemRect, Color32(0xffff0000));

	fwRect extMemRect(x1, y1, x2, y2);
	CSprite2d::DrawRectSlow(extMemRect, Color32(0xffffff00));

	fwRect mysteryMemRect(x2, y1, x3, y2);
	CSprite2d::DrawRectSlow(mysteryMemRect, Color32(0xffff00ff));

	fwRect freeMemRect(x3, y1, x + GetGraphWidth(), y2);
	CSprite2d::DrawRectSlow(freeMemRect, Color32(0xff00ff00));

	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(fScreenX, fScreenY-5.0f), line);

	return STREAMGRAPH_BAR_SPACING();
}

float CStreamGraph::DrawMemoryBar(int memoryBarIndex, int numDivisions, float x, float y)
{
	float heightOfBarDrawn = 0.0f;

	CStreamGraphBuffer & buffer = m_statBuffers[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER];
	strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
	
	char achMsg[200];
	bool bDoneBackground = false;
	float curX = float(x + STREAMGRAPH_X_BORDER); 

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	u32 totalBarVirtMemory = 0;
	u32 totalBarPhysMemory = 0;

	size_t totalModuleMemory = 0;
	size_t memoryBudgetKB = 0;
	bool overMemoryBudget = false;
	float budgetMarker = 0.0f;

	if (!buffer.m_Transposed)
	{
		for (int div=0; div<numDivisions; div++)
		{
			if (m_ShowVirtualMemory)
			{
				totalModuleMemory += buffer.m_Modules[memoryBarIndex].m_CostVirt[div];
			}

			if (m_ShowPhysicalMemory)
			{
				totalModuleMemory += buffer.m_Modules[memoryBarIndex].m_CostPhys[div];
			}
		}

		// Find out the reference value.
		if ((u32) CStreamingDebug::GetMemoryProfileLocationList().m_Locations.GetCount() > (u32) m_MemoryLocationCompareIndex)
		{
			const MemoryFootprint &footprint = CStreamingDebug::GetMemoryProfileLocationList().m_Locations[m_MemoryLocationCompareIndex].m_MemoryFootprint;
			const MemoryProfileModuleStat *stat = footprint.m_ModuleMemoryStatsMap.SafeGet(atStringHash(moduleMgr.GetModule(memoryBarIndex)->GetModuleName()));

			if (stat)
			{
				if (m_ShowVirtualMemory)
				{
					memoryBudgetKB += stat->m_VirtualMemory;
				}

				if (m_ShowPhysicalMemory)
				{
					memoryBudgetKB += stat->m_PhysicalMemory;
				}

				overMemoryBudget = (totalModuleMemory / 1024 > memoryBudgetKB);
				budgetMarker = (x+STREAMGRAPH_X_BORDER) + ((float)GetGraphWidth() * (float) (memoryBudgetKB / 1024) / m_nMaxMemMB);
			}
		}
	}

	for ( s32 xAxisIdx = 0; xAxisIdx < numDivisions; xAxisIdx++ )
	{
		s32 moduleId	= memoryBarIndex;
		s32 categoryIdx = xAxisIdx;
		if ( buffer.m_Transposed )
		{
			std::swap( moduleId, categoryIdx );
		}

		strStreamingModule* pModule = moduleMgr.GetModule(moduleId);

		u32 memoryInBar = 0;
		if ( m_ShowPhysicalMemory )
		{
			totalBarPhysMemory		+= buffer.m_Modules[ moduleId ].m_CostPhys[ categoryIdx ];
			memoryInBar				+= buffer.m_Modules[ moduleId ].m_CostPhys[ categoryIdx ];
		}
		if ( m_ShowVirtualMemory )
		{
			totalBarVirtMemory		+= buffer.m_Modules[ moduleId ].m_CostVirt[ categoryIdx ];
			memoryInBar				+= buffer.m_Modules[ moduleId ].m_CostVirt[ categoryIdx ];
		}

		if ( !buffer.m_Transposed && memoryInBar == 0 )
		{
			continue;
		}

		float barWidth = ( (float)(memoryInBar) / (float)( m_nMaxMemMB * 1024 * 1024 ) ) * GetGraphWidth();
		bool overBudget = ( curX - x ) + barWidth > GetGraphWidth();

		Color32 barColour1 = Color32(0,255,0,255);
		Color32 barColour2 = GetLegendColour( xAxisIdx ); 

		if ( overMemoryBudget )
		{
			barColour1.Set( 255, 0, 0, 255 ); 
			barColour2.Set( 255, 0, 0, 255 );
		}

		if ( !bDoneBackground )
		{
			bDoneBackground = true;

			char const * textToRender = buffer.m_Transposed ? 
				sAssetCategoryLegend[memoryBarIndex].label : pModule->GetModuleName();
			
			DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), textToRender);

			DrawMemoryMarkers(x, y);

			// Show the budget marker, if available.
			if (memoryBudgetKB > 0)
			{
				Vector2 p1(STREAMGRAPH_SCREEN_COORDS(budgetMarker, y+STREAMGRAPH_BAR_SPACING()-STREAMGRAPH_BAR_HEIGHT()));
				Vector2 p2(STREAMGRAPH_SCREEN_COORDS(budgetMarker, y+STREAMGRAPH_BAR_SPACING()));
				grcDebugDraw::Line(p1, p2, Color32(255, 0, 255, 255));
			}
		}

		if( overBudget )
		{
			barWidth = GetGraphWidth() - ( curX - x );
			barColour1.Set( 255, 0, 0, 255 ); 
			barColour2.Set( 255, 0, 0, 255 );
		}
		else if ( !m_ShowCategoryBreakdown && !buffer.m_Transposed )  
		{
			barColour2.Set( 0, 255, 0, 255 );
		}

		fwRect barRect1((float) curX,(float) y + STREAMGRAPH_BAR_SPACING(),								  (float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f) );
		fwRect barRect2((float) curX,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f),(float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT());

		CSprite2d::DrawRectSlow(barRect1, barColour1);
		CSprite2d::DrawRectSlow(barRect2, barColour2);

		curX += barWidth;
	}

	if ( budgetMarker > 0.0f )
	{
		if ( !overMemoryBudget )
		{
			fwRect compareRect((float) curX, y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT() + 5.0f, budgetMarker, y + STREAMGRAPH_BAR_SPACING());
			CSprite2d::DrawRectSlow(compareRect, Color32(0xff000080));
		}
		else
		{
			fwRect compareRect(budgetMarker, y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT(), (float) curX, y + STREAMGRAPH_BAR_SPACING());
			CSprite2d::DrawRectSlow(compareRect, Color32(0xffff00ff));
		}
	}

	if ( buffer.m_Transposed || totalBarVirtMemory+totalBarPhysMemory > 0 ) 
	{
		heightOfBarDrawn = STREAMGRAPH_BAR_SPACING();

#if __PS3
		sprintf( achMsg, "%.3f MB (M), %.3f MB (V)", (float)totalBarVirtMemory / (1024.0f*1024.0f),  (float)totalBarPhysMemory / (1024.0f*1024.0f) );
#else
		sprintf( achMsg, "%.3f MB", (float)(totalBarVirtMemory+totalBarPhysMemory) / (1024.0f*1024.0f) );
#endif
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x + 150, y), achMsg);
	}

	// Don't draw the file selector if we're looking at an extension
	if ( !m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] && 
			((!buffer.m_Transposed && ( memoryBarIndex == buffer.m_CurrentlySelectedModuleIdx )) || 
			( buffer.m_Transposed && ( memoryBarIndex == buffer.m_CurrentlySelectedCategoryIdx))) )		
	{
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x - 40, y), ">>");			

		// Draw in white the contribution of the currently selected asset

		u32  selectedSegmentMemorySize       = 0;
		u32  accumulatedMemoryBeforeSelected = 0;
		if ( m_ShowPhysicalMemory ) 
		{
			selectedSegmentMemorySize		+= buffer.m_SelectedFileData.m_PhysicalSize;
			accumulatedMemoryBeforeSelected += buffer.m_SelectedFileData.m_AccumulatedPhysMemoryBeforeSelectedFile;
		}
		if ( m_ShowVirtualMemory )
		{
			selectedSegmentMemorySize		+= buffer.m_SelectedFileData.m_VirtualSize;
			accumulatedMemoryBeforeSelected += buffer.m_SelectedFileData.m_AccumulatedVirtMemoryBeforeSelectedFile;
		}

		float  selectedFileSegmentXStart	= x + STREAMGRAPH_X_BORDER + ( (float)(accumulatedMemoryBeforeSelected)  / (float)( m_nMaxMemMB * 1024 * 1024 ) ) * GetGraphWidth();
		float  selectedFileSegmentXEnd		= selectedFileSegmentXStart + ( (float)(selectedSegmentMemorySize) / (float)( m_nMaxMemMB * 1024 * 1024 ) ) * GetGraphWidth();

		// Never draw the segment smaller than one pixel, or it's kind of confusing.
		selectedFileSegmentXEnd = Max( selectedFileSegmentXStart + 1, selectedFileSegmentXEnd );

		fwRect selectedFileSegmentRect
			(
			(float) selectedFileSegmentXStart,					// left
			(float) y + STREAMGRAPH_BAR_SPACING(),						// bottom
			(float) selectedFileSegmentXEnd,					// right
			(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()	// top
			);

		Color32 selectedFileSegmentColor( 255, 255, 255, 255 );

		CSprite2d::DrawRectSlow( selectedFileSegmentRect, selectedFileSegmentColor );
	}

	return heightOfBarDrawn;
}

void CStreamGraph::DrawMemoryMarkers(float x, float y)
{
	Vector2 p1(STREAMGRAPH_SCREEN_COORDS(x+STREAMGRAPH_X_BORDER,				 y+STREAMGRAPH_BAR_SPACING()));
	Vector2 p2(STREAMGRAPH_SCREEN_COORDS(x+STREAMGRAPH_X_BORDER+GetGraphWidth(), y+STREAMGRAPH_BAR_SPACING()));
	grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));

	if (m_nMaxMemMB<=20)
	{
		// draw marker for each mb
		float fDiv = (float)GetGraphWidth() / m_nMaxMemMB;
		for (u32 i=0; i<m_nMaxMemMB+1; i++)
		{
			Vector2 p1(STREAMGRAPH_SCREEN_COORDS(x+STREAMGRAPH_X_BORDER+i*fDiv, y+STREAMGRAPH_BAR_SPACING()-STREAMGRAPH_BAR_HEIGHT()*0.25f));
			Vector2 p2(STREAMGRAPH_SCREEN_COORDS(x+STREAMGRAPH_X_BORDER+i*fDiv, y+STREAMGRAPH_BAR_SPACING()));
			grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));
		}
	}
	else
	{
		// draw marker for every 10mb
		float fDiv = (float)GetGraphWidth() / m_nMaxMemMB;
		for (u32 i=0; i<(m_nMaxMemMB/10)+1; i++)
		{
			Vector2 p1(STREAMGRAPH_SCREEN_COORDS(x+STREAMGRAPH_X_BORDER+i*fDiv*10, y+STREAMGRAPH_BAR_SPACING()-STREAMGRAPH_BAR_HEIGHT()*0.25f));
			Vector2 p2(STREAMGRAPH_SCREEN_COORDS(x+STREAMGRAPH_X_BORDER+i*fDiv*10, y+STREAMGRAPH_BAR_SPACING()));
			grcDebugDraw::Line(p1, p2, Color32(255, 255, 255, 255));
		}
	}
}

float CStreamGraph::DrawLegend(float x, float y)
{
	const float fBoxW = 10.0f;

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	float yEnd = y;

	if(m_ShowLODBreakdown)
	{
		s32 numCategories = sizeof(sAssetLODLegend)/sizeof(sAssetLODLegend[0]);

		for (u32 i=0; i<numCategories; i++)
		{
			Color32 boxColor  = GetLegendColour(i);
			const char* label = sAssetLODLegend[i].label;

			float fScreenX = (float)x;
			float fScreenY = y + i * 20.0f;

			fwRect boxRect(fScreenX,fScreenY + fBoxW,fScreenX + fBoxW,fScreenY);
			CSprite2d::DrawRectSlow(boxRect, boxColor);
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(fScreenX+10.0f, fScreenY-5.0f), label);
		}
		yEnd = y + (numCategories + 1) * 20;
	}
	else

	if ( m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ].m_Transposed )
	{
		strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();

		u32 numModules = GetNumTrackedModules();

		u32 visibleModuleIdx = 0;
		for ( u32 i = 0; i < numModules; i++ )
		{
			if ( !m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ].m_Modules[ i ].HasNonZeroCategory( m_ShowVirtualMemory, m_ShowPhysicalMemory ) )
			{
				continue;
			}

			strStreamingModule* pModule = moduleMgr.GetModule(i);

			Color32 boxColor  = GetLegendColour( i );
			const char* label = pModule->GetModuleName();

			float fScreenX = (float)x;
			float fScreenY = y + visibleModuleIdx * 20.0f;

			fwRect boxRect(fScreenX,fScreenY + fBoxW,fScreenX + fBoxW,fScreenY);
			CSprite2d::DrawRectSlow(boxRect, boxColor);
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(fScreenX+10.0f, fScreenY-4.0f), label);
			visibleModuleIdx++;
		}

		yEnd = y + (visibleModuleIdx + 1) * 20;
	}
	else
	{
		if ( m_ShowCategoryBreakdown )
		{
			s32 numCategories = sizeof(sAssetCategoryLegend)/sizeof(sAssetCategoryLegend[0]);

			for (u32 i=0; i<numCategories; i++)
			{
				Color32 boxColor  = GetLegendColour(i);
				const char* label = sAssetCategoryLegend[i].label;

				float fScreenX = (float)x;
				float fScreenY = y + i * 20.0f;

				fwRect boxRect(fScreenX,fScreenY + fBoxW,fScreenX + fBoxW,fScreenY);
				CSprite2d::DrawRectSlow(boxRect, boxColor);
				DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(fScreenX+10.0f, fScreenY-5.0f), label);
			}

			yEnd = y + (numCategories + 1) * 20;
		}
	}

	return yEnd;
}

void CStreamGraph::DrawFileList(float x, float y)
{
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));

	CStreamGraphBuffer & buffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ];

	const float headerSize = 16.0f*STREAMGRAPH_FONTSCALE_FACTOR();
	float xStart= x;
	float xEnd = x + GetFileListWidth();
	float yStart = y + 5 + headerSize;
	float yEnd = yStart + buffer.m_NumListEntries * 16;
	float yLength = yEnd - yStart;
	float y0 = yStart + yLength * (float)buffer.m_ListOffset / (float)buffer.m_NumFilesInSelectedBar;
	float y1 = y0 + yLength * (float)buffer.m_NumListEntries / (float)buffer.m_NumFilesInSelectedBar;

	fwRect scrollBarThumb(xEnd - 14, y1, xEnd - 4, y0);
	CSprite2d::DrawRectSlow(scrollBarThumb, Color_white);
	fwRect scrollBarThumbInner(xEnd - 12, y1, xEnd - 6, y0);
	CSprite2d::DrawRectSlow(scrollBarThumbInner, Color_black);

	if(m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ].m_Transposed)
	{
		float trackLeft = xEnd - 11;
		float trackRight = xEnd - 7;

		u32 totalFiles = buffer.m_NumFilesInSelectedBar;
		u32 totalFilesSoFar = 0;
		s32 numModules = GetNumTrackedModules();

		for (u32 i=0; i<numModules; i++)
		{
			u32 numFiles = buffer.m_NumFilesInSelectedCategoryModule[i];

			float yStartFraction = (float)totalFilesSoFar / (float)totalFiles;
			float yEndFraction = (float)(totalFilesSoFar + numFiles) / (float)totalFiles;
			float yStartCategory = Lerp(yStartFraction, yStart, yEnd);
			float yEndCategory = Lerp(yEndFraction, yStart, yEnd);

			Color32 categoryColor  = GetLegendColour(i);
			fwRect scrollBarTrack(trackLeft,yEndCategory, trackRight, yStartCategory);
			CSprite2d::DrawRectSlow(scrollBarTrack, categoryColor);

			totalFilesSoFar += numFiles;
		}
	}
	else if(m_ShowCategoryBreakdown)
	{
		float trackLeft = xEnd - 11;
		float trackRight = xEnd - 7;

		u32 totalFiles = buffer.m_NumFilesInSelectedBar;
		u32 totalFilesSoFar = 0;
		s32 numCategories = sizeof(sAssetCategoryLegend)/sizeof(sAssetCategoryLegend[0]);

		for (u32 i=0; i<numCategories; i++)
		{
			u32 numFiles = buffer.m_NumFilesInSelectedModuleCategory[i];

			float yStartFraction = (float)totalFilesSoFar / (float)totalFiles;
			float yEndFraction = (float)(totalFilesSoFar + numFiles) / (float)totalFiles;
			float yStartCategory = Lerp(yStartFraction, yStart, yEnd);
			float yEndCategory = Lerp(yEndFraction, yStart, yEnd);

			Color32 categoryColor  = GetLegendColour(i);
			fwRect scrollBarTrack(trackLeft,yEndCategory, trackRight, yStartCategory);
			CSprite2d::DrawRectSlow(scrollBarTrack, categoryColor);

			totalFilesSoFar += numFiles;
		}
	}
	else
	{
		fwRect scrollBarTrack(xEnd - 11,yEnd, xEnd - 7, yStart);
		CSprite2d::DrawRectSlow(scrollBarTrack, Color_white);
	}

	// header
	Color32 color = Color_white;
	DebugTextLayout.SetColor(color);
	char fileListHeader[128];
	const char * barName = buffer.m_Transposed ? sAssetCategoryLegend[ buffer.m_CurrentlySelectedCategoryIdx ].label : GetSelectedModule(buffer)->GetModuleName();
	formatf(fileListHeader, 128, "%s: %i files", barName, buffer.m_NumFilesInSelectedBar);
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, yStart - headerSize), fileListHeader);

	// Show the size of the currently selected item.
	char sizeInfo[64];
	formatf(sizeInfo, "Selected: %dKB (M), %dKB (V)", buffer.m_SelectedFileData.m_VirtualSize / 1024, buffer.m_SelectedFileData.m_PhysicalSize / 1024);
	DebugTextLayout.SetColor(Color_green);
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, yStart), sizeInfo);
	yStart += 18*STREAMGRAPH_FONTSCALE_FACTOR();

	fwRect headerUnderline(xStart,yStart - 2,xEnd - 4,yStart - 1);
	CSprite2d::DrawRectSlow(headerUnderline, Color_white);

	// list
	for(int i = 0; i < buffer.m_NumListEntries; ++i)
	{
		Color32 color = i == (buffer.m_SelectedListEntry - buffer.m_ListOffset) ? Color_green : Color_white;
		DebugTextLayout.SetColor(color);
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, i * 16 + yStart), buffer.m_ListBuffer[i].m_fileName);

		// Draw the memory size to the right
		char textBuffer[256];
		sprintf(textBuffer, "%dK(M), %dK(V)", buffer.m_ListBuffer[i].m_vSize / 1024, buffer.m_ListBuffer[i].m_pSize / 1024);
		Vector2	textWrap(0.0f, (xEnd-24.0f)/grcViewport::GetDefaultScreen()->GetWidth());
		DebugTextLayout.SetWrap(textWrap);
		DebugTextLayout.SetOrientation(FONT_RIGHT);
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(0.0f, i * 16 + yStart), textBuffer);
		DebugTextLayout.SetOrientation(FONT_LEFT);
	}
}

void CStreamGraph::CreateAssetInfoString(strIndex index, char *outBuffer, size_t bufferSize)
{
	char flagString[256];
	char refCntString[32];

	strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);
	strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);

	CreateFlagString(info.GetFlags(), flagString, sizeof(flagString));
	module->GetRefCountString(module->GetObjectIndex(index), refCntString, sizeof(refCntString));

	formatf(outBuffer, bufferSize, "%-40s %s dp=%d %s", strStreamingEngine::GetObjectName(index), refCntString, info.GetDependentCount(), flagString);
}

void CStreamGraph::DrawFileInfo(float x, float y)
{
	char achMsg[400];
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	
	CStreamGraphBuffer & buffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ];
	const CStreamGraphSelectedFileData &fileData = buffer.m_SelectedFileData;

	y += STREAMGRAPH_MODULE_SPACING(); 

	// hack so drawable data fits on screen
	{
		x = 80;
		y = 80;
	}

	// Finally draw the current file from the current module being enumerated with the direction keys
	u32 flags = buffer.m_SelectedFileData.m_Flags;

	strStreamingModule* selectedModule = GetSelectedModule(buffer);
	if(selectedModule)
	{
		strLocalIndex objIdx = strLocalIndex(buffer.m_CurrentlySelectedFileObjIdx);
		strIndex streamIndex = selectedModule->GetStreamingIndex(objIdx);
		int dependentCount = strStreamingEngine::GetInfo().GetStreamingInfoRef(streamIndex).GetDependentCount();

		snprintf( achMsg, NELEM(achMsg) - 1, "(%s), file %s main = %d vram = %d\nstrIndex=%d objIndex=%d\n%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s dp=%d", 
			selectedModule->GetModuleName(),
			buffer.m_SelectedFileData.m_Name,
			buffer.m_SelectedFileData.m_VirtualSize,
			buffer.m_SelectedFileData.m_PhysicalSize, 
			streamIndex.Get(), objIdx.Get(),
			flags & STRFLAG_DONTDELETE				? "DONT_DELETE" : "",
			flags & STRFLAG_FORCE_LOAD				? "FORCE_LOAD" : "",
			flags & STRFLAG_PRIORITY_LOAD			? "PRIORITY_LOAD" : "",
			flags & STRFLAG_LOADSCENE				? "LOAD_SCENE" : "",
			flags & STRFLAG_MISSION_REQUIRED		? "MISSION_REQUIRED" : "",
			flags & STRFLAG_CUTSCENE_REQUIRED		? "CUTSCENE_REQUIRED" : "",
			flags & STRFLAG_INTERIOR_REQUIRED		? "INTERIOR_REQUIRED" : "",
			flags & STRFLAG_ZONEDASSET_REQUIRED		? "ZONEDASSET_REQUIRED" : "",
			flags & STRFLAG_INTERNAL_LIVE			? "LIVE" : "",
			flags & STRFLAG_INTERNAL_DUMMY			? "DUMMY" : "", 
			flags & STRFLAG_INTERNAL_COMPRESSED		? "COMPRESSED" : "",
			flags & STRFLAG_INTERNAL_RESOURCE		? "RESOURCE" : "",
			flags & STRFLAG_INTERNAL_DEFRAGGING		? "DEFRAGGING" : "",
			flags & STRFLAG_INTERNAL_DONT_DEFRAG	? "DONT_DEFRAG" : "",
			flags & STRFLAG_INTERNAL_PLACING		? "PLACING" : "",
			flags & STRFLAG_INTERNAL_UNUSED_2		? "UNUSED2" : "",
			dependentCount
			);

		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), achMsg);

		// Add ref count information
		if (m_ShowExtendedFileInfo)
		{
			y+= 48*STREAMGRAPH_FONTSCALE_FACTOR();

			// Show dependencies
			char refCountInfo[256] = "Ref Cnt: ";
			selectedModule->GetRefCountString(objIdx, &refCountInfo[9], sizeof(refCountInfo));
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), refCountInfo);

			char assetString[256];
			strIndex deps[256];
			int depCount = selectedModule->GetDependencies(objIdx, deps, NELEM(deps));

			if (depCount)
			{
				y+= 16*STREAMGRAPH_FONTSCALE_FACTOR();
				DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), "IMMEDIATE DEPENDENCIES:");

				for (int i=0; i<depCount; i++)
				{
					if(strStreamingEngine::GetInfo().HasObjectLoaded(deps[i]))
					{
						y+= 16*STREAMGRAPH_FONTSCALE_FACTOR();
						CreateAssetInfoString(deps[i], assetString, sizeof(assetString));
						DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x + 32, y), assetString);
					}
				}
			}

			// Show dependents
			if (fileData.m_DependentCount)
			{
				y+= 16*STREAMGRAPH_FONTSCALE_FACTOR();
				DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), "IMMEDIATE DEPENDENTS:");

				for (int i=0; i<fileData.m_DependentCount; i++)
				{
					if(strStreamingEngine::GetInfo().HasObjectLoaded(fileData.m_Dependents[i]))
					{
						y+= 16*STREAMGRAPH_FONTSCALE_FACTOR();
						CreateAssetInfoString(fileData.m_Dependents[i], assetString, sizeof(assetString));
						DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x + 32, y), assetString);
					}
				}
			}
		}

		Color32 color = Color_white;
		DebugTextLayout.SetColor(color);

		for(int i = 0; i < buffer.m_NumFileInfoEntries; ++i)
		{		
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, i * 16 + y + 32), buffer.m_FileInfoBuffer[i]);
		}
	}
}

void CStreamGraph::DrawDebug()
{
	if ( !m_ShowMemoryTracker || ( !m_ShowPhysicalMemory && !m_ShowVirtualMemory ) )
	{
		return;
	}

	if( m_ShowLODBreakdown )
	{
		DrawLODStats();
		return;
	}

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));

	CStreamGraphBuffer & buffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ];
	s32 numModules = GetNumTrackedModules();

	DrawTitle();
	DrawBackground();

	s32 yAxisCount = numModules;
	s32 xAxisCount = sizeof(sAssetCategoryLegend)/sizeof(sAssetCategoryLegend[0]);
	if ( buffer.m_Transposed )
	{
		std::swap( xAxisCount, yAxisCount );
	}

	float barOffset = 0.0f;

	// Start off with the master memory display
	barOffset += DrawMasterMemoryBar(m_vPos.x, m_vPos.y + barOffset);

	// Draw streaming graph bars
	for ( s32 yAxisIdx = 0; yAxisIdx < yAxisCount; yAxisIdx++ )
	{
		barOffset += DrawMemoryBar(yAxisIdx, xAxisCount, m_vPos.x, m_vPos.y + barOffset);
	}

	// Draw any extension bars
	for(int i=0;i<m_pStreamGraphExtManager->GetNumExtensions();i++)
	{
		CStreamGraphExtensionBase *pExtension = m_pStreamGraphExtManager->GetExtension(i);
		if( pExtension->IsActive() )
		{
			barOffset += pExtension->DrawMemoryBar(m_vPos.x, m_vPos.y + barOffset);
		}
	}

	if(!m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER])
	{
		// Normal streamgraph selected 
		float legendBottom = DrawLegend(m_vPos.x + GetGraphWidth() + 20, m_vPos.y + 5);
		DrawFileList(m_vPos.x + GetWindowWidth() - GetFileListWidth(), legendBottom);
		if (m_ShowFileInfo)
		{
			DrawFileInfo(m_vPos.x, m_vPos.y + 540*STREAMGRAPH_FONTSCALE_FACTOR());
		}
	}
	else
	{
		// Extension selected
		CStreamGraphExtensionBase *pExtension = m_pStreamGraphExtManager->GetSelectedExt(SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER);
		float legendBottom = pExtension->DrawLegend(m_vPos.x + GetGraphWidth() + 20, m_vPos.y + 5);
		pExtension->DrawFileList(m_vPos.x + GetWindowWidth() - GetFileListWidth(), legendBottom);
		if (m_ShowFileInfo)
		{
			pExtension->DrawFileInfo(80*STREAMGRAPH_FONTSCALE_FACTOR(),80*STREAMGRAPH_FONTSCALE_FACTOR());
		}
	}
}

void CStreamGraph::SetFirstSelected()
{
	CStreamGraphBuffer &buffer  = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	buffer.m_CurrentlySelectedCategoryIdx = 0;

	buffer.m_CurrentlySelectedModuleIdx = 0;
	while( !buffer.m_Modules[ buffer.m_CurrentlySelectedModuleIdx ].HasNonZeroCategory( m_ShowVirtualMemory, m_ShowPhysicalMemory ) )
	{
		buffer.m_CurrentlySelectedModuleIdx++;
	}

	buffer.m_CurrentlySelectedFileIdx  = 0;
	buffer.m_CurrentlySelectedFileObjIdx = 0;
}

void CStreamGraph::SetLastSelected()
{
	CStreamGraphBuffer &buffer  = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	buffer.m_CurrentlySelectedCategoryIdx = GetNumTrackedCategories() - 1;
	// Assumes at least one module will have some data, or it's going to go badly wrong!
	buffer.m_CurrentlySelectedModuleIdx = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules() - 1;
	while( !buffer.m_Modules[ buffer.m_CurrentlySelectedModuleIdx ].HasNonZeroCategory( m_ShowVirtualMemory, m_ShowPhysicalMemory ) )
	{
		buffer.m_CurrentlySelectedModuleIdx--;
	}
	buffer.m_CurrentlySelectedFileIdx  = 0;
	buffer.m_CurrentlySelectedFileObjIdx = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::UpdateMemoryBarSelection()
{
	CStreamGraphBuffer &buffer  = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	if(m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE])
	{
		// Selecting Extension
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG, "move up in extension lists"))
		{
			// Next one up
			s32	next = m_pStreamGraphExtManager->GetPrevActive();
			if( next != -1)
			{
				m_pStreamGraphExtManager->SetSelected(next);
			}
			else
			{
				// Setup whatever we need to transition back to the main bars

				// Set the streamgraph normal bars to be the last one
				SetLastSelected();

				m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = false;
				m_pStreamGraphExtManager->SetSelected(-1);
			}
		}
		else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG, "move down in extension lists") )
		{
			// Next one down
			s32	next = m_pStreamGraphExtManager->GetNextActive();
			if( next != -1)
			{
				m_pStreamGraphExtManager->SetSelected(next);
			}
			else
			{
				// Setup whatever we need to transition back to the main bars

				// Set the streamgraph normal bars to be the first one
				SetFirstSelected();

				m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = false;
				m_pStreamGraphExtManager->SetSelected(-1);
			}
		}

		// A shit bodge incase someone decides to use the transpose button when on the category screen, 
		//	because the category screen part uses m_CurrentlySelectedModuleIdx for other functionality and and can set it to be -1.
		buffer.m_CurrentlySelectedModuleIdx = Clamp( buffer.m_CurrentlySelectedModuleIdx, 0, strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules() - 1 );
	}
	else

	if(m_TransposeGraph)
	{
		s32 originalCategorySelection = buffer.m_CurrentlySelectedCategoryIdx;

		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG, "move up in category lists"))
		{
			--buffer.m_CurrentlySelectedCategoryIdx;
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG, "move down in category lists") )
		{
			++buffer.m_CurrentlySelectedCategoryIdx;
		}

		if( buffer.m_CurrentlySelectedCategoryIdx < 0 )
		{
			if( m_pStreamGraphExtManager->GetActiveCount() )
			{
				m_pStreamGraphExtManager->SetLastSelected();
				m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = true;
				//return;
			}
		}
		else if( buffer.m_CurrentlySelectedCategoryIdx >= GetNumTrackedCategories() )
		{
			if( m_pStreamGraphExtManager->GetActiveCount() )
			{
				m_pStreamGraphExtManager->SetFirstSelected();
				m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = true;
				//return;
			}
		}


		buffer.m_CurrentlySelectedCategoryIdx = Clamp(buffer.m_CurrentlySelectedCategoryIdx, 0, GetNumTrackedCategories() - 1);

		if ( originalCategorySelection != buffer.m_CurrentlySelectedCategoryIdx )
		{
			buffer.m_CurrentlySelectedFileIdx  = 0;
			buffer.m_CurrentlySelectedFileObjIdx = 0;
			buffer.m_CurrentlySelectedModuleIdx = 0;
		}
	}
	else
	{
		buffer.m_CurrentlySelectedModuleIdx = Clamp( buffer.m_CurrentlySelectedModuleIdx, 0, strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules() - 1 );

		s32 nextModuleSelection = buffer.m_CurrentlySelectedModuleIdx;

		if( !buffer.m_Modules[ buffer.m_CurrentlySelectedModuleIdx ].HasNonZeroCategory( m_ShowVirtualMemory, m_ShowPhysicalMemory ) ||
			CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG, "move up in category lists"))
		{
			do
			{
				nextModuleSelection--;
				if ( nextModuleSelection < 0 )
				{
					// We've gone off the end, does this push us into the Extensions?
					if( m_pStreamGraphExtManager->GetActiveCount() )
					{
						m_pStreamGraphExtManager->SetLastSelected();
						m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = true;
						return;
					}

					nextModuleSelection += strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
				}

				if ( buffer.m_Modules[ nextModuleSelection ].HasNonZeroCategory( m_ShowVirtualMemory, m_ShowPhysicalMemory ) )
				{
					break;
				}

			} while ( buffer.m_CurrentlySelectedModuleIdx != nextModuleSelection );
		}

		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG, "move down in category lists") )
		{
			do
			{
				nextModuleSelection++;
				if ( nextModuleSelection >= strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules() )
				{
					// We've gone off the end, does this push us into the Extensions?
					if( m_pStreamGraphExtManager->GetActiveCount() )
					{
						m_pStreamGraphExtManager->SetFirstSelected();
						m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = true;
						return;
					}

					nextModuleSelection -= strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
				}

				if ( buffer.m_Modules[ nextModuleSelection ].HasNonZeroCategory( m_ShowVirtualMemory, m_ShowPhysicalMemory ) )
				{
					break;
				}

			} while ( buffer.m_CurrentlySelectedModuleIdx != nextModuleSelection );
		}

		if ( nextModuleSelection != buffer.m_CurrentlySelectedModuleIdx )
		{
			buffer.m_CurrentlySelectedFileIdx  = 0;
			buffer.m_CurrentlySelectedFileObjIdx = 0;
			buffer.m_CurrentlySelectedModuleIdx = nextModuleSelection;
			buffer.m_CurrentlySelectedCategoryIdx = 0;
		}
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::UpdateRepositionWindow()
{
	// handle mouse click
	const Vector2 mousePosition = Vector2( static_cast<float>(ioMouse::GetX()), static_cast<float>(ioMouse::GetY()) );

	if (m_bMove)
	{
		// already dragging so adjust position of results boxSTREAMGRAPH_TITLE_HEIGHT
		m_vPos = mousePosition - m_vClickDragDelta;
		if (m_vPos.x<0.0f)
		{
			m_vPos.Set(0.0f, m_vPos.y);
			m_vClickDragDelta = mousePosition - m_vPos;
		}
		if (m_vPos.y<0.0f)
		{
			m_vPos.Set(0.0f, m_vPos.y);
			m_vClickDragDelta = mousePosition - m_vPos;
		}

		// check for button release
		if ((ioMouse::GetButtons()&ioMouse::MOUSE_LEFT) == false)
		{
			m_bMove = false;
		}
	}
	else
	{
		// check for player clicking on title bar
		if (ioMouse::GetButtons()&ioMouse::MOUSE_LEFT)
		{
			fwRect rect
				(
				m_vPos.x,
				m_vPos.y,
				m_vPos.x + GetGraphWidth(),
				m_vPos.y + STREAMGRAPH_TITLE_HEIGHT()
				);

			if (rect.IsInside(mousePosition))
			{
				m_bMove = true;
				m_vClickDragDelta = mousePosition - m_vPos;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::CheckToggleOnOff()
{
	if( m_ShowMemoryTracker && !m_ShowMemoryTrackerToggle )
	{
		// We just turned on
		AllocateMemoryForLODDisplay();
	}
	else if(!m_ShowMemoryTracker && m_ShowMemoryTrackerToggle)
	{
		// Just just turned off
		FreeMemoryForLODDisplay();
	}

	m_ShowMemoryTrackerToggle = m_ShowMemoryTracker;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::Update()
{
	// Keyboard shortcut
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_SHIFT, "show stream graph"))
	{
		// On PS3, we'll toggle between memory types.
#if __PPU
		if (m_ShowMemoryTracker)
		{
			// Here's a funny idea - let's turn the two bools into a two-bit integer.
			int memStatValue = (int) m_ShowVirtualMemory | ((int) m_ShowPhysicalMemory << 1);

			if (memStatValue < 2)
			{
				// Turn it off again if we went through all combinations.
				m_ShowMemoryTracker = false;
				m_ShowVirtualMemory = m_ShowPhysicalMemory = true;
			}
			else
			{
				// Create the next combination.
				memStatValue--;
				m_ShowVirtualMemory = (memStatValue & 1) != 0;
				m_ShowPhysicalMemory = (memStatValue & 2) != 0;
			}
		}
		else
#endif // __PPU
		m_ShowMemoryTracker = !m_ShowMemoryTracker;
	}

	CheckToggleOnOff();

	if ( !m_ShowMemoryTracker || ( !m_ShowPhysicalMemory && !m_ShowVirtualMemory ) )
	{
		return;
	}

	CStreamGraphBuffer &buffer  = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];
#if __DEV
	s32 originalSelectedModule = buffer.m_CurrentlySelectedModuleIdx;
	s32 originalSelectedObjectIndex = buffer.m_CurrentlySelectedFileObjIdx;
#endif

	UpdateRepositionWindow();
	UpdateMemoryBarSelection();
	CompareFunc compareFunc = GetCompareFunc();
	ResetFileList();

	BuildDependentOnList();

	if(m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE])
	{
		// Do the keys left and right here
		m_pStreamGraphExtManager->Update();
	}

	if( m_ShowLODBreakdown )
	{
		UpdateLodStats( );
	}
	else 
	if(m_TransposeGraph)
	{
		s32 numCategories = GetNumTrackedCategories();

		// update just one module each time round, in addition to the currently selected module
		static int updateCategory = 0;
		s32 categoryId = updateCategory % numCategories;
		++updateCategory;

		if(categoryId == buffer.m_CurrentlySelectedCategoryIdx)
		{
			UpdateCategoryStats(categoryId, compareFunc);
		}
		else
		{
			UpdateCategoryStats(categoryId,  NULL);
			UpdateCategoryStats(buffer.m_CurrentlySelectedCategoryIdx, compareFunc);
		}

	}

	else
	{
		s32 numModules = GetNumTrackedModules();

		// update just one module each time round, in addition to the currently selected module
		static int updateModule = 0;
		s32 moduleId = updateModule % numModules;
		++updateModule;
		//for ( s32 moduleId = 0; moduleId < numModules; moduleId++ )
		{
			// Not actually necessary to sort anything but the one we're looking at, as the sorting is only obvious when moving through the files on the
			// currently selected module with left/right.
			UpdateModuleStats( moduleId, moduleId == buffer.m_CurrentlySelectedModuleIdx ? compareFunc : NULL );
		}

		if( buffer.m_CurrentlySelectedModuleIdx != -1)
		{
			if(moduleId != buffer.m_CurrentlySelectedModuleIdx)
			{
				UpdateModuleStats(buffer.m_CurrentlySelectedModuleIdx, compareFunc);
			}
		}
	}

	// Render settings
	buffer.m_Transposed = m_TransposeGraph;

#if __DEV

	if ( buffer.m_CurrentlySelectedModuleIdx != originalSelectedModule || buffer.m_CurrentlySelectedFileObjIdx != originalSelectedObjectIndex )
	{
		strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
		if(buffer.m_CurrentlySelectedModuleIdx >= 0)
		{
			strIndex streamingIndex = strInfoManager.GetModuleMgr().GetModule(buffer.m_CurrentlySelectedModuleIdx)
									  ->GetStreamingIndex(strLocalIndex(buffer.m_CurrentlySelectedFileObjIdx));
			if(strInfoManager.IsObjectInImage(streamingIndex))
			{
				CStreamingDebug::SetResourceForEntityFraming(streamingIndex);
			}
		}
	}

	CStreamingDebug::UpdateObjectFraming();

#endif

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::SetSelectedFileName(CStreamGraphBuffer & buffer, const char * name)
{
	strncpy(buffer.m_SelectedFileData.m_Name, name, CStreamGraphSelectedFileData::MAX_FILE_NAME_LENGTH);
}


///////////////////////////////////////////////////////////////////////////////////////////////////

int CStreamGraph::GetNumTrackedCategories()
{
	return CDataFileMgr::CONTENTS_MAX;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int CStreamGraph::GetNumTrackedModules()
{
	return Min(strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules(), CStreamGraphBuffer::MAX_TRACKED_MODULES);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CStreamGraph::CompareFunc CStreamGraph::GetCompareFunc()
{
	if (m_OrderByName)
	{
		return CompareStreamingInfoByName;
	}
	else
	{
		if ( m_ShowCategoryBreakdown )
		{
			if ( m_ShowPhysicalMemory && m_ShowVirtualMemory )
			{
				return CompareStreamingInfoByCategoryTotalSize;
			}
			else if ( m_ShowVirtualMemory )
			{
				return CompareStreamingInfoByCategoryVirtSize;
			}
			else
			{
				return CompareStreamingInfoByCategoryPhysSize;
			}
		}
		else
		{
			if ( m_ShowPhysicalMemory && m_ShowVirtualMemory )
			{
				return CompareStreamingInfoByTotalSize;
			}
			else if ( m_ShowVirtualMemory )
			{
				return CompareStreamingInfoByVirtSize;
			}
			else
			{
				return CompareStreamingInfoByPhysSize;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

strStreamingModule * CStreamGraph::GetSelectedModule(const CStreamGraphBuffer & buffer)
{
	s32 moduleId = buffer.m_CurrentlySelectedModuleIdx;

	if(moduleId < 0 )
	{
		return NULL;
	}
	else
	{
		return strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
	}
}

void CStreamGraph::GetDrawableInfo(const Drawable & drawable, u32 & vertexCount, int & textureCount)
{
	vertexCount = 0;
	const rmcLodGroup& lodGroup = drawable.GetLodGroup();
	int lodIndex=0;
	while (lodIndex < rage::LOD_COUNT && lodGroup.ContainsLod(lodIndex) )
	{
		const rmcLod& lod = lodGroup.GetLod(lodIndex);

		int modelCount = lod.GetCount();
		for (int i=0; i<modelCount; ++i)
		{
			grmModel* model = lod.GetModel(i);

			int geomCount = model->GetGeometryCount();
			for (int j=0; j<geomCount; ++j)
			{
				grmGeometry& geometry = model->GetGeometry(j);
				vertexCount += geometry.GetVertexCount();

			}
		}
		lodIndex++;
	}

	textureCount = 0;
	const fwTxd * txd = drawable.GetShaderGroup().GetTexDict();

	if(txd)
	{
		textureCount = txd->GetCount();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::SwapBuffers()
{
	if(!m_ShowMemoryTracker)
		return;

	// copy dbl buffer stat vals
	sysMemCpy
		(
		&m_statBuffers[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER],
		&m_statBuffers[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE],
		sizeof(CStreamGraphBuffer)
		);

	// Copy LOD info buffers
	sysMemCpy( &m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER], &m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE], sizeof(MemoryByLODBuffer));

	m_pStreamGraphExtManager->SwapBuffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::CollectResultsForAutoCapture( atArray<streamingMemoryResult*> &results )
{
	USE_DEBUG_MEMORY();

	m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_CurrentlySelectedFileIdx	= -1;
	m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_CurrentlySelectedModuleIdx = -1;

	s32 numModules = GetNumTrackedModules();
	for ( s32 moduleId = 0; moduleId < numModules; moduleId++ )
	{
		UpdateModuleStats( moduleId, NULL );
	}

	for ( s32 moduleId = 0; moduleId < numModules; moduleId++ )
	{
		streamingMemoryResult *result = rage_new streamingMemoryResult;
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
		result->moduleName.Set( pModule->GetModuleName(), istrlen(pModule->GetModuleName()), 0, -1 );
 
		for ( u32 categoryIdx = 0; categoryIdx < CDataFileMgr::CONTENTS_MAX; categoryIdx++ )
		{
			result->categories[ categoryIdx ].categoryName.Set( sAssetCategoryLegend[ categoryIdx ].label, istrlen( sAssetCategoryLegend[ categoryIdx ].label ), 0, -1 );
			result->categories[ categoryIdx ].physicalMemory = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_Modules[ moduleId ].m_CostPhys[ categoryIdx ];
			result->categories[ categoryIdx ].virtualMemory  = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_Modules[ moduleId ].m_CostVirt[ categoryIdx ];
		} 

		results.Grow() = result;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int CStreamGraph::CompareStreamingInfoByVirtSize(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	return pB->virtualSize - pA->virtualSize;
}

int CStreamGraph::CompareStreamingInfoByPhysSize(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	return pB->physicalSize - pA->physicalSize;
}

int CStreamGraph::CompareStreamingInfoByTotalSize(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	return (pB->virtualSize+pB->physicalSize) - (pA->virtualSize+pA->physicalSize);
}

int CStreamGraph::CompareStreamingInfoByCategoryPhysSize(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	if ( pA->category == pB->category )
	{
		return CompareStreamingInfoByPhysSize( pVoidA, pVoidB );
	}
	return pA->category - pB->category;
}

int CStreamGraph::CompareStreamingInfoByCategoryVirtSize(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	if ( pA->category == pB->category )
	{
		return CompareStreamingInfoByVirtSize( pVoidA, pVoidB );
	}
	return pA->category - pB->category;
}

int CStreamGraph::CompareStreamingInfoByCategoryTotalSize(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	if ( pA->category == pB->category )
	{
		return CompareStreamingInfoByTotalSize( pVoidA, pVoidB );
	}
	return pA->category - pB->category;
}

int CStreamGraph::CompareStreamingInfoByName(const void* pVoidA, const void* pVoidB)
{
	GraphEntry* pA = (GraphEntry*)pVoidA;
	GraphEntry* pB = (GraphEntry*)pVoidB;
	return strcmp(pA->name, pB->name);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

u32 CStreamGraph::GetStreamingCategory( strStreamingInfo *info )
{
	s32 imageIndex = strPackfileManager::GetImageFileIndexFromHandle(info->GetHandle()).Get();
	strStreamingFile* file = NULL;  
	if (imageIndex != -1)
	{
		file = strPackfileManager::GetImageFile( imageIndex );
	}
	u32 categoryIdx = CDataFileMgr::CONTENTS_DEFAULT;
	if ( file )
	{
		if ( file->m_contentsType < CDataFileMgr::CONTENTS_MAX )
		{
			categoryIdx = file->m_contentsType; 
		}
	}
	return ( categoryIdx );
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::ResetFileList()
{
	CStreamGraphBuffer &updateBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	updateBuffer.m_NumListEntries = 0;
	updateBuffer.m_SelectedListEntry = -1;
	updateBuffer.m_NumFilesInSelectedBar = 0;
	memset( updateBuffer.m_NumFilesInSelectedModuleCategory, 0, sizeof(updateBuffer.m_NumFilesInSelectedModuleCategory));
	memset( updateBuffer.m_NumFilesInSelectedCategoryModule, 0, sizeof(updateBuffer.m_NumFilesInSelectedCategoryModule));
}

///////////////////////////////////////////////////////////////////////////////////////////////////


void CStreamGraph::AddDependenciesToMap(strIndex index, atMap<strIndex, bool> &outMap)
{
	if (outMap.Access(index) == NULL)
	{
		outMap.Insert(index, true);
		
		strIndex deps[256];
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
		strLocalIndex objIndex = pModule->GetObjectIndex(index);

		int depCount = pModule->GetDependencies(objIndex, deps, NELEM(deps));

		for (int x=0; x<depCount; x++)
		{
			AddDependenciesToMap(deps[x], outMap);
		}
	}
}

void CStreamGraph::BuildDependentOnList()
{
	USE_DEBUG_MEMORY();

	if( m_OnlyIncludeCutsceneRequestedObjects || m_ExcludeCutsceneRequestedObjects /*|| m_OnlyIncludeScriptRequestedObjects*/ )
	{
		// Clear out the list of files
		m_CutsceneRequired.Reset();

		for(int moduleId = 0; moduleId < GetNumTrackedModules(); ++moduleId)
		{
			strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);

			for(s32 objIdx = 0; objIdx < pModule->GetCount(); ++objIdx)
			{
				strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
				strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(objIdx));

				if ( strInfoManager.IsObjectInImage(streamingIndex) )
				{
					bool bLoaded  = strInfoManager.HasObjectLoaded(streamingIndex);
					bool bRequested = strInfoManager.IsObjectRequested(streamingIndex) || strInfoManager.IsObjectLoading(streamingIndex);

					if( bLoaded || ( bRequested && m_IncludeRequestedObjects ) )
					{
						strStreamingInfo* info = strInfoManager.GetStreamingInfo(streamingIndex);

						bool isCutsceneRequired = (info->GetFlags() & STRFLAG_CUTSCENE_REQUIRED) != 0;

//						if(	m_OnlyIncludeCutsceneRequestedObjects == isCutsceneRequired && m_ExcludeCutsceneRequestedObjects != isCutsceneRequired )
						if (isCutsceneRequired)
						{
							// Add to list
							AddDependenciesToMap(streamingIndex, m_CutsceneRequired);
						}
					}
				}
			}
		}
	}
	else if( m_OnlyIncludeScriptRequestedObjects )
	{
		// Clear out the list of files
		m_ScriptDependentOnList.clear();
		// Get the list of items and all the dependencies (minus ignored items)
		CTheScripts::GetScriptHandlerMgr().GetScriptDependencyList(m_ScriptDependentOnList);
	}

}

// Whizz through the list m_DependantOnList, to see if this object is a dependancy of one of them.
bool CStreamGraph::CheckIfDependency(atArray<strIndex> &dependentList, strIndex streamingIndex)
{
	for(int i=0;i<dependentList.size();i++)
	{
		// Get the dependents of the parent
		strIndex parentDeps[STREAMING_MAX_DEPENDENCIES];
		strStreamingModule* pParentModule = strStreamingEngine::GetInfo().GetModule(dependentList[i]);
		strLocalIndex	parentObjIDX = pParentModule->GetObjectIndex(dependentList[i]);
		s32 parentDepsCount = pParentModule->GetDependencies(parentObjIDX, &parentDeps[0], STREAMING_MAX_DEPENDENCIES);

		for(int j=0;j<parentDepsCount;j++)
		{
			if(parentDeps[j] == streamingIndex)
				return true;
			else
			{
				if( CheckChildren(parentDeps[j], streamingIndex ) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CStreamGraph::CheckChildren(strIndex parentStrIdx, strIndex thisStrIdx)
{
	strIndex parentDeps[STREAMING_MAX_DEPENDENCIES];

	strStreamingModule *pParentModule = strStreamingEngine::GetInfo().GetModule(parentStrIdx);
	strLocalIndex	parentObjIdx = pParentModule->GetObjectIndex(parentStrIdx);
	s32 parentDepsCount = pParentModule->GetDependencies(parentObjIdx, &parentDeps[0], STREAMING_MAX_DEPENDENCIES);

	for(int i=0;i<parentDepsCount;i++)
	{
		if(parentDeps[i] == thisStrIdx)
			return true;
		else
		{
			if( CheckChildren(parentDeps[i], thisStrIdx) )
			{
				return true;
			}
		}
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::UpdateFileList(const GraphEntry * sortedEntries, const int entryCount)
{
	int fileCount = 0;
	CStreamGraphBuffer &updateBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	if(m_TransposeGraph)
	{
		for (int i = 0; i < entryCount; i++ )
		{
			const GraphEntry & entry = sortedEntries[i];
			strStreamingModule* module = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(entry.module);
			strIndex sortedIndex = entry.index;
			strLocalIndex sortedObjectIndex = module->GetObjectIndex(sortedIndex);
			++updateBuffer.m_NumFilesInSelectedCategoryModule[entry.module];

			if(sortedObjectIndex == updateBuffer.m_CurrentlySelectedFileObjIdx && entry.module == updateBuffer.m_CurrentlySelectedModuleIdx)
			{
				updateBuffer.m_SelectedListEntry = updateBuffer.m_NumFilesInSelectedBar + fileCount;

				// scroll the selected file in to view
				if(updateBuffer.m_SelectedListEntry < updateBuffer.m_ListOffset)
				{
					updateBuffer.m_ListOffset = updateBuffer.m_SelectedListEntry;
				}
				else if(updateBuffer.m_SelectedListEntry >= updateBuffer.m_ListOffset + updateBuffer.MAX_LIST_ENTRIES_TRANSPOSED)
				{
					updateBuffer.m_ListOffset = updateBuffer.m_SelectedListEntry - (updateBuffer.MAX_LIST_ENTRIES_TRANSPOSED - 1);
				}

				UpdateFileInfo(entry, module);
			}

			if((updateBuffer.m_NumFilesInSelectedBar + fileCount) >= updateBuffer.m_ListOffset && updateBuffer.m_NumListEntries != updateBuffer.MAX_LIST_ENTRIES_TRANSPOSED)
			{
				const char *fileName = strStreamingEngine::GetObjectName(sortedIndex);

				CStreamGraphBuffer::listBufferContents *pListBuffer = &updateBuffer.m_ListBuffer[updateBuffer.m_NumListEntries++];
				char * lineBuffer = pListBuffer->m_fileName;
				strncpy(lineBuffer, fileName, updateBuffer.MAX_LIST_ENTRY_LENGTH);
				strStreamingInfo* info = module->GetStreamingInfo( sortedObjectIndex );
				GetSizesSafe(sortedIndex, *info, pListBuffer->m_vSize, pListBuffer->m_pSize, false );
			}

			++fileCount;
		}
	}
	else
	{
		strStreamingModule* module = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(updateBuffer.m_CurrentlySelectedModuleIdx);

		for (int i = 0; i < entryCount; i++ )
		{
			strIndex sortedIndex = sortedEntries[i].index;
			++updateBuffer.m_NumFilesInSelectedModuleCategory[sortedEntries[i].category];

			if(i == updateBuffer.m_CurrentlySelectedFileIdx)
			{
				updateBuffer.m_SelectedListEntry = fileCount;
				
				// scroll the selected file in to view
				if(updateBuffer.m_SelectedListEntry < updateBuffer.m_ListOffset)
				{
					updateBuffer.m_ListOffset = updateBuffer.m_SelectedListEntry;
				}
				else if(updateBuffer.m_SelectedListEntry >= updateBuffer.m_ListOffset + updateBuffer.MAX_LIST_ENTRIES)
				{
					updateBuffer.m_ListOffset = updateBuffer.m_SelectedListEntry - (updateBuffer.MAX_LIST_ENTRIES - 1);
				}

				UpdateFileInfo(sortedEntries[updateBuffer.m_CurrentlySelectedFileIdx], module);
			}

			if(fileCount >= updateBuffer.m_ListOffset && updateBuffer.m_NumListEntries != updateBuffer.MAX_LIST_ENTRIES)
			{
				const char *fileName = strStreamingEngine::GetObjectName(sortedIndex);

				CStreamGraphBuffer::listBufferContents *pListBuffer = &updateBuffer.m_ListBuffer[updateBuffer.m_NumListEntries++];
				char * lineBuffer = pListBuffer->m_fileName;
				strncpy(lineBuffer, fileName, updateBuffer.MAX_LIST_ENTRY_LENGTH);
				strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo( sortedIndex );
				GetSizesSafe(sortedIndex, *info, pListBuffer->m_vSize, pListBuffer->m_pSize, false );
			}

			++fileCount;
		}
	}

	updateBuffer.m_NumFilesInSelectedBar += fileCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CStreamGraph::UpdateFileInfo(const GraphEntry & file, strStreamingModule* pModule)
{
	CStreamGraphBuffer &updateBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	updateBuffer.m_NumFileInfoEntries = 0;
	if(pModule == &g_TxdStore)
	{
		strLocalIndex txdObjectIndex = strLocalIndex(pModule->GetObjectIndex(file.index));
		fwTxd * txd = g_TxdStore.GetSafeFromIndex(txdObjectIndex);
		if(txd)
		{
			int texCount = txd->GetCount();
			formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH, texCount == 1 ? "%i texture" : "%i textures", texCount);
			++updateBuffer.m_NumFileInfoEntries;

			if(m_TrackTxd)
			{
				CTxdRef	ref(AST_TxdStore, txdObjectIndex.Get(), INDEX_NONE, "");
				CDebugTextureViewerInterface::SelectTxd(ref, false, true, false);
			}
		}
	}

	else if(pModule == &g_DrawableStore)
	{
		u32 vertexCount = 0;
		int textureCount = 0;

		strLocalIndex drawableObjectIndex = pModule->GetObjectIndex(file.index);
		Drawable * drawable = g_DrawableStore.GetSafeFromIndex(drawableObjectIndex);
		if(drawable)
		{
			GetDrawableInfo(*drawable, vertexCount, textureCount);
			if(m_TrackTxd)
			{
				CTxdRef	ref(AST_DrawableStore, drawableObjectIndex.Get(), INDEX_NONE, "");
				CDebugTextureViewerInterface::SelectTxd(ref, false, true, false);
			}
		}

		formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries],
			updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
			"%u vertices, %i texture(s)", vertexCount, textureCount);
		++updateBuffer.m_NumFileInfoEntries;
	}
	else if(pModule == &g_DwdStore)
	{
		strLocalIndex dwdObjectIndex = pModule->GetObjectIndex(file.index);
		Dwd * dwd = g_DwdStore.GetSafeFromIndex(dwdObjectIndex);
		if(dwd)
		{
			int drawableCount = dwd->GetCount();
			formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH, drawableCount == 1 ? "%i drawable" : "%i drawables", drawableCount);
			++updateBuffer.m_NumFileInfoEntries;
			for(int drawableIndex = 0; drawableIndex < drawableCount; ++drawableIndex)
			{
				Drawable * drawable = dwd->GetEntry(drawableIndex);
				if(drawable && updateBuffer.m_NumFileInfoEntries < CStreamGraphBuffer::MAX_FILE_INFO_ENTRIES)
				{
					u32 vertexCount = 0;
					int textureCount = 0;
					GetDrawableInfo(*drawable, vertexCount, textureCount);
					formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries],
					updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"%u vertices, %i texture(s)", vertexCount, textureCount);
					++updateBuffer.m_NumFileInfoEntries;
				}
			}
			if(m_TrackTxd)
			{
				CTxdRef	ref(AST_DwdStore, dwdObjectIndex.Get(), 0, "");
				CDebugTextureViewerInterface::SelectTxd(ref, false, true, false);
			}
		}
	}
	else if(pModule == &g_FragmentStore)
	{
		strLocalIndex fragObjectIndex = pModule->GetObjectIndex(file.index);
		Fragment * fragment = g_FragmentStore.GetSafeFromIndex(fragObjectIndex);
		if(fragment && fragment->GetCommonDrawable())
		{
			u32 vertexCount = 0;
			const rmcLodGroup& lodGroup = fragment->GetCommonDrawable()->GetLodGroup();
			int lodIndex=0;
			while (lodGroup.ContainsLod(lodIndex))
			{
				const rmcLod& lod = lodGroup.GetLod(lodIndex);

				int modelCount = lod.GetCount();
				for (int i=0; i<modelCount; ++i)
				{
					grmModel* model = lod.GetModel(i);

					int geomCount = model->GetGeometryCount();
					for (int j=0; j<geomCount; ++j)
					{
						grmGeometry& geometry = model->GetGeometry(j);
						vertexCount += geometry.GetVertexCount();

					}
				}
				lodIndex++;
			}

			formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries],
				updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
				"%i vertices", vertexCount);
			++updateBuffer.m_NumFileInfoEntries;

			fwTxd * txd = fragment->GetCommonDrawable()->GetShaderGroup().GetTexDict();
			if(txd)
			{
				int texCount = txd->GetCount();
				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH, texCount == 1 ? "%i texture" : "%i textures", texCount);
				++updateBuffer.m_NumFileInfoEntries;

				if(m_TrackTxd)
				{
					CTxdRef	ref(AST_FragmentStore, fragObjectIndex.Get(), INDEX_NONE, "");
					CDebugTextureViewerInterface::SelectTxd(ref, false, true, false);
				}
			}
		}
	}
#if __STATS
	else if (pModule == &g_ScaleformStore)
	{
		strLocalIndex scaleformObjectIdx = pModule->GetObjectIndex(file.index);
		CScaleformMovieObject* movie = g_ScaleformStore.GetSafeFromIndex(scaleformObjectIdx);
		if (movie)
		{
			s32 scaleformMovieIdx = CScaleformMgr::FindMovieByFilename(g_ScaleformStore.GetName(strLocalIndex(scaleformObjectIdx)));
			if (scaleformMovieIdx >= 0 && CScaleformMgr::IsMovieActive(scaleformMovieIdx))
			{
				ScaleformMemoryStats stats;
				CScaleformMgr::GatherMemoryStats(scaleformMovieIdx, stats);

				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"MovieDef: %.3fk in-use (%dk total)", (float)stats.m_MovieDefUsed / 1024.0f, stats.m_MovieDefTotal / 1024);
				++updateBuffer.m_NumFileInfoEntries;

				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"MovieView %.3fk in-use (%dk total)", (float)stats.m_MovieViewUsed / 1024.0f, stats.m_MovieViewTotal / 1024);
				++updateBuffer.m_NumFileInfoEntries;

				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"Prealloc: %dk in-use", stats.m_PreallocUsed / 1024);
				++updateBuffer.m_NumFileInfoEntries;

				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"     (%dk total, %dk peak)", stats.m_PreallocTotal / 1024, stats.m_PreallocPeak / 1024);
				++updateBuffer.m_NumFileInfoEntries;

				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"Overalloc: %dk", stats.m_Overalloc / 1024);
				++updateBuffer.m_NumFileInfoEntries;
			}
		}
	}
#endif // __STATS
	else if (pModule == &g_StreamedScripts)
	{
		strLocalIndex scriptObjectIdx = pModule->GetObjectIndex(file.index);
		scrProgram * prog = g_StreamedScripts.GetSafeFromIndex(scriptObjectIdx);
		if (prog)
		{
			int used, total;
			scrThread::GetStackSummary(used,total);
			formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries++], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
				"%dk total stacks in use, %dk total allocated",used >> 10,total >> 10);
			formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries++], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
				"Code %0.3fk + Strings %0.3fk", prog->GetOpcodeSize() / 1024.0f, prog->GetStringHeapSize() / 1024.0f);
			atArray<scrThread*> &threads = scrThread::GetScriptThreads();
			int copiesRunning = 0;
			for (int i=0; i<threads.GetCount(); i++)
			{
				scrThread *t = threads[i];
				if (t && scrProgram::GetProgram(t->GetProgram()) == prog)
				{
					++copiesRunning;
					formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries++], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
						"Copy %d: %.3fk used / %.3fk total stack", copiesRunning, t->GetStackSize() / 1024.0f, t->GetTotalStackSize() / 1024.0f);
				}
			}
			formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries++], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"%d cop%s of this thread currently running", copiesRunning, copiesRunning==1?"y":"ies");
			if (copiesRunning)
				formatf(updateBuffer.m_FileInfoBuffer[updateBuffer.m_NumFileInfoEntries++], updateBuffer.MAX_FILE_INFO_ENTRY_LENGTH,
					"%.3fk of each thread stack is statics", prog->GetStaticSize() * sizeof(scrValue) / 1024.0f);
		}
	}

}

// Recursive for to be able to handle composite types
u32	CStreamGraph::GetBoundSize(phBound *pBound)
{
u32 thisSize = 0;

	// Calculate the size
	switch(pBound->GetType())
	{
		case phBound::SPHERE:
			{
				thisSize = sizeof(rage::phBoundSphere);
			}
			break;

		case phBound::CAPSULE:
			{
				thisSize = sizeof(rage::phBoundCapsule);
			}
			break;

#if USE_TAPERED_CAPSULE
		case phBound::TAPERED_CAPSULE:
			{
				thisSize = sizeof(rage::phBoundTaperedCapsule);
			}
			break;
#endif

		case phBound::BOX:
			{
				thisSize = sizeof(rage::phBoundBox);
			}
			break;

#if USE_GEOMETRY_CURVED
		case phBound::GEOMETRY_CURVED:
			{
				thisSize = sizeof(rage::phBoundCurvedGeometry);
			}
			break;
#endif

#if USE_GRIDS
		case phBound::GRID:
			{
				thisSize = sizeof(rage::phBoundGrid);
			}
			break;
#endif

#if USE_RIBBONS
		case phBound::RIBBON:
			{
				thisSize = sizeof(rage::phBoundRibbon);
			}
			break;
#endif

#if USE_SURFACES
		case phBound::SURFACE:
			{
				thisSize = sizeof(rage::phBoundSurface);
			}
			break;
#endif

		case phBound::TRIANGLE:
			{
				thisSize = sizeof(rage::TriangleShape);
			}
			break;

		case phBound::DISC:
			{
				thisSize = sizeof(rage::phBoundDisc);
			}
			break;

		case phBound::CYLINDER:
			{
				thisSize = sizeof(rage::phBoundCylinder);
			}
			break;

		// These require extra processing
		case phBound::GEOMETRY:
			{
				rage::phBoundGeometry * pGeom = (rage::phBoundGeometry*)pBound;

				int iNumPolys = pGeom->GetNumPolygons();
				int iNumVerts = pGeom->GetNumVertices();

				int vertSize = 3*sizeof(CompressedVertexType);
				int polySize = sizeof(phPolygon);

				thisSize = sizeof(rage::phBoundGeometry);
				thisSize += iNumVerts * vertSize;								// Verts
				thisSize += iNumPolys * polySize;								// Polygons
				thisSize += pGeom->m_NumMaterials * sizeof(phMaterialMgr::Id);	// Materials
				thisSize += iNumPolys * sizeof(u8);								// index numbers into this bound's list of material ids, one for each polygon
																				// No access to SecondSurfaceVertexDisplacement
																				// No access to OctantVertmap

			}
			break;

		case phBound::COMPOSITE:
			{
				rage::phBoundComposite * pComposite = (rage::phBoundComposite*)pBound;

				thisSize = sizeof(rage::phBoundComposite);

				// Some other stuff to add later:-
				//(between 17 and 35, depending on whether there are composite flags and last matrices) * number of sub bounds

				int iNumBounds = pComposite->GetNumBounds();
				for(int b=0; b<iNumBounds; b++)
				{
					phBound * pCompositeBound = pComposite->GetBound(b);
					thisSize += GetBoundSize(pCompositeBound);
				}
			}
			break;

		case phBound::BVH:
			{
				rage::phBoundBVH * pBVH = (rage::phBoundBVH*)pBound;

				thisSize += sizeof(rage::phBoundBVH);

				int iNumPolys = pBVH->GetNumPolygons();
				int polySize = sizeof(phPolygon);
				thisSize += iNumPolys * polySize;								// Polys?

				int vertSize = 3*sizeof(CompressedVertexType);
				int iNumVerts = pBVH->GetNumVertices();
				thisSize += iNumVerts * vertSize;								// Verts

				int numNodes = pBVH->GetBVH()->GetNumNodes();
				thisSize += numNodes * sizeof(phOptimizedBvhNode);

				/*
				int curNodeIndex = 0;
				while(curNodeIndex < numNodes)
				{
					const phOptimizedBvhNode &curNode = pBVH->GetBVH()->GetNode(curNodeIndex);
					thisSize += sizeof(phOptimizedBvhNode);
					// Do we need to include the polygon indices array?.. No, just one start index.
					++curNodeIndex;
				}
				*/

			}
			break;

	}
	return thisSize;
}



void CStreamGraph::UpdateCategoryStats(s32 categoryId, CompareFunc compareFunc)
{
	USE_DEBUG_MEMORY();

	CStreamGraphBuffer &updateBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	bool isSelectedCategory = categoryId == updateBuffer.m_CurrentlySelectedCategoryIdx;
	if (isSelectedCategory)
	{
		// update the selected file if there's been a keypress
		if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG, "move forward in file list") )
		{
			++updateBuffer.m_CurrentlySelectedFileIdx;
		}

		if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG, "move back in file list") )
		{
			--updateBuffer.m_CurrentlySelectedFileIdx;
		}

		// stop selected index dropping below zero, upper clamp will be applied later when the
		// total number of filtered files is known
		updateBuffer.m_CurrentlySelectedFileIdx = Max(updateBuffer.m_CurrentlySelectedFileIdx, 0);
	}

	int totalCategoryFileCount = 0;
	int totalPhysicalSize = 0;
	int totalVirtualSize = 0;
	
	// keep track of the last known filtered file in case the selected file index is less than the totalCategoryFileCount
	const char * lastFileName = "";
	strIndex lastIndex;
	u16 lastFileFlags = 0;
	int lastFileModule = -1;
	int lastFileObjIdx = -1;
	int lastFilePhysicalSize = 0;
	int lastFileVirtualSize = 0;

	// reset selected file info
	if (isSelectedCategory)
	{
		updateBuffer.m_SelectedFileData.m_Name[0] = '\0';
		updateBuffer.m_SelectedFileData.m_VirtualSize = 0;
		updateBuffer.m_SelectedFileData.m_PhysicalSize = 0;
		updateBuffer.m_SelectedFileData.m_Flags = 0;
	}

	size_t totalCostPhys = 0;
	size_t totalCostVirt = 0;

	for(int moduleId = 0; moduleId < GetNumTrackedModules(); ++moduleId)
	{
		CStreamGraphModuleBuffer &moduleBuffer = updateBuffer.m_Modules[ moduleId ];
		moduleBuffer.m_CostPhys[categoryId] = 0;
		moduleBuffer.m_CostVirt[categoryId] = 0;

		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);

		int filteredCount = 0;
		int unfilteredCount = pModule->GetCount();
		GraphEntry * sortedObjOrder = rage_new GraphEntry[unfilteredCount];

		for(s32 objIdx = 0; objIdx < unfilteredCount; ++objIdx)
		{
			strStreamingInfo * info = pModule->GetStreamingInfo(strLocalIndex(objIdx));
			strIndex index = pModule->GetStreamingIndex(strLocalIndex(objIdx));

			if (ObjectPassesCategoryFilter(pModule, categoryId, strLocalIndex(objIdx)))
			{
				GraphEntry & entry = sortedObjOrder[filteredCount];
				entry.index = index;
				entry.module = moduleId;
				GetLoadedSizes(pModule, strLocalIndex(objIdx), entry.virtualSize, entry.physicalSize);
				++filteredCount;
			}

			switch (info->GetStatus())
			{
			case STRINFO_LOADING:
				totalCostVirt += info->ComputeOccupiedVirtualSize(index, true);
				totalCostPhys += info->ComputePhysicalSize(index, true);
				break;

			case STRINFO_LOADED:
				totalCostVirt += pModule->GetVirtualMemoryOfLoadedObj(strLocalIndex(objIdx), false);
				totalCostPhys += pModule->GetPhysicalMemoryOfLoadedObj(strLocalIndex(objIdx), false);
				break;
				
			default:
				break;
			}
		}

		updateBuffer.m_UsedByModulesVirt = totalCostVirt;
		updateBuffer.m_UsedByModulesPhys = totalCostPhys;

		bool blockContainsSelectedFile =
			isSelectedCategory &&
			(totalCategoryFileCount <= updateBuffer.m_CurrentlySelectedFileIdx) &&
			(updateBuffer.m_CurrentlySelectedFileIdx < (totalCategoryFileCount + filteredCount));

		// if sorting everything is too slow, sorting just the block containing the selected 
		// file is an option but it would mean the file list behaves a bit strangely
		if(compareFunc)
		{
			qsort(sortedObjOrder, filteredCount, sizeof(GraphEntry), compareFunc);
		}

		// update the memory count
		for(s32 objIdx = 0; objIdx < filteredCount; ++objIdx)
		{
			GraphEntry & entry = sortedObjOrder[objIdx];

			if (blockContainsSelectedFile && (totalCategoryFileCount + objIdx) == updateBuffer.m_CurrentlySelectedFileIdx)
			{
				strIndex streamingIndex = entry.index;
				const char *fileName = strStreamingEngine::GetObjectName(streamingIndex);
				SetSelectedFileName(updateBuffer, fileName);
				updateBuffer.m_SelectedFileData.m_Flags = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex)->GetFlags();
				updateBuffer.m_SelectedFileData.m_AccumulatedPhysMemoryBeforeSelectedFile = totalPhysicalSize;
				updateBuffer.m_SelectedFileData.m_AccumulatedVirtMemoryBeforeSelectedFile = totalVirtualSize;
				updateBuffer.m_SelectedFileData.m_PhysicalSize = entry.physicalSize;
				updateBuffer.m_SelectedFileData.m_VirtualSize = entry.virtualSize;
				if (m_ShowExtendedFileInfo)
				{
					updateBuffer.m_SelectedFileData.CollectDependents(streamingIndex);
				}

				updateBuffer.m_CurrentlySelectedModuleIdx = entry.module;
				updateBuffer.m_CurrentlySelectedFileObjIdx = pModule->GetObjectIndex(streamingIndex).Get();
			}				

			totalPhysicalSize += entry.physicalSize;
			totalVirtualSize += entry.virtualSize;

			moduleBuffer.m_CostPhys[categoryId] += entry.physicalSize;
			moduleBuffer.m_CostVirt[categoryId] += entry.virtualSize;
		}

		totalCategoryFileCount += filteredCount;
		
		// update the stats for the last good file
		if(filteredCount > 0)
		{
			GraphEntry & lastFileGraphEntry = sortedObjOrder[filteredCount - 1];
			strIndex index = lastFileGraphEntry.index;
			strStreamingInfo *info = strStreamingEngine::GetInfo().GetStreamingInfo(index);
			lastFileName = strStreamingEngine::GetObjectName(index);
			lastIndex = index;
			lastFileFlags = info->GetFlags();
			lastFileModule = moduleId;
			lastFileObjIdx = pModule->GetObjectIndex(index).Get();
			lastFilePhysicalSize = lastFileGraphEntry.physicalSize;
			lastFileVirtualSize = lastFileGraphEntry.virtualSize;
		}

		if(isSelectedCategory)
		{
			UpdateFileList(sortedObjOrder, filteredCount);
		}

		delete[] sortedObjOrder;
	}

	// if the selected file is outside the total number of filtered files across all modules clamp it to the last file
	if(isSelectedCategory && totalCategoryFileCount <= updateBuffer.m_CurrentlySelectedFileIdx )
	{
		updateBuffer.m_CurrentlySelectedFileIdx = totalCategoryFileCount - 1;
		updateBuffer.m_CurrentlySelectedModuleIdx = lastFileModule;
		updateBuffer.m_CurrentlySelectedFileObjIdx = lastFileObjIdx;

		SetSelectedFileName(updateBuffer, lastFileName);
		updateBuffer.m_SelectedFileData.m_Flags = lastFileFlags;
		updateBuffer.m_SelectedFileData.m_AccumulatedPhysMemoryBeforeSelectedFile = totalPhysicalSize - lastFilePhysicalSize;
		updateBuffer.m_SelectedFileData.m_AccumulatedVirtMemoryBeforeSelectedFile = totalVirtualSize - lastFileVirtualSize;
		updateBuffer.m_SelectedFileData.m_PhysicalSize = lastFilePhysicalSize;
		updateBuffer.m_SelectedFileData.m_VirtualSize = lastFileVirtualSize;
		if (m_ShowExtendedFileInfo)
		{
			updateBuffer.m_SelectedFileData.CollectDependents(lastIndex);
		}
	}
}

void CStreamGraph::UpdateModuleStats( s32 moduleId, CompareFunc compareFunc )
{
	USE_DEBUG_MEMORY();

	CStreamGraphModuleBuffer &moduleBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_Modules[ moduleId ];

	memset( moduleBuffer.m_CostPhys, 0, sizeof( moduleBuffer.m_CostPhys ) );
	memset( moduleBuffer.m_CostVirt, 0, sizeof( moduleBuffer.m_CostVirt ) );

	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
	CStreamGraphBuffer &updateBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ];

	int filteredCount = 0;
	GraphEntry * sortedObjOrder = rage_new GraphEntry[ pModule->GetCount() ];

	size_t totalVirtUsed = 0;
	size_t totalPhysUsed = 0;

	size_t filteredTotalVirtUsed = 0;
	size_t filteredTotalPhysUsed = 0;

	for(s32 objIdx = 0; objIdx < pModule->GetCount(); ++objIdx)
	{
		strStreamingInfo * info = pModule->GetStreamingInfo(strLocalIndex(objIdx));
		strIndex index = pModule->GetStreamingIndex(strLocalIndex(objIdx));

		if ( ObjectPassesFilter( pModule, strLocalIndex(objIdx) ) )
		{
			char lowerName[128];
			strcpy(lowerName, pModule->GetName(strLocalIndex(objIdx)));
			strlwr(lowerName);
			if (strlen(m_AssetFilterStr) > 0)
			{
				strlwr(m_AssetFilterStr);

				// We skip if the asset name doesn't contain the filter
				if (strstr(lowerName, m_AssetFilterStr) == NULL)
					continue;
			}
			GraphEntry & entry = sortedObjOrder[filteredCount];
			entry.index = pModule->GetStreamingIndex(strLocalIndex(objIdx));
			entry.category = GetStreamingCategory(info);
			entry.virtualSize = (int)pModule->GetVirtualMemoryOfLoadedObj(strLocalIndex(objIdx), m_ShowTemporaryMemory);
			entry.physicalSize = (int)pModule->GetPhysicalMemoryOfLoadedObj(strLocalIndex(objIdx), m_ShowTemporaryMemory);
			strcpy(entry.name, lowerName);

			filteredTotalVirtUsed += (size_t) entry.virtualSize;
			filteredTotalPhysUsed += (size_t) entry.physicalSize;
			++filteredCount;
		}

		switch (info->GetStatus())
		{
		case STRINFO_LOADING:
			totalVirtUsed += info->ComputeOccupiedVirtualSize(index, true);
			totalPhysUsed += info->ComputePhysicalSize(index, true);
			break;

		case STRINFO_LOADED:
			totalVirtUsed += pModule->GetVirtualMemoryOfLoadedObj(strLocalIndex(objIdx), false);
			totalPhysUsed += pModule->GetPhysicalMemoryOfLoadedObj(strLocalIndex(objIdx), false);
			break;

		default:
			break;
		}
	}

	moduleBuffer.m_TotalCostVirt = totalVirtUsed;
	moduleBuffer.m_TotalCostPhys = totalPhysUsed;

	moduleBuffer.m_FilteredTotalCostVirt = filteredTotalVirtUsed;
	moduleBuffer.m_FilteredTotalCostPhys = filteredTotalPhysUsed;

	if ( compareFunc != NULL )
	{
		qsort(sortedObjOrder, filteredCount, sizeof(GraphEntry), compareFunc);
	}

	// if this is the currently selected module there's more to do
	if(moduleId == updateBuffer.m_CurrentlySelectedModuleIdx && filteredCount)
	{
		// update the selected file if there's been a keypress
		if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG, "move forward in file list") )
		{
			++updateBuffer.m_CurrentlySelectedFileIdx;
		}

		if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG, "move back in file list") )
		{
			--updateBuffer.m_CurrentlySelectedFileIdx;
		}

		// clamp the index of the selected file to total number of files after filtering
		updateBuffer.m_CurrentlySelectedFileIdx = Clamp( updateBuffer.m_CurrentlySelectedFileIdx, 0, Max(filteredCount - 1,0) );

		// sync the selected object index with the selected sorted index
		updateBuffer.m_CurrentlySelectedFileObjIdx = pModule->GetObjectIndex(sortedObjOrder[updateBuffer.m_CurrentlySelectedFileIdx].index).Get();
		updateBuffer.m_CurrentlySelectedCategoryIdx = sortedObjOrder[updateBuffer.m_CurrentlySelectedFileIdx].category;

		// update the selected file data
		if ( filteredCount == 0 )
		{
			updateBuffer.m_SelectedFileData.m_Name[0] = '\0';
			updateBuffer.m_SelectedFileData.m_VirtualSize = 0;
			updateBuffer.m_SelectedFileData.m_PhysicalSize = 0;
			updateBuffer.m_SelectedFileData.m_Flags = 0;
		}
		else
		{
			strLocalIndex selectedFileModuleIdx = pModule->GetObjectIndex( sortedObjOrder[updateBuffer.m_CurrentlySelectedFileIdx].index );
			strStreamingInfo* info = pModule->GetStreamingInfo( selectedFileModuleIdx );
			strIndex index = pModule->GetStreamingIndex( selectedFileModuleIdx );
			const char *fileName   = strStreamingEngine::GetObjectName( index );
			SetSelectedFileName(updateBuffer, fileName);
			int v,p;
			GetSizesSafe( index, *info, v, p, false );
			updateBuffer.m_SelectedFileData.m_VirtualSize	= v;
			updateBuffer.m_SelectedFileData.m_PhysicalSize	= p;
			updateBuffer.m_SelectedFileData.m_Flags			= info->GetFlags();

			if (m_ShowExtendedFileInfo)
			{
				updateBuffer.m_SelectedFileData.CollectDependents(pModule->GetStreamingIndex( selectedFileModuleIdx ));
			}
		}

		UpdateFileList(sortedObjOrder, filteredCount);
	}

	// add up memory
	u32 totalPhysMemory = 0;
	u32 totalVirtMemory = 0;
	for(s32 objIdx = 0; objIdx < filteredCount; ++objIdx)
	{
		GraphEntry & entry = sortedObjOrder[objIdx];

		if ( moduleId == m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_CurrentlySelectedModuleIdx && objIdx == m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_CurrentlySelectedFileIdx )
		{
			m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_SelectedFileData.m_AccumulatedPhysMemoryBeforeSelectedFile = totalPhysMemory;
			m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ].m_SelectedFileData.m_AccumulatedVirtMemoryBeforeSelectedFile = totalVirtMemory;
		}				

		totalVirtMemory += entry.virtualSize;
		totalPhysMemory += entry.physicalSize;

		moduleBuffer.m_CostPhys[entry.category] += entry.physicalSize;
		moduleBuffer.m_CostVirt[entry.category] += entry.virtualSize;
	}

	delete [] sortedObjOrder;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void	CStreamGraph::AllocateMemoryForLODDisplay()
{
	// Debug Heap
	sysMemAutoUseDebugMemory debug;

	// Create some storage for lod level counts for the buffers we're interested in
	m_pTxdLodCounts = rage_new int[g_TxdStore.GetMaxSize() * LODTYPES_DEPTH_TOTAL];
	m_pDwdLodCounts = rage_new int[g_DwdStore.GetMaxSize() * LODTYPES_DEPTH_TOTAL];
	m_pDrawableLodCounts = rage_new int[g_DrawableStore.GetMaxSize() * LODTYPES_DEPTH_TOTAL];
}

void	CStreamGraph::FreeMemoryForLODDisplay()
{
	// Debug Heap
	sysMemAutoUseDebugMemory debug;

	delete [] m_pTxdLodCounts;
	m_pTxdLodCounts = NULL;

	delete [] m_pDwdLodCounts;
	m_pDwdLodCounts = NULL;

	delete [] m_pDrawableLodCounts;
	m_pDrawableLodCounts = NULL;
}

void	CStreamGraph::UpdateLodStats()
{
	// Clear out buffer storage
	ResetMemoryByLODBuffer(&m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE]);

	// If the buffers aren't allocated, exit
	if(m_pTxdLodCounts == NULL)
		return;

	memset(m_pTxdLodCounts,0, sizeof(int) * g_TxdStore.GetMaxSize() * LODTYPES_DEPTH_TOTAL );
	memset(m_pDwdLodCounts,0, sizeof(int) * g_DwdStore.GetMaxSize() * LODTYPES_DEPTH_TOTAL);
	memset(m_pDrawableLodCounts,0, sizeof(int) * g_DrawableStore.GetMaxSize() * LODTYPES_DEPTH_TOTAL);

	CDummyObject::Pool* pDPool = CDummyObject::GetPool();
	for(s32 i=0;i<pDPool->GetSize();i++)
	{
		CDummyObject* pObject = pDPool->GetSlot(i);
		CEntity *pEntity = (CEntity*)pObject;
		if(pEntity && CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
		{
			BuildTxdLODCounts(pEntity, m_pTxdLodCounts);
			BuildDwdLODCounts(pEntity, m_pDwdLodCounts);
			BuildDrawableLODCounts(pEntity, m_pDrawableLodCounts);
		}
	}

	// BUILDINGS
	CBuilding::Pool* pBPool = CBuilding::GetPool();
	for(s32 i=0;i<pBPool->GetSize();i++)
	{
		CBuilding* pObject = pBPool->GetSlot(i);
		CEntity *pEntity = (CEntity*)pObject;
		if(pEntity && pEntity->IsArchetypeSet() && CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
		{
			BuildTxdLODCounts(pEntity, m_pTxdLodCounts);
			BuildDwdLODCounts(pEntity, m_pDwdLodCounts);
			BuildDrawableLODCounts(pEntity, m_pDrawableLodCounts);
		}
	}

	BuildTxdLODData(m_pTxdLodCounts);
	BuildDwdLODData(m_pDwdLodCounts);
	BuildDrawableLODData(m_pDrawableLodCounts);

}

int CStreamGraph::GetLodFromCountArray(int idx, int *pLodCounts)
{
	int	lodLevel = LODTYPES_DEPTH_TOTAL;	// UNKNOWN!
	int	maxCount = 0;
	int *pBaseCountsForThisAsset = &pLodCounts[idx*LODTYPES_DEPTH_TOTAL];
	for(int j=0;j<LODTYPES_DEPTH_TOTAL;j++)
	{
		if( pBaseCountsForThisAsset[j] > maxCount )
		{
			maxCount = pBaseCountsForThisAsset[j];
			lodLevel = j;
		}
	}
	return lodLevel;
}

void	CStreamGraph::BuildTxdLODCounts(CEntity *pEntity, int *pTxdLodCounts)
{
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	u32	lodType = pEntity->GetLodData().GetLodType();

	if(pModelInfo->GetHasLoaded())
	{
		// Get the textures used for this LOD?
		s32 txdDictIndex = pModelInfo->GetAssetParentTxdIndex();
		if( txdDictIndex != -1 )
		{
			pTxdLodCounts[ (txdDictIndex*LODTYPES_DEPTH_TOTAL) + lodType ]++;

			strLocalIndex parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(txdDictIndex));
			while (parentTxdIndex!=-1)
			{
				pTxdLodCounts[ (parentTxdIndex.Get()*LODTYPES_DEPTH_TOTAL) + lodType ]++;
				parentTxdIndex = g_TxdStore.GetParentTxdSlot(parentTxdIndex);
			}
		}
	}
}

void	CStreamGraph::BuildTxdLODData(int *pLodCounts)
{
	MemoryByLODBuffer *pLODBuffer = &m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];

	for(int i=0; i<g_TxdStore.GetSize(); i++)
	{
		if(g_TxdStore.GetPtr(strLocalIndex(i)) != NULL)
		{
			int	lodLevel = GetLodFromCountArray(i,pLodCounts);

			// update mem counts
			strIndex streamingIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(i));
			strStreamingInfo *info = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);

			int	v,m;
			GetSizesSafe(streamingIndex, *info, v, m, false);
			pLODBuffer->m_TxdStoreMemoryByLod[lodLevel].phys += m;
			pLODBuffer->m_TxdStoreMemoryByLod[lodLevel].virt += v;
		}
	}
}

void	CStreamGraph::BuildDwdLODCounts(CEntity *pEntity, int *pDwdLodCounts)
{
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	u32	lodType = pEntity->GetLodData().GetLodType();

	if(pModelInfo->GetHasLoaded())
	{
		u32 dwdDictIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLEDICTIONARY) ? pModelInfo->GetDrawDictIndex() : INVALID_DRAWDICT_IDX;
		if (dwdDictIndex!=INVALID_DRAWDICT_IDX)
		{
			pDwdLodCounts[ (dwdDictIndex*LODTYPES_DEPTH_TOTAL) + lodType ]++;
		}
	}
}

void	CStreamGraph::BuildDwdLODData(int *pLodCounts)
{
	MemoryByLODBuffer *pLODBuffer = &m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];

	for(int i=0; i<g_DwdStore.GetSize(); i++)
	{
		if(g_DwdStore.GetPtr(strLocalIndex(i)) != NULL)
		{
			int	lodLevel = GetLodFromCountArray(i,pLodCounts);

			// update mem counts
			strIndex streamingIndex = g_DwdStore.GetStreamingIndex(strLocalIndex(i));
			strStreamingInfo *info = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);

			int	v,m;
			GetSizesSafe(streamingIndex, *info, v, m, false);
			pLODBuffer->m_DwdStoreMemoryByLod[lodLevel].phys += m;
			pLODBuffer->m_DwdStoreMemoryByLod[lodLevel].virt += v;
		}
	}
}


void	CStreamGraph::BuildDrawableLODCounts(CEntity *pEntity, int *pDrawableLodCounts)
{
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	u32	lodType = pEntity->GetLodData().GetLodType();

	if(pModelInfo->GetHasLoaded())
	{
		u32 drawableIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLE) ? pModelInfo->GetDrawableIndex() : INVALID_DRAWABLE_IDX;
		if (drawableIndex!=INVALID_DRAWABLE_IDX)
		{
			pDrawableLodCounts[ (drawableIndex*LODTYPES_DEPTH_TOTAL) + lodType ]++;
		}
	}
}

void	CStreamGraph::BuildDrawableLODData(int *pLodCounts)
{
	MemoryByLODBuffer *pLODBuffer = &m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];

	for(int i=0; i<g_DrawableStore.GetSize(); i++)
	{
		if(g_DrawableStore.GetPtr(strLocalIndex(i)) != NULL)
		{
			int	lodLevel = GetLodFromCountArray(i,pLodCounts);

			// update mem counts
			strIndex streamingIndex = g_DrawableStore.GetStreamingIndex(strLocalIndex(i));
			strStreamingInfo *info = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);

			int	v,m;
			GetSizesSafe(streamingIndex, *info, v, m, false);
			pLODBuffer->m_DrawablesStoreMemoryByLod[lodLevel].phys += m;
			pLODBuffer->m_DrawablesStoreMemoryByLod[lodLevel].virt += v;
		}
	}
}


void	CStreamGraph::DrawLODStats()
{
	DrawTitle();
	DrawBackground();
	DrawLegend(m_vPos.x + GetGraphWidth() + 20, m_vPos.y + 5);

	// Draw the memory bars
	s32 xAxisCount = sizeof(sAssetLODLegend)/sizeof(sAssetLODLegend[0]);
	float barOffset = 0.0f;
	for ( s32 yAxisIdx = 0; yAxisIdx < 3; yAxisIdx++ )
	{
		barOffset += DrawLODMemoryBar(yAxisIdx, xAxisCount, m_vPos.x, m_vPos.y + barOffset);
	}
}

float CStreamGraph::DrawLODMemoryBar(int memoryBarIndex, int numDivisions, float x, float y)
{
	char achMsg[200];
	float heightOfBarDrawn = 0.0f;

	MemoryByLODBuffer *pBuffer = &m_MemoryByLOD[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER];

	s32 moduleId = -1;
	MemoryByLOD *pMemoryByLODBuffer = NULL;
	switch(memoryBarIndex)
	{
		case 0:
			pMemoryByLODBuffer = pBuffer->m_TxdStoreMemoryByLod;
			moduleId = g_TxdStore.GetStreamingModuleId();
			break;
		case 1:
			pMemoryByLODBuffer = pBuffer->m_DwdStoreMemoryByLod;
			moduleId = g_DwdStore.GetStreamingModuleId();
			break;
		case 2:
			pMemoryByLODBuffer = pBuffer->m_DrawablesStoreMemoryByLod;
			moduleId = g_DrawableStore.GetStreamingModuleId();
			break;
	}

	bool bDoneBackground = false;
	float curX = float(x + STREAMGRAPH_X_BORDER); 

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	u32 totalBarVirtMemory = 0;
	u32 totalBarPhysMemory = 0;

	for ( s32 xAxisIdx = 0; xAxisIdx < numDivisions; xAxisIdx++ )
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);

		u32 memoryInBar = 0;
		if ( m_ShowPhysicalMemory )
		{
			totalBarPhysMemory		+= pMemoryByLODBuffer[ xAxisIdx ].phys;
			memoryInBar				+= pMemoryByLODBuffer[ xAxisIdx ].phys;
		}
		if ( m_ShowVirtualMemory )
		{
			totalBarVirtMemory		+= pMemoryByLODBuffer[ xAxisIdx ].virt;
			memoryInBar				+= pMemoryByLODBuffer[ xAxisIdx ].virt;
		}

		if ( memoryInBar == 0 )
		{
			continue;
		}

		if ( !bDoneBackground )
		{
			bDoneBackground = true;

			DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), pModule->GetModuleName());

			DrawMemoryMarkers(x, y);
		}

		float barWidth = ( (float)(memoryInBar) / (float)( m_nMaxMemMB * 1024 * 1024 ) ) * GetGraphWidth();

		Color32 barColour1 = Color32(0,255,0,255);
		Color32 barColour2 = GetLegendColour( xAxisIdx ); 
		if( ( curX - x ) + barWidth > GetGraphWidth() )
		{
			barWidth = GetGraphWidth() - ( curX - x );
			barColour1.Set( 255, 0, 0, 255 ); 
			barColour2.Set( 255, 0, 0, 255 );
		}

		fwRect barRect1((float) curX,(float) y + STREAMGRAPH_BAR_SPACING(),								  (float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f) );
		fwRect barRect2((float) curX,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f),(float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT());

		CSprite2d::DrawRectSlow(barRect1, barColour1);
		CSprite2d::DrawRectSlow(barRect2, barColour2);

		curX += barWidth;
	}

	if ( totalBarVirtMemory+totalBarPhysMemory > 0 ) 
	{
		heightOfBarDrawn = STREAMGRAPH_BAR_SPACING();

#if __PS3
		sprintf( achMsg, "%.3f MB (M), %.3f MB (V)", (float)totalBarVirtMemory / (1024.0f*1024.0f),  (float)totalBarPhysMemory / (1024.0f*1024.0f) );
#else
		sprintf( achMsg, "%.3f MB", (float)(totalBarVirtMemory+totalBarPhysMemory) / (1024.0f*1024.0f) );
#endif

		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x + 150, y), achMsg);
	}

	return heightOfBarDrawn;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////

bool CStreamGraph::ObjectPassesFilter( strStreamingModule* pModule, strLocalIndex objIdx )
{
	strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
	strIndex streamingIndex = pModule->GetStreamingIndex(objIdx);

	if ( strInfoManager.IsObjectInImage(streamingIndex) )
	{
		bool bLoaded  = strInfoManager.HasObjectLoaded(streamingIndex);
		bool bRequested = strInfoManager.IsObjectRequested(streamingIndex) || strInfoManager.IsObjectLoading(streamingIndex);

		if( bLoaded || ( bRequested && m_IncludeRequestedObjects ) )
		{
			strStreamingInfo* info = strInfoManager.GetStreamingInfo(streamingIndex);

			bool bReferenced = ( pModule->GetNumRefs( objIdx ) > 0 || info->GetDependentCount() > 0 );
			bool bRequired = pModule->IsObjectRequired( objIdx );
			bool bCached = bLoaded && (!bReferenced && !bRequired); 

			bool includeByState = ( (bReferenced && m_IncludeReferencedObjects) || 
				(bRequired && m_IncludeDontDeleteObjects) || 
				(bCached && m_IncludeCachedObjects) ||
				(bRequested && m_IncludeRequestedObjects) );

			if (includeByState)
			{
				if (s_DumpExcludeSceneAssetsInGraph)
				{
					if (s_SceneMap.Access(streamingIndex) != NULL)
					{
						return false;
					}
				}

				if ( m_ExcludeCutsceneRequestedObjects)
				{
					if ( info->GetFlags() & STRFLAG_CUTSCENE_REQUIRED )
					{
						return false;
					}

					if (m_CutsceneRequired.Access(streamingIndex) != NULL)
					{
						return false;
					}
				}

				if ( m_OnlyIncludeCutsceneRequestedObjects && !( info->GetFlags() & STRFLAG_CUTSCENE_REQUIRED ) )
				{
					if (m_CutsceneRequired.Access(streamingIndex) == NULL)
						return true;

					return false;
				}
				else if ( m_OnlyIncludeScriptRequestedObjects )
				{
					if( m_ScriptDependentOnList.Find(streamingIndex) != -1 )
					{
						return true;
					}
					return false;
				}
				else
				{
					return true;
				}
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool CStreamGraph::ObjectPassesCategoryFilter( strStreamingModule* pModule, s32 categoryIdx, strLocalIndex objIdx )
{
	strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
	strIndex streamingIndex = pModule->GetStreamingIndex(objIdx);

	if ( strInfoManager.IsObjectInImage(streamingIndex) )
	{
		strStreamingInfo* info = strInfoManager.GetStreamingInfo(streamingIndex);

		if(GetStreamingCategory(info) != (u32)categoryIdx)
		{
			return false;
		}

		return ObjectPassesFilter(pModule, objIdx);
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerSearch.h" // for CTxdRef, GetAssociatedTxds_ModelInfo

extern	void GetTxdRefsUsedByEntity(atMap<CTxdRef, u32>& txdRefMap, const CEntity* pEntity);

bool	CStreamGraph::IsSelectedUsedByCB(CEntity *pEntity)
{
	// exclude anything selected in an extension.
	if(m_ShowMemoryTracker && !m_pStreamGraphExtManager->m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] )
	{
		CStreamGraphBuffer &updateBuffer = m_statBuffers[ SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ];
		strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
		strStreamingModuleMgr& moduleMgr = strInfoManager.GetModuleMgr();
		strStreamingModule* pModule = moduleMgr.GetModule(updateBuffer.m_CurrentlySelectedModuleIdx);

		if(pModule == NULL)
			return false;

		if(pModule == &g_TxdStore)
		{
			strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(updateBuffer.m_CurrentlySelectedFileObjIdx));

			atMap<CTxdRef, u32> txdRefMap;
			GetTxdRefsUsedByEntity(txdRefMap, pEntity);

			for (atMap<CTxdRef, u32>::Iterator iter = txdRefMap.CreateIterator(); !iter.AtEnd(); iter.Next())
			{
				const CTxdRef& ref = iter.GetKey();

				strIndex strIDX = pModule->GetStreamingIndex(ref.GetAssetIndex());

				if( strIDX == streamingIndex )
				{
					return true;
				}
			}
		}
		else
		{
			strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(updateBuffer.m_CurrentlySelectedFileObjIdx));

			strIndex deps[STREAMING_MAX_DEPENDENCIES];
			strStreamingModule *pModelInfoStreamingModule = fwArchetypeManager::GetStreamingModule();
			int depsCount = pModelInfoStreamingModule->GetDependencies(strLocalIndex(pEntity->GetModelIndex()), deps, STREAMING_MAX_DEPENDENCIES );
			for(int i=0;i<depsCount;i++)
			{
				if( deps[i] == streamingIndex)
				{
					return true;
				}
			}
		}
	}

	return false;
}

#endif //__BANK
