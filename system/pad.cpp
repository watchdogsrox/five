//
// name:		pad.cpp
// description:	Code to read the game pad

#include "system/pad.h"

// Rage headers
#include "input\input.h"
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"

// Game headers
#include "Frontend/Credits.h"
#include "Frontend/PauseMenu.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "vehicles/vehicle.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "scene/world/GameWorld.h"
#include "camera/CamInterface.h"

#if RSG_ORBIS
#define RSG_DEADZONE	0.13f		// ~32/255
#else
#define RSG_DEADZONE	0.20f
#endif

//
// name:		CPad::CPad
// description:	constructor
CPad::CPad()
{
	Clear();
	m_pPad = NULL;

#if !USE_ACTUATOR_EFFECTS
	m_bIsScriptShake = false;
#endif // !USE_ACTUATOR_EFFECTS
	m_bIsShaking = false;
	m_bShakeStoped = false;

#if __PPU
	m_WasConnected = false;
#endif
	m_suppressedId = -1;
}

//
// name:		Update
// description:	Update controller state
void CPad::Update()
{
	m_oldState = m_state;
	if(m_pPad)
	{
		m_state.Update(m_pPad);

#if __PPU
		// If pad hasn't been connected yet. Check for when it will be
		if(m_WasConnected == false)
			m_WasConnected = m_pPad->IsConnected();
#endif

#if USE_SIXAXIS_GESTURES
		// Update gestures from motion sensors
		if(m_pPad->HasSensors())
		{
			m_PadGesture.Update(this);
		}
#endif // USE_SIXAXIS_GESTURES

#if RSG_ORBIS
		m_TouchPadGesture.Update(this);
#endif // RSG_ORBIS

		UpdateActuatorsAndShakes();
	}

	m_bShakeStoped = false;
}

// name:		UpdateActuatorsAndShakes
// description:	Updates either the shakes, or actuators depending on the USE_ACTUATOR_EFFECTS define.
//				Additionally, stops shaking if the pad is intercepted.
void CPad::UpdateActuatorsAndShakes()
{
	if(!IsIntercepted())
	{
#if USE_ACTUATOR_EFFECTS
		UpdateActuatorEffects();
#else
		UpdateShakes();
#endif // USE_ACTUATOR_EFFECTS
	}
	else
	{
		// Stop all shakes
		StopShaking(true);
	}
}

//
// name:		Update
// description:	clear controller state
void CPad::Clear()
{
	m_state.Clear();
	m_oldState.Clear();
}

s32 CPad::GetPressedButtons()
{
	return (m_pPad ? m_pPad->GetPressedButtons() : 0);
}

s32 CPad::GetDebugButtons()
{
#if __DEBUGBUTTONS
	return (m_pPad ? m_pPad->GetDebugButtons() : 0);
#else
	return 0;
#endif // __DEBUGBUTTONS
}

s32 CPad::GetPressedDebugButtons()
{
#if __DEBUGBUTTONS
	return (m_pPad ? m_pPad->GetPressedDebugButtons() : 0);
#else
	return 0;
#endif // __DEBUGBUTTONS
}

bool CPad::HasInput()
{
	if(m_pPad->GetPressedButtons() != 0 ||
		m_state.m_leftX != 0 ||
		m_state.m_rightX != 0 ||
		m_state.m_leftY != 0 ||
		m_state.m_rightY != 0)
	{
		return true;
	}
	return false;
}

