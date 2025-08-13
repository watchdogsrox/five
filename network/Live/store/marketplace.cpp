
// --- Include Files ------------------------------------------------------------

// C headers
#if __XENON
#include "system/xtl.h"
#elif __PPU
#include <cell/http.h>
#include <np/commerce2.h>
#endif // __XENON

// Rage headers
#include "net/status.h"
#include "diag/seh.h"
#include "diag/channel.h"
#include "diag/output.h"
#include "system/param.h"
#include "system/nelem.h"
#include "rline/rldiag.h"

// network headers
#include "marketplace.h"
#include "marketplace_offer.h"
#include "marketplace_iterator.h"
#if __XENON
#include "marketplace_asset.h"
#elif __PPU
#include "marketplace_category.h"
#endif // __XENON

namespace rage
{
RAGE_DEFINE_SUBCHANNEL(rline, store);
#undef __rage_channel
#define __rage_channel rline_store

#if __XENON
#define getOfferType(x) \
						(x == OFFER_TYPE_CONTENT      ? "CONTENT" : \
						(x == OFFER_TYPE_GAME_DEMO    ? "GAME_DEMO" : \
						(x == OFFER_TYPE_GAME_TRAILER ? "GAME_TRAILER" : \
						(x == OFFER_TYPE_THEME        ? "THEME" : \
						(x == OFFER_TYPE_TILE         ? "TILE" : \
						(x == OFFER_TYPE_ARCADE       ? "ARCADE" : \
						(x == OFFER_TYPE_VIDEO        ? "VIDEO" : \
						(x == OFFER_TYPE_CONSUMABLE   ? "CONSUMABLE" : "UNKNOWN"))))))))
#endif // __XENON

#if __XENON
CompileTimeAssert(rlMarketplace::OFFER_TYPE_CONTENT      == XMARKETPLACE_OFFERING_TYPE_CONTENT);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_GAME_DEMO    == XMARKETPLACE_OFFERING_TYPE_GAME_DEMO);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_GAME_TRAILER == XMARKETPLACE_OFFERING_TYPE_GAME_TRAILER);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_THEME        == XMARKETPLACE_OFFERING_TYPE_THEME);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_TILE         == XMARKETPLACE_OFFERING_TYPE_TILE);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_ARCADE       == XMARKETPLACE_OFFERING_TYPE_ARCADE);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_VIDEO        == XMARKETPLACE_OFFERING_TYPE_VIDEO);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_CONSUMABLE   == XMARKETPLACE_OFFERING_TYPE_CONSUMABLE);
CompileTimeAssert(rlMarketplace::OFFER_TYPE_CONSUMABLE   == XMARKETPLACE_OFFERING_TYPE_CONSUMABLE);
CompileTimeAssert(sizeof(sMarketOverlapped) == sizeof(XOVERLAPPED));
CompileTimeAssert(sizeof(sMarketContentOfferInfo) == sizeof(XMARKETPLACE_CONTENTOFFER_INFO));
CompileTimeAssert(sizeof(sMarketPlaceAsset) == sizeof(XMARKETPLACE_ASSET));
CompileTimeAssert(sizeof(sMarketPlaceAssetPackage) == sizeof(XMARKETPLACE_ASSET_PACKAGE));
CompileTimeAssert(sizeof(sMarketPlaceAssetEnumerateReply) == sizeof(XMARKETPLACE_ASSET_ENUMERATE_REPLY));
CompileTimeAssert(sizeof(sMarketOfferingContentAvailableResult) == sizeof(XOFFERING_CONTENTAVAILABLE_RESULT));
CompileTimeAssert(sizeof(sMarketFiletime) == sizeof(FILETIME));
CompileTimeAssert(CONTENT_ID_LEN == XMARKETPLACE_CONTENT_ID_LEN);
CompileTimeAssert(ASSET_SIGNATURE_SIZE == XMARKETPLACE_ASSET_SIGNATURE_SIZE);
#elif __PPU
CompileTimeAssert(rlMarketplace::MAX_TEXT_LENGTH == CategoryInfo::MAX_TEXT_LENGTH);
#endif // __PPU

