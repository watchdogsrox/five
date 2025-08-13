//
// name:		NetworkPopMultiplierAreaWorldState.h
// description:	Support for network scripts to switch pop multiplier areas
// written by:	Shamelessly copied by Allan Walton from Daniel Yelland code
//

#ifndef NETWORK_POP_MULTIPLIER_AREA_WORLD_STATE_H
#define NETWORK_POP_MULTIPLIER_AREA_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes a region of the map in which road nodes have been disabled by a script
class CNetworkPopMultiplierAreaWorldStateData : public CNetworkWorldStateData
{
public:

	static const s32	INVALID_AREA_INDEX;
	static const float	MAXIMUM_PED_DENSITY_VALUE;
	static const float	MAXIMUM_TRAFFIC_DENSITY_VALUE;
	static const u32	SIZEOF_FLOAT_PACKING;
	static const u32	SIZEOF_AREA_INDEX_PACKING;

public:

    FW_REGISTER_CLASS_POOL(CNetworkPopMultiplierAreaWorldStateData);

    //PURPOSE
    // Class constructor
    CNetworkPopMultiplierAreaWorldStateData();

    //PURPOSE
    // Class constructor
    //PARAMS
    // scriptID     - The ID of the script that has disabled the car generators
    // locallyOwned - Indicates whether these generators have been disabled by the local machine
    // regionMin    - Minimum values of the region bounding box
    // regionMax    - Maximum values of the region bounding box
    CNetworkPopMultiplierAreaWorldStateData(	
												const CGameScriptId &scriptID,
												const Vector3		&minWS,
												const Vector3		&maxWS,
												const float			pedDensity,
												const float			trafficDensity,
												const bool			local,
												const bool			sphere,
												const bool			bCameraGlobalMultiplier
											);

    //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_POP_MULTIPLIER_AREA; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkPopMultiplierAreaWorldStateData *Clone() const;

    //PURPOSE
    // Returns the bounding box region of disabled car generators
    //PARAMS
    // min - Minimum values of the region bounding box
    // max - Maximum values of the region bounding box
    void          GetRegion(Vector3 &min, 
                            Vector3 &max) const;

    //PURPOSE   
    // Returns whether the specified region of disabled car generators
    // disabled by a script is equal to the data stored in this instance
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsEqualTo(const CNetworkWorldStateData &worldState) const;

    //PURPOSE
    // Switches off the car generators within the region stored in this instance
    void ChangeWorldState();

    //PURPOSE
    // Switches on the car generators within the region stored in this instance
    void RevertWorldState();


	//PURPOSE
    // Adds an area. If this is called locally, the area will have already been added
	// if called on a network update, it adds the area....
	static void AddArea(	
							const CGameScriptId &_scriptID,
							const Vector3		&_minWS,
							const Vector3		&_maxWS,
							const float			_pedDensity,
							const float			_trafficDensity,
							const bool			_fromNetwork,
							const bool			_sphere,
							const bool			bCameraGlobalMultiplier
						);

	//PURPOSE
    // Removes an area. If this is called locally, the area will have already been added
	// if called on a network update, it adds the area....
	static void RemoveArea(
							const CGameScriptId &_scriptID,
							const Vector3		&_minWS,
							const Vector3		&_maxWS,
							const float			_pedDensity,
							const float			_trafficDensity,
							const bool			_fromNetwork,
							const bool			sphere,
							const bool			bCameraGlobalMultiplier
						);

	static bool	FindArea(
							const Vector3		&_minWS,
							const Vector3		&_maxWS,
							const float			_pedDensity,
							const float			_trafficDensity,
							const bool			_sphere,
							const bool			bCameraGlobalMultiplier
						);

    //PURPOSE
    // Returns the name of this world state data
    const char *GetName() const { return "POP_AREA_MULTIPLIERS"; };

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
    // We don't allow bounding boxes of pop multipliers to overlap.
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

private:

	Vector3		m_minWS;
	Vector3		m_maxWS;
	float		m_pedDensity;
	float		m_trafficDensity;
	bool		m_isSphere;
	bool		m_bCameraGlobalMultiplier;
};

#endif  // NETWORK_POP_MULTIPLIER_AREA_WORLD_STATE_H
