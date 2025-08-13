//
// name:        GlassPaneSyncNodes.h
// description: Network sync nodes used by CNetObjGlassPane
// written by:    John Gurney
//

#ifndef GLASS_PANE_SYNC_NODES_H
#define GLASS_PANE_SYNC_NODES_H

//#include "pickups/Data/PickupIds.h"
#include "script/Handlers/GameScriptIds.h"
#include "Network/Objects/Synchronisation/SyncNodes/BaseSyncNodes.h"

//framework includes
#include "fwscript/scriptid.h" // for USE_SCRIPT_NAME

#include "fwmaths/random.h"

class CGlassPaneCreationDataNode;

///////////////////////////////////////////////////////////////////////////////////////
// IGlassPaneNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the glass panes.
// Any class that wishes to send/receive data contained in the object nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IGlassPaneNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IGlassPaneNodeDataAccessor);

    virtual void GetGlassPaneCreateData(CGlassPaneCreationDataNode& data) const = 0;
    virtual void SetGlassPaneCreateData(const CGlassPaneCreationDataNode& data) = 0;

protected:

    virtual ~IGlassPaneNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CGlassPaneCreationDataNode
//
// Handles the serialization of an pickup's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CGlassPaneCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Glass Pane Creation", CGlassPaneCreationDataNode, IGlassPaneNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

private:

    void ExtractDataForSerialising(IGlassPaneNodeDataAccessor &glassPaneData)
	{
		glassPaneData.GetGlassPaneCreateData(*this);
    }

    void ApplyDataFromSerialising(IGlassPaneNodeDataAccessor &glassPaneData)
    {
		glassPaneData.SetGlassPaneCreateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

	u32						m_geometryObjHash;	// a custom hash made from the WS position of the CObject associated with this CNetObjGlassPane.

	Vector3					m_position;		// Needed to find the object in question...
	float					m_radius;		// Needed for migration check if geometry isn't streamed in...
	bool					m_isInside;		// Needed to speed up object lookup search...

	Vector2					m_hitPositionOS;
	u8						m_hitComponent;
};

#endif  // GLASS_PANE_SYNC_NODES_H
