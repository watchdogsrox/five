
#if __PPU

// --- Include Files ------------------------------------------------------------

// C headers
#include <cell/sysmodule.h>
#include <cell/http.h>
#include <netex/net.h>
#include <netex/libnetctl.h>
#include <np.h>
#include <np/commerce2.h>
#include <cell/ssl.h>


// Rage headers
#include "diag/seh.h"
#include "net/status.h"
#include "atl/functor.h"
#include "data/callback.h"
#include "diag/channel.h"
#include "diag/output.h"
#include "system/memory.h"
#include "string/unicode.h"
#include "System/Param.h"
#include "rline/rldiag.h"
#include "rline/rl.h"

// Network headers
#include "marketplace.h"
#include "marketplace_offer.h"
#include "marketplace_category.h"
#include "marketplace_iterator.h"

#pragma comment(lib, "http_stub")
#pragma comment(lib, "ssl_stub")
#pragma comment(lib, "sysutil_np_commerce2_stub")
#pragma comment(lib, "sysutil_bgdl_stub")

namespace rage
{

// Max number of offers that can be enumerate
#define MAX_OFFERS_ENUMERATED  SCE_NP_COMMERCE2_GETCAT_MAX_COUNT

// Max number of assets that can be enumerate
#define MAX_ASSETS_ENUMERATED  0

// Maximum number of contents to obtain on a request.
#define MAX_PAGE_CONTENT 18

// Index of the product skuID to retrieve.
#define NP_SKU_PRODUCT_ID 0

//Pool sizes used for product checkout
#define HTTP_POOL_SIZE        (512 * 1024)
#define HTTP_COOKIE_POOL_SIZE (128 * 1024)
#define SSL_POOL_SIZE         (196 * 1024)

#ifdef getErrorName
#undef getErrorName
#endif
#define getErrorName(x) x == static_cast <int> (CELL_SYSUTIL_ERROR_STATUS) ? "CELL_SYSUTIL_ERROR_STATUS" : ::rage::rlNpGetErrorString(x)

#define getDataTypeString(x) \
						(x == SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_THIN ? "SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_THIN" : \
						(x == SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL ? "SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL" : "UNKNOWN_DATA_TYPE"))
#define getPurchasabilityString(x) \
						(x == SCE_NP_COMMERCE2_SKU_PURCHASABILITY_FLAG_ON  ? "Can be purchased" : \
						(x == SCE_NP_COMMERCE2_SKU_PURCHASABILITY_FLAG_OFF ? "Cannot be purchased" : "Unknown Flag"))
#define getAnnotationString(x) \
						(x == SCE_NP_COMMERCE2_SKU_ANN_PURCHASED_CANNOT_PURCHASE_AGAIN ? "SCE_NP_COMMERCE2_SKU_ANN_PURCHASED_CANNOT_PURCHASE_AGAIN" : \
						(x == SCE_NP_COMMERCE2_SKU_ANN_PURCHASED_CAN_PURCHASE_AGAIN ? "SCE_NP_COMMERCE2_SKU_ANN_PURCHASED_CAN_PURCHASE_AGAIN" : \
						(x == SCE_NP_COMMERCE2_SKU_ANN_IN_THE_CART ? "SCE_NP_COMMERCE2_SKU_ANN_IN_THE_CART" : \
						(x == SCE_NP_COMMERCE2_SKU_ANN_CONTENTLINK_SKU ? "SCE_NP_COMMERCE2_SKU_ANN_CONTENTLINK_SKU" : "UNKNOWN_ANNOTATION" ))))

#if __NO_OUTPUT
void spewError(const int /*errorCode*/, const char* /*name*/) { }
#else
void spewError(const int errorCode, const char* name)
{
	switch (errorCode)
	{
	case SCE_NP_COMMERCE2_SERVER_ERROR_ACCESS_PERMISSION_DENIED:
		rlWarning("%s:: Failed (code=\"0x%08x\" name=\"%s\")", name, errorCode, getErrorName(errorCode));
		break;

	default:
		rlError("%s:: Failed (code=\"0x%08x\" name=\"%s\")", name, errorCode, getErrorName(errorCode));
	}
}
#endif // __NO_OUTPUT

CompileTimeAssert(MARKET_RECV_BUF_SIZE           == SCE_NP_COMMERCE2_RECV_BUF_SIZE);
CompileTimeAssert(MARKET_CURRENCY_CODE_LEN       == SCE_NP_COMMERCE2_CURRENCY_CODE_LEN);
CompileTimeAssert(MARKET_CURRENCY_SYMBOL_LEN     == SCE_NP_COMMERCE2_CURRENCY_SYMBOL_LEN);
CompileTimeAssert(MARKET_THOUSAND_SEPARATOR_LEN  == SCE_NP_COMMERCE2_THOUSAND_SEPARATOR_LEN);
CompileTimeAssert(MARKET_DECIMAL_LETTER_LEN      == SCE_NP_COMMERCE2_DECIMAL_LETTER_LEN);
CompileTimeAssert(sizeof(unsigned)              == sizeof(size_t));
CompileTimeAssert(sizeof(sMarketCellHttpsData)  == sizeof(CellHttpsData));
CompileTimeAssert(sizeof(sMarketSessionInfo)    == sizeof(SceNpCommerce2SessionInfo));


// --- Memory Allocation/DeAllocation ----------------------------------------------

//PURPOSE
// Does a logged allocate with the marketplace allocator.
template<typename T>
T*  TryLoggedAllocate(sysMemAllocator* allocator, const size_t len)
{
	Assert(allocator);

	T* t = 0;

	if (allocator)
	{
		t = (T*)allocator->TryLoggedAllocate(len, 0, 0, __FILE__, __LINE__);
		if(t)
		{
			for(unsigned i = 0; i < len; i++) 
			{
				new(&t[i]) T();
			}
		}
	}
	else
	{
		rlError("allocator is null, possibly shutdown? can't allocate");
	}

	return t;
}

//PURPOSE
// Deletes one or more objects allocated with TryLoggedAllocate<T>..
template<typename T>
bool  MemDeAllocate(sysMemAllocator* allocator, T* t, const unsigned count = 1)
{
	Assert(allocator);
	Assert(t);

	bool ret = false;

	if(t)
	{
		for(unsigned i = 0; i < count; ++i)
		{
			t[i].~T();
		}

		if (allocator)
		{
			ret = true;
			Assert(allocator->IsValidPointer(t));
			allocator->Free(t);
		}
	}

	return ret;
}


// --- RequestInfo ----------------------------------------------

void  
rlMarketplace::RequestInfo::Init(sysMemAllocator* allocator)
{
	if (AssertVerify(allocator))
	{
		m_Allocator = allocator;
		CreateBuffer();
	}
}


void 
rlMarketplace::RequestInfo::ClearBuffer()
{
	if (AssertVerify(m_Buffer))
	{
		sysMemSet(m_Buffer, 0, MARKET_RECV_BUF_SIZE);
	}
}

void  
rlMarketplace::RequestInfo::CreateBuffer()
{
	if (AssertVerify(m_Allocator) && AssertVerify(!m_Buffer))
	{
		OUTPUT_ONLY(u32 memAvail = m_Allocator->GetMemoryAvailable());

		m_Buffer = TryLoggedAllocate< u8 >(m_Allocator, MARKET_RECV_BUF_SIZE);
		if (AssertVerify(m_Buffer))
		{
			OUTPUT_ONLY(u32 memUsed = memAvail - m_Allocator->GetMemoryAvailable());
			OUTPUT_ONLY(if (0 < memUsed) rlDebug2("Mem used: %d", memUsed);)
		}
	}
}

void  
rlMarketplace::RequestInfo::DeleteBuffer()
{
	if (AssertVerify(m_Allocator) && AssertVerify(m_Buffer))
	{
		OUTPUT_ONLY(u32 memAvail = m_Allocator->GetMemoryAvailable());

		if(AssertVerify(m_Buffer))
		{
			MemDeAllocate< u8 >(m_Allocator, m_Buffer);
		}
		m_Buffer = 0;

		OUTPUT_ONLY(u32 memUsed = memAvail - m_Allocator->GetMemoryAvailable());
		OUTPUT_ONLY(if (0 < memUsed) rlDebug2("Mem used: %d", memUsed);)
	}
}

// --- XovOverlapped ----------------------------------------------

rlMarketplace::XovOverlapped::XovOverlapped()
: m_Event(0),
  m_ErrorCode(0),
  m_MyStatus(NULL)
{
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

	m_Event = 0;
	m_ErrorCode = 0;
	m_MyStatus = NULL;
}

void
rlMarketplace::XovOverlapped::Cancel( )
{
	if (rlVerify(Pending()))
	{
		m_Event = 0;
		m_ErrorCode = 0;
		m_MyStatus->SetCanceled();
		m_MyStatus = NULL;
	}
}

bool
rlMarketplace::XovOverlapped::Pending() const
{
	if (m_MyStatus)
		return (m_MyStatus && m_MyStatus->Pending());

	return false;
}

void
rlMarketplace::XovOverlapped::BeginOverlapped(netStatus* status)
{
	Assert(status);
	if (status)
	{
		m_MyStatus = status;
		m_MyStatus->SetPending();
		m_Event = 0;
		m_ErrorCode = 0;
	}
}

void
rlMarketplace::XovOverlapped::EndOverlapped(const int sysEvent, const int errorCode)
{
	Assert(m_MyStatus->Pending());
	m_Event = sysEvent;
	m_ErrorCode = errorCode;
}

bool
rlMarketplace::XovOverlapped::CheckExtendedError()
{
	rlError("Extended overllaped Error \"0x%08x\"", m_ErrorCode);
	return false;
}

bool
rlMarketplace::XovOverlapped::CheckOverlapped()
{
	if (rlVerify(m_MyStatus) && m_MyStatus->Pending() && 0 != m_Event)
	{
		switch(m_Event)
		{
		case SCE_NP_COMMERCE2_EVENT_REQUEST_ERROR:
			spewError(m_ErrorCode, "Event \"SCE_NP_COMMERCE2_EVENT_REQUEST_ERROR\"");
			m_MyStatus->SetFailed(m_ErrorCode);
		break;

		case SCE_NP_COMMERCE2_EVENT_CREATE_SESSION_DONE:
		case SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_STARTED:
		case SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_FINISHED:
		case SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_SUCCESS:
		case SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_BACK:
		case SCE_NP_COMMERCE2_EVENT_DO_DL_LIST_STARTED:
		case SCE_NP_COMMERCE2_EVENT_DO_DL_LIST_SUCCESS:
		case SCE_NP_COMMERCE2_EVENT_DO_DL_LIST_FINISHED:
		case SCE_NP_COMMERCE2_SERVER_ERROR_SESSION_EXPIRED:
			m_MyStatus->SetSucceeded(m_ErrorCode);
		break;

		case SCE_NP_COMMERCE2_EVENT_CREATE_SESSION_ABORT:
			m_MyStatus->SetCanceled();
		break;

		default:
			rlError("Invalid Event \"0x%08x\"", m_Event);
		}

		return true;
	}

	return false;
}

int
rlMarketplace::XovOverlapped::GetOverlappedResult()
{
	return m_Event;
}


// --- Public ----------------------------------------------------

rlMarketplace::rlMarketplace()
: m_IsInitialed(false),
  m_Offers(NULL),
  m_SessionCreated(false),
  m_Operation(OP_IDLE),
  m_CtxId(0),
  m_CheckoutContainer(SYS_MEMORY_CONTAINER_ID_INVALID),
  m_LastPage(0),
  m_Categories(NULL),
  m_Type(REQ_GET_OFFER),
  m_HttpPool(0),
  m_HttpCookiePool(0),
  m_SslPool(0),
  m_Allocator(0)
{
	memset(m_TopCategory, 0, MAX_TEXT_LENGTH);
	m_MyStatus.Reset();
}

void
rlMarketplace::Init(sysMemAllocator* allocator, const char* categories)
{
	rtry
	{
		rlDebug2("Init Store Browsing");

		rcheck(rlVerify(!m_IsInitialed), catchall, rlError("Init:: FAILED - Already initialized"));

		rcheck(rlVerify(categories), catchall, rlError("Init:: FAILED - Invalid Top Category (null)."));
		rcheck(rlVerify(allocator), catchall, rlError("Init:: FAILED - Invalid allocator (null)."));

		Assert(MAX_TEXT_LENGTH > strlen(categories));
		formatf(m_TopCategory, MAX_TEXT_LENGTH, categories);
		rlDebug2("... Set Top category \"%s\"", m_TopCategory);

		m_IsInitialed = true;
		m_MyStatus.Reset();
		m_XovStatus.Clear();

		m_Operation = OP_IDLE;

		m_Allocator = allocator;

		m_LastPage = 0;

		ClearOffers();
		ClearCategories();
	}
	rcatchall
	{
		Shutdown();
	}
}

void
rlMarketplace::Shutdown()
{
	ShutdownSession();

	ClearOffers();
	ClearCategories();

	m_IsInitialed = false;
}

bool
rlMarketplace::Cancel()
{
	Assert(m_MyStatus.Pending());

	int errorCode = 0;

	rtry
	{
		//Abort Session creation
		if (OP_CREATE_SESSION == m_Operation)
		{
			errorCode = sceNpCommerce2CreateSessionAbort(m_CtxId);
			rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2CreateSessionAbort"));

			rlWarning("Aborted creating a commerce 2 session.");
		}

		//Abort the communication processing for obtaining
		//  category content information and offer information.
		if (OP_REQUEST == m_Operation)
		{
			errorCode = sceNpCommerce2AbortReq(m_RequestInfo.m_Id);
			rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2AbortReq"));

			rlWarning("Aborted a commerce 2 request.");
		}

		if (m_MyStatus.Pending())
		{
			m_XovStatus.Cancel();
		}

		return true;
	}
	rcatchall
	{
	}

	return false;
}

void
rlMarketplace::Update()
{
	if (!m_IsInitialed &&
		!m_SessionCreated)
	{
		return;
	}

	// --------------------------------------------
	// I am assuming this being called else were.
	// --------------------------------------------
	//int errorCode = 0;
	//if (CELL_OK != (errorCode = cellSysutilCheckCallback()))
	//{
	//	rlError("cellSysutilCheckCallback:: FAILED (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));
	//}

#define HANDLER_EVENT_CASE(x) case x: rlDebug2("In Game Browsing Event \""#x"\"");
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
				break;

				HANDLER_STATE_CASE(OP_CREATE_SESSION)
				break;

				HANDLER_STATE_CASE(OP_CHECKOUT)
				break;

				HANDLER_STATE_CASE(OP_REDOWNLOAD)
				break;

				HANDLER_STATE_CASE(OP_REQUEST)
					switch (m_Type)
					{
						HANDLER_REQ_CASE(REQ_GET_OFFER)
							break;
						HANDLER_REQ_CASE(REQ_GET_CATEGORY)
							break;
					}
				break;
			}
#undef HANDLER_STATE_CASE
#undef HANDLER_REQ_CASE
		}
		break;

	case netStatus::NET_STATUS_SUCCEEDED:
	case netStatus::NET_STATUS_FAILED:
	case netStatus::NET_STATUS_CANCELED:
		{
			switch(m_XovStatus.GetOverlappedResult())
			{
			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_REQUEST_ERROR)
				if (OP_CHECKOUT == m_Operation)
				{
					rlError("An error occurred during checkout (code=\"0x%08x\" name=\"%s\")", m_MyStatus.GetResultCode(), getErrorName(m_MyStatus.GetResultCode()));
					FinishCheckOut();
					DestroyMemoryContainer();
				}
				else if (OP_REDOWNLOAD == m_Operation)
				{
					rlError("An error occurred during re-download (code=\"0x%08x\" name=\"%s\")", m_MyStatus.GetResultCode(), getErrorName(m_MyStatus.GetResultCode()));
					FinishReDownload();
					DestroyMemoryContainer();
				}
				else if (OP_CREATE_SESSION == m_Operation)
				{
					rlError("An error occurred during create session (code=\"0x%08x\" name=\"%s\")", m_MyStatus.GetResultCode(), getErrorName(m_MyStatus.GetResultCode()));
					FinishCreateSession();
				}
				m_Operation = OP_IDLE;
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_CREATE_SESSION_DONE)
				FinishCreateSession();
				m_Operation = OP_IDLE;
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_CREATE_SESSION_ABORT)
				m_Operation = OP_IDLE;
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_STARTED)
				m_MyStatus.Reset();
				m_XovStatus.BeginOverlapped(&m_MyStatus);
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_SUCCESS)
				FinishCheckOut();
				m_MyStatus.Reset();
				m_XovStatus.BeginOverlapped(&m_MyStatus);
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_FINISHED)
				DestroyMemoryContainer();
				m_MyStatus.Reset();
				m_XovStatus.Clear();
				m_Operation = OP_IDLE;
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_CHECKOUT_BACK)
				FinishCheckOut();
				m_MyStatus.Reset();
				m_XovStatus.BeginOverlapped(&m_MyStatus);
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_DL_LIST_STARTED)
				m_MyStatus.Reset();
				m_XovStatus.BeginOverlapped(&m_MyStatus);
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_DL_LIST_SUCCESS)
				FinishReDownload();
				m_MyStatus.Reset();
				m_XovStatus.BeginOverlapped(&m_MyStatus);
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_DO_DL_LIST_FINISHED)
				DestroyMemoryContainer();
				m_MyStatus.Reset();
				m_XovStatus.Clear();
				m_Operation = OP_IDLE;
				break;

			HANDLER_EVENT_CASE(SCE_NP_COMMERCE2_SERVER_ERROR_SESSION_EXPIRED)
				m_MyStatus.Reset();
				m_XovStatus.Clear();
				m_Operation = OP_IDLE;
				RecreateSession();
				break;

			default:
				rlError("Invalid Event \"0x%08x\"", m_XovStatus.GetOverlappedResult());
				m_MyStatus.Reset();
			}
		}
		break;

	case netStatus::NET_STATUS_NONE:
		break;
	}
