//
// marketplace_offer.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETPLACE_OFFER_H
#define MARKETPLACE_OFFER_H

namespace rage
{

// --- Structure/Class Definitions ----------------------------------------------

//PURPOSE
//   Stores info about offers
//
// Xenon:
//      - Offer price in points
//
//   PS3:
//      - The value stored to price includes the numbers past the decimal point, and the application
//           must move the decimal point as necessary. For example, in the US store, decimals is 2.
//           If price is 399, this indicates $3.99.
//      - When the offer has already been purchased then the offer can only be re-downloaded.
//
class OfferInfo
{
	friend class rlMarketplace;

public:

#if __XENON
	static const unsigned int STORE_PRODUCT_NAME_LEN = 100;
	static const unsigned int STORE_PRODUCT_LONG_DESCRIPTION_LEN = 1000;
	static const unsigned int STORE_CONTENT_ID_SIZE = 20;
#elif __PPU
	static const unsigned int STORE_PRODUCT_ID_LEN = 48;
	static const unsigned int STORE_PRODUCT_NAME_LEN = 256;
	static const unsigned int STORE_PRODUCT_SHORT_DESCRIPTION_LEN = 1024;
	static const unsigned int STORE_PRODUCT_LONG_DESCRIPTION_LEN = 4000;
	static const unsigned int STORE_CATEGORY_NAME_LEN = 256;
	static const unsigned int STORE_SKU_ID_LEN = 56;
#elif __WIN32PC || RSG_DURANGO
	static const unsigned int STORE_PRODUCT_NAME_LEN = 256;
#endif // __XENON

private:
	// Offer name
	char  m_Name[STORE_PRODUCT_NAME_LEN];

	// TRUE when the offer has been installed.
	bool  m_Instaled;

	// TRUE when the offer has been purchased.
	bool  m_Purchased;

	// TRUE when the offer has been consumed ( already paid for and downloaded ).
	bool  m_Downloadable;

	// Offer price in points
	unsigned  m_Price;

#if __XENON

	// Sell text
	char  m_Description[STORE_PRODUCT_LONG_DESCRIPTION_LEN];

	// Identification number for the marketplace offer
	u64  m_OfferId;

	// ID of the asset
	unsigned  m_AssetID;

	// The content category is used by the title to group different sorts of content.
	unsigned  m_CategoryId;

	// Marketplace offer type - see OfferTypes for details.
	unsigned  m_Type;

	// The license mask can be used to unlock specific items or functionality within a content package.
	unsigned  m_LicenseMask;

	// unsigned char array, of length XMARKETPLACE_CONTENT_ID_LEN, that contains the content ID.
	unsigned char  m_ContentId[STORE_CONTENT_ID_SIZE];

#elif __PPU

	// Product Short description - This string is in UTF-8. Convert it to widechar using Utf8ToWide() on unicode.h
	char  m_ShortDesc[STORE_PRODUCT_SHORT_DESCRIPTION_LEN];

	// Product Long description - This string is in UTF-8. Convert it and deal with any html tags that it contains.
	char  m_LongDesc[STORE_PRODUCT_LONG_DESCRIPTION_LEN];

	// Identification for the marketplace offer
	char  m_OfferId[STORE_PRODUCT_ID_LEN];

	// ID of SKU
	char  m_SkuId[STORE_SKU_ID_LEN];

	// The content category is used by the title to group different sorts of content. - Game is expecting 00000001 - Episodes
	char  m_CategoryId[STORE_CATEGORY_NAME_LEN];

#endif // __XENON

	// Pointer to next offer.
	OfferInfo*  m_Next;

public:

	OfferInfo();
	~OfferInfo();

	void  Copy(const OfferInfo& offer);
	void  Clear();

	OfferInfo* Next() { return m_Next ? m_Next : NULL; }
	const OfferInfo* Next() const { return m_Next ? m_Next : NULL; }

	unsigned  GetPrice() const { return m_Price; }
	void  SetPrice(const unsigned price) { m_Price = price; }

	bool  GetWasPurchased() const { return m_Purchased; }
	void  SetWasPurchased(const bool purchased) { m_Purchased = purchased; }

	bool  GetIsInstaled() const { return m_Instaled; }
	void  SetIsInstaled(const bool instaled) { m_Instaled = instaled; }

	bool  GetIsDownloadable() const { return m_Downloadable; }
	void  SetIsDownloadable(const bool downloadable) { m_Downloadable = downloadable; }

	const char*  GetName() const { return ('\0' != m_Name ? m_Name : "(null)"); }
	void  SetName(const char* name);

#if __XENON

	const char*  GetDescription() const { return ('\0' != m_Description ? m_Description : "(null)"); }
	void  SetDescription(const char* desc);

	u64   GetOfferId() const { return m_OfferId; }
	void  SetOfferId(const u64 id) { m_OfferId = id; }

	unsigned  GetAssetId() const { return m_AssetID; }
	void  SetAssetId(const unsigned id) { m_AssetID = id; }

	unsigned  GetCategoryId() const { return m_CategoryId; }
	void  SetCategoryId(const unsigned id) { m_CategoryId = id; }

	unsigned  GetType() const { return m_Type; }
	void  SetType(const unsigned type) { m_Type = type; }

	unsigned  GetLicenseMaskId() const { return m_LicenseMask; }
	void  SetLicenseMaskId(const unsigned id) { m_LicenseMask = id; }

	const unsigned char*  GetContentId() const { return m_ContentId; }
	void  SetContentId(const unsigned char* contentId);

	//PURPOSE
	//  Checks the current status of a Marketplace download.
	//PARAMS
	//  localGamerIndex    - Index of the local gamer.
	//RETURNS
	//  Returns the download status of a offer.
	//   - ERROR_NOT_FOUND if the download manager doesn't have the Offer ID yet.
	//   - ERROR_IO_PENDING if the download is in progress.
	//   - ERROR_SUCCESS if the download has completed.
	u64  GetDownloadStatus(const int localGamerIndex) const;

#elif __PPU

	const char*  GetShortDesc() const { return ('\0' != m_ShortDesc ? m_ShortDesc : "(null)"); }
	void  SetShortDesc(const char* desc);

	const char*  GetLongDesc() const { return ('\0' != m_LongDesc ? m_LongDesc : "(null)"); }
	void  SetLongDesc(const char* desc);

	const char*  GetOfferId()    const { return ('\0' != m_OfferId ? m_OfferId : "(null)"); }
	const char*  GetSkuId()      const { return ('\0' != m_SkuId ? m_SkuId : "(null)"); }
	const char*  GetCategoryId() const { return ('\0' != m_CategoryId ? m_CategoryId : "(null)"); }

	void SetOfferId(const char* id);
	void SetSkuId(const char* id);
	void SetCategoryId(const char* id);

	//PURPOSE
	//  Obtaining the progress of background downloads.
	int  GetDownloadStatus() const;

#endif // __XENON
};

} // namespace rage

#endif // MARKETPLACE_OFFER_H

// EOF
