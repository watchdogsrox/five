#ifndef INC_SCENARIO_MANAGER_H
#define INC_SCENARIO_MANAGER_H

#include "atl/array.h"
#include "atl/hashstring.h"
#include "entity/archetypemanager.h"		// fwModelId
#include "Peds/PedType.h"
#include "Peds/pedpopulation.h"
#include "scene/RegdRefTypes.h"
#include "Task/Scenario/Info/ScenarioTypes.h"
#include "Task/Scenario/ScenarioSpawnHistory.h"
#include "ai/ambient/AmbientModelSet.h"

// Forward Declarations
namespace rage { 
	class aiTask;
	class Quaternion;
	class Matrix34;
}

class CAmbientModelVariations;
class CAmbientPedModelSet;
class CAmbientVehicleModelVariations;
class CCarGenerator;
class CInteriorInst;
class CScenarioInfo;
class CPed;
class CDynamicEntity;
class CEntity;
class CObject;
class CTask;
class CTrain;
class DefaultScenarioInfo;
class CVehicle;
class CVehicleScenarioLayoutInfo;
class CVehicleScenarioManager;
class CConditionalAnimsGroup;
class CScenarioChainTests;
class CScenarioPoint;
class CScenarioPointChainUseInfo;
class CScenarioVehicleInfo;
class CVehicleModelInfo;

#define MIN_BONNET_HEIGHT_FOR_BONNET_CLIPS 0.9f

//-----------------------------------------------------------------------------
// CScenarioSpawnCheckCache

/*
PURPOSE
	A simple cache used for scenario spawning, to make sure we don't keep repeating expensive
	and potentially futile checks for collision problems and ped models.
*/
class CScenarioSpawnCheckCache
{
public:
	// Cache entry
	struct Entry
	{
		void UpdateTime(u32 timeUntilInvalidMs);

		CScenarioPoint*	m_Point;		// Note: not safe to dereference!
		int				m_ScenarioType;
		bool			m_FailedCheck;
		bool			m_PassedEarlyCollisionCheck;
		bool			m_PassedEarlyModelCheck;
		u32				m_TimeNoLongerValid;
	};

	void Update();

	void Clear()
	{
		m_CacheEntries.Reset();
	}

	Entry& FindOrCreateEntry(CScenarioPoint& pt, int scenarioTypeReal);
	void RemoveEntries(CScenarioPoint& pt);

#if !__FINAL
	void DebugDraw() const;
#endif

protected:
	static const int kMaxEntries = 16;
	atFixedArray<Entry, kMaxEntries> m_CacheEntries;
};

//////////////////////////////////////////////////////////////////////////
// CAmbientModelSetFilterForScenario -- Now exposed in the header since it is needed elsewhere
//////////////////////////////////////////////////////////////////////////
class CAmbientModelSetFilterForScenario : public CAmbientModelSetFilter
{
public:
	CAmbientModelSetFilterForScenario();
	virtual bool Match(u32 modelHash) const;
#if __ASSERT
	virtual bool FailureShouldTriggerAsserts() const;
#endif 
#if __BANK
	virtual void GetFilterDebugInfoString(char* strOut, int strMaxLen) const;
#endif	// __BANK

	Gender m_RequiredGender;
	const CAmbientPedModelSet ** m_BlockedModels;
	const CConditionalAnimsGroup*	m_ConditionalAnims; // Used to CheckForMatchingPopulationConditionalAnims() if not null
	s32	 m_NumBlockedModelSets;
	bool m_bProhibitBulkyItems;
#if __ASSERT
	mutable bool m_bModelWasSuppressed;
	mutable bool m_bIgnoreGenderFailure;
	mutable bool m_bAmbientModelWasBlocked;
#endif
};

//////////////////////////////////////////////////////////////////////////
// CScenarioManager
//////////////////////////////////////////////////////////////////////////

class CScenarioManager
{
public:
	struct CollisionCheckParams
	{
		CScenarioPoint*	m_pScenarioPoint;
		int				m_ScenarioTypeReal;
		Vec3V			m_ScenarioPos;
		CEntity*		m_pAttachedEntity;
		CTrain*			m_pAttachedTrain;
		CObject*		m_pPropInEnvironment;
	};

