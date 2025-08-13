//
// name:		pad.h
// description:	class handling pad input
#ifndef INC_PAD_H_
#define INC_PAD_H_

// Rage headers
#include "atl/array.h"
#include "input/pad.h"
#include "output/padactuatordevice.h"

//#include "vector/matrix34.h"

#define MAX_VIBRATION_FREQUENCY	(256.0f)  // 256 based on old SA value - could probably stick with it as the level guys are used to it, but may be better as a % value
#define MAX_VIBRATION_DROP_RATIO (0.8f)
#define MIN_VIBRATION_DROP (10)

#define USE_ACTUATOR_EFFECTS	1

#include "system/PadGestures.h"
#include "system/TouchPadGestures.h"

namespace rage {
	class ioActuatorEffect;
	class ioPad;
	class Quaternion;
}
//
// name:		CController
// description:	Class returning state of controller
class CPad
{
	friend class CControlMgr;
public:
	CPad();

	void SetPlayerNum(s32 num);
	void Update();
	void UpdateActuatorsAndShakes();
	void Clear();

	bool HasInput();
	s32 GetPressedButtons();
	s32 GetDebugButtons();
	s32 GetPressedDebugButtons();
	
#if RSG_ORBIS
	struct TouchPoint
	{
		u16		x;
		u16		y;
	};
#endif // RSG_ORBIS

	struct CPadState
	{
		s32 m_leftX;
		s32 m_leftY;
		s32 m_rightX;
		s32 m_rightY;
		s32 m_l1;
		s32 m_l2;
		s32 m_l3;
		s32 m_r1;
		s32 m_r2;
		s32 m_r3;
		s32 m_square;
		s32 m_triangle;
		s32 m_circle;
		s32 m_cross;
		s32 m_dpadUp;
		s32 m_dpadDown;
		s32 m_dpadLeft;
		s32 m_dpadRight;
		s32 m_start;
		s32 m_select;
		s32 m_touchClick;

#if __PPU
		// Motion sensors (PS3 Specific)
		// Values defined at top of header
		// XYZ give acceleration
		// Equilibrium = 512 (no accel)
		// Sensitivity = 113 counts/G
		s32 m_sensorX;		// +ve X is right of pad's face
		s32 m_sensorY;		// +ve Y is out of pad's face
		s32 m_sensorZ;		// +ve Z is down from pad's face

		// Yaw angular vel
		// Zero velocity = 512
		// Sensitivity = 123 counts / 90 deg
		s32 m_sensorG;
#endif

#if RSG_ORBIS
		TouchPoint	m_TouchOne;
		TouchPoint	m_TouchTwo;
		u8			m_TouchIdOne;
		u8			m_TouchIdTwo;
		u8			m_NumTouches;
#endif // RSG_ORBIS

		void Update(ioPad* pPad);
		void Clear();
	};


	s32 GetLeftStickX() { return m_state.m_leftX;}
	s32 GetLeftStickY() { return m_state.m_leftY;}
	s32 GetRightStickX() { return m_state.m_rightX; }
	s32 GetRightStickY() { return m_state.m_rightY; }

	s32 GetDPadUp() { return m_state.m_dpadUp; }
	s32 GetDPadDown() { return m_state.m_dpadDown; }
	s32 GetDPadLeft() { return m_state.m_dpadLeft; }
	s32 GetDPadRight() { return m_state.m_dpadRight; }

	s32 GetStart() { return m_state.m_start; }
	s32 GetSelect() { return m_state.m_select; }
	s32 GetTouchClick() { return m_state.m_touchClick; }

	s32 GetButtonSquare() { return m_state.m_square; }
	s32 GetButtonTriangle() { return m_state.m_triangle; }
	s32 GetButtonCross() { return m_state.m_cross; }
	s32 GetButtonCircle() { return m_state.m_circle; }

	s32 GetLeftShoulder1() { return m_state.m_l1;}
	s32 GetLeftShoulder2() { return m_state.m_l2; }
	s32 GetShockButtonL() { return m_state.m_l3; }
	s32 GetRightShoulder1() { return m_state.m_r1; }
	s32 GetRightShoulder2() { return m_state.m_r2; }
	s32 GetShockButtonR() { return m_state.m_r3; }
	
