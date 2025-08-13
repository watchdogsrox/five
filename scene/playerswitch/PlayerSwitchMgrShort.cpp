/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchMgrShort.cpp
// PURPOSE : manages all state, camera behaviour etc for short-range player switch
// AUTHOR :  Colin Entwistle, Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/playerswitch/PlayerSwitchMgrShort.h"

#include "audio/frontendaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "renderer/PostProcessFXHelper.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

atHashString CPlayerSwitchMgrShort::ms_switchFxName[CPlayerSwitchMgrShort::SHORT_SWITCHFX_COUNT] =
{
	atHashString("SwitchShortFranklinIn",	0xc2225603),	// SHORT_SWITCHFX_FRANKLIN_IN
	atHashString("SwitchShortMichaelIn",	0x10fb8eb6),	// SHORT_SWITCHFX_MICHAEL_IN
	atHashString("SwitchShortTrevorIn",		0xd91bdd3),		// SHORT_SWITCHFX_TREVOR_IN
	atHashString("SwitchShortNeutralIn",	0x66fcfd3e),	// SHORT_SWITCHFX_NEUTRAL_IN
	atHashString("SwitchShortFranklinMid",	0x6ba28347),	// SHORT_SWITCHFX_FRANKLIN_MID
	atHashString("SwitchShortMichaelMid",	0x8a7d2d1b),	// SHORT_SWITCHFX_MICHAEL_MID
	atHashString("SwitchShortTrevorMid",	0x67ae7f68),	// SHORT_SWITCHFX_TREVOR_MID
	atHashString("SwitchShortNeutralMid",	0x19035b64),	// SHORT_SWITCHFX_NEUTRAL_MID
	atHashString("SwitchHUDIn",				0xb2895e1b),	// SHORT_SWITCHFX_HUD
	atHashString("SwitchHUDMichaelIn",		0x10493196),	// SHORT_SWITCHFX_HUD_MICHAEL
	atHashString("SwitchHUDFranklinIn",		0x07e17b1a),	// SHORT_SWITCHFX_HUD_FRANKLIN
	atHashString("SwitchHUDTrevorIn",		0xee33c206)		// SHORT_SWITCHFX_HUD_TREVOR

};

CPlayerSwitchMgrShort::eSwitchEffectType CPlayerSwitchMgrShort::ms_currentSwitchFxId = CPlayerSwitchMgrShort::SHORT_SWITCHFX_NEUTRAL_IN;

