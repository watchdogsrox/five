/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchInterface.h
// PURPOSE : interface to player switch system
// AUTHOR :  Ian Kiigan
// CREATED : 02/10/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_PLAYERSWITCH_PLAYERSWITCHINTERFACE_H_
#define _SCENE_PLAYERSWITCH_PLAYERSWITCHINTERFACE_H_

#include "parser/macros.h"
#include "peds/ped.h"
#include "scene/playerswitch/PlayerSwitchMgrLong.h"
#include "scene/playerswitch/PlayerSwitchMgrShort.h"
#include "system/bit.h"

#define PLAYERSWITCH_SETTINGS_FILE ("common:/data/playerswitch")
#define PLAYER_SWITCH_ON_SCREEN_SPAWN_DISTANCE (g_PlayerSwitch.GetOnScreenSpawnDistance())	

class CPlayerSwitchSettings
{
public:
	float			GetRange() const						{ return m_range; }
	float			GetPanSpeed() const						{ return m_panSpeed; }
	float			GetCeilingHeight() const				{ return m_ceilingHeight; }
	float			GetMinHeightAbove() const				{ return m_minHeightAbove; }
	u32				GetCeilingLodLevel() const				{ return m_ceilingLodLevel; }
	float			GetHeightBetweenJumps() const			{ return m_heightBetweenJumps; }
	u32				GetNumJumpsMin() const					{ return m_numJumpsMin; }
	u32				GetNumJumpsMax() const					{ return m_numJumpsMax; }
	float			GetSkipPanRange() const					{ return m_skipPanRange; }
	u32				GetMaxTimeToStayInStartupModeOnDescent() const { return m_maxTimeToStayInStartupModeOnLongRangeDescent; }
	u32				GetScenarioAnimsTimeout() const			{ return m_scenarioAnimsTimeout; }
	float			GetRadiusOfScenarioAnimsCheck() const	{ return m_radiusOfStreamedScenarioPedCheck; }

	void			SetRange(float range)					{ m_range = range; }
	void			SetPanSpeed(float panSpeed)				{ m_panSpeed = panSpeed; }
	void			SetCeilingHeight(float height)			{ m_ceilingHeight = height; }
	void			SetMinHeightAbove(float height)			{ m_minHeightAbove = height; }
	void			SetCeilingLodLevel(u32 lodLevel)		{ m_ceilingLodLevel = lodLevel; }
	void			SetHeightBetweenJumps(float height)		{ m_heightBetweenJumps = height; }
	void			SetNumJumpsMin(u32 numJumps)			{ m_numJumpsMin = numJumps; }
	void			SetNumJumpsMax(u32 numJumps)			{ m_numJumpsMax = numJumps; }
	void			SetSkipPanRange(float range)			{ m_skipPanRange = range; }
	void			SetMaxTimeToStayInStartupModeOnLongRangeDescent(const u32 iTime) { m_maxTimeToStayInStartupModeOnLongRangeDescent = iTime; }
	void			SetScenarioAnimsTimeout(const u32 iTime) { m_scenarioAnimsTimeout = iTime; }
	void			SetRadiusOfScenarioAnimsCheck(const float r) { Assert(r >= 0.0f); m_radiusOfStreamedScenarioPedCheck = r; }

	void			SetName(const char* pszName)			{ m_name.SetFromString(pszName); }

private:
	atHashString	m_name;
	float			m_range;
	u32				m_numJumpsMin;
	u32				m_numJumpsMax;
	float			m_panSpeed;
	float			m_ceilingHeight;
	float			m_minHeightAbove;
	u32				m_ceilingLodLevel;
	float			m_heightBetweenJumps;
	float			m_skipPanRange;
	u32				m_maxTimeToStayInStartupModeOnLongRangeDescent;
	u32				m_scenarioAnimsTimeout;
	float			m_radiusOfStreamedScenarioPedCheck;

	PAR_SIMPLE_PARSABLE;
};

class CPlayerSwitchInterface
{
public:
	enum
	{
		SWITCH_TYPE_AUTO = 0,
		SWITCH_TYPE_LONG,
		SWITCH_TYPE_MEDIUM,
		SWITCH_TYPE_SHORT,

