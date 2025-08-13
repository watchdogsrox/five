//
// name:        NetObjObject.cpp
// description: Derived from netObject, this class handles all object-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//
 
// game headers
#include "network/Objects/entities/NetObjObject.h"

#include "Objects/ObjectIntelligence.h"
#include "Task/Movement/TaskParachuteObject.h"
#include "network/Debug/NetworkDebug.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "network/Objects/Entities/NetObjVehicle.h"
#include "network/Objects/NetworkObjectMgr.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "network/Objects/prediction/NetBlenderPhysical.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkDamageTracker.h"
#include "network/players/NetworkPlayerMgr.h"
#include "objects/Door.h"
#include "objects/DummyObject.h"
#include "objects/Object.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "vfx/misc/LODLightManager.h"
#include "Shaders/CustomShaderEffectWeapon.h"
#include "control/replay/replay.h"
#include "physics/inst.h"

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CNetObjObject, MAX_NUM_NETOBJOBJECTS, atHashString("CNetObjObject",0xb81c53e2));
FW_INSTANTIATE_CLASS_POOL(CNetObjObject::CObjectSyncData, MAX_NUM_NETOBJOBJECTS, atHashString("CObjectSyncData",0xe4ccd07f));

#if __BANK
s32 CNetObjObject::ms_updateRateOverride = -1;
#endif

PARAM(NoArenaBallBlenderData, "Don't use custom Arena Ball blender data.");

namespace
{
    const unsigned int SIZEOF_BROKEN_FLAGS_MIN = 8;
    const unsigned int SIZEOF_BROKEN_FLAGS_MAX = 21; // hotdog stands have this many broken parts and are used in mafya work!
}

CObjectBlenderDataStandard      s_ObjectBlenderDataStandard;
CObjectBlenderDataHighPrecision s_ObjectBlenderDataHighPrecision;
CObjectBlenderDataParachute		s_ObjectBlenderDataParachute;
CObjectBlenderDataArenaBall		s_ObjectBlenderDataArenaBall;

CPhysicalBlenderData           *CNetObjObject::ms_ObjectBlenderDataStandard      = &s_ObjectBlenderDataStandard;
CPhysicalBlenderData           *CNetObjObject::ms_ObjectBlenderDataHighPrecision = &s_ObjectBlenderDataHighPrecision;
CPhysicalBlenderData		   *CNetObjObject::ms_ObjectBlenderDataParachute	 = &s_ObjectBlenderDataParachute;	
CPhysicalBlenderData		   *CNetObjObject::ms_ObjectBlenderDataArenaBall     = &s_ObjectBlenderDataArenaBall;

CObjectSyncTree                *CNetObjObject::ms_objectSyncTree;
CNetObjObject::CObjectScopeData CNetObjObject::ms_objectScopeData;

CNetObjObject*					CNetObjObject::ms_unassignedAmbientObjects[CNetObjObject::MAX_UNASSIGNED_AMBIENT_OBJECTS];
u32								CNetObjObject::ms_numUnassignedAmbientObjects = 0;
u32								CNetObjObject::ms_peakUnassignedAmbientObjects = 0;
bool							CNetObjObject::ms_assigningAmbientObject = false;
bool							CNetObjObject::ms_gettingFragObjectFromParent = false;

ObjectId CNetObjObject::ms_unassignedVehicleFrags[CNetObjObject::MAX_UNASSIGNED_VEHICLE_FRAGS];

u32 CNetObjObject::ms_collisionSyncIgnoreTimeArenaBall = 200;
float CNetObjObject::ms_collisionMinImpulseArenaBall = 1000.0f;

// ================================================================================================================
// CNetObjObject
// ================================================================================================================

CNetObjObject::CNetObjObject(CObject                    *object,
                             const NetworkObjectType    type,
                             const ObjectId             objectID,
                             const PhysicalPlayerIndex  playerIndex,
                             const NetObjFlags          localFlags,
                             const NetObjFlags          globalFlags) :
CNetObjPhysical(object, type, objectID, playerIndex, localFlags, globalFlags)
, m_bCloneAsTempObject(false)
, m_registeredWithKillTracker(false)
, m_UseHighPrecisionBlending(false)
, m_UseParachuteBlending(false)
, m_UsingScriptedPhysicsParams(false)
, m_DisableBreaking(false)
, m_DisableDamage(false)
, m_BlenderModeDirty(false)
, m_UnregisterAsleepCheck(false)
, m_ScriptGrabbedFromWorld(false)
, m_ScriptGrabPosition(VEC3_ZERO)
, m_ScriptGrabRadius(0.0f)
, m_AmbientProp(false)
, m_AmbientFence(false)
, m_KeepRegistered(false)
, m_UnassignedAmbientObject(false)
, m_UnassignedVehicleFragment(false)
, m_DummyPosition(VEC3_ZERO)
, m_FragGroupIndex(-1)
, m_brokenFlags(0)
, m_DestroyFrags(false)
, m_lastTimeBroken(fwTimer::GetSystemTimeInMilliseconds())
, m_UnregisteringDueToOwnershipConflict(false)
, m_HasBeenPickedUpByHook(false)
, m_GameObjectBeingDestroyed(false)
, m_HasExploded(object && object->m_nFlags.bHasExploded)
, m_bDelayedCreateDrawable(false)
, m_bDontAddToUnassignedList(false)
, m_ScriptAdjustedScopeDistance(0)
, m_bCanBlendWhenFixed(false)
, m_bHasLoadedDroneModelIn(false)
, m_bUseHighPrecisionRotation(false)
, m_ArenaBallBlenderOverrideTimer(0)
, m_bIsArenaBall(false)
, m_fragParentVehicle(NETWORK_INVALID_OBJECT_ID)
, m_bDriveSyncPending(false)
, m_SyncedJointToDriveIndex(0)
, m_bSyncedDriveToMinAngle(false)
, m_bSyncedDriveToMaxAngle(false)
, m_bObjectDamaged(false)
, m_fragSettledOverrideBlender(false)
, m_forceScopeCheckUntilFirstUpdateTimer(0)
{
	if (object)
	{
		// find the dummy position
		if (IsAmbientObjectType(object->GetOwnedBy()) && !object->m_nObjectFlags.bIsNetworkedFragment)
		{
			CDummyObject *pDummyObject = object->GetRelatedDummy();

			if (!pDummyObject && object->GetFragParent() && object->GetFragParent()->GetIsTypeObject())
			{
				pDummyObject = SafeCast(CObject, object->GetFragParent())->GetRelatedDummy();
			}

			if (pDummyObject)
			{
				m_DummyPosition = VEC3V_TO_VECTOR3(pDummyObject->GetTransform().GetPosition());
			}
			else
			{
#if __ASSERT
				Vector3 objPos = VEC3V_TO_VECTOR3(object->GetTransform().GetPosition());
				gnetAssertf(0, "Failed to find dummy object for %s_%d at %f, %f, %f", GetObjectTypeName(), GetObjectID(), objPos.x, objPos.y, objPos.z);
#endif // __ASSERT
			}
		}

		if (object->GetFragInst() && 
			object->GetFragInst()->GetCacheEntry() && 
			object->GetFragInst()->GetCacheEntry()->GetHierInst() &&
			object->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken)
		{
			u32 numBrokenFlags = object->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->GetNumBits();

			u32 bit = 0;
			for (bit=0; bit<numBrokenFlags; bit++)
			{
				if (!object->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(bit))
				{
					m_FragGroupIndex = (u8)bit;
					break;
				}
			}

			gnetAssertf(bit<numBrokenFlags, "%s_%d is a fragment cache object with all bits broken!", GetObjectTypeName(), GetObjectID());
		}

		m_AmbientFence = IsAmbientFence();
	}
}

CNetObjObject::~CNetObjObject()
{
	if (m_UnassignedAmbientObject)
	{
		RemoveUnassignedAmbientObject(*this, "CNetObjObject destroyed", false);
	}
	else if(m_UnassignedVehicleFragment)
	{
		CNetObjObject::RemoveUnassignedVehicleFragment(*this);
	}
}

void CNetObjObject::CreateSyncTree()
{
    ms_objectSyncTree = rage_new CObjectSyncTree(OBJECT_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjObject::DestroySyncTree()
{
    ms_objectSyncTree->ShutdownTree();
    delete ms_objectSyncTree;
    ms_objectSyncTree = 0;
}

netINodeDataAccessor *CNetObjObject::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IObjectNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IObjectNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjPhysical::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   player        - the new owner
//                  migrationType - how the entity migrated
// Returns      :   void
void CNetObjObject::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    CObject* pObject = GetCObject();

    CNetObjPhysical::ChangeOwner(player, migrationType);

    if (!IsClone())
    {
		if (pObject)
		{
			// need to add object to process control list so it updates itself and tries to pass control of itself to other machines
			if (!pObject->GetIsOnSceneUpdate())
			{
				pObject->AddToSceneUpdate();
			}
		}
		else if (GetSyncTree() && GetObjectType() == NET_OBJ_TYPE_OBJECT)
		{
			if (!GetSyncData())
			{
				StartSynchronising();
			}

			if (GetSyncData())
			{
				// if we don't have a game object, make sure the ownership token gets sent out
				CObjectSyncTree* pSyncTree = SafeCast(CObjectSyncTree, GetSyncTree());

				if (pSyncTree)
				{
					pSyncTree->DirtyNode(this, *pSyncTree->GetGlobalFlagsNode());
				}
			}
		}
    }
}

void CNetObjObject::SetUseHighPrecisionBlending(bool useHighPrecisionBlending)
{
    m_UseHighPrecisionBlending = useHighPrecisionBlending;
    UpdateBlenderData();
}

void CNetObjObject::SetIsArenaBall(bool bIsArenaBall)
{
    m_bIsArenaBall = bIsArenaBall;
    UpdateBlenderData();
}

void CNetObjObject::RegisterCollision(CNetObjPhysical* pCollider, float impulseMag)
{
    CNetObjPhysical::RegisterCollision(pCollider, impulseMag);
    if (IsArenaBall() && IsClone() && GetPhysical() && GetNetBlender() && pCollider->GetPhysical() && (pCollider->GetPhysical()->GetIsTypeObject() || pCollider->GetPhysical()->GetIsTypeVehicle()))
    {
        if (impulseMag > ms_collisionMinImpulseArenaBall && static_cast<CNetBlenderPhysical*>(GetNetBlender())->GetLastVelocityReceived().IsZero())
        {
            SetArenaBallBlenderOverrideTimer(ms_collisionSyncIgnoreTimeArenaBall);
        }
    }
}

void CNetObjObject::SetUseParachuteBlending(bool useParachuteBlending)
{
    m_UseParachuteBlending = useParachuteBlending;
    UpdateBlenderData();
}

void CNetObjObject::UpdateBlenderData()
{
    netBlenderLinInterp *blender = SafeCast(netBlenderLinInterp, GetNetBlender());

	if (!blender)
		return;

    if (m_UseParachuteBlending)
    {
        blender->SetBlenderData(ms_ObjectBlenderDataParachute);
        blender->SetUseLogarithmicBlending(false);
    }
    else if(m_UseHighPrecisionBlending)
    {
        blender->SetBlenderData(ms_ObjectBlenderDataHighPrecision);
        blender->SetUseLogarithmicBlending(true);
    }
    else if (m_bIsArenaBall && !PARAM_NoArenaBallBlenderData.Get())
    {
        blender->SetBlenderData(ms_ObjectBlenderDataArenaBall);
        blender->SetUseLogarithmicBlending(false);
    }
    else
    {
        blender->SetBlenderData(ms_ObjectBlenderDataStandard);
        blender->SetUseLogarithmicBlending(false);
    }
}

void CNetObjObject::SetUseHighPrecisionRotation(bool bUseHightPrecisionRotation)
{
    m_bUseHighPrecisionRotation = bUseHightPrecisionRotation;
}

void CNetObjObject::SetScriptGrabParameters(const Vector3 &scriptGrabPos, const float scriptGrabRadius)
{
    m_ScriptGrabbedFromWorld = true;
    m_ScriptGrabPosition     = scriptGrabPos;
    m_ScriptGrabRadius       = scriptGrabRadius;
}

// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns      :   True if the object wants to unregister itself
bool CNetObjObject::Update()
{
    CObject *pObject = GetCObject();
	VALIDATE_GAME_OBJECT(pObject);

	if (pObject)
	{
		m_AmbientProp = IsAmbientObjectType(pObject->GetOwnedBy());

		gnetAssertf(pObject->GetNetworkObject(), "%s is assigned to a CObject that has no network object ptr", GetLogName());
		gnetAssertf(!pObject->GetNetworkObject() || pObject->GetNetworkObject() == this, "%s is assigned to a CObject that has network object ptr to %s", GetLogName(), pObject->GetNetworkObject()->GetLogName());

        if (m_ArenaBallBlenderOverrideTimer > 0)
        {
            const CNetBlenderPhysical* pBlender = static_cast<CNetBlenderPhysical*>(GetNetBlender());
            if (pBlender && pBlender->GetLastVelocityReceived().IsNonZero())
            {
                m_ArenaBallBlenderOverrideTimer = 0;
            }
            else
            {
                m_ArenaBallBlenderOverrideTimer -= fwTimer::GetSystemTimeStepInMilliseconds();
            }
        }

		if (!pObject->GetNetworkObject() || pObject->GetNetworkObject() != this)
		{
			SetGameObject(NULL);
		}

        if (m_bDriveSyncPending)
        {
            m_bDriveSyncPending = false;
            pObject->UpdateDriveToTargetFromNetwork(m_SyncedJointToDriveIndex, m_bSyncedDriveToMinAngle, m_bSyncedDriveToMaxAngle);
        }

		if (pObject->GetFragInst())
		{
			if (IsClone() && pObject->GetIsAttached())
			{
				CEntity* pEntityAttachedTo = GetEntityAttachedTo();

				if (pEntityAttachedTo && pEntityAttachedTo->GetIsPhysical() && CNetObjVehicle::GetPickupRopeGadget((CPhysical*)pEntityAttachedTo))
				{
					// prevent attached clone objects attached to a pickup rope from breaking, this can happen if they get warped into the vehicle by the net blender
					// the rope
					pObject->GetFragInst()->SetDisableBreakable(true);
				}
			}
			else 
			{
				if (m_DisableBreaking != pObject->GetFragInst()->IsBreakingDisabled())
				{
					pObject->GetFragInst()->SetDisableBreakable(m_DisableBreaking);
				}

				if (m_DisableDamage != pObject->GetFragInst()->IsDamageDisabled())
				{
					pObject->GetFragInst()->SetDisableDamage(m_DisableDamage);
				}
			}
		}
	}
	else
	{
		m_AmbientProp = true;
	}
	
	if (!IsClone())
    {
		if (!HasGameObject())
		{
			if (UpdateUnassignedObject())
			{
#if ENABLE_NETWORK_LOGGING
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "UNREGISTERING_AMBIENT_PROP", GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", "All broken bits set");
#endif
				return true;
			}
			else
			{
				// we need to call this so that the scope is recalculated for each player
				return netObject::Update();
			}
		}
		else if (pObject) 
		{
			// **** temporary hack fix to remove non-script for sale signs *****
			if (!IsScriptObject() && !IsPendingOwnerChange() && ((pObject->GetModelId() == MI_PROP_FORSALE_DYN_01) || (pObject->GetModelId() == MI_PROP_FORSALE_DYN_02)))
			{
				return true;
			}

			u32 brokenFlags = 0;
			SyncGroupFlags groupFlags;

			fragInst* pFragInst = pObject->GetFragInst();
			if (pFragInst && pFragInst->GetCacheEntry())
			{
				groupFlags.m_NumBitsUsed = pFragInst->GetCacheEntry()->GetHierInst()->groupSize;
				if (groupFlags.m_NumBitsUsed > SyncGroupFlags::MAX_NUM_GROUP_FLAGS_BITS)
				{
					groupFlags.m_NumBitsUsed = SyncGroupFlags::MAX_NUM_GROUP_FLAGS_BITS;
					gnetAssertf(0, "%s trying to grab group flag size for array with larger than synced size: %d", GetLogName(), groupFlags.m_NumBitsUsed);
				}

				for (s32 bit = 0; bit < groupFlags.m_NumBitsUsed; bit++)
				{
					if (pFragInst->GetCacheEntry()->GetHierInst()->groupInsts[bit].IsDamaged())
					{
						groupFlags.m_Flags.Set(bit);
					}
				}

				if (m_FragGroupIndex <= 0)
				{
					// ignore root
					for (s32 bit = 1; bit < pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->GetNumBits(); bit++)
					{
						if (pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(bit))
						{
							brokenFlags |= (1 << bit);
						}
					}
				}
			}

			bool bJustExploded = pObject->m_nFlags.bHasExploded && !m_HasExploded;

			if (bJustExploded || (brokenFlags != m_brokenFlags) || (groupFlags.m_Flags != m_groupFlags.m_Flags))
			{
#if ENABLE_NETWORK_LOGGING
				if (bJustExploded)
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "OBJECT_EXPLODED", GetLogName());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Clone", "false");
				}
#endif
				// the object has just broken, force an immediate update as broken frags can generate explosions, etc
				m_brokenFlags = brokenFlags;
				m_groupFlags = groupFlags;
				m_HasExploded = pObject->m_nFlags.bHasExploded;

				m_lastTimeBroken = fwTimer::GetSystemTimeInMilliseconds();

				if (!pObject->IsADoor() && GetSyncTree())
				{
					if (!GetSyncData())
					{
						StartSynchronising();
					}

					if (GetSyncData())
					{
						GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());
						GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CObjectSyncTree, GetSyncTree())->GetObjectGameStateNode());
					}
				}
			}

			if (!m_KeepRegistered && !m_HasExploded)
			{
				if (!IsPendingOwnerChange())
				{
					// unregister any fragments that have all broken bits set (this can happen to bin bags that have exploded and disappeared)
					if (IsFragmentObjectType(pObject->GetOwnedBy()) &&
						pObject->GetCurrentPhysicsInst() &&
						pObject->GetFragInst() &&
						pObject->GetFragInst()->GetCacheEntry())
					{
						s32 numBrokenFlags  = pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->GetNumBits();

						if (pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->CountOnBits() == numBrokenFlags)
						{
#if ENABLE_NETWORK_LOGGING
							NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "UNREGISTERING_AMBIENT_PROP", GetLogName());
							NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", "All broken bits set");
#endif
							return true;
						}
					}
				}

				// unregister any ambient objects that have not moved far enough away from their dummy position
				if (!pObject->IsADoor() && pObject->IsAsleep() && !m_AmbientFence && !(pObject->GetFragInst() && pObject->m_nObjectFlags.bHasBeenUprooted) && m_brokenFlags == 0)
				{
					if (!m_UnregisterAsleepCheck)
					{
						CDummyObject *pDummyObject = pObject->GetRelatedDummy();

						if (pDummyObject)
						{
							static float minMoveDist = 0.2f*0.2f;

							const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
							const Vector3 vDummyObjectPosition = VEC3V_TO_VECTOR3(pDummyObject->GetTransform().GetPosition());

							Vector3 toDummy = vObjectPosition - vDummyObjectPosition;
							float toDummyDist = toDummy.XYMag2();

							if (toDummyDist < minMoveDist)
							{
#if ENABLE_NETWORK_LOGGING
								NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "UNREGISTERING_AMBIENT_PROP", GetLogName());
								NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", "Not moved far enough from dummy position");
#endif
								// clear the bMovedInNetworkGame flag so that the object can be registered when moved again
								pObject->ResetMovedInNetworkGame();
								return true;
							}
						}

						m_UnregisterAsleepCheck = true;
					}
				}
				else
				{
					m_UnregisterAsleepCheck = false;
				}
			}
		}
	}
    else
    {
		// we may not have an object if this is an ambient prop
		if (!pObject)
		{
			return false;
		}

		if (m_HasExploded && !pObject->m_nFlags.bHasExploded)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "OBJECT_EXPLODED", GetLogName());
			NetworkInterface::GetObjectManagerLog().WriteDataValue("Clone", "true");

			if (pObject->m_nFlags.bHasExploded)
			{
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Already exploded", "true");
			}
			else if (pObject->TryToExplode(pObject->GetWeaponDamageEntity(), false))
			{
				m_lastTimeBroken = fwTimer::GetSystemTimeInMilliseconds();
			}
			else
			{
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Failed", "true");
			}
		}

		if(m_BlenderModeDirty)
        {
            if(GetNetBlender())
            {
                UpdateBlenderData();
                m_BlenderModeDirty = false;
            }
        }

		TryToBreakObject();

		m_UnregisterAsleepCheck = false;

		CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());
		if (netBlenderPhysical && IsArenaBall() && netBlenderPhysical->HasStoppedPredictingPosition())
		{			
			// Lerp the balls to a stand-still if the remote player is not responding. Also don't blend them (CanBlend())
			const float LERP_RATE = 0.08f;
			pObject->SetVelocity(Lerp(LERP_RATE, pObject->GetVelocity(), VEC3_ZERO));
		}
    }

	CEntity* pAttachParent = pObject ? SafeCast(CEntity, pObject->GetAttachParent()) : NULL;

	if (pAttachParent && pAttachParent->GetIsPhysical())
	{
		CNetObjPhysical* pNetObj = SafeCast(CNetObjPhysical, SafeCast(CPhysical, pAttachParent)->GetNetworkObject());

		if (pNetObj && pNetObj->IsConcealed())
		{
			SetOverridingLocalVisibility(false, "Attached to concealed entity");
		}
	}
	
	if (m_forceScopeCheckUntilFirstUpdateTimer > 0)
	{
		m_forceScopeCheckUntilFirstUpdateTimer -= fwTimer::GetSystemTimeStepInMilliseconds();
		if (m_forceScopeCheckUntilFirstUpdateTimer < 0)
		{
			m_forceScopeCheckUntilFirstUpdateTimer = 0;
		}
	}

	if(m_fragParentVehicle != NETWORK_INVALID_OBJECT_ID)
	{
		//static const float MAX_DIST_FROM_TARGET = rage::square(0.35f);
		static const float VEL_THRESHOLD = rage::square(0.8f);
		static const float VEL_REACTIVATION_THRESHOLD = rage::square(1.2f);
		CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());
		CObject* object = GetCObject();
		if (netBlenderPhysical && object)
		{
		//	Vector3 targetPosition = netBlenderPhysical->GetLastPositionReceived();
		//	Vector3 currentPosition = VEC3V_TO_VECTOR3(object->GetTransform().GetPosition());
			float velocityMag = object->GetVelocity().Mag2();

			if(velocityMag < VEL_THRESHOLD || (m_fragSettledOverrideBlender && velocityMag < VEL_REACTIVATION_THRESHOLD))
			{				
				m_fragSettledOverrideBlender = true;
			}					
			else
			{
				m_fragSettledOverrideBlender = false;
			}
		}				
	}		

    return CNetObjPhysical::Update();
}

