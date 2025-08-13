/////////////////////////////////////////////////////////////////////////////////
// FILE :		CScenarioAnimDebugHelper.cpp
// PURPOSE :	Helper used for debugging scenario animations
// AUTHOR :		Jason Jurecka.
// CREATED :	5/1/2012
/////////////////////////////////////////////////////////////////////////////////

#include "task/Scenario/ScenarioAnimDebugHelper.h"
#if SCENARIO_DEBUG
#include "task/Scenario/ScenarioAnimDebugHelper_parser.h"
#endif // SCENARIO_DEBUG
///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage headers
#include "cranimation/vcrwidget.h"
#include "parser/visitorxml.h"

// framework headers
#include "ai/aichannel.h"

// game includes
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/ambient/ConditionalAnimManager.h"
#include "debug/DebugScene.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Default/AmbientAnimationManager.h"
#include "task/General/TaskBasic.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/TaskScenarioAnimDebug.h"

AI_OPTIMISATIONS()


#if SCENARIO_DEBUG

///////////////////////////////////////////////////////////////////////////////
//  STATIC CODE
///////////////////////////////////////////////////////////////////////////////
fwMvClipSetId NA_ClipSet("N/A",0xFA8F1AB1);
fwMvClipId NA_Clip("N/A",0xFA8F1AB1);
atHashString NA_Prop("N/A",0xFA8F1AB1);
atHashString NA_ScenarioType("N/A",0xFA8F1AB1);
static char s_ScenarioQueueFileName[RAGE_MAX_PATH];

int SortAlphabeticalNoCase(const char* const* pp_A,const char* const* pp_B)
{
	return stricmp(*pp_A,*pp_B);
}

///////////////////////////////////////////////////////////////////////////////
//  CODE CScenarioAnimDebugHelper
///////////////////////////////////////////////////////////////////////////////
CScenarioAnimDebugHelper::CScenarioAnimDebugHelper()
: m_Parent(NULL)
, m_FConGroupCombo(NULL)
, m_FClipSetCombo(NULL)
, m_FClipCombo(NULL)
, m_FPropCombo(NULL)
, m_UFClipSetCombo(NULL)
, m_UFClipCombo(NULL)
, m_CConClipsCombo(NULL)
, m_CClipSetCombo(NULL)
, m_CClipCombo(NULL)
, m_CPropCombo(NULL)
, m_QueueGroup(NULL)
, m_SelFConGroup(0)
, m_SelFType(0)
, m_SelFClipSet(0)
, m_SelFClip(0)
, m_SelFProp(0)
, m_SelUFType(0)
, m_SelUFClipSet(0)
, m_SelUFClip(0)
, m_SelCType(0)
, m_SelCConGroup(0)
, m_SelCConClips(0)
, m_SelCClipSet(0)
, m_SelCClip(0)
, m_SelCProp(0)
, m_CurrentQueueItem(0)
, m_PlayQueue(false)
, m_LoopQueue(false)
, m_ToolEnabled(false)
, m_HideUI(false)
, m_UseEnterExitOff(false)
, m_EnterExitOff(V_ZERO)
{
	m_SelectedEntityName[0] = '\0';
	m_FPropSetName[0] = '\0';
	m_CPropSetName[0] = '\0';
}

void CScenarioAnimDebugHelper::Init(bkGroup* parent)
{
	ResetSelected();
	UpdateTypeNames();
	m_Parent = parent;
	AddWidgets();
 }

void CScenarioAnimDebugHelper::Update(CScenarioPoint* selectedPoint, CEntity* selectedEntity)
{
	if (!m_ToolEnabled)
	{
		OnStopQueuePressed();//stop anything that is playing
		ClearEntity();
		return;
	}

	if (selectedPoint && selectedPoint != m_ScenarioPoint)
	{
		OnStopQueuePressed();//stop anything that is playing
		SwitchPoint(*selectedPoint);
	}

	if (selectedEntity && selectedEntity != m_SelectedEntity)
	{
		OnStopQueuePressed();//stop anything that is playing
		SwitchEntity(*selectedEntity);
	}

	bool updateUI = false;
	for (int item = 0; item < m_AnimQueue.GetCount(); item++)
	{
		if (m_AnimQueue[item].m_Invalid)
		{
			m_AnimQueue.Delete(item);
			item--;
			updateUI = true;
		}
	}

	if (updateUI)
	{
		UpdateQueueUI();
	}
	
	if (!m_AnimQueue.GetCount())
	{
		OnStopQueuePressed();//queue should not be running anymore ... 
	}

	//update the selected entity name
	formatf(m_SelectedEntityName, "None");
	if (m_SelectedEntity)
	{
		formatf(m_SelectedEntityName, "%s", m_SelectedEntity->GetModelName());
	}
}

void CScenarioAnimDebugHelper::Render()
{
	//if we are hiding the ui we need to disable point drawing as well ...
	CScenarioDebug::ms_bAnimDebugDisableSPRender = m_HideUI;

	if (!m_ToolEnabled || m_HideUI)
	{
		return;
	}

	//Draw the selected point ... 
	if (m_ScenarioPoint)
	{
		const Vec3V vPos = m_ScenarioPoint->GetPosition();
		const Vec3V vPos2 = vPos + Vec3V(0.0f,0.0f,CScenarioDebug::ms_fPointDrawRadius*2.0f);
		const Vec3V vPos3 = vPos + Vec3V(0.0f,0.0f,CScenarioDebug::ms_fPointDrawRadius*1.0f);
		const Vec3V vHandleOffset = m_ScenarioPoint->GetDirection() * ScalarV(CScenarioDebug::ms_fPointHandleLength*1.5f);

		Color32 color = Color_yellow;
		color.SetAlpha(160);
		grcDebugDraw::Cylinder(vPos, vPos2, CScenarioDebug::ms_fPointDrawRadius*4.0f, color, false, false);
		grcDebugDraw::Line(vPos3, vPos3 + vHandleOffset, color);

		if (m_UseEnterExitOff)
		{
			Color32 color = Color_blue;
			color.SetAlpha(160);
			grcDebugDraw::Sphere(vPos+m_EnterExitOff, CScenarioDebug::ms_fPointDrawRadius,color);
			grcDebugDraw::Line(vPos, vPos+m_EnterExitOff, Color_PaleGreen);
		}
	}

	//Draw the selected ped ...
	if (m_SelectedEntity)
	{
		const Vec3V vPos = m_SelectedEntity->GetTransform().GetPosition();
		const Vec3V vPos2 = vPos + Vec3V(0.0f,0.0f,CScenarioDebug::ms_fPointDrawRadius*2.0f);
		Color32 color = Color_OrangeRed;
		color.SetAlpha(160);
		grcDebugDraw::Cylinder(vPos, vPos2, CScenarioDebug::ms_fPointDrawRadius*6.0f, color, false, false);
	}
}

