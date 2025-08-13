/////////////////////////////////////////////////////////////////////////////////
// FILE :		vehiclepopulation.h
// PURPOSE :	Deals with creation and removal of vehicles in the world.
//				It manages the vehicles population by removing what vehicles we
//				can and adding what vehicles we should.
//				It creates ambient vehicles and also vehicles at attractors when
//				appropriate.
// AUTHOR :		Obbe.
//				Adam Croston.
// CREATED :	10/05/06
/////////////////////////////////////////////////////////////////////////////////
#ifndef VEHICLEPOPULATION_H_
#define VEHICLEPOPULATION_H_

// rage includes
#include "vector/vector3.h"
#include "vector/color32.h"
#include "grcore/debugdraw.h"
#include "parser/macros.h"

// game includes
#include "control/population.h"
#include "modelinfo/ModelInfo.h"
#include "scene/DynamicEntity.h"
#include "scene/EntityTypes.h"
#include "scene/RegdRefTypes.h"
#include "vehicleAi/pathfind.h"
#include "metadata/VehiclePopulationTuning.h"
#include "game/popmultiplierareas.h"

#include "game/Dispatch/DispatchServices.h"
#include "script/thread.h"

// forward declarations
class CPopCtrl;
class CVehicle;
class CAutomobile;
class CHeli;

#define VEHGEN_STORE_DEBUG_INFO_IN_LINKS			0	// for more help when debugging vehicle-generation links
#define DR_VEHGEN_GRADE_DEBUGGING					0
#define VEHICLE_POPULATION_SPAWN_HISTORY_SIZE		64	// in __BANK builds we can maintain a history of spawned vehicles for debugging purposes
#define VEHGEN_LINKS_IN_BOTH_DIRECTIONS				1	// whether to have two entries for every pair of nodes (more processing, but we can differentiate between directions)
#define VEHGEN_NUM_PRESELECTED_LINKS				60
#define VEHGEN_MAX_DISABLED_NODES					60
#define VEHGEN_CHECK_FOR_TRAFFIC_JAMS				1	// When computing densities, check ahead for conjested junctions/nodes
#define VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE		0	// Try to find and cache the next possible junction for each link. Disabled for now due to performance. Future - cache this offline
#define VEHGEN_NUM_AVERAGE_DISPARITY_FRAMES			30

#define NUM_PLAYERS_ROAD_CID_PENALTIES				5

enum
{
	PLAYERS_ROAD_CID_ANGLE_DEGREES = 0,
	PLAYERS_ROAD_CID_ANGLE,
	PLAYERS_ROAD_CID_PENALTY,
	NUM_PLAYERS_ROAD_CID_VALUES
};

struct sScheduledLink
{
	Vec4V pos_and_mi;
};

/////////////////////////////////////////////////////////////////////////////////
// This class deals with creation and removal of vehicles in the world.
/////////////////////////////////////////////////////////////////////////////////
class CVehiclePopulation
{
	friend struct CVehiclePopulationAsyncUpdateData;

public:

	// Debug enums
	enum EVehLastRemovalReason
	{
		Removal_None,
		Removal_OutOfRange,
		Removal_OutOfRange_OnScreen,
		Removal_OutOfRange_Parked,
		Removal_OutOfRange_NoPeds,
		Removal_OutOfRange_BehindZoomedPlayer,
		Removal_OutOfRange_OuterBand,
		Removal_StuckOffScreen,
		Removal_StuckOnScreen,
		Removal_LongQueue,
		Removal_WreckedOffScreen,
		Removal_EmptyCopOrWrecked,
		Removal_EmptyCopOrWrecked_Respawn,
		Removal_NetGameCongestion,
		Removal_NetGameEmptyCrowdingPlayers,
		Removal_OccupantAddFailed,
		Removal_BadTrailerType,
		Removal_Debug,
		Removal_RemoveAllVehicles,
		Removal_ExcessNetworkVehicle,
		Removal_RemoveAggressivelyFlag,
		Removal_KickedFromVehicleReusePool,
		Removal_PatrollingLawNoLongerNeeded,
		Removal_TooSteepAngle,
		Removal_RemoveAllVehsWithExcepton
	};

public:
	static void			InitValuesFromConfig();
	static void			InitValuesFromTunables();
	static void			InitTunables();
	static void			Init     								(unsigned initMode);
	static void			Shutdown    							(unsigned shutdownMode);
	static void			Process									();
#if GTA_REPLAY
	static void			ProcessReplay							();
#endif // GTA_REPLAY

	static void			StartAsyncUpdateJob						();
	static void			StartAsyncUpdateDensitiesJob			();

#if __BANK
	static void			InitWidgets								();
	static void			VehicleMemoryBudgetMultiplierChanged	();
	static void			DisplayMessageHistory					();
	static void			DebugEventCreateVehicle				    (CVehicle* veh, CVehicleModelInfo* modelInfo);
	static void			SetVehGroupOverride();
	static void			ModifyVehicleSpacings();
	static void			OnScaleUpDefaultVehicleSpacings();
	static void			OnScaleDownDefaultVehicleSpacings();
	static s32			GetNumExcessDisparateLinks() { return ms_iNumExcessDisparateLinks; }
#endif

	// Population control functions.
	static float		GetVehicleLodScale						(bool bIgnoreSettings = true);
	static float		GetPopulationRangeScale					() { return ms_fCalculatedPopulationRangeScale; }
	static float		GetViewRangeScale						() { return ms_fCalculatedViewRangeScale; }
	static float		GetAverageLinkHeight					() { return ms_averageLinkHeight; }
	static float		CalculateHeightRangeScale				(const float fLowZ, const float fHighZ, const float fMult);
	static float		CalculateAircraftRangeScale				();
	static void 		CalculatePopulationRangeScale			();
	static void			LockPopulationRangeScale				() { ms_bLockPopuationRangeScale = true; }
	static void			UnlockPopulationRangeScale				() { ms_bLockPopuationRangeScale = false; }
	static void			SetDesertedStreetMultiplierLocked		(const bool locked ) { ms_bDesertedStreetMultiplierLocked = locked; }
	static void			CalculateSpawnFrustumNearDist			();
	static float		GetRemovalTimeScale();
	static float		CalculatePopulationSpawnThicknessScale	();
	static void			ForcePopulationInit						();
	static bool			HasForcePopulationInitFinished			();
	static s32			CountVehsOfType							(u32 MI, bool bIgnoreParkedVehicles = false, bool bOnlyCountRandomPopType = false, bool bIgnoreWrecked = false);
	static s32			GetTotalVehsOnMap						();
	static s32			GetTotalNonMissionVehsOnMap				();
	static s32			GetTotalAmbientMovingVehsOnMap			();
	static s32			GetTotalEmptyWreckedVehsOnMap			() { return ms_iNumWreckedEmptyCars; }
	enum VehPopOp{ ADD = 0, SUB = 1};
	static void			UpdateVehCount							(CVehicle* pVeh, VehPopOp AddOrSub);
	static void			MakeStreetsALittleMoreDeserted			();
	static float		GetDesertedStreetsMultiplier			(){return ms_desertedStreetsMultiplier;}
	static void			RegisterInterestingVehicle				(CVehicle* pVeh);
	static bool			IsVehicleInteresting					(const CVehicle* const pVeh);
	static void			ClearInterestingVehicles				(bool force = true);
	static bool			HasInterestingVehicle					();
    static bool         HasInterestingVehicleStreamedIn         ();
	static u32			GetInterestingVehicleClearTime			();
	static CVehicle*	SetInterestingVehicle					(CVehicle* newVeh);
	static CVehicle*	GetInterestingVehicle					();
	static s32			GetOffscreenRemovalDelayMin				();
	static s32			GetOffscreenRemovalDelayMax				();
	static void			LoadFlightZones							(const char* fileName);
	static bool			GetFlightHeightsFromFlightZones			(const Vector3& pos, Vector2& minMaxHeight_out);
	static void			LoadMarkupSpheres						();

	static void			InitPlayersRoadModifiers();
	static void			ModifyPlayersRoadModifiers();
	static void			UpdateMaxVehiclePopulationCyclesPerFrame();

	static void			InitVehicleSpacings();
	static float		GetVehicleSpacingsMultiplier			(const float fRangeScale, float * fPopCycleMult=NULL, float * fTotalRandomVehicleDensityMult=NULL, float * fRangeScaleMult=NULL, float * fWantedLevelMult=NULL);
	static void			UpdateCurrentVehicleSpacings			(float fRangeScale = -1.0f);
	static void			LoadStreetVehicleAssociations			(const char * pFilename);

	static float		GetDesiredNumberAmbientVehicles(int * outUpperLimit=NULL, float * outDesertedStreetsMult=NULL, float * outInteriorMult=NULL, float * outWantedMult=NULL, float * outCombatMult=NULL, float * outHighwayMult=NULL, float * outBudgetMult=NULL);		// uber calculation which returns the number of wandering vehicles desired
	static s32			GetDesiredNumberAmbientVehicles_Cached() { return ms_desiredNumberAmbientVehicles; }

