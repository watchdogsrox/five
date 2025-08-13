//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================
#include "NetworkSCPresenceUtil.h"
//rage
#include "rline/scpresence/rlscpresence.h"
#include "rline/rlpresence.h"
#include "snet/session.h"

//framework
#include "fwsys/timer.h"
#include "fwnet/netchannel.h"

//game
#include "Network/NetworkInterface.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Live/livemanager.h"
#include "network/sessions/NetworkGameConfig.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"
#include "Stats/StatsMgr.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, presutil, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_DISPLAY, DIAG_SEVERITY_ERROR)
#undef __net_channel
#define __net_channel net_presutil

using namespace rage;

const float NetworkSCPresenceUtil::sm_DEFAULT_PlayerLocUpdateDist2 = 5000.0f * 5000.0f;

NetworkSCPresenceUtil::NetworkSCPresenceUtil() 
: m_uLastUpdateTimeMs(0)
, m_bTunablesRecvd(false)
, m_PlayerLocUpdateIntervalSec(sm_DEFAULT_PlayerLocUpdateIntervalSec)
, m_PlayerLocUpdateTimeoutMs(sm_DEFAULT_PlayerLocUpdateTimeoutMs)
, m_PlayerLocUpdateDist2(sm_DEFAULT_PlayerLocUpdateDist2)
{
	m_vLastUpdatePos.ZeroComponents();
	RegisterScriptAttributeIds();
}

void NetworkSCPresenceUtil::Update()
{
	if(!SCPresenceSingleton::IsInstantiated())
		return;

	NetworkSCPresenceUtil& instRef = SCPresenceSingleton::GetInstance();
	instRef.UpdateCurrentPlayerLocation();
	instRef.UpdateDefferedAttributes();
	instRef.UpdateGameVersionPresence();
}

void NetworkSCPresenceUtil::UpdateMPSCPresenceInfo( const snSession& inSession )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return;

	int isCheater = CLiveManager::GetSCGamerDataMgr().IsCheater() ? 1 : 0;
    
	rlPresence::SetIntAttribute(localGamerIndex, "cheater", (s64)isCheater, true);

	//Make sure we're in a public session
	if(inSession.Exists() && inSession.GetMaxSlots(RL_SLOT_PUBLIC) > 0)
	{
		const unsigned gameMode = inSession.GetConfig().m_Attrs.GetGameMode();
		rlPresence::SetIntAttribute(localGamerIndex, "gamemode", gameMode, false);

		// leaving the attribute as sessionType for legacy
        const unsigned sessionPurpose = inSession.GetConfig().m_Attrs.GetSessionPurpose();
        rlPresence::SetIntAttribute(localGamerIndex, "sessionType", sessionPurpose, false);
	}

	rlPresence::SetIntAttribute(localGamerIndex, "socketport", CNetwork::GetSocketPort());

	if( const CNetGamePlayer* pLocalNetPlayer = NetworkInterface::GetLocalPlayer() )
	{
		rlPresence::SetIntAttribute(localGamerIndex, "sctv", pLocalNetPlayer->GetTeam() == eTEAM_SCTV, true);
	}

	if( inSession.IsHost() )
	{
		rlPresence::SetIntAttribute(localGamerIndex, "aimmode", NetworkGameConfig::GetMatchmakingAimBucket(), true);
		rlPresence::SetIntAttribute(localGamerIndex, "pool", NetworkGameConfig::GetMatchmakingPool(), true);
	}

	//Force an update of the player location.
	if(SCPresenceSingleton::IsInstantiated())
	{
		SCPresenceSingleton::GetInstance().UpdateCurrentPlayerLocation(true);
		SCPresenceSingleton::GetInstance().UpdateDefferedAttributes(true);
	}
}

void NetworkSCPresenceUtil::UpdateCurrentMission( const char* missionName)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return;

	if(!SCPresenceSingleton::IsInstantiated())
		return;

	NetworkSCPresenceUtil& instRef = SCPresenceSingleton::GetInstance();
	if (missionName && strlen(missionName) > 0)
	{
		instRef.AddDefferedAttribute("curmission", missionName);
	}
	else
	{
		instRef.AddDefferedAttribute("curmission", "NONE");
	}
}