void CScenarioAnimDebugHelper::RebuildUI()
{
	AddWidgets();
}

CScenarioPoint* CScenarioAnimDebugHelper::GetScenarioPoint()
{
	Assert(m_ScenarioPoint);
	return m_ScenarioPoint.Get();
}

bool CScenarioAnimDebugHelper::WantsToPlay()
{
	if (!m_AnimQueue.GetCount())
		m_PlayQueue = false;
	
	if (m_CurrentQueueItem < 0 || m_CurrentQueueItem >= m_AnimQueue.GetCount())
		m_PlayQueue = false;
	
	if (!m_PlayQueue)
		return false;

	return true;
}

void CScenarioAnimDebugHelper::GoToQueueStart()
{
	m_CurrentQueueItem = 0;
}

bool CScenarioAnimDebugHelper::GoToQueueNext(bool ignoreLoop)
{
	bool retval = false;
	m_CurrentQueueItem++;
	if (m_CurrentQueueItem < m_AnimQueue.GetCount()) //can we play the next in queue
	{
		retval = true;
	}
	else if (m_LoopQueue && !ignoreLoop) //does the queue loop?
	{
		m_CurrentQueueItem = 0;
		retval = true;
	}

	//If there are no more things in the queue then we are done playing
	if (!m_AnimQueue.GetCount())
	{
		retval = false;
	}
	return retval;
}

void CScenarioAnimDebugHelper::GoToQueuePrev(bool ignoreLoop)
{
	m_CurrentQueueItem--;
	if (m_LoopQueue && !ignoreLoop && m_CurrentQueueItem < 0) //does the queue loop?
	{
		m_CurrentQueueItem = m_AnimQueue.GetCount()-1; //jump to end ... 
	}
	else if (m_CurrentQueueItem < 0) //dont allow us to go back beyond the first item... 
	{
		m_CurrentQueueItem = 0;
	}
}

ScenarioAnimDebugInfo CScenarioAnimDebugHelper::GetCurrentInfo() const
{
	Assert(m_CurrentQueueItem < m_AnimQueue.GetCount());
	return m_AnimQueue[m_CurrentQueueItem].m_Info;
}

void CScenarioAnimDebugHelper::OnStopQueuePressed()
{
	m_PlayQueue = false;
}

void CScenarioAnimDebugHelper::OnSelScenarioTypeChange()
{
	if (!m_FConGroupCombo)
		return;

	m_SelFConGroup = 0;
	UpdateFConGroupNames();
	m_FConGroupCombo->UpdateCombo("Condition Group", &m_SelFConGroup, m_FConGroupNames.GetCount(), m_FConGroupNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelFConGroupChange), this));

	OnSelFConGroupChange(); //Call this here to trigger updates
}

void CScenarioAnimDebugHelper::OnSelFConGroupChange()
{
	Assert(m_SelScenarioType >= 0);
	Assert(m_SelScenarioType < m_ScenatioTypeNames.GetCount());

	//reset the selected type
	m_SelFType = 0;
	OnSelFTypeChange(); //Call this here to trigger updates

	if (m_FPropCombo)
	{
		m_SelFProp = 0;
		UpdateFPropNames();
		m_FPropCombo->UpdateCombo("Prop", &m_SelFProp, m_FPropNames.GetCount(), m_FPropNames.GetElements(), datCallback::NULL_CALLBACK);
	}

	//Fill the idle time default
	m_BasePlayTime = 1.0f;
	atHashString scenarioName(m_ScenatioTypeNames[m_SelScenarioType]);
	if (scenarioName != NA_ScenarioType)
	{
		const CScenarioInfo* sceninfo = SCENARIOINFOMGR.GetScenarioInfo(scenarioName);
		if (aiVerifyf(sceninfo,"Did not find expected scenario info for %s", m_ScenatioTypeNames[m_SelScenarioType]))
		{
			const CConditionalAnimsGroup* congroup = sceninfo->GetClipData();
			if (aiVerify(congroup))
			{
				const u32 count = congroup->GetNumAnims();
				for (u32 con = 0; con < count; con++)
				{
					const CConditionalAnims * conanims = congroup->GetAnims(con);
					if (aiVerify(conanims))
					{
						if (!stricmp(conanims->GetName(), m_FConGroupNames[m_SelFConGroup]))
						{
							m_BasePlayTime = conanims->GetNextIdleTime();
							break;
						}
					}
				}
			}
		}
	}
}

void CScenarioAnimDebugHelper::OnSelCConGroupChange()
{
	if (!m_CConClipsCombo)
		return;

	m_SelCConClips = 0;
	UpdateCConClipsNames();
	m_CConClipsCombo->UpdateCombo("Condition Clips", &m_SelCConClips, m_CConClipsNames.GetCount(), m_CConClipsNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCConClipsChange), this));

	OnSelCConClipsChange(); //Call this here to trigger updates
}

void CScenarioAnimDebugHelper::OnSelCConClipsChange()
{
	if (m_CClipSetCombo)
	{
		m_SelCClipSet = 0;
		UpdateCClipSetNames();
		m_CClipSetCombo->UpdateCombo("ClipSet", &m_SelCClipSet, m_CClipsetNames.GetCount(), m_CClipsetNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCClipSetChange), this));

		OnSelCClipSetChange(); //Call this here to trigger updates
	}

	if (m_CPropCombo)
	{
		m_SelCProp = 0;
		UpdateCPropNames();
		m_CPropCombo->UpdateCombo("Prop", &m_SelCProp, m_CPropNames.GetCount(), m_CPropNames.GetElements(), datCallback::NULL_CALLBACK);
	}
}

