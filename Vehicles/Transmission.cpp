// Title	:	Transmission.cpp
// Author	:	Bill Henderson
// Started	:	10/12/1999


//rage headers
#include "grcore/debugdraw.h"
#include "math/vecMath.h"

//game headers
#include "control/record.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "peds/ped.h"
#include "renderer/water.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/Planes.h"
#include "vehicles/transmission.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"
#include "crskeleton/skeleton.h"
#include "vehicles/handlingMgr.h"
#include "Audio/caraudioentity.h"
#include "system/controlMgr.h"
#include "Frontend/NewHud.h"

#include "network/NetworkInterface.h"
#include "network/General/NetworkUtil.h"
#include "network/Objects/Entities/NetObjPhysical.h"

#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"

#if __BANK
#include "peds/ped.h"
#include "grprofile/timebars.h"
#include "timecycle/TimeCycle.h"
#endif
//#if __BANK
//	#include "text\text.h"
//#endif

VEHICLE_OPTIMISATIONS()



dev_float sfTransFirstGearRatio = 0.3f;
static dev_float sfLowSpeedTorqueIncreaseFirstGearRatio = 0.5f;
static dev_float sfCloseRatioGearboxFirstGearRatio = 0.5f;
dev_float sfTransReverseGearRatio = -0.3f;
dev_s32 snThrottleBlipDurationMin = 400;
dev_s32 snThrottleBlipDurationMax = 500;

atHashString CTransmission::ms_lectroKERSScreenEffect("LectroKERS", 0x4020f9f7);
atHashString CTransmission::ms_lectroKERSOutScreenEffect("LectroKERSOut", 0xF53D5701);

CTransmission::CTransmission()
{
	m_nGear = 0;
	m_nPrevGear = 0;
	m_nFlags = 0;

    m_nDriveGears = 0;
    for(int i=0; i<MAX_NUM_GEARS + 1; i++)
    	m_aGearRatios[i] = 0.0f;

    m_fDriveForce = 0.0f;
    m_fDriveMaxFlatVel = 0.0f;
    m_fDriveMaxVel = 0.0f;
	m_fForceThrottle = -1.0f;

	m_fRevRatio = 0.0f;
	m_fRevRatioOld = 0.0f;
	m_fForceMultOld = 0.0f;
	m_fClutchRatio = 1.0f;


	m_fThrottle = 0.0f;
	m_fThrottleBlipAmount = 0.0f;
	m_fThrottleBlipTimer = 0.0f;
	m_fThrottleBlipStartTime = 0u;
	m_fThrottleBlipDuration = 0u;

    m_fManifoldPressure = 0.0f;

	m_nGearChangeTime = 0;
	m_UnderLoadChangeUpDelay = 0;
    m_fMissFireTime = 0.0f;
    m_fOverrideMissFireTime = 0.0f;
	m_fFireEvo = 0.0f;

	m_fEngineHealth = ENGINE_HEALTH_MAX;
	m_fEngineHealthDamageScale = 1.0f;
	m_pEntityThatSetUsOnFire = NULL;

	m_fRemainingNitrousDuration = ms_NitrousDurationMax;
	m_fRemainingKERS = ms_KERSDurationMax;

	m_fPlaneDamageThresholdOverride = 0.0f;

	m_bKERSAllowed = true;

	m_fBoatDriveForce = 0.0f;
	m_fLastCalculatedDriveForce = 0.0f;
}

CTransmission::~CTransmission()
{
}

float GetNonExpandedModTurboPowerModifier(const CVehicle* pVehicle, float fTurboPowerModifier)
{
	const CVehicleVariationInstance& variationInstance = pVehicle->GetVariationInstance();
	if (variationInstance.IsToggleModOn(VMT_TURBO))
	{
		static atHashString feltzer3Hash = ATSTRINGHASH("feltzer3", 0xA29D6D10);
		static atHashString vigero2Hash = ATSTRINGHASH("vigero2", 0x973141FC);

		if (pVehicle->GetModelNameHash() == feltzer3Hash || pVehicle->GetModelNameHash() == vigero2Hash)
		{
			u8 idx = variationInstance.GetModIndex(VMT_KNOB);
			if (idx != INVALID_MOD)
			{
				// NOTE: VMT_KNOB is visual mod, there are no stats for it from what I understand, so we have to fake the UI here, also give some extra boost to the turbo
				// It is possible old vehicle to be turned in HSW mod friendly, so we have to grow the list above as needed
				// idx is the actual slot index for the turbo, stored in char, i.e. '0', '1', '2' ... etc to '9'. We don't expect more than 9 turbo upgrade slots
				Assertf( idx >= '0' && idx <= '9', "ModIndex: %d is out of range (0-9)", idx );
				float modIndexF = float(idx - '0');
				if( modIndexF < 0.0f || modIndexF > 9.0f )
				{
					return fTurboPowerModifier;
				}
				static float fakeMulti = 40.0f;
				float turboModifier = modIndexF / fakeMulti;
				return fTurboPowerModifier * (1.0f + turboModifier);
			}
		}
	}
	return fTurboPowerModifier;
}

// NOTE: pVehicle should be const here, but we don't have a GetCarHandlingData() that's const so it's easier to just leave it non-const...
float CTransmission::GetTurboPowerModifier(CVehicle* pVehicle)
{
	if (pVehicle && pVehicle->pHandling && pVehicle->pHandling->GetCarHandlingData())
	{
		// Gate this behind a new flag
		if (pVehicle->HasExpandedMods())
		{
			for (int i = 0; i < pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData.GetCount(); i++)
			{
				int advancedDataModSlot = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Slot;

				// NOTE: Knob is the mod slot for the expanded turbo
				if (advancedDataModSlot == VMT_KNOB)
				{
					bool variation = false;
					int modIndex = (int)pVehicle->GetVariationInstance().GetModIndexForType((eVehicleModType)advancedDataModSlot, pVehicle, variation);

					if (modIndex == pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Index)
					{
						return pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Value;
					}
				}
			}
		}
#if RSG_GEN9
		else
		{
			return GetNonExpandedModTurboPowerModifier(pVehicle, ms_TurboPowerModifier);
		}
#endif //RSG_GEN9
	}

	return ms_TurboPowerModifier;
}

// NOTE: pHandling should be const here, but we don't have a GetCarHandlingData() that's const so it's easier to just leave it non-const...
float CTransmission::GetMaxTurboPowerModifier(CHandlingData* pHandling)
{
	float fMaxTurboModifier = ms_TurboPowerModifier;
	if (pHandling && pHandling->GetCarHandlingData())
	{
		// Gate this behind a new flag
		if (pHandling->GetCarHandlingData()->aFlags & CF_ALLOWS_EXTENDED_MODS)
		{
			for (int i = 0; i < pHandling->GetCarHandlingData()->m_AdvancedData.GetCount(); i++)
			{
				int advancedDataModSlot = pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Slot;

				if (advancedDataModSlot == VMT_KNOB)
				{
					const float fTurboPowerModifierForSlot = pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Value;
					if (fTurboPowerModifierForSlot > fMaxTurboModifier)
					{
						fMaxTurboModifier = fTurboPowerModifierForSlot;
					}
				}
			}
		}
	}

	return fMaxTurboModifier;
}

//
// this should be called to setup gear ratios and change drive force, max speed etc.
//
void CTransmission::SetupTransmission(float fDriveForce, float fMaxFlatVel, float fMaxVel, u8 nNumGears, CVehicle* pParentVehicle)
{    
#if __ASSERT
	Assert(pParentVehicle);
	if (MI_CAR_CYCLONE2.IsValid() && (pParentVehicle->GetModelIndex() == MI_CAR_CYCLONE2.GetModelIndex()) )
	{
		CHandlingData* pHandling = pParentVehicle->pHandling;
		Assertf(nNumGears == 1, "%s: CVT vehicles should be set up with only 1 gear", pHandling->m_handlingName.TryGetCStr());
	}
#endif
	if(nNumGears <= MAX_NUM_GEARS)
    {
        m_nDriveGears = nNumGears;
    }
    else
    {
        m_nDriveGears = MAX_NUM_GEARS;
    }

    m_fDriveForce = fDriveForce;
    m_fDriveMaxFlatVel = fMaxFlatVel;
    m_fDriveMaxVel = fMaxVel;

    CalculateGearRatios( m_fDriveMaxVel, nNumGears, pParentVehicle );

	m_fRemainingNitrousDuration = ms_NitrousDurationMax * pParentVehicle->GetNitrousDurationModifier();
}

//
// this should get called every frame, even when the vehicle is asleep
//
float CTransmission::Process(CVehicle* pParentVehicle, Mat34V_In vehicleMatrix, Vec3V_In vehicleVelocity, float fDriveWheelSpeedAverage, Vector3::Vector3Param vDriveWheelGroundVelocityAverage, int nNumDriveWheels, int nNumDriveWheelsOnGround, float fTimeStep)
{
	m_fRevRatioOld = m_fRevRatio;

	// All the users are checking for this already
	Assert(pParentVehicle->m_nVehicleFlags.bEngineOn);

	// Calculate speed coming from vehicle and coming from wheels.
	float fSpeedFromWheels = fDriveWheelSpeedAverage;
	Vec3V vRelativeVelocity = Subtract(vehicleVelocity, RCC_VEC3V(vDriveWheelGroundVelocityAverage));
	float fSpeedAlongHeading = Dot(vehicleMatrix.GetCol1(), vRelativeVelocity).Getf();

	//if(fSpeedFromWheels > 0.0f) fSpeedFromVehicle = rage::Max(0.0f, fSpeedFromVehicle);	
	//else fSpeedFromVehicle = rage::Min(0.0f, fSpeedFromVehicle);
	// Selectf below matches above.  It selects on -fSpeedFromWheels to handle fSpeedFromWheels==0, but I don't know if that case really matters.  Left the match exact just in case.
	// * 08/10/12 - Now changed round so that locked wheels (fSpeedFromWheels == 0.0f) don't give a zero vehicle velocity
	float fSpeedFromVehicle = Selectf(fSpeedFromWheels,rage::Max(0.0f, fSpeedAlongHeading), rage::Min(0.0f, fSpeedAlongHeading));	
	bool bAllDriveWheelsOnGround = nNumDriveWheels==nNumDriveWheelsOnGround;

	// During cutscenes, the wheel information won't be valid so we need to rely on the vehicle speed
	if(pParentVehicle->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE)
	{
		fSpeedFromWheels = fSpeedFromVehicle;
		bAllDriveWheelsOnGround = true;
	}

	// change gear if necessary.
	ProcessGears(pParentVehicle, bAllDriveWheelsOnGround, fSpeedFromWheels, fSpeedFromVehicle, fTimeStep);

	// get force from engine
	float fDriveForce = 0.0f;
	if(pParentVehicle->pHandling->hFlags & HF_CVT)
	{
		if( !pParentVehicle->pHandling->GetSpecialFlightHandlingData() ||
			!( pParentVehicle->pHandling->GetSpecialFlightHandlingData()->m_flags & SF_FORCE_SPECIAL_FLIGHT_WHEN_DRIVEN ) )
		{
			fDriveForce = ProcessEngineCvt( pParentVehicle, nNumDriveWheelsOnGround, fSpeedFromWheels, fSpeedFromVehicle, fTimeStep );
		}
		else
		{
			fDriveForce = ProcessEngineCvt( pParentVehicle, nNumDriveWheelsOnGround, fSpeedFromVehicle, fSpeedFromVehicle, fTimeStep );
		}
	}
	else
	{	
		fDriveForce = ProcessEngine(pParentVehicle, vehicleMatrix, vRelativeVelocity, nNumDriveWheelsOnGround, fSpeedFromWheels, fSpeedFromVehicle, fTimeStep);//Changed this to use nNumDriveWheelsOnGround, so cars are less likely to get stuck, when ontop of objects lifting their wheels up.
	}
	
    //Process whether we are misfiring or not
	if(m_fMissFireTime > 0.0f)
	{
		m_fMissFireTime = rage::Max(0.0f,m_fMissFireTime-fTimeStep);
		float fDriveForceGearedAbs = m_fDriveForce * rage::Abs(m_aGearRatios[m_nGear]);
		fDriveForce = Selectf(fSpeedFromWheels,-fDriveForceGearedAbs,fDriveForceGearedAbs);
	}
	else if(m_fOverrideMissFireTime)
	{
		m_fOverrideMissFireTime = rage::Max(0.0f,m_fOverrideMissFireTime-fTimeStep);
	}

#if __BANK
	if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
        && (pParentVehicle->GetDriver() && pParentVehicle->GetDriver()->IsLocalPlayer()))
	{
		DrawTransmissionStats(pParentVehicle, fDriveForce, fSpeedFromWheels, nNumDriveWheels);
	}
#endif

	if(pParentVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = (CPlane *)pParentVehicle;
		fDriveForce *= pPlane->GetAircraftDamage().GetThrustMult(pPlane);
	}

	m_fLastCalculatedDriveForce = fDriveForce;

	return fDriveForce;
}


