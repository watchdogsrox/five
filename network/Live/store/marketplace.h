//
// marketplace.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

/*

 [ PS3 ]

   - Manage access to PLAYSTATION®Store, preform "in-game browsing".
     Uses APIs of the NP IN-GAME commerce 2 utility to obtain the offer catalog and enables browsing.
     When the user chooses a offer, the API of the NP IN-GAME commerce 2 utility is executed to process the purchase.

	  IDs Example:
         - TOP_CATEGORY_ID: "EP1017-BLES00229_00"
         - CATEGORY_ID:     "-TEST_CAT_001"
         - PRODUCT_ID:      "-GTAIVTESTE000001"
         - SKU_ID:          "-E001"

      Category Ids: "EP1017-BLES00229_00-TEST_CAT_001"
      Offer Ids: "EP1017-BLES00229_00-GTAIVTESTE000001"
      SKU Ids: "EP1017-BLES00229_00-GTAIVTESTE000001-E001"

 [ XENON ]

   - Manage access to xbox live marketplace.

     1 - Before calling downloadable items UI, you must determine what items has the player downloaded in order
         to not accidentally double charge with the paid items UI, and to make sure that consumable items can
         be downloaded again.
     2 - To determine if a consumable item has been previously purchased, use the XContent APIs to enumerate downloaded
         Marketplace content, then pass the XCONTENT_DATA structs to XMarketplaceDoesContentIdMatch.
         XMarketplaceDoesContentIdMatch will return TRUE if the player has a particular offer.

*/


#ifndef MARKETPLACE_H
#define MARKETPLACE_H

// --- Include Files ------------------------------------------------------------

// Rage includes
#include "net/status.h"
#include "string/unicode.h"

namespace rage
{

// --- Forward Definitions ------------------------------------------------------
class sysMemAllocator;
class OfferInfo;
class rlMarketplaceOfferIt;
class rlMarketplaceOfferConstIt;
#if __XENON
class AssetInfo;
class rlMarketplaceAssetIt;
class rlMarketplaceAssetConstIt;
#elif __PPU
class CategoryInfo;
class rlMarketplaceCategoryIt;
class rlMarketplaceCategoryConstIt;
#endif // __XENON

#if __XENON

#define CONTENT_ID_LEN 20
#define ASSET_SIGNATURE_SIZE    256

#elif __PPU
#define MARKET_RECV_BUF_SIZE 256*1024
// Size of currency code
#define MARKET_CURRENCY_CODE_LEN 3
// Size of the currency symbol
#define MARKET_CURRENCY_SYMBOL_LEN 3
// Size of the character separating every 3 digits of the price
#define MARKET_THOUSAND_SEPARATOR_LEN 4
// Size of the character indicating the decimal point in the price
#define MARKET_DECIMAL_LETTER_LEN 4

#endif // __PPU

// --- Structure/Class Definitions ----------------------------------------------

#if __XENON

typedef void* MarketHandle;

struct sMarketOverlapped;

typedef void (*MarketOverlappedCompletionRoutine)(u32 dwErrorCode, u32 dwNumberOfBytesTransfered, sMarketOverlapped* pOverlapped);

struct sMarketOverlapped
{
	u32*        InternalLow;
	u32*        InternalHigh;
	u32*        InternalContext;
	MarketHandle   hEvent;
	MarketOverlappedCompletionRoutine  pCompletionRoutine;
	u32*        dwCompletionContext;
	u32         dwExtendedError;
};

struct sMarketOfferingContentAvailableResult
{
	unsigned  dwNewOffers;
	unsigned  dwTotalOffers;
};

struct sMarketContentOfferInfo
{
	u64                     qwOfferID;
	u64                     qwPreviewOfferID;
	unsigned                dwOfferNameLength;
	char16                  *wszOfferName;
	unsigned                dwOfferType;
	unsigned char           contentId[CONTENT_ID_LEN];
	int                     fIsUnrestrictedLicense;
	unsigned                dwLicenseMask;
	unsigned                dwTitleID;
	unsigned                dwContentCategory;
	unsigned                dwTitleNameLength;
	char16                  *wszTitleName;
	int                     fUserHasPurchased;
	unsigned                dwPackageSize;
	unsigned                dwInstallSize;
	unsigned                dwSellTextLength;
	char16                  *wszSellText;
	unsigned                dwAssetID;
	unsigned                dwPurchaseQuantity;
	unsigned                dwPointsPrice;
};

struct sMarketPlaceAsset
{
	unsigned   dwAssetID;
	unsigned   dwQuantity;
};

struct sMarketFiletime
{
	unsigned dwHighDateTime;
	unsigned dwLowDateTime;
};

struct sMarketPlaceAssetPackage
{
	sMarketFiletime ftEnumerate;
	unsigned      cAssets;
	unsigned      cTotalAssets;
	sMarketPlaceAsset aAssets[1];         // this array contains cAssets number of entries... not 1.
};

struct sMarketPlaceAssetEnumerateReply
{
	unsigned char             signature[ASSET_SIGNATURE_SIZE];
	sMarketPlaceAssetPackage  assetPackage;
};

#elif __PPU

struct sMarketCellHttpsData
{
	char*  ptr;
	unsigned size;
};

struct sMarketSessionInfo
{
	char  currencyCode[MARKET_CURRENCY_CODE_LEN + 1];
	u32   decimals;
	char  currencySymbol[MARKET_CURRENCY_SYMBOL_LEN + 1];
	u32   symbolPosition;
	bool  symbolWithSpace;
	char  thousandSeparator[MARKET_THOUSAND_SEPARATOR_LEN + 1];
	char  decimalLetter[MARKET_DECIMAL_LETTER_LEN + 1];
	char  padding[1];
	u32   reserved[4];
};

#endif // __XENON

//PURPOSE
//   Manage access to marketplace store.
class rlMarketplace
{
public:

#if __PPU
	static const unsigned int MAX_TEXT_LENGTH = 30;
#endif // __PPU

