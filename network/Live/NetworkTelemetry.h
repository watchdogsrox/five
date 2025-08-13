//
// NetworkTelemetry.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _NETWORKTELEMETRY_H_
#define _NETWORKTELEMETRY_H_

#include "vector/vector3.h"
#include "rline/rltelemetry.h"
#include "network/networkinterface.h"
#include "network/live/NetworkMetrics.h"

#include "system/EndUserBenchmark.h"

#if RSG_OUTPUT
#include "atl/hashstring.h"
#endif
#include "snet/session.h"
namespace rage
{
	class RsonWriter;
}

class CPed;
class CPlayerInfo;
struct sWindfallInfo;

// Needs to match CONST_INTs in 'x:\gta5\script\dev_gen9_sga\multiplayer\globals\mp_globals_ambience_definitions.sch' (CHAT_{OPTION})
enum class ChatOption {
	Everyone,
	Crew,
	Friends,
	CrewAndFriends,
	Noone,
	Organisation,
	Max,
};

// PURPOSE
//   This class is responsible for sending metrics to a remote host
//   for collection and processing.
//   Telemetry is not robust because data is not saved locally and 
//   can be lost if a player powers down before the data is sent.
class CNetworkTelemetry
{
public:

	static bool AppendMetric(const rlMetric& m, const bool deferredCashTransaction=false);
	static bool FlushTelemetry(bool flushnow = false);

	//Called when the cash immediate flush happens.
	static void OnDeferredFlush( void );
	static void IncrementCloudCashTransactionCount( void );

	//Called when the telemetry ends flushing.
	static void OnTelemetryFlushEnd( const netStatus& status );

	static void OnTelemetyEvent( const rlTelemetryEvent& evt );

	//Telemetry accessors methods that can be called to append metrics
	static void EarnedAchievement(const unsigned id);

	//Setup multiplayer game join type sent in metrics
	static void SetOnlineJoinType(const u8 type);
	static u8   GetOnlineJoinType();

	static u64 GetCurMpSessionId();
	static u8  GetCurMpGameMode();
	static u8  GetCurMpNumPubSlots();
	static u8  GetCurMpNumPrivSlots();
	static u32 GetMatchStartTime();

	static u32 GetMissionId() { return sm_MissionStarted.GetHash(); }
	static u32 GetMatchStartedId() { return sm_MatchStartedId.GetHash(); }
	static u32 GetRootContentId() { return sm_RootContentId; }
	
	static void EnteredSoloSession(const u64 sessionId, const u64 sessionToken, const int gameMode, const eSoloSessionReason soloSessionReason, const eSessionVisibility sessionVisibility, const eSoloSessionSource soloSessionSource, const int userParam1, const int userParam2, const int userParam3);
	static void StallDetectedMultiPlayer(const unsigned stallTimeMs, const unsigned networkTimeMs, const bool bIsTransitioning, const bool bIsLaunching, const bool bIsHost, const unsigned nTimeoutsPending, const unsigned sessionSize, const u64 sessionToken, const u64 sessionId);
	static void StallDetectedSinglePlayer(const unsigned stallTimeMs, const unsigned networkTimeMs);
	static void NetworkBail(const sBailParameters bailParams,
							const unsigned nSessionState,
							const u64 sessionId,
							const bool bIsHost,
							const bool bJoiningViaMatchmaking,
							const bool bJoiningViaInvite,
							const unsigned nTime);

	static void NetworkKicked(const u64 nSessionToken,
							  const u64 nSessionId,
							  const u32 nGameMode,
							  const u32 nSource,
							  const u32 nPlayers,
							  const u32 nSessionState, 
							  const u32 nTimeInSession,
							  const u32 nTimeSinceLastMigration,
							  const unsigned nNumComplaints,
							  const rlGamerHandle& hGamer);

