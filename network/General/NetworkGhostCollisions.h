//
// name:        NetworkGhostCollisions.h
// description: Used to resolve collisions between ex-ghost (ie passive mode) vehicles
// written by:  John Gurney
//

#ifndef NETWORK_GHOST_COLLISIONS_H
#define NETWORK_GHOST_COLLISIONS_H

#include "scene/RegdRefTypes.h"
#include "scene/Physical.h"

class CPhysical;
class CNetObjPhysical;

class CNetworkGhostCollisions 
{
private:

	typedef u64 GhostCollisionFlags;

	class CGhostCollision
	{
	public:

		CGhostCollision() { Clear(); }

		bool IsSet() const { return m_pPhysical != NULL; }

		void SetPhysical(const CPhysical& phys)  { m_pPhysical = &phys; }
		const CPhysical* GetPhysical() const		{ return m_pPhysical; }

		CNetObjPhysical* GetPhysicalNetObj() const;
		const char* GetPhysicalLogName() const;

		void SetCollidingWith(u32 i, u32 frame) { m_collisionFlags |= ((GhostCollisionFlags)1 << i); m_frameAdded = frame; }
		void ClearCollidingWith(u32 i) { m_collisionFlags &= ~((GhostCollisionFlags)1 << i); }
		bool IsCollidingWith(u32 i) { return ((m_collisionFlags & ((GhostCollisionFlags)1 << i)) != 0); }
		bool IsColliding() { return m_collisionFlags != 0; }

		GhostCollisionFlags GetCollisionFlags() const { return m_collisionFlags; }

		u32  GetFrameAdded() const { return m_frameAdded; }
		u32  GetNumFreeAttempts() const { return m_numFreeAttempts; }
		void IncNumFreeAttempts() { m_numFreeAttempts++; }

		void Clear();

	private:

		RegdConstPhy			m_pPhysical;
		GhostCollisionFlags		m_collisionFlags;
		u32						m_frameAdded;
		u32						m_numFreeAttempts;
	};

	static const unsigned FRAME_COUNT_BEFORE_FREE_CHECK = 20;   
	static const unsigned FRAME_COUNT_BETWEEN_FREE_CHECKS = 3; 

public:

	static const unsigned MAX_GHOST_COLLISIONS = sizeof(GhostCollisionFlags)<<3; 
	static const unsigned INVALID_GHOST_COLLISION_SLOT = 0xff;   

	// returns true if the collision between the two given entities should be treated as a ghost collision (ie ignored)
	static bool IsAGhostCollision(const CPhysical& physical1, const CPhysical& physical2);

	// returns true if a ghost collision is currently in process between the two given entities (ie they are intersecting)
	static bool IsInGhostCollision(const CPhysical& physical1, const CPhysical& physical2);

	// returns true if the given physical is in ghost collision with any other entity
	static bool IsInGhostCollision(const CPhysical& physical);

	// returns true if the collision between the local player and the given entity should be treated as a ghost collision (ie ignored)
	static bool IsLocalPlayerInGhostCollision(const CPhysical& physical);

	// returns true if the local player is currently in any ghost collisions
	static bool IsLocalPlayerInAnyGhostCollision();

	// returns all of the entities currently in ghost collision with the given entity
	static void GetAllGhostCollisions(const CPhysical& physical, const CPhysical* collisions[MAX_GHOST_COLLISIONS], u32& numCollisions);

	// registers a ghost (ie ignored) collision between two entities
	static void RegisterGhostCollision(const CPhysical& physical1, const CPhysical& physical2);

	// removes the ghost collision entry for the given slot, and clears all collision flags referencing it.
	static void RemoveGhostCollision(int slot, bool allowAssert=true);

	// updates the ghost collision array
	static void Update();
	
	// returns the driver of a vehicle (includes trailers)
	static CPed* GetDriverFromVehicle(const CVehicle& vehicle);

private:

	// returns true if the collision between the two given peds should be treated as a ghost collision (ie ignored)
	static bool IsAGhostCollision(const CPed& ped1, const CPed& ped2);

	// returns true if the collision between a given ped and vehicle should be treated as a ghost collision (ie ignored)
	static bool IsAGhostCollision(const CPed& ped, const CVehicle& vehicle);

	// used to sort the collisions based on the length of time they have been in the array
	static int SortCollisions(const void *paramA, const void *paramB);

	// finds or makes space for a free collision slot
	static u32 FindFreeCollisionSlot(const CPhysical& physical);

	// does a collision check to see if the entity in the given ghost collision slot is now free of intersections with other entities
	static void TryToFreeGhostCollision(int slot);

	static CGhostCollision ms_aGhostCollisions[MAX_GHOST_COLLISIONS];
};

#endif  // #NETWORK_GHOST_COLLISIONS_H