	enum Operations
	{
		OP_IDLE             = 0, // Idle - doing f*all.
		OP_REQUEST          = 1, // Doing a Request.
#if __PPU
		OP_CREATE_SESSION   = 2, // Creating a Session.
		OP_CHECKOUT         = 3, // Active during the checkout process.
		OP_REDOWNLOAD       = 4, // Of the offers sold on PLAYSTATION®Store, DRM contents
								 //   (downloadable offers) are provided under the policy that if the user
								 //   loses the purchased content – due to a hardware malfunction, for example, they can be re-downloaded.
#endif // __PPU

		OP_TOTAL_NUMBER
	};

	enum RequestType
	{
		REQ_GET_OFFER, // Requesting offer information
#if __XENON
		REQ_GET_ASSET,     // Requesting asset information
		REQ_CONSUME_ASSET, // Consuming Assets.
		REQ_GET_COUNTS,    // Retrieves the counts of Cached Content.
#elif __PPU
		REQ_GET_CATEGORY, // Requesting category information
#endif // __XENON

		REQ_TOTAL_NUMBER
	};

#if __XENON
	enum OfferTypes
	{
		OFFER_TYPE_CONTENT      = 0x00000002,
		OFFER_TYPE_GAME_DEMO    = 0x00000020,
		OFFER_TYPE_GAME_TRAILER = 0x00000040,
		OFFER_TYPE_THEME        = 0x00000080,
		OFFER_TYPE_TILE         = 0x00000800,
		OFFER_TYPE_ARCADE       = 0x00002000,
		OFFER_TYPE_VIDEO        = 0x00004000,
		OFFER_TYPE_CONSUMABLE   = 0x00010000
	};

public:
	//PURPOSE
	//  Control system calls.
	struct XovOverlapped
	{
		sMarketOverlapped  m_Overlapped;
		unsigned      m_OverlappedResult;
		netStatus*    m_MyStatus;

		XovOverlapped();
		~XovOverlapped();

		void  Clear();
		void  Cancel();
		bool  Pending() const;
		void  BeginOverlapped(netStatus* status);
		void  EndOverlapped();
		bool  CheckExtendedError();
		bool  CheckOverlapped();
		unsigned  GetOverlappedResult();
		sMarketOverlapped* GetOverlapped();
	};

#elif __PPU

public:

	//PURPOSE
	//  Control system calls.
	struct XovOverlapped
	{
		int         m_Event;
		int         m_ErrorCode;
		netStatus*  m_MyStatus;

