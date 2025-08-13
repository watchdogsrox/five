#include "PostProcessFXHelper.h"

#include "atl/string.h"
#include "camera/viewports/ViewportManager.h"
#include "game/Clock.h"
#include "math/math.h"
#include "Network/NetworkInterface.h"
#include "file/default_paths.h"
#include "frontend/HudTools.h"
#include "frontend/NewHud.h"
#include "fwsys/timer.h"
#include "parser/manager.h"
#include "PostProcessFXHelper_parser.h"
#include "peds/ped.h"
#include "peds/PlayerArcadeInformation.h"
#include "renderer/PostProcessFX.h"
#include "renderer/rendertargets.h"
#include "renderer/ScreenshotManager.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene/world/GameWorld.h"

#include "timecycle/timecycle.h"

#include "fwsys/fileexts.h"
#include "grcore/debugdraw.h"
#include "grcore/quads.h"
#include "system/param.h"

#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "Text/TextConversion.h"
#include "Text/TextFormat.h"

PARAM(animpostfx_debug, "animpostfx_debug");
PARAM(animpostfx_save_path, "animpostfx_save_path");
PARAM(animpostfx_enable_metadata_preview, "When enabled, the AnimPostFX Manager will try to load from the export file instead of the platform data file.");


#if __BANK
//////////////////////////////////////////////////////////////////////////
//
static inline int RemapForBlockCompare(int user)
{
	switch(user)
	{
		//case kDefault:
		case AnimPostFXManager::kPedKill:
		case AnimPostFXManager::kCameraFlash:
		case AnimPostFXManager::kSelectionWheel:
		case AnimPostFXManager::kLongPlayerSwitch:
		case AnimPostFXManager::kShortPlayerSwitch:
		case AnimPostFXManager::kKERSBoost:
		case AnimPostFXManager::kFirstPersonProp:
			return AnimPostFXManager::kDefault;
			break;
		default:
			return user;
		//case kScript,
		//case kSpecialAbility,
	}
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Set(atHashString modifierName, eAnimatedPostFXMode animMode, u32 inDelayDuration, u32 inDuration, u32 holdDuration, u32 outDuration)
{
	m_ModifierName = modifierName;
	m_InDuration = inDuration;
	m_StartDelayDuration = inDelayDuration;
	m_HoldDuration = holdDuration;
	m_OutDuration = outDuration;
	m_AnimMode = animMode;

	m_StartTime = 0U;
	m_FadeLevel = 0.0f;
	m_State = POSTFX_IDLE;
	m_Scale = 1.0f;
	m_Loop = false;
	m_LoopMode = POSTFX_LOOP_NONE;
}

void AnimatedPostFX::Init()
{
	m_State = POSTFX_IDLE; 
	m_FadeLevel = 0.0f;
	m_StartTime = 0U;
	m_StartDelayDuration = 0U;
	m_InDuration = 0U;
	m_HoldDuration = 0U;
	m_OutDuration = 0U;
	m_ModifierName = 0U;
	m_Disabled = false;
	m_AnimMode = POSTFX_IN_HOLD_OUT;
	m_Loop = false;
	m_LoopMode = POSTFX_LOOP_NONE;
	m_Scale = 1.0f;
	m_CurrentTime = 0U;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Reset()
{
	m_State = POSTFX_IDLE;
	m_FadeLevel = 0.0f;
	m_StartTime = 0U;
	m_Scale = 1.0f;
	m_CurrentTime = 0U;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Start()
{
	m_State = POSTFX_IDLE;
	u32 currentDuration = (m_StartDelayDuration+m_InDuration+m_HoldDuration+m_OutDuration);

	if (m_ModifierName.IsNull() || currentDuration == 0U)
	{
		m_Scale = 1.0f;
		return;
	}

	m_StartTime = (m_CanBePaused BANK_ONLY(|| ANIMPOSTFXMGR.AllowPausing())) ? fwTimer::GetTimeInMilliseconds_NonScaledClipped() : fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
	m_FadeLevel = 0.0f;
	m_CurrentTime = 0U;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Start(u32 duration)
{
	if (duration == 0U)
	{
		Start();
		return;
	}

	if (m_ModifierName.IsNull())
	{
		m_Scale = 1.0f;
		return;
	}

	float currentDuration = (float)(m_StartDelayDuration+m_InDuration+m_HoldDuration+m_OutDuration)+0.01f;
	m_Scale = (float)duration/currentDuration;

	Start();
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Update()
{
	// do nothing if we're not active
	if (m_StartTime == 0U || m_Disabled)
	{
		return;
	}

	switch (m_AnimMode)
	{

	case POSTFX_IN_HOLD_OUT:
	case POSTFX_EASE_IN_HOLD_EASE_OUT:
			Update_InHoldOut();
	break;

	case POSTFX_EASE_IN:
	case POSTFX_EASE_OUT:
			Update_EaseInOrOut();
	break;

	default:
		{
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Update_InHoldOut()
{
	float fInterp = 0.0f;

	const bool bEaseIn	= NeedsEaseIn();
	const bool bEaseOut = NeedsEaseOut();

	u32 c_uCurrentTime = (m_CanBePaused	BANK_ONLY(|| ANIMPOSTFXMGR.AllowPausing())) ? fwTimer::GetTimeInMilliseconds_NonScaledClipped() : fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();

	// force effect to loop on its hold state
	const bool bLoopDuringHoldState = (m_State == POSTFX_ON_HOLD && m_Loop && m_LoopMode == POSTFX_LOOP_HOLD_ONLY);

	if (bLoopDuringHoldState)
	{
		m_StartTime = c_uCurrentTime;
	}
	
	m_CurrentTime = c_uCurrentTime-m_StartTime;

	const u32 _inDuration			= (u32)((float)(m_InDuration)*m_Scale);
	const u32 _startDelayDuration	= (u32)((float)(m_StartDelayDuration)*m_Scale);
	const u32 _holdRuration			= (u32)((float)(m_HoldDuration)*m_Scale);
	const u32 _outDuration			= (u32)((float)(m_OutDuration)*m_Scale);

	const u32 delayInDuration		= m_StartTime + _startDelayDuration;
	const u32 goingInDuration		= delayInDuration + _inDuration;
	const u32 onHoldDuration		= goingInDuration + _holdRuration;
	const u32 goingOutDuration		= onHoldDuration + _outDuration;

	if (bLoopDuringHoldState)
	{
		c_uCurrentTime += (_startDelayDuration+_inDuration);
	}

	if (c_uCurrentTime < delayInDuration)
	{
		m_State = POSTFX_DELAY_IN;
	}
	else if (c_uCurrentTime < goingInDuration)
	{
		// Ramping up
		fInterp = (float)(c_uCurrentTime - delayInDuration) / (float)_inDuration;
		fInterp = bEaseIn ? rage::SlowIn(fInterp) : fInterp;
		m_State = POSTFX_GOING_IN;
	}
	else if (c_uCurrentTime < onHoldDuration)
	{
		fInterp = 1.0f;
		m_State = POSTFX_ON_HOLD;
	}
	else if (c_uCurrentTime < goingOutDuration)
	{
		fInterp = (float)(_outDuration - (c_uCurrentTime - onHoldDuration)) / (float)_outDuration;
		fInterp = bEaseOut ? rage::SlowOut(fInterp) : fInterp;
		m_State = POSTFX_GOING_OUT;
	}
	else
	{
		if (m_Loop && m_LoopMode == POSTFX_LOOP_ALL)
		{
			m_StartTime = (m_CanBePaused BANK_ONLY(|| ANIMPOSTFXMGR.AllowPausing())) ? fwTimer::GetTimeInMilliseconds_NonScaledClipped() : fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
		}
		else
		{
			// Done, so disable the effect.
			m_StartTime = 0U;
			m_FadeLevel = 0.0f;
			m_State = POSTFX_IDLE;
		}
	}
	m_FadeLevel = Saturate(fInterp);

}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::Update_EaseInOrOut()
{
	const bool bEaseIn = NeedsEaseIn();

	float fInterp = 1.0f;

	if (m_State != POSTFX_ON_HOLD)
	{
		const bool bEaseIn	= NeedsEaseIn();

		const u32 _startDelayDuration	= (u32)((float)(m_StartDelayDuration)*m_Scale);
		const u32 _inDuration			= (u32)((float)(m_InDuration)*m_Scale);
		const u32 _outDuration			= (u32)((float)(m_OutDuration)*m_Scale);

		const u32 c_uCurrentTime	= (m_CanBePaused BANK_ONLY(|| ANIMPOSTFXMGR.AllowPausing())) ? fwTimer::GetTimeInMilliseconds_NonScaledClipped() : fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
		m_CurrentTime = c_uCurrentTime-m_StartTime;
	
		const u32 delayInDuration	= m_StartTime + _startDelayDuration;
		const u32 goingInDuration	= delayInDuration + (bEaseIn ? _inDuration : _outDuration);

		if (c_uCurrentTime < delayInDuration)
		{
			fInterp = 0.0f;
			m_State = POSTFX_DELAY_IN;
		}
		else if (c_uCurrentTime < goingInDuration)
		{
			// Ramping up
			fInterp = (float)(c_uCurrentTime - delayInDuration) / (float)(bEaseIn ? _inDuration : _outDuration);
			fInterp = bEaseIn ? rage::SlowIn(fInterp) : rage::SlowOut(fInterp);
			m_State = POSTFX_GOING_IN;
		}
		else 
		{
			fInterp = 1.0f;

			if (m_Loop && m_LoopMode == POSTFX_LOOP_ALL)
			{
				m_StartTime = (m_CanBePaused BANK_ONLY(|| ANIMPOSTFXMGR.AllowPausing())) ? fwTimer::GetTimeInMilliseconds_NonScaledClipped() : fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
			}	
			else		
			{
				m_State = POSTFX_ON_HOLD;
			}
	
		}
	}
	// effect on hold after fading out, reset automatically
	else if (bEaseIn == false)
	{
		if (m_Loop && m_LoopMode == POSTFX_LOOP_ALL)
		{
			m_StartTime = (m_CanBePaused BANK_ONLY(|| ANIMPOSTFXMGR.AllowPausing())) ? fwTimer::GetTimeInMilliseconds_NonScaledClipped() : fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
		}
		else
		{
			// Done, so disable the effect.
			m_StartTime = 0U;
			m_FadeLevel = 0.0f;
			m_State = POSTFX_IDLE;
		}
	}

	m_FadeLevel = Saturate(bEaseIn ? fInterp : 1.0f - fInterp);

}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFX::ApplyModifiers(tcKeyframeUncompressed& keyframe, float globalFadeLevel) const
{
	if (IsDisabled() || m_ModifierName.IsNull())
	{
		return;
	}

	const float fadeLevel = GetFadeLevel()*globalFadeLevel;
	atHashString modName = m_ModifierName;
	const tcModifier* pModifier = g_timeCycle.FindModifier(modName, "AnimatedPostFX");
	if (pModifier)
	{
#if	__BANK
		if (AnimPostFXManager::GetInstance().DebugDisplay())
		{
			grcDebugDraw::AddDebugOutput(Color_white, "[ANIMPOSTFX] Applying %s - %f", modName.GetCStr(), fadeLevel);
		}
		if (AnimPostFXManager::GetInstance().DebugOutput())
		{
			Displayf("[ANIMPOSTFX] Applying %s - %f", modName.GetCStr(), fadeLevel);
		}
#endif // __BANK
		pModifier->Apply(keyframe, fadeLevel);
	}
}


//////////////////////////////////////////////////////////////////////////
//
void LayerBlendModifier::Init()
{
	m_LayerA.Null();
	m_LayerB.Null();
	m_Disabled = true;

	m_pLayerA = NULL;
	m_pLayerB = NULL;

	m_FrequencyNoise = 0.0f;
	m_AmplitudeNoise = 0.0f;
	m_Frequency = 0.0f;
	m_Bias = 0.5f;
	m_Level = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
//
void LayerBlendModifier::Reset()
{
	Init();
}

//////////////////////////////////////////////////////////////////////////
//
void LayerBlendModifier::ApplyModifiers(tcKeyframeUncompressed& keyframe, float globalFadeLevel) const
{
	const tcModifier* pModA = g_timeCycle.FindModifier(m_LayerA, "AnimatedPostFX");
	const tcModifier* pModB = g_timeCycle.FindModifier(m_LayerB, "AnimatedPostFX");

	if (pModA == NULL || pModB == NULL)
	{
		return;
	}

#if	__BANK
		if (AnimPostFXManager::GetInstance().DebugDisplay())
		{
			grcDebugDraw::AddDebugOutput(Color_white, "[ANIMPOSTFX] Blending A %s - %f", m_LayerA.GetCStr(), m_Level*m_pLayerA->GetFadeLevel()*globalFadeLevel);
			grcDebugDraw::AddDebugOutput(Color_white, "[ANIMPOSTFX] Blending B %s - %f", m_LayerB.GetCStr(), (1.0f-m_Level)*m_pLayerB->GetFadeLevel()*globalFadeLevel);
		}
		if (AnimPostFXManager::GetInstance().DebugOutput())
		{
			Displayf("[ANIMPOSTFX] Blending A %s - %f", m_LayerA.GetCStr(), m_Level*m_pLayerA->GetFadeLevel()*globalFadeLevel);
			Displayf("[ANIMPOSTFX] Blending B %s - %f", m_LayerB.GetCStr(), (1.0f-m_Level)*m_pLayerB->GetFadeLevel()*globalFadeLevel);
		}
#endif // __BANK

	tcModifier blendModA;
	blendModA.Init(*pModA);
	blendModA.Apply(keyframe, m_Level*m_pLayerA->GetFadeLevel()*globalFadeLevel);

	tcModifier blendModB;
	blendModB.Init(*pModB);
	blendModB.Apply(keyframe, (1.0f-m_Level)*m_pLayerB->GetFadeLevel()*globalFadeLevel);

}

//////////////////////////////////////////////////////////////////////////
//
void LayerBlendModifier::Set(const AnimatedPostFX* pLayerA, const AnimatedPostFX* pLayerB)
{

	m_LayerA = pLayerA->GetModifierName();
	m_LayerB = pLayerB->GetModifierName();

	m_pLayerA = pLayerA;
	m_pLayerB = pLayerB;
}

//////////////////////////////////////////////////////////////////////////
//
void LayerBlendModifier::Update()
{
	if (fwTimer::IsGamePaused())
		return;

	const float timeSecs = static_cast<float>(fwTimer::GetSystemTimeInMilliseconds())*0.001f;

	const float noise = static_cast<float>(rand())/static_cast<float>(RAND_MAX);
	const float ampNoise = Lerp(m_AmplitudeNoise, 1.0f, noise);
	float base = Sinf(timeSecs*m_Frequency + noise*m_FrequencyNoise)*ampNoise;
	base = Saturate(base*0.5f + m_Bias);

	m_Level = base;
}

//////////////////////////////////////////////////////////////////////////
//
const s32 AnimPostFXEventPollingManager::MAX_EVENTS = 128;
u32 AnimPostFXEventPollingManager::EVENT_POLLING_TIMEOUT = 20000U;

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventPollingManager::Init()
{
	m_events.Init(MAX_EVENTS);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventPollingManager::Shutdown()
{
	m_events.Shutdown();
	m_managedEventGroups.Reset();
}

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXEventPollingManager::AddListener(const atHashString fxName)
{
	const RegisteredAnimPostFXStack* pStack = ANIMPOSTFXMGR.Get(fxName);

	// Check the effect exists at all...
	if (pStack == NULL)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] AddListener: effect \"%s\" doesn't exist", fxName.TryGetCStr());
	#endif	
		return false;
	}

	// Check it's been set up with a valid event group...
	s32 groupId = pStack->GetGroupId();
	if (groupId == -1)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] AddListener: effect \"%s\" hasn't been setup with a valid group id (-1)", fxName.TryGetCStr());
	#endif
		return false;
	}

	// Also make sure we can store a new group...
	if (m_managedEventGroups.Access(groupId) == NULL && m_managedEventGroups.GetNumUsed() >= AnimPostFXEventManager::GetMaxListeners())
	{
#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] AddListener: effect \"%s\" cannot add more listeners (%d registered)", fxName.TryGetCStr(), m_managedEventGroups.GetNumUsed());
#endif	
		return false;
	}


	// Add the listener...
	bool bOk = AddListener(groupId);

	if (bOk)
	{
		s32* pGroupRefCount = m_managedEventGroups.Access(groupId);

		// Is it already managed? Add a ref...
		if (pGroupRefCount)
		{
			*pGroupRefCount = *pGroupRefCount+1;
		}
		else // Add the group to our managed groups table
		{
			m_managedEventGroups.Insert(groupId, 1);
		}
	}

#if __BANK
	const s32* pRefCount = m_managedEventGroups.Access(groupId);
	const s32 refCount = pRefCount ? *pRefCount : -1;
	Displayf("[ANIMPOSTFX][POLLING] AddListener: effect \"%s\" (%d, numRefs: %d) registration %s", fxName.TryGetCStr(), groupId, refCount, bOk ? "SUCCEEDED" : "FAILED");
#endif

	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventPollingManager::RemoveListener(const atHashString fxName)
{
	const RegisteredAnimPostFXStack* pStack = ANIMPOSTFXMGR.Get(fxName);
	// Check the effect exists at all...
	if (pStack == NULL)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] RemoveListener: effect \"%s\" doesn't exist", fxName.TryGetCStr());
	#endif	
		return;
	}

	// Check it's been set up with a valid event group...
	s32 groupId = pStack->GetGroupId();
	if (groupId == -1)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] RemoveListener: effect \"%s\" hasn't been setup with a valid group id (-1)", fxName.TryGetCStr());
	#endif
		return;
	}
	
	s32* pGroupRefCount = m_managedEventGroups.Access(groupId);
	bool bShouldRemoveListener = false;

	// The group should be in our table... remove a ref
	if (Verifyf(pGroupRefCount, "[ANIMPOSTFX][POLLING] RemoveListener: effect \"%s\" belongs to an unmanaged group (%d)", fxName.TryGetCStr(), groupId))
	{
		*pGroupRefCount = *pGroupRefCount-1;

		// No more effects from the same group being referenced?
		if (*pGroupRefCount <= 0)
		{
			m_managedEventGroups.Delete(groupId);
			bShouldRemoveListener = true;
		}
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] RemoveListener: effect \"%s\" (group %d has %d refs)", fxName.TryGetCStr(), groupId, *pGroupRefCount);
	#endif
	}


	if (bShouldRemoveListener)
	{
		// We're done with all effects belonging to groupId, so completely remove the listener 
		RemoveListener(groupId);
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] RemoveListener: effect \"%s\" AND group %d REMOVED", fxName.TryGetCStr(), groupId);
	#endif
	}

}

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXEventPollingManager::AddListener(s32 groupId)
{	
	bool bOk = ANIMPOSTFXMGR.RegisterListener(MakeFunctor(*this, &AnimPostFXEventPollingManager::OnEventTriggered), groupId);
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventPollingManager::RemoveListener(s32 groupId)
{
	ANIMPOSTFXMGR.RemoveListener(MakeFunctor(*this, &AnimPostFXEventPollingManager::OnEventTriggered), groupId);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventPollingManager::OnEventTriggered(AnimPostFXEvent* pEvent)
{
	if (m_events.GetNumFree() == 0)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] OnEventTriggered \"%s\", event queue is FULL", pEvent->fxName.TryGetCStr());
	#endif	
		return;
	}
	AnimPostFXPolledEvent incomingEvent;
	incomingEvent.srcEvent = *pEvent;
	incomingEvent.timeStamp = fwTimer::GetTimeInMilliseconds();
	incomingEvent.bConsumed = false;
	m_events.Insert(incomingEvent);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventPollingManager::Update()
{
	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	// Remove consumed and old events
	PolledEventNode* pNode = m_events.GetFirst()->GetNext();
	while(pNode != m_events.GetLast())
	{
		AnimPostFXPolledEvent& curEntry = (pNode->item);
		PolledEventNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		u32 timeElapsed = (currentTime-curEntry.timeStamp);
		bool bExpired = (timeElapsed > EVENT_POLLING_TIMEOUT);
		if (curEntry.bConsumed || bExpired)
		{
		#if __BANK
			Displayf("[ANIMPOSTFX][POLLING] Update \"%s\", removing event - %s", curEntry.srcEvent.fxName.TryGetCStr(), bExpired ? "EXPIRED" : "CONSUMED");
		#endif	
			// Forget about it
			m_events.Remove(pLastNode);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXEventPollingManager::HasEventTriggered(atHashString fxName, AnimPostFXEventType eventType, bool bPeekOnly, bool& bIsRegistered)
{
	// Assume it hasn't triggered...
	bool bHasTriggered = false;

	// Assume the group id for the effect is actually registered (checks incoming...)
	bIsRegistered = true;
	
	// Check the effect exists at all...
	const RegisteredAnimPostFXStack* pStack = ANIMPOSTFXMGR.Get(fxName);
	if (pStack == NULL)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] HasEventTriggered: effect \"%s\" doesn't exist", fxName.TryGetCStr());
	#endif	
		bIsRegistered = false;
		return false;
	}

	// Check it's been set up with a valid event group...
	s32 groupId = pStack->GetGroupId();
	if (groupId == -1)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] HasEventTriggered: effect \"%s\" hasn't been setup with a valid group id (-1)", fxName.TryGetCStr());
	#endif
		bIsRegistered = false;
		return false;
	}

	// Check we're actually listening to this type of event...
	if (m_managedEventGroups.Access(groupId) == NULL)
	{
	#if __BANK
		Displayf("[ANIMPOSTFX][POLLING] HasEventTriggered: effect \"%s\" with group id (%d) is NOT registered", fxName.TryGetCStr(), groupId);
	#endif
		bIsRegistered = false;
		return false;
	}

	// See if the event is here
	PolledEventNode* pNode = m_events.GetFirst()->GetNext();
	while(pNode != m_events.GetLast())
	{
		AnimPostFXPolledEvent& curEntry = (pNode->item);
		pNode = pNode->GetNext();

		const bool bNameMatches			= (fxName == curEntry.srcEvent.fxName);
		const bool bNotConsumedYet		= (curEntry.bConsumed == false);
		const bool bEventTypeMatches	= (curEntry.srcEvent.eventType == eventType);

		// If name and event type match and the event hasn't been previously consume, report it as triggered
		if (bNameMatches && bEventTypeMatches && bNotConsumedYet)
		{
			bHasTriggered = true;
			curEntry.bConsumed = !bPeekOnly;
		#if __BANK
			Displayf("[ANIMPOSTFX][POLLING] HasEventTriggered \"%s\" TRIGGERED (groupdId: %d, consumed: %d, eventType: %d)", 
					curEntry.srcEvent.fxName.TryGetCStr(), groupId, curEntry.bConsumed, curEntry.srcEvent.eventType);
		#endif
			break;
		}
	}

	return bHasTriggered;
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
char AnimPostFXEventPollingManager::ms_registeredFxName[128] = {0};

