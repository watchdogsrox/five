// 
// 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef PAYNE_THRESHOLDS_DEBUG_H
#define PAYNE_THRESHOLDS_DEBUG_H

#if __BANK
// Rage headers
#include "data/base.h"
#include "atl/atfixedstring.h"
#include "atl/map.h"

namespace rage
{
	class bkButton;
}

class BackgroundScriptsDebug : public datBase
{
public:
	
	BackgroundScriptsDebug() {}

	// Utility
	void Init();

	void UnloadScriptsAndXML();
	void LoadScriptsAndXML();

	void PrintScriptCounts();
	void PrintActiveScripts();
	void PrintActiveContexts();
	void PrintActiveMode();

	void AddContext(const char* pContext);
	void AddName(const char* pName);
	const char* GetName(u32 hash) const {return hash && m_bgNameMap.Access(hash) ? m_bgNameMap.Access(hash)->c_str() : "NULL";}

	void StartContext();
	void EndContext();
	void EndAllContexts();

	void CreateBankWidgetsOnDemand();

private:
	typedef atFixedString<64> ContextString;
	atMap<u32, ContextString> m_contextMap;

	typedef atFixedString<64> BGNameString;
	atMap<u32, BGNameString> m_bgNameMap;

	char m_contextName[50];

	bkButton* m_pCreateBankButton;
};
#endif // __BANK

#endif // PAYNE_THRESHOLDS_DEBUG_H