float CTransmission::ProcessBoat(CVehicle* pParentVehicle, float fDriveInWater, float fTimeStep)
{
	m_fRevRatioOld = m_fRevRatio;
	m_nPrevGear = m_nGear;

	CHandlingData* pHandling = pParentVehicle->pHandling;

	float fThrottle = 0.0f;

	if(!pParentVehicle->InheritsFromAmphibiousAutomobile())
	{
		fThrottle = pParentVehicle->GetThrottle();
	}
	else
	{
		// Simulate boat braking on amphibious vehicle's boat transmission
		CAmphibiousAutomobile* pCar = static_cast<CAmphibiousAutomobile*>(pParentVehicle);

		fThrottle = pCar->GetBoatEngineThrottle();
	}
	
	m_fThrottle = fThrottle;

	// If the engine hasn't started yet the vehicle doesn't move.
	if (!pParentVehicle->m_nVehicleFlags.bEngineOn)
	{
		m_fRevRatio = 0.0f;
		m_nGear = 1;
		m_fClutchRatio = 0.0f;
		return 0.0f;
	}

	if((fThrottle > 0.0f && m_nGear != 1 && m_fRevRatio < 0.1f) || pHandling->hFlags & HF_NO_REVERSE)
		m_nGear = 1;
	else if(fThrottle < 0.0f && m_nGear != 0 && m_fRevRatio < 0.1f)
		m_nGear = 0;

	float fEngineAccelResistance = (0.5f + 0.5f*(1.0f - fDriveInWater)) / rage::Max(1.0f, pHandling->m_fDriveInertia);

	if((fThrottle < 0.0f && m_nGear > 0) || (fThrottle > 0.0f && m_nGear == 0))
	{
        // We want the boats to be able to change between forward and reverse really quickly, so speed up the change in revratio with sfEngineNeutralAccelResistance
        static dev_float sfEngineNeutralAccelResistance = 4.0f;
		m_fRevRatio -= rage::Abs(fThrottle)*sfEngineNeutralAccelResistance*fTimeStep;
		return 0.0f;
	}

	float fSpeedForRevs = DotProduct(VEC3V_TO_VECTOR3(pParentVehicle->GetTransform().GetB()), pParentVehicle->GetVelocity());
	float fDesiredRevs = fSpeedForRevs * (m_aGearRatios[m_nGear] / m_fDriveMaxVel);

	// desired revs increases with speed, from base flat out revs at rest
	fDesiredRevs = 0.5f + 0.5f * Clamp(fDesiredRevs, 0.0f, 1.0f);

	fDesiredRevs *= rage::Abs(fThrottle);

	// idle revs
	if(fDesiredRevs < ms_fIdleRevs)
		fDesiredRevs = ms_fIdleRevs;

	if(m_fRevRatio < fDesiredRevs)
	{
		m_fRevRatio += rage::Abs(fThrottle) * fEngineAccelResistance * fTimeStep;
		if(m_fRevRatio > fDesiredRevs)
			m_fRevRatio = fDesiredRevs;
	}
	else if(m_fRevRatio > fDesiredRevs)
	{
		m_fRevRatio -= fEngineAccelResistance * fTimeStep;
		if(m_fRevRatio < fDesiredRevs)
			m_fRevRatio = fDesiredRevs;
	}

	float fDriveForce = fThrottle * (m_fRevRatio / fDesiredRevs) * m_fDriveForce;

	if(m_nGear==0 && fSpeedForRevs < 0.0f)// reduce engine force in reverse when we are actually going backwards
	{
		if( m_fDriveForce < 0.2f &&
			pParentVehicle->InheritsFromAmphibiousAutomobile() )
		{
			fDriveForce *= 0.6f;
		}
		else
		{
			fDriveForce *= 0.2f;
		}
	}

	if(pParentVehicle->m_nVehicleFlags.bScriptSetHandbrake)
		fDriveForce = 0.0f;

	fDriveForce *= fDriveInWater;

#if __BANK
	if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
	&& pParentVehicle->GetStatus()==STATUS_PLAYER)
	{
		DrawTransmissionStats(pParentVehicle, fDriveForce, m_fRevRatio, 1);
	}
#endif

	m_fBoatDriveForce = fDriveForce;

	fDriveForce *= pParentVehicle->pHandling->GetBoatHandlingData()->m_fTransmissionMultiplier;

	return fDriveForce;
}

float CTransmission::ProcessSubmarine(CVehicle* pParentVehicle, float fDriveInWater, float fTimeStep)
{
    m_fRevRatioOld = m_fRevRatio;
	m_nPrevGear = m_nGear;

    CHandlingData* pHandling = pParentVehicle->pHandling;

    float fThrottle = m_fThrottle = pParentVehicle->GetThrottle();
    // If the engine hasn't started yet the vehicle doesn't move.
    if (!pParentVehicle->m_nVehicleFlags.bEngineOn)
    {
        m_fRevRatio = 0.0f;
        m_nGear = 1;
        m_fClutchRatio = 0.0f;
        return 0.0f;
    }


    static dev_float sfMinimumRevsToChangeDriveDirection = 0.1f;

    if((fThrottle > 0.0f && m_nGear != 1 && m_fRevRatio < sfMinimumRevsToChangeDriveDirection) || pHandling->hFlags & HF_NO_REVERSE)
        m_nGear = 1;
    else if(fThrottle < 0.0f && m_nGear != 0 && m_fRevRatio < sfMinimumRevsToChangeDriveDirection)
        m_nGear = 0;

    float fEngineAccelResistance = (0.5f + 0.5f*(1.0f - fDriveInWater)) / rage::Max(1.0f, pHandling->m_fDriveInertia);

    // If we're trying to change direction of drive get the revs back down before changing gear.
    if((fThrottle < 0.0f && m_nGear > 0) || (fThrottle > 0.0f && m_nGear == 0))
    {
        static dev_float sfEngineResistanceModifier = 2.0f;// This is to allow the prop to spin down faster than it spins up.
        m_fRevRatio -= rage::Abs(fThrottle)* (fEngineAccelResistance * sfEngineResistanceModifier) *fTimeStep;
        return 0.0f;
    }

    float fDesiredRevs = rage::Abs(fThrottle);

    // idle revs
    if(fDesiredRevs < ms_fIdleRevs)
        fDesiredRevs = ms_fIdleRevs;

    if(m_fRevRatio < fDesiredRevs)
    {
        m_fRevRatio += rage::Abs(fThrottle) * fEngineAccelResistance * fTimeStep;
        if(m_fRevRatio > fDesiredRevs)
            m_fRevRatio = fDesiredRevs;
    }
    else if(m_fRevRatio > fDesiredRevs)
    {
        m_fRevRatio -= fEngineAccelResistance * fTimeStep;
        if(m_fRevRatio < fDesiredRevs)
            m_fRevRatio = fDesiredRevs;
    }

    float fDriveForce = fThrottle * (m_fRevRatio / fDesiredRevs) * m_fDriveForce;

    if(m_nGear==0)
        fDriveForce *= 0.5f;

    fDriveForce *= fDriveInWater;

#if __BANK
    if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
        && pParentVehicle->GetStatus()==STATUS_PLAYER)
    {
        DrawTransmissionStats(pParentVehicle, fDriveForce, m_fRevRatio, 1);
    }
#endif

	if( pParentVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		return fDriveForce * 11.0f;
	}
	else
	{
		return fDriveForce;
	}
}


float CTransmission::ms_fIdleRevs = 0.2f;
float CTransmission::ms_fSmoothedIdleRevs = 0.1f;
dev_float sfTransIdleRevLimit = 0.2f;
dev_float sfTransFwd2RevLimit = 0.5f;

dev_float sfTransChangeUpRatio[MAX_NUM_GEARS + 1] = 
{
	0.94f,
	0.82f,
	0.97f,
	0.95f,
	0.94f,
	0.94f,
	0.94f,
	0.94f,
    0.94f,
    0.94f,
};

dev_float sfTransChangeDownRatio[MAX_NUM_GEARS + 1] = 
{
	0.75f,
	0.63f,
	0.79f,
	0.74f,
	0.77f,
	0.75f,
	0.75f,
	0.75f,
    0.75f,
    0.75f,
    0.75f,
};

dev_float sfTransCruiseToStopChangeDownRatio[MAX_NUM_GEARS + 1] = 
{
	1.1f,
	0.7f,
	0.85f,
	1.0f,
	1.1f,
	1.1f,
	1.1f,
	1.1f,
    1.1f,
    1.1f,
    1.1f,
};

dev_float sfTransChangeHeavyBrakingChangeDownRatio[MAX_NUM_GEARS + 1] = 
{
	2.0f,
	1.1f,
	1.3f,
	1.7f,
	2.0f,
	2.0f,
	2.0f,
	2.0f,
    2.0f,
    2.0f,
    2.0f,
};

dev_float sfTransChangeThrottleContrib = 0.15f;
dev_float sfTransChangeBrakeContrib = 0.9f;
dev_u32 snTransChangeUpDelayTime = 2000;
dev_u32 snTransChangeDownDelayTime = 3000;
dev_u32 snTransCruiseChangeDownHoldTime = 2000;
dev_u32 snTransHeavyBrakeChangeDownHoldTime = 2000;

dev_u32 snBicycleTransChangeToReverseDelayTime = 500;

dev_float sfTransChangeDisableThrottleWhenClutchDown = 0.60f;

dev_u32 sfTransUnderLoadChangeUpDelay = 2000;
dev_u32 sfTransUnderLoadChangeUpGearDiff = 1000;
dev_float sfTransIdleClutch = 0.1f;
dev_float sfTransChangeClutch = 0.1f;

dev_float sfTransClutchIn1stRevRatio = 0.5f;

dev_float sfTransRevsChangeTimeFactor = 1.f;

dev_float sfCvtLowGearRato = 5.0f;
dev_float sfCvtReverseLowGearRato = -5.0f;
dev_float sfCvtReverseMaxSpeedMult = 0.5f;
dev_float sfCvtReverseMaxSpeedMultBicycle = 0.05f;

void CTransmission::SelectGearForSpeed(float fSpeed)
{
	m_nGear = 1; // Default to 1st gear
	m_fClutchRatio = 1.0f; // Make sure the clutch is completely up so we can start putting power down straight away
	//don't attempt to put into reverse, if reverse is needed the code below will sort it out
	//PH start from the top and work down. pick the first gear that wont immediately shift down due to the lower gears ratio
	for(s16 i = m_nDriveGears; i > 1; i--)
	{
		//just pick the first gear that can cope with this speed
		if(CalcRevRatio(fSpeed, i-1) > sfTransChangeDownRatio[i-1])
		{
			m_nGear = i;
			return;
		}
	}
}

bool CTransmission::IsCurrentGearATopGear() const
{
	return (m_nGear == m_nDriveGears);
}