void AnimPostFXEventPollingManager::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Polled Events Test");
		bank.AddText("FX Name To Test Events:", &ms_registeredFxName[0], 127);
		bank.AddButton("REGISTER FX STACK", datCallback(MFA(AnimPostFXEventPollingManager::OnRegisterEvent), (datBase*)this));
		bank.AddButton("UNREGISTER FX STACK", datCallback(MFA(AnimPostFXEventPollingManager::OnUnregisterEvent), (datBase*)this));
	bank.PopGroup();
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void AnimPostFXEventPollingManager::OnRegisterEvent()
{
	atHashString fxName = &ms_registeredFxName[0];
	AddListener(fxName);
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void AnimPostFXEventPollingManager::OnUnregisterEvent()
{
	atHashString fxName = &ms_registeredFxName[0];
	RemoveListener(fxName);
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
const s32 AnimPostFXEventManager::MAX_LISTENERS = 32;

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventManager::Init()
{
	m_listenerList.Init(MAX_LISTENERS);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventManager::Shutdown()
{
	m_listenerList.Shutdown();
}

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXEventManager::RegisterListener(AnimPostFXEventCB pCBFunc, s32 groupId)
{
	if (m_listenerList.GetNumFree() == 0 || Exists(pCBFunc, groupId))
	{
		return false;
	}

	AnimPostFXListener newListener;
	newListener.eventCallback = pCBFunc;
	newListener.groupId = groupId;

	m_listenerList.Insert(newListener);

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventManager::RemoveListener(AnimPostFXEventCB pCBFunc, s32 groupId)
{
	ListenerNode* pNode = m_listenerList.GetFirst()->GetNext();
	while(pNode != m_listenerList.GetLast())
	{
		AnimPostFXListener& curEntry = (pNode->item);
		ListenerNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		if (curEntry.eventCallback == pCBFunc && curEntry.groupId == groupId)
		{
			// Forget about it
			m_listenerList.Remove(pLastNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXEventManager::OnEventTriggered(const RegisteredAnimPostFXStack* pStack, AnimPostFXEventType eventType)
{
	// Don't bother with NULL fx stacks or invalid event types
	if (pStack == NULL || eventType == ANIMPOSTFX_EVENT_INVALID)
	{
		return;
	}

	// Or if the group id is invalid
	s32 groupId = pStack->GetGroupId();
	if (groupId == -1)
	{
		return;
	}

	// Broadcast the event
	const ListenerNode* pCurrentNode = m_listenerList.GetFirst()->GetNext();
	while(pCurrentNode != m_listenerList.GetLast())
	{
		const AnimPostFXListener& currentEntry = (pCurrentNode->item);
		pCurrentNode = pCurrentNode->GetNext();
		if (currentEntry.groupId == groupId)
		{
			// Cache event data
			AnimPostFXEvent currentEvent;
			currentEvent.groupId = pStack->GetGroupId();
			currentEvent.fxName = pStack->GetName();
			currentEvent.eventType = eventType;
			currentEvent.userTag = pStack->GetEventUserTag();

			// Pass on the event data to the listener
			if (Verifyf(currentEntry.eventCallback != 0, "AnimPostFXEventManager::OnEventTriggered: NULL callback"))
			{
				currentEntry.eventCallback(&currentEvent);
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
bool AnimPostFXEventManager::Exists(AnimPostFXEventCB pCBFunc, s32 groupId) const
{
	const ListenerNode* pCurrentNode = m_listenerList.GetFirst()->GetNext();
	while(pCurrentNode != m_listenerList.GetLast())
	{
		const AnimPostFXListener& currentEntry = (pCurrentNode->item);
		pCurrentNode = pCurrentNode->GetNext();
		if (currentEntry.eventCallback == pCBFunc && currentEntry.groupId == groupId)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::Add(atHashString modifierName, eAnimatedPostFXMode animMode, u32 inDelayDuration, u32 inDuration, u32 holdDuration, u32 outDuration)
{
	if (m_Layers.GetCount() >= kNumEffectLayers)
	{
		return;
	}

	AnimatedPostFX effect;
	effect.Set(modifierName, animMode, inDelayDuration, inDuration, holdDuration, outDuration);
	m_Layers.Push(effect);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::Set(u32 layer, atHashString modifierName, eAnimatedPostFXMode animMode, u32 inDelayDuration, u32 inDuration, u32 holdDuration, u32 outDuration)
{
	if (m_Layers.GetCount() <= layer)
	{
		return;
	}

	m_Layers[layer].Set(modifierName, animMode, inDelayDuration, inDuration, holdDuration, outDuration);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::SetLoopEnabled(bool bEnabled)
{
	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		m_Layers[i].SetLoopEnabled(bEnabled);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::Start()
{
	if (m_UserFadeOverrideEnabled == false)
	{
		m_UserFadeOverrideLevel = 1.0f;
	}

	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		m_Layers[i].Start();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::Start(u32 duration)
{
	if (m_UserFadeOverrideEnabled == false)
	{
		m_UserFadeOverrideLevel = 1.0f;
	}

	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		m_Layers[i].Start(duration);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::Update()
{
	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		m_Layers[i].Update();
	}

	if (m_LayerBlend.IsEnabled())
	{
		m_LayerBlend.Update();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::Reset()
{
	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		m_Layers[i].Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
//
u32 AnimatedPostFXStack::GetDuration() const
{
	u32 duration = 0U;
	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		duration = Max(duration, m_Layers[i].GetDuration());
	}

	return duration;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::ApplyModifiers(tcKeyframeUncompressed& keyframe) const
{

	if (m_LayerBlend.IsEnabled())
	{
		m_LayerBlend.ApplyModifiers(keyframe, m_UserFadeOverrideLevel);

		for (u32 i = 0; i < m_Layers.GetCount(); i++)
		{
			bool bSkipLayer = (m_Layers[i].GetModifierName() == m_LayerBlend.m_LayerA || m_Layers[i].GetModifierName() == m_LayerBlend.m_LayerB);
			if (!bSkipLayer)
			{
				m_Layers[i].ApplyModifiers(keyframe, m_UserFadeOverrideLevel);
			}
		}

		return;
	}

	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		m_Layers[i].ApplyModifiers(keyframe, m_UserFadeOverrideLevel);
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool AnimatedPostFXStack::IsIdle() const 
{
	bool bIsIdle = true;
	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		bIsIdle = bIsIdle && (m_Layers[i].IsIdle());
	}

	return bIsIdle;
}

//////////////////////////////////////////////////////////////////////////
//
float AnimatedPostFXStack::GetCurrentTime() const
{
	float currentTime = 0.0f;

	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		currentTime = rage::Max(currentTime, m_Layers[i].GetFadeLevel());	
	}

	return currentTime;
}

//////////////////////////////////////////////////////////////////////////
//
u32 AnimatedPostFXStack::GetCurrentTimeMS() const
{
	u32 currentTime = 0U;

	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		currentTime = rage::Max(currentTime, m_Layers[i].GetCurrentTime());	
	}

	return currentTime;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::SetUserFadeOverride(bool bEnable)
{
	m_UserFadeOverrideEnabled = bEnable;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::SetUserFadeLevel(float level)
{
	m_UserFadeOverrideLevel = Saturate(level);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::PostLoadCallback()
{
	// set up layer blending modifier
	if (m_LayerBlend.IsEnabled())
	{
		// disable it until we verify it's been correctly setup
		m_LayerBlend.Disable();

		const AnimatedPostFX* pLayerA = NULL;
		const AnimatedPostFX* pLayerB = NULL;

		const atHashString layerA = m_LayerBlend.m_LayerA;
		const atHashString layerB = m_LayerBlend.m_LayerB;

		for (u32 i = 0; i < m_Layers.GetCount(); i++)
		{
			if (m_Layers[i].GetModifierName() == layerA)
			{
				pLayerA = &m_Layers[i];
			}
			else if (m_Layers[i].GetModifierName() == layerB)
			{
				pLayerB = &m_Layers[i];
			}
		}

		// if it's all good, set up and toggle the blending mod on
		if (pLayerA != NULL && pLayerB != NULL && pLayerA != pLayerB)
		{
			m_LayerBlend.Enable();
			m_LayerBlend.Set(pLayerA, pLayerB);
		}

		Assertf(pLayerA != NULL && pLayerB != NULL, "FX stack has bad data: layer blending modifier is enabled, but layers could not be found");
		Assertf(pLayerA != pLayerB, "FX stack has bad data: layer blending modifier is enabled, but layer A and B are the same");
	}
}


#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void AnimatedPostFXStack::AddWidgets(rage::bkBank& bank )
{
	for (u32 i = 0; i < m_Layers.GetCount(); i++)
	{
		bank.AddToggle("Disable Layer", &(m_Layers[i].m_Disabled));
		PARSER.AddWidgets(bank, &(m_Layers[i]));
		bank.AddSeparator();
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::Init()
{
	m_Name = 0U;
	m_Priority = 0;
	m_StartRequested = false;
	m_StopRequested = false;
	m_OverriddenDuration = 0U;
	m_FXStack.m_Layers.Reset();
	m_FrameEventTriggered = false;
}

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::Start()
{
	m_StartRequested = true;
	m_OverriddenDuration = 0U;
	m_FrameEventTriggered = false;
}

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::Start(u32 duration)
{
	m_StartRequested = true;
	m_OverriddenDuration = duration;
	m_FrameEventTriggered = false;
}

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::CancelStartRequest() 
{
	m_StartRequested = false; 
}

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::Reset()
{
	m_FXStack.Reset();
	m_OverriddenDuration = 0U;
	m_FrameEventTriggered = false;
	m_FrameDelay = 0U;
}

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::Update()
{
	m_FXStack.Update();
}

void RegisteredAnimPostFXStack::FrameDelayUpdate()
{
	#if __BANK
		if (AnimPostFXManager::GetInstance().DebugDisplay())
		{
			Displayf("[ANIMPOSTFX] FrameDelayUpdate for %s - frameDelay %d", m_Name.TryGetCStr(), m_FrameDelay);
		}
		if (AnimPostFXManager::GetInstance().DebugOutput())
		{
			grcDebugDraw::AddDebugOutput(Color_white, "[ANIMPOSTFX] FrameDelayUpdate for %s - frameDelay %d", m_Name.TryGetCStr(), m_FrameDelay);
		}
	#endif

	if( m_FrameDelay )
	{
		m_FrameDelay--;

		if( m_FrameDelay == 0 )
		{
			if (GetGroupId() != -1 && DoesEventTypeMatch(ANIMPOSTFX_EVENT_ON_START))
			{
			#if __BANK
				if (AnimPostFXManager::GetInstance().DebugOutput())
				{
					Displayf("[ANIMPOSTFX][EVENT] Delayed Start event triggered for \"%s\"", m_Name.TryGetCStr());
				}
			#endif	
				AnimPostFXManager::GetInstance().OnEventTriggered(this, ANIMPOSTFX_EVENT_ON_START);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
//
float RegisteredAnimPostFXStack::GetCurrentTime() const
{
	float currentTime = 0.0f;

	for (u32 i = 0; i < m_FXStack.m_Layers.GetCount(); i++)
	{
		currentTime = rage::Max(currentTime, m_FXStack.m_Layers[i].GetFadeLevel());	
	}

	return currentTime;
}

//////////////////////////////////////////////////////////////////////////
//
void RegisteredAnimPostFXStack::ApplyModifiers(tcKeyframeUncompressed& keyframe)
{
	if (m_StopRequested)
	{
		Reset();
		m_StopRequested = false;
	}

	if (m_StartRequested)
	{
		const u32 duration = m_OverriddenDuration;
		Reset();

		if (duration > 0)
		{
			m_FXStack.Start(duration);
		}
		else 
		{
			m_FXStack.Start();
		}
		// run an update tick so that the effect has a chance to get picked up this frame
		m_FXStack.Update();
		m_StartRequested = false;
	}

	m_FXStack.ApplyModifiers(keyframe);
}

//////////////////////////////////////////////////////////////////////////
//
bool RegisteredAnimPostFXStack::DoesEventTypeMatch(AnimPostFXEventType incomingEventType) const
{
	AnimPostFXEventType eventType = GetEventType();

	// Out of bounds?
	if ((incomingEventType < ANIMPOSTFX_EVENT_ON_START || incomingEventType > ANIMPOSTFX_EVENT_ON_FRAME) ||
		(eventType < ANIMPOSTFX_EVENT_ON_START || eventType > ANIMPOSTFX_EVENT_ON_STOP_ON_FRAME))
		return false;
	
	static const bool eventTypeTable[7][3]=
	{
		// [ANIMPOSTFX_EVENT_ON_START]		[ANIMPOSTFX_EVENT_ON_STOP]		[ANIMPOSTFX_EVENT_ON_FRAME]
		{true,								false,							false },	// ANIMPOSTFX_EVENT_ON_START,
		{false,								true,							false },	// ANIMPOSTFX_EVENT_ON_STOP,
		{false,								false,							true  },	// ANIMPOSTFX_EVENT_ON_FRAME,
		{true,								true,							false },	// ANIMPOSTFX_EVENT_ON_START_ON_STOP,
		{true,								true,							true  },	// ANIMPOSTFX_EVENT_ON_START_ON_STOP_ON_FRAME,
		{true,								false,							true  },	// ANIMPOSTFX_EVENT_ON_START_ON_FRAME,
		{true,								true,							false },	// ANIMPOSTFX_EVENT_ON_STOP_ON_FRAME,
	};

	return eventTypeTable[eventType][incomingEventType];
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void RegisteredAnimPostFXStack::ClearForEdit()
{
	Init();
	m_FXStack.m_Layers.SetCount(AnimatedPostFXStack::kNumEffectLayers);

	for (int i = 0; i < m_FXStack.m_Layers.GetCount(); i++)
	{
		m_FXStack.m_Layers[i].Init();
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool RegisteredAnimPostFXStack::Validate()
{
	bool bIsValid = true;

	u32 numUnusedMods = 0;
	u32 totalDuration = 0;
	for (int i = 0; i < m_FXStack.GetCount(); i++)
	{
		// it's ok to leave some of the layers unused
		if (m_FXStack.m_Layers[i].GetModifierName().IsNull())
		{
			numUnusedMods++;
		}
		// but if a mod name was assigned, it better exists
		else if (Verifyf(g_timeCycle.FindModifier(m_FXStack.m_Layers[i].GetModifierName(),"RegisteredAnimPostFXStack") != NULL, "TCMod \"%s\" in layer %d doesn't exist", m_FXStack.m_Layers[i].GetModifierName().TryGetCStr(), i))
		{
			totalDuration += m_FXStack.m_Layers[i].GetDuration();
		}
		// neither of them: fail
		else 
		{
			bIsValid = false;
			break;
		}
	}

	bIsValid = bIsValid && Verifyf(numUnusedMods < AnimatedPostFXStack::kNumEffectLayers, "None of the layers was assigned any TC mod");
	bIsValid = bIsValid && Verifyf(totalDuration > 0, "Total duration for effect stack is 0");

	return bIsValid;
}
#endif


#if GTA_REPLAY

// Just add a new entry for any replay is interested in
AnimPostFXManagerReplayEffectLookup::AnimPostFXReplayEffectTableEntry	AnimPostFXManagerReplayEffectLookup::m_PostEffectReplayGameCamRemovalLookupTable[] = 
{
	// Pause menu, weapon wheel, player switch etc...
	{ ATSTRINGHASH("SwitchHUDMichaelIn",0x10493196),		NULL, false },
	{ ATSTRINGHASH("SwitchHUDMichaelOut",0x2fe80a59),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenMichaelIn",0x848674fd),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenMichaelMid",0x621be8f8),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenMichaelOut",0x15dca17b),		NULL, false },
	{ ATSTRINGHASH("SwitchSceneMichael",0x2f8758e3),		NULL, false },
	{ ATSTRINGHASH("SwitchShortMichaelIn",0x10fb8eb6),		NULL, false },
	{ ATSTRINGHASH("SwitchShortMichaelMid",0x8a7d2d1b),		NULL, false },

	{ ATSTRINGHASH("SwitchHUDFranklinIn",0x07e17b1a),		NULL, false },
	{ ATSTRINGHASH("SwitchHUDFranklinOut",0x106d489b),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenFranklinIn",0x776fe6f3),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenFranklinMid",0x22d04949),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenFranklinOut",0x1bae2d29),		NULL, false },
	{ ATSTRINGHASH("SwitchSceneFranklin",0x66fb30fe),		NULL, false },
	{ ATSTRINGHASH("SwitchShortFranklinIn",0xc2225603),		NULL, false },
	{ ATSTRINGHASH("SwitchShortFranklinMid",0x6ba28347),	NULL, false },

	{ ATSTRINGHASH("SwitchHUDTrevorIn",0xee33c206),			NULL, false },
	{ ATSTRINGHASH("SwitchHUDTrevorOut",0x08c59987),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenTrevorIn",0x1a4e629d),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenTrevorMid",0xfb80db62),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenTrevorOut",0x87f585eb),		NULL, false },
	{ ATSTRINGHASH("SwitchSceneTrevor",0xee02c950),			NULL, false },
	{ ATSTRINGHASH("SwitchShortTrevorIn",0xd91bdd3),		NULL, false },
	{ ATSTRINGHASH("SwitchShortTrevorMid",0x67ae7f68),		NULL, false },

	{ ATSTRINGHASH("SwitchHUDIn",0xb2895e1b),				NULL, false },
	{ ATSTRINGHASH("SwitchHUDOut",0x9ac7662a),				NULL, false },
	{ ATSTRINGHASH("SwitchOpenNeutral",0xc36d571f),			NULL, false },
	{ ATSTRINGHASH("SwitchOpenNeutralIn",0xe5cafc5b),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenNeutralMid",0x6cbdd92d),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenNeutralOut",0x2977920c),		NULL, false },
	{ ATSTRINGHASH("SwitchShortNeutralIn",0x66fcfd3e),		NULL, false },
	{ ATSTRINGHASH("SwitchShortNeutralMid",0x19035b64),		NULL, false },

	// FIB2 sniping from chopper
	{ ATSTRINGHASH("CamPushInFranklin",0xee41dacb),			NULL, false },

	// Player killing a ped
	{ ATSTRINGHASH("PedGunKill",0x9cfb4c6e),				NULL, false },

	// Lektor motorbike
	{ ATSTRINGHASH("LectroKERS",0x4020f9f7),				NULL, false },
	{ ATSTRINGHASH("LectroKERSOut",0xf53d5701),				NULL, false },

	// Player Death
	{ ATSTRINGHASH("DeathFailNeutralIn",0x7842c770),		NULL, false },
	{ ATSTRINGHASH("DeathFailFranklinIn",0x1074ad81),		NULL, false },
	{ ATSTRINGHASH("DeathFailMichaelIn",0x19509151),		NULL, false },
	{ ATSTRINGHASH("DeathFailTrevorIn",0x2fb801ed),			NULL, false },
	{ ATSTRINGHASH("DeathFailOut",0x4ac9635a),				NULL, false },

	// Green Respawn
	{ ATSTRINGHASH("RespawnMichael",0x1df47fe2),			NULL, false },
	{ ATSTRINGHASH("RespawnTrevor",0x7e421bfe),				NULL, false },
	{ ATSTRINGHASH("RespawnFranklin",0x6f193857),			NULL, false },

	// Mission Success
	{ ATSTRINGHASH("SuccessNeutral",0xe59492af),			NULL, false },
	{ ATSTRINGHASH("SuccessFranklin",0x33460f26),			NULL, false },
	{ ATSTRINGHASH("SuccessMichael",0x4c589541),			NULL, false },
	{ ATSTRINGHASH("SuccessTrevor",0x96631bd8),				NULL, false },

	// Special Ability
	{ ATSTRINGHASH("REDMIST",0xe4357941),					NULL, false },		// Trevor
	{ ATSTRINGHASH("REDMISTOut",0x8889c8b6),				NULL, false },		
	{ ATSTRINGHASH("BulletTime",0x446dd766),				NULL, false },		// Micheal
	{ ATSTRINGHASH("BulletTimeOut",0x0d2427e1),				NULL, false },
	{ ATSTRINGHASH("DrivingFocus",0x2a135c43),				NULL, false },		// Franklin
	{ ATSTRINGHASH("DrivingFocusOut",0x6e5c08ad),			NULL, false },
	{ ATSTRINGHASH("DefaultBlinkIntro",0xe7590770),			NULL, false },
	{ ATSTRINGHASH("DefaultBlinkOutro",0x22d8dce3),			NULL, false },

	// Pause Menu
	{ ATSTRINGHASH("PauseMenuFranklinIn",0x80802e11),		NULL, false },
	{ ATSTRINGHASH("PauseMenuFranklinOut",0x388a0486),		NULL, false },
	{ ATSTRINGHASH("PauseMenuMichaelIn",0xa0544755),		NULL, false },
	{ ATSTRINGHASH("PauseMenuMichaelOut",0x9cbeebee),		NULL, false },
	{ ATSTRINGHASH("PauseMenuTrevorIn",0xb3ed728),			NULL, false },
	{ ATSTRINGHASH("PauseMenuTrevorOut",0x24239ef8),		NULL, false },
	{ ATSTRINGHASH("PauseMenuIn",0x44dabe22),				NULL, false },
	{ ATSTRINGHASH("PauseMenuOut",0x725941f7),				NULL, false },

	// CnC Pause Menu
	{ ATSTRINGHASH("PauseMenuCopsIn",0x786AA98A),			NULL, false },
	{ ATSTRINGHASH("PauseMenuCopsOut",0x6A4AFA62),			NULL, false },
	{ ATSTRINGHASH("PauseMenuCrooksIn",0x833600C0),			NULL, false },
	{ ATSTRINGHASH("PauseMenuCrooksOut",0x9B727EB3),		NULL, false },

	// Misc
	{ ATSTRINGHASH("MP_race_crash",0xd78f4146),				NULL, false },		// Filter effect from respawning in an MP air race
	{ ATSTRINGHASH("FocusIn",0x99d8ef9),					NULL, false },		// Tow Truck Focus
	{ ATSTRINGHASH("FocusOut",0xd8664e89),					NULL, false },		// Tow Truck Focus
	{ ATSTRINGHASH("RaceTurbo",0x600d9d85),					NULL, false },		// Turbo Boost at start of a race.
	{ ATSTRINGHASH("MP_Bull_tost",0xa51fd197),				NULL, false },		// Bullshark testosterone
	{ ATSTRINGHASH("MP_Bull_tost_Out",0x876bff8e),			NULL, false },
	{ ATSTRINGHASH("MP_Killstreak",0xe0940793),				NULL, false },		// Kill streak
	{ ATSTRINGHASH("MP_Killstreak_Out",0xaba3efb6),			NULL, false },
	{ ATSTRINGHASH("MP_Loser_Streak",0x5d964052),			NULL, true },		// Losing streak
	{ ATSTRINGHASH("MP_Loser_Streak_Out",0xef8c38c5),		NULL, false },
	{ ATSTRINGHASH("MP_Powerplay",0xab722f6a),				NULL, true },
	{ ATSTRINGHASH("MP_Powerplay_Out",0x290be57e),			NULL, false },
	{ ATSTRINGHASH("MP_Celeb_Preload_Fade",0xee6b9efe),		NULL, false },		// Gang attack wins
	{ ATSTRINGHASH("MP_Celeb_Win",0xd4851430),				NULL, false },
	{ ATSTRINGHASH("MP_Celeb_Win_Out",0xfde9ac37),			NULL, false },
	{ ATSTRINGHASH("PeyoteIn",0x6a7b1294),					NULL, true },
	{ ATSTRINGHASH("PeyoteOut",0xe7e4f08c),					NULL, true },
	{ ATSTRINGHASH("PeyoteEndIn",0x91304b90),				NULL, true },
	{ ATSTRINGHASH("PeyoteEndOut",0xf878a71b),				NULL, true },
	{ ATSTRINGHASH("PennedIn",0x5aae758f),					NULL, true },
	{ ATSTRINGHASH("PennedInOut",0xd43dcab2),				NULL, true },
	{ ATSTRINGHASH("MinigameEndNeutral",0x507bf7b6),		NULL, false },
	{ ATSTRINGHASH("CrossLine", 0x6B66A555),				NULL, false },
	{ ATSTRINGHASH("CrossLineOut", 0xc67eb9dc),				NULL, false },
	{ ATSTRINGHASH("StuntFast", 0xee553f83),				NULL, false },
	{ ATSTRINGHASH("StuntSlow", 0x6824d91c),				NULL, false },
	{ ATSTRINGHASH("VolticTurbo", 0xe43bb5e5),				NULL, false },
	{ ATSTRINGHASH("ArenaEMP", 0xcfdef9f5),					NULL, false },
	{ ATSTRINGHASH("ArenaEMPOut", 0xca951551),				NULL, false },

	{ ATSTRINGHASH("CnC_FlashGrenade_Strong", 0xef6e233b),	NULL, false },
	{ ATSTRINGHASH("SwitchOpenNeutralCnC", 0x53f72a36), 	NULL, false },
	{ ATSTRINGHASH("SwitchOpenCrookIn", 0x2767da43),		NULL, false },
	{ ATSTRINGHASH("SwitchOpenCrookMid", 0x7990e0dd), 		NULL, false },
	{ ATSTRINGHASH("SwitchOpenCrookOut", 0x9bebec20), 		NULL, false },

	{ ATSTRINGHASH("DanceIntensity01", 0x4676ff8c), 		NULL, true },
	{ ATSTRINGHASH("DanceIntensity02", 0x30afd3fe), 		NULL, true },
	{ ATSTRINGHASH("DanceIntensity03", 0x178d21b9), 		NULL, true },
};

AnimPostFXManagerReplayEffectLookup::AnimPostFXReplayEffectTableEntry	AnimPostFXManagerReplayEffectLookup::m_PostEffectReplayFreeCamRemovalLookupTable[] = 
{
	{ ATSTRINGHASH("DrugsDrivingIn",0x8b05df27),			NULL, false }
};


void AnimPostFXManagerReplayEffectLookup::Init(AnimPostFXManager *pFxManager)
{
	// Clear out any pointers in the tables
	Reset();

	// And reset them
	int nElements = sizeof(m_PostEffectReplayGameCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0; i<nElements; i++)
	{
		AnimPostFXReplayEffectTableEntry &tableEntry = m_PostEffectReplayGameCamRemovalLookupTable[i];
		tableEntry.m_pStack = pFxManager->Get(tableEntry.m_Name);
	}

	nElements = sizeof(m_PostEffectReplayFreeCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0; i<nElements; i++)
	{
		AnimPostFXReplayEffectTableEntry &tableEntry = m_PostEffectReplayFreeCamRemovalLookupTable[i];
		tableEntry.m_pStack = pFxManager->Get(tableEntry.m_Name);
	}
}


void AnimPostFXManagerReplayEffectLookup::Shutdown()
{

}

void AnimPostFXManagerReplayEffectLookup::Reset()
{
	int nElements = sizeof(m_PostEffectReplayGameCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0;i<nElements;i++)
	{
		m_PostEffectReplayGameCamRemovalLookupTable[i].m_pStack = NULL;
	}

	nElements = sizeof(m_PostEffectReplayFreeCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0;i<nElements;i++)
	{
		m_PostEffectReplayFreeCamRemovalLookupTable[i].m_pStack = NULL;
	}
}

bool AnimPostFXManagerReplayEffectLookup::AreAllGameCamStacksIdle(bool forThumbnail, int* activeIndex)
{
	bool bAllIdle = true;
	if(activeIndex) *activeIndex = -1;

	int nElements = sizeof(m_PostEffectReplayGameCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0;i<nElements;i++)
	{
		if(m_PostEffectReplayGameCamRemovalLookupTable[i].m_pStack)
		{
			if(m_PostEffectReplayGameCamRemovalLookupTable[i].m_pStack->IsIdle() == false)
			{
				if(forThumbnail && m_PostEffectReplayGameCamRemovalLookupTable[i].m_AllowForThumbnail)
				{
					// Ignore this filter for thumbnails (allow the thumbnail to be created.)
				}
				else
				{
					bAllIdle = false;
					if(activeIndex) *activeIndex = i;
					break;
				}
			}
		}
	}
	return bAllIdle;
}

bool AnimPostFXManagerReplayEffectLookup::IsStackDisabledForGameCam(const RegisteredAnimPostFXStack* pStack)
{
	int nElements = sizeof(m_PostEffectReplayGameCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0;i<nElements;i++)
	{
		if(m_PostEffectReplayGameCamRemovalLookupTable[i].m_pStack == pStack)
		{
			return true;
		}
	}

	return false;
}

bool AnimPostFXManagerReplayEffectLookup::IsStackDisabledForFreeCam(const RegisteredAnimPostFXStack* pStack)
{
	if( IsStackDisabledForGameCam(pStack) )
	{
		return true;
	}

	int nElements = sizeof(m_PostEffectReplayFreeCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	for(int i=0;i<nElements;i++)
	{
		if(m_PostEffectReplayFreeCamRemovalLookupTable[i].m_pStack == pStack)
		{
			return true;
		}
	}
	return false;
}


#if !__FINAL
const char* AnimPostFXManagerReplayEffectLookup::GetFilterName(int index)
{
	int nElements = sizeof(m_PostEffectReplayGameCamRemovalLookupTable) / sizeof(AnimPostFXReplayEffectTableEntry);
	if(index < 0 || index >= nElements)
		return "Not Filter";

	return m_PostEffectReplayGameCamRemovalLookupTable[index].m_Name.GetCStr();
}
#endif // !__FINAL

#endif	// GTA_REPLAY

//////////////////////////////////////////////////////////////////////////
//
AnimPostFXManager*	AnimPostFXManager::smp_Instance = NULL;
#define ANIMPOSTFX_METADATA_FILE_LOAD_NAME	"platform:/data/effects/animpostfx" 
#define ANIMPOSTFX_METADATA_FILE_SAVE_NAME	RS_ASSETS "/export/data/effects/animpostfx" 

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
const char*	AnimPostFXManager::ms_debugUserTypeStr[] = {	"CODE_MISC", 
															"CODE_PEDKILL",
															"CODE_CAMFLASH",
															"CODE_SELECTIONWHEEL",
															"CODE_LONGSWITCH",
															"CODE_SHORTSWITCH",
															"CODE_KERSBOOST",
															"CODE_FPPROP",
															"SCRIPT", 
															"CODE_SPECIAL_ABILITY" 
														}; 


#endif 


//////////////////////////////////////////////////////////////////////////
//
#if __BANK
static bool AnimPostFXIsSpecialAbility(u32 strHash)
{
	const u32 specialFxHash[] = {
		ATSTRINGHASH("REDMIST",				0xe4357941),
		ATSTRINGHASH("REDMISTOut",			0x8889c8b6),
		ATSTRINGHASH("BulletTime",			0x446dd766),
		ATSTRINGHASH("BulletTimeOut",		0x0d2427e1),
		ATSTRINGHASH("DrivingFocus",		0x2a135c43),
		ATSTRINGHASH("DrivingFocusOut",		0x6e5c08ad),
		ATSTRINGHASH("DefaultBlinkIntro",	0xe7590770),
		ATSTRINGHASH("DefaultBlinkOutro",	0x22d8dce3)
	};

	for (u32 i = 0; i < NELEM(specialFxHash); i++)
	{
		if (specialFxHash[i] == strHash)
			return true;
	}

	return false;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::CancelStartRequest(atHashString effectName, eAnimPostFXUser BANK_ONLY(user))
{
	RegisteredAnimPostFXStack* pStack = Get(effectName);

#if __BANK
	if (m_bDebugOutput)
	{
		Displayf("[ANIMPOSTFX] CancelStartRequest \"%s\" requested by %s", effectName.TryGetCStr(), ms_debugUserTypeStr[user]);
	}
#endif	

	if (pStack)
	{
		pStack->CancelStartRequest();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Start(atHashString effectName, u32 duration, bool bLoop, bool bUserControlsFade, bool bRunOnceOnly, int frameDelay, eAnimPostFXUser BANK_ONLY(user))
{
#if __BANK
	if (m_bBlockRequestFromUser && RemapForBlockCompare(m_blockedUserType) == RemapForBlockCompare(user))
	{
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] BLOCKED Start \"%s\" duration: %u, loop: %d, userFade: %d requested by %s", effectName.TryGetCStr(), duration, bLoop, bUserControlsFade, ms_debugUserTypeStr[user]);
		}
		return;
	}
#endif	

#if __BANK
	if (m_bDebugOutput && AnimPostFXIsSpecialAbility(effectName.GetHash()) == false)
	{
		Displayf("[ANIMPOSTFX] Start \"%s\" duration: %u, loop: %d, userFade: %d requested by %s", effectName.TryGetCStr(), duration, bLoop, bUserControlsFade, ms_debugUserTypeStr[user]);
	}
#endif

	const bool bIsRunning = IsRunning(effectName);
	// respect run-once flag
	if (bRunOnceOnly && bIsRunning)
	{
		return;
	}

	RegisteredAnimPostFXStack* pStack = Get(effectName);

	if( !bIsRunning && frameDelay > 0 )
	{
		pStack->SetLoopEnabled(bLoop);
		pStack->SetUserFadeOverride(bUserControlsFade);
		pStack->Start(duration);
		pStack->SetFrameDelay(frameDelay);

		AddToFrameDelayList(pStack);

		return;
	}

	// if we found the stack and it's already running, don't add it again,
	// just request a start
	if (pStack && (bIsRunning || AddToActiveList(pStack)))
	{
		pStack->SetLoopEnabled(bLoop);
		pStack->SetUserFadeOverride(bUserControlsFade);
		pStack->Start(duration);

		// Generate event if we've been assigned to a group...
		if (pStack->GetGroupId() != -1 && pStack->DoesEventTypeMatch(ANIMPOSTFX_EVENT_ON_START))
		{
		#if __BANK
			if (m_bDebugOutput)
			{
				Displayf("[ANIMPOSTFX][EVENT] Start event triggered for \"%s\"", effectName.TryGetCStr());
			}
		#endif	
			OnEventTriggered(pStack, ANIMPOSTFX_EVENT_ON_START);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Stop(atHashString effectName, eAnimPostFXUser BANK_ONLY(user))
{

#if __BANK
	if (m_bBlockRequestFromUser && RemapForBlockCompare(m_blockedUserType) == RemapForBlockCompare(user))
	{
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] BLOCKED Stop \"%s\" requested by %s", effectName.TryGetCStr(), ms_debugUserTypeStr[user]);
		}
		return;
	}
#endif	

#if __BANK
	if (m_bDebugOutput && AnimPostFXIsSpecialAbility(effectName.GetHash()) == false)
	{
		Displayf("[ANIMPOSTFX] Stop \"%s\" requested by %s", effectName.TryGetCStr(), ms_debugUserTypeStr[user]);
	}
#endif
	RegisteredAnimPostFXStack* pStack = Get(effectName);
	if (pStack)
	{
		pStack->Stop();

		// Generate event if we've been assigned to a group (no support for looped effects for now)...
		if (pStack->GetGroupId() != -1 && pStack->DoesEventTypeMatch(ANIMPOSTFX_EVENT_ON_STOP))
		{
		#if __BANK
			if (m_bDebugOutput)
			{
				Displayf("[ANIMPOSTFX][EVENT] Stop event triggered for \"%s\"", effectName.TryGetCStr());
			}
		#endif	
			OnEventTriggered(pStack, ANIMPOSTFX_EVENT_ON_STOP);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::SetUserFadeLevel(atHashString effectName, float level)
{
	RegisteredAnimPostFXStack* pStack = Get(effectName);
	if (pStack)
	{
		pStack->SetUserFadeLevel(level);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::StopAll(eAnimPostFXUser BANK_ONLY(user))
{ 
	if (IsIdle())
	{
	#if __BANK
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] StopAll requested by %s (NOTHING TO STOP)", ms_debugUserTypeStr[user]);
		}
	#endif	
		return;
	}

#if __BANK
	if (m_bBlockRequestFromUser && RemapForBlockCompare(m_blockedUserType) == RemapForBlockCompare(user))
	{
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] BLOCKED StopAll requested by %s", ms_debugUserTypeStr[user]);
		}
		return;
	}
#endif	

#if __BANK
	if (m_bDebugOutput)
	{
		Displayf("[ANIMPOSTFX] StopAll requested by %s", ms_debugUserTypeStr[user]);
	}
#endif
	m_bStopAllRequested = true; 
};

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXManager::IsRunning(atHashString effectName) const
{
	const RegisteredAnimPostFXStack* pStack = Get(effectName);
	if (pStack)
	{
		return (pStack->IsIdle() == false);
	}

	return false;
}

bool AnimPostFXManager::IsStartPending(atHashString effectName) const
{
	const RegisteredAnimPostFXStack* pStack = Get(effectName);
	if (pStack)
	{
		return pStack->IsStartPending();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
float AnimPostFXManager::GetCurrentTime(atHashString effectName) const
{
	const RegisteredAnimPostFXStack* pStack = Get(effectName);
	if (pStack)
	{
		return pStack->GetCurrentTime();
	}

	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXManager::AddToFrameDelayList(RegisteredAnimPostFXStack* pStack)
{
	if (m_FrameDelayStacks.GetNumFree() == 0 || pStack == NULL)
	{
		return false;
	}

	m_FrameDelayStacks.InsertSorted(pStack);
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXManager::AddToActiveList(RegisteredAnimPostFXStack* pStack)
{
	if (m_ActiveStacks.GetNumFree() == 0 || pStack == NULL)
	{
		return false;
	}

	m_ActiveStacks.InsertSorted(pStack);
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Init()
{
#if __BANK
	// This is here because ms_debugUserTypeStr is a private...
	CompileTimeAssert(NELEM(AnimPostFXManager::ms_debugUserTypeStr) == AnimPostFXManager::kCount);
#endif // __BANK

	m_RegisteredStacks.Reset();
	m_ActiveStacks.Init(8);
	m_FrameDelayStacks.Init(4);
	m_bStopAllRequested = false;

#if __BANK
	m_bBlockRequestFromUser = false;
	m_blockedUserType = kDefault;
#endif // __BANK
	
#if __BANK
	m_EditTool.Init();
	m_bDebugOutput = PARAM_animpostfx_debug.Get() ? true : false;
	m_bDebugDisplay = PARAM_animpostfx_debug.Get() ? true : false;

	m_userTypeComboIdx = 0;
	m_pUserTypeCombo = NULL;
	m_bDebugAllowPausing = false;

	// EditTool::OnLoadData will call "AnimPostFXManager::LoadMetaData"
	m_EditTool.OnLoadData();
#else
	LoadMetaData();
#endif 

	m_eventManager.Init();
	m_polledEventManager.Init();
#if GTA_REPLAY
	AnimPostFXManagerReplayEffectLookup::Init(this);
#endif	//GTA_REPLAY

}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Shutdown()
{
	m_eventManager.Shutdown();
	m_polledEventManager.Shutdown();

	m_bStopAllRequested = false;
	m_RegisteredStacks.Reset();
	m_ActiveStacks.Shutdown();
	m_FrameDelayStacks.Shutdown();
#if __BANK
	m_EditTool.Shutdown();
#endif 

#if GTA_REPLAY
	AnimPostFXManagerReplayEffectLookup::Shutdown();
#endif	//GTA_REPLAY
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Update()
{
	if (m_FrameDelayStacks.GetNumUsed() != 0)
	{
		ActiveStackNode* pNode = m_FrameDelayStacks.GetFirst()->GetNext();
		while(pNode != m_FrameDelayStacks.GetLast())
		{
			RegisteredAnimPostFXStack* pCurEntry = (pNode->item);
			ActiveStackNode* pLastNode = pNode;
			pNode = pNode->GetNext();

			pCurEntry->FrameDelayUpdate();

			if( pCurEntry->GetFrameDelay() == 0 )
			{
				if( AddToActiveList(pCurEntry) )
				{
					m_FrameDelayStacks.Remove(pLastNode);
				}
			}
		}
	}

	if (m_ActiveStacks.GetNumUsed() == 0)
	{
		m_polledEventManager.Update();
		return;
	}

	if (m_bStopAllRequested)
	{

	#if __BANK
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] StopAll");
		}
	#endif

		ActiveStackNode* pStopNode = m_ActiveStacks.GetFirst()->GetNext();
		while(pStopNode != m_ActiveStacks.GetLast())
		{
			RegisteredAnimPostFXStack* pCurEntryToStop = (pStopNode->item);
			pStopNode = pStopNode->GetNext();
			pCurEntryToStop->Stop();
		}
		m_Crossfade.Stop();
		m_bStopAllRequested = false;
	}

	m_Crossfade.Update();


	ActiveStackNode* pNode = m_ActiveStacks.GetFirst()->GetNext();
	while(pNode != m_ActiveStacks.GetLast())
	{
		RegisteredAnimPostFXStack* pCurEntry = (pNode->item);
		ActiveStackNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		if (pCurEntry->IsStartPending() == false && pCurEntry->IsIdle())
		{
			// Forget about it
			m_ActiveStacks.Remove(pLastNode);
		}
		else
		{

		#if __BANK
			if (m_bDebugDisplay)
			{
				grcDebugDraw::AddDebugOutput(Color_white, "[ANIMPOSTFX] \"%s\"", pCurEntry->m_Name.TryGetCStr());
			}
		#endif

			pCurEntry->Update();

			// Generate event if...
			u32 eventTime = pCurEntry->GetEventTimeMS();
			bool bShouldTriggerEvent = (pCurEntry->GetGroupId() != -1					&&	// group is valid
										eventTime != 0U									&&  // event time is valid
										pCurEntry->HasFrameEventTriggered() == false	&&	// we haven't triggered the event
										pCurEntry->IsStopPending() == false				&&	// stop request isn't pending
										pCurEntry->IsStartPending() == false);				// start request isn't pending

			if (bShouldTriggerEvent && pCurEntry->GetCurrentTimeMS() >=  eventTime && pCurEntry->DoesEventTypeMatch(ANIMPOSTFX_EVENT_ON_FRAME))
			{
			#if __BANK
				if (m_bDebugOutput)
				{
					Displayf("[ANIMPOSTFX][EVENT] Frame event triggered for \"%s\" at time: %u (set up for time: %u)", pCurEntry->m_Name.TryGetCStr(), pCurEntry->GetCurrentTimeMS(), eventTime);
				}
			#endif	
				// Mark event as dispatched...
				pCurEntry->MarkFrameEventTriggered(true);
				OnEventTriggered(pCurEntry, ANIMPOSTFX_EVENT_ON_FRAME);
			}
		}
	}

	m_polledEventManager.Update();

}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::StartCrossfade(atHashString effectNameIn, atHashString effectNameOut, u32 duration, eAnimPostFXUser BANK_ONLY(user))
{
#if __BANK
	if (m_bBlockRequestFromUser && RemapForBlockCompare(m_blockedUserType) == RemapForBlockCompare(user))
	{
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] BLOCKED Start Crossfade in: \"%s\" out: \"%s\" duration: %u requested by %s", effectNameIn.TryGetCStr(), effectNameOut.TryGetCStr(), duration, ms_debugUserTypeStr[user]);
		}
		return;
	}
#endif	

	m_Crossfade.Start(effectNameIn, effectNameOut, duration);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::StopCrossfade(eAnimPostFXUser BANK_ONLY(user))
{
#if __BANK
	if (m_bBlockRequestFromUser && RemapForBlockCompare(m_blockedUserType) == RemapForBlockCompare(user))
	{
		if (m_bDebugOutput)
		{
			Displayf("[ANIMPOSTFX] BLOCKED Stop Crossfade");
		}
		return;
	}
#endif

#if __BANK
	if (m_bDebugOutput)
	{
		Displayf("[ANIMPOSTFX] Stop Crossfade");
	}
#endif
	m_Crossfade.Stop();
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::CrossfadeHelper::Start(atHashString effectNameIn, atHashString effectNameOut, u32 duration)
{
#if __BANK
	if (ANIMPOSTFXMGR.m_bDebugOutput)
	{
		Displayf("[ANIMPOSTFX] Start Crossfade in: \"%s\" out: \"%s\" duration: %u", effectNameIn.TryGetCStr(), effectNameOut.TryGetCStr(), duration);
	}
#endif
	if (m_StartRequested || m_StopRequested || m_Active)
	{
		return;
	}

	m_InStackName = effectNameIn;
	m_OutStackName = effectNameOut;
	m_Duration = duration;
	m_StartRequested = true;
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::CrossfadeHelper::Stop()
{
	if (m_Active)
	{
		m_StopRequested = true;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::CrossfadeHelper::Update()
{
	if (m_StartRequested)
	{
		// Start the fade in stack
		ANIMPOSTFXMGR.Start(m_InStackName, 0U, true, false, false, 0U, AnimPostFXManager::kDefault);

		// Cache it
		m_pInStack = ANIMPOSTFXMGR.Get(m_InStackName);
		
		// Bail if it failed
		if (m_pInStack == NULL)
		{
			Reset();
			return;
		}

		// Setup fade level override
		m_pInStack->SetUserFadeOverride(true);
		m_pInStack->SetUserFadeLevel(0.0f);


		// Try caching the stack to fade out
		m_pOutStack = ANIMPOSTFXMGR.Get(m_OutStackName);

		if (m_pOutStack)
		{
			m_pOutStack->SetUserFadeOverride(true);
			m_pOutStack->SetUserFadeLevel(1.0f);
		}

		m_StartTime = fwTimer::GetSystemTimeInMilliseconds();
		m_StartRequested = false;
		m_Active = true;
		m_State = AnimatedPostFX::POSTFX_GOING_IN;
	}
	else if (m_StopRequested)
	{
		m_StartTime = fwTimer::GetSystemTimeInMilliseconds();
		m_StopRequested = false;
		m_StartRequested = false;
		m_Active = true;
		m_State = AnimatedPostFX::POSTFX_GOING_OUT;
	}
	else if (m_Active)
	{
		UpdateTiming();
	}

}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::CrossfadeHelper::UpdateTiming()
{
	u32 c_uCurrentTime = fwTimer::GetSystemTimeInMilliseconds();
	u32 timeDiff		= (c_uCurrentTime - m_StartTime);
	float fInterp = (float)(timeDiff) / (float)m_Duration;


	if (m_State == AnimatedPostFX::POSTFX_GOING_IN)
	{
		if (m_pInStack)
		{
			m_pInStack->SetUserFadeLevel(fInterp);

			if (m_pOutStack)
			{
				m_pOutStack->SetUserFadeLevel(1.0f-fInterp);
			}
		}
	}
	else if (m_State == AnimatedPostFX::POSTFX_ON_HOLD)
	{
		if (m_pInStack)
		{
			m_pInStack->SetUserFadeLevel(1.0f);

			if (m_pOutStack)
			{
				m_pOutStack->SetUserFadeLevel(0.0f);
			}
		}
	}
	else if (m_State == AnimatedPostFX::POSTFX_GOING_OUT)
	{

		if (m_pInStack)
		{
			m_pInStack->SetUserFadeLevel(1.0f-fInterp);

			if (m_pOutStack)
			{
				m_pOutStack->SetUserFadeLevel(fInterp);
			}

			if (fInterp >= 1.0f)
			{
				m_pInStack->SetUserFadeOverride(false);
				m_pInStack->Stop();

				if (m_pOutStack)
				{
					m_pOutStack->SetUserFadeOverride(false);
					m_pOutStack->SetUserFadeLevel(1.0f);
				}

				Reset();
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::ApplyModifiers(tcKeyframeUncompressed& keyframe REPLAY_ONLY(, eTimeCyclePhase currentPhase))
{
	if (m_ActiveStacks.GetNumUsed() == 0)
	{
		return;
	}

	ActiveStackNode* pNode = m_ActiveStacks.GetFirst()->GetNext();
	while(pNode != m_ActiveStacks.GetLast())
	{
		RegisteredAnimPostFXStack* pCurEntry = (pNode->item);

#if GTA_REPLAY
		bool shouldApply = true;
		if( currentPhase == TIME_CYCLE_PHASE_REPLAY_NORMAL )
		{
			if( AnimPostFXManagerReplayEffectLookup::IsStackDisabledForGameCam( pCurEntry ) )
			{
				shouldApply = false;
			}
		}
		else if( currentPhase == TIME_CYCLE_PHASE_REPLAY_FREECAM )
		{
			if( AnimPostFXManagerReplayEffectLookup::IsStackDisabledForFreeCam( pCurEntry ) )
			{
				shouldApply = false;
			}
		}
		if(shouldApply)
#endif	//GTA_REPLAY
		{
			pCurEntry->ApplyModifiers(keyframe);
		}
		pNode = pNode->GetNext();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Reset()
{
	ActiveStackNode* pNode = m_ActiveStacks.GetFirst()->GetNext();
	while(pNode != m_ActiveStacks.GetLast())
	{
		RegisteredAnimPostFXStack* pCurEntry = (pNode->item);
		ActiveStackNode* pLastNode = pNode;
		pNode = pNode->GetNext();
		pCurEntry->Reset();
		m_ActiveStacks.Remove(pLastNode);
	}

	m_Crossfade.Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::ClassInit()
{
	smp_Instance = rage_new AnimPostFXManager();
	Assert(smp_Instance);

	if (smp_Instance)
	{
		ANIMPOSTFXMGR.Init();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::ClassShutdown()
{
	ANIMPOSTFXMGR.Shutdown();

	delete smp_Instance;
	smp_Instance = NULL;
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void AnimPostFXManager::AddWidgets(rage::bkBank& bank )
{
	bank.PushGroup("Anim PostFX Manager", false);
		bank.AddToggle("Enable Debug Output", &m_bDebugOutput);
		bank.AddToggle("Enable Debug Display", &m_bDebugDisplay);
		bank.AddToggle("Allow Pausing", &m_bDebugAllowPausing);
		bank.AddToggle("Block Requests from User", &m_bBlockRequestFromUser);
		m_pUserTypeCombo = bank.AddCombo("User Type To Block", &m_userTypeComboIdx, NELEM(ms_debugUserTypeStr), &ms_debugUserTypeStr[0], datCallback(MFA(AnimPostFXManager::OnUserTypeToBlockSelected), (datBase*)this));

		m_EditTool.AddWidgets(bank);
		m_polledEventManager.AddWidgets(bank);
	bank.PopGroup();
}
#endif

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void AnimPostFXManager::OnUserTypeToBlockSelected()
{
	m_blockedUserType = (eAnimPostFXUser)m_userTypeComboIdx;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
bool AnimPostFXManager::LoadMetaData()
{
	bool bOk = false;
#if RSG_BANK
	if(PARAM_animpostfx_enable_metadata_preview.Get())
	{
		USE_DEBUG_MEMORY();

		const char* pSavePath = (m_EditTool.GetSavePath()[0] != 0 ? m_EditTool.GetSavePath() : ANIMPOSTFX_METADATA_FILE_SAVE_NAME);
		bOk = PARSER.LoadObject(pSavePath, "pso.meta", *this);
	}
#endif
	bOk = bOk || fwPsoStoreLoader::LoadDataIntoObject(ANIMPOSTFX_METADATA_FILE_LOAD_NAME, META_FILE_EXT, *this);

	// if data is loaded ok, run the postload callback the parser is supposed to call
	// tried to have the pso loader do this automatically by passing a flag to invoke the callback but didn't get too far
	if (bOk)
	{
		for (int i = 0; i < m_RegisteredStacks.GetCount(); i++)
		{
			if (m_RegisteredStacks[i].GetStack() != NULL)
				m_RegisteredStacks[i].GetStack()->PostLoadCallback();
		}
	}
	//PARSER.SaveObject(ANIMPOSTFX_METADATA_FILE_LOAD_NAME, "meta", this);
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
const RegisteredAnimPostFXStack* AnimPostFXManager::Get(atHashString name) const
{
	for (int i = 0; i < m_RegisteredStacks.GetCount(); i++)
	{
		if (m_RegisteredStacks[i].GetName() == name)
		{
			return &m_RegisteredStacks[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
RegisteredAnimPostFXStack* AnimPostFXManager::Get(atHashString name)
{
	for (int i = 0; i < m_RegisteredStacks.GetCount(); i++)
	{
		if (m_RegisteredStacks[i].GetName() == name)
		{
			return &m_RegisteredStacks[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK && !__FINAL
bool AnimPostFXManager::SaveMetaData()
{
	const char* pSavePath = (m_EditTool.GetSavePath() != NULL ? m_EditTool.GetSavePath() : ANIMPOSTFX_METADATA_FILE_SAVE_NAME);
	bool bOk = PARSER.SaveObject(pSavePath, "pso.meta", this);

	if (bOk == false)
	{
		Displayf("AnimPostFXManager::SaveMetaData: Failed to save data for \"%s\"", ANIMPOSTFX_METADATA_FILE_SAVE_NAME);
	}

	return bOk;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Add(RegisteredAnimPostFXStack& fxStack)
{
	m_RegisteredStacks.PushAndGrow(fxStack);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::Remove(atHashString name)
{
	for (int i = 0; i < m_RegisteredStacks.GetCount(); i++)
	{
		if (m_RegisteredStacks[i].GetName() == name)
		{
			m_RegisteredStacks.Delete(i);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
#if __BANK

atHashString AnimPostFXManager::EditTool::ms_newStackName = "[NEW]";
char AnimPostFXManager::EditTool::ms_testStackName[6][32];
char AnimPostFXManager::EditTool::ms_testCrossfadeName[2][32];
char AnimPostFXManager::EditTool::ms_savePath[256] = {0};

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::Init()
{
	m_RegStackNames.Reset();
	m_RegStackNames.Reserve(4);

	m_RegStackNamesComboIdx = -1;
	m_pRegStackNamesCombo = NULL;

	m_editableStack.ClearForEdit();
	UpdateRegStackNamesList(ANIMPOSTFXMGR.m_RegisteredStacks);

	m_testStackDuration[0] = 0U;
	m_testStackDuration[1] = 0U;
	m_testStackDuration[2] = 0U;
	m_testStackDuration[3] = 0U;
	m_testStackDuration[4] = 0U;
	m_testStackDuration[5] = 0U;
	ms_testStackName[0][0] = 0;
	ms_testStackName[1][0] = 0;
	ms_testStackName[2][0] = 0;
	ms_testStackName[3][0] = 0;
	ms_testStackName[4][0] = 0;
	ms_testStackName[5][0] = 0;
	m_bTestLoopEnabled[0] = false;
	m_bTestLoopEnabled[1] = false;
	m_bTestLoopEnabled[2] = false;
	m_bTestLoopEnabled[3] = false;
	m_bTestLoopEnabled[4] = false;
	m_bTestLoopEnabled[5] = false;

	ms_testCrossfadeName[0][0] = 0;
	ms_testCrossfadeName[1][0] = 0;
	m_testCrossfadeDuration = 0U;

	if (PARAM_animpostfx_save_path.Get())
	{
		const char* pSavePath = NULL;
		PARAM_animpostfx_save_path.Get(pSavePath);
		strcpy(&ms_savePath[0], pSavePath);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::Shutdown()
{
	m_RegStackNames.Reset();
	m_RegStackNamesComboIdx = -1;
	m_pRegStackNamesCombo = NULL;
	m_editableStack.Init();
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::AddWidgets(rage::bkBank& bank )
{
	bank.PushGroup("Edit FX Stacks");
		m_pRegStackNamesCombo = bank.AddCombo("Select FX Stack For Edit", &m_RegStackNamesComboIdx, m_RegStackNames.GetCount(), &m_RegStackNames[0], datCallback(MFA(AnimPostFXManager::EditTool::OnFXStackNameSelected), (datBase*)this));
		bank.AddSeparator();
		bank.PushGroup("Current FX Stack Selected For Edit", false);
			PARSER.AddWidgets(bank, &m_editableStack);
			bank.AddSeparator();
			bank.AddButton("Save Current FX Stack", datCallback(MFA(AnimPostFXManager::EditTool::OnSaveCurrentFXStack), (datBase*)this));
			bank.AddButton("Delete Current FX Stack", datCallback(MFA(AnimPostFXManager::EditTool::OnDeleteCurrentFXStack), (datBase*)this));
			bank.AddSeparator();
			bank.AddButton("SAVE DATA", datCallback(MFA(AnimPostFXManager::EditTool::OnSaveData), (datBase*)this));
			bank.AddButton("RELOAD DATA", datCallback(MFA(AnimPostFXManager::EditTool::OnLoadData), (datBase*)this));
			bank.AddSeparator();
		bank.PopGroup();
	bank.PopGroup();
		bank.PushGroup("Test FX Crossfade", false);
			bank.AddSeparator();
			bank.AddText("Fade In Effect:", &ms_testCrossfadeName[0][0], 32);
			bank.AddText("Fade Out Effect:", &ms_testCrossfadeName[1][0], 32);
			bank.AddSlider("Duration", &m_testCrossfadeDuration, 0U, 10000U, 1U);
			bank.AddSeparator();
			bank.AddButton("PLAY CROSSFADE", datCallback(MFA(AnimPostFXManager::EditTool::OnStartCrossfade), (datBase*)this));
			bank.AddSeparator();
			bank.AddButton("STOP CROSSFADE", datCallback(MFA(AnimPostFXManager::EditTool::OnStopCrossfade), (datBase*)this));
			bank.AddSeparator();
		bank.PopGroup();

		bank.PushGroup("Test FX Stacks", false);
		bank.AddSeparator();
		bank.AddText("[0] Test FX Stack Name:", &ms_testStackName[0][0], 32);
		bank.AddSlider("[0] Duration For Test FX Stack (0 for default duration)", &m_testStackDuration[0], 0U, 10000U, 1U);
		bank.AddToggle("[0] Loop Effect", &m_bTestLoopEnabled[0]);
		bank.AddSeparator();
		bank.AddButton("PLAY TEST FX STACK 0", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFX0), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("STOP TEST FX STACK 0", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFX0), (datBase*)this));
		bank.AddSeparator();

		bank.AddText("[1] Test FX Stack Name:", &ms_testStackName[1][0], 32);
		bank.AddSlider("[1] Duration For Test FX Stack (0 for default duration)", &m_testStackDuration[1], 0U, 10000U, 1U);
		bank.AddToggle("[1] Loop Effect", &m_bTestLoopEnabled[1]);
		bank.AddSeparator();
		bank.AddButton("[1] PLAY TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFX1), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("[1] STOP TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFX1), (datBase*)this));
		bank.AddSeparator();

		bank.AddText("[2] Test FX Stack Name:", &ms_testStackName[2][0], 32);
		bank.AddSlider("[2] Duration For Test FX Stack (0 for default duration)", &m_testStackDuration[2], 0U, 10000U, 1U);
		bank.AddToggle("[2] Loop Effect", &m_bTestLoopEnabled[2]);
		bank.AddSeparator();
		bank.AddButton("[2] PLAY TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFX2), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("[2] STOP TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFX2), (datBase*)this));
		bank.AddSeparator();

		bank.AddText("[3] Test FX Stack Name:", &ms_testStackName[3][0], 32);
		bank.AddSlider("[3] Duration For Test FX Stack (0 for default duration)", &m_testStackDuration[3], 0U, 10000U, 1U);
		bank.AddToggle("[3] Loop Effect", &m_bTestLoopEnabled[3]);
		bank.AddSeparator();
		bank.AddButton("[3] PLAY TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFX3), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("[3] STOP TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFX3), (datBase*)this));
		bank.AddSeparator();

		bank.AddText("[4] Test FX Stack Name:", &ms_testStackName[4][0], 32);
		bank.AddSlider("[4] Duration For Test FX Stack (0 for default duration)", &m_testStackDuration[4], 0U, 10000U, 1U);
		bank.AddToggle("[4] Loop Effect", &m_bTestLoopEnabled[4]);
		bank.AddSeparator();
		bank.AddButton("[4] PLAY TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFX4), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("[4] STOP TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFX4), (datBase*)this));
		bank.AddSeparator();

		bank.AddText("[5] Test FX Stack Name:", &ms_testStackName[5][0], 32);
		bank.AddSlider("[5] Duration For Test FX Stack (0 for default duration)", &m_testStackDuration[5], 0U, 10000U, 1U);
		bank.AddToggle("[5] Loop Effect", &m_bTestLoopEnabled[5]);
		bank.AddSeparator();
		bank.AddButton("[5] PLAY TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFX5), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("[5] STOP TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFX5), (datBase*)this));
		bank.AddSeparator();

		bank.AddSeparator();
		bank.AddButton("PLAY ALL TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStartTestStackFXAll), (datBase*)this));
		bank.AddButton("STOP ALL TEST FX STACK", datCallback(MFA(AnimPostFXManager::EditTool::OnStopTestStackFXAll), (datBase*)this));
		bank.AddSeparator();

	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::UpdateRegStackNamesList(const atArray<RegisteredAnimPostFXStack>& stacks)
{

	m_RegStackNames.Reset();
	m_RegStackNames.PushAndGrow(ms_newStackName.TryGetCStr());

	for (int i = 0; i < stacks.GetCount(); i++)
	{
		m_RegStackNames.PushAndGrow(stacks[i].GetName().TryGetCStr());
	} 

	m_RegStackNamesComboIdx = 0;

	if (m_pRegStackNamesCombo)
	{
		m_pRegStackNamesCombo->UpdateCombo("Select FX Stack", &m_RegStackNamesComboIdx, m_RegStackNames.GetCount(), &m_RegStackNames[0], datCallback(MFA(AnimPostFXManager::EditTool::OnFXStackNameSelected), (datBase*)this));
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnStartCrossfade()
{
	if (ms_testCrossfadeName[0][0] == 0 || ms_testCrossfadeName[1][0] == 0)
	{
		return;
	}

	atHashString nameIn(&(ms_testCrossfadeName[0][0]));
	atHashString nameOut(&(ms_testCrossfadeName[1][0]));

	ANIMPOSTFXMGR.StartCrossfade(nameIn, nameOut, m_testCrossfadeDuration,AnimPostFXManager::kDefault);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnStopCrossfade()
{
	ANIMPOSTFXMGR.StopCrossfade(AnimPostFXManager::kDefault);
}


//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnStartTestStackFX(int idx)
{
	if (ms_testStackName[idx][0] == 0)
	{
		return;
	}

	atHashString name(&(ms_testStackName[idx][0]));

	if (m_testStackDuration > 0)
	{
		ANIMPOSTFXMGR.Start(name, m_testStackDuration[idx], m_bTestLoopEnabled[idx], false, false, 0U, AnimPostFXManager::kDefault);
	}
	else
	{
		ANIMPOSTFXMGR.Start(name, 0U, m_bTestLoopEnabled[idx], false, false, 0U, AnimPostFXManager::kDefault);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnStopTestStackFX(int idx)
{
	if (ms_testStackName[idx][0] == 0)
	{
		return;
	}

	atHashString name(&(ms_testStackName[idx][0]));

	ANIMPOSTFXMGR.Stop(name,AnimPostFXManager::kDefault);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnFXStackNameSelected()
{

	int idx = m_RegStackNamesComboIdx;
	if (idx < 0 || idx > m_RegStackNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(m_RegStackNames[idx]);
	if (selectionName == ms_newStackName)
	{
		m_editableStack.ClearForEdit();
	}
	else
	{
		// otherwise, try finding the effect stack
		const RegisteredAnimPostFXStack* pStack = ANIMPOSTFXMGR.Get(selectionName);
		if (pStack != NULL) 
		{
			m_editableStack = *pStack;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnSaveCurrentFXStack()
{
	int idx = m_RegStackNamesComboIdx;
	if (idx < 0 || idx > m_RegStackNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected, add the entry to the collection
	atHashString selectionName = atHashString(m_RegStackNames[idx]);

	if (m_editableStack.Validate())
	{
		if (selectionName == ms_newStackName)
		{
			// only add to the collection is another preset with the same
			// name doesn't already exist
			if (ANIMPOSTFXMGR.Get(m_editableStack.GetName()) == NULL)
			{
				ANIMPOSTFXMGR.Add(m_editableStack);

			}
		}
		else
		{
			// otherwise, try finding the preset in the collection
			// and update it
			RegisteredAnimPostFXStack* pStack = ANIMPOSTFXMGR.Get(selectionName);
			if (pStack != NULL) 
			{
				*pStack = m_editableStack;
				if (pStack->GetStack())
					pStack->GetStack()->PostLoadCallback();
			}
		}
	}


}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnDeleteCurrentFXStack()
{
	int idx = m_RegStackNamesComboIdx;
	if (idx < 0 || idx > m_RegStackNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(m_RegStackNames[idx]);
	if (selectionName == ms_newStackName)
	{
		m_editableStack.ClearForEdit();
	}
	else
	{
		// otherwise, try finding the preset in the collection
		// and remove it
		const RegisteredAnimPostFXStack* pStack = ANIMPOSTFXMGR.Get(selectionName);
		if (pStack != NULL) 
		{
			ANIMPOSTFXMGR.Remove(pStack->GetName());
			m_editableStack.ClearForEdit();
		}
	}

	UpdateRegStackNamesList(ANIMPOSTFXMGR.m_RegisteredStacks);
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnSaveData()
{
#if !__FINAL
	ANIMPOSTFXMGR.SaveMetaData();
#endif
	UpdateRegStackNamesList(ANIMPOSTFXMGR.m_RegisteredStacks);
	m_editableStack.ClearForEdit();
}

//////////////////////////////////////////////////////////////////////////
//
void AnimPostFXManager::EditTool::OnLoadData()
{
	if (ANIMPOSTFXMGR.LoadMetaData())
	{
		UpdateRegStackNamesList(ANIMPOSTFXMGR.m_RegisteredStacks);
		m_editableStack.ClearForEdit();
	}
}

#endif // __BANK







//////////////////////////////////////////////////////////////////////////
//
PauseMenuPostFXManager* PauseMenuPostFXManager::smp_Instance = NULL;
#define PAUSEMENU_POSTFX_METADATA_FILE_NAME	"common:/data/ui/pausemenueffects" 

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::ClassInit()
{
	smp_Instance = rage_new PauseMenuPostFXManager();
	Assert(smp_Instance);

	if (smp_Instance)
	{
		PAUSEMENUPOSTFXMGR.Init();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void  PauseMenuPostFXManager::ClassShutdown()
{
	PAUSEMENUPOSTFXMGR.Shutdown();

	delete smp_Instance;
	smp_Instance = NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::Init()
{
	m_FadeInCheckTreshold = 0.0f;
	m_FadeOutCheckTreshold = 0.0f;

	LoadMetaData();
	m_State = PMPOSTFX_IDLE;
	m_FadeInRequested = false;
	m_FadeOutRequested = false;

	// Base game effects
	m_pFadeInEffects[PAUSEFX_FRANKLIN]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuFranklinIn",	0x80802e11));
	m_pFadeOutEffects[PAUSEFX_FRANKLIN]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuFranklinOut",0x388a0486));
	m_pFadeInEffects[PAUSEFX_MICHAEL]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuMichaelIn",	0xa0544755));
	m_pFadeOutEffects[PAUSEFX_MICHAEL]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuMichaelOut",	0x9cbeebee));
	m_pFadeInEffects[PAUSEFX_TREVOR]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuTrevorIn",	0x0b3ed728));
	m_pFadeOutEffects[PAUSEFX_TREVOR]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuTrevorOut",	0x24239ef8));
	m_pFadeInEffects[PAUSEFX_NEUTRAL]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuIn",			0x44dabe22));
	m_pFadeOutEffects[PAUSEFX_NEUTRAL]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuOut",		0x725941f7));

	/*
	// CnC Effects
	m_pFadeInEffects[PAUSEFX_CNC_COP]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuCopsIn",		0x786AA98A));
	m_pFadeOutEffects[PAUSEFX_CNC_COP]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuCopsOut",	0x6A4AFA62));
	m_pFadeInEffects[PAUSEFX_CNC_CROOK]	= ANIMPOSTFXMGR.Get(atHashString("PauseMenuCrooksIn",	0x833600C0));
	m_pFadeOutEffects[PAUSEFX_CNC_CROOK] = ANIMPOSTFXMGR.Get(atHashString("PauseMenuCrooksOut",	0x9B727EB3));
	*/

	// don't crash because of a pause menu effect please...
	for (int i = 0; i < PAUSEFX_COUNT; i++)
	{
		Assertf(m_pFadeInEffects[i] && m_pFadeOutEffects[i], "Pause menu effects failed to load");
	
		if (m_pFadeInEffects[i] == NULL)
		{
			m_pFadeInEffects[i] = &m_FallbackEffect;
		}

		if (m_pFadeOutEffects[i] == NULL)
		{
			m_pFadeOutEffects[i] = &m_FallbackEffect;
		}
	}


	m_pCurFadeInFX	= m_pFadeInEffects[PAUSEFX_NEUTRAL]->GetStack();
	m_pCurFadeOutFX	= m_pFadeOutEffects[PAUSEFX_NEUTRAL]->GetStack();


#if __BANK
	m_EditTool.Init();
#endif 
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::Shutdown()
{
#if __BANK
	m_EditTool.Shutdown();
#endif 
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::StartFadeIn()
{
	m_FadeInRequested = true;
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::StartFadeOut()
{
	PostFX::ResetAdaptedLuminance();
	m_FadeOutRequested = true;
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::Update()
{
	if (m_State == PMPOSTFX_IDLE)
	{
		return;
	}

	UpdateCurrentEffect();

	if (m_State == PMPOSTFX_FADING_IN)
	{
		m_pCurFadeInFX->Update();
	}
	else
	{
		m_pCurFadeOutFX->Update();	
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::UpdateCurrentEffect()
{
	const CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	if (pPlayerPed == NULL)
	{
		return;
	}

	if(NetworkInterface::IsGameInProgress())
	{
		if(CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
		{
			eArcadeTeam ePlayerTeam = pPlayerPed->GetPlayerInfo()->GetArcadeInformation().GetTeam();
			switch (ePlayerTeam)
			{
			case eArcadeTeam::AT_CNC_COP:
			/*	{
					m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_CNC_COP]->GetStack();
					m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_CNC_COP]->GetStack();
				}
				break;
			*/
			case eArcadeTeam::AT_CNC_CROOK:
			/*	{
					m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_CNC_CROOK]->GetStack();
					m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_CNC_CROOK]->GetStack();
				}
				break;
			*/
			default:
				{
					m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_NEUTRAL]->GetStack();
					m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_NEUTRAL]->GetStack();
				}
			}
		}
		else
		{
			eEffectType effectType = PAUSEFX_NEUTRAL;
			switch (CNewHud::GetCurrentCharacterColour())
			{
			case HUD_COLOUR_MICHAEL:
				effectType = PAUSEFX_MICHAEL;
				break;
			case HUD_COLOUR_FRANKLIN:
				effectType = PAUSEFX_FRANKLIN;
				break;
			case HUD_COLOUR_TREVOR:
				effectType = PAUSEFX_TREVOR;
				break;
			default:
				effectType = PAUSEFX_NEUTRAL;
			}

			m_pCurFadeInFX = m_pFadeInEffects[effectType]->GetStack();
			m_pCurFadeOutFX = m_pFadeOutEffects[effectType]->GetStack();
		}
	}
	else
	{
		ePedType pedType = pPlayerPed->GetPedType();

		switch (pedType)
		{
		case PEDTYPE_PLAYER_0: //MICHAEL
			{
				m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_MICHAEL]->GetStack();
				m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_MICHAEL]->GetStack();
			}
			break;
		case PEDTYPE_PLAYER_1: //FRANKLIN
			{
				m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_FRANKLIN]->GetStack();
				m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_FRANKLIN]->GetStack();
			}
			break;
		case PEDTYPE_PLAYER_2: //TREVOR
			{
				m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_TREVOR]->GetStack();
				m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_TREVOR]->GetStack();
			}
			break;
		default:
			{
				m_pCurFadeInFX = m_pFadeInEffects[PAUSEFX_NEUTRAL]->GetStack();
				m_pCurFadeOutFX = m_pFadeOutEffects[PAUSEFX_NEUTRAL]->GetStack();
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//
bool PauseMenuPostFXManager::IsFading() const
{
	if (m_State == PMPOSTFX_IDLE)
	{
		return false;
	}

	const float fadeInLevel = m_pCurFadeInFX->GetCurrentTime();
	const float fadeOutLevel = m_pCurFadeOutFX->GetCurrentTime();
	if (((m_State == PMPOSTFX_FADING_IN) && fadeInLevel < m_FadeInCheckTreshold) || 
		((m_State == PMPOSTFX_FADING_OUT) && fadeOutLevel < m_FadeOutCheckTreshold))
	{
		return false;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::Reset()
{
	m_State = PMPOSTFX_IDLE;
	m_pCurFadeInFX->Reset();
	m_pCurFadeOutFX->Reset();
	m_FadeInRequested = false;
	m_FadeOutRequested = false;
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::ApplyModifiers(tcKeyframeUncompressed& keyframe)
{
	if (m_State != PMPOSTFX_IDLE)
	{
		int scriptModIdx = g_timeCycle.GetScriptModifierIndex();

		const AnimatedPostFXStack& curFxStack = (m_State == PMPOSTFX_FADING_IN) ? *m_pCurFadeInFX : *m_pCurFadeOutFX;

#if	__BANK
		if (AnimPostFXManager::GetInstance().DebugDisplay())
		{
			grcDebugDraw::AddDebugOutput(Color_white, "[ANIMPOSTFX] Pause menu Post %d", m_State);
		}
		if (AnimPostFXManager::GetInstance().DebugOutput())
		{
			Displayf("[ANIMPOSTFX] Pause menu Post %d", m_State);
		}
#endif // __BANK

		// If script has pushed a TC mod that uses colour correction, skip over layers in the effect stack that do it too.
		// we could have script flag those cases, but it'd require more data in the animpostfx stacks (and exposing disabling individual
		// layers to script). Let's just hardcode the cases we know of for now.
		u32 numLayers = curFxStack.GetCount();
		const int gCCTVCamModIdx = 65;
		if (scriptModIdx == gCCTVCamModIdx)
		{
			numLayers = numLayers < 2 ? numLayers : 2;
		}

		for (u32 i = 0; i < numLayers; i++)
		{
			curFxStack.GetLayer(i).ApplyModifiers(keyframe, curFxStack.GetUserFadeLevel());
		}

	}

	// To prevent Pause menu Post 2 APFX being permanently on (bugstar://6532397)
	if ((m_State  == PMPOSTFX_FADING_OUT) && m_pCurFadeOutFX->IsIdle()) Reset();

	// to prevent game getting stuck blurred out if we request both fade OUT and IN at the same time, (B* 1423857), let fading out win.
	if (m_FadeOutRequested)
	{
		Reset();
		UpdateCurrentEffect();
		m_State = PMPOSTFX_FADING_OUT;
		m_pCurFadeOutFX->Start();
		m_FadeOutRequested = false;
	}
	else if (m_FadeInRequested)
	{
		Reset();
		UpdateCurrentEffect();
		m_State = PMPOSTFX_FADING_IN;
		m_pCurFadeInFX->SetLoopEnabled(true);
		m_pCurFadeInFX->Start();
		m_FadeInRequested = false;
	}
}



#if GTA_REPLAY

//////////////////////////////////////////////////////////////////////////
//
bool PauseMenuPostFXManager::AreAllPostFXAtIdle()
{
	bool	allIdle = true;
	for(int i=0; i<PAUSEFX_COUNT; i++)
	{
		if( m_pFadeInEffects[i]->IsIdle() == false )
		{
			allIdle = false;
			break;
		}
		if( m_pFadeOutEffects[i]->IsIdle() == false )
		{
			allIdle = false;
			break;
		}
	}
	return allIdle;
}

#endif	//GTA_REPLAY

//////////////////////////////////////////////////////////////////////////
//
bool PauseMenuPostFXManager::LoadMetaData()
{
	bool bOk = PARSER.LoadObject(PAUSEMENU_POSTFX_METADATA_FILE_NAME, "meta", *this);
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK && !__FINAL
bool PauseMenuPostFXManager::SaveMetaData()
{
	bool bOk = PARSER.SaveObject(PAUSEMENU_POSTFX_METADATA_FILE_NAME, "meta", this);

	if (bOk == false)
	{
		Displayf("PauseMenuPostFXManager::SaveMetaData: Failed to save data for \"%s\"", PAUSEMENU_POSTFX_METADATA_FILE_NAME);
	}

	return bOk;
}
#endif



#if __BANK

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::AddWidgets(rage::bkBank& bank )
{
	m_EditTool.AddWidgets(bank);
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::Init()
{
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::Shutdown()
{
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::AddWidgets(rage::bkBank& bank )
{
	bank.PushGroup("Pause Menu PostFX Edit");
		
		bank.AddSeparator();
		bank.AddButton("Trigger Fade In Effects", datCallback(MFA(PauseMenuPostFXManager::EditTool::OnTestFadeInEffects), (datBase*)this));
		bank.AddButton("Trigger Fade Out Effects", datCallback(MFA(PauseMenuPostFXManager::EditTool::OnTestFadeOutEffects), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("SAVE DATA", datCallback(MFA(PauseMenuPostFXManager::EditTool::OnSaveData), (datBase*)this));
		bank.AddButton("RELOAD DATA", datCallback(MFA(PauseMenuPostFXManager::EditTool::OnLoadData), (datBase*)this));
		bank.AddSeparator();
		bank.AddSlider("Fade In Check Bias", &(PAUSEMENUPOSTFXMGR.m_FadeInCheckTreshold), 0.0f, 1.0f, 0.001f);
		bank.AddSlider("Fade Out Check Bias", &(PAUSEMENUPOSTFXMGR.m_FadeOutCheckTreshold), 0.0f, 1.0f, 0.001f);

	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::OnSaveData()
{
	PAUSEMENUPOSTFXMGR.SaveMetaData();
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::OnLoadData()
{
	PAUSEMENUPOSTFXMGR.LoadMetaData();
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::OnTestFadeInEffects()
{
	PAUSEMENUPOSTFXMGR.StartFadeIn();
}

//////////////////////////////////////////////////////////////////////////
//
void PauseMenuPostFXManager::EditTool::OnTestFadeOutEffects()
{
	PAUSEMENUPOSTFXMGR.StartFadeOut();
}

#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
grcBlendStateHandle	OverlayTextRenderer::sm_textBlendState = grcStateBlock::BS_Invalid;

void OverlayTextRenderer::RenderText( char const * const text, Vector2 const& position, Vector2 const& scale, CRGBA const& color, s32 const style, Vector2 const& wrap, Vector2 const& c_maskTL,  Vector2 const& c_maskBR )
{
	if ( text == NULL || text[0] == 0 )
		return;

#if __BANK
	static const char *prevText = NULL;
	if (prevText != text)
	{
		Displayf("Text rendered by replay (pos %0.2f,%0.2f): %s", position.x, position.y, text);
		prevText = text;
	}
#endif // __BANK

	CText::Flush();

	Vector2 textPos( position );

	// setup the text layout/format
	CTextLayout stringLayout;

	stringLayout.SetOrientation(FONT_CENTRE);

	if (style < FONT_MAX_FONT_STYLES)  // ensure we swap out any unsupported fonts that are not in the global fontmap to the STANDARD font
	{
		stringLayout.SetStyle(style);
	}
	else
	{
		stringLayout.SetStyle(FONT_STYLE_STANDARD);
	}


	stringLayout.SetColor(color);
	stringLayout.SetScale(scale);
	stringLayout.SetWrap(wrap);

	if (c_maskTL.IsNonZero() || c_maskBR.IsNonZero())
	{
		stringLayout.SetScissor(c_maskTL.x, c_maskTL.y, c_maskBR.x, c_maskBR.y);
	}

	float const c_charHeight = CTextFormat::GetCharacterHeight(stringLayout);

	s32 const c_numLines = stringLayout.GetNumberOfLines(Vector2(0.5f, 0.0f), text);
	float const c_textboxHeight = c_charHeight * c_numLines;

	textPos.y -= (c_textboxHeight * 0.5f);

	RenderText( text, textPos, stringLayout );
}

void OverlayTextRenderer::RenderText( char const * const text, Vector2 const& position, CTextLayout& textLayout )
{
	if ( text == NULL || text[0] == 0 )
		return;

	// override Scaleform blend state, we don't want to write alpha
	sfScaleformManager* pScaleformMgr = CScaleformMgr::GetMovieMgr();
	sfRendererBase& scRenderer = pScaleformMgr->GetRageRenderer();
	scRenderer.OverrideBlendState(true);

	grcBlendStateHandle prevBlendState = grcStateBlock::BS_Active;
	grcStateBlock::SetBlendState( GetTextBlendState() );

	textLayout.Render( position, text );
	CText::Flush();

	// restore blend state
	grcStateBlock::SetBlendState(prevBlendState);
	scRenderer.OverrideBlendState(false);
}

void OverlayTextRenderer::InitializeTextBlendState()
{
	if( sm_textBlendState != grcStateBlock::BS_Invalid )
		return;

	grcBlendStateDesc blendStateBlockDesc;

	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendEnable = 1;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendOp = grcRSV::BLENDOP_ADD;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendEnable = 1;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].DestBlendAlpha = grcRSV::BLEND_ONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].SrcBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
	blendStateBlockDesc.IndependentBlendEnable = 1;

	sm_textBlendState = grcStateBlock::CreateBlendState(blendStateBlockDesc);
}

//////////////////////////////////////////////////////////////////////////
//
PhonePhotoEditor* PhonePhotoEditor::smp_Instance = NULL;
const atHashString s_PHONEPHOTOEDITOR_FRAME_TEXTURE_NAMES[5] = {	atHashString("Frame_Corner", 0x44e529d5), 
																	atHashString("Frame_Horizontal", 0x295245e1),
																	atHashString("Frame_Vertical", 0x5cb9dd7d),
																	atHashString("Frame_Detail_Left", 0xd590bc78),
																	atHashString("Frame_Detail_Right", 0x370f66fd) };

#if __BANK
char PhonePhotoEditor::sm_borderTxdName[PHOTO_TEXT_MAX_LENGTH*2] = "platform:/textures/graphics";
char PhonePhotoEditor::sm_topText[PHOTO_TEXT_MAX_LENGTH] = { 0 };
char PhonePhotoEditor::sm_bottomText[PHOTO_TEXT_MAX_LENGTH] = { 0 };
Color32	 PhonePhotoEditor::sm_topTextCol = Color32(1.0f, 1.0f, 1.0f, 1.0f);
Color32	PhonePhotoEditor::sm_bottomTextCol = Color32(1.0f, 1.0f, 1.0f, 1.0f);
Vector4	PhonePhotoEditor::sm_textPos = Vector4(0.5f, 0.25f, 0.5f, 0.75f);
s32 PhonePhotoEditor::sm_topTextStyle = 4;
s32	PhonePhotoEditor::sm_bottomTextStyle = 4;
float PhonePhotoEditor::sm_topTextSize = 2.5f;
float PhonePhotoEditor::sm_bottomTextSize = 2.0f;
bool PhonePhotoEditor::sm_bToggleActive = false;
const char* PhonePhotoEditor::sm_fontTypesStr[] = { "STANDARD", "CURSIVE", "ROCKSTAR_TAG", "LEADERBOARD", "CONDENSED", "FIXED_WIDTH_NUMBERS", "CONDENSED_NOT_GAMERNAME" };
bool PhonePhotoEditor::sm_bToggleGalleryEdit = false;
bool PhonePhotoEditor::sm_bDebugRender = false;
bool PhonePhotoEditor::sm_bTestSave = false;
char PhonePhotoEditor::sm_photoToEditName[64] = "PHOTO_TEXTURE_0_MAX";
s32 PhonePhotoEditor::sm_galleryEditTxdId = -1;
Vector2 PhonePhotoEditor::sm_debugRenderPos = Vector2(0.0f, 0.0f);
Vector2 PhonePhotoEditor::sm_debugRenderSize = Vector2(640.0f, 360.0f);
#endif

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::Init()
{
	m_bBorderTextureRefAdded = false;
	ResetTextParameters();

	Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::Shutdown()
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::Disable()
{
	if (m_bBorderTextureRefAdded && m_borderTextureTxd != -1)
	{
		gDrawListMgr->AddRefCountedModuleIndex(m_borderTextureTxd, &g_TxdStore);
	}

	Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::Reset()
{
	m_borderTextureTxd = -1;

	m_pDestRt = NULL;
	m_pBorderTexture[0] = NULL;
	m_pBorderTexture[1] = NULL;
	m_pBorderTexture[2] = NULL;
	m_pBorderTexture[3] = NULL;
	m_pBorderTexture[4] = NULL;

	m_topText[0] = 0;
	m_bottomText[0] = 0;

	m_borderTxdName.Null();			

	m_bActive = false;
	m_bUpdateBorderTexture = false;
	m_bReleaseCurrentBorderTexture = false;
	m_bUseText = false;
	m_bBorderTextureRefAdded = false;
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::SetText(const char* pTopText, const char* pBottomText, const Vector4& textPos, float topTextSize, float bottomTextSize, s32 topTextStyle, s32 bottomTextStyle, Color32 topTextCol, Color32 bottomTextCol)
{
	if ( pTopText )
	{
		safecpy( m_topText, pTopText );

#if USE_TEXT_CANVAS
		sEditedTextProperties &editedText = CTextTemplate::GetEditedText();
		safecpy(editedText.m_text, m_topText, NELEM(editedText.m_text));
		CTextTemplate::UpdateTemplate(0, editedText, "TEXT");
#endif // USE_TEXT_CANVAS

	}


	if ( pBottomText )
	{
		safecpy( m_bottomText, pBottomText );
	}

	m_textPos = textPos;
	m_topTextSize = topTextSize;
	m_bottomTextSize = bottomTextSize;
	m_topTextCol = topTextCol;
	m_bottomTextCol = bottomTextCol;

#if USE_CODE_TEXT
	if (topTextStyle >= (s32)FONT_STYLE_STANDARD && topTextStyle < (s32)FONT_MAX_FONT_STYLES)
#endif
		m_topTextStyle = topTextStyle;

#if USE_CODE_TEXT
	if (bottomTextStyle >= (s32)FONT_STYLE_STANDARD && bottomTextStyle < (s32)FONT_MAX_FONT_STYLES)
#endif
		m_bottomTextStyle = bottomTextStyle;
}


void PhonePhotoEditor::ResetTextParameters()
{
	m_topText[0] = 0;
	m_bottomText[0] = 0;
	m_topTextCol = Color32(1.0f, 1.0f, 1.0f, 1.0f);
	m_bottomTextCol = Color32(1.0f, 1.0f, 1.0f, 1.0f);
	m_textPos = Vector4(0.5f, 0.5f, 0.5f, 0.5f);
	m_topTextSize = 2.5f;
	m_bottomTextSize = 2.5f;
	m_topTextStyle = (s32)FONT_STYLE_CONDENSED;
	m_bottomTextStyle = (s32)FONT_STYLE_CONDENSED;
}

void PhonePhotoEditor::SetBorderTexture(const char* txdName, bool bDisableCurrentFrame) 
{ 
	if (bDisableCurrentFrame)
	{
		m_bReleaseCurrentBorderTexture = true;
		m_bUpdateBorderTexture = false;
		return;
	}

	m_bUpdateBorderTexture = true; 
	m_borderTxdName = txdName; 
}

void PhonePhotoEditor::ReleaseCurrentBorderTextures()
{
	if (m_bBorderTextureRefAdded && m_borderTextureTxd != -1)
	{
		gDrawListMgr->AddRefCountedModuleIndex(m_borderTextureTxd, &g_TxdStore);
	}

	m_pBorderTexture[0] = NULL;
	m_pBorderTexture[1] = NULL;
	m_pBorderTexture[2] = NULL;
	m_pBorderTexture[3] = NULL;
	m_pBorderTexture[4] = NULL;
	m_borderTextureTxd = -1;
}
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::Update()
{

	UpdateGalleryEdit();

	if (!m_bActive)
		return;

	if (m_bReleaseCurrentBorderTexture)
	{
		ReleaseCurrentBorderTextures();
		m_bReleaseCurrentBorderTexture = false;
	}
	else if (m_bUpdateBorderTexture)
	{
		// Release any previous one
		ReleaseCurrentBorderTextures();

		m_borderTextureTxd = g_TxdStore.FindSlotFromHashKey(m_borderTxdName.GetHash()).Get();

	#if __BANK
		// If needed, force the texture to load when we're using the widget
		if (sm_bToggleActive && m_borderTextureTxd != -1 && g_TxdStore.HasObjectLoaded(strLocalIndex(m_borderTextureTxd)) == false)
		{
			g_TxdStore.StreamingRequest(strLocalIndex(m_borderTextureTxd), STRFLAG_FORCE_LOAD);
			return;

		}
	#endif

		// Make sure the TXD is loaded...
		if (Verifyf(m_borderTextureTxd != -1 && g_TxdStore.HasObjectLoaded(strLocalIndex(m_borderTextureTxd)), "PhonePhotoEditor::Update: txd \"%s\" is not loaded", m_borderTxdName.TryGetCStr()))
		{
			fwTxd* pTxd = g_TxdStore.Get(strLocalIndex(m_borderTextureTxd));
			m_pBorderTexture[0] = pTxd->Lookup(s_PHONEPHOTOEDITOR_FRAME_TEXTURE_NAMES[0]);
			m_pBorderTexture[1] = pTxd->Lookup(s_PHONEPHOTOEDITOR_FRAME_TEXTURE_NAMES[1]);
			m_pBorderTexture[2] = pTxd->Lookup(s_PHONEPHOTOEDITOR_FRAME_TEXTURE_NAMES[2]);
			m_pBorderTexture[3] = pTxd->Lookup(s_PHONEPHOTOEDITOR_FRAME_TEXTURE_NAMES[3]);
			m_pBorderTexture[4] = pTxd->Lookup(s_PHONEPHOTOEDITOR_FRAME_TEXTURE_NAMES[4]);

			// Add a ref...
			if (m_pBorderTexture[0] != NULL && m_pBorderTexture[1] != NULL && m_pBorderTexture[2] != NULL)
			{
				g_TxdStore.AddRef(strLocalIndex(m_borderTextureTxd), REF_RENDER);
				m_bBorderTextureRefAdded = true;
			}
			// Or reset everything is the lookup fails...
			else
			{
				Assertf(0, "PhonePhotoEditor::Update: required textures don't exist in txd \"%s\"",  m_borderTxdName.TryGetCStr());
				Reset();
				return;
			}
			
			// Grab an overlay texture if there's one
			m_bUpdateBorderTexture = false;
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::Render()
{
	if (!m_bActive || m_pDestRt == NULL || m_bUpdateBorderTexture)
		return;


	grcTextureFactory::GetInstance().LockRenderTarget(0, m_pDestRt, NULL);
	RenderBorder();

#if USE_CODE_TEXT
	RenderText( m_pDestRt->GetWidth(), m_pDestRt->GetHeight() );
#endif

#if USE_TEXT_CANVAS
	RenderTextOverlay();
#endif

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::RenderBorder(bool bAdjustFor43)
{
	if (m_pBorderTexture[0] == NULL || m_pBorderTexture[1] == NULL || m_pBorderTexture[2] == NULL)
		return;

#if SUPPORT_MULTI_MONITOR
	if (bAdjustFor43)
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		grcViewport::GetCurrent()->SetWindow( mon.uLeft, mon.uTop, mon.getWidth(), mon.getHeight() );
	}
#endif //SUPPORT_MULTI_MONITOR

	float width = 0.25f;
	float height = 0.4f;

	float uoffset = 1.0f/80.0f;
	float uoffsetCorner = 1.0f/72.0f;
	float voffset = 1.0f/72.0f;

	float absMaxX = 0.5625f;

	if (bAdjustFor43 && !CHudTools::GetWideScreen())
	{
		absMaxX = 0.75f;
	}
	
	CSprite2d sprite;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

	// Draw corners
	sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	sprite.BeginCustomList(CSprite2d::CS_BLIT, m_pBorderTexture[0]);
		
		// Top left
		float x0 = -absMaxX;
		float y0 = 1.0f;
		float x1 = -absMaxX + width;
		float y1 = 1.0f - height;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 0.0f, 1.0f-uoffsetCorner, 1.0f-voffset, 
							Color32(255, 255, 255 ,255));

		// Top right
		x0 = absMaxX - width;
		y0 = 1.0f;
		x1 = absMaxX;
		y1 = 1.0f - height;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							1.0f-uoffsetCorner, 0.0f, 0.0f, 1.0f-voffset, 
							Color32(255, 255, 255 ,255));

		// Bottom right
		x0 = absMaxX - width;
		y0 = -1.0f + height;
		x1 = absMaxX;
		y1 = -1.0f;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							1.0f-uoffsetCorner, 1.0f-voffset, 0.0f, 0.0f, 
							Color32(255, 255, 255 ,255));

		// Bottom left
		x0 = -absMaxX;
		y0 = -1.0f + height;
		x1 = -absMaxX + width;
		y1 = -1.0f;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 1.0f-uoffsetCorner, 1.0f-voffset, 0.0f, 
							Color32(255, 255, 255 ,255));

	sprite.EndCustomList();


	// Draw horizontals
	sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	sprite.BeginCustomList(CSprite2d::CS_BLIT, m_pBorderTexture[1]);

		// Top
		x0 = -absMaxX + width;
		y0 = 1.0f;
		x1 = absMaxX - width;
		y1 = 1.0f - height;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 0.0f, 3.0f, 1.0f-voffset, 
							Color32(255, 255, 255 ,255));

		// Bottom
		x0 = -absMaxX + width;
		y0 = -1.0f + height;
		x1 = absMaxX - width;
		y1 = -1.0f;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 1.0f-voffset, 3.0f, 0.0f, 
							Color32(255, 255, 255 ,255));

	sprite.EndCustomList();

	// Draw verticals
	sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	sprite.BeginCustomList(CSprite2d::CS_BLIT, m_pBorderTexture[2]);

		// Left
		x0 = -absMaxX;
		y0 = 1.0f - height;
		x1 = -absMaxX + width;
		y1 = -1.0f + height;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 0.0f, 1.0f-uoffset, 3.0f, 
							Color32(255, 255, 255 ,255));

		// Right
		x0 = absMaxX - width;
		y0 = 1.0f - height;
		x1 = absMaxX;
		y1 = -1.0f + height;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							1.0f-uoffset, 0.0f, 0.0f, 3.0f, 
							Color32(255, 255, 255 ,255));

	sprite.EndCustomList();


	// Draw top-left detail texture if there's one
	if (m_pBorderTexture[3] != NULL)
	{
		sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		sprite.BeginCustomList(CSprite2d::CS_BLIT, m_pBorderTexture[3]);



		float uoffsetDetail = 1.0f/(float)m_pBorderTexture[3]->GetWidth();
		float voffsetDetail = 1.0f/(float)m_pBorderTexture[3]->GetHeight();
		float widthDetail = ((float)m_pBorderTexture[3]->GetWidth()/640.0f)*2.0f;
		float heightDetail = ((float)m_pBorderTexture[3]->GetHeight()/360.0f)*2.0f;

		float x0 = -absMaxX;
		float y0 = 1.0f;
		float x1 = -absMaxX + widthDetail;
		float y1 = 1.0f - heightDetail;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 0.0f, 1.0f-uoffsetDetail, 1.0f-voffsetDetail,
							Color32(255, 255, 255 ,255));


		sprite.EndCustomList();
	}

	// Draw top-right detail texture if there's one
	if (m_pBorderTexture[4] != NULL)
	{
		sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		sprite.BeginCustomList(CSprite2d::CS_BLIT, m_pBorderTexture[4]);

		float uoffsetDetail = 1.0f/(float)m_pBorderTexture[4]->GetWidth();
		float voffsetDetail = 1.0f/(float)m_pBorderTexture[4]->GetHeight();
		float widthDetail = ((float)m_pBorderTexture[4]->GetWidth()/640.0f)*2.0f;
		float heightDetail = ((float)m_pBorderTexture[4]->GetHeight()/360.0f)*2.0f;

		x0 = absMaxX - widthDetail;
		y0 = 1.0f;
		x1 = absMaxX;
		y1 = 1.0f - heightDetail;
		grcDrawSingleQuadf(	x0,	y0,
							x1, y1,
							0.0f, 
							0.0f, 0.0f, 1.0f-uoffsetDetail, 1.0f-voffsetDetail,
							Color32(255, 255, 255 ,255));
		
		sprite.EndCustomList();

	}

	// Draw cropping areas
	sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	sprite.BeginCustomList(CSprite2d::CS_BLIT, grcTexture::NoneBlack);

	grcDrawSingleQuadf(	-1.0f, 1.0f, -absMaxX+(2.0f/1280.0f), -1.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0, 0, 0 ,255));

	grcDrawSingleQuadf(	absMaxX-(2.0f/1280.0f), 1.0f, 1.0f, -1.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0, 0, 0 ,255));

	sprite.EndCustomList();
}


//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::RenderGalleryEdit()
{

	if (m_galleryEdit.m_bEditing == false 
		PPU_ONLY(&& m_galleryEdit.m_requestFlags.bSave == false))
		return;

	if (m_galleryEdit.m_pSrcImg == NULL)
		return;

	PF_PUSH_MARKER("MemeEditor");

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_galleryEdit.m_pRt, NULL);

	CSprite2d sprite;
	sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	sprite.BeginCustomList(CSprite2d::CS_BLIT, m_galleryEdit.m_pSrcImg);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f, Color32(255, 255, 255 ,255));
	sprite.EndCustomList();

#if USE_CODE_TEXT
	RenderText( m_galleryEdit.m_pRt->GetWidth(), m_galleryEdit.m_pRt->GetHeight() );
#endif

#if USE_TEXT_CANVAS
	RenderTextOverlay();
#endif

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = XENON_SWITCH(true, false);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

	PF_POP_MARKER();

#if __PPU
	if (m_galleryEdit.m_requestFlags.bSave)
		SaveToJPEGBuffer();
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::RenderText( int const destWidth, int const destHeight )
{
	if (m_topText[0] == 0 && m_bottomText[0] == 0)
		return;

	{
		GRC_VIEWPORT_AUTO_PUSH_POP();
		m_galleryEdit.m_viewport.SetWindow( 0, 0, destWidth, destHeight );
		m_galleryEdit.m_viewport.Screen();
		grcViewport::SetCurrent( &m_galleryEdit.m_viewport );

		// override Scaleform blend state, we don't want to write alpha
		sfScaleformManager* pScaleformMgr = CScaleformMgr::GetMovieMgr();
		sfRendererBase& scRenderer = pScaleformMgr->GetRageRenderer();
		scRenderer.OverrideBlendState(true);

		grcBlendStateHandle prevBlendState = grcStateBlock::BS_Active;
		grcStateBlock::SetBlendState( GetTextBlendState() );
		bool bFlushText = false;

		// TOP TEXT //////////////////////////////////////////////////////////////////////////

		// setup the text layout/format
		Color32 textCol = m_topTextCol;
		Vector2 textPos = Vector2(m_textPos.x, m_textPos.y);
		Vector2 textSize = Vector2(m_topTextSize, m_topTextSize);

		const char* pStr = &(m_topText[0]);
		CTextLayout stringLayout;
		float charHeight;

		if (pStr != NULL)
		{
			stringLayout.SetOrientation(FONT_CENTRE);
			stringLayout.SetStyle(m_topTextStyle);
			stringLayout.SetColor(textCol);
			stringLayout.SetScale(&textSize);

			charHeight = CTextFormat::GetCharacterHeight(stringLayout);
			textPos.y -= charHeight * 0.5f;

			stringLayout.Render(&textPos, pStr);
			bFlushText = true;
		}

		// BOTTOM TEXT //////////////////////////////////////////////////////////////////////////
		// setup the text layout/format
		textCol = m_bottomTextCol;
		textPos = Vector2(m_textPos.z, m_textPos.w);
		textSize = Vector2(m_bottomTextSize, m_bottomTextSize);

		pStr = &(m_bottomText[0]);

		if (pStr != NULL)
		{
			stringLayout.SetOrientation(FONT_CENTRE);
			stringLayout.SetStyle(m_bottomTextStyle);
			stringLayout.SetColor(textCol);
			stringLayout.SetScale(&textSize);

			charHeight = CTextFormat::GetCharacterHeight(stringLayout);
			textPos.y -= charHeight * 0.5f;

			stringLayout.Render(&textPos, pStr);
			bFlushText = true;
		}
	
		if (bFlushText)
			CText::Flush();

		// restore blend state
		grcStateBlock::SetBlendState(prevBlendState);
		scRenderer.OverrideBlendState(false);
	}
}

void PhonePhotoEditor::RenderTextOverlay()
{
	if (m_topText[0] == 0 && m_bottomText[0] == 0)
		return;

	sfRendererBase& scRenderer = CScaleformMgr::GetMovieMgr()->GetRageRenderer();
	scRenderer.OverrideBlendState(true); 

	grcBlendStateHandle prevBlendState = grcStateBlock::BS_Normal;
	grcStateBlock::SetBlendState( GetTextBlendState() );

	CTextTemplate::RenderSnapmatic();

	// restore blend state 
	grcStateBlock::SetBlendState(prevBlendState); 
	scRenderer.OverrideBlendState(false);
}


#if RSG_PC
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::DeviceLost()
{
}


//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::DeviceReset()
{
	if (smp_Instance)
	{
		PHONEPHOTOEDITOR.InitGalleryEdit(true);
	}
}
#endif	//	RSG_PC


//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::ClassInit()
{
	smp_Instance = rage_new PhonePhotoEditor();
	Assert(smp_Instance);

	if (smp_Instance)
	{
		PHONEPHOTOEDITOR.Init();
		PHONEPHOTOEDITOR.InitGalleryEdit(false);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::InitGalleryEdit(bool bReInit)
{
	if (!bReInit)
	{
		m_galleryEdit.Reset();
	}

	u32 width;
	u32 height;
	CHighQualityScreenshot::GetDefaultScreenshotSize( width, height );

#if __XENON

	grcTextureFactory::CreateParams qparams;	
	qparams.Format		= grctfA8R8G8B8;
	qparams.HasParent	= true;
	qparams.Parent		= CRenderTargets::GetUIBackBuffer();
	
	m_galleryEdit.m_pRt	= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_PERLIN_NOISE, "MemeBuffer", grcrtPermanent, width, height, 32, &qparams, kMemeEditorHeap);
	m_galleryEdit.m_pRt->AllocateMemoryFromPool();

#elif __PS3

	grcTextureFactory::CreateParams params;
	params.HasParent			= true;
	params.Parent				= NULL;
	params.EnableCompression	= false;
	params.Format				= grctfA8R8G8B8;

	m_galleryEdit.m_pRt	= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "MemeBuffer", grcrtPermanent, width, height, 32, &params, 10);
	m_galleryEdit.m_pRt->AllocateMemoryFromPool();

#elif __WIN32PC || RSG_DURANGO || RSG_ORBIS

	grcTextureFactory::CreateParams params;
	params.Multisample  = 0;
	params.UseFloat     = true;
#if RSG_ORBIS
	params.Format       = grctfA8R8G8B8;
	params.ForceNoTiling = true;
#else
	params.Format       = grctfA8B8G8R8;
#endif
	params.Lockable     = true;

	m_galleryEdit.m_pRt = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "MemeBuffer ", grcrtPermanent, width, height, 32, &params, 0 WIN32PC_ONLY(, m_galleryEdit.m_pRt) );

#endif
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::ShutdownGalleryEdit()
{
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::ClassShutdown()
{
	PHONEPHOTOEDITOR.Shutdown();
	PHONEPHOTOEDITOR.ShutdownGalleryEdit();

	delete smp_Instance;
	smp_Instance = NULL;

}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void PhonePhotoEditor::OnToggleActive()
{
	if (sm_bToggleActive)
	{
		PHONEPHOTOEDITOR.Enable();
		PHONEPHOTOEDITOR.SetBorderTexture(&sm_borderTxdName[0], false);
		PHONEPHOTOEDITOR.SetText(&sm_topText[0], &sm_bottomText[0], sm_textPos, sm_topTextSize, sm_bottomTextSize, sm_topTextStyle, sm_bottomTextStyle, sm_topTextCol, sm_bottomTextCol);
	}
	else
	{
		PHONEPHOTOEDITOR.Disable();
	}

}
#endif

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void PhonePhotoEditor::OnTextChange()
{
	PHONEPHOTOEDITOR.SetText(&sm_topText[0], &sm_bottomText[0], sm_textPos, sm_topTextSize, sm_bottomTextSize, sm_topTextStyle, sm_bottomTextStyle, sm_topTextCol, sm_bottomTextCol);
}
#endif 

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void PhonePhotoEditor::AddWidgets(rage::bkBank& bank )
{
	AddWidgetsForGalleryEdit(bank);

	bank.PushGroup("Phone Photo Editor");
		bank.AddToggle("Enable", &(sm_bToggleActive), datCallback(MFA(PhonePhotoEditor::OnToggleActive), (datBase*)this));
		bank.AddSeparator();
		bank.AddText("Top Text:", &(sm_topText[0]), PHOTO_TEXT_MAX_LENGTH-1, datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddText("Bottom Text:", &(sm_bottomText[0]), PHOTO_TEXT_MAX_LENGTH-1, datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddColor("Top Text Color:", &(sm_topTextCol), datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddColor("Bottom Text Color:", &(sm_bottomTextCol), datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddSlider("Top Text Size:", &(sm_topTextSize), 0.01f, 10.0f, 0.01f, datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddSlider("Bottom Text Size:", &(sm_bottomTextSize), 0.01f, 100.0f, 0.01f, datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddVector("Text Position:", &(sm_textPos), 0.0f, 1.0f, 0.01f, datCallback(MFA(PhonePhotoEditor::OnTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddText("Border TXD Name:", &(sm_borderTxdName[0]), PHOTO_TEXT_MAX_LENGTH*2-1);
		bank.AddSeparator();
	bank.PopGroup();

}
#endif

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::StartEditing(s32 txdId)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// Check there are no pending requests and that we're available
	if (m_galleryEdit.m_bRequestsPending != 0 || m_galleryEdit.m_bEditing != false || m_galleryEdit.m_bWaitingForUser != false)
	{
		Assertf(0, "PhonePhotoEditor::StartEditing: already in use (editing: %d saving: %d, releasing: %d)", m_galleryEdit.m_bEditing || m_galleryEdit.m_bEditing, m_galleryEdit.m_requestFlags.bSave || m_galleryEdit.m_bWaitingForUser, m_galleryEdit.m_requestFlags.bRelease );
		return false;
	}

	// Assume everything's going to be alright
	bool bOk = true;

	// Try allocating memory for the JPEG buffer
	if (Verifyf(AllocateGalleryEditJPEGBuffer(), "PhonePhotoEditor::StartEditing: failed to allocate memory for JPEG buffer"))
	{
		// Try setting up the source image
		if (Verifyf(ProcessGalleryEditSourceImage(txdId), "PhonePhotoEditor::StartEditing: failed to add ref to source image (txd: %d)", txdId))
		{
			// We're now editing!
			m_galleryEdit.m_bEditing = true;
		}
		else
		{
			// Release JPEG buffer
			ReleaseGalleryEditJPEGBuffer();
			bOk = false;
		}
	}

	// No pending requests
	m_galleryEdit.m_bRequestsPending = 0;

	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::IsEditingAvailable() const
{
	return (m_galleryEdit.m_bRequestsPending == 0 && m_galleryEdit.m_bEditing == false && m_galleryEdit.m_bWaitingForUser == false);
}

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::FinishEditing()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// Verify something was going on
	if (Verifyf(m_galleryEdit.m_bWaitingForUser || m_galleryEdit.m_bEditing || m_galleryEdit.m_requestFlags.bSave, "PhonePhotoEditor::FinishEditing: editor was not in use or there's a pending save request"))
	{
		// Request release
		m_galleryEdit.m_requestFlags.bRelease = true;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::RequestSave(OnPhotoEditorSaveCallback pCallback)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// Verify we're already editing something AND that there are no pending requests
	if (Verifyf(m_galleryEdit.m_bEditing && m_galleryEdit.m_bRequestsPending == false, "PhonePhotoEditor::RequestSave: there are pending requests"))
	{
		// Verify we've got a functor for the callback
		if (Verifyf(pCallback != 0, "PhonePhotoEditor::RequestSave: functor is NULL"))
		{
			m_galleryEdit.m_pOnSaveFnc = pCallback;
			m_galleryEdit.m_requestFlags.bSave = true;
			m_galleryEdit.m_bEditing = false; // no more editing
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::UpdateGalleryEdit()
{
	if (m_galleryEdit.m_requestFlags.bRelease)
	{
		ResetGalleryEditState();
		m_galleryEdit.m_bRequestsPending = 0;
		m_galleryEdit.m_bEditing = false;
		m_galleryEdit.m_bWaitingForUser = false;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::ResetGalleryEditState()
{
	ReleaseGalleryEditJPEGBuffer();
	ReleaseGalleryEditSourceImage();
}

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::AllocateGalleryEditJPEGBuffer()
{
	MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
	m_galleryEdit.m_pJPEGBuffer = (u8*) strStreamingEngine::GetAllocator().Allocate(m_galleryEdit.m_JPEGBufferSize, 16, MEMTYPE_RESOURCE_VIRTUAL);

	return (m_galleryEdit.m_pJPEGBuffer != NULL);
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::ReleaseGalleryEditJPEGBuffer()
{
	if (m_galleryEdit.m_pJPEGBuffer)
	{
		MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
		strStreamingEngine::GetAllocator().Free(m_galleryEdit.m_pJPEGBuffer);
		m_galleryEdit.m_pJPEGBuffer = NULL;
		m_galleryEdit.m_JPEGBufferSize = CPhotoBuffer::GetDefaultSizeOfJpegBuffer();
		m_galleryEdit.m_JPEGActualSize = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::ReleaseGalleryEditSourceImage()
{
	if (m_galleryEdit.m_bSrcImgRefAdded && m_galleryEdit.m_SrcImgTxd != -1)
	{
		gDrawListMgr->AddRefCountedModuleIndex(m_galleryEdit.m_SrcImgTxd, &g_TxdStore);
		m_galleryEdit.m_pSrcImg = NULL;
		m_galleryEdit.m_bSrcImgRefAdded = false;
		m_galleryEdit.m_SrcImgTxd = -1;
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::ProcessGalleryEditSourceImage(s32 txdId)
{
	if (txdId == -1 || g_TxdStore.IsValidSlot(strLocalIndex(txdId)) == false)
	{
		return false;
	}

	fwTxd* pTxd = g_TxdStore.Get(strLocalIndex(txdId));
	if (pTxd == NULL || pTxd->GetCount() == 0)
	{
		return false;
	}

	g_TxdStore.AddRef(strLocalIndex(txdId), REF_RENDER);
	m_galleryEdit.m_pSrcImg = pTxd->GetEntry(0);
	m_galleryEdit.m_SrcImgTxd = txdId;
	m_galleryEdit.m_PendingSrcImgTxd = -1;
	m_galleryEdit.m_bSrcImgRefAdded = true;
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::UpdateSafe()
{
#if __PPU
	if (m_galleryEdit.m_bNeedToIssueCallback == true)
	{
		// Let user know about save result
		if (m_galleryEdit.m_pOnSaveFnc)
		{
			m_galleryEdit.m_pOnSaveFnc( m_galleryEdit.m_pJPEGBuffer, m_galleryEdit.m_JPEGActualSize, !m_galleryEdit.m_bWasSaveOk );
		}

		m_galleryEdit.m_bNeedToIssueCallback = false;
	}
#else
	if (m_galleryEdit.m_requestFlags.bSave)
		SaveToJPEGBuffer();
#endif
}

//////////////////////////////////////////////////////////////////////////
//
bool PhonePhotoEditor::SaveToJPEGBuffer()
{

	Assertf(CPauseMenu::GetPauseRenderPhasesStatus(), "Rendering is not frozen");

	if (m_galleryEdit.m_pJPEGBuffer == NULL)
		return false;

#if __PPU
	grcFenceHandle fence = GRCDEVICE.InsertFence();
	GRCDEVICE.GPUBlockOnFence(fence);
#endif

	m_galleryEdit.m_JPEGActualSize = 0U;

	bool bOk =	CHighQualityScreenshot::StoreScreenShot(NULL,											// No need to pass a file name
														m_galleryEdit.m_pRt,							// Source image 
														true,											// Save to memory buffer 
														m_galleryEdit.m_pJPEGBuffer,					// Our memory buffer
														m_galleryEdit.m_JPEGBufferSize,					// Current JPEG buffer size 
														&m_galleryEdit.m_JPEGActualSize,				// Actual JPEG buffer size 
														true,											// Save as JPEG		
														CHighQualityScreenshot::GetDefaultQuality() );	// Quality	

#if __PPU
	m_galleryEdit.m_bNeedToIssueCallback = true;
	m_galleryEdit.m_bWasSaveOk = bOk;
#else
	// Let user know about save result
	if (m_galleryEdit.m_pOnSaveFnc)
	{
		m_galleryEdit.m_pOnSaveFnc( m_galleryEdit.m_pJPEGBuffer, m_galleryEdit.m_JPEGActualSize, bOk );
	}
#endif

	m_galleryEdit.m_requestFlags.bSave = false;
	m_galleryEdit.m_bWaitingForUser = true;

	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::GalleryEditData::Reset()
{
	m_bRequestsPending = 0;
	m_bEditing = false;
	m_bWaitingForUser = false;
	m_pRt = NULL; 
	m_pSrcImg = NULL; 
	m_pJPEGBuffer = NULL; 
	m_JPEGBufferSize = CPhotoBuffer::GetDefaultSizeOfJpegBuffer();
	m_JPEGActualSize = 0;
	m_SrcImgTxd = -1; 
	m_PendingSrcImgTxd = -1;
	m_bSrcImgRefAdded = false; 
	m_pOnSaveFnc = NULL;
	m_bNeedToIssueCallback = 0;
	m_bWasSaveOk = 0;
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::AddWidgetsForGalleryEdit(rage::bkBank& bank)
{

	bank.PushGroup("Gallery Photo Edit");
		bank.AddToggle("Toggle Editing", &sm_bToggleGalleryEdit, datCallback(MFA(PhonePhotoEditor::OnToggleGalleryEdit), (datBase*)this));
		bank.AddToggle("Toggle Debug Render", &sm_bDebugRender);
		bank.AddVector("Debug Render Position", &sm_debugRenderPos, 0.0f, 1280.0f, 1.0f);
		bank.AddVector("Debug Render Size", &sm_debugRenderSize, 0.0f, 1280.0f, 0.5f);
		bank.AddSeparator();
		bank.AddToggle("Test Save", &sm_bTestSave, datCallback(MFA(PhonePhotoEditor::OnGalleryEditSave), (datBase*)this));
		bank.AddSeparator();
		bank.AddText("Photo TXD Name:", &(sm_photoToEditName[0]), 63, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextureNameChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddText("Top Text:", &(sm_topText[0]), PHOTO_TEXT_MAX_LENGTH-1, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddText("Bottom Text:", &(sm_bottomText[0]), PHOTO_TEXT_MAX_LENGTH-1, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddCombo("Top Text Font", &sm_topTextStyle, FONT_MAX_FONT_STYLES-2, sm_fontTypesStr, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddCombo("Bottom Text Font", &sm_bottomTextStyle, FONT_MAX_FONT_STYLES-2, sm_fontTypesStr, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddColor("Top Text Color:", &(sm_topTextCol), datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddColor("Bottom Text Color:", &(sm_bottomTextCol), datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddSlider("Top Text Size:", &(sm_topTextSize), 0.01f, 10.0f, 0.01f, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddSlider("Bottom Text Size:", &(sm_bottomTextSize), 0.01f, 100.0f, 0.01f, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddSeparator();
		bank.AddVector("Text Position:", &(sm_textPos), 0.0f, 1.0f, 0.01f, datCallback(MFA(PhonePhotoEditor::OnGalleryEditTextChange), (datBase*)this));
		bank.AddSeparator();
	bank.PopGroup();

}
#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::OnToggleGalleryEdit()
{
	if (sm_bToggleGalleryEdit)
	{

		if (sm_galleryEditTxdId == -1 && sm_photoToEditName[0] != 0)
		{
			atHashString hashStr(&sm_photoToEditName[0]);
			sm_galleryEditTxdId = g_TxdStore.FindSlotFromHashKey(hashStr.GetHash()).Get();
		}

		if (sm_galleryEditTxdId == -1)
		{
			sm_bToggleGalleryEdit = false;
			return;
		}


		PHONEPHOTOEDITOR.StartEditing(sm_galleryEditTxdId);
	}
	else if (PHONEPHOTOEDITOR.IsEditingAvailable() == false)
	{
		PHONEPHOTOEDITOR.FinishEditing();
		sm_galleryEditTxdId = -1;
	}
}
#endif

#if __BANK
static void GalleryEditOnSaveCallback(u8* pJPEGBuffer, u32 size, bool bSuccess )
{
	if (pJPEGBuffer && size && bSuccess )
		Warningf("[PhonePhotoEditor] Save test OK");
	else
		Warningf("[PhonePhotoEditor] Save test FAILED");
}

//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::OnGalleryEditSave()
{
	if (m_galleryEdit.m_bEditing)
	{
		PHONEPHOTOEDITOR.RequestSave( MakeFunctor(GalleryEditOnSaveCallback) );
	}
}
#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::OnGalleryEditTextureNameChange()
{
	if (sm_photoToEditName[0] != 0)
	{
		atHashString hashStr(&sm_photoToEditName[0]);
		sm_galleryEditTxdId = g_TxdStore.FindSlotFromHashKey(hashStr.GetHash()).Get();
	}
}
#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::OnGalleryEditTextChange()
{
	PHONEPHOTOEDITOR.SetText(&sm_topText[0], &sm_bottomText[0], sm_textPos, sm_topTextSize, sm_bottomTextSize, sm_topTextStyle, sm_bottomTextStyle, sm_topTextCol, sm_bottomTextCol);
}
#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void PhonePhotoEditor::RenderDebug()
{
	if (sm_bDebugRender == false || m_galleryEdit.m_bEditing == false)
		return;

	PF_PUSH_MARKER("MemeEditorDebug");

	float x = sm_debugRenderPos.x;
	float y = sm_debugRenderPos.y;
	float w = sm_debugRenderSize.x;
	float h = sm_debugRenderSize.y;

	PUSH_DEFAULT_SCREEN();
		const grcTexture* pTexture = m_galleryEdit.m_pRt;
		grcBindTexture(pTexture);
		grcDrawSingleQuadf(x,y,x+w,y+h,0,0,0,1,1,Color32(255,255,255,255));
		grcBindTexture(NULL);
	POP_DEFAULT_SCREEN();

	PF_POP_MARKER();

}

#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
Vector3	DofTODOverrideHelper::sm_debugSamples[MAX_TIME_SAMPLES] = { Vector3(0.0f, 0.0f, -2.0f),  Vector3(0.0f, 0.0f, -2.0f),  Vector3(0.0f, 0.0f, -2.0f),  Vector3(0.0f, 0.0f, -2.0f) };
float DofTODOverrideHelper::sm_debugOutOfRangeOverride = 0.0f;
bool DofTODOverrideHelper::sm_bToggle = false;
#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void DofTODOverrideHelper::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("TOD Overrides");

		bank.AddToggle("Toggle Override", &sm_bToggle, datCallback(MFA(DofTODOverrideHelper::OnToggle), (datBase*)this));
		bank.AddSlider("Out of Range Override Value", &sm_debugOutOfRangeOverride, -1.0f, 1.0f, 0.01f);
		bank.AddVector("[0] X: From (Hours) Y: To (Hours) Z: Override Value", &sm_debugSamples[0], -2.0f, 24.0f, 0.01f);
		bank.AddVector("[1] X: From (Hours) Y: To (Hours) Z: Override Value", &sm_debugSamples[1], -2.0f, 24.0f, 0.01f);
		bank.AddVector("[2] X: From (Hours) Y: To (Hours) Z: Override Value", &sm_debugSamples[2], -2.0f, 24.0f, 0.01f);
		bank.AddVector("[3] X: From (Hours) Y: To (Hours) Z: Override Value", &sm_debugSamples[3], -2.0f, 24.0f, 0.01f);

	bank.PopGroup();
}
#endif

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void DofTODOverrideHelper::OnToggle()
{
	if (sm_bToggle)
	{
		Begin();

		for (int i = 0; i < MAX_TIME_SAMPLES; i++)
		{
			if (sm_debugSamples[i].z >= -1.0f)
			{
				AddSample(sm_debugSamples[i].x, sm_debugSamples[i].y, sm_debugSamples[i].z);
			}
		}
	}
	else
	{
		End();

		sm_debugSamples[0].z = -2.0f;
		sm_debugSamples[1].z = -2.0f;
		sm_debugSamples[2].z = -2.0f;
		sm_debugSamples[3].z = -2.0f;
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//
bool DofTODOverrideHelper::Begin()
{
	if (Verifyf(IsActive() == false, "Already active"))
	{
		m_bActive = true;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool DofTODOverrideHelper::End()
{
	if (Verifyf(IsActive(), "Not active"))
	{
		Reset();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void DofTODOverrideHelper::Reset()
{
	m_outOfRangeOverrideValue = 0.0f;
	m_bActive = false;
	m_currentValue = 0.0f;
	m_samples.Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void DofTODOverrideHelper::AddSample(float hourFrom, float hourTo, float overrideValue)
{
	if (m_samples.GetCount() == m_samples.GetMaxCount())
	{
		Reset();
		Assertf(0, "DofTODOverrideHelper::AddSample: cannot add more samples");
		return;
	}

	DofTODOverrideEntry& sample = m_samples.Append();
	sample.hourFrom = hourFrom;
	sample.hourTo = hourTo;
	sample.overrideValue = overrideValue;
}

//////////////////////////////////////////////////////////////////////////
//
void DofTODOverrideHelper::Update()
{
	if (IsActive() == false)
		return;

	const float time = CClock::GetDayRatio()*24.0f;

	// By default use the override value for any time that's not within the range of any of the samples
	m_currentValue = m_outOfRangeOverrideValue;

	// Find a suitable range
	for (int i = 0; i < m_samples.GetCount(); i++)
	{
		float lowerBound = Min(m_samples[i].hourFrom, m_samples[i].hourTo);
		float upperBound = Max(m_samples[i].hourFrom, m_samples[i].hourTo);

		if (time >= lowerBound && time <= upperBound)
		{
			m_currentValue = m_samples[i].overrideValue;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
float DofTODOverrideHelper::GetCurrentValue()
{
	return m_currentValue;
}
