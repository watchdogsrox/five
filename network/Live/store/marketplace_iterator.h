//
// marketplace_iterators.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETPLACE_ITERATORS_H
#define MARKETPLACE_ITERATORS_H

// --- Include Files ------------------------------------------------------------

#include "marketplace_offer.h"
#if __XENON
#include "marketplace_asset.h"
#elif __PPU
#include "marketplace_category.h"
#endif // __XENON

namespace rage
{
	//PURPOSE: Offers iterator.
	class rlMarketplaceOfferIt
	{
		friend class rlMarketplace;

	public:
		rlMarketplaceOfferIt()
			: m_Current(NULL)
		{
		}

		OfferInfo* Next()
		{
			m_Current = m_Current->Next() ? m_Current->Next() : NULL;
			return m_Current;
		}

		OfferInfo* Current()
		{
			return m_Current;
		}

	private:
		rlMarketplaceOfferIt(OfferInfo* cur)
			: m_Current(cur)
		{
		}

		OfferInfo* m_Current;
	};

	//PURPOSE: Const Offers iterator.
	class rlMarketplaceOfferConstIt
	{
		friend class rlMarketplace;

	public:
		rlMarketplaceOfferConstIt()
			: m_Current(NULL)
		{
		}

		const OfferInfo* Next()
		{
			m_Current = m_Current->Next() ? m_Current->Next() : NULL;
			return m_Current;
		}

		const OfferInfo* Current()
		{
			return m_Current;
		}

	private:
		rlMarketplaceOfferConstIt(const OfferInfo* cur)
			: m_Current(cur)
		{
		}

		const OfferInfo* m_Current;
	};

#if __XENON

	//PURPOSE: Offers iterator.
	class rlMarketplaceAssetIt
	{
		friend class rlMarketplace;

	public:
		rlMarketplaceAssetIt()
			: m_Current(NULL)
		{
		}

		AssetInfo* Next()
		{
			m_Current = m_Current->Next() ? m_Current->Next() : NULL;
			return m_Current;
		}

		AssetInfo* Current()
		{
			return m_Current;
		}

	private:
		rlMarketplaceAssetIt(AssetInfo* cur)
			: m_Current(cur)
		{
		}

		AssetInfo* m_Current;
	};

	//PURPOSE: Const Assets iterator.
	class rlMarketplaceAssetConstIt
	{
		friend class rlMarketplace;

	public:

		rlMarketplaceAssetConstIt()
			: m_Current(NULL)
		{
		}

		const AssetInfo* Next()
		{
			m_Current = m_Current->Next() ? m_Current->Next() : NULL;
			return m_Current;
		}

		const AssetInfo* Current()
		{
			return m_Current;
		}

	private:
		rlMarketplaceAssetConstIt(const AssetInfo* cur)
			: m_Current(cur)
		{
		}

		const AssetInfo* m_Current;
	};

#elif __PPU

	//PURPOSE: Categorys iterator.
	class rlMarketplaceCategoryIt
	{
		friend class rlMarketplace;

	public:
		rlMarketplaceCategoryIt()
			: m_Current(NULL)
		{
		}

		CategoryInfo* Next()
		{
			m_Current = m_Current->Next() ? m_Current->Next() : NULL;
			return m_Current;
		}

		CategoryInfo* Current()
		{
			return m_Current;
		}

	private:
		rlMarketplaceCategoryIt(CategoryInfo* cur)
			: m_Current(cur)
		{
		}

		CategoryInfo* m_Current;
	};

	//PURPOSE: Const Categorys iterator.
	class rlMarketplaceCategoryConstIt
	{
		friend class rlMarketplace;

	public:
		rlMarketplaceCategoryConstIt()
			: m_Current(NULL)
		{
		}

		const CategoryInfo* Next()
		{
			m_Current = m_Current->Next() ? m_Current->Next() : NULL;
			return m_Current;
		}

		const CategoryInfo* Current()
		{
			return m_Current;
		}

	private:
		rlMarketplaceCategoryConstIt(const CategoryInfo* cur)
			: m_Current(cur)
		{
		}

		const CategoryInfo* m_Current;
	};

#endif

} // namespace rage

#endif // MARKETPLACE_ITERATORS_H

// EOF
