//
// name:		NetworkCarGenWorldState.cpp
// description:	Support for network scripts to switch pop multiplier areas
// written by:	Daniel Yelland
//
//
#include "NetworkVehiclePlayerLockingWorldState.h"
#include "Network/General/NetworkVehicleModelDoorLockTable.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "peds/Ped.h"

#include "fwdebug/picker.h"

#include "Network/Events/NetworkEventTypes.h"
#include "script/script.h"


//--------------------------------------------------------

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CNetworkVehiclePlayerLockingWorldState, CNetworkVehicleModelDoorLockedTableManager::MAX_NUM_PLAYER_LOCK_INFO_SLOTS, atHashString("CNetworkVehiclePlayerLockingWorldState",0xe327c0d));

//--------------------------------------------------------
// <CNetworkPopMultiplierAreaWorldStateData::CNetworkPopMultiplierAreaWorldStateData>
// RSGLDS ADW 
//--------------------------------------------------------

CNetworkVehiclePlayerLockingWorldState::CNetworkVehiclePlayerLockingWorldState()
:
	m_playerIndex(INVALID_PLAYER_INDEX),
	m_vehicleIndex(CNetworkVehicleModelDoorLockedTableManager::INVALID_INSTANCE_INDEX),
	m_locked(false),
	m_modelId(fwModelId::MI_INVALID),
	m_index(-1)
{}

//--------------------------------------------------------
//
//--------------------------------------------------------

CNetworkVehiclePlayerLockingWorldState::CNetworkVehiclePlayerLockingWorldState
(
	const CGameScriptId &scriptID,
	bool const locallyOwned,
	u32 const playerIndex,
	s32 const vehicleIndex,
	u32 const modelId,
	bool const lock,
	s32 const index
)
: CNetworkWorldStateData
(
	scriptID, 
	locallyOwned
)
{
	gnetAssert(playerIndex	!= INVALID_PLAYER_INDEX);

	m_playerIndex	= playerIndex;
	m_vehicleIndex	= vehicleIndex;
	m_locked		= lock;
	m_modelId		= modelId;
	m_index			= index;

	if(m_vehicleIndex != CNetworkVehicleModelDoorLockedTableManager::INVALID_INSTANCE_INDEX)
	{
		gnetAssert(m_modelId == fwModelId::MI_INVALID);
		m_lockType = CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance;
	}
	else if(m_modelId != fwModelId::MI_INVALID)
	{
		gnetAssert(m_vehicleIndex == CNetworkVehicleModelDoorLockedTableManager::INVALID_INSTANCE_INDEX);
		m_lockType = CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId;
	}
	else
	{
		gnetAssertf(false, "CNetworkVehiclePlayerLockingWorldState : Invalid Lock Parameters?!");
	}
}

//--------------------------------------------------------
//
//--------------------------------------------------------

bool CNetworkVehiclePlayerLockingWorldState::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
		const CNetworkVehiclePlayerLockingWorldState *vehicleData = SafeCast(const CNetworkVehiclePlayerLockingWorldState, &worldState);

		if(vehicleData)
		{
			gnetAssert(m_index != -1);
			if(m_index == vehicleData->m_index)
			{
				// this is our node (index is basically an Id here) but 
				// lets make sure the other data matches up at least
				// Don't test "locked" as that can be changed independantly
				// by CModifyVehicleLockWorldStateDataEvent

				gnetAssert(m_playerIndex == vehicleData->m_playerIndex);
				gnetAssert(m_vehicleIndex == vehicleData->m_vehicleIndex);
				gnetAssert(m_modelId == vehicleData->m_modelId);

				return true;
			}
		}
	}

	return false;
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::ClearModelIdPlayerLock
(
	const CGameScriptId &scriptID, 
	bool const fromNetwork,
	u32 const playerIndex, 
	u32 const modelId,
	bool const lock,
	int const index
)
{
	gnetAssert(playerIndex	!= INVALID_PLAYER_INDEX);
	gnetAssert(modelId		!= fwModelId::MI_INVALID);

	CNetworkVehiclePlayerLockingWorldState worldStateData(scriptID, !fromNetwork, playerIndex, -1, modelId, lock, index);
	CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, fromNetwork);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::ClearModelInstancePlayerLock
