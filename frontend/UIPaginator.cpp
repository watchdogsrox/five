#include "ButtonEnum.h"
#include "Frontend/UIPaginator.h"
#include "frontend/ScaleformMenuHelper.h"
#include "Network/NetworkInterface.h"
#include "rline/clan/rlclan.h"
#include "frontend/PauseMenu.h"
#include "frontend/ui_channel.h"
#include "frontend/MousePointer.h"

#if __BANK
#include "bank/bank.h"
#endif

using namespace rage;
using namespace UIPage;

FRONTEND_MENU_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#define PAGINATOR_INVALID_INDEX (-1)

UIDataPageBase::UIDataPageBase(const UIPaginatorBase* const pOwner, PageIndex pageIndex)
: m_pPageOwner(pOwner)
, m_PagedIndex(pageIndex)
, m_LastShownIndex(PAGINATOR_INVALID_INDEX)
, m_iLastShownItemCount(0)
{
	 
}


void UIDataPageBase::LoseFocus()
{
	m_LastShownIndex = PAGINATOR_INVALID_INDEX;
	m_iLastShownItemCount = 0;
}

void UIDataPageBase::FillScaleformBase(int iColumn, MenuScreenId ColumnId, DataIndex iStartingIndex, int iMaxPerPage, bool bHasFocus)
{
	playerlistDebugf1("[UIDataPageBase] UIDataPageBase::FillScaleformBase");

	if( !IsReady() )
		return;

	PM_COLUMNS pmc = static_cast<PM_COLUMNS>(iColumn);

	m_iLastShownItemCount = 0;
	DataIndex pageBase = GetLowestEntry();
	// adjust iStartingIndex from data-space to local-space
	LocalDataIndex LocalIndex = iStartingIndex - pageBase;

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(pmc);
	for(LocalDataIndex i=LocalIndex; m_iLastShownItemCount < iMaxPerPage && i < GetSize(); ++i)
	{
		playerlistDebugf1("[UIDataPageBase] UIDataPageBase::FillScaleformBase - Calling FillScaleformEntry - m_iLastShownItemCount = %d ", m_iLastShownItemCount);
		if( FillScaleformEntry(i, m_iLastShownItemCount, ColumnId, i+pageBase, iColumn, false) )
			++m_iLastShownItemCount;
	}

	if(m_iLastShownItemCount < iMaxPerPage)
	{
		int lastEmptyShown = m_iLastShownItemCount;
		for(LocalDataIndex i=LocalIndex + lastEmptyShown; lastEmptyShown < iMaxPerPage; ++i)
		{
			playerlistDebugf1("[UIDataPageBase] UIDataPageBase::FillScaleformBase - Calling FillScaleformEntry - lastEmptyShown = %d ", lastEmptyShown);
			if(FillEmptyScaleformSlots(i, lastEmptyShown, ColumnId, i+pageBase, iColumn))
				++lastEmptyShown;
		}
	}

	CScaleformMenuHelper::DISPLAY_DATA_SLOT(pmc);

	if( bHasFocus )
	{
		SetSelectedItem(iColumn, 0, iStartingIndex);

		CScaleformMenuHelper::SET_COLUMN_FOCUS(pmc, true);
	}

	m_LastShownIndex = iStartingIndex;
}

void UIDataPageBase::UpdateIndex(int iColumn, MenuScreenId ColumnId, DataIndex item)
{
	LocalDataIndex LocalIndex = item - GetLowestEntry();
	int onScreenIndex = item - GetLastShownIndex();

	FillScaleformEntry(LocalIndex, onScreenIndex, ColumnId, item, iColumn, true);
}

void UIDataPageBase::SetSelectedItem(int iColumn, int iSlotIndex, DataIndex UNUSED_PARAM(playerIndex))
{
	PM_COLUMNS pmc = static_cast<PM_COLUMNS>(iColumn);

	if(CanHighlightSlot() && m_pPageOwner->HasFocus())
	{
		CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(pmc, iSlotIndex, true);
	}

	int iMaxPerPage = m_pPageOwner->GetNumVisibleItemsPerPage();

	const CMenuScreen* const pOwner = m_pPageOwner->GetOwner();

	// guess if we've set it up at all
	if(!pOwner->m_ScrollBarFlags.AreAnySet())
		return;

	DataIndex totalResult = iSlotIndex + GetLastShownIndex();

	// not enough items in the set to bother with a scroll bar
	if( !m_pPageOwner->HasFocus() || (!pOwner->HasScrollFlag(ShowIfNotFull) && m_iLastShownItemCount + GetLastShownIndex() < iMaxPerPage ))
		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(pmc);

	else if( pOwner->HasScrollFlag(ShowAsPages) )
		CScaleformMenuHelper::SET_COLUMN_SCROLL(pmc, totalResult / iMaxPerPage, (GetTotalSize()/iMaxPerPage)+1 );

	else
		CScaleformMenuHelper::SET_COLUMN_SCROLL(pmc, totalResult, GetTotalSize());

}

