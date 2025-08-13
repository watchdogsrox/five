//
// name:		NetObjDoor.h
// description:	Derived from netObject, this class handles network of the free moving hinged DOOR_TYPE_STD type
//				door objects. See NetworkObject.h for a full description of all the methods.
//

#include "network/Objects/entities/NetObjDoor.h"

// game headers
#include "Camera/CamInterface.h"
#include "Network/Debug/NetworkDebug.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "Objects/Door.h"
#include "peds/Ped.h"
#include "renderer/DrawLists/drawListNY.h"
#include "script/Handlers/GameScriptIds.h"
#include "script/Handlers/GameScriptMgr.h"
#include "script/script.h"
#include "streaming/streaming.h"

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CNetObjDoor, MAX_NUM_NETOBJDOORS, atHashString("CNetObjDoor",0x3c787bb2));
FW_INSTANTIATE_CLASS_POOL(CNetObjDoor::CDoorSyncData, MAX_NUM_NETOBJDOORS, atHashString("CDoorSyncData",0xe6df226c));

CDoorSyncTree*  CNetObjDoor::ms_doorSyncTree;
CNetObjDoor::CDoorScopeData	CNetObjDoor::ms_doorScopeData;

CNetObjDoor::CNetObjDoor(	CDoor *						door,                             
							const NetworkObjectType		type,
							const ObjectId				objectID,
							const PhysicalPlayerIndex	playerIndex,
							const NetObjFlags			localFlags,
							const NetObjFlags			globalFlags)
: CNetObjObject(door, type, objectID, playerIndex, localFlags, globalFlags)
, m_doorSystemHash(0)
, m_ModelHash(0)
, m_Position(VEC3_ZERO)
, m_fTargetOpenRatio(0.0f)
, m_closedTimer(0)
, m_migrationTimer(TIME_BETWEEN_OUT_OF_SCOPE_MIGRATION_ATTEMPTS)
, m_pDoorSystemData(NULL)
, m_bFlagForUnregistration(false)
, m_bOpening(false)
{
	SetGameObject(NULL);
	SetGameObject(&m_gameObjectWrapper);

	if (door)
	{
		m_gameObjectWrapper.SetDoor(*door);

		CBaseModelInfo *modelInfo = door->GetBaseModelInfo();
		if(gnetVerifyf(modelInfo, "No model info for networked object!"))
		{
			m_ModelHash = modelInfo->GetHashKey();
		}

		m_Position = door->GetDoorStartPosition();

		if (door->GetDoorSystemData() && AssertVerify(!door->GetDoorSystemData()->GetNetworkObject()))
		{
			m_pDoorSystemData = door->GetDoorSystemData();

			m_doorSystemHash = m_pDoorSystemData->GetEnumHash();

			if (ValidateDoorSystemData("Construction", false))
			{
				m_pDoorSystemData->SetNetworkObject(this);
			}

			m_Position = door->GetDoorSystemData()->GetPosition();
		}
	}
}

CNetObjDoor::~CNetObjDoor()
{
	if (m_pDoorSystemData && ValidateDoorSystemData("Destruction", true, true))
	{
		m_pDoorSystemData->SetNetworkObject(NULL);
	}

	if (GetDoor())
	{
		if (GetDoor()->GetNetworkObject())
		{
			if (GetDoor()->GetNetworkObject() == this)
			{
				GetDoor()->SetNetworkObject(NULL);
			}
			else
			{
				gnetAssertf(0, "%s is being destroyed while still pointing to a door, which has a different network object (%s)", GetLogName(), GetDoor()->GetNetworkObject()->GetLogName());
			}
		}
		else
		{
			gnetAssertf(0, "%s is being destroyed while still pointing to a door, which has no network object", GetLogName());
		}
	}

	SetGameObject(NULL);
}

