/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/InstancePriority.h
// PURPOSE : adjusts map entity instancing priority based on game behaviour and script hints
// AUTHOR :  Ian Kiigan
// CREATED : 17/04/13
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_INSTANCEPRIORITY_H_
#define _SCENE_INSTANCEPRIORITY_H_

#if __BANK
#include "bank/bank.h"
#include "bank/bkmgr.h"
#endif	//__BANK

class CInstancePriority
{
public:
	enum ePlayMode
	{
		PLAY_MODE_SINGLEPLAYER,
		PLAY_MODE_MULTIPLAYER
	};

	enum eScriptHint
	{
		SCRIPT_HINT_NONE,
		SCRIPT_HINT_SHOOTING,
		SCRIPT_HINT_DRIVING,
		SCRIPT_HINT_SNIPING,
		SCRIPT_HINT_HELICAM
	};

	static void Init();
	static void Update();
	static void SetMode(s32 mode);
	static void SetScriptHint(s32 hint) { ms_scriptHint=hint; }
	static u32 GetCurrentPriority() { return ms_entityInstancePriority; }

#if RSG_PC
	static void SetForceLowestPriority(bool b) { ms_bForceLowestPriority = b; }
#endif // RSG_PC

private:
	static void FlushHdMapEntities();
	static void CheckForPlayerMovement();

	enum
	{
		PRIORITY_LOWEST	= 0,
		PRIORITY_LOW,
		PRIORITY_NORMAL,
		PRIORITY_HIGH
	};

	static s32 ms_scriptHint;
	static s32 ms_playMode;
	static u32 ms_entityInstancePriority;

	static bool ms_bFlyingFastRecently;
	static u32 ms_flyingFastTimeStamp;
#if RSG_PC
	static bool ms_bForceLowestPriority;
#endif // RSG_PC
	//////////////////////////////////////////////////////////////////////////
	// DEBUG WIDGETS
#if __BANK
	
public:
	static void	InitWidgets(bkBank& bank);
	static void	UpdateDebug();

private:
	static void ChangeModeCB();
	static void ChangePriorityCB();

	static bool ms_bDisplayInstanceInfo;
	static bool ms_bOverrideInstancePriority;
	static s32 ms_dbgOverrideInstancePriority;
	
#endif	//__BANK
	//////////////////////////////////////////////////////////////////////////
};

#endif	//_SCENE_INSTANCEPRIORITY_H_