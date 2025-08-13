#if __XENON

// --- Include Files ------------------------------------------------------------

// C headers
#if __XENON
#include "system/xtl.h"
#endif // __XENON

// Rage headers
#include "rline/rl.h"
#include "diag/seh.h"
#include "net/status.h"
#include "net/netdiag.h"
#include "diag/channel.h"
#include "rline/rldiag.h"

// network headers
#include "marketplace.h"
#include "marketplace_offer.h"
#include "marketplace_asset.h"
#include "marketplace_iterator.h"

namespace rage
{

// Max number of offers that can be enumerate
#define MAX_OFFERS_ENUMERATED  XMARKETPLACE_MAX_OFFERS_ENUMERATED
// Max number of assets that can be enumerate
#define MAX_ASSETS_ENUMERATED  XMARKETPLACE_ASSET_MAX_ENUM_SIZE

#ifdef getErrorName
#undef getErrorName
#endif
#define getErrorName(x) rlXOnlineGetErrorString(x)


// --- Constants ----------------------------------------------------------------


// --- WideString_to_String ----------------------------------------------


//PURPOSE
//  Maps a wide-character string to a new character string.
bool
WideString_to_String(const WCHAR* stringScr, char* stringDest, const unsigned destSize)
{
	if (AssertVerify(stringScr) && AssertVerify(stringDest) && AssertVerify(0 < destSize))
	{
		const int strSize = lstrlenW((LPCWSTR)stringScr);
		unsigned srcSize = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)stringScr, strSize, NULL, 0, NULL, NULL);
		if (0 < srcSize)
		{
			srcSize = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)stringScr, strSize, stringDest, destSize, NULL, NULL);

			if (AssertVerify(srcSize <= destSize))
			{
				stringDest[srcSize] = '\0';
			}
			else
			{
				stringDest[destSize] = '\0';
			}

			return true;
		}
	}

	return false;
}


// --- XovOverlapped ----------------------------------------------

rlMarketplace::XovOverlapped::XovOverlapped()
: m_OverlappedResult(0),
  m_MyStatus(NULL)
{
	ZeroMemory( &m_Overlapped, sizeof(XOVERLAPPED) );
}

rlMarketplace::XovOverlapped::~XovOverlapped()
{
	Clear();
}

void
rlMarketplace::XovOverlapped::Clear()
{
	if (Pending())
		Cancel();

	m_OverlappedResult = 0;
	m_MyStatus = NULL;
	ZeroMemory(&m_Overlapped, sizeof(XOVERLAPPED));
}

void
rlMarketplace::XovOverlapped::Cancel( )
{
	if (rlVerify(Pending()))
	{
		XCancelOverlapped((PXOVERLAPPED)&m_Overlapped);
		//m_MyStatus->Cancel();
		m_MyStatus->SetCanceled();
		m_MyStatus = NULL;
	}
}

bool
rlMarketplace::XovOverlapped::Pending() const
{
	if (m_MyStatus)
		return (NULL != m_MyStatus && m_MyStatus->Pending());

	return false;
}

void
rlMarketplace::XovOverlapped::BeginOverlapped(netStatus* status)
{
	Assert(status);
	if (status)
	{
		m_MyStatus = status;
		ZeroMemory(&m_Overlapped, sizeof(XOVERLAPPED));
		m_Overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		Assert(m_Overlapped.hEvent);
		m_MyStatus->SetPending();
	}
}

void
rlMarketplace::XovOverlapped::EndOverlapped()
{
	Clear();
}

bool
rlMarketplace::XovOverlapped::CheckExtendedError()
{
	const u32 error_no_more_files = 0x80070012;
	DWORD dwStatus = XGetOverlappedExtendedError((PXOVERLAPPED)&m_Overlapped);
	if(ERROR_SUCCESS == dwStatus || ERROR_NO_MORE_FILES == dwStatus || error_no_more_files == dwStatus)
		return true;

	rlError("Extended overllaped Error (%x)", dwStatus);
	return false;
}

bool
rlMarketplace::XovOverlapped::CheckOverlapped()
{
	Assert(m_MyStatus);
	if (m_MyStatus && XHasOverlappedIoCompleted(&m_Overlapped))
	{
		DWORD dwRet = XGetOverlappedResult((PXOVERLAPPED)&m_Overlapped, (LPDWORD)&m_OverlappedResult, FALSE);
		if (ERROR_SUCCESS == dwRet || CheckExtendedError())
		{
			dwRet = ERROR_SUCCESS;
			m_MyStatus->SetSucceeded();
			return true;
		}
		else
		{
			rlError("Unexpected error (%d) during operation", dwRet);
			m_MyStatus->SetFailed();
		}
	}

	return false;
}

