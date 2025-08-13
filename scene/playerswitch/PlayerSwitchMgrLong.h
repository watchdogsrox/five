/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchMgrLong.h
// PURPOSE : manages all state, camera, streaming behaviour etc for long-range player switch
// AUTHOR :  Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_PLAYERSWITCH_PLAYERSWITCHMGRLONG_H_
#define _SCENE_PLAYERSWITCH_PLAYERSWITCHMGRLONG_H_

#include "atl/hashstring.h"
#include "scene/playerswitch/PlayerSwitchEstablishingShot.h"
#include "scene/playerswitch/PlayerSwitchMgrBase.h"
#include "vectormath/mat34v.h"

// overridden outro scene (e.g. for going to a mocap camera)
class CPlayerSwitchOutroScene
{
public:
	void Init(Mat34V_In worldMatrix, float fFarClip, float fFov)
	{
		m_worldMatrix = worldMatrix;
		m_fCamFarClip = fFarClip;
		m_fCamFov = fFov;
	}

	Mat34V_Out	GetWorldMatrix() const	{ return m_worldMatrix; }
	const float	GetFarClip() const		{ return m_fCamFarClip; }
	const float	GetFov() const			{ return m_fCamFov; }
	Vec3V_Out GetPos() const			{ return m_worldMatrix.d(); }

private:
	Mat34V		m_worldMatrix;
	float		m_fCamFarClip;
	float		m_fCamFov;
};

// state for long range switch includes a state and jump index
class CPlayerSwitchMgrLong_State
{
public:
	CPlayerSwitchMgrLong_State() {}
	CPlayerSwitchMgrLong_State(u32 state, u32 jumpCut) : m_state(state), m_jumpCut(jumpCut) {}

	u32 GetJumpCutIndex() const			{ return m_jumpCut; }
	u32 GetState() const				{ return m_state; }

	void SetJumpCutIndex(u32 index)		{ m_jumpCut = index; }
	void SetState(u32 state)			{ m_state = state; }

	CPlayerSwitchMgrLong_State& operator=(const CPlayerSwitchMgrLong_State& rhs)
	{
		m_jumpCut = rhs.m_jumpCut;
		m_state = rhs.m_state;
		return *this;
	}

private:
	u32		m_jumpCut;
	u32		m_state;
};

// streaming helper for long range switches
class CPlayerSwitchMgrLong_Streamer
{
public:
	void Init(Vec3V_In vDestPos, const CPlayerSwitchParams& params, bool bStartedFromInterior) { m_bStartedFromInterior=bStartedFromInterior; m_vDestPos=vDestPos; m_switchParams=params; m_timeout=STREAMING_TIMEOUT_NORMAL; m_bIsStreaming=false; m_bIsStreamingSecondary=false; }
	void StartStreaming(const CPlayerSwitchMgrLong_State& stateToStreamFor);
	void StartStreamingSecondary(Vec3V_In vPos);
	bool IsFinishedStreaming();
	bool IsStreaming() const { return m_bIsStreaming; }
	bool IsStreamingSecondary() const { return m_bIsStreamingSecondary; }
	bool CanTimeout() const { return m_bCanTimeout; }
	void StopStreaming(bool bAll=false);
	u32 GetTimeout() const { return m_timeout; }

	s32	GetState() const { return m_stateToStreamFor.GetState(); }
	s32 GetCutIndex() const { return m_stateToStreamFor.GetJumpCutIndex(); }
	
private:

	enum
	{
		STREAMING_TIMEOUT_SHORT		= 4000,
		STREAMING_TIMEOUT_NORMAL	= 7000,
		STREAMING_TIMEOUT_LONG		= 9000,
		STREAMING_TIMEOUT_OUTRO		= 10000
	};
	bool m_bIsStreaming;
	bool m_bIsStreamingSecondary;
	bool m_bCanTimeout;
	Vec3V m_vDestPos;
	u32 m_timeout;
	bool m_bStartedFromInterior;
	
	CPlayerSwitchMgrLong_State m_stateToStreamFor;
	CPlayerSwitchParams m_switchParams;
};

// switch mgr for long range switches
class CPlayerSwitchMgrLong : public CPlayerSwitchMgrBase
{
public:

	enum eState
	{
		STATE_INTRO = 0,			// camera is pulling up from the player ped
		STATE_PREP_DESCENT,			// preparing scene for descent-only switch
		STATE_PREP_FOR_CUT,			// flash and start pre-streaming the first jump cut position
		STATE_JUMPCUT_ASCENT,		// flash and pre-stream next state, with some small camera movement
		STATE_WAITFORINPUT_INTRO,	// look up to the sky before waiting for user input in player select screen
		STATE_WAITFORINPUT,			// switching to multiplayer only - hold on top ascent while waiting for player selection
		STATE_WAITFORINPUT_OUTRO,	// look down to the ground before resuming switch
		STATE_PAN,					// pan very quickly across low-lod scene
		STATE_JUMPCUT_DESCENT,		// flash and pre-stream next state, with some small camera movement
		STATE_OUTRO_HOLD,			// hold outro to the next camera
		STATE_OUTRO_SWOOP,			// continue swoop down towards next camera
		STATE_ESTABLISHING_SHOT,	// optional establishing shot
		STATE_FINISHED,				// finished

