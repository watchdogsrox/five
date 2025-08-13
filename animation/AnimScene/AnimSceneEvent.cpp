#include "AnimSceneEvent.h"
#include "AnimSceneEvent_parser.h"

// rage includes
#include "bank/bank.h"
#include "bank/msgbox.h"

// game includes
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CAnimSceneEvent
CAnimSceneEvent::CAnimSceneEvent()
	: m_startTime(0.0f)
	, m_endTime(0.0f)
	, m_flags(0)
	, m_preloadTime(0.0f)
	, m_pChildren(NULL)
	, m_pSection(NULL)
{

}

//////////////////////////////////////////////////////////////////////////

CAnimSceneEvent::CAnimSceneEvent(const CAnimSceneEvent& other)
	: m_endTime (other.m_endTime)
	, m_startTime (other.m_startTime)
	, m_preloadTime (other.m_preloadTime)
	, m_flags (other.m_flags)
	, m_internalFlags (other.m_internalFlags)
	, m_pChildren(NULL)
	, m_pSection(NULL)
{
	if (other.HasChildren())
	{
		for (s32 i=0; i<other.GetChildren()->GetEventCount(); i++)
		{
			if (!HasChildren())
			{
				m_pChildren = rage_new CAnimSceneEventList();
			}
			else
			{
				m_pChildren->ClearEvents();
			}

			m_pChildren->AddEvent(other.GetChildren()->GetEvent(i)->Copy());
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneEvent::~CAnimSceneEvent()
{
	if (m_pChildren)
	{
		delete m_pChildren;
		m_pChildren = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::Init(CAnimScene* pScene, CAnimSceneSection* pOwningSection)
{
	fwFlags16 childFlags = 0;
	m_pSection = pOwningSection;


	// parent gets initialised first
	OnInit(pScene);

	// init any child events
	if (HasChildren())
	{
		BANK_ONLY(m_pChildren->SetOwningScene(pScene);)
		for (s32 i=0; i<m_pChildren->GetEventCount(); i++)
		{
			animAssert(m_pChildren->GetEvent(i));
			m_pChildren->GetEvent(i)->Init(pScene, pOwningSection);
			childFlags.SetFlag(m_pChildren->GetEvent(i)->GetFlags());
		}
	}

	m_flags.SetFlag(childFlags);
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::Shutdown(CAnimScene* pScene)
{
	// shutdown any child events first
	if (HasChildren())
	{
		for (s32 i=0; i<m_pChildren->GetEventCount(); i++)
		{
			animAssert(m_pChildren->GetEvent(i));
			m_pChildren->GetEvent(i)->Shutdown(pScene);
		}
	}

	// shutdown the parent
	OnShutdown(pScene);
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::StartOfScene(CAnimScene* pScene)
{
	// shutdown any child events first
	if (HasChildren())
	{
		for (s32 i=0; i<m_pChildren->GetEventCount(); i++)
		{
			animAssert(m_pChildren->GetEvent(i));
			m_pChildren->GetEvent(i)->StartOfScene(pScene);
		}
	}

	// first check if this event has a primary entity
	if (GetPrimaryEntityId()!=ANIM_SCENE_ENTITY_ID_INVALID)
	{
		CAnimSceneEntity* pEnt = pScene->GetEntity(GetPrimaryEntityId());
		if (pEnt && pEnt->IsDisabled())
		{
			m_internalFlags.SetFlag(kDisabled);
		}
	}

	// shutdown the parent
	OnStartOfScene(pScene);
}

//////////////////////////////////////////////////////////////////////////

float CAnimSceneEvent::GetStart() const
{ 
	return m_pSection ? m_pSection->GetStart() + m_startTime : m_startTime; 
}

//////////////////////////////////////////////////////////////////////////

float CAnimSceneEvent::GetEnd() const
{ 
	return m_pSection ? m_pSection->GetStart() + m_endTime : m_endTime; 
}

//////////////////////////////////////////////////////////////////////////

#if __BANK

float CAnimSceneEvent::GetStart(CAnimScene* pScene, u32 overrideListId)
{ 
	return m_pSection ? m_pSection->GetStart(pScene, overrideListId) + m_startTime : m_startTime; 
}

//////////////////////////////////////////////////////////////////////////

float CAnimSceneEvent::GetEnd(CAnimScene* pScene, u32 overrideListId)
{ 
	return m_pSection ? m_pSection->GetStart(pScene, overrideListId) + m_endTime : m_endTime; 
}

//////////////////////////////////////////////////////////////////////////

#endif //__BANK

bool CAnimSceneEvent::OnEnterPreloadInternal(CAnimScene* pScene)
{
	bool loaded = true;
	if (!OnEnterPreload(pScene))
	{
		loaded = false;
	}
	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			animAssert(GetChildren()->GetEvent(i));
			if (!GetChildren()->GetEvent(i)->OnEnterPreloadInternal(pScene))
			{
				loaded = false;
			}
		}
	}
	if (loaded)
		m_internalFlags.SetFlag(kLoaded);
	else
		m_internalFlags.ClearFlag(kLoaded);
	return loaded;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEvent::OnUpdatePreloadInternal(CAnimScene* pScene)
{
	bool loaded = true;	

	if (!OnUpdatePreload(pScene))
	{
		loaded = false;
	}
	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			animAssert(GetChildren()->GetEvent(i));
			if (!GetChildren()->GetEvent(i)->OnUpdatePreloadInternal(pScene))
			{
				loaded = false;
			}
		}
	}
	if (loaded)
		m_internalFlags.SetFlag(kLoaded);
	else
		m_internalFlags.ClearFlag(kLoaded);
	return loaded;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEvent::OnExitPreloadInternal(CAnimScene* pScene)
{
	bool loaded = true;
	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			animAssert(GetChildren()->GetEvent(i));
			if (!GetChildren()->GetEvent(i)->OnExitPreloadInternal(pScene))
			{
				loaded = false;
			}
		}
	}
	if (!OnExitPreload(pScene))
	{
		loaded = false;
	}
	return loaded;
}

//////////////////////////////////////////////////////////////////////////


void CAnimSceneEvent::OnEnterInternal(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	OnEnter(pScene, phase);

	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			animAssert(GetChildren()->GetEvent(i));
			GetChildren()->GetEvent(i)->OnEnterInternal(pScene, phase);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::OnUpdateInternal(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	OnUpdate(pScene, phase);

	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			animAssert(GetChildren()->GetEvent(i));
			GetChildren()->GetEvent(i)->OnUpdateInternal(pScene, phase);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::OnExitInternal(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			animAssert(GetChildren()->GetEvent(i));
			GetChildren()->GetEvent(i)->OnExitInternal(pScene, phase);
		}
	}

	OnExit(pScene, phase);
}

//////////////////////////////////////////////////////////////////////////

s32 CAnimSceneEvent::CountChildren()
{
	s32 children = 0;

	if (HasChildren())
	{
		for (s32 i=0; i<GetChildren()->GetEventCount(); i++)
		{
			children++;
			children+=GetChildren()->GetEvent(i)->CountChildren();
		}
	}

	return children;
}
#if __BANK

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::SetStart(float time) 
{ 
	m_pSection ? m_startTime = Max((time - m_pSection->GetStart()), 0.0f) : m_startTime = time; 
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::SetEnd(float time) 
{ 
	m_pSection ? m_endTime = Max((time - m_pSection->GetStart()), 0.0f) : m_endTime = time; 
}

//////////////////////////////////////////////////////////////////////////

float CAnimSceneEvent::GetRoundedTime(float fTime)
{
	const float fErrorTolerance = 0.001f;
	int numFrames = (int)((fTime + fErrorTolerance) * 30.0f);

	return (float)numFrames * (1.0f/30.0f);
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::OnPostAddWidgets(bkBank& bank)
{
	if (m_pChildren)
	{
		// add the widgets for the child events
		PARSER.AddWidgets(bank, m_pChildren);
		// add a button to remove all the child events
		m_widgets->m_pChildEventButton = bank.AddButton("Delete child events", datCallback(MFA1(CAnimSceneEvent::RemoveChildEventStructure), this, (datBase*)&bank));
	}
	else
	{
		m_widgets->m_pChildEventButton = bank.AddButton("Add child events", datCallback(MFA1(CAnimSceneEvent::AddChildEventStructure), this, (datBase*)&bank));
	}
	// add the delete button to the widgets
	m_widgets->m_pParentList = CAnimScene::GetCurrentAddWidgetsEventList();
	if (m_widgets->m_pParentList)
	{
		bank.AddButton("Delete this event", datCallback(MFA1(CAnimSceneEventList::DeleteEvent), (datBase*)m_widgets->m_pParentList, this));
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::NotifyRangeUpdated()
{
	// Give derived classes a chance to override
	OnRangeUpdated();

	if (m_widgets->m_pParentList)
	{
		CAnimScene* pScene = m_widgets->m_pParentList->GetOwningScene();
		if (pScene)
		{
			pScene->RecalculateSceneDurations();
		}
	}
}
#if __DEV
//////////////////////////////////////////////////////////////////////////
void CAnimSceneEvent::DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput)
{ 
	const char * eventState = "idle";

	if (IsDisabled())
	{
		eventState = "disabled (primary entity disabled)";
	}
	else if (IsInPreload())
	{
		if (IsLoaded())
		{
			eventState = "loaded";
		}
		else
		{
			eventState = "loading";
		}
	}
	else if (IsInEvent())
	{
		eventState = "active";
	}

	atString triggerState;
	if (m_flags.IsFlagSet(kRequiresEnter))
	{
		if (m_internalFlags.IsFlagSet(kTriggerEnter))
			triggerState+=" EN";
		else
			triggerState+=" en";
	}
	if (m_flags.IsFlagSet(kRequiresUpdate))
	{
		if (m_internalFlags.IsFlagSet(kTriggerUpdate))
			triggerState+=" UP";
		else
			triggerState+=" up";
	}
	if (m_flags.IsFlagSet(kRequiresExit))
	{
		if (m_internalFlags.IsFlagSet(kTriggerExit))
			triggerState+=" EX";
		else
			triggerState+=" ex";
	}

	mainOutput.AddLine("%s %.3f-%.3f, preload %.3f, %s%s", parser_GetStructure()->GetName(), GetStart(), GetEnd(), GetPreloadDuration(), eventState, triggerState.c_str());

	if (m_pChildren)
	{
		m_pChildren->DebugRender(pScene, mainOutput);
	}
}
#endif //__DEV

//////////////////////////////////////////////////////////////////////////
void CAnimSceneEvent::SceneEditorRender(CAnimScene* pScene, CAnimSceneEditor& editor)
{ 
	editor.UpdateEventLayerInfo(this);
	if (m_pChildren)
	{
		m_pChildren->SceneEditorRender(pScene, editor);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	float time = pInitialiser->m_pEditor->GetCurrentTime();

	if (pInitialiser->m_pSection)
	{
		// set the start based on the selected time in the 
		// editor maintaining the original duration.
		SetStart(Max(time - pInitialiser->m_pSection->GetStart(), 0.0f));
		SetEnd(Max(time - pInitialiser->m_pSection->GetStart(), 0.0f));
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEvent::GetEditorEventText(atString& textToDisplay, CAnimSceneEditor& UNUSED_PARAM(editor))
{
	textToDisplay = parser_GetStructure()->GetName();
	textToDisplay.Replace("CAnimScene", "");
	textToDisplay.Replace("Event", "");
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEvent::GetEditorHoverText(atString& textToDisplay, CAnimSceneEditor& UNUSED_PARAM(editor))
{
	u32 startFrame = (u32)(GetStart() * 30.0f); 
	u32 endFrame = (u32)(GetEnd() * 30.0f); 
	textToDisplay = atVarString("%d - %d", startFrame, endFrame);
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::GetEditorColor(Color32& color, CAnimSceneEditor& editor, int colourId)
{

	if(colourId % 2 == 0)
	{
		color=editor.GetColor(CAnimSceneEditor::kDefaultEventColor);
	}
	else
	{
		color=editor.GetColor(CAnimSceneEditor::kDefaultEventColor); //no alt currently
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::AddChildEventStructure(bkBank* pBank)
{
	
	if (!m_pChildren && pBank)
	{
		bkGroup* pGroup = NULL;
		if (m_widgets->m_pChildEventButton)
		{
			pGroup = (bkGroup*)m_widgets->m_pChildEventButton->GetParent();
			m_widgets->m_pChildEventButton->Destroy();
			m_widgets->m_pChildEventButton = NULL;			
		}

		m_pChildren = rage_new CAnimSceneEventList();

		CAnimScene::SetCurrentAddWidgetsScene(m_widgets->m_pParentList->GetOwningScene());
		if(pGroup)
		{
			pBank->SetCurrentGroup(*pGroup);
		}
		
		PARSER.AddWidgets(*pBank, m_pChildren);
		m_widgets->m_pChildEventButton = pBank->AddButton("Delete child events", datCallback(MFA1(CAnimSceneEvent::RemoveChildEventStructure), this, (datBase*)pBank));
		
		if(pGroup)
		{
			pBank->UnSetCurrentGroup(*pGroup);
		}
		
		CAnimScene::SetCurrentAddWidgetsScene(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEvent::RemoveChildEventStructure(bkBank* pBank)
{
	if (m_pChildren && pBank)
	{
		bkGroup* pGroup = NULL;
		if (m_widgets->m_pChildEventButton)
		{
			pGroup = (bkGroup*)m_widgets->m_pChildEventButton->GetParent();
			m_widgets->m_pChildEventButton->Destroy();
			m_widgets->m_pChildEventButton = NULL;
		}
		
		delete m_pChildren;
		m_pChildren = NULL;

		if(pGroup)
		{
			pBank->SetCurrentGroup(*pGroup);
		}
		
		m_widgets->m_pChildEventButton = pBank->AddButton("Add child events", datCallback(MFA1(CAnimSceneEvent::AddChildEventStructure), this, (datBase*)pBank));
		
		if(pGroup)
		{
			pBank->UnSetCurrentGroup(*pGroup);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

atArray<const char *> CAnimSceneEventList::sm_EventTypeNames;
s32 CAnimSceneEventList::sm_SelectedEventType = 0;

void CAnimSceneEventList::InitEventEditing()
{
	if (sm_EventTypeNames.GetCount()==0)
	{
		const parStructure* pAnimSceneEventStruct = PARSER.FindStructure("CAnimSceneEvent");

		parManager::StructureMap::ConstIterator it = PARSER.GetStructureMap().CreateIterator();

		for( ; !it.AtEnd(); it.Next())
		{
			if (pAnimSceneEventStruct && it.GetData()->IsSubclassOf(pAnimSceneEventStruct))
			{
				if (it.GetData()!=pAnimSceneEventStruct)
				{
					sm_EventTypeNames.PushAndGrow(it.GetData()->GetName());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
 void CAnimSceneEventList::OnPreAddWidgets(bkBank& bank)
 {
	 m_widgets->m_pOwningScene = CAnimScene::GetCurrentAddWidgetsScene();
	 m_widgets->m_pBank = &bank;
	 CAnimScene::PushAddWidgetsEventList(this);
 }

void CAnimSceneEventList::OnPostAddWidgets(bkBank& bank)
{
	m_widgets->m_pGroup = bank.PushGroup("Child events");
	bank.PushGroup("Add new event");
	bank.AddCombo("Type:", &sm_SelectedEventType, sm_EventTypeNames.GetCount(), &sm_EventTypeNames[0], NullCB);
	bank.AddButton("Add", datCallback(MFA(CAnimSceneEventList::AddNewEvent), (datBase*)this));
	bank.PopGroup(); // Add new event
	for (s32 i=0; i<m_events.GetCount(); i++)
	{
		s32 numChildEvents = m_events[i]->CountChildren();
		atVarString groupName("%s", m_events[i]->parser_GetStructure()->GetName());
		if (numChildEvents)
		{
			groupName += atVarString("(%d children)", numChildEvents);
		}
		bank.PushGroup(groupName.c_str());
		PARSER.AddWidgets(bank, m_events[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
	CAnimScene::PopAddWidgetsEventList();

	if (m_widgets->m_pOwningScene)
	{
		m_widgets->m_pOwningScene->RegisterWidgetShutdown(MakeFunctor(*this, &CAnimSceneEventList::ShutdownWidgets));
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEventList::ShutdownWidgets()
{
	m_widgets->m_pBank = NULL;
	if (m_widgets->m_pGroup)
	{
		m_widgets->m_pGroup->Destroy();
		m_widgets->m_pGroup=NULL;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEventList::AddNewEvent()
{
	if (sm_SelectedEventType>=0 && sm_SelectedEventType < sm_EventTypeNames.GetCount())
	{
		const char * pType = sm_EventTypeNames[sm_SelectedEventType];

		parStructure* pStruct = PARSER.FindStructure(pType);

		if(!pStruct)
		{
			bkMessageBox("Invalid event type", "Error adding a new event! The selected event type is not recognised", bkMsgOk, bkMsgError);
			return;
		}

		CAnimSceneEvent* pNewEvt = (CAnimSceneEvent*)pStruct->Create();
		pNewEvt->PostAddedFromRag(m_widgets->m_pOwningScene);
		AddEvent(pNewEvt);

		if (m_widgets->m_pGroup)
		{
			atVarString groupName("%s",pNewEvt->parser_GetStructure()->GetName());
			bkGroup* pEventGroup = m_widgets->m_pGroup->AddGroup(groupName.c_str());
			CAnimScene::SetCurrentAddWidgetsScene(m_widgets->m_pOwningScene);
			CAnimScene::PushAddWidgetsEventList(this);
			m_widgets->m_pBank->SetCurrentGroup(*pEventGroup);
			PARSER.AddWidgets(*m_widgets->m_pBank, pNewEvt);
			m_widgets->m_pBank->UnSetCurrentGroup(*pEventGroup);
			CAnimScene::SetCurrentAddWidgetsScene(NULL);
			CAnimScene::PopAddWidgetsEventList();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEventList::DeleteEvent(CAnimSceneEvent* pEvt)
{
	s32 index = m_events.Find(pEvt);

	if (index>=0 && index<GetEventCount())
	{
		m_events.DeleteMatches(pEvt);
		pEvt->Shutdown(m_widgets->m_pOwningScene);
		delete pEvt;

		if (m_widgets->m_pGroup)
		{
			// remove the widgets too
			bkWidget* pEventGroup = m_widgets->m_pGroup->GetChild();

			// skip the add event group
			if (pEventGroup)
				pEventGroup= pEventGroup->GetNext();

			while (index>0 && pEventGroup)
			{
				pEventGroup= pEventGroup->GetNext();
				index--;
			}

			if (pEventGroup && index==0)
			{
				pEventGroup->Destroy();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEventList::DeleteAllEventsForEntity(CAnimSceneEntity::Id entityId)
{
	atArray<CAnimSceneEvent*> eventsToDelete;
	for (s32 i=0; i<m_events.GetCount(); i++) 
	{
		if (m_events[i]->GetPrimaryEntityId()==entityId)
		{
			eventsToDelete.PushAndGrow(m_events[i]);
		}
	}

	for (s32 i=0; i<eventsToDelete.GetCount(); i++)
	{
		DeleteEvent(eventsToDelete[i]);
	}
}


#endif //__BANK

//////////////////////////////////////////////////////////////////////////

CAnimSceneEventList::CAnimSceneEventList()
{

}

//////////////////////////////////////////////////////////////////////////

CAnimSceneEventList::~CAnimSceneEventList()
{
	ClearEvents();
#if __BANK
	ShutdownWidgets();
#endif //__BANK
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEventList::ClearEvents(){ 
	for (s32 i=0; i<m_events.GetCount(); i++) 
	{
		delete m_events[i];
		m_events[i]=NULL;
	}
	m_events.clear();
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
#if __DEV
void CAnimSceneEventList::DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput)
{
	if (mainOutput.AddLine("----%d Events---", GetEventCount()) && CAnimSceneManager::IsMouseDown())
	{
		m_widgets->m_debugSelected = !m_widgets->m_debugSelected;
	}

	if (m_widgets->m_debugSelected)
	{
		mainOutput.PushIndent();
		{
			for (s32 i=0; i<GetEventCount(); i++)
			{
				CAnimSceneEvent* pEvt = GetEvent(i);
				pEvt->DebugRender(pScene, mainOutput);
			}	
		}
		mainOutput.PopIndent();
	}
}
#endif //__DEV

void CAnimSceneEventList::SceneEditorRender(CAnimScene* pScene, CAnimSceneEditor& editor)
{
	for (s32 i=0; i<GetEventCount(); i++)
	{
		CAnimSceneEvent* pEvt = GetEvent(i);
		if (pEvt->GetParentList()==NULL)
		{
			pEvt->SetParentList(this);
		}
		pEvt->SceneEditorRender(pScene, editor);
	}
}

#endif //__BANK