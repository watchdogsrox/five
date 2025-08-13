//
// name:        AutomobileSyncNodes.cpp
// description: Network sync nodes used by CNetObjAutomobiles
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/AutomobileSyncNodes.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IAutomobileNodeDataAccessor);


void CAutomobileCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_allDoorsClosed, "All doors closed");

	if (!m_allDoorsClosed)
	{
		for (int index=0; index<NUM_VEH_DOORS_MAX; index++)
		{
			SERIALISE_BOOL(serialiser, m_doorsClosed[index], "Door Closed");
		}
	}
}

bool CAutomobileCreationDataNode::HasDefaultState() const 
{ 
	return m_allDoorsClosed; 
}

void CAutomobileCreationDataNode::SetDefaultState() 
{ 
	m_allDoorsClosed = true; 
}

