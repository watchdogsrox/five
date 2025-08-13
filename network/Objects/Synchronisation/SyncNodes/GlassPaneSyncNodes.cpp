//
// name:        GlassPaneSyncNodes.cpp
// description: Network sync nodes used by CNetObjGlassPanes
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/GlassPaneSyncNodes.h"

#include "fwnet/netlog.h"

#include "network/network.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptMgr.h"
#include "script/script.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IGlassPaneNodeDataAccessor);

void CGlassPaneCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_RADIUS	 = 8;
	static const unsigned int SIZEOF_HIT_POS = 8;
	static const unsigned int SIZEOF_GEOM_HASH = 32;

	gnetAssertf(m_radius < 5.f,				"ERROR : CGlassPaneCreationDataNode::Seralise : Window radius is over 5m?! : ADW RSGLDS");
	gnetAssertf(m_hitPositionOS.x < 10.f,	"ERROR : CGlassPaneCreationDataNode::Seralise : Window pane x is over 10m?! : ADW RSGLDS");
	gnetAssertf(m_hitPositionOS.y < 10.f,	"ERROR : CGlassPaneCreationDataNode::Seralise : Window pane y is over 10m?! : ADW RSGLDS");

	SERIALISE_UNSIGNED(serialiser, m_geometryObjHash, SIZEOF_GEOM_HASH, "Geometry Object Hash");

	SERIALISE_POSITION(serialiser, m_position, "Pos");
	SERIALISE_PACKED_FLOAT(serialiser, m_radius, 5.0f,	SIZEOF_RADIUS, "Radius");
	SERIALISE_BOOL(serialiser, m_isInside, "Is Inside");

	SERIALISE_PACKED_FLOAT(serialiser, m_hitPositionOS.x, 10.0f, SIZEOF_HIT_POS, "Hit Pos x");
	SERIALISE_PACKED_FLOAT(serialiser, m_hitPositionOS.y, 10.0f, SIZEOF_HIT_POS, "Hit Pos y");
	SERIALISE_UNSIGNED(serialiser, m_hitComponent, 8, "Hit Component");
}


