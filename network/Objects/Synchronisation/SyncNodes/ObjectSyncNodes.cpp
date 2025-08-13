//
// name:        ObjectSyncNodes.cpp
// description: Network sync nodes used by CNetObjObjects
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/ObjectSyncNodes.h"
#include "network/network.h"
#include "network/general/NetworkStreamingRequestManager.h"
#include "objects/dummyobject.h"
#include "objects/object.h"
#include "physics/physics.h"
#include "streaming/streaming.h"
#include "fwnet/netserialisers.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IObjectNodeDataAccessor);

CObjectGameStateDataNode::fnTaskDataLogger CObjectGameStateDataNode::m_taskDataLogFunction = 0;

bool CObjectCreationDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CNetObjObject::IsAmbientObjectType(m_ownedBy))
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(m_modelHash, &modelId);

		if (!gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash %u for object clone", m_modelHash))
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, model not valid");
			return false;
		}

		if(!CNetworkStreamingRequestManager::RequestModel(modelId))
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, model not loaded");
			return false;
		}

        if(m_ScriptGrabbedFromWorld)
        {
            CEntity *pClosestEntity = CObject::GetPointerToClosestMapObjectForScript(m_ScriptGrabPosition.x,
                                                                                     m_ScriptGrabPosition.y,
                                                                                     m_ScriptGrabPosition.z,
                                                                                     m_ScriptGrabRadius,
                                                                                     m_modelHash);

            if(pClosestEntity == 0)
            {
                CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, cannot find related entity");
			    return false;
            }
			else if (!pClosestEntity->GetIsDynamic())
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, entity is not dynamic");
				return false;
			}
			else if (static_cast<CDynamicEntity*>(pClosestEntity)->GetNetworkObject())
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, entity already has a network object (%s)", static_cast<CDynamicEntity*>(pClosestEntity)->GetNetworkObject()->GetLogName());
				return false;
			}
        }
	}
	else
	{
		CNetObjObject* pNewNetObj = SafeCast(CNetObjObject, pObj);

		pNewNetObj->SetDummyPositionAndFragGroupIndex(m_dummyPosition, m_IsFragObject ? (s8)m_fragGroupIndex : -1);

		CNetObjObject* pFoundNetObj = nullptr;

		// find the corresponding object at our end
		CObject* pObject = NULL;
		
		if(m_fragParentVehicle == NETWORK_INVALID_OBJECT_ID)
		{
			pFoundNetObj = CNetObjObject::FindUnassignedAmbientObject(m_dummyPosition, m_IsFragObject ? (s8)m_fragGroupIndex : -1);
			if (!pFoundNetObj)
			{
				pObject = CNetObjObject::FindAssociatedAmbientObject(m_dummyPosition, m_IsFragObject ? (int) m_fragGroupIndex : -1);
			}
		}
		else
		{
			pFoundNetObj = CNetObjObject::FindUnassignedVehicleFragment(m_fragParentVehicle);
			if(!pFoundNetObj)
			{
				pObject = CNetObjObject::FindAssociatedVehicleFrag(m_fragParentVehicle);
			}
		}
		

		if (!pObject && !pFoundNetObj)
		{
			CNetwork::GetMessageLog().WriteDataValue("NO OBJECT", "No object found for this dummy location");

			// we must wait for the prop to stream in before it can be broken 
			if (m_IsBroken)
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, object should be broken and object does not exist");
				return false;
			}

			if(m_fragParentVehicle == NETWORK_INVALID_OBJECT_ID)
			{
				if (!CNetObjObject::AddUnassignedAmbientObject(*pNewNetObj, "Clone create"))
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, failed to add unassigned ambient object");
					return false;
				}
			}
			else
			{
				if(!CNetObjObject::AddUnassignedVehicleFrag(*pNewNetObj))
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, failed to add unassigned vehicle frag");
					return false;
				}
			}
		}
		else
		{
			if (pObject && !pObject->GetNetworkObject())
			{
				CNetwork::GetMessageLog().WriteDataValue("OBJECT FOUND", "0x%x (- no network object -)", pObject);
			}
			else
			{
				if (!pFoundNetObj && AssertVerify(pObject))
				{
					pFoundNetObj = SafeCast(CNetObjObject, pObject->GetNetworkObject());
				}

				if (AssertVerify(pFoundNetObj))
				{
					bool bUnregister = false;

					if (pObject)
					{
						CNetwork::GetMessageLog().WriteDataValue("OBJECT FOUND", "0x%x (%s)", pObject, pFoundNetObj->GetLogName());
					}
					else
					{
						CNetwork::GetMessageLog().WriteDataValue("UNASSIGNED NET OBJ FOUND", "%s", pFoundNetObj->GetLogName());
					}

					if (pFoundNetObj->IsClone())
					{
						// this can happen on a third party player while two other players are in the process of deciding who
						// should control an object (see the else code block below). if two players create an object at the same time,
						// they can both send create messages to a third party player before one of them unregisters their object based
						// on the priority system used
						CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, a clone of an object at the specified dummy position already exists!\r\n");
						return false;
					}
					else
					{
						// this code handles a tricky problem. When an object starts to move it will tend to be registered on all
						// the players that it exists on, who will then all send clone create messages. The object will start moving
						// because another entity has collided with it. Ideally the player which controls this entity needs to control
						// the object as well so that the collisions between the two are handled properly. However on the player that
						// controls this entity, it may have not collided with the object at all. But it still needs to be set moving on all
						// players. If a player detects that one if its entities has collided with an object it flags it as having
						// forced ownership and demands control. The other players then have to reliquish control. If this flag is not
						// set then the player with the lowest player id will take control.
						bool bWeWantControl = pObject && pObject->m_nObjectFlags.bWeWantOwnership;

						if (pObject && pObject->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
						{
							bWeWantControl = pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject() && static_cast<CObject*>( pObject->GetFragParent())->m_nObjectFlags.bWeWantOwnership;
						}

						PhysicalPlayerIndex ourPhysicalPlayerIndex = NetworkInterface::GetLocalPhysicalPlayerIndex();

						bool bWeHavePrecidence = (ourPhysicalPlayerIndex == CNetObjObject::GetObjectConflictPrecedence(ourPhysicalPlayerIndex, static_cast<netObject*>(pObj)->GetPhysicalPlayerIndex()));
						bool bPlayerWantsControlMoreThanUs = m_playerWantsControl && !bWeWantControl;
						bool bWeWantControlMoreThanPlayer = bWeWantControl && !m_playerWantsControl;

						if (bWeHavePrecidence)
						{
							if (bPlayerWantsControlMoreThanUs)
							{
								// the other player wants the object more than us - unregister our object and allow it to be controlled by the other machine
								bUnregister = true;
							}
						}
						else if (!bWeWantControlMoreThanPlayer)
						{
							// we do not want the object more than the other player - unregister our object and allow it to be controlled by the other machine
							bUnregister = true;
						}

						if (!bUnregister)
						{
							// we retain control, so skip the clone create
							CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, object ownership conflict\r\n");
							return false;
						}
					}

					if (bUnregister)
					{
						CNetwork::GetMessageLog().WriteDataValue("UNREGISTER", "%s", pFoundNetObj->GetLogName());

						if (pFoundNetObj->IsPendingOwnerChange())
						{
							CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, object is migrating\r\n");

							return false;
						}
						else if (pObject)
						{
							static_cast<CNetObjObject*>(pObject->GetNetworkObject())->SetUnregisteringDueToOwnershipConflict();
							NetworkInterface::UnregisterObject(pObject);
						}
						else if (pFoundNetObj)
						{
							CNetObjObject::RemoveUnassignedAmbientObject(*pFoundNetObj, "Ownership conflict", true);

							if (!CNetObjObject::AddUnassignedAmbientObject(*pNewNetObj, "Clone create"))
							{
								CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, failed to add unassigned ambient object");
								return false;
							}
						}
					}
				}
			}
		}

		m_pRandomObject = pObject;
	}

	return true;
}

