//
// name:        DoorSyncNodes.cpp
// description: Network sync nodes used by CNetObjDoor
// written by:    
//

#include "network/objects/synchronisation/syncnodes/DoorSyncNodes.h"

#include "network/network.h"
#include "network/Objects/Entities/NetObjDoor.h"
#include "objects/door.h"
#include "objects/dummyobject.h"
#include "objects/object.h"
#include "script/script.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IDoorNodeDataAccessor);


bool CDoorCreationDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	bool bCanApplyData = false;
	bool bUnregister = false;

	m_pDoorObject = NULL;

	// find the corresponding door at our end
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_ModelHash, NULL);
	gnetAssertf(pModelInfo, "CDoorCreationDataNode::CanApplyData - no model info found for model hash %d", m_ModelHash);

	if (pModelInfo)
	{
		float radius = CDoor::GetScriptSearchRadius(*pModelInfo);

		CEntity* pDoor = CDoor::FindClosestDoor(pModelInfo, m_Position, radius);

		if (pDoor && pDoor->GetType() == ENTITY_TYPE_OBJECT)
		{ 
			CBaseModelInfo* pModelInfo = pDoor->GetBaseModelInfo();

			// can only use a door that has physics
			if(!pModelInfo->GetUsesDoorPhysics() || pDoor->GetCurrentPhysicsInst())
			{
				gnetAssert(dynamic_cast<CDoor*>(pDoor));
				m_pDoorObject = static_cast<CDoor*>(pDoor);
			}
			else
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door has no physics\r\n");
			}
		}
	}

	if (!m_pDoorObject)
	{
		if (m_scriptDoor)
		{
			bCanApplyData = true;
		}
		else
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door not found\r\n");
		}
	}
	else
	{
		if (!m_pDoorObject->GetNetworkObject())
		{
			bCanApplyData = true;
		}
		else
		{
			if (m_pDoorObject->IsNetworkClone())
			{
				// this can happen on a third party player while two other players are in the process of deciding who
				// should control an object (see the else code block below). if two players create an object at the same time,
				// they can both send create messages to a third party player before one of them unregisters their object based
				// on the priority system used
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, a clone of a door at the specified position already exists!\r\n");
			}
			else if (m_pDoorObject->GetNetworkObject()->IsScriptObject())
			{
				// our door is a script door so it must take precedence  
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door ownership conflict (our door is a script door)\r\n");
			}
			else if (m_scriptDoor && !m_pDoorObject->GetNetworkObject()->IsScriptObject())
			{
				// the other player's door is a script door so it must take precedence 
				bUnregister = true;
			}
			else 
			{
				// this code handles a tricky problem. When a door starts to move it will tend to be registered on all
				// the players that it exists on, who will then all send clone create messages. The door will start moving
				// because another entity has collided with it. Ideally the player which controls this entity needs to control
				// the door as well so that the collisions between the two are handled properly. However on the player that
				// controls this entity, it may have not collided with the door at all. But it still needs to be set moving on all
				// players. If a player detects that one if its entities has collided with an door it flags it as having
				// forced ownership and demands control. The other players then have to reliquish control. If this flag is not
				// set then the player with the lowest player id will take control.
				bool bWeWantControl = m_pDoorObject->m_nObjectFlags.bWeWantOwnership;

				if (m_playerWantsControl)
				{
					// do we think we should have control as well? If so decide on player id
					if (!bWeWantControl || (bWeWantControl && NetworkInterface::GetLocalPhysicalPlayerIndex() > static_cast<netObject*>(pObj)->GetPhysicalPlayerIndex()))
					{
						// unregister our door and allow it to be controlled by the other machine
						bUnregister = true;
					}
					else
					{
						// we retain control, so skip the clone create
						CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door ownership conflict\r\n");
					}
				}
				else if (!bWeWantControl && NetworkInterface::GetLocalPhysicalPlayerIndex() > static_cast<netObject*>(pObj)->GetPhysicalPlayerIndex())
				{
					// unregister our door and allow it to be controlled by the other machine
					bUnregister = true;
				}
				else
				{
					// we still own the door so skip the clone create
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door ownership conflict\r\n");
				}
			}
		}

		if (bUnregister)
		{
			if (m_pDoorObject->GetNetworkObject()->IsPendingOwnerChange())
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door is migrating\r\n");
			}
			else
			{
				NetworkInterface::UnregisterObject(m_pDoorObject);
				bCanApplyData = true;
			}
		}
	}
	
	return bCanApplyData;
}

void CDoorCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	//static const unsigned SIZEOF_ENUM_HASH = 32;
	static const unsigned SIZEOF_MODEL_HASH = 32;

	SERIALISE_UNSIGNED(serialiser, m_ModelHash, SIZEOF_MODEL_HASH, "Model hash");
	SERIALISE_POSITION(serialiser, m_Position, "Position");

	SERIALISE_BOOL(serialiser, m_scriptDoor, "Script Door");

	if (!m_scriptDoor)
	{
		SERIALISE_BOOL(serialiser, m_playerWantsControl, "Player Wants Control");
	}
}

