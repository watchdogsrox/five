#ifndef __WARP_MANAGER_H_
#define __WARP_MANAGER_H_

#include "scene/world/gameworld.h"
#include "peds/ped.h"
#include "vehicles/vehicle.h" 
#include "game/Clock.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"

class CWarpManager
{
public:

	enum eFlags
	{
		FLAG_NO_LOADSCENE = 1,
		FLAG_HD_ASSETS = 1<<1
	};

	static const unsigned MAX_WAIT_TIME_MS = 15000;

public:

	static void	Update();
	static bool	SetWarp(const Vector3 &pos, const Vector3 &vel, float fHeadingInRadians, bool teleportVehicle, bool bSnapToGround, float fRadius = -1.0f, bool bFromMap = false);
	static bool IsActive() { return m_eState != STATE_INACTIVE; }
	static void	StopWarp();
	static float CalculateLoadSceneRadius(float &radius);
	static u32	CalculateLoadSceneFlags(const float &radius);
	static bool HaveVehicleModsLoaded();
	
	static void FadeOutPlayer(bool bSet)		{ m_bFadeOutPlayer = bSet; }
	static void FadeOutAtStart(bool bSet)		{ m_bFadeOutAtStart = bSet; }
	static void FadeInWhenComplete(bool bSet)	{ m_bFadeInWhenComplete = bSet; }
	static void InstantFillPopulation(bool bSet){ m_bInstantFillPopulation = bSet; }

	static void SetScriptThreadId(scrThreadId scriptThreadId);

#if __BANK

	static void	SetNoLoadScene()				{ m_Flags |= FLAG_NO_LOADSCENE; }
	static void	SetWasDebugStart(bool val);
	static void	SetPostWarpCameraPos(Vector3 &pos, Vector3 &front);
	static void	SetPostWarpTimeOfDay(s32 hours, s32 minutes);
	static void DoPreWarpDebugStuff();
	static void DoPostWarpDebugStuff();

#endif	// BANK

	BANK_ONLY(static void WaitUntilTheSceneIsLoaded(bool await) { sm_WaitUntilSceneLoaded = await;});
	BANK_ONLY(static void FadeCamera(bool fade) { sm_FadeCamera = fade;});

private:

	enum eState
	{
		STATE_INACTIVE = 0,
		STATE_FADE_OUT,
		STATE_START_WARP,
		STATE_DO_WARP,
		STATE_END_WARP,
		STATE_FADE_IN,
		STATE_TOTAL
	};

	static RegdPhy	    m_pPhysical;
	static eState		m_eState;
	static Vector3		m_Pos;
	static Vector3		m_Vel;
	static float		m_fHeading;
	static float		m_fRadius;
	static bool			m_bSnapToGround;
	static u32			m_StartTime;
	static bool			m_StopWarp;
	static bool			m_bFadeOutAtStart;
	static bool			m_bFadeOutPlayer;
	static bool			m_bFadeInWhenComplete;
	static bool			m_bInstantFillPopulation;
	static bool			m_bOwnedByScript;
	static bool			m_bFromMap;
	static scrThreadId  m_scriptThreadId;
	static u32			m_Flags;

	BANK_ONLY(static Vector3	m_CamPos);
	BANK_ONLY(static Vector3	m_CamFront);
	BANK_ONLY(static bool		m_SetCamPos);

	BANK_ONLY(static s32		m_ClockHours);
	BANK_ONLY(static s32		m_ClockMinutes);
	BANK_ONLY(static bool		m_SetTimeOfDay);
	BANK_ONLY(static bool		m_WasDebugStart);

	//Used for audio purpose to do some offline work, we need the scene completely loaded before we start calculating things. 
	BANK_ONLY(static bool sm_WaitUntilSceneLoaded);
	BANK_ONLY(static bool sm_FadeCamera);
};

#endif	//__WARP_MANAGER_H_