void CNetObjDoor::AssignDoor(CDoor* pDoor)
{
	CDoor* pPrevDoor = m_gameObjectWrapper.GetDoor();

	// this should only be called on script doors
	gnetAssert(IsScriptObject());

	netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

	// the game object either points to a physical door or a locked status
	if (pDoor)
	{
		if (pDoor != pPrevDoor)
		{
			NetworkLogUtils::WriteLogEvent(log, "ASSIGNING_DOOR", GetLogName());
			
			Vector3 doorPos = VEC3V_TO_VECTOR3(pDoor->GetTransform().GetPosition());
			log.WriteDataValue("Door", "%p", pDoor);
			log.WriteDataValue("Position", "%f, %f, %f", doorPos.x, doorPos.y, doorPos.z);

			if (pPrevDoor)
			{
#if __ASSERT
				Vector3 prevDoorPos = VEC3V_TO_VECTOR3(pPrevDoor->GetTransform().GetPosition());
				Vector3 diff = doorPos - prevDoorPos;

				if (diff.Mag() < 0.01f)
				{
					gnetAssertf(0, "CNetObjDoor::AssignDoor - two doors found at the same location (%0.2f, %0.2f, %0.2f) when trying to assign to %s", doorPos.x, doorPos.y, doorPos.z, GetLogName());
				}
				else
				{
					gnetAssertf(0, "CNetObjDoor::AssignDoor - assigning a door at %0.2f, %0.2f, %0.2f to %s, which already has a door at %0.2f, %0.2f, %0.2f", doorPos.x, doorPos.y, doorPos.z, GetLogName(), prevDoorPos.x, prevDoorPos.y, prevDoorPos.z);
				}
#endif // ASSERT
				pPrevDoor->SetNetworkObject(NULL);
			}

			m_gameObjectWrapper.SetDoor(*pDoor);

			pDoor->SetNetworkObject(this);
		}
	}
	else if (pPrevDoor)
	{
		NetworkLogUtils::WriteLogEvent(log, "UNASSIGNING_DOOR", GetLogName());

		if (AssertVerify(pPrevDoor->GetNetworkObject() == this))
		{
			pPrevDoor->SetNetworkObject(NULL);
		}

		if (AssertVerify(m_pDoorSystemData))
		{
			m_gameObjectWrapper.SetDoorData(*m_pDoorSystemData);
			gnetAssert(m_pDoorSystemData->GetNetworkObject() == this);
		}
	}
}

bool CNetObjDoor::IsFullyOpen() const
{
	gnetAssert(IsClone());

	CDoor* pDoor = GetDoor();

	// use the door if we have one
	if (pDoor)
	{
		return pDoor->GetDoorOpenRatio() > 0.99f;
	}

	// else fall back to last received update
	return m_bFullyOpen;
}


bool CNetObjDoor::IsClosed() const 
{
	gnetAssert(IsClone());

	CDoor* pDoor = GetDoor();

	// use the door if we have one
	if (pDoor)
	{
		return pDoor->IsDoorShut();
	}

	// else fall back to last received update
	return m_bClosed;
}

void CNetObjDoor::SetDoorSystemData(CDoorSystemData* data)
{
	if (data)
	{
		if (gnetVerifyf(!m_pDoorSystemData || m_pDoorSystemData == data, "%s : SetDoorSystemData - This object is already assigned to door system data %d", GetLogName(), m_pDoorSystemData->GetEnumHash()))
		{
			m_pDoorSystemData = data;
			m_doorSystemHash = data->GetEnumHash();

			if (ValidateDoorSystemData("SetDoorSystemData", false))
			{
				data->SetNetworkObject(this);

				m_Position = data->GetPosition();
				m_ModelHash = data->GetModelInfoHash();

				if (!m_gameObjectWrapper.GetDoor())
				{
					m_gameObjectWrapper.SetDoorData(*data);
				}
			}
			else
			{
				m_pDoorSystemData = NULL;
				m_doorSystemHash = 0;

				if (!m_gameObjectWrapper.GetDoor())
				{
					m_gameObjectWrapper.Clear();
				}
			}
		}
		else
		{
			LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s : SetDoorSystemData - This object is already assigned to door system data %d ##\r\n", GetLogName(), m_pDoorSystemData->GetEnumHash()));
		}
	}
	else
	{
		m_pDoorSystemData = NULL;
		
		if (!m_gameObjectWrapper.GetDoor())
		{
			m_gameObjectWrapper.Clear();
		}
	}
}

extern bool removeAllDoors;

