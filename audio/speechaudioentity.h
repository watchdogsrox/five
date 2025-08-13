// 
// audio/speechaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_SPEECHAUDIOENTITY_H
#define AUD_SPEECHAUDIOENTITY_H

#include "audio_channel.h"
#include "gtaaudioentity.h"
#include "gameobjects.h"

#include "audioentity.h"

#include "audioengine/curve.h"
#include "audioengine/soundset.h"
#include "audioengine/widgets.h"
#include "audiosoundtypes/speechsound.h"
#include "cranimation/animation.h"
#include "crclip/clipdictionary.h"
#include "control/replay/ReplaySettings.h"
#include "fwScene/stores/clipdictionarystore.h"
#include "Peds/PedType.h"
#include "vector/vector3.h"
#include "scene/RegdRefTypes.h"
#include "script/thread.h"
#include "streaming/streamingallocator.h"
#include "streaming/streamingengine.h"

class CPed;

namespace rage{

class crClip;
}

class CVehicle;

#if __BANK
extern bool g_ForceAllowGestures;

enum{
	DEBUG_SPEECH_VOL_AMBIENT_NORMAL,
	DEBUG_SPEECH_VOL_AMBIENT_FRONTEND,
	DEBUG_SPEECH_VOL_AMBIENT_SHOUTED,
	DEBUG_SPEECH_VOL_AMBIENT_PAIN,
	DEBUG_SPEECH_VOL_AMBIENT_MEGAPHONE,
	DEBUG_SPEECH_VOL_AMBIENT_HELI,
	DEBUG_SPEECH_VOL_SCRIPTED_NORMAL,
	DEBUG_SPEECH_VOL_SCRIPTED_HIGHROLLOFF,
	DEBUG_SPEECH_VOL_SCRIPTED_SHOUTED,
	DEBUG_SPEECH_VOL_SCRIPTED_FRONTEND,
	DEBUG_SPEECH_VOL_SCRIPTED_MEGAPHONE,
	DEBUG_SPEECH_VOL_TOTAL_NUM
};
#endif

enum
{
	AUD_ROMANS_MOOD_NORMAL,
	AUD_ROMANS_MOOD_SAD,
	AUD_ROMANS_MOOD_SHAKEN_UP,
	AUD_ROMANS_MOOD_DRUNK
};

enum
{
	AUD_BRIANS_MOOD_CLEAN,
	AUD_BRIANS_MOOD_DRUG
};

typedef enum
{
	AUD_ANIMAL_MOOD_ANGRY,
	AUD_ANIMAL_MOOD_PLAYFUL,

	AUD_ANIMAL_MOOD_NUM_MOODS
}audAnimalMood;

enum AudTougnessType
{
	AUD_TOUGHNESS_MALE_NORMAL = 0,
	AUD_TOUGHNESS_MALE_COWARDLY,
	AUD_TOUGHNESS_MALE_BUM,
	AUD_TOUGHNESS_MALE_BRAVE,
	AUD_TOUGHNESS_MALE_GANG,
	AUD_TOUGHNESS_FEMALE,
	AUD_TOUGHNESS_FEMALE_BUM,
	AUD_TOUGHNESS_COP,
	NUM_TOUGNESS,
};


typedef enum
{
	AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER,
	AUD_SPEECH_METADATA_CATEGORY_GESTURE_LISTENER,
	AUD_SPEECH_METADATA_CATEGORY_FACIAL_SPEAKER,
	AUD_SPEECH_METADATA_CATEGORY_FACIAL_LISTENER,
	AUD_SPEECH_METADATA_CATEGORY_RESTART_POINT,
	NUM_AUD_SPEECH_METADATA_CATEGORIES

}audSpeechMetadataCategories;

// Keep these in sync with the strings in the .cpp since that's what we use to initialize the curves
enum audSpeechVolume
{
	AUD_SPEECH_NORMAL,
	AUD_SPEECH_SHOUT,
	AUD_SPEECH_FRONTEND,
	AUD_SPEECH_MEGAPHONE,
	AUD_NUM_SPEECH_VOLUMES
};

// Keep this in sync with the strings in the .cpp since that's what we use to initialize the curves
enum audConversationAudibility
{
	AUD_AUDIBILITY_NORMAL = 0,
	AUD_AUDIBILITY_CLEAR,
	AUD_AUDIBILITY_CRITICAL,
	AUD_AUDIBILITY_LEAD_IN,
	AUD_NUM_AUDIBILITIES

};

enum audSpeechType
{
	AUD_SPEECH_TYPE_INVALID,
	AUD_SPEECH_TYPE_AMBIENT,
	AUD_SPEECH_TYPE_SCRIPTED,
	AUD_SPEECH_TYPE_PAIN,
	AUD_SPEECH_TYPE_ANIMAL_PAIN,
	AUD_SPEECH_TYPE_ANIMAL_NEAR,
	AUD_SPEECH_TYPE_ANIMAL_FAR,
	AUD_SPEECH_TYPE_BREATHING,
	AUD_NUM_SPEECH_TYPES
};

enum audContextBlockTarget
{
	AUD_CONTEXT_BLOCK_PLAYER,
	AUD_CONTEXT_BLOCK_NPCS,
	AUD_CONTEXT_BLOCK_BUDDYS,
	AUD_CONTEXT_BLOCK_EVERYONE,

	AUD_CONTEXT_BLOCK_TARGETS_TOTAL
};

#define AUD_DEBUG_CUSTOM_LIPSYNC 0

#define AUD_CONVERSATION_TOPIC_SMOKER		(1)
#define AUD_CONVERSATION_TOPIC_BUSINESS		(2)
#define AUD_CONVERSATION_TOPIC_BUM			(4)
#define AUD_CONVERSATION_TOPIC_CONSTRUCTION	(8)
#define AUD_CONVERSATION_TOPIC_PIMP			(16)
#define AUD_CONVERSATION_TOPIC_HOOKER		(32)
#define AUD_CONVERSATION_TOPIC_MOBILE		(64)
#define AUD_CONVERSATION_TOPIC_TWO_WAY		(128)
#define AUD_CONVERSATION_TOPIC_GANG			(256)
#define AUD_CONVERSATION_TOPIC_GUARD		(512)

#define AUD_MAX_GESTURES_PER_LINE			(4)

#define AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS (2)

#define AUD_MAX_BLOCKED_CONTEXT_LISTS (8)

enum audDamageReason
{
	AUD_DAMAGE_REASON_DEFAULT,
	AUD_DAMAGE_REASON_FALLING,
	AUD_DAMAGE_REASON_SUPER_FALLING,
	AUD_DAMAGE_REASON_SCREAM_PANIC,
	AUD_DAMAGE_REASON_SCREAM_PANIC_SHORT,
	AUD_DAMAGE_REASON_SCREAM_SCARED,
	AUD_DAMAGE_REASON_SCREAM_SHOCKED,
	AUD_DAMAGE_REASON_SCREAM_TERROR,
	AUD_DAMAGE_REASON_ON_FIRE,
	AUD_DAMAGE_REASON_DROWNING,
	AUD_DAMAGE_REASON_SURFACE_DROWNING,	// drowning on the surface of water, after we time out
	AUD_DAMAGE_REASON_INHALE,
	AUD_DAMAGE_REASON_EXHALE,
	AUD_DAMAGE_REASON_POST_FALL_GRUNT, 
	AUD_DAMAGE_REASON_ENTERING_RAGDOLL_DEATH,
	AUD_DAMAGE_REASON_EXPLOSION,
	AUD_DAMAGE_REASON_MELEE,
	AUD_DAMAGE_REASON_SHOVE,
	AUD_DAMAGE_REASON_WHEEZE,
	AUD_DAMAGE_REASON_COUGH,
	AUD_DAMAGE_REASON_TAZER,
	AUD_DAMAGE_REASON_EXHAUSTION,
	AUD_DAMAGE_REASON_CLIMB_LARGE,
	AUD_DAMAGE_REASON_CLIMB_SMALL,
	AUD_DAMAGE_REASON_JUMP,
	AUD_DAMAGE_REASON_COWER,
	AUD_DAMAGE_REASON_WHIMPER,
	AUD_DAMAGE_REASON_DYING_MOAN,
	AUD_DAMAGE_REASON_CYCLING_EXHALE,
	AUD_DAMAGE_REASON_PAIN_RAPIDS,
	AUD_DAMAGE_REASON_SNEEZE,
	AUD_DAMAGE_REASON_MELEE_SMALL_GRUNT,
	AUD_DAMAGE_REASON_MELEE_LARGE_GRUNT,
	AUD_DAMAGE_REASON_POST_FALL_GRUNT_LOW,