void NetworkSCPresenceUtil::UpdatePlayerLocationParamsFromTunables()
{
	if(!Tunables::GetInstance().HasCloudRequestFinished() || m_bTunablesRecvd)
		return;

	m_bTunablesRecvd = true;

	//check the tunable for updated values
	int tempVal = 0; 
	if( Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PRESENCE_PLAYERLOC_UPDATESEC", 0x824b323b), tempVal) )
	{
		m_PlayerLocUpdateIntervalSec = (u32) tempVal;
	}

	if( Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PRESENCE_PLAYERLOC_TIMEOUTMS", 0xef4ffa06), tempVal) )
	{
		m_PlayerLocUpdateTimeoutMs = (u32) tempVal;
	}

	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PRESENCE_PLAYERLOC_DIST2", 0x53518599), m_PlayerLocUpdateDist2);
}

void NetworkSCPresenceUtil::UpdateCurrentPlayerLocation(bool bForce)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return;

	UpdatePlayerLocationParamsFromTunables();

	//Don't over spam it too much.  
	//This is how frequently to do teh below check.  
	u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
	u32 timeSinceLastUpdateMs = currentTime - m_uLastUpdateTimeMs;
	if ((m_PlayerLocUpdateIntervalSec == 0 && !bForce) || timeSinceLastUpdateMs < m_PlayerLocUpdateIntervalSec * 1000 )
	{
		return;
	}

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (!pPlayerPed)
	{
		return;
	}

	//If we've waiting longer than the big timeout, just update it.
	bool bDoUpdate = timeSinceLastUpdateMs > m_PlayerLocUpdateTimeoutMs || m_uLastUpdateTimeMs == 0 || bForce;

	float fPlayerCurrentSpeed2 = pPlayerPed->GetVelocity().Mag2();
	if(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayerPed->GetMyVehicle())
	{
		fPlayerCurrentSpeed2 = pPlayerPed->GetMyVehicle()->GetVelocity().Mag2();
	}

	//If the player isn't moving and we don't need a forced up, bail.
	if( !bDoUpdate && fPlayerCurrentSpeed2 < 0.1)
	{
		return;
	}

	const Vec3V playerLoc = pPlayerPed->GetTransform().GetPosition();

	//Make sure we've moved far enough to make a different, scaled by speed.
	//At the player's current speed, make sure they've gone far enough given the normal time interval 
	if(!bDoUpdate)
	{
		//current speed x base update interval = Speed scaled distance threshold (faster the player is moving, the bigger the distance).
		float fDist2ThresholdBasedOnSpeed = fPlayerCurrentSpeed2 * ( m_PlayerLocUpdateIntervalSec * m_PlayerLocUpdateIntervalSec );
		float currentDist2Threshold = rage::Max((fDist2ThresholdBasedOnSpeed), m_PlayerLocUpdateDist2);

		//Get the distance the player has moved since our last update to decide if we've gone far enough.	
		Vector3 locDiff = VEC3V_TO_VECTOR3(playerLoc - m_vLastUpdatePos);
		if (locDiff.Mag2() > currentDist2Threshold)
		{
			bDoUpdate = true;
		}
	}

	if(bDoUpdate || m_uLastUpdateTimeMs == 0)
	{
		netDebug1("Player location Presence - time since last update is time=%d, and dist=%f", currentTime-m_uLastUpdateTimeMs, VEC3V_TO_VECTOR3(playerLoc - m_vLastUpdatePos).Mag2());

		//Check for distance changing too fast - player warping.
		static const float DISTANCE_CHANGED_FAST = 100.0f;
		if (0<m_uLastUpdateTimeMs && CStatsMgr::IsPlayerActive() && pPlayerPed->GetDistanceTravelled()<DISTANCE_CHANGED_FAST)
		{
			netDebug1("Setting presence Player location.");

			rlPresence::SetIntAttribute(localGamerIndex, "playerloc_x", (int)playerLoc.GetXf(), false);
			rlPresence::SetIntAttribute(localGamerIndex, "playerloc_y", (int)playerLoc.GetYf(), false);
		}

		m_uLastUpdateTimeMs = currentTime;
		m_vLastUpdatePos = playerLoc;
	}
}

#define ATTR_ONLINE_EDITION "onlineedition"
#define ATTR_ONLINE_EDITION_MP "onlineeditionmp"

void NetworkSCPresenceUtil::UpdateGameVersionPresence()
{
#if RSG_GEN9
	// only do this when we reach an online state
	if(!NetworkInterface::HasValidRosCredentials())
		return;

	// static so we know this changes from our last call
	static eGameEdition s_GameVersion = eGameEdition::GE_UNKNOWN;
	static bool s_First = true;

	// get the current game version
	eGameEdition gameVersion = CGameEditionManager::GetInstance()->GameEditionForTelemetry();

	// should we sent the game version
	const bool sendGameVersion = (s_GameVersion != gameVersion) || s_First;

	// check if this is different to what we know
	if(sendGameVersion)
	{
		SetPresenceAttributeInt(ATTR_ONLINE_EDITION, gameVersion, false);
	}

	// are we in any multiplayer
	static bool s_WasInAnyMultiplayer = false;
	const bool isInAnyMultiplayer = NetworkInterface::IsInAnyMultiplayer();

	// if we have entered multiplayer or our game version value needs updated
	if((isInAnyMultiplayer && !s_WasInAnyMultiplayer) || (isInAnyMultiplayer && sendGameVersion))
	{
		SetPresenceAttributeInt(ATTR_ONLINE_EDITION_MP, gameVersion, false);
	}

	// if we are not in multiplayer, clear the attribute
	if(!isInAnyMultiplayer && s_WasInAnyMultiplayer && IsAttributeSet(ATTR_ONLINE_EDITION_MP))
	{
		ClearAttribute(ATTR_ONLINE_EDITION_MP);
	}

	// update cached values
	s_WasInAnyMultiplayer = isInAnyMultiplayer;
	s_First = false;
#endif
}

bool NetworkSCPresenceUtil::SetPresenceAttributeInt( const char* name, int value, bool bDefferCommit )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	//Check to see if there is a cached value already there
	s64 curLocalValue = 0;
	if(rlPresence::GetIntAttribute(localGamerIndex, name, &curLocalValue))
	{
		//If there is already a value and it's the same, don't worry about it.
		if (curLocalValue == (s64)value)
		{
			return true;
		}
	}

	return rlPresence::SetIntAttribute(localGamerIndex, name, value, !bDefferCommit);
}