	struct PedTypeForScenario
	{
		PedTypeForScenario()
				: m_PedModelIndex(fwModelId::MI_INVALID)
				, m_Variations(NULL)
				, m_NeedsToBeStreamed(false)
 				, m_NoBulkyItems(false)
		{}

		u32					m_PedModelIndex;
		const CAmbientModelVariations*	m_Variations;
		bool							m_NeedsToBeStreamed;
		bool							m_NoBulkyItems;
	};


	static CScenarioSpawnCheckCache& GetSpawnCheckCache()
	{
		return sm_SpawnCheckCache;
	}

	static CVehicleScenarioManager& GetVehicleScenarioManager()
	{
		Assertf(sm_VehicleScenarioManager, "CVehicleScenarioManager not initialized.");
		return* sm_VehicleScenarioManager;
	}

	static CScenarioChainTests& GetChainTests()
	{
		return *sm_ScenarioChainTests;
	}

	// Initialisation
	static void Init(unsigned initMode);

	// Shutdown
	static void Shutdown(unsigned shutdownMode);

	// Update
	static void Update();

	// Happens every frame
	static void ResetExclusiveScenarioGroup();

	// PURPOSE:	Call if we need to refresh car generators for vehicle scenarios.
	static void CarGensMayNeedToBeUpdated() { sm_CarGensNeedUpdate = true; }

	// PURPOSE:	Called by the car generator shutdown code, to verify that the scenario system should have
	//			released all car generators it is using.
	static bool IsSafeToDeleteCarGens() { return !sm_ReadyForOnAddCalls; }

#if __ASSERT
	// PURPOSE:	Get a string describing the scenario we think is associated with a given car generator.
	// PARAMS:	strBuff		- Output buffer to write to.
	//			buffSize	- The size of the output buffer.
	//			carGenIndex	- Index of the car generator, in its pool.
	static void GetInfoStringForScenarioUsingCarGen(char* strBuff, int buffSize, int carGenIndex);
#endif	// __ASSERT

	///////////////////
	// Helper functions
	///////////////////

	static bool					IsStreamingUnderPressure() { return sm_StreamingUnderPressure; }
	static void					UpdateConditions();
	static void					UpdateStreamingUnderPressure();

	static void					ProcessPreScripts();

	// Handle the ability for script to disable normal scenario exits.
	// Normal scenario exits are when peds leave a scenario because of time constraints or condition mismatches, not from threat response / shocking events.
	
	static bool					AreScenarioExitsSuppressed()					{ return sm_AreNormalScenarioExitsSuppressed; }
	static void					SetScenarioExitsSuppressed(bool bAreSuppressed) { sm_AreNormalScenarioExitsSuppressed = bAreSuppressed; }

	// Handle the ability for script to disable peds being attracted to scenario points.
	static bool					IsScenarioAttractionSuppressed()				{ return sm_IsScenarioAttractionSuppressed; }
	static void					SetScenarioAttractionSuppressed(bool bIsSuppressed) { sm_IsScenarioAttractionSuppressed = bIsSuppressed; }

	static bool					AreScenarioBreakoutExitsSuppressed()					{ return sm_ScenarioBreakoutExitsSuppressed; }
	static void					SetScenarioBreakoutExitsSuppressed(bool bSuppressed)	{ sm_ScenarioBreakoutExitsSuppressed = bSuppressed; }

	// Gets the scenario info from the metadata
	static const CScenarioInfo*	GetScenarioInfo(s32 scenarioType);

	// Returns the scenario name from the type
	static const char*			GetScenarioName(s32 scenarioType);
	// Returns the scenario type from the name string
	static s32					GetScenarioTypeFromName(const char* scenario);
	// Returns the scenario type from the name hashkey
	static s32					GetScenarioTypeFromHashKey(u32 uHashKey);

	static const CConditionalAnimsGroup * GetConditionalAnimsGroupForScenario(s32 scenarioType);