	AUD_NUM_DAMAGE_REASONS
};

enum audAmbientSpeechVolume
{
	AUD_AMBIENT_SPEECH_VOLUME_UNSPECIFIED,
	AUD_AMBIENT_SPEECH_VOLUME_NORMAL,
	AUD_AMBIENT_SPEECH_VOLUME_SHOUTED,
	AUD_AMBIENT_SPEECH_VOLUME_FRONTEND,
	AUD_AMBIENT_SPEECH_VOLUME_MEGAPHONE,
	AUD_AMBIENT_SPEECH_VOLUME_HELI
};

typedef enum
{
	AUD_SPEECH_SAY_FAILED,
	AUD_SPEECH_SAY_SUCCEEDED,
	AUD_SPEECH_SAY_DEFERRED
}audSaySuccessValue;

enum audVisemeType{
	AUD_VISEME_TYPE_FACEFX,
	AUD_VISEME_TYPE_IM,
	AUD_NUM_VISEME_TYPES
};

enum audAmbSpeechModifiers{
	AUD_AMB_MOD_MORNING,
	AUD_AMB_MOD_EVENING,
	AUD_AMB_MOD_NIGHT,
	AUD_AMB_MOD_RAINING,

	AUD_AMB_MOD_TOT
};

enum PlayerBreathState
{
	kBreathingIdle,
	kBreathingRunRampUp,
	kBreathingRun,
	kBreathingRunStop,
	kBreathingJogHighBreathing,
	kBreathingJogRampDown,
	kBreathingJogLowBreathing,
	kBreathingJogStop,

	kNumBreathingStates
};

enum AnimalPantState
{
	kPantIdle,
	kPantStarting,
	kPantLow,
	kPantMed,
	kPantHigh,
	kPantLowStopping,
	kPantMedStopping,
	kPantHighStopping,

	kNumPantStates
};

enum PlayerVocalEffectType
{
	BREATH_EXHALE,
	BREATH_EXHALE_CYCLING,
	BREATH_INHALE,
	BREATH_SWIM_PRE_DIVE,

	AUD_NUM_PLAYER_VFX_TYPES
};

enum TennisVocalEffectType
{
	TENNIS_VFX_LITE,
	TENNIS_VFX_MED,
	TENNIS_VFX_STRONG,

	AUD_NUM_TENNIS_VFX_TYPES
};

enum audSpeechRoute
{
	AUD_SPEECH_ROUTE_FE,			// Frontend
	AUD_SPEECH_ROUTE_VERB_ONLY,		// Positional reverb only for use with frontend
	AUD_SPEECH_ROUTE_POS,			// Positioned
	AUD_SPEECH_ROUTE_HS,			// Headset
	AUD_SPEECH_ROUTE_PAD,			// Pad Speaker
	AUD_NUM_SPEECH_ROUTES
};

enum audSpeechPanicType
{
	AUD_SPEECH_PANIC_NONE,
	AUD_SPEECH_PANIC_LOW,
	AUD_SPEECH_PANIC_MEDIUM,
	AUD_SPEECH_PANIC_HIGH,
	AUD_SPEECH_PANIC_JACKED
};

struct audDamageStats
{
	audDamageStats()	:
	Inflictor(NULL),
	RawDamage(0.0f),
	PreDelay(0),
	DamageReason(AUD_DAMAGE_REASON_DEFAULT),
	Fatal(false),
	PedWasAlreadyDead(false),
	PlayGargle(false),
	IsHeadshot(false),
	IsSilenced(false),
	IsSniper(false),
	IsFrontend(false),
	IsFromAnim(false),
	ForceDeathShort(false),
	ForceDeathMedium(false),
	ForceDeathLong(false)
	{
	}
	RegdEnt Inflictor;
	f32 RawDamage;
	s32 PreDelay;
	audDamageReason DamageReason;
	bool Fatal;
	bool PedWasAlreadyDead;
	bool PlayGargle;
	bool IsHeadshot;
	bool IsSilenced;
	bool IsSniper;
	bool IsFrontend;
	bool IsFromAnim;
	bool ForceDeathShort;
	bool ForceDeathMedium;
	bool ForceDeathLong;
};

struct audSpeechInitParams
{
	audSpeechInitParams()	:
	forcePlay(false),
	allowRecentRepeat(false),
	isConversation(false),
	sayOverPain(false),
	allowSubtitles(false),
	forceThroughSpeechBlock(false),
	isScripted(false),
	repeatTime(0),
	repeatTimeOnSameVoice(0),
	requestedVolume(0),
	requestedAudibility(CONTEXT_AUDIBILITY_NORMAL),
	preloadOnly(false),
	allowContextToOverride(true),
	preloadTimeoutInMilliseconds(30000),
	isConversationInterrupt(false),
	addBlip(false),
	fromScript(false),
	isUrgent(false),
	interruptAmbientSpeech(false),
	isMutedDuringSlowMo(true),
	isPitchedDuringSlowMo(true)
	{
	}

	audSpeechInitParams(const audSpeechInitParams& other)
	{
		forcePlay = other.forcePlay;
		allowRecentRepeat = other.allowRecentRepeat;
		isConversation = other.isConversation;
		sayOverPain = other.sayOverPain;
		allowSubtitles = other.allowSubtitles;
		forceThroughSpeechBlock = other.forceThroughSpeechBlock;
		isScripted = other.isScripted;
		repeatTime = other.repeatTime;
		repeatTimeOnSameVoice = other.repeatTimeOnSameVoice;
		requestedVolume = other.requestedVolume;
		requestedAudibility = other.requestedAudibility;
		preloadOnly = other.preloadOnly;
		allowContextToOverride = other.allowContextToOverride;
		preloadTimeoutInMilliseconds = other.preloadTimeoutInMilliseconds;
		isConversationInterrupt = other.isConversationInterrupt;
		addBlip = other.addBlip;
		fromScript = other.fromScript;
		isUrgent = other.isUrgent;
		interruptAmbientSpeech = other.interruptAmbientSpeech;
		isMutedDuringSlowMo = other.isMutedDuringSlowMo;
		isPitchedDuringSlowMo = other.isPitchedDuringSlowMo;
	}

	s32 repeatTime;
	s32 repeatTimeOnSameVoice;
	u32 requestedVolume;
	SpeechAudibilityType requestedAudibility;
	u32 preloadTimeoutInMilliseconds;
	bool forcePlay;
	bool allowRecentRepeat; 
	bool isConversation;
	bool sayOverPain;
	bool allowSubtitles;
	bool forceThroughSpeechBlock;
	bool isScripted;
	bool preloadOnly;
	bool allowContextToOverride; //Used when an audSpeechInitParams is made by hand
	bool isConversationInterrupt;
	bool addBlip;
	bool fromScript;
	bool isUrgent;
	bool interruptAmbientSpeech;
	bool isMutedDuringSlowMo;
	bool isPitchedDuringSlowMo;
};