		XovOverlapped();
		~XovOverlapped();

		void  Clear();
		void  Cancel();
		bool  Pending() const;
		void  BeginOverlapped(netStatus* status);
		void  EndOverlapped(const int sysEvent, const int errorCode);
		bool  CheckExtendedError();
		bool  CheckOverlapped();
		int   GetOverlappedResult();
	};


private:

	//PURPOSE
	// Serves for communication purposes and retrieve info from PLAYSTATION®Store.
	struct RequestInfo
	{
	public:

		//Request ID
		u32  m_Id;

		//Category Id
		const char*  m_CategoryId;

		//Offer Id
		const char*  m_OfferId;

		//Request Buffer
		u8*  m_Buffer;

		//Buffer Size
		unsigned  m_BufferSize;

		//Memory allocator pointer
		sysMemAllocator*  m_Allocator;

	public:
		RequestInfo() 
			: m_Id(0)
			 ,m_CategoryId(0)
			 ,m_OfferId(0)
			 ,m_Buffer(0)
			 ,m_BufferSize(0)
			 ,m_Allocator(0)
		{
		}

		~RequestInfo()
		{
			Shutdown();
		}

		void  Init(sysMemAllocator* allocator);

		void  Shutdown()
		{
			DeleteBuffer();
		}

		void  ClearBuffer();


	private:

		void  CreateBuffer();
		void  DeleteBuffer();
	};
#endif // __XENON

private:

	// This is true when it is initialized.
	bool  m_IsInitialed;

	// Offers List
	OfferInfo* m_Offers;

	// Current operation
	Operations  m_Operation;

	// Current operation
	Operations  m_LastOperation;

	// Maintain operations status
	netStatus  m_MyStatus;

#if !__WIN32PC && !RSG_DURANGO
	// Control system calls.
	XovOverlapped  m_XovStatus;
#endif

	// Type of the request
	RequestType m_Type;

#if __XENON

	// Assets List
	AssetInfo* m_Assets;

	// The type of offer to be enumerated. This value is constructed
	// by using a bitwise OR operator on OfferTypes.
	u32  m_OfferType;

	// A filter that is defined by the game,
	// to specify the types of offer to enumerate.
	u32  m_Category;

	// Offers Category
	unsigned m_TopCategory;

	// Local gamer index.
	int m_LocalGamerIndex;

	// Retrieve offer data.
	sMarketContentOfferInfo*  m_OfferData;

	// Retrieve asset data.
	sMarketPlaceAssetEnumerateReply*  m_AssetData;

	// Countains info about the cached content in the marketplace - Only for Offers not Assets.
	sMarketOfferingContentAvailableResult  m_CachedContentCount;

	// Handle used for enumerations.
	MarketHandle  m_handleEvent;

#elif __PPU

	// Top store category ~ Usually you will want this to be the title ID
	char  m_TopCategory[MAX_TEXT_LENGTH];

	// Flag initialization
	bool  m_SessionCreated;

	//Allocate memory for the pools
	sysMemAllocator* m_Allocator;

	// Pool for http
	u8*  m_HttpPool;

	// Pool for http cookies
	u8*  m_HttpCookiePool;

	// Pool for SSL
	u8*  m_SslPool;

	// Structure that holds HTTPS data
	sMarketCellHttpsData  m_caList;

	// Context ID
	u32  m_CtxId;

	// Session Info
	sMarketSessionInfo m_SessionInfo;

	// Request info
	RequestInfo m_RequestInfo;

	// Memory container for the checkout process
	sys_memory_container_t m_CheckoutContainer;

	// Control Last request page from marketplace.
	unsigned  m_LastPage;

	// Category list
	CategoryInfo* m_Categories;

#endif // __PPU

public:

	rlMarketplace();
	~rlMarketplace();

