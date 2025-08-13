// Title	:	Transmission.h
// Author	:	Bill Henderson
// Started	:	10/12/1999


#ifndef INC_TRANSMISSION_H_
#define INC_TRANSMISSION_H_

#include "debug/debug.h"
#include "scene/RegdRefTypes.h"
#include "weapons/weaponenums.h"

class CEntity;
class CVehicle;
class CHandlingData;

namespace rage
{
#if __BANK
	class bkBank;
#endif
}

#define MAX_NUM_GEARS (10)

#define ENGINE_HEALTH_MAX						    (1000.0f)
#define ENGINE_DAMAGE_PLANE_DAMAGE_START		    (900.0f)
#define ENGINE_DAMAGE_RADBURST					    (400.0f)
#define ENGINE_DAMAGE_OIL_LEAKING				    (200.0f)
#define ENGINE_DAMAGE_ONFIRE					    (0.0f)
#define ENGINE_DAMAGE_FIRE_FULL					    (-700.0f)
#define ENGINE_DAMAGE_FX_FADE					    (-3200.0f)
#define ENGINE_DAMAGE_FIRE_FINISH				    (-3600.0f)
#define ENGINE_DAMAGE_FINSHED					    (-4000.0f)
#define ENGINE_DAMAGE_CAP_BY_MELEE					(100.0f)

#define ENGINE_DAMAGE_OIL_LEAK_RATE				    (0.025f) 
#define ENGINE_DAMAGE_OIL_FRACTION_BEFORE_DAMAGE	(0.25f) 
#define ENGINE_DAMAGE_OIL_LOW                   	(2.0f) 

#define ENGINE_DAMAGE_MISS_FIRE_BRAKING_FORCE      	(0.8f) 

class CTransmission
{
public:

#if GTA_REPLAY
	friend class CPacketVehicleUpdate;
#endif

	// misc flags for tracking engine behaviour
	enum eTransFlags
	{
		CHANGE_UP_TIME		    = BIT(0),
		CHANGE_DOWN_TIME	    = BIT(1),
		THROTTLE_HELD		    = BIT(2),
        SELECT_GEAR_FOR_SPEED   = BIT(3),  // Used to select an appropriate gear just after a car recording has stopped. Reset after one call to process gears
	    SELECTING_REVERSE       = BIT(4),  // Used for bicycles to stop them selecting reverse too quickly
		AUDIO_SUGGEST_CHANGE_UP = BIT(5),  // Suggest a slightly earlier up-shift if it would sound better (eg. audio revs have maxed out)
		ENGINE_UNDER_LOAD		= BIT(6)   // Makes the shift up later and therefore rev higher for longer
    };

	CTransmission();
	~CTransmission();

    void SetupTransmission(float fDriveForce, float fMaxFlatVel, float fMaxVel, u8 nNumGears, CVehicle* pParentVehicle);
	
	float Process(CVehicle* pParentVehicle, Mat34V_In vehicleMatrix, Vec3V_In vehicleVelocity, float aDriveWheelSpeedAverage, Vector3::Vector3Param vDriveWheelGroundVelocityAverage, int nNumDriveWheels, int nNumDriveWheelsOnGround, float fTimeStep);
	float ProcessBoat(CVehicle* pParentVehicle, float fDriveInWater, float fTimeStep);
    float ProcessSubmarine(CVehicle* pParentVehicle, float fDriveInWater, float fTimeStep);

	void ProcessGears(CVehicle* pParentVehicle, bool bAllDriveWheelsOnGround, float fSpeedFromWheels, float fSpeedFromVehicle, float fTimeStep);
	float ProcessEngine(CVehicle* pParentVehicle, Mat34V_In vehicleMatrix, Vec3V_In vehicleVelocity, int nNumDriveWheels, float fSpeedForRevs, float fSpeedFromVehicle, float fTimeStep);
	
	void ProcessTurbo(CVehicle *pParentVehicle, float &fDriveForce, float &fThrottle, float &fThrottleAbs, float fTimeStep);
	void ProcessNitrous(CVehicle *pParentVehicle, float &fDriveForce, float fThrottle, float fTimeStep);
	void ProcessKERS(CVehicle *pParentVehicle, float &fDriveForce, float fThrottle, float fFwdSpeed, float fTimeStep);

