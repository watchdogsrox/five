//
// name:		NetObjGame.h
// description:	Project specific network object functionality
// written by:	Daniel Yelland
//

#ifndef NETOBJGAMEOBJECT_H
#define NETOBJGAMEOBJECT_H

#include "fwnet/netobject.h"
#include "network/objects/NetworkObjectTypes.h"

//------------------------------------------------------------------------------------
//Dead strip out the ValidateGameObject calls in non-assert builds (optimization pass)
#if __ASSERT
#define VALIDATE_GAME_OBJECT(object)	ValidateGameObject(object);
#else
#define VALIDATE_GAME_OBJECT(...)
#endif
//------------------------------------------------------------------------------------

namespace rage
{
	class netScriptObjInfoBase;
	class scrThread;
}

class CNetGamePlayer;
class CScriptEntityExtension;

class CNetObjGame : public netObject
{
public:

    // global flags used by network objects
	static const unsigned int GLOBALFLAG_SCRIPTOBJECT        = BIT(GLOBALFLAG_USER);	// set if this object is a script object - these objects require extra sync information
	static const unsigned int GLOBALFLAG_CLONEALWAYS_SCRIPT	 = BIT(GLOBALFLAG_USER+1);  // set if this object is always cloned on machines running the same script that created the object
	static const unsigned int GLOBALFLAG_CLONEONLY_SCRIPT	 = BIT(GLOBALFLAG_USER+2);  // set if this object is only cloned on machines running the same script that created the object
	static const unsigned int GLOBALFLAG_SCRIPT_MIGRATION	 = BIT(GLOBALFLAG_USER+3);  // set if this object is only to migrate to machines running the same script that created the object
	static const unsigned int GLOBALFLAG_WAS_SCRIPTOBJECT	 = BIT(GLOBALFLAG_USER+4);  // set if this object used to be a script object
	static const unsigned int GLOBALFLAG_RESERVEDOBJECT		 = BIT(GLOBALFLAG_USER+5);  // set if this object was created as part of an external population reservation
	// don't forget to update SIZEOF_NETOBJ_GLOBALFLAGS if you add a new bit flag!

	static const unsigned int LOCALFLAG_NOLONGERNEEDED              = BIT(LOCALFLAG_USER);	// set if this object is a script object and is no longer needed by the script
	static const unsigned int LOCALFLAG_SHOWINCUTSCENE		        = BIT(LOCALFLAG_USER+1);	// this object has to remain locally visible in a MP cutscene
	static const unsigned int LOCALFLAG_RESPAWNPED   		        = BIT(LOCALFLAG_USER+2);	// set if this object will be used for player ped swap, we cant accept ownership while this is set. After the change is done this FLAG is RESET
	static const unsigned int LOCALFLAG_FORCE_SYNCHRONISE	        = BIT(LOCALFLAG_USER+3);	// set if the object should be forced to have sync data
	static const unsigned int LOCALFLAG_TELEPORT                    = BIT(LOCALFLAG_USER+4);	// set if the object is being teleported and ownership can not change
    static const unsigned int LOCALFLAG_DISABLE_BLENDING            = BIT(LOCALFLAG_USER+5);	// set if the object is has been flagged for disabling network blending
	static const unsigned int LOCALFLAG_REMOVE_POST_TUTORIAL_CHANGE = BIT(LOCALFLAG_USER+6);	// set if the object has been flagged for removal once a tutorial session state change has finished
	static const unsigned int LOCALFLAG_OWNERSHIP_CONFLICT			= BIT(LOCALFLAG_USER+7);	// set if the object is being unregistered due to an ownership conflict
	static const unsigned int LOCALFLAG_ENTITY_FIXED                = BIT(LOCALFLAG_USER+8);	// set if the object is being fixed so that it must be moved Straight to target.
	static const unsigned int LOCALFLAG_HAS_MIGRATED                = BIT(LOCALFLAG_USER+9);	// set if the object has migrated locally
	static const unsigned int LOCALFLAG_COUNTED_BY_RESERVATIONS     = BIT(LOCALFLAG_USER+10);	// set if the object has been accounted for by the script reservation system
	static const unsigned int LOCALFLAG_DISABLE_PROXIMITY_MIGRATION = BIT(LOCALFLAG_USER+11);	// set if the object cannot migrate by proximity
	
