/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchMgrShort.h
// PURPOSE : manages all state, camera behaviour etc for short-range player switch
// AUTHOR :  Colin Entwistle, Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_PLAYERSWITCH_PLAYERSWITCHMGRSHORT_H_
#define _SCENE_PLAYERSWITCH_PLAYERSWITCHMGRSHORT_H_

#include "scene/playerswitch/PlayerSwitchMgrBase.h"

// for short range switching
class CPlayerSwitchMgrShort : public CPlayerSwitchMgrBase
{
public:
	enum eState
	{
		STATE_INTRO = 0,	// gameplay camera switch behaviour for old ped
		STATE_OUTRO,		// gameplay camera switch behaviour for new ped

		STATE_TOTAL
	};

	enum eStyle
	{
		SHORT_SWITCH_STYLE_ROTATION = 0,
		SHORT_SWITCH_STYLE_TRANSLATION,
		SHORT_SWITCH_STYLE_ZOOM_TO_HEAD,
		SHORT_SWITCH_STYLE_ZOOM_IN_OUT,
		NUM_SHORT_SWITCH_STYLES
	};

	enum eSwitchEffectType
	{
		SHORT_SWITCHFX_FRANKLIN_IN = 0,
		SHORT_SWITCHFX_MICHAEL_IN,
		SHORT_SWITCHFX_TREVOR_IN,
		SHORT_SWITCHFX_NEUTRAL_IN,
		SHORT_SWITCHFX_FRANKLIN_MID,
		SHORT_SWITCHFX_MICHAEL_MID,
		SHORT_SWITCHFX_TREVOR_MID,
		SHORT_SWITCHFX_NEUTRAL_MID,
		SHORT_SWITCHFX_HUD,
		SHORT_SWITCHFX_HUD_MICHAEL,
		SHORT_SWITCHFX_HUD_FRANKLIN,
		SHORT_SWITCHFX_HUD_TREVOR,
		SHORT_SWITCHFX_COUNT
	};

	CPlayerSwitchMgrShort();
	virtual ~CPlayerSwitchMgrShort() {}

	eState			GetState() const { return m_state; }

	void			SetStyle(eStyle style)	{ m_style = style; }
	eStyle			GetStyle() const		{ return (m_style); }

	void			StartSwitchMidEffect() { StartSwitchEffect(m_dstPedSwitchFxType); }
	void			StartSwitchEffect(eSwitchEffectType effectType);

protected:
	virtual void	StartInternal(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& params);
	virtual void	UpdateInternal();
	virtual void	StopInternal(bool bHard);

private:
	void			SetState(eState newState);

 	void			SetupSwitchEffectTypes(const CPed& oldPed, const CPed& newPed);
	void			StartSwitchIntroEffect() { StartSwitchEffect(m_srcPedSwitchFxType); }

	eState			m_state;
	eStyle			m_style;
	eSwitchEffectType m_srcPedSwitchFxType;
	eSwitchEffectType m_dstPedSwitchFxType;
	float			m_directionSign;

	static atHashString ms_switchFxName[SHORT_SWITCHFX_COUNT];
	static eSwitchEffectType ms_currentSwitchFxId;
};

#endif	//_SCENE_PLAYERSWITCH_PLAYERSWITCHMGRSHORT_H_
