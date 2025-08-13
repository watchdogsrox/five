/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    gesturetool.h
// PURPOSE : a tool for generating gesture data on audio samples
// AUTHOR :  Greg
// CREATED : 5/1/2007
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef GESTURE_TOOL_H
#define GESTURE_TOOL_H

#if __DEV

#include "bank/bank.h"
#include "atl/string.h"
#include "atl/array.h"

class CPed;

class CGestureTool
{
public:
	static void Initialise();
	static void Shutdown();
	static void Process();
	static void Render();
	static bool IsActive() { return m_active; }

	static void Activate();
	static void SelectPed();
	static void SelectAudio();
	static void SelectGesture();
	static void ChangeTime();
	static void PlaySound();
	static void PauseSound();
	static void GotoNext();
	static void GotoPrevious();
	static void SetGesture();
	static void DeleteGesture();
	static int CbQSortModelNames(const char* const* pp_A,const char* const* pp_B);

private:
	static void AddWidgets();

	static bool m_active;
	static CPed* m_pPed;
	static bkBank* m_pBank;
	static float m_currentTime;
	static float m_animLength;
	static atArray<const char*> m_modelNames;
	static int m_modelIndex;
	static atArray<const char*> m_audioNames;
	static int m_audioIndex;
	static atArray<const char*> m_gestureNames;
	static int m_gestureIndex;	
};

#endif //#if __DEV

#endif //#define GESTURE_TOOL_H

