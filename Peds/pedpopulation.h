/////////////////////////////////////////////////////////////////////////////////
// FILE :		pedpopulation.cpp
// PURPOSE :	Deals with creation and removal of peds in the world.
//				It manages the ped population by removing what peds we can and
//				adding what peds we should.
//				It creates ambient peds and also peds at attractors when
//				appropriate.
//				It also revives persistent peds and cars that are close enough
//				to the player.
// AUTHOR :		Obbe.
// CREATED :	10/05/06
/////////////////////////////////////////////////////////////////////////////////
#ifndef PEDPOPULATION_H_
#define PEDPOPULATION_H_

// rage includes
#include "atl/array.h"
#include "fwanimation/animdefines.h"
#include "vector/vector3.h"
#include "grcore/debugdraw.h"

// game includes
#include "game/Dispatch/DispatchServices.h"
#include "control/population.h"
#include "modelinfo/ModelInfo.h"
#include "peds/peddefines.h"
#include "peds/pedtype.h"
#include "control/replay/Replay.h"


// forward declarations
class C2dEffect;
class CAmbientModelVariations;
class CAmbientPedModelSet;
class CCachedPedGenCoords;
class CDummyObject;
class CDynamicEntity;
class CEntity;
class CInteriorInst;
class CObject;
class CPed;
class CVehicle;
class DefaultScenarioInfo;
class CScenarioPoint;
class CConditionalAnimsGroup;
struct CPopCycleConditions;

enum Gender			{ GENDER_MALE, GENDER_FEMALE, GENDER_DONTCARE };
enum GangMembership	{ GANG_MEMBER, NOT_GANG_MEMBER, MEMBERSHIP_DONT_CARE };

/////////////////////////////////////////////////////////////////////////////////
// This class deals with creation and removal of peds in the world.
/////////////////////////////////////////////////////////////////////////////////
class CPedPopulation
{	
#if GTA_REPLAY
	friend class CReplayMgrInternal;		// To Remove
	friend class CReplayInterfacePed;
#endif // GTA_REPLAY

public:
	static	void		UpdateMaxPedPopulationCyclesPerFrame();
	static	void		InitValuesFromConfig();
	static	void		Init(unsigned initMode);
	static	void		Shutdown(unsigned shutdownMode);
	static	void		Process();
	static	void		ProcessInstantFill(const bool atStart);

	static	void		IncrementNumPedsCreatedThisCycle();
	static	void		IncrementNumPedsDestroyedThisCycle();

#if __BANK
	static void			InitWidgets();
	static void			DisplayMessageHistory();
	static void			DebugEventCreatePed(CPed* ped);
	static s32			GetFailString(char * failString, bool bScenario, bool bNonScenario);
	static void			RenderDebug();
	static void			OnToggleAllowPedGeneration();
#endif

#if DEBUG_DRAW
	//Event scanning (h/l), Motion task (h/l), physics(h/l), Entity scanning (h/l), Timeslicing (h/l)
	static void GetPedLODCounts(int &iCount, int &iEventScanHi, int &iEventScanLo, int &iMotionTaskHi, int &iMotionTaskLo, int &iPhysicsHi, int &iPhysicsLo, int &iEntityScanHi, int &iEntityScanLo, int &iActive, int &iInactive, int &iTimeslicingHi, int &iTimeslicingLo);
#endif //DEBUG_DRAW

	// Public as both ped and vehicle populations use this function.
	static	CPopCtrl	GetPopulationControlPointInfo			(CPopCtrl * pTempOverride=NULL);
	static	LocInfo		ClassifyInteriorLocationInfo			(const bool bPlayerIsFocus, const CInteriorInst * pInterior, const int iRoom);


	// Population control functions.
	static  void		ForcePopulationInit						() { ForcePopulationInit(ms_DefaultMaxMsAheadForForcedPopInit); }
	static	void		ForcePopulationInit						(const u32 msAheadOfCurrentToStop);
	static	bool		HasForcePopulationInitFinished			();
	static	void		SetDebugOverrideMultiplier				(const float value);
	static	float		GetDebugOverrideMultiplier				();
	static	void		SetAmbientPedDensityMultiplier			(float multiplier);
	static	void		SetScriptAmbientPedDensityMultiplierThisFrame(float multiplier);
	static	void		SetScriptedRangeMultiplierThisFrame(float multiplier);
	static	float		GetScriptedRangeMultiplierThisFrame() { return ms_fScriptedRangeMultiplierThisFrame; }
	static	void		SetAdjustPedSpawnPointDensityThisFrame(const s32 iModifier, const Vector3 & vMin, const Vector3 & vMax);
	static	float		GetAmbientPedDensityMultiplier			();
	static	float		GetTotalAmbientPedDensityMultiplier		();
	static	void		SetScenarioPedDensityMultipliers		(float fInteriorMult, float fExteriorMult);
	static	void		SetScriptScenarioPedDensityMultipliersThisFrame(float fInteriorMult, float fExteriorMult);
	static	void		GetScenarioPedDensityMultipliers		(float& fInteriorMult_out, float& fExteriorMult_out);
	static	void		GetTotalScenarioPedDensityMultipliers	(float& fInteriorMult_out, float& fExteriorMult_out);
	static	void		SetSuppressAmbientPedAggressiveCleanupThisFrame();
	static	void		SetAllowDummyConversions				(bool allow);
	static	void		SetForcedNewAmbientPedType				(s32 forceAllNewAmbientPedsType);
	static	void		SetAllowCreateRandomCops				(bool allowCreateRandomCops);
	static	bool		GetAllowCreateRandomCops				();
	static	void		SetAllowCreateRandomCopsOnScenarios		(bool allowCreateRandomCops);
	static	bool		GetAllowCreateRandomCopsOnScenarios		();
	static void			SetPopCycleMaxPopulationScaler			(float scaler);
	static	float		GetPopCycleMaxPopulationScaler			();
#if __BANK
	static	s32			GetPopCycleOverrideNumAmbientPeds		();
	static	s32			GetPopCycleOverrideNumScenarioPeds		();
#endif // __BANK

