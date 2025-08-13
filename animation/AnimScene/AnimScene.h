#ifndef _ANIM_SCENE_H_
#define _ANIM_SCENE_H_

// rage includes
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "physics/debugplayback.h"
#include "parser/macros.h"

// framework includes
#include "ai/task/task.h" // for FSM functionality
#include "streaming/streamingdefs.h"

// game includes
#include "AnimSceneEditor.h"
#include "AnimSceneEntity.h"
#include "AnimSceneEvent.h"
#include "AnimSceneFlags.h"
#include "AnimSceneHelpers.h"

#define USE_ANIM_SCENE_DICTIONARIES 0
#define ANIM_SCENE_CURRENT_VERSION (1)

class CAnimSceneManager;
class CAnimScenePed;
class CAnimSceneLeadInData;
class CTask;

//
// animation/AnimScene/AnimScene.h
//
// CAnimScene
// represents a time-synchronized animated scene that can be created, requested and positioned in the world
// Updates a common time line and origin which are shared by all members of the scene.
// New functionality can be added to the scene by overriding the CAnimSceneEvent class,
// and adding the new events to the timeline.
//
// Contains a basic FSM to handle initial preloading functionality, playing and pausing, etc.
//

class CAnimSceneSection
{
public:

	typedef atHashString Id;

	CAnimSceneSection();

	virtual ~CAnimSceneSection();

	// scene initialisation
	void Init();

	// scene cleanup
	void Shutdown();

	// event initialisation
	void InitEvents(CAnimScene* pScene);

	// have the events in this section been initialised?
	bool AreEventsInitialised() const { return m_bEventsInitialised; }

	// event cleanup
	void ShutdownEvents(CAnimScene* pScene);

	//////////////////////////////////////////////////////////////////////////
	 
	// event update and preload functions
	bool PreloadEvents(CAnimScene* pScene, float sceneTime);
	void ExitPreloadEvents(CAnimScene* pScene, float sceneTime);

	// The main function for processing events and deciding which callbacks to trigger.
	// the callbacks themselves will be triggered later by the appropriate TriggerEvents call.
	void ProcessEvents(CAnimScene* pScene, float startTime, float endTime, float deltaTime);

	// Trigger any required event callbacks for this update phase.
	void TriggerEvents(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	// PURPOSE: Add a new event to the event list
	void AddEvent(CAnimSceneEvent* pEvent)
	{
		if (pEvent)
		{
			m_events.AddEvent(pEvent);
		}
		else
		{
			animAssertf(0, "Trying to add a NULL anim scene event!");
		}
	}

	CAnimSceneEventList& GetEvents() { return m_events; }

	// PURPOSE: Get the length of the section (in seconds)
	float GetDuration() const { return m_duration; }

	// PURPOSE: Make a new copy of this anim scene section
	CAnimSceneSection* Copy();

	// PURPOSE: Copy any event data to the new scene
	void CopyEvents(CAnimSceneSection* pCopyTo);

	// PURPOSE: Reset the section duration based on the event times
	void RecalculateDuration();

	// PURPOSE: Get or set the section's start time within the current scene
	void SetStart(float sceneStartTime) { m_sceneStartTime = sceneStartTime; }
	float GetStart() { return m_sceneStartTime; }

	// PURPOSE: Get the section end time
	float GetEnd() { return GetStart()+GetDuration(); }

	// PURPOSE: Returns true if the id is valid or false if it contains invalid characters or is 0 length.
	static bool IsValidId(const CAnimSceneSection::Id& id);

#if __BANK	
	// PURPOSE: Get the section start time within the given scene and playback list
	float GetStart(CAnimScene* pScene, u32 overridePlaybackListHash);
	// PURPOSE: Get the section end time within the given scene and playback list
	float GetEnd(CAnimScene* pScene, u32 playbackListHash) { return GetStart(pScene, playbackListHash)+GetDuration(); }
	// PURPOSE: Set the owning scene
	void SetOwningScene(CAnimScene* pScene);
	// PURPOSE: Used by the rag widgets to remove an event from the scene
	void DeleteEvent(CAnimSceneEvent* pEvt) { m_events.DeleteEvent(pEvt); }
	// PURPOSE: Used by the rag widgets to remove an entity from the scene
	void DeleteAllEventsForEntity(CAnimSceneEntity::Id entityId) { m_events.DeleteAllEventsForEntity(entityId); }
#if __DEV
	// PURPOSE: Render debug information for this section
	void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif // __DEV
	// PURRPOSE: Render sections and event information in the provided scene editor
	void SceneEditorRender(CAnimScene* pScene, CAnimSceneEditor& editor);

#endif // __BANK

private:

	template<class T> friend class CAnimSceneEventIterator;

	// the list of events in the section
	CAnimSceneEventList m_events;

	//////////////////////////////////////////////////////////////////////////
	/// internal data

	// the length of the section
	float m_duration;

	// the start time of this section in the overall scene
	float m_sceneStartTime;

	// have the events in this section been initialised?
	bool m_bEventsInitialised;

	PAR_PARSABLE;
};

extern const CAnimSceneSection::Id ANIM_SCENE_SECTION_ID_INVALID;
extern const CAnimSceneSection::Id ANIM_SCENE_SECTION_DEFAULT;


// TODO - move this to its own file
class CAnimScenePlaybackList
{
public:

	typedef atHashString Id;

	CAnimScenePlaybackList(){}

	virtual ~CAnimScenePlaybackList(){}

	void AddSection(atHashString id, s32 index = -1);
	void RemoveSection(atHashString id) { m_sections.DeleteMatches(id); }

