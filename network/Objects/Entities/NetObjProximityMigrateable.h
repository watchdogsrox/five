//
// name:		NetObjProximityMigrateable.h
// description:	Derived from CNetObjGame, this class deals with network objects that have a position in the game world and
//				migrate by proximity to other players
// written by:	John Gurney
//

#ifndef NETOBJPROXIMITYMIGRATEABLE_H
#define NETOBJPROXIMITYMIGRATEABLE_H

#include "network/objects/entities/NetObjGame.h"
#include "network/objects/synchronisation/syncnodes/proximitymigrateablesyncnodes.h"

namespace rage
{
    class Color32;
}

class CNetObjProximityMigrateable : public CNetObjGame, public IProximityMigrateableNodeDataAccessor
{
public:

	static const unsigned INVALID_SECTOR       = 0xffff;
    static const float    CONTROL_DIST_MIN_SQR; // control can be taken of an entity if it is at least this distance away from its owner
    static const float    CONTROL_DIST_MAX_SQR; // control of an entity can be transferred to another player if he is closer than this distance to the entity

	template< class T >
	CNetObjProximityMigrateable(T						*gameObject,
							  const NetworkObjectType	type,
							  const ObjectId			objectID,
							  const PhysicalPlayerIndex playerIndex,
							  const NetObjFlags			localFlags,
							  const NetObjFlags			globalFlags)
	: CNetObjGame(gameObject, type, objectID, playerIndex, localFlags, globalFlags)
    , m_proximityMigrationTimer(0)
	, m_failedToPassControl(false)
    , m_scriptMigrationTimer(0)
	, m_sectorX(INVALID_SECTOR)
	, m_sectorY(INVALID_SECTOR)
	, m_sectorZ(INVALID_SECTOR)
	, m_controlTimer(0)
#if __BANK
    , m_MigrationFailTimer(0)
    , m_MigrationFailReason(0)
    , m_MigrationFailCPCReason(0)
#endif // __BANK
    {
    }

	CNetObjProximityMigrateable()
	: m_proximityMigrationTimer(0)
	, m_failedToPassControl(false)
	, m_scriptMigrationTimer(0)
	, m_sectorX(INVALID_SECTOR)
	, m_sectorY(INVALID_SECTOR)
	, m_sectorZ(INVALID_SECTOR)
	, m_controlTimer(0)
#if __BANK
    , m_MigrationFailTimer(0)
    , m_MigrationFailReason(0)
    , m_MigrationFailCPCReason(0)
#endif // __BANK
	{

	}

	// pure virtuals
	virtual bool IsInInterior() const = 0;
	virtual Vector3 GetPosition() const = 0;
	virtual void SetPosition( const Vector3& pos ) = 0;
	virtual bool IsInSameInterior(netObject&) const = 0;

	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

	virtual float	GetScopeDistance(const netPlayer* pRelativePlayer = NULL) const;
	virtual Vector3 GetScopePosition() const { return GetPosition(); }

	// see NetworkObject.h for a description of these functions
	virtual bool IsInScope(const netPlayer& player, unsigned* scopeReason = NULL) const;
	virtual bool Update();
	virtual void StartSynchronising();
	virtual u32	 CalcReassignPriority() const;
	virtual bool CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual void ChangeOwner(const netPlayer& player, eMigrationType migrationType);
    virtual bool CheckPlayerHasAuthorityOverObject(const netPlayer& player);
    virtual void LogAdditionalData(netLoggingInterface &log) const;
	virtual void PostMigrate(eMigrationType migrationType);

	virtual void ResetProximityControlTimer() { m_controlTimer = 0; };
	
	// Tries to pass control of this object to another machine if a player from that machine is closer
	virtual void TryToPassControlProximity();

	// Tries to pass control of this object when it is going out of scope with our players.
	virtual bool TryToPassControlOutOfScope();

	// Tries to pass control of this object to another machine when the player enters a tutorial session
	virtual void TryToPassControlFromTutorial();

   // returns the distance to the closest player squared
    virtual float GetDistanceToNearestPlayerSquared();
    virtual float GetDistanceXYToNearestPlayerSquared();

	void SetProximityMigrationTimer(u32 time) { m_proximityMigrationTimer = time; }
	virtual void SetScriptMigrationTimer(u32 time) { m_scriptMigrationTimer = time; }
	void SetFailedToPassControl(bool val) { m_failedToPassControl = val; }

	void KeepProximityControl(u32 forTime = 0);
	bool IsProximityMigrationPermitted() const;

	virtual bool GetPassControlInTutorial() const { return false; }

	// net obj will migrate to host if nearby
	virtual bool FavourMigrationToHost() const { return false; }

	// the coordinate space is shifted when being sent across the network to save bitspace. These functions calculate the shift.
	static void GetRealMapCoords(const Vector3 &adjustedPos, Vector3 &realPos);
	static void GetAdjustedMapCoords(const Vector3 &realPos, Vector3 &adjustedPos);
	static void GetAdjustedWorldExtents(Vector3 &min, Vector3 &max);

