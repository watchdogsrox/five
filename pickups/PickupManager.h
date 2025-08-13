#ifndef PICKUP_MANAGER_H
#define PICKUP_MANAGER_H

// Rage headers
#include "atl/array.h"
#include "atl/slist.h"

// Framework headers
#include "fwutil/PtrList.h"

// Game headers
#include "Scene/RegdRefTypes.h"
#include "pickups/Data/PickupData.h"
#include "pickups/Data/PickupIds.h"
#include "pickups/Pickup.h"
#include "pickups/PickupPlacement.h"

// rage headers
#include "atl/map.h"

// Forward declarations
namespace rage { 
	class bkBank;
	class fwQuadTreeNode;
	class Matrix34; 
	class spdAABB;
}

class GtaThread;
class CNetObjPickupPlacement;

//////////////////////////////////////////////////////////////////////////
// ScriptedGlow
//////////////////////////////////////////////////////////////////////////

struct ScriptedGlow 
{
    Color32 color;
    float	intensity;
    float	range;

#if __BANK
	void AddWidgets(bkBank *bank)
	{
		bank->AddColor("color",&color);
		bank->AddSlider("intensity",&intensity,0.0f,10.0f,0.1f);
		bank->AddSlider("range",&range,0.0f,250.0f,0.1f);
	}
#endif // __BANK	

	PAR_SIMPLE_PARSABLE;
};

struct ScriptedGlowList 
{
    atArray<ScriptedGlow> m_data;

	PAR_SIMPLE_PARSABLE;
};


//////////////////////////////////////////////////////////////////////////
// CPickupManager
//////////////////////////////////////////////////////////////////////////

class CPickupManager
{
public:

	// The distance from the player to his previous cached position at which the current placements in scope are recalculated
	static const s32 PLACEMENT_SCOPE_RANGE = 20;

	// The maximum distance a pickup placement can generate a pickup from the local player
	static const s32 MAX_PLACEMENT_GENERATION_RANGE = 300;

	// The maximum number of collections we keep a history for
	static const s32 COLLECTION_HISTORY_SIZE = 4;

	// The maximum number of extra custom archetypes for pickups with custom models
	static const s32 MAX_EXTRA_CUSTOM_ARCHETYPES = 20;

	// The maximum number of models that the scripts can prohibit the local player from collecting
	static const s32 MAX_PROHIBITED_PICKUP_MODELS = 50;

	static bool PickupHashIsMoneyType(int pickupHash)
	{
		if(PICKUP_MONEY_VARIABLE		== pickupHash
		|| PICKUP_MONEY_CASE			== pickupHash 
		|| PICKUP_MONEY_WALLET			== pickupHash
		|| PICKUP_MONEY_PURSE			== pickupHash
		|| PICKUP_MONEY_DEP_BAG			== pickupHash
		|| PICKUP_MONEY_MED_BAG			== pickupHash
		|| PICKUP_MONEY_PAPER_BAG		== pickupHash
		|| PICKUP_MONEY_SECURITY_CASE	== pickupHash
		|| PICKUP_GANG_ATTACK_MONEY     == pickupHash)
		{
			return true;
		}
		return false;
	}

protected:

	// class storing info on a pickup collection 
	class CCollectionInfo
	{
	public:

		CCollectionInfo() : m_ped(NULL) {}
		CCollectionInfo(const CGameScriptObjInfo& info) : m_scriptInfo(info), m_ped(NULL) {}
		CCollectionInfo(const CGameScriptObjInfo& info, CPed* pPed) : m_scriptInfo(info), m_ped(pPed) {}

		FW_REGISTER_CLASS_POOL(CCollectionInfo);

		bool operator==(const CCollectionInfo& info) const { return m_scriptInfo == info.m_scriptInfo; }

		CPed* GetPed() { return m_ped.Get(); }

	private:

		CCollectionInfo(const CCollectionInfo &);

		CGameScriptObjInfo	m_scriptInfo;	// the script info of pickup collected
		RegdPed	m_ped;						// the ped who collected it
	};

	// class storing info on a placement regeneration
	class CRegenerationInfo
	{
	public:

		CRegenerationInfo(CPickupPlacement* pPlacement);

