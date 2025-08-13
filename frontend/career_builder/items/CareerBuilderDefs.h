/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerBuilderDefs.h
// PURPOSE : Common definitions for Career Builder UI functionality.
//
// AUTHOR  : stephen.phillips
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CAREER_BUILDER_DEFS
#define CAREER_BUILDER_DEFS

// framework
#include "fwutil/Gen9Settings.h"
#include "streaming/streamingrequest.h"

// rage
#include "atl/array.h"
#include "atl/binmap.h"

#if UI_PAGE_DECK_ENABLED

class CCareerBuilderCategory;
class CPageItemBase;

namespace CareerBuilderDefs
{
	typedef rage::atArray<CCareerBuilderCategory*>  CategoryCollection;
	typedef CategoryCollection::iterator            CategoryIterator;
	typedef CategoryCollection::const_iterator      ConstCategoryIterator;

	typedef rage::atArray<CPageItemBase*>   ItemCollection;
	typedef ItemCollection::iterator        ItemIterator;
	typedef ItemCollection::const_iterator  ConstItemIterator;

	typedef atBinaryMap<strRequest, atHashString> TxdRequestCollection;

	enum class CategoryType
	{
		CareerSelection,
		ShoppingCartTab,
		ShoppingCartSummary
	};
}

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_BUILDER_DEFS