	bool LeftStickXJustActivated() { return (m_state.m_leftX && !m_oldState.m_leftX); }
	bool LeftStickYJustActivated() { return (m_state.m_leftY && !m_oldState.m_leftY); }
	bool RightStickXJustActivated() { return (m_state.m_rightX && !m_oldState.m_rightX); }
	bool RightStickYJustActivated() { return (m_state.m_rightY && !m_oldState.m_rightY); }

	bool StartJustDown() { return (m_state.m_start && !m_oldState.m_start);}
	bool StartJustUp() { return (!m_state.m_start && m_oldState.m_start);}
	bool SelectJustDown() { return (m_state.m_select && !m_oldState.m_select); }
	bool SelectJustUp() { return (!m_state.m_select && m_oldState.m_select); }

	bool ButtonSquareJustDown() { return (m_state.m_square && !m_oldState.m_square);}
	bool ButtonSquareJustUp() { return (!m_state.m_square && m_oldState.m_square);}
	bool ButtonTriangleJustDown() { return (m_state.m_triangle && !m_oldState.m_triangle);}
	bool ButtonTriangleJustUp() { return (!m_state.m_triangle && m_oldState.m_triangle);}
	bool ButtonCrossJustDown() { return (m_state.m_cross && !m_oldState.m_cross);}
	bool ButtonCrossJustUp() { return (!m_state.m_cross && m_oldState.m_cross);}
	bool ButtonCircleJustDown() { return (m_state.m_circle && !m_oldState.m_circle);}
	bool ButtonCircleJustUp() { return (!m_state.m_circle && m_oldState.m_circle);}

	bool LeftShoulder1JustDown() { return (m_state.m_l1 && !m_oldState.m_l1);}
	bool LeftShoulder1JustUp() { return (!m_state.m_l1 && m_oldState.m_l1);}
	bool LeftShoulder2JustDown() { return (m_state.m_l2 && !m_oldState.m_l2);}
	bool LeftShoulder2JustUp() { return (!m_state.m_l2 && m_oldState.m_l2);}
	bool ShockButtonLJustDown() { return (m_state.m_l3 && !m_oldState.m_l3);}
	bool ShockButtonLJustUp() { return (!m_state.m_l3 && m_oldState.m_l3);}
	bool RightShoulder1JustDown() { return (m_state.m_r1 && !m_oldState.m_r1);}
	bool RightShoulder1JustUp() { return (!m_state.m_r1 && m_oldState.m_r1);}
	bool RightShoulder2JustDown() { return (m_state.m_r2 && !m_oldState.m_r2);}
	bool RightShoulder2JustUp() { return (!m_state.m_r2 && m_oldState.m_r2);}
	bool ShockButtonRJustDown() { return (m_state.m_r3 && !m_oldState.m_r3);}
	bool ShockButtonRJustUp() { return (!m_state.m_r3 && m_oldState.m_r3);}
	bool TouchClickJustDown() { return (m_state.m_touchClick && !m_oldState.m_touchClick);}
	bool TouchClickJustUp() { return (!m_state.m_touchClick && m_oldState.m_touchClick);}