unsigned
rlMarketplace::XovOverlapped::GetOverlappedResult()
{
	return m_OverlappedResult;
}

sMarketOverlapped*
rlMarketplace::XovOverlapped::GetOverlapped()
{
	return &m_Overlapped;
}


// --- rlMarketplace ----------------------------------------------

rlMarketplace::rlMarketplace()
: m_IsInitialed(false),
  m_Offers(NULL),
  m_Operation(OP_IDLE),
  m_TopCategory(0),
  m_LocalGamerIndex(0),
  m_OfferData(NULL),
  m_AssetData(NULL),
  m_handleEvent(NULL),
  m_OfferType(0),
  m_Category(0),
  m_Type(REQ_GET_OFFER)
{
	m_MyStatus.Reset();
}

void
rlMarketplace::Init(const u32 offerType, const u32 categories)
{
	Assert(!m_IsInitialed);
	Assert(!m_Offers);
	Assert(!m_OfferData);
	Assert(!m_AssetData);
	Assert(!m_handleEvent);
	Assert(OP_IDLE == m_Operation);

	rtry
	{
		rlDebug2("Init Store Browsing");

		rcheck(AssertVerify(!m_IsInitialed), catchall, rlError("Init:: FAILED - Already initialized"));
		rcheck(AssertVerify(!m_Offers),     catchall, rlError("Init:: FAILED - "));
		rcheck(AssertVerify(!m_Assets),     catchall, rlError("Init:: FAILED - "));
		rcheck(AssertVerify(!m_OfferData),  catchall, rlError("Init:: FAILED - "));
		rcheck(AssertVerify(!m_AssetData),  catchall, rlError("Init:: FAILED - "));
		rcheck(AssertVerify(!m_handleEvent), catchall, rlError("Init:: FAILED - "));
		rcheck(AssertVerify(OP_IDLE == m_Operation), catchall, rlError("Init:: FAILED - "));

		m_IsInitialed = true;
		m_MyStatus.Reset();
		m_XovStatus.Clear();

		m_Operation = OP_IDLE;

		m_OfferType = offerType;
		rlDebug2("Type of offer to be enumerated = \"0x%08x\"", m_OfferType);

		m_Category = categories;
		rlDebug2("Category filter = \"0x%08x\"", m_Category);

		ClearOffers();
		SpewOffersInfo();

		ClearAssets();
		SpewAssetsInfo();
	}
	rcatchall
	{
		Shutdown();
	}
}

void
rlMarketplace::Shutdown()
{
	Assert(m_IsInitialed);
	Assert(!m_MyStatus.Pending());
	Assert(!m_XovStatus.Pending());

	m_IsInitialed = false;
	m_Operation = OP_IDLE;

	ClearOffers();
	SpewOffersInfo();

	ClearAssets();
	SpewAssetsInfo();

	if (m_MyStatus.Pending())
	{
		Cancel();
	}
	m_MyStatus.Reset();
	m_XovStatus.Clear();

	if (m_OfferData)
	{
		delete [] (BYTE*)m_OfferData;
		m_OfferData = NULL;
	}

	if (m_AssetData)
	{
		delete [] (BYTE*)m_AssetData;
		m_AssetData = NULL;
	}

	if (m_handleEvent)
	{
		CloseHandle(m_handleEvent);
		m_handleEvent = NULL;
	}
}

bool
rlMarketplace::Cancel()
{
	Assert(m_MyStatus.Pending());

	if (m_MyStatus.Pending())
	{
		m_XovStatus.Cancel();
	}

	return true;
}

