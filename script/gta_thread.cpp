#include "script/gta_thread.h"


// Rage headers
#include "profile/timebars.h"
#include "script/wrapper.h"
#include "system/param.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwscript/scriptInterface.h"
#include "fwutil/idgen.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"
#include "core/game.h"
#include "debug/Debug.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/arrays/NetworkArrayMgr.h"
#include "network/debug/NetworkDebug.h"
#include "network/network.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "peds/ped.h"
#include "renderer/RenderTargetMgr.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"
#include "scene/ExtraContent.h"
#include "scene/worldpoints.h"
#include "script/commands_graphics.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/script_debug.h"
#include "script/script_hud.h"
#include "script/streamedscripts.h"
#include "script/script_helper.h"
#include "script/script_text_construction.h"
#include "system/FileMgr.h"
#include "Task/General/TaskBasic.h"

#define SCRIPT_HEAP_SIZE (6700<<10)	// ~6.5MiB for the script heap
#define SCRIPT_DEBUG_HEAP_SIZE (2500<<10)	// ~2.4MiB extra for in non final

NETWORK_OPTIMISATIONS()

#if !__FINAL
PARAM(override_default_stack_size, "[gta_thread] when running with a testbed level, call this with 1024 to override the increased Default stack size specified in the gameconfig.xml in the Title Update");
#endif	//	!__FINAL

#define USE_SCRIPT_ALLOCATOR	ENABLE_POOL_ALLOCATOR

#if USE_SCRIPT_ALLOCATOR
sysMemAllocator* GtaThread::sm_Allocator = 0;
#endif // USE_SCRIPT_ALLOCATOR

#if !__FINAL
s32 GtaThread::sm_SoakTestStackSize = -1;
s32 GtaThread::sm_NetworkBotStackSize = -1;
s32 GtaThread::sm_DebugMenuStackSize = -1;
#endif	//	!__FINAL
s32 GtaThread::sm_DefaultStackSize = -1;
s32 GtaThread::sm_MissionStackSize = -1;

#if GTA_REPLAY
bool GtaThread::sm_bTriggerAutoCleanup = false;
#endif // GTA_REPLAY

s32 GtaThread::ScriptThreadIteratorIndex = 0;
scrThreadId GtaThread::ms_LastPathNodeScriptThreadID = THREAD_INVALID;

// PURPOSE:	Constructor
GtaThread::GtaThread() : scrThread()
{
	m_Handler = NULL;

	Initialise();

	//	DeatharrestCheckEnabled = true;
//	DoneDeatharrest = false;
#if __BANK
	pDebugger = NULL;
#endif
}

// PURPOSE:	Destructor
GtaThread::~GtaThread()
{
#if __BANK
	if (pDebugger)
	{
		delete pDebugger;
		pDebugger = NULL;
	}

	m_WidgetTree.RemoveAllWidgetNodes();
#endif
}

void GtaThread::StaticInit(unsigned UNUSED_PARAM(initMode))
{
#if USE_SCRIPT_ALLOCATOR
	if(!sm_Allocator)
	{
		const unsigned sizeData = SCRIPT_HEAP_SIZE NOTFINAL_ONLY(+SCRIPT_DEBUG_HEAP_SIZE);
		const unsigned sizeTotal = sizeof(sysMemSimpleAllocator) + sizeData;
		sysMemSimpleAllocator* source = sysMemManager::GetInstance().GetPoolAllocator();
		RAGE_TRACKING_ONLY(source->SetTallied(false));
		char* memory = (char*)source->Allocate(sizeTotal,16,0);
		RAGE_TRACKING_ONLY(source->SetTallied(true));
		char* addrData = memory;	// In memory the data section comes first
		char* addrAlc = memory + sizeData;	// Place the allocator object at the end of the data block
		sm_Allocator = new (addrAlc) sysMemSimpleAllocator(addrData,sizeData,sysMemSimpleAllocator::HEAP_MAIN,false);	// In place allocate the allocator
#if RAGE_TRACKING
		diagTracker* t = diagTracker::GetCurrent();
		if (t)
		{
			t->InitHeap("Script Heap", addrData, sizeData);
		}
#endif // RAGE_TRACKING
	}
#endif // USE_SCRIPT_ALLOCATOR
}

// PURPOSE:	Reset the thread to a runnable state at the top of the current program
void GtaThread::Reset(scrProgramId program,const void *argStruct,int argStructSize)
{
	scrThread::Reset(program,argStruct,argStructSize);
	m_HashOfScriptName = (s32) atHashString::ComputeHash(GetScriptName());
}


// PURPOSE:	Execute script instructions.
// PARAMS:	insnCount - Reference to a variable containing the maximum number of instructions
//				to execute before returning.  Modified after this call to contain the number
//				of instructions remaining.  Can be zero, in which case the script does nothing.
// RETURNS:	New machine state
/*
scrThread::State GtaThread::Run(int &insnCount)
{

}
*/

#if __DEV
// PURPOSE:	Dumps thread state to console (program counter, stack contents, etc)
/*
void GtaThread::DumpState(void (*printer)(const char *fmt,...))
{

}
*/
#endif

