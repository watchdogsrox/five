// File header
#include "game/GameSituation.h"

// Rage headers
#include "fwsys/timer.h"
#include "system/new.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CGameSituation
////////////////////////////////////////////////////////////////////////////////

CGameSituation* CGameSituation::sm_Instance = NULL;

CGameSituation::CGameSituation()
: m_fTimeInCombat(0.0f)
, m_uFlags()
{

}

CGameSituation::~CGameSituation()
{

}

void CGameSituation::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);

	//Update the flags.
	UpdateFlags();
}

void CGameSituation::Init(unsigned UNUSED_PARAM(initMode))
{
	//Create the instance.
	Assert(!sm_Instance);
	sm_Instance = rage_new CGameSituation;
}

void CGameSituation::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Free the instance.
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

void CGameSituation::ResetFlags()
{
	//Clear the flags.
	m_uFlags.ClearAllFlags();
}

void CGameSituation::Update()
{
	//Grab the time step.
	float fTimeStep = fwTimer::GetTimeStep();

	//Update the instance.
	GetInstance().Update(fTimeStep);
}

void CGameSituation::UpdateFlags()
{
	//Reset the flags.
	ResetFlags();
}

void CGameSituation::UpdateTimers(float fTimeStep)
{
	//Check if we are in combat.
	if(m_uFlags.IsFlagSet(Combat))
	{
		//Increase the time in combat.
		m_fTimeInCombat += fTimeStep;
	}
	else
	{
		//Clear the time in combat.
		m_fTimeInCombat = 0.0f;
	}
}