	static void SessionHosted(const u64 sessionId, const u64 sessionToken, const u64 timeCreated, const SessionPurpose sessionPurpose, const unsigned numPubSlots, const unsigned numPrivSlots, const unsigned matchmakingKey, const u64 nUniqueMatchmakingID);
	static void SessionJoined(const u64 sessionId, const u64 sessionToken, const u64 timeCreated, const SessionPurpose sessionPurpose, const unsigned numPubSlots, const unsigned numPrivSlots, const unsigned matchmakingKey, const u64 nUniqueMatchmakingID);
	static void SessionLeft(const u64 sessionId, const bool isTransitioning);
	static void SessionBecameHost(const u64 sessionId);
	static bool IsActivityInProgress();

	static void InviteResult(
		const u64 sessionToken,
		const char* uniqueMatchId,
		const int gameMode,
		const unsigned inviteFlags,
		const unsigned inviteId,
		const unsigned inviteAction,
		const int inviteFrom,
		const int inviteType,
		const rlGamerHandle& inviterHandle,
		const bool isFriend);

	//PURPOSE
	//  Must be called only when a match starts.
	static void SessionMatchStarted(const char* userId, const char* uid, const u32 rootContentId, const u64 matchHistoryId, const MetricJOB_STARTED::sMatchStartInfo& info);

	//PURPOSE
	//  Must be called only when a match ends.
	static void SessionMatchEnd();
	static void SessionMatchEnded(const unsigned matchType, const char* userId, const char* uid, const bool isTeamDeathmatch, const int raceType, const bool leftInProgress);

	static void PlayerDied(const CPed* victim, const CPed* killer, const u32 weaponHash, const int weaponDamageComponent, const bool withMeleeWeapon);
	static void PlayerArrested();
	static void PlayerSpawn(int location, int reason);
	static void PlayerWantedLevel();
	static void PlayerInjured();
	static void SignedOnline();
	static void SignedOut();

	static void PlayerHasJoined(const netPlayer& player);

	static void MakeRosBet(const CPed* gamer, const CPed* betOngamer, const u32 amount, const u32 activity, const float commission);

	static void RaceCheckpoint(const u32 vehicleId
								,const u32 checkpointId
								,const u32 racePos
								,const u32 raceTime
								,const u32 checkpointTime);

	static void PostMatchBlob(const u32 xp
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
								,const int survivedWave);

	static void NewGame();
	static void LoadGame();

	static void StartGame();
	static void ExitGame();

	static void AudTrackTagged(const unsigned id);
	static void RetuneToStation(const unsigned id);

	static void CheatApplied(const char *pCheatString);
	static void PlayerVehicleDistance(const u32 keyHash, const float distance, const u32 timeinvehicle, const char* areaName, const char* districtName, const char* streetName, const bool owned);

	static void AppendMetricRockstarDEV(const rlGamerHandle& gamerhandle);
	static void AppendMetricRockstarQA(const rlGamerHandle& gamerhandle);
	static void AppendMetricReporter( const char* UserId, const char* reportedId, const u32 type, const u32 numberoftimes );

	static void AppendMetricReportInvalidModelCreated(const rlGamerHandle& reportedHandle, const u32 modelHash, const u32 reportReason);

	static void AcquiredWeapon(const u32 weaponHash);
	static void ChangeWeaponMod(const u32 weaponHash, const int modIdFrom, const int modIdTo);
	static void ChangePlayerProp(const u32 modelHash, const int  position, const int  newPropIndex, const int  newTextIndex);
	static void ChangedPlayerCloth(const u32 modelHash, const int componentID, const int drawableID, const int textureID, const int paletteID);

	static void EmergencySvcsCalled(const s32 mi, const Vector3& destination);

	// - In need to be placed and tested
	static void AcquiredHiddenPackage(const int id);

	static void WebsiteVisited(const int id, const int timespent);
	static void FriendsActivityDone(int character, int outcome);

