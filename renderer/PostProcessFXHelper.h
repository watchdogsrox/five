//
// Filename:	PostProcessFXHelper.h
// Description:	This file contains miscellaneous, PostFX related classes

#ifndef __POSTPROCESSFX_HELPER_H__
#define __POSTPROCESSFX_HELPER_H__

#include "atl/hashstring.h"
#include "atl/array.h"
#include "atl/functor.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "data/callback.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "fwtl/LinkList.h"
#include "parser/macros.h"
#include "scene/RegdRefTypes.h"
#include "timecycle/tckeyframe.h"
#include "renderer/color.h"

#include "grcore/effect.h"
#include "grcore/stateblock.h"
#include "vector/color32.h"

#include "control/replay/ReplaySettings.h"
#include "timecycle/TimeCycle.h"

namespace rage
{
	class grcTexture;
	class grcRenderTarget;
};

class CTextLayout;

//////////////////////////////////////////////////////////////////////////
// Helper class to simplify handling effects setup as timecycle modifiers that animate over time
enum eAnimatedPostFXMode
{
	POSTFX_IN_HOLD_OUT = 0,
	POSTFX_EASE_IN_HOLD_EASE_OUT,
	POSTFX_EASE_IN,
	POSTFX_EASE_OUT,
};

enum eAnimatedPostFXLoopMode
{
	POSTFX_LOOP_ALL = 0,
	POSTFX_LOOP_HOLD_ONLY,
	POSTFX_LOOP_NONE,
};

class AnimatedPostFX
{

public:

	enum eState 
	{
		POSTFX_IDLE = 0,
		POSTFX_DELAY_IN,
		POSTFX_GOING_IN,
		POSTFX_ON_HOLD,
		POSTFX_GOING_OUT,
	};

	AnimatedPostFX() { Init(); }

	void Set(atHashString modifierName, eAnimatedPostFXMode animMode, u32 inDelayDuration, u32 inDuration, u32 holdDuration, u32 outDuration);
	void Start();
	void Start(u32 duration);
	void Update();
	void Init();
	void Reset();
	void Disable() { m_Disabled = true; }
	void Enable() { m_Disabled = false; }
	void SetLoopEnabled(bool bEnabled) { m_Loop = bEnabled; }
	void SetCanBePaused(bool bCanBePaused) { m_CanBePaused = bCanBePaused; }
	bool IsDisabled() const { return m_Disabled; }
	bool IsIdle() const { return (m_StartTime == 0U); }
	float GetFadeLevel() const { return m_FadeLevel; }
	atHashString GetModifierName() const { return m_ModifierName; }

	void ApplyModifiers(tcKeyframeUncompressed& keyframe, float globalFadeLevel) const;

	eState GetState() const { return m_State; }

	u32 GetDuration() const { return (m_StartDelayDuration+m_InDuration+m_HoldDuration+m_OutDuration); }
	u32 GetCurrentTime() const { return m_CurrentTime; }

private:

	bool NeedsEaseIn() const { return m_AnimMode > POSTFX_IN_HOLD_OUT && m_AnimMode < POSTFX_EASE_OUT; }
	bool NeedsEaseOut() const { return (m_AnimMode == POSTFX_EASE_IN_HOLD_EASE_OUT || m_AnimMode == POSTFX_EASE_OUT); }

	void Update_InHoldOut();
	void Update_EaseInOrOut();

	atHashString			m_ModifierName;

	u32						m_StartDelayDuration;
	u32   					m_InDuration;
	u32   					m_HoldDuration;
	u32   					m_OutDuration;
	u32						m_CurrentTime;

	float 					m_FadeLevel;
	float					m_Scale;
	u32   					m_StartTime;

	eAnimatedPostFXMode		m_AnimMode;
	eState					m_State;

	eAnimatedPostFXLoopMode	m_LoopMode;
	bool					m_Disabled;
	bool					m_Loop;
	bool					m_CanBePaused;

	PAR_SIMPLE_PARSABLE;

	friend class AnimatedPostFXStack;
};


class LayerBlendModifier
{

public:

	LayerBlendModifier() { Init(); }

	void Init();
	void Reset();

	void Disable() { m_Disabled = true; }
	void Enable() { m_Disabled = false; }
	bool IsEnabled() const { return m_Disabled == false; }

	void ApplyModifiers(tcKeyframeUncompressed& keyframe, float globalFadeLevel) const;

	void Set(const AnimatedPostFX* pLayerA, const AnimatedPostFX* pLayerB);
	void Update();

private:


	const AnimatedPostFX* m_pLayerA;
	const AnimatedPostFX* m_pLayerB;

	float			m_FrequencyNoise;
	float			m_AmplitudeNoise;
	float			m_Frequency;
	float			m_Bias;
	float			m_Level;
	atHashString	m_LayerA;
	atHashString	m_LayerB;
	bool			m_Disabled;

