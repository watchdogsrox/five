#include "WaypointRecording.h"

//game includes

#include "control\gamelogic.h"
#include "fwsys\fileExts.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "core\game.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds\AssistedMovement.h"
#include "Peds\ped.h"
#include "Peds\PedFactory.h"
#include "peds/Ped.h"
#include "Peds\PedMoveBlend\PedMoveBlendInWater.h"
#include "Peds\PedMoveBlend\PedMoveBlendOnSkis.h"
#include "Peds\PedDebugVisualiser.h"
#include "Peds\PedIntelligence.h"
#include "physics/physics.h"
#include "Scene\World\GameWorld.h"
#include "streaming/streaming.h"			
#include "Task\Motion\Locomotion\TaskMotionPed.h"
#include "Task\Movement\TaskGoto.h"
#include "Task\Movement\TaskFollowWaypointRecording.h"
#include "Task\Movement\Jumping\TaskJump.h"
#include "Task\General\TaskBasic.h"
#include "vehicleAi\task\TaskVehicleFollowRecording.h"
#include "debug/DebugScene.h"
#include "Task/Vehicle/TaskCar.h"

//rage includes

#include "math\angmath.h"

AI_OPTIMISATIONS()

#if __BANK
bank_float CWaypointRecording::ms_fMBRDeltaForNewWaypointOnSkis = 0.45f;
bank_float CWaypointRecording::ms_fMBRDeltaForNewWaypointOnFoot = 0.25f;
bank_float CWaypointRecording::ms_fMinWaypointSpacing = 1.0f;
bank_float CWaypointRecording::ms_fMaxWaypointSpacingOnSkis = 10.0f;
bank_float CWaypointRecording::ms_fMaxWaypointSpacingOnFoot = 2.0f;
bank_float CWaypointRecording::ms_fMaxPlaneDistOnSkis = 1.0f;
bank_float CWaypointRecording::ms_fMaxPlaneDistOnFoot = 0.5f;
bank_float CWaypointRecording::ms_fMBRDeltaForNewWaypointInVehicle = 1.0f;
bank_float CWaypointRecording::ms_fMaxWaypointSpacingInVehicle = 10.0f;
bank_float CWaypointRecording::ms_fMaxPlaneDistInVehicle = 1.0f;
bool CWaypointRecording::ms_bDebugDraw = false;
bool CWaypointRecording::ms_bLoopRoutePlayback = true;
bool CWaypointRecording::ms_bUseAISlowdown = false;
bool CWaypointRecording::ms_bDisplayStore = false;
int CWaypointRecording::ms_iDebugStoreIndex = 0;
char CWaypointRecording::ms_TestRouteName[128] = { 0 };
float CWaypointRecording::ms_fAssistedMovementPathWidth = CAssistedMovementRoute::ms_fDefaultPathWidth;
float CWaypointRecording::ms_fAssistedMovementPathTension = CAssistedMovementRoute::ms_fDefaultPathTension;
CPed * CWaypointRecording::ms_pPed = NULL;
::CWaypointRecordingRoute CWaypointRecording::ms_RecordingRoute;
bool CWaypointRecording::ms_bRecording = false;
bool CWaypointRecording::ms_bRecordingVehicle = false;
bool CWaypointRecording::ms_bWasFlipped = false;
bool CWaypointRecording::ms_bUnderwaterLastFrame = false;
bool CWaypointRecording::ms_bWaypointEditing = false;
char CWaypointRecording::m_LoadedRouteName[256] = { 0 };
int CWaypointRecording::m_iLoadedRouteNumWaypoints = 0;

float fOverrideMBR = MOVEBLENDRATIO_WALK;
bool bUseRunAndGun = false;
CWaypointRecording::TEditingControls CWaypointRecording::ms_EditingControls;
#endif

CWaypointRecordingStreamingInterface CWaypointRecording::m_StreamingInterface;
CLoadedWaypointRecording CWaypointRecording::ms_LoadedRecordings[ms_iMaxNumLoaded];
CWaypointRecording::TStreamingInfo* CWaypointRecording::m_StreamingInfo = NULL;

#if __BANK
void StartRecordingOnFocusPed()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CWaypointRecording::ms_bDebugDraw = true;
		CWaypointRecording::ClearRoute();
		CWaypointRecording::StartRecording(pFocus);
	}
}
void StartRecordingOnPlayerPed()
{
	CWaypointRecording::ms_bDebugDraw = true;
	CWaypointRecording::ClearRoute();
	CWaypointRecording::StartRecording(FindPlayerPed());
}
void StartRecordingOnPlayerVehicle()
{
	CWaypointRecording::ms_bDebugDraw = true;
	CWaypointRecording::ClearRoute();
	CWaypointRecording::StartRecording(FindPlayerPed(), true);
}
void StopRecording()
{
	if(CWaypointRecording::GetIsRecording())
	{
		CWaypointRecording::StopRecording();
	}
}
void StartPlaybackOnFocusPed()
{
	if(!CWaypointRecording::GetIsRecording())
	{
		CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
		if(pFocus)
		{
			CWaypointRecording::StartPlayback(pFocus);
		}
	}
}
void StartPlaybackOnPlayerPed()
{
	if(!CWaypointRecording::GetIsRecording())
	{
		CWaypointRecording::StartPlayback(FindPlayerPed());
	}
}
void StartPlaybackOnPlayerVehicle()
{
	if(!CWaypointRecording::GetIsRecording())
	{
		CWaypointRecording::StartPlayback(FindPlayerPed(), true);
	}
}
void StartPlaybackOnFocusPedVehicle()
{
	if(!CWaypointRecording::GetIsRecording())
	{
		CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
		if(pFocus)
		{
			CWaypointRecording::StartPlayback(pFocus, true);
		}
	}
}
void StopPlayback()
{
	if(!CWaypointRecording::GetIsRecording())
	{
		CWaypointRecording::StopPlayback();
	}
}
void ClearRoute()
{
	if(!CWaypointRecording::GetIsRecording())
	{
		CWaypointRecording::ClearRoute();
	}
}
void SaveRoute()
{
	Assertf(!CWaypointRecording::GetIsRecording(), "You need to stop recording first!");
	Assertf(CWaypointRecording::GetRecordingRoute()->GetSize(), "Route has no points!");

	if(!CWaypointRecording::GetRecordingRoute()->SanityCheck())
	{
		Assertf(false, "ERROR - the route has issues.. It cannot be saved until these are fixed.");
		return;
	}

	char filename[512];
	BANKMGR.OpenFile(filename, 512, "*.iwr", true, "Save waypoint file");
	{
		if(!CWaypointRecording::CheckWaypointRecordingName(filename))
		{
			Assertf(false, "Bad filename!");
			return;
		}
		fiStream * pFile = fiStream::Create(filename);
		Assertf(pFile, "Couldn't open file \"%s\" for writing.", filename);

		if(pFile)
		{
			CWaypointRecording::GetRecordingRoute()->Save(pFile);
			pFile->Flush();
			pFile->Close();
		}
	}
}
void LoadRoute()
{
	Assertf(!CWaypointRecording::GetIsRecording(), "You need to stop recording first!");

	char filename[512];
	BANKMGR.OpenFile(filename, 512, "*.iwr", false, "Load waypoint file");
	{
		if(!CWaypointRecording::CheckWaypointRecordingName(filename))
		{
			Assertf(false, "Bad filename!");
			return;
		}

		fiStream * pFile = fiStream::Open(filename);
		Assertf(pFile, "Couldn't open file \"%s\" for reading.", filename);

		if(pFile)
		{
			CWaypointRecording::GetRecordingRoute()->Clear();
			CWaypointRecording::GetRecordingRoute()->Load(pFile, CWaypointRecording::GetRecordingRoute()->GetWaypointsBuffer() );
			pFile->Flush();
			pFile->Close();

			strcpy(CWaypointRecording::m_LoadedRouteName, filename);
			CWaypointRecording::m_iLoadedRouteNumWaypoints = CWaypointRecording::GetRecordingRoute()->GetSize();
			CWaypointRecording::ms_EditingControls.m_iSelectedWaypoint = 0;
		}
	}
}
void TestStreamRoute()
{
	CWaypointRecording::RequestRecording(CWaypointRecording::ms_TestRouteName, THREAD_INVALID);
}
void TestRemoveRoute()
{
	CWaypointRecording::RemoveRecording(CWaypointRecording::ms_TestRouteName, THREAD_INVALID);
}
void TestStreamingLoadAllRequested()
{
	CStreaming::LoadAllRequestedObjects();
}
void WarpPlayerToRouteStart()
{
	::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecordingBySlotIndex(CWaypointRecording::ms_iDebugStoreIndex);
	if(pRoute && pRoute->GetSize() >= 2)
	{
		const ::CWaypointRecordingRoute::RouteData & wpt1 = pRoute->GetWaypoint(0);
		const ::CWaypointRecordingRoute::RouteData & wpt2 = pRoute->GetWaypoint(1);
		const float fHdg = fwAngle::GetRadianAngleBetweenPoints(wpt2.GetPosition().x, wpt2.GetPosition().y, wpt1.GetPosition().x, wpt1.GetPosition().y);

		CPed * pPlayer = FindPlayerPed();
		if(pPlayer)
		{
			pPlayer->Teleport(wpt1.GetPosition(), fHdg);
		}
	}
}
void TestPlayStreamedRecording(CPed * pPed, int iSlot)
{
	if(pPed)
	{
		::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecordingBySlotIndex(iSlot);
		if(pRoute && pRoute->GetSize() >= 2)
		{
			const ::CWaypointRecordingRoute::RouteData & wpt1 = pRoute->GetWaypoint(0);
			const ::CWaypointRecordingRoute::RouteData & wpt2 = pRoute->GetWaypoint(1);
			const float fHdg = fwAngle::GetRadianAngleBetweenPoints(wpt2.GetPosition().x, wpt2.GetPosition().y, wpt1.GetPosition().x, wpt1.GetPosition().y);

			pPed->GetPrimaryMotionTask()->ForceReset();
			pPed->Teleport(wpt1.GetPosition(), fHdg);

			CTaskFollowWaypointRecording * pFollowTask = rage_new CTaskFollowWaypointRecording(pRoute);
			pFollowTask->SetLoopPlayback(CWaypointRecording::ms_bLoopRoutePlayback);
			pPed->GetPedIntelligence()->AddTaskAtPriority(pFollowTask, PED_TASK_PRIORITY_PRIMARY);
		}
	}
}
void TestPlayStreamedRecordingOnFocusPed()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{	
		TestPlayStreamedRecording(pFocus, CWaypointRecording::ms_iDebugStoreIndex);
	}
}
void TestPauseFocusPed()
{
	static dev_bool bStopBeforeOrientation = true;
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->SetPause(false, bStopBeforeOrientation);
		}
	}
}
void TestPauseFocusPedAndFacePlayer()
{
	static dev_bool bStopBeforeOrientation = true;
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->SetPause(true, bStopBeforeOrientation);
		}
	}
}
void TestResumeFocusPed()
{
	static int iOptionalDelayMs = 3000;
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->SetResume(true, -1, iOptionalDelayMs);
		}
	}
}
void TestOverrideSpeed()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->SetOverrideMoveBlendRatio(fOverrideMBR, true);
		}
	}
}
void TestUseDefaultSpeed()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->UseDefaultMoveBlendRatio();
		}
	}
}
void TestFollowRouteAimingAtPlayer()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus && pFocus != FindPlayerPed())
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->StartAimingAtEntity(FindPlayerPed(), bUseRunAndGun);
		}
	}
}
void TestStopAimingOrShooting()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->StopAimingOrShooting();
		}
	}
}
void TestFollowRouteShootingAtPlayer()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus && pFocus != FindPlayerPed())
	{
		CTask * pTask = pFocus->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			((CTaskFollowWaypointRecording*)pTask)->StartShootingAtEntity(FindPlayerPed(), bUseRunAndGun);
		}
	}
}
void ToggleUseAsAssistedMovementRoute()
{
	::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecordingBySlotIndex(CWaypointRecording::ms_iDebugStoreIndex);
	if(pRoute)
	{
		if(pRoute->GetFlags() & ::CWaypointRecordingRoute::RouteFlag_AssistedMovement)
		{
			pRoute->SetFlags(pRoute->GetFlags() & ~::CWaypointRecordingRoute::RouteFlag_AssistedMovement);
		}
		else
		{
			pRoute->SetFlags(pRoute->GetFlags() | ::CWaypointRecordingRoute::RouteFlag_AssistedMovement);
		}

		CLoadedWaypointRecording * pRecordingInfo = CWaypointRecording::GetLoadedWaypointRecordingInfo(pRoute);
		pRecordingInfo->SetAssistedMovementPathWidth(CWaypointRecording::ms_fAssistedMovementPathWidth);
		pRecordingInfo->SetAssistedMovementPathTension(CWaypointRecording::ms_fAssistedMovementPathTension * 2.0f);

		CAssistedMovementRouteStore::Process(FindPlayerPed(), true);
	}
}
#define NUM_SKI_PEDS	5

