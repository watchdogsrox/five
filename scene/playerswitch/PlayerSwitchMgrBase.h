/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchMgrBase.h
// PURPOSE : base classes related to player switch management
// AUTHOR :  Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_PLAYERSWITCH_PLAYERSWITCHMGRBASE_H_
#define _SCENE_PLAYERSWITCH_PLAYERSWITCHMGRBASE_H_

#include "atl/hashstring.h"
#include "peds/ped.h"
#include "vectormath/vec3v.h"

class CPlayerSwitchParams
{
public:
	s32			m_switchType;
	s32			m_controlFlags;
	u32			m_numJumpsAscent;
	u32			m_numJumpsDescent;
	float		m_fPanSpeed;
	u32			m_ceilingLodLevel;
	float		m_fLowestHeightAscent;
	float		m_fLowestHeightDescent;
	float		m_fCeilingHeight;
	u32			m_maxTimeToStayInStartupModeOnLongRangeDescent;
	u32			m_scenarioAnimsTimeout;
	float		m_fRadiusOfStreamedScenarioPedCheck;
};

// wrap up any commonality for all player switch managers
class CPlayerSwitchMgrBase
{
public:

	virtual ~CPlayerSwitchMgrBase() {}

	void Start(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& params);
	void Update();
	void Stop(bool bHard);
	bool IsActive() const { return m_bActive; }
	Vec3V_Out GetStartPos() const { return m_vStartPos; }
	Vec3V_Out GetDestPos() const { return m_vDestPos; }
	const CPed* GetNewPed() const { return m_newPed; }
	const CPlayerSwitchParams& GetParams() const { return m_params; }
	void SetControlFlag(s32 controlFlag, bool bEnabled);
	virtual void ModifyLodScale(float& /*fLodScale*/) { }
	float GetOnScreenSpawnDistance() const {return m_onScreenSpawnDistance;}

protected:
	virtual void StartInternal(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& params) = 0;
	virtual void UpdateInternal() = 0;
	virtual void StopInternal(bool bHard) = 0;

	bool m_bActive;
	Vec3V m_vStartPos;
	Vec3V m_vDestPos;
	RegdPed m_oldPed;
	RegdPed m_newPed;

	CPlayerSwitchParams m_params;

	u32 m_startTime;

	float m_onScreenSpawnDistance; // distance we care about peds/vehicles spawning on screen
};

#endif	//_SCENE_PLAYERSWITCH_PLAYERSWITCHMGRBASE_H_