//
// name:		Update
// description:	Update controller state
void CPad::CPadState::Update(ioPad* pPad)
{
	m_start = (pPad->GetButtons() & ioPad::START) ? 255 : 0;
	m_select = (pPad->GetButtons() & ioPad::SELECT) ? 255 : 0;
	m_l3 = (pPad->GetButtons() & ioPad::L3) ? 255 : 0;
	m_r3 = (pPad->GetButtons() & ioPad::R3) ? 255 : 0;
	m_touchClick = (pPad->GetButtons() & ioPad::TOUCH) ? 255 : 0;

	m_leftX = (s32)rage::Floorf(ioAddDeadZone(pPad->GetNormLeftX(), RSG_DEADZONE) * 127.5f);
	m_leftY = (s32)rage::Floorf(ioAddDeadZone(pPad->GetNormLeftY(), RSG_DEADZONE) * 127.5f);
	m_rightX = (s32)rage::Floorf(ioAddDeadZone(pPad->GetNormRightX(), RSG_DEADZONE) * 127.5f);
	m_rightY = (s32)rage::Floorf(ioAddDeadZone(pPad->GetNormRightY(), RSG_DEADZONE) * 127.5f);

	if(pPad->AreButtonsAnalog())
	{
		m_square = pPad->GetAnalogButton(ioPad::RLEFT_INDEX);
		m_triangle = pPad->GetAnalogButton(ioPad::RUP_INDEX);
		m_circle = pPad->GetAnalogButton(ioPad::RRIGHT_INDEX);
		m_cross = pPad->GetAnalogButton(ioPad::RDOWN_INDEX);
		m_dpadUp = pPad->GetAnalogButton(ioPad::LUP_INDEX);
		m_dpadDown = pPad->GetAnalogButton(ioPad::LDOWN_INDEX);
		m_dpadLeft = pPad->GetAnalogButton(ioPad::LLEFT_INDEX);
		m_dpadRight = pPad->GetAnalogButton(ioPad::LRIGHT_INDEX);
		m_l1 = pPad->GetAnalogButton(ioPad::L1_INDEX);
		m_r1 = pPad->GetAnalogButton(ioPad::R1_INDEX);
	}
	else
	{
		m_square = (pPad->GetButtons() & ioPad::RLEFT) ? 255 : 0;
		m_triangle = (pPad->GetButtons() & ioPad::RUP) ? 255 : 0;
		m_circle = (pPad->GetButtons() & ioPad::RRIGHT) ? 255 : 0;
		m_cross = (pPad->GetButtons() & ioPad::RDOWN) ? 255 : 0;
		m_dpadUp = (pPad->GetButtons() & ioPad::LUP) ? 255 : 0;
		m_dpadDown = (pPad->GetButtons() & ioPad::LDOWN) ? 255 : 0;
		m_dpadLeft = (pPad->GetButtons() & ioPad::LLEFT) ? 255 : 0;
		m_dpadRight = (pPad->GetButtons() & ioPad::LRIGHT) ? 255 : 0;
		m_l1 = (pPad->GetButtons() & ioPad::L1) ? 255 : 0;
		m_r1 = (pPad->GetButtons() & ioPad::R1) ? 255 : 0;
	}

	if (pPad->AreTriggersAnalog())
	{
		m_l2 = pPad->GetAnalogButton(ioPad::L2_INDEX);
		m_r2 = pPad->GetAnalogButton(ioPad::R2_INDEX);
	}
	else
	{
		m_l2 = (pPad->GetButtons() & ioPad::L2) ? 255 : 0;
		m_r2 = (pPad->GetButtons() & ioPad::R2) ? 255 : 0;
	}
	

#if __PPU
	if(pPad->HasSensors())
	{
		m_sensorX = pPad->GetSensorAxis(ioPad::SENSOR_X);
		m_sensorY = pPad->GetSensorAxis(ioPad::SENSOR_Y);
		m_sensorZ = pPad->GetSensorAxis(ioPad::SENSOR_Z);
		m_sensorG = pPad->GetSensorAxis(ioPad::SENSOR_G);
	}
	else
	{
		m_sensorX = MOTION_SENSOR_ZERO_COUNT;
		m_sensorY = MOTION_SENSOR_ZERO_COUNT;
		m_sensorZ = MOTION_SENSOR_ZERO_COUNT;
		m_sensorG = MOTION_SENSOR_ZERO_COUNT;
	}
#endif

#if RSG_ORBIS
	m_NumTouches = pPad->GetNumTouches();

	rage::ioPad::TouchPadData oPadTouch = pPad->GetTouchData(0);
	m_TouchIdOne = oPadTouch.id;
	m_TouchOne.x = oPadTouch.x;
	m_TouchOne.y = oPadTouch.y;

	oPadTouch = pPad->GetTouchData(1);
	m_TouchIdTwo = oPadTouch.id;
	m_TouchTwo.x = oPadTouch.x;
	m_TouchTwo.y = oPadTouch.y;
#endif // RSG_ORBIS
}