	static s32			GetNumScheduledPedsInVehicles			();

	static void			ResetPerFrameScriptedMultipiers			();

#if __BANK
	static	void		DrawAddAndCullZones();
#endif // __DEV

	enum
	{
		SF_ignoreSpawnTimes = (1<<0),
		SF_objectsOnly = (1<<1),
		SF_interiorAndHighPriOnly = (1<<2),
		SF_dontSpawnInterior = (1<<3),
		SF_forceSpawn = (1<<4),
		SF_highPriOnly = (1<<5),
		SF_ignoreScenarioHistory = (1<<6),
		SF_dontSpawnExterior = (1<<7),
		SF_addingTrainPassengers = (1<<8)
	};

	static	s32			GenerateScenarioPedsInSpecificArea		(const Vector3& areaCentre, bool allowDeepInteriors, float fRange, s32 iMaxPeds, s32 iSpawnOptions = 0);

	struct GetScenarioPointInAreaArgs
	{
		explicit GetScenarioPointInAreaArgs(Vec3V_In areaCentre, float maxRange);

		Vec3V			m_AreaCentre;
		CDynamicEntity* m_PedOrDummyToUsePoint;
		s32*			m_Types;
		float			m_MaxRange;
		s32				m_NumTypes;
		bool			m_CheckPopulation;	// True to check existing population around the scenario.
		bool			m_EntitySPsOnly;
		bool			m_MustBeFree;
		bool			m_Random;
		bool			m_MustBeAttachedToTrain;
	};
	static	bool		GetScenarioPointInArea					(const GetScenarioPointInAreaArgs& args, CScenarioPoint** ppScenarioPoint);
	static	bool		GetScenarioPointInArea					(const Vector3& areaCentre, float fMaxRange, s32 iNumTypes, s32* aTypes, CScenarioPoint** ppScenarioPoint, bool bMustBeFree = true, CDynamicEntity* pPedOrDummyToUsePoint = NULL, const bool bRandom = false, const bool bEntitySPsOnly = false);

	static void			SetScriptCamPopulationOriginOverride	();
	static void			SetUseStartupModeDueToWarping(bool b) { ms_useStartupModeDueToWarping = b; }
	static void			UseTempPopControlPointThisFrame			(const Vector3& pos, const Vector3& dir, const Vector3& vel, float fov, LocInfo locationInfo);
	static void			UseTempPopControlSphereThisFrame		(const Vector3& pos, float radius, float vehicleRadius);	
	static void			ClearTempPopControlSphere				();
	static void			ScriptedConversionCentreSetThisFrame	(const Vector3& centre);
	static bool			GetIsScriptedConversionCenterActive		() { return ms_scriptedConversionCentreActive; }
	static const Vector3 & GetScriptedConversionCenter			() { return ms_scriptedConversionCenter; }
	static	void		PedNonCreationAreaSet					(const Vector3& min, const Vector3& max);
	static	void		PedNonCreationAreaClear					();
	static	void		PedNonRemovalAreaSet					(const Vector3& min, const Vector3& max);
	static	void		PedNonRemovalAreaClear					();

	// Individual ped creation, removal, and conversion functions.
	static	bool		IsPedGenerationCoordCurrentlyValid		(const Vector3& candidatePedRootPos, bool bIsInInterior, float pedRadius, bool allowDeepInteriors, bool doInFrustumTest, float fAddRangeInViewMin, bool doVisibleToAnyPlayerTest, bool doObjectCollisionTest, const CEntity* pExcludedEntity1, const CEntity* pExcludedEntity2, s32& roomIdxOut, CInteriorInst*& pIntInstOut, bool doMpTest);
	static	bool		IsCandidateInViewFrustum				(const Vector3& candidatePedRootPos, float fTestRadius, float maxDistToCheck, float minDistToCheck=0.0f);
	static	CPed*		AddPed									(u32 pedModelIndex, const Vector3& newPedRootPos, const float fHeading, CEntity* pEntityToGetInteriorDataFrom, bool hasInteriorData, s32 roomIdx, CInteriorInst* pInteriorInst, bool giveProps, bool giveAmbientProps, bool bGiveDefaultTask, 
		bool checkForScriptBrains, const DefaultScenarioInfo& defaultScenarioInfo, bool bCreatedForScenario, bool createdByScript, u32 modelSetFlags = 0, u32 modelClearFlags = 0, const CAmbientModelVariations* variations = NULL, CPopCycleConditions* pExtraConditions = NULL, bool inCar = false, u32 npcHash = 0, bool bScenarioPedCreatedByConcealeadPlayer = false);
	static	CPed*		AddPedInCar								(CVehicle *pVehicle, bool bDriver, s32 seat_num, bool bCriminal, bool bUseExistingNodes, bool bCreatedByScript = false, bool bIgnoreDriverRestriction = false, u32 desiredModel = fwModelId::MI_INVALID, const CAmbientModelVariations* variations = NULL);
	static	void		AddPedToPopulationCount					(CPed* pPed);
	static	void		RemovePedFromPopulationCount			(CPed* pPed);

	// Helper functions.
	static s32			GetDesiredMaxNumScenarioPeds(bool bForInteriors, s32 * iPopCycleTempMaxScenarioPeds=NULL);
	static  s32			HowManyMoreScenarioPedsShouldBeGenerated(bool bForInteriors = false);

	// PURPOSE:	Input parameters to ChooseCivilianPedModelIndex(), there are too many of them
	//			to pass them in individually.
	struct ChooseCivilianPedModelIndexArgs
	{
		explicit ChooseCivilianPedModelIndexArgs();

