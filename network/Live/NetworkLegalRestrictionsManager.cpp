//
// NetworkLegalRestrictionsManager.cpp
//

// Rage headers
#include "system/param.h"
#include "system/timer.h"
#include "rline/socialclub/rlsocialclub.h"

// Framework headers
#include "fwnet/netchannel.h"

// Game headers
#include "network/NetworkInterface.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, legal, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_legal

const u32 NetworkLegalRestrictionsManager::GAMBLING_RESTRICTED_GEOGRAPHIC = ATSTRINGHASH("GAMBLING_RESTRICTED_GEOGRAPHIC", 0x29c6c386);
const u32 NetworkLegalRestrictionsManager::GTAO_CASINO_PURCHASE_CHIPS = ATSTRINGHASH("GTAO_CASINO_PURCHASE_CHIPS", 0x5526f52b);
const u32 NetworkLegalRestrictionsManager::GTAO_CASINO_HOUSE = ATSTRINGHASH("GTAO_CASINO_HOUSE", 0xeddf5bd2);
const u32 NetworkLegalRestrictionsManager::MAX_NUM_RESTRICTIONS = 30;
const u32 NetworkLegalRestrictionsManager::MAX_NUM_RETRIALS = 10;
const u32 NetworkLegalRestrictionsManager::BACKOFF_MSTIMER = (20 * 1000);

NetworkLegalRestrictionsManager::NetworkLegalRestrictionsManager() 
: m_IsValid(false)
, m_Update(true)
, m_NumRestrictions(0)
, m_NumFailedRetrials(0)
, m_BackoffTimer(0)
{
	Init();
}

void 
NetworkLegalRestrictionsManager::Update( )
{
	if (!IsPending())
	{
		//Request was successful.
		if (m_Status.Succeeded() && !IsValid())
		{
			gnetDebug1("GetLegalTerritoryRestrictions - 'Succeeded'.");

			m_NumFailedRetrials = 0;

			m_BackoffTimer = 0;

			m_IsValid = true;

#if !__NO_OUTPUT
			gnetDebug1("Legal Territory Restrictions - %d", m_NumRestrictions);
			for (int i = 0; i < m_NumRestrictions; i++)
			{
				gnetDebug1("Restriction - '%s'.", m_Restrictions[i].c_str());
			}
#endif //!__NO_OUTPUT

#if !__FINAL
			gnetAssert( Gambling(ATSTRINGHASH("GTAO_CASINO_SLOTS", 0xcd517ca1)).IsValid() );
			gnetAssert( Gambling(ATSTRINGHASH("GTAO_CASINO_BLACKJACK", 0x4e7306f6)).IsValid() );
			gnetAssert( Gambling(ATSTRINGHASH("GTAO_CASINO_3CARDPOKER", 0x3d047ace)).IsValid() );
			gnetAssert( Gambling(ATSTRINGHASH("GTAO_CASINO_INSIDETRACK", 0x3e510ed6)).IsValid() );
			gnetAssert( Gambling(ATSTRINGHASH("GTAO_CASINO_ROULETTE", 0x65272e8b)).IsValid() );
			gnetAssert( Gambling(ATSTRINGHASH("GTAO_CASINO_LUCKYWHEEL", 0x4432a44a)).IsValid() );
#endif //!__FINAL
		}

		//Request failed.
		if ((m_Status.Failed() || m_Status.Canceled()) && !m_Update)
		{
			gnetError("GetLegalTerritoryRestrictions - '%s'.", m_Status.Failed() ? "Failed" : "Canceled");
			m_Update = true;
			m_NumFailedRetrials += 1;
			m_BackoffTimer = sysTimer::GetSystemMsTime() + (m_NumFailedRetrials * BACKOFF_MSTIMER);
		}

		//Pending request to update restrictions.
		if (m_Update && m_NumFailedRetrials < MAX_NUM_RETRIALS && m_BackoffTimer < sysTimer::GetSystemMsTime())
		{
			if (!NetworkInterface::IsLocalPlayerOnline())
				return;
			if (!NetworkInterface::IsOnlineRos())
				return;
			if (!RL_VERIFY_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()))
				return;
			if (!NetworkInterface::HasValidRosCredentials(NetworkInterface::GetLocalGamerIndex()))
				return;
			rlRosGeoLocInfo geoLocInfo;
			if (!rlRos::GetGeoLocInfo(&geoLocInfo))
				return;

			m_Status.Reset();

			if(gnetVerify(rlSocialClub::GetLegalTerritoryRestrictions(NetworkInterface::GetLocalGamerIndex(), m_Restrictions.GetElements(), m_Restrictions.GetCapacity(), &m_NumRestrictions, &m_Status)))
			{
				gnetDebug1("Started GetLegalTerritoryRestrictions.");
				m_Update = false;
			}
			else
			{
				gnetError("Failed to Start GetLegalTerritoryRestrictions.");
				m_NumFailedRetrials += 1;
				m_BackoffTimer = sysTimer::GetSystemMsTime() + (m_NumFailedRetrials * BACKOFF_MSTIMER);
			}
		}
	}
}