struct audDeferredSayStruct
{
	audDeferredSayStruct()	:
	priority(0.0f),
	preDelay(-1),
	startOffset(0),
	requestedVolume(AUD_AMBIENT_SPEECH_VOLUME_UNSPECIFIED),
	requestedAudibility(CONTEXT_AUDIBILITY_NORMAL),
	voiceNameHash(0),
	contextPHash(0),
	variationNumber(-1),
	preloadTimeOutInMilliseconds(30000),
	speechContext(NULL),
	preloadOnly(false),
	fromScript(false),
	isMultipartContext(false),
	addBlip(true),
	isMutedDuringSlowMo(true),
	isPitchedDuringSlowMo(true)
	{
#if __BANK
		context[0] = '\0';
		debugDrawMsg[0] = '\0';
#endif
	}

#if __BANK
	char context[255];
	char debugDrawMsg[255];
#endif
	f32 priority;
	s32 preDelay;
	u32 startOffset;
	u32 requestedVolume;
	SpeechAudibilityType requestedAudibility;
	u32 voiceNameHash;
	u32 contextPHash;
	s32 variationNumber;
	u32 preloadTimeOutInMilliseconds;
	SpeechContext* speechContext;
	bool preloadOnly;
	bool fromScript;
	bool isMultipartContext;
	bool addBlip;
	bool isUrgent;
	bool isMutedDuringSlowMo;
	bool isPitchedDuringSlowMo;
};

struct BackupContextStruct{
	
	BackupContextStruct() :
	SpeechContext(0),
	Weight(0),
	AlreadyAttempted(false)
	{}

	u32 SpeechContext;
	f32 Weight;
	bool AlreadyAttempted;
};

struct audBlockedContextStruct
{
	scrThreadId ScriptThreadId;
	SpeechContextList* ContextList;
	audContextBlockTarget Target;
};

#pragma pack(push)
#pragma pack(1)

struct audGestureData{
	audGestureData()
	{
		Reset();
	}

	void Reset()
	{
		ClipNameHash = g_NullSoundHash;
		MarkerTime = 0;
		StartPhase = 0.0f;
		EndPhase = 1.0f;
		Rate = 1.0f;
		MaxWeight = 1.0f;
		BlendInTime = 0.3f;
		BlendOutTime = 0.3f;
		IsSpeaker = true;
	}


	u32 ClipNameHash;
	u32 MarkerTime;
	f32 StartPhase;
	f32 EndPhase;
	f32 Rate;
	f32 MaxWeight;
	f32 BlendInTime;
	f32 BlendOutTime;
	u32 IsSpeaker;
};

#pragma pack(pop)

struct audEchoMetrics
{
	audEchoMetrics()
	{
		vol = g_SilenceVolume;
		volPostSubmix = g_SilenceVolume;
		lpf = kVoiceFilterLPFMaxCutoff;
		hpf = kVoiceFilterHPFMinCutoff;
		predelay = 0;
		useEcho = false;
	}

	Vec3V pos;
	f32 vol;
	f32 volPostSubmix;
	f32 lpf;
	f32 hpf;
	u32 predelay;
	bool useEcho;
};

struct audSpeechMetrics
{
	audSpeechMetrics()
	{
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			vol[i] = 0.0f;
		}
		hpf = kVoiceFilterHPFMinCutoff;
		lpf = kVoiceFilterLPFMaxCutoff;
	}

	f32 vol[AUD_NUM_SPEECH_ROUTES];
	f32 hpf;
	f32 lpf;
};

const u32 g_AmbientSpeechMarkerHistorySize = 15;
const u32 g_ScriptedSpeechMarkerHistorySize = 15;

#if __BANK
struct VFXDebugStruct
{
	VFXDebugStruct() :
	pos(ORIGIN),
	time(0.0f),
	actor(NULL),
	color(0.0f,0.0f,0.0f)
	{
		msg[0] = '\0';
	}

	char msg[128];
	Vector3 pos;
	f32 time;
	CPed* actor;
	Color32 color;
};
#endif


// PURPOSE: Special derived clip dictionary
// Manipulates sysMemAllowResourceAlloc to free from streaming memory
class AudioClipDictionary : public crClipDictionary
{
public:

	AudioClipDictionary(datResource& rsc) :
		crClipDictionary(rsc)
		{
#if __ASSERT
			ValidateClipDictionary(*this, "AudioClipDictionary");
#endif // __ASSERT
		}

	~AudioClipDictionary() 
	{
		TrapZ(HasPageMap());

		if (HasPageMap())
		{
			datResourceMap map;
			RegenerateMap(map);

			// Update streaming allocator tally
			strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
			for (int i = 0; i < map.VirtualCount + map.PhysicalCount; ++i)
				allocator.RemoveFromMemoryUsed(map.Chunks[i].DestAddr);
		}
		
		// need to increment before pgBase destruction, to free from streaming memory
		sysMemAllowResourceAlloc++;
	}

	IMPLEMENT_PLACE_INLINE(AudioClipDictionary);

	void operator delete(void* ptr)
	{
		::delete (char*) ptr;
		// decrement after delete operation is complete, to reset increment above
		sysMemAllowResourceAlloc--;
	}
};

class audSpeechAudioEntity : public naAudioEntity
{	friend class CPacketSpeechPain;
public:
	AUDIO_ENTITY_NAME(audSpeechAudioEntity);

	static void InitClass();

	audSpeechAudioEntity();
	virtual ~audSpeechAudioEntity();
	void Init(CPed *parent);
	void Init()
	{
		naAssertf(0, "Can't init audSpeechAudioEntity without a CPed *");
	}
	void InitAnimalParams();

	void Resurrect();
	void GameUpdate();
	void Update();
	virtual void UpdateSound(audSound* sound, audRequestedSettings *reqSets, u32 timeInMs);
	void FreeSlot(bool freeInstantly = false, bool stopPain = true);

	static void SetLocalPlayerVoice(u32 playerVoiceHash)
	{
		sm_LocalPlayerVoice = playerVoiceHash;
	}
	static void SetLocalPlayerPainVoice(u32 playerPainVoiceHash)
	{
		sm_LocalPlayerPainVoice = playerPainVoiceHash;
	}
	static u32 GetLocalPlayerVoice()
	{
		return sm_LocalPlayerVoice;
	}
	static u32 GetLocalPlayerPainVoice()
	{
		return sm_LocalPlayerPainVoice;
	}

	CPed* GetParent() {return m_Parent;}
	CPed* GetReplyingPed() const { return m_ReplyingPed; }

	static bool IsMPPlayerPedAllowedToSpeak(const CPed* pPed, u32 voiceHash = 0u);

	void SetPedGender(bool isMale);
	u32 GetPVGHashFromRace(const PedRaceToPVG* raceToPVG,s32 pedRace = -1) const;

	void OrphanSpeech();
	void OrphanScriptedSpeechEcho();

	void PlayScriptedSpeech(const char *voiceName, const char *contextName, s32 variationNum = -1, audWaveSlot* waveSlot = NULL, 
		audSpeechVolume volType = AUD_SPEECH_NORMAL, bool allowLoad = false, u32 startOffset = 0, const Vector3& posForNullSpeaker = ORIGIN, CEntity* entForNullSpeaker = NULL, 
		u32 setVoiceNameHash = 0, audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL, bool headset = false, crClip* visemeData = NULL, bool isPadSpeakerRoute = false, s32 predelay = 0);
	bool IsScriptedSpeechPlaying() const;	// Faster version when you don't need the length etc.
	bool IsScriptedSpeechPlaying(s32* length, s32* playTime) const;
	u32  GetActiveScriptedSpeechPlayTimeMs(void);
	void StopPlayingScriptedSpeech();
	bool GetIsUrgent() const { return m_IsUrgent; }
	bool GetPositionForSpeechSound(Vector3& position, audSpeechVolume volType, const Vector3& posForNullSpeaker = ORIGIN, CEntity* entForNullSpeaker = NULL);
	f32 ComputePositionedHeadsetVolume(const CPed* pPed, const audSpeechType speechType, const f32 distanceToPed, bool forceHeadset = false) const;

#if GTA_REPLAY
	void ReplayPlayScriptedSpeech(u32 contextHash, u32 contextNameHash, u32 voiceNameHash, s32 preDelay, audSpeechVolume requestedVolume, audConversationAudibility audibility, s32 uVariationNumber, u32 uMsOffset, s32 waveSlot, bool activeSpeechIsHeadset, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo, bool isUrgent, bool playSpeechWhenFinishedPreparing);
	void ReplaySay(u32 contextHash, u32 voiceNameHash, s32 preDelay, u32 requestedVolume, u32 variationNum, f32 priority, u32 offsetMs, SpeechAudibilityType requestedAudibility, bool preloadOnly, CPed* replyingPed, bool checkSlotVacant, Vector3 positionForNullSpeech, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo);
	s32 GetReplayPreDelay()	{ return m_ReplayPreDelay; }
#endif

