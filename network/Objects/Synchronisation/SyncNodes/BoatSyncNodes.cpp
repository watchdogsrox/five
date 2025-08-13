//
// name:        BoatSyncNodes.cpp
// description: Network sync nodes used by CNetObjBoats
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/BoatSyncNodes.h"
#include "vehicles/Boat.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IBoatNodeDataAccessor);

void CBoatGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_BOAT_WRECKED_ACTION = 2;
	static const unsigned int SIZEOF_BUOYANCY_FORCE_MULTIPLIER = 14;

	SERIALISE_BOOL(serialiser, m_lockedToXY, "Locked to XY");
	SERIALISE_UNSIGNED(serialiser, m_boatWreckedAction, SIZEOF_BOAT_WRECKED_ACTION, "Boat Wrecked Action");
    SERIALISE_BOOL(serialiser, m_ForceLowLodMode, "Force low lod mode");
	SERIALISE_BOOL(serialiser, m_UseWidestToleranceBoundingBoxTest, "Use widest tolerance bounding box test for rivers");

	SERIALISE_BOOL(serialiser, m_interiorLightOn, "Interior Light On");

	SERIALISE_PACKED_FLOAT(serialiser, m_BuoyancyForceMultiplier, 1.0f, SIZEOF_BUOYANCY_FORCE_MULTIPLIER, "Buoyancy Force Multiplier");

    bool hasAnchorLodDistance = m_AnchorLodDistance >= 0.0f;
    SERIALISE_BOOL(serialiser, hasAnchorLodDistance);

    if(hasAnchorLodDistance || serialiser.GetIsMaximumSizeSerialiser())
    {
        const float    MAX_ANCHOR_LOD_DISTANCE    = 1000.0f;
        const unsigned SIZEOF_ANCHOR_LOD_DISTANCE = 16;

        bool hasTooLargeAnchorDistance = m_AnchorLodDistance > MAX_ANCHOR_LOD_DISTANCE;

        SERIALISE_BOOL(serialiser, hasTooLargeAnchorDistance);

        if(!hasTooLargeAnchorDistance || serialiser.GetIsMaximumSizeSerialiser())
        {
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_AnchorLodDistance, MAX_ANCHOR_LOD_DISTANCE, SIZEOF_ANCHOR_LOD_DISTANCE, "Anchor LOD distance");
        }
        else
        {
            // horrible hack - scripts are using this value to force the LOD switch to never happen
            m_AnchorLodDistance = 99999.0f;
        }
    }
    else
    {
        m_AnchorLodDistance = -1.0f;
    }
}

bool CBoatGameStateDataNode::HasDefaultState() const 
{ 
	return (m_lockedToXY == false && m_boatWreckedAction == BWA_FLOAT && m_ForceLowLodMode && m_AnchorLodDistance < 0.0f
		&& m_UseWidestToleranceBoundingBoxTest == false && m_interiorLightOn == true);
} 

void CBoatGameStateDataNode::SetDefaultState() 
{ 
	m_lockedToXY						= false;
	m_boatWreckedAction					= BWA_FLOAT;
    m_ForceLowLodMode					= false;
	m_UseWidestToleranceBoundingBoxTest = false;
    m_AnchorLodDistance					= -1.0f;
	m_interiorLightOn					= true;
	m_BuoyancyForceMultiplier           = 1.0f;
}