void CNetObjObject::SetGameObject(void* gameObject)
{
	CNetObjPhysical::SetGameObject(gameObject);

	if (GetNetBlender())
	{
		SafeCast(CNetBlenderPhysical, GetNetBlender())->SetPhysical((CPhysical*)gameObject);	
	}

	if (gameObject)
	{
		m_AmbientFence = IsAmbientFence();
	}
}

void CNetObjObject::CreateNetBlender()
{
    CObject *object = GetCObject();

	if(object)
	{
		gnetAssert(!GetNetBlender());

		netBlender *blender = rage_new CNetBlenderPhysical(object, ms_ObjectBlenderDataStandard);
		gnetAssert(blender);
        blender->Reset();
        SetNetBlender(blender);
	}
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjObject::ProcessControl()
{
    CObject* pObject = GetCObject();
 
    CNetObjPhysical::ProcessControl();

    if (pObject && IsClone())
    {
		if(pObject->GetCollider() && pObject->GetModelIndex() == MI_BATTLE_DRONE_QUAD)
		{
			pObject->GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, true);
			pObject->GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);		
			

			// Script is using WEAPON_STUNGUN hash when firing bullets from the drone.
			// So we need to preload the model for the stungun otherwise damage events can fail multiple times which could cause the damage to not apply
			if(!m_bHasLoadedDroneModelIn)
			{
				const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(ATSTRINGHASH("WEAPON_STUNGUN", 0x3656C8C1));
				if(weaponInfo)
				{
					const strLocalIndex modelIndex = strLocalIndex(weaponInfo->GetModelIndex());
					fwModelId modelId(modelIndex);
					if(CModelInfo::IsValidModelInfo(modelIndex.Get()) && !CModelInfo::HaveAssetsLoaded(modelId))
					{
						CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
						m_bHasLoadedDroneModelIn = true;
					}
				}
			}
		}	

        bool isInWater = pObject->GetIsInWater();
        pObject->ProcessControl();
        pObject->SetIsInWater( isInWater );

		// clones should not have a primary task, but can have a secondary which runs locally (eg playing an animation)
		if(pObject->GetObjectIntelligence())
		{
			pObject->GetObjectIntelligence()->GetTaskManager()->Process(fwTimer::GetTimeStep());
		}

    }

    return true;
}

u32	CNetObjObject::CalcReassignPriority() const
{
	if (!HasGameObject())
	{
		return 0;
	}

	return CNetObjPhysical::CalcReassignPriority();
}

//
// name:        CleanUpGameObject
// description: Cleans up the pointers between this network object and the
//              game object which owns it
void CNetObjObject::CleanUpGameObject(bool bDestroyObject)
{
	CObject* pObject = GetCObject();

	// if m_UnregisteringDueToOwnershipConflict is set, the object is being passed from one network object to another so we don't want to 
	// change its state
	const bool bObjectShouldBeDestroyed = (bDestroyObject && !GetEntityLeftAfterCleanup() && !m_ScriptGrabbedFromWorld && !m_GameObjectBeingDestroyed);

	if (pObject)
	{
		// unregister the object with the scripts here when the object still has a network object ptr and the reservations can be adjusted

		// need to restore the game heap here in case the object deallocates from the heap
		sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		if(m_ScriptGrabbedFromWorld && IsClone())
		{	//B* 1758761 - Set this clone as null here so that upcoming CTheScripts::UnregisterEntity
			//will know it can unregister the script object even though it's a clone - see check in CGameScriptHandlerObject::IsCleanupRequired
			pObject->SetNetworkObject(NULL);
		}

		CTheScripts::UnregisterEntity(pObject, !bObjectShouldBeDestroyed);

		if(m_ScriptGrabbedFromWorld && IsClone())
		{
			// make sure the object is made random again so that it is cleaned up properly
			pObject->SetOwnedBy( ENTITY_OWNEDBY_RANDOM );
		}

		sysMemAllocator::SetCurrent(*oldHeap);

		pObject->SetNetworkObject(NULL);

		// random objects tidy themselves up when they go out of scope
		// with the player, so we don't need to delete them manually.
		if (IsAmbientObjectType(pObject->GetOwnedBy()))
		{
			// if m_UnregisteringDueToOwnershipConflict is set, the object is being passed from one network object to another so we don't want to 
			// change its state
			if (!m_UnregisteringDueToOwnershipConflict)
			{
				if (bObjectShouldBeDestroyed && (pObject->m_nFlags.bHasExploded || (IsClone() && pObject->GetIsAttached())))
				{
					if (pObject->GetIsAttached())
					{
						pObject->DetachFromParent(0);
					}

					// need to restore the game heap here in case the object deallocates from the heap
					sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
					sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

					// exploded objects must be converted back to dummy so that they do not get left exploded and unnetworked
					CObjectPopulation::ConvertToDummyObject(pObject, true);

					sysMemAllocator::SetCurrent(*oldHeap);
				}
				else
				{
					// clear the moved flag so the object can be registered again locally if moved again
					pObject->ResetMovedInNetworkGame();

					// clear any visibility altered by the net code
					SetIsVisibleForModuleSafe(pObject, SETISVISIBLE_MODULE_NETWORK, true, true);

					// restore collision if we are overriding it
					if (GetOverridingLocalCollision())
					{
						ClearOverridingLocalCollisionImmediately("Object being unregistered");
					}

					if (m_ScriptGrabbedFromWorld)
					{
						pObject->DestroyScriptExtension();
					}
				}
			}
		}
		else if(bObjectShouldBeDestroyed)
		{
			if (AssertVerify(!m_UnregisteringDueToOwnershipConflict))
			{
				// need to restore the game heap here in case the object constructor/destructor allocates from the heap
				sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
				sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

				// make sure it is on the scene update so it actually gets removed
                if(!pObject->GetIsOnSceneUpdate() || !gnetVerifyf(pObject->GetUpdatingThroughAnimQueue() || pObject->GetIsMainSceneUpdateFlagSet(),
                                                                  "Flagging an object for removal (%s) that is on neither the main scene update or update through anim queue list! This will likely cause a object leak!", GetLogName()))
                {
                    pObject->AddToSceneUpdate();
                }

				pObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);

				sysMemAllocator::SetCurrent(*oldHeap);
			}
		}
	}

	SetGameObject(0);

	m_GameObjectBeingDestroyed = false;

	if(IsClone())
	{
		BANK_ONLY(NetworkDebug::AddOtherCloneRemove());
	}
}

//
// name:        IsInScope
// description: This is used by the object manager to determine whether we need to create a
//              clone to represent this object on a remote machine. The decision is made using
//              the player that is passed into the method - this decision is usually based on
//              the distance between this player's players and the network object, but other criterion can
//              be used.
// Parameters:  player - the player that scope is being tested against
bool CNetObjObject::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
    CObject* pObject = GetCObject();

    if (CNetObjPhysical::IsInScope(player, scopeReason))
    {
        // can't send creation data if there is no frag parent
        if (!HasBeenCloned(player) && pObject && IsFragmentObjectType(pObject->GetOwnedBy()) && !pObject->GetFragParent())
        {
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_OBJECT_NO_FRAG_PARENT;
			}
            return false;
        }

		// if another networked entity is attached to this one, and cloned on this player's machine, we need to make this entity in scope too
		if (pObject && pObject->GetChildAttachment() && !((CEntity*)pObject->GetChildAttachment())->GetIsTypePed())
		{
			netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity((CEntity*)pObject->GetChildAttachment());

			if (pNetObj && pNetObj->IsInScope(player))
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_IN_OBJECT_ATTACH_CHILD_IN_SCOPE;
				}
				return true;
			}
		}

		if(pObject && pObject->m_nObjectFlags.bIsNetworkedFragment)
		{
			const CEntity* fragParent = pObject->GetFragParent();
			if(!fragParent || !fragParent->GetIsTypeVehicle())
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_OUT_OBJECT_FRAGMENT_PARENT_OUT_SCOPE;
				}
				return false;
			}
			else
			{
				const CVehicle* parentVehicle = SafeCast(const CVehicle, fragParent);
				const netObject* netParentVehicle = parentVehicle->GetNetworkObject();
				if(!netParentVehicle || !netParentVehicle->IsInScope(player))
				{
					if (scopeReason)
					{
						*scopeReason = SCOPE_OUT_OBJECT_FRAGMENT_PARENT_OUT_SCOPE;
					}
					return false;
				}
			}
		}

		return true;
    }

	// If this object is a part of a vehicle gadget on a vehicle that has been cloned, make sure this is also in scope.
	if(pObject && pObject->m_pGadgetParentVehicle && pObject->m_pGadgetParentVehicle->GetNetworkObject() && pObject->m_pGadgetParentVehicle->GetNetworkObject()->IsInScope(player))
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_IN_OBJECT_VEHICLE_GADGET_IN_SCOPE;
		}
		return true;
	}

	if(pObject && pObject->GetIsAttached() && pObject->GetAttachParent() && ((CEntity*)pObject->GetAttachParent())->GetIsTypeVehicle())
	{
		netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity((CEntity*)pObject->GetAttachParent());

		if(pNetObj && pNetObj->IsInScope(player))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_OBJECT_ATTACH_PARENT_IN_SCOPE;
			}
			return true;
		}
	}

    return false;
}

