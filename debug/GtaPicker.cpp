/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    GtaPicker.cpp
// PURPOSE : wrapper for our three picker types - world probe, intersecting entities, entity render
// AUTHOR :  
// CREATED : 15/04/11
//
/////////////////////////////////////////////////////////////////////////////////

#if __BANK

#include "debug/GtaPicker.h"

#include "system/memory.h"
#include "softrasterizer/tiler.h"

// Framework headers
#include "file/packfile.h"
#include "fwdrawlist/drawlist.h"
#include "fwmaths/kDOP18.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/metadatastore.h"
#include "fwscene/mapdata/mapdatadebug.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "vfx/ptfx/ptfxmanager.h"

// Game headers
#include "bank/slider.h"
#include "camera/CamInterface.h"
#include "debug/BlockView.h"
#include "debug/colorizer.h"
#include "debug/DebugScene.h"
#include "debug/DebugModelAnalysis.h"
#include "debug/DebugVoxelAnalysis.h"
#include "debug/TextureViewer/TextureViewerUtil.h"
#include "debug/UiGadget/UiGadgetInspector.h"
#include "debug/UiGadget/UiGadgetList.h"
#include "modelinfo/PedModelInfo.h"
#include "objects/DummyObject.h"
#include "objects/object.h"
#include "Peds/Ped.h"
#include "Peds/rendering/PedVariationDebug.h"
#include "physics/gtaArchetype.h"
#include "pickups/Pickup.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/GtaDrawable.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"
#include "renderer/occlusion.h"
#include "scene/2dEffect.h"
#include "scene/AnimatedBuilding.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/entities/compEntity.h"
#include "scene/Entity.h"
#include "scene/EntityBatch.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/LodMgr.h"
#include "scene/EntityTypes.h"
#include "scene/portals/PortalInst.h"
#include "scene/World/GameWorld.h"
#include "streaming/packfilemanager.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingdefs.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingengine.h"
#include "system/controlMgr.h"
#include "system/system.h"
#include "vehicles/vehicle.h"
#include "fwscene/world/StreamedSceneGraphNode.h"

#define NAME_OF_DEFAULT_PICKER "Default Picker"
#define NAME_OF_GPU_COST_PICKER "GPU Cost Picker"

#if __BANK
#include "fwscene/scan/ScanNodesDebug.h"
void BreakOnSelecedEntityCallback()
{
	if (g_PickerManager.GetSelectedEntity() && g_PickerManager.GetNumberOfEntities() == 1)
	{
		CEntity *pEntity = (CEntity *)g_PickerManager.GetSelectedEntity();
		if (pEntity->GetIplIndex())
		{
			fwMapDataContents* pContents = fwMapDataStore::GetStore().Get(strLocalIndex(pEntity->GetIplIndex()));
			if (pContents)
			{
				fwScanNodesDebug::SetSetGraphNodeToBreakOn((fwSceneGraphNode*)pContents->GetSceneGraphNode());
			}
		}
	}
}

class RunAtStartup
{
public:
	RunAtStartup() {fwScanNodesDebug::SetBreakOnSelectedEntityInContainerCallback(datCallback(BreakOnSelecedEntityCallback));}
};

RunAtStartup runAtStartup;
#endif

EntityDetailsArray CGtaPickerInterface::EntityDetails;

typedef fwEntity* _PickerResultType;
int CbPickerCompareBySize(const _PickerResultType* ppFwEnt1, const _PickerResultType* ppFwEnt2)
{
	const fwEntity *pFwEnt1 = *ppFwEnt1;
	const fwEntity *pFwEnt2 = *ppFwEnt2;
	const CEntity* pEntity1 = static_cast<const CEntity*>(pFwEnt1);
	const CEntity* pEntity2 = static_cast<const CEntity*>(pFwEnt2);
	float fRadius1 = pEntity1->GetBoundRadius();
	float fRadius2 = pEntity2->GetBoundRadius();

	if (fRadius1 < fRadius2)
	{
		return -1;
	}
	else if (fRadius1 > fRadius2)
	{
		return 1;
	}

	// equal in radius so sort by some arbitrary (but consistent) measure
	return ptrdiff_t_to_int((const u8*)pEntity1 - (const u8*)pEntity2);
}


/////////////////////////////////////////////////////////////////////////////////
//
// CGtaIntersectingEntityPicker
//
/////////////////////////////////////////////////////////////////////////////////
class CGtaIntersectingEntityPicker : public fwIntersectingEntitiesPicker
{
public:
	CGtaIntersectingEntityPicker()
		: fwIntersectingEntitiesPicker(INTERSECTING_ENTITY_PICKER) {}

	virtual void SetSelectedEntityInResultsArray();

private:
	virtual void GetMousePointing(Vector3& vMouseNear, Vector3& vMouseFar);
	virtual void GetCentreOfSphericalScan(Vector3& vSphereCentre);
	
	virtual void ForAllEntitiesIntersecting(fwSearchVolume* pColVol, fwIntersectingCB cb, void* data, s32 typeFlags, s32 locationFlags, s32 lodFlags);

	virtual void SortArrayOfIntersectingEntities(IntersectingEntitiesArray& ResultsArray);
};

void CGtaIntersectingEntityPicker::GetMousePointing(Vector3& vMouseNear, Vector3& vMouseFar)
{
	CDebugScene::GetMousePointing(vMouseNear, vMouseFar);
}

void CGtaIntersectingEntityPicker::GetCentreOfSphericalScan(Vector3& vSphereCentre)
{
	vSphereCentre = camInterface::GetPos();
}

void CGtaIntersectingEntityPicker::ForAllEntitiesIntersecting(fwSearchVolume* pColVol, fwIntersectingCB cb, void* data, s32 typeFlags, s32 locationFlags, s32 lodFlags)
{
	CGameWorld::ForAllEntitiesIntersecting(pColVol, cb, data, typeFlags, locationFlags, lodFlags);
}

void CGtaIntersectingEntityPicker::SortArrayOfIntersectingEntities(IntersectingEntitiesArray& ResultsArray)
{
	ResultsArray.QSort(0, -1, CbPickerCompareBySize);
}

void CGtaIntersectingEntityPicker::SetSelectedEntityInResultsArray()
{
	fwEntity *pEntityUnderMouse = NULL;
#if ENTITYSELECT_ENABLED_BUILD
	pEntityUnderMouse = CEntitySelect::GetSelectedEntity();
#endif

	s32 EntityIndex = -1;
	if (pEntityUnderMouse)
	{
		EntityIndex = g_PickerManager.GetIndexOfEntityWithinResultsArray(pEntityUnderMouse);
	}

	if (EntityIndex >= 0)
	{
		g_PickerManager.SetIndexOfSelectedEntity(EntityIndex);
	}
	else
	{
		g_PickerManager.SetIndexOfSelectedEntity(g_PickerManager.GetNumberOfEntities() - 1);
	}
}

/////////////////////////////////////////////////////////////////////////////////
//
// CGtaWorldProbePicker
//
/////////////////////////////////////////////////////////////////////////////////
class CGtaWorldProbePicker : public fwWorldProbePicker
{
public:
	CGtaWorldProbePicker()
		: fwWorldProbePicker(WORLD_PROBE_PICKER) {}

private:
	virtual fwEntity* GetEntityUnderMouse(s32 iFlags);
};

fwEntity* CGtaWorldProbePicker::GetEntityUnderMouse(s32 iFlags)
{
	return CDebugScene::GetEntityUnderMouse(NULL, NULL, iFlags);
}


/////////////////////////////////////////////////////////////////////////////////
//
// CGtaEntityRenderPicker
//
/////////////////////////////////////////////////////////////////////////////////
class CGtaEntityRenderPicker : public fwEntityRenderPicker
{
public:
	typedef fwEntityRenderPicker parent_type;
	CGtaEntityRenderPicker()
		: fwEntityRenderPicker(ENTITY_RENDER_PICKER) {}

	virtual bkGroup *AddWidgetGroup(bkBank *pBank);

private:
	virtual fwEntity* GetEntityFromRenderPicker();
};

bkGroup *CGtaEntityRenderPicker::AddWidgetGroup(bkBank *pBank)
{
	bkGroup *widgetGroup = parent_type::AddWidgetGroup(pBank);

	//Add Entity Select widgets for convenience.
	CEntitySelect::AddWidgets(*widgetGroup);

	return widgetGroup;
}

fwEntity* CGtaEntityRenderPicker::GetEntityFromRenderPicker()
{
#if ENTITYSELECT_ENABLED_BUILD
	return CEntitySelect::GetSelectedEntity();
#else
	return NULL;
#endif // ENTITYSELECT_ENABLED_BUILD
}

/////////////////////////////////////////////////////////////////////////////////
//
// CGtaPickerInterfaceSettings
//
/////////////////////////////////////////////////////////////////////////////////

PARAM(wipidbg,"wipidbg"); // wireframe picker interface debugging, it doesn't always behave right when you toggle wireframe while using the picker ..

CGtaPickerInterfaceSettings::CGtaPickerInterfaceSettings(bool bShowOnlySelectedEntity, bool bDisplayBoundsOfSelectedEntity, bool bDisplayBoundsOfEntitiesInList, bool bDisplayBoundsOfHoverEntity)
	: fwPickerInterfaceSettings(),
	m_bShowHideEnabled(bShowOnlySelectedEntity),
	m_ShowHideMode(PICKER_ISOLATE_SELECTED_ENTITY),
	m_bDisplayWireframe(PARAM_wipidbg.Get()), // wireframe on by default if -wipidbg is specified
	m_bDisplayBoundsOfSelectedEntity(bDisplayBoundsOfSelectedEntity),
	m_bDisplayBoundsOfEntitiesInList(bDisplayBoundsOfEntitiesInList),
	m_bDisplayBoundsOfHoverEntity(bDisplayBoundsOfHoverEntity),
	m_bDisplayBoundsInWorldspace(false),
	m_bDisplayBoundsOfGeometries(false),
	m_bDisplayVisibilityBoundsForSelectedEntity(false),
	m_bDisplayVisibilitySphereForSelectedEntity(false),

	m_bDisplayPhysicalBoundsForNodeOfSelectedEntity(false),
	m_bDisplayStreamingBoundsForNodeOfSelectedEntity(false),

	m_bDisplayVisibilitykDOPTest(false),
	m_bDisplayScreenQuadForSelectedEntity(false),
	m_bDisplayOcclusionScreenQuadForSelectedEntity(false),
	m_BoundingBoxOpacity(255)
{
}

