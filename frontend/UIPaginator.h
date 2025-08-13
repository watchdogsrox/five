#ifndef __UI_PAGINATOR_H__
#define __UI_PAGINATOR_H__

#include "atl/array.h"
#include "atl/pool.h"

#include "rline/clan/rlclancommon.h"
#include "Frontend/PauseMenuData.h" // for scrollflags
#include "Frontend/CMenuBase.h" // MenuScreenId
#include "data/callback.h"

//////////////////////////////////////////////////////////////////////////
//		A class for paginating data from a large superset
//		into smaller, bite-sized chunks which can then, in turn
//		be displayed in a much smaller UI list

//		Example: Leaderboard entries.
//			Data set: 100,000+ rows, obviously too big for memory
//			Data page: 96 rows, nice and good for fetches
//			UI Window: 16 rows at a time

//		LIMITATION: Your data page size should be a factor of your UI window,
//		because cross-datapage wrapping is haaaaaaard

//////////////////////////////////////////////////////////////////////////

/*
 // To create your own pagination class, you need to do the following four (4) things:

 // 1. Define a derived DataPage, having at least these functions
	class MyDataPage : public UIDataPageBase 
	{
		public:
			DECLARE_UI_DATA_PAGE(MyDataPage, 30); // or whatever data size you want

			// whatever information each row needs to collect its data
			bool SetData(UniqueDataPerColumn data)
			{
				// using GetPagedIndex(), we can determine what data to collect
				TakeColumnDataAndGetOurPortionOfIt(data, GetPagedIndex());
				return true;
			}

			// see UIDataPageBase for more information on these
			virtual bool IsReady() const { return m_Data.Valid(); }
			virtual int GetSize() const  { return m_Data.GetSize(); }

			virtual bool FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn )
			{
				// take iItemIndex, determine what we need from it, and pass it on
				if( IsIndexGoodToShow(iItemIndex) )
				{
					const char* pszShowThis = m_Data[iItemIndex];
					CPauseMenu::SET_DATA_SLOT(iColumn, iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, OPTION_DISPLAY_STYLE_SLIDER, false, true, pszShowThis); 
					return true;
				}

				return false;
			}

		private:
			UniqueData m_Data;
		
	};
		
// 2. In your .cpp, call:
	IMPLEMENT_UI_DATA_PAGE(MyDataPage);


// 3. Create your paginator class like so:
class MyDataPaginator : public UIPaginator<MyDataPage>
{
	public:
		// Called on creation to set your instance apart from the others
		void Init(UniqueDataPerColumn myData, int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex)
		{
			// IMPORTANT: set our data FIRST before calling InitInternal
			m_MyData = myData;


			InitInternal(VisibleItemsPerPage, ColumnId, iColumnIndex);
		}

		// imparts the UniqueDataPerColumn onto a given datapage
		virtual bool SetDataOnPage(UIDataPageBase* thisPage)
		{
			MyDataPage* pPage = verify_cast<MyDataPage*>(thisPage);
			if( pPage )
			{
				return pPage->SetData(m_MyData);
			}
			return false;
		}

	private:
		UniqueDataPerColumn m_MyData;
};

// 4. HOOK IT ALL UP
// In your MenuPage, call these (assuming MyDataPaginator m_PaginatorInstance exists in the class definition)
MenuPage::MenuPage()
{
	m_PaginatorInstance.Init(SomeData, MaxPerUI, MENU_UNIQUE_ID_WHATEVER, WhichColumn);
}

void MenuPage::UpdateInput(s32 eInput)
{
	// checks if LT/RT has been pressed
	if( m_PaginatorInstance.UpdateInput(eInput) )
		return; // handled!
}

void MenuPage::Populate()
{
	// assuming the first item in our menu is the paginator
	// the false is because on populate, our menus tend not to have focus
	// and as such, shouldn't redraw
	m_PaginatorInstance.UpdateItemSelection(0,false);
}

void MenuPage::LayoutChanged( MenuScreenId, MenuScreenId iNewLayout, s32 iUniqueId )
{
	// if we're navigating the menu
	if( m_PaginatorInstance.GetMenuScreenId() == iNewLayout )
	{
		// the true indicates that this menu is being navigated
		// and such, will need its highlight redrawn afterwards
		m_PaginatorInstance.UpdateItemSelection(iUniqueId, true);
	}
	else
	{
		// we're navigating some OTHER screen, so lose focus on that menu
		m_PaginatorInstace.LoseFocus();
	}
}

void MenuPage::Update()
{
	// necessary if your data takes time to process (such as net requests)
	m_PaginatorInstance.Update();
}

*/
//////////////////////////////////////////////////////////////////////////

namespace UIPage {
	typedef int	PageIndex;		// human-notioned pages, [0,infinity)
	typedef int DataIndex;		// item index from 0 to however large the dataset is; is NOT the internal
	
