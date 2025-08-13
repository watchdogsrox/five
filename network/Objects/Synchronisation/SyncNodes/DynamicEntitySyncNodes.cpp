//
// name:        DynamicEntitySyncNodes.cpp
// description: Network sync nodes used by CNetObjDynamicEntitys
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/DynamicEntitySyncNodes.h"
#include "scene/world/GameWorld.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IDynamicEntityNodeDataAccessor);

void CDynamicEntityGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_INTERIOR_LOC     = 32;
	static const unsigned int SIZEOF_DECORS_COUNT     = datBitsNeeded<IDynamicEntityNodeDataAccessor::MAX_DECOR_ENTRIES>::COUNT;
	static const unsigned int SIZEOF_DECORS_DATATYPE  = 3;
	static const unsigned int SIZEOF_DECORS_KEY       = 32;
	static const unsigned int SIZEOF_DECORS_DATAVAL   = 32;

	SERIALISE_BOOL(serialiser, m_retained);

	if (m_retained && !serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_InteriorProxyLoc, SIZEOF_INTERIOR_LOC, "Interior Retain Index");
	}
	else
	{
		u32 outsideLoc = CGameWorld::OUTSIDE.GetAsUint32();

		bool bOutside = m_InteriorProxyLoc == outsideLoc;

		SERIALISE_BOOL(serialiser, bOutside);

		if (!bOutside || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_InteriorProxyLoc, SIZEOF_INTERIOR_LOC, "Interior Location");
		}
		else
		{		
			m_InteriorProxyLoc = outsideLoc;
		}
	}

	SERIALISE_BOOL(serialiser, m_loadsCollisions, "Loads Collisions");

	// Decorator sync
	bool hasDecorators = ( m_decoratorListCount > 0 );

	SERIALISE_BOOL(serialiser, hasDecorators);

	if( hasDecorators || serialiser.GetIsMaximumSizeSerialiser() )
	{
		SERIALISE_UNSIGNED(serialiser, m_decoratorListCount,SIZEOF_DECORS_COUNT,"Decorator count");

		if (serialiser.GetIsMaximumSizeSerialiser())
		{
			m_decoratorListCount = IDynamicEntityNodeDataAccessor::MAX_DECOR_ENTRIES;
		}

		for(u32 decor = 0; decor < m_decoratorListCount; decor++)
		{
			SERIALISE_UNSIGNED(serialiser, m_decoratorList[decor].Type,SIZEOF_DECORS_DATATYPE);
			SERIALISE_UNSIGNED(serialiser, m_decoratorList[decor].Key,SIZEOF_DECORS_KEY);
			SERIALISE_UNSIGNED(serialiser, m_decoratorList[decor].String,SIZEOF_DECORS_DATAVAL);	// union of float/bool/int/string
		}
	}
	else
	{
		m_decoratorListCount = 0;
	}
}

bool CDynamicEntityGameStateDataNode::HasDefaultState() const 
{ 
	bool bDefaultState = (m_InteriorProxyLoc == CGameWorld::OUTSIDE.GetAsUint32() && !m_loadsCollisions && !m_retained);
	return bDefaultState; 
}

void CDynamicEntityGameStateDataNode::SetDefaultState()
{
	m_InteriorProxyLoc = CGameWorld::OUTSIDE.GetAsUint32();
	m_loadsCollisions = false;
	m_retained = false;
}