		Gender						m_RequiredGender;
		GangMembership				m_RequiredGangMembership;
		fwMvClipSetId				m_MustHaveClipSet;
		const Vector3*				m_CreationCoors;
		u32							m_RequiredPopGroupFlags;
		u32							m_SelectionPopGroup;			// (OPTIONAL) if set allows you to specifically say which pop group you want to use to select a ped
		u32							m_SpeechContextRequirement;
		const CAmbientPedModelSet**	m_BlockedModelSetsArray;
		u32							m_NumBlockedModelSets;
		const CAmbientPedModelSet*	m_RequiredModels;				// If set, only models in this set can be chosen.
		bool						m_AvoidNearbyModels;
		bool						m_IgnoreMaxUsage;
		const CAmbientModelVariations** m_PedModelVariationsOut;	// If set, model variation info associated with the chosen ped is written to the address specified here.
		bool						m_AllowFlyingPeds;
		bool						m_AllowSwimmingPeds;
		bool						m_MayRequireBikeHelmet;			// About to ride a bike, need a helmet or permission to ride without one.
		bool						m_GroupGangMembers;
		const CConditionalAnimsGroup*	m_ConditionalAnims;			// If set, only models that fulfill at least one conditional anim the given group can be chosen.
		bool						m_ProhibitBulkyItems;			// True if we should prohibit ped models that can't be used at all without bulky items.
		bool						m_SortByPosition;				// Sort the ped models by distance, and choose the one that is furthest away, instead of least used
	};

	static	u32			ChooseCivilianPedModelIndex (ChooseCivilianPedModelIndexArgs& args);

	static bool     DoesModelMeetConditions ( const ChooseCivilianPedModelIndexArgs &args, const CPedModelInfo* pPedModelInfo, const u32 pedModelIndex );

	static bool			CheckPedModelGenderAndBulkyItems						(fwModelId pedModelId, Gender requiredGender, bool bProhibitBulkyItems);

	//  Helper functions for streaming.
	static	void		LoadSpecificDriverModelsForCar			(fwModelId carModelId);
	static	void		RemoveSpecificDriverModelsForCar		(fwModelId carModelIndex);
	static	u32			PickModelFromGroupForInCar				(const class CLoadedModelGroup& group, CVehicle* pVeh);
	static	fwModelId	FindSpecificDriverModelForCarToUse		(fwModelId carModelIndex);
	static	fwModelId	FindRandomDriverModelForCarToUse		(fwModelId carModelIndex);
	static void			InstantFillPopulation					();
	static bool			GetInstantFillPopulation				() { return ms_bInstantFillPopulation!=0; }
	
	// Apply special conditions for streaming preferences on the model index.
	static void			ManageSpecialStreamingConditions		(u32 pedModelIndex);

	//  Helper functions for network.
	static	void		NetworkRegisterAllPedsAndDummies		();
	static	void		StopSpawningPeds						();	
	static	void		StartSpawningPeds						();
	static	bool		AllowedToCreateMoreLocalScenarioPeds	(bool *bAllowedToCreateHighPriorityOnly = 0);
	static	void		IncrementNetworkPedsSpawnedThisFrame	();
	static	bool		IsNetworkTurnToCreateScenarioPedAtPos	(const Vector3 &position);
	static	bool		ExistingPlayersWithSimilarPedGenRange	(const Vector3 &spawnPosition);

	enum ePedRemovalMode
	{
		PMR_CanBeDeleted,				// Default - use the CanBeDeleted function - basically random peds
										//			 that are in a position to go.
		PMR_ForceAllRandom,				// Delete all but player and mission peds, regardless of CanBeDeleted status
		PMR_ForceAllRandomAndMission,	// Delete all but player peds, regardless of CanBeDeleted status
	};

	static void			RemoveAllPedsHardNotMission		();
	static void			RemoveAllPedsHardNotPlayer		();

	static	void		RemoveAllPeds					(ePedRemovalMode mode );
	static	void		RemoveAllPedsWithException		(CPed * pException, ePedRemovalMode mode );

	static	void		RemoveAllRandomPeds				();
	static	void		RemoveAllRandomPedsWithException(CPed * pException);

#if __BANK
	static void			PedMemoryBudgetMultiplierChanged();
	static void			DebugSetForcedPopCenterFromCameraCB();
	static void			DebugSetPopControlSphere();
	static void			DebugClearPopControlSphere();
#endif // __BANK

	static void			AddScenarioPointToEntityFromEffect(C2dEffect* effect, CEntity* entity);
	static void			RemoveScenarioPointsFromEntity(CEntity* entity);
	static void			RemoveScenarioPointsFromEntities(const CEntity* const* entities, u32 numEntities);

	static	bool	    IsEffectInUse (const CScenarioPoint& scenarioPoint);

	static bool			ShouldUsePriRemovalOfAmbientPedsFromScenario();

    static float        GetCreationDistance() { return ms_addRangeBaseInViewMax; }

	static bool			IsAtScenarioPedLimit() { return ms_atScenarioPedLimit; }

	static s32			GetTotalNumPedsOnFoot()
	{
		return ms_nNumOnFootAmbient + ms_nNumOnFootScenario + ms_nNumOnFootOther + ms_nNumOnFootCop + ms_nNumOnFootSwat + ms_nNumOnFootEmergency;
	}

    static u32          GetMPVisibilityFailureCount() { return ms_mpVisibilityFailureCount; }

	// These are public since they are used directly by peds and the
	// replay, network, scripting, and population cycle systems.  Uhg.
	static	s32		ms_nNumOnFootAmbient;
	static	s32		ms_nNumOnFootScenario;
	static	s32		ms_nNumOnFootOther;
	static	s32		ms_nNumOnFootCop;
	static	s32		ms_nNumOnFootSwat;
	static	s32		ms_nNumOnFootEmergency;