// --- Constants ----------------------------------------------------------------


// --- rlMarketplace ----------------------------------------------

const char* rlMarketplace::GetOperationName(const Operations OUTPUT_ONLY(s))
{
#if !__NO_OUTPUT
	static const char* s_Names[] =
	{
		"OP_IDLE",
		"OP_REQUEST",
#if __PPU
		"OP_CREATE_SESSION",
		"OP_CHECKOUT",
		"OP_REDOWNLOAD",
#endif // __PPU
	};

	CompileTimeAssert(COUNTOF(s_Names) == OP_TOTAL_NUMBER);

	return (AssertVerify(s >= 0) && AssertVerify(s < OP_TOTAL_NUMBER)) ? s_Names[s] : "OPERATION_UNKNOWN";
#else
	return "";
#endif // __NO_OUTPUT
}

const char* rlMarketplace::GetRequestName(const RequestType OUTPUT_ONLY(s))
{
#if !__NO_OUTPUT

	static const char* s_Names[] =
	{
		"REQ_GET_OFFER",
#if __XENON
		"REQ_GET_ASSET",
		"REQ_CONSUME_ASSET",
		"REQ_GET_COUNTS",
#elif __PPU
		"REQ_GET_CATEGORY",
#endif // __XENON
	};

	CompileTimeAssert(COUNTOF(s_Names) == REQ_TOTAL_NUMBER);

	return (AssertVerify(s >= 0) && AssertVerify(s < REQ_TOTAL_NUMBER)) ? s_Names[s] : "REQUEST_UNKNOWN";
#else
	return "";
#endif // __NO_OUTPUT
}

rlMarketplace::~rlMarketplace()
{
	if (m_MyStatus.Pending())
	{
		Cancel();
	}
	m_MyStatus.Reset();

	Shutdown();
}

bool
rlMarketplace::Pending() const
{
	return m_MyStatus.Pending() || OP_IDLE != m_Operation;
}

bool
rlMarketplace::Failed() const
{
	return m_MyStatus.Failed();
}

bool
rlMarketplace::Succeeded() const
{
	return m_MyStatus.Succeeded();
}

bool
rlMarketplace::Canceled() const
{
	return m_MyStatus.Canceled();
}

rlMarketplace::Operations
rlMarketplace::GetStatus() const
{
	return m_Operation;
}

rlMarketplace::Operations
rlMarketplace::GetLastStatus() const
{
	return m_LastOperation;
}

rlMarketplace::RequestType
rlMarketplace::GetRequestType() const
{
	return m_Type;
}

bool
rlMarketplace::EnumerateOffers(const int XENON_ONLY(localGamerIndex), const bool PPU_ONLY(offersOnly))
{
	bool ret = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("EnumerateOffers:: FAILED - Pending operations"));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("EnumerateOffers:: FAILED - On going another operation."));
		rcheck(m_IsInitialed, catchall, rlError("EnumerateOffers:: FAILED - Not initialized"));

#if __XENON
		m_LocalGamerIndex = localGamerIndex;
#elif __PPU
		rcheck(m_SessionCreated, catchall, rlError("EnumerateOffers:: FAILED - Not initialized"));
#endif // __PPU

		ret = NativeEnumerateOffers(WIN32_ONLY(false)PPU_ONLY(offersOnly));
		rcheck(ret, catchall, rlError("NativeEnumerateOffers:: FAILED"));
	}
	rcatchall
	{
	}

	return ret;
}

bool
rlMarketplace::EnumerateAssets(const int XENON_ONLY(localGamerIndex))
{
	bool ret = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("EnumerateAssets:: FAILED - Pending operations"));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("EnumerateAssets:: FAILED - On going another operation."));
		rcheck(m_IsInitialed, catchall, rlError("EnumerateAssets:: FAILED - Not initialized"));

#if __XENON
		m_LocalGamerIndex = localGamerIndex;
#endif // __XENON

		ret = NativeEnumerateAssets();
		rcheck(ret, catchall, rlError("NativeEnumerateAssets:: FAILED"));
	}
	rcatchall
	{
	}

	return ret;
}

