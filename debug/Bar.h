//
// debug/bar.h
//
// Copyright (C) 1999-2013 Rockstar North.  All Rights Reserved. 
//

#include "debug/debug.h"

#ifndef __DEBUG_DEBUG_H__
#define __DEBUG_DEBUG_H__

#define FINAL_DISPLAY_BAR	!__MASTER || __FINAL_LOGGING
#if FINAL_DISPLAY_BAR
#define FINAL_DISPLAY_BAR_ONLY(...)		__VA_ARGS__
#else
#define FINAL_DISPLAY_BAR_ONLY(...)
#endif
#if ENABLE_DEBUG_HEAP
namespace rage 
{
	extern size_t g_sysExtraStreamingMemory;
}
#endif
#if FINAL_DISPLAY_BAR

enum eDEBUG_DISPLAY_STATE
{
	DEBUG_DISPLAY_STATE_OFF = 0,
	DEBUG_DISPLAY_STATE_STANDARD,
	DEBUG_DISPLAY_STATE_COORDS_ONLY,
	MAX_DEBUG_DISPLAY_STATES
};

class CDebugBar
{
public:
	static void Init(unsigned initMode);
	static void Update();
	static void Render();

	static eDEBUG_DISPLAY_STATE GetDisplayReleaseDebugText() {return sm_displayReleaseDebugText;}
	static void SetDisplayReleaseDebugText(eDEBUG_DISPLAY_STATE displayRelease) {sm_displayReleaseDebugText = displayRelease;}

#if __WIN32PC && __BANK
	static char* GetSystemInfo()			{ return sm_systemInfo; };
	static void  SetSystemInfo(char* info)	{ sm_systemInfo = info; };
#endif

private:
	static void Render_DISPLAY_STATE_STANDARD();
	static void Render_DEBUG_DISPLAY_STATE_COORDS_ONLY();
	static void RenderBackground(float position);

	static eDEBUG_DISPLAY_STATE sm_displayReleaseDebugText;

#if __WIN32PC && __BANK
	static char* sm_systemInfo;
#endif
};

#endif // FINAL_DISPLAY_BAR

#endif // __DEBUG_DEBUG_H__