	static void MissionStarted(const char *pMissionName, const u32 variant, const u32 checkpoint, const bool replaying);
	static void MissionOver(const char *pMissionName, const u32 variant, const u32 checkpoint, const bool failed, const bool canceled, const bool skipped);
	static void MissionCheckpoint(const char* missionName, const u32 variant, const u32 previousCheckpoint, const u32 checkpoint);
	static void RandomMissionDone(const char* missionName, const int outcome, const int timespent, const int attempts);

	static void MultiplayerAmbientMissionCrateDrop(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const u32 weaponHash, const u32 otherItemsHash, const u32 enemiesKilled, const u32 (&_SpecialItemHashs)[MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS], bool _CollectedBodyArmour);
	static void MultiplayerAmbientMissionCrateCreated(float X, float Y, float Z);
	static void MultiplayerAmbientMissionHoldUp(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const u32 shopkeepersKilled);
	static void MultiplayerAmbientMissionImportExport(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const int vehicleId);
	static void MultiplayerAmbientMissionSecurityVan(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned);
	static void MultiplayerAmbientMissionRaceToPoint(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const MetricAMBIENT_MISSION_RACE_TO_POINT::sRaceToPointInfo& rtopInfo);
	static void MultiplayerAmbientMissionProstitute(const u32 ambientMissionId, const u32 xpEarned, const u32 cashEarned, const u32 cashSpent, const u32 numberOfServices, const bool playerWasTheProstitute);

	static void CutSceneSkipped(const u32 nameHash, const u32 duration);
	static void CutSceneEnded(const u32 nameHash, const u32 duration);

	static void WeatherChanged(const u32 prevIndex, const u32 newIndex);

	static void EnterIngameStore();
	static void ExitIngameStore( bool didCheckout, u32 msSinceInputEnabled );

	static void SetupTelemetryStream();

	//Metric that gets sent when a piece of clothing is bought.
	static void ShopItem(const u32 itemHash, const u32 amountSpent, const u32 shopName, const u32 extraItemHash, const u32 colorHash);

	static void  AppendMetricTvShow(const u32 playListId, const u32 tvShowId, const u32 channelId, const u32 timeWatched);

	static void AppendMetricMem(const u32 namehash, const u32 newvalue);

	static void AppendDebuggerDetect(const u32 newvalue);

	static void AppendGarageTamper(const u32 newvalue);

	// NB: Was AppendCodeCRCFail, but that shows up in the .elf file
	static void AppendInfoMetric(const u32 version, const u32 address, const u32 crc);

	static void AppendTamperMetric(int param);
	static void AppendSecurityMetric(MetricLAST_VEH& metric);

	static void GetIsClosedAndPrivateForLastFreemode(bool& closed, bool& privateSlot);

	// Chat telemetry
	static void RecordGameChatMessage(const rlGamerId& gamerId, const char* text, bool bTeamOnly);
	static void RecordPhoneTextMessage(const rlGamerHandle& receiverGamerHandle, const char* text);
	static void RecordPhoneEmailMessage(const rlGamerHandle& receiverGamerHandle, int textLength);
	static void OnPhoneTextMessageReceived(const rlGamerHandle& senderGamerGandle, const char* text);
	static void OnNewEmailMessagesReceived(const rlGamerHandle& receiverGamerGandle);

	OUTPUT_ONLY( static u32  GetCurrentCheckpoint() {return sm_MissionCheckpoint;} )

public:
    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
    static void Update();
    static void SetUpFilename(const char *fileNameBase
							  ,char *fileNameResult
							  ,const unsigned lenofResult);

    //Set/get the single player session id.  The session id is the locally
    //unique id of the single player session.  i.e. each single player
    //session is assigned a unique id.
    static void SetSpSessionId(const u64 id);
    static u64 GetSpSessionId();

	static void DiscoverSessionType();
	static void FlushCommsTelemetry();