bool NetworkSCPresenceUtil::SetPresenceAttributeFloat( const char* name, float value, bool bDefferCommit )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	//Check to see if there is a cached value already there
	double curLocalValue = 0;
	if(rlPresence::GetDoubleAttribute(localGamerIndex, name, &curLocalValue))
	{
		//If there is already a value and it's the same, don't worry about it.
		if (curLocalValue == (double)value)
		{
			return true;
		}

	}

	return rlPresence::SetDoubleAttribute(localGamerIndex, name, value, !bDefferCommit);
}

bool NetworkSCPresenceUtil::SetPresenceAttributeString( const char* name, const char* value, bool bDefferCommit )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	if (!gnetVerifyf(strlen(value) < RLSC_PRESENCE_STRING_MAX_SIZE, "Presence string '%s' is too large for presence string (max size of 128)", value))
	{
		return false;
	}

	//Check to see if there is a cached value already there
	char curLocalValue[RLSC_PRESENCE_STRING_MAX_SIZE];
	if(rlPresence::GetStringAttribute(localGamerIndex, name, curLocalValue))
	{
		//If there is already a value and it's the same, don't worry about it.
		if (atStringHash(curLocalValue) == atStringHash(value))
		{
			return true;
		}
	}

	return rlPresence::SetStringAttribute(localGamerIndex, name, value, !bDefferCommit);
}