bool CObjectCreationDataNode::IsAmbientObjectType(u32 type)
{
	return CNetObjObject::IsAmbientObjectType(type);
}

bool CObjectCreationDataNode::IsFragmentObjectType(u32 type)
{
	return CNetObjObject::IsFragmentObjectType(type);
}

void CObjectCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_POSITION = 31;
	static const unsigned int SIZEOF_FRAG_GROUP_INDEX = 5;
	static const unsigned int SIZEOF_LOD_DISTANCE = 16;

	SERIALISE_OBJECT_CREATED_BY(serialiser, m_ownedBy, "Created By");

	if (IsAmbientObjectType(m_ownedBy) && !serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION_SIZE(serialiser, m_dummyPosition, "Dummy Position", SIZEOF_POSITION);
		SERIALISE_BOOL(serialiser, m_playerWantsControl, "Player Wants Control");
		SERIALISE_BOOL(serialiser, m_IsFragObject);
		SERIALISE_BOOL(serialiser, m_IsBroken, "Is Broken");
		SERIALISE_BOOL(serialiser, m_IsAmbientFence, "Is Fence");
		SERIALISE_BOOL(serialiser, m_HasExploded, "HasExploded");
		SERIALISE_BOOL(serialiser, m_KeepRegistered, "Keep Registered");
		SERIALISE_BOOL(serialiser, m_DestroyFrags, "Destroy Frags");

		if (m_IsFragObject)
		{
			SERIALISE_UNSIGNED(serialiser, m_fragGroupIndex, SIZEOF_FRAG_GROUP_INDEX, "Frag group index");
		}

		SERIALISE_BOOL(serialiser, m_hasGameObject);

		if (!m_hasGameObject)
		{
			SERIALISE_UNSIGNED(serialiser, m_ownershipToken, 10, "Ownership token");
			SERIALISE_POSITION(serialiser, m_objectMatrix.d, "Object Position");
			SERIALISE_ORIENTATION(serialiser, m_objectMatrix, "Object Orientation");
		}	
	}
	else
	{
		SERIALISE_MODELHASH(serialiser, m_modelHash, "Model");
		SERIALISE_BOOL(serialiser, m_hasInitPhysics,         "Has Init Physics");
        SERIALISE_BOOL(serialiser, m_ScriptGrabbedFromWorld, "Script Grabbed From World");
		SERIALISE_BOOL(serialiser, m_noReassign,				"No Reassign");

        if(m_ScriptGrabbedFromWorld || serialiser.GetIsMaximumSizeSerialiser())
        {
            const float    MAX_RADIUS    = 20.0f;
            const unsigned SIZEOF_RADIUS = 8;

            SERIALISE_POSITION(serialiser, m_ScriptGrabPosition, "Script Grab Position");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_ScriptGrabRadius, MAX_RADIUS, SIZEOF_RADIUS, "Script Grab Radius");
        }
	}

	bool isPartOfFragmentedVehicle = m_fragParentVehicle != NETWORK_INVALID_OBJECT_ID;
	SERIALISE_BOOL(serialiser, isPartOfFragmentedVehicle, "Fragment of Vehicle");
	if(isPartOfFragmentedVehicle || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_fragParentVehicle, "Vehicle Parent");
	}
	else
	{
		m_fragParentVehicle = NETWORK_INVALID_OBJECT_ID;
	}

	SERIALISE_BOOL(serialiser, m_lodOrphanHd, "LOD Orphan HD");
	if (m_lodOrphanHd || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_lodDistance, SIZEOF_LOD_DISTANCE, "LOD Distance");
	}

    SERIALISE_BOOL(serialiser, m_CanBlendWhenFixed, "Can Blend When Fixed");
}

