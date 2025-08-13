
//
// name:        NetObjEntity.h
// description: Derived from netObject, this class handles all entity-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjPhysical.h"

#include "grcore/debugdraw.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "physics/constraintattachment.h"
#include "physics/simulator.h"
#if __BANK
#include "system/stack.h"
#include "system/param.h"
#endif

// game headers
#include "Cutscene/CutSceneManagerNew.h"
#include "Cutscene/AnimatedModelEntity.h"  //AnimatedModelEntity.h declaration needs to go after CutSceneManagerNew.h to see cutfEvent
#include "debug/DebugRecorder.h"
#include "frontend/NewHud.h"
#include "frontend/UIReticule.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "network/Debug/NetworkDebug.h"
#include "Network/Events/NetworkEventTypes.h"
#include "network/general/NetworkDamageTracker.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "network/objects/synchronisation/syncnodes/PhysicalSyncNodes.h"
#include "network/players/NetGamePlayer.h"
#include "pedgroup/pedgroup.h"
#include "peds/ped.h"
#include "peds/ped.h"
#include "physics/physics.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/FocusEntity.h"
#include "scene/Physical.h"
#include "scene/world/gameWorld.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/Handlers/GameScriptIds.h"
#include "script/Handlers/GameScriptHandlerNetwork.h"
#include "script/commands_entity.h"
#include "script/script.h"
#include "vehicles/vehicle.h"
#include "vehicles/Heli.h"
#include "text/Text.h"
#include "text/TextConversion.h"
#include "event/EventGroup.h"
#include "stats/StatsMgr.h"
#include "Peds/pedDefines.h"
#include "event/EventDamage.h"

#if __BANK
#include "scene/DynamicEntity.h"
#endif

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

#if __BANK
PARAM(netDebugLastDamage, "Debug last damage done to physical entities.");
PARAM(netExcessiveAlphaLogging, "Prints logging when the network code sets the alpha");
#endif

#if __BANK
bool CNetObjPhysical::ms_bOverrideBlenderForFocusEntity = false;
bool CNetObjPhysical::ms_bDebugDesiredVelocity          = false;
#endif

#define NETOBJPHYSICAL_FIXED_PHYSICS_ACTIVATE_DIST   (MOVER_BOUNDS_PRESTREAM)
#define NETOBJPHYSICAL_FIXED_PHYSICS_DEACTIVATE_DIST (MOVER_BOUNDS_PRESTREAM - 20.0f)

FW_INSTANTIATE_CLASS_POOL(CPendingScriptObjInfo, 10, atHashString("CPendingScriptObjInfo",0xaf09e29c));

CPhysicalBlenderData s_physicalBlenderData;
CPhysicalBlenderData *CNetObjPhysical::ms_physicalBlenderData = &s_physicalBlenderData;

u32 CNetObjPhysical::ms_collisionSyncIgnoreTimeVehicle = 0; // syncs are ignored for this period of time after a collision with a vehicle
u32 CNetObjPhysical::ms_collisionSyncIgnoreTimeMap     = 0; // syncs are ignored for this period of time after a collision with the map
float CNetObjPhysical::ms_collisionMinImpulse   = 1000.0f; // the minimum impulse that will trigger the ignoring of syncs for a while
u32 CNetObjPhysical::ms_timeBetweenCollisions   = 1000; // the minimum time allowed between the allowed setting of the collision timer

u8 CNetObjPhysical::ms_ghostAlpha = DEFAULT_GHOST_ALPHA;
bool CNetObjPhysical::ms_invertGhosting = false;

CNetObjPhysical::FixedByNetworkInAirCheckData CNetObjPhysical::m_FixedByNetworkInAirCheckData;

CNetObjPhysical::CNetObjPhysical(CPhysical					*physical,
                                 const NetworkObjectType	type,
                                 const ObjectId				objectID,
                                 const PhysicalPlayerIndex  playerIndex,
                                 const NetObjFlags			localFlags,
                                 const NetObjFlags			globalFlags) :
CNetObjDynamicEntity(physical, type, objectID, playerIndex, localFlags, globalFlags)
, m_isInWater(false)
, m_collisionTimer(0)
, m_timeSinceLastCollision(0)
, m_damageTracker(0)
, m_lastDamageObjectID(NETWORK_INVALID_OBJECT_ID)
, m_lastDamageWeaponHash(0)
, m_alphaRampingTimeRemaining(-1)
, m_pendingAttachmentObjectID(NETWORK_INVALID_OBJECT_ID)
, m_pendingOtherAttachBone(-1)
, m_pendingMyAttachBone(-1)
, m_pendingAttachFlags(0)
, m_pendingAllowInitialSeparation(true)
, m_pendingInvMassScaleA(1.0f)
, m_pendingInvMassScaleB(1.0f)
, m_pendingScriptObjInfo(0)
, m_pendingDetachmentActivatePhysics(false)
, m_pendingAttachStateMismatch(false)
, m_bWasHiddenForTutorial(false)
, m_bPassControlInTutorial(false)
, m_hiddenUnderMap(false)
#if __ASSERT
, m_bScriptAssertIfRemoved(false)
#endif // __ASSERT
#if __DEV
, m_scriptExtension(0)
#endif // __DEV
, m_LastTimeFixed(0)
, m_InAirCheckPerformed(false)
, m_ZeroVelocityLastFrame(false)
, m_TimeToForceLowestUpdateLevel(0)
, m_LowestUpdateLevelToForce(CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW)
, m_LastCloneStateCheckedForPredictErrors(0)
, m_bAlphaRamping(false)
, m_bAlphaIncreasing(false)
, m_bAlphaIncreasingNoFlash(false)
, m_iAlphaOverride(255)
, m_bAlphaFading(false)
, m_bAlphaRampOut(false)
, m_bAlphaRampTimerQuit(false)
, m_bNetworkAlpha(false)
, m_bHideWhenInvisible(false)
, m_bHasHadAlphaAltered(false)
, m_bOverrideBlenderUntilAcceptingObjects(false)
, m_bAttachedToLocalObject(false)
, m_bForceSendPhysicalAttachVelocityNode	(false)
, m_FrameForceSendRequested(0)
, m_bInCutsceneLocally(false)
, m_bInCutsceneRemotely(false)
, m_bGhost(false)
, m_bGhostedForGhostPlayers(false)
, m_GhostCollisionSlot(CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT)
, m_BlenderDisabledAfterDetaching(false)
, m_BlenderDisabledAfterDetachingFrame(0)
, m_CameraCollisionDisabled(false)
, m_entityConcealed(false)
, m_entityConcealedLocally(false)
, m_AllowMigrateToSpectator(false)
, m_allowDamagingWhileConcealed(false)
, m_entityPendingConcealment(false)
, m_TriggerDamageEventForZeroDamage(false)
, m_TriggerDamageEventForZeroWeaponHash(false)
, m_LastReceivedVisibilityFlag(true)
, m_MaxHealthSetByScript(false)
, m_KeepCollisionDisabledAfterAnimScene(false)
#if __BANK
, m_lastDamageWasSet(false)
#endif
#if ENABLE_NETWORK_LOGGING
, m_LastFixedByNetworkChangeReason(FBN_NONE)
, m_LastNpfbnChangeReason(NPFBN_NONE)
, m_LastFailedAttachmentReason(CPA_SUCCESS)
#endif // ENABLE_NETWORK_LOGGING
, m_AllowCloneWhileInTutorial(false)
{
    m_LastVelocityReceived.Zero();
}

CNetObjPhysical::~CNetObjPhysical()
{
	ASSERT_ONLY(gnetAssertf(!m_bScriptAssertIfRemoved, "Entity %s tagged by script and is being removed.", GetLogName());)

    delete m_damageTracker;
    m_damageTracker = 0;

	if (m_pendingScriptObjInfo)
	{
		delete m_pendingScriptObjInfo;
	}

	if (m_GhostCollisionSlot != CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT)
	{
		CNetworkGhostCollisions::RemoveGhostCollision((int)m_GhostCollisionSlot, false);
	}
}

netINodeDataAccessor *CNetObjPhysical::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IPhysicalNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IPhysicalNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjDynamicEntity::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

//
// name:        RegisterCollision
// description: Called when this object collides with another network object
// Parameters:  pCollider - the other physical object we have collided with
//              impulseMag - the size of the collision impulse
void CNetObjPhysical::RegisterCollision(CNetObjPhysical* pCollider, float impulseMag)
{
    // if a clone is colliding with a local object we need to ignore sync updates from the clone for a short period
    // so that the collision response looks nicer. Otherwise we may get a sync update immediately after the collision
    // which places the object in its position just before the collision. This would mean the object would look like it
    // had a delayed response to the collision as the next sync from the object would then set it moving.
    if(!AssertVerify(pCollider))
        return;

	if (GetNetBlender())
	{
		GetNetBlender()->RegisterCollision(pCollider, impulseMag);
	}

    if (impulseMag >= ms_collisionMinImpulse && m_timeSinceLastCollision == 0)
    {
        if(GetPhysical() && GetPhysical()->GetIsTypeVehicle() && pCollider->GetPhysical() && pCollider->GetPhysical()->GetIsTypeVehicle())
        {
            if (IsClone() && !pCollider->IsClone())
            {
                if (pCollider->GetPhysical()->GetVelocity().Mag2() > GetPhysical()->GetVelocity().Mag2())
                {
                    m_collisionTimer = ms_collisionSyncIgnoreTimeVehicle;
                    m_timeSinceLastCollision = ms_timeBetweenCollisions;
                }
            }
            else if (!IsClone() && pCollider->IsClone())
            {
                if (GetPhysical()->GetVelocity().Mag2() > pCollider->GetPhysical()->GetVelocity().Mag2())
                {
                    pCollider->m_collisionTimer = ms_collisionSyncIgnoreTimeVehicle;
                    m_timeSinceLastCollision = ms_timeBetweenCollisions;
                }
            }
        }
    }
}

void CNetObjPhysical::RegisterExplosionImpact(float /*impulseMag*/)
{
    static const unsigned TIME_TO_FORCE_UPDATE = 10000;
    SetLowestAllowedUpdateLevel(CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH, TIME_TO_FORCE_UPDATE);
}

void CNetObjPhysical::ForceHighUpdateLevel()
{
	static const unsigned TIME_TO_FORCE_UPDATE = 10000;
	SetLowestAllowedUpdateLevel(CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH, TIME_TO_FORCE_UPDATE);
}

void CNetObjPhysical::RegisterCollisionWithBuilding(float impulseMag)
{
    if(IsClone() && GetPhysical() && GetPhysical()->GetIsTypeVehicle())
    {
        if (impulseMag >= ms_collisionMinImpulse && m_timeSinceLastCollision == 0)
        {
            m_collisionTimer         = ms_collisionSyncIgnoreTimeMap;
            m_timeSinceLastCollision = ms_timeBetweenCollisions;
        }
    }
}

// name:        GetIsAttachedForSync
// description: returns whether this object is consider attached for syncing purposes
bool CNetObjPhysical::GetIsAttachedForSync() const
{
    bool isAttachedForSync = false;

    // basic checks
    if(GetPhysical() &&
       GetPhysical()->GetIsAttached() &&
       GetPhysical()->GetAttachParent() &&
       NetworkUtils::GetNetworkObjectFromEntity((CPhysical *) GetPhysical()->GetAttachParent()))
    {
        // attachment state checks
        if(!GetPhysical()->GetIsAttachedInCar() && !GetPhysical()->GetIsAttachedOnMount())
        {
            // don't ever sync attachment states of weapons wielded by peds - they are synced elsewhere
            bool attachedWeaponObject = (GetPhysical()->GetIsTypeObject() && static_cast<CObject *>(GetPhysical())->GetWeapon() && ((CPhysical *) GetPhysical()->GetAttachParent())->GetIsTypePed());

            if(!attachedWeaponObject)
            {
                isAttachedForSync = true;
            }
        }
    }

    return isAttachedForSync;
}

// name:        PreventMigrationWhenPhysicallyAttached
// description: returns whether migration is not allowed when this object is physically attached
bool CNetObjPhysical::PreventMigrationWhenPhysicallyAttached(const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) const
{
	return false;
}

// name:        GetIsCloneAttached
// description: returns whether this clone object should be attached
bool CNetObjPhysical::GetIsCloneAttached() const
{
	return (m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID);
}

// name:        GetEntityAttachedTo
// description: returns the entity this object is attached to, or should be attached to
CEntity* CNetObjPhysical::GetEntityAttachedTo() const
{
	CPhysical* pPhysical = static_cast<CPhysical*>(GetEntity());
	CEntity* pAttachmentEntity = NULL;

	if (gnetVerify(pPhysical))
	{
		if (pPhysical->GetIsAttached())
		{
			pAttachmentEntity = static_cast<CEntity*>(pPhysical->GetAttachParent());
		}
		else if (m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID)
		{
			netObject* pNetObj = NetworkInterface::GetNetworkObject(m_pendingAttachmentObjectID);

			if (pNetObj)
			{
				pAttachmentEntity = pNetObj->GetEntity();
			}
		}
	}

	return pAttachmentEntity;
}

bool CNetObjPhysical::HasBeenDamagedBy(const CPhysical *physical) const
{
    bool hasBeenDamagedBy = false;

    if(physical && physical->GetNetworkObject())
    {
        if(m_lastDamageObjectID != NETWORK_INVALID_OBJECT_ID)
        {
            hasBeenDamagedBy = (m_lastDamageObjectID == physical->GetNetworkObject()->GetObjectID());

            if(!hasBeenDamagedBy && physical->GetIsTypePed())
            {
                const CPed *ped = static_cast<const CPed *>(physical);

                if (ped->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped->GetMyVehicle() && ped->GetMyVehicle()->GetNetworkObject() && ped->GetMyVehicle()->GetDriver() && (ped->GetMyVehicle()->GetDriver() == ped))
                {
                    hasBeenDamagedBy = (m_lastDamageObjectID == ped->GetMyVehicle()->GetNetworkObject()->GetObjectID());
                }
            }
        }
    }

    return hasBeenDamagedBy;
}

void CNetObjPhysical::ClearLastDamageObject()
{
	m_lastDamageObjectID   = NETWORK_INVALID_OBJECT_ID;
	m_lastDamageWeaponHash = 0;
}

CEntity *CNetObjPhysical::GetLastDamageObjectEntity() const
{
    CEntity *entity = 0;

    if(m_lastDamageObjectID != NETWORK_INVALID_OBJECT_ID)
    {
        netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_lastDamageObjectID);

        if(networkObject)
        {
            entity = networkObject->GetEntity();
        }
    }

    return entity;
}

scriptObjInfoBase*  CNetObjPhysical::GetScriptObjInfo()
{
    scriptHandlerObject* pScriptObj = GetScriptHandlerObject();

    if (pScriptObj)
    {
        return pScriptObj->GetScriptInfo();
    }

    return NULL;
}

const scriptObjInfoBase*    CNetObjPhysical::GetScriptObjInfo() const
{
    scriptHandlerObject* pScriptObj = GetScriptHandlerObject();

    if (pScriptObj)
    {
        return pScriptObj->GetScriptInfo();
    }

    return NULL;
}

void CNetObjPhysical::SetScriptObjInfo(scriptObjInfoBase* info)
{
    CPhysical* pPhysical = static_cast<CPhysical*>(GetEntity());

    if (gnetVerify(pPhysical))
    {
        CScriptEntityExtension* pExtension = FindScriptExtension();

		// don't unregister with the handler here, if info is null. This will be done in CProjectSyncTree::ApplyNodeData() after all the node 
		// data has been applied. Unregistering here will kick off the conversion to ambient stuff, calling CProjectSyncTree::ResetScriptState(), 
		// which will clear the updated flags on the tree. This will mean that the data in the nodes following this will not be applied.

		if (info)
        {
            scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(info->GetScriptId());
            bool bRegister = false;

            if (!pExtension)
            {
                pExtension = rage_new CScriptEntityExtension(*pPhysical);
                bRegister = true;
            }
            else if (gnetVerify(pExtension->GetScriptInfo()))
            {
                if (*info != *pExtension->GetScriptInfo())
                {
                    scriptHandler* pHandler = pExtension->GetScriptHandler();

                    if (pHandler)
                    {
                        pHandler->UnregisterScriptObject(*pExtension);

                        if (FindScriptExtension() == 0)
                        {
                            pExtension = rage_new CScriptEntityExtension(*pPhysical);
                        }
                    }

                    bRegister = true;
                }
            }
            else
            {
                bRegister = true;
            }

            if (bRegister)
            {
                pExtension->SetScriptInfo(*info);

				if (pHandler && pHandler->GetNetworkComponent())
                {
                    pHandler->RegisterExistingScriptObject(*pExtension);
                }
				else
				{
					OnConversionToScriptObject();
				}
            }
        }
    }
}

scriptHandlerObject* CNetObjPhysical::GetScriptHandlerObject() const
{
    return static_cast<scriptHandlerObject*>(FindScriptExtension());
}

void CNetObjPhysical::SetScriptExtension(CScriptEntityExtension* DEV_ONLY(ext))
{
#if __DEV
    m_scriptExtension = ext;
#endif
}

void CNetObjPhysical::AddBoxStreamerSearches(atArray<fwBoxStreamerSearch>& searchList)
{
    if(m_FixedByNetworkInAirCheckData.m_ObjectID != NETWORK_INVALID_OBJECT_ID)
    {
		if (NetworkInterface::IsGameInProgress())
		{
			netObject *networkObject = NetworkInterface::GetNetworkObject(m_FixedByNetworkInAirCheckData.m_ObjectID);

			if(networkObject && networkObject->GetEntity())
			{
				Vec3V position = networkObject->GetEntity()->GetTransform().GetPosition();

				const spdSphere searchSphere(position, ScalarV(V_TEN));

				searchList.Grow() = fwBoxStreamerSearch(
					searchSphere,
					fwBoxStreamerSearch::TYPE_NETWORK,
					fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER,
					false
				);
			}
		}
    }
}

void CNetObjPhysical::ProcessFixedByNetwork(bool bForceForCutscene)
{
	CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (!pPhysical || !pPhysical->GetCurrentPhysicsInst() || !pPhysical->GetCurrentPhysicsInst()->IsInLevel())
	{
		return;
	}

	bool bHiddenForCutscene = ShouldObjectBeHiddenForCutscene();

	if (AllowFixByNetwork() || bHiddenForCutscene || bForceForCutscene)
	{
		bool fixObject = false;
		unsigned fixedByNetworkReason = FBN_NONE;
		unsigned npfbnReason = NPFBN_NONE;

		if (bHiddenForCutscene)
		{
			fixObject = true;
			fixedByNetworkReason = FBN_HIDDEN_FOR_CUTSCENE;
		}
		else if (ShouldObjectBeHiddenForTutorial())
		{
			fixObject = true;
			fixedByNetworkReason = FBN_TUTORIAL;
		}
		else if (m_entityConcealedLocally)
		{
			fixObject = true;
			fixedByNetworkReason = FBN_FIXED_FOR_LOCAL_CONCEAL;
		}
		else
		{
			if (FixPhysicsWhenNoMapOrInterior(&npfbnReason))
			{
				const Vector3 vPhysicalPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());

				// set fixed physics on objects that are on an area of map or in an interior that has no collision streamed in, otherwise
				// they will fall through the map. We need to do this for local objects too because when the player is respawned objects may be 
				// still owned for a while even though they are far away (they may be visible to another player)
				if(IsInUnstreamedInterior(&fixedByNetworkReason))
				{
					fixObject = true;
					fixedByNetworkReason = FBN_UNSTREAMED_INTERIOR;
				}
				else 
				{
					if(UseBoxStreamerForFixByNetworkCheck() || !CNetwork::IsCollisionLoadedAroundLocalPlayer())
					{
						fixObject = !HasCollisionLoadedUnderEntity();
						fixedByNetworkReason = FBN_BOX_STREAMER;
					}
					else
					{
						Vector3 diff = vPhysicalPosition - CFocusEntityMgr::GetMgr().GetPopPos();

						const float fFixedByNetworkDist = NETOBJPHYSICAL_FIXED_PHYSICS_ACTIVATE_DIST;
						if (rage::Abs(diff.x) > fFixedByNetworkDist ||
							rage::Abs(diff.y) > fFixedByNetworkDist)
						{
							if(CheckBoxStreamerBeforeFixingDueToDistance())
							{
								fixObject = !HasCollisionLoadedUnderEntity();
								fixedByNetworkReason = FBN_BOX_STREAMER;
							}
							else
							{
								if(CFocusEntityMgr::GetMgr().HasOverridenPop())
								{
									diff = vPhysicalPosition - CFocusEntityMgr::GetMgr().GetPos();
									if (rage::Abs(diff.x) > fFixedByNetworkDist ||
										rage::Abs(diff.y) > fFixedByNetworkDist)
									{
										if(CheckBoxStreamerBeforeFixingDueToDistance())
										{
											fixObject = !HasCollisionLoadedUnderEntity();
											fixedByNetworkReason = FBN_BOX_STREAMER;
										}
										else
										{
											fixObject = true;
											fixedByNetworkReason = FBN_DISTANCE;
										}
									}
								}
								else
								{
									fixObject = true;
									fixedByNetworkReason = FBN_DISTANCE;
								}
							}
						}
					}
				}
			}
			else
			{
				fixedByNetworkReason = FBN_NOT_PROCESSING_FIXED_BY_NETWORK;
			}
		}

		UpdateFixedByNetwork(fixObject, fixedByNetworkReason, npfbnReason);
	}

}