	PAR_SIMPLE_PARSABLE;

	friend class AnimatedPostFXStack;
};

//////////////////////////////////////////////////////////////////////////
// Animpostfx events
enum AnimPostFXEventType
{
	ANIMPOSTFX_EVENT_INVALID = -1,
	ANIMPOSTFX_EVENT_ON_START = 0,
	ANIMPOSTFX_EVENT_ON_STOP,
	ANIMPOSTFX_EVENT_ON_FRAME,
	ANIMPOSTFX_EVENT_ON_START_ON_STOP,
	ANIMPOSTFX_EVENT_ON_START_ON_STOP_ON_FRAME,
	ANIMPOSTFX_EVENT_ON_START_ON_FRAME,
	ANIMPOSTFX_EVENT_ON_STOP_ON_FRAME,
};

//////////////////////////////////////////////////////////////////////////
// Helper class to simplify handling effects setup as timecycle modifiers that animate over time
class AnimatedPostFXStack
{
public:

	enum { kNumEffectLayers = 6 };

	AnimatedPostFXStack() { m_UserFadeOverrideEnabled = false; m_UserFadeOverrideLevel = 1.0f; m_Layers.Reset(); m_GroupId = -1; m_EventTimeMS = 0U; m_EventType = ANIMPOSTFX_EVENT_INVALID; m_EventUserTag = 0U; }

	void Add(atHashString modifierName, eAnimatedPostFXMode animMode, u32 inDelayDuration, u32 inDuration, u32 holdDuration, u32 outDuration);
	void Set(u32 layer, atHashString modifierName, eAnimatedPostFXMode animMode, u32 inDelayDuration, u32 inDuration, u32 holdDuration, u32 outDuration);

	void SetLoopEnabled(bool bEnable);
	void SetUserFadeOverride(bool bEnable);
	void SetUserFadeLevel(float level);
	float GetUserFadeLevel() const { return m_UserFadeOverrideLevel; }

	void Start();
	void Start(u32 duration);
	void Stop() { m_UserFadeOverrideEnabled = false; m_UserFadeOverrideLevel = 1.0f; Reset(); };
	void Update();
	void Reset();

	s32 GetGroupId() const { return m_GroupId; }
	u32 GetDuration() const;

	u32 GetEventTimeMS() const { return m_EventTimeMS; }
	AnimPostFXEventType GetEventType() const { return m_EventType; }
	u32 GetEventUserTag() const { return m_EventUserTag; }

	bool IsIdle() const;
	float GetCurrentTime() const;
	u32 GetCurrentTimeMS() const;

	const AnimatedPostFX& GetLayer(u32 idx) const { return m_Layers[idx]; }
	u32 GetCount() const { return (u32)m_Layers.GetCount(); }

	void ApplyModifiers(tcKeyframeUncompressed& keyframe) const;

	void PostLoadCallback();

#if __BANK
	void AddWidgets(rage::bkBank& bank);
#endif 

private:

	LayerBlendModifier	m_LayerBlend;
	atFixedArray<AnimatedPostFX, kNumEffectLayers> m_Layers;
	bool m_UserFadeOverrideEnabled;
	float m_UserFadeOverrideLevel;
	AnimPostFXEventType m_EventType;
	u32 m_EventUserTag;
	u32 m_EventTimeMS;
	s32 m_GroupId;

	friend class RegisteredAnimPostFXStack;

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
//
class RegisteredAnimPostFXStack
{
public:

	RegisteredAnimPostFXStack() { Init(); }

	void Init();

	bool operator >(const RegisteredAnimPostFXStack& stackB) const { return m_Priority > stackB.m_Priority; }
	bool operator >=(const RegisteredAnimPostFXStack& stackB) const	{ return m_Priority >= stackB.m_Priority; }

	void Start();
	void Start(u32 duration);
	void Stop() { m_StopRequested = true; };
	void CancelStartRequest();
	void Reset();
	void Update();
	void FrameDelayUpdate();
	float GetCurrentTime() const;

	void SetLoopEnabled(bool bEnable) { m_FXStack.SetLoopEnabled(bEnable); };
	void SetUserFadeOverride(bool bEnable) { m_FXStack.SetUserFadeOverride(bEnable); };
	void SetUserFadeLevel(float level) { m_FXStack.SetUserFadeLevel(level); };
	void SetFrameDelay(unsigned int frameDelay) { m_FrameDelay = frameDelay; }

	u32 GetDuration() const { return m_FXStack.GetDuration(); }
	u32 GetFrameDelay() const { return m_FrameDelay; }

