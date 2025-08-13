//
// name:        BaseSyncNodes.h
// description: Project base network sync node classes
// written by:    John Gurney
//

#ifndef BASE_SYNC_NODES_H
#define BASE_SYNC_NODES_H

NETWORK_OPTIMISATIONS()

#include "fwnet/netdisplay.h"
#include "fwnet/netlog.h"
#include "fwnet/netserialisers.h"
#include "fwnet/netsyncdataul.h"
#include "fwnet/netsyncnode.h"
#include "fwnet/netsynctree.h"

#include "network/NetworkInterface.h"
#include "network/general/NetworkSerialisers.h"

#define USE_DEFAULT_STATE 0

extern void WriteNodeHeader(netLoggingInterface &log, const char *nodeName);


///////////////////////////////////////////////////////////////////////////////////////
// CNodeSerialiserBase
//
// Base class for all node serialisers.
//
///////////////////////////////////////////////////////////////////////////////////////
class CNodeSerialiserBase
{
public:

	CNodeSerialiserBase(netLoggingInterface* pLog) :
	m_pLog(pLog)
	{
	}

protected:

    virtual ~CNodeSerialiserBase() {}

	netLoggingInterface* m_pLog;
};

///////////////////////////////////////////////////////////////////////////////////////
// CProjectBaseSyncParentNode
//
// Base class for all parent nodes used in this project. Contains a node name for
// debugging
///////////////////////////////////////////////////////////////////////////////////////
class CProjectBaseSyncParentNode : public netSyncParentNode
{
public:

	CProjectBaseSyncParentNode(const char* OUTPUT_ONLY(nodeName))
	{
#if !__NO_OUTPUT
		SetNodeName(nodeName);
#endif
	}

	//PURPOSE
	//	Writes the node data to a bit buffer
	virtual bool Write(SerialiseModeFlags serMode, ActivationFlags actFlags, netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, 
						const unsigned currTime, netLoggingInterface* pLog, const PhysicalPlayerIndex player, DataNodeFlags* pNodeFlags, unsigned &maxNumHeaderBitsRemaining);

	//PURPOSE
	//	Reads the node data from a bit buffer
	virtual bool Read(SerialiseModeFlags serMode, ActivationFlags actFlags, datBitBuffer& bitBuffer, netLoggingInterface* pLog);

	//PURPOSE
	// Used by nodes associated with script data, resets the data when it is detected that the object using the node tree has reverted to an ambient object.
	void ResetScriptState();

private:

#if __BANK
    //PURPOSE
	//	Returns the ID of the bandwidth recorder for tracking how much guard flag data is sent/received
    static RecorderId GetGuardFlagBandwidthRecorderID();
#endif
};

///////////////////////////////////////////////////////////////////////////////////////
// ICommonDataOperations
//
// Interface class for performing common data operations for a data node, including
// reading and writing the nodes data to a buffer and calculating the size of the data
///////////////////////////////////////////////////////////////////////////////////////
class ICommonDataOperations
{
protected:

    ICommonDataOperations(){;}
    virtual ~ICommonDataOperations(){;}


public:

    virtual void ReadData(datBitBuffer &bitBuffer, netLoggingInterface* pLog) = 0;
    virtual void WriteData(netSyncTreeTargetObject* pObj, datBitBuffer &bitBuffer, netLoggingInterface* pLog, bool extractFromObject = true) = 0;
	virtual void ApplyData(netSyncTreeTargetObject* pObj) = 0;
    virtual unsigned GetDataSize(netSyncTreeTargetObject &targetObject) const = 0;
	virtual u32      GetMaximumDataSize() = 0;
    virtual void LogData(netLoggingInterface& log) = 0;
	virtual void DisplayData(netSyncTreeTargetObject* pObj, netLogDisplay& displayLog) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////
// NodeCommonDataOperations
//
// This template class is used as a typesafe method of extracting, serialising and
// applying node data to/from a target object.
//
// Each data node implements a templated Serialise function for manipulating the node data,
// as well as methods for extracting/applying data to/from the node via a data accessor class
// that is specific to a group of nodes (e.g. nodes relating to syncing vehicle data)/
//
// The two template parameters are as follows:
//
// Node         - this is the type of the data node using the class (e.g. PedTaskDataNode)
// DataAccessor - this is the type of the data accessor used to extract/apply data from/to the node (e.g. IPedNodeDataAccessor)
//
// This main purpose of this class is to provide polymorphic behaviour to these functions, while
// avoiding making each individual function for serialising specific types virtual, which has performance implications.
//
///////////////////////////////////////////////////////////////////////////////////////
template <class Node, class DataAccessor> class NodeCommonDataOperations : public ICommonDataOperations
{
public:

    NodeCommonDataOperations() : m_node(0) {;}
    ~NodeCommonDataOperations() {;}

    void SetNode(Node &node) { m_node = &node; }

    void ReadData(datBitBuffer &bitBuffer, netLoggingInterface* pLog)
    {
        LOGGING_ONLY(if (pLog) WriteNodeHeader(*pLog, m_node->GetNodeName()));
        
		CSyncDataReader serialiser(bitBuffer, pLog);
		bool bDefaultState = false;

		if (USE_DEFAULT_STATE && m_node->CanHaveDefaultState())
		{
			SERIALISE_BOOL(serialiser, bDefaultState);

			if (bDefaultState)
			{
				m_node->SetDefaultState();
			}
		}

		if (!bDefaultState)
		{
			m_node->Serialise(serialiser);
		}
		else if (pLog)
		{
			CSyncDataLogger defaultSerialiser(pLog);
			pLog->Log("\t\t\t\t***DEFAULT STATE***\r\n");
			m_node->Serialise(defaultSerialiser);
		}
    }

    void WriteData(netSyncTreeTargetObject* pObj, datBitBuffer &bitBuffer, netLoggingInterface* pLog, bool extractFromObject = true)
    {
        LOGGING_ONLY(if (pLog) WriteNodeHeader(*pLog, m_node->GetNodeName()));
  
		if (extractFromObject)
		{
			m_node->ExtractDataForSerialising(*static_cast<DataAccessor*>(pObj->GetDataAccessor(DataAccessor::DATA_ACCESSOR_ID())));
		}

		CSyncDataWriter serialiser(bitBuffer, pLog);
		bool bDefaultState = false;

		if (USE_DEFAULT_STATE && m_node->CanHaveDefaultState())
		{
			bDefaultState = m_node->HasDefaultState();
			SERIALISE_BOOL(serialiser, bDefaultState);
		}

		if (!bDefaultState)
		{
			m_node->Serialise(serialiser);
		}
		else if (pLog)
		{
			CSyncDataLogger defaultSerialiser(pLog);
			pLog->Log("\t\t\t\t***DEFAULT STATE***\r\n");
			m_node->Serialise(defaultSerialiser);
		}
	}

	void ApplyData(netSyncTreeTargetObject* pObj)
    {
 	    m_node->ApplyDataFromSerialising(*static_cast<DataAccessor*>(pObj->GetDataAccessor(DataAccessor::DATA_ACCESSOR_ID())));
    }

	unsigned GetDataSize(netSyncTreeTargetObject &targetObject) const
	{
		DataAccessor* pDataAccessor = static_cast<DataAccessor*>(targetObject.GetDataAccessor(DataAccessor::DATA_ACCESSOR_ID()));
		gnetFatalAssertf(pDataAccessor, "%s : No data accessor for node %s", targetObject.GetLogName(), m_node->GetNodeName());
		m_node->ExtractDataForSerialising(*pDataAccessor);

		CSyncDataSizeCalculator serialiser;
		serialiser.SetIsMaximumSizeSerialiser(false);
		m_node->Serialise(serialiser);
		u32 size = serialiser.GetSize();
		if (USE_DEFAULT_STATE && m_node->CanHaveDefaultState()) size++;
		return size;
	}

    u32 GetMaximumDataSize()
    {
        CSyncDataSizeCalculator serialiser;
        m_node->Serialise(serialiser);
        u32 size = serialiser.GetSize();
		if (USE_DEFAULT_STATE && m_node->CanHaveDefaultState()) size++;
		return size;
    }	

    void LogData(netLoggingInterface& log)
    {
        LOGGING_ONLY(WriteNodeHeader(log, m_node->GetNodeName()));
        CSyncDataLogger serialiser(&log);

		if (USE_DEFAULT_STATE && m_node->CanHaveDefaultState() && m_node->HasDefaultState())
		{
			log.Log("\t\t\t\t***DEFAULT STATE***\r\n");
		}

		m_node->Serialise(serialiser);
    }

	void DisplayData(netSyncTreeTargetObject* pObj, netLogDisplay& displayLog)
	{
		m_node->ExtractDataForSerialising(*static_cast<DataAccessor*>(pObj->GetDataAccessor(DataAccessor::DATA_ACCESSOR_ID())));
		CSyncDataLogger serialiser(&displayLog);

		m_node->Serialise(serialiser);
	}

private:

    Node *m_node; // the node we are performing the operations upon
};

///////////////////////////////////////////////////////////////////////////////////////
// CProjectBaseSyncDataNode
//
// Base class for all data nodes used in this project. Holds update level data and provides
// some member functions used by more than one derived class.
///////////////////////////////////////////////////////////////////////////////////////
class CProjectBaseSyncDataNode : public netSyncDataNode
{
public:

	// default update rates
	enum
	{
		UPDATE_RATE_NONE				=	0,
        //DAN TEMP - UPDATE_RATE_DEFAULT_VERY_HIGH should be 50, but we need extra logic to prevent large delays when the frame rate is low
        //           For example if the frame time is > 50ms it would be possible for a very high update to be delayed 100ms based on timing
		UPDATE_RATE_DEFAULT_VERY_HIGH	=	25,
		UPDATE_RATE_DEFAULT_HIGH		=	100,
		UPDATE_RATE_DEFAULT_MEDIUM		=	300,
		UPDATE_RATE_DEFAULT_LOW			=	400,
		UPDATE_RATE_DEFAULT_VERY_LOW	=	1000,
		UPDATE_RATE_RESEND_UNACKED		=	200,
        MAX_BASE_RESEND_FREQUENCY       =   1000
	};

public:

	CProjectBaseSyncDataNode()
		: m_dataOperations(NULL)
	{
#if __BANK
		m_ppNodeData = NULL;
		//m_bandwidthRecorderType = INT_MAX;
        m_bandwidthRecorderID = INVALID_RECORDER_ID;
#endif // __BANK
		InitUpdateRates();
	}

    //PURPOSE
    // Resets the base resend frequencies to the default value for all players
    static void ResetBaseResendFrequencies()
    {
        for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
        {
            ms_BaseResendFrequencyForPlayer[index] = UPDATE_RATE_RESEND_UNACKED;
        }
    }

    //PURPOSE
    // Returns the base resend frequency for the specified player
    //PARAMS
    // playerIndex - Index of the player to return the base resend interval for
    static u32 GetBaseResendFrequency(PhysicalPlayerIndex playerIndex)
    {
        if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Trying to get resend frequency for an invalid physical player Index!"))
        {
            return ms_BaseResendFrequencyForPlayer[playerIndex];
        }

        return UPDATE_RATE_RESEND_UNACKED;
    }

    //PURPOSE
    // Sets the resend frequency for the specified player - used to extend the resend interval
    // for a player on a high latency connection
    //PARAMS
    // playerIndex    - Index of the player to set the resend interval to
    // resendInterval - Resend interval
    static void SetBaseResendFrequency(PhysicalPlayerIndex playerIndex, u32 resendInterval)
    {
        if(gnetVerifyf(playerIndex    <  MAX_NUM_PHYSICAL_PLAYERS,   "Trying to set resend frequency for an invalid physical player Index!") &&
           gnetVerifyf(resendInterval >= UPDATE_RATE_RESEND_UNACKED, "The resend frequency should only be increased from it's base value!"))
        {
            ms_BaseResendFrequencyForPlayer[playerIndex] = resendInterval;
        }
    }

	//PURPOSE
	//	Writes the node data to a bit buffer 
	virtual bool Write(SerialiseModeFlags serMode, ActivationFlags actFlags, netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, const unsigned currTime, 
						netLoggingInterface* pLog, const PhysicalPlayerIndex player, DataNodeFlags* pNodeFlags, unsigned &maxNumHeaderBitsRemaining);

	//PURPOSE
	//	Reads the node data from a bit buffer 
	virtual bool Read(SerialiseModeFlags serMode, ActivationFlags actFlags, datBitBuffer& bitBuffer, netLoggingInterface* pLog);
 
	//PURPOSE
	//  Reads the data represented by this node from the given bit buffer.
	//PARAMS
	//	bitBuffer	- the bit buffer
	//  pLog		- if this is set the message buffer data is written to the log as it is read
	virtual void ReadData(datBitBuffer& bitBuffer, netLoggingInterface* pLog) { m_dataOperations->ReadData(bitBuffer, pLog); }

	//PURPOSE
	//  Writes the data represented by this node to the given bit buffer.
	//PARAMS
	//	pObj				- the object using the tree
	//  bitBuffer			- the bit buffer
	//  pLog				- if this is set the message buffer data is written to the log as it is written
	// extractFromObject	- if true, the data is extracted from the object before writing
	virtual void WriteData(netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, netLoggingInterface* pLog, bool extractFromObject = true);

