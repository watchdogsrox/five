//
// name:		NetObjEntity.h
// description:	Derived from netObject, this class handles all entity-related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	John Gurney
//

#ifndef NETOBJENTITY_H
#define NETOBJENTITY_H

#include "network/objects/entities/NetObjProximityMigrateable.h"

#include "network/objects/synchronisation/syncnodes/entitysyncnodes.h"

#include "Scene/Entity.h"

namespace rage
{
    class netPlayer;
}

class CNetObjEntity : public CNetObjProximityMigrateable, public IEntityNodeDataAccessor
{
private:

    static const unsigned DELETION_TIME                   = 60; // the time in frames within which the entity will be deleted if it becomes local
    static const unsigned NUM_PLAYERS_TO_UPDATE_IMPORTANT = 4;  // the number of players to update per frame for important objects for operations batched by player
    static const unsigned NUM_PLAYERS_TO_UPDATE_NORMAL    = 1;  // the number of players to update per frame for normal objects for operations batched by player

public:

    static const float MIN_ENTITY_REMOVAL_DISTANCE_SQ;

	CNetObjEntity(class CEntity				 *entity,
				  const NetworkObjectType	 type,
				  const ObjectId			 objectID,
				  const PhysicalPlayerIndex  playerIndex,
				  const NetObjFlags			 localFlags,
				  const NetObjFlags          globalFlags);

    CNetObjEntity() :
    m_bUsesCollision(true),
    m_bIsVisible(true),
	m_deletionTimer(0),
	m_ForceSendEntityScriptGameState(false),
	m_FrameForceSendRequested(0),
	m_EnableCollisonPending(false),
	m_DisableCollisionCompletely(false),
	m_ShowRemotelyInCutsene(false),
	m_KeepIsVisibleForCutsceneFlagSet(false)
	{
	}

	virtual CEntity* GetEntity() const { return (CEntity*)GetGameObject(); }

	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);
	virtual bool IsInInterior() const;
	virtual Vector3 GetPosition() const;
	virtual void SetPosition( const Vector3& pos );
	virtual bool CanDeleteWhenFlaggedForDeletion(bool useLogging);

	void SetOverridingRemoteVisibility(bool bOverride, bool bVisible, const char* reason);
	void SetOverridingRemoteCollision(bool bOverride, bool bCollision, const char* reason);
	bool GetOverridingRemoteVisibility() const { return m_RemoteVisibilityOverride.m_Active; }
	bool GetOverridingRemoteCollision() const { return m_RemoteCollisionOverride.m_Active; }

	void         SetOverridingLocalVisibility(bool bVisible, const char* reason, bool bIncludeChildAttachments = true, bool bLogScriptName = false);
    virtual bool CanOverrideLocalCollision() const { return true; }
	void         SetOverridingLocalCollision(bool bCollision, const char* reason);
	bool         GetOverridingLocalCollision() const { return m_LocalCollisionOverride.m_Active; }
	void         ClearOverridingLocalCollisionImmediately(const char* reason);

	void FlagForDeletion();
	bool IsFlaggedForDeletion() const { return m_deletionTimer > 0; }

	void SetLeaveEntityAfterCleanup(bool bLeave)	{ m_bLeaveEntityAfterCleanup = bLeave;}
	bool GetEntityLeftAfterCleanup() const			{ return m_bLeaveEntityAfterCleanup; }

	void SetRemotelyVisibleInCutscene(bool bVisible) { m_ShowRemotelyInCutsene = bVisible; }
	bool GetRemotelyVisibleInCutscene() const		 { return m_ShowRemotelyInCutsene; }

	void RequestForceSendOfEntityScriptGameState() { m_FrameForceSendRequested = static_cast<u8>(fwTimer::GetSystemFrameCount()); m_ForceSendEntityScriptGameState = true; }

	// see NetworkObject.h for a description of these functions
	virtual void		ManageUpdateLevel(const netPlayer& player);
	virtual bool        Update();
	virtual void		OnUnregistered();
	virtual void		CleanUpGameObject(bool) {}
    virtual bool        IsInScope(const netPlayer& player, unsigned* scopeReason = NULL) const;
	virtual bool		CanPassControl(const netPlayer&   player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual bool        CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual void		ChangeOwner(const netPlayer& player, eMigrationType migrationType);

	// handles process control on game object
	virtual bool ProcessControl() { return true; }

	// called once a frame just before the physics updates
	virtual void UpdateBeforePhysics();

	// called once a frame after all physics updates
	virtual void UpdateAfterPhysics() {}

    // called once a frame after all code that can affect an entities position has been processed
    virtual void UpdateAfterAllMovement() {}

	// called once a frame after the scripts have been processed
	virtual void UpdateAfterScripts();

	// works out if the given player can currently see this entity
	virtual bool IsVisibleToPlayer(const netPlayer& player, Vector3* pNewPos = NULL) const;

	// returns true if this entity is an important game play entity that requires a higher update level, etc
	virtual bool IsImportant() const { return IsScriptObject(); }

    // returns how many players should be updated per frame for operations batched by players
    virtual u8 GetNumPlayersToUpdatePerBatch() const { return IsImportant() ? NUM_PLAYERS_TO_UPDATE_IMPORTANT : NUM_PLAYERS_TO_UPDATE_NORMAL; }

	// if true the entity can enable collision
	virtual bool CanEnableCollision() const { return true;}

	// works out if any remote player can currently see this entity
    bool IsVisibleToAnyRemotePlayer() const;

	// returns the script visibility state of the entity
	bool IsVisibleToScript() const;

	// forces an immediate send of current position 
	void ForceSendOfPositionData();
	
	// sync parser getter functions
	void GetEntityScriptInfo(bool& hasScriptInfo, CGameScriptObjInfo& scriptInfo) const;
    void GetEntityScriptGameState(CEntityScriptGameStateDataNode& data);
    void GetEntityOrientation(CEntityOrientationDataNode& data);

    // sync parser setter functions
	void SetEntityScriptInfo(bool hasScriptInfo, CGameScriptObjInfo& scriptInfo);
    void SetEntityScriptGameState(const CEntityScriptGameStateDataNode& data);
    virtual void SetIsFixed(bool isFixed);
    void SetIsFixedWaitingForCollision(bool isFixedWaitingForCollision);
    virtual void SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate = false);
    virtual void SetUsesCollision(bool usesCollision, const char* reason);
    virtual void SetEntityOrientation(const CEntityOrientationDataNode& data);
	virtual void DisableCollision();

    // displays debug info above the network object
	virtual void DisplayNetworkInfo();

	// hides the entity for a cutscene 
	virtual void HideForCutscene();

	// exposes the entity after a cutscene 
	virtual void ExposeAfterCutscene();

	virtual void UpdateLocalVisibility();

