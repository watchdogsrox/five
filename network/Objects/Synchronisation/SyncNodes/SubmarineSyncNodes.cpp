//
// name:        SubmarineSyncNodes.cpp
// description: Network sync nodes used by CNetObjSubmarine
// written by:    
//

#include "network/objects/synchronisation/syncnodes/SubmarineSyncNodes.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(ISubmarineNodeDataAccessor);

void CSubmarineControlDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_ANGCONTROL = 8;
	//static const unsigned int SIZEOF_THROTTLE   = 4;
	//static const unsigned int SIZEOF_PROPELLERSPEED = 32;

	SERIALISE_PACKED_FLOAT(serialiser, m_yaw,   1.0f, SIZEOF_ANGCONTROL, "Yaw Control");
	SERIALISE_PACKED_FLOAT(serialiser, m_pitch, 1.0f, SIZEOF_ANGCONTROL, "Pitch Control");
	SERIALISE_PACKED_FLOAT(serialiser, m_dive,  1.0f, SIZEOF_ANGCONTROL, "Dive Control");
}

bool CSubmarineControlDataNode::HasDefaultState() const
{
	return (m_yaw == 0.0f && m_pitch == 0.0f && m_dive == 0.0f);
}

void CSubmarineControlDataNode::SetDefaultState()
{
	m_yaw = m_pitch = m_dive = 0.0f;
}

void CSubmarineGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_IsAnchored, "Is Anchored");
}

bool CSubmarineGameStateDataNode::HasDefaultState() const 
{ 
	return m_IsAnchored == false;
} 

void CSubmarineGameStateDataNode::SetDefaultState() 
{ 
	m_IsAnchored = false;
}