#undef HANDLER_EVENT_CASE
}

void
rlMarketplace::InitSession()
{

	// In the following order:
	//  (1)  libnet module [initialize with sys_net_initialize_network() or sys_net_initialize_network_ex()]
	//  (2)  SSL module [initialize with cellSslInit()]
	//  (3)  HTTPS module (initialize with cellHttpInit(), cellHttpInitCookie(), and cellHttpsInit(), and then call
	//       cellSslCertificateLoader() with the CELL_SSL_LOAD_CERT_ALL option to load the certificate)
	//  (4)  NP module [initialize with sceNpInit()]
	//  (5)  Following, call sceNpCommerce2Init() to initialize the NP IN-GAME commerce 2 utility.
	//       When initialization is successful, sceNpCommerce2Init() returns 0.

	int errorCode = 0;
	rtry
	{
		rcheck(rlVerify(!m_Allocator), catchall, rlError("Init:: FAILED - Null memory allocator."));

		OUTPUT_ONLY(u32 memAvail = m_Allocator->GetMemoryAvailable());

		rlDebug2("Init Store Session");

		rcheck(rlVerify(m_IsInitialed), catchall, rlError("Init:: FAILED - Already initialized"));

		rcheck(rlVerify(OP_IDLE == m_Operation), catchall, rlError("Init:: FAILED - Operation not IDLE."));

		rcheck(rlVerify(!m_SessionCreated), catchall, rlError("Init:: FAILED - Session is already created."));

		rcheck(rlVerify(SYS_MEMORY_CONTAINER_ID_INVALID == m_CheckoutContainer), catchall, rlError("Init:: FAILED - Checkout container is not INVALID."));

		m_RequestInfo.Init(m_Allocator);

		rcheck(rlVerify(!m_HttpPool), catchall, rlError("Init:: FAILED - Http Pool."));
		rcheck(rlVerify(!m_HttpCookiePool), catchall, rlError("Init:: FAILED - http cookie Pool."));
		rcheck(rlVerify(!m_SslPool), catchall, rlError("Init:: FAILED - Sssl Pool."));

		m_HttpPool = TryLoggedAllocate< u8 >(m_Allocator, HTTP_POOL_SIZE);
		rcheck(rlVerify(m_HttpPool), catchall, rlError("Init:: FAILED - Http Pool."));

		m_HttpCookiePool = TryLoggedAllocate< u8 >(m_Allocator, HTTP_COOKIE_POOL_SIZE);
		rcheck(rlVerify(m_HttpCookiePool), catchall, rlError("Init:: FAILED - http cookie Pool."));

		m_SslPool = TryLoggedAllocate< u8 >(m_Allocator, SSL_POOL_SIZE);
		rcheck(rlVerify(m_SslPool), catchall, rlError("Init:: FAILED - Sssl Pool."));

		m_MyStatus.Reset();
		m_XovStatus.Clear();

		m_Operation = OP_IDLE;

		m_LastPage = 0;

		rcheck(LoadModule(), catchall, rlError("LoadModule:: FAILED"));

		m_SessionCreated = true;

		// This is already set up elsewhere, and sys_net_initialize_network is a *macro* that eats 128k every time you call it.
		/* errorCode = sys_net_initialize_network();
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sys_net_initialize_network"));

		errorCode = cellNetCtlInit(); // CELL_NET_CTL_ERROR_NOT_TERMINATED - Rage might have already done this
		rcheck(0 <= errorCode || static_cast<int>(CELL_NET_CTL_ERROR_NOT_TERMINATED) == errorCode, catchall, spewError(errorCode, "cellNetCtlInit")); */

		memset(m_HttpPool, 0, HTTP_POOL_SIZE);
		errorCode = cellHttpInit(m_HttpPool, HTTP_POOL_SIZE);
		rcheck(0 <= errorCode, catchall,  spewError(errorCode, "cellHttpInit"));

		memset(m_HttpCookiePool, 0, HTTP_COOKIE_POOL_SIZE);
		errorCode = cellHttpInitCookie(m_HttpCookiePool, HTTP_COOKIE_POOL_SIZE);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "cellHttpInitCookie"));

		// Load ssl certifiers
		rcheck(LoadSslCerticates(), catchall, );

		memset(m_SslPool, 0, SSL_POOL_SIZE);
		errorCode = cellSslInit(m_SslPool, SSL_POOL_SIZE);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "cellSslInit"));

		errorCode = cellHttpsInit(1, (CellHttpsData*)&m_caList);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "cellHttpsInit"));

		// Initialize NP - Note we assume NP2 is already initialized (rlnp.cpp/h) ... if not them it has to also be terminated in the shutdown.
		//errorCode = sceNp2Init(sizeof(s_gNpPool), s_gNpPool);
		//rcheck(/*0 <= errorCode || */static_cast<int>(SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED), catchall, spewError(errorCode, "sceNp2Init"));

		// Initialize the NP IN-GAME commerce 2 utility
		errorCode = sceNpCommerce2Init();
		rcheck(0 <= errorCode || static_cast<int>(SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED) == errorCode, catchall, spewError(errorCode, "sceNpCommerce2Init"));

		SceNpId npId;
		errorCode = sceNpManagerGetNpId(&npId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpManagerGetNpId"));

		errorCode = sceNpCommerce2CreateCtx(SCE_NP_COMMERCE2_VERSION, &npId, Commerce2Handler, &m_XovStatus, &m_CtxId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2CreateCtx"));

		rlDebug2("%s: ctxId = %u", __FUNCTION__, m_CtxId);
		rlDebug2("NP ID of user creating the context: \"%s\"", npId.handle.data);

		rcheck(rlVerify(CreateSession()), catchall, );

		OUTPUT_ONLY(u32 memUsed = memAvail - m_Allocator->GetMemoryAvailable());
		OUTPUT_ONLY(if (0 < memUsed) rlDebug2("Mem used: %d", memUsed);)
	}
	rcatchall
	{
		ShutdownSession();
	}
}