	s32 GetGroupId() const { return m_FXStack.GetGroupId(); }
	u32 GetEventTimeMS() const { return m_FXStack.GetEventTimeMS(); }
	u32 GetCurrentTimeMS() const { return m_FXStack.GetCurrentTimeMS(); }
	AnimPostFXEventType GetEventType() const { return m_FXStack.GetEventType(); }
	u32 GetEventUserTag() const { return m_FXStack.GetEventUserTag(); }
	bool DoesEventTypeMatch(AnimPostFXEventType eventType) const;

	bool HasFrameEventTriggered() const { return m_FrameEventTriggered; }
	void MarkFrameEventTriggered(bool bTriggered) { m_FrameEventTriggered = bTriggered; }

#if __BANK
	void ClearForEdit();
	bool Validate();
#endif 


	atHashString GetName() const { return m_Name; }

	u32 GetCount() const { return (u32)m_FXStack.GetCount(); }
	bool IsIdle() const { return m_FXStack.IsIdle(); }
	bool IsStartPending() const { return m_StartRequested; }
	bool IsStopPending() const { return m_StopRequested; }

	float GetUserFadeLevel() const { return m_FXStack.GetUserFadeLevel(); }

	const AnimatedPostFX& GetLayer(u32 idx) const { return m_FXStack.GetLayer(idx); }

	AnimatedPostFXStack* GetStack() {  return &m_FXStack; }
	const AnimatedPostFXStack* GetStack() const {  return &m_FXStack; }

	void ApplyModifiers(tcKeyframeUncompressed& keyframe);

private:

	atHashString		m_Name;
	AnimatedPostFXStack m_FXStack;
	u32					m_OverriddenDuration;
	u32					m_FrameDelay;
	u8					m_Priority;
	bool				m_StartRequested;
	bool				m_StopRequested;
	bool				m_FrameEventTriggered;

	PAR_SIMPLE_PARSABLE;

	friend class AnimPostFXManager;
};

#if GTA_REPLAY
//////////////////////////////////////////////////////////////////////////
//

class AnimPostFXManager;

class AnimPostFXManagerReplayEffectLookup
{
public:

	static void	Init(AnimPostFXManager *pFxManager);
	static void	Shutdown();

	static bool AreAllGameCamStacksIdle(bool forThumbnail, int* activeIndex = nullptr);

	static bool IsStackDisabledForGameCam(const RegisteredAnimPostFXStack* pStack);
	static bool IsStackDisabledForFreeCam(const RegisteredAnimPostFXStack* pStack);

#if !__FINAL
	static const char* GetFilterName(int index);
#endif // !__FINAL
private:
	// Clear out all pointers
	static void	Reset();			

	// Name + Cached pointer to stack for each table entry
	struct AnimPostFXReplayEffectTableEntry
	{
		atHashString				m_Name;
		RegisteredAnimPostFXStack	*m_pStack;
		bool						m_AllowForThumbnail;
	};
	// Storage for the table
	static AnimPostFXReplayEffectTableEntry	m_PostEffectReplayGameCamRemovalLookupTable[];
	static AnimPostFXReplayEffectTableEntry	m_PostEffectReplayFreeCamRemovalLookupTable[];
};

#endif	//GTA_REPLAY

struct AnimPostFXEvent
{
	AnimPostFXEvent() { Reset(); }
	~AnimPostFXEvent() { Reset(); }

	void Reset() 
	{
		fxName = 0U;
		groupId = -1;
		eventType = ANIMPOSTFX_EVENT_INVALID;
		currentTime = 0U;
		userTag = 0U;
	}

	bool IsValid() const { return (fxName != 0U && groupId != -1); }

	atHashString		fxName;				// name of the effect stack that triggered the event
	s32					groupId;			// group id (set up in the stack itself)
	AnimPostFXEventType	eventType;			// event type
	u32					currentTime;		// current running time of the effect
	u32					userTag;			// user tag for random flags
};

typedef Functor1<AnimPostFXEvent*> AnimPostFXEventCB;

struct AnimPostFXListener
{
	AnimPostFXListener() { Reset(); }
	~AnimPostFXListener() { Reset(); }

	bool IsValid () const { return (groupId != -1 && eventCallback != 0); }
	bool Matches(AnimPostFXEventCB pCBFunc, s32 _groupId) const { return (eventCallback == pCBFunc && _groupId == groupId); }

	void Reset() 
	{
		groupId = -1;
		eventCallback = NULL;
	}
	s32					groupId;
	AnimPostFXEventCB	eventCallback;
};

//////////////////////////////////////////////////////////////////////////
// Helper struct to store triggered events for polling
struct AnimPostFXPolledEvent
{
	AnimPostFXPolledEvent() { Reset(); }
	~AnimPostFXPolledEvent() { Reset(); }

	bool Matches(atHashString fxName) { return (fxName.IsNotNull() && fxName == srcEvent.fxName); }
	void Reset() 
	{
		srcEvent.Reset();
		timeStamp = 0U;
		bConsumed = false;
	}

