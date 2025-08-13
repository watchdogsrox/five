//
// name:        NetworkVoice.h
//

#ifndef NETWORKVOICE_H
#define NETWORKVOICE_H

// rage headers
#include "avchat/voicechat.h"

// framework headers
#include "fwnet/netinterface.h"

// game headers
#include "audio/voicechataudioentity.h"
#include "network/NetworkTypes.h"
#include "network/General/NetworkTextFilter.h"

#if RSG_DURANGO
#include "rline/durango/rlXblPrivacySettings_interface.h"
#endif

// Forward Defs
namespace rage
{
	class bkCombo;
	class rlXblChatEvent;
};
class CNetGamePlayer;

enum
{
	MAX_GAMERS =
		1 +								// local player
		(MAX_NUM_ACTIVE_PLAYERS - 1) +	// game session, minus local player
		(MAX_NUM_ACTIVE_PLAYERS - 1) +	// activity session, minus local player
		(MAX_NUM_PARTY_MEMBERS - 1) +	// party session, minus local player
		1,								// voice session
};
		
enum eVoiceGroup
{
	VOICEGROUP_NONE = 0,
	// and now in order of precedence
	VOICEGROUP_VOICE,
	VOICEGROUP_TRANSITION,
	VOICEGROUP_GAME,
	VOICEGROUP_ACTIVITY_STARTING,
	VOICEGROUP_SINGLEPLAYER,
	VOICEGROUP_NUM,
};

#if RSG_PC
struct FrontendVoiceSettings
{
	bool m_bEnabled;
	bool m_bOutputEnabled;
	bool m_bTalkEnabled;

	int m_iOutputVolume;
	int m_iMicVolume;
	int m_iMicSensitivity;

	u32 m_uInputDevice;
	u32 m_uOutputDevice;

	VoiceChat::CaptureMode m_eMode;
};
#endif // RSG_PC

struct VoiceGamerSettings
{
	enum VoiceFlag
	{
		VOICE_BITS					= 16,
		VOICE_LOCAL_TO_REMOTE_SHIFT = VOICE_BITS,
		VOICE_LOCAL_MASK			= (1 << VOICE_BITS) - 1,
		VOICE_REMOTE_MASK			= VOICE_LOCAL_MASK << VOICE_LOCAL_TO_REMOTE_SHIFT,

		VOICE_LOCAL_HAS_HEADSET     = BIT0,
		VOICE_LOCAL_MUTED           = BIT1, // Reflects the mute state set by the OS (e.g. the Xbox guide).
		VOICE_LOCAL_SYSTEM_MUTE		= BIT2, // Reflects a temporary mute state set by the OS (e.g. when the app is minimized/constrained).
		VOICE_LOCAL_FORCE_MUTED     = BIT3, // Reflects the mute state set by the app.
		VOICE_LOCAL_BLOCKED         = BIT4,
		VOICE_LOCAL_TEXT_BLOCKED    = BIT5,
		VOICE_LOCAL_THRU_SPEAKERS   = BIT6,
		VOICE_LOCAL_AUTO_MUTE       = BIT7,
		VOICE_LOCAL_ON_BLOCK_LIST	= BIT8,
		VOICE_LOCAL_UGC_BLOCKED		= BIT9,
		VOICE_NUM_BITS				= 10,
		
		VOICE_REMOTE_HAS_HEADSET    = VOICE_LOCAL_HAS_HEADSET	<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_MUTED          = VOICE_LOCAL_MUTED			<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_SYSTEM_MUTE	= VOICE_LOCAL_SYSTEM_MUTE	<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_FORCE_MUTED    = VOICE_LOCAL_FORCE_MUTED	<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_BLOCKED        = VOICE_LOCAL_BLOCKED		<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_TEXT_BLOCKED   = VOICE_LOCAL_TEXT_BLOCKED	<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_THRU_SPEAKERS  = VOICE_LOCAL_THRU_SPEAKERS << VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_AUTO_MUTE		= VOICE_LOCAL_AUTO_MUTE		<< VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_ON_BLOCK_LIST	= VOICE_LOCAL_ON_BLOCK_LIST << VOICE_LOCAL_TO_REMOTE_SHIFT,
		VOICE_REMOTE_UGC_BLOCKED	= VOICE_LOCAL_UGC_BLOCKED	<< VOICE_LOCAL_TO_REMOTE_SHIFT,
	};

	VoiceGamerSettings()
	{
		Clear();
	}

	VoiceGamerSettings(const rlGamerHandle& hGamer, bool bIsLocal)
	{
		// just call through
		Init(hGamer, bIsLocal);
	}