/////////////////////////////////////////////////////////////////////////////////
//
// CGtaPickerInterface
//
/////////////////////////////////////////////////////////////////////////////////
CGtaPickerInterface::CGtaPickerInterface()
 : fwPickerInterface()
{
	USE_DEBUG_MEMORY();

	m_pResultsWindow = NULL;
	m_pInspectorWindow = NULL;
	m_pMemoryWindow = NULL;

	m_activeInspector = -1;
	m_bHideWindowEvent = false;
	m_bShowWindowEvent = false;
	m_bResetWindowPos = false;

#if DEBUG_ENTITY_GPU_COST
	m_EnableGPUCostPicker = false;
	m_pSelectedEntityGPUCostPicker = NULL;
	if(AssertVerify(gDrawListMgr != NULL))
	{
		m_GPUCostMode = gDrawListMgr->GetDebugEntityGPUCostMode();
	}
#endif

//	Add intersecting entity picker
	g_PickerManager.AddPicker(rage_new CGtaIntersectingEntityPicker(), "Intersecting Entities");


//	Add world probe picker
	CGtaWorldProbePicker *pNewWorldProbePicker = rage_new CGtaWorldProbePicker();
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_MAP_TYPE_MOVER, "GTA_MAP_TYPE_MOVER");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_MAP_TYPE_WEAPON, "GTA_MAP_TYPE_WEAPON");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_VEHICLE_TYPE, "GTA_VEHICLE_TYPE");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_BOX_VEHICLE_TYPE, "GTA_BOX_VEHICLE_TYPE");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_PED_TYPE, "GTA_PED_TYPE");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_RAGDOLL_TYPE, "GTA_RAGDOLL_TYPE");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_OBJECT_TYPE, "GTA_OBJECT_TYPE");	//	, true);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, "GTA_ENVCLOTH_OBJECT_TYPE");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_PLANT_TYPE, "GTA_PLANT_TYPE");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_PROJECTILE_TYPE, "GTA_PROJECTILE_TYPE");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_EXPLOSION_TYPE, "GTA_EXPLOSION_TYPE");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_PICKUP_TYPE, "GTA_PICKUP_TYPE");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_WEAPON_TEST, "GTA_WEAPON_TEST");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_CAMERA_TEST, "GTA_CAMERA_TEST");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_AI_TEST, "GTA_AI_TEST");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_SCRIPT_TEST, "GTA_SCRIPT_TEST");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_WHEEL_TEST, "GTA_WHEEL_TEST");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_GLASS_TYPE, "GTA_GLASS_TYPE");	//	, false);
	pNewWorldProbePicker->AddArchetypeMask(ArchetypeFlags::GTA_RIVER_TYPE, "GTA_RIVER_TYPE");	//	, false);
	g_PickerManager.AddPicker(pNewWorldProbePicker, "World Probe");


//	Add entity render picker
	g_PickerManager.AddPicker(rage_new CGtaEntityRenderPicker(), "Entity Render");


//	Add entity type flags that are checked by all pickers
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_BUILDING, "ENTITY_TYPE_MASK_BUILDING");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_ANIMATED_BUILDING, "ENTITY_TYPE_MASK_ANIMATED_BUILDING");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_VEHICLE, "ENTITY_TYPE_MASK_VEHICLE");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_PED, "ENTITY_TYPE_MASK_PED");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_OBJECT, "ENTITY_TYPE_MASK_OBJECT");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_DUMMY_OBJECT, "ENTITY_TYPE_MASK_DUMMY_OBJECT");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_LIGHT, "ENTITY_TYPE_MASK_LIGHT");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_COMPOSITE, "ENTITY_TYPE_MASK_COMPOSITE");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_INSTANCE_LIST, "ENTITY_TYPE_MASK_INSTANCE_LIST");	//	, true);
	g_PickerManager.AddEntityTypeMask(ENTITY_TYPE_MASK_GRASS_INSTANCE_LIST, "ENTITY_TYPE_MASK_GRASS_INSTANCE_LIST");	//	, true);


	m_highLod = 0.f;
	m_medLod = 0.f;
	m_lowLod = 0.f;
	m_vlowLod = 0.f;
	m_sliderHighLod = NULL;
	m_sliderMedLod = NULL;
	m_sliderLowLod = NULL;
	m_sliderVLowLod = NULL;
	m_lastSelectedEntity = NULL;
}

Vector2 vecDefaultPos(400.0f, 50.0f);
const float fDefaultW = 250.0f;

void CGtaPickerInterface::Init(unsigned initMode)
{
	USE_DEBUG_MEMORY();

	if (initMode == INIT_CORE)
	{
		SetDefaultPickerSettings(false);
		g_PickerManager.TakeControlOfPickerWidget(NAME_OF_DEFAULT_PICKER);

		InitDetailedView();
	}
	else if (initMode == INIT_SESSION)
	{
		m_pResultsWindow = rage_new CUiGadgetSimpleListAndWindow("Picker Results", vecDefaultPos.x, vecDefaultPos.y, fDefaultW);
		m_pResultsWindow->SetUpdateCellCB(SelectedEntityResultsCB);
		m_pResultsWindow->SetDisplay(false);

		CUiColorScheme colorScheme;
		float afInspectorColumnOffsets[] = { 0.0f, 100.0f };
		m_pInspectorWindow = rage_new CUiGadgetInspector(0.0f, 0.0f, 500.0f, MAX_INSPECTOR_ROWS, 2, afInspectorColumnOffsets, colorScheme);
		m_pInspectorWindow->SetUpdateCellCB(DetailedEntityResultsCB);
		m_pInspectorWindow->SetNumEntries(MAX_INSPECTOR_ROWS);
		AttachInspectorChild(m_pInspectorWindow);

		float afColumnOffsets[] = { 0.0f, 180.0f };
		m_pMemoryWindow = rage_new CUiGadgetInspector(0.f, 0.f, 400.0f, MAX_MEMORY_INSPECTOR_ROWS, 2, afColumnOffsets, colorScheme);
		m_pMemoryWindow->SetUpdateCellCB(PopulateMemoryInspectorCB);
		m_pMemoryWindow->SetNumEntries(MAX_MEMORY_INSPECTOR_ROWS);
		AttachInspectorChild(m_pMemoryWindow);
	}
}

void CGtaPickerInterface::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_SESSION)
	{
		DetachInspectorChild(m_pMemoryWindow);
		delete m_pMemoryWindow;
		m_pMemoryWindow = NULL;

		DetachInspectorChild(m_pInspectorWindow);
		delete m_pInspectorWindow;
		m_pInspectorWindow = NULL;

		delete m_pResultsWindow;
		m_pResultsWindow = NULL;
	}
}

void CGtaPickerInterface::ShowUIGadgets()
{
	if (m_pResultsWindow)
	{
		const char *pTitleOfWindow = g_PickerManager.GetOwnerName();
		if (!pTitleOfWindow || (strlen(pTitleOfWindow) == 0) )
		{
			pTitleOfWindow = "Picker Results";
		}

		m_pResultsWindow->UpdateTitle(pTitleOfWindow);

		m_pResultsWindow->SetNumEntries(g_PickerManager.GetNumberOfEntities());
		m_pResultsWindow->SetCurrentIndex(g_PickerManager.GetIndexOfSelectedEntity());
		m_pResultsWindow->RequestSetViewportToShowSelected();

		if (m_bResetWindowPos && m_pResultsWindow->GetParent())
		{
			m_pResultsWindow->GetParent()->SetPos(vecDefaultPos, true);
		}

		m_pResultsWindow->SetDisplay(true);

		if (m_activeInspector >= 0 && m_activeInspector < m_inspectorList.GetCount())
		{
			CUiGadgetInspector *pInspectorToAttach = m_inspectorList[m_activeInspector];
			if (pInspectorToAttach->GetParent() == NULL)
			{
				Vector2 vecParentWindowPos = m_pResultsWindow->GetPos();

				pInspectorToAttach->SetPos(Vector2(vecParentWindowPos.x + fDefaultW, vecParentWindowPos.y), true);
				m_pResultsWindow->AttachChild(pInspectorToAttach);
			}
		}

		m_bResetWindowPos = false;
	}
}

void CGtaPickerInterface::HideUIGadgets()
{
	if (m_pResultsWindow)
	{
		if (m_activeInspector >= 0 && m_activeInspector < m_inspectorList.GetCount())
		{
			if (m_inspectorList[m_activeInspector]->GetParent())
			{
				m_pResultsWindow->DetachChild(m_inspectorList[m_activeInspector]);
			}
		}

		m_pResultsWindow->SetDisplay(false);
	}
}

void CGtaPickerInterface::SetDefaultPickerSettings(bool bEnablePicker)
{
	fwPickerManagerSettings DebugPickerManagerSettings(ENTITY_RENDER_PICKER, bEnablePicker, false, 
		ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_ANIMATED_BUILDING
		|ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED
		|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT
		|ENTITY_TYPE_MASK_LIGHT|ENTITY_TYPE_MASK_INSTANCE_LIST
		|ENTITY_TYPE_MASK_GRASS_INSTANCE_LIST, false);
	g_PickerManager.SetPickerManagerSettings(&DebugPickerManagerSettings);

	CGtaPickerInterfaceSettings DebugPickerInterfaceSettings(false, true, false, false);
	g_PickerManager.SetPickerInterfaceSettings(&DebugPickerInterfaceSettings);

	fwIntersectingEntitiesPickerSettings DebugIntersectingEntitiesSettings(true, true, false, 100);
	g_PickerManager.SetPickerSettings(INTERSECTING_ENTITY_PICKER, &DebugIntersectingEntitiesSettings);

	fwWorldProbePickerSettings DebugWorldProbeSettings(
		ArchetypeFlags::GTA_MAP_TYPE_MOVER
		|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE
		|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_RAGDOLL_TYPE
		|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE
		|ArchetypeFlags::GTA_PLANT_TYPE|ArchetypeFlags::GTA_PROJECTILE_TYPE
		|ArchetypeFlags::GTA_EXPLOSION_TYPE|ArchetypeFlags::GTA_PICKUP_TYPE
		|ArchetypeFlags::GTA_WEAPON_TEST|ArchetypeFlags::GTA_CAMERA_TEST
		|ArchetypeFlags::GTA_AI_TEST|ArchetypeFlags::GTA_SCRIPT_TEST|ArchetypeFlags::GTA_WHEEL_TEST
		|ArchetypeFlags::GTA_GLASS_TYPE|ArchetypeFlags::GTA_RIVER_TYPE);
	g_PickerManager.SetPickerSettings(WORLD_PROBE_PICKER, &DebugWorldProbeSettings);
}

void CGtaPickerInterface::EnablePickerWithDefaultSettings()
{
	SetDefaultPickerSettings(true);
	g_PickerManager.TakeControlOfPickerWidget(NAME_OF_DEFAULT_PICKER);
}

#if DEBUG_ENTITY_GPU_COST
void CGtaPickerInterface::EnablePickEntityForGPUCost()
{
	SetDefaultPickerSettings(m_EnableGPUCostPicker);
	if(m_EnableGPUCostPicker)
	{
		g_PickerManager.TakeControlOfPickerWidget(NAME_OF_GPU_COST_PICKER);
		//CDebug::SetDisplayTimerBars(true);
		grcDebugDraw::SetDisplayDebugText(true);
		fwTimer::StartUserPause();
	}
	else
	{
		if(gDrawListMgr->GetShowGPUStatsForEntity())
		{
			//CDebug::SetDisplayTimerBars(false);
			grcDebugDraw::SetDisplayDebugText(false);
			fwTimer::EndUserPause();
		}
	}
	gDrawListMgr->SetShowGPUStatsForEntity(m_EnableGPUCostPicker);
}
void CGtaPickerInterface::OnChangeGpuCostMode()
{
	if(AssertVerify(gDrawListMgr != NULL))
	{
		gDrawListMgr->SetDebugEntityGPUCostMode(m_GPUCostMode);
	}
}
#endif

static void ShowRenderingStats_update()
{
	if (CPostScanDebug::GetCountModelsRendered())
	{
		CPostScanDebug::SetCountModelsRendered(false);
		gDrawListMgr->SetStatsHighlight(false);
	}
	else
	{
		CPostScanDebug::SetCountModelsRendered(true, DL_RENDERPHASE);
		gDrawListMgr->SetStatsHighlight(true, "DL_RENDERPHASE", true, false, false);
		grcDebugDraw::SetDisplayDebugText(true);
	}
}

