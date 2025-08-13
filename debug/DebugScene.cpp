//
// name:		DebugScene
// description:	Debug Functions relating to the scene
#include "debug/debugscene.h"

#if !__FINAL

// rage headers
#include "crskeleton/skeleton.h" 
#include "fragment/typechild.h"
#include "fragment/tune.h"
#include "grcore/debugdraw.h"
#include "grcore/stateblock.h"
#include "grprofile/timebars.h"
#include "rmcore/drawable.h"
#include "phbound/BoundBox.h"
#include "phbound/BoundBVH.h"
#include "phbound/BoundComposite.h"
#include "phbound/BoundGeom.h"
#include "phbound/BoundSphere.h"
#include "pheffects/mouseinput.h"
#include "physics/inst.h"
#include "physics/debugPlayback.h"
#include "profile/rocky.h"
#include "spatialdata/aabb.h"
#include "string/stringutil.h"

// framework headers
#include "ai/task/taskspew.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/txdstore.h" 
#include "fwscene/search/SearchVolumes.h"
#include "fwsys/metadatastore.h"

// game headers
#include "ai/BlockingBounds.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/debug/AnimViewer.h"
#include "Animation/debug/AnimPlacementTool.h"
#include "audio/ambience/ambientaudioentity.h"
#include "audio/collisionaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/ViewportManager.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "control/WaypointRecording.h"
#include "core/game.h"
#include "debug/debugglobals.h"
#include "debug/DebugScene.h"
#include "debug/DebugDraw/DebugWindow.h"
#include "debug/gtapicker.h"
#include "Debug/UiGadget/UiColorScheme.h"
#include "Debug/UiGadget/UIGadgetInspector.h"
#include "debug/textureviewer/textureviewerwindow.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/vectormap.h"
#include "event/ShockingEvents.h"
#include "game/dispatch/DispatchManager.h"
#include "game/Dispatch/IncidentManager.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "network/Debug/NetworkDebug.h"
#include "network/General/NetworkColours.h"
#include "network/NetworkInterface.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetworkPlayerMgr.h"
#include "objects/CoverTuning.h"
#include "objects/Door.h" 
#include "objects/DoorTuning.h" 
#include "objects/object.h" 
#include "Objects/DummyObject.h"
#include "objects/ProcObjects.h"
#include "pathserver/NavEdit.h"
#include "ik/IkManager.h"
#include "peds/PedCapsule.h"
#include "peds/peddebugvisualiser.h"
#include "peds/ped.h"
#include "peds/pedfactory.h"
#include "peds/pedintelligence.h"
#include "peds/PedDebugVisualiser.h"
#include "peds/PedMoveBlend/pedmoveblendonfoot.h"
#include "peds/pedpopulation.h"
#include "peds/PlayerInfo.h"
#include "peds/rendering/PedVariationDebug.h"
#include "Peds/PedIntelligence/PedAILodManager.h"
#include "pedgroup/formation.h"
#include "pedgroup/pedgroup.h"
#include "pathserver/pathserver.h"
#include "physics/GtaArchetype.h"
#include "physics/gtainst.h"
#include "physics/iterator.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/Pickup.h"
#include "pickups/PickupPlacement.h"
#include "renderer/occlusion.h"
#include "renderer/water.h"
#include "renderer/renderer.h"
#include "renderer/lights/lights.h"
#include "scene/animatedbuilding.h"
#include "scene/debug/SceneCostTracker.h"
#include "scene/debug/SceneIsolator.h"
#include "scene/entity.h"
#include "scene/entities/compEntity.h"
#include "scene/EntityTypes.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/LodDrawable.h"
#include "scene/entities/compEntity.h"
#include "scene/portals/InteriorGroupManager.h"
#include "scene/portals/portalInst.h"
#include "scene/RegdRefTypes.h"
#include "scene/texLod.h"
#include "scene/world/gameWorld.h" 
#include "scene/world/WorldDebugHelper.h"
#include "scene/scene.h"
#include "scene/worldpoints.h"
#include "script/script.h"
#include "streaming/PrioritizedClipSetStreamer.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebuggraph.h"
#include "system/AutoGPUCapture.h"
#include "system/controlmgr.h"
#include "system/timer.h"
#include "Task/Combat/CombatManager.h"
#include "task/Combat/TacticalAnalysis.h"
#include "task/Crimes/RandomEventManager.h"
#include "task/Default/Patrol/PatrolRoutes.h"
#include "task/Response/TaskFlee.h"
#include "task/Movement/Climbing/ClimbDebug.h"
#include "task/Movement/Climbing/DropDownDetector.h"
#include "task/Movement/TaskCollisionResponse.h"
#include "task/Movement/TaskParachute.h"
#include "task/Physics/NmDebug.h"
#include "task/Scenario/ScenarioDebug.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/buses.h"
#include "vehicleAi/junctions.h"
#include "vehicleAi/VehicleAILodManager.h"
#include "vehicleAi/JunctionEditor.h"
#include "vehicles/train.h"
#include "weapons/weapondebug.h"
#include "debug/debug.h"
#include "ai/debug/types/VehicleDebugInfo.h"
#include "streaming/streamingvisualize.h"


AI_OPTIMISATIONS ()

namespace rage
{
#if __PFDRAW
	EXT_PFD_DECLARE_GROUP(Physics);
	EXT_PFD_DECLARE_GROUP(Fragment);
#endif
}

CEntity * g_pFocusEntity = NULL;

// static CEntity *gpLookingAtEntity = NULL;
#if __BANK
// static CEntity *gpClosestToEntity = NULL;
bool		CDebugScene::sm_VisualizeAutoOpenBounds										= false;
bool		CDebugScene::bDisplayDoorPersistentInfo										= false;
#endif
#if (__DEV) || (__BANK)
bool		CDebugScene::ms_bStopProcessCtrlAllEntities 								= false;
bool		CDebugScene::ms_bStopProcessCtrlAllExceptFocus0Entity 						= false;
bool		CDebugScene::ms_bStopProcessCtrlAllEntitiesOfFocus0Type						= false;
bool		CDebugScene::ms_bStopProcessCtrlAllOfFocus0TypeExceptFocus0					= false;
#endif

#if __DEV
int			CDebugScene::iDisplaySceneUpdateCostStep									= 0;
char		CDebugScene::DisplaySceneUpdateCostStepName[16]								= {0};
bool		CDebugScene::bDisplaySceneUpdateCostOnVMap									= false;
bool		CDebugScene::bDisplaySceneUpdateCostSelectedOnly							= false;
#endif // __DEV

#if __BANK
sysMessageQueue<CDebugScene::TuneWidgetData, CDebugScene::TUNE_WIDGET_QUEUE_SIZE, false> CDebugScene::sm_TuneWidgetQueue;
bool		CDebugScene::bDisplayBuildingsOnVMap										= false;
bool		CDebugScene::bDisplaySceneScoredEntities									= false;

bool		CDebugScene::bDisplayVehiclesOnVMap											= false;
bool		CDebugScene::bDisplayVehiclesOnVMapAsActiveInactive							= false;
bool		CDebugScene::bDisplayVehiclesOnVMapTimesliceUpdates							= false;
bool		CDebugScene::bDisplayVehiclesOnVMapBasedOnOcclusion							= false;
bool		CDebugScene::bDisplayLinesToLocalDrivingCars								= false;
bool		CDebugScene::bDisplayVehiclesUsesFadeOnVMap									= false;
bool		CDebugScene::bDisplayVehPopFailedCreateEventsOnVMap							= false;
bool		CDebugScene::bDisplayVehPopCreateEventsOnVMap								= false;
bool		CDebugScene::bDisplayVehPopDestroyEventsOnVMap								= false;
bool		CDebugScene::bDisplayVehPopConversionEventsOnVMap							= false;
bool		CDebugScene::bDisplayVehGenLinksOnVM										= false;
bool		CDebugScene::bDisplayVehicleCreationPathsOnVMap								= false;
bool		CDebugScene::bDisplayVehicleCreationPathsInWorld							= false;
bool		CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap					= false;
bool		CDebugScene::bDisplayVehiclesToBeStreamedOutOnVMap							= false;
bool		CDebugScene::bDisplayVehicleCollisionsOnVMap								= false;

bool		CDebugScene::bDisplayPedsOnVMap												= false;
bool		CDebugScene::bDisplayPedsOnVMapAsActiveInactive								= false;
bool		CDebugScene::bDisplayPedsOnVMapTimesliceUpdates								= false;
bool		CDebugScene::bDisplaySpawnPointsRawDensityOnVMap							= false;
bool		CDebugScene::bDisplayPedPopulationEventsOnVMap								= false;
bool		CDebugScene::bDisplayPedsToBeStreamedOutOnVMap								= false;
bool		CDebugScene::bDisplayCandidateScenarioPointsOnVMap							= false;

bool		CDebugScene::bDisplayObjectsOnVMap											= false;
bool		CDebugScene::bDisplayPickupsOnVMap											= false;
bool		CDebugScene::bDisplayLineAboveObjects										= false;
bool		CDebugScene::bDisplayLineAboveAllEntities									= false;
float		CDebugScene::fEntityDebugLineLength											= 1.0f;
bool		CDebugScene::bDisplayDoorInfo												= false;
bool		CDebugScene::bDisplayNetworkGameOnVMap										= false;
bool		CDebugScene::bDisplayPortalInstancesOnVMap									= false;
bool		CDebugScene::bDisplayRemotePlayerCameras                            		= false;
bool		CDebugScene::bDisplayCarCreation                                    		= false;
bool		CDebugScene::bDisplayLadderDebug											= false;
bool		CDebugScene::bDisplayWaterOnVMap											= false;
bool		CDebugScene::bDisplayCalmingWaterOnVMap										= false;
bool		CDebugScene::bDisplayShoreLinesOnVMap										= false;
bool		CDebugScene::ms_drawFocusEntitiesBoundBox									= false;
bool		CDebugScene::ms_drawFocusEntitiesBoundBoxOnVMap								= false;
bool		CDebugScene::ms_drawFocusEntitiesInfo										= false;
bool		CDebugScene::ms_drawFocusEntitiesCoverTuning								= false;
bool		CDebugScene::ms_drawFocusEntitiesSkeleton									= false;
bool		CDebugScene::ms_drawFocusEntitiesSkeletonNonOrthonormalities				= false;
bool		CDebugScene::ms_drawFocusEntitiesSkeletonNonOrthoDataOnly					= false;
bool		CDebugScene::ms_drawFocusEntitiesSkeletonBoneNamesAndIds					= false;
bool		CDebugScene::ms_drawFocusEntitiesSkeletonChannels							= false;
bool		CDebugScene::ms_drawFocusEntitiesSkeletonNonOrthonoMats						= false;
bool		CDebugScene::bDisplayDuplicateObjectsBB										= false;
bool		CDebugScene::bDisplayDuplicateObjectsOnVMap									= false;

bool		CDebugScene::bDisplayTargetingRanges										= false;
bool		CDebugScene::bDisplayTargetingCones											= false;
bool		CDebugScene::bDisplayTargetingEntities										= false;

#endif
#if __DEV

bool		CDebugScene::ms_bBreakOnWorldAddOfFocusEntity								= false;
bool		CDebugScene::ms_bBreakOnWorldRemoveOfFocusEntity							= false;
bool		CDebugScene::ms_bBreakOnProcessControlOfFocusEntity 						= false;
bool		CDebugScene::ms_bBreakOnProcessIntelligenceOfFocusEntity					= false;
bool		CDebugScene::ms_bBreakOnProcessPhysicsOfFocusEntity 						= false;
bool		CDebugScene::ms_bBreakOnPreRenderOfFocusEntity								= false;
bool		CDebugScene::ms_bBreakOnUpdateAnimOfFocusEntity								= false;
bool		CDebugScene::ms_bBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity			= false;
bool		CDebugScene::ms_bBreakOnRenderOfFocusEntity									= false;
bool		CDebugScene::ms_bBreakOnAddToDrawListOfFocusEntity							= false;
bool		CDebugScene::ms_bBreakOnDestroyOfFocusEntity								= false;
bool		CDebugScene::ms_bBreakOnCalcDesiredVelocityOfFocusEntity					= false;

static char	g_proximityTestPointString[256];
bool		CDebugScene::ms_bBreakOnProximityOfDestroyCallingEntity						= false;
bool		CDebugScene::ms_bBreakOnProximityOfAddCallingEntity							= false;
bool		CDebugScene::ms_bBreakOnProximityOfRemoveCallingEntity						= false;
bool		CDebugScene::ms_bBreakOnProximityOfAddAndRemoveCallingEntity				= false;
bool		CDebugScene::ms_bBreakOnProximityOfAddToInteriorCallingEntity				= false;
bool		CDebugScene::ms_bBreakOnProximityOfRemoveFromInteriorCallingEntity			= false;
bool		CDebugScene::ms_bBreakOnProximityOfWorldAddCallingEntity					= false;
bool		CDebugScene::ms_bBreakOnProximityOfWorldAddCallingPed						= false;
bool		CDebugScene::ms_bBreakOnProximityOfTeleportCallingObject					= false;
bool		CDebugScene::ms_bBreakOnProximityOfTeleportCallingPed						= false;
bool		CDebugScene::ms_bBreakOnProximityOfTeleportCallingVehicle					= false;
bool		CDebugScene::ms_bBreakOnProximityOfWorldRemoveCallingEntity					= false;
bool		CDebugScene::ms_bBreakOnProximityOfProcessControlCallingEntity				= false;
bool		CDebugScene::ms_bBreakOnProximityOfProcessControlCallingPed					= false;
bool		CDebugScene::ms_bBreakOnProximityOfProcessPhysicsCallingEntity				= false;
bool		CDebugScene::ms_bBreakOnProximityOfPreRenderCallingEntity					= false;
bool		CDebugScene::ms_bBreakOnProximityOfUpdateAnimCallingEntity					= false;
bool		CDebugScene::ms_bBreakOnProximityOfUpdateAnimAfterCameraUpdateCallingEntity	= false;
bool		CDebugScene::ms_bBreakOnProximityOfRenderCallingEntity						= false;
bool		CDebugScene::ms_bBreakOnProximityOfAddToDrawListCallingEntity				= false;
bool		CDebugScene::ms_bDrawAttachmentExtensions									= false;
bool		CDebugScene::ms_bDrawAttachmentEdges										= false;
bool		CDebugScene::ms_bDisplayAttachmentEdgeData									= false;
bool		CDebugScene::ms_bDisplayUpdateRecords										= false;
bool		CDebugScene::ms_bDisplayAttachmentInvokingFunction							= false;
bool		CDebugScene::ms_bDisplayAttachmentInvokingFile								= false;
bool		CDebugScene::ms_bDisplayAttachmentPhysicsInfo								= false;
bool		CDebugScene::ms_bRenderListOfAttachmentExtensions							= false;
bool		CDebugScene::ms_bDisplayAttachmentEdgeType									= false;
bool		CDebugScene::ms_bDebugRenderAttachmentOfFocusEntitiesOnly					= false;
bool		CDebugScene::ms_fAttachmentNodeRelPosWorldUp								= false;
float		CDebugScene::ms_fAttachmentNodeEntityDist									= 1.6f;
float		CDebugScene::ms_fAttachmentNodeRadius										= 0.15f;
bool		CDebugScene::ms_bAttachFlagAutoDetachOnRagdoll								= false;
bool		CDebugScene::ms_bAttachFlagCollisionOn										= false;
bool		CDebugScene::ms_bAttachFlagDeleteWithParent									= false;
bool		CDebugScene::ms_bAttachFlagRotConstraint									= false;
bool		CDebugScene::ms_bDetachFlagActivatePhysics									= false;
bool		CDebugScene::ms_bDetachFlagApplyVelocity									= false;
bool		CDebugScene::ms_bDetachFlagNoCollisionUntilClear							= false;
bool		CDebugScene::ms_bDetachFlagIgnoreSafePositionCheck							= false;
bool		CDebugScene::ms_bEraserEnabled												= false;

#endif // __DEV

#if !__FINAL
RegdEnt		CDebugScene::ms_focusEntities[FOCUS_ENTITIES_MAX];
char		CDebugScene::ms_focusEntity0AddressString[64]								= { "None" };
#if __DEV
char		CDebugScene::ms_focusEntity0ModelInfoString[64]								= { "None" };
#endif // __DEV
bool		CDebugScene::ms_changeSelectedEntityOnHover									= false;
bool		CDebugScene::ms_doClickTestsViaVectorMapInsteadViewport						= false;
bool		CDebugScene::ms_makeFocusEntitiesFlash										= false;
bool		CDebugScene::ms_lockFocusEntities											= false;
bool		CDebugScene::ms_logFocusEntitiesPosition									= false;
bool		CDebugScene::ms_clickTestForPeds											= true;
bool		CDebugScene::ms_clickTestForRagdolls										= true;
bool		CDebugScene::ms_clickTestForVehicles										= true;
bool		CDebugScene::ms_clickTestForObjects											= true;
bool		CDebugScene::ms_clickTestForWeapons											= true;
bool		CDebugScene::ms_clickTestForBuildingsAndMap									= false;

Vector3		CDebugScene::ms_proximityTestPoint											(0.0f, 0.0f, 0.0f);
float		CDebugScene::ms_proximityMaxAllowableVerticalDiff							= 0.0f;
float		CDebugScene::ms_proximityMaxAllowableRadialDiff								= 0.0f;
bool		CDebugScene::ms_drawProximityArea											= false;
bool		CDebugScene::ms_drawProximityAreaFlashes									= false;
bool		CDebugScene::ms_drawProximityAreaOnVM										= false;
bool		CDebugScene::ms_drawProximityAreaOnVMFlashes								= false;
bool		CDebugScene::ms_bDisplayDebugSummary										= false;
bool		CDebugScene::ms_bDisplayLODCounts											= false;
#endif 

#if __BANK
bool		CDebugScene::ms_bEnableFocusEntityDragging = false;
Vector3		CDebugScene::ms_lastFrameWorldPosition(VEC3_ZERO);

int			CDebugScene::ms_lastFrameMouseX = 0;
int			CDebugScene::ms_lastFrameMouseY = 0;
int			CDebugScene::ms_DebugTimerMode = DTM_Off;
float		CDebugScene::ms_DebugTimer = -1.0f;

atArray<datCallback> CDebugScene::ms_callbacks;
CUiGadgetInspector * CDebugScene::m_pInspectorWindow = NULL;
bool CDebugScene::m_bIsInspectorAttached = false;

s32 CAuthoringHelper::ms_AuthorMode = 0; 
CUiGadgetSimpleListAndWindow* CAuthoringHelper::ms_pAuthorToolBar = NULL; 
CAuthoringHelper* CAuthoringHelper::ms_SelectedAuthorHelper; 
//CAuthoringHelper* CAuthoringHelper::ms_LastSelectedAuthorHelper = NULL; 
float CAuthoringHelper::ms_DistanceToCamera; 
atString CAuthoringHelper::m_Keyboardbuffer; 

#endif //__BANK

PARAM(touchDebug, "Set this to enable the touchpad based debug inputs for focusing on peds and pausing the game.");
PARAM(touchDebugDisableConflictingInputs, "Disables conflicting inputs on d-pad, so that stuff doesnt trigger while trying to use touch debug");
PARAM(touchDebugTapTapTapOhWhyHasTheGamePaused, "Set this if you want to disable the touch with one finger and double tap pause part of it.");
PARAM(touchDebugDisableTwoTouchCommands, "Set this if you want to disable the two-touch d-pad commands.");

#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// Helper function for the TUNE_GROUP_ macros...
/////////////////////////////////////////////////////////////////////////////////
bkGroup* GetOrCreateGroup(bkBank* pBankToSearchOrAddTo, const char* groupName)
{
	const u32 groupNameHash = atStringHash(groupName);
	bkGroup* pGroup = NULL;
	{
		bkWidget* pChild = pBankToSearchOrAddTo->GetChild();
		while(pChild)
		{
			if(atStringHash(pChild->GetTitle()) == groupNameHash)
			{
				break;
			}
			pChild = pChild->GetNext();
		}
		if(pChild)
		{
			pGroup = dynamic_cast<bkGroup*>(pChild);
		}
	}
	if(!pGroup)
	{
		pGroup = pBankToSearchOrAddTo->PushGroup(groupName);
		Assert(pGroup);
		pBankToSearchOrAddTo->PopGroup();
	}

	return pGroup;
}

CDebugScene::TuneWidgetData::TuneWidgetData()
	: m_Type(TUNE_TYPE_INVALID)
	, m_GroupName(NULL)
	, m_Title(NULL)
{
	memset(&m_Params, 0, sizeof(m_Params));
}

CDebugScene::TuneWidgetData::TuneWidgetData(const s32 type, const char* groupName, const char* title, void* data, const void* min, const void* max, const void* delta)
	: m_Type(type)
	, m_GroupName(ConstStringDuplicate(groupName))
	, m_Title(ConstStringDuplicate(title))
{
	switch (type)
	{
	case TUNE_TYPE_BOOL:
		{
			m_Params.m_AsBool.m_Data = static_cast<bool*>(data);
			break;
		}
	case TUNE_TYPE_INT:
		{
			m_Params.m_AsInt.m_Data = static_cast<s32*>(data);
			m_Params.m_AsInt.m_Min = *static_cast<const s32*>(min);
			m_Params.m_AsInt.m_Max = *static_cast<const s32*>(max);
			m_Params.m_AsInt.m_Delta = *static_cast<const float*>(delta);
			break;
		}
	case TUNE_TYPE_FLOAT:
		{
			m_Params.m_AsFloat.m_Data = static_cast<float*>(data);
			m_Params.m_AsFloat.m_Min = *static_cast<const float*>(min);
			m_Params.m_AsFloat.m_Max = *static_cast<const float*>(max);
			m_Params.m_AsFloat.m_Delta = *static_cast<const float*>(delta);
			break;
		}
	case TUNE_TYPE_COLOR:
		{
			m_Params.m_AsColor.m_Data = static_cast<Color32*>(data);
			break;
		}
	default:
		{
			Assert(0);
		}
	}
}

