// INCLUDES
#include "Frontend/UIContexts.h"
#include "Frontend/ui_channel.h"
#include "System/nelem.h"

#if __BANK
#include "bank/bank.h"
#include "bank/list.h"
#endif

// static members

atArray<UISimpleContextList> SUIContexts::sm_ActiveContexts;


namespace UIContextKeywords
{
static const u32 ALL_HASH  =  ATSTRINGHASH("*ALL*",0x35df7980);
static const u32 ANY_HASH  =  ATSTRINGHASH("*ANY*",0xa6f05f11);
static const u32 NONE_HASH =  ATSTRINGHASH("*NONE*",0x1eb00a1);
static bool IsKeyword(u32 CheckThis)
{
	return CheckThis == ALL_HASH 
		|| CheckThis == ANY_HASH 
		|| CheckThis == NONE_HASH;
}
};

void SUIContexts::Activate(UIContext ContextHash)
{
	if(!IsActive(ContextHash))
	{
		if(sm_ActiveContexts.empty())
			PushList();
		sm_ActiveContexts.Top().PushAndGrow(ContextHash);

#if __BANK
		BankAdd(ContextHash);
#endif
		uiDebugf1("CONTEXT %s, (0x%08x) is now in effect on layer %i", atHashString(ContextHash).TryGetCStr()?atHashString(ContextHash).TryGetCStr():"-no string-", ContextHash.GetHash(), sm_ActiveContexts.GetCount());
	}
}


void SUIContexts::Deactivate(UIContext ContextHash)
{
	if( sm_ActiveContexts.empty() )
	{
		uiDebugf1("CONTEXT %s, (0x%08x) was never in effect", atHashString(ContextHash).TryGetCStr()?atHashString(ContextHash).TryGetCStr():"-no string-", ContextHash.GetHash());
		return;
	}

#if !__NO_OUTPUT
	int sizeBefore = sm_ActiveContexts.Top().GetCount();
#endif

#if __BANK
	BankRemove( ContextHash );
#endif

	sm_ActiveContexts.Top().DeleteMatches(ContextHash);

#if !__NO_OUTPUT
	if( sizeBefore != sm_ActiveContexts.Top().GetCount() )
	{
		uiDebugf1("CONTEXT %s, (0x%08x) is no longer in effect", atHashString(ContextHash).TryGetCStr()?atHashString(ContextHash).TryGetCStr():"-no string-", ContextHash.GetHash());
	}
#endif
}

#if RSG_DEV
void SUIContexts::DebugPrintActiveContexts()
{
	for(int i = 0; i < sm_ActiveContexts.GetCount(); ++i)
	{
		char outbufBuf[1024] = {0};
		for (int j = 0; j < sm_ActiveContexts[i].GetCount(); j++)
		{
			safecatf(outbufBuf, "%s%s", atHashString( sm_ActiveContexts[i][j] ).TryGetCStr() ? atHashString( sm_ActiveContexts[i][j] ).TryGetCStr() : "-no string-", j < sm_ActiveContexts[i].GetCount() - 1 ? "," : "");
		}
		uiDebugf1("Active UI Contexts %d: %s", i, outbufBuf);
	}
}
#endif // #if RSG_DEV

void SUIContexts::ClearAll()
{
#if __BANK

	int layer=sm_ActiveContexts.GetCount();
	while(layer--)
	{
		UISimpleContextList& rLayer = sm_ActiveContexts[layer];
		int index = rLayer.GetCount();
		while(index--)
		{
			BankRemove( rLayer[index], true );
		}
	}
#endif


	sm_ActiveContexts.Reset();

}

void SUIContexts::PushList()
{
	UISimpleContextList newList;
	if( !sm_ActiveContexts.empty() )
		newList = sm_ActiveContexts.Top(); 
	sm_ActiveContexts.PushAndGrow(newList,2);
	uiDebugf1("CONTEXT Layer Pushed. Now at layer %i", sm_ActiveContexts.GetCount());
}