// name:        ManageUpdateLevel
// description: Decides what the default update level should be for this object
void CNetObjObject::ManageUpdateLevel(const netPlayer& player)
{
#if __BANK
	if(ms_updateRateOverride >= 0)
	{
		SetUpdateLevel( player.GetPhysicalPlayerIndex(), static_cast<u8>(ms_updateRateOverride) );
		return;
	}
#endif

	CNetObjPhysical::ManageUpdateLevel(player);
}

bool CNetObjObject::IsInHeightScope(const netPlayer& playerToCheck, const Vector3& entityPos, const Vector3& playerPos, unsigned* scopeReason) const
{
    CObject *object = GetCObject();

    if(object && m_fragParentVehicle == NETWORK_INVALID_OBJECT_ID && IsAmbientObjectType(object->GetOwnedBy()) && !object->IsADoor())
    {
		Vector3 diff = entityPos - playerPos;

		if (fabs(diff.z) > ms_objectScopeData.m_scopeDistanceHeight)
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_OBJECT_HEIGHT_RANGE;
			}
			return false;
		}
    }

    return CNetObjPhysical::IsInHeightScope(playerToCheck, entityPos, playerPos, scopeReason);
}

bool CNetObjObject::CanDelete(unsigned* LOGGING_ONLY(reason))
{
	CObject *object = GetCObject();

	// Ambient objects can always be deleted at any point as we
    // don't have control of which objects are removed by the IPL system
	if (!object || IsAmbientObjectType(object->GetOwnedBy()))
	{
		return true;
	}

	return CNetObjPhysical::CanDelete(LOGGING_ONLY(reason));
}

bool CNetObjObject::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	CObject *object = GetCObject();

	// don't pass control of just exploded objects that are about to broadcast their state
	if (object && ((u32)m_HasExploded != object->m_nFlags.bHasExploded))
	{
		if (resultCode)
		{
			*resultCode = CAC_FAIL_UNEXPLODED;
		}

		return false;
	}

	// Don't pass control of objects that are a part of a vehicle gadget. These should remain under the control of the player who owns the vehicle and gadget.
	if(object && object->m_nObjectFlags.bIsVehicleGadgetPart)
	{
		netPlayer* pPlayerOwner = (object->m_pGadgetParentVehicle && object->m_pGadgetParentVehicle->GetNetworkObject()) ? object->m_pGadgetParentVehicle->GetNetworkObject()->GetPlayerOwner() : NULL;

		if (pPlayerOwner && &player != pPlayerOwner)
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_VEHICLE_GADGET_OBJECT;
			}

			return false;
		}
	}

	if(object && object->m_nObjectFlags.bRemoveFromWorldWhenNotVisible == true)
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_REMOVE_FROM_WORLD_WHEN_INVISIBLE;
		}

		return false;		
	}

	return CNetObjPhysical::CanPassControl(player, migrationType, resultCode);
}

bool CNetObjObject::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	CObject *object = GetCObject();

	// don't accept control of objects trying to explode
	if (object && ((u32)m_HasExploded != object->m_nFlags.bHasExploded))
	{
		if (resultCode)
		{
			*resultCode = CAC_FAIL_UNEXPLODED;
		}

		return false;
	}

	return CNetObjPhysical::CanAcceptControl(player, migrationType, resultCode);
}

void CNetObjObject::PostCreate()
{
    CObject *object = GetCObject();

	if (object && IsClone() && m_bDelayedCreateDrawable)
	{
		//Explicitly invoke CreateDrawable after the object has been added to the world to fix issues with attached lights - fixes url:bugstar:2099312. lavalley.
		object->CreateDrawable();
		m_bDelayedCreateDrawable = false; //reset
	}

	if(GetScriptObjInfo())
	{
		// if the object is created invisible, prevent it fading in once made visible. This necessary for objects made visible during synced scenes, etc, when swapping out static objects.
		if (object && !object->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT))
		{
			object->GetLodData().SetResetDisabled(true);
		}

		if (m_ScriptGrabbedFromWorld && gnetVerifyf(object, "No game object for networked object!"))
		{
			CScriptEntityExtension *extension = object->GetExtension<CScriptEntityExtension>();

			if(gnetVerifyf(extension, "Failed to get the script extension!"))
			{
				extension->SetScriptObjectWasGrabbedFromTheWorld(true);

				CBaseModelInfo *modelInfo = object->GetBaseModelInfo();

				if(gnetVerifyf(modelInfo, "No model info for networked object!"))
				{
					CDummyObject      *relatedDummy         = object->GetRelatedDummy();
					fwInteriorLocation relatedDummyLocation = object->GetRelatedDummyLocation();
					s32                iplIndexOfObject     = object->GetIplIndex();

					object->SetIplIndex(0);
					object->SetupMissionState();

					s32 scriptGUID = CTheScripts::GetGUIDFromEntity(*object);

					spdSphere searchVolume((Vec3V) m_ScriptGrabPosition, ScalarV(m_ScriptGrabRadius));
					CTheScripts::GetHiddenObjects().AddToHiddenObjectMap(scriptGUID, searchVolume, modelInfo->GetHashKey(), 
						relatedDummy, relatedDummyLocation, iplIndexOfObject);
				}
			}
		}
	}

	// *** temporary hack fix to prevent for sale signs breaking ****
	if (IsClone() && object && ((object->GetModelId() == MI_PROP_FORSALE_DYN_01) || (object->GetModelId() == MI_PROP_FORSALE_DYN_02)))
	{
		object->m_nPhysicalFlags.bNotDamagedByAnything = true;
		m_DisableBreaking = true;
		m_DisableDamage	= true;

		if (object->GetFragInst())
		{
			object->GetFragInst()->SetDisableBreakable(true);
			object->GetFragInst()->SetDisableDamage(true);
		}
	}
	
	if(IsClone())
    {
        BANK_ONLY(NetworkDebug::AddOtherCloneCreate());
    }

    // objects flagged to allow network blending when fixed use an extended blender pop distance
    if(m_bCanBlendWhenFixed)
    {
        netBlenderLinInterp *netBlender = SafeCast(netBlenderLinInterp, GetNetBlender());

        if(netBlender)
        {
            const float FIXED_BY_NETWORK_MULTIPLIER = 2.0f;
            netBlender->SetMaxPositionDeltaMultiplier(FIXED_BY_NETWORK_MULTIPLIER);
        }
    }

	CNetObjPhysical::PostCreate();
}

void CNetObjObject::PostMigrate(eMigrationType migrationType)
{
	CNetObjPhysical::PostMigrate(migrationType);

	// Force update to happen straight away rather than waiting for batch update
    if (m_bIsArenaBall)
    {  
		// check if the objects needs to change its update level to the remote players in the game
		for (u8 index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
		{
			const netPlayer *remotePlayer = netInterface::GetPlayerMgr().GetPhysicalPlayerFromIndex(index);
			if (remotePlayer && remotePlayer != GetPlayerOwner())
			{
				if (HasBeenCloned(*remotePlayer))
				{
                    PhysicalPlayerIndex playerIndex = remotePlayer->GetPhysicalPlayerIndex();
                    u8 oldUpdateLevel = GetUpdateLevel(playerIndex);
                    u8 newUpdateLevel = GetLowestAllowedUpdateLevel();
                    if (newUpdateLevel > oldUpdateLevel
#if __BANK
                        && ms_updateRateOverride < 0 // Don't update here if we are using debug to force the update level
#endif
                        ) // Only call ManageUpdateLevel() if new update level > old. Otherwise it will check for IsInCurrentUpdateBatch() which requires to be called after UpdateCurrentObjectBatches()
                    {
                        ManageUpdateLevel(*remotePlayer);
                    }
				}
			}
		}
	}
}

void CNetObjObject::OnUnregistered()
{
	if (m_UnassignedAmbientObject)
	{
		RemoveUnassignedAmbientObject(*this, "Object unregistering", false);
	}
	else if(m_UnassignedVehicleFragment)
	{
		CNetObjObject::RemoveUnassignedVehicleFragment(*this);
	}

	CNetObjPhysical::OnUnregistered();
}

bool CNetObjObject::CanSynchronise(bool bOnRegistration) const
{
	CObject *object = GetCObject();

	if (!object)
	{
		return false;
	}

	if (object && !bOnRegistration && !object->GetIsParachute() && !IsScriptObject() && !IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT))
	{
		if (m_AmbientFence)
		{
			// we don't want to sync uprooted fences, we only need to break them by sending a create
			return false;
		}
		else if (object->IsAsleep() && (fwTimer::GetSystemTimeInMilliseconds() - m_lastTimeBroken) > 2000)
		{
			// stop syncing ambient objects that have come to rest
			return false;
		}
	}

	return CNetObjPhysical::CanSynchronise(bOnRegistration);
}

bool CNetObjObject::CanBlend(unsigned *resultCode) const
{
	if(NetworkBlenderIsOverridden(resultCode))
	{
		if(resultCode)
		{
			*resultCode = CB_FAIL_BLENDER_OVERRIDDEN;
		}

		return false;
	}

	CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());
	if (netBlenderPhysical && IsArenaBall() && netBlenderPhysical->HasStoppedPredictingPosition())
	{
		if (resultCode)
		{
			*resultCode = CB_FAIL_STOPPED_PREDICTING;
		}

		return false;
	}

    return CNetObjPhysical::CanBlend(resultCode);
}

bool CNetObjObject::CanBlendWhenFixed() const
{
    if(IsClone() && m_bCanBlendWhenFixed)
    {
        return true;
    }

    return CNetObjPhysical::CanBlendWhenFixed();
}

bool CNetObjObject::NetworkBlenderIsOverridden(unsigned *resultCode) const
{
	CObject* object = GetCObject();

	if(object)
	{
        if (IsArenaBall()) 
        {
            if (m_ArenaBallBlenderOverrideTimer > 0)
            {
                if (resultCode)
                {
                    *resultCode = NBO_ARENA_BALL_TIMER;
                }

                return true;
            }
        }
        else if(object->GetIsParachute())
		{
			if(object->GetObjectIntelligence())
			{
				CTaskParachuteObject* parachuteObjTask = static_cast<CTaskParachuteObject*>(object->GetObjectIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_PARACHUTE_OBJECT));
				if(parachuteObjTask)
				{
					if(parachuteObjTask->OverridesNetworkBlender(NULL))
					{
						if(resultCode)
						{
							*resultCode = NBO_PED_TASKS;
						}

						return true;
					}
				}
			}
		}
		else
		{
			if(m_fragParentVehicle == NETWORK_INVALID_OBJECT_ID && !IsScriptObject() && !IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT))
			{
				// we don't want to blend broken objects, they should just be broken
				if (m_AmbientFence || object->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
				{
					if (resultCode)
					{
						*resultCode = NBO_FRAG_OBJECT;
					}

					return true;
				}
			}
			
			if(object && object->m_nObjectFlags.bIsVehicleGadgetPart && object->m_nParentGadgetIndex > -1 && object->m_pGadgetParentVehicle && object->m_pGadgetParentVehicle->GetVehicleGadget(object->m_nParentGadgetIndex)->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				if(resultCode)
				{
					*resultCode = NBO_MAGNET_PICKUP;
				}

				return true;				
			}
		
			if (object && m_fragSettledOverrideBlender)
			{
				if(resultCode)
				{
					*resultCode = NBO_VEHICLE_FRAG_SETTLED;
				}

				return true;
			}		
		}
	}

	return false;
}

bool CNetObjObject::NetworkAttachmentIsOverridden(void) const
{
	CObject* object = GetCObject();

	if(object && object->GetIsParachute() && object->GetObjectIntelligence())
	{
		CTaskParachuteObject* parachuteObjTask = static_cast<CTaskParachuteObject*>(object->GetObjectIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_PARACHUTE_OBJECT));
		if(parachuteObjTask)
		{
			if(parachuteObjTask->OverridesNetworkAttachment(NULL))
			{
				return true;
			}
		}
	}

	// During the pick-up sequence and while the magnet is carrying something, we want to override the attachment state so we can better predict the behaviour locally.
	if(object && object->m_nObjectFlags.bIsVehicleGadgetPart && object->m_nParentGadgetIndex > -1 && object->m_pGadgetParentVehicle && object->m_pGadgetParentVehicle->GetVehicleGadget(object->m_nParentGadgetIndex)->GetType() == VGT_PICK_UP_ROPE_MAGNET)
	{
		CVehicleGadgetPickUpRopeWithMagnet *pMagnet = static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(object->m_pGadgetParentVehicle->GetVehicleGadget(object->m_nParentGadgetIndex));
		if((pMagnet->GetIsActive() && pMagnet->GetTargetEntity() != NULL && pMagnet->GetAttachedEntity() == NULL) || (!pMagnet->GetIsActive() && pMagnet->GetAttachedEntity() != NULL))
		{
			return true;
		}
	}

	return false;
}

void CNetObjObject::ValidateGameObject(void* gameObject) const
{
	// ambient objects can have no game object, they can wait until they stream in
	if (!m_UnassignedAmbientObject && !m_UnassignedVehicleFragment)
	{
		CNetObjPhysical::ValidateGameObject(gameObject);
	}
}

void CNetObjObject::OnConversionToAmbientObject()
{
	CObject* object = GetCObject();
	int tintIndex = object ? object->GetTintIndex() : 0;

	CNetObjPhysical::OnConversionToAmbientObject();

	if(object)
	{
		SetTintIndexOnObjectHelper((u8)tintIndex); // Setting tint index of object
	}
}

bool CNetObjObject::CanShowGhostAlpha() const
{
	CObject* object = GetCObject();
	if(object)
	{
		const CEntity* attachParent =  SafeCast(const CEntity, object->GetAttachParent());
		if(attachParent && attachParent->GetIsTypeVehicle())
		{
			const CVehicle* parentVehicle = SafeCast(const CVehicle, attachParent);
			if(parentVehicle)
			{
				const CPed* pDriver = parentVehicle->GetDriver();
				if(pDriver && pDriver->IsPlayer())
				{
					if(NetworkInterface::IsSpectatingPed(pDriver))
					{
						return GetInvertGhosting();
					}
				}
			}
		}
	}

	return CNetObjPhysical::CanShowGhostAlpha();
}

CPhysical *CNetObjObject::GetAttachOrDummyAttachParent() const
{
	CObject* object = GetCObject();
	if(object && object->m_nObjectFlags.bIsNetworkedFragment)
	{
		CEntity* parentFrag = object->GetFragParent();
		if (parentFrag && parentFrag->GetIsTypeVehicle())
		{
			CVehicle* parentVehicle = SafeCast(CVehicle, parentFrag);
			if(parentVehicle && parentVehicle->GetNetworkObject())
			{
				return parentVehicle;
			}
		}
	}

	return CNetObjPhysical::GetAttachOrDummyAttachParent();
}