//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
UIPaginatorBase::UIPaginatorBase(const CMenuScreen* const pOwner)
: m_pOwner(pOwner)
, m_VisibleItemsPerPage(0)
, m_iColumnIndex(0)
, m_iBusyColumnIndex(0)
, m_bShowedBusy(false)
, m_bGainedFocus(false)
, m_bNoResults(false)
, m_iLastHighlight(PAGINATOR_INVALID_INDEX)
, m_iQueuedHighlight(PAGINATOR_INVALID_INDEX)
, m_ColumnId(MENU_UNIQUE_ID_INVALID)
{

}

void UIPaginatorBase::InitInternal(int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex, u8 MaxCachedPages /*= DEFAULT_MAX_DATA_PAGES*/, int iBusyColumnIndex /*= -1*/ )
{
	m_VisibleItemsPerPage = VisibleItemsPerPage;
	m_ColumnId = ColumnId;
	m_iColumnIndex = iColumnIndex;
	m_iBusyColumnIndex = iBusyColumnIndex != -1 ? iBusyColumnIndex : m_iColumnIndex;

	if( !HasBeenInitialized() )
	{
		InitPools(MaxCachedPages+1); //+1 so we can always create one on initialization
	}

	ResetPages();

	if( m_ActivePages.GetCapacity() != MaxCachedPages )
	{
		m_ActivePages.Reset();
		m_ActivePages.Reserve(MaxCachedPages);
	}

	if( m_ActivePages.empty() )
	{
		for(int i=0; i < MaxCachedPages; ++i)
			m_ActivePages.Push(NULL);
	}
}

void UIPaginatorBase::ResetPages()
{
	for(int i=0; i < m_ActivePages.GetCount(); ++i)
	{
		if(m_ActivePages[i])
		{
			delete m_ActivePages[i];
			m_ActivePages[i] = NULL;
		}
	}
	m_bGainedFocus = false;
	m_bNoResults = false;
}

void UIPaginatorBase::ShutdownBase()
{
	SetCurrentPool();
	while( !m_ActivePages.empty() )
	{
		delete m_ActivePages.Pop();
	}
	m_iLastHighlight = PAGINATOR_INVALID_INDEX;
	m_iQueuedHighlight = PAGINATOR_INVALID_INDEX;
}

UIDataPageBase* UIPaginatorBase::FindPageContaining(DataIndex iItem)
{

	for(LocalPageIndex i=0; i < m_ActivePages.GetCount(); ++i)
	{
		UIDataPageBase* pCurPage = GetPagePtr(i);
		if( !pCurPage )
			continue;

		PageIndex curPageIndex = pCurPage->GetPagedIndex();
		DataIndex PageRangeMin = curPageIndex*GetDataPageMaxSize();
		DataIndex PageRangeMax = PageRangeMin+ (pCurPage->IsReady() ? pCurPage->GetSize() : GetDataPageMaxSize());
		if( PageRangeMin <= iItem && iItem < PageRangeMax ) // if we just requested a page
			return pCurPage;
	}

	return NULL;
}

bool UIPaginatorBase::UpdateInput(s32 eInput)
{
#if KEYBOARD_MOUSE_SUPPORT
	if(CMousePointer::IsMouseWheelDown())
	{
		ItemDown();
		CPauseMenu::PlayInputSound(FRONTEND_INPUT_DOWN);
	}

	if(CMousePointer::IsMouseWheelUp())
	{
		ItemUp();
		CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	switch(eInput)
	{
		case PAD_LEFTSHOULDER2:
			PageUp();
			return true;
		case PAD_RIGHTSHOULDER2:
			PageDown();
			return true;
		case PAD_DPADUP:
			ItemUp();
			return true;
		case PAD_DPADDOWN:
			ItemDown();
			return true;
		default:
			return false;
	}
}

// Surely this is defined somewhere...
int RoundDownToNearest(int iValue, int iRoundingValue)
{
	return (iValue/iRoundingValue)*iRoundingValue;
}

void UIPaginatorBase::PageUp()
{
	if( GetActivePage() && GetActivePage()->IsReady() )
	{
		int newIndex = RoundDownToNearest(m_iLastHighlight-1, m_VisibleItemsPerPage);
		if( newIndex != m_iLastHighlight )
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);
		UpdateItemSelection(newIndex, true);
	}
}

