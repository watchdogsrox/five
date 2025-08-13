//
// scriptEventTypes.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------

#include "event/EventNetwork.h" 

// Rage headers
#include "string/stringhash.h"
#include "system/new.h"
#include "rline/rlfriend.h"
#include "system/stack.h"

// Framework headers
#include "fwnet/netplayer.h"
#include "fwscript/scriptInterface.h"
#include "fwnet/netblenderlininterp.h"

// Script headers
#include "script/handlers/GameScriptIds.h"
#include "script/script.h"
#include "script/script_channel.h"

// Network headers
#include "network/NetworkInterface.h"
#include "network/Live/livemanager.h"
#include "network/Players/NetGamePlayer.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/Objects/Entities/NetObjVehicle.h"

// Game headers
#include "event/EventGroup.h"
#include "Peds/PedFlagsMeta.h"
#include "Peds/Ped.h"
#include "scene/Physical.h"
#include "weapons/info/WeaponInfo.h"
#include "weapons/info/WeaponInfoManager.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "VehicleAi/VehicleNodeList.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "frontend/FrontendStatsMgr.h"


NETWORK_OPTIMISATIONS()

#define LARGEST_NETWORK_EVENT_CLASS sizeof(CEventNetworkPlayerJoinScript)

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CEventNetwork, CEventNetwork::MAX_NETWORK_EVENTS, 0.67f, atHashString("CEventNetwork",0xd3312d41), LARGEST_NETWORK_EVENT_CLASS);

RAGE_DEFINE_SUBCHANNEL(net, events, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_events

RAGE_DEFINE_SUBCHANNEL(script, net_event_data, DIAG_SEVERITY_DEBUG3)
#undef __script_channel
#define __script_channel script_net_event_data

CompileTimeAssert( sizeof(CEventNetworkPlayerJoinSession			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerLeftSession			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerJoinScript				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerLeftScript				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkStorePlayerLeft				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkStartSession					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkEndSession					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkStartMatch					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkEndMatch						) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkRemovedFromSessionDueToStall ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkRemovedFromSessionDueToComplaints ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkConnectionTimeout			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerSpawn					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkEntityDamage					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerCollectedPickup		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerCollectedAmbientPickup	) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerCollectedPortablePickup) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerDroppedPortablePickup	) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkInviteArrived				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkInviteAccepted				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkInviteConfirmed				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkInviteRejected				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkSummon						) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkScriptEvent					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerSignedOffline			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkSignInStateChanged			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkSignInChangeActioned			) <= LARGEST_NETWORK_EVENT_CLASS);
CompileTimeAssert( sizeof(CEventNetworkRosChanged					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkBail							) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkHostMigration				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkFindSession					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkHostSession					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkJoinSession					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkJoinSessionResponse			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkCheatTriggered				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerArrest					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTimedExplosion				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPrimaryClanChanged			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkClanJoined					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkClanLeft						) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkClanInviteReceived			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkClanInviteRequestReceived    ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkVoiceSessionStarted			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkVoiceSessionEnded			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkVoiceConnectionRequested		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkVoiceConnectionResponse		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkVoiceConnectionTerminated	) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTextMessageReceived	        ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkCloudFileResponse			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPickupRespawned				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPresence_StatUpdate			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetwork_InboxMsgReceived			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPedLeftBehind				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkSessionEvent					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionStarted			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionEvent				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionMemberJoined		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionMemberLeft			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionParameterChanged	) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionStringChanged		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkTransitionGamerInstruction	) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPresenceInvite				) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPresenceInviteRemoved		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPresenceInviteReply		    ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkCashTransactionLog   		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkVehicleUndrivable			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPresenceTriggerEvent			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkFollowInviteReceived			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkAdminInvited			        ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkSpectateLocal			    ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkCloudEvent			        ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkShopTransaction              ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPermissionCheckResult		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkAppLaunched					) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkOnlinePermissionsUpdated		) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkSystemServiceEvent			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkScAdminPlayerUpdated			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkScAdminReceivedCash			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerEnteredVehicle			) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert( sizeof(CEventNetworkPlayerActivatedSpecialAbility ) <= LARGEST_NETWORK_EVENT_CLASS );
CompileTimeAssert(sizeof(CEventNetworkPlayerDeactivatedSpecialAbility) <= LARGEST_NETWORK_EVENT_CLASS);
CompileTimeAssert(sizeof(CEventNetworkPlayerSpecialAbilityFailedActivation) <= LARGEST_NETWORK_EVENT_CLASS);

CompileTimeAssert( CScriptedGameEvent::SIZEOF_DATA == MAX_SCRIPTED_EVENT_DATA_SIZE );

//Minimum speed to consider movement
static const float MIN_SPEED_TO_CONSIDER_MOVEMENT = 0.1f;

fwEvent* CEventNetwork::Clone() const
{
	fwEvent* pClonedEvent = NULL;

	if (scriptVerifyf(CEventNetwork::GetPool(), "CEventNetwork pool has not been created"))
	{
		if (CEventNetwork::GetPool()->GetNoOfFreeSpaces() == 0)
		{
			for (int i=0; i<GetEventScriptNetworkGroup()->GetNumEvents(); i++)
			{
#if !__FINAL
				CEventNetwork* pEvent = static_cast<CEventNetwork*>(GetEventScriptNetworkGroup()->GetEventByIndex(i));

				if (scriptVerify(pEvent))
				{
					SpewToTTY("Event %d: %s", i, pEvent->GetName().c_str());
					pEvent->SpewEventInfo();
				}
#endif // !__FINAL
			}

			scriptAssertf(0, "CEventNetwork event pool is full");
		}
		else
		{
			pClonedEvent = CloneInternal();
#if !__FINAL
			if (scriptVerify(pClonedEvent))
			{
				SpewToTTY("Script network event %s triggered", pClonedEvent->GetName().c_str());
				static_cast<CEventNetwork*>(pClonedEvent)->SpewEventInfo();
			}
#endif // !__FINAL
		}
	}

	return pClonedEvent;
}

atString CEventNetwork::GetName() const 
{ 
#if !__FINAL
	return atString(CEventNames::GetEventName(static_cast<eEventType>(GetEventType()))); 
#else
	return atString("");
#endif // !__FINAL
}

void CEventNetwork::SpewDeletionInfo() const
{
#if !__NO_OUTPUT
	SpewToTTY("Flushing %s", GetName().c_str());
	SpewEventInfo();
#endif
}

#if !__NO_OUTPUT
void CEventNetwork::SpewToTTY_Impl(const char* str, ...) const
{
	char buffer[100];
	va_list args;
	va_start(args,str);
	vformatf(buffer,sizeof(buffer),str,args);
	va_end(args);

	scriptDebugf3("%s", buffer);
}
#endif

