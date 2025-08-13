//
// name:        marketplace_offer.h
// description: Represents an offer in the marketplace.
//
//
// written by: @RockNorth
//


// --- Include Files ------------------------------------------------------------

// C headers
#if __XENON
#include "system/xtl.h"
#elif __PPU
#include <cell/sysmodule.h>
#include <sysutil/sysutil_bgdl.h>
#include <np/commerce2.h>
#endif // __XENON

// Rage headers
#include "string/string.h"
#include "diag/channel.h"
#include "diag/seh.h"
#include "diag/output.h"
#include "rline/rldiag.h"
#include "rline/rl.h"

// Game headers
#include "marketplace_offer.h"

namespace rage
{

// --- Defines ------------------------------------------------------------------

#ifdef getErrorName
#undef getErrorName
#undef getDownloadState
#endif
#if __PPU
#define getErrorName(x) x == static_cast <int> (CELL_SYSUTIL_ERROR_STATUS) ? "CELL_SYSUTIL_ERROR_STATUS" : ::rage::rlNpGetErrorString(x)
#define getDownloadState(x) \
						(x == CELL_BGDL_STATE_ERROR ? "Abnormally terminated because an error occurred" : \
						(x == CELL_BGDL_STATE_PAUSE ? "Paused (by user operation, connection disconnection, etc.)" : \
						(x == CELL_BGDL_STATE_READY ? "Ready" : \
						(x == CELL_BGDL_STATE_RUN   ? "Downloading" : \
						(x == CELL_BGDL_STATE_COMPLETE ? "Download completed " : "UNKNOWN")))))
#elif __XENON
#define getErrorName(x) rlXOnlineGetErrorString(x)
#define getDownloadState(x) \
						(x == ERROR_NOT_FOUND ? "download manager doesn't have the Offer ID yet" : \
						(x == ERROR_IO_PENDING ? "download is in progress" : \
						(x == ERROR_SUCCESS ? "download has completed " : "UNKNOWN")))
#endif // __PPU


#if __XENON
CompileTimeAssert(OfferInfo::STORE_PRODUCT_NAME_LEN == XMARKETPLACE_MAX_OFFER_NAME_LENGTH);
CompileTimeAssert(OfferInfo::STORE_PRODUCT_LONG_DESCRIPTION_LEN == XMARKETPLACE_MAX_OFFER_SELL_TEXT_LENGTH);
CompileTimeAssert(OfferInfo::STORE_CONTENT_ID_SIZE == XMARKETPLACE_CONTENT_ID_LEN);
CompileTimeAssert(sizeof(unsigned char)== sizeof(BYTE));
#elif __PPU
CompileTimeAssert(OfferInfo::STORE_CATEGORY_NAME_LEN             == SCE_NP_COMMERCE2_CATEGORY_NAME_LEN);
CompileTimeAssert(OfferInfo::STORE_PRODUCT_ID_LEN                == SCE_NP_COMMERCE2_PRODUCT_ID_LEN);
CompileTimeAssert(OfferInfo::STORE_PRODUCT_NAME_LEN              == SCE_NP_COMMERCE2_PRODUCT_NAME_LEN);
CompileTimeAssert(OfferInfo::STORE_PRODUCT_SHORT_DESCRIPTION_LEN == SCE_NP_COMMERCE2_PRODUCT_SHORT_DESCRIPTION_LEN);
CompileTimeAssert(OfferInfo::STORE_PRODUCT_LONG_DESCRIPTION_LEN  == SCE_NP_COMMERCE2_PRODUCT_LONG_DESCRIPTION_LEN);
CompileTimeAssert(OfferInfo::STORE_SKU_ID_LEN                    == SCE_NP_COMMERCE2_SKU_ID_LEN);
//CompileTimeAssert(STORE_CURRENCY_CODE_LEN             == SCE_NP_COMMERCE2_CURRENCY_CODE_LEN);
//CompileTimeAssert(STORE_CURRENCY_SYMBOL_LEN           == SCE_NP_COMMERCE2_CURRENCY_SYMBOL_LEN);
//CompileTimeAssert(STORE_THOUSAND_SEPARATOR_LEN        == SCE_NP_COMMERCE2_THOUSAND_SEPARATOR_LEN);
//CompileTimeAssert(STORE_DECIMAL_LETTER_LEN            == SCE_NP_COMMERCE2_DECIMAL_LETTER_LEN);
//CompileTimeAssert(STORE_SP_NAME_LEN                   == SCE_NP_COMMERCE2_SP_NAME_LEN);
//CompileTimeAssert(STORE_CATEGORY_ID_LEN               == SCE_NP_COMMERCE2_CATEGORY_ID_LEN);
//CompileTimeAssert(STORE_CATEGORY_DESCRIPTION_LEN      == SCE_NP_COMMERCE2_CATEGORY_DESCRIPTION_LEN);
//CompileTimeAssert(STORE_SKU_NAME_LEN                  == SCE_NP_COMMERCE2_SKU_NAME_LEN);
//CompileTimeAssert(STORE_URL_LEN                       == SCE_NP_COMMERCE2_URL_LEN);
//CompileTimeAssert(STORE_RATING_SYSTEM_ID_LEN          == SCE_NP_COMMERCE2_RATING_SYSTEM_ID_LEN);
//CompileTimeAssert(STORE_RATING_DESCRIPTION_LEN        == SCE_NP_COMMERCE2_RATING_DESCRIPTION_LEN);
#endif // __XENON


// --- OfferInfo ----------------------------------------------

OfferInfo::OfferInfo()
: m_Price(0),
  m_Instaled(false),
  m_Purchased(false),
  m_Downloadable(false),
#if __XENON
  m_OfferId(0),
  m_AssetID(0),
  m_CategoryId(0),
  m_Type(0),
  m_LicenseMask(0),
#endif // __XENON
  m_Next(NULL)
{
	memset(m_Name, 0, STORE_PRODUCT_NAME_LEN);

#if __XENON
	memset(m_ContentId, 0, STORE_CONTENT_ID_SIZE);
	memset(m_Description, 0, STORE_PRODUCT_LONG_DESCRIPTION_LEN);
#elif __PPU
	memset(m_ShortDesc, 0, STORE_PRODUCT_SHORT_DESCRIPTION_LEN);
	memset(m_LongDesc, 0, STORE_PRODUCT_LONG_DESCRIPTION_LEN);
	memset(m_OfferId, 0, STORE_PRODUCT_ID_LEN);
	memset(m_SkuId, 0, STORE_SKU_ID_LEN);
	memset(m_CategoryId, 0, STORE_CATEGORY_NAME_LEN);
#endif // __XENON
}


OfferInfo::~OfferInfo()
{
	Clear();
}

void
OfferInfo::Copy(const OfferInfo& offer)
{
	m_Instaled = offer.m_Instaled;
	m_Purchased = offer.m_Purchased;
	m_Downloadable = offer.m_Downloadable;
	m_Price = offer.m_Price;

	sysMemCpy(&m_Name, &offer.m_Name, STORE_PRODUCT_NAME_LEN);

#if __XENON
	m_OfferId = offer.m_OfferId;
	m_AssetID = offer.m_AssetID;
	m_CategoryId = offer.m_CategoryId;
	m_Type = offer.m_Type;
	m_LicenseMask = offer.m_LicenseMask;
	sysMemCpy(&m_Description, &offer.m_Description, STORE_PRODUCT_LONG_DESCRIPTION_LEN);
	sysMemCpy(&m_ContentId, &offer.m_ContentId, STORE_CONTENT_ID_SIZE);
#elif __PPU
	sysMemCpy(&m_ShortDesc, &offer.m_ShortDesc, STORE_PRODUCT_SHORT_DESCRIPTION_LEN);
	sysMemCpy(&m_LongDesc, &offer.m_LongDesc, STORE_PRODUCT_LONG_DESCRIPTION_LEN);
	sysMemCpy(&m_OfferId, &offer.m_OfferId, STORE_PRODUCT_ID_LEN);
	sysMemCpy(&m_SkuId, &offer.m_SkuId, STORE_SKU_ID_LEN);
	sysMemCpy(&m_CategoryId, &offer.m_CategoryId, STORE_CATEGORY_NAME_LEN);
#endif // __XENON
}

void
OfferInfo::Clear()
{
	m_Price = 0;
	m_Purchased = false;
	m_Downloadable = false;
	m_Instaled = false;

	memset(m_Name, 0, STORE_PRODUCT_NAME_LEN);

#if __XENON
	m_OfferId = 0;
	m_AssetID = 0;
	m_CategoryId = 0;
	m_Type = 0;
	m_LicenseMask = 0;
	memset(m_Description, 0, STORE_PRODUCT_LONG_DESCRIPTION_LEN);
	memset(m_ContentId, 0, STORE_CONTENT_ID_SIZE);
#elif __PPU
	memset(m_ShortDesc, 0, STORE_PRODUCT_SHORT_DESCRIPTION_LEN);
	memset(m_LongDesc, 0, STORE_PRODUCT_LONG_DESCRIPTION_LEN);
	memset(m_OfferId, 0, STORE_PRODUCT_ID_LEN);
	memset(m_SkuId, 0, STORE_SKU_ID_LEN);
	memset(m_CategoryId, 0, STORE_CATEGORY_NAME_LEN);
#endif // __XENON
}

void
OfferInfo::SetName(const char* name)
{
	if(rlVerify(name) && rlVerify(0 < strlen(name) && STORE_PRODUCT_NAME_LEN > strlen(name)))
	{
		formatf(m_Name, STORE_PRODUCT_NAME_LEN, name);
	}
}

#if __XENON

void
OfferInfo::SetDescription(const char* desc)
{
	if(rlVerify(desc) && rlVerify(0 < strlen(desc) && STORE_PRODUCT_LONG_DESCRIPTION_LEN > strlen(desc)))
	{
		formatf(m_Description, STORE_PRODUCT_LONG_DESCRIPTION_LEN, desc);
	}
}

void
OfferInfo::SetContentId(const unsigned char* contentId)
{
	if (rlVerify(contentId))
	{
		sysMemCpy(&m_ContentId, &contentId, STORE_CONTENT_ID_SIZE);
	}
}

u64
OfferInfo::GetDownloadStatus(const int localGamerIndex) const
{
	bool returnVar = false;

	DWORD downloadResult = 0;
	DWORD pdwResult;
	returnVar = (ERROR_SUCCESS == (pdwResult = XMarketplaceGetDownloadStatus(localGamerIndex, m_OfferId, &downloadResult)));
	Assertf(returnVar, "XMarketplaceGetDownloadStatus:: FAILED code=\"0x%08x\".", pdwResult);

	rlDebug3("------------------------------------------------------------");
	rlDebug3("offers \"%" I64FMT "d\"", m_OfferId);
	rlDebug3("...... Download state: \"%s\"", getDownloadState(downloadResult));
	rlDebug3("------------------------------------------------------------");

	return downloadResult;
}


#elif __PPU

void
OfferInfo::SetShortDesc(const char* desc)
{
	if(rlVerify(desc) && rlVerify(0 < strlen(desc) && STORE_PRODUCT_SHORT_DESCRIPTION_LEN > strlen(desc)))
	{
		formatf(m_ShortDesc, STORE_PRODUCT_SHORT_DESCRIPTION_LEN, desc);
	}
}

void
OfferInfo::SetLongDesc(const char* desc)
{
	if(rlVerify(desc) && rlVerify(0 < strlen(desc) && STORE_PRODUCT_LONG_DESCRIPTION_LEN > strlen(desc)))
	{
		formatf(m_LongDesc, STORE_PRODUCT_LONG_DESCRIPTION_LEN, desc);
	}
}

void
OfferInfo::SetOfferId(const char* id)
{
	if(rlVerify(id) && rlVerify(0 < strlen(id) && STORE_PRODUCT_ID_LEN > strlen(id)))
	{
		formatf(m_OfferId, STORE_PRODUCT_ID_LEN, id);
	}
}

void
OfferInfo::SetSkuId(const char* id)
{
	if(rlVerify(id) && rlVerify(0 < strlen(id) && STORE_SKU_ID_LEN > strlen(id)))
	{
		formatf(m_SkuId, STORE_SKU_ID_LEN, id);
	}
}

void
OfferInfo::SetCategoryId(const char* id)
{
	if(rlVerify(id) && rlVerify(0 < strlen(id) && STORE_CATEGORY_NAME_LEN > strlen(id)))
	{
		formatf(m_CategoryId, STORE_CATEGORY_NAME_LEN, id);
	}
}

int
OfferInfo::GetDownloadStatus( ) const
{
	int errorCode = 0;

	rtry
	{
		if(cellSysmoduleIsLoaded(CELL_SYSMODULE_BGDL) != CELL_OK)
		{
			int errorCode = cellSysmoduleLoadModule(CELL_SYSMODULE_BGDL);
			rcheck(CELL_OK == errorCode, catchall, rlError("cellSysmoduleLoadModule \"CELL_SYSMODULE_BGDL\":: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode)));
		}

		CellBGDLInfo info;

		char offerId[256];
		memset(offerId, 0, 256);
		if (rlVerify(strlen(m_CategoryId) + strlen(m_OfferId) < 256))
		{
			strcat(offerId, m_CategoryId);
			strcat(offerId, m_OfferId);
		}

		errorCode = cellBGDLGetInfo(offerId, &info, 1);
		rcheck(0 <= errorCode, catchall, rlError("cellBGDLGetInfo::Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode)));

		rlDebug3("------------------------------------------------------------");
		rlDebug3("offers \"%s\"", offerId);
		rlDebug3("...... Download state: \"%s\"", getDownloadState(static_cast <int> (info.state)));
		rlDebug3("....... Received Size: \"%" I64FMT "d\"", info.received_size);
		rlDebug3("........ Content Size: \"%" I64FMT "d\"", info.content_size);
		rlDebug3(".......... Percentage: \"%" I64FMT "d%%\"", (100*info.received_size)/info.content_size);
		rlDebug3("------------------------------------------------------------");

		if(cellSysmoduleIsLoaded(CELL_SYSMODULE_BGDL) == CELL_OK)
		{
			int errorCode = cellSysmoduleUnloadModule(CELL_SYSMODULE_BGDL);
			rcheck(CELL_OK == errorCode, catchall, rlError("cellSysmoduleUnloadModule \"CELL_SYSMODULE_BGDL\":: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode)));
		}

		return static_cast <int> (info.state);
	}
	rcatchall
	{
	}

	return errorCode;
}

#endif // __XENON

} // namespace rage

// EOF
