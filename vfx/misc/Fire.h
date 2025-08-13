///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Fire.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	14 March 2006
//	WHAT:	Fire processing and rendering
//
///////////////////////////////////////////////////////////////////////////////

#ifndef FIRE_H
#define	FIRE_H

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

#if !__SPU

// rage
#include "fwutil/idgen.h"
#include "rmptfx/ptxeffectinst.h"
#include "script/thread.h"

// game
#include "Audio/FireAudioEntity.h"
#include "Network/General/NetworkFXId.h"
#include "Pathserver/PathServer_DataTypes.h"
#include "Scene/Entity.h"
#include "Scene/RegdRefTypes.h"
#include "fwscene/search/Search.h"

#endif // !__SPU


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////													

class CEntity;
class CObject;
class CPed;
class CVehicle;

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////		

#define MAX_FIRES						(128)									// maximum number of fires
#define MAX_OBJECTS_ON_FIRE				(32)
#define	FIRE_MAP_GRID_SIZE				(128)									// the number of blocks in the fire grid (for keeping track of burnt out areas)
#define FIRE_DEFAULT_NUM_GENERATIONS	(25)									// the default number of generations that a fire can create when triggered from game (i.e. when not spreading)


enum FireType_e
{
	FIRETYPE_UNDEFINED = -1,

	// timed fires
	FIRETYPE_TIMED_MAP = 0,
	FIRETYPE_TIMED_PETROL_TRAIL,
	FIRETYPE_TIMED_PETROL_POOL,
	FIRETYPE_TIMED_PED,
	FIRETYPE_TIMED_VEH_WRECKED,
	FIRETYPE_TIMED_VEH_WRECKED_2,
	FIRETYPE_TIMED_VEH_WRECKED_3,
	FIRETYPE_TIMED_VEH_GENERIC,
	FIRETYPE_TIMED_OBJ_GENERIC,

	// registered fires
	FIRETYPE_REGD_VEH_WHEEL,
	FIRETYPE_REGD_VEH_PETROL_TANK,
	FIRETYPE_REGD_FROM_PTFX,

	FIRETYPE_MAX
};


enum FireFlags_e
{
	// states
	FIRE_FLAG_IS_ACTIVE		= 0,
	FIRE_FLAG_IS_DORMANT,			// used during network game, fires are dormant until it is confirmed that they can be activated properly

	// info
	FIRE_FLAG_IS_SCRIPTED,
	FIRE_FLAG_IS_SCRIPTED_PETROL_FIRE,
	FIRE_FLAG_IS_BEING_EXTINGUISHED,
	FIRE_FLAG_IS_INTERIOR_FIRE,
	FIRE_FLAG_IS_ATTACHED,
	FIRE_FLAG_IS_REGISTERED,
	FIRE_FLAG_REGISTERED_THIS_FRAME,
	FIRE_FLAG_DONT_UPDATE_TIME_THIS_FRAME,
	FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED,
	FIRE_FLAG_IS_CONTAINED,
	FIRE_FLAG_IS_FLAME_THROWER,
	FIRE_FLAG_CULPRIT_CLEARED,
#if GTA_REPLAY
	FIRE_FLAG_IS_REPLAY,
#endif // GTA_REPLAY
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

struct fireMapStatusGridData
{
	float	m_timer;
	float	m_height;
};

struct fireObjectStatusData
{
	RegdObj m_pObject;
	float m_canBurnTimer;								// counts down whilst the object is allowed to burn (since it was first set alight)
	float m_dontBurnTimer;								// counts down whilst the object isn't allowed to burn again
};

class CFireSettings
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// constructor
		CFireSettings()
		{
			vPosLcl = Vec3V(V_ZERO);
			vNormLcl = Vec3V(V_Z_AXIS_WZERO);
			pEntity = NULL;
			boneIndex = -1;
			fireType = FIRETYPE_TIMED_MAP;
			parentFireType = FIRETYPE_UNDEFINED;
			mtlId = 0; //PGTAMATERIALMGR->g_idDefault
			burnTime = 10.0f;
			burnStrength = 1.0f;
			peakTime = 1.0f;
			numGenerations = 0;
			vParentPos = Vec3V(V_ZERO);
#if __BANK
			vRootPos = Vec3V(V_ZERO);
#endif
			fuelLevel = 0.0f;
			fuelBurnRate = 0.0f;
			regOwner = NULL;
			regOffset = 0;
			pCulprit = NULL;
			pNetIdentifier = NULL;
			isContained = false;
			isFlameThrower = false;
			hasSpreadFromScriptedFire = false;
			hasCulpritBeenCleared = false;
			weaponHash = 0;
		}

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		
	
