//
// NetworkTelemetry.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "peds/PlayerInfo.h"

#if RSG_SCE
#include <libnetctl.h>
#endif

// Rage headers
#include "file/stream.h"
#include "net/nethardware.h"
#include "profile/timebars.h"
#include "data/rson.h"
#include "fragment/manager.h"
#include "atl/delegate.h"
#include "net/connectionmanager.h"
#include "net/time.h"
#include "rline/ros/rlrostitleid.h"
#include "rline/rl.h"

// Framework headers
#include "fwnet/netchannel.h"
#include "fwcommerce/CommerceUtil.h"

// Network headers
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/NetworkDebugTelemetry.h"
#include "Network/Live/NetworkSCPresenceUtil.h"
#include "Network/Live/livemanager.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "network/Sessions/NetworkSession.h"
#include "network/Sessions/NetworkVoiceSession.h"
#include "network/Live/NetworkRemoteCheaterDetector.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"
#include "Network/Voice/NetworkVoice.h"

// Stats headers
#include "Stats/StatsInterface.h"
#include "Stats/stats_channel.h"
#include "Stats/MoneyInterface.h"

// Game headers
#include "audio/northaudioengine.h"
#include "Core/Game.h"
#if RSG_GEN9
#include "core/gameedition.h"
#endif
#include "cutscene/CutSceneManagerNew.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/landing_page/messages/uiLandingPageMessages.h"
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/UIContexts.h"
#include "Game/ModelIndices.h"
#include "peds/ped.h"
#include "peds/pedmotiondata.h"
#include "scene/world/gameWorld.h"
#include "audio/radioaudioentity.h"
#include "frontend/ProfileSettings.h"
#include "modelinfo/MloModelInfo.h"
#include "system/FileMgr.h"
#include "scene/portals/PortalTracker.h"
#include "scene/portals/Portal.h"
#include "security/ragesecgameinterface.h"

#if GEN9_STANDALONE_ENABLED
#include "peds/CriminalCareer/CriminalCareerDefs.h"
#endif

#if RSG_PC
#include "system/SystemInfo.h"
#include "rline/cloud/rlcloud.h"
#include "rline/rlpc.h"
#pragma comment(lib, "version.lib") 
#include "atl/map.h"
#include "system/InfoState.h"
#endif

#include <time.h>

#if RSG_OUTPUT
#include "scene/scene.h"
#include "control/gamelogic.h"
#include "control/leveldata.h"
#include "script/script.h"
#endif

NETWORK_OPTIMISATIONS()

extern __THREAD int RAGE_LOG_DISABLE;

XPARAM(nonetwork);
XPARAM(nonetlogs);

namespace rage
{
	XPARAM(logfile);
}

#if RSG_XBOX || RSG_PC
XPARAM(netlogprefix);
#endif

PARAM(telemetrywritefile, "If present then log local telemetry file.");
PARAM(telemetrybasefilename, "Base name of telemetry submission file.");
PARAM(telemetryplaytest, "Id for uniquely identifying stats for companywide playtests.");
PARAM(telemetryFlushCallstack, "Prints the callstack when a telemetry flush is requested.");
PARAM(telemetryLandingNavAll, "Let through all landing nav metrics");

#if RSG_PC
PARAM(statsDumpPCSettingsFile, "Write PC settings file locally");
#endif

#if RSG_PC && !__NO_OUTPUT
namespace rage
{
	XPARAM(processinstance);
}
#endif

#include "rline/rltitleid.h"
namespace rage
{
	extern const rlTitleId* g_rlTitleId;
}

RAGE_DEFINE_SUBCHANNEL(net, telemetry, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_telemetry

static const unsigned DEFAULT_DELAY_FLUSH_INTERVAL  = 3 * 1000; //Delay normal flush request so that we can call more than once and grab then together.
static const unsigned DEFAULT_AUD_TRACK_INTERVAL    = 10000;     //interval between sending audio track changes

static const int DEFAULT_METRIC_MAX_FPS_SAMPLES = 0; // We'll send the metric every 7500 frames, so around every 2 minutes @ 60 fps - 0 means the feature is deactivated

const atHashWithStringNotFinal AUD_OFF("OFF",0x77E9145); //Radio station is OFF

u32         CNetworkTelemetry::m_flushRequestedTime = 0;

unsigned    CNetworkTelemetry::m_LastAudTrackTime = 0;

static bool s_AppendMetricVehicleDriven = true;

static bool s_privateFmSession = false;
static bool s_closedFmSession = false;

u64         s_CurMpSessionId = 0;
u8          s_CurMpGameMode = 0;
u8          s_CurMpNumPubSlots = 0;
u8          s_CurMpNumPrivSlots = 0;

u64         s_previousMatchHistoryId = 0;
u64         s_MatchHistoryId = 0;

#if !__NO_OUTPUT
// In logging builds, we store  a bit more information about when/who set the matchHistoryId to print more info when we assert
char s_MatchHistoryIdLastModified[256] = {0};
#endif // !__NO_OUTPUT

u64         s_CurSpSessionId = 0;
bool        s_replayingMission = false;
u32         s_MatchStartedTime = 0;

static int s_metricMaxFpsSamples = DEFAULT_METRIC_MAX_FPS_SAMPLES;
// Whether or not we'll be collecting samples and sending the metrics, based on the tunable METRIC_INGAME_FPS_POP_SAMPLE
static bool s_sendInGameFpsMetrics = false;

unsigned CNetworkTelemetry::sm_VoiceChatTrackInterval=VOICE_CHAT_TRACK_INTERVAL;
unsigned CNetworkTelemetry::sm_LastVoiceChatTrackTime=0;
bool CNetworkTelemetry::sm_HasUsedVoiceChatSinceLastTime=false;
bool CNetworkTelemetry::sm_ShouldSendVoiceChatUsage=true;
bool CNetworkTelemetry::sm_PreviouslySentVoiceChatUsage=false;

unsigned    CNetworkTelemetry::sm_AudTrackInterval         = DEFAULT_AUD_TRACK_INTERVAL;
unsigned    CNetworkTelemetry::sm_AudMostFavouriteStation  = 0;
unsigned    CNetworkTelemetry::sm_AudLeastFavouriteStation = 0;
unsigned    CNetworkTelemetry::sm_AudPrevTunedStation      = ~0u;
unsigned    CNetworkTelemetry::sm_AudPlayerTunedStation    = ~0u;
unsigned    CNetworkTelemetry::sm_AudPlayerPrevTunedStation = ~0u;
static const float AUD_START_TIME_INVALID                 = -1.f;
float       CNetworkTelemetry::sm_AudTrackStartTime        = AUD_START_TIME_INVALID;
unsigned    CNetworkTelemetry::sm_AudPrevTunedUsbMix	   = ~0u;

u64         CNetworkTelemetry::m_PlayerSpawnTime   = 0;
Vector3     CNetworkTelemetry::m_PlayerSpawnCoords(VEC3_ZERO);
bool        CNetworkTelemetry::m_SendMetricInjury  = true;

//Single Player mission control
u32 CNetworkTelemetry::sm_MissionStartedTime = 0;
u32 CNetworkTelemetry::sm_MissionStartedTimePaused = 0;
u32 CNetworkTelemetry::sm_MissionCheckpointTime = 0;
u32 CNetworkTelemetry::sm_MissionCheckpointTimePaused = 0;

OUTPUT_ONLY( u32 CNetworkTelemetry::sm_MissionCheckpoint = 0; )

atHashString  CNetworkTelemetry::sm_MissionStarted;
atHashString  CNetworkTelemetry::sm_MatchStartedId;
u32 CNetworkTelemetry::sm_RootContentId = 0;

OUTPUT_ONLY( u32 CNetworkTelemetry::sm_CompanyWidePlaytestId = 0; )

u8 CNetworkTelemetry::sm_OnlineJoinType = 0;

static rlTelemetry::Delegate s_OnInternalDelegate(&CNetworkTelemetry::OnTelemetyEvent);
static u32 s_cloudCashTransactionCount = 0;
static u64 s_telemetryFlushFailedCounter = 0;
static bool s_telemetryFlushFailedPlayerJoined = false;
static const u32 TRIGGER_MINORITY_REPORT = 5;

MetricPURCHASE_FLOW::PurchaseMetricInfos CNetworkTelemetry::sm_purchaseMetricInformation;

MetricComms::CumulativeTextCommsInfo CNetworkTelemetry::sm_CumulativeTextCommsInfo[MetricComms::NUM_CUMULATIVE_CHAT_TYPES];
MetricComms::CumulativeVoiceCommsInfo CNetworkTelemetry::sm_CumulativeVoiceCommsInfo;

u64 CNetworkTelemetry::m_benchmarkUid=0;

//High volume metric 
static const u32 HIGH_VOLUME_METRIC_FLUSH_THESHOLD_BYTES = 1024*1024;
static const u32 HIGH_VOLUME_METRIC_TIMEOUT = 10*1000;
static rlMetricList  s_highVolumeMetricList;
static u32           s_highVolumeMetricListSize = 0;
static netTimeout    s_highVolumeMetricListTimeout;

u64		CNetworkTelemetry::sm_playtimeSessionId = 0;

bool CNetworkTelemetry::sm_AllowRelayUsageTelemetry = true;
bool CNetworkTelemetry::sm_AllowPCHardwareTelemetry = true;

int CNetworkTelemetry::ms_currentSpawnLocationOption = -1;

bool CNetworkTelemetry::sm_LastVehicleDrivenInTestDrive = false;
int CNetworkTelemetry::sm_VehicleDrivenLocation = 0;
char CNetworkTelemetry::ms_publicContentId[MetricPlayStat::MAX_STRING_LENGTH];
ChatOption CNetworkTelemetry::sm_ChatOption = ChatOption::Everyone;

int CNetworkTelemetry::sm_ChatIdExpiryTime = 1000u * 60u * 5u;
u32 CNetworkTelemetry::sm_CurrentPhoneChatId = 0;
u32 CNetworkTelemetry::sm_TimeOfLastPhoneChat = 0;
u32 CNetworkTelemetry::sm_CurrentEmailChatId = 0;
u32 CNetworkTelemetry::sm_TimeOfLastEmailChat = 0;

#if RSG_PC
bool CNetworkTelemetry::sm_bPostHardwareStats = false;
#endif

#if !__NO_OUTPUT
static const char* s_TelemetryChannelsNames[MAX_TELEMETRY_CHANNEL] =
								{"AMBIENT"
								,"FLOW"
								,"JOB"
								,"MEDIA"
								,"MISC"
								,"MISSION"
								,"MONEY"
								,"POSSESSIONS"
								,"DEV_DEBUG"
								,"DEV_CAPTURE"
								,"RAGE"};

const char* GetTelemetryChannelName(const u32 channel)
{
	if (channel < MAX_TELEMETRY_CHANNEL)
		return s_TelemetryChannelsNames[channel];

	return "UNKNOWN";
}


static const char* s_TelemetryLogLevelsNames[LOGLEVEL_DEBUG_NEVER] = 
								{"LOGLEVEL_VERYHIGH_PRIORITY"
								,"LOGLEVEL_HIGH_PRIORITY"
								,"LOGLEVEL_MEDIUM_PRIORITY"
								,"LOGLEVEL_LOW_PRIORITY"
								,"LOGLEVEL_VERYLOW_PRIORITY"
								,"LOGLEVEL_HIGH_VOLUME"
								,"LOGLEVEL_DEBUG1"
								,"LOGLEVEL_DEBUG2"
								,"LOGLEVEL_NONE"
								,"LOGLEVEL_NONE"
								,"LOGLEVEL_MAX"};

const char* GetTelemetryLogLevelName(const u32 loglevel)
{
	if (loglevel < LOGLEVEL_DEBUG_NEVER)
		return s_TelemetryLogLevelsNames[loglevel];

	return "UNKNOWN";
}

#endif //!__NO_OUTPUT

bool WriteSku(RsonWriter* rw)
{
	// not sure if this is still used for anything, but removed all hardcoded ps3-only sku output
	atStringBuilder str;
#if RSG_PC
	str.Append("P");
#elif RSG_DURANGO
	str.Append("D");
#elif RSG_ORBIS
	str.Append("O");
#elif RSG_PROSPERO
	str.Append("P");
#elif RSG_SCARLETT
	str.Append("S");
#endif
// General build
#if __MASTER
	str.Append("-M");
#elif __BANK
	str.Append("-B");
#elif __DEV
	str.Append("-D");
#elif __FINAL
#if __FINAL_LOGGING
	str.Append("-FWL");
#else
	str.Append("-F");
#endif
#else
	// This is 'Release' mode, weirdly.
	str.Append("-R");
#endif

// Is it a steam build?
#if __STEAM_BUILD
	str.Append("-S");
#endif

	rlGameRegion gameRegion = RL_GAME_REGION_INVALID;

#if RSG_DURANGO  // On Durango, we're using the sysAppContent (I didnt want to change rlXbl::GetGameRegion() )
	if (sysAppContent::IsEuropeanBuild())
		gameRegion = RL_GAME_REGION_EUROPE;
	if (sysAppContent::IsAmericanBuild())
		gameRegion = RL_GAME_REGION_AMERICA;
	if (sysAppContent::IsJapaneseBuild())
		gameRegion = RL_GAME_REGION_JAPAN;
#else
	gameRegion = rlGetGameRegion();
#endif // RSG_DURANGO

	switch (gameRegion)
	{
	case RL_GAME_REGION_EUROPE:
		str.Append("-EU");
		break;
	case RL_GAME_REGION_AMERICA:
		str.Append("-US");
		break;
	case RL_GAME_REGION_JAPAN:
		str.Append("-JP");
		break;
	default:
		break;
	}

// Write it out
	return rw->WriteString("sku", str.ToString());
}

//////// Basic playstats ////////


class MetricPlayStatString : public MetricPlayStat
{
	RL_DECLARE_METRIC(PLAYSSTRING, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:

	explicit MetricPlayStatString(const char* name)
	{
		safecpy(m_Name, name, COUNTOF(m_Name));
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteString("name", m_Name);
	}

private:

	char m_Name[MetricPlayStat::MAX_STRING_LENGTH];
};


class MetricLOADGAME : public MetricPlayStat
{
	RL_DECLARE_METRIC(LOADGAME, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteBool("dlc", CFileMgr::IsDownloadableVersion()) 
			&& WriteSku( rw );
	}
};
class MetricNEWGAME : public MetricPlayStat
{
    RL_DECLARE_METRIC(NEWGAME, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteBool("dlc", CFileMgr::IsDownloadableVersion()) 
			&& WriteSku( rw );
	}
};

class MetricSTART_GAME : public MetricPlayStat
{
	RL_DECLARE_METRIC(START_GAME, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_HIGH_PRIORITY);

public:
	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteBool("dlc", CFileMgr::IsDownloadableVersion()) 
			&& WriteSku( rw );
	}
};
class MetricEXIT_GAME : public MetricPlayStat
{
    RL_DECLARE_METRIC(EXIT_GAME, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteBool("dlc", CFileMgr::IsDownloadableVersion()) 
			&& WriteSku( rw );
	}
};

//////// Basic betting metric ////////
class MetricROS_BET : public MetricPlayStat
{
	RL_DECLARE_METRIC(ROS_BET, TELEMETRY_CHANNEL_JOB, LOGLEVEL_MEDIUM_PRIORITY);

public:
	explicit MetricROS_BET(const CPed* gamer, const CPed* betOngamer, const u32 amount, const u32 activity, const float commission = 0.0f)
		: m_Amount(amount), m_Activity(activity), m_commission(commission)
	{
		m_Gamer[0] = '\0';
		m_BetOnGamer[0] = '\0';

		if(gnetVerify(gamer) && gnetVerify(gamer->IsPlayer()) && gnetVerify(gamer->GetPlayerInfo()))
		{
			gamer->GetPlayerInfo()->m_GamerInfo.GetGamerHandle().ToString(m_Gamer, COUNTOF(m_Gamer));
		}

		if(gnetVerify(betOngamer) && gnetVerify(betOngamer->IsPlayer()) && gnetVerify(betOngamer->GetPlayerInfo()))
		{
			betOngamer->GetPlayerInfo()->m_GamerInfo.GetGamerHandle().ToString(m_BetOnGamer, COUNTOF(m_BetOnGamer));
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);

		if (m_Gamer[0] != '\0')
		{
			result = result && rw->WriteString("g", m_Gamer);
		}

		if (m_BetOnGamer[0] != '\0')
		{
			result = result && rw->WriteString("bg", m_BetOnGamer);
		}

		if (m_Amount > 0)
		{
			result = result && rw->WriteUns("amt", m_Amount);
		}

		if (m_Activity > 0)
		{
			result = result && rw->WriteUns("act", m_Activity);
		}

		if (m_commission > 0.0f)
		{
			result = result && rw->WriteFloat("cm", m_commission);
		}

		return result;
	}

private:
	char m_Gamer[RL_MAX_GAMER_HANDLE_CHARS];
	char m_BetOnGamer[RL_MAX_GAMER_HANDLE_CHARS];
	u32  m_Amount;
	u32  m_Activity;
	float  m_commission;
};


//////// Basic playstats with player coords ////////
class MetricDEATH : public MetricPlayStat
{
	RL_DECLARE_METRIC(DEATH, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

private:
	enum eInfoFlags
	{
		FLAG_IS_PLAYER      = BIT(0)
		,FLAG_IS_LOCALPLAYER = BIT(1)
		,FLAG_IN_VEHICLE     = BIT(2)
		,FLAG_RUNNING        = BIT(3)
		,FLAG_SPRINTING      = BIT(4)
		,FLAG_CROUCHING      = BIT(5)
	};

private:
	char    m_Victim[RL_MAX_GAMER_HANDLE_CHARS];
	char    m_Killer[RL_MAX_GAMER_HANDLE_CHARS];
	s32     m_VictimAccount;
	s32		m_VictimRank;
	s32     m_KillerAccount;
	s32		m_KillerRank;
	u32     m_KillerWeaponHash;
	u32     m_VictimInformation;
	u32     m_KillerInformation;
	u32     m_VehicleId;
	int     m_WeaponDamageComponent;
	Vector3 m_VictimPos;
	Vector3 m_KillerPos;
	u32     m_VictimWeaponHash;
	bool    m_killerInABike;
	bool    m_victimInABike;
	bool    m_withMeleeWeapon;
	u32     m_groupId;
	bool	m_pvpDeath;
	bool	m_isKiller;

public:

	explicit MetricDEATH(const CPed* victim, const CPed* killer, const u32 weaponHash, const int weaponDamageComponent, const bool withMeleeWeapon, bool pvpDeath, bool isKiller)
		: m_KillerWeaponHash(weaponHash)
		, m_WeaponDamageComponent(weaponDamageComponent)
		, m_VehicleId(0)
		, m_withMeleeWeapon(withMeleeWeapon)
		, m_pvpDeath(pvpDeath)
		, m_isKiller(isKiller)
		, m_VictimAccount(0)
		, m_VictimRank(0)
		, m_KillerAccount(0)
		, m_KillerRank(0)
	{
		m_victimInABike = false;
		m_VictimPos.Zero();
		m_VictimInformation = 0;
		m_Victim[0] = '\0';
		if(victim)
		{
			m_VictimPos = VEC3V_TO_VECTOR3(victim->GetTransform().GetPosition());
			if (victim->GetEquippedWeaponInfo())
			{
				m_VictimWeaponHash = victim->GetEquippedWeaponInfo()->GetHash();
			}

			if (victim->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				m_VictimInformation |=  FLAG_IN_VEHICLE;

				CVehicle* vehicle = victim->GetMyVehicle();
				if (vehicle)
				{
					if (vehicle->GetBaseModelInfo())
					{
						m_VehicleId = vehicle->GetBaseModelInfo()->GetHashKey();
					}

					//Check if the victim is in a motorbike.
					m_victimInABike = vehicle->InheritsFromBike();
				}
			}
			if(victim->GetIsCrouching())
			{
				m_VictimInformation |=  FLAG_CROUCHING;
			}
			if(victim->GetMotionData()->GetIsSprinting())
			{
				m_VictimInformation |=  FLAG_SPRINTING;
			}
			if(victim->GetMotionData()->GetIsRunning())
			{
				m_VictimInformation |=  FLAG_RUNNING;
			}

			if(victim->IsPlayer())
			{
				victim->GetPlayerInfo()->m_GamerInfo.GetGamerHandle().ToString(m_Victim, COUNTOF(m_Victim));

				m_VictimInformation |=  FLAG_IS_PLAYER;
				if(victim->IsLocalPlayer())
				{
					m_VictimInformation |=  FLAG_IS_LOCALPLAYER;
				}

				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(victim->GetPlayerInfo()->m_GamerInfo.GetGamerHandle());
				if (pPlayer)
				{
					m_VictimAccount = pPlayer->GetPlayerAccountId();
					m_VictimRank = pPlayer->GetCharacterRank();
				}
			}
			else
			{
				//No need for vehicle info if the victim is not a player.
				m_VehicleId = 0;

				const ePedType pedType = victim->GetPedType();
				const char* victimName = CPedType::GetPedTypeNameFromId(pedType);

				m_Victim[0] = '*';
				m_Victim[1] = '\0';

				safecat(m_Victim, victimName, COUNTOF(m_Victim));
			}
		}

		m_killerInABike = false;
		m_KillerPos.Zero();
		m_KillerInformation = 0;
		m_Killer[0] = '\0';
		if(killer)
		{
			m_KillerPos = VEC3V_TO_VECTOR3(killer->GetTransform().GetPosition());

			if (killer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				m_KillerInformation |=  FLAG_IN_VEHICLE;

				//Check if the inflictor is in a motorbike.
				CVehicle* vehicle = killer->GetMyVehicle();
				if (vehicle)
				{
					if (vehicle->GetBaseModelInfo())
					{
						m_VehicleId = vehicle->GetBaseModelInfo()->GetHashKey();
					}
					m_killerInABike = vehicle->InheritsFromBike();
				}
			}
			if(killer->GetIsCrouching())
			{
				m_KillerInformation |=  FLAG_CROUCHING;
			}
			if(killer->GetMotionData()->GetIsSprinting())
			{
				m_KillerInformation |=  FLAG_SPRINTING;
			}
			if(killer->GetMotionData()->GetIsRunning())
			{
				m_KillerInformation |=  FLAG_RUNNING;
			}

			if(killer->IsPlayer())
			{
				killer->GetPlayerInfo()->m_GamerInfo.GetGamerHandle().ToString(m_Killer, COUNTOF(m_Killer));

				m_KillerInformation |=  FLAG_IS_PLAYER;
				if(killer->IsLocalPlayer())
				{
					m_KillerInformation |=  FLAG_IS_LOCALPLAYER;
				}

				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(killer->GetPlayerInfo()->m_GamerInfo.GetGamerHandle());
				if (pPlayer)
				{
					m_KillerAccount = pPlayer->GetPlayerAccountId();
					m_KillerRank = pPlayer->GetCharacterRank();
				}
			}
			else
			{
				const ePedType pedType = killer->GetPedType();
				const char* killerName = CPedType::GetPedTypeNameFromId(pedType);

				m_Killer[0] = '*';
				m_Killer[1] = '\0';

				safecat(m_Killer, killerName, COUNTOF(m_Killer));
			}
		}
		m_groupId = m_KillerAccount | m_VictimAccount;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPlayStat::Write(rw);

		result = result && rw->BeginArray("c", NULL);
		{
			result = result && rw->WriteFloat(NULL, m_VictimPos.x, "%.2f");
			result = result && rw->WriteFloat(NULL, m_VictimPos.y, "%.2f");
			result = result && rw->WriteFloat(NULL, m_VictimPos.z, "%.2f");
		}
		result = result && rw->End();

		if (m_Victim[0] != '\0')
		{
			result = result && rw->WriteString("v", m_Victim);
			result = result && rw->WriteInt("va", m_VictimAccount);
			result = result && rw->WriteInt("vr", m_VictimRank);

			if (0 < m_VictimInformation)
			{
				result = result && rw->WriteUns("vinf", m_VictimInformation);
			}
		}

		if (0 < m_groupId)
		{
			result = result && rw->WriteUns("groupId", m_groupId);
		}

		if (0 < m_KillerWeaponHash)
		{
			result = result && rw->WriteUns("kw", m_KillerWeaponHash);
		}

		if (-1 < m_WeaponDamageComponent)
		{
			result = result && rw->WriteUns("wc", (u32)m_WeaponDamageComponent);
		}

		if (0 < m_VictimWeaponHash)
		{
			result = result && rw->WriteUns("vw", m_VictimWeaponHash);
		}

		if (m_Killer[0] != '\0')
		{
			result = result && rw->WriteString("k", m_Killer);
			result = result && rw->WriteInt("ka", m_KillerAccount);
			result = result && rw->WriteInt("kr", m_KillerRank);

			if (0 < m_KillerInformation)
			{
				result = result && rw->WriteUns("ki", m_KillerInformation);
			}

			if (!m_KillerPos.IsZero())
			{
				result = result && rw->BeginArray("kc", NULL);
				{
					result = result && rw->WriteFloat(NULL, m_KillerPos.x, "%.2f");
					result = result && rw->WriteFloat(NULL, m_KillerPos.y, "%.2f");
					result = result && rw->WriteFloat(NULL, m_KillerPos.z, "%.2f");
				}
				result = result && rw->End();
			}
		}

		if (0 < m_VehicleId)
		{
			result = result && rw->WriteUns("vi", m_VehicleId);
		}

		result = result && rw->WriteBool("kib", m_killerInABike);
		result = result && rw->WriteBool("vib", m_victimInABike);
		result = result && rw->WriteBool("wmw", m_withMeleeWeapon);
		result = result && rw->WriteBool("pvp", m_pvpDeath);
		result = result && rw->WriteBool("ik", m_isKiller);

		return result;
	}
};

class MetricPLAYER_INJURED : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(INJURED, TELEMETRY_CHANNEL_JOB, LOGLEVEL_LOW_PRIORITY);

public:

	explicit MetricPLAYER_INJURED(const u64 spawnTime, const Vector3& spawnCoords) 
		: m_SpawnTime(spawnTime)
		, m_X(0.0f)
		, m_Y(0.0f)
		, m_Z(0.0f)
	{
		m_X = spawnCoords.x;
		m_Y = spawnCoords.y;
		m_Z = spawnCoords.z;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayerCoords::Write(rw);

		result = result && rw->WriteInt64("st", static_cast<s64>(m_SpawnTime));

		result = result && rw->BeginArray("sc", NULL);
		{
			result = result && rw->WriteFloat(NULL, m_X, "%.2f");
			result = result && rw->WriteFloat(NULL, m_Y, "%.2f");
			result = result && rw->WriteFloat(NULL, m_Z, "%.2f");
		}
		result = result && rw->End();

		return result;
	}

private:
	u64    m_SpawnTime;
	float  m_X;
	float  m_Y;
	float  m_Z;
};

class MetricPOST_RACE_CHECKPOINT : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(R_C, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

private:
	u32 m_VehicleId;
	u32 m_CheckpointId;
	u32 m_Rank;
	u32 m_RaceTime;
	u32 m_CheckpointTime;

public:
	explicit MetricPOST_RACE_CHECKPOINT(const u32 vehicleId
										,const u32 checkpointId
										,const u32 racePos
										,const u32 raceTime
										,const u32 checkpointTime)
		: m_VehicleId(vehicleId)
		, m_CheckpointId(checkpointId)
		, m_Rank(racePos)
		, m_RaceTime(raceTime)
		, m_CheckpointTime(checkpointTime)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayerCoords::Write(rw);

		if (0 < m_VehicleId)
		{
			result = result && rw->WriteUns("vid", m_VehicleId);
		}

		if (0 < m_CheckpointId)
		{
			result = result && rw->WriteUns("c", m_CheckpointId);
		}

		if (0 < m_Rank)
		{
			result = result && rw->WriteUns("r", m_Rank);
		}

		if (0 < m_RaceTime)
		{
			result = result && rw->WriteUns("tt", m_RaceTime);
		}

		if (0 < m_CheckpointTime)
		{
			result = result && rw->WriteUns("ct", m_CheckpointTime);
		}

		return result;
	}
};