(
	const CGameScriptId &scriptID, 
	bool const fromNetwork,
	u32 const playerIndex,
	s32 const instanceIndex,
	bool const lock,
	s32 const index
)
{
	gnetAssert(playerIndex		!= INVALID_PLAYER_INDEX);
	gnetAssert(instanceIndex	!= CNetworkVehicleModelDoorLockedTableManager::INVALID_INSTANCE_INDEX);

	CNetworkVehiclePlayerLockingWorldState worldStateData(scriptID, !fromNetwork, playerIndex, instanceIndex, fwModelId::MI_INVALID, lock, index);
	CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, fromNetwork);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::SetModelIdPlayerLock
(
	const CGameScriptId &scriptID, 
	bool const fromNetwork,
	u32 const playerIndex, 
	u32 const modelId,
	bool const lock,
	s32 const index
)
{
	CNetworkVehiclePlayerLockingWorldState worldStateData(scriptID, !fromNetwork, playerIndex, -1, modelId, lock, index);
    CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::SetModelInstancePlayerLock
(
	const CGameScriptId &scriptID, 
	bool const fromNetwork,
	int const playerIndex, 
	int const instanceIndex,
	bool const lock,
	s32 const index
)
{
	CNetworkVehiclePlayerLockingWorldState worldStateData(scriptID, !fromNetwork, playerIndex, instanceIndex, fwModelId::MI_INVALID, lock, index);
    CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

CNetworkVehiclePlayerLockingWorldState* CNetworkVehiclePlayerLockingWorldState::Clone() const
{
	return rage_new CNetworkVehiclePlayerLockingWorldState(GetScriptID(), IsLocallyOwned(), m_playerIndex, m_vehicleIndex, m_modelId, m_locked, m_index);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::ChangeWorldState()
{
   NetworkLogUtils::WriteLogEvent
	(
		NetworkInterface::GetObjectManagerLog(), "CHANGING_PLAYER_VEHICLE_LOCK", 
        "%s: playerIndex %d, modelId %d, vehicleIndex %d, locked %d, lockType %d",
        GetScriptID().GetLogName(),
		m_playerIndex,
		m_modelId,
		m_vehicleIndex,
		m_locked,
		m_lockType
	);

   if(m_lockType == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance)
   {
	   NET_ASSERTS_ONLY(int iInfoIndex =) CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(m_playerIndex, m_vehicleIndex, m_locked);
	   gnetAssertf(CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX != iInfoIndex, "ChangeWorldState : Failed To Add / Update Model Instance Player Lock!?");
   }
   else if(m_lockType == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId)
   {
	   NET_ASSERTS_ONLY(int iInfoIndex =) CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(m_playerIndex, m_modelId, m_locked);
	   gnetAssertf(CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX != iInfoIndex, "ChangeWorldState : Failed To Add / Update Model Hash Player Lock!?");
   }
   else 
   {
		gnetAssert(false);
   }
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent
	(
		NetworkInterface::GetObjectManagerLog(), "REMOVING_PLAYER_VEHICLE_LOCK", 
        "%s: playerIndex %d, modelId %d, vehicleIndex %d, locked %d, lockType %d",
        GetScriptID().GetLogName(),
		m_playerIndex,
		m_modelId,
		m_vehicleIndex,
		m_locked,
		m_lockType
	);

	CNetworkVehicleModelDoorLockedTableManager::ClearPlayerLock(m_index);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::LogState(netLoggingInterface &log) const
{
	log.WriteDataValue("Script Name", "%s",		GetScriptID().GetLogName());
    log.WriteDataValue("Player Index", "%d",	GetPlayerIndex());
    log.WriteDataValue("Model Type", "%d",		GetModelId());
    log.WriteDataValue("Instance Index", "%d",	GetInstanceIndex());
    log.WriteDataValue("Locked", "%d",			GetLocked());
    log.WriteDataValue("Lock Type", "%d",		GetLockType());
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::Serialise(CSyncDataReader &serialiser)
{
	SerialiseWorldState(serialiser);
}

//--------------------------------------------------------
//
//--------------------------------------------------------
	
void CNetworkVehiclePlayerLockingWorldState::Serialise(CSyncDataWriter &serialiser)
{
	SerialiseWorldState(serialiser);
}

//--------------------------------------------------------
//
//--------------------------------------------------------

template <class Serialiser> void CNetworkVehiclePlayerLockingWorldState::SerialiseWorldState(Serialiser &serialiser)
{
	const u32 SIZEOF_PLAYER_INDEX	= 8;
	const u32 SIZEOF_MODEL_ID		= 32;
	const u32 SIZEOF_VEHICLE_INDEX	= 32;
	const u32 SIZEOF_LOCKTYPE		= 2;
	const u32 SIZEOF_INDEX			= 8;

	GetScriptID().Serialise(serialiser);
	SERIALISE_UNSIGNED(serialiser, m_playerIndex, SIZEOF_PLAYER_INDEX, "Player Index");
	SERIALISE_UNSIGNED(serialiser, m_modelId, SIZEOF_MODEL_ID, "Model Id");
	SERIALISE_UNSIGNED(serialiser, m_vehicleIndex, SIZEOF_VEHICLE_INDEX, "Instance Index");
	SERIALISE_UNSIGNED(serialiser, m_lockType, SIZEOF_LOCKTYPE, "Lock Type");
	SERIALISE_BOOL(serialiser, m_locked, "Locked");
	
	SERIALISE_UNSIGNED(serialiser, m_index,	SIZEOF_INDEX, "Index");
}

//--------------------------------------------------------
//
//--------------------------------------------------------

#if __DEV
bool CNetworkVehiclePlayerLockingWorldState::IsConflicting(const CNetworkWorldStateData& worldState) const
{
	if(gnetVerifyf(!IsEqualTo(worldState), "ERROR: IsConflicting : Trying to store equal nodes?!"))
	{
		return false;
	}

	return true;
}
#endif // __DEV

//--------------------------------------------------------
//
//--------------------------------------------------------

#if __BANK
void CNetworkVehiclePlayerLockingWorldState::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Vehicle Locking", false);
        {
            bank->AddButton("Lock picked vehicle model",		datCallback(NetworkBank_LockPickedVehicleModel));
			bank->AddButton("Unlock picked vehicle model",		datCallback(NetworkBank_UnlockPickedVehicleModel));
			bank->AddButton("Clear picked vehicle model",		datCallback(NetworkBank_ClearPickedVehicleModel));
			
			bank->AddButton("Lock picked vehicle instance",		datCallback(NetworkBank_LockPickedVehicleInstance));
			bank->AddButton("Unlock picked vehicle instance",	datCallback(NetworkBank_UnlockPickedVehicleInstance));
            bank->AddButton("Clear picked vehicle instance",	datCallback(NetworkBank_ClearPickedVehicleInstance));
        }
        bank->PopGroup();
    }
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::NetworkBank_LockPickedVehicleModel()
{
	if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetPlayerOwner())
        {
            CGameScriptId scriptID("freemode", -1);

			CEntity const* entity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle const* vehicle = static_cast<CVehicle const*>(entity);

				if(vehicle)
				{
					int playerIndex = playerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
					bool lock = true;

					int existing_index  = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(playerIndex, vehicle->GetModelId().ConvertToU32());
					if(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(playerIndex, vehicle->GetModelId().ConvertToU32(), lock);
						CModifyVehicleLockWorldStateDataEvent::Trigger(playerIndex, -1, vehicle->GetModelId().ConvertToU32(), lock);
					}
					else
					{
						int indexAdded		= CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(playerIndex, vehicle->GetModelId().ConvertToU32(), lock);
						if(indexAdded != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
						{
							CNetworkVehiclePlayerLockingWorldState::SetModelIdPlayerLock(scriptID, false, playerIndex, vehicle->GetModelId().ConvertToU32(), lock, indexAdded);
						}
					}
				}
			}
		}
	}
}

void CNetworkVehiclePlayerLockingWorldState::NetworkBank_UnlockPickedVehicleModel(void)
{
	if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetPlayerOwner())
        {
            CGameScriptId scriptID("freemode", -1);

			CEntity* entity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle const* vehicle = static_cast<CVehicle const*>(entity);

				if(vehicle)
				{
					u8 playerIndex		= playerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
					bool lock			= false;

					int existing_index  = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(playerIndex, vehicle->GetModelId().ConvertToU32());
					if(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(playerIndex, vehicle->GetModelId().ConvertToU32(), lock);
						CModifyVehicleLockWorldStateDataEvent::Trigger(playerIndex, -1, vehicle->GetModelId().ConvertToU32(), lock);
					}
					else
					{
						int indexAdded		= CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(playerIndex, vehicle->GetModelId().ConvertToU32(), lock);
						if(indexAdded != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
						{
							CNetworkVehiclePlayerLockingWorldState::SetModelIdPlayerLock(scriptID, false, playerIndex, vehicle->GetModelId().ConvertToU32(), lock, indexAdded);
						}
					}
				}
			}
		}
	}
}

