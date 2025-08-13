/////////////////////////////////////////////////////////////////////////////////
// FILE :    train.cpp
// PURPOSE : Those big metal cars on fixed paths that everybody loves.
// AUTHOR :  Obbe, Adam Croston
// CREATED : 21-8-00
/////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "crSkeleton/Skeleton.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "math/angmath.h"
#include "mathext/noise.h"
#include "parser/manager.h"// for parTree and PARSER (XML support)

// Framework headers
#include "entity/sceneupdate.h"
#include "grcore/debugdraw.h"
#include "fwmaths/random.h"
#include "fwmaths/vector.h"
#include "fwscene/world/WorldLimits.h"
#include "fwsys/timer.h"
#include "fwutil/PtrList.h"

// Game headers
#include "audio/northaudioengine.h"
#include "audio/policescanner.h"
#include "audio\gameobjects.h"
#include "audio/trainaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "control\gamelogic.h"
#include "control\replay/replay.h"
#include "core/game.h"
#include "debug\debugscene.h"
#include "debug\vectormap.h"
#include "event\eventGroup.h"
#include "game\clock.h"
#include "game\modelIndices.h"
#include "Stats\StatsMgr.h"
#include "network\Cloud\Tunables.h"
#include "network\NetworkInterface.h"
#include "network\events\NetworkEventTypes.h"
#include "network\Objects\NetworkObjectPopulationMgr.h"
#include "network\Objects\Prediction\NetBlenderTrain.h"
#include "modelInfo\modelInfo.h"
#include "modelInfo\vehicleModelInfo.h"
#include "modelinfo\VehicleModelInfoFlags.h"
#include "pathserver\ExportCollision.h"
#include "peds\ped.h"
#include "peds\PedFactory.h"
#include "peds\pedIntelligence.h"
#include "peds\pedpopulation.h"
#include "pedgroup\pedgroup.h"
#include "physics\gtaArchetype.h"
#include "physics\gtaInst.h"
#include "physics\physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/lights/lights.h"
#include "renderer\zoneCull.h"
#include "scene\portals\portal.h"
#include "scene/EntityIterator.h"
#include "scene\world\gameWorld.h"
#include "fwscene/search/SearchVolumes.h"
#include "streaming\streaming.h"
#include "streaming\populationstreaming.h"
#include "system\fileMgr.h"
#include "system\pad.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/Info/ScenarioTypes.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskRideTrain.h"
#include "vehicles\door.h"
#include "vehicles\planes.h"
#include "vehicles\train.h"
#include "vehicles\vehicleFactory.h"
#include "vehicles\vehiclepopulation.h"
#include "vehicles\vehicle_channel.h"
#include "vfx\Systems\VfxVehicle.h"
#include "vfx\vfxhelper.h"
#include "Vfx\Decals/DecalManager.h"
#include "Vfx\Misc\Fire.h"
#include "vehicleAi/Junctions.h"
#include "vehicleAi/vehicleintelligence.h"
#include "phbound\boundcomposite.h"
#include "vehicleAi/Task/TaskVehicleMissionBase.h"
#include "debug\debugglobals.h"



AI_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

PARAM(enablemptrain, "Enable the train in MP.");
PARAM(enablemptrainpassengers, "Enable train passengers in MP.");
PARAM(debugtrain, "Debug the distance and position of the closest train from the player.");
PARAM(displaytrainandtrackdebug, "Enable display of train and track debugging.");

//-----------------------------------------------------------------------------------------
RAGE_DEFINE_CHANNEL(train)
//-----------------------------------------------------------------------------------------

#if __BANK
bool CTrain::sm_bDebugTrain = false;
bool CTrain::ms_bForceSpawn = false;
#endif

bool CTrain::ms_bEnableTrainsInMP = true;
bool CTrain::ms_bEnableTrainPassengersInMP = false;
u32  CTrain::ms_iTrainNetworkHearbeatInterval = 10000;
bool CTrain::ms_bEnableTrainLinkLoopForceCrash = true;
CTrainCloudListener* CTrain::msp_trainCloudListener = NULL;

// Train lighting tuning constants.
static float		TrainLight_R									= 1.0f;
static float		TrainLight_G									= 1.0f;
static float		TrainLight_B									= 0.8f;
static float		TrainLight_Intensity							= 40.0f;
static float		TrainLight_FalloffMax							= 3.5f;
static float		TrainLight_fadingDistance						= 100.0f;
static float		TrainLights_FadeLength							= 10.0f;
static float		Train_AmbientVolume_Intensity_Scaler			= 2.0f;
#if __DEV
#define oo_TrainLights_FadeLength (1.0f / TrainLights_FadeLength)
#else // __DEV
static const float oo_TrainLights_FadeLength = (1.0f / TrainLights_FadeLength);
#endif // __DEV

#define TRAIN_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.225f))
BANK_SWITCH_NT(static, static const) Vec3V Train_AmbientVolume_Offset = TRAIN_AMBIENT_OCCLUDER_OFFSET;

static dev_s32		STOPATSTATIONDURATION_NORMAL 			= 20000;
static dev_s32		STOPATSTATIONDURATION_PLAYER_ON_BOARD 			= 6000;
static dev_s32		STOPATSTATIONDURATION_PLAYER_ON_BOARD_WITH_GANG = 22000;		// Give the gang some time to sit down

static dev_s32		STOPATSTATIONDURATION_MAXTIMEDOORSOPENINGCLOSING = 5000;

// Train pad shaking tuning constants.
static dev_float		fTrainPadShakeIntensityPlayersCarriage			= 0.1f;
static dev_float		fTrainPadShakeIntensityOtherCarriage			= 0.02f;
static dev_s32		iTrainPadShakeDuration							= 50;

// Train static members.
bool					CTrain::sm_bDisplayTrainAndTrackDebug			= false;
#if __BANK
bool					CTrain::sm_bProcessWithPhysics      			= true;
#endif
bool					CTrain::sm_bDisplayTrainAndTrackDebugOnVMap		= false;
bool					CTrain::sm_bDisableRandomTrains					= false;

float					CTrain::sm_stationRadius						= 5.0f;
float					CTrain::ms_fCloseStationDistEps					= 20.0f;
float					CTrain::sm_speedAtWhichConsideredStopped		= 0.04f;
float					CTrain::sm_maxDeceleration						= 8.0f;
float					CTrain::sm_maxAcceleration						= 4.0f;
bool					CTrain::sm_swingCarriages						= true;
bool					CTrain::sm_swingCarriagesSideToSide				= true;
float					CTrain::sm_swingSideToSideAngMax				= 0.04f;
float					CTrain::sm_swingSideToSideRate					= 1.5f;
float					CTrain::sm_swingSideToSideNoiseFrequency		= 1.50f;
float					CTrain::sm_swingSideToSideNoiseStrength			= 0.1f;
bool					CTrain::sm_swingCarriagesFrontToBack			= true;
float					CTrain::sm_swingFrontToBackAngMax				= 0.50f;
float					CTrain::sm_swingFrontToBackAngDeltaMaxPerSec	= 0.125f;
const float	  CTrain::sm_fMinSecondsToConsiderBeingThreatened  = 10.f;

CTrain::GenTrainStatus	CTrain::sm_genTrain_status;
s8						CTrain::sm_genTrain_trackIndex					= 0;

unsigned				CTrain::sm_networkTimeCreationRequest			= 0;

// DEBUG!! -AC, These shouldn't be global- they should be static members of CTrain
// (at least until we make a proper train manager class).
int		gDiskTrainTracksCount = 0;
int		gDLCTrainTracksCount = 0;
CTrainTrack gTrainTracks[MAX_TRAIN_TRACKS_PER_LEVEL];
TrainStation *g_audTrainStations[MAX_TRAIN_TRACKS_PER_LEVEL][MAX_NUM_STATIONS];
// END DEBUG!!


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// STATIC MEMBERS
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

static u32 sCRC_CTrain_ALLOWMODIFICATION				= TunableHash(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOWTRAINMODIFICATION", 0x8c76422f));
static u32 sCRC_CTrain_ms_bEnableTrainsInMP				= TunableHash(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLETRAINSINMP", 0xa28c50c2));
static u32 sCRC_CTrain_ms_bEnableTrainPassengersInMP	= TunableHash(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLETRAINPASSENGERSINMP", 0xef4a13c0));
static u32 sCRC_CTrain_ms_iTrainNetworkHearbeatInterval = TunableHash(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORKHEARTBEATINTERVAL", 0x8f4a2248));
static u32 sCRC_CTrain_ms_bEnableTrainLinkLoopForceCrash= TunableHash(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLETRAINCRASHONCIRCULARLINK", 0x6c543dda));

CTrainTrack::CTrainTrack()
{
	m_isEnabled = false;
	m_bInitiallySwitchedOff = true;
	m_iTurnedOffByScriptThreadID = THREAD_INVALID;
	m_iLastTrainSpawnTimeMS = 0;
	m_iScriptTrainSpawnFreqMS = -1;
	m_iLastTrainReportingTimeMS = 0;
	m_iSpawnFreqThreadID = THREAD_INVALID;

#if __BANK
	m_TurnedOffByScriptName[0] = 0;
#endif

	for (int i = 0; i < MAX_NUM_STATIONS; i++)
	{
		m_aStationSide[i] = CTrainTrack::None;
	}
}

int CTrainTrack::GetTrackIndex(CTrainTrack * pTrack)
{
	for(int t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		if(gTrainTracks[t].m_numNodes && pTrack==&gTrainTracks[t])
			return t;
	}
	Assertf(false, "CTrainTrack pointer is not valid.");
	return -1;
}

/////////////////////////////////////////////////////////////
// Train configuration related data structures.
/////////////////////////////////////////////////////////////
struct TrainCarriageInfo
{
	u32						m_modelNameHash;
#if __DEV
	char*						m_modelName;
#endif // __DEV
	u32						m_maxPedsPerCarriage;
	strLocalIndex				m_modelId;
	bool						m_flipModelDir;
	bool						m_doInteriorLights;
	float						m_fCarriageVertOffset; 
};
struct TrainConfig
{
	u32						m_nameHash;
#if __DEV
	char*						m_name;
#endif // __DEV

	float						m_populateTrainDist;
	u32						m_stopAtStationDurationNormal;
	u32						m_stopAtStationDurationPlayer;
	u32						m_stopAtStationDurationPlayerWithGang;
	bool						m_announceStations;
	bool						m_doorsBeep;
	bool						m_carriagesHang;
	bool						m_carriagesSwing;
	bool						m_carriagesUseEvenTrackSpacing;
	bool						m_bLinkTracksWithAdjacentStations;
	bool						m_noRandomSpawn;
	float						m_carriageGap;
	atArray<TrainCarriageInfo>	m_carriages;
};
struct TrainConfigRef
{
	u32						m_trainConfigsNameHash;
#if __DEV
	char*						m_trainConfigsName;
#endif // __DEV
	u8						m_trainConfigIndex;

	TrainConfig *				GetTrainConfig();
};
struct TrainConfigGroup
{
	u32						m_nameHash;
#if __DEV
	char*						m_name;
#endif // __DEV
	atArray<TrainConfigRef>		m_trainConfigRefs;
};
struct TrainConfigs
{
	u8 GetRandomTrainConfigIndexFromTrainConfigGroup(u32 trainGroupNameHash)
	{
		const int trainGroupCount = m_trainGroups.GetCount();
		for(int i = 0; i < trainGroupCount; ++i)
		{
			TrainConfigGroup& trainGroup = m_trainGroups[i];
			if(trainGroup.m_nameHash == trainGroupNameHash)
			{
				const int trainConfigRefCount = trainGroup.m_trainConfigRefs.GetCount();

				// create an intermediate array to hold the train configs we can actually spawn randomly
				atArray<u32> spawnableTrainConfigs;
				for (s32 f = 0; f < trainConfigRefCount; ++f)
				{
					if (!trainGroup.m_trainConfigRefs[f].GetTrainConfig() || trainGroup.m_trainConfigRefs[f].GetTrainConfig()->m_noRandomSpawn)
					{
						continue;
					}

					spawnableTrainConfigs.PushAndGrow(f);
				}

				s32 trainConfigRefIndex = fwRandom::GetRandomNumberInRange(0, spawnableTrainConfigs.GetCount());
				TrainConfigRef& trainConfigRef = trainGroup.m_trainConfigRefs[spawnableTrainConfigs[trainConfigRefIndex]];
				return trainConfigRef.m_trainConfigIndex;
			}
		}
		Assertf(false, "Train configuration group was not found.");
		return 0;
	}
	
	TrainConfig* GetConfigForTrain(const CTrain& rTrain)
	{
		//Ensure the index is valid.
		s8 iIndex = rTrain.m_trainConfigIndex;
		if(iIndex < 0)
		{
			return NULL;
		}
		
		return &m_trainConfigs[iIndex];
	}
	
	TrainCarriageInfo* GetCarriageInfoForTrain(const CTrain& rTrain)
	{
		//Ensure the config is valid.
		TrainConfig* pConfig = GetConfigForTrain(rTrain);
		if(!pConfig)
		{
			return NULL;
		}

		//Ensure the index is valid.
		s8 iIndex = rTrain.m_trainConfigCarriageIndex;
		if(iIndex < 0)
		{
			return NULL;
		}

		return &pConfig->m_carriages[iIndex];
	}
	
	TrainConfigGroup * GetTrainConfigGroupByHash(const u32 iHash)
	{
		Assert(iHash);
		for(int t=0; t<m_trainGroups.size(); t++)
		{
			if(m_trainGroups[t].m_nameHash==iHash)
				return &m_trainGroups[t];
		}
		return NULL;
	}
	atArray<TrainConfig>		m_trainConfigs;
	atArray<TrainConfigGroup>	m_trainGroups;
};
TrainConfigs gTrainConfigs;

TrainConfig * TrainConfigRef::GetTrainConfig()
{
	if(m_trainConfigIndex < gTrainConfigs.m_trainConfigs.size())
	{
		return &gTrainConfigs.m_trainConfigs[m_trainConfigIndex];
	}
	return NULL;
}

#if __BANK
void CTrain::PerformDebugTeleportEngineCB()
{
	PerformDebugTeleportBaseCB(true);
}

void CTrain::PerformDebugTeleportCB()
{
	PerformDebugTeleportBaseCB();
}

void CTrain::PerformDebugTeleportBaseCB(bool bTeleportToEngine)
{
	CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if(pLocalPlayer)
	{
		if (pLocalPlayer->GetIsInVehicle())
		{
			pLocalPlayer->SetPedOutOfVehicle(CPed::PVF_Warp);

			pLocalPlayer->DetachFromParent(0);
		}
		else
		{
			CTrain* pTrain = FindNearestTrainCarriage(MAT34V_TO_MATRIX34(pLocalPlayer->GetMatrix()).d, bTeleportToEngine);
			if (pTrain)
			{
				Vector3 pos = MAT34V_TO_MATRIX34(pTrain->GetMatrix()).d;
				if (!bTeleportToEngine)
				{
					pos.z += 1.0f;
				}

				pLocalPlayer->Teleport(pos,pLocalPlayer->GetCurrentHeading());
			}
		}
	}
}

void CTrain::PerformDebugTeleportToRoof()
{
	CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if(pLocalPlayer)
	{
		CTrain* pTrain = FindNearestTrainCarriage(MAT34V_TO_MATRIX34(pLocalPlayer->GetMatrix()).d, false);
		if (pTrain)
		{
			spdAABB tempBox;
			const spdAABB &vehicleBox = pTrain->GetBoundBox(tempBox);
			const float roofZ = vehicleBox.GetMax().GetZf();

			//const float roofZ = pTrain->GetBoundingBoxMax().z;

			Vector3 vNewPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());

			vNewPos.z = roofZ;

			pLocalPlayer->Teleport(vNewPos,pLocalPlayer->GetCurrentHeading());
		}
	}
}

void CTrain::PerformDebugTeleportIntoSeatCB()
{
	CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if(pLocalPlayer)
	{
		if (pLocalPlayer->GetIsInVehicle())
		{
			pLocalPlayer->SetPedOutOfVehicle(CPed::PVF_Warp);

			pLocalPlayer->DetachFromParent(0);
		}
		else
		{
			CTrain* pTrain = FindNearestTrainCarriage(MAT34V_TO_MATRIX34(pLocalPlayer->GetMatrix()).d, true);
			if (pTrain)
			{
				CPed* pPedSeat0 = pTrain->GetPedInSeat(0);
				if (pPedSeat0 && !pPedSeat0->IsPlayer())
				{
					pTrain->RemovePedFromSeat(pPedSeat0);
					if (!pPedSeat0->IsNetworkClone())
					{
						CPedFactory::GetFactory()->DestroyPed(pPedSeat0);
					}
				}

				pLocalPlayer->SetPedInVehicle(pTrain, 0, CPed::PVF_IfForcedTestVehConversionCollision|CPed::PVF_Warp);
			}
		}
	}
}

/////////////////////////////////////////////////////////////
// FUNCTION: InitWidgets
// PURPOSE:
/////////////////////////////////////////////////////////////
void	CTrain::InitWidgets(bkBank& BANK_ONLY(bank))
{
#if __BANK
	if (PARAM_debugtrain.Get())
		sm_bDebugTrain = true;
#endif

#if __BANK
	if (PARAM_displaytrainandtrackdebug.Get())
		sm_bDisplayTrainAndTrackDebug = true;

	// Set up some bank stuff.
	bank.PushGroup("Trains", false);

	bank.PushGroup("Tracks", true);
	bank.AddToggle("Display train and track debug",					&sm_bDisplayTrainAndTrackDebug);
	bank.AddToggle("Display train and track debug on VectorMap",	&sm_bDisplayTrainAndTrackDebugOnVMap);
	bank.PopGroup();

	bank.PushGroup("Train Debug",true);
	bank.AddToggle("Force spawn a train", &ms_bForceSpawn);
	bank.AddToggle("Enable train in MP", &ms_bEnableTrainsInMP);
	bank.AddToggle("Enable train passengers in MP", &ms_bEnableTrainPassengersInMP);
	bank.AddButton("Teleport Local Player to closest train engine", PerformDebugTeleportEngineCB);
	bank.AddButton("Teleport Local Player to closest train car", PerformDebugTeleportCB);
	bank.AddButton("Teleport Local Player into seat of closest train", PerformDebugTeleportIntoSeatCB);
	bank.AddToggle("Draw lines to player (green=train/local, orange=train/remote)", &sm_bDebugTrain);
	bank.PopGroup();

    bank.AddToggle("Process With Physics",					        &sm_bProcessWithPhysics);
	bank.PushGroup("Station Stopping and Starting ", true);
	bank.AddSlider("Station Radius",								&sm_stationRadius, 0.0f, 10.0f, 0.01f);//5.0f
	bank.AddSlider("Speed At Which Considered Stopped",				&sm_speedAtWhichConsideredStopped, 0.0f, 10.0f, 0.1f);//0.2f
	bank.AddSlider("Max Deceleration",								&sm_maxDeceleration, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Max Acceleration",								&sm_maxAcceleration, 0.0f, 100.0f, 0.1f);
	bank.PopGroup();

	bank.PushGroup("Carriage swinging", true);
	bank.AddToggle("Swing Carriages",								&sm_swingCarriages);
	bank.AddToggle("Swing Carriages Side To Side",					&sm_swingCarriagesSideToSide);
	bank.AddSlider("Swing Side To Side Ang Max",					&sm_swingSideToSideAngMax, 0.0f, 1000.0f, 0.01f);
	bank.AddSlider("Swing Side To Side Rate",						&sm_swingSideToSideRate, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Swing Side To Side Noise Frequency",			&sm_swingSideToSideNoiseFrequency, 0.0f, 30.0f, 0.01f);
	bank.AddSlider("Swing Side To Side Noise Strength",				&sm_swingSideToSideNoiseStrength, 0.0f, 10.0f, 0.01f);
	bank.AddToggle("Swing Carriages Front To Back",					&sm_swingCarriagesFrontToBack);
	bank.AddSlider("Swing Front To Back Ang Max",					&sm_swingFrontToBackAngMax, 0.0f, 1000.0f, 0.01f);
	bank.AddSlider("Swing Front To Back Ang Delta Max Per Sec",		&sm_swingFrontToBackAngDeltaMaxPerSec, 0.0f, 1000.0f, 0.01f);
	bank.PopGroup();

	bank.PushGroup("Rendering", true);
	bank.AddSlider("Ambient Volume Intensity Scaler", &Train_AmbientVolume_Intensity_Scaler, 0.0f, 100.0f, 0.01f);
	bank.AddVector("Ambient Occluder Offset", &Train_AmbientVolume_Offset, -5.0f, 5.0f, 0.001f);
	bank.PopGroup();
	bank.PopGroup();
#endif // __BANK
}
#endif	// __BANK


#if !__FINAL

atArray<CTrainCachedNode> * g_CulledNodes = NULL;

void CTrain::CullTrackNodesToArea(const Vector3 & vMin, const Vector3 & vMax)
{
	if(!g_CulledNodes)
		g_CulledNodes = new atArray<CTrainCachedNode>();
	g_CulledNodes->clear();

	for(s32 t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		for(s32 n=0; n<gTrainTracks[t].m_numNodes; n++)
		{
			CTrainTrackNode * pNode = gTrainTracks[t].GetNode(n);
			Vector3 vPos = pNode->GetCoors();
			if(vPos.IsGreaterOrEqualThan(vMin) && vMax.IsGreaterOrEqualThan(vPos))
			{
				CTrainCachedNode cachedNode;
				cachedNode.pNode = pNode;
				cachedNode.iTrack = t;
				cachedNode.iNode = n;
				g_CulledNodes->PushAndGrow(cachedNode);
			}
		}
	}
}

void CTrain::ForAllTrackNodesIntersectingArea(const Vector3 & vMin, const Vector3 & vMax, ForAllTrackNodesCB callbackFunction, void * pData)
{
	if(g_CulledNodes)
	{
		for(s32 n=0; n<g_CulledNodes->GetCount(); n++)
		{
			CTrainCachedNode & node = (*g_CulledNodes)[n];
			if( !callbackFunction( node.iTrack, node.iNode, pData ) )
			{
				return;
			}
		}
	}
	else
	{
		for(s32 t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
		{
			for(s32 n=0; n<gTrainTracks[t].m_numNodes; n++)
			{
				CTrainTrackNode * pNode = gTrainTracks[t].GetNode(n);
				Vector3 vPos = pNode->GetCoors();
				if( vPos.IsGreaterThan(vMin) && vMax.IsGreaterThan(vPos) )
				{
					if( !callbackFunction( t, n, pData ) )
					{
						return;
					}
				}
			}
		}
	}
}

bool g_bTriangleIntersectsTrack = false;
Vector3 g_vColPolyTrackIntersectNormal;

bool CollisionTriangleIntersectsTrackCB(s32 iTrackIndex, s32 iNodeIndex, void * data)
{
	const Vector3 * pPts = (const Vector3 *) data;

	const float fTrackWidth = 2.0f;

	CTrainTrackNode * pNode = gTrainTracks[iTrackIndex].GetNode(iNodeIndex);

	s32 iNextNode = iNodeIndex+1;
	if(iNextNode >= gTrainTracks[iTrackIndex].m_numNodes)
	{
		if(gTrainTracks[iTrackIndex].m_isLooped)
			iNextNode -= gTrainTracks[iTrackIndex].m_numNodes;
		else
			return false;
	}

	CTrainTrackNode * pNextNode = gTrainTracks[iTrackIndex].GetNode(iNextNode);
	Vector3 vNode = pNode->GetCoors();
	Vector3 vOther = pNextNode->GetCoors();

	Vector3 vFwd = vOther - vNode;
	float fLength = vFwd.Mag();
	vFwd.Normalize();

	Vector3 vRight = CrossProduct(vFwd, ZAXIS);
	vRight.Normalize();

	Vector3 vUp = CrossProduct(vRight, vFwd);
	vUp.Normalize();
	Assert(vUp.z > 0.0f);

	Vector3 vBoxMid = (vNode + vOther) * 0.5f;

	Matrix34 mat;
	mat.Identity();
	mat.a = vRight;
	mat.b = vFwd;
	mat.c = vUp;
	mat.d = vBoxMid;

	static dev_float fExtra = 0.125f;
	static dev_float fExtraLength = 1.0f;

	Vector3 vSize;
	vSize.x = fTrackWidth;  // (vEdgeLeft - vEdgeRight).Mag();
	vSize.y = fLength;
	vSize.z = 4.0f;

	vSize *= 0.5f;
	vSize += Vector3(fExtra, fExtraLength, fExtra);

	bool bIntersects = geomBoxes::TestPolygonToOrientedBoxFP(
		pPts[0], pPts[1], pPts[2],
		g_vColPolyTrackIntersectNormal,
		vBoxMid,
		mat,
		vSize);

	if(bIntersects)
	{
		g_bTriangleIntersectsTrack = true;
		return false;
	}
	return true;
}

bool CTrain::CollisionTriangleIntersectsTrainTracks(Vector3 * pTriPts)
{
	Vector3 vEdge1 = pTriPts[1] - pTriPts[0];
	Vector3 vEdge2 = pTriPts[2] - pTriPts[0];
	vEdge1.Normalize();
	vEdge2.Normalize();
	g_vColPolyTrackIntersectNormal = CrossProduct(vEdge2, vEdge1);

	const float fExtra = 10.0f;
	const Vector3 vMin(
		Min(Min(pTriPts[0].x, pTriPts[1].x), pTriPts[2].x) - fExtra,
		Min(Min(pTriPts[0].y, pTriPts[1].y), pTriPts[2].y) - fExtra,
		Min(Min(pTriPts[0].z, pTriPts[1].z), pTriPts[2].z) - fExtra);
	const Vector3 vMax(
		Max(Max(pTriPts[0].x, pTriPts[1].x), pTriPts[2].x) + fExtra,
		Max(Max(pTriPts[0].y, pTriPts[1].y), pTriPts[2].y) + fExtra,
		Max(Max(pTriPts[0].z, pTriPts[1].z), pTriPts[2].z) + fExtra);

	g_bTriangleIntersectsTrack = false;

	ForAllTrackNodesIntersectingArea( vMin, vMax, CollisionTriangleIntersectsTrackCB, (void*)pTriPts );

	return g_bTriangleIntersectsTrack;
}
#endif

class CTrainConfigFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::TRAINTRACK_FILE:
			CTrain::LoadTrackXml(file.m_filename);
			CTrain::TrainTracksPostLoad();
			return true;
		case CDataFileMgr::TRAINCONFIGS_FILE:
			CTrain::LoadTrainConfigs(file.m_filename);
			CTrain::TrainConfigsPostLoad();
			return true;
		default:
			return false;
		}
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/)
	{
		
	}
}
g_trainConfigFileMounter;

void CTrain::TrainTracksPostLoad()
{
	for(int t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		CTrainTrack & track = gTrainTracks[t];

		if(track.m_numNodes && track.m_trainConfigGroupNameHash)
		{
			TrainConfigGroup * pConfigGroup = gTrainConfigs.GetTrainConfigGroupByHash(track.m_trainConfigGroupNameHash);

			TrainConfig * pLinkedConfig = NULL;
			if(pConfigGroup)
			{
				for(int c=0; c<pConfigGroup->m_trainConfigRefs.size(); c++)
				{
					if(pConfigGroup->m_trainConfigRefs[c].GetTrainConfig() && pConfigGroup->m_trainConfigRefs[c].GetTrainConfig()->m_bLinkTracksWithAdjacentStations)
					{
						pLinkedConfig = pConfigGroup->m_trainConfigRefs[c].GetTrainConfig();
						break;
					}
				}
			}

			if(pLinkedConfig && track.m_iLinkedTrackIndex==-1)
			{
				for(int s=0; s<track.m_numStations && track.m_iLinkedTrackIndex==-1; s++)
				{
					const Vector3 & vStation = track.m_aStationCoors[s];

					//**************************************************************************************
					// Now we must iterate through all the stations in all other tracks which also may link

					for(int t2=0; t2<MAX_TRAIN_TRACKS_PER_LEVEL && track.m_iLinkedTrackIndex==-1; t2++)
					{
						if(t==t2)
							continue;

						CTrainTrack & track2 = gTrainTracks[t2];

						if(track2.m_numNodes && track2.m_trainConfigGroupNameHash)
						{
							TrainConfigGroup * pConfigGroup2 = gTrainConfigs.GetTrainConfigGroupByHash(track2.m_trainConfigGroupNameHash);

							TrainConfig * pLinkedConfig2 = NULL;
							if(pConfigGroup2)
							{
								for(int c=0; c<pConfigGroup2->m_trainConfigRefs.size(); c++)
								{
									if(pConfigGroup2->m_trainConfigRefs[c].GetTrainConfig() && pConfigGroup2->m_trainConfigRefs[c].GetTrainConfig()->m_bLinkTracksWithAdjacentStations)
									{
										pLinkedConfig2 = pConfigGroup2->m_trainConfigRefs[c].GetTrainConfig();
										break;
									}
								}
							}

							if(pLinkedConfig2 && pLinkedConfig2->m_bLinkTracksWithAdjacentStations)
							{
								for(int s2=0; s2<track2.m_numStations; s2++)
								{
									const Vector3 & vStation2 = track2.m_aStationCoors[s2];

									if(vStation.IsClose(vStation2, ms_fCloseStationDistEps))
									{
										Assert(track.m_iLinkedTrackIndex==-1);
										track.m_iLinkedTrackIndex = t2;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void CTrain::TrainConfigsPostLoad()
{
	const int trainConfigCount = gTrainConfigs.m_trainConfigs.GetCount();
	for(int i = 0; i < trainConfigCount; ++i)
	{
		TrainConfig& trainConfig = gTrainConfigs.m_trainConfigs[i];
		const int carriageCount = trainConfig.m_carriages.GetCount();
		for(int j = 0; j < carriageCount; ++j)
		{
			TrainCarriageInfo& carriage = trainConfig.m_carriages[j];

			fwModelId modelId;
			CModelInfo::GetBaseModelInfoFromHashKey(carriage.m_modelNameHash, &modelId);
			carriage.m_modelId = modelId.GetModelIndex();

			Assertf(carriage.m_modelId.Get() >= 0, "Car model needed for train has been removed");
		}
	}
}

s32 CTrain::GetTrainConfigIndex(u32 nameHash)
{
	for(int i=0; i<gTrainConfigs.m_trainConfigs.GetCount(); ++i)
	{
		if(gTrainConfigs.m_trainConfigs[i].m_nameHash == nameHash)
			return i;
	}
	Assertf(false,"Train configuration with the name given not found");
	return -1;
}

/////////////////////////////////////////////////////////////
// FUNCTION: LoadTrainConfigs
// PURPOSE:  Loads the train configurations, which describe
// which carriage models to use and how to chain them
// together into trains.
/////////////////////////////////////////////////////////////
static unsigned int TrainConfigsFileVersionNumber = 1;
void CTrain::LoadTrainConfigs(const char* pFileName)
{
	// Attempt to parse the note nodes XML data file and put our data into an
	// XML DOM (Document Object Model) tree.
	parTree* pTree = PARSER.LoadTree(pFileName, "");
	if(!pTree)
	{
		Errorf("Failed to lode train configs file '%s'.", pFileName);
	}

	// Try to get the root DOM node.
	// It should be of the form: <train_configs version="1"> ... </train_configs>.
	const parTreeNode* pRootNode = pTree->GetRoot();
	Assert(pRootNode);
	Assert(stricmp(pRootNode->GetElement().GetName(), "train_configs") == 0);

	// Make sure we have the correct version.
	unsigned int version = pRootNode->GetElement().FindAttributeIntValue("version", 0);
	Assert(version == TrainConfigsFileVersionNumber);
	if(version != TrainConfigsFileVersionNumber)
	{
		// Free all the memory created for the DOM tree (and its children).
		delete pTree;
		return;
	}
	//bool isDlc = pRootNode->GetElement().FindAttributeBoolValue("isDlc",false);

	// Go over any and all the DOM child trees and DOM nodes off of the root
	// DOM node.
	parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
	for(; i != pRootNode->EndChildren(); ++i)
	{
		// Check if this DOM child is the note nodes list or node links list.
		// It should be of the form: <train_config> ... </train_config> or
		// it should be of the form: <train_config_group> ... </train_config_group>.
		if(stricmp((*i)->GetElement().GetName(), "train_config") == 0)
		{
			// Add the train_config.
			TrainConfig& trainConfig = gTrainConfigs.m_trainConfigs.Grow();

			// Set the hash of the name of the group.
			char tempConfigNameBuff[256];
			const char* tempConfigName = (*i)->GetElement().FindAttributeStringValue("name", NULL, tempConfigNameBuff, 256);
#if __DEV
			trainConfig.m_name = StringDuplicate(tempConfigName);
#endif // __DEV
			trainConfig.m_nameHash = atStringHash(tempConfigName);

			trainConfig.m_populateTrainDist		= (*i)->GetElement().FindAttributeFloatValue("populate_train_dist", 0.0f);
// DEBUG!! -AC, These should actually come from the train configs...
			trainConfig.m_stopAtStationDurationNormal			= STOPATSTATIONDURATION_NORMAL;
			trainConfig.m_stopAtStationDurationPlayer			= STOPATSTATIONDURATION_PLAYER_ON_BOARD;
			trainConfig.m_stopAtStationDurationPlayerWithGang	= STOPATSTATIONDURATION_PLAYER_ON_BOARD_WITH_GANG;
// END DEBUG!!
			trainConfig.m_announceStations		= (*i)->GetElement().FindAttributeBoolValue("announce_stations", false);
			trainConfig.m_doorsBeep				= (*i)->GetElement().FindAttributeBoolValue("doors_beep", false);
			trainConfig.m_carriagesHang			= (*i)->GetElement().FindAttributeBoolValue("carriages_hang", false);
			trainConfig.m_carriagesSwing		= (*i)->GetElement().FindAttributeBoolValue("carriages_swing", false);
			trainConfig.m_bLinkTracksWithAdjacentStations = (*i)->GetElement().FindAttributeBoolValue("link_tracks_with_adjacent_stations", false);
			trainConfig.m_noRandomSpawn			= (*i)->GetElement().FindAttributeBoolValue("no_random_spawn", false);

			// Carriage gap handling is special as the gap spacing can be "*" which
			// means the spacing is defined by the number of carriages over the
			// length of the track.
			trainConfig.m_carriageGap			= 0.0f;
			char tempCarriageGapDescBuff[256];
			const char* tempCarriageGapDescStr = (*i)->GetElement().FindAttributeStringValue("carriage_gap", NULL, tempCarriageGapDescBuff, 256);
			const u32 asteriskHash = ATSTRINGHASH("*", 0x0ff387e65);
			trainConfig.m_carriagesUseEvenTrackSpacing = (atStringHash(tempCarriageGapDescStr) == asteriskHash);
			if(!trainConfig.m_carriagesUseEvenTrackSpacing)
			{
				trainConfig.m_carriageGap = static_cast<float>(atof(tempCarriageGapDescStr));
			}

			// Go over any and all the child DOM trees and DOM nodes off of the train_config_group
			// list.
			parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
			for(; j != (*i)->EndChildren(); ++j)
			{
				// Check if this DOM child is the carriage.
				// It should be of the form: <carriage> ... </carriage>.
				Assertf((stricmp((*j)->GetElement().GetName(), "carriage") == 0), "An unknown and unsupported DOM child of the train_config has been encountered");

				char tempModelNameBuff[256];
				const char* tempModelName = (*j)->GetElement().FindAttributeStringValue("model_name", NULL, tempModelNameBuff, 256);

				// Determine the number of peds per carriage.
				u32 maxPedsPerCarriage = (*j)->GetElement().FindAttributeIntValue("max_peds_per_carriage", 0);

				// Determine if the model facing direction should be reversed.
				bool flipModelDir = (*j)->GetElement().FindAttributeBoolValue("flip_model_dir", false);

				// Determine if the model should have the train code do interior lights for it.
				bool doInteriorLights = (*j)->GetElement().FindAttributeBoolValue("do_interior_lights", false);

				// Add the specified number of carriages.
				u32 carriagesOfThisType = (*j)->GetElement().FindAttributeIntValue("repeat_count", 0);
				
				//Add the offet per carriage
				float carriageoffset = (*j)->GetElement().FindAttributeFloatValue("carriage_vert_offset", 0);

				for(u32 carriage = 0; carriage < carriagesOfThisType; ++carriage)
				{
					TrainCarriageInfo& trainCarriage = trainConfig.m_carriages.Grow();

					trainCarriage.m_modelNameHash		= atStringHash(tempModelName);
#if __DEV
					trainCarriage.m_modelName			= StringDuplicate(tempModelName);
#endif // __DEV
					trainCarriage.m_flipModelDir		= flipModelDir;
					trainCarriage.m_maxPedsPerCarriage	= maxPedsPerCarriage;
					trainCarriage.m_doInteriorLights	= doInteriorLights;
					trainCarriage.m_fCarriageVertOffset = carriageoffset;
				}
			}
		}
		else if(stricmp((*i)->GetElement().GetName(), "train_config_group") == 0)
		{
			// Add the train_config_group.
			TrainConfigGroup& trainConfigGroup = gTrainConfigs.m_trainGroups.Grow();

			// Set the hash of the name of the group.
			char tempGroupNameBuff[256];
			const char* tempGroupName = (*i)->GetElement().FindAttributeStringValue("name", NULL, tempGroupNameBuff, 256);
#if __DEV
			trainConfigGroup.m_name = StringDuplicate(tempGroupName);
#endif // __DEV
			trainConfigGroup.m_nameHash = atStringHash(tempGroupName);

			// Go over any and all the child DOM trees and DOM nodes off of the train_config_group
			// list.
			parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
			for(; j != (*i)->EndChildren(); ++j)
			{
				// Check if this DOM child is the train_config_ref.
				// It should be of the form: <train_config_ref> ... </train_config_ref>.
				Assertf((stricmp((*j)->GetElement().GetName(), "train_config_ref") == 0), "An unknown and unsupported DOM child of the train_config_group has been encountered");

				// Add the train config group...
				TrainConfigRef& trainConfigRef = trainConfigGroup.m_trainConfigRefs.Grow();

				// Set the hash of the name of the train config ref.
				char tempTrainConfigNameBuff[256];
				const char* tempTrainConfigName = (*j)->GetElement().FindAttributeStringValue("name", NULL, tempTrainConfigNameBuff, 256);

				DEV_ONLY( trainConfigRef.m_trainConfigsName = StringDuplicate(tempTrainConfigName); )

				trainConfigRef.m_trainConfigsNameHash = atStringHash(tempTrainConfigName);

				// Try to find the train config in the list of train configs (for faster lookup later).

				ASSERT_ONLY( bool found = false; )

				const u8 trainConfigCount = static_cast<u8>(gTrainConfigs.m_trainConfigs.GetCount());
				for(u8 k = 0; k < trainConfigCount; ++k)
				{
					TrainConfig& trainConfig = gTrainConfigs.m_trainConfigs[k];
					if(trainConfigRef.m_trainConfigsNameHash == trainConfig.m_nameHash)
					{
						trainConfigRef.m_trainConfigIndex = k;

						ASSERT_ONLY( found = true; )

						break;
					}
				}
				Assertf(found, "Train configuration refernce by train cofig group was not found.");
			}
		}
		else
		{
			// An unknown and unsupported DOM child of the root has been encountered.
			Assert(false);
		}
	}

	// Free all the memory created for the DOM tree (and its children).
	delete pTree;
}


static unsigned int TrainTracksFileVersionNumber = 1;
void CTrain::LoadTrackXml(const char *pXmlFilename)
{
	trainDebugf2("CTrain::LoadTrackXml pXmlFilename[%s]",pXmlFilename);

	// Attempt to parse the train track XML data file and put our data into an
	// XML DOM (Document Object Model) tree.
	parTree* pTree = PARSER.LoadTree(pXmlFilename, "");
	if(!pTree)
	{
		Errorf("Failed to load train track file '%s'.", pXmlFilename);
	}

	// Try to get the root DOM node.
	// It should be of the form: <train_tracks version="1"> ... </train_tracks>.
	const parTreeNode* pRootNode = pTree->GetRoot();
	Assert(pRootNode);
	Assert(stricmp(pRootNode->GetElement().GetName(), "train_tracks") == 0);

	// Make sure we have the correct version.
	unsigned int version = pRootNode->GetElement().FindAttributeIntValue("version", 0);
	Assert(version == TrainTracksFileVersionNumber);
	if (!Verifyf(version == TrainTracksFileVersionNumber, "Version of train_tracks xml file does not match code version"))
	{
		// Free all the memory created for the DOM tree (and its children).
		delete pTree;
		return;
	}

	// Go over any and all the DOM child trees and DOM nodes off of the root DOM node.
	parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
	for(; i != pRootNode->EndChildren(); ++i)
	{
		// This DOM child should be of the form: <train_track> ... </train_track>
		if(stricmp((*i)->GetElement().GetName(), "train_track") == 0)
		{
			char tempFilenameBuff[256];
			const char* pFilename = (*i)->GetElement().FindAttributeStringValue("filename", NULL, tempFilenameBuff, 256);

			Assertf(gDiskTrainTracksCount+gDLCTrainTracksCount < MAX_TRAIN_TRACKS_PER_LEVEL, "Not enough room for extra traintrack. Increase MAX_TRAIN_TRACKS_PER_LEVEL");
			bool isDLC	= (*i)->GetElement().FindAttributeBoolValue("isDLC", false);
			int* _trainTracksCount = NULL;
			int curIdx = 0;
			if(isDLC)
			{
				_trainTracksCount = &gDLCTrainTracksCount;
				curIdx			= static_cast<s32>((*i)->GetElement().FindAttributeIntValue("dlcTrackIndex", -1));
				Assertf(curIdx!=-1 ,"Check the train tracks XML file for dlcIndex, has to be bigger than %d", MAX_ON_DISK_TRAIN_TRACKS_PER_LEVEL);
				Assertf(!gTrainTracks[curIdx].m_isEnabled, "A Track definition already exists for this slot");
			}
			else
			{
				_trainTracksCount = &gDiskTrainTracksCount;
				curIdx = gDiskTrainTracksCount;
				Assertf(curIdx<MAX_ON_DISK_TRAIN_TRACKS_PER_LEVEL,"Ran out of on disk (TU) tracks");
			}
			CTrain::ReadAndInterpretTrackFile(pFilename, gTrainTracks[curIdx], &g_audTrainStations[curIdx][0]);

			char tempConfigGroupNameBuff[256];
			const char* pConfigGroupName = (*i)->GetElement().FindAttributeStringValue("trainConfigName", NULL, tempConfigGroupNameBuff, 256);
			gTrainTracks[curIdx].m_trainConfigGroupNameHash = atStringHash(pConfigGroupName);
#if __DEV
			gTrainTracks[curIdx].m_trainConfigGroupName = StringDuplicate(pConfigGroupName);
#endif // __DEV

			bool bIsPingPongTrack = (*i)->GetElement().FindAttributeBoolValue("isPingPongTrack", true);
			gTrainTracks[curIdx].m_isLooped			= !bIsPingPongTrack;
			gTrainTracks[curIdx].m_isEnabled		= true;

			gTrainTracks[curIdx].m_stopAtStations	= (*i)->GetElement().FindAttributeBoolValue("stopsAtStations", true);
			gTrainTracks[curIdx].m_MPstopAtStations	= (*i)->GetElement().FindAttributeBoolValue("MPstopsAtStations", true);
			
			gTrainTracks[curIdx].m_maxSpeed			= static_cast<u32>((*i)->GetElement().FindAttributeIntValue("speed", 5));
			gTrainTracks[curIdx].m_breakDist			= static_cast<u32>((*i)->GetElement().FindAttributeIntValue("brakingDist", 5));
			gTrainTracks[curIdx].m_iLinkedTrackIndex = -1;

			trainDebugf2("CTrain::LoadTrackXml gTrainTracks[%d] m_isLooped[%d] m_isEnabled[%d] m_stopAtStations[%d] m_MPstopAtStations[%d] m_maxSpeed[%d] m_breakDist[%d]",curIdx,gTrainTracks[curIdx].m_isLooped,gTrainTracks[curIdx].m_isEnabled,gTrainTracks[curIdx].m_stopAtStations,gTrainTracks[curIdx].m_MPstopAtStations,gTrainTracks[curIdx].m_maxSpeed,gTrainTracks[curIdx].m_breakDist);

			PostProcessTrack(gTrainTracks[curIdx]);

			(*_trainTracksCount)++;
		}
		else
		{
			// An unknown and unsupported DOM child of the root has been encountered.
			Assert(false);
		}
	}

	// Free all the memory created for the DOM tree (and its children).
	delete pTree;
}

/////////////////////////////////////////////////////////////
// FUNCTION: LoadTrack
// PURPOSE:  Loads one train track
/////////////////////////////////////////////////////////////
/*
void CTrain::LoadTrack(char *pFileName, char* pParamString0, char *pParamString1, char *pParamString2, char *pParamString3, char *pParamString4)
{
	Assertf(gTrainTracksCount <= MAX_TRAIN_TRACKS_PER_LEVEL, "Not enough room for extra traintrack. Increase MAX_TRAIN_TRACKS_PER_LEVEL");

	CTrain::ReadAndInterpretTrackFile(pFileName, gTrainTracks[gTrainTracksCount], &g_audTrainStations[gTrainTracksCount][0]);

	gTrainTracks[gTrainTracksCount].m_trainConfigGroupNameHash = atStringHash(pParamString0);
#if __DEV
	gTrainTracks[gTrainTracksCount].m_trainConfigGroupName = StringDuplicate(pParamString0);
#endif // __DEV

	gTrainTracks[gTrainTracksCount].m_isLooped			= !(atStringHash(pParamString1) == ATSTRINGHASH("true", 0xB2F35C25));
	gTrainTracks[gTrainTracksCount].m_stopAtStations	= (atStringHash(pParamString2) == ATSTRINGHASH("true", 0xB2F35C25));
	gTrainTracks[gTrainTracksCount].m_maxSpeed			= static_cast<u32>(atoi(pParamString3));
	gTrainTracks[gTrainTracksCount].m_breakDist			= static_cast<u32>(atoi(pParamString4));
	gTrainTracks[gTrainTracksCount].m_iLinkedTrackIndex = -1;

	PostProcessTrack(gTrainTracks[gTrainTracksCount]);

	gTrainTracksCount++;
}
*/

/////////////////////////////////////////////////////////////
// name:		Shutdown
// description:	Shutdown train stuff
/////////////////////////////////////////////////////////////
void CTrain::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
	    for (s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
	    {
			gTrainTracks[trackIndex].m_isEnabled = false;
		    if(gTrainTracks[trackIndex].m_pNodes)
		    {
			    delete[] gTrainTracks[trackIndex].m_pNodes;
		    }
		    gTrainTracks[trackIndex].m_pNodes = NULL;
	    }
	    gDLCTrainTracksCount = 0;
		gDiskTrainTracksCount = 0;

	    gTrainConfigs.m_trainConfigs.Reset();
	    gTrainConfigs.m_trainGroups.Reset();
		
		if (msp_trainCloudListener)
		{
			delete msp_trainCloudListener;
			msp_trainCloudListener = NULL;
		}
    }	
}

void CTrain::InitLoadConfig()
{
	trainDebugf2("CTrain::InitLoadConfig");

	sm_bDisableRandomTrains = false;
	sm_genTrain_status = TGEN_NOTRAINPENDING;

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TRAINCONFIGS_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		LoadTrainConfigs(pData->m_filename);
		pData = DATAFILEMGR.GetNextFile(pData);
	}

	TrainConfigsPostLoad();

	//*******************************************************************************************************************
	// Post-process train tracks, in order to link those whose config is marked as "link_tracks_with_adjacent_stations"

	pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TRAINTRACK_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		LoadTrackXml(pData->m_filename);
		pData = DATAFILEMGR.GetNextFile(pData);
	}
	TrainTracksPostLoad();
}

/////////////////////////////////////////////////////////////
// name:		InitLevelWithMapLoaded
// description:	Stuff that needs to be done at the start of a level.
/////////////////////////////////////////////////////////////
void CTrain::Init(unsigned initMode)
{
	//@@: location CTRAIN_INIT_REGISTER_MOUNT_INTERFACES
	CDataFileMount::RegisterMountInterface(CDataFileMgr::TRAINCONFIGS_FILE,&g_trainConfigFileMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::TRAINTRACK_FILE,&g_trainConfigFileMounter);
	int trackIndex;
	for (trackIndex = 0 ; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
	{
		//DO NOT SET m_isEnabled to false here - as when Init is called on changing of state - say during INIT_SESSION for MP it will call this after the XML has already been loaded and turn trains off for MP.
		//Please contact Stephen LaValley or the MP team before changing this as it broke trains in MP.

		gTrainTracks[trackIndex].m_bInitiallySwitchedOff = true;
		gTrainTracks[trackIndex].m_iTurnedOffByScriptThreadID = THREAD_INVALID;
		gTrainTracks[trackIndex].m_iSpawnFreqThreadID = THREAD_INVALID;
		gTrainTracks[trackIndex].m_iScriptTrainSpawnFreqMS = -1;
		gTrainTracks[trackIndex].m_iLastTrainSpawnTimeMS = 0;

		gTrainTracks[trackIndex].m_iLastTrainReportingTimeMS = 0;

		gTrainTracks[trackIndex].m_bTrainActive = false;
		gTrainTracks[trackIndex].m_bTrainProcessNetworkCreation = false;
		gTrainTracks[trackIndex].m_bTrainNetworkCreationApproval = false;

#if __BANK
		gTrainTracks[trackIndex].m_TurnedOffByScriptName[0] = 0;
#endif
	}
    if(initMode == INIT_CORE)
    {
		trainDebugf2("CTrain::Init--INIT_CORE");

		for (trackIndex = 0 ; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
		{
			gTrainTracks[trackIndex].m_pNodes = NULL;
			gTrainTracks[trackIndex].m_numNodes = 0;		
		}
    }
    else if(initMode == INIT_BEFORE_MAP_LOADED)
    {
		trainDebugf2("CTrain::Init--INIT_BEFORE_MAP_LOADED");

        gDiskTrainTracksCount = 0;
		gDLCTrainTracksCount = 0;
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
		trainDebugf2("CTrain::Init--INIT_AFTER_MAP_LOADED");
		
		sm_bDisableRandomTrains = false;
		sm_genTrain_status = TGEN_NOTRAINPENDING;
    }
	else if (initMode == INIT_SESSION)
	{
		trainDebugf2("CTrain::Init--INIT_SESSION");

		RemoveAllTrains();
		sm_bDisableRandomTrains = false;

		//Read in config files as configurations change with DLC
		InitLoadConfig();

#if NAVMESH_EXPORT
		if( !CNavMeshDataExporter::WillExportCollision() )	// crash protection
#endif
		{
			SetTunablesValues();

			// create a train cloud listener - should always be active
			if(!msp_trainCloudListener)
				msp_trainCloudListener = rage_new CTrainCloudListener();
		}
	}
}

void CTrain::UpdateVisualDataSettings(const CVisualSettings& visualSettings)
{
	if (visualSettings.GetIsLoaded())
	{
		TrainLight_R									= visualSettings.Get( "train.light.r",							1.0f);
		TrainLight_G									= visualSettings.Get( "train.light.g",							1.0f);
		TrainLight_B									= visualSettings.Get( "train.light.b",							0.8f);
		TrainLight_Intensity							= visualSettings.Get( "train.light.intensity",					40.0f);
		TrainLight_FalloffMax							= visualSettings.Get( "train.light.falloffmax",					3.5f);
		TrainLight_fadingDistance						= visualSettings.Get( "train.light.fadingdistance",				100.0f);
		TrainLights_FadeLength							= visualSettings.Get( "train.light.fadelength",					10.0f);
		Train_AmbientVolume_Intensity_Scaler			= visualSettings.Get( "train.ambientvolume.intensityscaler",	2.0f);
	}

}

void CTrain::SetTunablesValues()
{
	trainDebugf2("CTrain::SetTunablesValues");

	bool bRetrieved = false;

	if (!Tunables::IsInstantiated())
	{
		trainDebugf2("CTrain::SetTunablesValues--!IsInstantiated-->return");
		return;
	}

	if (Tunables::GetInstance().Access(sCRC_CTrain_ALLOWMODIFICATION,bRetrieved))
	{
		trainDebugf2("CTrain::SetTunablesValues--sCRC_CTrain_ALLOWMODIFICATION");

		int iRetrieved = 0;

		if (Tunables::GetInstance().Access(sCRC_CTrain_ms_bEnableTrainsInMP,bRetrieved))
		{
			trainDebugf2("CTrain::SetTunablesValues--sCRC_CTrain_ms_bEnableTrainsInMP = bRetrieved[%d]",bRetrieved);
			ms_bEnableTrainsInMP = bRetrieved;
		}

		if (Tunables::GetInstance().Access(sCRC_CTrain_ms_bEnableTrainPassengersInMP,bRetrieved))
		{
			trainDebugf2("CTrain::SetTunablesValues--sCRC_CTrain_ms_bEnableTrainPassengersInMP = bRetrieved[%d]",bRetrieved);
			ms_bEnableTrainPassengersInMP = bRetrieved;
		}

		if (Tunables::GetInstance().Access(sCRC_CTrain_ms_iTrainNetworkHearbeatInterval,iRetrieved))
		{
			trainDebugf2("CTrain::SetTunablesValues--sCRC_CTrain_ms_iTrainNetworkHearbeatInterval = iRetrieved[%d]",iRetrieved);
			ms_iTrainNetworkHearbeatInterval = (u32) iRetrieved;
		}

		if(Tunables::GetInstance().Access(sCRC_CTrain_ms_bEnableTrainLinkLoopForceCrash, bRetrieved))
		{
			trainDebugf2("CTrain::SetTunablesValues--sCRC_CTrain_ms_bEnableTrainLinkLoopForceCrash = bRetrieved[%d]",bRetrieved);
			ms_bEnableTrainLinkLoopForceCrash = bRetrieved;
		}
	}

#if !__FINAL
	if (PARAM_enablemptrain.Get())
	{
		trainDebugf2("CTrain::SetTunablesValues--PARAM_enablemptrain -- set ms_bEnableTrainsInMP = true");
		ms_bEnableTrainsInMP = true;
	}

	if (PARAM_enablemptrainpassengers.Get())
	{
		trainDebugf2("CTrain::SetTunablesValues--PARAM_enablemptrainpassengers -- set ms_bEnableTrainPassengersInMP = true");
		ms_bEnableTrainPassengersInMP = true;
	}
#endif

	trainDebugf2("CTrain::SetTunablesValues ms_bEnableTrainsInMP[%d] ms_bEnableTrainPassengersInMP[%d] ms_iTrainNetworkHearbeatInterval[%u]",ms_bEnableTrainsInMP,ms_bEnableTrainPassengersInMP,ms_iTrainNetworkHearbeatInterval);
}

/////////////////////////////////////////////////////////////
// FUNCTION: ReadAndInterpretTrackFile
// PURPOSE:  Reads a file in from CD and modifies the z values of the points
//			 to be on the ground. Also splits stuff up into line segments.
/////////////////////////////////////////////////////////////

#define MAX_TRACK_FILE_SIZE ((128*1024) + READ_BLOCK_SIZE)

void CTrain::ReadAndInterpretTrackFile(const char * pFileName, CTrainTrack & track, TrainStation ** stations)
{
	s32			nFileSize;
	s32			Node;
	s32 			fpos, pos;
	Vector3			TempVec, VectorToNext, VectorPerp, FirstVector;
	u8			*pFileBuffer;

	track.m_numStations = 0;
	Assert(track.m_pNodes == NULL);

	{
		pFileBuffer = rage_new u8[MAX_TRACK_FILE_SIZE];
		Assert(pFileBuffer);

		// Open our file
		nFileSize = CFileMgr::LoadFile(pFileName, pFileBuffer, MAX_TRACK_FILE_SIZE, "rb");

		if(nFileSize <= 0)
		{
			Assertf(0, "Couldn't open traintrack file");
			return;
		}
		Assertf(nFileSize < MAX_TRACK_FILE_SIZE, "Need bigger buffer to load train tracks");

		fpos = 0;
		pos = 0;

		// First find out how many nodes there are in this train track.
		char tmp[1024];
		while(pFileBuffer[fpos] != '\n')
		{
			tmp[pos++] = pFileBuffer[fpos++];
		}
		tmp[pos] = '\0';
		fpos++;

		// If the first string is 'processed' the nodes have already been placed on the right z coordinate.
		if(!strcmp("processed", tmp))
		{
			// read the next string.
			pos = 0;
			while(pFileBuffer[fpos] != '\n')
			{
				tmp[pos++] = pFileBuffer[fpos++];
			}
			tmp[pos] = '\0';
			fpos++;
		}

		sscanf(tmp, "%d", &track.m_numNodes);

		// Allocate memory for the array of nodes
		track.m_pNodes = rage_new CTrainTrackNode[track.m_numNodes];
		Assert(track.m_pNodes);

		for (Node = 0; Node < track.m_numNodes; Node++)
		{
			pos = 0;

			while(pFileBuffer[fpos] != '\n')
			{
				tmp[pos++] = pFileBuffer[fpos++];
			}
			fpos++;
			tmp[pos] = 0;
			float	X, Y, Z;
			s32	Station;
			char stationNameForAudio[64];
			u32 numTokens = sscanf(tmp, "%f %f %f %d %s", &X, &Y, &Z, &Station, &stationNameForAudio[0]);
			Assert(X > WORLDLIMITS_XMIN && X < WORLDLIMITS_XMAX);
			Assert(Y > WORLDLIMITS_YMIN && Y < WORLDLIMITS_YMAX);
			Assert(Z > WORLDLIMITS_ZMIN && Z < WORLDLIMITS_ZMAX);

			(track.m_pNodes)[Node].SetCoorsX(X);
			(track.m_pNodes)[Node].SetCoorsY(Y);
			(track.m_pNodes)[Node].SetCoorsZ(Z);

			if ((Station & CTrainTrackNode::TUNNEL_MASK) == CTrainTrackNode::TUNNEL_MASK)
			{
				(track.m_pNodes)[Node].SetIsTunnel(true);
				Station &= (~CTrainTrackNode::TUNNEL_MASK);
			}

			if(Station)
			{
				Assertf((track.m_numStations) < MAX_NUM_STATIONS, "Too many stations on train track");

				(track.m_pNodes)[Node].SetStationType((CTrainTrackNode::eStationType)Station);
				track.m_aStationCoors[(track.m_numStations)] = Vector3(X, Y, Z); // DW - fixed bad pointer arithmetic
				if(Station == 1)
				{
					track.m_aStationSide[track.m_numStations] = CTrainTrack::Left;
				}
				else
				{
					track.m_aStationSide[track.m_numStations] = CTrainTrack::Right;

				}
				
				trainDebugf2("CTrain::ReadAndInterpretTrackFile station %s @ %f %f %f from file %s", Station == 1 ? "left" : "right",X,Y,Z,pFileName);

				if(stations != NULL)
				{
					if(numTokens == 5)
					{
						stations[track.m_numStations] = audNorthAudioEngine::GetObject<TrainStation>(stationNameForAudio);
					}
					else
					{
						stations[track.m_numStations] = NULL;
					}
				}
				if(track.m_aStationNodes)
				{
					track.m_aStationNodes[track.m_numStations] = Node;
				}
				track.m_numStations++;
			}
			else
			{
				track.m_pNodes[Node].SetStationType((CTrainTrackNode::eStationType)Station);
			}
		}
		// Sometimes the first node is identical to the last.
		// This fucks stuff up later on. (In CTrain::ProcessControl)
		// deal with this here
		Vector3 trackNodePos = track.m_pNodes[0].GetCoors();
		Vector3 trackNodePrevPos = track.m_pNodes[track.m_numNodes-1].GetCoors();

		if((trackNodePos.x == trackNodePrevPos.x) &&
			(trackNodePos.y == trackNodePrevPos.y))
		{
			track.m_numNodes = track.m_numNodes - 1;
		}

		delete[] pFileBuffer;

		// Since the nodes are on one of the tracks and we want the nodes in the middle
		// of the tracks we have to shift the points a bit.
	}
}

//*********************************************************************************
// After loading all the train tracks, we post-process them - finding the distance
// from the start to each node; and also optionally inserting new nodes.

void CTrain::PostProcessTrack(CTrainTrack & track, const float fMaxSpacingBetweenNodes)
{
	//***********************************************************************************
	// JB: Here is a test section which can add new nodes along long stretches of track.
	// As it currently stands, trains can only be generated at nodes/stations - and this
	// causes problems with some setups as the sparseness of the nodes means no trains.

	static dev_bool sbAddNewNodes = false;
	if(sbAddNewNodes)
	{
		int iNumExtraNodes = 0;

		for(int n=1; n<track.m_numNodes; n++)
		{
			Vector3 vLink = track.m_pNodes[n].GetCoors() - track.m_pNodes[n-1].GetCoors();
			float fLinkDistSq = vLink.Mag2();

			if(fLinkDistSq > fMaxSpacingBetweenNodes*fMaxSpacingBetweenNodes)
			{
				float fLinkDist = sqrtf(fLinkDistSq);
				iNumExtraNodes += (int) (fLinkDist / fMaxSpacingBetweenNodes);
			}
		}

		// We need to add new nodes
		if(iNumExtraNodes > 0)
		{
			// Make a buffer big enough to contain our new nodes
			const int iTotalNumNewNodes = iNumExtraNodes + track.m_numNodes;
			CTrainTrackNode* pNewNodes = rage_new CTrainTrackNode[iTotalNumNewNodes];

			// Fill it up out of the old array
			int iOldNodeCounter = 0;
			for(int i = 0; i < iTotalNumNewNodes; i++)
			{
				CTrainTrackNode& pCurrentNode = track.m_pNodes[iOldNodeCounter];
				pNewNodes[i] = pCurrentNode;

				// Look ahead to next node and see if it is too far away
				iOldNodeCounter++;
				if(iOldNodeCounter < track.m_numNodes)
				{
					CTrainTrackNode& pNextNode = track.m_pNodes[iOldNodeCounter];

					Vector3 vLinkDiff = pNextNode.GetCoors() - pCurrentNode.GetCoors();
					float fLinkDistSq = vLinkDiff.Mag2();

					if(fLinkDistSq > fMaxSpacingBetweenNodes*fMaxSpacingBetweenNodes)
					{
						float fLinkDist = sqrtf(fLinkDistSq);
						vLinkDiff.Scale(fMaxSpacingBetweenNodes / fLinkDist);

						// Loop over all the extra nodes we need to create

						int iExtraNodesForThisPair = (int) (fLinkDist / fMaxSpacingBetweenNodes);
						for(int iExtraNodeIndex = 0; iExtraNodeIndex < iExtraNodesForThisPair; iExtraNodeIndex++)
						{
							// Add a new node fMaxSpacingBetweenNodes away from last node

							Vector3 vNewLinkPos = pNewNodes[i].GetCoors() + vLinkDiff;

							i++;
							// Protect against array overrun... if this assert fires then we didn't allocate enough space
							Assert(i < iTotalNumNewNodes);
							if(i < iTotalNumNewNodes)
							{
								pNewNodes[i] = pNewNodes[i-1];
								pNewNodes[i].SetCoors(vNewLinkPos);

								// Make sure our new node has no station
								pNewNodes[i].SetStationType(CTrainTrackNode::ST_NONE);
							}
						}
					}
				}
			}

			// Copy new array into old one, and delete old one
			delete[] track.m_pNodes;
			track.m_pNodes = pNewNodes;
			track.m_numNodes = iTotalNumNewNodes;

		}
	}

	//*************************************************************************
	// First we have to initialise some of the values that relate to the nodes.

	float LengthFromStart, SegmentLength;
	s32 C;

	LengthFromStart = 0.0f;
	s32 Station = 0;
	for (C = 0; C < track.m_numNodes; C++)
	{
		track.m_pNodes[C].SetLengthFromStart(LengthFromStart);

		// If this was a station we set the stations distance on the track.
		if(track.m_pNodes[C].GetStationType() != CTrainTrackNode::ST_NONE)
		{
			Assert(Station < MAX_NUM_STATIONS);
			track.m_aStationDistFromStart[Station++] = LengthFromStart;
		}

		Vector3 trackNodePos = track.m_pNodes[C].GetCoors();
		Vector3 trackNodeNextPos = track.m_pNodes[(C+1)%track.m_numNodes].GetCoors();
		SegmentLength = (trackNodeNextPos - trackNodePos).Mag();
		LengthFromStart += SegmentLength;
	}
	track.m_totalLengthIfLooped = LengthFromStart;
	Assert(track.m_numStations == Station);			// Make sure stations are found correctly.
}


///////////////////////////////////////////////////////////////////////
// FUNCTION: RenderTrainDebug
// PURPOSE:  Calculates the coordinates of the engine of the train. The
//           other train carriages use this.
///////////////////////////////////////////////////////////////////////
void CTrain::RenderTrainDebug()
{
#if DEBUG_DRAW
	const Color32 startNodeColour = Color_green;
	const Color32 endNodeColour = Color_red;
	const Color32 stationColour = Color_cyan;
	const Color32 trackColour = Color_green;
	const Color32 trackDisabledByScriptColour = Color_red4;

	if(sm_bDebugTrain)
	{
		CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		if(pLocalPlayer)
		{
			Vector3 playerLoc = MAT34V_TO_MATRIX34(pLocalPlayer->GetMatrix()).d;
			playerLoc.z += 1.0f;

			CTrain* pTrain = FindNearestTrainCarriage(playerLoc, true);

			if (pTrain)
			{
				int carcount = 0;
				CTrain* nextCar = pTrain;
				while (nextCar)
				{
					carcount++;
					nextCar = nextCar->GetLinkedToBackward();
				}

				Vector3 trainTrackLoc = MAT34V_TO_MATRIX34(pTrain->GetMatrix()).d;
				if (!pTrain->IsNetworkClone())
				{
					grcDebugDraw::Line(trainTrackLoc, playerLoc, Color_green);
					grcColor(Color_green); //match the color of the line to the train
				}
				else
				{
					grcDebugDraw::Line(trainTrackLoc, playerLoc, Color_orange);
					grcColor(Color_orange); //match the color of the line to the train
				}	

				//Show the distance from the player to the train on the screen
				float fDistanceToPlayer = (trainTrackLoc - playerLoc).Mag();

				Vector3 camToTrain = trainTrackLoc - camInterface::GetPos();
				TUNE_GROUP_FLOAT(TRAINS, FullUpdateDist, 250.0f, 0.0f, 1000.0f, 0.01f);
				float ct = camToTrain.Mag();
				const bool wantsFullUpdate = (ct < FullUpdateDist);

				bool wantsFullUpdate2 = false;
				CTrain* pTrainBackward = pTrain->GetLinkedToBackward();
				if (pTrainBackward)
				{
					Vector3 train2TrackLoc = MAT34V_TO_MATRIX34(pTrainBackward->GetMatrix()).d;
					Vector3 camToTrain2 = train2TrackLoc - camInterface::GetPos();
					float ct2 = camToTrain2.Mag();
					wantsFullUpdate2 = (ct2 < FullUpdateDist);
				}

				int ypos = 0;
				char buf[100];
				formatf(buf,"dt[%.1f]-t[%d]-cct[%d]-spd[%.1f]-ds[%.1f]-pos[%.1f]-st[%s]",fDistanceToPlayer,pTrain->GetTrackIndex(),carcount,pTrain->m_trackForwardSpeed,pTrain->m_travelDistToNextStation,pTrain->m_trackPosFront,pTrain->GetTrainStateString());
				grcDebugDraw::Text(playerLoc, grcGetCurrentColor(), 0, ypos, buf);
				ypos += 10;

				formatf(buf,"f[%d:%d:%d]-v[%d:%d]-c2t[%.1f]-fu[%d:%d]-lod[%d:%d]-tl[%d:%d]-in[%d:%d]",pTrain->GetIsFixedFlagSet(),pTrain->GetIsFixedByNetworkFlagSet(),pTrain->GetIsFixedUntilCollisionFlagSet(),pTrain->IsVisible(),pTrainBackward ? pTrainBackward->IsVisible() : false,ct,wantsFullUpdate,wantsFullUpdate2,pTrain->m_updateLODState,pTrainBackward ? pTrainBackward->m_updateLODState : 0,pTrain->m_nVehicleFlags.bIsInTunnel,pTrainBackward ? pTrainBackward->m_nVehicleFlags.bIsInTunnel : false,pTrain->GetIsInInterior(),pTrainBackward ? pTrainBackward->GetIsInInterior() : false);
				grcDebugDraw::Text(playerLoc, grcGetCurrentColor(), 0, ypos, buf);
				ypos += 10;

				formatf(buf,"pass_ct[%d]-pass_st[%s]-p[%p]-m[%s]",pTrain->GetFullTrainPassengerCount(),pTrain->GetPassengerStateString(),pTrain,pTrain->GetModelName());
				grcDebugDraw::Text(playerLoc, grcGetCurrentColor(), 0, ypos, buf);
				ypos += 10;
			}
		}
	}

	if(sm_bDisplayTrainAndTrackDebug)
	{
		// Go through the full track and display the nodes that are nearby.
		Vector3	Coors = camInterface::GetPos();
		for(s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
		{
			for(s32 TestNode = 0; TestNode < gTrainTracks[trackIndex].m_numNodes; TestNode++)
			{
				float Dist = (Coors - gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors()).Mag();

				if(Dist < 100.0f)
				{
					if(TestNode == 0)
					{
						const Vector3 vDbgPos = gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 2.0f);
						grcDebugDraw::Sphere(vDbgPos, 0.25f, startNodeColour);
						grcDebugDraw::Text(vDbgPos, startNodeColour, 0, 0, "Start Node");
					}

					if(TestNode == gTrainTracks[trackIndex].m_numNodes - 1)
					{
						const Vector3 vDbgPos = gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 2.0f);
						grcDebugDraw::Sphere(vDbgPos, 0.25f, endNodeColour);
						grcDebugDraw::Text(vDbgPos, endNodeColour, 0, 0, "End Node");
					}

					if(gTrainTracks[trackIndex].m_pNodes[TestNode].GetStationType() != CTrainTrackNode::ST_NONE)
					{
						s32	PreviousNode = (TestNode + gTrainTracks[trackIndex].m_numNodes - 1) % gTrainTracks[trackIndex].m_numNodes;
						// Also display a line to show the direction of platform
						Vector3	vRight, vAlongTrack = gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() - gTrainTracks[trackIndex].m_pNodes[PreviousNode].GetCoors();
						vRight.Cross(vAlongTrack, Vector3(0.0f, 0.0f, 1.0f));
						if(gTrainTracks[trackIndex].m_pNodes[TestNode].GetStationType() == CTrainTrackNode::ST_RIGHT)
						{
							vRight = -vRight;
						}
						vRight.Normalize();

						const Vector3 vDbgPos = gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors();
						grcDebugDraw::Line(vDbgPos, gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + vRight,  stationColour);
						grcDebugDraw::Sphere(vDbgPos + Vector3(0.0f, 0.0f, 1.0f), 0.25f, stationColour);
						grcDebugDraw::Text(vDbgPos, stationColour, 0, 0, "Station");
					}

					grcDebugDraw::Line(	gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors(),
						gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f),
						(gTrainTracks[trackIndex].IsSwitchedOff()) ? trackDisabledByScriptColour : trackColour
					);

					const int nextNodeIndex = (TestNode+1)%gTrainTracks[trackIndex].m_numNodes;

					grcDebugDraw::Line(
						gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors(),
						gTrainTracks[trackIndex].m_pNodes[nextNodeIndex].GetCoors(),
						(gTrainTracks[trackIndex].IsSwitchedOff()) ? trackDisabledByScriptColour : trackColour
					);

					char debugText[50];
					bool bStation = (gTrainTracks[trackIndex].m_pNodes[TestNode].GetStationType() != CTrainTrackNode::ST_NONE);
					bool bTunnel  = gTrainTracks[trackIndex].m_pNodes[TestNode].GetIsTunnel();
					Color32 displayColor = bStation ? Color_orange : bTunnel ? Color_yellow : Color_white;
					const s32 textheight = grcDebugDraw::GetScreenSpaceTextHeight();

					sprintf(debugText, "Track %i, Node %i%s", trackIndex, TestNode, gTrainTracks[trackIndex].m_pNodes[TestNode].GetIsTunnel() ? ", Tunnel" : "");
					grcDebugDraw::Text(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f), displayColor, 0, textheight * -1, debugText);

					if(sm_bDisableRandomTrains || CVehiclePopulation::GetScriptDisableRandomTrains() )
					{
						sprintf(debugText, "(all trains off)");
					}
					else if(gTrainTracks[trackIndex].m_bInitiallySwitchedOff)
					{
						sprintf(debugText, "(initially off)");
					}
					else if(gTrainTracks[trackIndex].m_iTurnedOffByScriptThreadID != THREAD_INVALID)
					{
						if(gTrainTracks[trackIndex].m_TurnedOffByScriptName[0])
							sprintf(debugText, "(switched off by : %s)", gTrainTracks[trackIndex].m_TurnedOffByScriptName);
						else
							sprintf(debugText, "(switched off by : script unknown)");
					}
					else
					{
						sprintf(debugText, "(on)");
					}

					grcDebugDraw::Text(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f), displayColor, 0, 0, debugText);

					sprintf(debugText, "Dist: %.2f", gTrainTracks[trackIndex].m_pNodes[TestNode].GetLengthFromStart());
					grcDebugDraw::Text(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f), displayColor, 0, textheight, debugText);

					if (bStation)
					{
						sprintf(debugText, "Position %f %f %f", gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoorsX(), gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoorsY(), gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoorsZ());
						grcDebugDraw::Text(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f), displayColor, 0, textheight * 2, debugText);

						sprintf(debugText, "Station %s", gTrainTracks[trackIndex].m_pNodes[TestNode].GetStationType() == CTrainTrackNode::ST_LEFT ? "ST_LEFT" : "ST_RIGHT" );
						grcDebugDraw::Text(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f), displayColor, 0, textheight * 3, debugText);
					}
				}
			}
		}
	}

	if(sm_bDisplayTrainAndTrackDebugOnVMap)
	{
		for (s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
		{
			for (s32 TestNode = 0; TestNode < gTrainTracks[trackIndex].m_numNodes; TestNode++)
			{
				if(TestNode == 0)
				{
					Color32 startNodeColour(0, 255, 0, 255);
					CVectorMap::DrawCircle((gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 2.0f)),
						0.25f,
						startNodeColour);
				}

				if(TestNode == gTrainTracks[trackIndex].m_numNodes - 1)
				{
					Color32 endNodeColour(255, 0, 0, 255);
					CVectorMap::DrawCircle((gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 3.0f)),
						0.25f,
						endNodeColour);
				}

				if(gTrainTracks[trackIndex].m_pNodes[TestNode].GetStationType() != CTrainTrackNode::ST_NONE)
				{
					Color32 stationColour(0, 128, 255, 255);
					s32	PreviousNode = (TestNode + gTrainTracks[trackIndex].m_numNodes - 1) % gTrainTracks[trackIndex].m_numNodes;
					// Also display a line to show the direction of platform
					Vector3	vRight, vAlongTrack = gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() - gTrainTracks[trackIndex].m_pNodes[PreviousNode].GetCoors();
					vRight.Cross(vAlongTrack, Vector3(0.0f, 0.0f, 1.0f));
					if(gTrainTracks[trackIndex].m_pNodes[TestNode].GetStationType() == CTrainTrackNode::ST_RIGHT)
					{
						vRight = -vRight;
					}
					vRight.Normalize();
					CVectorMap::DrawLine(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors(),
						gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + vRight,
						stationColour, stationColour);

					CVectorMap::DrawCircle((gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors() + Vector3(0.0f, 0.0f, 1.0f)),
						0.25f,
						stationColour);
				}

				CVectorMap::DrawLine(Vector3(gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoorsX(), gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoorsY(), 0.0f),
					Vector3(gTrainTracks[trackIndex].m_pNodes[(TestNode+1)%gTrainTracks[trackIndex].m_numNodes].GetCoorsX(), gTrainTracks[trackIndex].m_pNodes[(TestNode+1)%gTrainTracks[trackIndex].m_numNodes].GetCoorsY(), 0.0f),
					Color32(255, 255, 255, 255), Color32(255, 255, 255, 255));

			}
		}
	}

	//*******************************
	// Debug the trains themselves

	if(sm_bDisplayTrainAndTrackDebug)
	{
		fwSceneUpdate::RunCustomCallbackBits(CGameWorld::SU_TRAIN, SceneUpdateDebugDisplayTrainAndTrack);
	}
#endif // DEBUG_DRAW
}

void CTrain::SetTrackActive(int trackindex, bool trainactive)
{
	trainDebugf2("CTrain::SetTrackActive trackindex[%d] trainactive[%d]",trackindex,trainactive);

	if (trackindex < MAX_TRAIN_TRACKS_PER_LEVEL)
	{
		gTrainTracks[trackindex].m_bTrainActive = trainactive;
		gTrainTracks[trackindex].m_iLastTrainReportingTimeMS = trainactive ? fwTimer::GetTimeInMilliseconds() : 0;
	}
}

bool CTrain::GetTrackActive(int trackindex)
{
	if (trackindex < MAX_TRAIN_TRACKS_PER_LEVEL)
	{
		return gTrainTracks[trackindex].m_bTrainActive;
	}

	return false;
}

bool CTrain::DetermineHostApproval(int trackindex)
{
	bool approval = false;

	if (trackindex < MAX_TRAIN_TRACKS_PER_LEVEL)
	{
		if (!gTrainTracks[trackindex].m_bTrainActive)
		{
			SetTrackActive(trackindex,true);
			approval = true;
		}
	}

	trainDebugf2("CTrain::DetermineHostApproval trackindex[%d] approval[%d]",trackindex,approval);
	return approval;
}

void CTrain::ReceivedHostApprovalData(int trackindex, bool trainapproval)
{
	trainDebugf2("CTrain::ReceivedHostApprovalData trackindex[%d] trainapproval[%d]",trackindex,trainapproval);

	if (trainapproval)
	{
		gTrainTracks[trackindex].m_bTrainProcessNetworkCreation = true;

		if (trackindex < MAX_TRAIN_TRACKS_PER_LEVEL)
		{
			gTrainTracks[trackindex].m_bTrainNetworkCreationApproval = trainapproval;

			ProcessTrainNetworkPending(trackindex);
		}
	}
}

#if !__FINAL
const char* CTrain::GetTrainStateString()
{
	switch(m_nTrainState)
	{
		case TS_Moving:
		{
			return "Moving";
		}
		case TS_ArrivingAtStation:
		{
			return "Arriving At Station";
		}
		case TS_OpeningDoors:
		{
			return "Opening Doors";
		}
		case TS_WaitingAtStation:
		{
			return "Waiting At Station";
		}
		case TS_ClosingDoors:
		{
			return "Closing Doors";
		}
		case TS_LeavingStation:
		{
			return "Leaving Station";
		}
		case TS_Destroyed:
		{
			return "Destroyed";
		}
	}

	return "Unknown";
}

const char* CTrain::GetPassengerStateString()
{
	switch(m_nPassengersState)
	{
		case PS_None:
			{
				return "None";
			}
		case PS_GetOff:
			{
				return "Get Off";
			}
		case PS_WaitingForGetOff:
			{
				return "Waiting For Get Off";
			}
		case PS_GetOn:
			{
				return "Get On";
			}
		case PS_WaitingForGetOn:
			{
				return "Waiting For Get On";
			}
	}

	return "Unknown";
}

const int CTrain::GetFullTrainPassengerCount()
{
	int count = 0;

	CTrain* p = m_nTrainFlags.bEngine ? this : FindEngine(this);
	while(p)
	{
		count += p->m_aPassengers.GetCount();
		p = p->GetLinkedToBackward();
	}

	return count;
}
#endif

#if DEBUG_DRAW
void CTrain::SceneUpdateDebugDisplayTrainAndTrack(fwEntity &entity, void *UNUSED_PARAM(userData))
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	const float fVisDist = 1000.0f;
	CEntity * pEntity = (CEntity*) &entity;

	Assert(pEntity->GetIsTypeVehicle() && ((CVehicle*)pEntity)->GetVehicleType()==VEHICLE_TYPE_TRAIN);

	CTrain * pTrain = (CTrain*)pEntity;

	const Vector3 carriageCenter = VEC3V_TO_VECTOR3(pTrain->GetTransform().GetPosition());

	float fDist = (vOrigin - carriageCenter).Mag();

	if(fDist > fVisDist)
		fDist = fVisDist;

	const float fAlpha = 1.0f - (fDist / fVisDist);
	const int iAlpha = (int)(fAlpha * 255.0f);

	if(sm_bDisplayTrainAndTrackDebugOnVMap)
	{
		CVectorMap::DrawCircle(carriageCenter, 0.25f, rage::Color32(0.0f, pTrain->m_updateLODCurrentAlpha, 1.0f - pTrain->m_updateLODCurrentAlpha, 1.0f));
	}

	Vector3 carriageSideDisplayOffset = 3.0f * VEC3V_TO_VECTOR3(pTrain->GetTransform().GetA());
	if(pTrain->m_nTrainFlags.iStationPlatformSides == CTrainTrack::Left)
	{
		carriageSideDisplayOffset = -carriageSideDisplayOffset;
	}
	Vector3 carriageSideDisplayEndpoint = carriageCenter + carriageSideDisplayOffset;
	if(sm_bDisplayTrainAndTrackDebug)
	{
		grcDebugDraw::Line(carriageCenter, carriageSideDisplayEndpoint, rage::Color32(0xff, 0xff, 0xff, iAlpha), rage::Color32(0xff, 0xff, 0x00, iAlpha));
	}
	if(sm_bDisplayTrainAndTrackDebugOnVMap)
	{
		CVectorMap::DrawLine(carriageCenter, carriageSideDisplayEndpoint, rage::Color32(0xff, 0xff, 0xff, 0xff), rage::Color32(0xff, 0xff, 0x00, 0xff));
	}

	if(pTrain->m_nTrainFlags.bEngine)
	{
		if(sm_bDisplayTrainAndTrackDebug)
		{
			grcDebugDraw::Sphere(carriageSideDisplayEndpoint, 0.25f, rage::Color32(0xff, 0xff, 0x00, iAlpha));
		}
		if(sm_bDisplayTrainAndTrackDebugOnVMap)
		{
			CVectorMap::DrawCircle(carriageSideDisplayEndpoint, 0.25f, rage::Color32(0xff, 0xff, 0x00, 0xff));
		}
	}

	if(pTrain->m_nTrainFlags.bCaboose)
	{
		if(sm_bDisplayTrainAndTrackDebug)
		{
			grcDebugDraw::Sphere(carriageSideDisplayEndpoint, 0.25f, rage::Color32(0xff, 0x00, 0xff, iAlpha));
		}
		if(sm_bDisplayTrainAndTrackDebugOnVMap)
		{
			CVectorMap::DrawCircle(carriageSideDisplayEndpoint, 0.25f, rage::Color32(0xff, 0x00, 0xff, 0xff));
		}
	}

	if(sm_bDisplayTrainAndTrackDebug)
	{
		Color32 iDisplayCol = Color_yellow;
		iDisplayCol.SetAlpha(iAlpha);
		char debugText[128];
		formatf(debugText, "%p : %s%s", pTrain, pTrain->m_nTrainFlags.bEngine ? "Engine" : "Carriage", pTrain->m_nTrainFlags.bCaboose ? "/Caboose" : "");
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, debugText);

		//Check the train state.
		const char* pTrainState = pTrain->GetTrainStateString();
		
		//Check the passengers state.
		const char* pPassengersState = NULL;
		switch(pTrain->m_nPassengersState)
		{
			case PS_None:
			{
				pPassengersState = "None";
				break;
			}
			case PS_GetOn:
			{
				pPassengersState = "Get On";
				break;
			}
			case PS_WaitingForGetOn:
			{
				pPassengersState = "Waiting For Get On";
				break;
			}
			case PS_GetOff:
			{
				pPassengersState = "Get Off";
				break;
			}
			case PS_WaitingForGetOff:
			{
				pPassengersState = "Waiting For Get Off";
				break;
			}
			default:
			{
				vehicleAssertf(false, "Unknown passengers state: %d.", pTrain->m_nPassengersState);
				pPassengersState = "Unknown";
				break;
			}
		}
		
		const char * pLod = NULL;
		switch(pTrain->m_updateLODState)
		{
			case SIMPLIFIED: pLod="Simplified"; break;
			case FADING_IN: pLod="Fading-in"; break;
			case FADING_OUT: pLod="Fading-out"; break;
			case NORMAL: default: pLod="Normal"; break;
		}

		formatf(debugText, "State: %s (%s)", pTrainState, pPassengersState);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		
		formatf(debugText, "Time in state: %.2f", pTrain->m_fTimeInTrainState);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*2, debugText);
		
		formatf(debugText, "Lod: %s", pLod);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*3, debugText);

		formatf(debugText, "Carriage center: (%.2f, %.2f, %.2f)", carriageCenter.x, carriageCenter.y, carriageCenter.z);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*4, debugText);

		formatf(debugText, "Speed: %.2f", pTrain->m_trackForwardSpeed);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*5, debugText);

		formatf(debugText, "Travel dist to next station: %.2f", pTrain->m_travelDistToNextStation);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*6, debugText);

		formatf(debugText, "Track Pos: %.2f", pTrain->m_trackPosFront);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*7, debugText);

		formatf(debugText, "pEngine: 0x%x, pLinkedToBackward: 0x%x, pLinkedToForward: 0x%x", pTrain->m_pEngine.Get(), pTrain->GetLinkedToBackward(), pTrain->GetLinkedToForward());
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*8, debugText);

		formatf(debugText, "Current node: %d", pTrain->m_currentNode);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*9, debugText);

		formatf(debugText, "Old current node: %d", pTrain->m_oldCurrentNode);
		grcDebugDraw::Text(carriageSideDisplayEndpoint, iDisplayCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*10, debugText);
	}
}
#endif // DEBUG_DRAW

#if !__FINAL
void CTrain::DumpTrainInformation()
{
	Displayf(TBlue"---------------------------------------------------" TNorm);

	int trainCount = 0;
	for (s8 t = 0; t < MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		if (gTrainTracks[t].m_bTrainActive)
		{
			trainCount++;

			CVehicle::Pool *VehPool = CVehicle::GetPool();
			CVehicle* pVeh;
			s32 i = (s32) VehPool->GetSize();
			CTrain* pTrainPresent = NULL;
			while(i-- && pTrainPresent == NULL)
			{
				pVeh = VehPool->GetSlot(i);
				if(pVeh && pVeh->InheritsFromTrain() &&
					(static_cast<CTrain*>(pVeh))->m_nTrainFlags.bEngine &&
					(static_cast<CTrain*>(pVeh))->m_trackIndex == t)
				{
					pTrainPresent = static_cast<CTrain*>(pVeh);
				}
			}

			CPed* pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			if (pTrainPresent && pLocalPlayer)
			{
				Vector3 playerLoc = MAT34V_TO_MATRIX34(pLocalPlayer->GetMatrix()).d;
				playerLoc.z += 1.0f;
				Vector3 trainTrackLoc = MAT34V_TO_MATRIX34(pTrainPresent->GetMatrix()).d;
				float fDistanceToPlayer = fabs((trainTrackLoc - playerLoc).Mag());

				Displayf(TBlue"Train[%d] active[%d] ACTUAL dt[%.1f] spd[%1.f] ds[%1.f] pos[%1.f] local[%d] st[%s] " TNorm,t,gTrainTracks[t].m_bTrainActive,fDistanceToPlayer,pTrainPresent->m_trackForwardSpeed,pTrainPresent->m_travelDistToNextStation,pTrainPresent->m_trackPosFront,!pTrainPresent->IsNetworkClone(),pTrainPresent->GetTrainStateString());
			}
			else
			{
				Displayf(TBlue"Train[%d] active[%d]" TNorm,t,gTrainTracks[t].m_bTrainActive);
			}
		}
	}

	Displayf(TBlue"Total active trains: %d" TNorm, trainCount);
	Displayf(TBlue"---------------------------------------------------" TNorm);
}
#else
void CTrain::DumpTrainInformation()
{
}
#endif

//////// The following functions are used by the level designers to create trains for missions.

/////////////////////////////////////////////////////////////
// FUNCTION: DisableRandomTrains
// PURPOSE:  Switches off the creation of random trains.
/////////////////////////////////////////////////////////////
void CTrain::DisableRandomTrains(bool bDisable)
{
	sm_bDisableRandomTrains = bDisable;
}

void CTrain::SwitchTrainTrackFromScript(const s32 iTrackIndex, const bool bSwitchOn, const scrThreadId iThreadId)
{
	Assert(iThreadId);
	Assert(iTrackIndex >= 0 && iTrackIndex < MAX_TRAIN_TRACKS_PER_LEVEL);

	if(iThreadId==0 || iTrackIndex < 0 || iTrackIndex >= MAX_TRAIN_TRACKS_PER_LEVEL)
		return;

	CTrainTrack & track = gTrainTracks[iTrackIndex];

	Assertf(track.m_numNodes > 0, "This is not a valid train track for this level.  Num nodes is zero.\n");

	if(track.m_numNodes <= 0)
		return;

	Assertf(track.m_iTurnedOffByScriptThreadID==0 || track.m_iTurnedOffByScriptThreadID==iThreadId, "This track is already under control by another script.\n");

	if(track.m_iTurnedOffByScriptThreadID !=0 && track.m_iTurnedOffByScriptThreadID != iThreadId)
		return;

	if(bSwitchOn && (iThreadId == track.m_iTurnedOffByScriptThreadID || track.m_iTurnedOffByScriptThreadID == THREAD_INVALID))
	{
		track.m_iTurnedOffByScriptThreadID = THREAD_INVALID;
		track.m_bInitiallySwitchedOff = false;

#if __BANK
		track.m_TurnedOffByScriptName[0] = 0;
#endif
		return;
	}

	if(!bSwitchOn && track.m_iTurnedOffByScriptThreadID==THREAD_INVALID)
	{
		track.m_iTurnedOffByScriptThreadID = iThreadId;
		track.m_bInitiallySwitchedOff = false;

#if __BANK
		scrThread * pScriptThread = scrThread::GetThread(iThreadId);
		if(pScriptThread)
			strncpy(track.m_TurnedOffByScriptName, pScriptThread->GetScriptName(), 64);
#endif
		return;
	}

	return;
}

void CTrain::SetTrainTrackSpawnFrequencyFromScript(const s32 iTrackIndex, const s32 iSpawnFreqMS, const scrThreadId iThreadId)
{
	Assert(iThreadId);
	Assert(iTrackIndex >= 0 && iTrackIndex < MAX_TRAIN_TRACKS_PER_LEVEL);

	if(iThreadId==0 || iTrackIndex < 0 || iTrackIndex >= MAX_TRAIN_TRACKS_PER_LEVEL)
		return;

	CTrainTrack & track = gTrainTracks[iTrackIndex];
	Assertf(track.m_numNodes > 0, "This is not a valid train track for this level.  Num nodes is zero.\n");
	if(track.m_numNodes <= 0)
		return;

	Assertf(track.m_iSpawnFreqThreadID==0 || track.m_iSpawnFreqThreadID==iThreadId, "This track is already under control by another script.\n");
	if(track.m_iSpawnFreqThreadID !=0 && track.m_iSpawnFreqThreadID != iThreadId)
		return;

	track.m_iSpawnFreqThreadID = iThreadId;
	track.m_iScriptTrainSpawnFreqMS = iSpawnFreqMS;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : RestoreScriptModificationsToTracks
// PURPOSE : Called by mission-cleanup code to restore tracks which
//           have been switched off by the terminating script.
//////////////////////////////////////////////////////////////////////

void CTrain::RestoreScriptModificationsToTracks(const scrThreadId iThreadId)
{
	Assert(iThreadId);

	for(int t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		CTrainTrack & track = gTrainTracks[t];

		// Was this track turned off by the mission script which is currently quitting?
		if(track.m_iTurnedOffByScriptThreadID == iThreadId)
		{
			// Allow trains to be created on this track again
			track.m_iTurnedOffByScriptThreadID = THREAD_INVALID;
		}

		// Was the spawn frequency controlled by this thread
		if(track.m_iSpawnFreqThreadID == iThreadId)
		{
			// Reset overridden spawn frequency
			track.m_iScriptTrainSpawnFreqMS = -1;
		}
	}
}

/////////////////////////////////////////////////////////////
// FUNCTION: CreateTrain
// PURPOSE:  This is the main function for the level designers. It creates a train to be used
//			 in a mission.
// NOTE:	 It is up to the level designers to make sure the models are streamed in before
//			 this function is called
/////////////////////////////////////////////////////////////
#define MAXCARRIAGESINCONFIG (100)
void CTrain::CreateTrain(
	s8 trackIndex,
	s32 trackNodeForEngine,
	bool bDirection,
	u8 trainConfigIndex,
	CTrain** ppEngine_out,
	CTrain** ppLastCarriage_out,
	scrThreadId iMissionScriptId)
{
	PF_AUTO_PUSH_DETAIL("Create Train");
	trainDebugf2("CTrain::CreateTrain trackIndex[%d] trackNodeForEngine[%d] bDirection[%d] trainConfigIndex[%d]",trackIndex,trackNodeForEngine,bDirection,trainConfigIndex);

	Assert(trackIndex >= 0);
	Assert(trackNodeForEngine >= 0);
	Assert(trackNodeForEngine < gTrainTracks[trackIndex].m_numNodes);

	const bool bIsMissionTrain = (iMissionScriptId!=THREAD_INVALID);

	Assert(bIsMissionTrain || !gTrainTracks[trackIndex].IsSwitchedOff());

	if(ppEngine_out){*ppEngine_out = NULL;}
	if(ppLastCarriage_out) *ppLastCarriage_out = NULL;

	CTrain	*pTrainsToBeAdded[MAXCARRIAGESINCONFIG];
	u32	NumTrainsToBeAdded = 0;

	const Vector3 worldPosForEngineStartNodeCenter = gTrainTracks[trackIndex].m_pNodes[trackNodeForEngine].GetCoors();
	const float trackPosEngineStartNodeCenter = gTrainTracks[trackIndex].m_pNodes[trackNodeForEngine].GetLengthFromStart();
	const float totalTrackLength = gTrainTracks[trackIndex].GetTotalTrackLength();

	CTrain* pNewCarriage = NULL;
	CTrain* pEngineCarriage = NULL;
	CTrain* pPreviousCarriage = NULL;
	float previousCarriageTrackDistFrontToNextCarriageFront = 0.0f;
	const int carriageCount = gTrainConfigs.m_trainConfigs[trainConfigIndex].m_carriages.GetCount();
	trainDebugf2("CTrain::CreateTrain carriageCount[%d]",carriageCount);
	for(u8 i = 0; i < carriageCount; ++i)
	{
		u32 modelIndex = gTrainConfigs.m_trainConfigs[trainConfigIndex].m_carriages[i].m_modelId.Get();
		fwModelId modelId((strLocalIndex(modelIndex)));

		Assertf( CModelInfo::HaveAssetsLoaded(modelId), "CTrain::CreateTrain - assets are not loaded for model \"%s\"!", CModelInfo::GetBaseModelInfoName(modelId) );

		if (!modelId.IsValid())
		{
			trainDebugf2("CTrain::CreateTrain !pNewCarriage i[%d] --> return",i);
			return;
		}	

		Matrix34 M;
		M.Identity();
		M.d = worldPosForEngineStartNodeCenter;
		pNewCarriage = (CTrain*)CVehicleFactory::GetFactory()->Create(modelId, bIsMissionTrain ? ENTITY_OWNEDBY_SCRIPT : ENTITY_OWNEDBY_POPULATION, bIsMissionTrain ? POPTYPE_MISSION : POPTYPE_RANDOM_PERMANENT, &M);

#if __ASSERT
		if(!pNewCarriage)
		{
			Displayf("CTrain::CreateTrain failed to create carriage %i, on track %i, using config %i", i, trackIndex, trainConfigIndex);
			Displayf("Mission script ID was %u", static_cast<u32>(iMissionScriptId));
			Displayf("Name of carriage was \"%s\"", gTrainConfigs.m_trainConfigs[trainConfigIndex].m_carriages[i].m_modelName);
			Displayf("Pointer returned by CModelInfo::GetBaseModelInfo() : %p", CModelInfo::GetBaseModelInfo(modelId));
			Displayf("FRAGCACHEMGR->CanGetNewCacheEntries(1) : %s", FRAGCACHEMGR->CanGetNewCacheEntries(1) ? "true" : "false");
		}
		Assertf(pNewCarriage, "CTrain::CreateTrain !pNewCarriage - please attach TTY");
#endif

		if (!pNewCarriage)
		{
			trainDebugf2("CTrain::CreateTrain !pNewCarriage i[%d] --> return",i);
			return;
		}

		trainDebugf2("CTrain::CreateTrain pNewCarriage[%p][%s][%s] i[%d]",pNewCarriage,pNewCarriage->GetModelName(),pNewCarriage->GetNetworkObject() ? pNewCarriage->GetNetworkObject()->GetLogName() : "",i);

		Assert(pNewCarriage->InheritsFromTrain());

		pNewCarriage->m_trainConfigIndex = trainConfigIndex;
		pNewCarriage->m_trainConfigCarriageIndex = i;
		pNewCarriage->m_trackIndex = (s8)trackIndex;

		// Determine the offset from the front of the engine to the front of this carriage.
		if(!pPreviousCarriage)
		{
			pNewCarriage->m_trackDistFromEngineFrontForCarriageFront = 0.0f;
		}
		else
		{
			pNewCarriage->m_trackDistFromEngineFrontForCarriageFront = pPreviousCarriage->m_trackDistFromEngineFrontForCarriageFront + previousCarriageTrackDistFrontToNextCarriageFront;
			Assert(pNewCarriage->m_trackDistFromEngineFrontForCarriageFront > -totalTrackLength);
			Assert(pNewCarriage->m_trackDistFromEngineFrontForCarriageFront < (2.0f * totalTrackLength));
		}

		// Determine the track distance to the next carriage.
		// Used next time through the loop.
		float carriageTrackDistFrontToNextCarriageFront = 0.0f;
		const float carriageLength = pNewCarriage->GetBoundingBoxMax().y - pNewCarriage->GetBoundingBoxMin().y;
		const float carriageHalfLength = carriageLength * 0.5f;
		if(pNewCarriage->m_trainConfigIndex >= 0)
		{
			if(gTrainConfigs.m_trainConfigs[pNewCarriage->m_trainConfigIndex].m_carriagesUseEvenTrackSpacing)
			{
				carriageTrackDistFrontToNextCarriageFront = totalTrackLength / gTrainConfigs.m_trainConfigs[pNewCarriage->m_trainConfigIndex].m_carriages.GetCount();
			}
			else
			{
				carriageTrackDistFrontToNextCarriageFront = carriageLength + gTrainConfigs.m_trainConfigs[pNewCarriage->m_trainConfigIndex].m_carriageGap;
			}
		}
		Assert((carriageTrackDistFrontToNextCarriageFront - carriageLength) >= 0.0f 
			|| gTrainConfigs.m_trainConfigs[pNewCarriage->m_trainConfigIndex].m_carriageGap < 0.0f );
		previousCarriageTrackDistFrontToNextCarriageFront = (bDirection)?-carriageTrackDistFrontToNextCarriageFront:carriageTrackDistFrontToNextCarriageFront;// Used next time through the loop.

		pNewCarriage->m_nVehicleFlags.bCannotRemoveFromWorld = true;
		pNewCarriage->m_currentNode = trackNodeForEngine;
		const float trackPosEngineFront = trackPosEngineStartNodeCenter + ((bDirection)?carriageHalfLength:-carriageHalfLength);
		pNewCarriage->m_trackPosFront = trackPosEngineFront + pNewCarriage->m_trackDistFromEngineFrontForCarriageFront;
		FixupTrackPos(pNewCarriage->m_trackIndex, pNewCarriage->m_trackPosFront);
		pNewCarriage->m_iMissionScriptId = iMissionScriptId;
		pNewCarriage->m_nTrainFlags.bDirectionTrackForward = bDirection;
		if(iMissionScriptId != THREAD_INVALID)
			pNewCarriage->m_nTrainFlags.bStopForStations = false;

		// Try to position the train as closely as possible to the original
		// coordinates.
		{
			Vector3 carriageWorldPosFront;
			s32	currentNode = FindCurrentNode(pNewCarriage->m_trackIndex, pNewCarriage->m_trackPosFront, pNewCarriage->m_nTrainFlags.bDirectionTrackForward);
			s32	nextNode = currentNode + ((pNewCarriage->m_nTrainFlags.bDirectionTrackForward)?(1):(-1));
			FixupTrackNode(trackIndex, nextNode);
			Assert(currentNode != nextNode);
			if(currentNode > nextNode){SwapEm(currentNode, nextNode);}
			Assert(currentNode < nextNode);
			// Pass currentNode and nextNode by reference here, so we can use them later
			bool bIsTunnel = false;
			GetWorldPosFromTrackPos(carriageWorldPosFront, currentNode, nextNode, trackIndex, pNewCarriage->m_trackPosFront, carriageLength, bIsTunnel);
// DEBUG!! -AC, This should add the half carriage bound box offset...
			pNewCarriage->SetPosition(carriageWorldPosFront);
// END DEBUG!!
			pNewCarriage->SetTrackPosFromWorldPos(currentNode, nextNode);

			pNewCarriage->m_nVehicleFlags.bIsInTunnel = bIsTunnel;
		}

		if(!pPreviousCarriage)
		{
			float	stationBrakingMaxSpeed			= 0.0f;
			//bool	bPlatformSideForClosestStation	= false;
			int	iPlatformSideForClosestStation	= 0;
			pNewCarriage->GetStationRelativeInfo(pNewCarriage->m_currentStation, pNewCarriage->m_nextStation, pNewCarriage->m_travelDistToNextStation, stationBrakingMaxSpeed, iPlatformSideForClosestStation);

			s32 NewSpeed = gTrainTracks[pNewCarriage->m_trackIndex].m_maxSpeed;
			SetCarriageSpeed(pNewCarriage, MIN(NewSpeed, stationBrakingMaxSpeed));
			SetCarriageCruiseSpeed(pNewCarriage, (float)NewSpeed);
			//pNewCarriage->m_nTrainFlags.bStationOnRightHandSide = bPlatformSideForClosestStation;
			pNewCarriage->m_nTrainFlags.iStationPlatformSides = iPlatformSideForClosestStation;
		}
		else
		{
			SetCarriageSpeed(pNewCarriage, GetCarriageSpeed(pPreviousCarriage));
			SetCarriageCruiseSpeed(pNewCarriage, GetCarriageCruiseSpeed(pPreviousCarriage));
		}

		// Add the trains to the world in reverse order. This way they are updated in the right order. (Engine first and then the carriages front to back)
		pTrainsToBeAdded[NumTrainsToBeAdded++] = pNewCarriage;
		Assert(NumTrainsToBeAdded < MAXCARRIAGESINCONFIG);

		CGameWorld::Add(pNewCarriage, CGameWorld::OUTSIDE );

		// Set these two flags so that the lights work properly (white in front, red at the back)
		if(!pPreviousCarriage)
		{
			pNewCarriage->m_nTrainFlags.bEngine = true;
			if(ppEngine_out){*ppEngine_out = pNewCarriage;}

			// We no longer spawn a driver: we will call SetUpWithPretendOccupants()
			// further down, which wouldn't remove the driver, so that would put us in
			// a strange state where we think we are in pretend occupant mode but still
			// have a real occupant.
			//	if(!bIsMissionTrain)
			//	{
			//		pNewCarriage->SetUpDriver(false, false);
			//	}
		}
		else
		{
			pNewCarriage->m_nTrainFlags.bEngine = false;
			pNewCarriage->m_nVehicleFlags.bHasBeenOwnedByPlayer = true;
			pNewCarriage->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
			pPreviousCarriage->m_nTrainFlags.bCaboose = false;
		}
		pNewCarriage->m_nTrainFlags.bCaboose = true;
		if(ppLastCarriage_out)
		{
			*ppLastCarriage_out = pNewCarriage;
		}

		// Link to the Engine carriage (if there is one yet).
		pNewCarriage->SetEngine( (pEngineCarriage)?pEngineCarriage:pNewCarriage );

		// Link us to the train in front of us.
		pNewCarriage->SetLinkedToForward( pPreviousCarriage );
		pNewCarriage->SetLinkedToBackward( NULL );
		// Link the train in front of us to us.
		if(pPreviousCarriage)
		{
			pPreviousCarriage->SetLinkedToBackward( pNewCarriage );
		}

		// Update the coordinates of this carriage (Otherwise all trains are created at (0.0f, 0.0f, 0.0f) and remain there till next update)
		//pNewCarriage->ProcessControl();
		pNewCarriage->UpdatePosition(fwTimer::GetTimeStep());		

		if(!pPreviousCarriage)
		{
			pEngineCarriage = pNewCarriage;
		}
		pPreviousCarriage = pNewCarriage;
	}

	trainDebugf2("CTrain::CreateTrain carriage creation complete");

	// Now add the trains to the world in reverse order. (Is this still needed under rage?)
	for (s32 C = NumTrainsToBeAdded-1; C >= 0; C--)
	{
		Assert(pTrainsToBeAdded[C]->GetEngine());

		// get these new trains to check for going into interiors & keep checking until collisions are loaded
		pTrainsToBeAdded[C]->GetPortalTracker()->RequestRescanNextUpdate();

		// set trains to alpha fade if they were created in-view, and in the distance.
		if(camInterface::GetDominantRenderedCamera())
		{
			static dev_float fMinDistSqr = 40.0f*40.0f;
			Vector3 vPos = VEC3V_TO_VECTOR3(pTrainsToBeAdded[C]->GetTransform().GetPosition());
			const camFrame & frame = camInterface::GetDominantRenderedCamera()->GetFrame();
			Vector3 vCamToTrain = vPos - frame.GetPosition();
			// Train must be distant
			if( vCamToTrain.Mag2() > fMinDistSqr)
			{
				// Train must be in front of the view plane
				if( frame.GetFront().Dot(vCamToTrain) > pTrainsToBeAdded[C]->GetBoundRadius() )
				{
					// Train must be within the left/right/top/bottom frustum planes
					CViewport* gameViewport = gVpMan.GetGameViewport();
					if(gameViewport)
					{
						spdAABB aabb;
						pTrainsToBeAdded[C]->GetAABB(aabb);
						bool bVisible = gameViewport->GetGrcViewport().IsAABBVisible(aabb.GetMin().GetIntrin128ConstRef(), aabb.GetMax().GetIntrin128ConstRef(), gameViewport->GetGrcViewport().GetFrustumLRTB()) != 0;

						// If train is visible, then we can set up an alpha fade
						if(bVisible)
						{
							pTrainsToBeAdded[C]->GetLodData().SetResetDisabled(false);
							pTrainsToBeAdded[C]->GetLodData().SetResetAlpha(true);
						}
					}
				}
			}
		}
	}

	Assert(*ppEngine_out && (*ppEngine_out)->m_nTrainFlags.bEngine);

	// If any carriages are passenger carriages, then flag each with 'bHasPassengerCarriages' flag
	CTrain* pCarriage = *ppEngine_out;
	while(pCarriage)
	{
		// In all carriages can take passengers
		pCarriage->m_nTrainFlags.bHasPassengerCarriages = true;

		pCarriage->m_TimeSliceLastProcessed = CPhysics::GetTimeSliceId() - 1;

		if(!bIsMissionTrain)
		{
			pCarriage->SetUpWithPretendOccupants();
		}

		pCarriage = pCarriage->GetLinkedToBackward();
	}

	trainDebugf2("CTrain::CreateTrain complete");
}


/////////////////////////////////////////////////////////////
// FUNCTION: RemoveMissionTrains
// PURPOSE:  Removes all engines/carriages that are part of a mission train
/////////////////////////////////////////////////////////////
void CTrain::RemoveMissionTrains(scrThreadId iCurrentThreadId)
{
	trainDebugf2("CTrain::RemoveMissionTrains");

	CVehicle::Pool 	*VehiclePool = CVehicle::GetPool();
	CVehicle		*pVeh;

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);

		if(pVeh && pVeh->InheritsFromTrain() && pVeh != FindPlayerVehicle())
		{
			CTrain * pTrain = static_cast<CTrain*>(pVeh);
			if(iCurrentThreadId != THREAD_INVALID && pTrain->GetMissionScriptId() == iCurrentThreadId)
			{
				CVehicleFactory::GetFactory()->Destroy(pVeh);
			}
		}
	}
}


/////////////////////////////////////////////////////////////
// FUNCTION: RemoveOneMissionTrain
// PURPOSE:  Removes just the one train that we have the engine pointer to.
/////////////////////////////////////////////////////////////
void CTrain::RemoveOneMissionTrain(GtaThread& ASSERT_ONLY(iCurrentThread), CTrain* pEngine)
{
	trainDebugf2("CTrain::RemoveOneMissionTrain");

	AssertEntityPointerValid_NotInWorld(pEngine);
	Assert(pEngine->GetIsMissionTrain());
	Assert(pEngine->m_nTrainFlags.bEngine || pEngine->m_nTrainFlags.bAllowRemovalByPopulation);

	CTrain	*pThisCar = pEngine, *pNextCar;

	while(pThisCar)
	{
		Assert(pThisCar->GetMissionScriptId() == iCurrentThread.GetThreadId() || (NetworkInterface::IsGameInProgress() && *pThisCar->GetScriptThatCreatedMe() == iCurrentThread.m_Handler->GetScriptId()));

		pNextCar = pThisCar->GetLinkedToBackward();

		CVehicleFactory::GetFactory()->Destroy(pThisCar);

		pThisCar = pNextCar;
	}
}

void CTrain::RemoveTrainsFromArea(const Vector3 &Centre, float Radius)
{
	trainDebugf2("CTrain::RemoveTrainsFromArea Centre[%f %f %f] Radius[%f]",Centre.x,Centre.y,Centre.z,Radius);

	//Trains are only removed if they are forced by a 0 radius, or if the radius is greater than the minimum radius.
	//This helps to ensure that trains players are standing on are not removed if radius example 2.5 come through.
	static const float sm_TrainMinRadius = 50.f;
	if (Radius > 0.f && Radius < sm_TrainMinRadius)
		return;

	CVehicle::Pool 	*VehiclePool = CVehicle::GetPool();
	CVehicle		*pVeh;

	float Radius2 = Radius * Radius;

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);

		if( pVeh && pVeh->InheritsFromTrain() && (!pVeh->GetNetworkObject() || (!pVeh->IsNetworkClone() && !pVeh->GetNetworkObject()->IsPendingOwnerChange())) )
		{
			bool bProcess = true;
			if ( Radius2 > 0.f )
			{
				float dist2 = (VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()) - Centre).Mag2();
				if (dist2 >= Radius2)
					bProcess = false;
			}

			if (((CTrain*)pVeh)->m_nTrainFlags.bIsCutsceneControlled)
				bProcess = false;
			
			if (bProcess)
			{
				bool bPartOfPlayerTrain = false;
				scrThreadId iMissionThreadId = THREAD_INVALID;

				CTrain* pTrain = static_cast<CTrain*>(pVeh);
				while(pTrain)
				{
					if(pTrain == FindPlayerVehicle())
						bPartOfPlayerTrain = true;
					if(pTrain->GetMissionScriptId() != THREAD_INVALID)
						iMissionThreadId = pTrain->GetMissionScriptId();
					pTrain = pTrain->GetLinkedToForward();
				}

				pTrain = static_cast<CTrain*>(pVeh);
				while(pTrain)
				{
					if(pTrain == FindPlayerVehicle())
						bPartOfPlayerTrain = true;
					if(pTrain->GetMissionScriptId() != THREAD_INVALID)
						iMissionThreadId = pTrain->GetMissionScriptId();
					pTrain = pTrain->GetLinkedToBackward();
				}

				if(!bPartOfPlayerTrain && iMissionThreadId == THREAD_INVALID)
				{
#if __BANK
					if (NetworkInterface::IsNetworkOpen())
					{
						pTrain = static_cast<CTrain*>(pVeh);
						trainDebugf2("CTrain::RemoveTrainsFromArea train[%s] m_pEngine[%p].IsNetworkClone[%d]",pVeh && pVeh->GetNetworkObject() ? pVeh->GetNetworkObject()->GetLogName() : "",pTrain ? pTrain->GetEngine() : 0,pTrain && pTrain->GetEngine() ? pTrain->GetEngine()->IsNetworkClone() : false);
					}
#endif
					CVehicleFactory::GetFactory()->Destroy(pVeh);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION: RemoveAllTrains
// PURPOSE:  Removes all ambient engines/carriages which are not in use by a mission script
//////////////////////////////////////////////////////////////////////////////////////////////

void CTrain::RemoveAllTrains()
{
	trainDebugf2("CTrain::RemoveAllTrains");

	RemoveTrainsFromArea(ORIGIN,0.f);
}


/////////////////////////////////////////////////////////////
// FUNCTION: ReleaseMissionTrains
// PURPOSE:  Releases all engines/carriages that are part of a mission train.
//			 They can now be removed once they are sufficiently far from the camera.
//			 iReleaseTrainFlags - combination of flags from ReleaseTrainFlags enum
/////////////////////////////////////////////////////////////
void CTrain::ReleaseMissionTrains(scrThreadId iCurrentThreadId, const u32 iReleaseTrainFlags)
{
	trainDebugf2("CTrain::ReleaseMissionTrains");

	CVehicle::Pool 	*VehiclePool = CVehicle::GetPool();
	CVehicle		*pVeh;

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);

		if(pVeh && pVeh->InheritsFromTrain()) //&& pVeh != FindPlayerVehicle())
		{
			CTrain*pTrain = static_cast<CTrain*>(pVeh);

			if(iCurrentThreadId != THREAD_INVALID && pTrain->GetMissionScriptId() == iCurrentThreadId)
			{
				pTrain->m_iMissionScriptId = THREAD_INVALID;
				pTrain->m_nTrainFlags.bStopForStations = true;		// give it normal behaviour.
				pTrain->ChangeTrainState(TS_Moving);

				if(pTrain->m_nTrainFlags.bRenderAsDerailed)
				{
					SetCarriageCruiseSpeed(pTrain, 0.0f);
				}
				else
				{
					if((iReleaseTrainFlags & RTF_KeepOldSpeed)==0)
					{
						const float trackMaxSpeed = (pTrain->m_trackIndex>=0)?static_cast<float>(gTrainTracks[pTrain->m_trackIndex].m_maxSpeed):0.0f;
						SetCarriageCruiseSpeed(pTrain, trackMaxSpeed);
					}
				}

				CTheScripts::UnregisterEntity(pTrain, true);
			}
		}
	}
}


/////////////////////////////////////////////////////////////
// FUNCTION: ReleaseOneMissionTrain
// PURPOSE:  Marks one mission train as being not a mission train. This means it will get released once far from the camera.
//			 iReleaseTrainFlags - combination of flags from ReleaseTrainFlags enum
/////////////////////////////////////////////////////////////
void CTrain::ReleaseOneMissionTrain(GtaThread& ASSERT_ONLY(iCurrentThread), CTrain* pEngine, const u32 iReleaseTrainFlags)
{
	trainDebugf2("CTrain::ReleaseOneMissionTrain");

	AssertEntityPointerValid_NotInWorld(pEngine);
	Assert(pEngine->GetIsMissionTrain());
	Assert(pEngine->m_nTrainFlags.bEngine || pEngine->m_nTrainFlags.bAllowRemovalByPopulation);

	while(pEngine)
	{
		Assert(pEngine->GetMissionScriptId() == iCurrentThread.GetThreadId() || (NetworkInterface::IsGameInProgress() && *pEngine->GetScriptThatCreatedMe() == iCurrentThread.m_Handler->GetScriptId()));

		pEngine->m_iMissionScriptId = THREAD_INVALID;
		pEngine->m_nTrainFlags.bStopForStations = true;		// give it normal behaviour.
		pEngine->ChangeTrainState(TS_Moving);

		if(pEngine->m_nTrainFlags.bRenderAsDerailed)
		{
			SetCarriageCruiseSpeed(pEngine, 0.0f);
		}
		else
		{
			if((iReleaseTrainFlags & RTF_KeepOldSpeed)==0)
			{
				const float trackMaxSpeed = (pEngine->m_trackIndex>=0)?static_cast<float>(gTrainTracks[pEngine->m_trackIndex].m_maxSpeed):0.0f;
				SetCarriageCruiseSpeed(pEngine, trackMaxSpeed);
			}
		}

		CTheScripts::UnregisterEntity(pEngine, true);

		pEngine = pEngine->GetLinkedToBackward();
	}
}


/////////////////////////////////////////////////////////////
// FUNCTION: SetCarriageSpeed
// PURPOSE:  Sets this train to go at a certain speed. (This doesn't change the ai's desired speed)
/////////////////////////////////////////////////////////////
void CTrain::SetCarriageSpeed(CTrain* pCarriage, float newSpeed)
{
	Assert(pCarriage);
	Assert(pCarriage->InheritsFromTrain());

	pCarriage->m_trackForwardSpeed = (pCarriage->m_nTrainFlags.bDirectionTrackForward)?(newSpeed):(-newSpeed);
	pCarriage->m_trackForwardAccPortion = 0.0f;// Used for swinging the carriage forward and back later.
	pCarriage->m_frontBackSwingAngPrev = 0.0f;
}


/////////////////////////////////////////////////////////////
// FUNCTION: GetCarriageSpeed
// PURPOSE:  Gets the trains speed.
/////////////////////////////////////////////////////////////
float CTrain::GetCarriageSpeed(CTrain* pCarriage)
{
	Assert(pCarriage);
	Assert(pCarriage->InheritsFromTrain());

	return pCarriage->m_trackForwardSpeed;
}


/////////////////////////////////////////////////////////////
// FUNCTION: SetCarriageCruiseSpeed
// PURPOSE:  Sets this train to try and go at a certain speed. (This doesn't change the current actual speed.)
/////////////////////////////////////////////////////////////
void CTrain::SetCarriageCruiseSpeed(CTrain* pCarriage, float NewSpeed)
{
	Assert(pCarriage);
	Assert(pCarriage->InheritsFromTrain());

	pCarriage->m_desiredCruiseSpeed = NewSpeed;
}


/////////////////////////////////////////////////////////////
// FUNCTION: GetCarriageCruiseSpeed
// PURPOSE:  Gets the trains normal desired speed. (This doesn't get the current actual speed.)
/////////////////////////////////////////////////////////////
float CTrain::GetCarriageCruiseSpeed(CTrain* pCarriage)
{
	Assert(pCarriage->InheritsFromTrain());

	return pCarriage->m_desiredCruiseSpeed;
}


/////////////////////////////////////////////////////////////
// FUNCTION: FindEngine
// PURPOSE:  This function will return the engine of a train.
/////////////////////////////////////////////////////////////
CTrain* CTrain::FindEngine(const CTrain* pCarriage)
{
#if __ASSERT
	if (!NetworkInterface::IsNetworkOpen())
	{
		Assert(pCarriage);
	}
#endif
	
	Assert( ( pCarriage && !pCarriage->GetEngine() ) || 
			( pCarriage && pCarriage->GetEngine() && (pCarriage->GetEngine()->m_nTrainFlags.bEngine || pCarriage->GetEngine()->m_nTrainFlags.bAllowRemovalByPopulation)) );

    return pCarriage ? pCarriage->m_pEngine : (CTrain*) NULL;
}


/////////////////////////////////////////////////////////////
// FUNCTION: FindCaboose
// PURPOSE:  This function will return the last carriage in a train.
/////////////////////////////////////////////////////////////
CTrain* CTrain::FindCaboose(const CTrain* pCarriage)
{
	CTrain* pCurrentCarriage = const_cast<CTrain*>(pCarriage);
	Assert(pCurrentCarriage);
	while(pCurrentCarriage->GetLinkedToBackward())
	{
		pCurrentCarriage = pCurrentCarriage->GetLinkedToBackward();
	}

	return (pCurrentCarriage);
}


/////////////////////////////////////////////////////////////
// FUNCTION: FindCarriage
// PURPOSE:  This function will return the specified carriage in a train.
// PARAMETERS: pointer to train engine, number of carriage to return (0 = engine)
/////////////////////////////////////////////////////////////
CTrain* CTrain::FindCarriage(const CTrain* pEngine, u8 CarriageNumber)
{
	Assert(pEngine);
	Assert(pEngine->m_nTrainFlags.bEngine || pEngine->m_nTrainFlags.bAllowRemovalByPopulation);

	u8 CurrentCarriageNumber = 0;

	CTrain* pCarriage = const_cast<CTrain*>(pEngine);
	Assertf(pCarriage, "CTrain::FindCarriage - Couldn't get train engine");
	while(CurrentCarriageNumber < CarriageNumber)
	{
		pCarriage = pCarriage->GetLinkedToBackward();
		//		Assertf(pCarriage, "CTrain::FindCarriage - Trying to find a carriage beyond the end of the train");
		if(pCarriage)
		{
			CurrentCarriageNumber++;
		}
		else
		{
			return NULL;
		}
	}

	return (pCarriage);
}

// NAME : FindClosestNodeOnTrack
// PURPOSE : Find the closest node on the specified track to the input position
// if 'fIn_ThisHeading' is specified, then angle from previous node to candidate node must be within 90degrees of the given heading
// if 'fOut_Distance' is specified, then the actual distance to the target is returned by parameter in this variable
s32 CTrain::FindClosestNodeOnTrack(const Vector3 &pos, s32 trackIndex, const float * fIn_MustFaceThisHeading, float * fOut_Distance, float * fOut_Heading, Vector3 * vOut_Position)
{
    s32 closestNode = -1;
	Vector3 vNodePos;
	Vector3 vLastNodePos;
    if(Verifyf(trackIndex >= 0 && trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL, "Invalid track index specified!"))
    {
	    float closestDistSquared = 99999.9f;

	    for(unsigned node = 0; node < gTrainTracks[trackIndex].m_numNodes; node++)
	    {
			vNodePos = gTrainTracks[trackIndex].m_pNodes[node].GetCoors();
		    const float distSquared = (pos - vNodePos).Mag2();

		    if(distSquared < closestDistSquared)
		    {
				if(fIn_MustFaceThisHeading && node > 0)
				{
					const float fHdg = fwAngle::GetRadianAngleBetweenPoints(vNodePos.x, vNodePos.y, vLastNodePos.x, vLastNodePos.y);
					if(Abs(SubtractAngleShorter(fHdg, *fIn_MustFaceThisHeading)) < HALF_PI)
					{
						closestNode = node;
						closestDistSquared = distSquared;
					}
				}
				else
				{
					closestNode = node;
					closestDistSquared = distSquared;
				}
			}

			vLastNodePos = vNodePos;
	    }

		if(closestNode != -1)
		{
			if(fOut_Distance)
			{
				*fOut_Distance = rage::Sqrtf(closestDistSquared);
			}

			if(fOut_Heading)
			{
				if(closestNode > 0)
				{
					vLastNodePos = gTrainTracks[trackIndex].m_pNodes[closestNode-1].GetCoors();
					vNodePos = gTrainTracks[trackIndex].m_pNodes[closestNode].GetCoors();
					*fOut_Heading = fwAngle::GetRadianAngleBetweenPoints(vNodePos.x, vNodePos.y, vLastNodePos.x, vLastNodePos.y);
				}
				else if(closestNode+1 < gTrainTracks[trackIndex].m_numNodes)
				{
					vLastNodePos = gTrainTracks[trackIndex].m_pNodes[closestNode].GetCoors();
					vNodePos = gTrainTracks[trackIndex].m_pNodes[closestNode+1].GetCoors();
					*fOut_Heading = fwAngle::GetRadianAngleBetweenPoints(vNodePos.x, vNodePos.y, vLastNodePos.x, vLastNodePos.y);
				}
				else
				{
					// Not really a meaningful result but sufficient for my purposes
					*fOut_Heading = 0.0f;
				}
			}

			if(vOut_Position)
			{
				*vOut_Position = gTrainTracks[trackIndex].m_pNodes[closestNode].GetCoors();
			}
		}
    }

	return closestNode;
}

// NAME : FindClosestNodeOnTrack
// PURPOSE : Find the closest node on the specified track to the input position
// Searches forwards and back from the given node until we exceed the max distance
s32 CTrain::FindClosestNodeOnTrack(const Vector3 &pos, s32 trackIndex, s32 startNode, f32 maxDistance, u32& numNodesSearched)
{
	s32 closestNode = -1;
	Vector3 vNodePos;
	f32 prevDistSquared;
	f32 maxDistanceSquared = maxDistance * maxDistance;
	f32 closestDistSquared = FLT_MAX;
	numNodesSearched = 0;

	if(Verifyf(trackIndex >= 0 && trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL, "Invalid track index specified!"))
	{
		// Start node must be in range to produce valid results
		if((pos - gTrainTracks[trackIndex].m_pNodes[startNode].GetCoors()).Mag2() >= maxDistanceSquared)
		{
			return closestNode;
		}

		// Search forward from the start node
		s32 currentNode = startNode;
		do 
		{
			vNodePos = gTrainTracks[trackIndex].m_pNodes[currentNode].GetCoors();
			prevDistSquared = (pos - vNodePos).Mag2();

			if(prevDistSquared < closestDistSquared)
			{
				closestDistSquared = prevDistSquared;
				closestNode = currentNode;
			}

			currentNode++;
			numNodesSearched++;

			if(currentNode >= gTrainTracks[trackIndex].m_numNodes)
			{
				currentNode = 0;
			}
		} while (currentNode != startNode && prevDistSquared < maxDistanceSquared);

		// Search back from the start node
		s32 endNode = currentNode;
		currentNode = startNode;
		do 
		{
			vNodePos = gTrainTracks[trackIndex].m_pNodes[currentNode].GetCoors();
			prevDistSquared = (pos - vNodePos).Mag2();

			if(prevDistSquared < closestDistSquared)
			{
				closestDistSquared = prevDistSquared;
				closestNode = currentNode;
			}

			currentNode--;
			numNodesSearched++;

			if(currentNode < 0)
			{
				currentNode = gTrainTracks[trackIndex].m_numNodes - 1;
			}
		} while (currentNode != startNode && currentNode != endNode && prevDistSquared < maxDistanceSquared);
	}

	return closestNode;
}

/////////////////////////////////////////////////////////////
// FUNCTION:	FindClosestTrackNode
// PURPOSE:		Given a coordinate this function will return the closest node on the
//					main track.
// RETURNS:		The node index.
/////////////////////////////////////////////////////////////
s32 CTrain::FindClosestTrackNode(const Vector3& pos, s8& trackIndex_out)
{
	trackIndex_out = -1;
	s32 ClosestNode = -1;

	s32 	TestNode;
	float	Dist, ClosestDist = 99999.9f;

	for (s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
	{
		for (TestNode = 0; TestNode < gTrainTracks[trackIndex].m_numNodes; TestNode++)
		{
			Dist = (pos - gTrainTracks[trackIndex].m_pNodes[TestNode].GetCoors()).Mag();

			if(Dist < ClosestDist)
			{
				ClosestNode = TestNode;
				trackIndex_out = trackIndex;
				ClosestDist = Dist;
			}
		}
	}
	return (ClosestNode);
}


#define		TRAIN_NETWORK_REQUEST_TIMEOUT	(2000)

void CTrain::ProcessTrainNetworkPending(int trackindex)
{
	if (gTrainTracks[trackindex].m_bTrainProcessNetworkCreation)
	{
		if (gTrainTracks[trackindex].m_bTrainNetworkCreationApproval)
		{
			sm_genTrain_status = TGEN_TRAINPENDING; //APPROVED
			sm_genTrain_trackIndex = (s8) trackindex; //ensure this matches up otherwise might fall apart in TGEN_TRAINPENDING with incorrect sm_genTrain_trackIndex
		}
		else
		{
			sm_genTrain_status = TGEN_NOTRAINPENDING; //DENIED
			gTrainTracks[trackindex].m_bTrainActive = false; //ensure this is reset on denial as this is also called on the host
		}

		trainDebugf2("CTrain::ProcessTrainNetworkPending %s FOR trackindex[%d]",gTrainTracks[trackindex].m_bTrainNetworkCreationApproval ? "APPROVED" : "DENIED",trackindex);
		gTrainTracks[trackindex].m_bTrainNetworkCreationApproval = false;
		sm_networkTimeCreationRequest = NetworkInterface::GetNetworkTime() + TRAIN_NETWORK_REQUEST_TIMEOUT;
	}
	else if (sm_networkTimeCreationRequest && CNetwork::GetNetworkTime() > (sm_networkTimeCreationRequest + TRAIN_NETWORK_REQUEST_TIMEOUT))
	{
		sm_genTrain_status = TGEN_NOTRAINPENDING;
		gTrainTracks[trackindex].m_bTrainNetworkCreationApproval = false;
		gTrainTracks[trackindex].m_bTrainActive = false;
		sm_networkTimeCreationRequest = NetworkInterface::GetNetworkTime() + TRAIN_NETWORK_REQUEST_TIMEOUT;
		trainDebugf2("CTrain::ProcessTrainNetworkPending TIMEOUT FOR trackindex[%d]",trackindex);
	}
}

/////////////////////////////////////////////////////////////
// FUNCTION: DoTrainGenerationAndRemoval
// PURPOSE:  If the player is not too far away from a point on a track we try to generate a train.
//			 Trains that are too far away from the camera are removed.
/////////////////////////////////////////////////////////////

#define		TRAINPOP_ADD_CAMTOTRACKDIST 	(120.0f)
#define		TRAINPOP_ADD_CAMTOSTATIONDIST 	(200.0f)
#define		TRAINPOP_ADD_DIST				(300.0f)
#define		TRAINPOP_REMOVE_DIST			(350.0f)
#define		TRAINPOP_REMOVE_DIST_IN_VIEW	(450.0f)
#define		TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW (270.0f)
#define		TRAINPOP_MINIMUM_CREATION_DIST	(60.0f)

void CTrain::DoTrainGenerationAndRemoval()
{
	trainDebugf3("CTrain::DoTrainGenerationAndRemoval");

	static	s32	sm_genTrain_generationNode;
	static  bool 	sm_genTrain_direction;
	static  u8	sm_genTrain_trainConfigIndex;

	float	CamFromTrackToCreateTrain;
	float	distFlatSq = FLT_MAX;
	s32	SearchFreq;
	static	bool	bPlayerNearStation = false;

	bool bIsNetworkOpen = NetworkInterface::IsNetworkOpen();

	if(fwTimer::GetTimeInMilliseconds() / 3000 != fwTimer::GetPrevElapsedTimeInMilliseconds() / 3000)
	{
		float	Dist2;
		bPlayerNearStation = false;

		for (s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex++)
		{
			// skip tracks that aren't initialised
			if(!gTrainTracks[trackIndex].m_isEnabled)
				continue;
			// skip tracks which have been turned off by script
			if(gTrainTracks[trackIndex].IsSwitchedOff())
				continue;

			for (s32 S = 0; S < gTrainTracks[trackIndex].m_numStations; S++)
			{
				Dist2 = Vector2(gTrainTracks[trackIndex].m_aStationCoors[S].x - CGameWorld::FindLocalPlayerCoors().x, gTrainTracks[trackIndex].m_aStationCoors[S].y - CGameWorld::FindLocalPlayerCoors().y).Mag2();

				if(Dist2 < rage::square(60.0f)) bPlayerNearStation = true;
			}
		}
	}

	BANK_ONLY( if(ms_bForceSpawn) bPlayerNearStation = true; )

	if(bPlayerNearStation)
	{
		SearchFreq = 1;
		CamFromTrackToCreateTrain = TRAINPOP_ADD_CAMTOSTATIONDIST;
	}
	else
	{
		SearchFreq = 950;
		CamFromTrackToCreateTrain = TRAINPOP_ADD_CAMTOTRACKDIST;
	}

	// If there is a train pending (train and driver models are being streamed in). We want to do the update as soon as so that
	// the models don't have the opportunity to be removed before being used.
	if(sm_genTrain_status == TGEN_TRAINPENDING)
	{
		SearchFreq = 1;
	}

	// See if we can remove any non-mission trains.
	if(fwTimer::GetTimeInMilliseconds() / SearchFreq != fwTimer::GetPrevElapsedTimeInMilliseconds() / SearchFreq)
	{
		bool	bAllFarAway;
		CTrain	*pTrainPresent[MAX_TRAIN_TRACKS_PER_LEVEL];
		const u32 kPhantomTrainInterval = (u32) (ms_iTrainNetworkHearbeatInterval * 2.5);
		for (s32 t = 0; t < MAX_TRAIN_TRACKS_PER_LEVEL; t++)
		{
			//First look for remote trains that might not be present - clear out the host phantom train information
			if ( bIsNetworkOpen && NetworkInterface::IsHost() && gTrainTracks[t].m_bTrainActive && gTrainTracks[t].m_iLastTrainReportingTimeMS && ((gTrainTracks[t].m_iLastTrainReportingTimeMS + kPhantomTrainInterval) < fwTimer::GetTimeInMilliseconds()) )
			{
				trainDebugf2("CTrain::DoTrainGenerationAndRemoval -- phantom train found on track[%d] m_iLastTrainReportingTimeMS[%u] older than kPhantomTrainInterval[%u] --> SetActive(false)",t,gTrainTracks[t].m_iLastTrainReportingTimeMS,kPhantomTrainInterval);
				SetTrackActive(t,false);
			}

			pTrainPresent[t] = bIsNetworkOpen && gTrainTracks[t].m_bTrainActive ? (CTrain*) 0x1 : NULL; //fake it out for remote trains
		}
		CVehicle::Pool *VehPool = CVehicle::GetPool();
		CVehicle* pVeh;
		s32 i = (s32) VehPool->GetSize();
		while(i--)
		{
			pVeh = VehPool->GetSlot(i);
			if(pVeh && pVeh->InheritsFromTrain() && !pVeh->IsNetworkClone() && 
				((static_cast<CTrain*>(pVeh))->m_nTrainFlags.bEngine || (static_cast<CTrain*>(pVeh))->m_nTrainFlags.bAllowRemovalByPopulation || (NetworkInterface::IsGameInProgress() &&(static_cast<CTrain*>(pVeh))->GetEngine() == nullptr)) &&
				!(static_cast<CTrain*>(pVeh))->GetIsMissionTrain() &&
				(static_cast<CTrain*>(pVeh))->m_trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL)	// Exclude planes.
			{
				s8 trackindex = (static_cast<CTrain*>(pVeh))->m_trackIndex;
				Assert(/*(static_cast<CTrain*>(pVeh)->m_trackIndex >= 0 &&*/ (trackindex < MAX_TRAIN_TRACKS_PER_LEVEL));
				if(trackindex >= 0)
				{
					pTrainPresent[trackindex] = static_cast<CTrain*>(pVeh);
				}

				CTrain	*pCarriage = static_cast<CTrain*>(pVeh);
				bAllFarAway = true;
				while(pCarriage && bAllFarAway)
				{
					const Vector3 vCarriagePosition = VEC3V_TO_VECTOR3(pCarriage->GetTransform().GetPosition());
					float	Dist = Vector2(camInterface::GetPos().x - vCarriagePosition.x, camInterface::GetPos().y - vCarriagePosition.y).Mag();

					if(pCarriage == FindPlayerVehicle() || Dist < TRAINPOP_REMOVE_DIST || CPedPopulation::IsCandidateInViewFrustum(vCarriagePosition, 10.f, TRAINPOP_REMOVE_DIST_IN_VIEW))
					{
						bAllFarAway = false;
					}

					if (bIsNetworkOpen && bAllFarAway)
					{
						CNetObjTrain* pNetObjTrain = (CNetObjTrain*) pCarriage->GetNetworkObject();
						const netPlayer* pClosest = NULL;
						if (pCarriage->IsNetworkClone() || (pNetObjTrain && (pNetObjTrain->IsScriptObject() || NetworkUtils::IsCloseToAnyRemotePlayer(pNetObjTrain->GetPosition(),TRAINPOP_REMOVE_DIST,pClosest))))
						{
							bAllFarAway = false;
						}
					}

					pCarriage = pCarriage->GetLinkedToBackward();
				}

				if(bAllFarAway)
				{
					//Ensure that all the trains are removable before removing one - otherwise might remove engine and leave carriages orphaned
					bool bAllowRemoval = true;

					// Remove this train entirely
					CTrain	*pCarriageToRemove = static_cast<CTrain*>(pVeh);
					if (pCarriageToRemove && (!pCarriageToRemove->GetNetworkObject() || pCarriageToRemove->GetNetworkObject()->CanDelete()))
					{
						while(pCarriageToRemove && bAllowRemoval)
						{
							CTrain* pNextCarriage = pCarriageToRemove->GetLinkedToBackward();

							bAllowRemoval = (!pCarriageToRemove->GetNetworkObject() || pCarriageToRemove->GetNetworkObject()->CanDelete());

							pCarriageToRemove = pNextCarriage;
						}
					}

					trainDebugf2("CTrain::DoTrainGenerationAndRemoval -- bAllFarAway ... bAllowRemoval[%d]",bAllowRemoval);

					if (bAllowRemoval)
					{
						pCarriageToRemove = static_cast<CTrain*>(pVeh);
						if (pCarriageToRemove && (!pCarriageToRemove->GetNetworkObject() || pCarriageToRemove->GetNetworkObject()->CanDelete()))
						{
							while(pCarriageToRemove)
							{
								CTrain* pNextCarriage = pCarriageToRemove->GetLinkedToBackward();
#if __BANK
								trainDebugf2("CTrain::DoTrainGenerationAndRemoval pCarriageToRemove[%p][%s].IsNetworkClone[%d] engine[%p] --> Destroy",pCarriageToRemove,pCarriageToRemove && pCarriageToRemove->GetNetworkObject() ? pCarriageToRemove->GetNetworkObject()->GetLogName() : "", pCarriageToRemove ? pCarriageToRemove->IsNetworkClone() : false, pCarriageToRemove ? pCarriageToRemove->GetEngine() : 0);
#endif

								//Evict all the peds if this is a network train carriage and we are about to destroy it
								if (NetworkInterface::IsGameInProgress() && pCarriageToRemove->GetNetworkObject())
								{
									CNetObjTrain* pNetObjTrain = static_cast<CNetObjTrain*>(pCarriageToRemove->GetNetworkObject());
									if (pNetObjTrain)
									{
										pNetObjTrain->EvictAllPeds();
									}
								}

								CVehicleFactory::GetFactory()->Destroy(pCarriageToRemove);
								pCarriageToRemove = pNextCarriage;
							}

							if (trackindex >= 0)
							{
								if (NetworkInterface::IsGameInProgress())
								{
									SetTrackActive(trackindex,false);
									CNetworkTrainReportEvent::Trigger(trackindex, false);
								}	
							}
						}
					}
				}
			}
		}

		//Only allow train creation in MP if they are enabled.
		//Only allow train creation in MP if the game is in a stable gameinprogress state
		if(bIsNetworkOpen)
		{
			if (!ms_bEnableTrainsInMP || !NetworkInterface::IsGameInProgress())
			{
				trainDebugf3("CTrain::DoTrainGenerationAndRemoval--(!ms_bEnableTrainsInMP || !NetworkInterface::IsGameInProgress())-->return");
				return;
			}
		}

		if(sm_bDisableRandomTrains) return;

		if(CVehiclePopulation::GetScriptDisableRandomTrains())
		{
			trainDebugf3("CTrain::DoTrainGenerationAndRemoval--GetScriptDisableRandomTrains-->return");
			return;
		}

		switch(sm_genTrain_status)
		{
			case TGEN_NOTRAINPENDING:
			{
				// Only start thinking about creating a train if the pool has enough room for it.
				if(CVehicle::GetPool()->GetNoOfFreeSpaces() <= 10 || gDiskTrainTracksCount+gDLCTrainTracksCount <= 0)
				{
					trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(CVehicle::GetPool()->GetNoOfFreeSpaces() <= 10 || gDiskTrainTracksCount+gDLCTrainTracksCount <= 0)--break");
					break;
				}

				Vector3 vCreationPos;

				// Look for existing trains on the tracks as we can only have one
				// train per track. (No trains on mission specific tracks)
				// sm_genTrain_trackIndex = static_cast<s8>(fwRandom::GetRandomNumberInRange(0, gTrainTracksCount));

				for(sm_genTrain_trackIndex=0; sm_genTrain_trackIndex<MAX_TRAIN_TRACKS_PER_LEVEL; sm_genTrain_trackIndex++)
				{
					// skip tracks that are not initialised
					if(!gTrainTracks[sm_genTrain_trackIndex].m_isEnabled)
					{
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(!gTrainTracks[%d].m_isEnabled)--skip tracks that are not initialized--continue",sm_genTrain_trackIndex);
						continue;
					}

					// skip tracks which have been turned off by script
					if(gTrainTracks[sm_genTrain_trackIndex].IsSwitchedOff())
					{
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(gTrainTracks[%d].IsSwitchedOff())--skip tracks turned off by script--continue",sm_genTrain_trackIndex);
						continue;
					}

					static dev_s32 sMinTimeBetweenTrains = 120000;

					s32 iTimeSinceLastSpawn = fwTimer::GetTimeInMilliseconds() - gTrainTracks[sm_genTrain_trackIndex].m_iLastTrainSpawnTimeMS;

					BANK_ONLY( if(ms_bForceSpawn) iTimeSinceLastSpawn = 99999999; )

					// url:bugstar:1227004
					// Have a minimum frequency for trains on any one track; two in less than a minute is too frequent
					if(iTimeSinceLastSpawn < sMinTimeBetweenTrains && gTrainTracks[sm_genTrain_trackIndex].m_iScriptTrainSpawnFreqMS == -1)
					{
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(iTimeSinceLastSpawn[%d] < sMinTimeBetweenTrains[%d] && gTrainTracks[%d].m_iScriptTrainSpawnFreqMS == -1)--continue",iTimeSinceLastSpawn,sMinTimeBetweenTrains,sm_genTrain_trackIndex);
						continue;
					}

					// skip tracks which have a script-overridden train spawn frequency, which has not yet elapsed
					if(gTrainTracks[sm_genTrain_trackIndex].m_iScriptTrainSpawnFreqMS != -1 &&
						 iTimeSinceLastSpawn < gTrainTracks[sm_genTrain_trackIndex].m_iScriptTrainSpawnFreqMS)
					{
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(gTrainTracks[%d].m_iScriptTrainSpawnFreqMS != -1 && iTimeSinceLastSpawn[%d] < gTrainTracks[%d].m_iScriptTrainSpawnFreqMS[%d])--skip tracks which have a script-overridden train spawn frequency, which has not yet elapsed--continue",sm_genTrain_trackIndex,iTimeSinceLastSpawn,sm_genTrain_trackIndex,gTrainTracks[sm_genTrain_trackIndex].m_iScriptTrainSpawnFreqMS);
						continue;
					}

					if(pTrainPresent[sm_genTrain_trackIndex])
					{
						// url:bugstar:1227004
						// count the last spawn time, as the last time a train existed on this track
						// this way the spawn frequency makes more sense
						gTrainTracks[sm_genTrain_trackIndex].m_iLastTrainSpawnTimeMS = fwTimer::GetTimeInMilliseconds();

						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(pTrainPresent[%d])--continue",sm_genTrain_trackIndex);

						continue;
					}

					// Try and find a point on the track that is close to the player.
					Vector3	trainGenCentre = camInterface::GetPos();

					// This has got to be some of the shittest code in the entire game
					static const int iMaxNumAttempts = 30;
					int iNumAttempts = iMaxNumAttempts;

					BANK_ONLY( if(ms_bForceSpawn) iNumAttempts = gTrainTracks[sm_genTrain_trackIndex].m_numNodes; )

					while(iNumAttempts-- > 0)
					{
						sm_genTrain_generationNode = fwRandom::GetRandomNumberInRange(0, gTrainTracks[sm_genTrain_trackIndex].m_numNodes);
						const Vector3 trackCoors = gTrainTracks[sm_genTrain_trackIndex].m_pNodes[sm_genTrain_generationNode].GetCoors();
						distFlatSq = Vector2(trackCoors.x - trainGenCentre.x, trackCoors.y - trainGenCentre.y).Mag2();
						if(distFlatSq < rage::square(CamFromTrackToCreateTrain))												
							break;
					}
					if(distFlatSq >= rage::square(CamFromTrackToCreateTrain))
						continue;

					// In IV trains always travel in the direction of increasing node index. fwRandom::GetRandomNumber() & 1;
					sm_genTrain_direction = 1;

					// If this is a linked track, then see if we have an existing train on the link-to track.
					// If so then we will ensure that the new train is moving in the opposite direction.
					if(!gTrainTracks[sm_genTrain_trackIndex].m_isLooped)
					{
						if(gTrainTracks[sm_genTrain_trackIndex].m_iLinkedTrackIndex != -1)
						{
							CTrain * pTrainOnLinkedTrack = pTrainPresent[gTrainTracks[sm_genTrain_trackIndex].m_iLinkedTrackIndex];
							// Use the SAME direction, because the linked tracks are already running in opposite directions! :-)
							if(pTrainOnLinkedTrack)
							{
								// Don't create train on adjacent track if the linked train is at the station
								if(pTrainOnLinkedTrack->m_nTrainState != TS_Moving)
								{
									trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(pTrainOnLinkedTrack->m_nTrainState != TS_Moving)--continue");
									continue;
								}

								sm_genTrain_direction = pTrainOnLinkedTrack->m_nTrainFlags.bDirectionTrackForward;
							}
						}
						// Ping-pong (ie. non-looped) tracks which are not linked to any other adjacent track
						// will randomly generate trains in both directions.
						else
						{
							sm_genTrain_direction = fwRandom::GetRandomTrueFalse();
						}
					}

					// Now try to find the node on the same track a certain distance away.
					// The nodesExamined thing is needed as the whole track can be close to the camera
					// which would result in the while never being finished.
					//s32 previousNode = 0;
					s32 furthestNode = 0;
					float furthestDist = 0.0f;
					s32 nodesExamined = 0;

					const float addDistSq = rage::square(TRAINPOP_ADD_DIST);
					const float removalDistSq  = rage::square(TRAINPOP_REMOVE_DIST);

					while((distFlatSq < addDistSq || distFlatSq > removalDistSq ) &&
						(nodesExamined < gTrainTracks[sm_genTrain_trackIndex].m_numNodes))
					{
						//previousNode = sm_genTrain_generationNode;
						if(sm_genTrain_direction)
						{
							sm_genTrain_generationNode--;
							if(sm_genTrain_generationNode < 0)
							{
								sm_genTrain_generationNode += gTrainTracks[sm_genTrain_trackIndex].m_numNodes;
							}
						}
						else
						{
							sm_genTrain_generationNode++;
							if(sm_genTrain_generationNode >= gTrainTracks[sm_genTrain_trackIndex].m_numNodes)
							{
								sm_genTrain_generationNode -= gTrainTracks[sm_genTrain_trackIndex].m_numNodes;
							}
						}
						distFlatSq = (gTrainTracks[sm_genTrain_trackIndex].m_pNodes[sm_genTrain_generationNode].GetCoorsXY() - Vector2(trainGenCentre.x, trainGenCentre.y)).Mag2();
						nodesExamined++;
						if(distFlatSq > furthestDist)
						{
							furthestDist = distFlatSq;
							furthestNode = sm_genTrain_generationNode;
						}
					}

					// If we didn't find a suitable position on this track, then bail
					if(distFlatSq < addDistSq || distFlatSq > removalDistSq)
					{
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--(distFlatSq < addDistSq || distFlatSq > removalDistSq)--If we didn't find a suitable position on this track, then bail--continue");
						continue;
					}

					// If the complete track is close to the camera we pick the furthest point
					if(nodesExamined == gTrainTracks[sm_genTrain_trackIndex].m_numNodes)
					{
						sm_genTrain_generationNode = furthestNode;
					}

					if(!gTrainTracks[sm_genTrain_trackIndex].m_isLooped)
					{
						// The carriages on pingpong tracks should not be created between node 0 and the last one.
						// The carriages pingpongs between 2 stations.
						sm_genTrain_generationNode = MAX(sm_genTrain_generationNode, 2);
						sm_genTrain_generationNode = MIN(sm_genTrain_generationNode, gTrainTracks[sm_genTrain_trackIndex].m_numNodes-3);
					}

					sm_genTrain_trainConfigIndex =
						gTrainConfigs.GetRandomTrainConfigIndexFromTrainConfigGroup(gTrainTracks[sm_genTrain_trackIndex].m_trainConfigGroupNameHash);

					vCreationPos = gTrainTracks[sm_genTrain_trackIndex].m_pNodes[sm_genTrain_generationNode].GetCoors();
					distFlatSq = (vCreationPos - trainGenCentre).XYMag2();

					// Make sure the train wouldn't get removed immediately though.
					//if(distFlat > TRAINPOP_REMOVE_DIST)
					//{
					//	sm_genTrain_generationNode = previousNode;
					//}

					// Check the position
					if(CPedPopulation::IsCandidateInViewFrustum(vCreationPos, 10.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW) && distFlatSq > rage::square(TRAINPOP_MINIMUM_CREATION_DIST))
					{
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--IsCandidateInViewFrustum--continue");
						continue;
					}

					if(bIsNetworkOpen && NetworkUtils::IsVisibleOrCloseToAnyPlayer(vCreationPos, 10.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW, TRAINPOP_MINIMUM_CREATION_DIST))
					{
						// The position we found is still too close to the camera.
						trainDebugf3("CTrain::DoTrainGenerationAndRemoval--TGEN_NOTRAINPENDING--IsVisibleOrCloseToAnyPlayer--continue");
						continue;
					}

					sm_genTrain_status = TGEN_TRAINPENDING;

					if (NetworkInterface::IsGameInProgress())
					{
						unsigned curTime = CNetwork::GetNetworkTime();
						if (!sm_networkTimeCreationRequest || (curTime > sm_networkTimeCreationRequest))
						{
							sm_networkTimeCreationRequest = CNetwork::GetNetworkTime() + TRAIN_NETWORK_REQUEST_TIMEOUT;
							trainDebugf2("CTrain::DoTrainGenerationAndRemoval -- INITATE PENDING ON trackindex[%d] sm_networkTimeCreationRequest[%u]",sm_genTrain_trackIndex,sm_networkTimeCreationRequest);
							CNetworkTrainRequestEvent::Trigger(sm_genTrain_trackIndex);
						}
						
						if (!NetworkInterface::IsHost())
							sm_genTrain_status = TGEN_TRAINNETWORKPENDING;
					}

					BANK_ONLY( ms_bForceSpawn = false; )

					break;
				}

#if __DEV
				static bool bPrintTrainCreation = false;
				if(sm_genTrain_status == TGEN_TRAINPENDING && bPrintTrainCreation)
				{
					Printf("TRAINS : want to create train at (%.1f, %.1f, %.1f), dist = %.1f\n", vCreationPos.x, vCreationPos.y, vCreationPos.z, SqrtfSafe(distFlatSq));
				}
#endif
			}
			break;

			case TGEN_TRAINPENDING:
			{
				// skip tracks that aren't initialised
				if(!gTrainTracks[sm_genTrain_trackIndex].m_isEnabled)
				{
					break;
				}
				// skip tracks which have been turned off by script
				if(gTrainTracks[sm_genTrain_trackIndex].IsSwitchedOff())
				{
					// Mark all the models to be deletable. Don't want the trains in mem all the time.
					const int carriageCount = gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages.GetCount();
					for(int i = 0; i < carriageCount; ++i)
					{
						strLocalIndex modelIndex = gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages[i].m_modelId;
						fwModelId modelId(modelIndex);
						CModelInfo::SetAssetsAreDeletable(modelId);
					}

					sm_genTrain_status = TGEN_NOTRAINPENDING;

					break;
				}

				// If all the required carriages are in memory we generate the train.
				bool	bAllInMem = true;
				Assert(sm_genTrain_trainConfigIndex < gTrainConfigs.m_trainConfigs.GetCount());
				const int carriageCount = gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages.GetCount();
				trainDebugf2("TGEN_TRAINPENDING sm_genTrain_trackIndex[%d] carriageCount[%d]",sm_genTrain_trackIndex,carriageCount);
				for(int i = 0; i < carriageCount; ++i)
				{
					strLocalIndex modelIndex = gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages[i].m_modelId;
					fwModelId modelId(modelIndex);
					if(!CModelInfo::HaveAssetsLoaded(modelId))
					{
						CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
						bAllInMem = false;
					}
				}

				trainDebugf2("TGEN_TRAINPENDING train in memory -- bAllInMem[%d]",bAllInMem);

				// Request the driver model, if there is a specific one for the engine
				fwModelId driverModelId;
				driverModelId.Invalidate();

				if(carriageCount > 0)
				{
					fwModelId trainId((gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages[0].m_modelId));

					// let pop streaming request the train driver
					gPopStreaming.RequestVehicleDriver(trainId.GetModelIndex());

					driverModelId = CPedPopulation::FindSpecificDriverModelForCarToUse(trainId);
					if(driverModelId.IsValid())
					{
						if(!CModelInfo::HaveAssetsLoaded(driverModelId))
						{							
							bAllInMem = false;
						}
					}
					else
					{
						trainWarningf("TGEN_TRAINPENDING train driverModelId Invalid - this will preclude the train from starting");
					}
				}

				// If all the required carriages are in memory we generate the train.
				trainDebugf2("TGEN_TRAINPENDING train and driver in memory -- bAllInMem[%d]",bAllInMem);
				if(bAllInMem)
				{
					CTrain* pEngine = NULL;
					trainDebugf2("TGEN_TRAINPENDING-bAllInMem sm_genTrain_trackIndex[%d] sm_genTrain_generationNode[%d] gTrainTracksCount[%d]",sm_genTrain_trackIndex,sm_genTrain_generationNode,gDiskTrainTracksCount+gDLCTrainTracksCount);

					Vector3	vCamCenter = camInterface::GetPos();
					Vector3 vTrainGenCoords = gTrainTracks[sm_genTrain_trackIndex].m_pNodes[sm_genTrain_generationNode].GetCoors();

					const float fDist = (vCamCenter - vTrainGenCoords).XYMag();

					trainDebugf2("TGEN_TRAINPENDING-check fDist[%f] TRAINPOP_MINIMUM_CREATION_DIST[%f]",fDist,TRAINPOP_MINIMUM_CREATION_DIST);

					Assert(sm_genTrain_trainConfigIndex < gTrainConfigs.m_trainConfigs.GetCount());
					const int carriageCount = gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages.GetCount();

					bool bEnoughFreeVehicleSpacesForTrain = (CVehicle::GetPool()->GetNoOfFreeSpaces() >= carriageCount);
					bool bHasFreeObjectIdsAvailable = NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetObjectManager().GetObjectIDManager().HasFreeObjectIdsAvailable(carriageCount, false) : true; //default to true for SP
					bool bCanRegisterLocalObjectsOfTypeTrainCount = NetworkInterface::IsNetworkOpen() ? CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_TRAIN, carriageCount, false) : true;

					// Test dist: Streaming might have taken so long we are now very close to the train. Abort if so.
					// Check the position
					if(bEnoughFreeVehicleSpacesForTrain && bHasFreeObjectIdsAvailable && bCanRegisterLocalObjectsOfTypeTrainCount && 
						!CPedPopulation::IsCandidateInViewFrustum(vTrainGenCoords, 5.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW) && fDist > TRAINPOP_MINIMUM_CREATION_DIST
						&& !CheckTrainSpawnTrackInView(sm_genTrain_generationNode, sm_genTrain_direction, sm_genTrain_trainConfigIndex, vCamCenter))
					{
						if(!NetworkInterface::IsNetworkOpen() || !NetworkUtils::IsVisibleOrCloseToAnyPlayer(vTrainGenCoords, 5.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW, TRAINPOP_MINIMUM_CREATION_DIST))
						{							
							trainDebugf2("TGEN_TRAINPENDING-CreateTrain carriageCount[%d]",carriageCount);
							CreateTrain(sm_genTrain_trackIndex, sm_genTrain_generationNode, sm_genTrain_direction, sm_genTrain_trainConfigIndex, &pEngine, NULL, THREAD_INVALID);
							Assertf(pEngine,"CTrain::DoTrainGenerationAndRemoval !pEngine");

							if (pEngine)
							{
								gTrainTracks[sm_genTrain_trackIndex].m_iLastTrainSpawnTimeMS = fwTimer::GetTimeInMilliseconds();
							}							
						}
						else
						{
							trainDebugf2("TGEN_TRAINPENDING- coors <%.0f, %.0f, %.0f> too close or visible to network player - not creating",vTrainGenCoords.x,vTrainGenCoords.y,vTrainGenCoords.z);
						}
					}
					else
					{
						trainDebugf2("TGEN_TRAINPENDING- coors <%.0f, %.0f, %.0f> too close or visible to player - not creating",vTrainGenCoords.x,vTrainGenCoords.y,vTrainGenCoords.z);
					}

					trainDebugf2("TGEN_TRAINPENDING-markdeletable sm_genTrain_trainConfigIndex[%d]",sm_genTrain_trainConfigIndex);

					// Mark all the models to be deletable. Don't want the trains in mem all the time.
					for(int i = 0; i < carriageCount; ++i)
					{
						u32 modelIndex = gTrainConfigs.m_trainConfigs[sm_genTrain_trainConfigIndex].m_carriages[i].m_modelId.Get();
						fwModelId modelId((strLocalIndex(modelIndex)));
						Assert(CModelInfo::HaveAssetsLoaded(modelId));
						CModelInfo::SetAssetsAreDeletable(modelId);
					}

					trainDebugf2("TGEN_TRAINPENDING-check driverModelId.IsValid()");					

					if (NetworkInterface::IsGameInProgress())
					{
						//Report that the engine was created, or if it wasn't ensure that we report that it wasn't created and the track is cleared for another engine creation
						SetTrackActive(sm_genTrain_trackIndex,pEngine ? true : false);
						trainDebugf2("CTrain::DoTrainGenerationAndRemoval -- local train %s on track[%d] set m_bTrainActive[%d]",gTrainTracks[sm_genTrain_trackIndex].m_bTrainActive ? "created" : "aborted",sm_genTrain_trackIndex,gTrainTracks[sm_genTrain_trackIndex].m_bTrainActive);
						CNetworkTrainReportEvent::Trigger(sm_genTrain_trackIndex, gTrainTracks[sm_genTrain_trackIndex].m_bTrainActive);
					}

					sm_genTrain_status = TGEN_NOTRAINPENDING;
					trainDebugf2("TGEN_TRAINPENDING set TGEN_NOTRAINPENDING");
				}

				trainDebugf2("TGEN_TRAINPENDING-complete");
			}
			break;

			case TGEN_TRAINNETWORKPENDING:
			{
				ProcessTrainNetworkPending(sm_genTrain_trackIndex);
			}
			break;
		}
	}
}

/////////////////////////////////////////////////////////////
// FUNCTION: CheckTrainSpawnTrackInView
// PURPOSE:  Check to see if the player can see any of the track where the given train is going to spawn
// RETURNS:  True if the train will be visible to the player within TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW
/////////////////////////////////////////////////////////////
bool CTrain::CheckTrainSpawnTrackInView(s32 generationNode, bool bDirection, u8 trainConfigIndex, const Vector3& vCamCenter)
{
	bool inView = false;
	s32 carriageCount = gTrainConfigs.m_trainConfigs[trainConfigIndex].m_carriages.GetCount();
	// we need to check the position of the end of the train, as the trains are long and even if the engine is behind the player it might still be visible
	// first get the length of the train
	float totalTrainLength = 0.f;
	for(int i = 0; i < carriageCount; i++)
	{
		fwModelId trainId(gTrainConfigs.m_trainConfigs[trainConfigIndex].m_carriages[i].m_modelId);

		CVehicleModelInfo* trainMI = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(trainId);
		if(trainMI)
		{
			float carriageLength = trainMI->GetBoundingBoxMax().y - trainMI->GetBoundingBoxMin().y;

			totalTrainLength += carriageLength;
		}
	}

	// get the start and end positions of the train in track-space
	float startTrackPos = gTrainTracks[sm_genTrain_trackIndex].m_pNodes[generationNode].GetLengthFromStart();	

	float currentTrackPosition = startTrackPos;
	s32	currentNode = FindCurrentNode(sm_genTrain_trackIndex, currentTrackPosition, bDirection);
	
	float endTrackPosition = startTrackPos;
	if(bDirection)
	{
		endTrackPosition -= totalTrainLength; // we're going from start to end, against the direction the train is traveling
	}
	else
	{
		endTrackPosition += totalTrainLength;
	}

#if __ASSERT
	const float totalTrackLength = gTrainTracks[sm_genTrain_trackIndex].GetTotalTrackLength();
	if(IsLoopedTrack(sm_genTrain_trackIndex))
	{
		Assertf(endTrackPosition >= (-2.0f * totalTrackLength),
			"CTrain::CheckTrainSpawnTrackInView end pos %3.2f is out of range, track index %d, track length %3.2f, start pos %3.2f, start node %d, train length %3.2f, direction %d",
			endTrackPosition, sm_genTrain_trackIndex, totalTrackLength, startTrackPos, generationNode, totalTrainLength, (s32) bDirection);
		Assertf(endTrackPosition <= (2.0f * totalTrackLength), 
			"CTrain::CheckTrainSpawnTrackInView end pos %3.2f is out of range, track index %d, track length %3.2f, start pos %3.2f, start node %d, train length %3.2f, direction %d",
			endTrackPosition, sm_genTrain_trackIndex, totalTrackLength, startTrackPos, generationNode, totalTrainLength, (s32) bDirection);
	}

#endif
	FixupTrackPos(sm_genTrain_trackIndex, endTrackPosition);
	s32 endNode = FindCurrentNode(sm_genTrain_trackIndex, endTrackPosition, bDirection);

	// check start node													
	Vector3 carriageWorldPos =	VEC3V_TO_VECTOR3(gTrainTracks[sm_genTrain_trackIndex].m_pNodes[currentNode].GetCoorsV());
	//grcDebugDraw::Sphere(carriageWorldPos, 5.f, Color_blue, false, 1000);
	float fDist2 = (carriageWorldPos - vCamCenter).XYMag2();
	if(CPedPopulation::IsCandidateInViewFrustum(carriageWorldPos, 5.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW) || fDist2 < rage::square(TRAINPOP_MINIMUM_CREATION_DIST))
	{
		// too close and/or visible to the player
		inView = true;
		return inView;
	}

	// check end node															
	carriageWorldPos =	VEC3V_TO_VECTOR3(gTrainTracks[sm_genTrain_trackIndex].m_pNodes[endNode].GetCoorsV());
	//grcDebugDraw::Sphere(carriageWorldPos, 5.f, Color_purple, false, 1000);
	fDist2 = (carriageWorldPos - vCamCenter).XYMag2();
	if(CPedPopulation::IsCandidateInViewFrustum(carriageWorldPos, 5.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW) || fDist2 < rage::square(TRAINPOP_MINIMUM_CREATION_DIST))
	{
		// too close and/or visible to the player
		inView = true;
		return inView;
	}

	// check node visibility for interior nodes
#define CHECK_TRAIN_SPAWN_TRACK_NODE_JUMP (3)
	currentNode += (bDirection?-1:1)*CHECK_TRAIN_SPAWN_TRACK_NODE_JUMP; // advance towards the end of the train
	FixupTrackNode(sm_genTrain_trackIndex, currentNode);
	ASSERT_ONLY(u32 counter = 0;)

	while(ABS(currentNode - endNode) > CHECK_TRAIN_SPAWN_TRACK_NODE_JUMP) // while we're not close to the end
	{
#if __ASSERT
		counter++;
		Assert(counter < 100); // if we loop too many times, we're going the wrong way around the track!
#endif

		Vector3 carriageWorldPos =	VEC3V_TO_VECTOR3(gTrainTracks[sm_genTrain_trackIndex].m_pNodes[currentNode].GetCoorsV());
		//grcDebugDraw::Sphere(carriageWorldPos, 5.f, Color_red, false, 1000);
		fDist2 = (carriageWorldPos - vCamCenter).XYMag2();
		if(CPedPopulation::IsCandidateInViewFrustum(carriageWorldPos, 5.f, TRAINPOP_MINIMUM_CREATION_DIST_IN_VIEW) || fDist2 < rage::square(TRAINPOP_MINIMUM_CREATION_DIST))
		{
			// too close and/or visible to the player
			inView = true;
			break;
		}
		
		// advance CHECK_TRAIN_SPAWN_TRACK_NODE_JUMP more nodes
		currentNode += (bDirection?-1:1)*CHECK_TRAIN_SPAWN_TRACK_NODE_JUMP;
		FixupTrackNode(sm_genTrain_trackIndex, currentNode);

	}	
	return inView;
}

/////////////////////////////////////////////////////////////
// FUNCTION: SetTrackPosFromWorldPos
// PURPOSE:  Given the new found coordinates of the train we work out what the
//			 position of this train is on the track. This will be used to set the
//			 m_trackPosFront after updating playback data.
/////////////////////////////////////////////////////////////
void CTrain::SetTrackPosFromWorldPos(s32 currentNode, s32 nextNode)
{
	s32 bestTrackNodeIndex = -1;
	s32 bestOtherNodeIndex = -1;
	float DistAlongSegment = 0;
	Vector3	bestNodeCoors;
	Vector3 bestOtherNodeCoors;

	if(currentNode == -1 || nextNode == -1)
	{
		// need to find our nodes

		Vector3 TrainCoors = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		TrainCoors.z = 0.0f;

		// Go for every node on the track.
		const int numNodes = (m_trackIndex>=0)?gTrainTracks[m_trackIndex].m_numNodes:0;
	
		// find closest track node to the point
		float minTrackCoorsDistance = (float)1.0e20; // max float value		
		for (s32 iTrackNode = 0; iTrackNode < numNodes; iTrackNode++)
		{
			Vector3	trackNodeCoors(gTrainTracks[m_trackIndex].m_pNodes[iTrackNode].GetCoors());
			trackNodeCoors.z = 0.0f;

			float dist2 = (trackNodeCoors - TrainCoors).Mag2();
			if(dist2 < minTrackCoorsDistance)
			{
				minTrackCoorsDistance = dist2;
				bestTrackNodeIndex = iTrackNode;
			}
		}
	
		if(bestTrackNodeIndex != -1) // found a valid closest node
		{
			// test the two adjacent nodes to find the segment that the train is on
			s32 otherTrackNode1 = (bestTrackNodeIndex + 1) % gTrainTracks[m_trackIndex].m_numNodes;
			s32 otherTrackNode2 = bestTrackNodeIndex > 0 ? (bestTrackNodeIndex - 1) % gTrainTracks[m_trackIndex].m_numNodes : gTrainTracks[m_trackIndex].m_numNodes - 1;

			// Calculate the distance of this point to the line between these two nodes.
			bestNodeCoors = gTrainTracks[m_trackIndex].m_pNodes[bestTrackNodeIndex].GetCoors();
			bestNodeCoors.z = 0.0f;
			Vector3	otherNode1Coors(gTrainTracks[m_trackIndex].m_pNodes[otherTrackNode1].GetCoors());
			otherNode1Coors.z = 0.0f;
			Vector3	otherNode2Coors(gTrainTracks[m_trackIndex].m_pNodes[otherTrackNode2].GetCoors());
			otherNode2Coors.z = 0.0f;

			// Find how far along this line segment our point would be			

			// Other node 1
			float Length2 = (bestNodeCoors - otherNode1Coors).Mag2();
			DistAlongSegment = DotProduct(TrainCoors - bestNodeCoors, otherNode1Coors - bestNodeCoors);
			DistAlongSegment = Length2 > SMALL_FLOAT ? DistAlongSegment / (Length2) : 0.0f;
			Assertf(Length2 > SMALL_FLOAT, "Duplicate train track nodes at %.2f, %.2f, %.2f, please bug this for art"
				, bestNodeCoors.x, bestNodeCoors.y, bestNodeCoors.z);		
			if(DistAlongSegment > 0.001f && DistAlongSegment < 1.001f) 
			{
				// use other node 1
				bestOtherNodeCoors = otherNode1Coors;
				bestOtherNodeIndex = otherTrackNode1;
			}
			else // this node pair is bad, try the other one
			{
				// since node 2 comes before our closest node in the list, we need to swap them so that the math works out
				Vector3 vTemp = bestNodeCoors;
				bestNodeCoors = otherNode2Coors;
				otherNode2Coors = vTemp;

				s32 iTemp = bestTrackNodeIndex;
				bestTrackNodeIndex = otherTrackNode2;
				otherTrackNode2 = iTemp;

				// Other node 2
				Length2 = (bestNodeCoors - otherNode2Coors).Mag2();
				DistAlongSegment = DotProduct(TrainCoors - bestNodeCoors, otherNode2Coors - bestNodeCoors);
				DistAlongSegment = Length2 > SMALL_FLOAT ? DistAlongSegment / (Length2) : 0.0f;
				Assertf(Length2 > SMALL_FLOAT, "Duplicate train track nodes at %.2f, %.2f, %.2f, please bug this for art"
					, bestNodeCoors.x, bestNodeCoors.y, bestNodeCoors.z);
				if(DistAlongSegment > 0.001f && DistAlongSegment < 1.001f)  
				{
					// use other node 2
					bestOtherNodeCoors = otherNode2Coors;
					bestOtherNodeIndex = otherTrackNode2;
				}
				else
				{
					// we've failed
					return;
				}
			}
		
			// Find out how far from the line this point is.
			Vector3 PointOnLine = bestNodeCoors + (bestOtherNodeCoors - bestNodeCoors) * DistAlongSegment;
			if((PointOnLine - TrainCoors).Mag2() >= (3.0f*3.0f))
			{
				return; // another failure case
			}			
		}
	}
	else
	{		
		// use the provided nodes
		bestTrackNodeIndex = currentNode;
		bestOtherNodeIndex = nextNode;
	}

	if(bestTrackNodeIndex != -1 && bestOtherNodeIndex !=-1)
	{		
		// Get the track pos of the point.
		const float trackNodeTrackPos = gTrainTracks[m_trackIndex].m_pNodes[bestTrackNodeIndex].GetLengthFromStart();
		float otherNodeTrackPos = gTrainTracks[m_trackIndex].m_pNodes[bestOtherNodeIndex].GetLengthFromStart();
		if (bestOtherNodeIndex < bestTrackNodeIndex)
		{
			// Add the total length of track if looping.
			otherNodeTrackPos += gTrainTracks[m_trackIndex].GetTotalTrackLength();
		}
		const float trackPosCentre = trackNodeTrackPos + ((otherNodeTrackPos - trackNodeTrackPos) * DistAlongSegment);

		// carriageTrackDistFrontToNextCarriageFront by half the length of the carriage.
		const float carriageLength = GetBoundingBoxMax().y - GetBoundingBoxMin().y;
		if(m_nTrainFlags.bDirectionTrackForward)
		{
			float trackPosFront = trackPosCentre + (carriageLength * 0.5f);
			FixupTrackPos(m_trackIndex, trackPosFront);
			m_trackPosFront = trackPosFront;
		}
		else
		{
			float trackPosFront = trackPosCentre - (carriageLength * 0.5f);
			FixupTrackPos(m_trackIndex, trackPosFront);
			m_trackPosFront = trackPosFront;
		}
		m_trackForwardSpeed = GetVelocity().Mag();
		bool bDir = (DotProduct(GetVelocity(), bestOtherNodeCoors - bestNodeCoors) > 0.0f);
		if(!(m_nTrainFlags.bDirectionTrackForward ^ bDir))
		{
			m_trackForwardSpeed = -m_trackForwardSpeed;
		}
		m_trackForwardAccPortion = 0.0f;// Used for swinging the carriage forward and back later.
		m_frontBackSwingAngPrev = 0.0f;

		return;	
	}
}


/////////////////////////////////////////////////////////////
// FUNCTION: FindNearestTrainCarriage
// PURPOSE:  Gives us a pointer to the nearest train.
//			 Used by railway crossings.
/////////////////////////////////////////////////////////////
CTrain* CTrain::FindNearestTrainCarriage(const Vector3& Coors, bool bOnlyEngine)
{
	CVehicle::Pool  *VehiclePool = CVehicle::GetPool();
	CVehicle		*pVeh, *pResult = NULL;
	float			NearestDist = 9999999.9f;

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);

		if(pVeh && pVeh->InheritsFromTrain())
		{
			const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
			float	Dist = Vector2(vVehPosition.x - Coors.x, vVehPosition.y - Coors.y).Mag();
			if(Dist < NearestDist)
			{
				if((!bOnlyEngine) || (static_cast<CTrain*>(pVeh))->m_nTrainFlags.bEngine)
				{
					NearestDist = Dist;
					pResult = pVeh;
				}
			}
		}
	}
	return (static_cast<CTrain*>(pResult));
}


/////////////////////////////////////////////////////////////
// FUNCTION: SetNewTrainPosition
// PURPOSE:  Finds a new position on the track for this train.
/////////////////////////////////////////////////////////////
void CTrain::SetNewTrainPosition(CTrain* pEngine, const Vector3& NewCoors)
{
	AssertEntityPointerValid_NotInWorld(pEngine);
	Assert(pEngine->GetIsMissionTrain() || pEngine->IsNetworkClone());
	Assert(pEngine->m_nTrainFlags.bEngine || pEngine->IsNetworkClone() || pEngine->m_nTrainFlags.bAllowRemovalByPopulation); //allow independent cars to be repositioned remotely to correct for positional errors

	pEngine->SetPosition(NewCoors);
	pEngine->SetTrackPosFromWorldPos();
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION:	SkipToNextAllowedStation
// DOES:		Goes through all the stations and calls at the next one that
//				the player is allowed at.
///////////////////////////////////////////////////////////////////////////
void CTrain::SkipToNextAllowedStation(CTrain* pCarriage)
{
	Assert(pCarriage);
	Assert(pCarriage->m_trackIndex>=0);

	CTrain* pEngine = FindEngine(pCarriage);
	Assert(pEngine);
	if (!pEngine)
		return;

	float	trackPosOfStationCenter;
	s32	Station;
	pEngine->FindNextStationPositionInDirection(pEngine->m_nTrainFlags.bDirectionTrackForward, pEngine->m_trackPosFront, &trackPosOfStationCenter, &Station);
	FixupTrackPos(pEngine->m_trackIndex, trackPosOfStationCenter);

	const float carriageLength = pCarriage->GetBoundingBoxMax().y - pCarriage->GetBoundingBoxMin().y;
	const float trackMaxSpeed = (pEngine->m_trackIndex>=0)?(static_cast<float>(gTrainTracks[pEngine->m_trackIndex].m_maxSpeed)):0.0f;
	if(pEngine->m_nTrainFlags.bDirectionTrackForward)
	{
		pEngine->m_trackPosFront = trackPosOfStationCenter + (carriageLength * 0.5f);
		pEngine->m_trackForwardSpeed = trackMaxSpeed;
	}
	else
	{
		pEngine->m_trackPosFront = trackPosOfStationCenter - (carriageLength * 0.5f);
		pEngine->m_trackForwardSpeed = -trackMaxSpeed;
	}
	FixupTrackPos(pEngine->m_trackIndex, pEngine->m_trackPosFront);
	pEngine->m_trackForwardAccPortion = 0.0f;// Used for swinging the carriage forward and back later.
	pEngine->m_frontBackSwingAngPrev = 0.0f;

	//			CStreaming::LoadScene(m_aStationCoors[Station]); rage-streaming conv
	//			CStreaming::LoadAllRequestedModels(); rage-streaming

	float	Distance = (pEngine->m_trackIndex>=0)?(gTrainTracks[pEngine->m_trackIndex].m_aStationCoors[Station] - VEC3V_TO_VECTOR3(pEngine->GetTransform().GetPosition())).XYMag():0.0f;
	CClock::PassTime(3 + (s32)(Distance / 200.0f), false);

	// Need to process the engines so that they get moved out of any interiors (like the subway tunnel)
	// before the load scene happens this frame.
	CTrain* pProcessCarriage = pEngine;
	while(pProcessCarriage)
	{
		//pProcessCarriage->ProcessControl();
		pProcessCarriage->UpdatePosition(fwTimer::GetTimeStep());
		pProcessCarriage = pProcessCarriage->GetLinkedToBackward();
	}

	return;
}

///////////////////////////////////////////////////////////////////////////
// FUNCTION: GetCarraigeVertical
// PURPOSE:  Get the vertical offset for each carriage 
///////////////////////////////////////////////////////////////////////////
float CTrain::GetCarriageVerticalOffsetFromTrack() const
{
	float fOffset = 0.0f; 
	if( (m_trainConfigIndex >= 0 &&  m_trainConfigIndex < gTrainConfigs.m_trainConfigs.GetCount()))
	{
		if(m_trainConfigCarriageIndex>= 0 && m_trainConfigCarriageIndex <gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages.GetCount() )
		{
			fOffset = gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages[m_trainConfigCarriageIndex].m_fCarriageVertOffset; 
		}
	}
	return fOffset; 
}

int CTrain::GetNumCarriages() const
{
	int nNumCarriages = 1;
	if( (m_trainConfigIndex >= 0 &&  m_trainConfigIndex < gTrainConfigs.m_trainConfigs.GetCount()))
	{
		nNumCarriages = gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages.GetCount();
	}
	return nNumCarriages;
}

CTrainTrack * CTrain::GetTrainTrack(const s32 iTrackIndex)
{
	Assert(iTrackIndex >= 0 && iTrackIndex < MAX_TRAIN_TRACKS_PER_LEVEL);

	CTrainTrack * pTrack = &gTrainTracks[iTrackIndex];
	return pTrack;
}

s32 CTrain::GetTrackIndexFromNode(CTrainTrackNode * pNode)
{
	for(s32 t=0; t<MAX_TRAIN_TRACKS_PER_LEVEL; t++)
	{
		CTrainTrack * pTrack = GetTrainTrack(t);
		if(pTrack && pTrack->m_numNodes)
		{
			// Use a pointer comparison to determine to which track this node belongs
			CTrainTrackNode * pLastNode = &pTrack->m_pNodes[pTrack->m_numNodes-1];
			if(pNode >= pTrack->m_pNodes && pNode <= pLastNode)
			{
				return t;
			}
		}
	}
	return -1;
}

// NAME : IsTrainApproachingNode
// PURPOSE : Determine whether any train is approaching the input node
// TODO: Must make this aware of a train's length - currently only the engine is considered
CTrain* CTrain::IsTrainApproachingNode(CTrainTrackNode * pInputNode)
{
	if(!pInputNode)
		return NULL;
	const s32 iTrack = GetTrackIndexFromNode(pInputNode);
	if(iTrack==-1)
		return NULL;
	CTrainTrack * pTrack = GetTrainTrack(iTrack);
	if(!pTrack)
		return NULL;

	const bool bLoop = pTrack->m_isLooped;
	const s32 iNumNodes = pTrack->m_numNodes;

	static dev_float fScanAheadDistance = 200.0f;

	// Find all engines on this track
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	s32 v = (s32) VehiclePool->GetSize();

	while(v--)
	{
		CVehicle * pVehicle = VehiclePool->GetSlot(v);
		if(pVehicle && pVehicle->InheritsFromTrain())
		{
			CTrain * pTrain = (CTrain*)pVehicle;
			// Only consider train engines which are on this track
			if(!pTrain->m_nTrainFlags.bEngine || pTrain->m_trackIndex != iTrack)
				continue;

			// Find the final carriage on this train
			CTrain * pLastCarriage = pTrain;
			while(pLastCarriage->GetLinkedToBackward())
				pLastCarriage = pLastCarriage->GetLinkedToBackward();
			const s32 iLastCarriageNode = pLastCarriage->m_currentNode;
			const s32 iFrontNode = pTrain->m_currentNode;
			bool bTraversedTrain = false;

			// Scan along the track for the specified distance
			// If we hit the input node within this distance, then the train is a threat
			const int iScanDir = pTrain->m_nTrainFlags.bDirectionTrackForward ? 1 : -1;
			float fDistLeft = fScanAheadDistance;

			s32 iCurrNode = iLastCarriageNode;
			
			Assertf(iCurrNode >= 0 && iCurrNode < pTrack->m_numNodes, "iCurrNode: %i / %i", iCurrNode, pTrack->m_numNodes);
			if(iCurrNode < 0 || iCurrNode >= pTrack->m_numNodes)
				continue;

			float fLastDist = pTrack->GetNode(iCurrNode)->GetLengthFromStart();

			while(fDistLeft > 0.0f)
			{
				if(iCurrNode == iFrontNode)
					bTraversedTrain = true;

				// Inc/dec the current node
				iCurrNode += iScanDir;
				// Watch out for looping on circular tracks; I don't think this can actually
				// happen but have added this check in case
				if(iCurrNode == iLastCarriageNode)
					break;

				// Handle looped tracks
				if(iCurrNode >= iNumNodes)
				{
					if(bLoop)
					{
						iCurrNode = 0;

						Assertf(iCurrNode >= 0 && iCurrNode < pTrack->m_numNodes, "iCurrNode: %i / %i", iCurrNode, pTrack->m_numNodes);
						if(iCurrNode < 0 || iCurrNode >= pTrack->m_numNodes)
							break;

						fLastDist = pTrack->GetNode(iCurrNode)->GetLengthFromStart(); // avoid complications
					}
					else break;
				}
				else if(iCurrNode < 0)
				{
					if(bLoop)
					{
						iCurrNode = iNumNodes-1;

						Assertf(iCurrNode >= 0 && iCurrNode < pTrack->m_numNodes, "iCurrNode: %i / %i", iCurrNode, pTrack->m_numNodes);
						if(iCurrNode < 0 || iCurrNode >= pTrack->m_numNodes)
							break;

						fLastDist = pTrack->GetNode(iCurrNode)->GetLengthFromStart(); // avoid complications
					}
					else break;
				}

				Assertf(iCurrNode >= 0 && iCurrNode < pTrack->m_numNodes, "iCurrNode: %i / %i", iCurrNode, pTrack->m_numNodes);
				if(iCurrNode < 0 || iCurrNode >= pTrack->m_numNodes)
					break;

				const CTrainTrackNode * pCurrNode = pTrack->GetNode(iCurrNode);
				if(pCurrNode == pInputNode)
				{
					return pTrain;
				}

				const float fCurrDist = pCurrNode->GetLengthFromStart();
				const float fDistDelta = Abs(fCurrDist - fLastDist);
				fLastDist = fCurrDist;

				// We only start ticking off the distance left once we've traversed the
				// whole train; this way we will take into account nodes which are behind
				// the engine, but in front of the final carriage.
				if(bTraversedTrain)
					fDistLeft -= fDistDelta;
			}
		}
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////
// FUNCTION: IsLoopedTrack
// PURPOSE:  Determine if this track goes around in a loop connecting to
//				itself.
///////////////////////////////////////////////////////////////////////////
bool CTrain::IsLoopedTrack(s8 trackIndex)
{
	return (trackIndex >= 0)?gTrainTracks[trackIndex].m_isLooped:false;
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION: StopsAtStations
// PURPOSE:  Determine if this track allows for stopping at stations.
///////////////////////////////////////////////////////////////////////////
bool CTrain::StopsAtStations(s8 trackIndex)
{
	return (trackIndex >= 0) ? NetworkInterface::IsNetworkOpen() ? gTrainTracks[trackIndex].m_MPstopAtStations : gTrainTracks[trackIndex].m_stopAtStations : false;
}


///////////////////////////////////////////////////////////////////////////
// Take a track node which may have gone beyond the bounds of the track
// and properly wrap/ ping pong the value.
///////////////////////////////////////////////////////////////////////////
void CTrain::FixupTrackNode(s8 trackIndex, s32& nodeIndex_inOut)
{
	if(trackIndex < 0)
	{
		return;
	}

	const s32 totalTrackNodes = gTrainTracks[trackIndex].m_numNodes;
	Assert(nodeIndex_inOut > -totalTrackNodes);
	Assert(nodeIndex_inOut < (2 * totalTrackNodes));

	if(IsLoopedTrack(trackIndex))
	{
		if(nodeIndex_inOut < 0)
		{
			nodeIndex_inOut += totalTrackNodes;
		}
		if(nodeIndex_inOut >= totalTrackNodes)
		{
			nodeIndex_inOut -= totalTrackNodes;
		}
	}
	else
	{
		if(nodeIndex_inOut < 0)
		{
			nodeIndex_inOut = -nodeIndex_inOut;
		}
		if(nodeIndex_inOut >= totalTrackNodes)
		{
			nodeIndex_inOut = (2 * (totalTrackNodes - 1)) - nodeIndex_inOut;
		}
	}
}


///////////////////////////////////////////////////////////////////////////
// Take a track position which may have gone beyond the bounds of the track
// and properly wrap/ ping pong the value.
///////////////////////////////////////////////////////////////////////////
bool CTrain::FixupTrackPos(s8 trackIndex, float& trackPos_inOut)
{
	if(trackIndex < 0)
	{
		return false;
	}

	bool bIsAtEndOfTrack = false;

	const float totalTrackLength = gTrainTracks[trackIndex].GetTotalTrackLength();
	if(IsLoopedTrack(trackIndex))
	{
		Assertf(trackPos_inOut >= (-2.0f * totalTrackLength), "CTrain::FixupTrackPos track pos %3.2f is out of range, track index %d, track length %3.2f",
			trackPos_inOut, trackIndex, totalTrackLength);
		Assertf(trackPos_inOut <= (2.0f * totalTrackLength), "CTrain::FixupTrackPos track pos %3.2f is out of range, track index %d, track length %3.2f",
			trackPos_inOut, trackIndex, totalTrackLength);

		while(trackPos_inOut < 0.0f)
		{
			trackPos_inOut += totalTrackLength;
			bIsAtEndOfTrack = true;
		}
		while(trackPos_inOut >= totalTrackLength)
		{
			trackPos_inOut -= totalTrackLength;
			bIsAtEndOfTrack = true;
		}
	}
	else
	{
		Assertf(trackPos_inOut > -totalTrackLength, "CTrain::FixupTrackPos track pos %3.2f is out of range, track index %d, track length %3.2f",
			trackPos_inOut, trackIndex, totalTrackLength);
		Assertf(trackPos_inOut < (2.0f * totalTrackLength), "CTrain::FixupTrackPos track pos %3.2f is out of range, track index %d, track length %3.2f",
			trackPos_inOut, trackIndex, totalTrackLength);

		if(trackPos_inOut < 0.0f)
		{
			trackPos_inOut = -trackPos_inOut;
			bIsAtEndOfTrack = true;
		}
		if(trackPos_inOut >= totalTrackLength)
		{
			trackPos_inOut = (2.0f * totalTrackLength) - trackPos_inOut;
			bIsAtEndOfTrack = true;
		}
	}

	return bIsAtEndOfTrack;
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION: CalcWorldPosAndForward
// PURPOSE:  Determine the world position and direction of a carriage.
///////////////////////////////////////////////////////////////////////////
void CTrain::CalcWorldPosAndForward(Vector3& worldPos_out, Vector3& forward_out, s8 trackIndex, float trackPosFront, bool bDirectionTrackForward, s32 initialNodeToSearchFrom, float carriageLength, bool carriagesAreSuspended, bool flipModelDir, bool& bIsTunnel)
{
	const float preferredPositionSmoothingLength = carriagesAreSuspended?0.0f:carriageLength;

	// Find the carriage front position.
	Vector3 carriageWorldPosFront(0.0f, 0.0f, 0.0f);
	{
		s32	currentNode = FindCurrentNodeFromInitialNode(trackIndex, trackPosFront, bDirectionTrackForward, initialNodeToSearchFrom);
		s32	nextNode = currentNode + ((bDirectionTrackForward)?(1):(-1));
		FixupTrackNode(trackIndex, nextNode);
		Assert(currentNode != nextNode);
		if(currentNode > nextNode){SwapEm(currentNode, nextNode);}
		Assert(currentNode < nextNode);
		GetWorldPosFromTrackPos(carriageWorldPosFront, currentNode, nextNode, trackIndex, trackPosFront, preferredPositionSmoothingLength, bIsTunnel);
	}

	// Find the carriage rear position.
	float carriageTrackPosRear = (bDirectionTrackForward)?(trackPosFront - carriageLength):(trackPosFront + carriageLength);
	FixupTrackPos(trackIndex, carriageTrackPosRear);
	Vector3 carriageWorldPosRear(0.0f, 0.0f, 0.0f);
	{
		s32	currentNode = FindCurrentNodeFromInitialNode(trackIndex, carriageTrackPosRear, bDirectionTrackForward, initialNodeToSearchFrom);
		s32	nextNode = currentNode + ((bDirectionTrackForward)?(1):(-1));
		FixupTrackNode(trackIndex, nextNode);
		Assert(currentNode != nextNode);
		if(currentNode > nextNode){SwapEm(currentNode, nextNode);}
		Assert(currentNode < nextNode);
		GetWorldPosFromTrackPos(carriageWorldPosRear, currentNode, nextNode, trackIndex, carriageTrackPosRear, preferredPositionSmoothingLength, bIsTunnel);
	}

	// Determine our new position.
	worldPos_out = carriagesAreSuspended?carriageWorldPosFront:(carriageWorldPosFront + carriageWorldPosRear)*0.5f;

	// Cable cars travel both directions. Other trains are placed with their nose in the forward direction of the track.
	forward_out = carriageWorldPosFront - carriageWorldPosRear;
	forward_out.Normalize();
	//if(IsLoopedTrack(trackIndex))
	//{
	//	if(!bDirectionTrackForward){forward_out = -forward_out;}
	//}
	if(flipModelDir){forward_out = -forward_out;}
}
s32 CTrain::FindCurrentNode(s8 trackIndex, float trackPos, bool bDirectionTrackForward)
{
#if __ASSERT
	const float totalTrackLength = gTrainTracks[trackIndex].GetTotalTrackLength();
#endif // __ASSERT
	Assert(trackPos >= 0.0f);
	Assert(trackPos <= totalTrackLength);

// DEBUG!! -AC, We replace the linear search with a binary search as the
// tracks can be very long and thus a linear search can be quite slow...
// http://en.wikipedia.org/wiki/Binary_search_algorithm
	// Get the closest node earlier or equal to the position
	// along the track.
	//s32 nodeIndex = 0;
	//bool found = false;
	//const s32 nodeCount = gTrainTracks[trackIndex].m_numNodes;
	//for(s32 i = 1; i < nodeCount; ++i)
	//{
	//	const float nodeLengthFromStart = gTrainTracks[trackIndex].m_pNodes[i].GetLengthFromStart();
	//	if(nodeLengthFromStart > trackPos)
	//	{
	//		nodeIndex = i - 1;
	//		found = true;
	//		break;
	//	}
	//}
	//if(!found)
	//{
	//	nodeIndex = nodeCount - 1;
	//}

	const s32 nodeCount	= gTrainTracks[trackIndex].m_numNodes;
	const float lastNodeLengthFromStart = gTrainTracks[trackIndex].m_pNodes[nodeCount-1].GetLengthFromStart();
	if(trackPos >= lastNodeLengthFromStart)
	{
		return nodeCount-1;
	}

	s32	nodeIndex		= 0;
	bool	found			= false;
	bool	searchExhausted	= false;

	s32	boundLeft		= 0;
	s32	boundRight		= nodeCount - 1;
	while(!found && !searchExhausted)
	{
		s32 spanWidth = boundRight - boundLeft;
		Assert(spanWidth > 0);
		if(spanWidth == 1)
		{
			searchExhausted = true;
			break;
		}

		nodeIndex = boundLeft + (spanWidth / 2);
		const float nodeLengthFromStart = gTrainTracks[trackIndex].m_pNodes[nodeIndex].GetLengthFromStart();
		if(nodeLengthFromStart < trackPos)
		{
			boundLeft = nodeIndex;
		}
		else if(nodeLengthFromStart > trackPos)
		{
			boundRight = nodeIndex;
		}
		else
		{
			found = true;
			break;
		}
	}
	if(!found)
	{
		Assert(searchExhausted);
		nodeIndex = boundLeft;
	}
// END DEBUG!!

	if(bDirectionTrackForward)
	{
		return nodeIndex;
	}
	else
	{
// Fix for B*7007679 ... code didnt handle 1-0 segment properly for trains traveling in opposite direction 
		FastAssert( nodeIndex <= (gTrainTracks[trackIndex].m_numNodes - 1) );
		if( nodeIndex == (gTrainTracks[trackIndex].m_numNodes - 1) )
		{
			return nodeIndex;
		}
		else
		{
			nodeIndex = (nodeIndex + 1);
		}
	}

	return nodeIndex;
}
s32 CTrain::FindCurrentNodeFromInitialNode(s8 trackIndex, float trackPos, bool bDirectionTrackForward, s32 initialNodeToSearchFrom)
{
#if __ASSERT
	const float totalTrackLength = gTrainTracks[trackIndex].GetTotalTrackLength();
#endif // __ASSERT
	Assert(trackPos >= 0.0f);
	Assert(trackPos <= totalTrackLength);

	// Get the closest node earlier or equal to the position
	// along the track.
	s32 nodeIndex = 0;
	const float initialNodeLengthFromStart = gTrainTracks[trackIndex].m_pNodes[initialNodeToSearchFrom].GetLengthFromStart();
	if(initialNodeLengthFromStart == trackPos)
	{
		nodeIndex = initialNodeToSearchFrom;
	}
	else if(initialNodeLengthFromStart < trackPos)
	{
		// Do a linear search forward along the track.
		bool found = false;
		const s32 nodeCount = gTrainTracks[trackIndex].m_numNodes;
		for(s32 i = initialNodeToSearchFrom + 1; i < nodeCount; ++i)
		{
			const float nodeLengthFromStart = gTrainTracks[trackIndex].m_pNodes[i].GetLengthFromStart();
			if(nodeLengthFromStart > trackPos)
			{
				nodeIndex = i - 1;
				found = true;
				break;
			}
		}
		if(!found)
		{
			nodeIndex = nodeCount - 1;
		}
	}
	else // (initialNodeLengthFromStart > trackPos)
	{
		// Do a linear search backward along the track.
		bool found = false;
		for(s32 i = initialNodeToSearchFrom; i > 0; --i)
		{
			const float nodeLengthFromStart = gTrainTracks[trackIndex].m_pNodes[i].GetLengthFromStart();
			if(nodeLengthFromStart <= trackPos)
			{
				nodeIndex = i;
				found = true;
				break;
			}
		}
		if(!found)
		{
			nodeIndex = 0;
		}
	}

	// Make sure the node given back makes sense for the direction of movement for the
	// train.  (And whether it is looped or not, but that doesn't change the algorithm
	// below.)
	if(bDirectionTrackForward)
	{
		return nodeIndex;
	}
	else
	{
// Fix for B*7007679 ... code didnt handle 1-0 segment properly for trains traveling in opposite direction 
		FastAssert( nodeIndex <= (gTrainTracks[trackIndex].m_numNodes - 1) );
		if( nodeIndex == (gTrainTracks[trackIndex].m_numNodes - 1) )
		{
			return nodeIndex;
		}
		else
		{
			nodeIndex = (nodeIndex + 1);
		}
	}

	return nodeIndex;
}

bool CTrain::GetWorldPosFromTrackNode(Vector3 &worldPos, s8 trackIndex, s32 trackNode)
{
    bool positionFound = false;

    if(Verifyf(trackIndex >= 0 && trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL, "Invalid track index specified!"))
    {
        if(Verifyf(trackNode >= 0 && trackNode < gTrainTracks[trackIndex].m_numNodes, "Invalid track node specified!"))
        {
            worldPos = gTrainTracks[trackIndex].m_pNodes[trackNode].GetCoors();

            positionFound = true;
        }
    }

    return positionFound;
}

void CTrain::GetWorldPosFromTrackPos(Vector3& worldPos_out, s32& currentNode, s32& nextNode, s8 trackIndex, float trackPos, float preferredSmoothLength, bool& bIsTunnel)
{
	Assert(currentNode != nextNode);
	Assert(currentNode < nextNode);
#if __ASSERT
	const float totalTrackLength = gTrainTracks[trackIndex].GetTotalTrackLength();
#endif // __ASSERT
	Assert(trackPos >= 0.0f);
	Assert(trackPos <= totalTrackLength);

	const CTrainTrackNode*	pTrackNodes			= gTrainTracks[trackIndex].m_pNodes;
	const s32				numTrackNodes		= gTrainTracks[trackIndex].m_numNodes;


	// Find the interpolation values.
	float segmentLength = (pTrackNodes[nextNode].GetCoors() - pTrackNodes[currentNode].GetCoors()).Mag();
	float segmentPos = 0.0f;
	if(currentNode == 0 && nextNode == (numTrackNodes-1))
	{
		segmentPos = segmentLength - (trackPos - pTrackNodes[nextNode].GetLengthFromStart());
	}
	else
	{
		segmentPos = trackPos - pTrackNodes[currentNode].GetLengthFromStart();
	}
	const float segmentPosPortion = segmentPos / segmentLength;

	TUNE_GROUP_BOOL(TRAINS, doPositionalSmoothing, false);
	if(!doPositionalSmoothing)
	{
		worldPos_out = (pTrackNodes[nextNode].GetCoors() * segmentPosPortion) + (pTrackNodes[currentNode].GetCoors() * (1.0f - segmentPosPortion));
	}
	else
	{
		// Determine if the position + the smooth length spills over to the next or previous node.
		if(segmentPos > segmentLength - preferredSmoothLength && segmentPos > segmentLength * 0.5f)
		{
			// Smooth out taking the next node into account.
			s32 nextNextNode = (nextNode + 1);
			FixupTrackNode(trackIndex, nextNextNode);

			if(!GetWorldPosFromTrackSegmentPos(worldPos_out, segmentPos - segmentLength, currentNode, nextNode, nextNextNode, pTrackNodes, preferredSmoothLength))
			{
				worldPos_out.x = pTrackNodes[nextNode].GetCoorsX() * segmentPosPortion + pTrackNodes[currentNode].GetCoorsX() * (1.0f - segmentPosPortion);
				worldPos_out.y = pTrackNodes[nextNode].GetCoorsY() * segmentPosPortion + pTrackNodes[currentNode].GetCoorsY() * (1.0f - segmentPosPortion);
			}

			// Calculate height using the normal interpolation
			worldPos_out.z = pTrackNodes[nextNode].GetCoorsZ() * segmentPosPortion + pTrackNodes[currentNode].GetCoorsZ() * (1.0f - segmentPosPortion);
		}
		else if(segmentPos < preferredSmoothLength && segmentPos < segmentLength * 0.5f)
		{
			// Smooth out taking the previous node into account.
			s32 previousNode = (currentNode - 1);
			FixupTrackNode(trackIndex, previousNode);

			if(!GetWorldPosFromTrackSegmentPos(worldPos_out, segmentPos, previousNode, currentNode, nextNode, pTrackNodes, preferredSmoothLength))
			{
				worldPos_out.x = pTrackNodes[nextNode].GetCoorsX() * segmentPosPortion + pTrackNodes[currentNode].GetCoorsX() * (1.0f - segmentPosPortion);
				worldPos_out.y = pTrackNodes[nextNode].GetCoorsY() * segmentPosPortion + pTrackNodes[currentNode].GetCoorsY() * (1.0f - segmentPosPortion);
			}

			// Calculate height using the normal interpolation
			worldPos_out.z = pTrackNodes[nextNode].GetCoorsZ() * segmentPosPortion + pTrackNodes[currentNode].GetCoorsZ() * (1.0f - segmentPosPortion);
		}
		else
		{
			// Simply do a normal interpolation
			worldPos_out = pTrackNodes[nextNode].GetCoors() * segmentPosPortion + pTrackNodes[currentNode].GetCoors() * (1.0f - segmentPosPortion);
		}
		Assert(worldPos_out.x > WORLDLIMITS_XMIN && worldPos_out.x < WORLDLIMITS_XMAX &&
			worldPos_out.y > WORLDLIMITS_YMIN && worldPos_out.y < WORLDLIMITS_YMAX &&
			worldPos_out.z > WORLDLIMITS_ZMIN && worldPos_out.z < WORLDLIMITS_ZMAX);
	}

	bIsTunnel = pTrackNodes[currentNode].GetIsTunnel();
}
bool CTrain::GetWorldPosFromTrackSegmentPos(Vector3& worldPos_out, float segmentPos, s32 PreviousNode, s32 Node, s32 NextNode, const CTrainTrackNode *pTrackNodes, float PreferredSmoothLength)
{
	// The idea is to identify the circle that touches both the incoming and outgoing lines. The interpolation then proceeds along that circle.

	Vector2 PreviousCoors = pTrackNodes[PreviousNode].GetCoorsXY();
	Vector2 Coors = pTrackNodes[Node].GetCoorsXY();
	Vector2 NextCoors = pTrackNodes[NextNode].GetCoorsXY();

	Vector2	Point1, Point2, CircleCentre;
	Vector2	Vec1, Vec2f;

	Vec1 = Coors - PreviousCoors;
	Vec2f = NextCoors - Coors;

	float	Length1 = Vec1.Mag();
	float	Length2 = Vec2f.Mag();

	// Make sure the smooth length isn't so big that interpolations for adjacent nodes overlap
	float	SmoothLength = rage::Min(PreferredSmoothLength, rage::Min((Length1 * 0.5f), (Length2 * 0.5f)));

	// Test of we're too far from
	if(rage::Abs(segmentPos) > SmoothLength)
	{
		return false;
	}

	Point1 = Coors - Vec1 * (SmoothLength / Length1);
	Point2 = Coors + Vec2f * (SmoothLength / Length2);

	Vector2	Perp1, Perp2;		// The vectors from the points where the circle touches the path to the centerpoint of the circle.
	Perp1.x = Vec1.y;
	Perp1.y = -Vec1.x;
	Perp2.x = Vec2f.y;
	Perp2.y = -Vec2f.x;
	Perp1.Normalize();
	Perp2.Normalize();
	// If the two segments are aligned we use the old interpolation (our algorhytm would fail)
	if(rage::Abs(DotProduct(Perp1, Perp2)) > 0.995f)
	{
		return false;
	}

	// Now we calculate the actual centre point of the circle.
	float t = (Point1.x - Point2.x) / (Perp2.x - Perp1.x);
	Vector2 CentrePoint;
	CentrePoint.x = Point1.x + t * Perp1.x;
	CentrePoint.y = Point1.y + t * Perp1.y;
	float	Radius = rage::Abs(t);

	// Now calculate the angles that correspond with the points touching the circle.
	float	Angle1 = rage::Atan2f(Point1.x - CentrePoint.x, Point1.y - CentrePoint.y);
	float	Angle2 = rage::Atan2f(Point2.x - CentrePoint.x, Point2.y - CentrePoint.y);

	// Calculate the actual angle for this point in the interpolation.
	float	DeltaAngle = Angle2 - Angle1;
	DeltaAngle = fwAngle::LimitRadianAngle(DeltaAngle);
	float	Inter = (segmentPos + SmoothLength) / (2.0f * SmoothLength);
	float	ActualAngle = Angle1 + DeltaAngle * Inter;

	// Now calculate the actual smoothed out coordinate

	worldPos_out.x = CentrePoint.x + Radius * rage::Sinf(ActualAngle);
	worldPos_out.y = CentrePoint.y + Radius * rage::Cosf(ActualAngle);

	return true;
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// INSTANCE MEMBERS
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// initialise data in constructor
///////////////////////////////////////////////////////////////////////////
CTrain::CTrain(const eEntityOwnedBy ownedBy, u32 popType) : CVehicle(ownedBy, popType, VEHICLE_TYPE_TRAIN)
{
	trainDebugf2("CTrain::CTrain [%p] m_pEngine[%p]",this,m_pEngine.Get());

	m_updateLODState = SIMPLIFIED;
	m_updateLODFadeStartTimeMS = 0;
	m_updateLODCurrentAlpha = 0.0f;

	m_nTrainFlags.bAtStation = false;
	m_nTrainFlags.bStopForStations = true;
	m_nTrainFlags.bIsForcedToSlowDown = true;
	m_nTrainFlags.bHasPassengerCarriages = false;
	m_nTrainFlags.bIsCarriageTurningRight = false;
	m_nTrainFlags.bRenderAsDerailed = false;
	m_nTrainFlags.bStartedClosingDoors = false;
	m_nTrainFlags.bStartedOpeningDoors = false;
	m_nTrainFlags.bDisableNextStationAnnouncements = false;
	m_nTrainFlags.bCreatedAsScriptVehicle = false;
	m_nTrainFlags.bIsCutsceneControlled = false;
	m_nTrainFlags.bIsSyncedSceneControlled = false;
	m_nTrainFlags.bAllowRemovalByPopulation = false;

	SetFixedPhysics(true);
	m_nVehicleFlags.bUnfreezeWhenCleaningUp = false;

	m_trackIndex = -1;
	m_trainConfigIndex = -1;
	m_trainConfigCarriageIndex = -1;
	m_trackDistFromEngineFrontForCarriageFront = 0.0f;
	m_trackPosFront = 0.0f;
	m_trackForwardSpeed = 0.0f;
	m_trackForwardAccPortion = 0.0f;// Used for swinging the carriage forward and back later.
	m_frontBackSwingAngPrev = 0.0f;
	m_trackSteepness = 0.0f;
	m_currentNode = 0;
	m_oldCurrentNode = 0;


	m_nPhysicalFlags.bNotDamagedByAnything = true;

	m_pLinkedToForward = NULL;
	m_pLinkedToBackward = NULL;

	m_wheelSquealIntensity = 0.0f;
	m_inactiveMoveSpeed = VEC3_ZERO;
	m_inactiveTurnSpeed = VEC3_ZERO;
	m_inactiveLastMatrix.Identity();

	// Trains are always on __ALWAYS__
	SwitchEngineOn(true);

	m_nextStation = m_currentStation = 0;
	m_travelDistToNextStation = 0.0f;
	m_iTimeOfSecondaryPadShake = 0;

	m_desiredCruiseSpeed = 8.0f;

	m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;
	
	//Change the train state.
	ChangeTrainState(TS_Moving);
	
	//Initialize the passengers state.
	m_nPassengersState = PS_None;

//	m_TimeSinceRoadJunctionSet = 0;
//	m_pRoadJunction.SetEmpty();

	m_iLastTimeScannedForVehicles = 0;
	m_pVehicleBlockingProgress = NULL;

	m_iLastTimeScannedForPeds = 0;
	m_pPedBlockingProgress = NULL;

	m_iLastHornTime = 0;
	m_iTimeWaitingForBlockage = 0;

	m_fTimeSinceThreatened = 1000.0f;

	m_iMissionScriptId = THREAD_INVALID;

	m_requestInteriorRescanTime = 0;

	m_bMetroTrain = false;
	m_bArchetypeSet = false;

	m_doorsForcedOpen = false;

#if __ASSERT
	m_uFrameCreated = fwTimer::GetFrameCount();
#endif
}

///////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////
CTrain::~CTrain()
{
	trainDebugf2("CTrain::~CTrain [%p] m_pEngine[%p]",this,m_pEngine.Get());

	NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.CancelAnySentencesFromTracker(GetPlaceableTracker()));

	m_pLinkedToBackward = NULL;
	m_pLinkedToForward = NULL;

	DEV_BREAK_IF_FOCUS(CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this);
	DEV_BREAK_ON_PROXIMITY(CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(GetTransform().GetPosition()));

	PHLEVEL->ReleaseInstLastMatrix(GetCurrentPhysicsInst());
}

bool CTrain::CanGetOff() const
{
	return CanGetOnOrOff();
}

bool CTrain::CanGetOn() const
{
	return CanGetOnOrOff();
}

bool CTrain::CanGetOnOrOff() const
{
	//Check the train state.
	switch(m_nTrainState)
	{
		case TS_OpeningDoors:
		case TS_WaitingAtStation:
		case TS_ClosingDoors:
		{
			return true;
		}
		case TS_Moving:
		case TS_ArrivingAtStation:
		case TS_LeavingStation:
		case TS_Destroyed:
		{
			return false;
		}
		default:
		{
			vehicleAssertf(false, "Unknown train state: %d.", m_nTrainState);
			return false;
		}
	}
}

bool CTrain::IsMoving() const
{
	return (m_nTrainState == TS_Moving);
}

bool CTrain::AreDoorsMoving() const
{
	return m_nTrainState == TS_OpeningDoors || m_nTrainState == TS_ClosingDoors || m_nTrainState == TS_WaitingAtStation;
}

bool CTrain::SetUpDriverOnEngine(bool bUseExistingNodes)
{
	if (this == m_pEngine)
	{
		return ( SetUpDriver(bUseExistingNodes) != NULL );
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
// Set the model to use for rendering (and size calculations).
///////////////////////////////////////////////////////////////////////////
void CTrain::SetModelId(fwModelId modelId)
{
	//Call the base class version.
	CVehicle::SetModelId(modelId);
}



///////////////////////////////////////////////////////////////////////////
// Update the dooors and door audio.
///////////////////////////////////////////////////////////////////////////
bool CTrain::ProcessDoors(float fTargetRatio)
{
	const bool doProcessDoors = ShouldDoFullUpdate();
	if(!doProcessDoors)
	{
		return false;
	}
	
	//Keep track of the doors.
	CCarDoor* aDoors[4] = { NULL, NULL, NULL, NULL };
	int iNumDoors = 0;
	
	//Check the side flags closest to the platform.
	fwFlags8 uSideFlags = GetSideFlagsClosestToPlatform();
	bool bOpenAllDoors = NetworkInterface::IsGameInProgress() && m_bMetroTrain;
	if(uSideFlags.IsFlagSet(SF_Left) || bOpenAllDoors)
	{
		aDoors[iNumDoors++] = GetDoorFromId(VEH_DOOR_DSIDE_F);
		aDoors[iNumDoors++] = GetDoorFromId(VEH_DOOR_DSIDE_R);
	}
	if(uSideFlags.IsFlagSet(SF_Right) || bOpenAllDoors)
	{
		aDoors[iNumDoors++] = GetDoorFromId(VEH_DOOR_PSIDE_F);
		aDoors[iNumDoors++] = GetDoorFromId(VEH_DOOR_PSIDE_R);
	}
	
	//Check if we are opening or closing the doors.
	const bool bClosingDoors = (fTargetRatio == 0.0f);
	const bool bOpeningDoors = (fTargetRatio == 1.0f);
	
	//Iterate over the doors.
	bool bHaveAllDoorsReachedTargetRatio = true;
	for(int d = 0; d < iNumDoors; d++)
	{
		//Ensure the door is valid.
		CCarDoor * pDoor = aDoors[d];
		if(!pDoor)
		{
			continue;
		}
		
		if(!m_nTrainFlags.bStartedClosingDoors && bClosingDoors)
		{
			m_VehicleAudioEntity->TriggerDoorCloseSound(pDoor->GetHierarchyId(), false);
			m_nTrainFlags.bStartedClosingDoors = true;
			m_nTrainFlags.bStartedOpeningDoors = false;
		}
		else if(!m_nTrainFlags.bStartedOpeningDoors && bOpeningDoors)
		{
			m_VehicleAudioEntity->TriggerDoorOpenSound(pDoor->GetHierarchyId());
			m_nTrainFlags.bStartedOpeningDoors = true;
			m_nTrainFlags.bStartedClosingDoors = false;
		}

		//Get the ratio.
		float fRatio = pDoor->GetDoorRatio();

		//Check if the door is closing, and blocked.
		if(m_nTrainFlags.bStartedClosingDoors && IsDoorBlocked(*pDoor))
		{
			//Set the target ratio.
			pDoor->SetTargetDoorOpenRatio(fRatio, CCarDoor::DRIVEN_SMOOTH);

			//Note that all doors have not reached their target ratio.
			bHaveAllDoorsReachedTargetRatio = false;
		}
		else
		{
			//Set the target ratio.
			pDoor->SetTargetDoorOpenRatio(fTargetRatio, CCarDoor::DRIVEN_SMOOTH);
			
			//Check if this door has not reached its target ratio.
			if(Abs(fRatio - fTargetRatio) > SMALL_FLOAT)
			{
				bHaveAllDoorsReachedTargetRatio = false;
			}
		}
	}
	
	m_nTrainFlags.bStartedClosingDoors = bClosingDoors;
	m_nTrainFlags.bStartedOpeningDoors = bOpeningDoors;
	
	return bHaveAllDoorsReachedTargetRatio;
}


///////////////////////////////////////////////////////////////////////////
// Update visual FX such as wheel sparks, etc.
///////////////////////////////////////////////////////////////////////////
void CTrain::ProcessVfx()
{
	const bool doProcessVfx = ShouldDoFullUpdate();
	if(!doProcessVfx)
	{
		return;
	}

	float squealIntensity = m_wheelSquealIntensity;

#if __BANK
	if(g_vfxVehicle.GetTrainSquealOverride()>0.0f)
	{
		squealIntensity = g_vfxVehicle.GetTrainSquealOverride();
	}
#endif

	if(squealIntensity>0.25f)
	{
		// calc squeal evo
		float squealEvo = (squealIntensity-0.25f)/0.75f;
		Assert(squealEvo>=0.0f);
		Assert(squealEvo<=1.0f);

		// calc start wheel
		bool isLeft = true;
		s32 startWheel = VEH_WHEEL_LF;
		if(m_nTrainFlags.bIsCarriageTurningRight)
		{
			startWheel = VEH_WHEEL_RF;
			isLeft = false;
		}

		// get the wheel matrices
		CVehicleModelInfo* pVehicleModelInfo = GetVehicleModelInfo();
		Assert(pVehicleModelInfo);

		Vector3 wheelOffset(0.0f, 0.0f, -pVehicleModelInfo->GetRimRadius());

		for (s32 i=0; i<3; i++)
		{
			// wheels now go lf rf, then lm rm, then lr rr
			s32 boneId = pVehicleModelInfo->GetBoneIndex(startWheel+2*i);
			if(boneId>=0)
			{
				g_vfxVehicle.UpdatePtFxTrainWheelSparks(this, boneId, RCC_VEC3V(wheelOffset), squealEvo, i, isLeft);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////
// Lights inside the carriage.
///////////////////////////////////////////////////////////////////////////
void CTrain::DoInteriorLights()
{
	const bool doInteriorLightsUpdate = ShouldDoFullUpdate();
	if(!doInteriorLightsUpdate)
	{
		return;
	}

	const bool doInteriorLights = (m_trainConfigIndex>=0) && (m_trainConfigCarriageIndex>=0) && gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages[m_trainConfigCarriageIndex].m_doInteriorLights;
	if(!doInteriorLights)
	{
		return;
	}

	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();

	const float camDist = rage::Sqrtf(CVfxHelper::GetDistSqrToCamera(GetTransform().GetPosition()));
	const float fade = rage::Clamp((TrainLight_fadingDistance - camDist)*oo_TrainLights_FadeLength,0.0f,1.0f);
	if(fade <= 0.0f)
	{
		return;
	}

	// Test if we want to avoid having the light affect alpha stuff
	const bool playerIsInside = ContainsLocalPlayer();
	bool bDontRenderAlpha = playerIsInside && (camInterface::IsRenderedCameraInsideVehicle() || CVfxHelper::GetCamInVehicleFlag());
	bDontRenderAlpha |= (CVfxHelper::GetRenderLastVehicle() == this);
	int lightFlags = LIGHTFLAG_VEHICLE | (bDontRenderAlpha ? LIGHTFLAG_DONT_LIGHT_ALPHA : 0);
	//Displayf("Alpha is: %s", bDontRenderAlpha ? "off" : "on");

	for (s32 boneIdx = VEH_MISC_A; boneIdx <= VEH_MISC_F; boneIdx++)
	{
		const s32 boneId = pModelInfo->GetBoneIndex(boneIdx);
		if(boneId <= -1)
		{
			continue;
		}

		Matrix34 worldMtx;
		GetGlobalMtx(boneId, worldMtx);

		fwInteriorLocation interiorLocation = this->GetInteriorLocation();

		CLightSource light(
			LIGHT_TYPE_POINT,
			lightFlags,
			worldMtx.d,
			Vector3(TrainLight_R,TrainLight_G,TrainLight_B),
			TrainLight_Intensity * fade,
			LIGHT_ALWAYS_ON);
		light.SetDirTangent(Vector3(0,0,-1), Vector3(0,1,0));
		light.SetRadius(TrainLight_FalloffMax);
		light.SetInInterior(interiorLocation);
		Lights::AddSceneLight(light);
	}
}


///////////////////////////////////////////////////////////////////////////
// Do some skeletal updates if necessary.
///////////////////////////////////////////////////////////////////////////
ePrerenderStatus CTrain::PreRender(const bool bIsVisibleInMainViewport)
{
	const bool doPreRender = ShouldDoFullUpdate();
	if(!doPreRender)
	{
		return PRERENDER_DONE;
	}

	DEV_BREAK_IF_FOCUS(CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this);
	DEV_BREAK_ON_PROXIMITY(CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(GetTransform().GetPosition()));

	// DEBUG!! -AC, This should really assert if there is no pivot bone, but not all the models are
	// properly set up with the correct bone ID yet (only the cable car is)...
	if(CarriagesAreSuspended())
	{
		// Get the pivot bones matrix so we can adjust its angle.
		CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();
		Assertf(pModelInfo, "Error getting the model info for the train carriage.");
		s32 boneId = pModelInfo->GetBoneIndex(VEH_TRANSMISSION_F);
		if(boneId< 0)
		{
			//Warningf("Suspended carriages must have a pivot bone (VEH_TRANSMISSION_F).");
		}
		else
		{
			Assertf(GetSkeletonData().GetParentIndex(boneId) == 0,"Suspended carriages expect the pivot bone to be a child of the root.");
			Matrix34 &boneMat = GetLocalMtxNonConst(boneId);

			// Make adjust the pivot bone.
			boneMat.Identity3x3();
			boneMat.RotateLocalX(m_trackSteepness);
		}
	}
	// END DEBUG!!

	for(int i=0; i<GetNumDoors(); i++)
		GetDoor(i)->ProcessPreRender(this);

	return CVehicle::PreRender(bIsVisibleInMainViewport);
}


///////////////////////////////////////////////////////////////////////////
// Do some lighting and FX updates if necessary.
///////////////////////////////////////////////////////////////////////////
void CTrain::PreRender2(const bool bIsVisibleInMainViewport)
{
	const bool doPreRender2 = ShouldDoFullUpdate();
	if(!doPreRender2)
	{
		return;
	}

	CVehicle::PreRender2(bIsVisibleInMainViewport);

	//Disabling the ignore damage flag for the lights to work properly
	//s32 lightFlags = LIGHTS_IGNORE_DAMAGE;
	s32 lightFlags = 0;
	if(m_nTrainFlags.bEngine)
	{
		// Headlights only
		lightFlags |= LIGHTS_IGNORE_TAILLIGHTS;
	}
	else if(m_nTrainFlags.bCaboose)
	{
		// Taillights only
		lightFlags |= LIGHTS_IGNORE_HEADLIGHTS;
	}
	else
	{
		// Special(emissive and such) lights only.
		lightFlags |= LIGHTS_IGNORE_TAILLIGHTS | LIGHTS_IGNORE_HEADLIGHTS;
	}


	// The engine stater seems to be fucked around with by scripters,
	// so we force the lights manually.
	m_OverrideLights = GetVehicleLightsStatus() ? FORCE_CAR_LIGHTS_ON : NO_CAR_LIGHT_OVERRIDE;

	Vec4V ambientVolumeParams(Train_AmbientVolume_Offset, ScalarV(Train_AmbientVolume_Intensity_Scaler));
	//Adding volume scaler to add more strength to the ambient volumes under the train
	DoVehicleLights(lightFlags, ambientVolumeParams);
	// Apply interior train lights
	DoInteriorLights();

	// process vfx
	ProcessVfx();
}

void CTrain::CorrectLocalPlayerInsideTrain()
{
	static atHashWithStringNotFinal s_hMetroTrain("metrotrain",0x33C9E158);
	static atHashWithStringNotFinal s_hFlatBedTrain("FREIGHTCAR",0xAFD22A6);
	 
	bool bEngineMetroFlatBedInterior = IsEngine() || (GetBoneIndex(TRAIN_INTERIOR) != -1) || 
										(GetArchetype()->GetModelNameHash() == s_hMetroTrain.GetHash()) || 
										(GetArchetype()->GetModelNameHash() == s_hFlatBedTrain.GetHash()) || 
										(MI_CAR_FREIGHTCAR2.IsValid() && GetModelIndex() == MI_CAR_FREIGHTCAR2);

	if (!bEngineMetroFlatBedInterior)
	{
		CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			CEntity *pPlayerGroundPhysical = pPlayerPed->GetGroundPhysical();
			if (pPlayerGroundPhysical == this)
			{
				spdAABB tempBox;
				const spdAABB &vehicleBox = GetBoundBox(tempBox);

				Vector3 vPedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

				bool bInsideVehicleZ = false;
				if (vPedPos.z > vehicleBox.GetMin().GetZf() && vPedPos.z < (vehicleBox.GetMax().GetZf() - 0.3f)) //reduce the max roof so that we ensure the ped is under the roof
					bInsideVehicleZ = true;

				if (bInsideVehicleZ)
				{
					Vector3 vVehiclePos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

					Vector3 vPedPosNoZ = vPedPos; 
					vPedPosNoZ.z = vVehiclePos.z;

					const spdAABB& trainLocalBB = GetBaseModelInfo()->GetBoundingBox();

					//Need to look at a scaled bounding box here - so we don't look at the edges of a train car and put the ped on the roof incorrectly

					const float kfScaleValue = 0.65f;
					Vector3 vVehicleToMin = (VEC3V_TO_VECTOR3(trainLocalBB.GetMin()) * kfScaleValue);
					Vector3 vVehicleToMax = (VEC3V_TO_VECTOR3(trainLocalBB.GetMax()) * kfScaleValue);
					spdAABB reducedbb(VECTOR3_TO_VEC3V(vVehicleToMin), VECTOR3_TO_VEC3V(vVehicleToMax));

					spdOrientedBB trainReducedOrientedBB(reducedbb, GetMatrix());
					bool bTrainReducedOrientedBBContainsPoint = trainReducedOrientedBB.ContainsPoint(VECTOR3_TO_VEC3V(vPedPosNoZ * -1.f));

					//-----

					//trainDebugf2("CTrain::CorrectLocalPlayerInsideTrain local player is standing on/in this traincar -- [%p][%s][%s] -- bTrainReducedOrientedBBContainsPoint[%d] vPedPos[%f %f %f]",this,GetModelName(),GetNetworkObject() ? GetNetworkObject()->GetLogName() : "",bTrainReducedOrientedBBContainsPoint,vPedPos.x,vPedPos.y,vPedPos.z);
					//grcDebugDraw::Sphere(vPedPos,0.5f,bTrainReducedOrientedBBContainsPoint ? Color_green : Color_pink,false);
					//grcDebugDraw::BoxOriented(reducedbb.GetMin(),reducedbb.GetMax(),GetMatrix(),bTrainReducedOrientedBBContainsPoint ? Color_green : Color_pink,false);

					//-----

					if (bTrainReducedOrientedBBContainsPoint)
					{
						trainWarningf("CTrain::CorrectLocalPlayerInsideTrain local player is standing in this traincar -- [%p][%s][%s] -- bTrainReducedOrientedBBContainsPoint[%d] vPedPos[%f %f %f] -- teleport to roof",this,GetModelName(),GetNetworkObject() ? GetNetworkObject()->GetLogName() : "",bTrainReducedOrientedBBContainsPoint,vPedPos.x,vPedPos.y,vPedPos.z);

						vPedPos.z = vehicleBox.GetMax().GetZf(); //put the ped on the roof

						pPlayerPed->Teleport(vPedPos,pPlayerPed->GetCurrentHeading());
					}
				}
			}
		}
	}
}

ePhysicsResult CTrain::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	if( m_nTrainFlags.bCreatedAsScriptVehicle || m_trackIndex == -1 )
	{
		ePhysicsResult returnValue = CVehicle::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);
		
		//Set the last matrix.
		m_inactiveLastMatrix.Set(MAT34V_TO_MATRIX34(GetMatrix()));
		
		// Still need to update the last matrix (as in UpdatePosition) to allow scripts to arbitrarily
		// position train carriages safely.
		CPhysics::GetSimulator()->SetLastInstanceMatrix(GetCurrentPhysicsInst(), RCC_MAT34V(m_inactiveLastMatrix));
		return returnValue;
	}

	DEV_BREAK_IF_FOCUS(CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), this);
	DEV_BREAK_ON_PROXIMITY(CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(GetTransform().GetPosition()));

	// Update position is where m_updateLODState is updated (which controls ShouldDoFullUpdate()).
#if __BANK
    if(sm_bProcessWithPhysics)
	{
#endif
        ProcessTrainControl(fTimeStep);
#if __BANK
	}
#endif


#if GTA_REPLAY
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		UpdatePosition(fTimeStep);
	}

	if (NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene())
	{
		CorrectLocalPlayerInsideTrain();
	}

	const bool doVehProcessPhysics = ShouldDoFullUpdate();
	if(doVehProcessPhysics)
	{
		return CVehicle::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);
	}
	else
	{
		return PHYSICS_DONE;
	}
}

void CTrain::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion)
{
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	// Disable the COM offset
	GetVehicleFragInst()->GetArchetype()->GetBound()->SetCGOffset(Vec3V(V_ZERO_WONE));
	phCollider *pCollider = GetCollider();
	if(pCollider)
		pCollider->SetColliderMatrixFromInstance();

	// we can't blow up cars controlled by another machine
	// but we still have to change their status to wrecked
	// so the car doesn't blow up if we take control of an
	// already blown up car
	if (IsNetworkClone())
	{
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);

		// knock bits off the car
		GetVehicleDamage()->BlowUpCarParts(pCulprit);

		// Break lights, windows and sirens
		GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

		m_nPhysicalFlags.bRenderScorched = TRUE;  
		SetTimeOfDestruction();
		SetHealth(0.0f, true);	
		SetIsWrecked();

		// Switch off the engine. (For sound purposes)
		this->SwitchEngineOff(false);
		this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
		this->m_nVehicleFlags.bLightsOn = FALSE;
		this->TurnSirenOn(FALSE);
		this->m_nVehicleFlags.bApproachingJunctionWithSiren = FALSE;
		this->m_nVehicleFlags.bPreviousApproachingJunctionWithSiren = FALSE;

		// set the engine temp super high so we get nice sounds as the chassis cools down
		m_EngineTemperature = MAX_ENGINE_TEMPERATURE;

		g_decalMan.Remove(this);

		return;
	}

	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	//	If the flag is set for don't damage this car (usually during a cutscene)
	}

	if (NetworkUtils::IsNetworkCloneOrMigrating(this))
	{
		// the vehicle is migrating. Create a weapon damage event to blow up the vehicle, which will be sent to the new owner. If the migration fails
		// then the vehicle will be blown up a little later.
		CBlowUpVehicleEvent::Trigger(*this, pCulprit, bAddExplosion, weaponHash, bDelayedExplosion);
		return;
	}

	//Total damage done for the damage trackers
	float totalDamage = GetHealth() + m_VehicleDamage.GetEngineHealth() + m_VehicleDamage.GetPetrolTankHealth();
	for(s32 i=0; i<GetNumWheels(); i++)
	{
		totalDamage += m_VehicleDamage.GetTyreHealth(i);
		totalDamage += m_VehicleDamage.GetSuspensionHealth(i);
	}
	totalDamage = totalDamage > 0.0f ? totalDamage : 1000.0f;

	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->HavocCaused += HAVOC_BLOWUPCAR;
	}

	SetIsWrecked();

	SetWeaponDamageInfo(pCulprit, weaponHash, fwTimer::GetTimeInMilliseconds());

	//Set the destruction information.
;
	m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off

	SetHealth(0.0f);	// Make sure this happens before AddExplosion or it will blow up twice

	//	Vector3	Temp = GetPosition();

	KillPedsInVehicle(pCulprit, weaponHash);
	KillPedsGettingInVehicle(pCulprit);

	// knock bits off the car
	GetVehicleDamage()->BlowUpCarParts(pCulprit);

	// Break lights, windows and sirens
	GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;
	this->TurnSirenOn(FALSE);
	this->m_nVehicleFlags.bApproachingJunctionWithSiren = FALSE;
	this->m_nVehicleFlags.bPreviousApproachingJunctionWithSiren = FALSE;

	// set the engine temp super high so we get nice sounds as the chassis cools down
	m_EngineTemperature = MAX_ENGINE_TEMPERATURE;

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
	}

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);

	//If the engine explodes then stop the train
	if(m_nTrainFlags.bEngine || m_nTrainFlags.bCaboose)
		ChangeTrainState(TS_Destroyed);

	NetworkInterface::ForceHealthNodeUpdate(*this);
	m_nVehicleFlags.bBlownUp = true;
}

void CTrain::SetThreatened()
{
	m_fTimeSinceThreatened = 0.0f;
}

///////////////////////////////////////////////////////////////////////////
// Has the entire train been updated, if so we can clear the m_processedThisFrame flag for all parts of the train
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Determine if this train is far enough away to do a simplified update.
///////////////////////////////////////////////////////////////////////////
bool CTrain::ShouldDoFullUpdate()
{
	//Check the lod state.
	switch(m_updateLODState)
	{
		case FADING_IN:
		case NORMAL:
		case FADING_OUT:
		{
			return true;
		}
		case SIMPLIFIED:
		{
			return false;
		}
		default:
		{
			vehicleAssertf(false, "Unknown lod state: %d.", m_updateLODState);
			return false;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// Update our LOD state.
///////////////////////////////////////////////////////////////////////////
void CTrain::UpdateLODAndFadeState(const Vector3& worldPosNew)
{
	const Vector3 camToTrain = worldPosNew - camInterface::GetPos();

	TUNE_GROUP_FLOAT(TRAINS, FullUpdateDist, 250.0f, 0.0f, 1000.0f, 0.01f);

	bool wantsFullUpdate = (camToTrain.Mag() < FullUpdateDist);
	if (m_nVehicleFlags.bIsInTunnel && !GetIsInInterior())
	{
		//Should be in an interior - request a rescan next update - throttled
		if (GetPortalTracker())
		{
			u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
			if (wantsFullUpdate && (m_requestInteriorRescanTime < currentTime))
			{
				trainDebugf2("CTrain::UpdateLODandFadeState -- in tunnel and not interior requestrescannextupdate");
				GetPortalTracker()->RequestRescanNextUpdate();
				m_requestInteriorRescanTime = currentTime + 500;
			}
		}

		wantsFullUpdate = false;
	}

	TUNE_GROUP_INT(TRAINS, AlphaInDurationMS, 4000, 1, 10000, 10);
	TUNE_GROUP_INT(TRAINS, AlphaOutDurationMS, 4000, 1, 10000, 10);
	const s32 currentTimeMS = fwTimer::GetTimeInMilliseconds();
	switch(m_updateLODState)
	{
	case FADING_IN:
		//if(!GetIsVisible()){SetIsVisible(true);}

		// Check to see if we should fade out or go to normal update mode.
		if(!wantsFullUpdate)
		{
			// Start fading out.
			// Adjust the m_updateLODFadeStartTimeMS into the past so that we fade out
			// for the correct amount of time and from the correct alpha level.
			m_updateLODState = FADING_OUT;
			const s32 alphaOutDurationMSFromCurrentLevel = static_cast<s32>((1.0f - m_updateLODCurrentAlpha) * static_cast<float>(AlphaOutDurationMS));
			m_updateLODFadeStartTimeMS = currentTimeMS - alphaOutDurationMSFromCurrentLevel;
		}
		else
		{
			// Check if we're done fading in.
			s32 fadeTimeElapsed = currentTimeMS - m_updateLODFadeStartTimeMS;
			if(fadeTimeElapsed >= AlphaInDurationMS)
			{
				// We're done fading in.
				m_updateLODState = NORMAL;
				m_updateLODCurrentAlpha = 1.0f;
			}
			else
			{
				// Determine our current alpha level.
				m_updateLODCurrentAlpha = 1.0f - (static_cast<float>(AlphaOutDurationMS - fadeTimeElapsed) / static_cast<float>(AlphaOutDurationMS));
			}
		}
		break;
	case NORMAL:
		m_updateLODCurrentAlpha = 1.0f;
		//if(!GetIsVisible()){SetIsVisible(true);}

		// Check if should start fading out.
		if(!wantsFullUpdate)
		{
			m_updateLODState = FADING_OUT;
			m_updateLODFadeStartTimeMS = currentTimeMS;
		}
		break;
	case FADING_OUT:
		//if(!GetIsVisible()){SetIsVisible(true);}

		// Check to see if we should fade in or go to minimal update mode.
		if(wantsFullUpdate)
		{
			// Start fading in.
			// Adjust the m_updateLODFadeStartTimeMS into the past so that we fade in
			// for the correct amount of time and from the correct alpha level.
			m_updateLODState = FADING_IN;
			const s32 alphaInDurationMSFromCurrentLevel = static_cast<s32>(m_updateLODCurrentAlpha * static_cast<float>(AlphaInDurationMS));
			m_updateLODFadeStartTimeMS = currentTimeMS - alphaInDurationMSFromCurrentLevel;
		}
		else
		{
			// Check if we're done fading out.
			s32 fadeTimeElapsed = currentTimeMS - m_updateLODFadeStartTimeMS;
			if(fadeTimeElapsed >= AlphaOutDurationMS)
			{
				// We're done fading out.
				m_updateLODState = SIMPLIFIED;
				m_updateLODCurrentAlpha = 0.0f;
			}
			else
			{
				// Determine our current alpha level.
				m_updateLODCurrentAlpha = static_cast<float>(AlphaOutDurationMS - fadeTimeElapsed) / static_cast<float>(AlphaOutDurationMS);
			}
		}
		break;
	case SIMPLIFIED:
		m_updateLODCurrentAlpha = 0.0f;
		//if(GetIsVisible()){SetIsVisible(false);}

		// Check to see if we should fade in.
		if(wantsFullUpdate)
		{
			m_updateLODState = FADING_IN;
			m_updateLODFadeStartTimeMS = currentTimeMS;
		}
		break;
	}

// DEBUG!! -AC, This is a pretty nasty hack to make the carriages alpha fade properly.
	// Make sure to update the rendering side of things.
// 	u8 alpha0to255 = static_cast<u8>(m_updateLODCurrentAlpha * 255.0f);
// 	if(alpha0to255 > 0)
// 	{
// 		// IK I have no idea why trains need to do things differently, but I'd like to
// 		// avoid a proliferation of different ways to over-ride alpha values. using
// 		// this one will do for now, until it goes away as a result of interior changes.
// 		SetAlpha(alpha0to255);
// 	}
// END DEBUG!!
}

void CTrain::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	DEV_BREAK_IF_FOCUS(CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this);
	DEV_BREAK_ON_PROXIMITY(CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity(), VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
	
	//Increase the time in the train state.
	m_fTimeInTrainState += fwTimer::GetTimeStep();

	if(m_nTrainFlags.bIsCutsceneControlled || m_nTrainFlags.bIsSyncedSceneControlled)
	{
		if(m_pIntelligence)
		{
			m_pIntelligence->GetTaskManager()->Process(fwTimer::GetTimeStep());
		}
	}

	if(m_nTrainFlags.bCreatedAsScriptVehicle)
	{
		static bool bVehProcControl = false;
		if(bVehProcControl)
		{
			CVehicle::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
		}
		goto processHD;
	}

#if __BANK
	if(!sm_bProcessWithPhysics)
	{
		ProcessTrainControl(fwTimer::GetTimeStep());
	}
#endif

processHD:
	// check for HD resource requests from any vehicles which may support HD
	if(GetVehicleLodState() != VLS_HD_NA)
	{
		Update_HD_Models();
	}
}


bool CTrain::WasProcessedThisTimeSlice() const
{
	return m_TimeSliceLastProcessed == CPhysics::GetTimeSliceId();
}
void CTrain::SetWasProcessedThisTimeSlice()
{
	m_TimeSliceLastProcessed = CPhysics::GetTimeSliceId();
}

///////////////////////////////////////////////////////////////////////////
// Do the main update of the vehicle.  Mainly updates the vehicles position
// and passengers.
//
// TODO: this is being called from ProcessPhysics(), but the majority of the code below does not need to be.
///////////////////////////////////////////////////////////////////////////
bool CTrain::ProcessTrainControl(float fTimeStep)
{ 
    TUNE_GROUP_BOOL(TRAINS, doProcessTrainControl, true);
    if(!doProcessTrainControl || WasProcessedThisTimeSlice() REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
    {
        return true;
	}
	SetWasProcessedThisTimeSlice();
#if __ASSERT
    float fOriginalTrackPos = m_trackPosFront;
#endif

	const bool doFullUpdate = ShouldDoFullUpdate();
	if(doFullUpdate)
	{
		// reset flag for has train warned peds of it's approach
		m_nVehicleFlags.bWarnedPeds = false;
	}

	bool bIsAtEndOfTrack = false;

	//Determine if we have a metrotrain - executes once
	if (!m_bArchetypeSet && IsArchetypeSet())
	{
		static atHashWithStringNotFinal s_hMetroTrain("metrotrain",0x33C9E158);
		if(GetArchetype()->GetModelNameHash() == s_hMetroTrain.GetHash())
			m_bMetroTrain = true;

		m_bArchetypeSet = true;
	}

	//Ensure this is set to true for SP - only set to false in MP (sometimes)
	bool bCheckDoors = true;

	if (NetworkInterface::IsGameInProgress())
	{
		if (m_bMetroTrain)
		{
			m_doorsForcedOpen |= IsLocalPlayerSeatedInAnyCarriage();
			if (m_doorsForcedOpen)
			{
				bCheckDoors = false;

				CTrain* pCarriage = this;
				if (pCarriage && pCarriage->ShouldDoFullUpdate())
				{
					for(pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
						pCarriage->ProcessDoors(1.0f);
				}
			}
		}
		else
		{
			//Don't check doors for freightrain and other trains in MP - keep the doors closed
			bCheckDoors = false;
		}
	}

	// If this is an engine it could be controlled by AI or the player.
	// If it is not an engine it simply follows the one in front of it.
	if(!m_nTrainFlags.bEngine)
	{
		// This isn't an engine carriage...
		// Note: trains are guaranteed to be updated from the engine towards the end.
		CTrain* pEngine = FindEngine(this);
		if(pEngine)
		{
			if (!pEngine->WasProcessedThisTimeSlice())
			{
				pEngine->ProcessTrainControl(fTimeStep);
			}
			// Just update our position ans speed with data from the engine.
			m_trackForwardSpeed = pEngine->m_trackForwardSpeed;
			m_trackForwardAccPortion = pEngine->m_trackForwardAccPortion;// Used for swinging the carriage forward and back later.
			m_trackPosFront = pEngine->m_trackPosFront + m_trackDistFromEngineFrontForCarriageFront;
			bIsAtEndOfTrack = FixupTrackPos(m_trackIndex, m_trackPosFront);
		}
		else
		{
			// Just keep rolling and slow down slowly.
// DEBUG!! -AC, Turn this off for now to aid debugging...
			static dev_bool bDisableSmoothing = false;
			if(!bDisableSmoothing)
				m_trackForwardSpeed *= powf(0.995f, fTimeStep);
// END DEBUG!!
			m_trackForwardAccPortion = 0.0f;// Used for swinging the carriage forward and back later.
			m_trackPosFront += m_trackForwardSpeed * fTimeStep;
			bIsAtEndOfTrack = FixupTrackPos(m_trackIndex, m_trackPosFront);
		}
	}
	else
	{
		// This is an engine carriage.
		TUNE_GROUP_BOOL(TRAINS, doProcessControl_EnginesProcessPassengersAndDoors, true);

		// m_trackForwardSpeed is the speed along the positive direction of the track.
		// We are interested in the speed in the direction of the train however.
		const float dirForwardSpeedCurrent	= (m_nTrainFlags.bDirectionTrackForward)?(m_trackForwardSpeed):(-m_trackForwardSpeed);
		float dirForwardSpeedDesired		= GetCarriageCruiseSpeed(this);

		m_fTimeSinceThreatened += fTimeStep;
		m_fTimeSinceThreatened = Min(m_fTimeSinceThreatened, 1000.0f);


		bool bIsDriverThreatened = (m_fTimeSinceThreatened < sm_fMinSecondsToConsiderBeingThreatened);

		bool bIsDriverAlive = NetworkInterface::IsGameInProgress() ? true : false; //MP default to bIsDriverAlive = true because there might not be a driver in MP and we still want the trains to stop
		CPed* pDriverPed = GetDriver();
		if (pDriverPed)
		{
			bIsDriverAlive = !pDriverPed->IsDead();
		}

		bool bLocalPlayerIsOnTrain = IsLocalPlayerSeatedInAnyCarriage() || IsLocalPlayerSeatedOnThisEnginesTrain() || IsLocalPlayerStandingInThisTrain();

		bool allowStoppingAtStations =
			(CGameWorld::FindLocalPlayerWanted()->m_WantedLevel <= WANTED_LEVEL2) || // Only give the player the chance to escape on a train if his wanted level is low.
			bLocalPlayerIsOnTrain || // Make sure to stop if the player is on board (so he can get off).
			GetIsMissionTrain() || // Mission trains always stop.
			!IsLoopedTrack(m_trackIndex);// Pingpong tracks MUST stop before heading the other direction.
 
		if ((bIsDriverThreatened || !bIsDriverAlive) && !bLocalPlayerIsOnTrain && !GetIsMissionTrain())
		{
			allowStoppingAtStations = false;
		}

		if (CTrain::DoesLocalPlayerDisableAmbientTrainStops() && !bLocalPlayerIsOnTrain && !GetIsMissionTrain())
		{
			allowStoppingAtStations = false;
		}

		bool allowLeavingFromStations = true;
		if ( bLocalPlayerIsOnTrain && (CGameWorld::FindLocalPlayerWanted()->m_WantedLevel > WANTED_LEVEL2 || bIsDriverThreatened) && !GetIsMissionTrain())
		{
			// If the player is on the train, don't leave if his wanted level is > 2. Also don't leave if the driver is threatened. But mission trains can always go.
			allowLeavingFromStations = false;
		}
		

		float	stationBrakingMaxSpeed			= 0.0f;
//		bool	bPlatformSideForClosestStation	= false;
		int	iPlatformSidesForClosestStation	= 0;
		GetStationRelativeInfo(m_currentStation, m_nextStation, m_travelDistToNextStation, stationBrakingMaxSpeed, iPlatformSidesForClosestStation);
		Assert(m_nextStation >= 0);
		Assert(gTrainTracks[m_trackIndex].m_numStations == 0 || m_nextStation < gTrainTracks[m_trackIndex].m_numStations);
		Assert(stationBrakingMaxSpeed >= 0);
		
		//Check the train state.
		switch(m_nTrainState)
		{
			case TS_Moving:
			{
				//Ensure the train can stop for stations.
				if(!m_nTrainFlags.bStopForStations)
				{
					break;
				}
				
				//Ensure we allow stopping at stations.
				if(!allowStoppingAtStations)
				{
					break;
				}
				
				//Ensure this track is allowed to stop at stations.
				if(!StopsAtStations(m_trackIndex))
				{
					break;
				}
				
				//Ensure we have been moving for at least 3 seconds.
				//Otherwise, the train can get stuck at stations after trying to leave.
				if(m_fTimeInTrainState < 3.0f)
				{
					break;
				}
				
				//Set the desired speed.
				dirForwardSpeedDesired = stationBrakingMaxSpeed;
				
				//Set the station side.
				SetStationSideForEngineAndCarriages(iPlatformSidesForClosestStation);
				
				if (!IsNetworkClone())
				{
					//Check if we are within the next station's radius.
					if(m_travelDistToNextStation < sm_stationRadius)
					{
						//Announce that we are arriving at a station.
						if(GetVehicleAudioEntity() && GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
						{
							static_cast<audTrainAudioEntity*>(GetVehicleAudioEntity())->AnnounceTrainAtStation();
						}

						//Iterate over the carriages.
						for(CTrain* pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
						{
							//Note that we are arriving at the station.
							pCarriage->ChangeTrainState(TS_ArrivingAtStation);
						}
					}
				}

				break;
			}
			case TS_ArrivingAtStation:
			{
				//Set the desired speed.
				dirForwardSpeedDesired = stationBrakingMaxSpeed;
				
				//Check if we are considered stopped.
				if(Abs(dirForwardSpeedCurrent) < sm_speedAtWhichConsideredStopped)
				{
					//Iterate over the carriages.
					for(CTrain* pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
					{
						//Set the door flags.
						pCarriage->m_nTrainFlags.bStartedOpeningDoors = true;
						pCarriage->m_nTrainFlags.bStartedClosingDoors = false;
						
						//Open the doors.
						pCarriage->ChangeTrainState(TS_OpeningDoors);
					}
				}
				
				break;
			}
			case TS_OpeningDoors:
			{
				//Set the desired speed.
				dirForwardSpeedDesired = 0.0f;
				
				//Iterate over the carriages.
				bool bAreAllDoorsOpen = true;
				if (bCheckDoors)
				{
					CTrain* pCarriage = this;
					if (pCarriage && pCarriage->ShouldDoFullUpdate())
					{
						for(pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
						{
							//Open the doors.
							bool bAreDoorsOpen = pCarriage->ProcessDoors(1.0f);
							if(!bAreDoorsOpen)
							{
								bAreAllDoorsOpen = false;
							}
						}
					}

					//Provide an fallback time here if we are taking forever in the door opening state to ensure the train keeps movin' along.
					//I have seen this happen when the engine was visible, but the 2nd car wasn't - another issue - but this led to the train remaining in opening_doors forever.
					if(m_fTimeInTrainState > STOPATSTATIONDURATION_MAXTIMEDOORSOPENINGCLOSING)
					{
						bAreAllDoorsOpen = true;
						trainWarningf("TS_OpeningDoors exceeded maximum time opening doors");
					}
				}

				//Check if all doors are open.
				if(bAreAllDoorsOpen)
				{
					//Iterate over the carriages.
					for(CTrain* pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
					{
						//Note that we are at a station.
						pCarriage->m_nTrainFlags.bAtStation = true;
						
						//Set the passenger state.
						pCarriage->m_nPassengersState = PS_GetOff;
						
						//Move to the waiting state.
						pCarriage->ChangeTrainState(TS_WaitingAtStation);
					}
				}
				
				break;
			}
			case TS_WaitingAtStation:
			{
				//Set the desired speed.
				dirForwardSpeedDesired = 0.0f;
				
				if (!allowLeavingFromStations)
				{
					break;
				}

				if (!IsNetworkClone())
				{
					//Check if we have waited long enough.
					if(m_fTimeInTrainState > 30.0f || (bIsDriverThreatened && !IsLocalPlayerStandingInThisTrain()))
					{
						//Iterate over the carriages.
						for(CTrain* pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
						{
							//Set the door flags.
							pCarriage->m_nTrainFlags.bStartedOpeningDoors = false;
							pCarriage->m_nTrainFlags.bStartedClosingDoors = true;

							//Close the doors.
							pCarriage->ChangeTrainState(TS_ClosingDoors);
						}
					}
				}
				
				break;
			}
			case TS_ClosingDoors:
			{
				//Set the desired speed.
				dirForwardSpeedDesired = 0.0f;
				
				//Iterate over the carriages.
				bool bAreAllDoorsClosed = true;
				if (bCheckDoors)
				{
					CTrain* pCarriage = this;
					if (pCarriage && pCarriage->ShouldDoFullUpdate())
					{
						for(pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
						{
							//Close the doors.
							bool bAreDoorsClosed = pCarriage->ProcessDoors(0.0f);
							if(!bAreDoorsClosed)
							{
								bAreAllDoorsClosed = false;
							}
						}
					}
				
					//Provide an fallback time here if we are taking forever in the door closing state to ensure the train keeps movin' along.
					//I have seen this happen when the engine was visible, but the 2nd car wasn't - another issue - but this led to the train remaining in closing_doors forever.
					if(m_fTimeInTrainState > STOPATSTATIONDURATION_MAXTIMEDOORSOPENINGCLOSING)
					{
						bAreAllDoorsClosed = true;
						trainWarningf("TS_ClosingDoors exceeded maximum time closing doors");
					}
				}

				if (!IsNetworkClone())
				{
					//Check if all doors are closed.
					if(bAreAllDoorsClosed)
					{
						//Iterate over the carriages.
						for(CTrain* pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
						{
							//Note that we are at a station.
							pCarriage->m_nTrainFlags.bAtStation = false;

							//Set the passenger state.
							pCarriage->m_nPassengersState = PS_None;

							//Move to the leaving state.
							pCarriage->ChangeTrainState(TS_LeavingStation);
						}

						//Announce that we are leaving the station.
						if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
						{
							static_cast<audTrainAudioEntity*>(GetVehicleAudioEntity())->AnnounceTrainLeavingStation();
						}
					}
				}
				
				break;
			}
			case TS_LeavingStation:
			{
				//Set the desired speed.
				dirForwardSpeedDesired = 0.0f;
				
				//Check if we have waited long enough.
				if(m_fTimeInTrainState > 3.0f)
				{
					//Check if this is not a looped track.
					if(!IsLoopedTrack(m_trackIndex))
					{
						//Switch the track direction.
						m_nTrainFlags.bDirectionTrackForward = !m_nTrainFlags.bDirectionTrackForward;
					}
					
					//Iterate over the carriages.
					for(CTrain* pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
					{
						//Move to the moving state.
						pCarriage->ChangeTrainState(TS_Moving);
					}
				}
				
				break;
			}
			case TS_Destroyed:
			{
				dirForwardSpeedDesired = 0.0f;
				break;
			}
		}


		// if this carriage is at the front of a train, want to check for vehicles and peds in front of it
		if(m_nTrainFlags.bEngine)
		{
			PF_PUSH_TIMEBAR("Train scan for entities");

			if(dirForwardSpeedDesired > 0.0f && !bIsDriverThreatened && bIsDriverAlive)
			{
				ScanForEntitiesBlockingProgress(dirForwardSpeedDesired);
			}

			PF_POP_TIMEBAR();
		}
		
		// Use the gasPortion and brakePortion to affect the speed of the train.
		float newDirForwardSpeed = dirForwardSpeedCurrent;
		{
			const float dirForwardSpeedDeltaDesired	= dirForwardSpeedDesired - dirForwardSpeedCurrent;
			const float brakingAccMaxThisFrame = sm_maxDeceleration * fTimeStep;
			const float gasAccMaxThisFrame = sm_maxAcceleration * fTimeStep;

			float dirForwardAccThisFrame = 0.0f;
			if(dirForwardSpeedDeltaDesired < 0.0f)
			{
				dirForwardAccThisFrame = rage::Clamp(dirForwardSpeedDeltaDesired, -brakingAccMaxThisFrame, 0.0f);
			}
			else
			{
				dirForwardAccThisFrame = rage::Clamp(dirForwardSpeedDeltaDesired, 0.0f, gasAccMaxThisFrame);
			}

			newDirForwardSpeed += dirForwardAccThisFrame;

			const float dirForwardAccPortion = dirForwardAccThisFrame / rage::Max(brakingAccMaxThisFrame, gasAccMaxThisFrame);// Used for swinging the carriage forward and back.
			m_trackForwardAccPortion = (m_nTrainFlags.bDirectionTrackForward)?(dirForwardAccPortion):(-dirForwardAccPortion);
		}

		// Apply the new speed.
		if (!IsNetworkClone())
			m_trackForwardSpeed = (m_nTrainFlags.bDirectionTrackForward)?(newDirForwardSpeed):(-newDirForwardSpeed);
		
		m_trackPosFront += m_trackForwardSpeed * fTimeStep;
		bIsAtEndOfTrack = FixupTrackPos(m_trackIndex, m_trackPosFront);

		//*******************************************************************************
		// If we've reached the end of the line, and are still stopping - then reduce
		// the speed to zero otherwise we may never be able to reach a standstill.

		if(bIsAtEndOfTrack && !IsLoopedTrack(m_trackIndex))
		{
			m_trackForwardSpeed = 0.0f;
		}
	}

	/////////////////////////////////////////////////////////////////////////
	// Handle the trains interactions with peds and other objects.

	/////////////////////////////////////////////////////////////////////////
	if(doFullUpdate)
	{
		//	Process getting random passengers on/off the train
		ProcessPassengers();

		// If peds haven't been warned of this train's approach by now - do it!

		//TODO
		/*
		CVehMission *pUberMission = GetIntelligence()->FindUberMissionForCar();
		if(!m_nVehicleFlags.bWarnedPeds)
		{
			CCarAI::ScanForPedDanger(this, pUberMission);
		}
		*/


		// See if we need to make a Clunk clunk sound.
		if(!CarriagesAreSuspended() && (m_oldCurrentNode != m_currentNode))
		{
			if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
			{
				static_cast<audTrainAudioEntity*>(GetVehicleAudioEntity())->TrainHasMovedTrackNode();
			}
		}
		m_oldCurrentNode = m_currentNode;

		// Shake pad as it changes link
		// This needs to be queued from audio trigger
		const bool bLocalPlayerOnThisCarriage = IsLocalPlayerSeatedInThisCarriage();
		const bool bLocalPlayerOnOtherCarriage = IsLocalPlayerSeatedInAnyCarriage();
		if((bLocalPlayerOnThisCarriage || bLocalPlayerOnOtherCarriage)
			&& m_iTimeOfSecondaryPadShake > 0
			&& m_iTimeOfSecondaryPadShake < fwTimer::GetTimeInMilliseconds())
		{
			CControlMgr::StartPlayerPadShakeByIntensity(iTrainPadShakeDuration,bLocalPlayerOnThisCarriage ? fTrainPadShakeIntensityPlayersCarriage : fTrainPadShakeIntensityOtherCarriage);
			m_iTimeOfSecondaryPadShake = 0;
		}
	}

#if __ASSERT
	// Make sure we haven't moved too far.
	// (JB: unless we've just looped around at the end of a circular track, in which case it might look like we've moved an inordinately large distance)
	// (BW: can't rely on bIsAtEndOfTrack to determine if we've looped if m_trackPosFront was assigned rather than incremented,
	//      as in the case of non-engine carriages that take their position from the engine)
	if(m_nTrainFlags.bEngine)
	{
		float dist = rage::Abs(fOriginalTrackPos-m_trackPosFront);
		if (IsLoopedTrack(m_trackIndex))
		{
			const float totalTrackLength = gTrainTracks[m_trackIndex].GetTotalTrackLength();
			if (dist > (totalTrackLength * 0.5f))
			{
				// Use shortest distance
				dist = totalTrackLength - dist;
			}
		}

		Assertf( dist < 10.0f || (bIsAtEndOfTrack && IsLoopedTrack(m_trackIndex)), "dist = %.2f", dist );
	}
#endif

	if (m_nTrainFlags.bEngine && doFullUpdate)
	{
		ProcessSirenAndHorn(true);
	}

	return true;
}

static dev_float sfTrainVelocityLimitSq = 80.0f * 80.0f;
bool CTrain::UpdatePosition(float fTimeStep)
{
	// Figure out the placement and orientation of the carriage.  Also, determine what
	// type of update the train needs (full or simplified).
	Vector3 worldPosNew(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
	Matrix34 mat = MAT34V_TO_MATRIX34(GetMatrix());
	Vector3 tempUp		(mat.c);
	Vector3 tempForward	(mat.b);
	Vector3 tempRight	(mat.a);
	if(m_trackIndex >= 0)
	{
		// Update the current node and get our world position and forward direction for the carriage.
		m_currentNode = FindCurrentNode(m_trackIndex, m_trackPosFront, m_nTrainFlags.bDirectionTrackForward);

		const float carriageLength = GetBoundingBoxMax().y - GetBoundingBoxMin().y;
		const bool flipModelDir = (m_trainConfigIndex>=0) && (m_trainConfigCarriageIndex>=0) && gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages[m_trainConfigCarriageIndex].m_flipModelDir;
		bool bIsTunnel = false;
		CalcWorldPosAndForward(worldPosNew, tempForward, m_trackIndex, m_trackPosFront, m_nTrainFlags.bDirectionTrackForward, m_currentNode, carriageLength, CarriagesAreSuspended(), flipModelDir, bIsTunnel);
		m_nVehicleFlags.bIsInTunnel = bIsTunnel;

#if __ASSERT && 0
	if(!IsNetworkClone() && !GetIsMissionTrain() && m_currentNode > 0 && m_currentNode < gTrainTracks[m_trackIndex].m_numNodes-1)
	{
		if(m_uFrameCreated != fwTimer::GetFrameCount()) // when we create the train it can jump around a bit the frame we create it, before it settles down. This disables the assert on the frame the train was created
		{
			const float fMag2 = (worldPosNew - mat.d).Mag2();
			if( fMag2 > 25.0f * 25.0f )
			{
				Assertf( false, "Train 0x%p has warped by %.2f; m_trackIndex %i, m_currentNode %i, mat.d (%.1f, %.1f, %.1f), worldPosNew (%.1f, %.1f, %.1f)", this, Sqrtf(fMag2), m_trackIndex, m_currentNode, mat.d.x, mat.d.y, mat.d.z, worldPosNew.x, worldPosNew.y, worldPosNew.z );
			}
		}
	}
#endif
		UpdateLODAndFadeState(worldPosNew);
	}

	// DEBUG!! -AC, This should really assert if there is no pivot bone, but not all the models are
	// properly set up with the correct bone ID yet (only the cable car is)...
	if(CarriagesAreSuspended())
	{
		m_trackSteepness = rage::Atan2f(tempForward.z, tempForward.XYMag());
		tempForward.z = 0.0f;
		tempForward.Normalize();

		// Get the pivot bones matrix so we can hang the cart from it.
		CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();
		Assertf(pModelInfo, "Error getting the model info for the train carriage.");
		s32 boneId = pModelInfo->GetBoneIndex(VEH_TRANSMISSION_F);
		if(boneId< 0)
		{
			//Warningf("Suspended carriages must have a pivot bone (VEH_TRANSMISSION_F).");
		}
		else
		{
			Assertf(GetSkeletonData().GetParentIndex(boneId) == 0,"Suspended carriages expect the pivot bone to be a child of the root.");
			const Matrix34 &boneMat = GetLocalMtx(boneId);

			// Make the cable car hang from the pivot bone.
			worldPosNew -= boneMat.d;
		}
	}
	// END DEBUG!!
	TUNE_GROUP_FLOAT(TRAINS, offsetExtra, 0.0f, 0.0f, 100.0f, 0.01f);

	float fVertCarrigeOffset = GetCarriageVerticalOffsetFromTrack(); 
	worldPosNew.z += (fVertCarrigeOffset + offsetExtra);

#if __DEV
	if(	worldPosNew.x < WORLDLIMITS_XMIN || worldPosNew.x > WORLDLIMITS_XMAX ||
		worldPosNew.y < WORLDLIMITS_YMIN || worldPosNew.y > WORLDLIMITS_YMAX)
	{
		Displayf("Train out of bounds: %f %f %f\n", worldPosNew.x, worldPosNew.y, worldPosNew.z);
		Displayf("m_currentNode:%d m_trackIndex:%d trackPos:%f\n", m_currentNode, m_trackIndex, m_trackPosFront);

		Assertf(0, "Train processcontrol put train outside world. Check output window for details (Obbe)");
	}
#endif // __DEV

	// Get the un-modified basis vectors.
	tempUp = Vector3(0.0f, 0.0f, 1.0f);
	tempRight = CrossProduct(tempForward, tempUp);
	tempRight.Normalize();
	tempUp = CrossProduct(tempRight, tempForward);

	// Fill in the positioning matrix.
	Matrix34	worldMatNew;
	worldMatNew.a.x = tempRight.x;		worldMatNew.a.y = tempRight.y;		worldMatNew.a.z = tempRight.z;
	worldMatNew.c.x = tempUp.x;			worldMatNew.c.y = tempUp.y;			worldMatNew.c.z = tempUp.z;
	worldMatNew.b.x = tempForward.x;	worldMatNew.b.y = tempForward.y;	worldMatNew.b.z = tempForward.z;
	worldMatNew.d = worldPosNew;

	// Do Swinging And Derails.
	if(m_trackIndex >= 0)
	{
		// Modify the matrix if it is derailed or swinging.
		if(m_nTrainFlags.bRenderAsDerailed)
		{
			u16 usedRandomSeed = GetRandomSeed();

			worldMatNew.d.z -= 0.1f;
			worldMatNew.RotateLocalZ((((s32)(usedRandomSeed & 31)) - 16) * 0.012f);
			worldMatNew.RotateLocalX((((s32)(usedRandomSeed & 127)) - 64) * 0.001f);
		}
		else if(sm_swingCarriages && CarriagesAreSuspended() && CarriagesSwing())
		{
			float cruiseSpeedPortion = rage::Abs(m_trackForwardSpeed) / GetCarriageCruiseSpeed(this);

			// Do side to side swinging.
			if(sm_swingCarriagesSideToSide)
			{
				const float timePos = sm_swingSideToSideRate * (fwTimer::GetTimeInMilliseconds()/1000.0f);
				const float noiseValAtTimePos = sm_swingSideToSideNoiseStrength * rage::PerlinNoise::RawNoise(timePos * sm_swingSideToSideNoiseFrequency);
				const float sideToSideSwingAng = (sm_swingSideToSideAngMax * cruiseSpeedPortion * rage::Sinf(timePos + noiseValAtTimePos));
				worldMatNew.RotateLocalY(sideToSideSwingAng);
			}

			// Do forward and back swinging.
			if(sm_swingCarriagesFrontToBack)
			{
				// Apply a smoothed version of the swing amount.
				const float frontBackSwingAngDesired = sm_swingFrontToBackAngMax * -m_trackForwardAccPortion * cruiseSpeedPortion;
				const float frontBackSwingAngDeltaDesired = frontBackSwingAngDesired - m_frontBackSwingAngPrev;
				const float frontBackSwingAngDeltaDesiredPerSec = frontBackSwingAngDeltaDesired / fTimeStep;
				const float frontBackSwingAngDeltaPerSecToApply = rage::Clamp(frontBackSwingAngDeltaDesiredPerSec, -sm_swingFrontToBackAngDeltaMaxPerSec, sm_swingFrontToBackAngDeltaMaxPerSec);
				const float frontBackSwingAngDeltaToApply = frontBackSwingAngDeltaPerSecToApply * fTimeStep;
				const float	frontBackSwingAng = m_frontBackSwingAngPrev + frontBackSwingAngDeltaToApply;

				worldMatNew.RotateLocalX(frontBackSwingAng);

				// Store the current swing amount (to allow smoothing later).
				m_frontBackSwingAngPrev = frontBackSwingAng;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////
	// Position the train in the world.
	/////////////////////////////////////////////////////////////////////////

	const Mat34V worldMatOld = GetTransform().GetMatrix();

	// only activate use of last matrix now that we've set it to non-zero position
	m_inactiveLastMatrix.Set(MAT34V_TO_MATRIX34(GetMatrix()));
    SetMatrix(worldMatNew);

	CPhysics::GetSimulator()->SetLastInstanceMatrix(GetCurrentPhysicsInst(), worldMatOld);
	
	//Calculate the inverse time step.
	ScalarV scInvTimeStep = InvertSafe(ScalarVFromF32(fTimeStep), ScalarV(V_ZERO));
	
	//Calculate and set the velocity from the matrices.
	Vec3V vVelocity = CalculateVelocityFromMatrices(worldMatOld, RCC_MAT34V(worldMatNew), scInvTimeStep);
	m_inactiveMoveSpeed = VEC3V_TO_VECTOR3(vVelocity);
	
	//Velocity should just be limited to sfTrainVelocityLimitSq
	if(m_inactiveMoveSpeed.Mag2() > sfTrainVelocityLimitSq)
	{
		//Clamp the velocity.
		m_inactiveMoveSpeed *= sfTrainVelocityLimitSq / m_inactiveMoveSpeed.Mag2();
	}
	
	//Set the velocity.
	if (!IsNetworkClone())
		SetVelocity(m_inactiveMoveSpeed);
	
	//Calculate and set the angular velocity from the matrices.
	Vec3V vAngularVelocity = CalculateAngVelocityFromMatrices(worldMatOld, RCC_MAT34V(worldMatNew), scInvTimeStep);
	m_inactiveTurnSpeed = VEC3V_TO_VECTOR3(vAngularVelocity);


    //Anuglar velocity should just be limited to sfTrainVelocityLimitSq
    if(m_inactiveTurnSpeed.Mag2() > sfTrainVelocityLimitSq)
    {
        //Clamp the angular velocity.
		m_inactiveTurnSpeed *= sfTrainVelocityLimitSq / m_inactiveTurnSpeed.Mag2();
    }

	//Set the angular velocity.
	SetAngVelocity(m_inactiveTurnSpeed);

	// Make sure if we can't do proper interior audio occlusion, then we're at least silent if in a tunnel
	naEnvironmentGroup* environmentGroup = (naEnvironmentGroup*)GetAudioEnvironmentGroup();
	if(environmentGroup)
	{
		if(!GetInteriorLocation().IsValid() && m_nVehicleFlags.bIsInTunnel)
		{
			environmentGroup->SetForceFullyOccluded(true);
		}
		else
		{
			environmentGroup->SetForceFullyOccluded(false);
		}
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////
// Some useful query functions.
///////////////////////////////////////////////////////////////////////////
float	CTrain::PopulateTrainDist() const					{ return (m_trainConfigIndex>=0)?gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_populateTrainDist:0.0f;}
s32	CTrain::StopAtStationDurationNormal() const			{ return (m_trainConfigIndex>=0)?gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_stopAtStationDurationNormal:0; }
s32	CTrain::StopAtStationDurationPlayer() const			{ return (m_trainConfigIndex>=0)?gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_stopAtStationDurationPlayer:0; }
s32	CTrain::StopAtStationDurationPlayerWithGang() const	{ return (m_trainConfigIndex>=0)?gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_stopAtStationDurationPlayerWithGang:0; }
bool	CTrain::AnnouncesStations() const					{ return (m_trainConfigIndex>=0) && gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_announceStations; }
bool	CTrain::DoorsGiveWarningBeep() const				{ return (m_trainConfigIndex>=0) && gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_doorsBeep; }
bool	CTrain::CarriagesAreSuspended() const				{ return (m_trainConfigIndex>=0) && gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriagesHang;}
bool	CTrain::CarriagesSwing() const						{ return (m_trainConfigIndex>=0) && gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriagesSwing;}
bool	CTrain::IsEngine() const							{ return m_nTrainFlags.bEngine;}

///////////////////////////////////////////////////////////////////////////
// Determine if the network local player is on this carriage.
///////////////////////////////////////////////////////////////////////////
bool CTrain::IsLocalPlayerSeatedInThisCarriage() const
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	CVehicle * pPlayerTrain = pPlayerPed->GetPlayerInfo()->GetTrainPedIsInside();
	if(!pPlayerTrain && pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayerPed->GetMyVehicle())
		pPlayerTrain = pPlayerPed->GetMyVehicle();

	if(!pPlayerTrain || !pPlayerTrain->InheritsFromTrain())
	{
		return false;
	}

	return (this == pPlayerTrain);
}


///////////////////////////////////////////////////////////////////////////
// Determine if the network local player is on this or any attached carriage.
///////////////////////////////////////////////////////////////////////////
bool CTrain::IsLocalPlayerSeatedInAnyCarriage() const
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if(!pPlayerPed)
	{
		return false;
	}

	CVehicle * pPlayerTrain = pPlayerPed->GetPlayerInfo()->GetTrainPedIsInside();
	if(!pPlayerTrain && pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayerPed->GetMyVehicle())
		pPlayerTrain = pPlayerPed->GetMyVehicle();

	if(!pPlayerTrain || !pPlayerTrain->InheritsFromTrain())
	{
		return false;
	}

	const CTrain* p = this;

	// First search forwards
	while(p)
	{
		if(p == pPlayerTrain)
		{
			return true;
		}
		p = p->GetLinkedToForward();
	}

	p = GetLinkedToBackward();

	// Now work backwards
	while(p)
	{
		if(p == pPlayerTrain)
		{
			return true;
		}
		p = p->GetLinkedToBackward();
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////
// Determine where the carriage in relation to the station nodes.
//
// This function will look at the coordinates of this train and work out
// what its maximum speed is to stop at the stations on the track.
///////////////////////////////////////////////////////////////////////////
void CTrain::GetStationRelativeInfo(s32& currentStation_out, s32& nextStation_out,	float& travelDistToNextStation_out, float& maxSpeedToApproachStation_out, int & iPlatformSideForNextStation_out) const
{
	if(!Verifyf(m_trackIndex >=0, "Invalid track index for train %p. Was this train initialised properly?", this))
	{
		return;
	}

	const float totalTrackLength = gTrainTracks[m_trackIndex].GetTotalTrackLength();

	// Calculate the travel distance to the next station.
	// This is used to recalculate the true current stations and also to determine
	// the max speed (we go slower if entering a station).
	s32 nextStationIndex = 0;
	float minTravelDistToNextStation = 100000.0f; // Large sentinel value.
	for(s32 stationIndex = 0; stationIndex < gTrainTracks[m_trackIndex].m_numStations; stationIndex++)
	{
		const float positionOnTrackFromEnd	= totalTrackLength - m_trackPosFront;
		const float stationDistFromStart	= gTrainTracks[m_trackIndex].m_aStationDistFromStart[stationIndex];
		const float stationDistFromEnd		= totalTrackLength - stationDistFromStart;

		float signedTrackDistToStation = stationDistFromStart - m_trackPosFront;
		float travelDistToStation = 100000.0f; // Large sentinel value.
		if(IsLoopedTrack(m_trackIndex))
		{
			// Looping train.
			if(m_nTrainFlags.bDirectionTrackForward)
			{
				if(signedTrackDistToStation > 0)
				{
					travelDistToStation = signedTrackDistToStation;
				}
				else
				{
					travelDistToStation = stationDistFromStart + positionOnTrackFromEnd;
				}
			}
			else
			{
				if(signedTrackDistToStation > 0)
				{
					travelDistToStation =  m_trackPosFront + stationDistFromEnd;
				}
				else
				{
					travelDistToStation = -signedTrackDistToStation;
				}
			}
		}
		else
		{
			// Pingponging train.
			if(m_nTrainFlags.bDirectionTrackForward)
			{
				if(signedTrackDistToStation > 0)
				{
					travelDistToStation = signedTrackDistToStation;
				}
				else
				{
					travelDistToStation = positionOnTrackFromEnd + stationDistFromEnd;
				}
			}
			else
			{
				if(signedTrackDistToStation > 0)
				{
					travelDistToStation = m_trackPosFront + stationDistFromStart;
				}
				else
				{
					travelDistToStation = -signedTrackDistToStation;
				}
			}
		}
		Assert(travelDistToStation != 100000.0f);
		Assert(travelDistToStation >= 0.0f);

		if(travelDistToStation < minTravelDistToNextStation)
		{
			minTravelDistToNextStation = travelDistToStation;
			nextStationIndex = stationIndex;
		}
	}
	Assert(gTrainTracks[m_trackIndex].m_numStations == 0 || minTravelDistToNextStation != 100000.0f);
	Assert(minTravelDistToNextStation >= 0.0f);
	Assert(nextStationIndex >= 0);
	Assert(gTrainTracks[m_trackIndex].m_numStations == 0 || nextStationIndex < gTrainTracks[m_trackIndex].m_numStations);

	const u32 iGondolasHash = ATSTRINGHASH("gondolas", 0x0f582de0a);
	if(gTrainTracks[m_trackIndex].m_trainConfigGroupNameHash == iGondolasHash)
	{
		iPlatformSideForNextStation_out = CTrainTrack::Both;
	}
	else
	{
		int iSide = gTrainTracks[m_trackIndex].m_aStationSide[nextStationIndex];

		// If heading the opposite away along a track, then flip the door sides accordingly
		if(!m_nTrainFlags.bDirectionTrackForward)
		{
			if(iSide == CTrainTrack::Left)
				iSide = CTrainTrack::Right;
			else if(iSide == CTrainTrack::Right)
				iSide = CTrainTrack::Left;
		}
		iPlatformSideForNextStation_out = iSide;
	}

	// Set some of the output vals.
	travelDistToNextStation_out = minTravelDistToNextStation;

	nextStation_out = nextStationIndex;

	// Re-calculate what the current station is.
	if(IsLoopedTrack(m_trackIndex))
	{
		// Looping train.
		if(m_nTrainFlags.bDirectionTrackForward)
		{
			currentStation_out = (nextStation_out - 1);
			if(currentStation_out < 0)
			{
				currentStation_out = (gTrainTracks[m_trackIndex].m_numStations - 1);
			}
		}
		else
		{
			currentStation_out = (nextStation_out + 1);
			if(currentStation_out == gTrainTracks[m_trackIndex].m_numStations)
			{
				currentStation_out = 0;
			}
		}
	}
	else
	{
		// Pingponging train.
		if(m_nTrainFlags.bDirectionTrackForward)
		{
			currentStation_out = (nextStation_out - 1);
			if(currentStation_out < 0)
			{
				currentStation_out = 1;
			}
		}
		else
		{
			currentStation_out = (nextStation_out + 1);
			if(currentStation_out == gTrainTracks[m_trackIndex].m_numStations)
			{
				currentStation_out = (gTrainTracks[m_trackIndex].m_numStations - 1);
			}
		}
	}
	Assertf(currentStation_out >= 0, "(m_trackIndex is %i)", m_trackIndex);
	Assert(gTrainTracks[m_trackIndex].m_numStations == 0 || currentStation_out < gTrainTracks[m_trackIndex].m_numStations);

	maxSpeedToApproachStation_out = static_cast<float>(gTrainTracks[m_trackIndex].m_maxSpeed);
	if(StopsAtStations(m_trackIndex) && (minTravelDistToNextStation < gTrainTracks[m_trackIndex].m_breakDist))
	{
		//Artificially increase the min travel distance to the next station when calculating brake lerp,
		//to handle cases where we approach too slowly and get stuck creeping ever so slowly to the 'stopped' threshold.
		static dev_float s_fExtraDistance = 0.25f;
		float fMinTravelDistToNextStationModified = minTravelDistToNextStation + s_fExtraDistance;
		fMinTravelDistToNextStationModified = Min(fMinTravelDistToNextStationModified, (float)gTrainTracks[m_trackIndex].m_breakDist);

		float brakeDistClosenessPortion = ((gTrainTracks[m_trackIndex].m_breakDist - fMinTravelDistToNextStationModified) / gTrainTracks[m_trackIndex].m_breakDist);
		if(minTravelDistToNextStation < 0.03f){brakeDistClosenessPortion = 1.0f;}

		maxSpeedToApproachStation_out = gTrainTracks[m_trackIndex].m_maxSpeed * (1.0f - brakeDistClosenessPortion);
	}
}


///////////////////////////////////////////////////////////////////////////
// Set which side the station will be on (used to determine which set of
// doors to open.
///////////////////////////////////////////////////////////////////////////
void CTrain::SetStationSideForEngineAndCarriages(int iSides) //bool bSide)
{
	CTrain* pCarriage = FindEngine(this);
	while(pCarriage)
	{
		pCarriage->m_nTrainFlags.iStationPlatformSides = iSides;
		pCarriage = pCarriage->GetLinkedToBackward();
	}
}


///////////////////////////////////////////////////////////////////////////
// Init the physical representation of the carriage.
///////////////////////////////////////////////////////////////////////////
int CTrain::InitPhys()
{
	CVehicle::InitPhys();
	PHLEVEL->SetInactiveCollidesAgainstInactive(GetCurrentPhysicsInst()->GetLevelIndex(), true);

	// Need to explicitly tell the physics engine to query the external velocity (phInst::GetExternallControlledVelocity)
	if(GetCurrentPhysicsInst())
	{
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,true);

		PHLEVEL->ReserveInstLastMatrix(GetCurrentPhysicsInst());
	}
	return INIT_OK;
}


///////////////////////////////////////////////////////////////////////////
// Determine how fast this carriage is moving.
///////////////////////////////////////////////////////////////////////////
const Vector3& CTrain::GetVelocity() const
{
	return m_inactiveMoveSpeed;
}

const Vector3& CTrain::GetAngVelocity() const
{
	return m_inactiveTurnSpeed;
}

Mat34V_Out CTrain::GetMatrixPrePhysicsUpdate() const
{
	return RCC_MAT34V(m_inactiveLastMatrix);
}


///////////////////////////////////////////////////////////////////////////
// Init the carriage's doors.
///////////////////////////////////////////////////////////////////////////
dev_float sfTrainDoorSize = 0.7f;
void CTrain::InitDoors()
{
	CVehicle::InitDoors();

	m_pDoors = m_aTrainDoors;
	m_nNumDoors = 0;

	if(GetBoneIndex(VEH_DOOR_DSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_F, sfTrainDoorSize, 0.0f, CCarDoor::AXIS_SLIDE_Y);
        m_pDoors[m_nNumDoors].ClearFlag(CCarDoor::IS_ARTICULATED);// Force the doors to not be moved physically
		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_PSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_F, sfTrainDoorSize, 0.0f, CCarDoor::AXIS_SLIDE_Y);
        m_pDoors[m_nNumDoors].ClearFlag(CCarDoor::IS_ARTICULATED);// Force the doors to not be moved physically
		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_DSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_R, -sfTrainDoorSize, 0.0f, CCarDoor::AXIS_SLIDE_Y);
        m_pDoors[m_nNumDoors].ClearFlag(CCarDoor::IS_ARTICULATED);// Force the doors to not be moved physically
		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_PSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_R, -sfTrainDoorSize, 0.0f, CCarDoor::AXIS_SLIDE_Y);
        m_pDoors[m_nNumDoors].ClearFlag(CCarDoor::IS_ARTICULATED);// Force the doors to not be moved physically
		m_nNumDoors++;
	}
}

void CTrain::InitCompositeBound()
{
	CVehicle::InitCompositeBound();

	// If we've got an interior then disbale all non-interior parts collision with peds and ragdolls
	int iInteriorBoneIndex = GetBoneIndex(TRAIN_INTERIOR);
	int iInteriorGroup = -1;
	if(iInteriorBoneIndex > -1)
	{
		iInteriorGroup = GetVehicleFragInst()->GetGroupFromBoneIndex(iInteriorBoneIndex);
	}

	if(iInteriorGroup > -1)
	{
		// We've got an interior
		// Sort out the composite include flags
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());

		if(pBoundComp->GetTypeAndIncludeFlags())
		{
			for(int i = 0; i < pBoundComp->GetNumBounds(); i++)
			{
				if(GetVehicleFragInst()->GetTypePhysics()->GetAllChildren()[i]->GetOwnerGroupPointerIndex() != iInteriorGroup)
				{
					// This is not an interior component so disable ped collision
					int iIncludeFlags = pBoundComp->GetIncludeFlags(i);
					iIncludeFlags &= ~(ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE);
					pBoundComp->SetIncludeFlags(i,iIncludeFlags);
				}
			}
		}
	}

	if(MI_CAR_FREIGHTCAR2.IsValid() && GetModelIndex() == MI_CAR_FREIGHTCAR2) //Having to use a model index check here as FLAG_EXTRAS_SCRIPT is also on tankercar and I dont want to break it and this train is the only one with more than one extra so needs special logic
	{
		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());
		if(pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_SCRIPT))
		{
			// turn on collisions with extra's that have frag children / bounds
			for(int nExtra=VEH_EXTRA_1; nExtra <= VEH_LAST_EXTRA; nExtra++)
			{
				if(GetBoneIndex((eHierarchyId)nExtra)!=-1)
				{
					int nGroup = GetVehicleFragInst()->GetGroupFromBoneIndex(GetBoneIndex((eHierarchyId)nExtra));
					if(nGroup > -1)
					{				
						fragPhysicsLOD* physicsLOD = GetVehicleFragInst()->GetTypePhysics();
						fragTypeGroup* pGroup = physicsLOD->GetGroup(nGroup);
						int iChild = pGroup->GetChildFragmentIndex();

						for(int k = 0; k < pGroup->GetNumChildren(); k++)
						{
							if(GetIsExtraOn((eHierarchyId)nExtra))
							{
								pBoundComp->SetIncludeFlags(iChild+k, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
							}
							else
							{
								pBoundComp->SetIncludeFlags(iChild+k, 0);
							}
						}
					}
				}
			}
		}
	}

}

bool CTrain::CanPedGetOnTrain(const CPed& rPed) const
{
	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}
	
	//Ensure the ped is not injured.
	if(rPed.IsInjured())
	{
		return false;
	}
	
	//Ensure the ped is not a player.
	if(rPed.IsAPlayerPed())
	{
		return false;
	}
	
	//Ensure the ped is not law enforcement.
	if(rPed.IsLawEnforcementPed())
	{
		return false;
	}
	
	//Ensure the ped is not getting on the train.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_ON_TRAIN))
	{
		return false;
	}
	
	//Ensure the ped is not getting off the train.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_OFF_TRAIN))
	{
		return false;
	}
	
	//Ensure the ped is not riding a train.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_RIDE_TRAIN))
	{
		return false;
	}
	
	//Ensure the ped did not just get on the train.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_JustGotOnTrain))
	{
		return false;
	}
	
	//Ensure the ped did not just get off the train.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_JustGotOffTrain))
	{
		return false;
	}
	
	//Ensure the ped is not riding a train.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain))
	{
		return false;
	}
	
	//Ensure the ped is in a known state.
	if(!rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_UNALERTED, true))
	{
		return false;
	}
	
	//Grab the position.
	Vec3V vPosition = GetTransform().GetPosition();
	
	//Grab the right vector.
	Vec3V vRight = GetTransform().GetRight();
	
	//Grab the ped position.
	Vec3V vPedPosition = rPed.GetTransform().GetPosition();
	
	//Calculate the vector to the ped.
	Vec3V vToPed = Subtract(vPedPosition, vPosition);
	
	//Check if the ped is on the right.
	ScalarV scDot = Dot(vToPed, vRight);
	bool bPedIsOnRight = (IsGreaterThanAll(scDot, ScalarV(V_ZERO)) != 0);
	
	//Get the side flags closest to the platform.
	fwFlags8 uSideFlags = GetSideFlagsClosestToPlatform();
	
	//Check if the ped is on the correct side.
	if(bPedIsOnRight && uSideFlags.IsFlagSet(SF_Right))
	{
		return true;
	}
	else if(!bPedIsOnRight && uSideFlags.IsFlagSet(SF_Left))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTrain::AddPassenger(CPed* pPed)
{
	if (NetworkInterface::IsNetworkOpen() && !ms_bEnableTrainPassengersInMP)
		return false;

	trainDebugf2("CTrain::AddPassenger pPed[0x%p]",pPed);

	//Clear the invalid passengers.
	ClearInvalidPassengers();
	
	//Ensure there is room.
	if(m_aPassengers.IsFull())
	{
		return false;
	}
	
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}
	
	//Add the passenger.
	m_aPassengers.Append() = pPed;
	
	return true;
}

void CTrain::ClearInvalidPassengers()
{
	//Remove the invalid passengers.
	for(int i = 0; i < m_aPassengers.GetCount(); ++i)
	{
		if(!m_aPassengers[i])
		{
			m_aPassengers.DeleteFast(i);
			--i;
		}
	}
}

u32 CTrain::GetEmptyPassengerSlots() const
{
	//Get the max passengers.
	u32 uMaxPassengers = ((m_trainConfigIndex  >= 0) && (m_trainConfigCarriageIndex >= 0))? gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages[m_trainConfigCarriageIndex].m_maxPedsPerCarriage : 0;
	
	//Count the passengers.
	u32 uPassengers = m_aPassengers.GetCount();
	if(uPassengers >= uMaxPassengers)
	{
		return 0;
	}
	else
	{
		return uMaxPassengers - uPassengers;
	}
}

bool CTrain::HasPassengers() const
{
	//Check if there is a valid passenger.
	for(int i = 0; i < m_aPassengers.GetCount(); ++i)
	{
		if(m_aPassengers[i])
		{
			return true;
		}
	}
	
	return false;
}

bool CTrain::RemovePassenger(CPed* pPed)
{
	//Remove the passenger.
	for(int i = 0; i < m_aPassengers.GetCount(); ++i)
	{
		if(m_aPassengers[i] == pPed)
		{
			m_aPassengers.DeleteFast(i);
			return true;
		}
	}
	
	return false;
}

bool CTrain::HasNetworkClonedPassenger() const
{
	for (s32 iPassenger = 0; iPassenger < m_aPassengers.GetCount(); ++iPassenger)
	{
		CPed* pPassenger = m_aPassengers[iPassenger];
		if (pPassenger && (pPassenger->IsNetworkClone() || (pPassenger->GetNetworkObject() && pPassenger->GetNetworkObject()->IsPendingOwnerChange())))
		{
			return true;
		}
	}

	return false;
}


void CTrain::InitiateTrainReportOnJoin(u8 playerIndex)
{
	if (m_nTrainFlags.bEngine && !IsNetworkClone() && NetworkInterface::IsGameInProgress())
	{
		SetTrackActive(m_trackIndex,true);
		trainDebugf2("CTrain::InitiateTrainReportOnJoin playerIndex[%d] m_trackIndex[%d] m_bTrainActive[%d]",playerIndex,m_trackIndex,gTrainTracks[m_trackIndex].m_bTrainActive);
		CNetworkTrainReportEvent::Trigger(m_trackIndex, gTrainTracks[m_trackIndex].m_bTrainActive, playerIndex);
	}
}

bool CTrain::IsDoorBlocked(const CCarDoor& rDoor) const
{
	//Ensure the local player is valid.
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
	{
		return false;
	}

	//Get the door local BB.
	Vector3 vDoorMin;
	Vector3 vDoorMax;
	if(!rDoor.GetLocalBoundingBox(this, vDoorMin, vDoorMax))
	{
		return false;
	}

	//Get the door local matrix.
	Matrix34 mLocalDoor;
	if(!rDoor.GetLocalMatrix(this, mLocalDoor))
	{
		return false;
	}

	//Get the door position in world space.
	Vec3V vDoorPosition = GetTransform().Transform(VECTOR3_TO_VEC3V(mLocalDoor.d));

	//Create the door matrix.
	Mat34V mDoor = GetMatrix();
	mDoor.SetCol3(vDoorPosition);

	//Get the door oriented BB.
	spdAABB doorLocalBB(VECTOR3_TO_VEC3V(vDoorMin), VECTOR3_TO_VEC3V(vDoorMax));
	spdOrientedBB doorOrientedBB(doorLocalBB, mDoor);

	//Get the player oriented BB.
	const spdAABB& playerLocalBB = pPlayer->GetBaseModelInfo()->GetBoundingBox();
	spdOrientedBB playerOrientedBB(playerLocalBB, pPlayer->GetMatrix());

	//Ensure the BB's intersect.
	if(!doorOrientedBB.IntersectsOrientedBB(playerOrientedBB))
	{
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindNextStationPositionInDirection
// DOES:
///////////////////////////////////////////////////////////////////////////
// DEBUG!! -AC, This needs to take into account pingpong trains...
void CTrain::FindNextStationPositionInDirection(bool bDir, float PosOnTrack, float *pResultPosOnTrack, s32 *pStation) const
{
	s32 Station;
	// First find out what stations we're between.
	for (Station = 0; Station < gTrainTracks[m_trackIndex].m_numStations; Station++)
	{
		if(gTrainTracks[m_trackIndex].m_aStationDistFromStart[Station] > PosOnTrack)
		{
			break;
		}
	}
	if(Station >= gTrainTracks[m_trackIndex].m_numStations) Station = 0;

	if(!bDir)
	{
		Station -= 1;
		if(Station < 0) Station += gTrainTracks[m_trackIndex].m_numStations;
	}

	// If we are already very close to this station we pick the next one.
	if(rage::Abs(PosOnTrack - gTrainTracks[m_trackIndex].m_aStationDistFromStart[Station]) < 100.0f)
	{
		if(bDir)
		{
			Station += 1;
		}
		else
		{
			Station -= 1;
		}
		if(Station < 0) Station += gTrainTracks[m_trackIndex].m_numStations;
		if(Station >= gTrainTracks[m_trackIndex].m_numStations) Station = 0;
	}


	Assert(Station >= 0 && Station < gTrainTracks[m_trackIndex].m_numStations);

	*pStation = Station;
	*pResultPosOnTrack = gTrainTracks[m_trackIndex].m_aStationDistFromStart[Station];
}
// END DEBUG!!


// DEBUG!! -AC, This isn't used, but sounds too useful to remove even
// though it is commented out right now...
///////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindNearestStation
// DOES:		Goes through all the stations and finds the one closest to coordinates
///////////////////////////////////////////////////////////////////////////
//s32 CTrain::FindNearestStation(Vector3 pos)
//{
//	float	NearestDist, dist;
//	s32	Result = -1;
//
//	for (s32 S = 0; S < NUM_STATIONS; S++)
//	{
//		dist = (m_aStationCoors[S] - pos).XYMag();
//		if(dist < NearestDist || Result < 0)
//		{
//			Result = S;
//			NearestDist = dist;
//		}
//	}
//	Assert(Result >= 0);
//	return Result;
//}
// END DEBUG!!


///////////////////////////////////////////////////////////////////////////
// FUNCTION:	TrainStopsForStations
// DOES:		Sets the flag that determines whether this train should stop
//				at stations.
///////////////////////////////////////////////////////////////////////////
void CTrain::SetTrainStopsForStations(bool bStops)
{
	m_nTrainFlags.bStopForStations = bStops;
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetTrainIsStoppedAtStation
// DOES:		Makes the changes required for a mission train to be as if
//				stopped at a station (doors open etc)
///////////////////////////////////////////////////////////////////////////
void CTrain::SetTrainIsStoppedAtStation()
{
	// First we need to find out what side of the pCarriage the station is on.
	SetStationSideForEngineAndCarriages(FindNearestStationSide(m_trackIndex, m_trackPosFront) ^ m_nTrainFlags.bDirectionTrackForward);

	CTrain* pCarriage = FindEngine(this);
	Assert(pCarriage);
	while(pCarriage)
	{
		pCarriage->m_nTrainFlags.bAtStation = true;
		pCarriage->m_nPassengersState = PS_GetOff;

		pCarriage = pCarriage->GetLinkedToBackward();
	}
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetTrainToLeaveStation
// DOES:		Tells this train to pull out of the station
///////////////////////////////////////////////////////////////////////////
void CTrain::SetTrainToLeaveStation()
{
	CTrain* pCarriage = FindEngine(this);
	Assert(pCarriage);
	while(pCarriage)
	{
		pCarriage->m_nTrainFlags.bAtStation = false;
		pCarriage->m_nPassengersState = PS_None;

		pCarriage = pCarriage->GetLinkedToBackward();
	}
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindNearestStation
// DOES:
///////////////////////////////////////////////////////////////////////////
bool CTrain::FindNearestStationSide(s8 trackIndex, float PositionOnTrack) const
{
	float	closestDist = 9999999.9f;
	bool	bPlatformSide = false;

	const float totalTrackLength = gTrainTracks[m_trackIndex].GetTotalTrackLength();

	for (s32 Station = 0; Station < gTrainTracks[trackIndex].m_numStations; Station++)
	{
		float DistToStation = gTrainTracks[trackIndex].m_aStationDistFromStart[Station] - PositionOnTrack;

		while(DistToStation > totalTrackLength * 0.5f) DistToStation -= totalTrackLength;
		while(DistToStation < -totalTrackLength * 0.5f) DistToStation += totalTrackLength;
		DistToStation = rage::Abs(DistToStation);

		if(DistToStation < closestDist)
		{
			closestDist = DistToStation;
			bPlatformSide = gTrainTracks[trackIndex].m_aStationSide[Station] != CTrainTrack::None;
		}
	}
	return bPlatformSide;
}


///////////////////////////////////////////////////////////////////////////
// Get the next station along the track in the direction we're moving.
///////////////////////////////////////////////////////////////////////////
int CTrain::GetNextStation() const
{
	CTrain* pEngine = const_cast<CTrain*>(FindEngine(this));
	Assert(pEngine);
	if (!pEngine)
		return 0;

	s32 currentStation = pEngine->GetCurrentStation();
// DEBUG!! -AC, This needs to take into account pingpong trains...
	if(!pEngine->m_nTrainFlags.bDirectionTrackForward)
	{
		return (currentStation - 1 + gTrainTracks[m_trackIndex].m_numStations) % gTrainTracks[m_trackIndex].m_numStations;
	}
	else
	{
		return (currentStation + 1) % gTrainTracks[m_trackIndex].m_numStations;
	}
// END DEBUG!!
}


///////////////////////////////////////////////////////////////////////////
// Get what we consider to be our current station.
///////////////////////////////////////////////////////////////////////////
int CTrain::GetCurrentStation() const
{
	CTrain* pEngine = const_cast<CTrain*>(FindEngine(this));
	Assert(pEngine);

// DEBUG!! -AC, This seems damn dodgy to me...  Why rely and hope it has a
// good copy or our internal information?

	if(pEngine && pEngine->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
	{
		return static_cast<audTrainAudioEntity*>(pEngine->GetVehicleAudioEntity())->GetAnnouncedCurrentStation();
	}

	return -1;
	/*
	if(m_nTrainFlags.bDirectionTrackForward)
	{
	return (pEngine->m_nextStation - 1 + m_numStations[m_trackIndex]) % m_numStations[m_trackIndex];
	}
	else
	{
	return (pEngine->m_nextStation + 1) % m_numStations[m_trackIndex];
	}
	*/
	/*
	f32 posOnTrack = 0.f;
	s32 station = -1;
	pEngine->FindNextStationPositionInDirection(pEngine->m_nTrainFlags.bDirectionTrackForward, pEngine->m_trackPosFront, &posOnTrack, &station);

	return station;
	*/
	//return pEngine->m_currentStation;
// END DEBUG!!
}


///////////////////////////////////////////////////////////////////////////
// Determine how long until we arrive at the next station.
///////////////////////////////////////////////////////////////////////////
// DEBUG!! -AC, This function needs to properly take pingpong trains into account and needs a general
// going over...
static float ANOTHER_DAMN_MAGIC_NUMBER0 = 5.0f;
static float ANOTHER_DAMN_MAGIC_NUMBER1 = 30.0f;
int CTrain::GetTimeTilNextStation() const
{
	if(m_trackIndex < 0)
	{
		return 0;
	}

	CTrain* pEngine = const_cast<CTrain*>(FindEngine(this));
	Assert(pEngine);
	if (!pEngine)
		return 0;

	s32 nextStation = pEngine->GetNextStation();
	if(nextStation == -1 || Abs(pEngine->m_trackForwardSpeed) < ANOTHER_DAMN_MAGIC_NUMBER0)
	{
		return 0;
	}
	f32 stationPosOnTrack = gTrainTracks[m_trackIndex].m_aStationDistFromStart[nextStation];

	f32 dist = 0.f;
	if(!m_nTrainFlags.bDirectionTrackForward)
	{
		// pEngine runs backwards
		if(stationPosOnTrack > pEngine->m_trackPosFront)
		{
			if(stationPosOnTrack - pEngine->m_trackPosFront < ANOTHER_DAMN_MAGIC_NUMBER1)
			{
				// the pEngine will overshot the station node somewhat
				dist = 0.f;
			}
			else
			{
				// deal with wrapping around the circle
				const float totalTrackLength = gTrainTracks[m_trackIndex].GetTotalTrackLength();
				dist = pEngine->m_trackPosFront + (totalTrackLength - stationPosOnTrack);
			}
		}
		else
		{
			dist = pEngine->m_trackPosFront - stationPosOnTrack;
		}
	}
	else
	{
		if(stationPosOnTrack < pEngine->m_trackPosFront)
		{
			if(pEngine->m_trackPosFront - stationPosOnTrack < ANOTHER_DAMN_MAGIC_NUMBER1)
			{
				// the pEngine will overshot the station node somewhat
				dist = 0.f;
			}
			else
			{
				// deal with wrapping around the circle
				const float totalTrackLength = gTrainTracks[m_trackIndex].GetTotalTrackLength();
				dist = stationPosOnTrack + (totalTrackLength - pEngine->m_trackPosFront);
			}
		}
		else
		{
			dist = stationPosOnTrack - pEngine->m_trackPosFront;
		}
	}

	// roughly how long till we get there?
	return (s32)((Max(0.01f, dist) / Max(0.01f, Abs(pEngine->m_trackForwardSpeed))) * 1000.f);
}
// END DEBUG!!


///////////////////////////////////////////////////////////////////////////
// Get the string name of the sation at the index indicated.
///////////////////////////////////////////////////////////////////////////
const char *CTrain::GetStationName(int stationId) const
{
	//Assert(stationId > -1 && stationId < m_numStations[m_trackIndex] && g_audTrainStations[m_trackIndex][stationId]);
	if(stationId > -1 && stationId < gTrainTracks[m_trackIndex].m_numStations && g_audTrainStations[m_trackIndex][stationId])
	{
		return audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Always(g_audTrainStations[m_trackIndex][stationId]->NameTableOffset);
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////
// FUNCTION:	PopulateCarriage
// DOES:		Put some random peds in this carriage. Only if approaching station
//				and player is nearby (but not too close)
///////////////////////////////////////////////////////////////////////////
void CTrain::PopulateCarriage(s32& maxPedsToCreateForTrain_inOut)
{
	maxPedsToCreateForTrain_inOut = 0;
// 	s32 maxPedsPerCarriage = ((m_trainConfigIndex>=0) && (m_trainConfigCarriageIndex>=0))?gTrainConfigs.m_trainConfigs[m_trainConfigIndex].m_carriages[m_trainConfigCarriageIndex].m_maxPedsPerCarriage:0;
// 
// 	s32 iPassengersForThisCarriage = fwRandom::GetRandomNumberInRange(0,maxPedsPerCarriage);
// 	if(!gPopStreaming.IsFallbackPedAvailable())
// 	{
// 		return;
// 	}
// 
// 	if(maxPedsToCreateForTrain_inOut <= 0)
// 	{
// 		return;
// 	}

	//// Possibly add a driver.
	//if(!pDriver && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.7f)
	//{
	//	pDriver = CPedPopulation::AddPedInCar(this, true, 0, false, true, false);
	//	--iPassengersForThisCarriage;
	//	if((--maxPedsToCreateForTrain_inOut) <= 0)
	//	{
	//		return;
	//	}
	//}

// 	for (s32 s = 0; s < maxPedsPerCarriage && iPassengersForThisCarriage > 0 && s < m_SeatManager.GetMaxSeats(); s++)
// 	{
// 		if(!m_SeatManager.GetPedInSeat(s) && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.7f)
// 		{
// 			CPedPopulation::AddPedInCar(this, false, s, false, true, false);
// 			--iPassengersForThisCarriage;
// 			if((--maxPedsToCreateForTrain_inOut) <= 0)
// 			{
// 				return;
// 			}
// 		}
// 	}
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsLocalPlayerOnBoardThisEnginesTrain
// DOES:		If the local player is on a train that is attached to this engine we return true.
///////////////////////////////////////////////////////////////////////////
bool CTrain::IsLocalPlayerSeatedOnThisEnginesTrain() const
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	if(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		CVehicle *pVeh = pPlayerPed->GetMyVehicle();

		if(pVeh && pVeh->InheritsFromTrain())
		{
			CTrain* pCarriage = const_cast<CTrain*>(this);
			while(pCarriage)
			{
				if(pCarriage == pVeh)
				{
					return true;
				}
				pCarriage = pCarriage->GetLinkedToBackward();
			}
		}
	}
	return false;
}

bool CTrain::IsLocalPlayerStandingInThisTrain() const
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CEntity *pPlayerGroundPhysical = pPlayerPed->GetGroundPhysical();

		if (pPlayerGroundPhysical && pPlayerGroundPhysical->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pPlayerGroundPhysical);
			if(pVeh && pVeh->InheritsFromTrain())
			{
				CTrain* pCarriage = const_cast<CTrain*>(this);
				while(pCarriage)
				{
					if(pCarriage == pVeh)
					{
						return true;
					}
					pCarriage = pCarriage->GetLinkedToBackward();
				}
			}
		}
	}
	return false;
}

bool CTrain::DoesLocalPlayerDisableAmbientTrainStops() const
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		// B* 2046603 - trains glitch out when the player is an animal.
		if (pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////
// make the game pad shake.  Used for crossing over rail sections, etc.
///////////////////////////////////////////////////////////////////////////
void CTrain::SignalTrainPadShake(u32 iTimeToSecondaryShake /* = 0 */)
{
	const bool bLocalPlayerOnThisCarriage = IsLocalPlayerSeatedInThisCarriage();
	const bool bLocalPlayerOnOtherCarriage = IsLocalPlayerSeatedInAnyCarriage();
	if(bLocalPlayerOnThisCarriage || bLocalPlayerOnOtherCarriage)
	{
		CControlMgr::StartPlayerPadShakeByIntensity(iTrainPadShakeDuration,bLocalPlayerOnThisCarriage ? fTrainPadShakeIntensityPlayersCarriage : fTrainPadShakeIntensityOtherCarriage);
		if(iTimeToSecondaryShake)
		{
			m_iTimeOfSecondaryPadShake = fwTimer::GetTimeInMilliseconds() + iTimeToSecondaryShake;
		}
		else
		{
			m_iTimeOfSecondaryPadShake = 0;
		}
	}
}

bool CTrain::IsEntityBlockingProgress(CEntity * pEntity, const Vector3 & vTrainFrontPos, const float fScanDist, float & fOut_DistToEntity)
{
	const ScalarV fMaxScanDist(fScanDist);
	const float fMaxScanDistSqr = fScanDist*fScanDist;

	static dev_float sfMaxDistToSide = 10.0f;

	fOut_DistToEntity = 0.0f;

	Vector3 vToEntity = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - vTrainFrontPos;
	if(Abs(vToEntity.z) > 4.0f)
		return false;
	const float fDistSqr = vToEntity.Mag2();
	if(fDistSqr > SMALL_FLOAT && fDistSqr < fMaxScanDistSqr)
	{
		const float fDistAhead = DotProduct(vToEntity, VEC3V_TO_VECTOR3(GetTransform().GetB()));

		if(fDistAhead > 0.0f)
		{
			const float fDistToSide = DotProduct(vToEntity, VEC3V_TO_VECTOR3(GetTransform().GetA()));

			if(Abs(fDistToSide) < sfMaxDistToSide)
			{
				// Scan ahead along track
				CEntityBoundAI bound(*pEntity, pEntity->GetTransform().GetPosition().GetZf(), 1.0f);

				s32 c = 50;
				s32 iCurr = m_currentNode;
				ScalarV fScannedDist(V_ZERO);
				while(c > 0 && (fScannedDist < fMaxScanDist).Getb())
				{
					// get next node; loop if appropriate
					s32 iNext = iCurr+1;
					if(iNext >= gTrainTracks[m_trackIndex].m_numNodes)
					{
						if(!gTrainTracks[m_trackIndex].m_isLooped)
							break;
						iNext -= gTrainTracks[m_trackIndex].m_numNodes;
					}

					const Vec3V v1 = gTrainTracks[m_trackIndex].GetNode(iCurr)->GetCoorsV();
					const Vec3V v2 = gTrainTracks[m_trackIndex].GetNode(iNext)->GetCoorsV();

					ScalarV fTestDist;
					if(!bound.TestLineOfSightReturnDistance(v1, v2, fTestDist))
					{
						fScannedDist += fTestDist;
						ScalarV fTrainProgressFromStartNode( m_trackPosFront - gTrainTracks[m_trackIndex].GetNode(m_currentNode)->GetLengthFromStart() );
						fScannedDist -= fTrainProgressFromStartNode;
						fOut_DistToEntity = Max(fScannedDist.Getf(), 0.0f);

						return true;
					}

					fScannedDist += Dist(v1, v2);
					iCurr = iNext;
					c--;
				}
			}
		}
	}
	return false;
}

void CTrain::ScanForEntitiesBlockingProgress(float & fInOut_dirForwardSpeedDesired)
{
	static dev_s32 sTimeBetweenScans = 2000;	// how long between scans
	static dev_float fScanAheadTime = 4.0f;		// how many seconds ahead

	const float fScanDist = fInOut_dirForwardSpeedDesired * fScanAheadTime;

	//Trams only should slow down for vehicles. 
	const CVehicleModelInfo* pModelInfo = GetVehicleModelInfo();
	const bool bLocalPlayerOnAnyCarriage = IsLocalPlayerSeatedInAnyCarriage();
	const bool bSlowsForEntities = (pModelInfo && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_ELECTRIC)) && !bLocalPlayerOnAnyCarriage;


	Vector3 vecFrontPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(GetTransform().GetB()) *(GetBoundingBoxMax().y);
	vecFrontPos += GetVelocity()*fwTimer::GetTimeStep();

	float fClosestEntityDist = FLT_MAX;
	CEntity * pClosestEntity = NULL;

	bool bWasAvoiding = (m_pVehicleBlockingProgress || m_pPedBlockingProgress);

	//---------------------------------
	// Check for vehicles in the way

	if(m_pVehicleBlockingProgress)
	{
		float fDistToEntity = 0.0f;
		if(!IsEntityBlockingProgress(m_pVehicleBlockingProgress, vecFrontPos, fScanDist, fDistToEntity))
		{
			m_pVehicleBlockingProgress = NULL;
			m_iLastTimeScannedForVehicles = 0;
		}
		else
		{
			if(fDistToEntity < fClosestEntityDist)
			{
				fClosestEntityDist = fDistToEntity;
				pClosestEntity = m_pVehicleBlockingProgress;
			}
		}
	}

	if(!m_pVehicleBlockingProgress)
	{
		s32 iScanDeltaTime = fwTimer::GetTimeInMilliseconds() - m_iLastTimeScannedForVehicles;
		if(iScanDeltaTime > sTimeBetweenScans)
		{
			float fClosestDist = FLT_MAX;

			s32 i = (s32) CVehicle::GetPool()->GetSize();
			while(i--)
			{
				CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(i);
				if(pVehicle)
				{
					if(pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
					{
						float fDistToEntity = 0.0f;
						if(IsEntityBlockingProgress(pVehicle, vecFrontPos, fScanDist, fDistToEntity))
						{
							if(fDistToEntity < fClosestDist)
							{
								m_pVehicleBlockingProgress = pVehicle;
								fClosestDist = fDistToEntity;
							}
						}
					}
				}
			}

			if(fClosestDist < fClosestEntityDist)
			{
				fClosestEntityDist = fClosestDist;
				pClosestEntity = m_pVehicleBlockingProgress;
			}

			m_iLastTimeScannedForVehicles = fwTimer::GetTimeInMilliseconds();
		}
	}

	//----------------------------
	// Check for peds in the way

	if(m_pPedBlockingProgress)
	{
		float fDistToEntity = 0.0f;
		if(!IsEntityBlockingProgress(m_pPedBlockingProgress, vecFrontPos, fScanDist, fDistToEntity))
		{
			m_pPedBlockingProgress = NULL;
			m_iLastTimeScannedForPeds = 0;
		}
		else
		{
			if(fDistToEntity < fClosestEntityDist)
			{
				fClosestEntityDist = fDistToEntity;
				pClosestEntity = m_pPedBlockingProgress;
			}
		}
	}

	if(!m_pPedBlockingProgress)
	{
		s32 iScanDeltaTime = fwTimer::GetTimeInMilliseconds() - m_iLastTimeScannedForPeds;
		if(iScanDeltaTime > sTimeBetweenScans)
		{
			float fClosestDist = FLT_MAX;

			s32 i = CPed::GetPool()->GetSize();
			while(i--)
			{
				CPed * pPed = CPed::GetPool()->GetSlot(i);
				if(pPed)
				{
					if(!pPed->IsDead() && !pPed->GetIsInVehicle())
					{
						const bool bAvoidThisType = pPed->GetCapsuleInfo()->IsBiped() ||
							(pPed->GetCapsuleInfo()->IsQuadruped() && pPed->GetCapsuleInfo()->GetMaxSolidHeight() > 0.25f);

						if(bAvoidThisType)
						{
							float fDistToEntity = 0.0f;
							if(IsEntityBlockingProgress(pPed, vecFrontPos, fScanDist, fDistToEntity))
							{
								if(fDistToEntity < fClosestDist)
								{
									m_pPedBlockingProgress = pPed;
									fClosestDist = fDistToEntity;
								}
							}
						}
					}
				}
			}

			if(fClosestDist < fClosestEntityDist)
			{
				fClosestEntityDist = fClosestDist;
				pClosestEntity = m_pPedBlockingProgress;
			}

			m_iLastTimeScannedForPeds = fwTimer::GetTimeInMilliseconds();
		}
	}

	//--------------------------------------------------------------------------
	// Now process stopping / sounding horn, for closest entity if applicable

	if(pClosestEntity)
	{
		// Calculate slow down speed
		if(bSlowsForEntities)
		{
			static dev_float fMinDist = 0.1f;

			float trackForwardSpeed;

			if(fClosestEntityDist < fMinDist)
			{
				trackForwardSpeed = 0.0f;
			}
			else
			{
				const float fRatio = Clamp(fClosestEntityDist / fScanDist, 0.0f, 1.0f);
				const float fInvRatio = 1.0f - fRatio;
				const float fInvSqrFalloff = 1.0f - (fInvRatio*fInvRatio);
				//const float fCosineFalloff = (Cosf(fInvRatio * PI) + 1.0f) * 0.5f;
				//const float fFalloff = (0.8f * fInvSqrFalloff) + (0.2f * fCosineFalloff);	

				trackForwardSpeed = fInvSqrFalloff * fInOut_dirForwardSpeedDesired;
			}

			fInOut_dirForwardSpeedDesired = trackForwardSpeed;

			m_iTimeWaitingForBlockage += fwTimer::GetTimeStepInMilliseconds();
		}
		else
		{
			m_iTimeWaitingForBlockage = 0;
		}

		// Sound horn
		static dev_u32 hornTypeHash = ATSTRINGHASH("HELDDOWN", 0x839504CB);

		// Play long horn sound if we're first encountering something on the track, and we've not played horn within 10secs
		if( !bWasAvoiding && fwTimer::GetTimeInMilliseconds() - m_iLastHornTime > 10000 )
		{
			PlayCarHorn(true, hornTypeHash);

			// Set last horn time; add in a bit of variation to the next time it is triggered
			m_iLastHornTime = fwTimer::GetTimeInMilliseconds() - fwRandom::GetRandomNumberInRange(0, 4000);
		}
		else
		{
			// Have we been waiting for a ped/vehicle to move for over 30secs?
			if( m_iTimeWaitingForBlockage > 30000 )
			{
				// Play horn sound, choosing randomly which type (favour short?)
				PlayCarHorn(true, fwRandom::GetRandomTrueFalse() ? hornTypeHash : 0);
				// Set last horn time; add in a bit of variation to the next time it is triggered
				m_iLastHornTime = fwTimer::GetTimeInMilliseconds() - fwRandom::GetRandomNumberInRange(0, 4000);
				// Reset blockage counter, add in some amount of variation
				m_iTimeWaitingForBlockage = fwRandom::GetRandomNumberInRange(0, 2000);
			}
		}

		//grcDebugDraw::Line(vecFrontPos, VEC3V_TO_VECTOR3(pClosestEntity->GetTransform().GetPosition()), Color_orange, Color_orange);
	}
	else
	{
		m_iTimeWaitingForBlockage = 0;
	}
}

///////////////////////////////////////////////////////////////////////////
// Trigger the train horn
///////////////////////////////////////////////////////////////////////////
void CTrain::PlayCarHorn(bool UNUSED_PARAM(bOneShot), u32 hornTypeHash)
{
	if(!IsHornOn()) 
	{
		if(m_VehicleAudioEntity)
		{
			m_VehicleAudioEntity->SetHornType(hornTypeHash);
			m_VehicleAudioEntity->PlayVehicleHorn();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// Update the passengers (boarding, seating, etc.) for this carriage.
///////////////////////////////////////////////////////////////////////////
void CTrain::ProcessPassengers()
{
	//Ensure we are at a station.
	if(!m_nTrainFlags.bAtStation)
	{
		return;
	}
	
	//Check the passengers state.
	switch(m_nPassengersState)
	{
		case PS_None:
		{
			break;
		}
		case PS_GetOff:
		{
			//Make the passengers get off.
			MakePassengersGetOff();
			
			//Wait for the passengers to get off.
			m_nPassengersState = PS_WaitingForGetOff;
			
			break;
		}
		case PS_WaitingForGetOff:
		{
			//Check if any passengers are getting off.
			if(ArePassengersGettingOff())
			{
				break;
			}
			
			//Allow passengers to get on.
			m_nPassengersState = PS_GetOn;
			
			break;
		}
		case PS_GetOn:
		{
			//Make the nearby peds get on.
			MakeNearbyPedsGetOn();
			
			//Wait for the passengers to get on.
			m_nPassengersState = PS_WaitingForGetOn;
			
			break;
		}
		case PS_WaitingForGetOn:
		{
			//Check if any passengers are getting on.
			if(ArePassengersGettingOn())
			{
				break;
			}
			
			//Clear the state.
			m_nPassengersState = PS_None;
			
			break;
		}
		default:
		{
			vehicleAssertf(false, "Unknown passengers state: %d.", m_nPassengersState);
			break;
		}
	}
}

bool CTrain::FindClosestBoardingPoint(Vec3V_In vPos, CBoardingPoint& rBoardingPoint) const
{
	//Ensure the boarding points are valid.
	const CBoardingPoints* pBoardingPoints = GetBoardingPoints();
	if(!pBoardingPoints || pBoardingPoints->GetNumBoardingPoints() == 0)
	{
		return false;
	}

	//Get the side flags closest to the platform.
	fwFlags8 uSideFlags = GetSideFlagsClosestToPlatform();
	if(uSideFlags == 0)
	{
		return false;
	}

	//Transform the world space position to local space.
	Vec3V vLocalPos = GetTransform().UnTransform(vPos);

	//Find the closest boarding point, based on the platform sides.
	int iClosest = -1;
	ScalarV scBestDistSq = ScalarVFromF32(FLT_MAX);
	for(int b = 0; b < pBoardingPoints->GetNumBoardingPoints(); b++)
	{
		//Grab the boarding point.
		const CBoardingPoint& bp = pBoardingPoints->GetBoardingPoint(b);

		//Check if the boarding point is on the right.
		bool bBoardingPointIsOnRight = (bp.m_fPosition[0] >= 0.0f);
		if(bBoardingPointIsOnRight && !uSideFlags.IsFlagSet(SF_Right))
		{
			continue;
		}
		else if(!bBoardingPointIsOnRight && !uSideFlags.IsFlagSet(SF_Left))
		{
			continue;
		}

		//Generate the boarding point position.
		Vec3V vBoardingPointPos = Vec3V(bp.m_fPosition[0], bp.m_fPosition[1], bp.m_fPosition[2]);

		//Ensure the boarding point is better.
		ScalarV scDistSq = DistSquared(vLocalPos, vBoardingPointPos);
		if(IsGreaterThanOrEqualAll(scDistSq, scBestDistSq))
		{
			continue;
		}

		//Assign the boarding point.
		scBestDistSq = scDistSq;
		iClosest = b;
	}

	//Ensure the closest boarding point is valid.
	if(iClosest < 0)
	{
		return false;
	}

	//Assign the boarding point.
	rBoardingPoint = pBoardingPoints->GetBoardingPoint(iClosest);

	return true;
}

bool CTrain::ArePassengersGettingOff() const
{
	//Iterate over the passengers.
	for(int i = 0; i < m_aPassengers.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPassengers[i];
		if(!pPed)
		{
			continue;
		}
		
		//Ensure the task is valid.
		CTaskRideTrain* pTask = static_cast<CTaskRideTrain *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RIDE_TRAIN));
		if(!pTask)
		{
			continue;
		}
		
		//Check if the ped is getting off.
		if(pTask->IsGettingOff())
		{
			return true;
		}
	}
	
	return false;
}

bool CTrain::ArePassengersGettingOn() const
{
	//Iterate over the passengers.
	for(int i = 0; i < m_aPassengers.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPassengers[i];
		if(!pPed)
		{
			continue;
		}

		//Ensure the task is valid.
		CTaskRideTrain* pTask = static_cast<CTaskRideTrain *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RIDE_TRAIN));
		if(!pTask)
		{
			continue;
		}

		//Check if the ped is getting on.
		if(pTask->IsGettingOn())
		{
			return true;
		}
	}

	return false;
}

void CTrain::ChangeTrainState(TrainState nTrainState)
{
	//Ensure the train state is changing.
	if(nTrainState == m_nTrainState)
	{
		return;
	}
	
	//Change the train state.
	m_nTrainState = nTrainState;
	
	//Clear the time in the train state.
	m_fTimeInTrainState = 0.0f;

	//Note: This code below is only processed on remotes in MP
	if (NetworkInterface::IsGameInProgress() && IsNetworkClone() && m_nTrainFlags.bEngine)
	{
		if (m_nTrainState == TS_ArrivingAtStation)
		{
			//Announce that we are arriving at a station.
			if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
			{
				static_cast<audTrainAudioEntity*>(GetVehicleAudioEntity())->AnnounceTrainAtStation();
			}
		}
		else if (m_nTrainState == TS_LeavingStation)
		{
			//Iterate over the carriages
			CTrain* pCarriage = this;
			if (pCarriage && pCarriage->ShouldDoFullUpdate())
			{
				bool bCloseDoors = true;
				if (m_bMetroTrain)
					bCloseDoors = !m_doorsForcedOpen;

				for(pCarriage = this; pCarriage; pCarriage = pCarriage->GetLinkedToBackward())
				{
					if (bCloseDoors)
					{
						//Ensure doors are closed
						pCarriage->ProcessDoors(0.0f);
					}

					//Note that we are at a station.
					pCarriage->m_nTrainFlags.bAtStation = false;

					//Set the passenger state.
					pCarriage->m_nPassengersState = PS_None;

					//Move to the leaving state.
					pCarriage->ChangeTrainState(TS_LeavingStation);
				}

				//Announce that we are leaving the station.
				if(GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
				{
					static_cast<audTrainAudioEntity*>(GetVehicleAudioEntity())->AnnounceTrainLeavingStation();
				}
			}
		}
	}
}

fwFlags8 CTrain::GetSideFlagsClosestToPlatform() const
{
	//Check if the model dir is flipped.
	bool bFlipModelDir = GetFlipModelDir();
	
	//Check the platform side.
	CTrainTrack::PlatformSide nPlatformSide = (CTrainTrack::PlatformSide)m_nTrainFlags.iStationPlatformSides;
	switch(nPlatformSide)
	{
		case CTrainTrack::None:
		{
			return 0;
		}
		case CTrainTrack::Left:
		{
			return !bFlipModelDir ? (u8)SF_Left : (u8)SF_Right;
		}
		case CTrainTrack::Right:
		{
			return !bFlipModelDir ? (u8)SF_Right : (u8)SF_Left;
		}
		case CTrainTrack::Both:
		{
			return SF_Left | SF_Right;
		}
		default:
		{
			vehicleAssertf(false, "The platform side is unknown: %d.", nPlatformSide);
			return 0;
		}
	}
}

bool CTrain::GetFlipModelDir() const
{
	//Ensure the carriage info is valid.
	TrainCarriageInfo* pInfo = gTrainConfigs.GetCarriageInfoForTrain(*this);
	if(!pInfo)
	{
		return false;
	}
	
	return pInfo->m_flipModelDir;
}

void CTrain::MakeNearbyPedsGetOn()
{
	//MP: Swith to enable or disable MP AI passengers
	if (NetworkInterface::IsNetworkOpen() && ms_bEnableTrainPassengersInMP)
		return;

	//Get the empty passenger slots.
	u32 uEmptyPassengerSlots = GetEmptyPassengerSlots();
	if(uEmptyPassengerSlots == 0)
	{
		return;
	}
	
	//Choose the number of passengers.
	u32 uPassengers = fwRandom::GetRandomNumberInRange(0, uEmptyPassengerSlots + 1);
	if(uPassengers == 0)
	{
		return;
	}
	
	//Ensure the model info is valid.
	CBaseModelInfo* pBaseModelInfo = GetBaseModelInfo();
	if(!pBaseModelInfo)
	{
		return;
	}
	
	//Calculate the radius.
	float fRadius = pBaseModelInfo->GetBoundingSphereRadius();
	fRadius += 15.0f;
	
	//Grab the train position.
	Vec3V vPosition = GetTransform().GetPosition();
	
	//Iterate over the nearby peds.
	CEntityIterator entityIterator(IteratePeds, NULL, &vPosition, fRadius);
	for(CEntity* pEntity = entityIterator.GetNext(); pEntity; pEntity = entityIterator.GetNext())
	{
		//Grab the ped.
		CPed* pPed = static_cast<CPed *>(pEntity);
		
		//Ensure the ped can get on the train.
		if(!CanPedGetOnTrain(*pPed))
		{
			continue;
		}

		//Tell the ped to ride the train.
		CTaskRideTrain* pTask = rage_new CTaskRideTrain(this);
		CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pTask, false, E_PRIORITY_RIDE_TRAIN);
		pPed->GetPedIntelligence()->AddEvent(event);
		
		//Decrease the number of passengers to board.
		--uPassengers;
		if(uPassengers == 0)
		{
			break;
		}
	}
}

void CTrain::MakePassengersGetOff()
{
	//Calculate the chances to get off.
	TUNE_GROUP_FLOAT(TRAIN, fChancesToGetOff, 0.33f, 0.0f, 1.0f, 0.01f);

	//Iterate over the passengers.
	for(int i = 0; i < m_aPassengers.GetCount(); ++i)
	{
		//Ensure the ped is valid.
		CPed* pPed = m_aPassengers[i];
		if(!pPed)
		{
			continue;
		}

		//Ensure this ped can get off.
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NeverLeaveTrain))
		{
			continue;
		}

		//Ensure this ped should get off.
		float fValue = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AlwaysLeaveTrainUponArrival) && fValue >= fChancesToGetOff)
		{
			continue;
		}

		//Ensure the task is valid.
		CTaskRideTrain* pTask = static_cast<CTaskRideTrain *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RIDE_TRAIN));
		if(!pTask)
		{
			continue;
		}

		//Tell the ped to get off.
		pTask->GetOff();
	}
}

void CTrain::RemoveAllOccupants()
{
	for (s32 iPassenger = 0; iPassenger < m_aPassengers.GetCount(); ++iPassenger)
	{
		CPed* pPassenger = m_aPassengers[iPassenger];
		if (pPassenger)
		{
			AssertEntityPointerValid_NotInWorld(pPassenger);
			pPassenger->FlagToDestroyWhenNextProcessed(false);
		}
	}
}

void CTrain::ValidateLinkedLists( CTrain* pCarriage )
{
	if( !pCarriage )
	{
		return;
	}

	CTrain* engine = FindEngine( pCarriage );

	// make sure the engine pointer is valid
	if( engine &&
		(!CVehicle::GetPool()->IsValidPtr( engine ) ||
		!engine->InheritsFromTrain() ))
	{
		Assertf( 0, "Train Engine (%p) is not valid for carriage %p (%s) (%s)", engine, pCarriage, pCarriage ? pCarriage->GetModelName() : "",pCarriage->GetNetworkObject() ? pCarriage->GetNetworkObject()->GetLogName() : "-none-");
		return;
	}

	CTrain* slow = engine;
	CTrain* fast = engine ? engine->GetLinkedToBackward() : NULL;

	bool bAssert = false;
	char assertStr[200];

	// check backwards through all the carriages to make sure they are linked correctly
	while(  fast &&
			fast->GetLinkedToBackward() )
	{
		if( fast == slow ||
			fast->GetLinkedToBackward() == slow )
		{
			bAssert = true;
			formatf(assertStr, "Train linked to backwards contains a loop for carriage %p (%s) (%s)", pCarriage, pCarriage ? pCarriage->GetModelName() : "", pCarriage->GetNetworkObject() ? pCarriage->GetNetworkObject()->GetLogName() : "-none-");
			break;
		}
		slow = slow->GetLinkedToBackward();
        fast = fast->GetLinkedToBackward()->GetLinkedToBackward();
	}

	if (!bAssert)
	{
		// get the caboose, this is always the last carriage, and validate it
		slow = FindCaboose(pCarriage );

		if( slow &&
			(!CVehicle::GetPool()->IsValidPtr( slow ) ||
			!slow->InheritsFromTrain() ) )
		{
			bAssert = true;
			formatf(assertStr, "Train Caboose (%p) is not valid for carriage %p (%s) (%s)", slow, pCarriage, pCarriage ? pCarriage->GetModelName() : "", pCarriage->GetNetworkObject() ? pCarriage->GetNetworkObject()->GetLogName() : "-none-");
		}
	}

	if (!bAssert)
	{
		fast = slow ? slow->GetLinkedToForward() : NULL;

		// loop back forward through the list and check it is valid
		while(  fast &&
				fast->GetLinkedToForward() )
		{
			if( fast == slow ||
				fast->GetLinkedToForward() == slow )
			{
				bAssert = true;
				formatf(assertStr, "Train linked to forward contains a loop for carriage %p (%s) (%s)", pCarriage, pCarriage ? pCarriage->GetModelName() : "",pCarriage->GetNetworkObject() ? pCarriage->GetNetworkObject()->GetLogName() : "-none-");
				break;
			}
			slow = slow->GetLinkedToForward();
			fast = fast->GetLinkedToForward()->GetLinkedToForward();
		}
	}

	if (!bAssert)
	{
		// loop back through the list just to make sure the engine is valid on all 
		// carriages
		slow = engine;
	
		while( slow )
		{
			if( slow->m_pEngine && slow->m_pEngine != engine )
			{
				bAssert = true;
				formatf(assertStr, "Train not all carriages have the same engine for carriage %p (%s) (%s) -- weird engine %p (%s) (%s)", pCarriage, pCarriage ? pCarriage->GetModelName() : "", pCarriage->GetNetworkObject() ? pCarriage->GetNetworkObject()->GetLogName() : "-none-",slow->m_pEngine.Get(),slow->m_pEngine.Get() ? slow->m_pEngine->GetModelName() : "",slow->m_pEngine.Get() && slow->m_pEngine->GetNetworkObject() ? slow->m_pEngine->GetNetworkObject()->GetLogName() : "-none-");
				break;
			}
			slow = slow->GetLinkedToBackward();
		}
	}

	if (bAssert)
	{
#if __BANK 		
		slow = engine;

		u32 numCarriages = engine->GetNumCarriages();
		u32 currCarriage = 0;

		//Note: changed trainDebugf1 here to trainDisplayf so it will always be present in __BANK builds and report this information (crashed at one point in the while below and lost information from assertStr) - also present the assertStr up front to ensure it is provided. lavalley
		trainDisplayf("%s",assertStr);
		trainDisplayf("Num carriages = %d", numCarriages);

		while( slow && currCarriage < numCarriages ) //think the intent here is to log the train make-up, but we want to ensure that 1) slod is valid (otherwise crash); 2) our current carriage iterator is less than the max num carriages - otherwise potential loop.  lavalley
		{
			netObject* slowObj = slow->GetNetworkObject();

			CTrain* engine = slow->GetEngine(); //this was a crash point previously because of previous while conditional while( slow || currCarriage == numCarriages ) -- which could allow a slow of NULL. lavalley
			netObject* engineObj = engine ? engine->GetNetworkObject() : NULL;
			CTrain* forward = slow->GetLinkedToForward();
			netObject* forObj = forward ? forward->GetNetworkObject() : NULL;
			CTrain* backward = slow->GetLinkedToBackward();
			netObject* backObj = backward ? backward->GetNetworkObject() : NULL;

			trainDisplayf("%s %p (%s) (%s): engine %p (%s) (%s), forward %p (%s) (%s), backward %p (%s) (%s)\n", slow == engine ? "ENGINE" : "CARRIAGE", slow, slow->GetModelName(), slowObj ? slowObj->GetLogName() : "-none-", engine, engine ? engine->GetModelName() : "", engineObj ? engineObj->GetLogName() : "-none-", forward, forward ? forward->GetModelName() : "", forObj ? forObj->GetLogName() : "-none-", backward, backward ? backward->GetModelName() : "", backObj ? backObj->GetLogName() : "-none");

			currCarriage++;
			slow = backward;
		}
#endif

#if __FINAL
		if(ms_bEnableTrainLinkLoopForceCrash)
		{
			NETWORK_QUITF(0, "%s", assertStr);
		}
#else
		NETWORK_QUITF(0, "%s", assertStr);
#endif
	}
}

void CTrainCloudListener::OnCloudEvent(const CloudEvent* pEvent)
{ 
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
		return;

	CTrain::SetTunablesValues(); 
}