void UIPaginatorBase::PageDown()
{
	if( GetActivePage() && GetActivePage()->IsReady() )
	{
		int newIndex = RoundDownToNearest(m_iLastHighlight + 1 + m_VisibleItemsPerPage, m_VisibleItemsPerPage)-1;
		newIndex = Min(newIndex, GetActivePage()->GetTotalSize()-1);
		if( newIndex != m_iLastHighlight )
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_DOWN);
		UpdateItemSelection(newIndex, true);
	}
}

void UIPaginatorBase::ItemUp()
{
	if( GetActivePage() && GetActivePage()->IsReady() )
	{
		int newIndex = m_iLastHighlight - 1;
		if( m_iLastHighlight == 0)
			newIndex = GetActivePage()->GetTotalSize()-1;
		UpdateItemSelection(newIndex, true);
	}
}

void UIPaginatorBase::ItemDown()
{
	if( GetActivePage() && GetActivePage()->IsReady() )
	{
		int newIndex = m_iLastHighlight + 1;
		if( m_iLastHighlight == GetActivePage()->GetTotalSize()-1 )
			newIndex = 0;
		UpdateItemSelection(newIndex, true);
	}
}

void UIPaginatorBase::UpdateItemSelection(DataIndex iNewIndex, bool bHasFocus)
{

	playerlistDebugf1("[UIPaginatorBase] UIPaginatorBase::UpdateItemSelection");

	bool bGainFocusThisTime = (bHasFocus && m_bGainedFocus != bHasFocus);
	m_bGainedFocus = bHasFocus;
	UIDataPageBase* pActive = GetActivePage();

	if( pActive && pActive->IsReady()  )
	{
		// if NO results, just chill the fuck out and do nothing else
		if( pActive->GetTotalSize() == 0)
		{
			ShowNoResults(true);
			return;
		}
		ShowNoResults(false);
	}

	iNewIndex = Max(iNewIndex,0);

	// determine which page this new item is on
	UIDataPageBase* pWhichPage( FindPageContaining(iNewIndex) );

	// on none? request it
	if( !pWhichPage )
	{
		PageIndex dataPage = iNewIndex/GetDataPageMaxSize(); // assumes data is filled out contiguously... which it isn't?
		pWhichPage = RequestNewPage(dataPage);
		ShowBusy(true);
		// request could shift pages, so re-set the active one
		pActive = GetActivePage();
	}
	
	if( Verifyf(pWhichPage && pActive,"Couldn't find an initial active page" ) )
	{
		// shift the active page
		SetActivePage(pWhichPage);
		pActive = GetActivePage();

		if( Verifyf(pActive,"How do we STILL not have a valid active page!?") )
		{
			if( pActive->IsReady() )
			{
				// if NO results, just chill the fuck out and do nothing else
				if( pActive->GetTotalSize() == 0)
				{
					playerlistDebugf1("[UIPaginatorBase] UIPaginatorBase::UpdateItemSelection - pActive->GetTotalSize() == 0");
					ShowNoResults(true);
				}
				else
				{
					ShowNoResults(false);

					int bottom = pActive->GetLastShownIndex();
					int top = bottom + m_VisibleItemsPerPage-1;

					playerlistDebugf1("[UIPaginatorBase] UIPaginatorBase::UpdateItemSelection - bottom = %d, top = %d", bottom, top);
					if( bottom == -1 || iNewIndex < bottom || iNewIndex > top )
					{
						playerlistDebugf1("[UIPaginatorBase] UIPaginatorBase::UpdateItemSelection - iNewIndex = %d, m_VisibleItemsPerPage=%d", iNewIndex, m_VisibleItemsPerPage);
						pActive->FillScaleformBase(m_iColumnIndex, m_ColumnId, RoundDownToNearest(iNewIndex, m_VisibleItemsPerPage), m_VisibleItemsPerPage, bHasFocus);
					}
				}
			}
			else
			{
				m_iLastHighlight = PAGINATOR_INVALID_INDEX;
			}
		}
	}

	if( pActive && pActive->IsReady() )
	{
		if( iNewIndex != m_iLastHighlight || bGainFocusThisTime )
		{
			m_iLastHighlight = iNewIndex;
			pActive->SetSelectedItem(m_iColumnIndex, (m_iLastHighlight - pActive->GetLowestEntry()) % m_VisibleItemsPerPage, m_iLastHighlight);
		}
	}
	else
		m_iQueuedHighlight = iNewIndex;

}