bool CNetObjDoor::Update()
{
	CDoor* pDoor = GetDoor();

	if (pDoor)
	{
		gnetAssertf(pDoor->GetNetworkObject(), "%s is assigned to a CDoor that has no network object ptr", GetLogName());
		gnetAssertf(!pDoor->GetNetworkObject() || pDoor->GetNetworkObject() == this, "%s is assigned to a CDoor that has network object ptr to %s", GetLogName(), pDoor->GetNetworkObject()->GetLogName());

		if (!pDoor->GetNetworkObject() || pDoor->GetNetworkObject() != this)
		{
			if (AssertVerify(m_pDoorSystemData) && AssertVerify(m_pDoorSystemData == m_gameObjectWrapper.GetDoorData()))
			{
				m_gameObjectWrapper.SetDoorData(*m_pDoorSystemData);
			}
			else
			{
				m_gameObjectWrapper.Clear();
			}
		}
	}

	ValidateDoorSystemData("Update");

	if (!m_gameObjectWrapper.GetDoor() && m_gameObjectWrapper.GetDoorData() != m_pDoorSystemData)
	{
		gnetAssertf(0, "%s has mismatching door system data", GetLogName());
		LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s has mismatching door system data ##\r\n", GetLogName()));
	}

	// stop networking the door if possible
	if (m_pDoorSystemData && !IsClone() && !IsPendingOwnerChange() && m_pDoorSystemData->CanStopNetworking() && IsSyncedWithAllPlayers())
	{
		return true;
	}	
	
	if (m_bFlagForUnregistration && !IsClone() && !IsPendingOwnerChange())
	{
		return true;
	}
	else if (pDoor)
	{
		bool bRet = CNetObjObject::Update();

		if (IsClone())
		{
			if (!pDoor->IsHingedDoorType() && !pDoor->GetDoorSystemData())
			{
				if (m_bOpening)
				{
					pDoor->OpenDoor();
				}
				else
				{
					pDoor->CloseDoor();
				}
			}
		}
		else 
		{
			// unregister non-script doors after they have been shut for a while
			if (!IsScriptObject() && !IsPendingOwnerChange() && pDoor->GetIsStatic() && pDoor->IsDoorShut() && IsSyncedWithAllPlayers())
			{
				m_closedTimer += fwTimer::GetSystemTimeStepInMilliseconds();

				if (m_closedTimer >= 1000)
				{
					// B*1912949: Changed this from an early out to prevent non-script doors from being removed if they are in the door system (Via the check below).
					// Throwing a sticky bomb on to a door causes it to be registered as a network object.
					// If the door is shut and not a script object (e.g. Doors to Michael's house), it'll get unregistered as a network object and eventually removed from the door system.				
					bRet = true;
				}
			}
			else
			{
				m_closedTimer = 0;
			}

			m_migrationTimer = 0;
		}

		// we can't remove the door if it has a door system entry	
		// Removing doors from the door system forces them to become unlocked and therefore allows access to areas players shouldn't be able to get into.
		if (bRet && m_pDoorSystemData)
		{
			bRet = false;
		}

		return bRet;
	}
	else
	{
		// if we have no door try and pass control to the nearest player every so often
		if (!IsClone())
		{
			if (m_migrationTimer > 0)
			{
				m_migrationTimer -= fwTimer::GetTimeStepInMilliseconds();
			}
			else 
			{
				if (!IsPendingOwnerChange())
				{
					TryToPassControlOutOfScope();
				}

				m_migrationTimer = TIME_BETWEEN_OUT_OF_SCOPE_MIGRATION_ATTEMPTS;
			}
		}
		else
		{
			m_migrationTimer = 0;
		}

		return CNetObjProximityMigrateable::Update();
	}
}

void CNetObjDoor::UpdateBeforePhysics()
{
	static float blendSpeed = 0.2f;

	if (IsClone())
	{
		CDoor* pDoor = GetDoor();

		if (pDoor && pDoor->IsHingedDoorType() && pDoor->GetCurrentPhysicsInst() && !pDoor->IsBaseFlagSet(fwEntity::IS_FIXED))
		{
			float currentOpenRatio = pDoor->GetDoorOpenRatio();

			float diff = m_fTargetOpenRatio - currentOpenRatio;

			pDoor->SetTargetDoorRatio(currentOpenRatio + diff*blendSpeed, true);
		}
	}
}

void CNetObjDoor::CreateSyncTree()
{
	ms_doorSyncTree = rage_new CDoorSyncTree();
}

void CNetObjDoor::DestroySyncTree()
{
	ms_doorSyncTree->ShutdownTree();
	delete ms_doorSyncTree;
	ms_doorSyncTree = 0;
}

// sync parser getter functions
void CNetObjDoor::GetDoorCreateData(CDoorCreationDataNode& data) const
{
	data.m_scriptDoor	= IsScriptObject();

	data.m_ModelHash	= m_ModelHash;
	data.m_Position		= m_Position;
	data.m_playerWantsControl	= false;

	if (!data.m_scriptDoor)
	{
		data.m_playerWantsControl = (GetDoor() ? GetDoor()->m_nObjectFlags.bWeWantOwnership == 1 : false);
	}
}