bool CEventNetwork::CanCreateEvent()
{
	return CEventNetwork::GetPool() && CEventNetwork::GetPool()->GetNoOfFreeSpaces() > 0;
}

// ----- CEventNetworkEntityDamage -------------------------

static const float ANGLE_VECTORS_POINT_SAME_DIR = 0.7f;
static const float VELOCITY_DIFF_TRESHOLD = 7.0f;

bool  CEventNetworkEntityDamage::PointSameDirection(const float dot)
{
	return dot > ANGLE_VECTORS_POINT_SAME_DIR;
}

bool  CEventNetworkEntityDamage::PointOppositeDirection(const float dot)
{
	return dot < -ANGLE_VECTORS_POINT_SAME_DIR;
}

CVehicle* CEventNetworkEntityDamage::GetVehicle(CEntity* entity)
{
	CVehicle* victimVehicle = NULL;

	if (entity && entity->GetIsPhysical())
	{
		if (entity->GetIsTypePed() && static_cast<CPed*>(entity)->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			victimVehicle = static_cast<CPed *>(entity)->GetMyVehicle();
		}
		else if(entity->GetIsTypeVehicle())
		{
			victimVehicle = static_cast<CVehicle *>(entity);
		}
	}

	return victimVehicle;
}

bool CEventNetworkEntityDamage::IsVehicleVictim(const CVehicle* victim, const CVehicle* damager, const bool victimDriverIsPlayer, const bool damagerDriverIsPlayer)
{
	if (!victim || !damager)
		return false;

	gnetDebug1("............  Collision History:");

	float victimHitFwdDot = 0.0f;
	float victimHitRightDot = 0.0f;
	float damagerHitFwdDot = 0.0f;
	float damagerHitRightDot = 0.0f;

	//Use collision history if we have the collision normal
	if (PhysicsHelpers::GetCollisionRecordInfo(victim, damager, victimHitFwdDot, victimHitRightDot)
		&& PhysicsHelpers::GetCollisionRecordInfo(damager, victim, damagerHitFwdDot, damagerHitRightDot))
	{
		gnetDebug1("....................... Hit: victim=<Fwd=%f, Right=%f>, damager=<Fwd=%f, Right=%f>", victimHitFwdDot, victimHitRightDot, damagerHitFwdDot, damagerHitRightDot);
	}
	else
	{
		gnetDebug1("....................... Failed to retrieve collision record info");
	}

	Vector3 victimVelocityNorm  = victim->GetVelocity();
	Vector3 damagerVelocityNorm = damager->GetVelocity();
	victimVelocityNorm.NormalizeFast();
	damagerVelocityNorm.NormalizeFast();

	bool isVictim = true;
	bool isVictimDone = true;
	if (PointOppositeDirection(victimHitFwdDot)) //Victim: Crash in Front Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(victim->GetTransform().GetB()), victimVelocityNorm);
		if (PointSameDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Victim->Front and Victim Going forward");
			isVictim = false;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Victim->Front and Victim Going backward");
			isVictim = true;
		}
		else
		{
			isVictimDone = false;
		}
	}
	else if (PointSameDirection(victimHitFwdDot)) //Victim: Crash in Back Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(victim->GetTransform().GetB()), victimVelocityNorm);
		if (PointSameDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Victim->Back and Victim Going Forward");
			isVictim = true;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Victim->Back and Victim Going backward");
			isVictim = false;
		}
		else
		{
			isVictimDone = false;
		}
	}
	else if (PointOppositeDirection(victimHitRightDot)) //Victim: Crash in Right Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(victim->GetTransform().GetA()), victimVelocityNorm);
		if (PointSameDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Victim->Right and Victim Going to the right");
			isVictim = false;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Victim->Right and Victim Going to the left");
			isVictim = true;
		}
		else
		{
			isVictimDone = false;
		}
	}
	else if (PointSameDirection(victimHitRightDot)) //Victim: Crash in Left Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(victim->GetTransform().GetA()), victimVelocityNorm);
		if (PointSameDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Victim->Left and Victim Going to the right");
			isVictim = true;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Victim->Left and Victim Going to the left");
			isVictim = false;
		}
		else
		{
			isVictimDone = false;
		}
	}

	bool isDamager = true;
	bool isDamagerDone = true;
	if (PointOppositeDirection(damagerHitFwdDot)) //Damager: Crash in Front Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(damager->GetTransform().GetB()), damagerVelocityNorm);
		if (PointSameDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Damager->Front and Damager Going Forward");
			isDamager = true;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Damager->Front and Damager Going Backward");
			isDamager = false;
		}
		else
		{
			isDamagerDone = false;
		}
	}
	else if (PointSameDirection(damagerHitFwdDot)) //Damager: Crash in Back Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(damager->GetTransform().GetB()), damagerVelocityNorm);

		if (PointSameDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Damager->Back and Damager Going Forward");
			isDamager = false;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Damager->Back and Damager Going Backward");
			isDamager = true;
		}
		else
		{
			isDamagerDone = false;
		}
	}
	else if (PointOppositeDirection(damagerHitRightDot)) //Damager: Crash in Right Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(damager->GetTransform().GetA()), damagerVelocityNorm);

		if (PointSameDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Damager->Right and Damager Going to the Right");
			isDamager = true;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Damager->Right and Damager Going to the Left");
			isDamager = false;
		}
		else
		{
			isDamagerDone = false;
		}
	}
	else if (PointSameDirection(damagerHitRightDot)) //Damager: Crash in Left Side
	{
		const float dot = DotProduct(VEC3V_TO_VECTOR3(damager->GetTransform().GetA()), damagerVelocityNorm);

		if (PointSameDirection(dot))
		{
			gnetWarning("....................... Skip Damage Event: Damager->Left and Damager Going to the Right");
			isDamager = false;
		}
		else if (PointOppositeDirection(dot))
		{
			gnetDebug1("....................... Valid Damage Event: Damager->Left and Damager Going to the Left");
			isDamager = true;
		}
		else
		{
			isDamagerDone = false;
		}
	}

	if (isVictimDone && isDamagerDone)
	{
		if (isVictim && isDamager)
		{
			return true;
		}

		if (!isVictim && !isDamager)
		{
			return false;
		}
	}

	if (isVictimDone)
	{
		if (isVictim)
		{
			return true;
		}

		return false;
	}
	else if (isDamagerDone)
	{
		if (isDamager)
		{
			return true;
		}

		return false;
	}

	const float velocityDiff = victim->GetVelocity().Mag() - damager->GetVelocity().Mag();

	//If we are ambiguous about the responsibility of the collision
	if (victimDriverIsPlayer && !damagerDriverIsPlayer)
	{
		gnetWarning("....................... Skip Damage Event: victimDriverIsPlayer && !damagerDriverIsPlayer");
		return false;
	}
	else if (!victimDriverIsPlayer && damagerDriverIsPlayer)
	{
		gnetDebug1("....................... Valid Damage Event: !victimDriverIsPlayer && damagerDriverIsPlayer");
		return true;
	}
	else if (velocityDiff > VELOCITY_DIFF_TRESHOLD)
	{
		gnetWarning("....................... Skip Damage Event: velocitydiff > VELOCITY_DIFF_TRESHOLD");
		return false;
	}
	else if (velocityDiff < -VELOCITY_DIFF_TRESHOLD)
	{
		gnetDebug1("....................... Valid Damage Event: velocitydiff < -VELOCITY_DIFF_TRESHOLD");
		return true;
	}
	else if (damager->GetVelocity().IsZero() && victim->GetVelocity().IsNonZero())
	{
		gnetWarning("...... Skip Damage Event: damagerVelocity.IsZero() && victimVelocity.IsNonZero().");
		return false;
	}

	gnetDebug1("....................... Valid Damage Event: Default.");
	return true;
}

