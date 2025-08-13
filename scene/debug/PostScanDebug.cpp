/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneIsolator.cpp
// PURPOSE : debug processing on PVS list after visibility scan
// AUTHOR :  Ian Kiigan
// CREATED : 30/07/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/PostScanDebug.h"

SCENE_OPTIMISATIONS();

#if __BANK

#include "grcore/debugdraw.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/world/EntityDesc.h"
#include "fwdebug/picker.h"
#include "bank/bank.h" 
#include "bank/bkmgr.h"
#include "profile/page.h"
#include "profile/group.h"
#include "profile/element.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/BlockView.h"
#include "debug/DebugScene.h"
#include "debug/Rendering/DebugLighting.h"
#include "fwsys/timer.h"
#include "game/Clock.h"
#include "input/keys.h"
#include "objects/object.h"
#include "objects/DummyObject.h"
#include "objects/objectpopulation.h"
#include "scene/debug/MapOptimizeHelper.h"
#include "scene/debug/SceneCostTracker.h"
#include "scene/debug/SceneIsolator.h"
#include "scene/debug/EntityReport.h"
#include "scene/entity.h"
#include "scene/FocusEntity.h"
#include "scene/lod/LodMgr.h"
#include "scene/lod/LodDebug.h"
#include "scene/MapChange.h"
#include "scene/scene_channel.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameScan.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "script/commands_misc.h"
#include "renderer/Renderer.h"
#include "renderer/occlusion.h"
#include "renderer/PostScan.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#include "renderer/RenderPhases/RenderPhaseParaboloidShadows.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"
#include "renderer/sprite2d.h"
#include "renderer/TreeImposters.h"
#include "shaders/CustomShaderEffectBase.h"
#include "physics/physics.h"
#include "breakableglass/glassmanager.h"
#include "spatialdata/sphere.h"
#include "system/controlMgr.h"
#include "Peds/ped.h"
#include "fwscene/world/SceneGraphVisitor.h"
#include "fwscene/world/WorldRepMulti.h"
#include "scene/loader/MapData_Interiors.h"
#include "modelinfo/MloModelInfo.h"
#include "streaming/streamingdebuggraph.h"

#include "TimeCycle/TimeCycle.h"
#include "fwscene/scan/ScanNodesdebug.h"
#include "renderer/RenderListBuilder.h"
#include "system/timemgr.h"

#define DEFAULT_CULLBOX_W (100)
#define DEFAULT_CULLBOX_H (100)
#define DEFAULT_CULLBOX_D (500)

PF_PAGE( VisibilityResults, "Visibility Results" );

PF_GROUP( VisibilityResults_CascadeShadows );
PF_LINK( VisibilityResults, VisibilityResults_CascadeShadows );
PF_COUNTER( Cascade0Hits, VisibilityResults_CascadeShadows );
PF_COUNTER( Cascade1Hits, VisibilityResults_CascadeShadows );
PF_COUNTER( Cascade2Hits, VisibilityResults_CascadeShadows );
PF_COUNTER( Cascade3Hits, VisibilityResults_CascadeShadows );
PF_COUNTER( CascadeTotalHits, VisibilityResults_CascadeShadows );

PF_GROUP( VisibilityResults_Reflections );
PF_LINK( VisibilityResults, VisibilityResults_Reflections );
PF_COUNTER( Reflection0Hits, VisibilityResults_Reflections );
PF_COUNTER( Reflection1Hits, VisibilityResults_Reflections );
PF_COUNTER( Reflection2Hits, VisibilityResults_Reflections );
PF_COUNTER( Reflection3Hits, VisibilityResults_Reflections );
PF_COUNTER( Reflection4Hits, VisibilityResults_Reflections );
PF_COUNTER( Reflection5Hits, VisibilityResults_Reflections );
PF_COUNTER( ReflectionTotalHits, VisibilityResults_Reflections );


bool	CPostScanDebug::ms_bTimedModelInfo = false;
s32		CPostScanDebug::ms_exteriorCullSphereRadius = 1;
bool	CPostScanDebug::ms_bEnableExteriorHdCullSphere = false;
bool	CPostScanDebug::ms_bEnableExteriorHdModelCull = false;
static spdSphere s_cullSphere;

bool s_DisableOcclusion = false;

s32		CPostScanDebug::ms_modelSwapRadius = 10;
s32		CPostScanDebug::ms_modelSwapIndex = -1;
bool	CPostScanDebug::ms_bDisplayModelSwaps				   = false;
bool	CPostScanDebug::ms_bLazySwap						   = false;
bool	CPostScanDebug::ms_bIncludeScriptObjects			   = true;
bool	CPostScanDebug::ms_bForceUpAlpha                       = false;
bool	CPostScanDebug::ms_bCountModelsRendered                = false;
bool	CPostScanDebug::ms_bCountShadersRendered               = false;
int		CPostScanDebug::ms_CountModelsRenderedDrawList         = 0; // 0="All"
bool	CPostScanDebug::ms_bCountModelsRenderedShowCascaShad   = true;
bool	CPostScanDebug::ms_bCountModelsRenderedShowParabShad   = true;
bool	CPostScanDebug::ms_bCountModelsRenderedShowReflections = true;
bool	CPostScanDebug::ms_bCountModelsRenderedShowBreakdown   = false;
bool	CPostScanDebug::ms_bCountModelsRenderedShowPreRender   = false;
bool	CPostScanDebug::ms_bCountModelsRenderedShowStreaming   = true;
bool	CPostScanDebug::ms_bCountModelsRendered_PVS            = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_VISIBLE  = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_LOD      = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_CUTOUT   = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_DECAL    = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_FADING   = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_ALPHA    = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_WATER    = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_TREE     = true;
bool	CPostScanDebug::ms_bCountModelsRendered_RPASS_IMPOSTER = true;

bool	CPostScanDebug::ms_bOverrideGBufBitsEnabled		= false;
int		CPostScanDebug::ms_nOverrideGBufBitsPhase		= 0;
int		CPostScanDebug::ms_nOverrideGBufVisibilityType	= 0;
bool	CPostScanDebug::ms_bOverrideSubBucketMaskOnly	= false;

bool	CPostScanDebug::ms_bScreenSizeCullEnabled	= false;
bool	CPostScanDebug::ms_bCullNonFragTypes		= false;
bool	CPostScanDebug::ms_bCullFragTypes			= false;
u32		CPostScanDebug::ms_nScreenSizeCullMaxW		= 20;
u32		CPostScanDebug::ms_nScreenSizeCullMaxH		= 20;
bool	CPostScanDebug::ms_bScreenSizeCullShowGuide	= false;
bool	CPostScanDebug::ms_bScreenSizeCullReversed	= false;

bool	CPostScanDebug::ms_bOnlyDrawLodTrees		= false;
bool	CPostScanDebug::ms_bOnlyDrawSelectedLodTree	= false;
bool	CPostScanDebug::ms_bDrawLodRanges			= false;
bool	CPostScanDebug::ms_bDrawByLodLevel			= false;
bool	CPostScanDebug::ms_bStripLodLevel			= false;
bool	CPostScanDebug::ms_bStripAllExceptLodLevel	= false;
s32		CPostScanDebug::ms_nSelectedLodLevel		= 0;
bool	CPostScanDebug::ms_bCullIfParented			= false;
bool	CPostScanDebug::ms_bCullIfHasOneChild		= false;

bool	CPostScanDebug::ms_bBoxCull					= false;
bool	CPostScanDebug::ms_bShowEntityCount			= false;
bool	CPostScanDebug::ms_bShowOnlySelectedType    = false;
s32		CPostScanDebug::ms_nCullBoxX				= 0;
s32		CPostScanDebug::ms_nCullBoxY				= 0;
s32		CPostScanDebug::ms_nCullBoxZ				= 0;
s32		CPostScanDebug::ms_nCullBoxW				= 0;
s32		CPostScanDebug::ms_nCullBoxH				= 0;
s32		CPostScanDebug::ms_nCullBoxD				= 0;
s32		CPostScanDebug::ms_nCullBoxSelectedPolicy	= 0;

bool	CPostScanDebug::ms_bTrackSelectedEntity			= false;

bool	CPostScanDebug::ms_bDisplayObjects				= false;
bool	CPostScanDebug::ms_bDisplayDummyObjects			= false;
bool	CPostScanDebug::ms_bDisplayObjectPoolSummary	= false;
bool	CPostScanDebug::ms_bDisplayDummyDebugOutput		= false;
bool	CPostScanDebug::ms_bEnableForceObj				= false;
bool	CPostScanDebug::ms_bDrawForceObjSphere			= false;
Vec3V	CPostScanDebug::ms_vForceObjPos;
float	CPostScanDebug::ms_fForceObjRadius				= 1.0f;
float	CPostScanDebug::ms_fClearAreaObjRadius			= 100.0f;
s32		CPostScanDebug::ms_ClearAreaFlag				= 0;

bool	CPostScanDebug::ms_bDoAlphaUpdate				= true;
u8		CPostScanDebug::ms_userAlpha					= 0;
bool	CPostScanDebug::ms_bTweakAlpha					= false;
bool	CPostScanDebug::ms_PartitionPvsInsteadOfSorting	= true;
bool	CPostScanDebug::ms_VerifyPvsSort				= false;
bool	CPostScanDebug::ms_bCollectEntityStats			= false;
bool	CPostScanDebug::ms_bDrawEntityStats				= false;
bool	CPostScanDebug::ms_bDisplayAlwaysPreRenderList	= false;

bool	CPostScanDebug::ms_bAdjustLodData				= false;
bool	CPostScanDebug::ms_bAdjustLodData_LoadedFlag	= false;
u8		CPostScanDebug::ms_adjustLodData_Alpha			= 0;

bool	CPostScanDebug::ms_bTogglePlayerPedVisibility 	= false;

bool	CPostScanDebug::ms_bCullLoddedHd				= false;
bool	CPostScanDebug::ms_bCullOrphanHd				= false;
bool	CPostScanDebug::ms_bCullLod						= false;
bool	CPostScanDebug::ms_bCullSlod1					= false;
bool	CPostScanDebug::ms_bCullSlod2					= false;
bool	CPostScanDebug::ms_bCullSlod3					= false;
bool	CPostScanDebug::ms_bCullSlod4					= false;

bool	CPostScanDebug::ms_bDrawHdSphere				= false;
bool	CPostScanDebug::ms_bDrawHdSphereCounts			= false;
bool	CPostScanDebug::ms_bDrawHdSphereList			= false;
bool	CPostScanDebug::ms_bDrawLodSphere				= false;
bool	CPostScanDebug::ms_bDrawLodSphereCounts			= false;
bool	CPostScanDebug::ms_bDrawLodSphereList			= false;
bool	CPostScanDebug::ms_bDrawInteriorSphere			= false;
bool	CPostScanDebug::ms_bDrawInteriorSphereCounts	= false;
bool	CPostScanDebug::ms_bDrawInteriorSphereList		= false;

bool	CPostScanDebug::ms_bDebugSelectedEntityPreRender = false;
bool	CPostScanDebug::ms_bSelectedEntityWasPreRenderedThisFrame = false;
int		CPostScanDebug::ms_preRenderFailTimer = 0;

bool	CPostScanDebug::ms_breakIfEntityIsInPVS = false;
bool	CPostScanDebug::ms_breakOnSelectedEntityInBuildRenderListFromPVS = false;
bool	CPostScanDebug::ms_enableSortingAndExteriorScreenQuadScissoring = true;
bool	CPostScanDebug::ms_forceRoomZeroReady = false;

bool	CPostScanDebug::ms_dontScissorDynamicEntities = true;
bool    CPostScanDebug::ms_renderVisFlagsForSelectedEntity = false;
bool    CPostScanDebug::ms_renderClosedInteriors = false;
bool    CPostScanDebug::ms_renderOpenPortalsForSelectedInterior = false;

bool	CPostScanDebug::ms_renderDebugSphere = false;
Vector3 CPostScanDebug::ms_debugSpherePos; 
float   CPostScanDebug::ms_debugSphereRadius = 0.125f;

CGtaPvsEntry CPostScanDebug::ms_pvsEntryForSelectedEntity[2];
u16			 CPostScanDebug::ms_pvsEntryEntityRenderPhaseVisibilityMask[2];

CPostScanDebugSimplePvsStats CPostScanDebug::ms_simplePvsStats;

bool CPostScanDebug::ms_enableForceLodInCCS = true;

// these get updated in CPostScanDebug::ProcessDebug, displayed in CPostScanDebug::PrintNumModelsRendered
int		CPostScanDebug::ms_PVSModelCounts_RenderPhaseId[];
int		CPostScanDebug::ms_PVSModelCounts_CascadeShadowsRenderPhaseId = -1;
int		CPostScanDebug::ms_PVSModelCounts_CascadeShadows[] = {-1,-1,-1,-1};
int		CPostScanDebug::ms_PVSModelCounts_ReflectionsRenderPhaseId = -1;
int		CPostScanDebug::ms_PVSModelCounts_Reflections[] = {-1,-1,-1,-1};
int		CPostScanDebug::ms_PSSModelCount = -1;
int		CPostScanDebug::ms_PSSModelSubCounts[] = {-1,-1,-1,-1};

int		CPostScanDebug::ms_NumModelPreRendered;
int		CPostScanDebug::ms_NumModelPreRendered2;

char	CPostScanSqlDumper::ms_filename[512];
bool	CPostScanSqlDumper::ms_dumpEnabled = false;
bool	CPostScanSqlDumper::ms_emitCreateTableStatement = true;
int		CPostScanSqlDumper::ms_captureId = 0;

fiStream*	CPostScanSqlDumper::ms_dumpFile;
int		CPostScanSqlDumper::ms_gbufPhaseId;
int		CPostScanSqlDumper::ms_csmPhaseId;
int		CPostScanSqlDumper::ms_extReflectionPhaseId;
int		CPostScanSqlDumper::ms_hdStreamPhaseId;
int		CPostScanSqlDumper::ms_streamVolumePhaseId;

enum
{
	CULLBOX_REJECTIONPOLICY_CONTAINED,
	CULLBOX_REJECTIONPOLICY_INTERSECTED,
	CULLBOX_REJECTIONPOLICY_TOTAL
};

const char*	apszCullboxRejectionPolicy[CULLBOX_REJECTIONPOLICY_TOTAL] =
{
	"Contained",			//CULLBOX_REJECTIONPOLICY_TOTAL
	"Intersected",			//CULLBOX_REJECTIONPOLICY_TOTAL
};

const char* apszLodLevels[] =
{	
	"HD",
	"LOD",
	"SLOD1",
	"SLOD2",
	"SLOD3",
	"SLOD4"
};

char renderPhaseNames[MAX_NUM_RENDER_PHASES][64];

char s_achPvsLogFileName[256] = "X:\\gta5\\build\\dev\\PvsLog.txt";

static char ms_oldModelName[RAGE_MAX_PATH];
static char ms_newModelName[RAGE_MAX_PATH];

template <int tFlag > void SetSceneNodeFlagForCurrentRoom()
{
	if ( g_SceneToGBufferPhaseNew->GetPortalVisTracker() == NULL )
		return;

	CInteriorInst*		renderPhaseInterior = g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorInst();
	const s32			renderPhaseRoom = g_SceneToGBufferPhaseNew->GetPortalVisTracker()->m_roomIdx;
	fwInteriorLocation	renderPhaseInteriorLocation = CInteriorInst::CreateLocation( renderPhaseInterior, renderPhaseRoom );

	if ( !renderPhaseInteriorLocation.IsValid() )
		return;
	else
	{
		for (fwSceneGraphVisitor visitor( fwWorldRepMulti::GetSceneGraphRoot() ); !visitor.AtEnd(); visitor.Next())
		{
			fwSceneGraphNode*	sceneNode = visitor.GetCurrent();
			if ( sceneNode->IsTypeRoom() )
			{
				fwRoomSceneGraphNode* roomSceneNode = static_cast< fwRoomSceneGraphNode* >( sceneNode );
				if ( roomSceneNode->GetInteriorLocation().IsSameLocation( renderPhaseInteriorLocation ) )
				{
					if (tFlag == ROOM_DONT_RENDER_EXTERIOR)
					{
						bool exteriorDisabled = roomSceneNode->GetExteriorDisabled();
						exteriorDisabled = !exteriorDisabled;
						roomSceneNode->SetExteriorDisabled(exteriorDisabled);
					}
					if (tFlag == ROOM_FORCE_DIR_LIGHT_ON)
					{
						bool dirEnabled = roomSceneNode->GetDirectionalLightEnabled();
						dirEnabled = !dirEnabled;
						roomSceneNode->SetDirectionalLightEnabled(dirEnabled);
					}

					s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
					CMloModelInfo *pModelInfo = reinterpret_cast<CMloModelInfo*>(renderPhaseInterior->GetBaseModelInfo());
					u32 *pFlags = (u32*)&pModelInfo->GetRooms()[roomIdx].m_flags;
					u32 flagBitSet = (*pFlags) & tFlag;
					if (flagBitSet)
					{
						*pFlags &= ~tFlag;
					}
					else
					{
						*pFlags |= tFlag;
					}
					break;
				}
			}
		}
	}
}

