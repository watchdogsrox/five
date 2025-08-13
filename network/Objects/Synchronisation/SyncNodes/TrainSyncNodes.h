//
// name:        TrainSyncNodes.h
// description: Network sync nodes used by CNetObjTrain
// written by:  
//

#ifndef TRAIN_SYNC_NODES_H
#define TRAIN_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/VehicleSyncNodes.h"

class CTrainGameStateDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// ITrainNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the train nodes.
// Any class that wishes to send/receive data contained in the train nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class ITrainNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(ITrainNodeDataAccessor);

	virtual void GetTrainGameState(CTrainGameStateDataNode& data) const = 0;

	virtual void SetTrainGameState(const CTrainGameStateDataNode& data) = 0;

protected:

    virtual ~ITrainNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CTrainGameStateDataNode
//
// Handles the serialization of a train's game state
///////////////////////////////////////////////////////////////////////////////////////
class CTrainGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Train Game State", CTrainGameStateDataNode, ITrainNodeDataAccessor);

private:

    void ExtractDataForSerialising(ITrainNodeDataAccessor &trainNodeData)
    {
		trainNodeData.GetTrainGameState(*this);
    }

    void ApplyDataFromSerialising(ITrainNodeDataAccessor &trainNodeData)
    {
		trainNodeData.SetTrainGameState(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

public:

    bool     m_IsEngine;                 // is this train an engine or carriage?
	bool	 m_AllowRemovalByPopulation; // used by stationary trains in missions
	bool     m_IsCaboose;                // Is this a caboose
	bool     m_IsMissionTrain;           // Is this a mission created train?
	bool     m_Direction;                // Direction traveling on track
	bool     m_HasPassengerCarriages;    // Does this train have any passenger carriages?
	bool     m_RenderDerailed;           // Should this train be rendered as derailed?
	bool	 m_StopForStations;		     // Stop for stations
	ObjectId m_EngineID;                 // ID of the engine this carriage is attached to (if this train is not an engine)
	s8       m_TrainConfigIndex;         // Config index of the entire train this carriage/engine is a part of
	s8       m_CarriageConfigIndex;      // Config index of the carriage
	s8       m_TrackID;                  // the track the train is on
	float    m_DistFromEngine;           // the distance of this carriage from the engine (0.0 if this is an engine)
	float    m_CruiseSpeed;              // the target cruise speed of the train (desired speed)
	ObjectId m_LinkedToBackwardID;	     // ID of the car linked backward from this train car
	ObjectId m_LinkedToForwardID;	     // ID of the car linked forward from this train car
	unsigned m_TrainState;			     // the train state
	bool	 m_doorsForcedOpen;		     // force the doors open
	bool     m_UseHighPrecisionBlending; // are we using high precision blending on the train
};

#endif  //TRAIN_SYNC_NODES_H