void CDebugScene::AddWidgets(bkBank &DEV_ONLY(bank))
{
#if __DEV
	bank.PushGroup("Point And Click Entity Debugging",false);
		bank.AddTitle("Mouse point and click to select the Focus0 entity." );
		bank.AddTitle("Control-click to add additional focus entities." );
#if !__FINAL
		
		bank.AddToggle("Select entity under mouse (without click)",						&ms_changeSelectedEntityOnHover,						NullCB, "Allows selection update per frame for whatever is under the mouse cursor.");
		bank.AddToggle("Do click tests On VectorMap instead of 3D",						&ms_doClickTestsViaVectorMapInsteadViewport,			NullCB, "Allows selecting via VectorMap instead of the 3D Viewport.");
		bank.AddText("Focus Entity0 Address ('0xABCDEF12')",							ms_focusEntity0AddressString,							64, false, datCallback(HandleFocusEntityTextBoxChange));
#if __DEV		
		bank.AddText("Focus Entity0 Model Info Name",									ms_focusEntity0ModelInfoString,							64, false, datCallback(HandleFocusEntityTextBoxChange));
#endif // __DEV
		bank.AddToggle("Draw Focus Entity(s) Bound Box",								&ms_drawFocusEntitiesBoundBox,							NullCB, "Makes the focus entities bounding box draw.");
		bank.AddToggle("Draw Focus Entity(s) Bound Box On VectorMap",					&ms_drawFocusEntitiesBoundBoxOnVMap,					NullCB, "Makes the focus entities bounding box draw on VectorMap.");
		bank.AddToggle("Draw Focus Entity(s) Info",										&ms_drawFocusEntitiesInfo,								NullCB, "Makes the focus entities info draw.");
		bank.AddToggle("Draw Focus Entity(s) Cover Tuning",								&ms_drawFocusEntitiesCoverTuning,						NullCB, "Makes the focus entities cover tuning draw.");
		bank.AddToggle("Draw Focus Entity(s) Skeleton",									&ms_drawFocusEntitiesSkeleton,							NullCB, "Draw the focus entities skeleton");
		bank.AddToggle("Draw Focus Entity(s) Skeleton's Non-orthonormalities",			&ms_drawFocusEntitiesSkeletonNonOrthonormalities,		NullCB, "Draw the focus entities skeleton's non-orthonormalities");
		bank.AddToggle("Draw Focus Entity(s) Skeleton's Non-ortho data only",			&ms_drawFocusEntitiesSkeletonNonOrthoDataOnly,			NullCB, "Draw the focus entities skeleton's non-ortho data only");
		bank.AddToggle("Draw Focus Entity(s) Skeleton's Bone names and Ids",			&ms_drawFocusEntitiesSkeletonBoneNamesAndIds,			NullCB, "Draw the focus entities skeleton's bone names and Ids");
		bank.AddToggle("Draw Focus Entity(s) Skeleton's Channels",						&ms_drawFocusEntitiesSkeletonChannels,					NullCB, "Draw the focus entities skeleton's channels");
		bank.AddToggle("Draw Focus Entity(s) Skeleton's Non-ortho mats",				&ms_drawFocusEntitiesSkeletonNonOrthonoMats,			NullCB, "Draw the focus entities skeleton's non-ortho mats");
		bank.AddToggle("Make Focus Entity(s) Flash",									&ms_makeFocusEntitiesFlash,								NullCB, "Makes the focus entity only draw every other frame.");
		bank.AddToggle("Lock Focus Entity(s)",											&ms_lockFocusEntities,									NullCB, "Locks in the current focus entities so that clicking will not select new ones or de-select the current ones.");
		bank.AddToggle("Print Position/Rotation On Focus Entity(s) Change",				&ms_logFocusEntitiesPosition,							NullCB, "Prints out additional information when selecting new focus entities.");
		bank.PushGroup("What to entities to select on click",false);
			bank.AddToggle("Peds",														&ms_clickTestForPeds,									NullCB, "Allows peds to be selectable via mouse click.");
			bank.AddToggle("Ragdolls",													&ms_clickTestForRagdolls,								NullCB, "Allows ragdolls to be selectable via mouse click.");
			bank.AddToggle("Vehicles",													&ms_clickTestForVehicles,								NullCB, "Allows vehicles to be selectable via mouse click.");
			bank.AddToggle("Objects",													&ms_clickTestForObjects,								NullCB, "Allows objects to be selectable via mouse click.");
			bank.AddToggle("Weapons",													&ms_clickTestForWeapons,								NullCB, "Allows weapons to be selectable via mouse click.");
			bank.AddToggle("Buildings and Map",											&ms_clickTestForBuildingsAndMap,						NullCB, "Allows buildings and map to be selectable via mouse click.");
	//		bank.AddToggle("Select peds from inside of cars",							&ms_selectPedsTypesFromInsideVehicles,					NullCB, "Select peds from inside vehicles.");
		bank.PopGroup();
#endif // !FINAL
		bank.PushGroup("Break when the Focus0 entity has...",false);
			bank.AddToggle("Destroy Called",											&ms_bBreakOnDestroyOfFocusEntity,						NullCB, "Causes the debugger to break in the Destroy (~) function of the focus entity");
			bank.AddToggle("WorldAdd Called",											&ms_bBreakOnWorldAddOfFocusEntity,						NullCB, "Causes the debugger to break when CGameWorld::Add() function is called on the focus entity");
			bank.AddToggle("WorldRemove Called",										&ms_bBreakOnWorldRemoveOfFocusEntity,					NullCB, "Causes the debugger to break when CGameWorld::Remove() function is called on the focus entity");
			bank.AddToggle("ProcessControl Called",										&ms_bBreakOnProcessControlOfFocusEntity,				NullCB, "Causes the debugger to break in the ::ProcessControl() function of the focus entity");
			bank.AddToggle("ProcessIntelligence Called",								&ms_bBreakOnProcessIntelligenceOfFocusEntity,			NullCB, "Causes the debugger to break in the ::ProcessIntellgence() function of the focus entity");
			bank.AddToggle("ProcessPhysics Called",										&ms_bBreakOnProcessPhysicsOfFocusEntity,				NullCB, "Causes the debugger to break in the ::ProcessPhysics() function of the focus entity");
			bank.AddToggle("PreRender Called",											&ms_bBreakOnPreRenderOfFocusEntity,						NullCB, "Causes the debugger to break in the ::PreRender() function of the focus entity");
			bank.AddToggle("UpdateAnim Called",											&ms_bBreakOnUpdateAnimOfFocusEntity,					NullCB, "Causes the debugger to break in the ::UpdateAnim() function of the focus entity");
			bank.AddToggle("UpdateAnimAfterCameraUpdate Called",						&ms_bBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity,	NullCB, "Causes the debugger to break in the ::UpdateAnimAfterCameraUpdate() function of the focus entity");
			bank.AddToggle("Render Called",												&ms_bBreakOnRenderOfFocusEntity,						NullCB, "Causes the debugger to break in the ::Render() function of the focus entity");
			bank.AddToggle("AddToDrawList Called",										&ms_bBreakOnAddToDrawListOfFocusEntity,					NullCB, "Causes the debugger to break in the ::AddToDrawList() function of the focus entity");
			bank.AddToggle("CalcDesiredVelocity Called",								&ms_bBreakOnCalcDesiredVelocityOfFocusEntity,			NullCB, "Causes the debugger to break in the ::CalcDesiredVelocity() function of the focus entity");
		bank.PopGroup();
		bank.AddToggle("Stop ProcessControl Of All Entities",							&ms_bStopProcessCtrlAllEntities,						NullCB, "Stops processing all entities");
		bank.AddToggle("Stop ProcessControl Of Entities Except Focus0",					&ms_bStopProcessCtrlAllExceptFocus0Entity,				NullCB, "Stops processing all entities except the focus entity");
		bank.AddToggle("Stop ProcessControl Of All Entities Of Focus0 Type",			&ms_bStopProcessCtrlAllEntitiesOfFocus0Type,			NullCB, "Stops processing all entities of focus type");
		bank.AddToggle("Stop ProcessControl Of Entities Of Focus0 Type Except Focus0",	&ms_bStopProcessCtrlAllOfFocus0TypeExceptFocus0,		NullCB, "Stops processing all entities of focus type except the focus entity");
	bank.PopGroup();
	bank.PushGroup("Proximity Debugging",false);
		bank.AddText("Proximity Point x y z", g_proximityTestPointString, sizeof(g_proximityTestPointString), false, &ChangeProximityTestPoint);
#if !__FINAL
		bank.AddSlider("Max Allowable Vertical Diff",									&ms_proximityMaxAllowableVerticalDiff,								0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Max Allowable radial Diff,",									&ms_proximityMaxAllowableRadialDiff,								0.0f, 1000.0f, 1.0f);
		bank.AddToggle("Draw Proximity Area",											&ms_drawProximityArea,												NullCB, "Draws the proximity area (in 3D)");
		bank.AddToggle("Draw Proximity Area Flashes",									&ms_drawProximityAreaFlashes,										NullCB, "If drawn the proximity area flashes (in 3D)");
		bank.AddToggle("Draw Proximity Area (On VectorMap)",							&ms_drawProximityAreaOnVM,											NullCB, "Draws the proximity area (on the Vector Map)");
		bank.AddToggle("Draw Proximity Area On VM Flashes",								&ms_drawProximityAreaOnVMFlashes,									NullCB, "If drawn the proximity area flashes (on the Vector Map)");
#endif // !FINAL
		bank.PushGroup("Break on proximity of...",false);
			bank.AddToggle("Destroy Calling Entity",									&ms_bBreakOnProximityOfDestroyCallingEntity,						NullCB, "Causes the debugger to break on proximity of a Destroy (~) function calling entity");
			bank.AddToggle("Add Calling Entity",										&ms_bBreakOnProximityOfAddCallingEntity,							NullCB, "Causes the debugger to break on proximity of a Add function calling entity");
			bank.AddToggle("Remove Calling Entity",										&ms_bBreakOnProximityOfRemoveCallingEntity,							NullCB, "Causes the debugger to break on proximity of a Remove function calling entity");
			bank.AddToggle("RemoveAndAdd Calling Entity",								&ms_bBreakOnProximityOfAddAndRemoveCallingEntity,					NullCB, "Causes the debugger to break on proximity of a AddAndRemove function calling entity");
			bank.AddToggle("AddToInterior Calling Entity",								&ms_bBreakOnProximityOfAddToInteriorCallingEntity,					NullCB, "Causes the debugger to break on proximity of a AddToInterior function calling entity");
			bank.AddToggle("RemoveFromInterior Calling Entity",							&ms_bBreakOnProximityOfRemoveFromInteriorCallingEntity,				NullCB, "Causes the debugger to break on proximity of a RemoveFromInterior function calling entity");
			bank.AddToggle("WorldAdd Calling Entity",									&ms_bBreakOnProximityOfWorldAddCallingEntity,						NullCB, "Causes the debugger to break on proximity of the entity CGameWorld::Add() is called on");
			bank.AddToggle("WorldAdd Calling Ped",										&ms_bBreakOnProximityOfWorldAddCallingPed,							NullCB, "Causes the debugger to break on proximity of the ped CGameWorld::Add() is called on");
			bank.AddToggle("Teleport Calling Object",									&ms_bBreakOnProximityOfTeleportCallingObject,						NullCB, "Causes the debugger to break on proximity of the entity CGameWorld::Add() is called on");
			bank.AddToggle("Teleport Calling Ped",										&ms_bBreakOnProximityOfTeleportCallingPed,							NullCB, "Causes the debugger to break on proximity of the ped CGameWorld::Add() is called on");
			bank.AddToggle("Teleport Calling Vehicle",									&ms_bBreakOnProximityOfTeleportCallingVehicle,						NullCB, "Causes the debugger to break on proximity of the dummy ped CGameWorld::Add() is called on");
			bank.AddToggle("WorldRemove Calling Entity",								&ms_bBreakOnProximityOfWorldRemoveCallingEntity,					NullCB, "Causes the debugger to break on proximity of the entity CGameWorld::Remove() is called on");
			bank.AddToggle("ProcessControl Calling Entity",								&ms_bBreakOnProximityOfProcessControlCallingEntity,					NullCB, "Causes the debugger to break on proximity of a ::ProcessControl() function calling entity");
			bank.AddToggle("ProcessControl Calling Ped",								&ms_bBreakOnProximityOfProcessControlCallingPed,					NullCB, "Causes the debugger to break on proximity of the ped CGameWorld::Remove() is called on");
			bank.AddToggle("ProcessPhysics Calling Entity",								&ms_bBreakOnProximityOfProcessPhysicsCallingEntity,					NullCB, "Causes the debugger to break on proximity of a ::ProcessPhysics() function calling entity");
			bank.AddToggle("PreRender Calling Entity",									&ms_bBreakOnProximityOfPreRenderCallingEntity,						NullCB, "Causes the debugger to break on proximity of a ::PreRender() function calling entity");
			bank.AddToggle("UpdateAnim Calling Entity",									&ms_bBreakOnProximityOfUpdateAnimCallingEntity,						NullCB, "Causes the debugger to break on proximity of a ::UpdateAnim() function calling entity");
			bank.AddToggle("UpdateAnimAfterCameraUpdate Calling Entity",				&ms_bBreakOnProximityOfUpdateAnimAfterCameraUpdateCallingEntity,	NullCB, "Causes the debugger to break on proximity of a ::UpdateAnimAfterCameraUpdate() function calling entity");
			bank.AddToggle("Render Calling Entity",										&ms_bBreakOnProximityOfRenderCallingEntity,							NullCB, "Causes the debugger to break on proximity of a ::Render() function calling entity");
			bank.AddToggle("AddToDrawList Calling Entity",								&ms_bBreakOnProximityOfAddToDrawListCallingEntity,					NullCB, "Causes the debugger to break on proximity of a ::AddToDrawList() function calling entity");
		bank.PopGroup();
	bank.PopGroup();
	bank.PushGroup("Attach Test");
		bank.AddButton("Attach focus entities",AttachFocusEntitiesTogether);
		bank.AddButton("Attach focus entities physically",AttachFocusEntitiesTogetherPhysically);
		bank.AddButton("Attach all focus entities to world",AttachFocusEntitiesToWorld);
		bank.AddButton("Detach focus entity",DetachFocusEntity);
		bank.AddButton("DELETE all focus entities",DeleteFocusEntities);
		bank.AddToggle("Draw attachment nodes",&ms_bDrawAttachmentExtensions);
		bank.AddToggle("Draw attachment edges",&ms_bDrawAttachmentEdges);
		bank.AddToggle("Display attachment edge type",&ms_bDisplayAttachmentEdgeType);
		bank.AddToggle("Display attachment edge data",&ms_bDisplayAttachmentEdgeData);
		bank.AddToggle("Display update records",&ms_bDisplayUpdateRecords);
		bank.AddToggle("Display attachment invoking function",&ms_bDisplayAttachmentInvokingFunction);
		bank.AddToggle("Display attachment invoking file",&ms_bDisplayAttachmentInvokingFile);
		bank.AddToggle("Display attachment physics info",&ms_bDisplayAttachmentPhysicsInfo);
		bank.AddToggle("Render list of attachment extensions",&ms_bRenderListOfAttachmentExtensions);
		bank.AddToggle("Debug render attachment of focus entities only",&ms_bDebugRenderAttachmentOfFocusEntitiesOnly);
		bank.AddToggle("Attachment node relative pos world up",&ms_fAttachmentNodeRelPosWorldUp);
		bank.AddSlider("Attachment node radius", &ms_fAttachmentNodeRadius, 0.01f, 100.0f, 0.01f);
		bank.AddSlider("Attachment node dist from entity", &ms_fAttachmentNodeEntityDist, 0.0f, 100.0f, 0.01f);
		bank.PushGroup("Attach flags",false);
			bank.AddToggle("Collision on",&ms_bAttachFlagCollisionOn);
			bank.AddToggle("Auto detach on ragdoll",&ms_bAttachFlagAutoDetachOnRagdoll);
			bank.AddToggle("Delete with parent",&ms_bAttachFlagDeleteWithParent);
			bank.AddToggle("Rot constraint",&ms_bAttachFlagRotConstraint);
		bank.PopGroup();
		bank.PushGroup("Detach flags",false);
			bank.AddToggle("Activate physics",&ms_bDetachFlagActivatePhysics);
			bank.AddToggle("Apply velocity",&ms_bDetachFlagApplyVelocity);
			bank.AddToggle("No collision until clear",&ms_bDetachFlagNoCollisionUntilClear);
			bank.AddToggle("Ignore safe position check",&ms_bDetachFlagIgnoreSafePositionCheck);
		bank.PopGroup();
	bank.PopGroup();

	bank.PushGroup("Eraser");
		bank.AddTitle("This allows point and click erasing");
		bank.AddToggle("Eraser enabled", &ms_bEraserEnabled);
	bank.PopGroup();
#endif // __DEV

	bkBank& theTuneBank = BANKMGR.CreateBank("_TUNE_");
	theTuneBank.AddTitle("Items are added here automatically by the macros");
	theTuneBank.AddTitle("TUNE_BOOL, TUNE_INT, or TUNE_FLOAT and");
	theTuneBank.AddTitle("TUNE_GROUP_BOOL, TUNE_GROUP_INT, or TUNE_GROUP_FLOAT.");
	theTuneBank.AddTitle("Use them, love them.");

	bkBank *pBank = BANKMGR.FindBank("Optimization");
#if !__FINAL
	pBank->AddToggle("Debug Summary",								&ms_bDisplayDebugSummary,				NullCB, "Toggle the debug summary render output");
	pBank->AddToggle(" Include LOD Counts in Debug Summary",		&ms_bDisplayLODCounts,					NullCB, "Include LOD counts in debug summary render output");
	pBank->PushGroup("Entities on vectormap");
		pBank->AddToggle("Display peds active/inactive",	&CDebugScene::bDisplayPedsOnVMapAsActiveInactive);
		pBank->AddToggle("Display vehicles active/inactive",&CDebugScene::bDisplayVehiclesOnVMapAsActiveInactive);
		pBank->AddToggle("Display objects active/inactive", &CDebugScene::bDisplayObjectsOnVMap);
		pBank->AddToggle("Display peds timeslice updates",	&CDebugScene::bDisplayPedsOnVMapTimesliceUpdates);
		pBank->AddToggle("Display vehicles timeslice updates",&CDebugScene::bDisplayVehiclesOnVMapTimesliceUpdates);
		pBank->AddToggle("Display vehicles occluded",&CDebugScene::bDisplayVehiclesOnVMapBasedOnOcclusion);
		pBank->AddToggle("Display local camera", &CVectorMap::m_bDisplayLocalPlayerCamera );
	pBank->PopGroup();
#endif // !FINAL
// DEBUG!! -AC, Not the best place for this, but it will do for now...
	FocusEntities_Clear();
// END DEBUG!!
}

bool CDebugScene::QueueTuneWidget(const TuneWidgetData& data)
{
	return sm_TuneWidgetQueue.Push(data) != NULL;
}

void CDebugScene::FlushTuneWidgetQueue()
{
	bkBank* bank = BANKMGR.FindBank("_TUNE_");

	if (bank)
	{
		while (!sm_TuneWidgetQueue.IsEmpty())
		{
			const TuneWidgetData data = sm_TuneWidgetQueue.Pop();

			bkGroup* group = NULL;

			if (data.m_GroupName)
			{
				group = GetOrCreateGroup(bank, data.m_GroupName);
				bank->SetCurrentGroup(*group);
			}

			switch (data.m_Type)
			{
			case TUNE_TYPE_BOOL:
				{
					bank->AddToggle(data.m_Title, data.m_Params.m_AsBool.m_Data);
					break;
				}
			case TUNE_TYPE_INT:
				{
					bank->AddSlider(data.m_Title, data.m_Params.m_AsInt.m_Data, data.m_Params.m_AsInt.m_Min, data.m_Params.m_AsInt.m_Max, (s32)data.m_Params.m_AsInt.m_Delta);
					break;
				}
			case TUNE_TYPE_FLOAT:
				{
					bank->AddSlider(data.m_Title, data.m_Params.m_AsFloat.m_Data, data.m_Params.m_AsFloat.m_Min, data.m_Params.m_AsFloat.m_Max, data.m_Params.m_AsFloat.m_Delta);
					break;
				}
			case TUNE_TYPE_COLOR:
				{
					bank->AddColor(data.m_Title, data.m_Params.m_AsColor.m_Data);
					break;
				}
			default:
				{
					Assert(0);
				}
			}

			if (group)
			{
				bank->UnSetCurrentGroup(*group);
			}
		}
	}
}
#endif // __BANK

#if __BANK

void CDebugScene::DisableInputsForTouchDebug()
{
	if (PARAM_touchDebugDisableConflictingInputs.Get())
	{
		CControl& rControl = CControlMgr::GetMainPlayerControl();

		rControl.SetInputExclusive(INPUT_FRONTEND_UP);
		rControl.SetInputExclusive(INPUT_FRONTEND_DOWN);
		rControl.SetInputExclusive(INPUT_FRONTEND_LEFT);
		rControl.SetInputExclusive(INPUT_FRONTEND_RIGHT);
	}
}

void CDebugScene::RegisterEntityChangeCallback(datCallback callback)
{
	ms_callbacks.PushAndGrow(callback);
}

#endif //__BANK

#if USE_PROFILER
const char* MEM_TYPE_TO_SIZE_STRING[] =	{	"Memory\\Game Virtual (Kb)", 
											"Memory\\Resource Virtual (Kb)", 
											"Memory\\Game Physical (Kb)", 
											"Memory\\Resource Physical (Kb)", 
											"Memory\\Debug Virtual (Kb)" };

const char* MEM_TYPE_TO_TOTAL_STRING[] = {	"Memory\\Game Virtual [Total] (Kb)", 
											"Memory\\Resource Virtual [Total] (Kb)", 
											"Memory\\Game Physical [Total] (Kb)", 
											"Memory\\Resource Physical [Total] (Kb)", 
											"Memory\\Debug Virtual [Total] (Kb)" };

template<eMemoryType MEM_TYPE>
struct MemoryReporter
{
	static void Report()
	{
		if (sysMemAllocator* allocator = sysMemAllocator::GetMaster().GetAllocator(MEM_TYPE))
		{																													\
			PROFILER_TAG(MEM_TYPE_TO_SIZE_STRING[MEM_TYPE], (rage::u64)(allocator->GetMemoryUsed() >> 10));
			PROFILER_TAG(MEM_TYPE_TO_TOTAL_STRING[MEM_TYPE], (rage::u64)(allocator->GetHeapSize() >> 10));
		}																													\
	}
};

struct PoolReporter
{
	const Profiler::TagDescription* tags[ENTITY_TYPE_TOTAL];
	PoolReporter()
	{
		char buffer[256];
		for (int i = 0; i < ENTITY_TYPE_TOTAL; ++i)
		{
			sprintf_s(buffer, "EntityCount\\%s", entityTypeStr[i]);
			tags[i] = Profiler::TagDescription::Create(buffer);
		}
	}

	static void Report()
	{
		static PoolReporter reporter;

		Profiler::Timestamp time;
		time.Start();

		for (int i = 0; i < ENTITY_TYPE_TOTAL; ++i)
			if (const fwBasePool* pEntPool = CEntity::ms_EntityPools[i])
				Profiler::AttachTag(reporter.tags[i], (u64)pEntPool->GetNoOfUsedSpaces(), time);
	}
};

void CDebugScene::AttachProfilingVariables()
{
	if (Profiler::IsCollectingTags())
	{
		PoolReporter::Report();

		PROFILER_TAG("General\\Camera Position", VECTOR3_TO_VEC3V(camInterface::GetPos()));
		PROFILER_TAG("General\\Camera FOV", camInterface::GetFov());
		PROFILER_TAG("General\\Camera Rotation", Vec3V(camInterface::GetHeading(), camInterface::GetPitch(), camInterface::GetRoll()));

		PROFILER_TAG("Frame\\Number", (rage::u64)fwTimer::GetFrameCount());
		PROFILER_TAG("Frame\\GPU Number", (rage::u64)GRCDEVICE.GetFrameCounter());
		PROFILER_TAG("Frame\\CPU Time (ms)", fwTimer::GetSystemTimeStep() * 1000.0f);

		MemoryReporter<MEMTYPE_GAME_VIRTUAL>::Report();
		MemoryReporter<MEMTYPE_RESOURCE_VIRTUAL>::Report();
		MemoryReporter<MEMTYPE_GAME_PHYSICAL>::Report();
		MemoryReporter<MEMTYPE_RESOURCE_PHYSICAL>::Report();
		MemoryReporter<MEMTYPE_DEBUG_VIRTUAL>::Report();
	}

	// Outside IsCollectingTags scope check - we need to add them in any case
	Profiler::AddInternalTags();
}
#endif //USE_PROFILER


void CDebugScene::ChangeProximityTestPoint()
{
#if __DEV
	Vector3 vecPos(VEC3_ZERO);
	int nResults = 0;

	nResults = sscanf(g_proximityTestPointString, "%f %f %f", &vecPos.x, &vecPos.y, &vecPos.z);

	if(nResults)
	{
		ms_proximityTestPoint = vecPos;
	}
#endif // __DEV
}



#if __BANK

static const char* gDebugSceneInspectorEntries[CDebugScene::GADGET_NUM] =
{
	"",
	"Matrix-X",
	"Matrix-Y",
	"Matrix-Z",
	"Translation",
	"Scale",
	"Heading",
	"Entity type",
	"PathServer Id",
	"Mass",
	"Health",
	"Collision model type",
	"Num 2dFx",
	"Num attached lights",
	"Visible",
	"Fixed",
	"Fixed for navigation",
	"Not avoided",
	"Does not provide AI cover",
	"Does not provide player cover",
	"Is ladder",
	"Uses door physics",
	"Has animation",
	"Animation auto start",
	"Has uv animation",
	"Has spawn points",
	"Generates wind audio",
	"Ambient scale",
	"Is prop",
	"Is traffic light",
	"Tint palette index",
	"Climbable by AI",
	"Has added lights",
	"Is targettable with no los",
	"Is a target priority",
	"Can be targetted by player",
	"Cull Small Shadows",
	"Don't cast shadows",
	"Render Only In Reflections",
	"Don't Render In Reflections",
	"Render Only In Shadow",
	"Don't Render In Shadow",
	"LOD Group Mask",
	"LOD Flags",
	"Is Used In MP",
	"POPTYPE",
	"Owned By",
	"Replay ID",
	"Replay Map Hash",
	"Replay Hide Count",
};

void PopulateDebugSceneInspectorWindowCB(CUiGadgetText* pResult, u32 row, u32 col)
{
	if (pResult && row < CDebugScene::GADGET_NUM)
	{
		char str[256];
		CEntity *pEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
		if (pEntity)
		{
			if(col==0)
			{
				switch(row)
				{
				case 0:
					sprintf(str, "\"%s\"", pEntity->GetModelName());
					break;
				default:
					sprintf(str, "%s", gDebugSceneInspectorEntries[row]);
					break;
				}
			}
			else
			{
				switch(row)
				{
				case 0:
					sprintf(str, "0x%p (MI:%i)", pEntity, pEntity->GetModelIndex());
					break;

				case CDebugScene::GADGET_ENTITY_TYPE:
					{
						sprintf(str, "%i (%s)", pEntity->GetType(), GetEntityTypeStr(pEntity->GetType()) );
						break;
					}
				case CDebugScene::GADGET_MAT_XAXIS:
					{
						Vector3 vRight = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetRight());
						sprintf(str, "%.4f, %.4f, %.4f", vRight.x, vRight.y, vRight.z);
						break;
					}
				case CDebugScene::GADGET_MAT_YAXIS:
					{
						Vector3 vFwd = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetForward());
						sprintf(str, "%.4f, %.4f, %.4f", vFwd.x, vFwd.y, vFwd.z);
						break;
					}
				case CDebugScene::GADGET_MAT_ZAXIS:
					{
						Vector3 vUp = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetUp());
						sprintf(str, "%.4f, %.4f, %.4f", vUp.x, vUp.y, vUp.z);
						break;
					}
				case CDebugScene::GADGET_MAT_TRANS:
					{
						Vector3 vPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
						sprintf(str, "%.4f, %.4f, %.4f", vPos.x, vPos.y, vPos.z);
						break;
					}
				case CDebugScene::GADGET_SCALE:
					{
						float fScaleXY = 1.0f;
						float fScaleZ = 1.0f;

						//Mat34V mat = pEntity->GetNonOrthoMatrix();
						//Vec3V vscale = mat.d();
						//Displayf("%.2f, %.2f, %.2f", vscale.GetXf(), vscale.GetYf(), vscale.GetZf());

						const fwTransform* pTransform = pEntity->GetTransformPtr();
						if (pTransform->IsSimpleScaledTransform())
						{
							fScaleXY = ((fwSimpleScaledTransform*)pTransform)->GetScaleXY();
							fScaleZ = ((fwSimpleScaledTransform*)pTransform)->GetScaleZ();
						}
						else if (pTransform->IsMatrixScaledTransform())
						{
							fScaleXY = ((fwMatrixScaledTransform*)pTransform)->GetScaleXY();
							fScaleZ = ((fwMatrixScaledTransform*)pTransform)->GetScaleZ();
						}
						else if (pTransform->IsQuaternionScaledTransform())
						{
							fScaleXY = ((fwQuaternionScaledTransform*)pTransform)->GetScaleXY();
							fScaleZ = ((fwQuaternionScaledTransform*)pTransform)->GetScaleZ();
						}
						sprintf(str, "%.4f, %.4f, %.4f", fScaleXY, fScaleXY, fScaleZ);
						break;
					}
				case CDebugScene::GADGET_ENTITY_HEADING:
					{
						float rad = pEntity->GetTransform().GetHeading();
						sprintf(str, "%.2f (degrees: %.2f)", rad, rad * RtoD);
						break;
					}
				case CDebugScene::GADGET_PATHSERVER_ID:
					{
						if(pEntity->GetIsTypeObject())
							sprintf(str, "%i", ((CObject*)pEntity)->GetPathServerDynamicObjectIndex());
						else if(pEntity->GetIsTypeVehicle())
							sprintf(str, "%i", ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex());
						else
							sprintf(str, "-");
						break;
					}
				case CDebugScene::GADGET_MASS:
					{
						sprintf(str, "%.2f", pEntity->GetPhysArch() ? ((CPhysical*)pEntity)->GetMass() : 0.0f);
						break;
					}
				case CDebugScene::GADGET_PED_HEALTH:
					{
						if(pEntity->GetIsPhysical())
						{
							CPhysical *pPed = static_cast<CPhysical*>(pEntity);
							sprintf(str, "%.2f", pPed->GetHealth());
						}
						else
						{
							sprintf(str, "N/A");
						}
						break;
					}
				case CDebugScene::GADGET_COLLISION_MODEL_TYPE:
					{
						if(pEntity->GetPhysArch() && pEntity->GetPhysArch()->GetBound())
							sprintf(str, "%s", pEntity->GetPhysArch()->GetBound()->GetTypeString());
						else
							sprintf(str, "none");
						break;
					}
				case CDebugScene::GADGET_NUM_2D_FX:
					{
						sprintf(str, "%i", pEntity->GetBaseModelInfo()->GetNum2dEffects());
						break;
					}
				case CDebugScene::GADGET_NUM_2D_FX_LIGHTS:
					{
						sprintf(str, "%i", pEntity->GetBaseModelInfo()->GetNum2dEffectLights());
						break;
					}
				case CDebugScene::GADGET_VISIBLE:
					{
						pEntity->GetIsVisibleInfo( str, 256 );
						break;
					}
				case CDebugScene::GADGET_FIXED:
					{
						sprintf(str, "%s", pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_FIXED_FOR_NAVIGATION:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetIsFixedForNavigation() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_NOT_AVOIDED:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetNotAvoidedByPeds() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_DOES_NOT_PROVIDE_AI_COVER:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetDoesNotProvideAICover() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_DOES_NOT_PROVIDE_PLAYER_COVER:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetDoesNotProvidePlayerCover() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_LADDER:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetIsLadder() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_DOOR_PHYSICS:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetUsesDoorPhysics() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_HAS_ANIM:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetHasAnimation() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_ANIM_AUTO_START:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetAutoStartAnim() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_HAS_UV_ANIM:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetHasUvAnimation() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_HAS_SPAWN_POINTS:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetHasSpawnPoints() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_GENERATES_WIND_AUDIO:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetGeneratesWindAudio() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_AMBIENT_SCALE:
					{
						sprintf(str, "%s - %s - %s", pEntity->GetBaseModelInfo()->GetUseAmbientScale() ? "true" : "false", 
													 pEntity->GetUseAmbientScale() ? "true" : "false", 
													 pEntity->GetUseDynamicAmbientScale() ? "true" : "false" );
						break;
					}
				case CDebugScene::GADGET_IS_PROP:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetIsProp() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_IS_TRAFFIC_LIGHT:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT) ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_TINT_PALETTE_INDEX:
					{
						sprintf(str, "%d", pEntity->GetTintIndex());
						break;
					}
				case CDebugScene::GADGET_CLIMBABLE_BY_AI:
					{
						sprintf(str, "%s", pEntity->GetBaseModelInfo()->GetIsClimbableByAI() ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_HAS_ADDED_LIGHTS:
					{
						sprintf(str, "%s", pEntity->m_nFlags.bLightObject ? "true" : "false");
						break;
					}
				case CDebugScene::GADGET_IS_TARGETTABLE_WITH_NO_LOS:
					{
						if (pEntity->GetIsTypeObject())
						{
							sprintf(str, "%s", static_cast<CObject*>(pEntity)->m_nObjectFlags.bTargettableWithNoLos ? "true" : "false");
							break;
						}
						else
						{
							sprintf(str, "Unknown");
							break;
						}
					}
				case CDebugScene::GADGET_IS_A_TARGET_PRIORITY:
					{
						if (pEntity->GetIsTypeObject())
						{
							sprintf(str, "%s", static_cast<CObject*>(pEntity)->m_nObjectFlags.bIsATargetPriority ? "true" : "false");
							break;
						}
						else
						{
							sprintf(str, "Unknown");
							break;
						}
					}
				case CDebugScene::GADGET_CAN_BE_TARGETTED_BY_PLAYER:
					{
						if (pEntity->GetIsTypeObject())
						{
							sprintf(str, "%s", static_cast<CObject*>(pEntity)->m_nObjectFlags.bCanBeTargettedByPlayer ? "true" : "false");
							break;
						}
						else
						{
							sprintf(str, "Unknown");
							break;
						}
					}

				case CDebugScene::GADGET_IS_CULL_SMALL_SHADOWS:
					{
						sprintf(str, "%s", (pEntity->GetBaseFlags() & fwEntity::RENDER_SMALL_SHADOW) ? "false" : "true");
						break;
					}

				case CDebugScene::GADGET_IS_DONT_CAST_SHADOWS:
					{
						const fwFlags16 &flags = pEntity->GetRenderPhaseVisibilityMask();
						bool dontCastShadows = !flags.IsFlagSet(VIS_PHASE_MASK_CASCADE_SHADOWS) && !flags.IsFlagSet(VIS_PHASE_MASK_PARABOLOID_SHADOWS);
						sprintf(str, "%s", dontCastShadows ? "true" : "false");
						break;
					}

				case CDebugScene::GADGET_IS_RENDER_ONLY_IN_REFLECTIONS:
					{
						fwFlags16 &visibilityMask = pEntity->GetRenderPhaseVisibilityMask();
						sprintf(str, "%s", ( visibilityMask & (u16)(~(VIS_PHASE_MASK_REFLECTIONS|VIS_PHASE_MASK_STREAMING))) ? "false" : "true" );
						break;
					}

				case CDebugScene::GADGET_IS_DONT_RENDER_IN_REFLECTIONS:
					{
						fwFlags16 &visibilityMask = pEntity->GetRenderPhaseVisibilityMask();
						const bool bIsRenderingInReflections = visibilityMask.AreAllFlagsSet(VIS_PHASE_MASK_REFLECTIONS);
						sprintf(str, "%s", !bIsRenderingInReflections ? "true" : "false");
						break;
					}

				case CDebugScene::GADGET_IS_RENDER_ONLY_IN_SHADOWS:
					{
						fwFlags16 &visibilityMask = pEntity->GetRenderPhaseVisibilityMask();
						sprintf(str, "%s", (visibilityMask & (u16)(~(VIS_PHASE_MASK_SHADOWS|VIS_PHASE_MASK_STREAMING))) ? "false" : "true" );
						break;
					}

				case CDebugScene::GADGET_IS_DONT_RENDER_IN_SHADOWS:
					{
						fwFlags16 &visibilityMask = pEntity->GetRenderPhaseVisibilityMask();
						sprintf(str, "%s", (visibilityMask & VIS_PHASE_MASK_SHADOWS) ? "false" : "true");
						break;
					}

				case CDebugScene::GADGET_LODGROUP_FLAGS:
					{
						const Drawable* pDrawable = pEntity->GetDrawable();
						u32 fl00 = 0xFFFF;
						u32 fl01 = 0xFFFF;
						u32 fl02 = 0xFFFF;
						u32 fl03 = 0xFFFF;

						if( pDrawable )
						{
							fl00 = pDrawable->GetLodGroup().GetBucketMask(0);
							fl01 = pDrawable->GetLodGroup().GetBucketMask(1);
							fl02 = pDrawable->GetLodGroup().GetBucketMask(2);
							fl03 = pDrawable->GetLodGroup().GetBucketMask(3);
						}
						
						sprintf(str, "%x - %x - %x - %x", fl00,fl01,fl02,fl03);
						
						break;
					}
				case CDebugScene::GADGET_LODMASK_FLAGS:
					{
						u32 shadowFlag		= (pEntity->GetPhaseLODs() >> LODDrawable::LODFLAG_SHIFT_SHADOW		) & LODDrawable::LODFLAG_MASK;
						u32 reflectionFlag	= (pEntity->GetPhaseLODs() >> LODDrawable::LODFLAG_SHIFT_REFLECTION	) & LODDrawable::LODFLAG_MASK;
						u32 waterFlag		= (pEntity->GetPhaseLODs() >> LODDrawable::LODFLAG_SHIFT_WATER		) & LODDrawable::LODFLAG_MASK;
						u32 mirrorFlag		= (pEntity->GetPhaseLODs() >> LODDrawable::LODFLAG_SHIFT_MIRROR		) & LODDrawable::LODFLAG_MASK;
						u32 lastLODIdx		= pEntity->GetLastLODIdx();
						u32 AltFade			= pEntity->GetUseAltFadeDistance() ? 1 : 0;

						sprintf(str, "%d - %d - %x - %x - %x - %x", AltFade, lastLODIdx, shadowFlag, reflectionFlag, waterFlag, mirrorFlag);
						
						break;
					}
				case CDebugScene::GADGET_IS_USED_IN_MP:
					{
						bool isUsedInMP = CMPObjectUsage::IsUsedInMP(pEntity);
						sprintf(str, "%s", isUsedInMP ? "true" : "false");
						break;
					}

				case CDebugScene::GADGET_IS_POPTYPE:
					{
						if(pEntity->GetIsPhysical())
						{
							CPhysical *pPhysical = static_cast<CPhysical*>(pEntity);
							const char *pPopTypeName = CTheScripts::GetPopTypeName(pPhysical->PopTypeGet());
							sprintf(str, "%s", pPopTypeName);
						}
						else
						{
							sprintf(str, "N/A");
						}
						break;
					}
				case CDebugScene::GADGET_OWNED_BY:
					{
						sprintf(str, "%d", (int)pEntity->GetOwnedBy());
						break;
					}
				case CDebugScene::GADGET_REPLAY_ID:
					{
#if GTA_REPLAY
						if(pEntity->GetIsPhysical())
						{
							CPhysical *pPhysical = static_cast<CPhysical*>(pEntity);
							sprintf(str, "0x%08X", pPhysical->GetReplayID().ToInt());
						}
#endif // GTA_REPLAY
						break;
					}

				case CDebugScene::GADGET_REPLAY_MAPHASH:
					{
#if GTA_REPLAY
						if(pEntity->GetIsTypeObject())
						{
							CObject *pObject = static_cast<CObject*>(pEntity);
							if(pObject->GetRelatedDummy())
							{
								sprintf(str, "%u", pObject->GetRelatedDummy()->GetHash());
							}
						}
#endif // GTA_REPLAY
						break;
					}

				case CDebugScene::GADGET_REPLAY_HIDECOUNT:
					{
#if GTA_REPLAY
						if(pEntity->GetIsTypeObject())
						{
 							CObject *pObject = static_cast<CObject*>(pEntity);
 							if(pObject->GetRelatedDummy())
 							{
								u32 hash = pObject->GetRelatedDummy()->GetHash();
								CReplayID id;
								FrameRef frameRef;
								if(CReplayMgr::IsMapObjectExpectedToBeHidden(hash, id, frameRef))
								{
									sprintf(str, "0x%08X, %u:%u", id.ToInt(), frameRef.GetBlockIndex(), frameRef.GetFrame());
								}
 							}
						}
#endif // GTA_REPLAY
						break;
					}

				default:
					{
						Assertf(false, "inspector gadget didn't handle case %i..", row);
						sprintf(str, "???");
						break;
					}
				}
			}
			pResult->SetString(str);
		}
	}
}
#endif // __BANK

const char *
CDebugScene::GetEntityDescription(const CEntity * pEntity)
{
	switch(pEntity->GetType())
	{
	case ENTITY_TYPE_NOTHING:
		return "Nothing";
	case ENTITY_TYPE_BUILDING:
		if(!pEntity->GetLodData().IsHighDetail())
			return "Lod";
		return "Building";
	case ENTITY_TYPE_VEHICLE:
		return "Vehicle";
	case ENTITY_TYPE_PED:
		return "Ped";
	case ENTITY_TYPE_OBJECT:
		return "Object";
	case ENTITY_TYPE_DUMMY_OBJECT:
		return "Dummy Object";
	case ENTITY_TYPE_PORTAL:
		return "Portal";
	case ENTITY_TYPE_MLO:
		return "MLO";
	}

	// if not handled above, get the 'official' type name ..
	return GetEntityTypeStr(pEntity->GetType());
}

CEntity* CDebugScene::GetBestFilteredEntity(const FocusFilter& filter)
{
	CEntity* pBestEntity = nullptr;
	float bestScore = -100000.f;
	const ScalarV sUpCloseDist(filter.upCloseDist);
	const int iNumPeds = CPed::GetPool()->GetSize();
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	for (int i = 0; i < iNumPeds; i++)
	{
		CPed* pPed = CPed::GetPool()->GetSlot(i);
		const bool isAnimal = (pPed && CPedType::IsAnimalType(pPed->GetPedType()));
		if (!pPed
			|| FocusEntities_IsInGroup(pPed)
			|| (filter.bDead != pPed->GetIsDeadOrDying())
			|| (!filter.bPlayer && pPed == pLocalPlayer)
			|| (!filter.bAnimal && isAnimal)
			|| (!filter.bBird && pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsBird())
			|| (!filter.bHuman && !isAnimal && pPed != pLocalPlayer)
			|| (filter.bRider && !pPed->GetVehiclePedInside())
			)
		{
			continue;
		}

		const float score = filter.CalculateScore(pPed);
		if (score > bestScore)
		{
			pBestEntity = pPed;
			bestScore = score;
		}
	}

	if (filter.bVehicle)
	{
		const int iNumVehicles = (int)CVehicle::GetPool()->GetSize();
		for (int i = 0; i < iNumVehicles; i++)
		{
			CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(i);
			if (!pVehicle || FocusEntities_IsInGroup(pVehicle))
			{
				continue;
			}

			const float score = filter.CalculateScore(pVehicle);
			if (score > bestScore)
			{
				pBestEntity = pVehicle;
				bestScore = score;
			}
		}
	}
	return pBestEntity;
}

float CDebugScene::FocusFilter::CalculateScore(CEntity* pEntity) const
{
	const Vec3V entPos(pEntity->GetTransform().GetPosition());
	Vec3V toEnt(entPos - camPos);
	const float dist = VEC3V_TO_VECTOR3(toEnt).Mag();
	VEC3V_TO_VECTOR3(toEnt).NormalizeSafe();
	const float dot = VEC3V_TO_VECTOR3(camFwd).Dot(VEC3V_TO_VECTOR3(toEnt));
	const float dotScore = FPRangeClamp(dot, minDot, 1.f);
	const float distScore = square(FPRamp(dist, 1.f, 120.f, 1.f, 0.f));
	return dotScore + distScore;
}

void CDebugScene::FocusAddClosestAIPed(FocusFilter& filter)
{
	// From the player or from in front of the camera
	filter.camFwd = VECTOR3_TO_VEC3V(camInterface::GetFront());
	filter.camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	if (!filter.bPlayer)
	{
		const Vec3V cameraFwd = filter.camFwd;
		const Vec3V posCalc = VECTOR3_TO_VEC3V(CPlayerInfo::ms_cachedMainPlayerPos - camInterface::GetPos());
		const float newPos = VEC3V_TO_VECTOR3(posCalc).Dot(VEC3V_TO_VECTOR3(cameraFwd));
		filter.camPos += VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(cameraFwd)* newPos);
	}
	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		const camFrame& freeCam = camInterface::GetDebugDirector().GetFreeCamFrame();
		filter.camPos = VECTOR3_TO_VEC3V(freeCam.GetPosition());
		filter.camFwd = VECTOR3_TO_VEC3V(freeCam.GetFront());
	}

	CEntity* pEntity = GetBestFilteredEntity(filter);
	if (!pEntity && filter.bPlayer)
	{
		pEntity = CGameWorld::FindLocalPlayer();
	}
	if (pEntity)
	{
		int index = -1;
		FocusEntities_Add(pEntity, index);
#if DEBUG_DRAW
		spdAABB bounds;
		pEntity->GetAABB(bounds);
		grcDebugDraw::BoxAxisAligned(bounds.GetMin(), bounds.GetMax(), Color_yellow, false, 30);
#endif 
	}

}

