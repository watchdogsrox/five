//
// name:        PhysicalSyncNodes.h
// description: Network sync nodes used by CNetObjPhysicals
// written by:    John Gurney
//

#ifndef PHYSICAL_SYNC_NODES_H
#define PHYSICAL_SYNC_NODES_H

#include "network/General/NetworkUtil.h"
#include "network/objects/synchronisation/SyncNodes/DynamicEntitySyncNodes.h"

#include "vector/Quaternion.h"

class CPhysicalGameStateDataNode;
class CPhysicalScriptGameStateDataNode;
class CPhysicalAngVelocityDataNode;
class CPhysicalHealthDataNode;
class CPhysicalMigrationDataNode;
class CPhysicalScriptMigrationDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IPhysicalNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the physical nodes.
// Any class that wishes to send/receive data contained in the physical nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IPhysicalNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IPhysicalNodeDataAccessor);

    struct ScriptPhysicalFlags
    {
        bool notDamagedByAnything;
		bool dontLoadCollision;
        bool allowFreezeIfNoCollision;
		bool damagedOnlyByPlayer;
        bool notDamagedByBullets;
        bool notDamagedByFlames;
        bool ignoresExplosions;
        bool notDamagedByCollisions;
        bool notDamagedByMelee;
		bool notDamagedByRelGroup;
		bool onlyDamageByRelGroup;
        bool notDamagedBySmoke;
        bool notDamagedBySteam;
		bool onlyDamagedWhenRunningScript;
		bool dontResetDamageFlagsOnCleanupMissionState;
        bool noReassign;
		bool passControlInTutorial;
		bool inCutscene;
		bool ghostedForGhostPlayers;
		bool pickUpByCargobobDisabled;
    };

	enum eAlphaType
	{
		ART_NONE,
		ART_FADE_OUT,
		ART_FADE_IN,
		ART_RAMP_CONTINUOUS,
		ART_RAMP_IN_AND_WAIT,
		ART_RAMP_IN_AND_QUIT,
		ART_RAMP_OUT,
		ART_NUM_TYPES
	};

    // getter functions
    virtual void GetPhysicalGameStateData(CPhysicalGameStateDataNode& data) = 0;

	virtual void GetPhysicalScriptGameStateData(CPhysicalScriptGameStateDataNode& data) = 0;

    virtual void GetVelocity(s32 &packedVelocityX,
                             s32 &packedVelocityY,
                             s32 &packedVelocityZ) = 0;

    virtual void GetAngularVelocity(CPhysicalAngVelocityDataNode& data) = 0;

    virtual void GetPhysicalHealthData(CPhysicalHealthDataNode& data) = 0;

    virtual void GetPhysicalAttachmentData(bool       &attached,
                                           ObjectId   &attachedObjectID,
                                           Vector3    &attachmentOffset,
                                           Quaternion &attachmentQuat,
										   Vector3    &attachmentParentOffset,
                                           s16        &attachmentOtherBone,
                                           s16        &attachmentMyBone,
                                           u32        &attachmentFlags,
                                           bool       &allowInitialSeparation,
                                           float      &invMassScaleA,
                                           float      &invMassScaleB,
										   bool		  &activatePhysicsWhenDetached,
                                           bool       &isCargoVehicle) const = 0;

	virtual void GetPhysicalMigrationData(CPhysicalMigrationDataNode& data) = 0;

	virtual void GetPhysicalScriptMigrationData(CPhysicalScriptMigrationDataNode& data) = 0;

	// setter functions
    virtual void SetPhysicalGameStateData(const CPhysicalGameStateDataNode& data) = 0;

	virtual void SetPhysicalScriptGameStateData(const CPhysicalScriptGameStateDataNode& data) = 0;

	virtual void SetVelocity(const s32 packedVelocityX,
                             const s32 packedVelocityY,
                             const s32 packedVelocityZ) = 0;

    virtual void SetAngularVelocity(const CPhysicalAngVelocityDataNode& data) = 0;

    virtual void SetPhysicalHealthData(const CPhysicalHealthDataNode& data) = 0;

    virtual void SetPhysicalAttachmentData(const bool             attached,
                                           const ObjectId         attachedObjectID,
                                           const Vector3         &attachmentOffset,
                                           const Quaternion      &attachmentQuat,
										   const Vector3         &attachmentParentOffset,
                                           const s16              attachmentOtherBone,
                                           const s16              attachmentMyBone,
                                           const u32              attachmentFlags,
                                           const bool             allowInitialSeparation,
                                           const float            invMassScaleA,
                                           const float            invMassScaleB,
										   const bool			  activatePhysicsWhenDetached,
                                           const bool             isCargoVehicle) = 0;

    virtual void SetPhysicalMigrationData(const CPhysicalMigrationDataNode& data) = 0;

	virtual void SetPhysicalScriptMigrationData(const CPhysicalScriptMigrationDataNode& data) = 0;