PlayerFlags CObjectSectorPosNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

    PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

	return playerFlags;
}

void CObjectSectorPosNode::ExtractDataForSerialising(IObjectNodeDataAccessor &nodeData)
{
	nodeData.GetObjectSectorPosData(*this);
}

void CObjectSectorPosNode::ApplyDataFromSerialising(IObjectNodeDataAccessor &nodeData)
{
	nodeData.SetObjectSectorPosData(*this);
}

void CObjectSectorPosNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_UseHighPrecision);

	if(m_UseHighPrecision || serialiser.GetIsMaximumSizeSerialiser())
	{
		const unsigned SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION = 20;
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosX, (float)WORLD_WIDTHOFSECTOR_NETWORK,  SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION, "Sector Pos X (high precision)");
	    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosY, (float)WORLD_DEPTHOFSECTOR_NETWORK,  SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION, "Sector Pos Y (high precision)");
	    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosZ, (float)WORLD_HEIGHTOFSECTOR_NETWORK, SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION, "Sector Pos Z (high precision)");
	}
    else
    {
        const unsigned SIZEOF_OBJECT_SECTORPOS = 12;
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosX, (float)WORLD_WIDTHOFSECTOR_NETWORK,  SIZEOF_OBJECT_SECTORPOS, "Sector Pos X");
	    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosY, (float)WORLD_DEPTHOFSECTOR_NETWORK,  SIZEOF_OBJECT_SECTORPOS, "Sector Pos Y");
	    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosZ, (float)WORLD_HEIGHTOFSECTOR_NETWORK, SIZEOF_OBJECT_SECTORPOS, "Sector Pos Z");
    }
}

