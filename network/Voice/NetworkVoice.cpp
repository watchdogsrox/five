//
// name:        NetworkVoice.h
//
#include "Network/Voice/NetworkVoice.h"

// framework/engine headers
#include "fwnet/netchannel.h"
#include "fwnet/netutils.h"
#include "rline/rlfriendsmanager.h"
#include "rline/rlpresence.h"
#include "rline/rlsystemui.h"
#include "system/memory.h"

#if RSG_PC
#include "input/keyboard.h"
#endif // RSG_PC

// game headers
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/ProfileSettings.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Bandwidth/NetworkBandwidthManager.h"
#include "Network/Live/livemanager.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkVoiceSession.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Peds/Ped.h"
#include "scene/world/GameWorld.h"
#include "Stats/StatsInterface.h"

#if RSG_PC
#include "Frontend/PauseMenu.h"
#include "system/controlMgr.h"
#endif // RSG_PC

#if RSG_DURANGO
#include "avchat/voicechat_durango.h"
#endif

#if RSG_SCARLETT
#pragma warning (disable : 4668)   // turn warning off - error C4668: 'WINAPI_PARTITION_TV_APP' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' in xaudio2.h (which we will be replacing)
#include "xaudio2.h"
#pragma warning (default : 4668)   // turn warning back on
#endif // RSG_SCARLETT

#if __BANK
#include "bank/combo.h"
#endif // __BANK

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, voice, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_voice

PARAM(voiceDisabled, "[network] Disables voice chat in network games");
PARAM(voiceOverrideActive, "[network] Voice is always active");
PARAM(voiceOverrideAllRestrictions, "[network] No restrictions on voice chat");
PARAM(voiceOverrideTeamChat, "[network] Don't filter voice chat based on teams");
PARAM(voiceIgnoreHardware, "[network] Don't filter voice chat based on connection (or otherwise) microphones, etc. ");
PARAM(voiceForceHeadset, "[network] Will return true to external systems that we have a headset");
PARAM(netTextMessageEnableFilter, "[network] Whether text message filtering is enabled");

namespace rage
{
	XPARAM(noaudiothread);
}

static const unsigned DEFAULT_FILTER_HOLDING_PEN_MAX_REQUESTS = 10;
static const unsigned DEFAULT_FILTER_HOLDING_PEN_INTERVAL_MS = 500;
static const unsigned DEFAULT_FILTER_SPAM_MAX_REQUESTS = 30;
static const unsigned DEFAULT_FILTER_SPAM_INTERVAL_MS = 10000;
static const unsigned DEFAULT_TEXT_MESSAGE_VALID_SEND_GROUPS = SentFromGroup::Groups_CloseContacts | SentFromGroup::Group_SameTeam;

// default maximum voice recipients (0 implies no limit)
static const unsigned DEFAULT_MAX_VOICE_RECIPIENTS = 0; 

// default bandwidth limit for voice channel
static const unsigned DEFAULT_VOICE_BANDWIDTH_LIMIT = netChannelPolicies::DEFAULT_OUTBOUND_BANDWIDTH_LIMIT;

// default settings for voice proximity detection (0-10, 10 is most sensitive, i.e. requires
// voice to be very close to the mic, while 0 picks up loads of background noise).
static const unsigned VPD_THRESHOLD = 9;

// default minimum voice send interval 
static const unsigned SEND_INTERVAL = VoiceChat::DEFAULT_MIN_SEND_INTERVAL;

// default proximity values
static const float DEFAULT_TALKER_PROXIMITY_HEIGHT = 10.0f;
static const float DEFAULT_LOUDHAILER_PROXIMITY = 150.0f;
static const float DEFAULT_LOUDHAILER_PROXIMITY_HEIGHT = 50.0f;

// default dead-zone at which we won't thrash voice add / remove
static const float DEFAULT_DEAD_ZONE = 5.0f;

struct CMsgVoiceStatus
{
	NET_MESSAGE_DECL(CMsgVoiceStatus, CMSG_VOICE_STATUS);

	CMsgVoiceStatus()
		: m_VoiceFlags(0)
	{
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_VoiceFlags, SIZEOF_FLAGS) 
			&& bb.SerUser(msg.m_hFromGamer)
			//Don't send remotely set flags.
			&& gnetVerify(0 == (msg.m_VoiceFlags & VoiceGamerSettings::VOICE_REMOTE_MASK));
	}

	int GetMessageDataBitSize() const
	{
		return SIZEOF_FLAGS;
	}

	static const unsigned SIZEOF_FLAGS = VoiceGamerSettings::VOICE_NUM_BITS;

	unsigned m_VoiceFlags;
	rlGamerHandle m_hFromGamer;
};

CompileTimeAssert(((1<<CMsgVoiceStatus::SIZEOF_FLAGS)-1) <= VoiceGamerSettings::VOICE_LOCAL_MASK);

struct MsgTextMessage
{
	NET_MESSAGE_DECL(MsgTextMessage, MSG_TEXT_MESSAGE);

	MsgTextMessage()
	{
		m_szTextMessage[0] = '\0';
	}

	MsgTextMessage(const rlGamerHandle& hGamer, const char* szTextMessage)
	{
		m_hFromGamer = hGamer;
		safecpy(m_szTextMessage, szTextMessage);
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerStr(msg.m_szTextMessage, MAX_TEXT_MESSAGE_LENGTH) 
			&& bb.SerUser(msg.m_hFromGamer);
	}

	char m_szTextMessage[MAX_TEXT_MESSAGE_LENGTH];
	rlGamerHandle m_hFromGamer;
};

NET_MESSAGE_IMPL(CMsgVoiceStatus);
NET_MESSAGE_IMPL(MsgTextMessage);

#if __BANK

//Debug bank to start a chat with a certain user.
struct CMsgVoiceChatRequest
{
	NET_MESSAGE_DECL(CMsgVoiceChatRequest, CMSG_VOICE_CHAT_REQUEST);

	CMsgVoiceChatRequest()
	{
	}

	NET_MESSAGE_SER(/*bb*/, /*msg*/)
	{
		return true;
	}

	int GetMessageDataBitSize() const
	{
		return 0;
	}
};
NET_MESSAGE_IMPL(CMsgVoiceChatRequest);

//Debug bank to start a chat with a certain user.
struct CMsgVoiceChatReply
{
	NET_MESSAGE_DECL(CMsgVoiceChatReply, CMSG_VOICE_CHAT_REPLY);

	CMsgVoiceChatReply()
	{
	}

	NET_MESSAGE_SER(/*bb*/, /*msg*/)
	{
		return true;
	}

	int GetMessageDataBitSize() const
	{
		return 0;
	}
};
NET_MESSAGE_IMPL(CMsgVoiceChatReply);

//Debug bank to start a chat with a certain user.
struct CMsgVoiceChatEnd
{
	NET_MESSAGE_DECL(CMsgVoiceChatEnd, CMSG_VOICE_CHAT_END);

	CMsgVoiceChatEnd()
	{
	}

	NET_MESSAGE_SER(/*bb*/, /*msg*/)
	{
		return true;
	}

	int GetMessageDataBitSize() const
	{
		return 0;
	}
};
NET_MESSAGE_IMPL(CMsgVoiceChatEnd);

#endif // __BANK

void OnPresenceEvent(const rlPresenceEvent* pEvent)
{
	// catch events that require us to rebuild the mute lists
	switch(pEvent->GetId())
	{
	case PRESENCE_EVENT_MUTE_LIST_CHANGED:
	case PRESENCE_EVENT_SIGNIN_STATUS_CHANGED:
		NetworkInterface::GetVoice().BuildMuteLists();
		break;
	default:
		break;
	}
}

// presence Management
rlPresence::Delegate sm_PresenceDlgt(&OnPresenceEvent);

#if RSG_DURANGO
static rlXblChatEventDelegate m_ChatDlgt;
#endif

CNetworkVoice::CNetworkVoice()
	: m_CxnMgr(nullptr)
	, m_IsInitialized(false)
	, m_HasAttemptedInitialize(false)
	, m_bWorkerThreadInitialised(false)
	, m_bIsCommunicationActive(true)
	, m_bRemainInGameChat(false)
	, m_bOverrideTransitionChatScript(false)
	, m_bOverrideTransitionChat(false)
	, m_nOverrideTransitionRestrictions(0)
	, m_nVoiceGroup(VOICEGROUP_NONE)
#if !__NO_OUTPUT
	, m_nVoiceChannel(netPlayer::VOICE_CHANNEL_NONE)
	, m_nTutorialIndex(CNetObjPlayer::INVALID_TUTORIAL_INDEX)
	, m_nTeam(-1)
	, m_bSpectating(false)
#endif
	, m_nMaxVoiceRecipients(DEFAULT_MAX_VOICE_RECIPIENTS)
	, m_VoiceBandwidthLimit(DEFAULT_VOICE_BANDWIDTH_LIMIT)
	, m_bOverrideAllRestrictions(false)
	, m_TeamOnlyChat(false)
	, m_TeamChatIncludeUnassignedTeams(false)
	, m_OverrideTeamRestrictions(0)
	, m_OverrideChatRestrictions(0)
	, m_OverrideChatRestrictionsSendLocal(0)
	, m_OverrideChatRestrictionsReceiveLocal(0)
	, m_OverrideChatRestrictionsSendRemote(0)
	, m_OverrideChatRestrictionsReceiveRemote(0)
	, m_bOverrideAllSendRestrictions(false)
	, m_bOverrideAllReceiveRestrictions(false)
	, m_OverrideSpectatorMode(false)
	, m_OverrideTutorialSession(false)
	, m_OverrideTutorialRestrictions(0)
	, m_ProximityAffectsReceive(false)
	, m_ProximityAffectsSend(false)
	, m_ProximityAffectsTeam(false)
	, m_NoSpectatorChat(false)
	, m_bIgnoreSpectatorLimitsSameTeam(false)
	, m_TalkerProximityLo(0)
	, m_TalkerProximityHi(0)
	, m_TalkerProximityLoSq(m_TalkerProximityLo * m_TalkerProximityLo)
	, m_TalkerProximityHiSq(m_TalkerProximityHi * m_TalkerProximityHi)
	, m_TalkerProximityHeightLo(DEFAULT_TALKER_PROXIMITY_HEIGHT)
	, m_TalkerProximityHeightHi(DEFAULT_TALKER_PROXIMITY_HEIGHT + DEFAULT_DEAD_ZONE)
	, m_TalkerProximityHeightLoSq(m_TalkerProximityHeightLo * m_TalkerProximityHeightLo)
	, m_TalkerProximityHeightHiSq(m_TalkerProximityHeightHi * m_TalkerProximityHeightHi)
	, m_bScriptAllowUniDirectionalOverrideForNegativeProximity(true)
	, m_LoudhailerProximityLo(DEFAULT_LOUDHAILER_PROXIMITY)
	, m_LoudhailerProximityHi(DEFAULT_LOUDHAILER_PROXIMITY + DEFAULT_DEAD_ZONE)
	, m_LoudhailerProximityLoSq(m_LoudhailerProximityLo * m_LoudhailerProximityLo)
	, m_LoudhailerProximityHiSq(m_LoudhailerProximityHi * m_LoudhailerProximityHi)
	, m_LoudhailerProximityHeightLo(DEFAULT_LOUDHAILER_PROXIMITY_HEIGHT)
	, m_LoudhailerProximityHeightHi(DEFAULT_LOUDHAILER_PROXIMITY_HEIGHT + DEFAULT_DEAD_ZONE)
	, m_LoudhailerProximityHeightLoSq(m_LoudhailerProximityHeightLo * m_LoudhailerProximityHeightLo)
	, m_LoudhailerProximityHeightHiSq(m_LoudhailerProximityHeightHi * m_LoudhailerProximityHeightHi)
	, m_DisableBandwidthRestrictions(0)
	, m_LastTargetUpstreamBandwidth(0)
	, m_nMaxBandwidthVoiceRecipients(DEFAULT_MAX_VOICE_RECIPIENTS)
	, m_nFullCheckTalkerIndex(0)
	, m_UsePresenceForTextMessages(false)
	, m_DisableReceivingTextMessages(false)
	, m_IsTextFilterEnabled(false)
	, m_FilterHoldingMaxRequests(DEFAULT_FILTER_HOLDING_PEN_MAX_REQUESTS)
	, m_FilterHoldingPenIntervalMs(DEFAULT_FILTER_HOLDING_PEN_INTERVAL_MS)
	, m_FilterSpamMaxRequests(DEFAULT_FILTER_SPAM_MAX_REQUESTS)
	, m_FilterSpamIntervalMs(DEFAULT_FILTER_SPAM_INTERVAL_MS)
	, m_TextAllowedSentFromGroups(DEFAULT_TEXT_MESSAGE_VALID_SEND_GROUPS)
	, m_bAllowUniDirectionalOverrideForNegativeProximity(true)
	, m_PendingMutesForAutoMute(0)
	, m_PendingTalkersForAutoMute(0)
#if RSG_NP
	, m_bNpMuteWhenUIShowing(true)
#endif
#if RSG_PC
	, m_bEnabled(true)
	, m_bTalkEnabled(true)
#elif RSG_DURANGO
	, m_bRefreshingMuteList(false)
	, m_bHasMuteList(false)
#endif
#if RSG_XBOX
	, m_IssueCommunicationCheck(true)
	, m_bIsConstrained(false)
#endif
{
#if __BANK
	m_Dlgt.Bind(this, &CNetworkVoice::OnMessageReceived);
#endif

#if RSG_DURANGO
	m_ChatDlgt.Bind(this, &CNetworkVoice::OnChatEvent);
	m_MuteList.m_Players.Reset();
#endif

	m_VoiceChatDlgt.Bind(this, &CNetworkVoice::OnVoiceChatEventReceived);
	m_CxnMgrDlgtGame.Bind(this, &CNetworkVoice::OnConnectionEvent);
	m_CxnMgrDlgtActivity.Bind(this, &CNetworkVoice::OnConnectionEvent);
	m_CxnMgrDlgtVoice.Bind(this, &CNetworkVoice::OnConnectionEvent);

	for(unsigned index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
	{
		m_DisableBandwidthVoiceTime[index] = 0;
	}
}

CNetworkVoice::~CNetworkVoice()
{
	this->Shutdown();
}

bool CNetworkVoice::InitCore(netConnectionManager* pCxnMgr)
{
	gnetDebug1("InitCore");

	// initialise voice
	void* vcDevice = NULL;

	if(!gnetVerify(pCxnMgr))
	{
		gnetError("InitCore :: Connection Manager is invalid!");
		return false;
	}

	// keep a copy of the connection manager
	m_CxnMgr = pCxnMgr;

	// initialise voice chat class
	bool bSuccess = m_VoiceChat.Init(VoiceChatTypes::MAX_LOCAL_TALKERS, VoiceChatTypes::MAX_REMOTE_TALKERS, vcDevice, NETWORK_CPU_AFFINITY);
	m_VoiceChat.SetVpdThreshold(VPD_THRESHOLD);
	m_VoiceChat.SetVpdThresholdPTT(0);

	// set voice channel policies
	m_VoicePolicies.m_CompressionEnabled = false;
	m_VoicePolicies.m_OutboundBandwidthLimit = m_VoiceBandwidthLimit;
	m_CxnMgr->SetChannelPolicies(NETWORK_VOICE_CHANNEL_ID, m_VoicePolicies);

#if RSG_PC
	// TOOD: NS - apply user's saved preferences
	bool voiceChatEnabled = !PARAM_voiceDisabled.Get() && !PARAM_noaudiothread.Get();
	bool talkingEnabled = true;
	u32 outputVolume = 100;
	u32 microphoneVolume = 100;
	u32 outputDeviceId = 0; // default device as set up in Windows
	u32 inputDeviceId = 0; // default device as set up in Windows

	if(voiceChatEnabled)
	{
		m_VoiceChat.SetPlaybackDeviceById32(outputDeviceId);

		if(talkingEnabled)
		{
			m_VoiceChat.SetCaptureDeviceById32(inputDeviceId);
		}
	}

	m_VoiceChat.SetPlaybackDeviceVolume((float)outputVolume / 100.0f);
	m_VoiceChat.SetCaptureDeviceVolume((float)microphoneVolume / 100.0f);

	m_VoiceChat.SetVpdThreshold(0);
	m_VoiceChat.SetVpdThresholdPTT(0);
	m_VoiceChat.SetCaptureMode(VoiceChat::CAPTURE_MODE_PUSH_TO_TALK);
#elif RSG_DURANGO
	VoiceChatDurangoSingleton::GetInstance().AddDelegate(&m_ChatDlgt);
#endif

	return bSuccess;
}

void CNetworkVoice::ShutdownCore()
{
	m_VoiceChat.Shutdown();

#if RSG_PC
	// fixing network heap warnings by freeing our devices on shutdown
	// these should perhaps be moved to core setup and initialization and allocated from the game heap
	m_VoiceChat.FreeCaptureDevice();
	m_VoiceChat.FreePlaybackDevice();
#elif RSG_DURANGO
	VoiceChatDurangoSingleton::GetInstance().RemoveDelegate(&m_ChatDlgt);
#endif
}

bool CNetworkVoice::Init()
{
	gnetDebug1("Init");

	// mark that we had the intention of initialising
	m_HasAttemptedInitialize = true; 

	// check that we're not already initialised
	if(!gnetVerifyf(!m_IsInitialized, "Init :: Already Initialised!"))
	{
		gnetError("Init :: Already Initialised!");
		return false;
	}

	// if we're in single player group - make sure we remove the local talkers
	if(m_nVoiceGroup == VOICEGROUP_SINGLEPLAYER)
		m_VoiceChat.RemoveAllTalkers();

	// setup connection manager delegates
	m_CxnMgr->AddChannelDelegate(&m_CxnMgrDlgtGame, NETWORK_SESSION_GAME_CHANNEL_ID);
	m_CxnMgr->AddChannelDelegate(&m_CxnMgrDlgtActivity, NETWORK_SESSION_ACTIVITY_CHANNEL_ID);
	m_CxnMgr->AddChannelDelegate(&m_CxnMgrDlgtVoice, NETWORK_SESSION_VOICE_CHANNEL_ID);

	// initialise voice chat class
	bool bSuccess = m_VoiceChat.InitNetwork(m_CxnMgr, NETWORK_VOICE_CHANNEL_ID);

	// bail out on failure
	if(!gnetVerifyf(bSuccess, "Init :: Failed to Initialise VoiceChat!"))
		return false;

	// set initial tunables
	m_VoiceChat.SetSendInterval(SEND_INTERVAL);

#if __BANK
	m_BankVpdThreshold = m_VoiceChat.GetVpdThreshold();
	m_BankSendInterval = m_VoiceChat.GetSendInterval();
#endif

	m_AudioEntity.Init();

	// add delegates
#if __BANK
	NetworkInterface::GetPlayerMgr().AddDelegate(&m_Dlgt);
#endif

	rlPresence::AddDelegate(&sm_PresenceDlgt);
	m_VoiceChat.AddDelegate(&m_VoiceChatDlgt);

	// clear out focus talker
	m_FocusTalker.Clear();

	// reset proximities
	m_TalkerProximityLo = 0;
	m_TalkerProximityHi = 0;
	m_TalkerProximityLoSq = m_TalkerProximityLo * m_TalkerProximityLo;
	m_TalkerProximityHiSq = m_TalkerProximityHi * m_TalkerProximityHi;
	m_TalkerProximityHeightLo = DEFAULT_TALKER_PROXIMITY_HEIGHT;
	m_TalkerProximityHeightHi = DEFAULT_TALKER_PROXIMITY_HEIGHT + DEFAULT_DEAD_ZONE;
	m_TalkerProximityHeightLoSq = m_TalkerProximityHeightLo * m_TalkerProximityHeightLo;
	m_TalkerProximityHeightHiSq = m_TalkerProximityHeightHi * m_TalkerProximityHeightHi;
	m_LoudhailerProximityLo = DEFAULT_LOUDHAILER_PROXIMITY;
	m_LoudhailerProximityHi = DEFAULT_LOUDHAILER_PROXIMITY + DEFAULT_DEAD_ZONE;
	m_LoudhailerProximityLoSq = m_LoudhailerProximityLo * m_LoudhailerProximityLo;
	m_LoudhailerProximityHiSq = m_LoudhailerProximityHi * m_LoudhailerProximityHi;
	m_LoudhailerProximityHeightLo = DEFAULT_LOUDHAILER_PROXIMITY_HEIGHT;
	m_LoudhailerProximityHeightHi = DEFAULT_LOUDHAILER_PROXIMITY_HEIGHT + DEFAULT_DEAD_ZONE;
	m_LoudhailerProximityHeightLoSq = m_LoudhailerProximityHeightLo * m_LoudhailerProximityHeightLo;
	m_LoudhailerProximityHeightHiSq = m_LoudhailerProximityHeightHi * m_LoudhailerProximityHeightHi;

	// not in a session
	m_nVoiceGroup = VOICEGROUP_NONE;

	// reset talker index
	m_nFullCheckTalkerIndex = 0;

	// reset team only chat
	m_TeamOnlyChat = false;
	m_TeamChatIncludeUnassignedTeams = false;
	m_OverrideTeamRestrictions = 0;

	// reset overrides
	m_OverrideChatRestrictions = 0;
	m_OverrideChatRestrictionsSendLocal = 0;
	m_OverrideChatRestrictionsReceiveLocal = 0;
	m_OverrideChatRestrictionsSendRemote = 0;
	m_OverrideChatRestrictionsReceiveRemote = 0;
	m_OverrideSpectatorMode = false;
	m_OverrideTutorialSession = false;
	m_OverrideTutorialRestrictions = 0;

	// reset voice limits
	m_nMaxVoiceRecipients = DEFAULT_MAX_VOICE_RECIPIENTS;
	m_VoiceBandwidthLimit = DEFAULT_VOICE_BANDWIDTH_LIMIT;

	// reset bandwidth restrictions
	m_DisableBandwidthRestrictions = 0;
	m_LastTargetUpstreamBandwidth  = 0;
	m_nMaxBandwidthVoiceRecipients = DEFAULT_MAX_VOICE_RECIPIENTS;

	for(unsigned index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
	{
		m_DisableBandwidthVoiceTime[index] = 0;
	}

#if RSG_PC
	CPauseMenu::UpdateVoiceSettings();
#endif // RSG_PC

	// initialised
	m_IsInitialized = true;

	// indicate success
	return true;
}

void CNetworkVoice::Shutdown()
{
	gnetDebug1("Shutdown :: Initialised: %s", m_IsInitialized ? "True" : "False");

	// check that we're in a good state to shutdown
	if(!m_IsInitialized)
	{
		m_HasAttemptedInitialize = false; 
		return;
	}

	// voice chat indirectly allocates memory from the main heap through the audio provider
	// we're currently deallocating out of the network heap so temporarily restore the 
	// main heap to remove the audio allocations
	sysMemAllocator* pNetworkHeap = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

	if(VoiceChat::GetAudioProvider())
		VoiceChat::GetAudioProvider()->RemoveAllRemotePlayers();

	// restore the network heap
	sysMemAllocator::SetCurrent(*pNetworkHeap);

	// now we can safely shutdown voicechat
	m_VoiceChat.RemoveDelegate(&m_VoiceChatDlgt);
	m_VoiceChat.ShutdownNetwork();
	m_AudioEntity.Shutdown();

	// remove delegates
#if __BANK
	NetworkInterface::GetPlayerMgr().RemoveDelegate(&m_Dlgt);
#endif

	rlPresence::RemoveDelegate(&sm_PresenceDlgt);

	// clear out focus talker
	m_FocusTalker.Clear();

	// clear our connection manager delegates
	m_CxnMgr->RemoveChannelDelegate(&m_CxnMgrDlgtGame);
	m_CxnMgr->RemoveChannelDelegate(&m_CxnMgrDlgtActivity);
	m_CxnMgr->RemoveChannelDelegate(&m_CxnMgrDlgtVoice);

	// no longer initialise
	m_HasAttemptedInitialize = false; 
	m_IsInitialized = false;

	// toggle this on to cover error scenarios where script might not reach their toggle
	m_bIsCommunicationActive = true;

	// drop all tracked gamers if we're going back to single player
	if(!CNetwork::GetNetworkSession().IsTransitioning())
	{
		gnetDebug1("Shutdown :: Flushing %d gamers, %d pending gamers", m_Gamers.GetCount(), m_PendingGamers.GetCount());
		m_Gamers.Reset();
		m_PendingGamers.Reset();

		// clear local gamer settings
		m_bHasLocalGamer = false;
		m_LocalGamer.Clear();

#if RSG_DURANGO
		// Remove All Chat Users on Durango re-starts the chat manager, which alters memory 
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
		m_VoiceChat.RemoveAllChatUsers();
		sysMemAllocator::SetCurrent(*pNetworkHeap);
#endif

		// reset these
		m_NoSpectatorChat = false;
		m_bIgnoreSpectatorLimitsSameTeam = false;

		// reset transition chat override
		m_bOverrideTransitionChatScript = false;
		m_bOverrideTransitionChat = false;
		m_nOverrideTransitionRestrictions = 0;
	}
}

void CNetworkVoice::InitWorkerThread()
{
	gnetDebug1("InitWorkerThread :: Enabled: %s, Worker Initialised: %s", IsEnabled() ? "True" : "False", m_bWorkerThreadInitialised ? "True" : "False");

	if(!IsEnabled())
		return;

	m_bWorkerThreadInitialised = true; 
	m_VoiceChat.InitWorkerThread(NETWORK_CPU_AFFINITY);
}

void CNetworkVoice::ShutdownWorkerThread()
{
	gnetDebug1("ShutdownWorkerThread :: Enabled: %s, Worker Initialised: %s", IsEnabled() ? "True" : "False", m_bWorkerThreadInitialised ? "True" : "False");

	if(!m_bWorkerThreadInitialised)
		return;

	m_bWorkerThreadInitialised = false;

	m_VoiceChat.ShutdownWorkerThread(); 
}

void CNetworkVoice::InitTunables()
{
	m_TextAllowedSentFromGroups = CLiveManager::GetValidSentFromGroupsFromTunables(DEFAULT_TEXT_MESSAGE_VALID_SEND_GROUPS, "VOICE_CHAT");

	m_UsePresenceForTextMessages = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("NET_VOICE_USE_PRESENCE_FOR_TEXT_MESSAGES", 0xe734c93e), false);
	m_DisableReceivingTextMessages = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("NET_VOICE_DISABLE_RECEIVING_TEXT_MESSAGES", 0xd5316c94), false);

#if RSG_NP
	m_bNpMuteWhenUIShowing = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("NET_VOICE_ORBIS_MUTE_WHEN_UI_SHOWING", 0xdb6747b5), true);
