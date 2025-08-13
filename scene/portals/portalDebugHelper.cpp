////////////////////////////////////////////////////////////////////////////////////
// Title	:	PortalDebugHelper.cpp
// Author	:	John
// Started	:	10/2/2010
//			:	Portal debugging code
////////////////////////////////////////////////////////////////////////////////////


#include "PortalDebugHelper.h"

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "grcore/debugdraw.h"

//framework
#include "fwscene/scan/ScanDebug.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "core/game.h"
#include "modelInfo/MLOmodelInfo.h"
#include "peds/ped.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "debug/GtaPicker.h"
#include "scene/scene_channel.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalTracker.h"
#include "Scene/World/GameWorld.h"
#include "scene/LoadScene.h"
#include "scene/loader/MapData.h"
#include "scene/lod/LodMgr.h"
#include "streaming/streaming.h"

RENDER_OPTIMISATIONS()


#if __BANK
// debug widgetty stuff
#define WIDGET_NAME_LENGTH (24)

#define NAME_LENGTH 64

// MLO dropdown stuff
static atArray<const char*> mloNames;

static char ms_currIntInfo[RAGE_MAX_PATH] = { 0 };

atArray<CInteriorProxy*> proxyPtrs;
char selectedMLOBound[NAME_LENGTH] = "(0.00, 0.00, 0.00, 0.00)";
char selectedMLOImap[NAME_LENGTH] = " None selected ";
char selectedMLOLOD[NAME_LENGTH] = " None ";
char selectedMLOPos[RAGE_MAX_PATH] = " None ";
char selectedMLORot[RAGE_MAX_PATH] = " None ";
char selectedMLOCurrentState[NAME_LENGTH] = " N/A ";

static bkCombo*	pMloCombo = NULL;
static bkCombo*	pMloCombo2 = NULL;
static bkToggle* pCapAlphaEntities = NULL;
static bkToggle* pShortFadeDist = NULL;
static bkToggle* pTurnGPSOn = NULL;
static s32	currMloNameIdx = 0;

// entity set stuff
char activeSetHeader[NAME_LENGTH];
char inactiveSetHeader[NAME_LENGTH];
static atArray<const char*> ActiveEntitySets;
static atArray<const char*> InactiveEntitySets;
static s32 currActiveEntitySetIdx = 0;
static s32 currInactiveEntitySetIdx = 0;
static bkCombo* pActiveEntitySetCombo = NULL;
static bkCombo* pInactiveEntitySetCombo = NULL;


roomDebugData	roomsData[32];
s32	debugRoomIdx = 0;

static bool gbDebugEntitiesInVolume = false;
static bool gbDebugIncludePedsInVolume = true;
static bool gbDebugIncludeVehiclesInVolume = true;
static Vector3 g_minTestBox(0.0f, 0.0f, 0.0f);
static Vector3 g_maxTestBox(0.0f, 0.0f, 0.0f);

static bool bRenderObjects = true;
static bool bRenderObjPts = false;
static bool bRenderPortals = false;
bool bDrawOnlyCurrentInterior = false;
static bool bShowMLOBounds = false;
static bool bShowTunnelMLOBounds = false;
static bool bShowTunnelMLOBox = false;
static bool bShowTunnelMLOOrientation = false;
static bool bShowCamMLOBounds = false;
static bool bDisableSelectedMLO = false;
static bool bCapPartialSelectedMLO = false;

bool	bTrackCamera = false;
bool	bCapAlphaOfContents = false;
bool	bShortFadeDistance = false;
bool	bTurnOnGPS = false;	
bool	bShowPortalInstances = false;
bool	bShowPortalTrackerSafeZoneSpheres = false;
bool	bShowPortalTrackerSafeZoneSphereForSelected = false;
bool	bBreakWhenSelectedPortalTrackerSafeSphereRadiusIsNearZero = false;
bool	bSafeZoneSphereBoundingSphereMethod = true;
bool	bVisualiseAllTrackers = false;
bool	bVisualiseCutsceneTrackers = false;
bool	bVisualiseActivatingTrackers = false;
bool	bVisualiseObjectNames = false;
bool	bCheckConsistency = false;
bool	bDontPerformPtUpdate = false;
bool	bPtUpdateDebugSpew = false;
bool    bPtEnableDrawingOfCollectedCallstack = false;
bool bObjPointClip = false;
bool bDisablePortalStream = false;
bool bDumpMLODebugData = false;
bool bPrintfMLOPool = false;
bool bDumpActiveInteriorList = false;
bool bFilterShowAll = true;
bool bFilterPortalNotInstanced = false;
bool bFilterNotRendered = false;
bool bFilterNotInUse = false;
bool bFilterLod1Only = false;
bool bFilterDetailOnly = false;
bool bFilterPopulated = false;
bool bFilterState = false;
bool bFilterRetaining = false;
bool bFilterPickerList = false;
s32 groupFilter = -1;
s32 IndexFilter = -1;

bool bDisplayMemUse = false;
bool bDisplayRoomMemUse = false;
bool bDisplayLodDetail = false;
s32 roomIdxMemDetailDebug = -1;
CInteriorInst* pCurrDebugInterior = NULL;

bool bOverrideCurrentMLOAlpha = false;
s32 currentMLOAlphaVal = 255;

s32 filterMLOLODLevel = -1;
s32 forceMLOLODLevel = -1;
bool bShowPortalRenderStats =false;
void ChangeRoomIdxCB(void);

bool g_enableRecursiveCheck = false;
bool g_breakOnRecursiveFail = false;
bool g_recursiveCheckAddToPicker = false;
bool g_pauseGameOnRecursiveFail = false;


int g_bruteForceMoveInteriorProxyIndex = -1;
int g_bruteForceMoveRoomIndex = -1;
void BruteForceMoveCallback(void);
void BruteForceRetainListCallback(void);
bool g_ignoreSmallDeltaEarlyOut = false;

// portal rendering stats
s32 numMLOsInScene = 0;
s32 numLowLODRenders = 0;
s32 numHighLODRenders = 0;
s32 numIntModelsRendered = 0;

s32 numCurrIntModelsRendered = 0;
s32 numCurrIntTrackers = 0;
s32 numCurrIntRooms = 0;
s32 numCurrIntModels = 0;

CInteriorInst* pCurrInterior = NULL;

// names of interior states & modules which can request those state (for debugging)
static const char* interiorStates[CInteriorProxy::PS_NUM_STATES] = {"    NONE   ","  PARTIAL  ","   FULL    ","FULL+MODELS"};
static const char* requestingModules[CInteriorProxy::RM_NUM_MODULES] = {"    NONE    ", "VISIBLE ", "  PT_PROX  ","   GROUP    ","   SCRIPT   "," CUTSCENE ", 
																		"LOADSCENE", "   TUNNEL   "};

// Rage controls
namespace rage {
extern bool	bShowTCScreenSpacePortals;
extern float fDebugRenderScale;
};

#endif //__BANK

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :  Initialises everything at the start of game.
/////////////////////////////////////////////////////////////////////////////////

void CPortalDebugHelper::Init(unsigned /*initMode*/)
{
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shutdown
// PURPOSE :  Frees everything on game shutdown.
/////////////////////////////////////////////////////////////////////////////////

void CPortalDebugHelper::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
#if __BANK
	    mloNames.Reset();
		ActiveEntitySets.Reset();
		InactiveEntitySets.Reset();
#endif //__BANK
    }
}

#if __BANK

void DisplayInteriorArrayContents(atArray<u32>& interiorArray)
{
	s32 totalMemory = 0;
	s32 drawableMemory = 0;
	s32 textureMemory = 0;
	s32 boundsMemory = 0;
	atArray<u32> tempArray;
	for(s32 j=0;j<interiorArray.GetCount();j++){

		tempArray.Reset();
		tempArray.PushAndGrow(interiorArray[j]);
		CStreamingDebug::GetMemoryStats(tempArray, totalMemory, drawableMemory, textureMemory, boundsMemory);

		strLocalIndex txd = strLocalIndex(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(interiorArray[j])))->GetAssetParentTxdIndex());
		if(txd != -1)
			grcDebugDraw::AddDebugOutput("   %s : model %dK, txd %s %dK", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(interiorArray[j]))), drawableMemory>>10, 
			g_TxdStore.GetName(txd), textureMemory>>10);
		else
			grcDebugDraw::AddDebugOutput("   %s : model %dK", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(interiorArray[j]))), drawableMemory>>10);
	}

}

