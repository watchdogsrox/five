#ifndef _ANIM_SCENE_EVENT_H_
#define _ANIM_SCENE_EVENT_H_

// rage includes
#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "AnimSceneHelpers.h"
#if __BANK
#include "AnimSceneEntity.h"
#include "AnimSceneEditor.h"
#include "bank/button.h"
#include "physics/debugplayback.h"
#endif //__BANK
#include "math/amath.h"
#include "parser/macros.h"

// framework includes
#include "animation/anim_channel.h"
#include "fwutil/flags.h"

//
// animation/AnimScene/AnimSceneEvent.h
//
// CAnimSceneEvent
// Represents a single event that can be triggered on the anim scene timeline.
// Should be overridden to implement all anim scene functionality.
//
// Supports overridable preload and event callbacks (OnEnter, OnUpdate and OnExit in both cases) for implementing event functionality.
// Main event callbacks will be triggered multiple times per frame (once for each of the AnimSceneupdatePhases) to allow 
// inserting functionality at different points throughout the game frame as appropriate.
// Events may also be parented to each other. Child events ignore thier own start, end and preload times and are triggered
// automatically by the parent event instead.

class CAnimScene;
class CAnimSceneEventList;
class CAnimSceneSection;

enum AnimSceneEventType
{
	UnknownEvent = 0, 
	PlayAnimEvent,
	SyncedSceneEvent,
	SyncedSceneAudioEvent,
	CreatePedEvent,
	CreateObjectEvent,
	CreateVehicleEvent,
	PlaySceneEvent,
	PlaySyncedCameraEvent,
	PlaySyncedAnimEvent,
	PlayVfxEvent,
	ForceMotionStateEvent,
	InternalLoopEvent,
	LeadInEvent,
	PlayCameraAnimEvent
};

// PURPOSE: Multiple update phases to allow triggering event callbacks
// at different points in the frame
enum AnimSceneUpdatePhase {
	kAnimSceneUpdatePreAnim,
	kAnimSceneUpdatePreAi,
	kAnimSceneUpdatePostAi,
	kAnimSceneUpdatePostMovement,
	kAnimSceneUpdatePostScript,
	kAnimSceneUpdatePostCamera,
	kAnimSceneUpdatePreRender,
	kAnimSceneShuttingDown
};

#if __BANK

class CAnimSceneEventInitialiser;

#endif //__BANK

class CAnimSceneEvent : public datBase
{
public:

	enum EventFlags
	{
		kRequiresPreloadEnter = BIT(0),
		kRequiresPreloadUpdate = BIT(1),
		kRequiresPreloadExit = BIT(2),
		kRequiresEnter = BIT(3),
		kRequiresUpdate = BIT(4),
		kRequiresExit = BIT(5)
	};

	enum InternalFlags
	{
		kInPreLoad = BIT(0),
		kLoaded = BIT(1),
		kInEvent = BIT(2),
		kTriggerEnter = BIT(3),
		kTriggerUpdate = BIT(4),
		kTriggerExit = BIT(5),
		kDisabled = BIT(6)
	};

	CAnimSceneEvent();

	CAnimSceneEvent(const CAnimSceneEvent& other);

	virtual ~CAnimSceneEvent();
	
	float GetStart() const;
	float GetEnd() const;

#if __BANK
	float GetStart(CAnimScene* pScene, u32 overrideListId);
	float GetEnd(CAnimScene* pScene, u32 overrideListId);
#endif //__BANK

	inline float GetPreloadStart() { return rage::Max(GetStart()-m_preloadTime, 0.0f); }
	inline float GetPreloadDuration() { return m_preloadTime; }
	inline fwFlags16 GetFlags()  { return m_flags; }

	// PURPOSE: Access the (optional) child event structure
	CAnimSceneEventList* GetChildren() { return m_pChildren; }
	const CAnimSceneEventList* GetChildren() const { return m_pChildren; }
	bool HasChildren() const { return m_pChildren!=NULL; }
	// PURPOSE: Returns the number of child events owned by this event.
	// NOTES:	Recursive (includes children of children / etc).
	s32 CountChildren();