	static bool					CheckScenarioPointEnabled			(const CScenarioPoint& scenarioPt, int indexWithinTypeGroup);
	static bool					CheckScenarioPointEnabledByGroupAndAvailability(const CScenarioPoint& scenarioPt);
	static bool					CheckScenarioPointEnabledByType		(const CScenarioPoint& scenarioPt, int indexWithinTypeGroup);

	static bool					CheckScenarioPointMapData			(const CScenarioPoint& scenarioPt);
	static bool					IsScenarioPointAttachedEntityUprootedOrBroken (const CScenarioPoint& pSecnarioPoint);
	static bool					IsScenarioPointInBlockingArea		(const CScenarioPoint& scenarioPt);

	// Returns true if its a valid time for the scenario
	static bool					CheckScenarioPointTimesAndProbability(const CScenarioPoint& scenarioPt, bool checkProbability, bool checkInterval, int indexWithinTypeGroup BANK_ONLY(, bool bRegisterSpawnFailure = false));
	static bool					CheckScenarioSpawnInterval			(int scenarioType);
	static bool					CheckScenarioTimesAndProbability	(const CScenarioPoint* pt, s32 scenarioType, bool checkProbability = true, bool checkInterval = true BANK_ONLY(, bool bRegisterSpawnFailure = false));
	static bool					CheckScenarioProbability			(const CScenarioPoint* pt, int scenarioType);
	static bool					CheckVariousSpawnConditions			(int scenarioType, unsigned modelSetIndex);
	static bool					GetRangeValuesForScenario			(s32 scenarioType, float& fRange, s32& iMaxNum);
	static u32					GetFlagFromName						(char* szFlag);
	static s32					AddScenarioInfo						(const char* scenario);

	static void					GetCreationParams(const CScenarioInfo* pScenarioInfo, const CEntity* pAttachEntity, Vector3& vPedPos, float& fHeading);
	static void					GetCreationMatrix(Matrix34& matInOut, const Vector3& vTransOffset, const Quaternion& qRotOffset, const CEntity* pEntity);

	// Randomly gives scenarios to peds spawned wandering
	static DefaultScenarioInfo GetDefaultScenarioInfoForRandomPed(const Vector3& pos, u32 modelIndex);
	static void ApplyDefaultScenarioInfoForRandomPed(const DefaultScenarioInfo& scenarioInfo, CPed* pPed);

	// Randomly Creates peds and scenarios to the scenariopoint passed
	static s32 GiveScenarioToScenarioPoint(CScenarioPoint& scenarioPoint, float maxFrustumDistToCheck, bool allowDeepInteriors, s32 iMaxPeds, s32 iSpawnOptions, int scenarioTypeIndexWithinGroup, CObject* pPropInEnvironment, const CollisionCheckParams* pCollisionCheckParams, const PedTypeForScenario* predeterminedPedType, CScenarioPointChainUseInfo* chainUserInfo);
	// Randomly gives scenarios to cars spawned parked
	static bool GiveScenarioToParkedCar(CVehicle* pVehicle, ParkedType parkedType = PT_ParkedOnStreet, int scenarioType = -1 );
	// Randomly gives scenarios to cars spawned driving
	static bool GiveScenarioToDrivingCar( CVehicle* pVehicle, s32 specificScenario = Scenario_Invalid );
	// Sets up the task and the scenario for a ped todo with a particular scenario
	// Returns NULL for no ped
	static CTask* SetupScenarioAndTask( s32 scenario, Vector3& vPedPos, float& fHeading, CScenarioPoint& scenarioPoint, s32 iSpawnOptions = 0, bool bStartInCurrentPos = false, bool bUsePointForTask = true, bool bForAmbientUsage = false, bool bSkipIntroClip = true);
	// Given a scenario task created by SetupScenarioAndTask(), possibly wrap it inside a control
	// task such as CTaskRoam.
	static CTask* SetupScenarioControlTask(const CPed& rPed, const CScenarioInfo& scenarioInfo, CTask* pUseScenarioTask, CScenarioPoint* pScenarioPoint, int scenarioTypeReal);
	//Wrap a particular scenario task
	static CTask* WrapScenarioTask(const CPed& rPed, CTask* pUseScenarioTask, CScenarioPoint* pScenarioPoint, int scenarioTypeReal, bool bRoamMode);
	// Helper function to work out the heading to use when spawning at a scenario
	static float SetupScenarioPointHeading( const CScenarioInfo* pScenarioInfo, CScenarioPoint& scenarioPoint );
	// Sets up a couple scenario
	static int SetupCoupleScenario(CScenarioPoint& scenarioPoint, float maxFrustumDistToCheck, bool allowDeepInteriors, s32 iSpawnOptions, int scenarioType);
	// Sets up a group ped scenario
	static s32 SetupGroupScenarioPoint( s32 scenario, CScenarioPoint& scenarioPoint, s32 iMaxPeds, s32 iSpawnOptions );
	// Sets up a driving scenario for the vehicle
	static s32  PickRandomScenarioForDrivingCar(CVehicle* pVehicle);
 	static bool	SetupDrivingVehicleScenario( s32 scenario, CVehicle* pVehicle );