void CDebugScene::FocusEntities_Clear()
{
#if !__FINAL
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		FocusEntities_Set(NULL, i);
	}
#endif // !__FINAL
}
bool CDebugScene::FocusEntities_IsEmpty()
{
#if !__FINAL
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		if(ms_focusEntities[i])
		{
			return false;
		}
	}
#endif // !__FINAL
	return true;
}
CEntity* CDebugScene::FocusEntities_Get(int index)
{
	if(index >= FOCUS_ENTITIES_MAX)
	{
		return NULL;
	}
#if !__FINAL
	return ms_focusEntities[index];
#else
	return NULL;
#endif
}
bool CDebugScene::FocusEntities_IsInGroup(const CEntity* const pEntity, int& NOTFINAL_ONLY(out_index))
{
	if(!pEntity)
	{
		return false;
	}

#if !__FINAL
	for(int i = 0; i < FOCUS_ENTITIES_MAX; ++i)
	{
		if(pEntity == ms_focusEntities[i])
		{
			out_index = i;
			return true;
		}
	}
#endif // !__FINAL

	return false;
}
bool CDebugScene::FocusEntities_IsInGroup(const CEntity* const pEntity)
{
	int index = -1;
	return FocusEntities_IsInGroup(pEntity, index);
}
bool CDebugScene::FocusEntities_IsNearGroup(Vec3V_In vPos, float fThreshold)
{
#if !__FINAL
	const ScalarV fThresholdSq(square(fThreshold));
	for(int i = 0; i < FOCUS_ENTITIES_MAX; ++i)
	{
		if(ms_focusEntities[i] && IsLessThanAll(DistSquared(vPos,ms_focusEntities[i]->GetTransform().GetPosition()),fThresholdSq))
		{
			return true;
		}
	}
#endif // !__FINAL
	return false;
}
bool CDebugScene::FocusEntities_Add(CEntity* NOTFINAL_ONLY(pEntity), int& NOTFINAL_ONLY(out_index))
{
#if !__FINAL
	if(!FocusEntities_IsInGroup(pEntity, out_index))
	{
		for(int i = 0; i < FOCUS_ENTITIES_MAX; ++i)
		{
			if(!ms_focusEntities[i])
			{
				FocusEntities_Set(pEntity, i);
				out_index = i;
				return true;
			}
		}
	}
#endif // !__FINAL

	return false;
}

extern char gCurrentDisplayObject[];

#if __DEV
XPARAM(displayobject);
#endif // __DEV

void CDebugScene::FocusEntities_Set(CEntity* NOTFINAL_ONLY(pEntity), int index)
{
	if(index >= FOCUS_ENTITIES_MAX)
	{
		return;
	}

#if !__FINAL
	// Make sure the old entity in this slot is visible.
	if(ms_focusEntities[index])
	{
		if(ms_makeFocusEntitiesFlash)
		{
			ms_focusEntities[index]->SetIsVisibleForModule( SETISVISIBLE_MODULE_DEBUG, true );
		}
	}

	if (index == 0)
	{	
		aiTaskTree::ms_pFocusEntity = pEntity;
		if (pEntity)
		{
			DR_ONLY( debugPlayback::TrackEntity(pEntity, false) );
		}
	}

#if __BANK
	bool bActualChange = (ms_focusEntities[index] != pEntity);
#endif
	ms_focusEntities[index] = pEntity;

	//******************************************************************
	// Print out the entity's address here.
	// This is because the in-game font printing doesn't work reliably
	// enough in the CPedDebugVisualiser.
#if __BANK
	const char * pType = "Unknown";
	const char * pModelName = "Unknown";
	if(pEntity)
	{
		pType = "Entity";
		if(pEntity->GetIsTypePed()) pType = "Ped";
		else if(pEntity->GetIsTypeVehicle()) pType = "Vehicle";

		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if(pModelInfo)
		{
			pModelName = pModelInfo->GetModelName();
		}
	}

	#if __DEV
	if (!PARAM_displayobject.Get())
	{
		strncpy(gCurrentDisplayObject, pModelName, STORE_NAME_LENGTH);
	}
	#endif

	if (bActualChange)
	{
		Displayf("\n******************************************************************************************\n");
		Displayf("Focus Changed - Type = %s. Model Name = %s. Address = 0x%p", pType, pModelName, pEntity);
		if (pEntity && ms_logFocusEntitiesPosition)
		{
			Displayf("Focus Changed - Position = %.3f,%.3f,%.3f Roll = %.3f Pitch = %.3f Heading = %.3f", pEntity->GetTransform().GetPosition().GetXf(), pEntity->GetTransform().GetPosition().GetYf(), pEntity->GetTransform().GetPosition().GetZf(),
																										  pEntity->GetTransform().GetRoll(), pEntity->GetTransform().GetPitch(), pEntity->GetTransform().GetHeading());
		}
		Displayf("\n******************************************************************************************\n");
	}
#endif

	OnChangeFocusEntity(index);

#if __BANK
	NetworkDebug::UpdateFocusEntity(true);
#endif

#else
	return;
#endif
}
#if !__FINAL
static const u32 atStringHashNone = ATSTRINGHASH("None", 0x1D632BA1);
#endif // !FINAL
void CDebugScene::HandleFocusEntityTextBoxChange(void)
{
#if !__FINAL
	CEntity* pForceFocusEntity = NULL;
	if(atStringHash(ms_focusEntity0AddressString) != atStringHashNone)
	{
		sscanf(ms_focusEntity0AddressString, "0x%p", &pForceFocusEntity);
	}
	if(pForceFocusEntity != FocusEntities_Get(0))
	{
		FocusEntities_Set(pForceFocusEntity, 0);

#if __DEV
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(pForceFocusEntity->GetModelId());
		if(pModelInfo && pModelInfo->GetModelName())
		{
			formatf(ms_focusEntity0ModelInfoString, 64, "%s", pModelInfo->GetModelName());
		}
#endif // __DEV
	}
#endif // !FINAL
}
void CDebugScene::OnChangeFocusEntity(int NOTFINAL_ONLY(index))
{
#if !__FINAL
	if(index != 0)
	{
		return;
	}

	CEntity* pFocusEntity = FocusEntities_Get(0);
	if(!pFocusEntity)
	{
		formatf(ms_focusEntity0AddressString,   64, "%s", "None");
		
		DEV_ONLY( formatf(ms_focusEntity0ModelInfoString, 64, "%s", "None"); )

#if __BANK
		CPedDebugVisualiserMenu::OnChangeFocusPedCB(NULL);
#endif
	}
	else
	{
		formatf(ms_focusEntity0AddressString,   64, "0x%X", pFocusEntity);
#if __BANK

#if __DEV
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(pFocusEntity->GetModelId());
		if(pModelInfo && pModelInfo->GetModelName())
		{
			formatf(ms_focusEntity0ModelInfoString, 64, "%s", pModelInfo->GetModelName());
		}
#endif // __DEV

		if(pFocusEntity->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
			CPedGroup* pGroup = pFocusPed->GetPedsGroup();
			if(pGroup)
			{
				CPedDebugVisualiserMenu::m_iFormationComboIndex = pGroup->GetFormation()->GetFormationType();
				CPedDebugVisualiserMenu::m_fFormationSpacing = pGroup->GetFormation()->GetSpacing();
			}

			CPedDebugVisualiserMenu::OnChangeFocusPedCB(pFocusPed);
		}
		else
		{
			CPedDebugVisualiserMenu::m_iFormationComboIndex = 0;
			CPedDebugVisualiserMenu::m_fFormationSpacing = 0;
		}

		// Callback the list of interested systems to say there's been a change of focus entity
		
		for (int i = 0; i < ms_callbacks.GetCount(); i++)
		{
			ms_callbacks[i].Call(CallbackData(pFocusEntity));
		}
#endif // __BANK
		aiTaskTree::ms_pFocusEntity = pFocusEntity;
	}
#endif // !__FINAL
}


#if __DEV
void CDebugScene::UpdateAttachTest()
{
	IF_COMMERCE_STORE_RETURN();

	if(!(ms_bDrawAttachmentExtensions || ms_bDrawAttachmentEdges || ms_bDisplayAttachmentEdgeType || ms_bDisplayAttachmentEdgeData  || ms_bDisplayUpdateRecords
			|| ms_bRenderListOfAttachmentExtensions || ms_bDisplayAttachmentInvokingFile || ms_bDisplayAttachmentInvokingFunction || ms_bDisplayAttachmentPhysicsInfo))
	{
		//Avoid looping over all of the attachments if we'll render nothing
		return;
	}

	if( ms_bDebugRenderAttachmentOfFocusEntitiesOnly )
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			if(FocusEntities_Get(i) && FocusEntities_Get(i)->GetIsPhysical())
			{
				fwAttachmentEntityExtension const * pAttachExt = static_cast<CPhysical*>(FocusEntities_Get(i))->GetAttachmentExtension();

				if(pAttachExt)
					pAttachExt->RenderDebug(ms_bDrawAttachmentExtensions, ms_bDrawAttachmentEdges, ms_bDisplayAttachmentEdgeType, ms_bDisplayAttachmentEdgeData, ms_bDisplayAttachmentInvokingFunction, 
					ms_bDisplayAttachmentInvokingFile, ms_bDisplayAttachmentPhysicsInfo, ms_bDisplayUpdateRecords, ms_fAttachmentNodeRadius, ms_fAttachmentNodeRelPosWorldUp, ms_fAttachmentNodeEntityDist);
			}
		}
	}
	else
	{
		int const n =  fwAttachmentEntityExtension::GetPool()->GetSize();
		const Vec2V attachmentListScreenPos(0.03f, 0.05f);
		u32 lineNum = 0u;
		
		for(s32 i = 0 ; i < n; i++)
		{
			fwAttachmentEntityExtension const * pAttachExt = fwAttachmentEntityExtension::GetPool()->GetSlot(i);

			if(pAttachExt)
			{
				pAttachExt->RenderDebug(ms_bDrawAttachmentExtensions, ms_bDrawAttachmentEdges, ms_bDisplayAttachmentEdgeType, ms_bDisplayAttachmentEdgeData, 
					ms_bDisplayAttachmentInvokingFunction, ms_bDisplayAttachmentInvokingFile, ms_bDisplayAttachmentPhysicsInfo, ms_bDisplayUpdateRecords, ms_fAttachmentNodeRadius, ms_fAttachmentNodeRelPosWorldUp, ms_fAttachmentNodeEntityDist);

				if(ms_bRenderListOfAttachmentExtensions && pAttachExt->GetAttachState() != ATTACH_STATE_NONE)
				{
					char printBuffer[1024];
					snprintf(printBuffer, 1024, "%s. %s", 
						fwAttachmentEntityExtension::GetAttachStateString( pAttachExt->GetAttachState()), pAttachExt->DebugGetInvokingFunction() );
					
					Vec2V lineScreenPos = attachmentListScreenPos + Vec2V(0.0f, 0.0125f * (float)lineNum++);
						
					grcDebugDraw::Text(lineScreenPos, Color_black, printBuffer);

					snprintf(printBuffer, 1024, "    extension:0x%p. m_pThisEntity:0x%p(%s). m_pAttachParent:0x%p(%s)", 
						pAttachExt, pAttachExt->GetThisEntity(), pAttachExt->GetThisEntity()->GetArchetype()->GetModelName(), 
						pAttachExt->GetAttachParentForced(), pAttachExt->GetAttachParentForced() ? pAttachExt->GetAttachParentForced()->GetArchetype()->GetModelName() : "");
					
						 lineScreenPos = attachmentListScreenPos + Vec2V(0.0f, 0.0125f * (float)lineNum++);
						
					grcDebugDraw::Text(lineScreenPos, Color_black, printBuffer);

				}

			}
		}
	}
}

void CDebugScene::AttachFocusEntitiesTogether()
{	

	// Attach focus entity 0 to focus entity 1
	// Will only work if both are physical
	if(FocusEntities_Get(0) && FocusEntities_Get(0)->GetIsPhysical() 
		&& FocusEntities_Get(1) && FocusEntities_Get(1)->GetIsPhysical())
	{
		CPhysical* pPhys0 = static_cast<CPhysical*>(FocusEntities_Get(0));
		CPhysical* pPhys1 = static_cast<CPhysical*>(FocusEntities_Get(1));

		Matrix34 matRotate;
		matRotate.DotTranspose(MAT34V_TO_MATRIX34(pPhys0->GetMatrix()),MAT34V_TO_MATRIX34(pPhys1->GetMatrix()));
		Quaternion vPivot;
		vPivot.FromMatrix34(matRotate);

		u32 iAttachFlags = ATTACH_STATE_BASIC;
		if(ms_bAttachFlagAutoDetachOnRagdoll) 
			iAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
		if(ms_bAttachFlagCollisionOn)
			iAttachFlags |= ATTACH_FLAG_COL_ON;
		if(ms_bAttachFlagDeleteWithParent)
			iAttachFlags |= ATTACH_FLAG_DELETE_WITH_PARENT;

		pPhys0->AttachToPhysicalBasic(pPhys1,-1,iAttachFlags,&matRotate.d,&vPivot);
	}
}

void CDebugScene::AttachFocusEntitiesTogetherPhysically()
{	

	// Attach focus entity 0 to focus entity 1
	// Will only work if both are physical
	if(FocusEntities_Get(0) && FocusEntities_Get(0)->GetIsPhysical() 
		&& FocusEntities_Get(1) && FocusEntities_Get(1)->GetIsPhysical())
	{
		CPhysical* pPhys0 = static_cast<CPhysical*>(FocusEntities_Get(0));
		CPhysical* pPhys1 = static_cast<CPhysical*>(FocusEntities_Get(1));

		u32 iAttachFlags = ATTACH_STATE_PHYSICAL | ATTACH_FLAG_POS_CONSTRAINT;
		if(ms_bAttachFlagAutoDetachOnRagdoll) 
			iAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
		if(ms_bAttachFlagCollisionOn)
			iAttachFlags |= ATTACH_FLAG_COL_ON;
		if(ms_bAttachFlagDeleteWithParent)
			iAttachFlags |= ATTACH_FLAG_DELETE_WITH_PARENT;
		if(ms_bAttachFlagRotConstraint)
			iAttachFlags |= ATTACH_FLAG_ROT_CONSTRAINT;

		pPhys0->AttachToPhysicalUsingPhysics(pPhys1,-1,-1,iAttachFlags, VEC3V_TO_VECTOR3(pPhys0->GetTransform().GetPosition()) ,0.0f);
	}
}

void CDebugScene::AttachFocusEntitiesToWorld()
{
	for(int i = 0; i < FOCUS_ENTITIES_MAX; i++)
	{
		if(FocusEntities_Get(i) && FocusEntities_Get(i)->GetIsPhysical())
		{
			u32 iAttachFlags = ATTACH_STATE_WORLD;
			if(ms_bAttachFlagAutoDetachOnRagdoll) 
				iAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
			if(ms_bAttachFlagCollisionOn)
				iAttachFlags |= ATTACH_FLAG_COL_ON;
			if(ms_bAttachFlagDeleteWithParent)
				iAttachFlags |= ATTACH_FLAG_DELETE_WITH_PARENT;

			Quaternion quat = QUATV_TO_QUATERNION(QuatVFromMat33V(FocusEntities_Get(i)->GetMatrix().GetMat33()));
			static_cast<CPhysical*>(FocusEntities_Get(i))->AttachToWorldBasic(iAttachFlags,VEC3V_TO_VECTOR3(FocusEntities_Get(i)->GetTransform().GetPosition()),&quat);
		}
	}
}

void CDebugScene::DetachFocusEntity()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		if(FocusEntities_Get(i) && FocusEntities_Get(i)->GetIsPhysical())
		{
			u16 iDetachFlags = 0;
			if(ms_bDetachFlagActivatePhysics)
				iDetachFlags |= DETACH_FLAG_ACTIVATE_PHYSICS;
			if(ms_bDetachFlagApplyVelocity)
				iDetachFlags |= DETACH_FLAG_APPLY_VELOCITY;
			if(ms_bDetachFlagNoCollisionUntilClear)
				iDetachFlags |= DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR;
			if(ms_bDetachFlagIgnoreSafePositionCheck)
				iDetachFlags |= DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK;

			CPhysical* pPhysical = static_cast<CPhysical*>(FocusEntities_Get(i));
			pPhysical->DetachFromParent(iDetachFlags);
		}
	}
}

void CDebugScene::DeleteFocusEntities()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		if(FocusEntities_Get(i))
		{
			CGameWorld::RemoveEntity(FocusEntities_Get(i));
		}
	}
}
#endif	// __DEV


//
// name:		CDebugScene::RenderCollisionGeometry
// description:	
void CDebugScene::RenderCollisionGeometry(void)
{
	IF_COMMERCE_STORE_RETURN();

#if !__FINAL //No debug cameras in final builds.
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CGameWorld::FindLocalPlayerCoors();
#else
	Vector3 vOrigin = CGameWorld::FindLocalPlayerCoors();
#endif

	phLevelNew * phLevel = CPhysics::GetLevel();
#if ENABLE_PHYSICS_LOCK
	phIterator iterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
	phIterator iterator;
#endif	// ENABLE_PHYSICS_LOCK

	// Use this option to get all objects matching the req'd state
	iterator.InitCull_All();
	iterator.SetStateIncludeFlags(phLevelNew::STATE_FLAG_FIXED);

	u16 iIndexInLevel = phLevel->GetFirstCulledObject(iterator);

	while(iIndexInLevel != phInst::INVALID_INDEX)
	{
		phInst * pInstance = phLevel->GetInstance(iIndexInLevel);

		if(pInstance)
		{
			phArchetype * pArchetype = pInstance->GetArchetype();
			const rage::phBound * pBound = pArchetype->GetBound();

			if(pBound)
			{
				DrawBoundCollisionGeometry(vOrigin, pBound);
			}
		}

		iIndexInLevel = phLevel->GetNextCulledObject(iterator);
	}
}


//
// name:		CDebugScene::DrawBoundCollisionGeometry
// description:	
void CDebugScene::DrawBoundCollisionGeometry(const Vector3 & BANK_ONLY(vOrigin), const rage::phBound * BANK_ONLY(pBound))
{
#if __BANK
	int iBoundType = pBound->GetType();
	s32 r;

	Vector3 vTriCentroid;
	Vector3 vDiff;
	float fDistSqr;
//	static const float fOneThird = 1.0f / 3.0f;
	static const float fMaxDistSqr = 30.0f * 30.0f;
	int v;

	switch(iBoundType)
	{
		// All of these exist as types and might want to draw one day
		//  Seems silly to just assert that they occur (in default condition below)
		case phBound::CAPSULE:
		USE_TAPERED_CAPSULE_ONLY(case phBound::TAPERED_CAPSULE:)
		case phBound::BOX:
		USE_GEOMETRY_CURVED_ONLY(case phBound::GEOMETRY_CURVED:)
		USE_GRIDS_ONLY(case phBound::GRID:)
		USE_RIBBONS_ONLY(case phBound::RIBBON:)
		USE_SURFACES_ONLY(case phBound::SURFACE:)
		case phBound::TRIANGLE:
		case phBound::DISC:
		case phBound::CYLINDER:
		{
			break;
		}
		case phBound::SPHERE:
		{
			// These spheres are just a byproduct of the export process (You can't create a
			// composite simply by adding meshes).  So we'll ignore them..

			//rage::phBoundSphere * pSphere = (rage::phBoundSphere*)pBound;
			//Color32 col(255,0,0,255);
			//grcDebugDraw::Sphere(vBoundCenter.x, vBoundCenter.y, vBoundCenter.z, pSphere->GetRadius(), col);
			break;
		}
		// all these bounds are derived from geometry
		case phBound::BVH:
		case phBound::GEOMETRY:
		{
			rage::phBoundGeometry * pGeom = (rage::phBoundGeometry*)pBound;
			int iNumPolys = pGeom->GetNumPolygons();

			for(int p=0; p<iNumPolys; p++)
			{
				const phPolygon & poly = pGeom->GetPolygon(p);
				const rage::phPrimitive *curPrim = reinterpret_cast<const rage::phPrimitive *>(&poly);
				// TODO -- Draw other types of bounds?
				if(curPrim->GetType() == rage::PRIM_TYPE_POLYGON)
				{
					bool drawThis = false;
					for(v=0; v<POLY_MAX_VERTICES; v++)
					{
						rage::phPolygon::Index vIndex0 = poly.GetVertexIndex(v);
						Vector3 vert0 = VEC3V_TO_VECTOR3(pGeom->GetVertex(vIndex0));
						fDistSqr = (vert0 - vOrigin).Mag2();
						if(fDistSqr <= fMaxDistSqr)
						{
							drawThis = true;
							break;
						}
					}

					if(drawThis)
					{
						r = p * 8;
						r &= 0xFF;
						Color32 col(64,r,64,255);

						int lastv = POLY_MAX_VERTICES-1;
						for(v=0; v<POLY_MAX_VERTICES; v++)
						{
							rage::phPolygon::Index vIndex0 = poly.GetVertexIndex(lastv);
							rage::phPolygon::Index vIndex1 = poly.GetVertexIndex(v);
							Vector3 vert0 = VEC3V_TO_VECTOR3(pGeom->GetVertex(vIndex0));
							Vector3 vert1 = VEC3V_TO_VECTOR3(pGeom->GetVertex(vIndex1));
							grcDebugDraw::Line(vert0, vert1, col);
							lastv = v;
						}
					}
				}
			}

			break;
		}
		case phBound::COMPOSITE:
		{
			rage::phBoundComposite * pComposite = (rage::phBoundComposite*)pBound;
			int iNumBounds = pComposite->GetNumBounds();

			for(int b=0; b<iNumBounds; b++)
			{
				phBound * pCompositeBound = pComposite->GetBound(b);
				DrawBoundCollisionGeometry(vOrigin, pCompositeBound);
			}
			break;
		}
		default:
		{
			Assert(0);
			break;
		}
	}
#endif // __BANK	
}

//
// name:		CDebugScene::DrawEntityBoundingBox
// description:	Draw a bounding box for an entity
void CDebugScene::DrawEntityBoundingBox(CEntity* pEntity, Color32 color, bool bDrawWorldAABB, bool bDrawGeometryBoxes)
{
	Matrix34 matrix = MAT34V_TO_MATRIX34(pEntity->GetNonOrthoMatrix());

	// Draw the bounding-box
	Vector3 vMin, vMax;
	if(pEntity->IsArchetypeSet())
	{
		if (bDrawGeometryBoxes)
		{
			const Drawable* pDrawable = pEntity->GetDrawable();

			if (pDrawable)
			{
				const rmcLodGroup& group = pDrawable->GetLodGroup();

				if (group.ContainsLod(LOD_HIGH))
				{
					const grmModel& model = group.GetLodModel0(LOD_HIGH);
					const int numGeoms = model.GetGeometryCount();

					if (numGeoms > 1)
					{
						for (int i = 0; i < numGeoms; i++)
						{
							const spdAABB& aabb = model.GetGeometryAABB(i);
							const Vector3 vMin = VEC3V_TO_VECTOR3(aabb.GetMin());
							const Vector3 vMax = VEC3V_TO_VECTOR3(aabb.GetMax());

							Draw3DBoundingBox(vMin, vMax, matrix, color);
						}
					}
					else
					{
						const spdAABB& aabb = model.GetModelAABB();
						const Vector3 vMin = VEC3V_TO_VECTOR3(aabb.GetMin());
						const Vector3 vMax = VEC3V_TO_VECTOR3(aabb.GetMax());

						Draw3DBoundingBox(vMin, vMax, matrix, color);
					}
				}
			}
		}
		else
		{
			vMin = pEntity->GetBoundingBoxMin();
			vMax = pEntity->GetBoundingBoxMax();
		}
	}
	else if(pEntity->GetCurrentPhysicsInst())
	{
		vMin = VEC3V_TO_VECTOR3(pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetBoundingBoxMin());
		vMax = VEC3V_TO_VECTOR3(pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetBoundingBoxMax());
	}
	else
		return;

	if (bDrawWorldAABB)
	{
		spdAABB temp(RCC_VEC3V(vMin), RCC_VEC3V(vMax));
		temp.Transform(RCC_MAT34V(matrix));
		vMin = VEC3V_TO_VECTOR3(temp.GetMin());
		vMax = VEC3V_TO_VECTOR3(temp.GetMax());
		matrix.Identity();
	}

	Draw3DBoundingBox(vMin, vMax, matrix, color);
}


//
// name:		CDebugScene::Draw3DBoundingBox
// description:	Draw a bounding box for an entity
void CDebugScene::Draw3DBoundingBox(const Vector3 & BANK_ONLY(vMin), const Vector3 & BANK_ONLY(vMax), const Matrix34 & BANK_ONLY(mat), Color32 BANK_ONLY(color))
{
#if __BANK
	s32 v;
	Vector3 vCorners[8] = {
		Vector3(vMin.x, vMin.y, vMin.z), Vector3(vMax.x, vMin.y, vMin.z), Vector3(vMax.x, vMax.y, vMin.z), Vector3(vMin.x, vMax.y, vMin.z),
		Vector3(vMin.x, vMin.y, vMax.z), Vector3(vMax.x, vMin.y, vMax.z), Vector3(vMax.x, vMax.y, vMax.z), Vector3(vMin.x, vMax.y, vMax.z)
	};
	for(v=0; v<8; v++)
		mat.Transform(vCorners[v]);

	for(v=0; v<4; v++)
	{
		grcDebugDraw::Line(vCorners[v], vCorners[(v+1)%4], color);
		grcDebugDraw::Line(vCorners[v+4], vCorners[((v+1)%4)+4], color);
		grcDebugDraw::Line(vCorners[v], vCorners[v+4], color);
	}
#endif // __BANK	
}

void CDebugScene::DrawEntitySubBounds(CEntity * pEntity, Color32 color)
{
	phInst* pInst = pEntity->GetFragInst();
	if(pInst==NULL)
		pInst = pEntity->GetCurrentPhysicsInst();

	phBound* pBound = pInst->GetArchetype()->GetBound();
	Matrix34 currMat = RCC_MATRIX34(pInst->GetMatrix());
	DrawSubBounds(pInst, pBound, &currMat, color);

	/*
	Matrix34 compoundMat;
	phBound * pParentBound = pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound();

	if(pParentBound->GetType()==phBound::COMPOSITE)
	{
		phBoundComposite * pComposite = (phBoundComposite*)pParentBound;
		int iNumComponents = pComposite->GetNumBounds();
		for(int c=0; c<iNumComponents; c++)
		{
			const phBound * pSubBound = pComposite->GetBound(c);
			const Matrix34 & subMat = pComposite->GetCurrentMatrix(c);
			const Vector3 & vMin = pSubBound->GetBoundingBoxMin();
			const Vector3 & vMax = pSubBound->GetBoundingBoxMax();

			//compoundMat.Dot(subMat, pEntity->GetMatrix());
			compoundMat.Dot(pEntity->GetMatrix(), subMat);
			Draw3DBoundingBox(vMin, vMax, compoundMat, color);
		}
	}
	*/
}

