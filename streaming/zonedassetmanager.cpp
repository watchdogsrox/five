//
// streaming/zonedassetmanager.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "zonedassetmanager.h"

#include "zonedassets.h"

#include "atl/string.h"
#include "control/gamelogic.h"
#include "entity/archetypemanager.h"
#include "file/asset.h"
#include "fwsys/gameskeleton.h"
#include "modelinfo/modelinfo.h"
#include "streaming/streaming_channel.h"
#include "streaming/streamingengine.h"


#if __BANK
#include "camera/viewports/viewportmanager.h"
#include "grcore/debugdraw.h"
#include "grcore/viewport.h"
#include "spatialdata/transposedplaneset.h"
#include "streaming/streamingdebug.h"
#endif	//__BANK


using namespace rage;

#define ZONE_RECOMPUTE_DISTANCE_SQUARE	(100.0f)

PARAM(nozonedassets, "Disable zoned / mem resident props by region");


Vec3V								CZonedAssetManager::sm_LastFocus;
atMap<atHashString, strRequest>		CZonedAssetManager::sm_Requests; 
atVector<CZonedAssets*>				CZonedAssetManager::sm_Zones;

#if __BANK
static bool s_bShowZonedAssets = false;
static s32 s_DrawBoxOption = 0;
static bool s_bShowSelectedZoneDetails = false;
static s32 s_SelectedZone = 0;
static bool s_bColorBoxesByCost = false;
static int s_fColorViewMin = 0;
static int s_fColorViewMax = 10000;

#endif // __BANK
static bool s_bEnableZonedAssets = true;


class CZonedAssetFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		if(strstr(file.m_filename, ".meta") != nullptr)
		{
			CZonedAssetManager::LoadFile(file.m_filename);
		}
		else
		{
			CZonedAssetManager::EnumAndProcessFolder(file.m_filename, CZonedAssetManager::LoadFile);
		}

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file) 
	{
		if(strstr(file.m_filename, ".meta") != nullptr)
		{
			CZonedAssetManager::UnloadFile(file.m_filename);
		}
		else
		{
			CZonedAssetManager::EnumAndProcessFolder(file.m_filename, CZonedAssetManager::UnloadFile);
		}
	}

} g_ZonedAssetFileMounter;

//////////////////////////////////////////////////////////////////////////
static void FindFileCallback(const fiFindData &data, void *user)
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);

	atVector<atString> *files = reinterpret_cast<atVector<atString>*>(user);
	atString file(ASSET.FileName(data.m_Name));
	file.Lowercase();
	if(file.EndsWith(".meta"))
	{
		files->PushAndGrow(file);
	}
}


//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::LoadFile(char const* filename)
{
	CZonedAssets* newZone = NULL;
	if (streamVerifyf(PARSER.LoadObjectPtr(filename, "", newZone), "Failed to load %s", filename))
	{
		sm_Zones.PushAndGrow(newZone);
	}
}