#endif

#if RSG_XBOX
	m_IssueCommunicationCheck = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ISSUE_COMMUNICATION_CHECKS", 0x099db7cf), m_IssueCommunicationCheck);
#endif

	m_bAllowUniDirectionalOverrideForNegativeProximity = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_VOICE_UNI_DIRECTIONAL_OVERRIDE_FOR_NEGATIVE_PROXIMITY", 0xa4e18912), m_bAllowUniDirectionalOverrideForNegativeProximity);

	// text filter
	m_IsTextFilterEnabled = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_TEXT_FILTER_ENABLED", 0x154891e0), m_IsTextFilterEnabled);
	m_FilterHoldingMaxRequests = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_TEXT_FILTER_HOLDING_PEN_MAX_REQUESTS", 0xd35e42a8), m_FilterHoldingMaxRequests);
	m_FilterHoldingPenIntervalMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_TEXT_FILTER_HOLDING_PEN_INTERVAL_MS", 0xb4613d4b), m_FilterHoldingPenIntervalMs);
	m_FilterSpamMaxRequests = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_TEXT_FILTER_SPAM_MAX_REQUESTS", 0x8a29484a), m_FilterSpamMaxRequests);
	m_FilterSpamIntervalMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_TEXT_FILTER_SPAM_INTERVAL_MS", 0x53ae8d54), m_FilterSpamIntervalMs);
}

void CNetworkVoice::OnMainGamerAssigned()
{
	gnetDebug1("OnMainGamerAssigned");

#if RSG_DURANGO
	m_bHasMuteList = false;
	m_MuteList.m_Players.Reset();
	if(NetworkInterface::IsLocalPlayerOnline() && !m_bRefreshingMuteList)
	{
		m_bRefreshingMuteList = true; 
		g_rlXbl.GetPlayerManager()->GetMuteList(NetworkInterface::GetLocalGamerIndex(), &m_MuteList, &m_MuteListStatus);
	}
#endif
}

void CNetworkVoice::OnSignedOnline()
{
	gnetDebug1("OnSignedOnline");

	// work out session type
	m_nVoiceGroup = VOICEGROUP_NONE;
	m_VoiceChat.RemoveAllTalkers();
	m_AudioEntity.RemoveAllTalkers();
	m_LastTalkers.Reset();
}

void CNetworkVoice::OnSignOut()
{
	gnetDebug1("OnSignOut");

	// clear local gamer settings
	m_bHasLocalGamer = false;
	m_LocalGamer.Clear();

	// work out session type
	//m_nVoiceGroup = VOICEGROUP_NONE;
	//m_VoiceChat.RemoveAllTalkers();
	//m_AudioEntity.RemoveAllTalkers();
	//m_LastTalkers.Reset();
}

bool CNetworkVoice::IsInitialized() const
{
	return m_IsInitialized;
}

bool CNetworkVoice::IsEnabled() const
{
#if RSG_PC
	return !PARAM_voiceDisabled.Get() && !PARAM_noaudiothread.Get() && m_bEnabled;
#else
	return !PARAM_voiceDisabled.Get() && !PARAM_noaudiothread.Get();
#endif // RSG_PC
}

#if RSG_PC
bool CNetworkVoice::IsTalkEnabled() const
{
	return !PARAM_voiceDisabled.Get() && !PARAM_noaudiothread.Get() && m_bTalkEnabled;
}
#endif // RSG_PC

void CNetworkVoice::Update()
{
	// update bandwidth restrictions - done before talkers are updated
	// so bandwidth restrictions are applied in the same frame.
	if(CNetwork::IsNetworkOpen())
		UpdateBandwidthRestrictions();

	// update the collection of talkers who can hear me and whom I can hear.
	UpdateTalkers();

	// update stats
	CheckAndAddPendingTalkersForAutoMute();
	CheckAndAddPendingMutesForAutoMute();

#if RSG_DURANGO
	// track when this completes
	if(m_bRefreshingMuteList)
	{
		if(!m_MuteListStatus.Pending())
		{
			gnetDebug1("Mute List :: %s - %d entries", m_MuteListStatus.Succeeded() ? "Retrieved" : "Failed", m_MuteList.m_Players.GetCount());
			m_bRefreshingMuteList = false;

			// we might want to consider trying again if this fails
			m_bHasMuteList = m_MuteListStatus.Succeeded(); 
		}
	}
#elif RSG_ORBIS
	// rebuild mute lists if the PlayStation Home menu is opened
	static bool bWasSystemUIOpen = false;
	if (g_SystemUi.IsUiShowing() != bWasSystemUIOpen)
	{
		gnetDebug1("OnSystemUi :: Open: %s", bWasSystemUIOpen ? "False" : "True");
		bWasSystemUIOpen = !bWasSystemUIOpen;
		BuildMuteLists();
	}
#endif

	// update voice chat
	if(m_VoiceChat.IsInitialized())
	{
#if RSG_PC
		if(m_VoiceChat.GetCaptureMode() == VoiceChat::CAPTURE_MODE_PUSH_TO_TALK)
		{
			// We treat Caps Lock as a toggle for Push to Talk
			const bool isPushToTalkMappedToCapsLock = (CControlMgr::GetMainPlayerControl(false).GetInputSource(rage::INPUT_PUSH_TO_TALK, ioSource::IOMD_KEYBOARD_MOUSE, 0, false).m_Parameter == rage::KEY_CAPITAL ||
													   CControlMgr::GetMainPlayerControl(false).GetInputSource(rage::INPUT_PUSH_TO_TALK, ioSource::IOMD_KEYBOARD_MOUSE, 1, false).m_Parameter == rage::KEY_CAPITAL) ||
													  (CControlMgr::GetMainFrontendControl(false).GetInputSource(rage::INPUT_PUSH_TO_TALK, ioSource::IOMD_KEYBOARD_MOUSE, 0, false).m_Parameter == rage::KEY_CAPITAL ||
													   CControlMgr::GetMainFrontendControl(false).GetInputSource(rage::INPUT_PUSH_TO_TALK, ioSource::IOMD_KEYBOARD_MOUSE, 1, false).m_Parameter == rage::KEY_CAPITAL);

			if(isPushToTalkMappedToCapsLock && ioKeyboard::IsCapsLockToggled())
			{
				if(!m_VoiceChat.GetPushToTalk())
				{
					m_VoiceChat.SetPushToTalk(true);
				}
			}
			else
			{
				if((CControlMgr::GetMainPlayerControl().GetPushToTalk().IsDown() &&
				    CControlMgr::GetMainPlayerControl().GetPushToTalk().GetSource().m_Parameter != rage::KEY_CAPITAL) ||
				   (CControlMgr::GetMainFrontendControl().GetPushToTalk().IsDown() &&
					CControlMgr::GetMainFrontendControl().GetPushToTalk().GetSource().m_Parameter != rage::KEY_CAPITAL))
				{
					if(!m_VoiceChat.GetPushToTalk())
					{
						m_VoiceChat.SetPushToTalk(true);
					}
				}
				else if(m_VoiceChat.GetPushToTalk())
				{
					m_VoiceChat.SetPushToTalk(false);
				}
			}
		}

		CPauseMenu::UpdateVoiceMuteOnLostFocus();
#endif // RSG_PC

		m_VoiceChat.Update();
	}

	// remove stale pending gamers
	int nCount = m_PendingGamers.GetCount();
	unsigned nCurrentTime = sysTimer::GetSystemMsTime();
	for(int i = 0; i < nCount; i++)
	{
		if(nCurrentTime > m_PendingGamers[i].m_nTimestamp)
		{
			gnetDebug1("Update :: Removing Pending Gamer %s", NetworkUtils::LogGamerHandle(m_PendingGamers[i].m_Settings.GetGamerHandle()));

#if RSG_DURANGO
			if(m_PendingGamers[i].m_bPendingLeave)
			{
				// remove remote chat user and from mute list tracking
				m_VoiceChat.RemoveRemoteChatUser(m_PendingGamers[i].m_Settings.GetGamerHandle());
			}
#endif
			m_PendingGamers[i].Clear();
			m_PendingGamers.Delete(i);
			i--;
			nCount--;
		}
	}

#if __BANK
	if(m_BankDisplayBandwidthRestrictions)
	{
		DisplayBandwidthRestrictions();
	}

	if(CNetwork::IsNetworkOpen())
	{
		const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
		if (pLocalPlayer)
		{
			u32 MuteCount, TalkerCount;
			pLocalPlayer->GetMuteData(MuteCount, TalkerCount );
			m_BankLocalPlayerMuteCount = (int)MuteCount;
			m_BankLocalPlayerTalkersMet = (int)TalkerCount;
		}
	}

	m_BankTalkerCount = m_VoiceChat.GetTalkerCount();

	Bank_Update();
#endif // __BANK

#if !__NO_OUTPUT
	static bool s_bSignedIn = false;
	static bool s_bHasHeadset = false; 
	static bool s_bThruSpeakers = false; 

	int nLocalIndex = NetworkInterface::GetLocalGamerIndex();
	bool bSignedIn = (nLocalIndex >= 0);

	if(bSignedIn)
	{
		bool bJustSignedIn = (bSignedIn && !s_bSignedIn);

		const bool bHasHeadset = m_VoiceChat.HasHeadset(nLocalIndex);
		if(bJustSignedIn || (s_bHasHeadset != bHasHeadset))
		{
			gnetDebug1("Voice :: Headset %s.", bHasHeadset ? "Plugged In" : "Not Plugged In");
			s_bHasHeadset = bHasHeadset;
		}

		const bool bThruSpeakers = CProfileSettings::GetInstance().GetInt(CProfileSettings::VOICE_THRU_SPEAKERS) != 0;
		if(bJustSignedIn || (s_bThruSpeakers != bThruSpeakers))
		{
			gnetDebug1("Voice :: Voice %s", bThruSpeakers ? "Through Speakers" : "Not Through Speakers");
			s_bThruSpeakers = bThruSpeakers;
		}
	}

	// log that we were signed in
	s_bSignedIn = bSignedIn;
#endif
}

bool CNetworkVoice::SendTextMessage(const char* szTextMessage, const rlGamerHandle& hGamer)
{
	gnetDebug1("SendTextMessage :: To: %s, Text: %s", NetworkUtils::LogGamerHandle(hGamer), szTextMessage ? szTextMessage : "<null>");

	// validate text
	if(!gnetVerify(szTextMessage))
	{
		gnetError("SendTextMessage :: No message");
		return false; 
	}

	// grab text length
	unsigned nTextLength = static_cast<unsigned>(strlen(szTextMessage));

	// validate length
	if(!gnetVerify(nTextLength > 0))
	{
		gnetError("SendTextMessage :: Empty message");
		return false; 
	}

	// validate length
	if(!gnetVerify(nTextLength < MAX_TEXT_MESSAGE_LENGTH))
	{
		gnetError("SendTextMessage :: Message too long (Supplied: %u, Max: %u)", nTextLength, MAX_TEXT_MESSAGE_LENGTH);
		return false; 
	}

	// check we can communicate
	if(!CanCommunicateTextWithGamer(hGamer))
	{
		gnetDebug1("SendTextMessage :: Cannot Communicate");
		return false;
	}

	CNetworkTelemetry::RecordPhoneTextMessage(hGamer, szTextMessage);

	if(m_UsePresenceForTextMessages)
	{
		CTextMessageEvent::Trigger(&hGamer, szTextMessage);
	}
	else
	{
		MsgTextMessage msg(NetworkInterface::GetLocalGamerHandle(), szTextMessage);
		CNetwork::GetNetworkSession().SendSessionMessage(hGamer, msg);
	}

	return true;
}

void CNetworkVoice::OnTextMessageReceivedViaPresence(const char* szTextMessage, const rlGamerHandle& hFromGamer)
{
	if(!m_UsePresenceForTextMessages)
	{
		gnetDebug1("OnTextMessageReceivedViaPresence :: Presence text messaging disabled");
		return;
	}

	OnTextMessageReceived(szTextMessage, hFromGamer);
}

void CNetworkVoice::OnTextMessageReceived(const char* szTextMessage, const rlGamerHandle& hFromGamer)
{
	if(m_DisableReceivingTextMessages)
	{
		gnetDebug1("OnTextMessageReceived :: Receiving disabled");
		return;
	}

	if(hFromGamer.IsLocal())
	{
		gnetDebug1("OnTextMessageReceived :: Receiving from local! Ignoring!");
		return;
	}

	// check if we are allowed to receive from this player
	if(!CLiveManager::CanReceiveFrom(hFromGamer, RL_INVALID_CLAN_ID, m_TextAllowedSentFromGroups OUTPUT_ONLY(, "NetworkVoice")))
	{
		gnetDebug1("OnTextMessageReceived :: Not allowed to receive from %s", hFromGamer.ToString());
		return;
	}

#if RSG_XBOX
	// check we can communicate
	if(!CanCommunicateTextWithGamer(hFromGamer))
	{
		gnetDebug1("OnTextMessageReceived :: Cannot Communicate");
		return;
	}
#endif

#if RSG_DURANGO
	// check remote is not muted
	if(IsInMuteList(hFromGamer))
	{
		gnetDebug1("OnTextMessageReceived :: Sending Player Muted");
		return;
	}
#endif

	// get the physical gamer associated with this sender
	CNetGamePlayer* player = NetworkInterface::GetPlayerFromGamerHandle(hFromGamer);

	bool isSubmittedToFilter = false;
	if(IsTextFilteringEnabled() && player && player->HasValidPhysicalPlayerIndex())
	{
		const PhysicalPlayerIndex playerIndex = player->GetPhysicalPlayerIndex();
		if(gnetVerify(m_TextFilter[playerIndex].IsInitialised()))
			isSubmittedToFilter = m_TextFilter[playerIndex].AddReceivedText(hFromGamer, szTextMessage);
	}

	if(!isSubmittedToFilter)
	{
		gnetDebug1("OnTextMessageReceived :: Filter Check on Text - From: %s, Text: %s", hFromGamer.ToString(), szTextMessage);
		OnTextMessageProcessed(hFromGamer, szTextMessage);
	}
}

void CNetworkVoice::OnTextMessageProcessed(const rlGamerHandle& hFromGamer, const char* szTextMessage)
{
	// create script event
	gnetDebug1("OnTextMessageProcessed :: From: %s, Text: %s", hFromGamer.ToString(), szTextMessage);
	GetEventScriptNetworkGroup()->Add(CEventNetworkTextMessageReceived(szTextMessage, hFromGamer));

	CNetworkTelemetry::OnPhoneTextMessageReceived(hFromGamer, szTextMessage);
}

void CNetworkVoice::BuildMuteLists(const rlGamerHandle& hGamer)
{
	int nGamers = m_Gamers.GetCount();
	if(hGamer.IsValid())
	{
		// ignore for local gamer
		if(hGamer.IsLocal())
			return;

		for(int i = 0; i < nGamers; i++)
		{
			if(hGamer == m_Gamers[i].GetGamerHandle())
			{
				BuildMuteListForGamer(i);
				break;
			}
		}
	}
	else
	{
		for(int i = 0; i < nGamers; i++)
		{
			// ignore for local gamer
			if(m_Gamers[i].IsLocal())
				continue;

			BuildMuteListForGamer(i);
		}
	}
}

void CNetworkVoice::BuildMuteListForGamer(const int nGamerIndex)
{
	// validate index
	if(nGamerIndex < 0 || nGamerIndex >= m_Gamers.GetCount())
		return;

	bool bIsMutedByMe = false;                
	bool bIsBlockedByMe = false;

#if !__NO_OUTPUT
	unsigned nLocalVoiceFlags = m_Gamers[nGamerIndex].GetLocalVoiceFlags();
	unsigned nRemoteVoiceFlags = m_Gamers[nGamerIndex].GetRemoteVoiceFlags();
#endif
	
	bool bGlobalIsMuted = false;
	bool bGlobalIsBlocked = false;
	bool bGlobalIsTextBlocked = false;
	bool bSystemIsMuted = false;

#if RSG_NP
	// if the home menu is showing, turn on the system mute (if tunable says so)
	bSystemIsMuted |= (g_SystemUi.IsUiShowing() && m_bNpMuteWhenUIShowing);
#elif RSG_XBOX
	// on Xbox One, players are muted when we are constrained (turn on the system mute)
	bSystemIsMuted |= m_bIsConstrained;

	// check global permissions
	if(m_Gamers[nGamerIndex].m_VoiceCommunicationCheck.HasResult() && !m_Gamers[nGamerIndex].m_VoiceCommunicationCheck.HasPermission())
		bGlobalIsMuted = true;
	if(m_Gamers[nGamerIndex].m_TextCommunicationCheck.HasResult() && !m_Gamers[nGamerIndex].m_TextCommunicationCheck.HasPermission())
		bGlobalIsTextBlocked = true;
#endif

	// run through *all* local users and check if any one of them is muting the remote player
	// possibly update vs. local speaker flags - but this is accepted by TCR
	for(int j = 0; j < RL_MAX_LOCAL_GAMERS; j++) if(rlPresence::IsSignedIn(j))
	{
#if RSG_ORBIS
		// on PS4, only the active profile counts for this
		if(j != NetworkInterface::GetLocalGamerIndex())
			continue;
#endif

#if !RSG_DURANGO
		// get muted status - on Xbox One, this setting is unreliable so ignore it
		bIsMutedByMe = m_VoiceChat.IsMuted(m_Gamers[nGamerIndex].GetGamerHandle());
#else
		// pull from mute / block list
		bIsMutedByMe |= m_Gamers[nGamerIndex].m_bIsMuted;
		bIsBlockedByMe |= m_Gamers[nGamerIndex].m_bIsBlocked;
#endif

		// get blocked status
		bIsBlockedByMe |= !m_VoiceChat.HasChatPrivileges(j, m_Gamers[nGamerIndex].GetGamerHandle());

		// update flags
		bGlobalIsBlocked |= bIsBlockedByMe;
		bGlobalIsMuted |= bIsMutedByMe;
	}

	// update flags
	m_Gamers[nGamerIndex].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_MUTED, bGlobalIsMuted);
	m_Gamers[nGamerIndex].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_BLOCKED, bGlobalIsBlocked);
	m_Gamers[nGamerIndex].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_SYSTEM_MUTE, bSystemIsMuted);
	m_Gamers[nGamerIndex].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_TEXT_BLOCKED, bGlobalIsTextBlocked);
	m_Gamers[nGamerIndex].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_ON_BLOCK_LIST, CLiveManager::IsInBlockList(m_Gamers[nGamerIndex].GetGamerHandle()));
	m_Gamers[nGamerIndex].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_UGC_BLOCKED, !CLiveManager::CheckUserContentPrivileges());

	// logging if the flags have been changed
#if !__NO_OUTPUT
	if((m_Gamers[nGamerIndex].GetLocalVoiceFlags() != nLocalVoiceFlags) || (m_Gamers[nGamerIndex].GetRemoteVoiceFlags() != nRemoteVoiceFlags))
		gnetDebug1("BuildMuteListForGamer :: %s, Local: 0x%x -> 0x%x, Remote: 0x%x -> 0x%x", NetworkUtils::LogGamerHandle(m_Gamers[nGamerIndex].GetGamerHandle()), nLocalVoiceFlags, m_Gamers[nGamerIndex].GetLocalVoiceFlags(), nRemoteVoiceFlags, m_Gamers[nGamerIndex].GetRemoteVoiceFlags());
#endif
}

void CNetworkVoice::PlayerHasJoined(const rlGamerInfo& gamerInfo, const  EndpointId DURANGO_ONLY(endpointId))
{
	gnetAssertf(!(IsEnabled() && !m_HasAttemptedInitialize), "PlayerHasJoined called when voice system has not been Initialized");
	if(!m_IsInitialized)
	{
		gnetDebug1("PlayerHasJoined :: Not adding %s, Voice not Initialised", gamerInfo.GetName());
		return;
	}

	// create settings
	if(gamerInfo.IsLocal())
	{
		// check if we have a copy
		if(!m_bHasLocalGamer)
		{
			m_LocalGamer = VoiceGamerSettings(gamerInfo.GetGamerHandle(), true);
			m_bHasLocalGamer = true; 

			// add chat user for Xbox One
			DURANGO_ONLY(m_VoiceChat.AddLocalChatUser(gamerInfo.GetLocalIndex()));
		}
		else
			m_LocalGamer.IncrementRefCount();

		// logging
		gnetDebug1("PlayerHasJoined :: Local gamer references: %d", m_LocalGamer.GetNumReferences());
	}
	else
	{
		VoiceGamerSettings* pSettings = FindGamer(gamerInfo.GetGamerHandle());
		if(pSettings)
		{
			// increment count and set the voice flags to dirty - we might know about this player
			// but timing in the session flow might mean that the remote player does not know about us
			pSettings->IncrementRefCount();
			pSettings->SetDirtyVoiceFlags();
			gnetDebug1("PlayerHasJoined :: Incrementing VoiceGamerSettings references for %s. Count: %d", gamerInfo.GetName(), pSettings->GetNumReferences());

			// build mute lists to account for this new player
			BuildMuteLists(pSettings->GetGamerHandle());
		}
		else
		{
			VoiceGamerSettings tSettings(gamerInfo.GetGamerHandle(), false);

			// track whether we need to add this talker for our mute tracking
			bool needMuteTracking = true;

			// pending entry for this player - grab the flags
			int nCount = m_PendingGamers.GetCount();
			for(int i = 0; i < nCount; i++)
			{
				if(m_PendingGamers[i].m_Settings.GetGamerHandle() == gamerInfo.GetGamerHandle())
				{
					gnetDebug1("PlayerHasJoined :: Found pending VoiceGamerSettings for %s. Flags: 0x%x", gamerInfo.GetName(), m_PendingGamers[i].m_Settings.m_nVoiceFlags);
					tSettings.m_nVoiceFlags = m_PendingGamers[i].m_Settings.m_nVoiceFlags;
					m_PendingGamers[i].Clear();
					m_PendingGamers.Delete(i);

					// restored flags - we need to set the dirty state since this won't change
					// when we apply the local settings
					tSettings.SetDirtyVoiceFlags();

					// found pending settings, we added this talker there
					needMuteTracking = false;

					break;
				}
			}

			// only add if not full
			if(gnetVerify(!m_Gamers.IsFull()))
			{
#if RSG_DURANGO
				// update XBO mute / block settings once (list could grow large)
				tSettings.m_bIsMuted = IsInMuteList(tSettings.GetGamerHandle());
				tSettings.m_bIsBlocked = CLiveManager::IsInAvoidList(tSettings.GetGamerHandle());

				// we need to add for GameChat
				m_VoiceChat.AddRemoteChatUser(gamerInfo, endpointId);
#endif
				// add gamer and build mute lists to account for this new player
				m_Gamers.Push(tSettings);

				// add talker for mute list stats
				if(needMuteTracking)
					m_PendingTalkersForAutoMute++;

				// log at the end to get the updated pool size
				gnetDebug1("PlayerHasJoined :: Adding VoiceGamerSettings for %s, Handle: %s. Pool Count: %d", gamerInfo.GetName(), NetworkUtils::LogGamerHandle(gamerInfo.GetGamerHandle()), m_Gamers.GetCount());

#if RSG_DURANGO
				// grab settings reference - need to this as the netStatus* needs to be the array copy
				VoiceGamerSettings& tNewSettings = m_Gamers.Top();

				// store friends setting
				tNewSettings.m_bIsFriend = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), tNewSettings.GetGamerHandle());

				// for user content, the platform check is broken (friends / blocked grouped together) 
				// if we have full privileges, skip a service check but otherwise we'll need to do this
				if(CLiveManager::CheckUserContentPrivileges())
					tNewSettings.m_UserContentCheck.SetResult(true);
				else if(!tNewSettings.m_bIsFriend)
					tNewSettings.m_UserContentCheck.SetResult(false);
				else
					tNewSettings.m_UserContentCheck.FlagForCheck();

				if(m_IssueCommunicationCheck)
				{
					// for communication, the check is broken the other way (friends / everyone grouped together)
					// we'll need to issue a check for everyone that is not a friend
					if(!CLiveManager::CheckVoiceCommunicationPrivileges())
					{
						tNewSettings.m_VoiceCommunicationCheck.SetResult(false);
					}
					else if(tNewSettings.m_bIsFriend)
					{
						tNewSettings.m_VoiceCommunicationCheck.SetResult(true);
					}
					else 
					{
						tNewSettings.m_VoiceCommunicationCheck.FlagForCheck();
					}

					// the same checks for text
					if(!CLiveManager::CheckTextCommunicationPrivileges())
					{
						tNewSettings.m_TextCommunicationCheck.SetResult(false);
					}
					else if(tNewSettings.m_bIsFriend)
					{
						tNewSettings.m_TextCommunicationCheck.SetResult(true);
					}
					else 
					{
						tNewSettings.m_TextCommunicationCheck.FlagForCheck();
					}
				}
				else
				{
					// just assume true
					tNewSettings.m_VoiceCommunicationCheck.SetResult(true);
					tNewSettings.m_TextCommunicationCheck.SetResult(true);
				}
				
				// log permissions
				gnetDebug1("PlayerHasJoined :: Permissions - Muted: %s, Blocked: %s, Friends: %s, UserContent: %s, Voice: %s, Text: %s, OnBlockList: %s", 
							tNewSettings.m_bIsMuted ? "True" : "False", 
							tNewSettings.m_bIsBlocked ? "True" : "False", 
							tNewSettings.m_bIsFriend ? "True" : "False", 
							tNewSettings.m_UserContentCheck.HasResult() ? (tNewSettings.m_UserContentCheck.HasPermission() ? "True" : "False") : (tNewSettings.m_UserContentCheck.Pending() ? "Pending" : "Unknown"),	
							tNewSettings.m_VoiceCommunicationCheck.HasResult() ? (tNewSettings.m_VoiceCommunicationCheck.HasPermission() ? "True" : "False") : (tNewSettings.m_VoiceCommunicationCheck.Pending() ? "Pending" : "Unknown"),	
							tNewSettings.m_TextCommunicationCheck.HasResult() ? (tNewSettings.m_TextCommunicationCheck.HasPermission() ? "True" : "False") : (tNewSettings.m_TextCommunicationCheck.Pending() ? "Pending" : "Unknown"),
							CLiveManager::IsInBlockList(tSettings.GetGamerHandle()) ? "True" : "False");
#else
				// log permissions
				gnetDebug1("PlayerHasJoined :: Permissions - Muted: %s, Blocked: %s, Friends: %s, UserContent: %s, Voice: %s, Text: %s, OnBlockList: %s", 
							m_VoiceChat.IsMuted(tSettings.GetGamerHandle()) ? "True" : "False",
							m_VoiceChat.IsMuted(tSettings.GetGamerHandle()) ? "True" : "False", // same thing
							rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), tSettings.GetGamerHandle()) ? "True" : "False",
							CLiveManager::CheckUserContentPrivileges() ? "True" : "False",
							CLiveManager::CheckVoiceCommunicationPrivileges() ? "True" : "False",
							CLiveManager::CheckTextCommunicationPrivileges() ? "True" : "False",
							CLiveManager::IsInBlockList(tSettings.GetGamerHandle()) ? "True" : "False");
