#include "CScriptMenu.h"
#include "parsercore/element.h"
#include "Parsercore/attribute.h"
#include "script/script_channel.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "Script/gta_thread.h"
#include "script/streamedscripts.h"
#include "streaming/streamingvisualize.h"

#include "game/config.h"

XPARAM(scriptProtectGlobals);

#if __ASSERT
#include "control/gamelogic.h"
#endif

FRONTEND_MENU_OPTIMISATIONS()
#define __CHECK_FOR_CLEANUPS 0

#undef __script_channel
#define __script_channel script


CScriptMenu::CScriptMenu(CMenuScreen& owner) : CMenuBase(owner)
, m_MyProgramId(srcpidNONE)
, m_MyThreadId(THREAD_INVALID)
, m_pMyThread(NULL)
, m_MyStreamRequest(-1)
, m_StackSize(0)
#if __BANK
, m_pMyGroup(NULL)
#endif
{
	owner.FindDynamicData("path",			m_ScriptPath);
	owner.FindDynamicData("handlesButtons", m_bHasButtonLogic);
	owner.FindDynamicData("continual",		m_bContinualUpdateMode);
	owner.FindDynamicData("canpersist",		m_bAllowToLive);

	const char* pszStack = NULL;
	bool bWasSpecified = owner.FindDynamicData("stack", pszStack, "PAUSE_MENU_SCRIPT");
	
	atHashValue stackSize(pszStack);
	// do a lookup
	const CConfigScriptStackSizes& configData = CGameConfig::Get().GetConfigScriptStackSizes();
	for(int i=0; i < configData.m_StackSizeData.GetCount(); ++i )
	{
		if( configData.m_StackSizeData[i].m_StackName == stackSize )
		{
			m_StackSize = configData.m_StackSizeData[i].m_SizeOfStack;
			break;
		}
	}

	if( bWasSpecified && !scriptVerifyf(m_StackSize!=0, "Unable to find a match for stack='%s', reverting to default size of %i", pszStack, GtaThread::GetDefaultStackSize() ) )
		m_StackSize = GtaThread::GetDefaultStackSize();
}

void CScriptMenu::Init()
{
	// punt off the streaming request a bit early so it may be ready later
	HandleStreaming();
}

CScriptMenu::~CScriptMenu()
{
#if __CHECK_FOR_CLEANUPS
	if(m_pMyThread != NULL)
		scriptErrorf("CScriptMenu::~CScriptMenu - expected the script thread to have already been killed by an earlier call to LoseFocus(). We'll tidy this up... for now.");
	scriptAssertf(m_QueuedActions.GetCount() == 0, "CScriptMenu::~CScriptMenu (%s) - expected the QueuedActions array to have already been Reset by an earlier call to LoseFocus()", m_Owner.MenuScreen.GetParserName());
#endif
	KillScript();

	if( m_MyStreamRequest != -1 )
	{
		g_StreamedScripts.ClearRequiredFlag(m_MyStreamRequest, STRFLAG_MISSION_REQUIRED);
	}
	m_MyProgramId = srcpidNONE;

#if __BANK
	if( m_pMyGroup )
		m_pMyGroup->Destroy();
#endif
}

#if !__FINAL
void CScriptMenu::Render(const PauseMenuRenderDataExtra* UNUSED_PARAM(renderData))
{
	if( m_bContinualUpdateMode && GetScriptPath().length() == 0 )
	{
		CTextLayout DeadTextDebug;
		DeadTextDebug.SetScale(Vector2(0.5f, 1.0f));
		DeadTextDebug.SetColor( Color32(0xFFE15050) );
		DeadTextDebug.Render( Vector2(0.27f, 0.35f), "The script of this page");
		DeadTextDebug.Render( Vector2(0.27f, 0.42f), "Stopped unexpectedly");
		DeadTextDebug.Render( Vector2(0.27f, 0.49f), "    Forever broken");
		CText::Flush();
	}
}
#endif