void 
NetworkLegalRestrictionsManager::Init( )
{
	m_Update = true;

	m_Restrictions.clear();
	m_Restrictions.Reserve(MAX_NUM_RESTRICTIONS);
	m_Restrictions.Resize(MAX_NUM_RESTRICTIONS);
	m_NumRestrictions = 0;
	m_NumFailedRetrials = 0;
	m_BackoffTimer = 0;
}

void 
NetworkLegalRestrictionsManager::Cancel()
{
	if (m_Status.Pending())
	{
		rlSocialClub::Cancel(&m_Status);
	}
	m_Status.Reset();
}

void 
NetworkLegalRestrictionsManager::Shutdown()
{
	Cancel();
	m_IsValid = false;
	m_NumRestrictions = 0;
	m_Restrictions.clear();
	m_NumFailedRetrials = 0;
	m_BackoffTimer = 0;
}

void
NetworkLegalRestrictionsManager::UpdateRestrictions() 
{
	if ( !m_IsValid )
	{
		m_Update = true;
		m_BackoffTimer = 0;

		if ( m_NumFailedRetrials > MAX_NUM_RETRIALS )
		{
			m_NumFailedRetrials = 0;
		}
	}
}

const rlLegalTerritoryRestriction& 
NetworkLegalRestrictionsManager::Access(const u32 restrictionId, const u32 gameType) const
{
	if (m_IsValid)
	{
		for (u32 i = 0; i < m_NumRestrictions; i++)
		{
			if (m_Restrictions[i].m_RestrictionId == restrictionId && m_Restrictions[i].m_GameType == gameType)
			{
				return m_Restrictions[i];
			}
		}
	}

	return rlLegalTerritoryRestriction::INVALID_RESTRICTION;
}

bool 
NetworkLegalRestrictionsManager::CanGamble(const u32 gameType, const bool pvcAllowed) const
{
	if (m_IsValid)
	{
		for (u32 i = 0; i < m_NumRestrictions; i++)
		{
			//Exclude casino chips buying restriction from gambling restrictions.
			if (m_Restrictions[i].m_RestrictionId == GAMBLING_RESTRICTED_GEOGRAPHIC && (m_Restrictions[i].m_GameType == GTAO_CASINO_PURCHASE_CHIPS || m_Restrictions[i].m_GameType == GTAO_CASINO_HOUSE))
			{
				continue;
			}

			//Gambling restrictions.
			if (m_Restrictions[i].m_RestrictionId == GAMBLING_RESTRICTED_GEOGRAPHIC && (0 == gameType || gameType == m_Restrictions[i].m_GameType))
			{
				if (pvcAllowed)
				{
					if (m_Restrictions[i].m_PvcAllowed)
					{
						return true;
					}
				}
				else if (m_Restrictions[i].m_EvcAllowed)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool
NetworkLegalRestrictionsManager::EvcOnlyOnCasinoChipsTransactions( ) const
{
	//If we can gamble then use gambling restrictions.
	if ( NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() )
	{
		return ( !NETWORK_LEGAL_RESTRICTIONS.IsAnyPvcAllowed() );
	}

	//If we are not allowed to gamble then check for buying chips restriction.
	const rlLegalTerritoryRestriction& buyChipsRestriction = NETWORK_LEGAL_RESTRICTIONS.Access(GAMBLING_RESTRICTED_GEOGRAPHIC, GTAO_CASINO_PURCHASE_CHIPS);
	if ( buyChipsRestriction.IsValid() )
	{
		return ( !buyChipsRestriction.m_PvcAllowed );
	}

	//By default we allow to spend PVC/EVC to buy chips.
	return false;
}

//eof 