	static const char*   GetOperationName(const Operations);
	static const char*   GetRequestName(const RequestType);

#if __XENON
	//PURPOSE
	//  Main initialization routine.
	//PARAMS
	//  offerType    -  The type of offer to be enumerated. This value is constructed by using
	//                  a bitwise OR operator on OfferTypes.
	//  categories   -  A filter that is defined by the game, to specify the types of offer
	//                  to enumerate. A value of 0xffffffff means all categories.
	void  Init(const u32 offerType,
			   const u32 categories);
#elif __PPU
	//PURPOSE
	//  Main initialization routine.
	//PARAMS
	//  allocator    -  Allocator used to allocate memory for the pools.
	//  categories   -  A filter that is defined by the game, to specify the types of offer to
	//                  enumerate. This is the top category ID, for example, "EP1017-BLES00229_00".
	void  Init(sysMemAllocator* allocator, const char* categories);
#endif // __XENON

	//PURPOSE
	//  Shutdown marketplace. Any pending operation is canceled.
	void  Shutdown();

	//PURPOSE
	//  Cancel pending operations.
	bool  Cancel();

	//PURPOSE
	//  Call this on the update loop. Starts new operations and checks for asynchronous operations.
	void  Update();

	//PURPOSE
	//  Returns TRUE when an operation is pending.
	bool  Pending() const;

	//PURPOSE
	//   Returns TRUE when last operation Failed.
	bool  Failed() const;

	//PURPOSE
	//   Returns TRUE when last operation Succeeded.
	bool  Succeeded() const;

	//PURPOSE
	//   Returns TRUE when last operation is canceled.
	bool  Canceled() const;

	//PURPOSE
	//  Returns the current status code.
	Operations GetStatus() const;

	//PURPOSE
	//  Returns the last status code.
	Operations GetLastStatus() const;

	//PURPOSE
	//  Returns the request type.
	RequestType GetRequestType() const;

	//PURPOSE
	//  Enumerates a list of Marketplace offers including offers for consumables.
	//PARAMS
	//  localGamerIndex    - Index of the local gamer.
	//RETURNS
	//  Returns true if operation has been activated.
	//NOTES
	//  Xenon:  Local gamer index only for the xbox.
	//    PS3:  Local gamer index is not used - pass in any integer.
	bool  EnumerateOffers(const int localGamerIndex, const bool offersOnly);

	//PURPOSE
	//  Enumerates a list of a gamer's consumable assets.
	//  Consumables are only offered within game and are not available from the game marketplace dashboard.
	//  Consumable offers can only be downloaded using the new marketplace functions.
	//PARAMS
	//  localGamerIndex    - Index of the local gamer.
	//RETURNS
	//  Returns true if operation has been activated.
	//NOTES
	//  Xenon:  Local gamer index only for the xbox.
	//    PS3:  Local gamer index is not used - pass in any integer.
	bool  EnumerateAssets(const int localGamerIndex);

	//PURPOSE
	//  Return the number of total offers available in the marketplace for a currently signed-in gamer.
	//NOTES
	//  Xenon:  The count can be retrieved by using the marketplace counts.
	//    PS3:  To retrieve the count you will need to Enumerate the marketplace offers.
	unsigned  GetOffersCount() const;

	//PURPOSE
	//  Returns an iterator for an Offer.
	rlMarketplaceOfferConstIt GetConstOfferIterator( ) const;

#if __XENON

	//PURPOSE
	//  Return the number of total assets available in the marketplace for a currently signed-in gamer.
	unsigned  GetAssetsCount() const;

	//PURPOSE
	//  Returns an iterator for an Asset.
	rlMarketplaceAssetConstIt GetConstAssetIterator( ) const;

#elif __PPU
	//PURPOSE
	//  Returns TRUE if the session was created successfully.
	inline bool GetSessionCreated() const { return m_SessionCreated; }

	//PURPOSE
	//  Return the number of total categories available in the store for a currently signed-in gamer.
	unsigned  GetCategoryCount() const;

	//PURPOSE
	//  Returns an iterator for a Category.
	rlMarketplaceCategoryConstIt GetConstCategoryIterator( ) const;
#endif // __XENON

	//PURPOSE
	//  Xenon:  Obtain background download "Always On / Auto" setting.
	//    PS3:  Obtain background download enabled/disabled setting.
	//RETURNS
	//  Xenon:  TRUE if background downloads occur, regardless of title network activity.
	//          FALSE if background downloads automatically start or stop background downloads based on title network activity.
	//    PS3:  TRUE in case the background download is enabled.
	//          FALSE in case the background download is disabled.
	//NOTES
	//    PS3:  This setting affects the following: "Store browsing", "Checkout" and "Download list display".
	bool  GetBackgroundDownloadStatus() const;