namespace rage 
{
extern int g_debugDrawGlobalIsolatedPortalIndex;
}

template <int tFlag > void SetSceneNodeFlagForIsolatedPortal()
{
	if ( g_SceneToGBufferPhaseNew->GetPortalVisTracker() == NULL )
		return;

	for (fwSceneGraphVisitor visitor( fwWorldRepMulti::GetSceneGraphRoot() ); !visitor.AtEnd(); visitor.Next())
	{
		fwSceneGraphNode*	sceneNode = visitor.GetCurrent();
		if ( sceneNode->IsTypePortal())
		{
			fwPortalSceneGraphNode* portalSceneNode = static_cast< fwPortalSceneGraphNode* >( sceneNode );
			if ( portalSceneNode->GetIndex() == g_debugDrawGlobalIsolatedPortalIndex )
			{
				if (tFlag == INTINFO_PORTAL_ONE_WAY)
				{
					bool oneWay = portalSceneNode->GetIsOneWay();
					oneWay = !oneWay;
					portalSceneNode->SetIsOneWay(oneWay);
				}
				else if (tFlag == INTINFO_PORTAL_LOWLODONLY)
				{
					bool lodsOnly = portalSceneNode->GetRenderLodsOnly();
					lodsOnly = !lodsOnly;
					portalSceneNode->SetRenderLodsOnly(lodsOnly);
				}
				else if (tFlag == INTINFO_PORTAL_ALLOWCLOSING)
				{
					fwInteriorLocation loc = portalSceneNode->GetInteriorLocation();
					const CInteriorInst* intInst = CInteriorInst::GetInteriorForLocation(loc);
					CMloModelInfo* pMloModelInfo = intInst->GetMloModelInfo();

					sceneAssert(pMloModelInfo);
					sceneAssert(pMloModelInfo->GetIsStreamType());
					s32 portalIndex = loc.GetPortalIndex();

					const atArray< CMloPortalDef >&	portals = pMloModelInfo->GetPortals();
					CMloPortalDef* portalDef = (CMloPortalDef*)&portals[portalIndex];

					CPortalFlags portalFlags = pMloModelInfo->GetPortalFlags(portalIndex);

					if (portalFlags.GetAllowClosing())
					{
						if (!portalSceneNode->IsEnabled())
						{
							portalSceneNode->Enable(true);
						}
						portalSceneNode->SetAllowClosing(false);
						portalDef->m_flags &= ~INTINFO_PORTAL_ALLOWCLOSING;
					}
					else
					{
						portalSceneNode->SetAllowClosing(true);
						portalDef->m_flags |= INTINFO_PORTAL_ALLOWCLOSING;
					}
				}

				CompileTimeAssert(tFlag == INTINFO_PORTAL_ONE_WAY || tFlag == INTINFO_PORTAL_LOWLODONLY || tFlag == INTINFO_PORTAL_ALLOWCLOSING);
				break;
			}
		}
	}
}

template <eIsVisibleModule module > void ToggleEntityIsVisibleForModule()
{
	// trace selected entity
	if (!g_PickerManager.GetSelectedEntity())
	{
		return;
	}
	CEntity *pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
	bool visible = pEntity->GetIsVisibleForModule(module);
	pEntity->SetIsVisibleForModule(module, !visible, true);
}

template <u32 phase > void ToggleEntityIsVisibleForPhase()
{
	// trace selected entity
	if (!g_PickerManager.GetSelectedEntity())
	{
		return;
	}
	CEntity *pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
	bool flag = pEntity->GetRenderPhaseVisibilityMask().IsFlagSet(phase);
	pEntity->ChangeVisibilityMask(phase, !flag, false);
}

bool CPostScanDebug::GetCountModelsRendered()
{
	return ms_bCountModelsRendered;
}

void CPostScanDebug::SetCountModelsRendered(bool bEnable, int drawList, bool bShowStreaming)
{
	ms_bCountModelsRendered = bEnable;
	ms_CountModelsRenderedDrawList = drawList + 1; // 0 is "All"
	ms_bCountModelsRenderedShowStreaming = bShowStreaming;
}