	public: ///////////////////////////

		// placement
		Vec3V vPosLcl;
		Vec3V vNormLcl;
		CEntity* pEntity;
		s32 boneIndex;

		// type
		FireType_e fireType;
		FireType_e parentFireType;

		// burn info
		phMaterialMgr::Id mtlId;				// the material that this fire is on (only set on map fires)
		float burnTime;							// the time that this fire burns for
		float burnStrength;						// the strength at which this fire burns (at maximum)
		float peakTime;							// the time at which the fire reaches peak strength

		// hierarchy 
		s32 numGenerations; 
		Vec3V vParentPos;
#if __BANK
		Vec3V vRootPos;
#endif

		// fuel info
		float fuelLevel;						// the current amount of fuel this fire has
		float fuelBurnRate;						// how much fuel burns per second

		// registration 
		void* regOwner;
		u32 regOffset;

		// misc
		CPed* pCulprit;
		CNetFXIdentifier* pNetIdentifier;
		bool isContained;
		bool isFlameThrower;
		bool hasSpreadFromScriptedFire;
		bool hasCulpritBeenCleared;
		u32 weaponHash;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CFire  ////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

#if !__SPU

class CFire
{	
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class				CFireManager;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		bool				Init					(const CFireSettings& fireSettings);
		void				Shutdown				(bool finishPtFx=false);
		void				Update					(float deltaTime);
#if __BANK
		void				RenderDebug				() const;
#endif

		// access functions
		FireType_e			GetFireType				() const					{return m_fireType;}
		phMaterialMgr::Id	GetMtlId				() const					{return m_mtlId;}

		Mat34V_Out			GetMatrixOffset			() const;
		Vec3V_Out 			GetPositionWorld		() const;
		void				SetPositionLocal		(Vec3V_In vPosLcl)			{m_vPosLcl = vPosLcl;}
		Vec3V_Out 			GetPositionLocal		() const					{return m_vPosLcl;}
		Vec3V_Out			GetParentPositionWorld	() const					{return m_vParentPosWld;}

		CEntity*			GetEntity				() const					{return m_regdEntity;}
		s32					GetBoneIndex			() const					{return m_boneIndex;}
		CPed*				GetCulprit				() const					{return m_regdCulprit;}
		void				SetCulprit				(CPed* pCulprit)			{m_regdCulprit = pCulprit;}
		u32					GetWeaponHash			() const					{return m_weaponHash;}
		void				SetWeaonHash			(u32 weaponHash)			{m_weaponHash = weaponHash;}
		s16					GetNumGenerations		() const					{return m_numGenerations;}
		float				GetCurrStrength			() const					{return m_currStrength;}
		float				GetCurrBurnTime			() const					{return m_currBurnTime;}
		float				CalcCurrRadius			() const;
		float				GetMaxRadius			() const;
		float				GetBurnTime				() const					{return m_totalBurnTime;}
		float				GetLifeRatio			() const					{return m_currBurnTime/m_totalBurnTime;}
		float				GetMaxStrengthRatio		() const					{return m_currBurnTime<m_maxStrengthTime ? m_currStrength/m_maxStrength: 1.0f;}
		float				GetBurnStrength			() const					{return m_maxStrength;}
		float				GetFuelLevel			() const					{return m_fuelLevel;}