protected:

    virtual ~IPhysicalNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalGameStateDataNode
//
// Handles the serialization of an physical's game state.
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Physical Game State", CPhysicalGameStateDataNode, IPhysicalNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

	bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.GetPhysicalGameStateData(*this);
    }

    void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.SetPhysicalGameStateData(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	bool m_isVisible;           // gamestate flag indicating whether the object is visible
    bool m_renderScorched;      // render scorched game state flag
	bool m_isInWater;           // is in water game state flag
	bool m_alteringAlpha;		// the entity is fading out / alpha ramping
	bool m_fadingOut;           // the entity is fading out
	u32  m_alphaType;		// the type of alpha ramp the entity is doing
	s16  m_customFadeDuration;	// A custom max duration for fading
	bool m_allowCloningWhileInTutorial; // if set, the entity won't be stopped from cloning for players who are not in a tutorial
};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalScriptGameStateDataNode
//
// Handles the serialization of an physical's script game state.
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalScriptGameStateDataNode : public CSyncDataNodeInfrequent
{
public:

	static const float MAX_MAX_SPEED;

protected:

	NODE_COMMON_IMPL("Physical Script Game State", CPhysicalScriptGameStateDataNode, IPhysicalNodeDataAccessor);

private:

	void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
	{
		physicalNodeData.GetPhysicalScriptGameStateData(*this);
	}

	void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
	{
		physicalNodeData.SetPhysicalScriptGameStateData(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

	virtual bool ResetScriptStateInternal();

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;

	virtual void SetDefaultState();

public:

    IPhysicalNodeDataAccessor::ScriptPhysicalFlags m_PhysicalFlags;
	u32                                            m_RelGroupHash;
    u32                                            m_AlwaysClonedForPlayer;
	bool                                           m_HasMaxSpeed;
	bool										   m_AllowMigrateToSpectator;
    float                                          m_MaxSpeed;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalVelocityDataNode
//
// Handles the serialization of a physical's velocity.
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalVelocityDataNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Physical Velocity", CPhysicalVelocityDataNode, IPhysicalNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		if (GetIsInCutscene(pObj))
		{
			return ~0u;
		}

		return GetIsAttachedForPlayers(pObj, false);
	}

private:


    void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.GetVelocity(m_packedVelocityX, m_packedVelocityY, m_packedVelocityZ);
    }

    void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.SetVelocity(m_packedVelocityX, m_packedVelocityY, m_packedVelocityZ);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    s32 m_packedVelocityX; // current velocity X (packed)
    s32 m_packedVelocityY; // current velocity Y (packed)
    s32 m_packedVelocityZ; // current velocity Z (packed)

};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalAngVelocityDataNode
//
// Handles the serialization of a physical's angular velocity.
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalAngVelocityDataNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Physical AngVelocity", CPhysicalAngVelocityDataNode, IPhysicalNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		if (GetIsInCutscene(pObj))
		{
			return ~0u;
		}

		return GetIsAttachedForPlayers(pObj, false);
	}

