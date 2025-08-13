#include "AnimScene.h"
#include "AnimScene_parser.h"

// rage includes
#include "bank/msgbox.h"
#include "bank/slider.h"
#include "grcore/debugdraw.h"

// framework includes
#include "fwdebug/picker.h"
#include "fwsys/metadatastore.h"
#include "fwsys/timer.h"

// game includes
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/AnimScene/entities/AnimSceneEntities.h"
#include "animation/AnimScene/events/AnimScenePlayAnimEvent.h"
#include "animation/AnimScene/AnimSceneEntityIterator.h"
#include "animation/AnimScene/AnimSceneEventIterator.h"
#include "peds/ped.h"
#include "Peds\PedIntelligence.h"
#include "task/Animation/TaskAnims.h"
#include "task\Scenario\Types\TaskUseScenario.h"
#include "vehicles/vehicle.h"
#include "vehicleAi\VehicleIntelligence.h"
#include "objects/object.h"
#if __BANK
#include "camera/base/BaseCamera.h"
#include "camera/system/CameraManager.h"
#include "Peds/PlayerInfo.h"
#endif //__BANK

ANIM_OPTIMISATIONS()

const CAnimScene::Id ANIM_SCENE_ID_INVALID((u32)0);

const CAnimSceneSection::Id ANIM_SCENE_SECTION_ID_INVALID((u32)0);
const CAnimSceneSection::Id ANIM_SCENE_SECTION_DEFAULT("Main", 0x27eb33d7);