void CScriptMenu::LaunchScript( const SPauseMenuScriptStruct& args )
{
	atString& scriptPath = GetScriptPath();
	// do nothing if there's no script path
	if( scriptPath.length() == 0 )
		return;
#if RSG_PC	&& ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif

	// script isn't running, boot it up
	if( !m_pMyThread )
	{
		if( m_MyProgramId != srcpidNONE )
		{
			scriptDisplayf("[ScriptMenu] Launching script '%s' with Program 0x%08x and stack size of %i.", scriptPath.c_str(), m_MyProgramId, m_StackSize);
			scriptDisplayf("[ScriptMenu] Args of eType: %i\t\tMenuScreenId: %i '%s'\t\tPreviousId: %i '%s'\t\tUniqueId: %i"
				, args.eTypeOfInteraction.Int
				, args.MenuScreenId.Int, MenuScreenId(args.MenuScreenId.Int).GetParserName()
				, args.PreviousId.Int, MenuScreenId(args.PreviousId.Int).GetParserName()
				, args.iUniqueId.Int);

			m_MyThreadId = CTheScripts::GtaStartNewThreadWithProgramId(m_MyProgramId, &args, sizeof(args), m_StackSize, scriptPath.c_str() );

			m_pMyThread = static_cast<GtaThread*>( scrThread::GetThread(m_MyThreadId) );
			m_pMyThread->SetThreadPriority(scrThread::THREAD_PRIO_MANUAL_UPDATE);
			if (m_pMyThread == NULL)
			{
#if __ASSERT
				scriptAssertf(0, "CScriptMenu::LaunchScript - Could not get a pointer to the new thread %s", scriptPath.c_str());
#else
				scriptErrorf("CScriptMenu::LaunchScript - Could not get a pointer to the new thread %s", scriptPath.c_str());
#endif	//	__ASSERT
			}
		}
		// program's not loaded yet, queue up this action for later
	//	else if( !m_bContinualUpdateMode )
		else
		{
			m_QueuedActions.PushAndGrow(args,4);
			scriptDisplayf("[ScriptMenu] %s: Queued up desired action #0: T%i M%i P%i U%i", m_Owner.MenuScreen.GetParserName(), args.eTypeOfInteraction.Int, args.MenuScreenId.Int, args.PreviousId.Int, args.iUniqueId.Int);
		}
		
	} // </Creation>

	bool bScriptHasAborted = false;
	if(m_pMyThread)
	{
		scrThread::State endResult = m_pMyThread->Update(DEFAULT_INSTRUCTION_COUNT);
		bScriptHasAborted = (endResult == scrThread::ABORTED);
		
		if(	bScriptHasAborted && m_bContinualUpdateMode )
		{
			scriptAssertf(0, "Unexpected termination of %s! Never running it again!", GetScriptPath().c_str() );
			
			// nuke the script path so it doesn't run again (anymore)
			GetScriptPath().Reset();
		}
	}

	if( (!m_bAllowToLive && !m_bContinualUpdateMode) || bScriptHasAborted)
		KillScript();
}

void CScriptMenu::KillScript()
{
	if(m_pMyThread)
	{
		// Script thread can be killed externally during SHUTDOWN_SESSION, so validate ID before proceeding
		if(m_pMyThread->GetThreadId() != THREAD_INVALID)
		{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
			CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
			// if the script is continually updating, give it one last chance to clean up (if it so desires)
			if(m_bContinualUpdateMode || m_bAllowToLive)
			{
				if( m_pMyThread->ForceCleanup(GtaThread::FORCE_CLEANUP_PAUSE_MENU_TERMINATED) )
					m_pMyThread->Update(DEFAULT_INSTRUCTION_COUNT);
			}

			m_pMyThread->Kill();
		}

		m_pMyThread = NULL;
	}
	
	m_MyThreadId = THREAD_INVALID;
}

bool CScriptMenu::Populate(MenuScreenId newScreenId)
{
	if( !m_bContinualUpdateMode )
	{
		SPauseMenuScriptStruct newArgs(kFill, newScreenId.GetValue(), -1,-1);
		LaunchScript(newArgs);
	}
	return false;
}

void CScriptMenu::LoseFocus()
{
	KillScript();
	m_QueuedActions.Reset();
}

void CScriptMenu::LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) )
{
	if( !m_bContinualUpdateMode )
	{
		SPauseMenuScriptStruct newArgs(kLayoutChange, iNewLayout.GetValue(), iPreviousLayout.GetValue(), iUniqueId);
		LaunchScript(newArgs);
	}
}

bool CScriptMenu::TriggerEvent(MenuScreenId MenuId, s32 iUniqueId)
{
	if( !m_bContinualUpdateMode )
	{
		SPauseMenuScriptStruct newArgs(kTriggerEvent, MenuId.GetValue(), -1, iUniqueId);
		LaunchScript(newArgs);
		return true;
	}

	return false;
}

void CScriptMenu::PrepareInstructionalButtons( MenuScreenId MenuId, s32 iUniqueId)
{
	if( !m_bHasButtonLogic )
		return;

	if( !m_bContinualUpdateMode )
	{
		SPauseMenuScriptStruct newArgs(kPrepareButtons, MenuId.GetValue(), -1, iUniqueId);
		LaunchScript(newArgs);
	}
}

