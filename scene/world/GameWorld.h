/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/world/GameWorld.h
// PURPOSE : Store data about the world, or specific things in the world. But not
//			any form of representation of the world! (ie. stores all the other crap)
// AUTHOR :  John W.
// CREATED : 1/10/08
//
/////////////////////////////////////////////////////////////////////////////////


#ifndef INC_GAMEWORLD_H_
#define INC_GAMEWORLD_H_


// Rage headers
#include "physics/leveldefs.h"

// framework headers
#include "entity/sceneupdate.h"
#include "fwnet/nettypes.h"
#include "fwscene/world/worldMgr.h"
#include "fwscene/search/Search.h"
#include "fwscene/world/InteriorLocation.h"

// Game headers
#include "Peds/pedDefines.h"
#include "peds/pedfactory.h"
#include "scene/EntityTypes.h"
#include "scene/RegdRefTypes.h"

class CEntity;
class CObject;
class CVehicle;
class CPedGroup;
class CPed;
class CPlayerInfo;
class CControlledByInfo;
class CWanted;
class CSector;
class CRepeatSector;

// GTA_RAGE_PHYSICS
namespace rage
{
	namespace fwGeom { class fwLine; }
	class spdSphere;
	class spdAABB;
	class fwPtrList;
    class netPlayer;
	class phExplosionMgr;
	class phSimulator;
	class rlPresence;
	class rlPresenceEvent;
	class rlGamerInfo;
}

enum eTypeOfDistanceCheck
{
	DISTANCE_CHECK_NONE,
	DISTANCE_CHECK_CENTRE_RADIUS,
	DISTANCE_CHECK_ANGLED_AREA
};

struct SceneUpdateParameters;
//
// name:		CGameWorld
// description:	World class, contains array of sectors that entities are added
class CGameWorld
{
public:
	enum
	{
		SU_ADD_SHOCKING_EVENTS				= fwSceneUpdate::USER_SCENE_UPDATE,
		SU_START_ANIM_UPDATE_PRE_PHYSICS	= fwSceneUpdate::USER_SCENE_UPDATE << 1,	//  Mutually exclusive with SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2
		SU_END_ANIM_UPDATE_PRE_PHYSICS		= fwSceneUpdate::USER_SCENE_UPDATE << 2,
		SU_UPDATE_PAUSED					= fwSceneUpdate::USER_SCENE_UPDATE << 3,
		SU_START_ANIM_UPDATE_POST_CAMERA	= fwSceneUpdate::USER_SCENE_UPDATE << 4,
		SU_UPDATE							= fwSceneUpdate::USER_SCENE_UPDATE << 5,	// Mutually exclusive with SU_UPDATE_VEHICLE
		SU_AFTER_ALL_MOVEMENT				= fwSceneUpdate::USER_SCENE_UPDATE << 6,
		SU_RESET_VISIBILITY					= fwSceneUpdate::USER_SCENE_UPDATE << 7,
		SU_TRAIN							= fwSceneUpdate::USER_SCENE_UPDATE << 8,
		SU_UPDATE_PERCEPTION				= fwSceneUpdate::USER_SCENE_UPDATE << 9,
		SU_PRESIM_PHYSICS_UPDATE			= fwSceneUpdate::USER_SCENE_UPDATE << 10,
		SU_PHYSICS_PRE_UPDATE				= fwSceneUpdate::USER_SCENE_UPDATE << 11,
		SU_PHYSICS_POST_UPDATE				= fwSceneUpdate::USER_SCENE_UPDATE << 12,
		SU_UPDATE_VEHICLE					= fwSceneUpdate::USER_SCENE_UPDATE << 13,	// Mutually exclusive with SU_UPDATE
		SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2 = fwSceneUpdate::USER_SCENE_UPDATE << 14,// Mutually exclusive with SU_START_ANIM_UPDATE_PRE_PHYSICS
	};

	static void Init(unsigned initMode);
	static void Shutdown1(unsigned shutdownMode);
    static void Shutdown2(unsigned shutdownMode);
    static void Shutdown3(unsigned shutdownMode);

#if __PS3
	static CSector& GetSector(s32 x, s32 y);
	static CRepeatSector& GetRepeatSector(s32 x, s32 y);
	static fwPtrListSingleLink& GetLodPtrList(s32 x, s32 y);
#endif //__PS3

#if !__FINAL
	static void PrintProcessControlList();
	static void ValidateShutdown();
#endif

#if !__NO_OUTPUT
	static void LogClearCarFromArea(const char *pFunctionName, const CVehicle *pVehicle, const char *pReasonString);
#endif	//	!__NO_OUTPUT

