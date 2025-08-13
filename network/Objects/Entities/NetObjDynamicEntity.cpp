//
// name:        NetObjDynamicEntity.h
// description: Derived from netObject, this class handles all dynamic entity-related network
//              object calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

// game headers
#include "network/objects/entities/NetObjDynamicEntity.h"
#include "network/Objects/NetworkObjectMgr.h"

// Framework headers
#include "fwnet/netblender.h"
#include "fwnet/netlogutils.h"

// rage includes
#include "physics/physics.h"
#include "scene/DynamicEntity.h"
#include "scene/world/GameWorld.h"
#include "fwdecorator/decoratorExtension.h"
#include "fwdecorator/decoratorInterface.h"

NETWORK_OPTIMISATIONS()

CNetObjDynamicEntity::CNetObjDynamicEntity(CDynamicEntity  *dynamicEntity,
                                const NetworkObjectType     type,
                                const ObjectId              objectID,
                                const PhysicalPlayerIndex           playerIndex,
                                const NetObjFlags           localFlags,
                                const NetObjFlags           globalFlags) 
: CNetObjEntity(dynamicEntity, type, objectID, playerIndex, localFlags, globalFlags)
 ,m_InteriorProxyLoc(0)
 ,m_InteriorDiscrepancyTimer(0)
 ,m_bLoadsCollisions(false)
 ,m_bRetainedInInterior(false)
{
	fwInteriorLocation loc;
	loc.MakeInvalid();
	m_InteriorProxyLoc = loc.GetAsUint32();
}

CNetObjDynamicEntity::CNetObjDynamicEntity()
: m_InteriorProxyLoc(0)
 ,m_InteriorDiscrepancyTimer(0)
 ,m_bLoadsCollisions(false)
 ,m_bRetainedInInterior(false)
{
	fwInteriorLocation loc;
	loc.MakeInvalid();
	m_InteriorProxyLoc = loc.GetAsUint32();
}

netINodeDataAccessor *CNetObjDynamicEntity::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IDynamicEntityNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IDynamicEntityNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjEntity::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjDynamicEntity::ProcessControl()
{
    CDynamicEntity *pDynamicEntity = GetDynamicEntity();
	VALIDATE_GAME_OBJECT(pDynamicEntity);

	if (pDynamicEntity)
	{
		u32 myInteriorProxyLoc = 0;
		bool bRetainedInInterior = false;
		bool bLoadsCollisions = false;

		CPortalTracker* pPortalTracker = pDynamicEntity->GetPortalTracker();

		bRetainedInInterior = pDynamicEntity->GetIsRetainedByInteriorProxy();
		bLoadsCollisions = pPortalTracker ? pPortalTracker->GetLoadsCollisions() : false;

		if (bRetainedInInterior)
		{
			myInteriorProxyLoc = pDynamicEntity->GetRetainingInteriorProxy();
		} 
		else
		{
			myInteriorProxyLoc = pDynamicEntity->GetInteriorLocation().GetAsUint32();
		}

		if (!IsClone())
		{
			m_InteriorProxyLoc = myInteriorProxyLoc;
			m_bRetainedInInterior = bRetainedInInterior;
			m_bLoadsCollisions = bLoadsCollisions;
		} 
		else 
		{
			if (pPortalTracker && 
				m_InteriorProxyLoc != myInteriorProxyLoc && 
				!NetworkBlenderIsOverridden())
			{
				fwInteriorLocation location;
				location.SetAsUint32(m_InteriorProxyLoc);

				CInteriorProxy* pIntProxy = CInteriorProxy::GetFromLocation(location);

				// make sure the interior has loaded
				if (!pIntProxy || (pIntProxy->GetInteriorInst() && pIntProxy->GetInteriorInst()->IsPopulated()))
				{
					m_InteriorDiscrepancyTimer += static_cast<u16>(fwTimer::GetSystemTimeStepInMilliseconds());
	
					if (m_InteriorDiscrepancyTimer >= INTERIOR_DISCREPANCY_RESCAN_TIME)
					{
#if ENABLE_NETWORK_LOGGING
						Vector3 entityPos = VEC3V_TO_VECTOR3(pDynamicEntity->GetTransform().GetPosition());
						location.SetAsUint32(myInteriorProxyLoc);
						CInteriorProxy* pMyIntProxy = CInteriorProxy::GetFromLocation(location);

						NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "INTERIOR_DISCREPANCY", GetLogName());
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Entity position", "%f, %f, %f", entityPos.x, entityPos.y, entityPos.z);
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Synced Interior Location", "%u", m_InteriorProxyLoc);
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Synced Interior Name", "%s", pIntProxy ? pIntProxy->GetName().GetCStr() : "OUTSIDE");
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Local Interior Location", "%u%s", myInteriorProxyLoc, bRetainedInInterior?"(retained)":"");
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Local Interior Name", "%s", pMyIntProxy ? pMyIntProxy->GetName().GetCStr() : "OUTSIDE");
#endif
						// force a rescan
						pPortalTracker->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR); 
						pPortalTracker->RequestRescanNextUpdate(); 	

						m_InteriorDiscrepancyTimer = 0;
					}
				}
			}
			else
			{
				m_InteriorDiscrepancyTimer = 0;
			}
		}
	}

	 return CNetObjEntity::ProcessControl();
}

