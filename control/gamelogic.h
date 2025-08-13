

/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    gamelogic.cpp
// PURPOSE : Stuff having to do with the player dying/respawning etc
// AUTHOR :  Obbe.
// CREATED : 4/8/2000
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _GAMELOGIC_H_
#define _GAMELOGIC_H_

//rage headers
#include "vector/vector3.h"
#include "fwnet/nettypes.h"

#include "scene/WarpManager.h"

class CPed;
class CVehicle;

#define NUM_SHORTCUT_START_POINTS (16)


/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions. (The functions are all static)
/////////////////////////////////////////////////////////////////////////////////

class CGameLogic
{
public:

	//Game states
	enum GameState
	{ 
		GAMESTATE_PLAYING, 
		GAMESTATE_DEATH, 
		GAMESTATE_ARREST, 
		GAMESTATE_DEATH_FADE, 
		GAMESTATE_ARREST_FADE,
		GAMESTATE_RESPAWN
	};

public:

	static const int DEFAULT_EPISODE_INDEX = 1;

	static const int DEFAULT_LEVEL_INDEX = 1;

#if __FINAL
	static const int MAX_LEVEL_INDEX = 5;
#else
	static const int MAX_LEVEL_INDEX = 10;
#endif // __FINAL

private:

	// Represents the possible game states(see enum sm_GameState for details on states)
	static	GameState  sm_GameState;

	// When did the player die/get arrested etc
	static	u32 sm_TimeOfLastEvent;

	// Time of last respawn after arrest
	static	u32 sm_TimeOfLastRespawnAfterArrest;

	// Time of last respawn after death
	static	u32 sm_TimeOfLastRespawnAfterDeath;

	// time when we stop waiting for the scene to load
	static	u32 sm_TimeToAbortSceneLoadWait;

	static	u32 sm_TimeOfLastTeleport;
	static bool	sm_bTeleportActive;

	// This is the stuff dealing with shortcuts after death/arrest
	static	Vector3		ShortCutDropOffForMission;
	static	float		ShortCutDropOffOrientationForMission;
	static	bool		MissionDropOffReadyToBeUsed;

    static  Vector3     sm_lastArrestOrDeathPos;
    static  bool        sm_hasLastArrestOrDeathPos;

#if !__FINAL
	static	bool		bDisable2ndPadForDebug;
#endif

#if __BANK
	static void			TeleportPlayerCb();
	static void			UpdateTeleportLocationCb();

	static bool			keepVehicleOnTeleport;
	static u32			maxTeleportWaitTimeMs;
	static s32			teleportLocationIndex;
	static float		teleportX;
	static float		teleportY;
	static float		teleportZ;

	static bool			teleportHasFinished;
#endif

private:

	// Current level index - Should only be read during game playing
	static int  sm_CurrentLevel;

	// Requested level index - Should only be read during initialization (either when the game is first run or
	//   when a session or level is restarted)
	static int  sm_RequestedLevel;

	static bool sm_bSuppressArrestStateTransitionThisFrame;

public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

	static void	Update();

	static bool IsRunningGTA5Map();

#if __BANK
	static bkBank* GetGameLogicBank();
	static void InitWidgets();
	static void UpdateWidgets();
#endif

    static void UpdateNetwork();

	static void	ResurrectLocalPlayer(const Vector3& NewCoors, float NewHeading, const bool leaveDeadPed = true, bool advanceTime = true, bool unpauseRenderPhases = true, int spawnLocation = 0, int spawnReason = 0);
	static void TeleportPlayer(CPed* ped, const Vector3& coords, float headingInRadians, bool teleportVehicle, bool snapToGround, u32 maxWaitTimeMs = CWarpManager::MAX_WAIT_TIME_MS, bool fadePlayerOut = false);
	static bool HasTeleportPlayerFinished();
	static bool IsTeleportActive();
	static void StopTeleport();

	static void	ResurrectClonePlayer(CPed *pPlayerPed, const Vector3& NewCoors, const u32 timeStamp, bool bLeaveTemporaryPedCorpse = false);

	//Access current game state.
	static GameState GetGameState() { return sm_GameState; }
	static bool IsGameStateInPlay()          { return (sm_GameState == GAMESTATE_PLAYING); }
	static bool IsGameStateInDeath()         { return (sm_GameState == GAMESTATE_DEATH); }
	static bool IsGameStateInArrest()        { return (sm_GameState == GAMESTATE_ARREST); }
	static bool IsGameStateInDeathFade()     { return (sm_GameState == GAMESTATE_DEATH_FADE); }

	//Get Time of last Respawn after arrest.
	static	u32 GetTimeOfLastRespawnAfterArrest() { return sm_TimeOfLastRespawnAfterArrest; }

	//Get Time of last Respawn after death.
	static	u32 GetTimeOfLastRespawnAfterDeath() { return sm_TimeOfLastRespawnAfterDeath; }

	//Current Level index accessors.
	//This index is only valid during gameplay.
	static s32 GetCurrentLevelIndex();
	static void  SetCurrentLevelIndex(const int level);

	//Requested Level index accessors.
	static s32 GetRequestedLevelIndex();
	static void  SetRequestedLevelIndex(const int level);

	static void SuppressArrestState() { sm_bSuppressArrestStateTransitionThisFrame=true; }
	static void ForceStatePlaying();
};

#endif