	static void Add(CEntity* pEntity, fwInteriorLocation interiorLocation, bool bIgnorePhysicsWorld=false);
	static void Remove(CEntity* pEntity, bool bIgnorePhysicsWorld=false, bool bSkipPostRemove=false);
	static void RemoveAndAdd(CEntity* pEntity, fwInteriorLocation interiorLocation, bool bIgnorePhysicsWorld=false);
    static bool HasEntityBeenAdded(CEntity* pEntity);
	static bool IsEntityBeingAddedAndRemoved(const CEntity* pEntity) { return (pEntity && (pEntity == ms_pEntityBeingRemovedAndAdded)); }
	static bool IsEntityBeingRemoved(const CEntity* pEntity) { return (pEntity && (pEntity == ms_pEntityBeingRemoved)); }

	static void PostAdd(CEntity* pEntity, bool bIgnorePhysicsWorld=false);
	static void PostRemove(CEntity* pEntity, bool bIgnorePhysicsWorld=false, bool bTestScenarioPoints=true);

	static void AddScenarioPointsForEntity(CEntity& entity, bool useOverrides);

	static void UpdateEntityDesc(fwEntity* entity, fwEntityDesc* entityDesc);

	static void DebugTestsForAdd(CEntity* pEntity);

	static void AddWidgets(bkBank* pBank);

	static void SetPedBeingDeleted(const CPed* pPed) { ms_pPedBeingDeleted = pPed; }
	static const CPed* GetPedBeingDeleted() { return ms_pPedBeingDeleted; }

	// call this to enable/disable CGameWorld::Remove().
	static void AllowDelete(bool bAllow) { ms_bAllowDelete = bAllow; }
	static bool IsDeleteAllowed() { return ms_bAllowDelete; }
	static bool ms_bAllowDelete;

#if __BANK
	static bool ms_detailedOutOrMapSpew;
#endif

	static float GetLightingAtPoint(const Vector3& vecStart, float zEnd);
	static bool IsWanderPathClear(const Vector3& vStart, const Vector3& vEnd, const float fMaxHeightChange, const int iMaxNumSampes=0);
	static void ResetLineTestOptions();

	static void Process();
	static void ProcessPaused();
	static void ProcessGlobalEvents();
	static void ProcessVehicleEvents();

	static bool IsEntityFrozen(CEntity* pEntity);
	static bool CheckEntityFrozenByInterior(CEntity* pEntity);
	static void OverrideFreezeFlags(bool bOverride);
	static bool GetFreezeExteriorPeds() { return ms_freezeExteriorPeds; }
	static bool GetFreezeExteriorVehicles() { return ms_freezeExteriorVehicles; }

	static bool ShouldUpdateThroughAnimQueue(const CEntity& entity);

	// For Type/Control/Lod flags see (ENTITY_TYPE_MASK/SEARCH_LOCATION_FLAGS/SEARCH_LODTYPE) in "Scene/World/Search/SearchTypes.h" 
	static void ForAllEntitiesIntersecting(fwSearchVolume* pColVol, IntersectingCB cb, void* data, s32 typeFlags, s32 locationFlags, s32 lodFlags = SEARCH_LODTYPE_HIGHDETAIL, s32 optionFlags = SEARCH_OPTION_NONE, u32 module = WORLDREP_SEARCHMODULE_DEFAULT);
	static void ForAllEntitiesIntersecting(fwSearchVolume* pColVol, fwIntersectingCB cb, void* data, s32 typeFlags, s32 locationFlags, s32 lodFlags = SEARCH_LODTYPE_HIGHDETAIL, s32 optionFlags = SEARCH_OPTION_NONE, u32 module = WORLDREP_SEARCHMODULE_DEFAULT);

	// some search helper funcs
	//
	// use these as follows 
	// CFindData findData;
	// CGameWorld::ForAllEntitiesIntersecting(sphere, &CGameWorld::FindEntitiesCB, &findData);