	AnimPostFXEvent srcEvent;
	u32				timeStamp;
	bool			bConsumed;
};

//////////////////////////////////////////////////////////////////////////
// Helper class to turn the event-based AnimPostFXEventManager system into
// a polling-based one that's script-friendly
class AnimPostFXEventPollingManager
{
public:

	AnimPostFXEventPollingManager() {};
	~AnimPostFXEventPollingManager() {};

	void Init();
	void Shutdown();

	bool AddListener(const atHashString fxName);
	void RemoveListener(const atHashString fxName);

	bool HasEventTriggered(atHashString fxName, AnimPostFXEventType eventType, bool bPeekOnly, bool& bIsRegistered);

	void OnEventTriggered(AnimPostFXEvent* pEvent);
	void Update();

#if __BANK
	void AddWidgets(rage::bkBank& bank);
	void OnRegisterEvent();
	void OnUnregisterEvent();
#endif

private:

	bool AddListener(s32 groupId);
	void RemoveListener(s32 groupId);

	fwLinkList<AnimPostFXPolledEvent>		m_events;				// list of events received so they can be polled
	typedef fwLink<AnimPostFXPolledEvent>	PolledEventNode;

	atMap<s32, s32>							m_managedEventGroups;	// table with registered event groups, so that script doesn't have to know about groups 
																	// and can instead focus on individual effects when registering a listener.

	static const s32						MAX_EVENTS;
	static u32								EVENT_POLLING_TIMEOUT;

#if __BANK
	static char								ms_registeredFxName[128];
#endif
};

//////////////////////////////////////////////////////////////////////////
//
class AnimPostFXEventManager
{

public:

	AnimPostFXEventManager() {};
	~AnimPostFXEventManager() {};

	void Init();
	void Shutdown();

	bool RegisterListener(AnimPostFXEventCB pCBFunc, s32 groupId);
	void RemoveListener(AnimPostFXEventCB pCBFunc, s32 groupId);

	void OnEventTriggered(const RegisteredAnimPostFXStack* pStack, AnimPostFXEventType eventType);

	static s32 GetMaxListeners() { return MAX_LISTENERS; }

private:

	bool Exists(AnimPostFXEventCB pCBFunc, s32 groupId) const;

	fwLinkList<AnimPostFXListener>		m_listenerList;
	typedef fwLink<AnimPostFXListener>	ListenerNode;

	static const s32 MAX_LISTENERS;
};

//////////////////////////////////////////////////////////////////////////
//
class AnimPostFXManager
{
public:
	
	enum eAnimPostFXUser
	{
		kDefault = 0,
		kPedKill,
		kCameraFlash,
		kSelectionWheel,
		kLongPlayerSwitch,
		kShortPlayerSwitch,
		kKERSBoost,
		kFirstPersonProp,
		kScript,
		kSpecialAbility,
		kCount
	};

	AnimPostFXManager() {}
	~AnimPostFXManager() {}

	void Start(atHashString effectName, u32 duration, bool bLoop, bool bUserControlsFade, bool bStartOnce, int frameDelay, eAnimPostFXUser user);

	void CancelStartRequest(atHashString effectName, eAnimPostFXUser user);
	void StartCrossfade(atHashString effectNameIn, atHashString effectNameOut, u32 duration, eAnimPostFXUser user);
	void StopCrossfade(eAnimPostFXUser user);

	void SetUserFadeLevel(atHashString effectName, float level);

	void StopAll(eAnimPostFXUser user);

	void Stop(atHashString effectName, eAnimPostFXUser user);
	bool IsRunning(atHashString effectName) const;
	bool IsStartPending(atHashString effectName) const;
	float GetCurrentTime(atHashString effectName) const;

	bool IsIdle() const { return (m_ActiveStacks.GetNumUsed() == 0) && (m_FrameDelayStacks.GetNumUsed() == 0); }

	void Init();
	void Shutdown();

	void Update();
	void ApplyModifiers(tcKeyframeUncompressed& keyframe REPLAY_ONLY(, eTimeCyclePhase currentPhase));

	void Reset();
	void Add(RegisteredAnimPostFXStack& stack);
	void Remove(atHashString name);
	const RegisteredAnimPostFXStack* Get(atHashString name) const;
	RegisteredAnimPostFXStack* Get(atHashString name);

	// Event-based system for tagged FX stacks
	bool RegisterListener(AnimPostFXEventCB pCBFunc, s32 groupId) { return m_eventManager.RegisterListener(pCBFunc, groupId); };
	void RemoveListener(AnimPostFXEventCB pCBFunc, s32 groupId) { m_eventManager.RemoveListener(pCBFunc, groupId); };
	void OnEventTriggered(const RegisteredAnimPostFXStack* pStack, AnimPostFXEventType eventType) { m_eventManager.OnEventTriggered(pStack, eventType); }