unsigned
rlMarketplace::GetOffersCount() const
{
	unsigned uCount = 0;

	rlMarketplaceOfferConstIt it = GetConstOfferIterator();
	for(const OfferInfo* offer = it.Current(); offer; offer = it.Next())
	{
		uCount++;
	}

	return uCount;
}

rlMarketplaceOfferIt
rlMarketplace::GetOfferIterator()
{
	return rlMarketplaceOfferIt(m_Offers);
}

rlMarketplaceOfferConstIt
rlMarketplace::GetConstOfferIterator() const
{
	return rlMarketplaceOfferConstIt(const_cast<OfferInfo*>(m_Offers));
}

#if __XENON

unsigned
rlMarketplace::GetAssetsCount() const
{
	unsigned uCount = 0;

	rlMarketplaceAssetConstIt it = GetConstAssetIterator();
	for(const AssetInfo* asset = it.Current(); asset; asset = it.Next())
	{
		uCount++;
	}

	return uCount;
}

rlMarketplaceAssetIt
rlMarketplace::GetAssetIterator()
{
	return rlMarketplaceAssetIt(m_Assets);
}

rlMarketplaceAssetConstIt
rlMarketplace::GetConstAssetIterator() const
{
	return rlMarketplaceAssetConstIt(const_cast<AssetInfo*>(m_Assets));
}

bool
rlMarketplace::ConsumeAssets(const int localGamerIndex,
							const u32* assetId,
							const unsigned* quantity,
							const unsigned assetsCount)
{
	bool ret = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("ConsumeAssets:: FAILED - Pending operations"));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("ConsumeAssets:: FAILED - On going another operation."));
		rcheck(m_IsInitialed, catchall, rlError("ConsumeAssets:: FAILED - Not initialized"));

#if __XENON
		m_LocalGamerIndex = localGamerIndex;
#endif // __XENON

		XMARKETPLACE_ASSET* AssetIds = rage_new XMARKETPLACE_ASSET[assetsCount];
		for (unsigned i=0; i<assetsCount; i++)
		{
			bool found = false;

			rlMarketplaceAssetConstIt it = GetConstAssetIterator();
			for(const AssetInfo* asset = it.Current(); asset && !found; asset = it.Next())
			{
				if (asset->GetAssetId() == assetId[i] && rlVerify(quantity[i] <= asset->GetQuantity()))
				{
					AssetIds[i].dwAssetID  = assetId[i];
					AssetIds[i].dwQuantity = quantity[i];

					found = true;
				}
			}

			rcheck(found, catchall, rlError("Unknown asset \"%u\"", assetId[i]));
		}

		ret = NativeConsumeAssets((sMarketPlaceAsset*)AssetIds);

		delete [] AssetIds;

		rcheck(ret, catchall, rlError("NativeConsumeAssets:: FAILED"));
	}
	rcatchall
	{
	}

	return ret;
}

bool
rlMarketplace::RetrieveCachedContent(const int localGamerIndex)
{
	bool ret = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("RetrieveCachedContent:: FAILED - Pending operations"));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("RetrieveCachedContent:: FAILED - On going another operation."));
		rcheck(m_IsInitialed, catchall, rlError("RetrieveCachedContent:: FAILED - Not initialized"));

#if __XENON
		m_LocalGamerIndex = localGamerIndex;
#endif // __XENON

		ret = NativeRetrieveCachedContent();
		rcheck(ret, catchall, rlError("NativeRetrieveCachedContent:: FAILED"));
	}
	rcatchall
	{
	}

	return ret;
}

unsigned
rlMarketplace::GetTotalCachedContentCount()
{
	return m_CachedContentCount.dwTotalOffers;
}

unsigned
rlMarketplace::GetNewCachedContentCount() const
{
	return m_CachedContentCount.dwNewOffers;
}

#elif __PPU

unsigned
rlMarketplace::GetCategoryCount() const
{
	unsigned uCount = 0;

	rlMarketplaceCategoryConstIt it = GetConstCategoryIterator();
	for(const CategoryInfo* category = it.Current(); category; category = it.Next())
	{
		uCount++;
	}

	return uCount;
}