void CNetObjPhysical::ProcessAlpha()
{
	CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (!pPhysical)
		return;

	//static const unsigned NETOBJPHYSICAL_ALPHA_RAMPING_DELTA = 50;
	static const unsigned NETOBJPHYSICAL_ALPHA_FADING_DELTA = 10;
	
	if (m_bAlphaFading)
	{
		m_iAlphaOverride -= NETOBJPHYSICAL_ALPHA_FADING_DELTA;

		if (m_iAlphaOverride <= 0)
		{
			m_iAlphaOverride = 0;
			SetAlphaFading(false);
			SetInvisibleAfterFadeOut();
			SetHideWhenInvisible(true);
		}
		else
		{
			SetAlpha(m_iAlphaOverride);
		}
	}
	else if(m_bAlphaIncreasingNoFlash)
	{
		SetHideWhenInvisible(false);
		m_iAlphaOverride += NETOBJPHYSICAL_ALPHA_FADING_DELTA;

		if (m_iAlphaOverride >= 255)
		{
			m_bAlphaIncreasingNoFlash = false;
			m_iAlphaOverride = 255;
			SetAlphaFading(false);
		}
		else
		{
			SetAlpha(m_iAlphaOverride);
		}
	}
	else if (m_bAlphaRamping)
	{
		if (m_alphaRampingTimeRemaining > 0)
		{
			// don't start fading in until the entity is visible
			if (m_bAlphaRampOut || pPhysical->GetIsVisible() || IsLocalEntityVisibleOverNetwork())
			{
				u32 timeStep = MIN(fwTimer::GetSystemTimeStepInMilliseconds(), 100);
				m_alphaRampingTimeRemaining -= static_cast<s16>(timeStep);
			}

			if (m_alphaRampingTimeRemaining <= 0)
			{
				m_alphaRampingTimeRemaining = 0;

				if (!m_bAlphaRampOut && !m_bAlphaRampTimerQuit)
				{
					m_alphaRampingTimeRemaining = 1; //don't reduce to zero - always atleast 1 - will remain in special mode until terminated.
				}
			}
		}

		bool bWasIncreasing = m_bAlphaIncreasing;

		s16 maxTime = m_iCustomFadeDuration > 0 ? m_iCustomFadeDuration : (m_bAlphaRampOut ? NetworkUtils::GetSpecialAlphaRampingFadeOutTime() : NetworkUtils::GetSpecialAlphaRampingFadeInTime());

		s16 finalAlpha = NetworkUtils::GetSpecialAlphaRampingValue(m_iAlphaOverride, m_bAlphaIncreasing, maxTime, m_alphaRampingTimeRemaining, m_bAlphaRampOut); 

		SetAlpha(finalAlpha);

		// stop alpha ramping only once the alpha has finished a ramp
		if (m_alphaRampingTimeRemaining==0)
		{
			bool bRampFinished = m_bAlphaRampOut ? (!bWasIncreasing && m_bAlphaIncreasing) : (bWasIncreasing && !m_bAlphaIncreasing);

			if (bRampFinished || !pPhysical->GetIsVisible())
			{
				ResetAlphaActive();

				if (m_bAlphaRampOut)
				{
					SetInvisibleAfterFadeOut();
				}

				m_bAlphaRamping = false;
			}
		}
	}
	else if (m_bHasHadAlphaAltered && IsAlphaActive() && CanResetAlpha())
	{
		// and for insurance directly reset the alpha
		ResetAlphaActive();
	}
}

// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns      :   True if the object wants to unregister itself
bool CNetObjPhysical::Update()
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (!pPhysical)
		return false;

    if (m_collisionTimer > 0)
    {
		SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, false);
		SetRemotelyVisibleInCutscene(true);

		m_collisionTimer -= fwTimer::GetSystemTimeStepInMilliseconds();

        if (m_collisionTimer < 0)
            m_collisionTimer = 0;
    }

    if (m_timeSinceLastCollision > 0)
    {
       m_timeSinceLastCollision -= fwTimer::GetSystemTimeStepInMilliseconds();

        if (m_timeSinceLastCollision < 0)
            m_timeSinceLastCollision = 0;
    }

    if(m_TimeToForceLowestUpdateLevel > 0)
    {
        m_TimeToForceLowestUpdateLevel -= MIN(m_TimeToForceLowestUpdateLevel, static_cast<u16>(fwTimer::GetTimeStepInMilliseconds()));
    }

	if (m_bHideWhenInvisible &&	pPhysical && !m_bAlphaFading &&	!m_bAlphaRamping && IsVisibleToScript() && CanResetAlpha())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_OUT_FINISHED", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "CNetObjPhysical::Update()");

		SetHideWhenInvisible(false);
	}

    if(m_bOverrideBlenderUntilAcceptingObjects && NetworkInterface::GetLocalPlayer() && NetworkInterface::GetLocalPlayer()->CanAcceptMigratingObjects())
    {
        m_bOverrideBlenderUntilAcceptingObjects = false;
    }

	ProcessPendingAttachment();

    if(!IsClone())
    {
		UpdateForceSendRequests();

		// update the network water flag from the physical flag for non-clones
		// for clones this variable is just the last received value from the network
        m_isInWater = pPhysical->GetIsInWater();
 
		// if the object may have been flagged by the script as no longer needed and it is now local, convert it to an ambient object
		if (IsLocalFlagSet(LOCALFLAG_NOLONGERNEEDED) && !IsPendingOwnerChange())
		{
			ConvertToAmbientObject();
			gnetAssert(!IsLocalFlagSet(LOCALFLAG_NOLONGERNEEDED));
		}

        CheckPredictedPositionErrors();
	}
	else
	{
		UpdateBlenderData();
		// Keep the blender disabled one frame after detaching objects to handle edge cases 
		// such as bobcats attaching/detaching objects for 1 frame due to rope constraints.
		if (m_BlenderDisabledAfterDetaching)
		{
			if (fwTimer::GetFrameCount() > m_BlenderDisabledAfterDetachingFrame)
			{
				m_BlenderDisabledAfterDetaching = false;
			}
		}
	}
#if __BANK
	if(pPhysical && !pPhysical->IsCollisionEnabled())
	{
		phInst* pInst = pPhysical->GetCurrentPhysicsInst();
		if(pInst && pInst->IsInLevel() && PHLEVEL->IsActive(pInst->GetLevelIndex()))
		{
			Warningf("%s is active with no collision, it will probably fall through the map (fixed waiting on collision=%s, clone?=%s)",
				GetLogName(), pPhysical->GetIsFixedUntilCollisionFlagSet()?"true":"false", IsClone()?"true":"false");
		}
	}
#endif // __BANK

    m_ZeroVelocityLastFrame = pPhysical->GetVelocity().Mag2() < 0.001f;

	// remove the pending script obj info if the script that assigned it is not running anymore
	if (m_pendingScriptObjInfo)
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_pendingScriptObjInfo->m_info.GetScriptId());

		if (!pHandler || !pHandler->GetNetworkComponent() || pHandler->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING)
		{
			ClearPendingScriptObjInfo();
		}
		else if (!IsClone() && !IsScriptObject() && !IsPendingOwnerChange())
		{
			ApplyPendingScriptObjInfo();
		}
	}

	// possibly hide the entity if we are in a tutorial
	if (ShouldObjectBeHiddenForTutorial())
	{
		HideForTutorial();
        m_bWasHiddenForTutorial = true;
	}
    else 
    {
        if(m_bWasHiddenForTutorial && IsClone())
        {
            if(GetNetBlender())
            {
                GetNetBlender()->GoStraightToTarget();
            }
        }

        m_bWasHiddenForTutorial = false;
    }

	if (m_entityConcealedLocally)
	{
		// turn off collision and visibility
		SetOverridingLocalVisibility(false, "Entity concealed", true, true);
		SetOverridingLocalCollision(false, "Entity concealed");
	}

	if (m_CameraCollisionDisabled)
	{
		EnableCameraCollisions();
	}

	ProcessFixedByNetwork();
	ProcessAlpha();
	ProcessGhosting();

	if(m_entityPendingConcealment)
	{
		m_entityPendingConcealment = false;
		ConcealEntity(true, m_allowDamagingWhileConcealed);
	}

	if(IsConcealed())
	{
		HideUnderMap();
	}
	else if(m_hiddenUnderMap)
	{
		m_hiddenUnderMap = false;
		if(IsClone())
		{
			if(GetNetBlender())
			{
				GetNetBlender()->GoStraightToTarget();
			}
		}
		else
		{
			gnetDebug2("CNetObjPhysical::ConcealEntity %s Moving Back to Position: x:%.2f, y:%.2f, z:%.2f due to unconceal", GetLogName(), m_posBeforeHide.GetX(), m_posBeforeHide.GetY(), m_posBeforeHide.GetZ());
			pPhysical->SetPosition(m_posBeforeHide, true, true, true);
		}
	}

	return CNetObjDynamicEntity::Update();
}

void CNetObjPhysical::UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason)
{
    CPhysical *pPhysical = GetPhysical();

    if(pPhysical)
    {
		// attached objects inherit their parent object's fixed by network state
		if (AllowFixByNetwork())
		{
			CPhysical *attachParent = SafeCast(CPhysical, pPhysical->GetAttachParent());

			if(attachParent && attachParent->GetNetworkObject())
			{
				fixByNetwork = attachParent->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK);

				if (fixByNetwork)
				{
					reason = FBN_ATTACH_PARENT;
				}
			}
		}

        UpdateFixedByNetworkInAirChecks(fixByNetwork, reason);

		SetFixedByNetwork(fixByNetwork, reason, npfbnReason);
    }
}

#if __BANK
void CNetObjPhysical::DebugDesiredVelocity()
{
	if (ms_bDebugDesiredVelocity)
	{
		CPhysical *physical = GetPhysical();
		if (physical && physical == CDebugScene::FocusEntities_Get(0))
		{
			Vector3 position = VEC3V_TO_VECTOR3(physical->GetTransform().GetPosition());

			{
				Color32 color = Color_white;
				Vector3 vVelocity(Vector3::ZeroType);
				if (GetDynamicEntity()->GetIsTypePed())
				{
					CPed* pPed = (CPed*)GetGameObject();
					color = NetworkBlenderIsOverridden() ? Color_purple : pPed->GetUsingRagdoll() ? Color_pink : GetNetBlender()->IsBlendingOn() ? Color_orange : Color_white;
					vVelocity = pPed->GetDesiredVelocity();
				}
				else
				{
					if (GetDynamicEntity()->GetIsTypeVehicle())
						position.z += 3.5f;

					color = NetworkBlenderIsOverridden() ? Color_purple : GetNetBlender()->IsBlendingOn() ? Color_orange : Color_white;
					vVelocity = physical->GetVelocity();
				}

				Vector3 posvel = position + vVelocity;

				grcDebugDraw::Sphere(position, 0.05f, color, false, 60);
				grcDebugDraw::Line(position, posvel, color, color, 60);

				if (!m_vDebugPosition.IsZero())
				{
					// Vec3V debugvec3 = VECTOR3_TO_VEC3V(m_vDebugPosition); //unused
					bool bUp = ((position.z - m_vDebugPosition.z) > 0.f);
					Color32 colorDebugLine = bUp ? Color_green : Color_grey;
					grcDebugDraw::Line(m_vDebugPosition, position, colorDebugLine, colorDebugLine, 60);
				}
			}
			m_vDebugPosition = position;
		}
	}
}
#endif

void CNetObjPhysical::UpdateAfterPhysics()
{
	CNetObjDynamicEntity::UpdateAfterPhysics();

    // All active network clones will update their network blender
    // after the physics has been applied to them, however inactive
    // network clones still need their blenders updated, which is done here
    CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    if (GetNetBlender())
    {
        if(pPhysical && IsClone())
        {
            if (CanBlend())
            {
                if(pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->IsInLevel())
                {
                    if(!CPhysics::GetLevel()->IsActive(pPhysical->GetCurrentPhysicsInst()->GetLevelIndex()))
                    {
                        gnetAssert(GetNetBlender());
                        GetNetBlender()->ProcessPostPhysics();
                    }
                }
            }
            else if(pPhysical->GetIsAttached() && !CanBlendWhenAttached())
            {
                ResetBlenderDataWhenAttached();
            }
            else if(m_bInCutsceneLocally)
            {
                GetNetBlender()->Reset();
            }
        }
    }

#if __BANK
	DebugDesiredVelocity();
#endif 
}

void CNetObjPhysical::UpdateAfterScripts()
{
    //Level designers want code to handle clearing the last damage object at the end of the script update
    ClearLastDamageObject();

    CNetObjDynamicEntity::UpdateAfterScripts();
}

void CNetObjPhysical::CreateNetBlender()
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		gnetAssert(!GetNetBlender());

		netBlender *blender = rage_new CNetBlenderPhysical(pPhysical, ms_physicalBlenderData);
		gnetAssert(blender);
        blender->Reset();
        SetNetBlender(blender);
	}
}

u32 CNetObjPhysical::CalcReassignPriority() const
{
    u32 reassignPriority = CNetObjDynamicEntity::CalcReassignPriority();

    // network objects that are set to only migrate to machines running the script that created them must have the highest priority
    if (IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION) || IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT))
    {
        scriptHandlerObject* scriptObj = GetScriptHandlerObject();

        if (scriptObj &&
            scriptObj->GetScriptHandler() &&
            scriptObj->GetScriptHandler()->RequiresANetworkComponent() &&
            static_cast<CGameScriptHandlerNetwork*>(scriptObj->GetScriptHandler())->CanAcceptMigratingObject(*scriptObj, *NetworkInterface::GetLocalPlayer(), MIGRATE_FORCED))
        {
            reassignPriority |= ((1<<(SIZEOF_REASSIGNPRIORITY))-1);
        }
		else
		{
			reassignPriority = 0;
		}
    }

    return reassignPriority;
}

// name:        CanBlend
// description:  returns true if this object can use the net blender to blend updates
bool CNetObjPhysical::CanBlend(unsigned *resultCode) const
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
        if(IsLocalFlagSet(CNetObjGame::LOCALFLAG_DISABLE_BLENDING))
        {
            if(resultCode)
            {
                *resultCode = CB_FAIL_BLENDING_DISABLED;
            }

            return false;
        }

		if (IsAttachedOrDummyAttached() && !CanBlendWhenAttached())
		{
            if(resultCode)
            {
                *resultCode = CB_FAIL_ATTACHED;
            }

            return false;
        }

		if(pPhysical->GetIsAnyFixedFlagSet() && !CanBlendWhenFixed())
		{
            if(resultCode)
            {
                *resultCode = CB_FAIL_FIXED;
            }

            return false;
        }

        if(m_collisionTimer != 0)
        {
            if(resultCode)
            {
                *resultCode = CB_FAIL_COLLISION_TIMER;
            }

            return false;
        }

		return CNetObjEntity::CanBlend(resultCode);
	}

    if(resultCode)
    {
        *resultCode = CB_FAIL_NO_GAME_OBJECT;
    }

	return false;
}

// name:         NetworkBlenderIsOverridden
// description:  returns true if the blender is being overridden
bool CNetObjPhysical::NetworkBlenderIsOverridden(unsigned *resultCode) const
{
#if __BANK
    if(ms_bOverrideBlenderForFocusEntity && GetPhysical() && (CDebugScene::FocusEntities_Get(0) == GetPhysical()))
    {
        if(resultCode)
        {
            *resultCode = NBO_FOCUS_ENTITY_DEBUG;
        }

        return true;
    }
#endif // __BANK

	if (m_collisionTimer > 0)
	{
		if(resultCode)
		{
            *resultCode = NBO_COLLISION_TIMER;
        }

		return true;
	}
	else if (IsFadingOut())
	{
        if(resultCode)
        {
            *resultCode = NBO_FADING_OUT;
        }

		return true;
    }
    else if(m_bOverrideBlenderUntilAcceptingObjects)
    {
        if(resultCode)
        {
            *resultCode = NBO_NOT_ACCEPTING_OBJECTS;
        }

		return true;
    }
	else if (IsInCutsceneLocallyOrRemotely())
	{
		if(resultCode)
		{
			*resultCode = NBO_FAIL_RUNNING_CUTSCENE;
		}

		return true;
	}
	else if (m_BlenderDisabledAfterDetaching)
	{
		if(resultCode)
		{
			*resultCode = NBO_ATTACHED_PICKUP;
		}

		return true;
	}
	else
	{
		if(resultCode)
		{
			*resultCode = NBO_NOT_OVERRIDDEN;
		}
	}
	
    return false;
}

// Name         :   IsInScope
// Purpose      :
bool CNetObjPhysical::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
	const CNetGamePlayer* pNetGamePlayer = SafeCast(const CNetGamePlayer, &player);
	bool bPlayerInDifferentTutorial = !ShouldAllowCloneWhileInTutorial() && pNetGamePlayer->IsInDifferentTutorialSession();

    // host objects with the GLOBALFLAG_CLONEALWAYS_SCRIPT flag set are always cloned on other machines running the script that created the object, otherwise
	// the script can get stuck waiting for the object to be cloned and the script can screw up if hosting migrates to the player on the other tutorial (who
	// will not have the script objects)
    if (IsGlobalFlagSet(GLOBALFLAG_CLONEALWAYS_SCRIPT) && GetScriptObjInfo() && (!bPlayerInDifferentTutorial || GetScriptObjInfo()->IsScriptHostObject()))
    {
        if (CTheScripts::GetScriptHandlerMgr().IsPlayerAParticipant(static_cast<const CGameScriptId&>(GetScriptObjInfo()->GetScriptId()), player))
        {
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_SCRIPT_PARTICIPANT;
			}
			return true;
        }
    }

	if(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
	{
		if (AlwaysClonedForPlayer(player.GetPhysicalPlayerIndex()))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_ENTITY_ALWAYS_CLONED;
			}
			return true;
		}
	}

	// If child attachment is owned by this player than it should not destroy this entity
	int counter = 0;
	if(RecursiveAreAnyChildrenOwnedByPlayer(GetPhysical(), player, counter))
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_IN_CHILD_ATTACHMENT_OWNER;
		}
		return true;
	}

	// physical objects should not be cloned on machines of players in different tutorial sessions,
	// players can be in a solo tutorial session too, where nothing will be cloned on other machines
	if(bPlayerInDifferentTutorial)
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_PLAYER_IN_DIFFERENT_TUTORIAL;
		}
		return false;
	}


	const CNetObjPlayer* pNetObjPlayer = pNetGamePlayer->GetPlayerPed() ? SafeCast(const CNetObjPlayer, pNetGamePlayer->GetPlayerPed()->GetNetworkObject()) : NULL;
	if(pNetObjPlayer && pNetObjPlayer->IsConcealed() && GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		const CPed* pPed = SafeCast(const CPed,GetPhysical());
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByConcealedPlayer))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_PLAYER_CONCEALED_FOR_FLAGGED_PED;
			}
			return false;
		}
	}


    return CNetObjDynamicEntity::IsInScope(player, scopeReason);
}

bool CNetObjPhysical::RecursiveAreAllChildrenCloned(const CPhysical* parent, const netPlayer& player) const
{
	if(!parent)
	{
		return true;
	}

	if (parent->GetChildAttachment())
	{
		const CEntity* childEntity = SafeCast(const CEntity, parent->GetChildAttachment());
		if(childEntity && childEntity->GetIsPhysical())
		{
			if(!RecursiveAreAllChildrenCloned(SafeCast(const CPhysical, childEntity), player))
				return false;
		}
	}
	if (parent->GetSiblingAttachment())
	{
		const CEntity* childSibling = SafeCast(const CEntity, parent->GetSiblingAttachment());
		if(childSibling && childSibling->GetIsPhysical())
		{
			if(!RecursiveAreAllChildrenCloned(SafeCast(const CPhysical, childSibling), player))
				return false;
		}
	}

	// On clones return true which would still cause problems but the issue is that heli owner doesn't own attachment. That should never happen
	if(!parent->GetNetworkObject() || parent->GetNetworkObject()->IsClone())
	{
		return true;
	}

	// Only do this for local network objects because HasBennCloned doesn't work on clones
	if(parent->GetNetworkObject()->HasBeenCloned(player))
	{
		return true;
	}
	return false;
}

bool CNetObjPhysical::RecursiveAreAnyChildrenOwnedByPlayer(const CPhysical* parent, const netPlayer& player, int &counter) const
{
	static const int MAX_RECURSION_DEPTH = 100;

	if(!parent)
	{
		return false;
	}

	counter++;
	if(counter > MAX_RECURSION_DEPTH)
	{
		return false;
	}

	if (parent->GetChildAttachment())
	{
		const CEntity* childEntity = SafeCast(const CEntity, parent->GetChildAttachment());
		if(childEntity && childEntity->GetIsPhysical())
		{
			if(RecursiveAreAnyChildrenOwnedByPlayer(SafeCast(const CPhysical, childEntity), player, counter))
				return true;
		}
	}
	if (parent->GetSiblingAttachment())
	{
		const CEntity* childSibling = SafeCast(const CEntity, parent->GetSiblingAttachment());
		if(childSibling && childSibling->GetIsPhysical())
		{
			if(RecursiveAreAnyChildrenOwnedByPlayer(SafeCast(const CPhysical, childSibling), player, counter))
				return true;
		}
	}

	// if child is local than return false right away
	if(!parent->GetNetworkObject() || !parent->GetNetworkObject()->IsClone())
	{
		return false;
	}

	// if child is owned by this player return true
	if(parent->GetNetworkObject()->GetPlayerOwner() == &player)
	{
		return true;
	}

	return false;
}