class MetricPOST_MATCH_BLOB : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(PMB, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

private:
	u32 m_XP;                   //Total XP earned in the match
	int m_Cash;                 //Total cash earned/lost in the match
	u32 m_HighestKillStreak;    //Highest kill streak achieved in the match
	u32 m_Kills;                //Total enemy kills in the match (should not include suicides or teamkills)
	u32 m_Deaths;               //Total player deaths in the match
	u32 m_Suicides;             //Total player suicides in the match
	u32 m_Rank;                 //The scoreboard ranking of the player at the end of the match (1st to Nth)
	int m_TeamId;               //Team the player was on at the end of the match
	int m_CrewId;               //Active crew the player had selected during the match, if applicable
	u32 m_VehicleId;            //Current vehicle ID (Model Key Hash) at the end of the match (probably only applicable for race modes)
	int m_BetCash;              //Cash earned from bets
	int m_CashStart;              //Cash earned from bets
	int m_CashEnd;              //Cash earned from bets
	int m_betWon;
	int m_survivedWave;

public:
	explicit MetricPOST_MATCH_BLOB(const u32 xp
									,const int cash
									,const u32 highestKillStreak
									,const u32 kills
									,const u32 deaths
									,const u32 suicides
									,const u32 rank
									,const int teamId
									,const int crewId
									,const u32 vehicleId
									,const int cashEarnedFromBets
									,const int cashStart
									,const int cashEnd
									,const int betWon
									,const int survivedWave)
		: m_XP(xp)
		, m_Cash(cash)
		, m_HighestKillStreak(highestKillStreak)
		, m_Kills(kills)
		, m_Deaths(deaths)
		, m_Suicides(suicides)
		, m_Rank(rank)
		, m_TeamId(teamId)
		, m_CrewId(crewId)
		, m_VehicleId(vehicleId)
		, m_BetCash(cashEarnedFromBets)
		, m_CashStart(cashStart)
		, m_CashEnd(cashEnd)
		, m_betWon(betWon)
		, m_survivedWave(survivedWave)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayerCoords::Write(rw);

		if (0 < m_XP)
		{
			result = result && rw->WriteUns("xp", m_XP);
		}

		if (0 != m_Cash)
		{
			result = result && rw->WriteInt("c", m_Cash);
		}

		if (0 < m_HighestKillStreak)
		{
			result = result && rw->WriteUns("hks", m_HighestKillStreak);
		}

		if (0 < m_Kills)
		{
			result = result && rw->WriteUns("k", m_Kills);
		}

		if (0 < m_Deaths)
		{
			result = result && rw->WriteUns("d", m_Deaths);
		}

		if (0 < m_Suicides)
		{
			result = result && rw->WriteUns("s", m_Suicides);
		}

		if (0 < m_Rank)
		{
			result = result && rw->WriteUns("r", m_Rank);
		}

		if (-1 < m_TeamId)
		{
			result = result && rw->WriteUns("tid", (u32)m_TeamId);
		}

		if (0 < m_CrewId)
		{
			result = result && rw->WriteUns("cid", (u32)m_CrewId);
		}

		if (0 < m_VehicleId)
		{
			result = result && rw->WriteUns("vid", m_VehicleId);
		}

		if (0 != m_BetCash)
		{
			result = result && rw->WriteInt("bc", m_BetCash);
		}

		if (0 != m_CashStart)
		{
			result = result && rw->WriteInt("csta", m_CashStart);
		}

		if (0 != m_CashEnd)
		{
			result = result && rw->WriteInt("cend", m_CashEnd);
		}

		if (0 < m_betWon)
		{
			result = result && rw->WriteInt("bet", m_betWon);
		}

		if (0 < m_survivedWave)
		{
			result = result && rw->WriteInt("wave", m_survivedWave);
		}

		result = result && rw->WriteUns("pt", (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash()));

		return result;
	}
};

class MetricARREST : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(ARREST, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);
};

class MetricSPAWN : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(SPAWN, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYLOW_PRIORITY);

private:
	int m_locOption; // Which spawn location option is currently set in the menu: specific property, random, last location
	int m_spawnLocation;
	int m_reason;

public:
	explicit MetricSPAWN(const int locOption, const int spawnLocation, const int reason)
		: m_locOption(locOption)
		, m_spawnLocation(spawnLocation)
		, m_reason(reason)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw)
			&& rw->WriteInt("d", m_locOption)
			&& rw->WriteInt("loc", m_spawnLocation)  // value from script enum PLAYER_SPAWN_LOCATION
			&& rw->WriteInt("res", m_reason);   // value from script enum SPAWN_REASON
	}
};

class MetricACQUIRED_HIDDEN_PACKAGE : public MetricPlayStat
{
	RL_DECLARE_METRIC(HIDDEN_PKG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

private:
	int m_id;
	float m_X;
	float m_Y;
	float m_Z;

public:
	explicit MetricACQUIRED_HIDDEN_PACKAGE(const int id) : m_id(id)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteInt("id", m_id);
	}
};

class MetricWEBSITE_VISITED : public MetricPlayStat
{
	RL_DECLARE_METRIC(WEB, TELEMETRY_CHANNEL_MEDIA, LOGLEVEL_HIGH_PRIORITY);

private:
	int m_id;
	int m_time;

public:
	explicit MetricWEBSITE_VISITED(const int id, const int timespent) : m_id(id), m_time(timespent)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw) 
			&& rw->WriteUns("id", (u32)m_id) 
			&& rw->WriteUns("time", (u32)m_time);
	}
};

class MetricFRIEND_ACTIVITY_DONE : public MetricPlayStat
{
	RL_DECLARE_METRIC(FRIEND_ACT, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_MEDIUM_PRIORITY);

private:
	int m_char;
	int m_outcome;

public:
	explicit MetricFRIEND_ACTIVITY_DONE(int character, int outcome) : m_char(character), m_outcome(outcome)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw) 
			&& rw->WriteInt("char", m_char)  // Characters that went in the friend activity.
			&& rw->WriteInt("res", m_outcome); // 1 -> successfully, 0 -> Failed, 2 -> aborted
	}
};

class MetricWANTED_LEVEL : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(W_L, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_MEDIUM_PRIORITY);

private:

	u32 m_WantedLevel;

public:

	explicit MetricWANTED_LEVEL()
	{
		const CPlayerInfo* pinfo = CGameWorld::GetMainPlayerInfo();
		m_WantedLevel = pinfo ? (u8) pinfo->GetPlayerPed()->GetPlayerWanted()->GetWantedLevel() : 0;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw) && rw->WriteUns("wantedlvl", m_WantedLevel);
	}
};

class MetricCLAN_FEUD : public MetricPlayStat
{
	RL_DECLARE_METRIC(CLAN_FEUD, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCLAN_FEUD(u32 winningClan, u32 losingClan, u32 winningKills, u32 losingKills)
		: MetricPlayStat()
		, m_winner(winningClan)
		, m_winnerKills(winningKills)
		, m_loser(losingClan)
		, m_loserKills(losingKills)
	{

	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteUns("w", m_winner)
			&& rw->WriteUns("l", m_loser)
			&& rw->WriteUns("wk", m_winnerKills)
			&& rw->WriteUns("lk", m_loserKills)
			;
	}
protected:
	u32 m_winner, m_loser, m_winnerKills, m_loserKills;
};

//////// Playstats with weapons ////////

class MetricACQUIRED_WEAPON : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(W_A, TELEMETRY_CHANNEL_POSSESSIONS, LOGLEVEL_VERYLOW_PRIORITY);

public:

    explicit MetricACQUIRED_WEAPON(const unsigned weaponHash)
        : m_WeaponHash(weaponHash)
    {
    }

    virtual bool Write(RsonWriter* rw) const
    {
        return this->MetricPlayerCoords::Write(rw)
                && rw->WriteUns("w", m_WeaponHash);
    }

private:

    unsigned m_WeaponHash;
};

class MetricWEAPON_MOD_CHANGE : public MetricPlayStat
{
	RL_DECLARE_METRIC(W_MOD, TELEMETRY_CHANNEL_POSSESSIONS, LOGLEVEL_LOW_PRIORITY);

public:

	explicit MetricWEAPON_MOD_CHANGE(const u32 weaponHash, const int modIdFrom, const int modIdTo)
		: m_WeaponHash(weaponHash), m_modIdFrom(modIdFrom), m_modIdTo(modIdTo)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPlayStat::Write(rw); 

		result = result && rw->WriteUns("name", m_WeaponHash);

		if (m_modIdFrom != 0)
		{
			result = result && rw->WriteUns("from", (u32)m_modIdFrom);
		}

		if (m_modIdTo != 0)
		{
			result = result && rw->WriteUns("to", (u32)m_modIdTo);
		}

		return result;
	}

private:
	u32 m_WeaponHash;
	int m_modIdFrom;
	int m_modIdTo;
};

class MetricPROP_CHANGE : public MetricPlayStat
{
	RL_DECLARE_METRIC(PROP, TELEMETRY_CHANNEL_POSSESSIONS, LOGLEVEL_MEDIUM_PRIORITY);

public:

	explicit MetricPROP_CHANGE(const u32 modelHash, const int  position, const int  newPropIndex, const int  newTextIndex) 
		: m_pedHash(modelHash), m_position(position), m_newPropIndex(newPropIndex), m_newTextIndex(newTextIndex) {}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw)
			&& rw->WriteUns("ped", (u32)m_pedHash)
			&& rw->WriteInt("pos", (u32)m_position)
			&& rw->WriteInt("pid", (u32)m_newPropIndex)
			&& rw->WriteInt("tid", (u32)m_newTextIndex);
	}

private:
	u32  m_pedHash;
	int  m_position;
	int  m_newPropIndex;
	int  m_newTextIndex;
};

class MetricCLOTH_CHANGE : public MetricPlayStat
{
	RL_DECLARE_METRIC(CLOTH, TELEMETRY_CHANNEL_POSSESSIONS, LOGLEVEL_MEDIUM_PRIORITY);

public:

	explicit MetricCLOTH_CHANGE(const u32 modelHash, const int componentID, const int drawableID, const int textureID, const int paletteID) 
		: m_pedHash(modelHash), m_componentID(componentID), m_drawableID(drawableID), m_textureID(textureID), m_paletteID(paletteID) {}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw)
			&& rw->WriteUns("ped", m_pedHash)
			&& rw->WriteInt("cid", m_componentID)
			&& rw->WriteInt("did", m_drawableID)
			&& rw->WriteInt("tid", m_textureID)
			&& rw->WriteInt("pid", m_paletteID);
	}

private:
	u32 m_pedHash;
	int m_componentID;
	int m_drawableID;
	int m_textureID;
	int m_paletteID;
};

//////// Vehicle playstats ////////

class MetricVehicle : public MetricPlayerCoords
{
public:

    explicit MetricVehicle(const u32 modelIndex)
    {
		m_VehicleHash = ~0u;

        if(modelIndex != fwModelId::MI_INVALID)
        {
			CBaseModelInfo* mi = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
			if (mi)
				m_VehicleHash = mi->GetHashKey();
        }
    }

    virtual bool Write(RsonWriter* rw) const
    {
        return this->MetricPlayerCoords::Write(rw)
                && rw->WriteUns("name", m_VehicleHash);
    }

private:

    unsigned m_VehicleHash;
};

class MetricVEHICLE_DIST_DRIVEN : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(VEH_DR, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYLOW_PRIORITY);

public:
	static const unsigned LOCATION_MAX_STRING_SIZE = 64;

	explicit MetricVEHICLE_DIST_DRIVEN(const u32 keyHash, const float dist, const u32 timeinvehicle, const char* areaName, const char* districtName, const char* streetName, const bool owned, const bool testDrive, const u32 location) 
		: m_dist(dist) 
		, m_keyHash(keyHash)
		, m_timeinvehicle(timeinvehicle)
		, m_owned(owned)
		, m_testDrive(testDrive)
		, m_location(location)
	{
		strcpy(m_areaName, areaName);
		strcpy(m_districtName, districtName);
		strcpy(m_streetName, streetName);
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw)
			&& rw->WriteUns("name", m_keyHash)
			&& rw->WriteFloat("dist", m_dist)
			&& rw->WriteUns("time", m_timeinvehicle)
			&& rw->WriteUns("loc", m_location)
			&& rw->WriteBool("own", m_owned)
			&& rw->WriteString("an", m_areaName)
			&& rw->WriteString("dn", m_districtName)
			&& rw->WriteString("sn", m_streetName)
			&& rw->WriteBool("td", m_testDrive);
	}

private:
	char	m_areaName[LOCATION_MAX_STRING_SIZE];
	char	m_districtName[LOCATION_MAX_STRING_SIZE];
	char	m_streetName[LOCATION_MAX_STRING_SIZE];
	float	m_dist;
	u32		m_keyHash;
	u32		m_timeinvehicle;
	bool	m_owned;
	bool	m_testDrive;
	u32		m_location;
};

class MetricEMERGENCY_SVCS : public MetricVehicle
{
	RL_DECLARE_METRIC(ESVCS, TELEMETRY_CHANNEL_MISC, LOGLEVEL_LOW_PRIORITY);

public:

	MetricEMERGENCY_SVCS(const int modelIndex,
		const Vector3& destination) : MetricVehicle(modelIndex)
	{
		m_X = (int) destination.x;
		m_Y = (int) destination.y;

		const CPlayerInfo* pinfo = CGameWorld::GetMainPlayerInfo();
		m_WantedLevel = pinfo ? pinfo->GetPlayerPed()->GetPlayerWanted()->GetWantedLevel() : 0;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricVehicle::Write(rw);

		result = result && rw->BeginArray("cd", NULL);
		{
			result = result && rw->WriteInt(NULL, m_X);
			result = result && rw->WriteInt(NULL, m_Y);
		}
		result = result && rw->End();

		return result && rw->WriteInt("wlvl", m_WantedLevel);
	}

private:
	int m_X;
	int m_Y;
	int m_WantedLevel;
};





class MetricAMBIENT_MISSION_HOLD_UP : public MetricBaseMultiplayerMissionDone, public MetricPlayerCoords
{
	RL_DECLARE_METRIC(HOLD_UP, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricAMBIENT_MISSION_HOLD_UP(const u32  missionId, const u32 xpEarned, const u32 cashEarned, const u32 shopkeepersKilled) 
		: MetricBaseMultiplayerMissionDone(missionId, xpEarned, cashEarned, 0)
		, m_ShopkeepersKilled(shopkeepersKilled)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricBaseMultiplayerMissionDone::Write(rw);

		result = result && this->MetricPlayerCoords::Write(rw);

		if (m_ShopkeepersKilled > 0)
		{
			result = result && rw->WriteUns("s", m_ShopkeepersKilled);
		}

		return result;
	}

private:
	u32  m_ShopkeepersKilled;
};

class MetricAMBIENT_MISSION_IMP_EXP : public MetricBaseMultiplayerMissionDone, public MetricVehicle
{
	RL_DECLARE_METRIC(IMP_EXP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricAMBIENT_MISSION_IMP_EXP(const u32  missionId, const u32 xpEarned, const u32 cashEarned, const int modelIndex) 
		: MetricBaseMultiplayerMissionDone(missionId, xpEarned, cashEarned, 0)
		, MetricVehicle(modelIndex)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricBaseMultiplayerMissionDone::Write(rw)
			&& this->MetricVehicle::Write(rw);
	}
};

class MetricAMBIENT_MISSION_SECURITY_VAN : public MetricBaseMultiplayerMissionDone, public MetricPlayerCoords
{
	RL_DECLARE_METRIC(SEC_VAN, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricAMBIENT_MISSION_SECURITY_VAN(const u32  missionId, const u32 xpEarned, const u32 cashEarned) 
		: MetricBaseMultiplayerMissionDone(missionId, xpEarned, cashEarned, 0)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricBaseMultiplayerMissionDone::Write(rw)
			&& this->MetricPlayerCoords::Write(rw);
	}
};

class MetricAMBIENT_PROSTITUTES : public MetricBaseMultiplayerMissionDone, public rlMetric
{
	RL_DECLARE_METRIC(PROSY, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricAMBIENT_PROSTITUTES(const u32  missionId, const u32 xpEarned, const u32 cashEarned, const u32 cashSpent, const u32 numberOfServices, const bool playerWasTheProstitute) 
		: MetricBaseMultiplayerMissionDone(missionId, xpEarned, cashEarned, cashSpent)
		, m_numberOfServices(numberOfServices)
		, m_playerWasTheProstitute(playerWasTheProstitute)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricBaseMultiplayerMissionDone::Write(rw);

		if (m_playerWasTheProstitute)
		{
			result = result && rw->WriteBool("p", m_playerWasTheProstitute);
		}

		if (m_numberOfServices > 0)
		{
			result = result && rw->WriteUns("n", m_numberOfServices);
		}

		return result;
	}

private:
	u32  m_numberOfServices;
	bool m_playerWasTheProstitute;
};

//////// Playstats with only a string value ////////

#if RSG_OUTPUT
class MetricMISSION_STARTED : public MetricPlayStatString
{
	RL_DECLARE_METRIC(MIS_S, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_DEBUG1);

public:

	explicit MetricMISSION_STARTED(const char* missionName, const u32 checkpoint) : MetricPlayStatString(missionName), m_Checkpoint(checkpoint) {;}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStatString::Write(rw);

		if (m_Checkpoint > 0)
			result = result && rw->WriteUns("chk", m_Checkpoint);

		return result;
	}

private:
	u32 m_Checkpoint;
};
#endif // RSG_OUTPUT

class MetricMISSION_OVER : public MetricPlayStatString
{
	RL_DECLARE_METRIC(MIS_O, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	explicit MetricMISSION_OVER(const char* missionName, const u32 variant, const u32 checkpoint, const bool failed, const bool canceled, const bool skipped, const u32 timespent, const bool replaying)
		: MetricPlayStatString(missionName)
		, m_Variant(variant)
		, m_Checkpoint(checkpoint)
		, m_Failed(failed)
		, m_Canceled(canceled)
		, m_skipped(skipped)
		, m_timespent(timespent)
		, m_replaying(replaying)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStatString::Write(rw);

		if (m_Variant > 0)
		{
			result = result && rw->WriteUns("var", m_Variant);
		}

		if (m_Checkpoint > 0)
		{
			result = result && rw->WriteUns("chk", m_Checkpoint);
		}

		if (m_timespent > 0)
		{
			result = result && rw->WriteUns("ts", m_timespent);
		}

		//No Param "r" - Mission was Passed
		if (m_Failed)
		{
			result = result && rw->WriteUns("r", 2);
		}
		else if (m_Canceled)
		{
			result = result && rw->WriteUns("r", 1);
		}
		else if (m_skipped)
		{
			result = result && rw->WriteUns("r", 3);
		}

		result = result && rw->BeginArray("cash", NULL);
		{
			result = result && rw->WriteUns(NULL, StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetHash(CHAR_MICHEAL)));
			result = result && rw->WriteUns(NULL, StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetHash(CHAR_FRANKLIN)));
			result = result && rw->WriteUns(NULL, StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetHash(CHAR_TREVOR)));
		}
		result = result && rw->End();

		if (m_replaying)
		{
			result = result && rw->WriteBool("replay", m_replaying);
		}

		return result;
	}

private:
	u32   m_Variant;
	u32   m_Checkpoint;
	u32   m_timespent;
	bool  m_Failed;
	bool  m_Canceled;
	bool  m_skipped;
	bool  m_replaying;
};

class MetricCHARACTER_SKILLS : public MetricPlayStatString
{
	RL_DECLARE_METRIC(SKILLS, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_MEDIUM_PRIORITY);

private:
	struct sSkills
	{
	public:
		u32 m_SpecialAbility;
		u32 m_Stamina;
		u32 m_Shooting;
		u32 m_Strength;
		u32 m_Stealth;
		u32 m_Flying;
		u32 m_Driving;
		u32 m_LungCapacity;
	};


public:

	explicit MetricCHARACTER_SKILLS(const char* missionName, const u64 mid) : MetricPlayStatString(missionName), m_mid(mid)
	{
		static const StatId_char STAT_SPECIAL_ABILITY_UNLOCKED = STATIC_STAT_ID_CHAR("SPECIAL_ABILITY_UNLOCKED", ATSTRINGHASH("SP0_SPECIAL_ABILITY_UNLOCKED", 0x4ECD9F81), ATSTRINGHASH("SP1_SPECIAL_ABILITY_UNLOCKED", 0x51CCFBA3), ATSTRINGHASH("SP2_SPECIAL_ABILITY_UNLOCKED", 0x5B06442), ATSTRINGHASH("MP0_SPECIAL_ABILITY_UNLOCKED", 0x3C46E471), ATSTRINGHASH("MP1_SPECIAL_ABILITY_UNLOCKED", 0x2C4A30CD));
		static const StatId_char STAT_SHOOTING_ABILITY         = STATIC_STAT_ID_CHAR("SHOOTING_ABILITY", ATSTRINGHASH("SP0_SHOOTING_ABILITY", 0xB4892709), ATSTRINGHASH("SP1_SHOOTING_ABILITY", 0xCB261497), ATSTRINGHASH("SP2_SHOOTING_ABILITY", 0x2A3A74EA), ATSTRINGHASH("MP0_SHOOTING_ABILITY", 0xB7D704EB), ATSTRINGHASH("MP1_SHOOTING_ABILITY", 0x9F635F5D));

		m_charskills[CHAR_MICHEAL].m_SpecialAbility = StatsInterface::GetIntStat(STAT_SPECIAL_ABILITY_UNLOCKED.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_Stamina        = StatsInterface::GetIntStat(STAT_STAMINA.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_Shooting       = StatsInterface::GetIntStat(STAT_SHOOTING_ABILITY.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_Strength       = StatsInterface::GetIntStat(STAT_STRENGTH.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_Stealth        = StatsInterface::GetIntStat(STAT_STEALTH_ABILITY.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_Flying         = StatsInterface::GetIntStat(STAT_FLYING_ABILITY.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_Driving        = StatsInterface::GetIntStat(STAT_WHEELIE_ABILITY.GetHash(CHAR_MICHEAL));
		m_charskills[CHAR_MICHEAL].m_LungCapacity   = StatsInterface::GetIntStat(STAT_LUNG_CAPACITY.GetHash(CHAR_MICHEAL));

		m_charskills[CHAR_FRANKLIN].m_SpecialAbility = StatsInterface::GetIntStat(STAT_SPECIAL_ABILITY_UNLOCKED.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_Stamina        = StatsInterface::GetIntStat(STAT_STAMINA.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_Shooting       = StatsInterface::GetIntStat(STAT_SHOOTING_ABILITY.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_Strength       = StatsInterface::GetIntStat(STAT_STRENGTH.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_Stealth        = StatsInterface::GetIntStat(STAT_STEALTH_ABILITY.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_Flying         = StatsInterface::GetIntStat(STAT_FLYING_ABILITY.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_Driving        = StatsInterface::GetIntStat(STAT_WHEELIE_ABILITY.GetHash(CHAR_FRANKLIN));
		m_charskills[CHAR_FRANKLIN].m_LungCapacity   = StatsInterface::GetIntStat(STAT_LUNG_CAPACITY.GetHash(CHAR_FRANKLIN));

		m_charskills[CHAR_TREVOR].m_SpecialAbility = StatsInterface::GetIntStat(STAT_SPECIAL_ABILITY_UNLOCKED.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_Stamina        = StatsInterface::GetIntStat(STAT_STAMINA.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_Shooting       = StatsInterface::GetIntStat(STAT_SHOOTING_ABILITY.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_Strength       = StatsInterface::GetIntStat(STAT_STRENGTH.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_Stealth        = StatsInterface::GetIntStat(STAT_STEALTH_ABILITY.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_Flying         = StatsInterface::GetIntStat(STAT_FLYING_ABILITY.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_Driving        = StatsInterface::GetIntStat(STAT_WHEELIE_ABILITY.GetHash(CHAR_TREVOR));
		m_charskills[CHAR_TREVOR].m_LungCapacity   = StatsInterface::GetIntStat(STAT_LUNG_CAPACITY.GetHash(CHAR_TREVOR));
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStatString::Write(rw);

		result = result && rw->WriteInt64("mid", static_cast<s64>(m_mid));

		result = result && rw->BeginArray("michael", NULL);
		{
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_SpecialAbility);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_Stamina);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_Shooting);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_Strength);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_Stealth);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_Flying);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_Driving);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_MICHEAL].m_LungCapacity);
		}
		result = result && rw->End();

		result = result && rw->BeginArray("franklin", NULL);
		{
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_SpecialAbility);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_Stamina);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_Shooting);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_Strength);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_Stealth);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_Flying);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_Driving);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_FRANKLIN].m_LungCapacity);
		}
		result = result && rw->End();

		result = result && rw->BeginArray("trevor", NULL);
		{
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_SpecialAbility);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_Stamina);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_Shooting);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_Strength);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_Stealth);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_Flying);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_Driving);
			result = result && rw->WriteUns(NULL, m_charskills[CHAR_TREVOR].m_LungCapacity);
		}
		result = result && rw->End();

		return result;
	}

private:
	sSkills m_charskills[MAX_NUM_SP_CHARS];
	u64 m_mid;
};


class MetricMISSION_CHECKPOINT : public MetricPlayStatString
{
	RL_DECLARE_METRIC(MIS_C, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_HIGH_PRIORITY);

public:

	explicit MetricMISSION_CHECKPOINT(const char* missionName, const u32 variant, const u32 previousCheckpoint, const u32 checkpoint, const u32 timespent)
		: MetricPlayStatString(missionName)
		, m_PreviousCheckpoint(previousCheckpoint)
		, m_Variant(variant)
		, m_Checkpoint(checkpoint)
		, m_timespent(timespent)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStatString::Write(rw);

		if (m_Variant > 0)
		{
			result = result && rw->WriteUns("var", m_Variant);
		}

		if (m_PreviousCheckpoint > 0)
		{
			result = result && rw->WriteUns("pchk", m_PreviousCheckpoint);
		}

		if (m_Checkpoint > 0)
		{
			result = result && rw->WriteUns("chk", m_Checkpoint);
		}

		if (m_timespent > 0)
		{
			result = result && rw->WriteUns("ts", m_timespent);
		}

		return result;
	}

private:
	u32 m_Variant;
	u32 m_PreviousCheckpoint;
	u32 m_Checkpoint;
	u32 m_timespent;
};

class MetricRANDOMMISSION_DONE : public MetricPlayStatString
{
	RL_DECLARE_METRIC(RMIS_DONE, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_HIGH_PRIORITY);

public:

	explicit MetricRANDOMMISSION_DONE(const char* missionName, const int outcome, const int timespent, const int attempts) : MetricPlayStatString(missionName), m_outcome(outcome), m_timespent(timespent), m_attempts(attempts)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStatString::Write(rw);

		if (m_outcome > 0)
			result = result && rw->WriteUns("res", m_outcome);

		if (m_timespent > 0)
			result = result && rw->WriteUns("ts", m_timespent);

		if (m_attempts > 0)
			result = result && rw->WriteUns("att", m_attempts);

		return result;
	}

private:
	int m_outcome;
	int m_timespent;
	int m_attempts;
};

class MetricCUTSCENE : public MetricPlayStat
{
	RL_DECLARE_METRIC(CUTSCENE, TELEMETRY_CHANNEL_MEDIA, LOGLEVEL_LOW_PRIORITY);

public:

    explicit MetricCUTSCENE(const u32 nameHash, const u32 duration, const bool skipped)
        : m_Name(nameHash), m_Duration(duration), m_Skipped(skipped)
    {
    }

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);
		
		result = result && rw->WriteUns("id", m_Name);

		if (m_Duration > 0)
		{
			result = result && rw->WriteUns("d", m_Duration);
		}

		if (m_Skipped)
		{
			result = result && rw->WriteInt("s", 1);
		}

		return result;
	}

private:
	u32  m_Name;
	u32  m_Duration;
	bool m_Skipped;
};

class MetricCHEAT : public MetricPlayStatString
{
	RL_DECLARE_METRIC(CHEAT, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

    explicit MetricCHEAT(const char* cheatName)
        : MetricPlayStatString(cheatName)
    {
    }
};


class MetricRDEV : public MetricPlayStat
{
	RL_DECLARE_METRIC(RDEV, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	explicit MetricRDEV(const rlGamerHandle& gamerhandle)
	{
		m_GamerHandle[0] = '\0';
		if (gnetVerify(gamerhandle.IsValid()))
		{
			gamerhandle.ToString(m_GamerHandle, COUNTOF(m_GamerHandle));
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);

		if (gnetVerify(m_GamerHandle[0] != '\0'))
		{
			result = result && rw->WriteString("g", m_GamerHandle);
		}

		return result;
	}

private:
	char m_GamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
};


class MetricRQA : public MetricPlayStat
{
	RL_DECLARE_METRIC(RQA, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	explicit MetricRQA(const rlGamerHandle& gamerhandle)
	{
		m_GamerHandle[0] = '\0';
		if (gnetVerify(gamerhandle.IsValid()))
		{
			gamerhandle.ToString(m_GamerHandle, COUNTOF(m_GamerHandle));
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);

		if (gnetVerify(m_GamerHandle[0] != '\0'))
		{
			result = result && rw->WriteString("g", m_GamerHandle);
		}

		return result;
	}

private:
	char m_GamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
};

class MetricREPORTER : public MetricPlayStat
{
	RL_DECLARE_METRIC(REPORTER, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);
	MetricREPORTER(const char* userID, const char* reportedID, const u32 type, const u32 numberoftimes) 
		: m_type(type)
		, m_numberoftimes(numberoftimes)
	{
		formatf(m_userID, 64,"%s",userID);
		formatf(m_reportedID, 64,"%s",reportedID);
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw)
			&& rw->WriteString("UserId",m_userID)
			&& rw->WriteString("ReportedId", m_reportedID)
			&& rw->WriteUns("type", m_type)
			&& rw->WriteUns("xtimes", m_numberoftimes);
	}

private:

	u32 m_type;
	u32 m_numberoftimes;
	char m_userID[64];
	char m_reportedID[64];
};

class MetricREPORTINVALIDOBJECT : public MetricPlayStat
{
	RL_DECLARE_METRIC(REPORT_INVALIDMODEL, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	explicit MetricREPORTINVALIDOBJECT(const rlGamerHandle& reportedHandle, const u32 modelHash, const u32 reportReason)
		: m_ModelHash(modelHash)
		, m_ReportReason(reportReason)
	{
		m_ReportGamerHandle[0] = '\0';
		if (gnetVerify(reportedHandle.IsValid()))
		{
			reportedHandle.ToString(m_ReportGamerHandle, COUNTOF(m_ReportGamerHandle));
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);
		if (gnetVerify(m_ReportGamerHandle[0] != '\0'))
		{
			result = result && rw->WriteString("a", m_ReportGamerHandle);
		}
		result = result && rw->WriteUns("b", m_ModelHash);
		result = result && rw->WriteUns("c", m_ReportReason);

		return result;
	}

private:
	char m_ReportGamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
	u32 m_ModelHash;
	u32 m_ReportReason;
};

class MetricMEM : public MetricPlayStat
{
	RL_DECLARE_METRIC(MEM_NEW, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricMEM(const u32 hashname, const u32 newvalue) 
		: m_hashname(hashname)
		, m_newvalue(newvalue)
	{;}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw)
			&& rw->WriteUns("name", m_hashname)
			&& rw->WriteUns("value", m_newvalue);
	}

private:
	u32 m_hashname;
	u32 m_newvalue;
};

class MetricDEBUGGER : public MetricPlayStat
{
	RL_DECLARE_METRIC(DEBUGGER_ATTACH, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricDEBUGGER(const u32 newvalue) : m_newvalue(newvalue) {;}
	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw) && rw->WriteUns("value", m_newvalue);
	}

private:
	u32 m_newvalue;
};

class MetricGARAGETAMPER : public MetricPlayStat
{
	RL_DECLARE_METRIC(GARAGE_TAMPER, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricGARAGETAMPER(const u32 newvalue) : m_newvalue(newvalue) {;}
	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw) && rw->WriteUns("value", m_newvalue);
	}

private:
	u32 m_newvalue;
};

#define MetricCODECRCFAIL MetricInfoChange
class MetricCODECRCFAIL : public MetricPlayStat
{
	RL_DECLARE_METRIC(CCF_UPDATE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCODECRCFAIL(const u32 version, const u32 address, const u32 crc) : m_version(version), m_address(address), m_crc(crc) {}
	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw) && rw->WriteUns("a", m_version) && rw->WriteUns("b", m_address) && rw->WriteUns("c", m_crc);
	}

private:
	u32 m_version;
	u32 m_address;
	u32 m_crc;
};

class MetricTamper : public MetricPlayStat
{
	RL_DECLARE_METRIC(AUX_DEUX, TELEMETRY_CHANNEL_MISC,  LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricTamper(const int param) : m_param(param) {}
	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw) && rw->WriteInt("param", m_param);
	}

private:
	int m_param;
};

class MetricWEATHER : public MetricPlayStat
{
	RL_DECLARE_METRIC(WEATHER, TELEMETRY_CHANNEL_DEV_DEBUG, LOGLEVEL_DEBUG1);

public:
	explicit MetricWEATHER(const u32 prevIndex, const u32 newIndex) : m_prevIndex(prevIndex), m_newIndex(newIndex)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPlayStat::Write(rw);
		result = result && rw->WriteUns("prev", m_prevIndex);
		result = result && rw->WriteUns("new", m_newIndex);
		return result;
	}

private:
	u32 m_prevIndex;
	u32 m_newIndex;
};


class MetricUSED_VOICE_CHAT : public rlMetric
{
	RL_DECLARE_METRIC(UVC, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY );

public:

	explicit MetricUSED_VOICE_CHAT(const bool usedVoiceChat)
		: m_usedVoiceChat(usedVoiceChat)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return rw->WriteBool("vc", m_usedVoiceChat);
	}

private:

	bool m_usedVoiceChat;
};

class MetricSESSION_MATCH_ENDED : public MetricMpSession
{
	RL_DECLARE_METRIC(SES_ME, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricSESSION_MATCH_ENDED(const u32 matchType,
							const u32 timespent,
							const char* matchCreator,
							const char* uniqueMatchId,
							const bool isTeamDeathmatch,
							const int  raceType,
							const bool leftInProgress)
		: MetricMpSession(false)
		, m_MatchType(matchType)
		, m_isTeamDeathmatch(isTeamDeathmatch)
		, m_raceType(raceType)
		, m_leftInProgress(leftInProgress)
		, m_timespent(timespent)
	{
		safecpy(m_MatchCreator, matchCreator, COUNTOF(m_MatchCreator));
		safecpy(m_UniqueMatchId, uniqueMatchId, COUNTOF(m_UniqueMatchId));
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;

		result = result && this->MetricMpSession::Write(rw);
		result = result && rw->WriteUns("mt", m_MatchType);
		result = result && rw->WriteString("mc", m_MatchCreator);
		result = result && rw->WriteString("mid", m_UniqueMatchId);

		if (m_isTeamDeathmatch)
			result = result && rw->WriteBool("teamd", m_isTeamDeathmatch);

		if (-1 < m_raceType)
			result = result && rw->WriteInt("race", m_raceType);

		if (m_leftInProgress)
			result = result && rw->WriteBool("left", m_leftInProgress);

		if (m_timespent > 0)
			result = result && rw->WriteUns("ts", m_timespent);

		return result;
	}

private:
	u32  m_MatchType;
	char m_MatchCreator[MetricPlayStat::MAX_STRING_LENGTH];
	char m_UniqueMatchId[MetricPlayStat::MAX_STRING_LENGTH];
	bool m_isTeamDeathmatch;
	int  m_raceType;
	bool m_leftInProgress;
	u32  m_timespent;
};

//////// Misc playstats ////////

class MetricEARNED_ACHIEVEMENT : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(ACHE, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:

    explicit MetricEARNED_ACHIEVEMENT(const unsigned achievementId)
        : m_AchievementId(achievementId)
    {
    }

    virtual bool Write(RsonWriter* rw) const
    {
        return this->MetricPlayerCoords::Write(rw)
                && rw->WriteUns("id", m_AchievementId);
    }

private:

    unsigned m_AchievementId;
};

class MetricAUD_TRACK_TAGGED : public MetricPlayStat
{
	RL_DECLARE_METRIC(AUD_TAG, TELEMETRY_CHANNEL_MEDIA, LOGLEVEL_VERYHIGH_PRIORITY);

public:

    explicit MetricAUD_TRACK_TAGGED(const unsigned trackId)
        : m_TrackId(trackId)
    {
    }

    virtual bool Write(RsonWriter* rw) const
    {
        return this->MetricPlayStat::Write(rw)
                && rw->WriteUns("id", m_TrackId);
    }

private:

    unsigned m_TrackId;
};

class MetricAUD_STATION_DETUNED : public MetricVehicle
{
	RL_DECLARE_METRIC(AUD_DET, TELEMETRY_CHANNEL_MEDIA, LOGLEVEL_VERYLOW_PRIORITY);

public:

	explicit MetricAUD_STATION_DETUNED(const u32 mi, const u32 trackId, const u32 timePlayed, const bool tuned, u32 usbMixId, u32 favourites1, u32 favourites2)
		: MetricVehicle(mi), m_TrackId(trackId), m_TimePlayed(timePlayed), m_Tuned(tuned), m_usbMixId(usbMixId), m_favourites1(favourites1), m_favourites2(favourites2)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricVehicle::Write(rw);

		result = result && rw->WriteUns("id", m_TrackId);
		result = result && rw->WriteUns("tp", m_TimePlayed);
		result = result && rw->WriteBool("tuned", m_Tuned);

		if(m_usbMixId > 0)
		{
			result = result && rw->WriteUns("track", m_usbMixId);
		}

		result = result && rw->WriteUns("f1", m_favourites1);
		result = result && rw->WriteUns("f2", m_favourites2);

		return result;
	}

private:
	u32  m_TrackId;
	u32  m_TimePlayed;
	bool m_Tuned;
	u32  m_usbMixId;
	u32  m_favourites1;
	u32  m_favourites2;
};

class MetricShopping : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(SHOP, TELEMETRY_CHANNEL_POSSESSIONS, LOGLEVEL_HIGH_PRIORITY);

public:

	explicit MetricShopping(const u32 itemHash, const u32 amountSpent, const u32 shopName, const u32 extraItemHash, const u32 colorHash) 
		: m_ItemHash(itemHash)
		, m_AmountSpent(amountSpent)
		, m_shopName(shopName)
		, m_extraItemHash(extraItemHash)
		, m_colorHash(colorHash)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayerCoords::Write(rw);

		result = result && rw->WriteUns("id", m_ItemHash);
		result = result && rw->WriteUns("as", m_AmountSpent);
		result = result && rw->WriteUns("sn", m_shopName);

		if (m_extraItemHash > 0)
		{
			result = result && rw->WriteUns("extraid", m_extraItemHash);
		}

		if (m_colorHash > 0)
		{
			result = result && rw->WriteUns("color", m_colorHash);
		}

		return result;
	}

private:
	u32 m_ItemHash;
	u32 m_AmountSpent;
	u32 m_shopName;
	u32 m_extraItemHash;
	u32 m_colorHash;
};

class MetricTV_SHOW : public MetricPlayStat
{
	RL_DECLARE_METRIC(TV, TELEMETRY_CHANNEL_MEDIA, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricTV_SHOW(const u32 playListId, const u32 tvShowId, const u32 channelId, const u32 timeWatched) 
		: m_playListId(playListId), m_tvShowId(tvShowId), m_channelId(channelId), m_timeWatched(timeWatched)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);

		if (m_playListId > 0)
		{
			result = result && rw->WriteUns("id", m_playListId);
		}

		if (m_tvShowId > 0)
		{
			result = result && rw->WriteUns("tv", m_tvShowId);
		}

		if (m_channelId > 0)
		{
			result = result && rw->WriteUns("tvc", m_channelId);
		}

		if (m_timeWatched > 0)
		{
			result = result && rw->WriteUns("t", m_timeWatched);
		}

		return result;
	}

private:
	u32 m_playListId;
	u32 m_tvShowId;
	u32 m_channelId;
	u32 m_timeWatched;
};

//////// Store UI Telemetry messages ////////

class MetricINGAMESTOREACTION : public MetricPlayStat
{	
	RL_DECLARE_METRIC(IGSACT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	enum eStoreTelemetryActions
	{
		ENTER_STORE=0,
		EXIT_STORE_NO_CHECKOUT,
		EXIT_STORE_CHECKOUT
	};

public:

	explicit MetricINGAMESTOREACTION(eStoreTelemetryActions actionId, u32 msSinceInputEnabled)
		: m_ActionId(actionId),
		  m_TimeSinceInputEnabled(msSinceInputEnabled)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		const char* actionString = "";

		switch(m_ActionId)
		{
		case(ENTER_STORE):
			actionString = "enter";
			break;
		case(EXIT_STORE_NO_CHECKOUT):
			actionString = "exitNoCo";
			break;
		case(EXIT_STORE_CHECKOUT):
			actionString = "exitDidCo";
			break;
		default:
			gnetAssertf(false,"Unhandled enum value in eStoreTelemetryActions");
			actionString = "unknown";
		}

		return this->MetricPlayStat::Write(rw)
			&& rw->WriteString("act", actionString)
			&& rw->WriteUns("inpTim", m_TimeSinceInputEnabled);
	}

private:

	eStoreTelemetryActions m_ActionId;
	u32 m_TimeSinceInputEnabled;
};


// Pointer to the file for playstats debug.
static rlTelemetryStream s_TelemetryStrm;

void CNetworkTelemetry::Init(unsigned initMode)
{
	if (PARAM_nonetwork.Get())
		return;

    if(initMode == INIT_CORE)
    {

		rlTelemetry::Init(true);
		rlTelemetry::AddDelegate(&s_OnInternalDelegate);

		m_flushRequestedTime        = 0;

		sm_AudPrevTunedStation      = ~0u;
		sm_AudPlayerTunedStation    = ~0u;
		sm_AudPlayerPrevTunedStation = ~0u;
		sm_AudMostFavouriteStation  = 0;
		sm_AudLeastFavouriteStation = 0;
		sm_AudTrackStartTime        = AUD_START_TIME_INVALID;
		sm_AudPrevTunedUsbMix       = ~0u;

		sm_OnlineJoinType = 0;

		sm_MissionStartedTime = 0;
		sm_MissionStartedTimePaused = 0;
		sm_MissionCheckpointTime = 0;
		sm_MissionCheckpointTimePaused = 0;
		OUTPUT_ONLY( sm_MissionCheckpoint = 0; )
		sm_MissionStarted.Clear();

#if RSG_OUTPUT
		u32 cwpId = 0;
		if(PARAM_telemetryplaytest.Get(cwpId))
		{
			sm_CompanyWidePlaytestId = cwpId;
		}
#endif // RSG_OUTPUT

		rlTelemetryPolicies policies;
		atDelegate<bool (RsonWriter*)> headerInfoCallback(CNetworkTelemetry::SetGamerHeader);
		policies.GameHeaderInfoCallback = headerInfoCallback;

		#define DEFINE_METRIC_CHANNEL(CHANNEL, CHANNEL_HASH) policies.m_Channels[CHANNEL].m_Id = atHashString(#CHANNEL, CHANNEL_HASH)

		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_AMBIENT, 0x3ABE84E6);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_FLOW, 0x3E78735E);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_JOB, 0x71A2F962);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_MEDIA, 0x72AD6C98);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_MISC, 0xD93600DF);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_MISSION, 0xA5E2905A);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_MONEY, 0xA9983317);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_POSSESSIONS, 0xBAA37017);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_DEV_DEBUG, 0xF01C39FA);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_DEV_CAPTURE, 0x6C15F920);
		DEFINE_METRIC_CHANNEL(TELEMETRY_CHANNEL_RAGE, 0xAC5CBD48);

		rlTelemetry::SetPolicies(policies);
    }
    else if(initMode == INIT_SESSION)
    {
#if RSG_OUTPUT
		CNetworkTelemetry::AddMetricFileMark();
#endif //RSG_OUTPUT

		s_AppendMetricVehicleDriven = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("APPEND_METRIC_VEHICLE_DRIVEN", 0xd9d5a0ae), true);
		s_metricMaxFpsSamples = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("METRIC_MAX_FPS_SAMPLES", 0x1154cb16), s_metricMaxFpsSamples);

		// The tunable is a percentage. we roll a dice and see if we're part of the percentage of players who are going to collect samples
		float percentageOfPlayersCollectingSamples = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("METRIC_INGAME_FPS_POP_SAMPLE", 0xedff32fe), 100.0f);
		mthRandom rand( (int) fwTimer::GetSystemTimeInMilliseconds() );
		float diceRoll = rand.GetRanged(0.0f, 100.0f);
		gnetDebug1("METRIC_INGAME_FPS_POP_SAMPLE (%f %%) dice roll (0, 100) = %f   -> %s", percentageOfPlayersCollectingSamples, diceRoll, (diceRoll <= percentageOfPlayersCollectingSamples) ? "succeed" : "failed");
		if(diceRoll <= percentageOfPlayersCollectingSamples)
		{
			s_sendInGameFpsMetrics = true;
		}

		s_replayingMission = false;
		s_MatchHistoryId = 0;
		s_MatchStartedTime = 0;
		sm_MissionStartedTime = 0;
		sm_MissionStartedTimePaused = 0;
		sm_MissionCheckpointTime = 0;
		sm_MissionCheckpointTimePaused = 0;
		OUTPUT_ONLY( sm_MissionCheckpoint = 0; )
		sm_OnlineJoinType = 0;
		s_telemetryFlushFailedCounter = 0;

		sm_MatchStartedId.Clear();
		sm_RootContentId = 0;
    }
}

void  CNetworkTelemetry::SignedOnline()
{
#if RSG_PC
	// Queue up a posting of hardware stats
	sm_bPostHardwareStats = true;

	if(g_rlPc.GetCommerceManager())
	{
		g_rlPc.GetCommerceManager()->SetPurchaseFlowTelemetryCallback(GetPurchaseFlowTelemetry);
	}
#endif
	s_telemetryFlushFailedCounter = 0;
}

void  CNetworkTelemetry::SignedOut()
{
	rlTelemetry::CancelFlushes();
	s_telemetryFlushFailedCounter = 0;
}

XPARAM(nonetwork);

//Appends a metric instance to the buffer.  
//If the buffer fills up Flush() will be called.
bool  CNetworkTelemetry::AppendMetric(const rlMetric& m, const bool deferredCashTransaction) 
{
	if (PARAM_nonetwork.Get())
		return true;

	bool result = false;

	if (-1 < NetworkInterface::GetLocalGamerIndex())
	{
#if RSG_OUTPUT
		if (m.GetMetricLogChannel() >= TELEMETRY_CHANNEL_DEV_DEBUG && m.GetMetricLogChannel() <= TELEMETRY_CHANNEL_DEV_CAPTURE)
			return CNetworkDebugTelemetry::AppendDebugMetric(&m);
#endif // RSG_OUTPUT

		if (m.GetMetricLogLevel() == LOGLEVEL_HIGH_VOLUME)
		{
			return AppendHighVolumeMetric(&m);
		}
		else if (m.GetMetricLogChannel() == TELEMETRY_CHANNEL_MONEY)
		{
			result = rlTelemetry::WriteImmediate(NetworkInterface::GetLocalGamerIndex(), m, deferredCashTransaction);
		}
		else
		{
			result = rlTelemetry::Write(NetworkInterface::GetLocalGamerIndex(), m);
		}
	}

#if !__NO_OUTPUT

	if (result)
	{
		LogMetricDebug("Metric Appended - \"%s\".", m.GetMetricName());
	}
	else
	{
		LogMetricWarning("Failed to Append Metric - \"%s\", LocalGamerIndex='%d'.", m.GetMetricName(), NetworkInterface::GetLocalGamerIndex());
	}

#endif //!__NO_OUTPUT

	return result;
}

u64 CNetworkTelemetry::GetCurMpSessionId()
{
	return s_CurMpSessionId;
}

u8 CNetworkTelemetry::GetCurMpGameMode()
{
	return s_CurMpGameMode;
}

u8 CNetworkTelemetry::GetCurMpNumPubSlots()
{
	return s_CurMpNumPubSlots;
}

u8 CNetworkTelemetry::GetCurMpNumPrivSlots()
{
	return s_CurMpNumPrivSlots;
}

void CNetworkTelemetry::SetOnlineJoinType(const u8 type)
{
	gnetDebug1("Change multiplayer join type from '%u' to '%u'", sm_OnlineJoinType, type);
	sm_OnlineJoinType = type;
}

u8 CNetworkTelemetry::GetOnlineJoinType()
{
	return sm_OnlineJoinType;
}

u32 CNetworkTelemetry::GetMatchStartTime()
{
	gnetAssert( s_MatchHistoryId );
	gnetAssert( s_MatchStartedTime );
	return s_MatchStartedTime;
}

//Flush current telemetry buffer - flushes all appended metrics.
bool CNetworkTelemetry::FlushTelemetry(bool flushnow) 
{
	if (PARAM_nonetwork.Get())
		return true;

	if (PARAM_telemetryFlushCallstack.Get())
	{
		gnetDebug1("CNetworkTelemetry::FlushTelemetry(%s) ", flushnow ? "true" : "false");
		sysStack::PrintStackTrace();
	}

	if (0 > NetworkInterface::GetLocalGamerIndex())
	{
		gnetWarning("Failed to Flush Metric - LOCAL USER IS NOT SIGNED IN.");
		return true;
	}

	if (!NetworkInterface::IsLocalPlayerOnline())
	{
		gnetWarning("Failed to Flush Metric - LOCAL USER IS NOT ONLINE.");
		return true;
	}

	//All buffers are empty - no metrics to be flushed
	if (!rlTelemetry::HasAnyMetrics())
	{
		gnetDebug1("Failed to Flush Metric - ALL BUFFERS ARE EMPTY.");
		return true;
	}

	bool result = false;

	//Credentials are invalid we can not Call Flush
	if (!NetworkInterface::HasValidRosCredentials())
	{
		gnetWarning("Failed to Flush Metric - CREDENTIALS ARE INVALID.");
		return result;
	}

	//Both flush buffers are being used
	if (rlTelemetry::AreAllPending())
	{
		gnetWarning("Failed to Flush Metric - ALL BUFFERS ARE PENDING FLUSH.");
		return result;
	}

	if (flushnow)
	{
		result = rlTelemetry::Flush( );
		m_flushRequestedTime = 0;

#if RSG_OUTPUT
		// In mp games, flush debug telemetry each time normal one happens (this is mainly to cover end of mission flushes)
		if( NetworkInterface::IsNetworkOpen() )
		{
			CNetworkDebugTelemetry::UpdateAndFlushDebugMetricBuffer(true); 
		}
#endif
	}
	else
	{
		result = true;
		m_flushRequestedTime = sysTimer::GetSystemMsTime() + DEFAULT_DELAY_FLUSH_INTERVAL;
	}

	return result;
}

void CNetworkTelemetry::OnDeferredFlush( void )
{
	statWarningf("Cash Transactions being flushed... Count of cloud transactions is '%u'", s_cloudCashTransactionCount);

	if (rlTelemetry::GetDeferredFlushBlocked())
	{
		bool savetypeShop = true;
		Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("IMMEDIATE_FLUSH_SHOP_ONLY_TYPE", 0xb8305c41), savetypeShop);

		if (savetypeShop && 0 == s_cloudCashTransactionCount)
		{
			statWarningf("Calling StatsInterface::Save() for category STAT_SAVETYPE_END_SHOPPING, due to flushing cash events.");
			StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, STAT_SAVETYPE_END_SHOPPING);
		}
		else
		{
			statWarningf("Calling StatsInterface::Save() for category STAT_SAVETYPE_IMMEDIATE_FLUSH, due to flushing cash events.");
			StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, STAT_SAVETYPE_IMMEDIATE_FLUSH);
		}
	}

	s_cloudCashTransactionCount = 0;
}

#define s_minorityReport mrintt
static StatId s_minorityReport("MINORITY_REPORT");

void CNetworkTelemetry::OnTelemetryFlushEnd( const netStatus& status )
{
	if (status.Failed())
	{
		s_telemetryFlushFailedCounter +=1;

		if(s_telemetryFlushFailedCounter == TRIGGER_MINORITY_REPORT
			|| (s_telemetryFlushFailedPlayerJoined && s_telemetryFlushFailedCounter > TRIGGER_MINORITY_REPORT))
		{
			if (NetworkInterface::IsGameInProgress())
			{
				s_telemetryFlushFailedPlayerJoined = false;
				gnetDebug1("s_telemetryFlushFailedCounter '%" I64FMT "d' > '%d' TELEMETRY_BLOCK", s_telemetryFlushFailedCounter, TRIGGER_MINORITY_REPORT);
				CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::TELEMETRY_BLOCK);
				StatsInterface::IncrementStat(s_minorityReport, 1);
			}
		}
	}
	else if (status.Succeeded())
	{
		s_telemetryFlushFailedCounter      = 0;
		s_telemetryFlushFailedPlayerJoined = false;

		if (NetworkInterface::IsGameInProgress())
		{
			StatsInterface::SetStatData(s_minorityReport, 0);
		}
	}
}

void CNetworkTelemetry::OnTelemetyEvent( const rlTelemetryEvent& evt )
{
	switch (evt.GetId())
	{
	case TELEMETRY_EVENT_DEFERRED_FLUSH:
		{
			OnDeferredFlush();
		} break;
	case TELEMETRY_EVENT_FLUSH_END:
		{
			OnTelemetryFlushEnd(*static_cast<const rlTelemetryEventFlushEnd&>(evt).m_Status);
		} break;
	case TELEMETRY_EVENT_CONFIG_CHANGED:
		{
			netConnectionManager::MetricConfigChanged();
			OUTPUT_ONLY(CNetworkDebugTelemetry::TelemetryConfigChanged());
		} break;
	default: break;
	}
}

void CNetworkTelemetry::PlayerHasJoined(const netPlayer& player)
{
	if (!player.IsLocal())
	{
		if(s_telemetryFlushFailedCounter >= TRIGGER_MINORITY_REPORT)
		{
			if (player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
			{
				CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::TELEMETRY_BLOCK);
			}
			else
			{
				s_telemetryFlushFailedPlayerJoined = true;
			}
		}
	}
}

void CNetworkTelemetry::IncrementCloudCashTransactionCount( void )
{
	s_cloudCashTransactionCount += 1;
}

#if RSG_OUTPUT
void InsertTelemetryHeader()
{
	// If writing to a remote file put the local time in the file name, to distinct different sessions playstats.
	if (s_TelemetryStrm.Stream && CFileMgr::IsValidFileHandle((FileHandle)s_TelemetryStrm.Stream))
	{
		s_TelemetryStrm.Stream->Write("\n\n------------------------------------------\n", istrlen("\n\n------------------------------------------\n"));

		s_TelemetryStrm.Stream->Write("\nPLATFORM - ", istrlen("\nPLATFORM - "));

		const u32 platformId = g_rlTitleId ? g_rlTitleId->m_RosTitleId.GetPlatformId() : rlRosTitleId::PlatformId::PLATFORM_ID_INVALID;
		if (platformId == rlRosTitleId::PlatformId::PLATFORM_ID_DURANGO)
			s_TelemetryStrm.Stream->Write("DURANGO", istrlen("DURANGO"));
		else if (platformId == rlRosTitleId::PlatformId::PLATFORM_ID_ORBIS)
			s_TelemetryStrm.Stream->Write("ORBIS", istrlen("ORBIS"));
		else if (platformId == rlRosTitleId::PlatformId::PLATFORM_ID_PC)
			s_TelemetryStrm.Stream->Write("PC", istrlen("PC"));
		else if (platformId == rlRosTitleId::PlatformId::PLATFORM_ID_PROSPERO)
			s_TelemetryStrm.Stream->Write("PROSPERO", istrlen("PROSPERO"));
		else if (platformId == rlRosTitleId::PlatformId::PLATFORM_ID_SCARLETT)
			s_TelemetryStrm.Stream->Write("SCARLETT", istrlen("SCARLETT"));

		s_TelemetryStrm.Stream->Write("\nENVIRONMENT - ", istrlen("\nENVIRONMENT - "));

		rlRosEnvironment rosEnvironment = g_rlTitleId ? g_rlTitleId->m_RosTitleId.GetEnvironment() : RLROS_ENV_UNKNOWN;
		if (rlRosGetEnvironmentAsString(rosEnvironment))
		{
			s_TelemetryStrm.Stream->Write(rlRosGetEnvironmentAsString(rosEnvironment), istrlen(rlRosGetEnvironmentAsString(rosEnvironment)));
		}

		//////////////////////////////////////////////////////////////////////////
		time_t timer;
		time(&timer);
		struct tm* tm = localtime(&timer);
		gnetAssert(tm);

		char timestamp[64];
		sysMemSet(timestamp, 0, 64);

		if (tm)
		{
			formatf(timestamp, "%02d:%02d:%02d  %02d\\%02d\\%04d \n", tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_mday, tm->tm_mon + 1, 1900 + tm->tm_year);
		}

		s_TelemetryStrm.Stream->Write("\n\n------------------------------------------\n", istrlen("\n\n------------------------------------------\n"));
		s_TelemetryStrm.Stream->Write("\nTimestamp - ", istrlen("\nTimestamp - "));
		s_TelemetryStrm.Stream->Write(timestamp, istrlen(timestamp));
#if RSG_OUTPUT
		char metricPosixtime[64];
		sysMemSet(metricPosixtime, 0, 64);
		formatf(metricPosixtime, "%" I64FMT "d\n", rlGetPosixTime());

		s_TelemetryStrm.Stream->Write("\nMetric Timestamp - ", istrlen("\nMetric Timestamp - "));
		s_TelemetryStrm.Stream->Write(metricPosixtime, istrlen(metricPosixtime));
#endif //RSG_OUTPUT

		s_TelemetryStrm.Stream->Write("\n", istrlen("\n"));
		s_TelemetryStrm.Stream->Write("\n------------------------------------------\n\n", istrlen("\n------------------------------------------\n\n"));
	}
}
#endif //RSG_OUTPUT

