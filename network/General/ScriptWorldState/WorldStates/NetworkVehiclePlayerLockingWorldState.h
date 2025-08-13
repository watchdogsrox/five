//
// name:		NetworkPopMultiplierAreaWorldState.h
// description:	Support for network scripts to switch pop multiplier areas
// written by:	Shamelessly copied by Allan Walton from Daniel Yelland code
//

#ifndef NETWORK_VEHICLE_PLAYER_LOCKING_WORLD_STATE_H
#define NETWORK_VEHICLE_PLAYER_LOCKING_WORLD_STATE_H

#include "fwtl/pool.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

//PURPOSE
// Describes which vehicles and which vehicle instances are locked to which players...
class CNetworkVehiclePlayerLockingWorldState : public CNetworkWorldStateData
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkVehiclePlayerLockingWorldState);

public:

	CNetworkVehiclePlayerLockingWorldState();

	CNetworkVehiclePlayerLockingWorldState(		const CGameScriptId &scriptID,
												bool const locallyOwned,
												u32 const playerIndex,
												s32 const vehicleIndex,
												u32 const modelId,
												bool const lock,
												s32 const index
											);

	 //PURPOSE
    // Returns the type of this world state
    virtual unsigned GetType() const { return NET_WORLD_STATE_VEHICLE_PLAYER_LOCKING; }

    //PURPOSE
    // Makes a copy of the world state data, required by the world state data manager
    CNetworkVehiclePlayerLockingWorldState *Clone() const;

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
    // Returns the name of this world state data
    const char *GetName() const { return "VEHICLE_PLAYER_LOCKING"; };

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

	//PURPOSE
    // 
    //PARAMS
    // serialiser - The serialiser to use
	static void SetModelIdPlayerLock
	(
		const CGameScriptId &scriptID, 
		bool const fromNetwork,
		u32 const playerIndex, 
		u32 const ModelId,
		bool const lock,
		s32 const index
	);

    //PURPOSE
    // 
    //PARAMS
    // serialiser - The serialiser to use
	static void SetModelInstancePlayerLock
	(
		const CGameScriptId &scriptID, 
		bool const fromNetwork,
		int const playerIndex, 
		int const instanceIndex,
		bool const lock,
		s32 const index
	);

    //PURPOSE
    // 
    //PARAMS
    //
	static void ClearModelIdPlayerLock(const CGameScriptId &scriptID, bool const fromNetwork, u32 const PlayerIndex, u32 const ModelId, bool const lock, s32 const index);

    //PURPOSE
    // 
    //PARAMS
    //
	static void ClearModelInstancePlayerLock(const CGameScriptId &scriptID, bool const fromNetwork, u32 const PlayerIndex, s32 const InstanceIndex, bool const lock, s32 const index);

#if __DEV
    //PURPOSE
    // Returns whether this world state data is conflicting with the specified world state data.
    // We don't allow bounding boxes of pop multipliers to overlap.
    //PARAMS
    // worldState - The world state to compare
    virtual bool IsConflicting(const CNetworkWorldStateData &worldState) const;
#endif // __DEV

#if __BANK

	static void NetworkBank_LockPickedVehicleModel(void);
	static void NetworkBank_UnlockPickedVehicleModel(void);
	static void NetworkBank_ClearPickedVehicleModel(void);
	
	static void NetworkBank_LockPickedVehicleInstance(void);
	static void NetworkBank_UnlockPickedVehicleInstance(void);
	static void NetworkBank_ClearPickedVehicleInstance(void);
		
	//PURPOSE
    // Adds debug widgets to test this world state
    static void AddDebugWidgets();

    //PURPOSE
    // Displays debug text describing this world state
    static void DisplayDebugText();
#endif // __BANK

public:

	inline u32	GetPlayerIndex(void)	const { return m_playerIndex;	}
	inline u32	GetModelId(void)		const { return m_modelId;		}
	inline s32	GetInstanceIndex(void)	const { return m_vehicleIndex;	}
	inline bool GetLocked(void)			const { return m_locked;		}
	inline u8	GetLockType(void)		const { return m_lockType;		}
	inline s32	GetIndex(void)			const { return m_index;			}

private:

    //PURPOSE
    // Write the contents of this instance of the class via a write serialiser
    //PARAMS
    // serialiser - The serialiser to use
    template <class Serialiser> void SerialiseWorldState(Serialiser &serialiser);

private:

	u32 m_playerIndex;
	u32 m_modelId;
	s32 m_vehicleIndex;
	bool m_locked;
	u8 m_lockType;
	s32 m_index;
};

#endif /* NETWORK_VEHICLE_PLAYER_LOCKING_WORLD_STATE_H */