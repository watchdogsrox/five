/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BootupScreen.h
// PURPOSE : Base class for screens used in the boot flow
//
// AUTHOR  : derekp
// STARTED : 2013, relocated October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef BOOTUP_SCREEN_H
#define BOOTUP_SCREEN_H

class CBootupScreen
{
protected:
	static const unsigned MAX_CUSTOM_BUTTON_TEXT = 128; 
	static char m_EventButtonDisplayName[MAX_CUSTOM_BUTTON_TEXT];
	static char m_JobButtonDisplayName[MAX_CUSTOM_BUTTON_TEXT];
	static bool HasCompletedPrologue();
	static bool CanSelectOnline();
	static const char* GetEventButtonText(bool bGetLoadingText);
	static const char* GetJobButtonText(bool bGetLoadingText);

#if RSG_SCE
    static bool ShouldShowPlusUpsell();
#endif // RSG_ORBIS
};

#endif // BOOTUP_SCREEN_H