private:

    void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.GetAngularVelocity(*this);
    }

    void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.SetAngularVelocity(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    s32 m_packedAngVelocityX; // current angular velocity X (packed)
    s32 m_packedAngVelocityY; // current angular velocity Y (packed)
    s32 m_packedAngVelocityZ; // current angular velocity Z (packed)
};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalHealthDataNode
//
// Handles the serialization of a physical's health.
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalHealthDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Physical Health", CPhysicalHealthDataNode, IPhysicalNodeDataAccessor);

public:

    void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.GetPhysicalHealthData(*this);
    }

    void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.SetPhysicalHealthData(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

public:

	bool	 m_hasMaxHealth;			// health is max
	bool	 m_maxHealthSetByScript;	// set when script alters max health
	u32      m_scriptMaxHealth;			// the script max health
	u32      m_health;					// health
	ObjectId m_weaponDamageEntity;		// weapon damage entity (only for script objects)
	u32      m_weaponDamageHash;		// weapon damage Hash 
	u64		 m_lastDamagedMaterialId;	// last material id that was damaged for vehicle
};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalAttachDataNode
//
// Handles the serialization of a physical's attachment info.
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalAttachDataNode : public CSyncDataNodeInfrequent
{
public:

	CPhysicalAttachDataNode() : m_syncPhysicsActivation(false) {}

	void SyncPhysicsActivation() { m_syncPhysicsActivation = true; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

protected:

	NODE_COMMON_IMPL("Physical Attach", CPhysicalAttachDataNode, IPhysicalNodeDataAccessor);

private:

    void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData);

    void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData);

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    bool       m_attached;						// is this object attached?
    ObjectId   m_attachedObjectID;				// object ID of the object attached to
    Vector3    m_attachmentOffset;				// attachment offset
    Quaternion m_attachmentQuat;				// attachment quaternion
	Vector3    m_attachmentParentOffset;		// attachment parent offset
    s16        m_attachmentOtherBone;			// attachment bone
    s16        m_attachmentMyBone;				// attachment bone
    u32        m_attachmentFlags;				// attachment flags
    bool       m_allowInitialSeparation;		// allowed initial separation
    float      m_InvMassScaleA;					// inv mass scale A
    float      m_InvMassScaleB;					// inv mass scale B
	bool	   m_activatePhysicsWhenDetached;	// activates the physics on the object when it is detached
	bool	   m_syncPhysicsActivation;			// if set m_activatePhysicsWhenDetached is synced
    bool       m_IsCargoVehicle;                // is the vehicle attached as a cargo vehicle
};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalMigrationDataNode
//
// Handles the serialization of a physical's migration data
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalMigrationDataNode : public CProjectBaseSyncDataNode
{
protected:

	NODE_COMMON_IMPL("Physical Migration", CPhysicalMigrationDataNode, IPhysicalNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

private:

    void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.GetPhysicalMigrationData(*this);
    }

    void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.SetPhysicalMigrationData(*this);

	}

	void Serialise(CSyncDataBase& serialiser);

public:

    bool       m_isDead;              // does this object have zero health?

};

///////////////////////////////////////////////////////////////////////////////////////
// CPhysicalScriptMigrationDataNode
//
// Handles the serialization of a physical's script migration data
///////////////////////////////////////////////////////////////////////////////////////
class CPhysicalScriptMigrationDataNode : public CProjectBaseSyncDataNode
{
protected:

	NODE_COMMON_IMPL("Physical Script Migration", CPhysicalScriptMigrationDataNode, IPhysicalNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

private:

	void ExtractDataForSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
	{
		physicalNodeData.GetPhysicalScriptMigrationData(*this);
	}

	void ApplyDataFromSerialising(IPhysicalNodeDataAccessor &physicalNodeData)
	{
		physicalNodeData.SetPhysicalScriptMigrationData(*this);

	}

	void Serialise(CSyncDataBase& serialiser);

public:

	bool			m_HasData;				// this can be false for entities that used to be script entities
	PlayerFlags		m_ScriptParticipants;	// the players participating in the script the object belongs to
	HostToken		m_HostToken;			// the host token used by the current host of the script
};

#endif  // PHYSICAL_SYNC_NODES_H