		void*				GetRegOwner				() const					{return m_regOwner;}
		fwUniqueObjId		GetRegOffset			() const					{return m_regOffset;}
		fwUniqueObjId		GetRegId				() const					{return fwIdKeyGenerator::Get(m_regOwner, m_regOffset);}

		Vec4V_Out			GetLimbBurnRatios		() const					{return m_vLimbBurnRatios;}

		u16					GetScriptRefIndex		() const					{return m_scriptRefIndex;}
		void				SetScriptRefIndex		(u16 scriptRefIndex)		{m_scriptRefIndex = scriptRefIndex;}
		CNetFXIdentifier&	GetNetworkIdentifier	()							{return m_networkIdentifier;}

		bool				IsAudioRegistered		()							{return m_audioRegistered;}
		void				SetAudioRegistered		(bool registered)			{m_audioRegistered = registered;}
		bool				CanDispatchFireTruckTo	();
		void				RegisterInUseByDispatch();

		void				SetReportedIncident		(CIncident* pIncident)		{ Assert(!m_reportedIncident); m_reportedIncident = pIncident;}
		CIncident*			GetReportedIncident		() const					{  return m_reportedIncident;}

		void				ActivateDormantFire		();

inline	void				SetPathServerObjectIndex(const TDynamicObjectIndex id) {m_pathServerDynObjId = id;}
inline	TDynamicObjectIndex GetPathServerObjectIndex() const					{return m_pathServerDynObjId;}

		// flag accessors
		bool				GetFlag					(u32 flag) const			{return (m_flags & (1<<flag)) != 0;}
		void				SetFlag					(u32 flag)					{m_flags |= (1<<flag);}
		void				ClearFlag				(u32 flag)					{m_flags &= ~(1<<flag);}


	private: //////////////////////////

		void				Spread					(bool onlySpreadToPetrol);
		void				ProcessVfx				();
		
		float				CalcStrengthGenScalar	() const;
		bool				FindMtlBurnInfo			(float& burnTime, float& burnStrength, float& peakTime, phMaterialMgr::Id groundMtl, bool isPetrol) const;

		void				ProcessAsynchProbes		();
		void				TriggerScorchMarkProbe	();
		void				TriggerGroundInfoProbe	(Vec3V_In vTestPos, CEntity* pPetrolEntity, bool foundPetrol, bool foundPetrolPool);
		void				TriggerUpwardProbe		(Vec3V_In vStartPos, Vec3V_In vEndPos);

		//Fire spreading search for object around
		//
		// Start asynchronous search
		void				StartAsyncFireSearch();
		// End asynchronous search
		void				EndAsyncFireSearch();
		// Call call back function for spreading the fire
		void				CollectFireSearchResults();
		//Request a search of objects around the fire to propagate the fire on other objects
		void				RequestAsyncFireSearch() {m_searchRequested = true;}
	
	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		
	
	private: //////////////////////////

		// base variables
		Vec3V				m_vPosLcl;
		Vec3V				m_vNormLcl;
		Vec3V				m_vParentPosWld;	
		RegdEnt				m_regdEntity;										// the entity that is on fire (null means no entity)
		s32					m_boneIndex;										// the bone index of the entity that the fire is on	(-1 means no bone)

		phMaterialMgr::Id	m_mtlId;											// the material that this fire is on (only set on map fires)

		FireType_e			m_fireType;											// the type of fire 
		FireType_e			m_parentFireType;									// the type of this fire's parent 
		RegdPed				m_regdCulprit;										// the culprit of the fire (who/what started it)
		u32					m_weaponHash;										// the weapon used to start the fire
		float				m_currStrength;										// current strength of the fire
		s16					m_numGenerations;									// number of generations this fire can spawn
		s16					m_numChildren;										// number of children this fire has spawned

		float				m_maxRadius;										// the max radius of the fire if it's defined externally (e.g. from vehicle wheel, petrol tanks and wrecked fires)