	void Init(const rlGamerHandle& hGamer, bool bIsLocal)
	{
		m_hGamer = hGamer;
		m_bIsLocal = bIsLocal;
		m_nVoiceFlags = 0;
		m_bVoiceFlagsDirty = false;
		m_nRefCount = 1;
		m_bTrackedForAutomute = false;
		m_bOverrideChatRestrictions = false;

#if RSG_DURANGO
		m_bIsMuted = false;
		m_bIsBlocked = false; 
		m_bIsFriend = false;
		m_VoiceCommunicationCheck.Reset(IPrivacySettings::PERM_CommunicateUsingVoice, hGamer);
		m_TextCommunicationCheck.Reset(IPrivacySettings::PERM_CommunicateUsingText, hGamer);
		m_UserContentCheck.Reset(IPrivacySettings::PERM_ViewTargetUserCreatedContent, hGamer);
		
		if(bIsLocal)
		{
			m_VoiceCommunicationCheck.SetResult(true);
			m_TextCommunicationCheck.SetResult(true);
			m_UserContentCheck.SetResult(true);
		}
#endif
	}

	void Clear()
	{
		m_hGamer.Clear();
		m_bIsLocal = true;
		m_nVoiceFlags = 0;
		m_bVoiceFlagsDirty = false;
		m_nRefCount = 0;
		m_bTrackedForAutomute = false;
		m_bOverrideChatRestrictions = false;

#if RSG_DURANGO
		m_bIsMuted = false;
		m_bIsBlocked = false; 
		m_bIsFriend = false;
		m_VoiceCommunicationCheck.Clear();
		m_TextCommunicationCheck.Clear();
		m_UserContentCheck.Clear();
#endif
	}

	void IncrementRefCount()
	{
		m_nRefCount++;
	}

	void DecrementRefCount()
	{
		if(m_nRefCount > 0)
			m_nRefCount--;
	}

	int GetNumReferences()
	{
		return m_nRefCount;
	}

	bool HasReferences()
	{
		return m_nRefCount > 0;
	}

	const rlGamerHandle& GetGamerHandle() const { return m_hGamer; }
	bool IsLocal() const { return m_bIsLocal; }

	//PURPOSE
	// Sets one of the voice flags we're allowed to set locally
	//PARAMS
	// nFlags - The flag to change
	// bValue - Whether to set or clear the flag
	void SetLocalVoiceFlag(const VoiceFlag nFlag, const bool bValue)
	{
		gnetAssert(0 == (VOICE_REMOTE_MASK & nFlag));

		// get local flags, keep a copy for dirty checking
		unsigned nLocalFlags = GetLocalVoiceFlags();
		unsigned nOldFlags = nLocalFlags;

		// make new local flags
		nLocalFlags = bValue ? (nLocalFlags | nFlag) : (nLocalFlags & ~nFlag);

		// apply local flags and set dirty value if changed
		m_nVoiceFlags = GetRemoteVoiceFlags() | (nLocalFlags & VOICE_LOCAL_MASK);
		m_bVoiceFlagsDirty |= (GetLocalVoiceFlags() != nOldFlags);
	}

	//PURPOSE
	// Set the remote voice flags
	//PARAMS
	// flags - The new value for the flags
	void SetRemoteVoiceFlags(const unsigned nFlags)
	{
		gnetAssert(0 == (VOICE_LOCAL_MASK & nFlags));
		m_nVoiceFlags = this->GetLocalVoiceFlags() | (nFlags & VOICE_REMOTE_MASK);
	}

	//PURPOSE
	// Returns the local voice flags
	unsigned GetLocalVoiceFlags() const
	{
		return m_nVoiceFlags & VOICE_LOCAL_MASK;
	}

	//PURPOSE
	// Returns the remote voice flags
	unsigned GetRemoteVoiceFlags() const
	{
		return m_nVoiceFlags & VOICE_REMOTE_MASK;
	}

	//PURPOSE
	// Returns whether any voice flags are dirty
	unsigned AreVoiceFlagsDirty() const
	{
		return m_bVoiceFlagsDirty;
	}

	//PURPOSE
	// Sets the dirty status of all voice flags
	void SetDirtyVoiceFlags()
	{
		m_bVoiceFlagsDirty = true;
	}

	//PURPOSE
	// Clears the dirty status of all voice flags
	void CleanDirtyVoiceFlags()
	{
		m_bVoiceFlagsDirty = false;
	}

	//PURPOSE
	// Convert local flags to remote flags.
	//PARAMS
	// localFlags - The flags to convert
	static unsigned LocalToRemoteVoiceFlags(const unsigned nLocalFlags)
	{
		gnetAssert(!(nLocalFlags & VOICE_REMOTE_MASK));
		return (nLocalFlags << VOICE_LOCAL_TO_REMOTE_SHIFT) & VOICE_REMOTE_MASK;
	}

	//PURPOSE
	// Convert remote flags to local flags.
	//PARAMS
	// remoteFlags - The flags to convert
	static unsigned RemoteToLocalVoiceFlags(const unsigned nRemoteFlags)
	{
		gnetAssert(!(nRemoteFlags & VOICE_LOCAL_MASK));
		return (nRemoteFlags >> VOICE_LOCAL_TO_REMOTE_SHIFT) & VOICE_LOCAL_MASK;
	}

	//PURPOSE
	// Returns whether this player has a headset connected
	bool HasHeadset() const
	{
		return m_bIsLocal
			? (0 != (VOICE_LOCAL_HAS_HEADSET & m_nVoiceFlags))
			: (0 != (VOICE_REMOTE_HAS_HEADSET & m_nVoiceFlags));
	}

