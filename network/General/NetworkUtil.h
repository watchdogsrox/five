//
// name:        NetworkUtil.h
// description: Network utilities: various bits & bobs used by network code
// written by:  John Gurney
//

#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H

#include "network/NetworkTypes.h"
#include "network/networkinterface.h"
#include "network/general/NetworkColours.h"
#include "network/Objects/Entities/NetObjProximityMigrateable.h"
#include "network/Objects/NetworkObjectMgr.h"
#include "network/Players/NetGamePlayer.h"
#include "network/Players/NetworkPlayerMgr.h"
#include "scene/dynamicentity.h"
#include "task/system/TaskClassInfo.h"
#include "vector/vector3.h"

// framework includes
#include "fwnet/netserialisers.h"
#include "fwnet/nettypes.h"

// rage include
#include "snet/session.h"

namespace rage
{
    class netObject;
    class netPlayer;
}

// simple class that will keep trying to request an object, used by some tasks
class CNetworkRequester
{
public:

    enum
    {
        NETWORK_REQUESTER_FREQ = 500,          // how often requests are sent
    };

public:

    CNetworkRequester();
    CNetworkRequester(netObject* pObject);
    ~CNetworkRequester();

    void Reset();
    void RequestObject(netObject* pObject);
    void Process();
    bool IsRequestSuccessful() const;
    bool IsInUse() const;
 
    netObject *GetNetworkObject();

private:

    void Request();
 
    netObject*     m_pObject;
    int                 m_requestTimer;
    bool                m_bWasPermanent;
};

//-------------------------------------------------------------------------------
//Dead strip out name
#if ENABLE_NETWORK_LOGGING
#define SERIALISE_SYNCEDENTITY(entity,serialiser,...)			entity.Serialise(serialiser, ##__VA_ARGS__);
#else
#define SERIALISE_SYNCEDENTITY(entity,serialiser,...)			entity.Serialise(serialiser);
#endif
//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------
// These structures are used to store reference counted pointers to game
// entities along with their network ID, which is used to sync over the network
//
// TODO : TEMPLATE THESE!!!!!!!! Sigh...
//-------------------------------------------------------------------------
class CSyncedEntity
{
public:

	CSyncedEntity();
	CSyncedEntity(CEntity *entity);
	CSyncedEntity(const CSyncedEntity& syncedEntity);

	CEntity *GetEntity();
	const CEntity *GetEntity() const;
	void  SetEntity(CEntity *entity);

	ObjectId &GetEntityID();
	const ObjectId &GetEntityID() const;
	void      SetEntityID(const ObjectId &entityID);

	bool IsValid() const { return ((GetEntity() != 0) || m_entityID != NETWORK_INVALID_OBJECT_ID); }
	void Invalidate()	 { m_entity = 0; m_entityID = NETWORK_INVALID_OBJECT_ID; }

	bool operator==(const CSyncedEntity& other) const
	{
		if (GetEntity() && other.GetEntity())
		{
			return GetEntity() == other.GetEntity();
		}
		else if (m_entityID != NETWORK_INVALID_OBJECT_ID)
		{
			return m_entityID == other.m_entityID;
		}

		return false;
	}

	void Serialise(CSyncDataBase& serialiser, const char* LOGGING_ONLY(name) = NULL)
	{
		SERIALISE_OBJECTID(serialiser, m_entityID, name);

		if (m_entityID == NETWORK_INVALID_OBJECT_ID)
		{
			Invalidate();
		}
	}

private:

	void SetEntityFromEntityID();
	void SetEntityIDFromEntity();

	ObjectId m_entityID;
	RegdEnt  m_entity;
};

class CSyncedPed
{
public:

	CSyncedPed();
	CSyncedPed(CPed *ped);

	CPed *GetPed();
	CPed *GetPed() const;
	void  SetPed(CPed *ped);

	ObjectId &GetPedID();
	const ObjectId &GetPedID() const;
	void      SetPedID(const ObjectId &pedID);

	bool IsValid() const { return ((GetPed() != 0) || m_pedID != NETWORK_INVALID_OBJECT_ID); }
	void Invalidate() { m_pedID = NETWORK_INVALID_OBJECT_ID; m_pedEntityID = 0; }

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_OBJECTID(serialiser, m_pedID, "Ped ID");

		if (m_pedID == NETWORK_INVALID_OBJECT_ID)
		{
			Invalidate();
		}
	}