	s32 GetNumSections() { return m_sections.GetCount(); }
	CAnimSceneSection::Id GetSectionId(s32 index) { if (index>-1 && index<GetNumSections()) return m_sections[index]; else return atHashString((u32)0); }

	CAnimScenePlaybackList* Copy () const;

	// PURPOSE: Returns true if the id is valid or false if it contains invalid characters or is 0 length.
	static bool IsValidId(const CAnimScenePlaybackList::Id& id);

#if __BANK
	void CullEmpty() { m_sections.DeleteMatches(ANIM_SCENE_SECTION_ID_INVALID); }
#endif //__BANK

private:

	atArray<CAnimSceneSection::Id> m_sections;

	PAR_PARSABLE;
};

extern const CAnimScenePlaybackList::Id ANIM_SCENE_PLAYBACK_LIST_ID_INVALID;

// typedef	fwRegdRef<class CAnimScene> RegdAnimScene;

class CAnimScene : /*fwRefAwareBase*/ datBase
{
public:

	friend class CAnimSceneManager;

	// PURPOSE: anim scene FSM states
	enum State{
		State_Invalid=-1,
		State_Initial=0,
		State_LoadMetadata,
		State_PreLoadAssets,
		State_Loaded,
		State_LeadIn,
		State_Running,
		State_Paused,
		State_Skipping,
		State_Finish
	};

	// PURPOSE: State return values, continue or quit
	enum FSM_Return
	{
		FSM_Continue = 0,
		FSM_Quit
	};
	// PURPOSE: Basic FSM operation events
	enum FSM_Event
	{
		OnEnter = 0,
		OnUpdate,
		OnExit
	};

	// An id referencing an anim scene description. Essentially, the anim scene to load.
	typedef atHashString Id;

	typedef atBinaryMap<CAnimScenePlaybackList*, CAnimScenePlaybackList::Id> PlaybackListMap;

	// private refing mechanism. Anim scenes should only be created /
	// destroyed by the anim scene manager, or as child scenes.
	// Had to leave the default constructor / destructor public, for pargen.

	CAnimScene();

	virtual ~CAnimScene();


private:

	CAnimScene(Id sceneId, eAnimSceneFlagsBitSet* pFlags, CAnimScenePlaybackList::Id list = CAnimScenePlaybackList::Id((u32)0));

	// scene initialisation
	void Init();

	// scene cleanup
	void Shutdown();	

	// Increment ref count storage
	void AddRef() { m_refs++; }

	// Decrement ref count storage
	void RemoveRefWithoutDelete() { m_refs--; }

	// Get the current ref count
	s32 GetNumRefs() const { return m_refs; }

public:

	// Main update function. Processes the finite state machine, 
	// progresses the timer and triggers events as appropriate.
	void Update(AnimSceneUpdatePhase phase);

	// queries the current state of the scene
	State GetState() const { return Data().m_state; }

	//////////////////////////////////////////////////////////////////////////
	// Loading and saving

#if USE_ANIM_SCENE_DICTIONARIES
	// Load the anim scene metadata from a resource dictionary
	bool Load(s32 dictIndex, Id hash);
	bool Load(atHashString dictIndex, Id hash);
#endif // USE_ANIM_SCENE_DICTIONARIES

	// copy the loaded pargen data from this scene to the passed in one
	void CopyTo(CAnimScene* pOtherScene)
	{
		pOtherScene->m_rate = m_rate;
		pOtherScene->m_flags.BitSet().Union(m_flags.BitSet());
		if (!pOtherScene->Data().m_sceneOriginOverridden)
			pOtherScene->m_sceneOrigin.SetMatrix(m_sceneOrigin.GetMatrix());

		pOtherScene->m_version = this->m_version;

		CopyEntities(pOtherScene);
		CopySections(pOtherScene);
		CopyPlaybackLists(pOtherScene);
	}

	// PURPOSE: Copy any entity data to the new scene
	// PARAMS:	pCopyTo - the scene to copy the entities into
	//			overwriteExisting - true if you want to overwrite the data in any existing events
	void CopyEntities(CAnimScene* pCopyTo, bool overwriteExisting = false);

	// PURPOSE: Copy any event data to the new scene
	void CopySections(CAnimScene* pCopyTo);

	// PURPOSE: Copy any playback list data to the new scene
	void CopyPlaybackLists(CAnimScene* pCopyTo);

#if !__NO_OUTPUT
	// PURPOSE: Return the name of the scene
	atHashString GetName() {return Data().m_fileName; }
	const char * GetStateName() { return GetStaticStateName(Data().m_state); }
#endif // !__NO_OUTPUT

#if __BANK

	//////////////////////////////////////////////////////////////////////////
	// rag editing widget functionality

	typedef Functor0 ShutdownWidgetCB;

	struct WidgetData
	{
		WidgetData()
		{
			Init();
		}

		~WidgetData()
		{
			Shutdown();
		}

		void Init()
		{
			m_pBank=NULL;
			m_pSceneGroup=NULL;
			m_pEntityGroup=NULL;
			m_pTimeSlider=NULL;
			m_timeControl=0.0f;
			m_debugSelected = false;
			m_bOwnedByEditor = false;
			m_DeleteEntityFromWidgetCB = false; 
		}

		void Shutdown()
		{
			DoWidgetShutdownCallbacks();

			m_pBank = NULL;
			m_pEntityGroup = NULL;
			m_pTimeSlider = NULL;

			if (m_pSceneGroup)
			{
				m_pSceneGroup->Destroy();
				m_pSceneGroup = NULL;
			}
		}

