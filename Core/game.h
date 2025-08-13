//
// name:		game.h
// description:	class controlling the game code
#ifndef INC_GAME_H_
#define INC_GAME_H_

#include "fwsys/gameskeleton.h"
#include "fwutil/Gen9Settings.h"
#include "core/gamesessionstatemachine.h"

// --- Forward Definitions ------------------------------------------------------
namespace rage
{
	class rlPresence;
	class rlPresenceEvent;
	class rlNp;
	class rlNpEvent;
}

// --- Structure/Class Definitions ----------------------------------------------
class CGameTempAllocator : public sysMemAllocator
{
	friend class CGame;

private:
	CGameTempAllocator() { }

public:
	virtual void* TryAllocate(size_t size,size_t align,int heapIndex = 0);
	virtual void* Allocate(size_t size, size_t align, int heapIndex = 0);
	virtual void Free(const void *ptr);

	virtual size_t GetMemoryUsed(int UNUSED_PARAM(bucket) = -1 /*all buckets*/) {return 0;}
	virtual size_t GetMemoryAvailable() {return 0;}
};

class CGame
{
private:

    static gameSkeleton m_GameSkeleton;
    static CGameSessionStateMachine m_GameSessionStateMachine;

public:
	static void PreLoadingScreensInit();
	static void Init();
	static void Shutdown();
	static void InitLevel(int level);
	static void InitLevelAfterLoadingFromMemoryCard(int level);
	static void ShutdownLevel();
	static void InitProfile();
	static void ShutdownProfile();
    static void InitSession(int level, bool bLoadingAPreviouslySavedGame);
	static void ShutdownSession();
	static void HandleSignInStateChange();

#if GEN9_LANDING_PAGE_ENABLED
	static void ReturnToLandingPageStateMachine(const ReturnToLandingPageReason reason) { m_GameSessionStateMachine.ReturnToLandingPage(reason); }
#endif

	static void ReInitStateMachine(int level) { m_GameSessionStateMachine.ReInit(level); }
	static void UpdateSessionState()	 { m_GameSessionStateMachine.Update(); }
    static bool IsChangingSessionState() { return !m_GameSessionStateMachine.IsIdle(); }
    static void StartNewGame(int level)  { m_GameSessionStateMachine.StartNewGame(level); }
	static void LoadSaveGame()           { m_GameSessionStateMachine.LoadSaveGame(); }
	static void HandleLoadedSaveGame()	 { m_GameSessionStateMachine.HandleLoadedSaveGame(); }
	static void SetStateToIdle()		 { m_GameSessionStateMachine.SetStateToIdle(); }
    static void ChangeLevel(int level)   { m_GameSessionStateMachine.ChangeLevel(level); }

#if RSG_PC
	static bool IsCheckingEntitlement() { return m_GameSessionStateMachine.IsCheckingEntitlement(); }
	static bool IsEntitlementUpdating() { return m_GameSessionStateMachine.IsEntitlementUpdating(); }
	static void InitCallbacks();
#endif //RSG_PC

	static void Update();
	static void Render();

#if COMMERCE_CONTAINER
	static void MarketplaceUpdate();
#endif

	static void DiskCacheSchedule();

	//Callback for presence events (signin, signout, friends events, etc.)
	static void OnPresenceEvent(const rlPresenceEvent* evt);
#if RSG_ORBIS
	static void OnNpEvent(rlNp* /*np*/, const rlNpEvent* evt);
	static void SaveGameSetup();
#endif

    static bool ShouldDoFullUpdateRender();

	static void GameExceptionDisplay();
	static void OutOfMemoryDisplay();
	ASSERT_ONLY(static void VerifyMainThread();)

    static bool IsSessionInitialized();
	static bool IsLevelInitialized();

private:
	// The nav exporter wants to call CreateFactories() and other private functions,
	// at least for now, this is allowed through this friend declaration. May want to
	// look for a better solution. /FF
	friend class CLevelProcessToolGameInterfaceGta;

	static void CreateFactories();
	static void DestroyFactories();

#if __BANK
	static void InitWidgets();
	static void InitLevelWidgets();
	static void ShutdownLevelWidgets();
	static void ProcessWidgetCommandLineHelpers();
#endif // __BANK

#if __ASSERT
	static void InitStringHacks();
#endif //__ASSERT

	static void RegisterStreamingModules();

    // Registers all initialisation, shutdown and update functions with the game skeleton
    static void RegisterGameSkeletonFunctions();

    static void RegisterCoreInitFunctions();
    static void RegisterCoreShutdownFunctions();
    static void RegisterBeforeMapLoadedInitFunctions();
    static void RegisterAfterMapLoadedInitFunctions();
    static void RegisterAfterMapLoadedShutdownFunctions();
    static void RegisterSessionInitFunctions();
    static void RegisterSessionShutdownFunctions();

    static void RegisterCommonMainUpdateFunctions();
    static void RegisterFullMainUpdateFunctions();
    static void RegisterSimpleMainUpdateFunctions();

#if GEN9_LANDING_PAGE_ENABLED
	static void RegisterLandingPagePreInitUpdateFunctions();
	static void RegisterLandingPageUpdateFunctions();
#endif // GEN9_LANDING_PAGE_ENABLED
    
#if COMMERCE_CONTAINER
	static void RegisterCommerceMainUpdateFunctions();
#endif

	static void RenderDebug();

    static void TimerInit(unsigned initMode);

	static void ViewportSystemInit(unsigned initMode);
	static void ViewportSystemShutdown(unsigned shutdownMode);

	static void ProcessAttractMode();
};


class CMultiViewportDemo
{
public:
	CMultiViewportDemo()	{m_bMultiViewportDemoRunning = false; m_multiViewportDemoPhase = 0;}
	~CMultiViewportDemo()	{}
	void	SetupCameraMultiViewports();
	void	StartMultiViewportsDemo();
	void	StopMultiViewportsDemo();
	void	ProcessMultiViewportDemo();
	void	NextMultiViewportsDemo();
	void	MultiViewportsDemo_ScaleMainViewport(bool bIn);
private:
	bool	m_bMultiViewportDemoToggle;
	bool	m_bMultiViewportDemoRunning;
	s32	m_multiVieportCamIds[4];
	s32   m_multiViewportDemoPhase;
};

#endif // INC_GAME_H_