		// fuel 
		float				m_fuelLevel;										// the current amount of fuel this fire has
		float				m_fuelBurnRate;										// how much fuel burns per second

		// update rate
		u32					m_entityTestTime;									// the next time we're allowed to test for intersections with entities

		// flags
		u16					m_flags;

		// script variables
		u16					m_scriptRefIndex;									// used by script

		// data from other systems
		TDynamicObjectIndex m_pathServerDynObjId;								// id of the fire within the path server
		audFireAudioEntity*	m_pAudioEntity;
		CNetFXIdentifier	m_networkIdentifier;								// used to identify this fire across the network 
		RegdIncident		m_reportedIncident;									// used to track the incident added to the dispatch system for this fire

		// timed variables
		float				m_totalBurnTime;									// the total time that this fire burns for (seconds)
		float  				m_currBurnTime;										// the current time the fire has burned for (seconds)
		float				m_maxStrength;										// maximum strength of the fire
		float				m_maxStrengthTime;									// the time it takes for the fire to reach full strength
		float				m_spreadChanceAccumTime;							// the accumulated time since the last chance of spreading

		//Fires are searching objects/Peds/Vehicles around themselves to detect the one close enough to spread on.  This 
		// class is used to accelerate the search by removing the need to wait for the results.
		fwSearch*			m_search;

		// registered variables
		fwUniqueObjId		m_regOffset;										// the registration offset of a registered fire
		void*				m_regOwner;											// the registration owner of a registered fire (the owner and offset combine to create a unique id)

		// ped variables
		Vec4V				m_vLimbBurnRatios;									// how long each limb fire will last (as a ratio of the main fire)

		// async probe variables
		CEntity*			m_pGroundInfoVfxProbePetrolEntity;
		s8					m_groundInfoVfxProbeId;
		bool				m_groundInfoVfxProbeFoundPetrol;
		bool				m_groundInfoVfxProbeFoundPetrolPool;
		s8					m_scorchMarkVfxProbeId;
		s8					m_upwardVfxProbeId;
		
		// audio 
		bool				m_audioRegistered;

		// nearby entity spreading
		bool				m_searchRequested;									// has a search been requested
		bool				m_searchDataReady;									// is the search data ready

#if __BANK	
		Vec3V				m_vRootPosWld;		
#endif
}; // CFire



//  CFireManager  /////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CFireManager
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		bool				Init					(unsigned initMode);
		void				Shutdown				(unsigned shutdownMode);

		void				Update					(float deltaTime);
		void				UpdateTask				(float deltaTime);

		void				ActivateFireCB			(CFire* pFire);
		void				DeactivateFireCB		(CFire* pFire);

#if __BANK
		void				InitWidgets				();
		void				RenderDebug				();
#endif