	static bool FindEntitiesCB(fwEntity* pEntity, void* data);
	static bool FindEntitiesOfTypeCB(fwEntity* pEntity, void* data);
	static bool FindMissionEntitiesCB(fwEntity* pEntity, void* data);

	//
	// AF: old style findobjects functions. I want to remove most of these functions to use the new style
	//		ForAllEntities.. functions
	//
	static void FindObjectsInRange(const Vector3 &Coors, float Range, bool Dist2D, s32 *pNum, s32 MaxNum, CEntity **ppResults, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies);
	static void FindObjectsKindaColliding(const Vector3 &Coors, float Range, bool useBoundBoxes, bool Dist2D, s32 *pNum, s32 MaxNum, CEntity **ppResults, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies);
	static void FindObjectsIntersectingCube(Vector3::Param CoorsMin, Vector3::Param CoorsMax, s32 *pNum, s32 MaxNum, CEntity **ppResults, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies);

	static void RemoveEntity(CEntity *pEntityToRemove);

	static void RemoveOutOfMapPed(CPed* pPed, bool bOutOfInterior);
	static void RemoveOutOfMapVehicle(CVehicle* pVehicle);
	static void RemoveOutOfMapObject(CObject* pObject);
	static void RemoveFallenPeds();
	static void RemoveFallenCars();
	static void RemoveFallenObjects();

	static void ClearProjectilesFromArea(const Vector3 &Centre, float Radius);
	static void ClearExcitingStuffFromArea(const Vector3 &Centre, float Radius, bool bRemoveProjectiles, bool bConvertObjectsWithBrainsToDummyObjects, bool bCheckInterestingVehicles, bool bLeaveCarGenCars, CObject *pExceptionPbject, bool bClearLowPriorityPickupsOnly, bool bClearPedsAnVehs, bool bClampVehicleHealth = true );
	static bool DeleteThisVehicleIfPossible(CVehicle *pVehicle, bool bOnlyDeleteCopCars, bool bTakeInterestingVehiclesIntoAccount, bool bAllowCopRemovalWithWantedLevel, eTypeOfDistanceCheck DistanceCheckType, Vector3 &vecPoint1, float fDistance, Vector3 &vecPoint2);
	static void ClearCarsFromArea(const Vector3 &Centre, float Radius, bool bCheckInterestingVehicles, bool bLeaveCarGenCars, bool bCheckViewFrustum=false, bool bIfWrecked=false, bool bIfAbandoned=false, bool bAllowCopRemovalWithWantedLevel=true, bool bIfEngineOnFire=false, bool bKeepScriptTrains=false);
	static void ClearCarsFromAngledArea(const Vector3 &vecPoint1, const Vector3 &vecPoint2, float DistanceToOppositeFace, bool bCheckInterestingVehicles, bool bLeaveCarGenCars, bool bCheckViewFrustum, bool bIfWrecked, bool bIfAbandoned, bool bAllowCopRemovalWithWantedLevel=true, bool bIfEngineOnFire=false, bool bKeepScriptTrains=false);
	static void ClearPedsFromArea(const Vector3 &Centre, float Radius);
	static void ClearPedsFromAngledArea(const Vector3 &vecPoint1, const Vector3 &vecPoint2, float DistanceToOppositeFace);
	static void ClearCopsFromArea(const Vector3 &Centre, float Radius);
	static void ClearObjectsFromArea(const Vector3 &Centre, float Radius, bool bConvertObjectsWithBrainsToDummyObjects = false, CObject *pExceptionObject = NULL, bool bForce=false, bool bIncludeDoors=false, bool bExcludeLadders=false);

	static void SetAllCarsCanBeDamaged(bool DmgeFlag);

	static void ExtinguishAllCarFiresInArea(const Vector3& Centre, float Radius, bool finishPtFx=false, bool bClampVehicleHealth = true);
	static void ClearCulpritFromAllCarFires(const CEntity* pCulprit);

	static void StopAllLawEnforcersInTheirTracks();

	static void ProcessAfterAllMovement();
	static void ProcessPedsEarlyAfterCameraUpdate();
	static void ProcessAfterCameraUpdate();
	static void ProcessAfterPreRender();

	static void ProcessDynamicObjectVisibility();