	// PURPOSE: returns true if the preload period contains the passed in time
	bool PreloadContains(float time, bool bReverse = false) const;
	// PURPOSE: returns true if the event period contains the passed in time
	bool Contains(float time) const;

	// PURPOSE: Return the section this event belongs to
	CAnimSceneSection* GetSection() {  return m_pSection; }
	
	virtual u32 GetType() { return  UnknownEvent; }

	// PURPOSE: Event preload callbacks. Override these to implement event loading
	// NOTE:	These functions will only be called if the appropriate event flag is set (see EventFlags above)
protected:
	virtual bool OnEnterPreload(CAnimScene* UNUSED_PARAM(pScene)) { return true; }
	virtual bool OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene)) { return true; }
	virtual bool OnExitPreload(CAnimScene* UNUSED_PARAM(pScene)) { return true; }

private:
	// PURPOSE: Internal preload callback triggering calls.
	// NOTE:	Handles recursive triggering of callbacks on child events. parent events are called before 
	//			children for enters and updates, and after the child for exits
	bool OnEnterPreloadInternal(CAnimScene* pScene);
	bool OnUpdatePreloadInternal(CAnimScene* pScene);
	bool OnExitPreloadInternal(CAnimScene* pScene);

public:
	// PURPOSE: Were we within the preload event times during the most recent update. Use to decide 
	//			when to trigger enter and exit preload callbacks.
	inline bool IsInPreload() { return m_internalFlags.IsFlagSet(kInPreLoad); }
	inline bool IsLoaded() { return m_internalFlags.IsFlagSet(kLoaded); }

	// PURPOSE: Preload callback triggering functions. Call from the controlling anim scene when the timeline crosses
	//			the event preloads. Event callbacks are triggered immediately.
	bool TriggerOnEnterPreload(CAnimScene* pScene);
	bool TriggerOnUpdatePreload(CAnimScene* pScene);
	bool TriggerOnExitPreload(CAnimScene* pScene);

protected:
	// PURPOSE: Main event callbacks. Override these in derived classes to implement event functionality
	// NOTE:	These functions will only be called if the appropriate event flag is set (see EventFlags above)
	virtual void OnEnter(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase UNUSED_PARAM(phase)) { }
	virtual void OnUpdate(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase UNUSED_PARAM(phase)) { }
	virtual void OnExit(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase UNUSED_PARAM(phase)) { }

public:
	// PURPOSE: Were we within the main event times during the most recent update. Use to decide 
	//			when to trigger enter and exit callbacks.
	inline bool IsInEvent() { return m_internalFlags.IsFlagSet(kInEvent); }
	
	// PURPOSE: Disabled events are treated as being off the time-line (no preloads or updates triggered)
	//			Events can be disabled during use, in which case they will trigger exits accordingly
	inline bool IsDisabled() { return m_internalFlags.IsFlagSet(kDisabled); }
	inline void SetDisabled(bool disabled) 
	{
		if (disabled)
		{
			m_internalFlags.SetFlag(kDisabled);
		}
		else
		{
			m_internalFlags.ClearFlag(kDisabled);
		}
	}

	// PURPOSE: Main Callback triggering functions. Called from the controlling anim scene when the timeline crosses
	//			the event thresholds. Flags the appropriate event callbacks to be triggered later 
	//			(during the appropriate update phases).
	inline void TriggerOnEnter()	{ m_internalFlags.SetFlag(kInEvent); if (m_flags.IsFlagSet(kRequiresEnter)) m_internalFlags.SetFlag(kTriggerEnter);  }
	inline void TriggerOnUpdate()	{ m_internalFlags.SetFlag(kInEvent); if (m_flags.IsFlagSet(kRequiresUpdate)) m_internalFlags.SetFlag(kTriggerUpdate); }
	inline void TriggerOnExit()	{ m_internalFlags.ClearFlag(kInEvent); m_internalFlags.ClearFlag(kLoaded); if (m_flags.IsFlagSet(kRequiresExit)) m_internalFlags.SetFlag(kTriggerExit);	}

	// PURPOSE: Call once per frame to reset the triggering flags used by TriggerCallbacks below:
	inline void ResetEventTriggers() { m_internalFlags.ClearFlag(kTriggerEnter | kTriggerUpdate | kTriggerExit); }

	// PURPOSE: Triggers the main event callbacks (enter update and exit) that have been marked for triggering this frame
	//			by the three trigger functions above.
	// NOTE:	Triggering and processing are split in this way so that the main event callbacks can receive multiple calls
	//			throughout the frame (one for each of the AnimSceneUpdatePhases) but still only handle the triggering logic once.
	void TriggerCallbacks(CAnimScene* pScene, AnimSceneUpdatePhase phase);