void CTransmission::ProcessGears(CVehicle* pParentVehicle, bool bAllDriveWheelsOnGround, float fSpeedFromWheels, float fSpeedFromVehicle, float fTimeStep)
{
	m_nPrevGear = m_nGear;

	CHandlingData* pHandling = pParentVehicle->pHandling;
	float fThrottle = m_fForceThrottle >= 0.0f? m_fForceThrottle : pParentVehicle->GetThrottle();
	float fBrakeAbs = rage::Abs(pParentVehicle->GetBrake());

	if(pParentVehicle->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE && rage::Abs(fSpeedFromVehicle) > 1.0f)
	{
		fThrottle = 1.0f;
		fBrakeAbs = 0.0f;
	}

	float fThrottleAbs = rage::Abs(fThrottle);
	u32 nGameTimeMs = fwTimer::GetTimeInMilliseconds();

	float fThrottleChangeMult = rage::Min(1.0f, fThrottleAbs + sfTransChangeBrakeContrib*fBrakeAbs);
	fThrottleChangeMult = sfTransChangeThrottleContrib*fThrottleChangeMult + (1.0f-sfTransChangeThrottleContrib);
	bool inWheelieMode = pParentVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>( pParentVehicle )->m_nAutomobileFlags.bInWheelieMode;

	bool bUseHardRevLimitFlag = CVehicle::UseHardRevLimitFlag( pHandling, pParentVehicle->IsTunerVehicle(), IsCurrentGearATopGear() );
	if( ( inWheelieMode || bUseHardRevLimitFlag ) 
		&& !pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive 
		)
	{
		// if we have a hard rev limit we need to change up slightly sooner or we can end up stuck bouncing against the rev limit and not changing up
		static dev_float hardRevLimitThottleChangeModifier = 0.875f;
		fThrottleChangeMult *= hardRevLimitThottleChangeModifier;
	}

	if(fThrottleAbs < 0.7f)
	{
		m_nFlags &= ~THROTTLE_HELD;
	}

	// use actual forward speed rather than the speed of the wheels to determine when to change gear
	//if(fSpeedFromWheels > 0.0f) fSpeedForAccel = rage::Min(fSpeedFromWheels, fSpeedFromVehicle);
	//else fSpeedForAccel = rage::Max(fSpeedFromWheels, fSpeedFromVehicle);
	// Selectf below matches the above test.  It selets on -fSpeedFromWheels to match the case when fSpeedFromWheels==0, but I don't know if that case really matters.  Matched it exactly for now.
	float fSpeedForAccel = 0.0f;
	fSpeedForAccel = Selectf( -fSpeedFromWheels, rage::Max( fSpeedFromWheels, fSpeedFromVehicle ), rage::Min( fSpeedFromWheels, fSpeedFromVehicle ) );

	// use actual forward speed rather than the speed of the wheels to determine when to change gear
	//if(fSpeedFromWheels > 0.0f) fSpeedForDeccel = rage::Max(fSpeedFromWheels, fSpeedFromVehicle);
	//else fSpeedForDeccel = rage::Min(fSpeedFromWheels, fSpeedFromVehicle);
	// Selectf below matches the above test.  It selets on -fSpeedFromWheels to match the case when fSpeedFromWheels==0, but I don't know if that case really matters.  Matched it exactly for now.
	float fSpeedForDeccel = Selectf(-fSpeedFromWheels,rage::Min(fSpeedFromWheels, fSpeedFromVehicle),rage::Max(fSpeedFromWheels, fSpeedFromVehicle));

	// if clutch was in then let it out again
	if(m_fClutchRatio < 1.0f)
	{
		// two clutch speeds, based on drive inertia for now
		// note: drive inertia is the wrong way round; lower number means slower rev change
		float clutchSpeed = 0.0f;
		
		if(m_nFlags & CHANGE_UP_TIME)
		{
			clutchSpeed = pHandling->m_fClutchChangeRateScaleUpShift;
		}
		else
		{
			clutchSpeed = pHandling->m_fClutchChangeRateScaleDownShift;
		}

		clutchSpeed *= (1.0f + (CVehicle::ms_fGearboxClutchSpeedVarianceMaxModifier * (pParentVehicle->GetVariationInstance().GetGearboxModifier()/100.0f)));

		m_fClutchRatio = rage::Min(m_fClutchRatio+fTimeStep*clutchSpeed,1.0f);
	}

    //if a car recording has just stopped just select the most appropriate gear for the current speed
    if( m_nFlags &SELECT_GEAR_FOR_SPEED)
    {
        m_nFlags &= ~SELECT_GEAR_FOR_SPEED;

		SelectGearForSpeed(fSpeedForAccel);
    }

	// if throttle is off, and speed is low, apply the clutch
	if(fThrottleAbs < 0.01f && fBrakeAbs < 0.01f && CalcRevRatio(fSpeedFromWheels, 1) < sfTransIdleRevLimit && pParentVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE && pParentVehicle->GetOwnedBy()!=ENTITY_OWNEDBY_CUTSCENE)
	{
        if( pHandling->GetCarHandlingData() &&
            pHandling->GetCarHandlingData()->aFlags & CF_GEARBOX_MANUAL )
        {
            m_fClutchRatio = 0.0f;
        }
        else
        {
		    m_fClutchRatio = sfTransIdleClutch;
        }
		//m_nGear = 0;
	}
	// if throttle is negative, want to reverse, but only change into reverse below set rev range
	else if(fThrottle < 0.0f && CalcRevRatio(fSpeedForAccel, 0) > -sfTransFwd2RevLimit)
	{
		if(m_nGear!=0)
		{
            bool bCanChangeDown = false;
            //make sure there is a delay before shifting into reverse on bicycles.
            if(pParentVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
            {
                if( !(m_nFlags & SELECTING_REVERSE) )
                {
                    m_nGearChangeTime = nGameTimeMs;
                    m_nFlags |= SELECTING_REVERSE;
                    
                }
                else if ( nGameTimeMs >= m_nGearChangeTime + snBicycleTransChangeToReverseDelayTime )
                {
                    bCanChangeDown = true;
                }
            }
            else
            {
                bCanChangeDown = true;
            }

            if(bCanChangeDown)
            {
                m_nGear = 0;
                m_fClutchRatio = sfTransChangeClutch;
                m_nFlags &= ~SELECTING_REVERSE;
            }

		}
	}
	// if throttle is positive, but gear is in reverse, want to change into first but only within set rev range (i.e. not when going flat out backwards). For car recording vehicles,
	// ignore this limit as they can potentially spawn in gear 0 at speed, and we don't want them stuck idling. Same goes for remotely controlled network vehicles.
	else if(fThrottle > 0.0f && m_nGear==0 && (CalcRevRatio(fSpeedForAccel, 1) < sfTransFwd2RevLimit || ( CPhysics::ms_bInStuntMode && fSpeedForAccel > 0.0f ) || CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pParentVehicle) || (pParentVehicle->GetDriver() && pParentVehicle->GetDriver()->IsNetworkPlayer())))
	{
		m_nGear = 1;
		m_fClutchRatio = sfTransChangeClutch;
	}
	// Under heavy braking work back down through the gears faster than normal to simulate engine braking. Using fSpeedFromVehicle so that wheels locking up don't affect this.
	else if(pParentVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE && m_nGear > 1 && fBrakeAbs > 0.8f && fThrottleAbs == 0.0f)
	{
		bool bCanChangeDown = !((m_nFlags &CHANGE_DOWN_TIME) && nGameTimeMs < m_nGearChangeTime + snTransHeavyBrakeChangeDownHoldTime);

		if(bCanChangeDown)
		{
			if(CalcRevRatio(fSpeedFromVehicle, m_nGear - 1) < sfTransChangeHeavyBrakingChangeDownRatio[m_nGear - 1])
			{
				f32 initialChangeDownRatio = sfTransChangeHeavyBrakingChangeDownRatio[m_nGear - 1];

				do 
				{
					m_nGear--;
				} while (m_nGear > 1 && CalcRevRatio(fSpeedFromVehicle, m_nGear - 1) < initialChangeDownRatio);

				m_nFlags = CHANGE_DOWN_TIME;
				m_fClutchRatio = sfTransChangeClutch;
				m_nGearChangeTime = nGameTimeMs;
			}
		}
	}
	// When cruising to a stop, do an early downshift (with the option of skipping multiple gears) to make it sound a bit more aggressive
	else if(pParentVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE && m_nGear > 1 && fThrottleAbs == 0.0f && fBrakeAbs == 0.0f && m_fForceThrottle != 0.0f)
	{
		bool bCanChangeDown = !((m_nFlags &CHANGE_DOWN_TIME) && nGameTimeMs < m_nGearChangeTime + snTransCruiseChangeDownHoldTime);
		
		if(bCanChangeDown)
		{
			if(CalcRevRatio(fSpeedForDeccel, m_nGear - 1) < sfTransCruiseToStopChangeDownRatio[m_nGear - 1])
			{
				f32 initialChangeDownRatio = sfTransCruiseToStopChangeDownRatio[m_nGear - 1];

				do
				{
					m_nGear--;
				} while (m_nGear > 1 && CalcRevRatio(fSpeedForDeccel, m_nGear - 1) < initialChangeDownRatio);

				m_nFlags = CHANGE_DOWN_TIME;
				m_fClutchRatio = sfTransChangeClutch;
				m_nGearChangeTime = nGameTimeMs;
			}
		}
	}
	// change up when revs exceed change up ratio (modified for throttle)
	else if(CalcRevRatio(fSpeedForAccel, m_nGear) > sfTransChangeUpRatio[m_nGear]*fThrottleChangeMult || (m_nFlags & AUDIO_SUGGEST_CHANGE_UP && CalcRevRatio(fSpeedForAccel, m_nGear + 1) > sfTransChangeDownRatio[m_nGear + 1]))
	{
		bool bCanChangeUp = !((m_nFlags &CHANGE_DOWN_TIME) && ((m_nFlags &THROTTLE_HELD) || m_UnderLoadChangeUpDelay > 0u) && nGameTimeMs < m_nGearChangeTime + snTransChangeUpDelayTime);

		if(m_nFlags & ENGINE_UNDER_LOAD && m_nGear >= 1 && fThrottleAbs > 0.0f && !pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DONT_HOLD_LOW_GEARS_WHEN_ENGINE_UNDER_LOAD ) )
		{
			bool bUseHardRevLimitFlag = CVehicle::UseHardRevLimitFlag( pHandling, pParentVehicle->IsTunerVehicle(), IsCurrentGearATopGear() );
			if( ( fThrottleAbs == 1.0f || 
				  ( bUseHardRevLimitFlag && !pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive ) 
				) &&
				nGameTimeMs > (m_nGearChangeTime + snTransChangeUpDelayTime) )
			{
				m_UnderLoadChangeUpDelay += fwTimer::GetTimeStepInMilliseconds();
			}

			if(m_UnderLoadChangeUpDelay < sfTransUnderLoadChangeUpDelay - (sfTransUnderLoadChangeUpGearDiff * (m_nGear - 1)))
			{
				bCanChangeUp = false;
			}
		}

		bool bHasHoldGearWheelSpinFlag = CVehicle::HasHoldGearWheelSpinFlag( pHandling, pParentVehicle );
        if( bHasHoldGearWheelSpinFlag
#if __BANK
			&& !CVehicle::ms_bDebugIgnoreHoldGearWithWheelspinFlag
#endif
			)
        {
            if( Abs( fSpeedFromWheels - fSpeedFromVehicle ) > 0.2f * fSpeedFromVehicle )
            {
                bCanChangeUp = false;
            }
        }

		if( pParentVehicle->InheritsFromAutomobile() &&
			static_cast<CAutomobile*>( pParentVehicle )->m_nAutomobileFlags.bInBurnout && 
			m_nGear >= 1 )
		{
			bCanChangeUp = false;
		}

		if(m_nGear < m_nDriveGears && (fThrottle > 0.0f || m_UnderLoadChangeUpDelay > 0u) && bAllDriveWheelsOnGround && bCanChangeUp)
		{
			m_nGear++;
			m_fClutchRatio = sfTransChangeClutch;
			m_UnderLoadChangeUpDelay = 0u;

			m_nFlags = CHANGE_UP_TIME | THROTTLE_HELD;
			m_nGearChangeTime = nGameTimeMs;

			if(pParentVehicle->GetDriver() && pParentVehicle->GetDriver()->IsLocalPlayer())
			{
				float fDriveForce = m_fDriveForce;

				// Take into account the turbo drive force increase.
				if( pParentVehicle->GetVariationInstance().IsToggleModOn(VMT_TURBO) &&
                    !pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_IS_ELECTRIC ) )
				{
					const float fTurboPowerModifier = GetTurboPowerModifier(pParentVehicle);
					fDriveForce *= 1.0f + (fTurboPowerModifier*m_fManifoldPressure);
				}

				float fDriveForceDelta = fDriveForce - pHandling->m_fInitialDriveForce;

				// If the drive force is greater than the initial the engine has been modded
				if(fDriveForceDelta > 0.0f)
				{
					static dev_float sfClutchKickIntensity = 3.0f;
					static dev_float sfClutchKickRumbleDuration = 75.0f;
					static dev_float sfClutchKickGearboxModMaxMult = 0.5f;// the amount the modded gearbox will reduce the rumble duration when fully modded.

					float fClutchKickRumbleDuration = sfClutchKickRumbleDuration;
					fClutchKickRumbleDuration -= (fClutchKickRumbleDuration * (sfClutchKickGearboxModMaxMult * (pParentVehicle->GetVariationInstance().GetGearboxModifier()/100.0f)));// reduce the duration if we have a modded gearbox

					CControlMgr::StartPlayerPadShakeByIntensity(u32(fClutchKickRumbleDuration), fDriveForceDelta * sfClutchKickIntensity);
				}
			}
		}
	}
	// change down when revs are below change down ratio (modified for throttle)
	else if(m_nGear > 1	&& CalcRevRatio(fSpeedForDeccel, m_nGear - 1) < sfTransChangeDownRatio[m_nGear-1]*fThrottleChangeMult)
	{
		bool bCanChangeDown = !((m_nFlags &CHANGE_UP_TIME) && (m_nFlags &THROTTLE_HELD) && (nGameTimeMs < m_nGearChangeTime + snTransChangeDownDelayTime || pParentVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE));

		if(pParentVehicle->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE && fSpeedForDeccel == 0.0f)
		{
			bCanChangeDown = false;
		}

		// must let the clutch out a bit before changing down again
		if(bCanChangeDown)// && m_fClutchRatio > 0.5f)
		{
			m_nGear--;
			m_fClutchRatio = sfTransChangeClutch;

			m_nFlags = CHANGE_DOWN_TIME | THROTTLE_HELD;
			m_nGearChangeTime = nGameTimeMs;
			m_UnderLoadChangeUpDelay = 0u;
		}
	}
	else if(fThrottleAbs == 0.0f)
	{
		m_UnderLoadChangeUpDelay = 0u;
	}

	if (m_nGear == 0 && pHandling->hFlags & HF_NO_REVERSE)
	{
		m_nGear = 1;
	}

/*
	float fRevRatioFromSpeed = CalcRevRatio(fSpeedForRevs, m_nGear, pHandling);
	if(m_fClutchRatio < 0.0f)
	{
		float fSmooth = rage::Powf(TRANS_IDLE_SMOOTH_RATE, fTimeStep);
		m_fRevRatio = fSmooth * m_fRevRatio + (1.0f - fSmooth)*TRANS_IDLE_RATIO;
	}
	else
	{
		m_fRevRatio = m_fClutchRatio * fRevRatioFromSpeed + (1.0f - m_fClutchRatio)*m_fRevRatio;
	}

	if(m_fRevRatio > 1.0f)
		m_fRevRatio = 1.0f;
	else if(m_fRevRatio < TRANS_IDLE_RATIO)
		m_fRevRatio = TRANS_IDLE_RATIO;
*/
}



dev_float POWER_CHEAT_MULT_1 = 1.2f;
dev_float POWER_CHEAT_MULT_2 = 1.5f;

float CTransmission::ms_TurboPowerModifier		= 0.10f;

// params to control nitrous
float CTransmission::ms_NitrousPowerModifier	= 3.0f;
float CTransmission::ms_NitrousDurationMax		= 3.0f;
float CTransmission::ms_NitrousRechargeRate		= 0.35f;

float CTransmission::ms_KERSPowerModifier					= 0.8f;
float CTransmission::ms_KERSBrakeRechargeMax				= 0.75f;//maximum amount of braking force that can be used for recharging KERS.
float CTransmission::ms_KERSEngineBrakeRechargeMultiplier	= 0.5f;
float CTransmission::ms_KERSRechargeMax						= 0.3f;
float CTransmission::ms_KERSDurationMax						= 3.0f;
float CTransmission::ms_KERSRechargeRate					= 1.0f;