PlayerFlags CObjectOrientationNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
    if (GetIsInCutscene(pObj))
    {
        return ~0u;
    }

    PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

    return playerFlags;
}

void CObjectOrientationNode::ExtractDataForSerialising(IObjectNodeDataAccessor &nodeData)
{
    nodeData.GetObjectOrientationData(*this);
}

void CObjectOrientationNode::ApplyDataFromSerialising(IObjectNodeDataAccessor &nodeData)
{
    nodeData.SetObjectOrientationData(*this);
}

void CObjectOrientationNode::Serialise(CSyncDataBase& serialiser)
{
    SERIALISE_BOOL(serialiser, m_bUseHighPrecision);

    if (m_bUseHighPrecision || serialiser.GetIsMaximumSizeSerialiser())
    {
        static const u32 SIZEOF_ORIENTATION = 20;
        static const float MAX_VALUE = 2.0f * TWO_PI;

        Vector3 eulers(0.0f, 0.0f, 0.0f);
        if (serialiser.GetType() == CSyncDataBase::SYNC_DATA_WRITE || serialiser.GetType() == CSyncDataBase::SYNC_DATA_LOG)
        {
            m_orientation.ToEulersXYZ(eulers);
        }
        SERIALISE_PACKED_FLOAT(serialiser, eulers.x, MAX_VALUE, SIZEOF_ORIENTATION, "Euler X");
        SERIALISE_PACKED_FLOAT(serialiser, eulers.y, MAX_VALUE, SIZEOF_ORIENTATION, "Euler Y");
        SERIALISE_PACKED_FLOAT(serialiser, eulers.z, MAX_VALUE, SIZEOF_ORIENTATION, "Euler Z");
        m_orientation.FromEulersXYZ(eulers);

        Assert(m_orientation.IsOrthonormal());
    }
    else
    {
        SERIALISE_ORIENTATION(serialiser, m_orientation, "Eulers");
    }
}

void CObjectGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_TASK_TYPE			= datBitsNeeded<CTaskTypes::MAX_NUM_TASK_TYPES>::COUNT;
	static const unsigned int SIZEOF_TASK_DATA_SIZE		= 8;
	static const unsigned int SIZEOF_BROKEN_FLAGS		= 32;

	bool hasTask = m_taskType < CTaskTypes::MAX_NUM_TASK_TYPES;

	SERIALISE_BOOL(serialiser, hasTask);

	if (hasTask || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_taskType,		SIZEOF_TASK_TYPE,		"Task Type");
		SERIALISE_UNSIGNED(serialiser, m_taskDataSize,	SIZEOF_TASK_DATA_SIZE,	"Task Data Size");
	}
	else
	{
		m_taskType = CTaskTypes::MAX_NUM_TASK_TYPES;
		m_taskDataSize = 0;
	}

	if (serialiser.GetIsMaximumSizeSerialiser())
	{
		m_taskDataSize = MAX_TASK_DATA_SIZE;
	}

	if(m_taskDataSize > 0)
	{
		SERIALISE_DATABLOCK(serialiser, m_taskSpecificData, m_taskDataSize);

		if (m_taskDataLogFunction)
		{
			(*m_taskDataLogFunction)(serialiser.GetLog(), m_taskType, m_taskSpecificData, m_taskDataSize);
		}
	}

	bool bIsBroken = m_brokenFlags != 0;

	SERIALISE_BOOL(serialiser, bIsBroken, "Is Broken");

	if (bIsBroken || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_brokenFlags,	SIZEOF_BROKEN_FLAGS, "Broken Flags");
	}
	else
	{
		m_brokenFlags = 0;
	}

	m_groupFlags.Serialise(serialiser);

	SERIALISE_BOOL(serialiser, m_objectHasExploded,     "Object Has Exploded");
	SERIALISE_BOOL(serialiser, m_hasAddedPhysics,		"Has Added Physics");
	SERIALISE_BOOL(serialiser, m_visible,				"Is Visible");
    SERIALISE_BOOL(serialiser, m_HasBeenPickedUpByHook, "Has Been Picked Up By Hook");
	SERIALISE_BOOL(serialiser, m_popTires,				"Pop Tires");
}