void CGtaPickerInterface::AddGeneralWidgets(bkBank* pWidgetBank)
{
	if (Verifyf(pWidgetBank, "CGtaPickerInterface::AddGeneralWidgets - widget bank doesn't exist"))
	{
		pWidgetBank->AddButton("Enable Picker With Default Settings", datCallback(MFA(CGtaPickerInterface::EnablePickerWithDefaultSettings), this));
		pWidgetBank->AddButton("Toggle Window Visibility", datCallback(MFA(CGtaPickerInterface::ToggleWindowVisibility), this));
		pWidgetBank->AddButton("Reset Window Position", datCallback(MFA(CGtaPickerInterface::ResetWindowPosition), this));
		pWidgetBank->AddToggle("Show Visibility Flags For Selected Entity", &CPostScanDebug::ms_renderVisFlagsForSelectedEntity);
		static bool dummy = false;
		pWidgetBank->AddToggle("Show Rendering Stats", &dummy, ShowRenderingStats_update);

#if DEBUG_ENTITY_GPU_COST
		const char* kDebugGPUCostModes[] =
		{
			"Immediate",
			"Average",
			"Peak"
		};
		CompileTimeAssert(NELEM(kDebugGPUCostModes) == DEBUG_ENT_GPU_COST_TOTAL);
		pWidgetBank->PushGroup("Debug Entity GPU Picker", false);
		pWidgetBank->AddToggle("Enable Pick Entity for GPU Cost", &m_EnableGPUCostPicker, datCallback(MFA(CGtaPickerInterface::EnablePickEntityForGPUCost), this));
		pWidgetBank->AddCombo("GPU Cost Mode", (int*)&m_GPUCostMode, NELEM(kDebugGPUCostModes), kDebugGPUCostModes, datCallback(MFA(CGtaPickerInterface::OnChangeGpuCostMode), this));
		pWidgetBank->PopGroup();
#endif
		pWidgetBank->PushGroup("LOD Settings", false);
		m_sliderHighLod = pWidgetBank->AddSlider("High", &m_highLod, 0.f, 6000.f, 1.f, datCallback(MFA(CGtaPickerInterface::LodSettingsChanged), this));
		m_sliderMedLod = pWidgetBank->AddSlider("Med", &m_medLod, 0.f, 6000.f, 1.f, datCallback(MFA(CGtaPickerInterface::LodSettingsChanged), this));
		m_sliderLowLod = pWidgetBank->AddSlider("Low", &m_lowLod, 0.f, 6000.f, 1.f, datCallback(MFA(CGtaPickerInterface::LodSettingsChanged), this));
		m_sliderVLowLod = pWidgetBank->AddSlider("VLow", &m_vlowLod, 0.f, 6000.f, 1.f, datCallback(MFA(CGtaPickerInterface::LodSettingsChanged), this));
		pWidgetBank->PopGroup();
	}
}

#if ENTITY_GPU_TIME
static bool sm_bShowEntityGPUTime = false;
#endif // ENTITY_GPU_TIME

void CGtaPickerInterface::AddOptionsWidgets(bkBank *pWidgetBank)
{
	const char* kShowHideModeStrings[] =
	{
		"Isolate Selected Entity",
		"Isolate Selected Entity But Don't Touch Alpha",
		"Isolate Entities in List",
		"Hide Selected Entity",
		"Hide Entities in List",
	};
	pWidgetBank->AddToggle("Show/hide enabled", &m_Settings.m_bShowHideEnabled);
	pWidgetBank->AddCombo("Show/hide mode", (int*)&m_Settings.m_ShowHideMode, NELEM(kShowHideModeStrings), kShowHideModeStrings);
	pWidgetBank->AddSeparator();
	pWidgetBank->AddToggle("Display wireframe", &m_Settings.m_bDisplayWireframe, datCallback(MFA(CGtaPickerInterface::OnDisplayWireframeChange), this));
	pWidgetBank->AddSlider("Wireframe opacity", &CRenderPhaseDebugOverlayInterface::DrawOpacityRef(), 0.0f, 1.0f, 1.0f/32.0f);
	pWidgetBank->AddColor("Wireframe colour", &CRenderPhaseDebugOverlayInterface::DrawColourRef());

	if (PARAM_wipidbg.Get())
	{
		pWidgetBank->AddToggle(" -- WIREFRAME DRAW", &CRenderPhaseDebugOverlayInterface::DrawEnabledRef());
		pWidgetBank->AddToggle(" -- WIREFRAME COLOUR MODE", &CRenderPhaseDebugOverlayInterface::ColourModeRef());
	}

	pWidgetBank->AddSeparator();
	pWidgetBank->AddToggle("Display bounds of selected entity", &m_Settings.m_bDisplayBoundsOfSelectedEntity);
	pWidgetBank->AddToggle("Display bounds of entities in list", &m_Settings.m_bDisplayBoundsOfEntitiesInList);
	pWidgetBank->AddToggle("Display bounds of hover entity", &m_Settings.m_bDisplayBoundsOfHoverEntity);
	pWidgetBank->AddToggle("Display bounds in worldspace", &m_Settings.m_bDisplayBoundsInWorldspace);
	pWidgetBank->AddToggle("Display bounds of geometries", &m_Settings.m_bDisplayBoundsOfGeometries);
	pWidgetBank->AddToggle("Display visibility box for selected entity", &m_Settings.m_bDisplayVisibilityBoundsForSelectedEntity);
	pWidgetBank->AddToggle("Display visibility sphere for selected entity", &m_Settings.m_bDisplayVisibilitySphereForSelectedEntity);
	pWidgetBank->AddToggle("Display visibility kDOP test", &m_Settings.m_bDisplayVisibilitykDOPTest);
	pWidgetBank->AddToggle("Display screen quad for selected entity", &m_Settings.m_bDisplayScreenQuadForSelectedEntity);
	pWidgetBank->AddToggle("Display occlusion screen quad", &m_Settings.m_bDisplayOcclusionScreenQuadForSelectedEntity);
	pWidgetBank->AddSlider("Bounding box opacity", &m_Settings.m_BoundingBoxOpacity, 0, 255, 1);

	pWidgetBank->AddToggle("Display Physical Bounds For Node Of Selected Entity", &m_Settings.m_bDisplayPhysicalBoundsForNodeOfSelectedEntity);
	pWidgetBank->AddToggle("Display Streaming Bounds For Node Of Selected Entity", &m_Settings.m_bDisplayStreamingBoundsForNodeOfSelectedEntity);


#if ENTITY_GPU_TIME
	pWidgetBank->AddSeparator();
	pWidgetBank->AddToggle("Show entity GPU time", &sm_bShowEntityGPUTime);
#endif // ENTITY_GPU_TIME

	CDebugModelAnalysisInterface::AddWidgets(*pWidgetBank);
	CDebugVoxelAnalysisInterface::AddWidgets(*pWidgetBank);
}

bool CGtaPickerInterface::IsEntitySelectRequired()
{
#if ENTITYSELECT_ENABLED_BUILD
	if (g_PickerManager.IsEnabled())
	{
		if (g_PickerManager.IsPickerOfTypeEnabled(ENTITY_RENDER_PICKER) ||
			g_PickerManager.IsPickerOfTypeEnabled(INTERSECTING_ENTITY_PICKER))
		{
			return true;
		}
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	return false;
}

void CGtaPickerInterface::OnWidgetChange()
{
#if ENTITYSELECT_ENABLED_BUILD
	//	Intersecting Entity Picker uses the same code as Entity Render Picker 
	//	to choose the highlighted entity from the results list
	//	( see CGtaIntersectingEntityPicker::SetSelectedEntityInResultsArray() )
	if (IsEntitySelectRequired())
	{
		CEntitySelect::SetEntitySelectPassEnabled(); // turn on new entity selector
	}
	//	There used to be an else here that called CEntitySelect::DisableEntitySelectPass();
	//	but it could interfere with Bernie's Texture Viewer
#endif // ENTITYSELECT_ENABLED_BUILD

	OnDisplayWireframeChange();
}

void CGtaPickerInterface::OnDisplayWireframeChange()
{
	if (m_Settings.m_bDisplayWireframe)
	{
		if (g_PickerManager.IsEnabled())
		{
			CRenderPhaseDebugOverlayInterface::DrawEnabledRef() = true;
			CRenderPhaseDebugOverlayInterface::SetColourMode(DOCM_PICKER);
		}
		else
		{
			CRenderPhaseDebugOverlayInterface::DrawEnabledRef() = false;
			CRenderPhaseDebugOverlayInterface::SetColourMode(DOCM_NONE);
		}
	}
}

void CGtaPickerInterface::ToggleWindowVisibility()
{
	g_PickerManager.SetUiEnabled(!g_PickerManager.IsUiEnabled());

	if (g_PickerManager.IsUiEnabled())
	{
		m_bShowWindowEvent = true;
	}
	else
	{
		m_bHideWindowEvent = true;
	}
}

void CGtaPickerInterface::ResetWindowPosition()
{
	m_bResetWindowPos = true;
}

void CGtaPickerInterface::LodSettingsChanged()
{
	if (m_lastSelectedEntity && g_PickerManager.GetSelectedEntity() == m_lastSelectedEntity)
	{
		if (m_lastSelectedEntity->GetDrawHandlerPtr())
		{
			const fwArchetype* pArchetype = m_lastSelectedEntity->GetArchetype();
			Assert(pArchetype);
			rmcDrawable* drawable = pArchetype->GetDrawable();
			if (drawable)
			{
				drawable->GetLodGroup().SetLodThresh(LOD_HIGH, m_highLod);
				drawable->GetLodGroup().SetLodThresh(LOD_MED, m_medLod);
				drawable->GetLodGroup().SetLodThresh(LOD_LOW, m_lowLod);
				drawable->GetLodGroup().SetLodThresh(LOD_VLOW, m_vlowLod);
			}
		}
	}
}

void CGtaPickerInterface::UpdateLodSettings()
{
	if (g_PickerManager.GetSelectedEntity() != m_lastSelectedEntity)
	{
		m_highLod = 0.f;
		m_medLod = 0.f;
		m_lowLod = 0.f;
		m_vlowLod = 0.f;

		m_lastSelectedEntity = g_PickerManager.GetSelectedEntity();

		if (m_lastSelectedEntity && m_lastSelectedEntity->GetDrawHandlerPtr())
		{
			const fwArchetype* pArchetype = m_lastSelectedEntity->GetArchetype();
			Assert(pArchetype);
			rmcDrawable* drawable = pArchetype->GetDrawable();

			if (drawable)
			{
				if (drawable->GetLodGroup().ContainsLod(LOD_HIGH))
				{
					m_highLod = drawable->GetLodGroup().GetLodThresh(LOD_HIGH);
					m_medLod = drawable->GetLodGroup().GetLodThresh(LOD_MED);
					m_lowLod = drawable->GetLodGroup().GetLodThresh(LOD_LOW);
					m_vlowLod = drawable->GetLodGroup().GetLodThresh(LOD_VLOW);
				}
			}
		}
	}
}

void CGtaPickerInterface::OnDisplayResultsWindow()
{
	HideUIGadgets();

	if (g_PickerManager.IsEnabled())
	{
		ShowUIGadgets();
	}
}

void CGtaPickerInterface::AttachInspectorChild(CUiGadgetInspector* child)
{
	if (child)
	{
		m_inspectorList.PushAndGrow(child);
	}
}

void CGtaPickerInterface::DetachInspectorChild(CUiGadgetInspector* child)
{
	if (child)
	{
		for (s32 i = 0; i < m_inspectorList.GetCount(); ++i)
		{
			if (m_inspectorList[i] == child)
			{
				if (m_activeInspector == i)
				{
					if (m_inspectorList[m_activeInspector]->GetParent())
					{
						m_pResultsWindow->DetachChild(m_inspectorList[m_activeInspector]);
					}
				}
				m_inspectorList.Delete(i);
				if (m_activeInspector >= i)
				{
					m_activeInspector--;
				}
				break;
			}
		}
		OnDisplayResultsWindow();
	}
}

CUiGadgetInspector* CGtaPickerInterface::GetCurrentInspector()
{
	if (m_activeInspector != -1)
	{
		return m_inspectorList[m_activeInspector];
	}

	return NULL;
}

CUiGadgetSimpleListAndWindow* CGtaPickerInterface::GetMainWindow()
{
	return m_pResultsWindow;
}

void EntityDetails_GetModelName(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		if (pEntity->IsArchetypeSet())
		{
			char debugText[50];
			sprintf(debugText, "%s", pEntity->GetBaseModelInfo()->GetModelName());
			ReturnString = debugText;
		}
	}
}