void CNetworkTelemetry::SetupTelemetryStream()
{
	if (s_TelemetryStrm.Stream)
		return;

#if RSG_OUTPUT
	if (PARAM_telemetrywritefile.Get() && !PARAM_nonetlogs.Get())
	{
		char filename[256];

		const char* baseFilename;
		if(PARAM_telemetrybasefilename.Get(baseFilename) && baseFilename)
		{
			CNetworkTelemetry::SetUpFilename(baseFilename, filename, sizeof(filename));
		}
		else
		{
			char tempfilename[256];

			static const char* DEFAULT_FILE_NAME = "playstats";

			formatf(tempfilename, sizeof(tempfilename), "%s", DEFAULT_FILE_NAME);

#if RSG_PC && !__NO_OUTPUT
			u32 processInstance = 0;
			if(PARAM_processinstance.Get(processInstance))
			{
				if(AssertVerify(processInstance > 0))
				{
					formatf(tempfilename, sizeof(tempfilename), "instance%d_%s", processInstance, DEFAULT_FILE_NAME);
				}
			}
#endif // RSG_PC && !__NO_OUTPUT

			CNetworkTelemetry::SetUpFilename(tempfilename, filename, sizeof(filename));
		}

		//APPEND
		s_TelemetryStrm.Stream = (fiStream*) CFileMgr::OpenFileForAppending(filename);
		if(s_TelemetryStrm.Stream)
		{
			s_TelemetryStrm.Stream->Seek(s_TelemetryStrm.Stream->Size());
		}
		//CREATE
		else
		{
			s_TelemetryStrm.Stream = (fiStream*) CFileMgr::OpenFileForWriting(filename);
		}

		gnetAssertf(s_TelemetryStrm.Stream, "Failed to create telemetry stream for file: %s", filename);
	}
#endif // RSG_OUTPUT

	if(s_TelemetryStrm.Stream)
	{
		rlTelemetry::AddStream(&s_TelemetryStrm);
	}
	else
	{
		//Write metrics Only to a server.
	}

#if RSG_OUTPUT
	// If writing to a remote file put the local time in the file name, to distinct different sessions playstats.
	InsertTelemetryHeader();
#endif // RSG_OUTPUT
}

void CNetworkTelemetry::Shutdown(unsigned shutdownMode)
{
	if (PARAM_nonetwork.Get())
		return;

	if(shutdownMode == SHUTDOWN_CORE)
    {
		//Flush current telemetry buffer
		gnetVerify( FlushTelemetry( true ) );

        if(s_TelemetryStrm.Stream)
        {
            s_TelemetryStrm.Stream->Close();
            s_TelemetryStrm.Stream = NULL;
			rlTelemetry::RemoveStream(&s_TelemetryStrm);
        }

		rlTelemetry::RemoveDelegate(&s_OnInternalDelegate);
		rlTelemetry::Shutdown();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
        ExitGame();        // This records the stat EXIT_GAME.

		//Lets reset this shit out... 
		s_MatchHistoryId = 0;
		s_MatchStartedTime = 0;
		s_replayingMission = false;
		sm_MatchStartedId.Clear();
		sm_RootContentId = 0;

		sm_AudPrevTunedStation = ~0u;
		sm_AudMostFavouriteStation  = 0;
		sm_AudLeastFavouriteStation = 0;
		sm_AudTrackStartTime = AUD_START_TIME_INVALID;
		sm_AudPlayerTunedStation = ~0u;
		sm_AudPlayerPrevTunedStation = ~0u;
		sm_AudPrevTunedUsbMix       = ~0u;

		sm_OnlineJoinType = 0;

		sm_MissionStartedTime = 0;
		sm_MissionStartedTimePaused = 0;
		sm_MissionCheckpointTime = 0;
		sm_MissionCheckpointTimePaused = 0;
		OUTPUT_ONLY( sm_MissionCheckpoint = 0; )
		sm_MissionStarted.Clear();
    }
}

/////////////////////////////////////////////////////////////
// FUNCTION: Update
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::Update()
{
	if (PARAM_nonetwork.Get())
		return;

	//Online bounded operations.
	//No point doing anything if the local player is not Online.
	if (!NetworkInterface::IsLocalPlayerOnline())
		return;

#if RSG_PC
	// Wait for profile settings to be valid
	if (sm_bPostHardwareStats && CProfileSettings::GetInstance().AreSettingsValid())
	{
		PostGamerHardwareStats();
		sm_bPostHardwareStats = false;
	}
#endif

	//Set the metric if it has not yet been done.
	CNetworkTelemetry::StartGame();

	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	//Update Time paused
	if (0 < sm_MissionStartedTime && fwTimer::IsGamePaused())
	{
		sm_MissionStartedTimePaused += fwTimer::GetSystemTimeStepInMilliseconds();

		if(0 < sm_MissionCheckpointTime)
		{
			sm_MissionCheckpointTimePaused += fwTimer::GetSystemTimeStepInMilliseconds();
		}
	}

	// Track on foot movement
	UpdateOnFootMovementTracking();

    // Every so often send changes to the currently tuned audio track.
    if(!m_LastAudTrackTime || (curTime - m_LastAudTrackTime) >= sm_AudTrackInterval)
    {
#if NA_RADIO_ENABLED
		static u32 s_vehiclemi = fwModelId::MI_INVALID;

		//Setup current radio station
		const audRadioStation* audibleStation = audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior() ? g_RadioAudioEntity.FindAudibleStation(25.0f) : nullptr;
		if (audibleStation && audibleStation->GetNameHash() == ATSTRINGHASH("HIDDEN_RADIO_CASINO", 0x6436272B))
		{
			gnetDebug1("audRadioStation - Skip HIDDEN_RADIO_CASINO");
			audibleStation = nullptr;

		}
		if (g_RadioAudioEntity.IsPlayerRadioActive() || audibleStation)
		{
			gnetDebug3("audRadioStation - g_RadioAudioEntity.IsPlayerRadioActive() || audibleStation");
			unsigned curTunedAudioStation = g_RadioAudioEntity.GetPlayerRadioStation() ? g_RadioAudioEntity.GetPlayerRadioStation()->GetStationImmutableIndex(IMMUTABLE_INDEX_GLOBAL) : ~0u;
			if (curTunedAudioStation == ~0u)
			{
				curTunedAudioStation = audibleStation ? audibleStation->GetStationImmutableIndex(IMMUTABLE_INDEX_GLOBAL) : 0;
			}

			bool stationChanged = curTunedAudioStation != sm_AudPrevTunedStation;
			u32 curUsbMixId = 0;
			if(!stationChanged && audibleStation && audibleStation->IsUSBMixStation())
			{
				curUsbMixId = audibleStation->GetCurrentTrack().GetSoundRef();
				stationChanged = curUsbMixId != sm_AudPrevTunedUsbMix;
			}

			if (stationChanged)
			{
				if (~0u != sm_AudPrevTunedStation)
				{
					audRadioStation* station = audRadioStation::FindStationWithImmutableIndex(sm_AudPrevTunedStation, IMMUTABLE_INDEX_GLOBAL);
					if (station)
					{
						u32 timePlayed = 0;
						if (sm_AudTrackStartTime > AUD_START_TIME_INVALID && station->GetListenTimer() - sm_AudTrackStartTime > 0.0f)
						{
							timePlayed = (u32) (station->GetListenTimer() - sm_AudTrackStartTime);
						}
						sm_AudTrackStartTime  = AUD_START_TIME_INVALID;

						u32 usbMixId = 0;
						if(station->IsUSBMixStation())
						{
							usbMixId = station->GetCurrentTrack().GetSoundRef();
						}

						u32 favourites1, favourites2;
						audRadioStation::GetFavouritedStations(favourites1, favourites2);

						//Instantiate the metric.
						MetricAUD_STATION_DETUNED m(s_vehiclemi, station->GetNameHash(), timePlayed, (sm_AudPlayerTunedStation != ~0u && sm_AudPlayerTunedStation == curTunedAudioStation), usbMixId, favourites1, favourites2);
						//Append the metric.
						AppendMetric(m);

						s_vehiclemi = fwModelId::MI_INVALID;
					}
				}

				if (~0u != curTunedAudioStation)
				{
					audRadioStation* station = audRadioStation::FindStationWithImmutableIndex(curTunedAudioStation, IMMUTABLE_INDEX_GLOBAL);
					if (gnetVerify(station) && station->GetNameHash() != AUD_OFF)
					{
						//Set Start time
						sm_AudTrackStartTime = station->GetListenTimer();

						CVehicle* vehicle = FindPlayerVehicle();
						if (vehicle)
						{
							s_vehiclemi = vehicle->GetModelIndex();
						}
					}
				}

				sm_AudPrevTunedStation = curTunedAudioStation;
				if(audibleStation && audibleStation->IsUSBMixStation()) 
				{
					sm_AudPrevTunedUsbMix = curUsbMixId;
				}
				else
				{
					sm_AudPrevTunedUsbMix = ~0u;
				}
			}
		}
		else if(~0u != sm_AudPrevTunedStation)
		{
			gnetDebug3("audRadioStation - ~0u != sm_AudPrevTunedStation");
			audRadioStation* station = audRadioStation::FindStationWithImmutableIndex(sm_AudPrevTunedStation, IMMUTABLE_INDEX_GLOBAL);
			if (station)
			{
				u32 timePlayed = 0;
				if (sm_AudTrackStartTime > AUD_START_TIME_INVALID && station->GetListenTimer() - sm_AudTrackStartTime > 0.0f)
				{
					timePlayed = (u32) (station->GetListenTimer() - sm_AudTrackStartTime);
				}
				sm_AudTrackStartTime  = AUD_START_TIME_INVALID;

				u32 usbMixId = 0;
				if(station->IsUSBMixStation())
				{
					usbMixId = station->GetCurrentTrack().GetSoundRef();
				}

				u32 favourites1, favourites2;
				audRadioStation::GetFavouritedStations(favourites1, favourites2);
				//Instantiate the metric.
				MetricAUD_STATION_DETUNED m(s_vehiclemi, station->GetNameHash(), timePlayed, (sm_AudPlayerTunedStation != ~0u && sm_AudPlayerTunedStation == sm_AudPrevTunedStation), usbMixId, favourites1, favourites2);
				//Append the metric.
				AppendMetric(m);

				s_vehiclemi = fwModelId::MI_INVALID;
			}

			sm_AudPrevTunedStation = ~0u;
			sm_AudPlayerTunedStation = ~0u;
			sm_AudPlayerPrevTunedStation = ~0u;
			sm_AudPrevTunedUsbMix = ~0u;
		}

		u32 mostFavourite, leastFavourite = 0;
		g_RadioAudioEntity.FindFavouriteStations(mostFavourite, leastFavourite);
		if (mostFavourite != sm_AudMostFavouriteStation)
		{
			sm_AudMostFavouriteStation = mostFavourite;
		}

		if (leastFavourite != sm_AudLeastFavouriteStation)
		{
			sm_AudLeastFavouriteStation = leastFavourite;
		}

		if (NetworkInterface::IsGameInProgress())
		{
			u32 mostFavourite, leastFavourite = 0;
			mostFavourite = StatsInterface::GetIntStat(STAT_MPPLY_MOST_FAVORITE_STATION);
			if (mostFavourite != sm_AudMostFavouriteStation && sm_AudMostFavouriteStation > 0)
			{
				StatsInterface::SetStatData(STAT_MPPLY_MOST_FAVORITE_STATION, (int)sm_AudMostFavouriteStation);
			}

			leastFavourite = StatsInterface::GetIntStat(STAT_MPPLY_LEAST_FAVORITE_STATION);
			if (leastFavourite != sm_AudMostFavouriteStation && sm_AudLeastFavouriteStation > 0)
			{
				StatsInterface::SetStatData(STAT_MPPLY_LEAST_FAVORITE_STATION, (int)sm_AudLeastFavouriteStation);
			}
		}
		else
		{
			u32 mostFavourite, leastFavourite = 0;
			mostFavourite = StatsInterface::GetIntStat(STAT_SP_MOST_FAVORITE_STATION);
			if (mostFavourite != sm_AudMostFavouriteStation && sm_AudMostFavouriteStation > 0)
			{
				StatsInterface::SetStatData(STAT_SP_MOST_FAVORITE_STATION, (int)sm_AudMostFavouriteStation);
			}

			leastFavourite = StatsInterface::GetIntStat(STAT_SP_LEAST_FAVORITE_STATION);
			if (leastFavourite != sm_AudLeastFavouriteStation && sm_AudLeastFavouriteStation > 0)
			{
				StatsInterface::SetStatData(STAT_SP_LEAST_FAVORITE_STATION, (int)sm_AudLeastFavouriteStation);
			}
		}

#endif
        m_LastAudTrackTime = curTime;
    }

	// Track if the user is using voice or text chat
	if(NetworkInterface::IsNetworkOpen())
	{
		if(!sm_HasUsedVoiceChatSinceLastTime)
		{
			sm_HasUsedVoiceChatSinceLastTime = NetworkInterface::UsedVoiceChat();
		}

		// we send periodic updates only in freeroaming
		if(!NetworkInterface::IsActivitySession() && (!sm_LastVoiceChatTrackTime || (curTime - sm_LastVoiceChatTrackTime) >= sm_VoiceChatTrackInterval) )
		{
			// We dont want to send the value again if it didnt change
			if(sm_PreviouslySentVoiceChatUsage!=sm_HasUsedVoiceChatSinceLastTime || sm_ShouldSendVoiceChatUsage)
			{
				MetricUSED_VOICE_CHAT m(sm_HasUsedVoiceChatSinceLastTime);
				AppendMetric(m);
				sm_LastVoiceChatTrackTime = curTime;
				sm_PreviouslySentVoiceChatUsage=sm_HasUsedVoiceChatSinceLastTime;
				sm_ShouldSendVoiceChatUsage=false;
				Displayf("[VoiceChatTelemetry] sending %s", sm_HasUsedVoiceChatSinceLastTime ? "true" : "false");
			}
			sm_HasUsedVoiceChatSinceLastTime=false;
		}

		// track timeouts for chat types and send current telemetry state
		const u32 kOneMinute = 1000u * 60u;
		int commsTelemetryIdleTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("COMMS_TELEMETRY_IDLE_TIME", 0x8a930a70), kOneMinute);


		// only track if we are enabled
		bool sendVoiceTelemetry = false;
		if (NetworkInterface::GetVoice().IsEnabled())
		{
			sm_CumulativeVoiceCommsInfo.UsingVoice(NetworkInterface::UsedVoiceChat());

			// so lets figure out the voice chat type
			int voiceChatType = 0;
			
			if (NetworkInterface::GetVoice().IsTeamOnlyChat())
			{
				voiceChatType |= MetricComms::CHAT_TYPE_TEAM;
			}

			//Set voice chat type through script set chatOption member
			bool isChatRestricted = true;
			switch (sm_ChatOption) {
			case ChatOption::Everyone:
				voiceChatType |= MetricComms::CHAT_TYPE_PUBLIC;
				isChatRestricted = false;
				break;
			case ChatOption::Crew:
				voiceChatType |= MetricComms::CHAT_TYPE_CREW;
				break;
			case ChatOption::Friends:
				voiceChatType |= MetricComms::CHAT_TYPE_FRIENDS;
				break;
			case ChatOption::CrewAndFriends:
				voiceChatType |= MetricComms::CHAT_TYPE_CREW;
				voiceChatType |= MetricComms::CHAT_TYPE_FRIENDS;
				break;
			default:
				break;
			}

			const bool isPrivate = NetworkInterface::GetVoice().GetFocusTalker().IsValid() ||
				CNetwork::GetNetworkVoiceSession().IsInVoiceSession() ||
				isChatRestricted;

			voiceChatType |= isPrivate ? MetricComms::CHAT_TYPE_PRIVATE : MetricComms::CHAT_TYPE_PUBLIC;

			if (sm_CumulativeVoiceCommsInfo.m_voiceChatType == MetricComms::CHAT_TYPE_NONE)
			{
				sm_CumulativeVoiceCommsInfo.m_voiceChatType = voiceChatType;
			}

			bool commsPastIdleTime = curTime > sm_CumulativeVoiceCommsInfo.m_lastChatTime && (curTime - sm_CumulativeVoiceCommsInfo.m_lastChatTime) >= commsTelemetryIdleTime;
			if ( sm_CumulativeVoiceCommsInfo.m_lastChatTime && ( commsPastIdleTime || sm_CumulativeVoiceCommsInfo.m_voiceChatType != voiceChatType ) )
			{
				sendVoiceTelemetry = true;
			}
		}
		else
		{
			// we have stopped using voice chat send the last stored telemetry
			if (sm_CumulativeVoiceCommsInfo.m_lastChatTime != 0)
			{
				sendVoiceTelemetry = true;
			}
		}

		sm_ChatIdExpiryTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("COMMS_TELEMETRY_CHAT_EXPIRY_TIME", 0xbb4d3ef4), (kOneMinute * 5u));
		u32 currentNetTime = CNetwork::GetNetworkClock().GetTime();

		// Handle current voice ChatID
		if (sm_ChatIdExpiryTime > 0)
		{
			CNetworkVoice& netVoice = NetworkInterface::GetVoice();
			if (sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId > 0)
			{
				if (netVoice.IsAnyGamerAboutToTalk() || netVoice.IsAnyGamerTalking())
				{
					gnetDebug3("CNetworkTelemetry::Update - COMMS - Voice chat keep alive time updated: %u [old: %u]", curTime, sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive);
					sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive = curTime;
				}
				else if ((curTime > sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive) &&
					(curTime - sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive) > sm_ChatIdExpiryTime)
				{
					// Current chat session has expired. New ID will need to be generated.
					gnetDebug3("CNetworkTelemetry::Update - COMMS - Reseting voice chat ID... [old ID: %u, lastChatTime: %u]", sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId, sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive);
					sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId = 0;
					sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive = 0;
				}
			}
			else if ((netVoice.IsAnyRemoteGamerAboutToTalk() || netVoice.IsAnyRemoteGamerTalking()) && netVoice.GetFocusTalker().IsValid())
			{
				const u32 chatIDTimeBlock = currentNetTime / sm_ChatIdExpiryTime;
				const u64 talkingPlayerID = netVoice.GetFocusTalker().ToUniqueId();

				sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId = CNetworkTelemetry::GenerateNewChatID(chatIDTimeBlock, talkingPlayerID);
				sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive = curTime;
				gnetDebug3("CNetworkTelemetry::Update - COMMS - Player (%" I64FMT "u) starting a new voice chat ID: %u [time: %u]", talkingPlayerID, sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId, curTime);
			}
		}
#if RSG_BANK
		else
		{
			gnetDebug3("CNetworkTelemetry::Update - COMMS - Failed to create new voide ChatID. sm_ChatIdExpiryTime == 0");
		}
#endif // RSG_BANK


		if (sendVoiceTelemetry)
		{
			rlGamerHandle playerHandle = NetworkInterface::GetVoice().GetFocusTalker();
			if (sm_ChatIdExpiryTime > 0)
			{
				if (sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId == 0)
				{
					u32 chatIdTimeBlock = currentNetTime / sm_ChatIdExpiryTime;
					sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId = CNetworkTelemetry::GenerateNewChatID(chatIdTimeBlock, playerHandle.ToUniqueId());
					sm_CumulativeVoiceCommsInfo.m_LastTimeOfChatIdKeepAlive = curTime;
					gnetDebug3("CNetworkTelemetry::Update - COMMS - Local player (%" I64FMT "u) starting a new voice chat ID: %u [time: %u]", playerHandle.ToUniqueId(), sm_CumulativeVoiceCommsInfo.m_CurrentVoiceChatId, curTime);
				}
			}

			// Insuring recAccId is set for voice sessions with 2 people when sending telemetry
			if (CNetwork::GetNetworkVoiceSession().IsInVoiceSession()) {
				rlGamerInfo gamers[RL_MAX_GAMERS_PER_SESSION];
				const unsigned numGamers = CNetwork::GetNetworkVoiceSession().GetGamers(gamers, RL_MAX_GAMERS_PER_SESSION);

				if (numGamers == 2) {
					for (int i = 0; i < numGamers; i++) {
						if (gamers[i].IsRemote()) {
							playerHandle = gamers[i].GetGamerHandle();
						}
					}
				}
			}

#if RSG_BANK
			else
			{
				gnetDebug3("CNetworkTelemetry::Update - COMMS - Failed to create new voide ChatID. sm_ChatIdExpiryTime == 0");
			}
#endif // RSG_BANK

			sm_CumulativeVoiceCommsInfo.SendTelemetry( NetworkInterface::GetPlayerFromGamerHandle(playerHandle) );
		}
		
		for (int cumulativeChatType = 0; cumulativeChatType < MetricComms::NUM_CUMULATIVE_CHAT_TYPES; ++cumulativeChatType)
		{
			MetricComms::CumulativeTextCommsInfo& cci = sm_CumulativeTextCommsInfo[cumulativeChatType];

			if (cci.m_lastChatTime && ((curTime - cci.m_lastChatTime) >= commsTelemetryIdleTime))
			{
				cci.SendTelemetry( (MetricComms::eChatTelemetryCumulativeChatCategory)cumulativeChatType, nullptr, -commsTelemetryIdleTime);
			}
		}
	}

	// Update the fps capture
	UpdateFramesPerSecondResult();


	//Check if we have any pending flush requests.
	if (m_flushRequestedTime>0 && m_flushRequestedTime<curTime)
	{
		FlushTelemetry( true );
		m_flushRequestedTime = 0;
	}

	//Update our High Volume metric
	UpdateHighVolumeMetric( );
}

/////////////////////////////////////////////////////////////
// FUNCTION: SetUpFilename
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::SetUpFilename(const char* fileNameBase, char* fileNameResult, const unsigned lenofResult)
{
	char ipStr[netIpAddress::MAX_STRING_BUF_SIZE];
	sysMemSet(ipStr, 0, netIpAddress::MAX_STRING_BUF_SIZE);

	const char *dirPrefix = "";

#if RSG_XBOX || RSG_SCE
	dirPrefix = "debug:/";

	if(PARAM_logfile.Get())
	{
		dirPrefix = "";
	}

#endif //RSG_XBOX || RSG_SCE

#if RSG_XBOX || RSG_PC
	const char* logPrefix = "";
	if(PARAM_netlogprefix.Get(logPrefix))
	{
		formatf(ipStr, netIpAddress::MAX_STRING_BUF_SIZE, logPrefix);
	}
	else
#endif // RSG_DURANGO || RSG_PC
	{
		netIpAddress ip;
		netHardware::GetLocalIpAddress(&ip);
		ip.Format(ipStr, netIpAddress::MAX_STRING_BUF_SIZE);

		if (ip.IsV4())
		{
			sysMemSet(ipStr, 0, netIpAddress::MAX_STRING_BUF_SIZE);
			ip.Format(ipStr);
		}
		else
		{
			char ipStrTemp[netIpAddress::MAX_STRING_BUF_SIZE];
			sysMemSet(ipStrTemp, 0, netIpAddress::MAX_STRING_BUF_SIZE);

			int j=0;
			for (int i=0; i<netIpAddress::MAX_STRING_BUF_SIZE; i++)
			{
				if (ipStr[i] != 0x5B && ipStr[i] != 0x5D) // '[' - 0x5b, ']' - 0x5D
				{
					if (ipStr[i] != 0x3A) // ':' - 0x3a
						ipStrTemp[j] = ipStr[i];
					else // '.' - 0x2E
						ipStrTemp[j] = 0x2E;
					j++;
				}
			}
			formatf(ipStr, netIpAddress::MAX_STRING_BUF_SIZE, ipStrTemp);

#if RSG_SCE
			SceNetCtlInfo info;
			int err = sceNetCtlGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &info);
			if(0 <= err)
			{
				sysMemSet(ipStr, 0, netIpAddress::MAX_STRING_BUF_SIZE);
				formatf(ipStr, netIpAddress::MAX_STRING_BUF_SIZE, info.ip_address);
			}
#endif // RSG_SCE
		}
	}

	formatf(fileNameResult, lenofResult, "%s%s_%s.log", dirPrefix, ipStr, fileNameBase);

#if __BANK
	const char *logFilePath = "";
	if(PARAM_logfile.Get(logFilePath))
	{			
		++RAGE_LOG_DISABLE;	// Disable the assert for the call to new within shared_ptr::Resetp
		std::string logFilePathStr(logFilePath);				
		std::string logFilePathFinal = logFilePathStr.substr(0, logFilePathStr.find_last_of("\\"));
		logFilePathFinal.append("\\");
		logFilePathFinal.append(fileNameResult);
		std::strcpy(fileNameResult, logFilePathFinal.c_str());
		--RAGE_LOG_DISABLE; // Re-enable the assert		
	}		
#endif //#if __BANK
}

void
CNetworkTelemetry::SetSpSessionId(const u64 id)
{
    s_CurSpSessionId = id;
}

u64
CNetworkTelemetry::GetSpSessionId()
{
    return s_CurSpSessionId;
}

void
CNetworkTelemetry::FlushCommsTelemetry()
{
	if (sm_CumulativeVoiceCommsInfo.m_lastChatTime != 0)
	{
		sm_CumulativeVoiceCommsInfo.SendTelemetry(NetworkInterface::GetPlayerFromGamerHandle(NetworkInterface::GetVoice().GetFocusTalker()));
	}

	for (int cumulativeChatType = 0; cumulativeChatType < MetricComms::NUM_CUMULATIVE_CHAT_TYPES; ++cumulativeChatType)
	{
		if (sm_CumulativeTextCommsInfo[cumulativeChatType].m_lastChatTime != 0)
		{
			sm_CumulativeTextCommsInfo[cumulativeChatType].SendTelemetry((MetricComms::eChatTelemetryCumulativeChatCategory)cumulativeChatType);
		}
	}
}

int
CNetworkTelemetry::GetMPCharacterCounter()
{
	int counter = 0;

	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		//First Time lucky
		if (!settings.Exists(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_0))
		{
			settings.Set(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_0, 1);
			settings.Set(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_1, 1);
			settings.Set(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_2, 1);
			settings.Set(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_3, 1);
			settings.Set(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_4, 1);
			settings.Write();

			counter = 1;
		}
		else
		{
			switch (StatsInterface::GetStatsModelPrefix())
			{
			case CHAR_MP0: counter = settings.GetInt(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_0); break;
			case CHAR_MP1: counter = settings.GetInt(CProfileSettings::TELEMETRY_MPCHARACTER_COUNTER_1); break;
			default:
				gnetAssertf(0, "Unsupported Multiplayer character Index %d", StatsInterface::GetStatsModelPrefix());
			}
		}
	}

	return counter;
}

u64 CNetworkTelemetry::GetMatchHistoryId()
{
	return s_MatchHistoryId;
}

void CNetworkTelemetry::SetPreviousMatchHistoryId(OUTPUT_ONLY(const char* caller))
{
	s_MatchHistoryId = s_previousMatchHistoryId;
#if !__NO_OUTPUT
	formatf(s_MatchHistoryIdLastModified, "[%d] SetPreviousMatchHistoryId - %s", fwTimer::GetFrameCount(), caller);
#endif // !__NO_OUTPUT
}

void CNetworkTelemetry::ClearAllMatchHistoryId( )
{
	s_MatchHistoryId = 0;
	s_previousMatchHistoryId = 0;
}

bool 
CNetworkTelemetry::SetGamerHeader(RsonWriter* rw)
{
	if (gnetVerify(rw))
	{
		u32 ver = CNetwork::GetVersion();
		gnetAssertf(0<ver, "Game header version must be > 0");
		if (0==ver) ver = 0xFFFF;
		bool result = rw->WriteUns("ver", ver);

#if RSG_OUTPUT
		const char* level = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetCurrentLevelIndex());
		if (level)
		{
			result = result && rw->WriteUns("lvl", atStringHash(level));
		}
		else
		{
			result = result && rw->WriteUns("lvl", CGameLogic::GetCurrentLevelIndex());
		}

		if (sm_CompanyWidePlaytestId != 0)
		{
			result = result && rw->WriteUns("cwp", sm_CompanyWidePlaytestId);
		}
#endif // RSG_OUTPUT

		result = result && rw->WriteUns("cid", StatsInterface::GetStatsModelPrefix());
		if (NetworkInterface::IsNetworkOpen() && StatsInterface::GetStatsModelPrefix()>CHAR_SP_END)
		{
			result = result && rw->WriteInt64("sid", static_cast<s64>(s_CurMpSessionId));
			result = result && rw->WriteUns("mp", (u32)GetMPCharacterCounter());
		}

		//Add current match ID for the match history
		if(0 < s_MatchHistoryId)
		{
			result = result && rw->WriteInt64("mid", static_cast<s64>(s_MatchHistoryId));
		}

		// Now the playing experience session id
		result = result && rw->WriteUns64("ptsid", sm_playtimeSessionId);
		
#if RSG_GEN9
		result = result && rw->WriteUns("ed", static_cast<unsigned>(CGameEditionManager::GetInstance()->GameEditionForTelemetry()));
#else
		// Current Game Edition (temporary hard-coded placeholder of GE_LEGACY)
		static u32 geHash = ATSTRINGHASH("GE_LEGACY", 0xF1BBB180);
		result = result && rw->WriteUns("ed", static_cast<unsigned>(geHash));
#endif

		// Grab the platform Id (include, where appropriate, check for playing on newer
		// consoles in backwards compatibility mode)
		unsigned titlePlatformId = rlRosTitleId::GetPlatformId();

#if RSG_ORBIS
		if (NetworkInterface::IsPlayingInBackCompatOnPS5())
		{
			titlePlatformId = rlRosTitleId::PLATFORM_ID_PROSPERO;
		}
#endif
		result &= rw->WriteUns("platid", titlePlatformId);

#if NET_ENABLE_MEMBERSHIP_METRICS
		// membership / event data
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
		result &= rw->WriteInt("mbr", rlScMembership::GetMembershipTelemetryState(NetworkInterface::GetLocalGamerIndex()));
#else
		result &= rw->WriteInt("mbr", NET_DUMMY_NOT_MEMBER_STATE);
#endif
#endif
		result &= rw->WriteUns("gev", SocialClubEventMgr::Get().GetGlobalEventHash());
#if NET_ENABLE_MEMBERSHIP_METRICS
		result &= rw->WriteUns("mev", SocialClubEventMgr::Get().GetMembershipEventHash());
#endif

		result &= rw->WriteInt("st", static_cast<int>(NetworkInterface::GetSessionTypeForTelemetry()));

		return result;
	}

	return false;
}