	static	bool PerformCollisionChecksForScenarioPoint(const CollisionCheckParams& params);

	static float GetVehicleScenarioDistScaleFactorForModel(fwModelId vehicleModelId, bool aerialSpawn);
	static u32 GetRandomDriverModelIndex(fwModelId vehicleModelId, const CAmbientModelSetFilter* pModelFilter);
	static bool GetPedTypeForScenario( s32 scenarioTypeIndexReal, const CScenarioPoint& scenarioPoint, strLocalIndex& newPedModelIndex, const Vector3& vPos, s32 iDesiredAudioContexts = 0, const CAmbientModelVariations** variationsOut = NULL, int pedIndexWithinScenario = 0, fwModelId vehicleModelToPickDriverFor = fwModelId());
	static bool SelectPedTypeForScenario(PedTypeForScenario& pedTypeOut, s32 scenarioTypeIndexReal, const CScenarioPoint& scenarioPoint, const Vector3& vPos, s32 iDesiredAudioContexts, int pedIndexWithinScenario, fwModelId vehicleModelToPickDriverFor, bool needVariations, bool bAllowFallbackPed = false);
	
	// helper function for querying a scenario chain for blocked modelsets and building the pBlockedModelSetsArray
	
	static void GatherBlockedModelSetsFromChainPt(const CScenarioPoint &pt, const CScenarioInfo &ptInfo, const CAmbientPedModelSet** pBlockedModelSetsArray, int& iNumBlockedModelSets, bool& bBlockBulkyItems, int iNumEdgesToCheck = 30);
	static void GatherBlockedModelSetsFromChain(const CScenarioChainingGraph& graph, int iChainIndex, const CAmbientPedModelSet** pBlockedModelSetsArray, int& iNumBlockedModelSets, bool& bBlockBulkyItems, int iNumEdgesToCheck = 30);

	// attempts to add pNewBlockedModelSet to pBlockedModelSetsArray, if the array becomes full (reaches kMaxBlockedModelSets) returns false.
	static bool BlockedModelSetsArrayContainsModelHash(const CAmbientPedModelSet ** pBlockedModelSetsArray, const int iNumBlockedModelSets, const u32 iModelNameHash);
	static bool AddBlockedModelSetToArray(const CAmbientPedModelSet** pBlockedModelSetsArray, int& iNumBlockedModelSets, const CAmbientPedModelSet* pNewBlockedModelSet);
	// checks the CAmbientPedModelSet hashes in pBlockedModelSetsArray for pNewBlockedModelSet's hash, if not found, returns true.
	static bool CanAddBlockedModelSetToArray(const CAmbientPedModelSet** pBlockedModelSetsArray, const int iNumBlockedModelSets, const CAmbientPedModelSet* pNewBlockedModelSet);
	
	static CPed* CreateScenarioPed(const int realScenarioType, CScenarioPoint& scenarioPoint, CTask* pScenarioTask, Vector3& vPedPos, float maxFrustumDistToCheck, bool allowDeepInteriors, float& fHeading, s32 iSpawnOptions, int pedIndexWithinScenario = 0, bool findZCoorForPed = false,
			const CollisionCheckParams* pCollisionCheckParams = NULL,
			const PedTypeForScenario* predeterminedPedType = NULL,
			bool bAllowFallbackPed = false, CScenarioPointChainUseInfo* chainUserInfo = NULL);