		FW_REGISTER_CLASS_POOL(CRegenerationInfo);

		CPickupPlacement*	GetPlacement() const			{ return m_pPlacement; }
		u32				    GetRegenerationTime() const		{ return m_regenerationTime; }
		void				SetRegenerationTime(u32 t)	    { m_regenerationTime = t; }
		void				SetForceRegenerate()			{ m_forceRegenerate = true; }

		bool IsReadyToRegenerate();

	private:

		CRegenerationInfo(const CRegenerationInfo &);
		CRegenerationInfo &operator=(const CRegenerationInfo &);

		CPickupPlacement* m_pPlacement; // the placement that is regenerating a pickup
		u32			      m_regenerationTime;	//	the time the placement will regenerate a pickup
		bool			  m_forceRegenerate;
	};

	class CCustomArchetypeInfo
	{
	public:

		CCustomArchetypeInfo() : m_archetype(NULL), m_modelIndex(fwModelId::MI_INVALID), m_refCount(0) {};

		void Set(phArchetypeDamp& archetype, u32 modelIndex);
		void Clear();

		bool					IsSet() const			{ return m_archetype != NULL; }
		phArchetypeDamp*		GetArchetype()			{ return m_archetype; }
		strLocalIndex			GetModelIndex() const	{ return m_modelIndex; }

		void					AddRef()				{ Assert(m_archetype); m_refCount++; }
		void					RemoveRef();	

	private:
	
		phArchetypeDamp* m_archetype;
		strLocalIndex	 m_modelIndex;
		u32				 m_refCount;
	};

	// used for sorting pickups based on their lifetime
	struct PickupCompare
	{
		bool operator()(const CPickup* pickupA, const CPickup* pickupB) const
		{
			return pickupA->GetLifeTime() > pickupB->GetLifeTime();
		}
	};

	// extra information associated with each pickup type
	class CPickupTypeInfo
	{
	public:
		CPickupTypeInfo() : m_prohibitedCollections(0) {}

		CCustomArchetypeInfo m_customArchetype;
		PlayerFlags			 m_prohibitedCollections;
	};

	typedef atMap<atHashValue, CPickupTypeInfo> PickupTypeInfoMap;

public:

	class CPickupPositionInfo
	{
	public:
		CPickupPositionInfo( const Vector3 &srcPos );

		Vector3 m_SrcPos;
		Vector3 m_Pos;			// Return
		float	m_MinDist;
		float	m_MaxDist;
		float   m_pickupHeightOffGround;
		bool	m_bOnGround;	// Return
	};

	static void InitPools();
	static void ShutdownPools();

	static void Init(unsigned initMode);
	static void InitDLCCommands();
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void SessionReset();

	static void UpdatePickupTypes();

	static void SetForceRotatePickupFaceUp() { ms_forceRotatePickupFaceUp = true; }
	static bool GetForceRotatePickupFaceUp() { return ms_forceRotatePickupFaceUp; }

	// Pickup registration. Pickup placement data is added to the quad tree. Returns the new pickup placement
	static CPickupPlacement* RegisterPickupPlacement(u32 pickupHash, 
													 Vector3& pickupPosition,
													 Vector3& pickupOrientation,
													 PlacementFlags flags = 0, 
													 CNetObjPickupPlacement* pNetObj = NULL,
													 u32 customModelIndex = fwModelId::MI_INVALID);

	// finds the pickup placement at the given position
	static CPickupPlacement* FindPickupPlacement(const Vector3 &pickupPosition);

	// Removes the pickup placement data with the given script info
	static void RemovePickupPlacement(const CGameScriptObjInfo& info);

	// Removes the given pickup placement data 
	static void RemovePickupPlacement(CPickupPlacement *pPlacement);

	// Creates a new pickup object (if this is called externally no pickup placement is generated. Use this for temporary pickups (eg dropped from dying peds))
	static CPickup* CreatePickup(u32 pickupHash, const Matrix34& placementMatrix, netObject* pNetObj = NULL, bool bRegisterAsNetworkObject = true, u32 customModelIndex = fwModelId::MI_INVALID, bool bCreateDefaultWeaponAttachments = true, bool bCreateAsScriptEntity = false);