		void DoWidgetShutdownCallbacks()
		{
			// destroy any registered sub widgets
			for (s32 i=0; i<m_widgetCallbacks.GetCount(); i++)
			{
				// call back
				m_widgetCallbacks[i]();
			}
			// only call them once
			m_widgetCallbacks.clear();
		}

		bkBank*	 m_pBank;
		bkGroup* m_pSceneGroup;
		bkGroup* m_pEntityGroup;
		bkSlider* m_pTimeSlider;

		atArray<ShutdownWidgetCB> m_widgetCallbacks;

		float m_timeControl;

		bool m_bOwnedByEditor;
		bool m_debugSelected;
		bool m_DeleteEntityFromWidgetCB; 
	};	

	// PURPOSE: Load the anim scene metadata from a meta file in the assets folder
	bool Load(const char * name);
	bool Load();
	// PURPOSE: Save the anim scene metadata to an meta file in the assets folder
	bool Save(const char * name);
	bool Save();
	// PURPOSE: Called befoe the anim scene file is saved to disk
	void PreSave();
	// PURPOSE: Only scenes owned by the editor show up for editing in the anim scene bank
	void SetOwnedByEditor() { m_widgets->m_bOwnedByEditor = true; }
	bool IsOwnedByEditor() { return m_widgets->m_bOwnedByEditor; }
	// PURPOSE: Externally set the duration of the scene
	void SetDuration(float duration)  { Data().m_duration = duration; }
	// PURPOSE: Add the anim scene editing widgets to the passed in RAG group
	void AddWidgets(bkBank* pBank, bool addGroup = true);
	// PURPOSE: Called before the pargen widgets for the scene are added - used to add editing functionality 
	void OnPreAddWidgets(bkBank& pBank);
	// PURPOSE: Called after the pargen widgets for the scene are added - used to add editing functionality 
	void OnPostAddWidgets(bkBank& pBank);
	// PURPOSE: Add a new event of the selected type
	void AddNewEvent();
	// PURPOSE: Add a new event of the selected type
	template <class Return_T>
	Return_T* AddEvent();
	// PURPOSE: Find the widget with the given name
	bkWidget* FindWidget(bkWidget* parent, const char * widgetName);
	// PURPOSE: Remove this scene
	void Delete();
	// PURPOSE: Toggle the pause mode on or off
	void TogglePause();
	// PURPOSE: Used by the rag widgets to remove an entity from the scene
	void DeleteEntity(CAnimSceneEntity* pEnt);
	// PURPOSE: Used by the rag widgets to remove an event from the scene
	void DeleteEvent(CAnimSceneEvent* pEvt);
	// PURPOSE: Used by the rag widgets to remove an entity from the scene
	void DeleteAllEventsForEntity(CAnimSceneEntity::Id entityId);
	// PURPOSE: Used by the rag widgets to remove a section from the scene
	void DeleteSection(CAnimSceneSection* pSec);
	// PURPOSE: Used by the rag widgets to remove a playback list from the scene
	void DeletePlaybackList(CAnimScenePlaybackList* pList);
	// PURPOSE: Returns a list of valid entity ids for this scene
	void GetEntityNames(atArray<const char *>& names);
	// PURPOSE: Skip callback for the timeline widget
	void Skip();
	// PURPOSE: Get the scene we're currently adding widgets to
	static CAnimScene* GetCurrentAddWidgetsScene() { return sm_pCurrentAddWidgetScene; }
	// PURPOSE: Set the scene we're currently adding widgets to
	static void SetCurrentAddWidgetsScene(CAnimScene* pScene) { sm_pCurrentAddWidgetScene = pScene; }
	// PURPOSE: Get the event list we're currently adding widgets to
	static CAnimSceneEventList* GetCurrentAddWidgetsEventList() { return sm_AddWidgetsEventListStack.GetCount()>0 ? sm_AddWidgetsEventListStack.Top() : NULL; }
	// PURPOSE: Set the scene we're currently adding widgets to
	static void PushAddWidgetsEventList(CAnimSceneEventList* pList) { sm_AddWidgetsEventListStack.PushAndGrow(pList); }
	static void PopAddWidgetsEventList() { sm_AddWidgetsEventListStack.Pop(); }
	// PURPOSE: Init the RAG editing lists for anim scene entities.
	static void InitEntityEditing();

	// PURPOSE: Set the file name used to load and save
	void SetFileName(const char * pName) { Data().m_fileName.SetFromString(pName); }
	// PURPOSE: Called before scene widgets are destroyed by the manager. Use to clean up any widget pointers, etc
	void ShutdownWidgets();
	// PURPOSE: REturna  pointer to the widgets structure (if it exists)
	//WidgetData& GetWidgets() { return *m_widgetData.Get(); }

	// PURPOSE: Used by child objects to register a function that destroys widgets.
	// Called before the scene widgets are removed
	void RegisterWidgetShutdown(ShutdownWidgetCB cb) { m_widgets->m_widgetCallbacks.PushAndGrow(cb); }

	// PURPOSE: Call the registered widget shutdown callbacks
	void DoWidgetShutdownCallbacks() { m_widgets->DoWidgetShutdownCallbacks(); }

	// PURPOSE: Returns a summary string containing debug information about the scene.
	// used by the main scene debug rendering
	atString GetDebugSummary();

#if __DEV
	// PURPOSE: Display detailed debug rendering for this anim scene.
	// used by the main scene debug rendering
	void DebugRender(debugPlayback::OnScreenOutput& output);
#endif //__DEV

	// PURPOSE: Display detailed debug rendering for this anim scene.
	// used by the main scene debug rendering
	void SceneEditorRender(CAnimSceneEditor& editor);