	//Adds game specific header values.
	static bool SetGamerHeader(RsonWriter* rw);

	// Get the current match history ID
	static u64 GetMatchHistoryId();
	static void SetPreviousMatchHistoryId(OUTPUT_ONLY(const char* caller));
	static void ClearAllMatchHistoryId();

	// return true if the user has used the voice chat during the last VOICE_CHAT_TRACK_INTERVAL ms.
	static bool HasUsedVoiceChatSinceLastTime() { return sm_HasUsedVoiceChatSinceLastTime;	}
	static void ResetHasUsedVoiceChat();

	static void OnPeerPlayerConnected(bool isTransition, const u64 sessionId, const rage::snEventAddedGamer* pEvent, rage::netNatType natType, const rage::netAddress& addr);
	static void AppendCreatorUsage(u32 timeSpent, u32 numberOfTries, u32 numberOfEarlyQuits, bool isCreating, u32 missionId);

	static MetricPURCHASE_FLOW::PurchaseMetricInfos& GetPurchaseMetricInformation()		{	return sm_purchaseMetricInformation;	}
	static void GetPurchaseFlowTelemetry(char *telemetry, const unsigned bufSize);

	static void AppendXPLoss(int xpBefore, int xpAfter);

	static void AppendHeistSaveCheat(int hashValue, int otherValue);
	static void AppendDirectorMode(const MetricDIRECTOR_MODE::VideoClipMetrics& values);
	static void AppendVideoEditorSaved(int clipCount, int* audioHash, int audioHashCount, u64 projectLength, u64 timeSpent);
	static void AppendVideoEditorUploaded(u64 videoLength, int m_scScore);
	static void AppendEventTriggerOveruse(Vector3 pos, u32 rank, u32 triggerCount, u32 triggerDur, u32 totalCount, atFixedArray<MetricEventTriggerOveruse::EventData, MetricEventTriggerOveruse::MAX_EVENTS_REPORTED>& triggeredData);

	static void AppendCashCreated(u64 uid, const rlGamerHandle& player, int amount, int hash);

	static void AppendStarterPackApplied(int item, int amount);

	static void AppendSinglePlayerSaveMigration(bool success, u32 savePosixTime, float completionPercentage, u32 lastCompletedMissionId, unsigned sizeOfSavegameDataBuffer);

#if RSG_OUTPUT
	static void  AddMetricFileMark();
#endif // RSG_OUTPUT

#if RSG_PC
	static void AppendBenchmarkMetrics(const atArray<EndUserBenchmarks::fpsResult>& values);
	static u64 GetBenchmarkGuid();
	static void GenerateBenchmarkGuid();
#endif

	static void SetAllowRelayUsageTelemetry(const bool allowRelayUsageTelemetry);
	static void SetAllowPCHardwareTelemetry(const bool allowPCHardwareTelemetry);

	static void AppendObjectReassignFailed(const u32 negotiationStartTime, const int restartCount, const int ambObjCount, const int scrObjCount);
	static void AppendObjectReassignInfo();

	static int GetCurrentSpawnLocationOption();
	static void SetCurrentSpawnLocationOption(const int newCurrentSpawnLocationOption);

	static bool GetVehicleDrivenInTestDrive();
	static void SetVehicleDrivenInTestDrive(const bool vehicleDrivenInTestDrive);

	static const int GetVehicleDrivenLocation();
	static void SetVehicleDrivenLocation(const int vehicleDrivenLocation);

	static const char* GetCurrentPublicContentId();
	static void SetCurrentPublicContentId(const char* newCurrentPublicContentId);

	static const ChatOption GetCurrentChatOption();
	static void SetCurrentChatOption(const ChatOption newChatOption);

	static bool IsInLobby();

	static void AppendPedAttached(int pedCount, bool clone, s32 account);

#if GEN9_LANDING_PAGE_ENABLED
    static void LandingPageEnter();
    static void LandingPageExit();
    static void LandingPageUpdate();