void CNetObjDoor::GetDoorScriptInfo(CDoorScriptInfoDataNode& data) const
{
	CNetObjObject::GetEntityScriptInfo(data.m_hasScriptInfo, data.m_scriptInfo);

	if (IsScriptObject() && !IsLocalFlagSet(CNetObjGame::LOCALFLAG_BEINGREASSIGNED))
	{
		gnetAssertf(data.m_hasScriptInfo, "%s has no associated script!", GetLogName());
		gnetAssertf(m_pDoorSystemData, "%s has no associated door system entry!", GetLogName());
	}

	if (m_pDoorSystemData)
	{
		data.m_doorSystemHash = m_pDoorSystemData->GetEnumHash();
		data.m_existingScriptDoor = m_pDoorSystemData->GetPersistsWithoutNetworkObject();
	}
	else
	{
		data.m_doorSystemHash = 0;
		data.m_existingScriptDoor = false;
	}
}

void CNetObjDoor::GetDoorScriptGameState(CDoorScriptGameStateDataNode& data) const
{
	if (IsScriptObject() && !IsLocalFlagSet(CNetObjGame::LOCALFLAG_BEINGREASSIGNED))
	{
		gnetAssertf(m_pDoorSystemData, "%s has no associated door system entry!", GetLogName());
	}

	if (m_pDoorSystemData)
	{
		if (m_pDoorSystemData->GetPendingState() != DOORSTATE_INVALID)
		{
			data.m_DoorSystemState = m_pDoorSystemData->GetPendingState();
		}
		else
		{
			gnetAssert(m_pDoorSystemData->GetState() != DOORSTATE_INVALID);
			data.m_DoorSystemState = m_pDoorSystemData->GetState();
		}

		data.m_AutomaticDist	= m_pDoorSystemData->GetAutomaticDistance();
		data.m_AutomaticRate	= m_pDoorSystemData->GetAutomaticRate();
		data.m_BrokenFlags		= (u32)m_pDoorSystemData->GetBrokenFlags();
		data.m_DamagedFlags		= (u32)m_pDoorSystemData->GetDamagedFlags();
		data.m_HoldOpen			= (u32)m_pDoorSystemData->GetHoldOpen();
	}
	else
	{
		data.m_DoorSystemState	= DOORSTATE_UNLOCKED;
		data.m_AutomaticDist	= 0.0f;
		data.m_AutomaticRate	= 0.0f;
		data.m_BrokenFlags		= 0;
		data.m_DamagedFlags		= 0;
	}
}

void CNetObjDoor::GetDoorMovementData(CDoorMovementDataNode& data) const
{
	data.m_bHasOpenRatio	= false;
	data.m_fOpenRatio		= 0.0f;
	data.m_bOpening			= false;
	data.m_bFullyOpen		= false;
	data.m_bClosed			= false;

	CDoor* pDoor = GetDoor();

	if (pDoor)
	{
		data.m_bHasOpenRatio = pDoor->IsHingedDoorType();

		if (data.m_bHasOpenRatio)
		{
			data.m_fOpenRatio = pDoor->GetDoorOpenRatio();

			// this needs to be clamped as it can often exceed its limits slightly
			data.m_fOpenRatio = rage::Min(data.m_fOpenRatio, 1.0f);
			data.m_fOpenRatio = rage::Max(data.m_fOpenRatio, -1.0f);
		}
		else 
		{
			data.m_bOpening = pDoor->IsDoorOpening();
			data.m_bFullyOpen = pDoor->GetDoorOpenRatio() > 0.99f;
			data.m_bClosed = pDoor->IsDoorShut();
		}
	}
}

// sync parser setter functions
void CNetObjDoor::SetDoorCreateData(const CDoorCreationDataNode& data) 
{
	m_ModelHash = data.m_ModelHash;
	m_Position  = data.m_Position;

	if (data.m_pDoorObject)
	{
		m_gameObjectWrapper.SetDoor(*data.m_pDoorObject);
		data.m_pDoorObject->SetNetworkObject(this);
	}
}