void CPostScanDebug::EnableCullSphereCB()
{
	if (ms_bEnableExteriorHdCullSphere)
	{
		s_cullSphere.Set( VECTOR3_TO_VEC3V(camInterface::GetPos()), ScalarV((float)ms_exteriorCullSphereRadius));
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		add rag widgets for post-scan processing / culling etc
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::AddWidgets(bkBank* pBank)
{
	Assert(pBank);

	CEntityReportGenerator::InitWidgets(*pBank);

	pBank->PushGroup("Timed models");
	{
		pBank->AddToggle("Display timed obj info", &ms_bTimedModelInfo);
	}
	pBank->PopGroup();

	pBank->PushGroup("Exterior HD Culling");
	{
		pBank->AddSlider("Radius", &ms_exteriorCullSphereRadius, 1, 300, 1);
		pBank->AddToggle("Enable cull sphere", &ms_bEnableExteriorHdCullSphere, EnableCullSphereCB);
		pBank->AddToggle("Enable cull selected models", &ms_bEnableExteriorHdModelCull);
		pBank->AddToggle("Disable occlusion", &s_DisableOcclusion);
	}
	pBank->PopGroup();

	pBank->PushGroup("LOD Filter Mode");
	{
		for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			pBank->AddToggle(fwLodData::ms_apszLevelNames[i], &CLodMgr::ms_abSlodModeLevels[i]);
		}
		pBank->AddButton("Start LOD Filter mode", CLodMgr::StartSlodModeCb);
		pBank->AddButton("Stop LOD Filter mode", CLodMgr::StopSlodModeCb);
	}
	pBank->PopGroup();

	pBank->PushGroup("PreRender");
	pBank->AddToggle("Display when selected is not pre-rendered", &ms_bDebugSelectedEntityPreRender);
	pBank->PopGroup();

	pBank->PushGroup("Track entity");
	pBank->AddToggle("Track selected entity", &ms_bTrackSelectedEntity);
	pBank->PopGroup();

	pBank->PushGroup("Map changes");
	{
		pBank->AddToggle("Display active changes", &ms_bDisplayModelSwaps);
		pBank->AddText("Model name", ms_oldModelName, RAGE_MAX_PATH);
		pBank->AddText("New model name", ms_newModelName, RAGE_MAX_PATH);
		pBank->AddSlider("Radius", &ms_modelSwapRadius, 1, 50, 1);
		pBank->AddToggle("Lazy", &ms_bLazySwap);
		pBank->AddToggle("Include script entities", &ms_bIncludeScriptObjects);
		pBank->AddButton("Get model name from picker", GetModelNameFromPickerCB);
		pBank->AddButton("Get new model name from picker", GetNewModelNameFromPickerCB);
		pBank->AddButton("Add swap at camera pos", CreateNewModelSwapCB);
		pBank->AddButton("Add hide at camera pos", CreateNewModelHideCB);
		pBank->AddButton("Add force to object at camera pos", CreateNewForceToObjectCB);
		pBank->AddSlider("Change index for delete", &ms_modelSwapIndex, -1, 500, 1);
		pBank->AddButton("Delete by index", DeleteModelSwapCB);
	}
	pBank->PopGroup();

	pBank->PushGroup("Object population");
	{
		pBank->AddToggle("Display objects", &ms_bDisplayObjects);
		pBank->AddToggle("Display dummy objects", &ms_bDisplayDummyObjects);
		pBank->AddToggle("Display object pool summary", &ms_bDisplayObjectPoolSummary);
		pBank->AddToggle("Display dummy debug output", &ms_bDisplayDummyDebugOutput);

		pBank->AddToggle("Enable force obj", &ms_bEnableForceObj);
		pBank->AddToggle("Draw force obj sphere", &ms_bDrawForceObjSphere);
		pBank->AddVector("Force pos", &ms_vForceObjPos, -16000.0f, 16000.0f, 0.001f);
		pBank->AddSlider("Force radius", &ms_fForceObjRadius, 1.0f, 50.0f, 0.01f);
		pBank->AddButton("Get pos from camera", GetForceObjPosFromCameraCB);

		pBank->PushGroup("Clear Area Of Objects");
		{
			pBank->AddSlider("Force Radius", &ms_fClearAreaObjRadius, 1.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Clear Flag", &ms_ClearAreaFlag, 0, 10, 1);
			pBank->AddButton("Clear Objects", ClearAreaOfObjectsCB);
		}
		pBank->PopGroup();
	}
	pBank->PopGroup();

	pBank->PushGroup("Streaming Spheres");
	{
		pBank->AddToggle("HD sphere - draw sphere", &ms_bDrawHdSphere);
		pBank->AddToggle("HD sphere - draw entity counts", &ms_bDrawHdSphereCounts);
		pBank->AddToggle("HD sphere - draw entity list", &ms_bDrawHdSphereList);
		pBank->AddToggle("LOD sphere - draw sphere", &ms_bDrawLodSphere);
		pBank->AddToggle("LOD sphere - draw entity counts", &ms_bDrawLodSphereCounts);
		pBank->AddToggle("LOD sphere - draw entity list", &ms_bDrawLodSphereList);
		pBank->AddToggle("Interior sphere - draw sphere", &ms_bDrawInteriorSphere);
		pBank->AddToggle("Interior sphere - draw entity counts", &ms_bDrawInteriorSphereCounts);
		pBank->AddToggle("Interior sphere - draw entity list", &ms_bDrawInteriorSphereList);
	}
	pBank->PopGroup();

	pBank->PushGroup("PostScan", false);
	{
		pBank->AddToggle("Partition PVS by LOD instead of sorting by LOD", &ms_PartitionPvsInsteadOfSorting);
		pBank->AddToggle("Verify PVS is sorted in descending LOD", &ms_VerifyPvsSort);

		pBank->AddToggle("Enable simple PVS stats", &ms_bCollectEntityStats);
		pBank->AddToggle("Draw simple PVS stats", &ms_bDrawEntityStats);

		pBank->AddToggle("Count models rendered" , &ms_bCountModelsRendered);
		pBank->AddToggle("Count shaders rendered", &ms_bCountShadersRendered);

		pBank->PushGroup("Override GBuf visibility", false);
		{
			const u32	hdStreamVisBit = CGameScan::GetHdStreamVisibilityBit();
			const u32	lodStreamVisBit = CGameScan::GetLodStreamVisibilityBit();
			const u32	streamingVolumeVisBit1 = CGameScan::GetStreamVolumeVisibilityBit1();
			const u32	streamingVolumeVisBit2 = CGameScan::GetStreamVolumeVisibilityBit2();
			const u32	interiorStreamVisBit = CGameScan::GetInteriorStreamVisibilityBit();
			const char*	renderPhaseNamePtrs[MAX_NUM_RENDER_PHASES];

			for (int i = 0; i < MAX_NUM_RENDER_PHASES; ++i)
			{
				fwRenderPhase*	renderPhase = gRenderListGroup.GetRenderListForPhase(i).GetRenderPhaseNew();

				if ( renderPhase )
					sprintf( renderPhaseNames[i], "%d: %s", i, renderPhase->GetName() );
				else if ( i == (int) hdStreamVisBit )
					sprintf( renderPhaseNames[i], "%d: Streaming (HD Stream sphere)", i );
				else if ( i == (int) lodStreamVisBit )
					sprintf( renderPhaseNames[i], "%d: Streaming (LOD sphere)", i );
				else if ( i == (int) streamingVolumeVisBit1 )
					sprintf( renderPhaseNames[i], "%d: Streaming (streaming volume 1)", i );
				else if ( i == (int) streamingVolumeVisBit2 )
					sprintf( renderPhaseNames[i], "%d: Streaming (streaming volume 2)", i );
				else if ( i == (int) interiorStreamVisBit )
					sprintf( renderPhaseNames[i], "%d: Streaming (Interior stream sphere)", i );
				else
					sprintf( renderPhaseNames[i], "%d:", i );

				renderPhaseNamePtrs[i] = &(renderPhaseNames[i][0]);
			}

			pBank->AddToggle("Enable", &ms_bOverrideGBufBitsEnabled);
			pBank->AddCombo( "Render phase", &ms_nOverrideGBufBitsPhase, MAX_NUM_RENDER_PHASES, renderPhaseNamePtrs, UpdateOverrideGBufVisibilityType );
			pBank->AddToggle("Override render sub-bucket mask only", &ms_bOverrideSubBucketMaskOnly);
		}
		pBank->PopGroup();

		pBank->PushGroup("Count models", false);
		{
			//pBank->AddCombo ("Drawlist", &ms_CountModelsRenderedDrawList, DL_MAX_TYPES + 1, GetUIStrings_eGTADrawListType(true));

			pBank->AddToggle("Show paraboloid shadows", &ms_bCountModelsRenderedShowParabShad);
			pBank->AddToggle("Show reflections"       , &ms_bCountModelsRenderedShowReflections);

			pBank->AddToggle("Show breakdown", &ms_bCountModelsRenderedShowBreakdown);
			pBank->AddToggle("Show prerender", &ms_bCountModelsRenderedShowPreRender);
			pBank->AddToggle("Show streaming", &ms_bCountModelsRenderedShowStreaming);

			pBank->AddSeparator();

			pBank->AddToggle("PVS"           , &ms_bCountModelsRendered_PVS           );
			pBank->AddToggle("RPASS_VISIBLE" , &ms_bCountModelsRendered_RPASS_VISIBLE );
			pBank->AddToggle("RPASS_LOD"     , &ms_bCountModelsRendered_RPASS_LOD     );
			pBank->AddToggle("RPASS_CUTOUT"  , &ms_bCountModelsRendered_RPASS_CUTOUT  );
			pBank->AddToggle("RPASS_DECAL"   , &ms_bCountModelsRendered_RPASS_DECAL   );
			pBank->AddToggle("RPASS_FADING"  , &ms_bCountModelsRendered_RPASS_FADING  );
			pBank->AddToggle("RPASS_ALPHA"   , &ms_bCountModelsRendered_RPASS_ALPHA   );
			pBank->AddToggle("RPASS_WATER"   , &ms_bCountModelsRendered_RPASS_WATER   );
			pBank->AddToggle("RPASS_TREE"    , &ms_bCountModelsRendered_RPASS_TREE    );
			pBank->AddToggle("RPASS_IMPOSTER", &ms_bCountModelsRendered_RPASS_IMPOSTER);
		}
		pBank->PopGroup();

		pBank->AddSeparator();

		pBank->AddToggle("Cull non-frags", &ms_bCullNonFragTypes);
		pBank->AddToggle("Cull frags", &ms_bCullNonFragTypes);

		pBank->AddTitle("PVS Screen Size Cull");
		pBank->AddToggle("Size on-screen cull", &ms_bScreenSizeCullEnabled);
		pBank->AddToggle("Reverse cull", &ms_bScreenSizeCullReversed);
		pBank->AddSlider("Cull W >", &ms_nScreenSizeCullMaxW, 0, 1000, 1);
		pBank->AddSlider("Cull H >", &ms_nScreenSizeCullMaxH, 0, 1000, 1);
		pBank->AddToggle("Show guide rect", &ms_bScreenSizeCullShowGuide);
		pBank->AddSeparator();

		pBank->AddTitle("LOD Based Cull");
		pBank->AddToggle("Only draw LOD trees", &ms_bOnlyDrawLodTrees);
		pBank->AddToggle("Only draw selected LOD tree", &ms_bOnlyDrawSelectedLodTree);
		pBank->AddToggle("Draw LOD range for selected entity", &ms_bDrawLodRanges );
		pBank->AddToggle("Switch to level", &ms_bDrawByLodLevel);
		pBank->AddToggle("Cull only level", &ms_bStripLodLevel);
		pBank->AddToggle("Cull all except level", &ms_bStripAllExceptLodLevel);
		pBank->AddToggle("Cull if parented", &ms_bCullIfParented);
		pBank->AddToggle("Cull if only one child", &ms_bCullIfHasOneChild);
		pBank->AddCombo("Lod Level", &ms_nSelectedLodLevel, LODTYPES_MAX_NUM_LEVELS, &apszLodLevels[0]);
		pBank->AddToggle("Cull lodded HD", &ms_bCullLoddedHd);
		pBank->AddToggle("Cull orphan HD", &ms_bCullOrphanHd);
		pBank->AddToggle("Cull LOD", &ms_bCullLod);
		pBank->AddToggle("Cull SLOD1", &ms_bCullSlod1);
		pBank->AddToggle("Cull SLOD2", &ms_bCullSlod2);
		pBank->AddToggle("Cull SLOD3", &ms_bCullSlod3);
		pBank->AddToggle("Cull SLOD4", &ms_bCullSlod4);
		pBank->AddSeparator();

		pBank->AddTitle("AABB Cull");
		pBank->AddToggle("Only draw stuff inside AABB", &ms_bBoxCull);
		pBank->AddToggle("Print entity count within AABB", &ms_bShowEntityCount);
		pBank->AddCombo("Only show", &ms_nCullBoxSelectedPolicy, CULLBOX_REJECTIONPOLICY_TOTAL, &apszCullboxRejectionPolicy[0]);
		pBank->AddSlider("Cull box X", &ms_nCullBoxX, -16000, 16000, 1);
		pBank->AddSlider("Cull box Y", &ms_nCullBoxY, -16000, 16000, 1);
		pBank->AddSlider("Cull box Z", &ms_nCullBoxZ, -16000, 16000, 1);
		pBank->AddSlider("Cull box W", &ms_nCullBoxW, 0, 8000, 10);
		pBank->AddSlider("Cull box H", &ms_nCullBoxH, 0, 8000, 10);
		pBank->AddSlider("Cull box D", &ms_nCullBoxD, 0, 8000, 10);
		pBank->AddButton("Centre around camera", CentreCullBox);
		pBank->AddSeparator();

		pBank->PushGroup("Alpha");
		{
		pBank->AddToggle("Force up alpha of selected", &ms_bForceUpAlpha);
		pBank->AddToggle("Alpha update enabled?", &ms_bDoAlphaUpdate);
		pBank->AddToggle("Adjust alpha for selected entity", &ms_bTweakAlpha);
		pBank->AddSlider("Alpha", &ms_userAlpha, 0, 255, 1);
		}
		pBank->PopGroup();

		pBank->PushGroup("AlphaDev");
		{
		pBank->AddButton("Reset alpha", ResetAlphaCB);
		pBank->AddToggle("Adjust selected", &ms_bAdjustLodData);
		pBank->AddToggle("Loaded flag", &ms_bAdjustLodData_LoadedFlag);
		pBank->AddSlider("Alpha", &ms_adjustLodData_Alpha, 0, 255, 1);
		}
		pBank->PopGroup();

		pBank->PushGroup("PreRender");
		{
		pBank->AddToggle("Display always prerender list", &ms_bDisplayAlwaysPreRenderList);
		pBank->AddButton("Set selected to be always prerendered", AddToAlwaysPreRenderListCB);
		pBank->AddButton("Stop selected from being always prerendered", RemoveFromAlwaysPreRenderListCB);
		}
		pBank->PopGroup();

		pBank->AddSeparator();
		pBank->AddToggle("Draw only model of selected type", &ms_bShowOnlySelectedType);

		CPostScanSqlDumper::AddWidgets( pBank );
	}
	{
		bkBank * pPostScanBank = BANKMGR.FindBank( "World Scan & Search" );

		if (Verifyf(pPostScanBank, "Failed to find World Scan & Search bank"))
		{
			pPostScanBank->PushGroup( "Post Scan" );

			pPostScanBank->AddToggle( "Break if selected Entity is in PVS", &ms_breakIfEntityIsInPVS);
			pPostScanBank->AddToggle( "Trace selected entity in BuildRenderListFromPVS", &ms_breakOnSelectedEntityInBuildRenderListFromPVS);
			pPostScanBank->AddToggle( "Enable Sorting Interior/Exterior and Scissoring of Exterior", &ms_enableSortingAndExteriorScreenQuadScissoring);
			pPostScanBank->AddToggle( "Enable Clamping of Scissor Rectangle", &CRenderListBuilder::s_RenderListBuilderEnableScissorClamping);
			pPostScanBank->AddToggle( "Trace selected entity in AddToDrawListEntityListCore", &fwRenderListBuilder::ms_breakInAddToDrawListEntityListCore);
			pPostScanBank->AddToggle( "Enable Force Rendering Lod for Cascade Shadows", &ms_enableForceLodInCCS);
			

			pPostScanBank->PushGroup( "Room Flags" );
			pPostScanBank->AddButton( "Toggle Freeze Vehicles", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_FREEZEVEHICLES>));
			pPostScanBank->AddButton( "Toggle Freeze Peds", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_FREEZEPEDS>));
			pPostScanBank->AddButton( "Toggle No Directional Light", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_NO_DIR_LIGHT>));
			pPostScanBank->AddButton( "Toggle Reduce Cars", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_REDUCE_CARS>));
			pPostScanBank->AddButton( "Toggle Reduce Peds", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_REDUCE_PEDS>));
			pPostScanBank->AddButton( "Toggle Force Directional Lights On", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_FORCE_DIR_LIGHT_ON>));
			pPostScanBank->AddButton( "Toggle Disable Exterior Rendering", datCallback(SetSceneNodeFlagForCurrentRoom<ROOM_DONT_RENDER_EXTERIOR>));
			pPostScanBank->PopGroup();

			pPostScanBank->AddToggle( "Ignore Disable Exterior Rendering flag for current room", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_IGNORE_DONT_RENDER_EXTERIOR_FROM_CURRENT_ROOM);
			extern bool gForceClosablePortalsShutOnInit;
			pPostScanBank->AddToggle( "Force Closable Portals shut at init", &gForceClosablePortalsShutOnInit);

			pPostScanBank->AddToggle( "Force Room 0 to be Ready", &CPostScanDebug::ms_forceRoomZeroReady);
			pPostScanBank->AddToggle( "Don't scissor Dynamic Entities", &ms_dontScissorDynamicEntities);
			pPostScanBank->AddToggle( "Draw Visibility Flags for selected entity", &ms_renderVisFlagsForSelectedEntity);
			pPostScanBank->AddToggle( "Display closed interiors", &ms_renderClosedInteriors);
			pPostScanBank->AddToggle( "Display open portals for selected interior", &ms_renderOpenPortalsForSelectedInterior);
			pPostScanBank->AddToggle( "Draw Debug Sphere", &ms_renderDebugSphere);
			pPostScanBank->AddSlider( "Debug Sphere postion.x", &ms_debugSpherePos.x, -8000.0f, 8000.0f, 0.01f);
			pPostScanBank->AddSlider( "Debug Sphere postion.y", &ms_debugSpherePos.y, -8000.0f, 8000.0f, 0.01f);
			pPostScanBank->AddSlider( "Debug Sphere postion.z", &ms_debugSpherePos.z, -8000.0f, 8000.0f, 0.01f);
			pPostScanBank->AddSlider( "Debug Sphere radius", &ms_debugSphereRadius, -100.0f, 100.0f, 0.01f);

			pPostScanBank->PushGroup( "Isolated Portal Flags" );
			pPostScanBank->AddSlider( "Global Isolate portal index",				fwScanNodesDebug::DebugDrawGlobalIsolatedPortalIndex(), -1, 65535, 1 );

			pPostScanBank->AddButton( "Toggle One Way", datCallback(SetSceneNodeFlagForIsolatedPortal<INTINFO_PORTAL_ONE_WAY>));
			pPostScanBank->AddButton( "Toggle Low Lod Only", datCallback(SetSceneNodeFlagForIsolatedPortal<INTINFO_PORTAL_LOWLODONLY>));
			pPostScanBank->AddButton( "Toggle Allow Closing", datCallback(SetSceneNodeFlagForIsolatedPortal<INTINFO_PORTAL_ALLOWCLOSING>));

			pPostScanBank->PopGroup();

			pPostScanBank->PushGroup( "Entity Visible for Module Flags");
			pPostScanBank->AddButton( "Toggle Debug", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_DEBUG>));
			pPostScanBank->AddButton( "Toggle Camera", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_CAMERA>));
			pPostScanBank->AddButton( "Toggle Script", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_SCRIPT>));
			pPostScanBank->AddButton( "Toggle Cutscene", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_CUTSCENE>));
			pPostScanBank->AddButton( "Toggle Gameplay", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_GAMEPLAY>));
			pPostScanBank->AddButton( "Toggle Frontend", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_FRONTEND>));
			pPostScanBank->AddButton( "Toggle VFX", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_VFX>));
			pPostScanBank->AddButton( "Toggle Network", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_NETWORK>));
			pPostScanBank->AddButton( "Toggle Physics", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_PHYSICS>));
			pPostScanBank->AddButton( "Toggle World", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_WORLD>));
			pPostScanBank->AddButton( "Toggle Dummy Conversion", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_DUMMY_CONVERSION>));
			pPostScanBank->AddButton( "Toggle Player", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_PLAYER>));
			pPostScanBank->AddButton( "Toggle Pickup", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_PICKUP>));
			pPostScanBank->AddButton( "Toggle First Person", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_FIRST_PERSON>));
			pPostScanBank->AddButton( "Toggle Vehicle Respot", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_VEHICLE_RESPOT>));
			pPostScanBank->AddButton( "Toggle Respawn", datCallback(ToggleEntityIsVisibleForModule<SETISVISIBLE_MODULE_RESPAWN>));
			pPostScanBank->PopGroup();

			pPostScanBank->PushGroup( "Entity Visible for Render Phase Flags");
			pPostScanBank->AddButton( "Toggle Other", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_OTHER>));
			pPostScanBank->AddButton( "Toggle GBuff", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_GBUF>));
			pPostScanBank->AddButton( "Toggle Cascade Shadows", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_CASCADE_SHADOWS>));
			pPostScanBank->AddButton( "Toggle Paraboloid Shadows", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_PARABOLOID_SHADOWS>));
			pPostScanBank->AddButton( "Toggle Paraboloid Reflection", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_PARABOLOID_REFLECTION>));
			pPostScanBank->AddButton( "Toggle Water Reflection", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_WATER_REFLECTION>));
			pPostScanBank->AddButton( "Toggle Mirror Reflection", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_MIRROR_REFLECTION>));
			pPostScanBank->AddButton( "Toggle Rain Collision Map", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_RAIN_COLLISION_MAP>));
			pPostScanBank->AddButton( "Toggle See Through Map", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_SEETHROUGH_MAP>));
			pPostScanBank->AddButton( "Toggle Streaming", datCallback(ToggleEntityIsVisibleForPhase<VIS_PHASE_MASK_STREAMING>));
			pPostScanBank->PopGroup();

			pPostScanBank->PopGroup();
		}
	}
	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates post scan debug values
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::Update()
{
	if (s_DisableOcclusion)
	{
		COcclusion::ScriptDisableOcclusionThisFrame();
	}

	if (ms_bTimedModelInfo)
	{
		DisplayTimedModelInfo();
	}

	const float fVelocity = CRenderer::GetPrevCameraVelocity();
	grcDebugDraw::AddDebugOutput("Player velocity: %4.2f", fVelocity);

	if (RENDERPHASEMGR.HasDebugDraw())
	{
		gRenderListGroup.WaitForSortEntityListJob();
		CPostScan::WaitForPostScanHelper();
	}

	RENDERPHASEMGR.DebugDraw();

	// toggle player ped visibility
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_Z, KEYBOARD_MODE_DEBUG_ALT)) 
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed != NULL)
		{
			pPlayerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_PLAYER, ms_bTogglePlayerPedVisibility);
			ms_bTogglePlayerPedVisibility = !ms_bTogglePlayerPedVisibility;
		}
	}

	if (ms_bTogglePlayerPedVisibility)
	{
		grcDebugDraw::AddDebugOutput("Player ped has been set to invisible by the debug keyboard.  Press Alt-Z again to make visible.");
	}

	// cycle through LOD levels
	{
		static char message[64] = "";
		static int messageTimer = 0;

		int LODCycle = 0;

		if      (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_PLUS,  KEYBOARD_MODE_DEBUG_ALT)) { LODCycle = +1; }
		else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_MINUS, KEYBOARD_MODE_DEBUG_ALT)) { LODCycle = -1; }

		if (LODCycle != 0)
		{
			if (!ms_bDrawByLodLevel)
			{
				ms_bDrawByLodLevel = true;
				ms_nSelectedLodLevel = (LODCycle > 0) ? 0 : (LODTYPES_MAX_NUM_LEVELS - 1);
			}
			else
			{
				ms_nSelectedLodLevel += LODCycle;

				if (ms_nSelectedLodLevel < 0 || ms_nSelectedLodLevel >= LODTYPES_MAX_NUM_LEVELS)
				{
					ms_nSelectedLodLevel = 0;
					ms_bDrawByLodLevel = false;
				}
			}

			if (ms_bDrawByLodLevel)
			{
				sprintf(message, "Scene LOD -> %s", apszLodLevels[ms_nSelectedLodLevel]);
				messageTimer = 60;
			}
			else
			{
				sprintf(message, "Scene LOD -> DEFAULT");
				messageTimer = 60;
			}
		}

		if (messageTimer > 0)
		{
			grcDebugDraw::AddDebugOutput(Color32(128,128,255,255), message);
			messageTimer--;
		}
	}

	if (ms_bDisplayModelSwaps)
	{
		g_MapChangeMgr.DbgDisplayActiveChanges();
	}

	if (ms_bDrawLodRanges)
	{
		DrawLodRanges();
	}

	if (ms_bDisplayObjectPoolSummary)
	{
		DisplayObjectPoolSummary();
	}

	if (ms_bDisplayDummyDebugOutput)
	{
		DisplayDummyDebugOutput();
	}

	if (ms_bDisplayAlwaysPreRenderList)
	{
		fwPtrNodeSingleLink* pNode = gPostScan.m_alwaysPreRenderList.GetHeadPtr();
		while (pNode)
		{
			CEntity* pEntity = (CEntity*) pNode->GetPtr();
			if (pEntity)
			{
				grcDebugDraw::AddDebugOutput("Always pre-render : %s", pEntity->GetModelName());
			}
			pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
		}
	}

	CVehicleModelInfo::DebugUpdate();

	if ( ms_bCountModelsRendered )
	{
		gRenderListGroup.WaitForSortEntityListJob();
		CPostScan::WaitForPostScanHelper();
		int count = RENDERPHASEMGR.GetRenderPhaseCount();

		for(int i=0;i<count;i++) 
		{
			int drawListType, entityListIndex;
			int waterOcclusionIndex = -1;
			const char *phaseName;

			fwRenderPhase *pRenderPhase = &RENDERPHASEMGR.GetRenderPhase(i);

			if (!pRenderPhase->HasEntityListIndex() || !pRenderPhase->IsActive())
			{
				continue;
			}

			drawListType = pRenderPhase->GetDrawListType();
			entityListIndex = pRenderPhase->GetEntityListIndex();
			phaseName = pRenderPhase->GetName();

			if (ms_CountModelsRenderedDrawList != 0)
			{
				if (drawListType != (eGTADrawListType)(ms_CountModelsRenderedDrawList - 1))
				{
					continue;
				}
			}

			if (drawListType == DL_RENDERPHASE_PARABOLOID_SHADOWS && !ms_bCountModelsRenderedShowParabShad)
			{
				continue;
			}
			else if (drawListType == DL_RENDERPHASE_CASCADE_SHADOWS && !ms_bCountModelsRenderedShowCascaShad)
			{
				continue;
			}
			else if (drawListType == DL_RENDERPHASE_REFLECTION_MAP)
			{
				if (!ms_bCountModelsRenderedShowReflections)
				{
					continue;
				}
			}

			// display the number of models in each of the render lists
			PrintNumModelsRendered(entityListIndex, drawListType, waterOcclusionIndex, phaseName);
		}

		if (ms_bCountModelsRenderedShowStreaming)
		{
			grcDebugDraw::AddDebugOutput("---");
			grcDebugDraw::AddDebugOutput("Streaming (visibility): %d [gbuf:%d, HD sphere:%d, stream volume:%d, interior:%d, lod:%d]", ms_PSSModelCount, ms_PSSModelSubCounts[0], ms_PSSModelSubCounts[1], ms_PSSModelSubCounts[2], ms_PSSModelSubCounts[3], ms_PSSModelSubCounts[4] );
		}

		if (ms_bCountModelsRenderedShowPreRender)
		{
			grcDebugDraw::AddDebugOutput("---");
			grcDebugDraw::AddDebugOutput("PreRender total : %d", ms_NumModelPreRendered );
			grcDebugDraw::AddDebugOutput("PreRender2 total : %d", ms_NumModelPreRendered2 );
		}

		if (ms_bCountModelsRenderedShowStreaming || ms_bCountModelsRenderedShowPreRender)
		{
			grcDebugDraw::AddDebugOutput("---");
		}
	}

	if ( ms_bCountShadersRendered )
	{
		gRenderListGroup.WaitForSortEntityListJob();
		CPostScan::WaitForPostScanHelper();
		int count = RENDERPHASEMGR.GetRenderPhaseCount();

		for(int i=0;i<count;i++) 
		{
			int entityListIndex;
			const char *phaseName;

			fwRenderPhase *pRenderPhase = &RENDERPHASEMGR.GetRenderPhase(i);

			if (!pRenderPhase->HasEntityListIndex() || !pRenderPhase->IsActive())
			{
				continue;
			}

			entityListIndex = pRenderPhase->GetEntityListIndex();
			phaseName = pRenderPhase->GetName();

			// display the number of shaders in each of the render lists
			CRenderer::PrintNumShadersRendered(entityListIndex, phaseName);
		}

		grcDebugDraw::AddDebugOutput("---");
	}