void
rlMarketplace::Update()
{
	if (!m_IsInitialed)
		return;

	switch(m_MyStatus.GetStatus())
	{
	case netStatus::NET_STATUS_PENDING:
		if (m_XovStatus.CheckOverlapped())
		{
			m_LastOperation = m_Operation;

#define HANDLER_STATE_CASE(x) case x: rlDebug2("End Operation \""#x"\"");
#define HANDLER_REQ_CASE(x) case x: rlDebug2("Request Type \""#x"\"");
			switch (m_Operation)
			{
				HANDLER_STATE_CASE(OP_IDLE)
					rlError("Invalid state OP_IDLE");
				break;

				HANDLER_STATE_CASE(OP_REQUEST)
					switch (m_Type)
					{
						HANDLER_REQ_CASE(REQ_GET_OFFER)
							break;
						HANDLER_REQ_CASE(REQ_GET_ASSET)
							break;
						HANDLER_REQ_CASE(REQ_CONSUME_ASSET)
							break;
						HANDLER_REQ_CASE(REQ_GET_COUNTS)
							break;
						HANDLER_REQ_CASE(REQ_TOTAL_NUMBER)
							break;
					}
				break;

				HANDLER_STATE_CASE(OP_TOTAL_NUMBER)
				break;
			}
#undef HANDLER_STATE_CASE
#undef HANDLER_REQ_CASE

		}
	break;

	case netStatus::NET_STATUS_SUCCEEDED:
		{
#define HANDLER_STATE_CASE(x) case x: rlDebug2("Operation Succeeded \""#x"\"");
#define HANDLER_REQ_CASE(x) case x: rlDebug2("Request Type \""#x"\"");
			switch(static_cast<unsigned>(m_Operation))
			{
				HANDLER_STATE_CASE(OP_IDLE)
					rlError("Invalid state OP_IDLE");
				break;

				HANDLER_STATE_CASE(OP_REQUEST)
					switch (m_Type)
					{
						HANDLER_REQ_CASE(REQ_GET_OFFER)
							LoadNewOffers();
							rlDebug2("End offers enumeration");
						break;

						HANDLER_REQ_CASE(REQ_GET_ASSET)
							LoadNewAssets();
							rlDebug2("End assets enumeration");
						break;

						HANDLER_REQ_CASE(REQ_CONSUME_ASSET)
							rlDebug2("End assets consuming");
						break;

						HANDLER_REQ_CASE(REQ_GET_COUNTS)
							rlDebug2("**** %d offers, %d new", m_CachedContentCount.dwTotalOffers, m_CachedContentCount.dwNewOffers);
							rlDebug2("End get marketplace counts");
						break;

						HANDLER_REQ_CASE(REQ_TOTAL_NUMBER)
						break;
					}
				break;
			}
#undef HANDLER_STATE_CASE
#undef HANDLER_REQ_CASE

			m_XovStatus.EndOverlapped();

			if (m_handleEvent)
			{
				CloseHandle(m_handleEvent);
				m_handleEvent = NULL;
			}
			m_MyStatus.Reset();
			m_XovStatus.Clear();

			if (m_OfferData)
			{
				delete [] (BYTE*)m_OfferData;
				m_OfferData = NULL;
			}

			if (m_AssetData)
			{
				delete [] (BYTE*)m_AssetData;
				m_AssetData = NULL;
			}

			m_Operation = OP_IDLE;
		}
	break;

	case netStatus::NET_STATUS_FAILED:
	case netStatus::NET_STATUS_CANCELED:
		{
			if (m_MyStatus.Failed())
			{
				rlError("Operation FAILED \"%d\"", m_Operation);
			}
			else if (m_MyStatus.Canceled())
			{
				rlWarning("Operation Canceled \"%d\"", m_Operation);
			}

			if (m_OfferData)
			{
				delete [] (BYTE*)m_OfferData;
				m_OfferData = NULL;
			}

			if (m_AssetData)
			{
				delete [] (BYTE*)m_AssetData;
				m_AssetData = NULL;
			}

			if (m_handleEvent)
			{
				CloseHandle(m_handleEvent);
				m_handleEvent = NULL;
			}

			Assert(!m_XovStatus.Pending());

			m_MyStatus.Reset();
			m_XovStatus.Clear();

			m_Operation = OP_IDLE;
		}
		break;

	case netStatus::NET_STATUS_NONE:
		break;
	}
}

bool
rlMarketplace::GetBackgroundDownloadStatus() const
{
	return  (XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW == XBackgroundDownloadGetMode());
}

void
rlMarketplace::SetBackgroundDownloadStatus(const bool bgdlAvailability)
{
	XBackgroundDownloadSetMode(bgdlAvailability ? XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW : XBACKGROUND_DOWNLOAD_MODE_AUTO);
}

// --- Private ----------------------------------------------------

