//
// NP_StoreBrowsing.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#if __PPU

// --- Include Files ------------------------------------------------------------

// C headers
#include <cell/sysmodule.h>
#include <np/commerce2.h>
#include <sysutil/sysutil_gamecontent.h>
#include <sysutil/sysutil_common.h>

// Rage headers
#include "rline/rl.h"
#include "diag/channel.h"
#include "diag/seh.h"
#include "diag/output.h"
#include "system/param.h"

// network headers
#include "NP_StoreBrowsing.h"
#include "fwnet/netchannel.h"
//#include "system/system.h"

// Game headers


// --- Defines ------------------------------------------------------------------
NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, np_store_browsing, DIAG_SEVERITY_DEBUG3)
#ifdef netDebug3
#undef netDebug3
#undef netError
#undef netWarning
#undef netAssertf
#undef netAssert
#undef netVerifyf
#undef netVerify
#endif

#define netDebug3(fmt, ...)       RAGE_DEBUGF3(net_np_store_browsing, fmt, ##__VA_ARGS__)
#define netError(fmt, ...)        RAGE_ERRORF(net_np_store_browsing, fmt, ##__VA_ARGS__)
#define netWarning(fmt, ...)      RAGE_WARNINGF(net_np_store_browsing, fmt, ##__VA_ARGS__)
#define netVerifyf(cond,fmt,...)  RAGE_VERIFYF(net_np_store_browsing, cond, fmt, ##__VA_ARGS__)
#define netVerify(cond)           RAGE_VERIFY(net_np_store_browsing, cond)
#define netAssertf(cond,fmt,...)  RAGE_ASSERTF(net_np_store_browsing, cond, fmt, ##__VA_ARGS__)
#define netAssert(cond)           RAGE_ASSERT(net_np_store_browsing, cond)

#ifdef getErrorName
#undef getErrorName
#endif
#define getErrorName(errorId) ::rage::rlNpGetErrorString(errorId)

#ifdef NP_SERVICE_ID
#undef NP_SERVICE_ID
#undef NP_CATEGORY_ID
#endif
#define NP_SERVICE_ID  "EP1017-BLES00229_00"
#define NP_CATEGORY_ID  NP_SERVICE_ID

static const int STORE_CALLBACK_SLOT = 1;

// --- Constants ----------------------------------------------------------------
// Pool for NP2
//static u8 s_gNpPool[SCE_NP_MIN_POOL_SIZE];


// --- Public -------------------------------------------------------------------

void
CNPStoreBrowsing::Init(const int iBrowserType)
{
	rtry
	{
		netDebug3("Starting Store Browsing");

		rcheck(LoadModule(), catchall, netError("LoadModule:: FAILED"));

		int iError;

		//**** Make sure no one else uses the same slot.
		iError = cellSysutilRegisterCallback(STORE_CALLBACK_SLOT, &CNPStoreBrowsing::SysutilCallback, this);
		rcheck(AssertVerify(CELL_OK == iError), catchall, netError("cellSysutilRegisterCallback:: FAILED (code=\"0x%08x\")", iError));

		m_bInitialized = true;

		// Initialize NP - Note we assume NP2 is already initialized (rlnp.cpp/h) ... if not them it has to also be terminated in the shutdown.
		//iError = sceNp2Init(sizeof(s_gNpPool), s_gNpPool);
		//rcheck(/*0 <= iError || */static_cast<int>(SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED), catchall, netError("sceNpInit:: FAILED (code=\"0x%08x\")", iError));

		// Initialize the NP IN-GAME commerce 2 utility
		iError = sceNpCommerce2Init();
		rcheck(0 <= iError || static_cast<int>(SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED) == iError, catchall, netError("sceNpCommerce2Init:: FAILED (code=\"0x%08x\")", iError));

		// Retrieve User Data..
		//int iUserdata;
		//unsigned uType, uAttributes;
		//iError = cellGameBootCheck(&uType, &uAttributes, NULL, NULL);
		//rcheck(0 <= iError, catchall, netError("cellGameBootCheck:: FAILED (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError)));
		//if (0 <= iError && uAttributes & CELL_GAME_ATTRIBUTE_COMMERCE2_BROWSER)
		//{
		//	iError = sceNpCommerce2GetStoreBrowseUserdata(&iUserdata);
		//	rcheck(0 < iError, catchall, netError("sceNpCommerce2GetStoreBrowseUserdata:: FAILED (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError)));
		//	netDebug3("Store Browse Userdata: \"0x%x\"", iUserdata);
		//}

		// Start the store browsing
		rcheck(AssertVerify(StartStoreBrowse(iBrowserType)), catchall, netError("StartStoreBrowse:: FAILED"));
	}
	rcatchall
	{
		Shutdown();
	}
}

