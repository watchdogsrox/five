#include "NetworkVehicleModelDoorLockTable.h"
#include "ModelInfo/ModelInfo.h"
#include "Vehicles/Vehicle.h"
#include "Script\script.h"
#include "scene/world/GameWorld.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"

NETWORK_OPTIMISATIONS()

//------------------------------------------------------------------
//
//------------------------------------------------------------------

atFixedArray<CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo, CNetworkVehicleModelDoorLockedTableManager::MAX_NUM_PLAYER_LOCK_INFO_SLOTS> CNetworkVehicleModelDoorLockedTableManager::ms_player_lock_entries;

//------------------------------------------------------------------
//
//------------------------------------------------------------------

const s32 CNetworkVehicleModelDoorLockedTableManager::INVALID_INSTANCE_INDEX = -1;
const s32 CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX = -1;

//------------------------------------------------------------------
//
//------------------------------------------------------------------

CNetworkVehicleModelDoorLockedTableManager::CNetworkVehicleModelDoorLockedTableManager()
{}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

CNetworkVehicleModelDoorLockedTableManager::~CNetworkVehicleModelDoorLockedTableManager()
{
	Shutdown();
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::Init()
{
	gnetAssert(ms_player_lock_entries.empty());
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::Shutdown()
{
	ms_player_lock_entries.clear();
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::CheckIntegrity(void) const
{
	if(m_lockType == PlayerLockInfo::LockType_ModelId)
	{
		gnetAssertf(m_playerIndex	!= INVALID_PLAYER_INDEX,	"PlayerLock::CheckIntegrity - Invalid player index?!");
		gnetAssertf(m_modelId		!= fwModelId::MI_INVALID,	"PlayerLock::CheckIntegrity - Invalid model hash?!");
		
		gnetAssertf(m_vehicleIndex  == INVALID_PLAYER_INFO_INDEX,"PlayerLock::CheckIntegrity - Invalid vehicle instance index?!");

		return ((m_playerIndex != INVALID_PLAYER_INDEX) && (m_modelId != fwModelId::MI_INVALID) && (m_vehicleIndex == INVALID_PLAYER_INFO_INDEX));
	}
	else if(m_lockType == PlayerLockInfo::LockType_ModelInstance)
	{
		gnetAssertf(m_playerIndex	!= INVALID_PLAYER_INDEX,	"PlayerLock::CheckIntegrity - Invalid player index?!");
		gnetAssertf(m_modelId		== fwModelId::MI_INVALID,	"PlayerLock::CheckIntegrity - Invalid model hash?!");
		
		gnetAssertf(m_vehicleIndex   != INVALID_PLAYER_INFO_INDEX,"PlayerLock::CheckIntegrity - Invalid vehicle instance index?!");

		return ((m_playerIndex != INVALID_PLAYER_INDEX) && (m_modelId == fwModelId::MI_INVALID) && (m_vehicleIndex != INVALID_PLAYER_INFO_INDEX));
	}
	else 
	{
		gnetAssertf(IsFree(),									"PlayerLock::CheckIntegrity - Invalid lock data?!");
		return IsFree();
	}
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::DebugRender(void)
{
	CPed* playerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	
	if(playerPed)
	{
		char buffer[2000] = "\0";

		// check there are no duplicates in the array and check the integrity of each item.
		int numEntries = ms_player_lock_entries.size();
		for(int i = 0; i < numEntries; ++i)
		{
			PlayerLockInfo const& lockInfo = ms_player_lock_entries[i];

			// check the entry integrity while we're scanning for duplicates....
			lockInfo.CheckIntegrity();

			static const char* lockType[] = {"LockType_ModelId", "LockType_ModelInstance", "LockType_MaxInvalid"};

			char temp[200];
			sprintf(temp, "%d : playerIndex %d : vehicleIndex %d : modelId %d : lockType %s : locked %d\n", 
			i,
			lockInfo.GetPlayerIndex(), 
			lockInfo.GetVehicleIndex(), 
			lockInfo.GetModelHash(), 
			lockType[lockInfo.GetLockType()], 
			lockInfo.GetLocked());

			strcat(buffer, temp);

			grcDebugDraw::Text(VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()), Color_white, buffer, false);
		}
	}
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::CheckIntegrity(void)
{
	// check there are no duplicates in the array and check the integrity of each item.
	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		PlayerLockInfo const& lhs = ms_player_lock_entries[i];

		// check the entry integrity while we're scanning for duplicates....
		lhs.CheckIntegrity();

		for(int j = 0; j < numEntries; ++j)
		{
			if(i != j)
			{
				PlayerLockInfo const& rhs = ms_player_lock_entries[j];	

				if(!lhs.IsFree() && (!rhs.IsFree()))
				{
					if(lhs == rhs)
					{
						gnetAssertf(false, "ERROR : NetworkVehicleLocking : Integrity check failed - duplicate lock information in the array?!");
						return false;
					}
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::IsVehicleModelLocked(u32 const playerIndex, u32 const modelId)
{
	// no one has tried to lock or unlock this specific instance....
	int const idx = GetPlayerLockIndexByModelId(playerIndex, modelId);
	if( idx == INVALID_PLAYER_INFO_INDEX )
	{
		return false;
	}

	// we've tried to lock it or unlock it so have to check....
	gnetAssert(idx < ms_player_lock_entries.size());
	return ms_player_lock_entries[idx].m_lock;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::IsVehicleModelRegistered(u32 const playerIndex, u32 const modelId)
{
	return GetPlayerLockIndexByModelId(playerIndex, modelId) != INVALID_PLAYER_INFO_INDEX;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceRegistered(u32 const playerIndex, s32 const instanceIndex)
{
	// no one has tried to lock or unlock this specific instance....
	int idx = GetPlayerLockIndexByVehicleInstance(playerIndex, instanceIndex);
	return ( idx != INVALID_PLAYER_INFO_INDEX );
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceLocked(u32 const playerIndex, s32 const instanceIndex)
{
	// no one has tried to lock or unlock this specific instance....
	int const idx = GetPlayerLockIndexByVehicleInstance(playerIndex, instanceIndex);
	if( idx == INVALID_PLAYER_INFO_INDEX )
	{
		return false;
	}

	// we've tried to lock it or unlock it so have to check....
	gnetAssert(idx < ms_player_lock_entries.size());
	return ms_player_lock_entries[idx].m_lock;
}

//------------------------------------------------------------------
// Model type locking functions
//------------------------------------------------------------------

int CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(u32 const playerIndex, u32 const modelId, bool const lock)
{
	gnetAssert(modelId != fwModelId::MI_INVALID);

	int modelSlot = GetPlayerLockIndexByModelId(playerIndex, modelId);

	if(modelSlot == INVALID_PLAYER_INFO_INDEX)
	{
		modelSlot = GetFreePlayerLockSlot();
		gnetAssert(modelSlot != INVALID_PLAYER_INFO_INDEX);
		gnetAssert(ms_player_lock_entries[modelSlot].IsFree());
	}

	gnetAssert(modelSlot != INVALID_PLAYER_INFO_INDEX);

	if(modelSlot != INVALID_PLAYER_INFO_INDEX)
	{
		gnetAssert(modelSlot < ms_player_lock_entries.size());
		ms_player_lock_entries[modelSlot].SetModelIdLock(playerIndex, modelId, lock);

		return modelSlot;
	}

	return INVALID_PLAYER_INFO_INDEX;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::ClearPlayerLock(u32 const index)
{
	gnetAssert(index < ms_player_lock_entries.size());

	if(index < ms_player_lock_entries.size())
	{
		ms_player_lock_entries[index].Reset();
	}
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::ClearAllModelIdPlayerLocksForPlayer(u32 const playerIndex)
{
#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
		CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId)
		{
			if(ms_player_lock_entries[i].m_playerIndex == playerIndex)
			{
				ms_player_lock_entries[i].Reset();
			}
		}
	}
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::ClearAllModelInstancePlayerLocksForPlayer(u32 const playerIndex)
{
	gnetAssert(playerIndex != INVALID_PLAYER_INDEX);

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance)
			{
				if(ms_player_lock_entries[i].m_playerIndex == playerIndex)
				{
					ms_player_lock_entries[i].Reset();
				}
			}
		}
	}
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::ClearModelIdPlayerLockForPlayer(u32 const playerIndex, u32 const modelId)
{
	gnetAssert(playerIndex != INVALID_PLAYER_INDEX);
	gnetAssert(modelId != fwModelId::MI_INVALID);

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId)
			{
				if((ms_player_lock_entries[i].m_playerIndex == playerIndex) && (ms_player_lock_entries[i].m_modelId == modelId))
				{
					ms_player_lock_entries[i].Reset();
				}
			}
		}
	}	
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::ClearModelInstancePlayerLockForPlayer(u32 const playerIndex, s32 const instanceIndex)
{
	gnetAssert(playerIndex != INVALID_PLAYER_INDEX);
	gnetAssert(instanceIndex != INVALID_INSTANCE_INDEX);

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance)
			{
				if((ms_player_lock_entries[i].m_playerIndex == playerIndex) && (ms_player_lock_entries[i].m_vehicleIndex == instanceIndex))
				{
					ms_player_lock_entries[i].Reset();
				}
			}
		}
	}	
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

s32 CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(u32 const playerIndex, u32 const modelId)
{
	gnetAssert(playerIndex != INVALID_PLAYER_INDEX);
	gnetAssert(modelId != fwModelId::MI_INVALID);

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId)
			{
				if((modelId == ms_player_lock_entries[i].m_modelId) && (playerIndex == ms_player_lock_entries[i].m_playerIndex))
				{
					return i;
				}
			}
		}
	}

	return INVALID_PLAYER_INFO_INDEX;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

int CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(u32 const playerIndex, s32 const instanceIndex, bool const lock)
{
	gnetAssert(playerIndex		!= INVALID_PLAYER_INDEX);
	gnetAssert(instanceIndex	!= INVALID_INSTANCE_INDEX);

	int modelSlot = GetPlayerLockIndexByVehicleInstance(playerIndex, instanceIndex);

	if(modelSlot == INVALID_PLAYER_INFO_INDEX)
	{
		modelSlot = GetFreePlayerLockSlot();
		gnetAssert(modelSlot != INVALID_PLAYER_INFO_INDEX);
		gnetAssert(ms_player_lock_entries[modelSlot].IsFree());
	}

	gnetAssert(modelSlot != INVALID_PLAYER_INFO_INDEX);

	if(modelSlot != INVALID_PLAYER_INFO_INDEX)
	{
		gnetAssert(modelSlot < ms_player_lock_entries.size());

		ms_player_lock_entries[modelSlot].SetModelInstanceLock(playerIndex, instanceIndex, lock);

		return modelSlot;
	}

	return INVALID_PLAYER_INFO_INDEX;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

s32 CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(u32 const playerIndex, s32 const instanceIndex)
{
	gnetAssert(playerIndex != INVALID_PLAYER_INDEX);
	gnetAssert(instanceIndex != INVALID_INSTANCE_INDEX);

#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance)
			{
				if((instanceIndex == ms_player_lock_entries[i].m_vehicleIndex) && (playerIndex == ms_player_lock_entries[i].m_playerIndex))
				{
					return i;
				}		
			}
		}
	}

	return INVALID_PLAYER_INFO_INDEX;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo const& CNetworkVehicleModelDoorLockedTableManager::GetPlayerLock(u32 const index)
{
	gnetAssert(index < ms_player_lock_entries.size());
	return ms_player_lock_entries[index];
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

u32 CNetworkVehicleModelDoorLockedTableManager::GetNumPlayerLocks(void)
{
#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numLocks = 0;
	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			++numLocks;
		}
	}	

	return numLocks;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

u32 CNetworkVehicleModelDoorLockedTableManager::GetNumVehicleInstancePlayerLocks()
{
#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numInstanceLocks = 0;
	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance)
			{
				++numInstanceLocks;
			}
		}
	}	

	return numInstanceLocks;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

u32 CNetworkVehicleModelDoorLockedTableManager::GetNumModelHashPlayerLocks()
{
#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */

	int numHashLocks = 0;
	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(!ms_player_lock_entries[i].IsFree())
		{
			if(ms_player_lock_entries[i].GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId)
			{
				++numHashLocks;
			}
		}
	}	

	return numHashLocks;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

u32 CNetworkVehicleModelDoorLockedTableManager::GetFreePlayerLockSlot()
{
	int numEntries = ms_player_lock_entries.size();
	for(int i = 0; i < numEntries; ++i)
	{
		if(ms_player_lock_entries[i].IsFree())
		{
			return i;
		}
	}

	int oldsize = ms_player_lock_entries.size();
	ms_player_lock_entries.Push(PlayerLockInfo());
	return oldsize;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::PlayerLockInfo()
:
	m_playerIndex(INVALID_PLAYER_INDEX),
	m_modelId(fwModelId::MI_INVALID),
	m_vehicleIndex(INVALID_PLAYER_INFO_INDEX),
	m_lock(false),
	m_lockType(LockType_MaxInvalid)
{
#if __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG
	CheckIntegrity();	
#endif /* __DEV && NETWORK_VEHICLE_DOOR_LOCKING_DEBUG */
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::operator==(PlayerLockInfo const& rhs) const
{
	return ((m_playerIndex == rhs.m_playerIndex) && 
				(m_modelId == rhs.m_modelId) &&
					 (m_vehicleIndex == rhs.m_vehicleIndex) &&
							(m_lock == rhs.m_lock) &&
								(m_lockType == rhs.m_lockType));  	
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

bool CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::IsFree(void) const 
{
	return ((m_playerIndex == INVALID_PLAYER_INDEX) && 
				(m_modelId == fwModelId::MI_INVALID) &&
					 (m_vehicleIndex == INVALID_PLAYER_INFO_INDEX) &&
							(m_lock == false) &&
								(m_lockType == LockType_MaxInvalid));  
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::Reset(void) 
{ 
	m_playerIndex	= INVALID_PLAYER_INDEX;
	m_modelId		= fwModelId::MI_INVALID;
	m_vehicleIndex	= INVALID_PLAYER_INFO_INDEX;
	m_lock			= false;
	m_lockType		= LockType_MaxInvalid;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::SetModelIdLock(u32 const playerIndex, u32 const modelId, bool lock) 
{
	Reset();

	m_playerIndex	= playerIndex, 
	m_modelId		= modelId; 
	m_lockType		= LockType_ModelId;
	m_lock			= lock;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::SetModelInstanceLock(u32 const playerIndex, u32 const vehicleIndex, bool lock) 
{
	Reset();

	m_playerIndex	= playerIndex, 
	m_vehicleIndex	= vehicleIndex; 
	m_lock			= lock;
	m_lockType		= LockType_ModelInstance;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------