bool CNetObjPhysical::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	VALIDATE_GAME_OBJECT(GetPhysical());

	if(!CNetObjDynamicEntity::CanPassControl(player, migrationType, resultCode))
	{
		return false;
	}

	// entities attached to another entity physically must migrate with the parent object
	if (IsPhysicallyAttached())
	{
		netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(GetCurrentAttachmentEntity());

		if(networkObject &&
			networkObject->GetPlayerOwner()        != &player &&
			networkObject->GetPendingPlayerIndex() != player.GetPhysicalPlayerIndex())
		{
			if(PreventMigrationWhenPhysicallyAttached(player, migrationType))
			{
				if(resultCode)
				{
					*resultCode = CPC_FAIL_PHYSICAL_ATTACH;
				}
				return false;
			}
		}
	}

	if(migrationType == MIGRATE_PROXIMITY && !RecursiveAreAllChildrenCloned(GetPhysical(), player))
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_ATTACHED_CHILD_NOT_CLONED;
		}
		return false;
	}

	if (IsInCutsceneLocallyOrRemotely())
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_IN_CUTSCENE;
		}
		return false;
	}

	if (m_pendingAttachStateMismatch)
	{
		if(resultCode)
		{
#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ATTACHMENT_STATE_MISMATCH", GetLogName());

			bool	   attached;
			ObjectId   attachedObjectID;
			Vector3    attachmentOffset;
			Quaternion attachmentQuat;
			Vector3	   attachmentParentOffset;
			s16        attachmentOtherBone;
			s16        attachmentMyBone;
			u32        attachmentFlags;
			bool       allowInitialSeparation;
			float      invMassScaleA;
			float      invMassScaleB;
			bool	   activatePhysicsWhenDetached;
			bool       isCargoVehicle;

			GetPhysicalAttachmentData(attached, 
				attachedObjectID, 
				attachmentOffset,
				attachmentQuat,
				attachmentParentOffset,
				attachmentOtherBone,
				attachmentMyBone,
				attachmentFlags,
				allowInitialSeparation,
				invMassScaleA,
				invMassScaleB,
				activatePhysicsWhenDetached,
				isCargoVehicle);

			if (m_pendingAttachmentObjectID != attachedObjectID)
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach entity differs", "Pending: %d, Current: %d", m_pendingAttachmentObjectID, attachedObjectID);
			}
			else if (m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID)
			{
				if (m_pendingMyAttachBone != attachmentMyBone || m_pendingOtherAttachBone != attachmentOtherBone)
				{
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach bones differ", "Pending: %d,%d, Current: %d,%d", m_pendingMyAttachBone, m_pendingOtherAttachBone, attachmentMyBone, attachmentOtherBone);
				}
				else if (m_pendingAttachFlags != attachmentFlags)
				{
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach flags differ", "Pending: %u, Current: %u", m_pendingAttachFlags, attachmentFlags);
				}
				else
				{
					gnetAssertf(0, "m_pendingAttachStateMismatch set on %s but there is no real attachment mismatch - object is attached correctly", GetLogName());
					m_pendingAttachStateMismatch = false;
					return true;
				}
			}
			else
			{
				gnetAssertf(0, "m_pendingAttachStateMismatch set on %s but there is no real attachment mismatch- object is detached correctly", GetLogName());
				m_pendingAttachStateMismatch = false;
				return true;
			}
#endif
			*resultCode = CPC_FAIL_PHYSICAL_ATTACH_STATE_MISMATCH;
		}

		return false;
	}

	if(IsConcealed())
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_CONCEALED;
		}
		return false;
	}

	return true;
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   player            - the new owner
//                  migrationType - how the entity migrated
// Returns      :   void
void CNetObjPhysical::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    bool bWasClone = IsClone();

	// cache attachment data if entity is becoming a clone, so we know what it is supposed to be attached to 
	if (!IsClone() && GetIsAttachedForSync() && IsAttachmentStateSentOverNetwork())
	{
		bool bAttached       = false;
        bool bIsCargoVehicle = false;

		GetPhysicalAttachmentData(bAttached, 
								m_pendingAttachmentObjectID, 
								m_pendingAttachOffset,
								m_pendingAttachQuat,
								m_pendingAttachParentOffset,
								m_pendingOtherAttachBone,
								m_pendingMyAttachBone,
								m_pendingAttachFlags,
                                m_pendingAllowInitialSeparation,
                                m_pendingInvMassScaleA,
                                m_pendingInvMassScaleB,
								m_pendingDetachmentActivatePhysics,
                                bIsCargoVehicle);
       
        // only include actual attachment flags (netobjtrailer adds some extra ones)
        m_pendingAttachFlags &= ((1<<NUM_EXTERNAL_ATTACH_FLAGS)-1);

		gnetAssert(bAttached);      
		gnetAssert(m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID);
	}

	CNetObjDynamicEntity::ChangeOwner(player, migrationType);

	m_pendingAttachStateMismatch = false;

	if (pPhysical)
	{
		if (bWasClone && !IsClone())
		{

#if ENABLE_NETWORK_LOGGING
			// temp code to help track down B* 1803473
			if (GetObjectType() == NET_OBJ_TYPE_PICKUP)
			{
				netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
				log.WriteDataValue("Visible", "%s",  pPhysical->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT ) ? "true" : "false");
				log.WriteDataValue("Visible over network", "%s", IsLocalEntityVisibleOverNetwork() ? "true" : "false");
			}
#endif

            for(unsigned index = 0; index < CNetworkSyncDataULBase::NUM_UPDATE_LEVELS; index++)
            {
                SnapshotData *snapshotData = GetSnapshotData(index);

                if(snapshotData)
                {
                    snapshotData->Reset(VEC3_ZERO, VEC3_ZERO, 0);
                }
            }

			if (PreventMigrationDuringAttachmentStateMismatch() && IsAttachmentStateSentOverNetwork())
			{
				bool	   attached;
				ObjectId   attachedObjectID;
				Vector3    attachmentOffset;
				Quaternion attachmentQuat;
				Vector3	   attachmentParentOffset;
				s16        attachmentOtherBone;
				s16        attachmentMyBone;
				u32        attachmentFlags;
				bool       allowInitialSeparation;
				float      invMassScaleA;
				float      invMassScaleB;
				bool	   activatePhysicsWhenDetached;
				bool       isCargoVehicle;

				GetPhysicalAttachmentData(attached, 
										attachedObjectID, 
										attachmentOffset,
										attachmentQuat,
										attachmentParentOffset,
										attachmentOtherBone,
										attachmentMyBone,
										attachmentFlags,
										allowInitialSeparation,
										invMassScaleA,
										invMassScaleB,
										activatePhysicsWhenDetached,
										isCargoVehicle);

                u32 pendingAttachFlags = m_pendingAttachFlags;

                // only include actual attachment flags for these checks (netobjtrailer adds some extra ones)
                pendingAttachFlags &= ((1<<NUM_EXTERNAL_ATTACH_FLAGS)-1);
                attachmentFlags    &= ((1<<NUM_EXTERNAL_ATTACH_FLAGS)-1);

				if (attachedObjectID != m_pendingAttachmentObjectID || 
					(m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID &&
					(pendingAttachFlags != attachmentFlags ||
					m_pendingMyAttachBone != attachmentMyBone ||	
					m_pendingOtherAttachBone != attachmentOtherBone)))
				{
	#if ENABLE_NETWORK_LOGGING
					netLoggingInterface& netLog = NetworkInterface::GetObjectManagerLog();

					NetworkLogUtils::WriteLogEvent(netLog, "ATTACHMENT_STATE_MISMATCH", GetLogName());

					if (attachedObjectID != m_pendingAttachmentObjectID)
					{
						netLog.WriteDataValue("Attach obj differs", "Curr: %d, Target: %d", attachedObjectID, m_pendingAttachmentObjectID);
					}
					else
					{
						if (m_pendingAttachFlags != attachmentFlags)
						{
							netLog.WriteDataValue("Flags differ", "Curr: %d, Target: %d", attachmentFlags, pendingAttachFlags);
						}

						if (m_pendingMyAttachBone != attachmentMyBone)
						{
							netLog.WriteDataValue("My bone differs", "Curr: %d, Target: %d", attachmentMyBone, m_pendingMyAttachBone);
						}

						if (m_pendingOtherAttachBone != attachmentOtherBone)
						{
							netLog.WriteDataValue("Other bone differs", "Curr: %d, Target: %d", attachmentOtherBone, m_pendingOtherAttachBone);
						}
					}
	#endif // ENABLE_NETWORK_LOGGING

					m_pendingAttachStateMismatch = true;
				}
			}

            m_bAttachedToLocalObject = false;
		}
	
		m_collisionTimer               = 0;
		m_timeSinceLastCollision       = 0;
        m_TimeToForceLowestUpdateLevel = 0;
        m_LowestUpdateLevelToForce     = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
	}

	if(bWasClone && !IsClone() && m_entityConcealed)
	{
		m_entityConcealed = false;
		m_entityConcealedLocally = true;

		ProcessFixedByNetwork(true);
	}
	else if (!bWasClone && m_entityConcealedLocally)
	{
		m_entityConcealed = true;
		m_entityConcealedLocally = false;
	}

	// make sure the cutscene flags are always clear after a migration (the entity could have migrated immediately afterwards) 
	m_bInCutsceneLocally = false;
	m_bInCutsceneRemotely = false;

	m_FrameForceSendRequested =0;
	m_bForceSendPhysicalAttachVelocityNode	 = false;
}

void CNetObjPhysical::PlayerHasLeft(const netPlayer& player)
{
	CPed* pPlayerPed = static_cast<const CNetGamePlayer*>(&player)->GetPlayerPed();

	// if this entity is pending attachment to the leaving player, clear the pending data
	if (pPlayerPed && pPlayerPed->GetNetworkObject() && m_pendingAttachmentObjectID == pPlayerPed->GetNetworkObject()->GetObjectID())
	{
		ClearPendingAttachmentData();
	}

	CNetObjDynamicEntity::PlayerHasLeft(player);
}

void CNetObjPhysical::OnRegistered()
{
	if (NetworkInterface::IsInMPCutscene() && GetPhysical())
	{
		HideForCutscene();
	}

	CNetObjEntity::OnRegistered();
}

bool CNetObjPhysical::OnReassigned()
{
	if (CNetObjDynamicEntity::OnReassigned())
	{
		return true;
	}
	
	// if we have an object reassigned to our machine, and we are not running the script that created it then we need to clean up the script state
	// if this object must only exist on machines running that script (if it has been reassigned to our machine this means that no other machines
	// are running the script)
	if (!IsClone() && IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION))
	{
		scriptHandlerObject* scriptObj = GetScriptHandlerObject();

		if (scriptObj)
		{
			if (!scriptObj->GetScriptHandler() ||
				!scriptObj->GetScriptHandler()->GetNetworkComponent() ||
				scriptObj->GetScriptHandler()->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING)
			{
				ConvertToAmbientObject();
			}
		}
	}

	// if the object has been reassigned and is pending conversion to a script object, send an event to the new owner to do this if we are hosting
	// the script that wishes conversion. The orginal event that was triggered will have been removed when the object went on the reassign list as
	// it would not have a player owner.
	if (IsClone() && m_pendingScriptObjInfo)
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_pendingScriptObjInfo->m_info.GetScriptId());

		if (pHandler && pHandler->GetNetworkComponent() && pHandler->GetNetworkComponent()->GetHost() && pHandler->GetNetworkComponent()->GetHost()->IsLocal())
		{
			CConvertToScriptEntityEvent::Trigger(*this);		
		}
	}

    return false;
}

//
// name:        CleanUpGameObject
// description: Cleans up the pointers between this network object and the
//              game object which owns it
void CNetObjPhysical::CleanUpGameObject(bool bDestroyObject)
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (!pPhysical)
	{
		SetGameObject(0);
		return;
	}

    pPhysical->SetNetworkObject(NULL);
    SetGameObject(0);

    // delete the game object if it is a clone
    if (bDestroyObject && !GetEntityLeftAfterCleanup())
    {
        // need to restore the game heap here in case the ped constructor/destructor allocates from the heap
        sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
        sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

        CGameWorld::Remove(pPhysical);
        delete pPhysical;
        sysMemAllocator::SetCurrent(*oldHeap);
    }
	else
	{
		if(pPhysical->GetNoCollisionFlags() & NO_COLLISION_NETWORK_OBJECTS)
		{
			pPhysical->ResetNoCollision();

			int collisionFlags = pPhysical->GetNoCollisionFlags();
			collisionFlags &= ~NO_COLLISION_NETWORK_OBJECTS;
			pPhysical->SetNoCollisionFlags((u8)collisionFlags); 
		}
	}
}

void CNetObjPhysical::PostSync()
{
    const netSyncDataNode *orientationNode   = static_cast<CProjectSyncTree*>(GetSyncTree())->GetOrientationNode();
    const netSyncDataNode *velocityNode      = static_cast<CProjectSyncTree*>(GetSyncTree())->GetVelocityNode();
    const netSyncDataNode *angVelocityNode   = static_cast<CProjectSyncTree*>(GetSyncTree())->GetAngVelocityNode();

    bool positionUpdated    = static_cast<CProjectSyncTree*>(GetSyncTree())->GetPositionWasUpdated();
    bool orientationUpdated = orientationNode ? orientationNode->GetWasUpdated() : false;
    bool velocityUpdated    = velocityNode    ? velocityNode->GetWasUpdated()    : false;
    bool angVelocityUpdated = angVelocityNode ? angVelocityNode->GetWasUpdated() : false;

#if DR_ENABLED
	CPhysical* pPhysical = GetPhysical();
	if (pPhysical)
	{
		debugPlayback::RecordBlenderState(pPhysical->GetCurrentPhysicsInst(), RCC_MAT34V(m_LastReceivedMatrix), positionUpdated, orientationUpdated, velocityUpdated, angVelocityUpdated);
	}
#endif

    UpdateBlenderState(positionUpdated, orientationUpdated, velocityUpdated, angVelocityUpdated);

    if (IsLocalFlagSet(CNetObjGame::LOCALFLAG_ENTITY_FIXED))
	{
		SetLocalFlag(CNetObjGame::LOCALFLAG_ENTITY_FIXED, false);

		// Check for network blender being overridden. This is need during players teleport due to fading the player out:
		// Setting to go straight to target will move the player and the fade out wont have time to finish.
		if (!NetworkBlenderIsOverridden())
		{
			GetNetBlender()->GoStraightToTarget();
		}
	}
}

void CNetObjPhysical::SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate)
{
	VALIDATE_GAME_OBJECT(GetPhysical());

	if (!IsClone())
	{
		if (m_bAlphaFading)
		{
			SetAlphaFading(false);
		}
		else if (m_bAlphaRamping && isVisible && m_bAlphaRampOut)
		{
			StopAlphaRamping();
		}

		if (m_bHideWhenInvisible && isVisible && CanResetAlpha())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_OUT_FINISHED", GetLogName());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);

			SetHideWhenInvisible(false);
		}
	}

	CNetObjDynamicEntity::SetIsVisible(isVisible, reason, bNetworkUpdate);
}

void CNetObjPhysical::OnUnregistered()
{
	CPhysical* pPhysical = GetPhysical();

	if (pPhysical && m_bHideWhenInvisible && pPhysical->GetNoCollisionEntity() == pPhysical)
	{
		pPhysical->SetNoCollision(0, 0);
	}

	CNetObjDynamicEntity::OnUnregistered();
}

bool CNetObjPhysical::CanApplyNodeData() const
{
	// don't accept updates for entities still in a cutscene
	if (IsInCutsceneLocally())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CANNOT_APPLY_NODE_DATA", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "In cutscene locally");
		return false;
	}

	return CNetObjEntity::CanApplyNodeData();
}

void CNetObjPhysical::OnConversionToAmbientObject()
{
	CPhysical* pPhysical = GetPhysical();

	bool bDontResetDamageFlagsOnCleanupMissionState = pPhysical ? pPhysical->m_bDontResetDamageFlagsOnCleanupMissionState : false;
	bool bIgnoresExplosions							= pPhysical ? pPhysical->m_nPhysicalFlags.bIgnoresExplosions : false;
	bool bOnlyDamagedByPlayer						= pPhysical ? pPhysical->m_nPhysicalFlags.bOnlyDamagedByPlayer : false;
	bool bOnlyDamagedByRelGroup						= pPhysical ? pPhysical->m_nPhysicalFlags.bOnlyDamagedByRelGroup : false;
	bool bNotDamagedByRelGroup						= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByRelGroup : false;
	bool bOnlyDamagedWhenRunningScript				= pPhysical ? pPhysical->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript : false;
	bool bNotDamagedByBullets						= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByBullets : false;
	bool bNotDamagedByFlames						= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByFlames : false;
	bool bNotDamagedByCollisions					= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByCollisions : false;
	bool bNotDamagedByMelee							= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByMelee : false;
	bool bNotDamagedByAnything						= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByAnything : false;
	bool bNotDamagedByAnythingButHasReactions		= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions : false;
	bool bNotDamagedBySteam							= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedBySteam : false;
	bool bNotDamagedBySmoke							= pPhysical ? pPhysical->m_nPhysicalFlags.bNotDamagedBySmoke : false;

	CNetObjDynamicEntity::OnConversionToAmbientObject();

	if(pPhysical && bDontResetDamageFlagsOnCleanupMissionState)
	{
		pPhysical->m_bDontResetDamageFlagsOnCleanupMissionState				= true;
		pPhysical->m_nPhysicalFlags.bIgnoresExplosions						= bIgnoresExplosions;
		pPhysical->m_nPhysicalFlags.bOnlyDamagedByPlayer					= bOnlyDamagedByPlayer;
		pPhysical->m_nPhysicalFlags.bOnlyDamagedByRelGroup					= bOnlyDamagedByRelGroup;
		pPhysical->m_nPhysicalFlags.bNotDamagedByRelGroup					= bNotDamagedByRelGroup;
		pPhysical->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript			= bOnlyDamagedWhenRunningScript;
		pPhysical->m_nPhysicalFlags.bNotDamagedByBullets					= bNotDamagedByBullets;
		pPhysical->m_nPhysicalFlags.bNotDamagedByFlames						= bNotDamagedByFlames;
		pPhysical->m_nPhysicalFlags.bNotDamagedByCollisions					= bNotDamagedByCollisions;
		pPhysical->m_nPhysicalFlags.bNotDamagedByMelee						= bNotDamagedByMelee;
		pPhysical->m_nPhysicalFlags.bNotDamagedByAnything					= bNotDamagedByAnything;
		pPhysical->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions	= bNotDamagedByAnythingButHasReactions;
		pPhysical->m_nPhysicalFlags.bNotDamagedBySteam						= bNotDamagedBySteam;
		pPhysical->m_nPhysicalFlags.bNotDamagedBySmoke						= bNotDamagedBySmoke;
	}
}

void CNetObjPhysical::UpdateBlenderState(bool positionUpdated, bool orientationUpdated,
                                         bool velocityUpdated, bool angVelocityUpdated)
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    if (pPhysical)
    {
        bool bWasFixed                    = pPhysical->IsBaseFlagSet(fwEntity::IS_FIXED);
        bool bWasFixedWaitingForCollision = pPhysical->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION);

        if (positionUpdated || orientationUpdated || velocityUpdated || angVelocityUpdated)
        {
            bool bBlendingOn = false;

            // set target state on blender, which object will be blended to from current position
            if (GetNetBlender())
            {
                if (GetPhysical()->GetIsTypePed())
                {
                    CNetBlenderPed* pPedBlender = static_cast<CNetBlenderPed *>(GetNetBlender());

                    if(positionUpdated)
                    {
                        pPedBlender->UpdatePosition(m_LastReceivedMatrix.d, pPedBlender->GetLastSyncMessageTime());
                    }
                }
                else
                {
                    CNetBlenderPhysical *pPhysicalBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());

                    if(positionUpdated)
                    {
                        pPhysicalBlender->UpdatePosition(m_LastReceivedMatrix.d, pPhysicalBlender->GetLastSyncMessageTime());
                    }

                    if(orientationUpdated)
                    {
                         pPhysicalBlender->UpdateMatrix(m_LastReceivedMatrix, pPhysicalBlender->GetLastSyncMessageTime());
                    }

                    if(velocityUpdated)
                    {
                        pPhysicalBlender->UpdateVelocity(m_LastVelocityReceived, pPhysicalBlender->GetLastSyncMessageTime());
                    }
                }

                bBlendingOn = GetNetBlender()->IsBlendingOn();
            }

            // if not blending, move straight to target
			if (!CanBlend() || !bBlendingOn || ((bWasFixed || bWasFixedWaitingForCollision) && !CanBlendWhenFixed()))
			{
				if(!NetworkBlenderIsOverridden() && !IsAttachedOrDummyAttached() && pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->IsInLevel())
                {
                    if (GetNetBlender())
                    {
                        BANK_ONLY(bool isMoving = false);

					    if (GetPhysical()->GetIsTypePed())
					    {
						    CNetBlenderPed* pPedBlender = static_cast<CNetBlenderPed *>(GetNetBlender());

                            if (positionUpdated)
                            {
                                pPedBlender->UpdatePosition(m_LastReceivedMatrix.d, pPedBlender->GetLastSyncMessageTime());
                            }

                            if (velocityUpdated)
                            {
                                pPedBlender->UpdateVelocity(pPedBlender->GetLastVelocityReceived(), pPedBlender->GetLastSyncMessageTime());
                            }

							if(!ShouldStopPlayerJitter())
							{
								pPedBlender->GoStraightToTarget();
							}

                            BANK_ONLY(isMoving = pPedBlender->GetLastVelocityReceived().Mag2() > 0.1f);
					    }
					    else
					    {
							Vector3 previousPos = GetPosition();

						    CNetBlenderPhysical *pPhysicalBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());
                            if (positionUpdated)
                            {
						        pPhysicalBlender->UpdatePosition(m_LastReceivedMatrix.d, pPhysicalBlender->GetLastSyncMessageTime());
                            }

                            if (velocityUpdated)
                            {
						        pPhysicalBlender->UpdateVelocity(pPhysicalBlender->GetLastVelocityReceived(), pPhysicalBlender->GetLastSyncMessageTime());
                            }

                            if (angVelocityUpdated)
                            {
                                pPhysicalBlender->UpdateAngVelocity(pPhysicalBlender->GetLastAngVelocityReceived(), pPhysicalBlender->GetLastSyncMessageTime());
                            }

                            if (orientationUpdated)
                            {
                                pPhysicalBlender->UpdateMatrix(m_LastReceivedMatrix, pPhysicalBlender->GetLastSyncMessageTime());
                            }
							
                            bool goStraightToTargetThisFrame = true;

                            if (GetPhysical()->IsBaseFlagSet(fwEntity::IS_FIXED) && previousPos.IsClose(m_LastReceivedMatrix.d, 0.1f))
                            {
                                goStraightToTargetThisFrame = false;
                            }

                            if (goStraightToTargetThisFrame)
                            {
                                pPhysicalBlender->GoStraightToTarget();
                            }

                            BANK_ONLY(isMoving = pPhysicalBlender->GetLastVelocityReceived().Mag2() > 0.1f);
					    }

#if __BANK
                        // track moving objects fixed by network popping between updates
                        if(GetPhysical()->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) && isMoving)
                        {
                            NetworkDebug::AddPredictionPop(GetObjectID(), GetObjectType(), true);
                        }
#endif // __BANK
				    }
                }
            }
        }
    }
}

void CNetObjPhysical::TryToPassControlProximity()
{
	// migrate entities that are attached to another entity physically with their parent object
    CPhysical *attachOrDummyAttachParent = GetAttachOrDummyAttachParent();

	if (attachOrDummyAttachParent && attachOrDummyAttachParent->GetNetworkObject())
	{
		CNetObjGame    *networkObject = attachOrDummyAttachParent->GetNetworkObject();
		CNetGamePlayer *parentOwner   = networkObject ? networkObject->GetPlayerOwner() : 0;

		if(networkObject             &&
			networkObject->IsClone() &&
			parentOwner              &&
			GetPendingPlayerIndex() != parentOwner->GetPhysicalPlayerIndex())
		{
			if(parentOwner->CanAcceptMigratingObjects() && CanPassControl(*parentOwner, MIGRATE_FORCED))
			{
				if(NetworkDebug::IsOwnershipChangeAllowed())
				{
					CGiveControlEvent::Trigger(*parentOwner, this, MIGRATE_FORCED);
				}
			}
		}
	}
	else
	{
		CNetObjDynamicEntity::TryToPassControlProximity();
	}
}

void CNetObjPhysical::GetVelocitiesForLogOutput(Vector3 &velocity, Vector3 &angularVelocity) const
{
    if (IsClone() &&
        GetNetBlender() &&
        GetNetBlender()->IsBlendingOn())
    {
        if (GetPhysical())
        {
            if (GetPhysical()->GetIsTypePed())
            {
                CNetBlenderPed* pPedBlender = static_cast<CNetBlenderPed *>(GetNetBlender());
                velocity = pPedBlender->GetLastVelocityReceived();
            }
            else
            {
                CNetBlenderPhysical *pPhysicalBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());
                velocity        = pPhysicalBlender->GetLastVelocityReceived();
                angularVelocity = pPhysicalBlender->GetLastAngVelocityReceived();
            }
        }
    }
    else
    {
        velocity        = GetPhysical()->GetVelocity();
        angularVelocity = GetPhysical()->GetAngVelocity();
    }
}

//
// name:        CNetObjPhysical::ActivateDamageTracking
// description: activates damage tracking on this object
//
void CNetObjPhysical::ActivateDamageTracking()
{
	if(AssertVerify(!IsTrackingDamage()))
	{
		m_damageTracker = rage_new CNetworkDamageTracker(this);

#if !__NO_OUTPUT
		CNetworkDamageTracker::Pool* pool = CNetworkDamageTracker::GetPool();
		if (pool)
			gnetDebug1("Activate Damage Tracking, [%s], Size='%d', FreeSpaces='%d', UsedSpaces='%d'.", GetLogName(), pool->GetSize(), pool->GetNoOfFreeSpaces(), pool->GetNoOfUsedSpaces());
#endif //!__NO_OUTPUT
	}
}