	typedef int LocalDataIndex; // index of data in local, datapage-space
	typedef int LocalPageIndex;	// internal array index of a datapage
};

using namespace UIPage;

// forward declarations
class UIPaginatorBase;
class CMenuScreen;


class UIDataPageBase
{
public:
	UIDataPageBase(const UIPaginatorBase* const pOwner, PageIndex pageIndex);
	virtual ~UIDataPageBase() {};

	// REQUIRE DEFINITION IN THE DERIVED CLASSES
	// <BEGIN READER EXERCISE>

	// returns true when the data is ready (downloaded, processed, whatever)
	virtual bool IsReady() const = 0;
	virtual bool HasFailed() const {return false;} // SHOULD probably be pure virtual, but I don't want to have to implement it everywhere
	virtual int GetResultCode() const { return 0; } // same as above; should return a code
	virtual void Update(){};

	// returns the actual number of entries
	virtual int GetSize() const = 0; // for this page
	virtual int GetTotalSize() const = 0; // for ALL pages

	// Fill out scaleform, via CPauseMenu::SET_DATA_SLOT
	// iItemIndex - Index of data, [0, GetSize() )
	// iUIIndex - Row index of UI  [0, UIWindowSize )
	// ColumnId - MenuIdentifier of what to call this row
	// iUniqueId - Total index in dataset [0, n)
	// iColumn - which column to SET_DATA_SLOT on.
	virtual bool FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate ) = 0;
	virtual bool FillEmptyScaleformSlots( LocalDataIndex UNUSED_PARAM(iItemIndex), int UNUSED_PARAM(iSlotIndex), MenuScreenId UNUSED_PARAM(menuId),
											DataIndex UNUSED_PARAM(playerIndex), int UNUSED_PARAM(iColumn) ) {return true;}
	// <END READER EXERCISE>

	// REGULAR FUNCTIONS requiring no interaction

	virtual int GetMaxSizeVirtual() const = 0;

	virtual bool CanHighlightSlot() const {return true;}
	virtual void SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex);

public:
	void LoseFocus();
	virtual void FillScaleformBase(int iColumn, MenuScreenId ColumnId, DataIndex iStartingIndex, int iMaxPerPage, bool bHasFocus);
	virtual void UpdateIndex(int iColumn, MenuScreenId ColumnId, DataIndex item);

	PageIndex GetPagedIndex() const		{ return m_PagedIndex; }
	DataIndex GetLastShownIndex() const	{ return m_LastShownIndex; }
	DataIndex GetLowestEntry() const	{ return m_PagedIndex*GetMaxSizeVirtual(); }
	int		  GetLastShownItemCount() const { return m_iLastShownItemCount; }
	
protected:
	const UIPaginatorBase* const m_pPageOwner;

private:
	PageIndex	m_PagedIndex;
	DataIndex	m_LastShownIndex;
	int			m_iLastShownItemCount;
};

class UIPaginatorBase : public datBase
{
public:
	UIPaginatorBase(const CMenuScreen* const pOwner);
	virtual ~UIPaginatorBase() {};

	static const int DEFAULT_MAX_PAGES = 5;

	virtual void UpdateItemSelection(DataIndex iNewIndex, bool bHasFocus);
	
	virtual void PageUp();
	virtual void PageDown();
	virtual void ItemUp();
	virtual void ItemDown();

	void LoseFocus();

	virtual bool UpdateInput(s32 eInput);
	virtual void Update(bool bAllowedToDraw = true);

	bool IsReady() const { return GetActivePage() && GetActivePage()->IsReady(); }
	bool HasFailed() const { return GetActivePage() && GetActivePage()->HasFailed(); }
	int GetResultCode() const { return GetActivePage() ? GetActivePage()->GetResultCode() : -1; }
	bool HasFocus() const { return m_bGainedFocus; }
	int GetTotalSize() const { return (GetActivePage() && GetActivePage()->IsReady()) ? GetActivePage()->GetTotalSize() : 0; }

	DataIndex GetCurrentHighlight() const {return m_iLastHighlight; }

	virtual void MoveToIndex(s32 UNUSED_PARAM(iIndex)) {}
	void UpdateIndex(DataIndex iItem){ if(GetActivePage()) GetActivePage()->UpdateIndex(m_iColumnIndex, m_ColumnId, iItem); }

	MenuScreenId GetMenuScreenId() const		{ return m_ColumnId; }
	const UIDataPageBase* GetActivePage() const { return GetPagePtr(0); }
		  UIDataPageBase* GetActivePage()		{ return GetPagePtr(0); }
	const CMenuScreen* const GetOwner() const;
	void SetDelayedOwner(MenuScreenId newOwner) { m_DelayedOwner = newOwner; m_pOwner = NULL; }
	int GetNumVisibleItemsPerPage() const {return m_VisibleItemsPerPage;}