void CPortalDebugHelper::DisplayMemUse(){

	grcDebugDraw::AddDebugOutput(" ");
	grcDebugDraw::AddDebugOutput("Memory Use for MLOs");
	grcDebugDraw::AddDebugOutput("-------------------");

	if (pCurrDebugInterior){

		atArray<u32> modelIdxArray;
		s32 totalMemory = 0;
		s32 drawableMemory = 0;
		s32 textureMemory = 0;
		s32 boundsMemory = 0;

		if(bDisplayLodDetail)
			DisplayInteriorArrayContents(modelIdxArray);

		// portals
		modelIdxArray.Reset();
		pCurrDebugInterior->AddInteriorContents(-1, false, true, modelIdxArray); //get portals
		CStreamingDebug::GetMemoryStats(modelIdxArray, totalMemory, drawableMemory, textureMemory, boundsMemory);
		// display memory use for this interior
		grcDebugDraw::AddDebugOutput("portal num %d, objects mem : total %dK, model %dK , txd %dK, bounds %dk", modelIdxArray.GetCount(), totalMemory>>10, drawableMemory>>10, textureMemory>>10, boundsMemory>>10);

		// display portal objects
		if(bDisplayLodDetail)
			DisplayInteriorArrayContents(modelIdxArray);

		// rooms
		modelIdxArray.Reset();
		pCurrDebugInterior->AddInteriorContents(-1, true, false, modelIdxArray); //get portals
		CStreamingDebug::GetMemoryStats(modelIdxArray, totalMemory, drawableMemory, textureMemory, boundsMemory);
		// display memory use for this interior
		grcDebugDraw::AddDebugOutput("room num %d, objects mem : total %dK, model %dK , txd %dK, bounds %dk", modelIdxArray.GetCount(), totalMemory>>10, drawableMemory>>10, textureMemory>>10, boundsMemory>>10);

		if (bDisplayRoomMemUse){
			// break down for each room
			grcDebugDraw::AddDebugOutput(" ");
			grcDebugDraw::AddDebugOutput("Memory Use by room");

			u32 numRooms = pCurrDebugInterior->GetNumRooms();
			for(u32 i=0;i<numRooms;i++){

				grcDebugDraw::AddDebugOutput("Room %d: %s", i, pCurrDebugInterior->GetRoomName(i));

				// rooms
				modelIdxArray.Reset();
				pCurrDebugInterior->AddInteriorContents(i, false, false, modelIdxArray); //get portals
				CStreamingDebug::GetMemoryStats(modelIdxArray, totalMemory, drawableMemory, textureMemory, boundsMemory);
				// display memory use for this interior
				grcDebugDraw::AddDebugOutput("objects num %d, mem : total %dK, model %dK , txd %dK, bounds %dk", modelIdxArray.GetCount(), totalMemory>>10, drawableMemory>>10, textureMemory>>10, boundsMemory>>10);
			}
		}

		if (roomIdxMemDetailDebug >= (s32)pCurrDebugInterior->GetNumRooms()){
			roomIdxMemDetailDebug = pCurrDebugInterior->GetNumRooms()-1;
		}

		if (roomIdxMemDetailDebug != -1){
			grcDebugDraw::AddDebugOutput(" ");
			// break down by object for the selected room
			grcDebugDraw::AddDebugOutput("Detail breakdown Room %d", roomIdxMemDetailDebug);
			modelIdxArray.Reset();
			pCurrDebugInterior->AddInteriorContents(roomIdxMemDetailDebug, false, false, modelIdxArray); //get portals
			
			DisplayInteriorArrayContents(modelIdxArray);
		}
	} else {
		grcDebugDraw::AddDebugOutput("No current interior to debug!");
	}

	grcDebugDraw::AddDebugOutput(" ");
}


void CPortalDebugHelper::DumpMLODebugData(void)
{
	grcDebugDraw::AddDebugOutput("MLO debug data");
	grcDebugDraw::AddDebugOutput("--------------");
	
	if (bPrintfMLOPool)
	{
		Displayf("MLO debug data");
		Displayf("--------------");
	}

	// scan through the interior instance list over a number of frames & dummify entries
	
	// dump names of all instanced MLOs in pool
	grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), " - Filtered instanced MLO list (%d entries) - ", CInteriorInst::GetPool()->GetNoOfUsedSpaces());
	if (bPrintfMLOPool)
	{
		Displayf(" - Filtered instanced MLO list (%d entries) - ", CInteriorInst::GetPool()->GetNoOfUsedSpaces());
	}
	CInteriorInst* pIntInst = NULL;
	s32 poolSize=CInteriorInst::GetPool()->GetSize();
	while(poolSize-- > 0) {
		pIntInst = CInteriorInst::GetPool()->GetSlot(poolSize);

		if (pIntInst){

			// filters
			if (bFilterShowAll)																								{ goto pass_filter; }
			if (bFilterPortalNotInstanced && pIntInst->m_bIsPopulated == true)												{ goto pass_filter; }
			if (bFilterNotInUse && (pIntInst->m_bInUse))																	{ goto pass_filter; }
			if (bFilterPopulated && (pIntInst->m_bIsPopulated))																{ goto pass_filter; }
			if (bFilterState && pIntInst->GetProxy()->GetCurrentState() > CInteriorProxy::PS_NONE)							{ goto pass_filter; }
			if (bFilterRetaining && (pIntInst->GetRetainListCount() > 0))													{ goto pass_filter; }
			if (bFilterPickerList && (g_PickerManager.GetIndexOfEntityWithinResultsArray(pIntInst) > -1))					{ goto pass_filter; }
			if ((groupFilter > -1) && ((s32)(pIntInst->GetGroupId()) == groupFilter))										{ goto pass_filter; }
			if ((IndexFilter > -1) && (CInteriorProxy::GetPool()->GetJustIndex(pIntInst->GetProxy()) == IndexFilter))		{ goto pass_filter; }

			continue;

pass_filter:
			char enableString[100];

			if (pIntInst->GetProxy()->GetIsDisabled())
			{
				sprintf(enableString, "<Disabled>");
			} 
			else if (pIntInst->GetProxy()->GetIsCappedAtPartial())
			{
				sprintf(enableString,"<Capped>");
			}
			else
			{
				sprintf(enableString,"<Active>");
			}

			char LodString[250];
			pIntInst->GetLODState(LodString);
		
			float popRadius = pIntInst->GetProxy()->GetPopulationDistance(false);
			char stateString[100];
			sprintf(stateString, "<State: (%s,%s) (ret:%d)> <Grp:%d> <pop:%.0fm> <Closed:%s>", interiorStates[pIntInst->GetProxy()->GetCurrentState()], 
																							   requestingModules[pIntInst->GetProxy()->GetRequestingModule()], 
																							   pIntInst->GetRetainListCount(), 
																							   pIntInst->GetGroupId(), 
																							   popRadius, 
																							   pIntInst->m_bAddedToClosedInteriorList ? "Yes" : "No" );
// 			grcDebugDraw::AddDebugOutput("%03d: (%s,%s) : %s : %s : (retains:%d) %s", (CInteriorInst::GetPool())->GetJustIndex(pIntInst), 
// 				interiorStates[pIntInst->GetCurrentState()], requestingModules[pIntInst->GetRequestingModule()], 
// 				pIntInst->GetModelName(),  LodString, pIntInst->GetRetainListCount(), physString);
		
			grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "%03d: %-20s %-60s", 
				(CInteriorProxy::GetPool())->GetJustIndex(pIntInst->GetProxy()), 
				pIntInst->GetModelName(),  
				enableString );
			grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "                          %-60s", 
				stateString); 
			grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "                          %-60s", 
				LodString);
			grcDebugDraw::AddDebugOutputEx(false, Color32(Color_white), "      %-80s", " ");

			if (bPrintfMLOPool)
			{
				Displayf("%03d: %-20s %-60s", (CInteriorProxy::GetPool())->GetJustIndex(pIntInst->GetProxy()),  pIntInst->GetModelName(),  enableString );
				Displayf("                          %-60s", stateString); 
				Displayf("                          %-60s", LodString);
				Displayf("      %-80s", " ");
			}

		}
	}
}
#endif //__BANK

#if __BANK
void CPortalDebugHelper::DumpActiveInteriorList(void){
	grcDebugDraw::AddDebugOutput("--- Active interiors ---");
	fwPtrNode* pNode = CPortal::ms_activeInteriorList.GetHeadPtr();

	while(pNode)
	{
		CInteriorInst* pIntInst = (CInteriorInst*)pNode->GetPtr();
		char LodString[250];
		pIntInst->GetLODState(LodString);
		grcDebugDraw::AddDebugOutput("%03d: %s %s",(CInteriorInst::GetPool())->GetJustIndex(pIntInst),pIntInst->GetModelName(), LodString);

		pNode = pNode->GetNextPtr();
	}
}
#endif //__BANK

//***************************** Widget stuff**************************
//--------------Widget stuff-------------
#if __BANK
// display the extents of the room.
void CPortalDebugHelper::DebugRoom(CInteriorInst* pInterior, u32 currRoomIdx)
{
	Assert(currRoomIdx < MAX_ROOMS_PER_INTERIOR);

	spdAABB bbox;
	pInterior->CalcRoomBoundBox(currRoomIdx, bbox);

	// draw room extents in correct colour for this room
	u8 r = roomsData[currRoomIdx].red;
	u8 g = roomsData[currRoomIdx].green;
	u8 b = roomsData[currRoomIdx].blue;
	Color32	iCol(r,g,b, 75);

	Vector3 tempPoint1 = bbox.GetMinVector3();
	tempPoint1.y = bbox.GetMaxVector3().y;
	Vector3 tempPoint3 = bbox.GetMinVector3();
	tempPoint3.x = bbox.GetMaxVector3().x;
	Vector3 tempPoint2 = bbox.GetMinVector3();
	tempPoint2.x = bbox.GetMaxVector3().x;
	tempPoint2.y = bbox.GetMaxVector3().y;

	grcDebugDraw::Poly(bbox.GetMin(), RCC_VEC3V(tempPoint2), RCC_VEC3V(tempPoint1), iCol);
	grcDebugDraw::Poly(RCC_VEC3V(tempPoint2), bbox.GetMin(), RCC_VEC3V(tempPoint3), iCol);

	grcDebugDraw::BoxAxisAligned(bbox.GetMin(),bbox.GetMax(),iCol);

	Vector3 centroid = VEC3V_TO_VECTOR3(bbox.GetCenter());
	centroid.z = bbox.GetMinVector3().z - 0.5f;

	char* pRoomName = const_cast<char*>(pInterior->GetRoomName(currRoomIdx));
	grcDebugDraw::Text(centroid, iCol, pRoomName);
}