	// Polling-based system for tagged FX stacks (script)
	bool AddListener(const atHashString fxName) { return m_polledEventManager.AddListener(fxName); };
	void RemoveListener(const atHashString fxName) { m_polledEventManager.RemoveListener(fxName); };
	bool HasEventTriggered(atHashString fxName, AnimPostFXEventType eventType, bool bPeekOnly, bool& bIsRegistered) { return m_polledEventManager.HasEventTriggered(fxName, eventType, bPeekOnly, bIsRegistered); }

	static void	ClassInit();
	static void	ClassShutdown();
	static AnimPostFXManager&	GetInstance() { return *smp_Instance; }

#if __BANK
	void AddWidgets(rage::bkBank& bank );
	bool DebugOutput() const { return m_bDebugOutput; };
	bool DebugDisplay() const { return m_bDebugDisplay; };
	bool AllowPausing() const { return m_bDebugAllowPausing; }
#endif

#if GTA_REPLAY
	bool	AreReplayRecordingRestrictedStacksAtIdle(bool forThumbnail, int* activeIndex)
	{
		return AnimPostFXManagerReplayEffectLookup::AreAllGameCamStacksIdle(forThumbnail, activeIndex);
	}
#endif	//GTA_REPLAY

private:

	bool AddToActiveList(RegisteredAnimPostFXStack* pStack);
	bool AddToFrameDelayList(RegisteredAnimPostFXStack* pStack);

	bool LoadMetaData();
#if __BANK && !__FINAL
	bool SaveMetaData();
#endif

	atArray<RegisteredAnimPostFXStack>			m_RegisteredStacks;

	fwLinkList<RegisteredAnimPostFXStack*>		m_ActiveStacks;
	fwLinkList<RegisteredAnimPostFXStack*>		m_FrameDelayStacks;
	
	typedef fwLink<RegisteredAnimPostFXStack*>	ActiveStackNode;


	struct CrossfadeHelper
	{
		CrossfadeHelper() { Reset(); }
		void Reset()
		{
			m_InStackName.Null();
			m_OutStackName.Null();
			m_pInStack = NULL;
			m_pOutStack = NULL;
			m_StartRequested = false;
			m_StopRequested = false;
			m_Duration = 0U;
			m_StartTime = 0U;
			m_FadeLevel = 0.0f;
			m_Active = false;
			m_State = AnimatedPostFX::POSTFX_IDLE;
		}

		void Start(atHashString effectNameIn, atHashString effectNameOut, u32 duration);
		void Stop();
		bool IsActive() const { return m_Active; }

		void Update();
		void UpdateTiming();

		atHashString				m_InStackName;
		atHashString				m_OutStackName;
		u32							m_Duration;
		u32							m_StartTime;
		float						m_FadeLevel;
		RegisteredAnimPostFXStack*	m_pInStack;
		RegisteredAnimPostFXStack*	m_pOutStack;
		AnimatedPostFX::eState		m_State;
		bool						m_StartRequested;
		bool						m_StopRequested;
		bool						m_Active;
	};

	CrossfadeHelper					m_Crossfade;
	bool							m_bStopAllRequested;

#if __BANK
	bool							m_bBlockRequestFromUser;
	eAnimPostFXUser					m_blockedUserType;
#endif // __BANK
	AnimPostFXEventManager			m_eventManager;
	AnimPostFXEventPollingManager	m_polledEventManager;

	static AnimPostFXManager*					smp_Instance;

#if __BANK
	class EditTool
	{
	public:

		EditTool() {}

		void Init();
		void Shutdown();

		void AddWidgets(rage::bkBank& bank );

		void OnSaveCurrentFXStack();
		void OnDeleteCurrentFXStack();
		void OnSaveData();
		void OnLoadData();
		void OnFXStackNameSelected();
		void OnStopTestStackFX0() { OnStopTestStackFX(0); }
		void OnStartTestStackFX0() { OnStartTestStackFX(0); }
		void OnStopTestStackFX1() { OnStopTestStackFX(1); }
		void OnStartTestStackFX1() { OnStartTestStackFX(1); }
		void OnStopTestStackFX2() { OnStopTestStackFX(2); }
		void OnStartTestStackFX2() { OnStartTestStackFX(2); }
		void OnStopTestStackFX3() { OnStopTestStackFX(3); }
		void OnStartTestStackFX3() { OnStartTestStackFX(3); }
		void OnStopTestStackFX4() { OnStopTestStackFX(4); }
		void OnStartTestStackFX4() { OnStartTestStackFX(4); }
		void OnStopTestStackFX5() { OnStopTestStackFX(5); }
		void OnStartTestStackFX5() { OnStartTestStackFX(5); }
		void OnStopTestStackFXAll() { OnStopTestStackFX(0);OnStopTestStackFX(1);OnStopTestStackFX(2);OnStopTestStackFX(3);OnStopTestStackFX(4);OnStopTestStackFX(5); }
		void OnStartTestStackFXAll() { OnStartTestStackFX(0);OnStartTestStackFX(1);OnStartTestStackFX(2);OnStartTestStackFX(3);OnStartTestStackFX(4);OnStartTestStackFX(5); }