void SUIContexts::PushThisList(const UIContextList& pushAllThese)
{
	
	if( sm_ActiveContexts.empty() )
	{
		sm_ActiveContexts.PushAndGrow(pushAllThese.m_ContextHashes,2);
	}
	else
	{
		const UISimpleContextList& originalTop = sm_ActiveContexts.Top();
		UISimpleContextList newList(0, pushAllThese.m_ContextHashes.GetCount() + originalTop.GetCount());

		for(int i=0; i < originalTop.GetCount(); ++i )
			newList.Push( originalTop[i] );

		for(int i=0; i < pushAllThese.m_ContextHashes.GetCount(); ++i )
			newList.Push( pushAllThese.m_ContextHashes[i] );

		sm_ActiveContexts.PushAndGrow(newList,2);
	}
	

	uiDebugf1("CONTEXT Layer Pushed with list of size %i. Now at layer %i", pushAllThese.m_ContextHashes.GetCount(), sm_ActiveContexts.GetCount());
}

void SUIContexts::PopList()
{
	if( uiVerifyf(!sm_ActiveContexts.empty(), "Can't pop context layer if we're empty! You must have mismatched Push/Pops!") )
	{
#if __BANK
		UISimpleContextList& rLayer = sm_ActiveContexts.Top();
		int index = rLayer.GetCount();
		while(index--)
		{
			BankRemove( rLayer[index] );
		}
#endif
		sm_ActiveContexts.Pop();
		uiDebugf1("CONTEXT Layer Popped. Now at layer %i", sm_ActiveContexts.GetCount());
	}
}


bool SUIContexts::IsActive(UIContext ContextToCheck)
{
	if( !sm_ActiveContexts.empty() && sm_ActiveContexts.Top().Find(ContextToCheck) != -1 )
		return true;
	
	return false;
}

//////////////////////////////////////////////////////////////////////////

atString& UIContextList::GetString(atString& out_Result) const
{
	out_Result.Reset();
#if !__FINAL
	for(int i=0; i < m_ContextHashes.GetCount(); ++i)
	{
		if( i != 0 )
			out_Result += ", ";
		out_Result += atHashString(m_ContextHashes[i]).TryGetCStr();
	}
#endif

	return out_Result;
}

void UIContextList::ParseFromString(const char* in_string, bool bAllowKeywords)
{
	// ensure we're empty
	m_ContextHashes.Reset();

	// split the string on commas (and remove whitespace)
	atString StringToParse(in_string);
	atStringArray splitData;
	StringToParse.Split(splitData, ',', true);

	if(!bAllowKeywords)
	{
		int i=splitData.GetCount();
		while( i-- )
		{
			bool bIsKeyword = UIContextKeywords::IsKeyword( atStringHash(splitData[i].c_str() ) );
			uiAssertf(!bIsKeyword, "A value of '%s' is disallowed for this use of context! It'll be stripped!", splitData[i].c_str());
			if(bIsKeyword)
			{
				splitData.Delete(i);
			}
		}
	}
#if __ASSERT
	else
	{
		bool bIsKeyword = UIContextKeywords::IsKeyword( atStringHash(splitData[0].c_str() ) );
		uiAssertf(bIsKeyword, "It is expected that all contexts explicitly set their initial state! Expected *NONE*,*ALL*,*ANY*, etc., but got %s!", splitData[0].c_str());
	}
#endif

	// allocate enough and push them all in
	m_ContextHashes.Reserve(splitData.GetCount());
	for(int i=0; i < splitData.GetCount();++i)
	{
		splitData[i].Trim();
		m_ContextHashes.Push( splitData[i].c_str() );
	}
}

#define uiContextf(fmt,...) uiDisplayf("CONTEXT: " fmt, ##__VA_ARGS__)

#if __BANK
#define uiContextTrace(fmt,...) if( SUIContexts::sm_bDebugContext ) {uiContextf(fmt, ##__VA_ARGS__);}
#else
#define uiContextTrace(fmt,...) do {} while(false);
#endif