#endif

				// build mute lists after all of our permissions have been setup
				BuildMuteLists(tSettings.GetGamerHandle());
			}
			else
			{
				gnetError("PlayerHasJoined :: Gamer Pool is Full!");
			}
		}
	}	
}

void CNetworkVoice::PlayerHasLeft(const rlGamerInfo& gamerInfo)
{
	gnetAssertf(!(IsEnabled() && !m_HasAttemptedInitialize), "PlayerHasLeft called when voice system has not been Initialized");
	if(!m_IsInitialized)
	{
		gnetDebug1("PlayerHasLeft :: Not removing %s, Voice not Initialised", gamerInfo.GetName());
		return; 
	}

	// check if this is a local gamer
	if(gamerInfo.IsLocal())
	{  
		// check if we have a copy
		if(gnetVerify(m_bHasLocalGamer))
		{
			m_LocalGamer.DecrementRefCount();
			if(!m_LocalGamer.HasReferences())
			{
				gnetDebug1("PlayerHasLeft :: References 0 for local gamer %s.", gamerInfo.GetName());
				m_bHasLocalGamer = false;
				m_LocalGamer.Clear();

				// local player
				DURANGO_ONLY(m_VoiceChat.RemoveLocalChatUser(gamerInfo.GetLocalIndex());)
			} 
			else
				gnetDebug1("PlayerHasLeft :: Decrementing references for local gamer %s. Count: %d", gamerInfo.GetName(), m_LocalGamer.GetNumReferences());
		}
	}
	else
	{
		VoiceGamerSettings* pSettings = FindGamer(gamerInfo.GetGamerHandle());
		if(pSettings)
		{
			if(gnetVerify(pSettings->HasReferences()))
			{
				pSettings->DecrementRefCount();

				if(!pSettings->HasReferences())
				{
					bool bFound = false;

					// remove
					int nGamers = m_Gamers.GetCount();
					for(int i = 0; i < nGamers; i++)
					{
						if(m_Gamers[i].GetGamerHandle() == pSettings->GetGamerHandle())
						{
							// pending gamer structure
							PendingVoiceGamer tPendingVoiceGamer;
							tPendingVoiceGamer.m_Settings = VoiceGamerSettings(m_Gamers[i].GetGamerHandle(), false);
							tPendingVoiceGamer.m_Settings.m_nVoiceFlags = m_Gamers[i].m_nVoiceFlags;
							tPendingVoiceGamer.m_nTimestamp = sysTimer::GetSystemMsTime() + MAX_DELETE_COOLDOWN;
							tPendingVoiceGamer.m_bPendingLeave = true;

							DURANGO_ONLY(bool bAddedPendingGamer =)
								AddPendingGamer(tPendingVoiceGamer OUTPUT_ONLY(, "PlayerHasLeft"));
#if RSG_DURANGO
							if(bAddedPendingGamer)
								m_VoiceChat.SetChatUserAsPendingRemove(m_Gamers[i].GetGamerHandle()); 
							else
								m_VoiceChat.RemoveRemoteChatUser(m_Gamers[i].GetGamerHandle());

							// cancel any in flight permission checks
							if(m_Gamers[i].m_VoiceCommunicationCheck.IsStatusPending())
							{
								gnetDebug1("PlayerHasLeft :: Pending Voice Communication Check (Status: %p), Cancelling...", m_Gamers[i].m_VoiceCommunicationCheck.m_Status);
								netTask::Cancel(m_Gamers[i].m_VoiceCommunicationCheck.m_Status);
							}
							if(m_Gamers[i].m_TextCommunicationCheck.IsStatusPending())
							{
								gnetDebug1("PlayerHasLeft :: Pending Text Communication Check (Status: %p), Cancelling...", m_Gamers[i].m_TextCommunicationCheck.m_Status);
								netTask::Cancel(m_Gamers[i].m_TextCommunicationCheck.m_Status);
							}	
							if(m_Gamers[i].m_UserContentCheck.IsStatusPending())
							{
								gnetDebug1("PlayerHasLeft :: Pending User Content Check (Status: %p), Cancelling...", m_Gamers[i].m_UserContentCheck.m_Status);
								netTask::Cancel(m_Gamers[i].m_UserContentCheck.m_Status);
							}
#endif
							// remove the gamer
							m_Gamers[i].Clear();
							m_Gamers.Delete(i);
							bFound = true; 
							break;
						}
					}

					// logging
					gnetDebug1("PlayerHasLeft :: References 0 for %s. Removing. Found: %s, Pool Count: %d", gamerInfo.GetName(), bFound ? "True" : "False", m_Gamers.GetCount());
				}
				else
					gnetDebug1("PlayerHasLeft :: Decrementing VoiceGamerSettings references for %s. Count: %d", gamerInfo.GetName(), pSettings->GetNumReferences());
			}
		}
		else
		{
			gnetWarning("PlayerHasLeft :: No settings found for %s", gamerInfo.GetName());
		}
	}
}

void CNetworkVoice::PlayerHasJoinedBubble(const rlGamerInfo& gamerInfo)
{
	gnetAssertf(!(IsEnabled() && !m_HasAttemptedInitialize), "PlayerHasJoinedBubble called when voice system has not been Initialized");
	if(!m_IsInitialized)
	{
		gnetDebug1("PlayerHasJoinedBubble :: Not adding %s, Voice not Initialised", gamerInfo.GetName());
		return; 
	}

	gnetDebug1("PlayerHasJoinedBubble :: %s has joined", gamerInfo.GetName());

	// if this is the local player, reset team only overrides
	if(gamerInfo.IsLocal())
	{
		m_OverrideChatRestrictions = 0;
		m_OverrideChatRestrictionsSendLocal = 0;
		m_OverrideChatRestrictionsReceiveLocal = 0;
		m_OverrideChatRestrictionsSendRemote = 0;
		m_OverrideChatRestrictionsReceiveRemote = 0;
		m_OverrideTutorialRestrictions = 0;
		m_nOverrideTransitionRestrictions = 0;
	}
	else
	{
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(gamerInfo.GetGamerId());
		if(!gnetVerifyf(pPlayer, "PlayerHasJoinedBubble :: Player does not exist for %s", gamerInfo.GetName()))
			return;

		PhysicalPlayerIndex nPPI = pPlayer->GetPhysicalPlayerIndex();
		if(!gnetVerifyf(nPPI < MAX_NUM_PHYSICAL_PLAYERS, "PlayerHasJoinedBubble :: Player index invalid for %s", gamerInfo.GetName()))
			return;

		gnetDebug1("PlayerHasJoinedBubble :: PhysicalIndex: %d", nPPI);

		// look up this players voice settings - we may have an active override
		VoiceGamerSettings* pSettings = FindGamer(pPlayer->GetGamerInfo().GetGamerHandle());
		SetOverrideChatRestrictions(nPPI, pSettings ? pSettings->GetOverrideChatRestrictions() : false);
		SetOverrideChatRestrictionsSend(nPPI, false);
		SetOverrideChatRestrictionsReceive(nPPI, false);

#if !__NO_OUTPUT
		m_nAddReason[nPPI] = REASON_INVALID;
		m_nRemoveReason[nPPI] = REASON_INVALID;
		m_nChangeFlagsReason[nPPI] = REASON_INVALID;
		m_nRemoteTeam[nPPI] = -1;
		m_bRemoteSpectating[nPPI] = false;
#endif

		// initialise our text filter
		if(IsTextFilteringEnabled())
		{
			m_TextFilter[nPPI].Init(
				MakeFunctor(*this, &CNetworkVoice::OnTextMessageProcessed),
				pPlayer->GetGamerInfo().GetGamerHandle(),
				m_FilterHoldingMaxRequests,
				m_FilterHoldingPenIntervalMs,
				m_FilterSpamMaxRequests,
				m_FilterSpamIntervalMs
				OUTPUT_ONLY(, "TextMessage"));
		}

		m_DisableBandwidthRestrictions &= (1 << nPPI);
		m_DisableBandwidthVoiceTime[nPPI] = 0;
	}
}

void CNetworkVoice::PlayerHasLeftBubble(const rlGamerInfo& gamerInfo)
{
	gnetAssertf(!(IsEnabled() && !m_HasAttemptedInitialize), "PlayerHasLeftBubble called when voice system has not been Initialized");
	if(!m_IsInitialized)
	{
		gnetDebug1("PlayerHasLeftBubble :: Not removing %s, Voice not Initialised", gamerInfo.GetName());
		return; 
	}

	gnetDebug1("PlayerHasLeftBubble :: %s has left", gamerInfo.GetName());

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerId(gamerInfo.GetGamerId());
	if(!gnetVerifyf(pPlayer, "PlayerHasLeftBubble :: Player does not exist for %s", gamerInfo.GetName()))
		return;

	PhysicalPlayerIndex nPPI = pPlayer->GetPhysicalPlayerIndex();
	if(!gnetVerifyf(nPPI < MAX_NUM_PHYSICAL_PLAYERS, "PlayerHasLeftBubble :: Player index invalid for %s", gamerInfo.GetName()))
		return;

	gnetDebug1("PlayerHasLeftBubble :: PhysicalIndex: %d", nPPI);

	SetOverrideChatRestrictions(nPPI, false);
	SetOverrideChatRestrictionsSend(nPPI, false);
	SetOverrideChatRestrictionsReceive(nPPI, false);

	m_DisableBandwidthRestrictions &= (1<<nPPI);
	m_DisableBandwidthVoiceTime[nPPI] = 0;

	// if this is our current focus talker - clear out
	if(m_FocusTalker == gamerInfo.GetGamerHandle())
	{
		gnetDebug1("PlayerHasLeftBubble :: Focus Talker, clearing!");
		m_FocusTalker.Clear();
	}

	if(IsTextFilteringEnabled())
	{
		// shutdown the text filter
		m_TextFilter[nPPI].Shutdown();
	}
}

const VoiceGamerSettings* CNetworkVoice::FindGamer(const rlGamerHandle& hGamer) const
{
	// check local gamer
	if(hGamer == m_LocalGamer.GetGamerHandle())
		return &m_LocalGamer;

	// then remote gamers
	int nGamers = m_Gamers.GetCount();
	for(int i = 0; i < nGamers; i++)
	{
		if(m_Gamers[i].GetGamerHandle() == hGamer)
			return &m_Gamers[i];
	}

	// not found
	return nullptr;
}

VoiceGamerSettings* CNetworkVoice::FindGamer(const rlGamerHandle& hGamer)
{
	// check local gamer
	if(hGamer == m_LocalGamer.GetGamerHandle())
		return &m_LocalGamer;

	// then remote gamers
	int nGamers = m_Gamers.GetCount();
	for(int i = 0; i < nGamers; i++)
	{
		if(m_Gamers[i].GetGamerHandle() == hGamer)
			return &m_Gamers[i];
	}

	// not found
	return NULL;
}

void CNetworkVoice::SetVoiceBandwidthLimit(unsigned nLimit)
{
	gnetAssertf(!(IsEnabled() && !m_HasAttemptedInitialize), "SetVoiceBandwidthLimit called when voice system has not been initialized");
	if(m_IsInitialized)
	{
#if !__NO_OUTPUT
		if(m_VoiceBandwidthLimit != nLimit)
			gnetDebug1("SetVoiceBandwidthLimit :: Setting limit to %d", nLimit);
#endif
		m_VoiceBandwidthLimit = nLimit; 

		// reset channel policies - make sure that compression is disabled
		gnetAssert(!m_VoicePolicies.m_CompressionEnabled);
		m_VoicePolicies.m_OutboundBandwidthLimit = m_VoiceBandwidthLimit;
		m_CxnMgr->SetChannelPolicies(NETWORK_VOICE_CHANNEL_ID, m_VoicePolicies);
	}
}

unsigned CNetworkVoice::GetVoiceBandwidthLimit()
{
	return m_VoiceBandwidthLimit;
}

bool CNetworkVoice::IsLocalTalking(const int nLocalIndex) const
{
	rlGamerId nGamerID;
	rlPresence::GetGamerId(nLocalIndex, &nGamerID);

	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.IsTalking(nGamerID);
}

bool CNetworkVoice::IsGamerTalking(const rlGamerId& gamerId) const
{
	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.IsTalking(gamerId);
}

bool CNetworkVoice::IsAnyGamerTalking() const
{
	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.IsAnyTalking();
}

bool CNetworkVoice::IsAnyRemoteGamerTalking() const
{
	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.IsAnyRemoteTalking();
}

bool CNetworkVoice::IsAnyGamerAboutToTalk() const
{
	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.IsAnyAboutToTalk();
}

bool CNetworkVoice::IsAnyRemoteGamerAboutToTalk() const
{
	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.IsAnyRemoteAboutToTalk();
}

bool CNetworkVoice::LocalHasHeadset(const int nLocalIndex) const
{
#if RSG_OUTPUT
	if(PARAM_voiceForceHeadset.Get())
		return true; 
#endif

	return m_VoiceChat.IsInitialized()
		&& m_VoiceChat.HasHeadset(nLocalIndex);
}

bool CNetworkVoice::GamerHasHeadset(const rlGamerHandle& hGamer) const
{
	if(!IsEnabled())
		return false;

#if RSG_OUTPUT
	if(PARAM_voiceForceHeadset.Get())
		return true; 
#endif

	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && pGamer->HasHeadset();
}

bool CNetworkVoice::MuteGamer(const rlGamerHandle& hGamer, const bool bIsMuted)
{
	if(!IsEnabled())
		return false;

	VoiceGamerSettings* pGamer = FindGamer(hGamer);
	if(!gnetVerify(pGamer))
		return false;

	bool bIsCurrentlyMuted = ((pGamer->GetLocalVoiceFlags() & VoiceGamerSettings::VOICE_LOCAL_FORCE_MUTED) != 0);
	if(bIsCurrentlyMuted != bIsMuted)
	{
		gnetDebug1("MuteGamer :: %s %s", bIsMuted ? "Muting" : "Unmuting", NetworkUtils::LogGamerHandle(pGamer->GetGamerHandle()));
		pGamer->SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_FORCE_MUTED, bIsMuted);
	}

	return true;
}

bool CNetworkVoice::ToggleMute(const rlGamerHandle& hGamer)
{
	if(!IsEnabled())
		return false;

	VoiceGamerSettings* pGamer = FindGamer(hGamer);
	if(!gnetVerify(pGamer))
		return false;

	bool bToggleSetting = !((pGamer->GetLocalVoiceFlags() & VoiceGamerSettings::VOICE_LOCAL_FORCE_MUTED) != 0);

	gnetDebug1("ToggleMute :: %s %s", bToggleSetting ? "Muting" : "Unmuting", NetworkUtils::LogGamerHandle(pGamer->GetGamerHandle()));
	pGamer->SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_FORCE_MUTED, bToggleSetting);

	return true;
}

bool CNetworkVoice::CanCommunicateTextWithGamer(const rlGamerHandle& hGamer) const 
{
	if(!IsEnabled())
		return false;

	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && pGamer->CanCommunicateTextWith();
}

bool CNetworkVoice::HasGamerRecord(const rlGamerHandle& hGamer) const
{
	return FindGamer(hGamer) != nullptr;
}

bool CNetworkVoice::CanCommunicateVoiceWithGamer(const rlGamerHandle& hGamer) const
{
	if(!IsEnabled())
		return false;

	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && pGamer->CanCommunicateVoiceWith();
}

bool CNetworkVoice::IsGamerMutedByMe(const rlGamerHandle& hGamer) const
{
	if(!IsEnabled())
		return false;

	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && !pGamer->IsLocal() && 
#if RSG_DURANGO
		// On Durango, all gamers are muted when the system is constrained.
		// Only return if the user has been muted from the game.
		pGamer->IsForceMutedByMe();
#else
		pGamer->IsMutedByMe();
#endif
}

bool CNetworkVoice::AmIMutedByGamer(const rlGamerHandle& hGamer) const
{
	if(!IsEnabled())
		return false;

	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer &&
#if RSG_DURANGO
	pGamer->IsForceMutedByHim();
#else
	pGamer->IsMutedByHim();
#endif
}

bool CNetworkVoice::IsGamerBlockedByMe(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && pGamer->IsBlockedByMe();
}

bool CNetworkVoice::AmIBlockedByGamer(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && pGamer->IsBlockedByHim();
}

bool CNetworkVoice::CanViewGamerUserContent(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return (pGamer != nullptr) && pGamer->CanViewUserContent();
}

bool CNetworkVoice::HasViewGamerUserContentResult(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return (pGamer != nullptr) && pGamer->HasUserContentCheckResult();
}

bool CNetworkVoice::CanPlayMultiplayerWithGamer(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return (pGamer != nullptr) && pGamer->CanPlayMultiplayer();
}

bool CNetworkVoice::CanGamerPlayMultiplayerWithMe(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return pGamer && pGamer->CanPlayMultiplayerWithMe();
}

bool CNetworkVoice::CanSendInvite(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return (pGamer != nullptr)
#if RSG_PROSPERO
		&& !pGamer->IsLocalOnRemoteBlocklist() && !pGamer->IsRemoteOnLocalBlocklist()
#endif
		;
}

bool CNetworkVoice::CanReceiveInvite(const rlGamerHandle& hGamer) const
{
	const VoiceGamerSettings* pGamer = FindGamer(hGamer);
	return (pGamer != nullptr)
#if RSG_PROSPERO
		&& !pGamer->IsLocalOnRemoteBlocklist() && !pGamer->IsRemoteOnLocalBlocklist()
#endif
		;
}

void CNetworkVoice::OnFriendListChanged()
{
	gnetDebug1("OnFriendListChanged");

#if RSG_DURANGO
	// remove
	int nGamers = m_Gamers.GetCount();
	for(int i = 0; i < nGamers; i++)
	{
		// track changes in friend status for players in our session
		bool bIsFriend = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), m_Gamers[i].GetGamerHandle());
		if(bIsFriend != m_Gamers[i].m_bIsFriend)
		{
			gnetDebug1("OnFriendListChanged : Friend status changed for %s", NetworkUtils::LogGamerHandle(m_Gamers[i].GetGamerHandle()));

			// for communication, a positive result groups friends / everyone
			if(m_IssueCommunicationCheck)
			{
				if(CLiveManager::CheckVoiceCommunicationPrivileges())
					m_Gamers[i].m_VoiceCommunicationCheck.FlagForCheck();
				if(CLiveManager::CheckTextCommunicationPrivileges())
					m_Gamers[i].m_TextCommunicationCheck.FlagForCheck();
			}

			// for content, a negative result groups friends / blocked
			if(!CLiveManager::CheckUserContentPrivileges())
				m_Gamers[i].m_UserContentCheck.FlagForCheck();

			m_Gamers[i].m_bIsFriend = bIsFriend;
		}
	}
#else
	BuildMuteLists();
#endif
}

#if RSG_DURANGO
void CNetworkVoice::OnAvoidListRetrieved()
{
	gnetDebug1("OnAvoidListRetrieved");

	// check all gamers
	int nGamers = m_Gamers.GetCount();
	for(int i = 0; i < nGamers; i++)
	{
		// don't do this for local
		if(m_Gamers[i].IsLocal())
			continue;

		bool bIsBlocked = CLiveManager::IsInAvoidList(m_Gamers[i].GetGamerHandle());
		if(bIsBlocked != m_Gamers[i].m_bIsBlocked)
		{
			m_Gamers[i].m_bIsBlocked = bIsBlocked;
			BuildMuteLists(m_Gamers[i].GetGamerHandle());
		}
	}
}

void CNetworkVoice::OnConstrained()
{
	gnetDebug1("OnConstrained");

	// mark constrained and build mute lists
	m_bIsConstrained = true;
	BuildMuteLists();
}

void CNetworkVoice::OnUnconstrained()
{
	gnetDebug1("OnUnconstrained");

	// mark unconstrained and build mute lists
	m_bIsConstrained = false;
	BuildMuteLists();
}
#endif

void CNetworkVoice::SetTalkerFocus(const rlGamerHandle* gamerHandle)
{
	gnetAssertf(!(IsEnabled() && !m_HasAttemptedInitialize), "SetTalkerFocus called when voice system has not been Initialized");
	if(m_IsInitialized)
	{
		if(gamerHandle)
		{
			if(*gamerHandle != m_FocusTalker)
			{
				netPlayer* player = NetworkInterface::GetPlayerFromGamerHandle(*gamerHandle);
				if(AssertVerify(player))
				{
					gnetDebug1("SetTalkerFocus :: Setting focus to %s", player->GetLogName());
					m_FocusTalker = *gamerHandle;
				}
			}
		}
		else
		{
#if !__NO_OUTPUT
			if(m_FocusTalker.IsValid())
				gnetDebug1("SetTalkerFocus :: Clearing");
#endif
			m_FocusTalker.Clear();
		}
	}
}

#define VOICE_PROXIMITY_EPSILON (0.0001f)

void CNetworkVoice::SetTalkerProximity(const float fProximity)
{
	gnetAssertf(!(fProximity > VOICE_PROXIMITY_EPSILON && IsEnabled() && !m_HasAttemptedInitialize), "SetTalkerProximity :: Applying non-default when voice system has not been initialized");
	if(m_IsInitialized)
	{
#if !__NO_OUTPUT
		if(Abs(m_TalkerProximityLo - fProximity) > VOICE_PROXIMITY_EPSILON)
			gnetDebug1("SetTalkerProximity :: Setting proximity to %.2f", fProximity);
#endif
		m_TalkerProximityLo = fProximity;
		m_TalkerProximityHi = fProximity + DEFAULT_DEAD_ZONE;
		m_TalkerProximityLoSq = m_TalkerProximityLo * m_TalkerProximityLo;
		m_TalkerProximityHiSq = m_TalkerProximityHi * m_TalkerProximityHi; 
	}
}

void CNetworkVoice::SetTalkerProximityHeight(const float fProximity)
{
	gnetAssertf(!(fProximity > VOICE_PROXIMITY_EPSILON && IsEnabled() && !m_HasAttemptedInitialize), "SetTalkerProximityHeight :: Applying non-default when voice system has not been initialized");
	if(m_IsInitialized)
	{
#if !__NO_OUTPUT
		if(Abs(m_TalkerProximityHeightLo - fProximity) > VOICE_PROXIMITY_EPSILON)
			gnetDebug1("SetTalkerProximityHeight :: Setting proximity to %.2f", fProximity);
#endif
		m_TalkerProximityHeightLo = fProximity;
		m_TalkerProximityHeightHi = fProximity + DEFAULT_DEAD_ZONE;
		m_TalkerProximityHeightLoSq = m_TalkerProximityHeightLo * m_TalkerProximityHeightLo;
		m_TalkerProximityHeightHiSq = m_TalkerProximityHeightHi * m_TalkerProximityHeightHi; 
	}
}

