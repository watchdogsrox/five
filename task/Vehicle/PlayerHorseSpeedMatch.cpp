#include "Task/Vehicle/PlayerHorseSpeedMatch.h"

#if ENABLE_HORSE

#include "Peds/ped.h"
#include "System/control.h"


AI_OPTIMISATIONS()

//float sagPlayerInputHorse::sm_HorseFollowActivationTime = 0.0f;
float CPlayerHorseSpeedMatch::sm_HorseFollowActivationTime = 1.0f;
float CPlayerHorseSpeedMatch::sm_HorseFollowFadeoutTime = 1.0f;


CPlayerHorseSpeedMatch::CPlayerHorseSpeedMatch(const CPed* pPlayerPed) :
	m_pPlayerPed(pPlayerPed)
{
	Reset();
}

CPlayerHorseSpeedMatch::~CPlayerHorseSpeedMatch()
{
}

////////////////////////////////////////////////////////////////////////////////

void CPlayerHorseSpeedMatch::Reset()
{
	m_HorseFollowActivationTime = 0.0f;
	m_CurrentHorseFollowTarget.Reset(NULL);
	m_CurrentSpeedMatchSpeed = -1.0f;
}

bool CPlayerHorseSpeedMatch::IsNewHorseFollowReady() const
{
	return m_HorseFollowTimeLastActive >= TIME.GetElapsedTime() - sm_HorseFollowFadeoutTime;
}

//if bMaintainSpeed is true coming into this function,
//reset the timeout
//if it is false, test against all the other things that can cause us to maintain
//speed:  lookstick, holding b, etc.  if the timer hasn't expired yet, then reset it
void CPlayerHorseSpeedMatch::UpdateNewHorseFollow(bool& bMaintainSpeedOut, bool bSpurredThisFrame)
{
	Assert(m_pPlayerPed);
	if (!m_pPlayerPed)
		return;

	//if we aren't holding the button, reset the activation timer
	if (!bMaintainSpeedOut)
	{
		m_HorseFollowActivationTime = 0.0f;
	}
	else
	{
		//otherwise, accumulate the time this button is held down
		m_HorseFollowActivationTime += TIME.GetSeconds();
	}

	//if we've held the button long enough, activate the follow behavior
	if (m_HorseFollowActivationTime > sm_HorseFollowActivationTime)
	{
		m_HorseFollowTimeLastActive = TIME.GetElapsedTime();
	}

	//if we're already active, and the user does something like 
	//pan the camera, we want to maintain activation of the behavior
	//the difference is these things can't activate the behavior if the
	//player wasn't already following
	if (IsNewHorseFollowReady())
	{
		const CControl* pControl = m_pPlayerPed->GetControlFromPlayer();

		//if the player spurs, break him out of the behavior immediately
		if (bSpurredThisFrame || pControl->GetVehicleHandBrake().IsDown())
		{
			m_HorseFollowActivationTime = 0.0f;
			m_HorseFollowTimeLastActive = 0.0f;
			bMaintainSpeedOut = false;
			return;
		}

		bool bIsSteering = !(IsNearZero(pControl->GetPedWalkLeftRight().GetNorm()) || IsNearZero(pControl->GetPedWalkUpDown().GetNorm()));
		
		if (bIsSteering
			|| pControl->GetVehicleHandBrake().IsDown()
			|| pControl->GetSelectWeaponSpecial().IsDown())
		{
			m_HorseFollowTimeLastActive = TIME.GetElapsedTime();
		}

		//moved this outside of the above block so that maintain speed out is always
		//true if within the fadeout time
		bMaintainSpeedOut = true;
	}
}

#endif // ENABLE_HORSE