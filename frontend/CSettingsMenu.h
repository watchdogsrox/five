#ifndef __SETTINGS_MENU_H__
#define __SETTINGS_MENU_H__

#include "Frontend/CMenuBase.h"
#include "Frontend/PauseMenu.h"
#include "system/EndUserBenchmark.h"
#include "atl/string.h"
#include "net/status.h"

namespace rage
{
	class parElement;
};


class CSettingsMenu : public CMenuBase
{
public:
	CSettingsMenu(CMenuScreen& owner);
	~CSettingsMenu();

	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual void Update();
	virtual bool UpdateInput(s32 eInput);
	virtual void LoseFocus();
	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId);
	virtual bool ShouldPlayNavigationSound(bool UNUSED_PARAM(bNavUp));
	virtual bool Populate(MenuScreenId newScreenId);

#if __BANK
	virtual void AddWidgets(bkBank* ToThisBank);

#endif

private:
	bool IsSideBarLayout(MenuScreenId screenID);
	bool IsPrefControledBySlider(eMenuPref ePref);
	char* GetControllerImageHeader();
	void PopulateControllerLabels(char* controllerHeader);
	bool IsInStateWithAButton() const;
	void PopulateWithMessage(const char* pHeader, const char* pBody, bool bLeaveRoom);
	bool IsOnControllerPref(eMenuPref currentPref);
#if COLLECT_END_USER_BENCHMARKS
	void OnConfirmBenchmarTests();
#endif

	enum SignupStatus
	{
		SS_OffPage,
		SS_Offline,
		SS_FacebookDown,
		SS_NoSocialClub,
		SS_OldPolicy,
		SS_NoFacebook,
		SS_TotallyFine,
		SS_NoPermission,
		SS_Banned,
		SS_Busy
	};

#if RSG_PC
	u32 m_uGfxApplyTimestamp;
	bool m_bFailedApply;
	bool m_bWriteFailed;
	bool m_bDisplayRestartWarning;

	bool m_bHasShownOnlineGfxWarning;
	bool m_bDisplayOnlineWarning;

	bool m_bOnVoiceTab;

	void ApplyGfxSettings();
	void WriteGfxSettings();

	void StartBenchmarkScript();
	void HandlePCWarningScreens();
#endif // RSG_PC

	s16			m_fbookPageHeight;

	SignupStatus m_eState;

	bool		m_bOnFacebookTab;
	bool		m_bOnControllerTab;
	bool		m_bInsideSystemUI;
	bool		m_bInSettingsColumn;
	bool		m_bHaveNavigatedToTheRightMenu;

#if COMMERCE_CONTAINER
	bool		m_bHaveLoadedPRX;
	bool		m_bWasWirelessHeadsetAvailable;
	void		PreparePRX(bool bLoadOrNot);
#endif

#if __BANK
	bool		CheckStatus(bool bOriginalValue, int iCheckStatus) const;
	bkGroup*	m_pDebugWidget;
	int			m_iFakedStatus;
#endif

	bool ChangeState( SignupStatus newState )
	{ 
		if(m_eState != newState)
		{
			m_eState = newState;
			return true;
		}
		return false;
	}

};


#endif // __SETTINGS_MENU_H__

