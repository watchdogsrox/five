#ifndef __CMENUBASE_H__
#define __CMENUBASE_H__

#include "fwtl/regdrefs.h"
#include "parser/macros.h"
#include "atl/hashstring.h"
#include "atl/array.h"
#include "frontend/UIContexts.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/Scaleform/ScaleformMgr.h"
#include "system/control_mapping.h"
#include "PauseMenuData.h"
#include "Text/TextFile.h"

// Forward declarations
namespace rage
{
	class parElement;
	class parTreeNode;
	class bkBank;
};
class CMenuScreen;
class CContextMenu;
class GFxValue;
struct PauseMenuRenderDataExtra;

enum eLAYOUT_CHANGED_DIR
{
	LAYOUT_CHANGED_DIR_BACKWARDS = -1,
	LAYOUT_CHANGED_DIR_UPDOWN = 0,
	LAYOUT_CHANGED_DIR_FORWARDS = 1
};



#define PREF_OPTIONS_THRESHOLD	(1000)
#define MAX_MENU_COLUMNS (8)

/// Wrapper to handle a directly enumerated list (non-extensible), or a hashed name (extensible)
class MenuScreenId
{
private:
	union { // here to facilitate easier debugging
		eMenuScreen m_asMenuScreen;
		s32 m_iValue;
	};

public:
	// creation methods
	MenuScreenId()						: m_iValue(MENU_UNIQUE_ID_INVALID) {};
	MenuScreenId(eMenuScreen newValue)	: m_iValue(newValue) {};
explicit MenuScreenId(s32 newValue)		: m_iValue(newValue) {}; 

	void SetFromXML(const char* str);

	// accessor/users
	s32  GetValue()		const					{ return  m_iValue; }
	s32* GetValuePtr()							{ return &m_iValue; }
	s32 operator+ ( const s32 rhs ) const		{ return  m_iValue+rhs;}

	const bool operator==(const eMenuScreen rhs) const { return m_iValue == static_cast<s32>(rhs); }
	const bool operator!=(const eMenuScreen rhs) const { return m_iValue != static_cast<s32>(rhs); }

	const bool operator==(const MenuScreenId& rhs) const { return m_iValue == rhs.m_iValue; }
	const bool operator!=(const MenuScreenId& rhs) const { return m_iValue != rhs.m_iValue; }

//	operator s32() const	{ return m_iValue; }

	// returns a human-readable name [if available]
	const char* GetParserName() const;
};


class CMenuButton
{
public:
	CMenuButton();

#if __BANK
	static int sm_Count;
	static int sm_Allocs;
	~CMenuButton();
#endif
	void SET_DATA_SLOT( int slotIndex, CScaleformMovieWrapper& instructionButtons );
	void preLoad(parTreeNode* parNode);

	UIContextList			m_ContextHashes;
	InputType				m_ButtonInput;
	InputGroup				m_ButtonInputGroup;

	// NOTES:	Only use for icons that are not gamepad buttons such as the loading spinner!
	eInstructionButtons		m_RawButtonIcon;

	atHashWithStringBank	m_hButtonHash;

	// PURPOSE:	Indicates that the button should be mouse clickable.
	bool					m_Clickable;

	bool CanShow() const { return m_ContextHashes.CheckIfPasses(); }

	// sorts buttons into consistent order
	static int PrioritySortReverse(const CMenuButton* a, const CMenuButton* b) { return PrioritySort(b,a); }
	static int PrioritySort(const CMenuButton* a, const CMenuButton* b);

	PAR_SIMPLE_PARSABLE;
};


class CMenuButtonList
{
	atArray<CMenuButton> m_ButtonPrompts;
	int m_WrappingPoint;

	// copies everything except the array, because that'll get parsed afterwards and ruin our day
	void CopyBitsFrom(CMenuButtonList& otherObject)
	{
		m_WrappingPoint = otherObject.m_WrappingPoint;
	}

public:
	CMenuButtonList() 
	{
		Reset();
#if __BANK
		++sm_Count;
		sm_Allocs += sizeof(CMenuButtonList);
#endif
	};

#if __BANK
	static int sm_Count;
	static int sm_Allocs;
	~CMenuButtonList()
	{
		--sm_Count;
		sm_Allocs -= sizeof(CMenuButtonList);
	}
#endif

	void Reset()
	{
		m_ButtonPrompts.Reset();
		m_WrappingPoint = 0;
	}


	void Draw(CScaleformMovieWrapper* pOverride = NULL);
	void Clear(CScaleformMovieWrapper* pOverride = NULL);
	void preLoad(parTreeNode* pTree);
	void postLoad();

	void Add(const CMenuButton& button) { m_ButtonPrompts.Grow() = button; }
	const bool empty() const { return m_ButtonPrompts.empty();}

	PAR_SIMPLE_PARSABLE;
};


//Very simple base class to provide entry points for extended functionality
class CMenuBase : public fwRefAwareBase
{
public:
	// on construction the element containing the type= attribute is passed in'
	// this allows optional other parameters to be teased out.
	CMenuBase( CMenuScreen& owner );
	virtual ~CMenuBase() {
#if __BANK
		--sm_Count;
		sm_Allocs -= sizeof(CMenuBase);
#endif
	};