void CNetObjDoor::SetDoorScriptInfo(CDoorScriptInfoDataNode& data)
{
	if (data.m_hasScriptInfo)
	{
		if (GetScriptObjInfo())
		{
			gnetAssert(*GetScriptObjInfo() == data.m_scriptInfo);
			gnetAssert(m_pDoorSystemData && m_pDoorSystemData->GetEnumHash() == (int)data.m_doorSystemHash);
		}
		else
		{
			if (AssertVerify(data.m_doorSystemHash))
			{
				m_doorSystemHash = data.m_doorSystemHash;

				CDoorSystemData* pDoorSystemData = CDoorSystem::GetDoorData((int)data.m_doorSystemHash);

				if (pDoorSystemData)
				{
					if (!pDoorSystemData->GetNetworkObject())
					{
						if (!data.m_existingScriptDoor)
						{
							gnetAssertf(0, "Door system entry found with no network object. Position: %f, %f, %f", pDoorSystemData->GetPosition().x, pDoorSystemData->GetPosition().y, pDoorSystemData->GetPosition().z);
						}
						else
						{
							gnetAssert(pDoorSystemData->GetPersistsWithoutNetworkObject());
							pDoorSystemData->SetNetworkObject(this);
						}
					}
					else if (pDoorSystemData->GetNetworkObject() != this)
					{
						gnetAssertf(0, "Trying to assign a door system entry to %s, which is already assigned to %s", GetLogName(), pDoorSystemData->GetNetworkObject()->GetLogName());
						return;
					}

					gnetAssert(pDoorSystemData->GetModelInfoHash() == (int)m_ModelHash);
					gnetAssert(pDoorSystemData->GetPosition().IsClose(m_Position, 1.0f));						
				}
				else if (!data.m_existingScriptDoor)
				{
					pDoorSystemData = CDoorSystem::AddDoor(data.m_doorSystemHash, m_ModelHash, m_Position);
				}

				if (AssertVerify(pDoorSystemData))
				{
					SetDoorSystemData(pDoorSystemData);

					SetScriptObjInfo(&data.m_scriptInfo);

					scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(data.m_scriptInfo.GetScriptId());

					if (pHandler && pHandler->GetNetworkComponent())
					{
						if (!pDoorSystemData->GetScriptHandler())
						{
							pHandler->RegisterExistingScriptObject(*pDoorSystemData);
						}
						else
						{
							gnetAssertf(pDoorSystemData->GetScriptHandler() == pHandler, "%s is aleady registered to a different script handler (%s)", GetLogName(), pHandler->GetLogName());
						}
					}
				}
			}
		}
	}
	else if (GetScriptObjInfo() && AssertVerify(m_pDoorSystemData))
	{
		scriptHandler* pHandler = m_pDoorSystemData->GetScriptHandler();

		if (pHandler)
		{
			if (ValidateDoorSystemData("SetDoorScriptInfo"))
			{
				pHandler->UnregisterScriptObject(*m_pDoorSystemData);

				if (m_pDoorSystemData)
				{
					m_pDoorSystemData->SetNetworkObject(NULL);
				}
			}

			SetDoorSystemData(NULL);
		}
	}
}

void CNetObjDoor::SetDoorScriptGameState(const CDoorScriptGameStateDataNode& data) 
{
	if (m_pDoorSystemData)
	{
		m_pDoorSystemData->SetPendingState((DoorScriptState)data.m_DoorSystemState);
		m_pDoorSystemData->SetAutomaticRate(data.m_AutomaticDist);
		m_pDoorSystemData->SetAutomaticDistance(data.m_AutomaticDist);
		m_pDoorSystemData->SetBrokenFlags(static_cast<DoorBrokenFlags>(data.m_BrokenFlags));
		m_pDoorSystemData->SetDamagedFlags(static_cast<DoorDamagedFlags>(data.m_DamagedFlags));
		m_pDoorSystemData->SetHoldOpen(data.m_HoldOpen, true);

		CDoorSystem::SetPendingStateOnDoor(*m_pDoorSystemData);

		if(m_pDoorSystemData->GetDoor())
		{
			if (data.m_BrokenFlags != 0)
			{
				CDoorSystem::BreakDoor(*m_pDoorSystemData->GetDoor(), static_cast<DoorBrokenFlags>(data.m_BrokenFlags), false);
			}

			if (data.m_DamagedFlags != 0)
			{
				CDoorSystem::DamageDoor(*m_pDoorSystemData->GetDoor(), static_cast<DoorDamagedFlags>(data.m_DamagedFlags));
			}
		}

		// set the target ratio based on the door system state
		switch (data.m_DoorSystemState)
		{
		case DOORSTATE_INVALID:
		case DOORSTATE_UNLOCKED:
		case DOORSTATE_FORCE_UNLOCKED_THIS_FRAME:
			{
				// HACK GTAV : bug 2184520 - found an unlocked door which is still fixed
				CDoor* pDoor = m_pDoorSystemData->GetDoor();
				if (pDoor)
				{
					if (pDoor->IsBaseFlagSet(fwEntity::IS_FIXED))
					{
						netAssertf(false, "Unlocking state (%d) for door (%s) has left it fixed! Unfixing...", data.m_DoorSystemState, GetLogName() );
						pDoor->AssignBaseFlag(fwEntity::IS_FIXED, false);
					}
				}
			}
			break;
		case DOORSTATE_LOCKED:
		case DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA:
		case DOORSTATE_FORCE_LOCKED_THIS_FRAME:
		case DOORSTATE_FORCE_CLOSED_THIS_FRAME:
			m_fTargetOpenRatio = 0.0f;
			break;
		case DOORSTATE_FORCE_OPEN_THIS_FRAME:
			m_fTargetOpenRatio = 1.0f;
			break;
		default:
			gnetAssertf(0, "CNetObjDoor::SetDoorScriptGameState: Unrecognised door system state %d", data.m_DoorSystemState);
		}
	}
}