	void SetShowHideCB(const datCallback& callWhenHidden)		{ m_ShowHideCB = callWhenHidden; }
	void SetNoResultsCB(const datCallback& callWhenNoResults)	{ m_HandleNoResultsCB = callWhenNoResults; }

	// derived class must call shutdownbase AND delete its own pool
	virtual void Shutdown() = 0;

	// blows away any pages that have been created
	void ResetPages();
protected:
	// must be called by derived class!
	void InitInternal( int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex, u8 MaxCachedPages = DEFAULT_MAX_PAGES, int iBusyColumnIndex = -1 );
	
	// must be set up on derived class
	virtual bool SetDataOnPage( UIDataPageBase* setForThisPage) = 0;


	// these are handled by the template.
	virtual void InitPools(u8 MaxCachedPages) = 0;
	virtual void SetCurrentPool() = 0;
	virtual bool IsPoolFull() const = 0;
	virtual int GetDataPageMaxSize() const = 0;
	virtual UIDataPageBase* CreatePage(PageIndex iPage) = 0;
	virtual bool HasBeenInitialized() const = 0;

	void ShutdownBase();
	

		  UIDataPageBase* FindPageContaining(DataIndex iItem);
	const UIDataPageBase* FindPageContaining(DataIndex iItem) const 
		{ return const_cast<const UIDataPageBase*>(const_cast<UIPaginatorBase*>(this)->FindPageContaining(iItem)); }
private: 

	UIDataPageBase* RequestPage(PageIndex iPageIndex );
	UIDataPageBase* RequestNewPage(PageIndex iPageIndex);
	
	// performs the necessary index->pool entry lookup
	const UIDataPageBase* GetPagePtr(LocalPageIndex index) const	{ return index < m_ActivePages.GetCount() ? m_ActivePages[index] : NULL;}
		  UIDataPageBase* GetPagePtr(LocalPageIndex index)			{ return index < m_ActivePages.GetCount() ? m_ActivePages[index] : NULL;}
	

	void SetActivePage(UIDataPageBase* pNewPage);

	bool ShowBusy( bool bHow );
	bool ShowNoResults( bool bNoZults );
protected:
	// protected for use in the template
	typedef rage::atArray<UIDataPageBase*>		PageWindow;
	PageWindow					m_ActivePages;
	mutable const CMenuScreen*	m_pOwner;

	DataIndex m_iLastHighlight;
	DataIndex m_iQueuedHighlight;
	int m_VisibleItemsPerPage;
	int m_iColumnIndex;

	
private:


	MenuScreenId	m_ColumnId;
	MenuScreenId	m_DelayedOwner;
	datCallback		m_ShowHideCB;
	datCallback		m_HandleNoResultsCB;

	int m_iBusyColumnIndex;
	bool m_bShowedBusy;
	bool m_bGainedFocus;
	bool m_bNoResults;
};


template <class _Type>
class UIPaginator : public UIPaginatorBase
{
private:
	typedef rage::atPool<_Type>		DataPool;
	DataPool*		m_PagePool;

	virtual _Type* CreatePage(PageIndex iPage ) { return rage_new _Type(this, iPage); }

public:
	UIPaginator(const CMenuScreen* const pOwner) : UIPaginatorBase(pOwner), m_PagePool(NULL) {};
	virtual ~UIPaginator() { if( m_PagePool ) delete m_PagePool; }

	virtual bool HasBeenInitialized() const { return m_PagePool != NULL; }

	virtual void SetCurrentPool()	{ _Type::SetCurrentPool(m_PagePool); }
	virtual bool IsPoolFull() const	{ return m_PagePool && m_PagePool->IsFull(); }
	virtual int GetDataPageMaxSize() const { return _Type::GetMaxSize(); }

	virtual void InitPools( u8 MaxCachedPages )
	{
		if( !HasBeenInitialized() )
		{
			m_PagePool = rage_new DataPool;
			m_PagePool->Init( MaxCachedPages );

		}
	}

	virtual void Shutdown()
	{
		UIPaginatorBase::ShutdownBase();
		if( HasBeenInitialized() )
		{
			delete m_PagePool;
			m_PagePool = NULL;
		}
	}
	
};

#define DECLARE_UI_DATA_PAGE(ClassName, MaxDataSize)\
	public:\
	static const int DATA_PAGE_SIZE = MaxDataSize;\
	virtual int GetMaxSizeVirtual()  const { return DATA_PAGE_SIZE; }\
	static int GetMaxSize() { return DATA_PAGE_SIZE; }\
	DECLARE_CLASS_NEW_DELETE(ClassName);


#define IMPLEMENT_UI_DATA_PAGE(ClassName) \
	IMPLEMENT_CLASS_NEW_DELETE(ClassName, #ClassName);

#endif // __UI_PAGINATOR_H__