	u32 GetActiveSpeechPlayTimeMs(u32& predelay, u32& remainingPredelay);

	audSaySuccessValue CompleteSay();
	audSaySuccessValue Say(const char* context, audSpeechInitParams speechInitParams, u32 voiceNameHash = 0, s32 preDelay = -1, CPed* replyingPed = NULL, 
		u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, s32* const variationNumberOverride = NULL, const Vector3& posForNullSpeaker = ORIGIN);
	audSaySuccessValue Say(const char* context, const char *speechParamsObject = "SPEECH_PARAMS_STANDARD", u32 voiceNameHash = 0, s32 preDelay = -1, CPed* replyingPed = NULL, 
		u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, bool fromScript = false, s32* const variationNumberOverride = NULL, const Vector3& posForNullSpeaker = ORIGIN);
	audSaySuccessValue Say(u32 contextPartialHash, audSpeechInitParams speechInitParams, u32 voiceNameHash = 0, s32 preDelay = -1, CPed* replyingPed = NULL, 
		u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, s32* const variationNumberOverride = NULL, const Vector3& posForNullSpeaker = ORIGIN
#if __BANK
		, const char* context = ""
#endif
		);
	audSaySuccessValue Say(u32 contextPartialHash, const char *speechParamsObject = "SPEECH_PARAMS_STANDARD", u32 voiceNameHash = 0, s32 preDelay = -1, CPed* replyingPed = NULL, 
		u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, bool fromScript = false, s32 * const variationNumberOverride = NULL, const Vector3& posForNullSpeaker = ORIGIN
#if __BANK
		, const char* context = ""
#endif
		);
	//To be called from PreRender thread and kicked off on next update
	//Keeping it simple for now...if we have to create a struct to hold all the regular Say arguments for this, we will, though it's only being
	//	used in one place at the moment    RAK 2/13/13
	void SayWhenSafe(const char* context, const char *speechParamsObject = "SPEECH_PARAMS_STANDARD");
	void FillSpeechInitParams(audSpeechInitParams& speechInitParams, const SpeechParams* pSpeechParams);
	void AlterSpeechInitParamsPassedAsArg(audSpeechInitParams&speechInitParams, const SpeechContext* speechContext);
	u32 SelectBackupContext(TriggeredSpeechContext* context);
	audSaySuccessValue AttemptSayWithBackupContext(audSpeechInitParams speechInitParams, u32 voiceNameHash = 0, 
		s32 preDelay = -1, CPed* replyingPed = NULL, u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f);
	void SetJackingSpeechInfo(CPed* jackedPed, CVehicle* vehicle, bool jackFromPassengerSide = false, bool vanJackClipUsed = false);
	void TriggerJackingSpeech();
	void SetOpenedDoor();
	u32 GetLastTimeOpenedDoor()	{ return m_LastTimeOpeningDoor; }

	void PlayTrevorSpecialAbilitySpeech();

	void PlayCarCrashSpeech(CPed* otherDriver, CVehicle* myVehicle, CVehicle* otherVehicle);

	bool IsPreloadedSpeechReady();
	bool CheckAndPrepareSpeechSound(audSpeechSound* sound, audWaveSlot* waveSlot);

	void KillPreloadedSpeech(bool bFromReset=false);
	bool IsPreloadedSpeechPlaying(bool* playTimeOverZero = NULL) const;

	void PlayPreloadedSpeech_FromScript();
	void PlayPreloadedSpeech(s32 uTimeOfPredelay);
	bool IsAmbientSpeechPlaying() const;
	u32	 GetCurrentlyPlayingAmbientSpeechContextHash() const {return m_LastPlayedSpeechContextPHash;};
	u32	 GetCurrentlyPlayingAmbientSpeechVariationNumber() const {return m_LastPlayedSpeechVariationNumber;};
	u32 GetLastVariationNumber() const {return m_VariationNum;}
	void StopAmbientSpeech();
	void StopPain();

	bool AudioHandlingBreathingVfx() {return m_BreathSound != NULL;};

	audSpeechType GetCachedSpeechType()	const { return m_CachedSpeechType; }

	bool IsMegaphoneSpeechPlaying() const;

	void StopSpeech(); // stops ambient and scripted
	void StopSpeechForinterruptingAmbientSpeech();
	bool GetIsAnySpeechPlaying();
	void MarkToStopSpeech(); //call if we need to stop speech from another thread.
	void MarkToSaySpaceRanger();
	void MarkToPlayCyclingBreath();

	//void ChooseFullVoiceIfUndecided();
	void ChooseVoiceIfUndecided(u32 contextPHash);
	void ForceFullVoice(s32 PVGHash = 0,s32 pedRace = -1);

	bool GetLastMetadataHash(u32 categoryId, u32 &hash);
	bool GetLastMetadataHash(u32 categoryId, u32 &hash, u32 &data);
	bool GetLastMetadataValue(u32 categoryId, f32 &value);

	void GetVisemeData(void **data);
	audVisemeType GetVisemeType() { return m_VisemeType; }
	f32 GetSpeechHeadroom();

	audWaveSlot* AddRefToActiveSoundSlot();
	void GetAndParseGestureData();
	const audGestureData* GetLastGestureDataForCategoryID(u32 categoryId);

	void SetIsHavingAConversation(bool isHavingAConversation);
	bool GetIsHavingAConversation() {return m_IsHavingAConversation;}
	bool GetIsHavingANonPausedConversation();
	u32	 GetTimeLastInScriptedConversation() {return m_LastScriptedConversationTime;}
	//have to add this due to wackiness when anim-triggering speech stop happens during a conversation and it takes a bit for the
	//	pain trigger to arrive
	bool GetIsOkayTimeToStartConversation();

	// This returns the cached answer - unless no voice is yet allocated, when it checks the full voices in the PedVoiceGroup
	u16  GetPossibleConversationTopics();

	void DisableSpeaking(bool shouldDisable )
	{
		m_SpeakingDisabled = shouldDisable ;
	}

	void BlockAllSpeechFromPed(bool shouldBlock, bool shouldSuppressOutgoingNetworkSpeech)
	{
		m_AllSpeechBlocked = shouldBlock;
		m_SpeechBlockAffectsOutgoingNetworkSpeech = shouldSuppressOutgoingNetworkSpeech;
	}

	void DisableSpeakingSynced(bool shouldDisable)
	{		
		m_SpeakingDisabledSynced = shouldDisable;
	}

	void DisablePainAudio(bool shouldDisable )
	{
		m_PainAudioDisabled = shouldDisable ;
	}

	bool IsSpeakingDisabled() const
	{
		return m_SpeakingDisabled;
	}

	bool IsAllSpeechBlocked() const
	{
		return m_AllSpeechBlocked;
	}

	bool IsOutgoingNetworkSpeechBlocked() const
	{
		return m_AllSpeechBlocked && m_SpeechBlockAffectsOutgoingNetworkSpeech;
	}

	bool IsSpeakingDisabledSynced() const
	{
		return m_SpeakingDisabledSynced;
	}