void CDebugScene::DrawSubBounds(phInst *pInst, phBound* pBound, Matrix34* pCurrMat, Color32 color)
{
	// recurse with any composite bounds
	if (pBound->GetType() == phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
		for (s32 i=0; i<pBoundComposite->GetNumBounds(); i++)
		{
			// check this bound is still valid (smashable code may have removed it)
			phBound* pChildBound = pBoundComposite->GetBound(i);
			if (pChildBound)
			{
				// calc the new matrix
				Matrix34 newMat = *pCurrMat;
				Matrix34 thisMat = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(i));
				newMat.DotFromLeft(thisMat);	

				// render the bound
				DrawSubBounds(pInst, pChildBound, &newMat, color);
			}
		}
	}

	const Vector3 & vMin = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMin());
	const Vector3 & vMax = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMax());

	Draw3DBoundingBox(vMin, vMax, *pCurrMat, color);
}


#if __DEV
void PrintBoundsInfo(phBound* pBound)
{
	if(pBound==NULL)
		return;

	int nNumPolys = 0;
	if(pBound->GetType()==phBound::GEOMETRY)
		nNumPolys = ((phBoundGeometry*)pBound)->GetNumPolygons();
	else if(pBound->GetType()==phBound::BVH)
		nNumPolys = ((phBoundBVH*)pBound)->GetNumPolygons();
	else if(pBound->GetType()==phBound::COMPOSITE)
		nNumPolys = ((phBoundComposite*)pBound)->GetNumBounds();

	grcDebugDraw::AddDebugOutput("Collision model type: %s, No. polys %d", pBound->GetTypeString(), nNumPolys);

	// if this is a composite we want to itterate through the bounds it contains
	if(pBound->GetType()==phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
		for(int i=0; i<pBoundComposite->GetNumBounds(); i++)
		{
			PrintBoundsInfo(pBoundComposite->GetBound(i));
		}
	}
}
#endif

//
// name:		CDebugScene::PrintInfoAboutEntity
// description:	
void CDebugScene::PrintInfoAboutEntity(CEntity* pEntity)
{
	CBaseModelInfo *pBMI = pEntity->GetBaseModelInfo();
	if (pBMI)
	{
#if DEBUG_DRAW

		const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		grcDebugDraw::AddDebugOutput(
			"Looking at %s : %s (0x%x) MI:%d LOD:%d Collision:%d  LevelIndex:%d Posn:%.1f,%.1f,%.1f Attr:%d",
			GetEntityDescription(pEntity),
	#if !__FINAL
			(char*)pBMI->GetModelName(),
	#else
			"-",
	#endif
			pEntity,
			pEntity->GetBaseModelInfo()->GetStreamSlot(),
			(s32)pEntity->GetLodDistance(),
			(pEntity->GetCurrentPhysicsInst() != NULL),
			(pEntity->GetCurrentPhysicsInst() ? pEntity->GetCurrentPhysicsInst()->GetLevelIndex() : -1),
			vEntityPosition.x, vEntityPosition.y, vEntityPosition.z,
			pBMI->GetAttributes()
			);

#endif // DEBUG_DRAW

#if __DEV
		// print some info about the collision bounds
		if(pEntity->GetPhysArch())
		{
			PrintBoundsInfo(pEntity->GetPhysArch()->GetBound());
		}
#endif

		if(pEntity->GetIsPhysical())
		{
#if DEBUG_DRAW
			CPhysical * pPhysical = (CPhysical*)pEntity;
			if(pEntity->IsBaseFlagSet(fwEntity::IS_FIXED))
				grcDebugDraw::AddDebugOutput("Entity is fixed");
			else if(pPhysical->GetPhysArch())
				grcDebugDraw::AddDebugOutput("Entity is NOT fixed, and has mass %.1f", pPhysical->GetMass());
#endif // DEBUG_DRAW
		}
#if DEBUG_DRAW
		u32 iPathServerID = DYNAMIC_OBJECT_INDEX_NONE;
		if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			iPathServerID = ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex();
		}
		else if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
		{
			iPathServerID = ((CObject*)pEntity)->GetPathServerDynamicObjectIndex();
		}
		if(iPathServerID != DYNAMIC_OBJECT_INDEX_NONE)
		{
			grcDebugDraw::AddDebugOutput("Entity is in PathServer (m_iPathServerDynObjId = %i)", iPathServerID);
		}
		if(pBMI->GetIsLadder())
		{
			grcDebugDraw::AddDebugOutput("Entity is a ladder object.");
		}
		if(pBMI->GetIsFixedForNavigation())
		{
			grcDebugDraw::AddDebugOutput("Entity is considered fixed for navigation purposes.");
		}
		if(pBMI->GetNotAvoidedByPeds())
		{
			grcDebugDraw::AddDebugOutput("Entity is marked as NOT AVOIDED BY PEDS.");
		}
		if(pBMI->GetDoesNotProvideAICover())
		{
			grcDebugDraw::AddDebugOutput("Entity is marked as NOT PROVIDING AI COVER.");
		}
		if(pBMI->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT))
		{
			grcDebugDraw::AddDebugOutput("Entity is marked with MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT.");
		}
#endif // DEBUG_DRAW
	} // if(pBMI != NULL)...
	// found entity no CBaseModelInfo. It must be attached to a static bound
	else
	{
#if DEBUG_DRAW
	#if !__FINAL
		grcDebugDraw::AddDebugOutput("Static bound: %s %s", g_StaticBoundsStore.GetName(strLocalIndex(pEntity->GetIplIndex())), pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetFilename());
	#else
		grcDebugDraw::AddDebugOutput("Static bound: %s", "-");
	#endif
#endif // DEBUG_DRAW
#if __DEV
	int nBoundType = pEntity->GetPhysArch()->GetBound()->GetType();
	int nNumPolys = 0;
	if(nBoundType==phBound::BVH)
		nNumPolys = ((phBoundBVH*)pEntity->GetPhysArch()->GetBound())->GetNumPolygons();
	else
		Assertf(false, "Static bound is not an Octree or BVH bound!");
	grcDebugDraw::AddDebugOutput("Collision model type: %s, No. polys %d", pEntity->GetPhysArch()->GetBound()->GetTypeString(), nNumPolys);
#endif // DEBUG_DRAW
	}

}// end of PrintInfoAboutEntity()...

#if __BANK
void CDebugScene::PrintCoverTuningForEntity(CEntity* pEntity)
{
	if( pEntity && pEntity->GetIsTypeObject() )
	{
		CObject* pObject = static_cast<CObject*>(pEntity);
		const CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
		if( pModelInfo )
		{
			const CCoverTuning& coverTuning = CCoverTuningManager::GetInstance().GetTuningForModel(pModelInfo->GetModelNameHash());
			Vector3 vecPosition = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
			vecPosition.x = rage::Floorf(vecPosition.x + 0.5f);
			vecPosition.y = rage::Floorf(vecPosition.y + 0.5f);
			vecPosition.z = rage::Floorf(vecPosition.z + 0.5f);

			const char* tuningName = CCoverTuningManager::GetInstance().FindNameForTuning(coverTuning);

			grcDebugDraw::AddDebugOutput("Model: %s, Cover Tuning: %s", pObject->GetModelName(), tuningName);
		}
	}
}
#endif // __BANK


//
// name:		CDebugScene::Draw3DBoundingBoxOnVMap
// description:	Draw a bounding box for an entity on the vector map.
void CDebugScene::DrawEntityBoundingBoxOnVMap(CEntity* pEntity, Color32 colour)
{
	// Draw the bounding-box

	spdAABB localBox;
	if(pEntity->IsArchetypeSet())
	{
		const Vec3V vMin = RCC_VEC3V(pEntity->GetBoundingBoxMin());
		const Vec3V vMax = RCC_VEC3V(pEntity->GetBoundingBoxMax());
		localBox.Set(vMin, vMax);
	}
	else if(pEntity->GetCurrentPhysicsInst())
	{
		const Vec3V vMin = pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetBoundingBoxMin();
		const Vec3V vMax = pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetBoundingBoxMax();
		localBox.Set(vMin, vMax);
	}
	else
	{
		return;
	}

	spdAABB box = localBox;
	box.Transform(pEntity->GetMatrix());
	fwVectorMap::Draw3DBoundingBoxOnVMap(box, colour);
}


//
// name:		CDebugScene::GetMouse0to1
// description:	Returns where the mouse is pointing as a 2d screen vector in 0 to 1 space.
Vector2 CDebugScene::GetMouse0to1(void)
{
	// Get the raw mouse position.
	const float  mouseScreenX = static_cast<float>(ioMouse::GetX());
	const float  mouseScreenY = static_cast<float>(ioMouse::GetY());

	// Convert the mouse position into screen o to 1 coordinates.
	const float mouseScreen0to1X = mouseScreenX / grcViewport::GetDefaultScreen()->GetWidth();
	const float mouseScreen0to1Y = mouseScreenY / grcViewport::GetDefaultScreen()->GetHeight();

	// Hand back the 2d screen vector in 0 to one space.
	return Vector2(mouseScreen0to1X, mouseScreen0to1Y);
}


//
// name:		CDebugScene::GetMouseLeftPressed
// description:	Returns whether the left mouse button has been clicked this frame.
bool CDebugScene::GetMouseLeftPressed(void)
{
	return (ioMouse::GetPressedButtons()&ioMouse::MOUSE_LEFT);
}
bool CDebugScene::GetMouseLeftReleased(void)
{
	return (ioMouse::GetReleasedButtons()&ioMouse::MOUSE_LEFT);
}

//
// name:		CDebugScene::GetMouseRightPressed
// description:	Returns whether the left mouse button has been clicked this frame.
bool CDebugScene::GetMouseRightPressed(void)
{
	return (ioMouse::GetPressedButtons()&ioMouse::MOUSE_RIGHT) != 0;
}

bool CDebugScene::GetMouseRightReleased(void)
{
	return (ioMouse::GetReleasedButtons()&ioMouse::MOUSE_RIGHT) != 0;
}

bool CDebugScene::GetMouseMiddlePressed(void)
{
	return (ioMouse::GetPressedButtons()&ioMouse::MOUSE_MIDDLE) != 0;
}

bool CDebugScene::GetMouseMiddleReleased(void)
{
	return (ioMouse::GetReleasedButtons()&ioMouse::MOUSE_MIDDLE) != 0;
}

//
// name:		CDebugScene::GetMousePointing
// description:	Returns where the mouse is pointing
void CDebugScene::GetMousePointing( Vector3& vMouseNear, Vector3& vMouseFar, bool onVectorMap )
{
	// Get where the mouse is pointing as a 2d screen vector in 0 to 1 space.
	Vector2 mouseScreen0to1 = CDebugScene::GetMouse0to1();

	if(onVectorMap)
	{

		// Convert the screen0to1 coordinates into World coordinates.
		float mouseWorldX=0;
		float mouseWorldY=0;
		CVectorMap::ConvertPointMapToWorld(mouseScreen0to1.x, mouseScreen0to1.y, mouseWorldX, mouseWorldY);

		const Vector3 clickPosWorldFlat(mouseWorldX, mouseWorldY, 0.0f);

		vMouseNear.Set(clickPosWorldFlat.x, clickPosWorldFlat.y, 1000.0f);
		vMouseFar.Set(clickPosWorldFlat.x, clickPosWorldFlat.y, -1000.0f);
	}
	else
	{
		// Fix for reverse transform not working with scaled window
		const float viewportX = mouseScreen0to1.x * gVpMan.GetGameViewport()->GetGrcViewport().GetWidth();
		const float viewportY = mouseScreen0to1.y * gVpMan.GetGameViewport()->GetGrcViewport().GetHeight();

		gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(viewportX, viewportY, (Vec3V&)vMouseNear, (Vec3V&)vMouseFar);
	}
}

//
// Name			CDebugScene::GetWorldPosIn
// Returns a world point in screen space
void CDebugScene::GetScreenPosFromWorldPoint(const Vector3& vWorldPos, float& fWindowX, float& fWindowY)
{
	gVpMan.GetGameViewport()->GetGrcViewport().Transform((Vec3V&)vWorldPos, fWindowX, fWindowY);
	
	fWindowX=( fWindowX*grcViewport::GetDefaultScreen()->GetWidth() ) /gVpMan.GetGameViewport()->GetGrcViewport().GetWidth();
	fWindowY=( fWindowY*grcViewport::GetDefaultScreen()->GetHeight() ) /gVpMan.GetGameViewport()->GetGrcViewport().GetHeight();

}

//
// name:		CDebugScene::GetWorldPositionUnderMouse
// description:	Return the world position under the mouse pointer
bool CDebugScene::GetWorldPositionFromScreenPos(float &fScreenX, float &fScreenY, Vector3& posn, s32 iOptionalFlags, Vector3* pvNormal, void **entity)
{
	Vector3	vMouseNear, vMouseFar;

	fScreenX=( fScreenX/grcViewport::GetDefaultScreen()->GetWidth() ) *gVpMan.GetGameViewport()->GetGrcViewport().GetWidth();
	fScreenY=( fScreenY/grcViewport::GetDefaultScreen()->GetHeight() ) *gVpMan.GetGameViewport()->GetGrcViewport().GetHeight();
	

	gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(fScreenX, fScreenY,(Vec3V&)vMouseNear,(Vec3V&)vMouseFar);


	if(iOptionalFlags == 0)
	{
		iOptionalFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
	}

	// check for nans in position passed in.
	if(vMouseNear==vMouseNear && vMouseFar==vMouseFar)
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<> probeResult;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(vMouseNear, vMouseFar);
		probeDesc.SetIncludeFlags(iOptionalFlags);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			posn = probeResult[0].GetHitPosition();

			phInst* pInst = probeResult[0].GetHitInst();
			if( pInst )
			{
				CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
				if( pEntity && entity)
					*entity = pEntity;
			}

			if( pvNormal )
			{
				*pvNormal = probeResult[0].GetHitNormal();
			}
			return true;
		}
	}
	return false;
}



//
// name:		CDebugScene::GetEntityUnderMouse
// description:	Return the entity under the mouse pointer
CEntity* CDebugScene::GetEntityUnderMouse( Vector3* pvOptionalIntersectionPosition, s32* piOptionalComponent, s32 iOptionalFlags)
{
	Vector3 mouseStart, mouseEnd;
	GetMousePointing(mouseStart, mouseEnd, ms_doClickTestsViaVectorMapInsteadViewport);
	if(	ms_doClickTestsViaVectorMapInsteadViewport &&
		GetMouseLeftPressed())
	{
		// Draw a temporal marker to show where we clicked.
		const Vector3 pos((mouseStart + mouseEnd) * 0.5f);
		const Color32 col(255,255,255,255);
		const  float finalRadius = 40.0f;
		const  u32 lifetime = 2000;
		CVectorMap::MakeEventRipple( pos,
			finalRadius,// Final radius in meters.
			lifetime,// Lifetime in milliseconds.
			col);
	}

	Vector3	StartPos;

	if( iOptionalFlags == 0 )
	{
		iOptionalFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER | ArchetypeFlags::GTA_PROJECTILE_TYPE | ArchetypeFlags::GTA_PLANT_TYPE;
	}
#if __DEV
	if(!rage::FPIsFinite(mouseStart.x) || !rage::FPIsFinite(mouseStart.y) || !rage::FPIsFinite(mouseStart.z))
		return NULL;
	if(!rage::FPIsFinite(mouseEnd.x) || !rage::FPIsFinite(mouseEnd.y) || !rage::FPIsFinite(mouseEnd.z))
		return NULL;
#endif

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(mouseStart, mouseEnd);
	probeDesc.SetIncludeFlags(iOptionalFlags); 
	probeDesc.SetTypeFlags(TYPE_FLAGS_ALL);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		if( pvOptionalIntersectionPosition )
		{
			*pvOptionalIntersectionPosition = probeResult[0].GetHitPosition();
		}
		if( piOptionalComponent )
		{
			*piOptionalComponent = (u16) probeResult[0].GetHitComponent();
		}
		return CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());
	}

	return NULL;
}

void CDebugScene::UpdateFocusEntityDragging()
{
	IF_COMMERCE_STORE_RETURN();

#if __BANK
	Vector3 thisFrameWorldPosition(VEC3_ZERO);

	//right click and drag the mouse to alter the entity heading
	if (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)
	{
		if(!(ioMapper::DebugKeyDown(KEY_UP) || ioMapper::DebugKeyDown(KEY_RIGHT))) //check that we are not rotating round another axis  
		{
			CEntity* pEntity = FocusEntities_Get(0);
			if (pEntity)
			{	

				TUNE_GROUP_FLOAT(DEBUG_RENDER, rotationMarkerBoundRadiusMult, 1.0f, 0.001f, 10.0f,0.001f);
				Matrix34 debugAxis = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
				debugAxis.d.z+=pEntity->GetBoundingBoxMax().z;
				grcDebugDraw::Axis(debugAxis, pEntity->GetBoundRadius()*rotationMarkerBoundRadiusMult, true);

				// if there's been a change in the mouse x, rotate the entities heading
				if (GetMouseDX()!=0)
				{
					TUNE_GROUP_FLOAT( DEBUG_CONTROL, focusEntityRotationSensitivity, 0.01f, 0.0001f, 0.5f,0.001f);
					float newHeading = pEntity->GetTransform().GetHeading() + ( (float)(GetMouseDX())*focusEntityRotationSensitivity );
					float fRotateAngle = ( (float)(GetMouseDX())*focusEntityRotationSensitivity );

					if (newHeading < -PI)
					{
						newHeading = (TWO_PI + newHeading);
					}
					else if (newHeading > PI)
					{
						newHeading = (-TWO_PI + newHeading);
					}
					
					if (pEntity->GetIsTypePed())
					{
						CPed* pPed = static_cast<CPed*>(pEntity);
						if (pPed)
						{
#if __BANK
							if (CAnimPlacementEditor::IsUsingPed(pPed))
							{
								CAnimPlacementEditor::RotateTool(pPed, ( (float)(GetMouseDX())*focusEntityRotationSensitivity ));
							}
							else
#endif // __BANK
							{
								pPed->SetHeading(newHeading);
								pPed->SetDesiredHeading(newHeading);
							}

						}
					}
					else
					{
						//instead of setting its heading just rotate round the z axis
						Matrix34 mMat = MAT34V_TO_MATRIX34(pEntity->GetMatrix()); 
						mMat.RotateLocalZ(fRotateAngle);
						pEntity->SetMatrix(mMat);
					}
				}
			}
		}
	}
	//Rotate the object round its local y axis
	if (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT && ioMapper::DebugKeyDown(KEY_UP))
	{
		CEntity* pEntity = FocusEntities_Get(0);
		if (pEntity)
		{	
			if (!pEntity->GetIsTypePed())
			{
				TUNE_GROUP_FLOAT( DEBUG_CONTROL, focusEntityRotationSensitivity, 0.01f, 0.0001f, 0.5f,0.001f);
				float fRotateAngle = ( (float)(GetMouseDX())*focusEntityRotationSensitivity );
			
			
				TUNE_GROUP_FLOAT(DEBUG_RENDER, rotationMarkerBoundRadiusMult, 1.0f, 0.001f, 10.0f,0.001f);
				Matrix34 debugAxis = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
				debugAxis.d.z+=pEntity->GetBoundingBoxMax().z;
				grcDebugDraw::Axis(debugAxis, pEntity->GetBoundRadius()*rotationMarkerBoundRadiusMult, true);
				
				
				Matrix34 mMat = MAT34V_TO_MATRIX34(pEntity->GetMatrix()); 
				mMat.RotateLocalY(fRotateAngle);
				pEntity->SetMatrix(mMat);
			}
		}
	}
	
	//Rotate the object round its local y axis
	if (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT && ioMapper::DebugKeyDown(KEY_RIGHT))
	{
		CEntity* pEntity = FocusEntities_Get(0);
		if (pEntity)
		{	
			if (!pEntity->GetIsTypePed())
			{
				TUNE_GROUP_FLOAT( DEBUG_CONTROL, focusEntityRotationSensitivity, 0.01f, 0.0001f, 0.5f,0.001f);
				float fRotateAngle = ( (float)(GetMouseDX())*focusEntityRotationSensitivity );

			
				TUNE_GROUP_FLOAT(DEBUG_RENDER, rotationMarkerBoundRadiusMult, 1.0f, 0.001f, 10.0f,0.001f);
				Matrix34 debugAxis = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
				debugAxis.d.z+=pEntity->GetBoundingBoxMax().z;
				grcDebugDraw::Axis(debugAxis, pEntity->GetBoundRadius()*rotationMarkerBoundRadiusMult, true);
				
				Matrix34 mMat = MAT34V_TO_MATRIX34(pEntity->GetMatrix()); 
				mMat.RotateLocalX(fRotateAngle);
				pEntity->SetMatrix(mMat);
			}
		}
	}
	// hold middle mouse and drag vertically to alter entity z
	if (ioMouse::GetButtons() & ioMouse::MOUSE_MIDDLE || ioMapper::DebugKeyDown(KEY_LSHIFT) || ioMapper::DebugKeyDown(KEY_RSHIFT) )
	{
		CEntity* pEntity = FocusEntities_Get(0);
		if (pEntity)
		{	
			//draw a line to represent the movement axis
			const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			grcDebugDraw::Line(vEntityPosition - Vector3(0.0f,0.0f,100.0f), vEntityPosition + Vector3(0.0f,0.0f,100.0f),Color_blue);

			// if there's been a change in the mouse y, change the entity height
			if (GetMouseDY()!=0)
			{
				TUNE_GROUP_FLOAT( DEBUG_CONTROL, focusEntityZSensitivity, 0.02f, 0.0001f, 2.0f,0.001f);
				float deltaZ = -( (float)(GetMouseDY())*focusEntityZSensitivity );

				CPed* pPed = static_cast<CPed*>(pEntity);
				if (pPed)
				{
#if __BANK
					if (CAnimPlacementEditor::IsUsingPed(pPed))
					{
						CAnimPlacementEditor::TranslateTool(pPed,Vector3(0.0f,0.0f,deltaZ));
					}
					else
#endif // __BANK
					{
						pPed->SetPosition(vEntityPosition+Vector3(0.0f,0.0f,deltaZ), true, true, true);
					}
				}
				else
				{
					pEntity->SetPosition(vEntityPosition+Vector3(0.0f,0.0f,deltaZ));
				}
			}
		}
	}

	// Click and drag the mouse to move the entity around
	if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
	{
		CEntity* pEntity = FocusEntities_Get(0);
		int archetypeFlags;
		Vector3 groundPosition(VEC3_ZERO);
		Vector3 groundOffset(VEC3_ZERO);

		if (pEntity)
		{

			archetypeFlags = GetArchetypeFlags(pEntity);

			switch(pEntity->GetType())
			{
			case ENTITY_TYPE_PED:
				{
					groundOffset.z = static_cast<CPed*>(pEntity)->GetCapsuleInfo()->GetGroundToRootOffset();
				}
				break;

			case ENTITY_TYPE_VEHICLE:
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
					groundOffset.z = -pVehicle->GetBoundingBoxMin().z;
				}
				break;
			default:
				{
					//test for the ground under the entities current position and get the z offset
					WorldProbe::CShapeTestProbeDesc probeDesc;
					WorldProbe::CShapeTestFixedResults<> probeResult;
					probeDesc.SetResultsStructure(&probeResult);
					const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
					probeDesc.SetStartAndEnd(vEntityPosition, vEntityPosition - Vector3(0.0f,0.0f,1000.0f));
					probeDesc.SetIncludeFlags(archetypeFlags);
					probeDesc.SetExcludeEntity(pEntity);
					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
					{
						groundOffset = (vEntityPosition - probeResult[0].GetHitPosition());
					}
				}
			}
			
			if(GetWorldPositionUnderMouse(thisFrameWorldPosition,  archetypeFlags))
			{
				Vector2 thisFrameScreenPos, lastFrameScreenPos, adjustedPedScreenPos;

				GetScreenPosFromWorldPoint(thisFrameWorldPosition, thisFrameScreenPos.x, thisFrameScreenPos.y);				//convert our second point to screen
				GetScreenPosFromWorldPoint(ms_lastFrameWorldPosition, lastFrameScreenPos.x, lastFrameScreenPos.y);			//convert our first point to screen

				GetScreenPosFromWorldPoint(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - groundOffset, adjustedPedScreenPos.x, adjustedPedScreenPos.y);	//convert our the ped point on screen, get his feet pos for correct line probe test

				adjustedPedScreenPos = adjustedPedScreenPos + (thisFrameScreenPos - lastFrameScreenPos );										//convert final position in screen space

				if (GetWorldPositionFromScreenPos(adjustedPedScreenPos.x, adjustedPedScreenPos.y, groundPosition, archetypeFlags))
				{
					//the mouse position has moved so update the ped
					if (MouseMovedThisFrame())
					{
						switch(pEntity->GetType())
						{
						case ENTITY_TYPE_PED:
							{
#if __BANK
								CPed* pPed = static_cast<CPed*>(pEntity);

								// check if this is a placement tool ped, if so we shouldn't update it
								if (!CAnimPlacementEditor::IsUsingPed(pPed))
								{
									pPed->Teleport(groundPosition+groundOffset, pPed->GetTransform().GetHeading());
								}
#endif // __BANK
							}
							break;

						case ENTITY_TYPE_VEHICLE:
							{
								CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
								pVehicle->Teleport(groundPosition+groundOffset, pVehicle->GetTransform().GetHeading());
							}
							break;
						default:
							{
								pEntity->SetPosition(groundPosition+groundOffset);
							}
						}
					}
					ms_lastFrameWorldPosition = thisFrameWorldPosition;
				}
			}
		}
	}
#endif // __BANK
}

int	CDebugScene::GetArchetypeFlags(CEntity* pEntity)
{
	switch(pEntity->GetType())
	{
	case ENTITY_TYPE_VEHICLE:
		{
			return (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE);
		}
	default:
		{
			return (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
		}
	}
}

//
// name:		CDebugScene::GetWorldPositionUnderMouse
// description:	Return the world position under the mouse pointer
bool CDebugScene::GetWorldPositionUnderMouse(Vector3& posn, s32 iOptionalFlags, Vector3* pvNormal, void **entity, bool onVectorMap )
{
	Vector3 mouseScreen, mouseFar;
	GetMousePointing(mouseScreen, mouseFar, onVectorMap);

	Vector3	StartPos;

	if(iOptionalFlags == 0)
	{
		iOptionalFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
	}

	// check for nans in position passed in.
	if(mouseScreen==mouseScreen && mouseFar==mouseFar)
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<> probeResult;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(mouseScreen, mouseFar);
		probeDesc.SetIncludeFlags(iOptionalFlags);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			posn = probeResult[0].GetHitPosition();

			phInst* pInst = probeResult[0].GetHitInst();
			if( pInst )
			{
				CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
				if( pEntity && entity)
					*entity = pEntity;
			}
			
			if( pvNormal )
			{
				*pvNormal = probeResult[0].GetHitNormal();
			}
			return true;
		}
	}
	return false;
}

//
// name:		CDebugScene::CheckMouse
// description:	any debug only functions that use the mouse, ie teleport to mouse, select ai entity etc
void CDebugScene::CheckMouse()
{
	if(fwTimer::GetSystemFrameCount() < 120)
	{
		// JB : Wait for ther game & viewports to be set up before processing this, or else
		// there is an assert about invalid start & end points for a line-of-sight.
		return;
	}

	// AI bank must be active for clicking on players to do anything
#if __BANK
	bool keyLControlDown=CControlMgr::GetKeyboard().GetKeyDown(KEY_LCONTROL, KEYBOARD_MODE_DEBUG, "Multiple select on");
	bool keyRControlDown=CControlMgr::GetKeyboard().GetKeyDown(KEY_RCONTROL, KEYBOARD_MODE_DEBUG, "Multiple select on");
	bool keyGDown=CControlMgr::GetKeyboard().GetKeyDown(KEY_G, KEYBOARD_MODE_DEBUG, "Teleport to Mouse");
	bool keyGReleased=CControlMgr::GetKeyboard().GetKeyJustUp(KEY_G, KEYBOARD_MODE_DEBUG);
	bool keyNDown=CControlMgr::GetKeyboard().GetKeyDown(KEY_N, KEYBOARD_MODE_DEBUG, "Focus-ped navmesh to Mouse");
	bool keyNReleased=CControlMgr::GetKeyboard().GetKeyJustUp(KEY_N, KEYBOARD_MODE_DEBUG);
	bool keyTDown=CControlMgr::GetKeyboard().GetKeyDown(KEY_T, KEYBOARD_MODE_DEBUG, "Give AI task to focus-ped");
	bool keyTReleased=CControlMgr::GetKeyboard().GetKeyJustUp(KEY_T, KEYBOARD_MODE_DEBUG);

	bool checkMouseSelectEntity = ms_changeSelectedEntityOnHover || GetMouseLeftPressed();

	if (keyGDown||keyGReleased||keyNDown||keyNReleased||keyTDown||keyTReleased||checkMouseSelectEntity)
	{
		if( checkMouseSelectEntity && !ms_lockFocusEntities )
		{
#if !__FINAL
			s32 physicsTypeFlags = 0;
			if(ms_clickTestForPeds)				{ physicsTypeFlags |= ArchetypeFlags::GTA_PED_TYPE;}
			if(ms_clickTestForRagdolls)			{ physicsTypeFlags |= ArchetypeFlags::GTA_RAGDOLL_TYPE;}
			// Dummy peds are special as they don't have any physics...
			if(ms_clickTestForVehicles)			{ physicsTypeFlags |= (ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);}
			if(ms_clickTestForObjects)			{ physicsTypeFlags |= ArchetypeFlags::GTA_OBJECT_TYPE;}
			if(ms_clickTestForWeapons)			{ physicsTypeFlags |= ArchetypeFlags::GTA_PROJECTILE_TYPE;}
			if(ms_clickTestForBuildingsAndMap)	{ physicsTypeFlags |= (ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_PLANT_TYPE);}
	
			CEntity* pEntity = GetEntityUnderMouse(NULL, NULL, physicsTypeFlags);

			const bool doingControlClick = (keyLControlDown || keyRControlDown);
			
			if(pEntity)
			{
				if(!doingControlClick)
				{
					CDebugScene::FocusEntities_Clear();
					CDebugScene::FocusEntities_Set(pEntity, 0);
					if(!CDebugScene::GetWorldPositionUnderMouse(ms_lastFrameWorldPosition, GetArchetypeFlags(pEntity)))
					{
						return ;
					}
				}
				else
				{
					// Try to add another focus entity if not already in group.
					int index = -1;
					CDebugScene::FocusEntities_Add(pEntity, index);
				}
			}
			else
			{
				if(!doingControlClick)
				{
					// Clear the selection when clicking on nothing.
					CDebugScene::FocusEntities_Clear();
				}
			}
#endif
		}
		else
		{
			Vector3	Pos;
			if(GetWorldPositionUnderMouse(Pos))
			{
				grcDebugDraw::Sphere(Pos, 0.3f, Color32(0xff, 0x00, 0x00, 0xff));

				if(keyGReleased) //released G so teleport
				{
					CPlayerInfo*	pPlayerInfo = CGameWorld::GetMainPlayerInfo();
					// Vector3		PlayerPos=pPlayerInfo->GetPos();
#if !__FINAL
					Pos.z += 1.0f;
					pPlayerInfo->GetPlayerPed()->Teleport(Pos, pPlayerInfo->GetPlayerPed()->GetCurrentHeading());
#endif
				}
				if(keyNReleased) //if we've got focus-ped, then goto pos
				{
					CEntity* pFocusEntity = FocusEntities_Get(0);
					if(pFocusEntity && pFocusEntity->GetIsTypePed())
					{
						CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
 						CTaskParachute* pParachuteTask = static_cast<CTaskParachute*>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
 						if( pParachuteTask )
 						{
 							pParachuteTask->SetTargetPosition(VECTOR3_TO_VEC3V(Pos));
 							pParachuteTask->GetParachuteFlags().SetFlag(CTaskParachute::PF_SkipSkydiving);
 						}

						CPedDebugVisualiserMenu::MakePedFollowNavMeshToCoord(pFocusPed, Pos);
					}
				}
				if(keyTReleased) //if we've got focus-ped, then give AI task from debug visualiser
				{
					CEntity* pFocusEntity = FocusEntities_Get(0);
					if(pFocusEntity && pFocusEntity->GetIsTypePed())
					{
						CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
						CTaskComplexEvasiveStep * pTask = rage_new CTaskComplexEvasiveStep(Pos);
						pFocusPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, pTask, PED_TASK_PRIORITY_PRIMARY);
					}
				}
			}
		}
	}

	CPhysics::UpdateHistoryRendering();

	if( CPhysics::ms_bDebugPullThingsAround )
	{
		CPhysics::UpdatePullingThingsAround();
	}
	else if ( CPhysics::ms_mouseInput && phMouseInput::IsEnabled() )
	{
		CPhysics::ms_mouseInput->Update(fwTimer::IsGamePaused(), ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), &gVpMan.GetGameViewport()->GetGrcViewport());
	}
	else if( CPhysics::ms_bMouseShooter || audPedFootStepAudio::sm_bMouseFootstep || audPedFootStepAudio::sm_bMouseFootstepBullet)
	{
		CPhysics::UpdateMouseShooter();
	}
	else if( CPhysics::ms_bDebugMeasuringTool )
	{
		CPhysics::UpdateDebugMeasuringTool();
	}
	else if(CPedDebugVisualiserMenu::IsDebugPedDraggingActivated())
	{
		CPedDebugVisualiserMenu::UpdatePedDragging();
	}
	else if(CPedDebugVisualiserMenu::IsDebugDefensiveAreaDraggingActivated())
	{
		CPedDebugVisualiserMenu::UpdateDefensiveAreaDragging();
	}
	else if(CPedDebugVisualiserMenu::IsNMInteractiveShooterActivated())
	{
		CPedDebugVisualiserMenu::UpdateNMInteractiveShooter();
	}
	else if(CPedDebugVisualiserMenu::IsNMInteractiveGrabberActivated())
	{
		CPedDebugVisualiserMenu::UpdateNMInteractiveGrabber();
	}