	void SelectGearForSpeed(float fSpeed);
	bool IsCurrentGearATopGear() const;
	float ProcessEngineCvt(CVehicle* pParentVehicle, int nNumDriveWheels, float fSpeedForRevs, float fSpeedFromVehicle, float fTimeStep);

	void Reset();

    void SelectAppropriateGearForSpeed();
	void AudioSuggestChangeUp(bool changeUp);
	void SetEngineUnderLoad(bool underLoad);
	bool IsEngineUnderLoad() const { return (m_nFlags & ENGINE_UNDER_LOAD) != 0; }
	bool IsNitrousFullyCharged( CVehicle *pParentVehicle );
	void ClearNitrousDuration() { m_fRemainingNitrousDuration = 0.0f; }
	void FullyChargeNitrous(CVehicle *pParentVehicle);

#if __BANK
	void DrawTransmissionStats(CVehicle* pParentVehicle, float fDriveForce, float fSpeedForRevs, int nNumDriveWheels);
	static void AddWidgets(bkBank* pBank);
#endif

	int GetGear() const {return (int)m_nGear;}
    float GetGearRatio(int nGear) const {Assert(nGear <= MAX_NUM_GEARS); return m_aGearRatios[nGear];}
	float GetRevRatio() const	{return m_fRevRatio;}
	float GetClutchRatio() const {return m_fClutchRatio;}
	float GetThrottle() const {return m_fThrottle;}

	static float GetTurboPowerModifier(CVehicle* pVehicle);
	static float GetMaxTurboPowerModifier(CHandlingData* pHandling);

	inline float GetMaxVelInGear() const
	{
		return m_fDriveMaxVel / m_aGearRatios[m_nGear];
	}

	void ForceThrottle(f32 throttle);

    u8 GetNumGears() const {return m_nDriveGears;}
    float GetDriveForce() const {return m_fDriveForce;}
    float GetDriveMaxFlatVelocity() const {return m_fDriveMaxFlatVel;}
    float GetDriveMaxVelocity() const {return m_fDriveMaxVel;}
	float GetManifoldPressure() const {return m_fManifoldPressure;}
	void SetManifoldPressure(float pressure) { m_fManifoldPressure = pressure; }

    void SetDriveForce(float fDriveForce) { m_fDriveForce = fDriveForce; }

	float GetEngineHealth() const{return m_fEngineHealth;} 
	void SetEngineHealth(float fHealth) {Assert(fHealth >= ENGINE_DAMAGE_FINSHED); m_fEngineHealth = fHealth;}
	void RestartEngineHealth() {if(m_fEngineHealth < 100.0f) m_fEngineHealth = 100.0f;}
	void ResetEngineHealth() {m_fEngineHealth = ENGINE_HEALTH_MAX;}
	void ResetEngineHealthIfItsOverTheMaximum() { if (m_fEngineHealth > ENGINE_HEALTH_MAX) { m_fEngineHealth = ENGINE_HEALTH_MAX; } }
	void RefillKERS() { m_fRemainingKERS = ms_KERSDurationMax; }
	void ApplyEngineDamage(CVehicle* pParent, CEntity* pInflictor, eDamageType nDamageType, float fDamage, bool bForceFire=false);
	bool GetEngineOnFire() const {return m_fEngineHealth < ENGINE_DAMAGE_ONFIRE && m_fEngineHealth > ENGINE_DAMAGE_FINSHED;}
	void ProcessEngineFire(CVehicle* pParent, float fTimeStep);

    void SetMissFireTime(float fMissFireTime, float fOverrideMissFireTime){ m_fMissFireTime = fMissFireTime; m_fOverrideMissFireTime = fOverrideMissFireTime; }
	float GetOverrideMissFireTime() { return m_fOverrideMissFireTime; }
    bool GetCurrentlyMissFiring() const { return m_fMissFireTime > 0.0f; }
    bool GetCurrentlyRecoveringFromMissFiring() const { return m_fOverrideMissFireTime > 0.0f; }
    // damage pre render effects

	void SetFireEvo(float fireEvo)
	{
		m_fFireEvo = fireEvo;
	}