	bool IsPainAudioDisabled() const
	{
		return m_PainAudioDisabled;
	}

	// Pain
	void InflictPain(audDamageStats& damageStats);
	void ReallyInflictPain(audDamageStats& damageStats);
	void RegisterFallingStart();
	void UpdateFalling(const u32 timeOnAudioTimer0);
	void ResetFalling();
	bool IsPainPlaying() const;
	void SetDeathIsFromEnteringRagdoll() {m_DeathIsFromEnteringRagdoll = true;}
	void SetCryingContext(const char* context);
	void UnsetCryingContext();
	void StartCrying();
	void StopCrying();
	void TriggerSingleCry();
	void RegisterLungDamage();
	bool ShouldPlayCoughForDamagedLungs() const;
	void TriggerPainFromPainId(int painIdNum, const s32 preDelay);

	s32 GetRandomVariation(u32 voiceNameHash, const char* context, u32* timeContextLastPlayedForThisVoice, bool* hasBeenPlayedRecently, bool isMultipart = false);//, char* voiceName=NULL);
	s32 GetRandomVariation(u32 voiceNameHash, u32 contextPHash, u32* timeContextLastPlayedForThisVoice, bool* hasBeenPlayedRecently, bool isMultipart = false);//, char* voiceName=NULL);

	// have this public because speechManager uses it for churn
	u32 GetPedVoiceGroup( bool bCreateIfUnassigned = true, s32 pedRace = -1) const;

	bool DoesContextExistForThisPed(const char* context, bool allowBackupPVG = false) const;
	bool DoesContextExistForThisPed(u32 contextPHash, bool allowBackupPVG = false) const;
	//Returns false if it fails to find a voice for this context and ped
	bool SetPedVoiceForContext(const char* context, bool allowBackupPVG = false);
	bool SetPedVoiceForContext(u32 contextPHash, bool allowBackupPVG = false);
	// If this entity already has an ambient voice set, it will check if it has any variations for phone conversations.
	//	Otherwise, it will call GetVoiceForPhoneConversation and return true if GetVoice... returned a valid voice
	bool DoesPhoneConversationExistForThisPed() const;
	//Attempts to set the entity's ambient voice to something appropriate for a phone conversation. 
	//Also picks a variation and returns the value (0 if the function fails to find an appropriate voice.)
	//The function adds this conversation to the history unless automaticallyAddToHistory is set to false.  We do this here to avoid ending up with two peds
	//	using the same voice if two conversations are requested back-to-back on peds with the same or similar PVGs.
	//TODO: Add a function to remove an entry from the history if this ends up assigning non-full voices to peds too often.
	u8 SetAmbientVoiceNameAndGetVariationForPhoneConversation(bool automaticallyAddToHistory = true);

	bool CanPedHaveChatConversationWithPed(CPed* ped, u32 statePHash, u32 respPHash, bool allowBackupPVG=false);

    u32  GetAmbientVoiceName() const { return m_AmbientVoiceNameHash; }
	void SetAmbientVoiceName(u32 voiceNameHash, bool forceSet = false);
	void SetAmbientVoiceName(char* voiceName, bool forceSet = false) ;
	void SetAmbientVoiceName(const char* context, bool allowBackupPVG = false) ;
	
	void SetPedIsDrunk(bool isDrunk)
	{
		m_PedIsDrunk = isDrunk;
	}
	bool IsPedDrunk() const
	{
		return m_PedIsDrunk;
	}

	void SetPedIsBlindRaging(bool isBlindRaging)
	{
		m_PedIsBlindRaging = isBlindRaging;
	}
	bool PedIsBlindRaging()
	{
		return m_PedIsBlindRaging;
	}

	void SetCodeSetIsAngry(bool isAngry) { m_IsCodeSetAngry = isAngry; }
	void SetPedIsAngry(bool isAngry);
	bool IsPedAngry()
	{
		return m_IsAngry;
	}
	void SetPedIsAngryShortTime(u32 timeInMs);
	bool ShouldBeAngry() const;
	bool IsAnimal() {return m_AnimalParams != NULL;}
	AnimalType GetAnimalType() { return m_AnimalParams ? (AnimalType)m_AnimalParams->Type : kAnimalNone; }
	void SetAnimalMood(audAnimalMood mood) { m_AnimalMood = mood; }
	void SetPedIsBuddy(bool isBuddy);
	void SetIsInRapids(bool isInRapids) { m_IsInRapids = isInRapids; }

	u32 GetCurrentlyPlayingVoiceNameHash(void) { return m_CurrentlyPlayingVoiceNameHash;	}

	s8 GetPedToughnessWithoutSetting() const;
	u8 GetPedToughness();

	void PlayAnimalVocalization(const AnimalVocalAnimTrigger* trigger);
	void PlayAnimalVocalization(AnimalType animalType, const char* context);
	void PlayAnimalVocalization(u32 contextPHash
#if __BANK		
								, const char* context = ""
#endif
								);
	void PlayAnimalNearVocalization(u32 contextPHash
#if __BANK		
								, const char* context = ""
#endif
								);
	void PlayAnimalFarVocalization(u32 contextPHash
#if __BANK		
									, const char* context = ""
#endif
									);
	bool IsAnimalVocalizationPlaying() const;

	bool IsAnimalContextABark(u32 contextPHash);
	void TriggerSharkAttackSound(CPed* target, bool secondCall=false);
	void PlaySharkVictimPain();

	const audWaveMarker *GetLastMarkerForCategoryId(u32 categoryId);
	u32 GetConversationInterruptionOffset(u32& playtime, bool& hasNoMarkers);

	void Panic(const s32 eventType, const bool isFirstReaction);

	void SetHasBeenThrownFromVehicle();
	void SetToPlayGetWantedLevel();
	static void UpdateClass();
	static void PlayGetWantedLevel();
	static s32 GetNextLaughterTrackMarkerIndex(const RadioTrackTextIds *textIds, u32 currentPlayTimeMs);
#if __BANK
	u32 GetLastTimeAnySpeechPlayed() { return m_LastTimePlayingAnySpeech; }

	static void PlaySpeechCallback(void);
	static void ResetSpecPainStatsCallback(void);
	static void DebugDraw();
	static void AddVFXDebugDrawStruct(const char* msg, CPed* actor);
	static void RemoveVFXDebugDrawStruct(CPed* actor);
	static void UpdateVFXDebugDrawStructs();
	static void AddAmbSpeechDebugDrawStruct(const char* msg, CPed* actor, Color32 color);
	static void RemoveAmbSpeechDebugDrawStruct(CPed* actor);
	static void UpdateAmbSpeechDebugDrawStructs();
	static void DisplayVoiceNameHashes();
	static void MakeSelectedPedConvTester(CPed* pSelectedEntity);
	static void ConvTesterCB();
	static void AddWidgets(bkBank &bank);
	static void StartLaughterTrackEditor();
	static void StopLaughterTrackEditor();
	static void SaveLaughterTrack();
	static void SetChannelCB();
#endif

#if __BANK
		void DisplayVoiceDebugInfo();

		void GetCurrentlyPlayingWaveName(char* waveName)
		{
			if (waveName && (m_ScriptedSpeechSound || m_AmbientSpeechSound))
			{
				formatf(waveName, 64, m_LastPlayedWaveName);
			}
		}
		void GetCurrentlyPlayingScriptedVoiceName(char* voiceName)
		{
			if (voiceName && m_ScriptedSpeechSound)
			{
				formatf(voiceName, 64, m_LastPlayedScriptedSpeechVoice);
			}
		}
#endif

		void UpdatePlayerBreathing();
		void PlayPlayerBreathing(PlayerVocalEffectType eBreathType, s32 predelay = -1, f32 volOffset = 0.0f, bool isOnBike = false);

		void PlayTennisVocalization(TennisVocalEffectType tennisVFXType);