bool
rlMarketplace::NativeEnumerateOffers(const bool PPU_ONLY(offersOnly))
{
	bool overlapped = false;
	rtry
	{
		rcheck(m_LocalGamerIndex > -1, catchall, rlError("NativeEnumerateOffers:: FAILED - Invalid game index."));
		rcheck(!m_MyStatus.Pending(), catchall, rlError("NativeEnumerateOffers:: FAILED - Pending Operations."));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("NativeEnumerateOffers:: FAILED - Invalid operation status."));
		rcheck(!m_handleEvent, catchall, rlError("Invalid handle."));

		DWORD cbBuffer;

		// Enumerate at most MAX_ENUMERATION_RESULTS consumable items
		DWORD dwErr = XMarketplaceCreateOfferEnumerator(m_LocalGamerIndex,
														m_OfferType,
														m_Category,
														MAX_OFFERS_ENUMERATED,
														&cbBuffer,
														&m_handleEvent);
		rcheck(ERROR_SUCCESS == dwErr, catchall, rlError("XMarketplaceCreateOfferEnumerator:: FAILED - (code=\"0x%08x\")", dwErr));

		m_Type = REQ_GET_OFFER;
		m_Operation = OP_REQUEST;
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		if(!m_OfferData)
		{
			m_OfferData = (sMarketContentOfferInfo*) rage_new BYTE[cbBuffer];
		}
		ZeroMemory(m_OfferData, cbBuffer);

		dwErr = XEnumerate(m_handleEvent, (VOID*)m_OfferData, cbBuffer, 0, (PXOVERLAPPED)m_XovStatus.GetOverlapped());

		// No data exists for enumeration
		if(ERROR_NO_MORE_FILES == dwErr)
		{
			dwErr = ERROR_SUCCESS;
		}
		rcheck(ERROR_SUCCESS == dwErr || ERROR_IO_PENDING == dwErr, catchall, rlError("XEnumerate:: FAILED - (code=\"0x%08x\")", dwErr));

		rlDebug2("Started offers enumeration");

		return true;
	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped();
			m_MyStatus.Reset();
			m_Operation = OP_IDLE;
		}

		if (m_OfferData)
		{
			delete [] (BYTE*)m_OfferData;
			m_OfferData = NULL;
		}

		if (m_handleEvent)
		{
			CloseHandle(m_handleEvent);
			m_handleEvent = NULL;
		}
	}

	return false;
}

bool
rlMarketplace::NativeEnumerateAssets()
{
	bool overlapped = false;

	rtry
	{
		rcheck(m_LocalGamerIndex > -1, catchall, rlError("NativeEnumerateAssets:: FAILED - Invalid game index."));
		rcheck(!m_MyStatus.Pending(), catchall, rlError("NativeEnumerateAssets:: FAILED - Pending Operations."));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("NativeEnumerateAssets:: FAILED - Invalid operation status."));
		rcheck(!m_handleEvent, catchall, rlError("NativeEnumerateAssets:: FAILED - Invalid handle."));

		DWORD cbBuffer;

		// Enumerate consumable assets. This is needed to retrieve the current available quantity for each asset
		DWORD dwErr = XMarketplaceCreateAssetEnumerator(m_LocalGamerIndex,
														MAX_ASSETS_ENUMERATED,
														&cbBuffer,
														&m_handleEvent);
		rcheck(ERROR_SUCCESS == dwErr, catchall, rlError("XMarketplaceCreateAssetEnumerator:: FAILED (code=\"0x%08x\")", dwErr));

		m_Type = REQ_GET_ASSET;
		m_Operation = OP_REQUEST;
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		if(!m_AssetData)
		{
			m_AssetData = (sMarketPlaceAssetEnumerateReply*)rage_new BYTE[cbBuffer];
		}
		ZeroMemory( m_AssetData, cbBuffer );

		dwErr = XEnumerate(m_handleEvent, (VOID*)m_AssetData, cbBuffer, 0, (PXOVERLAPPED)m_XovStatus.GetOverlapped());

		// No data exists for enumeration
		if(ERROR_NO_MORE_FILES == dwErr)
		{
			dwErr = ERROR_SUCCESS;
		}
		rcheck(ERROR_SUCCESS == dwErr || ERROR_IO_PENDING == dwErr, catchall, rlError("XEnumerate:: FAILED (code=\"0x%08x\")", dwErr));

		rlDebug2("Started assets enumeration");

		return true;
	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped();
			m_MyStatus.Reset();
			m_Operation = OP_IDLE;
		}

		if (m_AssetData)
		{
			delete [] (BYTE*)m_AssetData;
			m_AssetData = NULL;
		}

		if (m_handleEvent)
		{
			CloseHandle(m_handleEvent);
			m_handleEvent = NULL;
		}
	}

	return false;
}