	bool DPadUpJustDown() { return (m_state.m_dpadUp && !m_oldState.m_dpadUp);}
	bool DPadUpJustUp() { return (!m_state.m_dpadUp && m_oldState.m_dpadUp);}
	bool DPadDownJustDown() { return (m_state.m_dpadDown && !m_oldState.m_dpadDown);}
	bool DPadDownJustUp() { return (!m_state.m_dpadDown && m_oldState.m_dpadDown);}
	bool DPadLeftJustDown() { return (m_state.m_dpadLeft && !m_oldState.m_dpadLeft);}
	bool DPadLeftJustUp() { return (!m_state.m_dpadLeft && m_oldState.m_dpadLeft);}
	bool DPadRightJustDown() { return (m_state.m_dpadRight && !m_oldState.m_dpadRight);}
	bool DPadRightJustUp() { return (!m_state.m_dpadRight && m_oldState.m_dpadRight);}

#if USE_ACTUATOR_EFFECTS
	void ApplyActuatorEffect(ioActuatorEffect* effect, bool isScriptCommand, s32 suppressedId, u32 delayAfterThisOne);
	void ApplyActuatorEffect(ioActuatorEffect* effect, bool isScriptCommand, s32 suppressedId);
	void ApplyActuatorEffect(ioActuatorEffect* effect, bool isScriptCommand);
	void ApplyActuatorEffect(ioActuatorEffect* effect);
#else
	// pad shake
	void  StartShake(u32 Duration0, s32 MotorFrequency0 = 0, u32 Duration1 = 0, s32 MotorFrequency1 = 0, s32 DelayAfterThisOne = 0, s32 suppressedId = -1);
	void  StartShake_Distance(u32 Duration, s32 Frequency, float X, float Y, float Z);
	void  StartShake_Train(float X, float Y);

#if HAS_TRIGGER_RUMBLE
	void  StartTriggerShake(u32 LeftDuration, s32 LeftMotorFrequency, u32 RightDuration, s32 RightMotorFrequency, s32 suppressedId = -1);
#endif // HAS_TRIGGER_RUMBLE

	void SetScriptShake(bool fromScript) { m_bIsScriptShake = fromScript; }
#endif // USE_ACTUATOR_EFFECTS

	void  StopShaking(bool bForce = false);
	bool  IsShaking();
	void  NoMoreShake() { m_bShakeStoped = true; }
	void  AllowShake() { m_bShakeStoped = false; }
	bool  IsPreventedFromShaking() { return m_bShakeStoped; }

	const static s32 NO_SUPPRESS = -1;

	void  SetSuppressedId(s32 uniqueId) { m_suppressedId = uniqueId; }

	static bool IsIntercepted() { return ioPad::IsIntercepted(); }

	bool IsConnected( PPU_ONLY(bool bIgnoreWasConnected = false) );
	bool IsXBox360Pad() const;
	bool IsXBox360CompatiblePad() const;
	bool IsPs3Pad() const;
	bool IsPs4Pad() const;
	bool HasSensors() const;

#if USE_SIXAXIS_GESTURES
	CPadGesture*	GetPadGesture() { return HasSensors() ? &m_PadGesture : NULL; }
#endif

#if __PPU
	// SIXAXIS
	// Raw pad data
	s32 GetSensorX(){ return m_state.m_sensorX; }
	s32 GetSensorY(){ return m_state.m_sensorY; }
	s32 GetSensorZ(){ return m_state.m_sensorZ; }
	s32 GetSensorG(){ return m_state.m_sensorG; }

	// Normalised data. 
	// xyz accel in Gs
	// ang vel G in deg / s
	float GetSensorXf(){ return (m_state.m_sensorX - MOTION_SENSOR_ZERO_COUNT)/(float)MOTION_SENSOR_ACCEL_SENSITIVTY;}
	float GetSensorYf(){ return (m_state.m_sensorY - MOTION_SENSOR_ZERO_COUNT)/(float)MOTION_SENSOR_ACCEL_SENSITIVTY;}
	float GetSensorZf(){ return (m_state.m_sensorZ - MOTION_SENSOR_ZERO_COUNT)/(float)MOTION_SENSOR_ACCEL_SENSITIVTY;}
	float GetSensorGf(){ return 90.0f*(m_state.m_sensorG - MOTION_SENSOR_ZERO_COUNT)/(float)MOTION_SENSOR_ANG_VEL_SENSITIVITY;}
#endif

#if RSG_ORBIS
	bool  GetOrientation(Quaternion& result) const { if (m_pPad) { result = m_pPad->GetOrientation(); return true; } return false; }
	void  ResetOrientation()				{ if (m_pPad) { m_pPad->ResetOrientation(); } }

	bool  GetAcceleration(Vector3& acceleration) const { if (m_pPad) { acceleration = m_pPad->GetAcceleration(); return true; } return false; }

	u8    GetNumTouches() const				{ return m_state.m_NumTouches; }
	u8    GetTouchId(int index) const		{ return (index == 0) ? m_state.m_TouchIdOne : m_state.m_TouchIdTwo; }
	void  GetTouchPoint(int index, u16& x, u16& y);

