//
// filename:		physics.h
// description:		Class controlling all physics code
#ifndef INC_PHYSICS_H_
#define INC_PHYSICS_H_

// C headers
// Rage headers
#include "atl/array.h"
#include "physics/levelnew.h"
#include "physics/simulator.h"
#include "fragment/instance.h"			// for fragInst::Collision
#include "phframework/inst.h"
#include "streaming/streaminginfo.h"	// for STORE_NAME_LENGTH

// Game headers
#include "pathserver/NavGenParam.h"		// for NAVMESH_EXPORT
#include "scene/RegdRefTypes.h"
#include "scene/portals/InteriorInst.h"
#include "templates/linklist.h"
#include "fwscene/search/Search.h"
#include "vehicles/VehicleDefines.h"
#include "WorldProbe/wpcommon.h"

// debug headers
#if __DEV && !__SPU
#include "debug/DebugDrawStore.h"
#endif

#define PHYSICS_REQUEST_LIST_ENABLED (NAVMESH_EXPORT)

// GTA_RAGE_PHYSICS
#define USE_FRAG_TUNE (1)

// This value is set up in max script so we must stay in sync with it
// Artists specify each vert's depth in range 0-1
// And that value is multiplied by this number
#define MAX_SECOND_SURFACE_DEPTH (1.0f)

// Class prototypes
namespace rage
{
	class fragManager;
	class clothManager;
	class ropeManager;
#if __BANK && !__RESOURCECOMPILER
	class ropeBankManager;
#endif
	class verletTaskManager;
	class bkBank;
	class bkButton;
	class fwPtrList;
	class phMouseInput;
}

class CEntity;
class CObject;
class fragInstGta;

struct VfxCollisionInfo_s;

class CPhysics
{
#if NAVMESH_EXPORT
	// TODO: Consider replacing these friend declarations with accessors. /FF
	friend class CNavMeshDataExporterInGame;
	friend class CNavMeshDataExporterTool;
	friend class CAudMeshDataExporterTool;
#endif

public:
	enum {
		MAX_OCTREE_NODES				= 1500,
		MAX_PHYS_INST_BEHAVIOURS		= 256,
		MAX_ACTIVE_COBJECTS				= 64,
		MAX_FRAGMENT_CACHE_OBJECTS		= 64,		//MAX_ACTIVE_OBJECTS=(256),
		MAX_MANIFOLDS_PER_ACTIVE_OBJECT	= 2,
		MAX_CONTACTS_PER_MANIFOLD		= 4,
		COMPOSITE_MANIFOLD_RATIO		= 2,
		EXTERN_VELOCITY_MANIFOLDS		= 256
	};

	// Script interface to gravity is through an enum
	// Keep this in sync with commands_misc.sch
	enum eGravityLevel{
		GRAV_EARTH = 0,
		GRAV_MOON,
		GRAV_LOW,
		GRAV_ZERO,
		NUM_GRAVITY_LEVELS
	};

	struct PreSimControl
	{
		enum {
			MAX_POSTPONED_ENTITIES = 50
		};

		// This used to be MAX_SECONDPASS_ENTITIES, but was changed to a function
		// now when it's not known at compile time (VEHICLE_POOL_SIZE may be changed through
		// the configuration file).
		static int GetMaxSecondPassEntities()
		{	return VEHICLE_POOL_SIZE;	}
		static int GetMaxThirdPassEntities()
		{	return VEHICLE_POOL_SIZE;	}

		CPhysical* aPostponedEntities[MAX_POSTPONED_ENTITIES];
		int nNumPostponedEntities;
		CPhysical* aPostponedEntities2[MAX_POSTPONED_ENTITIES];
		int nNumPostponedEntities2;
		CPhysical** aSecondPassEntities;	// Pointer to array with GetMaxSecondPassEntities() elements.
		int nNumSecondPassEntities;
		CPhysical** aThirdPassEntities;	// Pointer to array with GetMaxThirdPassEntities() elements.
		int nNumThirdPassEntities;


		float fTimeStep;
		int nSlice;
	};