bool
rlMarketplace::NativeRetrieveCachedContent()
{
	bool overlapped = false;

	rtry
	{
		rcheck(m_LocalGamerIndex > -1, catchall, rlError("NativeRetrieveCachedContent:: FAILED - Invalid game index."));
		rcheck(!m_MyStatus.Pending(), catchall, rlError("NativeRetrieveCachedContent:: FAILED - Pending Operations."));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("NativeRetrieveCachedContent:: FAILED - Invalid operation status."));

		ZeroMemory(&m_CachedContentCount, sizeof(XOFFERING_CONTENTAVAILABLE_RESULT));

		m_Type = REQ_GET_COUNTS;
		m_Operation = OP_REQUEST;
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		// XContentGetMarketplaceCounts will only return non-consumable offer counts.
		DWORD dwErr = XContentGetMarketplaceCounts( m_LocalGamerIndex,
													m_Category,
													sizeof( XOFFERING_CONTENTAVAILABLE_RESULT ),
													(XOFFERING_CONTENTAVAILABLE_RESULT*)&m_CachedContentCount,
													(PXOVERLAPPED)m_XovStatus.GetOverlapped() );
		rcheck(ERROR_SUCCESS == dwErr || ERROR_IO_PENDING == dwErr, catchall, rlError("XContentGetMarketplaceCounts:: FAILED - (code=\"0x%08x\")", dwErr));

		rlDebug2("Started get marketplace counts");

		return true;
	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped();
			m_MyStatus.Reset();
			m_Operation = OP_IDLE;
		}
	}

	return false;
}

bool
rlMarketplace::NativeConsumeAssets(const sMarketPlaceAsset* assetIds)
{
	bool overlapped = false;

	rtry
	{
		rcheck(m_LocalGamerIndex > -1, catchall, rlError("Invalid game index."));
		rcheck(!m_MyStatus.Pending(), catchall, rlError("Pending Operations."));
		rcheck(OP_IDLE == m_Operation, catchall, rlError("Invalid operation status."));

		m_Type = REQ_CONSUME_ASSET;
		m_Operation = OP_REQUEST;
		m_XovStatus.Clear();
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		DWORD dwErr;
		dwErr = XMarketplaceConsumeAssets(m_LocalGamerIndex,
										  1,
										  (XMARKETPLACE_ASSET*)assetIds,
										  (XOVERLAPPED*)m_XovStatus.GetOverlapped());
		rcheck(ERROR_SUCCESS==dwErr || ERROR_IO_PENDING==dwErr, catchall, rlError("XMarketplaceConsumeAssets:: FAILED - (code=\"0x%08x\")", dwErr));

		rlDebug2("Started assets consuming");

		return true;
	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped();
			m_MyStatus.Reset();
			m_Operation = OP_IDLE;
		}
	}

	return false;
}

