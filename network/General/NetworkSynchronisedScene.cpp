//
// name:		NetworkSynchronisedScene.cpp
// description:	Support for running synchronised scenes in network games. A synchronised scene is a method
//              of running animations involving multiple entities that have animations that need to match up
//              (e.g. two peds walking up to one another and shaking hands). In network games these scenes may
//              involve entities owned by different machines in the session that need to be coordinated.
// written by:	Daniel Yelland
//

#include "network/General/NetworkSynchronisedScene.h"

#include "fwanimation/directorcomponentsyncedscene.h"

#include "animation/Move.h"
#include "camera/CamInterface.h"
#include "Camera/Scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "Debug/DebugScene.h"
#include "Event/Events.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "Script/ScriptTaskTypes.h"
#include "Task/Animation/TaskAnims.h"
#include "task/General/TaskBasic.h"
#include "vehicleai/VehicleIntelligence.h"
#include "vehicles/Train.h"

NETWORK_OPTIMISATIONS()

#if ENABLE_NETWORK_LOGGING
static bool gb_NoCallstackDumpOnPedAbort = false;
#endif // #if ENABLE_NETWORK_LOGGING

CNetworkSynchronisedScenes::SceneDescription CNetworkSynchronisedScenes::ms_SyncedScenes[CNetworkSynchronisedScenes::MAX_NUM_SCENES];

bool CNetworkSynchronisedScenes::ms_AllowRemoteSyncSceneLocalPlayerRequests = false;

void CNetworkSynchronisedScenes::Init()
{
    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        ms_SyncedScenes[index].Reset();
    }
}

void CNetworkSynchronisedScenes::Shutdown()
{
    // need to restore the game heap here as shutting down the synced scene can deallocate from the heap
	sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        SceneDescription &sceneData = ms_SyncedScenes[index];

        if(sceneData.m_Started)
        {
            StopLocalSynchronisedScene(sceneData, "Shutting Down");
        }
    }

    sysMemAllocator::SetCurrent(*oldHeap);
}

void CNetworkSynchronisedScenes::Update()
{
    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        SceneDescription &sceneData = ms_SyncedScenes[index];

        if(sceneData.m_PendingStart)
        {
            // wait for any peds involved in the scene to finish migrating or become detached
            bool pedsPendingOwnerChange = false;
            bool objectsStillAttached   = false;
            for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE && !pedsPendingOwnerChange && !objectsStillAttached; pedIndex++)
            {
                PedData &pedData = sceneData.m_PedData[pedIndex];

                if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject *pedObject = NetworkInterface::GetObjectManager().GetNetworkObject(pedData.m_PedID);
                    if(pedObject)
                    {
                        CNetObjPed *netObjPed = SafeCast(CNetObjPed, pedObject);
                        netObjPed->PreventTaskUpdateThisFrame();

                        if(pedObject->IsPendingOwnerChange())
                        {
                            pedsPendingOwnerChange = true;
                            gnetDebug1("Can't start pending synced scene because %s is migrating!", pedObject->GetLogName());
                        }
						else
                        {
							eSyncedSceneFlagsBitSet &sceneFlags = reinterpret_cast<eSyncedSceneFlagsBitSet&>(pedData.m_Flags);

							if(!sceneFlags.BitSet().IsSet(SYNCED_SCENE_NET_DISREGARD_ATTACHMENT_CHECKS))
							{
								CPed *ped = NetworkUtils::GetPedFromNetworkObject(pedObject);

								if(ped && ped->GetIsAttached())
								{
									// allow peds attached due to sitting inside a car to continue, technically this is wrong
									// but the scripts rely on this
									if(!ped->GetIsAttachedInCar())
									{
										objectsStillAttached = true;
										gnetDebug1("Can't start pending synced scene because %s is attached!", pedObject->GetLogName());
									}
								}
							}
                        }
                    }
                }
            }

            // wait for any non-peds involved in the scene to become detached
            for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE && !objectsStillAttached; nonPedIndex++)
            {
                NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

                if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject *nonPedObject = NetworkInterface::GetObjectManager().GetNetworkObject(nonPedEntityData.m_EntityID);

                    CPhysical *physical = (nonPedObject && nonPedObject->GetEntity() && nonPedObject->GetEntity()->GetIsPhysical()) ? static_cast<CPhysical *>(nonPedObject->GetEntity()) : 0;

                    if(physical && physical->GetIsAttached())
                    {
                        objectsStillAttached = true;
                        gnetDebug1("Can't start pending synced scene because %s is attached!", nonPedObject->GetLogName());
                    }
                }
            }

            bool attachEntityValid = true;

            if(sceneData.m_AttachToEntityID != NETWORK_INVALID_OBJECT_ID)
            {
                if(!NetworkInterface::GetObjectManager().GetNetworkObject(sceneData.m_AttachToEntityID))
                {
                    attachEntityValid = false;
                    gnetDebug1("Can't start pending synced scene because attach entity (%d) isn't on this machine!", sceneData.m_AttachToEntityID);
                }
            }

            if(!pedsPendingOwnerChange && !objectsStillAttached && attachEntityValid)
            {
                // wait for the anims to be loaded
                if(sceneData.m_ClipRequestHelper.HaveClipsBeenLoaded() && HaveNonPedModelsBeenLoaded(sceneData))
                {
                    StartLocalSynchronisedScene(sceneData);
                }
                else if(sceneData.m_ClipRequestHelper.GetCurrentlyRequiredDictIndex() == -1)
                {
                    gnetAssertf(0, "Synced scene animations have been unexpectedly released!");

                    StopLocalSynchronisedScene(sceneData, "Anims Released Unexpectedly!");
                }
                else
                {
                    gnetDebug1("Can't start pending synced scene - waiting for assets to stream in!");
                }
            }
        }
        else if(sceneData.m_Started)
        {
            bool stopScene = false;

            const char *stopReason = "";

            // Check for valid scene, scenes can become invalid if all entities in the scene
            // are removed (which can happen in a single frame if the player is warping across the map)
			if(!fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID))
		    {
                LOGGING_ONLY(stopReason = "Anim Director Scene Not Valid");
                stopScene = true;
            }
            else if(sceneData.m_PendingStop)
            {
                LOGGING_ONLY(stopReason = "Pending Stop");
                stopScene = true;
            }
            else
            {
                for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE && !stopScene; pedIndex++)
                {
                    PedData &pedData = sceneData.m_PedData[pedIndex];

                    if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
                    {
                        netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(pedData.m_PedID);

                        // if any of the peds involved in the scene are removed from the game we have to quit the scene
                        if(!networkObject)
                        {
                            LOGGING_ONLY(stopReason = "Involved network ped no longer exists!");
                            stopScene = true;
                        }
                        else if(networkObject->IsClone())
                        {
                            // ensure all involved clone peds are still part of the scene
                            CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

                            if(!ped)
                            {
                                LOGGING_ONLY(stopReason = "Involved ped no longer exists!");
                                stopScene = true;
                            }
                            else
                            {
                                if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SYNCHRONIZED_SCENE, true) &&
                                   ped->GetPedIntelligence()->GetQueriableInterface()->GetNetworkSynchronisedSceneID() == sceneData.m_NetworkSceneID)
                                {  
                                    pedData.m_WaitingForTaskUpdate = false;
                                }
                                else if(!pedData.m_WaitingForTaskUpdate)
                                {
                                    bool isPedPartOfPendingSyncedScene = IsPedPartOfPendingSyncedScene(pedData.m_PedID);

                                    if(!isPedPartOfPendingSyncedScene && !CStartNetworkSyncedSceneEvent::IsSceneStartEventPending(sceneData.m_NetworkSceneID))
                                    {
#if ENABLE_NETWORK_LOGGING
                                        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STOP_LOCAL_SYNC_SCENE_ON_PED", "");
                                        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped", "%s", ped->GetNetworkObject()->GetLogName());
                                        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "No longer running scene on owner");
#endif // #if ENABLE_NETWORK_LOGGING
                                        StopLocalSyncedSceneOnPed(pedData);
                                    }
                                    else
                                    {
                                        pedData.m_WaitingForTaskUpdate = true;
                                    }
                                }
                            }
                        }
                    }
                }

                // if any of the non-ped entities in the scene are removed from the game we have to quit the scene
                for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
                {
                    NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

                    if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
                    {
                        if(!NetworkInterface::GetObjectManager().GetNetworkObject(nonPedEntityData.m_EntityID))
                        {
                            LOGGING_ONLY(stopReason = "Involved non-ped no longer exists!");
                            stopScene = true;
                        }
                    }
                }

                // the scene may have become invalid after removing the peds above so it need to be checked again here
                if(!fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID))
                {
                    LOGGING_ONLY(stopReason = "Anim Director Scene Not Valid");
                    stopScene = true;
                }
                else
                {
                    // if the scene has completed finish it now
				    sceneData.m_Phase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(sceneData.m_LocalSceneID);

                    if(sceneData.m_Phase >= sceneData.m_PhaseToStopScene)
                    {
                        bool isLooped      = fwAnimDirectorComponentSyncedScene::IsSyncedSceneLooped(sceneData.m_LocalSceneID);
                        bool holdLastFrame = fwAnimDirectorComponentSyncedScene::IsSyncedSceneHoldLastFrame(sceneData.m_LocalSceneID);

                        if(!isLooped && !holdLastFrame)
                        {
                            LOGGING_ONLY(stopReason = "Reached end of scene");
                            stopScene = true;
                        }
                    }
                }
		    }

            if(stopScene)
            {
                StopLocalSynchronisedScene(sceneData, stopReason);
            }
        }
    }
}

bool CNetworkSynchronisedScenes::IsSceneActive(int networkSceneID)
{
    bool sceneRunning = false;

    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(sceneDescription && sceneDescription->m_Active)
    {
        sceneRunning = true;
    }

    return sceneRunning;
}

int CNetworkSynchronisedScenes::GetLocalSceneID(int networkSceneID)
{
    int localSceneID = -1;

    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(gnetVerifyf(sceneDescription, "Checking whether a network synced scene is active with an invalid scene ID (%d)!", networkSceneID))
    {
        if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneDescription->m_LocalSceneID))
        {
            localSceneID = sceneDescription->m_LocalSceneID;
        }
    }

    return localSceneID;
}

int CNetworkSynchronisedScenes::GetNetworkSceneID(int localSceneID)
{
    int networkSceneID = -1;

	if(fwAnimDirectorComponentSyncedScene::IsValidSceneId((fwSyncedSceneId)localSceneID))
	{
		SceneDescription *sceneDescription = GetSceneDescriptionFromLocalID(localSceneID);

		if(sceneDescription)
		{
			networkSceneID = sceneDescription->m_NetworkSceneID;
		}
	}

    return networkSceneID;
}

unsigned CNetworkSynchronisedScenes::GetTimeSceneStarted(int networkSceneID)
{
    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(gnetVerifyf(sceneDescription, "Trying to get the time a network synced scene started with an invalid scene ID (%d)!", networkSceneID))
    {
        return sceneDescription->m_NetworkTimeStarted;
    }

    return 0;
}