//
// name:		Clear
// description:	Clear controller state
void CPad::CPadState::Clear()
{
	m_leftX = 0;
	m_leftY = 0;
	m_rightX = 0;
	m_rightY = 0;
	m_l1 = 0;
	m_l2 = 0;
	m_l3 = 0;
	m_r1 = 0;
	m_r2 = 0;
	m_r3 = 0;
	m_square = 0;
	m_triangle = 0;
	m_circle = 0;
	m_cross = 0;
	m_dpadUp = 0;
	m_dpadDown = 0;
	m_dpadLeft = 0;
	m_dpadRight = 0;
	m_start = 0;
	m_select = 0;
#if __PPU
	m_sensorX = 0;
	m_sensorY = 0;
	m_sensorZ = 0;
	m_sensorG = 0;
#endif

#if RSG_ORBIS
	m_NumTouches = 0;
	m_TouchIdOne = INVALID_TOUCH_ID;
	m_TouchOne.x = 0;
	m_TouchOne.y = 0;
	m_TouchIdTwo = INVALID_TOUCH_ID;
	m_TouchTwo.x = 0;
	m_TouchTwo.y = 0;
#endif // RSG_ORBIS
}

void CPad::SetPlayerNum(s32 num) 
{
	m_pPad = &ioPad::GetPad(num);

#if USE_ACTUATOR_EFFECTS
	m_Actuator = ioPadActuatorDevice(num);
#endif // USE_ACTUATOR_EFFECTS
}


#if USE_ACTUATOR_EFFECTS

void CPad::UpdateActuatorEffects()
{
	// Fade out any rumble when the player dies.
	CPed* pPlayer = CGameWorld::FindLocalPlayerSafe();
	CPlayerInfo* pPlayerInfo = pPlayer ? pPlayer->GetPlayerInfo() : NULL;
	if(	pPlayerInfo && pPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED )
	{
		m_DeathRumbleScaler *= MAX_VIBRATION_DROP_RATIO;
	}
	else
	{
		m_DeathRumbleScaler = 1.0f;
	}

	const u32 timeNow = fwTimer::GetSystemTimeInMilliseconds();

	bool hasScriptActuatorEffect = false;

	for(s32 i = 0; i < m_ScriptActuatorEffects.size(); ++i)
	{
		if(m_ScriptActuatorEffects[i].m_pEffect != NULL)
		{
			float progress = static_cast<float>(timeNow - m_ScriptActuatorEffects[i].m_StartTime) /
								static_cast<float>(m_ScriptActuatorEffects[i].m_pEffect->GetDuration());

			progress = Clamp(progress, 0.0f, 1.0f);
			m_ScriptActuatorEffects[i].m_pEffect->Update(progress, &m_Actuator);

			// If there is a script effect then override all other effects as script effects take priority.
			hasScriptActuatorEffect = true;
		}
	}
	
	if(hasScriptActuatorEffect == false)
	{
		// Process all other effects.
		for (s32 i = 0; i < m_ActuatorEffects.size(); ++i)
		{
			if(m_ActuatorEffects[i].m_pEffect != NULL)
			{
				float progress = static_cast<float>(timeNow - m_ActuatorEffects[i].m_StartTime) /
									static_cast<float>(m_ActuatorEffects[i].m_pEffect->GetDuration());

				progress = Clamp(progress, 0.0f, 1.0f);
				m_ActuatorEffects[i].m_pEffect->Update(progress, &m_Actuator);
			}
		}
	}

	if(hasScriptActuatorEffect || m_ActuatorEffects.size() > 0)
	{
		m_bIsShaking = true;
	}
	else
	{
		m_bIsShaking = false;
	}

	m_Actuator.ScaleActuators(m_DeathRumbleScaler);
	m_Actuator.Update();
	m_pPad->SetActuatorsActivation(m_bIsShaking);

	RemoveExpiredActuatorEffects();
}