private:

	// PURPOSE: Internal main callback triggering calls.
	// NOTE:	Handles recursive triggering of callbacks on child events. parent events are called before 
	//			children for enters and updates, and after the child for exits
	void OnEnterInternal(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	void OnUpdateInternal(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	void OnExitInternal(CAnimScene* pScene, AnimSceneUpdatePhase phase);

public:

	// PURPOSE: Handles event init. Implement OnInit below in derived classes to add custom behaviour
	void Init(CAnimScene* pScene, CAnimSceneSection* pOwningSection);
	// PURPOSE: Handles event cleanup. Implement OnShutdown below in derived classes to add custom behaviour
	// NOTE:	Can be called more than once before the object is destructed
	void Shutdown(CAnimScene* pScene);
	// PURPOSE: Called on all events when the scene is about to begin.
	void StartOfScene(CAnimScene* pScene);

	// PURPOSE: Override these to implement custom init and shutdown functionality
	virtual void OnInit(CAnimScene* UNUSED_PARAM(pScene)) { }
	virtual void OnShutdown(CAnimScene* UNUSED_PARAM(pScene)) { }
	virtual void OnStartOfScene(CAnimScene* UNUSED_PARAM(pScene)) { }

	// PURPOSE: Make and return a copy of this event.
	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimSceneEvent(*this); }

	// PURPOSE: Return the entity this event belongs to (if applicable). Used to exclude events for optional entities from the scene.
	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return ANIM_SCENE_ENTITY_ID_INVALID; }

	// PURPOSE: returns true is valid,false if not, used to check the event length matches with the selected clip
	virtual bool Validate() {return true;}

#if __BANK
	void SetStart(float time);
	void SetEnd(float time);
	inline void SetPreloadDuration(float preload) { m_preloadTime = preload; }

	static float GetRoundedTime(float fTime);

	// PURPOSE: Return the parent list that owns these events.
	// NOTE:	Only available when being edited through RAG
	inline void SetParentList(CAnimSceneEventList* parent) { m_widgets->m_pParentList = parent; }
	inline CAnimSceneEventList* GetParentList() { return m_widgets->m_pParentList; }

	// PURPOSE: Called automatically after the pargen widgets for the scene are added - used to add editing functionality 
	void OnPostAddWidgets(bkBank& pBank);
	// PURPOSE: Do any custom dev initialisation for newly created events
	//			(called immediately after a new event is created via the RAG widgets)
	virtual void PostAddedFromRag(CAnimScene* UNUSED_PARAM(pScene)) { }

	// PURPOSE: Add the structure for adding and removing child events
	void AddChildEventStructure(bkBank* pBank);
	// PURPOSE: Remove the structure for adding and removing child events
	void RemoveChildEventStructure(bkBank* pBank);
	// PURPOSE: Called when the start and end times of the event are updated form the widgets.
	void NotifyRangeUpdated();
	// PURPOSE: Override in derived classes to provide custom functionality when the start / end time of the event is changed in the editor. 
	virtual void OnRangeUpdated() { }