void CNetObjObject::DisplayNetworkInfo()
{
	if (HasGameObject())
	{
		CNetObjPhysical::DisplayNetworkInfo();
	}
	else
	{
#if __BANK
		const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

		if (displaySettings.m_displayObjectInfo)
		{
			float   scale = 1.0f;
			Vector2 screenCoords;
			if(NetworkUtils::GetScreenCoordinatesForOHD(GetPosition(), screenCoords, scale))
			{
				const unsigned DEBUG_STR_LEN = 200;
				char debugStr[DEBUG_STR_LEN];

				NetworkColours::NetworkColour colour = NetworkUtils::GetDebugColourForPlayer(GetPlayerOwner());
				Color32 playerColour = NetworkColours::GetNetworkColour(colour);

				if (IsPendingOwnerChange())
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s* (Unassigned)", GetLogName());
				}
				else
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s %s (Unassigned)", GetLogName(), (!IsClone() && GetSyncData()) ? "(S)" : "");
				}

				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
			}
		}
#endif // __BANK
	}
}

bool CNetObjObject::CanBeNetworked(CObject* pObject, bool bIsBroken, bool bIsScript)
{
	if (pObject->m_nObjectFlags.bIsPickUp || pObject->IsADoor())
	{
		return false;
	}

	if (pObject->GetNetworkObject())
	{
		return false;
	}

	bool bCanBeNetworked = false;

	switch (pObject->GetOwnedBy())
	{
	case ENTITY_OWNEDBY_RANDOM:
		{
			if (pObject->GetRelatedDummy())
			{
				if (bIsScript)
				{
					bCanBeNetworked = true;
				}
				else
				{
					bool bIsFixed = pObject->GetIsAnyFixedFlagSet();

					if (!bIsFixed && !bIsBroken)
					{
						fragInst* pFragInst = pObject->GetFragInst();

						if (pFragInst && pFragInst->GetCacheEntry())
						{
							bIsFixed = 	pFragInst->GetCacheEntry()->GetHierInst()->body && 
								pFragInst->GetCacheEntry()->GetHierInst()->body->RootIsFixed();

							bIsFixed |= pFragInst->GetTypePhysics() && pFragInst->GetTypePhysics()->GetMinMoveForce() < 0.0f;
						}
					}

					if (!bIsFixed)
					{
						bCanBeNetworked = true;
					}
				}
			}
		}
		break;
	case ENTITY_OWNEDBY_FRAGMENT_CACHE:
		{
			CObject* pFragParent = NULL;

			if (pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
			{
				pFragParent = SafeCast(CObject, pObject->GetFragParent());
			}

			if (pFragParent && CanBeNetworked(pFragParent, true))
			{
				bCanBeNetworked = true;
			}
		}
		break;
	case ENTITY_OWNEDBY_SCRIPT:
		{
			bCanBeNetworked = true;
		}
		break;
	default:
		break;
	}

	return bCanBeNetworked;
}

// name:        HasToBeCloned
// description: Decides whether an object needs to be cloned on other machines
bool CNetObjObject::HasToBeCloned(CObject* pObject, bool bIsBroken)
{
    bool bHasToBeCloned = false;

	if (!CanBeNetworked(pObject, bIsBroken))
	{
		return false;
	}

	// don't sync floating objects
	if (pObject->m_nObjectFlags.bFloater || pObject->GetIsInWater() || pObject->m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode)
	{
		return false;
	}

	// check that the object does not already have a network object waiting to be assigned to it
	AssignAmbientObject(*pObject, false);

	TUNE_FLOAT(MIN_UPROOTED_PROP_HEIGHT_TO_SYNC, 2.0f, 0.0f, 4.0f, 0.1f);
	TUNE_FLOAT(MIN_UPROOTED_FORCE_TO_SYNC, 100.0f, 0.0f, 1000.0f, 10.0f);

    switch (pObject->GetOwnedBy())
    {
    case ENTITY_OWNEDBY_SCRIPT:
        {
            // mission objects are always cloned
            bHasToBeCloned = true;
        }
        break;
    case ENTITY_OWNEDBY_RANDOM:
        {
			if (!pObject->GetCurrentPhysicsInst())
				return false;

			// dont clone objects in interiors
			if (pObject->m_nFlags.bInMloRoom)
				return false;

			CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
			if(pModelInfo && pModelInfo->GetSyncObjInNetworkGame())
				bHasToBeCloned = true;
 
			// we are only interested in frags that stick up from the ground (have a large z difference in their bounds)
			if (pObject->m_nObjectFlags.bHasBeenUprooted || bIsBroken)
			{
				fragInst* pFragInst = pObject->GetFragInst();

				if (pFragInst && pFragInst->GetTypePhysics())
				{
					float minMoveForce = pFragInst->GetTypePhysics()->GetMinMoveForce();

					if (minMoveForce > MIN_UPROOTED_FORCE_TO_SYNC)
					{
						float propHeight = pObject->GetBoundingBoxMax().z - pObject->GetBoundingBoxMin().z;

						if (propHeight >= MIN_UPROOTED_PROP_HEIGHT_TO_SYNC)
						{
							return true;
						}
					}
				}
			}
		}
        break;
    case ENTITY_OWNEDBY_FRAGMENT_CACHE:
        {
			if (ms_gettingFragObjectFromParent)
				return false;

			if(pObject->m_nFlags.bInMloRoom)
				return false;

			// only sync uprooted frags for now
			if (!pObject->m_nObjectFlags.bHasBeenUprooted)
				return false;
	
			CObject* pFragParent = SafeCast(CObject, pObject->GetFragParent());
			
			if (HasToBeCloned(pFragParent, true))
			{
				return true;
			}

			bHasToBeCloned = true;

			// we are only interested in frags that stick up from the ground (have a large z difference in their bounds)
			float propHeight = pObject->GetBoundingBoxMax().z - pObject->GetBoundingBoxMin().z;

			if (propHeight < MIN_UPROOTED_PROP_HEIGHT_TO_SYNC)
			{
				bHasToBeCloned = false;
			}
					
			// we currently don't support multiple networked frags breaking off the same object. If these needs to be implemented in future 
			// we need to fix the problem of CObject::AddPhysics() calling AssignAmbientObject on frags that have not been added to the level
			// yet. These would need to be added later somehow.
			if (!ALLOW_MULTIPLE_NETWORKED_FRAGS_PER_OBJECT && (pFragParent->HasNetworkedFrags() || pFragParent->GetNetworkObject()))
			{
				bHasToBeCloned = false;
			}

			// don't sync completely broken frag cache objects
			if (pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->CountOnBits() == pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->GetNumBits())
			{
				bHasToBeCloned = false;
			}

			if (bHasToBeCloned)
			{
				pFragParent->SetHasNetworkedFrags();
			}
        }
        break;
    default:
        break;
    }

    return bHasToBeCloned;
}

float CNetObjObject::GetScopeDistance(const netPlayer* pRelativePlayer) const
{
	CObject *pObject = GetCObject();
	VALIDATE_GAME_OBJECT(pObject);

	static const float AMBIENT_EXTENDED_SCOPE_DIST = 60.0f;

	if (m_ScriptAdjustedScopeDistance > 0)
	{
		return (float)m_ScriptAdjustedScopeDistance;
	}

	if(m_HasBeenPickedUpByHook)
	{
		const float PICKED_UP_BY_HOOK_SCOPE_DISTANCE = 300.0f;
		return PICKED_UP_BY_HOOK_SCOPE_DISTANCE;
	}
	
	if (pObject && IsAmbientObjectType(pObject->GetOwnedBy()))
	{
		// extend the scope for uprooted ambient props so that they will be moved as soon as they stream in (this is for lampposts mainly)
		return ms_objectScopeData.m_scopeDistance + AMBIENT_EXTENDED_SCOPE_DIST;
	}

	return CNetObjPhysical::GetScopeDistance(pRelativePlayer);
}

Vector3 CNetObjObject::GetScopePosition() const
{
	CObject* pObject = GetCObject();
	VALIDATE_GAME_OBJECT(pObject);

	// use the dummy object position as the scope position for ambient objects
	if (!pObject || IsAmbientObjectType(pObject->GetOwnedBy()))
	{
		return m_DummyPosition;
	}

	return CNetObjPhysical::GetScopePosition();
}

Vector3 CNetObjObject::GetPosition() const
{
	if (!HasGameObject())
	{
		return m_DummyPosition;
	}

	return CNetObjPhysical::GetPosition();
}

bool CNetObjObject::IsAmbientObjectType(u32 type)
{
    return (type == ENTITY_OWNEDBY_RANDOM || type == ENTITY_OWNEDBY_FRAGMENT_CACHE);
}

bool CNetObjObject::IsFragmentObjectType(u32 type)
{
    return (type == ENTITY_OWNEDBY_FRAGMENT_CACHE);
}

bool CNetObjObject::IsScriptObjectType(u32 type)
{
    return (type == ENTITY_OWNEDBY_SCRIPT);
}

bool CNetObjObject::AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason)
{
	bool bSuccess = CNetObjPhysical::AttemptPendingAttachment(pEntityToAttachTo, reason);

	if (bSuccess)
	{
		// set this flag here so that unattached objects that migrate locally get activated when they are unfrozen. This fixes bugs where objects attached
		// to players that leave get left floating in the air.
		GetCObject()->m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = true;
	}

	return bSuccess;
}

u8 CNetObjObject::GetLowestAllowedUpdateLevel() const
{
	if(IsActiveVehicleFrag())
	{
		return CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH;
	}
	else if(GetCObject() && GetCObject()->GetIsCarParachute())
	{
		return CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
	}
	else if(m_UseParachuteBlending)
	{
		return CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
	}
    else if(m_UseHighPrecisionBlending || m_bIsArenaBall)
    {
        return CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH;
    }
	else
    {
        return CNetObjPhysical::GetLowestAllowedUpdateLevel();
    }	
}

bool CNetObjObject::IsActiveVehicleFrag() const
{
	// If we are a vehicle frag (toombstone for example) and moving fairly fast, we need to force the obj manager to update the scope and update rate ASAP
	if(m_fragParentVehicle != NETWORK_INVALID_OBJECT_ID)
	{
		CObject* pObject = GetCObject();
		if (pObject)
		{
			const float FORCE_UPDATE_IN_SCOPE_FOR_VEHICLE_FRAGS_THRESHOLD = rage::square(3.0f);
			float velocityMag = pObject->GetVelocity().Mag2();

			if (velocityMag >= FORCE_UPDATE_IN_SCOPE_FOR_VEHICLE_FRAGS_THRESHOLD)
			{				
				return true;
			}			
		}
	}

	return false; 
}

bool CNetObjObject::ForceManageUpdateLevelThisFrame()
{ 
	if ((m_forceScopeCheckUntilFirstUpdateTimer > 0) && IsActiveVehicleFrag())
	{
		return true;
	}

	return false;
}


bool CNetObjObject::UseBoxStreamerForFixByNetworkCheck() const
{
    return IsImportant();
}

bool CNetObjObject::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
	gnetAssert(GetCObject());

	if(GetCObject()->GetIsParachute())
    {
		if(npfbnReason)
		{	
			*npfbnReason = NPFBN_IS_PARACHUTE;
		}
		return false;
    }

	if(GetCObject()->m_nObjectFlags.bIsVehicleGadgetPart)
	{
		if(npfbnReason)
		{	
			*npfbnReason = NPFBN_IS_VEHICLE_GADGET;
		}
		return false;
	}

	return CNetObjPhysical::FixPhysicsWhenNoMapOrInterior(npfbnReason);
}

// sync parser create functions
void CNetObjObject::GetObjectCreateData(CObjectCreationDataNode& data)  const
{
    CObject* pObject = GetCObject();

    gnetAssert(!pObject || pObject->m_nFlags.bIsProcObject==false);

	if (pObject)
	{
		data.m_ownedBy = m_bCloneAsTempObject ? ENTITY_OWNEDBY_TEMP : pObject->GetOwnedBy();
	}
	else
	{
		gnetAssert(m_UnassignedAmbientObject);
		data.m_ownedBy = (m_FragGroupIndex > 0) ? ENTITY_OWNEDBY_FRAGMENT_CACHE : ENTITY_OWNEDBY_RANDOM;
	}

	data.m_IsFragObject      = m_FragGroupIndex != -1;
    data.m_CanBlendWhenFixed = m_bCanBlendWhenFixed;

    if ( !IsAmbientObjectType(data.m_ownedBy) )
    {
        // write model index
        CBaseModelInfo *modelInfo = pObject->GetBaseModelInfo();
        if(gnetVerifyf(modelInfo, "No model info for networked object!"))
        {
            data.m_modelHash = modelInfo->GetHashKey();
        }
        
		data.m_noReassign		      = (GetLocalFlags() & netObject::LOCALFLAG_NOREASSIGN) != 0; // Used by parachutes, owned by ENTITY_OWNEDBY_GAME
        data.m_hasInitPhysics         = pObject->m_nObjectFlags.bHasInitPhysics;
        data.m_ScriptGrabbedFromWorld = m_ScriptGrabbedFromWorld;

        if(data.m_ScriptGrabbedFromWorld)
        {
            data.m_ScriptGrabRadius   = m_ScriptGrabRadius;
            data.m_ScriptGrabPosition = m_ScriptGrabPosition;
        }
    }
    else
    {
		data.m_dummyPosition	= m_DummyPosition;
		data.m_fragGroupIndex	= m_FragGroupIndex;
		data.m_IsBroken			= false;
		data.m_IsAmbientFence	= m_AmbientFence;
		data.m_KeepRegistered	= m_KeepRegistered;
		data.m_DestroyFrags		= m_DestroyFrags;

		// only generate frags for nearby players if the object has just been broken and networked, otherwise just remove them
		if (fwTimer::GetSystemTimeInMilliseconds() - m_lastTimeBroken > 2000)
		{
			data.m_DestroyFrags = true;
		}

		if (pObject)
        {
			CObject* pParent = pObject;
			
			if (pObject->GetFragParent() && !pObject->m_nObjectFlags.bIsNetworkedFragment && gnetVerifyf(pObject->GetFragParent()->GetIsTypeObject(), "%s has a frag parent that is not a CObject!", GetLogName()))
			{
				pParent = SafeCast(CObject, pObject->GetFragParent());
			}

			data.m_hasGameObject = true;

			// write whether we think we have control
			data.m_playerWantsControl = (pParent->m_nObjectFlags.bWeWantOwnership == 1);

			data.m_IsBroken = IsBroken();
			data.m_HasExploded = m_HasExploded;
		}
        else
        {
			data.m_hasGameObject = false;
			data.m_ownershipToken = GetOwnershipToken();
			data.m_objectMatrix = m_LastReceivedMatrix;
		}
    }

	if (pObject)
	{
		data.m_lodOrphanHd = pObject->GetLodData().IsOrphanHd();
		data.m_lodDistance = pObject->GetLodData().GetLodDistance();
	}
	else
	{
		data.m_lodOrphanHd = false;
	}

	data.m_fragParentVehicle = m_fragParentVehicle;
}