#if RSG_OUTPUT
void 
CNetworkTelemetry::AddMetricFileMark()
{
	if (PARAM_telemetrywritefile.Get() && !PARAM_nonetlogs.Get())
	{
		// If writing to a remote file put the local time in the file name, to distinct different sessions playstats.
		InsertTelemetryHeader();
	}
}

#endif // RSG_OUTPUT

void CNetworkTelemetry::UpdateHighVolumeMetric( )
{
	s_highVolumeMetricListTimeout.Update();

	//We don't have any metrics to work with.
	if (0 == s_highVolumeMetricListSize)
		return;

	rlGamerHandle gamerHandle;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	bool validCredentials = RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) 
		&& rlRos::GetCredentials(localGamerIndex).IsValid() 
		&& rlPresence::GetGamerHandle(localGamerIndex, &gamerHandle);

	if(!validCredentials)
	{
		//If so, release the memory and flag that we're not taking anymore entries.
		if(s_highVolumeMetricListSize > HIGH_VOLUME_METRIC_FLUSH_THESHOLD_BYTES)
		{
			gnetDebug1("Releasing High Volume Telemetry buffer because it's filled up and we don't have credentials.");
			ReleaseHighVolumeMetricBuffer();
		}

		return;
	}

	//If we have a critical mass, send them
	if (s_highVolumeMetricListSize > HIGH_VOLUME_METRIC_FLUSH_THESHOLD_BYTES || (s_highVolumeMetricListTimeout.IsTimedOut()))
	{
		s_highVolumeMetricListTimeout.Clear();

		gnetDebug1("Submitting HighVolume Metric buffer with '%d' bytes in metrics", s_highVolumeMetricListSize);
		const unsigned MEMORY_STORE_METRIC_MEMORY_POLICY_SIZE = 1280; //This needs to be big enough to hold the biggest EXPORTED metric we have ( MetricMEMORY_STORE )
		rlTelemetrySubmissionMemoryPolicy memPolicy(MEMORY_STORE_METRIC_MEMORY_POLICY_SIZE);

		const u64 curTime = rlGetPosixTime();

		//Write the metric to registered streams
		const rlMetric* pMetricIter = s_highVolumeMetricList.GetFirst();
		while (pMetricIter)
		{
			rlTelemetry::DoWriteToStreams(localGamerIndex, curTime, gamerHandle, *pMetricIter);
			pMetricIter = pMetricIter->GetConstNext();
		}

		gnetVerifyf(rlTelemetry::SubmitMetricList(localGamerIndex, &s_highVolumeMetricList, &memPolicy), "Failed to submit telemetry for MetricMEMORY_STORE");

		memPolicy.ReleaseAndClear();

		ReleaseHighVolumeMetricBuffer();
	}
}

void CNetworkTelemetry::ReleaseHighVolumeMetricBuffer()
{
	sysMemAllocator* allocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);
	if (!gnetVerifyf(allocator, "Unable to get the allocator to free memory"))
	{
		OUTPUT_ONLY(Quitf("Bailing out because we should NOT be here!");)
		return;
	}

	for(rlMetric* m = s_highVolumeMetricList.Pop(); m; m = s_highVolumeMetricList.Pop())
		allocator->Free(m);

	gnetDebug1("ReleaseHighVolumeMetricBuffer freeing '%d'", s_highVolumeMetricListSize);
	gnetAssert(s_highVolumeMetricList.GetFirst() == NULL);

	s_highVolumeMetricListSize = 0;
	s_highVolumeMetricListTimeout.Clear();
}

bool CNetworkTelemetry::AppendHighVolumeMetric(const rlMetric* m)
{
	if (!gnetVerify(m))
		return false;

	//Metric is Disabled
	if (!rlTelemetry::ShouldSendMetric(*m OUTPUT_ONLY(, "HighVolumeMetric")))
	{
		gnetDebug1("Avoid CNetworkTelemetry::AppendMetric() Metric '%s' is disabled.", m->GetMetricName());
		return true;
	}

	const size_t metricSize = m->GetMetricSize();

	rlMetric* newMetric = 0;

	sysMemAllocator* allocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

	//Allocate a chunk of memory appropriate sized for this metric.
	newMetric = (rlMetric*) (allocator->TryAllocate(metricSize, 0) );
	gnetAssertf(newMetric, "Failed to allocate memory to add metric '%s' to buffer", m->GetMetricName());

	if (!newMetric)
		return false;

	//We are starting collection.
	if (0 == s_highVolumeMetricListSize)
	{
		gnetAssert(!s_highVolumeMetricListTimeout.IsRunning());
		s_highVolumeMetricListTimeout.InitMilliseconds( HIGH_VOLUME_METRIC_TIMEOUT );
	}

	//Slam it, like it's old school
	sysMemCpy((void*)newMetric, (void*)m, metricSize);

	//Now put it in the list
	s_highVolumeMetricList.Append(newMetric);
	s_highVolumeMetricListSize += (unsigned)metricSize;

	gnetDebug1("Appending HighVolumeMetric '%s' of size '%d'.", newMetric->GetMetricName(), (int)metricSize);

	return true;
}

u32 CNetworkTelemetry::GenerateNewChatID(const u64& netTime, const u64& senderId)
{
	u32 hash = 0;

	const u32 lowNetTime = netTime & 0xFFFF;
	const u32 highNetTime = (netTime >> 32) & 0xFFFF;
	const u32 lowSenderId = senderId & 0xFFFF;
	const u32 highSenderId = (senderId >> 32) & 0xFFFF;

	hash = atDataHash(&lowNetTime, sizeof(lowNetTime), hash);
	hash = atDataHash(&highNetTime, sizeof(highNetTime), hash);
	hash = atDataHash(&lowSenderId, sizeof(lowSenderId), hash);
	hash = atDataHash(&highSenderId, sizeof(highSenderId), hash);

	return hash;
}

/////////////////////////////////////////////////////////////
// FUNCTION: PlayerVehicleDistance
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::PlayerVehicleDistance(const u32 keyHash, const float distance, const u32 timeinvehicle, const char* areaName, const char* districtName, const char* streetName, const bool owned)
{
	if (s_AppendMetricVehicleDriven)
	{
		const int location = CNetworkTelemetry::GetVehicleDrivenLocation();
		//Instantiate the metric.
		MetricVEHICLE_DIST_DRIVEN m(keyHash, distance, timeinvehicle, areaName, districtName, streetName, owned,
			CNetworkTelemetry::GetVehicleDrivenInTestDrive(),
			location > 0 ? (u32) location : 0u);
		//Append the metric.
		AppendMetric(m);
	}
}