	// ---- Delayed Attachments
	// B*1868591 & 1958782: This will process all attachments for the given physical that were flagged as requiring a second, delayed pass due to the attachments being considered "complex".
	// Calling this will prevent the same attachment tree from being processed again in CGameWorld::ProcessAfterPreRender, which is where the processing normally happens.
	// Used by vehicles to process things attached to them before the normal processing in ProcessAfterPreRender because mod kit parts have their bone matrices zeroed in PreRender2.
	static void ProcessDelayedAttachmentsForPhysical(const CPhysical *const pPhysical);

	// ---- player stuff
	static int			CreatePlayer(Vector3& pos, u32 name, float health);
	static bool			AddPlayerToWorld(CPed* pPlayerPed, const Vector3& pos);
	static CPed*		ChangePedModel(CPed& playerPed, fwModelId newModelId);
    static bool         ChangePlayerPed(CPed& playerPed, CPed& newPlayerPed, bool bKeepScriptedTasks = false, bool bClearPedDamage = true);
	static bool			StoreCoverTaskInfoForCurrentPlayer(CPed& playerPed, bool& bFacingLeft);
	static bool			StoreCoverTaskInfoForAIPed(CPed& aiPed);
	static void			RestoreCoverTaskInfoForNewPlayer(CPed& newPlayerPed);
	static void			RestoreCoverTaskInfoForNewAIPed(CPed& newAiPed, bool bFaceLeft);

	static CPlayerInfo* GetMainPlayerInfo();
	static CPlayerInfo* GetMainPlayerInfoSafe();
	static CPed*		FindLocalPlayer();
	static CPed*		FindLocalPlayerSafe();
	static CPedGroup*	FindLocalPlayerGroup();
	static CVehicle*	FindLocalPlayerVehicle(bool bReturnRemoteVehicle = false);
	static CPed*		FindLocalPlayerMount();
	static CVehicle*	FindLocalPlayerTrain();
	static float		FindLocalPlayerHeading();
	static float		FindLocalPlayerHeight();
	static CWanted*		FindLocalPlayerWanted();
	static CWanted*		FindLocalPlayerWanted_Safe();
	static eArrestState	FindLocalPlayerArrestState();
	static eArrestState	FindLocalPlayerArrestState_Safe();
	static eDeathState	FindLocalPlayerDeathState();
	static eDeathState	FindLocalPlayerDeathState_Safe();
	static float		FindLocalPlayerHealth();
	static float		FindLocalPlayerHealth_Safe();
	static float		FindLocalPlayerMaxHealth();
	static float		FindLocalPlayerMaxHealth_Safe();
	static Vector3		FindLocalPlayerCoors();
	static Vector3		FindLocalPlayerCoors_Safe();
	static Vector3		FindLocalPlayerSpeed();
	static Vector3		FindLocalPlayerCentreOfWorld();
	static CPed*		FindPlayer(PhysicalPlayerIndex playerIndex);

	static void			SetPlayerFallbackPos(const Vector3 &pos);
	static void			ClearPlayerFallbackPos();
	static const Vector3& GetPlayerFallbackPos() {	Assert(ms_playerFallbackPosSet); return ms_playerFallbackPos;	}

	//PURPOSE
	//   Access the spectator player CPed in case the player is in spectator mode and 
	//    revert to the local player access methods in case it is not in spectator mode.
	static CPed*		 FindFollowPlayer();
	static CVehicle*	 FindFollowPlayerVehicle();
	static float         FindFollowPlayerHeading();
	static float         FindFollowPlayerHeight();
	static Vector3		 FindFollowPlayerCoors();
	static Vector3		 FindFollowPlayerSpeed();
	static Vector3		 FindFollowPlayerCentreOfWorld();

	static const char*	GetFollowPlayerCarName() { return sm_FollowPlayerCarName; }

	// -----------------


public:
	static fwInteriorLocation OUTSIDE;