#if __DEV
	// PURPOSE: Render any debug for this event
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif //__DEV
	void SceneEditorRender(CAnimScene* pScene, CAnimSceneEditor& editor);

	// PURPOSE: Init this event from the anim scene editor. Returns true if successful, false if failed.
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
	// PURPOSE: Get the text to be displayed in the editor within the on screen event box.
	virtual bool GetEditorEventText(atString& textToDisplay, CAnimSceneEditor& editor);
	// PURPOSE: Get the text to be displayed in the editor when hovering this event.
	virtual bool GetEditorHoverText(atString& textToDisplay, CAnimSceneEditor& editor);
	// PURPOSE: Get the color to display the event in the editor.
	virtual void GetEditorColor(Color32& color, CAnimSceneEditor& editor, int colourId);
#endif //__BANK

	PAR_PARSABLE;
protected:
	//////////////////////////////////////////////////////////////////////////
	// internal data

	fwFlags16 m_flags;
	fwFlags16 m_internalFlags;

	// PURPOSE: The section to which this event belongs
	CAnimSceneSection* m_pSection;
	float m_startTime;
	float m_endTime;
	float m_preloadTime;
private:

	// PURPOSE: optional list of child events
	CAnimSceneEventList* m_pChildren; 


#if __BANK
	struct WidgetData{
		WidgetData()
			: m_pChildEventButton(NULL)
			, m_pParentList(NULL)
		{

		}

		bkButton* m_pChildEventButton;
		CAnimSceneEventList* m_pParentList;
	};
#endif //__BANK

	DECLARE_ANIM_SCENE_BANK_DATA(WidgetData, m_widgets);
};

//////////////////////////////////////////////////////////////////////////

inline bool CAnimSceneEvent::PreloadContains(float time, bool bReverse /* = false */) const
{
	float startTime = bReverse ? GetEnd() : GetStart() - m_preloadTime;
	float endTime = bReverse ? GetEnd()+m_preloadTime : GetStart();

	//if(startTime < endTime)
	//{
		return (time >= startTime) && (time <= endTime);
	//}
	//else if(startTime > endTime) // wrapping support
	//{
	//	return (time <= endTime) || (time >= startTime);
	//}
	//return time == preloadTime;
}

//////////////////////////////////////////////////////////////////////////

inline bool CAnimSceneEvent::Contains(float time) const
{
	//if(m_startTime < m_endTime)
	//{
		return (time >= GetStart()) && (time <= GetEnd());
	//}
	//else if(m_startTime > m_endTime) // wrapping support
	//{
	//	return (time <= m_endTime) || (time >= m_startTime);
	//}
	//return time == m_startTime;
}

//////////////////////////////////////////////////////////////////////////

inline bool CAnimSceneEvent::TriggerOnEnterPreload(CAnimScene* pScene)	{ 
	m_internalFlags.SetFlag(kInPreLoad); 
	if (m_flags.IsFlagSet(kRequiresPreloadEnter) && !m_internalFlags.IsFlagSet(kLoaded))
	{
		animDebugf3("AnimSceneEvent: preload enter");
		return OnEnterPreloadInternal(pScene);
	}
	return true;		
}

//////////////////////////////////////////////////////////////////////////

inline bool CAnimSceneEvent::TriggerOnUpdatePreload(CAnimScene* pScene)	{ 
	m_internalFlags.SetFlag(kInPreLoad); 
	if (m_flags.IsFlagSet(kRequiresPreloadUpdate) && !m_internalFlags.IsFlagSet(kLoaded))
	{
		return OnUpdatePreloadInternal(pScene);
	}
	return true;		
}

//////////////////////////////////////////////////////////////////////////