bool CObjectGameStateDataNode::HasDefaultState() const
{
	return (m_taskType == CTaskTypes::MAX_NUM_TASK_TYPES );
}

void CObjectGameStateDataNode::SetDefaultState()
{
	m_taskType			= CTaskTypes::MAX_NUM_TASK_TYPES;
	m_taskDataSize		= 0;
	m_hasAddedPhysics	= false;
	m_visible			= true;
	m_popTires			= false;
}

void CObjectScriptGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_SCOPE_DISTANCE		= 10;
	static const unsigned SIZEOF_SPEED_BOOST		= 8;
	static const unsigned SIZEOF_JOINT_TO_DRIVE_IDX = 8;

	SERIALISE_BOOL(serialiser, m_ActivatePhysicsAsSoonAsUnfrozen, "Activate Physics When Unfrozen");
	SERIALISE_BOOL(serialiser, m_IsStealable,                     "Is Stealable");
	SERIALISE_BOOL(serialiser, m_UseHighPrecisionBlending,        "Use High Precision Blending");
	SERIALISE_BOOL(serialiser, m_UsingScriptedPhysicsParams,      "Using scripted physics parameters");
	SERIALISE_BOOL(serialiser, m_BreakingDisabled,				  "Breaking disabled");
	SERIALISE_BOOL(serialiser, m_DamageDisabled,				  "Damage disabled");
    SERIALISE_BOOL(serialiser, m_bIsArenaBall,                    "Is Arena Ball");
    SERIALISE_BOOL(serialiser, m_bNoGravity,                      "Has No Gravity");
    SERIALISE_BOOL(serialiser, m_bObjectDamaged,                  "Is Damaged");

    bool bHasDamageInflictor = m_DamageInflictorId != NETWORK_INVALID_OBJECT_ID;
    SERIALISE_BOOL(serialiser, bHasDamageInflictor, "Has Damage Inflictor");
    if (bHasDamageInflictor || serialiser.GetIsMaximumSizeSerialiser())
    {
        SERIALISE_OBJECTID(serialiser, m_DamageInflictorId, "Damage Inflictor Id");
    }
    else
    {
        m_DamageInflictorId = NETWORK_INVALID_OBJECT_ID;
    }

	if(m_UsingScriptedPhysicsParams || serialiser.GetIsMaximumSizeSerialiser())
	{
		const float    MAX_DAMPING_VALUE    = 3.0f;
		const unsigned SIZEOF_DAMPING_VALUE = 7;
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_TranslationDamping.x, MAX_DAMPING_VALUE, SIZEOF_DAMPING_VALUE, "Damping Linear C");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_TranslationDamping.y, MAX_DAMPING_VALUE, SIZEOF_DAMPING_VALUE, "Damping Linear V");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_TranslationDamping.z, MAX_DAMPING_VALUE, SIZEOF_DAMPING_VALUE, "Damping Linear V2");
	}

	SERIALISE_OBJECT_CREATED_BY(serialiser, m_OwnedBy, "Created By");

	bool bHasAdjustedScope = m_ScopeDistance > 0;
	
	SERIALISE_BOOL(serialiser, bHasAdjustedScope);

	if (bHasAdjustedScope || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_ScopeDistance, SIZEOF_SCOPE_DISTANCE, "Scope Distance");
	}
	else
	{
		m_ScopeDistance = 0;
	}

	const unsigned SIZEOF_TINT_INDEX = 6;
	bool hasTint = m_TintIndex != 0;
	SERIALISE_BOOL(serialiser, hasTint, "Has Tint Color");
	if(hasTint || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_TintIndex, SIZEOF_TINT_INDEX, "Tint Color");
	}
	else
	{
		m_TintIndex = 0;
	}
	
	bool bJointDriveUsed = m_bDriveToMaxAngle || m_bDriveToMinAngle;
	SERIALISE_BOOL(serialiser, bJointDriveUsed, "Joint Drive Used");
	if (bJointDriveUsed || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_jointToDriveIndex, SIZEOF_JOINT_TO_DRIVE_IDX, "Joint To Drive Index");
		SERIALISE_BOOL(serialiser, m_bDriveToMaxAngle, "Drive To Max Angle");
		SERIALISE_BOOL(serialiser, m_bDriveToMinAngle, "Drive To Min Angle");
	}
	else
	{
		m_bDriveToMaxAngle = false;
		m_bDriveToMinAngle = false;
	}

	SERIALISE_UNSIGNED(serialiser, m_objSpeedBoost,	SIZEOF_SPEED_BOOST, "Speed Boost");
	SERIALISE_BOOL(serialiser, m_IgnoreLightSettings, "Ignore Light Settings");
	SERIALISE_BOOL(serialiser, m_CanBeTargeted,       "Can Be Targeted");
	SERIALISE_BOOL(serialiser, m_bWeaponImpactsApplyGreaterForce, "Do Weapon Impacts Apply Greater Force");
    SERIALISE_BOOL(serialiser, m_bIsArticulatedProp, "Is Articulated Prop");
	SERIALISE_BOOL(serialiser, m_bObjectFragBroken, "Object Frag Broken");
}