private:

	void SetPedFromPedID();
	void SetPedIDFromPed();

	ObjectId m_pedID;
	u32	m_pedEntityID;
};

class CSyncedVehicle
{
public:

	CSyncedVehicle();
	CSyncedVehicle(CVehicle *vehicle);

	CVehicle *GetVehicle();
	const CVehicle *GetVehicle() const;
	void SetVehicle(CVehicle *vehicle);

	ObjectId &GetVehicleID();
	const ObjectId &GetVehicleID() const;
	void      SetVehicleID(const ObjectId &vehicleID);

	bool IsValid() const { return ((GetVehicle() != 0) || m_vehicleID != NETWORK_INVALID_OBJECT_ID); }
	void Invalidate() { m_vehicleID = NETWORK_INVALID_OBJECT_ID; m_vehicle = 0; }

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle ID");

		if (m_vehicleID == NETWORK_INVALID_OBJECT_ID)
		{
			Invalidate();
		}
	}

private:

	void SetVehicleFromVehicleID();
	void SetVehicleIDFromVehicle();

	ObjectId m_vehicleID;
	RegdVeh  m_vehicle;
};

class CEntity;
class CObject;
class CPed;
class CPickup;
class CPickupPlacement;
class CPlayerInfo;
class CVehicle;
class CPlane;

namespace rage
{
    class netLoggingInterface;
    class netPlayer;
}

namespace NetworkUtils
{
	// Returns the network object for the specified entity
    inline netObject *GetNetworkObjectFromEntity(const CEntity *pEntity)
    {
        if(pEntity && pEntity->GetIsDynamic())
        {
            const CDynamicEntity *dynamicEntity = static_cast<const CDynamicEntity *>(pEntity);
            return dynamicEntity->GetNetworkObject();
        }

        return 0;
    }

	// Returns whether the specified entity is a network clone
	inline bool IsNetworkClone(const CEntity *pEntity)
	{
		if(pEntity && pEntity->GetIsDynamic())
		{
			const CDynamicEntity *dynamicEntity = static_cast<const CDynamicEntity *>(pEntity);
			return dynamicEntity->IsNetworkClone();
		}

		return false;
	}

	// Returns whether the specified entity is local and not in the process of migrating
	inline bool IsNetworkCloneOrMigrating(const CEntity *pEntity)
	{
		if(pEntity && pEntity->GetIsDynamic())
		{
			const CDynamicEntity *dynamicEntity = static_cast<const CDynamicEntity *>(pEntity);

			if (dynamicEntity->GetNetworkObject())
			{
				if (dynamicEntity->GetNetworkObject()->IsClone() || dynamicEntity->GetNetworkObject()->IsPendingOwnerChange())
				{
					return true;
				}
			}
		}

		return false;
	}

	//Returns the position of a network player regardless of whether the player is physical/non physical
    Vector3 GetNetworkPlayerPosition(const CNetGamePlayer &networkPlayer);

    //Returns the viewport of a network player
    grcViewport *GetNetworkPlayerViewPort(const CNetGamePlayer &networkPlayer);

    //Returns a name for the specified object type
    const char *GetObjectTypeName(const NetworkObjectType objectType, bool isMissionObject);

    //
    // name:        GetObjectIdFromGameObject
    // description: Templated function for getting the network object ID from a game object pointer
    // note:        Inlined for template instantiation
    //
    template <typename T> const ObjectId GetObjectIDFromGameObject(T *gameObject)
    {
        if(gameObject)
        {
            netObject *networkObject = GetNetworkObjectFromEntity(gameObject);

            if(networkObject)
            {
                return networkObject->GetObjectID();
            }
        }

        return NETWORK_INVALID_OBJECT_ID;
    }

    // Returns a CObject pointer from the specified network object if it is of the correct type
    CObject *GetCObjectFromNetworkObject(netObject *networkObject);
    const CObject *GetCObjectFromNetworkObject(const netObject *networkObject);

	// Returns a CPickup pointer from the specified network object if it is of the correct type
	CPickup *GetPickupFromNetworkObject(netObject *networkObject);
    const CPickup *GetPickupFromNetworkObject(const netObject *networkObject);

	// Returns a CPickupPlacement pointer from the specified network object if it is of the correct type
	CPickupPlacement *GetPickupPlacementFromNetworkObject(netObject *networkObject);
	const CPickupPlacement *GetPickupPlacementFromNetworkObject(const netObject *networkObject);