	// Individual vehicle creation, removal, and conversion functions.
	static void			TryToGenerateOneRandomVeh				(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale);
	static CVehicle*    GenerateOneCriminalVehicle				(CNodeAddress CopSpawnNode, CNodeAddress CopSpawnPreviousNode, bool bCanDoDriveBys);
	static CVehicle*    GenerateCopVehicleChasingCriminal		(const Vector3& position, fwModelId iModelId, CNodeAddress& SpawnNode, CNodeAddress& SpawnPreviousNode);
	static CVehicle*	GenerateOneEmergencyServicesCar			(const Vector3& popCtrlCentre, const float rangeScale, fwModelId modelId, const Vector3& targetPos, const float minCreationDist, const bool bIgnoreDensity, Vector2 vCreationDir = Vector2(0.707f, 0.707f), float fCreationDot = -1.0f, bool bIncludeSwitchedOff = false, bool bAllowedAgainstFlowOfTraffic = true);
	static CHeli*		GenerateOneHeli							(u32 modelIndex, u32 driverPedIndex = fwModelId::MI_INVALID, bool bFadeIn = false);
	static u32			AddDriverAndPassengersForVeh			(CVehicle* pNewVehicle, s32 MinPassengers, s32 MaxPassengers, bool bUseExistingNodes = false);
	static u32			AddDriverAndPassengersForTrain		(CTrain*   pTrain, s32 iMinPassengers = 0, s32 iMaxPassengers = 99, bool bUseExistingNodes = false);
	static bool			AddPoliceVehOccupants					(CVehicle* pNewVehicle, bool bAlwaysCreatePassenger = false, bool bUseExistingNodes = false, u32 desiredModel = fwModelId::MI_INVALID, CIncident* pDispatchIncident=NULL, DispatchType iDispatchType = (DispatchType)0);	
	static bool			AddSwatAutomobileOccupants				(CVehicle* pNewVehicle, bool bAlwaysCreatePassenger = false, bool bUseExistingNodes = false, u32 desiredModel = fwModelId::MI_INVALID, CIncident* pDispatchIncident=NULL, DispatchType iDispatchType = (DispatchType)0, s32 pedGroupIndex = -1);	
	static u32			GetNumRequiredPoliceVehOccupants		(u32 modelIndex);
	static void			RemoveVeh								(CVehicle* pVehicle, bool cached = true, s32 iRemovalReason=Removal_None);
	static bool			VehCanBeRemoved							(const CVehicle* const pVeh);
	static void			UseTempPopControlPointThisFrame			(const Vector3& pos, const Vector3& dir, const Vector3& vel, float fov, LocInfo locationInfo);

	static bool			ScriptGenerateVehCreationPosFromPaths	(const Vector3& targetPos, float DesiredHeadingDegrees, float DesiredHeadingToleranceDegrees, float MinCreationDistance, bool bIncludeSwitchedOffNodes, bool bNoWater, bool bAllowAgainstTraffic, Vector3& pResult, Vector3& pResultLinkDir);
	static u32			ChooseModel								(bool* bPolice, const Vector3& pos, bool bNoCops, bool bSmallWorkers, bool bOnHighway, const u32 iStreetNameHash, bool offroad, const bool bNoBigVehicles, const int iNumLanesOurWay);
	static void			MakeVehicleIntoDummyBasedOnDistance		(CVehicle& rVehicle, Vec3V_In vPosition, bool& bSuperDummyConversionFailedOut);
	
	static bool			ShouldRemoveVehicle					(CVehicle *pVehicle, const Vector3& popCtrlCentre, const Vector4 & vViewPlane, const float rangeScale, const float removalTimeScale, const bool bAircraftPopCentre, EVehLastRemovalReason& removalReason_Out, s32& newDelayedRemovalTime_Out);

	static void			RegisterCloneSpawnRejection();
	static bool			CanRejectNetworkSpawn(const Vector3 &vPosition, float fRadius, bool &bForceFadeIn);
	static bool			AllowNetworkSpawnOverride(const Vector3& vehicleposition);
	static void			ResetNetworkVisibilityFail();

	static void			SetAmbientVehicleNeonsEnabled(bool enabled) { ms_ambientVehicleNeonsEnabled = enabled; }
	static bool			GetAmbientVehicleNeonsEnabled() { return ms_ambientVehicleNeonsEnabled; }
	static void			EnableVehicleNeonsIfAppropriate			(CVehicle *pVehicle);

	static float		GetVisibleCloneCreationDistance();

	// Emergency services car creation
	struct GenerateOneEmergencyServicesCarInput
	{
		GenerateOneEmergencyServicesCarInput(const Vector3& popCtrlCentre, const float rangeScale, fwModelId modelId, const Vector3& targetPos, const float minCreationDist, const bool bIgnoreDensity)
		: m_vPopCtrlCentre(popCtrlCentre)
		, m_fRangeScale(rangeScale)
		, m_iModelId(modelId)
		, m_vTargetPos(targetPos)
		, m_fMinCreationDist(minCreationDist)
		, m_bIgnoreDensity(bIgnoreDensity)
		, m_vCreationDir(0.707f, 0.707f)
		, m_fCreationDot(-1.0f)
		, m_bIncludeSwitchedOff(false)
		, m_bAllowedAgainstFlowOfTraffic(true)
		{
		
		}
		
		Vector3		m_vPopCtrlCentre;
		float		m_fRangeScale;
		fwModelId	m_iModelId;
		Vector3		m_vTargetPos;
		float		m_fMinCreationDist;
		bool		m_bIgnoreDensity;
		Vector2		m_vCreationDir;
		float		m_fCreationDot;
		bool		m_bIncludeSwitchedOff;
		bool		m_bAllowedAgainstFlowOfTraffic;
	};
	static CVehicle* GenerateOneEmergencyServicesCar(const GenerateOneEmergencyServicesCarInput& rInput);
	
	struct GenerateOneEmergencyServicesCarWithRoadNodesInput
	{
		GenerateOneEmergencyServicesCarWithRoadNodesInput(CNodeAddress node, CNodeAddress previousNode, fwModelId iModelId)
		: m_Node(node)
		, m_PreviousNode(previousNode)
		, m_iModelId(iModelId)
		, m_bPulledOver(false)
		, m_bFacePreviousNode(true)
		, m_bUseBoundingBoxesForCollision(false)
		, m_bTryToClearAreaIfBlocked(false)
		, m_bSwitchEngineOn(true)
		, m_bKickStartVelocity(true)
		, m_bPlaceOnRoadProperlyCausesFailure(true)
		{
		
		}
		
		CNodeAddress	m_Node;
		CNodeAddress	m_PreviousNode;
		fwModelId		m_iModelId;
		bool			m_bPulledOver;
		bool			m_bFacePreviousNode;
		bool			m_bUseBoundingBoxesForCollision;
		bool			m_bTryToClearAreaIfBlocked;
		bool			m_bSwitchEngineOn;
		bool			m_bKickStartVelocity;
		bool			m_bPlaceOnRoadProperlyCausesFailure;
	};
	static CVehicle* GenerateOneEmergencyServicesCarWithRoadNodes(const GenerateOneEmergencyServicesCarWithRoadNodesInput& rInput);
	
	struct GenerateOneEmergencyServicesVehicleWithMatrixInput
	{
		GenerateOneEmergencyServicesVehicleWithMatrixInput(const Matrix34& mTransform, const fwModelId modelId)
		: m_mTransform(mTransform)
		, m_ModelId(modelId)
		, m_bUseBoundingBoxesForCollision(false)
		, m_pExceptionForCollision(NULL)
		, m_nEntityOwnedBy(ENTITY_OWNEDBY_POPULATION)
		, m_nPopType(POPTYPE_RANDOM_AMBIENT)
		, m_bTryToClearAreaIfBlocked(false)
		, m_bSwitchEngineOn(true)
		, m_bKickStartVelocity(true)
		, m_bPlaceOnRoadProperlyCausesFailure(true)
		, m_bGiveDefaultTask(true)
		{}
		
		Matrix34		m_mTransform;
		fwModelId		m_ModelId;
		bool			m_bUseBoundingBoxesForCollision;
		const CEntity*	m_pExceptionForCollision;
		eEntityOwnedBy	m_nEntityOwnedBy;
		ePopType		m_nPopType;
		bool			m_bTryToClearAreaIfBlocked;
		bool			m_bSwitchEngineOn;
		bool			m_bKickStartVelocity;
		bool			m_bPlaceOnRoadProperlyCausesFailure;
		bool			m_bGiveDefaultTask;
	};
	static CVehicle* GenerateOneEmergencyServicesVehicleWithMatrix(const GenerateOneEmergencyServicesVehicleWithMatrixInput& rInput);
	
	static bool IsPositionClearToSpawnEmergencyServicesVehicle(const Vector3& vPosition, float fRadius, bool bUseBoundBoxes, const CEntity* pException);

	// Helper functions for scripting.
	static bool			CanGenerateMissionCar					();
	static CVehicle*	CreateCarForScript						(u32 ModelIndex, const Vector3& vecPosition, float fHeading, bool bRegisterAsNetworkObject, scrThreadId iMissionScriptId, bool bIgnoreGroundCheck = false);
	static void			SetCoordsOfScriptCar					(CVehicle* pVehicle, float NewX, float NewY, float NewZ, bool bClearCarOrientation, bool bAddOffset, bool bWarp=true);
	static	CVehicle*	ScriptGenerateOneEmergencyServicesCar	(u32 ModelIndex, const Vector3& TargetCoors, const bool bPedsWalkToLocation = false);
	static void			SetScriptCamPopulationOriginOverride	();
	static void			InstantFillPopulation					();
	static bool			IsInstantFillPopulationActive			();
	static bool			HasInstantFillPopulationFinished		();
	static void			SetScriptedRangeMultiplier				(const float f) { ms_scriptedRangeMultiplier = f; }
	static float		GetScriptedRangeMultiplier				() { return ms_scriptedRangeMultiplier * ms_scriptedRangeMultiplierThisFrame; }