bool CNetworkSynchronisedScenes::IsInScope(const netPlayer &player, int networkSceneID)
{
    float SCENE_SCOPE_DISTANCE_SQUARED = rage::square(200.0f);

    const CNetGamePlayer *netGamePlayer = SafeCast(const CNetGamePlayer, &player);

    if(!player.IsLocal())
    {
        SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

        if(sceneDescription == 0)
        {
            gnetDebug1("Synced scene %d is out scope with %s because the scene data does not exist!", networkSceneID, player.GetLogName());
            return false;
        }
        else
        {
            if(netGamePlayer->IsInDifferentTutorialSession())
            {
                gnetDebug1("Synced scene %d is out scope with %s because they are in a different tutorial session!", networkSceneID, player.GetLogName());
                return false;
            }
            else
            {
			    const CPed* pPlayerPed = netGamePlayer->GetPlayerPed();

				if(pPlayerPed && !(pPlayerPed->GetNetworkObject() && static_cast<CNetObjPhysical*>(pPlayerPed->GetNetworkObject())->IsConcealed()))
				{
					SceneDescription &sceneData = *sceneDescription;

					Vector3 playerPos = NetworkInterface::GetPlayerFocusPosition(player, NULL, true);
					Vector3 scenePos  = sceneData.m_ScenePosition;

					// if we have an attach entity the scene position is an offset from that entity
					if(sceneData.m_AttachToEntityID != NETWORK_INVALID_OBJECT_ID)
					{
						netObject *networkObject = NetworkInterface::GetNetworkObject(sceneData.m_AttachToEntityID);
						CEntity   *attachEntity  = networkObject ? networkObject->GetEntity() : 0;

						if(attachEntity)
						{
							scenePos += VEC3V_TO_VECTOR3(attachEntity->GetTransform().GetPosition());
						}
					}

					// ensure we still know about all network objects involved in the scene locally
					// before flagging the scene as in scope
					bool allEntitiesExist = true;
					OUTPUT_ONLY(ObjectId firstMissingEntity = NETWORK_INVALID_OBJECT_ID);

					for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE && allEntitiesExist; pedIndex++)
					{
						PedData &pedData = sceneData.m_PedData[pedIndex];

						if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
						{
							CNetObjGame *pPedNetObject = SafeCast(CNetObjGame, NetworkInterface::GetNetworkObject(pedData.m_PedID));
							if(!pPedNetObject)
							{
								OUTPUT_ONLY(firstMissingEntity = pedData.m_PedID);
								allEntitiesExist = false;
							}
							else if (pPlayerPed && pPedNetObject->GetEntity() && pPedNetObject->GetEntity()->GetIsInInterior() && !pPedNetObject->IsScriptObject())
							{
								float fNewScopeRange = rage::square(pPedNetObject->GetScopeData().m_scopeDistanceInterior);

								if( SCENE_SCOPE_DISTANCE_SQUARED > fNewScopeRange &&
									!pPlayerPed->GetInteriorLocation().IsSameLocation(pPedNetObject->GetEntity()->GetInteriorLocation()) )
								{
									SCENE_SCOPE_DISTANCE_SQUARED = fNewScopeRange;
								}
							}
						}
					}

					// if any of the non-ped entities in the scene are removed from the game we have to quit the scene
					for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE && allEntitiesExist; nonPedIndex++)
					{
						NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

						if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
						{
                            CNetObjGame *pNonPedNetObject = SafeCast(CNetObjGame, NetworkInterface::GetObjectManager().GetNetworkObject(nonPedEntityData.m_EntityID));
							if(!pNonPedNetObject)
							{
								OUTPUT_ONLY(firstMissingEntity = nonPedEntityData.m_EntityID);
								allEntitiesExist = false;
							}
							else if (pPlayerPed && pNonPedNetObject->GetEntity() && pNonPedNetObject->GetEntity()->GetIsInInterior() && !pNonPedNetObject->IsScriptObject())
							{
								float fNewScopeRange = rage::square(pNonPedNetObject->GetScopeData().m_scopeDistanceInterior);

								if( SCENE_SCOPE_DISTANCE_SQUARED > fNewScopeRange &&
									!pPlayerPed->GetInteriorLocation().IsSameLocation(pNonPedNetObject->GetEntity()->GetInteriorLocation()) )
								{
									SCENE_SCOPE_DISTANCE_SQUARED = fNewScopeRange;
								}
							}
						}
					}

					if(!allEntitiesExist)
					{
						gnetDebug1("Synced scene %d is out of scope with %s running because entity %d isn't on this machine!", networkSceneID, player.GetLogName(), firstMissingEntity);
						return false;
					}

					Vector3 delta = playerPos - scenePos;

					if(delta.XYMag2() <= SCENE_SCOPE_DISTANCE_SQUARED)
					{
						gnetDebug1("Synced scene %d is in scope with %s because ped position is within the XY scope distance Player Dist:%.2fm Scope Dist:%.2fm!", networkSceneID, player.GetLogName(), delta.XYMag(), rage::Sqrtf(SCENE_SCOPE_DISTANCE_SQUARED));
						return true;
					}
					else
					{
                        // if the player pos is out of scope, check the focus pos if it is different
                        Vector3 focusPos = NetworkInterface::GetPlayerFocusPosition(player);

                        if(!playerPos.IsClose(focusPos, 0.01f))
                        {
                            delta = focusPos - scenePos;

                            if(delta.XYMag2() <= SCENE_SCOPE_DISTANCE_SQUARED)
                            {
                                gnetDebug1("Synced scene %d is in scope with %s because focus position is within the XY scope distance Player Dist:%.2fm Scope Dist:%.2fm!", networkSceneID, player.GetLogName(), delta.XYMag(), rage::Sqrtf(SCENE_SCOPE_DISTANCE_SQUARED));
                                return true;
                            }
                        }

						// check if the player's ped or spectator target is part of the scene, if so they are always in scope
						netObject *playerObject   = netGamePlayer->GetPlayerPed() ? netGamePlayer->GetPlayerPed()->GetNetworkObject() : 0;
						ObjectId   playerObjectID = playerObject ? playerObject->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

						if(playerObjectID != NETWORK_INVALID_OBJECT_ID)
						{
							CNetObjPlayer *netObjPlayer      = SafeCast(CNetObjPlayer, playerObject);
							bool           isSpectating      = netObjPlayer ? netObjPlayer->IsSpectating() : false;
							ObjectId       spectatorObjectID = netObjPlayer ? netObjPlayer->GetSpectatorObjectId() : NETWORK_INVALID_OBJECT_ID;

							for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
							{
								PedData &pedData = sceneData.m_PedData[pedIndex];

								if(pedData.m_PedID == playerObjectID)
								{
									gnetDebug1("Synced scene %d is in scope with %s because the player's ped is part of the scene!", networkSceneID, player.GetLogName());
									return true;
								}

								if(isSpectating && pedData.m_PedID == spectatorObjectID)
								{
									gnetDebug1("Synced scene %d is in scope with %s because the ped the player is spectating is part of the scene!", networkSceneID, player.GetLogName());
									return true;
								}
							}
						}

						// check if the player is using an extended population range - in this case we want to also
						// check the player position
						CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, playerObject);
						if(netObjPlayer && netObjPlayer->IsUsingExtendedPopulationRange())
						{
							playerPos = NetworkUtils::GetNetworkPlayerPosition(*netGamePlayer);
							delta     = playerPos - scenePos;

							if(delta.XYMag2() <= SCENE_SCOPE_DISTANCE_SQUARED)
							{
								gnetDebug1("Synced scene %d is in scope with %s because it is within the extended population XY scope distance Player Dist:%.2fm Scope Dist:%.2fm!", networkSceneID, player.GetLogName(), delta.XYMag(), rage::Sqrtf(SCENE_SCOPE_DISTANCE_SQUARED));
								return true;
							}
						}

						gnetDebug1("Synced scene %d is out of scope with %s because it is outside the XY scope distance Player Dist:%.2fm Scope Dist:%.2fm!", networkSceneID, player.GetLogName(), delta.XYMag(), rage::Sqrtf(SCENE_SCOPE_DISTANCE_SQUARED));
						return false;
					}
				}
            }
        }
    }

    return false;
}

int CNetworkSynchronisedScenes::CreateScene(const Vector3    &position,
                                            const Quaternion &rotation,
                                            bool              holdLastFrame,
                                            bool              isLooped,
                                            float             phaseToStartScene,
                                            float             phaseToStopScene,
											float             startRate,
											const char*		  scriptName,
											int				  scriptThreadPC)
{
    if(phaseToStopScene < 0.0f || phaseToStopScene > 1.0f)
    {
        gnetAssertf(0, "Invalid phase to stop scene (%.2f)!", phaseToStopScene);
        phaseToStopScene = 1.0f;
    }

    int newSceneID = -1;

    SceneDescription *sceneDescription = AllocateNewSceneDescription();

    if(sceneDescription)
    {
        sceneDescription->m_ScenePosition     = position;
        sceneDescription->m_SceneRotation     = rotation;
        sceneDescription->m_AttachToEntityID  = NETWORK_INVALID_OBJECT_ID;
        sceneDescription->m_AttachToBone      = -1;
        sceneDescription->m_HoldLastFrame     = holdLastFrame;
        sceneDescription->m_Looped            = isLooped;
        sceneDescription->m_Phase             = phaseToStartScene;
        sceneDescription->m_PhaseToStopScene  = phaseToStopScene;
        sceneDescription->m_Rate              = startRate;
        sceneDescription->m_LocallyCreated    = true;
        sceneDescription->m_Active            = true;
		sceneDescription->m_ScriptName		  = scriptName;
		sceneDescription->m_ScriptThreadPC    = scriptThreadPC;

#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CREATING_NETWORK_SYNC_SCENE", "");
        CNetworkSynchronisedScenes::LogSceneDescription(NetworkInterface::GetObjectManager().GetLog(), sceneDescription->m_NetworkSceneID);
#endif // #if ENABLE_NETWORK_LOGGING

        newSceneID = sceneDescription->m_NetworkSceneID;
    }

    return newSceneID;
}

void CNetworkSynchronisedScenes::ReleaseScene(int networkID)
{
    ReleaseSceneDescription(networkID);
}

void CNetworkSynchronisedScenes::AttachSyncedSceneToEntity(int networkSceneID, const CPhysical *physical, int boneIndex)
{
    if(gnetVerifyf(physical, "Trying to attach a synced scene to a NULL physical entity!") &&
       gnetVerifyf(physical->GetNetworkObject(), "Trying to attach a synced scene to a non-networked physical entity!"))
    {
        SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

        if(gnetVerifyf(sceneDescription, "Trying to attach an entity to scene with an invalid scene ID (%d)!", networkSceneID))
        {
            if(gnetVerifyf(sceneDescription->m_Active && !sceneDescription->m_PendingStart && !sceneDescription->m_Started,
                           "Can only attach an active networked synced scene that has not been started yet to an entity!"))
            {
                sceneDescription->m_AttachToEntityID = physical->GetNetworkObject()->GetObjectID();
                sceneDescription->m_AttachToBone     = boneIndex;
            }
        }
    }
}

void CNetworkSynchronisedScenes::AddPedToScene(CPed       *ped, 
                                               int         networkSceneID,
                                               const char *animDictName,
                                               const char *animName,
                                               float       blendIn,
                                               float       blendOut,
                                               int         flags,
                                               int         ragdollBlockingFlags,
                                               float       moverBlendIn,
                                               int         ikFlags)
{
    if(gnetVerifyf(ped, "NULL ped being added to a network synced scene!") &&
       gnetVerifyf(ped->GetNetworkObject(), "Non-networked ped being added to a network synced scene!"))
    {
        ObjectId pedID = ped->GetNetworkObject()->GetObjectID();

        SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

        if(gnetVerifyf(sceneDescription, "Adding %s to scene with an invalid scene ID (%d)!", ped->GetNetworkObject()->GetLogName(), networkSceneID))
        {
            SceneDescription &sceneData = *sceneDescription;

            int  nextFreeSlot = -1;
            bool pedExists    = false;
            for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE && !pedExists; pedIndex++)
            {
                PedData &pedData = sceneData.m_PedData[pedIndex];

                if(pedData.m_PedID == NETWORK_INVALID_OBJECT_ID && (nextFreeSlot == -1))
                {
                    nextFreeSlot = pedIndex;
                }

                pedExists = (pedData.m_PedID == pedID);
            }

            if(gnetVerifyf(!pedExists, "Adding (%s) to a synced scene that has already been added!", ped->GetNetworkObject()->GetLogName()))
            {
                if(gnetVerifyf(nextFreeSlot >= 0, "Added too many peds to a network synced scene (limit is %d)", CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE))
                {
                    PedData &pedData = sceneData.m_PedData[nextFreeSlot];

                    pedData.m_PedID                = pedID;
                    pedData.m_BlendIn              = blendIn;
                    pedData.m_BlendOut             = blendOut;
                    pedData.m_MoverBlendIn         = moverBlendIn;
                    pedData.m_Flags                = flags;
                    pedData.m_RagdollBlockingFlags = ragdollBlockingFlags;
                    pedData.m_IkFlags              = ikFlags;
                    pedData.m_WaitingForTaskUpdate = false;

                    pedData.m_AnimPartialHash = atPartialStringHash(animName);

                    if(sceneData.m_AnimDict.GetHash() == 0)
                    {
                        sceneData.m_AnimDict.SetFromString(animDictName);
                    }
                    else
                    {
                        strStreamingObjectName animDict;
                        animDict.SetFromString(animDictName);
                        gnetAssertf(sceneData.m_AnimDict.GetHash() == animDict.GetHash(), "Network synced scenes only support using one anim dictionary!");
                    }

#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ADDING_PED_TO_SYNC_SCENE", "Network Scene ID: %d", networkSceneID);
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped", "%s", ped->GetNetworkObject()->GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Anim", "%s", animName?animName:"None");
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Flags", "%d", flags);
#endif // #if ENABLE_NETWORK_LOGGING
                }
            }
        }
    }
}

void CNetworkSynchronisedScenes::AddEntityToScene(CPhysical  *entity,
                                                  int         networkSceneID,
                                                  const char *animDictName,
                                                  const char *animName,
                                                  float       blendIn,
                                                  float       blendOut,
                                                  int         flags)
{
    if(gnetVerifyf(entity, "NULL entity being added to a network synced scene!") &&
       gnetVerifyf(!entity->GetIsTypePed(), "Ped being added to a network synced scene via AddEntityToScene()! Use AddPedToScene() instead!") &&
       gnetVerifyf(entity->GetNetworkObject(), "Non-networked entity being added to a network synced scene!"))
    {
        ObjectId entityID = entity->GetNetworkObject()->GetObjectID();

        SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

        if(gnetVerifyf(sceneDescription, "Adding %s to scene with an invalid scene ID (%d)!", entity->GetNetworkObject()->GetLogName(), networkSceneID))
        {
            SceneDescription &sceneData = *sceneDescription;

            int  nextFreeSlot = -1;
            bool entityExists = false;
            for(unsigned index = 0; index < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE && !entityExists; index++)
            {
                NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[index];

                if(nonPedEntityData.m_EntityID == NETWORK_INVALID_OBJECT_ID && (nextFreeSlot == -1))
                {
                    nextFreeSlot = index;
                }

                entityExists = (nonPedEntityData.m_EntityID == entityID);
            }

            if(gnetVerifyf(!entityExists, "Adding (%s) to a synced scene that has already been added!", entity->GetNetworkObject()->GetLogName()))
            {
                if(gnetVerifyf(nextFreeSlot >= 0, "Added too many entity to a network synced scene (limit is %d)", CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE))
                {
                    NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nextFreeSlot];

                    nonPedEntityData.m_EntityID = entityID;
                    nonPedEntityData.m_BlendIn  = blendIn;
                    nonPedEntityData.m_BlendOut = blendOut;
                    nonPedEntityData.m_Flags    = flags;

                    nonPedEntityData.m_Anim = atPartialStringHash(animName);

                    if(sceneData.m_AnimDict.GetHash() == 0)
                    {
                        sceneData.m_AnimDict.SetFromString(animDictName);
                    }
                    else
                    {
                        strStreamingObjectName animDict;
                        animDict.SetFromString(animDictName);
                        gnetAssertf(sceneData.m_AnimDict.GetHash() == animDict.GetHash(), "Network synced scenes only support using one anim dictionary!");
                    }

#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ADDING_ENTITY_TO_SYNC_SCENE", "");
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Entity", "%s", entity->GetNetworkObject()->GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Flags", "%d", flags);
#endif // #if ENABLE_NETWORK_LOGGING
                }
            }
        }
    }
}