void CScriptMenu::HandleStreaming()
{
	atString& scriptPath = GetScriptPath();

	// bail if we have nothing to stream OR the program's ready
	if( scriptPath.length() <= 0 || m_MyProgramId != srcpidNONE)
		return;

	// check if it's resident
	m_MyProgramId = scrProgram::IsCompiledAndResident(scriptPath);
	if( m_MyProgramId == srcpidNONE )
	{
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::SCRIPTMENU);

		// nope. Check if it's streamed
		m_MyStreamRequest = g_StreamedScripts.FindSlot(scriptPath.c_str()).Get();

		ASSERT_ONLY(bool bIgnoreMissingScripts = CGameLogic::GetCurrentLevelIndex() != 1);
		scriptAssertf(m_MyStreamRequest != -1 || bIgnoreMissingScripts, "%s:Script doesn't exist", scriptPath.c_str() );

		if( m_MyStreamRequest == -1 )
		{
			// bad file, just clear it out and give up forever
			scriptPath.Reset();

			// wipe out the FILL_CONTENT_SCRIPT flag
			for( int i=0; i < m_Owner.MenuItems.GetCount(); ++i )
			{
				if( m_Owner.MenuItems[i].MenuAction == MENU_OPTION_ACTION_FILL_CONTENT_FROM_SCRIPT )
					m_Owner.MenuItems.Delete(i);
			}
			return;
		}

		// if we can fetch its program, it's streamed.
		if( scrProgram* pProgram = g_StreamedScripts.Get(strLocalIndex(m_MyStreamRequest)) )
		{
			m_MyProgramId = pProgram->GetProgramId();
			g_StreamedScripts.SetRequiredFlag(m_MyStreamRequest, STRFLAG_MISSION_REQUIRED);
			scriptDisplayf("[ScriptMenu] Program of 0x%08x found for '%s' ", m_MyProgramId, scriptPath.c_str());
		}
		else
		{
			// otherwise, request that shiz.
			g_StreamedScripts.StreamingRequest(strLocalIndex(m_MyStreamRequest), STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
			scriptDisplayf("[ScriptMenu] Requesting '%s' with request of %i", scriptPath.c_str(), m_MyStreamRequest);
		}
	}
	else
	{
		scriptDisplayf("[ScriptMenu] Script '%s' is compiled and resident! Using that program id 0x%08x", scriptPath.c_str(), m_MyProgramId);
	}
}

void CScriptMenu::Update()
{
#if __ASSERT		
	CScriptHud::bLockScriptrendering = !NetworkInterface::IsGameInProgress();
#endif

	// check if streamed and stream it if not
	HandleStreaming();


	if(m_bContinualUpdateMode || (m_bAllowToLive && m_pMyThread))
	{
		// need to reset the per frame hud flags
		if( !CTheScripts::ShouldBeProcessed()) 
			CScriptHud::ClearMapVisibilityPerFrameFlags();

		SPauseMenuScriptStruct newArgs(kUpdate, -1, CPauseMenu::GetMenuceptionForceFocus().GetValue(), -1);
		LaunchScript(newArgs);
	}
	// if we had to queue any actions, fire them all off now
	// assuming that we don't already have a thread (rare), and our program's streamed
	else if( !m_pMyThread && m_MyProgramId != srcpidNONE && !m_QueuedActions.empty() )
	{
		for( int i=0; i < m_QueuedActions.GetCount(); ++i )
		{
			scriptDisplayf("[ScriptMenu] %s: Blowing away Queued Action #i: T%i M%i P%i U%i", m_Owner.MenuScreen.GetParserName(), m_QueuedActions[i].eTypeOfInteraction.Int, m_QueuedActions[i].MenuScreenId.Int, m_QueuedActions[i].PreviousId.Int, m_QueuedActions[i].iUniqueId.Int);
			LaunchScript(m_QueuedActions[i]);
		}

		m_QueuedActions.Reset();
	}
	
#if __ASSERT		
	CScriptHud::bLockScriptrendering = false;
#endif
}

#if __BANK
void CScriptMenu::AddWidgets(bkBank* pBank)
{
	if(m_pMyGroup)
		return;

	atString name("Scripted Bank: ");
	name += m_Owner.MenuScreen.GetParserName();

	m_pMyGroup = pBank->PushGroup(name);
	{
		// not ENTIRELY sure how safe this is
		if(  m_ScriptPath.length() > 0)
			pBank->AddROText("Script Path: ", m_ScriptPath.c_str(), m_ScriptPath.length());
		else
			pBank->AddTitle("Script Path: -None-");
		pBank->AddText("Stream Request",	&m_MyStreamRequest);
		if( sizeof(scrProgramId) == sizeof(int) )
			pBank->AddText("Program ID",		reinterpret_cast<int*>(&m_MyProgramId));
		pBank->AddText("Thread ID",			reinterpret_cast<int*>(&m_MyThreadId));
		pBank->AddText("Continual Update",	&m_bContinualUpdateMode);
		pBank->AddSlider("Queued Action Count", m_QueuedActions.GetCountPointer(), 0, 5, 0 );
	}
	pBank->PopGroup();
}

#endif