#if __DEV || __BANK
	CPortalTracker::DebugDraw();
#endif

#if __BANK
	DrawPedsMissingGfx();
#endif

	if (ms_bEnableForceObj)
	{
		spdSphere forceObjVolume(ms_vForceObjPos, ScalarV(ms_fForceObjRadius));
		CObjectPopulation::SetScriptForceConvertVolumeThisFrame(forceObjVolume);

		if (ms_bDrawForceObjSphere)
		{
			grcDebugDraw::Sphere(ms_vForceObjPos, ms_fForceObjRadius, Color32(0,0,255), false, 1, 64);
		}
	}

	if (ms_bEnableExteriorHdCullSphere)
	{
		g_PostScanCull.EnableSphereThisFrame(s_cullSphere);
	}
	if (ms_bEnableExteriorHdModelCull)
	{
		// cull selected models
		for (s32 i=0; i<g_PickerManager.GetNumberOfEntities(); i++)
		{
			CEntity* pEntity = (CEntity*) g_PickerManager.GetEntity(i);
			if (pEntity)
			{
				CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
				if (pModelInfo)
				{
					g_PostScanCull.EnableModelCullThisFrame(pModelInfo->GetModelNameHash());
				}
			}
		}
	}

	DisplayAlphaAndVisibilityBlameInfoForSelected();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Draw
// PURPOSE:		any required debug draw for post scan debug processing
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::Draw()
{
	char buff[1024];

	if (g_LodMgr.IsSlodModeActive())
	{
		const u32 lodMask = g_LodMgr.GetSlodModeRequiredMask();
		for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			if (lodMask & (1<<i))
			{
				grcDebugDraw::AddDebugOutput("LOD Filter Mode enabled! %s", fwLodData::ms_apszLevelNames[i]);
			}
		}
	}

	if (ms_bScreenSizeCullEnabled && ms_bScreenSizeCullShowGuide)
	{
		fwRect guideRect(100.0f, 100.0f, 100.0f+ms_nScreenSizeCullMaxW, 100.0f+ms_nScreenSizeCullMaxH ); // l, b, r, t
		Color32 guideColour(255, 255, 255, 125);
		CSprite2d::DrawRectSlow(guideRect, guideColour);
	}

	if (ms_bBoxCull)
	{
		//poo
	}

#if TC_DEBUG
	g_timeCycle.Render();