	//PURPOSE
	//  Xenon:  Set background download "Always On / Auto".
	//    PS3:  Enable/Disable .
	//PARAMS
	//  Xenon:  bgdlAvailability    -  Always On(true)/ Auto(false) background download setting.
	//    PS3:  bgdlAvailability    -  Enable(true)/Disable(false) background download setting.
	void  SetBackgroundDownloadStatus(const bool bgdlAvailability);

	//PURPOSE
	//  Return number of downloadable offers.
	unsigned  GetNumDownloadableOffers() const;

	//PURPOSE
	//  Return number of buyable offers.
	unsigned  GetNumBuyableOffers() const;

#if __XENON

	//PURPOSE
	//  Consumes some of a gamer's consumable assets.
	//PARAMS
	//  localGamerIndex    - Index of the local gamer.
	//  assetId            - Asset ID's array to consume.
	//  quantity           - Array count of each asset to consume.
	//  assetsCount        - Number of assets in the asset array to consume.
	//RETURNS
	//  Returns true if operation has been activated.
	bool  ConsumeAssets(const int localGamerIndex,
						const u32* assetId,
						const unsigned* quantity,
						const unsigned assetsCount);

	//PURPOSE
	//  Get the current number of new and existing offerings for the player.
	//  XContentGetMarketplaceCounts will only return non-consumable offer counts.
	//PARAMS
	//  localGamerIndex    - Index of the local gamer.
	bool  RetrieveCachedContent(const int localGamerIndex);

	//PURPOSE
	//  When the marketplace counts have been retrieved this will return the
	//  total number of offers.
	unsigned  GetTotalCachedContentCount();

	//PURPOSE
	//  When the marketplace counts have been retrieved this will return the
	//  total number of new offers.
	unsigned  GetNewCachedContentCount() const;

	//PURPOSE
	//  Spew assets information.
	void  SpewAssetsInfo() const;

#elif __PPU

	//PURPOSE
	//  Init a marketplace session.
	void  InitSession();

	//PURPOSE
	//  Init a marketplace session.
	void  ShutdownSession();

	//PURPOSE
	//  After a user has chosen the offers to purchase we start the processing for purchases ("checkout").
	//  The maximum number of SKU IDs that can be specified is SCE_NP_COMMERCE2_SKU_CHECKOUT_MAX.
	//PARAMS
	//  offerIds     -  Array with offers id's.
	//  offersCount  -  Number of offers being checked out.
	//RETURNS
	//  Returns TRUE if the checkout UI was started successfully.
	//NOTES
	//  Offers that can be checked out are offers that can be paid for. If a offer
	//  can not be paid for but can be downloaded use the re-download utility for that
	//  purpose.
	bool  StartCheckout(const char* offerIds[], const unsigned offersCount);

	//PURPOSE
	//  Re-Download specific offers that already have been paid for but for some reason
	//  they need need to be re-downloaded.
	//PARAMS
	//  offerIds     -  Array with offers id's.
	//  offersCount  -  Number of offers being checked out.
	//RETURNS
	//  Returns TRUE if the download UI was started successfully.
	bool  StartReDownload(const char* offerIds[], const unsigned offersCount);

	void  SpewCategoriesInfo() const;
#endif // __PPU
	void  SpewOffersInfo() const;

private:

	//PURPOSE
	//  Enumerate offers.
	bool  NativeEnumerateOffers(const bool PPU_ONLY(offersOnly));

	//PURPOSE
	//  Enumerate assets.
	bool  NativeEnumerateAssets();

	//PURPOSE
	//  Free offers memory.
	void  ClearOffers();

	//PURPOSE
	//  Add an offer to our offer list.
	void  AddOffer(OfferInfo* offer);

	//PURPOSE
	//  Returns an iterator for an Offer.
	rlMarketplaceOfferIt GetOfferIterator( );

#if __XENON
	//PURPOSE
	//  Retrieve marketplace counts.
	bool  NativeRetrieveCachedContent();

	//PURPOSE
	//  Consume assets.
	bool  NativeConsumeAssets(const sMarketPlaceAsset* assetIds);

	//PURPOSE
	//  Returns TRUE if the offer exists.
	bool  OfferExists(const u64 id) const;