//
// name:        CNetObjPhysical::DeactivateDamageTracking
// description: deactivates damage tracking on this object
//
void CNetObjPhysical::DeactivateDamageTracking()
{
	if(AssertVerify(IsTrackingDamage()))
	{
		delete m_damageTracker;
		m_damageTracker = 0;

#if !__NO_OUTPUT
		CNetworkDamageTracker::Pool* pool = CNetworkDamageTracker::GetPool();
		if (pool)
			gnetDebug1("DeActivate Damage Tracking, [%s] Size='%d', FreeSpaces='%d', UsedSpaces='%d'.", GetLogName(), pool->GetSize(), pool->GetNoOfFreeSpaces(), pool->GetNoOfUsedSpaces());
#endif //!__NO_OUTPUT
	}
}

//
// name:        CNetObjPhysical::IsTrackingDamage
// description: returns whether damage is being tracked on this object
//
bool CNetObjPhysical::IsTrackingDamage()
{
    return m_damageTracker != 0;
}

//
// name:        CNetObjPhysical::UpdateDamageTracker
// description: Updates the damage tracker with damage dealt by a player and/or a network object that has
//               Tracking Damage active.
//
void CNetObjPhysical::UpdateDamageTracker(const NetworkDamageInfo& damageInfo, const bool isResponsibleForCollision)
{
	CPhysical* physical= GetPhysical();
	VALIDATE_GAME_OBJECT(physical);

	if (!physical)
		return;

	const float damageDealt = damageInfo.m_HealthLost + damageInfo.m_ArmourLost;
#if __BANK
	if(damageDealt>1000.0f && damageInfo.m_WeaponHash!=0)
	{
			if( damageInfo.m_WeaponHash != -1568386805   // WEAPON_GRENADELAUNCHER
			&&	damageInfo.m_WeaponHash != -1312131151   // WEAPON_RPG
			&&	damageInfo.m_WeaponHash != 539292904     // WEAPON_EXPLOSION
			&&	damageInfo.m_WeaponHash != -1813897027   // WEAPON_GRENADE
			&&	damageInfo.m_WeaponHash != 1672152130    // WEAPON_HOMINGLAUNCHER
			&&	damageInfo.m_WeaponHash != -1420407917   // WEAPON_PROXMINE
			&&	damageInfo.m_WeaponHash != 741814745     // WEAPON_STICKYBOMB
			&&  damageInfo.m_WeaponHash != -1090665087   // WEAPON_VEHICLE_ROCKET
			&&  damageInfo.m_WeaponHash != 324506233     // WEAPON_AIRSTRIKE_ROCKET
			&&  damageInfo.m_WeaponHash != 1155224728    // VEHICLE_WEAPON_TURRET_INSURGENT
			&&  damageInfo.m_WeaponHash != -821520672    // VEHICLE_WEAPON_PLANE_ROCKET 
			&&  damageInfo.m_WeaponHash != -123497569    // VEHICLE_WEAPON_SPACE_ROCKET 
			&&  damageInfo.m_WeaponHash != -544306709    // WEAPON_FIRE 
			&&  damageInfo.m_WeaponHash != 1407524436    // WEAPONTYPE_SCRIPT_HEALTH_CHANGE 
			)
			{
				const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(damageInfo.m_WeaponHash);
				Warningf("damageDealt with %s is > 1000.0f (%f). Is this possible ?",  (wi && wi->GetName()) ? wi->GetName() : "unknown", damageDealt);
				sysStack::PrintStackTrace();
			}
	}
#endif
	if ((0.0f >= damageDealt && !NetworkInterface::ShouldTriggerDamageEventForZeroDamage(physical)) && !damageInfo.m_KillsVictim && damageInfo.m_WeaponHash != WEAPONTYPE_DLC_TRANQUILIZER && (0.0f >= damageInfo.m_EnduranceLost))
		return;

	//Update Damage tracker's
	CNetObjEntity* netInflictor = damageInfo.m_Inflictor ? static_cast<CNetObjEntity *>(NetworkUtils::GetNetworkObjectFromEntity(damageInfo.m_Inflictor)) : 0;
	if (netInflictor && damageInfo.m_WeaponHash != WEAPONTYPE_SCRIPT_HEALTH_CHANGE)
	{
		m_lastDamageObjectID   = netInflictor->GetObjectID();
		m_lastDamageWeaponHash = damageInfo.m_WeaponHash;
	}

	m_lastDamagedMaterialId = damageInfo.m_HitMaterial;

	//Check if the victim is a player
	bool victimIsPlayer = (GetObjectType() == NET_OBJ_TYPE_PLAYER);
	if (!victimIsPlayer)
	{
		if(physical->GetIsTypeVehicle())
		{
			CVehicle* vehicle = static_cast<CVehicle *>(physical);
			CPed* driver = vehicle->GetDriver();
			victimIsPlayer = (driver && driver->IsPlayer());
		}
	}

	//Check if the damage inflictor is a player
	bool inflictorIsPlayer = false;
	if (netInflictor)
	{
		inflictorIsPlayer = (netInflictor->GetObjectType() == NET_OBJ_TYPE_PLAYER);
		if (!inflictorIsPlayer)
		{
			if(damageInfo.m_Inflictor->GetIsTypeVehicle())
			{
				CVehicle* vehicle = static_cast<CVehicle *>(damageInfo.m_Inflictor);
				CPed* driver = vehicle->GetDriver();
				inflictorIsPlayer = (driver && driver->IsPlayer());
			}
		}
	}

	//Update the AI / Stats system Clone Ped being incapacitated
	if (physical->GetIsTypePed() && damageInfo.m_IncapacitatesVictim && IsClone())
	{
		const u32 weaponHash = (damageInfo.m_WeaponHash != WEAPONTYPE_SCRIPT_HEALTH_CHANGE ? damageInfo.m_WeaponHash : m_lastDamageWeaponHash);
		CPedDamageCalculator::RegisterIncapacitated((CPed*)physical, damageInfo.m_Inflictor, weaponHash, damageInfo.m_HeadShoot, damageInfo.m_WeaponComponent, damageInfo.m_WithMeleeWeapon, 0);
	}

	//Update the AI / Stats system Clone Ped being killed
	if (physical->GetIsTypePed() && damageInfo.m_KillsVictim && IsClone())
	{
		const u32 weaponHash = (damageInfo.m_WeaponHash != WEAPONTYPE_SCRIPT_HEALTH_CHANGE ? damageInfo.m_WeaponHash : m_lastDamageWeaponHash);
		CPedDamageCalculator::RegisterKill((CPed*)physical, damageInfo.m_Inflictor, weaponHash, damageInfo.m_HeadShoot, damageInfo.m_WeaponComponent, damageInfo.m_WithMeleeWeapon, 0);		
	}

	//Network Damage Event
	if ((IsTrackingDamage() || victimIsPlayer || inflictorIsPlayer || damageInfo.m_KillsVictim || NetworkInterface::ShouldTriggerDamageEventForZeroDamage(physical)) && (0 < damageInfo.m_WeaponHash || GetTriggerDamageEventForZeroWeaponHash()))
	{
		if (GetObjectType() == NET_OBJ_TYPE_PLAYER && !IsClone() && SafeCast(CPed, physical)->GetPedType() != PEDTYPE_ANIMAL)
		{
			CNetworkBodyDamageTracker::SetComponent(damageInfo);
		}

#if __BANK
		if (PARAM_netDebugLastDamage.Get() && damageInfo.m_KillsVictim)
		{
			gnetAssert( !m_lastDamageWasSet );
			m_lastDamageWasSet = true;
			
			gnetDebug1(" ////////////////////////////////////////////////////////////////////////// ");
			gnetDebug1(" ////////////////////////////////////////////////////////////////////////// ");
			gnetDebug1(" ////////////////////////////////////////////////////////////////////////// ");

			gnetDebug1(" /////////////////// VICTIM KILLED %s, on frame %d ///////////////////", GetLogName(), fwTimer::GetFrameCount());
			sysStack::PrintStackTrace( );

			gnetDebug1(" ////////////////////////////////////////////////////////////////////////// ");
			gnetDebug1(" ////////////////////////////////////////////////////////////////////////// ");
			gnetDebug1(" ////////////////////////////////////////////////////////////////////////// ");
		}
#endif // __BANK

		//Suppress damage events related to script decreasing the health.
		if (damageInfo.m_WeaponHash != WEAPONTYPE_SCRIPT_HEALTH_CHANGE)
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkEntityDamage(this, damageInfo, isResponsibleForCollision));

			if(damageInfo.m_KillsVictim && physical->GetIsTypePed() && (IsGlobalFlagSet(GLOBALFLAG_SCRIPTOBJECT) || IsGlobalFlagSet(GLOBALFLAG_WAS_SCRIPTOBJECT)))
			{
				ForceHighUpdateLevel();
			}

			if(inflictorIsPlayer && physical->GetIsTypeVehicle())
			{
				u32 uVehicleHash = static_cast<CVehicle *>(physical)->GetModelNameHash();
				CPed* pInflictor = (netInflictor->GetObjectType() == NET_OBJ_TYPE_PLAYER) ? static_cast<CPed *>(damageInfo.m_Inflictor) : static_cast<CVehicle *>(damageInfo.m_Inflictor)->GetDriver();
				if(pInflictor)
				{
					CNewHud::GetReticule().RegisterVehicleHit(*pInflictor, uVehicleHash, damageInfo.m_HealthLost);
				}
			}
		}
		else
		{
			gnetDebug1(" VICTIM '%s' avoid damage event because weapon hash is WEAPONTYPE_SCRIPT_HEALTH_CHANGE.", GetLogName());
		}
	}
#if __ASSERT
	else if (physical->GetIsTypePed() && damageInfo.m_KillsVictim && IsScriptObject())
	{
		static_cast<CNetObjPed*>(this)->CheckDamageEventOnDie();
	}
#endif // __ASSERT

	//Network Damage Tracker
	if(IsTrackingDamage() && netInflictor && 0 < damageDealt)
	{
		netPlayer* player = (netInflictor->GetObjectType() == NET_OBJ_TYPE_PLAYER) ? netInflictor->GetPlayerOwner() : 0;

		if(!player)
		{
			//Check if the Ped belongs to the player group.
			if (damageInfo.m_Inflictor->GetIsTypePed())
			{
				CPed *ped = static_cast<CPed *>(damageInfo.m_Inflictor);
				CPedGroup* pGroup = ped->GetPedsGroup();

				if (pGroup && pGroup->GetGroupMembership()->GetLeader() && pGroup->GetGroupMembership()->GetLeader()->IsPlayer())
				{
					player = pGroup->GetGroupMembership()->GetLeader()->GetNetworkObject()->GetPlayerOwner();
				}
			}
			//Check if the player is driving a vehicle.
			else if(damageInfo.m_Inflictor->GetIsTypeVehicle())
			{
				CVehicle *vehicle = static_cast<CVehicle *>(damageInfo.m_Inflictor);
				CPed *driver = vehicle->GetDriver();

				if(driver && driver->IsPlayer())
				{
					player = driver->GetNetworkObject()->GetPlayerOwner();
				}
			}
		}

		//Don't register if the weapon hash is WEAPONTYPE_SCRIPT_HEALTH_CHANGE.
		if (player && damageInfo.m_WeaponHash != WEAPONTYPE_SCRIPT_HEALTH_CHANGE)
		{
			m_damageTracker->AddDamageDealt(*player, damageDealt);
		}
	}
}

//
// name:        CNetObjPhysical::GetDamageDealtByPlayer
// description: returns the damage dealt by the specified player on this object
//
float CNetObjPhysical::GetDamageDealtByPlayer(const netPlayer& player)
{
    float damageDealt = 0.0f;

    if(AssertVerify(IsTrackingDamage()))
    {
        damageDealt = m_damageTracker->GetDamageDealt(player);
    }

    return damageDealt;
}

//
// name:        CNetObjPhysical::ResetDamageDealtByAllPlayers
// description: resets the damage dealt.
//
void CNetObjPhysical::ResetDamageDealtByAllPlayers()
{
	if(IsTrackingDamage())
	{
		m_damageTracker->ResetAll();
	}
}

bool CNetObjPhysical::IsAttachedOrDummyAttached() const
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    if(pPhysical)
    {
        return pPhysical->GetIsAttached();
    }

    return false;
}

CPhysical *CNetObjPhysical::GetAttachOrDummyAttachParent() const
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    if(pPhysical && pPhysical->GetIsAttached())
    {
        return SafeCast(CPhysical, pPhysical->GetAttachParent());
    }

    return 0;
}

void CNetObjPhysical::ConvertToAmbientObject()
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    if (pPhysical)
    {
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CONVERT_TO_AMBIENT", "%s cleaned up", GetLogName());

        // restore game heap ready for deallocation of game heap mem when the object's mission state is cleaned up
        sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
        sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

        scriptHandler* pScriptHandler = GetScriptHandlerObject() ? GetScriptHandlerObject()->GetScriptHandler() : NULL;

        if (pScriptHandler)
        {
			CScriptEntityExtension* pExtension = pPhysical->GetExtension<CScriptEntityExtension>();
			
			if (pExtension)
				pExtension->SetNoLongerNeeded(true);

            pScriptHandler->UnregisterScriptObject(*GetScriptHandlerObject());
        }
        else
        {
			pPhysical->CleanupMissionState();
			pPhysical->DestroyScriptExtension();
       }

        gnetAssert(FindScriptExtension() == 0);

        sysMemAllocator::SetCurrent(*oldHeap);
    }
}

void CNetObjPhysical::ConcealEntity(bool conceal, bool allowDamagingWhileConcealed)  
{
	if(conceal)
		m_allowDamagingWhileConcealed = allowDamagingWhileConcealed;
	else
		m_allowDamagingWhileConcealed = false;

	CPhysical* physical = GetPhysical();
	if(IsPendingOwnerChange() && conceal)
	{
		gnetDebug2("CNetObjPhysical::ConcealEntity %s Trying to conceal migrating entity", GetLogName());
		m_entityPendingConcealment = true;
	}
	else if (IsClone())
	{
		if (m_entityConcealed != conceal)
		{
			m_entityConcealed = conceal; 

			if (m_entityConcealed)
			{
				// turn off collision and visibility
				SetOverridingLocalVisibility(false, "Hidden by conceal entity");
				SetOverridingLocalCollision(false, "Hidden by conceal entity");				
				if (physical)
				{
					gnetDebug2("CNetObjPhysical::ConcealEntity %s Disabling collision", GetLogName());
					physical->DisableCollision(NULL, true);
				}
			}
			else
			{
				UpdateLocalVisibility();
				SetOverridingLocalCollision(true, "Entity unconcealed");
				if (physical)
				{
					gnetDebug2("CNetObjPhysical::ConcealEntity %s Enabling collision", GetLogName());
					physical->EnableCollision();
				}
				if(GetNetBlender())
				{
					GetNetBlender()->GoStraightToTarget();
				}
			}

			ProcessFixedByNetwork(true);
		}		
	}
	else if (m_entityConcealedLocally != conceal)
	{
		m_entityConcealedLocally = conceal;

		if (m_entityConcealedLocally)
		{
			// turn off collision and visibility
			SetOverridingLocalVisibility(false, "Entity concealed", true, true);
			SetOverridingLocalCollision(false, "Entity concealed");
			if (physical)
			{
				gnetDebug2("CNetObjPhysical::ConcealEntity %s Disabling collision", GetLogName());
				physical->DisableCollision(NULL, true);
			}
		}
		else
		{
			// turn collision and visibility back on
			SetOverridingLocalVisibility(true, "Entity unconcealed", true, true);
			SetOverridingLocalCollision(true, "Entity unconcealed");
			if (physical)
			{
				gnetDebug2("CNetObjPhysical::ConcealEntity %s Enabling collision", GetLogName());
				physical->EnableCollision();
			}
		}
		
		ProcessFixedByNetwork(true);
	}

	if(IsConcealed())
	{
		HideUnderMap();
	}
}

bool CNetObjPhysical::ShouldObjectBeHiddenForCutscene() const
{
	return NetworkInterface::IsInMPCutscene() && !IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE);
}


// name:		CNetObjEntity::HideForCutscene
// description: hides an ambient entity for a cutscene 
//
void CNetObjPhysical::HideForCutscene()
{
	CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE))
	{
		if (IsClone())
		{
			// make collideable on our machine
			SetOverridingLocalCollision(true, "Hidden for cutscene");
		}
		else if (!IsPendingOwnerChange() && !GetRemotelyVisibleInCutscene())
		{
			// make uncollideable on remote machines
			SetOverridingRemoteCollision(true, false, "Hidden for cutscene");
		}
	}
	else if (pPhysical->GetIsAddedToWorld())
	{
		// make uncollideable on our machine
		SetOverridingLocalCollision(false, "Hidden for cutscene");
		
		ProcessFixedByNetwork(true);
	}

	CNetObjEntity::HideForCutscene();

	if (GetRemotelyVisibleInCutscene() && !IsClone() && !IsPendingOwnerChange() && !IsLocalEntityCollidableOverNetwork())
	{
		SetOverridingRemoteCollision(true, true, "Hidden remotely during cutscene");
	}
}

void CNetObjPhysical::ExposeAfterCutscene()
{
	if (IsConcealed())
	{
		// Don't try to expose after cutscene if we are concealed!
		return;
	}

 	if (GetOverridingRemoteCollision() && !IsClone() && !IsPendingOwnerChange())
	{
		SetOverridingRemoteCollision(false, true, "Expose remotely after cutscene");
	}
	
	// stop overriding collision immediately as the physics is about to be activated
	ClearOverridingLocalCollisionImmediately("Expose after cutscene");

	ProcessFixedByNetwork(true);

	CNetObjEntity::ExposeAfterCutscene();
}

bool CNetObjPhysical::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	if(IsConcealed())
	{
		if(resultCode)
		{
			*resultCode = CAC_FAIL_CONCEALED;
		}

		return false;
	}

	return CNetObjDynamicEntity::CanAcceptControl(player, migrationType, resultCode);
}

bool CNetObjPhysical::ShouldObjectBeHiddenForTutorial() const
{
	// if we are in a tutorial session and this object is marked to be passed over, hide it
	if ((NetworkInterface::IsInTutorialSession() && m_bPassControlInTutorial) || IsLocalFlagSet(CNetObjGame::LOCALFLAG_REMOVE_POST_TUTORIAL_CHANGE))
		return true; 

	return false;
}

void CNetObjPhysical::HideForTutorial()
{
	// turn off collision and visibility
	SetOverridingLocalVisibility(false, "Hidden for tutorial");
	SetOverridingLocalCollision(false, "Hidden for tutorial");
}

void CNetObjPhysical::SetHideWhenInvisible(bool bSet)
{
	CPhysical* pPhysical = GetPhysical();

	m_bHideWhenInvisible = bSet;

	if (pPhysical)
	{
		if (m_bHideWhenInvisible)
		{
			pPhysical->SetNoCollision(pPhysical, NO_COLLISION_NETWORK_OBJECTS);
		}
		else
		{
			CNetObjEntity::SetIsVisible(IsClone()?m_LastReceivedVisibilityFlag:true, "SetHideWhenInvisible");

			if(pPhysical->GetNoCollisionEntity() == pPhysical)
			{
				pPhysical->SetNoCollision(0, 0);
			}
		}
	}
}

bool CNetObjPhysical::ShowGhostAlpha()
{
	CPhysical* pPhysical = GetPhysical();

	if (pPhysical && !IsAlphaRamping() && !IsAlphaFading())
	{
		SetAlpha(ms_ghostAlpha);

		DisableCameraCollisionsThisFrame();

		return true;
	}

	return false;
}

void CNetObjPhysical::ProcessGhosting()
{
	CPhysical* pPhysical = GetPhysical();

	if (IsGhost())
	{
		if (CanShowGhostAlpha())
		{
			ShowGhostAlpha();
		}
		else if (GetInvertGhosting() && CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(*pPhysical))
		{
			DisableCameraCollisionsThisFrame();
		}
	}
	else
	{
		// show the entity as ghosted if it will be treated as a ghost collision due to the local player being a ghost
		if (CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(*pPhysical))
		{
			if (CanShowGhostAlpha())
			{
				ShowGhostAlpha();
			}
			else if (GetInvertGhosting())
			{
				DisableCameraCollisionsThisFrame();
			}
		}		
	}
}


void CNetObjPhysical::EnableCameraCollisions()
{
	if (m_CameraCollisionDisabled)
	{
		CPhysical* pPhysical = GetPhysical();

		if (pPhysical)
		{
			phInst* pPhysicsInst = pPhysical->GetCurrentPhysicsInst();

			if (pPhysicsInst)
			{
				u16 levelIndex = pPhysicsInst->GetLevelIndex();
				phLevelNew* pPhysicsLevel = CPhysics::GetLevel();

				if (pPhysicsInst && pPhysicsLevel && pPhysicsLevel->IsInLevel(levelIndex))
				{
					u32 includeFlags = pPhysicsLevel->GetInstanceIncludeFlags(levelIndex);
					includeFlags |= ArchetypeFlags::GTA_CAMERA_TEST;
					CPhysics::GetLevel()->SetInstanceIncludeFlags(levelIndex, includeFlags);
				}
			}
		}

		m_CameraCollisionDisabled = false;
	}
}

void CNetObjPhysical::DisableCameraCollisionsThisFrame()
{
	if (!m_CameraCollisionDisabled)
	{
		CPhysical* pPhysical = GetPhysical();

		if (pPhysical)
		{
			phInst* pPhysicsInst = pPhysical->GetCurrentPhysicsInst();

			if (pPhysicsInst)
			{
				u16 levelIndex = pPhysicsInst->GetLevelIndex();
				phLevelNew* pPhysicsLevel = CPhysics::GetLevel();

				if (pPhysicsLevel && pPhysicsLevel->IsInLevel(levelIndex))
				{
					u32 includeFlags = pPhysicsLevel->GetInstanceIncludeFlags(levelIndex);
					if(includeFlags & ArchetypeFlags::GTA_CAMERA_TEST)
					{
						includeFlags &= ~ArchetypeFlags::GTA_CAMERA_TEST;
						CPhysics::GetLevel()->SetInstanceIncludeFlags(levelIndex, includeFlags);
	
						m_CameraCollisionDisabled = true;
					}
				}
			}
		}
	}
}


void CNetObjPhysical::SetPendingScriptObjInfo(CGameScriptObjInfo& objInfo)
{
	if (m_pendingScriptObjInfo)
	{
		gnetAssertf(m_pendingScriptObjInfo->m_info == objInfo, "%s: Setting pending script info (%s) when a different one is already set (%s)", GetLogName(), objInfo.GetScriptId().GetLogName(), m_pendingScriptObjInfo->m_info.GetScriptId().GetLogName());
	}
	else
	{
		if (CPendingScriptObjInfo::GetPool()->GetNoOfFreeSpaces() > 0)
		{
			m_pendingScriptObjInfo = rage_new CPendingScriptObjInfo(objInfo);
		}
		else
		{
			gnetAssertf(0, "Ran out of CPendingScriptObjInfos");
		}
	}
}