#endif // TC_DEBUG

    int pvsEntryIndex = (TIME.GetFrameCount() - 1) & 1;
	if (ms_renderVisFlagsForSelectedEntity && ms_pvsEntryForSelectedEntity[pvsEntryIndex].GetEntity())
	{
		Vector2 pos(40.0f, 40.0f);
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Selected PVS Entry Data");
		pos.y += 12.0f;

		s32 renderPhaseCount = RENDERPHASEMGR.GetRenderPhaseCount();

		const fwVisibilityFlags &visFlags = ms_pvsEntryForSelectedEntity[pvsEntryIndex].GetVisFlags();
		for(s32 i = 0; i < renderPhaseCount; ++i)
		{
			CRenderPhaseScanned* renderPhase = (CRenderPhaseScanned *) &RENDERPHASEMGR.GetRenderPhase(i);
			if (visFlags.GetPhaseVisFlags())
			{
				formatf(buff, "%-32s          %s", renderPhase->GetName(), (visFlags.GetPhaseVisFlags() & (1 << renderPhase->GetEntityListIndex())) ? "Visible" : "Not Visible");
				grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
				pos.y += 12.0f;
			}
		}

		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Streaming Only CullShapes");
		pos.y += 12.0f;
		for(s32 i = 0; i < fwScan::GetScanBaseInfo().m_cullShapeCount; ++i)
		{
			fwRenderPhaseCullShape &cullShape = fwScan::GetScanBaseInfo().m_cullShapes[i];
			u32 id = cullShape.GetRenderPhaseId();
			if ( id == CGameScan::GetHdStreamVisibilityBit()	  || id == CGameScan::GetLodStreamVisibilityBit()   || id == CGameScan::GetStreamVolumeVisibilityBit1() || 
				 id == CGameScan::GetStreamVolumeVisibilityBit2() || id == CGameScan::GetInteriorStreamVisibilityBit())
			{
				if (cullShape.GetFlags() & CULL_SHAPE_FLAG_GEOMETRY_SPHERE)
				{
					const spdSphere &sphere = cullShape.GetSphere();
					const Vector3 &centre  = VEC3V_TO_VECTOR3(sphere.GetCenter());
					formatf(buff, "Sphere Pos %.3f, %.3f, %.3f, Radius %.3f %s", centre.x, centre.y, centre.z, sphere.GetRadiusf(), (visFlags.GetPhaseVisFlags() & (1 << cullShape.GetRenderPhaseId())) ? "Visible" : "Not Visible");
				}
				else
				{
					const Vector3 &postion  = VEC3V_TO_VECTOR3(cullShape.GetCameraPosition());
					formatf(buff, "Frustum Pos %.3f, %.3f, %.3f             %s", postion.x, postion.y, postion.z, (visFlags.GetPhaseVisFlags() & (1 << cullShape.GetRenderPhaseId())) ? "Visible" : "Not Visible");
				}
				grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
				pos.y += 12.0f;
			}
		}


		pos.x = 720;
		pos.y = 40.0f;

		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Cascade Shadows");
		pos.y += 12.0f;

		for (int i = 0; i < 4; ++i)
		{
			formatf(buff, "Cascade %c                         %s", char('0' + i), (visFlags.GetSubphaseVisFlags() & (1 << i)) ? "Visible" : "Not Visible");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;
		}

		pos.y += 12.0f;
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Paraboloid Reflection");
		pos.y += 12.0f;

		for (int i = 0; i < 6; ++i)
		{
			formatf(buff, "Facet %c                           %s", char('0' + i), (visFlags.GetSubphaseVisFlags() & (16 << i)) ? "Visible" : "Not Visible");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;
		}

		pos.y += 12.0f;
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Option Flags");
		pos.y += 12.0f;
		{
			formatf(buff, "In Interior                       %s", (visFlags.GetOptionFlags() & VIS_FLAG_OPTION_INTERIOR) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Stream Entity                     %s", (visFlags.GetOptionFlags() & VIS_FLAG_STREAM_ENTITY) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Stream Only Entity                %s", (visFlags.GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Rendered Through LOD Only Portal  %s", (visFlags.GetOptionFlags() & VIS_FLAG_RENDERED_THROUGH_LODS_ONLY_PORTAL) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Ped Inside Vehicle                %s", (visFlags.GetOptionFlags() & VIS_FLAG_ENTITY_IS_PED_INSIDE_VEHICLE) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Entity HD Capable                 %s", (visFlags.GetOptionFlags() & VIS_FLAG_ENTITY_IS_HD_TEX_CAPABLE) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Entity Never Dummy                %s", (visFlags.GetOptionFlags() & VIS_FLAG_ENTITY_NEVER_DUMMY) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;
			pos.y += 12.0f;

			formatf(buff, "LOD Sort Value                    %d", visFlags.GetLodSortVal());
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;
		}

		pos.y += 12.0f;
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Entity Phase Visibility Mask Flags");
		pos.y += 12.0f;
		{
			u16 phaseFlags = ms_pvsEntryEntityRenderPhaseVisibilityMask[pvsEntryIndex];
			formatf(buff, "Phase Other                       %s", (phaseFlags & VIS_PHASE_MASK_OTHER) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase GBuff                       %s", (phaseFlags & VIS_PHASE_MASK_GBUF) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Cascade Shadows             %s", (phaseFlags & VIS_PHASE_MASK_CASCADE_SHADOWS) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Paraboloid Shadows          %s", (phaseFlags & VIS_PHASE_MASK_PARABOLOID_SHADOWS) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Paraboloid Reflection       %s", (phaseFlags & VIS_PHASE_MASK_PARABOLOID_REFLECTION) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Water Reflection            %s", (phaseFlags & VIS_PHASE_MASK_WATER_REFLECTION) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Mirror Reflection           %s", (phaseFlags & VIS_PHASE_MASK_MIRROR_REFLECTION) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Rain Collision Map          %s", (phaseFlags & VIS_PHASE_MASK_RAIN_COLLISION_MAP) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase See Through Map             %s", (phaseFlags & VIS_PHASE_MASK_SEETHROUGH_MAP) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Streaming                   %s", (phaseFlags & VIS_PHASE_MASK_STREAMING) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;
			pos.y += 12.0f;

			formatf(buff, "Phase Flag Shadow Proxy           %s", (phaseFlags & VIS_PHASE_MASK_SHADOW_PROXY) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Flag Water Reflection Proxy %s%s", (phaseFlags & VIS_PHASE_MASK_WATER_REFLECTION_PROXY) ? "True" : "False", (phaseFlags & VIS_PHASE_MASK_WATER_REFLECTION_PROXY_PRE_REFLECTED) ? " (pr)" : "");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;

			formatf(buff, "Phase Flag Water Reflection Child %s", ms_pvsEntryForSelectedEntity[pvsEntryIndex].GetEntity()->IsBaseFlagSet(fwEntity::HAS_NON_WATER_REFLECTION_PROXY_CHILD) ? "True" : "False");
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
			pos.y += 12.0f;
		}

	}

	if (ms_renderClosedInteriors)
	{
		Vector2 pos(40.0f, 40.0f);
		pos.y += 12.0f;
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, "Interiors that have been closed");
		pos.y += 12.0f;
		{
			for (int i = 0; i < fwScan::GetScanBaseInfo().m_closedInteriors.GetCount(); ++i)
			{
				CInteriorProxy *pProxy  = CInteriorProxy::GetPool()->GetSlot(fwScan::GetScanBaseInfo().m_closedInteriors[i]);
				grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, pProxy->GetModelName());
				pos.y += 12.0f;
			}
		}
	}

	if (ms_renderOpenPortalsForSelectedInterior)
	{
		fwEntity *pEntity = g_PickerManager.GetSelectedEntity();
		if (pEntity->GetType() == ENTITY_TYPE_MLO)
		{
			fwRenderPhaseCullShape *pCullShape = NULL;
			for (int i = 0; i < SCAN_RENDER_PHASE_COUNT; ++i)
			{
				if (fwScan::GetScanBaseInfo().m_cullShapes[i].GetFlags() & CULL_SHAPE_FLAG_PRIMARY)
				{
					pCullShape = &fwScan::GetScanBaseInfo().m_cullShapes[i];
				}
			}

			Assertf(pCullShape, "Failed to find GBuff cullshape");

			CInteriorInst *pInst = (CInteriorInst*)pEntity;
			CMloModelInfo *pModelInfo = pInst->GetMloModelInfo();

			for(u32 portalIdx=0; portalIdx < pInst->GetNumPortals(); portalIdx++)
			{
				const CMloPortalDef& portalData = pModelInfo->GetPortals()[portalIdx];
				bool	bPortalToExterior = ((portalData.m_roomFrom == 0) || (portalData.m_roomTo == 0));

				// if its portal to the exterior and its not one way 
				if (bPortalToExterior && !(portalData.m_flags & INTINFO_PORTAL_ONE_WAY)) 
				{
					// If allow closing and its portal container contains no entities
					if (pInst->GetPortalSceneGraphNode(portalIdx)->IsEnabled())
					{
						fwScanNodesDebug::DebugDrawPortal(NULL, pInst->GetPortalSceneGraphNode(portalIdx), NULL, pCullShape, false, true);
					}
				}
			}

			Vector2 pos(40.0f, 40.0f);
			pos.y += 12.0f;

			formatf(buff, "Number of open portals %d", pInst->m_openPortalsCount);
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);

		}
	}


	if (ms_renderDebugSphere)
	{
		grcDebugDraw::Sphere(ms_debugSpherePos, ms_debugSphereRadius, Color32(0xFF,	  0, 0, 0xFF),  false);
	}

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ForceUpAlphas
// PURPOSE:		after lod tree update, force up any debug alpha values
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::ForceUpAlphas()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity && ms_bForceUpAlpha) { pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE); }
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PrintNumModelsRendered
// PURPOSE:		debug dump of the number of models stored in render lists
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::PrintNumModelsRendered(int entityListIndex, int drawListType, int waterOcclusionIndex, const char *name)
{
	const int nList = entityListIndex;

	fwRenderListDesc& desc = gRenderListGroup.GetRenderListForPhase(nList);

	atString temp;
	bool bFirst = true;

	temp += atVarString("%s [id=%d]: ", name, nList);

	if (ms_bCountModelsRendered_PVS)
	{
		temp += atVarString("%s%s=%d", bFirst ? "" : ", ", "PVS", ms_PVSModelCounts_RenderPhaseId[nList]);
		bFirst = false;
	}

#define DEF_COUNT_MODELS_RENDERED_RPASS(rp) \
	if (ms_bCountModelsRendered_RPASS_##rp) \
	{ \
		const int numEntities = desc.GetNumberOfEntities(RPASS_##rp); \
		\
		if (numEntities > 0) \
		{ \
			temp += atVarString("%s%s=%d", bFirst ? "" : ", ", STRING(rp), numEntities); \
			bFirst = false; \
		} \
	} \
	// end.
	DEF_COUNT_MODELS_RENDERED_RPASS(VISIBLE)
	DEF_COUNT_MODELS_RENDERED_RPASS(LOD    )
	DEF_COUNT_MODELS_RENDERED_RPASS(CUTOUT )
	DEF_COUNT_MODELS_RENDERED_RPASS(DECAL  )
	DEF_COUNT_MODELS_RENDERED_RPASS(FADING )
	DEF_COUNT_MODELS_RENDERED_RPASS(ALPHA  )
#undef DEF_COUNT_MODELS_RENDERED_RPASS

	if (ms_bCountModelsRendered_RPASS_WATER)
	{
		const int numEntities = desc.GetNumberOfEntities(RPASS_WATER);

		if (numEntities > 0)
		{
			temp += atVarString("%s%s=%d (occ=%d)", bFirst ? "" : ", ", "WATER", numEntities, waterOcclusionIndex);
			bFirst = false;
		}
	}

	int treeCount = desc.GetNumberOfEntities(RPASS_TREE);
	int treeTypeNum = 0;
	int imposterCount = 0;
	int imposterModelCount = 0;

#if USE_TREE_IMPOSTERS
	if (ms_bCountModelsRendered_RPASS_TREE || ms_bCountModelsRendered_RPASS_IMPOSTER)
	{
		int treeTypeModelIndex[64];
		int treeTypeCacheIndex[64];

		for (int i=0; i<treeCount; i++)
		{
			int modelIndex = desc.GetEntity(i, RPASS_TREE)->GetModelIndex();
			int flag = -1;

			for (int n=0; n<treeTypeNum; n++)
			{
				if (modelIndex == treeTypeModelIndex[n])
				{
					flag = n;				
					break;
				}
			}

			if (flag == -1)
			{
				if (AssertVerify(treeTypeNum < NELEM(treeTypeModelIndex)))
				{
					treeTypeModelIndex[treeTypeNum] = modelIndex;
					treeTypeCacheIndex[treeTypeNum] = CTreeImposters::GetImposterCacheIndex(modelIndex);
				}
				else
				{
					break;
				}

				flag = treeTypeNum++;
			}

			if (treeTypeCacheIndex[flag] != -1)
			{
				float fade = CTreeImposters::GetImposterFade(treeTypeCacheIndex[flag], desc.GetEntitySortVal(i, RPASS_TREE));

				if (fade>0.0f) imposterCount++;
				if (fade<1.0f) imposterModelCount++;
			}
			else
			{
				imposterModelCount++;
			}
		}
	}
#endif // USE_TREE_IMPOSTERS

	if (ms_bCountModelsRendered_RPASS_TREE)
	{
		if (treeCount > 0)
		{
			temp += atVarString("%s%s=%d (types=%d)", bFirst ? "" : ", ", "TREE", treeCount, treeTypeNum);
			bFirst = false;
		}
	}

	if (ms_bCountModelsRendered_RPASS_IMPOSTER)
	{
		if (imposterCount > 0 || imposterModelCount > 0)
		{
			temp += atVarString("%s%s=%d/%d", bFirst ? "" : ", ", "IMPOSTER", imposterCount, imposterModelCount);
			bFirst = false;
		}
	}

	if (drawListType == DL_RENDERPHASE_CASCADE_SHADOWS)
	{
		Assert(ms_PVSModelCounts_CascadeShadowsRenderPhaseId == -1 || ms_PVSModelCounts_CascadeShadowsRenderPhaseId == nList);
		ms_PVSModelCounts_CascadeShadowsRenderPhaseId = nList;

		int indexCount[SUBPHASE_CASCADE_COUNT] = {0,0,0,0};

		indexCount[0] = ms_PVSModelCounts_CascadeShadows[0];
		indexCount[1] = ms_PVSModelCounts_CascadeShadows[1];
		indexCount[2] = ms_PVSModelCounts_CascadeShadows[2];
		indexCount[3] = ms_PVSModelCounts_CascadeShadows[3];

		temp += atVarString(
			"%s%s=[%d,%d,%d,%d], total=%d",
			bFirst ? "" : ", ",
			"cascades",
			indexCount[0],
			indexCount[1],
			indexCount[2],
			indexCount[3],
			indexCount[0] + indexCount[1] + indexCount[2] + indexCount[3]
		);
		bFirst = false;
	}
	else if (drawListType == DL_RENDERPHASE_REFLECTION_MAP)
	{
		Assert(ms_PVSModelCounts_ReflectionsRenderPhaseId == -1 || ms_PVSModelCounts_ReflectionsRenderPhaseId == nList);
		ms_PVSModelCounts_ReflectionsRenderPhaseId = nList;
#if ENABLE_REFLECTION_PER_FACET_CULLING
		int indexCount[SUBPHASE_REFLECT_COUNT] = {0,0,0,0,0,0};

		indexCount[0] = ms_PVSModelCounts_Reflections[0]; 
		indexCount[1] = ms_PVSModelCounts_Reflections[1]; 
		indexCount[2] = ms_PVSModelCounts_Reflections[2]; 
		indexCount[3] = ms_PVSModelCounts_Reflections[3]; 
		indexCount[4] = ms_PVSModelCounts_Reflections[4]; 
		indexCount[5] = ms_PVSModelCounts_Reflections[5]; 

		temp += atVarString(
			"%sfacet0=%d, facet1=%d, facet2=%d, facet3=%d, facet4=%d, facet5=%d, total=%d",
			bFirst ? "" : ", ",
			indexCount[0], 
			indexCount[1], 
			indexCount[2], 
			indexCount[3], 
			indexCount[4], 
			indexCount[5], 
			indexCount[0] + indexCount[1] + indexCount[2] + indexCount[3] + indexCount[4] + indexCount[5]
		);
#else
		int indexCount[SUBPHASE_REFLECT_COUNT] = {0,0,0,0};

		indexCount[0] = ms_PVSModelCounts_Reflections[0]; // SUBPHASE_REFLECT_UPPER_LOLOD
		indexCount[1] = ms_PVSModelCounts_Reflections[1]; // SUBPHASE_REFLECT_LOWER_LOLOD
		indexCount[2] = ms_PVSModelCounts_Reflections[2]; // SUBPHASE_REFLECT_UPPER_HILOD
		indexCount[3] = ms_PVSModelCounts_Reflections[3]; // SUBPHASE_REFLECT_LOWER_HILOD

		temp += atVarString(
			"%supper=%d(%d), lower=%d(%d), total=%d",
			bFirst ? "" : ", ",
			indexCount[2], // SUBPHASE_REFLECT_UPPER_HILOD
			indexCount[0], // SUBPHASE_REFLECT_UPPER_LOLOD
			indexCount[3], // SUBPHASE_REFLECT_LOWER_HILOD
			indexCount[1], // SUBPHASE_REFLECT_LOWER_LOLOD
			indexCount[0] + indexCount[1] + indexCount[2] + indexCount[3]
		);
#endif
		bFirst = false;
	}

	grcDebugDraw::AddDebugOutput(temp.c_str());

	if (ms_bCountModelsRenderedShowBreakdown)
	{
		int	fadingCount[10];
		memset(fadingCount, 0, sizeof(fadingCount));

		for(int i=0; i<desc.GetNumberOfEntities(RPASS_FADING); i++)
		{
			CEntity *pEntity = (CEntity *) desc.GetEntity(i, RPASS_FADING);
			fadingCount[pEntity->GetType()]++;
		}

		grcDebugDraw::AddDebugOutput("Fading Model breaks down as Type non %d, buil %d, veh %d, ped %d, obj %d, dumob %d, dummy ped %d, po %d, mlo %d",
			fadingCount[0], fadingCount[1], fadingCount[2], fadingCount[3], fadingCount[4], fadingCount[5], fadingCount[6], fadingCount[7], fadingCount[8]);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ProcessDebug1
// PURPOSE:		any required debug PVS processing that must occur before CPostScan::ProcessPostVisibility
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::ProcessDebug1(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	// this must happen before CPostScan::ProcessPostVisibility because that nulls out PVS entries
	ProcessVisibilityStreamingCullshapes(pStart, pStop);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ProcessDebug2
// PURPOSE:		any required debug PVS processing that must occur after CPostScan::ProcessPostVisibility
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::ProcessDebug2(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop)
{
	ForceUpAlphas();

	if ( ms_bOverrideGBufBitsEnabled && !ms_bOverrideSubBucketMaskOnly )
	{
		const int	gbufBits = CRenderer::GetSceneToGBufferListBits() | CRenderer::GetDebugPhases();
		const int	nonGbufBits = ~gbufBits & ( (1 << fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS) - 1 );
		for (CGtaPvsEntry* pvsEntry = pPvsStart; pvsEntry != pPvsStop; pvsEntry++)
		{
			fwVisibilityFlags&	visFlags = pvsEntry->GetVisFlags();

			if( visFlags.GetVisBit( ms_nOverrideGBufBitsPhase ) )
				visFlags.SetPhaseVisFlags( visFlags | gbufBits );
			else
				visFlags.SetPhaseVisFlags( visFlags & nonGbufBits );
		}
	}

	if (ms_bCountModelsRendered)
	{
		memset(ms_PVSModelCounts_RenderPhaseId, 0, sizeof(ms_PVSModelCounts_RenderPhaseId));
		memset(ms_PVSModelCounts_CascadeShadows, 0, sizeof(ms_PVSModelCounts_CascadeShadows));
		memset(ms_PVSModelCounts_Reflections, 0, sizeof(ms_PVSModelCounts_Reflections));
		ms_PSSModelCount = 0;
		memset(ms_PSSModelSubCounts, 0, sizeof(ms_PSSModelSubCounts));

		// code roughly follows CPostScan::BuildRenderListsFromPVS
		for (int i=0; i<24; i++)
		{
			for (CGtaPvsEntry* pvsEntry = pPvsStart; pvsEntry != pPvsStop; pvsEntry++)
			{
				if ( !(pvsEntry->GetVisFlags().GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY) && pvsEntry->GetVisFlags().GetVisBit(i))
				{
					const CEntity* entity = pvsEntry->GetEntity();

					if (entity)
					{
						if (entity->GetIsTypePortal()) { continue; }
						if (entity->GetIsTypeLight ()) { continue; }

						const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

						if (!modelInfo->GetHasLoaded()) { continue; }

						ms_PVSModelCounts_RenderPhaseId[i]++;

						if (i == ms_PVSModelCounts_CascadeShadowsRenderPhaseId)
						{
							const u8 shadowVisFlags = (u8)((pvsEntry->GetVisFlags().GetSubphaseVisFlags() & SUBPHASE_CASCADE_MASK) >> SUBPHASE_CASCADE_0);

							for (int k = 0; k < SUBPHASE_CASCADE_COUNT; k++)
							{
								extern bool s_DebugAlwaysRenderShadow;

								if ((shadowVisFlags & (1<<k)) != 0 || s_DebugAlwaysRenderShadow)
								{
									ms_PVSModelCounts_CascadeShadows[k]++;
#if __STATS
									switch (k)
									{
									case 0: PF_INCREMENT( Cascade0Hits ); break;
									case 1: PF_INCREMENT( Cascade1Hits ); break;
									case 2: PF_INCREMENT( Cascade2Hits ); break;
									case 3: PF_INCREMENT( Cascade3Hits ); break;
									}
									PF_INCREMENT( CascadeTotalHits );
#endif // __STATS
								}
							}
						}

						if (i == ms_PVSModelCounts_ReflectionsRenderPhaseId)
						{
#if ENABLE_REFLECTION_PER_FACET_CULLING
							const u16 reflectionVisFlags = (u16)((pvsEntry->GetVisFlags().GetSubphaseVisFlags() & SUBPHASE_REFLECT_FACET_MASK) >> SUBPHASE_REFLECT_FACET0);
#else
							const u8 reflectionVisFlags = (u8)((pvsEntry->GetVisFlags().GetSubphaseVisFlags() & SUBPHASE_REFLECT_MASK) >> SUBPHASE_REFLECT_UPPER_LOLOD);
#endif
							for (int k = 0; k < SUBPHASE_REFLECT_COUNT; k++)
							{
								extern bool s_DebugAlwaysRenderReflection;

								if ((reflectionVisFlags & (1<<k)) != 0 || s_DebugAlwaysRenderReflection)
								{
									ms_PVSModelCounts_Reflections[k]++;
#if __STATS
									switch (k)
									{
									case 0: PF_INCREMENT( Reflection0Hits ); break;
									case 1: PF_INCREMENT( Reflection1Hits ); break;
									case 2: PF_INCREMENT( Reflection2Hits ); break;
									case 3: PF_INCREMENT( Reflection3Hits ); break;
									case 4: PF_INCREMENT( Reflection4Hits ); break;
									case 5: PF_INCREMENT( Reflection5Hits ); break;
									}
									PF_INCREMENT( ReflectionTotalHits );
#endif // __STATS
								}
							}
						}
					}
				}
			}
		}

		for (CGtaPvsEntry* pvsEntry = pPvsStart; pvsEntry != pPvsStop; pvsEntry++)
		{
			CEntity*	entity = pvsEntry->GetEntity();
			if ( !entity || entity->GetIsTypePortal() || entity->GetIsTypeLight() || !(pvsEntry->GetVisFlags().GetOptionFlags() & VIS_FLAG_STREAM_ENTITY) )
				continue;

			const u32	gbufMask = gRenderListGroup.GetRenderPhaseTypeMask( RPTYPE_MAIN );
			const u32	hdStreamVisBit = CGameScan::GetHdStreamVisibilityBit();
			const u32	streamingVolumeVisMask = CGameScan::GetStreamVolumeMask();
			const u32	interiorStreamBit = CGameScan::GetInteriorStreamVisibilityBit();
			const u32	lodStreamBit = CGameScan::GetLodStreamVisibilityBit();

			++ms_PSSModelCount;
			
			if ( pvsEntry->GetVisFlags() & gbufMask )
				ms_PSSModelSubCounts[0] += 1;

			if ( pvsEntry->GetVisFlags() & (1 << hdStreamVisBit) )
				ms_PSSModelSubCounts[1] += 1;

			if ( pvsEntry->GetVisFlags() & streamingVolumeVisMask )
				ms_PSSModelSubCounts[2] += 1;

			if ( pvsEntry->GetVisFlags() & (1 << interiorStreamBit) )
				ms_PSSModelSubCounts[3] += 1;

			if ( pvsEntry->GetVisFlags() & (1 << lodStreamBit) )
				ms_PSSModelSubCounts[4] += 1;
		}
	}

	// ===============================================================================================

	CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (ms_bTrackSelectedEntity && pSelectedEntity)
	{
		Printf("Tracking entity %s\n", pSelectedEntity->GetBaseModelInfo()->GetModelName());
		__debugbreak();
	}
	
	if (CLodDebugMisc::FakeForceLodRequired() && pSelectedEntity && !pSelectedEntity->GetLodData().IsHighDetail())
	{
		CLodDebugMisc::SubmitForcedLodInfo(pSelectedEntity, NULL, pPvsStart, pPvsStop);
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DELETE, KEYBOARD_MODE_DEBUG, "Enable alpha tweak and set to 0"))
	{
		ms_bTweakAlpha = !ms_bTweakAlpha;
		ms_userAlpha = 0;
	}
	if (ms_bTweakAlpha)
	{
		for (s32 i=0; i<g_PickerManager.GetNumberOfEntities(); i++)
		{
			CEntity* pEntity = (CEntity*) g_PickerManager.GetEntity(i);
			if (pEntity)
			{
				pEntity->SetAlpha(ms_userAlpha);
			}
		}
	}

	if (ms_bShowEntityCount)
	{
		ShowBoxEntityCount();
	}

	CGtaPvsEntry* pPvsEntry = pPvsStart;

	if(CMapOptimizeHelper::IsolateEnabled()
		|| ms_bBoxCull
		|| ms_bOnlyDrawLodTrees
		|| ms_bOnlyDrawSelectedLodTree
		|| ms_bScreenSizeCullEnabled
		|| ms_bDrawByLodLevel
		|| ms_bStripAllExceptLodLevel
		|| ms_bStripLodLevel
		|| ms_bCullFragTypes
		|| ms_bCullNonFragTypes
		|| ms_bCullOrphanHd || ms_bCullLoddedHd || ms_bCullLod || ms_bCullSlod1 || ms_bCullSlod2 || ms_bCullSlod3 || ms_bCullSlod4
		|| ms_bCullIfParented
		|| ms_bCullIfHasOneChild
		|| ms_bShowOnlySelectedType )
	{
		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry && pPvsEntry->GetEntity())
			{
				CEntity* pEntity = pPvsEntry->GetEntity();	
				CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();

				// debug entity rejection cases
				if (	(CMapOptimizeHelper::IsolateEnabled() && !CMapOptimizeHelper::ShouldDraw(pEntity))
					||	(ms_bBoxCull && IsBoxCulled(pEntity))
					||	(ms_bOnlyDrawLodTrees && pEntity->GetLodData().IsOrphanHd())
					||	(ms_bOnlyDrawSelectedLodTree && !IsInSelectedLodTree(pEntity))
					||	(ms_bScreenSizeCullEnabled && IsSizeOnScreenCulled(pEntity))
					||	(ms_bDrawByLodLevel && IsLodLevelCulled(pEntity, true))
					||	(ms_bStripAllExceptLodLevel && IsLodLevelCulled(pEntity, false))
					||	(ms_bStripLodLevel && !IsLodLevelCulled(pEntity, false))
					||	(pMI && ms_bCullFragTypes && (pMI->GetFragType()!=NULL))
					||  (pMI && ms_bCullNonFragTypes && (pMI->GetFragType()==NULL))
					||	IsCulledBasedOnLodType(pEntity)
					||	(ms_bCullIfParented && pEntity->GetLodData().HasLod())
					||	(ms_bCullIfHasOneChild && HasOnlyOneChild(pEntity))
					||  (pSelectedEntity && ms_bShowOnlySelectedType && pEntity->GetModelIndex() != pSelectedEntity->GetModelIndex() )
					)
				{
					pPvsEntry->SetEntity(NULL);
				}
			}
			pPvsEntry++;
		}
	}

	// collect stats
	if (ms_bCollectEntityStats)
	{
		ms_simplePvsStats.Reset();
		pPvsEntry = pPvsStart;
		
		u32	canRenderAlphaFlags = CRenderer::GetCanRenderAlphaFlags();

		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry && pPvsEntry->GetEntity() && pPvsEntry->GetEntity()->GetAlpha())
			{
				u32 numShaders = 0;
				CBaseModelInfo* pModelInfo = pPvsEntry->GetEntity()->GetBaseModelInfo();
				rmcDrawable* pDrawable = pModelInfo->GetDrawable();
				if (pDrawable)
				{
					numShaders = (u32)pDrawable->GetShaderGroup().GetCount();
				}

				ms_simplePvsStats.m_numEntitiesInPvs_TOTAL++;				// total pvs entry entity count
				ms_simplePvsStats.m_numShadersInPvs_TOTAL += numShaders;	// total pvs entry shader count

				CEntity* pEntity = pPvsEntry->GetEntity();
				if (pSelectedEntity && pSelectedEntity->GetModelIndex()==pEntity->GetModelIndex())
				{
					ms_simplePvsStats.m_numEntitiesInPvs_SELECTED++;
					ms_simplePvsStats.m_numShadersInPvs_SELECTED += numShaders;
				}

				// check for visibility in each renderphase
				bool bVisInGbuf = false;
				bool bVisInShadows = false;
				bool bVisInReflectionMap = false;
				bool bVisInFwdAlpha = false;

				for (u32 i=0; i<fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS; i++)
				{
					if (pPvsEntry->GetVisFlags().GetVisBit(i))
					{
						fwRenderListDesc& rlDesc = gRenderListGroup.GetRenderListForPhase(i);
						CRenderPhase* renderPhase = (CRenderPhase *) rlDesc.GetRenderPhaseNew();
						
						if ( !renderPhase )
							continue;

						u32 phaseMask = 1<<i;

						if ((phaseMask&canRenderAlphaFlags)
							&&  (pEntity->IsBaseFlagSet(fwEntity::HAS_ALPHA) || pEntity->IsBaseFlagSet(fwEntity::HAS_DISPL_ALPHA)) )
						{
							bVisInFwdAlpha = true;
						}
						
						switch (renderPhase->GetDrawListType())
						{
						case DL_RENDERPHASE_GBUF:
							bVisInGbuf = true;
							break;
						case DL_RENDERPHASE_PARABOLOID_SHADOWS:
						case DL_RENDERPHASE_CASCADE_SHADOWS:
							bVisInShadows = true;
							break;
						case DL_RENDERPHASE_REFLECTION_MAP:
							bVisInReflectionMap = true;
							break;
						default:
							break;
						}
					}
				}
				// update stats
				ms_simplePvsStats.m_numEntitiesVis_GBUF += bVisInGbuf;
				ms_simplePvsStats.m_numEntitiesVis_SHADOWS += bVisInShadows;
				ms_simplePvsStats.m_numEntitiesVis_REFLECTIONS += bVisInReflectionMap;
				ms_simplePvsStats.m_numShadersVis_GBUF += bVisInGbuf*numShaders;
				ms_simplePvsStats.m_numShadersVis_ALPHA += bVisInFwdAlpha*numShaders;
			}
			pPvsEntry++;
		}

		// draw stats
		if (ms_bDrawEntityStats)
		{
			grcDebugDraw::AddDebugOutput("Total number of entities in PVS: %d", ms_simplePvsStats.m_numEntitiesInPvs_TOTAL);
			if (pSelectedEntity)
			{
				grcDebugDraw::AddDebugOutput("Total number of occurrences of %s in PVS: %d", pSelectedEntity->GetModelName(), ms_simplePvsStats.m_numEntitiesInPvs_SELECTED);
			}
			grcDebugDraw::AddDebugOutput("Total number of entities visible (GBUF): %d", ms_simplePvsStats.m_numEntitiesVis_GBUF);
			grcDebugDraw::AddDebugOutput("Total number of entities visible (SHADOWS): %d", ms_simplePvsStats.m_numEntitiesVis_SHADOWS);
			grcDebugDraw::AddDebugOutput("Total number of entities visible (REFLECTION MAP): %d", ms_simplePvsStats.m_numEntitiesVis_REFLECTIONS);
		}
	}

	ProcessBlockReject(pPvsStart, pPvsStop);

	// copy over locked PVS list
	if (CSceneIsolator::ms_bEnabled)
	{
		if (!CSceneIsolator::ms_bLocked)
		{			
			CSceneIsolator::Start();
			CGtaPvsEntry* pPvsEntry	= pPvsStart;
			while (pPvsEntry != pPvsStop)
			{
				if (pPvsEntry && pPvsEntry->GetEntity())
				{
					CSceneIsolator::Add(pPvsEntry->GetEntity());
				}
				pPvsEntry++;
			}
			CSceneIsolator::Stop();
		}
	}

	// update scene budget tracker
	if (CSceneCostTracker::GetShowBudgets(SCENECOSTTRACKER_LIST_PVS))
	{
		g_SceneCostTracker.AddCostsForPvsList(pPvsStart, pPvsStop);
	}
	else if (CSceneCostTracker::GetShowBudgets(SCENECOSTTRACKER_LIST_SPHERE))
	{
		g_SceneCostTracker.AddCostsForSphere();
	}


	if ( CMPObjectUsage::IsActive() || DEBUGSTREAMGRAPH.ShouldShowTrackerDependentsInScene()
#if ENTITYSELECT_ENABLED_BUILD
		|| g_PickerManager.IsFlashingSelectedEntity()
#endif
		|| (CSceneIsolator::ms_bEnabled && CSceneIsolator::ms_bLocked && CSceneIsolator::ms_bOnlyShowSelected))
	{
		pPvsEntry	= pPvsStart;
		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry && pPvsEntry->GetEntity())
			{
				CEntity* pEntity = pPvsEntry->GetEntity();

				// Draw objects only used in MP
				if(CMPObjectUsage::IsActive())
				{
					if( CMPObjectUsage::ShouldDisplayOnMap(pEntity) )
					{
						spdAABB tempBox;
						const spdAABB &bbox = pEntity->GetBoundBox(tempBox);
						grcDebugDraw::BoxAxisAligned(bbox.GetMin(), bbox.GetMax(), Color32(255, 0, 0, 255), false);
					}
				}

				if( DEBUGSTREAMGRAPH.ShouldShowTrackerDependentsInScene() )
				{
					if( DEBUGSTREAMGRAPH.IsSelectedUsedByCB(pEntity) )
					{
						spdAABB tempBox;
						const spdAABB &bbox = pEntity->GetBoundBox(tempBox);
						grcDebugDraw::BoxAxisAligned(bbox.GetMin(), bbox.GetMax(), Color32(255, 0, 0, 255), false);
					}
					else if(DEBUGSTREAMGRAPH.ShouldOnlyShowTrackerDependentsInScene())
					{
						pPvsEntry->SetEntity(NULL);
					}
				}

#if ENTITYSELECT_ENABLED_BUILD		// otherwise g_RenderPhaseEntitySelectNew doesn't exist as a class.
				// flash selected
				if (g_PickerManager.IsFlashingSelectedEntity() && pEntity && g_PickerManager.GetSelectedEntity()==pEntity && fwTimer::GetSystemFrameCount()&2)
				{
					fwVisibilityFlags& scanFlags = pPvsEntry->GetVisFlags();

					// Keep the entity in the entity select phase even when we want it flashing in all phases
					if ( g_PickerManager.IsFlashingInAllPhases() )
						scanFlags.SetPhaseVisFlags( scanFlags & (1 << g_RenderPhaseEntitySelectNew->GetEntityListIndex()) );
					else
						scanFlags.ClearVisBit(g_SceneToGBufferPhaseNew->GetEntityListIndex());
				}
#endif

				// cull all except selected in locked PVS 
				if (CSceneIsolator::ms_bEnabled && CSceneIsolator::ms_bLocked && CSceneIsolator::ms_bOnlyShowSelected && CSceneIsolator::GetCurrentEntity())
				{
					CEntity* pSelected = CSceneIsolator::GetCurrentEntity();
					if (pEntity!=pSelected)
					{
						pPvsEntry->SetEntity(NULL);
					}
				}
			}
			pPvsEntry++;
		}
	}

	// object pop
	if (ms_bDisplayObjects || ms_bDisplayDummyObjects)
	{
		u32 gbufPhaseFlag = 1 << g_SceneToGBufferPhaseNew->GetEntityListIndex();
		pPvsEntry = pPvsStart;
		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry)
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				bool bVisibleInGbuf = (pPvsEntry->GetVisFlags() & gbufPhaseFlag) != 0;
				if (pEntity && bVisibleInGbuf)
				{
					if (ms_bDisplayObjects && pEntity->GetIsTypeObject())
					{
						Vec3V vPos = pEntity->GetTransform().GetPosition();
						grcDebugDraw::Text(vPos, Color32(Color_green), pEntity->GetModelName());
						grcDebugDraw::Line(vPos, vPos+Vec3V(0.0f, 0.0f, 15.0f), Color32(Color_green));
					}
					else if (ms_bDisplayDummyObjects && pEntity->GetIsTypeDummyObject())
					{
						Vec3V vPos = pEntity->GetTransform().GetPosition();
						grcDebugDraw::Text(vPos, Color32(Color_red), pEntity->GetModelName());
						grcDebugDraw::Line(vPos, vPos+Vec3V(0.0f, 0.0f, 15.0f), Color32(Color_red));
					}
				}
			}
			pPvsEntry++;
		}
	}

	// breakable glass stats
	if (bgGlassManager::GetShowVisibleBreakableGlassEntities())
	{
		pPvsEntry	= pPvsStart;
		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry && pPvsEntry->GetEntity() && pPvsEntry->GetEntity()->GetIsDynamic())
			{
				CDynamicEntity* pDynEntity = static_cast<CDynamicEntity*>(pPvsEntry->GetEntity());

				if( pDynEntity->m_nDEflags.bIsBreakableGlass )
				{
					spdAABB tempBox;
					const spdAABB &bbox = pDynEntity->GetBoundBox(tempBox);
					grcDebugDraw::BoxAxisAligned(bbox.GetMin(), bbox.GetMax(), Color32(255, 0, 0, 255), false);
				}
			}
		}
		pPvsEntry++;
	}

	CMapOptimizeHelper::UpdateFromPvs(pPvsStart, pPvsStop);

	CEntityReportGenerator::Update(pPvsStart, pPvsStop);

	if ( g_PickerManager.GetShowHideMode() == PICKER_ISOLATE_SELECTED_ENTITY )
	{
		CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
		if (pEntity)
		{
			pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ProcessBlockReject
// PURPOSE:		strip out any entities filtered by the blockview widgets
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::ProcessBlockReject(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	if ((CBlockView::FilterBlock() || CBlockView::FilterArtist() || CBlockView::FilterLodTree()) == false) { return; }

	CGtaPvsEntry* pPvsEntry	= pStart;
	while (pPvsEntry != pStop)
	{
		if (pPvsEntry && pPvsEntry->GetEntity())
		{
			CEntity* pEntity = pPvsEntry->GetEntity();
			if (pEntity && !CBlockView::ShouldDraw(pEntity))
			{
				pPvsEntry->SetEntity(NULL);
			}
		}
		pPvsEntry++;
	}
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE:		IsSizeOnScreenCulled
// PURPOSE:		return true if culled due to onscreen size
//////////////////////////////////////////////////////////////////////////
bool CPostScanDebug::IsSizeOnScreenCulled(CEntity* pEntity)
{
	if (pEntity)
	{
		fwRect screenRect;
		Vector3 vMin, vMax;
		vMin = pEntity->GetBoundingBoxMin();
		vMax = pEntity->GetBoundingBoxMax();
		const Matrix34& mat = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

		Vector3 vCorners[8] = {
			Vector3(vMin.x, vMin.y, vMin.z), Vector3(vMax.x, vMin.y, vMin.z), Vector3(vMax.x, vMax.y, vMin.z), Vector3(vMin.x, vMax.y, vMin.z),
			Vector3(vMin.x, vMin.y, vMax.z), Vector3(vMax.x, vMin.y, vMax.z), Vector3(vMax.x, vMax.y, vMax.z), Vector3(vMin.x, vMax.y, vMax.z)
		};
		for(u32 i=0; i<8; i++)
		{
			mat.Transform(vCorners[i]);
			float fScreenX, fScreenY;
			CDebugScene::GetScreenPosFromWorldPoint(vCorners[i], fScreenX, fScreenY);
			screenRect.Add(fScreenX, fScreenY);
		}
		if (screenRect.GetWidth()>ms_nScreenSizeCullMaxW || screenRect.GetHeight()>ms_nScreenSizeCullMaxH)
		{
			return ms_bScreenSizeCullReversed ? false : true;
		}
	}
	return ms_bScreenSizeCullReversed ? true : false;
}

void CountEntitiesCB(fwEntity* pEntity, void* pData)
{
	if (pData && pEntity)
	{
		u32* pCount = (u32*) pData;
		*pCount += 1;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ShowBoxEntityCount
// PURPOSE:		print the number of entities intersecting with cull box
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::ShowBoxEntityCount()
{
	u32 numEntities = 0;

	float fMinX = (float) (ms_nCullBoxX - ms_nCullBoxW);
	float fMinY = (float) (ms_nCullBoxY - ms_nCullBoxH);
	float fMinZ = (float) (ms_nCullBoxZ - ms_nCullBoxD);
	float fMaxX = (float) (ms_nCullBoxX + ms_nCullBoxW);
	float fMaxY = (float) (ms_nCullBoxY + ms_nCullBoxH);
	float fMaxZ = (float) (ms_nCullBoxZ + ms_nCullBoxD);
	spdAABB cullBox(Vec3V(fMinX, fMinY, fMinZ), Vec3V(fMaxX, fMaxY, fMaxZ));

	fwIsBoxIntersectingBB testVolume(cullBox);
	CGameWorld::ForAllEntitiesIntersecting(
		&testVolume,
		IntersectingCB(CountEntitiesCB),
		(void*) &numEntities,
		ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT,
		SEARCH_LOCATION_EXTERIORS,
		SEARCH_LODTYPE_ALL
	);
	grcDebugDraw::AddDebugOutput("Number of entities in box): %d", numEntities);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsBoxCulled
// PURPOSE:		reject any entity not intersecting with specified cull volume
//////////////////////////////////////////////////////////////////////////
bool CPostScanDebug::IsBoxCulled(CEntity* pEntity)
{
	float fMinX = (float) (ms_nCullBoxX - ms_nCullBoxW);
	float fMinY = (float) (ms_nCullBoxY - ms_nCullBoxH);
	float fMinZ = (float) (ms_nCullBoxZ - ms_nCullBoxD);
	float fMaxX = (float) (ms_nCullBoxX + ms_nCullBoxW);
	float fMaxY = (float) (ms_nCullBoxY + ms_nCullBoxH);
	float fMaxZ = (float) (ms_nCullBoxZ + ms_nCullBoxD);

	spdAABB cullBox(Vec3V(fMinX, fMinY, fMinZ), Vec3V(fMaxX, fMaxY, fMaxZ));
	grcDebugDraw::BoxAxisAligned(cullBox.GetMin(), cullBox.GetMax(), Color32(255, 0, 0, 255), false);

	if (pEntity)
	{
		spdAABB tempBox;
		const spdAABB &aabb = pEntity->GetBoundBox(tempBox);
		switch (ms_nCullBoxSelectedPolicy)
		{
		case CULLBOX_REJECTIONPOLICY_CONTAINED:
			if (!cullBox.ContainsAABB(aabb))
			{
				return true;
			}
			break;
		case CULLBOX_REJECTIONPOLICY_INTERSECTED:
			if (!cullBox.IntersectsAABB(aabb))
			{
				return true;
			}
			break;
		default:
			break;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsCulledBasedOnLodType
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
bool CPostScanDebug::IsCulledBasedOnLodType(CEntity* pEntity)
{
	if (!pEntity)
		return false;

	if (!ms_bCullOrphanHd && !ms_bCullLoddedHd && !ms_bCullLod && !ms_bCullSlod1 && !ms_bCullSlod2 && !ms_bCullSlod3 && !ms_bCullSlod4)
		return false;

	const fwLodData& lodData = pEntity->GetLodData();
	if (ms_bCullOrphanHd && lodData.IsOrphanHd())
		return true;

	if (ms_bCullLoddedHd && lodData.IsLoddedHd())
		return true;

	if (ms_bCullLod && lodData.IsLod())
		return true;

	if (ms_bCullSlod1 && lodData.IsSlod1())
		return true;

	if (ms_bCullSlod2 && lodData.IsSlod2())
		return true;

	if (ms_bCullSlod3 && lodData.IsSlod3())
		return true;

	if (ms_bCullSlod4 && lodData.IsSlod4())
		return true;

	return false;
}
//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsLodLevelCulled
// PURPOSE:		returns true if not the lod level specified by combo
//////////////////////////////////////////////////////////////////////////
bool CPostScanDebug::IsLodLevelCulled(CEntity* pEntity, bool bForceUpAlphas)
{
	bool bRetVal = false;
	if (pEntity)
	{
		switch (ms_nSelectedLodLevel)
		{
		case 0:
			bRetVal = !pEntity->GetLodData().IsHighDetail();
			break;
		case 1:
			bRetVal = (pEntity->GetLodData().GetLodType() != LODTYPES_DEPTH_LOD);
			break;
		case 2:
			bRetVal = (pEntity->GetLodData().GetLodType() != LODTYPES_DEPTH_SLOD1);
			break;
		case 3:
			bRetVal = (pEntity->GetLodData().GetLodType() != LODTYPES_DEPTH_SLOD2);
			break;
		case 4:
			bRetVal = (pEntity->GetLodData().GetLodType() != LODTYPES_DEPTH_SLOD3);
			break;
		case 5:
			bRetVal = (pEntity->GetLodData().GetLodType() != LODTYPES_DEPTH_SLOD4);
		default:
			break;
		}
		if (!bRetVal && bForceUpAlphas)
		{
			pEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
		}
	}
	return bRetVal;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	HasOnlyOneChild
// PURPOSE:		return true if entity one child
//////////////////////////////////////////////////////////////////////////
bool CPostScanDebug::HasOnlyOneChild(CEntity* pEntity)
{
	if (pEntity && pEntity->GetLodData().HasChildren())
	{
		if (pEntity->GetLodData().GetNumChildren()==1)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CentreCullBox
// PURPOSE:		reset cull box for convenience, around player ped position
//////////////////////////////////////////////////////////////////////////
void CPostScanDebug::CentreCullBox()
{
	const Vector3& vPos = camInterface::GetPos();
	ms_nCullBoxX = (s32) vPos.x;
	ms_nCullBoxY = (s32) vPos.y;
	ms_nCullBoxZ = (s32) vPos.z;
	ms_nCullBoxW = DEFAULT_CULLBOX_W;
	ms_nCullBoxH = DEFAULT_CULLBOX_H;
	ms_nCullBoxD = DEFAULT_CULLBOX_D;
}

//////////////////////////////////////////////////////////////////////////
// CPostScanSqlDumper stuff
//////////////////////////////////////////////////////////////////////////

static char			s_buffer[2048];
static const char*	s_beginTransactionStatement = "BEGIN TRANSACTION;\n";
static const char*	s_commitStatement = "COMMIT;\n";
static const char*	s_createTableStatement = "\
CREATE TABLE pvs						\
(										\
	capture_id INTEGER,					\
	ptr INTEGER,						\
	type TEXT,							\
	model TEXT,							\
	lod_type TEXT,						\
	x FLOAT,							\
	y FLOAT,							\
	z FLOAT,							\
	alpha INTEGER,						\
	camera_dist FLOAT,					\
	vis_gbuf BOOLEAN,					\
	vis_csm BOOLEAN,					\
	vis_reflection BOOLEAN,				\
	vis_cascade_0 BOOLEAN,				\
	vis_cascade_1 BOOLEAN,				\
	vis_cascade_2 BOOLEAN,				\
	vis_cascade_3 BOOLEAN,				\
	vis_reflect_upper_lolod BOOLEAN,	\
	vis_reflect_lower_lolod BOOLEAN,	\
	vis_reflect_upper_hilod BOOLEAN,	\
	vis_reflect_lower_hilod BOOLEAN,	\
	vis_hd_stream BOOLEAN,				\
	vis_stream_volume BOOLEAN,			\
	is_tree BOOLEAN						\
);\n";

void CPostScanSqlDumper::OpenSqlFile()
{
	ms_dumpFile = fiStream::Create( ms_filename );
	Assertf( ms_dumpFile, "Couldn't open file for SQL dump" );
	ms_dumpFile->Write( s_beginTransactionStatement, istrlen(s_beginTransactionStatement) );

	if ( ms_emitCreateTableStatement )
		ms_dumpFile->Write( s_createTableStatement, istrlen(s_createTableStatement) );
	
	for (int i = 0; i < 24; ++i)
	{
		CRenderPhase*	renderPhase = (CRenderPhase*) gRenderListGroup.GetRenderListForPhase(i).GetRenderPhaseNew();
		if ( renderPhase )
		{
			switch ( renderPhase->GetDrawListType() )
			{
				case DL_RENDERPHASE_GBUF: ms_gbufPhaseId = i; break;
				case DL_RENDERPHASE_CASCADE_SHADOWS: ms_csmPhaseId = i; break;
				case DL_RENDERPHASE_REFLECTION_MAP: ms_extReflectionPhaseId = i; break;
				default: break;
			}
		}
	}

	ms_hdStreamPhaseId = CGameScan::GetHdStreamVisibilityBit();
	ms_streamVolumePhaseId = CGameScan::GetStreamVolumeVisibilityBit1();
}

void CPostScanSqlDumper::CloseSqlFile()
{
	ms_dumpFile->Write( s_commitStatement, istrlen(s_commitStatement) );
	ms_dumpFile->Flush();
	ms_dumpFile->Close();
	ms_dumpFile = NULL;

	ms_dumpEnabled = false;
}

void CPostScanSqlDumper::EnableDump() {
	ms_dumpEnabled = true;
}

void CPostScanSqlDumper::AddWidgets(bkBank* bank)
{
	bank->PushGroup( "PostScan SQL Dumper" );
	bank->AddText( "File name", ms_filename, 512 );
	bank->AddSlider( "Capture ID", &ms_captureId, 0, 100, 1 );
	bank->AddToggle( "Emit CREATE TABLE statement", &ms_emitCreateTableStatement );
	bank->AddButton( "Dump", CPostScanSqlDumper::EnableDump );
	bank->PopGroup();
}

void CPostScanSqlDumper::DumpPvsEntry(const CGtaPvsEntry* entry)
{
	const CEntity*	entity = entry->GetEntity();
	const Vec3V		position = entity->GetTransformPtr()->GetPosition();
	const u32		subphaseFlags = entry->GetVisFlags().GetSubphaseVisFlags();

	const int		capture_id = ms_captureId;
	const size_t	ptr = size_t(entity);
	const char*		type = CDebugScene::GetEntityDescription( entity );
	const char*		model = entity->GetModelName();
	const char*		lod_type = entity->GetLodData().GetLodTypeName();
	const float		x = position.GetXf();
	const float		y = position.GetYf();
	const float		z = position.GetZf();
	const int		alpha = entity->GetAlpha();
	const float		camera_dist = entry->GetDist();
	const bool		vis_gbuf = entry->GetVisFlags().GetVisBit( ms_gbufPhaseId );
	const bool		vis_csm = entry->GetVisFlags().GetVisBit( ms_csmPhaseId );
	const bool		vis_ext_reflection = entry->GetVisFlags().GetVisBit( ms_extReflectionPhaseId );
	const int		cascadeVisFlags = subphaseFlags >> SUBPHASE_CASCADE_0;
	const bool		vis_cascade_0 = ( cascadeVisFlags & (1 << 0) ) != 0;
	const bool		vis_cascade_1 = ( cascadeVisFlags & (1 << 1) ) != 0;
	const bool		vis_cascade_2 = ( cascadeVisFlags & (1 << 2) ) != 0;
	const bool		vis_cascade_3 = ( cascadeVisFlags & (1 << 3) ) != 0;
	const bool		vis_reflect_upper_lolod = ( subphaseFlags & BIT(SUBPHASE_REFLECT_UPPER_LOLOD) ) ? true : false;
	const bool		vis_reflect_lower_lolod = ( subphaseFlags & BIT(SUBPHASE_REFLECT_LOWER_LOLOD) ) ? true : false;
	const bool		vis_reflect_upper_hilod = ( subphaseFlags & BIT(SUBPHASE_REFLECT_UPPER_HILOD) ) ? true : false;
	const bool		vis_reflect_lower_hilod = ( subphaseFlags & BIT(SUBPHASE_REFLECT_LOWER_HILOD) ) ? true : false;
	const bool		vis_hd_stream = entry->GetVisFlags().GetVisBit( ms_hdStreamPhaseId );
	const bool		vis_stream_volume = entry->GetVisFlags().GetVisBit( ms_streamVolumePhaseId );
	const bool		is_tree = entity && entity->GetDrawHandlerPtr() && entity->GetDrawHandlerPtr()->GetShaderEffect() && entity->GetDrawHandlerPtr()->GetShaderEffect()->GetType() == CSE_TREE;

	sprintf( s_buffer, "INSERT INTO pvs VALUES (%d, %" PTRDIFFTFMT "u, '%s', '%s', '%s', %f, %f, %f, %d, %f, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d);\n",
		capture_id, ptr, type, model, lod_type, x, y, z, alpha, camera_dist, vis_gbuf, vis_csm, vis_ext_reflection,
		vis_cascade_0, vis_cascade_1, vis_cascade_2, vis_cascade_3, vis_reflect_upper_lolod, vis_reflect_lower_lolod, vis_reflect_upper_hilod, vis_reflect_lower_hilod,
		vis_hd_stream, vis_stream_volume, is_tree );

	ms_dumpFile->Write( s_buffer, istrlen(s_buffer) );
	ms_dumpFile->Flush();
}

void CPostScanDebug::AddToAlwaysPreRenderListCB()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		gPostScan.AddToAlwaysPreRenderList(pEntity);
	}
}

void CPostScanDebug::RemoveFromAlwaysPreRenderListCB()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		gPostScan.RemoveFromAlwaysPreRenderList(pEntity);
	}
}

class CObjectCount
{
public:
	CObjectCount() {}
	CObjectCount(u32 nameHash, u32 instCount) : m_nameHash(nameHash), m_instCount(instCount) {}
	u32 m_nameHash, m_instCount;
	static s32 CompareCB(const CObjectCount* pObj1, const CObjectCount* pObj2)
	{
		if (pObj1->m_instCount != pObj2->m_instCount)
		{
			return pObj2->m_instCount - pObj1->m_instCount;
		}
		return pObj2->m_nameHash - pObj1->m_nameHash;
	}
};

void CPostScanDebug::DisplayObjectPoolSummary()
{
	CObject::Pool* ObjPool = CObject::GetPool();
	CObject* pObj;
	s32 i = (s32) ObjPool->GetSize();

	u32 numObjects = 0;
	u32 numMapObjects = 0;

	atMap<u32, u32> usageMap;

	while(i--)
	{
		pObj = ObjPool->GetSlot(i);
		if (pObj)
		{
			numObjects++;

			CDummyObject* pDummy = pObj->GetRelatedDummy();
			if (pDummy)
			{
				numMapObjects++;
			}

			CBaseModelInfo* pModelInfo = pObj->GetBaseModelInfo();
			if (pModelInfo)
			{
				u32 nameHash = pModelInfo->GetModelNameHash();

				const u32* pResult = usageMap.Access(nameHash);
				if (!pResult)
				{		
					usageMap.Insert(nameHash, 1);
				}
				else
				{
					u32 newCount = (*pResult) + 1;
					usageMap.Delete(nameHash);
					usageMap.Insert(nameHash, newCount);
				}
			}
		}
	}

	grcDebugDraw::AddDebugOutput("Num objects=%d, num from mapdata=%d", numObjects, numMapObjects);

	atArray<CObjectCount> instCountArray;

	atMap<u32, u32>::Iterator iter = usageMap.CreateIterator();
	while (iter) { instCountArray.PushAndGrow(CObjectCount(iter.GetKey(), iter.GetData())); ++iter; }
	instCountArray.QSort(0, -1, CObjectCount::CompareCB);

	for (s32 i=0; i<instCountArray.GetCount(); i++)
	{
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(instCountArray[i].m_nameHash, NULL);
		grcDebugDraw::AddDebugOutput("%s: %d", pModelInfo->GetModelName(), instCountArray[i].m_instCount);		
	}
}

void CPostScanDebug::DisplayDummyDebugOutput()
{
	CDummyObject::Pool* pool = CDummyObject::GetPool();
	s32 i = pool->GetSize();

	u32 numDummies = 0;
	float closestDummy = 999999.f;

	Vector3 vecCentre = CFocusEntityMgr::GetMgr().GetPos();
	sCloseDummy* closeDummies = CObjectPopulation::GetCloseDummiesArray();

	while(i--)
	{
		CDummyObject* dummy = pool->GetSlot(i);
		if (dummy && dummy->GetIsVisible() && dummy->GetIsAddedToWorld())
		{
			bool foundIt = false;
			for (s32 f = 0; f < NUM_CLOSE_DUMMIES_PER_FRAME; ++f)
			{
				if (closeDummies[f].ent == dummy)
				{
					foundIt = true;
					break;
				}
			}

			const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(dummy->GetTransform().GetPosition());
			Vector3 vecTargetObject = vObjectPosition - vecCentre;
			float fDistanceSqr = vecTargetObject.Mag2();

			if (fDistanceSqr < OBJECT_POPULATION_RESET_RANGE_SQR)
				numDummies++;

			// only regard it as potentially closest dummy if it isn't part of the closest array
			if (!foundIt)
			{
				if (fDistanceSqr < closestDummy)
					closestDummy = fDistanceSqr;
			}
		}
	}

	closestDummy = Sqrtf(closestDummy);
	{
		static u32 radiusMin = 999999;
		static u32 radiusMax = 0;
		static float closestMin = 999999.f;
		static float closestMax = 0.f;
		radiusMin = rage::Min(radiusMin, numDummies);
		radiusMax = rage::Max(radiusMax, numDummies);
		closestMin = rage::Min(closestMin, closestDummy);
		closestMax = rage::Max(closestMax, closestDummy);
		grcDebugDraw::AddDebugOutput("Num dummies = %d, in radius = %d[%d, %d], closest = %.2f[%.2f, %.2f]", pool->GetNoOfUsedSpaces(), numDummies, radiusMin, radiusMax, closestDummy, closestMin, closestMax);
	}

	u32 numCloseDummies = 0;
	float closest = 999999.f;
	float farthest = 0.f;
	for (s32 i = 0; i < NUM_CLOSE_DUMMIES_PER_FRAME; ++i)
	{
		if (closeDummies[i].ent)
		{
			numCloseDummies++;
			if (closeDummies[i].dist < closest)
				closest = closeDummies[i].dist;
			if (closeDummies[i].dist > farthest)
				farthest = closeDummies[i].dist;
		}
	}

	{
		static float closestMin = 999999.f;
		static float closestMax = 0.f;
		static float farthestMin = 999999.f;
		static float farthestMax = 0.f;
		closestMin = rage::Min(closestMin, closest);
		if (closest < 999999.f)
			closestMax = rage::Max(closestMax, closest);
		if (farthest > 0.f)
			farthestMin = rage::Min(farthestMin, farthest);
		farthestMax = rage::Max(farthestMax, farthest);
		grcDebugDraw::AddDebugOutput("Closest dummies = %d, closest = %.2f[%.2f, %.2f], farthest = %.2f[%.2f, %.2f]", numCloseDummies, closest, closestMin, closestMax, farthest, farthestMin, farthestMax);
	}
}

bool CPostScanDebug::IsInSelectedLodTree(CEntity* pEntity)
{
	CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();

	if (!pSelectedEntity)
	{
		return false;
	}
	else if (pSelectedEntity && pEntity && !pEntity->GetLodData().IsOrphanHd() && !pSelectedEntity->GetLodData().IsOrphanHd())
	{		
		return (pSelectedEntity == pEntity) ? true : ( pSelectedEntity->GetRootLod() == pEntity->GetRootLod() );
	}
	return false;	
}

void CPostScanDebug::DrawLodRanges()
{
	if (!ms_bDrawLodRanges) { return; }

	CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();

	if (pSelectedEntity)
	{
		Vector3 vPos;
		g_LodMgr.GetBasisPivot(pSelectedEntity, vPos);
		float fRadius = (float) pSelectedEntity->GetLodDistance() + LODTYPES_FADE_DISTF;
		grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vPos), fRadius, Color32(255,0,0,80), false, 1, 64);
	}
}

void CPostScanDebug::GetModelNameFromPickerCB()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		sprintf(ms_oldModelName, "%s", pEntity->GetModelName());
	}
}

void CPostScanDebug::GetNewModelNameFromPickerCB()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		sprintf(ms_newModelName, "%s", pEntity->GetModelName());
	}
}

void CPostScanDebug::CreateNewModelSwapCB()
{
	u32 oldHash = atStringHash(ms_oldModelName);
	u32 newHash = atStringHash(ms_newModelName);
	float fRadius = (float) ms_modelSwapRadius;

	const Vector3& vPos = camInterface::GetPos();
	spdSphere searchVolume(VECTOR3_TO_VEC3V(vPos), ScalarV(fRadius));

	g_MapChangeMgr.Add(oldHash, newHash, searchVolume, true, CMapChange::TYPE_SWAP, ms_bLazySwap, ms_bIncludeScriptObjects);
}

void CPostScanDebug::CreateNewModelHideCB()
{
	u32 oldHash = atStringHash(ms_oldModelName);
	float fRadius = (float) ms_modelSwapRadius;

	const Vector3& vPos = camInterface::GetPos();
	spdSphere searchVolume(VECTOR3_TO_VEC3V(vPos), ScalarV(fRadius));

	g_MapChangeMgr.Add(oldHash, oldHash, searchVolume, true, CMapChange::TYPE_HIDE, ms_bLazySwap, ms_bIncludeScriptObjects);
}

void CPostScanDebug::CreateNewForceToObjectCB()
{
	u32 oldHash = atStringHash(ms_oldModelName);
	float fRadius = (float) ms_modelSwapRadius;

	const Vector3& vPos = camInterface::GetPos();
	spdSphere searchVolume(VECTOR3_TO_VEC3V(vPos), ScalarV(fRadius));

	g_MapChangeMgr.Add(oldHash, oldHash, searchVolume, true, CMapChange::TYPE_FORCEOBJ, ms_bLazySwap, ms_bIncludeScriptObjects);
}

void CPostScanDebug::DeleteModelSwapCB()
{
	if (ms_modelSwapIndex >= 0)
		g_MapChangeMgr.DbgRemoveByIndex(ms_modelSwapIndex, ms_bLazySwap);
}

void CPostScanDebug::ProcessBeforeAllPreRendering()
{
	if (ms_bDebugSelectedEntityPreRender)
	{
		CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
		if (pSelectedEntity)
		{
			ms_bSelectedEntityWasPreRenderedThisFrame = false;
		}
	}
}

void CPostScanDebug::ProcessPreRenderEntity(CEntity* pEntity)
{
	if (ms_bDebugSelectedEntityPreRender)
	{
		CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
		if (pEntity && pEntity==pSelectedEntity)
		{
			ms_bSelectedEntityWasPreRenderedThisFrame = true;
		}
	}
}

void CPostScanDebug::ProcessAfterAllPreRendering()
{
	if (ms_bDebugSelectedEntityPreRender)
	{
		CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();

		if (ms_bSelectedEntityWasPreRenderedThisFrame)
		{
			if (ms_preRenderFailTimer) { ms_preRenderFailTimer--; }
		}
		else
		{
			ms_preRenderFailTimer = 100;
		}

		if (ms_preRenderFailTimer && pSelectedEntity)
		{
			grcDebugDraw::AddDebugOutput(Color_red, "%s was not pre-rendered recently %d", pSelectedEntity->GetModelName(), ms_preRenderFailTimer);
		}
	}
}

void CPostScanDebug::ResetAlphaCB()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		pEntity->GetLodData().SetResetAlpha(true);
	}
}