void CNetworkSynchronisedScenes::AddMapEntityToScene(int sceneID, u32 entityModelHash, Vector3 entityPosition, const char *animDictName, const char *animName, float blendIn, float blendOut, int flags)
{
	SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(sceneID);

	if (gnetVerifyf(sceneDescription, "Trying to add a map entity for a scene with an invalid scene ID (%d)!", sceneID))
	{
		SceneDescription &sceneData = *sceneDescription;

		int  nextFreeSlot = -1;
		bool entityExists = false;
		for (unsigned index = 0; index < CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE && !entityExists; index++)
		{
			MapEntityData &mapEntityData = sceneData.m_MapEntityData[index];

			if (mapEntityData.m_EntityNameHash == 0 && (nextFreeSlot == -1))
			{
				nextFreeSlot = index;
			}

			static const float MAX_ENTITY_POS_EPSILON = 0.01f;
			entityExists = (mapEntityData.m_EntityNameHash == entityModelHash && mapEntityData.m_EntityPosition.IsClose(entityPosition, MAX_ENTITY_POS_EPSILON));
		}

		if (gnetVerifyf(!entityExists, "Adding entity with hash: %u position: x:%.2f, y:%.2f, z:%.2f to a synced scene that has already been added!", entityModelHash, entityPosition.x, entityPosition.y, entityPosition.z))
		{
			if (gnetVerifyf(nextFreeSlot >= 0, "Added too many map entity to a network synced scene (limit is %d)", CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE))
			{
				MapEntityData &mapEntityData = sceneData.m_MapEntityData[nextFreeSlot];

				mapEntityData.m_EntityNameHash = entityModelHash;
				mapEntityData.m_EntityPosition = entityPosition;
				mapEntityData.m_BlendIn = blendIn;
				mapEntityData.m_BlendOut = blendOut;
				mapEntityData.m_Flags = flags;

				mapEntityData.m_Anim = atPartialStringHash(animName);

				if (sceneData.m_AnimDict.GetHash() == 0)
				{
					sceneData.m_AnimDict.SetFromString(animDictName);
				}
				else
				{
					strStreamingObjectName animDict;
					animDict.SetFromString(animDictName);
					gnetAssertf(sceneData.m_AnimDict.GetHash() == animDict.GetHash(), "Network synced scenes only support using one anim dictionary!");
				}

#if ENABLE_NETWORK_LOGGING
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ADDING_MAP_ENTITY_TO_SYNC_SCENE", "%d", sceneID);
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Entity Hash", "%s[%u]", atHashString(entityModelHash).GetCStr(), entityModelHash);
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Entity Position", "x:%.2f, y:%.2f, z:%.2f", entityPosition.x, entityPosition.y, entityPosition.z);
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Flags", "%d", flags);
#endif // #if ENABLE_NETWORK_LOGGING
			}
		}
	}
}

void CNetworkSynchronisedScenes::AddCameraToScene(int networkSceneID, const char *animDictName, const char *animName)
{
    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(gnetVerifyf(sceneDescription, "Trying to use a camera for a scene with an invalid scene ID (%d)!", networkSceneID))
    {
        SceneDescription &sceneData = *sceneDescription;
        sceneData.m_UseCamera = true;

        sceneData.m_CameraAnim.SetFromString(animName);

        if(sceneData.m_AnimDict.GetHash() == 0)
        {
            sceneData.m_AnimDict.SetFromString(animDictName);
        }
        else
        {
            strStreamingObjectName animDict;
            animDict.SetFromString(animDictName);
            gnetAssertf(sceneData.m_AnimDict.GetHash() == animDict.GetHash(), "Network synced scenes only support using one anim dictionary!");
        }
    }
}

void CNetworkSynchronisedScenes::ForceLocalUseOfCamera(int networkSceneID)
{
    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(gnetVerifyf(sceneDescription, "Trying to force local use of camera for a scene with an invalid scene ID (%d)!", networkSceneID))
    {
        SceneDescription &sceneData = *sceneDescription;
        sceneData.m_ForceLocalUseOfCamera = true;
    }
}

bool CNetworkSynchronisedScenes::SetSceneRate(int networkSceneID, float const rate)
{
	SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

	if(sceneDescription)
	{
		SceneDescription &sceneData = *sceneDescription;

		// some scripts appear to constantly set the rate every frame...
		if((Abs(sceneData.m_Rate - rate)) > 0.001f)
		{
			sceneData.m_Rate = rate;

#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CHANGE_SYNC_SCENE_RATE", "");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Scene ID", "%d",   networkSceneID);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("New Rate", "%.2f", rate);
#endif // #if ENABLE_NETWORK_LOGGING

			// send an event to all other players to update their rate...
			if(NetworkInterface::IsGameInProgress())
			{
				CUpdateNetworkSyncedSceneEvent::Trigger(networkSceneID, sceneData.m_Rate);
			}
		}

		return true;
	}

	return false;
}

bool CNetworkSynchronisedScenes::GetSceneRate(int networkSceneID, float& rate)
{
	SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

	if(sceneDescription)
	{
		rate = sceneDescription->m_Rate;

		return true;
	}

	return false;
}

struct MapEntitySearch
{
	static const int NUM_MAP_ENTITY_SEARCH_RESULTS = 10;
	int nNumEntitiesInRange;
	CPhysical *pInRangeEntities[NUM_MAP_ENTITY_SEARCH_RESULTS];
	u32 entityNameHash;
};

bool MapEntitySeachCB(CEntity* pEntity, void* data)
{
	MapEntitySearch* pData = reinterpret_cast<MapEntitySearch*>(data);
	if (pData->nNumEntitiesInRange >= MapEntitySearch::NUM_MAP_ENTITY_SEARCH_RESULTS)
	{
		return true;
	}

	if (!pEntity->GetIsPhysical())
	{
		return true;
	}

	if (pEntity->GetModelNameHash() == pData->entityNameHash)
	{
		pData->pInRangeEntities[pData->nNumEntitiesInRange] = static_cast<CPhysical *>(pEntity);
		pData->nNumEntitiesInRange++;
	}

	return true;
}

CPhysical* FindMapEntity(u32 entityNameHash, Vector3 entityPosition)
{
	const float RADIUS = 0.1f;

	MapEntitySearch mapEntitySearchData;
	mapEntitySearchData.nNumEntitiesInRange = 0;
	mapEntitySearchData.entityNameHash = entityNameHash;

	spdSphere testSphere(VECTOR3_TO_VEC3V(entityPosition), ScalarV(RADIUS));
	fwIsSphereIntersecting searchSphere(testSphere);

	CGameWorld::ForAllEntitiesIntersecting(&searchSphere, MapEntitySeachCB, (void*)&mapEntitySearchData,
		ENTITY_TYPE_MASK_OBJECT, (SEARCH_LOCATION_INTERIORS | SEARCH_LOCATION_EXTERIORS),
		SEARCH_LODTYPE_ALL, SEARCH_OPTION_DYNAMICS_ONLY, WORLDREP_SEARCHMODULE_NONE);

	if (mapEntitySearchData.nNumEntitiesInRange > 0)
	{
		CPhysical* closestFound = mapEntitySearchData.pInRangeEntities[0];
		float closestDist2 = entityPosition.Dist2(VEC3V_TO_VECTOR3(closestFound->GetTransform().GetPosition()));
		
		for (int i = 1; i < mapEntitySearchData.nNumEntitiesInRange; i++)
		{
			float tempDistance2 = entityPosition.Dist2(VEC3V_TO_VECTOR3(mapEntitySearchData.pInRangeEntities[i]->GetTransform().GetPosition()));
			if (tempDistance2 < closestDist2)
			{
				closestFound = mapEntitySearchData.pInRangeEntities[i];
				closestDist2 = tempDistance2;
			}
		}
		return closestFound;
	}
	return nullptr;
}

bool CNetworkSynchronisedScenes::TryToStartSceneRunning(int networkSceneID)
{
    bool canStartScene = false;

    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(!sceneDescription)
    {
        gnetDebug1("Can't start synced scene running because network scene %d isn't on this machine!", networkSceneID);
    }
    else
    {
        SceneDescription &sceneData = *sceneDescription;

        if(gnetVerifyf(sceneData.m_Active, "Trying to start a network synced scene that has not been created (Id = %d)", networkSceneID) &&
           gnetVerifyf(!sceneData.m_Started, "Trying to start a network synced scene that has already been started! (Id = %d)", networkSceneID))
        {
            canStartScene = true;

            // ensure all of the peds involved in the scene exist on this machine
            CNetObjPed *netObjPeds[CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE];
            unsigned numLocalPeds = 0;

            for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE && canStartScene; pedIndex++)
            {
                PedData &pedData = sceneData.m_PedData[pedIndex];

                if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject *netObject = NetworkInterface::GetObjectManager().GetNetworkObject(pedData.m_PedID);

                    if(!netObject)
                    {
                        gnetDebug1("Can't start synced scene ID %d running because ped %d isn't on this machine!", networkSceneID, pedData.m_PedID);
                        canStartScene = false;
                    }
                    else if(!netObject->IsClone() && gnetVerifyf(numLocalPeds < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE, "Unexpected number of local peds!"))
                    {
                        netObjPeds[numLocalPeds] = SafeCast(CNetObjPed, netObject);
                        numLocalPeds++;
                    }

                    if(netObject && !netObject->IsClone() && netObject->GetObjectType() == NET_OBJ_TYPE_PLAYER && !AreRemoteSyncSceneLocalPlayerRequestsAllowed())
                    {
                        bool blockSceneStart = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("BLOCK_REMOTE_SYNCED_SCENE_REQUESTS", 0x70207708), true);

                        if(blockSceneStart)
                        {
                            gnetAssertf(0, "Received a remote synced scene request when remote requests are blocked! Script need to call NETWORK_ALLOW_REMOTE_SYNCED_SCENE_LOCAL_PLAYER_REQUESTS(TRUE) to allow this!");
                            gnetDebug1("Can't start synced scene ID %d running because ped %d is the local player and remote requests are blocked! Script need to call NETWORK_ALLOW_REMOTE_SYNCED_SCENE_LOCAL_PLAYER_REQUESTS(TRUE) to allow this!", networkSceneID, pedData.m_PedID);

                            canStartScene = false;
                        }
                    }
                }
            }

            // ensure all of the non-peds involved in the scene exist on this machine
            for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE && canStartScene; nonPedIndex++)
            {
                NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

                if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
                {
                    if(!NetworkInterface::GetObjectManager().GetNetworkObject(nonPedEntityData.m_EntityID))
                    {
                        gnetDebug1("Can't start synced scene ID %d running because non-ped %d isn't on this machine!", networkSceneID, nonPedEntityData.m_EntityID);
                        canStartScene = false;
                    }
                }
            }

			for (unsigned mapEntityIndex = 0; mapEntityIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE && canStartScene; mapEntityIndex++)
			{
				MapEntityData& mapEntityData = sceneData.m_MapEntityData[mapEntityIndex];
				if (mapEntityData.m_EntityNameHash != 0)
				{
					if (!FindMapEntity(mapEntityData.m_EntityNameHash, mapEntityData.m_EntityPosition))
					{
						gnetDebug1("Can't start synced scene ID %d running because map entity with hash: %u position: x:%.2f, y:%.2f, z:%.2f isn't on this machine!", networkSceneID, mapEntityData.m_EntityNameHash, mapEntityData.m_EntityPosition.x, mapEntityData.m_EntityPosition.y, mapEntityData.m_EntityPosition.z);
						canStartScene = false;
					}
				}
			}

            if(canStartScene)
            {
                StartSceneRunning(networkSceneID);

                // prevent all local network peds from sending a task update this frame
                for(unsigned index = 0; index < numLocalPeds; index++)
                {
                    netObjPeds[index]->PreventTaskUpdateThisFrame();
                }
            }
        }
    }

#if ENABLE_NETWORK_LOGGING
	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "TRY_TO_START_SYNCED_SCENE", "");
	NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Scene ID", "%d", networkSceneID);
	NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Can Start", "%s", canStartScene ?  "TRUE" : "FALSE");
#endif // #if ENABLE_NETWORK_LOGGING

    return canStartScene;
}

void CNetworkSynchronisedScenes::StartSceneRunning(int networkSceneID)
{
    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(sceneDescription)
    {
        SceneDescription &sceneData = *sceneDescription;

        if(gnetVerifyf(sceneData.m_Active, "Trying to start a network synced scene that has not been created (Id = %d)", networkSceneID) &&
           gnetVerifyf(!sceneData.m_Started, "Trying to start a network synced scene that has already been started! (Id = %d)", networkSceneID))
        {
            // request required animations
            s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(sceneData.m_AnimDict).Get();
            sceneData.m_ClipRequestHelper.RequestClipsByIndex(animDictIndex);

            // send an event to all other players to play the synced scene
            if(!sceneData.m_PendingStart && sceneData.m_LocallyCreated && NetworkInterface::IsGameInProgress())
            {
                CStartNetworkSyncedSceneEvent::Trigger(networkSceneID, sceneData.m_NetworkTimeStarted);
            }

            sceneData.m_PendingStart = true;
        }
    }
}

void CNetworkSynchronisedScenes::StopSceneRunning(int networkSceneID LOGGING_ONLY(, const char *stopReason))
{
    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(sceneDescription)
    {
        SceneDescription &sceneData = *sceneDescription;

#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STOP_SCENE_RUNNING", "Scene ID: %d", networkSceneID);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "%s", stopReason);
#endif // #if ENABLE_NETWORK_LOGGING

        if(sceneData.m_Active)
        {
            if(sceneData.m_Started)
            {
                // send an event to all other players to play the synced scene
                if(sceneData.m_LocallyCreated && NetworkInterface::IsGameInProgress())
                {
                    CStopNetworkSyncedSceneEvent::Trigger(networkSceneID);
                }

                StopLocalSynchronisedScene(sceneData, "Stop Scene Running called");
            }
            else
            {
                sceneData.Reset();
            }
        }
    }
}