bool UIContextList::CheckIfPasses() const
{
#if __BANK
	if( SUIContexts::sm_bDebugContext )
	{
		atString tempList;
		if( m_ContextHashes.empty() )
			tempList += "-empty-";
		else
		{
			tempList += atHashString(m_ContextHashes[0]).TryGetCStr() ? atHashString(m_ContextHashes[0]).TryGetCStr() : "-no hash-";
			for(int i=1; i < m_ContextHashes.GetCount(); ++i )
			{
				tempList += ", ";
				tempList += atHashString(m_ContextHashes[i]).TryGetCStr() ? atHashString(m_ContextHashes[i]).TryGetCStr() : "-no hash-";
			}
		}
		
		uiContextf("Beginning to Parse: '%s'", tempList.c_str());
	}
#endif
	if(m_ContextHashes.empty())
	{
		uiContextTrace("Was empty!");
		return true;
	}

	enum ParseMode
	{
		PM_Any,
		PM_All,
		PM_None
	};



	ParseMode curMode = PM_Any;
	ParseMode prevMode = PM_Any;
	int iNumAny = 0;
	bool bOneSuccess = false;
	bool bAnyFound = false;

	for(int i=0; i<m_ContextHashes.size(); ++i)
	{
		const UIContext& curCheck = m_ContextHashes[i];

		if(curMode != prevMode)
		{
			if(prevMode == PM_Any)
			{
				if(iNumAny)
				{
					if(bAnyFound)
					{
						uiContextTrace("ANY: End of check, any found");
						bOneSuccess = true;
					}
					else
					{
						uiContextTrace("ANY: None Found, returning false");
						return false;
					}
				}

				iNumAny = 0;
				bAnyFound = false;
			}

			prevMode = curMode;
		}

		// current one is ALL, switch to all mode
		if(curCheck == UIContextKeywords::ALL_HASH )
		{
			curMode = PM_All;
			uiContextTrace("Mode switch to *ALL*");
			continue;
		}

		// it's "ANY", switch to any mode
		if( curCheck == UIContextKeywords::ANY_HASH )
		{
			curMode = PM_Any;
			uiContextTrace("Mode switch to *ANY*");
			continue;
		}

		// "None"? Go to none
		if( curCheck == UIContextKeywords::NONE_HASH )
		{
			curMode = PM_None;
			uiContextTrace("Mode switch to *NONE*");
			continue;
		}



		switch( curMode )
		{
			// in ALL mode, bail at first unmatch
		case PM_All:
			if( !SUIContexts::IsActive( curCheck ) )
			{
				uiContextTrace("ALL:  %s (0x%08x) is INACTIVE, return false", atHashString(curCheck).TryGetCStr() ? atHashString(curCheck).TryGetCStr() : "-no hash-", curCheck.GetHash() );
				return false;
			}
			uiContextTrace("ALL:  %s (0x%08x) is   ACTIVE", atHashString(curCheck).TryGetCStr() ? atHashString(curCheck).TryGetCStr() : "-no hash-", curCheck.GetHash() );
			bOneSuccess = true;
			break;

			// in NONE mode, bail at first match
		case PM_None:
			if( SUIContexts::IsActive( curCheck ) )
			{
				uiContextTrace("NONE: %s (0x%08x) is   ACTIVE, return false", atHashString(curCheck).TryGetCStr() ? atHashString(curCheck).TryGetCStr() : "-no hash-", curCheck.GetHash() );
				return false;
			}
			uiContextTrace("NONE: %s (0x%08x) is INACTIVE", atHashString(curCheck).TryGetCStr() ? atHashString(curCheck).TryGetCStr() : "-no hash-", curCheck.GetHash() );
			bOneSuccess = true;
			break;

			// in any mode, at first success, win!
		case PM_Any:
			++iNumAny;
			if( SUIContexts::IsActive( curCheck ) )
			{
				uiContextTrace("ANY:  %s (0x%08x) is   ACTIVE", atHashString(curCheck).TryGetCStr() ? atHashString(curCheck).TryGetCStr() : "-no hash-", curCheck.GetHash() );
				bAnyFound = true;
			}
#if !__NO_OUTPUT
			else
			{
				uiContextTrace("ANY:  %s (0x%08x) is INACTIVE", atHashString(curCheck).TryGetCStr() ? atHashString(curCheck).TryGetCStr() : "-no hash-", curCheck.GetHash() );
			}
#endif
			break;			
		}
	}

	if(curMode == PM_Any && iNumAny)
	{
		uiContextTrace("ANY: End of check, any found? %s", bAnyFound ? "true" : "false");
		if(bAnyFound)
		{
			bOneSuccess = true;
		}
	}

	uiContextTrace("Finished loop, returning %s", bOneSuccess ? "true" : "false");
	return bOneSuccess;
}