void CNetObjPhysical::ClearPendingScriptObjInfo()
{
	if (AssertVerify(m_pendingScriptObjInfo))
	{
		delete m_pendingScriptObjInfo;
		m_pendingScriptObjInfo = 0;
	}
}

void CNetObjPhysical::ApplyPendingScriptObjInfo()
{
	if (gnetVerify(m_pendingScriptObjInfo) && 
		gnetVerify(!IsClone()) && 
		gnetVerify(!IsPendingOwnerChange()) &&
		gnetVerify(GetEntity()))
	{
		SetScriptObjInfo(&m_pendingScriptObjInfo->m_info);

		static_cast<CPhysical*>(GetEntity())->SetupMissionState();

		// host objects should always be cloned on all machines running the script
		SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT, true);

		ClearPendingScriptObjInfo();
	}
}

void CNetObjPhysical::ReapplyLastVelocityUpdate()
{
    CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());

    if(netBlenderPhysical)
    {
        netBlenderPhysical->UpdateVelocity(m_LastVelocityReceived, netBlenderPhysical->GetLastSyncMessageTime());
        netBlenderPhysical->GoStraightToTarget();
    }
}

void CNetObjPhysical::HideUnderMap()
{
	CPhysical* physical = GetPhysical();

	if(gnetVerify(physical) && !physical->GetIsAttached()) 
	{
		SetOverridingLocalVisibility(false, "Hidden by conceal entity");
		SetOverridingLocalCollision(false, "Hidden by conceal entity");	
		
		const float DISTANCE_DELTA = 0.5f;
		if(!m_hiddenUnderMap || (VEC3V_TO_VECTOR3(physical->GetTransform().GetPosition()) - m_posBeforeHide).XYMag2() > DISTANCE_DELTA)
		{
			Vector3 currentPos = VEC3V_TO_VECTOR3(physical->GetTransform().GetPosition());
			if(IsClose(currentPos.z, -180.0f, 0.1f))
			{
				currentPos.z = m_posBeforeHide.z;
			}
			m_posBeforeHide = currentPos;
			gnetDebug2("CNetObjPhysical::ConcealEntity %s Caching posBeforeHide: x:%.2f, y:%.2f, z:%.2f", GetLogName(), m_posBeforeHide.GetX(), m_posBeforeHide.GetY(), m_posBeforeHide.GetZ());
		}
		m_hiddenUnderMap = true;

		if(IsClone())
		{
			NetworkInterface::OverrideNetworkBlenderForTime(physical, 1);
		}
		// some aspects of game interact with invisible objects so park under the map
		float HEIGHT_TO_HIDE_WHEN_CONCEALED = -180.0f;
		Vector3 vPosition = IsClone()
								? NetworkInterface::GetLastPosReceivedOverNetwork(physical)
								:  VEC3V_TO_VECTOR3(physical->GetTransform().GetPosition());
		vPosition.z = HEIGHT_TO_HIDE_WHEN_CONCEALED;
		physical->SetPosition(vPosition, true, true, true);
	}
}

// sync parser migration functions
void CNetObjPhysical::GetPhysicalMigrationData(CPhysicalMigrationDataNode& data)
{
    CPhysical* pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

    data.m_isDead = (pPhysical && pPhysical->GetHealth() <= 0.0f);
}

void CNetObjPhysical::GetPhysicalScriptMigrationData(CPhysicalScriptMigrationDataNode& data)
{
	data.m_HasData = false;

	if (GetScriptObjInfo())
    {
		data.m_HasData = true;

        scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(GetScriptObjInfo()->GetScriptId());

		if (pHandler && pHandler->GetNetworkComponent() && pHandler->GetNetworkComponent()->GetState() <= scriptHandlerNetComponent::NETSCRIPT_PLAYING)
        {
            data.m_ScriptParticipants = static_cast<CGameScriptHandlerNetComponent*>(pHandler->GetNetworkComponent())->GetParticipantFlags();
            data.m_HostToken = static_cast<CGameScriptHandlerNetComponent*>(pHandler->GetNetworkComponent())->GetHostToken();
        }
        else
        {
            const CGameScriptHandlerMgr::CRemoteScriptInfo* pInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptObjInfo()->GetScriptId(), false);

            if (gnetVerifyf(pInfo, "Script entity %s has been orphaned and not cleaned up when the script that created it terminated", GetLogName()))
            {
                data.m_ScriptParticipants = pInfo->GetActiveParticipants();
                data.m_HostToken = pInfo->GetHostToken();
            }
        }
    }
}

void CNetObjPhysical::SetPhysicalMigrationData(const CPhysicalMigrationDataNode& data)
{
    CPhysical* pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

    if (pPhysical && data.m_isDead && pPhysical->GetHealth() > 0.0f)
    {
        pPhysical->SetHealth(0.0f, true);
    }
}

void CNetObjPhysical::SetPhysicalScriptMigrationData(const CPhysicalScriptMigrationDataNode& data)
{
    if (data.m_HasData && gnetVerify(GetScriptObjInfo()))
    {
        scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(GetScriptObjInfo()->GetScriptId());

        if (!pHandler)
        {
            if (!CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptObjInfo()->GetScriptId(), false))
            {
                // add a new script info for the script that created this object, we aren't aware of it yet
                CGameScriptHandlerMgr::CRemoteScriptInfo newInfo(GetScriptObjInfo()->GetScriptId(), data.m_ScriptParticipants, data.m_HostToken, GetPlayerOwner(), 0, 0, 0);
                CTheScripts::GetScriptHandlerMgr().AddRemoteScriptInfo(newInfo, GetPlayerOwner());
            }
        }
    }
}

void CNetObjPhysical::GetPhysicalGameStateData(CPhysicalGameStateDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		data.m_isVisible      = IsLocalEntityVisibleOverNetwork();
		data.m_renderScorched = pPhysical->m_nPhysicalFlags.bRenderScorched;
		data.m_isInWater      = pPhysical->GetIsInWater();
		data.m_alteringAlpha  = m_bNetworkAlpha;
		data.m_customFadeDuration = m_iCustomFadeDuration;

		if (m_bNetworkAlpha)
		{
			if (m_bAlphaFading)
			{
				data.m_alphaType = IPhysicalNodeDataAccessor::ART_FADE_OUT;
			}
			else if (m_bAlphaRamping)
			{
				if (m_bAlphaRampOut)
				{
					data.m_alphaType = IPhysicalNodeDataAccessor::ART_RAMP_OUT;
				}
				else if (m_bAlphaRampTimerQuit)
				{
					data.m_alphaType = IPhysicalNodeDataAccessor::ART_RAMP_IN_AND_QUIT;
				}
				else
				{
					data.m_alphaType = IPhysicalNodeDataAccessor::ART_RAMP_IN_AND_WAIT;
				}
			}
			else if(m_bAlphaIncreasingNoFlash)
			{
				data.m_alphaType = IPhysicalNodeDataAccessor::ART_FADE_IN;
			}
			else
			{
				data.m_alphaType = IPhysicalNodeDataAccessor::ART_NONE;
			}
		}
		data.m_allowCloningWhileInTutorial = ShouldAllowCloneWhileInTutorial();
	}
	else
	{
		data.m_renderScorched = data.m_isInWater = data.m_alteringAlpha = false;
	}
}

void CNetObjPhysical::GetPhysicalScriptGameStateData(CPhysicalScriptGameStateDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		data.m_PhysicalFlags.notDamagedByAnything			= pPhysical->m_nPhysicalFlags.bNotDamagedByAnything;
		data.m_PhysicalFlags.dontLoadCollision				= pPhysical->m_nPhysicalFlags.bDontLoadCollision;
        data.m_PhysicalFlags.allowFreezeIfNoCollision		= pPhysical->m_nPhysicalFlags.bAllowFreezeIfNoCollision;
		data.m_PhysicalFlags.damagedOnlyByPlayer			= pPhysical->m_nPhysicalFlags.bOnlyDamagedByPlayer;
		data.m_PhysicalFlags.notDamagedByBullets			= pPhysical->m_nPhysicalFlags.bNotDamagedByBullets;
		data.m_PhysicalFlags.notDamagedByFlames				= pPhysical->m_nPhysicalFlags.bNotDamagedByFlames;
		data.m_PhysicalFlags.ignoresExplosions				= pPhysical->m_nPhysicalFlags.bIgnoresExplosions;
		data.m_PhysicalFlags.notDamagedByCollisions			= pPhysical->m_nPhysicalFlags.bNotDamagedByCollisions;
		data.m_PhysicalFlags.notDamagedByMelee				= pPhysical->m_nPhysicalFlags.bNotDamagedByMelee;
		data.m_PhysicalFlags.notDamagedByRelGroup			= pPhysical->m_nPhysicalFlags.bNotDamagedByRelGroup;
		data.m_PhysicalFlags.onlyDamageByRelGroup			= pPhysical->m_nPhysicalFlags.bOnlyDamagedByRelGroup;
		data.m_PhysicalFlags.notDamagedBySteam              = pPhysical->m_nPhysicalFlags.bNotDamagedBySteam;
        data.m_PhysicalFlags.notDamagedBySmoke              = pPhysical->m_nPhysicalFlags.bNotDamagedBySmoke;
		data.m_PhysicalFlags.onlyDamagedWhenRunningScript	= pPhysical->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript;
		data.m_PhysicalFlags.dontResetDamageFlagsOnCleanupMissionState	= pPhysical->m_bDontResetDamageFlagsOnCleanupMissionState;
		data.m_PhysicalFlags.noReassign						= IsLocalFlagSet(LOCALFLAG_NOREASSIGN);
		data.m_PhysicalFlags.passControlInTutorial			= m_bPassControlInTutorial;
		data.m_PhysicalFlags.inCutscene						= IsInCutsceneLocally();
		data.m_PhysicalFlags.ghostedForGhostPlayers			= m_bGhostedForGhostPlayers;
		data.m_PhysicalFlags.pickUpByCargobobDisabled		= pPhysical->m_nPhysicalFlags.bPickUpByCargobobDisabled;

		if(data.m_PhysicalFlags.inCutscene)
		{
			CutSceneManager* pManager = CutSceneManager::GetInstance();
			if (pManager)
			{
				CCutsceneAnimatedModelEntity* pCSAModelEntity = pManager->GetAnimatedModelEntityFromEntity(pPhysical);

				if( gnetVerifyf(pCSAModelEntity,"%s cut scene entity not found",GetLogName()) )
				{
					data.m_PhysicalFlags.notDamagedByAnything = pCSAModelEntity->GetWasInvincible();
				}
			}
		}

		if(data.m_PhysicalFlags.notDamagedByRelGroup || data.m_PhysicalFlags.onlyDamageByRelGroup)
		{
			data.m_RelGroupHash = pPhysical->m_specificRelGroupHash;
		}
		else
		{
			data.m_RelGroupHash = 0;
		}

        data.m_AlwaysClonedForPlayer = GetAlwaysClonedForPlayerFlags();
        
        if(pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->GetArchetype())
        {
            const float MAX_SPEED_EPSILON = 0.1f; // larger epsilon value to take quantisation from network serialisation into account
            gnetAssertf(pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed() <= CPhysicalScriptGameStateDataNode::MAX_MAX_SPEED, "Syncing invalid max speed for %s! (%.2f)", GetLogName(), pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed());
            data.m_MaxSpeed    = rage::Min(pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed(), CPhysicalScriptGameStateDataNode::MAX_MAX_SPEED);
            data.m_HasMaxSpeed = !IsClose(data.m_MaxSpeed, DEFAULT_MAX_SPEED, MAX_SPEED_EPSILON);
        }
        else
        {
            data.m_HasMaxSpeed = false;
        }

		data.m_AllowMigrateToSpectator = m_AllowMigrateToSpectator;
	}
	else
	{
		data.m_PhysicalFlags.notDamagedByAnything			= false;
		data.m_PhysicalFlags.dontLoadCollision				= false;
        data.m_PhysicalFlags.allowFreezeIfNoCollision		= false;
		data.m_PhysicalFlags.damagedOnlyByPlayer			= false;
		data.m_PhysicalFlags.notDamagedByBullets			= false;
		data.m_PhysicalFlags.notDamagedByFlames				= false;
		data.m_PhysicalFlags.ignoresExplosions				= false;
		data.m_PhysicalFlags.notDamagedByCollisions			= false;
		data.m_PhysicalFlags.notDamagedByMelee				= false;
		data.m_PhysicalFlags.notDamagedByRelGroup			= false;
		data.m_PhysicalFlags.onlyDamageByRelGroup			= false;
		data.m_PhysicalFlags.notDamagedBySteam				= false;
		data.m_PhysicalFlags.notDamagedBySmoke				= false;
		data.m_PhysicalFlags.onlyDamagedWhenRunningScript	= false;	
		data.m_PhysicalFlags.dontResetDamageFlagsOnCleanupMissionState = false;
		data.m_PhysicalFlags.noReassign						= false;	
		data.m_PhysicalFlags.passControlInTutorial			= false;
		data.m_PhysicalFlags.inCutscene						= IsInCutsceneLocally();
		data.m_PhysicalFlags.ghostedForGhostPlayers			= m_bGhostedForGhostPlayers;
		data.m_PhysicalFlags.pickUpByCargobobDisabled		= false;
		data.m_RelGroupHash									= 0;
        data.m_AlwaysClonedForPlayer						= 0;
        data.m_HasMaxSpeed                                  = false;

		data.m_AllowMigrateToSpectator						= false;
	}
}

void CNetObjPhysical::GetVelocity(s32 &packedVelocityX, s32 &packedVelocityY, s32 &packedVelocityZ)
{
    CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		Vector3 velocity = pPhysical->GetVelocity();

		// cap velocity if it is a silly amount (happens now and then)
		if (velocity.Mag() > (1<<(SIZEOF_VELOCITY-SIZEOF_VELOCITY_POINT-1)))
		{
			if (GetObjectType() != NET_OBJ_TYPE_OBJECT)
			{
				NetworkInterface::GetMessageLog().Log("%s has a velocity far too high! (%f, %f, %f)\r\n", GetLogName(), velocity.x, velocity.y, velocity.z);
			}

			velocity *= ((1<<(SIZEOF_VELOCITY-SIZEOF_VELOCITY_POINT-1))-1) / velocity.Mag();
		}

		// pack velocity
		packedVelocityX = (s32) netSerialiserUtils::PackFixedPoint16(velocity.x, SIZEOF_VELOCITY_POINT);
		packedVelocityY = (s32) netSerialiserUtils::PackFixedPoint16(velocity.y, SIZEOF_VELOCITY_POINT);
		packedVelocityZ = (s32) netSerialiserUtils::PackFixedPoint16(velocity.z, SIZEOF_VELOCITY_POINT);
	}
	else
	{
		packedVelocityX = packedVelocityY = packedVelocityZ = 0;
	}
}

void CNetObjPhysical::GetAngularVelocity(CPhysicalAngVelocityDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		Vector3 angularVelocity = pPhysical->GetAngVelocity();

		// cap velocity if it is a silly amount (happens now and then)
		if (angularVelocity.Mag() > (1<<(SIZEOF_ANGVELOCITY-SIZEOF_ANGVELOCITY_POINT-1)))
		{
			angularVelocity *= (1<<(SIZEOF_ANGVELOCITY-SIZEOF_ANGVELOCITY_POINT-1)) / angularVelocity.Mag();
		}

		// pack velocity
		data.m_packedAngVelocityX = netSerialiserUtils::PackFixedPoint16(angularVelocity.x, SIZEOF_ANGVELOCITY_POINT);
		data.m_packedAngVelocityY = netSerialiserUtils::PackFixedPoint16(angularVelocity.y, SIZEOF_ANGVELOCITY_POINT);
		data.m_packedAngVelocityZ = netSerialiserUtils::PackFixedPoint16(angularVelocity.z, SIZEOF_ANGVELOCITY_POINT);

        static const s32 MAX_PACKED_VALUE = ((1<<(SIZEOF_ANGVELOCITY-1))-1);
        data.m_packedAngVelocityX = Clamp<s32>(data.m_packedAngVelocityX, -MAX_PACKED_VALUE, MAX_PACKED_VALUE);
        data.m_packedAngVelocityY = Clamp<s32>(data.m_packedAngVelocityY, -MAX_PACKED_VALUE, MAX_PACKED_VALUE);
        data.m_packedAngVelocityZ = Clamp<s32>(data.m_packedAngVelocityZ, -MAX_PACKED_VALUE, MAX_PACKED_VALUE);
	}
	else
	{
		data.m_packedAngVelocityX = data.m_packedAngVelocityY = data.m_packedAngVelocityZ = 0;
	}
}

void CNetObjPhysical::GetPhysicalHealthData(CPhysicalHealthDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		// calculate the packed health value
		if (pPhysical->GetHealth() < 0.0f)
		{
			data.m_health = 0;
		}
		else if (pPhysical->GetHealth() > 0.0f && pPhysical->GetHealth() < 1.0f)
		{
			// object is just alive, so its health must be above 0
			data.m_health = 1;
		}
		else
		{
			data.m_health = (s32) pPhysical->GetHealth();

			// round it up if necessary
			if (pPhysical->GetHealth() - (float) data.m_health >= 0.5f)
				data.m_health++;
		}

		data.m_hasMaxHealth = (data.m_health == pPhysical->GetMaxHealth());

		data.m_maxHealthSetByScript = m_MaxHealthSetByScript;
		
		if (data.m_maxHealthSetByScript)
		{
			data.m_scriptMaxHealth = (u32)pPhysical->GetMaxHealth();
		}
		else
		{
			data.m_scriptMaxHealth = 0;
		}

		// calculate the weapon damage entity ID
		data.m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
		data.m_weaponDamageHash = 0;

		if (!data.m_hasMaxHealth)
		{
			netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pPhysical->GetWeaponDamageEntity());
			if (networkObject)
			{
				data.m_weaponDamageEntity = networkObject->GetObjectID();
			}

			data.m_weaponDamageHash = pPhysical->GetWeaponDamageHash();
		}
	}
	else
	{
		data.m_hasMaxHealth = false;
		data.m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
		data.m_weaponDamageHash = 0;
		data.m_health = 0;
	}

	data.m_lastDamagedMaterialId = (u64)GetLastDamagedMaterialId();
}

void CNetObjPhysical::GetPhysicalAttachmentData(bool       &attached,
                                                ObjectId   &attachedObjectID,
                                                Vector3    &attachmentOffset,
                                                Quaternion &attachmentQuat,
												Vector3	   &attachmentParentOffset,
                                                s16        &attachmentOtherBone,
                                                s16        &attachmentMyBone,
                                                u32        &attachmentFlags,
                                                bool       &allowInitialSeparation,
                                                float      &invMassScaleA,
                                                float      &invMassScaleB,
												bool	   &activatePhysicsWhenDetached,
                                                bool       &UNUSED_PARAM(isCargoVehicle)) const
{
    CPhysical *pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

	attached = false;

	if(pPhysical && GetIsAttachedForSync() && !m_pendingAttachStateMismatch)
	{
		fwAttachmentEntityExtension *extension = GetAttachmentExtension();
		gnetAssert(extension);

		if (AssertVerify(extension))
		{
			netObject *pAttachEntity = NetworkUtils::GetNetworkObjectFromEntity((CPhysical *) extension->GetAttachParentForced());

			if (AssertVerify(pAttachEntity))
			{
				attached				= true;
				attachedObjectID		= pAttachEntity->GetObjectID();
				attachmentOffset		= extension->GetAttachOffset();
				attachmentQuat			= extension->GetAttachQuat();
				attachmentParentOffset	= extension->GetAttachParentOffset();
				attachmentOtherBone		= extension->GetAttachBone();
				attachmentMyBone		= extension->GetMyAttachBone();
				attachmentFlags			= extension->GetExternalAttachFlags();
				allowInitialSeparation  = true;
				invMassScaleA           = 1.0f;
				invMassScaleB           = 1.0f;

				if(extension->GetAttachState() == ATTACH_STATE_PHYSICAL)
				{
					int numConstraints = extension->GetNumConstraintHandles();

					if(numConstraints > 0)
					{
						gnetAssertf(numConstraints == 1, "Only physically attached entities with a single constraint are supported in network games currently!");
						phConstraintHandle constraintHandle = extension->GetConstraintHandle(0);

						phConstraintAttachment *constraint = (phConstraintAttachment *)PHCONSTRAINT->GetTemporaryPointer(constraintHandle);

						if(gnetVerifyf(constraint, "Failed to find constraint!"))
						{
							allowInitialSeparation = constraint->GetMaxSeparation() != 0.0f;
							invMassScaleA          = constraint->GetMassInvScaleA();
							invMassScaleB          = constraint->GetMassInvScaleB();
						}
					}
				}

				gnetAssert(attachedObjectID != NETWORK_INVALID_OBJECT_ID);
				gnetAssertf((attachmentFlags < (1<<NETWORK_ATTACH_FLAGS_START)), "Physical attachment flags have one the network flags set. Has someone added a new physical attachment flag?");
			}
		}
	}
	else
	{
		if (m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID)
		{
			// send pending attachment data if the entity is not attached yet (this can happen because the entity is attached after ChangeOwner,
			// and the sync data is initialised within ChangeOwner).
			attached				= true;
			attachedObjectID		= m_pendingAttachmentObjectID;
			attachmentOffset		= m_pendingAttachOffset;
			attachmentQuat			= m_pendingAttachQuat;
			attachmentParentOffset	= m_pendingAttachParentOffset;
			attachmentOtherBone		= m_pendingOtherAttachBone;
			attachmentMyBone		= m_pendingMyAttachBone;
			attachmentFlags			= m_pendingAttachFlags;
			invMassScaleA			= m_pendingInvMassScaleA;
			invMassScaleB			= m_pendingInvMassScaleB;
		}
		else if (pPhysical)
		{
			activatePhysicsWhenDetached = pPhysical->GetCurrentPhysicsInst() ? !pPhysical->GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) : true;
		}
	}
}

// sync parser setter functions
void CNetObjPhysical::SetPhysicalGameStateData(const CPhysicalGameStateDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		pPhysical->m_nPhysicalFlags.bRenderScorched      = data.m_renderScorched;
		pPhysical->SetIsInWater(data.m_isInWater);
		m_isInWater                                      = data.m_isInWater;

		if (data.m_alteringAlpha)
		{
			switch (data.m_alphaType)
			{
			case IPhysicalNodeDataAccessor::ART_NONE :
				if (IsAlphaFading())
				{
					if (data.m_isVisible)
					{
						NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_OUT_FINISHED", GetLogName());
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "SetPhysicalGameStateData");
					}
					SetAlphaFading(false);
				}
				else if (IsAlphaRamping())
				{
					if (data.m_isVisible)
					{
						NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_OUT_FINISHED", GetLogName());
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "SetPhysicalGameStateData");
					}
					StopAlphaRamping();
				}
				break;
			case IPhysicalNodeDataAccessor::ART_FADE_OUT:
				SetAlphaFading(true, true);
				break;
			case IPhysicalNodeDataAccessor::ART_RAMP_IN_AND_WAIT:
				SetAlphaRampFadeInAndWait(true);
				break;
			case IPhysicalNodeDataAccessor::ART_RAMP_IN_AND_QUIT:
				SetAlphaRampFadeInAndQuit(true);
				break;
			case IPhysicalNodeDataAccessor::ART_RAMP_OUT:
				SetAlphaRampFadeOutOverDuration(true, data.m_customFadeDuration);
				break;
			case IPhysicalNodeDataAccessor::ART_FADE_IN:
				SetAlphaIncreasing(true);
				break;
			default:
				gnetAssertf(0, "Unrecognised alpha type");
			}
		}

		m_LastReceivedVisibilityFlag = data.m_isVisible;
		SetIsVisible(m_LastReceivedVisibilityFlag, "SetPhysicalGameStateData", true);

		m_AllowCloneWhileInTutorial = data.m_allowCloningWhileInTutorial;
	}
}