	static void			SetScriptedRangeMultiplierThisFrame		(const float f) 
	{
		if(ms_scriptedRangeMultiplierThisFrameModified)
		{
			ms_scriptedRangeMultiplierThisFrame = Min(ms_scriptedRangeMultiplierThisFrame, f);
		}
		else
		{
			ms_scriptedRangeMultiplierThisFrame = f;
			ms_scriptedRangeMultiplierThisFrameModified = true;
		}
	}

	static void			SetScriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame(bool bEnableThisFrame) { ms_scriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame = bEnableThisFrame; }
	static void			SetScriptCapEmptyCrowdingPlayersInNetGamesThisFrame(UInt32 capNum) { ms_scriptCapEmptyCrowdingPlayersInNetGamesThisFrame = capNum; }

	static void			SetRendererRangeMultiplierOverrideThisFrame(const float fRendererRangeMultiplierOverrideThisFrame) { ms_fRendererRangeMultiplierOverrideThisFrame = fRendererRangeMultiplierOverrideThisFrame; }

	static void			ResetPerFrameModifiers();

	static bool			HasPopGenShapeBeenInitialized() { return ms_popGenKeyholeShapeInitialized; }
	static const CPopGenShape& GetPopGenShape() { return ms_popGenKeyholeShape; }

	// Helper functions for network.
	static void			NetworkRegisterAllVehicles				();
	static void			StopSpawningVehs						();
	static void			StartSpawningVehs						();
	static void			RemoveAllVehsHard						();
	static void			RemoveAllVehsSoft						();
	static void			RemoveAllVehsSoftExceptFocus			();
	static void			RemoveAllVehsHardExceptNearFocus		();
	static void			RemoveFocusVehicleSoft					();
	static void			RemoveAllVehs							(bool respectCanBeDeleted = true, bool bAllExpectFocus = false);
	static void			RemoveAllVehsWithException				(CVehicle* exception);
	static bool			HasPriorityOverNearbyPlayers			(const Vector3& nodeCoords, u32 nodeIndex, float rangeScale);
    static void         RemoveExcessNetworkVehicle              (CVehicle *pVehicle);

	static void			IncrementNumVehiclesCreatedThisCycle();
	static void			IncrementNumPedsCreatedThisCycle();
	static s32			GetNumExcessEntitiesCreatedThisFrame() { return ms_iNumExcessEntitiesCreatedThisFrame; }
	static void			IncrementNumVehiclesDestroyedThisCycle();
	static void			IncrementNumNetworkVehiclesCreatedThisFrame();
    
    // Helper functions for ambient vehicles.
    static void			AddDrivingFlagsForAmbientVehicle(s32& iDrivingFlags, const bool bIsBike);
	static float		PickCruiseSpeedWithVehModel				(const CVehicle* pVehicle, u32 carModel);

	static CPortalTracker::eProbeType GetVehicleProbeType(CVehicle *pVehicle);

	// Associates a street name with a model - only those model types will be spawned there
	static void			AddStreetModelAssociation(const char * pStreetName, const char * pVehicleName, const Vector3 & vMin, const Vector3 & vMax);

#if __BANK
	static void			ClearCarsFromArea						();
	static void			UpdateDebugDrawing						();
	static void GetPopulationCounts(
		int & iVehiclePoolSize,
		int & iTotalNumVehicles,
		int & iNumMission,
		int & iNumParked,
		int & iNumAmbient,
		int & iNumDesiredAmbient,
		int & iNumReal,
		int & iNumRealRandomTraffic,
		int & iNumDummy,
		int & iNumSuperDummy,
		int & iNumRandomGenerated,
		int & iNumPathGenerated,
		int & iNumCargenGenerated,
		int & iNumReused,
		int & iNumInReusePool);

	static s32 GetFailString(char * failString);
#endif

#if DEBUG_DRAW
	static void GetVehicleLODCounts(bool bParked, int &iCount, int &iReal, int &iDummy, int &iSuperDummy, int &iActive, int &iInactive, int &iTimesliced );
	static void GetVehicleLODCounts(int &iCount, int &iReal, int &iDummy, int &iSuperDummy, int &iActive, int &iInactive, int &iTimesliced );
#endif //DEBUG_DRAW

	static inline float	GetAllZoneScaler() { return ms_allZoneScaler; }
	static inline float	GetRangeScaleShallowInterior() { return ms_rangeScaleShallowInterior; }
	static inline float	GetRangeScaleDeepInterior() { return ms_rangeScaleDeepInterior; }

	struct CVehGenLink
	{
	public:

		u8 m_bIsActive				:1;
		u8 m_bUpToDate				:1;
		u8 m_bOnPlayersRoad			:1;
		u8 m_bOverlappingRoads		:1;	// if set, we should be less tolerant of Z contribution from nearby nodes
		u8 m_iCategory				:4;
		u8 m_iInitialDensity		:4;	// density calculated from nodes
		u8 m_iDensity				:4; // usable density, modified by "player's road modifier"
		u8 m_iLinkLanes;
		u8 m_iLinkAttempts;
		// 4 bytes

		u16 m_fpNearbyVehiclesCount;

		s16 m_iMinX;
		s16 m_iMaxX;
		s16 m_iMinY;
		s16 m_iMaxY;
		s16 m_iMinZ;
		s16 m_iMaxZ;

		CNodeAddress m_nodeA;
		CNodeAddress m_nodeB;
#if	VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
		CNodeAddress m_junctionNode;
#endif

		float m_fScore;
		float m_fDesiredNearbyVehicleCountDelta;
		float m_fDensity;
		float m_fVehicleDensityContribution;

		//float m_fLinkLengths;

		u16 m_fpLinkLengths;
		s16 m_fpLinkGrade;
		s16 m_fpLinkHeading;


		u32 m_iLastUpdateTime;
		u32 m_preferedMi;

		void SetNearbyVehiclesCount(const float f)
		{
			Assertf(f >= 0.0f && f < 255.0f, "can't have more than 255 nearby vehicles with 8:8 fixed pt!");
			m_fpNearbyVehiclesCount = (u16) ( Clamp(f, 0.0f, 255.0f) * 255.0f);
		}
		float GetNearbyVehiclesCount() const { return ((float)m_fpNearbyVehiclesCount) / 255.0f; }

		void SetLinkLengths(const float f)
		{
			Assert(f > 0.0f && f <= 255.0f);
			m_fpLinkLengths = (u16) ( Clamp(f, 0.0f, 255.0f) * 255.0f);
		}
		float GetLinkLengths() const { return ((float)m_fpLinkLengths) / 255.0f; }

		void SetLinkGrade(const float f)
		{
			Assert(f >= -128.0f && f <= 127.0f);
			m_fpLinkGrade = (s16) ( Clamp(f, -128.0f, 127.0f) * 255.0f);
		}
		float GetLinkGrade() const { return ((float)m_fpLinkGrade) / 255.0f; }

		void SetLinkHeading(const float f)
		{
			Assertf(f >= -PI && f <= PI, "%.1f is out of range", f);
			m_fpLinkHeading = (s16) (Clamp(f, -PI, PI) * 8192.0f); // bits to spare here - no need for this kinda precision
		}
		float GetLinkHeading() const { return ((float)m_fpLinkHeading) / 8192.0f; }

		float GetVehicleDensityContributionFromLink(const CVehGenLink &from) const;

		void ComputeScore();

#if __BANK
		u16 m_iFailType;
		float m_fNumNearbyVehiclesIncludedByGrade;
		u8 m_iNumTrafficVehicles;
		
		bool m_bIsWithinKeyholeShape;

#if DR_VEHGEN_GRADE_DEBUGGING
		float m_fDrDistance;
		float m_fDrPlanarDistance;
		float m_fDrZDist;
		float m_fDrGradeOffset;
#endif // DR_VEHGEN_GRADE_DEBUGGING
#endif

#if VEHGEN_STORE_DEBUG_INFO_IN_LINKS
		u16 m_iFromIndex;	// expected to be same as m_nodeA.m_index
		u16 m_iToIndex;		// expected to be same as m_nodeB.m_index
		u16 m_iLinkRegion;
		u16 m_iLinkIndex;
		u16 m_iVehGenLink;
		u16 m_iNumLanes;
		u16 m_iNumLinksInRange;
#endif
	};

#define MAX_PLAYERS_ROAD_NODES		(256)


	static void			UpdateLinksAfterGather					();
	static void			GatherPotentialVehGenLinks(
		const Vector3& popCtrlCentre,
		const Vector2& sidewallFrustumOffset,
		float maxNodeDistBase,
		float maxNodeDist, 
		bool bUseKeyholeShape,
		float fKeyhole_InViewInnerRadius,
		float fKeyhole_InViewOuterRadius,
		float fKeyhole_BehindInnerRadius,
		float fKeyhole_BehindOuterRadius,
		const Vector3& popCtrlDir,
		float fKeyhole_CosHalfFOV,
		u32& numGenLinks,
		CVehGenLink genLinks[MAX_GENERATION_LINKS],
		float& averageLinkHeight,
		CVehGenSphere * apOverlappingAreas,
		s32 iNumOverlappingAreas
#if __SPU
		, PopMultiplierArea *PopMultiplierAreaArray,
		s32 iNumTrafficAreas
#endif //__SPU
		);
	static void			UpdateAmbientPopulationData				();
	static int			ActiveLinkScoreComparison(const void *a, const void *b);
	static void			UpdateDensities							(sScheduledLink* scheduledLinks, u32 numScheduledLinks);

#if !__FINAL
	static inline int	GetNumGenerationLinks() { return ms_numGenerationLinks; }
	static inline int	GetNumActiveLinks() { return ms_iNumActiveLinks; }
#endif
	//ms_iNumActiveLinksUsingLowerScore is the count of links activated using a modified score
	//we don't want to include them in the disparity count as this will cause positive feedback and thus create more disparities
	static s32			GetLinkDisparity() { return ms_iNumActiveLinks - ms_iNumActiveLinksUsingLowerScore - ms_iNumNormalVehiclesCreatedSinceDensityUpdate; }
	static float		GetAverageLinkDisparity() { return ms_averageGenLinkDisparity; }