// PURPOSE:	Reserve space for fixed number of threads
// PARAMS:	count - Maximum number of threads we will support simultaneously
// NOTES:	Reimplement this in a subclass and call that version if you subclass
//			the script thread.
void GtaThread::AllocateThreads()
{
	const CConfigScriptStackSizes& configData = CGameConfig::Get().GetConfigScriptStackSizes();

	const s32 num_of_array_entries = configData.m_StackSizeData.GetCount();

	s32 TotalNumberOfStacks = 0;
	for (s32 i=0; i<num_of_array_entries; i++)
	{
		TotalNumberOfStacks += configData.m_StackSizeData[i].m_NumberOfStacksOfThisSize;
	}

	sm_Threads.Resize(TotalNumberOfStacks);
	sm_Stacks.Reserve(TotalNumberOfStacks*2);

	{	// temporary scope for allocator.
#if USE_SCRIPT_ALLOCATOR
		sysMemAutoUseAllocator alc(*sm_Allocator);
#endif // USE_SCRIPT_ALLOCATOR
		for (s32 i=0; i<TotalNumberOfStacks; i++)
		{
			sm_Threads[i] = rage_new GtaThread();
		}
	}

#if !__NO_OUTPUT
	s32 TotalStackSize = 0;
#if !__FINAL
	s32 TotalDebugStackSize = 0;
#endif	//	!__FINAL
#endif	//	!__NO_OUTPUT

	for (s32 i=0; i<num_of_array_entries; i++)
	{
		s32 stackSize = configData.m_StackSizeData[i].m_SizeOfStack;
		s32 numberOfStacks = configData.m_StackSizeData[i].m_NumberOfStacksOfThisSize;

#if !__FINAL
		if (configData.m_StackSizeData[i].m_StackName == ATSTRINGHASH("DEFAULT",0xe4df46d5) )
		{
			PARAM_override_default_stack_size.Get(stackSize);
		}
#endif	//	!__FINAL

		s32* codeStackSize = GetCodeStackSize(configData.m_StackSizeData[i].m_StackName);
		if(codeStackSize) *codeStackSize = stackSize;

#if !__FINAL
		if(IsDebugStack(configData.m_StackSizeData[i].m_StackName))
		{
			TotalDebugStackSize += (numberOfStacks * stackSize);
		}
#endif //!__FINAL

		scriptDebugf1("GtaThread::AllocateThreads - adding %d script stacks of size %d", numberOfStacks, stackSize);
#if USE_SCRIPT_ALLOCATOR
		sysMemAutoUseAllocator alc(*sm_Allocator);
#endif // USE_SCRIPT_ALLOCATOR
		AddThreadStack(numberOfStacks, stackSize);

#if !__NO_OUTPUT
		TotalStackSize += (numberOfStacks * stackSize);
#endif	//	!__NO_OUTPUT

	}

#if !__NO_OUTPUT
	scriptDisplayf("GtaThread::AllocateThreads - total of all stacks = %d * %d bytes = %d bytes", TotalStackSize, (s32) sizeof(scrValue), (TotalStackSize * (s32) sizeof(scrValue)));
#if !__FINAL
	scriptDisplayf("GtaThread::AllocateThreads - that includes %d * %d bytes = %d bytes for debug scripts", TotalDebugStackSize, (s32) sizeof(scrValue), (TotalDebugStackSize * (s32) sizeof(scrValue)));
#endif	//	!__FINAL
#endif	//	!__NO_OUTPUT

	scriptAssertf(sm_DefaultStackSize > 0, "GtaThread::AllocateThreads - stack size with name DEFAULT not found in gameconfig.xml");
	scriptAssertf(sm_MissionStackSize > 0, "GtaThread::AllocateThreads - stack size with name MISSION not found in gameconfig.xml");
#if !__FINAL
	scriptAssertf(sm_SoakTestStackSize > 0, "GtaThread::AllocateThreads - stack size with name SOAK_TEST not found in gameconfig.xml");
	scriptAssertf(sm_NetworkBotStackSize > 0, "GtaThread::AllocateThreads - stack size with name NETWORK_BOT not found in gameconfig.xml");
	scriptAssertf(sm_DebugMenuStackSize > 0, "GtaThread::AllocateThreads - stack size with name DEBUG_MENU not found in gameconfig.xml");
#endif	//	!__FINAL
}

extern scrCommandHash<scrCmd> s_CommandHash;
void GtaThread::DeallocateThreads()
{
#if !__FINAL
	sysStack::PrePrintStackTrace = scrThread::OriginalPrePrintStackTrace;
#endif // !__FINAL

	s_CommandHash.Kill();
	{
#if USE_SCRIPT_ALLOCATOR
		sysMemAutoUseAllocator alc(*sm_Allocator);	// Auto push/pop the allocator
#endif // USE_SCRIPT_ALLOCATOR
		for (int i=0; i<sm_Threads.GetCount(); i++)
			delete sm_Threads[i];
		for (int i=0; i<sm_Stacks.GetCount(); i++)
			delete[] sm_Stacks[i].m_Stack;
	}
	sm_Threads.Reset();
	sm_Stacks.Reset();

	sm_DefaultStackSize =
	sm_MissionStackSize =
#if !__FINAL
	sm_SoakTestStackSize =
	sm_NetworkBotStackSize =
	sm_DebugMenuStackSize = 
#endif    //            !__FINAL
		0;
}

#if __DEV
static GtaThread *lastActiveGtaThread;
#endif