//Returns true if the damage event should be added.
//  Exceptions:
//    - 2 vehicles colliding and if one is stopped we dont want to add a damage event if the damager is the car stopped;
bool CEventNetworkEntityDamage::IsVehicleResponsibleForCollision(const CVehicle* victim, const CVehicle* damager, const u32 weaponHash)
{
	//Only interested in vehicle collisions.
	if (weaponHash != WEAPONTYPE_RAMMEDBYVEHICLE || !victim || !damager || victim == damager)
	{
		return false;
	}

	CNetObjVehicle* netObjVictim = static_cast<CNetObjVehicle*>(victim->GetNetworkObject());
	CNetObjVehicle* netObjDamager = static_cast<CNetObjVehicle*>(damager->GetNetworkObject());
	if (!netObjVictim || !netObjDamager)
	{
		return false;
	}

	gnetDebug1("Damage Event: Victim=<%s,%s>, Damager=<%s:%s>", netObjVictim->GetLogName(), netObjVictim->IsClone() ?"Clone":"Local", netObjDamager->GetLogName(), netObjDamager->IsClone() ?"Clone":"Local");
	gnetDebug1("Damage Event: Victim=<%f, %f>, Damager=<%f, %f>", victim->GetVelocity().Mag(), victim->GetDistanceTravelled() / ((fwTimer::GetTimeStep()/60)/60), damager->GetVelocity().Mag(), damager->GetDistanceTravelled() / ((fwTimer::GetTimeStep()/60)/60));

	//damager is not a Drivable Car
	if (!damager->m_nVehicleFlags.bIsThisADriveableCar)
	{
		gnetWarning("...... Skip Damage Event: damager %s is not a Drivable Car.", netObjDamager->GetLogName());
		return false;
	}

	//damager has the engine is OFF
	if (!damager->m_nVehicleFlags.bEngineOn)
	{
		gnetWarning("...... Skip Damage Event: damager %s has the engine is OFF.", netObjDamager->GetLogName());
		return false;
	}

	//damager is a Stationary car
	if (damager->m_nVehicleFlags.bIsThisAStationaryCar)
	{
		gnetWarning("...... Skip Damage Event: damager %s is a Stationary car.", netObjDamager->GetLogName());
		return false;
	}

	const bool damagerVehicleIsPlayer = damager->GetDriver() ? damager->GetDriver()->IsAPlayerPed() : false;
	if (!damagerVehicleIsPlayer) //drive by AI ped
	{
		if (!netObjDamager->IsClone() && damager->HasCarStoppedBecauseOfLight(true))
		{
			gnetWarning("...... Skip Damage Event: damager %s is stopped because of a light.", netObjDamager->GetLogName());
			return false;
		}
	}

	const bool victimVehicleIsPlayer = victim->GetDriver() ? victim->GetDriver()->IsAPlayerPed() : false;
	if (!victimVehicleIsPlayer) //drive by AI ped
	{
		if (!netObjVictim->IsClone() && victim->HasCarStoppedBecauseOfLight(true))
		{
			gnetDebug1("...... Valid Damage Event: Victim %s HasCarStoppedBecauseOfLight", netObjVictim->GetLogName());
			return true;
		}
	}

	return IsVehicleVictim(victim, damager, victimVehicleIsPlayer, damagerVehicleIsPlayer);
}

CEventNetworkEntityDamage::CEventNetworkEntityDamage(const CNetObjPhysical* victim, const CNetObjPhysical::NetworkDamageInfo& damageInfo, const bool isResponsibleForCollision) :
m_pVictim(victim->GetEntity()),
m_pDamager(damageInfo.m_Inflictor)
{
	m_EventData.VictimId.Int			= -1;
	m_EventData.DamagerId.Int			= -1;
	m_EventData.Damage.Float			= damageInfo.m_HealthLost + damageInfo.m_ArmourLost;
	m_EventData.EnduranceDamage.Float	= damageInfo.m_EnduranceLost;
	m_EventData.VictimIncapacitated.Int = damageInfo.m_IncapacitatesVictim;
	m_EventData.VictimDestroyed.Int		= damageInfo.m_KillsVictim;
	m_EventData.WeaponUsed.Int			= damageInfo.m_WeaponHash;
	m_EventData.VictimSpeed.Float		= 0.0f;
	m_EventData.DamagerSpeed.Float		= 0.0f;
	m_EventData.IsHeadShot.Int			= damageInfo.m_HeadShoot;
	m_EventData.WithMeleeWeapon.Int		= damageInfo.m_WithMeleeWeapon;

	// get the first 8 bits from the id. That is the actual index of the material
	m_EventData.HitMaterial.Uns = (u8)(damageInfo.m_HitMaterial & 0x00000000000000FFLL);

	const CVehicle* victimVehicle = nullptr;
	CEntity* entity = victim ? victim->GetEntity() : nullptr;
	if (entity)
	{
		victimVehicle = (damageInfo.m_WeaponHash == WEAPONTYPE_RAMMEDBYVEHICLE) ? GetVehicle(entity) : nullptr;
		m_EventData.VictimSpeed.Float = victimVehicle ? victimVehicle->GetVelocity().Mag2() : 0.0f;
	}

	const CVehicle* damagerVehicle = nullptr;
	entity = damageInfo.m_Inflictor;
	if (entity)
	{
		damagerVehicle = (damageInfo.m_WeaponHash == WEAPONTYPE_RAMMEDBYVEHICLE) ? GetVehicle(entity) : nullptr;
		m_EventData.DamagerSpeed.Float = damagerVehicle ? damagerVehicle->GetVelocity().Mag2() : 0.0f;
	}

	//this has been cached in the clone update
	if (victimVehicle && damagerVehicle && victimVehicle->IsNetworkClone())
	{
		m_EventData.IsResponsibleForCollision.Int = isResponsibleForCollision;
	}
	else
	{
		m_EventData.IsResponsibleForCollision.Int = IsVehicleResponsibleForCollision(victimVehicle, damagerVehicle, damageInfo.m_WeaponHash);
	}

	// Custom behaviour for C&C
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		ApplyCopsCrooksModifiers(victimVehicle, damagerVehicle);
	}
}