dev_float MOTORBIKE_REVERSE_VEL = -2.0f;
dev_float MOTORBIKE_REVERSE_FALLOFF = 1.0f;
dev_float MOTORBIKE_REVERSE_THRUST = -1.0f;

dev_float sfTransRevLimiterBounce = 0.90f;
dev_float sfTransRevLimiterThrottle = 0.90f;
dev_float sfTransRevLimiterAudioThrottle = 0.0f;
dev_float sfTransOverRevThrottle = 0.0f;

dev_float sfTransOverRevMax		= 0.25f;
dev_float sfTransOverRevMaxInv	= 1.0f / sfTransOverRevMax;

dev_float sfMinimumManifoldPressure = -1.0f;
dev_float sfMaximumManifoldPressure = 1.0f;

dev_float sfMaximumManifoldPressureRate = 0.5f;

dev_float sfHydraulicsPowerLoss = 0.6f;
dev_float sfGliderPowerLoss = 0.4f;

dev_float sfShuntModePowerLoss = 0.2f;

//
float CTransmission::ProcessEngine(CVehicle* pParentVehicle, Mat34V_In vehicleMatrix, Vec3V_In vehicleVelocity, int nNumDriveWheels, float fSpeedFromWheels, float fSpeedFromVehicle, float fTimeStep)
{
	CHandlingData* pHandling = pParentVehicle->pHandling;
	float fThrottle = m_fThrottle = m_fForceThrottle >= 0.0f? m_fForceThrottle : pParentVehicle->GetThrottle();
	float fInputThrottleAbs = rage::Abs(fThrottle);
	bool bHandBrake = pParentVehicle->GetHandBrake();
	float fBrakeAbs = rage::Abs(pParentVehicle->GetBrake());

	if(pParentVehicle->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE && rage::Abs(fSpeedFromVehicle) > 1.0f)
	{
		fThrottle = m_fThrottle = 1.0f;
		fBrakeAbs = 0.0f;
		bHandBrake = false;
	}

	float fThrottleAbs = rage::Abs(fThrottle);

	if((fThrottle < 0.0f && m_nGear > 0) || (fThrottle > 0.0f && m_nGear == 0))
	{
		m_fThrottle = fThrottle = 0.0f;
		return 0.0f;
	}
	// if the clutch is down then lift off the throttle (except when in first gear or reverse or applying handBrake)
	else if(!bHandBrake && sfTransChangeDisableThrottleWhenClutchDown >= 0.0f && m_fClutchRatio < sfTransChangeDisableThrottleWhenClutchDown && m_nGear > 1)
	{
		m_fThrottle = fThrottle = fThrottleAbs = 0.0f;
	}
	
	// If we're stationary, the player applies the throttle, trigger the throttle blip behavior. This basically just
	// amplifies and extends the duration of any short throttle presses so that they produce a bigger blip than they would naturally
	float fFwdSpeed = Dot(vehicleMatrix.GetCol1(),vehicleVelocity).Getf();
	if(fFwdSpeed < 1.0f)
	{
		// Once the revs have dropped back down, we reset the blip timer, allowing use to do more blips
		if(m_fRevRatio > 0.4f)
		{
			m_fThrottleBlipTimer += fTimeStep;
		}
		else
		{
			m_fThrottleBlipTimer = 0.0f;
		}

		// Detect the player blipping the throttle, amplify the throttle amount, and remember the time it occurred.
		if(m_fThrottle > 0.5f && m_fThrottle > m_fThrottleBlipAmount && m_fThrottleBlipTimer < 0.4f)
		{
			m_fThrottleBlipAmount = Min(m_fThrottle * 2.0f, 1.0f);
			m_fThrottleBlipStartTime = fwTimer::GetTimeInMilliseconds();
			m_fThrottleBlipDuration = fwRandom::GetRandomNumberInRange(snThrottleBlipDurationMin, snThrottleBlipDurationMax);
		}
	}

	bool throttleBlipActive = false;

	// If we're still within the blip time and the throttle has been released, increase it back up to our blip level
	if(fwTimer::GetTimeInMilliseconds() - m_fThrottleBlipStartTime < m_fThrottleBlipDuration)
	{
		if(m_fThrottle == 0.0f)
		{
			audVehicleAudioEntity* audioEntity = (audVehicleAudioEntity*)pParentVehicle->GetAudioEntity();

			if(audioEntity && audioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				audCarAudioEntity* carAudioEntity = (audCarAudioEntity*)audioEntity;

				if(!carAudioEntity->HasAutoShutOffEngine() || fwTimer::GetTimeInMilliseconds() - carAudioEntity->GetVehicleEngine()->GetAutoShutOffRestartTime() > 500)
				{
					fThrottle = m_fThrottle = fThrottleAbs = Max(m_fThrottle, m_fThrottleBlipAmount);
					throttleBlipActive = true;
				}
			}
		}
	}
	else
	{
		m_fThrottleBlipAmount = 0.0f;
	}

	float fDesiredRevs; 

	// Under heavy braking, calculate revs from vehicle speed to prevent revs suddenly dropping if wheels lock up
	if(fThrottleAbs == 0.0f && fBrakeAbs > 0.0f &&
        ( !pHandling->GetCarHandlingData() || !( pHandling->GetCarHandlingData()->aFlags & CF_GEARBOX_MANUAL ) ) )
	{
		fDesiredRevs = CalcRevRatio(fSpeedFromVehicle, m_nGear);
	}
	// If amphibious automobile is in water with no wheels touching the ground and no throttle, calculate desired revs based on vehicle speed
	else if(fThrottleAbs == 0.0f && pParentVehicle->InheritsFromAmphibiousAutomobile() && pParentVehicle->GetNumContactWheels() == 0 && static_cast<CAmphibiousAutomobile*>(pParentVehicle)->IsPropellerSubmerged())
	{
		fDesiredRevs = CalcRevRatio(fSpeedFromVehicle, m_nGear);
	}
	else
	{
		bool bUseHardRevLimitFlag = CVehicle::UseHardRevLimitFlag( pHandling, pParentVehicle->IsTunerVehicle(), IsCurrentGearATopGear() );		
		if( !pHandling->GetCarHandlingData() || !( bUseHardRevLimitFlag && !pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive ) ||
			!pParentVehicle->InheritsFromAutomobile() || !static_cast< CAutomobile* >( pParentVehicle )->m_nAutomobileFlags.bInBurnout )
		{
			fDesiredRevs = CalcRevRatio( fSpeedFromWheels, m_nGear );
		}
		else
		{
			//if( Abs( fSpeedFromVehicle ) >
			//	Abs( fSpeedFromWheels ) )
			//{
			//	fDesiredRevs = CalcRevRatio( fSpeedFromVehicle, m_nGear );
			//}
			//else
			{
				fDesiredRevs = CalcRevRatio( fSpeedFromWheels, m_nGear );
			}
		}
	}

	float fClutchMult = Clamp(m_fClutchRatio, 0.0f, 1.0f);
	if(throttleBlipActive || (bHandBrake && !(pHandling->hFlags & HF_HANDBRAKE_REARWHEELSTEER)))
	{
		fClutchMult = 0.f;
	}
	fDesiredRevs = fClutchMult * fDesiredRevs + (1.0f - fClutchMult) * fThrottleAbs;

	bool bBikeDriveForceCalculated = false;
	float fDriveForce = 0.0f;
	if(pParentVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE || pParentVehicle->GetVehicleType()==VEHICLE_TYPE_BICYCLE || 
		( pParentVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE && pParentVehicle->GetNumWheels() == 3 && ( pParentVehicle->pHandling->GetBikeHandlingData() || pParentVehicle->GetModelIndex() == MI_CAR_STRYDER ) ) )
	{
		if(fThrottle < 0.0f && m_nGear==0)
		{
			if(fFwdSpeed > MOTORBIKE_REVERSE_VEL)
			{
				fDriveForce = rage::Min(1.0f, MOTORBIKE_REVERSE_FALLOFF*(fFwdSpeed - MOTORBIKE_REVERSE_VEL)) * MOTORBIKE_REVERSE_THRUST;
			}

			bBikeDriveForceCalculated = true;
			fDesiredRevs = 0.0f;
			m_fClutchRatio = 0.0f;

            if( pParentVehicle->GetModelIndex() == MI_CAR_STRYDER )
            {
                static dev_float sfScaleStryderReverseForce = 0.1f;
                fDriveForce *= sfScaleStryderReverseForce;
            }
		}
	}

	// idle revs
	float minRevs = !pParentVehicle->pHandling->GetCarHandlingData() || !( pParentVehicle->pHandling->GetCarHandlingData()->aFlags & CF_FORCE_SMOOTH_RPM ) ? ms_fIdleRevs : ms_fSmoothedIdleRevs;

	fDesiredRevs = rage::Max(fDesiredRevs, minRevs );

	// Update m_fRevRatio
	float fRevRatioChangeRate = pHandling->m_fDriveInertia * sfTransRevsChangeTimeFactor * fTimeStep;

	if( pHandling->GetCarHandlingData() &&
		pHandling->GetCarHandlingData()->aFlags & CF_FORCE_SMOOTH_RPM &&
		m_fRevRatio < fDesiredRevs )
	{
		static float sfSmoothedRpmRate = 0.1f;

		// when going over bumps the rpm can rise too quickly and it drops again when the wheels regain traction, this makes the audio sound bad
		// to try and smooth this reduce the rate at which the rpm can increase based on current rpm and throttle input
		// so at full throttle at high rpm the rpm will only increase slowly.
		fRevRatioChangeRate *= Max( 0.0f, sfSmoothedRpmRate + ( ( 1.0f - ( GetThrottle() * m_fRevRatio ) ) * ( 1.0f - sfSmoothedRpmRate ) ) );
	}

	m_fRevRatio = Selectf(m_fRevRatio-fDesiredRevs,
		rage::Max(m_fRevRatio-fRevRatioChangeRate,fDesiredRevs),
		rage::Min(m_fRevRatio+fRevRatioChangeRate,fDesiredRevs));


	// jump out here for reversing bikes
	if(bBikeDriveForceCalculated)
	{
		ProcessNitrous(pParentVehicle, fDriveForce, fThrottle, fTimeStep);
		return fDriveForce;
	}

//	how about adding a flag that allows temporary overreving. It should still fix the top speed issue but would allow some curb boosting and the drag racing handbrake exploit

	//TODO NITROUS BURST - DO THIS IN HERE RATHER THAN USING ROCKET BOOST SO THAT IT INCREASES DRIVE FORCE CAN PROBABLY RE-USE SOME OF THE PARAMS
	//	WILL NEED TO KILL THE REV LIMIT, OR Raise IT WHEN BOOSTING

	if( m_fRevRatio > 1.0f || (m_fRevRatio >= 1.0f && bHandBrake) )
	{
		bool inWheelieMode = pParentVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>( pParentVehicle )->m_nAutomobileFlags.bInWheelieMode;
		bool bUseHardRevLimitFlag = CVehicle::UseHardRevLimitFlag( pHandling, pParentVehicle->IsTunerVehicle(), IsCurrentGearATopGear() );		// rev limiter
        if( ( inWheelieMode ||
			  ( bUseHardRevLimitFlag && 
			    ( !pParentVehicle->InheritsFromAutomobile() ||
				  !static_cast<CAutomobile*>( pParentVehicle )->m_nAutomobileFlags.bInBurnout ) ) ) &&
			!pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive )
        {
			// to try and make this a bit more consistent across different platforms and configs
			// at 60 fps we drop the rpm to 0 for 1 update so if the frame rate is lower we still get a bit of throttle.
			static dev_float zeroThrottleTime = 1.0f / 60.0f;
			float throttleScale = Max( 0.0f, ( fTimeStep - zeroThrottleTime ) / fTimeStep );
			throttleScale *= Clamp( ( 1.0f - ( ( m_fRevRatio - 1.0f ) * 4.0f ) ), 0.0f, 1.0f );

            fThrottle *= throttleScale;
            m_fThrottle *= throttleScale;

			if (m_nGear == 1)
			{
				// Vehicles with CF_HARD_REV_LIMIT seem to hang around at the top end of the rev range for a while 
				// before switching gears and end up spamming the rev limiter/exhaust pops, which sounds bad.
				// By restricting the limiter to only trigger in first gear we fix the above, but still get a brief limiter
				// hit + pop if you accelerate fast enough in first gear, which is the same as non-CF_HARD_REV_LIMIT vehicles.
				pParentVehicle->GetVehicleAudioEntity()->RevLimiterHit();
			}
        }
		else if((m_nGear > 0 && m_nGear < m_nDriveGears) || bHandBrake)
		{
			// check to see if any of the drive wheels have broken off
			bool bAnyBrokenDriveWheels = false;

			if( pParentVehicle->GetVehicleType() == VEHICLE_TYPE_CAR &&
				pParentVehicle->GetWheelBrokenIndex() != 0 )
			{
				for( int i = 0; i < pParentVehicle->GetNumWheels(); i++ )
				{
					if( pParentVehicle->GetWheelBroken( i ) )
					{
						CWheel* pWheel = pParentVehicle->GetWheel( i );
						if( pWheel->GetIsDriveWheel() )
						{
							bAnyBrokenDriveWheels = true;
							break;
						}
					}
				}
			}

			// GTAV - B*2481474 - Vehicles with broken off wheels can go excessively fast
			// this is because it doesn't allow them to change up through the gears which results
			// in the wheel torque being really high
			// to fix that we scale down the throttle value as the rev limit is exceeded.
			// GTAV - B*2404292 - With the hydraulics it is possible to jump the car off the ground. 
			// This causes the engine to over rev and produce too much power at the wheels
			if( bAnyBrokenDriveWheels ||
				pParentVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics ) 
			{
				m_fRevRatio = sfTransRevLimiterBounce;
 				fThrottle *= sfTransRevLimiterThrottle;
 				m_fThrottle *= sfTransRevLimiterAudioThrottle;
			}
            else
			{
				m_fRevRatio = 1.0f;
			}

			pParentVehicle->GetVehicleAudioEntity()->RevLimiterHit();
		}
		else if(m_nGear == 0) // reverse gear so reduce throttle to 0.0f when hitting the limiter
		{
			m_fRevRatio = 1.0f;
			fThrottle *= sfTransOverRevThrottle;
			m_fThrottle *= sfTransOverRevThrottle;

			pParentVehicle->GetVehicleAudioEntity()->RevLimiterHit();
		}
		else
		{
			//if( pParentVehicle->pHandling->m_fDownforceModifier > 1.0f )
			//{
			//	m_fRevRatio = sfTransRevLimiterBounce;
			//	fThrottle *= sfTransOverRevThrottle;
			//	m_fThrottle *= sfTransRevLimiterAudioThrottle;
			//}
			//else
			{
				m_fRevRatio = 1.0f;
			}
		}
	}
	else
	{
		float fDesiredRevsFromFwdSpeed = CalcRevRatio(fSpeedFromVehicle, m_nGear);
		fThrottle = Selectf(fDesiredRevsFromFwdSpeed-1.0f,sfTransOverRevThrottle,fThrottle);
	}

	fThrottleAbs = rage::Abs(fThrottle);

	// if we're in 1st or reverse, use the clutch to stop the revs dropping too low
	if(m_nGear < 2 && m_fRevRatio < sfTransClutchIn1stRevRatio)
	{
		float minRevs = !pParentVehicle->pHandling->GetCarHandlingData() || !( pParentVehicle->pHandling->GetCarHandlingData()->aFlags & CF_FORCE_SMOOTH_RPM ) ? ms_fIdleRevs : ms_fSmoothedIdleRevs;

		if( !pHandling->GetCarHandlingData() ||
			!( pHandling->GetCarHandlingData()->aFlags & CF_FORCE_SMOOTH_RPM ) ||
			m_fRevRatio < ms_fIdleRevs )
		{
			m_fClutchRatio = rage::Min( m_fClutchRatio, rage::Max( minRevs, ( m_fRevRatio / sfTransClutchIn1stRevRatio ) ) );
		}
	}

	fDriveForce = fThrottleAbs * m_fDriveForce;

	// Get the secondary drive force (currently set up only for boats) and make sure the total drive force does not go ridiculously high
	if(pParentVehicle->GetSecondTransmission())
	{
		float fSecondDriveForce = pParentVehicle->GetSecondTransmission()->GetLastBoatDriveForce();
		float fFirstDriveForce = fDriveForce * Sign(GetThrottle());

		static dev_float sfSecondDriveForceReductionMult = 0.95f;

		if(GetThrottle() > 0.0f)
		{
			fSecondDriveForce *= sfSecondDriveForceReductionMult;
		}

		if(Sign(fFirstDriveForce - fSecondDriveForce) != Sign(fFirstDriveForce))
		{
			fDriveForce = 0.0f;
		}
		else if(Sign( fFirstDriveForce ) == Sign(fSecondDriveForce))
		{
			fDriveForce -= fSecondDriveForce * Sign(GetThrottle());
		}
	}

	fDriveForce = rage::Min(1.0f, 2.0f*fClutchMult) * fDriveForce * m_aGearRatios[m_nGear];

	if( !pParentVehicle->InheritsFromAutomobile() ||
		!static_cast<CAutomobile*>( pParentVehicle )->m_nAutomobileFlags.bInWheelieMode )
	{
		fDriveForce *= ( m_fRevRatio / fDesiredRevs );
	}

    if( Abs( m_fRevRatio ) > 0.1f &&
        m_nGear > 0 &&
        pHandling->GetCarHandlingData() )
    {
        fDriveForce -= Clamp( m_fDriveForce * m_fRevRatio * pHandling->GetCarHandlingData()->m_fEngineResistance * ( 1.0f - fThrottleAbs ) * fClutchMult, -Abs(fDriveForce), Abs(fDriveForce) );
    }

    //reduce power at peak revs
    static dev_float sfPeakPowerAtRevs = 0.8f;
    static dev_float sfPeakPowerDrop = 0.4f;
    if(m_fRevRatio > sfPeakPowerAtRevs && m_nGear != 1) //don't limit power in first
    {
        fDriveForce *= 1.0f - (((m_fRevRatio - sfPeakPowerAtRevs)/(1.0f - sfPeakPowerAtRevs))*sfPeakPowerDrop);
    }

	fDriveForce /= float(Max(nNumDriveWheels, 1));


	ProcessTurbo(pParentVehicle, fDriveForce, fThrottle, fThrottleAbs, fTimeStep);

	ProcessNitrous(pParentVehicle, fDriveForce, fThrottle, fTimeStep);

	ProcessKERS(pParentVehicle, fDriveForce, fInputThrottleAbs, fFwdSpeed, fTimeStep);

	// If the vehicle has modified hydraulics reduce the drive force to simulate the loss 
	// caused by powering the pumps
	if( pParentVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics )
	{
		fDriveForce *= sfHydraulicsPowerLoss;
	}
	if( pParentVehicle->HasGlider() )
	{
		float deployedRatio = pParentVehicle->GetGliderNormaliseDeployedRatio();
		fDriveForce -= fDriveForce * deployedRatio * sfGliderPowerLoss;
	}
	if( pParentVehicle->IsShuntModifierActive() )
	{
		fDriveForce -= fDriveForce * sfShuntModePowerLoss;
	}

	TUNE_GROUP_FLOAT( VEHICLE_TRANSMISSION, LOW_SPEED_TORQUE_INCREASE_THRESHOLD, 15.0f, 0.0f, 50.0f, 0.1f );
	TUNE_GROUP_FLOAT( VEHICLE_TRANSMISSION, LOW_SPEED_TORQUE_INCREASE, 3.0f, 0.0f, 10.0f, 0.1f );
	fFwdSpeed = Abs( fFwdSpeed );

	if( ( pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_INCREASE_LOW_SPEED_TORQUE ) &&
		  fFwdSpeed < LOW_SPEED_TORQUE_INCREASE_THRESHOLD &&
		  m_nGear > 0 ) ||
		( pParentVehicle->m_pGroundPhysical &&
		  pParentVehicle->m_pGroundPhysical->GetModelNameHash() == MI_TRAILER_TRAILERLARGE.GetName().GetHash() ) )
	{
		float torqueIncrease = 1.0f - ( fFwdSpeed / LOW_SPEED_TORQUE_INCREASE_THRESHOLD );
		torqueIncrease *= LOW_SPEED_TORQUE_INCREASE;
		fDriveForce += fDriveForce * torqueIncrease;
	}

	if( pParentVehicle->InheritsFromAutomobile() &&
		static_cast< CAutomobile* >( pParentVehicle )->m_nAutomobileFlags.bInWheelieMode )
	{
		static dev_float sfWheelieModeTorqueIncrease = 1.5f;
		fDriveForce += fDriveForce * sfWheelieModeTorqueIncrease;
	}

	// Apply any cheat increase in power.
	if(pParentVehicle->GetCheatFlag(VEH_CHEAT_POWER2|VEH_SET_POWER2|VEH_CHEAT_POWER1|VEH_SET_POWER1))
	{
		if(pParentVehicle->GetCheatFlag(VEH_CHEAT_POWER2|VEH_SET_POWER2))
			fDriveForce *= POWER_CHEAT_MULT_2;
		else if(pParentVehicle->GetCheatFlag(VEH_CHEAT_POWER1|VEH_SET_POWER1))
			fDriveForce *= POWER_CHEAT_MULT_1;
	}
	fDriveForce *= pParentVehicle->GetCheatPowerIncrease();

	return fDriveForce;
}