void CNetworkSynchronisedScenes::PostSceneUpdateFromNetwork(int networkSceneID, unsigned networkTimeStarted)
{
    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(sceneDescription)
    {
        SceneDescription &sceneData = *sceneDescription;

        sceneData.m_Started            = false;
        sceneData.m_LocallyCreated     = false;
        sceneData.m_NetworkTimeStarted = networkTimeStarted;
    }
}

void CNetworkSynchronisedScenes::PostTaskUpdateFromNetwork(CPed *ped)
{
    CheckSyncedSceneStateForPed(ped);
}

void CNetworkSynchronisedScenes::CheckSyncedSceneStateForPed(CPed *ped)
{
    if(ped && ped->GetNetworkObject())
    {
        gnetAssertf(ped->IsNetworkClone(), "Calling CheckSyncedSceneStateForPed on a local ped!");

        ObjectId pedID = ped->GetNetworkObject()->GetObjectID();

        // check if this ped is running a synchronised scene
        CTaskSynchronizedScene *syncedSceneTask = static_cast<CTaskSynchronizedScene *>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));

        int networkSceneID = -1;

        if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SYNCHRONIZED_SCENE, true))
        {
            networkSceneID = ped->GetPedIntelligence()->GetQueriableInterface()->GetNetworkSynchronisedSceneID();
        }

        for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
        {
            SceneDescription &sceneData = ms_SyncedScenes[index];

            if(sceneData.m_Started)
            {
                for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
                {
                    PedData &pedData = sceneData.m_PedData[pedIndex];

                    if(pedData.m_PedID == pedID)
                    {
                        if(sceneData.m_NetworkSceneID == networkSceneID)
                        {
                            if(!syncedSceneTask)
                            {
                                if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID))
                                {
                                    fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(sceneData.m_LocalSceneID);

									gnetDebug1("%s CheckSyncedSceneStateForPed StartLocalSyncedSceneOnPed - networkSceneID %d, sceneData.m_LocalSceneID %d, Frame: %d",
										ped->GetNetworkObject()->GetLogName(),
										networkSceneID,
										sceneData.m_LocalSceneID,
										fwTimer::GetFrameCount());

                                    StartLocalSyncedSceneOnPed(sceneData, pedData);

                                    fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(sceneData.m_LocalSceneID);
                                }

                                syncedSceneTask = static_cast<CTaskSynchronizedScene *>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
                            }
                        }

                        if(pedData.m_WaitingForTaskUpdate)
                        {
                            pedData.m_WaitingForTaskUpdate = false;
                        }
                    }
                }
            }
        }

        if(syncedSceneTask == 0 && networkSceneID != -1 && ped->GetNetworkObject()->GetPlayerOwner())
        {
            if(!GetSceneDescriptionFromNetworkID(networkSceneID) && NetworkInterface::IsGameInProgress())
            {
                const float SCENE_SCOPE_DISTANCE_SQUARED     = rage::square(100.0f);
				const unsigned SYNC_SCENE_REQUEST_COOLDOWN   = 200;

                Vector3 localPlayerPos			   = NetworkInterface::GetPlayerFocusPosition(*NetworkInterface::GetLocalPlayer(), NULL, true);
                Vector3 delta					   = localPlayerPos - VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
				CNetObjPed* netObjectPed           = SafeCast(CNetObjPed, ped->GetNetworkObject());
				u32 currTime					   = fwTimer::GetSystemTimeInMilliseconds();
				

                if(delta.XYMag2() <= SCENE_SCOPE_DISTANCE_SQUARED 
					&& HasFreeSceneDescription() 
					&& netObjectPed
					&& currTime > netObjectPed->GetLastRequestedSyncSceneTime() + SYNC_SCENE_REQUEST_COOLDOWN)
                {
					CNetGamePlayer* player = ped->GetNetworkObject()->GetPlayerOwner();
					if(player && player->GetPlayerPed() && player->GetPlayerPed()->GetNetworkObject())
					{
						CNetObjPhysical* physicalNetPlayer = SafeCast(CNetObjPhysical, player->GetPlayerPed()->GetNetworkObject());
						if(physicalNetPlayer && !physicalNetPlayer->IsConcealed())
						{
							netObjectPed->SetLastRequestedSyncSceneTime(currTime);
							CRequestNetworkSyncedSceneEvent::Trigger(networkSceneID, *ped->GetNetworkObject()->GetPlayerOwner());
						}
					}
				}
            }
        }
    }
}

void CNetworkSynchronisedScenes::OnPedAbortingSyncedScene(CPed *LOGGING_ONLY(ped), int LOGGING_ONLY(networkSceneID), const char *LOGGING_ONLY(eventName))
{
#if ENABLE_NETWORK_LOGGING
    if(ped && ped->GetNetworkObject() && networkSceneID != -1)
    {

        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "PED_ABORTING_SYNCED_SCENE", "Scene ID: %d", networkSceneID);
        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped", "%s", ped->GetNetworkObject()->GetLogName());
        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Event", "%s", eventName);
        
        if(!gb_NoCallstackDumpOnPedAbort && ped->IsNetworkClone())
        {
            sysStack::PrintStackTrace();
        }
    }
#endif // #if ENABLE_NETWORK_LOGGING
}

void CNetworkSynchronisedScenes::OnPedAbortingSyncedSceneEarly(CPed *ped, int networkSceneID)
{
    if(ped && ped->GetNetworkObject() && networkSceneID != -1)
    {
        SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

        if(sceneDescription && sceneDescription->m_Started && !sceneDescription->m_Stopping)
        {
            bool stopScene = false;
            bool foundPed  = false;

            for(unsigned int index = 0; index < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE && !foundPed; index++)
            {
                if(sceneDescription->m_PedData[index].m_PedID == ped->GetNetworkObject()->GetObjectID())
                {
                    foundPed = true;

                    if((sceneDescription->m_PedData[index].m_Flags & BIT(SYNCED_SCENE_ON_ABORT_STOP_SCENE)) != 0)
                    {
                        stopScene = true;
                    }
                }
            }

            gnetAssertf(foundPed, "The specified ped is not part of the synced scene specified!");

            if(stopScene)
            {
                sceneDescription->m_PendingStop = true;
            }
        }
    }
}

bool CNetworkSynchronisedScenes::IsCloneBlockedFromPlayingScene(int networkSceneID)
{
	if(sysAppContent::IsJapaneseBuild())
	{
		SceneDescription *sceneDescription = CNetworkSynchronisedScenes::GetSceneDescriptionFromNetworkID(networkSceneID);
		if(sceneDescription)
		{
			if(sceneDescription->m_AnimDict == ATSTRINGHASH("mini@prostitutes@sexnorm_veh", 0x68D5C818)
			|| sceneDescription->m_AnimDict == ATSTRINGHASH("mini@prostitutes@sexlow_veh", 0x6380BFE9)
			|| sceneDescription->m_AnimDict == ATSTRINGHASH("mini@prostitutes@sexlow_veh_first_person", 0x3A68E23A)
			|| sceneDescription->m_AnimDict == ATSTRINGHASH("mini@prostitutes@sexnorm_veh_first_person", 0xF72790A8)
			|| sceneDescription->m_AnimDict == ATSTRINGHASH("anim@mini@prostitutes@sex@veh_vstr@", 0x2D596AE5))
			{
				return true;
			}
		}
	}
	return false;
}

#if !__FINAL

static void GetAnimFlagsString(int animFlags, char *animFlagsString, unsigned stringMaxSize)
{
    char const * const animFlagNames[eSyncedSceneFlags_NUM_ENUMS] = 
    {
        "SYNCED_SCENE_USE_KINEMATIC_PHYSICS",
        "SYNCED_SCENE_TAG_SYNC_OUT",
        "SYNCED_SCENE_DONT_INTERRUPT",
        "SYNCED_SCENE_ON_ABORT_STOP_SCENE",
        "SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE",
        "SYNCED_SCENE_BLOCK_MOVER_UPDATE",
        "SYNCED_SCENE_LOOP_WITHIN_SCENE",
        "SYNCED_SCENE_PRESERVE_VELOCITY",
        "SYNCED_SCENE_EXPAND_PED_CAPSULE_FROM_SKELETON",
        "SYNCED_SCENE_ACTIVATE_RAGDOLL_ON_COLLISION",
        "SYNCED_SCENE_HIDE_WEAPON",
        "SYNCED_SCENE_ABORT_ON_DEATH",
        "SYNCED_SCENE_VEHICLE_ABORT_ON_LARGE_IMPACT",
        "SYNCED_SCENE_VEHICLE_ALLOW_PLAYER_ENTRY",
        "SYNCED_SCENE_PROCESS_ATTACHMENTS_ON_START",
        "SYNCED_SCENE_NET_ON_EARLY_NON_PED_STOP_RETURN_TO_START",
        "SYNCED_SCENE_SET_PED_OUT_OF_VEHICLE_AT_START",
        "SYNCED_SCENE_NET_DISREGARD_ATTACHMENT_CHECKS"
    };

    animFlagsString[0] = '\0';

    for(unsigned index = 0; index < eSyncedSceneFlags_NUM_ENUMS; index++)
    {
        if((animFlags & (1<<index)) != 0)
        {
            if(strlen(animFlagsString))
            {
                safecat(animFlagsString, ";", stringMaxSize);
                animFlagsString[stringMaxSize-1] = '\0';
            }
            const char *flagName = animFlagNames[index];
            safecat(animFlagsString, flagName, stringMaxSize);
            animFlagsString[stringMaxSize-1] = '\0';
        }
    }
}

static void GetRagdollBlockingFlags(int ragdollBlockingFlags, char *rbfString, unsigned stringMaxSize)
{
    char const * const rbfFlagNames[eRagdollBlockingFlags_NUM_ENUMS] = 
    {
        "RBF_BULLET_IMPACT",
        "RBF_VEHICLE_IMPACT",
        "RBF_FIRE",
        "RBF_ELECTROCUTION",
        "RBF_PLAYER_IMPACT",
        "RBF_EXPLOSION",
        "RBF_IMPACT_OBJECT",
        "RBF_MELEE",
        "RBF_RUBBER_BULLET",
        "RBF_FALLING",
        "RBF_WATER_JET",
        "RBF_DROWNING",
        "RBF_ALLOW_BLOCK_DEAD_PED",
        "RBF_PLAYER_BUMP",
        "RBF_PLAYER_RAGDOLL_BUMP",
        "RBF_PED_RAGDOLL_BUMP",
        "RBF_VEHICLE_GRAB",
        "RBF_SMOKE_GRENADE"
    };

    rbfString[0] = '\0';

    for(unsigned index = 0; index < eRagdollBlockingFlags_NUM_ENUMS; index++)
    {
        if((ragdollBlockingFlags & (1<<index)) != 0)
        {
            if(strlen(rbfString))
            {
                safecat(rbfString, ";", stringMaxSize);
                rbfString[stringMaxSize-1] = '\0';
            }
            const char *flagName = rbfFlagNames[index];
            safecat(rbfString, flagName, stringMaxSize);
            rbfString[stringMaxSize-1] = '\0';
        }
    }
}

void CNetworkSynchronisedScenes::LogSceneDescription(netLoggingInterface &log, int networkSceneID)
{
	log.WriteDataValue("Scene ID", "%d", networkSceneID);

    SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

    if(gnetVerifyf(sceneDescription, "Trying to serialise an invalid scene (scene ID = %d)!", networkSceneID))
    {
        SceneDescription &sceneData = *sceneDescription;

        Vector3 eulers;
        sceneData.m_SceneRotation.ToEulers(eulers);

        
        log.WriteDataValue("Position", "%.2f, %.2f, %.2f", sceneData.m_ScenePosition.x, sceneData.m_ScenePosition.y, sceneData.m_ScenePosition.z);
        log.WriteDataValue("Rotation", "%.2f, %.2f, %.2f (%f, %f, %f, %f)", eulers.x, eulers.y, eulers.z, 
                                                                            sceneData.m_SceneRotation.x,
                                                                            sceneData.m_SceneRotation.y,
                                                                            sceneData.m_SceneRotation.z,
                                                                            sceneData.m_SceneRotation.w);
        log.WriteDataValue("Phase", "%.3f", sceneData.m_Phase);
		log.WriteDataValue("Rate", "%.2f", sceneData.m_Rate);
        log.WriteDataValue("Attach Entity", "%d", sceneData.m_AttachToEntityID);
        log.WriteDataValue("Attach Entity Bone", "%d", sceneData.m_AttachToBone);
        log.WriteDataValue("Hold Last Frame", "%s", sceneData.m_HoldLastFrame ? "TRUE" : "FALSE");
        log.WriteDataValue("Looped", "%s", sceneData.m_Looped ? "TRUE" : "FALSE");

        if(sceneData.m_PhaseToStopScene < 1.0f)
        {
            log.WriteDataValue("Phase to stop scene", "%.2f", sceneData.m_PhaseToStopScene);
        }

        log.WriteDataValue("Use Camera", "%s", sceneData.m_UseCamera ? "TRUE" : "FALSE");
        log.WriteDataValue("Active", "%s", sceneData.m_Active ? "TRUE" : "FALSE");
        log.WriteDataValue("Started", "%s", sceneData.m_Started ? "TRUE" : "FALSE");
        log.WriteDataValue("Locally Created", "%s", sceneData.m_LocallyCreated ? "TRUE" : "FALSE");

        const char *dictName = sceneData.m_AnimDict.TryGetCStr();

        if(dictName)
        {
            log.WriteDataValue("Anim Dictionary", "%s", dictName);
        }
        else
        {
            log.WriteDataValue("Anim Dictionary", "%d", sceneData.m_AnimDict.GetHash());
        }

        if(sceneData.m_UseCamera)
        {
            const char *animName = sceneData.m_CameraAnim.TryGetCStr();

            if(animName)
            {
                log.WriteDataValue("Camera Anim Name", "%s", animName);
            }
            else
            {
                log.WriteDataValue("Camera Anim Name", "%d", sceneData.m_CameraAnim.GetHash());
            }
        }

        for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
        {
            PedData &pedData = sceneData.m_PedData[pedIndex];

            if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
            {
                netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(pedData.m_PedID);
                log.WriteDataValue("Object", "%s", networkObject ? networkObject->GetLogName() : "Unknown");
                
                u32 animHash = atFinalizeHash(pedData.m_AnimPartialHash);
                const char *animName = atHashString(animHash).TryGetCStr();

                if(animName)
                {
                    log.WriteDataValue("Anim Name", "%s", animName);
                }
                else
                {
                    
                    log.WriteDataValue("Anim Name", "%d", animHash);
                }

                const unsigned MAX_FLAG_STRING_SIZE = 256;
                char animFlags[MAX_FLAG_STRING_SIZE]            = "";
                char ragdollBlockingFlags[MAX_FLAG_STRING_SIZE] = "";
                GetAnimFlagsString(pedData.m_Flags, animFlags, MAX_FLAG_STRING_SIZE);
                GetRagdollBlockingFlags(pedData.m_RagdollBlockingFlags, ragdollBlockingFlags, MAX_FLAG_STRING_SIZE);

                log.WriteDataValue("Blend in rate",          "%.2f", pedData.m_BlendIn);
                log.WriteDataValue("Blend out rate",         "%.2f", pedData.m_BlendOut);
                log.WriteDataValue("Anim Flags",             "%s",   animFlags);
                log.WriteDataValue("Ragdoll blocking flags", "%s",   ragdollBlockingFlags);
                log.WriteDataValue("IK flags",               "%d",   pedData.m_IkFlags);
                log.WriteDataValue("Mover Blend in rate",    "%.2f", pedData.m_MoverBlendIn);
            }
        }

        for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
        {
            NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

            if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
            {
                netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(nonPedEntityData.m_EntityID);
                log.WriteDataValue("Object", "%s", networkObject ? networkObject->GetLogName() : "Unknown");
                
                const char *animName = nonPedEntityData.m_Anim.TryGetCStr();

                if(animName)
                {
                    log.WriteDataValue("Anim Name", "%s", animName);
                }
                else
                {
                    
                    log.WriteDataValue("Anim Name", "%d", nonPedEntityData.m_Anim.GetHash());
                }

                log.WriteDataValue("Blend in rate",  "%.2f", nonPedEntityData.m_BlendIn);
                log.WriteDataValue("Blend out rate", "%.2f", nonPedEntityData.m_BlendOut);
                log.WriteDataValue("Anim Flags",     "%d",   nonPedEntityData.m_Flags);
            }
        }
    }
}
#endif

