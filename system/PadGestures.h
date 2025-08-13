// Title:		PadGestures.h
// Author:		Jack Potter
// Description: Classes to analyse input from motion sensors
// Created:		10/1/2008

#ifndef PadGestures_h__
#define PadGestures_h__

#define USE_SIXAXIS_GESTURES	(__PPU || RSG_ORBIS)

#if USE_SIXAXIS_GESTURES

#define MOTION_SENSOR_ZERO_COUNT (512)				// Reading at zero acceleration
#define MOTION_SENSOR_ACCEL_SENSITIVTY (113)		// Counts per G
#define MOTION_SENSOR_ANG_VEL_SENSITIVITY (123)		// Counts per 90 deg rot

#include "system/criticalsection.h"
#include "vector/matrix34.h"


class CPad;

#if __BANK
namespace rage{
	class Color32;
	class Vector2;
}
#endif

#define MAX_NUMBER_TRIGGER_STAGES (4)

// PURPOSE:
// Class to provide single interface for determining whether motion controls are enabled
// for a certain type of action.
// This checks user preferences as well as specific overrides applicable to certain types.
//
class CPadGestureMgr
{
public:

	// Keep up to date with commands_pad.sch!
	enum eMotionControlTypes
	{
		MC_TYPE_AIRCRAFT		= 0,    
		MC_TYPE_BIKE			= 1,    
		MC_TYPE_BOAT			= 2,    
		MC_TYPE_AFTERTOUCH		= 3,
		MC_TYPE_RELOAD			= 4,
		MC_TYPE_SCRIPT			= 5,
		MC_TYPE_RAGDOLL			= 6
	};

	enum eMotionControlStatus
	{
		MC_STATUS_USE_PREFERENCE,		// No override: Use user preferences
		MC_STATUS_FORCE_ON,				// Force controls on regardless of user preference
		MC_STATUS_FORCE_OFF				// Force controls off regardless of user preference
	};

	static bool GetMotionControlEnabled(eMotionControlTypes nMotionControlType);

	static u8 GetMotionControlStatus();
	static void SetMotionControlStatus(u8 nNewStatus) { sn_MotionControlStatus = nNewStatus; }

	static void CalibratePlayerPitchFromCurrent(const float fMinZeroPoint = -1.0f, const float fMaxZeroPoint = 1.0f);
	static void ResetPlayerPitchCalibration();

	static void Shutdown(unsigned shutdownMode);

private:
	// Are sixaxis controls overridden?
	static u8 sn_MotionControlStatus;
};

// Class to analyse motion sensor data and translate into game actions
class CPadGesture
{
	// Helper class for detecting the triggering of gestures.
	// Tracks triggering of multistage triggers, 
	//
	// e.g. could track a value going from 0.0 to 1.0 then to -1.0
	// in a specified restricted amount of time.
	//
	// Gestures start on stage 1. 
	// TriggerStage 0 defines the 'direction' the value needs to come from initially to get to stage 2
	class CGestureTrigger
	{
	public:
		CGestureTrigger();

		bool Update(float fValue, float fTimestep);

		// Sets trigger back to first stage
		// Note: Does not change value
		void Reset();

		void SetTrigger(int iTriggerNo, float fValue);
		void SetTimeout(int iStage, float fTimeout);
		void SetTotalNumStages(int iTotalStages);
		void SetStartPosition(float fStart){m_fStartPosition = fStart;}

		int GetCurrentStage() {return m_iCurrentStage;}
		int GetTotalNoStages() {return m_iTotalStages;}
		float GetCurrentValue() {return m_fCurrentValue;}
		float GetTriggerValue(int iTriggerNo) {return m_fTriggers[iTriggerNo];}
		float GetTimeoutValue(int iStage) {return m_fTimeouts[iStage];}
		float GetTimeOnCurrentStage(){return m_fTimeOnCurrentStage;}

#if __BANK
		// These are used to set up a RAG debug bank. Don't use as part of standard game logic
		float* GetTriggerValuePtrBANKONLY(int iTriggerNo){return &m_fTriggers[iTriggerNo];}
		float* GetTimeoutValuePtrBANKONLY(int iStage){return &m_fTimeouts[iStage];}
#endif

	private:
		sysCriticalSectionToken m_Cs;

		float m_fStartPosition;

		float m_fTriggers[MAX_NUMBER_TRIGGER_STAGES];
		float m_fTimeouts[MAX_NUMBER_TRIGGER_STAGES-1];		// First stage needs no timeout