void CNetworkVehiclePlayerLockingWorldState::NetworkBank_ClearPickedVehicleModel()
{
	if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetPlayerOwner())
        {
            CGameScriptId scriptID("freemode", -1);

			CEntity const* entity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle const* vehicle = static_cast<CVehicle const*>(entity);

				if(vehicle)
				{
					int playerIndex = playerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();

					int idx = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(playerIndex, vehicle->GetModelId().ConvertToU32());
					if(idx != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo const& playerLock = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLock(idx);

						CNetworkVehiclePlayerLockingWorldState::ClearModelIdPlayerLock(scriptID, false, playerIndex, vehicle->GetModelId().ConvertToU32(), playerLock.GetLocked(), idx);
						CNetworkVehicleModelDoorLockedTableManager::ClearModelIdPlayerLockForPlayer(playerIndex, vehicle->GetModelId().ConvertToU32());
					}
				}
			}
		}
	}
}

void CNetworkVehiclePlayerLockingWorldState::NetworkBank_LockPickedVehicleInstance()
{
	if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetPlayerOwner())
        {
            CGameScriptId scriptID("freemode", -1);

			CEntity* entity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle const* vehicle = static_cast<CVehicle const*>(entity);

				if(vehicle)
				{
					int playerIndex		= playerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
					int vehicleIndex	= CTheScripts::GetGUIDFromEntity(*entity);
					bool lock			= true;

					int existing_index  = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(playerIndex, vehicleIndex);
					if(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(playerIndex, vehicleIndex, lock);
						CModifyVehicleLockWorldStateDataEvent::Trigger(playerIndex, vehicleIndex, fwModelId::MI_INVALID, lock);
					}
					else
					{
						int indexAdded		= CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(playerIndex, vehicleIndex, lock);
						if(indexAdded != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
						{
							CNetworkVehiclePlayerLockingWorldState::SetModelInstancePlayerLock(scriptID, false, playerIndex, vehicleIndex, lock, indexAdded);
						}
					}
				}
			}
		}
	}
}