    static void AppendLandingPageMisc();
    static void SetLandingPageOnlineData(unsigned layoutPublishId, const uiCloudSupport::LayoutMapping* mapping, const CSocialClubFeedTilesRow* row,
                                         const ScFeedPost* (&persistentOnlineTiles)[4], const atHashString(&persistentOnDiskTiles)[4]);
    static void AppendLandingPageOnline(const eLandingPageOnlineDataSource& source, u32 mainOnDiskTileId);
    static void SetLandingPagePersistentTileData(const atHashString* rowArea, const unsigned rowCount);
    static void AppendLandingPagePersistent();
    static void PriorityBigFeedEntered();
    static void PastPriorityFeed();
    static void AppendBigFeedNav(u32 hoverTile, u32 leftBy, u32 clickedTile, u32 durationMs, int skipTimerMs, bool whatsNew);

    static void LandingPageTrackMessage(const uiPageDeckMessageBase& msg, const u32 activePage, const bool handled);
    static void LandingPageTabEntered(u32 currentTab, u32 newTab);
    static void CardFocusGained(int cardBit);
    static void LandingPageTileClicked(u32 clickedTile, u32 leftBy, u32 activePage, bool exitingLp);
    static void AppendLandingPageNav(u32 clickedTile, u32 leftBy, u32 activePage, bool exitingLp);
    static void AppendLandingPageNavFromScript(u32 clickedTile, u32 leftBy, u32 activePage, u32 hoverBits, bool exitingLp);

    static u32 LandingPageMapPos(u32 x, u32 y);

	static void AppendPlaystatsWindfall(const sWindfallInfo& data);

    // Membership and SP upsell
    static void AppendSaInNav(const unsigned product);
    static void SpUpgradeBought(const unsigned product);
    static void GtaMembershipBought(const unsigned product);
    static void AppendSaOutNav();
#endif // GEN9_LANDING_PAGE_ENABLED

private:

#if RSG_PC
	static bool sm_bPostHardwareStats;
	static void PostGamerHardwareStats(bool toTelemetry=true);
#endif

	static void UpdateFramesPerSecondResult();

	static void LogMetricWarning(const char* szFormat, ...);
	static void LogMetricDebug(const char* szFormat, ...);

	// These counters go up for each character deleted so that we can keep track
	//   of the unique character for mp metrics.
	// These can be used by metric processing so that we know if the stats are 
	//   relative to a new character.
	static int  GetMPCharacterCounter();

	//PURPOSE
	// Used for high volume metrics - add them to a list and send them all together later on.
	static bool        AppendHighVolumeMetric(const rlMetric* metric);
	static void        UpdateHighVolumeMetric( );
	static void ReleaseHighVolumeMetricBuffer( );

	static u32 GenerateNewChatID(const u64& netTime, const u64& senderId);

private:

	static void UpdateOnFootMovementTracking();

	static u32          m_flushRequestedTime; //Signal when to start next flush 

    static unsigned     m_LastAudTrackTime;     //Last time an audio track change was checked

	// Track the usage of the voice chat in free roaming and during jobs
	//static const unsigned	VOICE_CHAT_TRACK_INTERVAL = 300000;
	static const unsigned	VOICE_CHAT_TRACK_INTERVAL = 60000;
	static unsigned			sm_LastVoiceChatTrackTime;
	static unsigned			sm_VoiceChatTrackInterval;
	static bool				sm_HasUsedVoiceChatSinceLastTime;
	enum VoiceChatSendState
	{
		VCSS_READY,
		VCSS_NOT_MODIFIED,
	};
	static bool				sm_ShouldSendVoiceChatUsage;
	static bool				sm_PreviouslySentVoiceChatUsage;