float CNetworkVoice::GetTalkerProximity() const
{
	return m_TalkerProximityLo;
}

void CNetworkVoice::SetLoudhailerProximity(const float fProximity)
{
	gnetAssertf(!(fProximity > VOICE_PROXIMITY_EPSILON && IsEnabled() && !m_HasAttemptedInitialize), "SetLoudhailerProximity :: Applying non-default when voice system has not been initialized");
	if(m_IsInitialized)
	{
#if !__NO_OUTPUT
		if(Abs(m_LoudhailerProximityLo - fProximity) > VOICE_PROXIMITY_EPSILON)
			gnetDebug1("SetLoudhailerProximity :: Setting proximity to %.2f", fProximity);
#endif
		m_LoudhailerProximityLo = fProximity;
		m_LoudhailerProximityHi = fProximity + DEFAULT_DEAD_ZONE;
		m_LoudhailerProximityLoSq = m_LoudhailerProximityLo * m_LoudhailerProximityLo;
		m_LoudhailerProximityHiSq = m_LoudhailerProximityHi * m_LoudhailerProximityHi; 
	}
}

void CNetworkVoice::SetLoudhailerProximityHeight(const float fProximity)
{
	gnetAssertf(!(fProximity > VOICE_PROXIMITY_EPSILON && IsEnabled() && !m_HasAttemptedInitialize), "SetLoudhailerProximityHeight :: Applying non-default when voice system has not been initialized");
	if(m_IsInitialized)
	{
#if !__NO_OUTPUT
		if(Abs(m_LoudhailerProximityHeightLo - fProximity) > VOICE_PROXIMITY_EPSILON)
			gnetDebug1("SetLoudhailerProximity :: Setting proximity to %.2f", fProximity);
#endif
		m_LoudhailerProximityHeightLo = fProximity;
		m_LoudhailerProximityHeightHi = fProximity + DEFAULT_DEAD_ZONE;
		m_LoudhailerProximityHeightLoSq = m_LoudhailerProximityHeightLo * m_LoudhailerProximityHeightLo;
		m_LoudhailerProximityHeightHiSq = m_LoudhailerProximityHeightHi * m_LoudhailerProximityHeightHi; 
	}
}

float CNetworkVoice::GetLoudhailerProximity() const
{
	return m_LoudhailerProximityLo;
}

void CNetworkVoice::SetOverrideAllRestrictions(const bool bOverride)
{
	if(m_bOverrideAllRestrictions != bOverride)
	{
		gnetDebug1("SetOverrideAllRestrictions :: Overriding All Restrictions: %s", bOverride ? "True" : "False");
		m_bOverrideAllRestrictions = bOverride;
	}
}

void CNetworkVoice::SetCommunicationActive(const bool bIsCommunicationActive)
{
	if(m_bIsCommunicationActive != bIsCommunicationActive)
	{
		gnetDebug1("SetCommunicationActive :: Setting Communication %s", bIsCommunicationActive ? "Active" : "Inactive");
		m_bIsCommunicationActive = bIsCommunicationActive;
	}
}

void CNetworkVoice::SetRemainInGameChat(bool bRemainInGameChat)
{
	if(m_bRemainInGameChat != bRemainInGameChat)
	{
		gnetDebug1("SetRemainInGameChat :: Setting to %s", bRemainInGameChat ? "Active" : "Inactive");
		m_bRemainInGameChat = bRemainInGameChat;
	}
}

void CNetworkVoice::SetOverrideTransitionChatScript(const bool bOverride)
{
	if(m_bOverrideTransitionChatScript != bOverride)
	{
		gnetDebug1("SetOverrideTransitionChatScript :: Setting to %s", bOverride ? "Active" : "Inactive");
		m_bOverrideTransitionChatScript = bOverride;
	}
}

void CNetworkVoice::SetOverrideTransitionRestrictions(const int nPlayerID, const bool bOverride)
{
	// applies the local override - this is sent across the network and sets the remote send override
	ApplyPlayerRestrictions(m_nOverrideTransitionRestrictions, nPlayerID, bOverride, "SetOverrideTransitionRestrictions");
}

bool CNetworkVoice::GetOverrideTransitionRestrictions(const int nPlayerID) const
{
	// check both local and remote here
	gnetAssert(nPlayerID >= 0 && nPlayerID < MAX_NUM_PHYSICAL_PLAYERS);
	return m_bOverrideTransitionChat ||
		m_bOverrideTransitionChatScript ||
		((m_nOverrideTransitionRestrictions & (1 << nPlayerID)) != 0);
}

void CNetworkVoice::SetTeamOnlyChat(const bool bTeamOnlyChat)
{
	gnetAssertf(!(bTeamOnlyChat && IsEnabled() && !m_HasAttemptedInitialize), "SetTeamOnlyChat :: Setting to true when voice system not initialised!");
	if(!m_IsInitialized)
		return;

	if(m_TeamOnlyChat != bTeamOnlyChat)
	{
		gnetDebug1("SetTeamOnlyChat :: Setting Team Only Chat %s", bTeamOnlyChat ? "Active" : "Inactive");
		m_TeamOnlyChat = bTeamOnlyChat;
	}
}

void CNetworkVoice::SetTeamChatIncludeUnassignedTeams(const bool bIncludeUnassignedTeams)
{
	if(!m_IsInitialized)
		return;

	if(m_TeamChatIncludeUnassignedTeams != bIncludeUnassignedTeams)
	{
		gnetDebug1("SetTeamChatIncludeUnassignedTeams :: Setting to %s", bIncludeUnassignedTeams ? "Active" : "Inactive");
		m_TeamChatIncludeUnassignedTeams = bIncludeUnassignedTeams;
	}
}

void CNetworkVoice::SetProximityAffectsReceive(const bool bEnable)
{ 
	if(m_ProximityAffectsReceive != bEnable)
	{
		gnetDebug1("SetProximityAffectsReceive :: Setting proximity affects receive to %s", bEnable ? "Active" : "Inactive");
		m_ProximityAffectsReceive = bEnable; 
	}
}

void CNetworkVoice::SetProximityAffectsSend(const bool bEnable)
{ 
	if(m_ProximityAffectsSend != bEnable)
	{
		gnetDebug1("SetProximityAffectsSend :: Setting proximity affects send to %s", bEnable ? "Active" : "Inactive");
		m_ProximityAffectsSend = bEnable; 
	}
}

void CNetworkVoice::SetProximityAffectsTeam(const bool bEnable)
{ 
	if(m_ProximityAffectsTeam != bEnable)
	{
		gnetDebug1("SetProximityAffectsTeam :: Setting proximity affects team to %s", bEnable ? "Active" : "Inactive");
		m_ProximityAffectsTeam = bEnable; 
	}
}

void CNetworkVoice::SetNoSpectatorChat(const bool bEnable)
{ 
	if(m_NoSpectatorChat != bEnable)
	{
		gnetDebug1("SetNoSpectatorChat :: Setting no spectator chat to %s", bEnable ? "Active" : "Inactive");
		m_NoSpectatorChat = bEnable; 
	}
}

void CNetworkVoice::SetIgnoreSpectatorLimitsForSameTeam(const bool bEnable)
{
	if(m_bIgnoreSpectatorLimitsSameTeam != bEnable)
	{
		gnetDebug1("SetIgnoreSpectatorLimitsForSameTeam :: Setting to %s", bEnable ? "Active" : "Inactive");
		m_bIgnoreSpectatorLimitsSameTeam = bEnable; 
	}
}

void CNetworkVoice::SetOverrideTeam(const int nTeam, const bool bOverride)
{
	gnetAssertf(!(bOverride && IsEnabled() && !m_HasAttemptedInitialize), "SetOverrideTeam :: Setting to true when voice system not initialised!");
	if(!m_IsInitialized)
		return;

	if(!gnetVerify(nTeam >= 0 && nTeam < MAX_NUM_TEAMS))
		return;

	if(bOverride != GetOverrideTeam(nTeam))
	{
		gnetDebug1("SetOverrideTeam :: Setting chat override for team %d to %s", nTeam, bOverride ? "Active" : "Inactive");
		if(bOverride)
			m_OverrideTeamRestrictions |= (1 << nTeam);
		else
			m_OverrideTeamRestrictions &= ~(1 << nTeam);
	}
}

bool CNetworkVoice::GetOverrideTeam(const int nTeam) const
{
	gnetAssert(nTeam >= 0 && nTeam < MAX_NUM_TEAMS);
	return (m_OverrideTeamRestrictions & (1 << nTeam)) != 0;
}

void CNetworkVoice::ApplyPlayerRestrictions(PlayerFlags& nFlags, const int nPlayerID, const bool bOverride, const char* OUTPUT_ONLY(szFunctionName))
{
	gnetAssertf(!(bOverride && IsEnabled() && !m_HasAttemptedInitialize), "%s :: Setting to true when voice system not initialised!", szFunctionName);
	if(!m_IsInitialized)
		return;

	if(!gnetVerify(nPlayerID >= 0 && nPlayerID < MAX_NUM_PHYSICAL_PLAYERS))
		return;

#if !__NO_OUTPUT
	netPlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(nPlayerID));
	if(!gnetVerifyf(pPlayer, "%s :: Invalid player", szFunctionName))
		return;
#endif 

	bool bCurrent = ((nFlags & (1 << nPlayerID)) != 0);

	// apply setting
	if(bOverride != bCurrent)
	{
		gnetDebug1("%s :: Setting chat override for %s to %s", szFunctionName, pPlayer->GetLogName(), bOverride ? "Active" : "Inactive");
		if(bOverride)
			nFlags |= (1 << nPlayerID);
		else
			nFlags &= ~(1 << nPlayerID);
	}
}

void CNetworkVoice::SetOverrideChatRestrictions(const int nPlayerID, const bool bOverride)
{
	ApplyPlayerRestrictions(m_OverrideChatRestrictions, nPlayerID, bOverride, "SetOverrideChatRestrictions");

	// we need to track this in the gamer settings so that we can take these rules between transitions
	netPlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(nPlayerID));
	if(pPlayer)
	{
		VoiceGamerSettings* pSettings = FindGamer(pPlayer->GetGamerInfo().GetGamerHandle());
		if(pSettings)
			pSettings->SetOverrideChatRestrictions(bOverride);
}
}

bool CNetworkVoice::GetOverrideChatRestrictions(const int nPlayerID) const
{
	gnetAssert(nPlayerID >= 0 && nPlayerID < MAX_NUM_PHYSICAL_PLAYERS);
	return (m_OverrideChatRestrictions & (1 << nPlayerID)) != 0;
}

void CNetworkVoice::SetOverrideChatRestrictionsSend(const int nPlayerID, const bool bOverride)
{
	// applies the local override - this is sent across the network and sets the remote receive override
	ApplyPlayerRestrictions(m_OverrideChatRestrictionsSendLocal, nPlayerID, bOverride, "SetOverrideChatRestrictionsSend");
}

bool CNetworkVoice::GetOverrideChatRestrictionsSend(const int nPlayerID) const
{
	// check both local and remote here
	gnetAssert(nPlayerID >= 0 && nPlayerID < MAX_NUM_PHYSICAL_PLAYERS);
	return m_bOverrideAllSendRestrictions ||
		((m_OverrideChatRestrictionsSendLocal & (1 << nPlayerID)) != 0) ||
		((m_OverrideChatRestrictionsSendRemote & (1 << nPlayerID)) != 0);
}

void CNetworkVoice::SetOverrideChatRestrictionsReceive(const int nPlayerID, const bool bOverride)
{
	// applies the local override - this is sent across the network and sets the remote send override
	ApplyPlayerRestrictions(m_OverrideChatRestrictionsReceiveLocal, nPlayerID, bOverride, "SetOverrideChatRestrictionsReceive");
}

bool CNetworkVoice::GetOverrideChatRestrictionsReceive(const int nPlayerID) const
{
	// check both local and remote here
	gnetAssert(nPlayerID >= 0 && nPlayerID < MAX_NUM_PHYSICAL_PLAYERS);
	return m_bOverrideAllReceiveRestrictions ||
		((m_OverrideChatRestrictionsReceiveLocal & (1 << nPlayerID)) != 0) ||
		((m_OverrideChatRestrictionsReceiveRemote & (1 << nPlayerID)) != 0);
}

void CNetworkVoice::SetOverrideSpectatorMode(const bool bOverride)
{ 
	if(m_OverrideSpectatorMode != bOverride)
	{
		gnetDebug1("SetOverrideSpectatorMode :: Setting spectator override to %s", bOverride ? "Active" : "Inactive");
		m_OverrideSpectatorMode = bOverride; 
	}
}

void CNetworkVoice::SetOverrideTutorialSession(const bool bOverride)
{ 
	if(m_OverrideTutorialSession != bOverride)
	{
		gnetDebug1("SetOverrideTutorialSession :: Setting tutorial override to %s", bOverride ? "Active" : "Inactive");
		m_OverrideTutorialSession = bOverride; 
	}
}

void CNetworkVoice::SetOverrideTutorialRestrictions(const int nPlayerID, const bool bOverride)
{
	// applies the local override - this is sent across the network and sets the remote send override
	ApplyPlayerRestrictions(m_OverrideTutorialRestrictions, nPlayerID, bOverride, "SetOverrideTutorialRestrictions");
}

bool CNetworkVoice::GetOverrideTutorialRestrictions(const int nPlayerID) const
{
	// check both local and remote here
	gnetAssert(nPlayerID >= 0 && nPlayerID < MAX_NUM_PHYSICAL_PLAYERS);
	return m_OverrideTutorialSession ||
		((m_OverrideTutorialRestrictions & (1 << nPlayerID)) != 0);
}

void CNetworkVoice::SetOverrideChatRestrictionsSendAll(const bool bOverride)
{ 
	if(m_bOverrideAllSendRestrictions != bOverride)
	{
		gnetDebug1("SetOverrideChatRestrictionsSendAll :: Setting send all override to %s", bOverride ? "Active" : "Inactive");
		m_bOverrideAllSendRestrictions = bOverride; 
	}
}

void CNetworkVoice::SetOverrideChatRestrictionsReceiveAll(const bool bOverride)
{ 
	if(m_bOverrideAllReceiveRestrictions != bOverride)
	{
		gnetDebug1("SetOverrideChatRestrictionsReceiveAll :: Setting receive all override to %s", bOverride ? "Active" : "Inactive");
		m_bOverrideAllReceiveRestrictions = bOverride; 
	}
}

void CNetworkVoice::ApplyReceiveBitfield(const int nFromPlayerID, PlayerFlags nPlayerBitField)
{
	CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
	if(!gnetVerify(pMyPlayer))
		return;

	// applies the remote override for sending (remote player has asked to receive)
	ApplyPlayerRestrictions(m_OverrideChatRestrictionsSendRemote, nFromPlayerID, ((nPlayerBitField & (1 << pMyPlayer->GetPhysicalPlayerIndex())) != 0), "ApplyReceiveBitfield");
}

void CNetworkVoice::BuildReceiveBitfield(PlayerFlags& nPlayerBitField) const
{
	nPlayerBitField = 0;

	// check the physical iterator - we are only checking vs. fully joined players
	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const CNetGamePlayer* pRemotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
		if(pRemotePlayer->IsBot())
			continue; 

		if(m_bOverrideAllReceiveRestrictions || ((m_OverrideChatRestrictionsReceiveLocal & (1 << pRemotePlayer->GetPhysicalPlayerIndex())) != 0))
			nPlayerBitField |= (1 << pRemotePlayer->GetPhysicalPlayerIndex());
	}
}

void CNetworkVoice::ApplySendBitfield(const int nFromPlayerID, PlayerFlags nPlayerBitField)
{
	CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
	if(!gnetVerify(pMyPlayer))
		return;

	// applies the remote override for receiving (remote player has asked to send)
	ApplyPlayerRestrictions(m_OverrideChatRestrictionsReceiveRemote, nFromPlayerID, ((nPlayerBitField & (1 << pMyPlayer->GetPhysicalPlayerIndex())) != 0), "ApplySendBitfield");
}

void CNetworkVoice::BuildSendBitfield(PlayerFlags& nPlayerBitField) const
{
	nPlayerBitField = 0;

	// check the physical iterator - we are only checking vs. fully joined players
	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const CNetGamePlayer* pRemotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
		if(pRemotePlayer->IsBot())
			continue; 

		if(m_bOverrideAllSendRestrictions || ((m_OverrideChatRestrictionsSendLocal & (1 << pRemotePlayer->GetPhysicalPlayerIndex())) != 0))
			nPlayerBitField |= (1 << pRemotePlayer->GetPhysicalPlayerIndex());
	}
}

float CNetworkVoice::GetPlayerLoudness(const int localPlayerIndex)
{
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(!pInfo)
		return 0.0f;

	if(!m_VoiceChat.IsInitialized() || !m_VoiceChat.IsTalking(pInfo->GetGamerId()))
		return 0.0f;

	return m_VoiceChat.GetLoudness(localPlayerIndex);
}

void CNetworkVoice::EnableBandwidthRestrictions(PhysicalPlayerIndex playerIndex)
{
	ApplyPlayerRestrictions(m_DisableBandwidthRestrictions, playerIndex, false, "EnableBandwidthRestrictions");
}

void CNetworkVoice::DisableBandwidthRestrictions(PhysicalPlayerIndex playerIndex)
{
	ApplyPlayerRestrictions(m_DisableBandwidthRestrictions, playerIndex, true, "DisableBandwidthRestrictions");

	if(gnetVerifyf(playerIndex != INVALID_PLAYER_INDEX, "Invalid physical player index!"))
		m_DisableBandwidthVoiceTime[playerIndex] = 0;
}

void CNetworkVoice::SetScriptAllowUniDirectionalOverrideForNegativeProximity(const bool bAllowUniDirectionalOverrideForNegativeProximity)
{
	if(m_bScriptAllowUniDirectionalOverrideForNegativeProximity != bAllowUniDirectionalOverrideForNegativeProximity)
	{
		gnetDebug1("SetScriptAllowUniDirectionalOverrideForNegativeProximity :: Setting to %s", bAllowUniDirectionalOverrideForNegativeProximity ? "True" : "False");
		m_bScriptAllowUniDirectionalOverrideForNegativeProximity = bAllowUniDirectionalOverrideForNegativeProximity;
	}
}

//private:

#if RSG_DURANGO
void CNetworkVoice::OnChatEvent(const rlXblChatEvent* pEvent)
{
	gnetDebug1("OnChatEvent :: %s", pEvent->GetAutoIdNameFromId(pEvent->GetId()));

	switch(pEvent->GetId())
	{
	case RLXBL_CHAT_EVENT_USER_MUTE_STATE_CHANGED:
		{
			const rlXblChatEventUserMuteStateChanged* pMuteStateChanged = static_cast<const rlXblChatEventUserMuteStateChanged*>(pEvent);

			// modify the muted list directly
			if(pMuteStateChanged->IsMuted())
			{
				if(m_MuteList.m_Players.Find(pMuteStateChanged->GetRecipientXUID()) < 0)
					m_MuteList.m_Players.PushAndGrow(pMuteStateChanged->GetRecipientXUID());
			}
			else
				m_MuteList.m_Players.DeleteMatches(pMuteStateChanged->GetRecipientXUID());

			// logging
			gnetDebug1("OnChatEvent :: XUID: %" I64FMT "u, Muted: %s, MuteListCount: %d", pMuteStateChanged->GetRecipientXUID(), pMuteStateChanged->IsMuted() ? "True" : "False", m_MuteList.m_Players.GetCount());

			// check if this is a gamer we are in current communication with and update mute lists
			rlGamerHandle hGamer;
			hGamer.ResetXbl(pMuteStateChanged->GetRecipientXUID());

			VoiceGamerSettings* pGamer = FindGamer(hGamer);
			if(pGamer)
			{
				gnetDebug1("OnChatEvent :: XUID: %" I64FMT "u is active, refreshing restriction checks", pMuteStateChanged->GetRecipientXUID());
				pGamer->m_bIsMuted = pMuteStateChanged->IsMuted();
				pGamer->m_VoiceCommunicationCheck.SetResult(!pGamer->m_bIsMuted);
				BuildMuteLists(hGamer);
			}
		}
		break;

	default:
		break;
	}
}

bool CNetworkVoice::IsInMuteList(const rlGamerHandle& hGamer)
{
	return m_MuteList.m_Players.Find(hGamer.GetXuid()) >= 0;
}
#endif

void CNetworkVoice::OnConnectionEvent(netConnectionManager* UNUSED_PARAM(pCxnMgr), const netEvent* pEvent)
{
	switch(pEvent->GetId())
	{
		// a frame of data was received.
	case NET_EVENT_FRAME_RECEIVED:
		{
			const netEventFrameReceived* pFrame = pEvent->m_FrameReceived;

			unsigned nMsgID;
			if(!netMessage::GetId(&nMsgID, pFrame->m_Payload, pFrame->m_SizeofPayload))
				break;

			if(nMsgID == MsgTextMessage::MSG_ID())
			{
				// verify message contents
				MsgTextMessage sMsg;
				if(!gnetVerify(sMsg.Import(pFrame->m_Payload, pFrame->m_SizeofPayload)))
					break;

				if(m_UsePresenceForTextMessages)
				{
					gnetDebug1("OnConnectionEvent :: Received Text - Disabled via P2P");
					break;
				}

				// get the player for this endpoint
				CNetGamePlayer* remotePlayer = NetworkInterface::GetPlayerFromEndpointId(pFrame->GetEndpointId());
				if(!remotePlayer)
				{
					gnetDebug1("OnConnectionEvent :: Received Text - Invalid player");
					break;
				}

				// make sure we aren't spoofing the player in the message
				if(remotePlayer->GetGamerInfo().GetGamerHandle() != sMsg.m_hFromGamer)
				{
					gnetDebug1("OnConnectionEvent :: Received Text - Player mismatched");
					break;
				}

				gnetDebug1("OnConnectionEvent :: Received Text - Message: %s, From: %s", sMsg.m_szTextMessage, NetworkUtils::LogGamerHandle(sMsg.m_hFromGamer));
				OnTextMessageReceived(sMsg.m_szTextMessage, sMsg.m_hFromGamer);
			}
			else if(nMsgID == CMsgVoiceStatus::MSG_ID())
			{
				// verify message contents
				CMsgVoiceStatus sMsg;
				if(!gnetVerify(sMsg.Import(pFrame->m_Payload, pFrame->m_SizeofPayload)))
					break;

				VoiceGamerSettings* pGamer = FindGamer(sMsg.m_hFromGamer);
				if(!pGamer)
				{
					// don't know about this player yet - will be along shortly
					PendingVoiceGamer tPendingVoiceGamer;
					tPendingVoiceGamer.m_Settings = VoiceGamerSettings(sMsg.m_hFromGamer, false);
					tPendingVoiceGamer.m_nTimestamp = sysTimer::GetSystemMsTime() + MAX_PENDING_TIME;

					// add this player for mute tracking
					m_PendingTalkersForAutoMute++;

					// add to pending list
					if(AddPendingGamer(tPendingVoiceGamer OUTPUT_ONLY(, "OnConnectionEvent")))
						pGamer = &m_PendingGamers.Top().m_Settings;
				}

				// if we have a gamer
				if(pGamer)
				{
					gnetDebug1("OnConnectionEvent :: MsgVoiceStatus received. Gamer: %s, Flags: 0x%x", NetworkUtils::LogGamerHandle(pGamer->m_hGamer), sMsg.m_VoiceFlags);

					bool bHeHadMeMuted = pGamer->IsMutedByHim();
					DURANGO_ONLY(bool bHeHadMeBlocked = pGamer->IsBlockedByHim());

					// convert from remote flags
					unsigned nRemoteFlags = VoiceGamerSettings::LocalToRemoteVoiceFlags(sMsg.m_VoiceFlags);
					pGamer->SetRemoteVoiceFlags(nRemoteFlags);

					// see if I've been muted by this guy.
#if RSG_DURANGO
					if((!bHeHadMeMuted && pGamer->IsForceMutedByHim()) || (!bHeHadMeBlocked && pGamer->IsBlockedByHim()))
#else
					if(!bHeHadMeMuted && pGamer->IsMutedByHim())
#endif
					{
						// prevent toggling the mute from thrashing the stats
						if(!pGamer->HasTrackedForAutomute())
						{
							// add mute for mute list stats
							gnetDebug1("OnConnectionEvent :: Muted / Blocked by %s. Adding to mute count!", NetworkUtils::LogGamerHandle(pGamer->m_hGamer));
							pGamer->SetTrackedForAutomute();
							m_PendingMutesForAutoMute++;
						}
					}

					// rebuild mute lists with this new information
					BuildMuteLists(pGamer->GetGamerHandle());
				}
			}
		}
		break;
	}
}

void CNetworkVoice::OnVoiceChatEventReceived( const VoiceChat::TalkerEvent* /*pEvent*/ )
{

}