void CPostScanDebug::AdjustAlphaBeforeLodForce(CEntity* pEntity)
{
	if (pEntity && ms_bAdjustLodData)
	{
		fwLodData& lodData = pEntity->GetLodData();
		lodData.SetAlpha(ms_adjustLodData_Alpha);
		lodData.SetLoaded(ms_bAdjustLodData_LoadedFlag);
	}
}

void CPostScanDebug::SetOverrideGBufBits(const char* renderPhaseName)
{
	for (int i = 0; i < MAX_NUM_RENDER_PHASES; ++i)
	{
		const fwRenderPhase* pRenderPhase = gRenderListGroup.GetRenderListForPhase(i).GetRenderPhaseNew();

		if ( pRenderPhase && stricmp(pRenderPhase->GetName(), renderPhaseName) == 0 )
		{
			ms_nOverrideGBufBitsPhase = i;
			UpdateOverrideGBufVisibilityType();
			break;
		}
	}
}

int CPostScanDebug::GetOverrideGBufVisibilityType()
{
	return ms_nOverrideGBufVisibilityType;
}

void CPostScanDebug::UpdateOverrideGBufVisibilityType()
{
	const fwRenderPhase* pRenderPhase = gRenderListGroup.GetRenderListForPhase(ms_nOverrideGBufBitsPhase).GetRenderPhaseNew();

	if (pRenderPhase)
	{
		ms_nOverrideGBufVisibilityType = pRenderPhase->GetVisibilityType();
	}
	else
	{
		ms_nOverrideGBufVisibilityType = -1;
	}
}