	// default actions. Called no matter what, and expect nothing
	virtual void Init() {};
	virtual void LoseFocus() {};
	virtual void Update() {};
	virtual void Render(const PauseMenuRenderDataExtra* UNUSED_PARAM(renderData)) {};
	virtual void LayoutChanged( MenuScreenId UNUSED_PARAM(iPreviousLayout), MenuScreenId UNUSED_PARAM(iNewLayout), s32 UNUSED_PARAM(iUniqueId), eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) ) {};

	virtual void OnNavigatingContent(bool UNUSED_PARAM(bAreWe)) {};
	virtual void PrepareInstructionalButtons( MenuScreenId UNUSED_PARAM(MenuId), s32 UNUSED_PARAM(iUniqueId) ) {};
	
	virtual bool HandleContextOption(atHashWithStringBank UNUSED_PARAM(whichOption) ) {return false;}
	virtual void BuildContexts() {};


	// some screens may need more time to prepare themselves. This is for them
	virtual bool IsDoneLoading() const { return true; }
	

	// interrupts. If true is returned, no further action is performed
	virtual bool CheckIncomingFunctions(atHashWithStringBank UNUSED_PARAM(methodName), const GFxValue* UNUSED_PARAM(args)) {return false;}
	virtual bool UpdateInput(s32 UNUSED_PARAM(eInput)) { return false; }
	virtual bool TriggerEvent(MenuScreenId UNUSED_PARAM(MenuId), s32 UNUSED_PARAM(iUniqueId)) {return false;}
	virtual bool Populate(MenuScreenId UNUSED_PARAM(newScreenId)) {return false;}
	virtual bool InitScrollbar() { return false; }
	virtual bool ShouldBlockEntrance(MenuScreenId UNUSED_PARAM(iMenuScreenBeingFilledOut)) { return false; }
	virtual bool ShouldBlockCloseAttempt(const char*& UNUSED_PARAM(rpWarningMessageToDisplay), bool& UNUSED_PARAM(bCanContinue)) { return false; }

	virtual CContextMenu* GetContextMenu();
	virtual const CContextMenu* GetContextMenu() const;
	virtual void HandleContextMenuVisibility(bool UNUSED_PARAM(bIsVisibleOrNot)) { };

	void ShowColumnWarning(PM_COLUMNS startingColumn, int ColumnWidth, const char* bodyStringToShow, const char* titleStringToShow="", int ColumnPxHeightOverride=0, const char* txdStr ="", const char* imgStr = "", PM_WARNING_IMG_ALIGNMENT imgAlignment = IMG_ALIGN_NONE, const char* footerStr = "");
	void ShowColumnWarning_Loc(PM_COLUMNS startingColumn, int ColumnWidth, const char* bodyStringToShow, const char* titleStringToShow="", int ColumnPxHeightOverride=0, const char* txdStr ="", const char* imgStr = "", PM_WARNING_IMG_ALIGNMENT imgAlignment = IMG_ALIGN_NONE, const char* footerStr = "", bool bRequestTXD = false);

	// similar to the above version, but handles hiding your columns FOR you!
	void ShowColumnWarning_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, const char* titleStringToLoc, int ColumnPxHeightOverride = 0);
	void ShowColumnWarning_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, u32 uTitleStringToLoc);
	
	// hide the warning message (if shown using the three above)
	void ClearWarningColumn();
	void ClearWarningColumn_EX(PM_COLUMNS columnFields);
	virtual bool IsShowingWarningColumn() const {return m_hasWarning != 0;}
	virtual bool IsShowingFullWarningColumn() const {return m_hasWarning == 3;} // all columns (bits 1,2,4)

#if RSG_PC
	virtual void DeviceReset() {};
#endif
	
#if __BANK
	static int sm_Count;
	static int sm_Allocs;
	// NOTE: If widgets are added, ENSURE you kill them off in your destructor!
	virtual void AddWidgets(bkBank* UNUSED_PARAM(ToThisBank)) {};
#endif
	CMenuScreen& GetOwner() const { return m_Owner; }
	MenuScreenId GetMenuScreenId() const;
	virtual MenuScreenId GetUncorrectedMenuScreenId() const {return GetMenuScreenId();}

	virtual bool ShouldPlayNavigationSound(bool UNUSED_PARAM(navUp)) {return true;}

	void SHOW_COLUMN(PM_COLUMNS column, bool bVisible);
protected:

	void ClearColumnVisibilities();
	void ClearColumnDisplayed();

	void SET_DATA_SLOT_EMPTY(PM_COLUMNS column);
	void DISPLAY_DATA_SLOT(PM_COLUMNS column);

	void HandleColumns(PM_COLUMNS& whichColumns, bool bShowOrHide, int& width);

	void SHOW_WARNING_MESSAGE_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, u32 uTitleStringToLoc);
	void SHOW_WARNING_MESSAGE_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, const char* titleStringToLoc, int ColumnPxHeightOverride = 0);
	void HIDE_WARNING_MESSAGE_EX(PM_COLUMNS columnsAffected);

	CMenuScreen& m_Owner;
	int m_hasWarning;
	atFixedBitSet8 m_columnVisibility;
	atFixedBitSet8 m_columnDisplayed;
};

typedef fwRegdRef<CMenuBase>  MenuBaseRef;


#endif // __CMENUBASE_H__

