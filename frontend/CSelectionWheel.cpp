#include "CSelectionWheel.h"
#include "audio/northaudioengine.h"
#include "fwsys/timer.h"
#include "frontend/ui_channel.h"
#include "system/controlMgr.h"
#include "fwmaths/angle.h"
#include "Renderer/PostProcessFXHelper.h"
#include "Network/Network.h"
#include "Peds/PlayerInfo.h"

#include "scaleform/tweenstar.h"


FRONTEND_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////////
// Handle your button's tap v. hold state
/////////////////////////////////////////////////////////////////////////////////////
CSelectionWheel::ButtonState CSelectionWheel::HandleButton(const ioValue& relevantControl, const Vector2& stickPos, bool BANK_ONLY(bInvert))
{
	bool bIsDown = relevantControl.IsDown();
	bool bWasUp = relevantControl.WasUp();
	bool bIsUp = relevantControl.IsUp();
	bool bWasDown = relevantControl.WasDown();
#if __BANK
	if( bInvert )
	{
		SwapEm(bIsDown, bIsUp);
		SwapEm(bWasDown, bWasUp);
	}
#endif

	if (bIsDown)
	{
		if( bWasUp || !m_iTimeToShowTimer.IsStarted())
		{
			m_iTimeToShowTimer.Start();
			m_bStickPressed = false;
			return eBS_FirstDown;
		}

		// short-circuit the timer if you press the stick. NOTE: On keyboard/mouse we do not do this (B* 1900122). We are checking
		// relevantControl is the keyboard/mouse rather than the stick used for stickPos as we are only passed pos.
		if( stickPos.IsNonZero() KEYBOARD_MOUSE_ONLY(&& CControl::IsRelativeLookSource(relevantControl.GetSource())) )
			m_bStickPressed = true;

		if( m_bStickPressed || (m_iTimeToShowTimer.IsStarted() && m_iTimeToShowTimer.IsComplete( WHEEL_DELAY_TIME ) ) )
			return eBS_HeldEnough;


		return eBS_NotHeldEnough;
	} 
	else if (bIsUp && bWasDown)
	{
		// quick tap performs differently than a hold
		if( !m_bStickPressed && m_iTimeToShowTimer.IsStarted() && !m_iTimeToShowTimer.IsComplete( BUTTON_HOLD_TIME ) )
		{
			m_iTimeToShowTimer.Zero();
			return eBS_Tapped;
		}
		m_iTimeToShowTimer.Zero();
	}

	return eBS_NotPressed;
}

void CSelectionWheel::GetInputAxis(const ioValue*& LRAxis, const ioValue*& UDAxis) const
{
	CControl& FrontendControl = CControlMgr::GetMainFrontendControl();
	LRAxis = &FrontendControl.GetWeaponWheelLeftRight();
	UDAxis = &FrontendControl.GetWeaponWheelUpDown();
}


//////////////////////////////////////////////////////////////////////////
// Helper function to get and normalize stick input
// Mostly because it comes in as a distinct 1D axis, but we want a 2D vector out of it
//////////////////////////////////////////////////////////////////////////
Vector2 CSelectionWheel::GenerateStickInput() const
{
	const ioValue* pUDAxis = nullptr;
	const ioValue* pLRAxis = nullptr;
	
	GetInputAxis(pLRAxis, pUDAxis);

	Vector2 stickPos(   pLRAxis->GetUnboundNorm(ioValue::NO_DEAD_ZONE),
						pUDAxis->GetUnboundNorm(ioValue::NO_DEAD_ZONE) );

#if KEYBOARD_MOUSE_SUPPORT || RSG_TOUCHPAD
	
	const bool leftRightRequiresDeadZone = ioValue::RequiresDeadZone(pLRAxis->GetSource());
	const bool upDownRequiresDeadZone    = ioValue::RequiresDeadZone(pUDAxis->GetSource());

	if(leftRightRequiresDeadZone == false || upDownRequiresDeadZone == false)
	{
		// NOTE: Do not assume both or none are the mouse, it is possible for one to be the mouse and the other to be noise from the
		// stick (as it is not deadzoned).
		if(leftRightRequiresDeadZone)
		{
			// Ignore heading stick noise.
			stickPos.x = 0.0f;
		}
		else if(upDownRequiresDeadZone)
		{
			// Ignore heading stick noise.
			stickPos.y = 0.0f;
		}
	}
	else
#endif // KEYBOARD_MOUSE_SUPPORT || RSG_TOUCHPAD
	{
		// Since the radial angle can get narrow at small magnitudes, zero the output if you haven't pushed it very much.
		// this'll help prevent 'snap back' from changing your selection (especially on fine wheels like the radio wheel)
		if( stickPos.Mag2() < WHEEL_EDGE_DEADZONE_SQRD )
		{
			stickPos.Zero();
		}
	}

	return stickPos;
}