void CScenarioAnimDebugHelper::OnSelFTypeChange()
{
	if (!m_FClipSetCombo)
		return;

	m_SelFClipSet = 0;
	UpdateFClipSetNames();
	m_FClipSetCombo->UpdateCombo("ClipSet", &m_SelFClipSet, m_FClipsetNames.GetCount(), m_FClipsetNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelFClipSetChange), this));
	
	OnSelFClipSetChange(); //Call this here to trigger updates
}

void CScenarioAnimDebugHelper::OnSelCTypeChange()
{
	if (!m_CClipSetCombo)
		return;

	m_SelCClipSet = 0;
	UpdateCClipSetNames();
	m_CClipSetCombo->UpdateCombo("ClipSet", &m_SelCClipSet, m_CClipsetNames.GetCount(), m_CClipsetNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCClipSetChange), this));

	OnSelCClipSetChange(); //Call this here to trigger updates
}

void CScenarioAnimDebugHelper::OnSelFClipSetChange()
{
	if (!m_FClipCombo)
		return;
	
	m_SelFClip = 0;
	UpdateFClipNames();
	m_FClipCombo->UpdateCombo("Clip", &m_SelFClip, m_FClipNames.GetCount(), m_FClipNames.GetElements(), datCallback::NULL_CALLBACK);
}

void CScenarioAnimDebugHelper::OnSelUFClipSetChange()
{
	if (!m_UFClipCombo)
		return;

	m_SelUFClip = 0;
	UpdateUFClipNames();
	m_UFClipCombo->UpdateCombo("Clip", &m_SelUFClip, m_UFClipNames.GetCount(), m_UFClipNames.GetElements(), datCallback::NULL_CALLBACK);
}

void CScenarioAnimDebugHelper::OnSelCClipSetChange()
{
	if (!m_CClipCombo)
		return;

	m_SelCClip = 0;
	UpdateCClipNames();
	m_CClipCombo->UpdateCombo("Clip", &m_SelCClip, m_CClipNames.GetCount(), m_CClipNames.GetElements(), datCallback::NULL_CALLBACK);
}

void CScenarioAnimDebugHelper::OnQueueFSelPressed()
{
	Assert(m_SelFClipSet >= 0);
	Assert(m_SelFClipSet < m_FClipsetNames.GetCount());

	Assert(m_SelFClip >= 0);
	Assert(m_SelFClip < m_FClipNames.GetCount());

	Assert(m_SelFConGroup >= 0);
	Assert(m_SelFConGroup < m_FConGroupNames.GetCount());

	Assert(m_SelFProp >= 0);
	Assert(m_SelFProp < m_FPropNames.GetCount());

	fwMvClipSetId clipSetId(m_FClipsetNames[m_SelFClipSet]);
	fwMvClipId clipId(m_FClipNames[m_SelFClip]);
	atHashString conGroupName = atHashString(m_FConGroupNames[m_SelFConGroup]);
	atHashString propName = atHashString(m_FPropNames[m_SelFProp]);

	if (clipSetId != NA_ClipSet && clipId != NA_Clip)
	{
		QueueClip(clipSetId, clipId, conGroupName, m_SelFType, propName);
	}
}

void CScenarioAnimDebugHelper::OnQueueUFSelPressed()
{
	Assert(m_SelUFClipSet >= 0);
	Assert(m_SelUFClipSet < m_UFClipsetNames.GetCount());

	Assert(m_SelUFClip >= 0);
	Assert(m_SelUFClip < m_UFClipNames.GetCount());

	fwMvClipSetId clipSetId(m_UFClipsetNames[m_SelUFClipSet]);
	fwMvClipId clipId(m_UFClipNames[m_SelUFClip]);

	if (clipSetId != NA_ClipSet && clipId != NA_Clip)
	{
		QueueClip(clipSetId, clipId, NA_ScenarioType, m_SelUFType, NA_Prop);
	}
}

void CScenarioAnimDebugHelper::OnQueueCSelPressed()
{
	Assert(m_SelCClipSet >= 0);
	Assert(m_SelCClipSet < m_CClipsetNames.GetCount());

	Assert(m_SelCClip >= 0);
	Assert(m_SelCClip < m_CClipNames.GetCount());

	Assert(m_SelCConGroup >= 0);
	Assert(m_SelCConGroup < m_CConGroupNames.GetCount());

	Assert(m_SelCProp >= 0);
	Assert(m_SelCProp < m_CPropNames.GetCount());

	fwMvClipSetId clipSetId(m_CClipsetNames[m_SelCClipSet]);
	fwMvClipId clipId(m_CClipNames[m_SelCClip]);
	atHashString conGroupName = atHashString(m_CConGroupNames[m_SelCConGroup]);
	atHashString propName = atHashString(m_CPropNames[m_SelCProp]);

	if (clipSetId != NA_ClipSet && clipId != NA_Clip)
	{
		QueueClip(clipSetId, clipId, conGroupName, m_SelCType, propName);
	}
}

void CScenarioAnimDebugHelper::OnClearQueuePressed()
{
	for (int i = 0; i < m_AnimQueue.GetCount(); i++)
	{
		m_AnimQueue[i].OnRemovePressed();
	}
}

void CScenarioAnimDebugHelper::OnPlayQueuePressed()
{
	if (m_SelectedEntity && m_ScenarioPoint && m_AnimQueue.GetCount())
	{
		GoToQueueStart();
		m_PlayQueue = true;
	}
	else
	{
		m_PlayQueue = false;
	}
}

void CScenarioAnimDebugHelper::OnLoadQueuePressed()
{
	if (WantsToPlay())
	{
		Errorf("Cant perform Queue load while queue is running");
		return;
	}

	if ( !BANKMGR.OpenFile( s_ScenarioQueueFileName, RAGE_MAX_PATH, "*.scque", false, "Scenario Queue (*.scque)") )
	{
		s_ScenarioQueueFileName[0] = '\0';
		return;
	}

	OnClearQueuePressed();
	EnqueueFromFile(s_ScenarioQueueFileName);
}