const CAnimScenePlaybackList::Id ANIM_SCENE_PLAYBACK_LIST_ID_INVALID((u32)0);

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection::CAnimSceneSection()
{
	Init();
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection::~CAnimSceneSection()
{
	Shutdown();
}


//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::Init()
{
	m_duration = 0.0f;
	m_sceneStartTime = 0.0f;
	m_bEventsInitialised = false;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::Shutdown()
{
	m_events.ClearEvents();
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::InitEvents(CAnimScene* pScene)
{
	animAssertf(!m_bEventsInitialised, "Events are already initialised!");

	m_sceneStartTime = pScene->CalcSectionStartTime(this);

	BANK_ONLY(m_events.SetOwningScene(pScene);)

	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		CAnimSceneEvent& event = *m_events.GetEvent(i);

		// first init the event
		event.Init(pScene, this);
	}

	m_bEventsInitialised = true;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::ShutdownEvents(CAnimScene* pScene)
{
	animAssertf(m_bEventsInitialised, "Events are not initialised!");

	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		CAnimSceneEvent& event = *m_events.GetEvent(i);
		// make sure we call any necessary exit callbacks when the scene shuts down
		if (event.IsInPreload())
		{
			event.TriggerOnExitPreload(pScene);
		}
		// make sure we call any necessary exit callbacks when the scene shuts down
		if (event.IsInEvent())
		{
			event.TriggerOnExit();
		}

		// make sure we trigger any necessary callbacks before shutting down
		event.TriggerCallbacks(pScene, kAnimSceneShuttingDown);

		event.ResetEventTriggers();
		event.Shutdown(pScene);
	}

	m_bEventsInitialised = false;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneSection::PreloadEvents(CAnimScene* pScene, float sceneTime)
{
	bool allEventsLoaded = true;

	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		// determine which event callbacks to trigger based on the time
		CAnimSceneEvent& event = *m_events.GetEvent(i);

		if (event.PreloadContains(sceneTime - m_sceneStartTime))
		{
			// trigger a preload enter if the event preload hits the start of the scene
			if (!event.IsInPreload())
			{
				if (!event.TriggerOnEnterPreload(pScene))
				{
					allEventsLoaded = false;
				}
			}

			if (!event.TriggerOnUpdatePreload(pScene))
			{
				allEventsLoaded = false;
			}
		}
	}

	return allEventsLoaded;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::ExitPreloadEvents(CAnimScene* pScene, float UNUSED_PARAM(sceneTime))
{
	// trigger event preload exit events for preloads starting on zero
	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		// determine which event callbacks to trigger based on the time
		CAnimSceneEvent& event = *m_events.GetEvent(i);

		if (event.IsInPreload())
		{
			event.TriggerOnExitPreload(pScene);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Event handling and processing

inline bool HasJumpedEvent(float eventStartTime, float eventEndTime, float startTime, float endTime, float delta, bool loopedThisUpdate)
{
	if (loopedThisUpdate)
	{
		// Test from start to (start+delta) and from (end-delta) to end.
		if (delta<0.0f)
		{
			return (endTime<eventStartTime && (endTime-delta)>eventEndTime) || ((startTime+delta)<eventStartTime && startTime>eventEndTime);
		}
		else
		{
			return (startTime<eventStartTime && (startTime+delta)>eventEndTime) || ((endTime-delta)<eventStartTime && endTime>eventEndTime);
		}
	}
	else
	{
		// standard behaviour: Test from start to end
		return delta<0.0f ? (endTime<eventStartTime && startTime > eventEndTime) : (startTime<eventStartTime && endTime > eventEndTime);
	}
}

void CAnimSceneSection::ProcessEvents(CAnimScene* pScene, float sceneStartTime, float sceneEndTime, float deltaTime)
{
	float startTime = sceneStartTime;
	float endTime = sceneEndTime;

	bool bLooped = ((deltaTime!=0.0f) && (Sign(endTime-startTime) != Sign(deltaTime)));

	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		// determine which event callbacks to trigger based on the time
		CAnimSceneEvent& event = *m_events.GetEvent(i);

		event.ResetEventTriggers();

		// check the preload
		{
			float eventStartTime = event.GetPreloadStart();
			float eventEndTime = event.GetStart();

			bool bReverse = deltaTime<0;
			bool bWasInEvent = event.IsInPreload();
			bool bContainsEnd = !event.IsDisabled() && (event.PreloadContains(endTime, bReverse) || (pScene->IsFlagSet(ASF_LOOPING) && event.PreloadContains(bReverse ? endTime+m_duration : endTime-m_duration, bReverse)));
			bool bJumpedTag = !event.IsDisabled() && HasJumpedEvent(eventStartTime, eventEndTime, startTime, endTime, deltaTime, bLooped);

			if((!bWasInEvent && bContainsEnd) || bJumpedTag)
			{
				event.TriggerOnEnterPreload(pScene);
			}
			if (bWasInEvent || bContainsEnd || bJumpedTag)
			{
				event.TriggerOnUpdatePreload(pScene);
			}
			if((bWasInEvent && !bContainsEnd) || bJumpedTag)
			{
				event.TriggerOnExitPreload(pScene);
			}
		}

		// check the main event update
		{
			float eventStartTime = event.GetStart();
			float eventEndTime = event.GetEnd();

			bool bWasInEvent = event.IsInEvent();
			bool bContainsEnd = !event.IsDisabled() && event.Contains(endTime);
			bool bJumpedTag = !event.IsDisabled() && HasJumpedEvent(eventStartTime, eventEndTime, startTime, endTime, deltaTime, bLooped);

			if((!bWasInEvent && bContainsEnd) || bJumpedTag)
			{
				event.TriggerOnEnter();
			}
			if (bWasInEvent || bContainsEnd || bJumpedTag)
			{
				event.TriggerOnUpdate();
			}
			if((bWasInEvent && !bContainsEnd) || bJumpedTag)
			{
				event.TriggerOnExit();
			}
		}
	
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::TriggerEvents(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		// determine which event callbacks to trigger based on the time
		CAnimSceneEvent& event = *m_events.GetEvent(i);
		event.TriggerCallbacks(pScene, phase);
	}
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection* CAnimSceneSection::Copy()
{
	CAnimSceneSection* pNewSection = rage_new CAnimSceneSection();

	if (pNewSection)
	{
		pNewSection->m_duration = m_duration;
		pNewSection->m_sceneStartTime = m_sceneStartTime;
		CopyEvents(pNewSection);
	}
	else
	{
		animAssertf(pNewSection, "unable to create new anim scene section! Out of memory?");
	}

	return pNewSection;
}


//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::CopyEvents(CAnimSceneSection* pCopyTo)
{	
	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		pCopyTo->AddEvent(m_events.GetEvent(i)->Copy());
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::RecalculateDuration()
{	
	float duration = 0.0f;

	for (s32 i=0; i<m_events.GetEventCount(); i++)
	{
		CAnimSceneEvent* pEvt = m_events.GetEvent(i);
		if (pEvt)
		{
			duration = Max(duration, pEvt->GetEnd() - m_sceneStartTime);
		}
	}

	m_duration = duration;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneSection::IsValidId(const Id& id)
{
	//Only check for valid ids in bank
#if __BANK
	const char* idStr = id.TryGetCStr();
	//Check for null/invalid string
	if(!idStr)
	{
		return false;
	}
	std::string idStrS = idStr;
	if(idStrS.length() == 0)
	{
		return false;
	}

	for(int i = 0; i < idStrS.length(); )
	{
		if(idStrS[i] == '\n' || idStrS[i] == '\r')
		{
			return false;
		}
		else
		{
			++i;
		}
	}
#endif //__BANK

	return id.IsNotNull();
}

#if __BANK

float CAnimSceneSection::GetStart(CAnimScene* pScene, u32 overridePlaybackListHash)
{
	if (pScene)
	{
		return pScene->CalcSectionStartTime(this, (CAnimScenePlaybackList::Id)overridePlaybackListHash);
	}

	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::SetOwningScene(CAnimScene* pScene)
{	
	m_events.SetOwningScene(pScene);
}

//////////////////////////////////////////////////////////////////////////
#if __DEV
void CAnimSceneSection::DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput)
{
	m_events.DebugRender(pScene, mainOutput);
}
#endif //__DEV

//////////////////////////////////////////////////////////////////////////

void CAnimSceneSection::SceneEditorRender(CAnimScene* pScene, CAnimSceneEditor& editor)
{
	m_events.SceneEditorRender(pScene, editor);
}






//////////////////////////////////////////////////////////////////////////

atArray<const char *> CAnimScene::sm_EntityTypeNames;
atArray<const char *> CAnimScene::sm_FriendlyEntityTypeNames;
atArray<atString> CAnimScene::sm_FriendlyEntityTypeStrings;

s32 CAnimScene::sm_SelectedEntityType = 0;
char CAnimScene::sm_NewEntityId[256]="";

// the scene currently being added to the widgets
CAnimScene* CAnimScene::sm_pCurrentAddWidgetScene = NULL;
atArray<CAnimSceneEventList*> CAnimScene::sm_AddWidgetsEventListStack;

atHashString CAnimScene::sm_LoadSaveDirectory("assets:/export/anim/anim_scenes");

#endif // __BANK

void CAnimScenePlaybackList::AddSection(atHashString id, s32 index /* = -1 */)
{ 
	if (id!= ANIM_SCENE_SECTION_ID_INVALID && m_sections.Find(id)==-1)
	{
		if (index>-1 && index<m_sections.GetCount())
		{
			// grow the array if necessary
			if (m_sections.GetCount()==m_sections.GetCapacity())
			{
				m_sections.Grow();
			}
			m_sections.Insert(index) = id;
		}
		else
		{
			m_sections.PushAndGrow(id);
		}
	}
}

CAnimScenePlaybackList* CAnimScenePlaybackList::Copy() const
{
	CAnimScenePlaybackList* pList = rage_new CAnimScenePlaybackList();
	pList->m_sections = m_sections; // copy array contents
	return pList;
}

bool CAnimScenePlaybackList::IsValidId(const Id& id)
{
	//Only check for valid ids in bank release
#if __BANK
	const char* idStr = id.TryGetCStr();
	if(!idStr)
	{
		return false;
	}
	std::string idStrS = idStr;
	if(idStrS.length() == 0)
	{
		return false;
	}

	for(int i = 0; i < idStrS.length(); )
	{
		if(idStrS[i] == '\n' || idStrS[i] == '\r')
		{
			return false;
		}
		else
		{
			++i;
		}
	}
#endif //__BANK
	return id.IsNotNull();
}

//////////////////////////////////////////////////////////////////////////
CAnimScene::CAnimScene()
	: m_refs(0)
{
	Init();
}

//////////////////////////////////////////////////////////////////////////
CAnimScene::CAnimScene(CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags, CAnimScenePlaybackList::Id list /*= CAnimScenePlaybackList::Id((u32)0)*/)
	: m_refs(0)
{
	Init();

	// set the scene id to preload
	Data().m_id = sceneId;
#if !__NO_OUTPUT
	Data().m_fileName = Data().m_id;
#endif // __NO_OUTPUT

	Data().m_playbackList = list;
	Data().m_bEditorHoldAtEnd = false;

	// preset the flags
	if (pFlags)
	{
		m_flags.BitSet().Union(pFlags->BitSet());
	}
}

//////////////////////////////////////////////////////////////////////////

CAnimScene::~CAnimScene()
{
	Shutdown();
}

//////////////////////////////////////////////////////////////////////////
void CAnimScene::Init()
{
	m_version = ANIM_SCENE_CURRENT_VERSION;
	Data().m_time = 0.0f;
	m_rate =1.0f;

	Data().m_state = State_Initial;
	Data().m_previousState=State_Invalid;
	
	Data().m_startPreload = false;
	Data().m_startScene = false;
	Data().m_stopScene = false;
	Data().m_paused = false;
	Data().m_skipToTime = false;
	Data().m_metadataLoaded = false;
	Data().m_eventsShutdown = false;
	Data().m_sceneOriginOverridden = false;

	Data().m_canChangeState = false;

	Data().m_pMasterScene = NULL;
	Data().m_masterSceneRelativeTime = 0.0f;

	Data().m_internalLoopStart = 0.0f;
	Data().m_internalLoopEnd = 0.0f;
	Data().m_enableEternalLooping = false;

	Data().m_playbackList.SetHash(0);
	Data().m_currentSectionIndex=0;

	// make sure we always have a default section
	CAnimSceneSection* pDefaultSection = rage_new CAnimSceneSection();
	AddSection(ANIM_SCENE_SECTION_DEFAULT, pDefaultSection);
	pDefaultSection->InitEvents(this);

	Data().m_fileName.SetHash(0);

	//Attachments (parenting to entities)
	Data().m_attachedBoneIndex = -1;
	Data().m_attachedParentEntity = nullptr;
	m_transformedSceneMatrix.SetMatrix(m_sceneOrigin.GetMatrix());
	//Remove the anim scene matrices from the static list of them, so we can update them manually
#if __BANK
	CAnimSceneMatrix::m_AnimSceneMatrix.DeleteMatches(&m_sceneOrigin);
	CAnimSceneMatrix::m_AnimSceneMatrix.DeleteMatches(&m_transformedSceneMatrix);
#endif
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::Shutdown()
{
#if __BANK
	ShutdownWidgets();
#endif // __BANK

	ShutdownFSM();

	ShutdownEntitiesAndEvents();

	// delete all sections and reclaim storage
	for(s32 i = 0; i < m_sections.GetCount(); i ++)
	{
		delete m_sections.GetRawDataArray()[i].data; m_sections.GetRawDataArray()[i].data = NULL;
	}
	m_sections.Reset();

	// delete all the entities and reclaim storage
	for(s32 i = 0; i < m_entities.GetCount(); i ++)
	{
		delete m_entities.GetRawDataArray()[i].data; m_entities.GetRawDataArray()[i].data = NULL;
	}
	m_entities.Reset();

	// delete any child scenes that are left lying around
	for (s32 i=0; i<Data().m_childScenes.GetCount(); i++)
	{
		if (Data().m_childScenes[i])
		{
			delete Data().m_childScenes[i];
		}
	}
	Data().m_childScenes.clear();
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
void CAnimScene::RenderAndAdjustAuthoringTools()
{
	if(Data().m_attachedParentEntity)
	{
		Mat34V parentAttachmentMatrix;
		GetParentMatrix(parentAttachmentMatrix);
		float axisSize = 1.0f;
		//Render at the new local offset (along with controls)
		Matrix34 newSceneOrigin; Vector3 zeroVec(0.f,0.f,0.f);
		bool shouldUpdate = m_sceneOrigin.GetAuthoringHelper().Update( MAT34V_TO_MATRIX34(m_sceneOrigin.GetMatrix()),
			axisSize,
			zeroVec,
			newSceneOrigin,
			MAT34V_TO_MATRIX34(parentAttachmentMatrix) );

		if(shouldUpdate)
		{
			Mat34V newSceneOriginV = MATRIX34_TO_MAT34V(newSceneOrigin);
			m_sceneOrigin.SetMatrix(newSceneOriginV);
		}
	}
}
#endif //__BANK

void CAnimScene::Update(AnimSceneUpdatePhase phase)
{
	switch (phase)
	{
	case kAnimSceneUpdatePreAnim:
		{
			// update the state machine
			// (we only do this once per frame, currently before the anim update)
			s32 stateIterations = 0;
			static s32 s_maxStateIterations = 100;

			//Update authoring tools
#if __BANK
			RenderAndAdjustAuthoringTools();
#endif

			ProcessPreFSM();

			// trap the case where the state has been changed from outside the state machine update
			if (Data().m_state != Data().m_previousState && Data().m_previousState!=State_Invalid)
			{
				// call the on exit for the previous state
				UpdateFSM(Data().m_previousState, OnExit);

				// call the on enter for the new state
				UpdateFSM(Data().m_state, OnEnter);
			}

			do
			{
				// sync up the previous state
				Data().m_previousState = Data().m_state;

				Data().m_canChangeState = true; // state changes are only allowed during OnUpdate calls
				UpdateFSM(Data().m_state, OnUpdate);
				Data().m_canChangeState = false;

				if (Data().m_state != Data().m_previousState)
				{
					// call the on exit for the previous state
					UpdateFSM(Data().m_previousState, OnExit);

					// call the on enter for the new state
					UpdateFSM(Data().m_state, OnEnter);
				}

				stateIterations++;
				animAssertf(stateIterations<s_maxStateIterations, "Infinite state machine loop detected in anim scene!");
			}
			while(Data().m_previousState!=Data().m_state && stateIterations<s_maxStateIterations);
			break;
		}	
	default:
		{
			// no special handling
		}
	}


	// Trigger any events for the sections as appropriate
	CAnimSceneSectionIterator it(*this);
	while(*it)
	{
		(*it)->TriggerEvents(this, phase);
		++it;
	}

	// update any registered child scenes
	for (s32 i=0; i<Data().m_childScenes.GetCount(); i++)
	{
		if (Data().m_childScenes[i])
		{
			Data().m_childScenes[i]->Update(phase);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::ShutdownFSM()
{
	UpdateFSM(Data().m_state, OnExit);
	Data().m_state = State_Initial;
	Data().m_previousState = State_Invalid;
}

//////////////////////////////////////////////////////////////////////////

CAnimScene * CAnimScene::CreateChildScene(CAnimScene::Id sceneId, eAnimSceneFlagsBitSet* pFlags, float relativeTime)
{
	CAnimScene* pScene = rage_new CAnimScene(sceneId, pFlags);
	if (pScene)
	{
		Data().m_childScenes.PushAndGrow(pScene);
		pScene->Data().m_pMasterScene = this;
		pScene->Data().m_masterSceneRelativeTime = relativeTime;
		return pScene;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::DestroyChildScene(CAnimScene* pScene)
{
	animAssertf(Data().m_childScenes.Find(pScene)>-1, "Anim scene %s has been asked to delete a child anim scene it doesn't own!", Data().m_fileName.GetCStr());
	Data().m_childScenes.DeleteMatches(pScene);
	delete pScene;
}

//////////////////////////////////////////////////////////////////////////
float CAnimScene::GetTimeWithSkipping() const
{
	if (Data().m_skipToTime)
	{
		return Data().m_skipTime;
	}

	return Data().m_time;
}

//////////////////////////////////////////////////////////////////////////
// FSM processing

void CAnimScene::ProcessPreFSM()
{
#if __BANK
	// keep the time slider range up to date
	if (m_widgets->m_pTimeSlider && m_widgets->m_pTimeSlider->GetFloatMaxValue()!=Data().m_duration)
	{
		m_widgets->m_pTimeSlider->SetRange(0.0f, Data().m_duration);
	}

	// Keep the time controller widget up to date
	if (!Data().m_skipToTime)
	{
		m_widgets->m_timeControl=Data().m_time;
	}
#endif //__BANK
}

CAnimScene::FSM_Return CAnimScene::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	 FSM_Begin
	 	FSM_State(State_Initial)
	 		FSM_OnUpdate
				// just wait for the signal to start loading the assets
	 			if (Data().m_startPreload)
				{
					if (Data().m_id!=ANIM_SCENE_ID_INVALID && !Data().m_metadataLoaded)
					{
						// start loading the scene metadata
						SetState(State_LoadMetadata); 
					}
					else
					{
						// Metadata was loaded from disk. Move to asset pre-loading immediately.
						SetState(State_PreLoadAssets);
					}					
				}

		FSM_State(State_LoadMetadata)
			FSM_OnEnter
				LoadMetadata_OnEnter();
			FSM_OnUpdate
				LoadMetadata_OnUpdate();
			FSM_OnExit
				LoadMetadata_OnExit();

	 	FSM_State(State_PreLoadAssets)
	 		FSM_OnEnter
	 			PreLoadAssets_OnEnter();
	 		FSM_OnUpdate
	 			PreLoadAssets_OnUpdate();
			FSM_OnExit
				PreLoadAssets_OnExit();

		FSM_State(State_LeadIn)
			FSM_OnEnter
				LeadIn_OnEnter();
			FSM_OnUpdate
				LeadIn_OnUpdate();
			FSM_OnExit
				LeadIn_OnExit();

	 	FSM_State(State_Loaded)
	 		FSM_OnEnter
	 			Loaded_OnEnter();
	 		FSM_OnUpdate
	 			Loaded_OnUpdate();
	 		FSM_OnExit
	 			Loaded_OnExit();

			FSM_State(State_Running)
				FSM_OnEnter
				Running_OnEnter();
			FSM_OnUpdate
				Running_OnUpdate();
			FSM_OnExit
				Running_OnExit();

			FSM_State(State_Paused)
				FSM_OnEnter
				Paused_OnEnter();
			FSM_OnUpdate
				Paused_OnUpdate();
			FSM_OnExit
				Paused_OnExit();

			FSM_State(State_Skipping)
				FSM_OnEnter
				Skipping_OnEnter();
			FSM_OnUpdate
				Skipping_OnUpdate();
			FSM_OnExit
				Skipping_OnExit();

			FSM_State(State_Finish)
				FSM_OnEnter
				Finish_OnEnter();
			FSM_OnUpdate
				Finish_OnUpdate();
			FSM_OnExit
				Finish_OnExit();
	 FSM_End
}

//////////////////////////////////////////////////////////////////////////
// Preload assets state

void CAnimScene::LoadMetadata_OnEnter()
{
	// get the dictionary index

	strLocalIndex metaFileIndex = g_fwMetaDataStore.FindSlotFromHashKey(Data().m_id);
	animAssertf(metaFileIndex != -1, "Unable to find anim scene %s in the metadata store!", Data().m_id.GetCStr());
	if (g_fwMetaDataStore.IsValidSlot(metaFileIndex))
	{
		Data().m_streamingIndex = metaFileIndex;
	}
}
void CAnimScene::LoadMetadata_OnUpdate()
{
	// request until loaded
	if (g_fwMetaDataStore.IsValidSlot(Data().m_streamingIndex))
	{
		g_fwMetaDataStore.StreamingRequest(Data().m_streamingIndex, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);

		if (g_fwMetaDataStore.HasObjectLoaded(Data().m_streamingIndex))
		{
			if (g_fwMetaDataStore.Get(Data().m_streamingIndex))
			{
				// pull out the static copy of the scene from metadata
				CAnimScene* pScene  = g_fwMetaDataStore.Get(Data().m_streamingIndex)->GetObject< CAnimScene >();

				// copy the scene data to our instance
				pScene->CopyTo(this);
				Data().m_metadataLoaded = true;
#if __BANK
				if (IsOwnedByEditor())
				{
					CAnimSceneManager::AddSceneToWidgets(*this);
				}
#endif //__BANK

				// start loading assets
				SetState(State_PreLoadAssets);
			}
			else
			{
				// empty slot index.
				animAssertf(0, "Failed to load anim scene %s from metadata store slot %d: The slot is empty!", Data().m_id.GetCStr(), Data().m_streamingIndex.Get());
				SetState(State_Finish);
			}

			g_fwMetaDataStore.ClearRequiredFlag(Data().m_streamingIndex.Get(), STRFLAG_DONTDELETE);
		}
	}
	else
	{
		SetState(State_Finish);
	}
}
void CAnimScene::LoadMetadata_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// Preload assets state

void CAnimScene::PreLoadAssets_OnEnter()
{
	// run over and init the entities
	for (s32 i=0; i< m_entities.GetCount(); i++)
	{
		(*m_entities.GetItem(i))->Init(this);
	}

	// init all events for all sections
	CAnimSceneSectionIterator it(*this);
	while (*it)
	{
		CAnimSceneSection& section = *(*it);
		section.InitEvents(this);
		++it;
	}

	RecalculateSceneDurations();
}
void CAnimScene::PreLoadAssets_OnUpdate()
{
	// trigger event preload update events for preloads starting on zero
	bool allEventsLoaded = true;

	CAnimSceneSectionIterator it(*this);
	while (*it)
	{
		if (!(*it)->PreloadEvents(this, GetTime()))
		{
			allEventsLoaded = false;
		}
		++it;
	}

	// when all events return ready, move to the Loaded state
	if (allEventsLoaded)
	{
		SetState(State_Loaded);
	}
}
void CAnimScene::PreLoadAssets_OnExit()
{
	CAnimSceneSectionIterator it(*this);
	while (*it)
	{
		(*it)->ExitPreloadEvents(this, GetTime());
		++it;
	}
}

//////////////////////////////////////////////////////////////////////////
// Loaded state
void CAnimScene::Loaded_OnEnter()
{
	// nothing to see here
}
void CAnimScene::Loaded_OnUpdate()
{
	if (IsFlagSet(ASF_AUTO_START_WHEN_LOADED) || Data().m_startScene)
	{
		AnimSceneEntityTypeFilter filter;
		filter.SetAll();
		CAnimSceneEntityIterator entityIt(this, filter);
		entityIt.begin();

		while (*entityIt)
		{
			CAnimSceneEntity& ent = *(*entityIt);
			ent.StartOfScene();
			++entityIt;
		}
	
		CAnimSceneEventIterator<CAnimSceneEvent> eventIt(*this);
		eventIt.begin();

		while (*eventIt)
		{
			CAnimSceneEvent& evt = *(*eventIt);
			evt.StartOfScene(this);
			++eventIt;
		}

// 		if (ShouldUseLeadIns())
// 		{
// 			SetState(State_LeadIn);
// 		}
// 		else
		{
			SetState(State_Running);
		}
	}
}
void CAnimScene::Loaded_OnExit()
{
	// nothing to see here
}

//////////////////////////////////////////////////////////////////////////
// Loaded state
void CAnimScene::LeadIn_OnEnter()
{
// 	// start lead in tasks on the relevant entities
// 	for (s32 i=0; i< m_entities.GetCount(); i++)
// 	{
// 		CAnimSceneEntity* pEnt = (*m_entities.GetItem(i));
// 		// TODO - support this on other entity types (object, vehicle)
// 		if (pEnt && pEnt->GetType()==ANIM_SCENE_ENTITY_PED)
// 		{
// 			// TODO - pass in the current playback list (if it exists)
// 			CAnimScenePed* pScenePed = static_cast<CAnimScenePed*>(pEnt);
// 			if (pScenePed->GetPed(BANK_ONLY(false)))
// 			{
// 				CAnimSceneLeadInData* pData = pScenePed->FindLeadIn(Data().m_playbackList);
// 				if (pData)
// 				{
// 					// create the lead in task and start it on the entity
// 					StartLeadInTask(pScenePed, pData);
// 				}
// 			}
// 		}
// 	}
}
void CAnimScene::LeadIn_OnUpdate()
{
// 	bool leadInFinished = true;
// 
// 	for (s32 i=0; i< m_entities.GetCount(); i++)
// 	{
// 		CAnimSceneEntity* pEnt = (*m_entities.GetItem(i));
// 		// TODO - support this on other entity types (object, vehicle)
// 		if (pEnt && pEnt->GetType()==ANIM_SCENE_ENTITY_PED)
// 		{
// 			CAnimScenePed* pScenePed = static_cast<CAnimScenePed*>(pEnt);
// 			if (FindLeadInTask(pScenePed))
// 			{
// 				leadInFinished = false;
// 			}
// 		}
// 	}
// 
// 	if (leadInFinished)
// 	{
// 		// if all lead in tasks are finished
// 		// proceed to running
// 		SetState(State_Running);
// 	}
}
void CAnimScene::LeadIn_OnExit()
{
// 	// nothing to see here
// 	for (s32 i=0; i< m_entities.GetCount(); i++)
// 	{
// 		CAnimSceneEntity* pEnt = (*m_entities.GetItem(i));
// 		// TODO - support this on other entity types (object, vehicle)
// 		if (pEnt && pEnt->GetType()==ANIM_SCENE_ENTITY_PED)
// 		{
// 			CAnimScenePed* pScenePed = static_cast<CAnimScenePed*>(pEnt);
// 			CTask* pTask = FindLeadInTask(pScenePed);
// 			if (pTask)
// 			{
// 				pTask->RequestTermination();
// 			}
// 		}
// 	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::Running_OnEnter()
{

}
void CAnimScene::Running_OnUpdate()
{
	bool bEnded = false;

	if (Data().m_skipToTime)
	{
		SetState(State_Skipping);
		return;
	}

	if (Data().m_paused)
	{
		// move to the paused state
		SetState(State_Paused);
		return;
	}

	// update time and process events
	float delta = fwTimer::GetTimeStep() * m_rate;
	float endTime = Data().m_time + delta;

	// Synchronize our timer with that of the master scene (if it exists)
	if (GetMasterScene())
	{
		endTime = GetMasterScene()->GetTime() - Data().m_masterSceneRelativeTime;
		delta = endTime - Data().m_time;
	}

	// Process internal looping.  Do this before processing the end of the scene so we can loop right up to the end.
	// Add and subtract SMALL_FLOAT to the looping start and end times when evaluating the looping range.
	// We do this because event ranges are inclusive.  We may have the end of one event at the same time as
	// the start of the next, but we only want to loop the first event.  So adding/subtracting a small fraction
	// allows us to loop within the intended event without starting new events we didn't intend to see.
	if (Data().m_enableEternalLooping)
	{
		const float fAdjustedInternalLoopStart = Data().m_internalLoopStart + SMALL_FLOAT;
		const float fAdjustedInternalLoopEnd = Data().m_internalLoopEnd - SMALL_FLOAT;

		if (endTime > fAdjustedInternalLoopEnd && Data().m_time < fAdjustedInternalLoopEnd)
		{
			endTime -= (fAdjustedInternalLoopEnd - fAdjustedInternalLoopStart);
		}
		else if (endTime < fAdjustedInternalLoopStart && Data().m_time > fAdjustedInternalLoopStart)
		{
			endTime += (fAdjustedInternalLoopEnd - fAdjustedInternalLoopStart);
		}
	}

	// Process end of scene
	if (endTime>Data().m_duration)
	{
		if (IsFlagSet(ASF_LOOPING))
		{
			// wrap the scene
			endTime -= Data().m_duration;
		}
		else
		{
			endTime = Data().m_duration;
			bEnded = true;
		}
	}
	else if (endTime<0.0f)
	{
		if (IsFlagSet(ASF_LOOPING))
		{
			// wrap the scene
			endTime += Data().m_duration;
		}
		else
		{
			// what to do about playing backwards? should the scene end at this point?
			// For now just clamp to the start.
			endTime=0.0f;
		}
	}

	ProcessSections(Data().m_time, endTime, delta);

	// store the new time
	Data().m_time = endTime;

	if (!Data().m_bEditorHoldAtEnd && ( (bEnded && !IsFlagSet(ASF_HOLD_AT_END)) || Data().m_stopScene) )
	{
		SetState(State_Finish);
	}
}
void CAnimScene::Running_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::Paused_OnEnter()
{

}
void CAnimScene::Paused_OnUpdate()
{
	if (Data().m_stopScene)
	{
		SetState(State_Finish);
	}

	if (Data().m_skipToTime)
	{
		SetState(State_Skipping);
		return;
	}

	if (!Data().m_paused)
	{
		// move to the running state
		SetState(State_Running);
		return;
	}

	// process events without updating time
	ProcessSections(Data().m_time, Data().m_time, 0.0f);
}
void CAnimScene::Paused_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::Skipping_OnEnter()
{
	//nothing to see here
}
void CAnimScene::Skipping_OnUpdate()
{
	// trigger event preload update events for the new time
	bool allEventsLoaded = true;

	CAnimSceneSectionIterator it(*this);

	while(*it)
	{
		if (!(*it)->PreloadEvents(this, Data().m_skipTime))
		{
			allEventsLoaded = false;
		}
		++it;
	}

	// when all events return ready, move back to the playing / paused state
	if (allEventsLoaded)
	{
		if (Data().m_paused)
		{
			// move to the paused state
			SetState(State_Paused);
			return;
		}
		else
		{
			// move to the running state
			SetState(State_Running);
			return;
		}
	}
}
void CAnimScene::Skipping_OnExit()
{
	CAnimSceneSectionIterator it(*this);
	while (*it)
	{
		(*it)->ExitPreloadEvents(this, Data().m_skipTime);
		++it;
	}

	Data().m_time = Data().m_skipTime;
	Data().m_skipTime = 0.0f;
	Data().m_skipToTime = false;
}


//////////////////////////////////////////////////////////////////////////

void CAnimScene::Finish_OnEnter()
{

	ShutdownEntitiesAndEvents();
}
void CAnimScene::Finish_OnUpdate()
{
	// all done
}
void CAnimScene::Finish_OnExit()
{

}



//////////////////////////////////////////////////////////////////////////

// bool CAnimScene::ShouldUseLeadIns()
// {
// 	// Do any of our entities need to perform a lead in?
// 	for (s32 i=0; i< m_entities.GetCount(); i++)
// 	{
// 		CAnimSceneEntity* pEnt = (*m_entities.GetItem(i));
// 		if (pEnt && pEnt->GetType()==ANIM_SCENE_ENTITY_PED)
// 		{
// 			// TODO - pass in the current playback list (if it exists)
// 			if (static_cast<CAnimScenePed*>(pEnt)->FindLeadIn()!=NULL)
// 			{
// 				return true;
// 			}
// 		}
// 	}
// 	return false;
// }
// 
// //////////////////////////////////////////////////////////////////////////
// void CAnimScene::StartLeadInTask(CAnimSceneEntity* pEnt, CAnimSceneLeadInData* pData)
// {
// 	if (pEnt && pData)
// 	{
// 		// grab the entity and assert it exists
// 		CPhysical* pPhys = GetPhysicalEntity(pEnt);
// 		animAssertf(pPhys, "StartLeadInTask: entity not found! entity id: %s", pEnt->GetId().GetCStr());
// 
// 		if (!pPhys)
// 		{
// 			return;
// 		}
// 
// 		// assert the clip set exists
// 		if (fwClipSetManager::GetClipSet(pData->m_clipSet.GetId())==NULL)
// 		{
// 			animAssertf(0, "clip set %s does not exist! From updating anim scene entity %s", pData->m_clipSet.GetName().GetCStr(), pEnt->GetId().GetCStr());
// 			return;
// 		}
// 
// 		Mat34V origin = GetSceneOrigin();
// 
// 		CTask* pTask = rage_new CTaskAlignedAnim(pData->m_clipSet.GetId(), origin, 0);
// 
// 		// if everythings good, start the aligned anim task on the entity
// 		if(pPhys->GetIsTypePed())
// 		{
// 			CPed * pPed = static_cast<CPed*>(pPhys);
// 
// 			// If we're running as part of a scenario, we need to pas the task to the scenario task to run as a subtask
// 			CTaskUseScenario* pScenariotask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
// 			if (pScenariotask)
// 			{
// 				pScenariotask->AddToAnimScene(pTask);
// 			}
// 			else
// 			{
// 				pPed->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY, TRUE);
// 			}
// 		}
// 		else if (pPhys->GetIsTypeObject())
// 		{
// 			CObject* pObj = static_cast<CObject*>(pPhys);
// 			pObj->SetTask(pTask);
// 		}
// 		else if (pPhys->GetIsTypeVehicle())
// 		{
// 			CVehicle* pVeh = static_cast<CVehicle*>(pPhys);
// 			pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(pTask, VEHICLE_TASK_SECONDARY_ANIM);
// 		}
// 	}
// }
// 
// //////////////////////////////////////////////////////////////////////////
// CTask* CAnimScene::FindLeadInTask(CAnimSceneEntity* pSceneEnt)
// {
// 	if (pSceneEnt)
// 	{
// 		CPhysical* pPhys = GetPhysicalEntity(pSceneEnt BANK_ONLY(, false));
// 
// 		if (pPhys)
// 		{
// 			if (pPhys->GetIsTypePed())
// 			{
// 				CPed* pPed = static_cast<CPed*>(pPhys);
// 
// 				return pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ALIGNED_ANIM);
// 			}
// 			else if (pPhys->GetIsTypeVehicle())
// 			{
// 				CVehicle* pVeh = static_cast<CVehicle*>(pPhys);
// 
// 				if (pVeh->GetIntelligence()->GetTaskManager())
// 				{
// 					return static_cast<CTask*>(pVeh->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_ALIGNED_ANIM));
// 				}
// 			}
// 			else if (pPhys->GetIsTypeObject())
// 			{
// 				CObject* pObj = static_cast<CObject*>(pPhys);
// 				return pObj->GetObjectIntelligence() ? pObj->GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_ALIGNED_ANIM) : NULL;
// 			}
// 		}		
// 	}
// 
// 	return NULL;
// }

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection* CAnimScene::GetSection(CAnimSceneSection::Id sectionId)
{
	CAnimSceneSection **ppSec = m_sections.SafeGet(sectionId);
	if(ppSec)
	{
		return *ppSec;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection* CAnimScene::GetCurrentSection()
{
	CAnimSceneSection::Id sectionId = FindSectionForTime(GetTime());

	if (sectionId!=ANIM_SCENE_SECTION_ID_INVALID)
	{
		return GetSection(sectionId);
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection::Id CAnimScene::GetCurrentSectionId()
{
	return FindSectionForTime(GetTime());
}

//////////////////////////////////////////////////////////////////////////

float CAnimScene::CalcSectionStartTime(CAnimSceneSection* pSection)
{
	return CalcSectionStartTime(pSection, GetPlayBackList());
}

float CAnimScene::CalcSectionStartTime(CAnimSceneSection* pSection, CAnimScenePlaybackList::Id listId)
{
	CAnimSceneSectionIterator it(*this, false, listId!=GetPlayBackList(), listId);
	float time= 0.0f;

	while (*it && (*it)!=pSection)
	{
		time+=(*it)->GetDuration();
		++it;
	}

	animAssertf((*it) && (*it)==pSection, "Section does not exist in the specified playback list %s! Anim scene %s", listId.GetCStr(), Data().m_id.GetCStr());
	return time;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::ProcessSections(float startTime, float endTime, float deltaTime)
{
	CAnimSceneSectionIterator it(*this);
	while(*it)
	{
		(*it)->ProcessEvents(this, startTime, endTime, deltaTime);
		++it;
	}
}


//////////////////////////////////////////////////////////////////////////

void CAnimScene::ShutdownEntitiesAndEvents()
{
	if (!Data().m_eventsShutdown)
	{
		// shut down our events
		for (s32 i=0; i<m_sections.GetCount(); i++)
		{
			CAnimSceneSection* pSection = *m_sections.GetItem(i);
			if (pSection && pSection->AreEventsInitialised())
			{
				pSection->ShutdownEvents(this);
			}
		}

		// shut down our entities
		for (s32 i=0; i< m_entities.GetCount(); i++)
		{
			(*m_entities.GetItem(i))->Shutdown(this);
		}

		Data().m_eventsShutdown = true;
	}
}

#if USE_ANIM_SCENE_DICTIONARIES

//////////////////////////////////////////////////////////////////////////
bool CAnimScene::Load(s32 UNUSED_PARAM(dictIndex), CAnimScene::Id UNUSED_PARAM(hash))
{
	// add dictionary loading code here
	animAssertf(0, "TODO - implement anim scene dictionary loading");
	return false;
}
bool CAnimScene::Load(atHashString UNUSED_PARAM(dictIndex), CAnimScene::Id UNUSED_PARAM(hash))
{
	// add dictionary loading code here
	animAssertf(0, "TODO - implement anim scene dictionary loading");
	return false;
}

#endif // USE_ANIM_SCENE_DICTIONARIES

//////////////////////////////////////////////////////////////////////////
void CAnimScene::CopyEntities(CAnimScene* pCopyTo, bool overwriteExisting /*= false*/)
{
	for (s32 i=0; i<m_entities.GetCount(); i++)
	{
		if (*m_entities.GetItem(i))
		{	
			// don't overwrite entities that have already been applied to the other scene (unless explicitly allowed)
			if (pCopyTo->GetEntity(*m_entities.GetKey(i))==NULL)
			{
				pCopyTo->AddEntity(*m_entities.GetKey(i), (*(m_entities.GetItem(i)))->Copy());
			}
			else
			{
#if __ASSERT
				CAnimSceneEntity::Id id = (*(m_entities.GetKey(i)));
				eAnimSceneEntityType existingType = pCopyTo->GetEntity(*(m_entities.GetKey(i)))->GetType();
				eAnimSceneEntityType newType = (*m_entities.GetItem(i))->GetType();
				// just assert we don't have a type mismatch
				animAssertf(existingType==newType, "Mismatched entity types found loading anim scene %s: Entity name %s, existing type:%s, type from data: %s", 
					pCopyTo->Data().m_id.GetCStr(), 
					id.GetCStr(), 
					CAnimSceneEntity::GetEntityTypeName(existingType),
					CAnimSceneEntity::GetEntityTypeName(newType)
					);

#endif // __ASSERT
				if (overwriteExisting)
				{
					pCopyTo->ReplaceEntity(*m_entities.GetKey(i), (*(m_entities.GetItem(i)))->Copy());
				}
			}
		}
	}
}

// PURPOSE: Copy any section data to the new scene
void CAnimScene::CopySections(CAnimScene* pCopyTo)
{	
	for (s32 i=0; i<m_sections.GetCount(); i++)
	{
		if (*m_sections.GetItem(i))
		{
			pCopyTo->AddSection(*m_sections.GetKey(i), (*(m_sections.GetItem(i)))->Copy());
		}
	}
}

// PURPOSE: Copy any section data to the new scene
void CAnimScene::CopyPlaybackLists(CAnimScene* pCopyTo)
{	
	for (s32 i=0; i<m_playbackLists.GetCount(); i++)
	{
		if (*m_playbackLists.GetItem(i))
		{
			pCopyTo->AddPlaybackList(*m_playbackLists.GetKey(i), (*(m_playbackLists.GetItem(i)))->Copy());
		}
	}
}

//////////////////////////////////////////////////////////////////////////

float CAnimScene::GetDuration() const
{
	return Data().m_duration;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SetInternalLooping(float startTime, float endTime)
{
	// Clamp looping range to the end of the scene.
	// The scene time is only looped when we pass the end of the looping event.  So if this looping end
	// time is after the scene duration, then it's possible to get a scene time that lands after the
	// end of the scene but before the end of the looping range.  The scene would therefore finish, but our
	// intention was to loop.
	if (endTime > Data().m_duration)
	{
		endTime = Data().m_duration;
	}

	Data().m_internalLoopStart = startTime;
	Data().m_internalLoopEnd = endTime;
	Data().m_enableEternalLooping = true;
}

//////////////////////////////////////////////////////////////////////////
CAnimSceneEntity* CAnimScene::GetEntity(CAnimSceneEntity::Id id)
{
	if (GetMasterScene())
	{
		return GetMasterScene()->GetEntity(id);
	}
	else
	{
		if(id != ANIM_SCENE_ID_INVALID)
		{
			CAnimSceneEntity **ppEntity = m_entities.SafeGet(id);
			if(ppEntity)
			{
				return *ppEntity;
			}
		}

		return NULL;
	}
}

CPhysical* CAnimScene::GetPhysicalEntity(CAnimScene::Id id BANK_ONLY(, bool createDebugEntIfRequired /*= true*/))
{
	CAnimSceneEntity* pEnt = GetEntity(id);

	return GetPhysicalEntity(pEnt BANK_ONLY(, createDebugEntIfRequired));
}

CPhysical* CAnimScene::GetPhysicalEntity(CAnimSceneEntity* pEnt BANK_ONLY(, bool createDebugEntIfRequired/* = true*/))
{
	if (pEnt)
	{
		switch (pEnt->GetType())
		{
		case ANIM_SCENE_ENTITY_PED:
			{
				return static_cast<CAnimScenePed*>(pEnt)->GetPed(BANK_ONLY(createDebugEntIfRequired));
			}
			break;
		case ANIM_SCENE_ENTITY_OBJECT:
			{
				return static_cast<CAnimSceneObject*>(pEnt)->GetObject(BANK_ONLY(createDebugEntIfRequired));
			}
			break;
		case ANIM_SCENE_ENTITY_VEHICLE:
			{
				return static_cast<CAnimSceneVehicle*>(pEnt)->GetVehicle(BANK_ONLY(createDebugEntIfRequired));
			}
			break;
		default:
			{
				return NULL;
			}
			break;
		}		
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::RemoveEntityFromScene(CAnimSceneEntity::Id entity)
{
	// run over the entities in the scene
	AnimSceneEntityTypeFilter typeFilter;
	typeFilter.SetAll();
	CAnimSceneEntityIterator it(this, typeFilter);

	it.begin();

	while (*it)
	{
		CAnimSceneEntity* pEnt = (*it);

		if(!animVerifyf(pEnt,"Dynamic cast failed from CAnimSceneEntityIterator to CAnimSceneEntity* whilst removing entity %s from scene %s",
			entity.TryGetCStr(),this->GetName().TryGetCStr()) )
		{
			++it;
			continue;
		}

		if (pEnt->GetId()==entity)
		{
			pEnt->OnRemovedFromSceneEarly();

			// walk over the entity's events and disable them
			CAnimSceneEventIterator<CAnimSceneEvent> it(*this);
			it.begin();

			while (*it)
			{
				CAnimSceneEvent& evt = *(*it);

				if (evt.GetPrimaryEntityId()==entity)
				{			
					evt.SetDisabled(true);
				}

				++it;
			}
		}
		else
		{
			pEnt->OnOtherEntityRemovedFromSceneEarly(entity);
		}
		++it;
	}
}

//////////////////////////////////////////////////////////////////////////

const AnimSceneEntityLocationData* CAnimScene::GetEntityLocationData(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackListId)
{
	CAnimSceneEntity* pEnt = GetEntity(entityId);

	if (pEnt && pEnt->SupportsLocationData())
	{
		return pEnt->GetLocationData(playbackListId);		
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScene::GetEntityInitialLocation(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackListId, Mat34V_InOut location)
{
	const AnimSceneEntityLocationData* pSit = GetEntityLocationData(entityId, playbackListId);

	if (pSit)
	{
		Mat34VFromQuatV(location, pSit->m_startRot, pSit->m_startPos);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScene::GetEntityFinalLocation(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackListId, Mat34V_InOut location)
{
	const AnimSceneEntityLocationData* pSit = GetEntityLocationData(entityId, playbackListId);

	if (pSit)
	{
		Mat34VFromQuatV(location, pSit->m_endRot, pSit->m_endPos);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
bool CAnimScene::CalcEntityLocationData(CAnimSceneEntity::Id entityId, CAnimScenePlaybackList::Id playbackList, AnimSceneEntityLocationData& situation)
{
	// search the anim scene for the last play anim event
	bool overridePlaybackList = playbackList!=GetPlayBackList();
	CAnimSceneEventIterator<CAnimScenePlayAnimEvent> it(*this, overridePlaybackList, playbackList);
	CAnimScenePlayAnimEvent* pFirstEvent = NULL;
	CAnimScenePlayAnimEvent* pLastEvent = NULL;
	float firstEventStart = FLT_MAX;
	float lastEventEnd = -FLT_MAX;

	it.begin();
	while (*it)
	{
		CAnimScenePlayAnimEvent* pAnimEvent = (*it);
		if (pAnimEvent->GetEntityId()==entityId)
		{
			float eventStart = overridePlaybackList ? pAnimEvent->GetStart(this, playbackList.GetHash()) : pAnimEvent->GetStart();
			if (eventStart < firstEventStart)
			{
				pFirstEvent = pAnimEvent;
				firstEventStart = eventStart;
			}
			float eventEnd = overridePlaybackList ? pAnimEvent->GetEnd(this, playbackList.GetHash()) : pAnimEvent->GetEnd();
			if (eventEnd > lastEventEnd)
			{
				pLastEvent = pAnimEvent;
				lastEventEnd = eventEnd;
			}
		}

		++it;
	}

	Mat34V location(V_IDENTITY);
	if (pFirstEvent)
	{
		pFirstEvent->CalcMoverOffset(this, location, 0.0f);
		situation.m_startPos =location.d();
		situation.m_startRot = QuatVFromMat34V(location);
		situation.m_startTime = firstEventStart;
	}

	if (pLastEvent)
	{
		pLastEvent->CalcMoverOffset(this, location, pLastEvent->GetEnd()-pLastEvent->GetStart());
		situation.m_endPos = location.d();
		situation.m_endRot = QuatVFromMat34V(location);
		situation.m_endTime = lastEventEnd;
	}

	return pFirstEvent!=NULL;
}

#endif // __BANK


//////////////////////////////////////////////////////////////////////////
void CAnimScene::Reset()
{
	ShutdownFSM();

	ShutdownEntitiesAndEvents();

	Data().m_startPreload=false;
	Data().m_startScene=false;
	Data().m_stopScene=false;
	Data().m_paused=false;
	Data().m_eventsShutdown=false;
	Data().m_time=0.0f;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SetSceneOrigin(Mat34V_ConstRef mat)
{
	 m_sceneOrigin.SetMatrix(mat);
	 Data().m_sceneOriginOverridden = true;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SetPlayBackList(CAnimScenePlaybackList::Id listId)
{
	CAnimSceneSection::Id currentSectionId = ANIM_SCENE_SECTION_ID_INVALID;
	CAnimSceneSection* pCurrentSection = NULL;
	float sectionTime = 0.0f;
	if (IsPlayingBack())
	{
		currentSectionId = GetCurrentSectionId();
		pCurrentSection = GetSection(currentSectionId);
		sectionTime = pCurrentSection ? GetTime() - pCurrentSection->GetStart() : 0.0f;
	}

	Data().m_playbackList = listId;

	// Initialise any events in sections that haven't already been initialised
	if (GetState() >= State_PreLoadAssets)
	{
		CAnimSceneSectionIterator it(*this, false);
		while (*it)
		{
			CAnimSceneSection& section = *(*it);
			if (ContainsSection(it.GetId()))
			{
				if (!section.AreEventsInitialised())
				{
					section.InitEvents(this);
				}
			}
			else
			{
				if (section.AreEventsInitialised())
				{
					section.ShutdownEvents(this);
				}
			}

			++it;
		}
	}

	RecalculateSceneDurations();

	// jump to the new matching time
	if (pCurrentSection && ContainsSection(currentSectionId))
	{
		Data().m_time = pCurrentSection->GetStart() + sectionTime; 
	}
}


#if __BANK

// Load the anim scene metadata from a meta file on disk
bool CAnimScene::Load(const char * name)
{
	bool bfileLoaded = false;

	// clear out any data associated with this anim scene
	Shutdown();
	Init();

	Displayf("Attempting to load anim scene '%s'", name);

	ASSET.PushFolder(sm_LoadSaveDirectory.GetCStr());

	if (PARSER.LoadObject(CAnimSceneManager::GetFileNameFromSceneName(name).c_str(), "pso.meta", *this))
	{
		bfileLoaded = true;
		Data().m_fileName.SetFromString(name);
		animAssertf(Validate(),"[Anim Scene %s] was invalid (at least one play [camera] anim event length is incorrect).  Please bug with output from TTY/console as specific anim event information is there (warnings).", 
			this->GetName().TryGetCStr());
	}

	ASSET.PopFolder();

	Displayf("Anim scene load '%s' - %s", name, bfileLoaded ? "Successful" : "Failed");

	return bfileLoaded;
}
// Save the anim scene metadata to a meta file on disk
bool CAnimScene::Save(const char * name)
{
	bool bfileSaved = false;

	PreSave();

	Displayf("Attempting to save anim scene '%s'", name);

	if (!name || name[0] == '\0')
	{
		Assertf(0, "anim scene name must have at least one char. Saving file as \"default\"");
		name = "default";
	}

	ASSET.PushFolder(sm_LoadSaveDirectory.GetCStr());

	atString sFullPath(sm_LoadSaveDirectory.GetCStr());
	sFullPath += "/";
	sFullPath += CAnimSceneManager::GetFileNameFromSceneName(name).c_str();

	// Create directory if it doesn't exist
	bool bSaveDirExists = ASSET.CreateLeadingPath(sFullPath.c_str());
	if (!bSaveDirExists)
	{
		Displayf("Anim scene save '%s' - Could not create save directory", name);
	}

	if (PARSER.SaveObject(CAnimSceneManager::GetFileNameFromSceneName(name).c_str(), "pso.meta", this))
	{
		bfileSaved = true;
	}

	ASSET.PopFolder();

	Displayf("Anim scene save '%s' - %s", name, bfileSaved ? "Successful" : "Failed");

	return bfileSaved;
}

void CAnimScene::PreSave()
{
	// run over the entities and generate a situation for each playback list
	for (s32 i=0; i<m_entities.GetCount(); i++)
	{
		CAnimSceneEntity* pEnt = *m_entities.GetItem(i);
		animAssert(pEnt);
		if (pEnt->SupportsLocationData())
		{
			CAnimSceneEntity::Id entityId = *m_entities.GetKey(i);
			AnimSceneEntityLocationData sit;
			CalcEntityLocationData(*m_entities.GetKey(i), ANIM_SCENE_PLAYBACK_LIST_ID_INVALID, sit);
			pEnt->SetLocationData(ANIM_SCENE_PLAYBACK_LIST_ID_INVALID, sit);

			for (s32 i=0; i<m_playbackLists.GetCount(); i++)
			{
				AnimSceneEntityLocationData sit;
				CAnimScenePlaybackList::Id id = *m_playbackLists.GetKey(i);
				CalcEntityLocationData(entityId, id, sit);
				pEnt->SetLocationData(id.GetHash(), sit);
			}
		}		
	}

	for (s32 i=0 ;i<m_playbackLists.GetCount(); i++)
	{
		CAnimScenePlaybackList** ppList = m_playbackLists.GetItem(i);
		if (*ppList)
		{
			(*ppList)->CullEmpty();
		}
	}

}
// Add the anim scene editing widgets to the passed in RAG group
void CAnimScene::AddWidgets(bkBank* pBank, bool addGroup)
{
	if (addGroup)
	{
		atVarString sceneName("%s (scene %d)", Data().m_fileName.GetCStr(), CAnimSceneManager::FindInstId(this));
		m_widgets->m_pSceneGroup = pBank->PushGroup(sceneName);
	}

	{
		PARSER.AddWidgets(*pBank, this);
	}

	if (addGroup)
	{
		pBank->PopGroup();
	}
}

void CAnimScene::OnPreAddWidgets(bkBank& UNUSED_PARAM(bank))
{

}

void CAnimScene::OnPostAddWidgets(bkBank& bank)
{
	m_widgets->m_pBank = &bank;

	bkGroup* pSceneOriginGroup = (bkGroup*)FindWidget(bank.GetCurrentGroup(), "sceneOrigin");
	if (pSceneOriginGroup)
	{
		pSceneOriginGroup->AddButton("Set origin to selected entity coords", datCallback(MFA(CAnimScene::SetOriginToEntityCoords), (datBase*)this));
		pSceneOriginGroup->AddButton("Set origin to camera coords", datCallback(MFA(CAnimScene::SetOriginToCamCoords), (datBase*)this));
		pSceneOriginGroup->AddButton("Warp player to scene origin", datCallback(MFA(CAnimScene::MovePlayerToSceneOrigin), (datBase*)this));
		bkGroup* pAttachmentGroup = pSceneOriginGroup->AddGroup("Attachment Prototyping (not saved)");
		pAttachmentGroup->AddButton("Attach (parent) scene to selected (use sceneOrigin matrix)",  datCallback(MFA(CAnimScene::AttachToSelected), (datBase*)this));
		pAttachmentGroup->AddButton("Attach (parent) scene to selected (preserve current offset)",  datCallback(MFA(CAnimScene::AttachToSelectedPreservingLocation), (datBase*)this));
		pAttachmentGroup->AddButton("Detach scene",  datCallback(MFA(CAnimScene::DetachFromEntity), (datBase*)this));
		pAttachmentGroup->AddButton("Detach scene (preserve location)",  datCallback(MFA(CAnimScene::DetachFromEntityPreservingLocation), (datBase*)this));
	}
}

//////////////////////////////////////////////////////////////////////////

bkWidget* CAnimScene::FindWidget(bkWidget* parent, const char * widgetName)
{
	bkWidget* pChild = parent->GetChild();

	while (pChild && strcmp(pChild->GetTitle(), widgetName))
	{
		pChild = pChild->GetNext();
	}

	return pChild;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScene::Load()
{
	// shut down and remove any scene widgets
	ShutdownWidgets();

	bool bResult = Load(Data().m_fileName.GetCStr());

	CAnimSceneManager::AddSceneToWidgets(*this);

	animAssertf(Validate(),"Anim Scene failed to validate (at least one play anim or play camera anim event length is incorrect). Please bug with output from TTY/console as specific anim event information is there (warnings).");

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimScene::Save()
{
	// TODO - success message
	return Save(Data().m_fileName.GetCStr());
}

//////////////////////////////////////////////////////////////////////////
void CAnimScene::Delete()
{
	CAnimSceneManager::BankDestroyAnimScene(this);
}

//////////////////////////////////////////////////////////////////////////
void CAnimScene::TogglePause()
{
	SetPaused(!GetPaused());
}

//////////////////////////////////////////////////////////////////////////
void CAnimScene::DeleteEntity(CAnimSceneEntity* pEnt)
{
	s32 index = -1;
	
	for (s32 i=0; i< m_entities.GetCount() && index==-1; i++)
	{
		if (*m_entities.GetItem(i)==pEnt)
			index = i;
	}
	
	if (index>=0 && index<m_entities.GetCount())
	{
		CAnimSceneEntity::Id id = *m_entities.GetKey(index);
		m_entities.Remove(index);
		pEnt->Shutdown(this);
		delete pEnt;

		// kill the widgets too
		if (m_widgets->m_pEntityGroup)
		{
			bkWidget* pEntGroup = FindWidget(m_widgets->m_pEntityGroup, id.GetCStr());
			if (pEntGroup)
			{
				pEntGroup->Destroy();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimScene::DeleteSection(CAnimSceneSection* pSec)
{
	s32 index = -1;

	if (pSec->AreEventsInitialised())
	{
		pSec->ShutdownEvents(this);
	}

	for (s32 i=0; i< m_sections.GetCount() && index==-1; i++)
	{
		if (*m_sections.GetItem(i)==pSec)
			index = i;
	}

	if (index>=0 && index<m_sections.GetCount())
	{
		CAnimSceneEntity::Id id = *m_sections.GetKey(index);
		m_sections.Remove(index);
		delete pSec;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimScene::DeletePlaybackList(CAnimScenePlaybackList* pList)
{
	s32 index = -1;

	for (s32 i=0; i< m_playbackLists.GetCount() && index==-1; i++)
	{
		if (*m_playbackLists.GetItem(i)==pList)
			index = i;
	}

	if (index>=0 && index<m_playbackLists.GetCount())
	{
		CAnimSceneEntity::Id id = *m_playbackLists.GetKey(index);
		m_playbackLists.Remove(index);
		delete pList;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::AddSectionToPlaybackList(CAnimSceneSection::Id sectionId, CAnimScenePlaybackList::Id listId, s32 index /*=-1*/)
{
	CAnimScenePlaybackList* pList = GetPlayBackList(listId);

	if (pList)
	{
		pList->AddSection(sectionId, index);
	}

	RecalculateSceneDurations();
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::RemoveSectionFromPlaybackList(CAnimSceneSection::Id sectionId, CAnimScenePlaybackList::Id listId)
{
	CAnimScenePlaybackList* pList = GetPlayBackList(listId);

	if (pList)
	{
		pList->RemoveSection(sectionId);
	}

	RecalculateSceneDurations();
}

#if __BANK

//////////////////////////////////////////////////////////////////////////

void CAnimScene::DeleteEvent(CAnimSceneEvent* pEvt)
{
	CAnimSceneSectionIterator it(*this, true);

	while (*it)
	{
		(*it)->DeleteEvent(pEvt);
		++it;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::DeleteAllEventsForEntity(CAnimSceneEntity::Id entityId)
{
	CAnimSceneSectionIterator it(*this, true);

	while (*it)
	{
		(*it)->DeleteAllEventsForEntity(entityId);
		++it;
	}
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScene::GetShouldDeleteEntityFromWidgetCB() 
{
	return m_widgets->m_DeleteEntityFromWidgetCB; 
}

void CAnimScene::SetShouldDeleteEntityFromWidgetCB(bool shouldDeleteEntity)
{
	m_widgets->m_DeleteEntityFromWidgetCB = shouldDeleteEntity;
}

void CAnimScene::SetShouldDeleteEntityCB()
{
	SetShouldDeleteEntityFromWidgetCB(true); 
};
#endif
//////////////////////////////////////////////////////////////////////////

void CAnimScene::GetEntityNames(atArray<const char *>& names)
{
	for (s32 i=0; i<m_entities.GetCount(); i++)
	{
		names.PushAndGrow(m_entities.GetKey(i)->GetCStr());
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::Skip()
{
	SetPaused(true);
	Skip(m_widgets->m_timeControl);
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::ShutdownWidgets()
{
	m_widgets->Shutdown();
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::InitEntityEditing()
{
	// first off, initialise the static type arrays, if we haven't done so already
	if (sm_EntityTypeNames.GetCount()==0)
	{
		const parStructure* pAnimSceneEntityStruct = PARSER.FindStructure("CAnimSceneEntity");

		parManager::StructureMap::ConstIterator it = PARSER.GetStructureMap().CreateIterator();

		for( ; !it.AtEnd(); it.Next())
		{
			if (pAnimSceneEntityStruct && it.GetData()->IsSubclassOf(pAnimSceneEntityStruct))
			{
				if (it.GetData()!=pAnimSceneEntityStruct)
				{
					sm_EntityTypeNames.PushAndGrow(it.GetData()->GetName());
					atString str(it.GetData()->GetName());
					sm_FriendlyEntityTypeStrings.PushAndGrow(str);
					sm_FriendlyEntityTypeStrings.Top().Replace("CAnimScene", "");
					sm_FriendlyEntityTypeNames.PushAndGrow(sm_FriendlyEntityTypeStrings.Top().c_str());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

atString CAnimScene::GetDebugSummary()
{
	s32 numSections = 0;
	s32 numEvents = 0;
	s32 numChildEvents=0;

	
	CAnimSceneSectionIterator it(*this);

	while (*it)
	{
		CAnimSceneEventList& events = (*it)->GetEvents();
		numEvents+=events.GetEventCount();
		for (s32 i=0; i<events.GetEventCount(); i++)
		{
			numChildEvents += events.GetEvent(i)->CountChildren();
		}
		numSections++;
		++it;
	}
	
	return atVarString("%s: %d entities, %d sections, %d events (%d children). State: %s, Time: %.3f/%.3f, Rate:%.3f, pos: (%.3f, %.3f, %.3f)",
		Data().m_id.GetHash()!=0 ? Data().m_id.GetCStr() : Data().m_fileName.GetCStr(), m_entities.GetCount(), numSections, numEvents, numChildEvents,
		GetStaticStateName(Data().m_state), Data().m_time, Data().m_duration, m_rate,
		m_sceneOrigin.GetMatrix().d().GetXf(), m_sceneOrigin.GetMatrix().d().GetYf(), m_sceneOrigin.GetMatrix().d().GetZf());
}

//////////////////////////////////////////////////////////////////////////


#if __DEV
void CAnimScene::DebugRender(debugPlayback::OnScreenOutput& mainOutput)
{
	float axisSize = 1.0f;
	if(!Data().m_attachedParentEntity)
	{
		// render the scene matrix and id
		grcDebugDraw::Axis(m_sceneOrigin.GetMatrix(), axisSize, true);

		atVarString sceneText("scene %d: %s", CAnimSceneManager::FindInstId(this), Data().m_fileName.TryGetCStr() ? Data().m_fileName.GetCStr() : "");
		grcDebugDraw::Text(m_sceneOrigin.GetMatrix().d(), Color_blue, sceneText.c_str());
	}
	else //has a parent
	{
		Color32 Color_transparent_grey = Color_grey;
		Color_transparent_grey.SetAlpha(160);

		Mat34V parentAttachmentMatrix;
		GetParentMatrix(parentAttachmentMatrix);

		grcDebugDraw::Axis(MAT34V_TO_MATRIX34(parentAttachmentMatrix),axisSize/2.0f,true,1);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(parentAttachmentMatrix.d()), VEC3V_TO_VECTOR3(GetSceneOrigin().d()), Color_transparent_grey, Color_transparent_grey);

		atVarString sceneText("scene offset %d: %s", CAnimSceneManager::FindInstId(this), Data().m_fileName.TryGetCStr() ? Data().m_fileName.GetCStr() : "");
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetSceneOrigin().d()), Color_blue, sceneText.c_str());
	}

	if (IsDebugSelected())
	{
		//debug render entities and events
		mainOutput.PushIndent();
		{
			if (IsInternalLoopingEnabled())
			{
				mainOutput.AddLine("Internal looping enabled: %.3f - %.3f", Data().m_internalLoopStart, Data().m_internalLoopEnd);
			}
			mainOutput.AddLine("----%d Entities---", m_entities.GetCount());
			mainOutput.PushIndent();
			{
				for (s32 i=0; i<m_entities.GetCount(); i++)
				{
					CAnimSceneEntity* pEnt = *m_entities.GetItem(i);
					if (pEnt)
					{
						pEnt->DebugRender(this, mainOutput);
					}
				}	
			}
			mainOutput.PopIndent();
			mainOutput.AddLine("----Sections---");
			mainOutput.PushIndent();
			{
				for (s32 i=0; i<m_sections.GetCount(); i++)
				{
					CAnimSceneSection* pSec = *m_sections.GetItem(i);
					if (pSec)
					{
						mainOutput.AddLine("----%s---", (*m_sections.GetKey(i)).GetCStr());
						pSec->DebugRender(this, mainOutput);
					}
				}	
			}
			mainOutput.PopIndent();


			mainOutput.AddLine("----%d Child scenes---", Data().m_childScenes.GetCount());
			mainOutput.PushIndent();
			{
				for (s32 i=0; i<Data().m_childScenes.GetCount(); i++)
				{
					CAnimScene* pScene = Data().m_childScenes[i];
					if (pScene)
					{
						if (mainOutput.AddLine("%d: %s", i, pScene->GetDebugSummary().c_str()) && CAnimSceneManager::IsMouseDown())
						{
							// select or deselect the scene
							pScene->SetDebugSelected(!pScene->IsDebugSelected());
						}
						if (pScene->IsDebugSelected())
						{
							pScene->DebugRender(mainOutput);
						}
					}
				}	
			}
			mainOutput.PopIndent();
		}
		mainOutput.PopIndent();
	}	
}
#endif //__DEV

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SceneEditorRender(CAnimSceneEditor& editor)
{
	CAnimSceneSectionIterator it(*this);

	while (*it)
	{
		(*it)->SceneEditorRender(this, editor);
		++it;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SetOriginToEntityCoords()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt)
	{
		SetSceneOrigin(pEnt->GetMatrix());
	}
	else if (FindPlayerPed())
	{
		SetSceneOrigin(FindPlayerPed()->GetMatrix());
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SetOriginToCamCoords()
{
	const camBaseCamera* pCam = camManager::GetSelectedCamera();

	if (pCam)
	{
		SetSceneOrigin(MATRIX34_TO_MAT34V(pCam->GetFrame().GetWorldMatrix()));
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::MovePlayerToSceneOrigin()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt)
	{
		pEnt->SetMatrix(MAT34V_TO_MATRIX34(m_sceneOrigin.GetMatrix()));
	}
	else if (FindPlayerPed())
	{
		FindPlayerPed()->SetMatrix(MAT34V_TO_MATRIX34(m_sceneOrigin.GetMatrix()));
	}
}

void CAnimScene::AttachToSelected()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt)
	{
		AttachToEntity(pEnt);
	}
	else
	{
		Displayf("No entity selected when attaching anim scene %s.", this->GetName().TryGetCStr());
	}
}

void CAnimScene::AttachToSelectedPreservingLocation()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt)
	{
		AttachToEntityPreservingLocation (pEnt);
	}
	else
	{
		Displayf("No entity selected when attaching anim scene %s.", this->GetName().TryGetCStr());
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::SceneEditorAddEvent(CAnimSceneEvent* pEvt, CAnimSceneSection::Id id)
{
	 AddEvent(pEvt, id);

	 // if we've already initialised our events, make sure we also init the new one
	 if (GetState()>=State_PreLoadAssets)
	 {
		 pEvt->Init(this, GetSection(id));
	 }
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScene::RenameEntity(Id oldName, Id newName)
{
	if (GetEntity(newName))
	{
		// Entity of the new name already exists.
		return false;
	}

	CAnimSceneEntity **ppEntity = m_entities.SafeGet(oldName);
	if(ppEntity && 
		animVerifyf(CAnimSceneEntity::IsValidId(newName), "Anim scene entity new id is invalid: %s", newName.TryGetCStr() ) )
	{
		// Entity exists
		CAnimSceneEntity* pEnt = (*ppEntity);

		// Remove current entry in map
		m_entities.RemoveKey(oldName);

		// Rename it
		pEnt->SetId(newName);
		
		// Reinsert with new id (key)
		m_entities.Insert(newName, pEnt);
		m_entities.FinishInsertion();
	}
	else
	{
		// Entity does not exist, nothing to rename
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::AddNewSection(CAnimSceneSection::Id id)
{
	if (animVerifyf(!GetSection(id), "Anim scene section %s already exists!", id.GetCStr()) &&
		animVerifyf(CAnimSceneSection::IsValidId(id), "Anim scene section id is invalid: %s", id.TryGetCStr()) )
	{
		AddSection(id, rage_new CAnimSceneSection());
	}
	
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::AddNewPlaybackList(CAnimScenePlaybackList::Id id)
{
	if (animVerifyf(!GetPlayBackList(id), "Anim scene playback list %s already exists!", id.GetCStr()) &&
		animVerifyf(CAnimScenePlaybackList::IsValidId(id), "Anim scene playback list id is invalid: %s", id.TryGetCStr()))
	{
		AddPlaybackList(id, rage_new CAnimScenePlaybackList());
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::DeleteSection(CAnimSceneSection::Id id)
{
	CAnimSceneSection* pSec = GetSection(id);
	if (animVerifyf(pSec, "Anim scene section %s doesn't exist!", id.GetCStr()))
	{
		// remove the section id from all playback lists
		for (s32 i=0; i<m_playbackLists.GetCount(); i++)
		{
			if (m_playbackLists.GetItem(i))
			{
				(*m_playbackLists.GetItem(i))->RemoveSection(id);
			}			
		}
		DeleteSection(pSec);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScene::DeletePlaybackList(CAnimScenePlaybackList::Id id)
{
	CAnimScenePlaybackList* pList = GetPlayBackList(id);
	if (animVerifyf(pList, "Anim scene playback list %s doesn't exist!", id.GetCStr()))
	{
		DeletePlaybackList(pList);
	}
}


#endif //__BANK

//////////////////////////////////////////////////////////////////////////

CAnimSceneSection::Id CAnimScene::FindSectionForTime(float time)
{
	CAnimSceneSectionIterator it(*this);

	CAnimSceneSection* pSec = NULL;
	CAnimSceneSection::Id lastId = ANIM_SCENE_SECTION_ID_INVALID;
	while (*it)
	{
		CAnimSceneSection& section = *(*it);
		if ( (section.GetStart()<=time && section.GetEnd()>time) )
		{
			return it.GetId();
		}
		pSec = (*it);
		lastId = it.GetId();
		++it;
	}

	// if the time matches the end of the scene, return the final section
	if (pSec && (pSec->GetEnd()==time && GetDuration()==time))
	{
		return lastId;
	}

	return ANIM_SCENE_SECTION_ID_INVALID;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScene::ContainsSection(CAnimSceneSection::Id sectionId)
{

	CAnimSceneSectionIterator it(*this);

	while (*it)
	{
		if (it.GetId().GetHash()==sectionId.GetHash())
		{
			return true;
		}
		++it;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

// PURPOSE: trigger a recalculation of all section durations and start times (and update the scene duration)
void CAnimScene::RecalculateSceneDurations()
{
	CAnimSceneSectionIterator it(*this);

	float sceneDuration = 0.0f;

	while(*it)
	{
		CAnimSceneSection& section = *(*it);
		section.RecalculateDuration();
		section.SetStart(sceneDuration);
		sceneDuration+=section.GetDuration();
		++it;
	}

	Data().m_duration = sceneDuration;
}

rage::Mat34V_ConstRef CAnimScene::GetSceneOrigin()
{
	return GetSceneMatrix().GetMatrix();
}

CAnimSceneMatrix& CAnimScene::GetSceneMatrix()
{
	if (GetMasterScene())
	{
		if(!IsAttachedToParent())
		{
			//quickly return master scene matrix
			return GetMasterScene()->GetSceneMatrix();
		}
		m_transformedSceneMatrix.SetMatrix(GetMasterScene()->GetSceneOrigin());
	}
	else
	{
		if(!IsAttachedToParent())
		{
			//quickly return scene matrix
			return m_sceneOrigin;
		}
		m_transformedSceneMatrix.SetMatrix(m_sceneOrigin.GetMatrix());
	}
	Mat34V parentMatrix(V_IDENTITY);
	Mat34V sceneMatrix(m_transformedSceneMatrix.GetMatrix());//make a copy as .GetMatrix is constref

	GetParentMatrix(parentMatrix);
	Transform(sceneMatrix, parentMatrix, sceneMatrix);

	m_transformedSceneMatrix.SetMatrix(sceneMatrix);
	return m_transformedSceneMatrix;
}


//////////////////////////////////////////////////////////////////////////
//// Attachment support
//////////////////////////////////////////////////////////////////////////
void CAnimScene::AttachToEntity(CEntity* pEnt)
{
	animAssertf(pEnt,"Error: null pEnt passed as parent to anim scene.");
	if(pEnt)
	{
		AttachToEntitySkeleton(pEnt,-1);
	}
}

void CAnimScene::AttachToEntityPreservingLocation(CEntity* pEnt)
{
	animAssertf(pEnt,"Error: null pEnt passed as parent to anim scene.");
	if(pEnt)
	{
		AttachToSkeletonPreservingLocation(pEnt,-1);
	}
}

void CAnimScene::AttachToEntitySkeleton(CEntity* pEnt, s16 boneIdx)
{
	animAssertf(pEnt,"Error: null pEnt passed as parent to anim scene.");

	if(pEnt)
	{
		if( !animVerifyf( !IsEntityInScene(pEnt),"Error: Tried to attach entity %s that already was in scene %s.", pEnt->GetDebugName(), this->GetName().TryGetCStr() ) )
		{
			return;
		}

		if(boneIdx != -1 )
		{
			animAssertf(pEnt->GetSkeleton(), "Error: pEnt (%s) did not have skeleton for parenting to anim scene with bone %d",
				pEnt->GetDebugName(),boneIdx);
			if(pEnt->GetSkeleton() && boneIdx < pEnt->GetSkeleton()->GetBoneCount())
			{
				Data().m_attachedParentEntity = pEnt;
				Data().m_attachedBoneIndex = boneIdx;
			}
			else //just attach to entity root matrix
			{
				animAssertf(0, "Error: pEnt (%s) did not have correct bone for parenting to anim scene with bone %d",
					pEnt->GetDebugName(),boneIdx);
				Data().m_attachedParentEntity = pEnt;
				Data().m_attachedBoneIndex = -1;
			}

		}
		else
		{
			Data().m_attachedParentEntity = pEnt;
			Data().m_attachedBoneIndex = -1;
		}
	}
}

//This function computes and sets the offset for the scene matrix, preserving the spatial relationship of the scene and entity matrices in the world
void CAnimScene::AttachToSkeletonPreservingLocation(CEntity* pEnt, s16 boneIdx)
{
	animAssertf(pEnt,"Entity passed to anim scene (for skeletal parenting) does not exist.");

	if (pEnt)
	{	
		if(!animVerifyf( !IsEntityInScene(pEnt),"Error: Tried to attach entity %s that already was in scene %s.", pEnt->GetDebugName(), this->GetName().TryGetCStr() ) )
		{
			return;
		}
		
		Mat34V invertedEntityTransform, entityTransform, offset;
		if (boneIdx>-1 && pEnt->GetSkeleton())
		{
			if (boneIdx < (int)pEnt->GetSkeleton()->GetBoneCount())
			{
				pEnt->GetGlobalMtx(boneIdx, RC_MATRIX34(entityTransform));
			}
			else
			{
				animAssertf(0, "An animScene %d attempted to attach with an invalid bone index %d!", GetInstId(), boneIdx);
				boneIdx = -1;
				entityTransform = pEnt->GetMatrix();
			}
		}
		else
		{
			entityTransform = pEnt->GetMatrix();
			if(boneIdx != -1)
			{
				animAssertf(0, "Skeleton passed to anim scene (for skeletal parenting) does not exist, attempted to access bone: %d.", boneIdx);
			}
		}
		//Compute the transformation matrix from the entity to the current 
		//location of the anim scene
		InvertTransformFull(invertedEntityTransform,entityTransform);
		Transform(offset, invertedEntityTransform, GetSceneOrigin() );
		m_sceneOrigin.SetMatrix(offset);
		AttachToEntitySkeleton(pEnt,boneIdx);
	}
}

void CAnimScene::GetParentMatrix(Mat34V_InOut mat)
{
	animAssertf(Data().m_attachedParentEntity != nullptr, "null attached parent entity when attempting to get parent matrix.");
	if(!Data().m_attachedParentEntity)
	{
		mat.SetIdentity3x3();
		mat.SetM30M31M32Zero();
		return;
	}

	if (Data().m_attachedBoneIndex>-1 && Data().m_attachedParentEntity->GetSkeleton())
	{
		if (Data().m_attachedBoneIndex < (int)Data().m_attachedParentEntity->GetSkeleton()->GetBoneCount())
		{
			Data().m_attachedParentEntity->GetGlobalMtx(Data().m_attachedBoneIndex, RC_MATRIX34(mat));
		}
		else
		{
			animAssertf(0, "An animScene %d is attached with an invalid bone index %d!", GetInstId(), Data().m_attachedBoneIndex);
			mat = Data().m_attachedParentEntity->GetMatrix();
		}
	}
	else
	{
		mat = Data().m_attachedParentEntity->GetMatrix();
	}
}

void CAnimScene::DetachFromEntity()
{
	Data().m_attachedParentEntity = nullptr;
	Data().m_attachedBoneIndex = -1;
}

void CAnimScene::DetachFromEntityPreservingLocation()
{
	m_sceneOrigin.SetMatrix(GetSceneOrigin());
	DetachFromEntity();
}

// Only valid for physical objects (CPhysical)
bool CAnimScene::IsEntityInScene(CEntity* pEnt)
{
	if(pEnt->GetIsPhysical())
	{
		CPhysical* pPhysicalEnt = static_cast<CPhysical*>(pEnt);
		for(atBinaryMap<CAnimSceneEntity*, CAnimSceneEntity::Id>::Iterator it = m_entities.Begin(); it != m_entities.End(); it++)
		{
			CPhysical* pPhysicalComp = GetPhysicalEntity( (*it) );
			if(pPhysicalEnt == pPhysicalComp)
			{
				return true;
			}
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////
/// Support for validation and fixing of strings 
//////////////////////////////////////////////////////////////////////////
/*
bool CAnimScene::FixId(Id& id)
{
	const char* idStr = id.TryGetCStr();
	std::string idStrS = idStr;
	bool hasChanged = false;
	for(int i = 0; i < idStrS.length(); )
	{
		if(idStrS[i] == '\n' || idStrS[i] == '\r')
		{
			animAssertf(0,"Tried to assign id with newline characters in. Orig: %s", idStr);
			idStrS.erase(i,1);
			hasChanged = true;
		}
		else
		{
			++i;
		}
	}

	if(hasChanged)
	{
		id.SetFromString(idStrS.c_str());
	}
	
	return hasChanged;
}*/

void CAnimScene::AddEntity(Id id, CAnimSceneEntity* pEnt)
{
	//No need to fix the id on creation, just assert if we're passed an invalid id
	animAssertf(CAnimSceneEntity::IsValidId(id), "Id passed to anim scene to add entity is invalid: \"%s\"",id.TryGetCStr());
	/*
	if(FixId(id))
	{
		animAssertf(0,"Newly processed id value %s",id.TryGetCStr());
	}*/

	CAnimSceneEntity** ppEnt = m_entities.SafeGet(id);
	if (ppEnt==NULL)
	{
		m_entities.Insert(id, pEnt);
		m_entities.FinishInsertion();
	}
	else
	{
		animAssertf(0, "Failed to add new anim scene entity (id:%s, type:%s) - an entity with that id already exists!", id.GetCStr(), CAnimSceneEntity::GetEntityTypeName(pEnt->GetType()));
	}
}

bool CAnimScene::Validate(bool checkAll)
{
	//go through all events and check that their length is equal or less than the 
	//referenced animations
	CAnimSceneSectionIterator it(*this);
	unsigned int numInvalid = 0;

	while(*it)
	{
		CAnimSceneSection* section = (*it);
		if(!section)
		{
			++it;
			continue;
		}
		CAnimSceneEventList& eList = section->GetEvents();
		for(int i = 0; i < eList.GetEventCount(); ++i)
		{
			//Asserts inside GetEvent if event is null, so no check here
			CAnimSceneEvent* event =  eList.GetEvent(i);
			
			if(!event->Validate())
			{
				//invalid due to time mismatch
				if(checkAll)
				{
					++numInvalid;
					animWarningf("[Anim Scene %s] Section %s, Event %d is invalid.", this->GetName().TryGetCStr(), it.GetId().TryGetCStr(), i);
				}
				else
				{
					return false; 
				}				
			}				
		}
		
		++it;
	}

	if(numInvalid > 0)
	{
		return false;
	}
	else
	{
		return true;
	}	
}

//////////////////////////////////////////////////////////////////////////
/// Anim Scene Section Iterator
//////////////////////////////////////////////////////////////////////////

CAnimSceneSectionIterator::CAnimSceneSectionIterator(CAnimScene& animScene, bool ignorePlaybackList /*= false*/, bool overridePlaybackList/* = false*/, CAnimScenePlaybackList::Id overrideList/* = ANIM_SCENE_PLAYBACK_LIST_ID_INVALID*/) 
	: m_animScene(&animScene)
	, m_pPlaybackList(NULL)
	, m_pSection(NULL)
	, m_currentIndex(-1)
	, m_ignorePlaybackList(ignorePlaybackList)
	, m_overridePlaybackList(overridePlaybackList)
	, m_overrideList(overrideList)
	, m_currentSection(ANIM_SCENE_SECTION_ID_INVALID)
{
	begin();
}


CAnimSceneSectionIterator::~CAnimSceneSectionIterator()
{

}

void CAnimSceneSectionIterator::begin()
{
	m_currentIndex = -1;
	m_pSection = NULL;
	FindNextSection();
}

CAnimSceneSectionIterator& CAnimSceneSectionIterator::operator++() 
{ 
	FindNextSection();
	return *this; 
}

const CAnimSceneSection* CAnimSceneSectionIterator::operator*() const 
{ 
	return m_pSection; 
}

CAnimSceneSection* CAnimSceneSectionIterator::operator*() 
{ 
	return m_pSection; 
}

void CAnimSceneSectionIterator::FindNextSection()
{
	if(!m_animScene)
	{
		return;
	}


	// initialise the stored playback list if necessary
	if (!m_ignorePlaybackList && !m_pPlaybackList)
	{
		CAnimScenePlaybackList::Id listId = m_overridePlaybackList ? m_overrideList : m_animScene->Data().m_playbackList;
		if ( listId!=ANIM_SCENE_PLAYBACK_LIST_ID_INVALID)
		{
			CAnimScenePlaybackList **ppList = m_animScene->m_playbackLists.SafeGet(m_animScene->Data().m_playbackList);
			if(ppList)
			{
				m_pPlaybackList = *ppList;
			}
		}
	}

	m_currentIndex++;

	if (!m_ignorePlaybackList && m_pPlaybackList)
	{
		if (m_currentIndex<0 || m_currentIndex>=m_pPlaybackList->GetNumSections())
		{
			m_pSection = NULL;
			m_currentSection = ANIM_SCENE_SECTION_ID_INVALID;
		}
		else
		{
			m_currentSection = m_pPlaybackList->GetSectionId(m_currentIndex);
			m_pSection = m_animScene->GetSection(m_currentSection);
		}
	}
	else
	{
		m_currentSection = m_animScene->GetSectionIdByIndex(m_currentIndex);
		m_pSection = m_animScene->GetSectionByIndex(m_currentIndex);
	}
}
