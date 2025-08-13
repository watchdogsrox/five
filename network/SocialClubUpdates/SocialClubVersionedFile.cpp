//
// NetworkSocialClubUpdates.cpp
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//
// Reads Social Club updates via title file ( news.xml )
// url:bugstar:355527 SOCIAL CLUB - News Message System

// rage
#include "file/device.h"
#include "fwnet/netchannel.h"
#include "rline/ros/rlros.h"
#include "rline/socialclub/rlsocialclub.h"

// game
#include "Network/SocialClubUpdates/SocialClubVersionedFile.h"
#include "Network/Live/SocialClubMgr.h"
#include "Network/Live/livemanager.h"
#include "Frontend/SocialClubMenu.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"

NETWORK_OPTIMISATIONS()

RAGE_DECLARE_SUBCHANNEL(net, sc)
#undef __net_channel
#define __net_channel net_sc

#define MAX_VERSION_WITH_NO_POLICY_CHANGE_PAGE (3)


// As long as the accepted version < the current version, we need to update.
const s16	SocialClubVersionedFile::sInvalidAcceptedVersion	= -1;
const s16	SocialClubVersionedFile::sInvalidOfficialVersion	= 0x7FFF;
const s16	SocialClubVersionedFile::sNotFoundOfficialVersion	= 1;

SocialClubVersionedFile::SocialClubVersionedFile()
{
	m_rlscNetStatus.Reset();
	Shutdown();
}

SocialClubVersionedFile::~SocialClubVersionedFile()
{
	Shutdown();
}

void SocialClubVersionedFile::PreInit(const char* pPolicyTag, SettingId settingId, u32 statHash)
{
	if(!gnetVerifyf(pPolicyTag, "SocialClubVersionedFile::PreInit - Need to have a valid policy name."))
	{
		return;
	}

	m_statHash = statHash;
	m_settingId = settingId;
	m_pPolicyTag = pPolicyTag;
}

void SocialClubVersionedFile::Init()
{
	if (IsInitialized())
	{
		gnetAssertf(0, "SocialClubVersionedFile::Init - Already initialized, shutting down to reinit.");
		Shutdown();
	}

	m_rlscNetStatus.Reset();
	m_rlscNetFFStatus.Reset();

	m_acceptedLocalVersion = GetAcceptedVersion(&m_areStatsValid);
	m_acceptedCloudVersion = sInvalidAcceptedVersion;

	// Start the invalid stat read timer
	m_InvalidStatReadTimer.Reset();

	GoToWaitingOnOfficialVersion();
}

void SocialClubVersionedFile::Shutdown()
{
	m_state = STATE_IDLE;
	m_areStatsValid = false;
	m_acceptedCloudVersion = sInvalidAcceptedVersion;
	m_acceptedLocalVersion = sInvalidAcceptedVersion;
	m_officialCloudVersion = sInvalidOfficialVersion;
	m_officialLocalVersion = sInvalidOfficialVersion;

	m_cloudPolicy.Shutdown();

	if (m_rlscNetStatus.Pending())
		rlSocialClub::Cancel(&m_rlscNetStatus);
	m_rlscNetStatus.Reset();
	if (m_rlscNetFFStatus.Pending())
		rlSocialClub::Cancel(&m_rlscNetFFStatus);
	m_rlscNetFFStatus.Reset();
}

#define INVALID_STATS_TIMEOUT 15000

void SocialClubVersionedFile::Update()
{
	if(IsError() || m_state == STATE_WAITING_ON_OFFICIAL_VERSION)
	{
		if(!m_areStatsValid)
		{
			// Try and get valid stats
			m_acceptedLocalVersion = GetAcceptedVersion(&m_areStatsValid);

			if(!m_areStatsValid)
			{
				// If stat validation has timed out, go to error (if we're not already in error)
				if((m_InvalidStatReadTimer.GetMsTime() >= INVALID_STATS_TIMEOUT) && !IsError())
				{
					// Stats read has failed for too long, go to error so we don't hang
					gnetDebug1("SocialClubVersionedFile::Update -- timed-out because the stats have failed to validate");
					GoToError();
				}
			}
			else if(IsError())
			{
				gnetDebug1("SocialClubVersionedFile::Update -- stats became valid, returning to default state");
				// Stats became valid but we were in error, go back to valid state
				GoToWaitingOnOfficialVersion();
			}
		}
	}
	

	switch(m_state)
	{
	case STATE_IDLE:
	case STATE_UP_TO_DATE:
	case STATE_ERROR:
		break;
	case STATE_WAITING_ON_OFFICIAL_VERSION:
		UpdateWaitingOnOfficialVersion();
		break;

	case STATE_WAITING_FOR_ACCEPTED_CLOUD_VERSION:
		UpdateWaitingForAcceptedCloudVersion();
		break;

	case STATE_WAITING_FOR_POLICY_ACCEPTANCE:
		UpdateWaitingForPolicyAcceptance();
		break;
	};
}