bool NetworkSCPresenceUtil::ClearAttribute(const char* name)
{
	return rlPresence::ClearAttribute(NetworkInterface::GetLocalGamerIndex(), name);
}

bool NetworkSCPresenceUtil::HasAttribute(const char* name)
{
	return rlPresence::HasAttribute(NetworkInterface::GetLocalGamerIndex(), name);
}

bool NetworkSCPresenceUtil::IsAttributeSet(const char* name)
{
	return rlPresence::IsAttributeSet(NetworkInterface::GetLocalGamerIndex(), name);
}

void NetworkSCPresenceUtil::PresenceAttrId::Set( const char* name, int defferedDelaySecs )
{
	gnetAssertf(strlen(name) < RLSC_PRESENCE_ATTR_NAME_MAX_SIZE, "Script SCPresence Attribute ID name is too long: %s is %" SIZETFMT "u characters", name, strlen(name));
	safecpy(m_attrName, name);
	m_hashValue = atStringHash(m_attrName);
	m_defferedSecs = defferedDelaySecs;
	gnetDebug3("Adding Script PresenceAttrId %s [%d]", m_attrName, m_hashValue);
}

//this should match the HASH_ENUM SC_PRES_ATTR_ID_ENUM
void NetworkSCPresenceUtil::RegisterScriptAttributeIds()
{
	m_AttrIdMap.Append().Set("mp_mis_str");
	m_AttrIdMap.Append().Set("mp_mis_inst");
	m_AttrIdMap.Append().Set("mp_team");
	m_AttrIdMap.Append().Set("mp_cash");
	m_AttrIdMap.Append().Set("mp_rank");
	m_AttrIdMap.Append().Set("mp_curr_gamemode");
	m_AttrIdMap.Append().Set("mp_mis_id");
}

const NetworkSCPresenceUtil::PresenceAttrId* NetworkSCPresenceUtil::FindInfoForAttrId( int attributeId ) const
{
	for (int i = 0; i < m_AttrIdMap.GetCount(); ++i)
	{
		if( m_AttrIdMap[i].m_hashValue == attributeId)
			return &m_AttrIdMap[i];
	}

	return NULL;
}

bool NetworkSCPresenceUtil::Script_SetPresenceAttributeInt( int attributeID, int value )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	const PresenceAttrId* pAttributeId = FindInfoForAttrId(attributeID);
	if (pAttributeId == NULL)
	{
		gnetError("No script PresenceAttrId found for %d", attributeID);
		return false;
	}

	if (pAttributeId->IsDeffered())
	{
		AddDefferedAttribute(pAttributeId->m_attrName, value, pAttributeId->m_defferedSecs);
		return true;
	}
	else
		//Even though it's not a deffered attribute, we only want to send it when we have other stuff to send.
		return SetPresenceAttributeInt(pAttributeId->m_attrName, value, true);
}

bool NetworkSCPresenceUtil::Script_SetPresenceAttributeFloat( int attributeID, float value )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	const PresenceAttrId* pAttributeId = FindInfoForAttrId(attributeID);
	if (pAttributeId == NULL)
	{
		gnetError("No script PresenceAttrId found for %d", attributeID);
		return false;
	}

	if (pAttributeId->IsDeffered())
	{
		AddDefferedAttribute(pAttributeId->m_attrName, value, pAttributeId->m_defferedSecs);
		return true;
	}
	else
		//Even though it's not a deffered attribute, we only want to send it when we have other stuff to send.
		return SetPresenceAttributeFloat(pAttributeId->m_attrName, value, true);
}

bool NetworkSCPresenceUtil::Script_SetPresenceAttributeString( int attributeID, const char* value )
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	const PresenceAttrId* pAttributeId = FindInfoForAttrId(attributeID);
	if (pAttributeId == NULL)
	{
		gnetError("No script PresenceAttrId found for %d", attributeID);
		return false;
	}

	if (pAttributeId->IsDeffered())
	{
		AddDefferedAttribute(pAttributeId->m_attrName, value, pAttributeId->m_defferedSecs);
		return true;
	}
	else
		//Even though it's not a deffered attribute, we only want to send it when we have other stuff to send.
		return SetPresenceAttributeString(pAttributeId->m_attrName, value, true);  
}