bool CNetworkVoice::AddPendingGamer(const PendingVoiceGamer& tPendingVoiceGamer OUTPUT_ONLY(, const char* szSource))
{
	// check if this player exists in pending gamer array
	int nPendingGamers = m_PendingGamers.GetCount();
	for(int i = 0; i < nPendingGamers; i++)
	{
		if(tPendingVoiceGamer.m_Settings.GetGamerHandle() == m_PendingGamers[i].m_Settings.GetGamerHandle())
		{
			// update existing copy
			m_PendingGamers[i].m_Settings.m_nVoiceFlags = tPendingVoiceGamer.m_Settings.m_nVoiceFlags;
			m_PendingGamers[i].m_nTimestamp = tPendingVoiceGamer.m_nTimestamp;
			m_PendingGamers[i].m_bPendingLeave = tPendingVoiceGamer.m_bPendingLeave;

			// we've found this player, just exit
			return true;
		}
	}

	// add if we have space
	if(!gnetVerifyf(!m_PendingGamers.IsFull(), "AddPendingGamer :: %s - Array is full!", szSource))
		return false;

	// add the gamer 
	m_PendingGamers.Push(tPendingVoiceGamer);
	gnetDebug1("AddPendingGamer :: %s - Handle: %s, Flags: 0x%x, Count: %d", szSource, NetworkUtils::LogGamerHandle(tPendingVoiceGamer.m_Settings.GetGamerHandle()), tPendingVoiceGamer.m_Settings.GetLocalVoiceFlags(), m_PendingGamers.GetCount());

	// success
	return true;
}

#if RSG_DURANGO
#if !__NO_OUTPUT
const char* GetPermissionString(IPrivacySettings::ePermissions nPermissionID)
{
	if(nPermissionID == IPrivacySettings::PERM_CommunicateUsingVoice)
		return "VoiceCommunication";
	if(nPermissionID == IPrivacySettings::PERM_CommunicateUsingText)
		return "TextCommunication";
	else if(nPermissionID == IPrivacySettings::PERM_ViewTargetUserCreatedContent)
		return "ViewUserContent";
	else if(nPermissionID == IPrivacySettings::PERM_PlayMultiplayer)
		return "Multiplayer";

	return "Invalid";
}
#endif

void VoiceGamerSettings::PermissionCheck::Clear()
{
	m_bHasResult = false;
	m_bIsAllowed = false;
	m_bResultPending = false;
	m_bRefreshPending = false;
	if(m_Status != nullptr)
	{
		gnetDebug1("PermissionCheck::Clear :: Deleting Status: %p", m_Status);
		delete m_Status; 
		m_Status = nullptr;
	}
}

void VoiceGamerSettings::PermissionCheck::FlagForCheck()
{
	if(m_Status == nullptr)
	{
		m_Status = rage_new netStatus();
	}

	gnetDebug1("PermissionCheck::FlagForCheck :: Flagging %s for Handle: %s (Status: %p)", GetPermissionString(m_nPermissionID), NetworkUtils::LogGamerHandle(m_hGamer), m_Status);
	m_bRefreshPending = true;
}

void VoiceGamerSettings::PermissionCheck::CheckPermission()
{
	gnetDebug1("PermissionCheck::CheckPermission :: Checking %s for Handle: %s (Status: %p)", GetPermissionString(m_nPermissionID), NetworkUtils::LogGamerHandle(m_hGamer), m_Status);
	IPrivacySettings::CheckPermission(NetworkInterface::GetLocalGamerIndex(), m_nPermissionID, m_hGamer, &m_Result, m_Status);
	m_bResultPending = true; 
	m_bRefreshPending = false;
}

void VoiceGamerSettings::PermissionCheck::Update()
{
	if(m_bRefreshPending && !IsStatusPending())
		CheckPermission();

	// update permissions check
	if(m_bResultPending)
	{
		if(!IsStatusPending())
		{
			// if this is the first result, or the result is different than previous
			if(!m_bHasResult || (m_bIsAllowed != m_Result.m_isAllowed))
			{
				gnetDebug1("PermissionCheck::Update :: %s - %s. Handle: %s, Status: %p, Allowed: %s, DenyReason: %s", 
						   GetPermissionString(m_nPermissionID), m_Status && m_Status->Succeeded() ? "Succeeded" : "Failed", 
						   NetworkUtils::LogGamerHandle(m_hGamer), 
						   m_Status,
						   m_Result.m_isAllowed ? "True" : "False", 
						   m_Result.m_isAllowed ? "None" : m_Result.m_denyReason);

				m_bIsAllowed = m_Result.m_isAllowed;
				m_bHasResult = true;
				
				// hacktacular but will do for now
				if(m_nPermissionID == IPrivacySettings::PERM_CommunicateUsingVoice)
					CNetwork::GetVoice().BuildMuteLists(m_hGamer);
			}

			m_bResultPending = false;
		}
	}
}
#endif

void CNetworkVoice::UpdateVoiceStatus()
{
#if RSG_DURANGO
	static unsigned s_nLastTime = 0;
	unsigned nCurrentTime = sysTimer::GetSystemMsTime();
#endif

	if(!CNetwork::IsNetworkOpen())
		return;

	if(!CNetwork::GetPlayerMgr().IsInitialized())
		return;

	// must have local gamer
	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if(!pLocalPlayer)
		return;

	// must have local voice gamer
	if(!m_bHasLocalGamer)
		return;

	const int nLocalGamerIndex = pLocalPlayer->GetGamerInfo().GetLocalIndex();

	// the message that we send to players to signal changes in our voice status.
	CMsgVoiceStatus tVoiceStatus;

	// grab and set these now - these will also be used to notify remote players about our local status
	bool bHasHeadset = m_VoiceChat.HasHeadset(nLocalGamerIndex);
#if RSG_PC
	const bool bThruSpeakers = m_VoiceChat.IsPlaybackDeviceStarted();
#else
	const bool bThruSpeakers = CProfileSettings::GetInstance().GetInt(CProfileSettings::VOICE_THRU_SPEAKERS) != 0;
#endif // RSG_PC
	const bool bAutoMute = (CLiveManager::GetSCGamerDataMgr().IsAutoMuted() || CNetwork::IsScriptAutoMuted()) && !CLiveManager::GetSCGamerDataMgr().HasAutoMuteOverride();

#if RSG_OUTPUT
	if(PARAM_voiceForceHeadset.Get())
		bHasHeadset = true; 
#endif

	// apply headset / speaker flags for the local player
	m_LocalGamer.SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_HAS_HEADSET, bHasHeadset);
	m_LocalGamer.SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_THRU_SPEAKERS, bThruSpeakers);
	m_LocalGamer.SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_AUTO_MUTE, bAutoMute);

	if(IsTextFilteringEnabled())
	{
		// update all text filters to release any released text from it's holding pen
		const unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer* const* remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			if(!remotePhysicalPlayers[index]->HasValidPhysicalPlayerIndex())
				continue;

			const PhysicalPlayerIndex physicalPlayerIndex = remotePhysicalPlayers[index]->GetPhysicalPlayerIndex();
			m_TextFilter[physicalPlayerIndex].Update();
		}
	}
	
	// update permissions
#if RSG_SCARLETT
	s_permissionChecks->Update();
#endif

	// check all gamers
	int nGamers = m_Gamers.GetCount();
	for(int i = 0; i < nGamers; i++)
	{
		// don't do this for local
		if(m_Gamers[i].IsLocal())
			continue;

#if RSG_DURANGO
		// update permissions
		m_Gamers[i].m_VoiceCommunicationCheck.Update();
		m_Gamers[i].m_TextCommunicationCheck.Update();
		m_Gamers[i].m_UserContentCheck.Update();
#endif

		// apply headset / speaker flags so that we inform remote players
		m_Gamers[i].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_HAS_HEADSET, bHasHeadset);
		m_Gamers[i].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_THRU_SPEAKERS, bThruSpeakers);
		m_Gamers[i].SetLocalVoiceFlag(VoiceGamerSettings::VOICE_LOCAL_AUTO_MUTE, bAutoMute);

		// as long as we have a connection to this player, update the voice flags
		if(m_Gamers[i].AreVoiceFlagsDirty())
		{
			// these will be converted to remote flags at the other side
			tVoiceStatus.m_VoiceFlags = m_Gamers[i].GetLocalVoiceFlags();
			tVoiceStatus.m_hFromGamer = m_LocalGamer.GetGamerHandle();

			// logging
			gnetDebug1("UpdateVoiceStatus :: Sending gamer settings to %s. Flags: 0x%x", NetworkUtils::LogGamerHandle(m_Gamers[i].GetGamerHandle()), tVoiceStatus.m_VoiceFlags);

			// send to session
			if(CNetwork::GetNetworkSession().IsSessionMember(m_Gamers[i].GetGamerHandle()))
			{
				CNetwork::GetNetworkSession().SendSessionMessage(m_Gamers[i].GetGamerHandle(), tVoiceStatus);
				BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
			}

			// send to transition
			if(CNetwork::GetNetworkSession().IsTransitionMember(m_Gamers[i].GetGamerHandle()))
			{
				CNetwork::GetNetworkSession().SendTransitionMessage(m_Gamers[i].GetGamerHandle(), tVoiceStatus);
				BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
			}

			// send to voice
			if(CNetwork::GetNetworkVoiceSession().IsInSession(m_Gamers[i].GetGamerHandle()))
			{
				CNetwork::GetNetworkVoiceSession().SendMessage(m_Gamers[i].GetGamerHandle(), tVoiceStatus);
				BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
			}

			m_Gamers[i].CleanDirtyVoiceFlags();
		}
	}

#if RSG_DURANGO
	s_nLastTime = nCurrentTime;
#endif
}

#if !__NO_OUTPUT
static const char* gs_szTalkerReasons[] =
{
	// reasons to add
	"ADD_ALL_TESTS_PASSED",				// ADD_ALL_TESTS_PASSED
	"ADD_IS_FOCUS_TALKER",				// ADD_IS_FOCUS_TALKER
	"ADD_OVERRIDE_RESTRICTIONS",		// ADD_OVERRIDE_RESTRICTIONS
	"ADD_LOCAL_LOUDHAILER",				// ADD_LOCAL_LOUDHAILER
	"ADD_REMOTE_LOUDHAILER",			// ADD_REMOTE_LOUDHAILER
	// reasons to remove
	"REMOVE_UNKNOWN",					// REMOVE_UNKNOWN
	"REMOVE_NOT_REMOVED",				// REMOVE_NOT_REMOVED
	"REMOVE_NOT_CONNECTED",				// REMOVE_NOT_CONNECTED
	"REMOVE_NO_GAMER_ENTRY",            // REMOVE_NO_GAMER_ENTRY
	"REMOVE_BLOCKED",					// REMOVE_BLOCKED
	"REMOVE_AUTO_MUTED",				// REMOVE_AUTO_MUTED
	"REMOVE_HARDWARE",					// REMOVE_HARDWARE
	"REMOVE_COMMUNICATIONS_INACTIVE",	// REMOVE_COMMUNICATIONS_INACTIVE
	"REMOVE_NOT_FOCUS_TALKER",			// REMOVE_NOT_FOCUS_TALKER
	"REMOVE_BANDWIDTH",					// REMOVE_BANDWIDTH
	"REMOVE_NO_PLAYER_PED",             // REMOVE_NO_PLAYER_PED
	"REMOVE_IN_TRANSITION",				// REMOVE_IN_TRANSITION
	"REMOVE_DIFFERENT_TUTORIAL",		// REMOVE_DIFFERENT_TUTORIAL
	"REMOVE_DIFFERENT_VOICE_CHANNEL",	// REMOVE_DIFFERENT_VOICE_CHANNEL
	"REMOVE_SPECTATOR_CHAT_DISABLED",	// REMOVE_SPECTATOR_CHAT_DISABLED
	"REMOVE_NEGATIVE_PROXIMITY",		// REMOVE_NEGATIVE_PROXIMITY
	"REMOVE_DIFFERENT_TEAMS",			// REMOVE_DIFFERENT_TEAMS
	"REMOVE_OUTSIDE_PROXIMITY",			// REMOVE_OUTSIDE_PROXIMITY
	// reasons to change flags
	"CHANGE_FLAGS_NOT_CHANGED",			// CHANGE_FLAGS_NOT_CHANGED
	"CHANGE_FLAGS_LOCAL_SPECTATING",	// CHANGE_FLAGS_LOCAL_SPECTATING
	"CHANGE_FLAGS_REMOTE_SPECTATING",	// CHANGE_FLAGS_REMOTE_SPECTATING
	"CHANGE_FLAGS_LOCAL_LOUDHAILER",	// CHANGE_FLAGS_LOCAL_LOUDHAILER
	"CHANGE_FLAGS_REMOTE_LOUDHAILER",	// CHANGE_FLAGS_REMOTE_LOUDHAILER
	"CHANGE_FLAGS_OVERRIDE_SEND",		// CHANGE_FLAGS_OVERRIDE_SEND
	"CHANGE_FLAGS_OVERRIDE_RECEIVE",	// CHANGE_FLAGS_OVERRIDE_RECEIVE
	"CHANGE_FLAGS_OVERRIDE_BOTH",		// CHANGE_FLAGS_OVERRIDE_BOTH
};

void CNetworkVoice::SetAddTalkerReason(bool bCanChat, eTalkerReason& nMyReason, eTalkerReason nReason)
{
	if(bCanChat)
		nMyReason = nReason; 
}

void CNetworkVoice::SetRemoveTalkerReason(bool bCanChat, eTalkerReason& nMyReason, int& nMyLocalSetting, int& nMyRemoteSetting, eTalkerReason nReason, int nLocalSetting, int nRemoteSetting)
{
	if(!bCanChat)
	{
		nMyReason = nReason;
		nMyLocalSetting = nLocalSetting;
		nMyRemoteSetting = nRemoteSetting;
	}
}

void CNetworkVoice::SetChangeFlagsReason(bool bCanChat, eTalkerReason& nMyReason, eTalkerReason nReason)
{
	if(bCanChat)
		nMyReason = nReason; 
}
#endif

bool CNetworkVoice::CanAddTalker()
{
	const unsigned nTalkers = m_VoiceChat.GetTalkerCount();
	return ((nTalkers == 0) || ((nTalkers - 1) < VoiceChatTypes::MAX_REMOTE_TALKERS));
}

void CNetworkVoice::UpdateTalkers()
{
	OUTPUT_ONLY(CompileTimeAssert(COUNTOF(gs_szTalkerReasons) == REASON_NUM));

	// bail out if we've not initialised
	if(!m_VoiceChat.IsInitialized())
		return;

	// update voice flags and inform remote users of our setup
	UpdateVoiceStatus();

	// manage this flag each frame
	m_bOverrideTransitionChat = false;

	// work out session type
	eVoiceGroup nVoiceGroup = VOICEGROUP_NONE;
	if(CNetwork::GetNetworkVoiceSession().IsInVoiceSession())
		nVoiceGroup = VOICEGROUP_VOICE;
	else if(CNetwork::GetNetworkSession().IsActivityStarting())
		nVoiceGroup = VOICEGROUP_ACTIVITY_STARTING;
	else if(CNetwork::GetNetworkSession().IsActivitySession())
		nVoiceGroup = VOICEGROUP_GAME;
	else if(CNetwork::GetNetworkSession().IsTransitionEstablished() && !CNetwork::GetNetworkSession().IsTransitionLeaving() && !IsRemainingInGameChat())
		nVoiceGroup = VOICEGROUP_TRANSITION;
	else if(CNetwork::GetNetworkSession().IsSessionCreated())
		nVoiceGroup = VOICEGROUP_GAME;
	else if(NetworkInterface::IsLocalPlayerOnline())
		nVoiceGroup = VOICEGROUP_SINGLEPLAYER;

	// override transition flag
#if !RSG_ORBIS 
	m_bOverrideTransitionChat = ((CNetwork::GetNetworkSession().IsTransitionHostPending() || CNetwork::GetNetworkSession().IsTransitionHost()) && CNetwork::GetNetworkSession().GetTransitionMemberCount() <= 1);
#endif

#if RSG_OUTPUT
	bool firstFrameInNewGroup = false; 
#endif

	// if we've changed session types - remove everyone
	if(nVoiceGroup != m_nVoiceGroup)
	{
		// remove all talkers - new relevant talkers will be added by code below
		m_VoiceChat.RemoveAllTalkers();
		m_AudioEntity.RemoveAllTalkers();
		m_LastTalkers.Reset();

#if !__NO_OUTPUT
		static const char* gs_szVoiceGroupNames[VOICEGROUP_NUM] =
		{
			"VOICEGROUP_NONE",				// VOICEGROUP_NONE
			"VOICEGROUP_VOICE",				// VOICEGROUP_VOICE
			"VOICEGROUP_TRANSITION",		// VOICEGROUP_TRANSITION
			"VOICEGROUP_GAME",				// VOICEGROUP_GAME
			"VOICEGROUP_ACTIVITY_STARTING",	// VOICEGROUP_ACTIVITY_STARTING
			"VOICEGROUP_SINGLEPLAYER",		// VOICEGROUP_SINGLEPLAYER
		};

		// reset talker reasons
		for(unsigned i = 0; i < MAX_NUM_PHYSICAL_PLAYERS; i++)
		{
			m_nAddReason[i] = REASON_INVALID;
			m_nRemoveReason[i] = REASON_INVALID;
			m_nChangeFlagsReason[i] = REASON_INVALID;
			m_nRemoteTeam[i] = -1;
			m_bRemoteSpectating[i] = false;
		}
#endif
		// logging
		gnetDebug1("UpdateTalkers :: Voice Group Changed. Was: %s, Now: %s", gs_szVoiceGroupNames[m_nVoiceGroup], gs_szVoiceGroupNames[nVoiceGroup]);

		// store last value
		m_nVoiceGroup = nVoiceGroup;

#if RSG_OUTPUT
		firstFrameInNewGroup = true; 
#endif
	}

	// track active talkers this frame
	atArray<rlGamerId> aActiveTalkers;

	// check if we're overriding all restrictions
	bool bOverrideRestrictions = m_bOverrideAllRestrictions;
#if RSG_OUTPUT						
	bOverrideRestrictions |= PARAM_voiceOverrideAllRestrictions.Get();
#endif

	// if we're in an active voice session
	switch(m_nVoiceGroup)
	{
	case VOICEGROUP_VOICE:
		{
			rlGamerInfo aGamers[CNetworkVoiceSession::kMAX_VOICE_PLAYERS];
			unsigned nGamers = CNetwork::GetNetworkVoiceSession().GetGamers(aGamers, CNetworkVoiceSession::kMAX_VOICE_PLAYERS);

			for(unsigned i = 0; i < nGamers; i++)
			{
				if(m_VoiceChat.HaveTalker(aGamers[i].GetGamerId()))
					continue;

				if(!CNetwork::GetNetworkVoiceSession().CanChat(aGamers[i].GetGamerHandle()))
					continue; 

				// get net endpointId
				EndpointId endpointId;
				CNetwork::GetNetworkVoiceSession().GetEndpointId(aGamers[i], &endpointId);

				// add talker
				gnetDebug1("UpdateTalkers :: Voice :: Add Talker \"%s\"", aGamers[i].GetName());
				m_VoiceChat.AddTalker(aGamers[i], endpointId, VoiceChat::TALKER_FLAG_SEND_RECEIVE_VOICE);
			}

			// add talker to tracking
			for(unsigned i = 0; i < nGamers; i++)
				aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
		}
		break;

	case VOICEGROUP_TRANSITION:
		{
			unsigned nDefaultTalkerFlags = VoiceChat::TALKER_FLAG_SEND_RECEIVE_VOICE;

#if RSG_ORBIS
			// on PS4, we need to be able to view who is chatting at any time so that we know who to block
			// but we have no room for spectators in the corona UI.
			// just explicitly disable sending of voice if the local player is spectating
			if(CNetwork::GetNetworkSession().IsTransitionMemberSpectating(NetworkInterface::GetLocalGamerHandle()))
				nDefaultTalkerFlags = VoiceChat::TALKER_FLAG_RECEIVE_VOICE;
#endif

			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

			for(unsigned i = 0; i < nGamers; i++)
			{
				// find their voice gamer entry
				VoiceGamerSettings* pGamer = FindGamer(aGamers[i].GetGamerHandle());

#if RSG_DURANGO
				// gamer can be added a little after on Xbox One while we retrieve the display name
				if(!pGamer)
					continue;
#else
				if(!gnetVerify(pGamer))
				{
					gnetError("UpdateTalkers :: Transition :: No settings for %s!", aGamers[i].GetName());
					continue; 
				}
#endif

				// find the talker index and check if the player is currently added to 
				int nTalkerIndex = m_VoiceChat.GetTalkerIndex(aGamers[i].GetGamerId());
				const bool bHaveTalker = nTalkerIndex >= 0;

				unsigned nTalkerFlags = nDefaultTalkerFlags;
				bool bCanChat = pGamer->IsLocal() || pGamer->CanCommunicateVoiceWith();

				if(bCanChat)
				{
					if(!bHaveTalker)
					{
						// get net endpointId
						EndpointId endpointId;
						CNetwork::GetNetworkSession().GetTransitionNetEndpointId(aGamers[i], &endpointId);

						// only add with a valid endpointId
						if(pGamer->IsLocal() || NET_IS_VALID_ENDPOINT_ID(endpointId))
						{
							if(CanAddTalker())
							{
								gnetDebug1("UpdateTalkers :: Transition :: Add Talker \"%s\" :: Flags: 0x%x", aGamers[i].GetName(), nTalkerFlags);
								m_VoiceChat.AddTalker(aGamers[i], endpointId, nTalkerFlags);
								aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
							}
							else
							{
								gnetDebug2("UpdateTalkers :: Transition :: Can't add \"%s\". TalkerCount: %u, MAX_REMOTE_TALKERS: %u",
									aGamers[i].GetName(),
									m_VoiceChat.GetTalkerCount(),
									VoiceChatTypes::MAX_REMOTE_TALKERS);
							}
						}
					}
					else if(nTalkerFlags != m_VoiceChat.GetTalkerFlags(nTalkerIndex))
					{
						gnetDebug1("UpdateTalkers :: Transition :: Change Talker Flags \"%s\" :: Flags: 0x%x", aGamers[i].GetName(), nTalkerFlags);
						m_VoiceChat.SetTalkerFlags(nTalkerIndex, nTalkerFlags);
						aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
					}
					else
						aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
				}
				else if(!bCanChat && bHaveTalker)
				{
					gnetDebug1("UpdateTalkers :: Transition :: Remove Talker \"%s\"", aGamers[i].GetName());
					m_VoiceChat.RemoveTalker(nTalkerIndex);
				}
			}

			const bool isInTransitionToGame = CNetwork::GetNetworkSession().IsInTransitionToGame();

#if RSG_OUTPUT
			static bool s_OverrideRestrictions = false;
			if ((s_OverrideRestrictions != bOverrideRestrictions) || firstFrameInNewGroup)
				gnetDebug1("UpdateTalkers :: Transition :: OverrideRestrictions: %s", bOverrideRestrictions ? "True" : "False");

			static bool s_bOverrideTransitionChatScript = false;
			if ((s_bOverrideTransitionChatScript != m_bOverrideTransitionChatScript) || firstFrameInNewGroup)
				gnetDebug1("UpdateTalkers :: Transition :: OverrideTransitionChatScript: %s", m_bOverrideTransitionChatScript ? "True" : "False");

			static bool s_bOverrideTransitionChat = false;
			if ((s_bOverrideTransitionChat != m_bOverrideTransitionChat) || firstFrameInNewGroup)
				gnetDebug1("UpdateTalkers :: Transition :: OverrideTransitionChat: %s", m_bOverrideTransitionChat ? "True" : "False");

			static bool s_IsInTransitionToGame = false;
			if ((s_IsInTransitionToGame != isInTransitionToGame) || firstFrameInNewGroup)
				gnetDebug1("UpdateTalkers :: Transition :: IsInTransitionToGame: %s", isInTransitionToGame ? "True" : "False");

			s_OverrideRestrictions = bOverrideRestrictions;
			s_bOverrideTransitionChatScript = m_bOverrideTransitionChatScript;
			s_bOverrideTransitionChat = m_bOverrideTransitionChat;
			s_IsInTransitionToGame = isInTransitionToGame;
#endif

			// we still process freeroam players if...
			//  - bOverrideRestrictions: overriding all restrictions (via command line)
			//  - m_bOverrideTransitionChatScript: script have requested this
			//  - m_bOverrideTransitionChat: hosting a lobby but no-one else has joined yet
			//	- isInTransitionToGame: switching between a job and freeroam (to hear our group)
			if(!bOverrideRestrictions && !m_bOverrideTransitionChatScript && !m_bOverrideTransitionChat && !isInTransitionToGame)
				break;

			// always break when we're not in a network session
			if(!CNetwork::GetNetworkSession().IsSessionCreated())
				break;
		}
		// intentionally no break, see conditions above for when we break

	case VOICEGROUP_GAME:
		{
			if(!CNetwork::IsNetworkOpen())
				break;

			if(!CNetwork::GetPlayerMgr().IsInitialized())
				break;

			CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
			if(!pLocalPlayer)
				break;

			const bool bLocalSpectating = NetworkInterface::IsInSpectatorMode();

#if !__NO_OUTPUT
			if(m_nVoiceChannel != pLocalPlayer->GetVoiceChannel())
			{
				gnetDebug1("UpdateTalkers :: Game :: Voice Channel Changed. Was: %d, Now: %d", m_nVoiceChannel, pLocalPlayer->GetVoiceChannel());
				m_nVoiceChannel = pLocalPlayer->GetVoiceChannel();
			}

			u8 nTutorialIndex = NetworkInterface::GetPlayerTutorialSessionIndex(*pLocalPlayer);
			if(m_nTutorialIndex != nTutorialIndex)
			{
				gnetDebug1("UpdateTalkers :: Game :: Tutorial index changed. Was: %d, Now: %d", m_nTutorialIndex, nTutorialIndex);
				m_nTutorialIndex = nTutorialIndex;
			}

			if(m_nTeam != pLocalPlayer->GetTeam())
			{
				gnetDebug1("UpdateTalkers :: Game :: Team changed. Was: %d, Now: %d", m_nTeam, pLocalPlayer->GetTeam());
				m_nTeam = pLocalPlayer->GetTeam();
			}

			if(m_bSpectating != bLocalSpectating)
			{
				gnetDebug1("UpdateTalkers :: Game :: Spectator setting changed. Was: %s, Now: %s", m_bSpectating ? "T" : "F", bLocalSpectating ? "T" : "F");
				m_bSpectating = bLocalSpectating;
			}
#endif

#if RSG_PC
			const bool hasLocalTalker = m_VoiceChat.HaveTalker(pLocalPlayer->GetGamerInfo().GetGamerId());
			if(!hasLocalTalker && m_bEnabled)
			{
				m_VoiceChat.AddTalker(pLocalPlayer->GetGamerInfo(), pLocalPlayer->GetEndpointId());
			}
			else if(hasLocalTalker && !m_bEnabled)
			{
				m_VoiceChat.RemoveTalker(pLocalPlayer->GetGamerInfo().GetGamerId());
			}
#else
			// if we don't have the local talker - add
			if(!m_VoiceChat.HaveTalker(pLocalPlayer->GetGamerInfo().GetGamerId()))
				m_VoiceChat.AddTalker(pLocalPlayer->GetGamerInfo(), pLocalPlayer->GetEndpointId());
#endif // RSG_PC

			// tag the local player
			aActiveTalkers.PushAndGrow(pLocalPlayer->GetGamerInfo().GetGamerId());

			// check if we need to clear out the focus player
			const CNetGamePlayer* pFocusPlayer = NULL;
			if(m_FocusTalker.IsValid())
			{
				// check if the talker is still in the game
				pFocusPlayer = NetworkInterface::GetPlayerFromGamerHandle(m_FocusTalker);
				if(!gnetVerifyf(pFocusPlayer, "UpdateTalkers :: Game :: Focus talker invalid!"))
					m_FocusTalker.Clear();
				else if(!m_OverrideSpectatorMode)
				{
					CNetObjPlayer* pFocusPlayerObj = static_cast<CNetObjPlayer*>(pFocusPlayer->GetPlayerPed()->GetNetworkObject());
					if(pFocusPlayerObj && (pFocusPlayerObj->IsSpectating() != bLocalSpectating))
					{
						gnetDebug1("UpdateTalkers :: Game :: Clearing focus talker. Different spectator settings.");
						pFocusPlayer = NULL;
						m_FocusTalker.Clear();
					}
				}
			}

			// grab the local ped
			const CPed* pLocalPed = pLocalPlayer->GetPlayerPed();
			gnetAssert(pLocalPed);
			if(!pLocalPed)
				return;

			// does our local player have a loudhailer
			const CPedWeaponManager* pWM = pLocalPed->GetWeaponManager();
			bool bLocalPlayerHasLoudhailer = pWM && (pWM->GetEquippedWeaponObjectHash() == WEAPONTYPE_LOUDHAILER) && pLocalPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun);

			// switch on/off loudhailer DSP
			m_AudioEntity.UpdateLocalPlayer(pLocalPed, bLocalPlayerHasLoudhailer);

			// check all remote players - for each player, determine if we can chat with him
			unsigned           numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
			netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

			// work out which talker will get the full checks applied - one per frame
			if(m_nFullCheckTalkerIndex >= numRemoteActivePlayers)
				m_nFullCheckTalkerIndex = 0;

			// run through all talkers
			for(unsigned index = 0; index < numRemoteActivePlayers; index++)
			{
				// grab the remote player
				CNetGamePlayer* pRemotePlayer = SafeCast(CNetGamePlayer, remoteActivePlayers[index]);

				// gamer info for the remote player
				const rlGamerInfo& hGamerInfo = pRemotePlayer->GetGamerInfo();

				// if we're in a transition voice group (and falling through to here)... 
				// skip any players who are already in the transition session
				if(m_nVoiceGroup == VOICEGROUP_TRANSITION)
				{
					if(CNetwork::GetNetworkSession().IsTransitionMember(hGamerInfo.GetGamerHandle()))
						continue; 
				}

				// find their voice gamer entry
				VoiceGamerSettings* pGamer = FindGamer(hGamerInfo.GetGamerHandle());

				// find the talker index and check if the player is currently added to 
				int nTalkerIndex = m_VoiceChat.GetTalkerIndex(pRemotePlayer->GetGamerInfo().GetGamerId());
				const bool bHaveTalker = nTalkerIndex >= 0;

				// assume full chat
				unsigned nTalkerFlags = VoiceChat::TALKER_FLAG_SEND_RECEIVE_VOICE;

				// should we only consider removing this player
				bool bFullCheck = (m_nFullCheckTalkerIndex == static_cast<int>(index)); 

#if !__NO_OUTPUT
				// reset talker tracking
				eTalkerReason nAddTalkerReason = ADD_ALL_TESTS_PASSED; 
				eTalkerReason nRemoveTalkerReason = REMOVE_UNKNOWN; 
				eTalkerReason nChangeFlagsReason = CHANGE_FLAGS_NOT_CHANGED; 
				int nLocalSetting = 0;
				int nRemoteSetting = 0;

				// track team changes
				if(m_nRemoteTeam[index] != pRemotePlayer->GetTeam())
				{
					gnetDebug1("UpdateTalkers :: Game :: %s Team changed. Was: %d, Now: %d", pRemotePlayer->GetLogName(), m_nRemoteTeam[index], pRemotePlayer->GetTeam());
					m_nRemoteTeam[index] = pRemotePlayer->GetTeam();
				}
#endif

				// check that this player is still active
				bool bCanChat = (pRemotePlayer->GetConnectionId() >= 0);
				OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_NOT_CONNECTED, 0, 0));

				// check that this player has a gamer entry
				if(bCanChat)
				{
					bCanChat = (pGamer != NULL);
					OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_NO_GAMER_ENTRY, 0, pRemotePlayer->GetConnectionId()));
				}

				if(bCanChat)
				{
					// if we aren't overriding all restrictions, check these settings
					if(!bOverrideRestrictions)
					{	
						// check if chat is muted or blocked
						bCanChat &= pGamer->CanCommunicateVoiceWith();
						OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_BLOCKED, (pGamer->IsMutedByMe() || pGamer->IsBlockedByMe()) ? 1 : 0, (pGamer->IsMutedByHim() || pGamer->IsBlockedByHim()) ? 1 : 0));

						// check for auto mute
						if(bCanChat)
						{
							bCanChat &= !pGamer->IsAutoMuted();
							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_AUTO_MUTED, pGamer->IsLocalAutomuted() ? 1 : 0, pGamer->IsRemoteAutomuted() ? 1 : 0));
						}