// show a marker for a position inside a room
void CPortalDebugHelper::DebugPosition(const Vector3& pos, u32 UNUSED_PARAM(fade), u32 roomIdx){
	Vector3 vecPosition = pos;

	if ((roomsData[roomIdx].bRenderObjPts)){

		u8 r = roomsData[roomIdx].red;
		u8 g = roomsData[roomIdx].green;
		u8 b = roomsData[roomIdx].blue;

		//	Color32 iCol((r - fade), (g - fade),(b - fade), 255);
		Color32	iCol(r,g,b);

		grcDebugDraw::Line(vecPosition + Vector3( -0.2f, 0.0f, -0.2f), vecPosition + Vector3( +0.2f, 0.0f, +0.2f), iCol);
		grcDebugDraw::Line(vecPosition + Vector3( +0.2f, 0.0f, -0.2f), vecPosition + Vector3( -0.2f, 0.0f, +0.2f), iCol);
		grcDebugDraw::Line(vecPosition + Vector3(  0.0f, 0.2f,  0.0f), vecPosition + Vector3(  0.0f,-0.2f,  0.0f), iCol);
	}
}

void ChangeRoomIdxCB(void)
{
	bRenderObjects = roomsData[debugRoomIdx].bRenderObjects;
	bRenderObjPts = roomsData[debugRoomIdx].bRenderObjPts;
	bRenderPortals = roomsData[debugRoomIdx].bRenderPortals;
}

static void UpdateValuesCB(void)
{
	roomsData[debugRoomIdx].bRenderObjects = bRenderObjects;
	roomsData[debugRoomIdx].bRenderObjPts = bRenderObjPts;
	roomsData[debugRoomIdx].bRenderPortals = bRenderPortals;
}

static void ResetDataCB(void)
{
	debugRoomIdx = 0;
	for(u32 i=0; i<32; i++){
		if ((i%6) == 0) {
			roomsData[i].red = 255; roomsData[i].green = roomsData[i].blue = 0;
		} else if ((i%6) == 1) {
			roomsData[i].green = 255; roomsData[i].red = roomsData[i].blue = 0;
		} else if ((i%6) == 2) {
			roomsData[i].blue = 255; roomsData[i].red = roomsData[i].green = 0;
		} else 	if ((i%6) == 3) {
			roomsData[i].red = 0; roomsData[i].green = roomsData[i].blue = 255;
		} else if ((i%6) == 4) {
			roomsData[i].green = 0; roomsData[i].red = roomsData[i].blue = 255;
		} else if ((i%6) == 5) {
			roomsData[i].blue = 0; roomsData[i].red = roomsData[i].green = 255;
		} 

		roomsData[i].bRenderObjects = bRenderObjects;
		roomsData[i].bRenderObjPts = bRenderObjPts;
		roomsData[i].bRenderPortals = bRenderPortals;
	}

	debugRoomIdx = 0;

	ChangeRoomIdxCB();
}

// ped ID has changed, so load it in and scan txd for available variations
void MloIDChangeCB(void)
{
	CInteriorProxy* pProxy = proxyPtrs[currMloNameIdx];
	
	spdSphere boundSphere;
	pProxy->GetBoundSphere(boundSphere);

	Vec3V vPos;
	QuatV vRot;

	pProxy->GetPosition( vPos );
	pProxy->GetQuaternion( vRot );

	sprintf(selectedMLOImap, "%s", pProxy->GetContainerName());
	sprintf(selectedMLOBound, "( %.0f, %.0f, %.0f )    [ %.2f ]",boundSphere.GetCenter().GetXf(), boundSphere.GetCenter().GetYf(), boundSphere.GetCenter().GetZf(), boundSphere.GetRadiusf());
	sprintf( selectedMLOPos, "( %f, %f, %f )", vPos.GetXf(), vPos.GetYf(), vPos.GetZf() );
	sprintf( selectedMLORot, "( %f, %f, %f, %f )", vRot.GetXf(), vRot.GetYf(), vRot.GetZf(), vRot.GetWf() );
	sprintf(selectedMLOCurrentState, "Disabled: %s      Capped:%s",(pProxy->GetIsDisabled() ? "Yes":"No"), (pProxy->GetIsCappedAtPartial() ? "Yes":"No"));

	bDisableSelectedMLO = pProxy->GetIsDisabled();
	bCapPartialSelectedMLO = pProxy->GetIsCappedAtPartial();
}

void SelectFirstMlo()
{
	currMloNameIdx = 0;
	MloIDChangeCB();
}

bool SelectNextMlo()
{
	currMloNameIdx++;

	if(currMloNameIdx >= mloNames.GetCount())
	{
		currMloNameIdx = 0;
		MloIDChangeCB();
		return false;
	}
	MloIDChangeCB();
	return true;
}

float	timeOut = 0;
Vector3	targetPos;
bool	bLoadSceneActive = false;
bool	bMLOListActive = false;

void WarpCameraToMLOCB(void) {

 	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	debugDirector.ActivateFreeCam();								//Turn on debug free cam.

	Vec3V Pos;
	proxyPtrs[currMloNameIdx]->GetPosition(Pos);
	targetPos = VEC3V_TO_VECTOR3(Pos);

 	debugDirector.GetFreeCamFrameNonConst().SetPosition(targetPos);	//Move free camera to desired place.

}

void DisplayCurrentInteriorInfo()
{
	memset(ms_currIntInfo, 0, sizeof(ms_currIntInfo));

	if (CPed* pPlayer = CGameWorld::FindFollowPlayer())
	{
		if (CInteriorInst* pPlayerIntInst = pPlayer->GetPortalTracker()->GetInteriorInst())
		{
			if (CInteriorProxy* pProxy = pPlayerIntInst->GetProxy())
			{
				formatf(ms_currIntInfo, "%s", INSTANCE_STORE.GetName(pProxy->GetMapDataSlotIndex()));
			}
		}
	}
}

void NuLoadSceneToMLOCB(void){
	
	if (!bMLOListActive)
		return;

	bLoadSceneActive = false;

	Vec3V	lookAt;
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer){
		lookAt = pPlayer->GetTransform().GetForward();
		sceneDisplayf("+++ player warping from: (%2.2f,%2.2f,%2.2f)", CGameWorld::FindLocalPlayerCoors().GetX(), CGameWorld::FindLocalPlayerCoors().GetY(),
			CGameWorld::FindLocalPlayerCoors().GetZ());
	}

	if (proxyPtrs[currMloNameIdx])
	{
		Vec3V Pos;
		proxyPtrs[currMloNameIdx]->GetPosition(Pos);
		targetPos = VEC3V_TO_VECTOR3(Pos);
		targetPos += Vector3(0.0f, 0.0f, 1.5f);
		bLoadSceneActive = true;
		g_LoadScene.Start(Pos, lookAt, 10.0f, false, 0, CLoadScene::LOADSCENE_OWNER_DEBUG);
		timeOut = TIME.GetSeconds() + 10.0f;
	}
}

void DumpMLOToFileCB()
{
	char const* pFilename = "X:\\MLO.txt";
	fiStream* logStream = ASSET.Create(pFilename, "");

	if (!logStream)
	{
		Errorf("Could not create '%s'", pFilename);
		return;
	}

	for(char const* p : mloNames)
	{
		logStream->Write(p, (int)strlen(p));
		logStream->Write("\n", 1);
	}
	logStream->Close();

}