	//PURPOSE
	//  Returns an iterator for an Asset.
	rlMarketplaceAssetIt GetAssetIterator( );

	//PURPOSE
	//  Free asset memory.
	void  ClearAssets();

	//PURPOSE
	//  Add an asset to our asset list.
	void  AddAsset(AssetInfo* asset);

	//PURPOSE
	//  Returns TRUE if the asset exists.
	bool  AssetExists(const u32 id) const;

	//PURPOSE
	//  When the offer enumeration ends this method adds all new offers.
	void  LoadNewOffers( );

	//PURPOSE
	//  When the asset enumeration ends this method adds all new offers.
	void  LoadNewAssets( );
#elif __PPU
	//PURPOSE
	//  Returns an iterator for a Category.
	rlMarketplaceCategoryIt GetCategoryIterator( );

	//PURPOSE
	//  Returns TRUE if the offer exists.
	bool  OfferExists(const char* id) const;

	//PURPOSE
	//  Returns TRUE if the category already exists.
	bool  CategoryExists(const char* id) const;

	//PURPOSE
	//  Callback function registered to the NP IN-GAME commerce 2 utility.
	static void Commerce2Handler(u32 ctx_id, u32 subject_id, int sysEvent, int errorCode, void *arg);

	// --- Initialization ---------

	//PURPOSE
	//  Load CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 module.
	bool  LoadModule();

	//PURPOSE
	//  UnLoad CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 module
	bool  UnLoadModule();

	//PURPOSE
	//  Load SSL CA certificates
	bool  LoadSslCerticates();

	//PURPOSE
	//  UnLoad SSL CA certificates
	void  UnLoadSslCerticates();


	// --- Session ---------
	//
	//  - When the session is successfully created, the callback function is called to notify
	//    the SCE_NP_COMMERCE2_EVENT_CREATE_SESSION_DONE event.

	//PURPOSE
	//  When the event SCE_NP_COMMERCE2_SERVER_ERROR_SESSION_EXPIRED is received, we
	//  need to delete the commerce 2 context and recreate it.
	void  RecreateSession();

	//PURPOSE
	//  Session creation.
	bool  CreateSession();

	//PURPOSE
	//  Finish Session creation
	bool  FinishCreateSession();


	// --- Obtaining the offer Catalog ---------
	//
	//  - The offer catalog is organized into hierarchical categories,
	//    and the information is obtained as either category content information or offer information.

	//PURPOSE
	//  Obtain Category Content Data.
	//NOTES
	//  This function is blocking. It does not return until it receives category content
	//  data from the NP server or an error (timeout, for example) occurs within the internally-used libhttp.
	//  This function initializes the category content data obtained with sceNpCommerce2GetCategoryContentsGetResult(),
	//  and enables category content information to be taken out with a function such as sceNpCommerce2GetCategoryInfo().
	bool  GetCategoryContentResults(const char* pCategoryId);

	//PURPOSE
	//  Obtain offer Data.
	//NOTES
	//  This function is blocking. It does not return until it receives offer data from the NP server or
	//  an error (timeout, for example) occurs within the internally-used libhttp.
	//  This function initializes the offer data obtained with sceNpCommerce2GetProductInfoGetResult(),
	//  and enables offer data information to be taken out with a function such as sceNpCommerce2GetGameProductInfo().
	bool  GetProductContentResults(const char* pOfferId, OfferInfo* pOffer);

	//PURPOSE
	//  End checkout
	bool  FinishCheckOut();

	//PURPOSE
	//  End checkout
	bool  FinishReDownload();

	//PURPOSE
	//  Create the memory container for the checkout process.
	bool  CreateMemoryContainer(const unsigned size);

	//PURPOSE
	//  Destroy the memory container and free checkout memory.
	bool  DestroyMemoryContainer();

	void  ClearCategories();
	void  AddCategory(CategoryInfo* pCategory);
	void  SetCountOfOffers(const char* pCategoryId, const unsigned uCount);
	void  SetNeedsEnumeration(const char* pCategoryId, const bool bEnumerate);
	bool  GetNeedsEnumeration() const;
#endif // __PPU

};

} // namespace rage

#endif // MARKETPLACE_H

// EOF