bool CObjectScriptGameStateDataNode::ResetScriptStateInternal()
{
	m_OwnedBy                           = ENTITY_OWNEDBY_TEMP;
    m_DamageInflictorId                 = NETWORK_INVALID_OBJECT_ID;
	m_IsStealable                       = false;
	m_ActivatePhysicsAsSoonAsUnfrozen   = false;
	m_UseHighPrecisionBlending          = false;
	m_UsingScriptedPhysicsParams        = false;
	m_BreakingDisabled					= false;
	m_DamageDisabled					= false;
	m_IgnoreLightSettings				= false;
	m_CanBeTargeted                     = false;
	m_bDriveToMaxAngle					= false;
	m_bDriveToMinAngle					= false;
	m_bWeaponImpactsApplyGreaterForce	= false;
    m_bIsArticulatedProp                = false;
    m_bIsArenaBall                      = false;
    m_bNoGravity                        = false;
	m_bObjectDamaged                    = false;
	m_bObjectFragBroken					= false;
	m_TranslationDamping.Zero();

	return true;
}

bool CObjectScriptGameStateDataNode::HasDefaultState() const
{
	return (!m_ActivatePhysicsAsSoonAsUnfrozen  &&
		!m_IsStealable                      &&
		m_OwnedBy == POPTYPE_RANDOM_AMBIENT &&
		!m_UseHighPrecisionBlending         &&
		!m_UsingScriptedPhysicsParams		&&
		!m_bDriveToMaxAngle					&&
		!m_bDriveToMinAngle					&&
		!m_bWeaponImpactsApplyGreaterForce  &&
        !m_bIsArticulatedProp               &&
        !m_bIsArenaBall                     &&
        !m_bNoGravity                       &&
		!m_bObjectDamaged                   &&
        m_DamageInflictorId == NETWORK_INVALID_OBJECT_ID &&
		!m_bObjectFragBroken);
}

void CObjectScriptGameStateDataNode::SetDefaultState() 
{ 
	ResetScriptStateInternal(); 
}