	// When deleting constraints, a static size array is used to hold the results. This defines the
	// number of entries which that array can hold. If there are more attachments on an entity, then they
	// probably won't get cleaned up properly.
	static const u32 MAX_NUM_CONSTRAINTS_TO_SEARCH_FOR = 16;

	static bool ms_bIgnoreForcedSingleStepThisFrame;

#if __BANK
	static bool ms_bDoVehicleDamage;
	static bool ms_bDoCollisionEffects;
	static bool ms_bDoWeaponImpactEffects;
#endif
#if __DEV && !__SPU
	static CDebugDrawStore ms_debugDrawStore;
#endif
#if __DEV
	static bool ms_bDoPedProbesWithImpacts;
#endif	// __DEV

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void UpdateRequests();	// Normally called through Update()
	static void PreSimUpdate(float fTimeStep, int nSlice);
	static void SimUpdate(float fTimeStep, int nSlice);
	static void VerletSimPreUpdate(float fTimeStep);
	static void VerletSimUpdate(int verletType);
	static void VerletSimPostUpdate();
	static void RopeSimUpdate(float fTimeStep, int updateOrderType);
	static void WaitForVerletTasks();	
	static void PostSimUpdate(int nLoop, float fTimeStep);
	static void IterateOverManifolds();
	static void PostProcessAfterAllUpdates(void);
	static void CommitDeferredOctreeUpdates();
	static void ProcessDeferredCompositeBvhUpdates();

	static void UpdateBreakableGlass();
	static bool BreakGlass(CDynamicEntity *entity, Vec3V_In position, float radius, Vec3V_In impulse, float damage, int crackType, bool forceStoreHit = false);
	
	static bool KeepBroadphasePair(int levelIndexA, int levelIndexB);

	static void ClearManifoldsForInst(phInst* pInst);
	static void WarpInstance(phInst* pInst, Vec3V_In pos);