void
rlMarketplace::ShutdownSession()
{
	Assert(m_IsInitialed);
	Assert(!m_MyStatus.Pending());

	m_Operation = OP_IDLE;

	if (m_MyStatus.Pending())
	{
		Cancel();
	}
	m_MyStatus.Reset();

	if (m_SessionCreated && rlVerify(m_Allocator))
	{
		OUTPUT_ONLY(u32 memAvail = m_Allocator->GetMemoryAvailable());

		ASSERT_ONLY(int errorCode = ) sceNpCommerce2DestroyCtx(m_CtxId);
		Assertf(CELL_OK == errorCode, "sceNpCommerce2DestroyCtx:: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));

		cellHttpsEnd();

		cellSslEnd();

		UnLoadSslCerticates();

		cellHttpEndCookie();

		cellHttpEnd();

		// Dont do it ... Controlled else were
		//cellNetCtlTerm();

		// Dont do it ... Controlled else were
		//sys_net_finalize_network();

		//Terminate the NP IN-GAME commerce 2 utility
		ASSERT_ONLY(errorCode = )sceNpCommerce2Term();
		Assert(CELL_OK == errorCode);

		//Terminate NP - We dont want to do that ... We assume it is being controlled else were (rlnp.cpp/h)
		//errorCode = sceNp2Term();
		//rlVerify(CELL_OK == errorCode /*|| static_cast<int>(SCE_NP_ERROR_NOT_INITIALIZED) == errorCode*/);

		UnLoadModule();

		// Did we FuckUp on the memory container???
		if (SYS_MEMORY_CONTAINER_ID_INVALID != m_CheckoutContainer)
		{
			sys_memory_info info;
			int errorCode = sys_memory_container_get_size(&info, m_CheckoutContainer);
			if (0 != errorCode)
			{
				rlError("sys_memory_container_get_size:: FAILED code=\"0x%08x\"", errorCode);
			}
			else
			{
				rlDebug3("-----------------------------");
				rlDebug2("Memory container:");
				rlDebug2("......... Used: \"%u\"", info.total_user_memory);
				rlDebug2(".... Available: \"%u\"", info.available_user_memory);
				rlDebug3("-----------------------------");

				if (0 < info.total_user_memory)
				{
					DestroyMemoryContainer();
				}
			}
		}

		Assert(m_HttpPool);
		if(m_HttpPool)
		{
			MemDeAllocate< u8 >(m_Allocator, m_HttpPool);
		}
		m_HttpPool = 0;

		Assert(m_HttpCookiePool);
		if(m_HttpCookiePool)
		{
			MemDeAllocate< u8 >(m_Allocator, m_HttpCookiePool);
		}
		m_HttpCookiePool = 0;

		Assert(m_SslPool);
		if(m_SslPool)
		{
			MemDeAllocate< u8 >(m_Allocator, m_SslPool);
		}
		m_SslPool = 0;

		m_RequestInfo.Shutdown();

		OUTPUT_ONLY(u32 memUsed = memAvail - m_Allocator->GetMemoryAvailable());
		OUTPUT_ONLY(if (0 < memUsed) rlDebug2("Mem used: %d", memUsed);)
	}

	m_SessionCreated = false;
}


bool
rlMarketplace::StartCheckout(const char* offerIds[], const unsigned offersCount)
{
	int errorCode = 0;
	bool overlapped = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("StartCheckout:: Pending Operation \"%d\"", m_Operation));

		rcheck(offerIds, catchall, rlError("(null) offers"));

		rcheck(0 < offersCount && offersCount <= GetOffersCount()/*GetNumDownloadableProducts()*/, catchall, rlError("Invalid number of offers \"%d\". There are only \"%u\" offers for download.", offersCount, GetOffersCount()));

		rcheck(offersCount <= SCE_NP_COMMERCE2_SKU_CHECKOUT_MAX, catchall, rlError("The maximum number of SKU IDs that can be specified is SCE_NP_COMMERCE2_SKU_CHECKOUT_MAX (=%u).", SCE_NP_COMMERCE2_SKU_CHECKOUT_MAX));

		char skuId[SCE_NP_COMMERCE2_SKU_CHECKOUT_MAX][256];

		unsigned count = 0;
		rlMarketplaceOfferConstIt it = GetConstOfferIterator();
		for(const OfferInfo* offer = it.Current(); offer && count < offersCount; offer = it.Next())
		{
			if ((0 == strcmp(offer->GetOfferId(), &offerIds[count][0])) &&
				rlVerify(strlen(m_TopCategory) + strlen(offer->GetOfferId()) + strlen(offer->GetSkuId()) < 256) &&
				rlVerify(!offer->GetWasPurchased()) &&
				rlVerify(offer->GetIsDownloadable()))
			{
				memset(skuId[count], 0, 256);
				strcat(skuId[count], m_TopCategory);
				strcat(skuId[count], offer->GetOfferId());
				strcat(skuId[count], offer->GetSkuId());

				count++;
			}
		}
		rcheck(offersCount == count, catchall, rlError("Number of offers \"%u\" do not match to the number counted \"%u\"", offersCount, count));

		const char* SkuIds[] = {  &skuId[0][0],  &skuId[1][0],  &skuId[2][0],  &skuId[3][0],
								  &skuId[4][0],  &skuId[5][0],  &skuId[6][0],  &skuId[7][0],
								  &skuId[8][0],  &skuId[9][0],  &skuId[10][0], &skuId[11][0],
								  &skuId[12][0], &skuId[13][0], &skuId[14][0], &skuId[15][0]
							   };

		m_Operation = OP_CHECKOUT;
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		rcheck(CreateMemoryContainer(SCE_NP_COMMERCE2_DO_CHECKOUT_MEMORY_CONTAINER_SIZE), catchall, rlError("CreateMemoryContainer:: FAILED"));

		errorCode = sceNpCommerce2DoCheckoutStartAsync(m_CtxId, SkuIds, offersCount, m_CheckoutContainer);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2DoCheckoutStartAsync"));

		return true;
	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped(0, errorCode);
			m_MyStatus.SetFailed();
		}
	}

	return false;
}

