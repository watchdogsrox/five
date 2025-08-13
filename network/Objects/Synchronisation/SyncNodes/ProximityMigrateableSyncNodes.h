//
// name:        ProximityMigrateableSyncNodes.h
// description: Network sync nodes used by CNetObjProximityMigrateables
// written by:    John Gurney
//

#ifndef PROXIMITY_MIGRATEABLE_SYNC_NODES_H
#define PROXIMITY_MIGRATEABLE_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/BaseSyncNodes.h"

///////////////////////////////////////////////////////////////////////////////////////
// IProximityMigrateableNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the entity nodes.
// Any class that wishes to send/receive data contained in the entity nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IProximityMigrateableNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IProximityMigrateableNodeDataAccessor);

    // getter functions
    virtual NetObjFlags GetGlobalFlags() const = 0;
    virtual u32 GetOwnershipToken() const = 0;
    virtual const ObjectId GetObjectID() const = 0;
    virtual void   GetSector(u16 &sectorX, u16 &sectorY, u16 &sectorZ) = 0;
    virtual void   GetSectorPosition(float &sectorPosX, float &sectorPosY, float &sectorPosZ) const = 0;
	virtual void   GetMigrationData(PlayerFlags &clonedState, DataNodeFlags &unsyncedNodes, DataNodeFlags &clonedPlayersThatLeft) = 0;

    // setter functions
    virtual void SetGlobalFlags(NetObjFlags globalFlags) = 0;
    virtual void SetOwnershipToken(u32 ownershipToken) = 0;
    virtual void SetSector(const u16 sectorX, const u16 sectorY, const u16 sectorZ) = 0;
    virtual void SetSectorPosition(const float sectorPosX, const float sectorPosY, const float sectorPosZ) = 0;
	virtual void SetMigrationData(PlayerFlags clonedState, DataNodeFlags clonedPlayersThatLeft) = 0;

protected:

    virtual ~IProximityMigrateableNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CGlobalFlagsDataNode
//
// Handles the serialization of an object's global flags
///////////////////////////////////////////////////////////////////////////////////////
class CGlobalFlagsDataNode : public CSyncDataNodeInfrequent
{
public:

	static const unsigned int SIZEOF_OWNERSHIP_TOKEN = 5;

	u32 GetOwnershipToken()       const { return m_ownershipToken; }
	u32 GetSizeOfOwnershipToken() const { return SIZEOF_OWNERSHIP_TOKEN; }

	virtual bool IsAlwaysSentWithMigrate() const { return true; }

protected:

	NODE_COMMON_IMPL("Global Flags", CGlobalFlagsDataNode, IProximityMigrateableNodeDataAccessor);

	bool CanSendWithNoGameObject() const { return true; }

	bool IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const;

private:

    void ExtractDataForSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        m_globalFlags    = nodeData.GetGlobalFlags();
        m_ownershipToken = nodeData.GetOwnershipToken();
    }

    void ApplyDataFromSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.SetGlobalFlags((NetObjFlags)m_globalFlags);
        nodeData.SetOwnershipToken(m_ownershipToken);
    }

	void Serialise(CSyncDataBase& serialiser)
    {
		SERIALISE_GLOBAL_FLAGS(serialiser, m_globalFlags, "Global Flags");
        SERIALISE_UNSIGNED(serialiser, m_ownershipToken, SIZEOF_OWNERSHIP_TOKEN, "Ownership Token");
    }

private:

	static const unsigned int SIZEOF_GLOBALFLAGS     = SIZEOF_NETOBJ_GLOBALFLAGS;
 
    u32 m_globalFlags;    // network object global flags
    u32 m_ownershipToken; // current ownership token
};

///////////////////////////////////////////////////////////////////////////////////////
// CSectorDataNode
//
// Handles the serialization of an object's sector.
///////////////////////////////////////////////////////////////////////////////////////
class CSectorDataNode : public CSyncDataNodeFrequent
{
public:

    static const unsigned int SIZEOF_SECTOR_X = 10;
    static const unsigned int SIZEOF_SECTOR_Y = 10;
    static const unsigned int SIZEOF_SECTOR_Z = 6;

protected:

	NODE_COMMON_IMPL("Sector", CSectorDataNode, IProximityMigrateableNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

    bool IsAlwaysSentWithCreate() const { return true; }
    bool IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const;

private:

   void ExtractDataForSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.GetSector(m_sectorX, m_sectorY, m_sectorZ);
    }

