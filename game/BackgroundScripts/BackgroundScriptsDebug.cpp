// 
// 
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

// rage
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank\button.h"
#include "fwsys/gameskeleton.h"
#include "string/stringutil.h"

// game
#include "BackgroundScriptsDebug.h"
#include "BackgroundScripts.h"

NETWORK_OPTIMISATIONS()

void BackgroundScriptsDebug::Init()
{
	if (BANKMGR.FindBank("BGScripts") == NULL)
	{
		if( bkBank* pBank = &BANKMGR.CreateBank("BGScripts") )
		{
			m_pCreateBankButton = pBank->AddButton("Create Background Scripts Debug widgets", datCallback(MFA(BackgroundScriptsDebug::CreateBankWidgetsOnDemand), this));
		}
	}
}

void BackgroundScriptsDebug::CreateBankWidgetsOnDemand()
{
	bkBank* pBank = BANKMGR.FindBank("BGScripts");
	if (pBank && m_pCreateBankButton)
	{
		m_pCreateBankButton->Destroy();
		m_pCreateBankButton = NULL;

		pBank->PushGroup("Reseters", true);
		pBank->AddButton("Unload Scripts And XML", datCallback(MFA(BackgroundScriptsDebug::UnloadScriptsAndXML), this));
		pBank->AddButton("Load Scripts And XML", datCallback(MFA(BackgroundScriptsDebug::LoadScriptsAndXML), this));
		pBank->PopGroup();

		pBank->PushGroup("Output", true);
		pBank->AddButton("Print Script Counts", datCallback(MFA(BackgroundScriptsDebug::PrintScriptCounts), this));
		pBank->AddButton("Print Active Scripts", datCallback(MFA(BackgroundScriptsDebug::PrintActiveScripts), this));
		pBank->AddButton("Print Active Context", datCallback(MFA(BackgroundScriptsDebug::PrintActiveContexts), this));
		pBank->AddButton("Print Active Game Mode", datCallback(MFA(BackgroundScriptsDebug::PrintActiveMode), this));
		pBank->PopGroup();

		pBank->PushGroup("Contexts", true);
		memset(m_contextName, 0, sizeof(m_contextName));
		pBank->AddText("Context", m_contextName, sizeof(m_contextName));
		pBank->AddButton("Start Context", datCallback(MFA(BackgroundScriptsDebug::StartContext), this));
		pBank->AddButton("End Context", datCallback(MFA(BackgroundScriptsDebug::EndContext), this));
		pBank->AddButton("End All Context", datCallback(MFA(BackgroundScriptsDebug::EndAllContexts), this));
		pBank->PopGroup();
	}
}

void BackgroundScriptsDebug::UnloadScriptsAndXML()
{
	bgScriptDisplayf("BackgroundScriptsDebug::UnloadScriptsAndXML - start");

	SBackgroundScripts::GetInstance().Shutdown();

	bgScriptDisplayf("BackgroundScriptsDebug::UnloadScriptsAndXML - end");
}

void BackgroundScriptsDebug::LoadScriptsAndXML()
{
	bgScriptDisplayf("BackgroundScriptsDebug::LoadScriptsAndXML - start");

	SBackgroundScripts::GetInstance().Init();

	bgScriptDisplayf("BackgroundScriptsDebug::LoadScriptsAndXML - end");
}