void CScenarioAnimDebugHelper::OnAppendQueuePressed()
{
	if (WantsToPlay())
	{
		Errorf("Cant perform Queue append while queue is running");
		return;
	}

	if ( !BANKMGR.OpenFile( s_ScenarioQueueFileName, RAGE_MAX_PATH, "*.scque", false, "Scenario Queue (*.scque)") )
	{
		s_ScenarioQueueFileName[0] = '\0';
		return;
	}
	EnqueueFromFile(s_ScenarioQueueFileName);
}

void CScenarioAnimDebugHelper::OnSaveQueuePressed()
{
	if ( !BANKMGR.OpenFile( s_ScenarioQueueFileName, RAGE_MAX_PATH, "*.scque", true, "Scenario Queue (*.scque)") )
	{
		s_ScenarioQueueFileName[0] = '\0';
		return;
	}

	fiStream* pFile = ASSET.Create(s_ScenarioQueueFileName, "");
	if(Verifyf(pFile, "Unable to save scenario queue file '%s'.", s_ScenarioQueueFileName))
	{
		//Create the save structure ... 
		ScenarioQueueLoadSaveObject saveobj;
		const int count = m_AnimQueue.GetCount();
		saveobj.m_QueueItems.Reserve(count);

		for (int i = 0; i < count; i++)
		{
			if (!m_AnimQueue[i].m_Invalid)
			{
				saveobj.m_QueueItems.Append() = m_AnimQueue[i].m_Info;
			}
		}

		// Save using parXmlWriterVisitor rather than PARSER.SaveObject(), because
		// the latter uses a lot of memory to store the whole parser tree before saving,
		// which could make us run out of memory and lose data.
		parXmlWriterVisitor v1(pFile);
		v1.Visit(saveobj);
		pFile->Close();
		pFile = NULL;
		Displayf("Saved scenario queue to '%s'.", s_ScenarioQueueFileName);
	}
}

void CScenarioAnimDebugHelper::AddWidgets()
{   
	aiAssert(m_Parent);
	
	while(m_Parent->GetChild())
		m_Parent->Remove(*m_Parent->GetChild());

	ResetSelected();
	OnClearQueuePressed();//clear out the queue cause something changed that calls for a reset.
	UpdateScenarioTypeNames();
	UpdateUFClipSetNames();
	UpdateCConGroupNames();

	m_Parent->AddToggle("Debugging Enabled", &m_ToolEnabled);
	m_Parent->AddToggle("Hide UI", &m_HideUI);
	m_Parent->AddToggle("Use Enter/Exit Offset", &m_UseEnterExitOff);
	m_Parent->AddVector("Enter/Exit Offset", &m_EnterExitOff, -10.0f, 10.0f, 0.1f);
	m_Parent->AddText("Selected Entity", m_SelectedEntityName, 64, true);
	
	//Filtered Clip info
	bkGroup* Fgroup = m_Parent->AddGroup("Filtered Clip Data");
		Fgroup->AddCombo("Scenario Type", &m_SelScenarioType, m_ScenatioTypeNames.GetCount(), m_ScenatioTypeNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelScenarioTypeChange), this));
		Fgroup->AddCombo("Anim Type", &m_SelFType, m_AnimTypeNames.GetCount(), m_AnimTypeNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelFTypeChange), this));
		m_FConGroupCombo = Fgroup->AddCombo("Condition Group", &m_SelFConGroup, m_FConGroupNames.GetCount(), m_FConGroupNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelFConGroupChange), this));
		m_FClipSetCombo = Fgroup->AddCombo("ClipSet", &m_SelFClipSet, m_FClipsetNames.GetCount(), m_FClipsetNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelFClipSetChange), this));
		m_FClipCombo = Fgroup->AddCombo("Clip", &m_SelFClip, m_FClipNames.GetCount(), m_FClipNames.GetElements(), datCallback::NULL_CALLBACK);
		Fgroup->AddText("Prop Set", m_FPropSetName, 64, true);
		m_FPropCombo = Fgroup->AddCombo("Prop", &m_SelFProp, m_FPropNames.GetCount(), m_FPropNames.GetElements(), datCallback::NULL_CALLBACK);
		Fgroup->AddButton("Queue Clip", datCallback(MFA(CScenarioAnimDebugHelper::OnQueueFSelPressed), this));

	//Unfiltered Clip info
	bkGroup* UFgroup = m_Parent->AddGroup("Unfiltered Clip Data");
		UFgroup->AddCombo("Anim Type", &m_SelUFType, m_AnimTypeNames.GetCount(), m_AnimTypeNames.GetElements(), datCallback::NULL_CALLBACK);
		m_UFClipSetCombo = UFgroup->AddCombo("ClipSet", &m_SelUFClipSet, m_UFClipsetNames.GetCount(), m_UFClipsetNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelUFClipSetChange), this));
		m_UFClipCombo = UFgroup->AddCombo("Clip", &m_SelUFClip, m_UFClipNames.GetCount(), m_UFClipNames.GetElements(), datCallback::NULL_CALLBACK);
		UFgroup->AddButton("Queue Clip", datCallback(MFA(CScenarioAnimDebugHelper::OnQueueUFSelPressed), this));

	//Conditional Clip info
	bkGroup* Cgroup = m_Parent->AddGroup("Conditional Clip Data");
		Cgroup->AddCombo("Condition Group", &m_SelCConGroup, m_CConGroupNames.GetCount(), m_CConGroupNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCConGroupChange), this));
		Cgroup->AddCombo("Anim Type", &m_SelCType, m_AnimTypeNames.GetCount(), m_AnimTypeNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCTypeChange), this));
		m_CConClipsCombo = Cgroup->AddCombo("Condition Clips", &m_SelCConClips, m_CConClipsNames.GetCount(), m_CConClipsNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCConClipsChange), this));
		m_CClipSetCombo = Cgroup->AddCombo("ClipSet", &m_SelCClipSet, m_CClipsetNames.GetCount(), m_CClipsetNames.GetElements(), datCallback(MFA(CScenarioAnimDebugHelper::OnSelCClipSetChange), this));
		m_CClipCombo = Cgroup->AddCombo("Clip", &m_SelCClip, m_CClipNames.GetCount(), m_CClipNames.GetElements(), datCallback::NULL_CALLBACK);
		Cgroup->AddText("Prop Set", m_CPropSetName, 64, true);
		m_CPropCombo = Cgroup->AddCombo("Prop", &m_SelCProp, m_CPropNames.GetCount(), m_CPropNames.GetElements(), datCallback::NULL_CALLBACK);
		Cgroup->AddButton("Queue Clip", datCallback(MFA(CScenarioAnimDebugHelper::OnQueueCSelPressed), this));

	m_Parent->AddToggle("Loop Queue", &m_LoopQueue);
	m_Parent->AddButton("Play Queue", datCallback(MFA(CScenarioAnimDebugHelper::OnPlayQueuePressed), this));
	m_Parent->AddButton("Stop Queue", datCallback(MFA(CScenarioAnimDebugHelper::OnStopQueuePressed), this));
	m_Parent->AddButton("Clear Queue", datCallback(MFA(CScenarioAnimDebugHelper::OnClearQueuePressed), this));
	
	bkGroup* TQgroup = m_Parent->AddGroup("Saved Queues",false);
		TQgroup->AddButton("Load Queue", datCallback(MFA(CScenarioAnimDebugHelper::OnLoadQueuePressed), this));
		TQgroup->AddButton("Append Queue", datCallback(MFA(CScenarioAnimDebugHelper::OnAppendQueuePressed), this));
		TQgroup->AddButton("Save Queue", datCallback(MFA(CScenarioAnimDebugHelper::OnSaveQueuePressed), this));

	m_QueueGroup = m_Parent->AddGroup("Current Queue");

	//call this here so we trigger updates
	OnSelScenarioTypeChange();
	OnSelUFClipSetChange();
	OnSelCConGroupChange();
}