	static void RenderDebug();
	static void UpdateStreaming();
	static bool SafeToIncreaseStreamingRangeForMoverCollision();
	static void LoadAboutPosition(Vec3V_In posn);

#if GPU_DAMAGE_WRITE_ENABLED
	static void ProcessDamagedVehicles();
	static void RemoveVehicleDeformation(CVehicle* pVehicle);
	static void QueueDamagedVehicleByGPU(CVehicle *pVehicle, bool &bUpdatedByGPU);
#endif

#if __BANK
	static void UpdateEntityFromFrag(void* userData);
	static void MarkEntityDirty(void* userData);
#if WORLD_PROBE_DEBUG
	static void SetAllWorldProbeContextDrawEnabled();
	static void SetAllWorldProbeContextDrawDisabled();
#endif // WORLD_PROBE_DEBUG
	static bool ms_bForceDummyObjects;
	static bool ms_bDrawMoverMapCollision;
	static bool ms_bDrawWeaponMapCollision;
	static bool ms_bDrawHorseMapCollision;
	static bool ms_bDrawCoverMapCollision;
	static bool ms_bDrawVehicleMapCollision;
	static bool ms_bDrawStairsSlopeMapCollision;
	static bool ms_bDrawDynamicCollision;
	static bool ms_bDrawPedDensity;
	static bool ms_bDrawPavement;
	static bool ms_bDrawPolygonDensity;
	static bool ms_bDrawEdgeAngles;
	static bool ms_bDrawPrimitiveDensity;
	static bool ms_bIncludePolygons;
	static void InitWidgets();
	static bkBank *ms_pPhysicsBank;
	static bkButton *ms_pCreatePhysicsBankButton;
	static bkBank *ms_pFragTuneBank;
	static bkButton *ms_pCreateFragTuneBankButton;
	static void CreatePhysicsBank();
	static void CreateFragTuneBank();
	static bool ms_bFragManagerUpdateEachSimUpdate;
	static bool ms_bDebugPullThingsAround;
	static float ms_pullForceMultiply;
	static float ms_pullDistMax;
	static void UpdatePullingThingsAround();
	static phMouseInput* ms_mouseInput;
	static bool ms_bMouseShooter;
	static bool ms_bAdvancedMouseShooter;
	static bool ms_bAttachedShooterMode;
	static bool ms_bOverrideMouseShooterWeapon;
	static s32 ms_iWeaponComboIndex;
	static void UpdateMouseShooter();
	static bool ms_bDebugMeasuringTool;
	static s32 ms_iNumInstances;
	static float ms_fWidth;
	static float ms_fCapsuleWidth;
	static float ms_fCapsuleWidth2;
	static float ms_fBoundingBoxWidth;
	static float ms_fBoundingBoxLength;
	static float ms_fBoundingBoxHeight;
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	static float ms_fSecondSurfaceInterp;
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	static bool ms_bLOSIgnoreSeeThru;
	static bool ms_bLOSIgnoreShootThru;
	static bool ms_bLOSIgnoreShootThruFX;
	static bool ms_bLOSGoThroughGlass;
	static bool ms_bDoLineProbeTest;
	static bool ms_bDoAsyncLOSTest;
	static bool ms_bDoAsyncSweptSphereTest;
	static bool ms_bDoLineProbeWaterTest;
	static bool ms_bDoCapsuleTest;
	static bool ms_bDoCapsuleBatchShapeTest;
	static bool ms_bDoBoundingBoxTest;
	static bool ms_bDoInitialSphereTestForCapsuleTest;
	static bool ms_bDoTaperedCapsuleBatchShapeTest;	
	static bool ms_bDoProbeBatchShapeTest;
	static bool ms_bDoBooleanTest;
	static bool ms_bTestCapsuleAgainstFocusEntity;
	static bool ms_bTestCapsuleAgainstFocusEntityBBoxes;
	static bool ms_bDoBringVehicleToHaltTest;
	static void UpdateDebugPositionOne();
	static void UpdateDebugPositionTwo();
	static char ms_Pos1[256];
	static char ms_Ptr1[20];
	static char ms_Pos2[256];
	static char ms_Ptr2[20];
	static char ms_Diff[256];
	static char ms_HeadingDiffRadians[256];
	static char ms_HeadingDiffDegrees[256];
	static char ms_Distance[256];
	static char ms_HorizDistance[256];
	static char ms_VerticalDistance[256];
	static char ms_Normal1[256];
	static char ms_Normal2[256];
	static Vector3 g_vClickedPos[2];
	static void *g_pPointers[2];
	static Vector3 g_vClickedNormal[2];
	static bool g_bHasPosition[2];
	static char ms_PlayerEntityTestResult[256];
	static char ms_zExcludedEntityList[352]; // Enough space to store 32 exclusion entity pointers in hex format with a space between each and '\0'.
	static bool ms_bDoPlayerEntityTest;
	static bool ms_bRenderAllHistories;
	static bool ms_bRenderFocusHistory;
	static bool ms_bDebugRagdollCount;
	static bool ms_bRenderFocusImpulses;
	static int ms_iNumIsecsTaperedCapsule;
	static bool ms_bPrintBreakingImpulsesForPeds;
	static bool ms_bPrintBreakingImpulsesForVehicles;
	static bool ms_bPrintBreakingImpulsesApplied;
	static bool ms_bDoSphereTest;
	static bool ms_bDoSphereTestNotFixed;
	static bool ms_bShowFoliageImpactsPeds;
	static bool ms_bShowFoliageImpactsVehicles;
	static bool ms_bShowFoliageImpactsRagdolls;

	static bool ms_bDisplaySelectedPhysicalCollisionRecords;
	static bool ms_bDisplayAllCollisionRecords;
	static float ms_fSphereRadius;

	static bool ms_bDisplayNumberOfBrokenPartsFocusFrag;
	
	static bool ms_bDisplayPedHealth;
	static bool ms_bDisplayPedFallHeight;

	static void updatePlayerEntityTest();
	static void DoBringVehicleToHaltTest(const Vector3& vLocatePos);
	static void UpdateHistoryRendering();
	static void UpdateDebugMeasuringTool();
	static void DoMeasuringToolBatchTests();
	static void AddFocusEntityToExclusionList();
	static void ClearEntityExclusionList();
#if USE_TAPERED_CAPSULE
	static void DoMeasuringToolTaperedCapsuleTests();
#endif
	static bool	GetMeasuringToolPos(s32 iPos, Vector3& vOutPos);
	static void DisplayRagdollCount();	
	static void DisplayCollisionRecords();
	static void DisplayNumPartsBrokenOffFocusFrag();
	static void DisplayRagdollDamageInfo();