	// PURPOSE: Select / deselect the scene in the on screen debug rendering
	void SetDebugSelected(bool selected) { m_widgets->m_debugSelected = selected; }
	bool IsDebugSelected() { return m_widgets->m_debugSelected; }

	// PURPOSE: Debug scene origin controls
	void SetOriginToEntityCoords();
	void SetOriginToCamCoords();
	void MovePlayerToSceneOrigin();
	// PURPOSE: Get the list of anim scene entity types
	static atArray<const char *>& GetEntityTypeNamesArray() { InitEntityEditing(); return sm_EntityTypeNames; }
	// PURPOSE: Get the list of friendly anim scene entity types
	static atArray<const char *>& GetFriendlyEntityTypeNamesArray() { InitEntityEditing(); return sm_FriendlyEntityTypeNames; }

	// PURPOSE: __BANK only access to adding entities for the scene editor
	void SceneEditorAddEntity(Id id, CAnimSceneEntity* pEnt) { AddEntity(id, pEnt); }
	// PURPOSE: __BANK only access to adding events for the scene editor
	void SceneEditorAddEvent(CAnimSceneEvent* pEvt, CAnimSceneSection::Id id);

	//Purpose: Check that the widget has requested the deletion of a entity
	bool GetShouldDeleteEntityFromWidgetCB();

	//Purpose: Set the state of the widget 
	void SetShouldDeleteEntityFromWidgetCB(bool shouldDeleteEntity);
	void SetShouldDeleteEntityCB(); 

	// PURPOSE: Renames an entity
	bool RenameEntity(Id oldName, Id newName);

	// PURPOSE: BANK_ONLY access to the playback list storage for editor purposes
	const PlaybackListMap& GetPlayBackLists() const { return m_playbackLists; }

	// PURPOSE: Add a new section with the given id
	void AddNewSection(CAnimSceneSection::Id id);

	// PURPOSE: Add a new playback list with the given id
	void AddNewPlaybackList(CAnimScenePlaybackList::Id id);

	// PURPOSE: Add a new section with the given id
	void DeleteSection(CAnimSceneSection::Id id);

	// PURPOSE: Add a new playback list with the given id
	void DeletePlaybackList(CAnimScenePlaybackList::Id id);

	// PURPOSE: Add the provided section id to the playback list with the given id
	void AddSectionToPlaybackList(CAnimSceneSection::Id sectionId, CAnimScenePlaybackList::Id listId, s32 index = -1);

	// PURPOSE: Remove the provided section id from the playback list with the given id
	void RemoveSectionFromPlaybackList(CAnimSceneSection::Id sectionId, CAnimScenePlaybackList::Id listId);

	// PURPOSE: Allow the editor to override the hold at end flag
	void SetEditorHoldAtEnd(bool val)
	{
		Data().m_bEditorHoldAtEnd = val;
	}

#endif //__BANK

#if !__NO_OUTPUT
	// PURPOSE: Get the name of the given state
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Initial&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"Initial",
			"Loading metadata",
			"Pre-loading assets",
			"Loaded",
			"lead-in",
			"Running",
			"Paused",
			"Skipping",
			"Finished"
		};

		return aStateNames[iState];
	}
#endif // !__NO_OUTPUT

	//////////////////////////////////////////////////////////////////////////
	// accessors

	// PURPOSE: Get the version number of the anim scene
	u32 GetVersion() {return m_version;}

	// PURPOSE: GEt and set the anim scene manager inst id for this scene
	void SetInstId(u32 id) { m_data->m_instId=id; }
	u32 GetInstId() { return m_data->m_instId; }

	// PURPOSE: scene flag control
	bool IsFlagSet(eAnimSceneFlags flag) const { return m_flags.BitSet().IsSet(flag); }
	void SetFlag(eAnimSceneFlags flag, bool val){ m_flags.BitSet().Set(flag, val); }

	// PURPOSE: Get the parent anim scene
	const CAnimScene* GetMasterScene() const { return Data().m_pMasterScene; }
	CAnimScene* GetMasterScene() { return Data().m_pMasterScene; }

	// PURPOSE: Make a new scene as a child of this scene
	CAnimScene* CreateChildScene(CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags, float relativeTime);
	void DestroyChildScene(CAnimScene* pScene);
	
	const eAnimSceneFlagsBitSet& GetFlags() const { return m_flags; } 
	eAnimSceneFlagsBitSet& GetFlags() {return m_flags; }

	// PURPOSE: Get the current time of the scene (in seconds)
	float GetTime() const { return Data().m_time; }

	// PURPOSE: Gets the current time of the scene (in seconds), but bear in mind if it's about to skip to a new time
	float GetTimeWithSkipping() const;

	// PURPOSE: Get the total length of the scene (in seconds)
	float GetDuration() const;

	// PURPOSE: pause control
	void SetPaused(bool bPaused) { Data().m_paused = bPaused; }
	bool GetPaused() const { return Data().m_paused; }

	// PURPOSE: playback rate control
	void SetRate(float fRate) { m_rate = fRate; }
	float GetRate() const { return m_rate; }

	// PURPOSE: Internal looping control
	void SetInternalLooping(float startTime, float endTime);
	void StopInternalLooping() { Data().m_internalLoopStart = 0.0f; Data().m_internalLoopEnd = 0.0f; Data().m_enableEternalLooping = false; }
	bool IsInternalLoopingEnabled() const { return Data().m_enableEternalLooping; }

	// PURPOSE: Return the currently active playback list
	CAnimScenePlaybackList::Id GetPlayBackList() const { return Data().m_playbackList; }
	void SetPlayBackList(CAnimScenePlaybackList::Id listId);

	// PURPOSE: Return the playback list with the given id
	CAnimScenePlaybackList* GetPlayBackList(CAnimScenePlaybackList::Id id)
	{
		CAnimScenePlaybackList** ppList = m_playbackLists.Access(id);
		if (ppList)
		{
			return *ppList;
		}
		return NULL;
	}

	// PURPOSE: Determine whether this scene has a parent (it is attached)
	bool IsAttachedToParent()
	{
		return this->Data().m_attachedParentEntity != nullptr;
	}

	// PURPOSE: Get and set the scene origin
	Mat34V_ConstRef GetSceneOrigin();

	CAnimSceneMatrix& GetSceneMatrix();

	void SetSceneOrigin(Mat34V_ConstRef mat);
	bool IsSceneOriginOverriden() { return Data().m_sceneOriginOverridden; }

	// PURPOSE: find the anim scene entity for the given id (any entity type)
	CAnimSceneEntity* GetEntity(CAnimSceneEntity::Id id);

	// PURPOSE: Get the saved situation data from the metadata for the given entity
	const AnimSceneEntityLocationData* GetEntityLocationData(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackList);
	bool GetEntityInitialLocation(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackList, Mat34V_InOut location);
	bool GetEntityFinalLocation(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackList, Mat34V_InOut location);