void CPad::RemoveExpiredActuatorEffects()
{
	u32 currTime = GetTimeInMilliseconds();

	for(int i = m_ScriptActuatorEffects.size() -1; i>= 0; --i)
	{
		// If there is a finished script effect then remove it.
		if(m_ScriptActuatorEffects[i].m_pEffect == NULL || (currTime - m_ScriptActuatorEffects[i].m_StartTime) >= m_ScriptActuatorEffects[i].m_pEffect->GetDuration())
		{
			m_ScriptActuatorEffects.DeleteFast(i);
		}
	}

	// Remove all other expired effects.
	for (int i = m_ActuatorEffects.size() -1; i >= 0; --i)
	{
		if(m_ActuatorEffects[i].m_pEffect == NULL)
		{
			m_ActuatorEffects.DeleteFast(i);
		}
		else
		{
			// NOTE: Originally I used if((currTime - m_ActuatorEffects[i].m_StartTime) >= m_ActuatorEffects[i].m_pEffect->GetDuration())
			// but scaleform is passing in -50 as the duration (that gets cast to a u32) so the rumble lasts forever. This is a temporary
			// workaround until scaleform passes in sensible values!
			u32 endTime = m_ActuatorEffects[i].m_StartTime + m_ActuatorEffects[i].m_pEffect->GetDuration();
			if(currTime >= endTime)
			{
				m_ActuatorEffects.DeleteFast(i);
			}
		}
	}
}

void CPad::ApplyActuatorEffect( ioActuatorEffect* effect, bool isScriptCommand, s32 suppressedId, u32 delayAfterThisOne )
{
	if (effect == NULL || !CPauseMenu::GetMenuPreference(PREF_VIBRATION))
		return;
	else if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() && !CutSceneManager::GetInstance()->CanVibratePadDuringCutScene()) || CCredits::IsRunning())
		return;
	else if (m_bShakeStoped)
		return;
	else if (suppressedId != m_suppressedId)
	 	return;
	else if (GetTimeInMilliseconds() < m_noShakeBeforeThis)
		return;

	m_noShakeBeforeThis = GetTimeInMilliseconds() + delayAfterThisOne;

	Assertf(effect->GetDuration() <= 10000, "A rumble for laster longer than 10 seconds was created! Check callstack for the culprit! Duration: %d", effect->GetDuration());

	// There can only be one script effect.
	if(isScriptCommand)
	{
		if(m_ScriptActuatorEffects.size() < m_ScriptActuatorEffects.max_size())
		{
			ActuatorEntry& entry = m_ScriptActuatorEffects.Append();
			entry.m_pEffect = effect;
			entry.m_StartTime = fwTimer::GetSystemTimeInMilliseconds();
		}
		else
		{
			Warningf("Not enough actuator slots for script rumble increase CPad::MAX_SCRIPT_SHAKE_ENTRIES");
		}
	}
	else if(m_ActuatorEffects.size() < m_ActuatorEffects.max_size())
	{
		ActuatorEntry& entry = m_ActuatorEffects.Append();
		entry.m_pEffect = effect;
		entry.m_StartTime = fwTimer::GetSystemTimeInMilliseconds();
	}
	else
	{
		Warningf("Not enough actuator slots increase CPad::MAX_SHAKE_ENTRIES");
	}
}