		// timed fire interface
		CFire*				StartMapFire			(Vec3V_In vFirePos, 
													 Vec3V_In vFireNormal,
													 phMaterialMgr::Id mtlId,
													 CPed* pCulprit, 
													 float burnTime,
													 float burnStrength,
													 float peakTime,
													 float fuelLevel,
													 float fuelBurnRate,
													 s32 numGenerations,
													 Vec3V_In vParentPos,
													 BANK_ONLY(Vec3V_In vRootPos,)
													 bool hasSpreadFromScriptedFire,
													 u32 weaponHash = 0,
													 bool hasCulpritBeenCleared = false);
		CFire*				StartPetrolFire			(CEntity* pEntity,
													 Vec3V_In vPosWld, 
													 Vec3V_In vNormWld,
													 CPed* pCulprit,
													 bool isPool,
													 Vec3V_In vParentPos,
													 BANK_ONLY(Vec3V_In vRootPos,)
													 bool hasSpreadFromScriptedFire,
													 FireType_e parentFireType=FIRETYPE_UNDEFINED,
													 bool hasCulpritBeenCleared = false);
		CFire*				StartPedFire			(CPed* pPed, 
													 CPed* pCulprit,
													 s32 numGenerations,
													 Vec3V_In vParentPos,
													 BANK_ONLY(Vec3V_In vRootPos,)
													 bool hasSpreadFromScriptedFire,
													 bool hasSpreadFromExplosion,
													 u32 weaponHash = 0,
													 float currBurnTime = 0.0f,
													 bool startAtMaxStrength = false,
													 bool bReportCrimes = true,
													 bool hasCulpritBeenCleared = false);
		CFire*				StartVehicleWreckedFire	(CVehicle* pVehicle, 
													 Vec3V_In vPosLcl,
													 s32 wreckedFireId,
													 CPed* pCulprit,
													 float burnTime,
													 float maxRadius,
													 s32 numGenerations);
		CFire*				StartVehicleFire		(CVehicle* pVehicle, 
													 s32 boneIndex,
													 Vec3V_In vPosLcl,
													 Vec3V_In vNormLcl,
													 CPed* pCulprit,
													 float burnTime,
													 float burnStrength,
													 float peakTime,
													 s32 numGenerations,
													 Vec3V_In vParentPos,
													 BANK_ONLY(Vec3V_In vRootPos,)
													 bool hasSpreadFromScriptedFire,
													 u32 WeaponHash = 0);
		CFire*				StartObjectFire			(CObject* pObject, 
													 s32 boneIndex,
													 Vec3V_In vPosLcl,
													 Vec3V_In vNormLcl,
													 CPed* pCulprit,
													 float burnTime,
													 float burnStrength,
													 float peakTime,
													 s32 numGenerations,
													 Vec3V_In vParentPos,
													 BANK_ONLY(Vec3V_In vRootPos,)
													 bool hasSpreadFromScriptedFire,
													 u32 weaponHash = 0,
													 bool hasCulpritBeenCleared = false);

		CFire*				StartFire				(const CFireSettings& fireSettings);

		// registered fires
		CFire*				RegisterVehicleTyreFire	(CVehicle* pVehicle, 
													 s32 boneIndex,
													 Vec3V_In vPosLcl, 
													 void* pRegOwner,
													 u32 regOffset,
													 float currStrength,
													 bool hasCulpritBeenCleared = false);
		CFire*				RegisterVehicleTankFire	(CVehicle* pVehicle, 
													 s32 boneIndex,
													 Vec3V_In vPosLcl, 
													 void* pRegOwner,
													 u32 regOffset,
													 float currStrength,
													 CPed* pCulprit);
		CFire*				RegisterPtFxFire		(CEntity* pEntity,
													 Vec3V_In vPosLcl,
													 void* pRegOwner,
													 u32 regOffset,
													 float currStrength,
													 bool isContained,
													 bool isFlameThrower,
													 CPed* pCulprit = NULL);

		CFire*				RegisterFire			(const CFireSettings& fireSettings);


		// management
		bool				ManageAddedPetrol		(Vec3V_In vPos, float dist);

		// spreading
		void				SetScriptFlammabilityMultiplier(float value, scrThreadId threadId);
		float 				GetScriptFlammabilityMultiplier();


		// extinguishing
		void				ExtinguishAll			(bool finishPtFx=false);
		void				ExtinguishArea			(Vec3V_In vPosition, float radius, bool finishPtFx=false);
		bool				ExtinguishAreaSlowly	(Vec3V_In vPosition, float radius);
		bool				ExtinguishAreaSlowly	(ptxDataVolume& dataVolume);
		void				ExtinguishEntityFires	(CEntity* pEntity, bool finishPtFx=false);

		// access functions
		u32					GetNumActiveFires		()												{return m_numFiresActive;}
		CFire*				GetActiveFire			(s32 activeFireIndex);
		CFire&				GetFire					(s32 index);	
		CFire*				GetFire					(const CNetFXIdentifier& netIdentifier);		
		