bool CEventNetworkEntityDamage::RetrieveData(u8* data, const unsigned sizeOfData) const 
{ 
	if (m_pVictim.Get())
	{
		m_EventData.VictimId.Int = CTheScripts::GetGUIDFromEntity(*m_pVictim);
	}

	if (m_pDamager.Get())
	{
		CEntity* entity = m_pDamager.Get();
		if (entity->GetIsTypeVehicle())
		{
			CVehicle* vehicle = static_cast< CVehicle* >(entity);
			CPed* driver = vehicle->GetDriver();
			if (!driver && vehicle->GetSeatManager())
			{
				driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
			}

			if (driver)
			{
				entity = driver;
			}
		}

		m_EventData.DamagerId.Int = CTheScripts::GetGUIDFromEntity(*entity);
	}

	return CEventNetworkWithData<CEventNetworkEntityDamage, EVENT_NETWORK_DAMAGE_ENTITY, sEntityDamagedData>::RetrieveData(data, sizeOfData);
}

fwEvent* CEventNetworkEntityDamage::CloneInternal() const 
{ 
	CEventNetworkEntityDamage* pClone = rage_new CEventNetworkEntityDamage(m_EventData); 

	pClone->m_pVictim  = m_pVictim;
	pClone->m_pDamager = m_pDamager;

	return pClone;
}

void CEventNetworkEntityDamage::SpewEventInfo() const
{
#if !__FINAL

	CPhysical* physical = 0;
	netObject* victim   = 0;
	const CEntity* entityVictim = m_pVictim.Get();
	if (entityVictim && entityVictim->GetIsPhysical())
	{
		physical = SafeCast(CPhysical, const_cast<CEntity*>(entityVictim));
		victim = physical ? physical->GetNetworkObject() : NULL;
	}

	if (victim)
	{
		SpewToTTY("... Victim Health: \"%f\"", physical->GetHealth());
		SpewToTTY("... Victim Max Health: \"%f\"", physical->GetMaxHealth());

		bool isDriver           = false;
		netObject* netvehicle   = NULL;
		netObject* driverNetObj = NULL;
		if (entityVictim->GetIsTypePed())
		{
			CPed* ped = SafeCast(CPed, const_cast<CEntity*>(entityVictim));
			SpewToTTY("... Victim Armour: \"%f\"", ped->GetArmour());
			SpewToTTY("... Victim Flags: \"KilledByHeadShoot=%s,KilledWithMeleeWeapon=%s,KilledByStealth=%s,KilledByTakedown=%s, Knockedout=%s, StandardMelee=%s\""
							,m_EventData.IsHeadShot.Int ? "true":"false"
							, m_EventData.WithMeleeWeapon.Int ? "true":"false"
							,ped->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) ? "true":"false"
							,ped->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown) ? "true":"false"
							,ped->GetPedConfigFlag(CPED_CONFIG_FLAG_Knockedout) ? "true":"false"
							,ped->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStandardMelee) ? "true":"false");
			CVehicle* vehicle = ped && ped->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? ped->GetMyVehicle() : NULL;
			if (vehicle)
			{
				netvehicle = vehicle->GetNetworkObject();
				isDriver = (vehicle->GetDriver() == entityVictim);
			}
		}
		else if(entityVictim->GetIsTypeVehicle())
		{
			CVehicle* vehicle =  SafeCast(CVehicle, const_cast<CEntity*>(entityVictim));
			CPed* driver = vehicle->GetDriver();
			if (!driver && vehicle->GetSeatManager())
			{
				driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
			}

			if (driver)
			{
				driverNetObj = driver->GetNetworkObject();
			}
		}

		const char* playerName = "";
		if(victim->GetObjectType() == NET_OBJ_TYPE_PLAYER && victim->GetPlayerOwner())
		{
			playerName = victim->GetPlayerOwner()->GetGamerInfo().GetName();
		}

		SpewToTTY("... Victim: %s,%s <<%s><%d>> <<%s><%s><%s>>"
			,victim->GetLogName()
			,driverNetObj?driverNetObj->GetLogName():""
			,playerName
			,m_EventData.VictimId
			,victim->IsClone()?"Clone ":"Local"
			,netvehicle?netvehicle->GetLogName():""
			,isDriver?"Driver": (netvehicle?"Passenger":""));
	}
	else
		SpewToTTY("... Victim Id: \"%d\"", m_EventData.VictimId.Int);

	netObject* damager = NULL;
	const CEntity* entityDamager = m_pDamager.Get();
	if (entityDamager && entityDamager->GetIsPhysical())
	{
		CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>(entityDamager));
		damager = physical ? physical->GetNetworkObject() : NULL;
	}

	if (damager)
	{
		bool isDriver           = false;
		netObject* netvehicle   = NULL;
		netObject* driverNetObj = NULL;
		if (entityDamager->GetIsTypePed())
		{
			CPed* ped = SafeCast(CPed, const_cast<CEntity*>(entityDamager));
			CVehicle* vehicle = ped ? ped->GetMyVehicle() : NULL;
			if (vehicle)
			{
				netvehicle = vehicle->GetNetworkObject();
				isDriver = (vehicle->GetDriver() == entityDamager);
			}
		}
		else if(entityDamager->GetIsTypeVehicle())
		{
			CVehicle* vehicle =  SafeCast(CVehicle, const_cast<CEntity*>(entityDamager));
			CPed* driver = vehicle->GetDriver();
			if (!driver && vehicle->GetSeatManager())
			{
				driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
			}

			if (driver)
			{
				driverNetObj = driver->GetNetworkObject();
			}
		}

		const char* playerName = "";
		if(damager->GetObjectType() == NET_OBJ_TYPE_PLAYER && damager->GetPlayerOwner())
		{
			playerName = damager->GetPlayerOwner()->GetGamerInfo().GetName();
		}

		SpewToTTY("... Damager: %s,%s <<%s><%d>> <<%s><%s><%s>>"
			,damager->GetLogName()
			,driverNetObj?driverNetObj->GetLogName():""
			,playerName
			,m_EventData.DamagerId
			,damager->IsClone() ? "Clone ":""
			,netvehicle?netvehicle->GetLogName():""
			,isDriver?"Driver": (netvehicle?"Passenger":""));
	}
	else
	{
		SpewToTTY("... Damager Id: \"%d\"", m_EventData.DamagerId.Int);
	}

	SpewToTTY("...  Victim Speed: \"%f\"", m_EventData.VictimSpeed.Float);
	SpewToTTY("... Damager Speed: \"%f\"", m_EventData.DamagerSpeed.Float);

	SpewToTTY("... Damage: \"%f\"", m_EventData.Damage.Float);
	SpewToTTY("... EnduranceDamage: \"%f\"", m_EventData.EnduranceDamage.Float);
	SpewToTTY("... Victim Incapacitated: \"%s\"", (1 == m_EventData.VictimIncapacitated.Int) ? "True" : "False");
	SpewToTTY("... Victim Destroyed: \"%s\"", (1 == m_EventData.VictimDestroyed.Int) ? "True" : "False");

	SpewToTTY("... Damager IsResponsibleForCollision: \"%s\"", m_EventData.IsResponsibleForCollision.Int ? "True" : "False");

	const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_EventData.WeaponUsed.Int);
	SpewToTTY("... Weapon Used Type: %s (%d)", (wi && wi->GetName()) ? wi->GetName() : "unknown", m_EventData.WeaponUsed.Int);
	SpewToTTY("... Hit Material: %d", m_EventData.HitMaterial.Uns);