#if SCENARIO_DEBUG
	else if (CScenarioDebug::ms_Editor.IsEditing())
	{
		CScenarioDebug::ms_Editor.Update();
	}
#endif // SCENARIO_DEBUG
	else if(audCollisionAudioEntity::sm_EnableModelEditor)
	{
		audCollisionAudioEntity::UpdateModelEditorSelector();
	}
	else
	{
		FRAGTUNE->Update(&gVpMan.GetGameViewport()->GetGrcViewport());
	}
#endif  //__BANK
}

#if __DEV

const Vector3 modelViewerPosn(0.0f, 0.0f, 1000.0f);
const Vector3 modelViewerCamPosn(0.0f, -10.0f, 1001.0f);
const Vector3 modelViewerCamDir(0.0f, 0.0f, 0.0f);
static bool gEnableModelViewer = false;
static RegdEnt gpModelViewerEntity(NULL);
static s32 gModelViewerIndex = 0;
static char gModelViewerName[STORE_NAME_LENGTH];
static char gNumMeshesName[64];
static char gNumPolysName[64];
static s32 gNumMeshes = 0;
static s32 gNumPolys = 0;
static s32 gMeshIndex = -1;

static void SwitchModelViewerOnOff()
{
	if(gEnableModelViewer)
		CDebugScene::InitModelViewer();
	else
		CDebugScene::ShutdownModelViewer(SHUTDOWN_WITH_MAP_LOADED);
}

//
// name:		CountGeometriesInDrawable
// description:	Count geometies and polys in drawable
static s32 CountGeometriesInDrawable(rmcDrawable* pDrawable, s32& polyCount)
{
	s32 count=0;
	if(pDrawable == NULL)
		return 0;
	rmcLod* pLod = &pDrawable->GetLodGroup().GetLod(LOD_HIGH);
	if(pLod == NULL)
		return 0;
	for(s32 i=0; i<pLod->GetCount(); i++)
	{
		count += pLod->GetModel(i)->GetGeometryCount();
		for(s32 j=0; j<pLod->GetModel(i)->GetGeometryCount(); j++)
			polyCount += pLod->GetModel(i)->GetGeometry(j).GetPrimitiveCount();
	}
	return count;
}

static s32 CountGeometriesInFragInst(fragInst* pFrag, s32& polyCount)
{
	s32 count = 0;
	fragTypeChild** pChildren = pFrag->GetTypePhysics()->GetAllChildren();

	for(s32 i=0; i<pFrag->GetTypePhysics()->GetNumChildren(); i++)
	{
		fragDrawable* pDrawable = pChildren[i]->GetUndamagedEntity();
		if(pDrawable)
			count += CountGeometriesInDrawable(pDrawable, polyCount);
	}
	return count;
}

static s32 CountGeometriesInEntity(CEntity* pEntity, s32& polyCount)
{
	//peds do something funny they say they are fragments but their physics inst is just a normal inst
	if(pEntity->GetIsTypePed())
		return CountGeometriesInDrawable(pEntity->GetDrawable(), polyCount);
	else if(pEntity->GetFragInst())
		return CountGeometriesInFragInst(pEntity->GetFragInst(), polyCount);
	else
		return CountGeometriesInDrawable(pEntity->GetDrawable(), polyCount);
}

//
// name:		DrawGeometryInDrawable
// description:	Draw a single geometry from a drawable
static void DrawGeometryInDrawable(rmcDrawable* pDrawable, const Matrix34& rootMatrix, const Matrix34* pMatrix, s32 num)
{
	if(gNumMeshes == 0 || num >= gNumMeshes)
		return;

	Assert(pDrawable);
	if(pDrawable == NULL)
		return;
	rmcLod* pLod = &pDrawable->GetLodGroup().GetLod(LOD_HIGH);
	if(pLod == NULL)
		return;

	for(s32 i=0; i<pLod->GetCount(); i++)
	{
		grmModel* pModel = pLod->GetModel(i);
		s32 idx = pModel->GetMatrixIndex();
		Matrix34 worldMatrix;
		if(pMatrix)
		{
			worldMatrix.Dot(pMatrix[idx], rootMatrix);
		}
		else
		{
			worldMatrix = rootMatrix;
		}
		grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMatrix));
		if(num < pModel->GetGeometryCount())
		{
			grmShader& shader = pDrawable->GetShaderGroup().GetShader(pModel->GetShaderIndex(num));
			shader.Draw(*pModel, num, 0, true);
			break;
		}
		num -= pModel->GetGeometryCount();
	}
	grcStateBlock::Default();
}

static void DrawGeometryInFragInst(fragInst* pFragInst, const Matrix34& rootMatrix, Matrix34* pMatrices, s32 num)
{
	if(gNumMeshes == 0 || num >= gNumMeshes)
		return;
	Assert(pFragInst);
	fragTypeChild** pChildren = pFragInst->GetTypePhysics()->GetAllChildren();

	for(s32 i=0; i<pFragInst->GetTypePhysics()->GetNumChildren(); i++)
	{
		fragDrawable* pDrawable = pChildren[i]->GetUndamagedEntity();
		s32 polyCount=0;
		s32 count = CountGeometriesInDrawable(pDrawable, polyCount);
		if(num < count)
		{
			DrawGeometryInDrawable(pDrawable, rootMatrix, pMatrices, num);
			break;
		}
		num -= count;
	}
	
}

static void DrawGeometryInEntity(CEntity* pEntity, s32 num)
{
	//peds do something funny they say they are fragments but their physics inst is just a normal inst
	if(pEntity->GetIsTypePed())
	{
		DrawGeometryInDrawable(pEntity->GetDrawable(), MAT34V_TO_MATRIX34(pEntity->GetMatrix()), reinterpret_cast<const Matrix34*>(((CDynamicEntity*)pEntity)->GetSkeleton()->GetObjectMtxs()), num);
	}
	else if(pEntity->GetFragInst())
	{
		DrawGeometryInFragInst(pEntity->GetFragInst(), MAT34V_TO_MATRIX34(pEntity->GetMatrix()), reinterpret_cast<Matrix34*>(((CDynamicEntity*)pEntity)->GetSkeleton()->GetObjectMtxs()), num);
	}else
		DrawGeometryInDrawable(pEntity->GetDrawable(), MAT34V_TO_MATRIX34(pEntity->GetMatrix()), NULL, num);
}

//
// name:		FindGeometryInDrawable
// description:	Find a single geometry from a drawable
static grmGeometry* FindGeometryInDrawable(rmcDrawable* pDrawable, s32 num)
{
	if(gNumMeshes == 0 || num >= gNumMeshes)
		return NULL;

	Assert(pDrawable);
	rmcLod* pLod = &pDrawable->GetLodGroup().GetLod(LOD_HIGH);
	if(pLod == NULL)
		return NULL;

	for(s32 i=0; i<pLod->GetCount(); i++)
	{
		grmModel* pModel = pLod->GetModel(i);
		if(num < pModel->GetGeometryCount())
		{
			return &pModel->GetGeometry(num);
		}
		num -= pModel->GetGeometryCount();
	}
	return NULL;
}

static grmGeometry* FindGeometryInFragInst(fragInst* pFragInst, s32 num)
{
	if(gNumMeshes == 0 || num >= gNumMeshes)
		return NULL;
	Assert(pFragInst);
	fragTypeChild** pChildren = pFragInst->GetTypePhysics()->GetAllChildren();

	for(s32 i=0; i<pFragInst->GetTypePhysics()->GetNumChildren(); i++)
	{
		fragDrawable* pDrawable = pChildren[i]->GetUndamagedEntity();
		s32 polyCount=0;
		s32 count = CountGeometriesInDrawable(pDrawable, polyCount);
		if(num < count)
		{
			return FindGeometryInDrawable(pDrawable, num);
		}
		num -= count;
	}
	return NULL;
}

static grmGeometry* FindGeometryInEntity(CEntity* pEntity, s32 num)
{
	//peds do something funny they say they are fragments but their physics inst is just a normal inst
	if(pEntity->GetIsTypePed())
	{
		return FindGeometryInDrawable(pEntity->GetDrawable(), num);
	}
	else if(pEntity->GetFragInst())
	{
		return FindGeometryInFragInst(pEntity->GetFragInst(), num);
	}
	else
		return FindGeometryInDrawable(pEntity->GetDrawable(), num);
}

//
// name:		GetModelIndexFromName/GetModelNameFromIndex
// description:	Convert model index to name and vice versa
static void GetModelIndexFromName()
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromName(gModelViewerName, &modelId);
	if(modelId.IsValid())
		gModelViewerIndex = modelId.GetModelIndex();
}

static void GetModelNameFromIndex()
{
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(gModelViewerIndex)));
	if(pModelInfo)
		strcpy(gModelViewerName, pModelInfo->GetModelName());
}


//
// name:		CDebugScene::AddModelViewerBankWidgets
// description:	
void CDebugScene::AddModelViewerBankWidgets(bkBank& bank)
{
	bank.PushGroup("Model Viewer", false);
	bank.AddToggle("On/Off", &gEnableModelViewer, &SwitchModelViewerOnOff);
	bank.AddText("Modelname", &gModelViewerName[0], sizeof(gModelViewerName), false, &GetModelIndexFromName);
	bank.AddSlider("Modelindex", &gModelViewerIndex, 0, CModelInfo::GetMaxModelInfos(), 1, &GetModelNameFromIndex);
	bank.AddText("Num meshes", &gNumMeshesName[0], sizeof(gNumMeshesName), true);
	bank.AddText("Num primitives", &gNumPolysName[0], sizeof(gNumPolysName), true);
	bank.AddSlider("Mesh render", &gMeshIndex, -1, 1000, 1);
	bank.PopGroup();
}

//
// name:		CDebugScene::InitModelViewer
// description:	Initialise model viewer
void CDebugScene::InitModelViewer()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	debugDirector.ActivateFreeCam();	//Turn on debug free cam.

	//Move free camera to desired place.
	camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
	freeCamFrame.SetPosition(modelViewerCamPosn);

	Matrix34 worldMatrix;
	CScriptEulers::MatrixFromEulers(worldMatrix, modelViewerCamDir * DtoR, EULER_YXZ);

	freeCamFrame.SetWorldMatrix(worldMatrix);

	CControlMgr::SetDebugPadOn(true);
}

CEntity* CreateModelViewerEntity(strLocalIndex modelIndex)
{
	const CControlledByInfo localAiControl(false, false);
	CEntity* pEntity;
	fwModelId modelId(modelIndex);
	switch(CModelInfo::GetBaseModelInfo(modelId)->GetModelType())
	{
	case MI_TYPE_VEHICLE:
		pEntity = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_TOOL, NULL);
		break;
	case MI_TYPE_PED:
		pEntity = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, NULL, true, true, false);
		((CPed*)pEntity)->PopTypeSet(POPTYPE_TOOL);
		((CPed*)pEntity)->SetDefaultDecisionMaker();
		((CPed*)pEntity)->SetCharParamsBasedOnManagerType();
		((CPed*)pEntity)->GetPedIntelligence()->SetDefaultRelationshipGroup();
		break;
	default:
		pEntity = rage_new CBuilding( ENTITY_OWNEDBY_DEBUG );

		pEntity->SetModelId(modelId);

		pEntity->InitPhys();
		pEntity->CreateDrawable();
		if(pEntity->GetIsDynamic())
			((CDynamicEntity*)pEntity)->CreateSkeleton();
		break;
	}
	return pEntity;
}

void DeleteModelViewerEntity(CEntity* pEntity)
{
	switch(pEntity->GetBaseModelInfo()->GetModelType())
	{
	case MI_TYPE_VEHICLE:
		CVehicleFactory::GetFactory()->Destroy((CVehicle*)pEntity);
		break;
	case MI_TYPE_PED:
		CPedFactory::GetFactory()->DestroyPed((CPed*)pEntity);
		break;
	default:
		delete pEntity;
		break;
	}
}

//
// name:		CDebugScene::ShutdownModelViewer
// description:	Shutdown model viewer
void CDebugScene::ShutdownModelViewer(unsigned /*shutdownMode*/)
{
	if(gpModelViewerEntity)
		DeleteModelViewerEntity(gpModelViewerEntity);
}

void CDebugScene::UpdateModelViewer()
{
	IF_COMMERCE_STORE_RETURN();

	if(gEnableModelViewer == false)
		return;

	if(gpModelViewerEntity)
	{
		// update number of polys
		if(gMeshIndex == -1)
		{
			gNumPolys=0;
			gNumMeshes = CountGeometriesInEntity(gpModelViewerEntity, gNumPolys);
			sprintf(gNumPolysName, "%d", gNumPolys);
		}
		else
		{
			grmGeometry* pGeom = FindGeometryInEntity(gpModelViewerEntity, gMeshIndex);
			if(pGeom)
				sprintf(gNumPolysName, "%d", pGeom->GetPrimitiveCount());
		}
		// Check if index has changed
		if(gpModelViewerEntity->GetModelIndex() != (u32)gModelViewerIndex)
		{
			DeleteModelViewerEntity(gpModelViewerEntity);
			gpModelViewerEntity = NULL;
		}
	}
	if(!gpModelViewerEntity)
	{
		fwModelId modelId((strLocalIndex(gModelViewerIndex)));
		if(modelId.IsValid())
		{
			// if object is in memory. Create version of object
			if(CModelInfo::HaveAssetsLoaded(modelId))
			{
				gpModelViewerEntity = CreateModelViewerEntity(strLocalIndex(gModelViewerIndex));
				gpModelViewerEntity->SetPosition(modelViewerPosn);

				gNumPolys=0;
				gNumMeshes = CountGeometriesInEntity(gpModelViewerEntity, gNumPolys);
				sprintf(gNumMeshesName, "%d", gNumMeshes);
				sprintf(gNumPolysName, "%d", gNumPolys);
				gMeshIndex = -1;
			}
			// request object be streamed
			else
				CModelInfo::RequestAssets(modelId, 0);
		}
	}
}

//
// name:		CDebugScene::RenderModelViewer
// description:	Render model viewer

void CDebugScene::RenderModelViewer()
{
	IF_COMMERCE_STORE_RETURN();

	if(gEnableModelViewer == false)
		return;

	if(gpModelViewerEntity)
	{
		gpModelViewerEntity->PreRender();
		if(gMeshIndex < 0)
		{
			// can't call render buckets through an entity any more
			//gpModelViewerEntity->RenderBuckets(CRenderer::RB_OPAQUE, rmStandard);
			//gpModelViewerEntity->RenderBuckets(CRenderer::RB_ALPHA, rmStandard);
			//gpModelViewerEntity->RenderBuckets(CRenderer::RB_DECAL, rmStandard);
			//gpModelViewerEntity->RenderBuckets(CRenderer::RB_CUTOUT, rmStandard);

			// don't know if this works...
			DrawGeometryInEntity(gpModelViewerEntity, 0);
		}
		else
			DrawGeometryInEntity(gpModelViewerEntity, gMeshIndex);
	}
}

#endif
#if __BANK
//
// name:		CDebugScene::DisplayEntitiesOnVMap
// description:	Displays peds,cars and mission objects on the vector map

