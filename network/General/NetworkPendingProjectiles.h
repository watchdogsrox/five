//
// name:        NetworkPendingProjectiles.h
// description: Maintains a list of projectile information received over the network that needs to be
//              delayed until another event occurs (such as waiting for a clone task to reach a certain stage)
// written by:  Daniel Yelland
//

#ifndef NETWORK_PENDING_PROJECTILES_H
#define NETWORK_PENDING_PROJECTILES_H

class CEntity;
class CPed;

#include "Network/General/NetworkFXId.h"
#include "vector/matrix34.h"
#include "vector/vector3.h"
#include "weapons/WeaponEnums.h"
#include "Scene/RegdRefTypes.h"

// rage includes
#include "vector/quaternion.h"

struct ProjectileInfo
{
    ProjectileInfo() :
    m_active(false),
    m_projectileHash(0),
    m_weaponFiredFromHash(0),
    m_effectGroup(NUM_EWEAPONEFFECTGROUP),
    m_taskSequenceID(0),
    m_timeAdded(0),
    m_fireDirection(Vector3(0.0f, 0.0f, 0.0f)),
	m_bAllowDamping(true),
	m_fLaunchSpeedOverride(-1.0f),
	m_stickyBomb(false),
	m_stickyBombArraySlot(0),
	m_stickEntity(false),
	m_stickEntityID(NETWORK_INVALID_OBJECT_ID),
	m_stickPosition(VEC3_ZERO),
	m_stickComponent(-1),
	m_stickMaterial(0)
    {
    }

    bool                 m_active;
    RegdEnt				 m_entity;
    RegdEnt				 m_targetEntity;
    RegdEnt              m_projectileEntity;
    u32                  m_projectileHash;
    u32                  m_weaponFiredFromHash;
    eWeaponEffectGroup   m_effectGroup;
    Matrix34             m_projectileMatrix;
    u32                  m_taskSequenceID;
    u32                  m_timeAdded;
    Vector3              m_fireDirection;
    CNetFXIdentifier     m_identifier;
	bool				 m_bAllowDamping;
	bool				 m_bWasLockedOnWhenFired;
	float				 m_fLaunchSpeedOverride;

	// sticky bomb stuff:
	bool				 m_stickyBomb;
	unsigned			 m_stickyBombArraySlot;
	bool				 m_stickEntity;
	ObjectId			 m_stickEntityID;			// attached to entity - e.g. van
	Vector3				 m_stickPosition;			// stick position global to map or local to sticky entity
	Quaternion			 m_stickOrientation;		// stick orientation global to map or local to sticky entity
	s32					 m_stickComponent;			// the component of the entity stuck to
	u32					 m_stickMaterial;			// the material at the entity stick position
};

class CNetworkPendingProjectiles
{
public:

    static void AddProjectile(CEntity                *entity,
                              u32                     taskSequenceID,
                              u32                     projectileHash,
                              u32                     weaponFiredFromHash,
                              const Matrix34         &projectileMatrix,
                              eWeaponEffectGroup      effectGroup,
                              CEntity                *targetEntity,
                              CEntity                *projectileEntity,
                              Vector3&                fireDirection,
                              const CNetFXIdentifier &identifier,
							  bool					  bAllowDamping = true,
							  bool					  bWasLockedOnWhenFired = false,
							  float					  fLaunchSpeedOverride = -1.0f);

	static bool AddStickyBomb(u32					  arraySlot,
								CEntity                *entity,
								u32                   taskSequenceID,
								u32                   projectileHash,
								u32                   weaponFiredFromHash,
								const Matrix34        &projectileMatrix,
								eWeaponEffectGroup    effectGroup,
								const CNetFXIdentifier &identifier,
								bool				  stickEntity,
								ObjectId			  stickEntityId,
								Vector3&			  stickPosition,
								Quaternion&			  stickOrientation,
								s32					  stickComponent,
								u32					  stickMaterial);

	static ProjectileInfo *GetProjectile(CPed *ped, u32 taskSequenceID, bool removeFromList = true);

	static void RemoveProjectile(CPed *ped, const CNetFXIdentifier& networkIdentifier);

	static void RemoveAllProjectileData();
	static void RemoveAllStickyBombsForPed(CPed *ped);
	static void RemoveAllStickyBombsAttachedToEntity(const CEntity *entity);
	static void RemoveStickyBomb(u32 arraySlot);
    static void Update();

	// Script created projectiles or Task created projectiles
	static const unsigned int NUM_TYPES_OF_PENDING_PROJECTILES = 2;
	static const unsigned int MAX_PENDING_PROJECTILES_PER_TYPE = 50;
	static const unsigned int MAX_PENDING_PROJECTILES = (NUM_TYPES_OF_PENDING_PROJECTILES * MAX_PENDING_PROJECTILES_PER_TYPE);

private:	
    static ProjectileInfo m_aProjectileStorage[MAX_PENDING_PROJECTILES];
};

#endif  // NETWORK_PENDING_PROJECTILES_H