// Name         :   PostCreate
// Purpose      :   set the desired target location of the entity
// Parameters   :   void     
// Returns      :   void
// ensure objects get correct destination set if necessary after creation
void CNetObjDynamicEntity::PostCreate()
{
	if (m_InteriorProxyLoc != CGameWorld::OUTSIDE.GetAsUint32())
	{
		CDynamicEntity* pDE = GetDynamicEntity();

		if (pDE != NULL)
		{
			fwInteriorLocation currentLoc = pDE->GetInteriorLocation();
			fwInteriorLocation targetLoc;
			targetLoc.SetAsUint32(m_InteriorProxyLoc);

			// target location might not contain a valid room index (if it came from a retain list)
			s32 proxyIndex = targetLoc.GetInteriorProxyIndex();
			if (proxyIndex != INTLOC_INVALID_INDEX)
			{
				if (currentLoc.GetInteriorProxyIndex() != targetLoc.GetInteriorProxyIndex())
				{
					CInteriorProxy* pProxy = CInteriorProxy::GetFromProxyIndex(proxyIndex);
					if (gnetVerifyf(pProxy!=NULL, "SET_INITIAL_INTERIOR : Interior %u (idx:%d) for %s has no proxy", m_InteriorProxyLoc, proxyIndex, GetLogName()))
					{
						pProxy->MoveToRetainList(pDE);
						m_bRetainedInInterior = true;

#if ENABLE_NETWORK_LOGGING
						NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "SET_INITIAL_INTERIOR", GetLogName());
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Move to retain list:", "From %u to %u (idx:%d)", currentLoc.GetAsUint32(), targetLoc.GetAsUint32(), proxyIndex);
#endif // ENABLE_NETWORK_LOGGING
					}
					else
					{
					#if !__FINAL
						u32 checkSum = CInteriorProxy::GetChecksum();
						Displayf("CInteriorProxy::GetChecksum() = %u, targetLoc.IsValid() = %s", checkSum, targetLoc.IsValid() ? "true" : "false");
					#endif

#if ENABLE_NETWORK_LOGGING
						NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "SET_INITIAL_INTERIOR", GetLogName());
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Error: no proxy", "%u", m_InteriorProxyLoc);
#endif // ENABLE_NETWORK_LOGGING
					}
				}
				else 
				{
#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "SET_INITIAL_INTERIOR", GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Entity already in interior", "%u", m_InteriorProxyLoc);
#endif // ENABLE_NETWORK_LOGGING
				}
			}
			else
			{
#if ENABLE_NETWORK_LOGGING
				NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "SET_INITIAL_INTERIOR", GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Error: invalid interior", "%u", m_InteriorProxyLoc);
#endif // ENABLE_NETWORK_LOGGING
			}
		}
	}

	CNetObjEntity::PostCreate();
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   playerIndex   - playerIndex of new owner
//                  migrationType - how the entity migrated
// Returns      :   void
void CNetObjDynamicEntity::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	CDynamicEntity *pDynamicEntity = GetDynamicEntity();
	VALIDATE_GAME_OBJECT(pDynamicEntity);

	const bool bWasClone = IsClone();

    CNetObjEntity::ChangeOwner(player, migrationType);

	if (pDynamicEntity)
	{
		if (bWasClone && !IsClone())
		{
			// set the loads collision flag for the dynamic entity's portal tracker
			CPortalTracker *pPortalTracker = GetDynamicEntity() ? GetDynamicEntity()->GetPortalTracker() : 0;

			if(pPortalTracker)
			{
				pPortalTracker->SetLoadsCollisions(m_bLoadsCollisions);
			}
		}
	}
}