void CPortalDebugHelper::Update(void){

	// update the interior picker widget
	CInteriorInst* pPreviousCurrDebugInterior = pCurrDebugInterior;

	if (g_PickerManager.IsCurrentOwner("Interior picker")){
		pCurrDebugInterior = NULL;
		fwEntity* pPickedEntity = g_PickerManager.GetSelectedEntity();
		if (pPickedEntity){
			if (pPickedEntity->GetType() == ENTITY_TYPE_MLO)
			{
				pCurrDebugInterior = static_cast<CInteriorInst*>(pPickedEntity);
				if (pCurrDebugInterior != pPreviousCurrDebugInterior) 
				{
					UpdateEntitySetListCB();
				}
			}
		}  else {
			if (pPreviousCurrDebugInterior != NULL)
			{
				UpdateEntitySetListCB();
			}
		}
	} else if (g_PickerManager.IsCurrentOwner("Interior via contents"))
	{
		pCurrDebugInterior = NULL;
		CEntity* pPickedEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());

		if (pPickedEntity)
		{
			if (pPickedEntity->GetType() == ENTITY_TYPE_MLO)
			{
				// read first
				bCapAlphaOfContents = ((static_cast<CInteriorInst*>(pPickedEntity)->GetMLOFlags() & INSTANCE_MLO_CAP_CONTENTS_ALPHA) != 0);
				bTurnOnGPS = ((static_cast<CInteriorInst*>(pPickedEntity)->GetMLOFlags() & INSTANCE_MLO_GPS_ON) != 0);
				bShortFadeDistance = ((static_cast<CInteriorInst*>(pPickedEntity)->GetMLOFlags() & INSTANCE_MLO_SHORT_FADE) != 0);

				// then set
				pCapAlphaEntities->SetBool(bCapAlphaOfContents);
				pTurnGPSOn->SetBool(bTurnOnGPS);
				pShortFadeDist->SetBool(bShortFadeDistance);

				if (bOverrideCurrentMLOAlpha)
				{
					pPickedEntity->SetAlpha(currentMLOAlphaVal);
				}
			}
			else if (pPickedEntity && pPickedEntity->GetIsInInterior())
			{
				fwInteriorLocation loc = pPickedEntity->GetInteriorLocation();
				pCurrDebugInterior = CInteriorInst::GetInteriorForLocation(loc);
				g_PickerManager.AddEntityToPickerResults(pCurrDebugInterior, true, true);
			} 
			else
			{
				g_PickerManager.AddEntityToPickerResults(NULL, true, false);
			}
		}
		else
		{
			pCapAlphaEntities->SetBool(false);
			pTurnGPSOn->SetBool(false);
			pShortFadeDist->SetBool(false);
		}
	}


	// update the proxy displays
	if (currMloNameIdx < proxyPtrs.GetCount())
	{
		CInteriorProxy* pProxy = proxyPtrs[currMloNameIdx];

		sprintf(selectedMLOLOD, " None ");
		if (pProxy->GetInteriorInst()){
			fwEntity* pEntity = pProxy->GetInteriorInst()->GetLod();
			if (pEntity)
			{
				sprintf(selectedMLOLOD, " %s     (alpha : %d) ", pEntity->GetModelName(), pEntity->GetAlpha());
			}
		} 
	}

	// debug loadscene update
	if (!bLoadSceneActive){
		return;
	}

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	float currentTime = TIME.GetSeconds();
	// is NuLoadScene() ready yet, or have we timed out
	if (!g_LoadScene.IsLoaded() && (currentTime < timeOut)){
		pPlayer->SetPosition(targetPos);
		return;
	}

	// well, just warp the player there anyway
	if (pPlayer){
		pPlayer->Teleport(targetPos, pPlayer->GetCurrentHeading(), false);
		g_LoadScene.Stop();
		bLoadSceneActive = false;
	}
}

int CompareProxy_Names(CInteriorProxy* const* lhs, CInteriorProxy* const* rhs)
{
	return stricmp((*rhs)->GetModelName(), (*lhs)->GetModelName());
}

void RefreshInteriorPickerCB(void) {

	if (g_PickerManager.IsEnabled())
	{
		CInteriorInst* pInterior = NULL;
		s32 poolSize = CInteriorInst::GetPool()->GetSize();
		s32 base = 0;

		while (base < poolSize)
		{
			pInterior = CInteriorInst::GetPool()->GetSlot(base);
			if (pInterior && (pInterior->GetProxy()->GetCurrentState() != CInteriorProxy::PS_NONE))
			{
				bool selected = false;
				if (pInterior == pCurrDebugInterior)
					selected = true;
				g_PickerManager.AddEntityToPickerResults(pInterior, false, selected);
			}
			base++;
		}
	}
}

void ActivateInteriorPickerCB(void){

	fwPickerManagerSettings RayfirePickerManagerSettings(INTERSECTING_ENTITY_PICKER, true, false, 0, false);		// no mask -> no picking!
	g_PickerManager.SetPickerManagerSettings(&RayfirePickerManagerSettings);

	g_PickerManager.SetEnabled(true);
	g_PickerManager.TakeControlOfPickerWidget("Interior picker");
	g_PickerManager.ResetList(false);

	CInteriorInst* pInterior = NULL;
	s32 poolSize=CInteriorInst::GetPool()->GetSize();
	s32 base = 0;

	while(base < poolSize)
	{
		pInterior = CInteriorInst::GetPool()->GetSlot(base);
		if(pInterior && (pInterior->GetProxy()->GetCurrentState() != CInteriorProxy::PS_NONE))
		{
			g_PickerManager.AddEntityToPickerResults(pInterior, false, false);
		}	
		base++;
	}
}

void ActivateContentsPickerCB(void)
{
	// Enable picker when it changes
	if (g_PickerManager.IsEnabled() == false)
	{
		{
			g_PickerManager.TakeControlOfPickerWidget("Interior via contents");

			fwPickerManagerSettings pickerSettings(ENTITY_RENDER_PICKER, true, true, 
				ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT, false);
			g_PickerManager.SetPickerManagerSettings(&pickerSettings);

			g_PickerManager.ResetList(false);
		}

		g_PickerManager.SetEnabled(true);
	} else {
		g_PickerManager.SetEnabled(false);
	}
}

void UpdateFlagsCB(void)
{
	CEntity* pPickedEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pPickedEntity && pPickedEntity->GetIsTypeMLO())
	{
		CInteriorInst* pIntInst = static_cast<CInteriorInst*>(pPickedEntity);

		u32 newFlags = pIntInst->GetMLOFlags();

		if (bCapAlphaOfContents)
		{
			newFlags = newFlags | INSTANCE_MLO_CAP_CONTENTS_ALPHA;
		}
		else
		{
			newFlags = newFlags & (~INSTANCE_MLO_CAP_CONTENTS_ALPHA);
		}

		if (bTurnOnGPS)
		{
			newFlags = newFlags | INSTANCE_MLO_GPS_ON;
		}
		else
		{
			newFlags = newFlags & (~INSTANCE_MLO_GPS_ON);
		}

		if (bShortFadeDistance)
		{
			newFlags = newFlags | INSTANCE_MLO_SHORT_FADE;
		}
		else
		{
			newFlags = newFlags & (~INSTANCE_MLO_SHORT_FADE);
		}

		pIntInst->SetMLOFlags(newFlags);
	}
}
////////////////////////////////////////////////////////////////////////////
// name:	UpdateMloList
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CPortalDebugHelper::UpdateMloList()
{
	u32 i=0;

	atArray<const char*> emptyNames;
	proxyPtrs.ResetCount();

	CInteriorProxy::Pool* proxyPool = CInteriorProxy::GetPool();

	int numProxies = proxyPool->GetSize();
	for(int i =0; i < numProxies; i++)
	{
		CInteriorProxy* pProxy = proxyPool->GetSlot(i);
		if(!pProxy)
		{
			continue;
		}
		proxyPtrs.PushAndGrow(pProxy);
	}

	proxyPtrs.QSort(0, proxyPtrs.GetCount(), CompareProxy_Names);

	mloNames.Reset();
	emptyNames.Reset();

	numProxies = proxyPtrs.GetCount();

	for(i=0;i<numProxies;i++) {	
		mloNames.PushAndGrow(proxyPtrs[i]->GetModelName());
		emptyNames.PushAndGrow("");
	}

	if(numProxies == 0)
	{
		// if "Render" Widgets haven't been allocated pMloCombo isn't valid yet
		if (pMloCombo != NULL)
		{
			pMloCombo->UpdateCombo("MLO", &currMloNameIdx, numProxies, NULL);
		}

		pMloCombo2->UpdateCombo("MLO", &currMloNameIdx, numProxies, NULL);
	}
	else
	{
		// this works!

		// if "Render" Widgets haven't been allocated pMloCombo isn't valid yet
		if (pMloCombo != NULL)
		{
			pMloCombo->UpdateCombo("MLO", &currMloNameIdx, numProxies, &emptyNames[0], MloIDChangeCB);
		}

		pMloCombo2->UpdateCombo("MLO", &currMloNameIdx, numProxies, &emptyNames[0], MloIDChangeCB);

		char	mloName[80];
		for(i=0;i<proxyPtrs.GetCount();i++) {	
			CInteriorProxy* pProxy = proxyPtrs[i];
			char	enableChar = '_';
			enableChar = pProxy->GetIsDisabled() ? 'D' : enableChar;
			enableChar = pProxy->GetIsCappedAtPartial() ? 'C' :enableChar;

			if (pProxy->GetGroupId() == 0)
			{
				sprintf(mloName,"%03d %c:%c   %-30.25s    (id:%03d)", i, (pProxy->IsContainingImapActive() ? ' ' : '!'), enableChar, pProxy->GetModelName(), proxyPool->GetJustIndex(pProxy));

			} else 
			{
				sprintf(mloName,"%03d %c:%c   %-30.25s    (id:%03d   grp:%02d)", i, (pProxy->IsContainingImapActive() ? ' ' : '!'), enableChar, pProxy->GetModelName(), proxyPool->GetJustIndex(pProxy), pProxy->GetGroupId());
			}

			// if "Render" Widgets haven't been allocated pMloCombo isn't valid yet
			if (pMloCombo != NULL)
			{
				pMloCombo->SetString(i, mloName);
			}

			pMloCombo2->SetString(i, mloName);
		}
	}

	MloIDChangeCB();

	bMLOListActive = true;
}