#if RSG_PC
						// only add the player to the voice chat system if they can receive audio
						// (we don't allow players to send voice chat without being able to receive it)
						if(bCanChat && !PARAM_voiceIgnoreHardware.Get())
						{
							bCanChat &= pGamer->IsVoiceThruSpeakers();
							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_HARDWARE, pGamer->HasHeadset() ? 1 : 0, pGamer->IsVoiceThruSpeakers() ? 1 : 0));
						}
#else
						// verify that the remote player has a headset or speaker playback
						if(bCanChat && !PARAM_voiceIgnoreHardware.Get())
						{
							bCanChat &= (pGamer->HasHeadset() || pGamer->IsVoiceThruSpeakers());
							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_HARDWARE, pGamer->HasHeadset() ? 1 : 0, pGamer->IsVoiceThruSpeakers() ? 1 : 0));
						}
#endif //!RSG_PC

						if(bCanChat && !PARAM_voiceOverrideActive.Get())
						{
							bCanChat &= m_bIsCommunicationActive;
							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_COMMUNICATIONS_INACTIVE, m_bIsCommunicationActive ? 1 : 0, 0));
						}
					}

					// if we are able to chat and not overriding restrictions, look for reasons why
					// we cannot communicate with this player
					if(bCanChat && !bOverrideRestrictions)
					{
						PhysicalPlayerIndex playerIndex = pRemotePlayer->GetPhysicalPlayerIndex();
						if(pFocusPlayer)
						{
							// we currently have a focus talker - we can only chat with him
							bCanChat &= (pFocusPlayer == pRemotePlayer);
							OUTPUT_ONLY(SetAddTalkerReason(bCanChat, nAddTalkerReason, ADD_IS_FOCUS_TALKER));
							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_NOT_FOCUS_TALKER, static_cast<int>(pFocusPlayer->GetActivePlayerIndex()), 0));
						}
						else if(playerIndex != INVALID_PLAYER_INDEX && m_DisableBandwidthVoiceTime[playerIndex] > 0)
						{
							bCanChat = false;
							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_BANDWIDTH, static_cast<int>(m_DisableBandwidthVoiceTime[playerIndex]), 0));
						}
						else 
						{
							// check remote ped - if we have no ped, we can assume we're in the lobby and can chat
							const CPed* pRemotePed = pRemotePlayer->GetPlayerPed();

							// we need a player ped, as some of the information used to reject a player will only be known at that time
							// do not do this for activity sessions, since we are launching with those players and will lose chat for a
							// time if we wait for the information to come through
							bCanChat &= (pRemotePed != NULL);
							bCanChat |= CNetwork::GetNetworkSession().IsActivitySession();
							bCanChat |= CNetwork::GetNetworkSession().IsTransitionMember(hGamerInfo.GetGamerHandle());
							bCanChat |= m_bOverrideTransitionChatScript; //! passing through, allow chat

							OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_NO_PLAYER_PED, 0, 0));

							if(bCanChat && pRemotePed)
							{
								if(NetworkInterface::IsInSession() || bLocalSpectating)
								{
									// cannot chat with players in transition sessions
									if(bCanChat && !GetOverrideTransitionRestrictions(pRemotePlayer->GetPhysicalPlayerIndex()))
									{
										bCanChat &= !SafeCast(const CNetGamePlayer, pRemotePlayer)->HasStartedTransition();
										OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_IN_TRANSITION, 0, 0));
									}

									// cannot chat with players in different tutorial sessions
									if(bCanChat && !GetOverrideTutorialRestrictions(pRemotePlayer->GetPhysicalPlayerIndex()))
									{
										bCanChat &= !SafeCast(const CNetGamePlayer, pRemotePlayer)->IsInDifferentTutorialSession();
										OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_DIFFERENT_TUTORIAL, static_cast<int>(NetworkInterface::GetPlayerTutorialSessionIndex(*pLocalPlayer)), static_cast<int>(NetworkInterface::GetPlayerTutorialSessionIndex(*pRemotePlayer))));
									}

									if(bCanChat)
									{
										// check if either player is in an assigned voice channel
										if((pLocalPlayer->GetVoiceChannel() != netPlayer::VOICE_CHANNEL_NONE) || (pRemotePlayer->GetVoiceChannel() != netPlayer::VOICE_CHANNEL_NONE))
										{
											bCanChat &= (pLocalPlayer->GetVoiceChannel() == pRemotePlayer->GetVoiceChannel());
											OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_DIFFERENT_VOICE_CHANNEL, pLocalPlayer->GetVoiceChannel(), pRemotePlayer->GetVoiceChannel()));
										}
										// only run these checks for one talker per frame
										else if(bFullCheck)
										{
											// cache so that we don't have to work this out multiple times
											float fDistanceFromPlayer = -1.0f;
											float fHeightFromPlayer = -1.0f;

											// grab spectator settings
											CNetObjPlayer* pOtherPlayerObj = static_cast<CNetObjPlayer*>(pRemotePed->GetNetworkObject());
											const bool bRemoteSpectating = (pOtherPlayerObj && pOtherPlayerObj->IsSpectating());
											const bool bSomeoneSpectating = bLocalSpectating || bRemoteSpectating;
#if !__NO_OUTPUT
											// track spectator changes
											if(m_nRemoteTeam[index] != pRemotePlayer->GetTeam())
											{
												gnetDebug1("UpdateTalkers :: Game :: %s Spectating changed. Was: %s, Now: %s", pRemotePlayer->GetLogName(), m_bRemoteSpectating[index] ? "True" : "False", bRemoteSpectating ? "True" : "False");
												m_bRemoteSpectating[index] = bRemoteSpectating;
											}
#endif
											// are these players on the same team
											const bool bInTeam = (pLocalPlayer->GetTeam() >= 0);
											const bool bRemoteInTeam = (pRemotePlayer->GetTeam() >= 0);
											const bool bInSameTeam = (bInTeam && pLocalPlayer->GetTeam() == pRemotePlayer->GetTeam());
											
											// should we run spectator rules
											bool bRunSpectatorRules = !m_OverrideSpectatorMode && bSomeoneSpectating;
											bRunSpectatorRules &= !m_NoSpectatorChat;
											bRunSpectatorRules &= !(m_bIgnoreSpectatorLimitsSameTeam && bInSameTeam);

											// spectator rules
											if(bRunSpectatorRules)
											{
												nTalkerFlags = VoiceChat::TALKER_FLAG_NONE;
												if(bLocalSpectating && !bRemoteSpectating)
												{
													nTalkerFlags |= VoiceChat::TALKER_FLAG_RECEIVE_VOICE;
													OUTPUT_ONLY(SetChangeFlagsReason(bCanChat, nChangeFlagsReason, CHANGE_FLAGS_LOCAL_SPECTATING));
												}
												else if(!bLocalSpectating && bRemoteSpectating)
												{
													nTalkerFlags |= VoiceChat::TALKER_FLAG_SEND_VOICE;
													OUTPUT_ONLY(SetChangeFlagsReason(bCanChat, nChangeFlagsReason, CHANGE_FLAGS_REMOTE_SPECTATING));
												}
												else if(bLocalSpectating && bRemoteSpectating)
													nTalkerFlags |= VoiceChat::TALKER_FLAG_SEND_RECEIVE_VOICE;
											}

											// check chat restrictions if this player is not flagged for override
											if(!GetOverrideChatRestrictions(pRemotePlayer->GetPhysicalPlayerIndex()))
											{
												// assume false for now
												bool bProximityAffectsReceive = false;
												bool bProximityAffectsSend = false;

												// negative proximity implies chat disabled
												bCanChat = m_TalkerProximityLo >= 0; 
												OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_NEGATIVE_PROXIMITY, static_cast<int>(m_TalkerProximityLo), 0));

												if(bCanChat)
												{
													// turn off spectator rules if requested
													if(bLocalSpectating && m_NoSpectatorChat)
													{
														bCanChat = false;
														OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_SPECTATOR_CHAT_DISABLED, 0, 0));
													}

													// is team only chat
													bool bTeamOnlyChat = m_TeamOnlyChat;

													// unless we are including unassigned teams, skip players not yet assigned
													if(bTeamOnlyChat && !m_TeamChatIncludeUnassignedTeams)
													{
														bTeamOnlyChat &= bInTeam;
														bTeamOnlyChat &= bRemoteInTeam;
													}							
#if RSG_OUTPUT
													// check debug override setting
													bTeamOnlyChat &= !PARAM_voiceOverrideTeamChat.Get();
#endif  //__FINAL
													// check team only chat settings
													if(bTeamOnlyChat)
													{
														// not the same team? check if we have an override
														bool bInOverrideTeam = false;
														if(!bInSameTeam && bRemoteInTeam)
															bInOverrideTeam = GetOverrideTeam(pRemotePlayer->GetTeam());

														// we can only chat with gamers on our team (or if this player is exempt)
														bCanChat &= (bInSameTeam || bInOverrideTeam);
														OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_DIFFERENT_TEAMS, pLocalPlayer->GetTeam(), pRemotePlayer->GetTeam()));
													}

													// work out whether we should run a proximity check
													bool bRunProximityCheck = m_TalkerProximityLo > 0;

													// with team chat, we only run the proximity check for players who we can't talk to already
													if(bRunProximityCheck && !m_ProximityAffectsTeam && bTeamOnlyChat)
														bRunProximityCheck = !bCanChat;

													// if this player is in our local party, skip the proximity check
													//if(bRunProximityCheck && NetworkInterface::IsPartyMember(&hGamerInfo.GetGamerHandle()))
													//	bRunProximityCheck = false;

													// check proximity override
													if(bRunProximityCheck)
													{
														Vector3 vLocal, vRemote;
														Vector3 diff = pLocalPlayer->GetVoiceProximity(vLocal) - pRemotePlayer->GetVoiceProximity(vRemote);
														fHeightFromPlayer = Abs(diff.z);

														// flatten height
														diff.z = 0;
														fDistanceFromPlayer = diff.Mag2();

														// height check, with dead zone
														bCanChat = (!bHaveTalker && fHeightFromPlayer <= static_cast<float>(m_TalkerProximityHeightLoSq))
																|| ( bHaveTalker && fHeightFromPlayer <= static_cast<float>(m_TalkerProximityHeightHiSq));

														if(bCanChat)
														{
															// distance check, with dead zone
															bCanChat = (!bHaveTalker && fDistanceFromPlayer <= static_cast<float>(m_TalkerProximityLoSq))
																	|| ( bHaveTalker && fDistanceFromPlayer <= static_cast<float>(m_TalkerProximityHiSq));
														}

														// toggle these on
														bProximityAffectsReceive = !bCanChat && m_ProximityAffectsReceive;
														bProximityAffectsSend = !bCanChat && bProximityAffectsSend;

														OUTPUT_ONLY(SetRemoveTalkerReason(bCanChat, nRemoveTalkerReason, nLocalSetting, nRemoteSetting, REMOVE_OUTSIDE_PROXIMITY, static_cast<int>(fHeightFromPlayer), static_cast<int>(fDistanceFromPlayer)));
													}

													// if we can't chat, reset the talker flags so that the loudhailer is applied correctly
													if(!bCanChat)
														nTalkerFlags = VoiceChat::TALKER_FLAG_NONE;

													// check for loudhailers - players with loudhailers can send to anyone within proximity
													if(!bCanChat || (nTalkerFlags & VoiceChat::TALKER_FLAG_SEND_VOICE) == 0)
													{
														const CPed* pRemotePed = pRemotePlayer->GetPlayerPed();
														if(pRemotePed)
														{
															if(bLocalPlayerHasLoudhailer && !bLocalSpectating)
															{
																// if we haven't already worked this out for proximity checks above
																if(fDistanceFromPlayer < 0.0f)
																{
																	if(pLocalPed)
																	{
																		Vector3 vLocal, vRemote;
																		Vector3 diff = pLocalPlayer->GetVoiceProximity(vLocal) - pRemotePlayer->GetVoiceProximity(vRemote);
																		fHeightFromPlayer = Abs(diff.z);

																		// flatten height
																		diff.z = 0; 
																		fDistanceFromPlayer = diff.Mag2();
																	}
																}

																// height check, with dead zone
																bCanChat = (!bHaveTalker && fHeightFromPlayer <= static_cast<float>(m_LoudhailerProximityHeightLoSq))
																	|| ( bHaveTalker && fHeightFromPlayer <= static_cast<float>(m_LoudhailerProximityHeightHiSq));

																if(bCanChat)
																{
																	// distance check, with dead zone
																	bCanChat = (!bHaveTalker && fDistanceFromPlayer <= static_cast<float>(m_LoudhailerProximityLoSq))
																		|| ( bHaveTalker && fDistanceFromPlayer <= static_cast<float>(m_LoudhailerProximityLoSq));
																}

																if(bCanChat)
																	nTalkerFlags |= VoiceChat::TALKER_FLAG_SEND_VOICE;

																OUTPUT_ONLY(SetChangeFlagsReason(bCanChat, nChangeFlagsReason, CHANGE_FLAGS_LOCAL_LOUDHAILER));
																OUTPUT_ONLY(SetAddTalkerReason(bCanChat, nAddTalkerReason, ADD_LOCAL_LOUDHAILER));
															}
														}
													}

													// does the remote player have a loudhailer
													const CPedWeaponManager* pRWM = pRemotePed->GetWeaponManager();
													bool bRemotePlayerHasLoudhailer = pRWM && (pRWM->GetEquippedWeaponObjectHash() == WEAPONTYPE_LOUDHAILER) && pRemotePed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun);;

													// switch on/off loudhailer DSP
													m_VoiceChat.SetTalkerAudioParam(nTalkerIndex, ATSTRINGHASH("Enabled", 0x5B12990F), bRemotePlayerHasLoudhailer ? 1.0f : 0.0f);

													// players can receive from other players using loudhailers
													if(!bCanChat || (nTalkerFlags & VoiceChat::TALKER_FLAG_RECEIVE_VOICE) == 0)
													{
														if(bRemotePlayerHasLoudhailer)
														{
															// no distance check needed - we assume the other player has already done this
															bCanChat = true;
															nTalkerFlags |= VoiceChat::TALKER_FLAG_RECEIVE_VOICE;
															OUTPUT_ONLY(SetChangeFlagsReason(bCanChat, nChangeFlagsReason, CHANGE_FLAGS_REMOTE_LOUDHAILER));
															OUTPUT_ONLY(SetAddTalkerReason(bCanChat, nAddTalkerReason, ADD_REMOTE_LOUDHAILER));
														}
													}
												}
												
												// check for uni-directional overrides
												bool bAllowOverride = (m_TalkerProximityLo >= 0 || (m_bScriptAllowUniDirectionalOverrideForNegativeProximity && m_bAllowUniDirectionalOverrideForNegativeProximity));
												if((!bCanChat || nTalkerFlags != VoiceChat::TALKER_FLAG_SEND_RECEIVE_VOICE) && bAllowOverride)
												{
													// reset these if we can't chat
													if(!bCanChat)
														nTalkerFlags = VoiceChat::TALKER_FLAG_NONE;

													if((nTalkerFlags & VoiceChat::TALKER_FLAG_SEND_VOICE) == 0 && GetOverrideChatRestrictionsSend(pRemotePlayer->GetPhysicalPlayerIndex()) && !bProximityAffectsSend)
													{
														nTalkerFlags |= VoiceChat::TALKER_FLAG_SEND_VOICE;
														OUTPUT_ONLY(SetChangeFlagsReason(true, nChangeFlagsReason, CHANGE_FLAGS_OVERRIDE_SEND));
													}
													if((nTalkerFlags & VoiceChat::TALKER_FLAG_RECEIVE_VOICE) == 0 && GetOverrideChatRestrictionsReceive(pRemotePlayer->GetPhysicalPlayerIndex()) && !bProximityAffectsReceive)
													{
														nTalkerFlags |= VoiceChat::TALKER_FLAG_RECEIVE_VOICE;
														OUTPUT_ONLY(SetChangeFlagsReason(true, nChangeFlagsReason, (nChangeFlagsReason == CHANGE_FLAGS_OVERRIDE_SEND) ? CHANGE_FLAGS_OVERRIDE_BOTH : CHANGE_FLAGS_OVERRIDE_RECEIVE));
													}

													// this only applies if we couldn't chat in the first place (otherwise, we overwrite the original add reason)
													if(!bCanChat)
													{
													// if we applied flags, we can chat
													bCanChat = (nTalkerFlags != VoiceChat::TALKER_FLAG_NONE);
														OUTPUT_ONLY(SetAddTalkerReason(bCanChat, nAddTalkerReason, ADD_OVERRIDE_RESTRICTIONS));
													}
											}
										}
									}
								}
							}
						}
					}
				}
				}