void CNetObjDoor::SetDoorMovementData(const CDoorMovementDataNode& data)
{
	if (data.m_bHasOpenRatio)
	{
		m_fTargetOpenRatio = data.m_fOpenRatio;
	}
	else
	{
		m_bOpening		= data.m_bOpening;
		m_bFullyOpen	= data.m_bFullyOpen;
		m_bClosed		= data.m_bClosed;
	}
}

scriptObjInfoBase* CNetObjDoor::GetScriptObjInfo() 
{ 
	gnetAssertf(!m_pDoorSystemData || m_pDoorSystemData->GetScriptInfo(), "%s has a door system entry but no associated script!", GetLogName());

	return m_pDoorSystemData ? m_pDoorSystemData->GetScriptInfo() : NULL; 
}

const scriptObjInfoBase* CNetObjDoor::GetScriptObjInfo() const 
{ 
	return m_pDoorSystemData ? m_pDoorSystemData->GetScriptInfo() : NULL; 
}

void CNetObjDoor::SetScriptObjInfo(scriptObjInfoBase* info) 
{ 
	if (AssertVerify(m_pDoorSystemData)) 
	{
		m_pDoorSystemData->SetScriptObjInfo(info); 
	}
}

void CNetObjDoor::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	CDoor* pDoor = GetDoor();
	bool bWasClone = IsClone();

	CNetObjObject::ChangeOwner(player, migrationType);

	if (pDoor)
	{
		if (!IsClone())
		{
			GetDoor()->ClearUnderCodeControl();
		}
		else if (bWasClone)
		{
			CDoorMovementDataNode doorMovementDataNode;
			GetDoorMovementData(doorMovementDataNode);

			if (doorMovementDataNode.m_bHasOpenRatio)
			{
				m_fTargetOpenRatio = doorMovementDataNode.m_fOpenRatio;
			}
		}
	}
}


void CNetObjDoor::OnRegistered()
{
	netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

	log.WriteDataValue("Position", "%f, %f, %f", m_Position.x, m_Position.y, m_Position.z);
	log.WriteDataValue("Model hash", "%d", m_ModelHash);

	if (m_pDoorSystemData)
	{
		log.WriteDataValue("Door system hash", "%u", m_pDoorSystemData->GetEnumHash());

		CSyncDataLogger serialiser(&NetworkInterface::GetObjectManager().GetLog());
		u32 state = (u32)m_pDoorSystemData->GetInitialState();
		SERIALISE_DOOR_SYSTEM_STATE(serialiser, state, "Pre-networked state");
	}
}

void CNetObjDoor::OnUnregistered()
{
	if (m_pDoorSystemData)
	{
		CSyncDataLogger serialiser(&NetworkInterface::GetObjectManager().GetLog());
		u32 state = (u32)m_pDoorSystemData->GetInitialState();
		SERIALISE_DOOR_SYSTEM_STATE(serialiser, state, "Pre-networked state");
	}

	CNetObjPhysical::OnUnregistered();
}

void CNetObjDoor::CleanUpGameObject(bool UNUSED_PARAM(bDestroyObject))
{
	if (m_pDoorSystemData)
	{
		if (ValidateDoorSystemData("CleanUpGameObject"))
		{
			m_pDoorSystemData->SetNetworkObject(NULL);

			scriptHandler* pHandler = m_pDoorSystemData->GetScriptHandler();

			if (pHandler)
			{
				if (!m_pDoorSystemData->GetPersistsWithoutNetworkObject())
				{
					pHandler->UnregisterScriptObject(*m_pDoorSystemData);
				}
			}
			else
			{
				m_pDoorSystemData->SetFlagForRemoval();
			}
		}

		SetDoorSystemData(NULL);
	}

	if (m_gameObjectWrapper.GetDoor())
	{
		m_gameObjectWrapper.GetDoor()->SetNetworkObject(NULL);
	}

	m_gameObjectWrapper.Clear();

	SetGameObject(0);
}