void CNetObjObject::SetObjectCreateData(const CObjectCreationDataNode& data)
{
	CObject* pIsolatedObject = NULL;

    m_bCanBlendWhenFixed = data.m_CanBlendWhenFixed;

	m_fragParentVehicle = data.m_fragParentVehicle;
	if(m_fragParentVehicle != NETWORK_INVALID_OBJECT_ID)
	{
		KeepRegistered();
	}

    if (!IsAmbientObjectType(data.m_ownedBy))
    {
        // ensure that the model is loaded
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(data.m_modelHash, &modelId);

        if (AssertVerify(CModelInfo::HaveAssetsLoaded(modelId)))
        {
            m_ScriptGrabbedFromWorld = data.m_ScriptGrabbedFromWorld;

            if(m_ScriptGrabbedFromWorld)
            {
                CEntity *closestEntity = CObject::GetPointerToClosestMapObjectForScript(data.m_ScriptGrabPosition.x,
                                                                                        data.m_ScriptGrabPosition.y,
                                                                                        data.m_ScriptGrabPosition.z,
                                                                                        data.m_ScriptGrabRadius,
                                                                                        data.m_modelHash);

	            if(gnetVerifyf(closestEntity, "Failed to find closest entity for network object grabbed by script from world!"))
	            {
                    CObject *closestObject = 0;

	                if(closestEntity->GetIsTypeDummyObject())
	                {
		                CDummyObject *dummyObject = static_cast<CDummyObject*>(closestEntity);

		                closestObject = CObjectPopulation::ConvertToRealObject(dummyObject);
                    }
                    else if(gnetVerifyf(closestEntity->GetIsTypeObject(), "Closest entity grabbed by script from world is not an object or dummy object!"))
                    {
                        closestObject = static_cast<CObject*>(closestEntity);
                    }

                    if(closestObject)
                    {
                        if (!closestObject->GetIsOnSceneUpdate())
		                {
			                closestObject->AddToSceneUpdate();
		                }

                        SetGameObject((void*) closestObject);
                        closestObject->SetNetworkObject(this);

						pIsolatedObject = closestObject;
                    }
                }
            }
            else
            {
				// create a new object
				CObjectPopulation::CreateObjectInput input(modelId, (eEntityOwnedBy) data.m_ownedBy, true);
				input.m_bInitPhys = data.m_hasInitPhysics;
				input.m_bClone = false;
				input.m_iPoolIndex = -1;
				input.m_bForceObjectType = true; // force this to be an object (the scripts may create a CObject using a door model, this would get created as a CDoor without setting this flag)
				
				CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
				if (pModelInfo && pModelInfo->Has2dEffects())
				{
					m_bDelayedCreateDrawable = true;
					input.m_bCreateDrawable = false; //if the object has 2deffects - lights - then prevent the createdrawable in createobject - it will occur in PostCreate - lavalley.
				}

				CObject* pObject = CObjectPopulation::CreateObject(input);

                NETWORK_QUITF(pObject, "Failed to create a cloned object. Cannot continue.");

                if (data.m_ownedBy == ENTITY_OWNEDBY_SCRIPT)
                    pObject->SetupMissionState();
                else if (data.m_ownedBy == ENTITY_OWNEDBY_TEMP)
                    pObject->m_nEndOfLifeTimer = 0xffffffff; // set end of life timer to max as we only want the object to get removed when it gets too far away from its owner

				SetGameObject((void*) pObject);
                pObject->SetNetworkObject(this);

				m_DummyPosition = data.m_dummyPosition;

				SetLocalFlag(netObject::LOCALFLAG_NOREASSIGN, data.m_noReassign);

#if PHINST_VALIDATE_POSITION
                // set the x,y and z components to random numbers to avoid a physics assert
                Vector3 tempPos;
                tempPos.x = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
                tempPos.y = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
                tempPos.z = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
                pObject->SetPosition(tempPos);
#endif // PHINST_VALIDATE_POSITION

                CGameWorld::Add(pObject, CGameWorld::OUTSIDE, !data.m_hasInitPhysics);

				REPLAY_ONLY(CReplayMgr::RecordObject(pObject));

				pObject->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);
				pObject->GetPortalTracker()->RequestRescanNextUpdate();
				pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));

				pIsolatedObject = pObject;
            }
        }
    }
    else
    {
		if (!data.m_hasGameObject)
		{
			SetOwnershipToken(data.m_ownershipToken);
			m_LastReceivedMatrix = data.m_objectMatrix;
		}

		m_HasExploded	 = data.m_HasExploded;
		m_AmbientFence	 = data.m_IsAmbientFence;
		m_KeepRegistered = data.m_KeepRegistered;
		m_DestroyFrags	 = data.m_DestroyFrags;

		if (data.m_pRandomObject && gnetVerifyf(!data.m_pRandomObject->GetNetworkObject(),"%s. Invalid m_pRandomObject? data.m_pRandomObject 0x%p, data.m_pRandomObject->GetNetworkObject() 0x%p",GetLogName(),data.m_pRandomObject,data.m_pRandomObject->GetNetworkObject() ))
		{
			SetGameObject((void*) data.m_pRandomObject);
			data.m_pRandomObject->SetNetworkObject(this);
	
			if (m_HasExploded && data.m_DestroyFrags)
			{
#if ENABLE_NETWORK_LOGGING
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "OBJECT_EXPLODED", GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Clone", "true");
				NetworkInterface::GetObjectManagerLog().WriteDataValue("No Fx", "true");
#endif
				data.m_pRandomObject->SetHasExploded(NULL);
			}

			if (!data.m_hasGameObject)
			{
				CreateNetBlender();
				UpdateBlenderState(true, true, false, false);
				GetNetBlender()->GoStraightToTarget();
			}

			pIsolatedObject = data.m_pRandomObject;
		}
	}

	if (pIsolatedObject && data.m_lodOrphanHd && pIsolatedObject->GetLodData().IsOrphanHd())
	{
		if (pIsolatedObject->GetLodData().GetLodDistance() != data.m_lodDistance)
		{
			pIsolatedObject->GetLodData().SetLodDistance(data.m_lodDistance);
			pIsolatedObject->RequestUpdateInWorld();
		}
	}
}

// sync parser getter functions
void CNetObjObject::GetObjectSectorPosData(CObjectSectorPosNode& data) const
{
    data.m_UseHighPrecision = IsScriptObject();
    GetSectorPosition(data.m_SectorPosX, data.m_SectorPosY, data.m_SectorPosZ);
}

void CNetObjObject::GetObjectGameStateData(CObjectGameStateDataNode& data) const
{
    CObject* pObject = GetCObject();
	VALIDATE_GAME_OBJECT(pObject);

	data.m_visible		 = const_cast<CNetObjObject*>(this)->IsLocalEntityVisibleOverNetwork();

	GetObjectAITask(data.m_taskType, data.m_taskDataSize, data.m_taskSpecificData);
	
	//Assign whether the object has added physics.
	data.m_hasAddedPhysics = pObject ? pObject->m_nObjectFlags.bHasAddedPhysics : false;

	data.m_brokenFlags = m_brokenFlags;
	data.m_groupFlags = m_groupFlags;

	// write whether the object has exploded
	data.m_objectHasExploded     = m_HasExploded;
    data.m_HasBeenPickedUpByHook = m_HasBeenPickedUpByHook;
	
	data.m_popTires = pObject ? pObject->m_nObjectFlags.bPopTires : false;
}

void CNetObjObject::GetObjectScriptGameStateData(CObjectScriptGameStateDataNode& data) const
{
    CObject* pObject = GetCObject();
    VALIDATE_GAME_OBJECT(pObject);

	data.m_TranslationDamping.Zero();

	if (pObject)
	{
        data.m_OwnedBy							= pObject->GetOwnedBy();
        data.m_IsStealable						= pObject->m_nObjectFlags.bIsStealable;
        data.m_ActivatePhysicsAsSoonAsUnfrozen	= pObject->m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen;
        data.m_UseHighPrecisionBlending			= m_UseHighPrecisionBlending;
        data.m_UsingScriptedPhysicsParams		= m_UsingScriptedPhysicsParams;
		data.m_BreakingDisabled					= m_DisableBreaking;
		data.m_DamageDisabled					= m_DisableDamage;
		data.m_TintIndex						= pObject->GetTintIndex();
		data.m_objSpeedBoost					= pObject->GetSpeedBoost();
		data.m_IgnoreLightSettings				= (bool)pObject->m_nFlags.bLightsIgnoreDayNightSetting;
		data.m_CanBeTargeted					= pObject->CanBeTargetted();
        if (m_bDriveSyncPending)
        {
            data.m_jointToDriveIndex = m_SyncedJointToDriveIndex;
            data.m_bDriveToMaxAngle = m_bSyncedDriveToMaxAngle;
            data.m_bDriveToMinAngle = m_bSyncedDriveToMinAngle;
        }
        else
        {
            data.m_jointToDriveIndex = pObject->GetJointToDriveIndex();
            data.m_bDriveToMaxAngle = pObject->IsDriveToMaxAngleEnabled();
            data.m_bDriveToMinAngle = pObject->IsDriveToMinAngleEnabled();
        }
		data.m_bWeaponImpactsApplyGreaterForce	= pObject->GetWeaponImpactsApplyGreaterForce();
        data.m_bIsArticulatedProp               = pObject->IsObjectArticulated();
        data.m_bIsArenaBall                     = m_bIsArenaBall;
		data.m_bObjectDamaged                   = pObject->IsObjectDamaged();

        if (pObject->GetCurrentPhysicsInst())
        {
            data.m_bNoGravity = pObject->GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NO_GRAVITY) != 0;
        }
        else
        {
            data.m_bNoGravity = false;
        }

        const CEntity* pDamageInflictor = pObject->GetDamageInflictor();
        if (pDamageInflictor)
        {
            netObject* pDamageInflictorNetObject = NetworkUtils::GetNetworkObjectFromEntity(pDamageInflictor);
            if (pDamageInflictorNetObject)
            {
                data.m_DamageInflictorId = pDamageInflictorNetObject->GetObjectID();
            }
            else
            {
                data.m_DamageInflictorId = NETWORK_INVALID_OBJECT_ID;
            }
        }
        else
        {
            data.m_DamageInflictorId = NETWORK_INVALID_OBJECT_ID;
        }

        if(data.m_UsingScriptedPhysicsParams)
        {
            phArchetypeDamp *gtaArchetype = pObject->GetCurrentPhysicsInst() ? static_cast<phArchetypeDamp*>(pObject->GetCurrentPhysicsInst()->GetArchetype()) : 0;

            if(gtaArchetype)
            {
                data.m_TranslationDamping.x = gtaArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).x;
                data.m_TranslationDamping.y = gtaArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).x;
                data.m_TranslationDamping.z = gtaArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).x;
            }
        }

		data.m_bObjectFragBroken = false;
		bool allowSyncOfBrokenApart = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_SYNC_OF_BROKEN_APART", 0xDEA7A643), true);
		if(allowSyncOfBrokenApart && pObject->GetModelNameHash() == ATSTRINGHASH("prop_secdoor_01", 0x3457AE6D))
		{
			if(pObject->GetFragInst())
			{
				data.m_bObjectFragBroken = pObject->GetFragInst()->GetBroken();
			}
		}
    }
	else
	{
		data.m_OwnedBy							= ENTITY_OWNEDBY_RANDOM;
		data.m_IsStealable						= false;
		data.m_ActivatePhysicsAsSoonAsUnfrozen	= false;
		data.m_UseHighPrecisionBlending			= false;
		data.m_UsingScriptedPhysicsParams		= false;
		data.m_TintIndex						= 0;
		data.m_objSpeedBoost				    = 0;
		data.m_IgnoreLightSettings				= false;
		data.m_CanBeTargeted                    = false;
		data.m_bDriveToMaxAngle					= false;
		data.m_bDriveToMinAngle					= false;
        data.m_bIsArticulatedProp               = false;
        data.m_bIsArenaBall                     = false;
		data.m_bObjectDamaged                   = false;
        data.m_DamageInflictorId                = NETWORK_INVALID_OBJECT_ID;
		data.m_bObjectFragBroken				= false;
		data.m_BreakingDisabled					= false;
		data.m_DamageDisabled					= false;
		data.m_bWeaponImpactsApplyGreaterForce	= false;
		data.m_bNoGravity						= false;
	}

	data.m_ScopeDistance = m_ScriptAdjustedScopeDistance;
}

// sync parser setter functions
void CNetObjObject::SetObjectSectorPosData(CObjectSectorPosNode& data)
{
    SetSectorPosition(data.m_SectorPosX, data.m_SectorPosY, data.m_SectorPosZ);
}

void CNetObjObject::SetObjectOrientationData(CObjectOrientationNode& data)
{
    data.m_bUseHighPrecision = m_bUseHighPrecisionRotation;
    m_LastReceivedMatrix.Set3x3(data.m_orientation);
}

void CNetObjObject::GetObjectOrientationData(CObjectOrientationNode& data) const
{
    CObject *pObject = GetCObject();
    VALIDATE_GAME_OBJECT(pObject);
    if (pObject)
    {
        data.m_orientation = MAT34V_TO_MATRIX34(pObject->GetMatrix());
    }
    else
    {
        data.m_orientation.Identity();
    }
    data.m_bUseHighPrecision = m_bUseHighPrecisionRotation;
}

void CNetObjObject::SetObjectGameStateData(const CObjectGameStateDataNode& data)
{
    CObject* pObject = GetCObject();
    VALIDATE_GAME_OBJECT(pObject);

	SetObjectAITask(data.m_taskType, data.m_taskDataSize, data.m_taskSpecificData);
	
	//Sync the physics state.
	if (pObject)
	{
		if(data.m_hasAddedPhysics && !pObject->m_nObjectFlags.bHasAddedPhysics)
		{
			//Check if physics have been initialized.
			if(!pObject->m_nObjectFlags.bHasInitPhysics)
			{
				//Initialize the physics.
				pObject->InitPhys();
			}
			
			//Add the physics.
			pObject->AddPhysics();
		}
		else if(!data.m_hasAddedPhysics && pObject->m_nObjectFlags.bHasAddedPhysics)
		{
			//Remove the physics.
			pObject->RemovePhysics();
		}

		m_brokenFlags = data.m_brokenFlags;
		m_groupFlags = data.m_groupFlags;
		m_HasExploded = data.m_objectHasExploded;

		if (m_HasExploded && !pObject->m_nFlags.bHasExploded)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "OBJECT_EXPLODED", GetLogName());
			NetworkInterface::GetObjectManagerLog().WriteDataValue("Clone", "true");

			if (pObject->m_nFlags.bHasExploded)
			{
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Already exploded", "true");
			}
			else if (pObject->TryToExplode(pObject->GetWeaponDamageEntity(), false))
			{
				m_lastTimeBroken = fwTimer::GetSystemTimeInMilliseconds();
			}
			else
			{
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Failed", "true");
			}
		}

		pObject->m_nObjectFlags.bPopTires = data.m_popTires;
		
		// Set the visibility state...
		SetIsVisible(data.m_visible, "SetObjectGameStateData", true);

		if (!m_HasBeenPickedUpByHook && data.m_HasBeenPickedUpByHook && pObject->GetIplIndex() > 0)
		{
			pObject->DecoupleFromMap();
		}
	}

    m_HasBeenPickedUpByHook = data.m_HasBeenPickedUpByHook;
}

