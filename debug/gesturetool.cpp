/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    gesturetool.cpp
// PURPOSE : a tool for generating gesture data on audio samples
// AUTHOR :  Greg
// CREATED : 5/1/2007
//
/////////////////////////////////////////////////////////////////////////////////

#if __DEV

// Rage headers
#include "grcore/debugdraw.h"

// Game headers

#include "gesturetool.h"
#include "ModelInfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Renderer/renderer.h"
#include "Frontend/MiniMap.h"

#include "Text/Text.h"
#include "Text/TextConversion.h"


bool CGestureTool::m_active(false);
bkBank* CGestureTool::m_pBank(NULL);
CPed* CGestureTool::m_pPed(NULL);
atArray<const char*> CGestureTool::m_modelNames;
int CGestureTool::m_modelIndex(-1);
atArray<const char*> CGestureTool::m_audioNames;
int CGestureTool::m_audioIndex(-1);
atArray<const char*> CGestureTool::m_gestureNames;
int CGestureTool::m_gestureIndex(-1);
float CGestureTool::m_currentTime(0.0f);
float CGestureTool::m_animLength(10.0f);

int CGestureTool::CbQSortModelNames(const char* const* pp_A,const char* const* pp_B)
{
	return stricmp(*pp_A,*pp_B);
}

void CGestureTool::Initialise()
{
	m_pPed = FindPlayerPed();
	Assert(m_pPed);

	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);
	int numPedsAvail = pedTypesArray.GetCount();

	for (int i=0; i<numPedsAvail; i++)
	{
		CPedModelInfo& pedModelInfo = *pedTypesArray[i];
		m_modelNames.Grow() = pedModelInfo.GetModelName();
	}

	m_modelNames.QSort(0,m_modelNames.GetCount(),CbQSortModelNames);

	for (int i=0; i<m_modelNames.GetCount(); i++)
	{
		u32 modelIndex = CModelInfo::GetModelIdFromName(m_modelNames[i]).GetModelIndex();
		if (modelIndex  == m_pPed->GetModelIndex())
		{
			m_modelIndex = i;
		}
	}

	m_audioNames.Grow() = "none";
	m_gestureNames.Grow() = "none";

	AddWidgets();
}

void CGestureTool::Shutdown()
{

}

void CGestureTool::Process()
{

}

#define TIMELINE_START 20.0f/320.0f
#define TIMELINE_END 300.0f/320.0f
#define TIMELINE_POSY 200.0f/240.0f

void CGestureTool::Render()
{
	if(!m_active)
		return;

	CTextLayout	DebugTextLayout;

	Vector2 vTextScale(0.25f, 0.3f);
	u8 iAlpha			= 0xff;

	grcDebugDraw::SetDoDebugZTest(false);

	Color32 textColor(255, 255, 0);

	DebugTextLayout.SetScale(&vTextScale);
	DebugTextLayout.SetColor(CRGBA(255, 255, 0, iAlpha));
	DebugTextLayout.SetEdge(1);

	char PrintBuffer[64];
	sprintf(PrintBuffer,"%.2f",m_animLength);

	Vector2 vTextPosition(TIMELINE_START, (TIMELINE_POSY + 1.0f));

	DebugTextLayout.Render(&vTextPosition, "0.0");
	DebugTextLayout.Render(&vTextPosition, PrintBuffer);

	//timeline

	grcDebugDraw::Line(Vector2(TIMELINE_START, TIMELINE_POSY),Vector2( TIMELINE_END, TIMELINE_POSY), textColor);

	//position 
	float ScreenMarkerPos = TIMELINE_START + ((TIMELINE_END - TIMELINE_START) * (m_currentTime / m_animLength));

	grcDebugDraw::Line(Vector2(ScreenMarkerPos, TIMELINE_POSY),Vector2( ScreenMarkerPos + 5.0f, TIMELINE_POSY + 5.0f), textColor);
	grcDebugDraw::Line(Vector2(ScreenMarkerPos, TIMELINE_POSY),Vector2( ScreenMarkerPos - 5.0f, TIMELINE_POSY + 5.0f), textColor);
	grcDebugDraw::Line(Vector2(ScreenMarkerPos - 5.0f, TIMELINE_POSY + 5.0f),Vector2( ScreenMarkerPos + 5.0f, TIMELINE_POSY + 5.0f), textColor);
}

void CGestureTool::AddWidgets()
{
	bool shown;
	int x, y, width, height; 

	if (m_pBank)
	{
		m_pBank->GetState(shown, x, y, width, height);
		BANKMGR.DestroyBank(*m_pBank);
		bkBank & bank = BANKMGR.CreateBank("Gesture Tool", 100, 100, false);
		m_pBank = &bank;
		bank.SetState(true, x, y, width, height);
	}
	else
	{
		bkBank & bank = BANKMGR.CreateBank("Gesture Tool", 100, 100, false);
		m_pBank = &bank;
	}

	m_pBank->AddToggle(	"Active",							
						&m_active,							
						Activate, 
						"Activate Gesture Tool");

	m_pBank->AddCombo(	"Model name", 
						&m_modelIndex, 
						m_modelNames.GetCount(), 
						&m_modelNames[0], 
						SelectPed, 
						"Lists the models");

	m_pBank->AddCombo(	"Audio file", 
						&m_audioIndex, 
						m_audioNames.GetCount(), 
						&m_audioNames[0], 
						SelectAudio, 
						"Lists the audio files");

	m_pBank->AddSlider(	"Current Time", 
						&m_currentTime, 
						0.0f, 
						100000.0f, 
						0.001f, 
						ChangeTime, 
						"Lists the audio files");

	m_pBank->AddButton(	"Play", 
						PlaySound,
						"Begin playback of sound");

	m_pBank->AddButton(	"Pause", 
						PauseSound,
						"Pause playback of sound");

	m_pBank->AddCombo(	"Gesture", 
						&m_gestureIndex, 
						m_gestureNames.GetCount(), 
						&m_gestureNames[0], 
						SelectGesture, 
						"Lists the audio files");

	m_pBank->AddButton(	"Goto Next", 
						GotoNext,
						"Go to next gesture");

	m_pBank->AddButton(	"Goto Previous", 
						GotoPrevious,
						"Go to previous gesture");

	m_pBank->AddButton(	"Set", 
						SetGesture,
						"Set gesture at current time");

	m_pBank->AddButton(	"Delete", 
						DeleteGesture,
						"Delete gesture at current time");
}

void CGestureTool::Activate()
{
	CMiniMap::SetVisible(m_active);
}

void CGestureTool::SelectPed()
{

}

void CGestureTool::SelectAudio()
{

}

void CGestureTool::SelectGesture()
{

}

void CGestureTool::ChangeTime()
{
	if(m_currentTime > m_animLength)
		m_currentTime = m_animLength;

	if(m_currentTime < 0.0f)
		m_currentTime = 0.0f;
}

void CGestureTool::PlaySound()
{

}

void CGestureTool::PauseSound()
{

}

void CGestureTool::GotoNext()
{

}

void CGestureTool::GotoPrevious()
{

}

void CGestureTool::SetGesture()
{

}

void CGestureTool::DeleteGesture()
{

}

#endif //#if __DEV