		void				GetMapStatusGridIndices	(Vec3V_In vQueryPos, s32& gridX, s32& gridY);
inline	fireMapStatusGridData&	GetMapStatusGrid			(s32 x, s32 y)									{return m_mapStatusGrid[x][y];}
inline	void				SetMapStatusGrid			(s32 x, s32 y, float timeVal, float heightVal)	{m_mapStatusGridActive=true; m_mapStatusGrid[x][y].m_timer = timeVal; m_mapStatusGrid[x][y].m_height = heightVal;}

		fireObjectStatusData* GetObjectStatus		(CObject* pObject);
		bool				SetObjectStatus			(CObject* pObject, float canBurnTime, float dontBurnTime);

		// query functions
		bool 				PlentyFiresAvailable	();
		s32					GetNumFiresInRange		(Vec3V_In vPos, float range);
		s32					GetNumFiresInArea		(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
		u32					GetNumNonScriptFires	();
		u32					GetNumWreckedFires		();
		CFire*				FindNearestFire			(Vec3V_In vSearchPos, float& nearestDistSqr);	// if a more advanced version of this is required then used GetNumActiveFires() and GetActiveFire() to iterate through the fires manually
		bool				IsEntityOnFire			(const CEntity* pEntity)						{return pEntity->m_nFlags.bIsOnFire;}
		CFire*				GetEntityFire			(CEntity* pEntity);
		float				GetClosestEntityFireDist(CEntity* pEntity, Vec3V_In vTestPos);
		bool				GetClosestFirePos		(Vec3V_InOut vClosestFirePos, Vec3V_In vTestPos);

		// script functions
		void				RemoveScript			(scrThreadId scriptThreadId);
		s32					StartScriptFire			(Vec3V_In vPos, 
													 CEntity* pEntity = NULL, 
													 float burnTime = 0.0f,
													 float burnStrength = 1.0f,
													 s32 numGenerations = 5,
													 bool isPetrolFire = false);

		void				RemoveScriptFire		(s32 fireIndex);
		void				ClearScriptFireFlag		(s32 fireIndex);
		Vec3V_Out			GetScriptFireCoords		(s32 fireIndex);
//		bool				IsScriptFireExtinguished(s32 fireIndex);

		//Call back function to spread fire around 
		static	bool		FireNextToEntityCB		(fwEntity* entity, void* data);

		//To avoid waiting on our fire search for objects around, we must call those two functions (start and finalize) with enough cpu interval in between
		// in order to hide the search.
		// Start the fire searches for spreading
		void				StartFireSearches		();
		// End the fire searches for spreading
		void				FinalizeFireSearches	();

		// Process the stored fire search results.
		static void			ProcessAllEntitiesNextToFires();

		void				ClearCulpritFromAllFires(const CEntity* pCulprit);

		void				UnregisterPathserverID(const TDynamicObjectIndex dynObjId);

		static bool			CanSetPedOnFire(CPed* pPed);

#if GTA_REPLAY
		void				ClearReplayFireFlag		(s32 fireIndex);

		void SetReplayIsScriptedFire(bool isScripted) { m_replayIsScriptedFire = isScripted;}
		void SetReplayIsPetrolFire(bool isPetrol) { m_replayIsPetrolFire = isPetrol;}
		bool ReplayIsScriptedFire() { return m_replayIsScriptedFire;}
		bool ReplayIsPetrolFire() { return m_replayIsPetrolFire;}
#endif //GTA_REPLAY

	private: //////////////////////////

		CFire*				GetNextFreeFire			(bool isScriptRequest);
		CFire*				FindRegisteredFire		(fwUniqueObjId regdId);

		s32					GetFireIndex(CFire* pFire);		//Used for Replay to identify fire to track it expiring.

		static void			AddEntityNextToFireResult(CEntity* pEntity, CFire* pFire);

		void				ProcessPedNextToFire	(CPed* pPed, CFire* pFire);
		void				ProcessVehicleNextToFire(CVehicle* pVehicle, CFire* pFire);
		void				ProcessObjectNextToFire	(CObject* pObject, CFire* pFire);

		void				ManageWreckedFires		();

#if __BANK
static	void				StartDebugMapFire		();
static	void				StartDebugScriptFire	();
static	void				StartDebugFocusPedFire	();
static	void				ExtinguishAreaAroundPlayer();
static	void				RemoveFromAreaAroundPlayer();
static	void				ResetStatusGrid			();
static	void				ResetObjectStatus		();
static	void				OutputStatusGrid		();
		void				RenderStatusGrid		();
		void				RenderObjectStatus		();
static	void				Reset					();
#endif


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	public: ///////////////////////////

#if __BANK | GTA_REPLAY
		bool				m_disableAllSpreading;
#endif

#if __BANK
		s32					m_numScriptedFiresActive;
		s32					m_numScriptedFires;

		float				m_debugBurnTime;
		float				m_debugBurnStrength;
		float				m_debugPeakTime;
		float				m_debugFuelLevel;
		float				m_debugFuelBurnRate;
		s32					m_debugNumGenerations;

		float				m_overrideMtlFlammability;
		float				m_overrideMtlBurnTime;
		float				m_overrideMtlBurnStrength;

		bool				m_disableDecals;
		bool				m_disablePtFx;
		bool				m_disableGroundSpreading;
		bool				m_disableUpwardSpreading;
		bool				m_disableObjectSpreading;
		bool				m_ignoreGenerations;
		bool				m_ignoreMaxChildren;
		bool				m_ignoreStatusGrid;
		bool				m_renderDebugSpheres;
		bool				m_renderDebugParents;
		bool				m_renderDebugRoots;
		bool				m_renderDebugSpreadProbes;
		bool				m_renderStartFireProbes;
		bool				m_renderMapStatusGrid;
		float				m_renderMapStatusGridHeight;
		float				m_renderMapStatusGridAlpha;
		float				m_renderMapStatusGridMinZ;
		bool				m_renderObjectStatus;
		bool				m_renderPedNextToFireTests;
		bool				m_renderVehicleNextToFireTests;
#endif


	private: //////////////////////////

		CFire				m_fires					[MAX_FIRES];				// array of fires 

		s32					m_numFiresActive;
		s32					m_activeFireIndices		[MAX_FIRES];

		bool				m_mapStatusGridActive;
		fireMapStatusGridData	m_mapStatusGrid			[FIRE_MAP_GRID_SIZE][FIRE_MAP_GRID_SIZE] ;

		bool				m_objectStatusActive;
		fireObjectStatusData m_objectStatus		[MAX_OBJECTS_ON_FIRE];

		// Array of ref-counted entity pointers. Entities that were found to be next to a fire 
		// during the fire search, we'll process them later when the scene graph is unlocked.
		struct EntityNextToFire 
		{
			EntityNextToFire() 
				: m_pEntity(), 
				m_pFire(NULL)
			{}

			EntityNextToFire(CEntity* pEntity, CFire* pFire)
				: m_pEntity(pEntity), 
				m_pFire(pFire)
			{}

			RegdEnt m_pEntity;
			CFire* m_pFire;
		};
		static atArray<EntityNextToFire> m_EntitiesNextToFires;


		scrThreadId	m_scriptFlammabilityMultiplierScriptThreadId;
		f32			m_scriptFlammabilityMultiplier;


#if GTA_REPLAY
		bool m_replayIsScriptedFire;
		bool m_replayIsPetrolFire;
#endif //GTA_REPLAY

}; // CFireManager



///////////////////////////////////////////////////////////////////////////////
//	EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CFireManager g_fireMan;

extern dev_float FIRE_PETROL_BURN_TIME;
extern dev_float FIRE_PETROL_BURN_STRENGTH;
extern dev_float FIRE_PETROL_PEAK_TIME;
extern dev_float FIRE_PETROL_SPREAD_LIFE_RATIO;
extern dev_float FIRE_PETROL_SEARCH_DIST;

#endif // __SPU

#endif // FIRE_H


///////////////////////////////////////////////////////////////////////////////