#if __BANK
	// PURPOSE: Returns the first event for the given entity of the provided type.
	template <class Return_T>
	Return_T* FindFirstEventForEntity(CAnimSceneEntity::Id id);

	// PURPOSE: Calculate and save the entity start and end locations for the given playback list
	bool CalcEntityLocationData(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackList, AnimSceneEntityLocationData& situation);

	s32 GetNumEntities() { return m_entities.GetCount(); }
	// PURPOSE: bank only support for iterating the entities in the entity map by index
	const CAnimSceneEntity* GetEntity(s32 index) const { return *m_entities.GetItem(index); }
	CAnimSceneEntity* GetEntity(s32 index) { return *m_entities.GetItem(index); }
#endif //__BANK

	// PURPOSE: Get the anim scene entity for the given id if it is of the provided type (with automatic casting)
	// RETURNS: A pointer to the entity cast to the correct type, if the entity exists and is of that type. Otherwise returns NULL
	// NOTE:	Will assert only if the class type you're casting to doesn't match the type you passed in
	template <class Return_T>
	Return_T* GetEntity(CAnimSceneEntity::Id id, eAnimSceneEntityType type)
	{
		if (GetMasterScene())
		{
			return GetMasterScene()->GetEntity<Return_T>(id, type);
		}
		else
		{
			CAnimSceneEntity* pEnt =  GetEntity(id);

			if (pEnt)
			{
				if (pEnt->GetType()==type)
				{
					animAssertf(static_cast<Return_T*>(pEnt)->GetType()==type, "Casting anim scene entity %s to the wrong type! Check your GetEntity<> call...", id.GetCStr());
					return static_cast<Return_T*>(pEnt);
				}
			}

			return NULL;
		}
	}

	// PURPOSE: RAG helper function. Parents to entity picker selection.
	void AttachToSelected();

	// PURPOSE: RAG helper function. Parents to entity picker selection, then adds the inverse of the transformation to the current entity as the offset (maintains the current relationship)
	void AttachToSelectedPreservingLocation();

	// PURPOSE: Sets an entity's bone as the parent of this scene. Ensures the transformation of the scene is parented to the transformation of this bone
	void AttachToEntitySkeleton(CEntity* pEnt, s16 boneIdx);

	// PURPOSE: Sets an entity as the parent of this scene. Ensures the transformation of the scene is parented to the transformation of this entity
	void AttachToEntity(CEntity* pEnt);

	// PURPOSE: Sets an entity as the parent of this scene. Ensures the transformation of the scene is parented to the transformation of this entity
	void AttachToEntityPreservingLocation(CEntity* pEnt);

	// PURPOSE: Parent a scene to a skeleton with the current offset in the world
	void AttachToSkeletonPreservingLocation(CEntity* pEnt, s16 boneIdx);

	// PURPOSE: Removes any entity reference, and therefore any attachment/parenting of the scene's transformation
	void DetachFromEntity();

	// PURPOSE: Detaches the scene from any entity, but ensures the resulting transformation of the scene is preserved (the current world offset).
	void DetachFromEntityPreservingLocation();

	// PURPOSE: Returns the parent matrix (attached entity) for this anim scene if one exists (identity if not)
	void GetParentMatrix(Mat34V_InOut mat);

	// PURPOSE: Gets the existing anim scene entity for the given id (asserting it is of the correct type), or allocates, adds and returns a new one if not
	// RETURNS: A pointer to the entity cast to the correct type, if the entity exists and is of that type, or was successfully created. Otherwise returns null.
	// NOTE:	Will assert if the entity exists but is of the wrong type, the new entity could not be allocated, or the class type you're casting to doesn't match the type you passed in
	template <class Return_T>
	Return_T* GetOrAddEntity(CAnimSceneEntity::Id id, eAnimSceneEntityType type)
	{

		if (GetMasterScene())
		{
			return GetMasterScene()->GetOrAddEntity<Return_T>(id, type);
		}
		else
		{
			CAnimSceneEntity* pEnt =  GetEntity(id);

			if (pEnt)
			{
				if (pEnt->GetType()==type)
				{
					animAssertf(static_cast<Return_T*>(pEnt)->GetType()==type, "Casting anim scene entity %s to the wrong type! Check your GetOrAddEntity<> call...", id.GetCStr());
					return static_cast<Return_T*>(pEnt);
				}
				else
				{
					animAssertf(pEnt->GetType()==type, "Trying to FindOrCreate an anim scene entity with id: %s, type %s, but an entity with that id already exists (type: %s)", id.GetCStr(), CAnimSceneEntity::GetEntityTypeName(type), CAnimSceneEntity::GetEntityTypeName(pEnt->GetType()));
				}
			}
			else
			{
				// entity doesn't exist - create it
				Return_T* pNewEnt = rage_new Return_T();
				if (pNewEnt)
				{
					// add the entity
					pNewEnt->SetId(id);
					AddEntity(id, pNewEnt);
					return pNewEnt;
				}
				else
				{
					animAssertf(0, "Failed to allocate new anim scene entity (id:%s, type:%s)!", id.GetCStr(), CAnimSceneEntity::GetEntityTypeName(type));
				}
			}
			return NULL;
		}
	}

	// PURPOSE: Helper function to make retrieving CPhysical derived entities easier. 
	//			Asserts if an entity with an incompatible type but the same name was found
	CPhysical* GetPhysicalEntity(CAnimSceneEntity::Id sceneId BANK_ONLY(, bool createDebugEntIfRequired = true));
	CPhysical* GetPhysicalEntity(CAnimSceneEntity* pEnt BANK_ONLY(, bool createDebugEntIfRequired = true));

	//////////////////////////////////////////////////////////////////////////
	// Scene control methods

	// start loading the assets required by the scene
	void BeginLoad()
	{
		animAssert(GetState()==State_Initial);
		Data().m_startPreload = true;
	}

	// returns true once the scene has finished loading
	bool IsInitialised() const { return GetState()>State_Initial; }

	// returns true once the scene has finished loading
	bool IsLoaded() const { return GetState()>=State_Loaded; }

	// returns true once the scene metadata has finished loading
	bool IsMetadataLoaded() const { return GetState()>State_LoadMetadata; }

	// returns true once the scene has finished loading
	bool IsPlayingBack() const { return GetState()>=State_LeadIn && GetState()<State_Finish; }

	// returns true once the scene has finished playing
	bool IsFinished() const { return GetState()==State_Finish; }

	// start playback of the scene (once IsLoaded returns true)
	void Start() { 
		animAssert(GetState()==State_Loaded);
		Data().m_startScene = true;
	}

	// end playback of the scene now (making all appropriate cleanup calls, etc)
	void Stop() { 
		animAssert(IsPlayingBack());
		Data().m_stopScene = true;
	}

	// PURPOSE: Resets the scene to its initial state
	void Reset();

	// PURPOSE: Skip to the given time in the scene
	void Skip(float newTime)
	{
		animAssert(GetState()>State_Loaded);
		Data().m_skipToTime = true;
		Data().m_skipTime = newTime;
	}

	// PURPOSE: Return the seciton id for the section we're currently playing back
	CAnimSceneSection::Id GetCurrentSectionId();

	// PURPOSE: Get the section id for the given time in the scene.
	// NOTE:	If the time is precisely on a section boundary, returns the section that is about to start.
	CAnimSceneSection::Id FindSectionForTime(float time);

	// PURPOSE: Returns true if the scene's current playback list contains the given section
	bool ContainsSection(CAnimSceneSection::Id sectionId);

	// PURPOSE: Calculate the time in the scene that the provided section id should start, based on the current playback list
	float CalcSectionStartTime(CAnimSceneSection::Id sectionId) { return CalcSectionStartTime(GetSection(sectionId)); }
	float CalcSectionStartTime(CAnimSceneSection* pSection);
	
	// PURPOSE: Calculate the time in the scene that the provided section id should start, based on a given playback list
	float CalcSectionStartTime(CAnimSceneSection* pSection, CAnimScenePlaybackList::Id playbackList);

	// PURPOSE: trigger a recalculation of all section durations and start times, based on the current playback list (and update the scene duration)
	void RecalculateSceneDurations();

	// PURPOSE: Return an anim scene section by its id.
	CAnimSceneSection* GetSection(CAnimSceneSection::Id sectionId);

	// PURPOSE: Remove the given entity from the scene. Call when an entity has been forcibly 
	// removed from the scene by a higher priority event, etc to stop any further scene processing
	// (and abort the scene if necessary)
	void RemoveEntityFromScene(CAnimSceneEntity::Id entity);

	bool IsEntityInScene(CEntity* pEnt);

	// PURPOSE: validates the anim scene in terms of length of anim events as compared to the
	// animation clip itself (but only checks if the anim events are not longer than the clips)
	// PARAM: checkAll - If set, doesn't early out at first error, but checks all clips validity and warns if invalid for each clip.
	bool Validate(bool checkAll = true);

	PAR_PARSABLE;