CNetworkSynchronisedScenes::SceneDescription *CNetworkSynchronisedScenes::AllocateNewSceneDescription(const int networkSceneID)
{
    SceneDescription *sceneDescription = 0;

    LOGGING_ONLY(int slotIndex = -1);

    for(int index = 0; index < MAX_NUM_SCENES && (!sceneDescription); index++)
    {
        if(!ms_SyncedScenes[index].m_Active)
        {
            int newNetworkSceneID = networkSceneID;

            // allocate a new ID if one wasn't specified
            if(networkSceneID == -1)
            {
                // the scene ID is multiplied by the local physical player index to ensure 
                // uniqueness across scenes created by multiple machine
                newNetworkSceneID = (NetworkInterface::GetLocalPhysicalPlayerIndex() * MAX_NUM_SCENES) + index;
            }

            // check we don't already have a scene running with this network ID - this can happen if a scene this machine originally
            // started came back into scope and was allocated to a different slot in the array
            if(!GetSceneDescriptionFromNetworkID(newNetworkSceneID))
            {
                for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
                {
                    ms_SyncedScenes[index].m_PedData[pedIndex].m_PedID = NETWORK_INVALID_OBJECT_ID;
                }

                for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
                {
                    ms_SyncedScenes[index].m_NonPedEntityData[nonPedIndex].m_EntityID = NETWORK_INVALID_OBJECT_ID;
                }

                ms_SyncedScenes[index].m_NetworkSceneID        = newNetworkSceneID;
                ms_SyncedScenes[index].m_LocalSceneID          = INVALID_SYNCED_SCENE_ID;
                ms_SyncedScenes[index].m_NetworkTimeStarted    = NetworkInterface::GetNetworkTime();
                ms_SyncedScenes[index].m_Phase                 = 0.0f;
			    ms_SyncedScenes[index].m_Rate			       = 1.0f;
                ms_SyncedScenes[index].m_UseCamera             = false;
                ms_SyncedScenes[index].m_ForceLocalUseOfCamera = false;
                ms_SyncedScenes[index].m_Stopping              = false;
                ms_SyncedScenes[index].m_PendingStop           = false;
                ms_SyncedScenes[index].m_LocallyCreated        = false;
                ms_SyncedScenes[index].m_Active                = false;
                ms_SyncedScenes[index].m_Started               = false;
                ms_SyncedScenes[index].m_PendingStart          = false;

                ms_SyncedScenes[index].m_AnimDict.Clear();
                ms_SyncedScenes[index].m_CameraAnim.Clear();

                sceneDescription = &ms_SyncedScenes[index];

                LOGGING_ONLY(slotIndex = index);
            }
        }
    }

    if(!sceneDescription)
    {
        gnetDebug1("Failed to allocate new scene description!");
		for(int index = 0; index < MAX_NUM_SCENES; index++)
		{
			gnetDebug1("SyncedScene_%d: id:%d local_id:%d Active:%s. Started at:%d. Pending start: %s AnimDict: %d", 
				index, ms_SyncedScenes[index].m_NetworkSceneID, ms_SyncedScenes[index].m_LocalSceneID,
				ms_SyncedScenes[index].m_Active?"TRUE":"FALSE", ms_SyncedScenes[index].m_NetworkTimeStarted,
				ms_SyncedScenes[index].m_PendingStart?"TRUE":"FALSE", ms_SyncedScenes[index].m_AnimDict.GetHash());
		}
	}
    else
    {
#if ENABLE_NETWORK_LOGGING
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ALLOCATING_SYNC_SCENE_DESCRIPTION", "");
        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Scene ID", "%d", sceneDescription->m_NetworkSceneID);
        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Local Slot Index", "%d", slotIndex);
#endif // #if ENABLE_NETWORK_LOGGING
    }

    return sceneDescription;
}

void CNetworkSynchronisedScenes::ReleaseSceneDescription(int networkID)
{
    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        if(ms_SyncedScenes[index].m_NetworkSceneID == networkID)
        {
            ms_SyncedScenes[index].Reset();
        }
    }
}

CNetworkSynchronisedScenes::SceneDescription *CNetworkSynchronisedScenes::GetSceneDescriptionFromNetworkID(int networkID)
{
    SceneDescription *sceneDescription = 0;

    for(unsigned index = 0; index < MAX_NUM_SCENES && !sceneDescription; index++)
    {
        if(ms_SyncedScenes[index].m_NetworkSceneID == networkID)
        {
            sceneDescription = &ms_SyncedScenes[index];
        }
    }

    return sceneDescription;
}

CNetworkSynchronisedScenes::SceneDescription *CNetworkSynchronisedScenes::GetSceneDescriptionFromLocalID(int localID)
{
    SceneDescription *sceneDescription = 0;

    for(unsigned index = 0; index < MAX_NUM_SCENES && !sceneDescription; index++)
    {
        if(ms_SyncedScenes[index].m_LocalSceneID == localID)
        {
            sceneDescription = &ms_SyncedScenes[index];
        }
    }

    return sceneDescription;
}

bool CNetworkSynchronisedScenes::HasFreeSceneDescription()
{
	for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
	{
		if(!ms_SyncedScenes[index].m_Active)
		{
			return true;
		}
	}

	return false;
}