    void ApplyDataFromSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.SetSector(m_sectorX, m_sectorY, m_sectorZ);
    }

	void Serialise(CSyncDataBase& serialiser)
    {
        SERIALISE_UNSIGNED(serialiser, m_sectorX, SIZEOF_SECTOR_X, "Sector X");
        SERIALISE_UNSIGNED(serialiser, m_sectorY, SIZEOF_SECTOR_Y, "Sector Y");
        SERIALISE_UNSIGNED(serialiser, m_sectorZ, SIZEOF_SECTOR_Z, "Sector Z");
    }

private:

    u16 m_sectorX; // X sector this object is currently within
    u16 m_sectorY; // Y sector this object is currently within
    u16 m_sectorZ; // Z sector this object is currently within
};

///////////////////////////////////////////////////////////////////////////////////////
// CSectorPositionDataNode
//
// Handles the serialization of an object's sector position
///////////////////////////////////////////////////////////////////////////////////////
#define WORLD_WIDTHOFSECTOR_NETWORK		54.0f
#define WORLD_DEPTHOFSECTOR_NETWORK		54.0f
#define WORLD_HEIGHTOFSECTOR_NETWORK	69.0f

class CSectorPositionDataNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Sector Position", CSectorPositionDataNode, IProximityMigrateableNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

	bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.GetSectorPosition(m_sectorPosX, m_sectorPosY, m_sectorPosZ);
    }

    void ApplyDataFromSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.SetSectorPosition(m_sectorPosX, m_sectorPosY, m_sectorPosZ);
    }

protected:

	void Serialise(CSyncDataBase& serialiser)
    {
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_sectorPosX, (float)WORLD_WIDTHOFSECTOR_NETWORK, SIZEOF_SECTORPOS,  "Sector Pos X");
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_sectorPosY, (float)WORLD_DEPTHOFSECTOR_NETWORK, SIZEOF_SECTORPOS,  "Sector Pos Y");
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_sectorPosZ, (float)WORLD_HEIGHTOFSECTOR_NETWORK, SIZEOF_SECTORPOS, "Sector Pos Z");
    }

public:

    static const unsigned int SIZEOF_SECTORPOS = 12;

#if __DEV
	static RecorderId ms_bandwidthRecorderId;
#endif

    float m_sectorPosX; // X position of this object within the current sector
    float m_sectorPosY; // Y position of this object within the current sector
    float m_sectorPosZ; // Z position of this object within the current sector
};

///////////////////////////////////////////////////////////////////////////////////////
// CMigrationDataNode
//
// Handles the serialization of an object's migration data
///////////////////////////////////////////////////////////////////////////////////////
class CMigrationDataNode : public CProjectBaseSyncDataNode
{
protected:

	NODE_COMMON_IMPL("Entity Migration", CMigrationDataNode, IProximityMigrateableNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

public:

	DataNodeFlags GetUnsyncedNodes() const { return m_unsyncedNodes; }

private:

    void ExtractDataForSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.GetMigrationData(m_clonedState, m_unsyncedNodes, m_clonedPlayersThatLeft);
    }

    void ApplyDataFromSerialising(IProximityMigrateableNodeDataAccessor &nodeData)
    {
        nodeData.SetMigrationData(m_clonedState, m_clonedPlayersThatLeft);
	}

	void Serialise(CSyncDataBase& serialiser)
    {
		SERIALISE_BITFIELD(serialiser, m_clonedState, SIZEOF_CLONE_STATE, "Cloned State");

		bool bHasUnsyncedState = m_unsyncedNodes != 0;

		SERIALISE_BOOL(serialiser, bHasUnsyncedState);

		if (bHasUnsyncedState || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BITFIELD(serialiser, m_unsyncedNodes, SIZEOF_UNSYNCED_NODES, "Unsynced Nodes");
		}
		else
		{
			m_unsyncedNodes = 0;
		}

		bool bHasLeftState = m_clonedPlayersThatLeft != 0;

		SERIALISE_BOOL(serialiser, bHasLeftState);

		if (bHasLeftState || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BITFIELD(serialiser, m_clonedPlayersThatLeft, SIZEOF_CLONE_STATE, "Cloned players that left");
		}
		else
		{
			m_clonedPlayersThatLeft = 0;
		}
    }

private:

	static const unsigned int SIZEOF_CLONE_STATE	= MAX_NUM_PHYSICAL_PLAYERS;
	static const unsigned int SIZEOF_UNSYNCED_NODES	= sizeof(DataNodeFlags)<<3;

	PlayerFlags		m_clonedState;				// bit flags indicating which players the object is cloned on
	PlayerFlags		m_clonedPlayersThatLeft;	// bit flags indicating which players left while the object was cloned on their machine when local
	DataNodeFlags	m_unsyncedNodes;			// bit flags indicating which nodes are unsynced with any other player 
};

#endif  // PROXIMITY_MIGRATEABLE_SYNC_NODES_H