void EntityDetails_GetModelTypeFile(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity && pEntity->IsArchetypeSet())
	{
		fwModelId id = pEntity->GetModelId();
		if (id.IsValid())
		{
			if (id.IsGlobal())
			{
				ReturnString = "GLOBAL";
			} 
			else
			{
				if (id.GetMapTypeDefIndex().IsInvalid())
				{
					ReturnString = "EXTRA";
				}
				else
				{
					ReturnString = AssetAnalysis::GetAssetPath(g_MapTypesStore, id.GetMapTypeDefIndex().Get(), -1);
				}
			}
		}
	}
}

void EntityDetails_GetModelIndex(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		if (pEntity->IsArchetypeSet())
		{
			char debugText[16];
			sprintf(debugText, "%d", pEntity->GetModelIndex());
			ReturnString = debugText;
		}
	}
}

void EntityDetails_GetType(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		ReturnString = GetEntityTypeStr(pEntity->GetType());
	}
}

void EntityDetails_GetLODType(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		ReturnString = pEntity->GetLodData().GetLodTypeName();
	}
}

void EntityDetails_GetLODDist(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		CBaseModelInfo* pMI = NULL;
		if (pEntity->IsArchetypeSet())
		{
			pMI = pEntity->GetBaseModelInfo();
		}
		char debugText[16];
		if (pMI && !AreNearlyEqual(pMI->GetLodDistanceUnscaled(), float(pEntity->GetLodDistance())))
		{
			sprintf(debugText, "%d (%d)", pEntity->GetLodDistance(), u32(pMI->GetLodDistanceUnscaled()));
		}
		else
		{
			sprintf(debugText, "%d", pEntity->GetLodDistance());
		}

		ReturnString = debugText;
	}
}

void EntityDetails_GetLodParent(CEntity* pEntity, atString& ReturnString)
{
	if (pEntity && pEntity->GetLodData().HasLod())
	{
		fwEntity* pLod = pEntity->GetLod();
		if (pLod)
		{
			ReturnString = pLod->GetModelName();
			return;
		}
	}
	ReturnString = "";
}

void EntityDetails_GetAlpha(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[16];
		sprintf(debugText, "%d", pEntity->GetAlpha());
		ReturnString = debugText;
	}
}

void EntityDetails_GetNaturalScale(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[4];
		sprintf(debugText, "%d", pEntity->GetNaturalAmbientScale());
		ReturnString = debugText;
	}
}

void EntityDetails_GetArtificialScale(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[4];
		sprintf(debugText, "%d", pEntity->GetArtificialAmbientScale());
		ReturnString = debugText;
	}
}

void EntityDetails_GetIsHD(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[2];
		if (pEntity->GetLodData().IsHighDetail())
		{
			sprintf(debugText, "Y");
		}
		else
		{
			sprintf(debugText, "N");
		}
		ReturnString = debugText;
	}
}

void EntityDetails_GetNumChildren(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[16];
		sprintf(debugText, "%d", pEntity->GetLodData().GetNumChildren());
		ReturnString = debugText;
	}
}

void EntityDetails_GetDistFromCam(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[16];
		sprintf(debugText, "%4.2f m", CLodDebug::GetDistFromCamera(pEntity));
		ReturnString = debugText;
	}
}

void EntityDetails_GetBlock(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[64];

		s32 blockIndex = fwMapDataDebug::GetBlockIndex(pEntity->GetIplIndex());

		if (blockIndex>=0 && blockIndex<CBlockView::GetNumberOfBlocks())
		{
			sprintf(debugText, "%s", CBlockView::GetBlockName(blockIndex));
			ReturnString = debugText;
		}
	}
}

void EntityDetails_GetAddress(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		char debugText[24];
		sprintf(debugText, "0x%p", pEntity);
		ReturnString = debugText;
	}
}

void EntityDetails_GetPolys(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		if (pEntity->IsArchetypeSet())
		{
			CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
			if (pMI->GetHasLoaded())
			{
				char debugText[24];
				sprintf(debugText, "%d", ColorizerUtils::GetPolyCount(pEntity));
				ReturnString = debugText;
			}
		}
	}
}

void EntityDetails_GetBones(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		if (pEntity->IsArchetypeSet())
		{
			CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
			if (pMI->GetHasLoaded())
			{
				char debugText[24];
				sprintf(debugText, "%d", ColorizerUtils::GetBoneCount(pEntity));
				ReturnString = debugText;
			}
		}
	}
}

void EntityDetails_GetShaders(CEntity *pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		if (pEntity->IsArchetypeSet())
		{
			CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
			if (pMI->GetHasLoaded())
			{
				atArray<const grmShader*> uniqueShaders;
				const int count = GetShadersUsedByEntity(uniqueShaders, pEntity, true);
				const rmcDrawable* pDrawable = pMI->GetDrawable();
				ReturnString = atVarString("%d/%d (%d unique)", pDrawable ? pDrawable->GetShaderGroup().GetCount() : 0, count, uniqueShaders.size());
			}
		}
	}
}

void EntityDetails_GetDrawableType(CBaseModelInfo* pBaseModelInfo, atString &ReturnString)
{
	if (pBaseModelInfo)
	{
		switch (pBaseModelInfo->GetDrawableType())
		{
		case fwArchetype::DT_UNINITIALIZED:
			ReturnString = "UNINITIALIZED";
			break;
		case fwArchetype::DT_FRAGMENT:
			ReturnString = "FRAGMENT";
			break;
		case fwArchetype::DT_DRAWABLE:
			ReturnString = "DRAWABLE";
			break;
		case fwArchetype::DT_DRAWABLEDICTIONARY:
			ReturnString = "DRAWABLEDICTIONARY";
			break;
		case fwArchetype::DT_ASSETLESS:
			ReturnString = "ASSETLESS";
			break;
		default:
			break;
		}
	}
}

void EntityDetails_GetDrawableType(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		CBaseModelInfo* pBaseModelInfo = pEntity->GetBaseModelInfo();
		EntityDetails_GetDrawableType(pBaseModelInfo, ReturnString);
	}
}

void EntityDetails_GetPhysicsDict(CEntity* /*pEntity*/, atString &ReturnString)
{
	ReturnString = "";
}

void EntityDetails_GetAnimationDict(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		strLocalIndex clipDictionarySlot = strLocalIndex(pModelInfo->GetClipDictionaryIndex());
		if (clipDictionarySlot != -1)
		{
			ReturnString = g_ClipDictionaryStore.GetName(clipDictionarySlot);
			return;
		}
	}
	ReturnString = "";
}

void EntityDetails_GetWorldLocation(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		// not in scene graph at all?
		if (!pEntity->GetIsAddedToWorld())
		{
			ReturnString = "Not in scene graph";
			return;
		}

		// in an interior?
		fwInteriorLocation interiorLoc = pEntity->GetInteriorLocation();
		if (interiorLoc.IsValid())
		{
			CInteriorProxy *pInteriorProxy = CInteriorProxy::GetFromLocation(interiorLoc);

			const int size = 64;
			char achTmp[size];
			snprintf(achTmp, size, "%s:%d", pInteriorProxy->GetName().GetCStr(), interiorLoc.GetInteriorProxyIndex());

			if (interiorLoc.IsAttachedToRoom())
			{				
				snprintf(achTmp, size, "%s, Room:%d", achTmp, interiorLoc.GetRoomIndex());
			}
			else if (interiorLoc.IsAttachedToPortal())
			{
				snprintf(achTmp, size, "%s,Portal:%d", achTmp, interiorLoc.GetPortalIndex());
			}
			achTmp[size - 1] = '\0';
			ReturnString = achTmp;
			return;
		}
	}

	// retained by interior?
	if (pEntity->GetIsRetainedByInteriorProxy() && pEntity->GetRetainingInteriorProxy())
	{
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot( pEntity->GetRetainingInteriorProxy() );

		ReturnString = "Retained by: ";
		ReturnString += ( pIntProxy->GetName().GetCStr() );
		return;
	}

	// exterior
	ReturnString = "Exterior";
}

void EntityDetails_GetImapPath(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		strLocalIndex index = strLocalIndex(pEntity->GetIplIndex());
		if (index.Get()>0)
		{
			ReturnString = AssetAnalysis::GetAssetPath(fwMapDataStore::GetStore(), index.Get(), -1);
		}
		else
		{
			ReturnString = "";
		}
	}
}

void EntityDetails_GetGuid(CEntity* pEntity, atString& ReturnString)
{
	if (pEntity)
	{
		u32 guid;
		bool bHasGuid = pEntity->GetGuid(guid);
		if (bHasGuid)
		{
			char debugText[32];
			sprintf(debugText, "0x%x", guid);
			ReturnString = debugText;
			return;
		}
	}
	ReturnString = "";
}

template <int n> void EntityDetails_GetParentTxd(CEntity* pEntity, atString &ReturnString)
{
	const CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

	if (pModelInfo)
	{
		strLocalIndex parentTxdSlot = strLocalIndex(pModelInfo->GetAssetParentTxdIndex());

		for (int i = 0; parentTxdSlot != -1 && i < n; i++)
		{
			parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
		}

		if (parentTxdSlot != -1)
		{
			ReturnString = g_TxdStore.GetName(parentTxdSlot);

			if (g_TxdStore.GetSlot(parentTxdSlot)->m_isDummy)
			{
				ReturnString += atString(" (dummy)");
			}
		}
	}
}

void EntityDetails_FramesSinceRender(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity->GetDrawHandlerPtr() && pEntity->GetDrawHandlerPtr()->GetActiveListLink())
		ReturnString = atVarString("%d", pEntity->GetDrawHandler().GetActiveListLink()->item.GetTimeSinceLastVisible());
	else
		ReturnString = atString("no drawhandler/not in active list");
}

void EntityDetails_HasDrawablePhysics(CEntity* pEntity, atString& returnString)
{
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if (pModelInfo && pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLE && pModelInfo->GetHasLoaded())
		{
			gtaDrawable* pDrawable = (gtaDrawable*) pModelInfo->GetDrawable();
			if (pDrawable && pDrawable->m_pPhBound)
			{
				returnString = atVarString( "Yes" );
				return;
			}
		}
	}
	
	returnString = "n/a";
}