	// cached data to be accessed from the outside
	static s32			sm_localPlayerWanted;
	static eArrestState	sm_localPlayerArrestState;
	static eDeathState	sm_localPlayerDeathState;
	static float		sm_localPlayerHealth;
	static float		sm_localPlayerMaxHealth;

private:
	static void SceneUpdateAddShockingEventsCb(fwEntity &entity, void *userData);
	static void SceneUpdateStartAnimUpdatePrePhysicsCb(fwEntity &entity, void *userData);
	static void SceneUpdateEndAnimUpdatePrePhysicsCb(fwEntity &entity, void *userData);
	static void SceneUpdateStartAnimUpdatePostCameraCb(fwEntity &entity, void *userData);
	static void SceneUpdateUpdatePausedCb(fwEntity &entity, void *userData);
	static void SceneUpdateUpdateCb(fwEntity &entity, void *userData);
	static void SceneUpdateAfterAllMovementCb(fwEntity &entity, void *userData);
	static void SceneUpdateResetVisibilityCb(fwEntity &entity, void *userData);
	static void SceneUpdateUpdatePerceptionCb(fwEntity &entity, void *userData);
	static void SceneUpdatePreSimPhysicsUpdateCb(fwEntity &entity, void *userData);
	static void SceneUpdatePhysicsPreUpdateCb(fwEntity &entity, void *userData);
	static void SceneUpdatePhysicsPostUpdateCb(fwEntity &entity, void *userData);

	static void StartAnimationUpdate();
	static void EndAnimationUpdate();

	static void UpdateEntitiesThroughAnimQueue(SceneUpdateParameters& params);

	static void StartPostCameraAnimationUpdate();
	static void EndPostCameraAnimationUpdate();
	static void SetFreezeFlags();
	static bool WaitingForCollision(CEntity* pEntity);
	static void UpdateEntityFrozenByInterior(CEntity* pEntity);

	//Callback for presence events (signin, signout, friends events, etc.).
	static void OnPresenceEvent(rlPresence* presenceMgr,
		const rlPresenceEvent* evt);

	static void ProcessAttachmentAfterAllMovement(CPhysical* pPhysical);

	static void ClearFrozenEntitiesLists();
	static void FlushFrozenEntitiesLists();

	static void UpdateLensFlareAfterPlayerSwitch(const CPed* newPlayerPed);

	// List of physicals at the top of attachment trees that must be processed with a skeleton update
	static RegdPhy ms_delayedRootAttachments[MAX_NUM_POST_PRE_RENDER_ATTACHMENTS];
	static u16 ms_nNumDelayedRootAttachments;

	// PURPOSE:	How many more frames until we have to refresh the visibility timestamps in CDynamicEntity.
	static u8 ms_DynEntVisCountdownToRejuvenate;

	static bool ms_freezeExteriorVehicles;
	static bool ms_freezeExteriorPeds;
	static bool ms_overrideFreezeFlags;
#if !__FINAL
	static bool ms_busyWaitIfAnimQueueExhausted;
#endif	// !__FINAL
	static bool ms_updateObjectsThroughAnimQueue;
	static bool ms_updatePedsThroughAnimQueue;
	static bool ms_updateVehiclesThroughAnimQueue;
	static bank_bool ms_makeFrozenEntitiesInvisible;
	static atArray< CPhysical* > ms_frozenEntities;
	static atArray< CPhysical* > ms_unfrozenEntities;
	static atArray< CPed* > ms_pedsRunningInstantAnimUpdate;
	
	static bool ms_playerFallbackPosSet;
	static Vector3 ms_playerFallbackPos;
	
	static const CEntity* ms_pEntityBeingRemovedAndAdded;
	static const CEntity* ms_pEntityBeingRemoved;
	static const CPed* ms_pPedBeingDeleted;

	static u32 m_LastLensFlareModelHash;

	static const char* sm_FollowPlayerCarName;

#if !__FINAL
	static CEntity* ms_pProcessingEntity;
#endif

#if !__NO_OUTPUT
	static bool ms_bLogClearCarFromArea;
	static bool ms_bLogClearCarFromArea_OutOfRange;
#endif	//	!__NO_OUTPUT
};

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns a pointer to the local player
//////////////////////////////////////////////////////////////////////
inline CPed*	CGameWorld::FindLocalPlayer()
{
	// Local Player held in ped factory now.
	return CPedFactory::GetFactory()->GetLocalPlayer();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns a pointer to the local player
//////////////////////////////////////////////////////////////////////
inline CPed*	CGameWorld::FindLocalPlayerSafe()
{
	// Local Player held in ped factory now.
	if(CPedFactory::GetFactory())
		return CPedFactory::GetFactory()->GetLocalPlayer();
	return NULL;
}


#endif //INC_GAMEWORLD_H_