rlMarketplaceCategoryIt
rlMarketplace::GetCategoryIterator()
{
	return rlMarketplaceCategoryIt(m_Categories);
}

rlMarketplaceCategoryConstIt
rlMarketplace::GetConstCategoryIterator() const
{
	return rlMarketplaceCategoryConstIt(const_cast<CategoryInfo*>(m_Categories));
}

#endif // __PPU

// --- Private ----------------------------------------------------

void
rlMarketplace::ClearOffers()
{
	OfferInfo* offer = m_Offers;
	OfferInfo** offerNext = &m_Offers;
	while (offer)
	{
		*offerNext = offer->Next();
#if __XENON
		rlDebug3("...... Removing offer name:\"%s\" Id: \"%u\"", offer->GetName(), offer->GetOfferId());
#elif __PPU
		rlDebug3("...... Removing offer name:\"%s\" Id: \"%s\"", offer->GetName(), offer->GetOfferId());
#endif // __XENON
		delete offer;
		offer = *offerNext;
	}
}

unsigned
rlMarketplace::GetNumDownloadableOffers() const
{
	unsigned uCount = 0;

	rlMarketplaceOfferConstIt it = GetConstOfferIterator();
	for(const OfferInfo* offer = it.Current(); offer; offer = it.Next())
	{
		if (offer->GetIsDownloadable())
		{
			uCount++;
		}
	}

	return uCount;
}

unsigned
rlMarketplace::GetNumBuyableOffers() const
{
	unsigned uCount = 0;

	rlMarketplaceOfferConstIt it = GetConstOfferIterator();
	for(const OfferInfo* offer = it.Current(); offer; offer = it.Next())
	{
		if (offer->GetIsDownloadable() && !offer->GetWasPurchased())
		{
			uCount++;
		}
	}

	return uCount;
}

void
rlMarketplace::AddOffer(OfferInfo* offer)
{
	if (rlVerify(offer))
	{
		offer->m_Next = m_Offers;
		m_Offers = offer;
	}
}

#if __XENON

void
rlMarketplace::AddAsset(AssetInfo* asset)
{
	if (rlVerify(asset))
	{
		asset->m_Next = m_Assets;
		m_Assets = asset;
	}
}

void
rlMarketplace::ClearAssets()
{
	AssetInfo* asset = m_Assets;
	AssetInfo** assetNext = &m_Assets;
	while (asset)
	{
		*assetNext = asset->Next();
		rlDebug3("...... Removing asset Id: \"%u\"", asset->GetAssetId());
		delete asset;
		asset = *assetNext;
	}
}

bool
rlMarketplace::OfferExists(const u64 id) const
{
	rlMarketplaceOfferConstIt it = GetConstOfferIterator();
	for(const OfferInfo* offer = it.Current(); offer; offer = it.Next())
	{
		if (offer->GetOfferId() == id)
		{
			return true;
		}
	}

	return false;
}

bool
rlMarketplace::AssetExists(const u32 id) const
{
	rlMarketplaceAssetConstIt it = GetConstAssetIterator();
	for(const AssetInfo* asset = it.Current(); asset; asset = it.Next())
	{
		if (asset->GetAssetId() == id)
		{
			return true;
		}
	}

	return false;
}

void
rlMarketplace::SpewAssetsInfo() const
{
	rlDebug3("----------------------------------------------");
	rlDebug3("Assets Info:");
	rlDebug3("....... Total Number \"%u\"", GetAssetsCount());
	rlDebug3("----------------------------------------------");

	int count = 0;
	rlMarketplaceAssetConstIt it = GetConstAssetIterator();
	for(const AssetInfo* asset = it.Current(); asset; asset = it.Next())
	{
		rlDebug3(" ");
		rlDebug3("........... Index: \"%d\"", count);
		rlDebug3("........ Asset Id: \"%u\"", asset->GetAssetId());
		rlDebug3("..... Category Id: \"%u\"", asset->GetQuantity());
		count++;
	}

	rlDebug3("----------------------------------------------");
}