// PURPOSE:	Update TIMERA and TIMERB values and then call Run.
// PARAMS:	insnCount - As per scrThread::Run
// RETURNS:	New thread state as per scrThread::Run
// NOTES:	This functionality is separated out mostly for single-stepping support
scrThread::State GtaThread::Update(int insnCount)
{
	DIAG_CONTEXT_MESSAGE("Updating script %s", GetScriptName());
	
#if __DEV
	lastActiveGtaThread = this;
#endif

	scriptAssertf(GetThreadId() != 0, "GtaThread::Update - thread id should never be 0");

	if (bThreadHasBeenPaused)
	{
		return m_Serialized.m_State;
	}

#if __BANK
	if (CTheScripts::DisplayScriptProcessMessagesWhileScreenIsFaded())
	{
		scriptDisplayf("GtaThread::Update - %s processed while screen is faded", GetScriptName());
	}
#endif	//	__BANK

//	Graeme - 14.06.10 - I had to comment this out to allow the scripts to run for one frame during initialisation
//	if(CGame::IsChangingSessionState())
//	{	// If the network connection is lost then a message is displayed telling the player this and waiting
		//	for A to be pressed. It's probably a bad idea to continue processing the scripts while a session restart is pending.
		//	This return will stop any more scripts running after a script has flagged for a session restart in this frame.
//		return m_Serialized.m_State;
//	}

#if __DEV
	CTheScripts::ClearAllEntityCheckedForDeadFlags();
#endif

	DoForceCleanupCheck();

	// This is now done in scrThread::Update, so doing it here just introduces a bunch of
	// bogus extra timebars with very little time on them.
	//// PF_START_TIMEBAR_DETAIL(m_ScriptName.GetCStr());

#if __BANK
	if (pDebugger)
	{
		pDebugger->Update();
	}
#endif

#if SCRIPT_PROFILING
	if (m_bDisplayProfileData)
	{
		CScriptDebug::DisplayProfileDataForThisThread(this);
	}
#endif	//	SCRIPT_PROFILING

	// reset script hud
	CScriptHud::BeforeThreadUpdate();

	CTheScripts::SetScenarioPedsCanBeGrabbedByScript(false);

	ASSERT_ONLY(bool bExitFlagMarked = m_bExitFlagSet;)

	scrThread::State ReturnState = scrThread::Update(insnCount);

#if __ASSERT
	VerifyNetworkSafety();
#endif

#if __ASSERT
	scriptAssertf(CTheScripts::GetXmlTree() == NULL, "GtaThread::Update - %s %d - xml node != NULL. DELETE_XML_FILE should be called" , GetScriptName(), GetThreadPC());
#endif

	scriptAssertf(ReturnState == ABORTED || bExitFlagMarked == m_bExitFlagResponse, "BackgroundScript \"%s\" did not call BG_SET_EXITFLAG_RESPONSE before aborting!", GetScriptName());

#if __ASSERT
	scriptAssertf(CTheScripts::GetMissionReplayStats().IsReplayStatsStructureBeingConstructed() == false, "GtaThread::Update - BEGIN_REPLAY_STATS has been called without a matching END_REPLAY_STATS");
#endif	//	__ASSERT

#if __BANK
	CScriptWidgetTree::CheckWidgetGroupsHaveBeenClosed(GetScriptName());
#endif

	if (CScriptSaveData::GetCreationOfScriptSaveStructureInProgress())
	{
		scriptAssertf(0, "GtaThread::Update - check that you've called STOP_SAVE_DATA for %s", GetScriptName());
		int loop = 0;	//	Just to be sure that we don't end up stuck in the while loop below forever,
						//	quit after MAX_DEPTH_OF_STACK_OF_STRUCTS attempts. GetCreationOfScriptSaveStructureInProgress() should 
						//	return false before this ever happens though.
		while (CScriptSaveData::GetCreationOfScriptSaveStructureInProgress() && (loop < MAX_DEPTH_OF_STACK_OF_STRUCTS) )
		{
			CScriptSaveData::CloseScriptStruct(this);
			loop++;
		}
	}

	// reset script hud
	CScriptHud::AfterThreadUpdate();

	// check for no lingering particle asset overrides
	graphics_commands::VerifyUsePtFxAssetHashName();

	return ReturnState;
}


// PURPOSE: Virtual entry point called by KillAllThreads on each thread.
void GtaThread::Kill()
{
	scriptDisplayf("%d GtaThread::Kill - %s \n", fwTimer::GetSystemFrameCount(), GetScriptName());

#if __BANK
	CScriptDebug::SetComboBoxOfThreadNamesNeedsUpdated(true);
#endif	//	__BANK

	if (m_Handler)
	{
		CTheScripts::GetScriptHandlerMgr().UnregisterScript(*this);
	}

	switch(ScriptBrainType)
	{
		case CScriptsForBrains::PED_STREAMED:
		case CScriptsForBrains::SCENARIO_PED:
			{
				CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(EntityIndexForScriptBrain, 0);
				if (pPed)
				{
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain, false );
				}
			}
			break;
			
		case CScriptsForBrains::OBJECT_STREAMED :
			{
				CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(EntityIndexForScriptBrain, 0);
				if (pObject)
				{
					pObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_WAITING_TILL_OUT_OF_BRAIN_RANGE;	//	OBJECT_WAITING_TILL_IN_BRAIN_RANGE;
				}
			}
			break;

		case CScriptsForBrains::WORLD_POINT :
			{
				C2dEffect *pEffect = &CModelInfo::GetWorldPointStore().GetEntry(fwFactory::GLOBAL, EntityIndexForScriptBrain);//CWorldPoints::GetWorldPointFromIndex(EntityIndexForScriptBrain);

				if (pEffect)
				{
					pEffect->GetWorldPoint()->BrainStatus = CWorldPoints::WAITING_TILL_OUT_OF_BRAIN_RANGE;	//	OUT_OF_BRAIN_RANGE;
				}
			}
			break;

		default:
			break;
	}

#if __BANK
	if (pDebugger)
	{
		delete pDebugger; // KS WR 163 HACK
		pDebugger = NULL;
	}

	m_WidgetTree.RemoveAllWidgetNodes();
#endif

#if GTA_REPLAY
	if (m_bCleanupReplay)
	{
		sm_bTriggerAutoCleanup = true;
	}