	static CTask* CreateTask(CScenarioPoint* pScenarioPoint, int scenario, bool forAmbientUsage = false, float overrideMoveBlendRatio = -1.0f);
	// Checks to see if the area surrounding this position is overpopulated by similar scenarios
	static bool CheckScenarioPopulationCount(const s32 scenarioType, const Vector3& effectPos, float fRequestedClosestRange, const CDynamicEntity* pException = NULL, bool checkMaxInRange = true);
	static bool CheckVehicleScenarioPopulationCount(int scenarioType, Vec3V_In spawnPosV);
	
	static int ChooseRealScenario(int virtualOrRealScenarioTypeIndex, CPed* pPed);

	static bool IsVehicleScenarioPoint(const CScenarioPoint& scenarioPoint, int indexWithinTypeGroup);

	// PURPOSE:	Check if a given scenario type, which can be virtual or real, is a vehicle scenario.
	static bool IsVehicleScenarioType(int virtualOrRealIndex);

	// PURPOSE: Check if a given scenario type, which can be virtual or real, is a LookAt scenario.
	static bool IsLookAtScenarioType(int virtualOrRealIndex);

	static bool IsVehicleScenarioType(const CScenarioInfo& info);

	static bool IsLookAtScenarioType(const CScenarioInfo& info);

	static bool ShouldHaveCarGen(const CScenarioPoint& point, const CScenarioVehicleInfo& info);

	// PURPOSE:	Assuming that ShouldHaveCarGen() returns true, should the given scenario point have a
	//			car generator under the current conditions (time of day, groups enabled, etc).
	static bool ShouldHaveCarGenUnderCurrentConditions(const CScenarioPoint& point, const CScenarioVehicleInfo& info);

	// PURPOSE:	If a point has a car generator, and is supposed to have a car generator, check if the
	//			current car generator isn't good under the current conditions.
	static bool IsCurrentCarGenInvalidUnderCurrentConditions(const CScenarioPoint& point);

	static void OnAddScenario(CScenarioPoint& point);
	static void OnRemoveScenario(CScenarioPoint& point);

	static const float& GetClusterRangeForVehicleSpawn() { return sm_ClusterRangeForVehicleSpawn; }
	static const float& GetExtendedExtendedRangeForVehicleSpawn() { return sm_ExtendedExtendedRangeForVehicleSpawn; }
	static const float& GetMaxOnScreenMinSpawnDistForCluster() { return sm_MaxOnScreenMinSpawnDistForCluster; }

	// PURPOSE:	Check if the conditions are currently such that we should extend scenario spawn ranges
	//			further than normal, for scenarios marked as extended range.
	static bool ShouldExtendExtendedRangeFurther() { return sm_ShouldExtendExtendedRangeFurther; }

	enum CarGenConditionStatus
	{
		kCarGenCondStatus_Failed,
		kCarGenCondStatus_NotReady,
		kCarGenCondStatus_Ready
	};
	static CarGenConditionStatus CheckCarGenScenarioConditionsA(const CCarGenerator& cargen);
	static CarGenConditionStatus CheckCarGenScenarioConditionsB(const CCarGenerator& cargen, fwModelId carModelToUse, fwModelId trailerModelToUse);
	static float GetCarGenSpawnRangeMultiplyer(const CCarGenerator& cargen);
	static bool CheckPosAgainstVehiclePopulationKeyhole(Vec3V_In spawnPosV, ScalarV_In distScaleFactorV, bool extendedRange, bool relaxForCluster, bool noMaxLimit = false);
	static bool PopulateCarGenVehicle(CVehicle& veh, const CCarGenerator& cargen, bool calledByCluster, const CAmbientVehicleModelVariations** ppVehVarOut = NULL);
	static void ApplyVehicleColors(CVehicle& veh, const CAmbientVehicleModelVariations& vehVar);
	static void ApplyVehicleVariations(CVehicle& veh, const CAmbientVehicleModelVariations& vehVar);
	static void VehicleSetupBrokenDown(CVehicle& veh);
	static bool ReportCarGenVehicleSpawning(CVehicle& veh, const CCarGenerator& cargen);
	static void ReportCarGenVehicleSpawnFinished(CVehicle& veh, const CCarGenerator& cargen, CScenarioPointChainUseInfo* pChainUseInfo);
	static bool AllowExtraChainedUser(const CScenarioPoint& pt);
	static CScenarioPoint* GetScenarioPointForCarGen(const CCarGenerator& cargen);
	static void GetCarGenRequiredObjects(const CCarGenerator& cargen, int& numRequiredPedsOut, int& numRequiredVehiclesOut, int& numRequiredObjectsOut);
	static bool GetCarGenTrailerToUse(const CCarGenerator& cargen, fwModelId& trailerModelOut);
	static const CScenarioVehicleInfo* GetScenarioVehicleInfoForCarGen(const CCarGenerator& cargen);
	static bool ShouldWakeUpVehicleScenario(const CCarGenerator& cargen, bool desiredWakeUp);
	static bool ShouldDoOutsideRangeOfAllPlayersCheck(const CCarGenerator& cargen);