	f32 GetFireEvo() const
	{
		return m_fFireEvo;
	}

	CEntity* GetEntityThatSetUsOnFire() { return m_pEntityThatSetUsOnFire; }
	void ClearFireCulprit() { m_pEntityThatSetUsOnFire = NULL; }

	float GetKERSRemaining() const { return m_fRemainingKERS; }

	void DisableKERSEffect();

    // helper function to get rev ratio from speed and gear
    inline float CalcRevRatio(float fSpeedForRevs, int nGear)
    {
        return fSpeedForRevs * (m_aGearRatios[nGear] / m_fDriveMaxVel);
    }

	void SetKERSAllowed( CVehicle *pVehicle, bool bAllowed );
	const bool GetKERSAllowed() const { return m_bKERSAllowed; }

	// Determine if the transmission changed gear during processing.
	bool DidChangeGear() { return m_nGear != m_nPrevGear; }

	void SetPlaneDamageThresholdOverride(float val) {m_fPlaneDamageThresholdOverride=val;}
	float GetPlaneDamageThresholdOverride() {return m_fPlaneDamageThresholdOverride;}

	// static functions
	static float GetEngineHealthMax() {return ENGINE_HEALTH_MAX;}

	float GetLastBoatDriveForce() { return m_fBoatDriveForce; }
	void SetLastBoatDriveForce(float fNewValue) { m_fBoatDriveForce = fNewValue; }

	void SetEngineDamageScale( float engineDamageScale ) { m_fEngineHealthDamageScale = engineDamageScale; }
	float GetEngineDamageScale() { return m_fEngineHealthDamageScale; }

	float GetLastCalculatedDriveForce() { return m_fLastCalculatedDriveForce; }

	float GetRemainingNitrousDuration() const { return m_fRemainingNitrousDuration; }

private:	
	s16 m_nGear;
	s16 m_nPrevGear;

	s16 m_nFlags;

    u8 m_nDriveGears;
	bool m_bKERSAllowed;
    float m_aGearRatios[MAX_NUM_GEARS + 1];
    float m_fDriveForce;
    float m_fDriveMaxFlatVel;
    float m_fDriveMaxVel;
	
	float m_fRevRatio;
	float m_fRevRatioOld;
	float m_fForceMultOld;
	float m_fClutchRatio;
	float m_fThrottle;

	float m_fThrottleBlipAmount;
	float m_fThrottleBlipTimer;
	u32 m_fThrottleBlipStartTime;
	u32 m_fThrottleBlipDuration;

    float m_fManifoldPressure;// amount of vacuum or boost on the engine

	u32 m_nGearChangeTime;
	u32 m_UnderLoadChangeUpDelay;

    float m_fMissFireTime;// When a car is running out of petrol make it miss fire
    float m_fOverrideMissFireTime;// Time after miss fire that the car runs fine

	float m_fEngineHealth;
	float m_fEngineHealthDamageScale;
	float m_fFireEvo;
	RegdEnt m_pEntityThatSetUsOnFire;
	float m_fForceThrottle;
	float m_fRemainingNitrousDuration;
	float m_fRemainingKERS;

    void CalculateGearRatios(float fMaxVel, int nNumGears, CVehicle* pParentVehicle);

	float m_fPlaneDamageThresholdOverride;// Overrides ENGINE_DAMAGE_PLANE_DAMAGE_START

	float m_fBoatDriveForce;
	float m_fLastCalculatedDriveForce;

public:
	static float ms_fIdleRevs;
	static float ms_fSmoothedIdleRevs;
    static float ms_TurboPowerModifier;
	static float ms_NitrousPowerModifier;
	static float ms_NitrousDurationMax;
	static float ms_NitrousRechargeRate;

	static float ms_KERSPowerModifier;
	static float ms_KERSBrakeRechargeMax;
	static float ms_KERSEngineBrakeRechargeMultiplier;
	static float ms_KERSRechargeMax;
	static float ms_KERSDurationMax;
	static float ms_KERSRechargeRate;

	static atHashString ms_lectroKERSScreenEffect;
	static atHashString ms_lectroKERSOutScreenEffect;
};


#endif //CTRANSMISSION