	template< class T >
	CNetObjGame(T* pGameObject,
				const NetworkObjectType type,
				const ObjectId objectID,
				const PhysicalPlayerIndex playerIndex,
				const NetObjFlags localFlags,
				const NetObjFlags globalFlags)
	: netObject(pGameObject, type, objectID, playerIndex, localFlags, globalFlags)
	, m_dirtyNodes(0)
    , m_AlwaysClonedForPlayers(0)
	{
	}

	CNetObjGame()
	{
	}

    virtual ActivationFlags GetActivationFlags() const;

    CNetGamePlayer* GetPlayerOwner() const;

	virtual bool IsScriptObject(bool bIncludePlayer = false) const;
	bool IsOrWasScriptObject(bool bIncludePlayer = false) const;

	void SetAlwaysClonedForPlayer(PhysicalPlayerIndex playerIndex, bool alwaysCloned);

	virtual void Init(const unsigned numPlayers);
	virtual void SetGameObject(void* gameObject);
	virtual bool CanClone(const netPlayer& player, unsigned *resultCode = 0) const;
	virtual bool CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual bool CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
	virtual void ChangeOwner(const netPlayer& player, eMigrationType migrationType);
	virtual void PostMigrate(eMigrationType migrationType);
	virtual void PlayerHasLeft(const netPlayer& player);
	virtual bool OnReassigned();
	virtual void StartSynchronising();
	virtual void PrepareForScriptTermination() {};

	virtual bool NeedsPlayerTeleportControl() const { return false; }

	virtual void ConvertToAmbientObject() { OnConversionToAmbientObject(); }

	virtual void OnConversionToScriptObject();
	virtual void OnConversionToAmbientObject();

	virtual void  ValidateGameObject(void* ASSERT_ONLY(p)) const { ASSERT_ONLY(gnetAssertf(p || IsLocalFlagSet(LOCALFLAG_RESPAWNPED) || IsLocalFlagSet(LOCALFLAG_UNREGISTERING), "Network object id=\"%d\" doesnt have a game object.", GetObjectID());) }

	void SetDirtyNodes(DataNodeFlags& nodeFlags) { m_dirtyNodes |= nodeFlags; }

	// returns true if this is a script object and we are a participant of the script that created it
	bool IsLocallyRunningScriptObject() const;

	virtual bool CanPassControlWithNoSyncData() const { return true; }
	virtual bool CanInitialiseInitialStateBuffer() const { return true; }

	// usually broadcast data is held back waiting for script objects to clone. For some objects we want to avoid this.
	virtual bool RestrictsBroadcastDataWhenUncloned() const { return true; }

#if __ASSERT
	virtual void ValidateMigration() const {}
#endif

#if ENABLE_NETWORK_LOGGING
	void LogScopeReason(bool inScope, const netPlayer& player, unsigned reason);
#endif
    static const char *GetGlobalFlagName(unsigned flag);

#if ENABLE_NETWORK_LOGGING
    //PURPOSE
    // Returns a debug string describing the reason an object cannot be cloned to a remote machine
    //PARAMS
    // resultCode - The result code to return the debug string for
    virtual const char *GetCanCloneErrorString(unsigned resultCode);

	virtual const char *GetCannotDeleteReason(unsigned reason);
#endif // ENABLE_NETWORK_LOGGING

protected:

    bool AlwaysClonedForPlayer(PhysicalPlayerIndex playerIndex) const;

    PlayerFlags GetAlwaysClonedForPlayerFlags() const { return m_AlwaysClonedForPlayers; }
    void        SetAlwaysClonedForPlayerFlags(const PlayerFlags alwaysClonedForPlayers) { m_AlwaysClonedForPlayers = alwaysClonedForPlayers; }

	// Used by clones to flag which sync nodes we have received updates for
	DataNodeFlags m_dirtyNodes;

	virtual bool GetAllowMigrateToSpectator() const { return false; }

private:

    PlayerFlags m_AlwaysClonedForPlayers; // Bitfield indicating whether this object should always be cloned on a player's machine
};

#endif  // NETOBJGAMEOBJECT_H
