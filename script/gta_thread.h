#ifndef _GTA_THREAD_H_
#define _GTA_THREAD_H_

// Rage headers
#include "parser/rtstructure.h"
#include "parser/treenode.h"
#include "script/thread.h"
#include "streaming/streamingmodule.h"
#if __BANK
#include "scriptgui/debugger.h"
#endif


// Game headers
#include "SaveLoad/savegame_data.h"
#if __BANK
#include "script/script_debug.h"
#endif
//#include "script/script_brains.h"

/*
// Keep all these stack sizes different from one another to catch when too many of one type has been used
#if !__FINAL
#define SOAK_TEST_STACK_SIZE        (4088)
#define NETWORK_BOT_STACK_SIZE		(4096)
#define DEBUG_SCRIPT_STACK_SIZE		(4080)
#define DEBUG_MP_HUD_STACK_SIZE		(5008)
#define DEBUG_MENU_STACK_SIZE		(24320)

#define NUM_SOAK_TEST_STACKS		(1)
#define NUM_NETWORK_BOT_STACKS		(31)
#define NUM_DEBUG_SCRIPT_STACKS		(1)
#define NUM_DEBUG_MP_HUD_STACKS		(2)
#define NUM_DEBUG_MENU_STACKS		(1)
#endif // !__FINAL

#define MICRO_STACK_SIZE			(128)
#define MINI_STACK_SIZE				(512)
#define DEFAULT_STACK_SIZE			(1024)
#define MULTIPLAYER_MISSION_STACK_SIZE		(1536)
#define FRIEND_STACK_SIZE			(1820)
#define SPECIAL_ABILITY_STACK_SIZE	(1828)	//	for NeilF's scripts - Jacob's gun car, car bomb, two extra peds etc.
#define SHOP_STACK_SIZE				(2024)
#define CELLPHONE_STACK_SIZE		(2552)
#define SCRIPT_XML_STACK_SIZE		(4592)
#define MISSION_STACK_SIZE			(8192)


#define NUM_MICRO_STACKS			(20)
#define NUM_MINI_STACKS				(20)
#define NUM_DEFAULT_STACKS			(68)
#define NUM_FRIEND_STACKS			(10)
#define NUM_SPECIAL_ABILITY_STACKS	(5)
#define NUM_SHOP_STACKS				(2)
#define NUM_MULTIPLAYER_MISSION_STACKS		(8)
#define NUM_CELLPHONE_STACKS		(3)
#define NUM_SCRIPT_XML_STACKS		(1)
#define NUM_MISSION_STACKS			(1)

#if __FINAL
#define MAX_SCRIPT_THREADS			(NUM_MICRO_STACKS + NUM_MINI_STACKS + NUM_DEFAULT_STACKS + NUM_FRIEND_STACKS + NUM_SPECIAL_ABILITY_STACKS + NUM_SHOP_STACKS + NUM_MULTIPLAYER_MISSION_STACKS + NUM_CELLPHONE_STACKS + NUM_SCRIPT_XML_STACKS + NUM_MISSION_STACKS)
#else
#define MAX_SCRIPT_THREADS			(NUM_NETWORK_BOT_STACKS + NUM_MICRO_STACKS + NUM_MINI_STACKS + NUM_DEFAULT_STACKS + NUM_FRIEND_STACKS + NUM_SPECIAL_ABILITY_STACKS + NUM_SHOP_STACKS + NUM_MULTIPLAYER_MISSION_STACKS + NUM_CELLPHONE_STACKS + NUM_DEBUG_SCRIPT_STACKS + NUM_DEBUG_MP_HUD_STACKS + NUM_SCRIPT_XML_STACKS + NUM_DEBUG_MENU_STACKS + NUM_MISSION_STACKS)
#endif // __FINAL
*/

// This class replaces CRunningScript

class CScriptsForBrains;
class CGameScriptId;
class CGameScriptHandler;
class CGameScriptHandlerNetComponent;

class GtaThread : public scrThread
{
public:
	static const int FORCE_CLEANUP_FLAG_PLAYER_KILLED_OR_ARRESTED = 1;
	static const int FORCE_CLEANUP_FLAG_SP_TO_MP = 2;
	static const int FORCE_CLEANUP_PAUSE_MENU_TERMINATED = 4;

	enum MatchingMode
	{
		MATCH_SCRIPT_NAME,
		MATCH_THREAD_ID,
		MATCH_ALL
	};

	CGameScriptHandler*	m_Handler;						// the script handler that is associated with this thread
	CGameScriptHandlerNetComponent* m_NetComponent;		// the network component of the handler, for quick access
	
	s32 m_HashOfScriptName;
	
	int			ForceCleanupPC, ForceCleanupFP, ForceCleanupSP;
	u32			m_ForceCleanupFlags;			//	bit field of flags to specify which ForceCleanup "events" a script thread will respond to
	u32			m_CauseOfMostRecentForceCleanup;