// this differs from CNetObjProximityMigrateable::TryToPassControlOutOfScope in that we only pass out of scope doors on to machines that have the 
// physical door (within the sope distance)
bool CNetObjDoor::TryToPassControlOutOfScope()
{
	if (!NetworkInterface::IsGameInProgress())
		return true;

	gnetAssert(!IsClone());

	if (IsPendingOwnerChange())
		return false;

	if(!HasBeenCloned())
	{
		return true;
	}

	if (IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER))
		return false;

	const netPlayer* pNearestPlayer = NULL;

	// find closest player and pass control to it
	Vector3 diff;
	float closestDist2 = -1.0f;

	float scopeDist = GetScopeDistance() * 0.9f;
	float scopeDistSqr = scopeDist*scopeDist;

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

		if (HasBeenCloned(*remotePlayer))
		{
            bool isInScope = (GetInScopeState() & (1<<remotePlayer->GetPhysicalPlayerIndex())) != 0;

            if(isInScope)
            {
			    diff = NetworkUtils::GetNetworkPlayerPosition(*remotePlayer) - GetPosition();
			    const float dist2 = diff.XYMag2();

			    if (dist2 < scopeDistSqr && (!pNearestPlayer || dist2 < closestDist2))
			    {
				    if (CanPassControl(*remotePlayer, MIGRATE_OUT_OF_SCOPE) &&
					    NetworkInterface::GetObjectManager().RemotePlayerCanAcceptObjects(*remotePlayer, true))
				    {
					    pNearestPlayer = remotePlayer;
					    closestDist2 = dist2;
				    }
			    }
            }
		}
	}

	if (pNearestPlayer)
	{
		CGiveControlEvent::Trigger(*pNearestPlayer, this, MIGRATE_OUT_OF_SCOPE);
	}

	return false;
}

bool CNetObjDoor::OnReassigned()
{
	bool bRet = CNetObjObject::OnReassigned();

	// remove doors reassigned locally that have no associated script info
	if (!GetScriptObjInfo() && (IsGlobalFlagSet(GLOBALFLAG_SCRIPT_MIGRATION) || IsGlobalFlagSet(GLOBALFLAG_CLONEONLY_SCRIPT)))
	{
		bRet = true;
	}

	return bRet;
}

void CNetObjDoor::ConvertToAmbientObject()
{
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CONVERT_TO_AMBIENT", "%s cleaned up", GetLogName());

	if (m_pDoorSystemData)
	{
		if (ValidateDoorSystemData("Convert to ambient"))
		{
			m_pDoorSystemData->SetNetworkObject(NULL);
			m_pDoorSystemData->SetFlagForRemoval();
		}
	
		SetDoorSystemData(NULL);
	}

	// flag the door for unregistration if it is not large enough to be normally synced
	CDoor* pDoor = GetDoor();

	if (pDoor)
	{
		// the code control flag is set on clones and needs to be cleared to prevent the door being left open
		pDoor->ClearUnderCodeControl();
	}

	if (!pDoor || !CNetObjObject::HasToBeCloned(pDoor))
	{
		m_bFlagForUnregistration = true;
	}
}

bool CNetObjDoor::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	bool bCanAcceptControl = CNetObjObject::CanAcceptControl(player, migrationType, resultCode);

	if (bCanAcceptControl)
	{
		// can't accept control of an object that is set to only migrate to other machines running the same script if we are not running that script.
		if (!GetScriptObjInfo() && (IsGlobalFlagSet(GLOBALFLAG_SCRIPT_MIGRATION) || IsGlobalFlagSet(GLOBALFLAG_CLONEONLY_SCRIPT)))
		{
			if(resultCode)
			{
				*resultCode = CAC_FAIL_NOT_SCRIPT_PARTICIPANT;
			}
			
			bCanAcceptControl = false;
		}
	}

	return bCanAcceptControl;
}

netINodeDataAccessor *CNetObjDoor::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == IDoorNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (IDoorNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjObject::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}