	static void			HandlePopCycleInfoChange();

	static void			SetDebugOverrideMultiplier(const float value)		{ ms_debugOverrideVehDensityMultiplier = value; }
	static float		GetDebugOverrideMultiplier()						{ return ms_debugOverrideVehDensityMultiplier; }

	static void			SetScriptRandomVehDensityMultiplierThisFrame(const float value)	{ ms_scriptRandomVehDensityMultiplierThisFrame = Min(ms_scriptRandomVehDensityMultiplierThisFrame, value); }
	static void			SetScriptParkedCarDensityMultiplierThisFrame(const float value)	{ ms_scriptParkedCarDensityMultiplierThisFrame = Min(ms_scriptParkedCarDensityMultiplierThisFrame, value); }

	static void			SetRandomVehDensityMultiplier(const float value)	{ ms_randomVehDensityMultiplier = value; }
	static float		GetRandomVehDensityMultiplier(float * pBasicMult=NULL, float * pThisFrameScriptedMult=NULL, float * pMultiplayerMult=NULL);
	static float		GetTotalRandomVehDensityMultiplier() { return GetRandomVehDensityMultiplier() * ms_debugOverrideVehDensityMultiplier; }

	static void			SetParkedCarDensityMultiplier(const float value)	{ ms_parkedCarDensityMultiplier = value; }
	static float		GetParkedCarDensityMultiplier();
	static float		GetLowPrioParkedCarDensityMultiplier();
	static float		GetTotalParkedCarDensityMultiplier()				{ return GetParkedCarDensityMultiplier() * ms_debugOverrideVehDensityMultiplier; }
	static float		GetTotalLowPrioParkedCarDensityMultiplier()				{ return GetLowPrioParkedCarDensityMultiplier() * ms_debugOverrideVehDensityMultiplier; }

	static void			SetScriptDisableRandomTrainsThisFrame(const bool value)	{ ms_scriptDisableRandomTrains = value; }
	static bool			GetScriptDisableRandomTrains()						{ return ms_scriptDisableRandomTrains; }

	static float		GetCurrentVehicleSpacing(const int iDensity)		{ Assert(iDensity >= 0 && iDensity <= 15); return ms_fCurrentVehiclesSpacing[iDensity]; }
	static float		CalculateVehicleShareMultiplier			();

	// PURPOSE: Within a specific min/max area we can restrict specific models to spawning only on a specific named street
	//          See the file "steetvehicleassoc.xml"

	static bool			IsModelReservedForSpecificStreet(const u32 iModelIdx, const Vector3 & vPos);

#if VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
	private:
	static void 		CacheNextJunctionNode(CVehGenLink& genLink, const CPathNodeLink* nodeLink, const CPathNode* pOriginNode);
#endif

private:
	static float		ms_debugOverrideVehDensityMultiplier;
	static float		ms_randomVehDensityMultiplier;
	static float		ms_parkedCarDensityMultiplier;
	static float		ms_lowPrioParkedCarDensityMultiplier;
	static float		ms_scriptRandomVehDensityMultiplierThisFrame;
	static float		ms_scriptParkedCarDensityMultiplierThisFrame;
	static float		ms_fRendererRangeMultiplierOverrideThisFrame;
	static bool			ms_scriptDisableRandomTrains;
	static bool			ms_scriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame;
	static UInt32		ms_scriptCapEmptyCrowdingPlayersInNetGamesThisFrame;

public:
	static s32			ms_maxNumberOfCarsInUse;
	static float		ms_vehicleMemoryBudgetMultiplier;
	static s32			ms_desiredNumberAmbientVehicles;
	static float		ms_CloneSpawnValidationMultiplier;
	static float		ms_CloneSpawnSuccessCooldownTimerScale;
	static float		ms_CloneSpawnMultiplierFailScore;

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)	
	static float		ms_vehicleMemoryBudgetMultiplierNG;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

	static float		ms_pedMemoryBudgetMultiplier;
	static bank_bool		ms_bGenerateCarsWaitingAtLights;
	static bank_bool		ms_bGenerateCarsWaitingAtLightsOnlyAtRed;
	static bank_s32	ms_iMaxCarsForLights;
	static s32			ms_numRandomCars;
	static s32			ms_numMissionCars;
	static s32			ms_numParkedMissionCars;
	static s32			ms_numParkedCars;
	static s32			ms_numLowPrioParkedCars;
	static s32			ms_numCarsInReusePool;
	static s32			ms_numStationaryAmbientCars;
	static s32			ms_numAmbientCarsOnHighway;

	static s32			ms_overrideNumberOfParkedCars;
	static bool			ms_bVehIsBeingCreated;
	static bool			ms_bUseExistingCarNodes;
	static bool			ms_bMadDrivers;

	static bank_bool	ms_bEnableUseGatherLinksKeyhole;
	static bank_float	ms_fZoomThicknessScale;
	static bank_float	ms_fMaxRangeScale;
	static bank_float	ms_fGatherLinksUseKeyHoleShapeRangeThreshold;
	static u32			ms_lastGenLinkAttemptCountUpdateMs;
	static u32			ms_GenerateVehCreationPosFromPathsUpdateTime;

	static bank_float	ms_fMPScriptDirectorRenderingMultiplier;

	static float		ms_fAircraftSpawnRangeMult;
	static float		ms_fAircraftSpawnRangeMultMP;
	static bank_bool	ms_bIncreaseSpawnRangeBasedOnHeight;
	static bank_bool	ms_bZoneBasedHeightRange;
	static bank_float	ms_fHeightBandStartZ;
	static bank_float	ms_fHeightBandEndZ;
	static bank_float	ms_fHeightBandMultiplier;

	static bool			ms_bGenerationSkippedDueToNoLinksWithDisparity;
	static s32			ms_iNumNormalVehiclesCreatedSinceDensityUpdate;

	static s32			ms_iAmbientVehiclesUpperLimit;
	static s32			ms_iAmbientVehiclesUpperLimitMP;
	static s32			ms_iParkedVehiclesUpperLimit;

	static float		ms_fLocalAmbientVehicleBufferMult;
	static float		ms_fMaxLocalParkedCarsMult;
	static bool			ms_bNetworkPopCheckParkedCarsToggle;
	static bool			ms_bNetworkPopMaxIfCanUseLocalBufferToggle;

	static bool			ms_useTempPopControlPointThisFrame;
	static CPopCtrl		ms_tmpPopOrigin;
	static CPopCtrl		ms_lastPopCenter;
	static s8			ms_bInstantFillPopulation;
	static bank_float	ms_fInstantFillExtraPopulationCycles;
	static float		ms_TempPopControlSphereRadius;

#if __BANK
	static float		ms_popGenDensityMaxExpectedVehsNearLink;
	static bool			ms_bDisplayAbandonedRemovalDebug;
#endif // __BANK

	static int			ms_iNumVehiclesInRangeForAggressiveRemovalCarjackMission;
	static float		ms_iRadiusForAggressiveRemoalCarjackMission;

	static bool			ms_bAllowRandomBoats;
	static bool			ms_bAllowRandomBoatsMP;
	static bool			ms_bDefaultValueAllowRandomBoats;
	static bool			ms_bDefaultValueAllowRandomBoatsMP;
	static bool			ms_bAllowGarbageTrucks;
	static bool			ms_bDesertedStreetMultiplierLocked;
	static bool			ms_bAllowEmergencyServicesToBeCreated;
	static RegdVeh		ms_pInterestingVehicle;	// We only have one interesting	vehicle (players last car)
	static u32			ms_timeToClearInterestingVehicle;
	static u32			ms_clearInterestingVehicleDelaySP;
	static u32			ms_clearInterestingVehicleDelayMP;
	static float		ms_clearInterestingVehicleProximitySP;
	static float		ms_clearInterestingVehicleProximityMP;

	static bool			ms_extendDelayWithinOuterRadius;
	static u32			ms_extendDelayWithinOuterRadiusTime;
	static float		ms_extendDelayWithinOuterRadiusExtraDist;

	static float		ms_NetworkVisibleCreationDistMax;
	static float		ms_NetworkVisibleCreationDistMin;
	static float		ms_FallbackNetworkVisibleCreationDistMax;
	static float		ms_FallbackNetworkVisibleCreationDistMin;
	static float		ms_NetworkOffscreenCreationDistMax;
	static float		ms_NetworkOffscreenCreationDistMin;
	static float		ms_NetworkCreationDistMinOverride;

	static bool			ms_NetworkUsePlayerPrediction;
	static bool         ms_NetworkShowTemporaryRealVehicles;
	static float		ms_NetworkPlayerPredictionTime;

	static u32			ms_NetworkMaxTimeVisibilityFail;
	static u32			ms_NetworkCloneRejectionCooldownDelay;
	static u32			ms_NetworkTimeVisibilityFail;
	static u32			ms_NetworkTimeExtremeVisibilityFail;
	static u32			ms_NetworkVisibilityFailCount;
	static u32			ms_NetworkMinVehiclesToConsiderOverrideMin;
	static u32			ms_NetworkMinVehiclesToConsiderOverrideMax;

	static u32			ms_lastPopCycleChangeTime;
	static u32			ms_popCycleChangeBufferTime;
	static u32			ms_lastChosenVehicleModel;
	static u32			ms_sameVehicleSpawnLimit;
	static u32			ms_sameVehicleSpawnCount;
	static u32			ms_lastSpawnedNeonIdx;
	static RegdVeh		ms_pLastSpawnedNeonVeh;
	static bool			ms_ambientVehicleNeonsEnabled;
	static float		ms_spawnLastChosenVehicleChance;
	static Vec3V		ms_stationaryVehicleCenter;
