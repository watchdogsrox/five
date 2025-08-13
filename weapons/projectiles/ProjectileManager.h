#ifndef PROJECTILE_MANAGER_H
#define PROJECTILE_MANAGER_H

// Rage headers
#include "atl/array.h"
// Game headers
#include "Scene/RegdRefTypes.h"
#include "Weapons/WeaponEnums.h"
#include "network/General/NetworkFXId.h"
#include "network/General/NetworkUtil.h"
#include "vector/vector3.h"
#include "Weapons/Info/AmmoInfo.h"
#include "Weapons/Inventory/WeaponItem.h"

// Forward declarations
namespace rage { 
	class bkBank;
	class bkGroup;
	class Matrix34; 
	class Vector3;
	class spdAABB;
}

class CEntity;
class CProjectile;
class CProjectileRocket;
class CNetFXIdentifier;
class CPed;

//////////////////////////////////////////////////////////////////////////
// CProjectileManager
//////////////////////////////////////////////////////////////////////////
struct PendingRocketTargetUpdate
{
	static const u32 MAX_LIFE_TIME = 500;
	ObjectId m_projectileOwnerId;		// Owner of the projectile we are changing
	s32      m_projectileSequenceId;	// id of the projectile we are changing

	ObjectId m_flareOwnerId;			// Owner of flare projectile
	s32		 m_targetFlareGunSequenceId;// id of flare which is the new target

	u32		 m_eventAddedTime;			// Time when event was added to pending list
};


class CProjectileManager
{
	friend class CStickyBombsArrayHandler; // this needs direct access to the ms_NetSyncedProjectiles array

public:

	// The maximum number of projectiles we can allocate
	static const s32 MAX_STORAGE = 50;

	static const s32 MAX_STICKIES_PER_PED_SP = 20;
	static const s32 MAX_STICKIES_PER_PED_MP = 5;

	static const s32 MAX_STICK_TO_PED_PROJECTILES_PER_PED_SP = 10;
	static const s32 MAX_STICK_TO_PED_PROJECTILES_PER_PED_MP = 5;

	static const s32 MAX_FLARE_GUN_PROJECTILES_PER_PED_SP = 10;
	static const s32 MAX_FLARE_GUN_PROJECTILES_PER_PED_MP = 5;

	static const s32 MAX_PENDING_UPDATE_TARGET_EVENTS = 10;

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Since some missions involve more than 5 stickies, the limit depends on whether or not the player is in a network game.
	// See B* 467362
	static s32 GetMaxStickiesPerPed();
	static s32 GetMaxStickToPedProjectilesPerPed();
	static s32 GetMaxFlareGunProjectilesPerPed();

	// Process
	static void Process();
	static void PostPreRender();

	// Add projectile
	static CProjectile* CreateProjectile(u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, const Matrix34& mat, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CEntity* pTarget = NULL, const CNetFXIdentifier* pNetIdentifier = NULL, u32 tintIndex = 0, bool bCreatedFromScript = false, bool bProjectileCreatedFromGrenadeThrow = false);
	
	//Network shared array handling functions
	static void ClearAllNetSyncProjectile();
	static bool AddNetSyncProjectile(s32 &refIndex, CProjectile* projectile);
	static bool AddNetSyncProjectileAtSlot(s32 index, CProjectile* projectile);
	static bool RemoveNetSyncProjectile(CProjectile* projectile);
	static void DirtyNetSyncProjectile(s32 index, bool bStuckToPed = false);
	static bool MoveNetSyncProjectile(s32 index, s32& newSlot);
	static bool IsRemotelyControlled(s32 index);
	static CProjectile * GetExistingNetSyncProjectile(const CNetFXIdentifier& networkIdentifier);
	static CProjectile * GetAnyExistingProjectile(const CNetFXIdentifier& networkIdentifier);
	static bool			FireOrPlacePendingProjectile(CPed *ped, int taskSequenceID, CProjectile* pExistingProjectile = NULL);

	// Remove a projectile
	static void RemoveProjectile(const CProjectile * pProjectile);

	// Remove a projectile (used by network code)
	static void RemoveProjectile(const CNetFXIdentifier& networkIdentifier);
	
	static const CProjectile * GetProjectile(const CNetFXIdentifier& networkIdentifier);

	static void BreakProjectileHoming(const CNetFXIdentifier& networkIdentifier);

	// Remove all projectiles
	static void RemoveAllProjectiles();