void SpawnSkiersForStreamedRecordings()
{
	const char * skiPeds[NUM_SKI_PEDS] =
	{
		"A_M_M_SkiVill_04",
		"A_M_M_SkiVill_06",
		"A_M_M_SkiVill_07",
		"A_M_M_SkiVill_08",
		"A_M_M_SkiVill_09"
	};

	for(int s=0; s<CWaypointRecording::ms_iMaxNumLoaded; s++)
	{
		::CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecordingBySlotIndex(s);
		if(pRoute && pRoute->GetSize() >= 2)
		{
			const ::CWaypointRecordingRoute::RouteData & wpt1 = pRoute->GetWaypoint(0);

			// Create a ped
			const char * pSkiPed = skiPeds[ fwRandom::GetRandomNumberInRange(0, NUM_SKI_PEDS) ];
			fwModelId modelId = CModelInfo::GetModelIdFromName(pSkiPed);
			if(modelId.IsValid())
			{
				// ensure that the model is loaded and ready for drawing for this ped
				if (!CModelInfo::HaveAssetsLoaded(modelId))
				{
					CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
					CStreaming::LoadAllRequestedObjects(true);
				}

				Matrix34 mPed;
				mPed.Identity();
				mPed.MakeTranslate(wpt1.GetPosition());
				mPed.RotateFullZ(wpt1.GetHeading());
				const CControlledByInfo pedControl(false, false);
				const bool bMissionChar = true;
				CPed * pPed = CPedFactory::GetFactory()->CreatePed(pedControl, modelId, &mPed, true, true, bMissionChar);

				pPed->PopTypeSet(POPTYPE_MISSION);
				pPed->SetDontLoadCollision(false);

				pPed->SetDefaultDecisionMaker();
				pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
				CGameWorld::Add(pPed, CGameWorld::OUTSIDE );

				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsSkiing, true );

				TestPlayStreamedRecording(pPed, s);
			}
		}
	}
}
void TestGetWaypointDistanceFromStart()
{
	static dev_s32 iIndex = 10;
	static float fDistance = 0.0f;
	if(CWaypointRecording::GetRecordingRoute()->GetSize() >= iIndex)
	{
		fDistance = CWaypointRecording::GetRecordingRoute()->GetWaypointDistanceFromStart(iIndex);
		Displayf("Distance : %.2f\n", fDistance);
	}
}
void TestGetPlayerDistanceAlongRoute()
{
	CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording("FBI1");
	if(pRoute && FindPlayerPed())
	{
		int i = pRoute->GetClosestWaypointIndex(VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()));
		if(i <=0 && i <pRoute->GetSize())
		{
			float fDistance = CWaypointRecording::GetRecordingRoute()->GetWaypointDistanceFromStart(i);
			Displayf("Distance : %.2f\n", fDistance);
		}
	}
}
#endif	// __BANK


//******************************************************************************************

CLoadedWaypointRecording::CLoadedWaypointRecording()
{
	Reset();
}

void CLoadedWaypointRecording::Reset()
{
	m_iFileNameHashKey = 0;
	m_pRecording = NULL;
	m_iNumReferences = 0;
	m_iScriptID = THREAD_INVALID;

	m_fAssistedMovementPathWidth = CAssistedMovementRoute::ms_fDefaultPathWidth;
	m_fAssistedMovementPathTension = CAssistedMovementRoute::ms_fDefaultPathTension;
	BANK_ONLY( m_Filename[0] = 0; )
}
//******************************************************************************************

static int GetMaxNumRecordingsPerLevel()
{
	// Make sure we're never trying to read this before it's been set.
	TrapZ(CWaypointRecording::ms_iMaxNumRecordingsPerLevel);
	return CWaypointRecording::ms_iMaxNumRecordingsPerLevel;
}


CWaypointRecordingStreamingInterface::CWaypointRecordingStreamingInterface() :
	strStreamingModule("wptrec", WAYPOINTRECORDING_FILE_ID, 0, false, false, ::CWaypointRecordingRoute::RORC_VERSION, 1024) { }


int CWaypointRecording::ms_iMaxNumRecordingsPerLevel = 0;

CWaypointRecording::CWaypointRecording()
{

}	

CWaypointRecording::~CWaypointRecording()
{

}

void CWaypointRecording::RegisterStreamingModule()
{
	// This is a little odd - this is basically the first time we need the size, so we get
	// it from the configuration data here. Maybe would be cleaner to add some function that
	// gets called after the configuration file has loaded.
	if(!ms_iMaxNumRecordingsPerLevel)
	{
		ms_iMaxNumRecordingsPerLevel = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("wptrec", 0x7809ca42), 256);
		m_StreamingInterface.SetCount(ms_iMaxNumRecordingsPerLevel);
	}

	strStreamingEngine::GetInfo().GetModuleMgr().AddModule(&m_StreamingInterface);
}

void CWaypointRecording::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

    if(initMode == INIT_CORE)
    {
		Assert(!m_StreamingInfo);

		const int cnt = GetMaxNumRecordingsPerLevel();

		m_StreamingInfo = rage_new TStreamingInfo[cnt];

	    for(int i=0; i<cnt; i++)
	    {
		    m_StreamingInfo[i].m_iFilenameHashKey = 0;
#if !__FINAL
		    m_StreamingInfo[i].m_Filename[0] = 0;
#endif
	    }

#if __BANK
	    ms_RecordingRoute.AllocForRecording();
#endif

    }
}

void CWaypointRecording::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
	    for(int s=0; s<ms_iMaxNumLoaded; s++)
	    {
		    if(ms_LoadedRecordings[s].m_pRecording)
		    {
			    int iStreamingIndex = GetStreamingIndexForRecording(ms_LoadedRecordings[s].m_iFileNameHashKey);
			    Assertf(iStreamingIndex != -1, "The requested waypoint route \'%s\' was not found in the .rpf", ms_LoadedRecordings[s].m_Filename);
			    if(iStreamingIndex==-1)
				    return;

			    m_StreamingInterface.ClearRequiredFlag(iStreamingIndex, STRFLAG_MISSION_REQUIRED);
			    m_StreamingInterface.StreamingRemove(strLocalIndex(iStreamingIndex));
		    }

		    ms_LoadedRecordings[s].Reset();
	    }
	}
	else if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		const int cnt = GetMaxNumRecordingsPerLevel();
	    for(int i=0; i<cnt; i++)
	    {
		    m_StreamingInfo[i].m_iFilenameHashKey = 0;
#if !__FINAL
		    m_StreamingInfo[i].m_Filename[0] = 0;
#endif
	    }
    }
	else if(shutdownMode == SHUTDOWN_CORE)
	{
		Assert(m_StreamingInfo);
		delete []m_StreamingInfo;
		m_StreamingInfo = NULL;
	}
}