// sync parser getter functions
void CNetObjDynamicEntity::GetDynamicEntityGameState(CDynamicEntityGameStateDataNode& data) const
{
	data.m_InteriorProxyLoc = m_InteriorProxyLoc;
	data.m_loadsCollisions = m_bLoadsCollisions;
	data.m_retained = m_bRetainedInInterior;
	
	// Decorator sync
	CDynamicEntity *pDynamicEntity = GetDynamicEntity();
	VALIDATE_GAME_OBJECT(pDynamicEntity);

	data.m_decoratorListCount = 0;

	if( pDynamicEntity != NULL )
	{
		fwDecoratorExtension* pExt = pDynamicEntity->GetExtension<fwDecoratorExtension>();
		if( pExt != NULL )
		{
			u32 count = pExt->GetCount();
			u32 setdecoratorcount = 0;
	
			gnetAssertf(count<=MAX_DECOR_ENTRIES, "Decorator count[%d] exceeds list max[%d] for entity[%s]",count,MAX_DECOR_ENTRIES,pDynamicEntity->GetDebugName());
	
			if( count > MAX_DECOR_ENTRIES )
			{
#if __BANK
				//Display all the decorators before limiting count
				fwDecorator* pDecDebug = pExt->GetRoot();
				for(u32 decor = 0; decor < count; decor++)
				{
					atHashWithStringBank tempKey = pDecDebug->GetKey();

					gnetDebug3("%s Decorator %d: Name: %s, Hash: 0x%x, Type: %s",
						GetLogName(),
						decor,
						tempKey.GetCStr(),
						tempKey.GetHash(),
						fwDecorator::TypeAsString(pDecDebug->GetType()) );

					pDecDebug = pDecDebug->m_Next;
				}
#endif				
				count = MAX_DECOR_ENTRIES;
			}
	
			fwDecorator* pDec = pExt->GetRoot();
			for(u32 decor = 0; decor < count; decor++)
	        {
				data.m_decoratorList[decor].Type = pDec->GetType();

				atHashWithStringBank tempKey = pDec->GetKey();
				data.m_decoratorList[decor].Key = tempKey.GetHash();

				atHashWithStringBank String;
				switch( data.m_decoratorList[decor].Type )
				{
					case fwDecorator::TYPE_FLOAT:
					pDec->Get(data.m_decoratorList[decor].Float);
					setdecoratorcount++;
					break;
				 
					case fwDecorator::TYPE_BOOL:
					pDec->Get(data.m_decoratorList[decor].Bool);
					setdecoratorcount++;
					break;
	
					case fwDecorator::TYPE_INT:
					pDec->Get(data.m_decoratorList[decor].Int);
					setdecoratorcount++;
					break;
	
					case fwDecorator::TYPE_STRING:
					pDec->Get(String);
					data.m_decoratorList[decor].String = String.GetHash();
					setdecoratorcount++;
					break;
	
					case fwDecorator::TYPE_TIME:
					{
						fwDecorator::Time temp = fwDecorator::UNDEF_TIME;
						pDec->Get(temp);
						data.m_decoratorList[decor].Int = (int)temp;
						setdecoratorcount++;
					}
					break;

					default:
					gnetAssertf(false, "Unknown decorator type[%d]",data.m_decoratorList[decor].Type);
					break;
				}
				pDec = pDec->m_Next;
	        }
			data.m_decoratorListCount = setdecoratorcount;
		}
	}
}

// sync parser setter functions
void CNetObjDynamicEntity::SetDynamicEntityGameState(const CDynamicEntityGameStateDataNode& data)
{
	u32 targetLocation = data.m_InteriorProxyLoc;

#if ENABLE_NETWORK_LOGGING
	u32 currentLocation = m_InteriorProxyLoc;
	const bool outputLog = (currentLocation != targetLocation || m_bLoadsCollisions != data.m_loadsCollisions || m_bRetainedInInterior != data.m_retained);
	if (outputLog)
	{
		NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "INTERIOR_LOC_CHANGE", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Interior Location Change:", "From %u to %u", currentLocation, targetLocation);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Load Collision Change:", "From %s to %s", m_bLoadsCollisions ? "True" : "False", data.m_loadsCollisions ? "True" : "False");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Retained Change:", "From %s to %s", m_bRetainedInInterior ? "True" : "False", data.m_retained ? "True" : "False");
	}