void
CNPStoreBrowsing::Shutdown()
{
	if (m_bInitialized)
	{
		//Clear common event callback
		ASSERT_ONLY(int iError =) cellSysutilUnregisterCallback(STORE_CALLBACK_SLOT);
		Assert(CELL_OK == iError);

		//Terminate the NP IN-GAME commerce 2 utility
		ASSERT_ONLY(iError =) sceNpCommerce2Term();
		Assert(CELL_OK == iError);

		//Terminate NP - We dont want to do that ... We assume it is being controlled else were (rlnp.cpp/h)
		//iError = sceNp2Term();
		//AssertVerify(CELL_OK == iError /*|| static_cast<int>(SCE_NP_ERROR_NOT_INITIALIZED) == iError*/);

		UnLoadModule();
	}

	m_bInitialized = false;
}

void
CNPStoreBrowsing::Update()
{
	if (m_bInitialized)
	{
		int iError = cellSysutilCheckCallback();
		if (CELL_OK != iError)
		{
			netError("cellSysutilCheckCallback:: FAILED (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError));
		}
	}
}


// --- Private ------------------------------------------------------------------

void
CNPStoreBrowsing::SysutilCallback(uint64_t status, uint64_t /*param*/, void* ASSERT_ONLY(userdata))
{
	ASSERT_ONLY(CNPStoreBrowsing *setup = (CNPStoreBrowsing*) userdata);
	Assert(setup);

#define STORE_EVENT_CASE(x) case x: netWarning("Sysutil callback "#x"");

	switch (status)
	{
		STORE_EVENT_CASE(CELL_SYSUTIL_REQUEST_EXITGAME)
			netError("Start game Shutdown for store Browsing");
			//CSystem::sm_bExit = true;
			break;

		STORE_EVENT_CASE(CELL_SYSUTIL_DRAWING_BEGIN)
			break;

		STORE_EVENT_CASE(CELL_SYSUTIL_DRAWING_END)
			break;

		STORE_EVENT_CASE(SCE_NP_COMMERCE2_EVENT_CREATE_SESSION_DONE)
			break;

		STORE_EVENT_CASE(CELL_SYSUTIL_SYSTEM_MENU_OPEN)
			break;

		STORE_EVENT_CASE(CELL_SYSUTIL_SYSTEM_MENU_CLOSE)
			break;

	default:
		netError("Unknown Sysutil callback - \"0x%08" I64FMT "x\"", status);
	}

#undef STORE_EVENT_CASE

}

bool
CNPStoreBrowsing::StartStoreBrowse(const int iBrowserType)
{
	rtry
	{
		int iUserData = 0x12345678;
		int iError;

		switch(iBrowserType)
		{
		case BROWSE_BY_CATEGORY:
			{ // Start Store Browsing on a Category
				iError = sceNpCommerce2ExecuteStoreBrowse(SCE_NP_COMMERCE2_STORE_BROWSE_TYPE_CATEGORY, NP_CATEGORY_ID, iUserData);
				rcheck(0 <= iError, catchall, netError("sceNpCommerce2ExecuteStoreBrowse:: FAILED (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError)));
			}
			break;

		case BROWSE_BY_PRODUCT:
			{ // Start Store Browsing on a Product
				iError = sceNpCommerce2ExecuteStoreBrowse(SCE_NP_COMMERCE2_STORE_BROWSE_TYPE_PRODUCT, NP_SERVICE_ID, iUserData);
				rcheck(0 <= iError, catchall, netError("sceNpCommerce2ExecuteStoreBrowse:: FAILED (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError)));
			}
			break;

		default:
			rthrow(catchall, netError("Unknown store type \"%d\"", iBrowserType));
		}

		netDebug3("Start Browsing Store: \"0x%x\"", iUserData);

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool
CNPStoreBrowsing::LoadModule()
{
	rtry
	{
		netDebug3("Loading module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2...");

		if(cellSysmoduleIsLoaded(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2) != CELL_OK)
		{
			int iError = cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2);
			rcheck(CELL_OK == iError, catchall, netError("cellSysmoduleLoadModule:: Failed to load CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError)));
			netDebug3("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 Loaded...");
		}
		else
		{
			netDebug3("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 already Loaded...");
		}

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool
CNPStoreBrowsing::UnLoadModule()
{
	rtry
	{
		netDebug3("UnLoading module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2...");

		if(cellSysmoduleIsLoaded(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2) == CELL_OK)
		{
			int iError = cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2);
			rcheck(CELL_OK == iError, catchall, netError("cellSysmoduleUnloadModule:: Failed to UnLoad CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 (code=\"0x%08x\" name=\"%s\")", iError, getErrorName(iError)));
			netDebug3("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 UnLoaded.");
		}
		else
		{
			netDebug3("Module CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2 is not loaded.");
		}

		return true;
	}
	rcatchall
	{
	}

	return false;
}

#endif

// EOF
