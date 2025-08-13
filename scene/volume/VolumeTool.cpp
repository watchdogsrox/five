/////////////////////////////////////////////////////////////////////////////////
// FILE :		VolumeTool.cpp
// PURPOSE :	A tool for volume manipulation
// AUTHOR :		Jason Jurecka.
// CREATED :	10/21/2011
/////////////////////////////////////////////////////////////////////////////////

#include "Scene/Volume/VolumeTool.h"

#if __BANK && VOLUME_SUPPORT

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

//rage
#include "input/mouse.h"
#include "physics/debugPlayback.h"

//framework

//Game

#include "Scene/Volume/VolumeManager.h"
#include "script/script.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "system/controlMgr.h"
#include "script/handlers/GameScriptResources.h"
#include "camera/debug/DebugDirector.h"

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
CVolumeTool g_VolumeTool;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////
CVolumeTool::CVolumeTool()
: m_SelectedFilter(0xffffffff)
, m_isOpen(false)
, m_ParentBank(NULL)
{
	m_ExportFileName[0] = '\0';
}

void CVolumeTool::Init ()
{
	const camBaseCamera* active = camInterface::GetDominantRenderedCamera();
	const camFrame& activeFrame = active->GetFrame();

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	debugDirector.ActivateFreeCam();
	camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
	// move debug camera to desired place
	freeCamFrame.SetWorldMatrixFromFrontAndUp(activeFrame.GetFront(), activeFrame.GetUp());
	freeCamFrame.SetPosition(activeFrame.GetPosition());
	CControlMgr::SetDebugPadOn(true);

	m_isOpen = true;

	safecpy(m_ExportFileName, "x:\\volTool.txt");
}

void CVolumeTool::Shutdown ()
{
	if (m_isOpen)
	{
		if (m_Editor.IsActive())
		{
			m_Editor.Shutdown();
		}

		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		debugDirector.DeactivateFreeCam();
		CControlMgr::SetDebugPadOn(false);
		m_isOpen = false;
	}
}

void CVolumeTool::Update ()
{
	if (m_Editor.IsActive())
	{
		//Editor handles all updating ... 
		m_Editor.Update();
	}
	else
	{
		//Update selection and hovering ... 
	}
}

void CVolumeTool::Render ()
{
	if (m_Editor.IsActive())
	{
		//Editor handles all rendering ... 
		m_Editor.Render();
	}
	else
	{
#if DR_ENABLED
		int mousex = ioMouse::GetX();
		int mouseY = ioMouse::GetY();

		debugPlayback::OnScreenOutput screenspew(10.0f);
		screenspew.m_fXPosition = 50.0f;
		screenspew.m_fYPosition = 50.0f;
		screenspew.m_fMouseX = (float)mousex;
		screenspew.m_fMouseY = (float)mouseY;
		screenspew.m_Color.Set(160,160,160,255);
		screenspew.m_HighlightColor.Set(255,0,0,255);

		screenspew.AddLine("------------");
		if (screenspew.AddLine("Scripts {%d}", CGameScriptHandlerMgr::ms_NumHandlers))
		{
			if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
			{
				m_SelectedFilter = 0xffffffff;
			}
		}
		screenspew.AddLine("------------");
#endif
		for(unsigned i = 0; i < CGameScriptHandlerMgr::ms_NumHandlers; i++)
		{
			CGameScriptHandlerMgr& mgr = CTheScripts::GetScriptHandlerMgr();
			const CGameScriptHandler* handler = mgr.GetScriptHandlerFromComboName(CGameScriptHandlerMgr::ms_Handlers[i]);
			if (AssertVerify(handler))
			{
#if DR_ENABLED
				unsigned vCount = handler->GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_VOLUME);
				if (i == m_SelectedFilter)
				{
					screenspew.m_bDrawBackground = true;
				}

				m_RenderStyle = CVolumeTool::ersNormal;
				if (screenspew.AddLine("%s (%d)", CGameScriptHandlerMgr::ms_Handlers[i], vCount))
				{
					m_RenderStyle = CVolumeTool::ersHighlight;
					if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
					{
						m_SelectedFilter = i;
					}
				}
				else if (m_SelectedFilter != 0xffffffff)
				{
					if (m_SelectedFilter == i)
					{
						m_RenderStyle = CVolumeTool::ersSelected;
					}
					else
					{
						m_RenderStyle = CVolumeTool::ersInActive;
					}
				}
#endif
				handler->ForAllScriptResources(&CVolumeTool::RenderCB, NULL);
#if DR_ENABLED
				screenspew.m_bDrawBackground = false;
#endif
			}
		}
	}
}

void CVolumeTool::CreateWidgets (bkBank* pBank)
{
	m_ParentBank = pBank;
	pBank->AddText("Export File", m_ExportFileName, RAGE_MAX_PATH);
	pBank->AddButton("Export Script Volumes", &CVolumeTool::ExportScript);
	pBank->AddButton("Copy Script Volumes to clipboard", &CVolumeTool::CopyScriptToClipboard);
	pBank->PushGroup("Editor", true);
	pBank->PopGroup();
	ExitEditMode();  
}