#elif __PPU

void
rlMarketplace::ClearCategories()
{
	CategoryInfo* pCategory = m_Categories;
	CategoryInfo** pCategoryNext = &m_Categories;
	while (pCategory)
	{
		*pCategoryNext = pCategory->Next();
		rlDebug3("...... Removing Category Id: \"%s\"", pCategory->GetId());
		delete pCategory;
		pCategory = *pCategoryNext;
	}
}

void  rlMarketplace::AddCategory(CategoryInfo* pCategory)
{
	if (rlVerify(pCategory))
	{
		pCategory->m_Next = m_Categories;
		m_Categories = pCategory;
	}
}

bool
rlMarketplace::OfferExists(const char* id) const
{
	rlMarketplaceOfferConstIt it = GetConstOfferIterator();
	for(const OfferInfo* offer = it.Current(); offer; offer = it.Next())
	{
		if (0 == strcmp(id, offer->GetOfferId()))
		{
			return true;
		}
	}

	return false;
}

bool
rlMarketplace::CategoryExists(const char* id) const
{
	rlMarketplaceCategoryConstIt it = GetConstCategoryIterator();
	for(const CategoryInfo* category = it.Current(); category; category = it.Next())
	{
		if (0 == strcmp(id, category->GetId()))
		{
			return true;
		}
	}

	return false;
}

#endif // __XENON

void
rlMarketplace::SpewOffersInfo() const
{
	rlDebug3("----------------------------------------------");
	rlDebug3("Offers Info:");
	rlDebug3("....... Total Number \"%u\"", GetOffersCount());
	rlDebug3("----------------------------------------------");

	int count=0;
	rlMarketplaceOfferConstIt it = GetConstOfferIterator();
	for(const OfferInfo* offer=it.Current(); offer; offer=it.Next(), count++)
	{
		rlDebug3(" ");
		rlDebug3("........... Index: \"%d\"", count);
		rlDebug3("............ Name: \"%s\"", offer->GetName());
		rlDebug3("........ Instaled: \"%s\"", offer->GetIsInstaled() ? "TRUE" : "FALSE");
		rlDebug3("....... Purchased: \"%s\"", offer->GetWasPurchased() ? "TRUE" : "FALSE");
		rlDebug3(".... Downloadable: \"%s\"", offer->GetIsDownloadable() ? "TRUE" : "FALSE");
		rlDebug3("........... Price: \"%u\"", offer->GetPrice());
#if __XENON
		rlDebug3("........ Offer Id: \"0x%08" I64FMT "x\"", offer->GetOfferId());
		rlDebug3("........ Asset Id: \"%u\"", offer->GetAssetId());
		rlDebug3("..... Category Id: \"%u\"", offer->GetCategoryId());
		rlDebug3("...... Offer Type: \"%s\"", getOfferType(offer->GetCategoryId()));
		rlDebug3(".... License Mask Id: \"%u\"", offer->GetLicenseMaskId());
		rlDebug3("............ Desc: \"%s\"", offer->GetDescription());
#elif __PPU
		rlDebug3("........ Offer Id: \"%s\"", offer->GetOfferId());
		rlDebug3(".......... SKU Id: \"%s\"", offer->GetSkuId());
		rlDebug3("..... Category Id: \"%s\"", offer->GetCategoryId());
		rlDebug3("...... Short Desc: \"%s\"", offer->GetShortDesc());
		rlDebug3("....... Long Desc: \"%s\"", offer->GetLongDesc());
#endif // __XENON
	}

	rlDebug3("----------------------------------------------");
}

#if __WIN32PC || RSG_DURANGO		// Stubs for PC for now.

rlMarketplace::rlMarketplace() { }

void rlMarketplace::Shutdown() { }

void rlMarketplace::Update() { Assert(false); }

bool rlMarketplace::NativeEnumerateOffers(bool) { Assert(false); return false; }

bool rlMarketplace::NativeEnumerateAssets(void) { Assert(false); return false; }

bool rlMarketplace::Cancel(void) { Assert(false); return false; }

#endif	// __WIN32PC

}   //namespace rage

// EOF