#if __BANK
void CWaypointRecording::InitWidgets()
{
	bkBank *pBank = CGameLogic::GetGameLogicBank();

	if(pBank)
	{
		pBank->PushGroup("Waypoint Recordings");

		pBank->AddButton("Start recording on player ped", StartRecordingOnPlayerPed);
		pBank->AddButton("Start recording on player vehicle", StartRecordingOnPlayerVehicle);
		pBank->AddButton("Start recording on focus ped", StartRecordingOnFocusPed);
		pBank->AddButton("Stop recording", StopRecording);
		pBank->AddButton("Start playback on player ped", StartPlaybackOnPlayerPed);
		pBank->AddButton("Start playback on focus ped", StartPlaybackOnFocusPed);
		pBank->AddButton("Start playback on player vehicle", StartPlaybackOnPlayerVehicle);
		pBank->AddButton("Start playback on focus ped vehicle", StartPlaybackOnFocusPedVehicle);
		pBank->AddButton("Stop playback", StopPlayback);
		pBank->AddButton("Clear route", ClearRoute);

		pBank->AddToggle("Display routes", &CWaypointRecording::ms_bDebugDraw);
		pBank->AddToggle("Loop playback", &CWaypointRecording::ms_bLoopRoutePlayback);
		pBank->AddToggle("Use AI Slowdown", &CWaypointRecording::ms_bUseAISlowdown);
		//pBank->AddToggle("Use Bonnet Position", &s_bUseBonnetPositionForVehicleRecordings);

		pBank->PushGroup("Recording Params");
		//pBank->AddText("Route name: ", ms_RecordingRouteName, 64, false);
		pBank->AddButton("Save route", SaveRoute);
		pBank->AddButton("Load route", LoadRoute);
		pBank->PopGroup();

		pBank->PushGroup("Waypoint Editing");
		pBank->AddToggle("Enable Waypoint Editing", &CWaypointRecording::ms_bWaypointEditing);
		pBank->AddText("Last loaded route name:", &CWaypointRecording::m_LoadedRouteName[0], 255, true);
		pBank->AddText("Num waypoints:", &CWaypointRecording::m_iLoadedRouteNumWaypoints, true);
		pBank->AddSlider("Current Waypoint: ", &CWaypointRecording::ms_EditingControls.m_iSelectedWaypoint, 0, ::CWaypointRecordingRoute::ms_iMaxNumWaypoints, 1, CWaypointRecording::Edit_OnChangeWaypoint);

		pBank->AddButton("Warp player to this waypoint", CWaypointRecording::Edit_OnWarpToWaypoint);
		pBank->AddButton("Reposition this waypoint", CWaypointRecording::Edit_RepositionWaypoint);
		pBank->AddButton("Insert waypoint after", CWaypointRecording::Edit_InsertWaypoint);
		pBank->AddButton("Remove this waypoint", CWaypointRecording::Edit_RemoveWaypoint);
		pBank->AddSlider("Modify heading", &CWaypointRecording::ms_EditingControls.m_fWaypointHeading, 0.0, PI*2.0f, 0.1f, CWaypointRecording::Edit_OnModifyHeading);
		pBank->AddSlider("Modify MBR", &CWaypointRecording::ms_EditingControls.m_fWaypointMBR, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT, 0.1f, CWaypointRecording::Edit_OnModifyMBR);
		pBank->AddSlider("Modify Vehicle Speed", &CWaypointRecording::ms_EditingControls.m_fWaypointMBR, 0.0f, 64.0f, 0.25f, CWaypointRecording::Edit_OnModifyMBR);
		pBank->AddSlider("Modify Free Space to Left", &CWaypointRecording::ms_EditingControls.m_fFreeSpaceToLeft, 0.0f, 8.0f, 0.25f, CWaypointRecording::Edit_OnModifyDistLeft);
		pBank->AddSlider("Modify Free Space to Right", &CWaypointRecording::ms_EditingControls.m_fFreeSpaceToRight, 0.0f, 8.0f, 0.25f, CWaypointRecording::Edit_OnModifyDistRight);
		pBank->AddButton("Apply Free Space to Entire Route", CWaypointRecording::Edit_OnApplyAllDistances);
		pBank->AddButton("Snap route to ground", CWaypointRecording::Edit_OnSnapToGround);

		pBank->PushGroup("Nudge XYZ");
		pBank->AddButton("Increase X", CWaypointRecording::Edit_OnIncreaseX);
		pBank->AddButton("Decrease X", CWaypointRecording::Edit_OnDecreaseX);
		pBank->AddButton("Increase Y", CWaypointRecording::Edit_OnIncreaseY);
		pBank->AddButton("Decrease Y", CWaypointRecording::Edit_OnDecreaseY);
		pBank->AddButton("Increase Z", CWaypointRecording::Edit_OnIncreaseZ);
		pBank->AddButton("Decrease Z", CWaypointRecording::Edit_OnDecreaseZ);
		pBank->PopGroup();

		pBank->PushGroup("Translate Route");
		pBank->AddSlider("Relative X", &CWaypointRecording::ms_EditingControls.m_vTranslateRoute.x, -8192.0f, 8192.0f, 0.1f);
		pBank->AddSlider("Relative Y", &CWaypointRecording::ms_EditingControls.m_vTranslateRoute.y, -8192.0f, 8192.0f, 0.1f);
		pBank->AddSlider("Relative Z", &CWaypointRecording::ms_EditingControls.m_vTranslateRoute.z, -8192.0f, 8192.0f, 0.1f);
		pBank->AddButton("Translate Now", CWaypointRecording::Edit_OnTranslateNow);
		pBank->PopGroup();

		pBank->PushGroup("Waypoint Flags");
		pBank->AddToggle("Jump", &CWaypointRecording::ms_EditingControls.m_Flags.m_bJump, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Apply Jump Force", &CWaypointRecording::ms_EditingControls.m_Flags.m_bApplyJumpForce, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Land", &CWaypointRecording::ms_EditingControls.m_Flags.m_bLand, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Flip Backwards (Skiing)", &CWaypointRecording::ms_EditingControls.m_Flags.m_bFlipBackwards, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Flip Forwards (Skiing)", &CWaypointRecording::ms_EditingControls.m_Flags.m_bFlipForwards, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Drop Down (On-Foot)", &CWaypointRecording::ms_EditingControls.m_Flags.m_bDropDown, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Surfacing From Underwater (Swimming)", &CWaypointRecording::ms_EditingControls.m_Flags.m_bSurfacingFromUnderwater, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->AddToggle("Vehicle", &CWaypointRecording::ms_EditingControls.m_Flags.m_bVehicle, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		
		pBank->AddToggle("Non Interrupt", &CWaypointRecording::ms_EditingControls.m_Flags.m_bNonInterrupt, CWaypointRecording::Edit_CopyFlagsToWaypoint);
		pBank->PopGroup();

		pBank->PopGroup();

		pBank->PushGroup("Dev Controls");
		pBank->AddSlider("Max Waypoint Spacing (On-Foot)", &CWaypointRecording::ms_fMaxWaypointSpacingOnFoot, 1.0f, 40.0f, 1.0f);
		pBank->AddSlider("Max Waypoint Spacing (Skiing)", &CWaypointRecording::ms_fMaxWaypointSpacingOnSkis, 1.0f, 40.0f, 1.0f);
		pBank->AddSlider("Max Waypoint Spacing (Vehicle)", &CWaypointRecording::ms_fMaxWaypointSpacingInVehicle, 1.0f, 40.0f, 1.0f);
		pBank->AddSlider("Max Plane Dist (On-Foot)", &CWaypointRecording::ms_fMaxPlaneDistOnFoot, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Max Plane Dist (Skiing)", &CWaypointRecording::ms_fMaxPlaneDistOnSkis, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Max Plane Dist (Vehicle)", &CWaypointRecording::ms_fMaxPlaneDistInVehicle, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Max MBR Delta (Skiing)", &CWaypointRecording::ms_fMBRDeltaForNewWaypointOnSkis, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Max MBR Delta (On-Foot)", &CWaypointRecording::ms_fMBRDeltaForNewWaypointOnFoot, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Max Velocity Delta (Vehicle)", &CWaypointRecording::ms_fMBRDeltaForNewWaypointInVehicle, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Target Radius (On-Foot)", &CTaskFollowWaypointRecording::ms_fTargetRadius_OnFoot, 0.1f, 10.0f, 0.1f);
		pBank->AddSlider("Target Radius (Skiing)", &CTaskFollowWaypointRecording::ms_fTargetRadius_Skiing, 0.1f, 10.0f, 0.1f);
		pBank->AddButton("Test GetWaypointDistanceFromStart(10)", TestGetWaypointDistanceFromStart);
		pBank->AddButton("TestGetPlayerDistanceAlongRoute", TestGetPlayerDistanceAlongRoute);
		

		pBank->PushGroup("Ski-Specific");
		pBank->AddSlider("Heading tweak multiplier", &CTaskFollowWaypointRecording::ms_fSkiHeadingTweakMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Heading tweak (backwards) multiplier", &CTaskFollowWaypointRecording::ms_fSkiHeadingTweakBackwardsMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Heading tweak min speed-sqr", &CTaskFollowWaypointRecording::ms_fSkiHeadingTweakMinSpeedSqr, 0.0f, 100.0f, 0.1f);

		pBank->PopGroup();

		pBank->AddToggle("Force Jump Heading", &CTaskFollowWaypointRecording::ms_bForceJumpHeading);
		pBank->AddToggle("Apply Helper Forces", &CTaskFollowWaypointRecording::ms_bUseHelperForces);
		pBank->AddToggle("Debug Store", &CWaypointRecording::ms_bDisplayStore);
		pBank->AddSlider("Debug Store Index", &CWaypointRecording::ms_iDebugStoreIndex, 0, CWaypointRecording::ms_iMaxNumLoaded, 1);
		pBank->AddText("Test route", CWaypointRecording::ms_TestRouteName, 64, false);
		pBank->AddButton("Test : Stream route", TestStreamRoute);
		pBank->AddButton("Test : Remove route", TestRemoveRoute);
		pBank->AddButton("Test : Load all requested (streaming)", TestStreamingLoadAllRequested);
		pBank->AddButton("Test : Warp player to route start", WarpPlayerToRouteStart);
		pBank->AddButton("Test : Start playback on focus ped using streamed recording", TestPlayStreamedRecordingOnFocusPed);
		pBank->AddButton("Test : Spawn skiers for streamed recordings", SpawnSkiersForStreamedRecordings);
		pBank->AddButton("Test : Pause focus ped", TestPauseFocusPed);
		pBank->AddButton("Test : Pause focus ped and face player", TestPauseFocusPedAndFacePlayer);
		pBank->AddButton("Test : Resume focus ped", TestResumeFocusPed);
		pBank->AddSlider("Test : Override MBR", &fOverrideMBR, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT, 0.1f);

		pBank->AddButton("Test : Set override speed", TestOverrideSpeed);
		pBank->AddButton("Test : Set use default speed", TestUseDefaultSpeed);
		pBank->AddButton("Test : Follow route aiming at player", TestFollowRouteAimingAtPlayer);
		pBank->AddButton("Test : Follow route shooting at player", TestFollowRouteShootingAtPlayer);
		pBank->AddToggle("Test : Use \'Run and Gun\' for aiming/shooting", &bUseRunAndGun);
		pBank->AddButton("Test : Stop aiming or shooting", TestStopAimingOrShooting);

		pBank->PushGroup("Assisted Movement Integration");
		pBank->AddButton("Test : Toggle use as \'assisted-movement\' route", ToggleUseAsAssistedMovementRoute);
		pBank->AddSlider("Assisted Movement Path Width", &CWaypointRecording::ms_fAssistedMovementPathWidth, 0.0f, 16.0f, 0.5f);
		pBank->AddSlider("Assisted Movement Tension", &CWaypointRecording::ms_fAssistedMovementPathTension, 0.0f, 1.0f, 0.1f);

		pBank->PopGroup();

		pBank->PopGroup();

		pBank->PopGroup();
	}
}
void CWaypointRecording::StartPlayback(CPed * pPed, bool bPlaybackVehicle)
{
	Assert(!ms_bRecording);
	Assert(ms_RecordingRoute.GetSize());

	if(ms_RecordingRoute.GetSize() >= 2)
	{
		ms_pPed = pPed;

		const ::CWaypointRecordingRoute::RouteData & wpt1 = ms_RecordingRoute.GetWaypoint(0);
		const ::CWaypointRecordingRoute::RouteData & wpt2 = ms_RecordingRoute.GetWaypoint(1);
		const float fHdg = fwAngle::GetRadianAngleBetweenPoints(wpt2.GetPosition().x, wpt2.GetPosition().y, wpt1.GetPosition().x, wpt1.GetPosition().y);

		if (bPlaybackVehicle)
		{
			Assertf(pPed->GetMyVehicle(), "Trying to playback a vehicle waypoint recording on a ped with no vehicle!");
			if (pPed->GetMyVehicle())
			{	
				pPed->GetMyVehicle()->Teleport(wpt1.GetPosition(), fHdg);

				sVehicleMissionParams params;
				params.m_fCruiseSpeed = 64.0f;
				params.m_fTargetArriveDist = 2.0f;
				u32 iFlags = 0;
				if (ms_bUseAISlowdown)
				{
					iFlags |= CTaskFollowWaypointRecording::Flag_VehiclesUseAISlowdown;
				}
				CTaskVehicleFollowWaypointRecording* pFollowWaypointTask = rage_new CTaskVehicleFollowWaypointRecording(&ms_RecordingRoute, params, 0, iFlags);
				pFollowWaypointTask->SetLoopPlayback(ms_bLoopRoutePlayback);

				CTaskControlVehicle* pTask = rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pFollowWaypointTask);
				pPed->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY);
			}
		}
		else
		{
			pPed->GetPrimaryMotionTask()->ForceReset();
			pPed->Teleport(wpt1.GetPosition(), fHdg);

			CTaskFollowWaypointRecording * pFlwRec = rage_new CTaskFollowWaypointRecording(&ms_RecordingRoute);
			pFlwRec->SetLoopPlayback(ms_bLoopRoutePlayback);

			pPed->GetPedIntelligence()->AddTaskAtPriority(pFlwRec, PED_TASK_PRIORITY_PRIMARY);
		}
	}
}
void CWaypointRecording::StopPlayback()
{
	if(ms_pPed)
		ms_pPed->GetPedIntelligence()->ClearTasks();
}