#endif // !__FINAL
}

void CEventNetworkEntityDamage::ApplyCopsCrooksModifiers(const CVehicle* victimVehicle, const CVehicle* damagerVehicle)
{
	u32 weaponUsed = (u32)m_EventData.WeaponUsed.Int;
	if (m_pVictim != nullptr)
	{
		// Vehicle to Ped collisions can determine blame, such as a ragdoll falling onto a vehicle
		if (m_pVictim->GetIsTypePed() && weaponUsed == WEAPONTYPE_RUNOVERBYVEHICLE)
		{
			const CPed* victimPed = (CPed*)m_pVictim.Get();
			if (victimPed->GetPedResetFlag(CPED_RESET_FLAG_UseExtendedRagdollCollisionCalculator)
				&& m_pDamager == nullptr)
			{
				// Extended collision scanner was used and there is no instigator.
				// This occurs if a ragdoll fell onto the vehicle. Update weapon hash here
				gnetDebug1("Ragdoll fell onto vehicle, updating weapon used to WEAPONTYPE_FALL");
				m_EventData.WeaponUsed.Int = WEAPONTYPE_FALL;
			}
			else
			{
				// Vehicle is responsible for collision, update flag
				gnetDebug1("Vehicle caused collision, setting IsResponsibleForCollision = True");
				m_EventData.IsResponsibleForCollision.Int = true;
			}
		}
		else if (weaponUsed == WEAPONTYPE_RAMMEDBYVEHICLE && victimVehicle && damagerVehicle)
		{
			bool bIsVictimFault = false;
			bool bIsDamagerFault = false;
			const float fAngleSameDirRadians = DEGREES_TO_RADIANS(CVehicleDamage::sm_Tunables.m_AngleVectorsPointSameDir);

			if (PhysicsHelpers::FindVehicleCollisionFault(victimVehicle, damagerVehicle, bIsVictimFault, fAngleSameDirRadians, CVehicleDamage::sm_Tunables.m_MinFaultVelocityThreshold)
				&& PhysicsHelpers::FindVehicleCollisionFault(damagerVehicle, victimVehicle, bIsDamagerFault, fAngleSameDirRadians, CVehicleDamage::sm_Tunables.m_MinFaultVelocityThreshold))
			{
				// Damager is only blamed if victim did not also instigate the collision
				gnetDebug1("Inter-vehicle collision - IsDamagerFault: %s, IsVictimFault: %s", bIsDamagerFault ? "True" : "False", bIsVictimFault ? "True" : "False");
				m_EventData.IsResponsibleForCollision.Int = bIsDamagerFault && !bIsVictimFault;
			}
		}
	}
}

// ----- CEventNetworkPlayerCollectedPickup -------------------------

CEventNetworkPlayerCollectedPickup::CEventNetworkPlayerCollectedPickup(const CNetGamePlayer& player, const int placementReference, u32 pickupHash, const int pickupAmount, const u32 pickupModel, const int pickupCollected) 
{
	Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	m_EventData.PlayerIndex.Int		= player.GetPhysicalPlayerIndex();
	m_EventData.PlacementReference.Int	= placementReference;
	m_EventData.PickupHash.Int		= pickupHash;
	m_EventData.PickupAmount.Int	= pickupAmount;
	m_EventData.PickupModel.Int		= pickupModel;
	m_EventData.PickupCollected.Int = pickupCollected;
}

CEventNetworkPlayerCollectedPickup::CEventNetworkPlayerCollectedPickup(const int placementReference, u32 pickupHash, const int pickupAmount, const u32 pickupModel, const int pickupCollected) 
{
	m_EventData.PlayerIndex.Int			= INVALID_PLAYER_INDEX;
	m_EventData.PlacementReference.Int	= placementReference;
	m_EventData.PickupHash.Int			= pickupHash;
	m_EventData.PickupAmount.Int		= pickupAmount;
	m_EventData.PickupModel.Int			= pickupModel;
	m_EventData.PickupCollected.Int		= pickupCollected;
}

// ----- CEventNetworkPlayerCollectedAmbientPickup -------------------------

CEventNetworkPlayerCollectedAmbientPickup::CEventNetworkPlayerCollectedAmbientPickup(const CNetGamePlayer& player, u32 pickupHash, const int pickupID, const int pickupAmount, const u32 pickupModel, bool bPlayerGift, bool bDroppedByPed, const int pickupCollected, const int pickupIndex)
{
	Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	m_PickupID              		= pickupID;
	m_EventData.PickupHash.Int		= pickupHash;
	m_EventData.PickupAmount.Int	= pickupAmount;
	m_EventData.PlayerIndex.Int		= player.GetPhysicalPlayerIndex();
	m_EventData.PickupModel.Int		= pickupModel;
	m_EventData.PlayerGift.Int	    = bPlayerGift;
	m_EventData.DroppedByPed.Int    = bDroppedByPed;
	m_EventData.PickupCollected.Int = pickupCollected;
	m_EventData.PickupIndex.Int		= pickupIndex;
}

CEventNetworkPlayerCollectedAmbientPickup::CEventNetworkPlayerCollectedAmbientPickup(u32 pickupHash, const int pickupID, const int pickupAmount, const u32 pickupModel, bool bPlayerGift, bool bDroppedByPed, const int pickupCollected, const int pickupIndex)
{
	m_PickupID                      = pickupID;
	m_EventData.PickupHash.Int		= pickupHash;
	m_EventData.PickupAmount.Int	= pickupAmount;
	m_EventData.PlayerIndex.Int		= INVALID_PLAYER_INDEX;
	m_EventData.PickupModel.Int		= pickupModel;
	m_EventData.PlayerGift.Int	    = bPlayerGift;
	m_EventData.DroppedByPed.Int    = bDroppedByPed;
	m_EventData.PickupCollected.Int = pickupCollected;
	m_EventData.PickupIndex.Int		= pickupIndex;
}