#if !__NO_OUTPUT
				// if we can chat and haven't set a remove reason, flip it to REMOVE_NOT_REMOVED
				if(bCanChat && nRemoveTalkerReason == REMOVE_UNKNOWN)
					nRemoveTalkerReason = REMOVE_NOT_REMOVED;

				// log out why we can't add this guy once
				if(bFullCheck && pRemotePlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
				{
					if(!bCanChat && m_nRemoveReason[pRemotePlayer->GetPhysicalPlayerIndex()] != nRemoveTalkerReason)
					{
						gnetDebug1("UpdateTalkers :: Game :: Cannot chat with \"%s\" :: Reason: %s. Local: %d, Remote: %d", pRemotePlayer->GetLogName(), gs_szTalkerReasons[nRemoveTalkerReason], nLocalSetting, nRemoteSetting);
						m_nRemoveReason[pRemotePlayer->GetPhysicalPlayerIndex()] = nRemoveTalkerReason;
					}
					else if(bCanChat && m_nAddReason[pRemotePlayer->GetPhysicalPlayerIndex()] != nAddTalkerReason)
					{
						gnetDebug1("UpdateTalkers :: Game :: Can chat with \"%s\" :: Reason: %s", pRemotePlayer->GetLogName(), gs_szTalkerReasons[nAddTalkerReason]);
						m_nAddReason[pRemotePlayer->GetPhysicalPlayerIndex()] = nAddTalkerReason;
					}
				}
#endif
				bool bHaveTalkerPost = false;
				if(bCanChat && !bHaveTalker && (bFullCheck || nTalkerFlags == VoiceChat::TALKER_FLAG_NONE))
				{
					EndpointId endpointId = pRemotePlayer->GetEndpointId();

#if ENABLE_NETWORK_BOTS
					if(pRemotePlayer->IsBot())
					{
						// remote bots don't have a valid endpointId, so we need to look it up from the
						// local player on the machine owning the bots
						CNetGamePlayer *mainPlayerForPeer = SafeCast(CNetGamePlayer, NetworkInterface::GetPlayerFromPeerId(pRemotePlayer->GetPeerInfo().GetPeerId()));
						if(mainPlayerForPeer)
						{
							endpointId = mainPlayerForPeer->GetEndpointId();
						}
					}
#endif // ENABLE_NETWORK_BOTS

					if(CanAddTalker())
					{
						gnetDebug1("UpdateTalkers :: Game :: Add Talker \"%s\" :: Reason: %s, FlagsReason: %s, Flags: 0x%x", pRemotePlayer->GetLogName(), gs_szTalkerReasons[nAddTalkerReason], gs_szTalkerReasons[nChangeFlagsReason], nTalkerFlags);
						m_VoiceChat.AddTalker(hGamerInfo, endpointId, nTalkerFlags);
						m_AudioEntity.AddTalker(hGamerInfo.GetGamerId(), pRemotePlayer->GetPlayerPed());
						bHaveTalkerPost = true;
					}
					else
					{
						gnetDebug2("UpdateTalkers :: Game :: Can't add \"%s\". TalkerCount: %u, MAX_REMOTE_TALKERS: %u",
							pRemotePlayer->GetLogName(),
							m_VoiceChat.GetTalkerCount(),
							VoiceChatTypes::MAX_REMOTE_TALKERS);
					}
				}
				else if(!bCanChat && bHaveTalker)
				{
					gnetDebug1("UpdateTalkers :: Game :: Remove Talker \"%s\" :: Reason: %s. Local: %d, Remote: %d", pRemotePlayer->GetLogName(), gs_szTalkerReasons[nRemoveTalkerReason], nLocalSetting, nRemoteSetting);
					m_VoiceChat.RemoveTalker(nTalkerIndex);
					m_AudioEntity.RemoveTalker(hGamerInfo.GetGamerId());
					bHaveTalkerPost = false;
				}
				else if(bCanChat && bHaveTalker)
				{
					if((bFullCheck || nTalkerFlags == VoiceChat::TALKER_FLAG_NONE) && (nTalkerFlags != m_VoiceChat.GetTalkerFlags(nTalkerIndex)))
					{
						gnetDebug1("UpdateTalkers :: Game :: Change Talker Flags \"%s\" :: Flags: 0x%x, FlagsReason: %s, AddReason: %s, RemoveReason: %s", pRemotePlayer->GetLogName(), nTalkerFlags, gs_szTalkerReasons[nChangeFlagsReason], gs_szTalkerReasons[nAddTalkerReason], gs_szTalkerReasons[nRemoveTalkerReason]);
						m_VoiceChat.SetTalkerFlags(nTalkerIndex, nTalkerFlags);
					}
					bHaveTalkerPost = true;
				}

				// mark this talker as active if we're still in the talker pool
				if(bHaveTalkerPost)
				{
					// if we have just added this talker, we need to get the index
					if(!bHaveTalker)
						nTalkerIndex = m_VoiceChat.GetTalkerIndex(hGamerInfo.GetGamerId());

					aActiveTalkers.PushAndGrow(hGamerInfo.GetGamerId());
				}
			}

			// increment the full check index
			++m_nFullCheckTalkerIndex;

			// if we have a cap on the number of voice recipients
			if(m_nMaxVoiceRecipients > 0 || m_nMaxBandwidthVoiceRecipients > 0)
			{
				u32 maxVoiceRecipients = m_nMaxVoiceRecipients;
				if(maxVoiceRecipients == 0 || (m_nMaxBandwidthVoiceRecipients > 0 && m_nMaxBandwidthVoiceRecipients < maxVoiceRecipients))
				{
					maxVoiceRecipients = m_nMaxBandwidthVoiceRecipients;
				}

				// prioritise the remaining players
				static const unsigned nRecentVoiceThreshold			= 5000;
				static const float fWorldLimits						= WORLDLIMITS_XMAX;
				static const float fDifferentMissionsHandicap		= (fWorldLimits / 3) * (fWorldLimits / 3);
				static const float fDifferentTeamsHandicap			= (fWorldLimits / 2) * (fWorldLimits / 2);
				static const float fNoHeadsetHandicap				= (fWorldLimits / 4) * (fWorldLimits / 4);
				static const float fNoRecentVoiceHandicap			= (fWorldLimits / 3) * (fWorldLimits / 3);
				static const float fNotInLoudhailerRangeHandicap	= fWorldLimits * fWorldLimits;

				float fScore[RL_MAX_GAMERS_PER_SESSION];
				const CNetGamePlayer* pPlayers[RL_MAX_GAMERS_PER_SESSION];
				for(int i = 0; i < RL_MAX_GAMERS_PER_SESSION; i++)
				{
					pPlayers[i] = NULL;
					fScore[i] = LARGE_FLOAT;
				}

				// check all remote players
				unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
				const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

				for(unsigned index = 0; index < numRemoteActivePlayers; index++)
				{
					const CNetGamePlayer *pRemotePlayer = SafeCast(const CNetGamePlayer, remoteActivePlayers[index]);

					// this is a bot...? bail
					if(pRemotePlayer->IsBot())
						continue;

					const rlGamerInfo& hGamerInfo = pRemotePlayer->GetGamerInfo();
					const int nTalkerIndex = m_VoiceChat.GetTalkerIndex(hGamerInfo.GetGamerId());
					const bool bHaveTalker = nTalkerIndex >= 0;

					// find their voice gamer entry
					VoiceGamerSettings* pGamer = FindGamer(hGamerInfo.GetGamerHandle());

					// can't chat with this guy...? bail
					if(!bHaveTalker)
						continue;

					float fDistanceFromPlayer = 0.0f;

					// remote ped
					const CPed* pRemotePed = pRemotePlayer->GetPlayerPed();
					if(pRemotePed)
					{
						Vector3 vLocal, vRemote;
						Vector3 diff = pLocalPlayer->GetVoiceProximity(vLocal) - pRemotePlayer->GetVoiceProximity(vRemote);
						diff.z = 0; //< flatten z
						fDistanceFromPlayer = diff.Mag2();
					}
					else
						fDistanceFromPlayer = fWorldLimits;

					// initial score is just the distance
					float fCurrentScore = fDistanceFromPlayer;

					// favour players in the same mission as us
					const bool bInSameMission = true;
					if(!bInSameMission)
						fCurrentScore += fDifferentMissionsHandicap;

					// favour players in the same team as us
					const bool bInSameTeam = (pLocalPlayer->GetTeam() >= 0 && pLocalPlayer->GetTeam() == pRemotePlayer->GetTeam());
					if(!bInSameTeam)
						fCurrentScore += fDifferentTeamsHandicap;

					// favour players with headsets
					if(!pGamer->HasHeadset())
						fCurrentScore += fNoHeadsetHandicap;

					// favor people we've heard from recently, or who are hearing us
					// this means:
					// - we don't suddenly stop sending to someone who's been listening to us talking, mid-sentence
					// - we can generally reply and be heard by someone who we have just heard
					const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
					const unsigned nLastVoiceSentTime = m_VoiceChat.GetTalkerLastSendTime(nTalkerIndex);
					const unsigned nLastVoiceRecvTime = m_VoiceChat.GetTalkerLastReceiveTime(nTalkerIndex);
					const bool bRecentlySentVoice = (nLastVoiceSentTime > 0) && ((nLastVoiceSentTime + nRecentVoiceThreshold) > nCurrentTime);
					const bool bRecentlyRecvVoice = (nLastVoiceRecvTime > 0) && ((nLastVoiceRecvTime + nRecentVoiceThreshold) > nCurrentTime);

					if(!bRecentlySentVoice && !bRecentlyRecvVoice)
						fCurrentScore += fNoRecentVoiceHandicap;

					// if we have and are using a loudhailer, we can assume that we prioritise this over everything else
					if(bLocalPlayerHasLoudhailer && (fDistanceFromPlayer > m_LoudhailerProximityHiSq))
						fCurrentScore += fNotInLoudhailerRangeHandicap;

					// find the furthest away entry to replace
					float fWorstScore = 0.0f; 
					int nInsertionCandidate = -1; 
					for(int i = 0; i < maxVoiceRecipients; i++)
					{
						//existing entry is further than the furthest found
						//and further than the testing node
						//so is a candidate for eviction
						if(fScore[i] > fWorstScore && fScore[i] > fCurrentScore)
						{
							nInsertionCandidate = i;
							fWorstScore = fScore[i];
						}
					}

					// valid insertion index
					if(nInsertionCandidate != -1)
					{
						fScore[nInsertionCandidate]	= fCurrentScore;
						pPlayers[nInsertionCandidate] = pRemotePlayer;
					}
				}

				// reject players that we've not prioritised
				for(unsigned index = 0; index < numRemoteActivePlayers; index++)
				{
					const CNetGamePlayer *pRemotePlayer = SafeCast(const CNetGamePlayer, remoteActivePlayers[index]);

					// this is a bot...? bail
					if(pRemotePlayer->IsBot())
						continue;

					const rlGamerInfo& hGamerInfo = pRemotePlayer->GetGamerInfo();
					const int nTalkerIndex = m_VoiceChat.GetTalkerIndex(hGamerInfo.GetGamerId());
					const bool bHaveTalker = nTalkerIndex >= 0;

					// can't chat with this guy...? bail
					if(!bHaveTalker)
						continue;

					// check in our priority list
					bool bIsPrioritised = false;
					for(int i = 0; i < maxVoiceRecipients; i++)
					{
						if(pRemotePlayer == pPlayers[i])
						{
							bIsPrioritised = true; 
							break;
						}
					}

					// if not prioritised - remove
					if(!bIsPrioritised)
					{
						gnetDebug1("UpdateTalkers :: Game :: Remove Talker \"%s\" - Not Prioritised.", pRemotePlayer->GetLogName());
						m_VoiceChat.RemoveTalker(nTalkerIndex);
						m_AudioEntity.RemoveTalker(hGamerInfo.GetGamerId());
					}
				}
			}
		}
		break;

	case VOICEGROUP_ACTIVITY_STARTING:
		{
			unsigned nDefaultTalkerFlags = VoiceChat::TALKER_FLAG_SEND_RECEIVE_VOICE;

#if RSG_ORBIS
			// on PS4, we need to be able to view who is chatting at any time so that we know who to block
			// but we have no room for spectators in the corona UI.
			// just explicitly disable sending of voice if the local player is spectating
			if(CNetwork::GetNetworkSession().IsActivitySpectator(NetworkInterface::GetLocalGamerHandle()))
				nDefaultTalkerFlags = VoiceChat::TALKER_FLAG_RECEIVE_VOICE;
#endif

			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			unsigned nGamers = CNetwork::GetNetworkSession().GetSessionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

			for(unsigned i = 0; i < nGamers; i++)
			{
				// find their voice gamer entry
				VoiceGamerSettings* pGamer = FindGamer(aGamers[i].GetGamerHandle());

#if RSG_DURANGO
				// gamer can be added a little after on Xbox One while we retrieve the display name
				if(!pGamer)
					continue;
#else
				if(!gnetVerify(pGamer))
				{
					gnetError("UpdateTalkers :: ActivityStarting :: No settings for %s!", aGamers[i].GetName());
					continue; 
				}
#endif

				// find the talker index and check if the player is currently added to 
				int nTalkerIndex = m_VoiceChat.GetTalkerIndex(aGamers[i].GetGamerId());
				const bool bHaveTalker = nTalkerIndex >= 0;

				unsigned nTalkerFlags = nDefaultTalkerFlags;
				bool bCanChat = pGamer->IsLocal() || pGamer->CanCommunicateVoiceWith();

				if(bCanChat)
				{
					if(!bHaveTalker)
					{
						// get net endpointId
						EndpointId endpointId;
						CNetwork::GetNetworkSession().GetSessionNetEndpointId(aGamers[i], &endpointId);

						// only add with a valid net endpointId
						if(pGamer->IsLocal() || NET_IS_VALID_ENDPOINT_ID(endpointId))
						{
							// add talker
							gnetDebug1("UpdateTalkers :: ActivityStarting :: Add Talker \"%s\" :: Flags: 0x%x", aGamers[i].GetName(), nTalkerFlags);
							m_VoiceChat.AddTalker(aGamers[i], endpointId, nTalkerFlags);
							aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
						}
					}
					else if(nTalkerFlags != m_VoiceChat.GetTalkerFlags(nTalkerIndex))
					{
						gnetDebug1("UpdateTalkers :: ActivityStarting :: Change Talker Flags \"%s\" :: Flags: 0x%x", aGamers[i].GetName(), nTalkerFlags);
						m_VoiceChat.SetTalkerFlags(nTalkerIndex, nTalkerFlags);
						aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
					}
					else
						aActiveTalkers.PushAndGrow(aGamers[i].GetGamerId());
				}
				else if(!bCanChat && bHaveTalker)
				{
					gnetDebug1("UpdateTalkers :: ActivityStarting :: Remove Talker \"%s\"", aGamers[i].GetName());
					m_VoiceChat.RemoveTalker(nTalkerIndex);
				}
			}
		}
		break;

	case VOICEGROUP_SINGLEPLAYER:
		{
			// just add the local player
			const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
			if(pInfo)
			{
				if(!m_VoiceChat.HaveTalker(pInfo->GetGamerId()))
				{
					gnetDebug1("UpdateTalkers :: SinglePlayer :: Adding local talker");
					m_VoiceChat.AddTalker(*pInfo, NET_INVALID_ENDPOINT_ID);
				}

				aActiveTalkers.PushAndGrow(pInfo->GetGamerId());
			}
			else // remove talkers if we have no local player signed in
				m_VoiceChat.RemoveAllTalkers();
		}
		break;

	default:
		// voice group may be invalid for a short period after initialization, do nothing
		break;
	}

	// resolve players who are no longer with us
	int nLastTalkers = m_LastTalkers.GetCount();
	for(int i = 0; i < nLastTalkers; i++)
	{
		if(aActiveTalkers.Find(m_LastTalkers[i]) < 0 && m_VoiceChat.GetTalkerIndex(m_LastTalkers[i]) >= 0)
		{
			// removing inactive talker
			gnetDebug1("UpdateTalkers :: Remove Talker \"%" I64FMT "u\" :: Reason: No longer active", m_LastTalkers[i].GetId());
			m_VoiceChat.RemoveTalker(m_LastTalkers[i]);
			m_AudioEntity.RemoveTalker(m_LastTalkers[i]);
		}
	}

	// update the talkers
	m_LastTalkers = aActiveTalkers;

	// update this after resolving talkers who have left
	if(m_nVoiceGroup == VOICEGROUP_GAME)
		m_AudioEntity.UpdateRemoteTalkers();
}

void CNetworkVoice::UpdateBandwidthRestrictions()
{
	const u32 INITIAL_DISABLE_BANDWIDTH_VOICE_TIME_MS = 30 * 1000;
	const u32 MAX_DISABLE_BANDWIDTH_VOICE_TIME_MS     = 3 * 60 * 1000;

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const netPlayer *remotePlayer = remotePhysicalPlayers[index];

		if(remotePlayer)
		{
			PhysicalPlayerIndex physicalPlayerIndex = remotePlayer->GetPhysicalPlayerIndex();

			if(physicalPlayerIndex != INVALID_PLAYER_INDEX && (m_DisableBandwidthRestrictions & (1<<physicalPlayerIndex))==0)
			{
				if(!CNetwork::GetNetworkSession().IsTransitioning() && NetworkInterface::GetBandwidthManager().GetBandwidthThrottlingRequired(*remotePlayer, netBandwidthMgr::BANDWIDTH_THROTTLE_CHECK_RELIABLES))
				{
					if(m_DisableBandwidthVoiceTime[physicalPlayerIndex] == 0)
					{
						gnetDebug1("UpdateBandwidthRestrictions :: Restricted for %s.", remotePlayer->GetLogName());
						m_DisableBandwidthVoiceTime[physicalPlayerIndex] = INITIAL_DISABLE_BANDWIDTH_VOICE_TIME_MS;
					}
					else
					{
						u32 timestepInMs = fwTimer::GetSystemTimeStepInMilliseconds();
						m_DisableBandwidthVoiceTime[physicalPlayerIndex] = rage::Min(m_DisableBandwidthVoiceTime[physicalPlayerIndex] + timestepInMs, MAX_DISABLE_BANDWIDTH_VOICE_TIME_MS);
					}
				}
				else if(m_DisableBandwidthVoiceTime[physicalPlayerIndex] > 0)
				{
					u32 timestepInMs = fwTimer::GetSystemTimeStepInMilliseconds();

					if(m_DisableBandwidthVoiceTime[physicalPlayerIndex] > timestepInMs)
					{
						m_DisableBandwidthVoiceTime[physicalPlayerIndex] -= timestepInMs;
					}
					else
					{
						gnetDebug1("UpdateBandwidthRestrictions :: Removing restrictions for %s.", remotePlayer->GetLogName());
						m_DisableBandwidthVoiceTime[physicalPlayerIndex] = 0;
					}
				}
			}
		}
	}

	// reduce the voice channel limit and number of recipients as bandwidth decreases
	struct VoicebandwidthData
	{
		u32 m_BandwidthThreshold;
		u32 m_VoiceChannelLimit;
		u32 m_MaxVoiceRecipients;
	};

	static const unsigned NUM_BANDWIDTH_DATA = 4;

	VoicebandwidthData bandwidthData[NUM_BANDWIDTH_DATA] =
	{
		{ 1024, DEFAULT_VOICE_BANDWIDTH_LIMIT,   DEFAULT_MAX_VOICE_RECIPIENTS },
		{ 256,  128 * 1024,                      DEFAULT_MAX_VOICE_RECIPIENTS },
		{ 128,  64  * 1024,                      8 },
		{ 64,   30  * 1024,                      4 },
	};

	unsigned targetUpstreamBandwidth = NetworkInterface::GetBandwidthManager().GetTargetUpstreamBandwidth();

	if(targetUpstreamBandwidth != m_LastTargetUpstreamBandwidth)
	{
		unsigned bandwidthIndex = 0;

		for(unsigned index = 0; index < NUM_BANDWIDTH_DATA; index++)
		{
			bandwidthIndex = index;

			if(targetUpstreamBandwidth >= bandwidthData[index].m_BandwidthThreshold)
			{
				break;
			}
		}

		m_nMaxBandwidthVoiceRecipients = bandwidthData[bandwidthIndex].m_MaxVoiceRecipients;
		SetVoiceBandwidthLimit(bandwidthData[bandwidthIndex].m_VoiceChannelLimit);

		gnetDebug1("UpdateBandwidthRestrictions :: Setting maximum recipients to %d", m_nMaxBandwidthVoiceRecipients);

		m_LastTargetUpstreamBandwidth = targetUpstreamBandwidth;
	}
}

bool CNetworkVoice::IsTextFilteringEnabled()
{
	return m_IsTextFilterEnabled || PARAM_netTextMessageEnableFilter.Get();
}

#if __BANK

static NetworkTextFilter s_BankTextFilter;

static bool s_BankTextTest = false;
static unsigned s_BankTextTestNumSent = 0;
static unsigned s_BankTextTestNumSends = DEFAULT_FILTER_SPAM_MAX_REQUESTS;
static unsigned s_BankTextTestInterval = DEFAULT_FILTER_SPAM_INTERVAL_MS / DEFAULT_FILTER_SPAM_MAX_REQUESTS;
static unsigned s_BankTextTestStarted = 0;

static unsigned s_BankTextTestNumMessages = DEFAULT_FILTER_SPAM_MAX_REQUESTS;
static unsigned s_BankTextTestIntervalMs = DEFAULT_FILTER_SPAM_INTERVAL_MS;

void Bank_TestTextCustom()
{
	gnetDebug1("Bank_TestTextCustom");

	s_BankTextTest = true;
	s_BankTextTestNumSent = 0;
	s_BankTextTestNumSends = s_BankTextTestNumMessages;
	s_BankTextTestInterval = s_BankTextTestIntervalMs / s_BankTextTestNumSends;
	s_BankTextTestStarted = sysTimer::GetSystemMsTime();
}

void Bank_TextTestHoldingPen()
{
	gnetDebug1("Bank_TextTestHoldingPen");

	s_BankTextTest = true;
	s_BankTextTestNumSent = 0;
	s_BankTextTestNumSends = DEFAULT_FILTER_HOLDING_PEN_MAX_REQUESTS + 2;
	s_BankTextTestInterval = DEFAULT_FILTER_HOLDING_PEN_INTERVAL_MS / s_BankTextTestNumSends;
	s_BankTextTestStarted = sysTimer::GetSystemMsTime();
}

void Bank_TextTestSpam()
{
	gnetDebug1("Bank_TextTestSpam");

	s_BankTextTest = true;
	s_BankTextTestNumSent = 0;
	s_BankTextTestNumSends = DEFAULT_FILTER_SPAM_MAX_REQUESTS + 2;
	s_BankTextTestInterval = DEFAULT_FILTER_SPAM_INTERVAL_MS / s_BankTextTestNumSends;
	s_BankTextTestStarted = sysTimer::GetSystemMsTime();
}

void Bank_ProcessTextMessage(const rlGamerHandle& UNUSED_PARAM(sender), const char* text)
{
	gnetDebug1("Bank_ProcessTextMessage :: Text: %s", text);
}

void CNetworkVoice::Bank_Update()
{
	if(s_BankTextTest)
	{
		unsigned nextSendTime = s_BankTextTestStarted + (s_BankTextTestNumSent * s_BankTextTestInterval);
		if(sysTimer::GetSystemMsTime() > nextSendTime)
		{
			char szText[NetworkTextFilter::MAX_TEXT_LENGTH];
			formatf(szText, "Bank_TextTest :: %d", s_BankTextTestNumSent + 1);

			gnetDebug1("Bank_Update :: Sending: %s", szText);
			
			s_BankTextFilter.AddReceivedText(rlGamerHandle(), szText);

			s_BankTextTestNumSent++;
			if(s_BankTextTestNumSent >= s_BankTextTestNumSends)
			{
				gnetDebug1("Bank_Update :: Finished Text Test");
				s_BankTextTest = false;
			}
		}
	}

	s_BankTextFilter.Update();
}

void CNetworkVoice::InitWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Network");
	if(gnetVerify(bank))
	{
		bkGroup* group = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live/Debug Voice Chat"));
		if(group)
			bank->DeleteGroup(*group);

		m_BankVpdThreshold = VPD_THRESHOLD;
		m_BankSendInterval = SEND_INTERVAL;

		m_BankPlayerCombo = NULL;
		m_BankPlayerIndex = 0;
		m_BankPlayers[0] = "--No Players--";
		m_BankNumPlayers = 1;

		m_BankOverrideAllRestrictions      = m_bOverrideAllRestrictions;
		m_BankTeamOnlyChat                 = m_TeamOnlyChat;
		m_BankDisplayBandwidthRestrictions = false;

		m_BankLocalPlayerTalkersMet = 0;
		m_BankLocalPlayerMuteCount = 0;
		m_BankTalkerCount = 0;

		s_BankTextFilter.Init(
			MakeFunctor(&Bank_ProcessTextMessage),
			rlGamerHandle(),
			DEFAULT_FILTER_HOLDING_PEN_MAX_REQUESTS,
			DEFAULT_FILTER_HOLDING_PEN_INTERVAL_MS,
			DEFAULT_FILTER_SPAM_MAX_REQUESTS,
			DEFAULT_FILTER_SPAM_INTERVAL_MS,
			"Bank");

		bank->PushGroup("Debug Voice Chat");
		{
			bank->AddText("Number of Talkers", &m_BankTalkerCount, true);
			bank->AddButton("Build Mute Lists", datCallback(MFA(CNetworkVoice::Bank_BuildMuteLists), (datBase*)this));
			bank->AddToggle("Team Only Chat", &m_BankTeamOnlyChat, datCallback(MFA(CNetworkVoice::Bank_SetTeamOnlyChat), (datBase*)this));
			bank->AddToggle("Override All Restrictions", &m_BankOverrideAllRestrictions, datCallback(MFA(CNetworkVoice::Bank_OverrideAllRestrictions), (datBase*)this));
			bank->AddToggle("Display Bandwidth Restrictions", &m_BankDisplayBandwidthRestrictions);

			bank->AddSlider("Voice Proximity Threshold", &m_BankVpdThreshold, 0, 10, 1);
			bank->AddSlider("Voice Send Interval", &m_BankSendInterval, 20, 1000, 10);
			bank->AddButton("Apply Threshold / Interval", datCallback(MFA(CNetworkVoice::Bank_ApplyValues), (datBase*)this));

			bank->AddButton("Check Communication", datCallback(MFA(CNetworkVoice::Bank_CheckCommunication), (datBase*)this));

			m_BankPlayerCombo = bank->AddCombo("Players", &m_BankPlayerIndex, m_BankNumPlayers, (const char **)m_BankPlayers);
			bank->AddButton("Refresh Player List", datCallback(MFA(CNetworkVoice::Bank_RefreshPlayerLists), (datBase*)this));
			bank->AddButton("Set Focus Player", datCallback(MFA(CNetworkVoice::Bank_SetFocusPlayer), (datBase*)this));
			bank->AddButton("Clear Focus Player", datCallback(MFA(CNetworkVoice::Bank_ClearFocusPlayer), (datBase*)this));
			bank->AddButton("Mute Player", datCallback(MFA(CNetworkVoice::Bank_MutePlayer), (datBase*)this));
			bank->AddButton("Unmute Player", datCallback(MFA(CNetworkVoice::Bank_UnmutePlayer), (datBase*)this));
			bank->AddSeparator();
			bank->AddText("Local Player Mute count", &m_BankLocalPlayerMuteCount, true);
			bank->AddText("Local Player Talkers Met", &m_BankLocalPlayerTalkersMet, true);

			bank->AddSeparator();
			bank->AddSlider("NumTextMessages", &s_BankTextTestNumMessages, 0, 200, 1);
			bank->AddSlider("IntervalMs", &s_BankTextTestIntervalMs , 0, 30000, 100);
			bank->AddButton("Test Custom", datCallback(Bank_TestTextCustom));
			bank->AddButton("Test Holding Pen Rate Limit", datCallback(Bank_TextTestHoldingPen));
			bank->AddButton("Test Spam Rate Limit", datCallback(Bank_TextTestSpam));
		}
		bank->PopGroup();
	}
}