#if __BANK
	static char			ms_VehGroupOverride[64];
	static int			ms_VehGroupOverridePercentage;
	static char			ms_overrideArchetypeName[16]; 
	static bool			ms_noPoliceSpawn;
#endif

#if	__BANK

	static bool			ms_displayVehicleDirAndPath;
	enum VehPopEventType
	{
		VehPopFailedCreate,
		VehPopCreate,
		VehPopCreateEmegencyVeh,
		VehPopConvertToDummy,
		VehPopConvertToReal,
		VehPopDestroy
	};
	static void VecMapDisplayVehiclePopulationEvent(const Vector3& pos, VehPopEventType eventType);
#endif // __DEV

	enum EVehGenFailType
	{
		Fail_None,
		Fail_InViewFrustum,
		Fail_NoSpawnAttemptedYet,
		Fail_NoCreationPositionFound,
		Fail_LinkBetweenNodesNotFound,
		Fail_NoAppropriateModel,
		Fail_SameVehicleModelAsLastTime,
		Fail_NetworkRegister,
		Fail_PoliceBoat,
		Fail_Boat,
		Fail_AgainstFlowOfTraffic,
		Fail_MatrixNotAlignedWithTraffic,
		Fail_AmbientBoatAlreadyHere,
		Fail_DeadEnd,
		Fail_RegionNotLoaded,
		Fail_SomeKindOfWaterLevelFuckup,
		Fail_CouldntPlaceBike,
		Fail_CouldntPlaceCar,
		Fail_TooCloseToNetworkPlayer,
		Fail_NotNetworkTurnToCreateVehicleAtPosition,
		Fail_OutsideCreationDistance,
		Fail_RelativeSpeedCheck,
		Fail_CollisionBoxCheck,
		Fail_CouldntAllocateVehicle,
		Fail_CouldntAllocateDriver,
		Fail_NotStreamedIn,
		Fail_SingleTrackPotentialCollision,
		Fail_FallbackPedNotAvailable,
		Fail_PopulationIsFull,
		Fail_NodesSwitchedOff,
		Fail_CollidesWithTrajectoryOfMarkedVehicle,
		Fail_LinkDirectionInvalid,
        Fail_OutOfLocalPlayerScope
	};

#if __BANK
	// counters used to track reasons for failing to create random cars
	struct VehiclePopulationFailureData
	{
		VehiclePopulationFailureData()
		{
			Reset(0);
		}

		void Reset(u32 newTime)
		{
			m_counterTimer = newTime;
			m_numNoCreationPosFound = 0;
			m_numNoLinkBetweenNodes = 0;
			m_numChosenModelNotLoaded = 0;
			m_numSpawnLocationInViewFrustum = 0;
			m_numSingleTrackRejection = 0;
			m_numTrajectoryOfMarkedVehicleRejection = 0;
			m_numNoVehModelFound = 0;
			m_numNetworkPopulationFail = 0;
			m_numVehsGoingAgainstTrafficFlow = 0;
			m_numDeadEndsChosen = 0;
			m_numNoPathNodesAvailable = 0;
			m_numGroundPlacementFail = 0;
			m_numGroundPlacementSpecialFail = 0;
			m_numNetworkVisibilityFail = 0;
			m_numPointWithinCreationDistFail = 0;
			m_numPointWithinCreationDistSpecialFail = 0;
			m_numRelativeMovementFail = 0;
			m_numBoundingBoxFail = 0;
			m_numBoundingBoxSpecialFail = 0;
			m_numVehicleCreateFail = 0;
			m_numDriverAddFail = 0;
			m_numFallbackPedNotAvailable = 0;
			m_numPopulationIsFull = 0;
			m_numNodesSwitchedOff = 0;
			m_numIdenticalModelTooClose = 0;
			m_numNotNetworkTurnToCreateVehicleAtPos = 0;
			m_sameVehicleModelAsLastTime = 0;
			m_numChosenBoatModelNotLoaded = 0;
			m_numRandomBoatsNotAllowed = 0;
			m_numBoatAlreadyHere = 0;
			m_numWaterLevelFuckup = 0;
			m_numLinkDirectionInvalid = 0;
            m_numOutOfLocalPlayerScope = 0;
		}

		u32 m_counterTimer; // used to reset counters every second
		u32 m_numNoCreationPosFound; // used to count failures due to no creation positions being found
		u32 m_numNoLinkBetweenNodes;
		u32 m_numChosenModelNotLoaded;
		u32 m_numSpawnLocationInViewFrustum;
		u32 m_numSingleTrackRejection;
		u32 m_numTrajectoryOfMarkedVehicleRejection;
		u32 m_numNoVehModelFound; // used to count failures due to no car model being available
		u32 m_numNetworkPopulationFail; // used to count failures due to network population failures
		u32 m_numVehsGoingAgainstTrafficFlow; // used to count failures due to to cars going against the traffic flow
		u32 m_numDeadEndsChosen; // used to count failures due to dead ends being chosen
		u32 m_numNoPathNodesAvailable; // used to count failures due to no path nodes being available
		u32 m_numGroundPlacementFail; // used to count failures due to ground placement failures
		u32 m_numGroundPlacementSpecialFail;
		u32 m_numNetworkVisibilityFail; // used to count failures due to network visibility failures
		u32 m_numPointWithinCreationDistFail; // used to count failures due to creation distance failures
		u32 m_numPointWithinCreationDistSpecialFail;
		u32 m_numRelativeMovementFail; // used to count failures due to relative movement failures
		u32 m_numBoundingBoxFail; // used to count failures due to bounding box check failures
		u32 m_numBoundingBoxSpecialFail;
		u32 m_numVehicleCreateFail; // used to count failures due to vehicle creation failures
		u32 m_numDriverAddFail; // used to count failures due to failing to add a driver
		u32 m_numFallbackPedNotAvailable;
		u32 m_numPopulationIsFull;
		u32 m_numNodesSwitchedOff;
		u32 m_numIdenticalModelTooClose;
		u32 m_numNotNetworkTurnToCreateVehicleAtPos;
		u32 m_sameVehicleModelAsLastTime;
		u32 m_numChosenBoatModelNotLoaded;
		u32 m_numRandomBoatsNotAllowed;
		u32 m_numBoatAlreadyHere;
		u32 m_numWaterLevelFuckup;
		u32 m_numLinkDirectionInvalid;
        u32 m_numOutOfLocalPlayerScope;
	};
	static VehiclePopulationFailureData m_populationFailureData;

    static const char *GetLastRemovalReason();
#endif // __BANK

	struct FlightZone
	{
		Vector3 m_min;
		Vector3 m_max;
	};
	static atArray<FlightZone> sm_FlightZones;

	static atArray<CNodeAddress>	sm_OpenNodes;
	static atArray<CNodeAddress>	sm_ClosedNodes;
	static atArray<float>			sm_NodesTotalDistance;
	static atArray<float>			sm_NodesTotalDesiredNumVehs;
	
#if __BANK
	static bool		ms_bEverybodyCruiseInReverse;
	static bool		ms_bDontStopForPeds;
	static bool		ms_bDontStopForCars;
	static s32		ms_overrideNumberOfCars;
	static bool		ms_ignoreOnScreenRemovalDelay;

	static void AddVehicleToSpawnHistory(CVehicle * pVehicle, CVehiclePopulation::CVehGenLink * pSpawnLink, const char * pStaticText);

	static bool		ms_bCatchVehiclesRemovedInView;
	static float	ms_fInViewDistanceThreshold;
	static void CatchVehicleRemovedInView(CVehicle * pVehicle, s32 iRemovalReason);
#endif //__BANK

	static u32 &ms_numGenerationLinks;
	static u32 ms_numActiveVehGenLinks;
	static float ms_averageLinkHeight;
	static s32 ms_iActiveLinkUpdateFreqMs;
	static u32 ms_iNumMarkUpSpheres;

	struct CAmbientPopulationVehData
	{
		Vec3V			m_Pos;
		Vec3V			m_SpawnPos;
		float			m_fHeading;
		float			m_fSpeed;
		u32				m_modelIndex;
	};
	static atArray<CAmbientPopulationVehData> ms_aAmbientVehicleData;

	static CVehGenLink	ms_aGenerationLinks[MAX_GENERATION_LINKS] ALIGNED(128);

	static CVehGenMarkUpSpheres ms_VehGenMarkUpSpheres;

	// 1/spacing values as used by the vehicle generation code
	static float		ms_fVehiclesPerMeterDensityLookup[16] ;
	// Default spacings in cars/meter, prior to any scaling or modifiers are applied
	static float		ms_fDefaultVehiclesSpacing[16];
	// Current spacings modified by scripted or debug scales, etc
	static float		ms_fCurrentVehiclesSpacing[16] ;

	static float		ms_fCurrentVehicleSpacingMultiplier;

	static s32			ms_iPlayerRoadDensityInc[16] ;
	static s32			ms_iNonPlayerRoadDensityDec[16] ;

	static float		GetDesiredNumVehiclesPerLanePerMeter(const float fDensity);

	static CNodeAddress ms_aNodesOnPlayersRoad[MAX_PLAYERS_ROAD_NODES];
	static s32 ms_iNumNodesOnPlayersRoad;

	static RegdVeh ms_PlayersJunctionNodeVehicle;
	static CNodeAddress ms_PlayersJunctionNode;
	static CNodeAddress ms_PlayersJunctionEntranceNode;

	static float		ms_fPlayersRoadChangeInDirectionPenalties[NUM_PLAYERS_ROAD_CID_PENALTIES][NUM_PLAYERS_ROAD_CID_VALUES];

	static u32			GetPopulationFrameRate() { return NetworkInterface::IsGameInProgress() ? ms_iVehiclePopulationFrameRateMP : ms_iVehiclePopulationFrameRate; }
	static u32			GetPopulationCyclesPerFrame() { return NetworkInterface::IsGameInProgress() ? ms_iVehiclePopulationCyclesPerFrameMP : ms_iVehiclePopulationCyclesPerFrame; }
	static u32			GetMaxPopulationCyclesPerFrame() { return NetworkInterface::IsGameInProgress() ? ms_iMaxVehiclePopulationCyclesPerFrameMP : ms_iMaxVehiclePopulationCyclesPerFrame; }
	static bool			GetFrameCompensatedThisFrame() { return ms_bFrameCompensatedThisFrame; }

