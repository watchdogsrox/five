// Title	:	Performance.h
// Author	:	Phil Hooker
// Started	:	05/02/08
//
// Purpose : Keeps track of the estimated games performance, stores information
//			 That other systems can query to reduce features in order to improve framerate.
//

#include "game/Performance.h"
#include "game/wanted.h"
#include "scene/world/gameWorld.h"
#include "fwsys/timer.h"
#include "vehicles/Vehicle.h"

CPerformance* CPerformance::ms_pInstance = NULL;

void CPerformance::Init(unsigned /*initMode*/)
{
	Assert(CPerformance::ms_pInstance == NULL);
	CPerformance::ms_pInstance = rage_new CPerformance();
}

void CPerformance::Shutdown(unsigned /*shutdownMode*/)
{
	Assert(CPerformance::ms_pInstance != NULL);
	delete CPerformance::ms_pInstance;
	CPerformance::ms_pInstance = NULL;
}


CPerformance::CPerformance()
:	m_iLowPerformanceFlags(0),
	m_fLastTimePlayerMovingAtHighSpeed(0.0f),
	m_bSetLowPerformanceFlagFromScript(false)
{

}

CPerformance::~CPerformance()
{

}

// If this returns true, conditions in the game are such that poor performance is anticipated.
bool CPerformance::EstimateLowPerformance()
{
	return m_iLowPerformanceFlags != 0;
}

// Main update, check each of the performance estimation fns.
void CPerformance::Update()
{
	PF_START_TIMEBAR("Performance");

	if( fwTimer::IsGamePaused() )
		return;

	m_iLowPerformanceFlags = 0;

	CheckWantedLevel();
	CheckPlayerSpeed();
	CheckLowMissionPerformance();
}

// Check the players wanted level, a level of 2 or more will create cops and shooting
void CPerformance::CheckWantedLevel()
{
	if( CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL2 )
		m_iLowPerformanceFlags |= PF_WantedLevel;
}

float CPerformance::ms_fPlayerSpeedConsideredHigh = 15.0f;
float CPerformance::ms_fTimeToReportLowPerformanceOnHighSpeed = 5.0f;
// Check the players speed, if they more quickly, report the performance being low for a set time period.
void CPerformance::CheckPlayerSpeed()
{
	// Count down how long performance should be reported low
	if( m_fLastTimePlayerMovingAtHighSpeed > 0.0f )
		m_fLastTimePlayerMovingAtHighSpeed -= fwTimer::GetTimeStep();

	// If the player is moving quickly, set the timer, and allow it to count down
	if( CGameWorld::FindLocalPlayerVehicle() && CGameWorld::FindLocalPlayerVehicle()->GetVelocity().Mag2() > rage::square(ms_fPlayerSpeedConsideredHigh) )
	{
		m_fLastTimePlayerMovingAtHighSpeed = ms_fTimeToReportLowPerformanceOnHighSpeed;
	}

	// Performance is low if the player has been moving quickly in the last ms_fTimeToReportLowPerformanceOnHighSpeed seconds.
	if( m_fLastTimePlayerMovingAtHighSpeed > 0.0f )
		m_iLowPerformanceFlags |= PF_PlayerSpeed;
}

// Checks a reset bool set by script that estimates the performance is low
void CPerformance::CheckLowMissionPerformance()
{
	if( m_bSetLowPerformanceFlagFromScript )
		m_iLowPerformanceFlags |= PF_MissionFlag;

	m_bSetLowPerformanceFlagFromScript = false;
}