void CPostScanDebug::ProcessVisibilityStreamingCullshapes(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	if (!ms_bDrawHdSphere && !ms_bDrawLodSphere && !ms_bDrawInteriorSphere)
		return;

	//////////////////////////////////////////////////////////////////////////
	// draw cull shapes
	Vec3V vHdPos = g_SceneStreamerMgr.GetHdStreamSphereCentre();
	Vec3V vLodPos = g_SceneStreamerMgr.GetLodStreamSphereCentre();
	Vec3V vIntPos = g_SceneStreamerMgr.GetIntStreamSphereCentre();
	const float fHdRadius = g_SceneStreamerMgr.GetHdStreamSphereRadius();
	const float fLodRadius = g_SceneStreamerMgr.GetLodStreamSphereRadius();
	const float fIntRadius = g_SceneStreamerMgr.GetIntStreamSphereRadius();

	if (ms_bDrawHdSphere)
	{
		grcDebugDraw::Sphere( vHdPos, fHdRadius, Color32(255,0,0,80), false, 1, 64 );
	}
	if (ms_bDrawLodSphere)
	{
		grcDebugDraw::Sphere( vLodPos, fLodRadius, Color32(0,255,0,80), false, 1, 64 );
	}
	if (ms_bDrawInteriorSphere)
	{
		grcDebugDraw::Sphere( vIntPos, fIntRadius, Color32(0,0,255,80), false, 1, 64 );
	}

	//////////////////////////////////////////////////////////////////////////
	// draw entity counts
	if (ms_bDrawHdSphereCounts)
	{
		const u32 visMask = ( 1 << CGameScan::GetHdStreamVisibilityBit() );
		u32 numVisible = 0;
		CGtaPvsEntry* pPvsEntry = pStart;
		while (pPvsEntry != pStop)
		{
			if ( pPvsEntry->GetEntity() && (pPvsEntry->GetVisFlags()&visMask) )
			{
				numVisible++;
			}
			pPvsEntry++;
		}
		grcDebugDraw::AddDebugOutput("HD sphere position (%4.2f, %4.2f, %4.2f)", vHdPos.GetXf(), vHdPos.GetYf(), vHdPos.GetZf());
		grcDebugDraw::AddDebugOutput("PVS contains: %d entities for HD sphere (radius %4.2f)", numVisible, fHdRadius);
	}
	if (ms_bDrawLodSphereCounts)
	{
		const u32 visMask = ( 1 << CGameScan::GetLodStreamVisibilityBit() );
		u32 numVisible = 0;
		CGtaPvsEntry* pPvsEntry = pStart;
		while (pPvsEntry != pStop)
		{
			if ( pPvsEntry->GetEntity() && (pPvsEntry->GetVisFlags()&visMask) )
			{
				numVisible++;
			}
			pPvsEntry++;
		}
		grcDebugDraw::AddDebugOutput("LOD sphere position (%4.2f, %4.2f, %4.2f)", vLodPos.GetXf(), vLodPos.GetYf(), vLodPos.GetZf());
		grcDebugDraw::AddDebugOutput("PVS contains: %d entities for LOD sphere (radius %4.2f)", numVisible, fLodRadius);
	}
	if (ms_bDrawInteriorSphereCounts)
	{
		const u32 visMask = ( 1 << CGameScan::GetInteriorStreamVisibilityBit() );
		u32 numVisible = 0;
		CGtaPvsEntry* pPvsEntry = pStart;
		while (pPvsEntry != pStop)
		{
			if ( pPvsEntry->GetEntity() && (pPvsEntry->GetVisFlags()&visMask) )
			{
				numVisible++;
			}
			pPvsEntry++;
		}
		grcDebugDraw::AddDebugOutput("Interior sphere position (%4.2f, %4.2f, %4.2f)", vIntPos.GetXf(), vIntPos.GetYf(), vIntPos.GetZf());
		grcDebugDraw::AddDebugOutput("PVS contains: %d entities for interior sphere (radius %4.2f)", numVisible, fIntRadius);
	}

	//////////////////////////////////////////////////////////////////////////
	// list entities
	if (ms_bDrawHdSphereList)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3( vHdPos );
		const u32 visMask = ( 1 << CGameScan::GetHdStreamVisibilityBit() );
		CGtaPvsEntry* pPvsEntry = pStart;
		while (pPvsEntry != pStop)
		{
			if ( pPvsEntry->GetEntity() && (pPvsEntry->GetVisFlags()&visMask) )
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				Vector3 vBasisPivot;
				g_LodMgr.GetBasisPivot(pEntity, vBasisPivot);
				const float fDist = (vBasisPivot - vPos).Mag();
				grcDebugDraw::AddDebugOutput("HDsphere: name=%s level=%s loddist=%d dist=%4.2f",
					pEntity->GetModelName(),
					pEntity->GetLodData().GetLodTypeName(),
					pEntity->GetLodDistance(),
					fDist
				);
			}
			pPvsEntry++;
		}
	}
	if (ms_bDrawLodSphereList)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3( vLodPos );
		const u32 visMask = ( 1 << CGameScan::GetLodStreamVisibilityBit() );
		CGtaPvsEntry* pPvsEntry = pStart;
		while (pPvsEntry != pStop)
		{
			if ( pPvsEntry->GetEntity() && (pPvsEntry->GetVisFlags()&visMask) )
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				Vector3 vBasisPivot;
				g_LodMgr.GetBasisPivot(pEntity, vBasisPivot);
				const float fDist = (vBasisPivot - vPos).Mag();
				grcDebugDraw::AddDebugOutput("LODsphere: name=%s level=%s loddist=%d dist=%4.2f",
					pEntity->GetModelName(),
					pEntity->GetLodData().GetLodTypeName(),
					pEntity->GetLodDistance(),
					fDist
					);
			}
			pPvsEntry++;
		}
	}
	if (ms_bDrawInteriorSphereList)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3( vIntPos );
		const u32 visMask = ( 1 << CGameScan::GetInteriorStreamVisibilityBit() );
		CGtaPvsEntry* pPvsEntry = pStart;
		while (pPvsEntry != pStop)
		{
			if ( pPvsEntry->GetEntity() && (pPvsEntry->GetVisFlags()&visMask) )
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				Vector3 vBasisPivot;
				g_LodMgr.GetBasisPivot(pEntity, vBasisPivot);
				const float fDist = (vBasisPivot - vPos).Mag();
				grcDebugDraw::AddDebugOutput("Intsphere: name=%s level=%s loddist=%d dist=%4.2f",
					pEntity->GetModelName(),
					pEntity->GetLodData().GetLodTypeName(),
					pEntity->GetLodDistance(),
					fDist
				);
			}
			pPvsEntry++;
		}
	}
}