bool
rlMarketplace::StartReDownload(const char* offerIds[], const unsigned offersCount)
{
	int errorCode = 0;
	bool overlapped = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("StartReDownload:: Pending Operation \"%d\"", m_Operation));

		rcheck(offerIds, catchall, rlError("(null) offers"));

		rcheck(0 < offersCount && offersCount <= GetOffersCount(), catchall, rlError("Invalid number of offers \"%d\". There are only \"%u\" offers for download.", offersCount, GetOffersCount()));

		rcheck(offersCount <= SCE_NP_COMMERCE2_SKU_DL_LIST_MAX, catchall, rlError("The maximum number of SKU IDs that can be specified is SCE_NP_COMMERCE2_SKU_DL_LIST_MAX (=%u).", SCE_NP_COMMERCE2_SKU_DL_LIST_MAX));

		char skuId[SCE_NP_COMMERCE2_SKU_DL_LIST_MAX][256];

		unsigned count = 0;
		rlMarketplaceOfferConstIt it = GetConstOfferIterator();
		for(const OfferInfo* offer = it.Current(); offer && count < offersCount; offer = it.Next())
		{
			if (rlVerify(strlen(m_TopCategory) + strlen(offer->GetOfferId()) + strlen(offer->GetSkuId()) < 256) &&
				rlVerify(offer->GetWasPurchased()) &&
				rlVerify(!offer->GetIsDownloadable()) &&
				(0 == strcmp(offer->GetOfferId(), &offerIds[count][0])))
			{
				memset(skuId[count], 0, 256);
				strcat(skuId[count], m_TopCategory);
				strcat(skuId[count], offer->GetOfferId());
				strcat(skuId[count], offer->GetSkuId());

				count++;
			}
		}
		rcheck(offersCount == count, catchall, rlError("Number of offers \"%u\" do not match to the number counted \"%u\"", offersCount, count));

		const char* SkuIds[] = {  &skuId[0][0],  &skuId[1][0],  &skuId[2][0],  &skuId[3][0],
                                  &skuId[4][0],  &skuId[5][0],  &skuId[6][0],  &skuId[7][0],
                                  &skuId[8][0],  &skuId[9][0],  &skuId[10][0], &skuId[11][0],
                                  &skuId[12][0], &skuId[13][0], &skuId[14][0], &skuId[15][0]
                               };

		m_Operation = OP_REDOWNLOAD;
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		rcheck(CreateMemoryContainer(SCE_NP_COMMERCE2_DO_DL_LIST_MEMORY_CONTAINER_SIZE), catchall, rlError("CreateMemoryContainer:: FAILED"));

		errorCode = sceNpCommerce2DoDlListStartAsync(m_CtxId, NULL, SkuIds, offersCount, m_CheckoutContainer);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2DoDlListStartAsync"));

	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped(0, errorCode);
			m_MyStatus.SetFailed();
		}
	}

	return false;
}