	//PURPOSE
	//  Returns true if this node can have default state, if so only a bool is written if the state is default
	virtual bool CanHaveDefaultState() const { return false; }

	//PURPOSE
	//  Returns true if this node has default state, if so only a bool is written if the state is default
	virtual bool HasDefaultState() const { return false; }

	//PURPOSE
	//  Returns true if this node can have default state, if so only a bool is written if the state is default
	virtual void SetDefaultState() { gnetAssertf(0, "SetDefaultState not declared for node %s", GetNodeName()); }

	//PURPOSE
	// Returns the size of the data the node would write for the specified target object
	//PARAMS
	// targetObject - The target object to extract the data from for calculating the size
	virtual unsigned GetDataSize(netSyncTreeTargetObject &targetObject) const { return m_dataOperations->GetDataSize(targetObject); }

	//PURPOSE
	//	Returns the maximum size of the data the node can write to a message buffer
	virtual u32 GetMaximumDataSize() const { return m_dataOperations->GetMaximumDataSize(); }

	//PURPOSE
	//	Applies the data held in the node to the target object
	//PARAMS
	//	pObj		- the object using the tree
    virtual void ApplyData(netSyncTreeTargetObject* pObj);

    //PURPOSE
	//	Logs the data held in the node
	//PARAMS
	//  log			- the log
    virtual void LogData(netLoggingInterface& log) { m_dataOperations->LogData(log); }

	//PURPOSE
	//	Displays the node data onscreen.
	//PARAMS
	//  displayLog			- the log used to display the data on screen
	virtual void DisplayData(netSyncTreeTargetObject* pObj, netLogDisplay& displayLog) { m_dataOperations->DisplayData(pObj, displayLog); }

	//PURPOSE
	//	Returns true if the data represented by the node is ready to be sent to the given player, either because it is dirty or unacked.
	//	Can be overidden with different rules.
	//PARAMS
	//	pObj		- the object using the tree
	//	player		- the player we are sending to
	//  serMode		- the serialise mode (eg update, create, migrate)
	//  currTime	- the current sync time
	virtual bool IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime ) const;

	//PURPOSE
	//	Returns true if the data represented by the node is ready to be sent to the given player, because of the update rate. Does not take
	// forced sending into account.
	bool IsTimeToSendUpdateToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, const unsigned currTime ) const;

	//PURPOSE
	//	Returns true if the data represented by the node is to be sent in an update message. Defaults to true as most nodes are update nodes.
	virtual bool GetIsSyncUpdateNode() const { return true; }

	//PURPOSE
	// ResetScriptStateInternal is defined by nodes associated with script data, resets the data when it is detected that the object using the node tree has reverted to an ambient object.
	void ResetScriptState() { m_Updated = ResetScriptStateInternal(); }

    virtual u8 GetNodeUpdateLevel(const u32 updateLevel) const { return m_NodeUpdateLevels[updateLevel]; }

    //PURPOSE
    // returns the update frequency for the node at the specified update level
    //PARAMS
    // updateLevel - The update level to return the update frequency for
    virtual	u32 GetUpdateFrequency(const u32 updateLevel) const
	{
		if (AssertVerify(updateLevel<CNetworkSyncDataULBase::NUM_UPDATE_LEVELS))
		{
			return CNetworkSyncDataULBase::GetUpdateFrequency(m_NodeUpdateLevels[updateLevel]);
		}

		return CNetworkSyncDataULBase::GetUpdateFrequency(CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW);
	}

#if __BANK

	//PURPOSE
	//	Sets the ptr to sync data of the current network object using the node (for easier debugging)
	void SetSyncDataPtr(netSyncDataBase** ppSyncData) { m_ppSyncData = ppSyncData; }

	//PURPOSE
	//	Sets the ptr to node data of the current network object using the node (for easier debugging)
	void SetNodeDataPtr(netSyncDataUnit_Static<MAX_NUM_PHYSICAL_PLAYERS>** ppNodeData) { m_ppNodeData = ppNodeData; }

    //PURPOSE
	//	Sets the ID of the bandwidth recorder used to monitor this node
	void SetBandwidthRecorderID(RecorderId recorderID) { m_bandwidthRecorderID = recorderID; }

	//PURPOSE
	//	Returns true if a bandwidth recorder has been setup for this node
	bool HasBandwidthRecorder() const { return (m_bandwidthRecorderID != INVALID_RECORDER_ID); }

    //PURPOSE
	//	Returns the ID of the bandwidth recorder used to monitor this node
	RecorderId GetBandwidthRecorderID() { gnetAssert(HasBandwidthRecorder()); return m_bandwidthRecorderID; }

	void DisplayNodeInformation(netSyncTreeTargetObject* pObj, netLogDisplay& displayLog);