	static void SwitchPlayerToRagDollCB();
	static char ms_stackModelName[STORE_NAME_LENGTH];
	static void DoObjectBoundTestCB();
	static void CreateStackCB();

	static void AttachFlagToVehicleCB();
	static void AttachCapeToCharCB();
	static void ToggleNoPopups();
	static void TogglePauseUnpauseGame();

	//Widget callback to set gravity.
	static void SetGravitationalAccelerationCB()
	{
		SetGravitationalAcceleration(ms_fGravitationalAccleration);
	}
	
	static int ms_iSelectedGravLevel;
	static void SetGravityLevelCB();

#if __DEV
	static void ReloadFocusEntityWaterSamplesCB();
	static void RenderFocusEntityExternalForces();
#endif // DEV

	// Glass 
	static bool ms_OverrideCrackSelection;
	static s32 ms_OverrideSelectedCrack;
	static bool ms_MouseBreakEnable;
	static float ms_MouseBreakRadius;
	static float ms_MouseBreakDamage;
	static float ms_MouseBreakImpulse;
	static int ms_MouseBreakCrackType;
	static bool ms_MouseBreakHiddenHit;
	static Vector3 ms_MouseBreakPosition;
	static Vector3 ms_MouseBreakVelocity;

#endif // __BANK

	static int GetGlassTxdSlot() { return ms_GlassTxd; }
	static int GlassSelectCrack();
	static void SetSelectedCrack(s32 crackId) { ms_SelectedCrack = crackId; }

	static phSimulator* GetSimulator () { return ms_PhysSim; }
	static phLevelNew* GetLevel () { return ms_PhysLevel; }
	static fragManager* GetFragManager() { return ms_FragManager; }
	static phConstraintMgr* GetConstraintManager() { return ms_ConstraintManager; }

	////////////////////////////
	// GTA helper functions

	// get an entity from a general phInst pointer (returns NULL for a phInst, can get an entity for a phInstGta)
	static CEntity* GetEntityFromInst(phInst* pInst);
	static const CEntity* GetEntityFromInst(const phInst* pInst);

	// global switch for ragdolls / natural motion
	static bool CanUseRagdolls();
	static u32 GetTimeSliceId() { return ms_TimeSliceId; }
	static int GetCurrentTimeSlice() { return ms_CurrentTimeSlice; }
	static int GetNumTimeSlices() {return ms_NumTimeSlices;}
	static void SetNumTimeSlices(int nNumTimeSlices) {ms_NumTimeSlices = nNumTimeSlices;}
	static bool GetIsFirstTimeSlice(int nSlice) {return (nSlice==0);}
	static bool GetIsLastTimeSlice(int nSlice) {return (nSlice==ms_NumTimeSlices - 1);}

	// return whether physics sim is within section where it needs uninterrupted access to all available cpu cores/SPUs
	static inline bool GetIsSimUpdateActive() { return ms_bSimUpdateActive; }	

	static float GetRagdollActivationAnimVelClamp() {return ms_fRagdollActivationAnimVelClamp;}
	static float GetRagdollActivationAnimAngVelClamp() {return ms_fRagdollActivationAnimAngVelClamp;}

	static void DisableOnlyPushOnLastUpdateForOneFrame() { ms_DisableOnlyPushOnLastUpdateForOneFrame = true; }

	// Sets whether all manifolds will be cleared in postsimupdate
	static void SetClearManifoldsEveryFrame(bool bClear);
	// Same as the above, but will auto-shut off after the given frames elapse
	static void SetClearManifoldsFramesLeft(int framesToClear);

	static void SetAllowedPenetration( float fAllowedPenetration );

	// Sets the gravitational acceleration via an enum
	static void SetGravityLevel(eGravityLevel nLevel);
	static eGravityLevel GetGravityLevel();
	static void SetGravitationalAcceleration(const float fGravitationalAcceleration);
	static float GetGravitationalAcceleration() { return ms_fGravitationalAccleration; }