bool CNetworkSynchronisedScenes::HaveNonPedModelsBeenLoaded(SceneDescription &sceneData)
{
    for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
    {
        NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

        if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
        {
            netObject *nonPedNetObject = NetworkInterface::GetNetworkObject(nonPedEntityData.m_EntityID);

            if(nonPedNetObject)
            {
                CPhysical *entity = SafeCast(CPhysical, nonPedNetObject->GetEntity());

                if(entity)
                {
                    if(!entity->GetDrawable())
                    {
                        if(!entity->GetBaseModelInfo()->GetHasLoaded())
                        {
                            CModelInfo::RequestAssets(entity->GetBaseModelInfo(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
                        }

                        // invisible entities can't create drawables
                        bool wasVisible = entity->IsBaseFlagSet(fwEntity::IS_VISIBLE);
                        entity->SetBaseFlag(fwEntity::IS_VISIBLE);
                        entity->CreateDrawable();

                        if(!wasVisible)
                        {
                            entity->ClearBaseFlag(fwEntity::IS_VISIBLE);
                        }
                    }

                    if(!entity->GetDrawable())
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

void CNetworkSynchronisedScenes::StartLocalSynchronisedScene(SceneDescription &sceneData)
{
    if(gnetVerifyf(sceneData.m_Active, "Trying to start a network synced scene that has not been created!") &&
       gnetVerifyf(!sceneData.m_Started, "Trying to start a network synced scene that has already been started!"))
    {
		sceneData.m_LocalSceneID = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();

		if(gnetVerifyf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Failed to create local synced scene from network!"))
	    {
            fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(sceneData.m_LocalSceneID);
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(sceneData.m_LocalSceneID, sceneData.m_ScenePosition, sceneData.m_SceneRotation);

            if(sceneData.m_AttachToEntityID != NETWORK_INVALID_OBJECT_ID)
            {
                netObject *networkObject = NetworkInterface::GetNetworkObject(sceneData.m_AttachToEntityID);

                if(gnetVerifyf(networkObject, "Trying to start a synced scene attached to a network object not on our machine!"))
                {
                    CPhysical *physical = networkObject->GetEntity() && networkObject->GetEntity()->GetIsPhysical() ? static_cast<CPhysical *>(networkObject->GetEntity()) : 0;
                    fwAnimDirectorComponentSyncedScene::AttachSyncedScene(sceneData.m_LocalSceneID, physical, sceneData.m_AttachToBone);
                }
            }

            fwAnimDirectorComponentSyncedScene::SetSyncedSceneHoldLastFrame(sceneData.m_LocalSceneID, sceneData.m_HoldLastFrame);
            fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(sceneData.m_LocalSceneID, sceneData.m_Looped);
            fwAnimDirectorComponentSyncedScene::SetSyncedSceneRate(sceneData.m_LocalSceneID, sceneData.m_Rate);

            bool localPlayerInvolvedInScene = false;

            ObjectId localPlayerID = (FindPlayerPed() && FindPlayerPed()->GetNetworkObject()) ? FindPlayerPed()->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

            for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
            {
                PedData &pedData = sceneData.m_PedData[pedIndex];

                if(pedData.m_PedID == localPlayerID)
                {
                    localPlayerInvolvedInScene = true;
                }

                StartLocalSyncedSceneOnPed(sceneData, pedData);
            }

            for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
            {
                NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

                if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject *nonPedNetObject = NetworkInterface::GetNetworkObject(nonPedEntityData.m_EntityID);

                    if(nonPedNetObject)
                    {
						unsigned failReason = 0;
                        bool succeeded = false;
                        CPhysical *entity = SafeCast(CPhysical, nonPedNetObject->GetEntity());

                        if(entity)
                        {
							succeeded = StartLocalSyncedSceneOnPhysical(sceneData, entity, nonPedEntityData.m_Anim, nonPedEntityData.m_BlendIn, nonPedEntityData.m_BlendOut, nonPedEntityData.m_Flags, &failReason);
                        }

                        gnetAssertf(succeeded, "Failed to add %s to synced scene correctly! Reason: %s", nonPedNetObject->GetLogName(), GetSSSFailReason(failReason));
                    }

                    RemoveNonPedFromStartedScenes(nonPedEntityData.m_EntityID, sceneData.m_NetworkSceneID);
                }
            }

			for (unsigned mapEntityIndex = 0; mapEntityIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE; mapEntityIndex++)
			{
				MapEntityData& mapEntityData = sceneData.m_MapEntityData[mapEntityIndex];
				if (mapEntityData.m_EntityNameHash != 0)
				{
					unsigned failReason = 0;
					bool succeeded = false;
					CPhysical* entity = FindMapEntity(mapEntityData.m_EntityNameHash, mapEntityData.m_EntityPosition);
					if (entity)
					{
						succeeded = StartLocalSyncedSceneOnPhysical(sceneData, entity, mapEntityData.m_Anim, mapEntityData.m_BlendIn, mapEntityData.m_BlendOut, mapEntityData.m_Flags, &failReason);
					}

					gnetAssertf(succeeded, "Failed to add map entity: %s[%u] position: x:%.2f, y:%.2f, z:%.2f to synced scene correctly! Reason: %s", entity? entity->GetLogName() : "Missing", mapEntityData.m_EntityNameHash, mapEntityData.m_EntityPosition.x, mapEntityData.m_EntityPosition.y, mapEntityData.m_EntityPosition.z, GetSSSFailReason(failReason));			
				}
			}

            fwAnimDirectorComponentSyncedScene::SetSyncedScenePhase(sceneData.m_LocalSceneID, sceneData.m_Phase);

            if(sceneData.m_UseCamera && (localPlayerInvolvedInScene || sceneData.m_ForceLocalUseOfCamera))
            {
                s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(sceneData.m_AnimDict).Get();

                const crClip *clip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex, sceneData.m_CameraAnim.GetHash());

				if (gnetVerifyf(clip, "Couldn't find camera animation for networked sync scene. Has it been loaded?"))
				{
					Matrix34 sceneOrigin;
					fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(static_cast<fwSyncedSceneId>(sceneData.m_LocalSceneID), sceneOrigin);
					camInterface::GetSyncedSceneDirector().AnimateCamera(sceneData.m_AnimDict, *clip, sceneOrigin, sceneData.m_LocalSceneID, 0, camSyncedSceneDirector::NETWORK_SYNCHRONISED_SCENES_CLIENT); 
				}
            }

            fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(sceneData.m_LocalSceneID);

            sceneData.m_PendingStart = false;
            sceneData.m_Started      = true;

#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "START_LOCAL_SYNC_SCENE", "");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Scene ID", "%d", sceneData.m_NetworkSceneID);
#endif // #if ENABLE_NETWORK_LOGGING
	    }
    }
}

void CNetworkSynchronisedScenes::StopLocalSynchronisedScene(SceneDescription &sceneData, const char *LOGGING_ONLY(stopReason))
{
    if(gnetVerifyf(sceneData.m_Active,  "Trying to stop a network synced scene that has not been created!"))
	{
		if(sceneData.m_Started)
		{
			sceneData.m_Stopping = true;


			for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
			{
				PedData &pedData = sceneData.m_PedData[pedIndex];

				if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
				{
	#if ENABLE_NETWORK_LOGGING
					netObject *networkObject = NetworkInterface::GetNetworkObject(pedData.m_PedID);
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STOP_LOCAL_SYNC_SCENE_ON_PED", "");
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped", "%s", networkObject ? networkObject->GetLogName() : "Unknown");
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "Stopping entire scene locally");
	#endif // #if ENABLE_NETWORK_LOGGING
					StopLocalSyncedSceneOnPed(pedData);
				}
			}

			for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
			{
				NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

				if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
				{
					netObject *nonPedNetObject = NetworkInterface::GetNetworkObject(nonPedEntityData.m_EntityID);

					if(nonPedNetObject)
					{
						CPhysical *physical = SafeCast(CPhysical, nonPedNetObject->GetEntity());

						if(physical && physical->GetDrawable())
						{
							StopLocalSyncedSceneOnPhysical(sceneData, physical, nonPedEntityData.m_Anim, nonPedEntityData.m_BlendIn, nonPedEntityData.m_Flags);
						}
					}
				}
			}

			for (unsigned mapEntityIndex = 0; mapEntityIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE; mapEntityIndex++)
			{
				MapEntityData& mapEntityData = sceneData.m_MapEntityData[mapEntityIndex];
				if (mapEntityData.m_EntityNameHash != 0)
				{
					CPhysical* physical = FindMapEntity(mapEntityData.m_EntityNameHash, mapEntityData.m_EntityPosition);
					if (physical && physical->GetDrawable())
					{
						StopLocalSyncedSceneOnPhysical(sceneData, physical, mapEntityData.m_Anim, mapEntityData.m_BlendIn, mapEntityData.m_Flags);
					}
				}
			}


			if(sceneData.m_UseCamera)
			{
				camInterface::GetSyncedSceneDirector().StopAnimatingCamera(camSyncedSceneDirector::NETWORK_SYNCHRONISED_SCENES_CLIENT);
			}
		}

		bool allowStopSyncSceneIfNotStarted = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STOPPING_OF_NOT_STARTED_SYNCED_SCENE", 0x88BBDD0D), true);
		if(gnetVerifyf(sceneData.m_Started || allowStopSyncSceneIfNotStarted, "Trying to stop a network synced scene that has already been started!"))
		{
#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STOP_LOCAL_SYNC_SCENE", "");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Scene ID", "%d", sceneData.m_NetworkSceneID);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", stopReason);
#endif // #if ENABLE_NETWORK_LOGGING

			sceneData.Reset();
		}
	}
}

void CNetworkSynchronisedScenes::StartLocalSyncedSceneOnPed(SceneDescription &sceneData, PedData &pedData)
{
    if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
    {
        netObject *pedNetObject = NetworkInterface::GetNetworkObject(pedData.m_PedID);

        if(pedNetObject)
        {
            CPed *ped = NetworkUtils::GetPedFromNetworkObject(pedNetObject);

            if(ped && !ped->IsInjured())
            {
                // clear any secondary tasks running on the ped
                ped->GetPedIntelligence()->ClearTasks(false, true);

                eIkControlFlagsBitSet& ikControlFlags = reinterpret_cast<eIkControlFlagsBitSet&>(pedData.m_IkFlags);
                CTaskSynchronizedScene *syncedSceneTask = rage_new CTaskSynchronizedScene(sceneData.m_LocalSceneID, pedData.m_AnimPartialHash, sceneData.m_AnimDict, pedData.m_BlendIn, pedData.m_BlendOut, reinterpret_cast<eSyncedSceneFlagsBitSet&>(pedData.m_Flags), reinterpret_cast<eRagdollBlockingFlagsBitSet&>(pedData.m_RagdollBlockingFlags), pedData.m_MoverBlendIn, sceneData.m_NetworkSceneID, ikControlFlags);

                syncedSceneTask->SetNetSequenceID(0);

                if(ped->IsNetworkClone())
                {
                    gnetAssertf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Invalid local scene before giving clone task to %s!", pedNetObject->GetLogName());

                    // don't let the mover blend in lerp clone peds large distances if the scene starts while they are out of sync
                    if(pedData.m_MoverBlendIn != INSTANT_BLEND_IN_DELTA)
                    {
                        const float MAX_POSITION_DELTA_BEFORE_POP = 0.5f;

                        Vector3 targetPosition	= NetworkInterface::GetLastPosReceivedOverNetwork(ped);
                        Vector3 currentPosition = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
                        float   deltaPosSqr	    = (targetPosition - currentPosition).Mag2();

                        if(deltaPosSqr > rage::square(MAX_POSITION_DELTA_BEFORE_POP))
                        {
                            NetworkInterface::GoStraightToTargetPosition(ped);
                        }
                    }

                    syncedSceneTask->SetRunAsAClone(true);
                    ped->GetPedIntelligence()->AddTaskAtPriority(syncedSceneTask, PED_TASK_PRIORITY_PRIMARY, true);
                    
                    gnetAssertf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Invalid local scene after giving clone task to %s!", pedNetObject->GetLogName());

					ped->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, true);
					ped->InstantAIUpdate(true);
					ped->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, false);

                    gnetAssertf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Invalid local scene after updating clone task for %s!", pedNetObject->GetLogName());

                    pedData.m_WaitingForTaskUpdate = true;
                }
                else
                {
                    gnetAssertf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Invalid local scene before giving local task to %s!", pedNetObject->GetLogName());

                    CNetObjPed *netObjPed = SafeCast(CNetObjPed, pedNetObject);
					netObjPed->GivePedScriptedTask(syncedSceneTask, SCRIPT_TASK_SYNCHRONIZED_SCENE, sceneData.m_ScriptName, sceneData.m_ScriptThreadPC);

                    gnetAssertf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Invalid local scene after giving local task to %s!", pedNetObject->GetLogName());
                }

                // make sure we do an ai update now to sync up next frame
                ped->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
                ped->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
            }

            RemovePedFromStartedScenes(pedData.m_PedID, sceneData.m_NetworkSceneID);

            gnetAssertf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID), "Invalid local scene after removing %s from started scenes!", pedNetObject->GetLogName());
        }
    }
}

bool CNetworkSynchronisedScenes::StartLocalSyncedSceneOnPhysical(SceneDescription &sceneData, CPhysical* entity, strStreamingObjectName anim, float blendIn, float blendOut, int flags, unsigned* NET_ASSERTS_ONLY(failReason))
{
	if (!entity->GetDrawable())
	{
		entity->CreateDrawable();
	}

	if (entity->GetDrawable() && entity->GetDrawable()->GetSkeletonData())
	{
		// Lazily create the skeleton if it doesn't exist
		if (!entity->GetSkeleton())
		{
			entity->CreateSkeleton();
		}
		if (entity->GetSkeleton())
		{
			// Lazily create the anim director if it doesn't exist
			if (!entity->GetAnimDirector())
			{
				entity->CreateAnimDirector(*entity->GetDrawable());
			}
			if (entity->GetAnimDirector())
			{
				// Object has to be on the process control list for the animation director to be updated
				if (!entity->GetIsOnSceneUpdate())
				{
					entity->AddToSceneUpdate();
				}

				if (entity->GetIsTypeObject())
				{
					CObject *object = static_cast<CObject*>(entity);

					CTaskSynchronizedScene *task = rage_new CTaskSynchronizedScene(sceneData.m_LocalSceneID, anim, sceneData.m_AnimDict, blendIn, blendOut, reinterpret_cast<eSyncedSceneFlagsBitSet&>(flags), CTaskSynchronizedScene::RBF_NONE, INSTANT_BLEND_IN_DELTA, sceneData.m_NetworkSceneID);
					object->SetTask(task);

#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STARTING_SYNC_SCENE_ON_OBJECT", object->GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Flags", "%d", flags);
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Task", "0x%p", task);
#endif // #if ENABLE_NETWORK_LOGGING

					if (blendIn == INSTANT_BLEND_IN_DELTA)
					{
						object->ForcePostCameraAnimUpdate(true, true);
					}
				}
				else if (entity->GetIsTypeVehicle())
				{
					CVehicle *vehicle = static_cast<CVehicle*>(entity);

					if (vehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
					{
						CTrain *train = static_cast<CTrain*>(vehicle);
						train->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
						train->m_nTrainFlags.bIsSyncedSceneControlled = true;
					}

					CTaskSynchronizedScene *task = rage_new CTaskSynchronizedScene(sceneData.m_LocalSceneID, anim, sceneData.m_AnimDict, blendIn, blendOut, CTaskSynchronizedScene::SYNCED_SCENE_NONE, CTaskSynchronizedScene::RBF_NONE, INSTANT_BLEND_IN_DELTA, sceneData.m_NetworkSceneID);
					vehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(task, VEHICLE_TASK_SECONDARY_ANIM);

					if (blendIn == INSTANT_BLEND_IN_DELTA)
					{
						vehicle->ForcePostCameraAnimUpdate(true, true);
					}
				}

				return true;
			}
#if USE_NET_ASSERTS
			else
			{
				if (failReason != nullptr)
				{
					*failReason = SSSFR_NO_ANIM_DIRECTOR;
				}
			}
#endif // USE_NET_ASSERTS
		}
#if USE_NET_ASSERTS
		else
		{
			if (failReason != nullptr)
			{
				*failReason = SSSFR_NO_SKELETON;
			}
		}
#endif // USE_NET_ASSERTS
	}
#if USE_NET_ASSERTS
	else
	{
		if (!entity->GetDrawable())
		{
			if (failReason != nullptr)
			{
				*failReason = SSSFR_NO_DRAWABLE;
			}
		}
		else if(!entity->GetDrawable()->GetSkeletonData())
		{
			if (failReason != nullptr)
			{
				*failReason = SSSFR_NO_SKELETON_DATA;
			}
		}
	}
#endif // USE_NET_ASSERTS

	return false;
}

void CNetworkSynchronisedScenes::StopLocalSyncedSceneOnPed(PedData &pedData)
{
    netObject *pedNetObject = NetworkInterface::GetNetworkObject(pedData.m_PedID);

    if(pedNetObject)
    {
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(pedNetObject);

        if(ped)
        {
            LOGGING_ONLY(gb_NoCallstackDumpOnPedAbort = true);

            if(ped->IsNetworkClone())
            {
                CTask *syncedSceneTask = ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE);

                if(syncedSceneTask)
                {
                    if (!syncedSceneTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
                    {
                        if (!syncedSceneTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL))
                        {
                            gnetAssertf(0, "%s wouldn't abort immediately!", TASKCLASSINFOMGR.GetTaskName(syncedSceneTask->GetTaskType()));
                        }
                    }

					//B*1752709 Since FlushImmediately calls SetPedOutOfVehicle store any vehicle info here so we can put clone
					//back in the vehicle immediately in the same frame rather than wait for next CNetObjPed::UpdateVehicleForPed
					bool bWasInVehicle = ped->GetIsInVehicle();
					CVehicle *pVehicle = ped->GetMyVehicle();
					s32  iSeat     = pVehicle?pVehicle->GetSeatManager()->GetPedsSeatIndex(ped):-1;

                    ped->GetPedIntelligence()->FlushImmediately(false, false);

					CNetObjPed *netObjPed = SafeCast(CNetObjPed, pedNetObject);
					if(bWasInVehicle)
					{  
						netObjPed->SetClonePedInVehicle(pVehicle,iSeat);
					}

                    ped->GetPedIntelligence()->RecalculateCloneTasks();

                    netObjPed->AllowTaskSetting();
                    ped->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
                    netObjPed->DisallowTaskSetting();

					//B*1601298 instantly reset bound box in case it had expanded through something
					//and is being slowly contracted back to normal.Need to prevent pops when clones end sync scenes
					ped->InstantResetCloneDesiredMainMoverCapsuleData();
                }
            }
            else
            {
                CEvent *pEvent = ped->GetPedIntelligence()->GetEventOfType(EVENT_SCRIPT_COMMAND);

                if(pEvent)
	            {
		            CEventScriptCommand *pEventScript = SafeCast(CEventScriptCommand, pEvent);

		            aiTask *pTask = pEventScript->GetTask();

		            if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_SYNCHRONIZED_SCENE)
		            {
			            ped->GetPedIntelligence()->RemoveEventsOfType(EVENT_SCRIPT_COMMAND);
		            }
	            }

                if(!ped->IsInjured())
                {
					CTask *syncedSceneTask = ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE);
 
					if(syncedSceneTask)
					{
						if (!syncedSceneTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
						{
							if (!syncedSceneTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL))
							{
								gnetAssertf(0, "%s wouldn't abort immediately!", TASKCLASSINFOMGR.GetTaskName(syncedSceneTask->GetTaskType()));
							}
						}
					}
                }
            }

            LOGGING_ONLY(gb_NoCallstackDumpOnPedAbort = false);
        }
    }

    pedData.m_PedID                = NETWORK_INVALID_OBJECT_ID;
    pedData.m_BlendIn              = 0.0f;
    pedData.m_BlendOut             = 0.0f;
    pedData.m_MoverBlendIn         = 0.0f;
    pedData.m_Flags                = 0;
    pedData.m_RagdollBlockingFlags = 0;
    pedData.m_IkFlags              = 0;
    pedData.m_WaitingForTaskUpdate = false;

    pedData.m_AnimPartialHash = 0;
}