float WrapFloat(float in, float divisor = TWO_PI, float min = 0.0f)
{
	while( in < min )
		in += divisor;
	while( in > divisor )
		in -= divisor;
	
	return in;
}

/////////////////////////////////////////////////////////////////////////////////////
// Helper function for evaluating a stick's movement.
/////////////////////////////////////////////////////////////////////////////////////
int CSelectionWheel::HandleStick( const Vector2& stickPos, s32 iWheelSegments, int iValue, float fBaseOffset /* = 0.0f */ )
{
	if( stickPos.IsNonZero() && stickPos != m_vPreviousPos )
	{
		m_vPreviousPos = stickPos;

		float fAngle	= WrapFloat(rage::Atan2f(stickPos.y, stickPos.x) + HALF_PI + fBaseOffset);
		float fAnglePer = TWO_PI/float(iWheelSegments);

		float fHysteresisGimme = fAnglePer * WHEEL_HYSTERESIS_PCT;

		// kind of a mathy kludge for wanting to make diagonals more sticky
		/* not really necessary since we fixed the deadzones.
		if( WHEEL_HYSTERESIS_PCT != WHEEL_HYSTERESIS_PCT2 )
		{
			float fSqrtOf1 = 0.70710678118654752440084436210485f;
			// match this to which quadrant the stick is in
			Vector2 vDiagonal( Selectf(stickPos.x, fSqrtOf1, -fSqrtOf1), Selectf(stickPos.y, fSqrtOf1, -fSqrtOf1)  );
			Vector2 vNormed(stickPos);
			vNormed.Normalize();

			// get the dot product (which because we quadrant locked it will between 1 and fSqrtOf1)
			float fOffsetFromDiagonal( vDiagonal.Dot(vNormed) );
			fOffsetFromDiagonal = Clamp((fOffsetFromDiagonal-fSqrtOf1)/(1.0f-fSqrtOf1),0.0f,1.0f);

			// now tween between 'full' and 'no' hysteresis depending on how far onto the diagonal you are.
			float fEasedT = TweenStar::ComputeEasedValue(fOffsetFromDiagonal, rage::TweenStar::EaseType(s_iInterpolateFunc));
			fHysteresisGimme = Lerp(fEasedT, WHEEL_HYSTERESIS_PCT, WHEEL_HYSTERESIS_PCT2);
		}
		*/

		float fOldAngle = iValue*fAnglePer;

		float fMinAngle = WrapFloat(fOldAngle-fHysteresisGimme);
		float fMaxAngle = WrapFloat(fOldAngle+fHysteresisGimme);

		// apply hysteresis
		if( iValue < 0 
			|| fAngle <= fMinAngle
			|| fAngle >= fMaxAngle)
		{
			// we add in the iWheelSegments to ensure it stays positive
			iValue = (rage::round(fAngle/fAnglePer) + iWheelSegments) % iWheelSegments;
		}
	}

	return iValue;
}

void CSelectionWheel::ClearBase(bool bHardSet /* = true */, bool bZeroTimer /*= true */ )
{
	if( bZeroTimer )
		m_iTimeToShowTimer.Zero();

	m_vPreviousPos.Zero();
	m_bStickPressed = false;
	if( bHardSet )
		m_TimeWarper.Reset();
}