void CScenarioAnimDebugHelper::ResetSelected()
{
	m_SelScenarioType = 0;
	m_SelFType = 0;
	m_SelFConGroup = 0;
	m_SelFClipSet = 0;
	m_SelFClip = 0;
	m_SelFProp = 0;
	m_SelUFType = 0;
	m_SelUFClipSet = 0;
	m_SelUFClip = 0;
	m_SelCType = 0;
	m_SelCConGroup = 0;
	m_SelCConClips = 0;
	m_SelCClipSet = 0;
	m_SelCClip = 0;
	m_SelCProp = 0;
}

void CScenarioAnimDebugHelper::UpdateScenarioTypeNames()
{
	m_ScenatioTypeNames.Reset();

	if (m_ScenarioPoint)
	{
		const int virtualOrRealIndex = m_ScenarioPoint->GetScenarioTypeVirtualOrReal();
		const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(virtualOrRealIndex);
		m_ScenatioTypeNames.Reserve(numRealTypes);

		for (int r = 0; r < numRealTypes; r++)
		{
			int realType = SCENARIOINFOMGR.GetRealScenarioType(virtualOrRealIndex, r);
			const CScenarioInfo* sceninfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(realType);
			if (aiVerifyf(sceninfo,"Did not find expected scenario info for index %d", m_ScenarioPoint->GetScenarioTypeVirtualOrReal()))
			{
				m_ScenatioTypeNames.Append() = sceninfo->GetName();
			}
		}
	}

	ValidateNameFill(m_ScenatioTypeNames);
}

void CScenarioAnimDebugHelper::UpdateFConGroupNames()
{
	m_FConGroupNames.Reset();

	Assert(m_SelScenarioType >= 0);
	Assert(m_SelScenarioType < m_ScenatioTypeNames.GetCount());

	//Fill the clipset names
	atHashString scenarioName(m_ScenatioTypeNames[m_SelScenarioType]);
	if (scenarioName != NA_ScenarioType)
	{
		const CScenarioInfo* sceninfo = SCENARIOINFOMGR.GetScenarioInfo(scenarioName);
		if (aiVerifyf(sceninfo,"Did not find expected scenario info for %s", m_ScenatioTypeNames[m_SelScenarioType]))
		{
			const CConditionalAnimsGroup* congroup = sceninfo->GetClipData();
			if (aiVerify(congroup))
			{
				const u32 count = congroup->GetNumAnims();
				m_FConGroupNames.Reserve(count);
				for (u32 con = 0; con < count; con++)
				{
					const CConditionalAnims * conanims = congroup->GetAnims(con);
					if (aiVerify(conanims))
					{
						m_FConGroupNames.Append() = conanims->GetName();
					}
				}
			}
		}
	}

	ValidateNameFill(m_FConGroupNames);
}

void CScenarioAnimDebugHelper::UpdateCConGroupNames()
{
	m_CConGroupNames.Reset();

	const int congroups = CONDITIONALANIMSMGR.GetNumConditionalAnimsGroups();
	if (congroups)
	{
		m_CConGroupNames.Reserve(congroups);
		for (int con = 0; con < congroups; con++)
		{
			const CConditionalAnimsGroup& congrp = CONDITIONALANIMSMGR.GetConditionalAnimsGroupByIndex(con);
			m_CConGroupNames.Append() = congrp.GetName();
		}
	}

	ValidateNameFill(m_CConGroupNames);
}

void CScenarioAnimDebugHelper::UpdateCConClipsNames()
{
	m_CConClipsNames.Reset();

	Assert(m_SelCConGroup >= 0);
	Assert(m_SelCConGroup < m_CConGroupNames.GetCount());

	const CConditionalAnimsGroup* congrp = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_CConGroupNames[m_SelCConGroup]);
	if (congrp)
	{
		const u32 clipsnum = congrp->GetNumAnims();
		m_CConClipsNames.Reserve(clipsnum);
		for (u32 con = 0; con < clipsnum; con++)
		{			
			const CConditionalAnims* conclips = congrp->GetAnims(con);
			if (conclips)
			{
				m_CConClipsNames.Append() = conclips->GetName();
			}
		}
	}

	ValidateNameFill(m_CConClipsNames);
}