#endif // GTA_REPLAY

	Initialise();	//	clears ScriptBrainType and EntityIndexForScriptBrain

	scrThread::Kill();
}

#if GTA_REPLAY
bool GtaThread::GetShouldAutoCleanupReplay()
{
	if (sm_bTriggerAutoCleanup)
	{
		sm_bTriggerAutoCleanup = false;
		return true;
	}
	else
	{
		return false;
	}
}
#endif // GTA_REPLAY

void GtaThread::KillAllAbortedScriptThreads()
{
	for (int i=0; i<sm_Threads.GetCount(); i++)
	{
		GtaThread* pThread = static_cast<GtaThread*>(sm_Threads[i]);
		if ( pThread->GetState()==ABORTED && pThread->IsValid()
			&& !pThread->IsExitFlagResponseSet() )	//< background scripts get killed in BackgroundScripts.cpp
		{
			pThread->Kill();
		}
	}
}

void GtaThread::Initialise()
{
	Assert(!m_Handler);

	m_Handler = NULL;
	m_NetComponent = NULL;
	ForceCleanupPC = 0;
	ForceCleanupFP = 0;
	ForceCleanupSP = 0;
	m_ForceCleanupFlags = 0;
	m_ForceCleanupTriggered = 0;
	m_CauseOfMostRecentForceCleanup = 0;
	InstanceId = -1;
//	bDoneDeathArrest = false;
	IsThisAMissionScript = false;
	m_bIsARandomEventScript = false;
	m_bIsATriggerScript = false;
	bSafeForNetworkGame = false;
	bSetPlayerControlOnInMissionCleanup = true;
	bClearHelpInMissionCleanup = true;
	bIsThisAMiniGameScript = false;
	bThisScriptAllowsNonMiniGameHelpMessages = false;
	bThreadHasBeenPaused = false;
	bThisScriptCanBePaused = true;
	ScriptBrainType = -1;
	EntityIndexForScriptBrain = 0;
	bThisScriptCanRemoveBlipsCreatedByAnyScript = false;
	m_bHasUsedPedPathCommand = false;
	m_bHasOverriddenVehicleEntering = false;
	m_bDisplayProfileData = false;
	m_bExitFlagSet = false;
	m_bExitFlagResponse = false;
	m_bIsHighPrio = true;
#if GTA_REPLAY
	m_bCleanupReplay = false;
#endif // GTA_REPLAY
}


void GtaThread::TidyUpMiniGameFlag()
{
	if (scriptVerifyf(bIsThisAMiniGameScript, "Script Name = %s SET_MINIGAME_IN_PROGRESS FALSE - this script is not a minigame - maybe already called this command with FALSE", GetScriptName()))
	{
		if (bThisScriptAllowsNonMiniGameHelpMessages)
		{
			if (CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages > 0)
			{
				CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages--;
			}
			else
			{
				scriptAssertf(0, "Script Name = %s SET_MINIGAME_IN_PROGRESS - setting to FALSE - expecting to decrement NumberOfMiniGamesAllowingNonMiniGameHelpMessages but it's already less than 1", GetScriptName());
			}

			bThisScriptAllowsNonMiniGameHelpMessages = false;
		}
		if (CTheScripts::GetNumberOfMiniGamesInProgress() > 0)
		{
			CTheScripts::SetNumberOfMiniGamesInProgress(CTheScripts::GetNumberOfMiniGamesInProgress() - 1);
		}
		else
		{
			scriptAssertf(0, "Script Name = %s SET_MINIGAME_IN_PROGRESS - setting to FALSE so expect to decrement NumberOfMiniGamesInProgress but it's already less than 1", GetScriptName());
		}
		bIsThisAMiniGameScript = false;
	}
}

bool GtaThread::IsANetworkThread()
{
	return (m_Handler && m_Handler->RequiresANetworkComponent());
}

#if !__NO_OUTPUT
GtaThread* GtaThread::GetThreadWithThreadId(const scrThreadId threadId)
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		GtaThread *pGtaThread = static_cast<GtaThread *>(sm_Threads[i]);

		//	How do we avoid an active mission script being killed (in SA it would have ThisMustBeTheOnlyMissionRunning set)
		if ( pGtaThread->IsValid() )
		{
			if (threadId == pGtaThread->GetThreadId())
			{
				return pGtaThread;
			}
		}
	}

	return NULL;
}
#endif

void GtaThread::ForceCleanupForAllMatchingThreads(u32 ForceCleanupFlags, MatchingMode ForceCleanupMatchingMode, const char *pScriptName, const scrThreadId ThreadId)
{
	bool bFoundAMatch = false;
	GtaThread *pGtaThread = NULL;
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		pGtaThread = static_cast<GtaThread *>(sm_Threads[i]);
		if (pGtaThread->IsValid() && (pGtaThread->GetState() != ABORTED))
		{
			bFoundAMatch = false;
			switch (ForceCleanupMatchingMode)
			{
				case MATCH_SCRIPT_NAME :
					if (stricmp(pGtaThread->GetScriptName(), pScriptName) == 0)
					{
						bFoundAMatch = true;
					}
					break;

				case MATCH_THREAD_ID :
					if (pGtaThread->GetThreadId() == ThreadId)
					{
						bFoundAMatch = true;
					}
					break;

				default :
					bFoundAMatch = true;
					break;
			}

			if (bFoundAMatch)
			{
				pGtaThread->ForceCleanup(ForceCleanupFlags);
			}
		}
	}
}

