/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorPauseMenu.h
// PURPOSE : the pausemenu menu tab for replay editor
// AUTHOR  : Derek Payne
// STARTED : 11/09/2014
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __PAUSEVIDEOEDITORMENU_H__
#define __PAUSEVIDEOEDITORMENU_H__

#include "control/replay/ReplaySettings.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"

#if GTA_REPLAY

// rage
#include "atl/string.h"

// game
#include "frontend/CMenuBase.h"

#define MAX_NEWS_URL_LENGTH 512

class CPauseVideoEditorMenu : public CMenuBase
{
public:

    CPauseVideoEditorMenu(CMenuScreen& owner);
    virtual ~CPauseVideoEditorMenu();

    void Init();
    virtual void Update();
    virtual bool UpdateInput(s32 sInput);
	virtual bool Populate(MenuScreenId newScreenId);
	void PopulateWarning(const char* locTitle, const char* locBody);
	virtual void LoseFocus();

	static char* GetButtonURL() { return sm_ButtonURL; }

	bool PopulateNews();
	void UpdateStoreColumnScroll(int iNumStories, int iSelectedStory);
	void PostEditorStory(CNetworkSCNewsStoryRequest* pNewsItem, bool bUpdateTexture);

private:
	void Reset();

	static bool sm_waitOnSignIn;
	static bool sm_waitOnSignInUi;
	bool m_isDisplayingNews;
	bool m_storyTextPosted;
	static char sm_ButtonURL[MAX_NEWS_URL_LENGTH];
};

#endif //GTA_REPLAY

#endif //__PAUSEVIDEOEDITORMENU_H__


// eof