void CWaypointRecording::StartRecording(CPed * pPed, bool bRecordVehicle)
{
	Assert(!ms_bRecording);

	ms_bRecording = true;
	ms_bWasFlipped = false;
	ms_bUnderwaterLastFrame = false;

	ms_bRecordingVehicle = bRecordVehicle;

	ms_pPed = pPed;

	ms_EditingControls.m_iSelectedWaypoint = 0;

	if (bRecordVehicle)
	{
		Assertf(pPed->GetMyVehicle(), "Trying to start a vehicle waypoint recording but not in a vehicle!");
		if (pPed->GetMyVehicle())
		{
			ms_RecordingRoute.AddWaypoint(VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pPed->GetMyVehicle(), false)), Max(1.0f, pPed->GetMyVehicle()->GetVelocity().Mag()), pPed->GetMyVehicle()->GetTransform().GetHeading(), ::CWaypointRecordingRoute::WptFlag_Vehicle);
		}
		else
		{
			ms_bRecording = false;
		}
	}
	else
	{
		Vector2 vMBR;
		pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMBR);
		ms_RecordingRoute.AddWaypoint(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), vMBR.y, pPed->GetCurrentHeading(), 0);
	}
}
void CWaypointRecording::StopRecording()
{
	ms_bRecording = false;
	ms_EditingControls.m_iSelectedWaypoint = 0;
}
void CWaypointRecording::ClearRoute()
{
	StopRecording();
	ms_RecordingRoute.Clear();
	CWaypointRecording::m_iLoadedRouteNumWaypoints = 0;
	CWaypointRecording::m_LoadedRouteName[0] = 0;
}