		void OnStartCrossfade();
		void OnStopCrossfade();


		void OnStopTestStackFX(int fxTestIndex);
		void OnStartTestStackFX(int fxTestIndex);

		char* GetSavePath() { return &ms_savePath[0]; }

	private:

		bool ValidateCurrentFXStack() const;
		void UpdateRegStackNamesList(const atArray<RegisteredAnimPostFXStack>& stacks);

		atArray<const char*>			m_RegStackNames;
		int								m_RegStackNamesComboIdx;
		bkCombo*						m_pRegStackNamesCombo;
		RegisteredAnimPostFXStack		m_editableStack;

		u32								m_testStackDuration[6];
		bool							m_bTestLoopEnabled[6];

		static atHashString				ms_newStackName;
		static char						ms_testStackName[6][32];

		static char						ms_testCrossfadeName[2][32];
		u32								m_testCrossfadeDuration;

		static char						ms_savePath[256];
	};

	EditTool m_EditTool;
	bool m_bDebugOutput;
	bool m_bDebugDisplay;
	bool m_bDebugAllowPausing;
	int	m_userTypeComboIdx;
	bkCombo* m_pUserTypeCombo;

	static const char*					ms_debugUserTypeStr[]; 

	void OnUserTypeToBlockSelected();
#endif // __BANK

	PAR_SIMPLE_PARSABLE;
};
#define ANIMPOSTFXMGR AnimPostFXManager::GetInstance()

//////////////////////////////////////////////////////////////////////////
//
class PauseMenuPostFXManager
{

public:

	enum eState 
	{
		PMPOSTFX_IDLE = 0,
		PMPOSTFX_FADING_IN,
		PMPOSTFX_FADING_OUT,
	};
	
	PauseMenuPostFXManager() {}
	~PauseMenuPostFXManager() {}

	void Init();
	void Shutdown();

	void StartFadeIn();
	void StartFadeOut();

	void Update();
	void ApplyModifiers(tcKeyframeUncompressed& keyframe);

	void Reset();

	bool IsIdle() const { return (m_State == PMPOSTFX_IDLE); }

	// Returns true if the pause menu effects are fading, *having into account* the bias values m_FadeInCheckTreshold and m_FadeOutCheckTreshold.
	// This allows markers, etc., to resume rendering slightly earlier, before the effects are fully faded out.
	bool IsFading() const; 

	eState GetState() const { return m_State; }

	static void	ClassInit();
	static void	ClassShutdown();
	static PauseMenuPostFXManager&	GetInstance() { return *smp_Instance; }

#if __BANK
	void AddWidgets(rage::bkBank& bank );
#endif

#if GTA_REPLAY
	bool AreAllPostFXAtIdle();
#endif	//GTA_REPLAY

private:

	bool LoadMetaData();
#if __BANK && !__FINAL
	bool SaveMetaData();
#endif
	
	void UpdateCurrentEffect();

	enum eEffectType
	{
		PAUSEFX_FRANKLIN = 0,
		PAUSEFX_MICHAEL,
		PAUSEFX_TREVOR,
		PAUSEFX_NEUTRAL,
		//PAUSEFX_CNC_COP,
		//PAUSEFX_CNC_CROOK,
		PAUSEFX_COUNT
	};

	RegisteredAnimPostFXStack*			m_pFadeInEffects[PAUSEFX_COUNT];
	RegisteredAnimPostFXStack*			m_pFadeOutEffects[PAUSEFX_COUNT];
	RegisteredAnimPostFXStack			m_FallbackEffect;

	AnimatedPostFXStack*				m_pCurFadeInFX;
	AnimatedPostFXStack*				m_pCurFadeOutFX;

	eState								m_State;
	float								m_FadeInCheckTreshold;
	float								m_FadeOutCheckTreshold;
	bool								m_FadeInRequested;
	bool								m_FadeOutRequested;

	static PauseMenuPostFXManager*		smp_Instance;

#if __BANK
	class EditTool
	{
	public:

		EditTool() {}

		void Init();
		void Shutdown();

		void AddWidgets(rage::bkBank& bank );

		void OnTestFadeInEffects();
		void OnTestFadeOutEffects();
		void OnSaveData();
		void OnLoadData();


	};

	EditTool m_EditTool;
#endif // __BANK

