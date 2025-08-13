#ifndef __STATS_MENU_H__
#define __STATS_MENU_H__

#include "CMenuBase.h"
#include "frontend/PauseMenu.h"

class CStatsMenu : public CMenuBase
{
public:
	CStatsMenu(CMenuScreen& owner);
	~CStatsMenu();

	bool UpdateInput(s32 eInput);
	virtual void Update();
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual void LoseFocus();
	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId);

	virtual void OnNavigatingContent(bool bAreWe);
	virtual bool InitScrollbar(); 
	virtual bool CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

protected:
	bool IsMP();

	int PopulateColumnTitleByPlayerCharacter(int iForcedCharacter = -1);
	void SetMenuceptableSlot(int iSlotIndex, eMenuScreen iMenuID, const char* locKey);
	void UpdateCategory(int iCategory, int iForcedCharacter = -1, bool bScrolling = false);
	void UpdateMiscInfo(eMenuScreen iMenuID);

	int GetTotalStatsThisCategory(int iCharacters);
	int GetChecklistColor(int iCurrentItem);

private:
	int m_statsCategoryIndex;
	int m_statsPlayer;

	int m_iReturnIndex;
	eMenuScreen m_iPreviousSubMenu;
	
	int m_iStatOffset;
	int m_iTotalStatsInCategory;
	int m_iCategoryOffset;
	bool m_bShowPlayerColumn;
	bool m_bForcePlayerColumn;

	bool m_bOnChecklistPage;
	bool m_bOnSPSkillsPage;

	u32 m_uSkillStatIndex;
	u32 m_uTimeSinceLastSkillSwitch;

	int m_iMouseScrollDirection;

	atMap<eMenuScreen, int> m_menuIDToSlotIndex;
};

#endif // __STATS_MENU_H__

