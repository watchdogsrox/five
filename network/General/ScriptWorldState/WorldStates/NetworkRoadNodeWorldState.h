//
// name:		NetworkRoadNodeWorldState.h
// description:	Support for network scripts to switch road nodes off via the script world state management code
// written by:	Daniel Yelland
//

#ifndef NETWORK_ROAD_NODE_WORLD_STATE_H
#define NETWORK_ROAD_NODE_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes a region of the map in which road nodes have been disabled by a script
class CNetworkRoadNodeWorldStateData : public CNetworkWorldStateData
{
private:
	enum NodeSwitchState
	{
		eActivate,
		eDeactivate,
		eToOriginal
	};

public:

    FW_REGISTER_CLASS_POOL(CNetworkRoadNodeWorldStateData);

    //PURPOSE
    // Class constructor
    CNetworkRoadNodeWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID     - The ID of the script that has disabled the road nodes
    // locallyOwned - Indicates whether this nodes have been disabled by the local machine
    // regionMin    - Minimum values of the region bounding box
    // regionMax    - Maximum values of the region bounding box
	CNetworkRoadNodeWorldStateData(const CGameScriptId &scriptID,
								   bool                 locallyOwned,
								   const Vector3       &regionMin,
								   const Vector3       &regionMax,
								   NodeSwitchState		nodeState);

	CNetworkRoadNodeWorldStateData(const CGameScriptId &scriptID,
								   bool                 locallyOwned,
								   const Vector3       &regionStart,
								   const Vector3       &regionEnd,
								   float				regionWidth,
								   NodeSwitchState		nodeState);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_ROAD_NODE; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkRoadNodeWorldStateData *Clone() const;

    //PURPOSE
    // Returns the bounding box region of disabled road nodes
    //PARAMS
    // min - Minimum values of the region bounding box
    // max - Maximum values of the region bounding box
    void          GetRegion(Vector3 &min, 
                            Vector3 &max) const;

    //PURPOSE   
    // Returns whether the specified region of disabled nodes 
    // disabled by a script is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Switches off the road nodes within the region stored in this instance
    void ChangeWorldState();

    //PURPOSE
    // Switches on the road nodes within the region stored in this instance
    void RevertWorldState();

	//PURPOSE
	// Switches the road nodes back to their original value.
	// If this is called locally, the nodes will already have
	// been switch. If called based on a network update,
	// this function switches the nodes back to original.
	//PARAMS
	// scriptID    - The ID of the script that has enabled the road nodes
	// regionMin   - Minimum values of the region bounding box
	// regionMax   - Maximum values of the region bounding box
	// fromNetwork - Indicates whether this call has originated from a network update
	static void SetNodesToOriginal(const CGameScriptId &scriptID,
								   const Vector3       &regionMin,
								   const Vector3       &regionMax,
								   bool                 fromNetwork);

	// Switches the road nodes back to their original value.
	// If this is called locally, the nodes will already have
	// been switch. If called based on a network update,
	// this function switches the nodes back to original.
	//PARAMS
	// scriptID    - The ID of the script that has enabled the road nodes
	// regionMin   - Minimum values of the region bounding box
	// regionMax   - Maximum values of the region bounding box
	// regionWidth - Width of angled region
	// fromNetwork - Indicates whether this call has originated from a network update
	static void SetNodesToOriginalAngled(const CGameScriptId &scriptID,
										 const Vector3       &regionStart,
										 const Vector3       &regionEnd,
										 float				  regionWidth,
										 bool                 fromNetwork);

    //PURPOSE
    // Switches on the road nodes in the specified region.
    // If this is called locally, the nodes will already have
    // been enabled. If called based on a network update,
    // this function enables the nodes.
    //PARAMS
    // scriptID    - The ID of the script that has enabled the road nodes
    // regionMin   - Minimum values of the region bounding box
    // regionMax   - Maximum values of the region bounding box
    // fromNetwork - Indicates whether this call has originated from a network update
    static void SwitchOnNodes(const CGameScriptId &scriptID,
                              const Vector3       &regionMin,
                              const Vector3       &regionMax,
                              bool                 fromNetwork);

    //PURPOSE
    // Switches on the road nodes in the specified region.
    // If this is called locally, the nodes will already have
    // been enabled. If called based on a network update,
    // this function enables the nodes.
    //PARAMS
    // scriptID    - The ID of the script that has enabled the road nodes
	// regionStart - Top middle the region bounding box
	// regionEnd   - Bottom middle of the region bounding box
	// regionWidth - Width of angled region
    // fromNetwork - Indicates whether this call has originated from a network update
    static void SwitchOnNodesAngled(const CGameScriptId &scriptID,
									const Vector3       &regionStart,
									const Vector3       &regionEnd,
									float				  regionWidth,
									bool                 fromNetwork);

    //PURPOSE
    // Switches off the road nodes in the specified region.
    // If this is called locally, the nodes will already have
    // been disabled. If called based on a network update,
    // this function disables the nodes.
    //PARAMS
    // scriptID    - The ID of the script that has disabled the road nodes
    // regionMin   - Minimum values of the region bounding box
    // regionMax   - Maximum values of the region bounding box
    // fromNetwork - Indicates whether this call has originated from a network update
    static void SwitchOffNodes(const CGameScriptId &scriptID,
                               const Vector3       &regionMin,
                               const Vector3       &regionMax,
                               bool                 fromNetwork);

    //PURPOSE
    // Switches off the road nodes in the specified angled region.
    // If this is called locally, the nodes will already have
    // been disabled. If called based on a network update,
    // this function disables the nodes.
    //PARAMS
    // scriptID    - The ID of the script that has disabled the road nodes
    // regionStart - Top middle the region bounding box
    // regionEnd   - Bottom middle of the region bounding box
	// regionWidth - Width of angled region
    // fromNetwork - Indicates whether this call has originated from a network update
    static void SwitchOffNodesAngled(const CGameScriptId &scriptID,
									 const Vector3       &regionStart,
									 const Vector3       &regionEnd,
									 float				  regionWidth,
									 bool                 fromNetwork);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "ROAD_NODES"; };

    //PURPOSE
    // Logs the world state data
    //PARAMS
    // log - log file to use
    void LogState(netLoggingInterface &log) const;

    //PURPOSE
    // Reads the contents of an instance of this class via a read serialiser
    //PARAMS
    // serialiser - The serialiser to use
    void Serialise(CSyncDataReader &serialiser);

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    void Serialise(CSyncDataWriter &serialiser);

#if __DEV
    //PURPOSE
    // Returns whether this world state data is conflicting with the specified world state data.
    // We don't allow bounding boxes of disabled road nodes to overlap otherwise when one
    // world state is reverted it would enable road nodes still within the bounding box of
    // another potentially active world state
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsConflicting(const CNetworkWorldStateData &worldState) const;
#endif // __DEV

#if __BANK
    //PURPOSE
    // Adds debug widgets to test this world state
    static void AddDebugWidgets();

    //PURPOSE
    // Displays debug text describing this world state
    static void DisplayDebugText();
#endif // __BANK

private:

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

    Vector3 m_NodeRegionMin; // Minimum values of the region bounding box, in an angled region this is the top middle point
    Vector3 m_NodeRegionMax; // Maximum values of the region bounding box, in an angled region this is the bottom middle point
	float   m_BoxWidth;		 // When we are using a angled (non axis aligned region) this holds the width of the region bounding box
	bool	m_bAxisAligned;
	NodeSwitchState	m_nodeState;
};

#endif  // NETWORK_ROAD_NODE_WORLD_STATE_H