static dev_float sfKersRumbleIntensityMin = 0.0f;
#if __PS3
static dev_float sfKersRumbleIntensityMax = 0.9f;
#else
static dev_float sfKersRumbleIntensityMax = 0.09f;
#endif
static dev_float sfKersRumblePOW = 3.0f;
static dev_u32 sKersRumbleDuration = 1;

//
void CTransmission::ProcessKERS(CVehicle *pParentVehicle, float &fDriveForce, float fThrottle, float fFwdSpeed, float fTimeStep)
{
	bool bKERSActive = pParentVehicle->m_nVehicleFlags.bIsKERSBoostActive;
	pParentVehicle->m_nVehicleFlags.bIsKERSBoostActive = false;

	bool bKERS	= pParentVehicle->GetKERSActive();
	if( m_bKERSAllowed &&
		pParentVehicle->pHandling->hFlags & HF_HAS_KERS && NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInSpectatorMode())// Only supposed to be used in MP
	{
		audVehicleKersSystem::audKersState audioKersState = audVehicleKersSystem::AUD_KERS_STATE_OFF;
		f32 kersRechargeRate = 0.0f;
		bool triggerBoostActivate = false;

		CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
		const bool bFollowPlayerInThisVehicle	= pFollowPlayer ? (pFollowPlayer->GetIsInVehicle() && (pFollowPlayer->GetMyVehicle() == pParentVehicle)) : false;
		const bool bFollowPlayerInAnyVehicle	= pFollowPlayer ? (pFollowPlayer->GetIsInVehicle()) : false;
		
		// if the KERS is active and we're at full throttle and we have 
		// KERS remaining then apply power boost
		if( bKERS &&
			fThrottle >= 0.9f &&
			m_fRemainingKERS > 0.0f )
		{			
			audioKersState = audVehicleKersSystem::AUD_KERS_STATE_BOOSTING;

			// Trigger some additional SFX when boosting from a full charge
			if(m_fRemainingKERS >= ms_KERSDurationMax)
			{
				triggerBoostActivate = true;				
			}

			m_fRemainingKERS -= fTimeStep;

			if( m_fRemainingKERS <= 0.0f )
			{
				m_fRemainingKERS = 0.0f;			
				pParentVehicle->SetKERS( false );
			}
			else
			{
				fDriveForce += GetDriveForce() * ms_KERSPowerModifier;
				pParentVehicle->m_nVehicleFlags.bIsKERSBoostActive = true;				

				if(bFollowPlayerInThisVehicle)
				{
					float fKersRumbleIntensity = ( ( ms_KERSDurationMax - m_fRemainingKERS ) / ms_KERSDurationMax );
					fKersRumbleIntensity *= fKersRumbleIntensity;
					fKersRumbleIntensity = rage::Powf( fKersRumbleIntensity, sfKersRumblePOW ) * sfKersRumbleIntensityMax;
					fKersRumbleIntensity += sfKersRumbleIntensityMin;

#if __PS3
					// On PS3 CControlMgr::StartPlayerPadShakeByIntensity( sKersRumbleDuration, fKersRumbleIntensity ) doesn't allow
					// us to just spin the small motor which, for this effect, is all we want.
					if( fKersRumbleIntensity > 0.0f )
					{
						static dev_float fMinPadShakeFreq1 = 1.0f;
						static dev_float fMaxPadShakeFreq1 = 255.0f;
						u32 iFreq = (u32)( fMinPadShakeFreq1 + ( ( fMaxPadShakeFreq1 - fMinPadShakeFreq1 )* fKersRumbleIntensity ) );

						CControlMgr::StartPlayerPadShake( 0, 0, sKersRumbleDuration, iFreq, 0, CONTROL_MAIN_PLAYER );
					}
#else 
					CControlMgr::StartPlayerPadShakeByIntensity( sKersRumbleDuration, fKersRumbleIntensity );
#endif
				}
			}
		}
		else if(fThrottle == 0.0f && !pParentVehicle->IsInAir())
		{
			float fInitialRemainingKers = m_fRemainingKERS;
			// recharge the KERS system
			if( m_fRemainingKERS < ms_KERSDurationMax )
			{
				if(fFwdSpeed >= 1.0f)
				{
					float fMaxRechargeForce = GetDriveForce() * ms_KERSRechargeMax; // How much recharge do we want
					float fMaxRechargeForceWithOutEngineBraking = fMaxRechargeForce * (1.0f - ms_KERSEngineBrakeRechargeMultiplier);

					float fCurrentMaxRechargeBrakeForce = pParentVehicle->GetBrake() * pParentVehicle->GetBrakeForce() * ms_KERSBrakeRechargeMax; // how much recharge are the brakes potentially making at the moment

					float fBrakeForceRechargeDelta = fCurrentMaxRechargeBrakeForce - fMaxRechargeForceWithOutEngineBraking; // how much brake force have we got left?

					if(fBrakeForceRechargeDelta > 0.0f)// Current braking generates more force than we need so reduce braking by the amount we need to just produce max recharge
					{
						float fBrakeForceFractionRequired = (fCurrentMaxRechargeBrakeForce - fBrakeForceRechargeDelta) / fCurrentMaxRechargeBrakeForce;
						pParentVehicle->SetBrake(pParentVehicle->GetBrake() - (pParentVehicle->GetBrake() * ms_KERSBrakeRechargeMax * fBrakeForceFractionRequired) ); // reduce braking by amount required
						fBrakeForceRechargeDelta = fMaxRechargeForceWithOutEngineBraking;
					}
					else
					{
						pParentVehicle->SetBrake(pParentVehicle->GetBrake() * (1.0f - ms_KERSBrakeRechargeMax));
						fBrakeForceRechargeDelta = fMaxRechargeForceWithOutEngineBraking + fBrakeForceRechargeDelta;
					}

					float fRechargeAmount = (fMaxRechargeForce - fMaxRechargeForceWithOutEngineBraking) + fBrakeForceRechargeDelta; //engine braking

					fDriveForce -= fRechargeAmount;

					m_fRemainingKERS = Clamp( m_fRemainingKERS + ( fTimeStep * ms_KERSRechargeRate * (1.0f - ((fMaxRechargeForce - fRechargeAmount) / fMaxRechargeForce))), 0.0f, ms_KERSDurationMax );
					kersRechargeRate = ((m_fRemainingKERS - fInitialRemainingKers)/CTransmission::ms_KERSDurationMax) * fwTimer::GetInvTimeStep();
					audioKersState = audVehicleKersSystem::AUD_KERS_STATE_RECHARGING;
				}				
			}
		}

		//Kers ability in the special ability bar.
		if(bFollowPlayerInThisVehicle)
		{	
			CNewHud::SetAbilityOverride((GetKERSRemaining()/ms_KERSDurationMax)*100.0f, 100.0f); // Leaving this until the UI is setup in mp
		}

		if(pParentVehicle->GetVehicleAudioEntity())
		{
			audVehicleKersSystem* vehicleEngineAudio = pParentVehicle->GetVehicleAudioEntity()->GetVehicleKersSystem();

			if(vehicleEngineAudio)
			{
				if(triggerBoostActivate)
				{
					vehicleEngineAudio->TriggerKersBoostActivate();
				}

				vehicleEngineAudio->SetKersRechargeRate(kersRechargeRate);
				vehicleEngineAudio->SetKERSState(audioKersState);
			}
		}

		if(bFollowPlayerInThisVehicle)
		{
			if( pParentVehicle->m_nVehicleFlags.bIsKERSBoostActive )
			{
				if(ANIMPOSTFXMGR.IsRunning(ms_lectroKERSOutScreenEffect))
				{
					ANIMPOSTFXMGR.Stop(ms_lectroKERSOutScreenEffect,AnimPostFXManager::kKERSBoost);
				}

				if(!ANIMPOSTFXMGR.IsRunning(ms_lectroKERSScreenEffect))
				{
					ANIMPOSTFXMGR.Start(ms_lectroKERSScreenEffect, 0U, false, false, false, 0U, AnimPostFXManager::kKERSBoost);
				}
			}
			else
			{
				if(ANIMPOSTFXMGR.IsRunning(ms_lectroKERSScreenEffect))
				{
					ANIMPOSTFXMGR.Stop(ms_lectroKERSScreenEffect,AnimPostFXManager::kKERSBoost);
				}
				else if( ANIMPOSTFXMGR.IsStartPending(ms_lectroKERSScreenEffect) )
				{
					ANIMPOSTFXMGR.CancelStartRequest(ms_lectroKERSScreenEffect,AnimPostFXManager::kKERSBoost);
				}

				pParentVehicle->SetKERS( false );

				if( bKERSActive )
				{
					ANIMPOSTFXMGR.Start(ms_lectroKERSOutScreenEffect, 0U, false, false, false, 0U, AnimPostFXManager::kKERSBoost);
				}
			}
		}
		else
		{
			if(!bFollowPlayerInAnyVehicle)
			{
				// B*2379100: disable KERS postfx when ped no longer in any vehicle:
				if(ANIMPOSTFXMGR.IsRunning(ms_lectroKERSScreenEffect))
				{
					ANIMPOSTFXMGR.Stop(ms_lectroKERSScreenEffect,AnimPostFXManager::kKERSBoost);
				}
				else if( ANIMPOSTFXMGR.IsStartPending(ms_lectroKERSScreenEffect) )
				{
					ANIMPOSTFXMGR.CancelStartRequest(ms_lectroKERSScreenEffect,AnimPostFXManager::kKERSBoost);
				}
			}
		}
	}
}