void CNetObjPhysical::SetPhysicalScriptGameStateData(const CPhysicalScriptGameStateDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		pPhysical->m_nPhysicalFlags.bNotDamagedByAnything			= data.m_PhysicalFlags.notDamagedByAnything;
		pPhysical->m_nPhysicalFlags.bDontLoadCollision				= data.m_PhysicalFlags.dontLoadCollision;
        pPhysical->m_nPhysicalFlags.bAllowFreezeIfNoCollision		= data.m_PhysicalFlags.allowFreezeIfNoCollision;
		pPhysical->m_nPhysicalFlags.bOnlyDamagedByPlayer			= data.m_PhysicalFlags.damagedOnlyByPlayer;
		pPhysical->m_nPhysicalFlags.bNotDamagedByBullets			= data.m_PhysicalFlags.notDamagedByBullets;
		pPhysical->m_nPhysicalFlags.bNotDamagedByFlames				= data.m_PhysicalFlags.notDamagedByFlames;
		pPhysical->m_nPhysicalFlags.bIgnoresExplosions				= data.m_PhysicalFlags.ignoresExplosions;
		pPhysical->m_nPhysicalFlags.bNotDamagedByCollisions			= data.m_PhysicalFlags.notDamagedByCollisions;
		pPhysical->m_nPhysicalFlags.bNotDamagedByMelee				= data.m_PhysicalFlags.notDamagedByMelee;
        pPhysical->m_nPhysicalFlags.bNotDamagedBySteam              = data.m_PhysicalFlags.notDamagedBySteam;
        pPhysical->m_nPhysicalFlags.bNotDamagedBySmoke              = data.m_PhysicalFlags.notDamagedBySmoke;
		pPhysical->m_nPhysicalFlags.bNotDamagedByRelGroup			= data.m_PhysicalFlags.notDamagedByRelGroup;
		pPhysical->m_nPhysicalFlags.bOnlyDamagedByRelGroup			= data.m_PhysicalFlags.onlyDamageByRelGroup;
        pPhysical->m_bDontResetDamageFlagsOnCleanupMissionState		= data.m_PhysicalFlags.dontResetDamageFlagsOnCleanupMissionState;
		pPhysical->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript	= data.m_PhysicalFlags.onlyDamagedWhenRunningScript;
		pPhysical->m_nPhysicalFlags.bPickUpByCargobobDisabled		= data.m_PhysicalFlags.pickUpByCargobobDisabled;

		SetIsInCutsceneRemotely(data.m_PhysicalFlags.inCutscene);

		m_bGhostedForGhostPlayers = data.m_PhysicalFlags.ghostedForGhostPlayers;

		pPhysical->m_specificRelGroupHash = data.m_RelGroupHash;

        SetLocalFlag(LOCALFLAG_NOREASSIGN, data.m_PhysicalFlags.noReassign);

		m_bPassControlInTutorial = data.m_PhysicalFlags.passControlInTutorial;

        SetAlwaysClonedForPlayerFlags(data.m_AlwaysClonedForPlayer);

        if(data.m_HasMaxSpeed)
        {
			if(!IsClose(pPhysical->GetMaxSpeed(), data.m_MaxSpeed, 0.0001f))
			{
				pPhysical->SetMaxSpeed(data.m_MaxSpeed);
			}
        }
        else
        {
            pPhysical->SetMaxSpeed(-1);
        }

		m_AllowMigrateToSpectator = data.m_AllowMigrateToSpectator;
	}
}

void CNetObjPhysical::SetVelocity(const s32 packedVelocityX, const s32 packedVelocityY, const s32 packedVelocityZ)
{
    VALIDATE_GAME_OBJECT(GetPhysical());

    Vector3 targetVelocity = Vector3(0.0f, 0.0f, 0.0f);
    targetVelocity.x = netSerialiserUtils::UnPackFixedPoint16((s16)packedVelocityX, SIZEOF_VELOCITY_POINT);
    targetVelocity.y = netSerialiserUtils::UnPackFixedPoint16((s16)packedVelocityY, SIZEOF_VELOCITY_POINT);
    targetVelocity.z = netSerialiserUtils::UnPackFixedPoint16((s16)packedVelocityZ, SIZEOF_VELOCITY_POINT);

    m_LastVelocityReceived = targetVelocity;
}

void CNetObjPhysical::SetAngularVelocity(const CPhysicalAngVelocityDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

    Vector3 targetAngVelocity = Vector3(0.0f, 0.0f, 0.0f);
    targetAngVelocity.x = netSerialiserUtils::UnPackFixedPoint16((s16)data.m_packedAngVelocityX, SIZEOF_ANGVELOCITY_POINT);
    targetAngVelocity.y = netSerialiserUtils::UnPackFixedPoint16((s16)data.m_packedAngVelocityY, SIZEOF_ANGVELOCITY_POINT);
    targetAngVelocity.z = netSerialiserUtils::UnPackFixedPoint16((s16)data.m_packedAngVelocityZ, SIZEOF_ANGVELOCITY_POINT);

    // set target state on blender, which object will be blended to from current position
    CNetBlenderPhysical *pPhysicalBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());

    if (pPhysicalBlender)
    {
        pPhysicalBlender->UpdateAngVelocity(targetAngVelocity, pPhysicalBlender->GetLastSyncMessageTime());
    }
    // don't apply velocity updates to attached entities
    else if(pPhysical && !pPhysical->GetIsTypePed() && !pPhysical->GetIsAttached())
    {
        pPhysical->SetAngVelocity(targetAngVelocity);
    }
}

void CNetObjPhysical::UpdateDamageTrackers(const CPhysicalHealthDataNode& data)
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical && gnetVerify(IsClone()))
	{
		pPhysical->ResetWeaponDamageInfo();

		bool triggerDamageEvent = false;

		netObject* pNetObj = 0;
		float damageDealt  = 0.0f;
		bool  willDestroy  = false;

		if (data.m_weaponDamageEntity != NETWORK_INVALID_OBJECT_ID)
		{
			pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_weaponDamageEntity);

			if (pNetObj && pNetObj->GetEntity())
			{
				pPhysical->SetWeaponDamageInfo(pNetObj->GetEntity(), data.m_weaponDamageHash, fwTimer::GetTimeInMilliseconds());
			}
		}

		//Calculate if the network object is already dead/destroyed
		bool destroyed = false;
		if(pPhysical->GetIsTypeObject())
		{
			CObject* victim = static_cast<CObject *>(pPhysical);
			destroyed = victim->GetHealth() <= 0.0f;

			if (!destroyed)
			{
				damageDealt = victim->GetHealth() - (float)data.m_health;
				willDestroy = ((victim->GetHealth() - damageDealt) <= 0.0f);
			}
		}

		//We have received a RECEIVED_CLONE_CREATE - so this clone can be receiving a damage event.
		if (!NetworkInterface::GetObjectManager().GetNetworkObject(GetObjectID()))
		{
			damageDealt = 0.0f;
		}

		if (!data.m_hasMaxHealth && !destroyed && damageDealt > 0.0f)
		{
			triggerDamageEvent = true;
		}

		if (triggerDamageEvent)
		{
			NetworkDamageInfo damageInfo(pNetObj ? pNetObj->GetEntity() : NULL, damageDealt, 0.0f, 0.0f, false, data.m_weaponDamageHash, -1, false, willDestroy, false, data.m_lastDamagedMaterialId);
			UpdateDamageTracker(damageInfo);
		}
	}
}

void CNetObjPhysical::SetPhysicalHealthData(const CPhysicalHealthDataNode& data)
{
    CPhysical *pPhysical = GetPhysical();
    VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		// Update all damage trackers and trigger damage events.
		UpdateDamageTrackers(data);

		if (data.m_maxHealthSetByScript)
		{
			pPhysical->SetMaxHealth((float)data.m_scriptMaxHealth);
			m_MaxHealthSetByScript = true;
		}

		if (data.m_hasMaxHealth)
		{
			pPhysical->SetHealth(pPhysical->GetMaxHealth(), true);
		}
		else
		{
			pPhysical->SetHealth((float)data.m_health, true);
		}

		// Register the damage with the kill trackers (tests health internally)....
		RegisterKillWithNetworkTracker();
	}
}

void CNetObjPhysical::SetPhysicalAttachmentData(const bool        attached,
                                                const ObjectId    attachedObjectID,
                                                const Vector3    &attachmentOffset,
                                                const Quaternion &attachmentQuat,
												const Vector3	 &attachmentParentOffset,
                                                const s16         attachmentOtherBone,
                                                const s16         attachmentMyBone,
                                                const u32         attachmentFlags,
                                                const bool        allowInitialSeparation,
                                                const float       invMassScaleA,
                                                const float       invMassScaleB,
												const bool		  activatePhysicsWhenDetached,
                                                const bool        UNUSED_PARAM(isCargoVehicle))
{
    VALIDATE_GAME_OBJECT(GetPhysical());

    m_pendingAttachmentObjectID = NETWORK_INVALID_OBJECT_ID;

    if (attached)
    {
        m_pendingAttachmentObjectID = attachedObjectID;
        m_pendingAttachOffset       = attachmentOffset;
        m_pendingAttachQuat			= attachmentQuat;
		m_pendingAttachParentOffset = attachmentParentOffset;
		m_pendingOtherAttachBone    = attachmentOtherBone;
        m_pendingMyAttachBone       = attachmentMyBone;
        m_pendingAttachFlags        = attachmentFlags;
        m_pendingAllowInitialSeparation    = allowInitialSeparation;
        m_pendingInvMassScaleA      = invMassScaleA;
        m_pendingInvMassScaleB      = invMassScaleB;
    }
	else
	{
		m_pendingDetachmentActivatePhysics = activatePhysicsWhenDetached;
	}

	ProcessPendingAttachment();
}

bool CNetObjPhysical::IsPhysicallyAttached() const
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	bool bPhysicallyAttached = false;

	if (pPhysical)
	{
		fwAttachmentEntityExtension *attachExt = pPhysical->GetAttachmentExtension();
		if (attachExt && attachExt->GetIsAttached() && (attachExt->GetAttachState() == ATTACH_STATE_PHYSICAL || attachExt->GetAttachState() == ATTACH_STATE_RAGDOLL))
		{
			bPhysicallyAttached = true;
		}
	}

	return bPhysicallyAttached;
}

void CNetObjPhysical::SetCanBeReassigned(bool canBeReassigned)
{
    if(gnetVerifyf(IsGlobalFlagSet(GLOBALFLAG_SCRIPTOBJECT), "Trying to flag a non-script object to not reassign!"))
    {
        gnetAssertf(IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER), "Flagging a script object to not be reassigned that can migrate to other players!");
        SetLocalFlag(LOCALFLAG_NOREASSIGN, !canBeReassigned);
    }
}

// Name         :   ProcessPendingAttachment
// Purpose      :   updates pending attachment state, trying to attach to detach when possible
//
void CNetObjPhysical::ProcessPendingAttachment()
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	bool bShouldBeAttached = m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID;
	
	if (pPhysical && (bShouldBeAttached || pPhysical->GetIsAttached())) // the attachment entity may have changed, so we need to always run the following code when attached
	{
		CPhysical* pCurrentAttachmentEntity = NULL;

		int failReason = CPA_SUCCESS;

		if (CanProcessPendingAttachment(&pCurrentAttachmentEntity, &failReason))
		{
			if (bShouldBeAttached)
			{
				bool bSuccessfullyAttached = false;

				if (pCurrentAttachmentEntity)
				{
					if (IsClone())
					{
						ObjectId currentAttachmentId =  gnetVerify(pCurrentAttachmentEntity->GetNetworkObject()) ? pCurrentAttachmentEntity->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

						if (currentAttachmentId != m_pendingAttachmentObjectID)
						{
							// detach if the attachment entity has changed or the current attachment cannot be processed 
							AttemptPendingDetachment(pCurrentAttachmentEntity);
						}
						else 
						{
							// update current attachment if already attached properly
							UpdatePendingAttachment();
						}
					}
					else
					{
						bSuccessfullyAttached = true;
					}
				}
				else 
				{
					// create a new attachment 
					netObject *objectToAttachTo = NetworkInterface::GetObjectManager().GetNetworkObject(m_pendingAttachmentObjectID);

					if (objectToAttachTo)
					{
						CPhysical *pEntityToAttachTo = SafeCast(CPhysical, objectToAttachTo->GetEntity());

						if (pEntityToAttachTo)
						{
#if ENABLE_NETWORK_LOGGING
							unsigned attempedPendingAttachementFailReason = 0;
							bSuccessfullyAttached = AttemptPendingAttachment(pEntityToAttachTo, &attempedPendingAttachementFailReason);
							
							if(!bSuccessfullyAttached)
							{
								netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
								NetworkLogUtils::WriteLogEvent(log, "ATTEMPT_PENDING_ATTACHMENT_FAILED", GetLogName());
								log.WriteDataValue("Reason", NetworkUtils::GetAttemptPendingAttachmentFailReason(attempedPendingAttachementFailReason));
							}
#else
							bSuccessfullyAttached = AttemptPendingAttachment(pEntityToAttachTo);
#endif // ENABLE_NETWORK_LOGGING
							

							if (bSuccessfullyAttached)
							{
								m_pendingAttachStateMismatch = false;
							}
						}
					}
				}

				// we can now clear the pending attachment data for local entities
				if (bSuccessfullyAttached && !IsClone())
				{
					ClearPendingAttachmentData();
				}
			}
			else if (pCurrentAttachmentEntity && IsClone())
			{
				// don't allow the network code to detach a ped that is running a DYING_DEAD task without ragdolling because that task could force an attachment for animated deaths
				bool allowDetaching = true;
				bool allowDeathTaskCheck = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_DEATH_TASK_CHECK_FOR_NET_ATTACHMENT", 0xD1E93396), true);
				if(allowDeathTaskCheck && pCurrentAttachmentEntity->GetIsTypeVehicle() && SafeCast(CVehicle, pCurrentAttachmentEntity)->InheritsFromTrain() && pPhysical->GetIsTypePed())
				{
					CPed* ped = SafeCast(CPed, pPhysical);
					CTaskDyingDead* taskDead = static_cast<CTaskDyingDead*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
					if(taskDead && !ped->GetUsingRagdoll())
					{
						allowDetaching = false;
					}
				}

				if(allowDetaching)
				{
					AttemptPendingDetachment(pCurrentAttachmentEntity);
					m_pendingAttachStateMismatch = false;
				}
			}
		}
		else
		{
#if ENABLE_NETWORK_LOGGING
			if (failReason != CPA_SUCCESS)
			{
				ObjectId currentAttachmentId = (pCurrentAttachmentEntity && pCurrentAttachmentEntity->GetNetworkObject()) ? pCurrentAttachmentEntity->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

				if (m_LastFailedAttachmentReason != failReason && (bShouldBeAttached != pPhysical->GetIsAttached() || (pCurrentAttachmentEntity && currentAttachmentId != m_pendingAttachmentObjectID)))
				{
					m_LastFailedAttachmentReason = failReason;
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CANT_PROCESS_ATTACHMENT", GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "%s", NetworkUtils::GetCanProcessAttachmentErrorString(failReason));
				}
			}
#endif
			m_pendingAttachStateMismatch = false;
		}
	}
	else
	{
		m_pendingAttachStateMismatch = false;
	}
}

bool CNetObjPhysical::IsPendingAttachment() const
{
	return m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID;
}

bool CNetObjPhysical::IsAlphaActive()
{
	return (entity_commands::CommandGetEntityAlphaEntity(GetEntity()) < 255);
}

void CNetObjPhysical::SetAlpha(u32 alpha)
{
	CEntity* pEntity = GetEntity();
	if (pEntity)
	{
#if __BANK
		if(PARAM_netExcessiveAlphaLogging.Get())
		{
			gnetDebug1("%s setting alpha to %d", GetLogName(), alpha);
		}
#endif //__BANK
		
		entity_commands::CommandSetEntityAlphaEntity(pEntity, alpha, false BANK_PARAM(entity_commands::CMD_OVERRIDE_ALPHA_BY_NETCODE));
	}

	m_bHasHadAlphaAltered = (alpha != 255);
}

void CNetObjPhysical::ResetAlphaActive()
{
	CEntity* pEntity = GetEntity();
	if (pEntity)
	{
#if __BANK
		if(PARAM_netExcessiveAlphaLogging.Get())
		{
			gnetDebug1("%s resetting alpha", GetLogName());
		}
#endif //__BANK

		entity_commands::CommandSetEntityAlphaEntity(pEntity, 255, false BANK_PARAM(entity_commands::CMD_OVERRIDE_ALPHA_BY_NETCODE));
		entity_commands::CommandResetEntityAlphaEntity(pEntity);

		//also ensure base alpha is 255
		pEntity->SetAlpha(255);

		m_bHasHadAlphaAltered = false;
	}
}

void CNetObjPhysical::SetAlphaRampingInternal(bool bTimedRamp, bool bRampOut, bool bQuitWhenTimerExpired, bool bNetwork, s16 iCustomDuration) 
{ 
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (m_bAlphaFading && !bRampOut)
	{
		SetAlphaFading(false);
	}

	if (m_bAlphaRamping && m_bAlphaRampOut != bRampOut)
	{
		m_bAlphaRamping = false;
	}

	if (!m_bAlphaFading && !m_bAlphaRamping)
	{
		m_bAlphaRamping				= true;
		m_bAlphaRampOut				= bRampOut;
		m_alphaRampingTimeRemaining	= 0;

		if (bRampOut)
		{
			m_bAlphaIncreasing			= false;
			m_iAlphaOverride			= 255;
		}
		else
		{
			m_bAlphaIncreasing			= true;
			m_iAlphaOverride			= 0;
		}

		SetAlpha(m_iAlphaOverride);

		if (bTimedRamp)
		{
			if (iCustomDuration > 0) 
			{
				m_alphaRampingTimeRemaining = iCustomDuration;
				m_iCustomFadeDuration = iCustomDuration;
			}
			else
			{
				m_alphaRampingTimeRemaining = bRampOut ? NetworkUtils::GetSpecialAlphaRampingFadeOutTime() : NetworkUtils::GetSpecialAlphaRampingFadeInTime();
				m_iCustomFadeDuration = 0;
			}
		}

		m_bAlphaRampTimerQuit		= bQuitWhenTimerExpired; 
		m_bNetworkAlpha				= bNetwork;

		if (pPhysical && IsFadingOut())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_OUT (RAMP)", GetLogName());

			SetHideWhenInvisible(true);

			if (m_bNetworkAlpha && !IsClone() && GetSyncData())
			{
				// force an immediate update of the game state node, so that the fade will start before the entity is potentially moved
				GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());

				if (pPhysical->GetIsTypePed())
				{
					GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPedSyncTreeBase, GetSyncTree())->GetPhysicalGameStateNode());
				}
				else
				{
					GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPhysicalSyncTreeBase, GetSyncTree())->GetPhysicalGameStateNode());
				}
			}
		}
		else
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_IN (RAMP)", GetLogName());
		}
	}
}

void CNetObjPhysical::StopAlphaRamping()
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (m_bAlphaRamping)
	{
		m_alphaRampingTimeRemaining = 0;
		m_bAlphaRampTimerQuit = true;

		if (m_bNetworkAlpha && pPhysical && !IsClone() && GetSyncData())
		{
			// force an immediate update of the game state node, so that the fade will start before the entity is potentially moved
			GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());

			if (pPhysical->GetIsTypePed())
			{
				GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPedSyncTreeBase, GetSyncTree())->GetPhysicalGameStateNode());
			}
			else
			{
				GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPhysicalSyncTreeBase, GetSyncTree())->GetPhysicalGameStateNode());
			}
		}

		ProcessAlpha();

		if (m_bAlphaRampOut)
		{
			m_bAlphaRamping = false;
			SetIsVisible(true, "StopAlphaRamping");
			ResetAlphaActive();			
		}
	}
}

void CNetObjPhysical::SetAlphaIncreasing(bool bNetwork)
{
	m_iAlphaOverride = 0;
	m_bNetworkAlpha = bNetwork;
	m_bAlphaIncreasingNoFlash = true;
}

void CNetObjPhysical::SetAlphaFading(bool bValue, bool bNetwork)
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (m_bAlphaFading != bValue)
	{
		if (m_bAlphaFading)
		{
			ResetAlphaActive();
		}
		else
		{
			m_bAlphaIncreasing = false;
			m_iAlphaOverride = 255;
		}

		m_bAlphaFading = bValue;
		m_bAlphaRamping = false;

		if (m_bAlphaFading)
		{
			m_bNetworkAlpha = bNetwork;

			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FADING_OUT (SMOOTH)", GetLogName());

			SetHideWhenInvisible(true);

			if (!IsClone())
			{
				// force an immediate update of the game state node, so that the fade will start before the entity is potentially moved
				if (!IsClone() && GetSyncData())
				{
					GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());

					if (pPhysical->GetIsTypePed())
					{
						GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPedSyncTreeBase, GetSyncTree())->GetPhysicalGameStateNode());
					}
					else
					{
						GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPhysicalSyncTreeBase, GetSyncTree())->GetPhysicalGameStateNode());
					}
				}
			}
		}
	}
}

bool CNetObjPhysical::IsPendingAttachmentPhysical() const
{
	bool bPhysical = false;

	if ((m_pendingAttachFlags & ATTACH_STATES) == ATTACH_STATE_PHYSICAL || 
		(m_pendingAttachFlags & ATTACH_STATES) == ATTACH_STATE_RAGDOLL )
	{
		bPhysical = true;
	}

	return bPhysical;
}

fwAttachmentEntityExtension* CNetObjPhysical::GetAttachmentExtension() const
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		return pPhysical->GetAttachmentExtension();
	}

	return NULL;
}