void CNetworkTelemetry::AppendMetricRockstarDEV(const rlGamerHandle& gamerhandle)
{
	//Instantiate the metric.
	MetricRDEV m(gamerhandle);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::AppendMetricRockstarQA(const rlGamerHandle& gamerhandle)
{
	//Instantiate the metric.
	MetricRQA m(gamerhandle);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::AppendMetricReporter( const char* UserId, const char* reportedId, const u32 type, const u32 numberoftimes )
{
	//Instantiate the metric.
	MetricREPORTER m(UserId, reportedId,type, numberoftimes);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::AppendMetricReportInvalidModelCreated(const rlGamerHandle& reportedHandle, const u32 modelHash, const u32 reportReason)
{
	//Instantiate the metric.
	MetricREPORTINVALIDOBJECT m(reportedHandle, modelHash, reportReason);
	//Append the metric.
	AppendMetric(m);
}

// Death Event
void CNetworkTelemetry::PlayerDied(const CPed* victim, const CPed* killer, const u32 weaponHash, const int weaponDamageComponent, const bool withMeleeWeapon)
{
	gnetAssert(victim && victim->IsPlayer());

	if (NetworkInterface::IsLocalPlayerOnline() && victim && victim->IsPlayer())
	{
		bool sendEventFromKiller = false;
		bool sendEventFromVictim = false;

		if ((killer && killer->IsLocalPlayer()) || victim->IsLocalPlayer())
		{
			sendEventFromKiller = killer && killer->IsLocalPlayer();
			sendEventFromVictim = victim->IsLocalPlayer();

			// Tunables override sending only from the killer or victim
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("TELEMETRY_DEATH_KILLER", 0x9DFF8F00), sendEventFromKiller);
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("TELEMETRY_DEATH_VICTIM", 0x967AE54B), sendEventFromVictim);
		}

		if (sendEventFromKiller || sendEventFromVictim)
		{
			//Instantiate the metric.
			bool pvp = (killer && killer->IsPlayer()) && (victim && victim->IsPlayer()) && killer != victim;
			bool isKiller = killer && killer->IsLocalPlayer();
			MetricDEATH m(victim, killer, weaponHash, weaponDamageComponent, withMeleeWeapon, pvp, isKiller);
			//Append the metric.
			AppendMetric(m);
		}
	}
}

void CNetworkTelemetry::MakeRosBet(const CPed* gamer, const CPed* betOngamer, const u32 amount, const u32 activity, const float commission)
{
	//Instantiate the metric.
	MetricROS_BET m(gamer, betOngamer, amount, activity, commission);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::RaceCheckpoint(const u32 vehicleId
									  ,const u32 checkpointId
									  ,const u32 racePos
									  ,const u32 raceTime
									  ,const u32 checkpointTime)
{
	//Instantiate the metric.
	MetricPOST_RACE_CHECKPOINT m(vehicleId
								,checkpointId
								,racePos
								,raceTime
								,checkpointTime);

	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::PostMatchBlob(const u32 xp
									  ,const int cash
									  ,const u32 highestKillStreak
									  ,const u32 kills
									  ,const u32 deaths
									  ,const u32 suicides
									  ,const u32 rank
									  ,const int teamId
									  ,const int crewId
									  ,const u32 vehicleId
									  ,const int cashEarnedFromBets
									  ,const int cashAtStart
									  ,const int cashAtEnd
									  ,const int betWon
									  ,const int survivedWave)
{
	//Instantiate the metric.
	MetricPOST_MATCH_BLOB m(xp
		,cash
		,highestKillStreak
		,kills
		,deaths
		,suicides
		,rank
		,teamId
		,crewId
		,vehicleId
		,cashEarnedFromBets
		,cashAtStart
		,cashAtEnd
		,betWon
		,survivedWave);

	//Append the metric.
	AppendMetric(m);
}


/////////////////////////////////////////////////////////////
// FUNCTION: PlayerArrested
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::PlayerArrested()
{
	//Instantiate the metric.
	MetricARREST m;
	//Append the metric.
	AppendMetric(m);

	//Flush current telemetry buffer
	FlushTelemetry();
}

/////////////////////////////////////////////////////////////
// FUNCTION: MissionStarted
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::MissionStarted(const char* missionName, const u32 UNUSED_PARAM(variant), const u32 OUTPUT_ONLY(checkpoint), const bool replaying)
{
#if RSG_OUTPUT
	gnetAssertf(0 == sm_MissionStarted.GetHash(), "%s - Calling command PLAYSTATS_MISSION_STARTED for mission=%s, when a mission is already in progress for mission=%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionName, sm_MissionStarted.GetCStr());
	if( 0 == sm_MissionStarted.GetHash() )
#endif
	{
		gnetAssert(missionName);
		if (missionName)
		{
			sm_MissionStartedTime = fwTimer::GetSystemTimeInMilliseconds();
			sm_MissionStartedTimePaused = 0;
			sm_MissionCheckpointTime = 0;
			sm_MissionCheckpointTimePaused = 0;
			OUTPUT_ONLY( sm_MissionCheckpoint = 0; )
			sm_MissionStarted.SetFromString(missionName);

			//Setup new match id to collate metrics together
			s_MatchHistoryId = rlGetPosixTime();
			s_replayingMission = replaying;

#if RSG_OUTPUT
			MetricMISSION_STARTED m(missionName, checkpoint);
			AppendMetric(m);
#endif
			gnetVerify( FlushTelemetry( true ) );
		}
	}
}

void CNetworkTelemetry::MissionOver(const char* missionName, const u32 variant, const u32 checkpoint, const bool failed, const bool canceled, const bool skipped)
{
	gnetAssert(missionName);

#if RSG_OUTPUT
	gnetAssertf(0 < sm_MissionStarted.GetHash(), "%s - Calling command PLAYSTATS_MISSION_OVER for mission=%s, when a mission is not in progress.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionName);
	gnetAssertf(missionName && atStringHash( missionName ) == sm_MissionStarted.GetHash(), "%s - Calling command PLAYSTATS_MISSION_OVER for mission=%s, when the mission in progress is %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionName, sm_MissionStarted.GetCStr());
	if ( atStringHash( missionName ) == sm_MissionStarted.GetHash() )
#endif
	{
		if (missionName)
		{
			sm_MissionStarted.Clear();

			u32 timespent = 0;
			const u32 currTime = fwTimer::GetSystemTimeInMilliseconds();
			if (sm_MissionStartedTime < currTime)
			{
				timespent = currTime - sm_MissionStartedTime;

				if (timespent > sm_MissionStartedTimePaused)
				{
					timespent -= sm_MissionStartedTimePaused;
				}
			}
			sm_MissionStartedTime = currTime;
			sm_MissionStartedTimePaused = 0;

			//Instantiate the metric.
			MetricMISSION_OVER m(missionName, variant, checkpoint, failed, canceled, skipped, timespent, s_replayingMission);
			//Append the metric.
			AppendMetric(m);

			//Flush the buffer after this event because the odds are higher
			//that the player will power off the machine after completing
			//a mission.
			gnetVerify( FlushTelemetry( true ) );

			OUTPUT_ONLY( sm_MissionCheckpoint = 0; )

			if (!s_replayingMission && !failed && !canceled && !skipped)
			{
				//Instantiate player skills the metric.
				MetricCHARACTER_SKILLS skils(missionName, s_MatchHistoryId);
				//Append the metric.
				AppendMetric(skils);
			}

			//Clear Match id 
			s_MatchHistoryId = 0;
			s_replayingMission = false;
		}
	}
}

void CNetworkTelemetry::MissionCheckpoint(const char* missionName, const u32 variant, const u32 previousCheckpoint, const u32 checkpoint)
{
	gnetAssert(missionName);

#if RSG_OUTPUT
	gnetAssertf(0 < sm_MissionStarted.GetHash(), "%s - Calling command PLAYSTATS_MISSION_CHECKPOINT for mission=%s, when a mission is not in progress.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionName);
	gnetAssertf(atStringHash( missionName ) == sm_MissionStarted.GetHash(), "%s - Calling command PLAYSTATS_MISSION_CHECKPOINT for mission=%s, when the mission in progress is %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionName, sm_MissionStarted.GetCStr());
	if ( atStringHash( missionName ) == sm_MissionStarted.GetHash() )
#endif //RSG_OUTPUT
	{
		if (missionName)
		{
			u32 timespent = 0;
			const u32 currTime = fwTimer::GetSystemTimeInMilliseconds();
			if (sm_MissionCheckpointTime <= currTime)
			{
				if (sm_MissionCheckpointTime > 0)
				{
					timespent = currTime - sm_MissionCheckpointTime;
					if (gnetVerify(timespent >= sm_MissionCheckpointTimePaused))
					{
						timespent -= sm_MissionCheckpointTimePaused;
					}
				}
				else
				{
					timespent = currTime - sm_MissionStartedTime;
					if (gnetVerify(timespent >= sm_MissionStartedTimePaused))
					{
						timespent -= sm_MissionStartedTimePaused;
					}
				}
			}
			sm_MissionCheckpointTime = currTime;
			sm_MissionCheckpointTimePaused = 0;
			OUTPUT_ONLY( sm_MissionCheckpoint = checkpoint; )

			//Instantiate the metric.
			MetricMISSION_CHECKPOINT m(missionName, variant, previousCheckpoint, checkpoint, timespent);
			//Append the metric.
			AppendMetric(m);
		}
	}
}

void CNetworkTelemetry::RandomMissionDone(const char* missionName, const int outcome, const int timespent, const int attempts)
{
	gnetAssert(missionName);
	if (missionName)
	{
		//Instantiate the metric.
		MetricRANDOMMISSION_DONE m(missionName, outcome, timespent, attempts);
		//Append the metric.
		AppendMetric(m);
	}
}

/////////////////////////////////////////////////////////////
// FUNCTION: MultiPlayer Ambient missions
//
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::MultiplayerAmbientMissionCrateDrop(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const u32 weaponHash, const u32 otherItemsHash, const u32 enemiesKilled, const u32 (&_SpecialItemHashs)[MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS], bool _CollectedBodyArmour)
{
	//Instantiate the metric.
	MetricAMBIENT_MISSION_CRATEDROP m(ambientMissionId, xpEarned, cashEarned, weaponHash, otherItemsHash, enemiesKilled, _SpecialItemHashs, _CollectedBodyArmour);
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}

void CNetworkTelemetry::MultiplayerAmbientMissionCrateCreated(float X, float Y, float Z)
{
	//Instantiate the metric.
	MetricAMBIENT_MISSION_CRATE_CREATED m(X, Y, Z);
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}


void CNetworkTelemetry::MultiplayerAmbientMissionHoldUp(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const u32 shopkeepersKilled)
{
	//Instantiate the metric.
	MetricAMBIENT_MISSION_HOLD_UP m(ambientMissionId, xpEarned, cashEarned, shopkeepersKilled);
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}

void CNetworkTelemetry::MultiplayerAmbientMissionImportExport(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const int vehicleId)
{
	//Instantiate the metric.
	MetricAMBIENT_MISSION_IMP_EXP m(ambientMissionId, xpEarned, cashEarned, vehicleId);
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}

void CNetworkTelemetry::MultiplayerAmbientMissionSecurityVan(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned)
{
	//Instantiate the metric.
	MetricAMBIENT_MISSION_SECURITY_VAN m(ambientMissionId, xpEarned, cashEarned);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::MultiplayerAmbientMissionRaceToPoint(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const MetricAMBIENT_MISSION_RACE_TO_POINT::sRaceToPointInfo& rtopInfo)
{
	//Instantiate the metric.
	MetricAMBIENT_MISSION_RACE_TO_POINT m(ambientMissionId, xpEarned, cashEarned, rtopInfo);
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}

void CNetworkTelemetry::MultiplayerAmbientMissionProstitute(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const u32 cashSpent, const u32 numberOfServices, const bool playerWasTheProstitute)
{
	//Instantiate the metric.
	MetricAMBIENT_PROSTITUTES m(ambientMissionId, xpEarned, cashEarned, cashSpent, numberOfServices, playerWasTheProstitute);
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}

/////////////////////////////////////////////////////////////
// FUNCTION: LoadGame
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::LoadGame()
{
	//Instantiate the metric.
	MetricLOADGAME m;
	//Append the metric.
	AppendMetric(m);
}



/////////////////////////////////////////////////////////////
// FUNCTION: NewGame
//    
/////////////////////////////////////////////////////////////

// This gets called only when a new game is started (By selecting 'new game' from frontend or the first time the game starts up.
// It is also called when a network game starts.

// It is not called when a game is loaded or when the single player game restarts after a network game.

// The episode being played can be queried using: CGameLogic::GetCurrentEpisodeIndex

void CNetworkTelemetry::NewGame()
{
	//Flush current telemetry buffer
	FlushTelemetry();

    u8 macAddr[6];
	netHardware::GetMacAddress(macAddr);
    u64 spSessionId = atDataHash((const char*) macAddr, sizeof(macAddr));
    spSessionId = (spSessionId << 32) + sysTimer::GetTicks();

	//Clear Match History id
	s_MatchHistoryId = 0;
	s_MatchStartedTime = 0;
	s_replayingMission = false;
	sm_MatchStartedId.Clear();
	sm_RootContentId = 0;

    CNetworkTelemetry::SetSpSessionId(spSessionId);

	//Instantiate the metric.
	MetricNEWGAME m;
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::CutSceneSkipped(const u32 nameHash, const u32 duration)
{
	//Instantiate the metric.
	MetricCUTSCENE m(nameHash, duration, true);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::CutSceneEnded(const u32 nameHash, const u32 duration)
{
	//Instantiate the metric.
	MetricCUTSCENE m(nameHash, duration, false);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::ShopItem(const u32 itemHash, const u32 amountSpent, const u32 shopName, const u32 extraItemHash, const u32 colorHash)
{
	//Instantiate the metric.
	MetricShopping m(itemHash, amountSpent, shopName, extraItemHash, colorHash);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::WeatherChanged(const u32 prevIndex, const u32 newIndex)
{
	gnetAssertf(prevIndex!= (u32)-1, "invalid prevIndex, bump B*2167373");
	gnetAssertf(newIndex!= (u32)-1, "invalid newIndex, bump B*2167373");
	if (NetworkInterface::IsHost() && NetworkInterface::IsGameInProgress())
	{
		MetricWEATHER m(prevIndex, newIndex);
		CNetworkTelemetry::AppendMetric(m);
	}
}

void
CNetworkTelemetry::EnteredSoloSession(const u64 sessionId, const u64 sessionToken, const int gameMode, const eSoloSessionReason soloSessionReason, const eSessionVisibility sessionVisibility, const eSoloSessionSource soloSessionSource, const int userParam1, const int userParam2, const int userParam3)
{
	//Instantiate and append the metric.
	MetricENTERED_SOLO_SESSION m(sessionId, sessionToken, gameMode, soloSessionReason, sessionVisibility, soloSessionSource, userParam1, userParam2, userParam3);
	AppendMetric(m);
}

void
CNetworkTelemetry::StallDetectedMultiPlayer(const unsigned stallTimeMs, const unsigned networkTimeMs, const bool bIsTransitioning, const bool bIsLaunching, const bool bIsHost, const unsigned nTimeoutsPending, const unsigned sessionSize, const u64 sessionToken, const u64 sessionId)
{
	//Instantiate and append the metric.
	MetricSTALL_DETECTED m(stallTimeMs, networkTimeMs, bIsTransitioning, bIsLaunching, bIsHost, nTimeoutsPending, sessionSize, sessionToken, sessionId);
	AppendMetric(m);
}

void
CNetworkTelemetry::StallDetectedSinglePlayer(const unsigned stallTimeMs, const unsigned networkTimeMs)
{
	//Instantiate and append the metric.
	MetricSTALL_DETECTED m(stallTimeMs, networkTimeMs);
	AppendMetric(m);
}

void 
CNetworkTelemetry::NetworkBail(const sBailParameters bailParams,
				 			   const unsigned nSessionState,
				 			   const u64 sessionId,
				 			   const bool bIsHost,
				 			   const bool bJoiningViaMatchmaking,
				 			   const bool bJoiningViaInvite,
				 			   const unsigned nTime)
{
	//Instantiate the metric.
	MetricNETWORK_BAIL m(bailParams, nSessionState, sessionId, bIsHost, bJoiningViaMatchmaking, bJoiningViaInvite, nTime);
	//Append the metric.
	AppendMetric(m);
}

void 
CNetworkTelemetry::NetworkKicked(const u64 nSessionToken,
								 const u64 nSessionId,
								 const u32 nGameMode,
								 const u32 nSource,
								 const u32 nPlayers,
								 const u32 nSessionState, 
								 const u32 nTimeInSession,
								 const u32 nTimeSinceLastMigration,
								 const unsigned nNumComplaints,
								 const rlGamerHandle& hGamer)
{
	//Instantiate the metric.
	MetricNETWORK_KICKED m(nSessionToken, nSessionId, nGameMode, nSource, nPlayers, nSessionState, nTimeInSession, nTimeSinceLastMigration, nNumComplaints, hGamer);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::SessionHosted(const u64 sessionId,
						const u64 sessionToken,
						const u64 timeCreated,
                        const SessionPurpose sessionPurpose,
                        const unsigned numPubSlots,
                        const unsigned numPrivSlots,
						const unsigned matchmakingKey, 
						const u64 nUniqueMatchmakingID)
{
	//Cache the current session id.  It will be included in the header
    //of each submission.
    s_CurMpSessionId = sessionId;
	s_CurMpGameMode = (u8)sessionPurpose;
	s_CurMpNumPubSlots = (u8)numPubSlots;
	s_CurMpNumPrivSlots = (u8)numPrivSlots;
	//Clear Match History id
	s_MatchHistoryId = 0;
	s_MatchStartedTime = 0;
	s_replayingMission = false;
	ResetHasUsedVoiceChat();
	sm_MatchStartedId.Clear();
	sm_RootContentId = 0;

	//Instantiate the metric.
	MetricSESSION_HOSTED m(sessionPurpose == SessionPurpose::SP_Activity, matchmakingKey, nUniqueMatchmakingID, sessionToken, timeCreated);
	//Append the metric.
	AppendMetric(m);

	bool isInFM = CNetwork::IsGameInProgress() && !CNetwork::GetNetworkSession().IsActivitySession();
	if(isInFM)
	{
		s_closedFmSession = !CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) && CNetwork::GetNetworkSession().IsClosedSession(SessionType::ST_Physical);
		s_privateFmSession = !CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) && CNetwork::GetNetworkSession().IsPrivateSession(SessionType::ST_Physical);
	}
}

void
CNetworkTelemetry::SessionJoined(const u64 sessionId,
						const u64 sessionToken, 
						const u64 timeCreated,
                        const SessionPurpose sessionPurpose,
                        const unsigned numPubSlots,
                        const unsigned numPrivSlots,
						const unsigned matchmakingKey,
						const u64 nUniqueMatchmakingID)
{
    //Cache the current session id.  It will be included in the header
    //of each submission.
    s_CurMpSessionId = sessionId;
	s_CurMpGameMode = (u8)sessionPurpose;
	s_CurMpNumPubSlots = (u8)numPubSlots;
	s_CurMpNumPrivSlots = (u8)numPrivSlots;
	//Clear Match History id
	s_MatchHistoryId = 0;
	s_MatchStartedTime = 0;
	s_replayingMission = false;
	ResetHasUsedVoiceChat();
	sm_MatchStartedId.Clear();
	sm_RootContentId = 0;

	//Instantiate the metric.
	MetricSESSION_JOINED m(sessionPurpose == SessionPurpose::SP_Activity, matchmakingKey, nUniqueMatchmakingID, sessionToken, timeCreated);
	//Append the metric.
	AppendMetric(m);

	bool isInFM = CNetwork::IsGameInProgress() && !CNetwork::GetNetworkSession().IsActivitySession();
	if(isInFM)
	{
		s_closedFmSession = !CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) && CNetwork::GetNetworkSession().IsClosedSession(SessionType::ST_Physical);
		s_privateFmSession = !CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) && CNetwork::GetNetworkSession().IsPrivateSession(SessionType::ST_Physical);
	}
}

void
CNetworkTelemetry::SessionLeft(const u64 sessionId, const bool isTransitioning)
{
	// flush and send comms telemetry
	FlushCommsTelemetry();

	//Instantiate the metric.
	MetricSESSION_LEFT m(sessionId, isTransitioning);
	//Append the metric.
	AppendMetric(m);
	//Flush current telemetry buffer
	gnetVerify( FlushTelemetry( true ) );

    //Clear the current session id so it will no longer be included
    //in submission headers.
    s_CurMpSessionId = 0;
	s_CurMpGameMode = 0;
	s_CurMpNumPubSlots = 0;
	s_CurMpNumPrivSlots = 0;

	//Clear Match History id
	s_MatchHistoryId = 0;
	s_MatchStartedTime = 0;
	s_replayingMission = false;
	sm_MatchStartedId.Clear();
	sm_RootContentId = 0;
}

void
CNetworkTelemetry::SessionBecameHost(const u64 sessionId)
{
	//Instantiate the metric.
	MetricSESSION_BECAME_HOST m(sessionId);
	//Append the metric.
	AppendMetric(m);
}

void 
CNetworkTelemetry::InviteResult(
	const u64 sessionToken,
	const char* uniqueMatchId,
	const int gameMode,
	const unsigned inviteFlags,
	const unsigned inviteId,
	const unsigned inviteAction,
	const int inviteFrom,
	const int inviteType,
	const rlGamerHandle& inviterHandle,
	const bool isFriend)
{
	// instantiate the metric.
	MetricInviteResult m(
		sessionToken,
		uniqueMatchId,
		gameMode,
		inviteFlags,
		inviteId,
		inviteAction,
		inviteFrom,
		inviteType,
		inviterHandle,
		isFriend);

	// append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::SessionMatchStarted(const char* matchCreator, const char* uniqueMatchId, const u32 rootContentId, const u64 matchHistoryId, const MetricJOB_STARTED::sMatchStartInfo& info)
{
	if(gnetVerify(matchCreator) && gnetVerify(uniqueMatchId))
	{
		//Setup Match history ID used in the metric header.
		gnetAssertf(!s_MatchHistoryId, "MatchHistoryId still set from %s - Make sure you called PLAYSTATS_MATCH_ENDED before starting a new one.", s_MatchHistoryIdLastModified);
		s_MatchHistoryId = matchHistoryId;
#if !__NO_OUTPUT
		formatf(s_MatchHistoryIdLastModified, "[%d] SessionMatchStarted: %s - %s - %" I64FMT "d", fwTimer::GetFrameCount(), matchCreator, uniqueMatchId, matchHistoryId);
#endif // !__NO_OUTPUT

		s_MatchStartedTime = fwTimer::GetSystemTimeInMilliseconds();
		sm_MatchStartedId.SetFromString(uniqueMatchId);
		sm_RootContentId = rootContentId;

		MetricJOB_STARTED m(uniqueMatchId, info);
		//Append the metric.
		AppendMetric(m);

		gnetAssert(s_CurMpSessionId != 0);
	}
}

bool CNetworkTelemetry::IsActivityInProgress()
{
	return (NetworkInterface::IsInFreeMode() && s_MatchHistoryId > 0);
}

void CNetworkTelemetry::SessionMatchEnded(const unsigned matchType, const char* matchCreator, const char* uniqueMatchId, const bool isTeamDeathmatch, const int raceType, const bool leftInProgress)
{
	if(gnetVerify(matchCreator) && gnetVerify(uniqueMatchId))
	{
		gnetAssert(s_CurMpSessionId != 0);

		//Instantiate the metric.
		MetricSESSION_MATCH_ENDED m(matchType, fwTimer::GetSystemTimeInMilliseconds()-s_MatchStartedTime, matchCreator, uniqueMatchId, isTeamDeathmatch, raceType, leftInProgress);
		//Append the metric.
		AppendMetric(m);

		//Flush the buffer before this metric due to the header match history ID.
		gnetVerify( FlushTelemetry( true ) );

		//Clear Match History id
		s_previousMatchHistoryId = s_MatchHistoryId;
		s_MatchHistoryId = 0;
		s_MatchStartedTime = 0;
		s_replayingMission = false;
		sm_MatchStartedId.Clear();
		sm_RootContentId = 0;
	}
}

void CNetworkTelemetry::SessionMatchEnd( )
{
	gnetAssert(NetworkInterface::IsGameInProgress());
	gnetAssert(s_CurMpSessionId != 0);

	SCPRESENCEUTIL.Script_SetPresenceAttributeInt(ATSTRINGHASH("mp_curr_gamemode",0x53938A49), -1);
	CNetwork::GetLeaderboardMgr().GetLeaderboardWriteMgr().MatchEnded();

	//Clear Match History id 
	s_previousMatchHistoryId = s_MatchHistoryId;
	s_MatchHistoryId   = 0;
	s_MatchStartedTime = 0;
	s_replayingMission = false;
	sm_MatchStartedId.Clear();
	sm_RootContentId = 0;
}

void CNetworkTelemetry::PlayerWantedLevel()
{
	//Instantiate the metric.
	MetricWANTED_LEVEL m;
	//Append the metric.
	AppendMetric(m);
}


/////////////////////////////////////////////////////////////
// FUNCTION: PlayerSpawn
//    
/////////////////////////////////////////////////////////////

void CNetworkTelemetry::PlayerSpawn(int location, int reason)
{
	//Setup data for MetricPLAYER_INJURED
	m_SendMetricInjury  = true;
	m_PlayerSpawnTime   = rlGetPosixTime();
	//@@: location CNETWORKTELEMETRY_PLAYERSPAWN
	m_PlayerSpawnCoords = CGameWorld::FindLocalPlayerCoors();

	//Instantiate the metric.
	MetricSPAWN m(ms_currentSpawnLocationOption, location, reason);
	//Append the metric.
	RAGE_SEC_POP_REACTION
	AppendMetric(m);

	//Player died
	FlushTelemetry();
}

void CNetworkTelemetry::PlayerInjured()
{
	if (m_SendMetricInjury)
	{
		//Instantiate the metric.
		MetricPLAYER_INJURED m(m_PlayerSpawnTime, m_PlayerSpawnCoords);
		//Append the metric.
		AppendMetric(m);

		m_SendMetricInjury = false;
	}
}

void CNetworkTelemetry::StartGame()
{
	static bool s_started = false;

	if (!s_started && NetworkInterface::HasValidRosCredentials() && NetworkInterface::IsOnlineRos())
	{
		s_started = true;
		rlCreateUUID(&sm_playtimeSessionId);
		//Instantiate the metric.
		MetricSTART_GAME m;
		//Append the metric.
		AppendMetric(m);
	}

	static bool s_startedPlayerCheck = false;
	if (!s_startedPlayerCheck && NetworkInterface::HasValidRosCredentials() && NetworkInterface::IsOnlineRos() && StatsInterface::GetStatsPlayerModelIsMultiplayer() && MoneyInterface::IsSavegameLoaded())
	{
		s_startedPlayerCheck = true;
		MetricPLAYER_CHECK metricPlayerCheck;
		AppendMetric(metricPlayerCheck);
	}
}

void CNetworkTelemetry::ExitGame()
{
	//Instantiate the metric.
	MetricEXIT_GAME m;
	//Append the metric.
	AppendMetric(m);

	//Flush the buffer after this event because the odds are higher
	//that the player will power off the machine after completing
	//a mission.
	FlushTelemetry();
}

void
CNetworkTelemetry::CheatApplied(const char* cheatString)
{
	//Instantiate the metric.
	MetricCHEAT m(cheatString);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::EmergencySvcsCalled(const s32 mi, const Vector3& destination)
{
	//Instantiate the metric.
	MetricEMERGENCY_SVCS m(mi, destination);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::AcquiredHiddenPackage(const int id)
{
	//Instantiate the metric.
	MetricACQUIRED_HIDDEN_PACKAGE m(id);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::WebsiteVisited(const int id, const int timespent)
{
	//Instantiate the metric.
	MetricWEBSITE_VISITED m(id, timespent);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::FriendsActivityDone(int character, int outcome)
{
	//Instantiate the metric.
	MetricFRIEND_ACTIVITY_DONE m(character, outcome);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::AcquiredWeapon(const u32 weaponHash)
{
	//Instantiate the metric.
	MetricACQUIRED_WEAPON m(weaponHash);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::ChangeWeaponMod(const u32 weaponHash, const int modIdFrom, const int modIdTo)
{
	//Instantiate the metric.
	MetricWEAPON_MOD_CHANGE m(weaponHash, modIdFrom, modIdTo);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::ChangePlayerProp(const u32 modelHash, const int  position, const int  newPropIndex, const int  newTextIndex)
{
	//Instantiate the metric.
	MetricPROP_CHANGE m(modelHash, position, newPropIndex, newTextIndex);
	//Append the metric.
	AppendMetric(m);
}
void
CNetworkTelemetry::ChangedPlayerCloth(const u32 modelHash, const int componentID, const int drawableID, const int textureID, const int paletteID)
{
	//Instantiate the metric.
	MetricCLOTH_CHANGE m(modelHash, componentID, drawableID, textureID, paletteID); 
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::EarnedAchievement(const unsigned id)
{
	//Instantiate the metric.
	MetricEARNED_ACHIEVEMENT m(id);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::AudTrackTagged(const unsigned id)
{
	//Instantiate the metric.
	MetricAUD_TRACK_TAGGED m(id);
	//Append the metric.
	AppendMetric(m);
}

void
CNetworkTelemetry::RetuneToStation(const unsigned id)
{
	sm_AudPlayerPrevTunedStation = sm_AudPlayerTunedStation;
	sm_AudPlayerTunedStation = id;
}

void 
CNetworkTelemetry::AppendMetricTvShow(const u32 playListId, const u32 tvShowId, const u32 channelId, const u32 timeWatched)
{
	//Instantiate the metric.
	MetricTV_SHOW m(playListId, tvShowId, channelId, timeWatched);
	//Append the metric.
	AppendMetric(m);
}

void 
CNetworkTelemetry::AppendMetricMem(const u32 hashname, const u32 newvalue)
{
	//Instantiate the metric.
	MetricMEM m(hashname, newvalue);
	//Append the metric.
	AppendMetric(m);

	//Flush current telemetry buffer
	FlushTelemetry( true );
}

void 
CNetworkTelemetry::AppendDebuggerDetect(const u32 newvalue)
{
	//Instantiate the metric.
	MetricDEBUGGER m(newvalue);
	//Append the metric.
	AppendMetric(m);
}

void 
CNetworkTelemetry::AppendGarageTamper(const u32 newvalue)
{
	//Instantiate the metric.
	MetricGARAGETAMPER m(newvalue);
	//Append the metric.
	AppendMetric(m);
}

void 
CNetworkTelemetry::AppendSecurityMetric(MetricLAST_VEH& metric)
{
	bool sendTelemetry = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_EVENT_FOR_SCRIPT", 0x3e93ef3b), true);

	if(sendTelemetry)
	{
#if USE_RAGESEC
#if RSG_OUTPUT && __ASSERT
		const int stringHashTamper = (int)ATSTRINGHASH("TAMPER_WITH_PLAYER_INDEX", 0x7B7A3F83);
		gnetAssertf(stringHashTamper != metric.m_fields[0], "Add class A bug metric 'LAST_VEH' - 'TAMPER_WITH_PLAYER_INDEX'.");
#endif // RSG_OUTPUT && __ASSERT

		char buff[256] = {0};
		formatf(buff, 256,"%u,%u,%u,%s,%s", metric.m_session, metric.m_fields[0], metric.m_fields[1], metric.m_FromGH, metric.m_ToGH);
		const u32 hashVal = atStringHash((const char *)buff);

		if(!rageSecReportCache::GetInstance()->HasBeenReported(rageSecReportCache::eReportCacheBucket::RCB_GAMESERVER, hashVal))
		{
			gnetDebug3("AppendSecurityMetric: hval:0x%04x ", hashVal);

			AppendMetric(metric);	
			rageSecReportCache::GetInstance()->AddToReported(rageSecReportCache::eReportCacheBucket::RCB_GAMESERVER, hashVal);
		}
#else // USE_RAGESEC
		AppendMetric(metric);
#endif // USE_RAGESEC
	}
	else
	{
		gnetError("FAILED AppendSecurityMetric.");
	}
}

void 
CNetworkTelemetry::AppendInfoMetric(const u32 version, const u32 address, const u32 crc)
{
	//Instantiate the metric.
	MetricCODECRCFAIL m(version, address, crc);
	//Append the metric.
#if USE_RAGESEC
#if !__NO_OUTPUT
	const unsigned curTime = sysTimer::GetSystemMsTime();
#endif
	u32 hashVal = version ^ address ^ crc ^ (version == address ? 0 : 1);
	if(!rageSecReportCache::GetInstance()->HasBeenReported(rageSecReportCache::eReportCacheBucket::RCB_TELEMETRY, hashVal))
	{

#if !__NO_OUTPUT
		gnetDebug3("AppendInfoMetric:Time:%s %u crc:0x%04x hval:0x%04x ", version == address ? "SP" : "MP", sysTimer::GetSystemMsTime() - curTime, crc, hashVal);
#endif

		AppendMetric(m);	
		rageSecReportCache::GetInstance()->AddToReported(rageSecReportCache::eReportCacheBucket::RCB_TELEMETRY, hashVal);
	}
	else
	{
#if !__NO_OUTPUT
		gnetDebug3("AppendInfoMetric:ALREADY-REPORTED:Time:%s %u crc:0x%04x hval:0x%04x ", version == address ? "SP" : "MP", sysTimer::GetSystemMsTime() - curTime, crc, hashVal);
#endif
	}
#else // USE_RAGESEC
	AppendMetric(m);
#endif // USE_RAGESEC
}

void 
	CNetworkTelemetry::AppendTamperMetric(int param)
{
	//Instantiate the metric.
	MetricTamper m(param);
#if USE_RAGESEC
	if(!rageSecReportCache::GetInstance()->HasBeenReported(rageSecReportCache::eReportCacheBucket::RCB_TELEMETRY, param))
	{
		//Append the metric.
		AppendMetric(m);
		rageSecReportCache::GetInstance()->AddToReported(rageSecReportCache::eReportCacheBucket::RCB_TELEMETRY, param);
	}
#else // USE_RAGESEC
	AppendMetric(m);
#endif // USE_RAGESEC
}

void CNetworkTelemetry::GetIsClosedAndPrivateForLastFreemode(bool& closed, bool& privateSlot)
{
	closed = s_closedFmSession;
	privateSlot = s_privateFmSession;
}

void CNetworkTelemetry::RecordGameChatMessage( const rlGamerId& OUTPUT_ONLY(gamerId), const char* text, bool bTeamOnly)
{
	MetricComms::CumulativeTextCommsInfo& cci = sm_CumulativeTextCommsInfo[bTeamOnly ? MetricComms::GAME_CHAT_TEAM_CUMULATIVE : MetricComms::GAME_CHAT_CUMULATIVE];
	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	if (sm_ChatIdExpiryTime > 0)
	{
		if (cci.m_CurrentChatId == 0 ||
			((curTime > cci.m_lastChatTime) && (curTime - cci.m_lastChatTime) > sm_ChatIdExpiryTime))
		{
			const unsigned netTime = NetworkInterface::GetNetworkTime();
			cci.m_CurrentChatId = CNetworkTelemetry::GenerateNewChatID(netTime, NetworkInterface::GetLocalPlayer()->GetGamerInfo().GetGamerHandle().ToUniqueId());
			gnetDebug3("CNetworkTelemetry::RecordGameChatMessage - COMMS - Player (%" I64FMT "u) starting a new %s text chat ID: %u [time: %u]", gamerId.GetId(), bTeamOnly ? "team" : "", cci.m_CurrentChatId, curTime);
		}
	}
#if RSG_BANK
	else
	{
		gnetDebug3("CNetworkTelemetry::RecordPhoneTextMessage - COMMS - Failed to create new game chat ChatID. sm_ChatIdExpiryTime == 0");
	}
#endif // RSG_BANK

	// Last chat time value updated in RecordMessage
	cci.RecordMessage(text);
}

void CNetworkTelemetry::RecordPhoneTextMessage( const rlGamerHandle& receiverGamerHandle, const char* text )
{
	CNetGamePlayer* pReceiverPlayer = NetworkInterface::GetPlayerFromGamerHandle(receiverGamerHandle);
	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	if (sm_ChatIdExpiryTime > 0)
	{
		if (sm_CurrentPhoneChatId == 0 ||
			(curTime > sm_TimeOfLastPhoneChat && curTime - sm_TimeOfLastPhoneChat > sm_ChatIdExpiryTime))
		{
			const unsigned netTime = NetworkInterface::GetNetworkTime();

			sm_CurrentPhoneChatId =  CNetworkTelemetry::GenerateNewChatID(netTime, NetworkInterface::GetLocalPlayer()->GetGamerInfo().GetGamerHandle().ToUniqueId());
			gnetDebug3("CNetworkTelemetry::RecordPhoneTextMessage - COMMS - Local Player (%s) starting a new phone chat ID: %u [time: %u]", NetworkInterface::GetLocalPlayer()->GetLogName(), sm_CurrentPhoneChatId, curTime);
		}
#if RSG_BANK
		else
		{
			gnetDebug3("CNetworkTelemetry::RecordPhoneTextMessage - COMMS - Phone chat keep alive time updated (ID: %u): %u [old: %u]", sm_CurrentPhoneChatId, curTime, sm_TimeOfLastPhoneChat);
		}
#endif // RSG_BANK

		sm_TimeOfLastPhoneChat = curTime;
	}
#if RSG_BANK
	else
	{
		gnetDebug3("CNetworkTelemetry::RecordPhoneTextMessage - COMMS - Failed to create new phone text ChatID. sm_ChatIdExpiryTime == 0");
	}
#endif // RSG_BANK

	bool commsTelemetryKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_COMMS_TELEMETRY", 0xC1356D3F), false);

	if ( !commsTelemetryKillswitch )
	{
		MetricComms m(
			MetricComms::PHONE_TEXT,
			MetricComms::CHAT_TYPE_PRIVATE,
			sm_CurrentPhoneChatId,
			CNetworkTelemetry::GetCurMpSessionId(),
			(int)NetworkInterface::GetSessionTypeForTelemetry(),
			1,
			(int)strlen(text),
			0,
			!NetworkInterface::GetVoice().IsEnabled(),
			pReceiverPlayer ? pReceiverPlayer->GetPlayerAccountId() : 0,
			GetCurrentPublicContentId(),
			IsInLobby() );
		AppendMetric(m);
	}
}

void CNetworkTelemetry::RecordPhoneEmailMessage(const rlGamerHandle& receiverGamerHandle, int textLength)
{
	const u32 curTime = sysTimer::GetSystemMsTime() | 0x01;

	if (sm_ChatIdExpiryTime > 0)
	{
		if (sm_CurrentEmailChatId == 0 ||
			(curTime > sm_TimeOfLastEmailChat && curTime - sm_TimeOfLastEmailChat > sm_ChatIdExpiryTime))
		{
			const unsigned netTime = NetworkInterface::GetNetworkTime();

			const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();

			// for emails use receiver instead of sender to generate IDs (if not valid because there is no specific recipient, use the local sender)
			u64 playerId = 0;
			if (receiverGamerHandle.IsValid())
				playerId = receiverGamerHandle.ToUniqueId();
			else if(pLocalPlayer != nullptr)
				playerId = pLocalPlayer->GetGamerInfo().GetGamerHandle().ToUniqueId();

			// For emails use receiver instead of sender to generate IDs
			sm_CurrentEmailChatId =  CNetworkTelemetry::GenerateNewChatID(netTime, playerId);
			gnetDebug3("CNetworkTelemetry::RecordPhoneEmailMessage - COMMS - Email sent by local player (%s). Starting a new email chat ID: %u [time: %u]", pLocalPlayer ? pLocalPlayer->GetLogName() : "Invalid", sm_CurrentEmailChatId, curTime);
		}
#if RSG_OUTPUT
		else
		{
			gnetDebug3("CNetworkTelemetry::RecordPhoneEmailMessage - COMMS - Email chat keep alive time updated (ID: %u): %u [old: %u]", sm_CurrentEmailChatId, curTime, sm_TimeOfLastEmailChat);
		}
#endif // RSG_OUTPUT
		sm_TimeOfLastEmailChat = curTime;
	}
#if RSG_OUTPUT
	else
	{
		gnetDebug3("CNetworkTelemetry::RecordPhoneEmailMessage - COMMS - Failed to create new email ChatID. sm_ChatIdExpiryTime == 0");
	}
#endif // RSG_OUTPUT

	bool commsTelemetryKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_COMMS_TELEMETRY", 0xC1356D3F), false);

	if ( !commsTelemetryKillswitch )
	{
		// this would only be valid if the receiver is in session with the local player
		CNetGamePlayer* pReceiverPlayer = NetworkInterface::GetPlayerFromGamerHandle(receiverGamerHandle);

		MetricComms m(
			MetricComms::EMAIL_TEXT,
			MetricComms::CHAT_TYPE_PRIVATE,
			sm_CurrentEmailChatId,
			CNetworkTelemetry::GetCurMpSessionId(),
			(int)NetworkInterface::GetSessionTypeForTelemetry(),
			1,
			textLength,
			0,
			!NetworkInterface::GetVoice().IsEnabled(),
			pReceiverPlayer ? pReceiverPlayer->GetPlayerAccountId() : 0,
			GetCurrentPublicContentId(),
			IsInLobby(),
			&receiverGamerHandle);
		AppendMetric(m);
	}
}

void CNetworkTelemetry::OnPhoneTextMessageReceived(const rlGamerHandle& senderGamerGandle, const char* UNUSED_PARAM(text))
{
	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	if (sm_CurrentPhoneChatId == 0 ||
		((curTime > sm_TimeOfLastPhoneChat) && curTime - sm_TimeOfLastPhoneChat > sm_ChatIdExpiryTime))
	{
		CNetGamePlayer* pSenderPlayer = NetworkInterface::GetPlayerFromGamerHandle(senderGamerGandle);
		if (pSenderPlayer && sm_ChatIdExpiryTime > 0)
		{
			const unsigned netTime = NetworkInterface::GetNetworkTime();
			sm_CurrentPhoneChatId =  CNetworkTelemetry::GenerateNewChatID(netTime, pSenderPlayer->GetGamerInfo().GetGamerHandle().ToUniqueId());
			gnetDebug3("CNetworkTelemetry::OnPhoneTextMessageReceived - COMMS - Player (%s) starting a new phone chat ID: %u [time: %u]", pSenderPlayer->GetLogName(), sm_CurrentPhoneChatId, curTime);
		}
#if RSG_BANK
		else
		{
			gnetDebug3("CNetworkTelemetry::OnNewEmailMessagesReceived - COMMS - Failed to create new email chatID. chatIdExpiryTime: %u, senderPlayer: %s", sm_ChatIdExpiryTime, pSenderPlayer ? pSenderPlayer->GetLogName() : "nullptr");
		}
#endif // RSG_BANK
	}
#if RSG_BANK
	else
	{
		gnetDebug3("CNetworkTelemetry::OnPhoneTextMessageReceived - COMMS - Phone chat keep alive time updated (ID:%u ): %u [old: %u]", sm_CurrentPhoneChatId, curTime, sm_TimeOfLastPhoneChat);
	}
#endif // RSG_BANK

	sm_TimeOfLastPhoneChat = curTime;
}

void CNetworkTelemetry::OnNewEmailMessagesReceived(const rlGamerHandle& receiverGamerGandle)
{
	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	if (sm_CurrentEmailChatId == 0 ||
		(curTime > sm_TimeOfLastEmailChat && (curTime - sm_TimeOfLastEmailChat > sm_ChatIdExpiryTime)))
	{
		CNetGamePlayer* pReceiverPlayer = NetworkInterface::GetPlayerFromGamerHandle(receiverGamerGandle);
		if (pReceiverPlayer && sm_ChatIdExpiryTime > 0)
		{
			// For emails use receiver instead of sender to generate IDs
			const unsigned netTime = NetworkInterface::GetNetworkTime();
			sm_CurrentEmailChatId =  CNetworkTelemetry::GenerateNewChatID(netTime, pReceiverPlayer->GetGamerInfo().GetGamerHandle().ToUniqueId());
			gnetDebug3("CNetworkTelemetry::OnNewEmailMessagesReceived - COMMS - Email sent to %s. Starting a new email chat ID: %u [time: %u]", pReceiverPlayer->GetLogName(), sm_CurrentEmailChatId, curTime);
		}
#if RSG_BANK
		else
		{
			gnetDebug3("CNetworkTelemetry::OnNewEmailMessagesReceived - COMMS - Failed to create new email chatID. chatIdExpiryTime: %u, receivePlayer: %s", sm_ChatIdExpiryTime, pReceiverPlayer ? pReceiverPlayer->GetLogName() : "nullptr");
		}
#endif // RSG_BANK
	}
#if RSG_BANK
	else
	{
		gnetDebug3("CNetworkTelemetry::OnNewEmailMessagesReceived - COMMS - Email chat keep alive time updated (ID: %u): %u [old: %u]", sm_CurrentEmailChatId, curTime, sm_TimeOfLastEmailChat);
	}
#endif // RSG_BANK

	sm_TimeOfLastEmailChat = curTime;
}

void 
CNetworkTelemetry::EnterIngameStore()
{
	//Instantiate the metric.
	MetricINGAMESTOREACTION m(MetricINGAMESTOREACTION::ENTER_STORE, 0);
	//Append the metric.
	AppendMetric(m);
}

void 
CNetworkTelemetry::ExitIngameStore( bool didCheckout, u32 msSinceInputEnabled )
{
	//Instantiate the metric.
	MetricINGAMESTOREACTION::eStoreTelemetryActions action;

	if (didCheckout)
	{
		action = MetricINGAMESTOREACTION::EXIT_STORE_CHECKOUT;
	}
	else
	{
		action = MetricINGAMESTOREACTION::EXIT_STORE_NO_CHECKOUT;
	}
	// We exited the store on console without a purchase, send the metric here
	AppendMetric(MetricPURCHASE_FLOW());
	// Reset the saved information about this purchase
	GetPurchaseMetricInformation().Reset();
	FlushTelemetry();

	MetricINGAMESTOREACTION m(action, msSinceInputEnabled);
	//Append the metric.
	AppendMetric(m);
}

void CNetworkTelemetry::LogMetricWarning(const char* szFormat, ...)
{
	char szBuffer[256];
	va_list vArgs;
	va_start(vArgs, szFormat);
	vsprintf(szBuffer, szFormat, vArgs);
	va_end(vArgs);

	gnetWarning("%s", szBuffer);
}

void CNetworkTelemetry::LogMetricDebug(const char* szFormat, ...)
{
	char szBuffer[256];
	va_list vArgs;
	va_start(vArgs, szFormat);
	vsprintf(szBuffer, szFormat, vArgs);
	va_end(vArgs);

	gnetDebug2("%s", szBuffer);
}

void CNetworkTelemetry::ResetHasUsedVoiceChat()
{
	sm_HasUsedVoiceChatSinceLastTime=false;	
	sm_ShouldSendVoiceChatUsage=true;	
	Displayf("[VoiceChatTelemetry] Reset");
}


class MetricRelayUsage : public MetricPlayStat
{
	RL_DECLARE_METRIC(RELAY_USAGE, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_DEBUG1);

	bool m_isTransitioning;
	u64 m_sessionId;
	u32 m_GeoLocBucket;
	char m_Ip[rage::netSocketAddress::MAX_STRING_BUF_SIZE];
	char m_RelayIp[rage::netSocketAddress::MAX_STRING_BUF_SIZE];
	rage::netNatType m_Nat;
	bool m_UsedRelay;

public:

	MetricRelayUsage(bool isTransitioning, u64 sessionId, u32 geoLocBucket, char ip[rage::netSocketAddress::MAX_STRING_BUF_SIZE], char relayIp[rage::netSocketAddress::MAX_STRING_BUF_SIZE], rage::netNatType nat, bool usedRelay) 
		: m_isTransitioning(isTransitioning)
		, m_sessionId(sessionId)
		, m_GeoLocBucket(geoLocBucket)
		, m_Nat(nat)
		, m_UsedRelay(usedRelay)
	{
		gnetAssertf(sessionId, "SessionId is 0");
		strcpy_s(m_Ip, rage::netSocketAddress::MAX_STRING_BUF_SIZE, ip);
		strcpy_s(m_RelayIp, rage::netSocketAddress::MAX_STRING_BUF_SIZE, relayIp);
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result;
		result = MetricPlayStat::Write(rw)
			&& rw->WriteBool("tr", m_isTransitioning)
			// Vertica only supports signed 64-bit ints
			&& rw->WriteInt64("sid", static_cast<s64>(m_sessionId))
			&& rw->WriteUns("glb", m_GeoLocBucket)
			&& rw->WriteString("ip", m_Ip)
			&& rw->WriteString("rip", m_RelayIp)
			&& rw->WriteInt("nat", m_Nat)
			&& rw->WriteBool("r", m_UsedRelay);
		return result;
	}
};

void CNetworkTelemetry::OnPeerPlayerConnected(bool isTransition, const u64 sessionId, const rage::snEventAddedGamer* pEvent, rage::netNatType natType, const rage::netAddress& addr)
{
	if(CNetworkTelemetry::sm_AllowRelayUsageTelemetry)
	{
		// the local player infos will be sent with host/join metrics
		if(pEvent->m_GamerInfo.IsRemote())
		{
			rlRosGeoLocInfo geoLoc;
			rlRos::GetGeoLocInfo(&geoLoc);

			char ip[rage::netSocketAddress::MAX_STRING_BUF_SIZE] = {};
			if(addr.IsDirectAddr() || addr.IsRelayServerAddr())
			{
				addr.GetTargetAddress().FormatAttemptIpV4(ip, rage::netSocketAddress::MAX_STRING_BUF_SIZE, false);
			}

			char relayIp[rage::netSocketAddress::MAX_STRING_BUF_SIZE] = {};
			if(addr.IsRelayServerAddr())
			{
				addr.GetProxyAddress().FormatAttemptIpV4(relayIp, rage::netSocketAddress::MAX_STRING_BUF_SIZE, false);
			}

			rage::netNatType nat = natType;
			bool usedRelay = addr.IsRelayServerAddr();

			MetricRelayUsage m(isTransition, sessionId, geoLoc.m_RegionCode, ip, relayIp, nat, usedRelay);
			AppendMetric(m);
		}
	}
}

void CNetworkTelemetry::AppendCreatorUsage(u32 timeSpent, u32 numberOfTries, u32 numberOfEarlyQuits, bool isCreating, u32 missionId)
{
	MetricFREEMODE_CREATOR_USAGE m(timeSpent, numberOfTries, numberOfEarlyQuits, isCreating, missionId);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendXPLoss(int xpBefore, int xpAfter)
{
	MetricXP_LOSS m(xpBefore, xpAfter);
	AppendMetric(m, true); // this metric is deferred, no need to flush
}

void CNetworkTelemetry::AppendHeistSaveCheat(int hashValue, int otherValue)
{
	MetricHEIST_SAVE_CHEAT m(hashValue, otherValue);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendCashCreated(u64 uid, const rlGamerHandle& player, int amount, int hash)
{
	MetricCASH_CREATED m(uid, player, amount, hash);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendStarterPackApplied(int item, int amount)
{
	MetricSTARTERPACK_APPLIED m(item, amount);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendSinglePlayerSaveMigration(bool success, u32 savePosixTime, float completionPercentage, u32 lastCompletedMissionId, unsigned sizeOfSavegameDataBuffer)
{
	MetricSP_SAVE_MIGRATION m(success, savePosixTime, completionPercentage, lastCompletedMissionId, sizeOfSavegameDataBuffer);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendDirectorMode(const MetricDIRECTOR_MODE::VideoClipMetrics& values)
{
	MetricDIRECTOR_MODE m(values);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendVideoEditorSaved(int clipCount, int* audioHash, int audioHashCount, u64 projectLength, u64 timeSpent)
{
	MetricVIDEO_EDITOR_SAVE m(clipCount, audioHash, audioHashCount, projectLength, timeSpent);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendVideoEditorUploaded(u64 videoLength, int m_scScore)
{
	MetricVIDEO_EDITOR_UPLOAD m(videoLength, m_scScore);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendEventTriggerOveruse(Vector3 pos, u32 rank, u32 triggerCount, u32 triggerDur, u32 totalCount, atFixedArray<MetricEventTriggerOveruse::EventData, MetricEventTriggerOveruse::MAX_EVENTS_REPORTED>& triggeredData)
{
	MetricEventTriggerOveruse m(pos, rank, triggerCount, triggerDur, totalCount, triggeredData, (int)NetworkInterface::GetSessionTypeForTelemetry(), CNetworkTelemetry::GetMatchStartedId(), CNetworkTelemetry::sm_RootContentId);
	AppendMetric(m);
}

void CNetworkTelemetry::SetAllowRelayUsageTelemetry(const bool allowRelayUsageTelemetry)
{
	if(sm_AllowRelayUsageTelemetry != allowRelayUsageTelemetry)
	{
		gnetDebug1("SetAllowRelayUsageTelemetry :: %s", allowRelayUsageTelemetry ? "true" : "false");
		sm_AllowRelayUsageTelemetry = allowRelayUsageTelemetry;
	}
}

void CNetworkTelemetry::SetAllowPCHardwareTelemetry(const bool allowPCHardwareTelemetry)
{
	if(sm_AllowPCHardwareTelemetry != allowPCHardwareTelemetry)
	{
		gnetDebug1("SetAllowPCHardwareTelemetry :: %s", allowPCHardwareTelemetry ? "true" : "false");
		sm_AllowPCHardwareTelemetry = allowPCHardwareTelemetry;
	}
}

#if RSG_PC
// PC hardware metrics aren't actually reported over telemetry any longer.
// Instead we write out a json file to the cloud for each player.
class MetricPC_SETTINGS_BASE : public MetricPlayStat
{
	RL_DECLARE_METRIC(PC_SETTINGS_BASE, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPC_SETTINGS_BASE(bool withGuid = false)
	{
		m_uid = (withGuid) ? CNetworkTelemetry::GetBenchmarkGuid() : 0;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw)
			&& rw->WriteUns64("uid", m_uid);
	}
private:

	u64 m_uid;
};

class MetricPCHARDWARE_OS : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(HARDWARE_OS, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCHARDWARE_OS(bool withGuid = false)
		: MetricPC_SETTINGS_BASE(withGuid)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPC_SETTINGS_BASE::Write(rw)
			&& rw->WriteString("RawVers", CSystemInfo::GetRawWindowsVersion()) 
			&& rw->WriteString("Vers", CSystemInfo::GetWindowsString())
			&& rw->WriteString("Lang", CSystemInfo::GetLanguageString());
	}
};

class MetricPCHARDWARE_CPU : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(HARDWARE_CPU, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCHARDWARE_CPU(bool withGuid = false)
		: MetricPC_SETTINGS_BASE(withGuid)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPC_SETTINGS_BASE::Write(rw)
			&& rw->WriteString("Str", CSystemInfo::GetProcessorString())
			&& rw->WriteString("Id", CSystemInfo::GetProcessorIdentifier())
			&& rw->WriteString("Speed", CSystemInfo::GetProcessorSpeed())
			&& rw->WriteInt("NumPhysicalCores", CSystemInfo::GetNumberOfPhysicalCores())
			&& rw->WriteInt("NumLocalProcessors", CSystemInfo::GetNumberOfCPUS());
	}
};

class MetricPCHARDWARE_GPU : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(HARDWARE_GPU, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCHARDWARE_GPU(bool withGuid = false)
		: MetricPC_SETTINGS_BASE(withGuid)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPC_SETTINGS_BASE::Write(rw)
			&& rw->WriteString("Desc", CSystemInfo::GetVideoCardDescription())
			&& rw->WriteString("Mem", CSystemInfo::GetVideoCardRam())
			&& rw->WriteString("Drv", CSystemInfo::GetVideoCardDriverVersion())
			&& rw->WriteString("DX", CSystemInfo::GetDXVersion());
	}
};

class MetricPCHARDWARE_MOBO : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(HARDWARE_MOBO, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCHARDWARE_MOBO(bool withGuid = false)
		: MetricPC_SETTINGS_BASE(withGuid)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPC_SETTINGS_BASE::Write(rw)
			&& rw->WriteString("Manu", CSystemInfo::GetMBManufacturer())
			&& rw->WriteString("Prod", CSystemInfo::GetMBProduct())
			&& rw->WriteString("Vers", CSystemInfo::GetBIOSVersion())
			&& rw->WriteString("Vend", CSystemInfo::GetBIOSVendor());
	}
};

class MetricPCHARDWARE_MEM : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(HARDWARE_MEM, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCHARDWARE_MEM(bool withGuid = false)
		: MetricPC_SETTINGS_BASE(withGuid)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPC_SETTINGS_BASE::Write(rw);
		result = result && rw->WriteUns64("RAM_MB", CSystemInfo::GetTotalPhysicalRAM() / (1024 * 1024));
		result = result && rw->WriteUns64("HDD_Tot_MB", CSystemInfo::GetTotalPhysicalHDD() / (1024 * 1024));
		result = result && rw->WriteUns64("HDD_Avail_MB", CSystemInfo::GetAvailablePhysicalHDD() / (1024 * 1024));
		result = result && rw->WriteBool("SSD", CSystemInfo::GetIsSSD());
		result = result && rw->WriteUns64("ss", CSystemInfo::GetSpindleSpeed());
		result = result && rw->WriteUns64("mt", CSystemInfo::GetMediaType());
		result = result && rw->WriteUns64("bt", CSystemInfo::GetBusType());

		char* fn = CSystemInfo::GetFriendlyName();
		if (fn && fn[0] != 0)
		{
			result = result && rw->WriteString("fn", CSystemInfo::GetFriendlyName());
		}

		char* did = CSystemInfo::GetDeviceId();
		if (did && did[0] != 0)
		{
			result = result && rw->WriteString("did", CSystemInfo::GetDeviceId());
		}

		return result;
	}
};

class MetricPCSETTINGS : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(PCSETTINGS, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCSETTINGS(bool withGuid = false, bool shortnames=false)
		: MetricPC_SETTINGS_BASE(withGuid)
		, m_shortNames(shortnames)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		const Settings& settings = CSettingsManager::GetInstance().GetSettings();

		return MetricPC_SETTINGS_BASE::Write(rw)
			// Settings version
			&& rw->WriteInt(m_shortNames ? "sv" : "SettingsVersion", settings.m_version)

			// Config source
			&& rw->WriteInt(m_shortNames ? "cs" : "ConfigSource", settings.m_configSource)

			// Graphics Settings
			&& rw->WriteInt(m_shortNames ? "t" : "Tessellation", settings.m_graphics.m_Tessellation)
			&& rw->WriteFloat(m_shortNames ? "ls" : "LodScale", settings.m_graphics.m_LodScale)
			&& rw->WriteFloat(m_shortNames ? "pl" : "PedLodBias", settings.m_graphics.m_PedLodBias)
			&& rw->WriteFloat(m_shortNames ? "vl" : "VehicleLodBias", settings.m_graphics.m_VehicleLodBias)
			&& rw->WriteInt(m_shortNames ? "sq" : "ShadowQuality", settings.m_graphics.m_ShadowQuality)
			&& rw->WriteInt(m_shortNames ? "rq" : "ReflectionQuality", settings.m_graphics.m_ReflectionQuality)
			&& rw->WriteInt(m_shortNames ? "rm" : "ReflectionMSAA", settings.m_graphics.m_ReflectionMSAA)
			&& rw->WriteInt(m_shortNames ? "ssao" : "SSAO", settings.m_graphics.m_SSAO)
			&& rw->WriteInt(m_shortNames ? "af" : "AnisotropicFiltering", settings.m_graphics.m_AnisotropicFiltering)
			&& rw->WriteInt(m_shortNames ? "ms" : "MSAA", settings.m_graphics.m_MSAA)
			&& rw->WriteInt(m_shortNames ? "msf" : "MSAAFragments", settings.m_graphics.m_MSAAFragments)
			&& rw->WriteInt(m_shortNames ? "msq" : "MSAAQuality", settings.m_graphics.m_MSAAQuality)
			&& rw->WriteInt(m_shortNames ? "tq" : "TextureQuality", settings.m_graphics.m_TextureQuality)
			&& rw->WriteInt(m_shortNames ? "pq" : "ParticleQuality", settings.m_graphics.m_ParticleQuality)
			&& rw->WriteInt(m_shortNames ? "wq" : "WaterQuality", settings.m_graphics.m_WaterQuality)
			&& rw->WriteInt(m_shortNames ? "gq" : "GrassQuality", settings.m_graphics.m_GrassQuality)
			&& rw->WriteInt(m_shortNames ? "shq" : "ShaderQuality", settings.m_graphics.m_ShaderQuality)
			&& rw->WriteInt(m_shortNames ? "ss" : "Shadow_SoftShadows", settings.m_graphics.m_Shadow_SoftShadows)
			&& rw->WriteBool(m_shortNames ? "sps" : "Shadow_ParticleShadows", settings.m_graphics.m_Shadow_ParticleShadows)
			&& rw->WriteFloat(m_shortNames ? "sd" : "Shadow_Distance", settings.m_graphics.m_Shadow_Distance)
			&& rw->WriteBool(m_shortNames ? "sls" : "Shadow_LongShadows", settings.m_graphics.m_Shadow_LongShadows)
			&& rw->WriteFloat(m_shortNames ? "szs" : "Shadow_SplitZStart", settings.m_graphics.m_Shadow_SplitZStart)
			&& rw->WriteFloat(m_shortNames ? "sze" : "Shadow_SplitZEnd", settings.m_graphics.m_Shadow_SplitZEnd)
			&& rw->WriteFloat(m_shortNames ? "saw" : "Shadow_aircraftExpWeight", settings.m_graphics.m_Shadow_aircraftExpWeight)
			&& rw->WriteBool(m_shortNames ? "rmb" : "Reflection_MipBlur", settings.m_graphics.m_Reflection_MipBlur)
			&& rw->WriteBool(m_shortNames ? "fxa" : "FXAA_Enabled", settings.m_graphics.m_FXAA_Enabled)
			// TODO Add TAA_Quality
			&& rw->WriteBool(m_shortNames ? "lfv" : "Lighting_FogVolumes", settings.m_graphics.m_Lighting_FogVolumes)
			&& rw->WriteBool(m_shortNames ? "ssa" : "Shader_SSA", settings.m_graphics.m_Shader_SSA)
			&& rw->WriteFloat(m_shortNames ? "cd" : "CityDensity", settings.m_graphics.m_CityDensity)
			&& rw->WriteInt(m_shortNames ? "dx" : "DX_Version", settings.m_graphics.m_DX_Version)
			&& rw->WriteInt(m_shortNames ? "pfx" : "PostFX", settings.m_graphics.m_PostFX)

			// Video Settings
			&& rw->WriteInt(m_shortNames ? "ai" : "AdapterIndex", settings.m_video.m_AdapterIndex)
			&& rw->WriteInt(m_shortNames ? "oi" : "OutputIndex", settings.m_video.m_OutputIndex)
			&& rw->WriteInt(m_shortNames ? "sw" : "ScreenWidth", settings.m_video.m_ScreenWidth)
			&& rw->WriteInt(m_shortNames ? "sh" : "ScreenHeight", settings.m_video.m_ScreenHeight)
			&& rw->WriteInt(m_shortNames ? "rr" : "RefreshRate", settings.m_video.m_RefreshRate)
			&& rw->WriteInt(m_shortNames ? "w" : "Windowed", settings.m_video.m_Windowed)
			&& rw->WriteInt(m_shortNames ? "vs" : "VSync", settings.m_video.m_VSync)
			&& rw->WriteInt(m_shortNames ? "st" : "Stereo", settings.m_video.m_Stereo)
			&& rw->WriteFloat(m_shortNames ? "c" : "Convergence", settings.m_video.m_Convergence)
			&& rw->WriteFloat(m_shortNames ? "s" : "Separation", settings.m_video.m_Separation)
			&& rw->WriteInt(m_shortNames ? "p" : "PauseOnFocusLoss", settings.m_video.m_PauseOnFocusLoss)
			&& rw->WriteInt(m_shortNames ? "a" : "AspectRatio", settings.m_video.m_AspectRatio);
	}
	bool m_shortNames;
};

bool GetTitleVersionInfo(char (&version)[32])
{
	memset(version, 0, sizeof(version));

	char fileName[_MAX_PATH] = {0};
	bool success = GetModuleFileName(NULL, fileName, sizeof(fileName)) != 0;

	if(success == false)
	{
		return false;
	}

	DWORD handle = 0;
	DWORD size = GetFileVersionInfoSize(fileName, &handle);
	BYTE* versionInfo = rage_new BYTE[size];
	if(!GetFileVersionInfo(fileName, handle, size, versionInfo))
	{
		delete[] versionInfo;
		return false;
	}

	// we have version information
	UINT len = 0;
	VS_FIXEDFILEINFO* vsfi = NULL;
	VerQueryValue(versionInfo, "\\", (void**)&vsfi, &len);

	int aVersion[4] = {0};
	aVersion[0] = HIWORD(vsfi->dwFileVersionMS);
	aVersion[1] = LOWORD(vsfi->dwFileVersionMS);
	aVersion[2] = HIWORD(vsfi->dwFileVersionLS);
	aVersion[3] = LOWORD(vsfi->dwFileVersionLS);

	delete[] versionInfo;

	formatf(version, "%d.%d.%d.%d", aVersion[0], aVersion[1], aVersion[2], aVersion[3]);
	return true;
}

class MetricPCHARDWARE_OTHER : public MetricPC_SETTINGS_BASE
{
	RL_DECLARE_METRIC(PC_OTHER, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);

public:

	MetricPCHARDWARE_OTHER(bool withGuid = false)
		: MetricPC_SETTINGS_BASE(withGuid)
	{
		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		char gameVersion[32] = {0};
		GetTitleVersionInfo(gameVersion);

		return MetricPC_SETTINGS_BASE::Write(rw)
			&& rw->WriteUns64("Timestamp", rlGetPosixTime())
			&& rw->WriteString("GameVersion", gameVersion)
			&& rw->WriteString("SCVersion", g_rlPc.GetSocialClubVersion());
	}
};

void __GamerHardwareWriteHelper(RsonWriter* rw, const char* groupName, const rlMetric& metric, int& valueCursor)
{
	if(rw->Begin(groupName, &valueCursor))
	{
		const unsigned len = rw->Length();
		bool wroteIt = metric.Write(rw);

		if(!AssertVerify(wroteIt))
		{
			//The metric failed to write itself.
			rw->Undo(valueCursor);
		}
		else if(!rw->HasError() && rw->Length() == len)
		{
			//Metric didn't have a value item.
			//Remove empty value items so we don't get "value={}"
			//in the submission.
			rw->Undo(valueCursor);
		}
		else
		{
			rw->End();                              //value
		}
	}
}

void CNetworkTelemetry::PostGamerHardwareStats(bool toTelemetry/*=true*/)
{
	static const int BOUNCE_BUFFER_SIZE = 4*1024;
	static netStatus s_PCHardwareCloudPostNetStatus;

	// only allow one report per game launch
	static bool reportSent = false;
	if(reportSent)
	{
		return;
	}
	
	if(CNetworkTelemetry::sm_AllowPCHardwareTelemetry == false)
	{
		return;
	}

	// check if report has been sent within last two weeks
	CProfileSettings& profileSettings = CProfileSettings::GetInstance();
	gnetAssert(profileSettings.AreSettingsValid());
	const u64 currentPosixTime = rlGetPosixTime();

	u64 low32 = 0;
	if(profileSettings.Exists(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_LOW32))
	{
		low32 = static_cast< u64 >( profileSettings.GetInt(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_LOW32) );
	}

	u64 high32 = 0;
	if(profileSettings.Exists(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_HIGH32))
	{
		high32 = static_cast< u64 >( profileSettings.GetInt(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_HIGH32) );
	}

	u64 lastHardwareStatsUploadTime = high32 << 32;
	lastHardwareStatsUploadTime = lastHardwareStatsUploadTime | low32;

	const u64 REPORT_INTERVAL_IN_SECONDS = 60 * 60 * 24 * 14; // one report every 2 weeks
	if ((currentPosixTime - lastHardwareStatsUploadTime) < REPORT_INTERVAL_IN_SECONDS)
	{
		gnetDebug1("CNetworkTelemetry::PostGamerHardwareStats() - Stats were posted %llu seconds ago", (currentPosixTime - lastHardwareStatsUploadTime));
		return;
	}

	CSystemInfo::RefreshSystemInfo();

	reportSent = true;

	MetricPCHARDWARE_OS os;
	MetricPCHARDWARE_CPU cpu;
	MetricPCHARDWARE_GPU gpu;
	MetricPCHARDWARE_MOBO mobo;
	MetricPCHARDWARE_MEM mem;
	MetricPCSETTINGS settings(false, true);
	MetricPCHARDWARE_OTHER other;

	if(toTelemetry)
	{
		// Simply append the metrics and flush
		AppendMetric(os);
		AppendMetric(cpu);
		AppendMetric(gpu);
		AppendMetric(mobo);
		AppendMetric(mem);
		AppendMetric(settings);
		AppendMetric(other);
		FlushTelemetry();

		profileSettings.Set(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_LOW32,  (int)(currentPosixTime & 0xFFFFFFFF));
		profileSettings.Set(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_HIGH32, (int)(currentPosixTime >> 32));
		profileSettings.WriteNow(true);
	}
	else
	{
		// Send to the cloud
		RsonWriter rw;
		char bounceBuf[BOUNCE_BUFFER_SIZE];

		rw.Init(bounceBuf, sizeof(bounceBuf), RSON_FORMAT_JSON);

		int valueCursor = 0;
		rw.Begin(NULL, &valueCursor);
		__GamerHardwareWriteHelper(&rw, "OS",	os,		valueCursor);
		__GamerHardwareWriteHelper(&rw, "CPU",	cpu,	valueCursor);
		__GamerHardwareWriteHelper(&rw, "GPU",	gpu,	valueCursor);
		__GamerHardwareWriteHelper(&rw, "MOBO",	mobo,	valueCursor);
		__GamerHardwareWriteHelper(&rw, "MEM",	mem,	valueCursor);
		__GamerHardwareWriteHelper(&rw, "SETTINGS", settings, valueCursor);
		__GamerHardwareWriteHelper(&rw, "OTHER", other, valueCursor);
		rw.End();

		if (PARAM_statsDumpPCSettingsFile.Get())
		{
			Displayf("Writing X:/PcSettings.json");
			fiStream* fileStream = fiStream::Create("X:/PcSettings.json");
			if(fileStream)
			{
				fileStream->Write(rw.ToString(), rw.Length());
				fileStream->Flush();
				fileStream->Close();
			}
		}

		int localIndex = NetworkInterface::GetLocalGamerIndex();
		if(localIndex >= 0)
		{
			char path[256];
			formatf(path, "%s/hwspecs/pcsettings.json", g_rlTitleId->m_RosTitleId.GetTitleName());
			AssertVerify(rlCloud::PostMemberFile(localIndex, RL_CLOUD_ONLINE_SERVICE_NATIVE, path, (const void*)rw.ToString(), rw.Length(), rlCloud::POST_REPLACE, NULL, 0, &s_PCHardwareCloudPostNetStatus));

			// save time of last report upload
			profileSettings.Set(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_LOW32,  (int)(currentPosixTime & 0xFFFFFFFF));
			profileSettings.Set(CProfileSettings::PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_HIGH32, (int)(currentPosixTime >> 32));
			profileSettings.WriteNow(true);
			gnetDebug1("CNetworkTelemetry::PostGamerHardwareStats() - Stats posted at %llu POSIX time", currentPosixTime);
		}
	}
}

void CNetworkTelemetry::AppendBenchmarkMetrics(const atArray<EndUserBenchmarks::fpsResult>& values)
{
	GenerateBenchmarkGuid();

	CSystemInfo::RefreshSystemInfo();

	MetricPCHARDWARE_OS os(true);
	MetricPCHARDWARE_CPU cpu(true);
	MetricPCHARDWARE_GPU gpu(true);
	MetricPCHARDWARE_MOBO mobo(true);
	MetricPCHARDWARE_MEM mem(true);
	MetricPCSETTINGS settings(true, true);
	MetricPCHARDWARE_OTHER other(true);
	MetricBENCHMARK_FPS benchmark(values);
	MetricBENCHMARK_P benchmarkP(values);

	AppendMetric(os);
	AppendMetric(cpu);
	AppendMetric(gpu);
	AppendMetric(mobo);
	AppendMetric(mem);
	AppendMetric(settings);
	AppendMetric(other);
	AppendMetric(benchmark);
	AppendMetric(benchmarkP);
}

u64 CNetworkTelemetry::GetBenchmarkGuid()
{
	gnetAssertf(m_benchmarkUid, "Benchmark Uid is not set yet");
	return m_benchmarkUid;
}

void CNetworkTelemetry::GenerateBenchmarkGuid()
{
	rlCreateUUID(&m_benchmarkUid);
}

void CNetworkTelemetry::GetPurchaseFlowTelemetry(char *telemetry, const unsigned bufSize)
{
	RsonWriter rw;
	rw.Init(telemetry, bufSize, RSON_FORMAT_JSON);
	bool success = rw.Begin(NULL, NULL);

	if(NetworkInterface::IsGameInProgress()) 
	{
		GetPurchaseMetricInformation().SaveCurrentCash();
		// Write purchase flow telemetry
		struct BinComputer
		{
			static int ComputeBin(s64 cash)  // Compute price bin according to B*2281172
			{
				if(cash<=-7999999) return -6;
				if(cash<=-3499999) return -5;
				if(cash<=-1249999) return -4;
				if(cash<=-499999) return -3;
				if(cash<=-199999) return -2;
				if(cash<=-99999) return -1;
				if(cash<=0) return 0;
				if(cash<=99999) return 1;
				if(cash<=199999) return 2;
				if(cash<=499999) return 3;
				if(cash<=1249999) return 4;
				if(cash<=3499999) return 5;
				if(cash<=7999999) return 6;
				return 7; 
			}
		};

		success = success && rw.WriteUns("l", sm_purchaseMetricInformation.GetFromLocation());
		success = success && rw.WriteUns("sid", sm_purchaseMetricInformation.m_storeId);
		success = success && rw.WriteUns("liid", sm_purchaseMetricInformation.m_lastItemViewed);
		success = success && rw.WriteInt("lip", BinComputer::ComputeBin(sm_purchaseMetricInformation.m_lastItemViewedPrice));
		success = success && rw.WriteInt("b", BinComputer::ComputeBin(MoneyInterface::GetVCBankBalance()));
		s64 deficit = MoneyInterface::GetVCBankBalance()-sm_purchaseMetricInformation.m_lastItemViewedPrice;
		success = success && rw.WriteInt("d", BinComputer::ComputeBin(deficit));
	}

	success = success && rw.End();

	if(!success)
	{
		telemetry[0] = '\0';
	}

	// Also send metrics through the regular telemetry here
	AppendMetric(MetricPURCHASE_FLOW());
	GetPurchaseMetricInformation().Reset();
	FlushTelemetry();
}

#endif // RSG_PC

// FPS measurement

enum FpsCaptureMode
{
	FPSCM_NONE,
	FPSCM_SOLO,
	FPSCM_MP,
	FPSCM_DIRECTOR
};
enum FpsCaptureSituation
{
	FPSCS_NONE,
	FPSCS_INGAME,
	FPSCS_MISSION,
	FPSCS_TRANSITION,
	FPSCS_CUTSCENE,
	FPSCS_PAUSEMENU
};

#if !__NO_OUTPUT

static const char* sm_FpsCaptureModeString[] = 
{
	"FPSCM_NONE",
	"FPSCM_SOLO",
	"FPSCM_MP",
	"FPSCM_DIRECTOR"
};
static const char* sm_FpsCaptureSituationString[] = 
{ 
	"FPSCS_NONE",
	"FPSCS_INGAME",
	"FPSCS_MISSION",
	"FPSCS_TRANSITION",
	"FPSCS_CUTSCENE",
	"FPSCS_PAUSEMENU"
};

#endif // !__NO_OUTPUT


static FpsCaptureSituation sm_captureSituation = FPSCS_NONE;
static FpsCaptureMode sm_captureMode = FPSCM_NONE;
static s32 sm_region = 0;
static u32 sm_interiorHashKey = 0;

class MetricINGAME_FPS : public MetricPlayStat
{
	RL_DECLARE_METRIC(INGAME_FPS, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_HIGH_VOLUME);

public:

	MetricINGAME_FPS(framesPerSecondResult &result, FpsCaptureMode mode, FpsCaptureSituation situation, s32 region) : MetricPlayStat()
		, m_mode(mode)
		, m_situation(situation)
		, m_region(region)
	{
		m_min = result.min;
		m_avg = result.average;
		m_std = result.std;

#if RSG_GEN9
		m_graphicsMode = CSettingsManager::GetInstance().GetPriority();
		m_graphicsMode = (m_graphicsMode == SP_DEFAULT) ? CSettingsManager::GetInstance().GetDefaultPriority() : m_graphicsMode;
#endif
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			&& rw->WriteInt("m", (int) m_mode)
			&& rw->WriteInt("s", (int) m_situation)
			&& rw->WriteInt("r", m_region)
			&& rw->WriteUns("i", sm_interiorHashKey)
			&& rw->WriteFloat("min", m_min)
			&& rw->WriteFloat("avg", m_avg)
			&& rw->WriteFloat("std", m_std)
#if RSG_GEN9
			&& rw->WriteInt("pm", m_graphicsMode)
#endif
			;
	}

	float					m_min;
	float					m_avg;
	float					m_std;
	FpsCaptureSituation		m_situation;
	FpsCaptureMode			m_mode;
	s32						m_region;
#if RSG_GEN9
	eSettingsPriority		m_graphicsMode;
#endif
};


static MetricsCapture::SampledValue<float> ms_FramesPerSecond;

void FlushFpsResults(FpsCaptureSituation situation, FpsCaptureMode mode, s32 region)
{
	// Send the sample results
	if(ms_FramesPerSecond.Min() != FLT_MAX) // Only if we have results
	{
		framesPerSecondResult result;
		result.average = ms_FramesPerSecond.Mean();
		result.min = ms_FramesPerSecond.Min();
		result.max = ms_FramesPerSecond.Max();
		result.std = ms_FramesPerSecond.StandardDeviation();

		MetricINGAME_FPS m(result, mode, situation, region);
		CNetworkTelemetry::AppendMetric(m);

		ms_FramesPerSecond.Reset();
	}
}

void CNetworkTelemetry::UpdateFramesPerSecondResult()
{
	if(!s_sendInGameFpsMetrics || s_metricMaxFpsSamples<=0)
		return;

	FpsCaptureSituation previousSituation = sm_captureSituation;
	FpsCaptureMode previousMode = sm_captureMode;
	s32 previousRegion = sm_region;


	if (CTheScripts::GetIsInDirectorMode())
	{
		sm_captureMode = FPSCM_DIRECTOR;
	}
	else if(NetworkInterface::IsGameInProgress() || NetworkInterface::IsTransitionActive())
	{
		sm_captureMode = FPSCM_MP;

		if(NetworkInterface::IsInFreeMode())
			sm_captureSituation = FPSCS_INGAME;
		if(NetworkInterface::IsTransitionActive())
			sm_captureSituation = FPSCS_TRANSITION;
		if(NetworkInterface::IsActivitySession())
			sm_captureSituation = FPSCS_MISSION;
	}
	else
	{
		sm_captureMode = FPSCM_SOLO;
		sm_captureSituation = FPSCS_INGAME;
	}
	

	if(NetworkInterface::IsInMPCutscene() || CutSceneManager::GetInstance()->IsRunning())
		sm_captureSituation = FPSCS_CUTSCENE;

	if(CPauseMenu::IsActive())
		sm_captureSituation = FPSCS_PAUSEMENU;


	// Only compute the region/interior hash when relevant, otherwise set it to invalid 
	sm_interiorHashKey = 0;
	sm_region = -1;
	if(sm_captureSituation == FPSCS_INGAME || sm_captureSituation == FPSCS_MISSION)
	{
		CPed* playerPed = FindPlayerPed();
		if (playerPed)
		{
			Vec3V coords = playerPed->GetTransform().GetPosition();
			s32 localRegion = CPathFind::FindRegionForCoors(coords.GetXf(), coords.GetYf());
			sm_region = localRegion;
		}

		if(CPortal::IsInteriorScene())
		{
			CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
			if(pIntInst && pIntInst->GetMloModelInfo())
			{
				sm_interiorHashKey = pIntInst->GetMloModelInfo()->GetHashKey();
			}
		}
	}


	if( (previousSituation != sm_captureSituation && previousSituation != FPSCS_NONE) 
	 || (previousMode != sm_captureMode && previousMode != FPSCM_NONE)  )
	{
		gnetDebug1("CNetworkTelemetry::UpdateFramesPerSecondResult() - Flushing capture (%d samples) as we moved from %s:%s to %s:%s", 
					ms_FramesPerSecond.Samples(), 
					sm_FpsCaptureModeString[previousMode], sm_FpsCaptureSituationString[previousSituation],
					sm_FpsCaptureModeString[sm_captureMode], sm_FpsCaptureSituationString[sm_captureSituation]);
		FlushFpsResults(previousSituation, previousMode, sm_region);
	}
	else if(previousRegion != sm_region && previousRegion != 0 && sm_region != -1)
	{
		// Only flush region changes if we have enough samples (consider 10% of s_metricMaxFpsSamples enough)
		if(ms_FramesPerSecond.Samples() >= s_metricMaxFpsSamples / 10.0f )
		{
			gnetDebug1("CNetworkTelemetry::UpdateFramesPerSecondResult() - Flushing capture (%d samples) as we moved from Region %d to %d", 
				ms_FramesPerSecond.Samples(),
				previousRegion, sm_region);
			FlushFpsResults(sm_captureSituation, sm_captureMode, previousRegion);
		}
	}

	float fps = 1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep());
	ms_FramesPerSecond.AddSample(fps);

	if(ms_FramesPerSecond.Samples() >= s_metricMaxFpsSamples)
	{
		// Send the sample results
		gnetDebug1("CNetworkTelemetry::UpdateFramesPerSecondResult() - Enough samples, flushing capture %s:%s (region %d)", 
			sm_FpsCaptureModeString[sm_captureMode], sm_FpsCaptureSituationString[sm_captureSituation], sm_region);

		FlushFpsResults(sm_captureSituation, sm_captureMode, sm_region);
	}
}


////////////////////////////////////////////////////////////////////////// WALK MODE METRIC

enum WalkMode
{
	WM_NONE,
	WM_WALKING,
	WM_RUNNING,
	WM_SPRINTING,
};


class MetricWALKMODE : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(WALKMODE, TELEMETRY_CHANNEL_POSSESSIONS, LOGLEVEL_VERYLOW_PRIORITY);

public:

	explicit MetricWALKMODE(const WalkMode walkMode)
		: m_walkMode(walkMode)
	{
		m_isInMP = NetworkInterface::IsGameInProgress();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw)
			&& rw->WriteUns("w", m_walkMode)
			&& rw->WriteBool("m", m_isInMP);
	}

private:

	WalkMode m_walkMode;
	bool m_isInMP;
};

void CNetworkTelemetry::UpdateOnFootMovementTracking()
{
	if (!Tunables::IsInstantiated())
		return;

	// The sampling rate is under tunable, value should be between 0 and 100 - 0 means no one is sending the metric
	static float s_spSampleRate = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ONFOOTMOV_SP_SAMPLERATE", 0x312F0EFD), 0.0f);
	static float s_mpSampleRate = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ONFOOTMOV_MP_SAMPLERATE", 0x55C76455), 0.0f);

	static float s_walkingDelayTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MOVE_METRIC_WALKING_DELAY", 0x9E0AA1E8), 0.0f);
	static float s_runningDelayTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MOVE_METRIC_RUNNING_DELAY", 0xAE89E369), 0.0f);
	static float s_sprintingDelayTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MOVE_METRIC_SPRINTING_DELAY", 0xF78C204C), 0.0f);
	static float s_noMovementDelayTime = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MOVE_METRIC_NONE_DELAY", 0xFF93EA47), 0.0f);

	// Local dice roll to see if we want to be part of the sampling population
	static float s_spDiceRoll = -1.0f;
	static float s_mpDiceRoll = -1.0f;
	if (s_spDiceRoll < 0.0f || s_mpDiceRoll < 0.0f)
	{
		mthRandom rand((int)fwTimer::GetSystemTimeInMilliseconds());
		s_spDiceRoll = rand.GetRanged(0.0f, 100.0f);
		s_mpDiceRoll = rand.GetRanged(0.0f, 100.0f);
	}

	bool shouldSendMetric = s_spSampleRate > 0.0f || s_mpSampleRate > 0.0f;
	shouldSendMetric &= NetworkInterface::IsGameInProgress() ? s_mpDiceRoll < s_mpSampleRate : s_spDiceRoll < s_spSampleRate;

	if (!shouldSendMetric)
		return;

	static float s_timer = -1.0f;
	static WalkMode s_wm = WM_NONE;
	static WalkMode s_previousWm = WM_NONE;
	const CPed* localPed = CGameWorld::FindLocalPlayer();
	if (localPed && localPed->GetMotionData() && !localPed->GetIsSwimming())
	{
		if(s_timer >= 0.0f)
		{
			s_timer += fwTimer::GetTimeStep();
		}

		bool sendMetric = false;
		if (localPed->GetMotionData()->GetIsWalking())
		{
			s_wm = WM_WALKING;
			sendMetric = s_timer >= s_walkingDelayTime;
		}
		else if (localPed->GetMotionData()->GetIsRunning())
		{
			s_wm = WM_RUNNING;
			sendMetric = s_timer >= s_runningDelayTime;
		}
		else if (localPed->GetMotionData()->GetIsSprinting())
		{
			s_wm = WM_SPRINTING;
			sendMetric = s_timer >= s_sprintingDelayTime;
		}
		else
		{
			s_wm = WM_NONE;
			sendMetric = s_timer >= s_noMovementDelayTime;
		}
		
		if (s_wm != s_previousWm)
		{
			s_previousWm = s_wm;
			s_timer = 0.0f;
		}

		if(sendMetric)
		{
			MetricWALKMODE m(s_wm);
			APPEND_METRIC(m);
			s_timer = -1.0f;
		}
	}
	else
	{
		s_timer = 0.0f;
		s_wm = WM_NONE;
		s_previousWm = WM_NONE;
	}
}

//////////////////////////////////////////////////////////////////////////

static float s_objectReassignFailedDiceRoll = -1.0f;
static float s_objectReassignFailedSampleRate = 0.0f;
static float s_objectReassignDiceRoll = -1.0f;
static float s_objectReassignSampleRate = 0.0f;

static u32 s_lastNegotiationStartTime = 0;
static float s_restartTotal = 0;
static float s_timeTotal = 0;
static int s_maxDuration = 0;
static int s_sampleCount = 0;
static u32 s_reassignMetricTimer = 0;
static int s_reassignMetricPeriod = -1;

void CNetworkTelemetry::AppendObjectReassignFailed(const u32 negotiationStartTime, const int restartCount, const int ambObjCount, const int scrObjCount)
{
	if (s_objectReassignFailedDiceRoll < 0.0f)
	{
		s_objectReassignFailedSampleRate = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_REASSIGNMENT_FAILED_SAMPLE_RATE", 0xC31007A0), 0.0f);
		mthRandom rand((int)fwTimer::GetSystemTimeInMilliseconds());
		s_objectReassignDiceRoll = rand.GetRanged(0.0f, 100.0f);
		gnetDebug1("Object Reassign failed telemetry (%f %%) dice roll (0, 100) = %f   -> %s", s_objectReassignFailedSampleRate, s_objectReassignFailedDiceRoll, (s_objectReassignFailedDiceRoll <= s_objectReassignFailedSampleRate) ? "succeed" : "failed");
	}
	if(s_objectReassignFailedDiceRoll > s_objectReassignFailedSampleRate)
		return;

	if (s_lastNegotiationStartTime == negotiationStartTime)
		return;

	s_lastNegotiationStartTime = negotiationStartTime;

	MetricOBJECT_REASSIGN_FAIL m(CNetwork::GetNetworkSession().GetSessionMemberCount(), restartCount, ambObjCount, scrObjCount);
	AppendMetric(m);
}

void CNetworkTelemetry::AppendObjectReassignInfo()
{
	if (s_objectReassignDiceRoll < 0.0f)
	{
		s_objectReassignSampleRate = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_REASSIGNMENT_SAMPLE_RATE", 0x3C87FC35), 0.0f);
		mthRandom rand((int)fwTimer::GetSystemTimeInMilliseconds());
		s_objectReassignDiceRoll = rand.GetRanged(0.0f, 100.0f);
		gnetDebug1("Object Reassign telemetry (%f %%) dice roll (0, 100) = %f   -> %s", s_objectReassignSampleRate, s_objectReassignDiceRoll, (s_objectReassignDiceRoll <= s_objectReassignSampleRate) ? "succeed" : "failed");
	}
	if (s_objectReassignDiceRoll > s_objectReassignSampleRate)
		return;

	const int playersInvolved = NetworkInterface::GetObjectManager().GetReassignMgr().GetReassignmentPlayersInvolvedCount();

	// We dont send the metric if we're the last one in the session
	if (playersInvolved <= 0)
	{
		gnetDebug3("CNetworkTelemetry::AppendObjectReassignInfo - Skipped, no more than 1 player involved in the reassign (%d)", playersInvolved);
		return;
	}

	s_restartTotal += NetworkInterface::GetObjectManager().GetReassignMgr().GetRestartNumberForTelemetry();
	u32 reassignTime = NetworkInterface::GetObjectManager().GetReassignMgr().GetReassignmentRunningTimeMs();
	if (reassignTime > s_maxDuration)
	{
		s_maxDuration = reassignTime;
	}
	s_timeTotal += reassignTime;
	s_sampleCount++;

	static int s_maxSamples = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_REASSIGNMENT_METRIC_MAX_SAMPLES", 0x66D3E09B), 0);
	s_reassignMetricPeriod = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_REASSIGNMENT_METRIC_PERIOD", 0x2FC1EEA), -1);
	u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
	if ((s_maxSamples> 0 && s_sampleCount > s_maxSamples) || (s_reassignMetricPeriod > 0 && s_reassignMetricTimer + s_reassignMetricPeriod < currentTime))
	{
		MetricOBJECT_REASSIGN_INFO m(playersInvolved, s_sampleCount, s_restartTotal / s_sampleCount, s_timeTotal / s_sampleCount, s_maxDuration);
		AppendMetric(m);

		s_restartTotal = 0;
		s_timeTotal = 0;
		s_maxDuration = 0;
		s_sampleCount = 0;
		s_reassignMetricTimer = currentTime;
	}
}

int CNetworkTelemetry::GetCurrentSpawnLocationOption()
{
	return ms_currentSpawnLocationOption;
}

void CNetworkTelemetry::SetCurrentSpawnLocationOption(const int newSpawnLocationOption)
{
	ms_currentSpawnLocationOption = newSpawnLocationOption;
}

const char* CNetworkTelemetry::GetCurrentPublicContentId()
{
	return ms_publicContentId;
}

void CNetworkTelemetry::SetCurrentPublicContentId(const char* newPublicContentId)
{
	safecpy(ms_publicContentId, newPublicContentId, COUNTOF(ms_publicContentId));
}

const ChatOption CNetworkTelemetry::GetCurrentChatOption()
{
	return sm_ChatOption;
}

void CNetworkTelemetry::SetCurrentChatOption(const ChatOption newChatOption)
{
	sm_ChatOption = newChatOption;
}

bool CNetworkTelemetry::GetVehicleDrivenInTestDrive()
{
	return sm_LastVehicleDrivenInTestDrive;
}

void CNetworkTelemetry::SetVehicleDrivenInTestDrive(const bool vehicleDrivenInTestDrive)
{
	sm_LastVehicleDrivenInTestDrive = vehicleDrivenInTestDrive;
}

const int CNetworkTelemetry::GetVehicleDrivenLocation()
{
	return sm_VehicleDrivenLocation;
}

void CNetworkTelemetry::SetVehicleDrivenLocation(const int vehicleDrivenLocation)
{
	sm_VehicleDrivenLocation = vehicleDrivenLocation;
}

bool CNetworkTelemetry::IsInLobby()
{
	return SUIContexts::IsActive(ATSTRINGHASH("IN_CORONA_MENU", 0x5849F1FA));
}

void CNetworkTelemetry::AppendPedAttached(int pedCount, bool clone, s32 account)
{
	static int s_duration = 0;
	static int s_maxSubmissionsCount = 0;
	static u32 s_lastSent = sysTimer::GetSystemMsTime();
	static u32 s_submittedCount = 0;
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("ATTACH_PED_AGG_DURATION", 0x99A1AABC), s_duration);
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("ATTACH_PED_AGG_MAX_PER_DURATION", 0x279E3E8B), s_maxSubmissionsCount);

	if (s_duration == -1) // a value of -1 disables the metric completely
		return;

	if (s_duration != 0 && s_lastSent + s_duration < sysTimer::GetSystemMsTime()) // reset the counter if enough time has passed
	{
		s_submittedCount = 0;
		s_lastSent = sysTimer::GetSystemMsTime();
	}
	if (s_maxSubmissionsCount == 0 || s_submittedCount <= s_maxSubmissionsCount)
	{
		MetricATTACH_PED_AGG m(pedCount, clone, account);
		AppendMetric(m);
		s_submittedCount++;
	}
	else
	{
		gnetDebug2("AppendPedAttached  - Skip sending metric, we sent enough recently");
	}
}

#if GEN9_LANDING_PAGE_ENABLED
void CNetworkTelemetry::LandingPageEnter()
{
    MetricLandingPage::ClearPreviousLpId();
    MetricLandingPage::CreateLpId();
    MetricLandingPageNav::TabEntered(0);
    MetricLandingPageNav::ClearFocusedCards();
    MetricLandingPageMisc::SetPriorityBigFeedEnterState(PriorityFeedEnterState::Unknown);

    const ScFeedPost* persistentOnlineTile[4] = { 0 };
    const atHashString persistentOnDiskTile[4];
    SetLandingPagePersistentTileData(nullptr, 0);
    SetLandingPageOnlineData(0, nullptr, nullptr, persistentOnlineTile, persistentOnDiskTile);
}

void CNetworkTelemetry::LandingPageExit()
{
    MetricLandingPage::ClearLpId();
}

void CNetworkTelemetry::LandingPageUpdate()
{
    if (MetricLandingPage::GetLpId() == 0)
    {
        return;
    }

    // These check internally to not spam the server
    AppendLandingPageMisc();
    AppendLandingPagePersistent();
}

void CNetworkTelemetry::AppendLandingPageMisc()
{
    if (!CLandingPage::WasLaunchedFromBoot() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LP_MISC_BOOT_ONLY", 0xF2CC9E1D), false))
    {
        return;
    }

    if (MetricLandingPageMisc::sm_TriggerLpId != MetricLandingPage::GetLpId() && MetricLandingPageMisc::HasData())
    {
        MetricLandingPageMisc::sm_TriggerLpId = MetricLandingPage::GetLpId();

        MetricLandingPageMisc m;
        AppendMetric(m);
    }
}

void CNetworkTelemetry::SetLandingPageOnlineData(unsigned layoutPublishId, const uiCloudSupport::LayoutMapping* mapping, const CSocialClubFeedTilesRow* row,
                                                 const ScFeedPost* (&persistentOnlineTiles)[4], const atHashString(&persistentOnDiskTiles)[4])
{
    MetricLandingPageOnline::Set(layoutPublishId, mapping, row, persistentOnlineTiles, persistentOnDiskTiles);
}

void CNetworkTelemetry::AppendLandingPageOnline(const eLandingPageOnlineDataSource& source, u32 mainOnDiskTileId)
{
    if (!CLandingPage::WasLaunchedFromBoot() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LP_ONLINE_BOOT_ONLY", 0x801E89AF), false))
    {
        return;
    }

    // Non-cloud data automatically has data
    const bool hasData = MetricLandingPageOnline::HasData() || (MetricLandingPageOnline::sm_DataSource != (unsigned)eLandingPageOnlineDataSource::FromCloud);
    const bool hasChanged = (MetricLandingPageOnline::sm_TriggerLpId != MetricLandingPage::GetLpId()) || (MetricLandingPageOnline::sm_DataSource != (unsigned)source);

    if (hasData && hasChanged)
    {
        MetricLandingPageOnline::sm_TriggerLpId = MetricLandingPage::GetLpId();
        MetricLandingPageOnline::sm_DataSource = (unsigned)source;

        MetricLandingPageOnline m(source, mainOnDiskTileId);
        AppendMetric(m);
    }
}

void CNetworkTelemetry::SetLandingPagePersistentTileData(const atHashString* rowArea, const unsigned rowCount)
{
    MetricLandingPagePersistent::Set(rowArea, rowCount);
}

void CNetworkTelemetry::AppendLandingPagePersistent()
{
    if (!CLandingPage::WasLaunchedFromBoot() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LP_PERSIST_BOOT_ONLY", 0x1D259805), false))
    {
        return;
    }

    if (MetricLandingPagePersistent::sm_TriggerLpId != MetricLandingPage::GetLpId() && MetricLandingPagePersistent::HasData())
    {
        MetricLandingPagePersistent::sm_TriggerLpId = MetricLandingPage::GetLpId();

        MetricLandingPagePersistent m;
        AppendMetric(m);
    }
}

void CNetworkTelemetry::PriorityBigFeedEntered()
{
    MetricLandingPageMisc::SetPriorityBigFeedEnterState(PriorityFeedEnterState::Entered);
}

void CNetworkTelemetry::PastPriorityFeed()
{
    if (MetricLandingPageMisc::GetPriorityBigFeedEnterState() == PriorityFeedEnterState::Unknown)
    {
        MetricLandingPageMisc::SetPriorityBigFeedEnterState(PriorityFeedEnterState::NotEntered);
    }
}

void CNetworkTelemetry::AppendBigFeedNav(u32 hoverTile, u32 leftBy, u32 clickedTile, u32 durationMs, int skipTimerMs, bool whatsNew)
{
    MetricBigFeedNav m(hoverTile, leftBy, clickedTile, durationMs, skipTimerMs, whatsNew);
    AppendMetric(m);
}

void CNetworkTelemetry::LandingPageTrackMessage(const uiPageDeckMessageBase& msg, const u32 activePage, const bool handled)
{
    if (!handled)
    {
        return;
    }

    atHashString const c_trackingId = msg.GetTrackingId();
    atHashString trackingType;
    bool exitLandingPage = false;

    if (uiCloudSupport::CanBeTracked(msg, trackingType, exitLandingPage))
    {
        LandingPageTileClicked(c_trackingId, trackingType, activePage, exitLandingPage);
    }

    if (uiCloudSupport::TracksAsLandingPageSubPage(msg))
    {
        MetricLandingPage::StorePreviousLpId();
    }
}

void CNetworkTelemetry::LandingPageTabEntered(u32 currentTab, u32 newTab)
{
    const u32 prev = MetricLandingPageNav::GetTab();

    if (prev != 0 && prev != newTab && currentTab != newTab)
    {
        gnetDebug2("LandingPageTabEntered from [%s/0x%08X] to [%s/0x%08X]", atHashString(currentTab).TryGetCStr(), currentTab, atHashString(newTab).TryGetCStr(), newTab);

        // The current tab counts as clicked "tile" as requested by PIA
        AppendLandingPageNav(currentTab, ATSTRINGHASH("uiTabChange", 0x23126A47), currentTab, false);
        MetricLandingPageNav::ClearFocusedCards();
    }

    MetricLandingPageNav::TabEntered(newTab);
}

void CNetworkTelemetry::CardFocusGained(int cardBit)
{
    if (cardBit >= 0)
    {
        gnetDebug2("CardFocusGained [%d]", cardBit);
        MetricLandingPageNav::CardFocusGained((u32)cardBit);
    }
}

void CNetworkTelemetry::LandingPageTileClicked(u32 clickedTile, u32 leftBy, u32 activePage, bool exitingLp)
{
    AppendLandingPageNav(clickedTile, leftBy, activePage, exitingLp);
    MetricLandingPageNav::ClearFocusedCards();
}

void CNetworkTelemetry::AppendLandingPageNavFromScript(u32 clickedTile, u32 leftBy, u32 activePage, u32 hoverBits, bool exitingLp)
{
    if (MetricLandingPage::GetPreviousLpId() == 0)
    {
        // Only for pages triggered via the landing page
        return;
    }

    // Lazy, yes. But this way we need less natives
    if (clickedTile == 0 && leftBy == 0 && activePage == 0)
    {
        gnetDebug2("Resetting LP_NAV from script");
        MetricLandingPageNav::ClearFocusedCards();
        MetricLandingPageNav::TouchLastAction();
        return;
    }

    MetricLandingPageNav::SetFocusedCards(hoverBits);
    AppendLandingPageNav(clickedTile, leftBy, activePage, exitingLp);
}

void CNetworkTelemetry::AppendLandingPageNav(u32 clickedTile, u32 leftBy, u32 activePage, bool exitingLp)
{
    #define LP_DEFAULT_MIN_TIME 5000

    const u32 minTimeMs = (u32)Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LP_NAV_MIN_TIME_MS", 0xA0AD50B4), LP_DEFAULT_MIN_TIME);
    const u32 diffTimeMs = MetricLandingPageNav::GetLastActionDiffMs();

    gnetDebug2("AppendLandingPageNav - clickedTile[%s/0x%08X], leftBy[%s/0x%08X] exiting[%s] difftime[%u]", 
        atHashString(clickedTile).TryGetCStr(), clickedTile, atHashString(leftBy).TryGetCStr(), leftBy, LogBool(exitingLp), diffTimeMs);

    if (!exitingLp && diffTimeMs < minTimeMs OUTPUT_ONLY(&& !PARAM_telemetryLandingNavAll.Get()))
    {
        gnetDebug1("AppendLandingPageNav skipped. Not enough time passed [%ums < %ums].", diffTimeMs, minTimeMs);
        return;
    }

    MetricLandingPageNav m(clickedTile, leftBy, activePage);
    AppendMetric(m);
    MetricLandingPageNav::TouchLastAction();
}

u32 CNetworkTelemetry::LandingPageMapPos(u32 x, u32 y)
{
    return MetricLandingPage::MapPos(x, y);
}

/////////////////////////////////////////////////////////////////////////////// WINDFALL
bool WriteIntIfNotDefault(RsonWriter* rw, const char* name, const int value, const int defaultValue)
{
	bool result = true;
	if (value != defaultValue)
	{
		result = rw->WriteInt(name, value);
	}
	return result;
}

class MetricWINDFALL : public MetricPlayStat
{
	RL_DECLARE_METRIC(WINDFALL, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricWINDFALL(const sWindfallInfo& data)
		: m_data(data)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPlayStat::Write(rw);
		result = result && rw->WriteInt("a", m_data.m_career);
		result = result && WriteIntIfNotDefault(rw, "b", m_data.m_property, -1);
		result = result && WriteIntIfNotDefault(rw, "c", m_data.m_business, -1);

		if (m_data.m_vehiclesCount > 0)
		{
			result = result && rw->BeginArray("d", NULL);
			for (int i = 0; i < m_data.m_vehiclesCount; i++)
			{
				result = result && WriteIntIfNotDefault(rw, NULL, m_data.m_vehicles[i], 0);
			}
			result = result && rw->End();
		}

		if (m_data.m_weaponsCount > 0)
		{
			result = result && rw->BeginArray("e", NULL);
			for (int i = 0; i < m_data.m_weaponsCount; i++)
			{
				result = result && WriteIntIfNotDefault(rw, NULL, m_data.m_weapons[i], 0);
			}
			result = result && rw->End();
		}

		result = result && rw->WriteInt("f", m_data.m_outfits);
		result = result && rw->WriteInt("g", m_data.m_leftover);
		result = result && rw->WriteInt("h", m_data.m_tilePos);
		result = result && rw->WriteInt("i", m_data.m_durExec);
		result = result && rw->WriteInt("j", m_data.m_durGun);
		result = result && rw->WriteInt("k", m_data.m_durNight);
		result = result && rw->WriteInt("l", m_data.m_durBiker);
		result = result && rw->WriteInt("m", m_data.m_charSlot);
		result = result && rw->WriteBool("n", m_data.m_newSlot);
		result = result && rw->WriteInt("o", m_data.m_duration);
		result = result && WriteIntIfNotDefault(rw, "p", m_data.m_propertyAddon, -1);
		result = result && WriteIntIfNotDefault(rw, "q", m_data.m_businessAddon, -1);

		return result;
	}

private:
	sWindfallInfo m_data;
};

// triggers after the player boots in game following the windfall path
void CNetworkTelemetry::AppendPlaystatsWindfall(const sWindfallInfo& data)
{
	MetricWINDFALL m(data);
	APPEND_METRIC(m);
}

void CNetworkTelemetry::AppendSaInNav(const unsigned product)
{
    MetricSaInNav::CreateNavId();

    MetricSaInNav m(product);
    AppendMetric(m);
}

void CNetworkTelemetry::SpUpgradeBought(const unsigned product)
{
    if (MetricSaInNav::GetNavId() == 0)
    {
        gnetDebug1("SpUpgradeBought skipped - no nav id");
        return;
    }

    MetricSaOutNav::SpUpgradeBought(product);
}

void CNetworkTelemetry::GtaMembershipBought(const unsigned product)
{
    if (MetricSaInNav::GetNavId() == 0)
    {
        gnetDebug1("GtaMembershipBought skipped - no nav id");
        return;
    }

    MetricSaOutNav::GtaMembershipBought(product);
}

void CNetworkTelemetry::AppendSaOutNav()
{
    if (MetricSaInNav::GetNavId() == 0)
    {
        gnetDebug1("Metric SaOutNav skipped - no nav id");
        return;
    }

#if RSG_XBOX || RSG_NP
    const cCommerceUtil* commerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    if (commerceUtil && commerceUtil->GetLastCheckoutResult() == eCommerceCheckoutResult::Purchased)
    {
        atString product = commerceUtil->GetLastCheckoutId();
        const cCommerceItemData* data = commerceUtil->GetItemData(product.c_str(), cCommerceItemData::ITEM_TYPE_PRODUCT);

        if (data != nullptr && cCommerceManager::IsSpUpsellItem(*data))
        {
            MetricSaOutNav::SpUpgradeBought(atStringHash(product.c_str()));
        }

        if (data != nullptr && cCommerceManager::IsGtaMembershipItem(*data))
        {
            MetricSaOutNav::GtaMembershipBought(atStringHash(product.c_str()));
        }
    }
#endif

    MetricSaOutNav m;
    AppendMetric(m);

    MetricSaInNav::ClearNavId();
    MetricSaOutNav::ClearBought();
}
#endif // GEN9_LANDING_PAGE_ENABLED

// EOF