int CompareEntitySet_Names(const char* const* lhs, const char* const* rhs)
{
	return stricmp((*lhs), (*rhs));
}

void CPortalDebugHelper::UpdateEntitySetListCB()
{
	atArray<const char*> UnsortedActiveSet;
	atArray<const char*> UnsortedInactiveSet;

	UnsortedActiveSet.Reset();
	UnsortedInactiveSet.Reset();
	ActiveEntitySets.Reset();
	InactiveEntitySets.Reset();

	// now add the rest of the names from the current interior
	if (pCurrDebugInterior)
	{
		CMloModelInfo* pMLOModelInfo = pCurrDebugInterior->GetMloModelInfo();
		const atArray< CMloEntitySet > & entitySets = pMLOModelInfo->GetEntitySets();
		if (entitySets.GetCount() != 0)
		{
			UnsortedActiveSet.Reserve(MAX_ENTITY_SETS);
			UnsortedInactiveSet.Reserve(MAX_ENTITY_SETS);
			ActiveEntitySets.Reserve(MAX_ENTITY_SETS);
			InactiveEntitySets.Reserve(MAX_ENTITY_SETS);

			CInteriorProxy* pIntProxy = pCurrDebugInterior->GetProxy();

			for(u32 i=0; i< entitySets.GetCount(); i++)
			{
				atHashString setName = entitySets[i].m_name;

				if (pIntProxy->IsEntitySetActive(setName))
				{
					//ActiveEntitySets.Push(setName.GetCStr());
					UnsortedActiveSet.Push(setName.GetCStr());
				} else 
				{
					//InactiveEntitySets.Push(setName.GetCStr());
					UnsortedInactiveSet.Push(setName.GetCStr());
				}
			}

			//sort sets
			UnsortedActiveSet.QSort(0, UnsortedActiveSet.GetCount(), CompareEntitySet_Names);
			UnsortedInactiveSet.QSort(0, UnsortedInactiveSet.GetCount(), CompareEntitySet_Names);

			// push into correct place
			for(u32 i=0; i< UnsortedActiveSet.GetCount(); i++)
			{
				ActiveEntitySets.Push(UnsortedActiveSet[i]);
			}

			for(u32 i=0; i< UnsortedInactiveSet.GetCount(); i++)
			{
				InactiveEntitySets.Push(UnsortedInactiveSet[i]);
			}
		}

		u32 onCount = ActiveEntitySets.GetCount();
		u32 offCount = InactiveEntitySets.GetCount();

		sprintf(activeSetHeader, "  --- %d set%s marked active ---  ", onCount, (onCount != 1 ? "s" : ""));
		sprintf(inactiveSetHeader, "  --- %d set%s marked inactive ---  ", offCount, (offCount != 1 ? "s" : ""));

		ActiveEntitySets.PushAndGrow(activeSetHeader);
		InactiveEntitySets.PushAndGrow(inactiveSetHeader);

	} else 
	{
		ActiveEntitySets.PushAndGrow("* Select interior in picker *");
		InactiveEntitySets.PushAndGrow("* Select interior in picker *");
	}

	currActiveEntitySetIdx = Min(currActiveEntitySetIdx, ActiveEntitySets.GetCount()-1);
	pActiveEntitySetCombo->UpdateCombo("Active sets", &currActiveEntitySetIdx, ActiveEntitySets.GetCount(), &ActiveEntitySets[0], ChangeActiveSetCB);
	currInactiveEntitySetIdx = Min(currInactiveEntitySetIdx, InactiveEntitySets.GetCount()-1);
	pInactiveEntitySetCombo->UpdateCombo("Inactive sets", &currInactiveEntitySetIdx, InactiveEntitySets.GetCount(), &InactiveEntitySets[0], ChangeInactiveSetCB);

	currActiveEntitySetIdx = ActiveEntitySets.GetCount()-1;
	currInactiveEntitySetIdx = InactiveEntitySets.GetCount()-1;
}

void CPortalDebugHelper::GetCurrentInteriorCB()
{
	pCurrDebugInterior = NULL;
	pCurrDebugInterior = CPortalVisTracker::GetPrimaryInteriorInst();

	if (pCurrDebugInterior)
	{
		CInteriorProxy* pIntProxy = pCurrDebugInterior->GetProxy();
		pIntProxy->DeactivateEntitySet(ActiveEntitySets[currActiveEntitySetIdx]);
		if (g_PickerManager.IsCurrentOwner("Interior picker")){
			RefreshInteriorPickerCB();
		}
	}
	else
	{
		if (CInteriorInst::Pool* pPool = CInteriorInst::GetPool())
		{
			float nearestDist = WORLDLIMITS_XMAX;
			int nearest = -1;
			for (int i = 0; i < pPool->GetSize(); ++i)
			{
				if (CInteriorInst* pInteriorInst = pPool->GetSlot(i))
				{
					CEntity *pIntEnt = (CEntity*)pInteriorInst;
					float currentDist = camInterface::GetFrame().GetPosition().Dist(pIntEnt->GetBoundCentre());
					if (currentDist < nearestDist)
					{ 
						nearest = i;
						nearestDist = currentDist;
					}
				}
			}
			if (nearest >= 0)
			{ 
				pCurrDebugInterior = pPool->GetSlot(nearest);

				if (pCurrDebugInterior)
				{
					CInteriorProxy* pIntProxy = pCurrDebugInterior->GetProxy();
					pIntProxy->DeactivateEntitySet(ActiveEntitySets[currActiveEntitySetIdx]);
					if (g_PickerManager.IsCurrentOwner("Interior picker")){
						RefreshInteriorPickerCB();
					}
				}
			}
		}
	}
	UpdateEntitySetListCB();
}

void CPortalDebugHelper::ChangeActiveSetCB(){

	if (pCurrDebugInterior){
		CInteriorProxy* pIntProxy = pCurrDebugInterior->GetProxy();
		pIntProxy->DeactivateEntitySet(ActiveEntitySets[currActiveEntitySetIdx]);
	}
	UpdateEntitySetListCB();
}

void CPortalDebugHelper::ChangeInactiveSetCB(){
	if (pCurrDebugInterior){
		CInteriorProxy* pIntProxy = pCurrDebugInterior->GetProxy();
		pIntProxy->ActivateEntitySet(InactiveEntitySets[currInactiveEntitySetIdx]);
	}
	UpdateEntitySetListCB();
}

void CPortalDebugHelper::RefreshInteriorCB()
{
	if (pCurrDebugInterior){
		CInteriorProxy* pIntProxy = pCurrDebugInterior->GetProxy();
		pIntProxy->RefreshInterior();
	}

	CPortalVisTracker::RequestResetRenderPhaseTrackers();
}

void TriggerCaptureCB(void){
	if (CPortalVisTracker::GetPrimaryInteriorInst()){
		CPortalVisTracker::GetPrimaryInteriorInst()->CapturePedsAndVehicles();
	}
}

void ClearPlayerInteriorStateCB(void){
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer)
	{
		CPortalTracker::ClearInteriorStateForEntity(pPlayer);
	}
}

s32 numPOI = 5;
s32 selectedPOI = 0;
const char*	POIData[] = {"None", "Player","camera", "Boss MP", "Showroom"};

void PointsOfInterestCB(void){

	Vector3 delta(1.0f,1.0f,1.0f);
	switch(selectedPOI)
	{
		case 1:
			{
				g_minTestBox = CGameWorld::FindFollowPlayerCoors() - delta;
				g_maxTestBox = CGameWorld::FindFollowPlayerCoors() + delta;
			}
			break;
		case 2:
			{
				g_minTestBox = camInterface::GetPos() + (camInterface::GetFront() * 3.0f) - delta;
				g_maxTestBox = camInterface::GetPos() + (camInterface::GetFront() * 3.0f) + delta;
			}
			break;
		case 3:
			{
				g_minTestBox = Vector3(458.0f, -1002.0f, 10.0f);
				g_maxTestBox = Vector3(460.0f, -994.0f, 30.0f);
			}
		case 4:
			{
				g_minTestBox = Vector3(-55.f, -1105.f, 25.f);
				g_maxTestBox = Vector3(-33.f, -1093.f, 28.f);
			}
			break;
	}
}

void DisableSelectedMLOCB(void)
{
	CInteriorProxy* pProxy = proxyPtrs[currMloNameIdx];

	bCapPartialSelectedMLO = false;
	pProxy->SetIsCappedAtPartial(false);

	pProxy->SetIsDisabled(bDisableSelectedMLO);

	//MloIDChangeCB();
	CPortalDebugHelper::UpdateMloList();
}

void CapPartiaSelectedMLOCB(void)
{
	CInteriorProxy* pProxy = proxyPtrs[currMloNameIdx];

	bDisableSelectedMLO = false;
	pProxy->SetIsDisabled(false);

	pProxy->SetIsCappedAtPartial(bCapPartialSelectedMLO);

	//MloIDChangeCB();
	CPortalDebugHelper::UpdateMloList();
}