		SWITCH_TYPE_TOTAL
	};

	enum
	{
		CONTROL_FLAG_SKIP_INTRO				= BIT(0),
		CONTROL_FLAG_SKIP_OUTRO				= BIT(1),
		CONTROL_FLAG_PAUSE_BEFORE_PAN		= BIT(2),
		CONTROL_FLAG_PAUSE_BEFORE_OUTRO		= BIT(3),
		CONTROL_FLAG_SKIP_PAN				= BIT(4),
		CONTROL_FLAG_UNKNOWN_DEST			= BIT(5),
		CONTROL_FLAG_DESCENT_ONLY			= BIT(6),
		CONTROL_FLAG_START_FROM_CAMPOS		= BIT(7),
		CONTROL_FLAG_PAUSE_BEFORE_ASCENT	= BIT(8),
		CONTROL_FLAG_PAUSE_BEFORE_DESCENT	= BIT(9),
		CONTROL_FLAG_ALLOW_SNIPER_AIM_INTRO	= BIT(10),
		CONTROL_FLAG_ALLOW_SNIPER_AIM_OUTRO	= BIT(11),
		CONTROL_FLAG_SKIP_TOP_DESCENT		= BIT(12),
		CONTROL_FLAG_SUPPRESS_OUTRO_FX		= BIT(13),
		CONTROL_FLAG_SUPPRESS_INTRO_FX		= BIT(14),
		CONTROL_FLAG_DELAY_ASCENT_FX		= BIT(15),

		CONTROL_MASK_PANDISABLED			= ( CONTROL_FLAG_PAUSE_BEFORE_PAN | CONTROL_FLAG_SKIP_PAN )
	};

	CPlayerSwitchInterface();

	void Start(s32 switchType, CPed& oldPed, CPed& newPed, s32 controlFlags);
	void Update();
	void Stop();
	bool IsActive() const;

	bool ShouldDisableRadio() const;
	bool ShouldUseFullCircleForPopStreaming() const;

	s32 GetIdealSwitchType(Vec3V_In vStart, Vec3V_In vEnd) const;
	s32 GetSwitchType() const { return m_switchType; }
	void ModifyLodScale(float& fLodScale);
	float GetOnScreenSpawnDistance() const;

	void SetDest(CPed& newPed);

	CPlayerSwitchMgrBase* GetMgr(s32 switchType)
	{
		if (switchType==SWITCH_TYPE_SHORT)
		{
			return &m_switchMgrShort;
		}
		return &m_switchMgrLong;
	}

	const CPlayerSwitchMgrBase* GetMgrConst(s32 switchType) const
	{
		if (switchType==SWITCH_TYPE_SHORT)
		{
			return &m_switchMgrShort;
		}
		return &m_switchMgrLong;
	}

	CPlayerSwitchMgrLong& GetLongRangeMgr() { return m_switchMgrLong; }
	CPlayerSwitchMgrShort& GetShortRangeMgr() { return m_switchMgrShort; }

	void SaveSettings();
	void LoadSettings();

private:
	s32 AutoSelectSwitchType(CPed& oldPed, CPed& newPed, s32 controlFlags);
	void SetupParams(CPlayerSwitchParams& params, s32 switchType, CPed& oldPed, CPed& newPed, s32 controlFlags);

	s32	m_switchType;
	CPlayerSwitchMgrLong m_switchMgrLong;
	CPlayerSwitchMgrShort m_switchMgrShort;

	CPlayerSwitchSettings m_switchSettings[SWITCH_TYPE_TOTAL];

#if __BANK
public:
	CPlayerSwitchSettings& GetSettings(s32 switchType) { return m_switchSettings[switchType]; }
#endif	//__BANK

	PAR_SIMPLE_PARSABLE;
};

extern CPlayerSwitchInterface g_PlayerSwitch;

class CPlayerSwitch
{
public:
	static void Init() { }
	static void Shutdown(u32 /*shutdownMode*/) { g_PlayerSwitch.Stop(); }
};

#endif	//_SCENE_PLAYERSWITCH_PLAYERSWITCHINTERFACE_H_