#if __ASSERT
	virtual void ValidateMigration() const;
#endif

	void SetKeepIsVisibleForCutsceneFlagSet(bool set) { m_KeepIsVisibleForCutsceneFlagSet = set; }
protected:

	// some entities can specify their highest or lower update level
	virtual u8 GetHighestAllowedUpdateLevel() const { return CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH; }
	virtual u8 GetLowestAllowedUpdateLevel() const { return CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW; }

#if __BANK
    virtual void DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const;
#endif // __BANK

	virtual bool GetCachedEntityCollidableState();

	virtual void SetIsVisibleForModuleSafe(CEntity* pEntity, const eIsVisibleModule module, bool bIsVisible, bool bProcessChildAttachments = false);

public:
	virtual bool IsLocalEntityCollidableOverNetwork();
	bool IsLocalEntityVisibleOverNetwork();
	bool GetDisableCollisionCompletely() { return m_DisableCollisionCompletely; }
protected:

	bool IsLocalEntityVisibilityOverriden(bool& visible) const { visible = m_LocalVisibilityOverride.m_Visible; return m_LocalVisibilityOverride.m_Active; }
	bool IsLocalEntityCollisionOverriden(bool& collideable) const { collideable = m_LocalCollisionOverride.m_Collision; return m_LocalCollisionOverride.m_Active; }

#if __BANK
	virtual u32 GetNetworkInfoStartYOffset() { return 0; }
	virtual bool ShouldDisplayAdditionalDebugInfo() { return false; }
	virtual void GetOrientationDisplayString(char *buffer) const {buffer[0]='\0';}
	virtual void GetHealthDisplayString(char *buffer) const {buffer[0]='\0';}
	virtual void GetExtendedFlagsDisplayString(char *buffer) const {buffer[0]='\0';}
    virtual const char *GetNetworkBlenderIsOverridenString(unsigned resultCode) const;
#endif

private:

	void UpdateForceSendRequests();

	// override visibility structure, used
	// to override entity visibility on local/remote machines
	struct OverrideVisibility
	{
		OverrideVisibility() : m_Active (false), m_Visible(true), m_Set(false), m_SetLastFrame(false), m_IncludeChildAttachments(true)
		{
		}

		bool m_Active : 1;					// are we overriding visibility settings?
		bool m_Visible : 1;					// what value should be given
		bool m_Set : 1;						// set this frame
		bool m_SetLastFrame : 1;			// set last frame
		bool m_IncludeChildAttachments : 1; // alter visibility of child attachments as well 
	};

	// override Collision structure, used
	// to override entity Collision on local/remote machines
	struct OverrideCollision
	{
		OverrideCollision() : m_Active (false), m_Collision(true), m_Set(false), m_SetLastFrame(false)
		{
		}

		bool m_Active : 1;				// are we overriding Collision settings?
		bool m_Collision : 1;			// what value should be given
		bool m_Set : 1;					// set this frame
		bool m_SetLastFrame : 1;		// set last frame
	};

	u8 m_deletionTimer;

	// Used for cases where an entities visibility status is different on the
	//  local machine to what it should be on remote machine.
	OverrideVisibility	m_RemoteVisibilityOverride;
	OverrideCollision	m_RemoteCollisionOverride;

	// Used for cases where an entities visibility status is different on the
	//  local machine to what it should be on the local machine.
	// Override the cached copy of the visible flag.
	OverrideVisibility	m_LocalVisibilityOverride;
	// Override the cached copy of the collision flag.
	OverrideCollision	m_LocalCollisionOverride;

	u8   m_FrameForceSendRequested;

	// cached copy of the uses collision flag
	bool   m_bUsesCollision : 1;
	// cached copy of the is visible flag
	bool   m_bIsVisible : 1;
	// if this is set, the entity is not removed during Cleanup() when the network object is being destroyed 
	bool   m_bLeaveEntityAfterCleanup : 1;
	// set true to make the entity visible on other machines during a cutscene
	bool   m_ShowRemotelyInCutsene : 1;
	// set when the received collision state cannot be applied immediately
	bool   m_EnableCollisonPending : 1;

	bool   m_ForceSendEntityScriptGameState : 1;

	bool   m_DisableCollisionCompletely : 1;

	bool   m_KeepIsVisibleForCutsceneFlagSet : 1;
#if __ASSERT
	u32 m_notInWorldTimer;
#endif
#if __BANK
	u32      m_cloneCreateFailTimer;
public:
	static bool ms_logLocalVisibilityOverride;
#endif
protected:
    Matrix34 m_LastReceivedMatrix;
};

#endif  // NETOBJENTITY_H