private:

	template<class T> friend class CAnimSceneEventIterator;
	friend class CAnimSceneEntityIterator;
	friend class CAnimSceneSectionIterator;

	
	// PURPOSE: Return an anim scene section by its index.
	CAnimSceneSection* GetSectionByIndex(s32 sectionIndex) { return sectionIndex>-1 && sectionIndex<m_sections.GetCount() ? *m_sections.GetItem(sectionIndex) : NULL; }
	CAnimSceneSection::Id GetSectionIdByIndex(s32 sectionIndex) { return sectionIndex>-1 && sectionIndex<m_sections.GetCount() ? *m_sections.GetKey(sectionIndex) : ANIM_SCENE_SECTION_ID_INVALID; }

	// PURPOSE: Return the section we're currently in.
	CAnimSceneSection* GetCurrentSection();

	// PURPOSE: Process our scene sections
	void ProcessSections(float startTime, float endTime, float deltaTime);

	// PURPOSE: Add the passed in entity to the entity map (indexed with the given id)
	//			Takes ownership of the passed in pointer (i.e. doesn't make a copy)
	void AddEntity(Id id, CAnimSceneEntity* pEnt);

	// PURPOSE: Add the passed in section to the section map (indexed with the given id)
	//			Takes ownership of the passed in pointer (i.e. doesn't make a copy)
	void AddSection(CAnimSceneSection::Id id, CAnimSceneSection* pSection)
	{
		CAnimSceneSection** ppSec = m_sections.SafeGet(id);
		if (ppSec==NULL)
		{
			m_sections.Insert(id, pSection);
			m_sections.FinishInsertion();
		}
		else
		{
			// delete the existing section and overwrite it
			CAnimSceneSection* pSec = *ppSec;
			delete pSec;
			*ppSec = pSection;
		}

#if __BANK
		pSection->SetOwningScene(this);
#endif //__BANK

		RecalculateSceneDurations();
	}

	// PURPOSE: Add a new playback list with the given id
	void AddPlaybackList(CAnimScenePlaybackList::Id id, CAnimScenePlaybackList* pList)
	{
		CAnimScenePlaybackList** ppList = m_playbackLists.SafeGet(id);
		if (ppList==NULL)
		{
			m_playbackLists.Insert(id, pList);
			m_playbackLists.FinishInsertion();
		}
		else
		{
			// delete the existing list and overwrite it
			CAnimScenePlaybackList* pTemp = *ppList;
			delete pTemp;
			*ppList = pList;
		}
	}


	void ReplaceEntity(Id id, CAnimSceneEntity* pEnt)
	{
		if (pEnt!=NULL)
		{
			CAnimSceneEntity** ppEnt = m_entities.SafeGet(id);
			if (ppEnt!=NULL)
			{
				CAnimSceneEntity* pOldEnt = *ppEnt;
				*ppEnt = pEnt;
				delete pOldEnt;
			}
			else
			{
				animAssertf(0, "Failed to replace the anim scene entity (id:%s, type:%s) - no entity with that id exists!", id.GetCStr(), CAnimSceneEntity::GetEntityTypeName(pEnt->GetType()));
			}
		}
	}

	// PURPOSE: Add a new event to the event list
	void AddEvent(CAnimSceneEvent* pEvent, CAnimSceneSection::Id sectionId)
	{
		if (pEvent && sectionId!=ANIM_SCENE_SECTION_ID_INVALID)
		{
			CAnimSceneSection* pSec = GetSection(sectionId);
			animAssertf(pSec, "Unable to find section for section Id %s!", sectionId.GetCStr());
			pSec->AddEvent(pEvent);
			RecalculateSceneDurations();
		}
		else
		{
			animAssertf(pEvent, "Trying to add a NULL anim scene event!");
		}
	}

	// PURPOSE: Calls shutdown on the entities and events in the scene
	//			Checks internally to avoid multiple shutdown calls.
	void ShutdownEntitiesAndEvents();

	// PURPOSE: Shut down and reset the FSM, ensuring exit calls are made
	void ShutdownFSM();

	// FSM functions

	void ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	void SetState(State state)
	{ 
		animAssertf(Data().m_canChangeState, "Don't change the anim scene state outside of an OnUpdate call!"); 
		if (Data().m_canChangeState) Data().m_state = state; 
	}

	void LoadMetadata_OnEnter();
	void LoadMetadata_OnUpdate();
	void LoadMetadata_OnExit();

	void PreLoadAssets_OnEnter();
	void PreLoadAssets_OnUpdate();
	void PreLoadAssets_OnExit();

	void Loaded_OnEnter();
	void Loaded_OnUpdate();
	void Loaded_OnExit();

	void LeadIn_OnEnter();
	void LeadIn_OnUpdate();
	void LeadIn_OnExit();

	void Running_OnEnter();
	void Running_OnUpdate();
	void Running_OnExit();

	void Paused_OnEnter();
	void Paused_OnUpdate();
	void Paused_OnExit();

	void Skipping_OnEnter();
	void Skipping_OnUpdate();
	void Skipping_OnExit();

	void Finish_OnEnter();
	void Finish_OnUpdate();
	void Finish_OnExit();

	// TODO - Add support for lead ins