void CNetObjObject::SetObjectScriptGameStateData(const CObjectScriptGameStateDataNode& data)
{
    CObject* pObject = GetCObject();
    VALIDATE_GAME_OBJECT(pObject);

    if(pObject)
    {
        pObject->SetOwnedBy( (eEntityOwnedBy) data.m_OwnedBy );

		// must set corresponding pop type too
		if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
		{
			pObject->PopTypeSet(POPTYPE_MISSION);
		}
		else
		{
			pObject->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
		}

        pObject->m_nObjectFlags.bIsStealable                     = data.m_IsStealable;
        pObject->m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = data.m_ActivatePhysicsAsSoonAsUnfrozen;
        m_UseHighPrecisionBlending                               = data.m_UseHighPrecisionBlending;
        m_UsingScriptedPhysicsParams                             = data.m_UsingScriptedPhysicsParams;
		m_DisableBreaking										 = data.m_BreakingDisabled;
		m_DisableDamage											 = data.m_DamageDisabled;
        m_bIsArenaBall                                           = data.m_bIsArenaBall;
        m_BlenderModeDirty                                       = true;
        m_SyncedJointToDriveIndex                                = data.m_jointToDriveIndex;
        m_bSyncedDriveToMinAngle                                 = data.m_bDriveToMinAngle;
        m_bSyncedDriveToMaxAngle                                 = data.m_bDriveToMaxAngle;
		m_bObjectDamaged                                         = data.m_bObjectDamaged;

        if (m_SyncedJointToDriveIndex != pObject->GetJointToDriveIndex()
            || m_bSyncedDriveToMinAngle != pObject->IsDriveToMinAngleEnabled()
            || m_bSyncedDriveToMaxAngle != pObject->IsDriveToMaxAngleEnabled())
        {
            m_bDriveSyncPending = true;
        }

        CEntity* pDamageInflictor = nullptr;
        netObject* pNetObjDamageInflictor = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_DamageInflictorId);
        if (pNetObjDamageInflictor)
        {
            pDamageInflictor = pNetObjDamageInflictor->GetEntity();
        }
        pObject->SetDamageInflictor(pDamageInflictor);
        
		pObject->SetSpeedBoost(data.m_objSpeedBoost);

        if(m_UsingScriptedPhysicsParams)
        {
            phArchetypeDamp *gtaArchetype = pObject->GetCurrentPhysicsInst() ? static_cast<phArchetypeDamp*>(pObject->GetCurrentPhysicsInst()->GetArchetype()) : 0;

            if(gtaArchetype)
            {
                gtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C,  Vector3(data.m_TranslationDamping.x, data.m_TranslationDamping.x, data.m_TranslationDamping.x));
                gtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V,  Vector3(data.m_TranslationDamping.y, data.m_TranslationDamping.y, data.m_TranslationDamping.y));
                gtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(data.m_TranslationDamping.z, data.m_TranslationDamping.z, data.m_TranslationDamping.z));
            }
        }

		m_ScriptAdjustedScopeDistance = (u16)data.m_ScopeDistance;

		SetTintIndexOnObjectHelper((u8)data.m_TintIndex); // Setting tint index of object

		if(data.m_IgnoreLightSettings != (bool)pObject->m_nFlags.bLightsIgnoreDayNightSetting)
		{
			if (data.m_IgnoreLightSettings)
			{
				CLODLightManager::RegisterBrokenLightsForEntity(pObject);
			}
			else
			{
				CLODLightManager::UnregisterBrokenLightsForEntity(pObject);
			}
		}
		pObject->m_nFlags.bLightsIgnoreDayNightSetting = data.m_IgnoreLightSettings;
		pObject->SetObjectTargettable(data.m_CanBeTargeted);
		pObject->SetWeaponImpactsApplyGreaterForce(data.m_bWeaponImpactsApplyGreaterForce);
        pObject->SetObjectIsArticulated(data.m_bIsArticulatedProp);

        if (pObject->GetCurrentPhysicsInst())
        {
            pObject->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, data.m_bNoGravity);
        }

		bool allowSyncOfBrokenApart = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_SYNC_OF_BROKEN_APART", 0xDEA7A643), true);
		if(allowSyncOfBrokenApart && pObject->GetModelNameHash() == ATSTRINGHASH("prop_secdoor_01", 0x3457AE6D))
		{
			if(pObject->GetFragInst())
			{
				pObject->GetFragInst()->SetBroken(data.m_bObjectFragBroken);
			}
		}
    }
}

// sync parser ai task functions
void CNetObjObject::GetObjectAITask(u32 &taskType, u32& taskDataSize, u8* taskSpecificData) const
{
	//--

	// Use max rather than TASK_INVALID_ID to avoid the minus...
	taskType		= CTaskTypes::MAX_NUM_TASK_TYPES;
	taskDataSize	= 0;

	//--

	CObject* pObject = GetCObject();
    VALIDATE_GAME_OBJECT(pObject);

	if (pObject)
	{
		CObjectIntelligence* objIntel = pObject->GetObjectIntelligence();

		if(objIntel)
		{
			CTask* pTask = static_cast<CTask*>(objIntel->GetTaskManager()->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM));

			if (pTask)
			{
				// TEMP...
				if((CTaskTypes::TASK_SCRIPTED_ANIMATION == pTask->GetTaskType()) || (CTaskTypes::TASK_PARACHUTE_OBJECT == pTask->GetTaskType()))
				{
					taskType = pTask->GetTaskType();

					if(pTask->IsClonedFSMTask())
					{
						CTaskInfo* taskInfo = pTask->CreateQueriableState();

						datBitBuffer messageBuffer; 
						messageBuffer.SetReadWriteBits(taskSpecificData, CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE, 0);
						taskInfo->WriteSpecificTaskInfo(messageBuffer);

						taskDataSize = messageBuffer.GetCursorPos();

						delete taskInfo;
						taskInfo = NULL;
					}
				}
			}
		}
	}
}

void CNetObjObject::SetObjectAITask(u32 const taskType, u32 const taskDataSize, u8 const* taskSpecificData)
{
	CObject* pObject = GetCObject();
    VALIDATE_GAME_OBJECT(pObject);

	// we have a task to set up or update....
	if (pObject)
	{
		if(taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
		{
			CObjectIntelligence* objectIntelligence = pObject->GetObjectIntelligence();

			//-------

			// TEMP - only supports clone task and only CTaskRunNamedClip is for use on objects atm.
			gnetAssert((CTaskTypes::TASK_PARACHUTE_OBJECT == taskType) || (CTaskTypes::TASK_SCRIPTED_ANIMATION == taskType) || (CTaskTypes::MAX_NUM_TASK_TYPES == taskType));

			if((CTaskTypes::TASK_PARACHUTE_OBJECT != taskType) && (CTaskTypes::TASK_SCRIPTED_ANIMATION != taskType) && (CTaskTypes::MAX_NUM_TASK_TYPES != taskType))
			{
				return;
			}

			bool important = false;
			if(!CQueriableInterface::IsTaskSentOverNetwork(taskType, &important))
			{
				gnetAssertf(false, "CNetObjObject::SetObjectAITask : Task Is Not Sent Over Network!");
			}

			CClonedFSMTaskInfo* taskInfo = static_cast<CClonedFSMTaskInfo*>(CQueriableInterface::CreateEmptyTaskInfo(taskType, true));
			gnetAssert(taskInfo);

			// if we've got some data, read it in....
			if( taskDataSize>0 )
			{
				u32 byteSizeOfTaskData = (taskDataSize+7)>>3;
				datBitBuffer messageBuffer;
				messageBuffer.SetReadOnlyBits(taskSpecificData, byteSizeOfTaskData<<3, 0);
			
				if(taskInfo)
				{
					taskInfo->ReadSpecificTaskInfo(messageBuffer);
				}
			}

			CTaskFSMClone* currentTask = NULL;
			if(objectIntelligence)
			{
				currentTask = SafeCast(CTaskFSMClone, objectIntelligence->GetTaskManager()->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM));
			}
		
			if( (!currentTask) || (currentTask && currentTask->GetTaskType() != (s32)taskType) )
			{
				// if the task isn't a clone task this will fail but atm the cloned 
				// CTaskRunNamedClip is the only task for objects
				gnetAssert(taskInfo);
				if(taskInfo)
				{
					// a task can return NULL if it's not set up properly....
					currentTask = taskInfo->CreateCloneFSMTask();
					if(currentTask)
					{
						if(!pObject->SetTask(currentTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM))
						{
							currentTask = NULL;
						}
					}
				}
			}

			if( taskInfo )
			{
				if( currentTask )
					currentTask->ReadQueriableState(taskInfo);

				delete taskInfo;
				taskInfo = NULL;
			}
		}
		else
		{
			bool allowObjTaskCleanup = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_OBJECT_TASK_CLEANUP_ON_CLONE", 0xD97720D9), true);
			if(allowObjTaskCleanup)
			{
				CObjectIntelligence* objIntel = pObject->GetObjectIntelligence();
				if(objIntel)
				{
					CTask* pTask = static_cast<CTask*>(objIntel->GetTaskManager()->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM));
					if (pTask)
					{
						pTask->CleanUp();
					}
				}
			}
		}
	}
}

bool CNetObjObject::UpdateUnassignedObject()
{
	// we are controlling a network object for an ambient object that has been removed - try to pass control and delete it if not
	if (!IsClone() && !IsPendingOwnerChange())
	{
		bool bFlaggedForDeletion = IsFlaggedForDeletion();

		if (!m_UnassignedAmbientObject && !bFlaggedForDeletion && HasSpaceForUnassignedAmbientObject())
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

			// add or remove the object from the unassigned list depending on whether it is still in scope of the local player or not
			if (pLocalPlayer)
			{
				Vector3 diff = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()) - m_DummyPosition;

				if (diff.Mag2() < ms_objectScopeData.m_scopeDistance*ms_objectScopeData.m_scopeDistance)
				{
					// first search object pool to make sure that the object doesn't already exist
					CObject* pFoundObject = FindAssociatedAmbientObject(m_DummyPosition, m_FragGroupIndex);

					if (pFoundObject)
					{
						if (AssertVerify(!pFoundObject->GetNetworkObject()))
						{
							pFoundObject->SetNetworkObject(this);
							SetGameObject(pFoundObject);
						}
					}
					else
					{
						AddUnassignedAmbientObject(*this, "UpdateUnassignedObject - local object in scope");
					}
				}
			}
		}

		if (!HasGameObject())
		{
			if (bFlaggedForDeletion)
			{
				return true;
			}

			// unregister the network object only once the player has moved away from the prop creation range of the dummy. Felled props can be removed
			// quite close to the player when offscreen and the network object for these still needs to persist for other players coming into the area so
			// that the prop is still knocked over for them
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

			if (pLocalPlayer)
			{
				Vector3 diff = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()) - m_DummyPosition;
				float dist = diff.Mag2();

				if (dist > OBJECT_POPULATION_RESET_RANGE*OBJECT_POPULATION_RESET_RANGE && TryToPassControlOutOfScope())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CNetObjObject::IsBroken() const
{
	CObject* pObject = GetCObject();

	bool bBroken = false;

	if (pObject && m_FragGroupIndex == -1)
	{
		fragInst* pFragInst = pObject ? pObject->GetFragInst() : NULL;

		if (pFragInst && pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->AreAnySet())
		{
			bBroken = true;
		}
	}

	return bBroken;
}

bool CNetObjObject::IsAmbientFence() const
{
	if (GetObjectType() != NET_OBJ_TYPE_DOOR)
	{
		static const float THIN_BOUND = 0.3f;

		CObject* pObject = GetCObject();
		
		// fences are a special case in that we want to uproot them but not sync them. They don't sync very well and can end up thrashing around. They
		// only need to be uprooted so that other players can run through them

		if (gnetVerify(pObject) && pObject->GetFragInst() && !IsFragmentObjectType(pObject->GetOwnedBy()) && pObject->m_nObjectFlags.bHasBeenUprooted)
		{
			bool thinBoundX = (pObject->GetBoundingBoxMax().x - pObject->GetBoundingBoxMin().x) < THIN_BOUND;
			bool thinBoundY = (pObject->GetBoundingBoxMax().y - pObject->GetBoundingBoxMin().y) < THIN_BOUND;
			bool thinBoundZ = (pObject->GetBoundingBoxMax().z - pObject->GetBoundingBoxMin().z) < THIN_BOUND;
		
			if (!thinBoundZ && ((thinBoundX && !thinBoundY) || (!thinBoundX && thinBoundY)))
			{
				return true;
			}
		}
	}

	return false;
}

void CNetObjObject::TryToBreakObject()
{
	CObject* pObject = GetCObject();

	fragInst* pFragInst = pObject ? pObject->GetFragInst() : NULL;

	if (pFragInst && (m_brokenFlags != 0 || m_HasExploded || m_groupFlags.m_Flags.AreAnySet()))
	{
		fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

		if (!pFragCacheEntry)
		{
			pFragInst->PutIntoCache();
			pFragCacheEntry = pFragInst->GetCacheEntry();
		}

		if (pFragCacheEntry)
		{
			if (m_brokenFlags != 0 &&
				AssertVerify(m_FragGroupIndex <= 0) &&
				pFragInst->GetTypePhysics()->GetNumChildGroups() > 1)
			{
				//When applying the broken bit information start from the highest bit number
				//and decrement down to ensure the hierarchy breaks fully and completely
				for (s32 bit = pFragCacheEntry->GetHierInst()->groupBroken->GetNumBits()-1; bit >= 0; bit--)
				{
					if (m_brokenFlags & (1 << bit))
					{
						if (!pFragCacheEntry->GetHierInst()->groupBroken->IsSet(bit))
						{
							if (m_DestroyFrags)
							{
								pFragInst->DeleteAboveGroup(bit);

								// disable any light effects
								pObject->m_nFlags.bRenderDamaged = true;
								CLODLightManager::RegisterBrokenLightsForEntity(pObject);
							}
							else
							{
								// temporarily flag the object as unexploded so any frags that generate explosions do so 
								bool bHasExploded = pObject->m_nFlags.bHasExploded;
								pObject->m_nFlags.bHasExploded = false;
								pObject->m_nFlags.bRenderDamaged = true;
								pFragInst->BreakOffAboveGroup(bit);

								pObject->m_nFlags.bHasExploded = bHasExploded;
							}
						}
					}
				}

				// allow any further breaks to generate frags
				m_DestroyFrags = false;
			}

			if (m_groupFlags.m_Flags.AreAnySet())
			{
				for (s32 bit = 0; bit < m_groupFlags.m_NumBitsUsed; bit++)
				{
					if (m_groupFlags.m_Flags.IsSet(bit) && gnetVerifyf(bit < pFragInst->GetTypePhysics()->GetNumChildGroups(), "%s is trying to reduce health for group: %d that but object only has %d groups", GetLogName(), bit, pFragInst->GetTypePhysics()->GetNumChildGroups()))
					{
						pFragInst->ReduceGroupDamageHealth(bit, -1000.0f, Vec3V(V_ZERO), Vec3V(V_ZERO) FRAGMENT_EVENT_PARAM(true));
					}
				}
			}

			if (m_HasExploded && !pFragCacheEntry->GetHierInst()->anyGroupDamaged)
			{
				pFragInst->DamageAllGroups();
			}
		}
	}
}

void CNetObjObject::SetTintIndexOnObjectHelper(u8 tintIndex)
{
	CObject* pObject = GetCObject();
	VALIDATE_GAME_OBJECT(pObject);

	if(pObject)
	{
		pObject->SetTintIndex(tintIndex);
		CCustomShaderEffectTint *pTintCSE = pObject->GetCseTintFromObject();
		if(pTintCSE)
		{
			pTintCSE->SelectTintPalette((u8)tintIndex, pObject);
		} 

		// Handle the case where this object is a weapon, in which case we need to apply the tint differently. 
		fwDrawData *drawHandler = pObject->GetDrawHandlerPtr();
		if(drawHandler)
		{
			fwCustomShaderEffect* pShader = drawHandler->GetShaderEffect();
			if(pShader)
			{
				u32 type = pShader->GetType();			
				if (type == CSE_WEAPON)
				{	
					CCustomShaderEffectWeapon* pWeaponShader = static_cast<CCustomShaderEffectWeapon*>(pShader);
					if (pWeaponShader)
					{
						pWeaponShader->SelectTintPalette(tintIndex, pObject);					
						pWeaponShader->SetDiffusePalette(tintIndex);	
					}												
				}
			}
		}
	}
}

void CNetObjObject::LogTaskData(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits)
{
	if(log)
	{
		CClonedFSMTaskInfo* taskInfo = static_cast<CClonedFSMTaskInfo*>(CQueriableInterface::CreateEmptyTaskInfo(taskType, true));
		gnetAssert(taskInfo);
		
		if(taskInfo)
		{
			u32 byteSizeOfTaskData = (numBits+7)>>3;
			datBitBuffer messageBuffer;
			messageBuffer.SetReadOnlyBits(dataBlock, byteSizeOfTaskData<<3, 0);
			taskInfo->ReadSpecificTaskInfo(messageBuffer);

			taskInfo->LogSpecificTaskInfo(*log);

			delete taskInfo;
			taskInfo = NULL;
		}
	}
}

// name:        RegisterKillWithNetworkTracker
// description: Register a kill with the NetworkDamageTracker...
// Parameters:  none
void CNetObjObject::RegisterKillWithNetworkTracker(void)
{
    CObject *pObject = GetCObject();

    // check if this object has been destroyed and has not been registered with the kill tracking code yet
    if(pObject && pObject->GetHealth() <= 0.0f)
    {
        if(!m_registeredWithKillTracker)
        {
			CNetGamePlayer *player = NetworkUtils::GetPlayerFromDamageEntity(pObject, pObject->GetWeaponDamageEntity());

            if(player && player->IsMyPlayer() && pObject->GetWeaponDamageHash() != 0)
            {
				CNetworkKillTracker::RegisterKill(pObject->GetModelIndex(), pObject->GetWeaponDamageHash(), !pObject->PopTypeIsMission());

                m_registeredWithKillTracker = true;
            }
        }
    }
}

void CNetObjObject::ForceSendOfScriptGameStateData()
{
    CObjectSyncTree* pObjectSyncTree = SafeCast(CObjectSyncTree, GetSyncTree());
    pObjectSyncTree->ForceSendOfScriptGameStateData(*this, GetActivationFlags());
}

#if __BANK

bool CNetObjObject::ShouldDisplayAdditionalDebugInfo()
{
    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    if (displaySettings.m_displayTargets.m_displayObjects && GetCObject())
	{
		return true;
	}

	return CNetObjPhysical::ShouldDisplayAdditionalDebugInfo();
}

void CNetObjObject::DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const
{
    if(m_UseHighPrecisionBlending)
    {
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "High Precision Mode"));
        screenCoords.y += debugYIncrement;
    }
	else if(m_UseParachuteBlending)
	{
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "Parachute Mode"));
        screenCoords.y += debugYIncrement;
	}

    CNetObjPhysical::DisplayBlenderInfo(screenCoords, playerColour, scale, debugYIncrement);
}