	static	s32		ms_nNumInVehAmbient;
	static	s32		ms_nNumInVehScenario;
	static  s32		ms_nNumInVehAmbientDead;
	static  s32		ms_nNumInVehAmbientNoPretendModel;
	static	s32		ms_nNumInVehOther;
	static	s32		ms_nNumInVehCop;
	static	s32		ms_nNumInVehSwat;
	static	s32		ms_nNumInVehEmergency;

#if !__FINAL
	static	bool	ms_bAllowRagdolls;			// allow ragdolls / Natural Motion in game
#endif // !__FINAL

#if ENABLE_NETWORK_LOGGING
	static Vector3 m_vLastSetPopPosition;
#endif // ENABLE_NETWORK_LOGGING

#if __BANK
	static float	ms_addCullZoneHeightOffset;
	static	s32		ms_popCycleOverrideNumberOfAmbientPeds;
	static	s32		ms_popCycleOverrideNumberOfScenarioPeds;
	static	bool	ms_createPedsAnywhere;
	static	bool	ms_movePopulationControlWithDebugCam;
	static	s32		m_iDebugAdjustSpawnDensity;
	static float	m_fDebugScriptedRangeMultiplier;

	static	bool	ms_focusPedBreakOnPopulationRemovalTest;
	static	bool	ms_focusPedBreakOnPopulationRemoval;
	static	bool	ms_focusPedBreakOnAttemptedPopulationConvert;
	static	bool	ms_focusPedBreakOnPopulationConvert;
    static	bool	ms_allowNetworkPedPopulationGroupCycling;

	static	bool	ms_displayMiscDegbugTests;
	static	bool	ms_displayAddCullZonesOnVectorMap;
	static	bool	ms_displayAddCullZonesInWorld;
	static	bool	ms_bDrawNonCreationArea;
	static	bool	ms_displayCreationPosAttempts3D;
	static	bool	ms_displayConvertedPedLines;
	static  bool	ms_displayPedDummyStateContentness;
	static	bool	ms_forceScriptedPopulationOriginOverride;
	static	bool ms_forcePopControlSphere;
	static	bool ms_displayPopControlSphereDebugThisFrame;
	static	Vector3 	ms_vForcePopCenter;
	static	bool ms_bLoadCollisionAroundForcedPopCenter;	
	static	bool	ms_bDrawSpawnCollisionFailures;

	static bool m_bLogAlphaFade;

#endif // __BANK

	static	s32		ms_conversionDelayMsCivRefreshMin;
	static	s32		ms_conversionDelayMsCivRefreshMax;
	static	s32		ms_conversionDelayMsEmergencyRefreshMin;
	static	s32		ms_conversionDelayMsEmergencyRefreshMax;

	static	float	ms_maxSpeedBasedRemovalRateScale;
	static	float	ms_maxSpeedExpectedMax;
	static	s32		ms_removalDelayMsCivRefreshMin;
	static	s32		ms_removalDelayMsCivRefreshMax;
	static	s32		ms_removalDelayMsEmergencyRefreshMin;
	static	s32		ms_removalDelayMsEmergencyRefreshMax;
	static	s32		ms_removalMaxPedsInRadius;

	static	float	ms_removalClusterMaxRange;
	static	float	ms_pedMemoryBudgetMultiplier;
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	static float	ms_pedMemoryBudgetMultiplierNG;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	static	float	ms_convertRangeRealPedHardRadius;
	static	float	ms_convertRangeBaseDummyToReal;
	static	float	ms_convertRangeBaseRealToDummy;

	static bank_float ms_fCloseSpawnPointDistControlSq;
	static bank_float ms_fCloseSpawnPointDistControlOffsetSq;

	static bank_float m_fPedsPerMeterDensityLookup[9];
	static bank_s32 ms_minPedSlotsFreeForScenarios;

	static bank_float ms_minPatrollingLawRemovalDistanceSq;
	static bank_u32   ms_DefaultMaxMsAheadForForcedPopInit;
	static bank_float ms_fMinFadeDistance;
	static bank_bool  ms_bDontFadeDuringStartupMode;
	static u32 ms_uNumberOfFramesFaded;

	static s32 ms_iNumPedsCreatedThisCycle;
	static s32 ms_iNumPedsDestroyedThisCycle;

	static u32 GetPopulationFrameRate() { return NetworkInterface::IsGameInProgress() ? ms_iPedPopulationFrameRateMP : ms_iPedPopulationFrameRate; }
	static u32 GetPopulationCyclesPerFrame() { return NetworkInterface::IsGameInProgress() ? ms_iPedPopulationCyclesPerFrameMP : ms_iPedPopulationCyclesPerFrame; }
	static u32 GetMaxPopulationCyclesPerFrame() { return NetworkInterface::IsGameInProgress() ? ms_iMaxPedPopulationCyclesPerFrameMP : ms_iMaxPedPopulationCyclesPerFrame; }

private:
	static u32 ms_iPedPopulationFrameRate;
	static u32 ms_iPedPopulationCyclesPerFrame;
	static u32 ms_iMaxPedPopulationCyclesPerFrame;
	static u32 ms_iPedPopulationFrameRateMP;
	static u32 ms_iPedPopulationCyclesPerFrameMP;
	static u32 ms_iMaxPedPopulationCyclesPerFrameMP;
	static bank_float ms_fInstantFillExtraPopulationCycles;

public:
	// Population creation and convert.
	// (need access to this for the debug widget in pedVariationMgr)
	static	void	EquipPed										(CPed* pPed, bool giveWeapon, bool giveProps, bool giveAmbientProps, atFixedBitSet32* incs, atFixedBitSet32* excs, u32 setFlags, u32 clearFlags, const CAmbientModelVariations* variations = NULL, const CPedPropRestriction* pPassedPropRestriction = NULL);

	// Area helper functions