void CNetworkVoice::Bank_OverrideAllRestrictions()
{
	if(m_BankOverrideAllRestrictions != m_bOverrideAllRestrictions)
		m_bOverrideAllRestrictions = m_BankOverrideAllRestrictions;
}

void CNetworkVoice::Bank_SetTeamOnlyChat()
{
	if(m_BankTeamOnlyChat != m_TeamOnlyChat)
		m_TeamOnlyChat = m_BankTeamOnlyChat;
}

void CNetworkVoice::Bank_BuildMuteLists()
{
	BuildMuteLists();
}

void CNetworkVoice::Bank_ApplyValues()
{
	if(m_VoiceChat.GetVpdThreshold() != m_BankVpdThreshold)
		m_VoiceChat.SetVpdThreshold(m_BankVpdThreshold);

	if(m_VoiceChat.GetSendInterval() != m_BankSendInterval)
		m_VoiceChat.SetSendInterval(m_BankSendInterval);
}

void CNetworkVoice::Bank_RefreshPlayerLists()
{
	m_BankNumPlayers = 0;

	unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
	const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

	for(unsigned index = 0; index < numRemoteActivePlayers; index++)
	{
		const netPlayer* pPlayer = remoteActivePlayers[index];
		m_BankPlayers[m_BankNumPlayers] = pPlayer->GetGamerInfo().GetName();
		m_BankNumPlayers++;
	}

	if(m_BankNumPlayers == 0)
	{
		m_BankPlayers[0] = "--No Players--";
		m_BankNumPlayers = 1;
	}

	m_BankPlayerIndex = Min(m_BankPlayerIndex, static_cast<int>((m_BankNumPlayers - 1)));
	m_BankPlayerIndex = Max(m_BankPlayerIndex, 0);

	if(m_BankPlayerCombo)
		m_BankPlayerCombo->UpdateCombo("Player Focus", &m_BankPlayerIndex, m_BankNumPlayers, (const char**)m_BankPlayers);
}

const CNetGamePlayer* CNetworkVoice::Bank_GetChosenPlayer()
{
	if(unsigned(m_BankPlayerIndex) < m_BankNumPlayers)
	{
		const char* targetName = m_BankPlayers[m_BankPlayerIndex];
		gnetAssert(targetName);

		unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers; index++)
		{
			const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remoteActivePlayers[index]);
			if(!strcmp(pPlayer->GetGamerInfo().GetName(), targetName))
				return pPlayer;
		}
	}

	return NULL;
}

void CNetworkVoice::Bank_MutePlayer()
{
	const CNetGamePlayer* pPlayer = Bank_GetChosenPlayer();
	if(!pPlayer)
		return;

	const rlGamerHandle& gamerHandle = pPlayer->GetGamerInfo().GetGamerHandle();
	if(m_FocusTalker.IsValid() && m_FocusTalker == gamerHandle)
		gnetWarning("Cannot Mute Player \"%s\" since he is the focus player.", pPlayer->GetLogName());
	else
	{
		gnetDebug1("Player \"%s\" Muted=\"%s\"", pPlayer->GetLogName(), IsGamerMutedByMe(pPlayer->GetGamerInfo().GetGamerHandle()) ? "FALSE" : "TRUE");
		MuteGamer(pPlayer->GetGamerInfo().GetGamerHandle(), !IsGamerMutedByMe(pPlayer->GetGamerInfo().GetGamerHandle()));
	}
}

void CNetworkVoice::Bank_UnmutePlayer()
{
	const CNetGamePlayer* pPlayer = Bank_GetChosenPlayer();
	if(!pPlayer)
		return;

	if(IsGamerMutedByMe(pPlayer->GetGamerInfo().GetGamerHandle()))
	{
		gnetDebug1("Player \"%s\" Muted=\"FALSE\"", pPlayer->GetLogName());
		NetworkInterface::GetVoice().MuteGamer(pPlayer->GetGamerInfo().GetGamerHandle(), false);
	}
}

void CNetworkVoice::Bank_CheckCommunication()
{
	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	CNetGamePlayer* pFocusPlayer = m_FocusTalker.IsValid() ? NetworkInterface::GetPlayerFromGamerHandle(m_FocusTalker) : NULL;

	gnetDebug1("==== VOICE COMMUNICATION CHECK ====");

	gnetDebug1("Voice Restrictions: %s", m_bOverrideAllRestrictions ? "Active" : "Not Active");
	gnetDebug1("Voice Communication: %s", m_bIsCommunicationActive ? "Active" : "Not Active");
	gnetDebug1("   Override: %s", PARAM_voiceOverrideActive.Get() ? "Active" : "Not Active");
	gnetDebug1("Transition Override Script: %s", m_bOverrideTransitionChatScript ? "Active" : "Not Active");
	gnetDebug1("Transition Override: %s", m_bOverrideTransitionChat ? "Active" : "Not Active");
	gnetDebug1("Override Transition Restrictions: %d", m_nOverrideTransitionRestrictions);
	gnetDebug1("Voice Group: %d", m_nVoiceGroup);
	gnetDebug1("Max Voice Recipients: %d", m_nMaxVoiceRecipients);
	gnetDebug1("Max Bandwidth Voice Recipients: %d", m_nMaxBandwidthVoiceRecipients);
	gnetDebug1("Team Chat Only: %s", m_TeamOnlyChat ? "True" : "False");
	gnetDebug1("   Command Override: %s", PARAM_voiceOverrideTeamChat.Get() ? "True" : "False");
	gnetDebug1("   Team Chat Override: %d", m_OverrideTeamRestrictions);
	gnetDebug1("Chat Override: %d", m_OverrideChatRestrictions);
	gnetDebug1("Send Override Local: %d", m_OverrideChatRestrictionsSendLocal);
	gnetDebug1("Receive Override Local: %d", m_OverrideChatRestrictionsReceiveLocal);
	gnetDebug1("Send Override Remote: %d", m_OverrideChatRestrictionsSendRemote);
	gnetDebug1("Receive Override Remote: %d", m_OverrideChatRestrictionsReceiveRemote);
	gnetDebug1("Override Spectator: %s", m_OverrideSpectatorMode ? "True" : "False");
	gnetDebug1("Override Tutorial Session: %s", m_OverrideTutorialSession ? "True" : "False");
	gnetDebug1("Override Tutorial Restrictions: %d", m_OverrideTutorialRestrictions);
	gnetDebug1("Focus Player: %s", pFocusPlayer ? pFocusPlayer->GetGamerInfo().GetName() : "None");
	gnetDebug1("Talker Proximity: %f", m_TalkerProximityLo);
	gnetDebug1("Talker Proximity Height: %f", m_TalkerProximityHeightLo);
	gnetDebug1("Loudhailer Proximity: %f", m_LoudhailerProximityLo);
	gnetDebug1("Loudhailer Proximity Height: %f", m_LoudhailerProximityHeightLo);
	gnetDebug1("Local Player:");
	gnetDebug1("   Name: %s", pLocalPlayer->GetGamerInfo().GetName());
	gnetDebug1("   Have Talker: %s", m_VoiceChat.HaveTalker(pLocalPlayer->GetGamerInfo().GetGamerId()) ? "True" : "False");
	gnetDebug1("   Talker Flags: 0x%x", m_VoiceChat.GetTalkerFlags(pLocalPlayer->GetGamerInfo().GetGamerId()));
	gnetDebug1("   Has Headset: %s", m_LocalGamer.HasHeadset() ? "True" : "False");
	gnetDebug1("   Through Speakers: %s", m_LocalGamer.IsVoiceThruSpeakers() ? "True" : "False");
	gnetDebug1("   Team: %d", pLocalPlayer->GetTeam());
	gnetDebug1("   Voice Channel: %d", pLocalPlayer->GetVoiceChannel());
	const CPed* pLocalPed = pLocalPlayer->GetPlayerPed();
	gnetDebug1("   Has Ped: %s", pLocalPed ? "True" : "False");
	if(pLocalPed)
	{
		Vector3 vPos;
		pLocalPlayer->GetVoiceProximity(vPos);
		gnetDebug1("   Position: %.1f, %.1f, %.1f", vPos.x, vPos.y, vPos.z);
		const CPedWeaponManager* pWM = pLocalPed->GetWeaponManager();
		bool bHasLoudhailer = pWM && (pWM->GetEquippedWeaponObjectHash() == WEAPONTYPE_LOUDHAILER) && pLocalPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun);
		gnetDebug1("   Has Loudhailer: %s", bHasLoudhailer ? "True" : "False");
		CNetObjPlayer* pNetObj = static_cast<CNetObjPlayer*>(pLocalPed->GetNetworkObject());
		gnetDebug1("   Has Network Object: %s", pNetObj ? "True" : "False");
		if(pNetObj)
		{
			gnetDebug1("   Tutorial Index: %d", pNetObj->GetTutorialIndex());
			gnetDebug1("   Tutorial Instance: %d", pNetObj->GetTutorialInstanceID());
			gnetDebug1("   Is Spectating: %s", pNetObj->IsSpectating() ? "True" : "False");
		}
	}

	unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
	const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

	for(unsigned index = 0; index < numRemoteActivePlayers; index++)
	{
		const CNetGamePlayer* pPlayer = SafeCast(const CNetGamePlayer, remoteActivePlayers[index]);
		const VoiceGamerSettings* pVoiceGamer = FindGamer(pPlayer->GetGamerInfo().GetGamerHandle());
		if(!gnetVerify(pVoiceGamer))
			continue; 

		gnetDebug1("Remote Player:");
		gnetDebug1("   Name: %s", pPlayer->GetGamerInfo().GetName());
		gnetDebug1("   Have Talker: %s", m_VoiceChat.HaveTalker(pPlayer->GetGamerInfo().GetGamerId()) ? "True" : "False");
		gnetDebug1("   Talker Flags: 0x%x", m_VoiceChat.GetTalkerFlags(pPlayer->GetGamerInfo().GetGamerId()));
		gnetDebug1("   Is Bot: %s", pPlayer->IsBot() ? "True" : "False");
		gnetDebug1("   Has Headset: %s", pVoiceGamer->HasHeadset() ? "True" : "False");
		gnetDebug1("   Through Speakers: %s", pVoiceGamer->IsVoiceThruSpeakers() ? "True" : "False");
		gnetDebug1("   Team: %d", pPlayer->GetTeam());
		gnetDebug1("   Voice Channel: %d", pPlayer->GetVoiceChannel());
		gnetDebug1("   Is Blocked By Me: %s", pVoiceGamer->IsBlockedByMe() ? "True" : "False");
		gnetDebug1("   Is Muted By Me: %s", pVoiceGamer->IsMutedByMe() ? "True" : "False");
		gnetDebug1("   Is Blocking Me: %s", pVoiceGamer->IsBlockedByHim() ? "True" : "False");
		gnetDebug1("   Is Muting Me: %s", pVoiceGamer->IsMutedByHim() ? "True" : "False");

		PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();

		if(playerIndex != INVALID_PLAYER_INDEX)
		{
			gnetDebug1("   Is Bandwidth Restricted: %s", m_DisableBandwidthVoiceTime[playerIndex] > 0 ? "True" : "False");
		}

		const CPed* pRemotePed = pPlayer->GetPlayerPed();
		gnetDebug1("   Has Ped: %s", pRemotePed ? "True" : "False");
		if(pRemotePed)
		{
			Vector3 vPos;
			pPlayer->GetVoiceProximity(vPos);
			gnetDebug1("   Position: %.1f, %.1f, %.1f", vPos.x, vPos.y, vPos.z);
			const CPedWeaponManager* pWM = pRemotePed->GetWeaponManager();
			bool bHasLoudhailer = pWM && (pWM->GetEquippedWeaponObjectHash() == WEAPONTYPE_LOUDHAILER) && pRemotePed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun);
			gnetDebug1("   Has Loudhailer: %s", bHasLoudhailer ? "True" : "False");
			CNetObjPlayer* pNetObj = static_cast<CNetObjPlayer*>(pRemotePed->GetNetworkObject());
			gnetDebug1("   Has Network Object: %s", pNetObj ? "True" : "False");
			if(pNetObj)
			{
				gnetDebug1("   Tutorial Index: %d", pNetObj->GetTutorialIndex());
				gnetDebug1("   Tutorial Instance: %d", pNetObj->GetTutorialInstanceID());
				gnetDebug1("   Is Spectating: %s", pNetObj->IsSpectating() ? "True" : "False");
			}
		}

		if(pLocalPed && pRemotePed)
		{
			Vector3 vLocal, vRemote;
			Vector3 diff = pLocalPlayer->GetVoiceProximity(vLocal) - pPlayer->GetVoiceProximity(vRemote);
			gnetDebug1("Position Diff:  %.1f, %.1f, %.1f", diff.x, diff.y, diff.z);
			gnetDebug1("Position Mag:  %.2f", diff.Mag());
		}
	}

	m_VoiceChat.LogContents();
}

void CNetworkVoice::Bank_SetFocusPlayer()
{
	const CNetGamePlayer* pPlayer = Bank_GetChosenPlayer();
	if(!pPlayer)
		return;

	if(IsGamerMutedByMe(pPlayer->GetGamerInfo().GetGamerHandle()))
		gnetWarning("Can NOT focus Player \"%s\" since he is muted.", pPlayer->GetLogName());
	else
	{
		gnetDebug1("Send Player \"%s\" a Chat request.", pPlayer->GetLogName());
		CMsgVoiceChatRequest msg;
		NetworkInterface::SendReliableMessage(pPlayer, msg);
		NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent();

		SetTalkerFocus(&pPlayer->GetGamerInfo().GetGamerHandle());
	}
}

void CNetworkVoice::Bank_ClearFocusPlayer()
{
	if(!m_FocusTalker.IsValid())
		return;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(m_FocusTalker);
	if(!AssertVerify(pPlayer))
		return;

	gnetDebug1("Send Focus Player \"%s\" Chat end.", pPlayer->GetLogName());
	CMsgVoiceChatEnd msg;
	NetworkInterface::SendReliableMessage(pPlayer, msg);

	NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent();

	m_FocusTalker.Clear();
}

void CNetworkVoice::OnMessageReceived(const ReceivedMessageData &messageData)
{
	unsigned msgId = 0;
	if(netMessage::GetId(&msgId, messageData.m_MessageData, messageData.m_MessageDataSize))
	{
		if (msgId == CMsgVoiceChatReply::MSG_ID())
			this->Bank_HandleChatReply(messageData);
		else if (msgId == CMsgVoiceChatRequest::MSG_ID())
			this->Bank_HandleChatRequest(messageData);
		else if (msgId == CMsgVoiceChatEnd::MSG_ID())
			this->Bank_HandleChatEnd(messageData);
	}
}

void CNetworkVoice::Bank_HandleChatReply(const ReceivedMessageData &messageData)
{
	if(!m_FocusTalker.IsValid())
	{
		gnetDebug1("Start private chat with Player \"%s\".", messageData.m_FromPlayer->GetLogName());
		const rlGamerHandle* gamerHandle = &messageData.m_FromPlayer->GetGamerInfo().GetGamerHandle();
		SetTalkerFocus(gamerHandle);
	}
	else
	{
		gnetWarning("We are already in a private chat, cant accept request from \"%s\".", messageData.m_FromPlayer->GetLogName());
	}
}

void CNetworkVoice::Bank_HandleChatRequest(const ReceivedMessageData &messageData)
{
	if(!m_FocusTalker.IsValid())
	{
		gnetDebug1("Start private chat with Player \"%s\".", messageData.m_FromPlayer->GetLogName());
		const rlGamerHandle* gamerHandle = &messageData.m_FromPlayer->GetGamerInfo().GetGamerHandle();
		SetTalkerFocus(gamerHandle);

		gnetDebug1("Send Player \"%s\" a Chat reply.", messageData.m_FromPlayer->GetLogName());
		CMsgVoiceChatReply msg;
		NetworkInterface::SendReliableMessage(messageData.m_FromPlayer, msg);

		NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent();
	}
	else
	{
		gnetWarning("We are already in a private chat, cant accept request from '%s'.", messageData.m_FromPlayer->GetLogName());
	}
}

void CNetworkVoice::Bank_HandleChatEnd(const ReceivedMessageData &messageData)
{
	if(m_FocusTalker.IsValid() && messageData.m_FromPlayer->GetGamerInfo().GetGamerHandle() == m_FocusTalker)
	{
		gnetDebug1("End private chat with Player \"%s\".", messageData.m_FromPlayer->GetLogName());
		m_FocusTalker.Clear();
	}
}

void CNetworkVoice::DisplayBandwidthRestrictions()
{
	grcDebugDraw::AddDebugOutput("Voice Bandwidth Restrictions:");
	grcDebugDraw::AddDebugOutput("Last target upstream bandwidth %d", m_LastTargetUpstreamBandwidth);
	grcDebugDraw::AddDebugOutput("Maximum bandwidth voice recipients %d", m_nMaxBandwidthVoiceRecipients);

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const netPlayer *remotePlayer = remotePhysicalPlayers[index];

		if(remotePlayer)
		{
			PhysicalPlayerIndex physicalPlayerIndex = remotePlayer->GetPhysicalPlayerIndex();

			if(physicalPlayerIndex != INVALID_PLAYER_INDEX)
			{
				if((m_DisableBandwidthRestrictions & (1<<physicalPlayerIndex))!=0)
				{
					grcDebugDraw::AddDebugOutput("%s: Bandwidth restrictions disabled", remotePlayer->GetLogName());
				}
				else if(m_DisableBandwidthVoiceTime[physicalPlayerIndex] > 0)
				{
					grcDebugDraw::AddDebugOutput("%s: Bandwidth voice restriction time: %d", remotePlayer->GetLogName(), m_DisableBandwidthVoiceTime[physicalPlayerIndex]);
				}
			}
		}
	}
}

#endif  //__BANK

#if !__NO_OUTPUT
// these allow per run tracking of these stats
static bool s_bHasLoggedAutoMuteStats = false; 
static unsigned s_NumTalkersAddedForAutoMute = 0;
static unsigned s_NumMutesAddedForAutoMute = 0;
#endif

void CNetworkVoice::CheckAndAddPendingTalkersForAutoMute()
{
	// exit if we have no pending talkers
	if(m_PendingTalkersForAutoMute == 0)
		return;
	
	// check the relevant stats can be accessed
	if(!StatsInterface::CanAccessStat(STAT_PLAYER_MUTED) || !StatsInterface::CanAccessStat(STAT_PLAYER_MUTED_TALKERS_MET))
		return;

	// grab the current stat values
	u32 localPlayerMuteCount = StatsInterface::GetUInt32Stat(STAT_PLAYER_MUTED); 
	u32 localPlayerTalkerCount = StatsInterface::GetUInt32Stat(STAT_PLAYER_MUTED_TALKERS_MET);

#if !__NO_OUTPUT
	// log these stats the first time through
	if(!s_bHasLoggedAutoMuteStats)
	{
		gnetDebug1("CheckAndAddPendingTalkersForAutoMute :: Logging initial stats - Talkers: %u, Mutes: %u", localPlayerTalkerCount, localPlayerMuteCount);
		s_bHasLoggedAutoMuteStats = true; 
	}
	s_NumTalkersAddedForAutoMute += m_PendingTalkersForAutoMute;
#endif

	// validate stats
	if(!(localPlayerMuteCount <= localPlayerTalkerCount))
	{
		// stats are broken, just reset
		gnetError("CheckAndAddPendingTalkersForAutoMute :: More mutes [%u] than talkers [%u]. Resetting stats", localPlayerMuteCount, localPlayerTalkerCount);
		localPlayerMuteCount = 0;
		localPlayerTalkerCount = m_PendingTalkersForAutoMute;
		StatsInterface::SetStatData(STAT_PLAYER_MUTED, localPlayerMuteCount);
		StatsInterface::SetStatData(STAT_PLAYER_MUTED_TALKERS_MET, localPlayerTalkerCount);
	}
	else
	{
		// let's not update forever for no reason. If our talker count is REALLY big and we have barely any mute count, back it off
		if(localPlayerTalkerCount > 0xffffff && localPlayerMuteCount <= 5)
		{
			gnetDebug3("CheckAndAddPendingTalkersForAutoMute :: Skipping stat increment. Talkers: %u, Mutes: %u", localPlayerTalkerCount, localPlayerMuteCount);
			return;
		}

		// update our talker count
		StatsInterface::IncrementStat(STAT_PLAYER_MUTED_TALKERS_MET, static_cast<float>(m_PendingTalkersForAutoMute));
		localPlayerTalkerCount += m_PendingTalkersForAutoMute;
	}

	// update local player mute data stats for syncing
	if(NetworkInterface::IsNetworkOpen())
	{
		CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
		if(pLocalPlayer)
			pLocalPlayer->SetMuteData(localPlayerMuteCount, localPlayerTalkerCount);
	}

	// reset pending stat
	m_PendingTalkersForAutoMute = 0;
}

void CNetworkVoice::CheckAndAddPendingMutesForAutoMute()
{
	// exit if we have no pending talkers
	if(m_PendingMutesForAutoMute == 0)
		return;

	// check the relevant stats can be accessed
	if(!StatsInterface::CanAccessStat(STAT_PLAYER_MUTED) || !StatsInterface::CanAccessStat(STAT_PLAYER_MUTED_TALKERS_MET))
		return;

	// increment muted stat
	StatsInterface::IncrementStat(STAT_PLAYER_MUTED, static_cast<float>(m_PendingMutesForAutoMute));

#if !__NO_OUTPUT
	// validate per run stats
	s_NumMutesAddedForAutoMute += m_PendingMutesForAutoMute;
	gnetAssertf(s_NumMutesAddedForAutoMute <= s_NumTalkersAddedForAutoMute, "CheckAndAddPendingMutesForAutoMute :: More mutes [%u] than talkers [%u] added on this run", s_NumMutesAddedForAutoMute, s_NumTalkersAddedForAutoMute);
#endif

	// grab the current stat values
	const u32 localPlayerMuteCount = StatsInterface::GetUInt32Stat(STAT_PLAYER_MUTED); 
	const u32 localPlayerTalkerCount = StatsInterface::GetUInt32Stat(STAT_PLAYER_MUTED_TALKERS_MET);

	// validate stats
	gnetAssertf(localPlayerMuteCount <= localPlayerTalkerCount, "CheckAndAddPendingMutesForAutoMute :: Muted more talkers [%u] than met [%u]", localPlayerTalkerCount, localPlayerMuteCount);

	// update local player mute data stats for syncing
	if(NetworkInterface::IsNetworkOpen())
	{
		CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
		if (pLocalPlayer)
			pLocalPlayer->SetMuteData(localPlayerMuteCount, localPlayerTalkerCount);
	}

	// reset pending stat
	m_PendingMutesForAutoMute = 0;
}

#if RSG_PC
void CNetworkVoice::AdjustSettings(const FrontendVoiceSettings settings)
{
	const float FRONTEND_DIVISOR = 10.0f;

	float fMicVolume = (float)settings.m_iMicVolume / FRONTEND_DIVISOR;
	float fOutputVolume = (float)settings.m_iOutputVolume / FRONTEND_DIVISOR;

	m_bEnabled = settings.m_bEnabled;
	m_bTalkEnabled = settings.m_bTalkEnabled;

	gnetDebug1("AdjustSettings :: Enabled: %s", m_bEnabled ? "True" : "False");
	gnetDebug1("AdjustSettings :: Talk Enabled: %s", m_bTalkEnabled ? "True" : "False");

	if(m_bEnabled)
	{
		m_VoiceChat.SetPlaybackDeviceVolume(fOutputVolume);
		m_VoiceChat.SetCaptureDeviceVolume(fMicVolume);

		m_VoiceChat.SetVpdThreshold(settings.m_iMicSensitivity);
		m_VoiceChat.SetVpdThresholdPTT(0);

		m_VoiceChat.SetCaptureMode(settings.m_eMode);

		m_VoiceChat.SetPlaybackDeviceById32(settings.m_uOutputDevice);
		
		if(m_bTalkEnabled)
		{
			m_VoiceChat.SetCaptureDeviceById32(settings.m_uInputDevice);
		}
		else
		{
			m_VoiceChat.SetCaptureDeviceVolume(0);
			m_VoiceChat.FreeCaptureDevice();
		}
	}
	else
	{
		m_VoiceChat.SetCaptureDeviceVolume(0);
		m_VoiceChat.SetPlaybackDeviceVolume(0);

		m_VoiceChat.FreePlaybackDevice();
		m_VoiceChat.FreeCaptureDevice();
	}
}

void CNetworkVoice::SetMuteChatBecauseLostFocus(bool bShouldMute)
{
	m_VoiceChat.SetMuteChatBecauseFocusLost(bShouldMute);
}
#endif // RSG_PC

// eof