inline bool CAnimSceneEvent::TriggerOnExitPreload(CAnimScene* pScene)	{ 
	m_internalFlags.ClearFlag(kInPreLoad); 
	if (m_flags.IsFlagSet(kRequiresPreloadExit)) 
	{
		animDebugf3("AnimSceneEvent: preload exit");
		return OnExitPreloadInternal(pScene); 
	}
	else 
		return true; 
}

//////////////////////////////////////////////////////////////////////////

inline void CAnimSceneEvent::TriggerCallbacks(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (m_internalFlags.IsFlagSet(kTriggerEnter))
	{
		animDebugf3("AnimSceneEvent: Event entered");
		OnEnterInternal(pScene, phase);
	}
	if (m_internalFlags.IsFlagSet(kTriggerUpdate))
		OnUpdateInternal(pScene, phase);
	if (m_internalFlags.IsFlagSet(kTriggerExit))
	{
		animDebugf3("AnimSceneEvent: Event exited");
		OnExitInternal(pScene, phase);
	}
}

//////////////////////////////////////////////////////////////////////////
//	CAnimSceneEventList - a list of anim scene events.

class CAnimSceneEventList {
public:

	CAnimSceneEventList();

	virtual ~CAnimSceneEventList();

	s32		GetEventCount() const { return m_events.GetCount(); }
	CAnimSceneEvent* GetEvent(s32 i) { animAssert(i>=0 && i< m_events.GetCount()); animAssert(m_events[i]!=NULL); return m_events[i]; }
	const CAnimSceneEvent* GetEvent(s32 i) const { animAssert(i>=0 && i< m_events.GetCount()); animAssert(m_events[i]!=NULL); return m_events[i]; }
	void	AddEvent(CAnimSceneEvent* pEvent) { animAssert(pEvent!=NULL); m_events.PushAndGrow(pEvent); BANK_ONLY(pEvent->SetParentList(this);) }
	void	ClearEvents();
	
#if __BANK

	// PURPOSE: Init the static lists used for event editing.
	static void InitEventEditing();

	// PURPOSE: Called automatically before the pargen widgets for the event list are added - used to add editing functionality 
	void OnPreAddWidgets(bkBank& pBank);
	// PURPOSE: Called automatically after the pargen widgets for the scene are added - used to add editing functionality 
	void OnPostAddWidgets(bkBank& pBank);
	// PURPOSE: Handle widget shutdown. callback registered with the scene in OnPostAddWidgets
	void ShutdownWidgets();

	// PURPOSE: Add a new event of the selected type
	void AddNewEvent();

	// PURPOSE: Delete the event of the given type
	void DeleteEvent(CAnimSceneEvent* pEvent);

	// PURPOSE: Delete the event of the given type
	void DeleteAllEventsForEntity(CAnimSceneEntity::Id entityId);

	// PURPOSE: Return the scene that owns this event list. Required by RAG widgets
	CAnimScene* GetOwningScene() { return m_widgets->m_pOwningScene; }
	void SetOwningScene(CAnimScene* pScene) { m_widgets->m_pOwningScene = pScene; }

#if __DEV
	// PORPOSE: Render any debug info for this event group and call DebugRender on child events
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif //__DEV

	void SceneEditorRender(CAnimScene* pScene, CAnimSceneEditor& editor);
#endif // __BANK

	PAR_PARSABLE;
private:
	atArray<CAnimSceneEvent*> m_events;

#if __BANK
	struct WidgetData{
		WidgetData()
			: m_pGroup(NULL)
			, m_pBank(NULL)
			, m_pOwningScene(NULL)
			, m_debugSelected(false)
		{

		}

		bkBank* m_pBank;
		bkGroup* m_pGroup;
		CAnimScene* m_pOwningScene;
		bool m_debugSelected;
	};
#endif //__BANK

	DECLARE_ANIM_SCENE_BANK_DATA(WidgetData, m_widgets);

#if __BANK
	static atArray<const char *> sm_EventTypeNames;
	static s32 sm_SelectedEventType;
#endif //__BANK
};


#endif //_ANIM_SCENE_EVENT_H_