void EntityDetails_GetNeverDummyFlag(CEntity* pEntity, atString& returnString)
{
	returnString = (pEntity && pEntity->m_nFlags.bNeverDummy) ? "YES" : "NO";
}

#if ENTITY_GPU_TIME
void EntityDetails_GetEntityGPUTime(CEntity*, atString &ReturnString)
{
	if (sm_bShowEntityGPUTime)
	{
		const float t0 = dlCmdEntityGPUTimeFlush::GetTime();
		const float t1 = 1.0f;//CSystem::GetRageSetup()->GetGpuTime();

		ReturnString = atVarString("%.4f/%.4f ms (%.3f%%)", t0, t1, 100.0f*t0/t1);
	}
	else
	{
		ReturnString = "";
	}
}
#endif // ENTITY_GPU_TIME

void EntityDetails_GetAABB(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		spdAABB entityBounds;
		pEntity->GetAABB(entityBounds);
		if ( entityBounds.IsValid() )
		{
			Vec3V vMin = entityBounds.GetMin();
			Vec3V vMax = entityBounds.GetMax();
			const float fDimX = ( vMax.GetXf() - vMin.GetXf() );
			const float fDimY = ( vMax.GetYf() - vMin.GetYf() );
			const float fDimZ = ( vMax.GetZf() - vMin.GetZf() );
			ReturnString = atVarString("%.2f x %.2f x %.2f m", fDimX, fDimY, fDimZ);
		}
	}
}

void EntityDetails_GetAABBVis(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity && !pEntity->GetIsRetainedByInteriorProxy())
	{
		spdAABB entityBounds;
		fwBaseEntityContainer* pContainer = pEntity->GetOwnerEntityContainer();
		if (pContainer)
		{
			u32 index = pContainer->GetEntityDescIndex(pEntity);
			if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
			{
				pContainer->GetApproxBoundingBox(index, entityBounds);
			}
		}

		if ( entityBounds.IsValid() )
		{
			Vec3V vMin = entityBounds.GetMin();
			Vec3V vMax = entityBounds.GetMax();
			const float fDimX = ( vMax.GetXf() - vMin.GetXf() );
			const float fDimY = ( vMax.GetYf() - vMin.GetYf() );
			const float fDimZ = ( vMax.GetZf() - vMin.GetZf() );
			ReturnString = atVarString("%.2f x %.2f x %.2f m", fDimX, fDimY, fDimZ);
		}
	}
}

void EntityDetails_GetHDDist(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if (pModelInfo && pModelInfo->GetIsHDTxdCapable())
		{
			ReturnString = atVarString( "%.01f", pModelInfo->GetHDTextureDistance() );
			return;
		}
	}

	ReturnString = "";
}

void EntityDetails_GetSniperDamage(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity && pEntity->GetIsTypeObject())
	{
		CObject* pObj = (CObject*) pEntity;
		ReturnString = atVarString("%d (%d)", pObj->GetDamageScanCode(), CObject::GetCurrentDamageScanCode());
		return;
	}
	ReturnString = "";
}

void EntityDetails_GetOwner(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		switch (pEntity->GetOwnedBy())
		{
		case ENTITY_OWNEDBY_RANDOM:
			ReturnString = "RANDOM";
			break;
		case ENTITY_OWNEDBY_TEMP:
			ReturnString = "TEMP";
			break;
		case ENTITY_OWNEDBY_FRAGMENT_CACHE:
			ReturnString = "FRAGMENT_CACHE";
			break;
		case ENTITY_OWNEDBY_GAME:
			ReturnString = "GAME";
			break;
		case ENTITY_OWNEDBY_SCRIPT:
			ReturnString = "SCRIPT";
			break;
		case ENTITY_OWNEDBY_AUDIO:
			ReturnString = "AUDIO";
			break;
		case ENTITY_OWNEDBY_CUTSCENE:
			ReturnString = "CUTSCENE";
			break;
		case ENTITY_OWNEDBY_DEBUG:
			ReturnString = "DEBUG";
			break;
		case ENTITY_OWNEDBY_OTHER:
			ReturnString = "OTHER";
			break;
		case ENTITY_OWNEDBY_PROCEDURAL:
			ReturnString = "PROCEDURAL";
			break;
		case ENTITY_OWNEDBY_POPULATION:
			ReturnString = "POPULATION";
			break;
		case ENTITY_OWNEDBY_STATICBOUNDS:
			ReturnString = "STATICBOUNDS";
			break;
		case ENTITY_OWNEDBY_PHYSICS:
			ReturnString = "PHYSICS";
			break;
		case ENTITY_OWNEDBY_IPL:
			ReturnString = "IPL";
			break;
		case ENTITY_OWNEDBY_VFX:
			ReturnString = "VFX";
			break;
		case ENTITY_OWNEDBY_NAVMESHEXPORTER:
			ReturnString = "NAVMESHEXPORTER";
			break;
		case ENTITY_OWNEDBY_INTERIOR:
			ReturnString = "INTERIOR";
			break;
		case ENTITY_OWNEDBY_COMPENTITY:
			ReturnString = "COMPENTITY";
			break;
		case ENTITY_OWNEDBY_EXPLOSION:
			ReturnString = "EXPLOSION";
			break;
		case ENTITY_OWNEDBY_LIVEEDIT:
			ReturnString = "LIVEEDIT";
			break;
		case ENTITY_OWNEDBY_REPLAY:
			ReturnString = "REPLAY";
			break;
		case ENTITY_OWNEDBY_LAYOUT_MGR:
			ReturnString = "LAYOUT_MGR";
			break;
		default:
			break;
		}

		return;
	}
	ReturnString = "";
}

strIndex EntityDetails_StreamingIndex(CBaseModelInfo* pBaseModelInfo)
{
	strIndex streamingIndex;

	switch (pBaseModelInfo->GetDrawableType())
	{
	case fwArchetype::DT_FRAGMENT:
		{
			s32 fragIndex = pBaseModelInfo->GetFragmentIndex();
			if (fragIndex != INVALID_FRAG_IDX)
			{
				streamingIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(fragIndex));
			}
		}
		break;
	case fwArchetype::DT_DRAWABLE:
		{
			s32 drawableIndex = pBaseModelInfo->GetDrawableIndex();
			if (drawableIndex != INVALID_DRAWABLE_IDX)
			{
				streamingIndex = g_DrawableStore.GetStreamingIndex(strLocalIndex(drawableIndex));
			}
		}
		break;
	case fwArchetype::DT_DRAWABLEDICTIONARY:
		{
			s32 drawableDictIndex = pBaseModelInfo->GetDrawDictIndex();
			if (drawableDictIndex != INVALID_DRAWDICT_IDX)
			{
				streamingIndex = g_DwdStore.GetStreamingIndex(strLocalIndex(drawableDictIndex));
			}
		}
		break;

	case fwArchetype::DT_UNINITIALIZED:
	case fwArchetype::DT_ASSETLESS:
	default:
		break;
	}

	return streamingIndex;
}

void EntityDetails_GetPackfile(CEntity* pEntity, atString &ReturnString)
{
	if (pEntity)
	{
		CBaseModelInfo* pBaseModelInfo = pEntity->GetBaseModelInfo();
		if (pBaseModelInfo)
		{
			strIndex streamingIndex = EntityDetails_StreamingIndex(pBaseModelInfo);

			if (streamingIndex.IsValid())
			{
				strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
				strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

				if (pFile)
				{
					ReturnString = pFile->m_packfile->GetDebugName();
				}
				else
				{
					ReturnString = "[loose file]";
				}
			}
		}
	}
}

void EntityDetails_GetDrawablePath(CEntity* pEntity, atString& ReturnString)
{
	if (pEntity && pEntity->IsArchetypeSet())
	{
		ReturnString = AssetAnalysis::GetDrawablePath(pEntity->GetBaseModelInfo());
	}
}

void CGtaPickerInterface::InitDetailedView()
{
	EntityDetails.Reset();

	int numRows = 0;
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Model Name",			EntityDetails_GetModelName); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Model Index",		EntityDetails_GetModelIndex); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Model type file",	EntityDetails_GetModelTypeFile); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Type",				EntityDetails_GetType); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("LOD type",			EntityDetails_GetLODType); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("LOD dist",			EntityDetails_GetLODDist); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("LOD parent",			EntityDetails_GetLodParent); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Alpha",				EntityDetails_GetAlpha); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Natural Scale",		EntityDetails_GetNaturalScale); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Artificial Scale",	EntityDetails_GetArtificialScale); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Is HD?",				EntityDetails_GetIsHD); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Num children",		EntityDetails_GetNumChildren); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Dist from cam",		EntityDetails_GetDistFromCam); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Block",				EntityDetails_GetBlock); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Address",			EntityDetails_GetAddress); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Polys",				EntityDetails_GetPolys); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Bones",				EntityDetails_GetBones); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Shaders",			EntityDetails_GetShaders); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("DrawableType",		EntityDetails_GetDrawableType); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("DrawablePath",		EntityDetails_GetDrawablePath); }
	//if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("RPF",				EntityDetails_GetPackfile); }		// the rpf filename is contained in the mount path just above
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("IMAP",				EntityDetails_GetImapPath); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("GUID",				EntityDetails_GetGuid); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Physics",			EntityDetails_GetPhysicsDict); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Animation",			EntityDetails_GetAnimationDict); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Location",			EntityDetails_GetWorldLocation); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Parent txd[0]",		EntityDetails_GetParentTxd<0>); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Parent txd[1]",		EntityDetails_GetParentTxd<1>); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Parent txd[2]",		EntityDetails_GetParentTxd<2>); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Parent txd[3]",		EntityDetails_GetParentTxd<3>); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Parent txd[4]",		EntityDetails_GetParentTxd<4>); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Last render",		EntityDetails_FramesSinceRender); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Drawable phys",		EntityDetails_HasDrawablePhysics); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Never dummy?",		EntityDetails_GetNeverDummyFlag); }

#if ENTITY_GPU_TIME
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("GPU time",			EntityDetails_GetEntityGPUTime); }
#endif // ENTITY_GPU_TIME

	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Bbox",				EntityDetails_GetAABB); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Bbox (Vis)",			EntityDetails_GetAABBVis); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("HD Dist",			EntityDetails_GetHDDist); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Sniper Damage",		EntityDetails_GetSniperDamage); }
	if (AssertVerify(++numRows <= MAX_INSPECTOR_ROWS)) { PushDetailedView("Owner",				EntityDetails_GetOwner);  }


	// note: must increase MAX_INSPECTOR_ROWS if you add more rows
}

void CGtaPickerInterface::PushDetailedView(const char *pTitle, EntityDetailFunc pGetDetailFunction)
{
	EntityDetails.Push(EntityDetailStruct(pTitle, pGetDetailFunction));
}

void CGtaPickerInterface::SelectedEntityResultsCB(CUiGadgetText* pResult, u32 row, u32 UNUSED_PARAM(col), void * UNUSED_PARAM(extraCallbackData) )
{
	if (pResult && row < g_PickerManager.GetNumberOfEntities())
	{
		const fwEntity *pEntity = g_PickerManager.GetEntity(row);
		pResult->SetString(GetEntityDetailsString(pEntity));
	}
}