bool
rlMarketplace::GetBackgroundDownloadStatus() const
{
	int errorCode = 0;

	rtry
	{
		rcheck(m_SessionCreated, catchall, rlError("Session not started."));

		bool bgdlAvailability;

		errorCode = sceNpCommerce2GetBGDLAvailability(&bgdlAvailability);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetBGDLAvailability"));

		rlDebug2("Get Background download setting - \"%s\".", (bgdlAvailability ? "Enabled" : "Disabled"));

		return bgdlAvailability;
	}
	rcatchall
	{

	}

	return false;
}

void
rlMarketplace::SetBackgroundDownloadStatus(const bool bgdlAvailability)
{
	int errorCode = 0;

	rtry
	{
		errorCode = sceNpCommerce2SetBGDLAvailability(bgdlAvailability);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2SetBGDLAvailability"));

		rlDebug2("Set Background download setting - \"%s\".", (bgdlAvailability ? "Enabled" : "Disabled"));
	}
	rcatchall
	{
	}
}


// --- Private ----------------------------------------------------

bool
rlMarketplace::NativeEnumerateOffers(const bool offersOnly)
{
	Assert('\0' != m_TopCategory[0]);
	Assert(m_SessionCreated);

	if (m_MyStatus.Pending())
	{
		rlError("NativeEnumerateOffers:: Failed - Pending operations \"%u\"", m_Operation);
		return false;
	}

	m_Operation = OP_REQUEST;
	m_XovStatus.BeginOverlapped(&m_MyStatus);

	if (!offersOnly)
	{
		// Retrieve Top category items (they should be only categories)
		GetCategoryContentResults(m_TopCategory);

		// Retrieve Categories
		CategoryInfo* pCategory = m_Categories;
		while (pCategory && rlVerify(strlen(m_TopCategory) + strlen(pCategory->GetId()) < 256))
		{
			if (pCategory->GetEnumerate())
			{
				char concatenatedName[256];
				memset(concatenatedName, 0, 256);

				strcat(concatenatedName, m_TopCategory);
				strcat(concatenatedName, pCategory->GetId());

				GetCategoryContentResults(concatenatedName);
			}

			pCategory = pCategory->Next() ? pCategory->Next() : NULL;
			if (!pCategory && GetNeedsEnumeration())
			{
				pCategory = m_Categories;
			}
		}
	}

	SpewCategoriesInfo();

	// Retrieve Offers
	OfferInfo* pOffer = m_Offers;
	while (pOffer && rlVerify(strlen(m_TopCategory) + strlen(pOffer->GetOfferId()) < 256))
	{
		char concatenatedName[256];
		memset(concatenatedName, 0, 256);
		strcat(concatenatedName, m_TopCategory);
		strcat(concatenatedName, pOffer->GetOfferId());

		GetProductContentResults(concatenatedName, pOffer);

		pOffer = pOffer->Next() ? pOffer->Next() : NULL;
	}

	SpewOffersInfo();

	m_Operation = OP_IDLE;
	m_XovStatus.Clear();
	m_MyStatus.Reset();

	return true;
}

bool
rlMarketplace::NativeEnumerateAssets()
{
	Assert(0);

	return false;
}

void
rlMarketplace::Commerce2Handler(u32 /*ctx_id*/, u32 /*subject_id*/, int sysEvent, int errorCode, void *arg)
{
	((XovOverlapped*)arg)->EndOverlapped(sysEvent, errorCode);
}

bool
rlMarketplace::LoadModule()
{
	int errorCode = 0;

	rtry
	{
		rlDebug2("Loading module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2...");

		if(cellSysmoduleIsLoaded(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2) != CELL_OK)
		{
			errorCode = cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2);
			rcheck(CELL_OK == errorCode, catchall, spewError(errorCode, "cellSysmoduleLoadModule \"CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2\""));

			rlDebug2("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 Loaded...");
		}
		else
		{
			rlDebug2("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 already Loaded...");
		}

		return true;
	}
	rcatchall
	{

	}

	return false;
}

bool
rlMarketplace::UnLoadModule()
{
	int errorCode = 0;

	rtry
	{
		rlDebug2("UnLoading module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2...");

		if(cellSysmoduleIsLoaded(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2) == CELL_OK)
		{
			errorCode = cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2);
			rcheck(CELL_OK == errorCode, catchall, spewError(errorCode, "cellSysmoduleUnloadModule \"CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2\""));

			rlDebug2("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 UnLoaded.");
		}
		else
		{
			rlDebug2("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 is not loaded.");
		}

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool
rlMarketplace::LoadSslCerticates()
{
	rtry
	{
		char *buf = NULL;
		size_t size = 0;
		int errorCode = 0;

		errorCode = cellSslCertificateLoader(CELL_SSL_LOAD_CERT_ALL, NULL, 0, &size);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "cellSslCertificateLoader"));

		memset(&m_caList, 0, sizeof(m_caList));
		m_caList.ptr = rage_new char[size];
		buf = (char*)(m_caList.ptr);
		m_caList.size = size;

		rlDebug2("Array of data buffers containing PEM formatted CA certificates has size \"%u\"", size);

		errorCode = cellSslCertificateLoader(CELL_SSL_LOAD_CERT_ALL, buf, size, NULL);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "cellSslCertificateLoader"));

		return true;
	}
	rcatchall
	{
		UnLoadSslCerticates();
	}

	return false;
};


void
rlMarketplace::UnLoadSslCerticates()
{
	if (m_caList.ptr)
	{
		delete [] m_caList.ptr;
		m_caList.ptr = 0;
	}
};