void CPortalDebugHelper::InitInteriorWidgets()
{
	bkBank* pInteriorsBank = &BANKMGR.CreateBank("Interiors");

	mloNames.Reset();
	mloNames.PushAndGrow("Inactive");
	mloNames.PushAndGrow("Activate");

	static bool s_bInteriorVisMode = false;
	pInteriorsBank->AddToggle("Show portals", &g_scanDebugFlagsPortals, SCAN_PORTALS_DEBUG_DISPLAY_PORTALS);
	pInteriorsBank->AddToggle("Show objects belonging to room", &s_bInteriorVisMode, CRenderPhaseDebugOverlayInterface::ToggleInteriorLocationMode);
	pInteriorsBank->AddSeparator();
	pInteriorsBank->AddText("Current Interior: ", ms_currIntInfo, sizeof(ms_currIntInfo));
	pInteriorsBank->AddButton("Show current interior information", DisplayCurrentInteriorInfo);
	pInteriorsBank->AddSeparator();
	pMloCombo2 = pInteriorsBank->AddCombo("MLO Target", &currMloNameIdx, 2, &mloNames[0], UpdateMloList);
	pInteriorsBank->AddButton("Warp player", NuLoadSceneToMLOCB);
}

bool CPortalDebugHelper::InitWidgets(void)
{
	currMloNameIdx = 0;

	bkBank *pBank = BANKMGR.FindBank("Renderer");
	Assert(pBank);

	ASSERT_ONLY(bkGroup *pBankGroup = )pBank->PushGroup("Portals", false);
	Assert(pBankGroup);

	pBank->PushGroup("Entity state in volume");
	pBank->AddToggle("Activate",  &gbDebugEntitiesInVolume);
	pBank->AddToggle("Include peds",  &gbDebugIncludePedsInVolume);
	pBank->AddToggle("Include vehicles",  &gbDebugIncludeVehiclesInVolume);
	pBank->AddCombo("Points of interest", &selectedPOI, numPOI, POIData, PointsOfInterestCB );
	pBank->AddSlider("Min X", &g_minTestBox.x, -8000, 8000, 1);
	pBank->AddSlider("Min Y", &g_minTestBox.y, -8000, 8000, 1);
	pBank->AddSlider("Min Z", &g_minTestBox.z, -8000, 8000, 1);
	pBank->AddSlider("Max X", &g_maxTestBox.x, -8000, 8000, 1);
	pBank->AddSlider("Max Y", &g_maxTestBox.y, -8000, 8000, 1);
	pBank->AddSlider("Max Z", &g_maxTestBox.z, -8000, 8000, 1);
	pBank->PopGroup();

	pBank->AddButton("Trigger capture",TriggerCaptureCB);
	pBank->AddButton("Clear Player interior state",ClearPlayerInteriorStateCB);

	extern bool gbForceReScan;
	extern bool gbForcePlayerRescan;
	extern bool g_breakOnSelectedEntityInPortalTrackerUpdate;
	extern bool g_breakOnSelectedEntityHavingDifferentInteriorLoc;
	extern bool g_breakOnSelectedMoveToNewLocation;
	extern bool gUsePortalTrackerCapture;

	extern bool g_bToggleSelectedFromActivatingTrackerList;
	extern bool g_bToggleSelectedLoadingCollisions;
	extern bool g_IgnoreSpeedTestForSelectedEntity;

	pBank->AddToggle("FORCE RESCAN - USE WHEN PORTALS BREAK", &gbForceReScan);
	pBank->AddToggle("Force rescan on player ONLY", &gbForcePlayerRescan);
	pBank->AddToggle("Display portal render stats", &bShowPortalRenderStats);
	pBank->AddToggle("Break inside CPortalTracker::Update() on selected entity", &g_breakOnSelectedEntityInPortalTrackerUpdate);
	pBank->AddToggle("Break inside CPortalTracker::Update() on selected entity if has a different value to the debugLocation", &g_breakOnSelectedEntityHavingDifferentInteriorLoc);
	pBank->AddToggle("Break inside CPortalTracker::MoveToNewLocation() on selected entity", &g_breakOnSelectedMoveToNewLocation);
	pBank->AddToggle("Don't use RequestCaptureForInterior() when capturing peds and vehilces for interiors", &gUsePortalTrackerCapture);

	pBank->AddToggle("Toggle Active Tracker", &g_bToggleSelectedFromActivatingTrackerList);
	pBank->AddToggle("Toggle Loads Collisions", &g_bToggleSelectedLoadingCollisions);
	pBank->AddToggle("Ignore high speed test for selected entity", &g_IgnoreSpeedTestForSelectedEntity);


	
	pBank->PushGroup("MLO debug");
		mloNames.Reset();
		mloNames.PushAndGrow("Inactive");
		mloNames.PushAndGrow("Activate");

		pBank->AddText("Selected MLO bound", selectedMLOBound, NAME_LENGTH, true);
		pBank->AddText("Selected MLO .imap", selectedMLOImap, NAME_LENGTH, true);
		pBank->AddText("Selected MLO LOD", selectedMLOLOD, NAME_LENGTH, true);
		pBank->AddText("Selected MLO Pos", selectedMLOPos, NAME_LENGTH, true);
		pBank->AddText("Selected MLO Rot", selectedMLORot, NAME_LENGTH, true);
		pBank->AddText("Selected MLO state", selectedMLOCurrentState, NAME_LENGTH, true);
		pMloCombo = pBank->AddCombo("MLO Target", &currMloNameIdx, 2, &mloNames[0], UpdateMloList);
		pBank->AddButton("Dump to file", DumpMLOToFileCB);

		pBank->AddButton("Warp player to target", NuLoadSceneToMLOCB);
		pBank->AddButton("Warp camera to target", WarpCameraToMLOCB);
		pBank->AddToggle("'Disable' selected MLO completely", &bDisableSelectedMLO, DisableSelectedMLOCB);
		pBank->AddToggle("'Cap' selected MLO to shell only entities", &bCapPartialSelectedMLO, CapPartiaSelectedMLOCB);
		pBank->AddToggle("Show MLO bounds", &bShowMLOBounds);
		pBank->AddToggle("Show tunnel MLO bounds", &bShowTunnelMLOBounds);
		pBank->AddToggle("Show tunnel MLO box", &bShowTunnelMLOBox);
		pBank->AddToggle("Show tunnel MLO orientation", &bShowTunnelMLOOrientation);
		pBank->AddToggle("Show Camera MLO bounds", &bShowCamMLOBounds);	
		pBank->AddToggle("Render only current MLO", &bDrawOnlyCurrentInterior);
	pBank->PopGroup();

	pBank->PushGroup("Room debug", false);
		pBank->AddToggle("Track camera", &bTrackCamera);
		pBank->AddToggle("Clip objects by point", &bObjPointClip);
		pBank->AddSlider("Room idx",&debugRoomIdx, 0, 63, 1, ChangeRoomIdxCB);
		pBank->AddToggle("Render objects", &bRenderObjects, UpdateValuesCB);
		pBank->AddToggle("Render obj points", &bRenderObjPts, UpdateValuesCB);
		pBank->AddToggle("Render portals", &bRenderPortals, UpdateValuesCB);
		pBank->AddButton("Set render for all rooms", ResetDataCB);
	pBank->PopGroup();
	
	pBank->AddToggle("Show portal instances", &bShowPortalInstances);
	pBank->AddToggle("Show portal tracker safe zone spheres", &bShowPortalTrackerSafeZoneSpheres);
	pBank->AddToggle("Show portal tracker safe zone sphere for selected entity", &bShowPortalTrackerSafeZoneSphereForSelected);
	pBank->AddToggle("Break when selected portal tracker safe sphere radius is near zero", &bBreakWhenSelectedPortalTrackerSafeSphereRadiusIsNearZero);
	pBank->AddToggle("Calculate safe zone spheres using Bounding Sphere method ", &bSafeZoneSphereBoundingSphereMethod);
	pBank->AddToggle("Show timecycle screen space portals",&bShowTCScreenSpacePortals);
	pBank->AddSlider("Timecycle screen space portals scale",&fDebugRenderScale,0.0f,1.0f,0.01f);
	
	pBank->AddToggle("Disable portal streaming", &bDisablePortalStream);
	pBank->AddToggle( "Disable exterior scene",				&g_scanDebugFlags, SCAN_DEBUG_DONT_RENDER_EXTERIOR);
	pBank->AddToggle("Show all tracking", &bVisualiseAllTrackers);
	pBank->AddToggle("Show active tracking", &bVisualiseActivatingTrackers);
	pBank->AddToggle("Show cutscene tracking", &bVisualiseCutsceneTrackers);
	pBank->AddToggle("Show object names", &bVisualiseObjectNames);
	pBank->AddToggle("Interior scene?", &CPortal::ms_bIsInteriorScene);
	pBank->AddToggle("Check PT consistent", &bCheckConsistency);
	pBank->AddToggle("Don't perform PT update", &bDontPerformPtUpdate);
	pBank->AddToggle("Enable portal tracker update spew", &bPtUpdateDebugSpew);
	pBank->AddToggle("Show Captured Call Stack for selected entity", &bPtEnableDrawingOfCollectedCallstack);
	
	pBank->AddToggle("Enable Recursive Update check", &g_enableRecursiveCheck);
	pBank->AddToggle("Break on Recursive Update fail", &g_breakOnRecursiveFail);
	pBank->AddToggle("Pause game on Recursive Update fail", &g_pauseGameOnRecursiveFail);
	pBank->AddToggle("Recursive Update add mismatched portal tracker entities to picker", &g_recursiveCheckAddToPicker);
	pBank->AddToggle("Ignore small delta early out", &g_ignoreSmallDeltaEarlyOut);

	pBank->PushGroup("Brute Force Move Selected Entity to debug location", false);
		pBank->AddSlider("Interior Index", &g_bruteForceMoveInteriorProxyIndex, -1, 1000, 1);
		pBank->AddSlider("Room Index", &g_bruteForceMoveRoomIndex, -1, 1000, 1);
		pBank->AddButton("Move to location", datCallback(CFA(BruteForceMoveCallback)));
		pBank->AddButton("Move to retain list", datCallback(CFA(BruteForceRetainListCallback)));
	pBank->PopGroup();


	pBank->PushGroup("MLO Pool",false);
	pBank->AddSlider("Force MLO LOD Level", &forceMLOLODLevel, -1, 2, 1);
	pBank->AddSlider("Filter MLO LOD Level", &filterMLOLODLevel, -1, 2, 1);
	pBank->AddToggle("Show MLO pool", &bDumpMLODebugData);
	pBank->AddToggle("Log MLO pool", &bPrintfMLOPool);

	pBank->AddToggle("Filter: show everything", &bFilterShowAll);
	pBank->AddToggle("Filter: instanced portals", &bFilterPortalNotInstanced);
	pBank->AddToggle("Filter: rendered", &bFilterNotRendered);
	pBank->AddToggle("Filter: in use", &bFilterNotInUse);
	pBank->AddToggle("Filter: populated", &bFilterPopulated);
	pBank->AddToggle("Filter: state", &bFilterState);
	pBank->AddToggle("Filter: retaining", &bFilterRetaining);
	pBank->AddToggle("Filter: in picker list", &bFilterPickerList);
	pBank->AddSlider("Filter by group",  &groupFilter, -1, 32, 1);
	pBank->AddSlider("Filter by index", &IndexFilter, -1, 700, 1);
	pBank->AddToggle("Show Active interiors", &bDumpActiveInteriorList);
	pBank->PopGroup();

	pBank->PushGroup("Interior LODing", false);
	pBank->AddButton("Pick interior via contents", ActivateContentsPickerCB);
	pBank->AddToggle("Override the selected MLO alpha", &bOverrideCurrentMLOAlpha);
	pBank->AddSlider("Override alpha", &currentMLOAlphaVal, 0, 255, 1);
	pBank->PushGroup("Per interior flags", true);
	pCapAlphaEntities = pBank->AddToggle("Cap alpha of contents to MLO", &bCapAlphaOfContents, UpdateFlagsCB);
	pTurnGPSOn = pBank->AddToggle("Turn on GPS", &bTurnOnGPS, UpdateFlagsCB);
	pShortFadeDist = pBank->AddToggle("Use short fade distance", &bShortFadeDistance, UpdateFlagsCB);
	pBank->PopGroup();
	pBank->PopGroup();

	pBank->PushGroup("Entity Sets", false);
	pBank->AddButton("Interior picker", ActivateInteriorPickerCB);
	pBank->AddButton("Get closest interior", GetCurrentInteriorCB);
	ActiveEntitySets.PushAndGrow("* Select interior in picker *");
	InactiveEntitySets.PushAndGrow("* Select interior in picker *");
	pActiveEntitySetCombo = pBank->AddCombo("Active sets", &currActiveEntitySetIdx, 1, &ActiveEntitySets[0], UpdateEntitySetListCB);
	pInactiveEntitySetCombo = pBank->AddCombo("Inactive sets", &currInactiveEntitySetIdx, 1, &InactiveEntitySets[0], UpdateEntitySetListCB);
	pBank->AddButton("Flush out current interior", RefreshInteriorCB);
	pBank->PopGroup();

	pBank->PushGroup("Memory", false);
	pBank->AddToggle("Display memory stats", &bDisplayMemUse);
	pBank->AddToggle("Breakdown by room", &bDisplayRoomMemUse);
	pBank->AddToggle("Lod/Portal detail", &bDisplayLodDetail);
	pBank->AddSlider("Detail by room", &roomIdxMemDetailDebug, -1, 31, 1);
	pBank->PopGroup();

	pBank->AddSlider("Interior Fx Level", &CPortal::ms_interiorFxLevel, 0.0f, 1.0f, 0.0f);

	pBank->PopGroup();

	ResetDataCB();
	UpdateValuesCB();

	pBank = BANKMGR.FindBank("Optimization");
	pBank->AddToggle( "Don't render exterior", &g_scanDebugFlags, SCAN_DEBUG_DONT_RENDER_EXTERIOR);

	class GetSceneGraphNodeDebugName { public: static void func(char name[256], const fwSceneGraphNode* node, const fwSceneGraphNode* from, bool bShowInteriorName, bool bIsMirror)
	{
		fwInteriorLocation location; // invalid

		switch ((int)node->GetType())
		{
		case SCENE_GRAPH_NODE_TYPE_EXTERIOR : strcpy(name, "EXTERIOR"); return;
		case SCENE_GRAPH_NODE_TYPE_STREAMED : strcpy(name, "STREAMED"); return;
		case SCENE_GRAPH_NODE_TYPE_INTERIOR : strcpy(name, "INTERIOR"); return;
		case SCENE_GRAPH_NODE_TYPE_ROOM     : location = reinterpret_cast<const fwRoomSceneGraphNode  *>(node)->GetInteriorLocation(); break;
		case SCENE_GRAPH_NODE_TYPE_PORTAL   : location = reinterpret_cast<const fwPortalSceneGraphNode*>(node)->GetInteriorLocation(); break;
		}

		if (location.IsValid())
		{
			if (location.IsAttachedToRoom())
			{
				CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation(location);
				CInteriorInst*  pInteriorInst  = pInteriorProxy->GetInteriorInst();
				CMloModelInfo*  pMloModelInfo  = reinterpret_cast<CMloModelInfo*>(pInteriorInst->GetBaseModelInfo());

				if (bShowInteriorName)
				{
					sprintf(name, "%s / %s", pMloModelInfo->GetModelName(), pMloModelInfo->GetRoomName(location));
				}
				else
				{
					sprintf(name, "%s(%d)", pMloModelInfo->GetRoomName(location), location.GetRoomIndex());
				}
			}
			else if (location.IsAttachedToPortal())
			{
				const fwSceneGraphNode* node0 = reinterpret_cast<const fwPortalSceneGraphNode*>(node)->GetNegativePortalEnd();
				const fwSceneGraphNode* node1 = reinterpret_cast<const fwPortalSceneGraphNode*>(node)->GetPositivePortalEnd();

				char name0[256] = ""; func(name0, node0, NULL, false, false);
				char name1[256] = ""; func(name1, node1, NULL, false, false);

				if      (from == NULL ) { sprintf(name, "PORTAL_%d %s --- %s", (int)node->GetIndex(), name0, name1); }
				else if (from == node0) { sprintf(name, "PORTAL_%d %s --> %s", (int)node->GetIndex(), name0, bIsMirror ? "MIRROR" : name1); }
				else if (from == node1) { sprintf(name, "PORTAL_%d %s <-- %s", (int)node->GetIndex(), name1, bIsMirror ? "MIRROR" : name0); }
				else                    { sprintf(name, "PORTAL_%d %s <-> %s", (int)node->GetIndex(), name0, bIsMirror ? "MIRROR" : name1); }
			}
			else
			{
				strcpy(name, "UNKNOWN LOCATION");
			}

			return;
		}

		strcpy(name, "UNKNOWN");
	}};

	fwSceneGraphNode::sm_GetSceneGraphNodeDebugNameFunc = GetSceneGraphNodeDebugName::func;

	return true;
}
#endif //__BANK