#else


////////////////////////////////////////////
//
// RUMBLE START
//

int	CPad::FindSmallestShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries)
{
	// Find the smallest rumble
	int	smallestIDX = -1;
	s32	smallestFreq = 100000;	// A big number
	for(int i=0;i<rumbleEntries.size();i++)
	{
		RumbleEntry	thisEntry;
		RumbleEntry &theEntry = rumbleEntries[i];
		if(theEntry.m_IsScript == 0 && theEntry.m_Freq < smallestFreq )
		{
			smallestFreq = theEntry.m_Freq;
			smallestIDX = i;
		}
	}
	return smallestIDX;
}

int	CPad::FindLargestShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries)
{
	int	highestIDX = -1;
	s32	highestFreq = 0;
	for(int i=0;i<rumbleEntries.size();i++)
	{
		RumbleEntry	thisEntry;
		RumbleEntry &theEntry = rumbleEntries[i];

		// If it's script, use that
		if( theEntry.m_IsScript == 1 )
		{
			return i;
		}

		if( theEntry.m_Freq >= highestFreq )
		{
			highestFreq = theEntry.m_Freq;
			highestIDX = i;
		}
	}
	return highestIDX;
}

CPad::RumbleEntry *CPad::FindScriptEntry(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries)
{
	for(int i=0;i<rumbleEntries.size();i++)
	{
		if(rumbleEntries[i].m_IsScript == 1)
		{
			return &rumbleEntries[i];
		}
	}
	return NULL;
}


void CPad::PushNewShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries, s32 Frequency, u32 Duration)
{
	RumbleEntry	thisEntry;
	thisEntry.m_iStartTime = GetTimeInMilliseconds();
	thisEntry.m_iDuration = Duration;
	thisEntry.m_Freq = Frequency;
	thisEntry.m_IsScript = m_bIsScriptShake?1:0;
	rumbleEntries.Push(thisEntry);
}


// TODO: Implement the script rumble entry stuff, only one for script.
void CPad::RegisterShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries, s32 Frequency, u32 Duration)
{
	u32	startTime = GetTimeInMilliseconds();

	if( m_bIsScriptShake )
	{
		RumbleEntry *pEntry = FindScriptEntry(rumbleEntries);

		// If we have an entry, overwrite, otherwise just add as normal
		if( pEntry )
		{
			pEntry->m_iStartTime = startTime;
			pEntry->m_iDuration = Duration;
			pEntry->m_Freq = Frequency;
			return;	
		}
	}

	if( !rumbleEntries.IsFull() )
	{
		PushNewShake(rumbleEntries, Frequency, Duration);
	}
	else
	{
		int smallestIDX = FindSmallestShake(rumbleEntries);
		if( smallestIDX != -1 )
		{
			RumbleEntry &theEntry = rumbleEntries[smallestIDX];
			if( theEntry.m_Freq <= Frequency )
			{
				rumbleEntries.DeleteFast(smallestIDX);
				PushNewShake(rumbleEntries, Frequency, Duration);
			}
		}
	}
}


// Remove least important
void CPad::RemoveLeastImportantShakes()
{

}

// Remove expired
void CPad::RemoveExpiredShakes()
{
	u32 currTime = GetTimeInMilliseconds();

	for(int idx = 0; idx < ioPad::MAX_ACTUATORS; idx++)
	{
		atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries = m_RumbleEntries[idx];

		// Go backwards
		for(int i=rumbleEntries.size()-1;i>=0;i--)
		{
			RumbleEntry &theEntry = rumbleEntries[i];
			u32	endTime = theEntry.m_iStartTime + theEntry.m_iDuration;
			if(currTime >= endTime)
			{
				rumbleEntries.DeleteFast(i);
			}
		}
	}

}