CPlayerSwitchMgrShort::CPlayerSwitchMgrShort()
: m_state(STATE_INTRO)
, m_style(SHORT_SWITCH_STYLE_ZOOM_IN_OUT)
, m_srcPedSwitchFxType(SHORT_SWITCHFX_NEUTRAL_IN)
, m_dstPedSwitchFxType(SHORT_SWITCHFX_NEUTRAL_MID)
, m_directionSign(1.0f)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		initiate a short range player switch between the specified peds
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrShort::StartInternal(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& UNUSED_PARAM(params))
{
	m_style = static_cast<eStyle>(camInterface::GetGameplayDirector().ComputeShortSwitchStyle(oldPed, newPed, m_directionSign));

	SetupSwitchEffectTypes(oldPed, newPed);

	// stop any active hud effect here to minimise the delay caused if done by script
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SHORT_SWITCHFX_HUD],AnimPostFXManager::kShortPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SHORT_SWITCHFX_HUD_MICHAEL],AnimPostFXManager::kShortPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SHORT_SWITCHFX_HUD_FRANKLIN],AnimPostFXManager::kShortPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SHORT_SWITCHFX_HUD_TREVOR],AnimPostFXManager::kShortPlayerSwitch);

	// trigger initial switch effect
	StartSwitchIntroEffect();

	SetState( STATE_INTRO );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		shut down the player switch and release any resources etc
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrShort::StopInternal(bool UNUSED_PARAM(bHard))
{
	camInterface::GetGameplayDirector().StopSwitchBehaviour();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		update progress on active player switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrShort::UpdateInternal()
{
	switch (m_state)
	{
	case STATE_INTRO:
		if ( camInterface::GetGameplayDirector().IsSwitchBehaviourFinished() )
		{
			camInterface::GetGameplayDirector().StopSwitchBehaviour();
			SetState( STATE_OUTRO );
		}
		break;

	case STATE_OUTRO:
		if ( camInterface::GetGameplayDirector().IsSwitchBehaviourFinished() )
		{
			Stop(false);
		}
		break;

	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetState
// PURPOSE:		state changes for player switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrShort::SetState(eState newState)
{
	switch (newState)
	{
		//////////////////////////////////////////////////////////////////////////
		// gameplay camera Switch behaviour for old ped
	case STATE_INTRO:
		{
			if ( ( m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_INTRO ) == 0 )
			{
				g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("In", 0x4D5FA44B));
				camInterface::GetGameplayDirector().StartSwitchBehaviour(CPlayerSwitchInterface::SWITCH_TYPE_SHORT, true, m_oldPed, m_newPed, m_style, m_directionSign);
			}
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// gameplay camera Switch behaviour for new ped
	case STATE_OUTRO:
		{
			if( ( m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO ) == 0 )
			{
				g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Out", 0x12067E90));
				camInterface::GetGameplayDirector().StartSwitchBehaviour(CPlayerSwitchInterface::SWITCH_TYPE_SHORT, false, m_oldPed, m_newPed, m_style, m_directionSign);
			}
		}
		break;

	default:
		break;
	}

	m_state = newState;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetupSwitchEffectTypes
// PURPOSE:		figure out what effect should be used for each ped
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrShort::SetupSwitchEffectTypes(const CPed& oldPed, const CPed& newPed)
{
	ePedType srcPedType = oldPed.GetPedType();
	ePedType dstPedType = newPed.GetPedType();

	switch (srcPedType)
	{
	case PEDTYPE_PLAYER_0: //MICHAEL
		{
			m_srcPedSwitchFxType = SHORT_SWITCHFX_MICHAEL_IN;
		}
		break;

	case PEDTYPE_PLAYER_1: //FRANKLIN
		{
			m_srcPedSwitchFxType = SHORT_SWITCHFX_FRANKLIN_IN;
		}
		break;

	case PEDTYPE_PLAYER_2: //TREVOR
		{
			m_srcPedSwitchFxType = SHORT_SWITCHFX_TREVOR_IN;
		}
		break;

	default:
		{
			m_srcPedSwitchFxType = SHORT_SWITCHFX_NEUTRAL_IN;
		}
	}

	switch (dstPedType)
	{
	case PEDTYPE_PLAYER_0: //MICHAEL
		{
			m_dstPedSwitchFxType = SHORT_SWITCHFX_MICHAEL_MID;
		}						   
		break;

	case PEDTYPE_PLAYER_1: //FRANKLIN			   
		{										   
			m_dstPedSwitchFxType = SHORT_SWITCHFX_FRANKLIN_MID;
		}										   
		break;

	case PEDTYPE_PLAYER_2: //TREVOR				   
		{										   
			m_dstPedSwitchFxType = SHORT_SWITCHFX_TREVOR_MID;
		}
		break;

	default:
		{
			m_dstPedSwitchFxType = SHORT_SWITCHFX_NEUTRAL_MID;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartSwitchEffect
// PURPOSE:		triggers a short-range Switch effect
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrShort::StartSwitchEffect(eSwitchEffectType effectType)
{
	ANIMPOSTFXMGR.Stop(ms_switchFxName[ms_currentSwitchFxId],AnimPostFXManager::kShortPlayerSwitch);
	ms_currentSwitchFxId = effectType; 
	ANIMPOSTFXMGR.Start(ms_switchFxName[ms_currentSwitchFxId], 0, false, false, false, 0, AnimPostFXManager::kShortPlayerSwitch);
}
