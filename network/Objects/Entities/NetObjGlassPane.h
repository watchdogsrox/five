//
// name:		NetObjGlassPane.h
// description:	Derived from netObject, this class handles all pickup placement related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	John Gurney
//

#ifndef NETOBJGLASSPANE_H
#define NETOBJGLASSPANE_H

//----------------------

#include "glassPaneSyncing/GlassPaneInfo.h"
#include "network/objects/entities/NetObjProximityMigrateable.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "Objects/objectpopulation.h"

//----------------------

class CGlassPane;

//----------------------

#define NET_OBJ_GLASS_PANE_SCOPE_DISTANCE	(80.0f)

//----------------------

class CNetObjGlassPane : public CNetObjProximityMigrateable, public IGlassPaneNodeDataAccessor
{
public:

#if GLASS_PANE_SYNCING_ACTIVE
	FW_REGISTER_CLASS_POOL(CNetObjGlassPane);
#endif /* GLASS_PANE_SYNCING_ACTIVE */

	struct CGlassPaneScopeData : public netScopeData
	{
		CGlassPaneScopeData()
		{
			m_scopeDistance			= NET_OBJ_GLASS_PANE_SCOPE_DISTANCE;
			m_scopeDistanceInterior = NET_OBJ_GLASS_PANE_SCOPE_DISTANCE;
			m_syncDistanceNear		= 10.0;
			m_syncDistanceFar		= 60.0f;
		}
	};

	static const int GLASS_PANE_SYNC_DATA_NUM_NODES = 2;
	static const int GLASS_PANE_SYNC_DATA_BUFFER_SIZE = 4;

	class CGlassPaneSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, GLASS_PANE_SYNC_DATA_NUM_NODES, GLASS_PANE_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
	{
	public:
#if GLASS_PANE_SYNCING_ACTIVE
        FW_REGISTER_CLASS_POOL(CGlassPaneSyncData);
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    };

public:

	CNetObjGlassPane(  CGlassPane*					glassPane,
				       const NetworkObjectType		type,
				       const ObjectId				objectID,
				       const PhysicalPlayerIndex    playerIndex,
				       const NetObjFlags			localFlags,
				       const NetObjFlags			globalFlags);

    CNetObjGlassPane() {SetObjectType(NET_OBJ_TYPE_GLASS_PANE);}

    virtual const char *GetObjectTypeName() const { return IsScriptObject() ? "SCRIPT_GLASS_PANE" : "GLASS_PANE"; }

	CGlassPane* GetGlassPane() const		{ return (CGlassPane*)GetGameObject(); }
/*
	
*/
	virtual bool IsInInterior() const;
	virtual Vector3 GetPosition() const;
	void SetPosition( const Vector3& pos );
	virtual bool IsInSameInterior(netObject&) const { return true; } // this isn't used for glass panes

    static void CreateSyncTree();
    static void DestroySyncTree();

	static  CProjectSyncTree*	GetStaticSyncTree()  { return ms_glassPaneSyncTree; }
	virtual CProjectSyncTree*	GetSyncTree()		 { return ms_glassPaneSyncTree; }
	virtual netScopeData&		GetScopeData()		 { return ms_glassPaneScopeData; }
	virtual netScopeData&		GetScopeData() const { return ms_glassPaneScopeData; }
    static  netScopeData&		GetStaticScopeData() { return ms_glassPaneScopeData; }
	virtual netSyncDataBase*	CreateSyncData()	 { return rage_new CGlassPaneSyncData(); }

	virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

	virtual const scriptObjInfoBase*	GetScriptObjInfo() const;
	virtual void						SetScriptObjInfo(scriptObjInfoBase* info);
	virtual scriptHandlerObject*		GetScriptHandlerObject() const;

	virtual void    CleanUpGameObject(bool bDestroyObject);
	
	virtual	bool	Update();
	
    virtual void    LogAdditionalData(netLoggingInterface &log) const;

	virtual bool    CanDelete(unsigned* reason = nullptr);

//	displays debug info above the network object
	virtual void DisplayNetworkInfo();

	// sync parser getter functions*/
	void GetGlassPaneCreateData(CGlassPaneCreationDataNode& data) const;
	void SetGlassPaneCreateData(const CGlassPaneCreationDataNode& data);

private:

	static CGlassPaneSyncTree	   *ms_glassPaneSyncTree;
	static CGlassPaneScopeData		ms_glassPaneScopeData;
};

#endif  // NETOBJGLASSPANE_H