bool GtaThread::ForceCleanup( u32 ForceCleanupFlags )
{
	if ((ForceCleanupFlags & m_ForceCleanupFlags) != 0)
	{
		if (m_ForceCleanupTriggered == 0)
		{
			m_ForceCleanupTriggered = 2;
			m_CauseOfMostRecentForceCleanup = (ForceCleanupFlags & m_ForceCleanupFlags);
			return true;
		}
	}
	return false;
}

void GtaThread::KillAllThreadsWithThisName(const char *pProgramName)
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
//	How do we avoid an active mission script being killed (in SA it would have ThisMustBeTheOnlyMissionRunning set)
		if ( static_cast<GtaThread *>(sm_Threads[i])->IsValid() && (stricmp(sm_Threads[i]->GetScriptName(), pProgramName) == 0))
			sm_Threads[i]->Kill();
	}
}

//	Kill all scripts that haven't been flagged as safe for network game.
//	Probably only main, debug and cellphone scripts will be flagged as safe
/*
void GtaThread::KillAllThreadsForNetworkGame(void)
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		if ( static_cast<GtaThread *>(sm_Threads[i])->IsValid() && (false == static_cast<GtaThread *>(sm_Threads[i])->bSafeForNetworkGame) )
		{
			sm_Threads[i]->Kill();
		}
	}
}
*/

//	Kill all scripts that have not been flagged as safe for the single player game.
/*
void GtaThread::KillAllThreadsForSinglePlayerGame(void)
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		GtaThread * pThread = static_cast<GtaThread *>(sm_Threads[i]);

		if (AssertVerify(pThread)
			&& pThread->IsValid() 
			&& pThread->m_Handler
			&& pThread->m_Handler->RequiresANetworkComponent()
			&& !static_cast< CGameScriptHandlerNetwork* >(pThread->m_Handler)->ActiveInSinglePlayer())
		{
			sm_Threads[i]->Kill();
		}
	}
}
*/

// All threads are paused. Apart from the one specified.
void GtaThread::PauseAllThreadsBarThisOne(GtaThread *pThread)
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		//	How do we avoid an active mission script being killed (in SA it would have ThisMustBeTheOnlyMissionRunning set)
		if ( static_cast<GtaThread *>(sm_Threads[i])->IsValid())
		{
			if ( (pThread != sm_Threads[i]) && (static_cast<GtaThread *>(sm_Threads[i])->bThisScriptCanBePaused) )
			{
				static_cast<GtaThread *>(sm_Threads[i])->bThreadHasBeenPaused = true;
			}
		}
	}
}

// All threads are Unpaused.
void GtaThread::UnpauseAllThreads()
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		//	How do we avoid an active mission script being killed (in SA it would have ThisMustBeTheOnlyMissionRunning set)
		if ( static_cast<GtaThread *>(sm_Threads[i])->IsValid())
		{
			static_cast<GtaThread *>(sm_Threads[i])->bThreadHasBeenPaused = false;
		}
	}
}

// Returns true if any active scripts are not safe for network game.
// Once this function returns false, the game knows that all ambient scripts have been shut down and it is safe to launch the network game.
/*
bool GtaThread::AreThereAnyNonNetworkScriptsRunning(void)
{
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		if ( static_cast<GtaThread *>(sm_Threads[i])->IsValid()
            && (sm_Threads[i]->GetState() != ABORTED)
            && (false == static_cast<GtaThread *>(sm_Threads[i])->bSafeForNetworkGame) )
		{
			return true;
		}
	}

	return false;
}
*/

s32 GtaThread::CountActiveThreads()
{
	s32 number_of_threads = 0;
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		const GtaThread *pThread = static_cast<GtaThread *>(sm_Threads[i]);
		if (pThread->IsValid() && (pThread->GetState() != ABORTED))
		{
			number_of_threads++;
		}
	}

	return number_of_threads;
}

s32 GtaThread::CountActiveThreadsWithThisProgramID(const scrProgramId programid)
{
	s32 number_of_threads = 0;
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		const GtaThread *pThread = static_cast<GtaThread *>(sm_Threads[i]);
		if (pThread->IsValid() && (pThread->GetState() != ABORTED) && (pThread->GetProgram() == programid) )
		{
			number_of_threads++;
		}
	}

	return number_of_threads;
}

u32 GtaThread::CountSinglePlayerOnlyScripts()
{
	u32 number_of_threads = 0;
	for(int i = 0; i < sm_Threads.GetCount(); i++)
	{
		GtaThread *pCurrentGtaThread = static_cast<GtaThread *>(sm_Threads[i]);
		if ( pCurrentGtaThread->IsValid() 
			&& (pCurrentGtaThread->GetState() != ABORTED) 
			&& (!pCurrentGtaThread->IsANetworkThread()) 
			&& (!pCurrentGtaThread->bSafeForNetworkGame)
			)
		{
			if (pCurrentGtaThread->GetProgramCounter(0) != 0)
			{
				scriptAssertf((pCurrentGtaThread->m_ForceCleanupFlags & FORCE_CLEANUP_FLAG_SP_TO_MP) != 0, "GtaThread::CountSinglePlayerOnlyScripts - single player script %s doesn't handle FORCE_CLEANUP_FLAG_SP_TO_MP", pCurrentGtaThread->GetScriptName());
			}

			scriptDisplayf("Single player script %s is running", pCurrentGtaThread->GetScriptName());
			number_of_threads++;
		}
	}

	scriptDisplayf("%d single player scripts are running", number_of_threads);
	return number_of_threads;
}