    //Track changes to the audio track that is tuned.  Check for
    //changes on a regular interval and generate an event if a change
    //occurred.  Avoids generating too many events if players frequently
    //re-tune.
	static unsigned     sm_AudTrackInterval;
	static unsigned     sm_AudPrevTunedStation;
	static unsigned     sm_AudPlayerTunedStation;
	static unsigned     sm_AudPlayerPrevTunedStation;
	static unsigned     sm_AudMostFavouriteStation;
	static unsigned     sm_AudLeastFavouriteStation;
	static float        sm_AudTrackStartTime;
	static unsigned		sm_AudPrevTunedUsbMix;

	//Used for metric TIMETOINJURY - record timestamp when player spawn and send the 
	//  metric when the player is injured 1st time.
	static u64       m_PlayerSpawnTime;
	static Vector3   m_PlayerSpawnCoords;
	static bool      m_SendMetricInjury;

	//Single Player mission control
	static u32 sm_MissionStartedTime;
	static u32 sm_MissionStartedTimePaused;
	static u32 sm_MissionCheckpointTime;
	static u32 sm_MissionCheckpointTimePaused;
	OUTPUT_ONLY(static u32 sm_MissionCheckpoint; )
	static atHashString  sm_MissionStarted;
	OUTPUT_ONLY( static u32 sm_CompanyWidePlaytestId; )
	static atHashString  sm_MatchStartedId;
	static u32 sm_RootContentId;

	//Multiplayer game join type
	static u8 sm_OnlineJoinType;

	// InGame Purchase information
	static MetricPURCHASE_FLOW::PurchaseMetricInfos sm_purchaseMetricInformation;

	// Communication tracking information
	static MetricComms::CumulativeTextCommsInfo sm_CumulativeTextCommsInfo[MetricComms::NUM_CUMULATIVE_CHAT_TYPES];
	static MetricComms::CumulativeVoiceCommsInfo sm_CumulativeVoiceCommsInfo;

	// Unique identifier used to correlate the benchmark metrics
	static u64       m_benchmarkUid;

	// Map of the reported DWORDs from tamper actions
	// This is a map because the std::set still does an O(n) lookup
	// to determine if the DWORD has been sent along or not. I'm opting 
	// for the map for another bit of storage for the O(1) lookup time.
	static atMap<u32,bool> sm_reportedParams;

	// This provides a header entry for the entirety of a player's 
	// playing experience, regardless of if they float in and out of
	// SP / MP / transitioning. This provides the PIA team a way to 
	// identify all of the metrics a player will deliver from when 
	// they hit the play button until when they stop playing
	// the game
	static u64		sm_playtimeSessionId;

	// tunable that allows us to disable telemetry
	static bool sm_AllowRelayUsageTelemetry;
	static bool sm_AllowPCHardwareTelemetry;
	
	// Which spawn location option is currently set in the menu: specific property, random, last location
	static int ms_currentSpawnLocationOption;

	// Whether the last vehicle driven was being used in a test drive
	static bool sm_LastVehicleDrivenInTestDrive;
	static int sm_VehicleDrivenLocation;

	// public content ID if the player is in a lobby or instanced mode
	static char ms_publicContentId[MetricPlayStat::MAX_STRING_LENGTH];

	// chat type selection set from menu/script
	static ChatOption sm_ChatOption;

	// ID of the current chat session to be sent with Comms telemetry.
	static int sm_ChatIdExpiryTime;
	static u32 sm_CurrentPhoneChatId;
	static u32 sm_TimeOfLastPhoneChat;
	static u32 sm_CurrentEmailChatId;
	static u32 sm_TimeOfLastEmailChat;
};
#define APPEND_METRIC_FLUSH( m, b )\
	CNetworkTelemetry::AppendMetric( m );\
	CNetworkTelemetry::FlushTelemetry( b );

#define APPEND_METRIC( m )\
	CNetworkTelemetry::AppendMetric( m );

	bool WriteSku(RsonWriter* rw);
#endif // _NETWORKTELEMETRY_H_


