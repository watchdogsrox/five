#ifndef __KEYMAPPING_MENU_H__
#define __KEYMAPPING_MENU_H__

#include "CMenuBase.h"

#include "system/control.h"

#if KEYBOARD_MOUSE_SUPPORT
class CKeyMappingMenu : public CMenuBase
{
public:
	CKeyMappingMenu(CMenuScreen& owner);
	~CKeyMappingMenu();

	virtual void Update();
	virtual bool UpdateInput(s32 eInput);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId);
	virtual void LoseFocus();
	virtual bool CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);
	virtual void AdjustScrollbar(bool UNUSED_PARAM(bAreWe));
	virtual bool InitScrollbar();
	virtual bool ShouldBlockCloseAttempt(const char*& rpWarningMessageToDisplay, bool& bCanContinue);

#if __BANK
	virtual void AddWidgets(bkBank* ToThisBank);
#endif
private:
	int AdjustWarningIndicators(bool bIsUpdate = true);
	void UpdateCategory(const atHashWithStringNotFinal& uCategory, bool bIsUpdate);

	void SendControlToScaleform(InputType input, int iSlotIndex, bool bIsUpdate, int slotBinding);

	bool GetInputForCurrentSelection(InputType& outInput, u8& outMappingSlot, int* pOutCurSelectionIndex = NULL, int* pOutSlotBinding = NULL) const;
	bool IsInputBound(InputType input, u8 mappingSlot) const;

	void SendInput(InputType input, u8 slot);
	void SetInSubmenu(bool bInIt);

	void AcceptBackingOut();
	void AcceptConflict();
	void IgnoreConflict();
	void AcceptAutosave();
	void RejectAutosave();
	void AcceptRestoreDefaults();

	CControl::InputList			m_conflictingInputs;
	CControl::InputList			m_unmappedInputs;
	atHashWithStringNotFinal	m_curCategory;
	InputType					m_curMappingInput;
	InputType					m_lastConflict;
	int							m_iKeysInCategory;
	int							m_iCurBindingIndex;
	int							m_iCurHighlightedId;
	bool						m_bInSubMenu;
	bool						m_bTouchedAControl;

	static bool					sm_bAutoResolveConflicts;
#if __BANK
	bkGroup*					m_pMyGroup;
#endif
};

#endif // KEYBOARD_MOUSE_SUPPORT

#endif // __KEYMAPPING_MENU_H__