	// Creates a new pickup object from the peds current weapon
	static CPickup* CreatePickUpFromCurrentWeapon(CPed* pPed, bool bUsePedAmmo = false, bool bDropBestWeaponIfUnarmed = false);

	// Returns the placement data for the given pickup script info 
	static CPickupPlacement* GetPickupPlacementFromScriptInfo(const CGameScriptObjInfo& info);

	// Called when the pickup for the given placement is collected
	static void RegisterPickupCollection(CPickupPlacement *pPlacement, CPed* pPed);

	// Called when the pickup for the given placement is destroyed
	static void RegisterPickupDestruction(CPickupPlacement *pPlacement);

	// returns true if the pickup with the given script info has been collected
	static bool HasPickupBeenCollected(const CGameScriptObjInfo& info, CPed** ppPedWhoGotIt = NULL);

	// returns the time the given placement will regenerate its pickup object
	static u32 GetRegenerationTime(CPickupPlacement *pPlacement);

	// called when the ownership of a placement migrates in a network game
	static void MigratePlacementOwnership(CPickupPlacement *pPlacement, u32 regenerationTime);

	// Query if any pickups that intersect the bounds
	static bool GetIsAnyPickupInBounds(const spdAABB& bounds);

	// Remove all pickups
	static void RemoveAllPickups(bool bIncludeScriptPickups, bool bImmediately = false);

	// Removes all pickups & placements of the given type 
	static void RemoveAllPickupsOfType(u32 pickupHash, bool bIncludeScriptPickups = false);

	// Removes all pickups that lie within the given bounds
	static void RemoveAllPickupsInBounds(const spdAABB& bounds, bool bLowPriorityOnly = false, bool bIncludeScriptPickups = false);

	// Finds and detaches all portable pickups from the given ped
	static void DetachAllPortablePickupsFromPed(CPed* pPed);

	// Counts all pickups & placements of the given type 
	static s32 CountPickupsOfType(u32 pickupHash);

	// Tests to see if any pickups exist within the given sphere
	static bool TestForPickupsInSphere(const Vector3& spherePos, float sphereRadius);

	// Generates a pickup position as close to the given coords as possible
	static bool CalculateDroppedPickupPosition( CPickupPositionInfo &dat, bool bIgnoreInvalidePosWarning = false );

	// Generates a given number of money pickups up to the given amount, around the given coordinates
	static void CreateSomeMoney(const Vector3& targetCoords, u32 Amount, u32 NumPickups, u32 customModelIndex = fwModelId::MI_INVALID, CPed* pPedForVelocity = NULL);

	// Returns the stored custom archetype for this pickup 
	static phArchetypeDamp* GetCustomArchetype(const CPickup& pickup);

	// Stores a custom archetype for this pickup 
	static void StoreCustomArchetype(CPickup& pickup, phArchetypeDamp* pArchetype);

	// adds a reference to an extra custom archetype
	static void AddRefForExtraCustomArchetype(phArchetypeDamp* pArchetype);

	// removes a reference to an extra custom archetype
	static void RemoveRefForExtraCustomArchetype(u32 modelIndex);

	// Suppression flag accessors
	static bool		IsSuppressionFlagSet	( const s32 iFlag );
	static void		SetSuppressionFlag		( const s32 iFlag )			{ ms_SuppressionFlags.SetFlag(iFlag); }
	static void		ClearSuppressionFlag	( const s32 iFlag )			{ ms_SuppressionFlags.ClearFlag(iFlag); }
	static void		ClearSuppressionFlags	( void )					{ ms_SuppressionFlags.ClearAllFlags(); }

	// prevents a certain player from collecting a certain type of pickup 
	static void ProhibitCollectionForPlayer(u32 pickupHash, PhysicalPlayerIndex playerIndex);


	// allows collection of a certain type of pickup for the given player
	static void AllowCollectionForPlayer(u32 pickupHash, PhysicalPlayerIndex playerIndex);

	// returns true if the given player is prohibited from collecting a certain type of pickup
	static bool IsCollectionProhibited(u32 pickupHash, PhysicalPlayerIndex playerIndex);