void CVolumeTool::ExitEditMode ()
{
	Assertf(m_ParentBank, "Parent bank is not set");
	bkWidget* child = m_ParentBank->GetChild();
	while(child)
	{
		if (!stricmp("Editor", child->GetTitle()))
		{
			//only delete the editor widgets ... 
			child->Destroy();
			break;
		}
		child = child->GetNext();
	}
	m_ParentBank->PushGroup("Editor", true);
	m_ParentBank->AddButton("Enter Edit Mode", &CVolumeTool::StartEditMode);
	m_ParentBank->PopGroup();
	m_Editor.Shutdown();
}

void CVolumeTool::EnterEditMode ()
{
	if (m_SelectedFilter != 0xffffffff)
	{
		Assertf(m_ParentBank, "Parent bank is not set");
		bkWidget* child = m_ParentBank->GetChild();
		while(child)
		{
			if (!stricmp("Editor", child->GetTitle()))
			{
				//only delete the editor widgets ... 
				child->Destroy();
				break;
			}
			child = child->GetNext();
		}

		CGameScriptHandlerMgr& mgr = CTheScripts::GetScriptHandlerMgr();
		CGameScriptHandler* handler = mgr.GetScriptHandlerFromComboName(CGameScriptHandlerMgr::ms_Handlers[m_SelectedFilter]);
		Assert(handler);
		m_Editor.Init(handler);        
		m_ParentBank->PushGroup("Editor", true);
		m_ParentBank->AddButton("Export Selected Script Volumes", &CVolumeTool::ExportSel);
		m_ParentBank->AddButton("Copy Selected Script Volumes to clipboard", &CVolumeTool::CopySelToClipboard);
		m_Editor.CreateWidgets(m_ParentBank);
		m_ParentBank->AddButton("Exit Edit Mode", &CVolumeTool::EndEditMode);
		m_ParentBank->PopGroup();
	}
	else
	{
		Errorf("No script selected so nothing to edit.");
	}
}

void CVolumeTool::ExportSelectedScript ()
{
	if (m_SelectedFilter != 0xffffffff)
	{
		ms_ExportToFile = fiStream::Create(m_ExportFileName);
		ms_ExportCurVolIndex = 0;
		if (ms_ExportToFile)
		{
			const char* scriptName = CGameScriptHandlerMgr::ms_Handlers[m_SelectedFilter];
			Assert(CGameScriptHandlerMgr::ms_NumHandlers > m_SelectedFilter);
			CGameScriptHandlerMgr& mgr = CTheScripts::GetScriptHandlerMgr();
			const CGameScriptHandler* handler = mgr.GetScriptHandlerFromComboName(scriptName);
			Assert(handler);
			unsigned vCount = handler->GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_VOLUME);
			Displayf("Exporting (%d) script volumes for script (%s) to %s", vCount, scriptName, m_ExportFileName);
			handler->ForAllScriptResources(&CVolumeTool::ExportScriptVolumeCB, NULL);
			Displayf("Done Exporting script volumes for script (%s) to %s", scriptName, m_ExportFileName);

			ms_ExportToFile->Close();
			ms_ExportToFile = NULL;
		}
		else
		{
			Errorf("Unable to open file %s", m_ExportFileName);
		}
	}
	else
	{
		Errorf("No script selected to export volumes from.");
	}
}

void CVolumeTool::ExportSelectedScriptToClipboard ()
{
	if (m_SelectedFilter != 0xffffffff)
	{
		BANKMGR.ClearClipboard();
		ms_ExportCurVolIndex = 0;

		const char* scriptName = CGameScriptHandlerMgr::ms_Handlers[m_SelectedFilter];
		Assert(CGameScriptHandlerMgr::ms_NumHandlers > m_SelectedFilter);
		CGameScriptHandlerMgr& mgr = CTheScripts::GetScriptHandlerMgr();
		const CGameScriptHandler* handler = mgr.GetScriptHandlerFromComboName(scriptName);
		Assert(handler);
		unsigned vCount = handler->GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_VOLUME);
		Displayf("Exporting (%d) script volumes for script (%s) to Clipboard", vCount, scriptName);
		handler->ForAllScriptResources(&CVolumeTool::CopyScriptVolumeClipboardCB, NULL);
		Displayf("Done Exporting script volumes for script (%s) to Clipboard", scriptName);
	}
	else
	{
		Errorf("No script selected to export volumes from.");
	}

}

