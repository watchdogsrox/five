//
// name:        NetObjDynamicEntity.h
// description: Derived from netObject, this class handles all dynamic entity-related network
//              object calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJDYNAMICENTITY_H
#define NETOBJDYNAMICENTITY_H

#include "network/objects/entities/NetObjEntity.h"
#include "network/objects/synchronisation/syncnodes/dynamicentitysyncnodes.h"

class CNetObjDynamicEntity : public CNetObjEntity, public IDynamicEntityNodeDataAccessor
{
public:

	static const unsigned INTERIOR_DISCREPANCY_RESCAN_TIME = 1000; // the time we wait until forcing an interior rescan when the synced interior location does not match our locally determined location

public:

    CNetObjDynamicEntity(class CDynamicEntity		*dynamicEntity,
                         const NetworkObjectType	type,
                         const ObjectId				objectID,
                         const PhysicalPlayerIndex  playerIndex,
                         const NetObjFlags			localFlags,
                         const NetObjFlags			globalFlags);

	CNetObjDynamicEntity();

    CDynamicEntity* GetDynamicEntity() const { return (CDynamicEntity*)GetEntity(); }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    // see NetworkObject.h for a description of these functions
    virtual void        ChangeOwner(const netPlayer& player, eMigrationType migrationType);
	virtual bool        ProcessControl();

	virtual void		PostCreate();

    // sync parser getter functions
    void GetDynamicEntityGameState(CDynamicEntityGameStateDataNode& data) const ;
    // sync parser setter functions
    void SetDynamicEntityGameState(const CDynamicEntityGameStateDataNode& data);

	//Returns true if the give entity is in the same interior
	virtual bool IsInSameInterior(netObject& otherObj) const;

    bool IsInUnstreamedInterior(unsigned* fixedByNetworkReason = NULL) const;
    bool IsCloneInTargetInteriorState() const;

	void SetRetainedInInterior(bool retainedInInterior) { m_bRetainedInInterior = retainedInInterior; }
private:

	u32  m_InteriorProxyLoc;      // the interior this entity is in (used by clones only)
	u16  m_InteriorDiscrepancyTimer;	// set when m_InteriorProxyLoc does not match the locally determined interior of this entity 
	bool m_bLoadsCollisions : 1; // flag indicating this dynamic entity loads collisions (only applied during ownership change process)
	bool m_bRetainedInInterior : 1; // indicates whether the entity is on a retained list for an interior not streamed in
	
};
#endif  // NETOBJDYNAMICENTITY_H