void BackgroundScriptsDebug::PrintScriptCounts()
{
	int counts[NUM_SCRIPT_TYPES] = {0};
	int countsSP[NUM_SCRIPT_TYPES] = {0};
	int countsMP[NUM_SCRIPT_TYPES] = {0};
	int maxContextsUsed = 0;
	int maxLaunchParamsUsed = 0;

	BackgroundScripts& bgScripts = SBackgroundScripts::GetInstance();
	for(int i=0; i<bgScripts.GetScriptInfoCount(); ++i)
	{
		BGScriptInfo* pInfo = bgScripts.GetScriptInfo(i);
		ScriptType scriptType = pInfo->GetScriptType();
		if(0 <= scriptType && scriptType < NUM_SCRIPT_TYPES)
		{
			counts[scriptType]++;

			if(!pInfo->GetIsAnyFlagSet(BGScriptInfo::FLAG_MODE_MP))
			{
				countsSP[scriptType]++;
			}
			else
			{
				countsMP[scriptType]++;
			}
		}

		if(maxContextsUsed < pInfo->DebugGetContextCount())
		{
			maxContextsUsed = pInfo->DebugGetContextCount();
		}

		if(maxLaunchParamsUsed < pInfo->DebugGetLaunchParamCount())
		{
			maxLaunchParamsUsed = pInfo->DebugGetLaunchParamCount();
		}
	}

	bgScriptDisplayf("BackgroundScriptsDebug::PrintScriptCounts - size of BGScriptInfo = %lu bytes, size of CommonScriptInfo = %lu bytes", sizeof(BGScriptInfo), sizeof(CommonScriptInfo));
	bgScriptDisplayf("BackgroundScriptsDebug::PrintScriptCounts - %s = %d(%dsp %dmp), using %lu bytes (%lu bytes per object).", "UTIL", counts[SCRIPT_TYPE_UTIL], countsSP[SCRIPT_TYPE_UTIL], countsMP[SCRIPT_TYPE_UTIL], counts[SCRIPT_TYPE_UTIL]*sizeof(UtilityScriptInfo), sizeof(UtilityScriptInfo));
	bgScriptDisplayf("BackgroundScriptsDebug::PrintScriptCounts - total scripts = %d, using %lu bytes.",
			counts[SCRIPT_TYPE_UTIL],
			counts[SCRIPT_TYPE_UTIL]*sizeof(UtilityScriptInfo));

	if(bgScripts.GetScriptInfoAllocator())
		bgScriptDisplayf("BackgroundScriptsDebug::PrintScriptCounts - Allocator using %" SIZETFMT "u of %" SIZETFMT "u bytes",bgScripts.GetScriptInfoAllocator()->GetMemoryUsed(), bgScripts.GetScriptInfoAllocator()->GetHeapSize());
	else
		bgScriptDisplayf("BackgroundScriptsDebug::PrintScriptCounts - NULL ScriptInfoAllocator");
	
	bgScriptDisplayf("BackgroundScriptsDebug::PrintScriptCounts - Max Contexts used = %d, and max LaunchParamsUsed = %d", maxContextsUsed, maxLaunchParamsUsed);
}

void BackgroundScriptsDebug::PrintActiveScripts()
{
	bgScriptDisplayf("Starting BackgroundScriptsDebug::PrintActiveScripts: Printing all active BG Scripts");
	int count = 0;

	BackgroundScripts& bgScripts = SBackgroundScripts::GetInstance();
	for(int i=0; i<bgScripts.GetScriptInfoCount(); ++i)
	{
		BGScriptInfo* pInfo = bgScripts.GetScriptInfo(i);
		if(pInfo->IsRunning())
		{
			++count;
			bgScriptDisplayf("Active Script(%s)", bgScripts.GetScriptInfo(i)->GetName());
		}
	}

	bgScriptDisplayf("Finished BackgroundScriptsDebug::PrintActiveScripts: Printed %d scripts out of %d", count, bgScripts.GetScriptInfoCount());
}

void BackgroundScriptsDebug::PrintActiveContexts()
{
	bgScriptDisplayf("Starting BackgroundScriptsDebug::PrintActiveContexts: Printing all active BG Contexts");
	int count = 0;

	BackgroundScripts& bgScripts = SBackgroundScripts::GetInstance();
	for(ContextMap::Iterator it=bgScripts.m_contextMap.CreateIterator(); !it.AtEnd(); ++it)
	{
		if(it.GetData().isActive)
		{
			++count;
			const ContextString* pString = m_contextMap.Access(it.GetKey());
			if(pString)
			{
				bgScriptDisplayf("Active Context(%s)", pString->c_str());
			}
			else
			{
				bgScriptDisplayf("Active Context(Unknown 0x%x)", it.GetKey());
			}
		}
	}

	bgScriptDisplayf("Finished BackgroundScriptsDebug::PrintActiveContexts: Printed %d contexts", count);
}

void BackgroundScriptsDebug::PrintActiveMode()
{
	bgScriptDisplayf("BackgroundScriptsDebug::PrintActiveMode not implemented.");
}

void BackgroundScriptsDebug::AddContext(const char* pContext)
{
	u32 hash = atStringHash(pContext);
	
	if(!m_contextMap.Access(hash))
	{
		bgScriptDebug1("BackgroundScriptsDebug::AddContext: 0x%x %s", hash, pContext);
		ContextString str(pContext);
		m_contextMap.Insert(hash, str);
	}
}

void BackgroundScriptsDebug::StartContext()
{
	SBackgroundScripts::GetInstance().StartContext(m_contextName);
}

void BackgroundScriptsDebug::EndContext()
{
	SBackgroundScripts::GetInstance().EndContext(m_contextName);
}

void BackgroundScriptsDebug::EndAllContexts()
{
	SBackgroundScripts::GetInstance().EndAllContexts();
}

void BackgroundScriptsDebug::AddName(const char* pName)
{
	u32 hash = atStringHash(pName);

	if(!m_bgNameMap.Access(hash))
	{
		bgScriptDebug1("BackgroundScriptsDebug::AddName: 0x%x %s", hash, pName);
		BGNameString str(pName);
		m_bgNameMap.Insert(hash, str);
	}
}

#endif // __BANK