void CNetworkSynchronisedScenes::StopLocalSyncedSceneOnPhysical(SceneDescription &sceneData, CPhysical* physical, strStreamingObjectName anim, float blendIn, int flags)
{
	const crClip* pClip = 0;

	Matrix34 startMatrix(M34_IDENTITY);

	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneData.m_LocalSceneID))
	{
		fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(sceneData.m_LocalSceneID, startMatrix);

		atHashString clip = atFinalizeHash(anim);
		pClip = fwAnimManager::GetClipIfExistsByDictIndex(sceneData.m_ClipRequestHelper.GetCurrentlyRequiredDictIndex(), clip.GetHash());
	}

	if (pClip)
	{
		fwAnimDirectorComponentSyncedScene::CalcEntityMatrix(sceneData.m_LocalSceneID, pClip, false, startMatrix, 0.0f);
	}

	if (physical->GetDrawable()->GetSkeletonData() && physical->GetSkeleton() && physical->GetAnimDirector())
	{
		if (physical->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>())
		{
			physical->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>()->StopSyncedScenePlayback(blendIn);
			physical->GetAnimDirector()->RemoveAllComponentsByType(fwAnimDirectorComponent::kComponentTypeSyncedScene);
			physical->RemoveSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT);
		}
	}

	if (physical->GetIsTypeObject())
	{
		CObject *object = static_cast<CObject*>(physical);
		object->SetTask(0);
	}
	else if (physical->GetIsTypeVehicle())
	{
		CVehicle *vehicle = static_cast<CVehicle*>(physical);
		vehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(0, VEHICLE_TASK_SECONDARY_ANIM);
	}

	// if we aborted the scene early pop the object back to it's position at the start of the scene
	eSyncedSceneFlagsBitSet &sceneFlags = reinterpret_cast<eSyncedSceneFlagsBitSet&>(flags);

	if (sceneFlags.BitSet().IsSet(SYNCED_SCENE_NET_ON_EARLY_NON_PED_STOP_RETURN_TO_START))
	{
		if (pClip && sceneData.m_Phase < sceneData.m_PhaseToStopScene)
		{
			physical->SetMatrix(startMatrix, true, true, true);
		}
	}
}

void CNetworkSynchronisedScenes::RemovePedFromStartedScenes(ObjectId pedID, int sceneIDToIgnore)
{
    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        SceneDescription &sceneData = ms_SyncedScenes[index];

        if(sceneData.m_Started && sceneData.m_NetworkSceneID != sceneIDToIgnore)
        {
            for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
            {
                PedData &pedData = sceneData.m_PedData[pedIndex];

                if(pedData.m_PedID == pedID)
                {
                    pedData.m_PedID                = NETWORK_INVALID_OBJECT_ID;
                    pedData.m_BlendIn              = 0.0f;
                    pedData.m_BlendOut             = 0.0f;
                    pedData.m_MoverBlendIn         = 0.0f;
                    pedData.m_Flags                = 0;
                    pedData.m_RagdollBlockingFlags = 0;
                    pedData.m_IkFlags              = 0;
                    pedData.m_WaitingForTaskUpdate = false;
                    pedData.m_AnimPartialHash      = 0;
                }
            }
        }
    }
}

void CNetworkSynchronisedScenes::RemoveNonPedFromStartedScenes(ObjectId nonPedID, int sceneIDToIgnore)
{
    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        SceneDescription &sceneData = ms_SyncedScenes[index];

        if(sceneData.m_Started && sceneData.m_NetworkSceneID != sceneIDToIgnore)
        {
            for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
            {
                NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

                if(nonPedEntityData.m_EntityID == nonPedID)
                {
                    nonPedEntityData.m_EntityID = NETWORK_INVALID_OBJECT_ID;
                    nonPedEntityData.m_BlendIn  = 0.0f;
                    nonPedEntityData.m_BlendOut = 0.0f;
                    nonPedEntityData.m_Flags    = 0;

                    nonPedEntityData.m_Anim.Clear();
                }
            }
        }
    }
}

bool CNetworkSynchronisedScenes::IsPedPartOfPendingSyncedScene(ObjectId pedID)
{
    for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
    {
        SceneDescription &sceneData = ms_SyncedScenes[index];

        if(sceneData.m_Active && sceneData.m_PendingStart)
        {
            for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
            {
                PedData &pedData = sceneData.m_PedData[pedIndex];

                if(pedData.m_PedID == pedID)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

u32 CNetworkSynchronisedScenes::GetNumberPedsInScene(int networkSceneID)
{
	SceneDescription *sceneDescription = GetSceneDescriptionFromNetworkID(networkSceneID);

	u32 pedCount = 0;
	if(sceneDescription)
	{
		for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
		{
			PedData &pedData = sceneDescription->m_PedData[pedIndex];

			if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
			{
				pedCount++;
			}
		}
	}

	return pedCount;
}

#if __BANK

static bool  s_displaySyncedSceneDebug      = false;
static bool  s_holdLastFrame                = false;
static bool  s_isLooped                     = false;
static bool  s_forceLocalUseOfCamera        = false;
static bool  s_PedUseKinematicPhysics       = false;
static bool  s_PedDontInterrupt             = false;
static bool  s_PedOnAbortStopScene          = false;
static bool  s_PedAbortOnWeaponDamage       = false;
static bool  s_PedAbortOnDeath              = false;
static bool  s_PedSetPedOutOfVehicleAtStart = false;
static bool  s_Ped_RBF_Bullets              = false;
static bool  s_Ped_RBF_VehicleImpact        = false;
static bool  s_Ped_RBF_Fire                 = false;
static bool  s_Ped_RBF_Electrocution        = false;
static bool  s_Ped_RBF_PlayerImpact         = false;
static bool  s_Ped_RBF_Explosion            = false;
static bool  s_PedUseSlowMoverBlendIn       = false;
static float s_startRate                    = 1.0f;
static float s_startPhase                   = 0.0f;
static float s_stopPhase                    = 1.0f;
static int   s_SceneID                      = -1;

static int NetworkBank_GetPedSynchronisedSceneFlags()
{
    int flags = 0;

    if(s_PedUseKinematicPhysics)
    {
        flags |= BIT(SYNCED_SCENE_USE_KINEMATIC_PHYSICS);
    }

    if(s_PedDontInterrupt)
    {
        flags |= BIT(SYNCED_SCENE_DONT_INTERRUPT);
    }

    if(s_PedOnAbortStopScene)
    {
        flags |= BIT(SYNCED_SCENE_ON_ABORT_STOP_SCENE);
    }

    if(s_PedAbortOnWeaponDamage)
    {
        flags |= BIT(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE);
    }

    if(s_PedAbortOnDeath)
    {
        flags |= BIT(SYNCED_SCENE_ABORT_ON_DEATH);
    }

    if(s_PedSetPedOutOfVehicleAtStart)
    {
        flags |= BIT(SYNCED_SCENE_SET_PED_OUT_OF_VEHICLE_AT_START);
    }

    return flags;
}

static int NetworkBank_GetPedRagdollBlockingFlags()
{
    int flags = 0;

    if(s_Ped_RBF_Bullets)
    {
        flags |= BIT(RBF_BULLET_IMPACT);
    }

    if(s_Ped_RBF_VehicleImpact)
    {
        flags |= BIT(RBF_VEHICLE_IMPACT);
    }

    if(s_Ped_RBF_Fire)
    {
        flags |= BIT(RBF_FIRE);
    }

    if(s_Ped_RBF_Electrocution)
    {
        flags |= BIT(RBF_ELECTROCUTION);
    }

    if(s_Ped_RBF_PlayerImpact)
    {
        flags |= BIT(RBF_PLAYER_IMPACT);
    }

    if(s_Ped_RBF_Explosion)
    {
        flags |= BIT(RBF_EXPLOSION);
    }

    return flags;
}

static void NetworkBank_RunSynchronisedSceneMapEntityTest()
{
	CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);
	if (focusEntity && focusEntity->GetIsTypeObject())
	{
		Vector3 eulers(0.0f, 0.0f, 0.0f);
		Quaternion sceneRotation;
		sceneRotation.FromEulers(eulers);

		Vector3 scenePosition = VEC3V_TO_VECTOR3(focusEntity->GetTransform().GetPosition());
		s_SceneID = CNetworkSynchronisedScenes::CreateScene(scenePosition, sceneRotation, s_holdLastFrame, s_isLooped, s_startPhase, s_stopPhase, s_startRate);

		if (s_SceneID != -1)
		{
			CNetworkSynchronisedScenes::AddMapEntityToScene(s_SceneID, focusEntity->GetModelNameHash(), VEC3V_TO_VECTOR3(focusEntity->GetTransform().GetPosition()), "", "", 0.5f, 0.5f, 0);
			CNetworkSynchronisedScenes::StartSceneRunning(s_SceneID);
		}
	}
}

static void NetworkBank_RunSynchronisedSceneSinglePedTest()
{
    CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);

    CPed *ped  = (focusEntity && focusEntity->GetIsTypePed()) ? static_cast<CPed *>(focusEntity) : 0;

	if(!ped)
    {
		gnetDebug2("No valid focus ped selected for sync scene test\n");
	}
    else
    {
        Vector3 eulers(0.0f, 0.0f, 0.0f);
	    Quaternion sceneRotation;
	    sceneRotation.FromEulers(eulers);

        Vector3 scenePosition = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

        s_SceneID = CNetworkSynchronisedScenes::CreateScene(scenePosition, sceneRotation, s_holdLastFrame, s_isLooped, s_startPhase, s_stopPhase, s_startRate);

        if(s_SceneID != -1)
        {
            CNetworkSynchronisedScenes::AddPedToScene(ped, s_SceneID, "mini@strip_club@idles@dj@idle_01", "idle_01", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, NetworkBank_GetPedSynchronisedSceneFlags(), NetworkBank_GetPedRagdollBlockingFlags(), s_PedUseSlowMoverBlendIn ? SLOW_BLEND_IN_DELTA : INSTANT_BLEND_IN_DELTA, 0);
            CNetworkSynchronisedScenes::StartSceneRunning(s_SceneID);
        }
    }
}

static void NetworkBank_RunSynchronisedScenePedsAndCameraTest()
{
    CEntity *firstFocusEntity  = CDebugScene::FocusEntities_Get(0);
	CEntity *secondFocusEntity = CDebugScene::FocusEntities_Get(1);

    CPed *firstPed  = (firstFocusEntity && firstFocusEntity->GetIsTypePed())   ? static_cast<CPed *>(firstFocusEntity)  : 0;
    CPed *secondPed = (secondFocusEntity && secondFocusEntity->GetIsTypePed()) ? static_cast<CPed *>(secondFocusEntity) : 0;

	if(!firstPed || !secondPed)
    {
		gnetDebug2("No valid focus peds selected for sync scene test\n");
	}
    else
    {
        Vector3 eulers(0.0f, 0.0f, 0.0f);
	    Quaternion sceneRotation;
	    sceneRotation.FromEulers(eulers);

        Vector3 scenePosition = VEC3V_TO_VECTOR3(firstPed->GetTransform().GetPosition());
        scenePosition.z -= 1.0f;

        s_SceneID = CNetworkSynchronisedScenes::CreateScene(scenePosition, sceneRotation, s_holdLastFrame, s_isLooped, s_startPhase, s_stopPhase, s_startRate);

        if(s_SceneID != -1)
        {
            CNetworkSynchronisedScenes::AddPedToScene(firstPed, s_SceneID, "missintro_bank", "player_tie_up_guard", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, NetworkBank_GetPedSynchronisedSceneFlags(), NetworkBank_GetPedRagdollBlockingFlags(), INSTANT_BLEND_IN_DELTA, 0);
            CNetworkSynchronisedScenes::AddPedToScene(secondPed, s_SceneID, "missintro_bank", "guard_tie_up_guard", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, NetworkBank_GetPedSynchronisedSceneFlags(), NetworkBank_GetPedRagdollBlockingFlags(), INSTANT_BLEND_IN_DELTA, 0);
            CNetworkSynchronisedScenes::AddCameraToScene(s_SceneID, "missintro_bank", "cam_tie_up_guard");

            if(s_forceLocalUseOfCamera)
            {
                CNetworkSynchronisedScenes::ForceLocalUseOfCamera(s_SceneID);
            }

            CNetworkSynchronisedScenes::StartSceneRunning(s_SceneID);
        }
    }
}

static void NetworkBank_RunSynchronisedScenePedAndObjectTest()
{
    CEntity *firstFocusEntity  = CDebugScene::FocusEntities_Get(0);
    CEntity *secondFocusEntity = CDebugScene::FocusEntities_Get(1);
    CPed    *testPed           = (firstFocusEntity && firstFocusEntity->GetIsTypePed())      ? static_cast<CPed *>(firstFocusEntity)    : 0;
    CObject *testObject        = (secondFocusEntity && secondFocusEntity->GetIsTypeObject()) ? static_cast<CObject *>(secondFocusEntity): 0;

    if(!testPed || !testObject)
    {
		gnetDebug2("No valid focus entities selected for sync scene test!\n");
	}
    else
    {
        Vector3 eulers(0.0f, 0.0f, 0.0f);
	    Quaternion sceneRotation;
	    sceneRotation.FromEulers(eulers);

        Vector3 scenePosition = VEC3V_TO_VECTOR3(testPed->GetTransform().GetPosition());
        scenePosition.z -= 1.0f;

        s_SceneID = CNetworkSynchronisedScenes::CreateScene(scenePosition, sceneRotation, s_holdLastFrame, s_isLooped, s_startPhase, s_stopPhase, s_startRate);

        if(s_SceneID != -1)
        {
            /*CNetworkSynchronisedScenes::AddPedToScene(testPed, s_SceneID, "mp_amb_crim", "holdup_victim_20s", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, CTaskSynchronizedScene::SyncedScene_None, 0);
            CNetworkSynchronisedScenes::AddEntityToScene(testObject, s_SceneID, "mp_amb_crim", "holdup_victim_20s_bag", INSTANT_BLEND_IN_DELTA);*/
            CNetworkSynchronisedScenes::AddPedToScene(testPed, s_SceneID, "missfbi_s4mop", "plant_bomb_a", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, BIT(SYNCED_SCENE_ON_ABORT_STOP_SCENE), 0, INSTANT_BLEND_IN_DELTA, 0);
            CNetworkSynchronisedScenes::AddEntityToScene(testObject, s_SceneID, "missfbi_s4mop", "plant_bomb_a_bomb", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, 0);
            CNetworkSynchronisedScenes::StartSceneRunning(s_SceneID);
        }
    }
}

static void NetworkBank_RunSynchronisedScenePedAttachedToEntityTest()
{
    CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);

    CPed *ped  = (focusEntity && focusEntity->GetIsTypePed()) ? static_cast<CPed *>(focusEntity) : 0;

	if(!ped)
    {
		gnetDebug2("No valid focus ped selected for sync scene test\n");
	}
    else
    {
        Vector3 eulers(0.0f, 0.0f, 0.0f);
	    Quaternion sceneRotation;
	    sceneRotation.FromEulers(eulers);

        Vector3 scenePosition = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());

        s_SceneID = CNetworkSynchronisedScenes::CreateScene(scenePosition, sceneRotation, s_holdLastFrame, s_isLooped, s_startPhase, s_stopPhase, s_startRate);

        if(s_SceneID != -1)
        {
            CPhysical *attachEntity = SafeCast(CPhysical, CDebugScene::FocusEntities_Get(1));

            if(!attachEntity)
            {
                attachEntity = FindPlayerPed();
            }

            CNetworkSynchronisedScenes::AttachSyncedSceneToEntity(s_SceneID, attachEntity, 0);
            CNetworkSynchronisedScenes::AddPedToScene(ped, s_SceneID, "mini@strip_club@idles@dj@idle_01", "idle_01", INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, 0, 0, INSTANT_BLEND_IN_DELTA, 0);
            CNetworkSynchronisedScenes::StartSceneRunning(s_SceneID);
        }
    }
}

