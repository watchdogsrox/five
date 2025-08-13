#ifndef __UICONTEXTS_H__
#define __UICONTEXTS_H__
#pragma once

#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "data/base.h"


#if __BANK
namespace rage
{
class bkBank;
class bkList;
};
#endif


//////////////////////////////////////////////////////////////////////////
// 'Contexts' are flags which are turned on/off globally, and can be queried by a variety of things
// chief among them, a more contextual driven approach to buttons and widgets
// For example, "MENU_UNIQUE_ID_MENU_TYPE", "IsMP", "PausedInTheAir", or even "ItsTuesday"
// now functions can check these via data, instead of directly querying code

typedef atHashWithStringDev UIContext;
typedef atArray<UIContext> UISimpleContextList;
class UIContextAutoScope;
class UIContextList;

class SUIContexts : public datBase // for callbacks
{
public:
	// for interacting with the global set
	static bool IsActive(UIContext ContextToCheck);

	static void Activate(UIContext ContextHash);
	static void Deactivate(UIContext ContextHash);
	static void SetActive(UIContext ContextHash, bool bWhichWay) 
	{
		if(bWhichWay)	Activate(ContextHash);
		else		  Deactivate(ContextHash);
	}

#if RSG_DEV
	static void DebugPrintActiveContexts();
#endif

	static void ClearAll();

	// allows stacklike behavior for layering in more contexts  
	static void PushList();
	static void PopList();
	static void PushThisList(const UIContextList& pushAllThese);

#if __BANK
	static void CreateBankWidgets(bkBank* toThisBank);
	static void DestroyBankWidgets();

	static bool sm_bDebugContext;
protected:
	static void BankAdd(const UIContext& ThisContext);
	static void BankRemove(const UIContext& ThisContext, bool bDeleteInsteadOfX = false);
	static void BankParse();
	static void DebugActivate();
	static void DebugDeactivate();

	static bkList* s_pList;
	static char s_ContextDebugString[256];
	static char s_DebugContext[64];
	static char s_ContextTestResult[4];

#endif

protected:
	static atArray<UISimpleContextList>	sm_ActiveContexts;
};

//////////////////////////////////////////////////////////////////////////

class UIContextList
{
public:
	UIContextList() {};
	UIContextList(atString ListOfConditions)	{ ParseFromString(ListOfConditions); }
	UIContextList(const char* ListOfConditions) { ParseFromString(ListOfConditions); }

	void ParseFromString(const char* in_string, bool bAllowKeywords = true);

	// main evaluation function
	bool CheckIfPasses() const;
	
	bool IsSet() const { return !m_ContextHashes.empty(); }

	atString& GetString(atString& out_Result) const;

protected:

	UISimpleContextList		m_ContextHashes;

	// for ease of pushing around
	friend class SUIContexts;
};

//////////////////////////////////////////////////////////////////////////

/// Simple little class which on destructions pops the stack
class UIContextAutoScope
{
public:
	UIContextAutoScope() { SUIContexts::PushList(); }
	~UIContextAutoScope() { SUIContexts::PopList(); }
};

// push a UIContext Stack which will automatically pop on leaving scope;
#define SUICONTEXT_AUTO_PUSH() UIContextAutoScope localPushScoper;

#endif // __UICONTEXTS_H__