#if __DEV

void DrawCross(const Vector3 &vecPosition, u32 col){

	grcDebugDraw::Line(vecPosition + Vector3( -0.4f, 0.0f, -0.4f), vecPosition + Vector3( +0.4f, 0.0f, +0.4f), *grcDebugDraw::GetDebugColor(col));
	grcDebugDraw::Line(vecPosition + Vector3( +0.4f, 0.0f, -0.4f), vecPosition + Vector3( -0.4f, 0.0f, +0.4f), *grcDebugDraw::GetDebugColor(col));
	grcDebugDraw::Line(vecPosition + Vector3(  0.0f, 0.4f,  0.0f), vecPosition + Vector3(  0.0f,-0.4f,  0.0f), *grcDebugDraw::GetDebugColor(col));
}
#define STR_LENGTH (24)
static char gRoomString[STR_LENGTH];
static char gIinteriorString[STR_LENGTH];

bool CPortalDebugHelper::BuildCamLocationString(char* cTempString)
{
	if (gRoomString[0] != '\0' || gIinteriorString[0] != '\0')
	{
		sprintf(cTempString, "<room: %s>  <interior: %s>",gRoomString, gIinteriorString);
		return true;
	}
	else
	{
		return false;
	}
}
#endif //__DEV

#if __BANK
void CPortalDebugHelper::DebugOutput(void){

	// crappy debug code to catch boss falling out of interior in MP
	if (gbDebugEntitiesInVolume){
		grcDebugDraw::AddDebugOutput("entities in volume:");

		CPed* pTestPed = NULL;
		CVehicle* pTestVeh = NULL;
		spdAABB testBox;
		testBox.SetMin(VECTOR3_TO_VEC3V(g_minTestBox));
		testBox.SetMax(VECTOR3_TO_VEC3V(g_maxTestBox));

		grcDebugDraw::BoxAxisAligned(VECTOR3_TO_VEC3V(g_minTestBox), VECTOR3_TO_VEC3V(g_maxTestBox), Color32(255,0,0), false);

		if (gbDebugIncludePedsInVolume)
		{
			CPed::Pool* pPool = CPed::GetPool();
			if (pPool){
				for(u32 i= 0; i<pPool->GetSize(); i++){
					pTestPed = pPool->GetSlot(i);
					if (pTestPed){
						Vector3 testPos = VEC3V_TO_VECTOR3(pTestPed->GetTransform().GetPosition());
						if (testBox.ContainsPoint(pTestPed->GetTransform().GetPosition())){
							grcDebugDraw::AddDebugOutput("%s : %s : %s : %d : (%.2f,%.2f,%.2f)", pTestPed->GetModelName(), (pTestPed->IsNetworkClone()?"CLONE": "NOT CLONE"),
								(pTestPed->GetIsRetainedByInteriorProxy()?"RETAINED":"NOT RETAINED"), pTestPed->GetPortalTracker()->GetInteriorProxyIdx(),
								testPos.GetX(),testPos.GetY(), testPos.GetZ());
						}
					}
				}
			}
		}

		if (gbDebugIncludeVehiclesInVolume)
		{
			CVehicle::Pool* pPool = CVehicle::GetPool();
			if (pPool){
				for(u32 i= 0; i<pPool->GetSize(); i++){
					pTestVeh = pPool->GetSlot(i);
					if (pTestVeh){
						Vector3 testPos = VEC3V_TO_VECTOR3(pTestVeh->GetTransform().GetPosition());
						if (testBox.ContainsPoint(pTestVeh->GetTransform().GetPosition())){
							grcDebugDraw::AddDebugOutput("%s : %s : %s : %d : (%.2f,%.2f,%.2f)", pTestVeh->GetModelName(), (pTestVeh->IsNetworkClone()?"CLONE": "NOT CLONE"),
								(pTestVeh->GetIsRetainedByInteriorProxy()?"RETAINED":"NOT RETAINED"), pTestVeh->GetPortalTracker()->GetInteriorProxyIdx(),
								testPos.GetX(),testPos.GetY(), testPos.GetZ());
						}
					}
				}
			}
		}

		grcDebugDraw::AddDebugOutput("---------");
	}


	grcDebugDraw::AddDebugOutput("----- Active Trackers Info -----");
	s32 numActivatingTrackers = 0;
	fwPtrList& activeTrackerList =  CPortalTracker::GetActivatingTrackerList();
	fwPtrNode* pNode = activeTrackerList.GetHeadPtr();

	while(pNode != NULL){
#if __BANK
		CPortalTracker* pPT = reinterpret_cast<CPortalTracker*>(pNode->GetPtr());
		if (Verifyf(pPT, "Portal Tracker is NULL"))
		{
			if (bShowPortalRenderStats)
			{
				if (pPT->IsVisPortalTracker())
				{
					grcDebugDraw::AddDebugOutput("%d : Vis Portal Tracker %.3f, %.3f, %.3f", numActivatingTrackers, pPT->GetCurrentPos().x, pPT->GetCurrentPos().y, pPT->GetCurrentPos().z);
				}
				else
				{
					grcDebugDraw::AddDebugOutput("%d : %s %.3f, %.3f, %.3f", numActivatingTrackers, pPT->GetOwner() ? pPT->GetOwner()->GetModelName() : "No Owner", pPT->GetCurrentPos().x, pPT->GetCurrentPos().y, pPT->GetCurrentPos().z);
				}
			}
		}
#endif
		pNode = pNode->GetNextPtr();


		numActivatingTrackers++;
	}

	if (bShowPortalRenderStats){
		grcDebugDraw::AddDebugOutput("----- Portal render stats -----");
		grcDebugDraw::AddDebugOutput("No. MLOs in scene : %d",numMLOsInScene);
		grcDebugDraw::AddDebugOutput("No. low LOD MLOs rendered : %d",numLowLODRenders);
		grcDebugDraw::AddDebugOutput("No. high LOD MLOs rendered : %d",numHighLODRenders);
		grcDebugDraw::AddDebugOutput("No. models in High LODs rendered : %d",numIntModelsRendered);
		grcDebugDraw::AddDebugOutput("-----");
		grcDebugDraw::AddDebugOutput("Stats for current camera");
		grcDebugDraw::AddDebugOutput("No. active trackers in current MLO : %d",numCurrIntTrackers);
		grcDebugDraw::AddDebugOutput("No. rooms in current MLO : %d",numCurrIntRooms);
		grcDebugDraw::AddDebugOutput("No. models in current MLO : %d",numCurrIntModels);
		grcDebugDraw::AddDebugOutput("No. models in current MLO rendered : %d",numCurrIntModelsRendered);
		grcDebugDraw::AddDebugOutput("-----");
		grcDebugDraw::AddDebugOutput("No. activating trackers in world : %d", numActivatingTrackers);

		// reset
		numMLOsInScene = 0;
		numLowLODRenders = 0;
		numHighLODRenders = 0;
		numIntModelsRendered = 0;

		numCurrIntModelsRendered = 0;
		numCurrIntTrackers = 0;
		numCurrIntRooms = 0;
		numCurrIntModels = 0;

		if (pCurrInterior){
			numCurrIntRooms = pCurrInterior->GetNumRooms();

			// count tracked objects in this interior
			for(s32 i=0;i<numCurrIntRooms;i++){
				fwPtrList& trackedObjList =  pCurrInterior->GetTrackedObjects(i);
				fwPtrNode* pNode = trackedObjList.GetHeadPtr();

				while(pNode != NULL){
#if __ASSERT
					CPortalTracker* pPT = reinterpret_cast<CPortalTracker*>(pNode->GetPtr());
					Assert(pPT);
#endif
					pNode = pNode->GetNextPtr();

					numCurrIntTrackers++;
				}
			}
		}
	}

	if (bShowMLOBounds || bShowTunnelMLOBounds)
	{
		CInteriorProxy* pIntProxy = NULL;
		s32 poolSize=CInteriorProxy::GetPool()->GetSize();

		s32 i = 0;
		while(i<poolSize)
		{
			pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
			if (pIntProxy)
			{
				//spdAABB proxyBox;
				spdSphere proxySphere;
				//pIntProxy->GetBoundBox( proxyBox );
				pIntProxy->GetBoundSphere( proxySphere );

				Color32 MLOcolour;
				if (pIntProxy->GetGroupId() >= 1 && bShowTunnelMLOBounds)
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(proxySphere.GetCenter()), proxySphere.GetRadius().Getf(), Color32(0,0,255) , false);
				} 

				if (pIntProxy->GetGroupId() < 1 && bShowMLOBounds)
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(proxySphere.GetCenter()), proxySphere.GetRadius().Getf(), Color32(255,0,0) , false);
				}
			}
			i++;
		}
	}

	if (bDumpMLODebugData){
		DumpMLODebugData();
		bPrintfMLOPool = false;
		
	}
	if (bDumpActiveInteriorList){
		DumpActiveInteriorList();
	}

	if (bDisplayMemUse){
		DisplayMemUse();
	}

}