	PAR_SIMPLE_PARSABLE;

};

#define PAUSEMENUPOSTFXMGR PauseMenuPostFXManager::GetInstance()

//////////////////////////////////////////////////////////////////////////
// 
class OverlayTextRenderer
{
public: // methods
	static void InitializeTextBlendState();

	static void RenderText( char const * const text, Vector2 const& position, Vector2 const& scale, 
		CRGBA const& color, s32 const style, Vector2 const& wrap, Vector2 const& c_maskTL = Vector2(0,0), Vector2 const& c_maskBR = Vector2(0,0));

	static void RenderText( char const * const text, Vector2 const& position, CTextLayout& textLayout );

protected:
	inline static grcBlendStateHandle GetTextBlendState() { return sm_textBlendState; }

private: // declarations and variables
	static grcBlendStateHandle	sm_textBlendState;
};

//////////////////////////////////////////////////////////////////////////
//
class PhonePhotoEditor : public OverlayTextRenderer
{
public:
	static const int PHOTO_TEXT_MAX_LENGTH = 32;
	typedef Functor3<u8*, u32, bool> OnPhotoEditorSaveCallback;

	PhonePhotoEditor() { Reset(); }
	~PhonePhotoEditor() {}

	// API for text editing //////////////////////////////////////////////////

	// Use to check whether editing is available.
	bool IsEditingAvailable() const;

	// Will fail in either of these cases:
	//	1. Nested call within a Start/Finish bracket
	//	2. Rendering is not frozen
	//	3. there's no memory for JPEG buffer
	//	4. the TXD is not available
	bool StartEditing(s32 txdId);
		
	inline bool IsEditing() const { return m_galleryEdit.m_bEditing; }
	inline bool IsWaitingUserInput() const { return m_galleryEdit.m_bWaitingForUser; }
	inline bool IsRequestingSave() const { return m_galleryEdit.m_requestFlags.bSave; }

	// Only call to finish an edit previously initiated with StartEditing. Not synchronous: release of resources is deferred.
	bool FinishEditing();				

	// Not synchronous: user will get a callback with the JPEG buffer, its size and whether the save operation succeeded.
	bool RequestSave(OnPhotoEditorSaveCallback pCallback); //

	//////////////////////////////////////////////////////////////////////////

	void UpdateSafe();

	void Init();
	void Shutdown();
	void Reset();

	void Update();
	void Render();

	void Enable() { m_bActive = true; }
	void Disable();
	bool IsActive() const { return m_bActive; }

	inline void SetDestTarget(grcRenderTarget* pRT) { m_pDestRt = pRT; }
	void SetBorderTexture(const char* txdName, bool bDisableCurrentFrame);

	void SetText(const char* pTopText, const char* pBottomText, const Vector4& textPos, float topTextSize, float bottomTextSize, s32 topTextStyle, s32 bottomTextStyle, Color32 topTextCol, Color32 bottomTextCol);
	inline void SetText(const char* pTopText, const char* pBottomText )
	{
		SetText( pTopText, pBottomText, m_textPos, m_topTextSize, m_bottomTextSize, m_topTextStyle, m_bottomTextStyle, m_topTextCol, m_bottomTextCol );
	}

	inline const char * GetTopText() const { return m_topText; }
	inline s32 GetTopTextStyle() const { return m_topTextStyle; }
	inline float GetTopTextSize() const { return m_topTextSize; }
	inline Color32 const & GetTopColour() const { return m_topTextCol; }

	inline const char * GetBottomText() const { return m_bottomText; }
	inline s32 GetBottomTextStyle() const { return m_bottomTextStyle; }
	inline float GetBottomTextSize() const { return m_bottomTextSize; }
	inline Color32 const & GetBottomColour() const { return m_bottomTextCol; }

	inline Vector4 const & GetTextPosition() const { return m_textPos; }

	void ResetTextParameters();
	inline grcTexture * GetTargetTexture() { return m_galleryEdit.m_pRt; }

#if __BANK
	void AddWidgets(rage::bkBank& bank );
	void RenderDebug();
#endif

	static void ClassInit();
	static void ClassShutdown();

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif // RSG_PC

	static PhonePhotoEditor& GetInstance() { return *smp_Instance; }
	
	void RenderBorder(bool bAdjustFor43 = false);
	void RenderGalleryEdit();

private:

	void InitGalleryEdit(bool bReInit);
	void ShutdownGalleryEdit();

	void UpdateGalleryEdit();


	void ReleaseCurrentBorderTextures();
	void RenderText( int const destWidth, int const destHeight );
	void RenderTextOverlay();

	void ResetGalleryEditState();
	
	bool AllocateGalleryEditJPEGBuffer();
	void ReleaseGalleryEditJPEGBuffer();
	bool ProcessGalleryEditSourceImage(s32 txdId);
	void ReleaseGalleryEditSourceImage();
	bool SaveToJPEGBuffer();