// 	// PURPOSE: Returns true if this scene needs to run lead-ins
// 	bool ShouldUseLeadIns();
// 
// 	// PURPOSE: Start the specified lead in task on the provided ped entity
// 	void StartLeadInTask(CAnimSceneEntity* pSceneEnt, CAnimSceneLeadInData* data);
// 
// 	// PURPOSE: Find the lead in task on the provided ped entity
// 	CTask* FindLeadInTask(CAnimSceneEntity* pSceneEnt);

#if __BANK
	// PURPOSE: Render the special cases of authoring tools for parented scenes, and handle the input on them
	void RenderAndAdjustAuthoringTools();
#endif //__BANK

	//////////////////////////////////////////////////////////////////////////
	// scene data (can be configured / loaded from the metadata file)

	// the playback rate for the scene
	float m_rate;
	
	// the scene origin matrix
	CAnimSceneMatrix m_sceneOrigin;

	// The global transformed scene matrix if there is a parent
	CAnimSceneMatrix m_transformedSceneMatrix;

	// the register of entities in the scene
	atBinaryMap<CAnimSceneEntity*, CAnimSceneEntity::Id> m_entities;

	// register of all the sections in this anim scene
	atBinaryMap<CAnimSceneSection*, CAnimSceneSection::Id> m_sections;

	// register of all the available playback list
	PlaybackListMap m_playbackLists;

	// option flags for the whole scene (looping, etc)
	eAnimSceneFlagsBitSet m_flags;

	// Version number for the anim scene
	u32 m_version;

	//////////////////////////////////////////////////////////////////////////
	// internal variables

	s32 m_refs; // ref count variable#

	struct RuntimeData{

		Id m_id;
		u32 m_instId;
		strLocalIndex m_streamingIndex;

		// fsm processing data
		State m_state;
		State m_previousState;

		// the current scene time
		float m_time;

		// the total duration of the scene
		// calculated from the length of the individual setions post load.
		float m_duration;

		// the requested skip time
		float m_skipTime;

		// internal control flags
		bool m_metadataLoaded : 1;	// Has the scene metadata been loaded yet?
		bool m_startPreload : 1;	// The user has triggered the load of the scene assets
		bool m_startScene : 1;		// The user has triggered the start of the scene
		bool m_stopScene : 1;
		bool m_paused : 1;			// The user has paused the scene
		bool m_skipToTime : 1;		// skips to the given time on the next update
		bool m_eventsShutdown : 1;	// Set to true once the events have been shut down
		bool m_sceneOriginOverridden : 1;

		bool m_canChangeState : 1;  // Internal FSM system flag. Used to ensure the scene state can only be changed during OnUpdate callbacks

		bool m_enableEternalLooping : 1; // Turn on / off the internal looping functionality
		bool m_bEditorHoldAtEnd : 1; // Allow the editor to override the hold at end flag

		// The master anim scene to use as a reference, if this scene is running as a sub scene.
		// Use this to access the common scene origin and entity structure.
		CAnimScene* m_pMasterScene;

		// The time of this scene relative to the master scene
		float m_masterSceneRelativeTime;

		// register of child anim scenes. Used for update purposes.
		atArray<CAnimScene*> m_childScenes;

		// Control for internal looping of the scene
		float m_internalLoopStart;
		float m_internalLoopEnd;

		// The playback list hash to use. if 0, play all sections.
		atHashString m_playbackList;
		s32	m_currentSectionIndex;

		atHashString m_fileName;

		// If attached to an entity: the bone no the parent this entity it attached to. -1 corresponds to the entity matrix.
		s16 m_attachedBoneIndex; 

		// Parent entity reference
		fwRegdRef<class fwEntity> m_attachedParentEntity;
	};

	RuntimeData& Data() { return *m_data.Get(); }
	const RuntimeData& Data() const { return *m_data.Get(); }
	
	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);
	DECLARE_ANIM_SCENE_BANK_DATA(WidgetData, m_widgets);