#endif // __BANK

protected:

	//PURPOSE
	// Used by nodes associated with script data, resets the data when it is detected that the object using the node tree has reverted to an ambient object.
	virtual bool ResetScriptStateInternal() { return false; }

	//PURPOSE
	// returns flags for all players that have the entity cloned on their machine which the given object is attached to
	PlayerFlags GetIsAttachedForPlayers(const netSyncTreeTargetObject* pObj, bool includeTrailers = true) const;

	//PURPOSE
	// Returns whether the data synced via this node can be sent when there is no game object associated with the network object
	virtual bool CanSendWithNoGameObject() const { return false; }

	//PURPOSE
	// returns true if the entity is inside an interior.
	//PARAMS
	//	pObj		- the object using the tree
	bool GetIsInInterior(const netSyncTreeTargetObject* pObj) const;

	//PURPOSE
	// returns true if the entity is in a mocap cutscene
	bool GetIsInCutscene(const netSyncTreeTargetObject* pObj) const;

	//PURPOSE
	//	Returns the time between unacked resends
    //PARAMS
    // playerIndex - the player to check resend frequency for
	virtual u32 GetResendFrequency(PhysicalPlayerIndex playerIndex) const
	{
        FastAssert(playerIndex < MAX_NUM_PHYSICAL_PLAYERS);
        return ms_BaseResendFrequencyForPlayer[playerIndex];
	}

protected:

    void SetDataOperations(ICommonDataOperations &dataOperations) { m_dataOperations = &dataOperations; }

	void InitUpdateRates()
	{
		// initialise update rates as invalid, classes that derive from this class must initialise the update rates properly
		for (s32 i=0; i<CNetworkSyncDataULBase::NUM_UPDATE_LEVELS; i++)
		{
			m_NodeUpdateLevels[i] = CNetworkSyncDataULBase::NUM_UPDATE_LEVELS;
		}
	}

	// node adjusted update levels - some nodes want to send data at a different frequency than default
	u8 m_NodeUpdateLevels[CNetworkSyncDataULBase::NUM_UPDATE_LEVELS];

private:
    // pointer to the common data operations class for this node
    ICommonDataOperations *m_dataOperations;

    // Resend frequencies for players
    static u32 ms_BaseResendFrequencyForPlayer[MAX_NUM_PHYSICAL_PLAYERS];

#if __BANK
	// ptr to sync data of the current network object using the node (for easier debugging)
	netSyncDataBase** m_ppSyncData;

	// ptr to node data of the current network object using the node (for easier debugging)
	netSyncDataUnit_Static<MAX_NUM_PHYSICAL_PLAYERS>** m_ppNodeData;

    // bandwidth recorder ID for monitoring the data synched by this node
    RecorderId m_bandwidthRecorderID;
#endif // __BANK
};

///////////////////////////////////////////////////////////////////////////////////////
// CSyncDataNodeFrequent
//
// A CProjectBaseSyncDataNode that sets up frequent update frequencies
///////////////////////////////////////////////////////////////////////////////////////
class CSyncDataNodeFrequent : public CProjectBaseSyncDataNode
{
public:

	CSyncDataNodeFrequent()
	{
		SetUpdateRates();
	}

protected:

	void SetUpdateRates();
};

///////////////////////////////////////////////////////////////////////////////////////
// CSyncDataNodeInfrequent
//
// A CProjectBaseSyncDataNode that sets up infrequent update frequencies
///////////////////////////////////////////////////////////////////////////////////////
class CSyncDataNodeInfrequent : public CProjectBaseSyncDataNode
{
public:

	CSyncDataNodeInfrequent()
	{
		SetUpdateRates();
	}

protected:

	void SetUpdateRates();
};

#define NODE_COMMON_IMPL(NodeName, Node, NodeDataAccessorInterface) \
void InitialiseNode()\
{\
	OUTPUT_ONLY(SetNodeName(NodeName));\
	m_nodeDataOperations.SetNode(*this);\
	SetDataOperations(m_nodeDataOperations);\
}\
template <class Node, class DataAccessor> friend class NodeCommonDataOperations;\
NodeCommonDataOperations<Node, NodeDataAccessorInterface> m_nodeDataOperations;

#endif  // BASE_SYNC_NODES_H
