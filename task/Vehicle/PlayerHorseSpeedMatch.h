#ifndef PLAYER_HORSE_SPEED_MATCH_H
#define PLAYER_HORSE_SPEED_MATCH_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "scene/Physical.h"
#include "scene/RegdRefTypes.h"
#include "Task/Vehicle/PlayerHorseFollowTargets.h"

class CPed;


class CPlayerHorseSpeedMatch
{
public:
	CPlayerHorseSpeedMatch(const CPed* pPlayerPed);
	virtual ~CPlayerHorseSpeedMatch();
	virtual void Reset();

	CPlayerHorseFollowTargets &GetHorseFollowTargets() 
	{	return m_HorseFollowTargets;	}

	const CPlayerHorseFollowTargets &GetHorseFollowTargets() const
	{	return m_HorseFollowTargets;	}

	//has the player met the input conditions to speed match?
	bool IsNewHorseFollowReady() const;

	void ResetCurrentHorseFollowTarget()
	{
		m_CurrentHorseFollowTarget.Reset(NULL);
	}

	const CPhysical* GetCurrentHorseFollowTarget() const
	{
		return m_CurrentHorseFollowTarget.Get();
	}

	void SetCurrentHorseFollowTarget(const CPhysical* target)
	{
		m_CurrentHorseFollowTarget = target;
	}

	bool HasHorseFollowTarget() const
	{
		return (m_CurrentHorseFollowTarget != NULL);
	}

	float GetCurrentSpeedMatchSpeed() const
	{
		return m_CurrentSpeedMatchSpeed;
	}

	void SetCurrentSpeedMatchSpeed(float speed) 
	{
		m_CurrentSpeedMatchSpeed = speed;
	}

	void ResetCurrentSpeedMatchSpeed()
	{
		m_CurrentSpeedMatchSpeed = -1.0f;
	}

	bool FollowTargetIsPosseLeader() const
	{
		return m_HorseFollowTargets.TargetIsPosseLeader(m_CurrentHorseFollowTarget);
	}

	bool IsRidingInPosse() const
	{
		return IsNewHorseFollowReady() && HasHorseFollowTarget() && FollowTargetIsPosseLeader();
	}

	//has the player met the input conditions, and does
	//he have a valid target?
	bool IsNewHorseFollowActive() const
	{
		return IsNewHorseFollowReady()
			&& HasHorseFollowTarget();
	}

	void UpdateNewHorseFollow(bool& bMaintainSpeedOut, bool bSpurredThisFrame);

	void SetPlayerPed(const CPed* pPlayerPed) { m_pPlayerPed = pPlayerPed; }


	const CPed*	m_pPlayerPed;

	CPlayerHorseFollowTargets m_HorseFollowTargets;
	float						m_HorseFollowActivationTime;
	float						m_HorseFollowTimeLastActive;
	RegdConstPhy				m_CurrentHorseFollowTarget;
	//bool						m_HasHorseFollowTarget;
	static float				sm_HorseFollowActivationTime;
	static float				sm_HorseFollowFadeoutTime;
	float						m_CurrentSpeedMatchSpeed;
};

#endif // ENABLE_HORSE

#endif	// #ifndef PLAYER_HORSE_SPEED_MATCH_H