bool CDoorScriptInfoDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	// we have to remove any duplicate host script entities before this one can be created
	if (m_hasScriptInfo)
	{
		CDoorSystemData* pDoorData = CDoorSystem::GetDoorData((int)m_doorSystemHash);

		if (pDoorData && !pDoorData->GetFlaggedForRemoval())
		{
			if (pDoorData->GetScriptInfo())
			{
				if (*pDoorData->GetScriptInfo() != m_scriptInfo)
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, the door system data is registered with a different script (%s)\r\n", pDoorData->GetScriptInfo()->GetScriptId().GetLogName());
					return false;
				}

				scriptHandler* pScriptHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_scriptInfo.GetScriptId());

				if (!pScriptHandler || !pScriptHandler->GetNetworkComponent())
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, script %s is not running\r\n", pDoorData->GetScriptInfo()->GetScriptId().GetLogName());
					return false;
				}
				else if (!pScriptHandler->GetNetworkComponent()->IsPlaying())
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, script %s is not in a playing state\r\n", pDoorData->GetScriptInfo()->GetScriptId().GetLogName());
					return false;
				}
			}
			
			if (pDoorData->GetNetworkObject())
			{
				if (pDoorData->GetNetworkObject() != static_cast<netObject*>(pObj))
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door system data already exists for this door (%s)\r\n", pDoorData->GetNetworkObject()->GetLogName());
					return false;
				}
			}
			else if (!m_existingScriptDoor)
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, door system data already exists for this door\r\n");
				return false;
			}
			else
			{
				gnetAssert(pDoorData->GetPersistsWithoutNetworkObject());
			}
		}
		else if (m_existingScriptDoor)
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, the door system data entry does not exist for this door\r\n");
			return false;
		}
		else if (pDoorData && pDoorData->GetFlaggedForRemoval())
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, the door system data entry is flagged for removal\r\n");
			return false;
		}

		if (!NetworkInterface::GetObjectManager().CanAcceptScriptObject(*static_cast<CNetObjGame*>(pObj), m_scriptInfo))
		{
			return false;
		}
	}

	return true;
}

void CDoorScriptInfoDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_DOOR_HASH = 32;

	SERIALISE_BOOL(serialiser, m_hasScriptInfo, "Has script info");

	if (m_hasScriptInfo || serialiser.GetIsMaximumSizeSerialiser())
	{
		m_scriptInfo.Serialise(serialiser);
		if (serialiser.GetLog()) serialiser.GetLog()->WriteDataValue("Script", "%s", m_scriptInfo.GetScriptId().GetLogName());
		SERIALISE_UNSIGNED(serialiser, m_doorSystemHash, SIZEOF_DOOR_HASH, "Door system hash");
		SERIALISE_BOOL(serialiser, m_existingScriptDoor, "Existing door system entry");
	}
}

bool CDoorScriptInfoDataNode::HasDefaultState() const 
{ 
	return !m_hasScriptInfo; 
}

void CDoorScriptInfoDataNode::SetDefaultState() 
{ 
	m_hasScriptInfo = false; 
}

void CDoorScriptGameStateDataNode::ExtractDataForSerialising(IDoorNodeDataAccessor &doorNodeData)
{
	doorNodeData.GetDoorScriptGameState(*this);
	m_Broken = m_BrokenFlags != 0;
	m_Damaged = m_DamagedFlags != 0;
}

void CDoorScriptGameStateDataNode::ApplyDataFromSerialising(IDoorNodeDataAccessor &doorNodeData)
{
	doorNodeData.SetDoorScriptGameState(*this);
}

void CDoorScriptGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_DOOR_SLIDE_RATE	= 9;
	static const unsigned SIZEOF_DOOR_SLIDE_DIST	= 9;
	static const unsigned SIZEOF_DOOR_BROKEN_FLAGS	= 18;
	static const unsigned SIZEOF_DOOR_DAMAGED_FLAGS	= 18;

	bool sliding = m_AutomaticDist != 0.0f || m_AutomaticRate != 0.0f;

	SERIALISE_DOOR_SYSTEM_STATE(serialiser, m_DoorSystemState, "Door system state");

	SERIALISE_BOOL(serialiser, sliding);

	if (sliding || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_AutomaticDist, 100.0f, SIZEOF_DOOR_SLIDE_DIST, "Slide Dist");
		SERIALISE_PACKED_FLOAT(serialiser, m_AutomaticRate, 30.0f, SIZEOF_DOOR_SLIDE_RATE, "Slide Rate");
	}
	else
	{
		m_AutomaticDist = 0.0f;
		m_AutomaticRate = 0.0f;
	}

	SERIALISE_BOOL(serialiser, m_Broken);

	if (m_Broken || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_BrokenFlags, SIZEOF_DOOR_BROKEN_FLAGS, "Broken Flags");
	}
	else
	{
		m_BrokenFlags = 0;
	}

	SERIALISE_BOOL(serialiser, m_Damaged);

	if (m_Damaged || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_DamagedFlags, SIZEOF_DOOR_DAMAGED_FLAGS, "Damaged Flags");
	}
	else
	{
		m_DamagedFlags = 0;
	}

	SERIALISE_BOOL(serialiser, m_HoldOpen, "Hold Open");
}

PlayerFlags CDoorMovementDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	// don't send movement data if we don't have a physical door
	if (!((CNetObjDoor*)pObj)->GetDoor())
	{
		return ~0U;
	}

	return 0;
}

void CDoorMovementDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_OPEN_RATIO = 8;

	SERIALISE_BOOL(serialiser, m_bHasOpenRatio);

	if (m_bHasOpenRatio || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_fOpenRatio, 1.0f, SIZEOF_OPEN_RATIO, "Open Ratio");
	}
	else
	{
		SERIALISE_BOOL(serialiser, m_bOpening, "Opening");
		SERIALISE_BOOL(serialiser, m_bFullyOpen, "Fully open");
		SERIALISE_BOOL(serialiser, m_bClosed, "Closed");
	}
}

bool CDoorMovementDataNode::HasDefaultState() const
{
	return m_bHasOpenRatio && m_fOpenRatio == 0.0f;
}

void CDoorMovementDataNode::SetDefaultState()
{
	m_bHasOpenRatio = true;
	m_fOpenRatio = 0.0f;
}