	int			InstanceId;					// used to distinguish between different instances of the same script in a network game

	int			EntityIndexForScriptBrain;

	bool		IsThisAMissionScript;		// TRUE for a script which contains a mission
	bool		bSafeForNetworkGame;		// TRUE if the script should persist when a network game is started/ended
	bool		bSetPlayerControlOnInMissionCleanup;	//	TRUE (default) if the player control should be switched on when a proper mission ends
	bool		bClearHelpInMissionCleanup;				//	TRUE (default) if the help text should be cleared when a proper mission ends
	bool		bIsThisAMiniGameScript;		// Set by a script Command - used to decide which script can display help messages when a mini-game is in progress
	bool		bThisScriptAllowsNonMiniGameHelpMessages;
	bool		bThreadHasBeenPaused;		// This script should not be executed as it has been paused (by PAUSE_GAME)
	bool		bThisScriptCanBePaused;		// If false, PAUSE_GAME does not affect this script
	bool		bThisScriptCanRemoveBlipsCreatedByAnyScript;

	bool		m_bDisplayProfileData;
	bool		m_bIsHighPrio;

	u8		m_ForceCleanupTriggered;		//	will be set to a non-zero value when the ForceCleanup code should be run
	s8		ScriptBrainType;			// -1 or one of the values in the enum in CScriptsForBrains

#if __BANK
	scrDebugger *pDebugger;
	CScriptWidgetTree m_WidgetTree;
#endif

public:

	// PURPOSE:	Constructor
	GtaThread();

	// PURPOSE:	Destructor
	~GtaThread();

	static void StaticInit(unsigned initMode);

	// PURPOSE:	Reset the thread to a runnable state at the top of the current program
	virtual void Reset(scrProgramId program,const void *argStruct,int argStructSize);

	// PURPOSE:	Execute script instructions.
	// PARAMS:	insnCount - Reference to a variable containing the maximum number of instructions
	//				to execute before returning.  Modified after this call to contain the number
	//				of instructions remaining.  Can be zero, in which case the script does nothing.
	// RETURNS:	New machine state
//	scrThread::State Run(int &insnCount);

#if __DEV
	// PURPOSE:	Dumps thread state to console (program counter, stack contents, etc)
//	void DumpState(void (*printer)(const char *fmt,...));
#endif

	// PURPOSE:	Reserve space for fixed number of threads
	// NOTES:	Reimplement this in a subclass and call that version if you subclass
	//			the script thread.
	static void AllocateThreads();

	static void DeallocateThreads();

	// PURPOSE:	Update TIMERA and TIMERB values and then call Run.
	// PARAMS:	insnCount - As per scrThread::Run
	// RETURNS:	New thread state as per scrThread::Run
	// NOTES:	This functionality is separated out mostly for single-stepping support
	State Update(int insnCount);

	// PURPOSE: Virtual entry point called by KillAllThreads on each thread.
	void Kill();	//	 { }

	bool IsValid() const { return (m_Serialized.m_ThreadId != 0); }

	void TidyUpMiniGameFlag();

	// PURPOSE: returns true if this is a network script (is synched over the network). Threads with bSafeForNetworkGame set are not necessarily network scripts.
	bool IsANetworkThread();

#if !__NO_OUTPUT
	static GtaThread* GetThreadWithThreadId(const scrThreadId threadId);
#endif 

	//	KillAllAbortedScriptThreads - if a thread kills itself then it just sets its state to ABORTED.
	//	In order to properly kill an ABORTED thread, we need to call Kill again.
	//	Calling KillAllAbortedScriptThreads every frame should allow the Preview folder to work for sco files 
	//	because this second Kill will decrement the reference for the associated scrProgram.
	static void KillAllAbortedScriptThreads();

	static void ForceCleanupForAllMatchingThreads(u32 ForceCleanupFlags, MatchingMode ForceCleanupMatchingMode, const char *pScriptName, const scrThreadId ThreadId);
	static void KillAllThreadsWithThisName(const char *pProgramName);
//	static void KillAllThreadsForNetworkGame(void);
//	static void KillAllThreadsForSinglePlayerGame(void);
	static void PauseAllThreadsBarThisOne(GtaThread *pThread);
	static void UnpauseAllThreads();
//	static bool AreThereAnyNonNetworkScriptsRunning(void);
	static s32 CountActiveThreads();
	static s32 CountActiveThreadsWithThisProgramID(const scrProgramId programid);

	static u32 CountSinglePlayerOnlyScripts();

	bool ForceCleanup(u32 ForceCleanupFlags );
	void SetForceCleanupReturnPoint(u32 ForceCleanupBitField);
	void DoForceCleanupCheck();

	void Initialise();

	//Get the current program counter
	int GetThreadPC() { return m_Serialized.m_PC; }
	
	//Gets the current size of the used stack
	int GetThreadSP() { return m_Serialized.m_SP; }
	