// Find the highest frueqency shake that's still in the list for this actuator
CPad::RumbleEntry *CPad::FindCurrentShake(int actID)
{
	atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries = m_RumbleEntries[actID];

	int highestIDX = FindLargestShake(rumbleEntries);
	if( highestIDX != -1 )
	{
		return &rumbleEntries[highestIDX];
	}
	return NULL;
}

// Process and set shakes
void CPad::UpdateShakes()
{

	// Fade out any rumble when the player dies.
	CPed* pPlayer = CGameWorld::FindLocalPlayerSafe();
	CPlayerInfo* pPlayerInfo = pPlayer ? pPlayer->GetPlayerInfo() : NULL;
	if(	pPlayerInfo && pPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED )
	{
		m_DeathRumbleScaler *= MAX_VIBRATION_DROP_RATIO;
	}
	else
	{
		m_DeathRumbleScaler = 1.0f;
	}

	for(u32 i = 0; i < ioPad::MAX_ACTUATORS; ++i)
	{
		RumbleEntry *entry = FindCurrentShake(i);
		if( entry )
		{
			m_pPad->SetActuatorValue(i, (entry->m_Freq * m_DeathRumbleScaler)/MAX_VIBRATION_FREQUENCY);
			m_bIsShaking = true;
		}
		else
		{
			m_pPad->SetActuatorValue(i, 0);
		}
	}

	m_pPad->SetActuatorsActivation(m_bIsShaking);

	// Remove any expired entries
	RemoveExpiredShakes();
}

void CPad::StartShake(u32 Duration0, s32 MotorFrequency0, u32 Duration1, s32 MotorFrequency1, s32 DelayAfterThisOne, s32 suppressedId)
{
	if (!CPauseMenu::GetMenuPreference(PREF_VIBRATION))
		return;
	else if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() && !CutSceneManager::GetInstance()->CanVibratePadDuringCutScene()) || CCredits::IsRunning())
		return;
	else if (m_bShakeStoped)
		return;
	else if (suppressedId != m_suppressedId)
		return;

	// Only this func appears to obey this stuff?
	s32 iMaxFrequency = (MotorFrequency0>MotorFrequency1) ? MotorFrequency0 : MotorFrequency1;
	if (GetTimeInMilliseconds() < m_noShakeBeforeThis && (iMaxFrequency <= m_noShakeFreq) )
	{
		return;		// Don't do this one to avoid constant rumbling
	}

	// To stop continuous rumbling
	m_noShakeBeforeThis = GetTimeInMilliseconds() + DelayAfterThisOne;
	m_noShakeFreq = iMaxFrequency;

	RegisterShake(m_RumbleEntries[ioPad::HEAVY_MOTOR], MotorFrequency0, Duration0 );
	RegisterShake(m_RumbleEntries[ioPad::LIGHT_MOTOR], MotorFrequency1, Duration1 );

}

// Name			:	StartShake_Distance
// Purpose		:	Makes the pad shake for a wee while
// Parameters	:
// Returns		:	Nothing

void CPad::StartShake_Distance(u32 Duration, s32 Frequency, float X, float Y, float Z)
{
	if (!CPauseMenu::GetMenuPreference(PREF_VIBRATION))
		return;
	else if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())|| CCredits::IsRunning())
		return;
	else if (m_bShakeStoped)
		return;

	Vector3 pos(X,Y,Z);
	float	Distance2 = (camInterface::GetPos() - pos).Mag2();

	if (Distance2 < (70.0f * 70.0f) )	// For now simple on/off. Might get more sophisticated
	{
		RegisterShake(m_RumbleEntries[ioPad::HEAVY_MOTOR], Frequency, Duration );
	}

}