void CWaypointRecording::Edit_InitWaypointWidgets()
{
	if(ms_EditingControls.m_iSelectedWaypoint >= 0 && ms_EditingControls.m_iSelectedWaypoint < ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);

		ms_EditingControls.m_fWaypointHeading = wpt.GetHeading();
		ms_EditingControls.m_fWaypointMBR = wpt.GetSpeed();
		ms_EditingControls.m_Flags.m_bJump = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump) != 0;
		ms_EditingControls.m_Flags.m_bApplyJumpForce = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_ApplyJumpForce) != 0;
		ms_EditingControls.m_Flags.m_bLand = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Land) != 0;
		ms_EditingControls.m_Flags.m_bVehicle = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Vehicle) != 0;
		ms_EditingControls.m_Flags.m_bFlipBackwards = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SkiFlipBackwards) != 0;
		ms_EditingControls.m_Flags.m_bFlipForwards = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SkiFlipForwards) != 0;
		ms_EditingControls.m_Flags.m_bDropDown = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_DropDown) != 0;
		ms_EditingControls.m_Flags.m_bSurfacingFromUnderwater = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater) != 0;
		ms_EditingControls.m_Flags.m_bNonInterrupt = (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_NonInterrupt) != 0;
		ms_EditingControls.m_fFreeSpaceToLeft = wpt.GetFreeSpaceToLeft();
		ms_EditingControls.m_fFreeSpaceToRight = wpt.GetFreeSpaceToRight();

	}
}
void CWaypointRecording::Edit_CopyFlagsToWaypoint()
{
	if(ms_EditingControls.m_iSelectedWaypoint >= 0 && ms_EditingControls.m_iSelectedWaypoint < ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);

		u32 iFlags = 0;
		if(ms_EditingControls.m_Flags.m_bJump) iFlags |= ::CWaypointRecordingRoute::WptFlag_Jump;
		if(ms_EditingControls.m_Flags.m_bApplyJumpForce) iFlags |= ::CWaypointRecordingRoute::WptFlag_ApplyJumpForce;
		if(ms_EditingControls.m_Flags.m_bLand) iFlags |= ::CWaypointRecordingRoute::WptFlag_Land;
		if(ms_EditingControls.m_Flags.m_bVehicle) iFlags |= ::CWaypointRecordingRoute::WptFlag_Vehicle;
		if(ms_EditingControls.m_Flags.m_bFlipBackwards) iFlags |= ::CWaypointRecordingRoute::WptFlag_SkiFlipBackwards;
		if(ms_EditingControls.m_Flags.m_bFlipForwards) iFlags |= ::CWaypointRecordingRoute::WptFlag_SkiFlipForwards;
		if(ms_EditingControls.m_Flags.m_bDropDown) iFlags |= ::CWaypointRecordingRoute::WptFlag_DropDown;
		if(ms_EditingControls.m_Flags.m_bSurfacingFromUnderwater) iFlags |= ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater;
		if(ms_EditingControls.m_Flags.m_bNonInterrupt) iFlags |= ::CWaypointRecordingRoute::WptFlag_NonInterrupt;
		wpt.SetFlags(iFlags);
	}
}
void CWaypointRecording::Edit_OnChangeWaypoint()
{
	ms_EditingControls.m_iSelectedWaypoint = Min(ms_EditingControls.m_iSelectedWaypoint, ms_RecordingRoute.GetSize()-1);
	ms_EditingControls.m_iSelectedWaypoint = Max(ms_EditingControls.m_iSelectedWaypoint, 0);

	Edit_InitWaypointWidgets();
}
void CWaypointRecording::Edit_RepositionWaypoint()
{
	ms_EditingControls.ms_bRepositioningWaypoint = true;
}
void CWaypointRecording::Edit_InsertWaypoint()
{
	// To insert a waypoint, there must be two or more waypoints already in existence in this route..
	// The currently selected waypoint must not be the last in the route.  The new waypoint will be created between the
	// currently selected waypoint, and the one following it.

	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >= 0 && ms_EditingControls.m_iSelectedWaypoint < ms_RecordingRoute.GetSize())
	{
		int iIndex = ms_EditingControls.m_iSelectedWaypoint+1;
		bool bAddedOk = false;

		// Insert between two waypoints?
		if(iIndex < ms_RecordingRoute.GetSize()-1)
		{
			::CWaypointRecordingRoute::RouteData & prevWpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
			::CWaypointRecordingRoute::RouteData & nextWpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint+1);

			bAddedOk = ms_RecordingRoute.InsertWaypoint(
				iIndex,
				(prevWpt.GetPosition()+nextWpt.GetPosition())*0.5f,
				(prevWpt.GetSpeed()+nextWpt.GetSpeed())*0.5f,
				fwAngle::LimitRadianAngle( (prevWpt.GetHeading()+nextWpt.GetHeading())*0.5f ),
				0
			);
		}
		// Insert at end - extend route
		else
		{
			::CWaypointRecordingRoute::RouteData & prevWpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);

			static const float fExtendDist = 0.5f;
			Vector3 vExtendDir;
			float fHdg;

			if(ms_EditingControls.m_iSelectedWaypoint > 0)
			{
				::CWaypointRecordingRoute::RouteData & prevPrevWpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint-1);
				vExtendDir = prevWpt.GetPosition() - prevPrevWpt.GetPosition();
				vExtendDir.z = 0.0f;
				vExtendDir.Normalize();
				vExtendDir *= fExtendDist;

				fHdg = fwAngle::GetRadianAngleBetweenPoints(vExtendDir.x, vExtendDir.y, 0.0f, 0.0f);
			}
			else
			{
				vExtendDir = Vector3(-rage::Sinf(prevWpt.GetHeading()), rage::Cosf(prevWpt.GetHeading()), 0.0f) * fExtendDist;
				fHdg = prevWpt.GetHeading();
			}

			bAddedOk = ms_RecordingRoute.InsertWaypoint(
				iIndex,
				prevWpt.GetPosition() + vExtendDir,
				prevWpt.GetSpeed(),
				fwAngle::LimitRadianAngle(fHdg),
				0
			);
		}

		if(!bAddedOk)
		{
			Assertf(false, "Waypoint not inserted - maximum number of waypoints reached (%i)", ::CWaypointRecordingRoute::ms_iMaxNumWaypoints);
			return;
		}

		m_iLoadedRouteNumWaypoints = ms_RecordingRoute.GetSize();

		if(iIndex != -1)
		{
			ms_EditingControls.m_iSelectedWaypoint = iIndex;
			ms_EditingControls.m_iSelectedWaypoint = Min(ms_EditingControls.m_iSelectedWaypoint, ms_RecordingRoute.GetSize()-1);
			ms_EditingControls.m_iSelectedWaypoint = Max(ms_EditingControls.m_iSelectedWaypoint, 0);

			Edit_InitWaypointWidgets();
		}
	}
}
void CWaypointRecording::Edit_RemoveWaypoint()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1 && ms_RecordingRoute.GetSize() > 1 )
	{
		ms_RecordingRoute.RemoveWaypoint(ms_EditingControls.m_iSelectedWaypoint);

		ms_EditingControls.m_iSelectedWaypoint = Min(ms_EditingControls.m_iSelectedWaypoint, ms_RecordingRoute.GetSize()-1);
		ms_EditingControls.m_iSelectedWaypoint = Max(ms_EditingControls.m_iSelectedWaypoint, 0);

		m_iLoadedRouteNumWaypoints = ms_RecordingRoute.GetSize();

		Edit_InitWaypointWidgets();
	}
}
void CWaypointRecording::Edit_OnModifyHeading()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)		
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.SetHeading(ms_EditingControls.m_fWaypointHeading);
	}
}
void CWaypointRecording::Edit_OnModifyMBR()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)		
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.SetSpeed(ms_EditingControls.m_fWaypointMBR);
	}
}
void CWaypointRecording::Edit_OnModifyDistLeft()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)		
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.SetFreeSpaceToLeft(ms_EditingControls.m_fFreeSpaceToLeft);
	}
}
void CWaypointRecording::Edit_OnModifyDistRight()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)		
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.SetFreeSpaceToRight(ms_EditingControls.m_fFreeSpaceToRight);
	}
}
void CWaypointRecording::Edit_OnIncreaseX()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.m_fXYZ[0] += 0.1f;
	}
}
void CWaypointRecording::Edit_OnDecreaseX()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.m_fXYZ[0] -= 0.1f;
	}
}
void CWaypointRecording::Edit_OnIncreaseY()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.m_fXYZ[1] += 0.1f;
	}
}
void CWaypointRecording::Edit_OnDecreaseY()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.m_fXYZ[1] -= 0.1f;
	}
}
void CWaypointRecording::Edit_OnIncreaseZ()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.m_fXYZ[2] += 0.1f;
	}
}
void CWaypointRecording::Edit_OnDecreaseZ()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		wpt.m_fXYZ[2] -= 0.1f;
	}
}
void CWaypointRecording::Edit_OnTranslateNow()
{
	if(ms_bWaypointEditing)
	{
		for(int i=0; i<ms_RecordingRoute.GetSize(); i++)
		{
			::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(i);
			wpt.m_fXYZ[0] += ms_EditingControls.m_vTranslateRoute.x;
			wpt.m_fXYZ[1] += ms_EditingControls.m_vTranslateRoute.y;
			wpt.m_fXYZ[2] += ms_EditingControls.m_vTranslateRoute.z;
		}
	}
}
void CWaypointRecording::Edit_OnApplyAllDistances()
{
	if (ms_bWaypointEditing)
	{
		for (int i=0; i < ms_RecordingRoute.GetSize(); i++)
		{
			::CWaypointRecordingRoute::RouteData& wpt = ms_RecordingRoute.GetWaypoint(i);
			wpt.SetFreeSpaceToLeft(ms_EditingControls.m_fFreeSpaceToLeft);
			wpt.SetFreeSpaceToRight(ms_EditingControls.m_fFreeSpaceToRight);
		}
	}
}
void CWaypointRecording::Edit_OnWarpToWaypoint()
{
	if(ms_bWaypointEditing && ms_EditingControls.m_iSelectedWaypoint >=0 && ms_EditingControls.m_iSelectedWaypoint <= ms_RecordingRoute.GetSize()-1)
	{
		::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
		CPed * pPlayer = FindPlayerPed();
		if(pPlayer)
		{
			pPlayer->Teleport( wpt.GetPosition(), wpt.GetHeading() );
		}
	}
}
void CWaypointRecording::Edit_OnSnapToGround()
{
	if (ms_bWaypointEditing)
	{
		static dev_float s_fMaxSnap = 50.0f;

		for (int i=0; i < ms_RecordingRoute.GetSize(); i++)
		{
			::CWaypointRecordingRoute::RouteData& wpt = ms_RecordingRoute.GetWaypoint(i);
			Vector3 vPos = wpt.GetPosition();

			CStreaming::LoadAllRequestedObjects();
			CPhysics::LoadAboutPosition(RCC_VEC3V(vPos));
			CStreaming::LoadAllRequestedObjects();

			const float fNearestHeight = ThePaths.FindNearestGroundHeightForBuildPaths(vPos, s_fMaxSnap, NULL);
			if (rage::Abs(fNearestHeight - vPos.z) < s_fMaxSnap)
			{
				vPos.z = fNearestHeight;
				wpt.SetPosition(vPos);
			}
		}
	}
}
void CWaypointRecording::Debug()
{
#if DEBUG_DRAW
	if(ms_bDebugDraw)
	{
		camDebugDirector & debugDirector = camInterface::GetDebugDirector();
		Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
		char tmp[256];

		ms_RecordingRoute.Draw(true);

		for(int s=0; s<ms_iMaxNumLoaded; s++)
		{
			if(ms_LoadedRecordings[s].m_pRecording)
			{
				Vector3 vClosestPos = ms_LoadedRecordings[s].m_pRecording->GetClosestPosition(vOrigin);
				sprintf(tmp, "routename : %s", ms_LoadedRecordings[s].m_Filename);
				grcDebugDraw::Text(vClosestPos + ZAXIS, Color_green, tmp);

				ms_LoadedRecordings[s].m_pRecording->Draw(false);
			}
		}

		if(ms_bWaypointEditing)
		{
			if(ms_EditingControls.m_iSelectedWaypoint >= 0 && ms_EditingControls.m_iSelectedWaypoint < ms_RecordingRoute.GetSize())
			{
				const ::CWaypointRecordingRoute::RouteData & editWpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
				Vector3 vEditPos = editWpt.GetPosition() + Vector3(0.0f,0.0f,2.0f);
				const Color32 iEditCol1 = Color_white;
				const Color32 iEditCol2 = Color_magenta3;

				static float fEditBounceTheta = 0.0f;
				static float fEditBounceRot = 0.0f;

				fEditBounceTheta += fwTimer::GetTimeStep() * 3.0f;
				while(fEditBounceTheta > TWO_PI) fEditBounceTheta -= TWO_PI;
				fEditBounceRot += fwTimer::GetTimeStep() * 3.0f;
				while(fEditBounceRot > TWO_PI) fEditBounceRot -= TWO_PI;

				vEditPos.z += rage::Sinf(fEditBounceTheta) * 0.5f;

				Vector3 vArrowCoords[] =
				{
					Vector3(0.0f, 0.25f, 1.0f),
					Vector3(0.25f, 0.0f, 1.0f),
					Vector3(0.0f, -0.25f, 1.0f),
					Vector3(-0.25f, 0.0f, 1.0f)
				};
				Matrix34 rot;
				rot.Identity();
				rot.MakeRotateZ(fEditBounceRot);
				rot.Transform(vArrowCoords[0]);
				rot.Transform(vArrowCoords[1]);
				rot.Transform(vArrowCoords[2]);
				rot.Transform(vArrowCoords[3]);

				grcDebugDraw::Line(vEditPos, vEditPos+vArrowCoords[0], iEditCol1, iEditCol2);
				grcDebugDraw::Line(vEditPos, vEditPos+vArrowCoords[1], iEditCol1, iEditCol2);
				grcDebugDraw::Line(vEditPos, vEditPos+vArrowCoords[2], iEditCol1, iEditCol2);
				grcDebugDraw::Line(vEditPos, vEditPos+vArrowCoords[3], iEditCol1, iEditCol2);

				grcDebugDraw::Line(vEditPos+vArrowCoords[0], vEditPos+vArrowCoords[1], iEditCol2, iEditCol2);
				grcDebugDraw::Line(vEditPos+vArrowCoords[1], vEditPos+vArrowCoords[2], iEditCol2, iEditCol2);
				grcDebugDraw::Line(vEditPos+vArrowCoords[2], vEditPos+vArrowCoords[3], iEditCol2, iEditCol2);
				grcDebugDraw::Line(vEditPos+vArrowCoords[3], vEditPos+vArrowCoords[0], iEditCol2, iEditCol2);
			}
		}
	}

	if(ms_bDisplayStore)
	{
		for(int s=0; s<ms_iMaxNumLoaded; s++)
		{
			if(ms_LoadedRecordings[s].m_Filename[0])
			{
				grcDebugDraw::AddDebugOutput(Color_LightYellow, "%i) \"%s\" loaded=%s numrefs=%i", s, ms_LoadedRecordings[s].m_Filename, ms_LoadedRecordings[s].m_pRecording ? "true":"false", ms_LoadedRecordings[s].m_iNumReferences);
			}
			else
			{
				grcDebugDraw::AddDebugOutput(Color_LightYellow, "%i) ---", s);
			}
		}
	}
#endif // DEBUG_DRAW
}