	// Remove all the projectiles given an entity and ammo type
	static void RemoveAllProjectilesByAmmoType(u32 uProjectileNameHash, bool bExplode);

	// Remove all the projectiles given an entity and ammo type
	static void RemoveAllPedProjectilesByAmmoType(const CEntity* pOwner, u32 uProjectileNameHash);

	// Remove all the projectiles that are remotely detonated
	static void RemoveAllTriggeredPedProjectiles(const CEntity* pOwner);

	// Remove all the projectiles in the specified bounds
	static void RemoveAllProjectilesInBounds(const spdAABB& bounds);

	// Remove all sticky bombs attached to the given entity
	static void RemoveAllStickyBombsAttachedToEntity(const CEntity* pEntity, bool bInformRemoteMachines = false, const CPed* pOwner = NULL);

	// Explode projectiles of the specified type
	static void ExplodeProjectiles(const CEntity* pOwner, u32 uProjectileNameHash, bool bInstant = false);

	static bool CanExplodeProjectiles(const CEntity* pOwner, u32 uProjectileNameHash);

	// Explode projectiles that are remotely detonated
	static void ExplodeTriggeredProjectiles(const CEntity* pOwner, bool bInstant = false);

	static bool CanExplodeTriggeredProjectiles(const CEntity* pOwner);
	
	//get a list of projectiles for this owner
	static void GetProjectilesForOwner(const CEntity* pOwner, atArray<RegdProjectile>& projectileArray); 

	// Sets the disable explosion crimes for all projectiles owned by this owner
	static void DisableCrimeReportingForExplosions(const CEntity* pOwner);

	// Toggles currently existings projectiles visibility on or off
	static void ToggleVisibilityOfAllProjectiles(bool visible);

	// Returns Script set variable should hide projectiles in cutscene
	static bool GetHideProjectilesInCutscene() { return m_HideProjectilesInCutscene; }
	//
	// Accessors
	//

	// Find any projectile owned by pOwner, of correct type, within specified distance
	static CEntity * GetProjectileWithinDistance(const CEntity* pOwner, u32 uProjectileNameHash, float distance);

	// Find any projectile owned by pOwner, of correct type, within specified distance
	static CEntity * GetProjectileWithinDistance(const Vector3& vPositon, u32 uProjectileNameHash, float distance);

	// Find any projectile owned by pOwner, of correct type, within specified distance and return the newest
	static CEntity * GetNewestProjectileWithinDistance(const CEntity* pOwner, u32 uProjectileNameHash, float distance, bool bStationaryOnly = false);

	// Query if any projectiles that intersect the bounds
	static bool GetIsAnyProjectileInBounds(const spdAABB& bounds, u32 uProjectileNameHash = 0, CEntity* pOwnerFilter = NULL);

	// Query if any projectiles that intersect the bounds
	static bool GetIsAnyProjectileInAngledArea(const Vector3 & vecPoint1, const Vector3 & vecPoint2, float fDistance, u32 uProjectileNameHash = 0, const CEntity* pOwnerFilter = NULL);

	// Get projectile that intersect the bounds
	static const CEntity * GetProjectileInBounds(const spdAABB& bounds, u32 uProjectileNameHash = 0);

	// Get projectile that intersect the bounds
	static const CEntity * GetProjectileInAngledArea(const Vector3 & vecPoint1, const Vector3 & vecPoint2, float fDistance, u32 uProjectileNameHash = 0);

	// Query whether any projectiles owned by the specified owner are attached to the specified entity
	static bool GetIsProjectileAttachedToEntity(const CEntity* pOwner, const CEntity* pAttachedTo, u32 uProjectileNameHash = 0, s32 iAttachBone = -1);
	
	// Silently destroy all sticky projectiles owned by a given entity when leaving MP session or dies
	static void DestroyAllStickyProjectilesForPed(CPed* owner);

	// Populates list CProjectileRocket objects currently targeting an entity within a given range
	static void GetProjectilesToRedirect(const CEntity* pOwnerOfAttractorProjectile, Vec3V_In vAttractorProjectilePos, atArray<CProjectileRocket*>& projectileArray, float fMinDistanceToTarget = 0.0f, float fMaxDistanceFromAtractor = 0.0f);

	// Adds a new projectile event to the pending list
	static void AddPendingProjectileTargetUpdateEvent(PendingRocketTargetUpdate newPendingEvent);

