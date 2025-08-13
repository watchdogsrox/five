//
// NP_StoreBrowsing.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#if __PPU

#ifndef NP_STORE_BROWSING_H
#define NP_STORE_BROWSING_H

// --- Include Files ------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

//PURPOSE
//   Manage access to PLAYSTATION®Store, preform "store browsing" were main application is
//    terminated while all of the processing - from the browsing to the purchases – is carried out
//    by the system software, after which the application is restarted again.
class CNPStoreBrowsing
{
public:
	enum eStoreTypes
	{
		BROWSE_BY_CATEGORY, // Target to display upon starting store browsing
		BROWSE_BY_PRODUCT   // Target to display upon starting store browsing
	};

private:
	bool m_bInitialized;

public:
	CNPStoreBrowsing() : m_bInitialized(false)
	{
	}

	~CNPStoreBrowsing()
	{
		Shutdown();
	}

	void  Init(const int iBrowserType);
	void  Shutdown();
	void  Update();

private:
	static void  SysutilCallback(uint64_t status, uint64_t /*param*/, void *userdata);

	bool  StartStoreBrowse(const int iBrowserType);
	bool  LoadModule();
	bool  UnLoadModule();
};

#endif // NP_STORE_BROWSING_H

#endif // __PPU

// EOF
