#include "System/PadGestures.h"

#if USE_SIXAXIS_GESTURES

// Rage headers
#if __BANK
#include "bank\bkmgr.h"
#include "bank\bank.h"
#include "bank\combo.h"
#include "bank\slider.h"
#include "bank\group.h"
#endif

// Framework headers
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
//#include "fwmaths/Maths.h"

// Game headers
#include "System/pad.h"
#include "frontend\PauseMenu.h"
#include "system\controlmgr.h"
#include "peds/Ped.h"
#include "scene\world\gameWorld.h"

#if __BANK
#include "camera/viewports/ViewportManager.h"
#include "Vehicles\Heli.h"
#include "Vehicles\Planes.h" 
#include "Vehicles\Bike.h"
#include "Vehicles\Boat.h"
	static bool sbDebugSensors = false;
	static bool sbDebugOrientation = false;
	static bool sbDebugBikePitchDeadzone = false;
	static bool sbDebugBikeInputLimits = false;
	static bool sbDebugCombat = false;
	static bool sbCalibratePitch = false;
	static float sfOrientationScale = 1.5f;
#endif

// Init CPadGestureMgr statics
u8 CPadGestureMgr::sn_MotionControlStatus = CPadGestureMgr::MC_STATUS_USE_PREFERENCE;

static bank_float sfMaxCalibrationOffset = 0.2f;

bool CPadGestureMgr::GetMotionControlEnabled(eMotionControlTypes nMotionControlType)
{
	bool bReturn = false;

	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	CPlayerInfo* pPlayer = pPlayerPed ? pPlayerPed->GetPlayerInfo() : NULL;

	if(pPlayer && pPlayer->AreControlsDisabled() && nMotionControlType != MC_TYPE_SCRIPT)
	{
		bReturn = false;
	}
	else if(sn_MotionControlStatus == MC_STATUS_FORCE_ON)
	{
		bReturn = true;
	}
	else if(sn_MotionControlStatus == MC_STATUS_FORCE_OFF)
	{
		bReturn = false;
	}
	else if(sn_MotionControlStatus == MC_STATUS_USE_PREFERENCE)
	{
		switch(nMotionControlType)
		{
		case MC_TYPE_AIRCRAFT:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_HELI);
			break;
		case MC_TYPE_BIKE:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_BIKE);
			break;
		case MC_TYPE_BOAT:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_BOAT);
			break;
		case MC_TYPE_RELOAD:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_RELOAD);
			break;
		case MC_TYPE_AFTERTOUCH:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_AFTERTOUCH);
			break;
		case MC_TYPE_RAGDOLL:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_ACTIVITY);
			break;
		case MC_TYPE_SCRIPT:
			bReturn = (bool)CPauseMenu::GetMenuPreference(PREF_SIXAXIS_ACTIVITY);
			break;
		default:
			Assertf(false,"CPadGestureMgr::GetMotionControlEnabled : Unhandled motion control type");
			break;
		}
	}

	return bReturn;
}

void CPadGestureMgr::CalibratePlayerPitchFromCurrent(const float fMinZeroPoint, const float fMaxZeroPoint)
{
	CPad* pPad = CControlMgr::GetPlayerPad();
	if(pPad && pPad->GetPadGesture())
	{
		pPad->GetPadGesture()->CalibratePitchFromCurrentValue(rage::Max(fMinZeroPoint,-sfMaxCalibrationOffset),rage::Min(fMaxZeroPoint,sfMaxCalibrationOffset));
	}
#if RSG_ORBIS
	// TODO: Disable pitch offset for PS4.
	pPad->GetPadGesture()->SetPitchZeroValue(0.0f);
#endif
}

void CPadGestureMgr::ResetPlayerPitchCalibration()
{
	CPad* pPad = CControlMgr::GetPlayerPad();
	if(pPad && pPad->GetPadGesture())
	{
		pPad->GetPadGesture()->SetPitchZeroValue(0.0f);
	}
}

void CPadGestureMgr::Shutdown(unsigned /*shutdownMode*/)
{
	SetMotionControlStatus(MC_STATUS_USE_PREFERENCE);
	ResetPlayerPitchCalibration();
}

/////////////////////////////////////////////////////////////////////////////////////////////