void CScenarioAnimDebugHelper::UpdateTypeNames()
{
	m_AnimTypeNames.Reset();
	m_AnimTypeNames.Reserve(CConditionalAnims::AT_COUNT);
	for(u32 type = CConditionalAnims::AT_BASE; type < CConditionalAnims::AT_COUNT; type++)
	{
		const char* str = CConditionalAnims::GetTypeAsStr((CConditionalAnims::eAnimType)type);
		if (str)
		{
			m_AnimTypeNames.Append() = str;
		}
		else
		{
			m_AnimTypeNames.Append() = "Invalid";
		}
	}
	
	//NOTE: do not alpha sort these as the enum order is what is used when adding to the queue.
	//	CScenarioAnimDebugHelper::OnQueueSelPressed

}

void CScenarioAnimDebugHelper::UpdateFClipSetNames()
{
	m_FClipsetNames.Reset();

	Assert(m_SelScenarioType >= 0);
	Assert(m_SelScenarioType < m_ScenatioTypeNames.GetCount());

	Assert(m_SelFConGroup >= 0);
	Assert(m_SelFConGroup < m_FConGroupNames.GetCount());

	Assert(m_SelFType >= 0);
	Assert(m_SelFType < m_AnimTypeNames.GetCount());

	//Fill the clipset names
	atHashString scenarioName(m_ScenatioTypeNames[m_SelScenarioType]);
	if (scenarioName != NA_ScenarioType)
	{
		const CScenarioInfo* sceninfo = SCENARIOINFOMGR.GetScenarioInfo(scenarioName);
		if (aiVerifyf(sceninfo,"Did not find expected scenario info for %s", m_ScenatioTypeNames[m_SelScenarioType]))
		{
			const CConditionalAnimsGroup* congroup = sceninfo->GetClipData();
			if (aiVerify(congroup))
			{
				const CConditionalAnims * foundAnims = NULL;

				const u32 count = congroup->GetNumAnims();
				for (u32 con = 0; con < count; con++)
				{
					const CConditionalAnims * conanims = congroup->GetAnims(con);
					if (aiVerify(conanims))
					{
						if (!stricmp(conanims->GetName(), m_FConGroupNames[m_SelFConGroup]))
						{
							foundAnims = conanims;
							break;
						}
					}
				}

				if(foundAnims)
				{
					const CConditionalAnims::ConditionalClipSetArray* clipsets = foundAnims->GetClipSetArray((CConditionalAnims::eAnimType)m_SelFType);
					if (clipsets)
					{
						const int clipsetcount = clipsets->GetCount();
						m_FClipsetNames.Reserve(clipsetcount);
						for (int clipset = 0; clipset < clipsetcount; clipset++)
						{
							const CConditionalClipSet* conclipset = clipsets->GetElements()[clipset];
							if (aiVerify(conclipset))
							{
								m_FClipsetNames.Append() = conclipset->GetClipSetName();
							}
						}
					}
				}
			}
		}
	}

	ValidateNameFill(m_FClipsetNames);
}

void CScenarioAnimDebugHelper::UpdateUFClipSetNames()
{
	m_UFClipsetNames.Reset();

	m_UFClipsetNames.Reserve(fwClipSetManager::GetClipSetCount());
	for(int clipSetIndex = 0; clipSetIndex < fwClipSetManager::GetClipSetCount(); clipSetIndex++)
	{
		fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(clipSetIndex);
		m_UFClipsetNames.Append() = clipSetId.GetCStr();
	}

	ValidateNameFill(m_UFClipsetNames);
}

void CScenarioAnimDebugHelper::UpdateCClipSetNames()
{
	m_CClipsetNames.Reset();

	Assert(m_SelCConGroup >= 0);
	Assert(m_SelCConGroup < m_CConGroupNames.GetCount());

	Assert(m_SelCConClips >= 0);
	Assert(m_SelCConClips < m_CConClipsNames.GetCount());

	Assert(m_SelCType >= 0);
	Assert(m_SelCType < m_AnimTypeNames.GetCount());

	const CConditionalAnimsGroup* congroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_CConGroupNames[m_SelCConGroup]);
	if (aiVerify(congroup))
	{
		const CConditionalAnims * foundAnims = NULL;

		const u32 count = congroup->GetNumAnims();
		for (u32 con = 0; con < count; con++)
		{
			const CConditionalAnims * conanims = congroup->GetAnims(con);
			if (aiVerify(conanims))
			{
				if (!stricmp(conanims->GetName(), m_CConClipsNames[m_SelCConClips]))
				{
					foundAnims = conanims;
					break;
				}
			}
		}

		if(foundAnims)
		{
			const CConditionalAnims::ConditionalClipSetArray* clipsets = foundAnims->GetClipSetArray((CConditionalAnims::eAnimType)m_SelCType);
			if (clipsets)
			{
				const int clipsetcount = clipsets->GetCount();
				m_CClipsetNames.Reserve(clipsetcount);
				for (int clipset = 0; clipset < clipsetcount; clipset++)
				{
					const CConditionalClipSet* conclipset = clipsets->GetElements()[clipset];
					if (aiVerify(conclipset))
					{
						m_CClipsetNames.Append() = conclipset->GetClipSetName();
					}
				}
			}
		}
	}

	ValidateNameFill(m_CClipsetNames);
}

void CScenarioAnimDebugHelper::UpdateFClipNames()
{
	m_FClipNames.Reset();

	Assert(m_SelFClipSet >= 0);
	Assert(m_SelFClipSet < m_FClipsetNames.GetCount());

	//Fill the clipset names
	fwMvClipSetId clipSetId(m_FClipsetNames[m_SelFClipSet]);
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (pClipSet)
	{
		const s32 iNumClipsInSet = pClipSet->GetClipItemCount();
		m_FClipNames.Reserve(iNumClipsInSet);
		for( s32 i = 0; i < iNumClipsInSet; i++ )
		{
			fwMvClipId id = pClipSet->GetClipItemIdByIndex(i);
			m_FClipNames.Append() = id.IsNotNull() ? id.GetCStr() : "N/A";
		}
	}

	ValidateNameFill(m_FClipNames);
}