void CNetObjDoor::DisplayNetworkInfo()
{
#if __BANK
	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	const unsigned DEBUG_STR_LEN = 200;
	char debugStr[DEBUG_STR_LEN];

	float   scale = 1.0f;
	Vector2 screenCoords;

	if (!HasGameObject())
		return;

	if(NetworkUtils::GetScreenCoordinatesForOHD(m_Position, screenCoords, scale))
	{
		DisplaySyncTreeInfoForObject();

		if (camInterface::IsSphereVisibleInGameViewport(m_Position, 1.0f))
		{
			const float DEBUG_TEXT_Y_INCREMENT   = (scale * 0.025f); // amount to adjust screen coordinates to move down a line
			screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

			// draw the text over objects owned by a player in the default colour for the player
			// based on their ID. This is so we can distinguish between objects owned by different
			// players on the same team
			NetworkColours::NetworkColour colour = NetworkUtils::GetDebugColourForPlayer(GetPlayerOwner());
			Color32 playerColour = NetworkColours::GetNetworkColour(colour);

			// display the network object names of objects if they should be displayed
			if (displaySettings.m_displayObjectInfo)
			{
				if (IsPendingOwnerChange())
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s*", GetLogName());
				}
				else
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s %s", GetLogName(), (!IsClone() && GetSyncData()) ? "(S)" : "");
				}

				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
				screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
			}

			if (displaySettings.m_displayObjectScriptInfo && GetScriptObjInfo())
			{
				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, GetScriptObjInfo()->GetScriptId().GetLogName()));
				screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

				GetScriptObjInfo()->GetLogName(debugStr, DEBUG_STR_LEN);
				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
				screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

				snprintf(debugStr, DEBUG_STR_LEN, "%d", GetScriptObjInfo()->GetObjectId());
				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
				screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
			}
		}		
	}
#endif // __BANK
}

#if ENABLE_NETWORK_LOGGING || USE_NET_ASSERTS
#define LOGGING_OR_NET_ASSERTS_ONLY(x) x
#else
#define LOGGING_OR_NET_ASSERTS_ONLY(x)
#endif

bool CNetObjDoor::ValidateDoorSystemData(const char* LOGGING_OR_NET_ASSERTS_ONLY(reason), 
										 bool assertOnNoNetObject, bool fullValidation)
{
	bool bValid = true;

	if (m_pDoorSystemData)
	{
		if (fullValidation)
		{
			CDoorSystemData* pData = CDoorSystem::GetDoorData(m_doorSystemHash);
			
			if (!pData)
			{
				gnetAssertf(0, "%s : %s - Door system entry %d has been deleted", GetLogName(), reason, m_pDoorSystemData->GetEnumHash());
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s : %s - Door system entry %d has been deleted ##\r\n", GetLogName(), reason, m_pDoorSystemData->GetEnumHash()));
				bValid = false;
			}
			else if (pData != m_pDoorSystemData)
			{
				gnetAssertf(0, "%s : %s - Door system entry %d does not match our entry", GetLogName(), reason, pData->GetEnumHash());
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s : %s - Door system entry %d does not match our entry ##\r\n", GetLogName(), reason, m_pDoorSystemData->GetEnumHash()));
				bValid = false;
			}
		}

		if (bValid)
		{
			if (!m_pDoorSystemData->GetNetworkObject())
			{
				if (assertOnNoNetObject)
				{
					gnetAssertf(0, "%s : %s - Door system entry %d is not associated with this network object", GetLogName(), reason, m_pDoorSystemData->GetEnumHash());
					LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s : %s - Door system entry %d is not associated with this network object ##\r\n", GetLogName(), reason, m_pDoorSystemData->GetEnumHash()));
					bValid = false;
				}
			}
			else if (m_pDoorSystemData->GetNetworkObject() != this)
			{
				gnetAssertf(0, "%s : %s - Door system entry %d is pointing to a different network object! (%s)", GetLogName(), reason, m_pDoorSystemData->GetEnumHash(), m_pDoorSystemData->GetNetworkObject()->GetLogName());
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s : %s - Door system entry %d is pointing to a different network object! (%s) ##\r\n", GetLogName(), reason, m_pDoorSystemData->GetEnumHash(), m_pDoorSystemData->GetNetworkObject()->GetLogName()));
				bValid = false;
			}

			if (bValid && m_pDoorSystemData->GetEnumHash() != (int)m_doorSystemHash)
			{
				gnetAssertf(0, "%s : %s - Door system entry %d does not match our hash %d", GetLogName(), reason, m_pDoorSystemData->GetEnumHash(), m_doorSystemHash);
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("\t\t## %s : %s - Door system entry %d does not match our hash %d ##\r\n", GetLogName(), reason, m_pDoorSystemData->GetEnumHash(), m_doorSystemHash));
			}
		}
	}

	if (!bValid)
	{
		SetDoorSystemData(NULL);
	}

	return bValid;
}

#ifdef LOGGING_OR_NET_ASSERTS_ONLY
#undef LOGGING_OR_NET_ASSERTS_ONLY
#endif