		float m_fCurrentValue;
		float m_fTimeOnCurrentStage;

		int m_iCurrentStage;
		int m_iTotalStages;

	};

public:
	CPadGesture();
	~CPadGesture() { };

	void Update(CPad* pPad);

	// The orientation of a motion controller, allowing pitch roll and yaw to be read.
	// Since without calibration there is no way of knowing which direction of yaw is 'forward' 
	// this value can auto centre by adjusting m_fYawRotationResetRate
	////const Matrix34&	GetPadOrientation() const { return m_padOrientation; }

	// Roll, Pitch and Yaw in range -1.0 -> 1.0
	float			GetPadRoll() const;
	float			GetPadPitch() const;
	float			GetPadYaw() const;

	// Roll, Pitch and Yaw
	// This scales the actual pad input such that the min/max values give a return value of -1.0
	// bClamp Specifies whether output should be clamped to -1.0 -> 1.0
	inline float	GetPadRollInRange(float fMin, float fMax, bool bClamp) const {return GetScaledValueByRange(GetPadRoll(),fMin,fMax,-1.0f,1.0f,bClamp);}
	inline float	GetPadYawInRange(float fMin, float fMax, bool bClamp) const {return GetScaledValueByRange(GetPadYaw(),fMin,fMax,-1.0f,1.0f,bClamp);}
	inline float	GetPadPitchInRange(float fMin, float fMax, bool bClamp, float bUseCalibratedZero) const 
	{
		return bUseCalibratedZero ? 
			GetScaledValueByRange(GetPadPitch(),fMin + m_fPitchZeroValue,fMax + m_fPitchZeroValue,-1.0f,1.0f,bClamp)
		  :	GetScaledValueByRange(GetPadPitch(),fMin,fMax,-1.0f,1.0f,bClamp);
	}

#if RSG_ORBIS
	void ResetPadYaw()	{ m_bResetControllerOrientation = true; }
#else
	void ResetPadYaw()	{ m_fTotalRotationAboutPadZ = 0.0f; }
#endif

	// Combat gestures
	bool GetHasReloaded() const;
	float GetHasJumpedLeftRight() const;

	void CalibratePitchFromCurrentValue(const float fMinZeroPoint, const float fMaxZeroPoint);
	void SetPitchZeroValue(float fPitchZero) { m_fPitchZeroValue = fPitchZero; }

#if __BANK
	void InitWidgets(bkBank& bank);
#endif // __BANK

private:

	// Updates (in order of processing):
	void UpdateOrientation(CPad* pPad);
	void UpdateCombatGestures(CPad* pPad);

	// Calculated every update from sensors
#if RSG_ORBIS
	Vector3				m_vEulerAngle;
	bool				m_bExtrapolateYaw;
#else
	Matrix34			m_padOrientation;
	float				m_fTotalRotationAboutPadZ;		// DEGREES
	bool				m_bExtrapolateYaw;

	// Deadzones (around equilibrium position)
	s32				m_iSensorGDeadzone;

	// Yaw rotation slowly resets if sensor G is below deadzone
	float				m_fYawRotationResetRate;

	// Timers to track how long until the yaw starts to get reset
	float				m_fYawRotationResetTime;
	float				m_fYawTimeInDeadZone;
#endif

	// Combat triggers // 

	// We have two triggers for reload: if either of them fire then it counts as a reload
	CGestureTrigger m_ReloadTriggerZ;
	CGestureTrigger m_ReloadTriggerY;

	CGestureTrigger m_JumpRightTrigger;
	CGestureTrigger m_JumpLeftTrigger;
	bool m_bReloadTriggered;
	bool m_bJumpLeftTriggered;
	bool m_bJumpRightTriggered;

#if RSG_ORBIS
	bool m_bResetControllerOrientation;
#endif

	// PURPOSE:
	// Scales the value such that the minIn and maxIn values are scaled to fit the minOut and maxOut
	// bClamp specifies whether to clamp the return value to fMinOut / fMaxOut
	static float GetScaledValueByRange(const float fValue, const float fMinIn, const float fMaxIn, const float fMinOut = -1.0f, const float fMaxOut = 1.0f, const bool bClamped = false);

	float m_fPitchZeroValue;

#if __BANK
	void			UpdateSensorDebug(CPad* pPad);	
#endif

};

#endif // USE_SIXAXIS_GESTURES

#endif // PadGestures_h__