	//PURPOSE
	// Returns whether this player has been muted by the local player
	bool IsMutedByMe() const
	{
		return 0 != ((VOICE_LOCAL_MUTED | VOICE_LOCAL_FORCE_MUTED) & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether this player has been muted by the local player
	bool IsForceMutedByMe() const
	{
		return 0 != ((VOICE_LOCAL_FORCE_MUTED) & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether we have been muted by this player
	bool IsMutedByHim() const
	{
		return 0 != ((VOICE_REMOTE_MUTED | VOICE_REMOTE_FORCE_MUTED) & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether we have been muted by this player
	bool IsForceMutedByHim() const
	{
		return 0 != ((VOICE_REMOTE_FORCE_MUTED) & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether this player has been blocked by the local player
	bool IsBlockedByMe() const
	{
		return 0 != (VOICE_LOCAL_BLOCKED & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether this player has blocked the local player
	bool IsBlockedByHim() const
	{
		return 0 != (VOICE_REMOTE_BLOCKED & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether the local player can send text communication to the remote player
	bool IsTextBlockedByMe() const
	{
		return 0 != (VOICE_LOCAL_TEXT_BLOCKED & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether this player can send text communication to the local player
	bool IsTextBlockedByHim() const
	{
		return 0 != (VOICE_REMOTE_TEXT_BLOCKED & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether the local player can view user generated content
	// This corresponds to the general permission (and is not specific to the remote player)
	bool IsContentBlockedByLocal() const
	{
		return 0 != (VOICE_LOCAL_UGC_BLOCKED & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether the remote player can view user generated content
	// This corresponds to the general permission (and is not specific to the local player)
	bool IsContentBlockedByRemote() const
	{
		return 0 != (VOICE_REMOTE_UGC_BLOCKED & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether this player has been muted by an OS thing (e.g. app is constrained)
	bool IsSystemMuted() const 
	{
		return 0 != (VOICE_LOCAL_SYSTEM_MUTE & m_nVoiceFlags);
	}

	// Can we send or receive voice from this player.
	bool CanCommunicateVoiceWith() const
	{
		return !(IsMutedByMe() || IsMutedByHim() || IsBlockedByMe() || IsBlockedByHim() || IsSystemMuted());
	}

	// Can we send or receive text from this gamer.
	bool CanCommunicateTextWith() const
	{
#if RSG_DURANGO
		return !(IsTextBlockedByMe() || IsTextBlockedByHim() || IsBlockedByMe() || IsBlockedByHim());
#else
		return CanCommunicateVoiceWith();
#endif
	}

	//PURPOSE
	// Returns whether the local player is auto-muted
	bool IsLocalAutomuted() const
	{
		return 0 != (VOICE_LOCAL_AUTO_MUTE & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether the remote player is auto-muted
	bool IsRemoteAutomuted() const
	{
		return 0 != (VOICE_REMOTE_AUTO_MUTE & m_nVoiceFlags);
	}

	//PURPOSE
	// Returns whether either the local or remote player has auto-muted the other
	bool IsAutoMuted() const
	{
		return (IsLocalAutomuted() || IsRemoteAutomuted());
	}

	//PURPOSE
	// Marks a player as tracked for auto mute (prevents toggling mute from incrementing multiple times)
	void SetTrackedForAutomute()
	{
		m_bTrackedForAutomute = true;
	}

	//PURPOSE
	// Returns whether we've already tagged this player as tracked
	bool HasTrackedForAutomute() const
	{
		return m_bTrackedForAutomute;
	}

	//PURPOSE
	// Sets whether we are overriding this player for voice chat
	void SetOverrideChatRestrictions(bool bOverride)
	{
		m_bOverrideChatRestrictions = bOverride;
	}

	//PURPOSE
	// Returns if this player is overriding chat restrictions
	bool GetOverrideChatRestrictions() const
	{
		return m_bOverrideChatRestrictions;
	}

	//PURPOSE
	// Returns whether this players voice is being directed through their TV speakers
	bool IsVoiceThruSpeakers() const
	{
		return m_bIsLocal
			? (0 != (VOICE_LOCAL_THRU_SPEAKERS & m_nVoiceFlags))
			: (0 != (VOICE_REMOTE_THRU_SPEAKERS & m_nVoiceFlags));
	}

	//PURPOSE
	// Returns whether this player is on the local blocklist
	bool IsRemoteOnLocalBlocklist() const
	{
		return (0 != (VOICE_LOCAL_ON_BLOCK_LIST & m_nVoiceFlags));
	}

	//PURPOSE
	// Returns whether the local player is on the remote blocklist
	bool IsLocalOnRemoteBlocklist() const
	{
		return (0 != (VOICE_REMOTE_ON_BLOCK_LIST & m_nVoiceFlags));
	}

#if RSG_XBOX
	void CheckAllPermissions()
	{
		m_VoiceCommunicationCheck.CheckPermission();
		m_TextCommunicationCheck.CheckPermission();
		m_UserContentCheck.CheckPermission();
	}
#endif

	bool HasVoiceCommunicationCheckResult() const
	{
#if RSG_DURANGO
		return m_VoiceCommunicationCheck.HasResult();
#else
		return true;
#endif
	}

	bool IsVoiceCommunicationCheckPending() const
	{
#if RSG_DURANGO
		return m_VoiceCommunicationCheck.Pending();
#else
		return false;
#endif
	}

	bool HasTextCommunicationCheckResult() const
	{
#if RSG_DURANGO
		return m_TextCommunicationCheck.HasResult();
#else
		return true;
#endif
	}

	bool IsTextCommunicationCheckPending() const
	{
#if RSG_DURANGO
		return m_TextCommunicationCheck.Pending();
#else
		return false;
#endif
	}

	bool HasUserContentCheckResult() const
	{
#if RSG_DURANGO
		return m_UserContentCheck.HasResult();
#else
		return true;
#endif
	}

	bool IsUserContentCheckPending() const
	{
#if RSG_DURANGO
		return m_UserContentCheck.Pending();
#else
		return false;
#endif
	}

	bool CanViewUserContent() const
	{
#if RSG_DURANGO
		return m_UserContentCheck.HasPermission();
#elif RSG_PROSPERO
		// if either player is blocked by the other or does not have content
		// permissions then block content created by this user
		return !IsRemoteOnLocalBlocklist() &&
			!IsLocalOnRemoteBlocklist() &&
			!IsContentBlockedByLocal() &&
			!IsContentBlockedByRemote();
#else
		return true; 
#endif
	}

	//PURPOSE
	// Returns whether we can player multiplayer with this player
	bool CanPlayMultiplayer() const
	{
#if RSG_DURANGO
		return !m_bIsBlocked;
#else
		return true;
#endif
	}

	//PURPOSE
	// Returns whether this player has blocked the local player
	bool CanPlayMultiplayerWithMe() const
	{
		return true;
	}

	unsigned m_nVoiceFlags;
	bool m_bVoiceFlagsDirty;
	bool m_bIsLocal;
	rlGamerHandle m_hGamer;
	int m_nRefCount;
	bool m_bTrackedForAutomute;
	bool m_bOverrideChatRestrictions;

#if RSG_DURANGO
	bool m_bIsMuted; 
	bool m_bIsBlocked;

	struct PermissionCheck
	{
		PermissionCheck()
			: m_Status(nullptr)
		{
			Clear();
		}

		~PermissionCheck()
		{
			Clear();
		}

		void Clear();

		void Reset(IPrivacySettings::ePermissions nPermissionID, const rlGamerHandle& hGamer)
		{
			m_nPermissionID = nPermissionID;
			m_hGamer = hGamer;
			Clear();
		}

		void SetResult(bool bResult)
		{
			m_bHasResult = true;
			m_bIsAllowed = bResult;
			m_Result.m_isAllowed = bResult; 
		}

		void FlagForCheck();
		void CheckPermission();
		void Update(); 

		bool HasResult() const { return m_bHasResult; }
		bool Pending() const { return (m_Status && m_Status->Pending()) || m_bRefreshPending; }
		bool IsStatusPending() const { return (m_Status && m_Status->Pending()); }
		bool HasPermission() const { return m_bHasResult && m_Result.m_isAllowed; }

		IPrivacySettings::ePermissions m_nPermissionID;
		netStatus* m_Status; 
		bool m_bHasResult;
		bool m_bIsAllowed;
		bool m_bResultPending;
		bool m_bRefreshPending;
		rlGamerHandle m_hGamer;
		PrivacySettingsResult m_Result;
	};

	PermissionCheck m_VoiceCommunicationCheck;
	PermissionCheck m_TextCommunicationCheck;
	PermissionCheck m_UserContentCheck;
	bool m_bIsFriend; 
#endif
};

static const unsigned MAX_TEXT_MESSAGE_LENGTH = 256;

class CNetworkVoice
{
public:

	CNetworkVoice();
	~CNetworkVoice();

	bool InitCore(netConnectionManager* pCxnMgr);
	void ShutdownCore();

	bool Init();
	void Shutdown();
	void Update();

	void InitTunables();

	void InitWorkerThread();
	void ShutdownWorkerThread(); 

	void OnMainGamerAssigned();
	void OnSignedOnline();
	void OnSignOut();
	void OnFriendListChanged();
#if RSG_DURANGO
	void OnAvoidListRetrieved();
#endif

	bool SendTextMessage(const char* szTextMessage, const rlGamerHandle& hGamer);
	void OnTextMessageReceivedViaPresence(const char* szTextMessage, const rlGamerHandle& hFromGamer);
	
	bool IsInitialized() const;
	bool IsEnabled() const;
	bool IsActive() const; 

	void BuildMuteLists(const rlGamerHandle& hGamer = rlGamerHandle()); 
	void BuildMuteListForGamer(const int nGamerIndex);

	void PlayerHasJoined(const rlGamerInfo& gamerInfo, const EndpointId endpointId);
	void PlayerHasLeft(const rlGamerInfo& gamerInfo);
	void PlayerHasJoinedBubble(const rlGamerInfo& gamerInfo);
	void PlayerHasLeftBubble(const rlGamerInfo& gamerInfo);

#if RSG_PC
	bool IsTalkEnabled() const;
	bool IsPushToTalkActive() { return (m_VoiceChat.GetCaptureMode() == VoiceChat::CAPTURE_MODE_PUSH_TO_TALK) && m_VoiceChat.GetPushToTalk(); }
	void SetVoiceTestMode(bool bEnabled) { m_VoiceChat.SetVoiceTestMode(bEnabled); }
	void AdjustSettings(const FrontendVoiceSettings settings);
	void SetMuteChatBecauseLostFocus(bool bShouldMute);
#else
	bool IsPushToTalkActive() { return false; }
#endif // RSG_PC

	// find gamer settings by handle
	const VoiceGamerSettings* FindGamer(const rlGamerHandle& hGamer) const;
	VoiceGamerSettings* FindGamer(const rlGamerHandle& hGamer);

	// overrides all chat restrictions with all players
	void SetOverrideAllRestrictions(const bool bOverride);
	bool GetOverrideAllRestrictions() const { return m_bOverrideAllRestrictions; }

	// voice chat can be toggled on / off
	void SetCommunicationActive(const bool bIsCommunicationActive);

	// functions to check if a player is talking
	bool IsLocalTalking(const int nLocalIndex) const;
	bool IsGamerTalking(const rlGamerId& gamerId) const;
	bool IsAnyGamerTalking() const;
	bool IsAnyRemoteGamerTalking() const;

	// functions to check if a player is about to talk (true during delay between receiving
	// a voice packet and when the audio is played)
	bool IsAnyGamerAboutToTalk() const;
	bool IsAnyRemoteGamerAboutToTalk() const;

	// access to max voice recipients. 0 implies no limit. 
	void SetMaxVoiceRecipients(unsigned nMax) { m_nMaxVoiceRecipients = nMax; }
	unsigned GetMaxVoiceRecipients() { return m_nMaxVoiceRecipients; }

	// access to voice bandwidth limit
	void SetVoiceBandwidthLimit(unsigned nLimit);
	unsigned GetVoiceBandwidthLimit();

	// check if a player has a headset attached
	bool LocalHasHeadset(const int nLocalIndex) const;
	bool GamerHasHeadset(const rlGamerHandle& hGamer) const;

	// mutes/unmutes the given player
	bool MuteGamer(const rlGamerHandle& hGamer, const bool bIsMuted);
	bool ToggleMute(const rlGamerHandle& hGamer);

	// checks for mute / block status (remote block / mute must not be presented to player)
	bool HasGamerRecord(const rlGamerHandle& hGamer) const;
	bool CanCommunicateVoiceWithGamer(const rlGamerHandle& hGamer) const;
	bool CanCommunicateTextWithGamer(const rlGamerHandle& hGamer) const;
	bool IsGamerMutedByMe(const rlGamerHandle& hGamer) const;
	bool AmIMutedByGamer(const rlGamerHandle& hGamer) const;
	bool IsGamerBlockedByMe(const rlGamerHandle& hGamer) const;
	bool AmIBlockedByGamer(const rlGamerHandle& hGamer) const;
	bool CanViewGamerUserContent(const rlGamerHandle& hGamer) const;
	bool HasViewGamerUserContentResult(const rlGamerHandle& hGamer) const;
	bool CanPlayMultiplayerWithGamer(const rlGamerHandle& hGamer) const;
	bool CanGamerPlayMultiplayerWithMe(const rlGamerHandle& hGamer) const;
	bool CanSendInvite(const rlGamerHandle& hGamer) const;
	bool CanReceiveInvite(const rlGamerHandle& hGamer) const;

#if RSG_DURANGO
	void OnConstrained();
	void OnUnconstrained();
	void OnChatEvent(const rlXblChatEvent* pEvent);
	bool IsInMuteList(const rlGamerHandle& hGamer);
#endif

	// sets voice chat focus to the gamer with the given gamer id.
	// pass NULL to clear the current focus.
	void SetTalkerFocus(const rlGamerHandle* gamerHandle);

	// sets the distance threshold within which we can hear talkers
	// who are not on our team.
	// pass zero for infinity (no threshold).
	// pass a negative number to disable voice chat.
	void SetTalkerProximity(const float fProximity);
	void SetTalkerProximityHeight(const float fProximity);
	float GetTalkerProximity() const;

	// sets the distance threshold within which we can hear loudhailers
	// pass zero for infinity (no threshold).
	void SetLoudhailerProximity(const float fProximity); 
	void SetLoudhailerProximityHeight(const float fProximity);
	float GetLoudhailerProximity() const;

	void SetRemainInGameChat(bool bRemainInGameChat);
	bool IsRemainingInGameChat() const { return m_bRemainInGameChat; }

	// override transition chat
	void SetOverrideTransitionChatScript(const bool bOverride);
	bool IsOverrideTransitionChatScript() { return m_bOverrideTransitionChatScript; }
	bool IsOverrideTransitionChat() { return m_bOverrideTransitionChat; }
	void SetOverrideTransitionRestrictions(const int nPlayerID, const bool bOverride);
	bool GetOverrideTransitionRestrictions(const int nPlayerID) const;

	// apply team only chat
	void SetTeamOnlyChat(const bool bTeamOnly);
	void SetTeamChatIncludeUnassignedTeams(const bool bIncludeUnassignedTeams);
	void SetProximityAffectsReceive(const bool bEnable);
	void SetProximityAffectsSend(const bool bEnable);
	void SetProximityAffectsTeam(const bool bEnable);
	void SetNoSpectatorChat(const bool bEnable);
	void SetIgnoreSpectatorLimitsForSameTeam(const bool bEnable);

	// team restriction override
	void SetOverrideTeam(const int nTeam, const bool bOverride);
	bool GetOverrideTeam(const int nTeam) const;

	// override standard behaviours
	void ApplyPlayerRestrictions(PlayerFlags& nFlags, const int nPlayerID, const bool bOverride, const char* szFunctionName);
	void SetOverrideChatRestrictions(const int nPlayerID, const bool bOverride);
	bool GetOverrideChatRestrictions(const int nPlayerID) const;
	void SetOverrideChatRestrictionsSend(const int nPlayerID, const bool bOverride);
	bool GetOverrideChatRestrictionsSend(const int nPlayerID) const;
	void SetOverrideChatRestrictionsReceive(const int nPlayerID, const bool bOverride);
	bool GetOverrideChatRestrictionsReceive(const int nPlayerID) const;
	void SetOverrideSpectatorMode(const bool bOverride);
	void SetOverrideTutorialSession(const bool bOverride);
	bool IsOverrideTutorialSession() const { return m_OverrideTutorialSession; }
	void SetOverrideTutorialRestrictions(const int nPlayerID, const bool bOverride);
	bool GetOverrideTutorialRestrictions(const int nPlayerID) const;
	void SetOverrideChatRestrictionsSendAll(const bool bOverride);
	void SetOverrideChatRestrictionsReceiveAll(const bool bOverride);

	// these retrieve and apply our receive bitfields for remote players
	void ApplyReceiveBitfield(const int nFromPlayerID, const PlayerFlags nPlayerBitField);
	void BuildReceiveBitfield(PlayerFlags &nPlayerBitField) const;
	void ApplySendBitfield(const int nFromPlayerID, const PlayerFlags nPlayerBitField);
	void BuildSendBitfield(PlayerFlags &nPlayerBitField) const;

	// returns the loudness of a local player - [0,1] metric
	float GetPlayerLoudness(const int localPlayerIndex);

	// allows voice limiting based on bandwidth restrictions to be enabled/disabled
	void EnableBandwidthRestrictions(PhysicalPlayerIndex playerIndex);
	void DisableBandwidthRestrictions(PhysicalPlayerIndex playerIndex);

	// script behaviour
	void SetScriptAllowUniDirectionalOverrideForNegativeProximity(const bool bAllowUniDirectionalOverrideForNegativeProximity);


#if __BANK
	void InitWidgets();
	void Bank_Update();

	void Bank_OverrideAllRestrictions();
	void Bank_SetTeamOnlyChat();
	void Bank_ApplyValues();
	void Bank_BuildMuteLists(); 

	void Bank_RefreshPlayerLists();
	const CNetGamePlayer* Bank_GetChosenPlayer();
	void Bank_CheckCommunication(); 
	void Bank_SetFocusPlayer();
	void Bank_ClearFocusPlayer();
	void Bank_MutePlayer();
	void Bank_UnmutePlayer();

	void OnMessageReceived(const ReceivedMessageData &messageData);
	void Bank_HandleChatReply(const ReceivedMessageData &messageData);
	void Bank_HandleChatRequest(const ReceivedMessageData &messageData);
	void Bank_HandleChatEnd(const ReceivedMessageData &messageData);
#endif // __BANK

	rlGamerHandle	GetFocusTalker() { return m_FocusTalker; }
	bool			IsTeamOnlyChat() { return m_TeamOnlyChat; }
private:

	// handles incoming data
	void OnConnectionEvent(netConnectionManager* pCxnMgr, const netEvent* pEvent);
	void HandleVoiceStatus(void* pData, unsigned nDataSize);
	void HandleLocalPlayerMutedByRemotePlayer(const netPlayer* player);
	void OnVoiceChatEventReceived(const VoiceChat::TalkerEvent* pEvent);
	void OnTextMessageReceived(const char* szTextMessage, const rlGamerHandle& hFromGamer);
	void OnTextMessageProcessed(const rlGamerHandle& hFromGamer, const char* szTextMessage);

	// updates the collection of talkers who we can hear and to whom we can speak.
	bool CanAddTalker();
	void UpdateTalkers();

	// updates the voice flags for the local user and informs remote users
	void UpdateVoiceStatus();

	// update the bandwidth restrictions for the players in the game
	void UpdateBandwidthRestrictions();

	// checks if text filtering is enabled
	bool IsTextFilteringEnabled();

#if __BANK
	void DisplayBandwidthRestrictions();
#endif // __BANK

	// current voice group
	eVoiceGroup m_nVoiceGroup;

#if !__NO_OUTPUT
	int m_nVoiceChannel;
	u8 m_nTutorialIndex;
	int m_nTeam;
	bool m_bSpectating;

	enum eTalkerReason
	{
		REASON_INVALID = -1, 
		// reasons to add
		ADD_ALL_TESTS_PASSED,
		ADD_IS_FOCUS_TALKER,
		ADD_OVERRIDE_RESTRICTIONS,
		ADD_LOCAL_LOUDHAILER,
		ADD_REMOTE_LOUDHAILER,
		// reasons to remove
		REMOVE_UNKNOWN,
		REMOVE_NOT_REMOVED,
		REMOVE_NOT_CONNECTED,
		REMOVE_NO_GAMER_ENTRY,
		REMOVE_BLOCKED,
		REMOVE_AUTO_MUTED,
		REMOVE_HARDWARE,
		REMOVE_COMMUNICATIONS_INACTIVE,
		REMOVE_NOT_FOCUS_TALKER,
		REMOVE_BANDWIDTH,
		REMOVE_NO_PLAYER_PED,
		REMOVE_IN_TRANSITION,
		REMOVE_DIFFERENT_TUTORIAL,
		REMOVE_DIFFERENT_VOICE_CHANNEL,
		REMOVE_SPECTATOR_CHAT_DISABLED,
		REMOVE_NEGATIVE_PROXIMITY,
		REMOVE_DIFFERENT_TEAMS,
		REMOVE_OUTSIDE_PROXIMITY,
		// reasons to change flags
		CHANGE_FLAGS_NOT_CHANGED,
		CHANGE_FLAGS_LOCAL_SPECTATING,
		CHANGE_FLAGS_REMOTE_SPECTATING,
		CHANGE_FLAGS_LOCAL_LOUDHAILER,
		CHANGE_FLAGS_REMOTE_LOUDHAILER,
		CHANGE_FLAGS_OVERRIDE_SEND,
		CHANGE_FLAGS_OVERRIDE_RECEIVE,
		CHANGE_FLAGS_OVERRIDE_BOTH,
		//
		REASON_NUM
	};

	eTalkerReason m_nAddReason[MAX_NUM_PHYSICAL_PLAYERS];
	eTalkerReason m_nRemoveReason[MAX_NUM_PHYSICAL_PLAYERS];
	eTalkerReason m_nChangeFlagsReason[MAX_NUM_PHYSICAL_PLAYERS];
	int m_nRemoteTeam[MAX_NUM_PHYSICAL_PLAYERS];
	bool m_bRemoteSpectating[MAX_NUM_PHYSICAL_PLAYERS];

	void SetAddTalkerReason(bool bCanChat, eTalkerReason& nMyReason, eTalkerReason nReason);
	void SetRemoveTalkerReason(bool bCanChat, eTalkerReason& nMyReason, int& nMyLocalSetting, int& nMyRemoteSetting, eTalkerReason nReason, int nLocalSetting, int nRemoteSetting);
	void SetChangeFlagsReason(bool bCanChat, eTalkerReason& nMyReason, eTalkerReason nReason);
#endif

	VoiceChat::Delegate m_VoiceChatDlgt;

	VoiceChat m_VoiceChat;
	audVoiceChatAudioEntity m_AudioEntity;
	netConnectionManager* m_CxnMgr; 
	netConnectionManager::Delegate m_CxnMgrDlgtGame;
	netConnectionManager::Delegate m_CxnMgrDlgtActivity;
	netConnectionManager::Delegate m_CxnMgrDlgtVoice;

	// track last talkers
	atArray<rlGamerId> m_LastTalkers;

	bool m_IsInitialized;
	bool m_HasAttemptedInitialize;
	bool m_bIsCommunicationActive;
	bool m_bWorkerThreadInitialised;

	unsigned m_TextAllowedSentFromGroups;

	bool m_UsePresenceForTextMessages;
	bool m_DisableReceivingTextMessages;

	bool m_IsTextFilterEnabled;
	unsigned m_FilterHoldingMaxRequests;
	unsigned m_FilterHoldingPenIntervalMs;
	unsigned m_FilterSpamMaxRequests;
	unsigned m_FilterSpamIntervalMs;

#if RSG_NP
	// Tunable for controlling whether or not voice chat should be muted when the user is in the 
	// PlayStation Home menu.
	bool m_bNpMuteWhenUIShowing;
#endif

#if RSG_PC
	bool m_bEnabled;
	bool m_bTalkEnabled;
#endif // RSG_PC

	int m_nFullCheckTalkerIndex;

	// gamer pool
	static const unsigned MAX_DELETE_COOLDOWN = 30000;
	atFixedArray<VoiceGamerSettings, MAX_GAMERS> m_Gamers;

	// local gamer tracking
	bool m_bHasLocalGamer;
	VoiceGamerSettings m_LocalGamer;

	// pending gamers
	static const unsigned MAX_PENDING_TIME = 30000;
	struct PendingVoiceGamer
	{
		PendingVoiceGamer() { Clear(); }
		~PendingVoiceGamer() { Clear(); }
		void Clear() 
		{ 
			m_Settings.Clear();
			m_bPendingLeave = false;
			m_nTimestamp = 0;
		}
		VoiceGamerSettings m_Settings;
		unsigned m_nTimestamp; 
		bool m_bPendingLeave;
	};
	atFixedArray<PendingVoiceGamer, MAX_GAMERS> m_PendingGamers;

	// pending gamer
	bool AddPendingGamer(const PendingVoiceGamer& tPendingVoiceGamer OUTPUT_ONLY(, const char* szSource));

	// maximum number of remote players we can send to
	unsigned m_nMaxVoiceRecipients;

	// policies for the voice channel in connection manager
	netChannelPolicies m_VoicePolicies; 
	netChannelPolicies m_TextPolicies; 
	unsigned m_VoiceBandwidthLimit; 

	// focus talker - we only communicate with this talker, if set
	rlGamerHandle m_FocusTalker;

	// toggle to override restrictions between all players
	bool m_bOverrideAllRestrictions;

	// allow players in game / transition to stay in game chat
	bool m_bRemainInGameChat;

	// toggle to override transition blocking
	bool m_bOverrideTransitionChatScript;
	bool m_bOverrideTransitionChat;
	PlayerFlags m_nOverrideTransitionRestrictions; 

	// toggle to only communicate with players in the same team
	bool m_TeamOnlyChat;
	TeamFlags m_OverrideTeamRestrictions;
	bool m_TeamChatIncludeUnassignedTeams;

	// override bitfields to chat with or receive chat from remote players
	PlayerFlags m_OverrideChatRestrictions;
	PlayerFlags m_OverrideChatRestrictionsSendLocal;
	PlayerFlags m_OverrideChatRestrictionsReceiveLocal;
	PlayerFlags m_OverrideChatRestrictionsSendRemote;
	PlayerFlags m_OverrideChatRestrictionsReceiveRemote;
	bool m_bOverrideAllSendRestrictions;
	bool m_bOverrideAllReceiveRestrictions;

	// each physical player gets a text filter
	NetworkTextFilter m_TextFilter[MAX_NUM_PHYSICAL_PLAYERS];

	// override spectator mode so that players in the same team can still talk even if only one of them is in spectator mode
	bool m_OverrideSpectatorMode;
	bool m_NoSpectatorChat;
	bool m_bIgnoreSpectatorLimitsSameTeam;
	bool m_OverrideTutorialSession;
	PlayerFlags m_OverrideTutorialRestrictions; 

	// allow proximity to only affect one direction
	bool m_ProximityAffectsSend;
	bool m_ProximityAffectsReceive;
	bool m_ProximityAffectsTeam;

	// we can only hear talkers within this proximity. Zero means infinity.
	float m_TalkerProximityLo;
	float m_TalkerProximityHi;		
	float m_TalkerProximityLoSq;
	float m_TalkerProximityHiSq;		
	float m_TalkerProximityHeightLo;	
	float m_TalkerProximityHeightHi;	
	float m_TalkerProximityHeightLoSq;
	float m_TalkerProximityHeightHiSq;
	bool m_bScriptAllowUniDirectionalOverrideForNegativeProximity;
	bool m_bAllowUniDirectionalOverrideForNegativeProximity;

	// we can hear loudhailers within this proximity. 
	float m_LoudhailerProximityLo;
	float m_LoudhailerProximityHi;
	float m_LoudhailerProximityHeightLo;		
	float m_LoudhailerProximityHeightHi;		
	float m_LoudhailerProximityLoSq;		
	float m_LoudhailerProximityHiSq;	
	float m_LoudhailerProximityHeightLoSq;		
	float m_LoudhailerProximityHeightHiSq;		

	// bandwidth restrictions
	PlayerFlags m_DisableBandwidthRestrictions;
	u32         m_DisableBandwidthVoiceTime[MAX_NUM_PHYSICAL_PLAYERS];
	u32         m_nMaxBandwidthVoiceRecipients;
	u32         m_LastTargetUpstreamBandwidth;

#if RSG_DURANGO
	PlayerList m_MuteList;
	bool m_bRefreshingMuteList;
	bool m_bHasMuteList;
	netStatus m_MuteListStatus; 
	bool m_IssueCommunicationCheck; 
#endif

	unsigned m_PendingTalkersForAutoMute;
	unsigned m_PendingMutesForAutoMute;

	// auto mute tracking
	void CheckAndAddPendingTalkersForAutoMute();
	void CheckAndAddPendingMutesForAutoMute();

#if RSG_DURANGO
	bool m_bIsConstrained;
#endif

#if __BANK
	NetworkPlayerEventDelegate m_Dlgt;

	int m_BankTalkerCount;
	bool m_BankOverrideAllRestrictions;
	bool m_BankTeamOnlyChat;
	bool m_BankDisplayBandwidthRestrictions;
	unsigned m_BankVpdThreshold;
	unsigned m_BankSendInterval;
	unsigned m_BankNumPlayers;
	const char* m_BankPlayers[MAX_NUM_PHYSICAL_PLAYERS];
	int m_BankPlayerIndex;
	bkCombo* m_BankPlayerCombo;
	int m_BankLocalPlayerMuteCount;
	int m_BankLocalPlayerTalkersMet;
#endif  //__BANK
};

#endif  // NETWORKVOICE_H
