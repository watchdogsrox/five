//
// name:        TrainSyncNodes.cpp
// description: Network sync nodes used by CNetObjTrain
// written by:    
//

#include "network/objects/synchronisation/syncnodes/TrainSyncNodes.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(ITrainNodeDataAccessor);

void CTrainGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_CONFIG_INDEX     = 8;
	static const unsigned int SIZEOF_TRACK_ID         = 8;
	static const unsigned int SIZEOF_TRAIN_SPEED      = 8;
	static const unsigned int SIZEOF_DIST_FROM_ENGINE = 32;
	static const float        MAX_DIST_FROM_ENGINE = 1000.0f;
	static const float        MAX_TRAIN_SPEED = 30.0f;
	static const unsigned int SIZEOF_TRAIN_STATE = 3;

	SERIALISE_OBJECTID(serialiser, m_EngineID,           "Engine ID");
	SERIALISE_OBJECTID(serialiser, m_LinkedToBackwardID,  "LinkedToBackward ID");
	SERIALISE_OBJECTID(serialiser, m_LinkedToForwardID,   "LinkedToForward ID");
	SERIALISE_PACKED_FLOAT(serialiser, m_DistFromEngine, MAX_DIST_FROM_ENGINE, SIZEOF_DIST_FROM_ENGINE, "Distance from Engine");

	SERIALISE_INTEGER(serialiser, m_TrainConfigIndex,    SIZEOF_CONFIG_INDEX,    "Train Config Index");
	SERIALISE_INTEGER(serialiser, m_CarriageConfigIndex, SIZEOF_CONFIG_INDEX, "Carriage Config Index");
	SERIALISE_INTEGER(serialiser, m_TrackID, SIZEOF_TRACK_ID, "Track ID");
	SERIALISE_PACKED_FLOAT(serialiser, m_CruiseSpeed,    MAX_TRAIN_SPEED, SIZEOF_TRAIN_SPEED, "Cruise Speed");

	SERIALISE_UNSIGNED(serialiser, m_TrainState, SIZEOF_TRAIN_STATE, "m_TrainState");

	SERIALISE_BOOL(serialiser, m_IsEngine,               "Is Engine");
	SERIALISE_BOOL(serialiser, m_AllowRemovalByPopulation, "Allow Removal by Population");
	SERIALISE_BOOL(serialiser, m_IsCaboose,              "Is Caboose");
	SERIALISE_BOOL(serialiser, m_IsMissionTrain,         "Is Mission Train");
	SERIALISE_BOOL(serialiser, m_Direction,              "Direction");
	SERIALISE_BOOL(serialiser, m_HasPassengerCarriages,  "Has Passenger Carriages");
	SERIALISE_BOOL(serialiser, m_RenderDerailed,         "Render Derailed");

	SERIALISE_BOOL(serialiser, m_doorsForcedOpen,        "Force Doors Open");
	SERIALISE_BOOL(serialiser, m_StopForStations,        "Stopping at Station");

	SERIALISE_BOOL(serialiser, m_UseHighPrecisionBlending, "High Precision Train Blending")
}