void CPostScanDebug::DisplayAlphaAndVisibilityBlameInfoForSelected()
{
#if __BANK

	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		// wrong interior location?
		const fwInteriorLocation loc = pEntity->GetInteriorLocation();

		CRenderPhaseDeferredLighting_SceneToGBuffer* gbufPhase = (CRenderPhaseDeferredLighting_SceneToGBuffer*) g_SceneToGBufferPhaseNew;
		CInteriorInst*		gbufInterior = gbufPhase->GetPortalVisTracker()->GetInteriorInst();
		s32					gbufRoom = gbufPhase->GetPortalVisTracker()->m_roomIdx;
		fwInteriorLocation	gbufInteriorLocation = CInteriorInst::CreateLocation( gbufInterior, gbufRoom );

		if (!gbufInteriorLocation.IsSameLocation(loc))
		{
			grcDebugDraw::AddDebugOutput("VIS: entity %s has different interior location from camera", pEntity->GetModelName());
			if (loc.IsValid())
			{
				CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(loc);
				if (loc.IsAttachedToPortal())
				{
					grcDebugDraw::AddDebugOutput("VIS: entity %s is in interior=%s (%d) portalIndex=%d",
						pEntity->GetModelName(), pProxy->GetName().GetCStr(),
						loc.GetInteriorProxyIndex(), loc.GetPortalIndex());
				}
				else
				{
					grcDebugDraw::AddDebugOutput("VIS: entity %s is in interior=%s (%d) roomIndex=%d",
						pEntity->GetModelName(), pProxy->GetName().GetCStr(),
						loc.GetInteriorProxyIndex(), loc.GetRoomIndex());
				}
			}
			else
			{
				grcDebugDraw::AddDebugOutput("VIS: entity %s is in exterior", pEntity->GetModelName());
			}

			if (gbufInteriorLocation.IsValid())
			{
				if (gbufInteriorLocation.IsAttachedToPortal())
				{
					grcDebugDraw::AddDebugOutput("VIS: camera is in interior=%s (%d) portalIndex=%d",
						gbufInterior->GetModelName(),
						gbufInteriorLocation.GetInteriorProxyIndex(), gbufInteriorLocation.GetPortalIndex());
				}
				else
				{
					grcDebugDraw::AddDebugOutput("VIS: camera is in interior=%s (%d) roomIndex=%d",
						gbufInterior->GetModelName(),
						gbufInteriorLocation.GetInteriorProxyIndex(), gbufInteriorLocation.GetRoomIndex());
				}
			}
			else
			{
				grcDebugDraw::AddDebugOutput("VIS: camera is in exterior");
			}
		}

		// alpha override list?
		if (CPostScan::IsOnAlphaOverrideList(pEntity))
		{
			grcDebugDraw::AddDebugOutput("VIS: entity %s has alpha %d overridden by %s", pEntity->GetModelName(), pEntity->GetAlpha(), CPostScan::GetAlphaOverrideOwner(pEntity) );
		}

		// network alpha override?

	}

#endif	//__BANK
}

void CPostScanDebug::GetForceObjPosFromCameraCB()
{
	ms_vForceObjPos = VECTOR3_TO_VEC3V( camInterface::GetPos() );
}

void CPostScanDebug::ClearAreaOfObjectsCB()
{
	scrVector scrVecCoors(camInterface::GetPos().x, camInterface::GetPos().y, camInterface::GetPos().z);
	misc_commands::CommandClearAreaOfObjects(scrVecCoors, ms_fClearAreaObjRadius, ms_ClearAreaFlag);
}

void CPostScanDebug::DisplayTimedModelInfo()
{
	CEntity* pSelected = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pSelected)
	{
		CEntity* pEntity = pSelected;

		while (pEntity)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if (pModelInfo && pModelInfo->GetModelType()==MI_TYPE_TIME) 
			{
				const u32 noiseFactor = (( pEntity->GetLodData().IsOrphanHd() ? 0 : g_LodMgr.GetHierarchyNoiseFactor(pEntity) ) >> 2) & 31;

				CTimeInfo* pTimeInfo = pModelInfo->GetTimeInfo();
				grcDebugDraw::AddDebugOutput("Timed model: lod=%s name=%s mustSwapOffScreen=%s noise=%d",
					pEntity->GetLodData().GetLodTypeName(),
					pEntity->GetModelName(),
					( pTimeInfo->OnlySwapWhenOffScreen() ? "Y" : "N" ),
					noiseFactor
				);

				// edge case - check if it is enabled all day long
				bool bActiveAllHours = true;
				for (s32 i=0; i<24; i++)
				{
					if (!pTimeInfo->IsOn(i))
					{
						bActiveAllHours = false;
						break;
					}
				}

				// print out hours of operation
				if (bActiveAllHours)
				{
					grcDebugDraw::AddDebugOutput("... enabled at all times");
				}
				else
				{
					// find first transition from off to on
					s32 firstActiveHour = -1;
					for (s32 i=0; i<24; i++)
					{
						const s32 previousHour = i==0 ? 23 : i - 1;
						if (pTimeInfo->IsOn(i) && !pTimeInfo->IsOn(previousHour))
						{
							firstActiveHour = i;
							break;
						}
					}

					// get all the ranges
					if (firstActiveHour != -1)
					{
						bool bOn = false;
						s32 startHour = 0;

						s32 cursor;

						for (s32 i=0; i<24; i++)
						{
							cursor = (firstActiveHour + i) % 24;

							if (pTimeInfo->IsOn(cursor))
							{
								if (!bOn)
								{
									bOn = true;
									startHour = cursor;
								}
							}
							else
							{
								if (bOn)
								{
									bOn = false;
									grcDebugDraw::AddDebugOutput("... enabled %02d:00 to %02d:00 (subtract %d mins for noise factor)",
										startHour, cursor, noiseFactor );
								}
							}
						}
					}
				}
				//////////////////////////////////////////////////////////////////////////			
				
				
			}

			pEntity = (CEntity*) pEntity->GetLod();
		}
	}
}

#endif //__BANK