	// Returns a ped pointer from the specified network object if it is of the correct type
    CPed *GetPedFromNetworkObject(netObject *networkObject);
    const CPed *GetPedFromNetworkObject(const netObject *networkObject);

    // Returns a ped pointer from the specified network object if it is of the correct type
    CPed *GetPlayerPedFromNetworkObject(netObject *networkObject);
    const CPed *GetPlayerPedFromNetworkObject(const netObject *networkObject);

    // Returns a vehicle pointer from the specified network object if it is of the correct type
    CVehicle *GetVehicleFromNetworkObject(netObject *networkObject);
    const CVehicle *GetVehicleFromNetworkObject(const netObject *networkObject);

    // Returns a train pointer from the specified network object if it is of the correct type
    CTrain *GetTrainFromNetworkObject(netObject *networkObject);
    const CTrain *GetTrainFromNetworkObject(const netObject *networkObject);

	// Returns a plane pointer from the specified network object if it is of the correct type
	CPlane *GetPlaneFromNetworkObject(netObject *networkObject);
	const CPlane *GetPlaneFromNetworkObject(const netObject *networkObject);

	// Returns true for a given object position and object radius if it is within the given distance and visible to supplied players.
	bool IsVisibleToPlayer(const CNetGamePlayer* pPlayer, const Vector3& position, const float radius, const float distance, const bool bIgnoreExtendedRange = false);

    // Returns true for a given object position and object radius if it is within the given distance and visible to any remote players.
    // Will also return a player who it is visible to if needed
    bool IsVisibleToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, const netPlayer** ppVisiblePlayer = NULL, const bool bIgnoreExtendedRange = false);

    // Returns true for a given object position and object radius if it is within the given distance and visible to any local or remote players.
    // Also returns true if the object is withing the minDistance from another player
    // Will also return a player who it is visible to if needed
    bool IsVisibleOrCloseToAnyPlayer(const Vector3& position, const float radius, const float distance, const float minDistance, const netPlayer** ppVisiblePlayer = NULL, const bool predictFuturePosition = false, const bool bIgnoreExtendedRange = false);

    // Returns true for a given object position and object radius if it is within the given distance and visible to any remote players.
    // Also returns true if the object is within the minDistance from another player
    // Will also return a player who it is visible to if needed
    bool IsVisibleOrCloseToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, const float minDistance, const netPlayer** ppVisiblePlayer = NULL, const bool predictFuturePosition = false, const bool bIgnoreExtendedRange = false);

    // Returns true for a given object position if any players (local or remote) are closer than the given radius
    bool IsCloseToAnyPlayer(const Vector3& position, const float radius, const netPlayer*& ppClosestPlayer);

    // Returns true for a given object position if any remote players are closer than the given radius
    bool IsCloseToAnyRemotePlayer(const Vector3& position, const float radius, const netPlayer*& ppClosestPlayer);

    // builds an array of players within the given position and radius and can be sorted by distance, returns the number of players added to the array
    u32 GetClosestRemotePlayers(const Vector3& position, const float radius, const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS], bool sortResults = true, bool alwaysGetPlayerPos = false);

	// builds an array of players within the given position and radius and can be sorted by distance, returns the number of players added to the array
	u32 GetClosestRemotePlayersInScope(const CNetObjProximityMigrateable& networkObject, const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS], bool sortResults = true);

	// gets the distance from our player to another players player
    float GetDistanceToPlayer(const CNetGamePlayer* pRemotePlayer);

    // returns whether the player name should be rendered for the specified player ped
    bool CanDisplayPlayerName(CPed *playerPed);

    // helper function for calculating screen coordinates for the network OHD
	bool GetScreenCoordinatesForOHD(const Vector3& position, Vector2 &vScreenPos, float &scale, const Vector3& offset = Vector3( 0, 0, 0.82f ), float displayRange = 100.0f, float nearScalingDistance = 0.0f, float farScalingDistance = 100.0f, float maxScale = 1.0f, float minScale = 0.2f );

    // helper function for setting the font settings for the network OHD
    void SetFontSettingsForNetworkOHD(CTextLayout *pTextLayout, float scale, bool bSmallerVersion);

	// calculates which ped is responsible for the damage dealt by the specified weapon damage entity to the specified entity.
	CPed* GetPedFromDamageEntity(CEntity *entity, CEntity* pWeaponDamageEntity);

    // calculates which player is responsible for the damage dealt by the specified weapon damage entity to the specified entity
    CNetGamePlayer* GetPlayerFromDamageEntity(CEntity *entity, CEntity *pWeaponDamageEntity);

	// calculates the Damage entity of a Player.
	CEntity* GetPlayerDamageEntity(const CNetGamePlayer* player, int& weaponHash);

    // calculates a debug colour to use for the specified player
    NetworkColours::NetworkColour GetDebugColourForPlayer(const netPlayer *player);