void CWaypointRecording::UpdateRecording()
{
	// Handle the movement of individual waypoints
	if(ms_bWaypointEditing && !ms_bRecording)
	{
		Vector3 mouseScreen, mouseFar;
		CDebugScene::GetMousePointing(mouseScreen, mouseFar);

		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<> probeResult;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(mouseScreen, mouseFar);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			//****************************************
			// Handle changing the selected waypoint

			if(!ms_EditingControls.ms_bRepositioningWaypoint)
			{
				if(ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)
				{
					int iPrevSel = ms_EditingControls.m_iSelectedWaypoint;

					static const float fSelectEpsSqr = 4.0f;
					float fMinDistSqr = FLT_MAX;
					for(int w=0; w<ms_RecordingRoute.GetSize(); w++)
					{
						::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(w);
						const Vector3 vDiff = probeResult[0].GetHitPosition() - wpt.GetPosition();
						if(vDiff.Mag2() < fSelectEpsSqr && vDiff.Mag2() < fMinDistSqr)
						{
							ms_EditingControls.m_iSelectedWaypoint = w;
							fMinDistSqr = vDiff.Mag2();
						}
					}

					if(ms_EditingControls.m_iSelectedWaypoint != iPrevSel)
						Edit_OnChangeWaypoint();
				}
			}

			//*************************************
			// Process repositioning the waypoint

			if(ms_EditingControls.ms_bRepositioningWaypoint)
			{
				Vector3 mouseScreen, mouseFar;
				CDebugScene::GetMousePointing(mouseScreen, mouseFar);

				WorldProbe::CShapeTestProbeDesc probeDesc;
				WorldProbe::CShapeTestFixedResults<> probeResult;
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetStartAndEnd(mouseScreen, mouseFar);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					if(ms_EditingControls.m_iSelectedWaypoint >= 0 && ms_EditingControls.m_iSelectedWaypoint < ms_RecordingRoute.GetSize())
					{
						::CWaypointRecordingRoute::RouteData & wpt = ms_RecordingRoute.GetWaypoint(ms_EditingControls.m_iSelectedWaypoint);
						wpt.SetPosition(probeResult[0].GetHitPosition());
					}
				}

				if(ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)
					ms_EditingControls.ms_bRepositioningWaypoint = false;
			}
		}
	}
	if(!ms_bRecording)
		return;

	// Handle the recording

	Assert(ms_RecordingRoute.GetSize() > 0);
	const ::CWaypointRecordingRoute::RouteData & waypointB = ms_RecordingRoute.GetWaypoint(ms_RecordingRoute.GetSize()-1);
	const ::CWaypointRecordingRoute::RouteData & waypointA = ms_RecordingRoute.GetWaypoint(ms_RecordingRoute.GetSize() > 1 ? ms_RecordingRoute.GetSize()-2 : 0);

	CVehicle* pVehicle = ms_bRecordingVehicle ? ms_pPed->GetMyVehicle() : NULL;
	const float fVehicleVel = pVehicle ? pVehicle->GetVelocity().Mag() : 0.0f;

	Vector2 vMBR;
	ms_pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMBR);

	//don't let our velocity go under 1, since for vehicles it may cause them to stop moving
	const float fMBRToUse = ms_bRecordingVehicle ? Max(fVehicleVel, 1.0f) : vMBR.y;

	const float fDeltaMBR = fMBRToUse - waypointB.GetSpeed();

	CEntity* pTestEntity = ms_bRecordingVehicle ? static_cast<CEntity*>(pVehicle) : static_cast<CEntity*>(ms_pPed);

	Vector3 vTestPosition = ms_bRecordingVehicle ? VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, false))
		: VEC3V_TO_VECTOR3(pTestEntity->GetTransform().GetPosition());

	Vector3 vB = waypointB.GetPosition();
	Vector3 vOtherB = VEC3V_TO_VECTOR3(pTestEntity->GetTransform().GetB());
	Vector3 vA = (ms_RecordingRoute.GetSize() > 1) ? waypointA.GetPosition() : vB - vOtherB;
	Vector3 vC = vTestPosition;

	Vector3 vBC = vC - vB;
	const float fDist = NormalizeAndMag(vBC);

	vA.z = vB.z = vC.z;
	const float fDistToLine = geomDistances::DistanceLineToPoint(vA, vB-vA, vC);

	bool bJustJumped = false;
	bJustJumped = (ms_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) && (!(waypointB.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump)));

	const float fMaxWptSpacing = ms_bRecordingVehicle ? ms_fMaxWaypointSpacingInVehicle : ms_fMaxWaypointSpacingOnFoot;
	const float fMaxPlaneDist =  ms_bRecordingVehicle ? ms_fMaxPlaneDistInVehicle : ms_fMaxPlaneDistOnFoot;
	const float fMaxDeltaMBR = ms_bRecordingVehicle ? ms_fMBRDeltaForNewWaypointInVehicle :  ms_fMBRDeltaForNewWaypointOnFoot;

	const bool bAddPointNormal =
		((Abs(fDeltaMBR) > fMaxDeltaMBR || Abs(fDistToLine) > fMaxPlaneDist || fDist > fMaxWptSpacing)
		&& fDist > ms_fMinWaypointSpacing);

	const bool bInAir = ms_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);

	const bool bJustLanded = (waypointB.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump) && !bInAir
		&& !ms_pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping);

	const bool bSwimmingOnSurface = ms_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming) &&
		ms_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest() &&
		ms_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_MOTION_SWIMMING;

	const bool bSwimmingUnderwater = ms_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming) &&
		ms_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest() &&
		ms_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_MOTION_DIVING;

	bool bFlip = false;

	// We can't land & jump on the same waypoint.  If we cancel the bJustJumped flag here,
	// we will most likely detect the jump again the following frame.
	if(bJustLanded)
		bJustJumped = false;

	//***************************
	// Maybe add a waypoint here

	if((bAddPointNormal && !bInAir && !ms_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping )) || bJustJumped || bJustLanded || bFlip)
	{
		bool bAddWpt = true;
		if(ms_RecordingRoute.GetSize() > 0 && !bJustJumped && !bJustLanded)
		{
			const ::CWaypointRecordingRoute::RouteData & lastWpt = ms_RecordingRoute.GetWaypoint( ms_RecordingRoute.GetSize()-1 );
			const Vector3 vLastPt = lastWpt.GetPosition();
			if((vTestPosition - vLastPt).Mag2() < 1.0f)
				bAddWpt = false;
		}

		u16 iFlags = 0;
		if(bJustJumped)
			iFlags |= ::CWaypointRecordingRoute::WptFlag_Jump;
		else if(bJustLanded)
			iFlags |= ::CWaypointRecordingRoute::WptFlag_Land;
		if(bFlip)
		{
			if(!ms_bWasFlipped)
				iFlags |= ::CWaypointRecordingRoute::WptFlag_SkiFlipBackwards;
			else
				iFlags |= ::CWaypointRecordingRoute::WptFlag_SkiFlipForwards;
		}

		if (ms_bRecordingVehicle)
		{
			iFlags |= ::CWaypointRecordingRoute::WptFlag_Vehicle;
		}

		if(bAddWpt)
		{
			if(bSwimmingOnSurface)
			{
				if(ms_bUnderwaterLastFrame)
				{
					iFlags |= ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater;
					vTestPosition += Vector3(0.0f, 0.0f, 1.0f);
				}
				vTestPosition += Vector3(0.0f, 0.0f, 1.0f);
			}
			else
			{
				vTestPosition -= Vector3(0.0f, 0.0f, 1.0f);
			}

			//if this is a vehicle waypoint recording for a car or bike, not a boat or a plane 
			//(helis are planes--helis planes and boats are also technically cars)
			//then snap the position to ground.
			//I decided to do this here rather than in a second pass since we can be reasonably
			//sure that collision is loaded for this point right now
			if (ms_bRecordingVehicle && (pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBike()) 
				&& !pVehicle->InheritsFromBoat() && !pVehicle->InheritsFromPlane())
			{
				static dev_float s_fMaxSnap = 50.0f;
				const float fNearestHeight = ThePaths.FindNearestGroundHeightForBuildPaths(vTestPosition, s_fMaxSnap, NULL);
				if (rage::Abs(fNearestHeight - vTestPosition.z) < s_fMaxSnap)
				{
					vTestPosition.z = fNearestHeight;
				}
			}

			const float fHeadingToUse = ms_bRecordingVehicle ? pVehicle->GetTransform().GetHeading() : ms_pPed->GetCurrentHeading();
			ms_RecordingRoute.AddWaypoint(vTestPosition, fMBRToUse, fHeadingToUse, iFlags);

			ms_bUnderwaterLastFrame = bSwimmingUnderwater;
		}
	}

	// Toggle the flipped state
	if(bFlip)
		ms_bWasFlipped = !ms_bWasFlipped;
}

#endif	// __BANK

bool CWaypointRecording::CheckWaypointRecordingName(const char * ASSERT_ONLY(pRecordingName))
{
#if __ASSERT
	Assertf(pRecordingName && *pRecordingName, "Recording name was NULL");
	if(!pRecordingName || !*pRecordingName)
		return false;

	// Ensure that the recording name is free of any path separators, or periods.
	int iStrLen = istrlen(pRecordingName);

	for(int c=0; c<iStrLen; c++)
	{
		/*
		if(pRecordingName[c] == '\'' || pRecordingName[c] == '/' || pRecordingName[c] == '.' || pRecordingName[c] == ':')
		{
			Assertf(false, "Recording name must not include the path, or file extension");
			return false;
		}
		*/
		if(pRecordingName[c] == ' ' || pRecordingName[c] == '\t')
		{
			Assertf(false, "Recording name must not include spaces or tabs");
			return false;
		}
	}

#endif
	return true;
}