private:
	static u32			ms_iVehiclePopulationFrameRate;
	static u32			ms_iVehiclePopulationCyclesPerFrame;
	static u32			ms_iMaxVehiclePopulationCyclesPerFrame;
	static u32			ms_iVehiclePopulationFrameRateMP;
	static u32			ms_iVehiclePopulationCyclesPerFrameMP;
	static u32			ms_iMaxVehiclePopulationCyclesPerFrameMP;
	static bool			ms_bFrameCompensatedThisFrame;


	struct tDeletionEntry
	{
		CVehicle* pVehicle;
		float fDist;
		float fScore;
		bool bLocalVisible;
		bool bRemoteVisible;
		bool bPassive;
		bool bAggressive;

		tDeletionEntry() : pVehicle(NULL), bLocalVisible(false), bRemoteVisible(false), bPassive(false), bAggressive(false), fDist(0.0f), fScore(0.0f) 
		{}

		float GetScore() const { return fScore; }
		bool IsVisible() const { return bLocalVisible || bRemoteVisible; }

		static int Sort(const tDeletionEntry *a, const tDeletionEntry *b);
	};

	//	Population control functions.
	static void			RemoveExcessEmptyCopAndWreckedVehs		();
	static void			RemoveExcessEmptyCopAndWreckedVehs_MP	();
	static void			RemoveEmptyOrPatrollingLawVehs			(const Vector3& popCtrlCentre);
	static bool			ShouldEmptyCopOrWreckedVehBeRemovedAggressively(const CVehicle& rVehicle, float fDistSq, float fRemovalDistance);
	static bool			ShouldEmptyCopOrWreckedVehBeRemovedPassively(const CVehicle& rVehicle, float fDistSq, float fRangeScalar);
	static void			RemoveCongestionInNetGames				();
	static void			RemoveEmptyCrowdingPlayersInNetGames	();
	static int			CountVehsInArea							(const Vector3& Centre, float range);
	static void			RemoveDistantVehs						(const Vector3& popCtrlCentre, const float popCtrlHeading, const float rangeScale, const float removalTimeScale);
	static void			AddVehicleToReusePool					(CVehicle* pVehicle);
	static bool			IsVehicleWreckedOrEmpty(CVehicle *pVeh, float fDist, bool bRespawnDeletion);
	static float		GetDeletionScoreForVehicle(tDeletionEntry &entry, float fPassiveRejectionDist, float fAggressiveRejectionDist);

	static const int MAX_CARS_TO_STORE_MP_DELETE = 150;
	BANK_ONLY(static void DumpExcessEmptyCopAndWreckedListToLog(atFixedArray<tDeletionEntry, MAX_CARS_TO_STORE_MP_DELETE> &apCars));
	BANK_ONLY(static void ValidateVehicleCounts());
	
public:
	BANK_ONLY(static void DumpVehiclePoolToLog());

	static float		GetDistanceFromAllPlayers(const Vector3 &popCentre, CEntity *pEntity, CPed *pIgnorePed = NULL);
	static bool			IsEntityVisibleToAnyPlayer(const CDynamicEntity *pEntity, const CPed *pIgnorePed = NULL, float fDistance = 200.0f);
	static bool			IsEntityVisibleToRemotePlayer(const CEntity *pEntity, const CPed *pIgnorePed = NULL, float fDistance = 200.0f);

	static void			RemoveEmptyWreckedVehsAndPedsOnRespawn_MP		(CPed *pRespawningPed);

	static void			ResetDesertedStreetsMultiplier();

public:

	static u32	FindVehicleToReuse								(u32 vehModel);
	static CVehicle*		GetVehicleFromReusePool				(int vehReuseIndex);
	static void			RemoveVehicleFromReusePool				(int vehReuseIndex);
	static bool			ReuseVehicleFromVehicleReusePool		(CVehicle* pVehicle, Matrix34& newMatrix, ePopType popType, eEntityOwnedBy ownedBy, bool bClone, bool bCreateAsInactive);
	static bool			WillVehicleCollideWithOtherCarUsingRestrictiveChecks(const Matrix34& vehMatrix, const Vector3& vBoundBoxMin, const Vector3& vBoundBoxMax);

private:
	static void			UpdateVehicleReusePool					();
	static void			FlushVehicleReusePool					();
	static void			DropOneVehicleFromReusePool				();
	static bool			CanVehicleBeReused						(CVehicle* pVehicle);
	//static void			GetAssociatedPathLinks_FloodFill		(u32 startVehGenLinkIndex, bool UNUSED_PARAM(bIncludeSwitchedOffNodes), s32 & numPathLinksInRange, const s32 iMaxLinks);
	static void			UpdateCurrentVehGenLinks				();
	static void			UpdateKeyHoleShape						(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float popCtrlBaseHeading, const float popCtrlFov, const float popCtrlTanHFov, const float fRangeScale);

	static bool			IsSphereVisibleInGameViewport			(const Vector3 & vPos, const float fRadius);
	static bool			IsLinkVisibleInGameViewport				(const Vector3 & vNodePos1, const Vector3 & vNodePos2, const int iNumLanes);

	// No calculations required -- links no longer make it into the active links list if they don't have a disparity
	static bool			DoAnyVehGenLinksHaveADisparity			(void) { return ms_bInstantFillPopulation || ms_iCurrentActiveLink < ms_iNumActiveLinks; }
	static void			RegisterVehiclePopulationFailure		(BANK_ONLY(const u32 iLinkIndex, const Vector3& vPosition, const u16 iFailureType, u32& iFailureCount));
	static void			DisableCreationOnNode					(const CNodeAddress& nodeAddress);
	static void			ClearDisabledNodes						();
	static void			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
	static void			GenerateRandomVehs						(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale);
	static	void		SpawnGarbageTrucks						(const Vector3& locationCentre, const float rangeScale);
	static void			DoVehicleDummyVehicleConversions		(const Vector3& popCtrlCentre, const bool popCtrlIsInInterior, const float rangeScale);
	static s32			FindNumRandomVehsThatCanBeCreated		();

	//	Individual vehicle creation, removal, and conversion functions.
	static bool			CanGenerateRandomCar					();
	static	bool		AddAmbulanceOccupants					(CVehicle* pVehicle);
	static	bool		AddFiretruckOccupants					(CVehicle* pVehicle);
	static	bool		AddGarbageTruckOccupants				(CVehicle* pVehicle);
	static void			GenerateRandomCarsOnPaths				(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale);
	static void			PossiblyGenerateRandomCarsOnLink		(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale, const CPathNode* pNode, s32 iLink);
	static void			GenerateRandomCarsOnLink				(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale, const CPathNode* pNode, const CPathNode* pNode2, const CPathNodeLink* pLink, s32 generated, float maxDist, s32 maxNumCars);
	static void			GenerateRandomCarOnLinkPosition			(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale, const CPathNode* pNode, const CPathNode* pNode2, const CPathNodeLink* pLink, u32 lane, float pos, s32 generated);
	static bool			IsAmbientBoatNearby						(Vector3& vCoors, float range);
	static bool			IsVehicleFacingPotentialHeadOnCollision	(const Vector3& vCoors, const Vector3& vLinkDir, const CPathNodeLink* pLink, const float fRange);

	static bool			ShouldCreateCarsAtTrafficLight			(CPathNode * pTrafficLightNode, CPathNode * pJunctionNode);
	
	static void			CleanUpPlayersJunctionNode();
	static void			ScanForNodesOnPlayersRoad(bool bForce = false);
	static void			ClearOnPlayersRoadFlags();

	static void			GatherActiveLinks();
	static s32			SelectAndUpdateVehGenLinkToGenerateFrom	();

	static bool			GenerateVehCreationPosFromLinks			(Vector3* pResult, CNodeAddress* pFromNode, CNodeAddress* pToNode, float* pFraction, int* iVehGenLinkIndex, bool* bKeepTrying, const float rangeScale);
	static bool			GenerateVehCreationPosFromPaths			(const Vector3& popCtrlCentre, const float rangeScale, float DirectionX, float DirectionY, float RequiredDotProduct, bool bRequiredInside, float CreationDistOnScreen, float CreationDistOffScreen, bool bIncludeSwitchedOffNodes, bool bNoWater, Vector3* pResult, CNodeAddress* pFromNode, CNodeAddress* pToNode, float* pFraction, bool bIgnoreDensity, bool bAllowGoAgainstTraffic);

	static bool			ShouldRemoveVehicleInternal			(CVehicle *pVehicle, const Vector3& popCtrlCentre, const Vector4 & vViewPlane, float rangeScale, const float removalTimeScale, const bool bAircraftPopCentre, EVehLastRemovalReason& removalReason_Out, s32& newDelayedRemovalTime_Out);
	static int			ShouldAggressiveRemovalVehicleForCarjackingMission(CVehicle *pVehicle);
	static bool			CanTreatAsVisibleForRemoval			(CVehicle *pVehicle);

	static void			UpdateDesertedStreets					();
    static bool         IsInScopeOfLocalPlayer                  (const Vector3 &position);
    static bool         IsNetworkTurnToCreateVehicleAtPos       (const Vector3 &position);