void CVolumeTool::ExportSelected ()
{
	if (m_SelectedFilter != 0xffffffff)
	{
		atMap<unsigned, const CVolume*> SelectedObjects;
		m_Editor.GetSelectedList(SelectedObjects);
		unsigned vCount = SelectedObjects.GetNumUsed();
		if (vCount)
		{
			ms_ExportToFile = fiStream::Create(m_ExportFileName);
			ms_ExportCurVolIndex = 0;
			if (ms_ExportToFile)
			{
				const char* scriptName = CGameScriptHandlerMgr::ms_Handlers[m_SelectedFilter];
				Assert(CGameScriptHandlerMgr::ms_NumHandlers > m_SelectedFilter);

				Displayf("Exporting (%d) selected script volumes for script (%s) to %s", vCount, scriptName, m_ExportFileName);
				atMap<unsigned, const CVolume*>::Iterator iterator = SelectedObjects.CreateIterator();
				for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
				{
					const CVolume* volume = iterator.GetData();
					const CVolumeAggregate* owner = volume->GetOwner();
					//If my owner is in the selected list then i will get exported
					// by that guy .. otherwise i need to export
					if (!SelectedObjects.Access((unsigned)owner))
					{
						volume->ExportToFile();
					}
				}
				Displayf("Done Exporting selected script volumes for script (%s) to %s", scriptName, m_ExportFileName);

				ms_ExportToFile->Close();
				ms_ExportToFile = NULL;
			}
			else
			{
				Errorf("Unable to open file %s", m_ExportFileName);
			} 
		}
		else
		{
			Errorf("Nothing selected to Export to %s.", m_ExportFileName);
		}
	}
	else
	{
		Errorf("Nothing selected so Exporting nothing");
	}
}

void CVolumeTool::CopySelectedToClipboard ()
{
	if (m_SelectedFilter != 0xffffffff)
	{
		atMap<unsigned, const CVolume*> SelectedObjects;
		m_Editor.GetSelectedList(SelectedObjects);
		unsigned vCount = SelectedObjects.GetNumUsed();
		if (vCount)
		{
			BANKMGR.ClearClipboard();
			ms_ExportCurVolIndex = 0;

			const char* scriptName = CGameScriptHandlerMgr::ms_Handlers[m_SelectedFilter];
			Assert(CGameScriptHandlerMgr::ms_NumHandlers > m_SelectedFilter);

			Displayf("Copying (%d) selected script volumes for script (%s) to Clipboard", vCount, scriptName);
			atMap<unsigned, const CVolume*>::Iterator iterator = SelectedObjects.CreateIterator();
			for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
			{
				const CVolume* volume = iterator.GetData();
				const CVolumeAggregate* owner = volume->GetOwner();
				//If my owner is in the selected list then i will get copied
				// by that guy .. otherwise i need to copy
				if (!SelectedObjects.Access((unsigned)owner))
				{
					volume->CopyToClipboard();
				}
			}
			Displayf("Done Copying selected script volumes for script (%s) to Clipboard", scriptName);
		}
		else
		{
			Errorf("Nothing selected to copy to clipboard.");
		}
	}
	else
	{
		Errorf("No script selected to copy volumes from.");
	}
}

///////////////////////////////////////////////////////////////////////////////
//  STATIC CODE
///////////////////////////////////////////////////////////////////////////////
fiStream* CVolumeTool::ms_ExportToFile = NULL;
unsigned  CVolumeTool::ms_ExportCurVolIndex = 0;

bool CVolumeTool::RenderCB(const scriptResource* resource, void* data)
{
	if (resource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_VOLUME)
	{
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(resource->GetReference());
		if (AssertVerify(pVolume))
		{
			CVolumeManager::RenderDebugCB(pVolume, data);
		}
	}
	return true;
}

bool CVolumeTool::ExportScriptVolumeCB(const scriptResource* resource, void* /*data*/)
{
	if (resource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_VOLUME)
	{
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(resource->GetReference());
		if (AssertVerify(pVolume))
		{
			//Owned volumes will be exported by their owner ... 
			if (!pVolume->GetOwner())
			{
				pVolume->ExportToFile();
			}
		}
	}
	return true;
}

bool CVolumeTool::CopyScriptVolumeClipboardCB(const scriptResource* resource, void* /*data*/)
{
	if (resource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_VOLUME)
	{
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(resource->GetReference());
		if (AssertVerify(pVolume))
		{
			//Owned volumes will be exported by their owner ... 
			if (!pVolume->GetOwner())
			{
				pVolume->CopyToClipboard();
			}
		}
	}
	return true;
}

void CVolumeTool::StartEditMode ()
{
	g_VolumeTool.EnterEditMode();
}

void CVolumeTool::EndEditMode ()
{
	g_VolumeTool.ExitEditMode();
}

void CVolumeTool::ExportScript()
{
	g_VolumeTool.ExportSelectedScript();
}

void CVolumeTool::CopyScriptToClipboard()
{
	g_VolumeTool.ExportSelectedScriptToClipboard();
}

void CVolumeTool::ExportSel()
{
	g_VolumeTool.ExportSelected();
}

void CVolumeTool::CopySelToClipboard()
{
	g_VolumeTool.CopySelectedToClipboard();
}
#endif // __BANK && VOLUME_SUPPORT