void CGtaPickerInterface::GetObjectAndDependenciesSplitSizes(CEntity* ent, fwModelId modelId, const strIndex* ignoreList, s32 numIgnores,
															 u32& virtGeom, u32& physGeom, u32& virtTex, u32& physTex, u32& virtClip, u32& physClip,
															 u32& virtFrag, u32& physFrag, u32& virtTotal, u32& physTotal)
{
	strIndex backingStore[STREAMING_MAX_DEPENDENCIES*2];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES*2);

	CModelInfo::GetObjectAndDependencies(modelId, deps, ignoreList, numIgnores);

	if (ent && ent->GetIsTypePed() && ((CPed*)ent)->GetPedModelInfo()->GetIsStreamedGfx())
		((CPed*)ent)->AppendAssetStreamingIndices(deps, ignoreList, numIgnores);

	for (s32 i = 0; i < deps.GetCount(); ++i)
	{
		s32 itemVirtSize = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i], true);
		s32 itemPhysSize = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i], true);
		const char* name = strStreamingEngine::GetInfo().GetObjectName(deps[i]);
		//Displayf("Dependency %d: %s - virt: %d phys: %d", i, name, itemVirtSize, itemPhysSize);

		strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(deps[i]);

		if (module == &g_DrawableStore || module == &g_DwdStore)
		{
			virtGeom += itemVirtSize;
			physGeom += itemPhysSize;
			virtTotal += itemVirtSize;
			physTotal += itemPhysSize;
		}
		else if (module == &g_TxdStore || module == &ptfxManager::GetAssetStore())
		{
			virtTex += itemVirtSize;
			physTex += itemPhysSize;
			virtTotal += itemVirtSize;
			physTotal += itemPhysSize;
		}
		else if (module == &g_ClipDictionaryStore)
		{
			virtClip += itemVirtSize;
			physClip += itemPhysSize;
			virtTotal += itemVirtSize;
			physTotal += itemPhysSize;
		}
		else if (module == &g_FragmentStore)
		{
			virtFrag += itemVirtSize;
			physFrag += itemPhysSize;
			virtTotal += itemVirtSize;
			physTotal += itemPhysSize;
		}
		else if (itemVirtSize > 0 || itemPhysSize > 0)
		{
			// ignore expressions, metadata etc.
			if (module != &g_ExpressionDictionaryStore &&
				module != &g_fwMetaDataStore &&
				module != &g_ClothStore &&
				!strstr(name, "not resident"))
			{
				Assertf(false, "Dependency %s not counted in memory display! module=%s", name, module->GetModuleName());
			}
		}
	}
}

static const char* inspectorEntries[MAX_MEMORY_INSPECTOR_ROWS] = {"", "Dwd", "Txd", "Clip", "Frag", "Subtotal", "", "", "Vertices", "Indices", "Polys", "Vertices (lod 1)", "Indices (lod 1)", "Polys (lod 1)", "Vertices (lod 2)", "Indices (lod 2)", "Polys (lod 2)", "Vertices (lod 3)", "Indices (lod 3)", "Polys (lod 3)", "Vertices (lod 4)", "Indices (lod 4)", "Polys (lod 4)"};

// TODO -- use GetEntityTypeStr
static const char* entityTypeStrings[] =
{
	"nothing",
	"build",
	"anim build",
	"veh",
	"ped",
	"object",
	"dummy",
	"portal",
	"mlo",
	"not-in-pools",
	"particle",
	"light",
	"composite",
	"instance list",
	"grass inst list",
	"vehicle glass list",
};
CompileTimeAssert(NELEM(entityTypeStrings) == ENTITY_TYPE_TOTAL);

void CGtaPickerInterface::PopulateMemoryInspectorCB(CUiGadgetText* pResult, u32 row, u32 col )
{
	if (pResult && row < MAX_MEMORY_INSPECTOR_ROWS)
	{
		CEntity *pEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
		if (pEntity)
		{
			enum
			{
				GEOMETRY = 0,
				TEXTURE,
				CLIP,
				FRAG,
				SUBTOTAL,
				VERTICES_HD,
				VERTEX_SIZE_HD,
				INDICES_HD,
				INDEX_SIZE_HD,
				POLYS_HD,
				VERTICES_LOD,
				VERTEX_SIZE_LOD,
				INDICES_LOD,
				INDEX_SIZE_LOD,
				POLYS_LOD,
				VERTICES_SLOD1,
				VERTEX_SIZE_SLOD1,
				INDICES_SLOD1,
				INDEX_SIZE_SLOD1,
				POLYS_SLOD1,
				VERTICES_SLOD2,
				VERTEX_SIZE_SLOD2,
				INDICES_SLOD2,
				INDEX_SIZE_SLOD2,
				POLYS_SLOD2,
				VERTICES_SLOD3,
				VERTEX_SIZE_SLOD3,
				INDICES_SLOD3,
				INDEX_SIZE_SLOD3,
				POLYS_SLOD3,
				VERTICES_SLOD4,
				VERTEX_SIZE_SLOD4,
				INDICES_SLOD4,
				INDEX_SIZE_SLOD4,
				POLYS_SLOD4,
				SIZE_NUM
			};
			u32 virtSize[SIZE_NUM] = {0};
			u32 physSize[SIZE_NUM] = {0};

			const atArray<strIndex>& ignoreList = gPopStreaming.GetResidentObjectList();
			GetObjectAndDependenciesSplitSizes(pEntity, pEntity->GetModelId(), ignoreList.GetElements(), ignoreList.GetCount(),
				virtSize[GEOMETRY], physSize[GEOMETRY], virtSize[TEXTURE], physSize[TEXTURE], virtSize[CLIP], physSize[CLIP],
				virtSize[FRAG], physSize[FRAG], virtSize[SUBTOTAL], physSize[SUBTOTAL]);

			// get lod mesh data
			bool isVehHd = false;
			bool isPedHd = false;

			if(pEntity->GetIsTypePed())
			{
				const CPedModelInfo* pPedModelInfo = static_cast<const CPedModelInfo*>(pEntity->GetBaseModelInfo());
				const CPed* pPed = static_cast<const CPed*>(pEntity);
				if (pPedModelInfo->GetIsStreamedGfx())
				{
					const CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetConstPedRenderGfx();
					if (pGfxData)
					{
						for(int lodLevel=0;lodLevel<rage::LOD_COUNT;lodLevel++)
						{
							u32 off = lodLevel * 5;
							CPedVariationDebug::GetPolyCountPlayerInternal(pGfxData, &virtSize[POLYS_HD + off], lodLevel, &virtSize[VERTICES_HD + off], &virtSize[VERTEX_SIZE_HD + off], &virtSize[INDICES_HD + off], &virtSize[INDEX_SIZE_HD + off]);
						}
					}
				} 
				else 
				{
					for(int lodLevel=0;lodLevel<rage::LOD_COUNT;lodLevel++)
					{
						u32 off = lodLevel * 5;
						CPedVariationDebug::GetPolyCountPedInternal(pPedModelInfo, pPed->GetPedDrawHandler().GetVarData(), &virtSize[POLYS_HD + off], lodLevel, &virtSize[VERTICES_HD + off], &virtSize[VERTEX_SIZE_HD + off], &virtSize[INDICES_HD + off], &virtSize[INDEX_SIZE_HD + off]);
					}
				}	

				if (pPedModelInfo->GetAreHDFilesLoaded())
				{
					isPedHd = true;
				}

			}
			else
			{
				if(pEntity->GetBaseModelInfo()->GetFragType())
				{
					// hacky way to flag vehicles as HD lods
					if (pEntity->GetIsTypeVehicle())
					{
						CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pEntity->GetBaseModelInfo());
						if (pVMI->GetAreHDFilesLoaded())
						{
							isVehHd = true;
						}
					}
					for(int lodLevel=0;lodLevel<rage::LOD_COUNT;lodLevel++)
					{
						u32 off = lodLevel * 5;
						ColorizerUtils::GetFragInfo(pEntity, pEntity->GetBaseModelInfo(), &virtSize[POLYS_HD + off], &virtSize[VERTICES_HD + off], &virtSize[VERTEX_SIZE_HD + off], &virtSize[INDICES_HD + off], &virtSize[INDEX_SIZE_HD + off], lodLevel);
					}
				}
				else
				{
					rmcDrawable* pDrawable = pEntity->GetDrawable();
					if(pDrawable)
					{
						for(int lodLevel=0;lodLevel<rage::LOD_COUNT;lodLevel++)
						{
							u32 off = lodLevel * 5;
							ColorizerUtils::GetDrawableInfo(pDrawable, &virtSize[POLYS_HD + off], &virtSize[VERTICES_HD + off], &virtSize[VERTEX_SIZE_HD + off], &virtSize[INDICES_HD + off], &virtSize[INDEX_SIZE_HD + off], lodLevel);
						}
					}
				}
			}

			// add hd cost for vehicles and peds
			if (isVehHd)
			{
				Assert(pEntity->GetIsTypeVehicle());
				CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pEntity->GetBaseModelInfo());
				strIndex hdFragIndex = g_FragmentStore.GetStreamingModule()->GetStreamingIndex(strLocalIndex(pVMI->GetHDFragmentIndex()));

				s32 itemVirtSize = strStreamingEngine::GetInfo().GetObjectVirtualSize(hdFragIndex, true);
				s32 itemPhysSize = strStreamingEngine::GetInfo().GetObjectPhysicalSize(hdFragIndex, true);
				virtSize[FRAG] += itemVirtSize;
				physSize[FRAG] += itemPhysSize;
			}
			if (isPedHd)
			{
				Assert(pEntity->GetIsTypePed());
				CPedModelInfo* modelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
				strIndex hdTxdIndex = g_TxdStore.GetStreamingModule()->GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));

				s32 itemVirtSize = strStreamingEngine::GetInfo().GetObjectVirtualSize(hdTxdIndex, true);
				s32 itemPhysSize = strStreamingEngine::GetInfo().GetObjectPhysicalSize(hdTxdIndex, true);
				virtSize[TEXTURE] += itemVirtSize;
				physSize[TEXTURE] += itemPhysSize;
			}

			// display results
			char buf[256] = {0};
			if (col == 0)
			{
				if (row == 0)	
				{
					formatf(buf, "%s (%s)%s", pEntity->GetModelName(), pEntity->GetType() >= ENTITY_TYPE_TOTAL ? "unknown" : entityTypeStrings[pEntity->GetType()], (isVehHd || isPedHd) ? " (HD)" : "");
					pResult->SetString(buf);
				}
				else
				{
					pResult->SetString(inspectorEntries[row]);
				}
			}
			else
			{
				if (row == 0)	
				{
					formatf(buf, "Main Ram\t Video Ram\t Total Cost");
					pResult->SetString(buf);
				}
				else
				{
					switch (row)
					{
					case 1:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[GEOMETRY] / 1024.f, physSize[GEOMETRY] / 1024.f, (virtSize[GEOMETRY] + physSize[GEOMETRY]) / 1024.f);
						break;
					case 2:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[TEXTURE] / 1024.f, physSize[TEXTURE] / 1024.f, (virtSize[TEXTURE] + physSize[TEXTURE]) / 1024.f);
						break;
					case 3:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[CLIP] / 1024.f, physSize[CLIP] / 1024.f, (virtSize[CLIP] + physSize[CLIP]) / 1024.f);
						break;
					case 4:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[FRAG] / 1024.f, physSize[FRAG] / 1024.f, (virtSize[FRAG] + physSize[FRAG]) / 1024.f);
						break;
					case 5:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[SUBTOTAL] / 1024.f, physSize[SUBTOTAL] / 1024.f, (virtSize[SUBTOTAL] + physSize[SUBTOTAL]) / 1024.f);
						break;
					case 6:
						break;
					case 7:
#if __PS3
						formatf(buf, "Count\t\t\t\t Cost (decompressed)");
#else
						formatf(buf, "Count\t\t\t\t Cost");
#endif
						break;
					case 8:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[VERTICES_HD], virtSize[VERTEX_SIZE_HD] / 1024.f);
						break;
					case 9:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[INDICES_HD], virtSize[INDEX_SIZE_HD] / 1024.f);
						break;
					case 10:
						formatf(buf, "%06d", virtSize[POLYS_HD]);
						break;
					case 11:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[VERTICES_LOD], virtSize[VERTEX_SIZE_LOD] / 1024.f);
						break;
					case 12:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[INDICES_LOD], virtSize[INDEX_SIZE_LOD] / 1024.f);
						break;
					case 13:
						formatf(buf, "%06d", virtSize[POLYS_LOD]);
						break;
					case 14:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[VERTICES_SLOD1], virtSize[VERTEX_SIZE_SLOD1] / 1024.f);
						break;
					case 15:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[INDICES_SLOD1], virtSize[INDEX_SIZE_SLOD1] / 1024.f);
						break;
					case 16:
						formatf(buf, "%06d", virtSize[POLYS_SLOD1]);
						break;
					case 17:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[VERTICES_SLOD2], virtSize[VERTEX_SIZE_SLOD2] / 1024.f);
						break;
					case 18:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[INDICES_SLOD2], virtSize[INDEX_SIZE_SLOD2] / 1024.f);
						break;
					case 19:
						formatf(buf, "%06d", virtSize[POLYS_SLOD2]);
						break;
					case 20:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[VERTICES_SLOD3], virtSize[VERTEX_SIZE_SLOD3] / 1024.f);
						break;
					case 21:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[INDICES_SLOD3], virtSize[INDEX_SIZE_SLOD3] / 1024.f);
						break;
					case 22:
						formatf(buf, "%06d", virtSize[POLYS_SLOD3]);
						break;
					case 23:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[VERTICES_SLOD4], virtSize[VERTEX_SIZE_SLOD4] / 1024.f);
						break;
					case 24:
						formatf(buf, "%06d\t\t\t %07.2fk", virtSize[INDICES_SLOD4], virtSize[INDEX_SIZE_SLOD4] / 1024.f);
						break;
					case 25:
						formatf(buf, "%06d", virtSize[POLYS_SLOD4]);
						break;
					default:
						formatf(buf, "wrong number of rows", row);
					}
					pResult->SetString(buf);
				}
			}
		}
	}
}

void CGtaPickerInterface::DetailedEntityResultsCB(CUiGadgetText* pResult, u32 row, u32 col )
{
	if (pResult && row < EntityDetails.GetCount())
	{
		fwEntity* fwEntityPtr = g_PickerManager.GetSelectedEntity();
		CEntity* pEntity = fwEntityPtr ? dynamic_cast<CEntity*>(fwEntityPtr) : NULL;

		if (pEntity)
		{
			if (col == 0)
			{
				pResult->SetString(EntityDetails[row].GetTitle());
			}
			else
			{
				atString ValueString("");
				EntityDetails[row].GetValue(pEntity, ValueString);
				pResult->SetString(ValueString.c_str());
			}
		}
	}
}

bool CGtaPickerInterface::DoesUiHaveMouseFocus()
{
	return g_UiGadgetRoot.ContainsMouse();
}

bool CGtaPickerInterface::AllowMultipleSelection()
{
	if (CControlMgr::GetKeyboard().GetKeyDown(KEY_LCONTROL, KEYBOARD_MODE_DEBUG, "CGtaPickerInterface::AllowMultipleSelection"))
	{
		return true;
	}

	return false;
}

void CGtaPickerInterface::Update()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_BACK, KEYBOARD_MODE_DEBUG_SHIFT, "Hide selected IMAP files"))
	{
		atArray<s32> imapSlots;

		for (s32 i=0; i<g_PickerManager.GetNumberOfEntities(); i++)
		{
			CEntity* pEntity = (CEntity*) g_PickerManager.GetEntity(i);
			if (pEntity)
			{
				s32 imapIndex = pEntity->GetIplIndex();
				if (imapIndex)
				{
					imapSlots.PushAndGrow(imapIndex);
				}
			}
		}

		fwMapDataDebug::SuppressSelected(imapSlots);
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SLASH, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Cycle show/hide mode in Picker"))
	{
		if (m_Settings.m_ShowHideMode == PICKER_ISOLATE_SELECTED_ENTITY ||
			m_Settings.m_ShowHideMode == PICKER_ISOLATE_SELECTED_ENTITY_NOALPHA ||
			m_Settings.m_ShowHideMode == PICKER_ISOLATE_ENTITIES_IN_LIST)
		{
			m_Settings.m_ShowHideMode = PICKER_HIDE_ENTITIES_IN_LIST;
		}
		else
		{
			m_Settings.m_ShowHideMode = PICKER_ISOLATE_ENTITIES_IN_LIST;
		}
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SLASH, KEYBOARD_MODE_DEBUG_ALT, "Toggle show/hide enabled in Picker")
		|| (ioMouse::GetReleasedButtons()&ioMouse::MOUSE_RIGHT && !CDebugScene::ms_bEnableFocusEntityDragging) )
	{
		if (g_PickerManager.GetSelectedEntity() != NULL)
		{
			m_Settings.m_bShowHideEnabled = !m_Settings.m_bShowHideEnabled;
		}
	}
	if (m_Settings.m_bShowHideEnabled && g_PickerManager.GetSelectedEntity()==NULL)
	{
		m_Settings.m_bShowHideEnabled = false;
	}

	if ( (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SLASH, KEYBOARD_MODE_DEBUG_SHIFT, "Enable Default Picker (in Picker bank in Rag)"))
		||
		(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SLASH, KEYBOARD_MODE_DEBUG, "Enable Default Picker (in Picker bank in Rag)")) )
	{
		g_PickerManager.SetUiEnabled(true);
		if (g_PickerManager.IsEnabled())
		{
			// detach previous child
			if (m_activeInspector >= 0 && m_activeInspector < m_inspectorList.GetCount())
			{
				if (m_inspectorList[m_activeInspector]->GetParent())
				{
					m_pResultsWindow->DetachChild(m_inspectorList[m_activeInspector]);
				}
			}

			m_activeInspector++;

			if (m_activeInspector >= m_inspectorList.GetCount())
			{
				g_PickerManager.SetEnabled(false);
				m_activeInspector = -1;
			}
			else
			{
				OnDisplayResultsWindow();
			}
		}
		else
		{
			m_activeInspector = -1;

			EnablePickerWithDefaultSettings();
		}
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SLASH, KEYBOARD_MODE_DEBUG_CTRL, "Enable Default Picker (in Picker bank in Rag)"))
	{
		g_PickerManager.SetUiEnabled(true);
		if (g_PickerManager.IsEnabled())
		{
			bool bInspectorHasJustBeenDetached = false;
			// detach previous child
			if (m_activeInspector >= 0 && m_activeInspector < m_inspectorList.GetCount())
			{
				if (m_inspectorList[m_activeInspector]->GetParent())
				{
					m_pResultsWindow->DetachChild(m_inspectorList[m_activeInspector]);
					bInspectorHasJustBeenDetached = true;
				}
			}

			m_activeInspector--;

			if (m_activeInspector < 0)
			{
				m_activeInspector = -1;

				if (bInspectorHasJustBeenDetached)
				{	//	If we've just removed the first inspector in m_inspectorList then display the results window with no inspector
					OnDisplayResultsWindow();
				}
				else
				{	//	If we've just moved "back" from the display with no inspector then remove the results window
					g_PickerManager.SetEnabled(false);
				}
			}
			else
			{
				OnDisplayResultsWindow();
			}
		}
		else
		{
			m_activeInspector = m_inspectorList.GetCount() - 1;

			EnablePickerWithDefaultSettings();
		}
	}

	if (m_bResetWindowPos || m_bShowWindowEvent)
	{
		HideUIGadgets();
		ShowUIGadgets();
		m_bShowWindowEvent = false;
	}
	else if (!g_PickerManager.IsUiEnabled() || m_bHideWindowEvent)
	{
		HideUIGadgets();
		m_bHideWindowEvent = false;
	}

	if (m_pResultsWindow)
	{
		g_PickerManager.SetIndexOfSelectedEntity(m_pResultsWindow->GetCurrentIndex());
	}
#if DEBUG_ENTITY_GPU_COST
	if(m_pSelectedEntityGPUCostPicker != g_PickerManager.GetSelectedEntity())
	{
		m_pSelectedEntityGPUCostPicker = g_PickerManager.GetSelectedEntity();
		gDrawListMgr->ResetGPUCostTimers();
	}