void SocialClubVersionedFile::UpdateWaitingOnOfficialVersion()
{
	if(m_officialCloudVersion != sInvalidOfficialVersion && m_areStatsValid)
	{
		s16 officialVersion = (s16)GetOfficialVersion();

		if(m_acceptedLocalVersion < officialVersion)
		{
			gnetDebug1("SocialClubVersionedFile::UpdateWaitingOnOfficialVersion - %s, Versions mismatch. accepted local version = %d, current version = %d", m_pPolicyTag, m_acceptedLocalVersion, officialVersion);
			GoToWaitingForAcceptedCloudVersion();
		}
		else
		{
			gnetDebug1("SocialClubVersionedFile::UpdateWaitingOnOfficialVersion - %s, Up to Date. accepted local version = %d, current version = %d", m_pPolicyTag, m_acceptedLocalVersion, officialVersion);
			GoToComplete();
		}
	}
}

void SocialClubVersionedFile::UpdateWaitingForAcceptedCloudVersion()
{
	if (m_rlscNetStatus.Pending())
	{
		return;
	}

	s16 officialVersion = (s16)GetOfficialVersion();

	if (m_rlscNetStatus.Failed())
	{
		gnetDebug1("SocialClubVersionedFile::UpdateWaitingForAcceptedCloudVersion - %s, Versions failed. accepted cloud version = %d, current version = %d", m_pPolicyTag, m_acceptedCloudVersion, officialVersion);
		GoToWaitingForPolicyAcceptance();
	}
	else if (m_acceptedCloudVersion < officialVersion)
	{
		gnetDebug1("SocialClubVersionedFile::UpdateWaitingForAcceptedCloudVersion - %s, Versions mismatch. accepted cloud version = %d, current version = %d", m_pPolicyTag, m_acceptedCloudVersion, officialVersion);
		GoToWaitingForPolicyAcceptance();
	}
	else
	{
		gnetDebug1("SocialClubVersionedFile::UpdateWaitingForAcceptedCloudVersion - %s, Up to Date. accepted cloud version = %d, current version = %d", m_pPolicyTag, m_acceptedCloudVersion, officialVersion);
		GoToComplete();
	}
}

void SocialClubVersionedFile::UpdateWaitingForPolicyAcceptance()
{

}

const char* SocialClubVersionedFile::GetError() const
{
	return SocialClubMgr::GetErrorName(m_rlscNetStatus);
}

void SocialClubVersionedFile::AcceptVersion(const bool isCloudVersion)
{
	gnetAssertf(m_state == STATE_WAITING_FOR_POLICY_ACCEPTANCE, "SocialClubVersionedFile::UserAcceptsVersion - The user accepted the current version of the SC %s, but we were in the wrong state and weren't expecting this.", m_pPolicyTag);

	// If we only accepted the local version we also only flag that number as seen.
	s16 officialVersion = isCloudVersion ? (s16)GetOfficialVersion() : m_officialLocalVersion;

	gnetDisplay("AcceptVersion for %s. isCloudVersion[%s], newAccepted[%d]. prevAcceptedCloud[%d], prevAcceptedLocal[%d]", 
		m_pPolicyTag, isCloudVersion ? "true" : "false", officialVersion, m_acceptedCloudVersion, m_acceptedLocalVersion);

	if (isCloudVersion || officialVersion > m_acceptedLocalVersion)
	{
		StatId statId(m_statHash);
		StatsInterface::SetStatData(statId, (u16)officialVersion, STATUPDATEFLAG_LOAD_FROM_SP_SAVEGAME);

		CProfileSettings::GetInstance().Set(m_settingId, (int)officialVersion);
	}

	rtry
	{
		if (officialVersion > m_acceptedCloudVersion)
		{
			gnetDebug1("AcceptLegalPolicy on cloud for %s. Currently accepted: %d. New: %d", m_pPolicyTag, m_acceptedCloudVersion, officialVersion);

			m_rlscNetStatus.Reset();

			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetWarning("Invalid local gamer index"));

			// GTAV hasn't got the full error handling UI so this is almost fire and forget.
			m_rlscNetFFStatus.Reset();
			rcheck(rlSocialClub::AcceptLegalPolicy(localGamerIndex, m_pPolicyTag, officialVersion, &m_rlscNetFFStatus), catchall, gnetWarning("AcceptLegalPolicy failed"));
		}
	}
	rcatchall
	{

	}

	GoToComplete();
}