#if	__BANK
	// Debug functions
	static void			SwitchVehToDummyCB();
	static void			SwitchVehToRealCB();
	static void			SwitchVehSuperDummyCB();
	static void			WakeUpAllParkedVehiclesCB();
#endif // __BANK

#if __BANK
	// Editing of markup volumes
	/*
	static void			AddVehGenSphere(const Vector3 & vPos, const float fRadius, const u32 iFlags);
	static void			DeleteVehGenSphere(const u32 iIndex);
	static void			SetSphereFlags(const u32 iIndex, const u32 iFlags);
	static u32			GetSphereFlags(const u32 iIndex);
	*/
	static void			RenderMarkupSpheres();
	static void			SaveMarkupSpheres();

	class CMarkUpEditor
	{
	public:
		CMarkUpEditor()
		{
			Init();
		};
		
		static int m_iCurrentSphere;
		static int m_iCurrentSphereX;
		static int m_iCurrentSphereY;
		static int m_iCurrentSphereZ;
		static int m_iCurrentSphereRadius;
		static bool m_bCurrentSphere_FlagRoadsOverlap;

		static int m_iNumSpheres;
		static bkText * m_pNumSpheresText;
		static bkSlider * m_pCurrentSphereSlider;
		static bkSlider * m_pXPosSlider;
		static bkSlider * m_pYPosSlider;
		static bkSlider * m_pZPosSlider;
		static bkSlider * m_pRadiusSlider;
		static bkToggle * m_pRoadsOverlapToggle;

		static void Init();

		static void OnChangeCurrentSphere();
		static void OnModifyCurrentSphereData();

		static void OnAddNewSphere();
		static void OnDeleteCurrentSphere();

		static void OnSelectClosestToCamera();

		static const int ms_iDefaultSphereRadius = 8;
		static const int ms_iDefaultSphereFlags = 0;
	};
	static CMarkUpEditor ms_MarkUpEditor;
#endif // __BANK

#if __BANK

    static EVehLastRemovalReason ms_lastRemovalReason;

//	static void	RegisterVehCreation						(const Matrix34& pMatrix, const CNodeAddress oldNode, const CNodeAddress newNode, const float fLinkScore);
//	static void	RenderCarCreations						();

	static void	HeavyDutyVehGenDebugging();

	static void DumpVehicleMemoryInfoCB();
#endif // __BANK

	static float GetAssociatedPathLinks_AreaSearch		(CVehGenLink& link, const Vector3 & vLinkCenter, const float fLinkHeading, const float fLinkGrade, const float fScanDistance, const bool bMatchLinkHeading, const bool bOverlappingRoads, CAmbientPopulationVehData *pVehData, u32 vehDataCount, u32& preferedMi, sScheduledLink* scheduledLinks, u32& numScheduledLinks);

public:
	static float GetCreationDistance(float rangeScale = ms_fCalculatedPopulationRangeScale, float viewRangeScale = ms_fCalculatedViewRangeScale) { return ms_creationDistance * rangeScale * viewRangeScale; }
	static float GetCreationDistanceOffScreen(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_creationDistanceOffScreen * rangeScale; }
	static float GetCreationDistMultExtendedRange() { return ms_creationDistMultExtendedRange; }
	static float GetPopulationSpawnFrustumNearDist() { return ms_fCalculatedSpawnFrustumNearDist; }
	static float GetCullRange(float rangeScale = ms_fCalculatedPopulationRangeScale, float viewRangeScale = ms_fCalculatedViewRangeScale) { return ms_cullRange * rangeScale * viewRangeScale; }
	static float GetCullRangeOnScreen(float rangeScale = ms_fCalculatedPopulationRangeScale, float viewRangeScale = ms_fCalculatedViewRangeScale) { return (ms_cullRange * rangeScale * viewRangeScale) + (ms_cullRange * rangeScale * (ms_cullRangeOnScreenScale - 1.0f)); }
	static float GetCullRangeOffScreen(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_cullRangeOffScreen * rangeScale; }
	static float GetCullRangeOffScreenBehindPlayerWhenZoomed(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_cullRangeBehindPlayerWhenZoomed * rangeScale; }
	static float GetCullRangeOffScreenPedsRemoved(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_cullRangeOffScreenPedsRemoved * rangeScale; }
	static float GetCullRangeOffScreenWrecked(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_cullRangeOffScreenWrecked * rangeScale; }
	static const CPopGenShape& GetKeyholeShape() { return ms_popGenKeyholeShape; }
	static float GetKeyholeShapeInnerBandRadiusHighPriority(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_popGenKeyholeShapeInnerBandRadiusHighPriority * rangeScale; }
	static float GetKeyholeShapeOuterBandRadiusHighPriority(float rangeScale = ms_fCalculatedPopulationRangeScale) { return ms_popGenKeyholeShapeOuterBandRadiusHighPriority * rangeScale; }

private:
	static CNodeAddress	ms_DisabledNodes[VEHGEN_MAX_DISABLED_NODES];
	static u32			ms_iNextDisabledNodeIndex;
	static u32			ms_iClearDisabledNodesTime;

	static s32			ms_iNumVehiclesCreatedThisCycle;
	static s32			ms_iNumPedsCreatedThisCycle;
	static s32			ms_iNumExcessEntitiesCreatedThisFrame;
	static s32			ms_iNumVehiclesDestroyedThisCycle;
	static s32			ms_iNumNetworkVehiclesCreatedThisFrame;
	static s32			ms_iNumWreckedEmptyCars;

	static float		ms_popGenKeyholeOuterShapeThickness;
	static float		ms_popGenKeyholeInnerShapeThickness;
	static float		ms_popGenKeyholeShapeInnerBandRadius;
	static float		ms_popGenKeyholeShapeOuterBandRadius;
	static float		ms_popGenKeyholeSideWallThickness;
	static float		ms_popGenKeyholeShapeInnerBandRadiusHighPriority;
	static float		ms_popGenKeyholeShapeOuterBandRadiusHighPriority;
	static float		ms_popGenDensitySamplingVerticalFalloffStart;
	static float		ms_popGenDensitySamplingVerticalFalloffEnd;
	static float		ms_popGenDensitySamplingMinSummedDesiredNumVehs;
	static float		ms_popGenDensitySamplingMaxExtendedDist;
	static float		ms_popGenStandardVehsPerMeterPerLane;
	static float		ms_popGenRandomizeLinkDensity;
	static float		ms_popGenOuterBandLinkScoreBoost;

	static float		ms_allZoneScaler;
	static float		ms_scriptedRangeMultiplier;
	static float		ms_scriptedRangeMultiplierThisFrame;
	static float		ms_rangeScaleShallowInterior;
	static float		ms_rangeScaleDeepInterior;
	static float		ms_creationDistance;
	static float		ms_creationDistanceOffScreen;
	static float		ms_creationDistMultExtendedRange;
	static bank_float	ms_spawnFrustumNearDistMultiplier;
	static float		ms_cullRange;
	static float		ms_cullRangeOnScreenScale;
	static float		ms_cullRangeOffScreen;
	static float		ms_cullRangeOffScreenPedsRemoved;
	static float		ms_cullRangeBehindPlayerWhenZoomed;
	static float		ms_cullRangeOffScreenWrecked;
	static float		ms_maxSpeedBasedRemovalRateScale;
	static float		ms_maxSpeedExpectedMax;
	static float		ms_densityBasedRemovalRateScale;
	static u32			ms_densityBasedRemovalTargetHeadroom;
	static s32			ms_vehicleRemovalDelayMsRefreshMin;
	static s32			ms_vehicleRemovalDelayMsRefreshMax;
	static float		ms_maxGenerationHeightDiff;
	static bool			ms_allowSpawningFromDeadEndRoads;
	static s32			ms_maxNumMissionVehs;
	static s32			ms_maxRandomVehs;
	static bool			ms_noVehSpawn;
	static u32			ms_countDownToCarsAtStart;
	static	s32			ms_numPermanentVehicles;
	static u32			ms_lastTimeLawEnforcerCreated;
	static float		ms_passengerInASeatProbability;
	static float		ms_fMinGenerationScore;
	static bank_float	ms_fMinBaseGenerationScore;
	static s32			ms_previousGenLinkDisparity[VEHGEN_NUM_AVERAGE_DISPARITY_FRAMES];
	static s32			ms_genLinkDisparityIndex;
	static float		ms_averageGenLinkDisparity;

	static bool			ms_scriptedRangeMultiplierThisFrameModified;

#if	__BANK
	static bool			ms_displayAddCullZonesOnVectorMap;
	static bool			ms_movePopulationControlWithDebugCam;
#endif // __BANK

	static bool			ms_vehiclePopGenLookAhead;

	static float		ms_fPlayersRoadScanDistance;
	static s32			ms_iCurrentActiveLink;

	static u8			ms_ReusedVehicleTimeout;
	static bool			ms_bVehicleReusePool;
	static bool			ms_bVehicleReuseInMP;
	static bool			ms_bVehicleReuseChecksHasBeenSeen;