	static CarGenConditionStatus CheckPedsNearVehicleUsingLayoutConditions(const CCarGenerator& cargen, const CVehicleScenarioLayoutInfo& layout, fwModelId carModelToUse);
	static bool SpawnPedsNearVehicleUsingLayout(/*const*/ CVehicle& veh, int scenarioType, int* numSpawnedOut = NULL, CPed** spawnedPedArrayOut = NULL, int spaceInArray = 0);
	static const CAmbientModelSet* GetVehicleModelSetFromIndex(const CScenarioVehicleInfo& info, const unsigned vehicleModelSetIndexFromPoint);
	static bool HasValidVehicleModelFromIndex(const CScenarioVehicleInfo& info, const unsigned vehicleModelSetIndexFromPoint);
	static u32 GetRandomVehicleModelHashOverrideFromIndex(const CScenarioVehicleInfo& info, const unsigned vehicleModelSetIndexFromPoint);
	static bool CanVehicleModelBeUsedForBrokenDownScenario(const CVehicleModelInfo& modelInfo);
	static bool FindVehicleModelInModelSet(u32 modelNameHash, const CAmbientModelSet& modelSet, const CAmbientVehicleModelVariations* &varOut);

	static bool FindPropsInEnvironmentForScenario(CObject*& pPropOut, const CScenarioPoint& point, int realScenarioType, bool allowUprooted);
	static void TransformScenarioPointForEntity(CScenarioPoint& pointInOut, const CEntity& ent);

	static CScenarioSpawnHistory& GetSpawnHistory() { return sm_SpawnHistory; }

	static bank_u32   ms_iExtendedScenarioLodDist;

	static u32 LegacyDefaultModelSetConvert(const u32 modelSetHash);
	static atHashString MODEL_SET_USE_POPULATION;

	static CScenarioSpawnHistory sm_SpawnHistory; //public only for debug drawing CScenarioDebug::DebugRender

	static bool StreamScenarioPeds(fwModelId pedModel, bool highPri, int realScenarioType);

	static bool GetRespectSpawnInSameInteriorFlag() { return sm_RespectSpawnInSameInteriorFlag; }

	static u32 sm_AbandonedVehicleCheckTimer;

protected:
	static bool sm_ReadyForOnAddCalls;

	static bool sm_AreNormalScenarioExitsSuppressed;

	static bool sm_IsScenarioAttractionSuppressed;

	static bool sm_ScenarioBreakoutExitsSuppressed;

	static bool sm_RespectSpawnInSameInteriorFlag;