void SocialClubVersionedFile::GoToWaitingOnOfficialVersion()
{
	m_state = STATE_WAITING_ON_OFFICIAL_VERSION;
	m_rlscNetStatus.Reset();
}

void SocialClubVersionedFile::GoToWaitingForPolicyAcceptance()
{
	m_state = STATE_WAITING_FOR_POLICY_ACCEPTANCE;
	m_rlscNetStatus.Reset();
}

void SocialClubVersionedFile::GoToWaitingForAcceptedCloudVersion()
{
	m_rlscNetStatus.Reset();

	rtry
	{
		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetWarning("Invalid local gamer index"));

		// Requirements for this to happen:
		// 1) User's local accepted version is different than the cloud version
		// 2) Credentials have been invalidated between downloading the cloud version file and this state.
		// Impact:
		// Rather than waiting and risking a flow issue we go directly to the next state.
		// People whose local accepted version file is out of date might see the legal screen once more.
		// Everyone else will see the legal screen because they have to.
		// If the user went offline he'll see an error shortly when any of the next operations to go online fails.
		const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
		rcheck(cred.IsValid(), catchall, gnetWarning("We have no valid credentials but we downloaded a version file."));

		rcheck(rlSocialClub::GetAcceptedLegalPolicyVersion( localGamerIndex, m_pPolicyTag, &m_acceptedCloudVersion, &m_rlscNetStatus ), 
			catchall, gnetWarning("GetAcceptedLegalPolicyVersion failed"));

		m_state = STATE_WAITING_FOR_ACCEPTED_CLOUD_VERSION;
	}
	rcatchall
	{
		gnetWarning("GoToWaitingForAcceptedCloudVersion failed so we go to accept policy since the offline value is out of date");
		GoToWaitingForPolicyAcceptance();
	}
}

void SocialClubVersionedFile::GoToComplete()
{
	m_rlscNetStatus.Reset();
	m_state = STATE_UP_TO_DATE;
	m_acceptedLocalVersion = (s16)GetOfficialVersion();
	m_acceptedCloudVersion = (s16)GetOfficialVersion();
}

void SocialClubVersionedFile::GoToError()
{
//	m_rlscNetStatus.Reset();//Need this to display the error.
	m_state = STATE_ERROR;
}

s16 SocialClubVersionedFile::GetAcceptedVersion(bool* pOutStatsValid) const
{
	bool areStatsValid = true;
	s16 acceptedVersion = sInvalidAcceptedVersion;


	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		if(settings.Exists(m_settingId))
		{
			acceptedVersion = (s16)settings.GetInt(m_settingId);
			gnetDebug1(" SocialClubVersionedFile::GetAcceptedVersion - profile setting = '%d'", acceptedVersion);
		}
	}
	else
	{
		areStatsValid = false;
	}

	StatId statId(m_statHash);
	s16 statVersion = sInvalidAcceptedVersion;
	
	if(CStatsMgr::GetStatsDataMgr().IsStatsDataValid())
	{
		statVersion = static_cast<s16>(StatsInterface::GetUInt16Stat(statId));
	}
	else
	{
		areStatsValid = false;
	}

	if(areStatsValid)
	{
		if(0 < statVersion && acceptedVersion < statVersion)
		{
			acceptedVersion = statVersion;
			CProfileSettings::GetInstance().Set(m_settingId, (int)acceptedVersion);
			gnetDebug1(" SocialClubVersionedFile::GetAcceptedVersion - statVersion='%d'", acceptedVersion);
		}
		else
		{
			StatsInterface::SetStatData(statId, (u16)acceptedVersion, STATUPDATEFLAG_LOAD_FROM_SP_SAVEGAME);
		}
	}

	if(pOutStatsValid)
	{
		*pOutStatsValid = areStatsValid;
	}

#if !__NO_OUTPUT
	static s16 s_acceptedVersion = 0;
	static bool s_First = true;

	if((s_acceptedVersion != acceptedVersion) || s_First)
	{
		gnetDebug1(" SocialClubVersionedFile::GetAcceptedVersion - acceptedVersion='%d'", acceptedVersion);
	}
	s_acceptedVersion = acceptedVersion;
	s_First = false;
#endif

	return acceptedVersion;
}

bool SocialClubVersionedFile::HasEverAccepted() const
{
	s16 acceptedVersion = GetAcceptedVersion();
	if(0 == acceptedVersion || sInvalidAcceptedVersion == acceptedVersion) // If the player has never accepted the policy before.
	{
		return false;
	}
	// If the player is out of date, and the version is too low to show the policy change page, then suppress the policy change page.
	else if(!IsUpToDate() && GetOfficialVersion() <= MAX_VERSION_WITH_NO_POLICY_CHANGE_PAGE)
	{
		return false;
	}

	return true;
}

// eof