#if GTA_REPLAY
	static bool			ms_bReplayActive;
#endif // GTA_REPLAY

#if __BANK
	static float		ms_addCullZoneHeightOffset;
	static bool			ms_bHeavyDutyVehGenDebugging;
	static bool			ms_bDrawBoxesForVehicleSpawnAttempts;
	static u32			ms_iDrawBoxesForVehicleSpawnAttemptsDuration;
	static bool			ms_displayAddCullZonesInWorld;
	static bool			ms_freezeKeyholeShape;
	static bool			ms_displaySpawnCategoryGrid;
	static bool			ms_showDebugLog;
	static bool			ms_bDisplayVehicleSpacings;
	static bool			ms_bRenderMarkupSpheres;
	static s32			ms_iNumExcessDisparateLinks;
	
	static Vector3		ms_vClearAreaCenterPos;
	static float		ms_fClearAreaRadius;
	static bool			ms_bClearAreaCheckInteresting;
	static bool			ms_bClearAreaLeaveCarGenCars;
	static bool			ms_bClearAreaCheckFrustum;
	static bool			ms_bClearAreaWrecked;
	static bool			ms_bClearAreaAbandoned;
	static bool			ms_bClearAreaEngineOnFire;
	static bool			ms_bClearAreaAllowCopRemovealWithWantedLevel;
public:
	static bool			ms_displayInnerKeyholeDisparities;
	static u32			ms_innerKeyholeDisparities;
	static bool			ms_bDebugVehicleReusePool;
	static bool			ms_bDisplayVehicleReusePool;
	static bool			ms_bDisplayVehicleCreationMethod;
	static bool			ms_bDisplayVehicleCreationCost;
	static u32			ms_iVehicleCreationCostHighlightThreshold;
	static bool			ms_bDrawMPExcessiveDebug;
	static bool			ms_bDrawMPExcessivePedDebug;

	static float ms_RangeScale_fBaseRangeScale;
	static float ms_RangeScale_fScalingFromPosInfo;
	static float ms_RangeScale_fscalingFromCameraOrientation;
	static float ms_RangeScale_fScalingFromVehicleType;
	static float ms_RangeScale_fScriptedRangeMultiplier;
	static float ms_RangeScale_fHeightRangeScale;
	static float ms_RangeScale_fRendererRangeMultiplier;
	static float ms_RangeScale_fScopeModifier;

private:

	struct sDebugMessage
	{
		char msg[256];
		char timeStr[16];
		u32 time;
	};
	static u32			ms_nextDebugMessage;
	static sDebugMessage ms_debugMessageHistory[64];

	struct sSpawnEntry
	{
		RegdVeh m_pVehicle;
		Matrix34 m_Matrix;
		Vector3 m_AABB[2];
		float m_fNumNearbyVehicles;
		u32 m_iFrame;
		CNodeAddress m_Nodes[2];
		const char * m_pStaticText;
	};
	static sSpawnEntry ms_spawnHistory[VEHICLE_POPULATION_SPAWN_HISTORY_SIZE];
	static bool ms_bDisplaySpawnHistory;
	static bool ms_bSpawnHistoryDisplayArrows;

	static void RenderSpawnHistory();
	static void ClearSpawnHistory();
#endif // __BANK

	static bool			ms_popGenKeyholeShapeInitialized;
	static CPopGenShape	ms_popGenKeyholeShape;

	static s32			ms_iNumActiveLinksUsingLowerScore;
	static s32			ms_iNumActiveLinks;
	static s32			ms_aActiveLinks[MAX_GENERATION_LINKS] ALIGNED(128);

	static bank_float	ms_fLinkAttemptPenaltyMutliplier;
	static bank_bool	ms_bSearchLinksMustMatchVehHeading;

	static bank_bool	ms_spawnDoRelativeMovementCheck;
	static bank_bool	ms_spawnCareAboutLanes;
	static bank_bool	ms_spawnCareAboutRepeatingSameModel;
	static bank_bool	ms_spawnEnableSingleTrackRejection;
	static bank_bool	ms_spawnCareAboutDeadEnds;
	static bank_bool	ms_spawnCareAboutPlacingOnRoadProperly;
	static bank_bool	ms_spawnCareAboutBoxCollisionTest;
	static bank_bool	ms_spawnCareAboutAppropriateVehicleModels;
	static bank_bool	ms_spawnCareAboutFrustumCheck;
	static bank_float	ms_fSkipFrustumCheckHeight;

	static bank_float	ms_desertedStreetsMinimumMultiplier;
	static float		ms_desertedStreetsMultiplier;
	static u32			ms_lastTimeAmbulanceCreated;
	static u32			ms_lastTimeFireTruckCreated;
	static bool			ms_bUsePlayersRoadDensityModifier;
	static float		ms_fVehiclePerMeterScaleForNonPlayersRoad;
	static float		ms_fOnPlayersRoadPrioritizationThreshold;

	//throttling vehicle generation around areas of traffic
	static bool			ms_bUseTrafficDensityModifier;
	static bool			ms_bUseTrafficJamManagement;
	static u32			ms_iMinAmbientVehiclesForThrottle;
	static float		ms_fMinRatioForTrafficThrottle;
	static float		ms_fMaxRatioForTrafficThrottle;
	static float		ms_fMinDistanceForTrafficThrottle;
	static float		ms_fMaxDistanceForTrafficThrottle;
#if	__BANK
	static bool			ms_displayTrafficCenterInWorld;
	static bool			ms_displayTrafficCenterOnVM;
#endif

	// PURPOSE:	Keeps track of the current population range scale, as computed by
	//			CalculatePopulationRangeScale(). This gets updated (currently once per
	//			frame) during CVehiclePopulation's update.
	static float		ms_fCalculatedPopulationRangeScale;
	static float		ms_fCalculatedViewRangeScale;
	static float		ms_fCalculatedSpawnFrustumNearDist;
	static bool			ms_bZoomed;
	static bool			ms_bLockPopuationRangeScale;
	static bank_float	ms_fNearDistLodScaleThreshold;

	static bool			ms_AllowPoliceDeletion;

	// Parsable class containing tunable values
	static CVehiclePopulationTuning ms_TuningData;

	// Associations between a specific streetnames and modelindex
	static const s32 ms_iMaxStreetModelAssociations = 32;

	// This structure is used to associate a given streetname with a particular vehicle model, and a min/max area
	// Within in the specified min/max, the given street will only spawn vehicles of the types defined -
	// and these vehicle types will not spawn on any other roads within that min/max region.
	struct CStreetModelAssociation
	{
		static const int ms_iMaxModels = 4;

		// The streetname 
		u32 m_iStreetNameHash;
		// How many models are associated with this street
		s32 m_iNumModels;
		// The models
		fwModelId m_VehicleModelId[ms_iMaxModels];
		// The min/max within which the vehicle models will *only* spawn on the street specified
		Vector3 m_vMin[ms_iMaxModels];
		Vector3 m_vMax[ms_iMaxModels];

		void Clear()
		{
			m_iStreetNameHash = m_iNumModels = 0;
			for(int i=0; i<ms_iMaxModels; i++)
				m_VehicleModelId[i].Invalidate();
		}
	};

	struct CVehicleReuseEntry
	{
		RegdVeh	pVehicle;
		u32		uFrameCreated;
	};
#define VEHICLE_REUSE_POOL_SIZE (15)
	static atFixedArray<CVehicleReuseEntry, VEHICLE_REUSE_POOL_SIZE> ms_vehicleReusePool;

	static int ms_iNumStreetModelAssociations;
	static atRangeArray<CStreetModelAssociation, ms_iMaxStreetModelAssociations> m_StreetModelAssociations;

#if ENABLE_NETWORK_LOGGING
    static void LogPlayersSharingTurn(const netPlayer** players, int playerCount, const Vector3 &position);
    static atFixedBitSet<MAX_NUM_ACTIVE_PLAYERS, u32> ms_PlayersTakingTurn;
#endif // ENABLE_NETWORK_LOGGING
};

struct CVehicleCreationPos
{
	Vector3			Pos;
	CNodeAddress	FromNode;
	CNodeAddress	ToNode;
	float			Fraction;
};

struct CVehiclePopulationAsyncUpdateData
{
	Vector3 				popCtrlCentre;
	Vector2					sidewallFrustumOffset;
	Vector3					popCtrlDir;
	CVehiclePopulation::CVehGenLink* p_aGenerationLinks;
	u32* 					p_numGenerationLinks;
	CVehicleCreationPos*	m_pCreationPositions;
	u32*					m_pNumCreationPositions;
	float*					m_pAverageLinkHeight;
	u32						m_FrameCount;
	u32						m_NumCreationPositions;
	u32						m_iNumOverlappingRoadAreas;
	s32						m_iNumTrafficAreas;
	float					m_baseHeading;
	float					maxNodeDistBase;
	float					maxNodeDist;
	float					m_fKeyhole_InViewInnerRadius;
	float					m_fKeyhole_InViewOuterRadius;
	float					m_fKeyhole_BehindInnerRadius;
	float					m_fKeyhole_BehindOuterRadius;
	float					m_fKeyhole_CosHalfFOV;
	float					m_cambz;
	float					m_moveSpeedX;
	float					m_moveSpeedY;
	float					m_camFrontXNorm;
	float					m_camFrontYNorm;
	bool					m_bUseKeyholeShape;
	bool					m_playerInVehicle;
	bool					m_playerInHeli;
	bool					m_spawnCareAboutDeadEnds;
	bool					m_allowSpawningFromDeadEndRoads;
	u8						padding[7];
};

#endif // VEHICLEPOPULATION_H_