CPadGesture::CPadGesture()
{
#if RSG_ORBIS
	m_vEulerAngle.Zero();
	m_bExtrapolateYaw = false;
	m_bResetControllerOrientation = false;
#else
	m_padOrientation.Identity();
	m_fTotalRotationAboutPadZ = 0.0f;
	m_fYawRotationResetRate = 100.0f;
	m_fYawRotationResetTime = 0.75f;
	m_fYawTimeInDeadZone = 0.0f;
	m_iSensorGDeadzone = MOTION_SENSOR_ANG_VEL_SENSITIVITY / 20;
	m_bExtrapolateYaw = false;
#endif

	// Set up our combat triggers
	// Primary reload action: pitch controller up then down
	m_ReloadTriggerZ.SetStartPosition(1.0f);
	m_ReloadTriggerZ.SetTrigger(0,0.18f);
	m_ReloadTriggerZ.SetTrigger(1,0.25f);
	m_ReloadTriggerZ.SetTrigger(2,-0.10f);
	m_ReloadTriggerZ.SetTimeout(0,0.8f);
	m_ReloadTriggerZ.SetTimeout(1,1.0f);
	m_ReloadTriggerZ.SetTotalNumStages(3);

	// Another trigger for detecting reloads, vertical accel (gravity is subtracted so 0 = pad face up)
	m_ReloadTriggerY.SetStartPosition(1.0f);
	m_ReloadTriggerY.SetTrigger(0,-0.2f);
	m_ReloadTriggerY.SetTrigger(1,0.6f);
	m_ReloadTriggerY.SetTrigger(2,0.2f);
	m_ReloadTriggerY.SetTimeout(0,0.3f);
	m_ReloadTriggerY.SetTimeout(1,0.3f);
	m_ReloadTriggerY.SetTotalNumStages(3);

	m_JumpRightTrigger.SetStartPosition(-1.0f);
	m_JumpRightTrigger.SetTrigger(0,0.5f);
	m_JumpRightTrigger.SetTimeout(0,2.0f);
	m_JumpRightTrigger.SetTotalNumStages(1);

	m_JumpLeftTrigger.SetStartPosition(1.0f);
	m_JumpLeftTrigger.SetTrigger(0,-0.5f);
	m_JumpLeftTrigger.SetTimeout(0,2.0f);
	m_JumpLeftTrigger.SetTotalNumStages(1);

	m_bJumpRightTriggered = false;
	m_bJumpLeftTriggered = false;
	m_bReloadTriggered = false;

	m_fPitchZeroValue = 0.0f;
}

void CPadGesture::Update( CPad* pPad )
{
#if RSG_ORBIS
	if (m_bResetControllerOrientation)
	{
		pPad->ResetOrientation();
		m_bResetControllerOrientation = false;
	}
#endif

	UpdateOrientation(pPad);
	UpdateCombatGestures(pPad);

#if __BANK
	UpdateSensorDebug(pPad);
#endif

}