		u32 GetLastTimeFalling() { return m_LastTimeFalling; }
		void SetBlockFallingVelocityCheck();
		bool GetIsFallScreaming() { return m_bIsFallScreaming; }
		void SetJackingReplyShouldDelay();

		u32 GetLastTimePedaling() { return m_LastTimePedaling; }

		audSpeechVolume GetCachedVolType() const { return m_CachedVolType; }

		static crClipDictionary* GetVisemeData(void **data, audSpeechSound* sound
#if !__FINAL
												, bool& failedToInitWaveRef
#endif	
						);
		static crClipDictionary* ProcessClipDictionaryResource(void* chunkData, u32 numBytes);

		static bool IsMultipartContext(u32 contextPHash);

		static void MakeTrevorRage();
		static void OverrideTrevorRageContext(const char* context, bool fromScript=false);
		static void ResetTrevorRageContext(bool fromScript=false);

		bool ShouldBlockContext(u32 contextHash);
		static void RemoveAllContextBlocks();
		static void RemoveContextBlocks(scrThreadId ScriptThreadId);
		static void RemoveContextBlock(const char* contextListName);
		static void AddContextBlock(const char* contextListName, audContextBlockTarget target, scrThreadId ScriptThreadId);

		static void TriggerCollisionScreams();
		const char* GetPainContext(u32 painType) const;

		static bool CanPlayPlayerCarHitPedSpeech();
		static void SetPlayerCarHitPedSpeechPlayed();

		static u32 sm_TimeToNextPlayShotScream;
		static u32 sm_NumPedsCrying;

#if GTA_REPLAY
		void SetLastPainLevel(u32 painLevel)				{	m_LastPainLevel = painLevel;	}
		void SetLastPainTimes(u32 painTime0, u32 painTime1)	{	m_LastPainTimes[0] = painTime0; m_LastPainTimes[1] = painTime1;	}
#endif // GTA_REPLAY

private:

//	u32 GetRandomCopVoiceNameHash();

	void TriggerReplySpeech(CPed* replyingPed, u32 replyingContext, s32 delay);

#if __BANK
	u32 GetFakeGestureHashForContext(SpeechContext* context);
#endif
	// This internally calculates possible topics - externally, use GetPossibleCOnversationTopics() for the cached answer
	//u16 CalculatePossibleConversationTopics(u32 voiceNameHash);

	const audSoundSet* GetSoundSetForAmbientSpeech(const bool isPain) const;

	audSpeechVolume GetVolTypeForSpeechContext(const SpeechContext* context, const u32 contextPHash) const;
	audSpeechVolume GetVolTypeForSpeechParamsVolume(const audAmbientSpeechVolume requestedVolume) const;
	audSpeechVolume FindVolTypeToUseForAmbientSpeech(const audSpeechVolume contextVolType, const audSpeechVolume paramVolType) const;
	audConversationAudibility GetAudibilityForSpeechContext(const SpeechContext* context) const;
	audConversationAudibility GetAudibilityForSpeechAudibilityType(const SpeechAudibilityType requestedAudibility) const;

	void ComputeAmountToUsePreviousHighAudibility(const audSpeechVolume volType, const audConversationAudibility audibility);

	// Will return false if we pass in bad enum values or we can't get the ped's head matrix because it's !isFinite
	bool GetSpeechAudibilityMetrics(const Vec3V& speakerPos, const audSpeechType speechType, const audSpeechVolume volType, const bool inCarAndSceneSaysFrontend, 
									const audConversationAudibility audibility, audSpeechMetrics& metrics, audEchoMetrics& echo1, audEchoMetrics& echo2);
	bool UpdatePedHeadMatrix();
	void ComputeVehicleOpennessMetrics(const audSpeechVolume volType, f32& vol, f32& lpf) const;
	void ComputeSpeakerVolumeConeMetrics(const Vec3V& listToSpeakerNorm, const audSpeechVolume volType, const f32 distanceToPed, f32& vol, f32& lpf) const;
	void ComputeListenerVolumeConeMetrics(const Vec3V& listToSpeakerNorm, const audSpeechType speechType, const audSpeechVolume volType, const f32 distanceToPed, f32& vol, f32& lpf) const;
	void ComputeSpeechFEBubbleMetrics(const Vec3V& listToSpeakerNorm, const audSpeechType speechType, const audSpeechVolume volType, 
										const f32 distanceToPed, const f32 volTotal, f32& volPos, f32& volFE) const;
	void ComputeDistanceMetrics(const audSpeechType speechType, const audSpeechVolume volType, const audConversationAudibility audibility, 
								 f32 distanceToPed, f32& vol, f32& linHPF) const;
	f32 ComputeAmountToUseEnvironmentForFESpeech() const;
	void ComputeEchoInitMetrics(const Vector3& posSpeaker, const audSpeechVolume volType, const bool useEcho1, const bool useEcho2, audEchoMetrics& echo1, audEchoMetrics& echo2) const;
	void ComputeEchoMetrics(const Vec3V& posSpeaker, const audSpeechType speechType, const audSpeechVolume volType, const f32 volEchoBase, const f32 hpf, audEchoMetrics& echo) const;

	f32 ComputePostSubmixVolume(const audSpeechType speechType, const audSpeechVolume volType, const bool inCarAndSceneSaysFrontend, const f32 volOffset = 0.0f);
	f32 ComputeVolumeOffsetFromPlayerInCar();
	f32 GetSpeechPitchRatio();
	bool IsMutedDuringSlowMo();
	bool IsPitchedDuringSlowMo();
	
	void UpdateSpeechAndShadowSounds(const audSpeechType speechType, audSpeechMetrics& metrics, const Vector3& pos, bool& killedSpeech, const u32 timeOnAudioTimer0);
	void UpdateSpeechSound(const audSpeechType speechType, const audSpeechMetrics& metrics, const Vector3& pos);
	void UpdateSpeechEchoes(const audSpeechType speechType, const audEchoMetrics& echo1, const audEchoMetrics& echo2);

	// Finds LPF and reverb levels to use for non-player speech when the player is underwater
	void UpdateUnderwaterMetrics();

	void ResetSpeechSmoothers();

	const audSpeechSound* GetEchoPrimary() const;
	const audSpeechSound* GetEchoSecondary() const;

	int LookForPainContextInSay(u32 contextPHash, bool isPlayer, bool& isBankLoadedPain, bool& isPartialHash);
	bool PlayPain(u32 painLevel, audDamageStats damageStats, f32 distSqToVolListener);
	void PlayWaveLoadedPain(u32 painType, u32 painVoiceType, s32 preDelay);

	bool IsInCarFrontendScene() const; 

	bool IsPedInPlayerCar(CPed* ped, bool* isBus=NULL) const;

	u32 SelectActualVoice(u32 voiceNameHash);

	bool IsEpsilonPed(u32 voiceNameHash);
	u32 GetSpecificGreetForPlayer(CPed* replyingPed);

	bool GetIsMegaphoneContext(u32 c);

	void ResetTriggeredContextVariables();

	u32 GetHighDeathPainTypeFromDistance(f32 dist);

	void GetVoumeAndRolloffForAnimalContext(u32 contextPHash, f32& volumeOffset, f32& rolloff, f32& probability, u32& priority, bool& isOverlapping);
	u32 GetAngryContextPHashFromAnimTrigger(AnimalVocalAnimTrigger::tAnimTriggers animTrigger);
	u32 GetPlayfulContextPHashFromAnimTrigger(AnimalVocalAnimTrigger::tAnimTriggers animTrigger);	

	u32 CheckForModifiedVersion(u32 contextPHash, u32 voiceHash, u32& modVariation);

	void ReallyPlayAnimalVocalization(const AnimalVocalAnimTrigger* trigger);
	f32 GetMaxDistanceSqForAnimalContextPHash(u32 contextPHash);
	void UpdatePanting(u32 timeOnAudioTimer0);

	bool CanSayAngryLineToPlayer(CPed* jackedPed, u32& angryLineContext);