void CDebugScene::DisplayEntitiesOnVectorMap()
{
	IF_COMMERCE_STORE_RETURN();

	char debugText[100];
	
	//--------------------------------------------------------------------------
	// PROXIMITY AREA
	if(ms_drawProximityAreaOnVM)
	{
		// Determine if we should draw this frame or not.
		bool drawThisFrame = true;
		if(ms_drawProximityAreaOnVMFlashes)
		{
			static bool frameFlasher = true;
			frameFlasher = !frameFlasher;
			drawThisFrame = frameFlasher;
		}
		if(drawThisFrame)
		{
			CVectorMap::DrawCircle(ms_proximityTestPoint, ms_proximityMaxAllowableRadialDiff, Color32(255, 100, 100, 255), false);
		}
	}

	//--------------------------------------------------------------------------
	// WATER
	if (bDisplayWaterOnVMap)
	{
		Water::RenderDebug(Color32(100,100,255,255), Color32(0), Color32(255,255,0,255), false, true, true);
	}
	if (bDisplayCalmingWaterOnVMap)
	{
		Water::RenderDebug(Color32(0), Color32(0,0,255,255), Color32(0), false, true, true);
	}
	if(bDisplayShoreLinesOnVMap)
	{
		g_AmbientAudioEntity.RenderShoreLinesDebug();
	}

	//--------------------------------------------------------------------------
	// BUILDINGS
	if (bDisplayBuildingsOnVMap)
	{
		// Go over all the buildings.
		CBuilding::Pool *BuildingPool = CBuilding::GetPool();
		CBuilding* pBuilding;
		s32 i=BuildingPool->GetSize();
		while(i--)
		{
			pBuilding = BuildingPool->GetSlot(i);
			if(pBuilding)
			{
				// Determine what colour to draw the building.
				Color32	Colour(255, 255, 255, 255);

				// Draw the building.
				CDebugScene::DrawEntityBoundingBoxOnVMap(pBuilding, Colour);
			}
		}

		// Go over all the animated buildings.
		CAnimatedBuilding::Pool *AnimatedBuildingPool = CAnimatedBuilding::GetPool();
		CAnimatedBuilding* pAnimatedBuilding;
		i=AnimatedBuildingPool->GetSize();
		while(i--)
		{
			pAnimatedBuilding = AnimatedBuildingPool->GetSlot(i);
			if(pAnimatedBuilding)
			{
				// Determine what colour to draw the animated building.
				Color32	Colour(0, 100, 255, 255);

				// Draw the building.
				CDebugScene::DrawEntityBoundingBoxOnVMap(pAnimatedBuilding, Colour);
			}
		}
	}

	
	//--------------------------------------------------------------------------
	// VEHICLES
	if (bDisplayVehiclesOnVMap || bDisplayVehiclesOnVMapAsActiveInactive || bDisplayVehiclesOnVMapTimesliceUpdates || bDisplayVehiclesOnVMapBasedOnOcclusion || bDisplayVehiclesToBeStreamedOutOnVMap)
	{
		CVehicle::Pool *VehPool = CVehicle::GetPool();
		CVehicle* pVeh;
		s32 i = (s32) VehPool->GetSize();

		while(i--)
		{
			pVeh = VehPool->GetSlot(i);

			if(pVeh)
			{
				Color32	Colour;
				Color32 ActiveColour = Color_green;
				if( (pVeh->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsActive(pVeh->GetCurrentPhysicsInst()->GetLevelIndex())))
				{
					if( pVeh->GetVehicleAiLod().GetDummyMode() == VDM_REAL )
					{
						ActiveColour = Color_red;
					}
					else
					{
						ActiveColour = Color_orange;
					}
				}

				switch(pVeh->GetVehicleAiLod().GetDummyMode())
				{
					case VDM_REAL:
					default:
						Colour = Color_purple;
						break;
					case VDM_DUMMY:
						Colour = Color_orange;
						break;
					case VDM_SUPERDUMMY:
						Colour = Color_green;
						break;
				}

				if(pVeh->GetIsInReusePool())
				{
					if(!CVehiclePopulation::ms_bDebugVehicleReusePool && !CVehiclePopulation::ms_bDisplayVehicleReusePool) // don't show vehicles in reuse pool on vector map
					{
						continue; //skip this vehicle
					}
					else
					{
						// vehicles in the reuse pool are blue
						Colour = Color_blue;
					}
				}

				if (!pVeh->PopTypeIsMission())
				{			
					// Mission vehicles are twice as bright as regular vehicles
					Colour.SetRed( Colour.GetRed()/2 );
					Colour.SetGreen( Colour.GetGreen()/2 );
					Colour.SetBlue( Colour.GetBlue()/2 );
				}

				bool bEmptyPopCar = false;
				if( CNetwork::IsGameInProgress() && !pVeh->IsUsingPretendOccupants() && 
					((NetworkInterface::GetSyncedTimeInMilliseconds() - pVeh->m_LastTimeWeHadADriver) > 5000 ) && (!pVeh->GetLastDriver() || !pVeh->GetLastDriver()->IsAPlayerPed() ) )
				{
					bEmptyPopCar = true;
				}

				const bool bWrecked = pVeh->GetStatus() == STATUS_WRECKED || pVeh->m_nVehicleFlags.bIsDrowning;

				const bool bEmptyOrWrecked = !pVeh->PopTypeIsMission() && (pVeh->PopTypeGet() != POPTYPE_RANDOM_PARKED || bWrecked ) && (pVeh->IsLawEnforcementVehicle() || bEmptyPopCar || bWrecked || pVeh->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs) &&
					(!pVeh->HasAlivePedsInIt() || pVeh->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt) && !pVeh->HasMissionCharsInIt();

				if( bDisplayLinesToLocalDrivingCars )
				{
					if( !pVeh->IsNetworkClone() && ((pVeh->PopTypeGet() == POPTYPE_RANDOM_AMBIENT) || (pVeh->PopTypeGet() == POPTYPE_RANDOM_SCENARIO) ) && !bEmptyOrWrecked )
					{
						CVectorMap::DrawLine(CGameWorld::FindLocalPlayerCoors(), VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), Color_red, Color_red);
					}
				} 

				if (bDisplayVehiclesToBeStreamedOutOnVMap)
				{
					Colour.Setf(0.4f, 0.4f, 0.4f, 1.0f);
					if (CModelInfo::GetAssetsAreDeletable(pVeh->GetModelId()))
					{
						float Mult = 0.6f + 0.4f * ((fwTimer::GetTimeInMilliseconds() & 512) / 512.0f);
						Colour.Setf( Colour.GetRedf() * Mult, Colour.GetGreenf() * Mult, Colour.GetBluef() * Mult, 1.0f );
					}
				}

				if (NetworkInterface::IsGameInProgress())
				{
					if(bDisplayNetworkGameOnVMap)
					{
						if(pVeh->GetNetworkObject() && pVeh->GetNetworkObject()->GetPlayerOwner())
						{
							PhysicalPlayerIndex playerIndex = pVeh->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
							NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(playerIndex);
							Colour = NetworkColours::GetNetworkColour(colour);
						}
					}
					else
					{
						if( !bEmptyOrWrecked )
						{
							Colour = pVeh->IsNetworkClone() ? Color_blue : Color_green;
						}
						else
						{
							Colour = pVeh->IsNetworkClone() ? Color_purple : Color_red;
						}
					}
				}

				// Fade as we get closer to being removed.
				if(bDisplayVehiclesUsesFadeOnVMap)
				{
					float portionRemaining = 1.0f - pVeh->DelayedRemovalTimeGetPortion();
					Colour.SetRed(255-(int)(portionRemaining*255));
					Colour.SetGreen((int)(portionRemaining*255));
					Colour.SetBlue((int)(portionRemaining*255));
				}

                if(!bDisplayCarCreation DEV_ONLY(|| (pVeh->GetNetworkObject() && (fwTimer::GetSystemTimeInMilliseconds() - ((CNetObjVehicle *)pVeh->GetNetworkObject())->m_timeCreated) <= 1000)))
                {
					if( bDisplayVehiclesOnVMap )
					{
						CVectorMap::DrawVehicle(pVeh, Colour);
					}
					if( bDisplayVehiclesOnVMapAsActiveInactive )
					{
						float fScale = CVectorMap::m_vehicleVectorMapScale;
						CVectorMap::m_vehicleVectorMapScale = fScale * ((bDisplayVehiclesOnVMap) ? 1.3f:1.0f);
						CVectorMap::DrawVehicle(pVeh, ActiveColour);
						CVectorMap::m_vehicleVectorMapScale = fScale;
					}
					if( bDisplayVehiclesOnVMapBasedOnOcclusion )
					{
						Color32 occlusionColour = Color_green;
						if( !pVeh->GetIsVisibleInSomeViewportThisFrame() )
						{
							occlusionColour = Color_orange;
						}
						else
						{
							spdAABB tempBox;
							const spdAABB & aabb = pVeh->GetBoundBox(tempBox);
							bool bVehicleIsOccluded = !COcclusion::IsBoxVisibleFast(aabb);
							if( bVehicleIsOccluded )
							{
								occlusionColour = Color_red;
							}
						}
					}

					if( bDisplayVehiclesOnVMapTimesliceUpdates )
					{
						if( CVehicleAILodManager::ShouldDoFullUpdate(*pVeh) )
						{
							CVectorMap::DrawVehicle(pVeh, Color_green);
						}
					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------
	// PEDS
	if (bDisplayPedsOnVMap || bDisplayPedsOnVMapAsActiveInactive || bDisplayPedsOnVMapTimesliceUpdates || bDisplayPedsToBeStreamedOutOnVMap)
	{
		if (bDisplayPedsOnVMap || bDisplayPedsOnVMapAsActiveInactive || bDisplayPedsOnVMapTimesliceUpdates)
		{
			// Go over all the peds.
			CPed::Pool *PedPool = CPed::GetPool();
			CPed* pPed;
			s32 i=PedPool->GetSize();
			while(i--)
			{
				pPed = PedPool->GetSlot(i);
				if(pPed)
				{
					if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
					{
						if(!CPedPopulation::ms_bDebugPedReusePool && !CPedPopulation::ms_bDisplayPedReusePool)
							continue; // skip peds in the reuse pool
					}

					// Determine what colour to draw the ped.
					Color32	Colour(255, 255, 255, 255);
					{
						if (!pPed->PopTypeIsMission())
						{			// Mission chars are twice as bright as regular peds
							Colour.SetRed( Colour.GetRed()/2 );
							Colour.SetGreen( Colour.GetGreen()/2 );
							Colour.SetBlue( Colour.GetBlue()/2 );
						}


						if (pPed->IsPlayer())
						{
							Colour = Color32(255, 255, 255, 255);
							if (fwTimer::GetTimeInMilliseconds() & 512)
							{
								Colour = Color32(0, 0, 0, 255);
							}
						}

						if (pPed->PopTypeGet() == POPTYPE_RANDOM_SCENARIO)
						{
							Colour = Color32(0, 130, 0, 255);
						}

						if (NetworkInterface::IsGameInProgress() && bDisplayNetworkGameOnVMap)
						{
							if(pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner())
							{
								PhysicalPlayerIndex playerIndex = pPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
								NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(playerIndex);
								Colour = NetworkColours::GetNetworkColour(colour);
							}
						}

						// Fade as we get closer to being removed.
						float portionRemaining = 1.0f - pPed->DelayedRemovalTimeGetPortion();
						Colour.SetAlpha( (int) (255 * portionRemaining) );

						if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
						{
							// draw reused peds as purple
							Colour = Color_purple;

							// fade them like non mission peds
							Colour.SetRed( Colour.GetRed()/2 );
							Colour.SetGreen( Colour.GetGreen()/2 );
							Colour.SetBlue( Colour.GetBlue()/2 );
						}
					}

					// Draw the ped.
					if( bDisplayPedsOnVMap )
					{
						CVectorMap::DrawPed(pPed, Colour);
					}

					if( bDisplayPedsOnVMapAsActiveInactive )
					{
						if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
							continue; // skip peds in the reuse pool. they're not in the physics level anyway

						Color32 ActiveColour = pPed->GetVehiclePedInside() ? Color_blue: Color_green;
						if( (pPed->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsActive(pPed->GetCurrentPhysicsInst()->GetLevelIndex())))
						{
							// Draw an extra circle as they can be odd orientaitons
							if( pPed->GetRagdollState()==RAGDOLL_STATE_PHYS )
							{
								ActiveColour = Color_purple;
								CVectorMap::DrawCircle(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 2.0f, ActiveColour);
							}
							else
							{
								ActiveColour = Color_red;
							}
						}
						float fScale = CVectorMap::m_pedVectorMapScale;
						CVectorMap::m_pedVectorMapScale = fScale * 1.3f;
						CVectorMap::DrawPed(pPed, ActiveColour);
						CVectorMap::m_pedVectorMapScale = fScale;
					}

					if( bDisplayPedsOnVMapTimesliceUpdates )
					{
						if(CPedAILodManager::ShouldDoFullUpdate(*pPed))
						{
							CVectorMap::DrawPed(pPed, Color_green);
						}
					}

					// If this ped is in a vehicle we draw it too.
					if (bDisplayPedsOnVMap && (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle()))
					{
						CVectorMap::DrawVehicle(pPed->GetMyVehicle(), Color32(100, 100, 100, 255) );
					}
				}
			}
		}

		if (bDisplayPedsToBeStreamedOutOnVMap)
		{
			CPed::Pool *PedPool = CPed::GetPool();
			CPed* pPed;
			s32 i=PedPool->GetSize();

			while(i--)
			{
				pPed = PedPool->GetSlot(i);

				if(pPed)
				{
					Color32	Colour(255, 255, 255, 255);

					if (pPed->RemovalIsPriority(pPed->GetModelId()) && (fwTimer::GetTimeInMilliseconds() & 512))
					{
						Colour.SetRed( 0 );
						Colour.SetGreen( 0 );
						Colour.SetBlue( 0 );
					}


					CVectorMap::DrawPed(pPed, Colour);

					// if this ped is in a vehicle we draw this too
					if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
					{
						CVectorMap::DrawVehicle(pPed->GetMyVehicle(), Color32(100, 100, 100, 255) );

					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------
	// Objects
	if (bDisplayObjectsOnVMap || bDisplayLineAboveObjects)
	{
		if (bDisplayObjectsOnVMap)
		{
			CObject::Pool *ObjPool = CObject::GetPool();
			CObject* pObj;
			s32 i = (s32) ObjPool->GetSize();
			s32 numObjects = 0;

			while(i--)
			{
				pObj = ObjPool->GetSlot(i);

				if(pObj)
				{
					numObjects++;
					Color32	Colour = (pObj->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsActive(pObj->GetCurrentPhysicsInst()->GetLevelIndex())) ? Color_red : Color_green;
					if (pObj->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
					{			
						// Mission objects are twice as bright as regular objects
						Colour.SetRed( Colour.GetRed()/2 );
						Colour.SetGreen( Colour.GetGreen()/2 );
						Colour.SetBlue( Colour.GetBlue()/2 );
					}

					if (NetworkInterface::IsGameInProgress() && bDisplayNetworkGameOnVMap)
					{
						if(pObj->GetNetworkObject() && pObj->GetNetworkObject()->GetPlayerOwner())
						{
							PhysicalPlayerIndex playerIndex = pObj->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
							NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(playerIndex);
							Colour = NetworkColours::GetNetworkColour(colour);
						}
					}
					spdAABB box;
					box = pObj->GetBoundBox(box);
					CVectorMap::Draw3DBoundingBoxOnVMap(box, Colour);
					//CVectorMap::DrawMarker(VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()), Colour);
				}
			}
			sprintf(debugText, "Number of objects in pool: %d %d", numObjects, (int) CObject::GetPool()->GetSize());
			grcDebugDraw::AddDebugOutput(debugText);
		}
		if (bDisplayLineAboveObjects)
		{
			CObject::Pool *ObjPool = CObject::GetPool();
			CObject* pObj;
			s32 i = (s32) ObjPool->GetSize();

			while(i--)
			{
				pObj = ObjPool->GetSlot(i);

				if(pObj)
				{
					Color32	Colour(255, 255, 255, 255);
					if (pObj->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
					{			
						// Mission objects are twice as bright as regular objects
						Colour.SetRed( Colour.GetRed()/2 );
						Colour.SetGreen( Colour.GetGreen()/2 );
						Colour.SetBlue( Colour.GetBlue()/2 );
					}

					if (NetworkInterface::IsGameInProgress() && bDisplayNetworkGameOnVMap)
					{
						if(pObj->GetNetworkObject() && pObj->GetNetworkObject()->GetPlayerOwner())
						{
							PhysicalPlayerIndex playerIndex = pObj->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
							NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(playerIndex);
							Colour = NetworkColours::GetNetworkColour(colour);
						}
					}

					const Vector3 vObjPosition = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
					grcDebugDraw::Line(vObjPosition, vObjPosition+Vector3(0.0f, 0.0f, 15.0f), Colour);
				}
			}
		}
	}
	// Check for duplicate objects
	if(CDebugScene::bDisplayDuplicateObjectsBB || CDebugScene::bDisplayDuplicateObjectsOnVMap)
	{
		dev_float fDuplicateSepThresholdSq = 0.2f*0.2f;
		CObject::Pool* objectPool = CObject::GetPool();
		s32 i = (s32) objectPool->GetSize();
		while(i--)
		{
			s32 j = i;
			while(j--)
			{
				CObject* pObject = objectPool->GetSlot(i);
				CObject* pOtherObject = objectPool->GetSlot(j);
				if(pObject && pOtherObject
					&& (pObject->GetModelIndex() == pOtherObject->GetModelIndex())
					)
				{
					// Check if centres of these objects are close to each other, 
					// and also if their positions are both inside each other's bounding boxes

					Vector3 vCentre1;
					Vector3 vCentre2;
					pOtherObject->GetBoundCentre(vCentre2);
					pObject->GetBoundCentre(vCentre1);
					spdAABB tempBox1;
					spdAABB tempBox2;
					const spdAABB &box1 = pObject->GetBoundBox(tempBox1);
					const spdAABB &box2 = pOtherObject->GetBoundBox(tempBox2);
					float fSeperationSq = (vCentre1 - vCentre2).Mag2();

					if((fSeperationSq < fDuplicateSepThresholdSq)
						&& box1.ContainsPoint(RCC_VEC3V(vCentre2))
						&& box2.ContainsPoint(RCC_VEC3V(vCentre1))
						//&& (pOtherObject->GetPhysArch()->GetBound()->GetName() == pObject->GetPhysArch()->GetBound()->GetName())
						)
					{
						// Objects are overlapping so highlight them

						if(CDebugScene::bDisplayDuplicateObjectsBB)
						{
							CDebugScene::DrawEntityBoundingBox(pObject, Color32(255,0,0));
							CDebugScene::DrawEntityBoundingBox(pOtherObject, Color32(0,255,0));
						}
						if(CDebugScene::bDisplayDuplicateObjectsOnVMap)
						{
							CVectorMap::DrawMarker(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()),Color32(255,0,0));
						}
					}
				}
			}
		}
	}	// End if display duplicate objects

	//--------------------------------------------------------------------------
	// PICKUPS
	if (bDisplayPickupsOnVMap)
	{
		CPickupPlacement::Pool *PickupPool = CPickupPlacement::GetPool();
		CPickupPlacement* pPlacement;
		s32 i=PickupPool->GetSize();
		s32 numPickups = 0;

		while(i--)
		{
			pPlacement = PickupPool->GetSlot(i);

			if(pPlacement)
			{
				numPickups++;
				Color32	Colour(128, 128, 128, 255);

				if (pPlacement->GetPickup())
				{
					if (pPlacement->GetPickup()->GetNetworkObject() && pPlacement->GetPickup()->GetNetworkObject()->GetPlayerOwner())
					{
						NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(pPlacement->GetPickup()->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex());
						Colour = NetworkColours::GetNetworkColour(colour);
					}
					else
					{
						Colour = Color32(255, 0, 0, 255);
					}
				}

				CVectorMap::DrawMarker(pPlacement->GetPickupPosition(), Colour);
			}
		}
		sprintf(debugText, "Number of pickups in pool: %d %d", numPickups, CPickupPlacement::GetPool()->GetSize());
		grcDebugDraw::AddDebugOutput(debugText);
	}

#if __DEV
	//--------------------------------------------------------------------------
	// SCENE UPDATE
	if (bDisplaySceneUpdateCostOnVMap)
	{
		if (bDisplaySceneUpdateCostSelectedOnly)
		{
			// Pick the selected entity and display the cost only for that one.
			fwEntity *entity = g_PickerManager.GetSelectedEntity();

			if (entity)
			{
				fwSceneUpdateExtension *extension = entity->GetExtension<fwSceneUpdateExtension>();

				if (extension)
				{
					DisplaySceneUpdateCost(*entity, &iDisplaySceneUpdateCostStep);
				}
			}
		}
		else
		{
			// Show the cost for all entities.
			fwSceneUpdate::RunCustomCallbackBits(1 << iDisplaySceneUpdateCostStep, DisplaySceneUpdateCost, &iDisplaySceneUpdateCostStep);
		}
	}
#endif // __DEV
	//--------------------------------------------------------------------------
	// PORTALS
	if (bDisplayPortalInstancesOnVMap)
	{
		CPortalInst::Pool *Pool = CPortalInst::GetPool();
		CPortalInst* pPortalInst;
		s32 i=Pool->GetSize();
		s32 numObjects = 0;

		while(i--)
		{
			pPortalInst = Pool->GetSlot(i);

			if(pPortalInst)
			{
				numObjects++;
				Color32	Colour(100, 100, 255, 100);
				CVectorMap::DrawMarker(VEC3V_TO_VECTOR3(pPortalInst->GetTransform().GetPosition()), Colour);
			}
		}
	}

	//--------------------------------------------------------------------------
	// CAMERAS
    if(bDisplayRemotePlayerCameras)
    {
        if(NetworkInterface::IsGameInProgress())
        {
			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
            const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
            {
		        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

				CPed* pPlayerPed = pPlayer->GetPlayerPed();

                if(pPlayerPed)
                {
                    grcViewport *pViewport = ((CNetObjPlayer*)pPlayerPed->GetNetworkObject())->GetViewport();

                    if(pViewport)
                    {
					    NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(pPlayer->GetPhysicalPlayerIndex());
					    Color32 Colour = NetworkColours::GetNetworkColour(colour);
                        CVectorMap::DrawRemotePlayerCamera(RCC_MATRIX34(pViewport->GetCameraMtx()), pViewport->GetFarClip(), pViewport->GetTanHFOV(), Colour);

                        // draw the line from the actual to predicted position
                        bool playerInVehicle   = pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayerPed->GetMyVehicle();
                        Vector3 velocity       = playerInVehicle ? (pPlayerPed->GetMyVehicle()->GetVelocity() * CVehiclePopulation::ms_NetworkPlayerPredictionTime) : pPlayerPed->GetVelocity();
                        Vector3 futurePosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()) + velocity; // predict one second into the future

                        if(!velocity.IsZero())
                        {
                            CVectorMap::DrawLine(VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition()), futurePosition, Colour, Colour);
                        }
                    }
                }
            }
        }
    }

#if __DEV
    //--------------------------------------------------------------------------
	// NETWORKING
    NetworkInterface::DisplayNetworkingVectorMap();
#endif // __DEV
}
#endif
#if __DEV

#if __BANK
void CDebugScene::UpdateSceneUpdateCostStep()
{
	IF_COMMERCE_STORE_RETURN();

	safecpy(DisplaySceneUpdateCostStepName, fwSceneUpdate::GetSceneUpdateName(1 << iDisplaySceneUpdateCostStep));
}
#endif // __BANK

void CDebugScene::DisplaySceneUpdateCost(fwEntity &entity, void *userData)
{
	int updateToProfile = *((int *) userData);
	Vec3V position = entity.GetTransform().GetPosition();

	fwSceneUpdateExtension *extension = entity.GetExtension<fwSceneUpdateExtension>();
	FastAssert(extension);	// If we get to this function, it must have an extension.

	float radius = (float) extension->m_timeSpent[updateToProfile] / 50.0f;
	float colorRange = (float) extension->m_timeSpent[updateToProfile] / 100.0f;
	colorRange = 1.0f - Clamp(colorRange, 0.0f, 1.0f);

	Color32 color(1.0f, colorRange, colorRange);

	CVectorMap::DrawCircle(RCC_VECTOR3(position), radius + 0.5f, color);
}


#endif //__DEV

#if !__FINAL
void CDebugScene::PreUpdate()
{
#if DR_ENABLED
	if (debugPlayback::DebugRecorder::GetInstance())
	{
		debugPlayback::DebugRecorder::GetInstance()->PreUpdate();
	}
#endif // DR_ENABLED
}

void CDebugScene::Update()
{
	IF_COMMERCE_STORE_RETURN();

#if __BANK
	FlushTuneWidgetQueue();
#endif // __BANK

	g_pFocusEntity = FocusEntities_Get(0);

	if(	(FocusEntities_Get(0) == NULL) && 
		(atStringHash(ms_focusEntity0AddressString) != atStringHashNone))
	{
		formatf(ms_focusEntity0AddressString,   64, "%s", "None");

		DEV_ONLY( formatf(ms_focusEntity0ModelInfoString, 64, "%s", "None"); )
	}

#if ENABLE_ASYNC_STRESS_TEST
	if(WorldProbe::CShapeTestManager::ms_bAsyncStressTestEveryFrame)
	{
		WorldProbe::DebugAsyncStressTestOnce();
	}
#endif // ENABLE_ASYNC_STRESS_TEST

#if __DEV
	if(ms_makeFocusEntitiesFlash)
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			if(ms_focusEntities[i])
			{
				ms_focusEntities[i]->SetIsVisibleForModule( SETISVISIBLE_MODULE_DEBUG, (fwTimer::GetSystemFrameCount() & 1) );
			}
		}
	}

	// Scenario update
	CScenarioDebug::DebugUpdate();

	CVehicleFactory::DebugUpdate();
	UpdateAttachTest();
#endif // __DEV

	if (CPad* pPad = (camInterface::GetDebugDirector().IsFreeCamActive() && CControlMgr::IsDebugPadOn() ? &CControlMgr::GetDebugPad() : CControlMgr::GetPlayerPad()))
	{
		static bool debugTouchInputEnabled = PARAM_touchDebug.Get();
#if RSG_ORBIS
		if (!debugTouchInputEnabled)
		{
			debugTouchInputEnabled = pPad->GetButtonCircle() && pPad->GetButtonCross() && pPad->GetNumTouches() == 2;
		}
#endif //RSG_ORBIS

		if (debugTouchInputEnabled)
		{
#if RSG_ORBIS
			if (pPad->GetNumTouches() > 0)
#else
			if (pPad->GetLeftShoulder2())
#endif // RSG_ORBIS
			{
#if __BANK
				DisableInputsForTouchDebug();
#endif

#if RSG_ORBIS
				const bool twoTouches = (pPad->GetNumTouches() == 2);
#else
				const bool twoTouches = false;
#endif // RSG_ORBIS
				static u32 dirs = 0;
				static const u32 left = BIT0;
				static const u32 right = BIT1;
				static const u32 up = BIT2;
				static const u32 down = BIT3;
				dirs |= pPad->GetDPadLeft() ? left : 0;
				dirs |= pPad->GetDPadRight() ? right : 0;
				dirs |= pPad->GetDPadUp() ? up : 0;
				dirs |= pPad->GetDPadDown() ? down : 0;

				if (!pPad->GetDPadDown() && !pPad->GetDPadLeft() && !pPad->GetDPadRight() && !pPad->GetDPadUp())
				{
#if __BANK
					if (pPad->ButtonTriangleJustUp())
					{
						ms_DebugTimerMode = (ms_DebugTimerMode + 1) % DTM_NUM_MODES;

						switch (ms_DebugTimerMode)
						{
						case DTM_FrameCount:
						{
							break;
						}
						case DTM_TimerStart:
						{
							ms_DebugTimer = 0.0f;
							break;
						}
						case DTM_Off:
						{
							ms_DebugTimer = -1.0f;
							break;
						}
						default:
							break;
						}
					}
#endif // __BANK

					if (twoTouches && !PARAM_touchDebugDisableTwoTouchCommands.Get())
					{
						if (dirs == down)
						{
							// Delete focus peds.
							for (int i = (CDebugScene::FOCUS_ENTITIES_MAX - 1); i >= 0; i--)
							{
								if (CEntity* pFocus = FocusEntities_Get(i))
								{
									if (pFocus->GetIsTypePed())
									{
										if (static_cast<const CPed*>(pFocus)->IsPlayer())
										{
											// Don't try deleting a player
											continue;
										}
									}

									CGameWorld::RemoveEntity(pFocus);
								}
							}
						}
						else if (dirs == up)
						{
						}
						else if (dirs == left)
						{
							// Pause / unpause.
							fwTimer::SetDeferredDebugPause(!fwTimer::IsGamePaused());
							Displayf("Debug pause toggled by Orbis touchpad with -touchDebug on the commandline");
						}
						else if (dirs == right)
						{
							// Step a frame / pause.
							if (fwTimer::IsGamePaused())
							{
								fwTimer::SetWantsToSingleStepNextFrame();
							}
							else
							{
								fwTimer::SetDeferredDebugPause(true);
							}
						}
						else if (dirs == (right | up))
						{
						}
						else if (dirs == (left | up))
						{
						}
						else if (dirs == (down | right))
						{
							// Delete all except the focus ped.
							CEntity* pFocusEnt = FocusEntities_Get(0);
							CPed* pExceptPed = pFocusEnt && pFocusEnt->GetIsTypePed() ? static_cast<CPed*>(pFocusEnt) : nullptr;
							CPedPopulation::RemoveAllPedsWithException(pExceptPed, CPedPopulation::PMR_ForceAllRandomAndMission);
						}
						else if (dirs == (down | left))
						{
							//Remove all vehicles except the players
							CPed* pPed = CGameWorld::FindLocalPlayer();
							CVehicle* pPlayerVehicle = pPed->GetVehiclePedInside();

							CVehiclePopulation::RemoveAllVehsWithException(pPlayerVehicle);

							CPedPopulation::RemoveAllPedsWithException(NULL, CPedPopulation::PMR_ForceAllRandomAndMission);
						}
					}
					else
					{
						if (dirs == down)
						{
							// Down. Clear.
							FocusEntities_Clear();
						}
						else if (dirs == up)
						{
							// Up. All.
							FocusFilter filter;
							filter.bAnimal = true;
							filter.bBird = true;
							filter.bHuman = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
						else if (dirs == left)
						{
							// Left. Humans.
							FocusFilter filter;
							filter.bHuman = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
						else if (dirs == right)
						{
							// Right. Animals.
							FocusFilter filter;
							filter.bAnimal = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
						else if (dirs == (right | up))
						{
							// Up-Right. Vehicles.
							FocusFilter filter;
							filter.bVehicle = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
						else if (dirs == (left | up))
						{
							// Up-Left. Player.
							FocusFilter filter;
							filter.bPlayer = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
						else if (dirs == (down | right))
						{
							// Down-Right. All animals.
							FocusFilter filter;
							filter.bAnimal = true;
							filter.bBird = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
						else if (dirs == (down | left))
						{
							// Down-Left. Dead stuff!
							FocusFilter filter;
							filter.bDead = true;
							filter.bAnimal = true;
							filter.bBird = true;
							filter.bHuman = true;
							FocusEntities_Clear();
							FocusAddClosestAIPed(filter);
						}
					}

					// Clear the dirs var.
					dirs = 0;
				}

			}
#if RSG_ORBIS
			static u8 sequence = 0;
			const u8 touchCount = pPad->GetNumTouches();
			sequence = touchCount == 0 ? 0 : sequence;
			sequence = (sequence == 0 && touchCount == 1) ? 1 : sequence;
			sequence = (sequence == 1 && touchCount == 2) ? 2 : sequence;
			sequence = (sequence == 2 && touchCount == 1) ? 3 : sequence;
			sequence = (sequence == 3 && touchCount == 2) ? 4 : sequence;
			if (sequence == 4)
			{
				sequence = 0;
				if (!PARAM_touchDebugTapTapTapOhWhyHasTheGamePaused.Get())
				{
					fwTimer::SetDeferredDebugPause(!fwTimer::IsDebugPause());
				}
			}
#endif // RSG_ORBIS
		}
	}

#if __DEV
	if(ms_bEraserEnabled)
	{
		// delete all focus entities
		// This allows point and click erasing
		DeleteFocusEntities();
	}
#endif // __DEV

#if __BANK
	CPedVariationDebug::DebugUpdate();

	CVehicleDamage::UpdateNetworkDamageDebug();
	if (ms_bEnableFocusEntityDragging)
	{
		UpdateFocusEntityDragging();
	}
#endif //__BANK

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F1, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
	{
		SetDisplayDebugSummary(!GetDisplayDebugSummary());
	}

#if __BANK
	ms_lastFrameMouseX = ioMouse::GetX();
	ms_lastFrameMouseY = ioMouse::GetY();
#endif //__BANK

#if __BANK
	// Init inspector gadget window thing
	if (!m_pInspectorWindow)
	{
		CUiColorScheme colorScheme;
		float afColumnOffsets[] = { 0.0f, 192.0f };

		m_pInspectorWindow = rage_new CUiGadgetInspector(0.0f, 0.0f, 500.0f, GADGET_NUM, 2, afColumnOffsets, colorScheme);
		m_pInspectorWindow->SetUpdateCellCB(PopulateDebugSceneInspectorWindowCB);
		m_pInspectorWindow->SetNumEntries(GADGET_NUM);

		if (!m_bIsInspectorAttached)
		{
			m_bIsInspectorAttached = true;
			CGtaPickerInterface* pickerInterface = (CGtaPickerInterface*)g_PickerManager.GetInterface();
			if (pickerInterface)
			{
				pickerInterface->AttachInspectorChild(m_pInspectorWindow);
			}
		}
	}
#endif // __BANK	
#if DR_ENABLED
	if (debugPlayback::DebugRecorder::GetInstance())
	{
		debugPlayback::DebugRecorder::GetInstance()->Update();
	}
#endif // DR_ENABLED

#if __BANK
	INSTANCE_STORE.GetBoxStreamer().UpdateDebug();
	g_fwMetaDataStore.GetBoxStreamer().UpdateDebug();
	g_StaticBoundsStore.GetBoxStreamer().UpdateDebug();
#endif
}

void CDebugScene::PreRender()
{
}

// Templated function to make RederDebug code more paletable.
template<class T> void RenderDebugWholePool(void)
{
	typename T::Pool* pPool = T::GetPool();
	s32 poolSize = (s32) pPool->GetSize();
	for(s32 slot = 0; slot < poolSize; ++slot)
	{
		const T* pItem = pPool->GetSlot(slot);
		if(pItem)
		{
			pItem->RenderDebug();
		}
	}
}

#include<stdio.h>

void CDebugScene::Render_RenderThread()
{
	IF_COMMERCE_STORE_RETURN();

	// render physics simulation debug
#if __PFDRAW

	grcDebugDraw::SetDefaultDebugRenderStates();
	// Override lighting mode
	grcLightState::SetEnabled(true);

	const Matrix34 & cameraMtx = RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx());
	GetRageProfileDraw().SendDrawCameraForServer(cameraMtx);
	GetRageProfileDraw().Render();
	GetRageProfileDraw().SetDrawImmediately(true);
#endif // __PFDRAW

	CPhysics::RenderDebug();

#if __BANK
    NetworkDebug::RenderDebug();

	CAnimSceneManager::ProcessDebugRender();
#endif // __BANK

#if __PFDRAW
	GetRageProfileDraw().SetDrawImmediately(false);
	if(PFDGROUP_Fragment.WillDraw())
	{
		FRAGMGR->ProfileDraw();
	}
#endif

#if __BANK
	// render procedural object debug info
	g_procObjMan.RenderDebug();
#endif //__BANK

#if __BANK
	CWorldDebugHelper::DrawDebug();
	gWorldMgr.DrawDebug();
	CSceneIsolator::Draw();
	g_SceneCostTracker.DrawDebug();
	DEBUGSTREAMGRAPH.DrawDebug();

#if ENABLE_STATS_CAPTURE
	MetricsCapture::DrawWindows();
#endif // ENABLE_STATS_CAPTURE

#endif

}

#if DEBUG_DRAW
extern bool bDisplayName; // In PedVariationMgr.cpp
extern bool bDisplayFadeValue; // in pedvariationMgr.cpp
extern bool bShowMissingLowLOD; // In ped.cpp
extern bool bShowPedLODLevelData; // in ped.cpp
//extern bool CVehicleFactory::ms_bShowVehicleLODLevelData;	// in vehicleFactory.cpp
#endif

void CDebugScene::Render_MainThread()
{
#if USE_PROFILER_BASIC
	AttachProfilingVariables();
#endif

#if DEBUG_DRAW
	//Process ped debug visualiser keys every frame
	CPedDebugVisualiserMenu::Process();
	CVehicleDebugVisualiser::Process();
#endif

	PF_AUTO_PUSH_TIMEBAR("CDebugScene::Render_MainThread");

	IF_COMMERCE_STORE_RETURN();

	// Patrol route rendering
	CPatrolRoutes::DebugRenderRoutes();

#if SCENARIO_DEBUG
	// Scenario rendering
	CScenarioDebug::DebugRender(); 
#endif // SCENARIO_DEBUG

#if __BANK
	// Scenario rendering
	CRandomEventManager::DebugRender();
#endif // __BANK

	// Render cover-points, etc
	CCover::Render();

#if __BANK
	// Render the animation viewer?
	CAnimViewer::DebugRender();
	CAnimSceneManager::ProcessDebugRender();
#endif

#if __DEV
	// IKManager viewer debug render
	CIkManager::DebugRender();
#endif // __DEV

#if DEBUG_DRAW
	// Ped debug visualiser rendering. Inputs processed at start of Render_MainThread.
	CPedDebugVisualiserMenu::Render();
	CIncidentManager::GetInstance().DebugRender();
	CDispatchManager::GetInstance().DebugRender();

	const CPed* pPlayerPed = FindPlayerPed();
	if(pPlayerPed)
	{
		const CWanted* pWanted = pPlayerPed->GetPlayerWanted();
		if(pWanted)
		{
			pWanted->DebugRender();
		}
	}
#endif	// DEBUG_DRAW

#if __BANK // See JW
	CWeaponDebug::RenderDebug(); //Render weapons debug.
	CClimbDebug::RenderDebug(); //Render climbing debug.
	CDropDownDetector::RenderDebug(); //Render drop down debug.
	camManager::DebugRender(); //Render camera debug.
	CVehiclePopulation::UpdateDebugDrawing(); // Vehicle population, vector map, etc
	CVehicleAILodManager::Debug();
	CJunctions::RenderDebug();
	RenderDebugWholePool<CVehicle>();
	CObjectPopulation::UpdateDebugDrawing();
#if USE_OLD_DEBUGWINDOW
	CDebugWindowMgr::GetMgr().Draw();
#endif // USE_OLD_DEBUGWINDOW
	CDebugTextureViewerWindowMgr::GetMgr().Draw();
	g_UiGadgetRoot.DrawAll();
#endif // __BANK

#if __BANK
#if WORLD_PROBE_DEBUG
	WorldProbe::GetShapeTestManager()->RenderDebug(); // Render game shape test debug.
#endif // WORLD_PROBE_DEBUG
	CDeformableObjectManager::GetInstance().RenderDebug();
	CVehicleGadgetForks::RenderDebug();
#endif // __BANK

#if __DEV
	CNmDebug::RenderDebug(); // Render Natural Motion related debugging without an active NM task.
#endif // __DEV

	if (CPatrolRoutes::ms_bRender2DEffects)
	{
#if __DEV
		CWorldPoints::RenderDebug();
#endif
	}

#if __BANK
	CShockingEventsManager::RenderShockingEvents();
#endif

#if __BANK
	CFleeDebug::Render();
#endif

	CCombatManager::GetInstance().DebugRender();

	//------------------------------------------------------------------------------------------------------------


	// display navmeshes,	etc
#if __BANK
	CPathServer::Visualise();
	CScenarioBlockingAreas::RenderScenarioBlockingAreas();
#endif

#if __BANK
	// display paths
	ThePaths.RenderDebug();
	CPedPopulation::RenderDebug();
#endif // __DEV

#if __BANK
	// Print name of ped model above head.
	if(bDisplayName || bShowMissingLowLOD || bDisplayFadeValue || bShowPedLODLevelData)
	{
		CPed::Pool &PedPool = *CPed::GetPool();
		CPed* pPed;
		s32 k= PedPool.GetSize();
		while(k--)
		{
			pPed = PedPool.GetSlot(k);
			if(pPed)
			{
				char debugText[50];
				CPedModelInfo* pPMI = pPed->GetPedModelInfo();
				Assert(pPMI);

				sprintf(debugText, "OK");

				if(bDisplayName){
					sprintf(debugText, "%s", pPMI->GetModelName());
				}

				if(bDisplayFadeValue){
					sprintf(debugText, "(%d)", pPed->GetPedDrawHandler().GetVarData().GetLODFadeValue());
				}

				if(bShowMissingLowLOD && !pPMI->GetVarInfo()->HasLowLODs()){
					sprintf(debugText, "no LOD:%s", pPMI->GetModelName());
				}

				if (bShowPedLODLevelData)
				{
					Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					float pedDist = sqrtf(pedPos.Dist2(FRAGMGR->GetInterestMatrix().d));

					if (pPed->GetPedDrawHandler().GetVarData().GetIsHighLOD()) { sprintf(debugText, " %3.0f : High", pedDist); }
					else if (pPed->GetPedDrawHandler().GetVarData().GetIsSuperLOD()) { sprintf(debugText, " %3.0f : Super LOD", pedDist); }
					else { sprintf(debugText," %3.0f : Standard", pedDist); }	
				}

				grcDebugDraw::Text(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + Vector3(0.f,0.f,1.3f), CRGBA(255, 255, 128, 255), debugText);
			}		
		}
	}

	// Print debug vehicle stuff
	if(CVehicleFactory::ms_bShowVehicleLODLevelData)
	{
		CVehicle::Pool &VehiclePool = *CVehicle::GetPool();
		CVehicle* pVehicle;
		s32 k = (s32) VehiclePool.GetSize();
		while(k--)
		{
			pVehicle = VehiclePool.GetSlot(k);
			if(pVehicle)
			{
				char debugText[64];
				CVehicleModelInfo* pVMI = pVehicle->GetVehicleModelInfo();
				Assert(pVMI);

				sprintf(debugText, "OK");

				if(bDisplayName){
					sprintf(debugText, "%s", pVMI->GetModelName());
				}

// 				if(bShowMissingLowLOD && !pVMI->GetVarInfo()->HasLowLODs()){
// 					sprintf(debugText, "no LOD:%s", pPMI->GetModelName());
// 				}

				if (CVehicleFactory::ms_bShowVehicleLODLevelData)
				{
					Vector3 vehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
					float vehicleDist = sqrtf(vehiclePos.Dist2(FRAGMGR->GetInterestMatrix().d));

					{
						rmcDrawable *drawable = pVMI->GetDrawable();
						if (drawable){
							float dist0 = drawable->GetLodGroup().GetLodThresh(LOD_HIGH);
							float dist1 = drawable->GetLodGroup().GetLodThresh(LOD_MED);
							float dist2 = drawable->GetLodGroup().GetLodThresh(LOD_LOW);
							float dist3 = drawable->GetLodGroup().GetLodThresh(LOD_VLOW);
							u32 exists0 = drawable->GetLodGroup().ContainsLod(LOD_HIGH);
							u32 exists1 = drawable->GetLodGroup().ContainsLod(LOD_MED);
							u32 exists2 = drawable->GetLodGroup().ContainsLod(LOD_LOW);
							u32 exists3 = drawable->GetLodGroup().ContainsLod(LOD_VLOW);

							sprintf(debugText,"%s %2.0f :  HD(%2.0f), (%2.0f,%2.0f,%2.0f,%2.0f) : (%d,%d,%d,%d)",
								(pVehicle->GetIsCurrentlyHD() ? "STR HIGH" : ""),
								vehicleDist,  pVMI->GetHDDist(), dist0, dist1, dist2, dist3, exists0, exists1, exists2, exists3);
						}
					}
				}

				grcDebugDraw::Text(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) + Vector3(0.f,0.f,1.3f), CRGBA(255, 255, 128, 255), debugText);
			}		
		}
	}
#endif

	if(CAmbientClipStreamingManager::DISPLAY_AMBIENT_STREAMING_USE)
	{
		CAmbientClipStreamingManager::GetInstance()->DisplayAmbientStreamingUse();
	}

#if __BANK
	Color32 assetColors[fwBoxStreamerAsset::ASSET_TOTAL] =
	{
		Color32(255,0,0),	/* ASSET_STATICBOUNDS_MOVER */
		Color32(255,255,0),	/* ASSET_STATICBOUNDS_WEAPONS */
		Color32(0,255,255),	/* ASSET_STATICBOUNDS_MATERIAL */
		Color32(128,255,0),	/* ASSET_MAPDATA_HIGH */
		Color32(128,255,0),	/* ASSET_MAPDATA_HIGH_CRITICAL */
		Color32(128,255,0),	/* ASSET_MAPDATA_MEDIUM */
		Color32(128,255,0),	/* ASSET_MAPDATA_LOW */
		Color32(0,255,128)	/* ASSET_METADATA */
	};
	INSTANCE_STORE.GetBoxStreamer().RenderDebug(assetColors);
	g_fwMetaDataStore.GetBoxStreamer().RenderDebug(assetColors);
	g_StaticBoundsStore.GetBoxStreamer().RenderDebug(assetColors);
#endif	//__BANK

	// Render the train tracks.
	CTrain::RenderTrainDebug();

	CVehicleRecordingMgr::Render();

#if __BANK
	CWaypointRecording::Debug();
	CCompEntity::DebugRender();
#endif

	// Render the collision geometry of buildings & the world
	if(CRenderer::sm_bRenderCollisionGeometry)
	{
		CDebugScene::RenderCollisionGeometry();
	}

#if __DEV
	CPortalInst::ShowPortalInstances();
	CPortalTracker::RenderTrackerDebug();
	CInteriorGroupManager::DrawDebug();
#endif

#if __BANK
	CBoardingPointEditor::Process();
#endif

#if __JUNCTION_EDITOR
	CJunctionEditor::Render();
#endif

#if __NAVEDITORS
	CNavResAreaEditor::Render();
	CNavDataCoverpointEditor::Render();
#endif

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	CPrioritizedClipSetStreamer::GetInstance().RenderDebug();
#endif

#if DEBUG_DRAW
	CPrioritizedClipSetRequestManager::GetInstance().RenderDebug();
#endif // DEBUG_DRAW

#if __BANK
	CTacticalAnalysis::Render();

	if(bDisplayDoorPersistentInfo)
	{
		CDoorSystem::DebugDraw();
	}
#endif

	//------------------------------------------------------------------------------------------------------------


#if __DEV

	if(ms_drawProximityArea)
	{
		// Determine if we should draw this frame or not.
		bool drawThisFrame = true;
		if(ms_drawProximityAreaFlashes)
		{
			static bool frameFlasher = true;
			frameFlasher = !frameFlasher;
			drawThisFrame = frameFlasher;
		}
		if(drawThisFrame)
		{
			Vec3V vMin(ms_proximityTestPoint.x-ms_proximityMaxAllowableRadialDiff, ms_proximityTestPoint.y-ms_proximityMaxAllowableRadialDiff, ms_proximityTestPoint.z-ms_proximityMaxAllowableVerticalDiff);
			Vec3V vMax(ms_proximityTestPoint.x+ms_proximityMaxAllowableRadialDiff, ms_proximityTestPoint.y+ms_proximityMaxAllowableRadialDiff, ms_proximityTestPoint.z+ms_proximityMaxAllowableVerticalDiff);
			grcDebugDraw::BoxAxisAligned(vMin,vMax,Color32(255, 100, 100, 255));
		}
	}

	if (bDisplayDoorInfo)
	{
		CEntity* pCurrentEntity = GetEntityUnderMouse();
		if(pCurrentEntity && pCurrentEntity->GetIsTypeObject() && ((CObject*)pCurrentEntity)->IsADoor())
		{
			Vector3 vecPosition = VEC3V_TO_VECTOR3(pCurrentEntity->GetTransform().GetPosition());
			vecPosition.x = rage::Floorf(vecPosition.x + 0.5f);
			vecPosition.y = rage::Floorf(vecPosition.y + 0.5f);
			vecPosition.z = rage::Floorf(vecPosition.z + 0.5f);

			grcDebugDraw::AddDebugOutput("Door %s, Pos (%g, %g, %g) %s", pCurrentEntity->GetModelName(), vecPosition.x, vecPosition.y, vecPosition.z, pCurrentEntity->IsBaseFlagSet(fwEntity::IS_FIXED) ? "Fixed" : "Not Fixed");
		}

		const int maxDoors = (int) CObject::GetPool()->GetSize();
		for(int i = 0; i < maxDoors; i++)
		{
			const CObject* pObj = CObject::GetPool()->GetSlot(i);
			if(!pObj || !pObj->IsADoor())
			{
				continue;
			}
			const CDoor* pDoor = static_cast<const CDoor*>(pObj);

			char line1[1024];
			char line2[1024];
			char line3[1024];
			char buff[1024];

			const int doorType = pDoor->GetDoorType();
			formatf(line1, "Door %s 0x%x, Open %.2f, Tgt %.2f, %s, Type %d", pDoor->GetModelName(), pDoor, pDoor->GetDoorOpenRatio(), pDoor->GetTargetDoorRatio(), pDoor->IsBaseFlagSet(fwEntity::IS_FIXED) ? "Fixed" : "Not Fixed", doorType);

			if(CDoor::DoorTypeAutoOpens(doorType))
			{
				Vec3V testCenterV;
				float testRadius;
				pDoor->CalcAutoOpenInfo(testCenterV, testRadius);
				bool taper = true;
				const CDoorTuning* tuning = pDoor->GetTuning();
				if (tuning)
				{
					taper = tuning->m_AutoOpenCloseRateTaper;
				}
				formatf(line2, "Auto Open: Override Rate %.2f, Override Distance %.2f, Used Rate %.2f, Used Dist %.2f %s", pDoor->GetAutomaticRate(), pDoor->GetAutomaticDist(), pDoor->GetEffectiveAutoOpenRate(), testRadius, (taper)? "Tapered":"");
			}
			else
			{
				line2[0] = '\0';
			}
#if __BANK
			float massX = 1.0f;
			const char* tuningName = "No tuning";
			StdDoorRotDir stdDoorRotDir = StdDoorOpenBothDir;
			const CDoorTuning* tuning = pDoor->GetTuning();
			if(tuning)
			{
				tuningName = CDoorTuningManager::GetInstance().FindNameForTuning(*tuning);
				massX = tuning->m_MassMultiplier;
				stdDoorRotDir = tuning->m_StdDoorRotDir;
			}

			const char* rotDir = "";
			if (stdDoorRotDir == StdDoorOpenNegDir)
			{
				rotDir = "RotDir Neg";
			}
			else if (stdDoorRotDir == StdDoorOpenPosDir)
			{
				rotDir = "RotDir Pos";
			}
			formatf(line3, "%s XMass %.2f %s", tuningName, massX, rotDir);
#else
			const CDoorTuning* tuning = pDoor->GetTuning();
			if(tuning)
			{
				const char* rotDir = "";
				if (tuning->m_StdDoorRotDir == StdDoorOpenNegDir)
				{
					rotDir = "RotDir Neg";
				}
				else if (tuning->m_StdDoorRotDir == StdDoorOpenPosDir)
				{
					rotDir = "RotDir Pos";
				}

				formatf(line3, "XMass %.2f %s", tuning->m_MassMultiplier, rotDir);
			}
			else
			{
				line3[0] = '\0';
			}
#endif


			formatf(buff, "%s\n%s\n%s", line1, line2, line3);

			grcDebugDraw::Text(pDoor->GetTransform().GetPosition(), Color_white, buff);

		}
	}

	if (sm_VisualizeAutoOpenBounds)
	{
		const int maxDoors = (int) CObject::GetPool()->GetSize();
		for(int i = 0; i < maxDoors; i++)
		{
			const CObject* pObj = CObject::GetPool()->GetSlot(i);
			if(!pObj || !pObj->IsADoor())
			{
				continue;
			}
			const CDoor* pDoor = static_cast<const CDoor*>(pObj);

			if(CDoor::DoorTypeAutoOpens(pDoor->GetDoorType()))
			{
				const CDoorTuning* tuning = pDoor->GetTuning();
				if (!tuning || (tuning && !tuning->m_CustomTriggerBox))
				{
					Vec3V testCenterV;
					float testRadius;
					pDoor->CalcAutoOpenInfo(testCenterV, testRadius);
					grcDebugDraw::Sphere(testCenterV, testRadius, Color32(0xFFFF0000), false);
				}
				else
				{
					spdAABB localBBox;
					Mat34V mat;
					pDoor->CalcAutoOpenInfoBox(localBBox, mat);
					grcDebugDraw::BoxOriented(localBBox.GetMin(), localBBox.GetMax(), mat, Color32(0xFFFF0000), false);
				}
			}
		}
	}

	if(ms_drawFocusEntitiesBoundBox || ms_drawFocusEntitiesBoundBoxOnVMap || ms_drawFocusEntitiesSkeleton || 
		ms_drawFocusEntitiesSkeletonNonOrthonormalities || ms_drawFocusEntitiesSkeletonNonOrthoDataOnly || ms_drawFocusEntitiesSkeletonBoneNamesAndIds 
		|| ms_drawFocusEntitiesSkeletonChannels || ms_drawFocusEntitiesSkeletonNonOrthonoMats)
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			if(ms_focusEntities[i])
			{
				if(ms_drawFocusEntitiesBoundBox)
				{
					DrawEntityBoundingBox(ms_focusEntities[i], Color32(255, 0, 0, 255));
				}
				else if(ms_drawFocusEntitiesBoundBoxOnVMap)
				{
					// Make sure the VMap version flashesas if it was the entity drawing.
					if(ms_focusEntities[i]->GetIsVisible())
					{
						DrawEntityBoundingBoxOnVMap(ms_focusEntities[i], Color32(255, 0, 0, 255));
					}
					else
					{
						DrawEntityBoundingBoxOnVMap(ms_focusEntities[i], Color32(255, 0, 0, 64));
					}
				}

				if(ms_focusEntities[i]->GetIsDynamic())
				{
#if DEBUG_DRAW
					CDynamicEntity* pDynEnt = static_cast<CDynamicEntity*>(ms_focusEntities[i].Get());
					if(pDynEnt->GetSkeleton())
					{
						for(int iBoneIndex = 0; iBoneIndex < pDynEnt->GetSkeleton()->GetSkeletonData().GetNumBones(); iBoneIndex++)
						{
							const crBoneData* boneData = pDynEnt->GetSkeletonData().GetBoneData(iBoneIndex);

							Matrix34 matDraw;
							pDynEnt->GetGlobalMtx(iBoneIndex,matDraw);
							
							float fNonOrtho = matDraw.MeasureNonOrthonormality();
							bool bNonOrtho = fNonOrtho >= 1.0f;
							bool bDraw = !ms_drawFocusEntitiesSkeletonNonOrthoDataOnly || bNonOrtho;

							if(!bDraw) 
								continue;

							if( ms_drawFocusEntitiesSkeleton )
							{
								grcDebugDraw::Axis(matDraw,0.1f,true);
							}

							int lineNum = 0;
							char printBuffer[512];

							if( ms_drawFocusEntitiesSkeletonBoneNamesAndIds )
							{
								formatf(printBuffer,512,"%s, %i",boneData->GetName(), boneData->GetBoneId());
								grcDebugDraw::Text(matDraw.d,Color_white, 0, (grcDebugDraw::GetScreenSpaceTextHeight()-1) * lineNum++, printBuffer, true);
							}

							if( ms_drawFocusEntitiesSkeletonNonOrthonormalities || ms_drawFocusEntitiesSkeletonNonOrthonoMats)
							{
								Color32 col = fNonOrtho >= 1.0f ? Color_red : Color_white;

								if( fNonOrtho >= 100.0f )
								{
									col = Color_firebrick;
								}

								//Reasons for acceptable non-orthonormality
								if( boneData->HasDofs(crBoneData::SCALE)
									|| (Matrix33(matDraw).IsEqual(Matrix33(Matrix33::ZeroType) ) ) )
								{
									col = Color_copper;
								}
								
								if( ms_drawFocusEntitiesSkeletonNonOrthonormalities )
								{
									formatf(printBuffer,512,"%d:%f", iBoneIndex, fNonOrtho);
									grcDebugDraw::Text(matDraw.d,col, 0, (grcDebugDraw::GetScreenSpaceTextHeight()-1) * lineNum++, printBuffer, true);
								}

								if( fNonOrtho >= 1.0f )
								{
									if(ms_drawFocusEntitiesSkeletonNonOrthonoMats)
									{
										formatf(printBuffer,512,"[%.3f, %.3f, %.3f][%.3f, %.3f, %.3f][%.3f, %.3f, %.3f]", 
											matDraw.a.x, matDraw.a.y, matDraw.a.z, matDraw.b.x, matDraw.b.y, matDraw.b.z, matDraw.c.x, matDraw.c.y, matDraw.c.z);
										grcDebugDraw::Text(matDraw.d,col, 0, (grcDebugDraw::GetScreenSpaceTextHeight()-1) * lineNum++, printBuffer, true);
									}
								}
							}

							if( ms_drawFocusEntitiesSkeletonChannels )
							{
								formatf(printBuffer,512,"Rot Channels:%s. Scale Channels:%s. Trans Channels:%s", boneData->HasDofs(crBoneData::ROTATION) ? "true" : "false", boneData->HasDofs(crBoneData::SCALE) ? "true" : "false", boneData->HasDofs(crBoneData::TRANSLATION) ? "true" : "false");
								grcDebugDraw::Text(matDraw.d, Color_white, 0, (grcDebugDraw::GetScreenSpaceTextHeight()-1) * lineNum++, printBuffer, true);
							}
						}
					}
#endif
				}
			}
		}
	}

	if(ms_drawFocusEntitiesInfo)
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			if(ms_focusEntities[i])
			{
				PrintInfoAboutEntity(ms_focusEntities[i]);
			}
		}
	}

#if __BANK
	if(ms_drawFocusEntitiesCoverTuning)
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			if(ms_focusEntities[i])
			{
				PrintCoverTuningForEntity(ms_focusEntities[i]);
			}
		}
	}
#endif // __BANK

#endif // __DEV

#if DEBUG_DRAW
	if(GetDisplayDebugSummary())
	{
		bool curr = grcDebugDraw::GetDisplayDebugText();
		grcDebugDraw::SetDisplayDebugText(true);
		char debugText[1024];

		GetDebugSummary(debugText, GetDisplayLODCounts());

		char *pch = strtok (debugText, "\n");
		while (pch != NULL)
		{
			grcDebugDraw::AddDebugOutput(pch);
			pch = strtok (NULL, "\n");
		}

		grcDebugDraw::SetDisplayDebugText(curr);
	}
#endif // DEBUG_DRAW

}

#endif //__FINAL

#endif //!__FINAL

#if __BANK
//
// PURPOSE : copies debug summary information in a buffer
//
// NOTE : The buffer has to be big enough !
void CDebugScene::GetDebugSummary(char* summary, bool addLodCount)
{
	//num peds
	if (CPed::Pool * pPedPool = CPed::GetPool())
	{
		int totalCount = pPedPool->GetNoOfUsedSpaces();
		int otherCount   = CPedPopulation::ms_nNumOnFootOther + CPedPopulation::ms_nNumInVehOther;
		int missionCount = 0;
		int reusedCount = 0;
		int reusePool = 0;
		for(int i=0; i<pPedPool->GetSize(); i++)
		{
			if(const CPed * pPed = pPedPool->GetSlot(i))
			{
				missionCount += (pPed->PopTypeIsMission() ? 1 : 0);
				reusedCount += (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedWasReused) ? 1 : 0);
				reusePool += (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool) ? 1 : 0);
			}
		}

		sprintf(summary, "Peds: %d/%d (mission: %d, scenario (foot/car) %d/%d, ambient: (foot/car) %d/%d, other: %d, reusePool: %d) Reused: %d\n",
			totalCount, pPedPool->GetSize(), missionCount,
			CPedPopulation::ms_nNumOnFootScenario,
			CPedPopulation::ms_nNumInVehScenario,
			CPedPopulation::ms_nNumOnFootAmbient,
			CPedPopulation::ms_nNumInVehAmbient, otherCount, reusePool, reusedCount);
	}

	//num cars
	int iVehiclePoolSize, iTotalNumVehicles, iNumMission, iNumParked, iNumAmbient, iNumDesiredAmbient, iNumReal, iNumRealTraffic, iNumDummy, iNumSuperDummy, iNumRandomGenerated, iNumPathGenerated, iNumCargenGenerated, iNumReused, iNumInReusePool;
	CVehiclePopulation::GetPopulationCounts(iVehiclePoolSize, iTotalNumVehicles, iNumMission, iNumParked, iNumAmbient, iNumDesiredAmbient, iNumReal, iNumRealTraffic, iNumDummy, iNumSuperDummy, iNumRandomGenerated, iNumPathGenerated, iNumCargenGenerated, iNumReused, iNumInReusePool);

	sprintf(summary, "%sCars: %d/%d (ambient: %d/%d, mission: %d, parked: %d, reusePool: %d) (real: %d(%d traffic), dummy: %d, superdummy: %d) Reused: %d\n",
		summary, iTotalNumVehicles, iVehiclePoolSize,
		iNumAmbient, iNumDesiredAmbient,
		iNumMission, iNumParked, iNumInReusePool,
		iNumReal, iNumRealTraffic, iNumDummy, iNumSuperDummy, iNumReused);

	sprintf(summary, "%sRandom Cars: %d Path Cars: %d Cargen Cars: %d\n",
		summary, iNumRandomGenerated, iNumPathGenerated, iNumCargenGenerated);

	// Objects
	CObject::Pool* objPool = CObject::GetPool();
	CDummyObject::Pool* dummyObjPool = CDummyObject::GetPool();

	if (objPool && dummyObjPool)
	{
		int totalObjects = (int) objPool->GetNoOfUsedSpaces();
		int totalDummy = (int) dummyObjPool->GetNoOfUsedSpaces();
		sprintf(summary, "%sObjects: %d/%d - Dummy Objects: %d/%d\n",
			summary, totalObjects, objPool->GetSize(),
			totalDummy, dummyObjPool->GetSize());
	}

	// Display active (and total) peds, ragdolls, objects, and vehicles.
	s32 instPedsActive = 0;
	s32 instPedsTotal = 0;
	s32 instVehiclesActive = 0;
	s32 instVehiclesTotal = 0;
	s32 instObjectsTotal = 0;
	s32 instObjectsActive = 0;
	s32 instRagdollsActive = 0;
	s32 instRagdollsTotal = 0;
	for(int i = 0; CPhysics::GetLevel() && i < CPhysics::GetLevel()->GetMaxObjects(); ++i)
	{
		if(!CPhysics::GetLevel()->IsNonexistent(i))
		{
			phInst* pInst = CPhysics::GetLevel()->GetInstance(i);
			CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
			if(pEntity && pInst->GetClassType() != PH_INST_MAPCOL)
			{
				bool bActive = CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex());
				if(pEntity->GetIsTypeVehicle())
				{
					instVehiclesTotal++;
					instVehiclesActive += bActive ? 1 : 0;
				}
				if(pEntity->GetIsTypeObject())
				{
					instObjectsTotal++;
					instObjectsActive += bActive ? 1 : 0;
				}
				if(pEntity->GetIsTypePed())
				{
					instPedsTotal++;
					instPedsActive += bActive ? 1 : 0;
					if(((CPed*)pEntity)->GetRagdollState()==RAGDOLL_STATE_PHYS)
					{
						instRagdollsTotal++;
						instRagdollsActive += bActive ? 1 : 0;
					}
				}
			}
		}
	}

	sprintf(summary, "%sPhysics instances (active/total): objects %d/%d, cars: %d/%d, peds: %d/%d, ragdolls: %d/%d\n",
		summary, instObjectsActive, instObjectsTotal, instVehiclesActive, instVehiclesTotal,
		instPedsActive, instPedsTotal, instRagdollsActive, instRagdollsTotal);

	//num lights and shadow casting lights
	int shadowLightCount=0;
	int lightCount=0;

	for (int i=0;i<Lights::m_numPrevSceneShadowLights;i++)
	{
		if (CParaboloidShadow::GetLightShadowIndex(Lights::m_prevSceneShadowLights[i],true)!=-1)
			shadowLightCount++;

		lightCount++;
	}

	sprintf(summary, "%slights: %d shadow casters:%d\n", summary, lightCount, shadowLightCount);

#if RAGE_TIMEBARS
	s32 callcount;
	float timea = 0.0f,timeb = 0.0f;
	g_pfTB.GetFunctionTotals("Physics",callcount,timea,0);
	g_pfTB.GetFunctionTotals("Script",callcount,timeb,0);
	sprintf(summary, "%sscript: %.2fms physics: %.2fms\n", summary, timeb, timea);
#endif

	// B*537883
	if( addLodCount )
	{
		int iNumActive, iNumInactive;
		int iNumPeds;
		int iNumEventScanHi, iNumEventScanLo;
		int iNumMotionTaskHi, iNumMotionTaskLo;
		int iTimeslicingHi, iTimeslicingLo;
		int iNumPhysicsHi, iNumPhysicsLo;
		int iNumEntityScanHi, iNumEntityScanLo;
		int iNumTimesliced;

		sprintf(summary, "%sVehicle and Ped LOD Counts\n", summary);

		// Get the info for the Vehicle LOD Counts.
		sprintf(summary, "%sVehicles:-\n", summary);
		CVehiclePopulation::GetVehicleLODCounts(true, iTotalNumVehicles, iNumReal, iNumDummy, iNumSuperDummy, iNumActive, iNumInactive, iNumTimesliced );
		sprintf(summary, "%sParked: %i\n", summary, iTotalNumVehicles);
		sprintf(summary, "%s%i real, %i dummy, %i superdummy\n", summary, iNumReal, iNumDummy, iNumSuperDummy);
		sprintf(summary, "%s%i active, %i inactive\n", summary, iNumActive, iNumInactive);
		sprintf(summary, "%s%i timesliced\n", summary, iNumTimesliced);

		CVehiclePopulation::GetVehicleLODCounts(false, iTotalNumVehicles, iNumReal, iNumDummy, iNumSuperDummy, iNumActive, iNumInactive, iNumTimesliced );
		sprintf(summary, "%sMoving: %i\n", summary, iTotalNumVehicles);
		sprintf(summary, "%s%i real, %i dummy, %i superdummy\n", summary, iNumReal, iNumDummy, iNumSuperDummy);
		sprintf(summary, "%s%i active, %i inactive\n", summary, iNumActive, iNumInactive);
		sprintf(summary, "%s%i timesliced\n", summary, iNumTimesliced);

		// Get the info for the Ped LOD Counts.
		sprintf(summary, "%sPeds:-\n", summary);
		CPedPopulation::GetPedLODCounts(iNumPeds, iNumEventScanHi, iNumEventScanLo, iNumMotionTaskHi, iNumMotionTaskLo, iNumPhysicsHi, iNumPhysicsLo, iNumEntityScanHi, iNumEntityScanLo, iNumActive, iNumInactive, iTimeslicingHi, iTimeslicingLo );
		sprintf(summary, "%sCount = %i\n", summary, iNumPeds);
		sprintf(summary, "%sEvent scanning (h:%i/l:%i), Motion task (h:%i/l:%i), physics(h:%i/l:%i), Entity scanning (h:%i/l:%i), Timeslicing (h:%i/l:%i)\n", summary, iNumEventScanHi, iNumEventScanLo, iNumMotionTaskHi, iNumMotionTaskLo, iNumPhysicsHi, iNumPhysicsLo, iNumEntityScanHi, iNumEntityScanLo, iTimeslicingHi, iTimeslicingLo);
		sprintf(summary, "%s%i active, %i inactive\n", summary, iNumActive, iNumInactive);
	}
}

CAuthoringHelper::CAuthoringHelper(const char* Label, bool useHelpersMenu)
{
	Init(); 
	m_Label = Label; 
	m_useHelpersMenu = useHelpersMenu; 
}

CAuthoringHelper::~CAuthoringHelper()
{
	if(ms_SelectedAuthorHelper == this)
	{
		ms_DistanceToCamera = FLT_MAX; 
		ms_SelectedAuthorHelper = NULL; 
	}

	//if(	ms_LastSelectedAuthorHelper == this)
	//{
	//	ms_LastSelectedAuthorHelper = NULL; 
	//}


	delete ms_pAuthorToolBar; 
	ms_pAuthorToolBar = NULL; 
}

void CAuthoringHelper::Init()
{
	m_ScreenSpaceTangentVector.Set(0.0f, 0.0f); 
	m_CurrentInput = NO_INPUT;  
	m_IsAxisSelected = false;
	m_IsFirstUpdate = false; 
	m_useHelpersMenu = true; 

	m_AxisBox.RelativePositionOnAxis = 0.5f; 
	m_AxisBox.ScaledBoxDimensions = 0.5f; 
	m_AxisBox.NonScaledBoxDimenstions = 0.05f; 

	m_PlaneBox.RelativePositionOnAxis = 0.2f; 
	m_PlaneBox.ScaledBoxDimensions = 0.2f; 
	m_PlaneBox.NonScaledBoxDimenstions = 0.001f; 

	m_StartedTextInput = false; 
}


void CAuthoringHelper::CreateBoxMatMinAndMaxVectors(const Matrix34& Mat, const Vector3& Axis, float scale, const Vector3& Offset, const BoxDimensions& Dimensions, Matrix34& BoxMat, Vector3& VecMin, Vector3& VecMax)
{
	Matrix34 debugAxis(Mat);
	Vector3 OffsetPos; 

	debugAxis.Transform(Offset, OffsetPos); 

	debugAxis.d = OffsetPos;

	BoxMat = debugAxis; 

	Vector3 AxisVec(scale * Dimensions.RelativePositionOnAxis * Axis.x, scale * Dimensions.RelativePositionOnAxis * Axis.y, scale * Dimensions.RelativePositionOnAxis * Axis.z);  
	Vector3 AxisVecWorld; 

	BoxMat.Transform(AxisVec, AxisVecWorld); 

	//position matrix relative to scene origin
	BoxMat.d = AxisVecWorld; 

	//create a local vector to create a collision box

	for(int i = 0; i < 3; i++)
	{
		if(Axis[i] > 0.0f)
		{
			VecMax[i] = scale * Dimensions.ScaledBoxDimensions;
		}
		else
		{
			VecMax[i] = Dimensions.NonScaledBoxDimenstions; 
		}
	}

	// Create a min vector which is the opposite of the max
	VecMin = VecMax; 

	VecMin.Scale(-1.0f); 
};

void CAuthoringHelper::CreateTranslationCollisionBox(const Matrix34& Mat, const Vector3& Axis, float scale, const Vector3& Offset, const BoxDimensions& Dimensions, spdOrientedBB & Bound)
{
	Matrix34 BoxMat; 
	Vector3 VecMin = VEC3_ZERO; 
	Vector3 VecMax = VEC3_ZERO; 

	CreateBoxMatMinAndMaxVectors(Mat, Axis, scale, Offset, Dimensions, BoxMat, VecMin, VecMax); 

	const spdAABB& LocalBox = spdAABB(VECTOR3_TO_VEC3V(VecMin), VECTOR3_TO_VEC3V(VecMax)); 

	Bound.Set(LocalBox, MATRIX34_TO_MAT34V(BoxMat));

}

void CAuthoringHelper::RenderTranslationCollisionBox(const Matrix34& Mat, const Vector3& Axis, float scale, const Vector3& Offset,const Color32 color, const BoxDimensions& Dimensions)
{
	Matrix34 BoxMat; 
	Vector3 VecMin = VEC3_ZERO; 
	Vector3 VecMax = VEC3_ZERO; 

	CreateBoxMatMinAndMaxVectors(Mat, Axis, scale, Offset, Dimensions, BoxMat, VecMin, VecMax); 

	grcDebugDraw::BoxOriented( VECTOR3_TO_VEC3V(VecMin),  VECTOR3_TO_VEC3V(VecMax), MATRIX34_TO_MAT34V(BoxMat), color, false ); 
}

void CAuthoringHelper::UpdateMouseTranslationCollisionBox(spdOrientedBB & MouseProbe)
{
	Vector3 mouseScreen, mouseFar;
	CDebugScene::GetMousePointing(mouseScreen, mouseFar, false);

	Matrix34 ScreenMouseMat; 

	Vector3 MouseFront = mouseFar - mouseScreen; 
	MouseFront.NormalizeSafe(); 

	camFrame::ComputeWorldMatrixFromFront(MouseFront, ScreenMouseMat); 
	ScreenMouseMat.d = mouseScreen; 

	Vector3 LocalProbe(0.0f, 500.0f, 0.0f); 
	Vector3 ProbeWorld; 

	ScreenMouseMat.Transform(LocalProbe, ProbeWorld); 

	Matrix34 MouseMat(ScreenMouseMat); 
	MouseMat.d = ProbeWorld; 

	//grcDebugDraw::Axis(MouseMat, 1.0f, true);

	Vector3 MouseLocalMax (0.001f, 500.0f, 0.001f); 
	Vector3 MouseLocalMin (-0.001f, -500.0f, -0.001f); 

	const spdAABB& MouseLocalBox = spdAABB(VECTOR3_TO_VEC3V(MouseLocalMin), VECTOR3_TO_VEC3V(MouseLocalMax)); 
	MouseProbe.Set(MouseLocalBox, MATRIX34_TO_MAT34V(MouseMat));
}

void CAuthoringHelper::RenderTranslationHelper(const Matrix34& AuthoringMat, float Scale, const Vector3& UNUSED_PARAM(Offset))
{


#if DEBUG_DRAW
	//grcDebugDraw::Axis(AuthoringMat, Scale, true);
	grcDebugDraw::GizmoPosition(MATRIX34_TO_MAT34V(AuthoringMat), Scale, m_CurrentInput);
#endif

#if 0	//enable this if you want to see the collision boxes
	if(ms_SelectedAuthorHelper == this || ms_SelectedAuthorHelper == NULL)
	{
		Vector3 XYAxis = XAXIS + YAXIS; 
		Vector3 XZAxis = XAXIS + ZAXIS; 
		Vector3 ZYAxis = ZAXIS + YAXIS; 

		if(m_CurrentInput != XY_PLANE)
		{
			CAuthoringHelper::RenderTranslationCollisionBox(AuthoringMat, XYAxis, Scale, VEC3_ZERO, Color_red, m_PlaneBox); 
		}
		else 
		{
			CAuthoringHelper::RenderTranslationCollisionBox(AuthoringMat, XYAxis, Scale, VEC3_ZERO, Color_white, m_PlaneBox); 
		}

		if(m_CurrentInput != XZ_PLANE)
		{
			CAuthoringHelper::RenderTranslationCollisionBox(AuthoringMat, XZAxis, Scale, VEC3_ZERO, Color_blue, m_PlaneBox); 
		}
		else
		{
			CAuthoringHelper::RenderTranslationCollisionBox(AuthoringMat, XZAxis, Scale, VEC3_ZERO, Color_white, m_PlaneBox); 
		}

		if(m_CurrentInput != ZY_PLANE)
		{
			CAuthoringHelper::RenderTranslationCollisionBox(AuthoringMat, ZYAxis, Scale, VEC3_ZERO, Color_green, m_PlaneBox); 
		}
		else
		{
			CAuthoringHelper::RenderTranslationCollisionBox(AuthoringMat, ZYAxis, Scale, VEC3_ZERO, Color_white, m_PlaneBox); 
		}

		if(m_CurrentInput == X_AXIS)
		{
			RenderTranslationCollisionBox(AuthoringMat, XAXIS, Scale, VEC3_ZERO, Color_white, m_AxisBox); 
		}

		if(m_CurrentInput == Y_AXIS)
		{
			RenderTranslationCollisionBox(AuthoringMat, YAXIS, Scale, VEC3_ZERO, Color_white, m_AxisBox); 
		}

		if(m_CurrentInput == Z_AXIS)
		{
			RenderTranslationCollisionBox(AuthoringMat, ZAXIS, Scale, VEC3_ZERO, Color_white, m_AxisBox); 
		}
	}
#endif
}

Vector3 CAuthoringHelper::UpdateTranslationAxis(const Matrix34& Mat, const Vector3& Axis ,const spdPlane& Plane )
{
	//if(CDebugScene::GetMouseDX() == 0 && CDebugScene::GetMouseDY() == 0)
	int MouseDx = ioMouse::GetX() - m_LastFrameMouseX;
	int MouseDy = ioMouse::GetY() - m_LastFrameMouseY;
	
	if(	MouseDx == 0 && MouseDy == 0)
	{
		return VEC3_ZERO; 
	}
	
	Vector3 mouseScreen, mouseFar;
	//CDebugScene::GetMousePointing(mouseScreen, mouseFar, false);
	float windowX, windowY; 

	//get current screen space of matrix origin
	CDebugScene::GetScreenPosFromWorldPoint(Mat.d, windowX, windowY); 

	const float viewportX = windowX + (float)MouseDx; 
	const float viewportY = windowY + (float)MouseDy; 

	//Displayf("mouse: %f, %f ", (float)(ioMouse::GetX() - m_lastFrameMouseX), (float)(ioMouse::GetX() - m_lastFrameMouseX));

	gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(viewportX, viewportY, (Vec3V&)mouseScreen, (Vec3V&)mouseFar);

	Vec3V WorldPos; 

	Vector3 Dir = mouseFar - mouseScreen; 

	Plane.IntersectRayV(VECTOR3_TO_VEC3V(mouseScreen), VECTOR3_TO_VEC3V(Dir), WorldPos);

//	Displayf("Mat.d: %f, %f ", (float)CDebugScene::GetMouseDX(), (float)CDebugScene::GetMouseDY());

	Vector3 LocalToMat; 

	//grcDebugDraw::Sphere( WorldPos, 0.2f, Color_blue ); 

	Mat.UnTransform(VEC3V_TO_VECTOR3(WorldPos), LocalToMat); 

	Vector3 FinalVectorToApply = VEC3_ZERO; 

	for(int i =0; i < 3; i++)
	{
		Vector3 LocalAxis = VEC3_ZERO; 
		Vector3 MatAxis = VEC3_ZERO; 
		float AxisScalar = 0.0f; 

		if(Axis[i] > 0.0f && i == 0)
		{
			MatAxis = Mat.a; 
			LocalAxis = XAXIS; 
		}

		if(Axis[i] > 0.0f && i == 1)
		{
			MatAxis = Mat.b; 
			LocalAxis = YAXIS; 
		}

		if(Axis[i] > 0.0f && i == 2)
		{
			MatAxis = Mat.c; 
			LocalAxis = ZAXIS; 
		}

		AxisScalar = LocalAxis.Dot(LocalToMat); 
		FinalVectorToApply += MatAxis * AxisScalar; 
	}

	return FinalVectorToApply; 
}

bool CAuthoringHelper::Update(Matrix34& mat)
{
	Vector3 Offset(VEC3_ZERO); 
	float Scale = 1.0f; 

	Matrix34 parentMat(Matrix34::IdentityType); 

	return Update(mat, Scale, Offset, mat, parentMat); 
}

bool CAuthoringHelper::Update(const Matrix34& mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat)
{
	const Matrix34 parentMat(Matrix34::IdentityType); 

	return Update(mat, Scale, Offset, UpdatedMat, parentMat); 
}

bool CAuthoringHelper::Update(const Matrix34& mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat, const Matrix34& ParentMat)
{
	UpdatedMat = mat; 

	//UpdateHelperFromInput(UpdatedMat); 

	UpdatedMat.Dot(ParentMat);
	
	bool update = false; 

	UpdateAuthorUi(UpdatedMat); 

	if(ms_AuthorMode == TRANS_GBL || ms_AuthorMode == TRANS_LCL)
	{
		update = UpdateTranslationHelper(UpdatedMat, Scale, Offset, UpdatedMat);
	}

	if(ms_AuthorMode == ROT_GBL || ms_AuthorMode == ROT_LCL)
	{
		update = UpdateRotationHelper(UpdatedMat, Scale, Offset, UpdatedMat);
	}


		Assertf(UpdatedMat.IsOrthonormal(), "Helper produced non orthonormal");
	

	if(m_Label.GetLength() > 0)
	{
		grcDebugDraw::Text(UpdatedMat.d, Color_black, 0, 0, m_Label.c_str());
	}

	UpdatedMat.DotTranspose(ParentMat);
	
	m_LastFrameMouseY = ioMouse::GetY();
	m_LastFrameMouseX = ioMouse::GetX();

	return update; 
}

//void CAuthoringHelper::UpdateHelperFromInput(Matrix34& UpdatedMat)
//{
//	if(ms_pAuthorToolBar && ms_LastSelectedAuthorHelper == this)
//	{
//		float userValue = 0.0f; 
//		s32 authorMode = ms_pAuthorToolBar->GetCurrentIndex(); 
//		char info[64]; 
//
//		if(ms_pAuthorToolBar->UserProcessClick())
//		{
//			s32 currentAuthorMode = ms_pAuthorToolBar->GetCurrentIndex(); 
//			if(ms_AuthorMode == TRANS_GBL || ms_AuthorMode == TRANS_LCL)
//			{
//				m_StartedTextInput = false; 
//				m_Keyboardbuffer.Clear(); 
//
//				if(currentAuthorMode == INPUT_X)
//				{
//					char temp[32]; 
//					formatf(temp, "%f", UpdatedMat.d.x); 
//					m_Keyboardbuffer =temp; 
//				}
//
//				if(currentAuthorMode == INPUT_Y)
//				{
//					char temp[32]; 
//					formatf(temp, "%f", UpdatedMat.d.y); 
//					m_Keyboardbuffer =temp; 
//				}
//				if(currentAuthorMode == INPUT_Z)
//				{
//					char temp[32]; 
//					formatf(temp, "%f", UpdatedMat.d.z); 
//					m_Keyboardbuffer =temp; 
//				}
//
//			}
//		}
//		if(authorMode == INPUT_X)
//		{
//			formatf(info, "X:%s", m_Keyboardbuffer.c_str()); 
//			ms_pAuthorToolBar->SetCell(0, 4, NULL); 
//			ms_pAuthorToolBar->SetCell(0, 4, info); 
//		}
//
//		if(authorMode == INPUT_Y)
//		{
//			formatf(info, "Y:%s", m_Keyboardbuffer.c_str()); 
//			ms_pAuthorToolBar->SetCell(0, 5, NULL); 
//			ms_pAuthorToolBar->SetCell(0, 5, info); 
//		}
//
//		if(authorMode == INPUT_Z)
//		{
//			formatf(info, "Z:%s", m_Keyboardbuffer.c_str()); 
//			ms_pAuthorToolBar->SetCell(0, 6, NULL); 
//			ms_pAuthorToolBar->SetCell(0, 6, info); 
//		}
//
//		if(UpdateKeyboardInput(userValue))
//		{
//			if(ms_AuthorMode == TRANS_GBL || ms_AuthorMode ==TRANS_LCL)
//			{
//				if(authorMode == INPUT_X)
//				{
//					UpdatedMat.d.x = userValue; 
//				}
//
//				if(authorMode == INPUT_Y)
//				{
//					UpdatedMat.d.y = userValue; 
//				}
//
//				if(authorMode == INPUT_Z)
//				{
//					UpdatedMat.d.z = userValue; 
//				}
//			}
//
//		}
//	}
//}


bool CAuthoringHelper::IsMakingAuthorMenuSelection()
{
	if(ms_pAuthorToolBar)
	{
		return ms_pAuthorToolBar->ContainsMouse(); 	
	}

	return false; 
}

//bool CAuthoringHelper::UpdateKeyboardInput(float &newValue)
//{	
//	if(ms_pAuthorToolBar)
//	{
//		s32 authorMode = ms_pAuthorToolBar->GetCurrentIndex(); 
//
//		if(authorMode == INPUT_X || authorMode == INPUT_Y || authorMode == INPUT_Z )
//		{
//			bool haveKeyInput = false;
//			atString tempBuffer; 
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD0, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "0";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD1, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "1";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD2, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "2";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD3, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "3";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD4, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "4";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD5, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "5";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD6, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "6";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD7, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "7";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD8, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "8";
//				haveKeyInput = true;
//
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD9, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				tempBuffer += "9";
//				haveKeyInput = true;
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SUBTRACT, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				if(!m_StartedTextInput)
//				{
//					tempBuffer += "-";
//					haveKeyInput = true;
//				}
//			}
//
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DECIMAL, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				if(m_StartedTextInput)
//				{
//					if(m_Keyboardbuffer.IndexOf(".") == -1 && m_Keyboardbuffer.GetLength() != 0)
//					{
//						tempBuffer += ".";
//						haveKeyInput = true;
//					}
//				}
//			}
//
//			if(haveKeyInput)
//			{
//				if(!m_StartedTextInput)
//				{
//					m_StartedTextInput = true; 
//					m_Keyboardbuffer.Clear(); 
//				}
//				m_Keyboardbuffer += tempBuffer; 
//			}
//		
//			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPADENTER, KEYBOARD_MODE_DEBUG, "Toggle debug summary")||
//				CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RETURN, KEYBOARD_MODE_DEBUG, "Toggle debug summary"))
//			{
//				//ms_pAuthorToolBar->SetCurrentIndex(ms_AuthorMode); 
//				m_StartedTextInput = false; 
//				newValue = (float) atof(m_Keyboardbuffer.c_str()); 
//				//m_Keyboardbuffer.Clear();
//				
//				return true;  
//
//			}
//		}
//	}
//	return false; 
//};

void CAuthoringHelper::UpdateAuthorUi(const Matrix34& UpdatedMat)
{
	if(m_useHelpersMenu)
	{
		CreateAuthorUi(); 

		if(ms_pAuthorToolBar)
		{
			ms_pAuthorToolBar->RequestSetViewportToShowSelected();

			ms_pAuthorToolBar->SetDisplay(true);

			s32 authorMode = ms_pAuthorToolBar->GetCurrentIndex(); 

			if(ms_pAuthorToolBar->UserProcessClick())
			{
				s32 currentAuthorMode = ms_pAuthorToolBar->GetCurrentIndex(); 

				authorMode = currentAuthorMode; 

				if(authorMode < MAX_AUTHOR_MODES && authorMode > NO_AUTHOR_MODE)
				{
					ms_AuthorMode = authorMode; 
				}
			}

			if(ms_SelectedAuthorHelper == this)
			{
				if(ms_AuthorMode == TRANS_GBL || ms_AuthorMode == TRANS_LCL)
				{
					char info[64];

					formatf(info, "X:%f", UpdatedMat.d.x); 
					ms_pAuthorToolBar->SetCell(0, 4, info); 

					formatf(info, "Y:%f", UpdatedMat.d.y);
					ms_pAuthorToolBar->SetCell(0, 5, info); 

					formatf(info, "Z:%f", UpdatedMat.d.z); 
					ms_pAuthorToolBar->SetCell(0, 6, info); 

				}

			}
		}
	}
}

void CAuthoringHelper::CreateAuthorUi()
{
	if(ms_pAuthorToolBar == NULL)
	{

		ms_pAuthorToolBar =  rage_new CUiGadgetSimpleListAndWindow("Author", 100.0f, 100.0f, 100.0f, 7); 
		ms_pAuthorToolBar->SetNumEntries(7);
		ms_pAuthorToolBar->SetCell(0, 0, "Trans:GBL"); 
		ms_pAuthorToolBar->SetCell(0, 1, "Trans:LCL"); 
		ms_pAuthorToolBar->SetCell(0, 2, "Rot:GBL"); 
		ms_pAuthorToolBar->SetCell(0, 3, "Rot:LCL"); 
		ms_pAuthorToolBar->SetCell(0, 4, "X:"); 
		ms_pAuthorToolBar->SetCell(0, 5, "Y:"); 
		ms_pAuthorToolBar->SetCell(0, 6, "Z:"); 
		ms_pAuthorToolBar->SetCurrentIndex(ms_AuthorMode); 
	}
}

void CAuthoringHelper::UpdateAxisRotation(const Matrix34& AuthoringMat, Matrix34& UpdatedMat, const Vector3& SelectionVec, const Vector3& SelectionWorld)
{
	TUNE_GROUP_FLOAT( DEBUG_CONTROL, focusEntityRotationSensitivity, 0.01f, 0.000f, 100.0f,0.10f);

	if(m_IsFirstUpdate)
	{
		Vector3 RotatedLocal(SelectionVec); 

		switch (m_CurrentInput)
		{
		case ROT_Z_PLANE: //Y
			{
				RotatedLocal.RotateZ(HALF_PI); 
			}
			break;
		case ROT_Y_PLANE:
			{
				RotatedLocal.RotateY(HALF_PI); 
			}
			break;
		case ROT_X_PLANE:
			{
				RotatedLocal.RotateX(HALF_PI); 
			}
			break;
		}

		RotatedLocal.NormalizeSafe(); 

		Vector3 Result = SelectionVec + RotatedLocal; 

		AuthoringMat.Transform(Result); 

		Vector2 Screenextend; 
		Vector2 ScreenBase; 

		CDebugScene::GetScreenPosFromWorldPoint(Result, Screenextend.x, Screenextend.y);

		CDebugScene::GetScreenPosFromWorldPoint(SelectionWorld, ScreenBase.x, ScreenBase.y);

		m_ScreenSpaceTangentVector = Screenextend - ScreenBase; 

		m_ScreenSpaceTangentVector.NormalizeSafe(); 

	}

	int MouseDx = ioMouse::GetX() - m_LastFrameMouseX;
	int MouseDy = ioMouse::GetY() - m_LastFrameMouseY;

	if(	MouseDx != 0 || MouseDy != 0)
	{
//	if(CDebugScene::MouseMovedThisFrame())
//	{
		Vector2 MouseDelta((float)MouseDx, (float)MouseDy); 

		float heading = m_ScreenSpaceTangentVector.Dot(MouseDelta); 

		heading *= focusEntityRotationSensitivity; 

		switch (m_CurrentInput)
		{
		case ROT_Z_PLANE: //Y
			{
				if(ms_AuthorMode == ROT_LCL)
				{
					UpdatedMat.RotateLocalZ(heading);
				}
				else
				{
					UpdatedMat.RotateZ(heading);
				}
			}
			break;

		case ROT_Y_PLANE:
			{
				if(ms_AuthorMode == ROT_LCL)
				{
					UpdatedMat.RotateLocalY(heading);
				}
				else
				{
					UpdatedMat.RotateY(heading);
				}
			}
			break;
		case ROT_X_PLANE:
			{
				if(ms_AuthorMode == ROT_LCL)
				{
					UpdatedMat.RotateLocalX(heading);
				}
				else
				{
					UpdatedMat.RotateX(heading);
				}
			}
			break;
		}
	}
}

bool CAuthoringHelper::ShouldUpdateSelectedHelper(const Vector3& MatrixPosition )
{
	bool ShouldUpdate = false; 
	if(m_CurrentInput != NO_INPUT)
	{
		//reset this
		m_IsFirstUpdate = false; 

		if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT)
		{
			m_CurrentInput = NO_INPUT; 
			m_IsAxisSelected = false; 
			ms_SelectedAuthorHelper = NULL; 
			ms_DistanceToCamera = FLT_MAX; 
		}

		if(ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)
		{	
			//select the helper nearest the camera
			float distToCamera = MatrixPosition.Dist2(camInterface::GetPos()); 

			if(ms_SelectedAuthorHelper == NULL || distToCamera < ms_DistanceToCamera)
			{
				ms_SelectedAuthorHelper = this; 
				//ms_LastSelectedAuthorHelper = ms_SelectedAuthorHelper; 
				ms_DistanceToCamera = distToCamera;
				m_IsAxisSelected = true; 
				m_IsFirstUpdate = true; 
			}
		}

		if(ioMouse::GetButtons() & ioMouse::MOUSE_LEFT && m_IsAxisSelected)
		{
			if(ms_SelectedAuthorHelper == this)
			{
				ShouldUpdate = true; 
			}
		}
	}
	return ShouldUpdate; 
}