void
rlMarketplace::RecreateSession()
{
	int errorCode = 0;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("RecreateSession:: Invalid operation status (\"%d\")", m_Operation));

		errorCode = sceNpCommerce2DestroyCtx(m_CtxId);
		Assertf(CELL_OK == errorCode, "sceNpCommerce2DestroyCtx:: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));

		SceNpId npId;
		errorCode = sceNpManagerGetNpId(&npId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpManagerGetNpId"));

		errorCode = sceNpCommerce2CreateCtx(SCE_NP_COMMERCE2_VERSION, &npId, Commerce2Handler, &m_XovStatus, &m_CtxId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2CreateCtx"));

		rlDebug2("%s: ctxId = %u", __FUNCTION__, m_CtxId);
		rlDebug2("NP ID of user creating the context: \"%s\"", npId.handle.data);

		rcheck(rlVerify(CreateSession()), catchall, rlError("CreateSession:: FAILED "));
	}
	rcatchall
	{
	}
}

bool
rlMarketplace::CreateSession()
{
	int errorCode = 0;
	bool overlapped = false;

	rtry
	{
		rcheck(!m_MyStatus.Pending(), catchall, rlError("CreateSession:: Invalid operation status (\"%d\")", m_Operation));

		m_Operation = OP_CREATE_SESSION;
		m_XovStatus.BeginOverlapped(&m_MyStatus);
		overlapped = true;

		errorCode = sceNpCommerce2CreateSessionStart(m_CtxId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2CreateSessionStart"));

		rlDebug2("In Game Browsing - Start Operation \"OP_CREATE_SESSION\"");

		return true;
	}
	rcatchall
	{
		if (overlapped)
		{
			m_XovStatus.EndOverlapped(0, errorCode);
			m_MyStatus.SetFailed();
		}
	}

	return false;
}

bool
rlMarketplace::FinishCreateSession()
{
	int errorCode = 0;

	rtry
	{
		errorCode = sceNpCommerce2CreateSessionFinish(m_CtxId, (SceNpCommerce2SessionInfo*)&m_SessionInfo);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2CreateSessionFinish"));

		rlDebug2("Create Session Ended");

		return true;
	}
	rcatchall
	{

	}

	return false;
}

bool
rlMarketplace::GetCategoryContentResults(const char* pCategoryId)
{
	int errorCode = 0;
	SceNpCommerce2GetCategoryContentsResult contentsResult;

	rtry
	{
		rcheck(rlVerify(pCategoryId), catchall, rlError("Null category ID"));

		rcheck(MAX_PAGE_CONTENT <= SCE_NP_COMMERCE2_GETCAT_MAX_COUNT, catchall, spewError(errorCode, "GetCategoryContentResults"));

		//rcheck(!m_MyStatus.Pending(), catchall, rlError("GetCategoryContentResults:: Pending Operation \"%d\"", m_Operation));

		m_Type = REQ_GET_CATEGORY;

		m_RequestInfo.m_CategoryId = pCategoryId;
		m_RequestInfo.ClearBuffer();
		m_RequestInfo.m_BufferSize = sizeof(m_RequestInfo.m_Buffer);

		size_t revdSize;
		SceNpCommerce2CategoryInfo categoryInfo;
		SceNpCommerce2ContentInfo contentInfo;

		// Create a commerce 2 request for obtaining category content data.
		errorCode = sceNpCommerce2GetCategoryContentsCreateReq(m_CtxId, &m_RequestInfo.m_Id);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetCategoryContentsCreateReq"));

		// Start obtaining category content data. This function is blocking. It issues a request to
		// the NP server and does not return until a response is received or an error (timeout, for example)
		// occurs within the internally-used libhttp.
		errorCode = sceNpCommerce2GetCategoryContentsStart(m_RequestInfo.m_Id, m_RequestInfo.m_CategoryId, m_LastPage, MAX_PAGE_CONTENT);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetCategoryContentsStart"));

		// Obtains category content data. This function is blocking. It does not return until
		// it receives category content data from the NP server or an error (timeout, for example)
		// occurs within the internally-used libhttp.
		errorCode = sceNpCommerce2GetCategoryContentsGetResult(m_RequestInfo.m_Id, m_RequestInfo.m_Buffer, m_RequestInfo.m_BufferSize, &revdSize);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetCategoryContentsGetResult"));

		// Initialize category content data
		errorCode = sceNpCommerce2InitGetCategoryContentsResult(&contentsResult, &m_RequestInfo.m_Buffer, revdSize);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2InitGetCategoryContentsResult"));

		// Take out category information
		errorCode = sceNpCommerce2GetCategoryInfo(&contentsResult, &categoryInfo);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetCategoryInfo"));

		rlDebug3("------------------------------------------------------------------");
		rlDebug3("Request Info:");
		rlDebug3("......................... Type: \"%s\"", (m_Type == REQ_GET_CATEGORY ? "REQ_GET_CATEGORY" : "REQ_GET_PRODUCT"));
		rlDebug3("........................... ID: \"%d\"", m_RequestInfo.m_Id);
		rlDebug3(".................. Category ID: \"%s\"", m_RequestInfo.m_CategoryId);
		rlDebug3("..................... Offer ID: \"%s\"", m_RequestInfo.m_OfferId);
		//rlDebug3("......................... Size: \"%u\"", m_RequestInfo.m_BufferSize);
		rlDebug3(" ");
		rlDebug3("Category Contents Results:");
		rlDebug3(".................... Start POS: \"%u\"", contentsResult.rangeOfContents.startPosition);
		rlDebug3("........................ Count: \"%u\"", contentsResult.rangeOfContents.count);
		rlDebug3("....... Total Count Of Results: \"%u\"", contentsResult.rangeOfContents.totalCountOfResults);
		rlDebug3(" ");
		rlDebug3("Category Info:");
		rlDebug3("........................... ID: \"%s\"", categoryInfo.categoryId);
		//rlDebug3("......................... Name: \"%s\"", categoryInfo.categoryName);
		//rlDebug3(".................. Description: \"%s\"", categoryInfo.categoryDescription);
		//rlDebug3(".................... Image URL: \"%s\"", categoryInfo.imageUrl);
		rlDebug3("........... countOfSubCategory: \"%u\"", categoryInfo.countOfSubCategory);
		rlDebug3("............... countOfProduct: \"%u\"", categoryInfo.countOfProduct);
		rlDebug3("------------------------------------------------------------------");

		const int topCategoryIdSize = strlen(m_TopCategory);
		SetCountOfOffers(&pCategoryId[topCategoryIdSize], categoryInfo.countOfProduct);

		const unsigned count = contentsResult.rangeOfContents.count;
		for (unsigned i=0; i<count; i++)
		{
			// Take out information of content in the category
			errorCode = sceNpCommerce2GetContentInfo(&contentsResult, i, &contentInfo);
			rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetContentInfo"));

			rlDebug3("------------------------------------------------------------------");
			rlDebug3("Content Info:");
			rlDebug3(".............. Index: \"%d\"", i);
			rlDebug3("....... Content Type: \"%s\"", (SCE_NP_COMMERCE2_CONTENT_TYPE_CATEGORY == contentInfo.contentType ? "SCE_NP_COMMERCE2_CONTENT_TYPE_CATEGORY" : (SCE_NP_COMMERCE2_CONTENT_TYPE_PRODUCT == contentInfo.contentType ? "SCE_NP_COMMERCE2_CONTENT_TYPE_PRODUCT" : "UNKNOWN")));

			// If it a category them extract the category ID and their offer one by one
			if (SCE_NP_COMMERCE2_CONTENT_TYPE_CATEGORY == contentInfo.contentType)
			{
				SceNpCommerce2CategoryInfo subCategoryInfo;

				// Take out subcategory information
				//    -
				errorCode = sceNpCommerce2GetCategoryInfoFromContentInfo(&contentInfo, &subCategoryInfo);
				rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetCategoryInfoFromContentInfo"));

				rlDebug3(" ");
				rlDebug3("Category Info:");
				rlDebug3("........................... ID: \"%s\"", subCategoryInfo.categoryId);
				rlDebug3("......................... Name: \"%s\"", subCategoryInfo.categoryName);
				//rlDebug3(".................. Description: \"%s\"", subCategoryInfo.categoryDescription);
				//rlDebug3(".................... Image URL: \"%s\"", subCategoryInfo.imageUrl);
				//rlDebug3("....................... spName: \"%s\"", subCategoryInfo.spName);
				rlDebug3("........... countOfSubCategory: \"%u\"", subCategoryInfo.countOfSubCategory);
				rlDebug3("............... countOfProduct: \"%u\"", subCategoryInfo.countOfProduct);
				rlDebug3("------------------------------------------------------------------");

				CategoryInfo cat;
				cat.SetId(&subCategoryInfo.categoryId[topCategoryIdSize]);
				cat.SetParentId(&pCategoryId[topCategoryIdSize]);
				cat.SetCountOfProducts(subCategoryInfo.countOfProduct);

				if (!CategoryExists(cat.GetId()))
				{
					CategoryInfo* newCategory = rage_new CategoryInfo();
					newCategory->Copy(cat);
					AddCategory(newCategory);
				}
				else
				{
					rlMarketplaceCategoryIt it = GetCategoryIterator();
					for(CategoryInfo* category = it.Current(); category; category = it.Next())
					{
						if (0 == strcmp(cat.GetId(), category->GetId()))
						{
							category->Copy(cat);
							break;
						}
					}
				}
			}
			// If it is a offer them extract the offers info One by one
			else if (SCE_NP_COMMERCE2_CONTENT_TYPE_PRODUCT == contentInfo.contentType)
			{
				SceNpCommerce2GameProductInfo productInfo;

				// Take out offer information
				//     - This offer info has dataType set to SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_THIN.
				errorCode = sceNpCommerce2GetGameProductInfoFromContentInfo(&contentInfo, &productInfo);
				rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetGameProductInfoFromContentInfo"));

				rlDebug3(" ");
				rlDebug3("Offer Info:");
				rlDebug3("................ ID: \"%s\"", productInfo.productId);
				rlDebug3("........ countOfSku: \"%u\"", productInfo.countOfSku);
				//rlDebug3(".............. Name: \"%s\"", productInfo.productName); // Its in UTF-8
				//rlDebug3("....... Description: \"%s\"", productInfo.productShortDescription); // Its in UTF-8
				//rlDebug3("......... Image URL: \"%s\"", productInfo.imageUrl);
				//rlDebug3("......... Long Desc: \"%s\"", productInfo.productLongDescription); // Its in UTF-8
				//rlDebug3("........ Legal Desc: \"%s\"", productInfo.legalDescription);
				rlDebug3("------------------------------------------------------------------");

				// convert the char back to char16 and copy it
				OfferInfo offer;

				if (productInfo.productName)
					offer.SetName(productInfo.productName);
				if (productInfo.productShortDescription)
					offer.SetShortDesc(productInfo.productShortDescription);
				offer.SetOfferId(&productInfo.productId[topCategoryIdSize]);
				offer.SetCategoryId(&pCategoryId[topCategoryIdSize]);
				offer.SetIsInstaled(false);

				// Take out SKU information from game product information
				if (rlVerify(0 < productInfo.countOfSku))
				{
					SceNpCommerce2GameSkuInfo skuInfo;

					errorCode = sceNpCommerce2GetGameSkuInfoFromGameProductInfo(&productInfo,
																				NP_SKU_PRODUCT_ID/*Index number of SKU to take out ... region specific*/,
																				&skuInfo);
					rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetGameSkuInfoFromGameProductInfo"));

					rlDebug3("SKU Info:");
					rlDebug3("........................ ID: \"%s\"", skuInfo.skuId);
					rlDebug3("................. Data Type: \"%s\"", getDataTypeString(skuInfo.dataType));
					//rlDebug3(".... Count until Expiration: \"%u\"", skuInfo.countUntilExpiration);
					rlDebug3("....... Purchasability Flag: \"%s\"", getPurchasabilityString(skuInfo.purchasabilityFlag));
					//rlDebug3("................ Annotation: \"%s\" \"0x%08x\"", getAnnotationString(skuInfo.annotation), skuInfo.annotation);
					rlDebug3(".............. Downloadable: \"%s\"", skuInfo.downloadable ? "TRUE" : "FALSE");

					// If the currency uses a decimal point as indicated by the positive value of decimals in the
					//    SceNpCommerce2SessionInfo structure, the value stored to price includes the numbers past
					//    the decimal point, and the application must move the decimal point as necessary.
					//    For example, in the US store, decimals is 2. If price is 399, this indicates $3.99.
					rlDebug3("..................... Price: \"%u\"", skuInfo.price);
					//rlDebug3("...................... Name: \"%s\"", skuInfo.skuName);

					// Valid only when dataType is SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL
					rlDebug3(".................. Offer ID: \"%s\"", skuInfo.productId);

					// Valid only when dataType is SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL
					//rlDebug3(".......... Content Link URL: \"%s\"", skuInfo.contentLinkUrl);
					rlDebug3("------------------------------------------------------------------");

					const int idSize = strlen(&productInfo.productId[topCategoryIdSize]);
					offer.SetPrice(skuInfo.price);
					offer.SetWasPurchased(!static_cast<bool>(skuInfo.purchasabilityFlag));
					offer.SetIsDownloadable(skuInfo.downloadable);
					offer.SetSkuId(&skuInfo.skuId[topCategoryIdSize+idSize]);
				}

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
						if (0 == strcmp(offer.GetOfferId(), auxOffer->GetOfferId()))
						{
							auxOffer->Copy(offer);
							break;
						}
					}
				}
			}
		}

		// Destroy category content data
		errorCode = sceNpCommerce2DestroyGetCategoryContentsResult(&contentsResult);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2DestroyGetCategoryContentsResult"));

		// Delete a commerce 2 request
		errorCode = sceNpCommerce2DestroyReq(m_RequestInfo.m_Id);
		rcheck(0 <= errorCode || static_cast<int>(SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND) == errorCode, catchall, spewError(errorCode, "sceNpCommerce2DestroyReq"));

		if (contentsResult.rangeOfContents.totalCountOfResults > m_LastPage)
		{
			m_LastPage += contentsResult.rangeOfContents.count;
			GetCategoryContentResults(pCategoryId);
		}
		else
		{
			m_LastPage = 0;
		}

		SetNeedsEnumeration(&pCategoryId[topCategoryIdSize], false);

		return true;
	}
	rcatchall
	{
		m_LastPage = 0;

		// Destroy category content data
		errorCode = sceNpCommerce2DestroyGetCategoryContentsResult(&contentsResult);
		Assertf(0 <= errorCode, "sceNpCommerce2DestroyGetCategoryContentsResult:: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));

		// Delete a commerce 2 request
		errorCode = sceNpCommerce2DestroyReq(m_RequestInfo.m_Id);
		Assertf(0 <= errorCode || static_cast<int>(SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND) == errorCode, "sceNpCommerce2DestroyReq:: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));
	}

	return false;
}

bool
rlMarketplace::GetProductContentResults(const char* pProductId, OfferInfo* pOffer)
{
	int errorCode = 0;
	SceNpCommerce2GetProductInfoResult contentsResult;

	rtry
	{
		rcheck(rlVerify('\0' != m_TopCategory[0]), catchall, rlError("Empty to category"));

		//rcheck(!m_MyStatus.Pending(), catchall, rlError("GetCategoryContentResults:: Pending Operation \"%d\"", m_Operation));

		rcheck(rlVerify(pProductId), catchall, rlError("NULL offer Id."));

		//rcheck(!OfferExists(pProductId), catchall, rlDebug3("Offer with ID \"%s\" already has the info downloaded", pProductId));

		SceNpCommerce2GetProductInfoResult contentsResult;
		SceNpCommerce2GameProductInfo productInfo;
		SceNpCommerce2GameSkuInfo skuInfo;

		m_Type = REQ_GET_OFFER;

		m_RequestInfo.m_CategoryId = m_TopCategory;
		m_RequestInfo.m_OfferId = pProductId;
		m_RequestInfo.ClearBuffer();
		m_RequestInfo.m_BufferSize = sizeof(m_RequestInfo.m_Buffer);

		size_t revdSize;

		// This function creates a commerce 2 request for obtaining product data.
		errorCode = sceNpCommerce2GetProductInfoCreateReq(m_CtxId, &m_RequestInfo.m_Id);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetProductInfoCreateReq"));

		// Start obtaining product data. This function is blocking.
		// It issues a request to the NP server and does not return until a response
		// is received or an error (timeout, for example) occurs within the internally-used libhttp.
		errorCode = sceNpCommerce2GetProductInfoStart(m_RequestInfo.m_Id, m_RequestInfo.m_CategoryId, m_RequestInfo.m_OfferId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetProductInfoStart"));

		// Obtains product data. This function is blocking. It does not return
		// until it receives product data from the NP server or an error (timeout, for example)
		// occurs within the internally-used libhttp.
		errorCode = sceNpCommerce2GetProductInfoGetResult(m_RequestInfo.m_Id, m_RequestInfo.m_Buffer, m_RequestInfo.m_BufferSize, &revdSize);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetProductInfoGetResult"));

		// Initialize product data
		errorCode = sceNpCommerce2InitGetProductInfoResult(&contentsResult, m_RequestInfo.m_Buffer, revdSize);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2InitGetProductInfoResult"));

		// Take out game product information
		errorCode = sceNpCommerce2GetGameProductInfo(&contentsResult, &productInfo);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetGameProductInfo"));

		// Take out SKU information from game product information
		if (rlVerify(0 < productInfo.countOfSku))
		{
			errorCode = sceNpCommerce2GetGameSkuInfoFromGameProductInfo(&productInfo,
																		NP_SKU_PRODUCT_ID/*Index number of SKU to take out ... region specific*/,
																		&skuInfo);
			rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2GetGameSkuInfoFromGameProductInfo"));
		}

		rlDebug3("------------------------------------------------------------------");
		rlDebug3("Product Info:");
		rlDebug3("................ ID: \"%s\"", productInfo.productId);
		rlDebug3("........ countOfSku: \"%u\"", productInfo.countOfSku);
		rlDebug3(".............. Name: \"%s\"", productInfo.productName);
		rlDebug3("....... Description: \"%s\"", productInfo.productShortDescription);
		rlDebug3("......... Image URL: \"%s\"", productInfo.imageUrl);
		rlDebug3("......... Long Desc: \"%s\"", productInfo.productLongDescription);

		//--- DOES NOT EXIST
		//rlDebug3("........ Legal Desc: \"%s\"", productInfo.legalDescription);

		rlDebug3(" ");
		rlDebug3("SKU Info:");
		rlDebug3("........................ ID: \"%s\"", skuInfo.skuId);
		rlDebug3("................. Data Type: \"%s\"", getDataTypeString(skuInfo.dataType));
		rlDebug3(".... Count until Expiration: \"%u\"", skuInfo.countUntilExpiration);
		rlDebug3("....... Purchasability Flag: \"%s\"", getPurchasabilityString(skuInfo.purchasabilityFlag));
		rlDebug3("................ Annotation: \"%s\" \"0x%08x\"", getAnnotationString(skuInfo.annotation), skuInfo.annotation);
		rlDebug3(".............. Downloadable: \"%s\"", skuInfo.downloadable ? "TRUE" : "FALSE");

		// If the currency uses a decimal point as indicated by the positive value of decimals in the
		//    SceNpCommerce2SessionInfo structure, the value stored to price includes the numbers past
		//    the decimal point, and the application must move the decimal point as necessary.
		//    For example, in the US store, decimals is 2. If price is 399, this indicates $3.99.
		rlDebug3("..................... Price: \"%u\"", skuInfo.price);
		rlDebug3("...................... Name: \"%s\"", skuInfo.skuName);

		// Valid only when dataType is SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL
		rlDebug3("................ Offer ID: \"%s\"", skuInfo.productId);

		// Valid only when dataType is SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL
		rlDebug3(".......... Content Link URL: \"%s\"", skuInfo.contentLinkUrl);
		rlDebug3("------------------------------------------------------------------");

		if (pOffer)
		{
			if (productInfo.productLongDescription)
				pOffer->SetLongDesc(productInfo.productLongDescription);

			pOffer->SetWasPurchased(!static_cast<bool>(skuInfo.purchasabilityFlag));
			pOffer->SetIsDownloadable(skuInfo.downloadable);
		}

		// Destroy offer information
		errorCode = sceNpCommerce2DestroyGetProductInfoResult(&contentsResult);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2DestroyGetProductInfoResult"));

		// Delete a commerce 2 request...
		errorCode = sceNpCommerce2DestroyReq(m_RequestInfo.m_Id);
		rcheck(0 <= errorCode || static_cast<int>(SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND) == errorCode, catchall,  spewError(errorCode, "sceNpCommerce2DestroyReq"));

		return true;
	}
	rcatchall
	{
		// Offer does not Exist
		if (static_cast<int>(SCE_NP_COMMERCE2_SERVER_ERROR_NO_SUCH_PRODUCT) == errorCode)
		{
			rlWarning("Product \"%s\" is not available", m_RequestInfo.m_OfferId);
		}

		// Destroy offer information
		errorCode = sceNpCommerce2DestroyGetProductInfoResult(&contentsResult);
		Assertf(0 <= errorCode, "sceNpCommerce2DestroyGetProductInfoResult:: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));

		// Delete a commerce 2 request...
		errorCode = sceNpCommerce2DestroyReq(m_RequestInfo.m_Id);
		Assertf(0 <= errorCode || static_cast<int>(SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND) == errorCode, "sceNpCommerce2DestroyReq:: Failed (code=\"0x%08x\" name=\"%s\")", errorCode, getErrorName(errorCode));
	}

	return false;
}

bool
rlMarketplace::FinishCheckOut()
{
	int errorCode = 0;

	rtry
	{
		errorCode = sceNpCommerce2DoCheckoutFinishAsync(m_CtxId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2DoCheckoutFinishAsync"));

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool
rlMarketplace::FinishReDownload()
{
	int errorCode = 0;

	rtry
	{
		errorCode = sceNpCommerce2DoDlListFinishAsync(m_CtxId);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sceNpCommerce2DoDlListFinishAsync"));

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool
rlMarketplace::CreateMemoryContainer(const unsigned size)
{
	int errorCode = 0;

	rtry
	{
		rcheck(SYS_MEMORY_CONTAINER_ID_INVALID == m_CheckoutContainer, catchall, spewError(errorCode, "Memory container invalid state - SYS_MEMORY_CONTAINER_ID_INVALID"));

		errorCode = sys_memory_container_create(&m_CheckoutContainer, size);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sys_memory_container_create"));

		rlWarning("CheckOut Memory container Created");

		return true;
	}
	rcatchall
	{
		if (ENOMEM == errorCode)
		{
			rlError("Memory of the specified size cannot be allocated from the default memory container");
		}
		else if (EAGAIN == errorCode)
		{
			rlError("Kernel memory shortage");
		}
		else if (EFAULT == errorCode)
		{
			rlError("An invalid address is specified");
		}

		if (SYS_MEMORY_CONTAINER_ID_INVALID != m_CheckoutContainer)
		{
			DestroyMemoryContainer();
		}
	}

	return false;
}

bool
rlMarketplace::DestroyMemoryContainer()
{
	int errorCode = 0;

	rtry
	{
		rcheck(SYS_MEMORY_CONTAINER_ID_INVALID != m_CheckoutContainer, catchall, spewError(errorCode, "Memory container invalid state - SYS_MEMORY_CONTAINER_ID_INVALID"));

		int errorCode = sys_memory_container_destroy(m_CheckoutContainer);
		rcheck(0 <= errorCode, catchall, spewError(errorCode, "sys_memory_container_destroy"));

		m_CheckoutContainer = SYS_MEMORY_CONTAINER_ID_INVALID;

		rlWarning("CheckOut Memory container Destroyed");

		return true;
	}
	rcatchall
	{
		if (ESRCH == errorCode)
		{
			rlError("The specified memory container is not found.");
		}
		else if (EBUSY == errorCode)
		{
			rlError("The specified memory container is being used");
		}
	}

	return false;
}

void
rlMarketplace::SetCountOfOffers(const char* pCategoryId, const unsigned count)
{
	if (pCategoryId && 0 < strlen(pCategoryId))
	{
		rlMarketplaceCategoryIt it = GetCategoryIterator();
		for(CategoryInfo* category = it.Current(); category; category = it.Next())
		{
			if (0 == strcmp(pCategoryId, category->GetId()))
			{
				category->SetCountOfProducts(count);
				return;
			}
		}

		rlError("Category \"%s\" not found.", pCategoryId);
	}
}

void
rlMarketplace::SetNeedsEnumeration(const char* pCategoryId, const bool bEnumerate)
{
	if (pCategoryId && 0 < strlen(pCategoryId))
	{
		rlMarketplaceCategoryIt it = GetCategoryIterator();
		for(CategoryInfo* category = it.Current(); category; category = it.Next())
		{
			if (0 == strcmp(pCategoryId, category->GetId()))
			{
				category->SetEnumerate(bEnumerate);
				return;
			}
		}

		rlError("Category \"%s\" not found.", pCategoryId);
	}
}

bool
rlMarketplace::GetNeedsEnumeration() const
{
	rlMarketplaceCategoryConstIt it = GetConstCategoryIterator();
	for(const CategoryInfo* category = it.Current(); category; category = it.Next())
	{
		if (category->GetEnumerate())
		{
			return true;
		}
	}

	return false;
}

void
rlMarketplace::SpewCategoriesInfo() const
{
	rlDebug3("----------------------------------------------");
	rlDebug3("Category Info:");
	rlDebug3("....... Total Number \"%u\"", GetCategoryCount());
	rlDebug3("----------------------------------------------");

	int iCount = 0;

	rlMarketplaceCategoryConstIt it = GetConstCategoryIterator();
	for(const CategoryInfo* category = it.Current(); category; category = it.Next())
	{
		rlDebug3(" ");
		rlDebug3("................... Index: \"%d\"", iCount);
		rlDebug3("............. Category Id: \"%s\"", category->GetId());
		rlDebug3("...... Parent Category Id: \"%s\"", category->GetParentId());
		rlDebug3("......... Count Of offers: \"%u\"", category->GetCountOfProducts());

		iCount++;
	}

	rlDebug3("----------------------------------------------");
}

} // namespace rage

#endif // __PPU

// EOF