#if HAS_TRIGGER_RUMBLE
void CPad::StartTriggerShake( u32 LeftDuration, s32 LeftMotorFrequency, u32 RightDuration, s32 RightMotorFrequency, s32 suppressedId )
{
	if (!CPauseMenu::GetMenuPreference(PREF_VIBRATION))
		return;
	else if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() && !CutSceneManager::GetInstance()->CanVibratePadDuringCutScene()) || CCredits::IsRunning())
		return;
	else if (m_bShakeStoped)
		return;
	else if (suppressedId != m_suppressedId)
		return;

	if (GetTimeInMilliseconds() < m_noShakeBeforeThis )
	{
		return;		// Don't do this one to avoid constant rumbling
	}

	RegisterShake(m_RumbleEntries[ioPad::LEFT_TRIGGER], LeftMotorFrequency, LeftDuration );
	RegisterShake(m_RumbleEntries[ioPad::RIGHT_TRIGGER], RightMotorFrequency, RightDuration );
}
#endif // HAS_TRIGGER_RUMBLE

// Name			:	StartShake_Train
// Purpose		:	Makes the pad shake for a wee while
// Parameters	:
// Returns		:	Nothing
#define TRAINSHAKEDIST (70.0f)

void CPad::StartShake_Train(float X, float Y)
{
	if (!CPauseMenu::GetMenuPreference(PREF_VIBRATION))
		return;
	else if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())|| CCredits::IsRunning())
		return;
	else if (m_bShakeStoped)
		return;

	float	DistanceSqr, Distance;
	s32	Frequency;

	// If the player is ON a train we don't shake.
	if (FindPlayerVehicle() && FindPlayerVehicle()->InheritsFromTrain()) return;

	DistanceSqr = (camInterface::GetPos().x - X) * (camInterface::GetPos().x - X) +
				  (camInterface::GetPos().y - Y) * (camInterface::GetPos().y - Y);

	if (DistanceSqr < TRAINSHAKEDIST*TRAINSHAKEDIST)	// For now simple on/off. Might get more sophisticated
	{
		Distance = rage::Sqrtf(DistanceSqr);
	
		Frequency = 30 + (s32)(70 * (TRAINSHAKEDIST - Distance) / TRAINSHAKEDIST);

		RegisterShake(m_RumbleEntries[ioPad::HEAVY_MOTOR], Frequency, 100 );
	}
}

#endif // USE_ACTUATOR_EFFECTS

// Name			:	StopShaking
// Purpose		:	Tell this pad not to shake anymore
// Parameters	:	bForce - Will force a immediately stop by updating all pads and setting the flag to protect the shake
//                           to be set again until the next update happens.
// Returns		:	Nothing

void CPad::StopShaking(bool bForce )
{
#if USE_ACTUATOR_EFFECTS
	m_ScriptActuatorEffects.Reset();
	m_ActuatorEffects.Reset();
#else
	for(u32 i = 0; i < ioPad::MAX_ACTUATORS; ++i)
	{
		m_RumbleEntries[i].Reset();
	}
#endif // USE_ACTUATOR_EFFECTS

	if (bForce && !m_bShakeStoped && m_pPad)
	{
		m_pPad->StopShakingNow();
		m_bShakeStoped = bForce;
	}
}

// Name			:	StopShaking
// Purpose		:	Tell if this pad is shaking.
// Parameters	:	None
// Returns		:	True if it is shaking

bool CPad::IsShaking( )
{
	return m_bIsShaking;
}

//
// RUMBLE END
//
////////////////////////////////////////////



bool CPad::IsConnected( PPU_ONLY(bool bIgnoreWasConnected) )
{
#if __PPU
	// Only check for controller being connected if it has been already
	if(!m_WasConnected && !bIgnoreWasConnected)
	{
		return true;
	}
	else
	{
		return m_pPad->IsConnected();		
	}
#else
	return m_pPad->IsConnected();
#endif
}

bool CPad::HasSensors() const 
{ 
	return m_pPad->HasSensors();
}