	// note: An array of <entity*> is <entity*>*.  The address of that array is therefore &<entity*>* or entity***.  Could use an atFixedArray
	// to make it less of a head fuck, but then we'd also need extra parameters to indicate the quick path [don't store what, just if there is]
	// and potentially unused stack storage.
	static	bool	GetCopsInArea									( const Vector3 &minBounds, const Vector3 &maxBounds, CPed ***pedArray, int maxPeds, int &pedCount, CVehicle ***vehicleArray, int maxVehicles, int &vehicleCount );

#if __BANK
	// counters used to track reasons for failing to create random peds
	class PedPopulationFailureData
	{
	public:
		enum FailureType
		{
			FR_notAllowdByNetwork = 0,
			FR_factoryFailedToCreatePed,
			FR_factoryFailedToCreateVehicle,
			FR_poolOutOfSpace,
			FR_noSpawnPosFound,
			FR_noPedModelFound,
			FR_vehicleHasNoSeatBone,
			FR_noDriverFallbackPedFound,
			FR_spawnPointOutsideCreationZone,
			FR_scenarioHasNoClipData,
			FR_spawnPointInBlockedArea,
			FR_visibletoMpPlayer,
			FR_spawnCollisionFail,
			FR_closeToMpPlayer,
			FR_scenarioPointTooFarAway,
			FR_scenarioOutsideFail,
			FR_scenarioInsideFail,
			FR_scenarioTimeAndProbabilityFailCarGen,
			FR_scenarioTimeAndProbabilityFailVarious,
			FR_scenarioTimeAndProbabilityFailSpawnInterval,
			FR_scenarioTimeAndProbabilityFailTimeOverride,
			FR_scenarioTimeAndProbabilityFailStreamingPressure,
			FR_scenarioTimeAndProbabilityFailConditions,
			FR_scenarioTimeAndProbabilityFailProb,
			FR_scenarioPointDisabled,
			FR_scenarioObjectBroken,
			FR_scenarioObjectUprooted,
			FR_scenarioObjectDoesntSpawnPeds,
			FR_scenarioPointInUse,
			FR_collisionChecks,
			FR_scenarioCarGenNotReady,
			FR_scenarioCarGenScriptDisabled,
			FR_scenarioPosNotLocallyControlled,
			FR_scenarioPosNotLocallyControlledCarGen,
			FR_scenarioVehModel,
			FR_scenarioVehPopLimits,
			FR_scenarioCollisionNotLoaded,
			FR_scenarioPopCarGenFailed,
			FR_scenarioNoPropFound,
			FR_scenarioInteriorNotStreamedIn,
			FR_scenarioNotInSameInterior,
			FR_scenarioCachedFailure,
			FR_scenarioPointRequiresImap,
			FR_dontSpawnCops,
			FR_scenarioCarGenBlockedChain,
			FR_Max
		};

		enum SpawnType
		{
			ST_Scenario = 0,
			ST_VehicleScenario,
			ST_Wandering
		};

		class FailureInfo
		{
		public:
			void Set(FailureType type, const char* szName, const char * szShortName, Color32 colour, bool bActive, bool bScenarioSpecific)
			{
				m_type = type;
				m_colour = colour;
				m_bActive = bActive;
				m_bScenarioSpecific = bScenarioSpecific;
				strncpy(m_szName, szName, 31);
				strncpy(m_szShortName, szShortName, 31);
				m_szName[31] = '\0';
				m_iCount = 0;
			}
			PedPopulationFailureData::FailureType m_type;
			char		m_szName[32];
			char		m_szShortName[32];
			Color32		m_colour;
			s32			m_iCount;
			bool		m_bActive;
			bool		m_bScenarioSpecific;
		};
		
		FailureInfo m_aFailureInfos[FR_Max];

		FailureInfo* GetFailureEnumDebugInfo(FailureType iFailure)
		{
			Assert(m_aFailureInfos[iFailure].m_type == iFailure);
			return &m_aFailureInfos[iFailure];
		}

		void RegisterSpawnFailure(SpawnType type, FailureType iFailure, Vector3::Param vPosition, const CScenarioPoint* pScenarioPt = nullptr);

		PedPopulationFailureData();

		void Reset(u32 newTime)
		{
			m_counterTimer = newTime;
			for( int i = 0; i < FR_Max; i++ )
			{
				m_aFailureInfos[i].m_iCount = 0;
			}
		}

		u32 m_counterTimer; // used to reset counters every second
	};
	static PedPopulationFailureData ms_populationFailureData;
#endif // __BANK

public:
	// Creation and removal zones.
	static	bool	PointFallsWithinNonCreationArea					(const Vector3& vec);

	static  bool	ShouldRemovePed( CPed* pPed, const Vector3& popCtrlCentre, float removalRangeScale, bool popCtrlIsInInterior, float removalRateScale, float& fOutCullRangeInView );
	static	bool	ShouldUseStartupMode();

	static void		RemovePedsOnRespawn_MP(CPed *pPed);

protected:
	static  bool	CanSuppressAggressiveCleanupForPed				(const CPed *pPed);
	static	bool	CanRemovePedAtPedsPosition						(CDynamicEntity* pPedOrDummyPed, float removalRangeScale, const Vector3& popCtrlCentre, bool popCtrlIsInInterior, float& cullRangeInView_out);
	static	void	CalculatePedCullRanges							(CPed* pPed, const bool popCtrlIsInInterior, bool& allowCullingWhenOutOfView_out, float& cullRangeinView_out, float& cullRangeOutOfView_out);
	static	bool	PointIsWithinCreationZones						(const Vector3& candidatePedRootPos, const Vector3& popCtrlCentre, const float fAddRangeOutOfViewMin, const float fAddRangeOutOfViewMax, const float fAddRangeInViewMin, const float fAddRangeInViewMax, const float fAddRangeFrustumExtra, const bool doInFrustumTest, const bool use2dDistCheck);
	static	bool	PointFallsWithinNonRemovalArea					(const Vector3& vec);

	// Population culling.
	static	void	PedsRemoveIfPoolTooFull							(const Vector3& popCtrlCentre);
	static	void	PedsRemoveOrMarkForRemovalOnVisImportance		(const Vector3& popCtrlCentre, float removalRangeScale, float removalRateScale, bool popCtrlIsInInterior);
	static	void	PedsRemoveIfRemovalIsPriority					(const Vector3& popCtrlCentre, float removalRangeScale);
	static	bool	IsPedARemovalCandidate							(CPed* pPed, const Vector3& popCtrlCentre, float removalRangeScale, bool popCtrlIsInInterior, float& cullRangeInView_out);
	static	bool	IsLawPedARemovalCandidate						(CPed* pPed, const Vector3& popCtrlCentre);
	static	bool	CanPedBeRemoved									(CPed* pPed);
	static	void	CopsRemoveOrMarkForRemovalOnPrefdNum			(const Vector3& popCtrlCentre, u32 numPrefdCopsAroundPlayer, float removalRangeScale, float removalRateScale, bool popCtrlIsInInterior);
	static	void	DeadPedsRemoveExcessCounts						(const Vector3& popCtrlCentre, u32 numMaxDeadAroundPlayer, u32 numMaxDeadGlobally);