    // sync parser getter functions
	void GetSector(u16& x, u16& y, u16& z);
	void GetSectorPosition(float &sectorPosX, float &sectorPosY, float &sectorPosZ) const;

    // sync parser setter functions
	void SetSector(const u16 x, const u16 y, const u16 z) { m_sectorX = x; m_sectorY = y; m_sectorZ = z; }
    virtual void SetSectorPosition(const float sectorPosX, const float sectorPosY, const float sectorPosZ);

	// IProximityMigrateableNodeDataAccessor functions
	NetObjFlags GetGlobalFlags() const
	{
		return CNetObjGame::GetGlobalFlags();
	}

	u32 GetOwnershipToken() const
	{
		return CNetObjGame::GetOwnershipToken();
	}

	const ObjectId GetObjectID() const
	{
		return CNetObjGame::GetObjectID();
	}

	void GetMigrationData(PlayerFlags &clonedState, DataNodeFlags& unsyncedNodes, DataNodeFlags &clonedPlayersThatLeft);

	void SetGlobalFlags(NetObjFlags globalFlags)
	{
		CNetObjGame::SetGlobalFlags(globalFlags);
	}

	void SetOwnershipToken(u32 ownershipToken)
	{
		CNetObjGame::SetOwnershipToken(ownershipToken);
	}

	void SetMigrationData(PlayerFlags clonedState, DataNodeFlags clonedPlayersThatLeft);

#if ENABLE_NETWORK_LOGGING
    void LogScopeReason(bool inScope, const netPlayer& player, unsigned reason);
#endif

    static void GetSectorCoords(const Vector3& worldCoords, u16& sectorX, u16& sectorY, u16& sectorZ, Vector3* pSectorPos = NULL);
	static void GetWorldCoords(u16 sectorX, u16 sectorY, u16 sectorZ, Vector3& sectorPos, Vector3& worldCoords);
	static	bool IsInSameGarageInterior(const Vector3& entityPos, const Vector3& otherPos, unsigned* scopeReason);

	static void BlockProxyMigrationBetweenTutorialSessions(bool block) { ms_blockProxyMigrationBetweenTutorialSessions = block; }
	static bool IsBlockingProxyMigrationBetweenTutorialSessions() { return ms_blockProxyMigrationBetweenTutorialSessions; }

protected:

    virtual bool IsInHeightScope(const netPlayer& playerToCheck, const Vector3& entityPos, const Vector3& playerPos, unsigned* scopeReason) const;
    virtual bool IsCloserToRemotePlayerForProximity(const CNetGamePlayer &remotePlayer, const float distToCurrentOwnerSqr, const float distToRemotePlayerSqr);

	// returns the distance at which the object will be forcibly migrated to other players during a player transition (death, respawn, etc)
	virtual float GetForcedMigrationRangeWhenPlayerTransitioning() { return FLT_MAX; }

	virtual bool ShouldAllowCloneWhileInTutorial() const { return false; }
#if __BANK
	virtual void DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement);
	virtual void DisplaySyncTreeInfoForObject();

    virtual const char *GetCanPassControlErrorString(unsigned cpcFailReason, eMigrationType migrationType = MIGRATE_PROXIMITY) const;
#endif // __BANK

	u32 m_proximityMigrationTimer;
    u32 m_scriptMigrationTimer;
	bool m_failedToPassControl;
	static bool ms_blockProxyMigrationBetweenTutorialSessions;

private:

    bool ShouldDoMigrationChecksForPlayer(const CNetGamePlayer *player, float distToOwner, float &distToNearestRemotePlayerSqr, const netPlayer *&nearestRemotePlayer);
    bool DoTPCPScriptChecks(const netPlayer *player, bool bMigrateScriptObject, bool nextOwnerIsScriptParticipant, bool bPlayerInSameLocation, bool bLocalPlayerIsParticipantAndNearby, bool &bScriptParticipant, bool &bFoundScriptHost, bool &bSkipThisPlayer) const;
    bool DoTPCPInteriorChecks(bool bPlayerInSameLocation, bool bLocalPlayerIsInSameLocation, bool nextOwnerIsInSameLocation, float distToNewPlayer, float minDist, bool &bSkipThisPlayer) const;
#if __BANK
    void DoMigrationFailureTrackingLogging(const netPlayer &nearestRemotePlayer, unsigned migrationFailReason, unsigned cpcFailReason, eMigrationType migrationType);
#endif // __BANK

	u16				m_sectorX;
	u16				m_sectorY;
	u16				m_sectorZ;
	u32				m_controlTimer;

#if __BANK
    u16             m_MigrationFailTimer;     // timer to keep track of how long far away objects close to other players haven't migrated for
    u8              m_MigrationFailReason;    // last reason this object failed to migrate to a remote player
    u8              m_MigrationFailCPCReason; // last reason CanPassControl() returned false (used only when this function is preventing migration)
#endif // __BANK
};

#endif  // NETOBJPROXIMITYMIGRATEABLE_H