void CSelectionWheel::SetTargetTimeWarp(TW_Dir whichWay)
{
	if( whichWay == TW_Slow)
	{
		if( CNetwork::IsGameInProgress() )
		{
			whichWay = TW_Normal;
		}

		if(m_bTriggeredFX.SetState(true))
		{
			TriggerFadeInEffect();
		}
	}
	else // normal
	{
		whichWay = TW_Normal;
		if(m_bTriggeredFX.SetState(false))
		{
			TriggerFadeOutEffect();
		}
	}

	m_TimeWarper.SetTargetTimeWarp(whichWay);
}

void CSelectionWheel::TriggerFadeInEffect()
{
	CPed* pPlayerPed = FindPlayerPed();
	ePedType playerId = PEDTYPE_INVALID;
	const eHUD_COLOURS c_hudColor = CNewHud::GetCurrentCharacterColour();

	if (pPlayerPed)
	{
		playerId = pPlayerPed->GetPedType();
		m_FxPlayerId = (s32)playerId;
	}

	if (playerId == PEDTYPE_PLAYER_0 || c_hudColor == HUD_COLOUR_MICHAEL)
	{
		ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDMichaelIn",0x10493196), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
	}
	else if (playerId == PEDTYPE_PLAYER_1 || c_hudColor == HUD_COLOUR_FRANKLIN)
	{
		ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDFranklinIn",0x07e17b1a), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
	}
	else if (playerId == PEDTYPE_PLAYER_2 || c_hudColor == HUD_COLOUR_TREVOR)
	{
		ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDTrevorIn",0xee33c206), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
	}
	else
	{
		switch (GetArcadeTeam())
		{
		case eArcadeTeam::AT_CNC_COP:
			ANIMPOSTFXMGR.Start(ATSTRINGHASH("CnC_HUDCopIn",0xFA97AB50), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
			break;
		case eArcadeTeam::AT_CNC_CROOK:
			ANIMPOSTFXMGR.Start(ATSTRINGHASH("CnC_HUDCrookIn",0xCBC1E21E), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
			break;
		default:
			ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDIn",0xb2895e1b), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
			break;
		}
	}
}

void CSelectionWheel::TriggerFadeOutEffect()
{
	ePedType playerId = (ePedType)m_FxPlayerId;
	const eHUD_COLOURS c_hudColor = CNewHud::GetCurrentCharacterColour();

	if (playerId == PEDTYPE_PLAYER_0 || c_hudColor == HUD_COLOUR_MICHAEL)
	{
		ANIMPOSTFXMGR.Stop(ATSTRINGHASH("SwitchHUDMichaelIn",0x10493196),AnimPostFXManager::kSelectionWheel);
		if ((ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchOpenMichaelIn",0x848674fd)) == false) &&
			(ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchShortMichaelIn",0x10fb8eb6)) == false))
		{
			ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDMichaelOut",0x2fe80a59), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
		}
	}
	else if (playerId == PEDTYPE_PLAYER_1 || c_hudColor == HUD_COLOUR_FRANKLIN)
	{
		ANIMPOSTFXMGR.Stop(ATSTRINGHASH("SwitchHUDFranklinIn",0x07e17b1a),AnimPostFXManager::kSelectionWheel);
		if ((ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchOpenFranklinIn",0x776fe6f3)) == false) &&
			(ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchShortFranklinIn",0xc2225603)) == false))
		{
			ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDFranklinOut",0x106d489b), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
		}
	}
	else if (playerId == PEDTYPE_PLAYER_2 || c_hudColor == HUD_COLOUR_TREVOR)
	{
		ANIMPOSTFXMGR.Stop(ATSTRINGHASH("SwitchHUDTrevorIn",0xee33c206),AnimPostFXManager::kSelectionWheel);
		if ((ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchOpenTrevorIn",0x1a4e629d)) == false) &&
			(ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchShortTrevorIn",0xd91bdd3)) == false))
		{
			ANIMPOSTFXMGR.Start(ATSTRINGHASH("SwitchHUDTrevorOut",0x08c59987), 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
		}
	}
	else
	{
		atHashString effectName;

		switch (GetArcadeTeam())
		{
		case eArcadeTeam::AT_CNC_COP:
			ANIMPOSTFXMGR.Stop(ATSTRINGHASH("CnC_HUDCopIn",0xFA97AB50),AnimPostFXManager::kSelectionWheel);
			effectName = ATSTRINGHASH("CnC_HUDCopOut",0xF019B64E);
			break;
		case eArcadeTeam::AT_CNC_CROOK:
			ANIMPOSTFXMGR.Stop(ATSTRINGHASH("CnC_HUDCrookIn",0xCBC1E21E),AnimPostFXManager::kSelectionWheel);
			effectName = ATSTRINGHASH("CnC_HUDCrookOut",0xD17076B);
			break;
		default:
			ANIMPOSTFXMGR.Stop(ATSTRINGHASH("SwitchHUDIn",0xb2895e1b),AnimPostFXManager::kSelectionWheel);
			effectName = ATSTRINGHASH("SwitchHUDOut",0x9ac7662a);
			break;
		}

		if ((ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchOpenNeutralIn",0xe5cafc5b)) == false) &&
			(ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("SwitchShortNeutralIn",0x66fcfd3e)) == false))
		{
			ANIMPOSTFXMGR.Start(effectName, 0, false, false, false, 0, AnimPostFXManager::kSelectionWheel);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetArcadeTeam
// PURPOSE:		figure out what team the local player is on in arcade mode
//////////////////////////////////////////////////////////////////////////
eArcadeTeam CSelectionWheel::GetArcadeTeam()
{
	eArcadeTeam arcadeTeam = eArcadeTeam::AT_NONE;
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		CPed *pPlayer = FindPlayerPed();
		if(pPlayer && pPlayer->GetPlayerInfo())
		{
			arcadeTeam = pPlayer->GetPlayerInfo()->GetArcadeInformation().GetTeam();
		}
	}
	return arcadeTeam;
}

#if __BANK
static int s_iInterpolateFunc = rage::TweenStar::EASE_linear;

void CSelectionWheel::CreateBankWidgets(bkGroup* pOwningGroup)
{
	const char* pszaEaseNames[] = {
		"Linear",
		"Quadratic In",
		"Quadratic Out",
		"Quadratic InOut",
		"Cubic In",
		"Cubic Out",
		"Cubic InOut",
		"Quartic In",
		"Quartic Out",
		"Quartic InOut",
		"Sine In",
		"Sine Out",
		"Sine InOut",
		"Back In",
		"Back Out",
		"Back InOut",
		"Circular In",
		"Circular Out",
		"Circular InOut"
	};

	pOwningGroup->AddSlider("Button Hold Time", &BUTTON_HOLD_TIME, 0, 10000, 33);
	pOwningGroup->AddSlider("Wheel Show Time",	&WHEEL_DELAY_TIME, 0, 10000, 33);
	pOwningGroup->AddSlider("Wheel Deadzone (sqrd)",	&WHEEL_EDGE_DEADZONE_SQRD, 0.0f, 1.0f, 0.05f);
	pOwningGroup->AddSlider("Hysteresis",				 &WHEEL_HYSTERESIS_PCT, 0.0, 5.0f, 0.05f);
	pOwningGroup->AddSlider("Hysteresis (at Diagonals)", &WHEEL_HYSTERESIS_PCT2, 0.0, 5.0f, 0.05f);
	pOwningGroup->AddCombo("Interpolate (for Diagonals)",&s_iInterpolateFunc,		COUNTOF(pszaEaseNames), pszaEaseNames);

	pOwningGroup->AddVector("Previous Stick Pos", &m_vPreviousPos,-1.2f, 1.2f, 0.05f);
	pOwningGroup->AddToggle("Stick Pressed", &m_bStickPressed);

	m_TimeWarper.AddWidgets(pOwningGroup);
}
#endif