void CAuthoringHelper::RenderRotationHelper(const Matrix34& AuthoringMat, const Matrix34& updatedMat, float Scale)
{
#if DEBUG_DRAW
	Matrix34 renderedMat(updatedMat); 
	renderedMat.d = AuthoringMat.d; 
	grcDebugDraw::Axis(renderedMat, Scale, true);
	grcDebugDraw::GizmoRotation(MATRIX34_TO_MAT34V(AuthoringMat), Scale, m_CurrentInput);
#endif

#if 0
	if(ms_SelectedAuthorHelper == this || ms_SelectedAuthorHelper == NULL)
	{
		if(m_CurrentInput != ROT_Z_PLANE)
		{
			grcDebugDraw::Circle(VECTOR3_TO_VEC3V(AuthoringMat.d), Scale, Color_blue, VECTOR3_TO_VEC3V(AuthoringMat.a), VECTOR3_TO_VEC3V(AuthoringMat.b), false, false,1, 30);
		}
		else
		{

			grcDebugDraw::Circle(VECTOR3_TO_VEC3V(AuthoringMat.d), Scale, Color_white, VECTOR3_TO_VEC3V(AuthoringMat.a), VECTOR3_TO_VEC3V(AuthoringMat.b), false, false,1, 30);
		}

		if(m_CurrentInput != ROT_Y_PLANE)
		{
			grcDebugDraw::Circle(VECTOR3_TO_VEC3V(AuthoringMat.d), Scale, Color_green, VECTOR3_TO_VEC3V(AuthoringMat.c), VECTOR3_TO_VEC3V(AuthoringMat.a), false, false,1, 30);
		}
		else
		{
			grcDebugDraw::Circle(VECTOR3_TO_VEC3V(AuthoringMat.d), Scale, Color_white, VECTOR3_TO_VEC3V(AuthoringMat.c), VECTOR3_TO_VEC3V(AuthoringMat.a), false, false,1, 30);
		}

		if(m_CurrentInput != ROT_X_PLANE)
		{
			grcDebugDraw::Circle(VECTOR3_TO_VEC3V(AuthoringMat.d), Scale, Color_red, VECTOR3_TO_VEC3V(AuthoringMat.c), VECTOR3_TO_VEC3V(AuthoringMat.b), false, false, 1,30);
		}
		else
		{
			grcDebugDraw::Circle(VECTOR3_TO_VEC3V(AuthoringMat.d), Scale, Color_white, VECTOR3_TO_VEC3V(AuthoringMat.c), VECTOR3_TO_VEC3V(AuthoringMat.b), false, false, 1,30);
		}
	}
#endif
}
void CAuthoringHelper::UpdateRotationAxisSelection(const Matrix34& AuthoringMat, float Scale, Vector3& SelectionVec, Vector3&SelectionWorld)
{
	if(!m_IsAxisSelected)
	{
		spdSphere sphere = spdSphere(VECTOR3_TO_VEC3V(AuthoringMat.d), ScalarV(Scale));

		Vector3 mouseStart, mouseEnd;
		CDebugScene::GetMousePointing(mouseStart, mouseEnd, false);

		Vector3 w = mouseEnd - mouseStart;
		w.Normalize();

		Vector3 IntersectNear; 
		Vector3 IntersectFar; 
		Vector3 localNear(1.0f, 1.0f, 1.0f); 
		Vector3 localFar(1.0f, 1.0f, 1.0f); 
		bool isNearIntersection = false; 
		bool isFarIntersection = false; 

		TUNE_FLOAT(THIntersectTolerance, 0.08f, 0.0f, 1.0f, 0.001f)

			if(fwGeom::IntersectSphereRay(sphere, mouseStart, w, IntersectNear, IntersectFar))
			{
				grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(IntersectNear), 0.01f, Color_green);
				grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(IntersectFar), 0.01f, Color_red);

				//convert the sphere intersection locally to the matrix

				AuthoringMat.UnTransform(IntersectNear, localNear); 
				AuthoringMat.UnTransform(IntersectFar, localFar); 
			}

			Vector3 LocalNear(localNear);
			Vector3 LocalFar(localFar); 

			LocalNear.NormalizeSafe(); 
			LocalFar.NormalizeSafe(); 

			if(AreNearlyEqual(LocalNear.z, 0.0f, THIntersectTolerance) || AreNearlyEqual(LocalFar.z, 0.0f, THIntersectTolerance) )
			{
				m_CurrentInput = ROT_Z_PLANE; 

				if(AreNearlyEqual(LocalNear.z, 0.0f, THIntersectTolerance))
				{
					isNearIntersection = true; 
				}
				else
				{
					isFarIntersection = true; 
				}
			}
			else if(AreNearlyEqual(LocalNear.y, 0.0f, THIntersectTolerance) || AreNearlyEqual(LocalFar.y, 0.0f, THIntersectTolerance))
			{
				m_CurrentInput = ROT_Y_PLANE;

				if(AreNearlyEqual(LocalNear.y, 0.0f, THIntersectTolerance))
				{
					isNearIntersection = true; 
				}
				else
				{
					isFarIntersection = true; 
				}
			}
			else if(AreNearlyEqual(LocalNear.x, 0.0f, THIntersectTolerance) || AreNearlyEqual(LocalFar.x, 0.0f, THIntersectTolerance))
			{
				m_CurrentInput = ROT_X_PLANE; 

				if(AreNearlyEqual(LocalNear.x, 0.0f, THIntersectTolerance))
				{
					isNearIntersection = true; 
				}
				else
				{
					isFarIntersection = true; 
				}
			}
			else
			{
				m_CurrentInput = NO_INPUT; 
			}

			SelectionVec = localFar; 
			SelectionWorld = IntersectFar; 
			if(isNearIntersection)
			{
				SelectionVec = localNear;
				SelectionWorld = IntersectNear; 
			}
	}

};