#if ENABLE_NETWORK_LOGGING
	const char *GetCanCloneErrorString             (unsigned resultCode);
    const char *GetCanPassControlErrorString       (unsigned resultCode);
    const char *GetCanAcceptControlErrorString     (unsigned resultCode);
    const char *GetCanBlendErrorString             (unsigned resultCode);
    const char *GetNetworkBlenderOverriddenReason  (unsigned resultCode);
	const char *GetCanProcessAttachmentErrorString (unsigned resultCode);
	const char *GetAttemptPendingAttachmentFailReason(unsigned resultCode);
	const char *GetFixedByNetworkReason		       (unsigned resultCode);
	const char *GetNpfbnNetworkReason				(unsigned resultCode);
	const char *GetScopeReason					   (unsigned resultCode);
    const char *GetMigrationFailTrackingErrorString(unsigned resultCode);
    const char *GetPlayerFocus                      (unsigned resultCode);
#else
    inline const char *GetCanCloneErrorString             (unsigned) { return ""; }
	inline const char *GetCanPassControlErrorString       (unsigned) { return ""; }
	inline const char *GetCanAcceptControlErrorString     (unsigned) { return ""; }
	inline const char *GetCanBlendErrorString             (unsigned) { return ""; }
	inline const char *GetNetworkBlenderOverriddenReason  (unsigned) { return ""; }
	inline const char *GetCanProcessAttachmentErrorString (unsigned) { return ""; }
	inline const char *GetAttemptPendingAttachmentFailReason(unsigned){ return ""; }
	inline const char *GetFixedByNetworkReason		      (unsigned) { return ""; }
	inline const char *GetNpfbnNetworkReason		      (unsigned) { return ""; }
	inline const char *GetScopeReason					  (unsigned) { return ""; }
	inline const char *GetMigrationFailTrackingErrorString(unsigned) { return ""; }
    inline const char *GetPlayerFocus                     (unsigned) { return ""; }
#endif
	// returns the current language represented by a two character code: en,fr,de,it,jp,es
	const char* GetLanguageCode();

	bool IsLocalPlayersTurn(Vec3V_In vPosition, float fMaxDistance, float fDurationOfTurns, float fTimeBetweenTurns);

	s16			GetSpecialAlphaRampingValue(s16& currAlpha, bool& bAlphaIncreasing, s16 totalTime, s16 timeRemaining, bool bRampOut);
	const s16	GetSpecialAlphaRampingFadeInTime();
	const s16	GetSpecialAlphaRampingFadeOutTime();

	// used by code bail warning screens
	const char* GetBailReasonAsString(eBailReason nBailReason);

	// used to build the tunables
	const char* GetSentFromGroupStub(const unsigned sentFromGroupBit);

	// session utility
#if !__NO_OUTPUT
    const char* GetChannelAsString(const unsigned nChannelId);
	const char* GetSessionTypeAsString(const SessionType sessionType);
    const char* GetSessionPurposeAsString(const SessionPurpose sessionPurpose);
	const char* GetResponseCodeAsString(eJoinResponseCode nResponseCode);
	const char* GetSessionResponseAsString(snJoinResponseCode nResponseCode);
	const char* GetPoolAsString(eMultiplayerPool nPool);
	const char* GetTransitionBailReasonAsString(eTransitionBailReason nBailReason);
	const char* GetBailErrorCodeAsString(const int errorCode);
	const char* GetMultiplayerGameModeAsString(const u16 nGameMode);
	const char* GetMultiplayerGameModeIntAsString(const int nGameMode);
	const char* GetMultiplayerGameModeStateAsString(const int nGameModeState);

	// utility function to log a gamer handle
	const char* LogGamerHandle(const rlGamerHandle& hGamer);
#endif

#if __BANK
	const char* GetSetGhostingReason(unsigned reason);
#endif
};

#endif  // NETWORK_UTIL_H