void CNetworkVehiclePlayerLockingWorldState::NetworkBank_UnlockPickedVehicleInstance(void)
{
	if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetPlayerOwner())
        {
            CGameScriptId scriptID("freemode", -1);

			CEntity* entity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle const* vehicle = static_cast<CVehicle const*>(entity);

				if(vehicle)
				{
					int playerIndex		= playerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
					int vehicleIndex	= CTheScripts::GetGUIDFromEntity(*entity);
					bool lock			= false;

					int existing_index  = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(playerIndex, vehicleIndex);
					if(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(playerIndex, vehicleIndex, lock);
						CModifyVehicleLockWorldStateDataEvent::Trigger(playerIndex, vehicleIndex, fwModelId::MI_INVALID, lock);
					}
					else
					{
						int indexAdded		= CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(playerIndex, vehicleIndex, lock);
						if(indexAdded != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
						{
							CNetworkVehiclePlayerLockingWorldState::SetModelInstancePlayerLock(scriptID, false, playerIndex, vehicleIndex, lock, indexAdded);
						}
					}
				}
			}
		}
	}
}

void CNetworkVehiclePlayerLockingWorldState::NetworkBank_ClearPickedVehicleInstance()
{
	if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && playerPed->GetNetworkObject() && playerPed->GetNetworkObject()->GetPlayerOwner())
        {
            CGameScriptId scriptID("freemode", -1);

			CEntity* entity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle const* vehicle = static_cast<CVehicle const*>(entity);

				if(vehicle)
				{
					int playerIndex		= playerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
					int vehicleIndex	= CTheScripts::GetGUIDFromEntity(*entity);

					int idx = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(playerIndex, vehicleIndex);

					if(idx != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo const& playerLock = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLock(idx);

						CNetworkVehiclePlayerLockingWorldState::ClearModelInstancePlayerLock(scriptID, false, playerIndex, vehicleIndex, playerLock.GetLocked(), idx);
						CNetworkVehicleModelDoorLockedTableManager::ClearModelInstancePlayerLockForPlayer(playerIndex, vehicleIndex);
					}
				}
			}
		}
	}
}