	static float GetFoliageRadius(phInst* pOtherInstance, int component, int element);
	// Utility function for colliding with breakable glass
	static void CollideInstAgainstGlass(phInst* pInst);

	static inline ropeManager* GetRopeManager() { return ms_RopeManager; }
	static inline clothManager* GetClothManager() { return ms_ClothManager; }
	static inline clothInstanceTaskManager* GetClothInstanceTasksManager() { return ms_InstanceTasksManager; }
#if __BANK
	static inline ropeBankManager* GetRopeBankManager() { return ms_RopeBankManager; }
#endif
	
#if PHYSICS_LOD_GROUP_SHARING
	class ManagedPhysicsLODGroup
	{
	public:
		void Init(fragPhysicsLODGroup* pPhysicsLODGroup, int iPhysicsLODGroupFragStoreIdx, crFrameFilter* pInvalidRagdollFilter = NULL)
		{
			SetPhysicsLODGroup(pPhysicsLODGroup);
			m_PhysicsLODGroupFragStoreIdx = iPhysicsLODGroupFragStoreIdx;
			SetInvalidRagdollFilter(pInvalidRagdollFilter);
		}

		void Shutdown()
		{
			SetPhysicsLODGroup(NULL);
			m_PhysicsLODGroupFragStoreIdx = -1;
			physicsAssertf(m_InvalidRagdollFilter == NULL || m_InvalidRagdollFilter->GetRef() == 1, "CPhysics::ManagedPhysicsLODGroup::Shutdown: Invalid refcount %d for managed ragdoll filter", m_InvalidRagdollFilter->GetRef());
			SetInvalidRagdollFilter(NULL);
		}

		void SetPhysicsLODGroup(fragPhysicsLODGroup* pPhysicsLODGroup)
		{
			m_PhysicsLODGroup = pPhysicsLODGroup;
		}

		fragPhysicsLODGroup* GetPhysicsLODGroup() { return m_PhysicsLODGroup; }
		int GetPhysicsLODGroupFragStoreIdx() const { return m_PhysicsLODGroupFragStoreIdx; }

		void SetInvalidRagdollFilter(crFrameFilter* pInvalidRagdollFilter)
		{
			if (m_InvalidRagdollFilter)
			{
				m_InvalidRagdollFilter->Release();
			}

			m_InvalidRagdollFilter = pInvalidRagdollFilter;

			if (m_InvalidRagdollFilter)
			{
				m_InvalidRagdollFilter->AddRef();
			}
		}

		crFrameFilter* GetInvalidRagdollFilter() { return m_InvalidRagdollFilter; }

	private:
		fragPhysicsLODGroup* m_PhysicsLODGroup;
		int m_PhysicsLODGroupFragStoreIdx;
		crFrameFilter* m_InvalidRagdollFilter;
	};

	static ManagedPhysicsLODGroup& GetManagedPhysicsLODGroup(int iAssetID);
#endif
	static crFrameFilter* CreateInvalidRagdollFilter(const fragType& type);

private:

    static void InitCore();
	static void ShutdownCore();

	static void ScanAreaPtrArray( void appendFunc(atArray<CEntity*>&) );

	static void PrintPhysicsInstList(bool bIncludeList);
	static void DisplayVectorMapPhysicsInsts();

	static void IterateManifold(phManifold* manifold, CEntity* pEntityA, CEntity* pEntityB, phInst* pInstA, phInst* pInstB);
	static void ProcessCollisionData(phContact& contact, phManifold& manifold, CEntity* pEntityA, CEntity* pEntityB, const ScalarV& fAvgImpulseMag);

	static void ProcessCollisionVfxandSfx(VfxCollisionInfo_s& vfxColnInfo, bool shouldProcess = true);
	static bool ShouldProcessCollisionVfx(VfxCollisionInfo_s& vfxColnInfo);
	static bool ShouldProcessCollisionVfxAndSfx(VfxCollisionInfo_s& vfxColnInfo);
	static bool ShouldProcessWheelCollisionVfx(const CEntity* pEntity, Vec3V_In vCollNormal);
	static bool IsWheelComponent(const CEntity* pEntity, s32 componentId);		// move to vehicle class?
	static bool IsAttachedTo(const CEntity* pEntityA, const CEntity* pEntityB);		// move to physical class?