	// Population count
	enum PedPopOp{ ADD = 0, SUB= 1};
	static	void	UpdatePedCountIfNotAlready						(CPed* pPed, PedPopOp addOrSub, bool inVehicle, u32 mi);
	// Ambient ped creation.
public:
	static  s32		GetDesiredMaxNumAmbientPeds						(s32 * iPopCycleMaxPeds=NULL, s32 * iPopCycleMaxCopPeds=NULL, s32 * iPopCycleTempMaxAmbientPeds=NULL);
	static  s32		HowManyMoreAmbientPedsShouldBeGenerated			(float * fScriptPopAreaMultiplier=NULL, float * fPedMemoryBudgetMultiplier=NULL);
protected:
	static	bool	AddAmbientPedToPopulation						(bool popCtrlIsInInterior, const float fAddRangeInViewMin, const float fAddRangeOutOfViewMin, const bool doInFrustumTest, const bool bCachedPointsRemaining, const bool atStart);
	static	bool	GetPedGenerationGroundCoord						(Vector3& newPedGroundPosOut, bool& bNewPedGroundPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float fAddRangeOutOfViewMin, float pedRadius, bool doInFrustumTest, s32& roomIdxOut, CInteriorInst*& pIntInstOut);
	static	void	SetUpParamsForPathServerPedGenBackgroundTask	(const CPopGenShape & popGenShape);
	static	bool	GetPedGenerationGroundCoordFromPathserver		(Vector3& newPedGroundPosOut, bool& bIsInInteriorOut, bool& bUsableIfOccludedOut);

	// Scenario point ped creation
public:
	static	float	GetClosestAllowedScenarioSpawnDist();
	static  float GetMinDistFromScenarioSpawnToDeadPed();
	static  int		SelectPointsRealType( const CScenarioPoint& spoint, const s32 iSpawnOptions, int& indexWithinType_Out);

	static const float& GetAddRangeExtendedScenarioExtended() { return ms_addRangeExtendedScenarioExtended; }
	static const float& GetAddRangeExtendedScenarioNormal() { return ms_addRangeExtendedScenarioNormal; }

	
	static bool SP_SpawnValidityCheck_General(const CScenarioPoint& scenarioPt, bool allowClusters);
	static bool SP_SpawnValidityCheck_ScenarioType(const CScenarioPoint& scenarioPt, const s32 scenarioTypeReal);
	static bool SP_SpawnValidityCheck_EntityAndTrainAttachementFind(const CScenarioPoint& scenarioPt, CTrain*& pAttachedTrain_OUT);
	static bool SP_SpawnValidityCheck_Interior(const CScenarioPoint& scenarioPt, const s32 iSpawnOptions);
	static bool SP_SpawnValidityCheck_ScenarioInfo(const CScenarioPoint& scenarioPt, const s32 scenarioTypeReal);

	struct SP_SpawnValidityCheck_PositionParams
	{
		SP_SpawnValidityCheck_PositionParams();

		Vector3 popCtrlCentre;
		float fAddRangeOutOfViewMin;
		float fAddRangeOutOfViewMax;
		float fAddRangeInViewMin;
		float fAddRangeInViewMax;
		float fAddRangeFrustumExtra;
		float faddRangeExtendedScenario;
		float fMaxRangeShortScenario;
		float fMaxRangeShortAndHighPriorityScenario;
		float fMinRangeShortScenarioInView;
		float fMinRangeShortScenarioOutOfView;
		float fRemovalRangeScale;
		float fRemovalRateScale;
		bool  bInAnInterior;
		bool  doInFrustumTest;
	};

	static bool SP_SpawnValidityCheck_Position(const CScenarioPoint& scenarioPt, const s32 scenarioTypeReal, const SP_SpawnValidityCheck_PositionParams& params);

	static bool ShouldUse2dDistCheck( const CPed* ped );
	static const SP_SpawnValidityCheck_PositionParams& SP_SpawnValidityCheck_GetCurrentParams() { return ms_CurrentCalculatedParams; }
private:
	static SP_SpawnValidityCheck_PositionParams ms_CurrentCalculatedParams; //Updated by CPedPopulation::Process
	static CPopCtrl ms_PopCtrl;

protected:
	static	s32		GeneratePedsAtScenarioPoints				(const Vector3& popCtrlCentre, const bool allowDeepInteriors, const float fAddRangeOutOfViewMin, const float fAddRangeOutOfViewMax, const float fAddRangeInViewMin, const float fAddRangeInViewMax, const float fAddRangeFrustumExtra, const bool doInFrustumTest, const s32 iMaxPeds, const s32 iMaxInteriorPeds);
	static  s32		GeneratePedsFromScenarioPointList			(const Vector3& popCtrlCentre, const bool allowDeepInteriors, const float fAddRangeOutOfViewMin, const float fAddRangeOutOfViewMax, const float fAddRangeInViewMin, const float fAddRangeInViewMax, const float fAddRangeFrustumExtra, const bool doInFrustumTest, const s32 iMaxPeds, const s32 iSpawnOptions = 0);
	static  s32		GeneratePedsFromScenarioPointList			(CScenarioPoint** areaPoints, const int pointcount, const Vector3& popCtrlCentre, const bool allowDeepInteriors, const float fAddRangeOutOfViewMin, const float fAddRangeOutOfViewMax, const float fAddRangeInViewMin, const float fAddRangeInViewMax, const float fAddRangeFrustumExtra, const bool doInFrustumTest, const s32 iMaxPeds, const s32 iSpawnOptions, int& iTriesInOut);
	static  s32		SpawnScheduledScenarioPed();
	static  void	DropScheduledScenarioPed();
	static  s32		SpawnScheduledAmbientPed(const float fAddRangeInViewMin);
	static  void	DropScheduledAmbientPed();
	static  s32		SpawnScheduledPedInVehicle(bool bSkipFrameCheck = false);
	static  void	DropScheduledPedInVehicle();
	static  void	ReschedulePedInVehicle();
	static	s32		ProcessPedDeletionQueue(bool deleteAll = false);
		