#endif // ENABLE_NETWORK_LOGGING

	// Add to retain list only if this is a player entity
	if (GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX && GetObjectType() == NET_OBJ_TYPE_PLAYER && targetLocation != CGameWorld::OUTSIDE.GetAsUint32())
	{
		CDynamicEntity* pDE = GetDynamicEntity();

		if (pDE != NULL)
		{
			fwInteriorLocation currentLoc = pDE->GetInteriorLocation();
			fwInteriorLocation targetLoc;
			targetLoc.SetAsUint32(targetLocation);

			// target location might not contain a valid room index (if it came from a retain list)
			s32 proxyIndex = targetLoc.GetInteriorProxyIndex();
			if (proxyIndex != INTLOC_INVALID_INDEX)
			{
				if (currentLoc.GetInteriorProxyIndex() != targetLoc.GetInteriorProxyIndex())
				{
					CInteriorProxy* pProxy = CInteriorProxy::GetFromProxyIndex(proxyIndex);
					if (gnetVerifyf(pProxy!=NULL, "SET_INITIAL_INTERIOR : Interior %u (idx:%d) for %s has no proxy", targetLocation, proxyIndex, GetLogName()))
					{
						pProxy->MoveToRetainList(pDE);
						m_bRetainedInInterior = true;

#if ENABLE_NETWORK_LOGGING
						NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "SET_INITIAL_INTERIOR", GetLogName());
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Move to retain list:", "From %u to %u (idx:%d)", currentLoc.GetAsUint32(), targetLoc.GetAsUint32(), proxyIndex);
#endif // ENABLE_NETWORK_LOGGING
					}
				}
			}
		}
	}
	
	m_InteriorProxyLoc = targetLocation;
	m_bLoadsCollisions = data.m_loadsCollisions;
	m_bRetainedInInterior = data.m_retained;

	CDynamicEntity *pDynamicEntity = GetDynamicEntity();
	VALIDATE_GAME_OBJECT(pDynamicEntity);

	if (pDynamicEntity)
	{
		// Decorator sync
		fwExtensibleBase* pBase = (fwExtensibleBase*)pDynamicEntity;
		fwDecoratorExtension::RemoveAllFrom((*pBase));
		if( data.m_decoratorListCount > 0 )
		{
			for(u32 decor = 0; decor < data.m_decoratorListCount; decor++)
			{
				atHashWithStringBank hash = data.m_decoratorList[decor].Key;
				atHashWithStringBank String;
				switch( data.m_decoratorList[decor].Type ) 
				{
					case fwDecorator::TYPE_FLOAT:
					fwDecoratorExtension::Set((*pBase), hash, data.m_decoratorList[decor].Float);
					break;
				 
					case fwDecorator::TYPE_BOOL:
					fwDecoratorExtension::Set((*pBase), hash, data.m_decoratorList[decor].Bool);
					break;
	
					case fwDecorator::TYPE_INT:
					fwDecoratorExtension::Set((*pBase), hash, data.m_decoratorList[decor].Int);
					break;
	
					case fwDecorator::TYPE_STRING:
					String = data.m_decoratorList[decor].String;
					fwDecoratorExtension::Set((*pBase), hash, String);
					break;
	
					case fwDecorator::TYPE_TIME:
					{
						fwDecorator::Time temp = (fwDecorator::Time)data.m_decoratorList[decor].Int;
						fwDecoratorExtension::Set((*pBase), hash, temp);
					}
					break;

					default:
					gnetAssertf(false, "Unknown decorator type[%d]",data.m_decoratorList[decor].Type);
					break;
					
				}
			}
		}
	}
}

bool CNetObjDynamicEntity::IsInSameInterior(netObject& otherObj) const
{
	bool bIsInSameInterior = false;

	if (otherObj.GetEntity() && otherObj.GetEntity()->GetIsDynamic())
	{
		CNetObjDynamicEntity* pDynEntity = static_cast<CNetObjDynamicEntity*>(&otherObj);

		if (pDynEntity->m_bRetainedInInterior == m_bRetainedInInterior)
		{
			fwInteriorLocation myLocation, otherLocation;
			
			myLocation.SetAsUint32(m_InteriorProxyLoc);
			otherLocation.SetAsUint32(pDynEntity->m_InteriorProxyLoc);

			bIsInSameInterior = myLocation.GetInteriorProxyIndex() == otherLocation.GetInteriorProxyIndex();
		}
	}

	return bIsInSameInterior;
}

bool CNetObjDynamicEntity::IsInUnstreamedInterior(unsigned* fixedByNetworkReason) const
{

	CDynamicEntity *pDynamicEntity = GetDynamicEntity();

	if (pDynamicEntity->GetIsRetainedByInteriorProxy())
	{
		if (fixedByNetworkReason)
		{
			*fixedByNetworkReason = FBN_ON_RETAIN_LIST;
		}
			
		return true;
	}

    return false;
}

bool CNetObjDynamicEntity::IsCloneInTargetInteriorState() const
{
    CDynamicEntity *dynamicEntity = GetDynamicEntity();

	if(dynamicEntity)
	{
        u32  myInteriorProxyLoc   = 0;
	    bool isRetainedInInterior = dynamicEntity->GetIsRetainedByInteriorProxy();

	    if(isRetainedInInterior)
	    {
		    myInteriorProxyLoc = dynamicEntity->GetRetainingInteriorProxy();
	    } 
	    else
	    {
		    myInteriorProxyLoc = dynamicEntity->GetInteriorLocation().GetAsUint32();
	    }

        return (m_InteriorProxyLoc == myInteriorProxyLoc);
    }

    return true;
}