#endif // __BANK

void CNetObjObject::AssignAmbientObject(CObject& object, bool bBreakIfNecessary)
{
#if ENABLE_NETWORK_LOGGING
	netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
#endif // ENABLE_NETWORK_LOGGING

	if (!AssertVerify(!object.IsADoor() && !object.m_nObjectFlags.bIsPickUp))
	{
		return;
	}

	// the object can already have a network object if networked when being broken
	if (object.GetNetworkObject())
	{
		return;
	}

	if (ms_assigningAmbientObject)
	{
		// if we allow more than one networked frag per object, AssignAmbientObject will need to get moved out of CObject::AddPhysics(), as it will get called again
		// on each frag when they are broken off and CObject::AddPhysics() is called on them
		gnetAssert(!ALLOW_MULTIPLE_NETWORKED_FRAGS_PER_OBJECT);
		return;
	}

	ms_assigningAmbientObject = true;

	CDummyObject* pDummyObj = object.GetRelatedDummy();

	if (!pDummyObj && object.GetFragParent() && object.GetFragParent()->GetIsTypeObject())
	{
		pDummyObj = SafeCast(CObject, object.GetFragParent())->GetRelatedDummy();
	}

	if (pDummyObj)
	{
		Vector3 dummyPos = VEC3V_TO_VECTOR3(pDummyObj->GetTransform().GetPosition());

		CObject* newFrags[MAX_UNASSIGNED_AMBIENT_OBJECTS];
		u32 numNewFrags = 0;

		for (u32 i=0; i<ms_numUnassignedAmbientObjects; i++)
		{
			CNetObjObject* pUnassignedNetObject = ms_unassignedAmbientObjects[i];

			if (AssertVerify(pUnassignedNetObject))
			{
				if (IsSameDummyPos(pUnassignedNetObject->m_DummyPosition, dummyPos))
				{
					CObject* pFoundObject = NULL;

					if (pUnassignedNetObject->m_FragGroupIndex != -1)
					{
						if (object.GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
						{
						if (object.GetFragInst() && 
							object.GetFragInst()->GetCacheEntry() &&
							object.GetFragInst()->GetTypePhysics() &&
							pUnassignedNetObject->m_FragGroupIndex < object.GetFragInst()->GetTypePhysics()->GetNumChildGroups() &&
							object.GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(pUnassignedNetObject->m_FragGroupIndex))
						{
								pFoundObject = &object;
								break;
							}
							else if (bBreakIfNecessary)
							{
								pFoundObject = GetFragObjectFromParent(object, pUnassignedNetObject->m_FragGroupIndex, true);
							}
						}
						else if (object.GetFragInst() && bBreakIfNecessary)
						{
							pFoundObject = GetFragObjectFromParent(object, pUnassignedNetObject->m_FragGroupIndex, true);
						}
					}
					else if (object.GetOwnedBy() == ENTITY_OWNEDBY_RANDOM)
					{
						pFoundObject = &object;
					}

					if (pFoundObject)
					{
						LOGGING_ONLY(NetworkLogUtils::WriteLogEvent(log, "ASSIGNING_AMBIENT_OBJECT", pUnassignedNetObject->GetLogName()));

						if (gnetVerifyf(!pFoundObject->GetNetworkObject(), "Trying to assign %s to object which already has a network object (%s)", pUnassignedNetObject->GetLogName(), pFoundObject->GetNetworkObject()->GetLogName()) &&
							gnetVerifyf(!pUnassignedNetObject->HasGameObject(), "Trying to assign %s to an object when it already has a game object", pUnassignedNetObject->GetLogName()))
						{
							pUnassignedNetObject->SetGameObject(pFoundObject);
							pFoundObject->SetNetworkObject(pUnassignedNetObject);

							if (!pUnassignedNetObject->GetNetBlender())
							{
								pUnassignedNetObject->CreateNetBlender();
							}

							LOGGING_ONLY(log.WriteDataValue("Object", "0x%x", pFoundObject));
							LOGGING_ONLY(log.WriteDataValue("Position", "%f, %f, %f", pUnassignedNetObject->m_LastReceivedMatrix.d.x, pUnassignedNetObject->m_LastReceivedMatrix.d.y, pUnassignedNetObject->m_LastReceivedMatrix.d.z));

							pUnassignedNetObject->UpdateBlenderState(true, true, false, false);

							if (pUnassignedNetObject->GetNetBlender())
							{
								pUnassignedNetObject->GetNetBlender()->GoStraightToTarget();
							}

							if (pUnassignedNetObject->m_HasExploded)
							{
#if ENABLE_NETWORK_LOGGING
								NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "OBJECT_EXPLODED", pUnassignedNetObject->GetLogName());
								NetworkInterface::GetObjectManagerLog().WriteDataValue("Clone", "%s", pUnassignedNetObject->IsClone() ? "true" : "false");
								NetworkInterface::GetObjectManagerLog().WriteDataValue("No Fx", "true");
#endif
								pFoundObject->SetHasExploded(NULL);
							}

							// if the object is to be broken, flag it to just remove all frags otherwise we get a performance hit 
							pUnassignedNetObject->m_DestroyFrags = true;

							pUnassignedNetObject->TryToBreakObject();

							RemoveUnassignedAmbientObject(*pUnassignedNetObject, "Object assigned", false, i);

							if (bBreakIfNecessary && pFoundObject != &object && ALLOW_MULTIPLE_NETWORKED_FRAGS_PER_OBJECT)
							{
								// continue searching the array as there may be other frags that need to break off the original object
								newFrags[numNewFrags++] = pFoundObject;
							}
							else
							{
								break;
							}
						}
					}
				}
			}			
		}

		if (ALLOW_MULTIPLE_NETWORKED_FRAGS_PER_OBJECT)
		{
			// we need to see if any of the frags can be assigned to a network object. This can happen if we get a number of networked frags from one object.
			for (u32 i=0; i<numNewFrags; i++)
			{
				AssignAmbientObject(*newFrags[i], false);
			}
		}
	}

	ms_assigningAmbientObject = false;
}

// called when an object is being removed locally
bool CNetObjObject::UnassignAmbientObject(CObject& object)
{
	if (AssertVerify(object.GetNetworkObject()) && AssertVerify(!object.IsADoor()) && AssertVerify(!object.m_nObjectFlags.bIsPickUp))
	{
		CNetObjObject* pNetObj = SafeCast(CNetObjObject, object.GetNetworkObject());

		if (!pNetObj->m_bDontAddToUnassignedList)
		{
			// copy the current matrix from the object so that we can restore it to this position if it is assigned again
			pNetObj->m_LastReceivedMatrix = MAT34V_TO_MATRIX34(object.GetTransform().GetMatrix());

			if(!object.m_nObjectFlags.bIsNetworkedFragment)
			{
				if (AddUnassignedAmbientObject(*pNetObj, "Local object removal"))
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

CObject* CNetObjObject::FindAssociatedAmbientObject(const Vector3& dummyPos, int fragGroupIndex, bool bOnlyIncludeNetworkedObjects)
{
	CObject* pFoundObject = NULL;
	CObject* pFoundParent = NULL;

	CObject::Pool *ObjectPool = CObject::GetPool();

	int i = (int) ObjectPool->GetSize();

	while(i--)
	{
		CObject* pObject = ObjectPool->GetSlot(i);

		if (pObject && IsAmbientObjectType(pObject->GetOwnedBy()) && (!bOnlyIncludeNetworkedObjects || pObject->GetNetworkObject()))
		{
			CDummyObject* pDummyObj = pObject->GetRelatedDummy();

			if (!pDummyObj && pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
			{
				pDummyObj = SafeCast(CObject, pObject->GetFragParent())->GetRelatedDummy();
			}

			if (pDummyObj)
			{
				Vector3 thisDummyPos = VEC3V_TO_VECTOR3(pDummyObj->GetTransform().GetPosition());
				float dist;

				if (CNetObjObject::IsSameDummyPos(thisDummyPos, dummyPos, &dist))
				{
					if (fragGroupIndex != -1)
					{
						if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
						{
							if (pObject->GetFragInst() && fragGroupIndex < pObject->GetFragInst()->GetTypePhysics()->GetNumChildGroups())
							{
								if (pObject->GetFragInst()->GetCacheEntry() &&
									fragGroupIndex < pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->GetNumBits() &&
									pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(fragGroupIndex))
								{
									pFoundObject = pObject;
									break;
								}
							}
						}
						else if (pObject->GetFragInst())
						{
							pFoundParent = pObject;
						}
					}
					else 
					{
						if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM)
						{
							pFoundObject = pObject;
							break;
						}
					}
				}
			}
		}
	}

	if (pFoundParent && !pFoundObject)
	{
		pFoundObject = CNetObjObject::GetFragObjectFromParent(*pFoundParent, fragGroupIndex, false);
	}

	return pFoundObject;
}

CNetObjObject* CNetObjObject::FindUnassignedAmbientObject(const Vector3& dummyPos, int fragGroupIndex)
{
	for (u32 i=0; i<ms_numUnassignedAmbientObjects; i++)
	{
		CNetObjObject* pUnassignedObj = ms_unassignedAmbientObjects[i];

		if (pUnassignedObj && IsSameDummyPos(pUnassignedObj->m_DummyPosition, dummyPos) && pUnassignedObj->m_FragGroupIndex == fragGroupIndex)
		{
			return pUnassignedObj;
		}
	}

	return NULL;
}

CObject* CNetObjObject::GetFragObjectFromParent(CObject& object, u32 fragGroupIndex, bool bDeleteChildren)
{
	CObject* pObject = &object;

	if (AssertVerify(object.GetFragInst()))
	{
		if (!object.GetFragInst()->IsInLevel())
		{
			return NULL;
		}

		ms_gettingFragObjectFromParent = true;

		// break off the fragment we are trying to sync from its parent
		if (fragGroupIndex == 0)
		{
			if( object.IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) && object.GetFragInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) )
			{
				object.GetFragInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);
			}
#if __ASSERT
			bool trytoactivate = PHLEVEL->IsInactive( object.GetFragInst()->GetLevelIndex() ) && object.GetFragInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) ? true: false;
			Assertf( !trytoactivate, "inst %s with FLAG_NEVER_ACTIVATE will try to activate.  pos: %f %f %f", object.GetFragInst()->GetArchetype() ? object.GetFragInst()->GetArchetype()->GetFilename(): "NONAME", VEC3V_ARGS( object.GetMatrix().GetCol3() ) );
#endif
			object.GetFragInst()->BreakOffRoot();
			object.m_nObjectFlags.bHasBeenUprooted = true;
		}
		else 
		{
			bool bBreak = true;

			if (object.GetFragInst()->GetCacheEntry())
			{
				// check that the fragment we are after is still attached to this object
				if (object.GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(fragGroupIndex))
				{
					pObject = NULL;
					bBreak = false;
				}

				// don't break the frag anymore if we have a leaf 
				s32 numBrokenBits = object.GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->GetNumBits();

				if (bBreak && object.GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->CountOnBits() == numBrokenBits-1)
				{
					bBreak = false;
				}

				if (bBreak && fragGroupIndex >= object.GetFragInst()->GetTypePhysics()->GetNumChildGroups())
				{
#if __ASSERT
					Vector3 objectPos = VEC3V_TO_VECTOR3(object.GetTransform().GetPosition());
					gnetAssertf(0, "Trying to break off group %d on object at %f, %f, %f, which does not exist for this object", fragGroupIndex, objectPos.x, objectPos.y, objectPos.z);
#endif // __ASSERT
					bBreak = false;
				}
			}

			if (bBreak)
			{
				if (bDeleteChildren)
					fragGroupIndex = 1;

				// disable any light effects
				object.m_nFlags.bRenderDamaged = true;
				CLODLightManager::RegisterBrokenLightsForEntity(pObject);

				fragInst* pFragInst = object.GetFragInst()->BreakOffAboveGroup(fragGroupIndex);

				// if there is no frag inst, this frag is already broken off at the frag group index
				if (pFragInst)
				{
					pObject = static_cast<CObject*>(CPhysics::GetEntityFromInst(pFragInst));
				}
				else
				{
					bDeleteChildren = false;
				}

				if (bDeleteChildren && pObject && AssertVerify(pObject->GetFragInst()) && AssertVerify(pObject != &object))
				{
					fragTypeGroup *group = pObject->GetFragInst()->GetTypePhysics()->GetGroup(fragGroupIndex);
					fragCacheEntry* entry =  pObject->GetFragInst()->GetCacheEntry();
					gnetAssert(entry);
					fragHierarchyInst* hierInst = entry->GetHierInst();

					// This group is going to be disappearing so lets break off any child groups that might still be in existence.
					for(int childGroupOffset = 0; childGroupOffset < group->GetNumChildGroups(); ++childGroupOffset)
					{
						int childGroupIndex = group->GetChildGroupsPointersIndex() + childGroupOffset;
						if(hierInst->groupBroken->IsClear(childGroupIndex))
						{
							pObject->GetFragInst()->DeleteAboveGroup(childGroupIndex);
						}
					}

					// need to restore the game heap here in case the object constructor/destructor allocates from the heap
					sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
					sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

					CObjectPopulation::DestroyObject(pObject);

					sysMemAllocator::SetCurrent(*oldHeap);

					pObject = NULL;
				}
			}
		}
	}

	ms_gettingFragObjectFromParent = false;

	return pObject;
}