	static void		UpdatePedReusePool					();
	static void		FlushPedReusePool					();
	static void		DropOnePedFromReusePool			();

	static void		UpdateInVehAmbientExceptionsCount		();
	static bool		ShouldPedBeCountedAsInVehDead			(CPed* pPed);
	static bool		ShouldPedBeCountedAsInVehNoPretendModel	(CPed* pPed);

	static void		ReInitAPedAnimDirector				();
public:
	static int		FindPedToReuse						(u32 pedModel);
	static CPed*	GetPedFromReusePool					(int reusedPedIndex);
	static void		RemovePedFromReusePool				(int reusedPedIndex);
	static bool		ReusePedFromPedReusePool			(CPed* pPed, Matrix34& newMatrix, const CControlledByInfo& localControlInfo, bool createdByScript, bool shouldBeCloned, int pedReuseIndex, bool bScenarioPedCreatedByConcealeadPlayer = false);
	static void		AddPedToReusePool					(CPed* pPed);
	static bool		CanPedBeReused						(CPed* pPed, bool bSkipSeenCheck=false);


	// Clear out all of our ped creation/deletion queues
	static void		FlushPedQueues();
	// Get the number of peds in the deletion queue
	static u32		GetPedsPendingDeletion();
protected:

	// Vehicle ped creation.
	static	u32		ChooseCivilianPedModelIndexForVehicle			(CVehicle *pVeh, bool bDriver);

public:
	static  bool		SchedulePedInVehicle(CVehicle* pVehicle, s32 seat_num, bool canLeaveVehicle, u32 desiredModel, const CAmbientModelVariations* pVariations=NULL, CIncident* pDispatchIncident=NULL, DispatchType iDispatchType = (DispatchType)0, s32 pedGroupId = -1, COrder* pOrder = NULL);
	static	int			CountScheduledPedsForIncident(CIncident* pDispatchIncident, DispatchType iDispatchType = (DispatchType)0);
	static float		GetDistanceSqrToClosestScheduledPedWithModelId(const Vector3& pos, u32 modelIndex);

	enum OnPedSpawnedFlags
	{
		OPSF_InAmbient						= BIT0,
		OPSF_InVehicle						= BIT1,
		OPSF_InScenario						= BIT2,
		OPSF_ScenarioUsedSpecificModels		= BIT3,
	};
	static void OnPedSpawned(CPed& rPed, fwFlags8 uFlags = 0);

	static s32 GetAdjustPedSpawnDensity() { return ms_iAdjustPedSpawnDensityLastFrame; }
	static const Vector3 & GetAdjustPedSpawnDensityMin() { return ms_vAdjustPedSpawnDensityMin; }
	static const Vector3 & GetAdjustPedSpawnDensityMax() { return ms_vAdjustPedSpawnDensityMax; }

protected:
	static CPopGenShape ms_popGenKeyholeShape;

	static	u32		ms_uNextScheduledInVehAmbExceptionsScanTimeMS;
	
	static  u32		ms_timeWhenForcedPopInitStarted;
	static	u32		ms_timeWhenForcedPopInitEnds;
	static  bool	ms_bFirstForcePopFillIter;
	static	bool	ms_noPedSpawn;
	static	bool	ms_bVisualiseSpawnPoints;

	static	bank_bool ms_bPerformSpawnCollisionTest;

	static	float	ms_debugOverridePedDensityMultiplier;
	static	float	ms_pedDensityMultiplier;
	static	float	ms_scenarioPedInteriorDensityMultiplier;
	static	float	ms_scenarioPedExteriorDensityMultiplier;

	// Allow designers to adjust spawn mesh density values in a given AABB
	// Since the per-frame value is reset for part of the frame, and the ped-generation is meanwhile running asynchronously,
	// we cache and look at the the previous frame's value 'ms_iAdjustPedSpawnDensityLastFrame' which is not reset mid-frame
	static	s32		ms_iAdjustPedSpawnDensityThisFrame;
	static	s32		ms_iAdjustPedSpawnDensityLastFrame;
	static	Vector3		ms_vAdjustPedSpawnDensityMin;
	static	Vector3		ms_vAdjustPedSpawnDensityMax;

	static float	ms_fScriptedRangeMultiplierThisFrame;

	static	float	ms_scriptPedDensityMultiplierThisFrame;
	static	float	ms_scriptScenarioPedInteriorDensityMultiplierThisFrame;
	static	float	ms_scriptScenarioPedExteriorDensityMultiplierThisFrame;
	static	bool	ms_suppressAmbientPedAggressiveCleanupThisFrame;

	static	bool	ms_allowRevivingPersistentPeds;
	static	bool	ms_allowScenarioPeds;
	static	bool	ms_allowAmbientPeds;
	static	bool	ms_allowCreateRandomCops;
	static	bool	ms_allowCreateRandomCopsOnScenarios;
	static	bool	ms_bCreatingPedsForAmbientPopulation;
	static	bool	ms_bCreatingPedsForScenarios;
	static	s32		ms_forceCreateAllAmbientPedsThisType;
	static	u32		ms_maxPedsForEachPedTypeAvailableOnFoot;
	static	u32		ms_maxPedsForEachPedTypeAvailableInVeh;
	static	s32		ms_maxPedCreationAttemptsPerFrameOnInit;
	static	s32		ms_maxPedCreationAttemptsPerFrameNormal;
	static	s32		ms_maxScenarioPedCreationAttempsPerFrameOnInit;
	static	s32		ms_maxScenarioPedCreationAttempsPerFrameNormal;
	static	s32		ms_maxScenarioPedCreatedPerFrame;
	static	u32		ms_minTimeBetweenPriRemovalOfAmbientPedFromScenario;
	static	float	ms_allZoneScaler;
	static	float	ms_rangeScaleShallowInterior;
	static	float	ms_rangeScaleDeepInterior;
	static	float	ms_popCycleMaxPopulationScaler;