	const CTouchPadGesture& GetTouchPadGesture() { return m_TouchPadGesture; }

	bool IsRemotePlayPad() const			{ if (m_pPad) { return m_pPad->IsRemotePlayPad(); } return false; }

	bool HasBeenConnected() const			{ if (m_pPad) { return m_pPad->HasBeenConnected(); } return false; }
#endif // RSG_ORBIS

	u32 GetTimeInMilliseconds();

	void PrintDebug();


private:
	void UpdateFromPad(CPad& pad) {m_oldState = m_state; m_state = pad.m_state;}

	const static u32 MAX_SHAKE_ENTRIES = 32;
	const static u32 MAX_SCRIPT_SHAKE_ENTRIES = 4;

#if USE_ACTUATOR_EFFECTS
	struct ActuatorEntry
	{
		ActuatorEntry();
		ioActuatorEffect*	m_pEffect;
		u32					m_StartTime;
	};

	atFixedArray<ActuatorEntry, MAX_SHAKE_ENTRIES> m_ActuatorEffects;

	atFixedArray<ActuatorEntry, MAX_SCRIPT_SHAKE_ENTRIES> m_ScriptActuatorEffects;
	ioPadActuatorDevice	m_Actuator;
	
	void UpdateActuatorEffects();
	void RemoveExpiredActuatorEffects();
#else
	// Rumble entry
	struct RumbleEntry
	{
		RumbleEntry() { m_iStartTime = m_iDuration = 0; }
		u32	m_iStartTime;			// The start time
		u32	m_iDuration;			
		s32	m_Freq;
		u32	m_IsScript:1;
	};

	int	 FindSmallestShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries);
	int	 FindLargestShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries);
	RumbleEntry *FindScriptEntry(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries);
	void PushNewShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries, s32 Frequency, u32 Duration);
	void RegisterShake(atFixedArray <RumbleEntry, MAX_SHAKE_ENTRIES> &rumbleEntries, s32 Frequency, u32 Duration);
	void RemoveLeastImportantShakes();
	void RemoveExpiredShakes();
	RumbleEntry *FindCurrentShake(int actID);
	void UpdateShakes();

	atFixedArray<RumbleEntry, MAX_SHAKE_ENTRIES>	m_RumbleEntries[ioPad::MAX_ACTUATORS];

	bool			m_bIsScriptShake;
	s32				m_noShakeFreq;				// Don't shake if the frequency is lower than this? (only obeyed by one func)
#endif // USE_ACTUATOR_EFFECTS

	ioPad* m_pPad;
	CPadState m_state;
	CPadState m_oldState;

	// pad rumble
	float			m_DeathRumbleScaler;
	bool			m_bIsShaking;
	bool			m_bShakeStoped;				// Stop shaking after the current shake has completed flag?
	u32				m_noShakeBeforeThis;		// Presumably a don't shake until this time? (only obeyed by one func)
	s32				m_suppressedId;				// If the ID passed to StartPadShake == this ID, then don't register the command (only obeyed by one func)

#if __PPU
	bool				m_WasConnected;
#endif

#if USE_SIXAXIS_GESTURES
	CPadGesture			m_PadGesture;
#endif

#if RSG_ORBIS
	CTouchPadGesture	m_TouchPadGesture;
#endif // RSG_ORBIS

};

#if USE_ACTUATOR_EFFECTS
inline CPad::ActuatorEntry::ActuatorEntry()
	: m_pEffect(NULL)
	, m_StartTime(0u)
{
}

inline void CPad::ApplyActuatorEffect(ioActuatorEffect* effect)
{
	ApplyActuatorEffect(effect, false, NO_SUPPRESS, 0);
}

inline void CPad::ApplyActuatorEffect(ioActuatorEffect* effect, bool isScriptCommand)
{
	ApplyActuatorEffect(effect, isScriptCommand, NO_SUPPRESS, 0);
}

inline void CPad::ApplyActuatorEffect(ioActuatorEffect* effect, bool isScriptCommand, s32 suppressedId)
{
	ApplyActuatorEffect(effect, isScriptCommand, suppressedId, 0);
}
#endif // USE_ACTUATOR_EFFECTS

#endif // INC_PAD_H_