bool NetworkSCPresenceUtil::Script_SetActivityAttribute(int nActivityID, float fRating)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	if(!gnetVerifyf(nActivityID >= 0 && nActivityID < MAX_NUM_SCRIPT_ACTIVITY_ATTRS, "Invalid activity ID of %d", nActivityID))
		return false;

	// build full name
	char szFullName[RLSC_PRESENCE_ATTR_NAME_MAX_SIZE];
	formatf(szFullName, RLSC_PRESENCE_ATTR_NAME_MAX_SIZE, "act_%d", nActivityID);

	// flag this attribute as set and stash rating
	m_aActivityAttributes[nActivityID].m_bIsSet = true; 
	m_aActivityAttributes[nActivityID].m_fActivityRating = fRating; 

	// fire up to presence
	return SetPresenceAttributeFloat(szFullName, fRating, false);
}

bool NetworkSCPresenceUtil::IsActivityAttributeSet(int nActivityID)
{
	if(!gnetVerifyf(nActivityID >= 0 && nActivityID < MAX_NUM_SCRIPT_ACTIVITY_ATTRS, "Invalid activity ID of %d", nActivityID))
		return false;

	return m_aActivityAttributes[nActivityID].m_bIsSet;
}

void NetworkSCPresenceUtil::UpdateDefferedAttributes(bool bForce /*= false*/)
{
	if (m_defferedAttributeList.GetCount() == 0)
	{
		return;
	}

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		m_defferedAttributeList.ClearAll();
		return;
	}

	u32 currentSysTimeMs = fwTimer::GetSystemTimeInMilliseconds();
	for (int i = 0; i < m_defferedAttributeList.GetCount(); i++)
	{
		const DefferedAttributeUpdate& rAttr = m_defferedAttributeList.GetIndex(i);

		if (rAttr.IsExpired(currentSysTimeMs) || bForce)
		{
			gnetDebug3("Submitting deffered attribute %s", rAttr.GetName());
			//Push to attributes to presence
			switch (rAttr.GetType())
			{
			case DefferedAttributeUpdate::STRING:
				SetPresenceAttributeString(rAttr.GetName(), rAttr.GetString(), false);
				break;
			case DefferedAttributeUpdate::INT:
				SetPresenceAttributeInt(rAttr.GetName(), rAttr.GetInt(), false);
				break;
			case DefferedAttributeUpdate::FLOAT:
				SetPresenceAttributeFloat(rAttr.GetName(), rAttr.GetFloat(), false);
				break;
			case DefferedAttributeUpdate::NONE:
				gnetError("Invalid Attribute in list");
				break;
			}
		}
	}

	if(bForce)
	{
		m_defferedAttributeList.ClearAll();
	}
	else
	{
		m_defferedAttributeList.ClearExpired(currentSysTimeMs);
	}
}

void NetworkSCPresenceUtil::AddDefferedAttribute( const char* name, int value, u32 secDelay /*= DEFAULT_DEFFERED_DELAY_SEC*/ )
{
	//Check to see if it's there aready and update...otherwise, add a new one.
	int index = m_defferedAttributeList.GetIndexByName(name);
	if (index >= 0)
	{
		m_defferedAttributeList.GetIndexNonConst(index).UpdateValue(value);
	}
	else
		m_defferedAttributeList.Add().Set(name, secDelay, value);
}

void NetworkSCPresenceUtil::AddDefferedAttribute( const char* name, float value, u32 secDelay /*= DEFAULT_DEFFERED_DELAY_SEC*/ )
{
	//Check to see if it's there aready and update...otherwise, add a new one.
	int index = m_defferedAttributeList.GetIndexByName(name);
	if (index >= 0)
	{
		m_defferedAttributeList.GetIndexNonConst(index).UpdateValue(value);
	}
	else
		m_defferedAttributeList.Add().Set(name, secDelay, value);
}