void CTransmission::DisableKERSEffect()
{
	if( ANIMPOSTFXMGR.IsRunning(ms_lectroKERSScreenEffect) )
	{
		ANIMPOSTFXMGR.Stop(ms_lectroKERSScreenEffect,AnimPostFXManager::kDefault);
	}
	else if( ANIMPOSTFXMGR.IsStartPending(ms_lectroKERSScreenEffect) )
	{
		ANIMPOSTFXMGR.CancelStartRequest(ms_lectroKERSScreenEffect,AnimPostFXManager::kDefault);
	}
}

void CTransmission::SetKERSAllowed( CVehicle *pVehicle, bool bAllowed ) 
{ 
	if( !bAllowed )
	{
		pVehicle->SetKERS( false );

		CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
		bool bFollowPlayerInThisVehicle = pFollowPlayer ? ( pFollowPlayer->GetIsInVehicle() && ( pFollowPlayer->GetMyVehicle() == pVehicle ) ) : false;

		if( bFollowPlayerInThisVehicle )
		{
			DisableKERSEffect();
			if(m_bKERSAllowed && !pFollowPlayer->GetSpecialAbility())
			{
				CNewHud::SetToggleAbilityBar(false);
				uiDebugf3("CPed::SetPedOutOfVehicle::SetToggleAbilityBar false");

			}
		}
	}

	m_bKERSAllowed = bAllowed; 
}

//
void CTransmission::ProcessNitrous(CVehicle *pParentVehicle, float &fDriveForce, float UNUSED_PARAM( fThrottle ), float fTimeStep)
{	
	pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive = false;

	if( pParentVehicle->GetOverrideNitrousLevel() ||
		pParentVehicle->GetVariationInstance().IsToggleModOn( VMT_NITROUS ) ||
		pParentVehicle->HasNitrousBoost() ||
		pParentVehicle->HasSideShunt() )
	{		
		bool bNitrous		= pParentVehicle->GetNitrousActive();
		float maxDuration	= ms_NitrousDurationMax * pParentVehicle->GetNitrousDurationModifier();
		
		if( bNitrous )
		{
			// nitrous is active
			
			m_fRemainingNitrousDuration -= fTimeStep;
			
			if( m_fRemainingNitrousDuration <= 0.0f )
			{
				// no charge, set to inactive
				m_fRemainingNitrousDuration = 0.0f;
				pParentVehicle->SetNitrous( false );
				pParentVehicle->GetVehicleAudioEntity()->SetBoostActive( false );
				pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive = false;
				pParentVehicle->SetNitrousActive( false );
			}
			else
			{
				// apply drive force boost
				fDriveForce *= 1.0f + ( ms_NitrousPowerModifier * pParentVehicle->GetNitrousPowerModifier() );
				pParentVehicle->m_nVehicleFlags.bIsNitrousBoostActive = true;
				if ( !pParentVehicle->GetOverrideNitrousSound() )
				{
					pParentVehicle->GetVehicleAudioEntity()->SetBoostActive(true);
				}
			}
		}
		else
		{
			// nitrous is inactive
			if( m_fRemainingNitrousDuration < maxDuration )
			{
				// recharge
				float nitrousRechargeRateScale = pParentVehicle->GetNitrousRechargeModifier();
				float rechargeRate = ( fwTimer::GetTimeStep() * ms_NitrousRechargeRate * nitrousRechargeRateScale );
				if( pParentVehicle->HasSideShunt() )
				{
					static dev_float sfShuntRechargeRateScale = 4.0f;
					rechargeRate *= sfShuntRechargeRateScale;
				}
				m_fRemainingNitrousDuration = Clamp( m_fRemainingNitrousDuration + rechargeRate, 0.0f, maxDuration );
			}
		}

		// If the driver is in the vehicle, update the special ability bar
		if( pParentVehicle->GetDriver() && 
			pParentVehicle->GetDriver()->IsLocalPlayer() )
		{
			CNewHud::SetAbilityOverride( Min( 1.0f, ( m_fRemainingNitrousDuration / maxDuration ) ) * 100.0f, 100.0f );
		}
	}
}

bool CTransmission::IsNitrousFullyCharged( CVehicle *pParentVehicle )
{
	float maxDuration = ms_NitrousDurationMax * pParentVehicle->GetNitrousDurationModifier();
	return m_fRemainingNitrousDuration >= maxDuration;
}

void CTransmission::FullyChargeNitrous(CVehicle *pParentVehicle)
{
	m_fRemainingNitrousDuration = ms_NitrousDurationMax * pParentVehicle->GetNitrousDurationModifier();
	vehicleDebugf1("CTransmission::FullyChargeNitrous: Remaining nitrous: %f", m_fRemainingNitrousDuration);
}
//
void CTransmission::ProcessTurbo(CVehicle *pParentVehicle, float &fDriveForce, float &fThrottle, float &fThrottleAbs, float fTimeStep)
{
	//if we've got a turbo, model manifold pressure

	// The amount of power the turbo makes is based on the current manifold pressure. 
	// When off the throttle the engine produces a vacuum, when on the throttle this 
	// vacuum reduces, when on full throttle the turbo starts producing boost, as manifold 
	// pressure goes up engine power goes up. I've also reduced power slightly when the manifold 
	// pressure is still in vacuum this gives the engine a signature 80's laggy turbo feeling. 
    if( pParentVehicle->GetVariationInstance().IsToggleModOn( VMT_TURBO ) &&
        !pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_IS_ELECTRIC ) )
	{
		//if we're near idle we need a little throttle to stop the car from stalling, this obviously effects the vacuum
		if(m_fRevRatio < ms_fIdleRevs+0.05f)
		{
			fThrottle = fThrottleAbs = 0.1f;
		}

		float desiredManifoldPressure = -1.0f + fThrottle;
		if(m_fManifoldPressure > desiredManifoldPressure && fThrottle < 1.0f)
		{
			m_fManifoldPressure -= sfMaximumManifoldPressureRate * fTimeStep;
			if(m_fManifoldPressure < sfMinimumManifoldPressure)
			{
				m_fManifoldPressure = sfMinimumManifoldPressure;
			}
		}
		else// if we're full throttle start boosting
		{
			m_fManifoldPressure += sfMaximumManifoldPressureRate * fTimeStep;

			if(m_fManifoldPressure > sfMaximumManifoldPressure)
			{
				m_fManifoldPressure = sfMaximumManifoldPressure;
			}
		}

		const float fTurboPowerModifier = GetTurboPowerModifier(pParentVehicle);
		fDriveForce *= 1.0f + (fTurboPowerModifier*m_fManifoldPressure);
	}

}

void CTransmission::Reset()
{
	m_nGear = 0;
	m_nPrevGear = 0;
	m_nFlags = 0;
	m_fForceThrottle = -1.0f;
	m_fRevRatio = 0.0f;
	m_fRevRatioOld = 0.0f;
	m_fForceMultOld = 0.0f;
	m_fClutchRatio = 0.0f;
	m_fThrottle = 0.0f;
	m_fThrottleBlipAmount = 0.0f;
	m_fThrottleBlipTimer = 0.0f;
	m_fThrottleBlipStartTime = fwTimer::GetTimeInMilliseconds();

	m_nGearChangeTime = 0;
	m_UnderLoadChangeUpDelay = 0;
}

//
//
// Can be used to force the transmission into an appropriate gear for the velocity of the vehicle
void CTransmission::SelectAppropriateGearForSpeed()
{
    m_nFlags |= SELECT_GEAR_FOR_SPEED;
}

void CTransmission::AudioSuggestChangeUp(bool changeUp)
{
	if(changeUp)
	{
		m_nFlags |= AUDIO_SUGGEST_CHANGE_UP;
	}
	else
	{
		m_nFlags &= ~AUDIO_SUGGEST_CHANGE_UP;
	}
}

void CTransmission::SetEngineUnderLoad(bool underLoad)
{
	if(underLoad)
	{
		m_nFlags |= ENGINE_UNDER_LOAD;
	}
	else
	{
		m_nFlags &= ~ENGINE_UNDER_LOAD;
	}
}