void
rlMarketplace::LoadNewOffers( )
{
	if (!m_OfferData)
		return;

	unsigned iOffersCount = m_XovStatus.GetOverlappedResult();
	rlDebug2("**** %d offers", iOffersCount);

	for (unsigned i=0; i<iOffersCount; i++)
	{
		//
		//XMARKETPLACE_CONTENTOFFER_INFO
		//
		// qwOfferID					- Identification number for the marketplace offer.
		// qwPreviewOfferID				- Identification number for a previous related marketplace offer.
		// dwOfferNameLength			- Number of bytes in the offer name.
		// wszOfferName					- String name of the offer.
		// dwOfferType					- Marketplace offer type.
		// contentId					- Byte array, of length XMARKETPLACE_CONTENT_ID_LEN, that contains the content ID.
		// fIsUnrestrictedLicense		- Whether the license for the content is unrestricted. If TRUE, there are no restrictions on the license.
		// dwLicenseMask				- The license mask, if the content is not unrestricted.
		// dwTitleID					- The primary titleID that the offer is associated with. This may be different from the current title, in the case of cross-associated offers.
		// dwContentCategory			- The content category.
		// dwTitleNameLength			- Length of the title name.
		// wszTitleName					- Content title name.
		// fUserHasPurchased			- Whether the user has purchased this content item.
		// dwPackageSize				- Size in bytes of the content package.
		// dwInstallSize				- Size of the installed package.
		// dwSellTextLength				- Size in bytes of text to use in the sale offer of this content package.
		// wszSellText					- Text to use in the sale offer of this content package.
		// dwAssetID					- ID of the asset.
		// dwPurchaseQuantity			- Quantity of items that are included in a single purchase of this content item.
		// dwPointsPrice				- Point price of this content item.

		OfferInfo offer;

		char offerName[OfferInfo::STORE_PRODUCT_NAME_LEN];
		if (WideString_to_String((WCHAR*)m_OfferData[i].wszOfferName, offerName, OfferInfo::STORE_PRODUCT_NAME_LEN))
		{
			offer.SetName(offerName);
		}

		char sellText[OfferInfo::STORE_PRODUCT_LONG_DESCRIPTION_LEN];
		if (WideString_to_String((WCHAR*)m_OfferData[i].wszSellText, sellText, OfferInfo::STORE_PRODUCT_LONG_DESCRIPTION_LEN))
		{
			int iLanguageTextEnd = 0;
			if (sellText[0] == '(') // Language text start found
			{
				for (unsigned j=0; j<strlen(sellText); j++)
				{
					if (sellText[j] == ' ') // Language text end found
					{
						iLanguageTextEnd = ++j;
						break;
					}
					else if (sellText[j] == '\0')
					{
						rlError("Error searching for language text on string - %s -", sellText);
						iLanguageTextEnd = 0;
						break;
					}
				}
			}

			offer.SetDescription(sellText);
		}

		offer.SetOfferId(m_OfferData[i].qwOfferID);
		offer.SetAssetId(m_OfferData[i].dwAssetID);
		offer.SetType(m_OfferData[i].dwOfferType);
		offer.SetLicenseMaskId(m_OfferData[i].dwLicenseMask);
		offer.SetCategoryId(m_OfferData[i].dwContentCategory);
		offer.SetPrice(m_OfferData[i].dwPointsPrice);
		offer.SetWasPurchased(m_OfferData[i].fUserHasPurchased != 0);
		offer.SetIsInstaled(false);
		offer.SetIsDownloadable(false);
		offer.SetContentId(m_OfferData[i].contentId);

		//// Check to See if it is a new offer
		//if (DoesContentIdMatch((BYTE*)&m_OfferData[i].qwOfferID))
		//{
		//	offer.SetIsInstaled(true);
		//}

		if (!OfferExists(offer.GetOfferId()))
		{
			OfferInfo* newOffer = rage_new OfferInfo();
			newOffer->Copy(offer);
			AddOffer(newOffer);
		}
		else
		{
			rlMarketplaceOfferIt it = GetOfferIterator();
			for(OfferInfo* auxOffer = it.Current(); auxOffer; auxOffer = it.Next())
			{
				if (offer.GetOfferId() == auxOffer->GetOfferId())
				{
					auxOffer->Copy(offer);
					break;
				}
			}
		}
	}
}

void
rlMarketplace::LoadNewAssets( )
{
	if (!m_AssetData)
		return;

	rlDebug2("**** %d assets", m_AssetData->assetPackage.cAssets);

	for(DWORD i = 0; i<m_AssetData->assetPackage.cAssets; i++)
	{
		AssetInfo asset;

		asset.SetAssetId(m_AssetData->assetPackage.aAssets[i].dwAssetID);
		asset.SetQuantity(m_AssetData->assetPackage.aAssets[i].dwQuantity);

		if (rlVerify(!AssetExists(asset.GetAssetId())))
		{
			AssetInfo* newAsset = rage_new AssetInfo();
			newAsset->Copy(asset);
			AddAsset(newAsset);
		}
		else
		{
			rlMarketplaceAssetIt it = GetAssetIterator();
			for(AssetInfo* auxAsset = it.Current(); auxAsset; auxAsset = it.Next())
			{
				if (asset.GetAssetId() == auxAsset->GetAssetId())
				{
					auxAsset->Copy(asset);
					break;
				}
			}
		}
	}
}

//bool
//rlMarketplace::DoesContentIdMatch(BYTE* pbContentId)
//{
//	if (pbContentId)
//	{
//		for (int i=0; i<MAX_CONTENT_PACKAGES; i++)
//		{
//			if (XMarketplaceDoesContentIdMatch(pbContentId, &g_contentData[i]))
//				return true;
//		}
//	}
//
//	return false;
//}

}   //namespace rage

#endif // __XENON

// EOF