	grcRenderTarget*	m_pDestRt;
	grcTextureHandle	m_pBorderTexture[5];

	char				m_topText[PHOTO_TEXT_MAX_LENGTH];
	char				m_bottomText[PHOTO_TEXT_MAX_LENGTH];

	atFinalHashString	m_borderTxdName;			
	s32					m_borderTextureTxd;
	Color32				m_topTextCol;
	Color32				m_bottomTextCol;
	Vector4				m_textPos;
	s32					m_topTextStyle;
	s32					m_bottomTextStyle;
	float				m_topTextSize;
	float				m_bottomTextSize;

	bool				m_bBorderTextureRefAdded;
	bool				m_bActive;
	bool				m_bUpdateBorderTexture;
	bool				m_bReleaseCurrentBorderTexture;
	bool				m_bUseText;


	struct GalleryEditData
	{
		GalleryEditData() { Reset(); }
		~GalleryEditData() {}
		void Reset();

		OnPhotoEditorSaveCallback m_pOnSaveFnc;

		grcViewport				m_viewport;
		grcRenderTarget*		m_pRt;
		grcTextureHandle		m_pSrcImg;
		u8*						m_pJPEGBuffer;
		u32						m_JPEGBufferSize;
		u32						m_JPEGActualSize;
		s32						m_SrcImgTxd;
		s32						m_PendingSrcImgTxd;
		bool					m_bSrcImgRefAdded;

		union 
		{
			struct
			{
				u8 bSave : 1;
				u8 bRelease: 1;
			} m_requestFlags;
			bool m_bRequestsPending;
		};

		bool m_bEditing : 1;
		bool m_bWaitingForUser : 1;
		bool m_bNeedToIssueCallback : 1;
		bool m_bWasSaveOk : 1;
	};

	GalleryEditData			m_galleryEdit;

	static PhonePhotoEditor* smp_Instance;

#if __BANK
	void				AddWidgetsForGalleryEdit(rage::bkBank& bank );
	void				OnToggleActive();
	void				OnTextChange();
	static char			sm_borderTxdName[PHOTO_TEXT_MAX_LENGTH*2];
	static char			sm_topText[PHOTO_TEXT_MAX_LENGTH];
	static char			sm_bottomText[PHOTO_TEXT_MAX_LENGTH];
	static Color32		sm_topTextCol;
	static Color32		sm_bottomTextCol;
	static Vector4		sm_textPos;
	static float		sm_topTextSize;
	static float		sm_bottomTextSize;
	static bool			sm_bToggleActive;
	static s32			sm_topTextStyle;
	static s32			sm_bottomTextStyle;
	static const char*	sm_fontTypesStr[];

	void				OnToggleGalleryEdit();
	void				OnGalleryEditSave();
	void				OnGalleryEditTextChange();
	void				OnGalleryEditTextureNameChange();

	static Vector2		sm_debugRenderPos;
	static Vector2		sm_debugRenderSize;

	static s32			sm_galleryEditTxdId;
	static bool			sm_bToggleGalleryEdit;
	static bool			sm_bDebugRender;
	static bool			sm_bTestSave;
	static char			sm_photoToEditName[64];
#endif 

};

#define PHONEPHOTOEDITOR PhonePhotoEditor::GetInstance()

//////////////////////////////////////////////////////////////////////////
//
struct DofTODOverrideEntry
{
	DofTODOverrideEntry() : hourFrom(0.0f), hourTo(0.0f), overrideValue(0.0f) {};
	void Reset() { hourFrom = 0.0f; hourTo = 0.0f; overrideValue = 0.0f; };

	float hourFrom;
	float hourTo;
	float overrideValue;

};

//////////////////////////////////////////////////////////////////////////
//
class DofTODOverrideHelper
{
public:

	DofTODOverrideHelper() { Reset(); };

	bool IsActive()  const { return m_bActive; }

	bool Begin();
	bool End();

	void Reset();

	void AddSample(float hourFrom, float hourTo, float overrideValue);
	void SetDefaultOutOfRangeOverride(float defValue) { m_outOfRangeOverrideValue = defValue; }

	void Update();

	float GetCurrentValue();

#if __BANK
	void AddWidgets(rage::bkBank& bank);
	void OnToggle();
#endif 

private:

	static const int MAX_TIME_SAMPLES = 4;

	atFixedArray<DofTODOverrideEntry, MAX_TIME_SAMPLES> m_samples;
	float	m_outOfRangeOverrideValue;
	float	m_currentValue;
	bool	m_bActive;

#if __BANK
	static Vector3	sm_debugSamples[MAX_TIME_SAMPLES];
	static float	sm_debugOutOfRangeOverride;
	static bool		sm_bToggle;
#endif

};

#endif // __POSTPROCESSFX_HELPER_H__