void NetworkSCPresenceUtil::AddDefferedAttribute( const char* name, const char* value, u32 secDelay /*= DEFAULT_DEFFERED_DELAY_SEC*/ )
{
	//Check to see if it's there aready and update...otherwise, add a new one.
	int index = m_defferedAttributeList.GetIndexByName(name);
	if (index >= 0)
	{
		m_defferedAttributeList.GetIndexNonConst(index).UpdateValue(value);
	}
	else
		m_defferedAttributeList.Add().Set(name, secDelay, value);
}

NetworkSCPresenceUtil::DefferedAttributeUpdate& NetworkSCPresenceUtil::DefferedAttributeUpdate::operator=(const DefferedAttributeUpdate& that)
{
	Reset();
	m_type = that.m_type;
	safecpy(m_name, that.m_name);
	m_updateSysTimeMs = that.m_updateSysTimeMs;
	if (m_type == STRING)
	{
		m_asString = that.m_asString;
	}
	else
	{
		m_asInt = that.m_asInt;
	}

	return *this;
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::Set( const char* name, u32 secDelay, int value )
{
	Reset();
	m_type = INT;
	SetName(name);
	m_asInt = value;
	SetDelay(secDelay);
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::Set( const char* name, u32 secDelay, float value )
{
	Reset();
	m_type = FLOAT;
	SetName(name);
	m_asFloat = value;
	SetDelay(secDelay);
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::Set( const char* name, u32 secDelay, const char* value )
{
	Reset();
	m_type = STRING;
	SetName(name);
	m_asString = value;
	SetDelay(secDelay);
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::SetName( const char* name)
{
	safecpy(m_name, name);
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::SetDelay( u32 secDelay )
{
	m_updateSysTimeMs = fwTimer::GetSystemTimeInMilliseconds() + (secDelay * 1000);
}

bool NetworkSCPresenceUtil::DefferedAttributeUpdate::IsExpired(u32 currentTimeMs) const
{
	return (m_updateSysTimeMs != 0 && currentTimeMs > m_updateSysTimeMs) || GetType() == NONE;
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::UpdateValue( int value )
{
	gnetAssert(m_type == INT);
	m_asInt = value;
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::UpdateValue( float value )
{
	gnetAssert(m_type == FLOAT);
	m_asFloat = value;
}

void NetworkSCPresenceUtil::DefferedAttributeUpdate::UpdateValue( const char* value )
{
	gnetAssert(m_type == STRING);
	m_asString = value;
}

void NetworkSCPresenceUtil::DefferedAttributeUpdateList::ClearAll()
{
	//Reset them all, then clean them up.
	for (int i = 0; i < m_defferedAttributes.GetCount(); i++)
	{
		m_defferedAttributes[i].Reset();
	}
	m_defferedAttributes.Reset();
}

void NetworkSCPresenceUtil::DefferedAttributeUpdateList::ClearExpired(u32 currentSysTimeMs)
{
	if (m_defferedAttributes.GetCount() == 0)
	{
		return;
	}

	//Order doesn't matter, so go from the back of the list
	for(int i = m_defferedAttributes.GetCount() - 1; i >= 0; i--)
	{
		if (m_defferedAttributes[i].IsExpired(currentSysTimeMs))
		{
			m_defferedAttributes.DeleteFast(i);
		}
	}
}

NetworkSCPresenceUtil::DefferedAttributeUpdate& NetworkSCPresenceUtil::DefferedAttributeUpdateList::Add()
{
	return m_defferedAttributes.Grow(8);
}

int NetworkSCPresenceUtil::DefferedAttributeUpdateList::GetIndexByName( const char* name )
{
	for (int i = 0; i < m_defferedAttributes.GetCount(); i++)
	{
		const DefferedAttributeUpdate& rAttr = m_defferedAttributes[i];
		if (strcmp(name, rAttr.GetName()) == 0)
		{
			return i;
		}
	}

	return -1;
}