#endif

	if (g_PickerManager.IsEnabled())
	{
		CEntity* pSelectedEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
		CEntity* pHoverEntity = static_cast<CEntity*>(g_PickerManager.GetHoverEntity());

		if (m_Settings.m_bDisplayBoundsOfHoverEntity && pHoverEntity)
		{
			CDebugScene::DrawEntityBoundingBox(pHoverEntity, Color32(0, 0xff, 0, m_Settings.m_BoundingBoxOpacity), m_Settings.m_bDisplayBoundsInWorldspace, m_Settings.m_bDisplayBoundsOfGeometries);
		}

		if (m_Settings.m_bDisplayBoundsOfSelectedEntity && pSelectedEntity && m_pResultsWindow)
		{
			grcDebugDraw::Axis(pSelectedEntity->GetMatrix(), 1.0f);
			CDebugScene::DrawEntityBoundingBox(pSelectedEntity, Color32(0xff, 0, 0, m_Settings.m_BoundingBoxOpacity), m_Settings.m_bDisplayBoundsInWorldspace, m_Settings.m_bDisplayBoundsOfGeometries);
		}

		if (m_Settings.m_bDisplayBoundsOfEntitiesInList && m_pResultsWindow)
		{
			for (int i = 0; i < g_PickerManager.GetNumberOfEntities(); i++)
			{
				if (i != g_PickerManager.GetIndexOfSelectedEntity())
				{
					CEntity *pEntity = (CEntity*)g_PickerManager.GetEntity(i);
					if (pEntity)
					{
						CDebugScene::DrawEntityBoundingBox(pEntity, Color32(0xff, 0x55, 0x55, m_Settings.m_BoundingBoxOpacity), m_Settings.m_bDisplayBoundsInWorldspace, m_Settings.m_bDisplayBoundsOfGeometries);
					}
				}
			}
		}

		// draw the approximated AABB used for visibility testing - handy for tracking down visibility bugs
		if (m_Settings.m_bDisplayVisibilityBoundsForSelectedEntity && pSelectedEntity && m_pResultsWindow)
		{
			const Color32 colour(0, 0, 0xff, m_Settings.m_BoundingBoxOpacity);
			const fwkDOP18* kDOP = NULL;

			if (m_Settings.m_bDisplayVisibilitykDOPTest && pSelectedEntity->GetIsTypeBuilding())
			{
				kDOP = fwkDOP18::GetCachedFromDrawable(pSelectedEntity, pSelectedEntity->GetDrawable());
			}

			if (kDOP)
			{
				const grcViewport& vp = *gVpMan.GetUpdateGameGrcViewport();

				kDOP->DebugDrawEdges(colour);
				kDOP->DebugDrawBound(Color32(0xff, 0xff, 0, m_Settings.m_BoundingBoxOpacity), vp, false);
				kDOP->DebugDrawBound(Color32(0xff, 0xff, 0, m_Settings.m_BoundingBoxOpacity), vp, true);
			}
			else
			{
				spdAABB entityDescAABB;
				fwBaseEntityContainer* pContainer =  !pSelectedEntity->GetIsRetainedByInteriorProxy() ? pSelectedEntity->GetOwnerEntityContainer() : NULL;
				if (pContainer)
				{
					u32 index = pContainer->GetEntityDescIndex(pSelectedEntity);
					if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
					{
						pContainer->GetApproxBoundingBox(index, entityDescAABB);
						grcDebugDraw::BoxAxisAligned(entityDescAABB.GetMin(), entityDescAABB.GetMax(), colour, false);
					}
				}
			}
		}

		if (m_Settings.m_bDisplayPhysicalBoundsForNodeOfSelectedEntity && pSelectedEntity && m_pResultsWindow)
		{
			const Color32 colour(0, 0, 0xff, m_Settings.m_BoundingBoxOpacity);

			CEntity *pEntity = (CEntity *)g_PickerManager.GetSelectedEntity();
			if (pEntity->GetIplIndex())
			{
				fwMapDataContents* pContents = fwMapDataStore::GetStore().Get(strLocalIndex(pEntity->GetIplIndex()));
				if (pContents)
				{
					if (pContents->GetSceneGraphNode()->GetType() == SCENE_GRAPH_NODE_TYPE_STREAMED)
					{
						const spdAABB& aabb = static_cast< fwStreamedSceneGraphNode* >( pContents->GetSceneGraphNode() )->GetBoundingBox();
						grcDebugDraw::BoxAxisAligned(aabb.GetMin(), aabb.GetMax(), colour, false);
					}
				}
			}

		}

		if (m_Settings.m_bDisplayStreamingBoundsForNodeOfSelectedEntity && pSelectedEntity && m_pResultsWindow)
		{
			const Color32 colour(0xff, 0, 0xff, m_Settings.m_BoundingBoxOpacity);

			if (pSelectedEntity && pSelectedEntity->GetIplIndex())
			{
				const spdAABB& aabb = fwMapDataStore::GetStore().GetStreamingBounds( strLocalIndex(pSelectedEntity->GetIplIndex()) );
				grcDebugDraw::BoxAxisAligned(aabb.GetMin(), aabb.GetMax(), colour, false);
			}

		}

		if (m_Settings.m_bDisplayVisibilitySphereForSelectedEntity && pSelectedEntity && m_pResultsWindow && !pSelectedEntity->GetIsRetainedByInteriorProxy())
		{
			fwBaseEntityContainer* pContainer = pSelectedEntity->GetOwnerEntityContainer();
			if (pContainer)
			{
				u32 index = pContainer->GetEntityDescIndex(pSelectedEntity);
				if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
				{
					spdSphere sphere;
					pContainer->GetApproxBoundingSphere(index, sphere);
					grcDebugDraw::Sphere(sphere.GetCenter(), sphere.GetRadiusf(), Color32(255,0,0), false, 1, 64);
				}
			}
		}

		if (m_Settings.m_bDisplayScreenQuadForSelectedEntity && pSelectedEntity && m_pResultsWindow && !pSelectedEntity->GetIsRetainedByInteriorProxy())
		{
			fwBaseEntityContainer* pContainer = pSelectedEntity->GetOwnerEntityContainer();
			if (pContainer)
			{
				fwSceneGraphNode* pSceneNode = pContainer->GetOwnerSceneNode();

				if ( fwScan::GetGbufUsesScreenQuadPairs() )
					fwScan::GetGbufScreenQuadPair( pSceneNode ).DebugDraw( Color_magenta, Color_magenta );
				else
					fwScan::GetGbufScreenQuad( pSceneNode ).DebugDraw( Color_magenta );
			}
		}
		if ( m_Settings.m_bDisplayOcclusionScreenQuadForSelectedEntity && pSelectedEntity && m_pResultsWindow && !pSelectedEntity->GetIsRetainedByInteriorProxy())
		{
			fwBaseEntityContainer* pContainer = pSelectedEntity->GetOwnerEntityContainer();
			if (pContainer)
			{
				spdAABB entityDescAABB;
				u32 index = pContainer->GetEntityDescIndex(pSelectedEntity);
				if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
				{
					pContainer->GetApproxBoundingBox(index, entityDescAABB);
					
					Color32 col = COcclusion::IsBoxVisibleFast(entityDescAABB) ? Color_magenta : Color_WhiteSmoke;
					Vec3V hullPoints[8];
					u8* indices=0;
					int numPoints = rstGetBoxHull(  gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition(),
										entityDescAABB.GetMin(), entityDescAABB.GetMax(),  indices, hullPoints);
					
					for(int i=0;i<numPoints;i++){
						grcDebugDraw::Line( hullPoints[indices[i]], hullPoints[indices[(i+1)%numPoints]], col);
					}
				}
			}
		}

		CDebugModelAnalysisInterface::Update(pSelectedEntity);
	}

	UpdateLodSettings();

#if ENTITY_GPU_TIME
	dlCmdEntityGPUTimePush::SetEntity(sm_bShowEntityGPUTime ? g_PickerManager.GetSelectedEntity() : NULL);
#endif // ENTITY_GPU_TIME
}

void CGtaPickerInterface::OutputResults()
{
	if (m_pResultsWindow)
	{
		m_pResultsWindow->SetNumEntries(g_PickerManager.GetNumberOfEntities());
		m_pResultsWindow->SetCurrentIndex(g_PickerManager.GetIndexOfSelectedEntity());
		m_pResultsWindow->RequestSetViewportToShowSelected();
	}

/*
	for (u32 ResultsLoop = 0; ResultsLoop < ResultsArray.GetCount(); ResultsLoop++)
	{
		Printf("%s\n", GetEntityDetailsString(ResultsArray[ResultsLoop]));
	}

	Printf("Number of Selected Entities = %d\n", ResultsArray.GetCount());
*/
}

void CGtaPickerInterface::PickerHasNewController()
{
	if (m_pResultsWindow)
	{
		m_pResultsWindow->UpdateTitle(g_PickerManager.GetOwnerName());
	}
}

ePickerShowHideMode CGtaPickerInterface::GetShowHideMode()
{
	const bool bShowOnlySelectedOnRightMouseDown = false; // we could expose this as a flag to the settings

	if (bShowOnlySelectedOnRightMouseDown && (ioMouse::GetButtons()&ioMouse::MOUSE_RIGHT) != 0)
	{
		return PICKER_ISOLATE_SELECTED_ENTITY;
	}
	else if (m_Settings.m_bShowHideEnabled)
	{
		return m_Settings.m_ShowHideMode;
	}

	return PICKER_SHOW_ALL;
}

char *CGtaPickerInterface::GetEntityDetailsString(const fwEntity *pEntity)
{
	static char debugText[250];
	char prefix[64] = "";

	strcpy(debugText, "");

	if (!pEntity){
		return(debugText);
	}

	if (((CEntity*)pEntity)->GetIsTypeLight()) // light entities are always "LightObject", that's boring .. show the parent
	{
		CLightEntity* pLightEntity = (CLightEntity*)pEntity;
		const char* effectTypeStr = "UNKNOWN";

		switch ((int)pLightEntity->Get2dEffectType())
		{
		case ET_LIGHT       : effectTypeStr = "LIGHT"      ; break;
		case ET_LIGHT_SHAFT : effectTypeStr = "LIGHT_SHAFT"; break;
		}

		sprintf(prefix, "%s[%d] - ", effectTypeStr, pLightEntity->Get2dEffectId());
		pEntity = pLightEntity->GetParent();
	}

	if (pEntity)
	{
		if (pEntity->IsArchetypeSet())
		{
			sprintf(debugText, "%s%s", prefix, pEntity->GetArchetype()->GetModelName());
		}
		else
		{
			sprintf(debugText, "%sUnknown model", prefix);
		}
	}

	return debugText;
}

void CGtaPickerInterface::SetPickerSettings(fwPickerInterfaceSettings *pNewSettings)
{
	if (Verifyf(pNewSettings, "CGtaPickerInterface::SetPickerSettings - NewSettings pointer is NULL"))
	{
		fwPickerInterface::SetPickerSettings(pNewSettings);

		CGtaPickerInterfaceSettings *pNewGtaSettings = dynamic_cast<CGtaPickerInterfaceSettings*>(pNewSettings);
		if (Verifyf(pNewGtaSettings, "CGtaPickerInterface::SetPickerSettings - the supplied settings are not GTAPicker interface settings"))
		{
			m_Settings = *pNewGtaSettings;
		}
	}
}

// TODO -- move to Entity.cpp?
bool CGtaPickerInterface::IsValidEntityPtr(fwEntity* pToTest)
{
	if (CBuilding        ::GetPool()->IsValidPtr( (CBuilding        *) pToTest)) { return true; } // ENTITY_TYPE_BUILDING
	if (CAnimatedBuilding::GetPool()->IsValidPtr( (CAnimatedBuilding*) pToTest)) { return true; } // ENTITY_TYPE_ANIMATED_BUILDING
	if (CVehicle         ::GetPool()->IsValidPtr( (CVehicle         *) pToTest)) { return true; } // ENTITY_TYPE_VEHICLE
	if (CPed             ::GetPool()->IsValidPtr( (CPed             *) pToTest)) { return true; } // ENTITY_TYPE_PED
	if (CObject          ::GetPool()->IsValidPtr( (CObject          *) pToTest)) { return true; } // ENTITY_TYPE_OBJECT
	if (CDummyObject     ::GetPool()->IsValidPtr( (CDummyObject     *) pToTest)) { return true; } // ENTITY_TYPE_DUMMY_OBJECT
	if (CPortalInst      ::GetPool()->IsValidPtr( (CPortalInst      *) pToTest)) { return true; } // ENTITY_TYPE_PORTAL
	if (CInteriorInst    ::GetPool()->IsValidPtr( (CInteriorInst    *) pToTest)) { return true; } // ENTITY_TYPE_MLO
	// ENTITY_TYPE_NOTINPOOLS
	// ENTITY_TYPE_PARTICLESYSTEM
	if (CLightEntity     ::GetPool()->IsValidPtr( (CLightEntity     *) pToTest)) { return true; } // ENTITY_TYPE_LIGHT
	if (CCompEntity      ::GetPool()->IsValidPtr( (CCompEntity      *) pToTest)) { return true; } // ENTITY_TYPE_COMPOSITE
	if (CEntityBatch     ::GetPool()->IsValidPtr( (CEntityBatch     *) pToTest)) { return true; } // ENTITY_TYPE_INSTANCE_LIST
	if (CGrassBatch      ::GetPool()->IsValidPtr( (CGrassBatch      *) pToTest)) { return true; } // ENTITY_TYPE_GRASS_INSTANCE_LIST
	if (CPickup          ::GetPool()->IsValidPtr( (CPickup          *) pToTest)) { return true; } // ENTITY_TYPE_OBJECT_PICKUP (ENTITY_TYPE_OBJECT)

	return false;
}

#endif //__BANK