void GtaThread::SetForceCleanupReturnPoint(u32 ForceCleanupBitField)
{
	if(SCRIPT_VERIFY(ForceCleanupPC == 0, "%s:HAS_FORCE_CLEANUP_OCCURRED has already been called for this script - you can only call it once"))
	{
		ForceCleanupPC = (m_Serialized.m_PC + c_NativeInsnLength);
		ForceCleanupFP = m_Serialized.m_FP;
		ForceCleanupSP = m_Serialized.m_SP;

		m_ForceCleanupFlags = ForceCleanupBitField;
		m_CauseOfMostRecentForceCleanup = 0;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DoForceCleanupCheck
// PURPOSE :  Performs the ForceCleanup check if it has been enabled
/////////////////////////////////////////////////////////////////////////////////
void GtaThread::DoForceCleanupCheck()
{
	if (ForceCleanupPC)	//	 && CTheScripts::PlayerIsOnAMission)
	{
		scriptAssertf(FindPlayerPed(), "%s:CRunningScript::DoForceCleanupCheck - Player exists but doesn't have a character pointer", GetScriptName());

		bool bRunForceCleanupCode = false;
#if __DEV
		bool bPlayerIsDeadOrArrested = false;
#endif

		CPed * player = FindPlayerPed();
		if (player && !CTheScripts::IsPlayerPlaying(player))
		{
			if (m_ForceCleanupFlags & FORCE_CLEANUP_FLAG_PLAYER_KILLED_OR_ARRESTED)
			{	//	If this script is set up to listen for the death/arrest event
				if (!NetworkInterface::IsNetworkOpen())
				{	//	For network games, death and arrest don't actually trigger the ForceCleanup code.
					//	Only the FORCE_CLEANUP command will trigger the ForceCleanup code
					bRunForceCleanupCode = true;
					m_CauseOfMostRecentForceCleanup = FORCE_CLEANUP_FLAG_PLAYER_KILLED_OR_ARRESTED;
				}
			}
#if __DEV
			bPlayerIsDeadOrArrested = true;
#endif
		}

		if ( (!bRunForceCleanupCode) && m_ForceCleanupTriggered)	//	CTheScripts::sm_FakeDeatharrest )
		{
			scriptDisplayf("%d GtaThread::DoForceCleanupCheck - ForceCleanup has triggered for %s \n", fwTimer::GetSystemFrameCount(), GetScriptName());

			bRunForceCleanupCode = true;
#if __DEV
			if (player && !bPlayerIsDeadOrArrested)
			{
				if (m_ForceCleanupFlags & FORCE_CLEANUP_FLAG_PLAYER_KILLED_OR_ARRESTED)
				{	//	only set this flag if the script has been set up to listen for the player being killed/arrested
					player->m_nDEflags.bCheckedForDead = TRUE;
				}
			}
#endif
		}

		if (bRunForceCleanupCode)
		{	// got to deal with the player being arrested/dead.... Joy...
			m_Serialized.m_PC = ForceCleanupPC;
			m_Serialized.m_FP = ForceCleanupFP;
			m_Serialized.m_SP = ForceCleanupSP;
			Pushi(true);

			// Clear any blocked execution flag
			SetThreadBlockState(false);

			//	Reset these variables now - the script should really have been written to terminate within this frame
			//	but if not, the ForceCleanup would re-trigger again next frame
			ForceCleanupPC = 0;
			ForceCleanupFP = 0;
			ForceCleanupSP = 0;
		}
		else
		{
#if __DEV
			if (m_ForceCleanupFlags & FORCE_CLEANUP_FLAG_PLAYER_KILLED_OR_ARRESTED)
			{	//	only set this flag if the script has been set up to listen for the player being killed/arrested
				FindPlayerPed()->m_nDEflags.bCheckedForDead = TRUE;
			}
#endif
		}
	}

	if (m_ForceCleanupTriggered)
	{
		m_ForceCleanupTriggered--;
	}
}


int GtaThread::GetIDForThisLineInThisThread()
{
//	int ReturnID = ((int) m_Serialized.m_ThreadId) << 16;
	int ReturnID = (int) fwIdKeyGenerator::Get((void*)m_Serialized.m_ThreadId);
	ReturnID += m_Serialized.m_PC;
	return ReturnID;
}

void GtaThread::ScriptThreadIteratorReset()
{
	ScriptThreadIteratorIndex = 0;
}

s32 GtaThread::ScriptThreadIteratorGetNextThreadId()
{
	while (ScriptThreadIteratorIndex < sm_Threads.GetCount())
	{
		GtaThread *pGtaThread = static_cast<GtaThread*>(sm_Threads[ScriptThreadIteratorIndex]);
		if (pGtaThread->IsValid() && (pGtaThread->GetState() != ABORTED))
		{
			s32 returnIndex = static_cast<s32>(pGtaThread->GetThreadId());
			ScriptThreadIteratorIndex++;
			return returnIndex;
		}
		else
		{
			ScriptThreadIteratorIndex++;
		}
	}

	return 0;
}

void GtaThread::SetHasLoadedAllPathNodes(scrThreadId threadID)
{
	Assertf(ms_LastPathNodeScriptThreadID == THREAD_INVALID || 
		threadID == ms_LastPathNodeScriptThreadID,					// Script calls repeatedly, until the nodes are reported as loaded.
		"GtaThread: Thread <%s> trying to load all path nodes, when another thread <%s> has already loaded them!", 
		scrThread::GetThread( threadID )->GetScriptName(),
		scrThread::GetThread( ms_LastPathNodeScriptThreadID ) ? scrThread::GetThread( ms_LastPathNodeScriptThreadID )->GetScriptName():"Unknown");

	ms_LastPathNodeScriptThreadID = threadID;
}

void GtaThread::ClearHasLoadedAllPathNodes(ASSERT_ONLY(scrThreadId threadID))
{
	Assertf(ms_LastPathNodeScriptThreadID == THREAD_INVALID || 
		threadID == ms_LastPathNodeScriptThreadID, 
		"GtaThread: Thread <%s> trying to clear path nodes, not thread <%s> that allocated them!",
		scrThread::GetThread( threadID )->GetScriptName(),
		scrThread::GetThread( ms_LastPathNodeScriptThreadID ) ? scrThread::GetThread( ms_LastPathNodeScriptThreadID )->GetScriptName():"Unknown");

	ms_LastPathNodeScriptThreadID = THREAD_INVALID;
}

#if __ASSERT
void GtaThread::VerifyNetworkSafety()
{
	if (CGameLogic::GetCurrentLevelIndex() == 1)
	{	//	Only check this on the proper GTA5 level - not on the testbed levels
		if (IsValid() && (GetState() != ABORTED) )
		{
			if (NetworkInterface::IsGameInProgress())
			{
//	Graeme - I'll have to leave this commented out for now because single player scripts do exist for the first
//				few frames of the network session. They don't kill themselves until main_persistent.sc calls FORCE_CLEANUP(FORCE_CLEANUP_FLAG_SP_TO_MP)
//				scriptVerifyf( IsANetworkThread() || bSafeForNetworkGame, 
//					"GtaThread::VerifyNetworkSafety - %s is a single player script that hasn't called NETWORK_SET_SCRIPT_IS_SAFE_FOR_NETWORK_GAME but it's running during a multiplayer game. Does it handle FORCE_CLEANUP_FLAG_SP_TO_MP properly?", GetScriptName());
			}
			else
			{
				bool bCleansUpWhenEnteringMP = false;
				if ((m_ForceCleanupFlags & FORCE_CLEANUP_FLAG_SP_TO_MP) != 0)
				{
					bCleansUpWhenEnteringMP = true;
				}

				bool bIsANetworkScript = false;
				if (IsANetworkThread())
				{
//					scriptAssertf(0, "GtaThread::VerifyNetworkSafety - %s is a network script that is running during the single player session. Is this valid?", GetScriptName());
					scriptAssertf(!bCleansUpWhenEnteringMP, "GtaThread::VerifyNetworkSafety - %s is a network script that also handles FORCE_CLEANUP_FLAG_SP_TO_MP", GetScriptName());
					bIsANetworkScript = true;
				}

				if (bSafeForNetworkGame)
				{
					scriptAssertf(!bCleansUpWhenEnteringMP, "GtaThread::VerifyNetworkSafety - %s shouldn't call NETWORK_SET_SCRIPT_IS_SAFE_FOR_NETWORK_GAME and also handle FORCE_CLEANUP_FLAG_SP_TO_MP", GetScriptName());
				}

				scriptAssertf( bCleansUpWhenEnteringMP || bIsANetworkScript || bSafeForNetworkGame, 
					"GtaThread::VerifyNetworkSafety - %s is a single player script that hasn't called NETWORK_SET_SCRIPT_IS_SAFE_FOR_NETWORK_GAME and doesn't handle FORCE_CLEANUP_FLAG_SP_TO_MP", GetScriptName());
			}
		}	//	if (IsValid() && (GetState() != ABORTED) )
	}	//	if (CGameLogic::GetCurrentLevelIndex() == 1)
}
#endif	//	__ASSERT


s32* GtaThread::GetCodeStackSize( const atHashValue& name )
{
	if(name==ATSTRINGHASH("DEFAULT",0xe4df46d5)) return &sm_DefaultStackSize;
	else if(name==ATSTRINGHASH("MISSION",0x8bc157a3)) return &sm_MissionStackSize;
#if !__FINAL
	else if(name==ATSTRINGHASH("SOAK_TEST",0x1E2F6CC)) return &sm_SoakTestStackSize;
	else if(name==ATSTRINGHASH("NETWORK_BOT",0x908A0827)) return &sm_NetworkBotStackSize;
	else if(name==ATSTRINGHASH("DEBUG_MENU",0x458318E2)) return &sm_DebugMenuStackSize;
#endif //!__FINAL
	else return 0;
}

bool GtaThread::IsDebugStack( const atHashValue& name )
{
	return name==ATSTRINGHASH("SOAK_TEST",0x1E2F6CC) ||
		name==ATSTRINGHASH("NETWORK_BOT",0x908A0827) ||
		name==ATSTRINGHASH("DEBUG_SCRIPT",0xA0E550E4) ||
		name==ATSTRINGHASH("DEBUG_MENU",0x458318E2);
}


#if __BANK
void GtaThread::AllocateDebugger()
{
	if (scriptVerifyf(pDebugger == NULL, "%s:AllocateDebugger - thread already has a debugger", GetScriptName()))
	{
		if (CGameLogic::GetCurrentLevelIndex() >= 1 && CGameLogic::GetCurrentLevelIndex() <= 5)
		{
			CFileMgr::SetDir(CScriptDebug::GetScriptDebuggerPath());
			pDebugger = rage_new scrDebugger(GetScriptName(), m_Serialized.m_ThreadId, NULL, false);
			CFileMgr::SetDir("");
		}
	}
}
		

void GtaThread::AllocateDebuggerForThread(scrThreadId ThreadID)
{
	scrThread *pThreadToDebug = GetThread(ThreadID);
	if (scriptVerifyf(pThreadToDebug, "AllocateDebuggerForThread -  can't find thread with the given ID"))
	{
		( static_cast<GtaThread *>(pThreadToDebug) )->AllocateDebugger();
	}
}

void GtaThread::AllocateDebuggerForThreadInSlot(s32 ArrayIndex)
{
	if (scriptVerifyf( (ArrayIndex >= 0) && (ArrayIndex < sm_Threads.GetCount()), "GtaThread::AllocateDebuggerForThreadInSlot - array index %d is out of range 0 to %d", ArrayIndex, sm_Threads.GetCount()))
	{
		GtaThread *pGtaThread = static_cast<GtaThread*>(sm_Threads[ArrayIndex]);
		if (pGtaThread->IsValid() && (pGtaThread->GetState() != ABORTED))
		{
			pGtaThread->AllocateDebugger();
		}		
	}
}

//	Clear the m_bDisplayProfileData flag for all script threads except the one in the specified slot
void GtaThread::EnableProfilingForThreadInSlot(s32 ArrayIndex)
{
	if (scriptVerifyf( (ArrayIndex >= 0) && (ArrayIndex < sm_Threads.GetCount()), "GtaThread::EnableProfilingForThreadInSlot - array index %d is out of range 0 to %d", ArrayIndex, sm_Threads.GetCount()))
	{
		for(int i = 0; i < sm_Threads.GetCount(); i++)
		{
			GtaThread *pCurrentGtaThread = static_cast<GtaThread *>(sm_Threads[i]);
			if (pCurrentGtaThread)
			{
				if (i == ArrayIndex)
				{
					pCurrentGtaThread->m_bDisplayProfileData = true;
				}
				else
				{
					pCurrentGtaThread->m_bDisplayProfileData = false;
				}
			}
		}
	}
}

void GtaThread::UpdateArrayOfNamesOfRunningThreads(atArray<const char*> &ArrayOfThreadNames)
{
	static char EmptyEntry[4] = "---";

	const s32 SizeOfNamesArray = ArrayOfThreadNames.GetCount();
	const s32 TotalNumberOfThreads = sm_Threads.GetCount();
	scriptAssertf((SizeOfNamesArray == 0) || (SizeOfNamesArray == TotalNumberOfThreads), "GtaThread::UpdateArrayOfNamesOfRunningThreads - array of script names has size %d. Expected it to be 0 or %d", SizeOfNamesArray, TotalNumberOfThreads);
	if (SizeOfNamesArray != TotalNumberOfThreads)
	{
		ArrayOfThreadNames.Reset();
		ArrayOfThreadNames.Resize(TotalNumberOfThreads);
	}
	for (int i=0; i<TotalNumberOfThreads; i++)
	{
		GtaThread *pGtaThread = static_cast<GtaThread*>(sm_Threads[i]);
		if (pGtaThread->IsValid() && (pGtaThread->GetState() != ABORTED))
		{
			ArrayOfThreadNames[i] = pGtaThread->GetScriptName();
		}
		else
		{
			ArrayOfThreadNames[i] = EmptyEntry;
		}
	}
}
#endif

#if __BANK
void GtaThread::DisplayAllRunningScripts(bool bOutputToScreen)
{
	for (int i=0; i<sm_Threads.GetCount(); i++)
	{
		if (sm_Threads[i]->GetThreadId())
		{
			int InstructionsUsed = sm_Threads[i]->GetInstructionsUsed();
			int MaxStackUseForThisThread = -1;
#if __DEV
			MaxStackUseForThisThread = sm_Threads[i]->GetMaxStackUse();
#endif
			int CurrentStackSize = ((GtaThread*) sm_Threads[i])->GetThreadSP();
			int MaxAllowedStackSize = ((GtaThread*) sm_Threads[i])->GetAllocatedStackSize();

			const u32 MaxLengthOfStateString = 12;
			char StateString[MaxLengthOfStateString];
			safecpy(StateString, "", MaxLengthOfStateString);

			switch (sm_Threads[i]->GetState())
			{
				case RUNNING :
					safecpy(StateString, "RUNNING", MaxLengthOfStateString);
					break;
				case BLOCKED :
					//	Don't bother saying that the thread is BLOCKED since this is the normal state
					break;
				case ABORTED :
					safecpy(StateString, "ABORTED", MaxLengthOfStateString);
					break;
				case HALTED :
					safecpy(StateString, "HALTED", MaxLengthOfStateString);
					break;
				default :
					scriptAssertf(0, "GtaThread::DisplayAllRunningScripts - unknown thread state");
					break;
			}

			const u32 MaxLengthOfNetworkString = 12;
			char NetworkString[MaxLengthOfNetworkString];
			safecpy(NetworkString, "", MaxLengthOfNetworkString);

			if (((GtaThread*) sm_Threads[i])->IsANetworkThread())
			{
				safecat(NetworkString, "[N]", MaxLengthOfNetworkString);
			}

			if (((GtaThread*) sm_Threads[i])->bSafeForNetworkGame)
			{
				safecat(NetworkString, "[NS]", MaxLengthOfNetworkString);
			}

			if (bOutputToScreen)
			{
				grcDebugDraw::AddDebugOutput("%s%s(Id=%d) InstCount=%d CurrStack=%d MaxStackSoFar=%d StackLimit=%d %s", NetworkString, sm_Threads[i]->GetScriptName(), sm_Threads[i]->GetThreadId(), InstructionsUsed, CurrentStackSize, MaxStackUseForThisThread, MaxAllowedStackSize, StateString);
			}
			else
			{
				Displayf("%s%s(Id=%d) InstCount=%d CurrStack=%d MaxStackSoFar=%d StackLimit=%d %s\n", NetworkString, sm_Threads[i]->GetScriptName(), sm_Threads[i]->GetThreadId(), InstructionsUsed, CurrentStackSize, MaxStackUseForThisThread, MaxAllowedStackSize, StateString);
			}

		}
	}
}

#endif