bool CAuthoringHelper::UpdateRotationHelper(const Matrix34& mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat)
{
	UpdatedMat = mat; 

	Matrix34 AuthoringMat(mat); 
	AuthoringMat.d += Offset; 

	if(ms_AuthorMode == ROT_GBL)
	{
		AuthoringMat.Identity3x3(); 
	}

	Vector3 SelectionVec;
	Vector3 SelectionWorld; 

	UpdateRotationAxisSelection(AuthoringMat, Scale, SelectionVec, SelectionWorld); 

	bool ShouldUpdate = ShouldUpdateSelectedHelper(AuthoringMat.d); 

	if(ShouldUpdate)
	{
		UpdateAxisRotation(mat, UpdatedMat, SelectionVec, SelectionWorld);
	}

	RenderRotationHelper(AuthoringMat, UpdatedMat, Scale); 

	return ShouldUpdate;
}

void CAuthoringHelper::UpdateTranslationCollisionBoxes(const Matrix34& AuthoringMat, float Scale, const Vector3& UNUSED_PARAM(Offset) )
{
	if(!m_IsAxisSelected)
	{
		spdOrientedBB xBoundBox; 
		spdOrientedBB yBoundBox;
		spdOrientedBB zBoundBox; 
		spdOrientedBB xyPlaneBoundBox; 
		spdOrientedBB xzPlaneBoundBox; 
		spdOrientedBB zyPlaneBoundBox; 

		const Vector3 XYAxis = XAXIS + YAXIS; 
		const Vector3 XZAxis = XAXIS + ZAXIS; 
		const Vector3 ZYAxis = ZAXIS + YAXIS; 

		//Create the collision boxes
		CreateTranslationCollisionBox(AuthoringMat, XAXIS, Scale, VEC3_ZERO, m_AxisBox, xBoundBox); 
		CreateTranslationCollisionBox(AuthoringMat, YAXIS, Scale, VEC3_ZERO, m_AxisBox, yBoundBox); 
		CreateTranslationCollisionBox(AuthoringMat, ZAXIS, Scale, VEC3_ZERO, m_AxisBox, zBoundBox); 

		CreateTranslationCollisionBox(AuthoringMat, XYAxis, Scale, VEC3_ZERO, m_PlaneBox, xyPlaneBoundBox); 
		CreateTranslationCollisionBox(AuthoringMat, XZAxis, Scale, VEC3_ZERO, m_PlaneBox, xzPlaneBoundBox); 
		CreateTranslationCollisionBox(AuthoringMat, ZYAxis, Scale, VEC3_ZERO, m_PlaneBox, zyPlaneBoundBox); 

		//create the mouse probes
		spdOrientedBB MouseProbeBB; 
		CAuthoringHelper::UpdateMouseTranslationCollisionBox(MouseProbeBB); 
		Vector3 AxisDirectionMultiplyer = VEC3_ZERO; 

		if(xBoundBox.IntersectsOrientedBB(MouseProbeBB))
		{
			m_CurrentInput = X_AXIS;
		}
		else if(yBoundBox.IntersectsOrientedBB(MouseProbeBB))
		{
			m_CurrentInput = Y_AXIS;
		}
		else if(zBoundBox.IntersectsOrientedBB(MouseProbeBB))
		{
			m_CurrentInput = Z_AXIS; 
		}
		else if(xyPlaneBoundBox.IntersectsOrientedBB(MouseProbeBB))
		{
			m_CurrentInput = XY_PLANE; 
		}
		else if(xzPlaneBoundBox.IntersectsOrientedBB(MouseProbeBB))
		{
			m_CurrentInput = XZ_PLANE; 
		}
		else if(zyPlaneBoundBox.IntersectsOrientedBB(MouseProbeBB))
		{
			m_CurrentInput = ZY_PLANE;
		}
		else
		{
			m_CurrentInput = NO_INPUT; 
		}
	}
}

void CAuthoringHelper::UpdateTranslationPosition(const Matrix34& AuthoringMat, Matrix34& UpdatedMat)
{
	const float planeMax = 10.0f; 
	const float planeMin = -10.0f;
	const Vector3 XYAxis = XAXIS + YAXIS; 
	const Vector3 XZAxis = XAXIS + ZAXIS; 
	const Vector3 ZYAxis = ZAXIS + YAXIS; 

	switch (m_CurrentInput)
	{
	case X_AXIS:
		{
			Vector3 corner1(planeMin, 0.0f, planeMax); 
			Vector3 corner2(planeMax, 0.0f, planeMin); 
			Vector3 corner3(planeMin, 0.0f, planeMin); 

			AuthoringMat.Transform(corner1); 
			AuthoringMat.Transform(corner2); 
			AuthoringMat.Transform(corner3); 

			Vector4 PlaneDef; 

			PlaneDef.ComputePlane(corner1, corner2, corner3); 

			spdPlane Plane(PlaneDef); 

			UpdatedMat.d = UpdatedMat.d + UpdateTranslationAxis(AuthoringMat, XAXIS , Plane); 

		//	grcDebugDraw::Poly(corner1, corner2, corner3, Color_green, Color_green, Color_green, true); 

		}
		break; 

	case Y_AXIS:
		{
			Vector3 corner1(planeMin, planeMax, 0.0f); 
			Vector3 corner2(planeMax, planeMin, 0.0f); 
			Vector3 corner3(planeMin, planeMin, 0.0f); 

			AuthoringMat.Transform(corner1); 
			AuthoringMat.Transform(corner2); 
			AuthoringMat.Transform(corner3); 

			Vector4 PlaneDef; 

			PlaneDef.ComputePlane(corner1, corner2, corner3); 

			spdPlane Plane(PlaneDef); 

			UpdatedMat.d = UpdatedMat.d + UpdateTranslationAxis(AuthoringMat, YAXIS, Plane); 
		//	grcDebugDraw::Poly(corner1, corner2, corner3, Color_green, Color_green, Color_green, true); 

		}
		break;

	case Z_AXIS:
		{
			Vector3 corner1(planeMin, 0.0f, planeMax); 
			Vector3 corner2(planeMax, 0.0f, planeMin); 
			Vector3 corner3(planeMin, 0.0f, planeMin); 

			AuthoringMat.Transform(corner1); 
			AuthoringMat.Transform(corner2); 
			AuthoringMat.Transform(corner3); 

			Vector4 PlaneDef; 

			PlaneDef.ComputePlane(corner1, corner2, corner3); 

			spdPlane Plane(PlaneDef); 

			UpdatedMat.d = UpdatedMat.d + UpdateTranslationAxis(AuthoringMat, ZAXIS, Plane); 

			//grcDebugDraw::Poly(corner1, corner2, corner3, Color_green, Color_green, Color_green, true); 
		}
		break;

	case XY_PLANE:
		{
			Vector3 corner1(planeMin, planeMax, 0.0f); 
			Vector3 corner2(planeMax, planeMin, 0.0f); 
			Vector3 corner3(planeMin, planeMin, 0.0f); 


			AuthoringMat.Transform(corner1); 
			AuthoringMat.Transform(corner2); 
			AuthoringMat.Transform(corner3); 

			Vector4 PlaneDef; 

			PlaneDef.ComputePlane(corner1, corner2, corner3); 
			spdPlane Plane(PlaneDef); 

			UpdatedMat.d = UpdatedMat.d + UpdateTranslationAxis(AuthoringMat, XYAxis, Plane); 
		}
		break;

	case XZ_PLANE:
		{
			Vector3 corner1(planeMin, 0.0f, planeMax); 
			Vector3 corner2(planeMax, 0.0f, planeMin); 
			Vector3 corner3(planeMin, 0.0f, planeMin); 


			AuthoringMat.Transform(corner1); 
			AuthoringMat.Transform(corner2); 
			AuthoringMat.Transform(corner3); 

			Vector4 PlaneDef; 

			PlaneDef.ComputePlane(corner1, corner2, corner3); 
			spdPlane Plane(PlaneDef); 

			UpdatedMat.d = UpdatedMat.d + UpdateTranslationAxis(AuthoringMat, XZAxis, Plane); 
		}
		break;

	case ZY_PLANE:
		{
			Vector3 corner1(0.0f, planeMin, planeMax); 
			Vector3 corner2(0.0f, planeMax, planeMin); 
			Vector3 corner3(0.0f, planeMin, planeMin); 


			AuthoringMat.Transform(corner1); 
			AuthoringMat.Transform(corner2); 
			AuthoringMat.Transform(corner3); 

			Vector4 PlaneDef; 

			PlaneDef.ComputePlane(corner1, corner2, corner3); 
			spdPlane Plane(PlaneDef); 

			UpdatedMat.d = UpdatedMat.d + UpdateTranslationAxis(AuthoringMat, ZYAxis, Plane); 
		}
		break;
	}
}

bool CAuthoringHelper::UpdateTranslationHelper(const Matrix34& mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat)
{
	UpdatedMat = mat; 

	Matrix34 AuthoringMat(mat); 

	AuthoringMat.d += Offset; 

	if(ms_AuthorMode == TRANS_GBL)
	{
		AuthoringMat.Identity3x3(); 
	}

	UpdateTranslationCollisionBoxes(AuthoringMat, Scale, Offset); 

	bool ShouldUpdate = ShouldUpdateSelectedHelper(AuthoringMat.d); 

	if(ShouldUpdate)
	{
		UpdateTranslationPosition(AuthoringMat, UpdatedMat); 
	}

	RenderTranslationHelper(AuthoringMat, Scale, Offset); 

	return ShouldUpdate; 
}
#endif