	// removes all prohibited collection for the given pickup type
	static void RemoveProhibitedCollection(u32 pickupHash);

	// removes the given player from all prohibited collections for all pickup types
	static void AddPlayerToAllProhibitedCollections(PhysicalPlayerIndex playerIndex);

	// removes the given player from all prohibited collections for all pickup types
	static void RemovePlayerFromAllProhibitedCollections(PhysicalPlayerIndex playerIndex);

	// prevents the local player from collecting pickups with the given custom model  
	static void ProhibitPickupModelCollectionForLocalPlayer(u32 modelIndex, const char* scriptName);

	// returns true if the local player is prohibited from collecting pickups with the given custom model
	static bool IsPickupModelCollectionProhibited(u32 modelIndex);

	// removes all prohibited collection for the given pickup custom model
	static void RemoveProhibitedPickupModelCollection(u32 modelIndex, const char* scriptName);

	// render a glow
	static void RenderGlow(Vec3V_In pos, float fadeNear, float fadeFar, Vector3 &col, float intensity, float range );
	static void RenderScriptedGlow(Vec3V_In pos, int glowType);
	
	// moves any attached portable pickups from ped1 to ped2
	static void MovePortablePickupsFromOnePedToAnother(CPed& ped1, CPed& ped2);

	static void DetachAllPortablePickupsFromPed(CPed& ped);

	// returns true if the placement is on the regeneration list
	static bool IsPlacementOnRegenerationList(CPickupPlacement *pPlacement, CRegenerationInfo** pInfo = NULL);

	// removes the given placement from the collection history if it is on the collection list
	static void RemovePickupPlacementFromCollectionHistory(CPickupPlacement *pPlacement);

	// set the pickup's linear and angular velocities based on the ped's ragdoll state. 
	static bool SetStartingVelocities(CPickup* pPickup, CPed *pPed, RagdollComponent primaryBone, RagdollComponent secondaryBone, const Vector3& vExtraLinearVelocity = VEC3_ZERO); 

	// Check if the ped can drop a pickup from the last equipped weapon or the best weapon.
	static bool CanCreatePickupIfUnarmed(const CPed* pPed);

	// Check if use MP pickups. If pPed is null this will return true if any ped is using MP pickups
	static bool ShouldUseMPPickups(const CPed* pPed = NULL);

	static void SetPickupAmmoAmountScaler(float fScale) {ms_fAmmoAmountScaler = fScale;}
	static float GetPickupAmmoAmountScaler() {return ms_fAmmoAmountScaler;}

	// possibly dumps some old pickups to clear space in the pickup pool if it is getting too full. 
	static void ClearSpaceInPickupPool(bool network);
	//
	// Debug
	//

	// some modes only allow you to collect ammo pickups when low on ammo
	static void SetOnlyAllowAmmoCollectionWhenLowOnAmmo(bool b) { ms_OnlyAllowAmmoCollectionWhenLowOnAmmo = b; }
	static bool GetOnlyAllowAmmoCollectionWhenLowOnAmmo() { return ms_OnlyAllowAmmoCollectionWhenLowOnAmmo; }

	// Sets flag on CRegenerationInfo to force regenerate pickup even if time left on timer
	static void ForcePlacementFromRegenerationListToRegenerate(CPickupPlacement &pPlacement);

	static const bool IsNetworkedMoneyPickupAllowed() { return ms_AllowNetworkedMoneyPickup; }

#if __BANK
	// Widgets
	static void InitLevelWidgets();
	static void ShutdownLevelWidgets();

	static void PickupsBank_CreatePickup();
	static void PickupsBank_CreatePickupPlacement();
	static void PickupsBank_CreateAllPickups();
	static void PickupsBank_RemoveAllPickups();
	static void PickupsBank_DetachAllPickups();
	static void PickupsBank_ReloadPickupData();
	static void PickupsBank_RecalculatePickupsInScope();
	static void PickupsBank_SetPlayerCanCarryPortablePickups();
	static void PickupsBank_MakePickupFromPlayerWeapon();
	static void DebugScriptedGlow();