	//Gets the size of the allocated stack size
	int GetAllocatedStackSize() { return m_Serialized.m_StackSize; }

	int GetIDForThisLineInThisThread();

	// Need to undo any pause on the script so it can update and clean up properly
	void SetExitFlag() { m_bExitFlagSet = true; bThreadHasBeenPaused = false; CancelWait(); }
	void SetExitFlagResponse() { m_bExitFlagResponse = true; }
	bool IsExitFlagResponseSet() const { return m_bExitFlagResponse; }
	bool IsExitFlagSet() const { return m_bExitFlagSet; }

#if __BANK
	static void AllocateDebuggerForThread(scrThreadId ThreadID);
	static void AllocateDebuggerForThreadInSlot(s32 ArrayIndex);

	static void EnableProfilingForThreadInSlot(s32 ArrayIndex);

	static void UpdateArrayOfNamesOfRunningThreads(atArray<const char*> &ArrayOfThreadNames);

	static void DisplayAllRunningScripts(bool bOutputToScreen);
#endif	//	__BANK

	void SetHasUsedPedPathCommand(bool bNewValue) { m_bHasUsedPedPathCommand = bNewValue; }
	bool GetHasUsedPedPathCommand() { return m_bHasUsedPedPathCommand; }
	void SetHasOverriddenVehicleEntering(bool bNewValue) { m_bHasOverriddenVehicleEntering = bNewValue; }
	bool GetHasOverriddenVehicleEntering() { return m_bHasOverriddenVehicleEntering; }

	void SetIsARandomEventScript(bool bIsARandomEvent) { m_bIsARandomEventScript = bIsARandomEvent; }
	bool GetIsARandomEventScript() { return m_bIsARandomEventScript; }

	void SetIsATriggerScript(bool bIsATriggerScript) { m_bIsATriggerScript = bIsATriggerScript; }
	bool GetIsATriggerScript() { return m_bIsATriggerScript; }

#if GTA_REPLAY
	void SetNeedsReplayCleanup(bool bCleanupReplay) { m_bCleanupReplay = bCleanupReplay; }
	static bool GetShouldAutoCleanupReplay();
#endif // GTA_REPLAY
	

	static void RegisterGtaBuiltinCommands();

#if !__FINAL
	bool UseDebugPortionOfScriptTextArrays() { return sm_DebugMenuStackSize == GetAllocatedStackSize(); }

	static s32 GetSoakTestStackSize() { return sm_SoakTestStackSize; }
	static s32 GetNetworkBotStackSize() { return sm_NetworkBotStackSize; }
#endif	//	!__FINAL
	static s32 GetDefaultStackSize() { return sm_DefaultStackSize; }
	static s32 GetMissionStackSize() { return sm_MissionStackSize; }

	static void ScriptThreadIteratorReset();
	static s32 ScriptThreadIteratorGetNextThreadId();

	static void SetHasLoadedAllPathNodes(scrThreadId threadID);
	static void	ClearHasLoadedAllPathNodes(ASSERT_ONLY(scrThreadId threadID));	// script Thread ID passed in __BANK to compare. The wrong script should not be clearing this member.
	static const scrThreadId &GetHasLoadedAllPathNodesScriptId()	{ return ms_LastPathNodeScriptThreadID; }

private:
	static void CommandStartNewSavedScript(scrThread::Info &info);
	static void CommandStartNewSavedScriptWithArgs(scrThread::Info &info);

#if __ASSERT
	void VerifyNetworkSafety();
#endif	//	__ASSERT

#if !__FINAL
	static s32 sm_SoakTestStackSize;
	static s32 sm_NetworkBotStackSize;
	static s32 sm_DebugMenuStackSize;
#endif	//	!__FINAL
	static s32 sm_DefaultStackSize;
	static s32 sm_MissionStackSize;

	static s32* GetCodeStackSize( const atHashValue& name );
	static bool IsDebugStack( const atHashValue& name );
#if __BANK
	void AllocateDebugger();
#endif

private:
	friend class CScriptsForBrains;	//	so that CScriptsForBrains::StartNewStreamedScriptBrain can call scrThread::StartNewScriptWithArgs
	static sysMemAllocator* sm_Allocator;	// The allocator to use for thread objects and stacks
	static s32 ScriptThreadIteratorIndex;

	static scrThreadId ms_LastPathNodeScriptThreadID;

	u32 m_bHasUsedPedPathCommand				:1;
	u32 m_bHasOverriddenVehicleEntering			:1;
	u32 m_bIsARandomEventScript					:1;
	u32 m_bIsATriggerScript						:1;
	bool m_bExitFlagSet							:1;
	bool m_bExitFlagResponse					:1;
#if GTA_REPLAY
	bool m_bCleanupReplay						:1;
	static bool sm_bTriggerAutoCleanup;
#endif // GTA_REPLAY
	
};


#endif	//	_GTA_THREAD_H_