void CScenarioAnimDebugHelper::UpdateUFClipNames()
{
	m_UFClipNames.Reset();

	Assert(m_SelUFClipSet >= 0);
	Assert(m_SelUFClipSet < m_UFClipsetNames.GetCount());

	//Fill the clipset names
	fwMvClipSetId clipSetId(m_UFClipsetNames[m_SelUFClipSet]);
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (pClipSet)
	{
		const s32 iNumClipsInSet = pClipSet->GetClipItemCount();
		m_UFClipNames.Reserve(iNumClipsInSet);
		for( s32 i = 0; i < iNumClipsInSet; i++ )
		{
			fwMvClipId id = pClipSet->GetClipItemIdByIndex(i);
			m_UFClipNames.Append() = id.IsNotNull() ? id.GetCStr() : "N/A";
		}
	}

	ValidateNameFill(m_UFClipNames);
}

void CScenarioAnimDebugHelper::UpdateCClipNames()
{
	m_CClipNames.Reset();

	Assert(m_SelCClipSet >= 0);
	Assert(m_SelCClipSet < m_CClipsetNames.GetCount());

	//Fill the clipset names
	fwMvClipSetId clipSetId(m_CClipsetNames[m_SelCClipSet]);
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (pClipSet)
	{
		const s32 iNumClipsInSet = pClipSet->GetClipItemCount();
		m_CClipNames.Reserve(iNumClipsInSet);
		for( s32 i = 0; i < iNumClipsInSet; i++ )
		{
			fwMvClipId id = pClipSet->GetClipItemIdByIndex(i);
			m_CClipNames.Append() = id.IsNotNull() ? id.GetCStr() : "N/A";
		}
	}

	ValidateNameFill(m_CClipNames);
}

void CScenarioAnimDebugHelper::UpdateFPropNames()
{
	m_FPropNames.Reset();
	m_FPropSetName[0] = '\0';

	Assert(m_SelFConGroup >= 0);
	Assert(m_SelFConGroup < m_FConGroupNames.GetCount());

	//Fill the clipset names
	const CAmbientPropModelSet* pPropSet = NULL;
	atHashString scenarioName(m_ScenatioTypeNames[m_SelScenarioType]);
	if (scenarioName != NA_ScenarioType)
	{
		const CScenarioInfo* sceninfo = SCENARIOINFOMGR.GetScenarioInfo(scenarioName);
		if (aiVerifyf(sceninfo,"Did not find expected scenario info for %s", m_ScenatioTypeNames[m_SelScenarioType]))
		{
			const CConditionalAnimsGroup* congroup = sceninfo->GetClipData();
			if (aiVerify(congroup))
			{
				const u32 count = congroup->GetNumAnims();
				for (u32 con = 0; con < count; con++)
				{
					const CConditionalAnims * conanims = congroup->GetAnims(con);
					if (aiVerify(conanims))
					{
						if (!stricmp(conanims->GetName(), m_FConGroupNames[m_SelFConGroup]))
						{
							//Perhaps we dont have a propset ... 
							if (conanims->GetPropSetName())
							{
								formatf(m_FPropSetName, "%s", conanims->GetPropSetName());
								pPropSet = CAmbientAnimationManager::GetPropSetFromHash(conanims->GetPropSetHash());
							}
							break;
						}
					}
				}
			}
		}
	}

	if (pPropSet)
	{
		const u32 count = pPropSet->GetNumModels();
		m_FPropNames.Reserve(count);
		for (u32 i = 0; i < count; i++)
		{
			m_FPropNames.Append() = pPropSet->GetModelName(i);
		}
	}

	ValidateNameFill(m_FPropNames);
}

void CScenarioAnimDebugHelper::UpdateCPropNames()
{
	m_CPropNames.Reset();
	m_CPropSetName[0] = '\0';
	const CAmbientPropModelSet* pPropSet = NULL;

	Assert(m_SelCConGroup >= 0);
	Assert(m_SelCConGroup < m_CConGroupNames.GetCount());

	Assert(m_SelCConClips >= 0);
	Assert(m_SelCConClips < m_CConClipsNames.GetCount());

	const CConditionalAnimsGroup* congroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_CConGroupNames[m_SelCConGroup]);
	if (congroup)
	{
		const CConditionalAnims * foundAnims = NULL;

		const u32 count = congroup->GetNumAnims();
		for (u32 con = 0; con < count; con++)
		{
			const CConditionalAnims * conanims = congroup->GetAnims(con);
			if (aiVerify(conanims))
			{
				if (!stricmp(conanims->GetName(), m_CConClipsNames[m_SelCConClips]))
				{
					foundAnims = conanims;
					break;
				}
			}
		}

		//Perhaps we dont have a propset ... 
		if(foundAnims && foundAnims->GetPropSetName())
		{
			formatf(m_CPropSetName, "%s", foundAnims->GetPropSetName());
			pPropSet = CAmbientAnimationManager::GetPropSetFromHash(foundAnims->GetPropSetHash());
		}
	}

	if (pPropSet)
	{
		const u32 count = pPropSet->GetNumModels();
		m_CPropNames.Reserve(count);
		for (u32 i = 0; i < count; i++)
		{
			m_CPropNames.Append() = pPropSet->GetModelName(i);
		}
	}

	ValidateNameFill(m_CPropNames);
}

void CScenarioAnimDebugHelper::UpdateQueueUI()
{
	if (!m_QueueGroup)
		return;

	while(m_QueueGroup->GetChild())
		m_QueueGroup->Remove(*m_QueueGroup->GetChild());

	char name[32];
	for (int item = 0; item < m_AnimQueue.GetCount(); item++)
	{
		formatf(name, "Item %d", item);
		bkGroup* itemgroup = m_QueueGroup->AddGroup(name,false);
		m_AnimQueue[item].AddWidgets(itemgroup);
	}
}

void CScenarioAnimDebugHelper::ValidateNameFill(atArray<const char *>& nameArray, bool sort /*= true*/)
{
	if (!nameArray.GetCount())
	{
		if (!nameArray.GetCapacity())
		{
			nameArray.Reserve(1);
		}
		nameArray.Append() = "N/A";
	}

	if (sort && nameArray.GetCount() > 1)
	{
		nameArray.QSort(0, -1, SortAlphabeticalNoCase);
	}
}