void UIPaginatorBase::LoseFocus()
{
	ShowBusy(false);
	ShowNoResults(false);
	m_bGainedFocus = false;

	UIDataPageBase* pActive = GetActivePage();
	// no active page's been chosen? Bail
	if( !pActive )
		return;

	pActive->LoseFocus();
}

UIDataPageBase* UIPaginatorBase::RequestPage(PageIndex iPageIndex )
{
	UIDataPageBase* pNewPage = NULL;
	SetCurrentPool();

	Assertf( m_ActivePages.GetCount() == m_ActivePages.GetCapacity(), "Not sure what happened, but the activePages list should always be full (even if just of nulls)");
	if( !Verifyf( !IsPoolFull(), "The pool is full! How can that be!?") )
		return NULL;

	uiDisplayf("PAGINATOR REQUESTING A PAGE OF INDEX %d FOR %s.", iPageIndex, GetOwner() ? GetOwner()->MenuScreen.GetParserName(): "-Unknown-");

	// memory's free, initialize it
	pNewPage = CreatePage(iPageIndex);
	if( !Verifyf( pNewPage && SetDataOnPage(pNewPage), "Page #%i failed to initialize properly!", iPageIndex) )
	{
		delete pNewPage;
		return NULL;
	}
	return pNewPage;
}

UIDataPageBase* UIPaginatorBase::RequestNewPage(PageIndex iPageIndex)
{
	UIDataPageBase* pNewPage = RequestPage(iPageIndex);
	if( pNewPage )
	{
		SetActivePage(pNewPage);
	}
	
	return pNewPage;
}

const CMenuScreen* const UIPaginatorBase::GetOwner() const
{
	if( !m_pOwner )
		m_pOwner = &CPauseMenu::GetScreenData(m_DelayedOwner);
	return m_pOwner;
}

void UIPaginatorBase::Update(bool bAllowedToDraw /* = true */)
{
	UIDataPageBase* pActive = GetActivePage();
	// no active's been chosen? Bail
	if( !pActive )
		return;

	pActive->Update();

	// data's not ready? bail
	if( !pActive->IsReady() )
	{
		if( bAllowedToDraw )
			ShowBusy(true);
		return;
	}


	// we haven't had a chance to show this menu yet (must've been loading)
	if( bAllowedToDraw && m_iLastHighlight == PAGINATOR_INVALID_INDEX && (ShowBusy(false) || pActive->GetLastShownIndex() == PAGINATOR_INVALID_INDEX ))
	{
		UpdateItemSelection(m_iQueuedHighlight, m_bGainedFocus );
	}
}

void UIPaginatorBase::SetActivePage(UIDataPageBase* pNewTopPage)
{
	int index = m_ActivePages.Find(pNewTopPage);

	// not found? make room
	if( index == -1 )
	{
		// pop the oldest, least accessed page
		delete m_ActivePages.Top();
		index = m_ActivePages.GetCount()-1;
	}

	// move all the pages over. They're pointers, so this is 'cheap'.
	for(int i=index; i > 0; --i)
		m_ActivePages[i] = m_ActivePages[i-1];

	// before killing our old screen, have it lose focus
	if( pNewTopPage != m_ActivePages[0] && m_ActivePages[0] )
		m_ActivePages[0]->LoseFocus();


	// make this our new active page
	m_ActivePages[0] = pNewTopPage;
}

bool UIPaginatorBase::ShowBusy( bool bBusy )
{
	if(m_bShowedBusy != bBusy)
	{
		m_bShowedBusy = bBusy;

		if( bBusy )
		{
			PM_COLUMNS pmc = static_cast<PM_COLUMNS>(m_iColumnIndex);
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(pmc);
			m_ShowHideCB.Call( CallbackData(false) );
		}

		CScaleformMenuHelper::SET_BUSY_SPINNER(static_cast<PM_COLUMNS>(m_iBusyColumnIndex), bBusy);

		const CMenuScreen* const pOwner = GetOwner();

		if(bBusy && pOwner && pOwner->m_ScrollBarFlags.AreAnySet() )
			CScaleformMenuHelper::HIDE_COLUMN_SCROLL(static_cast<PM_COLUMNS>(pOwner->depth-1));

		return true;
	}
	return false;
}

bool UIPaginatorBase::ShowNoResults( bool bNoZults )
{
	ShowBusy(false);
	if( m_bNoResults != bNoZults )
	{
		m_bNoResults = bNoZults;
		m_HandleNoResultsCB.Call(CallbackData(bNoZults));
		return true;
	}
	return false;
}

// eof