// ----- CEventNetworkPlayerCollectedPortablePickup -------------------------

CEventNetworkPlayerCollectedPortablePickup::CEventNetworkPlayerCollectedPortablePickup(const CNetGamePlayer& player, const int pickupID, const ObjectId pickupNetID, const u32 pickupModel)
{
	Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	m_EventData.PickupID.Int			= pickupID;
	m_EventData.PickupNetID.Int			= pickupNetID;
	m_EventData.PlayerIndex.Int			= player.GetPhysicalPlayerIndex();
	m_EventData.PickupModel.Int			= pickupModel;
}

CEventNetworkPlayerCollectedPortablePickup::CEventNetworkPlayerCollectedPortablePickup(const int pickupID, const u32 pickupModel)
{
	m_EventData.PickupID.Int			= pickupID;
	m_EventData.PickupNetID.Int			= NETWORK_INVALID_OBJECT_ID;
	m_EventData.PlayerIndex.Int			= INVALID_PLAYER_INDEX;
	m_EventData.PickupModel.Int			= pickupModel;
}

// ----- CEventNetworkPlayerDroppedPortablePickup -------------------------

CEventNetworkPlayerDroppedPortablePickup::CEventNetworkPlayerDroppedPortablePickup(const CNetGamePlayer& player, const int pickupID, const ObjectId pickupNetID)
{
	Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	m_EventData.PickupID.Int	= pickupID;
	m_EventData.PickupNetID.Int = pickupNetID;
	m_EventData.PlayerIndex.Int = player.GetPhysicalPlayerIndex();
}

CEventNetworkPlayerDroppedPortablePickup::CEventNetworkPlayerDroppedPortablePickup(const int pickupID)
{
	m_EventData.PickupID.Int = pickupID;
	m_EventData.PickupNetID.Int = NETWORK_INVALID_OBJECT_ID;
	m_EventData.PlayerIndex.Int = INVALID_PLAYER_INDEX;
}

// ----- CEventNetworkPedDroppedWeapon -------------------------

CEventNetworkPedDroppedWeapon::CEventNetworkPedDroppedWeapon(int objectGuid, CPed& ped)
{
	m_EventData.m_ObjectGuid.Int = objectGuid;
	m_EventData.m_PedGuid.Int = CTheScripts::GetGUIDFromEntity(ped);
	
	if (ped.GetNetworkObject())
	{
		m_PedId = ped.GetNetworkObject()->GetObjectID();
	}
	else
	{
		m_PedId = NETWORK_INVALID_OBJECT_ID;
	}
}

// ----- CEventNetworkPlayerArrest -------------------------

CEventNetworkPlayerArrest::CEventNetworkPlayerArrest(CPed* arrester, CPed* arrestee, int ArrestType)
{
	CEntity* pArresterEntity = arrester;
	CEntity* pArresteeEntity = arrestee;
	m_EventData.ArresterIndex.Int = pArresterEntity ? CTheScripts::GetGUIDFromEntity(*pArresterEntity) : 0;
	m_EventData.ArresteeIndex.Int = pArresteeEntity ? CTheScripts::GetGUIDFromEntity(*pArresteeEntity) : 0;
	m_EventData.ArrestType.Int = ArrestType;
}

// ----- CEventNetworkPlayerEnteredVehicle -------------------------
CEventNetworkPlayerEnteredVehicle::CEventNetworkPlayerEnteredVehicle(const CNetGamePlayer& player, CVehicle& vehicle)
{
	m_EventData.PlayerIndex.Int			= player.GetPhysicalPlayerIndex();
	m_EventData.VehicleIndex.Int		= CTheScripts::GetGUIDFromEntity(vehicle);
}

// ----- CEventNetworkPlayerActivatedSpecialAbility -------------------------
CEventNetworkPlayerActivatedSpecialAbility::CEventNetworkPlayerActivatedSpecialAbility(const CNetGamePlayer& player, SpecialAbilityType specialAbility)
{
	m_EventData.PlayerIndex.Int = player.GetPhysicalPlayerIndex();
	m_EventData.SpecialAbility.Int = (int)specialAbility;
}

// ----- CEventNetworkPlayerDeactivatedSpecialAbility -------------------------
CEventNetworkPlayerDeactivatedSpecialAbility::CEventNetworkPlayerDeactivatedSpecialAbility(const CNetGamePlayer& player, SpecialAbilityType specialAbility)
{
	m_EventData.PlayerIndex.Int = player.GetPhysicalPlayerIndex();
	m_EventData.SpecialAbility.Int = (int)specialAbility;
}

// ----- CEventNetworkPlayerDeactivatedSpecialAbility -------------------------
CEventNetworkPlayerSpecialAbilityFailedActivation::CEventNetworkPlayerSpecialAbilityFailedActivation(const CNetGamePlayer& player, SpecialAbilityType specialAbility)
{
	m_EventData.PlayerIndex.Int = player.GetPhysicalPlayerIndex();
	m_EventData.SpecialAbility.Int = (int)specialAbility;
}

// ----- CEventNetworkTimedExplosion -------------------------

CEventNetworkTimedExplosion::CEventNetworkTimedExplosion(CEntity* pVehicle, CEntity* pCulprit)
{
	m_EventData.nVehicleIndex.Int = pVehicle ? CTheScripts::GetGUIDFromEntity(*pVehicle) : 0;
	m_EventData.nCulpritIndex.Int = pCulprit ? CTheScripts::GetGUIDFromEntity(*pCulprit) : 0;
}

// ----- CEventNetworkFiredDummyProjectile -------------------------

CEventNetworkFiredDummyProjectile::CEventNetworkFiredDummyProjectile(CEntity* pFiringPed, CEntity* pProjectile, u32 weaponType)
{
	m_EventData.nFiringPedIndex.Int = pFiringPed ? CTheScripts::GetGUIDFromEntity(*pFiringPed) : 0;
	m_EventData.nFiredProjectileIndex.Int = pProjectile ? CTheScripts::GetGUIDFromEntity(*pProjectile) : 0;
	m_EventData.nWeaponType.Int = weaponType;
}

// ----- CEventNetworkFiredVehicleProjectile -------------------------