bool CNetObjPhysical::CanProcessPendingAttachment(CPhysical** ppCurrentAttachmentEntity, int* failReason) const
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	bool bCanProcess = true;

	if (!pPhysical)
	{
		if (failReason)
		{
			*failReason = CPA_NO_ENTITY;
		}

		bCanProcess = false;
	}
	else 
	{
		CPhysical* pCurrentAttachmentEntity = ppCurrentAttachmentEntity ? *ppCurrentAttachmentEntity : NULL;
		
		if (!pCurrentAttachmentEntity)
		{
			fwAttachmentEntityExtension *physExtension = GetAttachmentExtension();

			CEntity* pAttachParent = physExtension ? static_cast<CEntity*>(physExtension->GetAttachParent()) : NULL;

			pCurrentAttachmentEntity = (pAttachParent && pAttachParent->GetIsPhysical()) ? static_cast<CPhysical*>(physExtension->GetAttachParent()) : NULL;

			if (ppCurrentAttachmentEntity)
			{
				*ppCurrentAttachmentEntity = pCurrentAttachmentEntity;
			}
		}
		
		if(IsClone() && NetworkAttachmentIsOverridden())
		{
			if (failReason)
			{
				*failReason = CPA_OVERRIDDEN;
			}

			bCanProcess = false;
		}

		// we can't process this attachment when the entity is attached to a non-networked entity
		if (bCanProcess && pCurrentAttachmentEntity && !pCurrentAttachmentEntity->GetNetworkObject())
		{
			if (failReason)
			{
				*failReason = CPA_NOT_NETWORKED;
			}

			bCanProcess = false;
		}

		// we can't process this attachment when the entity to attach to is attached to this entity (this can happen with players and parachutes)
		if (bCanProcess && 
			m_pendingAttachmentObjectID != NETWORK_INVALID_OBJECT_ID && 
			(!pCurrentAttachmentEntity || m_pendingAttachmentObjectID != pCurrentAttachmentEntity->GetNetworkObject()->GetObjectID()))
		{
			netObject *objectToAttachTo = NetworkInterface::GetObjectManager().GetNetworkObject(m_pendingAttachmentObjectID);
			CPhysical *pEntityToAttachTo = objectToAttachTo ? SafeCast(CPhysical, objectToAttachTo->GetEntity()) : NULL;
			PlayerAccountId accountId = this->GetPlayerOwner() ? SafeCast(const CNetGamePlayer, this->GetPlayerOwner())->GetPlayerAccountId() : 0;

			bool preventLocalPlayerAttachments = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PREVENT_LOCAL_PLAYER_ATTACHMENT", 0x1EA58FE7), true);
			bool preventLocalPlayerVehicleAttachments = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PREVENT_LOCAL_PLAYER_VEHICLE_ATTACHMENT", 0xCA9039F4), true);
			if (!pEntityToAttachTo)
			{
				if (failReason)
				{
					*failReason = CPA_NO_TARGET_ENTITY;
				}

				bCanProcess = false;
			}
			else if (pEntityToAttachTo->GetAttachmentExtension() && pEntityToAttachTo->GetAttachmentExtension()->GetAttachParent() && pEntityToAttachTo->GetAttachmentExtension()->GetAttachParent() == pPhysical)
			{
				if (failReason)
				{
					*failReason = CPA_CIRCULAR_ATTACHMENT;
				}

				bCanProcess = false;
			}
			// cannot attach entity to local player from sync code. This should be done from script command directly
			else if(preventLocalPlayerAttachments && pEntityToAttachTo->GetIsTypePed() && SafeCast(CPed, pEntityToAttachTo)->IsLocalPlayer())
			{
				if (failReason)
				{
					*failReason = CPA_LOCAL_PLAYER_ATTACHMENT;
				}
				CNetworkTelemetry::AppendPedAttached(1, pEntityToAttachTo->IsNetworkClone(), accountId);
				gnetAssertf(0, "Trying to attach %s to local player(%s) from synced data", GetLogName(), pEntityToAttachTo->GetLogName());
				bCanProcess = false;
			}
			else if(preventLocalPlayerVehicleAttachments && pEntityToAttachTo->GetIsTypeVehicle())
			{
				CVehicle* attachToVehicle = SafeCast(CVehicle, pEntityToAttachTo);
				CPed* driver = attachToVehicle->GetDriver();
				if(driver && driver->IsLocalPlayer() 
				&& GetObjectType() != NET_OBJ_TYPE_PLAYER
				&& (!attachToVehicle->InheritsFromHeli() || !SafeCast(CHeli, attachToVehicle)->GetIsCargobob())
				&& !(pPhysical->GetIsTypePed() && pPhysical->IsAScriptEntity()))
				{
					if (failReason)
					{
						*failReason = CPA_LOCAL_PLAYER_DRIVER_ATTACHMENT;
					}
					CNetworkTelemetry::AppendPedAttached(1, pEntityToAttachTo->IsNetworkClone(), accountId);
					gnetAssertf(0, "Trying to attach %s to vehicle(%s) driven by the local player(%s)", GetLogName(), pEntityToAttachTo->GetLogName(), driver->GetLogName());
					return false;
				}
			}
		}
	}

	return bCanProcess;
}

CPhysical* CNetObjPhysical::GetCurrentAttachmentEntity() const
{
	CPhysical* pCurrentAttachment = NULL;

	if (GetPhysical())
	{
		fwAttachmentEntityExtension *physExtension = GetAttachmentExtension();

		if (physExtension && physExtension->GetAttachParent() && static_cast<CEntity*>(physExtension->GetAttachParent())->GetIsPhysical())
		{
			pCurrentAttachment = SafeCast(CPhysical, physExtension->GetAttachParent());
		}
	}

	return pCurrentAttachment;
}

bool CNetObjPhysical::AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* LOGGING_ONLY(reason))
{
	bool bAttached = false;
#if ENABLE_NETWORK_LOGGING
	if(reason)
	{
		*reason = APA_NONE_PHYSICAL;
	}
#endif // ENABLE_NETWORK_LOGGING


	CPhysical *pPhysical = GetPhysical();

	CVehicleGadgetPickUpRope *pPickUpRope = CNetObjVehicle::GetPickupRopeGadget(pEntityToAttachTo);

	bool bHasPhysics = pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->IsInLevel();

	bool isCargobob = false;

	if(pEntityToAttachTo->GetIsTypeVehicle())
	{
		CVehicle* vehToAttachTo = SafeCast(CVehicle, pEntityToAttachTo);
		if(vehToAttachTo && vehToAttachTo->InheritsFromHeli())
		{
			CHeli* helToAttachTo = SafeCast(CHeli, vehToAttachTo);
			if(helToAttachTo && helToAttachTo->GetIsCargobob())
			{
				isCargobob = true;
			}
		}
	}

	if((pPickUpRope || isCargobob) && !pPhysical->GetIsTypePed() && !(pPhysical->GetIsTypeObject() && SafeCast(CObject, pPhysical)->IsPickup()))
	{
		if(!pPickUpRope)
		{
			#if ENABLE_NETWORK_LOGGING
			if(reason)
			{
				*reason = APA_CARGOBOB_HAS_NO_ROPE_ON_CLONE;
			}
			#endif // ENABLE_NETWORK_LOGGING
		}
		else
		{
			if (bHasPhysics)
			{
				CVehicle *pParentVehicle = SafeCast(CVehicle, pEntityToAttachTo);

				pPickUpRope->AttachEntityToPickUpProp(pParentVehicle, GetPhysical(), -1, m_pendingAttachOffset, static_cast<s32>(m_pendingMyAttachBone), false);

				bAttached = (pPickUpRope->GetAttachedEntity() == GetPhysical());

				if(bAttached)
				{
					OnAttachedToPickUpHook();
				}
	#if ENABLE_NETWORK_LOGGING
				else
				{
					if(reason)
					{
						*reason = APA_ROPE_ATTACHED_ENTITY_DIFFER;
					}
				}
	#endif // ENABLE_NETWORK_LOGGING
			}
		}
	}
	else
	{
		if ((m_pendingAttachFlags & ATTACH_STATES) == ATTACH_STATE_PHYSICAL)
		{
			if (bHasPhysics && GetPhysical()->AttachToPhysicalUsingPhysics(pEntityToAttachTo, m_pendingOtherAttachBone, m_pendingMyAttachBone, m_pendingAttachFlags, &m_pendingAttachOffset, &m_pendingAttachQuat, &m_pendingAttachParentOffset, -1.0f, m_pendingAllowInitialSeparation, m_pendingInvMassScaleA, m_pendingInvMassScaleB))
			{
				bAttached = true;
			}
		}
		else if((m_pendingAttachFlags & ATTACH_STATES) == ATTACH_STATE_WORLD)
		{
			gnetAssertf(false, "Network - unsupported attachment type in network game");
		}
		else
		{
			GetPhysical()->AttachToPhysicalBasic(pEntityToAttachTo, m_pendingOtherAttachBone, m_pendingAttachFlags, &m_pendingAttachOffset, &m_pendingAttachQuat);
			bAttached = true;
		}
	}

#if ENABLE_NETWORK_LOGGING
	if (bAttached)
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ATTACHING_OBJECT", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attached to", pEntityToAttachTo->GetNetworkObject() ? pEntityToAttachTo->GetNetworkObject()->GetLogName() : "??");
	}
#endif

	return bAttached;
}

void CNetObjPhysical::UpdatePendingAttachment()
{
	fwAttachmentEntityExtension *pExtension = GetPhysical()->GetAttachmentExtension();

	// update the attachment offset & orientation
	if (pExtension->GetAttachState()==ATTACH_STATE_BASIC || pExtension->GetAttachState()==ATTACH_STATE_PED || pExtension->GetAttachState()==ATTACH_STATE_PED_ON_GROUND)
	{
		pExtension->SetAttachOffset(m_pendingAttachOffset);
		pExtension->SetAttachQuat(m_pendingAttachQuat);
		pExtension->SetAttachParentOffset(m_pendingAttachParentOffset);
	}

	u32 localAttachFlags = pExtension->GetExternalAttachFlags();
	u32 attachFlags      = (u32)m_pendingAttachFlags & ((1<<NUM_EXTERNAL_ATTACH_FLAGS)-1);

	// for comparison here exclude ATTACH_FLAG_WARP_TO_PARENT as this can be in the m_pendingAttachFlags - but is removed on attachment - so shouldn't be used for comparison. lavalley
	attachFlags      &= ~ATTACH_FLAG_COL_ON;         // ATTACH_FLAG_COL_ON can be changed independently on the clone (e.g. if clone switches to low lod physics but remains high lod on the owner)
	localAttachFlags &= ~ATTACH_FLAG_COL_ON;         // ATTACH_FLAG_COL_ON can be changed independently on the clone (e.g. if clone switches to low lod physics but remains high lod on the owner)


	// detach the current attachment if the attachment flags or bones differ (we'll need to re-attach)
	if ((localAttachFlags != attachFlags) ||
		(pExtension->GetMyAttachBone()	  != m_pendingMyAttachBone) ||
		(pExtension->GetOtherAttachBone() != m_pendingOtherAttachBone))
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "PENDING_ATTACHMENT_DIFFER", GetLogName());
		if(localAttachFlags != attachFlags)
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach Flags Current:", "%d", localAttachFlags);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach Flags Pending:", "%d", attachFlags);
		}
		if(pExtension->GetMyAttachBone() != m_pendingMyAttachBone)
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach Bone Current:", "%d", pExtension->GetMyAttachBone());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach Bone Pending:", "%d", m_pendingMyAttachBone);
		}
		if(pExtension->GetOtherAttachBone() != m_pendingOtherAttachBone)
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach Other Bone Current:", "%d", pExtension->GetOtherAttachBone());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attach Other Bone Pending:", "%d", m_pendingOtherAttachBone);
		}
#endif // ENABLE_NETWORK_LOGGING
		AttemptPendingDetachment(GetCurrentAttachmentEntity());
	}
}

void CNetObjPhysical::AttemptPendingDetachment(CPhysical* pEntityAttachedTo)
{	
	CPhysical *pPhysical = GetPhysical();

	// we shouldn't be meddling with the attachment states of weapons wielded by peds - they are synced elsewhere
	gnetAssert(!(pPhysical->GetIsTypeObject() && static_cast<CObject *>(pPhysical)->GetWeapon() && pEntityAttachedTo->GetIsTypePed()));

	CVehicleGadgetPickUpRope *pPickUpRope = CNetObjVehicle::GetPickupRopeGadget(pEntityAttachedTo);

#if ENABLE_NETWORK_LOGGING
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "DETACHING_OBJECT", GetLogName());
	NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Attached from", pEntityAttachedTo->GetNetworkObject() ? pEntityAttachedTo->GetNetworkObject()->GetLogName() : "??");
#endif

	if(pPickUpRope && !pPhysical->GetIsTypePed() && !(pPhysical->GetIsTypeObject() && SafeCast(CObject, pPhysical)->IsPickup()))
	{
		if (pEntityAttachedTo)
		{
			if (!gnetVerifyf(pPickUpRope->GetAttachedEntity(), "%s is trying to detach from the pick up rope on %s, which has no attachment", GetLogName(), pEntityAttachedTo->GetNetworkObject()->GetLogName()))
			{
				return;
			}
			if (!gnetVerifyf(pPickUpRope->GetAttachedEntity() == pPhysical, "%s is trying to detach from the pick up rope on %s, which is actually attached to %s", GetLogName(), pEntityAttachedTo->GetNetworkObject()->GetLogName(), pPickUpRope->GetAttachedEntity()->GetNetworkObject() ? pPickUpRope->GetAttachedEntity()->GetNetworkObject()->GetLogName() : "??"))
			{
				return;
			}
		}

		pPickUpRope->DetachEntity();
	}
	else
	{
		// we need keep the uses collision flag as it was otherwise we can end up with this flag out of sync
		bool usesCollision = pPhysical->IsCollisionEnabled();
		pPhysical->DetachFromParent(m_pendingDetachmentActivatePhysics ? DETACH_FLAG_ACTIVATE_PHYSICS : 0);

		if(pPhysical->GetIsTypePed())
		{
			const CPed *ped = static_cast<const CPed *>(pPhysical);
			if(ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
			{
				return; //Ped base flag collision is kept false when at Low LOD 
			}
		}

		if (pPhysical->GetIsTypeObject())
		{
			// set no collision until clear 
			pPhysical->SetNoCollision(pPhysical->GetAttachParentForced(), NO_COLLISION_RESET_WHEN_NO_BBOX);
			
			// Check for when we detach a remote car parachute (Ruiner vehicle gadget)
			CObject* object = static_cast<CObject*>(pPhysical);

			if (object && IsClone() && object->GetIsCarParachute())
			{
				//Fade out the object.
				object->m_nObjectFlags.bFadeOut = true;				
				object->DisableCollision(NULL, true);
			}
		}

		usesCollision ? pPhysical->EnableCollision() : DisableCollision();
	}
}

void CNetObjPhysical::ClearPendingAttachmentData()
{
#if ENABLE_NETWORK_LOGGING
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CLEAR_OBJECT_ATTACHMENT", GetLogName());
#endif

	m_pendingAttachmentObjectID = NETWORK_INVALID_OBJECT_ID;
	m_pendingOtherAttachBone    = -1;
	m_pendingMyAttachBone       = -1;
	m_pendingAttachFlags        = 0;

	m_pendingAttachOffset.Zero();
	m_pendingAttachQuat.Identity();
	m_pendingAttachParentOffset.Zero();

	m_pendingAttachStateMismatch = false;
}

void CNetObjPhysical::SetAsGhost(bool bSet BANK_ONLY(, unsigned reason, const char* scriptInfo))
{
#if __BANK
	if(m_bGhost != bSet && NetworkInterface::IsGameInProgress())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "CHANGING_GHOSTING_STATE", "%s", GetLogName());
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Setting It To", bSet ? "True" : "False");
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", scriptInfo);
		NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", NetworkUtils::GetSetGhostingReason(reason));
	}
#endif // __BANK
	m_bGhost = bSet;
} 

bool CNetObjPhysical::CanShowGhostAlpha() const
{
	bool bCanShowAlpha = true;

	if (GetInvertGhosting() && !IsGhost())
	{
		return false;
	}

	if (IsClone())
	{
		// don't show the entity ghosted if a collision with the local player is not treated as a ghost collision
		if (GetPhysical() && !CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(*GetPhysical()))
		{
			bCanShowAlpha = false;
		}
	}

	return bCanShowAlpha;
}

void CNetObjPhysical::SetInvertGhosting(bool bSet)	
{ 
	ms_invertGhosting = bSet;
}

void CNetObjPhysical::OnAttachedToPickUpHook()
{
	if (!IsClone() && GetSyncData() && GetEntity() && GetSyncTree())
	{
		// force a sync tree update so that the net obj will not migrate until the attachment state is synced
		GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());
	}
}

void CNetObjPhysical::OnDetachedFromPickUpHook()
{
	OnAttachedToPickUpHook();
}

void CNetObjPhysical::ForceSendOfCutsceneState()
{
	if (!IsClone() && GetSyncData())
	{
		CPhysicalSyncTreeBase *syncTree = SafeCast(CPhysicalSyncTreeBase, GetSyncTree());

		if(syncTree)
		{
			syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetPhysicalScriptGameStateNode());
		}
	}
}

void CNetObjPhysical::SetIsInCutsceneRemotely(bool bSet)
{
	CPhysical *pPhysical = GetPhysical();

	//If remote is syncing being in a cut scene check if this local clone object is in cutscene on our machine also and
	//since notDamagedByAnything reflects the remote cached CS state ensure the CS has the same cached invincibility state here
	if (pPhysical && bSet && !m_bInCutsceneRemotely)
	{
		CutSceneManager* pManager = CutSceneManager::GetInstance();
		if (pManager)
		{
			CCutsceneAnimatedModelEntity* pCSAModelEntity = pManager->GetAnimatedModelEntityFromEntity(pPhysical);

			if( pCSAModelEntity )
			{
				pCSAModelEntity->SetWasInvincible(pPhysical->m_nPhysicalFlags.bNotDamagedByAnything);
			}
		}
	}
	
	m_bInCutsceneRemotely = bSet;
}

bool CNetObjPhysical::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
    CPhysical *pPhysical = GetPhysical();

    if(pPhysical)
    {
        // entities in water on either the local or remote machines don't need fixing
        if(pPhysical->GetIsInWater())
        {
			// GetIsInWater can return true even the object is not in water
			static const float MIN_SUBMERGE_LEVEL = 0.3f;
			float level = pPhysical->m_Buoyancy.GetSubmergedLevel();
			if(level > MIN_SUBMERGE_LEVEL)
			{
				if(npfbnReason)
				{	
					*npfbnReason = NPFBN_LOCAL_IN_WATER;
				}
				return false;
			}
        }
		else if(IsClone() && m_isInWater)
		{
			const Vector3 vOriginalPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
			float waterZ = 0.0f;
			WaterTestResultType waterTestResult = pPhysical->m_Buoyancy.GetWaterLevelIncludingRivers(vOriginalPosition, &waterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPhysical);
			if(waterTestResult > WATERTEST_TYPE_NONE)
			{
				if(npfbnReason)
				{	
					*npfbnReason = NPFBN_CLONE_IN_WATER;
				}
				return false;
			}
			else
			{
				 return true;
			}
		}

	    // all non-script entities are fixed when the map is streamed out (script objects have their fixedwaitingforcollision flag set elsewhere)
        if (!pPhysical->ShouldFixIfNoCollisionLoadedAroundPosition())
	    {
		    return true;
	    }

		// don't fix entities that are attached
		if (pPhysical->GetIsAttached())
		{
			if(npfbnReason)
			{	
				*npfbnReason = NPFBN_ATTACHED;
			}
			return false;
		}

	    if(IsClone())
        {
            if(pPhysical->m_nPhysicalFlags.bDontLoadCollision)
            {
                if (pPhysical->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool CNetObjPhysical::CheckBoxStreamerBeforeFixingDueToDistance() const
{
    static const float USE_BOX_STREAMER_LOD_RANGE_SCALE_THRESHOLD = 1.5f;

    if(CVehicleAILodManager::GetLodRangeScale() >= USE_BOX_STREAMER_LOD_RANGE_SCALE_THRESHOLD)
    {
        return true;
    }

    return false;
}

void CNetObjPhysical::SetFixedByNetwork(bool fixObject, unsigned LOGGING_ONLY(reason), unsigned LOGGING_ONLY(npfbnReason))
{
	CPhysical* pPhysical = static_cast<CPhysical*>(GetEntity());
	VALIDATE_GAME_OBJECT(pPhysical);

    bool fixedStateChanged = fixObject != pPhysical->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK);

#if ENABLE_NETWORK_LOGGING
    if(fixedStateChanged || (m_LastFixedByNetworkChangeReason != reason) || (!fixObject && m_LastNpfbnChangeReason != npfbnReason))
    {
        if (fixObject)
        {
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FIXED_BY_NETWORK", GetLogName());
            NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", NetworkUtils::GetFixedByNetworkReason(reason));
        }
        else
        {
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "UNFIXED_BY_NETWORK", GetLogName());
            NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", NetworkUtils::GetFixedByNetworkReason(reason));
			if(reason == FBN_NOT_PROCESSING_FIXED_BY_NETWORK)
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("NPFBN Reason", NetworkUtils::GetNpfbnNetworkReason(npfbnReason));
			}
		}

        m_LastFixedByNetworkChangeReason = reason;
		m_LastNpfbnChangeReason = npfbnReason;
    }
#endif // ENABLE_NETWORK_LOGGING
	if (pPhysical && fixedStateChanged)
	{
		pPhysical->SetFixedPhysics(fixObject, true);

		if (!fixObject)
		{
			pPhysical->ActivatePhysics();

			// do a portal rescan to make sure the entity is in the right interior
			if (pPhysical->GetPortalTracker())
			{
				pPhysical->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR); 
				pPhysical->GetPortalTracker()->ScanUntilProbeTrue(); 		
			}
		}
	}
}

u8 CNetObjPhysical::GetDefaultUpdateLevel() const
{
    CPhysical* pPhysical = static_cast<CPhysical*>(GetEntity());
    VALIDATE_GAME_OBJECT(pPhysical);

    if (pPhysical)
    {
        if(pPhysical->GetVelocity().Mag2() > 0.1f || pPhysical->GetAngVelocity().Mag2() > 0.1f)
        {
            return CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
        }
        else
        {
            return CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
        }
    }

    return CNetObjDynamicEntity::GetDefaultUpdateLevel();
}

u8 CNetObjPhysical::GetLowestAllowedUpdateLevel() const
{
    u8 lowestAllowedUpdateLevel = CNetObjDynamicEntity::GetLowestAllowedUpdateLevel();

    if(m_TimeToForceLowestUpdateLevel > 0)
    {
        if(m_LowestUpdateLevelToForce > lowestAllowedUpdateLevel)
        {
            lowestAllowedUpdateLevel = m_LowestUpdateLevelToForce;
        }
    }

    return lowestAllowedUpdateLevel;
}