bool CNetObjObject::IsSameDummyPos(const Vector3& dummyPos1, const Vector3& dummyPos2, float* pDist)
{
	Vector3 diff = dummyPos1 - dummyPos2;

	float dist = diff.Mag2();

	if (pDist)
	{
		*pDist = dist;
	}

	return dist < 0.0001f;
}

void CNetObjObject::TryToRegisterAmbientObject(CObject& object, bool weWantOwnership, const char* LOGGING_ONLY(reason))
{
#if ENABLE_NETWORK_LOGGING
	netLoggingInterface& log = NetworkInterface::GetObjectManagerLog();
	NetworkLogUtils::WriteLogEvent(log, "TRY_TO_REGISTER_AMBIENT_OBJ", "0x%x", &object);
	Vector3 objectPos = VEC3V_TO_VECTOR3(object.GetTransform().GetPosition());
	LOGGING_ONLY(log.WriteDataValue("Object pos", "%f, %f, %f", objectPos.x, objectPos.y, objectPos.z));
	log.WriteDataValue("Reason", reason);
#endif

	if (!object.GetNetworkObject() && NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_OBJECT, false))
	{
		CObject* pObjectToRegister = &object;

		if (object.GetFragParent() && object.GetFragParent()->GetIsTypeObject())
		{
			pObjectToRegister = SafeCast(CObject, object.GetFragParent());
			LOGGING_ONLY(log.WriteDataValue("Register frag parent", "0x%x", pObjectToRegister));
		}

		if (pObjectToRegister)
		{
			if (!pObjectToRegister->GetRelatedDummy())
			{
				LOGGING_ONLY(log.WriteDataValue("FAIL", "No related dummy"));
				return;
			}

			Vector3 dummyPos = VEC3V_TO_VECTOR3(pObjectToRegister->GetRelatedDummy()->GetTransform().GetPosition());

			LOGGING_ONLY(log.WriteDataValue("Dummy pos", "%f, %f, %f", dummyPos.x, dummyPos.y, dummyPos.z));

			if (pObjectToRegister->GetNetworkObject())
			{
				LOGGING_ONLY(log.WriteDataValue("FAIL", "Object already registered (%s)", pObjectToRegister->GetNetworkObject()->GetLogName()));
				pObjectToRegister = NULL;
			}
			else
			{
				// make sure there are no other networked objects which share this dummy position
				CNetObjObject::AssignAmbientObject(*pObjectToRegister, false);

				if (pObjectToRegister->GetNetworkObject())
				{
					pObjectToRegister = NULL;
				}
				else
				{
					CObject* pFoundObj = CNetObjObject::FindAssociatedAmbientObject(dummyPos, -1, true);

					if (pFoundObj)
					{
						LOGGING_ONLY(log.WriteDataValue("FAIL", "%s is already using this dummy position", pFoundObj->GetNetworkObject()->GetLogName()));
						pObjectToRegister = NULL;
					}
				}
			}

			if (pObjectToRegister)
			{
				pObjectToRegister->m_nObjectFlags.bWeWantOwnership = weWantOwnership;
				NetworkInterface::RegisterObject(pObjectToRegister);
			}
		}
	}
	else
	{
		LOGGING_ONLY(log.WriteDataValue("FAIL", "Object population rejected"));
	}
}

bool CNetObjObject::AddUnassignedAmbientObject(CNetObjObject& object, const char* LOGGING_ONLY(reason))
{
#if ENABLE_NETWORK_LOGGING
	netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "ADDING_UNASSIGNED_AMBIENT_OBJ", object.GetLogName());
	log.WriteDataValue("Reason", reason);
	log.WriteDataValue("Dummy Position", "%f, %f, %f", object.m_DummyPosition.x, object.m_DummyPosition.y, object.m_DummyPosition.z);
	if (object.m_FragGroupIndex != -1)
	{
		log.WriteDataValue("Frag group index", "%d", object.m_FragGroupIndex);
	}
#endif // ENABLE_NETWORK_LOGGING

	if (HasSpaceForUnassignedAmbientObject())
	{
		// check the object does not already exist
		for (u32 i=0; i<ms_numUnassignedAmbientObjects; i++)
		{
			CNetObjObject* pUnassignedObj = ms_unassignedAmbientObjects[i];

			if (pUnassignedObj == &object)
			{
				LOGGING_ONLY(log.WriteDataValue("WARNING", "Already exists in array"));
				gnetAssertf(0, "%s is already in the unassigned objects array", object.GetLogName());
				return true;
			}
			else if (AssertVerify(pUnassignedObj))
			{
				// we may already have a network object trying to claim the same CObject from another player. In this case favour the player with the
				// lowest physical player index
				if (pUnassignedObj->m_FragGroupIndex == object.m_FragGroupIndex && IsSameDummyPos(pUnassignedObj->m_DummyPosition, object.m_DummyPosition))
				{
					gnetAssertf(pUnassignedObj->GetPlayerOwner() != object.GetPlayerOwner(), "Two objects with same player owner in unassigned object array (%s, %s)", pUnassignedObj->GetLogName(), object.GetLogName());

					if (AssertVerify(object.GetPlayerOwner()))
					{
						PhysicalPlayerIndex objPlayerIndex = object.GetPlayerOwner()->GetPhysicalPlayerIndex();

						LOGGING_ONLY(log.WriteDataValue("CONFLICT", "%s shares same dummy position", pUnassignedObj->GetLogName()));

						if (!pUnassignedObj->GetPlayerOwner())
						{
							LOGGING_ONLY(log.WriteDataValue("SUCCESS", "Dump %s (no player owner)", pUnassignedObj->GetLogName()));
							RemoveUnassignedAmbientObject(*pUnassignedObj, "Owner conflict", true, i);
						}
						else if (objPlayerIndex == GetObjectConflictPrecedence(objPlayerIndex, pUnassignedObj->GetPlayerOwner()->GetPhysicalPlayerIndex()))
						{
							LOGGING_ONLY(log.WriteDataValue("SUCCESS", "Dump %s", pUnassignedObj->GetLogName()));
							RemoveUnassignedAmbientObject(*pUnassignedObj, "Owner conflict", true, i);
						}
						else
						{
							LOGGING_ONLY(log.WriteDataValue("FAIL", "Keep %s", pUnassignedObj->GetLogName()));
							return false;
						}
					}
				}
			}
		}

		if (ms_numUnassignedAmbientObjects < MAX_UNASSIGNED_AMBIENT_OBJECTS)
		{
			ms_unassignedAmbientObjects[ms_numUnassignedAmbientObjects++] = &object;

			object.m_UnassignedAmbientObject = true;

			if (ms_numUnassignedAmbientObjects > ms_peakUnassignedAmbientObjects)
			{
				ms_peakUnassignedAmbientObjects = ms_numUnassignedAmbientObjects;
			}

			if (object.HasGameObject())
			{
				SafeCast(CObject, object.GetEntity())->SetNetworkObject(NULL);
				object.SetGameObject(NULL);
				object.DestroyNetBlender();
			}

			if (object.HasSyncData())
			{
				object.StopSynchronising();
			}

#if ENABLE_NETWORK_LOGGING
			log.WriteDataValue("Num unassigned objects", "%d", ms_numUnassignedAmbientObjects);
			log.WriteDataValue("Peak unassigned objects", "%d", ms_peakUnassignedAmbientObjects);
#endif
			return true;
		}
	}

	return false;
}

void CNetObjObject::RemoveUnassignedAmbientObject(CNetObjObject& object, const char* LOGGING_ONLY(reason), bool bUnregisterObject, int objectSlot)
{
#if ENABLE_NETWORK_LOGGING
	netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "REMOVE_UNASSIGNED_AMBIENT_OBJ", object.GetLogName());
	log.WriteDataValue("Reason", reason);
#endif // ENABLE_NETWORK_LOGGING

	if (objectSlot == -1)
	{
		for (u32 i=0; i<ms_numUnassignedAmbientObjects; i++)
		{
			gnetAssert(ms_unassignedAmbientObjects[i]);

			if (ms_unassignedAmbientObjects[i] == &object)
			{
				objectSlot = i;
				break;
			}
		}
	}
	
	if (objectSlot != -1)
	{
		for (u32 i=objectSlot; i<ms_numUnassignedAmbientObjects-1; i++)
		{
			ms_unassignedAmbientObjects[i] = ms_unassignedAmbientObjects[i+1];
		}

		ms_numUnassignedAmbientObjects--;

		LOGGING_ONLY(log.WriteDataValue("Num unassigned objects", "%d", ms_numUnassignedAmbientObjects));
	}	
	else
	{
		LOGGING_ONLY(log.WriteDataValue("FAIL", "Not found"));
	}

	if (bUnregisterObject)
	{
		NetworkInterface::GetObjectManager().UnregisterNetworkObject(&object, CNetworkObjectMgr::AMBIENT_OBJECT_OWNERSHIP_CONFLICT);
	}
}

void CNetObjObject::RemoveUnassignedAmbientObjectsInArea(const Vector3& centre, float radius)
{
	float radiusSqr = radius*radius;

	for (int i=0; i<(int)ms_numUnassignedAmbientObjects; i++)
	{
		CNetObjObject* pUnassignedNetObject = ms_unassignedAmbientObjects[i];

		if (AssertVerify(pUnassignedNetObject))
		{
			if (!pUnassignedNetObject->IsClone() && !pUnassignedNetObject->IsPendingOwnerChange())
			{
				Vector3 diff = pUnassignedNetObject->m_DummyPosition - centre;

				if (diff.Mag2() <= radiusSqr)
				{
					RemoveUnassignedAmbientObject(*pUnassignedNetObject, "RemoveUnassignedAmbientObjectsInArea", true, i);
					i--;
				}
			}
			else
			{
				pUnassignedNetObject->FlagForDeletion();
			}
		}
	}
}

void CNetObjObject::AssignVehicleFrags(CObject& object)
{
	// the object can already have a network object if networked when being broken
	if (object.GetNetworkObject())
	{
		return;
	}

	if(!object.GetFragParent() || !object.GetFragParent()->GetIsTypeVehicle())
	{
		return;
	}

	CVehicle* parentVehicle = (CVehicle*)object.GetFragParent();
	if(!parentVehicle->GetNetworkObject())
	{
		return;
	}
	
	CNetObjVehicle* netParentVehicle = SafeCast(CNetObjVehicle, parentVehicle->GetNetworkObject());

	for (u32 i=0; i<MAX_UNASSIGNED_VEHICLE_FRAGS; i++)
	{
		if(ms_unassignedVehicleFrags[i] != NETWORK_INVALID_OBJECT_ID)
		{
			netObject* unassignedObj = NetworkInterface::GetNetworkObject(ms_unassignedVehicleFrags[i]);
			if (unassignedObj)
			{
				if(unassignedObj->GetObjectType() == NET_OBJ_TYPE_OBJECT)
				{
					CNetObjObject* unassignedNetObject = SafeCast(CNetObjObject, unassignedObj);
					if(netParentVehicle->GetObjectID() == unassignedNetObject->GetFragParentVehicleId())
					{
						CObject* vehicleFrag = parentVehicle->GetDetachedTombStone();
						if(vehicleFrag)
						{
							unassignedNetObject->SetGameObject(vehicleFrag);
							vehicleFrag->SetNetworkObject(unassignedNetObject);

							if (!unassignedNetObject->GetNetBlender())
							{
								unassignedNetObject->CreateNetBlender();
							}

							unassignedNetObject->UpdateBlenderState(true, true, true, true);

							if (unassignedNetObject->GetNetBlender())
							{
								unassignedNetObject->GetNetBlender()->GoStraightToTarget();
							}

							ms_unassignedVehicleFrags[i] = NETWORK_INVALID_OBJECT_ID;
							unassignedNetObject->m_UnassignedVehicleFragment = false;

		#if ENABLE_NETWORK_LOGGING
							netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
							NetworkLogUtils::WriteLogEvent(log, "ASSIGNING_VEHICLE_FRAGMENT", unassignedNetObject->GetLogName());
							log.WriteDataValue("Object", "%p", vehicleFrag);
		#endif // ENABLE_NETWORK_LOGGING
						}
						return;
					}
				}
				else
				{
					gnetAssertf(0, "ms_unassignedVehicleFrags contains at index %d a non object type entity: %s", i, unassignedObj->GetLogName());
					ms_unassignedVehicleFrags[i] = NETWORK_INVALID_OBJECT_ID;
				}
			}
		}
	}
}

bool CNetObjObject::AddUnassignedVehicleFrag(CNetObjObject& object)
{
	int freeIndex = -1;
	for (u32 i=0; i<MAX_UNASSIGNED_VEHICLE_FRAGS; i++)
	{
		if(freeIndex == -1 && ms_unassignedVehicleFrags[i] == NETWORK_INVALID_OBJECT_ID)
		{
			freeIndex = i;
		}
		if(ms_unassignedVehicleFrags[i] == object.GetObjectID())
		{
			return true;
		}
	}

	if(freeIndex == -1)
	{
		return false;
	}

	if(freeIndex < MAX_UNASSIGNED_VEHICLE_FRAGS)
	{
		ms_unassignedVehicleFrags[freeIndex] = object.GetObjectID();
		if (object.HasGameObject())
		{
			SafeCast(CObject, object.GetEntity())->SetNetworkObject(NULL);
			object.SetGameObject(NULL);
			object.DestroyNetBlender();
		}

		object.m_UnassignedVehicleFragment = true;

		if (object.HasSyncData())
		{
			object.StopSynchronising();
		}
#if ENABLE_NETWORK_LOGGING
		netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "ADDING_UNASSIGNED_VEHICLE_FRAGMENT", object.GetLogName());
		log.WriteDataValue("Index", "%d", freeIndex);
#endif // ENABLE_NETWORK_LOGGING

		return true;
	}
	return false;
}

CNetObjObject* CNetObjObject::FindUnassignedVehicleFragment(ObjectId parentVehicleId)
{
	for (u32 i=0; i<MAX_UNASSIGNED_VEHICLE_FRAGS; i++)
	{
		if(ms_unassignedVehicleFrags[i] != NETWORK_INVALID_OBJECT_ID)
		{
			netObject* unassignedObject = NetworkInterface::GetNetworkObject(ms_unassignedVehicleFrags[i]);
			if(unassignedObject && unassignedObject->GetObjectType() == NET_OBJ_TYPE_OBJECT)
			{
				CNetObjObject* unassignedNetObject = SafeCast(CNetObjObject, unassignedObject);
				if(unassignedNetObject->GetFragParentVehicleId() == parentVehicleId)
				{
					return unassignedNetObject;
				}
			}
		}
	}
	return NULL;
}

CObject* CNetObjObject::FindAssociatedVehicleFrag(ObjectId parentVehicleId)
{
	netObject* netParent = NetworkInterface::GetNetworkObject((ObjectId) parentVehicleId);
	if(netParent)
	{
		CNetObjVehicle* netParentVehicle = SafeCast(CNetObjVehicle, netParent);
		CVehicle* parentVehicle = netParentVehicle->GetVehicle();
		if(parentVehicle)
		{
			parentVehicle->DetachTombstone();
			return parentVehicle->GetDetachedTombStone();
		}
	}
	return nullptr;
}

void CNetObjObject::RemoveUnassignedVehicleFragment(CNetObjObject& object)
{
#if ENABLE_NETWORK_LOGGING
	netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "REMOVE_UNASSIGNED_VEHICLE_FRAGMENT", object.GetLogName());
#endif // ENABLE_NETWORK_LOGGING

	for (u32 i=0; i < MAX_UNASSIGNED_VEHICLE_FRAGS; i++)
	{
		if (ms_unassignedVehicleFrags[i] == object.GetObjectID())
		{
			ms_unassignedVehicleFrags[i] = NETWORK_INVALID_OBJECT_ID;
			return;
		}
	}
}