bool CPad::IsXBox360Pad() const
{
	if(m_pPad)
	{
#if __XENON
		return true;
#else
		const ioJoystick &S = ioJoystick::GetStick(m_pPad->GetPadIndex());
		if(S.GetJoyType()==ioJoystick::TYPE_XNA_GAMEPAD_XENON)
		{
			return true;
		}
#endif
	}
	return false;
}

bool CPad::IsXBox360CompatiblePad() const
{
	if(IsXBox360Pad())
		return true;

#if USE_XINPUT
	if(m_pPad)
	{
		return m_pPad->IsConnected();
	}
#endif

	return false;
}

bool CPad::IsPs3Pad() const
{
#if __PPU
	return true;
#else
	return false;
#endif
}

bool CPad::IsPs4Pad() const
{
#if RSG_ORBIS
	return true;
#else
	return false;
#endif
}

#if RSG_ORBIS
void CPad::GetTouchPoint(int index, u16& x, u16& y)
{
	if (index == 0)
	{
		x = m_state.m_TouchOne.x;
		y = m_state.m_TouchOne.y;
	}
	else
	{
		x = m_state.m_TouchTwo.x;
		y = m_state.m_TouchTwo.y;
	}
}
#endif // RSG_ORBIS

u32 CPad::GetTimeInMilliseconds()
{
	return fwTimer::GetSystemTimeInMilliseconds();
}

//
// name:		PrintDebug
// description:	Print state of buttons
void CPad::PrintDebug()
{
#if DEBUG_DRAW
	grcDebugDraw::AddDebugOutput("LeftX %d", m_state.m_leftX);
	grcDebugDraw::AddDebugOutput("LeftY %d", m_state.m_leftY);
	grcDebugDraw::AddDebugOutput("RightX %d", m_state.m_rightX);
	grcDebugDraw::AddDebugOutput("RightY %d", m_state.m_rightY);
	grcDebugDraw::AddDebugOutput("L1 %d", m_state.m_l1);
	grcDebugDraw::AddDebugOutput("L2 %d", m_state.m_l2);
	grcDebugDraw::AddDebugOutput("L3 %d", m_state.m_l3);
	grcDebugDraw::AddDebugOutput("R1 %d", m_state.m_r1);
	grcDebugDraw::AddDebugOutput("R2 %d", m_state.m_r2);
	grcDebugDraw::AddDebugOutput("R3 %d", m_state.m_r3);
	grcDebugDraw::AddDebugOutput("Square %d", m_state.m_square);
	grcDebugDraw::AddDebugOutput("Triangle %d", m_state.m_triangle);
	grcDebugDraw::AddDebugOutput("Circle %d", m_state.m_circle);
	grcDebugDraw::AddDebugOutput("Cross %d", m_state.m_cross);
	grcDebugDraw::AddDebugOutput("Up %d", m_state.m_dpadUp);
	grcDebugDraw::AddDebugOutput("Down %d", m_state.m_dpadDown);
	grcDebugDraw::AddDebugOutput("Left %d", m_state.m_dpadLeft);
	grcDebugDraw::AddDebugOutput("Right %d", m_state.m_dpadRight);
	grcDebugDraw::AddDebugOutput("Start %d", m_state.m_start);

#if RSG_ORBIS
	grcDebugDraw::AddDebugOutput("Touch %d", m_state.m_touchClick);
#else
	grcDebugDraw::AddDebugOutput("Select %d", m_state.m_select);
#endif // RSG_ORBIS

#if __PPU
	grcDebugDraw::AddDebugOutput("SensorX %d", m_state.m_sensorX);
	grcDebugDraw::AddDebugOutput("SensorY %d", m_state.m_sensorY);
	grcDebugDraw::AddDebugOutput("SensorZ %d", m_state.m_sensorZ);
	grcDebugDraw::AddDebugOutput("SensorG %d", m_state.m_sensorG);
#endif
#endif // DEBUG_DRAW
}