#if __BANK
	static s32 sm_SelectedEntityType;
	static char sm_NewEntityId[256];

	static atArray<const char *> sm_EntityTypeNames;
	static atArray<const char *> sm_FriendlyEntityTypeNames;
	static atArray<atString> sm_FriendlyEntityTypeStrings;
	static CAnimScene* sm_pCurrentAddWidgetScene;
	static atArray<CAnimSceneEventList*> sm_AddWidgetsEventListStack;

public:
	static atHashString sm_LoadSaveDirectory;

#endif //__BANK
};

class CAnimSceneSectionIterator
{
public:
	CAnimSceneSectionIterator(CAnimScene& animScene, bool ignorePlaybackList = false, bool overridePlaybackList = false, CAnimScenePlaybackList::Id overrideList = ANIM_SCENE_PLAYBACK_LIST_ID_INVALID);
	~CAnimSceneSectionIterator();

	void begin();
	CAnimSceneSectionIterator& operator++();
	CAnimSceneSectionIterator& operator++(int);
	const CAnimSceneSection* operator*() const;
	CAnimSceneSection* operator*();

	// PURPOSE: Return the id of the current section
	CAnimSceneSection::Id GetId() { return m_currentSection; }

	// PURPOSE: return the index of the section (relative to the playback list, if specified)
	s32 GetCurrentIndex() { return m_currentIndex; }

private:
	CAnimSceneSectionIterator();

	void FindNextSection();

	CAnimScene* m_animScene;
	s32 m_currentIndex;
	CAnimSceneSection::Id m_currentSection;
	CAnimScenePlaybackList* m_pPlaybackList;
	CAnimSceneSection * m_pSection;
	bool m_ignorePlaybackList;
	bool m_overridePlaybackList;
	CAnimScenePlaybackList::Id m_overrideList;
};

#if __BANK

template <class Return_T>
Return_T* CAnimScene::AddEvent()
{
	Return_T* pNewEvent = rage_new Return_T();
	AddEvent(pNewEvent, ANIM_SCENE_SECTION_DEFAULT);
	return pNewEvent;
};

#include "AnimSceneEventIterator.h"

template <class Return_T>
Return_T* CAnimScene::FindFirstEventForEntity(CAnimSceneEntity::Id id)
{
	// search the anim scene for the last play anim event
	CAnimSceneEventIterator<Return_T> it(*this);
	Return_T* pFirstEvent = NULL;
	float firstEventStart = FLT_MAX;

	it.begin();
	while (*it)
	{
		Return_T* pEvent = (*it);
		if (pEvent->GetPrimaryEntityId()==id)
		{
			float eventStart = pEvent->GetStart();
			if (eventStart < firstEventStart)
			{
				pFirstEvent = pEvent;
				firstEventStart = eventStart;
			}
		}

		++it;
	}

	return pFirstEvent;
}

#endif // __BANK

extern const CAnimScene::Id ANIM_SCENE_ID_INVALID;

#endif //_ANIM_SCENE_H_