void CScenarioAnimDebugHelper::QueueClip(fwMvClipSetId clipSetId, fwMvClipId clipId, atHashString conGroupName, int animType, atHashString propName)
{
	Assert(animType >= CConditionalAnims::AT_BASE);
	Assert(animType < CConditionalAnims::AT_COUNT);

	ScenarioAnimQueueItem item;
	item.m_Invalid = false;
	item.m_Info.m_ClipSetId = clipSetId;
	item.m_Info.m_ClipId = clipId;
	item.m_Info.m_AnimType = (CConditionalAnims::eAnimType)animType;
	item.m_Info.m_ConditionName = conGroupName;
	item.m_Info.m_BasePlayTime = m_BasePlayTime;

	// 0.75f is the lowest option - 1.25f is the highest option that is had for how long to play the base clip 
	//	between attempts to get a new idle variation. CTaskAmbientClips::InitIdleTimer
	item.m_WidgetMinIdleTime = m_BasePlayTime*0.75f;
	item.m_WidgetMaxIdleTime = m_BasePlayTime*1.25f;

	//only base clips should have a play time so set this to 0.0 for anything else
	// so we dont get a delay when the clip is being played.
	if (item.m_Info.m_AnimType != CConditionalAnims::AT_BASE)
	{
		item.m_Info.m_BasePlayTime = 0.0f;
	}

	if (propName != NA_Prop)
	{
		item.m_Info.m_PropName.Clear();
		//validate that the prop is known ... 
		CBaseModelInfo* pPropModelInfo = CModelInfo::GetBaseModelInfoFromName(propName, NULL);
		if (taskVerifyf( pPropModelInfo, "Selected debug Prop [%s] with model index [%d] is not a valid model index", propName.GetCStr(), propName.GetHash()))
		{
			item.m_Info.m_PropName = propName;
		}
	}
	else
	{
		item.m_Info.m_PropName.Clear();
	}

	m_AnimQueue.PushAndGrow(item);

	UpdateQueueUI();
}

void CScenarioAnimDebugHelper::SwitchPoint(CScenarioPoint& point)
{
	ResetSelected();
	m_ScenarioPoint = &point;
	AddWidgets();
}

void CScenarioAnimDebugHelper::SwitchEntity(CEntity& entity)
{
	ClearEntity();

	m_SelectedEntity = &entity;

	//setup the new guy for debugging
	if (m_SelectedEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_SelectedEntity.Get());
		CTask* task = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		if (task)
		{
			aiAssert(task->GetTaskType() != CTaskTypes::TASK_SCENARIO_ANIM_DEBUG);
			//Toggle it on
			pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskScenarioAnimDebug(*this), PED_TASK_PRIORITY_PRIMARY , true);
		}
		else
		{
			//Toggle it on if no primary task
			pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskScenarioAnimDebug(*this), PED_TASK_PRIORITY_PRIMARY , true);
		}
	}
}

void CScenarioAnimDebugHelper::ClearEntity()
{
	if (m_SelectedEntity && m_SelectedEntity->GetIsTypePed())
	{
		//clear the old selected guy
		CPed* pPed = static_cast<CPed*>(m_SelectedEntity.Get());
		CTask* task = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		if (task)
		{
			if (task->GetTaskType() == CTaskTypes::TASK_SCENARIO_ANIM_DEBUG)
			{
				//Toggle it off
				pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(1), PED_TASK_PRIORITY_PRIMARY , true);
			}
		}

		m_SelectedEntity.Reset(NULL);
	}
}

void CScenarioAnimDebugHelper::EnqueueFromFile(const char* filename)
{
	ScenarioQueueLoadSaveObject loadObj;
	if (Verifyf(PARSER.LoadObject(filename, "", loadObj), "Unable to load file '%s'", filename))
	{
		const int count = loadObj.m_QueueItems.GetCount();
		for (int i = 0; i < count; i++)
		{
			ScenarioAnimQueueItem item;
			item.m_Invalid = false;
			item.m_Info = loadObj.m_QueueItems[i];
			
			// 0.75f is the lowest option - 1.25f is the highest option that is had for how long to play the base clip 
			//	between attempts to get a new idle variation. CTaskAmbientClips::InitIdleTimer
			item.m_WidgetMinIdleTime = item.m_Info.m_BasePlayTime*0.75f;
			item.m_WidgetMaxIdleTime = item.m_Info.m_BasePlayTime*1.25f;

			m_AnimQueue.PushAndGrow(item);
		}
	}

	UpdateQueueUI();
}

///////////////////////////////////////////////////////////////////////////////
//  CODE CScenarioAnimDebugHelper::ScenarioAnimQueueItem
///////////////////////////////////////////////////////////////////////////////
void CScenarioAnimDebugHelper::ScenarioAnimQueueItem::AddWidgets(bkGroup* parent)
{
	char* cs = (char*)m_Info.m_ClipSetId.GetCStr();
	char* c = (char*)m_Info.m_ClipId.GetCStr();
	char* at = (char*)CConditionalAnims::GetTypeAsStr(m_Info.m_AnimType);
	parent->AddText("Type", at, istrlen(at)+1, true);
	parent->AddText("ClipSet", cs, istrlen(cs)+1, true);
	parent->AddText("Clip", c, istrlen(c)+1, true);
	char* p = (char*)m_Info.m_PropName.TryGetCStr(); //Note could be null ... 
	if (p)
	{
		parent->AddText("Prop", p, istrlen(p)+1, true);
	}
	if (m_Info.m_AnimType == CConditionalAnims::AT_BASE)
	{
		parent->AddSlider("Idle time", &m_Info.m_BasePlayTime, m_WidgetMinIdleTime, m_WidgetMaxIdleTime, 0.1f);
	}
	parent->AddButton("Remove", datCallback(MFA(CScenarioAnimDebugHelper::ScenarioAnimQueueItem::OnRemovePressed), this));
}

void CScenarioAnimDebugHelper::ScenarioAnimQueueItem::OnRemovePressed()
{
	m_Invalid = true;
}
#endif //SCENARIO_DEBUG