#if __BANK
void CTransmission::DrawTransmissionStats(CVehicle* pParentVehicle, float fDriveForce, float fSpeedForRevs, int nNumDriveWheels)
{
    float fSpeed = DotProduct(pParentVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pParentVehicle->GetTransform().GetB()));
    float fSpeedKmh = fSpeed * 3.6f;
    float fMaxSpeed = m_fDriveMaxVel * 3.6f;
    float fMaxSpeedEst = pParentVehicle->pHandling->m_fEstimatedMaxFlatVel * 3.6f;
    char debugText[256];

	static float fPreviousSpeed;

	float fAcceleration = (pParentVehicle->GetVelocity().Mag() - fPreviousSpeed)/ fwTimer::GetTimeStep();
    sprintf(debugText, "Spd:%3.1f m/s Acc:%3.3f m/s/s %3.1f km/h %3.1f mph (Max:%3.1f km/h, Est:%3.1f km/h) Gear %d  %s",fSpeed, fAcceleration, fSpeedKmh, fSpeedKmh/1.6f , fMaxSpeed, fMaxSpeedEst, m_nGear, CVehicle::ms_bForceSlowZone ? "Force Slow" : (pParentVehicle->m_nVehicleFlags.bInSlownessZone ? "Slow" : "_") );
    grcDebugDraw::AddDebugOutput(debugText);

	if( CPhysics::ms_bInStuntMode && pParentVehicle->GetDriver() && pParentVehicle->GetDriver()->IsLocalPlayer() )
	{
		sprintf(debugText, "Boost Duration: %3.3f Boost force: %3.3f Slowdown duration: %f Boost Object ID: %lu Previous Boost Object ID: %lu", pParentVehicle->GetSpeedUpBoostDuration(), pParentVehicle->GetBoostAmount(), pParentVehicle->GetSlowDownBoostDuration(), pParentVehicle->m_iCurrentSpeedBoostObjectID, pParentVehicle->m_iPrevSpeedBoostObjectID );
		grcDebugDraw::AddDebugOutput(debugText);
	}

	fPreviousSpeed = pParentVehicle->GetVelocity().Mag();

	if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
	{
		Color32 drawRed(255,0,0,255);
		Color32 drawGreen(0,255,0,255);
		Color32 drawBlue(0,0,255,255);
		Color32 drawPurple(255,0,255,255);
        Color32 drawYellow(255,255,0,255);
        Color32 drawOrange(255,165,0);

		static const float fScreenWidth = 1.0f;
		static const float fScreenHeight = 1.0f;

		float scale = CVehicle::ms_fVehicleDebugScale;

		Vector2 vecStart(0.5f*fScreenWidth, ( 0.8f/scale) *fScreenHeight);

		static const float fLineWidthMult = 0.01f;
		static const float fLineLengthMult = 0.1f;

		float fRevRatio = m_fRevRatio;
		float fSpeedRatio = CalcRevRatio(DotProduct(VEC3V_TO_VECTOR3(pParentVehicle->GetTransform().GetB()), pParentVehicle->GetVelocity()), m_nGear);
		float fDriveForceRatio = (fDriveForce * nNumDriveWheels) / (m_aGearRatios[1] * m_fDriveForce);
		float fClutchRatio = m_fClutchRatio;
        float fWheelSpeed = (fSpeedForRevs/m_fDriveMaxVel);
        float fManifoldPressure = m_fManifoldPressure;

		float fMarkerWidth = fLineWidthMult*fScreenWidth;
		float fLineLength = 1.0f*fLineLengthMult*fScreenHeight;

		fMarkerWidth *= scale;
		fLineLength *= scale;

		fRevRatio = -fLineLength*Clamp(fRevRatio, 0.0f, 1.0f);
		fSpeedRatio = -fLineLength*Clamp(fSpeedRatio, 0.0f, 1.0f);
		fDriveForceRatio = -fLineLength*Clamp(fDriveForceRatio, -1.0f, 1.0f);
		fClutchRatio = -fLineLength*Clamp(fClutchRatio, 0.0f, 1.0f);
        fWheelSpeed = -fLineLength*Clamp(fWheelSpeed, 0.0f, 1.0f);
        fManifoldPressure = -fLineLength*Clamp(fManifoldPressure, -1.0f, 1.0f);

		grcDebugDraw::Line(Vector2(vecStart.x, vecStart.y - fLineLength),Vector2( vecStart.x, vecStart.y + fLineLength), drawRed);
		grcDebugDraw::Line(Vector2(vecStart.x - fMarkerWidth, vecStart.y),Vector2( vecStart.x + fMarkerWidth, vecStart.y), drawRed);

		if( CVehicle::ms_bDebugDrawRevRatio )
		{
			grcDebugDraw::Line( Vector2( vecStart.x, vecStart.y + fRevRatio ), Vector2( vecStart.x + fMarkerWidth, vecStart.y + fRevRatio ), drawGreen );
		}
		if( CVehicle::ms_bDebugDrawSpeedRatio )
		{
			grcDebugDraw::Line( Vector2( vecStart.x, vecStart.y + fSpeedRatio ), Vector2( vecStart.x + fMarkerWidth, vecStart.y + fSpeedRatio ), drawRed );
		}
		if( CVehicle::ms_bDebugDrawDriveForce )
		{
			grcDebugDraw::Line( Vector2( vecStart.x - fMarkerWidth, vecStart.y + fDriveForceRatio ), Vector2( vecStart.x, vecStart.y + fDriveForceRatio ), drawBlue );
		}

		if( CVehicle::ms_bDebugDrawClutchRatio )
		{
			grcDebugDraw::Line( Vector2( vecStart.x - fMarkerWidth, vecStart.y + fClutchRatio ), Vector2( vecStart.x, vecStart.y + fClutchRatio ), drawPurple );
		}

		if( CVehicle::ms_bDebugDrawWheelSpeed )
		{
			grcDebugDraw::Line( Vector2( vecStart.x, vecStart.y + fWheelSpeed ), Vector2( vecStart.x + fMarkerWidth, vecStart.y + fWheelSpeed ), drawYellow );
		}

		if( CVehicle::ms_bDebugDrawManifoldPressure )
		{
			grcDebugDraw::Line( Vector2( vecStart.x, vecStart.y + fManifoldPressure ), Vector2( vecStart.x + fMarkerWidth, vecStart.y + fManifoldPressure ), drawOrange );
		}

        Vector2 vTextRenderPos(vecStart.x + fMarkerWidth, vecStart.y - fLineLength);

       
		if( CVehicle::ms_bDebugDrawSpeedRatio )
		{
			grcDebugDraw::Text( vTextRenderPos, drawRed, "SpeedRatio", true, scale );
			
			if( CVehicle::ms_bDebugDrawLargeText )
			{
				Vector2 largeTextPos = vTextRenderPos;
				largeTextPos.x -= 0.1f * scale;
				char text[ 32 ];
				sprintf( text, "%.2f", CalcRevRatio( DotProduct( VEC3V_TO_VECTOR3( pParentVehicle->GetTransform().GetB() ), pParentVehicle->GetVelocity() ), m_nGear ) );

				grcDebugDraw::Text( largeTextPos, drawRed, text, true, scale * 5.0f );
			}
			
			vTextRenderPos.y += 0.02f * scale;
		}

		if( CVehicle::ms_bDebugDrawRevRatio )
		{
			grcDebugDraw::Text( vTextRenderPos, drawGreen, "RevRatio", true, scale );
			if( CVehicle::ms_bDebugDrawLargeText )
			{
				Vector2 largeTextPos = vTextRenderPos;
				largeTextPos.x -= 0.1f * scale;
				char text[ 32 ];
				sprintf( text, "%.2f", m_fRevRatio );

				grcDebugDraw::Text( largeTextPos, drawRed, text, true, scale * 5.0f );
			}
			vTextRenderPos.y += 0.02f * scale;
		}
        
		if( CVehicle::ms_bDebugDrawDriveForce )
		{
			grcDebugDraw::Text( vTextRenderPos, drawBlue, "DriveForceRatio", true, scale );
			if( CVehicle::ms_bDebugDrawLargeText )
			{
				Vector2 largeTextPos = vTextRenderPos;
				largeTextPos.x -= 0.1f * scale;
				char text[ 32 ];
				sprintf( text, "%.2f", ( fDriveForce * nNumDriveWheels ) / ( m_aGearRatios[ 1 ] * m_fDriveForce ) );

				grcDebugDraw::Text( largeTextPos, drawRed, text, true, scale * 5.0f );
			}
			vTextRenderPos.y += 0.02f * scale;
		}

		if( CVehicle::ms_bDebugDrawClutchRatio )
		{
			grcDebugDraw::Text( vTextRenderPos, drawPurple, "ClutchRatio", true, scale );
			if( CVehicle::ms_bDebugDrawLargeText )
			{
				Vector2 largeTextPos = vTextRenderPos;
				largeTextPos.x -= 0.1f * scale;
				char text[ 32 ];
				sprintf( text, "%.2f", m_fClutchRatio );

				grcDebugDraw::Text( largeTextPos, drawRed, text, true, scale * 5.0f );
			}
			vTextRenderPos.y += 0.02f * scale;
		}

		if( CVehicle::ms_bDebugDrawWheelSpeed )
		{
			grcDebugDraw::Text( vTextRenderPos, drawYellow, "WheelSpeed", true, scale );
			if( CVehicle::ms_bDebugDrawLargeText )
			{
				Vector2 largeTextPos = vTextRenderPos;
				largeTextPos.x -= 0.2f * scale;
				char text[ 32 ];
				sprintf( text, "%.2f", ( fSpeedForRevs / m_fDriveMaxVel ) );

				grcDebugDraw::Text( largeTextPos, drawRed, text, true, scale * 5.0f );
			}
			vTextRenderPos.y += 0.02f * scale;
		}

		if( CVehicle::ms_bDebugDrawManifoldPressure )
		{
			grcDebugDraw::Text( vTextRenderPos, drawOrange, "ManifoldPressure", true, scale );
			if( CVehicle::ms_bDebugDrawLargeText )
			{
				Vector2 largeTextPos = vTextRenderPos;
				largeTextPos.x -= 0.2f * scale;
				char text[ 32 ];
				sprintf( text, "%.2f", m_fManifoldPressure );

				grcDebugDraw::Text( largeTextPos, drawRed, text, true, scale * 5.0f );
			}
			vTextRenderPos.y += 0.02f * scale;
		}

		g_pfTB.SetDisplayOverbudget( false );
		g_timeCycle.GetGameDebug().SetDisplayModifierInfo( false );
	}
}
	
void CTransmission::AddWidgets(bkBank* pBank)
{
	((void)pBank);

#if __DEV
	//pBank->AddSlider("ChangeUp Ratio", &sfTransChangeUpRatio, 0.5f, 1.0f, 0.01f);
	//pBank->AddSlider("ChangeDown Ratio", &sfTransChangeDownRatio, 0.1f, 0.9f, 0.01f);
	
	pBank->AddSlider("Throttle Contribution", &sfTransChangeThrottleContrib, 0.0f, 1.0f, 0.01f);
	pBank->AddSlider("Add Brake Contribution", &sfTransChangeBrakeContrib, 0.0f, 10.0f, 0.1f);

//	pBank->AddSlider("Min Change Delay", &ms_nChangeDelayTime, (u32)0, (u32)20000, 100.0f, NULL, "Min delay before changing down after changing up, or vice-versa");

	pBank->AddSlider("Cut Throttle when Clutch is Below", &sfTransChangeDisableThrottleWhenClutchDown, -1.0f, 0.9f, 0.1f);
	pBank->AddSlider("Revs Change Time Factor", &sfTransRevsChangeTimeFactor, 0.1f, 10.f, 0.1f);
#endif

}


#endif

void CTransmission::ForceThrottle(f32 throttle)
{
	m_fForceThrottle = throttle;
}

//
dev_float sfEngineDieChanceFromCollision = 0.5f;
dev_float sfEngineHeathThreasholdForSpreadingFire = -1000.0f;
//
void CTransmission::ApplyEngineDamage(CVehicle* pParent, CEntity* pInflictor, eDamageType nDamageType, float fDamage, bool bForceFire)
{
    if(pParent->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
    {
	    if(m_fEngineHealth >= ENGINE_DAMAGE_ONFIRE)
	    {
		    m_fEngineHealth -= fDamage * m_fEngineHealthDamageScale;

		    if(m_fEngineHealth <= ENGINE_DAMAGE_ONFIRE)
		    {
			    m_fEngineHealth = ENGINE_DAMAGE_ONFIRE + 0.01f;

			    float randomChance = 0.7f;
			    if(nDamageType >= DAMAGE_TYPE_BULLET && nDamageType <= DAMAGE_TYPE_FIRE)
				    randomChance = 0.5f;

			    if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > randomChance || bForceFire)
			    {
				    // if damage came from a collision, there's a chance your engine just dies instead of going on fire
				    if(nDamageType==DAMAGE_TYPE_COLLISION && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sfEngineDieChanceFromCollision)
				    {
					    // just turn engine off so it can still be shot and made to go on fire that way.
					    m_fEngineHealth = ENGINE_DAMAGE_ONFIRE;
					    pParent->m_nVehicleFlags.bEngineOn = false;
				    }
				    else
				    {
					    m_fEngineHealth = ENGINE_DAMAGE_ONFIRE - 1.0f;
					    if(pInflictor)
					    {
						    m_pEntityThatSetUsOnFire = pInflictor;
					    }
				    }
			    }
		    }
	    }
		else if(nDamageType >= DAMAGE_TYPE_BULLET && nDamageType <= DAMAGE_TYPE_FIRE)
		{
			m_fEngineHealth -= fDamage * m_fEngineHealthDamageScale;
			if(m_fEngineHealth < sfEngineHeathThreasholdForSpreadingFire)
			{
				pParent->GetVehicleDamage()->SetPetrolTankOnFire(m_pEntityThatSetUsOnFire);
			}

			// cap the damage
			if(m_fEngineHealth < ENGINE_DAMAGE_FINSHED)
				m_fEngineHealth = ENGINE_DAMAGE_FINSHED;
		}
    }
}