	// Update loop for pending projectiles
	static void UpdatePendingProjectileTargetUpdateEvent();

	//
	// Debug
	//

#if __BANK
	// Widgets
	static bkGroup* InitWidgets(bkBank& bank);

	// Debug rendering
	static void RenderDebug();

	// Set the last explosion point
	static void SetLastExplosionPoint(const Vector3& vExplosionPosition, const Vector3& vExplosionNormal);
#endif // __BANK

	static s32 AllocateNewFlareSequenceId()
	{
		s32 result = m_FlareGunSequenceId;
		m_FlareGunSequenceId++;
		if(m_FlareGunSequenceId >= MAX_FLARE_GUN_PROJECTILES_PER_PED_MP)
		{
			m_FlareGunSequenceId = 0;
		}

		return result;
	}

	static void ToggleHideProjectilesInCutscene(bool hide)
	{
		m_HideProjectilesInCutscene = hide;
	}

private:

	// Silently destroy the oldest projectile owned by a given entity
	static void DestroyOldestProjectileForPed(CPed* owner, const CAmmoProjectileInfo* pInfo);

	// Projectile pointers
	static atFixedArray<RegdProjectile, MAX_STORAGE> ms_projectiles;

public:
	class CProjectilePtr
	{
	public:
		CProjectilePtr() : m_RegdProjectile() {m_pWeaponItemForStreaming = NULL;}
		CProjectilePtr(CProjectile* pProjectile) : m_RegdProjectile(pProjectile) {m_pWeaponItemForStreaming = NULL;}
		~CProjectilePtr()
		{
			if (m_pWeaponItemForStreaming)
			{
				// need to restore the game heap here in case the below functions deallocate memory, as this
				// function is called while the network heap is active
				sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
				sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

				delete m_pWeaponItemForStreaming;
				m_pWeaponItemForStreaming = NULL;

				sysMemAllocator::SetCurrent(*oldHeap);
			}
		}

		CProjectile* Get() const
		{
			return m_RegdProjectile;
		}

		void Serialise(CSyncDataBase& serialiser);
		void Clear(bool bDestroy = true);
		void Copy(CProjectilePtr &projectilePtr);
		void CopyNetworkInfoCreateProjectile(const netPlayer& player, unsigned arrayIndex);
		bool IsProjectileModelLoaded();

		ObjectId GetStickEntityId() const { return ms_stickEntityID; }

	protected:

		void AdjustSentValuesForObjectOffset(const CEntity* pTargetEntity, const CProjectile* pProjectile);
		void AdjustReceivedValuesForObjectOffset(const CEntity* pTargetEntity);

		RegdProjectile m_RegdProjectile;

		// static members used when serialising
		static ObjectId				ms_ownerId;					// projectile owner (who fired it)
		static NetworkFXId			ms_fxId;					// the projectile's network identifier
		static u32					ms_taskSequenceId;			// the sequence id of the throw projectile task that created the sticky bomb 
		static u32					ms_projectileHash;			// projectile type
		static u32					ms_weaponFiredFromHash;		
		static eWeaponEffectGroup	ms_effectGroup;				// projectile weapon effect group
		static Matrix34				ms_projectileMatrix;		// the projectile's global matrix
		static bool					ms_stickEntity;				// attached to a networked or non-networked entity - e.g. van or ambient prop
		static ObjectId				ms_stickEntityID;			// the net id of the entity attached to 
		static Vector3				ms_stickPosition;			// stick position global to map or local to sticky entity
		static Quaternion			ms_stickOrientation;		// stick orientation global to map or local to sticky entity
		static s32					ms_stickComponent;			// the component of the entity stuck to
		static u32					ms_stickMaterial;			// the material at the entity stick position

		// weapon streaming helper
		CWeaponItem* m_pWeaponItemForStreaming;
	};

private:
	static atFixedArray<CProjectilePtr, MAX_STORAGE> ms_NetSyncedProjectiles;

	static atFixedArray<PendingRocketTargetUpdate, MAX_PENDING_UPDATE_TARGET_EVENTS> ms_pendingProjectileTargetUpdateEvents;
	static s32 m_FlareGunSequenceId;

	static bool m_HideProjectilesInCutscene;
#if __BANK
	// Last projectile explosion point
	static Vector3 ms_vLastExplosionPosition;

	// Last projectile explosion normal
	static Vector3 ms_vLastExplosionNormal;
#endif // __BANK
};

#endif // PROJECTILE_MANAGER_H