static void NetworkBank_StopSynchronisedSceneTest()
{
    CNetworkSynchronisedScenes::StopSceneRunning(s_SceneID LOGGING_ONLY(, "NetworkBank_StopSynchronisedSceneTest"));
}

void CNetworkSynchronisedScenes::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Network Synchronised Scenes", false);
        {
            bank->AddToggle("Display Synchronised Scene info", &s_displaySyncedSceneDebug);
            bank->AddToggle("Allow remote synced local player scene requests", &CNetworkSynchronisedScenes::ms_AllowRemoteSyncSceneLocalPlayerRequests);
            bank->AddToggle("Hold last frame", &s_holdLastFrame);
            bank->AddToggle("Looped", &s_isLooped);
            bank->AddToggle("Force local use of camera", &s_forceLocalUseOfCamera);
            bank->AddToggle("Use slow mover blend in", &s_PedUseSlowMoverBlendIn);
            bank->AddSlider("Start rate", &s_startRate, 0.0f, 2.0f, 0.01f);
            bank->AddSlider("Start phase", &s_startPhase, 0.0f, 1.0f, 0.01f);
            bank->AddSlider("Stop phase", &s_stopPhase, 0.0f, 1.0f, 0.01f);

            bank->PushGroup("Ped Synchronised Scene Flags", false);
            {
                bank->AddToggle("SYNCED_SCENE_USE_KINEMATIC_PHYSICS",           &s_PedUseKinematicPhysics);
                bank->AddToggle("SYNCED_SCENE_DONT_INTERRUPT",                  &s_PedDontInterrupt);
                bank->AddToggle("SYNCED_SCENE_ON_ABORT_STOP_SCENE",             &s_PedOnAbortStopScene);
                bank->AddToggle("SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE",          &s_PedAbortOnWeaponDamage);
                bank->AddToggle("SYNCED_SCENE_ABORT_ON_DEATH",                  &s_PedAbortOnDeath);
                bank->AddToggle("SYNCED_SCENE_SET_PED_OUT_OF_VEHICLE_AT_START", &s_PedSetPedOutOfVehicleAtStart);
                
            }
            bank->PopGroup();

            bank->PushGroup("Ped Ragdoll Blocking Flags", false);
            {
                bank->AddToggle("RBF_BULLET_IMPACT",    &s_Ped_RBF_Bullets);
                bank->AddToggle("RBF_VEHICLE_IMPACT",   &s_Ped_RBF_VehicleImpact);
                bank->AddToggle("RBF_FIRE",             &s_Ped_RBF_Fire);
                bank->AddToggle("RBF_ELECTROCUTION",    &s_Ped_RBF_Electrocution);
                bank->AddToggle("RBF_PLAYER_IMPACT",    &s_Ped_RBF_PlayerImpact);
                bank->AddToggle("RBF_EXPLOSION",        &s_Ped_RBF_Explosion);

            }
            bank->PopGroup();

			bank->AddButton("Run Synchronised Scene Map Entity Test", datCallback(NetworkBank_RunSynchronisedSceneMapEntityTest));
            bank->AddButton("Run Synchronised Scene Single Ped Test", datCallback(NetworkBank_RunSynchronisedSceneSinglePedTest));
            bank->AddButton("Run Synchronised Scene Peds And Camera Test", datCallback(NetworkBank_RunSynchronisedScenePedsAndCameraTest));
            bank->AddButton("Run Synchronised Scene Ped And Object Test", datCallback(NetworkBank_RunSynchronisedScenePedAndObjectTest));
            bank->AddButton("Run Synchronised Scene Ped Attached To Entity Test", datCallback(NetworkBank_RunSynchronisedScenePedAttachedToEntityTest));
            bank->AddButton("Stop Synchronised Scene Test", datCallback(NetworkBank_StopSynchronisedSceneTest));
        }
        bank->PopGroup();
    }
}

void CNetworkSynchronisedScenes::DisplayDebugText()
{
    if(s_displaySyncedSceneDebug)
    {
        for(unsigned index = 0; index < MAX_NUM_SCENES; index++)
        {
            SceneDescription &sceneData = ms_SyncedScenes[index];

            if(sceneData.m_Active)
            {
                Vector3 eulers;
                sceneData.m_SceneRotation.ToEulers(eulers);

                char dictName[256];
                if(sceneData.m_AnimDict.TryGetCStr())
                {
                    safecpy(dictName, sceneData.m_AnimDict.TryGetCStr());
                }
                else
                {
                    sprintf(dictName, "Anim Dict Hash (%d)", sceneData.m_AnimDict.GetHash());
                }

                char attachData[256];
                attachData[0] = '\0';

                if(sceneData.m_AttachToEntityID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject *networkObject = NetworkInterface::GetNetworkObject(sceneData.m_AttachToEntityID);

                    if(networkObject)
                    {
                        sprintf(attachData, "Attached To: %s (%d)", networkObject->GetLogName(), sceneData.m_AttachToBone);
                    }
                }

                grcDebugDraw::AddDebugOutput("%d - %s - (CRDI:%d) : Phase (%.2f), Rate (%.2f), Pos (%.2f, %.2f, %.2f), Rot (%.2f, %.2f, %.2f), %s %s %s %s %s, StopPhase:%.2f",
                                             sceneData.m_NetworkSceneID,
                                             dictName,
                                             sceneData.m_ClipRequestHelper.GetCurrentlyRequiredDictIndex(),
                                             sceneData.m_Phase,
											 sceneData.m_Rate,
                                             sceneData.m_ScenePosition.x,
                                             sceneData.m_ScenePosition.y,
                                             sceneData.m_ScenePosition.z,
                                             eulers.x,
                                             eulers.y,
                                             eulers.z,
                                             attachData,
                                             sceneData.m_HoldLastFrame ? "HLF" : "",
                                             sceneData.m_Looped ? "LOOP" : "",
                                             sceneData.m_Active ? "Active" : "",
                                             sceneData.m_Started ? "Started" : "",
                                             sceneData.m_LocallyCreated ? "Local" : "",
                                             sceneData.m_PhaseToStopScene);

                if(sceneData.m_UseCamera)
                {
                    const char *animName = sceneData.m_CameraAnim.TryGetCStr();

                    if(animName)
                    {
                        grcDebugDraw::AddDebugOutput("    Camera Anim Name: %s", animName);
                    }
                    else
                    {
                        grcDebugDraw::AddDebugOutput("    Camera Anim Name: %d", sceneData.m_CameraAnim.GetHash());
                    }

                    if(sceneData.m_ForceLocalUseOfCamera)
                    {
                        grcDebugDraw::AddDebugOutput("    Forcing Local Use Of Camera");
                    }
                }

                for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
                {
                    PedData &pedData = sceneData.m_PedData[pedIndex];

                    if(pedData.m_PedID != NETWORK_INVALID_OBJECT_ID)
                    {
                        char animName[256];
						atHashString anim(atFinalizeHash(pedData.m_AnimPartialHash));
                        if(anim.TryGetCStr())
                        {
                            safecpy(animName, anim.TryGetCStr());
                        }
                        else
                        {
                            sprintf(animName, "Anim Hash (%d)", atFinalizeHash(pedData.m_AnimPartialHash));
                        }

                        netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(pedData.m_PedID);
                        grcDebugDraw::AddDebugOutput("    %s: %s, Blend in (%.2f), Blend Out (%.2f), Flags (%d), RBF (%d), IK (%d), MBI(%.2f) %s",
                                                     networkObject ? networkObject->GetLogName() : "Unknown",
                                                     animName,
                                                     pedData.m_BlendIn,
                                                     pedData.m_BlendOut,
                                                     pedData.m_Flags,
                                                     pedData.m_RagdollBlockingFlags,
                                                     pedData.m_IkFlags,
                                                     pedData.m_MoverBlendIn,
                                                     pedData.m_WaitingForTaskUpdate ? "WFTU" : "");
                    }
                }

                for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
                {
                    NonPedEntityData &nonPedEntityData = sceneData.m_NonPedEntityData[nonPedIndex];

                    if(nonPedEntityData.m_EntityID != NETWORK_INVALID_OBJECT_ID)
                    {
                        char animName[256];
                        if(nonPedEntityData.m_Anim.TryGetCStr())
                        {
                            safecpy(animName, nonPedEntityData.m_Anim.TryGetCStr());
                        }
                        else
                        {
                            sprintf(animName, "Anim Hash (%d)", nonPedEntityData.m_Anim.GetHash());
                        }

                        netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(nonPedEntityData.m_EntityID);
                        grcDebugDraw::AddDebugOutput("    %s: %s, Blend in (%.2f), Blend Out (%.2f), Anim Flags (%d)",
                                                     networkObject ? networkObject->GetLogName() : "Unknown",
                                                     animName,
                                                     nonPedEntityData.m_BlendIn,
                                                     nonPedEntityData.m_BlendOut,
                                                     nonPedEntityData.m_Flags);
                    }
                }
            }
        }
    }
}

#endif // __BANK

void CNetworkSynchronisedScenes::SceneDescription::Reset()
{
    for(unsigned pedIndex = 0; pedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_PEDS_IN_SCENE; pedIndex++)
    {
        PedData &pedData = m_PedData[pedIndex];

        pedData.m_PedID                = NETWORK_INVALID_OBJECT_ID;
        pedData.m_BlendIn              = 0.0f;
        pedData.m_BlendOut             = 0.0f;
        pedData.m_MoverBlendIn         = 0.0f;
        pedData.m_MoverBlendIn         = 0.0f;
        pedData.m_Flags                = 0;
        pedData.m_RagdollBlockingFlags = 0;
        pedData.m_IkFlags              = 0;
        pedData.m_WaitingForTaskUpdate = false;

        pedData.m_AnimPartialHash = 0;
    }

    for(unsigned nonPedIndex = 0; nonPedIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_NON_PED_ENTITIES_IN_SCENE; nonPedIndex++)
    {
        NonPedEntityData &nonPedEntityData = m_NonPedEntityData[nonPedIndex];

        nonPedEntityData.m_EntityID = NETWORK_INVALID_OBJECT_ID;
        nonPedEntityData.m_BlendIn  = 0.0f;
        nonPedEntityData.m_BlendOut = 0.0f;
        nonPedEntityData.m_Flags    = false;

        nonPedEntityData.m_Anim.Clear();
    }

	for (unsigned mapEntityIndex = 0; mapEntityIndex < CNetworkSynchronisedScenes::SceneDescription::MAX_MAP_ENTITIES_IN_SCENE; mapEntityIndex++)
	{
		MapEntityData& mapEntityData = m_MapEntityData[mapEntityIndex];
		mapEntityData.m_EntityNameHash = 0;
		mapEntityData.m_EntityPosition.Zero();
		mapEntityData.m_BlendIn = 0.0f;
		mapEntityData.m_BlendOut = 0.0f;
		mapEntityData.m_Flags = false;

		mapEntityData.m_Anim.Clear();
	}

    m_LocalSceneID          = INVALID_SYNCED_SCENE_ID;
    m_NetworkSceneID        = -1;
    m_NetworkTimeStarted    = 0;
    m_AttachToEntityID      = NETWORK_INVALID_OBJECT_ID;
    m_AttachToBone          = -1;
    m_HoldLastFrame         = false;
    m_Looped                = false;
    m_PhaseToStopScene      = 1.0f;
    m_Active                = false;
    m_LocallyCreated        = false;
    m_PendingStart          = false;
    m_Started               = false;
    m_UseCamera             = false;
    m_ForceLocalUseOfCamera = false;
    m_Stopping              = false;
    m_PendingStop           = false;
    m_Phase                 = 0.0f;
    m_Rate		            = 1.0f;

	m_ScriptName.Clear();
	m_ScriptThreadPC = -1;

	m_ScenePosition.Zero();
    m_SceneRotation.Identity();

    m_AnimDict.Clear();
    m_CameraAnim.Clear();
    m_ClipRequestHelper.ReleaseClips();
}