bool CNetObjPhysical::HasCollisionLoadedUnderEntity() const
{
	CPhysical* pPhysical = static_cast<CPhysical*>(GetEntity());
	VALIDATE_GAME_OBJECT(pPhysical);

	if (pPhysical)
	{
		return g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(pPhysical->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	}

	return true;
}

void CNetObjPhysical::SetLowestAllowedUpdateLevel(u8 lowestUpdateLevelToForce, u16 timeToForce)
{
    if(m_TimeToForceLowestUpdateLevel == 0 || lowestUpdateLevelToForce > m_LowestUpdateLevelToForce)
    {
        m_LowestUpdateLevelToForce     = lowestUpdateLevelToForce;
        m_TimeToForceLowestUpdateLevel = timeToForce;
    }
}

CScriptEntityExtension* CNetObjPhysical::FindScriptExtension() const
{
    CPhysical* pPhysical = static_cast<CPhysical*>(GetEntity());
	VALIDATE_GAME_OBJECT(pPhysical);

    if (pPhysical)
    {
        return pPhysical->GetExtension<CScriptEntityExtension>();
    }

    return NULL;
}

void CNetObjPhysical::UpdateFixedByNetworkInAirChecks(bool &fixObject, unsigned &reason)
{
    CPhysical *pPhysical = GetPhysical();

    if(pPhysical)
    {
        if(fixObject && !pPhysical->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
        {
            m_LastTimeFixed       = fwTimer::GetSystemTimeInMilliseconds();
            m_InAirCheckPerformed = false;
        }

        if(m_FixedByNetworkInAirCheckData.m_ObjectID == GetObjectID())
        {
            if(!fixObject || IsClone())
            {
#if ENABLE_NETWORK_LOGGING
                NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "STOPPING_IN_AIR_CHECK", GetLogName());
                NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", "%s", IsClone() ? "Is Now Clone" : "Is no longer fixed");
#endif // ENABLE_NETWORK_LOGGING

                m_FixedByNetworkInAirCheckData.Reset();
            }
            else
            {
                if(!m_FixedByNetworkInAirCheckData.m_ProbeSubmitted)
                {
                    if(HasCollisionLoadedUnderEntity())
                    {
                        Vector3 probeStartPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());

                        m_FixedByNetworkInAirCheckData.m_TargetZ = probeStartPosition.z;

                        WorldProbe::CShapeTestProbeDesc probeDesc;
	                    WorldProbe::CShapeTestFixedResults<> probeResult;
	                    probeDesc.SetStartAndEnd(probeStartPosition, Vector3(probeStartPosition.x, probeStartPosition.y, -200.0f));
	                    probeDesc.SetResultsStructure(&probeResult);
	                    probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	                    probeDesc.SetContext(WorldProbe::ENetwork);

#if ENABLE_NETWORK_LOGGING
                        NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "IN_AIR_CHECK_PROBE", GetLogName());
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Probe Start", "%.2f, %.2f, %.2f", probeStartPosition.x, probeStartPosition.y, probeStartPosition.z);
#endif // ENABLE_NETWORK_LOGGING

                        if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
                        {
#if ENABLE_NETWORK_LOGGING
                            NetworkInterface::GetObjectManagerLog().WriteDataValue("Probe Result", "Succeeded");
                            NetworkInterface::GetObjectManagerLog().WriteDataValue("Hit Position Z", "%.2f", probeResult[0].GetHitPosition().z);
#endif // ENABLE_NETWORK_LOGGING

                            m_FixedByNetworkInAirCheckData.m_TargetZ = probeResult[0].GetHitPosition().z;

                            float waterLevel = m_FixedByNetworkInAirCheckData.m_TargetZ;

                            if(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(probeStartPosition, &waterLevel))
                            {
                                LOGGING_ONLY(NetworkInterface::GetObjectManagerLog().WriteDataValue("Water Level Z", "%.2f", waterLevel));

                                if(waterLevel > m_FixedByNetworkInAirCheckData.m_TargetZ)
                                {
                                    m_FixedByNetworkInAirCheckData.m_TargetZ = waterLevel;
                                }
                            }

                            LOGGING_ONLY(NetworkInterface::GetObjectManagerLog().WriteDataValue("Target Z", "%.2f", m_FixedByNetworkInAirCheckData.m_TargetZ));
                        }
                        else
                        {
                            LOGGING_ONLY(NetworkInterface::GetObjectManagerLog().WriteDataValue("Probe Result", "Failed"));
                        }

                        m_FixedByNetworkInAirCheckData.m_ProbeSubmitted       = true;
                        m_FixedByNetworkInAirCheckData.m_TimeZMovedSinceProbe = fwTimer::GetSystemTimeInMilliseconds();
                    }
                }
                else
                {
                    bool inAirCheckPerformed = m_InAirCheckPerformed;

                    Vector3 position = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());

                    bank_float SUCCESS_THRESHOLD = 0.5f;

                    float positionToStop = m_FixedByNetworkInAirCheckData.m_TargetZ + pPhysical->GetBoundRadius() + SUCCESS_THRESHOLD;

                    if(position.z <= positionToStop || pPhysical->GetIsInWater())
                    {
#if ENABLE_NETWORK_LOGGING
                        NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "STOPPING_IN_AIR_CHECK", GetLogName());
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", "Reached Target");
#endif // ENABLE_NETWORK_LOGGING

                        m_FixedByNetworkInAirCheckData.Reset();

                        inAirCheckPerformed = true;
                    }
                    else if(pPhysical->GetVelocity().z < -0.1f)
                    {
                        m_FixedByNetworkInAirCheckData.m_TimeZMovedSinceProbe = fwTimer::GetSystemTimeInMilliseconds();
                    }
                    else
                    {
                        const unsigned TIME_TO_CORRECT = 5000;

                        unsigned currTime = fwTimer::GetSystemTimeInMilliseconds();

                        if((currTime - m_FixedByNetworkInAirCheckData.m_TimeZMovedSinceProbe) > TIME_TO_CORRECT)
                        {
#if ENABLE_NETWORK_LOGGING
                            NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "STOPPING_IN_AIR_CHECK", GetLogName());
                            NetworkInterface::GetObjectManagerLog().WriteDataValue("Reason", "Not Moved Too Long");
#endif // ENABLE_NETWORK_LOGGING

                            m_FixedByNetworkInAirCheckData.Reset();

                            inAirCheckPerformed = true;
                        }
                    }

                    m_InAirCheckPerformed = inAirCheckPerformed;

                    // try and get the object to fall
                    fixObject = inAirCheckPerformed;

					if (fixObject)
					{
						reason = FBN_IN_AIR;
					}
                }
            }
        }
        else if(m_FixedByNetworkInAirCheckData.m_ObjectID == NETWORK_INVALID_OBJECT_ID)
        {
            if(fixObject && !IsClone() && !m_InAirCheckPerformed)
            {
                const unsigned IN_AIR_CHECK_DELAY = 5000;

                unsigned currTime = fwTimer::GetSystemTimeInMilliseconds();

                if((currTime - m_LastTimeFixed) > IN_AIR_CHECK_DELAY)
                {
                    if(ShouldDoFixedByNetworkInAirCheck())
                    {
#if ENABLE_NETWORK_LOGGING
                        NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "STARTING_IN_AIR_CHECK", GetLogName());
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Object", "%s", GetLogName());
#endif // ENABLE_NETWORK_LOGGING

                        m_FixedByNetworkInAirCheckData.m_ObjectID             = GetObjectID();
                        m_FixedByNetworkInAirCheckData.m_ProbeSubmitted       = false;
                        m_FixedByNetworkInAirCheckData.m_TargetZ              = 0.0f;
                        m_FixedByNetworkInAirCheckData.m_TimeZMovedSinceProbe = 0;
                    }
                    else
                    {
                        m_InAirCheckPerformed = true;
                    }
                }
            }
        }
    }
}

void CNetObjPhysical::CheckPredictedPositionErrors()
{
    gnetAssertf(!IsClone(), "Checking predicted position errors for a clone!");

    CPhysical *physical = GetPhysical();

    if(physical && SendUpdatesBasedOnPredictedPositionErrors())
    {
        CPhysicalSyncTreeBase *syncTree = SafeCast(CPhysicalSyncTreeBase, GetSyncTree());

        if(syncTree)
        {
            Vector3          currentPosition = VEC3V_TO_VECTOR3(physical->GetTransform().GetPosition());
            Vector3          currentVelocity = physical->GetVelocity();
            netSyncDataNode *velocityNode    = syncTree->GetVelocityNode();
            netSyncDataNode *orientationNode = syncTree->GetOrientationNode();

            bool forceUpdate[CNetworkSyncDataULBase::NUM_UPDATE_LEVELS];

            u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();

            // we only need to predict for the minimum update level we are sending update for
            // and only for the lower update levels, higher update rates should be sending the
            // position updates frequently enough already
            u8 updateLevelToStopPredicting = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;

            if(HasBeenCloned())
            {
                updateLevelToStopPredicting = rage::Min((u8)(GetMinimumUpdateLevel()+1), (u8)CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH);
            }

            bool checkForForcedUpdates = false;

            for(unsigned index = 0; index < CNetworkSyncDataULBase::NUM_UPDATE_LEVELS; index++)
            {
                forceUpdate[index] = false;

                SnapshotData *snapshotData = GetSnapshotData(index);

                if(snapshotData)
                {
                    // only check predicted errors for update levels we need to
                    if(index >= updateLevelToStopPredicting)
                    {
                        snapshotData->Reset(currentPosition, currentVelocity, currentTime);
                    }
                    else
                    {
                        // check if the update frequency has elapsed since the last snapshot time, if so the
                        // data will be sent this update anyway
                        u32 updateFrequency = velocityNode->GetUpdateFrequency(index);

                        if((currentTime - snapshotData->m_Timestamp) > updateFrequency && IsInCurrentUpdateBatch())
                        {
                            snapshotData->Reset(currentPosition, currentVelocity, currentTime);
                        }
                        else
                        {
                            // check if the object is too far away from where a remote machine will predict it to be
                            float timeElapsed = 0.0f;

                            if(currentTime > snapshotData->m_Timestamp)
                            {
                                timeElapsed = (currentTime - snapshotData->m_Timestamp) / 1000.0f;
                            }

                            Vector3 predictedPosition       = snapshotData->m_Position + (snapshotData->m_Velocity * timeElapsed);
                            float   maxPosErrorBeforeUpdate = GetPredictedPosErrorThresholdSqr(index);

                            if((predictedPosition - currentPosition).Mag2() >= maxPosErrorBeforeUpdate)
                            {
                                forceUpdate[index] = true;

                                snapshotData->Reset(currentPosition, currentVelocity, currentTime);

                                checkForForcedUpdates = true;
                            }
                        }
                    }
                }
            }

            // if the update level is wrong force an update and correct it,
            // this can be delayed due to object batching
            u8 lowestAllowedUpdateLevel = GetLowestAllowedUpdateLevel();
            u8 minimumUpdateLevel       = GetMinimumUpdateLevel();

            if(minimumUpdateLevel < CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH)
            {
                if(lowestAllowedUpdateLevel > minimumUpdateLevel)
                {
                    forceUpdate[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW] = true;
                    forceUpdate[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW]      = true;
                    forceUpdate[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM]   = true;
                    checkForForcedUpdates                                      = true;
                }
            }

            // we also want to force send to any players the vehicle has been newly cloned upon
            PlayerFlags clonedState = GetClonedState();

            if(clonedState != m_LastCloneStateCheckedForPredictErrors)
            {
                checkForForcedUpdates = true;
            }

            // we also want to force send to all players at lower update levels when the vehicle has stopped this frame
            if(physical->GetVelocity().Mag2() < 0.001f && !m_ZeroVelocityLastFrame)
            {
                forceUpdate[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW] = true;
                forceUpdate[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW]      = true;
                forceUpdate[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM]   = true;
                checkForForcedUpdates                                      = true;
            }

            if(checkForForcedUpdates)
            {
                ActivationFlags activationFlags = GetActivationFlags();

                unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	            for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
                {
		            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

                    PhysicalPlayerIndex physicalPlayerIndex = player->GetPhysicalPlayerIndex();

                    if(gnetVerifyf(physicalPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Invalid physical player index!"))
                    {
                        if(HasBeenCloned(*player))
                        {
                            if((m_LastCloneStateCheckedForPredictErrors & (1 << physicalPlayerIndex)) == 0)
                            {
                                static const float LOW_SPEED_THRESHOLD_SQR = 9.0f;

                                if(currentVelocity.Mag2() > LOW_SPEED_THRESHOLD_SQR)
                                {
                                    syncTree->ForceSendOfPositionDataToPlayer(*this, activationFlags, physicalPlayerIndex);
                                    syncTree->ForceSendOfNodeDataToPlayer(physicalPlayerIndex, SERIALISEMODE_UPDATE, activationFlags, this, *velocityNode);
                                    syncTree->ForceSendOfNodeDataToPlayer(physicalPlayerIndex, SERIALISEMODE_UPDATE, activationFlags, this, *orientationNode);
                                }
                            }
                            else
                            {
                                u32 updateLevel = GetUpdateLevel(physicalPlayerIndex);

                                if(gnetVerifyf(updateLevel < CNetworkSyncDataULBase::NUM_UPDATE_LEVELS, "Invalid update level!"))
                                {
                                    if(forceUpdate[updateLevel])
                                    {
                                        syncTree->ForceSendOfPositionDataToPlayer(*this, activationFlags, physicalPlayerIndex);
                                        syncTree->ForceSendOfNodeDataToPlayer(physicalPlayerIndex, SERIALISEMODE_UPDATE, activationFlags, this, *velocityNode);
                                        syncTree->ForceSendOfNodeDataToPlayer(physicalPlayerIndex, SERIALISEMODE_UPDATE, activationFlags, this, *orientationNode);
                                    }
                                }
                            }

                            // ensure the minimum update level is being respected, object batching can delay this
                            if(GetUpdateLevel(physicalPlayerIndex) < lowestAllowedUpdateLevel)
                            {
                                SetUpdateLevel(physicalPlayerIndex, lowestAllowedUpdateLevel);
                            }
                        }
                    }
                }
            }

            m_LastCloneStateCheckedForPredictErrors = clonedState;
        }
    }
}

void CNetObjPhysical::UpdateForceSendRequests()
{
	CPhysical *pPhysical = GetPhysical();
	VALIDATE_GAME_OBJECT(pPhysical);

	if(pPhysical)
	{
		u8 frameCount = static_cast<u8>(fwTimer::GetSystemFrameCount());

		if(frameCount != m_FrameForceSendRequested)
		{
			if(m_bForceSendPhysicalAttachVelocityNode)
			{
				m_bForceSendPhysicalAttachVelocityNode = false;

				// force an immediate update of the attach and velocity nodes
				GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());

				if (gnetVerifyf(pPhysical->GetIsTypeObject(),"%s only expect this to be used on objects", GetLogName()) )
				{
					GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPhysicalSyncTreeBase, GetSyncTree())->GetVelocityNode());
					GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this, *SafeCast(CPhysicalSyncTreeBase, GetSyncTree())->GetPhysicalAttachNode());
				}
			}
		}
	}
}

#if __BANK

bool CNetObjPhysical::ShouldDisplayAdditionalDebugInfo() 
{
	if (NetworkDebug::GetDebugDisplaySettings().m_displayVisibilityInfo || NetworkDebug::GetDebugDisplaySettings().m_displayGhostInfo)
	{
		return true;
	}

	return CNetObjDynamicEntity::ShouldDisplayAdditionalDebugInfo();
}

// Name         :   DisplayNetworkInfoForObject
// Purpose      :   Displays debug info above the network object, players always have their name displayed
//                  above them, if the debug flag is set then all network objects have debug info displayed
//                  above them
void CNetObjPhysical::DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement)
{
	CNetObjDynamicEntity::DisplayNetworkInfoForObject(col, scale, screenCoords, debugTextYIncrement);

    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    const unsigned int MAX_STRING = 50;
    char str[MAX_STRING];

    CPhysical *physical = GetPhysical();
	VALIDATE_GAME_OBJECT(physical);

    if(!physical)
        return;

    if(displaySettings.m_displayInformation.m_displayVelocity)
    {
        sprintf(str, "%.2f, %.2f, %.2f", physical->GetVelocity().x, physical->GetVelocity().y, physical->GetVelocity().z);
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;

        netBlenderLinInterp *netBlender = static_cast<netBlenderLinInterp *>(GetNetBlender());
        Vector3 blendTarget = netBlender->GetLastVelocityReceived();
        sprintf(str, "(%.2f, %.2f, %.2f)", blendTarget.x, blendTarget.y, blendTarget.z);
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;

        sprintf(str, "Speed (%.2f)", physical->GetVelocity().Mag());
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;
    }

    if(displaySettings.m_displayInformation.m_displayAngVelocity)
    {
        sprintf(str, "%.2f, %.2f, %.2f", physical->GetAngVelocity().x, physical->GetAngVelocity().y, physical->GetAngVelocity().z);
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;

        CNetBlenderPhysical *netBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());
        Vector3 blendTarget = netBlender->GetLastAngVelocityReceived();
        sprintf(str, "(%.2f, %.2f, %.2f)", blendTarget.x, blendTarget.y, blendTarget.z);
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;
    }

    if(displaySettings.m_displayInformation.m_displayDamageTrackers)
    {
        if(IsTrackingDamage())
        {
            float fCurrX = screenCoords.x;

            // need to set up a text format to calculate string widths
            CTextLayout damageTrackerText;
            NetworkUtils::SetFontSettingsForNetworkOHD(&damageTrackerText, scale, false);

            unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                NetworkColours::NetworkColour tempColour = NetworkUtils::GetDebugColourForPlayer(player);
                Color32 damageColour = NetworkColours::GetNetworkColour(tempColour);

                Vector2 vTextRenderPos(fCurrX, screenCoords.y);

                formatf(str, "%.2f ", GetDamageDealtByPlayer(*player));
                DLC ( CDrawNetworkDebugOHD_NY, (vTextRenderPos, damageColour, scale, str));

                fCurrX += damageTrackerText.GetStringWidthOnScreen(str, true);
            }

            screenCoords.y += debugTextYIncrement;
        }
    }

    if(displaySettings.m_displayInformation.m_displayCollisionTimer)
    {
        if(IsClone())
        {
            sprintf(str, "Collision timer: %d", GetCollisionTimer());
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
            screenCoords.y += debugTextYIncrement;
        }
    }

    if(displaySettings.m_displayInformation.m_displayFixedByNetwork)
    {
        sprintf(str, "Fixed by network: %s", physical->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) ? "Yes" : "No");
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;
    }

	if(displaySettings.m_displayVisibilityInfo)
	{
		bool visible, collideable;

		if (IsLocalEntityVisibilityOverriden(visible) && !visible)
		{
			snprintf(str, MAX_STRING, "Local visibility is overridden");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;
		}

		if (GetOverridingRemoteVisibility())
		{
			snprintf(str, MAX_STRING, "Remote visibility is overridden");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;
		}

		if (IsLocalEntityCollisionOverriden(collideable) && !collideable)
		{
			snprintf(str, MAX_STRING, "Local collision is overridden");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;
		}

		if (GetOverridingRemoteCollision())
		{
			snprintf(str, MAX_STRING, "Remote collision is overridden");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;
		}

		if (GetObjectType() == NET_OBJ_TYPE_PLAYER && static_cast<CNetObjPlayer*>(this)->IsConcealed())
		{
			snprintf(str, MAX_STRING, "Player is concealed");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;
		}

		if (m_bAlphaFading)
		{
			snprintf(str, MAX_STRING, "Entity is alpha faded (alpha : %d)", m_iAlphaOverride);
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;	
		}

		if (m_bAlphaRamping)
		{
			snprintf(str, MAX_STRING, "Entity is alpha ramping (alpha : %d)", m_iAlphaOverride);
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;	
		}

		for (u32 module=0; module<NUM_VISIBLE_MODULES; module++)
		{
			if (!physical->GetIsVisibleForModule((eIsVisibleModule)module))
			{
				switch (module)
				{
				case SETISVISIBLE_MODULE_DEBUG:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_DEBUG is false");
					break;
				case SETISVISIBLE_MODULE_CAMERA:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_CAMERA is false");
					break;
				case SETISVISIBLE_MODULE_SCRIPT:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_SCRIPT is false");
					break;
				case SETISVISIBLE_MODULE_CUTSCENE:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_CUTSCENE is false");
					break;
				case SETISVISIBLE_MODULE_GAMEPLAY:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_GAMEPLAY is false");
					break;
				case SETISVISIBLE_MODULE_FRONTEND:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_FRONTEND is false");
					break;
				case SETISVISIBLE_MODULE_VFX:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_VFX is false");
					break;
				case SETISVISIBLE_MODULE_NETWORK:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_NETWORK is false");
					break;
				case SETISVISIBLE_MODULE_PHYSICS:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_PHYSICS is false");
					break;
				case SETISVISIBLE_MODULE_WORLD:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_WORLD is false");
					break;
				case SETISVISIBLE_MODULE_DUMMY_CONVERSION:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_DUMMY_CONVERSION is false");
					break;
				case SETISVISIBLE_MODULE_PLAYER:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_PLAYER is false");
					break;
				case SETISVISIBLE_MODULE_PICKUP:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_PICKUP is false");
					break;
				case SETISVISIBLE_MODULE_FIRST_PERSON:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_FIRST_PERSON is false");
					break;
				case SETISVISIBLE_MODULE_VEHICLE_RESPOT:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_VEHICLE_RESPOT is false");
					break;
				case SETISVISIBLE_MODULE_RESPAWN:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_RESPAWN is false");
					break;
				case SETISVISIBLE_MODULE_REPLAY:
					snprintf(str, MAX_STRING, "SETISVISIBLE_MODULE_REPLAY is false");
					break;
				default:
					snprintf(str, MAX_STRING, "**UNRECOGNISED MODULE** is false");
					break;
				}

				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
				screenCoords.y += debugTextYIncrement;
			}
		}
	}
	
	if (displaySettings.m_displayGhostInfo)
	{
		if (m_bGhost)
		{
			snprintf(str, MAX_STRING, "Entity is ghost");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;	
		}

		if (m_bGhostedForGhostPlayers)
		{
			snprintf(str, MAX_STRING, "Ghosted for ghost players");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;	
		}

		if (m_GhostCollisionSlot != CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT)
		{
			snprintf(str, MAX_STRING, "Entity has ghost collision slot %d", m_GhostCollisionSlot);
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
			screenCoords.y += debugTextYIncrement;	
		}
	}
}

void CNetObjPhysical::GetOrientationDisplayString(char *buffer) const
{
    if(buffer)
    {
        if(GetPhysical())
        {
			Matrix34 m = MAT34V_TO_MATRIX34(GetPhysical()->GetMatrix());
            const Vector3 eulers = m.GetEulers();

            CNetBlenderPhysical *netBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());
            const Vector3 lastEulers = netBlender->GetLastMatrixReceived().GetEulers();
            sprintf(buffer, "%.2f, %.2f, %.2f (%.2f, %.2f, %.2f)", eulers.x, eulers.y, eulers.z, lastEulers.x, lastEulers.y, lastEulers.z);
        }
    }
}

void CNetObjPhysical::GetHealthDisplayString(char *buffer) const
{
    if(buffer)
    {
        if(GetPhysical())
        {
            sprintf(buffer, "%.2f", GetPhysical()->GetHealth());
        }
    }
}

#endif // __BANK

bool CNetObjPhysical::CanBlendWhenFixed() const
{
	return false;
}

void CNetObjPhysical::ResetBlenderDataWhenAttached()
{
    if(GetNetBlender() && !IsConcealed())
    {
        GetNetBlender()->Reset();
    }
}