	static	float	ms_initAddRangeBaseMin;
	static	float	ms_initAddRangeBaseMax;
	static	float	ms_initAddRangeBaseFrustumExtra;
	static	float	ms_addRangeBaseOutOfViewMin;
	static	float	ms_addRangeBaseOutOfViewMax;
	static	float	ms_addRangeBaseInViewMin;
	static	float	ms_addRangeBaseInViewMax;
	static	float	ms_addRangeBaseFrustumExtra;
	static	float	ms_addRangeBaseGroupExtra;
	static	float	ms_addRangeExtendedScenarioEffective;	// Either the normal extended or extended extended range, that we currently want to use.
	static	float	ms_addRangeExtendedScenarioExtended;	// Spawn range for extended range scenarios when flying, etc.
	static	float	ms_addRangeExtendedScenarioNormal;		// Spawn range for extended range scenarios in normal on-foot or in-vehicle cases.
	static	float	ms_maxRangeShortRangeScenario;
	static  float   ms_maxRangeShortRangeAndHighPriorityScenario;
	static	float	ms_minRangeShortRangeScenarioInView;
	static	float	ms_minRangeShortRangeScenarioOutOfView;
	static	float	ms_addRangeScaleInViewSpeedBasedMax;
	static	float	ms_addRangeHighPriScenarioOutOfViewMin;
	static	float	ms_addRangeHighPriScenarioOutOfViewMax;
	static	float	ms_addRangeHighPriScenarioInViewMin;
	static	float	ms_addRangeHighPriScenarioInViewMax;

	static	bool	ms_bAlwaysAllowRemoveScenarioOutOfView;
	static	s32		ms_iMinNumSlotsLeftToAllowRemoveScenarioOutOfView;

	static	bool	ms_usePrefdNumMethodForCopRemoval;
	static	float	ms_cullRangeBaseOutOfView;
	static	float	ms_cullRangeBaseInView;
	static	float	ms_cullRangeScaleSpecialPeds;
	static	float	ms_cullRangeScaleCops;
	static	float	ms_cullRangeExtendedInViewMin;

	static	bool	ms_useStartupModeDueToWarping;
	static	bool	ms_useTempPopControlPointThisFrame;
public:
	static	bool	ms_useTempPopControlSphereThisFrame;	
	static	float	ms_TempPopControlSphereRadius;
protected:
	static  CPopCtrl ms_tmpPopOrigin;
	static	s32		ms_bInstantFillPopulation;

	static	bool	ms_scriptedConversionCentreActive;
	static	Vector3	ms_scriptedConversionCenter;

	static	bool	ms_nonCreationAreaActive;
	static	Vector3	ms_nonCreationAreaMin;
	static	Vector3	ms_nonCreationAreaMax;

	static	bool	ms_nonRemovalAreaActive;
	static	Vector3	ms_nonRemovalAreaMin;
	static	Vector3	ms_nonRemovalAreaMax;

	static	bool	ms_allowDummyCoversions;
	static	bool	ms_useNumPedsOverConvertRanges;
	static	bool	ms_useTaskBasedConvertRanges;
	static	u32		ms_preferredNumRealPedsAroundPlayer;
	static	u32		ms_lowFrameRateNumRealPedsAroundPlayer;
	static	bool	ms_useHardDummyLimitOnLowFrameRate;
	static	bool	ms_useHardDummyLimitAllTheTime;

	static	bool	ms_aboveAmbientPedLimit;
	static	bool	ms_atScenarioPedLimit;
	static	u32		ms_nextAllowedPriRemovalOfAmbientPedFromScenario;

	static  bool	ms_bPopulationDisabled;

	static  Vector3* ms_vPedPositionsArray;	// Pointer to array of MAXNOOFPEDS elements.
	static  u32* ms_iPedFlagsArray;			// Pointer to array of MAXNOOFPEDS elements.

	static	float	ms_pedRemovalTimeSliceFactor;
	static	s32		ms_pedRemovalIndex;

	static const ScalarV ms_clusteredPedVehicleRemovalRangeSq;

	static bool		ms_extendDelayWithinOuterRadius;
	static u32		ms_extendDelayWithinOuterRadiusTime;
	static float	ms_extendDelayWithinOuterRadiusExtraDist;

    static u32      ms_disableMPVisibilityTestTimer;
    static u32      ms_mpVisibilityFailureCount;
    static u32      ms_lastMPVisibilityFailStartTime;
	static Vector3 ms_vCachedScenarioPointCenter;
#define NUM_CACHED_SCENARIO_POINTS (768)
	static atFixedArray<CScenarioPoint*, NUM_CACHED_SCENARIO_POINTS> ms_cachedScenarioPoints;
	static u32	ms_cachedScenarioPointFrame;
	static u32	ms_numHighPriorityScenarioPoints;
	static u32	ms_lowPriorityScenarioPointsAlreadyChecked;
	static bool	ms_bInvalidateCachedScenarioPoints;

public:
	static void InvalidateCachedScenarioPoints()
	{
		ms_bInvalidateCachedScenarioPoints = true;
	}
#if __BANK
public:
	static bool		ms_showDebugLog;
	static bool		ms_bDisplayMultipliers;

	struct sDebugMessage
	{
		char msg[128];
		char timeStr[16];
		u32 time;
	};
	static u32				ms_nextDebugMessage;
	static sDebugMessage	ms_debugMessageHistory[64];
#endif

#if __BANK
	static bool ms_bDebugPedReusePool;
	static bool ms_bDisplayPedReusePool;
#endif

protected:
};

#endif // PEDPOPULATION_H_