#define NUM_TIMES_ENGINE_FIRE_SPREADS (8)
dev_float saEngineFireSpreadTimes[NUM_TIMES_ENGINE_FIRE_SPREADS] = {-250.0f, -500.0f, -750.0f, -1000.0f, -1250.0f, -1500.0f, -2000.0f, -3000.0f};
dev_float sfEngineFireHealthDropMin = 60.0f;
dev_float sfEngineFireHealthDropMax = 100.0f;
dev_float sfEngineFireSpreadThreshold = 95.0f;
dev_float sfEngineFireSpreadThresholdMovingSubtract = 3.0f;
dev_float sfEngineFireSpreadMinForwardSpeed = 10.0f;
extern float sfEngineMinDistance;
//
void CTransmission::ProcessEngineFire(CVehicle* pParent, float fTimeStep)
{
	if(m_fEngineHealth < ENGINE_DAMAGE_ONFIRE && !pParent->m_nVehicleFlags.bDisableEngineFires)
	{
		float fHealthDrop = fwRandom::GetRandomNumberInRange(sfEngineFireHealthDropMin, sfEngineFireHealthDropMax);

		float fEngineHealthOld = m_fEngineHealth;
		m_fEngineHealth -= fHealthDrop * fTimeStep * m_fEngineHealthDamageScale;
		if(m_fEngineHealth < ENGINE_DAMAGE_FINSHED)
			m_fEngineHealth = ENGINE_DAMAGE_FINSHED;

		if(pParent->InheritsFromPlane())
		{
			Vector3 vEnginePosLocal = VEC3_ZERO;
			Vector3 vNormLocal = VEC3_ZERO;
			if(pParent->GetBoneIndex(VEH_ENGINE) >= 0)
			{
				Matrix34 matEngineLocal = pParent->GetLocalMtx(pParent->GetBoneIndex(VEH_ENGINE));
				vEnginePosLocal = matEngineLocal.d;
			}
			if(fwRandom::GetRandomTrueFalse())
			{
				vEnginePosLocal.x += sfEngineMinDistance - SMALL_FLOAT;
			}
			else
			{
				vEnginePosLocal.x -= sfEngineMinDistance - SMALL_FLOAT;
			}
			((CPlane *)pParent)->GetAircraftDamage().ApplyDamageToEngine(GetEntityThatSetUsOnFire(), (CPlane *)pParent, fHealthDrop * fTimeStep, DAMAGE_TYPE_FIRE, vEnginePosLocal, vNormLocal);
		}

		if(m_fEngineHealth > ENGINE_DAMAGE_FINSHED)
		{
			// chance to start a fire on the petrol tank (multiple chances now)
			for(int i=0; i<NUM_TIMES_ENGINE_FIRE_SPREADS; i++)
			{
				if(m_fEngineHealth < saEngineFireSpreadTimes[i] && fEngineHealthOld >= saEngineFireSpreadTimes[i])
				{
					float fSpreadThreshhold = sfEngineFireSpreadThreshold;

					// If the vehicle is moving forward fast enough, possibly spread fire from engine to petrol tank.  This threshold is set fairly small and is intended
					// to prevent vehicles that are stuck in traffic from exploding from just engine fire.  Can still explode from other sources of petrol tank igniting.
					float fForwardSpeed = DotProduct(pParent->GetVelocity(), VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()));
					if(fForwardSpeed > sfEngineFireSpreadMinForwardSpeed)
					{
						// Increase chance of fire spreading back to petrol tank in proportion to forward speed.
						fSpreadThreshhold -=  fForwardSpeed * sfEngineFireSpreadThresholdMovingSubtract;
						if (fHealthDrop > fSpreadThreshhold || i==NUM_TIMES_ENGINE_FIRE_SPREADS-1)
						{
							pParent->GetVehicleDamage()->SetPetrolTankOnFire(m_pEntityThatSetUsOnFire);
						}
					}
				}
			}
			// Let the world know its on fire
			CEventShockingFire ev(*pParent);
			CShockingEventsManager::Add(ev);
		}

		if(m_fEngineHealth <= ENGINE_DAMAGE_FIRE_FINISH)
		{
			pParent->m_nVehicleFlags.bEngineOn = false;
		}
	}
}
static dev_float sfFinalGearRatio = 0.9f;
void CTransmission::CalculateGearRatios(float UNUSED_PARAM(fMaxVel), int nNumGears, CVehicle* pParentVehicle)
{
	// reverse gear
	m_aGearRatios[0] = 1.0f / sfTransReverseGearRatio;
	// top gear
	m_aGearRatios[nNumGears] = sfFinalGearRatio;

	// if that's all the gears we've got
	if(nNumGears==1)
    {
		return;
    }

	// first gear
	m_aGearRatios[1] = 1.0f / sfTransFirstGearRatio;

	if( pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_INCREASE_LOW_SPEED_TORQUE ) )
	{
		m_aGearRatios[1] *= sfLowSpeedTorqueIncreaseFirstGearRatio;
	}

	if( pParentVehicle->pHandling->GetCarHandlingData() &&
		pParentVehicle->pHandling->GetCarHandlingData()->aFlags & CF_CLOSE_RATIO_GEARBOX )
	{
		m_aGearRatios[ 1 ] *= sfCloseRatioGearboxFirstGearRatio;
	}

	// if that's all the gears we've got
	if(nNumGears==2)
		return;

    static dev_bool sbSelectGearsWithGeometricProgressing = false;

    if(sbSelectGearsWithGeometricProgressing)
    {
	    float fGearConst = m_aGearRatios[1] / m_aGearRatios[nNumGears];
	    fGearConst = rage::Powf(fGearConst, 1.0f / (nNumGears - 1));

	    for(int i=2; i<=nNumGears; i++)
	    {
		    m_aGearRatios[i] = m_aGearRatios[i-1] / fGearConst;
	    }
    }
    else
    {
        static dev_float sfProgressionFactor = 1.1f;
    
        //select gears using a progression factor
        float baseRatio = rage::Powf( (1.0f/(rage::Powf(sfProgressionFactor, 0.5f*(nNumGears-1.0f)*(nNumGears-2.0f))) )*m_aGearRatios[1] , 1.0f/(nNumGears-1));

        for(int i=2; i<=nNumGears; i++)
        {
            m_aGearRatios[i] = m_aGearRatios[nNumGears] * rage::Powf(baseRatio, (float)nNumGears-i) * rage::Powf(sfProgressionFactor, 0.5f*(nNumGears-i)*(nNumGears-i-1.0f));
        }
    }
}

float CTransmission::ProcessEngineCvt(CVehicle* pParentVehicle, int nNumDriveWheels, float fSpeedForRevs, float UNUSED_PARAM(fSpeedFromVehicle), float fTimeStep)
{
	CHandlingData* pHandling = pParentVehicle->pHandling;
	float fThrottle = m_fThrottle = m_fForceThrottle >= 0.0f ? m_fForceThrottle : pParentVehicle->GetThrottle();

	Assertf(m_nDriveGears == 1,"%s: CVT vehicles should be set up with only 1 gear",pHandling->m_handlingName.TryGetCStr());

	if((fThrottle < 0.0f && m_nGear > 0) || (fThrottle > 0.0f && m_nGear == 0))
	{
		m_fThrottle = fThrottle = 0.0f;
		return 0.0f;
	}
	// if the clutch is down then lift off the throttle (except when in first gear or reverse or applying handBrake)
	else if(!pParentVehicle->GetHandBrake() && sfTransChangeDisableThrottleWhenClutchDown >= 0.0f && m_fClutchRatio < sfTransChangeDisableThrottleWhenClutchDown && m_nGear > 1)
	{
		m_fThrottle = fThrottle = 0.0f;
	}

	float fGearRatio = m_aGearRatios[m_nGear];
    float fDesiredRevs = 0.0f;

    if(m_nGear != 0)
    {
        float fSpeedAtMaxLowgear = 1.0f / sfCvtLowGearRato * m_fDriveMaxVel;
        float fSpeedFraction = (fSpeedForRevs - fSpeedAtMaxLowgear) / (m_fDriveMaxVel- fSpeedAtMaxLowgear);		
        if(fSpeedFraction <= 0.0f)
        {
	        // Assume we want full low gear
	        // since haven't maxd out revs yet
	        fGearRatio = sfCvtLowGearRato;
        }
        else
        {
	        if(fSpeedFraction > 1.0f)
	        {
		        fSpeedFraction = 1.0f;
		        fThrottle = sfTransOverRevThrottle;
	        }
	        fGearRatio = sfCvtLowGearRato + (fSpeedFraction * (m_aGearRatios[m_nGear] - sfCvtLowGearRato));

	        fGearRatio *= rage::Abs(fThrottle);	// Want revs to ease off with throttle. This is essentially the same as picking a higher gear at lower revs
	        fGearRatio = rage::Clamp(fGearRatio,m_aGearRatios[m_nGear],sfCvtLowGearRato);
        }
        fDesiredRevs = fGearRatio * fSpeedForRevs / m_fDriveMaxVel;
    }
    else
    {
		float fCvtReverseMaxSpeedMult = sfCvtReverseMaxSpeedMult;
		if(pParentVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)// don't want bicycles to reverse fast.
		{
			fCvtReverseMaxSpeedMult = sfCvtReverseMaxSpeedMultBicycle;
		}

        float fSpeedAtMaxLowgear = 1.0f / sfCvtReverseLowGearRato * (m_fDriveMaxVel * fCvtReverseMaxSpeedMult);
        float fSpeedFraction = (fSpeedForRevs - fSpeedAtMaxLowgear) / ((m_fDriveMaxVel * fCvtReverseMaxSpeedMult) - fSpeedAtMaxLowgear);		
        if(fSpeedFraction >= 0.0f)
        {
            // Assume we want full low gear
            // since haven't maxd out revs yet
            fGearRatio = sfCvtReverseLowGearRato;
        }
        else
        {
			float minSpeedFraction = -1.0f;

			if( pParentVehicle &&
				pParentVehicle->pHandling->GetCarHandlingData() &&
				pParentVehicle->pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT )
			{
				minSpeedFraction = -0.3f;
			}

            if(fSpeedFraction < minSpeedFraction )
            {
                fSpeedFraction = -1.0f;
                fThrottle = sfTransOverRevThrottle;
            }
            fGearRatio = sfCvtReverseLowGearRato + (fSpeedFraction * (m_aGearRatios[m_nGear] - sfCvtReverseLowGearRato));

            fGearRatio *= rage::Abs(fThrottle);	// Want revs to ease off with throttle. This is essentially the same as picking a higher gear at lower revs
            fGearRatio = rage::Clamp(fGearRatio, m_aGearRatios[m_nGear],sfCvtReverseLowGearRato);
        }
        fDesiredRevs = fGearRatio * fSpeedForRevs / (m_fDriveMaxVel * fCvtReverseMaxSpeedMult);
    }

	if(fDesiredRevs > 1.0f)
	{
		fDesiredRevs = 1.0f;
	}
	float fClutchMult = Clamp(m_fClutchRatio, 0.0f, 1.0f);
	if(pParentVehicle->GetHandBrake() && !(pHandling->hFlags & HF_HANDBRAKE_REARWHEELSTEER))
	{
		fClutchMult = 0.f;
	}
	fDesiredRevs = fClutchMult * fDesiredRevs + (1.0f - fClutchMult) * rage::Abs(fThrottle);

	float fDriveForce = 0.0f;

	// idle revs
	if(fDesiredRevs < ms_fIdleRevs)
		fDesiredRevs = ms_fIdleRevs;

	if(m_fRevRatio < fDesiredRevs)
	{
		m_fRevRatio += pHandling->m_fDriveInertia * sfTransRevsChangeTimeFactor * fTimeStep;
		if(m_fRevRatio > fDesiredRevs)
			m_fRevRatio = fDesiredRevs;
	}
	else if(m_fRevRatio > fDesiredRevs)
	{
		m_fRevRatio -= pHandling->m_fDriveInertia * sfTransRevsChangeTimeFactor * fTimeStep;
		if(m_fRevRatio < fDesiredRevs)
			m_fRevRatio = fDesiredRevs;
	}	

	// if we're in 1st or reverse, use the clutch to stop the revs dropping too low
	if(m_nGear < 2 && m_fRevRatio < sfTransClutchIn1stRevRatio)
	{
		m_fClutchRatio = rage::Min(m_fClutchRatio, rage::Max( ms_fIdleRevs, (m_fRevRatio / sfTransClutchIn1stRevRatio)));
	}

	fDriveForce = rage::Abs(fThrottle) * m_fDriveForce * fGearRatio;
	fDriveForce *= (m_fRevRatio / fDesiredRevs);
	fDriveForce /= float(Max(nNumDriveWheels, 1));

	ProcessNitrous( pParentVehicle, fDriveForce, fThrottle, fTimeStep );

    TUNE_GROUP_FLOAT( VEHICLE_TRANSMISSION, CVT_LOW_SPEED_TORQUE_INCREASE_THRESHOLD, 15.0f, 0.0f, 50.0f, 0.1f );
    TUNE_GROUP_FLOAT( VEHICLE_TRANSMISSION, CVT_LOW_SPEED_TORQUE_INCREASE, 2.0f, 0.0f, 10.0f, 0.1f );
    float fFwdSpeed = Dot( pParentVehicle->GetTransform().GetB(), VECTOR3_TO_VEC3V( pParentVehicle->GetVelocity() ) ).Getf();
    fFwdSpeed = Abs( fFwdSpeed );

    if( pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_INCREASE_LOW_SPEED_TORQUE ) &&
        fFwdSpeed < CVT_LOW_SPEED_TORQUE_INCREASE_THRESHOLD &&
        m_nGear != 0 )
    {
        float torqueIncrease = 1.0f - ( fFwdSpeed / CVT_LOW_SPEED_TORQUE_INCREASE_THRESHOLD );
        torqueIncrease *= CVT_LOW_SPEED_TORQUE_INCREASE;
        fDriveForce += fDriveForce * torqueIncrease;
    }

	if(pParentVehicle->GetCheatFlag(VEH_CHEAT_POWER2|VEH_SET_POWER2))
		fDriveForce *= POWER_CHEAT_MULT_2;
	else if(pParentVehicle->GetCheatFlag(VEH_CHEAT_POWER1|VEH_SET_POWER1))
		fDriveForce *= POWER_CHEAT_MULT_1;

	// Apply any cheat increase in power.
	fDriveForce *= pParentVehicle->GetCheatPowerIncrease();

	return fDriveForce;
}