bool ChangeInterior(void* pItem, void* data)
{
	fwInteriorLocation location;
	location.SetInteriorProxyIndex(g_bruteForceMoveInteriorProxyIndex);
	location.SetRoomIndex(g_bruteForceMoveRoomIndex);

	CLightEntity* pEntity = static_cast<CLightEntity*>(pItem);
	CEntity *pParent = static_cast<CEntity*>(data);

	if (pEntity && pEntity->GetParent() == pParent)
	{
		pEntity->RemoveFromInterior();
		pEntity->AddToInterior(location);
	}
	return true;
}

void BruteForceMoveCallback(void)
{
	fwInteriorLocation location;
	location.SetInteriorProxyIndex(g_bruteForceMoveInteriorProxyIndex);
	location.SetRoomIndex(g_bruteForceMoveRoomIndex);

	CEntity *pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
	if(pEntity)
	{
		if (pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle())
		{
			CDynamicEntity* pDynamic = (CDynamicEntity*)pEntity;
			pDynamic->GetPortalTracker()->MoveToNewLocation(location);
		}
		if (pEntity->GetIsTypeObject() || pEntity->GetIsTypeLight())
		{
			pEntity->RemoveFromInterior();
			pEntity->AddToInterior(location);
			if(pEntity->GetIsTypeObject())
			{
				CLightEntity::GetPool()->ForAll(ChangeInterior, pEntity);
			}
		} 		
	}
}

void BruteForceRetainListCallback(void)
{
	CEntity *pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
	if (pEntity && g_bruteForceMoveInteriorProxyIndex>=0 && CInteriorProxy::GetPool()->IsInPool(g_bruteForceMoveInteriorProxyIndex)){
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot( g_bruteForceMoveInteriorProxyIndex );
		if (pIntProxy){
			pIntProxy->MoveToRetainList(pEntity);
		}
	}
}
#endif //__BANK


