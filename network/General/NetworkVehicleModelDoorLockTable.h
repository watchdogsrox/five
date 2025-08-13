//
// name:        NetworkVehicleModelDoorLockTable.h
// description: Maintains an array that stores info about which vehicle types are locked for the local player
// written by:  Allan Walton
//

#ifndef NETWORK_VEHICLE_MODEL_DOOR_LOCK_TABLE_H_
#define NETWORK_VEHICLE_MODEL_DOOR_LOCK_TABLE_H_

#include "streaming/populationstreaming.h"

#define NETWORK_VEHICLE_DOOR_LOCKING_DEBUG 0

class CNetworkVehicleModelDoorLockedTableManager
{
	friend class CGame;

public:

	static const u32 MAX_NUM_PLAYER_LOCK_INFO_SLOTS = 64;

	// A player info should always have EITHER a model hash OR an instance ID & lock value - not both at the same time.
	struct PlayerLockInfo
	{
		PlayerLockInfo();

		enum LockType
		{
			LockType_ModelId		= 0, // Lock all vehicles of a certain model type (e.g. type = infernus)
			LockType_ModelInstance	= 1, // Lock a specific instance of a vehicle
			LockType_MaxInvalid		= 2
		};

		bool		operator==(PlayerLockInfo const& rhs) const;

		bool		CheckIntegrity(void) const; 
		inline bool IsFree(void) const;
		inline void Reset(void);
		inline void SetModelIdLock(u32 const playerIndex, u32 const modelType, bool const lock);
		inline void SetModelInstanceLock(u32 const playerIndex, u32 const vehicleIndex, bool lock);

		inline LockType	GetLockType(void)		const { return m_lockType; }
		inline u32		GetPlayerIndex(void)	const { return m_playerIndex; }
		inline u32		GetModelHash(void)		const { return m_modelId; }
		inline s32		GetVehicleIndex(void)	const { return m_vehicleIndex; }
		inline bool		GetLocked(void)			const { return m_lock; }

		u32			m_playerIndex;			// playerindex
		u32			m_modelId;				// model id
		s32			m_vehicleIndex;			// instance id
		bool		m_lock;					// an instance can be specifically locked or unlocked 
		LockType	m_lockType;				// are we a blanket model lock or an instance lock / unlock?
	};

public:

	static const s32 INVALID_PLAYER_INFO_INDEX;
	static const s32 INVALID_INSTANCE_INDEX;

public:

    CNetworkVehicleModelDoorLockedTableManager();
	~CNetworkVehicleModelDoorLockedTableManager();

	static void Init();
    static void Shutdown();

	static void ClearPlayerLock(u32 const index);
	static void ClearAllModelIdPlayerLocksForPlayer(u32 const playerIndex);
	static void ClearAllModelInstancePlayerLocksForPlayer(u32 const playerIndex);

	static void ClearModelIdPlayerLockForPlayer(u32 const playerIndex, u32 const modelId);
	static void ClearModelInstancePlayerLockForPlayer(u32 const playerIndex, s32 const instanceIndex);

	static int  SetModelIdPlayerLock(u32 const playerIndex, u32 const modelIndex, bool const lock);
	static int  SetModelInstancePlayerLock(u32 const playerIndex, s32 const instanceIndex, bool const lock);

	static bool	IsVehicleModelLocked(u32 const playerIndex, u32 const modelId);
	static bool IsVehicleModelRegistered(u32 const playerIndex, u32 const modelId);
	static bool	IsVehicleInstanceLocked(u32 const playerIndex, s32 const instanceIndex);
	static bool	IsVehicleInstanceRegistered(u32 const playerIndex, s32 const instanceIndex);

	static PlayerLockInfo const& GetPlayerLock(u32 const index);
	static u32					 GetNumPlayerLocks(void);
	static u32					 GetNumVehicleInstancePlayerLocks(void);
	static u32					 GetNumModelHashPlayerLocks(void);
	static s32					 GetPlayerLockIndexByModelId(u32 const playerIndex, u32 const modelId);
	static s32					 GetPlayerLockIndexByVehicleInstance(u32 const playerIndex, s32 const instanceIndex);

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG

	static bool CheckIntegrity(void);
	static void DebugRender(void);

#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

private:

	static u32 GetFreePlayerLockSlot();
	static atFixedArray<PlayerLockInfo, MAX_NUM_PLAYER_LOCK_INFO_SLOTS> ms_player_lock_entries;
};

#endif  // NETWORK_VEHICLE_MODEL_DOOR_LOCK_TABLE_H_