int CWaypointRecording::GetFreeRecordingSlot()
{
	for(int r=0; r<ms_iMaxNumLoaded; r++)
	{
		if(ms_LoadedRecordings[r].m_iFileNameHashKey==0)
		{
			return r;
		}
	}
	return -1;
}
int CWaypointRecording::FindExistingRecordingSlot(const u32 iHash)
{
	for(int r=0; r<ms_iMaxNumLoaded; r++)
	{
		if(ms_LoadedRecordings[r].m_iFileNameHashKey==iHash)
		{
			return r;
		}
	}
	return -1;
}
int CWaypointRecording::GetStreamingIndexForRecording(const u32 iHash)
{
	const int cnt = GetMaxNumRecordingsPerLevel();
	for(int i=0; i<cnt; i++)
	{
		if(m_StreamingInfo[i].m_iFilenameHashKey==iHash)
		{
			return i;
		}
	}
	return -1;
}
int CWaypointRecording::FindSlotForPointer(const ::CWaypointRecordingRoute * pRec)
{
	Assert(pRec);
	if(!pRec)
		return -1;

	for(int r=0; r<ms_iMaxNumLoaded; r++)
	{
		if(ms_LoadedRecordings[r].m_pRecording==pRec)
		{
			return r;
		}
	}
	return -1;
}

void CWaypointRecording::RequestRecording(const char * pRecordingName, const scrThreadId iScriptID)
{
	const u32 iHash = atStringHash(pRecordingName);

	//******************************************************
	// Find the streaming index of this waypoint recording

	int iStreamingIndex = GetStreamingIndexForRecording(iHash);
	Assertf(iStreamingIndex != -1, "The requested waypoint route \'%s\' was not found in the .rpf", pRecordingName);
	if(iStreamingIndex==-1)
		return;

	//***************************************************
	// Try to obtain a slot for loading this recording

	int s = FindExistingRecordingSlot(iHash);
	if(s != -1)
	{
		if(ms_LoadedRecordings[s].m_pRecording)
		{
			//Assertf(false, "recording \'%s\' is already loaded", pRecordingName);
			return;
		}
#if __ASSERT && 0
		GtaThread * pThread = ms_LoadedRecordings[s].m_iScriptID ? static_cast<GtaThread*>(scrThread::GetThread(ms_LoadedRecordings[s].m_iScriptID)) : NULL;
		Assert(ms_LoadedRecordings[s].m_iNumReferences==0);
		Assertf(ms_LoadedRecordings[s].m_iScriptID==iScriptID, "ERROR - Cannot request the same recording from different scripts.  Memory will leak.  Recording is \"%s\", this script is \"%s\", Original script is \"%s\"", pRecordingName, CTheScripts::GetCurrentScriptName(), pThread ? pThread->GetScriptName() : "NULL" );
#endif
	}
	else
	{
		s = GetFreeRecordingSlot();
		if(s == -1)
		{
			Assertf(false, "CWaypointRecording - no free slots to load recording \'%s\'.", pRecordingName);
			return;
		}
		Assert(ms_LoadedRecordings[s].m_iFileNameHashKey==0);
	}

	//*******************
	// Set up the slot

	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];

	Assert(rec.m_iNumReferences==0 && rec.m_pRecording==NULL);

	rec.m_iFileNameHashKey = iHash;
	rec.m_iNumReferences = 0;
	rec.m_pRecording = NULL;
	rec.m_iScriptID = iScriptID;
#if __BANK
	strncpy(rec.m_Filename, pRecordingName, 64);
#endif

	//*****************************
	// Issue a streaming request

	s32 flags = STRFLAG_FORCE_LOAD|STRFLAG_MISSION_REQUIRED;
	CTheScripts::PrioritizeRequestsFromMissionScript(flags);
	m_StreamingInterface.StreamingRequest(strLocalIndex(iStreamingIndex), flags);
}
void CWaypointRecording::RemoveRecording(const char * pRecordingName, const scrThreadId UNUSED_PARAM(iScriptID))
{
	const u32 iHash = atStringHash(pRecordingName);

	//******************************************************
	// Find the streaming index of this waypoint recording

	int iStreamingIndex = GetStreamingIndexForRecording(iHash);
	Assertf(iStreamingIndex != -1, "The requested waypoint route \'%s\' was not found in the .rpf", pRecordingName);
	if(iStreamingIndex==-1)
		return;

	//*****************************************************************************************************
	// If an assisted movement route has been created from this waypoint recording, then ensure that this
	// assisted movement route gets removed at the same time.  We clear the flag, and force an update.

	::CWaypointRecordingRoute * pRecording = GetRecording(pRecordingName);
	if(pRecording)
	{
		u32 iFlags = pRecording->GetFlags();
		if(iFlags & ::CWaypointRecordingRoute::RouteFlag_AssistedMovement)
		{
			iFlags &= ~::CWaypointRecordingRoute::RouteFlag_AssistedMovement;
			pRecording->SetFlags(iFlags);
			CAssistedMovementRouteStore::Process(FindPlayerPed(), true);
		}
	}

	//****************************
	// Issue a streaming remove..

	//	m_pRecording for the ms_LoadedRecordings entry will probably get cleared inside StreamingRemove
	//	so we have to search for the correct slot before calling StreamingRemove
	if (pRecording)
	{
		int s = FindSlotForPointer(pRecording);
		if(s != -1)
		{
			ms_LoadedRecordings[s].m_iScriptID = THREAD_INVALID;
		}
	}

	m_StreamingInterface.ClearRequiredFlag(iStreamingIndex, STRFLAG_MISSION_REQUIRED);
	m_StreamingInterface.StreamingRemove(strLocalIndex(iStreamingIndex));
}
void CWaypointRecording::RemoveAllRecordingsRequestedByScript(const scrThreadId iScriptID)
{
	for(int s=0; s<ms_iMaxNumLoaded; s++)
	{
		if(ms_LoadedRecordings[s].m_pRecording && ms_LoadedRecordings[s].m_iScriptID==iScriptID)
		{
			int iStreamingIndex = GetStreamingIndexForRecording(ms_LoadedRecordings[s].m_iFileNameHashKey);
			Assertf(iStreamingIndex != -1, "The requested waypoint route \'%s\' was not found in the .rpf", ms_LoadedRecordings[s].m_Filename);
			if(iStreamingIndex==-1)
				return;

			ms_LoadedRecordings[s].m_iScriptID = THREAD_INVALID;

			m_StreamingInterface.ClearRequiredFlag(iStreamingIndex, STRFLAG_MISSION_REQUIRED);
			m_StreamingInterface.StreamingRemove(strLocalIndex(iStreamingIndex));
		}
	}
}
bool CWaypointRecording::IsRecordingLoaded(const char * pRecordingName)
{
	const u32 iHash = atStringHash(pRecordingName);

	int s = FindExistingRecordingSlot(iHash);
	if(s == -1)
	{
		return false;
	}
	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];
	return (rec.m_pRecording != NULL);
}
bool CWaypointRecording::IsRecordingRequested(const char * pRecordingName)
{
	const u32 iHash = atStringHash(pRecordingName);

	int s = FindExistingRecordingSlot(iHash);
	if(s == -1)
	{
		return false;
	}
	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];
	return (rec.m_pRecording == NULL);
}
bool CWaypointRecording::IncReferencesToRecording(const char * pRecordingName)
{
	const u32 iHash = atStringHash(pRecordingName);

	int s = FindExistingRecordingSlot(iHash);
	if(s == -1)
	{
		Assertf(false, "Waypoint recording \'%s\' not found", pRecordingName);
		return false;
	}
	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];
	if(rec.m_pRecording == NULL)
	{
		Assertf(false, "Waypoint recording \'%s\' not yet loaded", pRecordingName);
		return false;
	}
	rec.m_iNumReferences++;
	return true;
}
bool CWaypointRecording::IncReferencesToRecording(const ::CWaypointRecordingRoute * pRoute)
{
	if(!pRoute)
	{
		return false;
	}
	int s = FindSlotForPointer(pRoute);
	if(s == -1)
	{
		return false;
	}
	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];
	if(rec.m_pRecording == NULL)
	{
		Assertf(false, "recording not yet loaded");
		return false;
	}
	rec.m_iNumReferences++;
	return true;
}
bool CWaypointRecording::DecReferencesToRecording(const char * pRecordingName)
{
	const u32 iHash = atStringHash(pRecordingName);

	int s = FindExistingRecordingSlot(iHash);
	if(s == -1)
	{
		// Not an error - this can happed when the game session is shutdown and recordings
		// are removed whilst peds are still playing tasks which reference them.
		//Assertf(false, "recording \'%s\' not found", pRecordingName);
		return false;
	}
	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];
	if(rec.m_pRecording == NULL)
	{
		Assertf(false, "recording \'%s\' not yet loaded", pRecordingName);
		return false;
	}

#if AI_OPTIMISATIONS_OFF
	Assertf(rec.m_iNumReferences > 0, "Dec-ref called on recording \'%s\' whose num refs is %i", pRecordingName, rec.m_iNumReferences);