CEventNetworkFiredVehicleProjectile::CEventNetworkFiredVehicleProjectile(CEntity* pFiringVehicle, CEntity* pFiringPed, CEntity* pProjectile, u32 weaponType)
{
	m_EventData.nFiringVehicleIndex.Int = pFiringVehicle ? CTheScripts::GetGUIDFromEntity(*pFiringVehicle) : 0;
	m_EventData.nFiringPedIndex.Int = pFiringPed ? CTheScripts::GetGUIDFromEntity(*pFiringPed) : 0;
	m_EventData.nFiredProjectileIndex.Int = pProjectile ? CTheScripts::GetGUIDFromEntity(*pProjectile) : 0;
	m_EventData.nWeaponType.Int = weaponType;
}

// ----- CEventNetworkHostMigration -------------------------

CEventNetworkHostMigration::CEventNetworkHostMigration(const scrThreadId threadId, const CNetGamePlayer& player)
{
	Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	m_EventData.ThreadId.Int = threadId;
	m_EventData.HostPlayerIndex.Int = player.GetPhysicalPlayerIndex();
}

void CEventNetworkHostMigration::SpewEventInfo() const
{
	GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(m_EventData.ThreadId.Int)));

	if (pThread)
	{
		SpewToTTY("... script name: \"%s\"", pThread->GetScriptName());
	}
	else
	{
		SpewToTTY("... script name: **terminated**");
	}

	SpewToTTY("... new host index: \"%d\"", m_EventData.HostPlayerIndex.Int);
}

// ----- CEventNetworkInviteArrived ----------------------

CEventNetworkInviteArrived::CEventNetworkInviteArrived(const unsigned index) 
{
	const UnacceptedInvite* ui = CLiveManager::GetInviteMgr().GetUnacceptedInvite(index);
	
	m_EventData.InviteIndex.Int = index;

	if (scriptVerify(ui))
	{
		m_EventData.GameMode.Int = 0;
		m_EventData.IsFriend.Int = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), ui->m_Inviter) ? TRUE : FALSE;
	}
}

// ----- CEventNetworkScriptEvent ---------------------

CEventNetworkScriptEvent::CEventNetworkScriptEvent(void* data, const unsigned sizeOfData, const netPlayer* fromPlayer)
{
	if (fromPlayer && fromPlayer->IsValid() && fromPlayer->HasValidPhysicalPlayerIndex())
	{
		scrValue* dataNew = (scrValue*)data;
		int size = 0;
		const int eventtype = dataNew[size].Int; ++size;
		const PhysicalPlayerIndex fromPlayerIndex = (PhysicalPlayerIndex)dataNew[size].Int;

		gnetAssertf(fromPlayerIndex == fromPlayer->GetPhysicalPlayerIndex(),
			"eventtype='%d', fromPlayerIndex='%d', realPlayerIndex='%d'", eventtype, fromPlayerIndex, fromPlayer->GetPhysicalPlayerIndex());

		if (fromPlayerIndex != fromPlayer->GetPhysicalPlayerIndex())
		{
			dataNew[size].Int = (int)fromPlayer->GetPhysicalPlayerIndex();

			MetricLAST_VEH metric;
			metric.m_fields[0] = ATSTRINGHASH("TAMPER_WITH_PLAYER_INDEX", 0x7B7A3F83);
			metric.m_fields[1] = eventtype;

			const rlGamerHandle& fromGH = fromPlayer->GetGamerInfo().GetGamerHandle();
			if (gnetVerify(fromGH.IsValid()))
			{
				fromGH.ToString(metric.m_FromGH, COUNTOF(metric.m_FromGH));
				fromGH.ToString(metric.m_ToGH, COUNTOF(metric.m_ToGH));

				if (!fromPlayer->IsLocal())
				{
					const rlGamerHandle& toGH = NetworkInterface::GetLocalGamerHandle();
					toGH.ToString(metric.m_ToGH, COUNTOF(metric.m_ToGH));
				}
			}

			CNetworkTelemetry::AppendSecurityMetric(metric);
		}
	}

	scriptAssertf(sizeOfData <= sizeof(m_EventData), "%s : Data size is too large", GetName().c_str());
	sysMemSet(m_EventData.Data, 0, sizeof(m_EventData));
	sysMemCpy(m_EventData.Data, data, MIN(sizeOfData, sizeof(m_EventData)));
}

void CEventNetworkScriptEvent::SpewEventInfo() const 
{
	const scrValue* dataNew = (const scrValue*)m_EventData.Data;
	int size = 0;
	
	OUTPUT_ONLY( const int eventtype = dataNew[size].Int; )
	++size;
	const PhysicalPlayerIndex playerId = (PhysicalPlayerIndex)dataNew[size].Int;

	const netPlayer* player = NULL;

	if (playerId < MAX_NUM_PHYSICAL_PLAYERS)
	{
		player = NetworkInterface::GetPhysicalPlayerFromIndex(playerId);
	}

	SpewToTTY("... type: \"%d\"", eventtype);

	if (player)
	{
		SpewToTTY("... from player: \"%s\"", player->GetLogName());
	}
	else
	{
		SpewToTTY("... from player: \"%d\"", playerId);
	}
}
// ----- CEventNetworkAttemptHostMigration ---------------------

CEventNetworkAttemptHostMigration::CEventNetworkAttemptHostMigration(const scrThreadId threadId, const CNetGamePlayer& player)
{
	Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

	m_EventData.ThreadId.Int = threadId;
	m_EventData.HostPlayerIndex.Int = player.GetPhysicalPlayerIndex();
}

void CEventNetworkAttemptHostMigration::SpewEventInfo() const
{
	GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(m_EventData.ThreadId.Int)));

	if (pThread)
	{
		SpewToTTY("... script name: \"%s\"", pThread->GetScriptName());
	}
	else
	{
		SpewToTTY("... script name: **terminated**");
	}

	SpewToTTY("... new host index: \"%d\"", m_EventData.HostPlayerIndex.Int);
}
// ----- CEventNetworkPedLeftBehind ---------------------

CEventNetworkPedLeftBehind::CEventNetworkPedLeftBehind(CPed* ped, const u32 reason) : m_pPed(ped), m_Reason(reason)
{
	gnetAssert(ped);

	m_EventData.m_PedId.Int  = -1;
	m_EventData.m_Reason.Int = m_Reason;
#if __ASSERT
	m_PedId = ped && ped->GetNetworkObject() ? ped->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
#endif
}

bool  CEventNetworkPedLeftBehind::RetrieveData(u8* data, const unsigned sizeOfData) const 
{
	if (m_pPed.Get())
	{
		m_EventData.m_PedId.Int = CTheScripts::GetGUIDFromEntity(*m_pPed);
		m_EventData.m_Reason.Int = m_Reason;
	}
#if __ASSERT
	else if (NETWORK_INVALID_OBJECT_ID != m_PedId)
	{
		netObject* networkObj = NetworkInterface::GetNetworkObject(m_PedId);
		gnetAssertf(!networkObj, "No ped point pointer in m_pPed.Get() but network object %s (%d) exists.", networkObj->GetLogName(), m_PedId);
	}
#endif // __ASSERT

	return CEventNetworkWithData< CEventNetworkPedLeftBehind, EVENT_NETWORK_PED_LEFT_BEHIND, sPedLeftBehindData >::RetrieveData(data, sizeOfData);
}