	static void InitSharedRagdollFragTypeData();
	static void ShutdownSharedRagdollFragTypeData();

	//Gravitational acceleration vector is (0,0,-ms_fGravitationalAccleration)
	//Default value for ms_fGravitationalAccleration is 9.81f.  
	//Set to new value using SetGravitationalAcceleration();
	static float ms_fGravitationalAccleration;
	//Update rage gravitational acceleration vector 
	//(automatically called from SetGravitationalAcceleration())
	static void SetRageGravityVector();
	//Update tolerances used to send rage physics objects to sleep
	//(automatically called form SetGravitationalAcceleration()).
	static void SetRageSleepTolerances();

	static atArray<RegdEnt> ms_buildingsWithPhysics;
	static phLevelNew* ms_PhysLevel;
	static phSimulator* ms_PhysSim;
	static fragManager* ms_FragManager;
	static phConstraintMgr* ms_ConstraintManager;
	static verletTaskManager* ms_TasksManager;
	static clothInstanceTaskManager* ms_InstanceTasksManager;
	static clothManager* ms_ClothManager;	
	static ropeManager* ms_RopeManager;	
#if __BANK && !__RESOURCECOMPILER
	static ropeBankManager* ms_RopeBankManager;	
#endif
	static fragInstGta* ms_pSuppliedFragCacheInsts;
	
	static grmShader* ms_GlassShader;
	static s32 ms_GlassTxd;
	static s32 ms_SelectedCrack;
	
	static grmShader* ms_RopeShader;

	static u32 ms_TimeSliceId;
	static int ms_CurrentTimeSlice;
	static int ms_NumTimeSlices;
	static bool ms_OverriderNumTimeSlices;
	static bool ms_OnlyPushOnLastUpdate;
	static bool ms_DisableOnlyPushOnLastUpdateForOneFrame;
	static bool ms_bSimUpdateActive;

	// The world attach entity, at the origin so we can attach peds in the world relative to this
	static CEntity* ms_pWorldAttachEntity;
	
	static bool ms_bClearManifoldsEveryFrame;
	static int ms_ClearManifoldsFramesLeft;

#if GPU_DAMAGE_WRITE_ENABLED
	static rage::sysIpcMutex sm_VehicleMutex;
ASSERT_ONLY(public:)
	static atArray<CVehicle*> ms_pDamagedVehicles;
#endif

#if PHYSICS_LOD_GROUP_SHARING
	static const int kNumManagedPhysicsLODGroups = 5;
	static ManagedPhysicsLODGroup m_ManagedPhysicsLODGroup[kNumManagedPhysicsLODGroups];
#endif

public:
	static bool AdditionalSetupModelInfoPhysics(phArchetype& archetype);

	static float ms_fRagdollActivationAnimVelClamp;
	static float ms_fRagdollActivationAnimAngVelClamp;

	static bool ms_bInStuntMode;
	static bool ms_bInArenaMode;

private:
	static bool GetShouldNetworkBlendBeforePhysics( CPhysical* pEntity );
#if PHYSICS_REQUEST_LIST_ENABLED
	//////////////////////////////////////////////////////////////////////////
public:
	// required until the navmesh generator tool has been fixed up
	static void AddToPhysicsRequestList(CEntity* pEntity);
	static void UpdatePhysicsRequestList();
	static CLinkList<RegdEnt> ms_physicsReqList;
#endif	//PHYSICS_REQUEST_LIST_ENABLED
	//////////////////////////////////////////////////////////////////////////
};

//
//Return an entity from any sub-class of phInst
//
__forceinline CEntity* CPhysics::GetEntityFromInst(phInst* pInst)
{
	if(pInst && pInst->GetInstFlag(phfwInst::FLAG_USERDATA_ENTITY))
		return (CEntity *)pInst->GetUserData();

	return NULL;
};

__forceinline const CEntity* CPhysics::GetEntityFromInst(const phInst* pInst)
{
	if(pInst && pInst->GetInstFlag(phfwInst::FLAG_USERDATA_ENTITY))
		return (const CEntity *)pInst->GetUserData();

	return NULL;
}



#endif