	static bool	ms_allowPlayerToCarryPortablePickups;

#if ENABLE_GLOWS	
	static bool GetForcePickupGlow() { return ms_forcePickupGlow; }
	static bool GetForceOffsetPickupGlow() { return ms_forceOffsetPickupGlow; }
#endif // ENABLE_GLOWS	
#endif // __BANK

private:

	// adds a placement to the regeneration list
	static CRegenerationInfo* AddPlacementToRegenerationList(CPickupPlacement *pPlacement);

	// removes a placement to the regeneration list
	static void RemovePlacementFromRegenerationList(CPickupPlacement *pPlacement, bool bAssert = true);

	// destroys a placement 
	static void DestroyPickupPlacement(CPickupPlacement *pPlacement);

	// called when the pickup manager is inactive and no pickups should exist
	static void ProcessInactiveState();

	// determines which pickups are in scope and adds them to the pending list
	static void ProcessPickupsInScope();

	// processes the pending list (placements waiting to generate pickups)
	static void ProcessPendingPlacements();

	// processes placements that are due to regenerate a pickup
	static void ProcessRegeneratingPlacements();

	// processes placements that are waiting to be removed
	static void ProcessRemovedPlacements();

	// updates all the current pickups
	static void UpdatePickups();

	// additional information held for each pickup type
	static PickupTypeInfoMap ms_pickupTypeInfoMap;

	// extra custom physics archetypes for pickups with custom models
	static atFixedArray<CCustomArchetypeInfo, MAX_EXTRA_CUSTOM_ARCHETYPES> ms_extraCustomArchetypes;

	// Queue holding recent history of collected pickups
	static atQueue<CCollectionInfo, COLLECTION_HISTORY_SIZE> ms_collectionHistory;

	// quad tree storing pickup placement data
	static fwQuadTreeNode* ms_pQuadTree;

	// linked list of current pickup placements waiting to generate pickups
	static fwPtrListSingleLink ms_pendingPickups;

	// linked list of placements waiting to regenerate pickups
	static fwPtrListSingleLink ms_regeneratingPlacements;

	// linked list of placements waiting to be removed
	static fwPtrListSingleLink ms_removedPlacements;

	// stores the player position when the placements in scope were last calculated 
	static Vector3 ms_cachedPlayerPosition;

	// Allows us to suppress different types of pickups
	static fwFlags32	ms_SuppressionFlags;

	// set when manager is shutting down
	static bool			 ms_bShuttingDown;

	// scripted glow list
	static ScriptedGlowList ms_glowList;
	
	static u32 ms_prohibitedPickupModels[MAX_PROHIBITED_PICKUP_MODELS];
	static bool	ms_hasProhibitedPickupModels;

	// some modes only allow you to collect ammo pickups when low on ammo
	static bool	ms_OnlyAllowAmmoCollectionWhenLowOnAmmo;

	static bool ms_forceRotatePickupFaceUp;

	static void GlowListLoad();

	static float ms_fAmmoAmountScaler;

#if __BANK
	static void GlowListSave();
	static void GlowListAddWidgets(bkBank *bank);
	static void DebugDisplay();
#endif // __BANK

	static bool				ms_AllowNetworkedMoneyPickup;
#if __BANK
	static rage::bkBank*	ms_pBank;
	static rage::bkCombo*	ms_pPickupNamesCombo;
	static int				ms_pickupComboIndex;
	static const char*		ms_pickupComboNames[CPickupData::MAX_STORAGE];
	static int				ms_numPickupComboNames;
	static bool				ms_fixedPickups;
	static bool				ms_regeneratePickups;
	static bool				ms_blipPickups;
	static bool				ms_forceMPStyleWeaponPickups;
	static bool				ms_createAllPickupsOnGround;
	static bool				ms_createAllPickupsWithArrowMarkers;
	static bool				ms_createAllPickupsUpright;
	static bool				ms_displayAllPickupPlacements;
	static bool				ms_NoMoneyDrop;
#if ENABLE_GLOWS
	static bool				ms_forcePickupGlow;
	static bool				ms_forceOffsetPickupGlow;
#endif // ENABLE_GLOWS	
	static bool				ms_debugScriptedPickups;
#endif
};


#endif // PICKUP_MANAGER_H