	audSpeechPanicType GetPanicType(const s32 eventType);

	bool CreateShadow(const audMetadataRef metaRef, audSound** sound, audSoundInitParams& initParam  ASSERT_ONLY(, const char* context, const u32 contextPHash));
	void CreateEcho(const audEchoMetrics& echo, const audSpeechType speechType, const audSpeechVolume volType, const f32 volPostSubmix, const u32 predelay, const f32 rolloff, 
					const u32 voiceHash, const u32 contextPHash, const s32 variation, const audMetadataRef metaRef, audSpeechSound** sndEcho, audSoundInitParams& initParam);

#if __BANK
	void UpdateDebug();
	void PrintSoundPool(ASSERT_ONLY(const char* context, const u32 contextPHash, const audMetadataRef soundRef));
#endif

#if __BANK
	char m_CurrentlyPlayingSpeechContext[128];
#endif
	Vector3 m_PositionForNullSpeaker;
	Matrix34 m_PedHeadMatrix;

	audDamageStats m_DamageStats;

	u32 m_ScriptedSoundNameHash;
	audSoundSet m_CurrentlyPlayingSoundSet;
	u32 m_CurrentlyPlayingPredelay1;
	u32 m_CurrentlyPlayingVoiceNameHash;
	u32 m_TotalPredelayForLipsync;

	crClip *m_VisemeAnimationData;
	
	atRangeArray<u32, NUM_AUD_SPEECH_METADATA_CATEGORIES> m_SpeechMetadataCategoryHashes;
	atRangeArray<s32, NUM_AUD_SPEECH_METADATA_CATEGORIES> m_LastQueriedMarkerIdForCategory;
	s32 m_VariationNum;
	CEntity* m_EntityForNullSpeaker;
	u32 m_TimeCanBeInConversation;

	const audGestureData* m_GestureData;
	u32 m_NumGestures;
	audWaveSlot* m_WaveslotUsedWhenLastAddingGestureRef;
#if __BANK
	audGestureData m_FakeGestureData;
#endif
	atRangeArray<s32, NUM_AUD_SPEECH_METADATA_CATEGORIES> m_LastQueriedGestureIdForCategory;

	u32 m_AmbientVoiceNameHash;
	s32 m_AmbientSlotId;
	audWaveSlot* m_ScriptedSlot;
	audWaveSlot* m_LastPlayedWaveSlot;
	u32 m_LastPlayedSpeechContextPHash;
	u32 m_LastPlayedSpeechVariationNumber;

	u32 m_TimeToPlayCollisionScream;
	u32 m_TimeToStopBeingAngry;
	u32 m_SpeechSyncId;

	CPed *m_Parent;

	u32 m_AnimalSoundPriority;
	AnimalType m_AnimalType;
	bool m_IsCrying;
	bool m_isMaleSet;
	bool m_IsMale;
	bool m_IsFalling;
	bool m_IsHighFall;
	bool m_bIsFallScreaming;
	bool m_HasPlayedInitialHighFallScream;
	bool m_ShouldStopSpeech;
	bool m_ShouldSaySpaceRanger;
	bool m_ShouldPlayCycleBreath;
	bool m_FirstUpdate;
	bool m_HasContextToPlaySafely;
	bool m_WaitingToPlayCollisionScream;
	bool m_IsAngry;
	bool m_ShouldUpdatePain;
	bool m_Preloading;
	bool m_WaitingForAmbientSlot;   //Used with new speech priority system.  This is true is we've just kicked some other speech out of
	// a slot and are waiting for the slot be clean be vacant.
	bool m_IsCoughing;

	public:
	audSpeechSound* m_ScriptedSpeechSound;
	bool m_HasPreparedAndPlayed;
	private:
	atRangeArray<audSound*, AUD_NUM_SPEECH_ROUTES> m_ScriptedShadowSound;
	audSpeechSound* m_ScriptedSpeechSoundEcho;
	audSpeechSound* m_ScriptedSpeechSoundEcho2;
	audSpeechSound* m_AmbientSpeechSound;
	atRangeArray<audSound*, AUD_NUM_SPEECH_ROUTES> m_AmbientShadowSound;
	audSpeechSound* m_AmbientSpeechSoundEcho;
	audSpeechSound* m_AmbientSpeechSoundEcho2;
	audSpeechSound* m_PainSpeechSound;
	atRangeArray<audSound*, AUD_NUM_SPEECH_ROUTES> m_PainShadowSound;
	audSpeechSound* m_PainEchoSound;
	audSound* m_BreathSound;
	audSpeechSound* m_AnimalVocalSound;
	atRangeArray<audSound*, AUD_NUM_SPEECH_ROUTES> m_AnimalVocalShadowSound;
	audSpeechSound* m_AnimalVocalEchoSound;
	atRangeArray<audSpeechSound*, AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS> m_AnimalOverlappableSound;
	AnimalParams* m_AnimalParams;
	audSound* m_HeadSetSubmixEnvSnd;

	char m_ContextToPlaySafely[64];
	u8 m_AmbientVoicePriority;
	u8 m_Toughness;
	u16 m_PossibleConversationTopics;

	//atArray<BackupContextStruct> m_BackupSpeechContexts;
	//bool m_AllowedToFillBackupSpeechArray;
	TriggeredSpeechContext* m_TriggeredSpeechContext;

	char m_PlaySafelySpeechParams[64];
	char m_AlternateCryingContext[64];

	u32 m_LastPainLevel;
	//u32 m_NextPainTime;
	atRangeArray<u32, 2> m_LastPainTimes;  //Hold last two pain times, so we can fall a level of toughness if hit multiple time in quick succession

	u32 m_LastTimeInVehicle;
	u32 m_LastTimePedaling;
	u32 m_LastTimeNotPedaling;
	f32 m_BreathingVolumeLin;
	u32 m_TimeToNextPlayJogLowBreath;
	u32 m_LastTimeRunning;
	u32 m_LastTimeSprinting;
	u32 m_LastTimeNotRunningOrSprinting;
	u32 m_LastTimeScubaBreathPlayed;
	PlayerBreathState m_BreathState; CompileTimeAssert(kNumBreathingStates<16);

	u32 m_PantStateChangeTime;
	AnimalPantState m_PantState;

	atArray<const AnimalVocalAnimTrigger*> m_DeferredAnimalVocalAnimTriggers;
	audAnimalMood m_AnimalMood;

	u32 m_LastScriptedConversationTime;

	atRangeArray<audSimpleSmoother, AUD_NUM_SPEECH_ROUTES> m_SpeechVolSmoother;
	audSimpleSmoother m_SpeechLinLPFSmoother;
	audSimpleSmoother m_SpeechLinHPFSmoother;

	s8 m_ActiveSubmixIndex;

	f32 m_EchoVolume;
	f32 m_BlipNoise;
	u32 m_TimeLastDrewSpeechBlip;

	// Cached values when creating the sound so we know how to properly update them
	u32 m_CachedSpeechStartOffset;
	audSpeechType m_CachedSpeechType;
	audSpeechVolume m_CachedVolType;
	audConversationAudibility m_CachedAudibility;
	f32 m_CachedRolloff;

	audConversationAudibility m_GreatestAudibility;
	u32 m_LastHighAudibilityLineTime;
	f32 m_AmountToUsePreviousHighAudibility;

	f32 m_SpeechVolumeOffset;

	RegdPed m_ReplyingPed;
	u32   m_ReplyingContext;
	s32 m_ReplyingDelay;

	audDeferredSayStruct m_DeferredSayStruct;
	audSaySuccessValue m_SuccessValueOfLastSay;

	u32 m_PreloadFreeTime;


	audVisemeType m_VisemeType;

#if __BANK
	char m_LastPlayedWaveName[64];
	char m_LastPlayedScriptedSpeechVoice[64];
#endif