#endif

	rec.m_iNumReferences--;
	rec.m_iNumReferences = Max(rec.m_iNumReferences, 0);
	return true;
}
bool CWaypointRecording::DecReferencesToRecording(const ::CWaypointRecordingRoute * pRoute)
{
	if(!pRoute)
	{
		return false;
	}
	int s = FindSlotForPointer(pRoute);
	if(s == -1)
	{
		//Assertf(false, "recording not found");
		return false;
	}
	CLoadedWaypointRecording & rec = ms_LoadedRecordings[s];
	if(rec.m_pRecording == NULL)
	{
		Assertf(false, "recording not yet loaded");
		return false;
	}

#if AI_OPTIMISATIONS_OFF
	Assertf(rec.m_iNumReferences > 0, "Dec-ref called on recording \'%p\' whose num refs is %i", pRoute, rec.m_iNumReferences);
#endif

	rec.m_iNumReferences--;
	return true;
}
::CWaypointRecordingRoute * CWaypointRecording::GetRecording(const char * pRecordingName)
{
	const u32 iHash = atStringHash(pRecordingName);
	int s = FindExistingRecordingSlot(iHash);
	if(s != -1)
	{
		return ms_LoadedRecordings[s].m_pRecording;
	}
	return NULL;
}
::CWaypointRecordingRoute * CWaypointRecording::GetRecordingBySlotIndex(const int iSlotIndex)
{
	if(iSlotIndex >= 0 && iSlotIndex < ms_iMaxNumLoaded)
	{
		return ms_LoadedRecordings[iSlotIndex].m_pRecording;
	}
	return NULL;
}
CLoadedWaypointRecording * CWaypointRecording::GetLoadedWaypointRecordingInfo(const ::CWaypointRecordingRoute * pRecording)
{
	return GetLoadedWaypointRecordingInfo(FindSlotForPointer(pRecording));
}
CLoadedWaypointRecording * CWaypointRecording::GetLoadedWaypointRecordingInfo(const int iSlot)
{
	if(iSlot >= 0)
	{
		return &ms_LoadedRecordings[iSlot];
	}
	return NULL;
}
void CWaypointRecording::SetRecordingFromStreamingIndex(s32 iStreamingIndex, ::CWaypointRecordingRoute * pRec)
{
	Assert(iStreamingIndex >= 0 && iStreamingIndex < GetMaxNumRecordingsPerLevel());
	const u32 iHashKey = m_StreamingInfo[iStreamingIndex].m_iFilenameHashKey;

	Assert(iHashKey);
	if(!iHashKey)
		return;

	for(int s=0; s<ms_iMaxNumLoaded; s++)
	{
		if(ms_LoadedRecordings[s].m_iFileNameHashKey == iHashKey)
		{
			Assertf(ms_LoadedRecordings[s].m_pRecording == NULL, "Setting recording into streaming slot - but m_pRecording wasn't NULL");
			Assertf(ms_LoadedRecordings[s].m_iNumReferences==0, "Setting recording into streaming slot - but num refs wasn't zero");

			ms_LoadedRecordings[s].m_pRecording = pRec;
			ms_LoadedRecordings[s].m_iNumReferences = 0;
			return;
		}
	}
	Assertf(false, "Couldn't find the streaming slot in which to put newly loaded waypoint-recording..");
}


//*************************************************************************************
// Implement the streaming interface

#if !__FINAL
const char* CWaypointRecordingStreamingInterface::GetName(strLocalIndex index) const
{
	return CWaypointRecording::m_StreamingInfo[index.Get()].m_Filename;
}
#endif

strLocalIndex CWaypointRecordingStreamingInterface::FindSlot(const char* name) const
{
	u32 nameHash = atStringHash(name);
	const int cnt = GetMaxNumRecordingsPerLevel();
	for(int i=0; i<cnt; i++)
		if(CWaypointRecording::m_StreamingInfo[i].m_iFilenameHashKey==nameHash)
			return strLocalIndex(i);
	return strLocalIndex(-1);
}

strLocalIndex CWaypointRecordingStreamingInterface::Register(const char* name)
{
//	Assert(FindSlot(name) == -1);

//	When starting a new session, the name will already be in this array so clear its entry now.
//	During development, someone could add a waypoint to the .rpf and use askBeforeRestart to
//	start a new session. In this case, the indices for all the following waypoint files will be one higher
//	than in the previous session. The code below doesn't handle the case of a waypoint file being removed
//	from the rpf between sessions.
//	In all other cases, the index for a waypoint file should be identical between sessions.
	int ExistingSlot = FindSlot(name).Get();
	if (ExistingSlot >= 0)
	{
		CWaypointRecording::m_StreamingInfo[ExistingSlot].m_iFilenameHashKey = 0;
#if !__FINAL
		CWaypointRecording::m_StreamingInfo[ExistingSlot].m_Filename[0] = 0;
#endif
	}

	const int cnt = GetMaxNumRecordingsPerLevel();
	for(int i=0; i<cnt; i++)
	{
		if(CWaypointRecording::m_StreamingInfo[i].m_iFilenameHashKey==0)
		{
			CWaypointRecording::m_StreamingInfo[i].m_iFilenameHashKey = atStringHash(name);
			
#if !__FINAL
			strncpy(CWaypointRecording::m_StreamingInfo[i].m_Filename, name, 64);
#endif

#if USE_PAGED_POOLS_FOR_STREAMING
			AllocateSlot(i);
#endif // USE_PAGED_POOLS_FOR_STREAMING

			return strLocalIndex(i);
		}
	}
	Assertf(false, "Too many waypoint recordings in this level, max is %d", GetMaxNumRecordingsPerLevel());
	return strLocalIndex(-1);
}
void CWaypointRecordingStreamingInterface::Remove(strLocalIndex index)
{
	const u32 iHashKey = CWaypointRecording::m_StreamingInfo[index.Get()].m_iFilenameHashKey;
	Assertf(iHashKey!=0, "Filename hashkey was zero - recording isn't loaded");

	for(int l=0; l<CWaypointRecording::ms_iMaxNumLoaded; l++)
	{
		if(CWaypointRecording::ms_LoadedRecordings[l].m_iFileNameHashKey == iHashKey)
		{
			//Assertf(CWaypointRecording::ms_LoadedRecordings[l].m_iNumReferences==0, "Deleting waypoint recording \'%s\', but a ped was still following this route - the task may crash (numrefs: %i)", CWaypointRecording::m_StreamingInfo[index].m_Filename, CWaypointRecording::ms_LoadedRecordings[l].m_iNumReferences);

			//**********************************************************************************************
			// If this route is being removed, and peds are using it - then find which peds are running the
			// waypoint-following task, and abort the task.

			// TODO: Fix this, see (url:bugstar:1402103)

			if(CWaypointRecording::ms_LoadedRecordings[l].m_iNumReferences)
			{
				CTask::Pool * pTaskPool = CTask::GetPool();
				for(s32 t=0; t<pTaskPool->GetSize(); t++)
				{
					CTask * pTask = pTaskPool->GetSlot(t);
					if(pTask)
					{
						if(pTask->GetTaskType()==CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING)
						{
							CTaskFollowWaypointRecording * pFollowTask = ((CTaskFollowWaypointRecording*)pTask);
							
							if(pFollowTask->GetRoute()==CWaypointRecording::ms_LoadedRecordings[l].m_pRecording)
							{
								if(pFollowTask->GetEntity())
									pFollowTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL);
								pFollowTask->ClearRoute();
								pFollowTask->SetQuitImmediately();
							}
						}
						else if(pTask->GetTaskType()==CTaskTypes::TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING)
						{
							CTaskVehicleFollowWaypointRecording* pFollowWaypointTask = ((CTaskVehicleFollowWaypointRecording*)pTask);
							if (pFollowWaypointTask->GetRoute()==CWaypointRecording::ms_LoadedRecordings[l].m_pRecording)
							{
								if(pFollowWaypointTask->GetEntity())
									pFollowWaypointTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL);
								pFollowWaypointTask->ClearRoute();
								pFollowWaypointTask->SetQuitImmediately();
							}
						}
					}
				}
			}

			delete CWaypointRecording::ms_LoadedRecordings[l].m_pRecording;

			CWaypointRecording::ms_LoadedRecordings[l].m_iFileNameHashKey = 0;
			CWaypointRecording::ms_LoadedRecordings[l].m_iNumReferences = 0;
			CWaypointRecording::ms_LoadedRecordings[l].m_pRecording = NULL;
#if __BANK
			CWaypointRecording::ms_LoadedRecordings[l].m_Filename[0] = 0;
#endif
		}
	}
}

void CWaypointRecordingStreamingInterface::RemoveSlot(strLocalIndex 
#if USE_PAGED_POOLS_FOR_STREAMING || __ASSERT
	index
#endif // USE_PAGED_POOLS_FOR_STREAMING || __ASSERT
	)
{
	ASSERT_ONLY(const u32 iHashKey = CWaypointRecording::m_StreamingInfo[index.Get()].m_iFilenameHashKey;)
	Assert(iHashKey==0);

#if USE_PAGED_POOLS_FOR_STREAMING
	FreeObject(index);
#endif // USE_PAGED_POOLS_FOR_STREAMING
}

bool CWaypointRecordingStreamingInterface::Load(strLocalIndex index, void * object, int size)
{
	::CWaypointRecordingRoute * pRecordingRoute = rage_new ::CWaypointRecordingRoute();

	char memFilename[256];
	fiDeviceMemory::MakeMemoryFileName(memFilename, 256, object, size, false, "WaypointRecording");

	if(pRecordingRoute->Load(memFilename))
	{
		// Put into the store
		CWaypointRecording::SetRecordingFromStreamingIndex(index.Get(), pRecordingRoute);
		return true;
	}
	return false;
}

void CWaypointRecordingStreamingInterface::PlaceResource(strLocalIndex index, datResourceMap& map, datResourceInfo& header)
{
	::CWaypointRecordingRoute * pRecordingRoute = NULL;

#if __FINAL
	pgRscBuilder::PlaceStream(pRecordingRoute, header, map, "<unknown>");
#else
	char tmp[64];
	sprintf(tmp, "::CWaypointRecordingRoute:%i", index.Get());
	pgRscBuilder::PlaceStream(pRecordingRoute, header, map, tmp);
#endif

	// Put into the store
	CWaypointRecording::SetRecordingFromStreamingIndex(index.Get(), pRecordingRoute);
}

void* CWaypointRecordingStreamingInterface::GetPtr(strLocalIndex index)
{
	const u32 iHashKey = CWaypointRecording::m_StreamingInfo[index.Get()].m_iFilenameHashKey;
	Assertf(iHashKey!=0, "Filename hashkey was zero - recording isn't loaded");

	for(int l=0; l<CWaypointRecording::ms_iMaxNumLoaded; l++)
	{
		if(CWaypointRecording::ms_LoadedRecordings[l].m_iFileNameHashKey == iHashKey)
		{
			return CWaypointRecording::ms_LoadedRecordings[l].m_pRecording;
		}
	}
	return NULL;
}