		STATE_TOTAL
	};

	enum eSwitchEffectType
	{
		SWITCHFX_FRANKLIN_IN = 0,
		SWITCHFX_MICHAEL_IN,
		SWITCHFX_TREVOR_IN,
		SWITCHFX_FRANKLIN_OUT,
		SWITCHFX_MICHAEL_OUT,
		SWITCHFX_TREVOR_OUT,
		SWITCHFX_NEUTRAL_IN,
		SWITCHFX_NEUTRAL,
		SWITCHFX_HUD,
		SWITCHFX_HUD_MICHAEL,
		SWITCHFX_HUD_FRANKLIN,
		SWITCHFX_HUD_TREVOR,
		SWITCHFX_NEUTRAL_OUT,
		SWITCHFX_FRANKLIN_MID,
		SWITCHFX_MICHAEL_MID,
		SWITCHFX_TREVOR_MID,
		SWITCHFX_NEUTRAL_MID,

		// CNC DLC
		SWITCHFX_CNC_COP_FIRST,
		SWITCHFX_CNC_COP_IN = SWITCHFX_CNC_COP_FIRST,
		SWITCHFX_CNC_COP_MID,
		SWITCHFX_CNC_COP_OUT,
		SWITCHFX_CNC_COP_LAST = SWITCHFX_CNC_COP_OUT,

		SWITCHFX_CNC_CROOK_FIRST,
		SWITCHFX_CNC_CROOK_IN = SWITCHFX_CNC_CROOK_FIRST,
		SWITCHFX_CNC_CROOK_MID,
		SWITCHFX_CNC_CROOK_OUT,
		SWITCHFX_CNC_CROOK_LAST = SWITCHFX_CNC_CROOK_OUT,

		SWITCHFX_NEUTRAL_CNC,

		SWITCHFX_COUNT
	};

	virtual ~CPlayerSwitchMgrLong() {}
	u32 GetState() const { return m_currentState.GetState(); }
	u32 GetCurrentJumpCutIndex() const { return m_currentState.GetJumpCutIndex(); }
	const CPlayerSwitchEstablishingShotMetadata* GetEstablishingShotData() const { return m_pEstablishingShotData; }
	void OverrideOutroScene(Mat34V_In worldMatrix, float fCamFov, float fCamFarClip);
	void AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags);
	void SetDest(CPed& newPed, const CPlayerSwitchParams& params);
	void SetEstablishingShot(const atHashString shotName);
	bool ReadyForAscent();
	bool ReadyForDescent();
	bool PerformingDescent() const { return IsActive() && m_currentState.GetState()>STATE_PAN; }
	bool PreppingDescent() const { return IsActive() && m_currentState.GetState()==STATE_PREP_DESCENT; }
	bool IsSkippingDescent() const;
	void EnablePauseBeforeDescent();
	virtual void ModifyLodScale(float& fLodScale);

	bool ShouldPinAssets()
	{
		return ( PerformingDescent() || PreppingDescent() || m_streamer.IsStreamingSecondary() );
	}

protected:
	virtual void StartInternal(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& params);
	virtual void UpdateInternal();
	virtual void StopInternal(bool bHard);

private:
	enum
	{
		JUMPCUT_MIN_DURATION	= 600,
		SHORT_RANGE_DIST		= 300
	};

	void SetState(const CPlayerSwitchMgrLong_State& newState);
	void SetSwitchShadows(bool bEnabled);
	u32 CalcPanDuration();

	CPlayerSwitchMgrLong_State GetNextState(const CPlayerSwitchMgrLong_State& currentState);
	bool ClearToProceed(const CPlayerSwitchMgrLong_State& nextState);

	void SetupSwitchEffectTypes(const CPed& oldPed, const CPed& newPed);
	void StartMainSwitchEffect(eSwitchEffectType effectType, bool bForceLoop, unsigned int delayFrame);
	void StartMainSwitchEffectOutro();
	void StopMainSwitchEffect();
	void UpdateDesSwitchEffectType(const CPed& newPed);

	eSwitchEffectType GetNeutralFX();

	eArcadeTeam GetArcadeTeam();
	eArcadeTeam GetArcadeTeamFromEffectType(eSwitchEffectType effectType);

	u32 m_timeOfLastStateChange;

	bool m_bOutroSceneOverridden;
	CPlayerSwitchOutroScene m_overriddenOutroScene;

	CPlayerSwitchMgrLong_State m_currentState;
	CPlayerSwitchMgrLong_Streamer m_streamer;

	CPlayerSwitchEstablishingShotMetadata* m_pEstablishingShotData;

	eSwitchEffectType m_srcPedSwitchFxType;
	eSwitchEffectType m_dstPedSwitchFxType;
	eSwitchEffectType m_srcPedMidSwitchFxType;
	eSwitchEffectType m_dstPedMidSwitchFxType;

	bool m_bKeepStartSceneMapData;

	static atHashString ms_switchFxName[SWITCHFX_COUNT];
	static eSwitchEffectType ms_currentSwitchFxId;

#if __BANK
	friend class CPlayerSwitchDbg;
#endif	//__BANK
};

#endif	//_SCENE_PLAYERSWITCH_PLAYERSWITCHMGRLONG_H_