	struct ScenarioSpawnParams
	{
		ScenarioSpawnParams()
				: m_GroundAdjustedPedRootPos(V_ZERO)
				, m_pVariations(NULL)
				, m_pInteriorInst(NULL)
				, m_NewPedModelIndex(fwModelId::MI_INVALID)
				, m_iRoomIdFromCollision(0)
				, m_bUseGroundToRootOffset(true)
				{}
		Vec3V							m_GroundAdjustedPedRootPos;
		const CAmbientModelVariations*	m_pVariations;
		CInteriorInst*					m_pInteriorInst;
		u32								m_NewPedModelIndex;
		s32								m_iRoomIdFromCollision;
		bool							m_bUseGroundToRootOffset;
	};
	static bool DetermineScenarioSpawnParameters(
			ScenarioSpawnParams& paramsOut,
			const int realScenarioType,
			Vec3V_ConstRef vPedRootPos, CScenarioPoint& scenarioPoint, CTask* pScenarioTask,
			const float fAddRangeInViewMin, const bool allowDeepInteriors,
			const int pedIndexWithinScenario,
			bool findZCoorForPed,
			const PedTypeForScenario* predeterminedPedType,
			bool bAllowFallbackPed = false);

	// PURPOSE:	If needed, update (add or remove) car generators for vehicle scenarios.
	static void UpdateCarGens();

	// PURPOSE:	Update the car generator presence for a specific point.
	// PARAMS:	point			- The point to update for.
	//			allowRemoval	- True if this function may actually remove the car generator for this point.
	// RETURNS:	True unless we wanted to remove a car generator but were not allowed (due to allowRemoval).
	static bool UpdateCarGen(CScenarioPoint &point, bool allowRemoval = true);

	// PURPOSE:	Determine the value of sm_ShouldExtendExtendedRangeFurther this frame.
	static void UpdateExtendExtendedRangeFurther();

    // PURPOSE:	Array for quick lookups of CScenarioPoints for car generators, see GetScenarioPointForCarGen().
	static atArray<CScenarioPoint*> sm_ScenarioPointForCarGen;

	struct CSpawnedVehicleInfo
	{
		RegdVeh		m_Vehicle;
		u16			m_ScenarioType;
	};
	static const int kMaxSpawnedVehicles = SCENARIO_MAX_SPAWNED_VEHICLES;
	static const int kMaxBlockedModelSets = 5;

	static atFixedArray<CSpawnedVehicleInfo, kMaxSpawnedVehicles> sm_SpawnedVehicles;
	
	// PURPOSE:	Global seed for random numbers used for ped and vehicle scenarios.
	static int sm_ScenarioProbabilitySeedOffset;

	// PURPOSE:	Manager responsible for performing tests on scenario chains.
	static CScenarioChainTests* sm_ScenarioChainTests;

	static CVehicleScenarioManager* sm_VehicleScenarioManager;

	static CScenarioSpawnCheckCache sm_SpawnCheckCache;

	static float sm_ClusterRangeForVehicleSpawn;
	static float sm_ExtendedExtendedRangeForVehicleSpawn;
	static float sm_ExtendedExtendendRangeMultiplier;
	static float sm_MaxOnScreenMinSpawnDistForCluster;

	// PURPOSE:	Speed above which we assume that the streaming system is under pressure, in m/s.
	static float sm_StreamingUnderPressureSpeedThreshold;

	// PURPOSE:	The current region we are updating car generators for, in UpdateCarGens().
	static s16 sm_CurrentRegionForCarGenUpdate;

	// PURPOSE:	The current point we are updating car generators for, in UpdateCarGens().
	static s16 sm_CurrentPointWithinRegionForCarGenUpdate;

	// PURPOSE:	The current point we are updating car generators for, in UpdateCarGens().
	static s16 sm_CurrentClusterWithinRegionForCarGenUpdate;

	// PURPOSE:	The last hour we did an update of the car generators for vehicle scenarios
	//			for. When this doesn't match the current hour, it's time to update again.
	static u8 sm_CarGenHour;

	// PURPOSE:	Set when something has changed so that we may need to add or remove
	//			car generators associated with vehicle scenarios. Basically, should be
	//			set when something that ShouldHaveCarGenUnderCurrentConditions() depends on
	//			has changed.
	static bool sm_CarGensNeedUpdate;

	// PURPOSE:	True if the conditions are currently such that we should extend scenario spawn ranges
	//			further than normal, for scenarios marked as extended range.
	static bool sm_ShouldExtendExtendedRangeFurther;

	// PURPOSE:	True if we consider the streaming system to be under pressure, for the purposes of
	//			scenario spawning.
	static bool sm_StreamingUnderPressure;
};

#endif // INC_SCENARIO_MANAGER_H