	u32 m_LungDamageStartTime;
	u32 m_LungDamageNextCoughTime;
	u32 m_TimeOfLastCoughCheck;
	u32 m_WaveLoadedPainType;
	u32 m_TimeToNextCry;

	u32 m_TimeToStartLandProbe;
	u32 m_LastTimeFalling;

	u32 m_MultipartContextCounter;
	u32 m_OriginalMultipartContextPHash;
	u32 m_MultipartContextPHash;
	u32 m_MultipartVersion;
	u32 m_MultipartContextNumLines;

	u32 m_LastTimeThrownFromVehicle;
	u32 m_LastTimeFallingVelocityCheckWasSet;

	u32 m_TimeToPlayJackedReply;
	u32 m_TimeJackedInforWasSet;
	u32 m_LastTimeOpeningDoor;
	RegdPed m_JackedPed;
	RegdVeh m_JackedVehicle;

	s8 m_FramesUntilNextFallingCheck;

	bool m_bIsGettingUpAfterVehicleEjection;
	bool m_JackedFromPassengerSide;
	bool m_PreloadedSpeechIsPlaying;
	bool m_ToughnessIsSet;
	bool m_PedHasDied;
	bool m_PedHasGargled;
	bool m_PedIsDrunk;
	bool m_PedIsBlindRaging;
	bool m_VanJackClipUsed;
	bool m_IsHavingAConversation;
	bool m_DropToughnessLevel;
	bool m_IsAttemptingTriggeredPrimary;
	bool m_IsAttemptingTriggeredBackup;
	bool m_WasMakingAnimalSoundLastFrame;
	bool m_WasPlayingAcitveSoundLastFrame;
	bool m_GesturesHaveBeenParsed;
	bool m_IsUnderwater;
	bool m_PainAudioDisabled;
	bool m_SpeakingDisabled;
	bool m_SpeakingDisabledSynced;
	bool m_AllSpeechBlocked;
	bool m_SpeechBlockAffectsOutgoingNetworkSpeech;
	bool m_IsCodeSetAngry;
	bool m_IsBuddy;
	bool m_IgnoreVisemeData;
	bool m_WaitingToPlayFallDeath;
	bool m_DeathIsFromEnteringRagdoll;
	bool m_DelayJackingReply;
	bool m_DontAddBlip;
	bool m_IsUrgent;
	bool m_HasPreloadedSurfacingBreath;
	bool m_ShouldPlaySurfacingBreath;
	bool m_LastBreathWasInhale;
	bool m_LastScubaBreathWasInhale;
	bool m_IsFromScript;	// Used when computing postSubmixVol, cache this so in PlayPreloadedSpeech we know what modifiers to apply.
	bool m_IsInRapids;
	bool m_ActiveSpeechIsHeadset;
	bool m_ActiveSpeechIsPadSpeaker;
	bool m_PlaySpeechWhenFinishedPreparing;
	bool m_UseShadowSounds;
	bool m_SuccessfullyUsedAmbientVoice;
	bool m_ForceSetAmbientVoice;
	bool m_PlayingSharkPain;
	bool m_IsPitchedDuringSlowMo;
	bool m_IsMutedDuringSlowMo;
	bool m_BreathingOnBike;
	bool m_ShouldResetReplyingPed;
	bool m_SpectatorPedInVehicle;
	bool m_SpectatorPedVehicleIsBus;
#if __BANK
	mutable bool m_UsingFakeFullPVG;
	u32 m_ContextToPlaySilentPHash;

	u32 m_FakeGestureNameHash;
	s32 m_TimeOfFakeGesture;
	s32 m_SoundLengthForFakeGesture;
	audWaveMarker m_FakeMarker;
	bool m_FakeMarkerReturned;

	u32 m_LastTimePlayingAnySpeech;
	u32 m_TimeScriptedSpeechStarted;

	bool m_UsingStreamingBucking;
	bool m_IsDebugEntity;
#endif
	
#if !__FINAL
	bool m_IsPlaceholder;
	bool m_AlertConversationEntityWhenSoundBegins;
#endif

#if GTA_REPLAY
	u32 m_LastTimePlayingReplayAmbientSpeech;
	u32 m_ActiveSoundStartTime;
	u32 m_ContextNameStringHash;

	s32 m_ReplayPreDelay;
	s32 m_ReplayWaveSlotIndex;
#endif

	static u32 sm_TimeLastPlayedJackingSpeech;
	static u32 sm_TimeLastPlayedHighFall;
	static u32 sm_NumExplosionPainsPlayedThisFrame;
	static u32 sm_NumExplosionPainsPlayedLastFrame;

	static u32 sm_TimeLastPlayedCrashSpeechFromPlayerCar;

	static u32 sm_LocalPlayerVoice;
	static u32 sm_LocalPlayerPainVoice;
	static char sm_TrevorRageContextOverride[64];
	static u32 sm_TrevorRageContextOverridePHash;
	static bool sm_TrevorRageOverrideIsScriptSet;

	static bool ms_ShouldProcessVisemes;

	static audCurve sm_StartHeightToLandProbeWaitTime;
	static audCurve sm_HeadsetSpeechDistToVol;
	static audCurve sm_DistToListenerConeStrengthCurve;
	static audCurve sm_FEBubbleAngleToDistanceInner;
	static audCurve sm_FEBubbleAngleToDistanceOuter;
	static audCurve sm_EqualPowerFallCrossFade;
	static audCurve sm_EqualPowerRiseCrossFade;
	static audCurve sm_TimeSinceDamageToCoughProbability;
	static audCurve sm_BuiltUpToEchoWetMedium;
	static audCurve sm_BuiltUpToEchoWetLarge;
	static audCurve sm_BuiltUpToEchoVolLinShouted;
	static audCurve sm_BuiltUpToEchoVolLinMegaphone;
	static audCurve sm_EchoShoutedDistToVolLinCurve;
	static audCurve sm_EchoMegaphoneDistToVolLinCurve;
	static audCurve sm_VehicleOpennessToReverbVol;
	static audCurve sm_FallVelToStopHeightCurve;
	
	static audCurve sm_SpeechDistanceToHPFCurve;
	static atMultiRangeArray<audCurve, AUD_NUM_SPEECH_VOLUMES, AUD_NUM_AUDIBILITIES> sm_ScriptedDistToVolCurve;
	static atMultiRangeArray<audCurve, AUD_NUM_SPEECH_VOLUMES, AUD_NUM_AUDIBILITIES> sm_AmbientDistToVolCurve;
	static atMultiRangeArray<audCurve, AUD_NUM_SPEECH_VOLUMES, AUD_NUM_AUDIBILITIES> sm_PainDistToVolCurve;

	static u32	sm_TimeOfNextCoughSpeech;
	static u32 sm_LastTimeCoughCheckWasReset;
	static bool sm_PerformedCoughCheckThisFrame;

	static atRangeArray<u32, AUD_AMB_MOD_TOT> sm_LastTimeAmbSpeechModsUsed;
	static atRangeArray<audBlockedContextStruct, AUD_MAX_BLOCKED_CONTEXT_LISTS> sm_BlockedContextLists;
	static u8 sm_NumCollisionScreamsToTrigger;
	static ePedType sm_PlayerPedType;

	static f32 sm_UnderwaterLPF;
	static f32 sm_UnderwaterReverb;

	static u32 sm_TimeOfLastPlayerCarHitPedSpeech;
	static u32 sm_TimeOfLastChat;
	static u32 sm_TimeOfLastSaySpaceRanger;

	static RegdPed sm_PedToPlayGetWantedLevel;
#if __BANK
	static atRangeArray<VFXDebugStruct, 64>sm_VFXDebugDrawStructs;
	static atRangeArray<VFXDebugStruct, 64>sm_AmbSpeechDebugDrawStructs;
	static CPed* sm_CanHaveConvTestPed;
	static char sm_VisemeTimingingInfoString[256];
#endif
};

#endif // AUD_SPEECHAUDIOENTITY_H
