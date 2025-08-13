#ifndef __PAUSESTOREMENU_H__
#define __PAUSESTOREMENU_H__

// rage
#include "atl/string.h"

// game
#include "frontend/CMenuBase.h"
#include "frontend/WarningScreen.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"


//Class to implement the store menu tab in the pause screen.
// (very) small now, this is being implemented in this way to give quick flexibility in the unlikely event of redesigns.
enum eStoreState
{
	STATE_NONE,
	STATE_UNAVAILABLE,
	STATE_UNAVAILABLE_MP,
	STATE_DISCONNECTED,
	STATE_NOCONNECTION,
#if RSG_ORBIS
	STATE_NP_UNAVAILABLE,
#endif  // RSG_ORBIS
	STATE_UNDERAGE
};

class CPauseStoreMenu : public CMenuBase
{
public:

    CPauseStoreMenu(CMenuScreen& owner);
    virtual ~CPauseStoreMenu();

    void Init();

    virtual void Update();
   // virtual void Render(const PauseMenuRenderDataExtra* renderData);
    virtual bool UpdateInput(s32 sInput);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void LoseFocus();

	eStoreState GetStoreState();
	bool IsStoreUnavailableMP();
	bool IsLinkNotConnected();
	bool IsNotConnected();
	bool IsStoreUnavailable();

	bool PopulateStoreNews();
	void PostStoreStory(CNetworkSCNewsStoryRequest* pNewsItem, bool bUpdateTexture);
	void UpdateStoreColumnScroll(int iNumStories, int iSelectedStory);

	void ConfirmDestructiveActionCallback();

private:
	
	void DisplayUnderageWarning();
	void DisplayLeaveSessionWarning();
	void DisplayStoreUnavailableWarning();
	
	eStoreState m_storeState;
	int m_StoreSigninFrameCount;

	bool m_WaitingForLoginScreenReturn;
    bool m_WaitingForLoginScreenEntrance;
	bool m_bIsDisplayingNews;
	bool m_bStoryTextPosted;
};


#endif //__PAUSESTOREMENU_H__