void CPadGesture::UpdateOrientation(CPad* pPad)
{
	// Define a matrix to convert from sensor coords to an 'upright' pad
	// i.e. we want pad face to be UP, pad cable to be +y

#if __PPU
	static Matrix34 padMat;
	static bool bMatCalculated = false;
	if(!bMatCalculated)
	{
		padMat.a = XAXIS;
		padMat.b = -ZAXIS;
		padMat.c = -YAXIS;
		bMatCalculated = true;
	}

	Vector3 vPadAccel;
	vPadAccel.x = (float)(pPad->GetSensorX() - MOTION_SENSOR_ZERO_COUNT) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY;
	vPadAccel.y = (float)(pPad->GetSensorY() - MOTION_SENSOR_ZERO_COUNT) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY;
	vPadAccel.z = (float)(pPad->GetSensorZ() - MOTION_SENSOR_ZERO_COUNT) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY;

	if(vPadAccel.IsZero())
	{
		return;
	}
	vPadAccel.Normalize();

	//Transform using our pad matrix
	padMat.Transform(vPadAccel);

	m_padOrientation.Identity();
	m_padOrientation.MakeRotateTo(ZAXIS,vPadAccel);

	if(m_bExtrapolateYaw)
	{
		// Consider rotation about gravitational axis
		float fCurrentAngSpeed = (float)(pPad->GetSensorG() - MOTION_SENSOR_ZERO_COUNT) / (float)MOTION_SENSOR_ANG_VEL_SENSITIVITY;
		fCurrentAngSpeed *= 90.0f;

		m_fTotalRotationAboutPadZ += fCurrentAngSpeed * fwTimer::GetTimeStep();

		// Account for wrap around
		if(m_fTotalRotationAboutPadZ < -180.0f)
		{
			m_fTotalRotationAboutPadZ += 360.0f;
		}
		else if (m_fTotalRotationAboutPadZ > 180.0f)
		{
			m_fTotalRotationAboutPadZ -= 360.0f;
		}

		// Add on the angular velocity 
		// sensorG is in units of deg / sec
		m_padOrientation.Rotate(-m_padOrientation.c,m_fTotalRotationAboutPadZ * (PI / 180.0f));

		// Some stuff for auto resetting the yaw
		int absG = (pPad->GetSensorG()- MOTION_SENSOR_ZERO_COUNT);
		if (absG < 0)
		{
			absG *= -1;
		}

		if(absG < m_iSensorGDeadzone)
		{
			m_fYawTimeInDeadZone += fwTimer::GetTimeStep();
			if(m_fYawTimeInDeadZone > m_fYawRotationResetTime)
			{
				if(m_fTotalRotationAboutPadZ > 0.0f)
				{
					m_fTotalRotationAboutPadZ -= m_fYawRotationResetRate * fwTimer::GetTimeStep();
					if(m_fTotalRotationAboutPadZ < 0.0f)
						m_fTotalRotationAboutPadZ = 0.0f;
				}
				else
				{
					m_fTotalRotationAboutPadZ += m_fYawRotationResetRate * fwTimer::GetTimeStep();
					if(m_fTotalRotationAboutPadZ > 0.0f)
						m_fTotalRotationAboutPadZ = 0.0f;
				}
			}
		}
		else
		{
			// We are not in deadzone so reset timer
			m_fYawTimeInDeadZone = 0.0f;
		}
	}	// Endif extrapolate yaw
	else
	{
		m_fTotalRotationAboutPadZ = 0.0f;
	}
#elif RSG_ORBIS
	Quaternion qPadOrientation;
	if (pPad->GetOrientation( qPadOrientation ))
	{
		qPadOrientation.ToEulers(m_vEulerAngle, eEulerOrderXYZ);

		m_vEulerAngle.x /= (PI*0.50f);		// pitch = [-1.0f, 1.0f]
		m_vEulerAngle.z /= (-PI*0.50f);		// roll = [-1.0f, 1.0f]

		if (m_bExtrapolateYaw)
		{
			m_vEulerAngle.y /= (-PI);			// yaw = [-1.0f, 1.0f]
		}
		else
		{
			m_vEulerAngle.y = 0.0f;
		}
	}
	else
	{
		m_vEulerAngle.Set(0.0f, 0.0f, 0.0f);
	}
#else
	Assertf(false, "Unsupported platform for pad gestures");
#endif
}