//////////////////////////////////////////////////////////////////////////


#if __BANK
bkList* SUIContexts::s_pList = NULL;
char	SUIContexts::s_ContextDebugString[256];
char	SUIContexts::s_DebugContext[64];
char	SUIContexts::s_ContextTestResult[4];
bool	SUIContexts::sm_bDebugContext = false;

void SUIContexts::DebugActivate()
{
	SUIContexts::Activate(SUIContexts::s_DebugContext);
}

void SUIContexts::DebugDeactivate()
{
	SUIContexts::Deactivate(SUIContexts::s_DebugContext);
}

void SUIContexts::CreateBankWidgets(bkBank* toThisBank)
{
	toThisBank->PushGroup("Contexts");
	toThisBank->AddToggle("Output Parsing traces to UI channel", &sm_bDebugContext);
	s_pList = toThisBank->AddList("Context history");
	s_pList->AddColumnHeader(0, "Hash (Hex)",	bkList::STRING);
	s_pList->AddColumnHeader(1, "Hash (Int)",	bkList::INT);
	s_pList->AddColumnHeader(2, "Key",			bkList::STRING);
	s_pList->AddColumnHeader(3, "Layer",		bkList::INT);
	s_pList->AddColumnHeader(4, "Removed",		bkList::STRING);
	for(int i = 0; i < sm_ActiveContexts.GetCount(); ++i)
	{
		for(int j=0; j < sm_ActiveContexts[i].GetCount(); ++j )
			BankAdd( sm_ActiveContexts[i][j] );
	}

	toThisBank->AddText("Debug Context", s_DebugContext, COUNTOF(s_DebugContext));
	toThisBank->AddButton("Activate", &DebugActivate);
	toThisBank->AddButton("Deactivate", &DebugDeactivate);
	
	toThisBank->AddText("Test Context String", s_ContextDebugString, COUNTOF(s_ContextDebugString), datCallback(SUIContexts::BankParse));
	toThisBank->AddROText("Passes", s_ContextTestResult, COUNTOF(s_ContextTestResult) );
	toThisBank->AddTitle("Enter text above and press Enter to determine if it'll pass or not");

	

	toThisBank->PopGroup();
}

void SUIContexts::DestroyBankWidgets()
{
	s_pList = NULL;
}

void SUIContexts::BankParse()
{
	UIContextList newList(s_ContextDebugString);

	if( newList.CheckIfPasses() )
		formatf(s_ContextTestResult,"YES");
	else
		formatf(s_ContextTestResult,"No");
}

void SUIContexts::BankAdd(const UIContext& ThisContext)
{
	if( !s_pList )
		return;

	char pszHashInHex[12];
	const char* pszNames = atHashString(ThisContext).TryGetCStr();
	s32 key = static_cast<s32>(ThisContext.GetHash());
	formatf(pszHashInHex, "0x%08x", ThisContext.GetHash() );
	s_pList->AddItem( key, 0, pszHashInHex);
	s_pList->AddItem( key, 1, key );
	s_pList->AddItem( key, 2, pszNames && pszNames[0] ? pszNames : "-no string-" ) ;
	s_pList->AddItem( key, 3, sm_ActiveContexts.GetCount() );
	s_pList->AddItem( key, 4, "" );
}

void SUIContexts::BankRemove(const UIContext& ThisContext, bool bDeleteInsteadOfX)
{
	if( !s_pList )
		return;
	
	if( bDeleteInsteadOfX )
	{
		s_pList->RemoveItem( static_cast<s32>( ThisContext.GetHash() ) );
	}
	else
	{
		// instead of removing it, just set its fourth column to 'dead'
		s_pList->AddItem( static_cast<s32>( ThisContext.GetHash() ), 4, "X" );
	}
}

#endif