fwEvent* CEventNetworkPedLeftBehind::CloneInternal() const 
{ 
	CEventNetworkPedLeftBehind* pClone = rage_new CEventNetworkPedLeftBehind(m_EventData); 

	pClone->m_pPed   = m_pPed;
	pClone->m_Reason = m_Reason;

#if __ASSERT
	pClone->m_PedId = m_PedId;
#endif

	return pClone;
}


void CEventNetworkPedLeftBehind::SpewEventInfo() const
{
	SpewToTTY("...   m_PedId: \"%d\"", m_EventData.m_PedId);
	SpewToTTY("...  m_Reason: \"%u\"", m_EventData.m_Reason);
}

CEventNetworkCashTransactionLog::CEventNetworkCashTransactionLog(int transactionId, eTransactionIds id, eTransactionTypes type, s64 amount, const rlGamerHandle& hGamer, int itemId)
{
	m_EventData.m_TransactionId.Int = transactionId;
	m_EventData.m_Id.Int            = static_cast<int>(id);
	m_EventData.m_Type.Int          = static_cast<int>(type);
	m_EventData.m_Amount.Int        = static_cast<int>(amount);
	m_EventData.m_ItemId.Int        = itemId;

	//format the amount in a string that can used by script
	char szText[64];
	sysMemSet(szText, 0, sizeof(szText));
	CFrontendStatsMgr::FormatInt64ToCash(amount, szText, NELEM(szText), false);
	safecpy(m_EventData.m_StrAmount, szText);

	if (hGamer.IsValid())
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
	}
	else
	{
		gnetError("CEventNetworkCashTransactionLog - hGamer.IsValid() - invalid Gamer");
	}
}

// ----- CEventNetworkVehicleUndrivable -------------------------

CEventNetworkVehicleUndrivable::CEventNetworkVehicleUndrivable(CEntity* vehicle, CEntity* damager, const int weaponHash)
{
	m_EventData.VehicleId.Int  = vehicle ? CTheScripts::GetGUIDFromEntity(*vehicle) : -1;
	m_EventData.DamagerId.Int  = damager ? CTheScripts::GetGUIDFromEntity(*damager) : -1;
	m_EventData.WeaponUsed.Int = weaponHash;

#if !__FINAL
	m_pVehicle = vehicle;
	m_pDamager = damager;
#endif
}

fwEvent* CEventNetworkVehicleUndrivable::CloneInternal() const 
{ 
	CEventNetworkVehicleUndrivable* pClone = rage_new CEventNetworkVehicleUndrivable(m_EventData); 
#if !__FINAL
	pClone->m_pVehicle = m_pVehicle;
	pClone->m_pDamager = m_pDamager;
#endif // !__FINAL
	return pClone;
}

void CEventNetworkVehicleUndrivable::SpewEventInfo() const
{
#if !__FINAL
	SpewToTTY("... Victim Id: \"%d\"", m_EventData.VehicleId.Int);

	if (m_pVehicle.Get())
	{
		CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>( m_pVehicle.Get() ));

		if (physical)
		{
			netObject* victim = physical ? physical->GetNetworkObject() : NULL;

			if (victim)
			{
				const char* playerName = "";
				netObject* driverNetObj = NULL;

				if( gnetVerify(physical->GetIsTypeVehicle()) )
				{
					CVehicle* vehicle =  SafeCast(CVehicle, physical);

					CPed* driver = vehicle->GetDriver();

					if (!driver && vehicle->GetSeatManager())
					{
						driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
					}

					if (driver)
					{
						driverNetObj = driver->GetNetworkObject();

						if (driverNetObj && driverNetObj->GetPlayerOwner())
						{
							playerName = driverNetObj->GetPlayerOwner()->GetGamerInfo().GetName();
						}
					}
				}

				SpewToTTY("... Victim: %s,%s <<%s><%s>>", victim->GetLogName(), driverNetObj ? driverNetObj->GetLogName():"", playerName, victim->IsClone()?"Clone ":"Local");
			}
		}
	}

	SpewToTTY("... Damager Id: \"%d\"", m_EventData.DamagerId.Int);

	if (m_pDamager.Get() && m_pDamager.Get()->GetIsPhysical())
	{
		CPhysical* physical = SafeCast(CPhysical, const_cast<CEntity*>(m_pDamager.Get()));
		if (physical)
		{
			netObject* damager = physical ? physical->GetNetworkObject() : NULL;
			if (damager)
			{
				bool isDriver           = false;
				netObject* netvehicle   = NULL;
				netObject* driverNetObj = NULL;

				if (physical->GetIsTypePed())
				{
					CPed* ped = SafeCast(CPed, physical);
					CVehicle* vehicle = ped ? ped->GetMyVehicle() : NULL;
					if (vehicle)
					{
						netvehicle = vehicle->GetNetworkObject();
						isDriver = (vehicle->GetDriver() == physical);
					}
				}
				else if(physical->GetIsTypeVehicle())
				{
					CVehicle* vehicle =  SafeCast(CVehicle, physical);

					if (vehicle->GetDriver())
					{
						driverNetObj = vehicle->GetDriver()->GetNetworkObject();
					}
					else if(vehicle->GetSeatManager())
					{
						CPed* driver = vehicle->GetSeatManager()->GetLastPedInSeat(0);

						if (driver)
						{
							driverNetObj = driver->GetNetworkObject();
						}
					}
				}

				const char* playerName = "";
				if(damager->GetObjectType() == NET_OBJ_TYPE_PLAYER && damager->GetPlayerOwner())
				{
					playerName = damager->GetPlayerOwner()->GetGamerInfo().GetName();
				}

				SpewToTTY("... Damager: %s,%s <<%s><%s>> <<%s><%s>>"
					,damager->GetLogName()
					,driverNetObj?driverNetObj->GetLogName():""
					,playerName
					,damager->IsClone() ? "Clone ":""
					,netvehicle?netvehicle->GetLogName():""
					,isDriver?"Driver": (netvehicle?"Passenger":""));
			}
		}
	}

	const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_EventData.WeaponUsed.Int);
	SpewToTTY("... Weapon Used Type: %s (%d)", (wi && wi->GetName()) ? wi->GetName() : "unknown", m_EventData.WeaponUsed.Int);
#endif // !__FINAL
}

#undef __net_channel
#define __net_channel net

#undef __script_channel
#define __script_channel script_net_event_data

// eof