//--------------------------------------------------------
//
//--------------------------------------------------------

void CNetworkVehiclePlayerLockingWorldState::DisplayDebugText()
{
	CPed* pPed = FindPlayerPed();
	if(!pPed)
	{
		return;
	}

	char buffer[2560] = "\0";

	CNetworkVehiclePlayerLockingWorldState::Pool *pool = CNetworkVehiclePlayerLockingWorldState::GetPool();
	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkVehiclePlayerLockingWorldState *worldState = pool->GetSlot(i);

		if(worldState)
		{
            u32 PlayerIndex		= worldState->GetPlayerIndex();
			u32 ModelType		= worldState->GetModelId();
			u32 InstanceIndex	= worldState->GetInstanceIndex();
			bool Locked			= worldState->GetLocked();
			u8 LockType			= worldState->GetLockType();

			static const char* lockType[] = {"LockType_ModelId", "LockType_ModelInstance", "LockType_MaxInvalid"};

			char temp[200];
			sprintf(temp, "%d : Vehicle Locking WorldState: %s : PlayerIndex %d : ModelType %d : InstanceIndex %d : Locked %d : LockType %s\n", 
				i,
				worldState->GetScriptID().GetLogName(),
				PlayerIndex,
				ModelType,
				InstanceIndex,
				Locked,
				lockType[LockType]);
			strcat(buffer, temp);
			/*
            grcDebugDraw::AddDebugOutput
			(
				"Vehicle Locking WorldState: %s : PlayerIndex %d : ModelType %d : InstanceIndex %d : Locked %d : LockType %d", 
				worldState->GetScriptID().GetLogName(),
				PlayerIndex,
				ModelType,
				InstanceIndex,
				Locked,
				LockType
			);*/
		}
    }

	Vector3 finalPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 1.0f);

	grcDebugDraw::Text(finalPos, Color_white, buffer, true);
}

#endif // __BANK