//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::UnloadFile(char const* filename)
{
	CZonedAssets* newZone = NULL;
	if (streamVerifyf(PARSER.LoadObjectPtr(filename, "", newZone), "Failed to unload %s", filename))
	{
		int toDelete = -1;
		for(int i = 0; i < sm_Zones.GetCount(); ++i)
		{
			if(sm_Zones[i]->m_Name == newZone->m_Name)
			{
				toDelete = i;
				break;
			}
		}

		if (streamVerifyf(toDelete != -1, "Failed to unload %s", filename))
		{
			sm_Zones.DeleteFast(toDelete);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::EnumAndProcessFolder(char const* pFolder, void(*process)(char const*))
{
	atVector<atString> files;
	ASSET.EnumFiles(pFolder, FindFileCallback, &files);
	ASSET.PushFolder(pFolder);
	for (int i = 0; i < files.GetCount(); ++i)
	{
		atString filename = files[i];

		(*process)(filename);
	}

	ASSET.PopFolder();
}


//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::Init()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::ZONED_ASSET_FILE, &g_ZonedAssetFileMounter);

	// load all the zones
	s_bEnableZonedAssets = false;
	if (CGameLogic::IsRunningGTA5Map())
	{
		EnumAndProcessFolder("common:/data/levels/gta5/resident/", LoadFile);

		// reset focus
		sm_LastFocus = Vec3V(V_FLT_MAX);

		s_bEnableZonedAssets = true;
#if __BANK
		if (PARAM_nozonedassets.Get())
		{
			s_bEnableZonedAssets = false;
		}
#endif	//__BANK
	}
}


//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::Shutdown(u32 shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		// Remove all the requests.
		Reset();

		// remove all the zones
		for (int i = 0; i < sm_Zones.GetCount(); ++i)
		{
			delete sm_Zones[i];
		}

		sm_Zones.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::Update(Vec3V_In focus)
{
#if __BANK
	if (s_bShowZonedAssets)
		ShowZonedAssets();

	if (s_bShowSelectedZoneDetails)
		ShowZoneDetails();

	DrawZoneBoxes();

	if (!s_bEnableZonedAssets)
		return;
#endif // __BANK

	// if focus is further than some distance from the last focus, recompute
	const ScalarV d2 = DistSquared(focus, sm_LastFocus);
	if (IsLessThanAll(d2, ScalarV(ZONE_RECOMPUTE_DISTANCE_SQUARE)))
	{
		return;
	}

	sm_LastFocus = focus;

	ClearRequests();

	// find all zones
	atVector<strIndex> requests;
	for (int i = 0; i < sm_Zones.GetCount(); ++i)
	{
		CZonedAssets& zone = *sm_Zones[i];

		if (zone.m_Extents.ContainsPoint(sm_LastFocus))
		{
			for (int j = 0; j < zone.m_Models.GetCount(); ++j)
			{
				AddRequest(zone.m_Models[j]);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::Reset()
{
	ClearRequests();
	sm_LastFocus = Vec3V(V_FLT_MAX);
}

//////////////////////////////////////////////////////////////////////////
 void CZonedAssetManager::ClearRequests()
{
	// clear all requests
	atMap<atHashString, strRequest>::Iterator entry = sm_Requests.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		entry.GetData().ClearRequiredFlags(STRFLAG_ZONEDASSET_REQUIRED); 
	} 

	sm_Requests.Reset();
}

//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::AddRequest(atHashString modelName)
{
	if (sm_Requests.Access(modelName))
		return;

	u32 archetypeStreamSlot = fwArchetypeManager::GetArchetypeIndexFromHashKey(modelName.GetHash());
	if (streamVerifyf(archetypeStreamSlot < fwModelId::MI_INVALID, "Invalid strModelRequest - model :%s is not found", modelName.GetCStr()))
	{
		strRequest request;
		request.Request(strLocalIndex(archetypeStreamSlot), CModelInfo::GetStreamingModuleId(), STRFLAG_ZONEDASSET_REQUIRED);
		sm_Requests.Insert(modelName, request);
	}
}

//////////////////////////////////////////////////////////////////////////
void CZonedAssetManager::SetEnabled(bool enabled)
{
	s_bEnableZonedAssets = enabled;
}

//////////////////////////////////////////////////////////////////////////

#if __BANK

enum
{
	SHOWBOX_NONE,
	SHOWBOX_ACTIVEONLY,
	SHOWBOX_ALL
};

const char* showBoxOptions[] =
{
	"None",
	"Active only",
	"All"
};

static atArray<const char*> s_ZoneNames;
static atMap<s32, u32> s_ZoneCostMap;

static void ToggleZonedAssets()
{
	if (!s_bEnableZonedAssets)
	{
		CZonedAssetManager::Reset();
	}
}

void CZonedAssetManager::InitWidgets(bkBank& bank)
{
	// init list of zones
	s_ZoneNames.ResetCount();
	for (int i = 0; i < sm_Zones.GetCount(); ++i)
	{
		CZonedAssets& zone = *sm_Zones[i];
		s_ZoneNames.Grow() = zone.m_Name.GetCStr();
	}

	bank.PushGroup("Zoned assets");
	bank.AddToggle("Enable zoned assets", &s_bEnableZonedAssets, datCallback(CFA(ToggleZonedAssets)));
	bank.AddToggle("Show zoned assets", &s_bShowZonedAssets);
	bank.AddSeparator("");
	bank.AddTitle("Debug Draw");
	bank.AddCombo("Draw zone boxes", &s_DrawBoxOption, 3, &showBoxOptions[0]);
	bank.AddToggle("Color boxes by cost", &s_bColorBoxesByCost);
	bank.AddSlider("Color view min (kb)", &s_fColorViewMin, 0, 50000, 1);
	bank.AddSlider("Color view max (kb)", &s_fColorViewMax, 0, 50000, 1);
	bank.AddSeparator("");
	bank.AddToggle("Show detail for selected zone", &s_bShowSelectedZoneDetails);
	if(s_ZoneNames.GetCount() > 0)
		bank.AddCombo("Selected zone", &s_SelectedZone, s_ZoneNames.GetCount(), &s_ZoneNames[0]);
	bank.PopGroup();
}

void CZonedAssetManager::ShowZonedAssets()
{
#if __DEV
	atVector<strIndex> indices;

	atMap<atHashString, strRequest>::Iterator entry = sm_Requests.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		indices.PushAndGrow(entry.GetData().GetIndex());
	} 

	CStreamingDebug::DisplayObjectList(NULL, true, indices.GetElements(), indices.GetCount());
#endif	//__DEV
}

void CZonedAssetManager::DrawZoneBoxes()
{
	if (s_DrawBoxOption == SHOWBOX_NONE)
		return;

	spdTransposedPlaneSet8 cullPlanes;
	const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
	cullPlanes.Set(grcVp, true, true);

	for (int i = 0; i < sm_Zones.GetCount(); ++i)
	{
		CZonedAssets& zone = *sm_Zones[i];

		bool bDrawBox = s_DrawBoxOption==SHOWBOX_ALL || (s_DrawBoxOption==SHOWBOX_ACTIVEONLY && zone.m_Extents.ContainsPoint(sm_LastFocus));
		if (bDrawBox && cullPlanes.IntersectsAABB(zone.m_Extents))
		{
			const Vec3V vMin = zone.m_Extents.GetMin();
			const Vec3V vMax = zone.m_Extents.GetMax();

			Color32 boxColor = (zone.m_Extents.ContainsPoint(sm_LastFocus)) ? Color_cyan : Color_red;

			if (s_bColorBoxesByCost)
			{
				const float fMin = (float) s_fColorViewMin;
				const float fMax = (float) s_fColorViewMax;
				float fZoneCost = (float) GetZoneCost(i);

				const float fClamped = Clamp(fZoneCost, fMin, fMax);
				
				boxColor.Setf(
					((fClamped - fMin) / (fMax - fMin)),
					1.0f - ((fClamped - fMin) / (fMax - fMin)),
					0.0f,
					1.0f
				);
			}

			grcDebugDraw::BoxAxisAligned(zone.m_Extents.GetMin(), zone.m_Extents.GetMax(), boxColor, false, 1);

			const Vec3V vTextPos(vMin.GetX(), vMax.GetY(), vMax.GetZ());
			grcDebugDraw::Text(vTextPos, boxColor, zone.m_Name.GetCStr());
		}
	}
}

void CZonedAssetManager::ShowZoneDetails()
{
	CZonedAssets& zone = *sm_Zones[s_SelectedZone];

	grcDebugDraw::AddDebugOutputEx(false, "Selected Zone: %s", zone.m_Name.GetCStr());
	
	for (s32 i=0; i<zone.m_Models.GetCount(); i++)
	{
		fwModelId modelId = CModelInfo::GetModelIdFromName(zone.m_Models[i].GetHash());

		u32 virtualSize  = 0;
		u32 physicalSize = 0;

		CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize);

#if __PS3
		grcDebugDraw::AddDebugOutputEx(false, "Model:%s Main:%dk Vram:%dk", zone.m_Models[i].GetCStr(), virtualSize>>10, physicalSize>>10);
#else
		grcDebugDraw::AddDebugOutputEx(false, "Model:%s Main:%dk", zone.m_Models[i].GetCStr(), (virtualSize+physicalSize)>>10);
#endif
	}
}

u32 CZonedAssetManager::GetZoneCost(s32 index)
{
	u32* const pResult = s_ZoneCostMap.Access(index);
	u32 totalCost = 0;
	if (pResult)
	{
		totalCost = *pResult;
	}
	else
	{
		CZonedAssets& zone = *sm_Zones[index];
		for (s32 i=0; i<zone.m_Models.GetCount(); i++)
		{
			fwModelId modelId = CModelInfo::GetModelIdFromName(zone.m_Models[i].GetHash());
			u32 virtualSize  = 0;
			u32 physicalSize = 0;
			CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize);
			totalCost += ( (virtualSize + physicalSize) >> 10 );
		}
		s_ZoneCostMap.Insert(index, totalCost);
	}
	
	return totalCost;
}

void CZonedAssetManager::GetZonedAssets(atArray<strIndex>& assets)
{
	atMap<atHashString, strRequest>::Iterator entry = sm_Requests.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		assets.PushAndGrow(entry.GetData().GetIndex());
	} 
}

#endif // __BANK