void CPadGesture::UpdateCombatGestures(CPad* pPad)
{
	float fTimestep = fwTimer::GetTimeStep();

	float fPitchAccel = 0.0f;
	float fRollAccel =  0.0f;
	float fVertAccel = -1.0f;
#if __PPU
	// Pitch or 'forwards' accel
	fPitchAccel = ((float)(pPad->GetSensorZ() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY;

	// Roll or  lateral accel
	fRollAccel = ((float)(pPad->GetSensorX() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY;

	// Take into account a bit of vert accel for reload
	fVertAccel = ((float)(pPad->GetSensorY() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY;
#elif RSG_ORBIS
	Vector3 vAcceleration(Vector3::ZeroType);
	pPad->GetAcceleration(vAcceleration);

	fPitchAccel = vAcceleration.z;
	fRollAccel  = vAcceleration.x;
	fVertAccel  = vAcceleration.y;
#endif

	fVertAccel += 1.0f; // Subtract gravity

	m_bReloadTriggered = m_ReloadTriggerZ.Update(fPitchAccel,fTimestep);
	m_bReloadTriggered |= m_ReloadTriggerY.Update(fVertAccel,fTimestep);
	m_bJumpRightTriggered = m_JumpRightTrigger.Update(fRollAccel,fTimestep);
	m_bJumpLeftTriggered = m_JumpLeftTrigger.Update(fRollAccel,fTimestep);
}

float CPadGesture::GetPadRoll() const
{
#if RSG_ORBIS
	return m_vEulerAngle.z;
#else
	return m_padOrientation.c.x;
#endif
}

float CPadGesture::GetPadPitch() const
{
#if RSG_ORBIS
	return m_vEulerAngle.x;
#else
	return -m_padOrientation.c.y;
#endif
}

float CPadGesture::GetPadYaw() const
{
#if RSG_ORBIS
	return m_vEulerAngle.y;
#else
	// Normalise so in range -1 - > 1
	return (m_fTotalRotationAboutPadZ / 180.0f);
#endif
}

float CPadGesture::GetHasJumpedLeftRight() const
{
	float fReturn = 0.0f;

	if(m_bJumpLeftTriggered)
	{
		fReturn -= 1.0f;
	}

	if(m_bJumpRightTriggered)
	{
		fReturn += 1.0f;
	}

	return fReturn;
}

bool CPadGesture::GetHasReloaded() const
{
	return m_bReloadTriggered;
}

#if __BANK
void CPadGesture::InitWidgets( bkBank& bank )
{
	bank.PushGroup("SIXAXIS Debug");
	bank.AddToggle("Debug sensors", &sbDebugSensors);
#if __PPU
	bank.AddSlider("Yaw reset rate",&m_fYawRotationResetRate,0.0f,2500.0f,1.0f);
	bank.AddSlider("Yaw deadzone",&m_iSensorGDeadzone,0,100,1.0f);
	bank.AddSlider("Yaw deadzone timer", &m_fYawRotationResetTime,0.0f,20.0f,0.1f);
#endif
	bank.AddToggle("Extrapolate yaw",&m_bExtrapolateYaw);
	bank.AddToggle("Debug orientation",&sbDebugOrientation);
	bank.AddSlider("Orientation axis scale",&sfOrientationScale,0.0f,20.0f,0.1f);
	bank.AddToggle("Calibrate pitch now",&sbCalibratePitch);
	bank.AddSlider("Max calibration offset",&sfMaxCalibrationOffset,0.0f,1.0f,0.01f);
	bank.AddToggle("Debug combat",&sbDebugCombat);
	bank.AddSlider("Y Reload trigger 1",m_ReloadTriggerY.GetTriggerValuePtrBANKONLY(0),-1.0f,1.0f,0.01f);
	bank.AddSlider("Y Reload trigger 2",m_ReloadTriggerY.GetTriggerValuePtrBANKONLY(1),-1.0f,1.0f,0.01f);
	bank.AddSlider("Y Reload trigger 3",m_ReloadTriggerY.GetTriggerValuePtrBANKONLY(2),-1.0f,1.0f,0.01f);
	bank.AddSlider("Y Reload timeout 1",m_ReloadTriggerY.GetTimeoutValuePtrBANKONLY(0),0.0f,5.0f,0.01f);
	bank.AddSlider("Y Reload timeout 2",m_ReloadTriggerY.GetTimeoutValuePtrBANKONLY(1),0.0f,5.0f,0.01f);
	bank.AddSlider("Z Reload trigger 1",m_ReloadTriggerZ.GetTriggerValuePtrBANKONLY(0),-1.0f,1.0f,0.01f);
	bank.AddSlider("Z Reload trigger 2",m_ReloadTriggerZ.GetTriggerValuePtrBANKONLY(1),-1.0f,1.0f,0.01f);
	bank.AddSlider("Z Reload trigger 3",m_ReloadTriggerZ.GetTriggerValuePtrBANKONLY(2),-1.0f,1.0f,0.01f);
	bank.AddSlider("Z Reload timeout 1",m_ReloadTriggerZ.GetTimeoutValuePtrBANKONLY(0),0.0f,5.0f,0.01f);
	bank.AddSlider("Z Reload timeout 2",m_ReloadTriggerZ.GetTimeoutValuePtrBANKONLY(1),0.0f,5.0f,0.01f);
	bank.PushGroup("Vehicle Input Limits",false);
	bank.PushGroup("Heli",false);
	bank.AddSlider("Yaw mult",&CHeli::MOTION_CONTROL_YAW_MULT,0.0f,100.0f,0.1f);
	bank.AddSlider("Pitch lower limit",&CHeli::MOTION_CONTROL_PITCH_MIN,-1.0f,1.0f,0.1f);
	bank.AddSlider("Pitch upper limit",&CHeli::MOTION_CONTROL_PITCH_MAX,-1.0f,1.0f,0.1f);
	bank.AddSlider("Roll lower limit",&CHeli::MOTION_CONTROL_ROLL_MIN,-1.0f,1.0f,0.1f);
	bank.AddSlider("Roll upper limit",&CHeli::MOTION_CONTROL_ROLL_MAX,-1.0f,1.0f,0.1f);
	bank.PopGroup();
	bank.PushGroup("Plane",false);
	bank.AddSlider("Yaw mult",&CPlane::MOTION_CONTROL_YAW_MULT,0.0f,100.0f,0.1f);
	bank.AddSlider("Pitch lower limit",&CPlane::MOTION_CONTROL_PITCH_MIN,-2.0f,2.0f,0.1f);
	bank.AddSlider("Pitch upper limit",&CPlane::MOTION_CONTROL_PITCH_MAX,-2.0f,2.0f,0.1f);
	bank.AddSlider("Roll lower limit",&CPlane::MOTION_CONTROL_ROLL_MIN,-2.0f,2.0f,0.1f);
	bank.AddSlider("Roll upper limit",&CPlane::MOTION_CONTROL_ROLL_MAX,-2.0f,2.0f,0.1f);
	bank.PopGroup();
	bank.PushGroup("Bike",false);
	bank.AddSlider("Pitch lower limit",&CBike::ms_fMotionControlPitchMin,-1.0f,1.0f,0.1f);
	bank.AddSlider("Pitch upper limit",&CBike::ms_fMotionControlPitchMax,-1.0f,1.0f,0.1f);
	bank.AddSlider("Roll lower limit",&CBike::ms_fMotionControlRollMin,-1.0f,1.0f,0.1f);
	bank.AddSlider("Roll upper limit",&CBike::ms_fMotionControlRollMax,-1.0f,1.0f,0.1f);
	bank.AddSlider("Lean pitch down deadzone",&CBike::ms_fMotionControlPitchDownDeadzone,-1.0f,0.0f,0.01f);
	bank.AddSlider("Lean pitch up deadzone",&CBike::ms_fMotionControlPitchUpDeadzone,0.0f,1.0f,0.01f);
	bank.AddToggle("Debug bike deadzones",&sbDebugBikePitchDeadzone);
	bank.AddToggle("Debug bike input limits",&sbDebugBikeInputLimits);
	bank.PopGroup();
	bank.PushGroup("Boat",false);
	bank.AddSlider("Pitch lower limit",&CBoat::MOTION_CONTROL_PITCH_MIN,-1.0f,1.0f,0.1f);
	bank.AddSlider("Pitch upper limit",&CBoat::MOTION_CONTROL_PITCH_MAX,-1.0f,1.0f,0.1f);
	bank.AddSlider("Roll lower limit",&CBoat::MOTION_CONTROL_ROLL_MIN,-1.0f,1.0f,0.1f);
	bank.AddSlider("Roll upper limit",&CBoat::MOTION_CONTROL_ROLL_MAX,-1.0f,1.0f,0.1f);
	bank.PopGroup();
	bank.AddSlider("Default pitch lower limit",&CVehicle::ms_fDefaultMotionContolPitchMin,-1.0f,1.0f,0.1f);
	bank.AddSlider("Default pitch upper limit",&CVehicle::ms_fDefaultMotionContolPitchMax,-1.0f,1.0f,0.1f);
	bank.AddSlider("Default roll lower limit",&CVehicle::ms_fDefaultMotionContolRollMin,-1.0f,1.0f,0.1f);
	bank.AddSlider("Default roll upper limit",&CVehicle::ms_fDefaultMotionContolRollMax,-1.0f,1.0f,0.1f);
	bank.AddSlider("Vehicle steer smooth rate override", &CVehicle::ms_fDefaultMotionContolSteerSmoothRate,0.0f,100.0f,0.1f);
	bank.PopGroup();
	bank.PushGroup("Vehicle aftertouch", false);
	bank.AddSlider("Aftertouch Mult", &CVehicle::ms_fDefaultMotionContolAftertouchMult,0.0f,100.0f,0.1f);
	bank.PopGroup();
	bank.PopGroup();
}


void CPadGesture::UpdateSensorDebug(CPad* pPad)
{
	if(!pPad || pPad!=CControlMgr::GetPlayerPad())
	{
		return;
	}

	static const float fDebugAxisLength = 0.25f;
	static const float fDebugAxisLengthVert = fDebugAxisLength * (16.0f/9.0f);
	static const float fDebugDrawWidth = fDebugAxisLength / 20.0f;

	static Vector2 ROLL_AXIS_POSITION	= Vector2((1.0f / 2.0f) - (fDebugAxisLength/2.0f),1.0f / 3.0f);
	static Vector2 PITCH_AXIS_POSITION	= Vector2(3.5f / 5.0f, 1.0f / 3.0f);
	static Vector2 RELOAD1_POSITION		= Vector2(4.5f / 5.0f, 1.0f / 3.0f);
	static Vector2 RELOAD2_POSITION		= Vector2(4.75f / 5.0f,1.0f / 3.0f);

#if __PPU
	static Vector2 SENSORX_POSITION		= Vector2((1.5f / 5.0f)-0.5f*fDebugAxisLength,(1.0f/3.0f)+0.5f*fDebugAxisLengthVert);
	static Vector2 SENSORY_POSITION		= Vector2(1.0f / 10.0f, 1.0f / 3.0f);
	static Vector2 SENSORZ_POSITION		= Vector2((1.5f / 5.0f),1.0f/3.0f);
	static Vector2 SENSORG_POSITION		= Vector2((1.5f / 5.0f)-0.5f*fDebugAxisLength,(1.0f/3.0f)-0.25f*fDebugAxisLengthVert);
#endif

	if(sbCalibratePitch)
	{
		CPadGestureMgr::CalibratePlayerPitchFromCurrent(-1.0f,1.0f);
		sbCalibratePitch = false;
	}

	if(sbDebugOrientation)
	{
		if(FindPlayerPed())
		{
		#if RSG_ORBIS
			Matrix34 debugMat;
			Quaternion qPadOrientation;
			if (!pPad->GetOrientation( qPadOrientation ))
			{
				qPadOrientation.Identity();
			}
			debugMat.FromQuaternion( qPadOrientation );
		#else
			Matrix34 debugMat = m_padOrientation;
		#endif
			debugMat.Dot(MAT34V_TO_MATRIX34(FindPlayerPed()->GetMatrix()));
			grcDebugDraw::Axis(debugMat,sfOrientationScale);
		}

		grcDebugDraw::Meter(ROLL_AXIS_POSITION,Vector2(1.0f,0.0f),fDebugAxisLength,fDebugDrawWidth,Color32(255,255,255),"Roll");
		grcDebugDraw::MeterValue(ROLL_AXIS_POSITION, Vector2(1.0f,0.0f), fDebugAxisLength, GetPadRoll(), fDebugDrawWidth, Color32(255,0,0));
		grcDebugDraw::MeterValue(ROLL_AXIS_POSITION, Vector2(1.0f,0.0f), fDebugAxisLength, 0.0f, fDebugDrawWidth/2.0f, Color32(255,255,255));

		grcDebugDraw::Meter(PITCH_AXIS_POSITION,Vector2(0.0f,1.0f),fDebugAxisLengthVert,fDebugDrawWidth,Color32(255,255,255),"Pitch");
		grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -GetPadPitch(), fDebugDrawWidth, Color32(255,0,0));
		grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, 0.0f, fDebugDrawWidth/2.0f, Color32(255,255,255));
		grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -m_fPitchZeroValue, fDebugDrawWidth/2.0f, Color32(0,0,0));
		if(sbDebugBikePitchDeadzone)
		{
			// Need to re scale deadzones from min/max range to real one
			const float fMinDeadzone = GetScaledValueByRange(CBike::ms_fMotionControlPitchDownDeadzone,-1.0f,1.0f,CBike::ms_fMotionControlPitchMin,CBike::ms_fMotionControlPitchMax,true) + m_fPitchZeroValue;
			const float fMaxDeadzone = GetScaledValueByRange(CBike::ms_fMotionControlPitchUpDeadzone,-1.0f,1.0f,CBike::ms_fMotionControlPitchMin,CBike::ms_fMotionControlPitchMax,true) + m_fPitchZeroValue;
			grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -fMinDeadzone, fDebugDrawWidth/2.0f, Color32(0,0,255));
			grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -fMaxDeadzone, fDebugDrawWidth/2.0f, Color32(0,0,255));
		}
		if(sbDebugBikeInputLimits)
		{
			grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -(CBike::ms_fMotionControlPitchMax + m_fPitchZeroValue), fDebugDrawWidth/2.0f, Color32(0,255,0));
			grcDebugDraw::MeterValue(PITCH_AXIS_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -(CBike::ms_fMotionControlPitchMin + m_fPitchZeroValue), fDebugDrawWidth/2.0f, Color32(0,255,0));
		}
	}

	if(sbDebugCombat)
	{
		grcDebugDraw::AddDebugOutput("[CombatGesture] Reload detected: %i",m_bReloadTriggered);
		grcDebugDraw::AddDebugOutput("[CombatGesture] Jump left detected: %i",m_bJumpLeftTriggered);
		grcDebugDraw::AddDebugOutput("[CombatGesture] Jump right detected: %i",m_bJumpRightTriggered);
		grcDebugDraw::AddDebugOutput("[CombatGesture] ReloadY stage: %i",m_ReloadTriggerY.GetCurrentStage());
		grcDebugDraw::AddDebugOutput("[CombatGesture] ReloadY last value: %.2f",m_ReloadTriggerY.GetCurrentValue());
		grcDebugDraw::AddDebugOutput("[CombatGesture] ReloatY current time: %.2f", m_ReloadTriggerY.GetTimeOnCurrentStage());
		grcDebugDraw::AddDebugOutput("[CombatGesture] ReloadZ stage: %i",m_ReloadTriggerZ.GetCurrentStage());
		grcDebugDraw::AddDebugOutput("[CombatGesture] ReloadZ last value: %.2f",m_ReloadTriggerZ.GetCurrentValue());
		grcDebugDraw::AddDebugOutput("[CombatGesture] ReloadZ current time: %.2f", m_ReloadTriggerZ.GetTimeOnCurrentStage());

		// Visualise the Y reload trigger
		grcDebugDraw::Meter(RELOAD1_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert,fDebugDrawWidth,Color32(255,255,255),"Y");
		grcDebugDraw::MeterValue(RELOAD1_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -m_ReloadTriggerY.GetCurrentValue(),fDebugDrawWidth,Color32(255,0,0));
		for(int i = 0; i < m_ReloadTriggerY.GetTotalNoStages(); i++)
		{
			const bool bCurrentTrigger = m_ReloadTriggerY.GetCurrentStage() == i;
			grcDebugDraw::MeterValue(RELOAD1_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -m_ReloadTriggerY.GetTriggerValue(i),fDebugDrawWidth/2.0f,bCurrentTrigger ? Color32(0,255,0) : Color32(0,0,255));
		}
		
		// Visualise the Z reload trigger
		grcDebugDraw::Meter(RELOAD2_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert,fDebugDrawWidth,Color32(255,255,255),"Z");
		grcDebugDraw::MeterValue(RELOAD2_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -m_ReloadTriggerZ.GetCurrentValue(),fDebugDrawWidth,Color32(255,0,0));
		for(int i = 0; i < m_ReloadTriggerZ.GetTotalNoStages(); i++)
		{
			const bool bCurrentTrigger = m_ReloadTriggerZ.GetCurrentStage() == i;
			grcDebugDraw::MeterValue(RELOAD2_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -m_ReloadTriggerZ.GetTriggerValue(i),fDebugDrawWidth/2.0f,bCurrentTrigger ? Color32(0,255,0) : Color32(0,0,255));
		}
	}

#if __PPU
	if(sbDebugSensors)
	{
		float fX = ((float)(pPad->GetSensorX() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY; 
		float fY = ((float)(pPad->GetSensorY() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY; 
		float fZ = ((float)(pPad->GetSensorZ() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ACCEL_SENSITIVTY; 
		float fG = ((float)(pPad->GetSensorG() - MOTION_SENSOR_ZERO_COUNT)) / (float)MOTION_SENSOR_ANG_VEL_SENSITIVITY; 

		grcDebugDraw::Meter(SENSORX_POSITION,Vector2(1.0f,0.0f),fDebugAxisLength,fDebugDrawWidth,Color32(255,255,255),"Sensor X");
		grcDebugDraw::MeterValue(SENSORX_POSITION, Vector2(1.0f,0.0f), fDebugAxisLength, fX , fDebugDrawWidth, Color32(255,0,0));
		grcDebugDraw::Meter(SENSORY_POSITION,Vector2(0.0f,1.0f),fDebugAxisLengthVert,fDebugDrawWidth,Color32(255,255,255),"Sensor Y");
		grcDebugDraw::MeterValue(SENSORY_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -fY , fDebugDrawWidth, Color32(255,0,0));
		grcDebugDraw::Meter(SENSORZ_POSITION,Vector2(0.0f,1.0f),fDebugAxisLengthVert,fDebugDrawWidth,Color32(255,255,255),"Sensor Z");
		grcDebugDraw::MeterValue(SENSORZ_POSITION, Vector2(0.0f,1.0f), fDebugAxisLengthVert, -fZ , fDebugDrawWidth, Color32(255,0,0));

		grcDebugDraw::Meter(SENSORG_POSITION,Vector2(1.0f,0.0f),fDebugAxisLength,fDebugDrawWidth,Color32(255,255,255),"Sensor G");
		grcDebugDraw::MeterValue(SENSORG_POSITION, Vector2(1.0f,0.0f), fDebugAxisLength, fG , fDebugDrawWidth, Color32(255,0,0));
	}
#endif
}

#endif	// __BANK

float CPadGesture::GetScaledValueByRange( const float fValue, const float fMinIn, const float fMaxIn, const float fMinOut /*= -1.0f*/, const float fMaxOut /*= 1.0f*/, const bool bClamp )
{
	float fReturn = fValue;
	// Rescale the raw pitch input to custom range
	fReturn -= fMinIn;
	fReturn /= (fMaxIn - fMinIn);
	fReturn *= fMaxOut - fMinOut;
	fReturn += fMinOut;
	if(bClamp)
	{
		fReturn = rage::Clamp(fReturn,fMinOut,fMaxOut);
	}
	return fReturn;
}

void CPadGesture::CalibratePitchFromCurrentValue(const float fMinZeroPoint, const float fMaxZeroPoint)
{
	const float fZeroPoint = GetPadPitch();
	m_fPitchZeroValue = rage::Clamp(fZeroPoint,fMinZeroPoint,fMaxZeroPoint);
}

///////////////////// CGestureTrigger ////////////////////

CPadGesture::CGestureTrigger::CGestureTrigger()
{
	m_fStartPosition = 0.0f;
	m_fCurrentValue = 0.0f;
	m_fTimeOnCurrentStage = 0.0f;
	m_iCurrentStage = 0;
	m_iTotalStages = 1;

	for(int i = 0; i < MAX_NUMBER_TRIGGER_STAGES; i++)
	{
		m_fTriggers[i] = 0.0f;
		if(i< (MAX_NUMBER_TRIGGER_STAGES - 1))
		{
			m_fTimeouts[i] = 0.0f;
		}
	}
}

bool CPadGesture::CGestureTrigger::Update(float fValue, float fTimeStep)
{

	sysCriticalSection lock(m_Cs);
	Assertf(m_iCurrentStage < m_iTotalStages, "%d < %d", m_iCurrentStage, m_iTotalStages);
	Assert(m_iCurrentStage < MAX_NUMBER_TRIGGER_STAGES);

	bool bSuccess = false;

	float fPreviousTrigger = m_iCurrentStage == 0 ? m_fStartPosition : m_fTriggers[m_iCurrentStage-1];

	// Check for timeout
	m_fTimeOnCurrentStage += fTimeStep;
	if(m_iCurrentStage == 0						// Timeouts don't really make sense on first stage
		|| m_fTimeOnCurrentStage < m_fTimeouts[m_iCurrentStage-1] )		
	{
		// Have we crossed a trigger point?
		if((fValue > m_fTriggers[m_iCurrentStage] && fPreviousTrigger < m_fTriggers[m_iCurrentStage])
		 ||(fValue < m_fTriggers[m_iCurrentStage] && fPreviousTrigger > m_fTriggers[m_iCurrentStage]))
		{
			m_iCurrentStage++;
			m_fTimeOnCurrentStage = 0.0f;
			
			// Have we finished all stages?
			if(m_iCurrentStage == m_iTotalStages)
			{
				Reset();
				bSuccess = true;
			}		
		}
	}
	else
	{
		// We've timed out
		Reset();
	}

	m_fCurrentValue = fValue;
	return bSuccess;
}

void CPadGesture::CGestureTrigger::Reset()
{
	m_iCurrentStage = 0;
	m_fTimeOnCurrentStage = 0.0f;
}

void CPadGesture::CGestureTrigger::SetTimeout(int iStage, float fTimeout)
{
	Assertf(iStage < MAX_NUMBER_TRIGGER_STAGES-1, "CGestureTrigger::SetTimeout: Invalid trigger number!");
	
	if(iStage < MAX_NUMBER_TRIGGER_STAGES-1)
	{
		m_fTimeouts[iStage] = fTimeout;
	}
}

void CPadGesture::CGestureTrigger::SetTrigger(int iTriggerNo, float fValue)
{
	Assertf(iTriggerNo < MAX_NUMBER_TRIGGER_STAGES, "CGestureTrigger::SetTrigger: Invalid trigger number!");

	if(iTriggerNo < MAX_NUMBER_TRIGGER_STAGES)
	{
		m_fTriggers[iTriggerNo] = fValue;
	}	
}

void CPadGesture::CGestureTrigger::SetTotalNumStages(int iTotalStages)
{
	sysCriticalSection lock(m_Cs);
	Assertf(iTotalStages <= MAX_NUMBER_TRIGGER_STAGES, "CGestureTrigger::SetTotalNumStages: Invalid trigger number!");

	m_iTotalStages = iTotalStages;
	Reset();
}


#endif // USE_SIXAXIS_GESTURES
