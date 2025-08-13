//
// name:		VehiclePacket.cpp
//
#include "control/replay/Vehicle/VehiclePacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "vectormath/vectormath.h"
#include "vectormath/classfreefuncsf.h"

#include "audio/caraudioentity.h"
#include "camera/CamInterface.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "renderer/PostScan.h"
#include "vehicles/Bike.h"
#include "vehicles/Heli.h"
#include "vehicles/Automobile.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "scene/world/GameWorld.h"
#include "script/commands_entity.h"

#include "control/replay/ReplayInternal.h"
#include "control/replay/Entity/FragmentPacket.h"
#include "control/replay/ReplayExtensions.h"
#include "control/replay/Vehicle/WheelPacket.h"
#include "control/replay/vehicle/ReplayWheelValues.h"

#include "control/replay/ReplayInterfaceVeh.h" // Urgh

#include "control/replay/Compression/Quantization.h"

#include "fwpheffects/ropemanager.h"

// ============================================================================
// Conversion Factors for Compression
const float FLOAT_TO_U8_RANGE_0_TO_1		 = 255.0f;		// Map float (range 0 to 1) to u8
const float FLOAT_TO_U8_RANGE_0_TO_1_PT_2	 = 212.5f;
const float FLOAT_TO_U8_RANGE_0_TO_1000		 = 0.255f;
const float FLOAT_TO_U8_RANGE_0_TO_5		 = 51.0f;
const float FLOAT_TO_S8_RANGE_NEG_170_TO_170 = 0.745f;
const float FLOAT_TO_U8_RANGE_0_TO_40		 = 6.375f;
const float FLOAT_TO_S8_RANGE_NEG_1_TO_1	 = 127.0f;		// Map float (range -1 to 1) to u8
const float FLOAT_TO_S8_RANGE_NEG_1_PT_5_TO_1_PT_5	= 85.0f;// Map float (range -1.5 to 1.5) to u8
const float STORE_MASS_FACTOR				 = 1 / 100.0f;	// Map float (range 0 to 50000 incremented by 100) to u16
const float FLOAT_TO_S8_FACTOR_20			 = 20.0f;
const float FLOAT_TO_S8_FACTOR_50			 = 50.0f;
const float FLOAT_TO_S8_FACTOR_100			 = 100.0f;
const float FLOAT_TO_S8_RANGE_NEG_PI_TO_PI	 = 127.0f / PI;

// Conversion Factors for Decompression
const float U8_TO_FLOAT_RANGE_0_TO_1		 = 1 / 255.0f;
const float U8_TO_FLOAT_RANGE_0_TO_1_PT_2	 = 1 / 212.5f;
const float U8_TO_FLOAT_RANGE_0_TO_1000		 = 1 / 0.255f;
const float U8_TO_FLOAT_RANGE_0_TO_5		 = 1 / 51.0f;
const float S8_TO_FLOAT_RANGE_NEG_170_TO_170 = 1 / 0.745f;
const float U8_TO_FLOAT_RANGE_0_TO_40		 = 1 / 6.375f;
const float S8_TO_FLOAT_RANGE_NEG_1_TO_1	 = 1 / 127.0f;
const float S8_TO_S8_FLOAT_NEG_1_PT_5_TO_1_PT_5	= 1 /  85.0f;
const float EXTRACT_MASS_FACTOR				 = 100.0f;		// Map u16 to float (range 0 to 50000 incremented by 100)
const float S8_TO_FLOAT_DIVISOR_20			 = 1 / 20.0f;
const float S8_TO_FLOAT_DIVISOR_50			 = 1 / 50.0f;
const float S8_TO_FLOAT_DIVISOR_100			 = 1 / 100.0f;
const float S8_TO_FLOAT_RANGE_NEG_PI_TO_PI	 = PI / 127.0f;


float ProcessChainValue(float currentVal, float nextVal, float interpIn, bool animFwd)
{
	if(currentVal == nextVal)
		return currentVal;

	float value1 = currentVal;
	float value2 = nextVal;

	if(animFwd && (value2 < value1))
		value2 += 1.0f;
 	else if(!animFwd && (value1 < value2))
 		value1 += 1.0f;

	float delta = (value2 - value1) * interpIn;
	float result = value1 + delta;

	if(result > 1.0f)
		result -= 1.0f;

	return result;
}

//========================================================================================================================
// CWheelFullData. 
//========================================================================================================================

void CWheelFullData::Extract(CVehicle* pVehicle, CWheel* pWheel, CWheelFullData const* pNextPacket, f32 fInterpValue) const
{
    (void)pVehicle;

	if(!pNextPacket)
	{
		// No Interpolation
		ExtractInterpolateableValues( pWheel );
	}
	else
	{
		// Interpolate Values
		ExtractInterpolateableValues( pWheel, pNextPacket, fInterpValue );
	}

	// Extract Values That Do Not Need Interpolation
	pWheel->m_nDynamicFlags = m_DynamicFlags; 
	pWheel->m_nConfigFlags =  m_ConfigFlags;


	m_vecHitPos.Load(pWheel->m_vecHitPos);
	m_vecHitCentrePos.Load(pWheel->m_vecHitCentrePos);
	m_vecHitNormal.Load(pWheel->m_vecHitNormal);
 	m_DynamicData.m_vecGroundContactVelocity.Load(pWheel->m_vecGroundContactVelocity);
	m_DynamicData.m_vecTyreContactVelocity.Load(pWheel->m_vecTyreContactVelocity);

#if REPLAY_WHEELS_USE_PACKED_VECTORS
	Vector3 wheelPosition;
	pWheel->GetWheelPosAndRadius(wheelPosition);
	pWheel->m_vecHitPos += wheelPosition;
	pWheel->m_vecHitCentrePos += wheelPosition;
#else // REPLAY_WHEELS_USE_PACKED_VECTORS
	(void)pVehicle;
#endif // REPLAY_WHEELS_USE_PACKED_VECTORS

	pWheel->m_fTyreLoad				= GetTyreLoad();
	pWheel->m_fTyreTemp				= GetTyreTemp();
	pWheel->m_fEffectiveSlipAngle	= GetfEffectiveSlipAngle();
}


//========================================================================================================================
f32 InterpolateWrappableValueFromS8(f32 difference, s8 currentValue, s8 nextValue, f32 interpolationStep)
{
	// Note that a s8 ranges from -128 to 127 and is stored as follows:
	//  0, 1, ... , 126, 127, -128, -127, ... , -2, -1

	f32 interpolatedValue = 0;

	if( difference > 127.5f )
	{
		interpolatedValue = currentValue + (255.0f - difference) * interpolationStep;

		if( interpolatedValue > 127.0f )
		{
			interpolatedValue -= 255.0f;
		}
	}
	else if( -difference > 127.5f )
	{
		interpolatedValue = currentValue - (255.0f + difference) * interpolationStep;

		if( interpolatedValue < -128.0f )
		{
			interpolatedValue += 255.0f;
		}
	}
	else
	{
		interpolatedValue = Lerp( interpolationStep, (f32)(currentValue), (f32)(nextValue) );
	}

	return interpolatedValue;
}

f32 InterpolateWrappableValueFrom(f32 currentValue, f32 nextValue, f32 interpolationStep)
{
	f32 difference = currentValue - nextValue;
	f32 interpolatedValue = 0.0f;

	if(difference > PI)
	{
		interpolatedValue = currentValue + (PI*2.0f - difference) * interpolationStep;

		if(interpolatedValue > PI)
		{
			interpolatedValue -= PI*2.0f;
		}
	}
	else if(difference < -PI)
	{
		interpolatedValue = currentValue - (PI*2.0f + difference) * interpolationStep;

		if(interpolatedValue < -PI)
		{
			interpolatedValue += PI*2.0f;
		}
	}
	else
	{
		interpolatedValue = Lerp(interpolationStep, currentValue, nextValue);
	}

	return interpolatedValue;
}

//=====================================================================================================
void CWheelFullData::ExtractInterpolateableValues(CWheel* pWheel, CWheelFullData const* pNextPacket, f32 fInterp) const
{
	f32 interpWheelCompression;
	f32 interpSteerAngle;
	f32 interpRotAng;

	f32 interpSuspensionHealth;
	f32 interpTyreHealth;
	f32 interpBrakeForce;
	f32 interpFrictionDamage;
	f32 interpCompression;
	f32 interpCompressionOld;
	f32 interpStaticDelta;
	f32 interpRotAngVel;
	f32 interpEffectiveSlipAngle;

	f32 interpFwdSlipAngle;
	f32 interpSideSlipAngle;

	if( pNextPacket && fInterp )
	{
		// Interpolate
		interpSteerAngle			= Lerp(fInterp, GetSteeringAngle(), pNextPacket->GetSteeringAngle());
		interpRotAng				= InterpolateWrappableValueFrom(GetRotAngle(), pNextPacket->GetRotAngle(), fInterp);
		interpWheelCompression		= Lerp(fInterp, GetWheelCompression(), pNextPacket->GetWheelCompression());
		interpSuspensionHealth		= Lerp(fInterp, GetSuspensionHealth(), pNextPacket->GetSuspensionHealth());
		interpTyreHealth			= Lerp(fInterp, GetTyreHealth(), pNextPacket->GetTyreHealth());
		interpBrakeForce			= Lerp(fInterp, GetBrakeForce(), pNextPacket->GetBrakeForce());
		interpFrictionDamage		= Lerp(fInterp, GetFrictionDamage(), pNextPacket->GetFrictionDamage());
		interpCompression			= Lerp(fInterp, GetCompression(), pNextPacket->GetCompression());
		interpCompressionOld		= Lerp(fInterp, GetCompressionOld(), pNextPacket->GetCompressionOld());
		interpStaticDelta			= Lerp(fInterp, GetStaticDelta(), pNextPacket->GetStaticDelta());
// TODO: Try seeing if it's possible to lerp this without messing up audio once slow-mo audio is in
//		interpRotAngVel				= Lerp(fInterp, GetRotAngVel(), pNextPacket->GetRotAngVel());
		interpRotAngVel				= GetRotAngVel();
		interpEffectiveSlipAngle	= Lerp(fInterp, GetEffectiveSlipAngle(), pNextPacket->GetEffectiveSlipAngle());

		interpFwdSlipAngle			= GetFwdSlipAngle();
		interpSideSlipAngle			= GetSideSlipAngle();
	}
	else
	{
		// No Interpolation
		interpWheelCompression		= GetWheelCompression();
		interpSteerAngle			= GetSteeringAngle();
		interpRotAng				= GetRotAngle();

		interpSuspensionHealth		= GetSuspensionHealth();
		interpTyreHealth			= GetTyreHealth();
		interpBrakeForce			= GetBrakeForce();
		interpFrictionDamage		= GetFrictionDamage();
		interpCompression			= GetCompression();
		interpCompressionOld		= GetCompressionOld();
		interpStaticDelta			= GetStaticDelta();
		interpRotAngVel				= GetRotAngVel();
		interpEffectiveSlipAngle	= GetEffectiveSlipAngle();

		interpFwdSlipAngle			= GetFwdSlipAngle();
		interpSideSlipAngle			= GetSideSlipAngle();
	}

	// Set Values
	pWheel->m_fWheelCompression		= interpWheelCompression;
	pWheel->m_fSteerAngle			= interpSteerAngle;
	pWheel->SetRotAngle(interpRotAng);

	pWheel->m_fSuspensionHealth		= interpSuspensionHealth;
	pWheel->SetTyreHealth(interpTyreHealth);
	pWheel->m_fBrakeForce			= interpBrakeForce;
	pWheel->m_fFrictionDamage		= interpFrictionDamage;
	pWheel->m_fCompression			= interpCompression;
	pWheel->m_fCompressionOld		= interpCompressionOld;
	pWheel->m_fStaticDelta			= interpStaticDelta;
	pWheel->m_fRotAngVel			= interpRotAngVel;
	pWheel->m_fEffectiveSlipAngle	= interpEffectiveSlipAngle;

	pWheel->m_fFwdSlipAngle			= interpFwdSlipAngle;
	pWheel->m_fSideSlipAngle		= interpSideSlipAngle;
}

//========================================================================================================================
// CPacketVehicleWheelUpdate_Old.
//========================================================================================================================

extern u32 g_NoOfDynamicsSaved;

#if REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

void CPacketVehicleWheelUpdate_Old::Store(const CVehicle *pVehicle)
{
	PACKET_EXTENSION_RESET(CPacketVehicleWheelUpdate_Old);
	CPacketBase::Store(PACKETID_WHEELUPDATE_OLD, sizeof(CPacketVehicleWheelUpdate_Old));
	SetPadU8(0, (u8)pVehicle->GetNumWheels());
	SetPadU8(1, sizeof(CPacketVehicleWheelUpdate_Old));

	CWheelBaseData *pWheelBase = GetBasePtr();
	CWheelBaseData *pWheelDest = pWheelBase;;

	for(u32 i=0; i<pVehicle->GetNumWheels(); i++)
	{
		CWheel const*pWheel = pVehicle->GetWheel(i);

		// Safety Checks
		// - Making sure that the ranges we assume when compressing values are true
		replayAssertf( pWheel->m_fSuspensionHealth		>= 0	&& pWheel->m_fSuspensionHealth		<= 1000,"CPacketVehicleWheelUpdate_Old::Store: Suspension Health %f",		pWheel->m_fSuspensionHealth		);
		replayAssertf( pWheel->m_fTyreHealth			>= 0	&& pWheel->m_fTyreHealth			<= 1000,"CPacketVehicleWheelUpdate_Old::Store: Tyre Health %f",				pWheel->m_fTyreHealth			);
		replayAssertf( pWheel->m_fBrakeForce			>= 0	&& pWheel->m_fBrakeForce			<= 5,	"CPacketVehicleWheelUpdate_Old::Store: Brake Force %f",				pWheel->m_fBrakeForce			);

		// Base data.
		pWheelDest->m_TypeInfo				= 0;
		pWheelDest->m_DynamicFlags			= pWheel->m_nDynamicFlags;
		pWheelDest->m_ConfigFlags			= pWheel->m_nConfigFlags;
		pWheelDest->m_steeringAngle			= pWheel->GetSteeringAngle();
		pWheelDest->m_rotAngle				= rage::Wrap(pWheel->m_fRotAng, -PI, PI);
	#if REPLAY_WHEELS_USE_PACKED_VECTORS
		Vec3V wheelPosition;
		pWheel->GetWheelPosAndRadius(RC_VECTOR3(wheelPosition));
		Vec3V hitPosMinusWheelPos = RCC_VEC3V(pWheel->m_vecHitPos) - wheelPosition;
		Vec3V hitCentrePosMinusWheelPos = RCC_VEC3V(pWheel->m_vecHitCentrePos) - wheelPosition;
		// Generally these shouldn't be greater than the wheel's radius...
		// Apparently it does happen though when vehicles warp or are pushed...skip if so
		if(IsGreaterThanAll(MagSquared(hitPosMinusWheelPos), ScalarV(pWheel->m_fWheelRadius * pWheel->m_fWheelRadius)))
		{
			hitPosMinusWheelPos = Vec3V(V_ZERO);
		}
		if(IsGreaterThanAll(MagSquared(hitCentrePosMinusWheelPos), ScalarV(pWheel->m_fWheelRadius * pWheel->m_fWheelRadius)))
		{
			hitCentrePosMinusWheelPos = Vec3V(V_ZERO);
		}
		replayAssertf(IsLessThanOrEqualAll(Abs(hitPosMinusWheelPos), Vec3V(V_TWO)), "CPacketVehicleWheelUpdate_Old::Store()...Wheel radius too high (> 2m).");
		replayAssertf(IsLessThanOrEqualAll(Abs(hitCentrePosMinusWheelPos), Vec3V(V_TWO)), "CPacketVehicleWheelUpdate_Old::Store()...Wheel radius too high (> 2m).");
		pWheelDest->m_vecHitPos.Store(RCC_VECTOR3(hitPosMinusWheelPos));
		pWheelDest->m_vecHitCentrePos.Store(RCC_VECTOR3(hitCentrePosMinusWheelPos));
		pWheelDest->m_vecHitNormal.Store(pWheel->m_vecHitNormal);
	#else // REPLAY_WHEELS_USE_PACKED_VECTORS
		pWheelDest->m_vecHitPos.Store(pWheel->m_vecHitPos);
		pWheelDest->m_vecHitCentrePos.Store(pWheel->m_vecHitCentrePos);
		pWheelDest->m_vecHitNormal.Store(pWheel->m_vecHitNormal);
	#endif // REPLAY_WHEELS_USE_PACKED_VECTORS

		CWheelDynamicData dynamicData;
		CWheelDynamicData defaultDynamicData;

		// Dynamic data.
		dynamicData.m_BrakeForce			= pWheel->m_fBrakeForce;
		dynamicData.m_FrictionDamage		= pWheel->m_fFrictionDamage;
		dynamicData.m_Compression			= pWheel->m_fCompression;
		dynamicData.m_CompressionOld		= pWheel->m_fCompressionOld;
		dynamicData.m_staticDelta			= pWheel->m_fStaticDelta;
		dynamicData.m_RotAngVel				= Clamp(pWheel->m_fRotAngVel, dynamicData.m_RotAngVel.GetMin(), dynamicData.m_RotAngVel.GetMax());
		dynamicData.m_EffectiveSlipAngle	= pWheel->m_fEffectiveSlipAngle;
		dynamicData.m_wheelCompression	= pWheel->GetWheelCompression();
		dynamicData.m_fwdSlipAngle		= pWheel->GetFwdSlipAngle();
		dynamicData.m_sideSlipAngle		= pWheel->GetSideSlipAngle();
		dynamicData.m_tyreLoad			= pWheel->m_fTyreLoad;
		dynamicData.m_tyreTemp			= pWheel->m_fTyreTemp;
		dynamicData.m_fEffectiveSlipAngle	= pWheel->m_fEffectiveSlipAngle;
		dynamicData.m_vecGroundContactVelocity.Store(pWheel->m_vecGroundContactVelocity);
		dynamicData.m_vecTyreContactVelocity.Store(pWheel->m_vecTyreContactVelocity);

		CWheelHealthData healthData;
		CWheelHealthData defaultHealthData;

		// Health data.
		healthData.m_SuspensionHealth	= pWheel->m_fSuspensionHealth;
		healthData.m_TyreHealth			= pWheel->m_fTyreHealth;

		if(!(dynamicData == defaultDynamicData))
			pWheelDest->m_TypeInfo |= WHEEL_HAS_DYNAMIC_DATA;
		if(!(healthData == defaultHealthData))
			pWheelDest->m_TypeInfo |= WHEEL_HAS_HEALTH_DATA;

		switch (pWheelDest->m_TypeInfo)
		{
		case 0:
			{
				// Move along 1 base wheel data.
				pWheelDest++;
				break;
			}
		case WHEEL_HAS_DYNAMIC_DATA:
			{
				CWheelWithDynamicData *pWithDynamic = (CWheelWithDynamicData *)pWheelDest;
				pWithDynamic->m_DynamicData = dynamicData;
				pWheelDest = (CWheelBaseData *)(&pWithDynamic[1]);
				break;
			}
		case  WHEEL_HAS_HEALTH_DATA:
			{
				CWheelWithHealthData *pWithHealth = (CWheelWithHealthData *)pWheelDest;
				pWithHealth->m_HealthData = healthData;
				pWheelDest = (CWheelBaseData *)(&pWithHealth[1]);
				break;
			}
		case  (WHEEL_HAS_DYNAMIC_DATA | WHEEL_HAS_HEALTH_DATA):
			{
				CWheelFullData *pFullWheel = (CWheelFullData *)pWheelDest;
				pFullWheel->m_DynamicData = dynamicData;
				pFullWheel->m_HealthData = healthData;
				pWheelDest = (CWheelBaseData *)(&pFullWheel[1]);
				break;
			}
		}
	}
	AddToPacketSize((tPacketSize)((ptrdiff_t)pWheelDest - (ptrdiff_t)pWheelBase));
}


//-------------------------------------------------------------------------------------------------
void CPacketVehicleWheelUpdate_Old::PrintXML(FileHandle handle) const
{
	(void)handle;
}


//-------------------------------------------------------------------------------------------------
void CPacketVehicleWheelUpdate_Old::GetWheelData(u32 wheelId, CWheelFullData &dest) const
{
	CWheelBaseData *pBase = GetCWheelBaseDataPtr(wheelId);

	// Copy over base data.
	(CWheelBaseData &)dest = *pBase;

	switch (pBase->m_TypeInfo)
	{
	case 0:
		{
			// Set defaults.
			new (&dest.m_DynamicData) CWheelDynamicData();
			new (&dest.m_HealthData) CWheelHealthData();
			break;
		}
	case WHEEL_HAS_DYNAMIC_DATA:
		{
			CWheelWithDynamicData *pWithDynamic = (CWheelWithDynamicData *)pBase;
			dest.m_DynamicData = pWithDynamic->m_DynamicData;
			new (&dest.m_HealthData) CWheelHealthData();
			break;
		}
	case  WHEEL_HAS_HEALTH_DATA:
		{
			new (&dest.m_DynamicData) CWheelDynamicData();
			CWheelWithHealthData *pWithHealth = (CWheelWithHealthData *)pBase;
			dest.m_HealthData = pWithHealth->m_HealthData;
			break;
		}
	case  (WHEEL_HAS_DYNAMIC_DATA | WHEEL_HAS_HEALTH_DATA):
		{
			CWheelFullData *pFull = (CWheelFullData *)pBase;
			dest.m_DynamicData = pFull->m_DynamicData;
			dest.m_HealthData = pFull->m_HealthData;
			break;
		}
	}
}


//-------------------------------------------------------------------------------------------------
u32 CPacketVehicleWheelUpdate_Old::EstimatePacketSize(const CVehicle *pVehicle)
{
	return sizeof(CPacketVehicleWheelUpdate_Old) + pVehicle->GetNumWheels()*sizeof(CWheelFullData);
}


//-------------------------------------------------------------------------------------------------
CWheelBaseData *CPacketVehicleWheelUpdate_Old::GetCWheelBaseDataPtr(u32 wheelId) const
{
	CWheelBaseData *pBase = GetBasePtr();

	while(wheelId--)
	{
		switch (pBase->m_TypeInfo)
		{
		case 0:
			{
				pBase = &pBase[1];
				break;
			}
		case WHEEL_HAS_DYNAMIC_DATA:
			{
				pBase = (CWheelBaseData *)(&((CWheelWithDynamicData *)pBase)[1]);
				break;
			}
		case  WHEEL_HAS_HEALTH_DATA:
			{
				pBase = (CWheelBaseData *)(&((CWheelWithHealthData *)pBase)[1]);
				break;
			}
		case  (WHEEL_HAS_DYNAMIC_DATA | WHEEL_HAS_HEALTH_DATA):
			{
				pBase = (CWheelBaseData *)(&((CWheelFullData *)pBase)[1]);
				break;
			}
		}
	}
	return pBase;
}

#else // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

void CPacketVehicleWheelUpdate_Old::Store(const CWheel* pWheel)
{
	CPacketBase::Store(PACKETID_WHEELUPDATE_OLD, sizeof(CPacketVehicleWheelUpdate_Old));
	// Safety Checks
	// - Making sure that the ranges we assume when compressing values are true
	replayAssertf( pWheel->m_fSuspensionHealth		>= 0	&& pWheel->m_fSuspensionHealth		<= 1000,"CPacketVehicleWheelUpdate_Old::Store: Suspension Health %f",		pWheel->m_fSuspensionHealth		);
	replayAssertf( pWheel->m_fTyreHealth			>= 0	&& pWheel->m_fTyreHealth			<= 1000,"CPacketVehicleWheelUpdate_Old::Store: Tyre Health %f",				pWheel->m_fTyreHealth			);
	replayAssertf( pWheel->m_fBrakeForce			>= 0	&& pWheel->m_fBrakeForce			<= 5,	"CPacketVehicleWheelUpdate_Old::Store: Brake Force %f",				pWheel->m_fBrakeForce			);

	// Base data.
	m_WheelData.m_DynamicFlags			= pWheel->m_nDynamicFlags;
	m_WheelData.m_ConfigFlags			= pWheel->m_nConfigFlags;
	m_WheelData.m_steeringAngle			= pWheel->GetSteeringAngle();
	m_WheelData.m_rotAngle				= pWheel->m_fRotAng;
	m_WheelData.m_vecHitPos.Store(pWheel->m_vecHitPos);
	m_WheelData.m_vecHitCentrePos.Store(pWheel->m_vecHitCentrePos);
	m_WheelData.m_vecHitNormal.Store(pWheel->m_vecHitNormal);

	// Dynamic data.
	m_WheelData.m_DynamicData.m_BrakeForce			= pWheel->m_fBrakeForce;
	m_WheelData.m_DynamicData.m_FrictionDamage		= pWheel->m_fFrictionDamage;
	m_WheelData.m_DynamicData.m_Compression			= pWheel->m_fCompression;
	m_WheelData.m_DynamicData.m_CompressionOld		= pWheel->m_fCompressionOld;
	m_WheelData.m_DynamicData.m_staticDelta			= pWheel->m_fStaticDelta;
	m_WheelData.m_DynamicData.m_RotAngVel			= Clamp(pWheel->m_fRotAngVel, m_WheelData.m_DynamicData.m_RotAngVel.GetMin(), m_WheelData.m_DynamicData.m_RotAngVel.GetMax());
	m_WheelData.m_DynamicData.m_EffectiveSlipAngle	= pWheel->m_fEffectiveSlipAngle;
	m_WheelData.m_DynamicData.m_wheelCompression	= pWheel->GetWheelCompression();
	m_WheelData.m_DynamicData.m_fwdSlipAngle		= pWheel->GetFwdSlipAngle();
	m_WheelData.m_DynamicData.m_sideSlipAngle		= pWheel->GetSideSlipAngle();
	m_WheelData.m_DynamicData.m_tyreLoad			= pWheel->m_fTyreLoad;
	m_WheelData.m_DynamicData.m_tyreTemp			= pWheel->m_fTyreTemp;
	m_WheelData.m_DynamicData.m_fEffectiveSlipAngle	= pWheel->m_fEffectiveSlipAngle;
	m_WheelData.m_DynamicData.m_vecGroundContactVelocity.Store(pWheel->m_vecGroundContactVelocity);
	m_WheelData.m_DynamicData.m_vecTyreContactVelocity.Store(pWheel->m_vecTyreContactVelocity);

	// Health data.
	m_WheelData.m_HealthData.m_SuspensionHealth		= pWheel->m_fSuspensionHealth;
	m_WheelData.m_HealthData.m_TyreHealth			= pWheel->m_fTyreHealth;
}

//////////////////////////////////////////////////////////////////////////
void CPacketVehicleWheelUpdate_Old::PrintXML(FileHandle handle) const
{
	(void)handle;
	/*
	CPacketBase::PrintXML(fileID);

	char str[1024];
	snprintf(str, 1024, "\t\t<SuspensionHealth>%f</SuspensionHealth>\n", m_HealthData.m_SuspensionHealth);
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<TyreHealth>%f</TyreHealth>\n", m_HealthData.m_TyreHealth);
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<BrakeForce>%f</BrakeForce>\n", GetBrakeForce());
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<FrictionDamage>%f</FrictionDamage>\n", GetFrictionDamage());
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<Compression>%f</Compression>\n", GetCompression());
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<CompressionOld>%f</CompressionOld>\n", GetCompressionOld());
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<RotAngVel>%f</RotAngVel>\n", GetRotAngVel());
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<EffectiveSlipAngle>%f</EffectiveSlipAngle>\n", GetEffectiveSlipAngle());
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<DynamicFlags>%u</DynamicFlags>\n", m_DynamicFlags);
	CFileMgr::Write(fileID, str, istrlen(str));

	snprintf(str, 1024, "\t\t<ConfigFlags>%u</ConfigFlags>\n", m_ConfigFlags);
	CFileMgr::Write(fileID, str, istrlen(str));
	*/
}

#endif // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

//========================================================================================================================
// CPacketVehicleDoorUpdate.
//========================================================================================================================

const u32 g_CPacketVehicleDoorUpdate_V1 = 1;
const u32 g_CPacketVehicleDoorUpdate_Extensions_V1 = 1;

void CPacketVehicleDoorUpdate::Store(CVehicle* pVehicle, s32 doorIdx)
{
	PACKET_EXTENSION_RESET(CPacketVehicleDoorUpdate);
	CPacketBase::Store(PACKETID_DOORUPDATE, sizeof(CPacketVehicleDoorUpdate), g_CPacketVehicleDoorUpdate_V1);

	// Door is open, record it
	int nDoorBoneIndex = pVehicle->GetBoneIndex(pVehicle->GetDoor(doorIdx)->GetHierarchyId());
	SetPadU8(0, (u8)doorIdx);

	if (pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN)
	{
		Matrix34 doorMatrix;
		pVehicle->GetGlobalMtx(nDoorBoneIndex, doorMatrix );

		Quaternion q;
		doorMatrix.ToQuaternion(q);
		m_DoorQuaternion.StoreQuaternion(q);

		m_doorTranslation.Store(doorMatrix.d);
	}
	else
	{
		const CCarDoor* pDoor = pVehicle->GetDoor(doorIdx);

		if (pDoor)
		{
			if(pDoor->GetAxis() == CCarDoor::AXIS_SLIDE_Y)
			{
				m_fTranslation = pVehicle->GetLocalMtxNonConst(nDoorBoneIndex).d.y;
			}
		}
	}

	m_doorRatio	= pVehicle->GetDoor(doorIdx)->GetDoorRatio();
	m_doorOldAudioRatio	= pVehicle->GetDoor(doorIdx)->GetOldAudioRatio();
}	

//========================================================================================================================
void CPacketVehicleDoorUpdate::StoreExtensions(const CVehicle* pVehicle, s32 doorIdx)
{ 
	PACKET_EXTENSION_RESET(CPacketVehicleDoorUpdate); 

	PacketExtensions *pExtensions = (PacketExtensions *)BEGIN_PACKET_EXTENSION_WRITE(g_CPacketVehicleDoorUpdate_Extensions_V1, CPacketVehicleDoorUpdate);
	pExtensions->m_doorFlags = pVehicle->GetDoor(doorIdx)->GetFlags();

	END_PACKET_EXTENSION_WRITE(pExtensions+1, CPacketVehicleDoorUpdate);
}


////////////////////////////////////////////////////////////
// Find out whether to update the window if there is one attached to the car door
bool UpdateWindow(CVehicle* pVehicle, CCarDoor* pCarDoor)
{
	if(!pVehicle || !pVehicle->GetSkeleton() || !pCarDoor)
		return false;

	crBoneData const* pDoorBoneData = pVehicle->GetSkeleton()->GetSkeletonData().GetBoneData(pVehicle->GetBoneIndex(pCarDoor->GetHierarchyId()));
	if(!pDoorBoneData)
		return false;

	crBoneData const* pChildBone = pDoorBoneData->GetChild();
	while(pChildBone)
	{
		if(pChildBone->GetBoneId() == CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId()))
		{
			return true;
		}

		pChildBone = pChildBone->GetNext();
	}

	return false;
}

//========================================================================================================================
bool CPacketVehicleDoorUpdate::Extract(CVehicle* pVehicle, CPacketVehicleDoorUpdate const* pNextPacket, f32 fInterp) const
{
	replayFatalAssertf(pVehicle, "Vehicle is NULL in CPacketVehicleDoorUpdate::Extract");
	ValidatePacket();

	CCarDoor* pCarDoor = pVehicle->GetDoor(GetDoorIdx());
	replayFatalAssertf(pCarDoor, "Car Door is NULL in CPacetVehicleDoorUpdate::Extract");

	int nDoorBoneIndex = pVehicle->GetBoneIndex(pCarDoor->GetHierarchyId());

	// Don't set the door matrix if the door is modded...the mod handling will sort this.
	bool setDoorMatrix = !pVehicle->GetVariationInstance().IsBoneModed(pCarDoor->GetHierarchyId());

	if(nDoorBoneIndex > -1 && setDoorMatrix)
	{
		Matrix34	doorMatrix;
		pVehicle->GetGlobalMtx(nDoorBoneIndex, doorMatrix);
		bool		isDoorMatrixSet = false;

		Matrix34	interpMatrix;
		if( fInterp )
		{
			Quaternion	prevQuat;
			Quaternion	nextQuat;
			Quaternion	interpQuat;
			Vector3 prevTranslation;
			Vector3 nextTranslation;
			Vector3 interpTranslation;

			if( pNextPacket && (pNextPacket->GetPacketID() == PACKETID_DOORUPDATE) )
			{
				if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					const crSkeleton* pSkel			  = pVehicle->GetSkeleton();
					const crSkeletonData* pSkelData	  = &pVehicle->GetSkeletonData();
					const crBoneData* pParentBoneData = pSkelData->GetBoneData(nDoorBoneIndex)->GetParent();

					Matrix34 globalMatParent;
					pSkel->GetGlobalMtx( pParentBoneData->GetIndex(), RC_MAT34V(globalMatParent)); 

					Matrix34& doorLocalMatrix = pVehicle->GetLocalMtxNonConst(nDoorBoneIndex);

					doorLocalMatrix.d.y = m_fTranslation * (1.0f - fInterp) + pNextPacket->m_fTranslation * fInterp;

					interpMatrix.Dot( doorLocalMatrix, globalMatParent );
					doorMatrix.Set(interpMatrix);

					isDoorMatrixSet = true;
				}
				else
				{
					// Interpolate Doors
					m_DoorQuaternion.LoadQuaternion(prevQuat);
					pNextPacket->m_DoorQuaternion.LoadQuaternion(nextQuat);

					if(GetPacketVersion() >= g_CPacketVehicleDoorUpdate_V1)
					{
						m_doorTranslation.Load(prevTranslation);
						pNextPacket->m_doorTranslation.Load(nextTranslation);
						interpTranslation = Lerp(fInterp, prevTranslation, nextTranslation);
					}

					// Hack for old clips here...
					// Some packets recorded on a door breaking off had a duff quaternion...
					// ...attempt to detect this here (all the components were the same and really small)
					if(!(nextQuat.x == nextQuat.y && nextQuat.x == nextQuat.z && nextQuat.x < 0.1f))
					{
						prevQuat.PrepareSlerp( nextQuat );
						interpQuat.Slerp( fInterp, prevQuat, nextQuat );
						interpMatrix.FromQuaternion( interpQuat );
						replayAssert( interpMatrix.IsOrthonormal() );

						doorMatrix.Set3x3(interpMatrix);
						isDoorMatrixSet = true;
					}
				}

				// Interpolate windows
				if (UpdateWindow(pVehicle, pCarDoor) && isDoorMatrixSet && pCarDoor->GetHierarchyId() >= VEH_DOOR_DSIDE_F && pCarDoor->GetHierarchyId() <= VEH_DOOR_PSIDE_R && !pVehicle->IsWindowDown(CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId())))
				{
					int nGlassBoneIndex = pVehicle->GetBoneIndex( CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId()) );
					if (nGlassBoneIndex > -1)
					{
						Matrix34&	glassLocalMatrix = pVehicle->GetLocalMtxNonConst(nGlassBoneIndex);
						Matrix34	glassGlobalMatrix;
						pVehicle->GetGlobalMtx(nGlassBoneIndex, glassGlobalMatrix);

						interpMatrix.Dot(glassLocalMatrix, doorMatrix);

						glassGlobalMatrix.Set(interpMatrix);
						pVehicle->SetGlobalMtx(nGlassBoneIndex, glassGlobalMatrix);
					}
				}
			}
			else if( !pNextPacket && doorMatrix.Determinant3x3() != 0.0f && m_doorRatio < 0.1f) // Don't use the door's current matrix if it's scale is zero!
			{
				// Door is About to Close
				//  Door update packets are not recorded when doors are closed,
				//  so we interpolate with the closed state.
				if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					const crSkeleton* pSkel			  = pVehicle->GetSkeleton();
					const crSkeletonData* pSkelData	  = &pVehicle->GetSkeletonData();
					const crBoneData* pParentBoneData = pSkelData->GetBoneData(nDoorBoneIndex)->GetParent();
					Matrix34 globalMatParent;
					pSkel->GetGlobalMtx(pParentBoneData->GetIndex(), RC_MAT34V(globalMatParent));

					Matrix34& doorLocalMatrix = pVehicle->GetLocalMtxNonConst(nDoorBoneIndex);

					doorLocalMatrix.d.y = m_fTranslation * (1.0f - fInterp) + doorLocalMatrix.d.y * fInterp;

					interpMatrix.Dot(doorLocalMatrix, globalMatParent);
					doorMatrix.Set(interpMatrix);

					isDoorMatrixSet = true;
				}
				else
				{
					m_DoorQuaternion.LoadQuaternion(prevQuat);

					doorMatrix.ToQuaternion(nextQuat);
					prevQuat.PrepareSlerp( nextQuat );
					interpQuat.Slerp( fInterp, prevQuat, nextQuat );
					interpMatrix.FromQuaternion( interpQuat );
					replayAssert( interpMatrix.IsOrthonormal() );

					doorMatrix.Set3x3(interpMatrix);

					if(GetPacketVersion() >= g_CPacketVehicleDoorUpdate_V1)
					{
						m_doorTranslation.Load(prevTranslation);
						nextTranslation = doorMatrix.d;
						interpTranslation = Lerp(fInterp, prevTranslation, nextTranslation);
					}

					isDoorMatrixSet = true;
				}

				// Interpolate windows
				if (UpdateWindow(pVehicle, pCarDoor) && isDoorMatrixSet && pCarDoor->GetHierarchyId() >= VEH_DOOR_DSIDE_F && pCarDoor->GetHierarchyId() <= VEH_DOOR_PSIDE_R && !pVehicle->IsWindowDown(CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId())))
				{
					int nGlassBoneIndex = pVehicle->GetBoneIndex( CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId()) );
					if (nGlassBoneIndex > -1)
					{
						Matrix34&	glassLocalMatrix = pVehicle->GetLocalMtxNonConst(nGlassBoneIndex);
						Matrix34	glassGlobalMatrix;
						pVehicle->GetGlobalMtx(nGlassBoneIndex, glassGlobalMatrix);

						interpMatrix.Dot(glassLocalMatrix, doorMatrix);

						glassGlobalMatrix.Set(interpMatrix);
						pVehicle->SetGlobalMtx(nGlassBoneIndex, glassGlobalMatrix);
					}
				}
			}
		}

		if( !isDoorMatrixSet )
		{
			if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				const crSkeleton* pSkel			  = pVehicle->GetSkeleton();
				const crSkeletonData* pSkelData	  = &pVehicle->GetSkeletonData();
				const crBoneData* pParentBoneData = pSkelData->GetBoneData(nDoorBoneIndex)->GetParent();

				Matrix34 globalMatParent;
				pSkel->GetGlobalMtx(pParentBoneData->GetIndex(), RC_MAT34V(globalMatParent));

				Matrix34& doorLocalMatrix = pVehicle->GetLocalMtxNonConst(nDoorBoneIndex);

				doorLocalMatrix.d.y = m_fTranslation;

				interpMatrix.Dot(doorLocalMatrix, globalMatParent);
				doorMatrix.Set(interpMatrix);

				isDoorMatrixSet = true;
			}
			else
			{
				Quaternion prevQuat;
				m_DoorQuaternion.LoadQuaternion(prevQuat);

				const crSkeleton* pSkel			  = pVehicle->GetSkeleton();
				const crSkeletonData* pSkelData	  = &pVehicle->GetSkeletonData();
				Quaternion localDefaultRotation = QUATV_TO_QUATERNION(pSkelData->GetBoneData(nDoorBoneIndex)->GetDefaultRotation());

				// Get the parent matrix in global space
 				Matrix34 globalMatParent;
				const crBoneData* pParentBoneData = pSkelData->GetBoneData(nDoorBoneIndex)->GetParent();
 				pSkel->GetGlobalMtx(pParentBoneData->GetIndex(), RC_MAT34V(globalMatParent));
				
				// Obtain the quaternion...
				Quaternion globalParentQuat;
				globalMatParent.ToQuaternion(globalParentQuat);

				// Multiply the local quaternion for this bone and it's parent together
				Quaternion nextQuat;
				nextQuat.Multiply(globalParentQuat, localDefaultRotation);

				// Do the slerp!
				Quaternion interpQuat;
				prevQuat.PrepareSlerp( nextQuat );
				interpQuat.Slerp( fInterp, prevQuat, nextQuat );
				interpMatrix.FromQuaternion( interpQuat );
				replayAssert( interpMatrix.IsOrthonormal() );

				doorMatrix.Set3x3(interpMatrix);
				
				if(GetPacketVersion() >= g_CPacketVehicleDoorUpdate_V1)
				{
					Vector3 prevTranslation;
					m_doorTranslation.Load(prevTranslation);
				
					Vector3 localDefaultTranslation = VEC3V_TO_VECTOR3(pSkelData->GetBoneData(nDoorBoneIndex)->GetDefaultTranslation());
					Vector3 nextTranslation;
					globalMatParent.Transform(localDefaultTranslation, nextTranslation);

					Vector3 interpTranslation = Lerp(fInterp, prevTranslation, nextTranslation);
					doorMatrix.d = interpTranslation;
				}
				
				isDoorMatrixSet = true;
			}

			// Interpolate windows
			if (UpdateWindow(pVehicle, pCarDoor) && pCarDoor->GetHierarchyId() >= VEH_DOOR_DSIDE_F && pCarDoor->GetHierarchyId() <= VEH_DOOR_PSIDE_R && !pVehicle->IsWindowDown(CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId())))
			{
				int nGlassBoneIndex = pVehicle->GetBoneIndex( CVehicle::GetWindowIdFromDoor(pCarDoor->GetHierarchyId()) ); 
				if (nGlassBoneIndex > -1)
				{
					Matrix34&	glassLocalMatrix = pVehicle->GetLocalMtxNonConst(nGlassBoneIndex);
					Matrix34	glassGlobalMatrix;
					pVehicle->GetGlobalMtx(nGlassBoneIndex, glassGlobalMatrix);

					interpMatrix.Dot(glassLocalMatrix, doorMatrix);

					glassGlobalMatrix.Set(interpMatrix);
					pVehicle->SetGlobalMtx(nGlassBoneIndex, glassGlobalMatrix);
				}
			}
		}

		if( isDoorMatrixSet )
		{
			pVehicle->SetGlobalMtx(nDoorBoneIndex, doorMatrix);
			pVehicle->GetSkeleton()->PartialInverseUpdate(nDoorBoneIndex);
			if(!CReplayMgrInternal::IsSettingUpFirstFrame())
				pCarDoor->ReplayUpdateDoors(pVehicle);
		}
	}

	pCarDoor->SetDoorRatio(m_doorRatio);

	pCarDoor->SetDoorOldAudioRatio(m_doorOldAudioRatio);

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleDoorUpdate) >= g_CPacketVehicleDoorUpdate_Extensions_V1)
	{
		//---------------------- Extensions V1 ------------------------/
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleDoorUpdate);
		pCarDoor->SetFlags(pExtensions->m_doorFlags);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleDoorUpdate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<DoorIndex>%u</DoorIndex>\n", GetDoorIdx());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<DoorRatio>%f</DoorRatio>\n", m_doorRatio);
	CFileMgr::Write(handle, str, istrlen(str));
}

//////////////////////////////////////////////////////////////////////////
void CPacketVehicleCreate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CPacketEntityInfo::PrintXML(handle);

	char str[1024];
	FrameRef fr(m_FrameCreated);
	snprintf(str, 1024, "\t\t<FrameCreated>%u:%u</FrameCreated>\n", fr.GetBlockIndex(), fr.GetFrame());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<ModelNameHash>%u</ModelNameHash>\n", GetModelNameHash());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsFirstCreationPacket>%s</IsFirstCreationPacket>\n", IsFirstCreationPacket() ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
CPacketVehDamageUpdate::CPacketVehDamageUpdate(const Vector4& rDamage, const Vector4& rOffset, CVehicle* pVehicle, eReplayPacketId packetId)
	: CPacketEvent(packetId, sizeof(CPacketVehDamageUpdate))
	, CPacketEntityInfo()
{
	m_OffsetVector.Store(rOffset.GetVector3());
	m_DamageVector.Store(rDamage);
	ReplayVehicleExtension::SetVehicleAsDamaged(pVehicle);
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehDamageUpdate::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	replayAssertf(pVehicle, "pVehicle is NULL in CPacketVehicleDamageUpdate::Extract");
	if(!pVehicle)
		return;	

	ReplayVehicleExtension *extension = ReplayVehicleExtension::GetExtension(pVehicle);
	if(!extension)
	{
		extension = ReplayVehicleExtension::Add(pVehicle);
		if(!extension)
		{
			replayAssertf(false, "Failed to add a vehicle extension...ran out?");
			return;
		}
	}	

	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		extension->RemoveDeformation();
		pVehicle->GetVehicleDamage()->GetDeformation()->ResetDamage();
		return;
	}

	Vector4 oDamageVec;
	Vector3 oOffsetVec;
	m_OffsetVector.Load(oOffsetVec);
	m_DamageVector.Load(oDamageVec);
	
	extension->StoreVehicleDeformationData(oDamageVec, oOffsetVec);

}

bool CPacketVehDamageUpdate::HasExpired(const CEventInfo<CVehicle>& info) const
{
	return ReplayVehicleExtension::IsVehicleUndamaged(info.GetEntity());
}

//////////////////////////////////////////////////////////////////////////
CPacketVehDamageUpdate_PlayerVeh::CPacketVehDamageUpdate_PlayerVeh(const Vector4& rDamage, const Vector4& rOffset, CVehicle* pVehicle)
	: CPacketVehDamageUpdate(rDamage, rOffset, pVehicle, PACKETID_VEHICLEDMGUPDATE_PLAYERVEH)
{
}


//-------------------------------------------------------------------------------------------------
void CPacketVehicleUpdate::StoreSpeed(const Vector3& rSpeed)
{
	// Max speed = 120 m/s = 432km/h
	const float SCALAR = (32767.0f / 120.0f);

	m_Speed[0] = (s16)(rSpeed.x * SCALAR);
	m_Speed[1] = (s16)(rSpeed.y * SCALAR);
	m_Speed[2] = (s16)(rSpeed.z * SCALAR);
}

//-------------------------------------------------------------------------------------------------
void CPacketVehicleUpdate::ExtractSpeed(Vector3& rSpeed) const
{
	// Max speed = 120 m/s = 432km/h
	const float SCALAR = (32767.0f / 120.0f);

	rSpeed.x = m_Speed[0] / SCALAR;
	rSpeed.y = m_Speed[1] / SCALAR;
	rSpeed.z = m_Speed[2] / SCALAR;
}

//========================================================================================================================
void CPacketVehicleUpdate::RemoveAndAdd(CVehicle* pVehicle, const fwInteriorLocation& interiorLoc, u32 playbackFlags, bool inInterior, bool bIgnorePhysics) const
{
	//todo4five temp as these are causing issues in the final builds
	(void) bIgnorePhysics;

	if((playbackFlags & REPLAY_CURSOR_JUMP) == 0)
	{
		pVehicle->UpdatePortalTracker();
	}

	if(inInterior)
	{
		CGameWorld::Remove(pVehicle);

		//only add to an interior that has been correctly populated, otherwise bad things happen, well it crashes :)
		CInteriorInst* pInteriorInst = CInteriorInst::GetInteriorForLocation( interiorLoc );
		if(pInteriorInst && pInteriorInst->CanReceiveObjects())
			CGameWorld::Add(pVehicle, interiorLoc);
	}
	else if (!pVehicle->GetIsInInterior())
	{
		CGameWorld::Remove(pVehicle);
		CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleUpdate::Print() const
{
	//replayDebugf1("Vehicle Type %u", GetVehicleType());
}

//========================================================================================================================
void CPacketVehicleUpdate::Store(eReplayPacketId uPacketID, u32 packetSize, const CVehicle* pVehicle, bool firstUpdatePacket, tPacketVersion derivedPacketVersion)
{
	PACKET_EXTENSION_RESET(CPacketVehicleUpdate);
	REPLAY_UNUSEAD_VAR(m_HasDriver);
	REPLAY_UNUSEAD_VAR(m_EngineStarting);
	REPLAY_UNUSEAD_VAR(m_bDrawEmissive);
	REPLAY_UNUSEAD_VAR(m_bShadowProxy);

	replayAssert(pVehicle);

	Assign(m_VehiclePacketUpdateSize, packetSize);

	CPacketBase::Store(uPacketID, packetSize, derivedPacketVersion);
	CPacketInterp::Store();
	CBasicEntityPacketData::Store(pVehicle, pVehicle->GetReplayID());

	m_unused0 = 0;

	SetIsFirstUpdatePacket(firstUpdatePacket);

	// Safety Checks
	// - Making sure that the ranges we assume when compressing values are true
	replayAssertf(pVehicle->m_Transmission.m_nGear		>= 0	&& pVehicle->m_Transmission.m_nGear			<= 7,		"CPacketVehicleUpdate::Store: Gear is %d",			pVehicle->m_Transmission.m_nGear		);
	replayAssertf(pVehicle->m_Transmission.m_fClutchRatio >= 0	&& pVehicle->m_Transmission.m_fClutchRatio	<= 1,		"CPacketVehicleUpdate::Store: Clutch Ratio is %f", pVehicle->m_Transmission.m_fClutchRatio);
	replayAssertf(pVehicle->m_Transmission.m_fThrottle	>= -1	&& pVehicle->m_Transmission.m_fThrottle		<= 1,		"CPacketVehicleUpdate::Store: Throttle is %f",		pVehicle->m_Transmission.m_fThrottle	);
	replayAssertf(pVehicle->m_Transmission.m_fFireEvo	>= 0	&& pVehicle->m_Transmission.m_fFireEvo		<= 1,		"CPacketVehicleUpdate::Store: Fire Evo is %f",		pVehicle->m_Transmission.m_fFireEvo	);
	//replayAssertf(pVehicle->pHandling->m_fMass			>= 0	&& pVehicle->pHandling->m_fMass				<= 50000,	"CPacketVehicleUpdate::Store: Mass is %f",			pVehicle->pHandling->m_fMass			);				// Submarine mass is large
	//replayAssertf(pVehicle->pHandling->m_fInitialDriveMaxVel >= 0	&& pVehicle->pHandling->m_fInitialDriveMaxVel		<= 55,		"CPacketVehicleUpdate::Store: Drive Max Vel is %f", pVehicle->pHandling->m_fInitialDriveMaxVel	);	// Getting this one a lot

	StoreSpeed(pVehicle->GetVelocity());

	m_SteerAngle	= (s8)(pVehicle->GetSteerAngle()	* FLOAT_TO_S8_FACTOR_20);
	m_GasPedal		= (s8)(pVehicle->GetThrottle()	* FLOAT_TO_S8_FACTOR_100);
	m_Brake			= (s8)(pVehicle->GetBrake()	* FLOAT_TO_S8_FACTOR_100);
	m_bHandBrake	= pVehicle->GetHandBrake();

	m_bDrawDecal	= pVehicle->IsBaseFlagSet(fwEntity::HAS_DECAL);
	m_bDrawAlpha	= pVehicle->IsBaseFlagSet(fwEntity::HAS_ALPHA);
	m_bDrawCutout	= pVehicle->IsBaseFlagSet(fwEntity::HAS_CUTOUT);

	m_status		= pVehicle->GetStatus();
	
	const fwFlags16 &flags = pVehicle->GetRenderPhaseVisibilityMask();
	m_bDontCastShadows = !flags.IsFlagSet(VIS_PHASE_MASK_CASCADE_SHADOWS) && !flags.IsFlagSet(VIS_PHASE_MASK_PARABOLOID_SHADOWS);

	//TODO4FIVE m_bShadowProxy = pVehicle->m_nFlags.bShadowProxy;

	m_bLightsIgnoreDayNightSetting = pVehicle->m_nFlags.bLightsIgnoreDayNightSetting;
	m_bLightsCanCastStaticShadows = pVehicle->m_nFlags.bLightsCanCastStaticShadows;
	m_bLightsCanCastDynamicShadows = pVehicle->m_nFlags.bLightsCanCastDynamicShadows;

	// Moved into the extension now so we can support more lights...left so it won't break anything but this will be ignored on playback
	ClearLightStates();
	if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
	{
		for (u32 uStateIdx = 0; uStateIdx < 14; uStateIdx++)
		{
			StoreLightState(pVehicle->GetVehicleDamage()->GetLightStateImmediate(uStateIdx), uStateIdx);
		}
	}

	ClearSirenStates();
	if (pVehicle->UsesSiren())
	{
		for (u32 uStateIdx = 0; uStateIdx < 8; uStateIdx++)
		{
			StoreSirenState(pVehicle->GetVehicleDamage()->GetSirenStateImmediate(uStateIdx), uStateIdx);
		}
	}

	//	Updated after storing the door packets
	m_bContainsFrag			= false;
	m_bHasDamageBones		= false;
	m_bHasModBoneData		= false; 
	m_DoorCount				= 0;
	m_WheelCount			= 0;
	m_WheelHitMaterialId	= 0;
	m_WheelRadiusRear		= 0;
	m_WheelRadiusFront		= 0;
	m_bIsTyreDeformOn		= 0;
	
	if (pVehicle->GetVehicleDamage() && pVehicle->GetVehicleDamage()->GetDeformation() && pVehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
	{
		m_HasDamage = 1;
	}
	else
	{
		m_HasDamage = 0;
	}

	m_bRenderScorched = pVehicle->m_nPhysicalFlags.bRenderScorched;

	sysMemCpy(m_VehicleFlags, &(pVehicle->m_nVehicleFlags), sizeof(m_VehicleFlags));

	m_VehicleTypeOld = 0;//(u8)pVehicle->GetVehicleType();

	m_IsPlayerControlled			= pVehicle->GetDriver()	&& 
									  pVehicle->GetDriver()->IsLocalPlayer();

	IsPossiblyTouchesWater	= pVehicle->m_nFlags.bPossiblyTouchesWater;

	// Horn / Sirens / Car Alarms
	m_CarAlarmState					= pVehicle->IsAlarmActivated();
	m_IsSirenForcedOn				= pVehicle->GetVehicleAudioEntity()->IsSirenForced();
	m_mustPlaySiren					= pVehicle->GetVehicleAudioEntity()->GetMustPlaySiren();
	m_alarmType						= static_cast<u8> (pVehicle->GetVehicleAudioEntity()->m_AlarmType);
 	m_IsSirenFucked					= pVehicle->GetVehicleAudioEntity()->m_IsSirenFucked;
 	m_UseSirenForHorn				= pVehicle->GetVehicleAudioEntity()->m_UseSirenForHorn;
	m_PlayerHornOn					= pVehicle->GetVehicleAudioEntity()->m_PlayerHornOn;
 	

	m_Revs							= pVehicle->GetVehicleAudioEntity()->GetReplayRevs();

	// Transmission
	m_EngineHealth		= pVehicle->m_Transmission.m_fEngineHealth;
	m_Gear				= static_cast<s8> (pVehicle->m_Transmission.m_nGear);
	m_ClutchRatio		= static_cast<u8> (pVehicle->m_Transmission.m_fClutchRatio	* FLOAT_TO_U8_RANGE_0_TO_1);
	m_Throttle			= static_cast<s8> (pVehicle->m_Transmission.m_fThrottle		* FLOAT_TO_S8_RANGE_NEG_1_TO_1);
	m_FireEvo			= static_cast<u8> (pVehicle->m_Transmission.m_fFireEvo		* FLOAT_TO_U8_RANGE_0_TO_1);
	m_revRatio			= pVehicle->m_Transmission.m_fRevRatio;

	// Handling Data
	m_Mass				= static_cast<u16> (pVehicle->pHandling->m_fMass * STORE_MASS_FACTOR);
	m_DriveMaxVel		= pVehicle->pHandling->m_fInitialDriveMaxVel;

	m_EngineSwitchedOnTime = pVehicle->m_EngineSwitchedOnTime;
	m_IgnitionHoldTime = pVehicle->GetVehicleAudioEntity()->GetIgnitionHoldTime();
	m_HotWiringVehicle = false;
	m_iNumGadgets = 0;	

	const audVehicleAudioEntity* pVehicleAudioEntity = pVehicle->GetVehicleAudioEntity();
	if(pVehicleAudioEntity )
	{
		StoreVehicleGadgetData(pVehicleAudioEntity);

		if(pVehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		{
			const audVehicleEngine* pVehicleEngine = static_cast<const audCarAudioEntity*>(pVehicleAudioEntity)->GetVehicleEngine();
			if(pVehicleEngine)
			{
				m_HotWiringVehicle = pVehicleEngine->IsHotwiringVehicle();
			}
		}
	}

	m_ExtrasTurnedOn = 0;

	for(int i=VEH_EXTRA_1; i<=VEH_LAST_EXTRA; i++)
	{
		if(pVehicle->GetIsExtraOn((eHierarchyId)i))
			m_ExtrasTurnedOn |= BIT(i - VEH_EXTRA_1);
	}

	m_IsInWater = pVehicle->GetIsInWater();
	m_IsDummy = pVehicle->IsDummy();
	m_IsVisible = pVehicle->GetIsVisibleInSomeViewportThisFrame();

	PACKET_EXTENSION_RESET(CPacketVehicleUpdate);
}


const u32 g_CPacketVehicleUpdate_Extensions_V1 = 1;
const u32 g_CPacketVehicleUpdate_Extensions_V2 = 2;
const u32 g_CPacketVehicleUpdate_Extensions_V3 = 3;
const u32 g_CPacketVehicleUpdate_Extensions_V4 = 4;
const u32 g_CPacketVehicleUpdate_Extensions_V5 = 5;
const u32 g_CPacketVehicleUpdate_Extensions_V6 = 6;
const u32 g_CPacketVehicleUpdate_Extensions_V7 = 7;
const u32 g_CPacketVehicleUpdate_Extensions_V8 = 8;
const u32 g_CPacketVehicleUpdate_Extensions_V9 = 9;
const u32 g_CPacketVehicleUpdate_Extensions_V10 = 10;
const u32 g_CPacketVehicleUpdate_Extensions_V11 = 11;
const u32 g_CPacketVehicleUpdate_Extensions_V12 = 12;
const u32 g_CPacketVehicleUpdate_Extensions_V13 = 13;
const u32 g_CPacketVehicleUpdate_Extensions_V14 = 14;
const u32 g_CPacketVehicleUpdate_Extensions_V15 = 15;
const u32 g_CPacketVehicleUpdate_Extensions_V16 = 16;
const u32 g_CPacketVehicleUpdate_Extensions_V17 = 17;
const u32 g_CPacketVehicleUpdate_Extensions_V18 = 18;
const u32 g_CPacketVehicleUpdate_Extensions_V19 = 19;
const u32 g_CPacketVehicleUpdate_Extensions_V20 = 20;
void CPacketVehicleUpdate::StoreExtensions(eReplayPacketId uPacketID, u32 packetSize, const CVehicle* pVehicle, bool firstUpdatePacket, tPacketVersion derivedPacketVersion) 
{ 
	PACKET_EXTENSION_RESET(CPacketVehicleUpdate); 

	//---------------------- Extensions V1 ------------------------/
	PacketExtensions *pExtensions = (PacketExtensions *)BEGIN_PACKET_EXTENSION_WRITE(g_CPacketVehicleUpdate_Extensions_V20, CPacketVehicleUpdate);
	u8* pAfterExtensionHeader = (u8*)(pExtensions+1);

	// Store a high resolution rotation (needed to prevent jitter in FP mode).
	const Matrix34 &rMatrix = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());

	if(rMatrix.a.IsZero() && rMatrix.b.IsZero() && rMatrix.c.IsZero())
	{
		pExtensions->m_Quaternion.SetZero();
	}
	else
	{
		Quaternion q;
		rMatrix.ToQuaternion(q);
		pExtensions->m_Quaternion.StoreQuaternion(q);
	}

	//---------------------- Extensions V2 ------------------------/
	pExtensions->m_scorchValue = 0;
	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	replayAssert(pShader);
	if(pShader)
		pExtensions->m_scorchValue	= (u8)(pShader->GetBurnoutLevel() * 255.0f);

	//---------------------- Extensions V3 ------------------------/

	pExtensions->m_bodyDirtLevel = static_cast<u8>( static_cast<u32>(pVehicle->GetBodyDirtLevel())&0x0F );

	//---------------------- Extensions V4 ------------------------/
	pExtensions->m_alphaOverrideValue = 0;
	pExtensions->m_hasAlphaOverride = CPostScan::GetAlphaOverride(pVehicle, (int&)pExtensions->m_alphaOverrideValue);

	//---------------------- Extensions V5/V6 ------------------------/
	pExtensions->m_modBoneDataCount = 0;
	pExtensions->m_modBoneDataOffset = 0;
	pExtensions->m_modBoneZeroedCount = 0;

	const CVehicleVariationInstance& variationInstance = pVehicle->GetVariationInstance();
	if (variationInstance.GetKitIndex() != INVALID_VEHICLE_KIT_INDEX && variationInstance.GetVehicleRenderGfx())
	{
		Assign(pExtensions->m_modBoneDataOffset, pAfterExtensionHeader - (u8*)pExtensions);

		for (u8 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS ; ++i)
		{
			s32 boneID = -1;
			if(i < VMT_RENDERABLE)
			{
				boneID = variationInstance.GetBone(i);
			}
			else
			{
				boneID = variationInstance.GetBoneForLinkedMod(i - VMT_RENDERABLE);
			}

			if(boneID == -1)
				continue;

			// Only record bonnet or steering related
			if(i != VMT_BONNET && i != VMT_STEERING && boneID != VEH_BONNET)
			{
				bool doContinue = true;
				// Fix for url:bugstar:5804346 - the mod seems to bypass the 'boot'/'trunk' that would normally work.
				if(i == VMT_SPOILER)
				{
					if(pVehicle->GetModelNameHash() == ATSTRINGHASH("NEBULA", 0xcb642637) ||
						pVehicle->GetModelNameHash() == ATSTRINGHASH("YOSEMITE3", 0x409D787) ||
						pVehicle->GetModelNameHash() == ATSTRINGHASH("GLENDALE2", 0xc98bbad6) ||
						pVehicle->GetModelNameHash() == ATSTRINGHASH("VIGERO2", 0x973141FC)) // Fix for url:bugstar:7760187
					{
						doContinue = false;
					}
				}

				// Fix for url:bugstar:6629467...record the doors for the yosemite3
				if(pVehicle->GetModelNameHash() == ATSTRINGHASH("YOSEMITE3", 0x409D787))
				{
					if(i == VMT_DOOR_L || i == VMT_DOOR_R)
					{
						doContinue = false;
					}
				}

				// Fix for url:bugstar:7761288 - handle the 4 doors
				if(pVehicle->GetModelNameHash() == ATSTRINGHASH("DRAUGUR", 0xD235A4A6))
				{
					if(i == VMT_SKIRT || i == VMT_RENDERABLE || i == VMT_RENDERABLE + 1 || i == VMT_RENDERABLE + 2)
					{
						doContinue = false;
					}
				}

				if(doContinue)
				{
					continue;
				}
			}

 			if(i == VMT_BONNET)
 			{
 				eHierarchyId doorBone = VEH_BONNET;
 				// url:bugstar:4519955 - on these vehicles the 'front door' bone is the 'boot', but the mod is still the 'bonnet'.......
 				u32 vehicleModelHash = pVehicle->GetModelNameHash();
 				if(vehicleModelHash == 0x59A9E570 /*torero*/ || vehicleModelHash == 0x71CBEA98 /*gb200*/ || vehicleModelHash == 0xE8A8BA94 /*viseris*/ || vehicleModelHash == 0x3e3d1f59 /*"thrax"*/)
 					doorBone = VEH_BOOT;
 
 				// Special handling for the bonnet as it sometimes had noticable 'jiggle'....don't bother recording a different position if it's closed.
 				const CCarDoor* pBonnet = pVehicle->GetDoorFromId(doorBone);
 				if(!pBonnet || (pBonnet->GetDoorRatio() == 0.0f))
 					continue;	// Don't record if the bonnet was fully closed...keeps the exact correct position on playback
 			}

			
			const Matrix33&	mat = variationInstance.GetVehicleRenderGfx()->GetMatrix(i);
			Vector3 pos = variationInstance.GetBonePosition(i);

			Matrix34 m(Matrix34::IdentityType);
			m.Set(mat.GetVector(0), mat.GetVector(1), mat.GetVector(2));
			Quaternion q; q.FromMatrix34(m);

			s32* pBoneID = (s32*)pAfterExtensionHeader;
			*pBoneID = boneID;
			pAfterExtensionHeader += sizeof(s32);

			CPacketVector<4>* pRotation = (CPacketVector<4>*)pAfterExtensionHeader;
				
			pRotation->Store(q);
			pAfterExtensionHeader += sizeof(CPacketVector<4>);

			CPacketVector<3>* pPos = (CPacketVector<3>*)pAfterExtensionHeader;
			pPos->Store(pos);
			pAfterExtensionHeader += sizeof(CPacketVector<3>);

			++(pExtensions->m_modBoneDataCount);
		}

		// Go through any mods and see if they've been zero'd out
		// If so the part was broken off so add the mod index to a list...
		for (u8 i = 0; i < VMT_RENDERABLE; ++i)
		{
			int iCount = variationInstance.GetNumBonesToTurnOffForSlot(i);
			if(iCount)
			{
				const Matrix33&	mat = variationInstance.GetVehicleRenderGfx()->GetMatrix(i);
				// Is the scale zero?
				if(mat.GetVector(0).GetX() == 0.0f && mat.GetVector(1).GetY() == 0.0f && mat.GetVector(2).GetZ() == 0.0f)
				{
					u8* pIndex = (u8*)pAfterExtensionHeader;
					*pIndex = i;	// Index added to the list...
					pAfterExtensionHeader += sizeof(u8);

					++(pExtensions->m_modBoneZeroedCount);
				}
			}
		}
	}

	//---------------------- Extensions V8 ------------------------/
	pExtensions->ClearLightStates();
	if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
	{
		for (u32 uStateIdx = 0; uStateIdx < VEHICLE_LIGHT_COUNT; uStateIdx++)
		{
			pExtensions->StoreLightState(pVehicle->GetVehicleDamage()->GetLightStateImmediate(uStateIdx), uStateIdx);
		}
	}

	//---------------------- Extensions V9 ------------------------/
	pExtensions->m_manifoldPressure = pVehicle->m_Transmission.GetManifoldPressure();


	//---------------------- Extensions V10 ------------------------/
	replayAssert((VEH_LAST_WINDOW - VEH_FIRST_WINDOW) < 8);
	pExtensions->m_windowsRolledDown = 0;
	for(int i = 0; i < VEH_LAST_WINDOW - VEH_FIRST_WINDOW; ++i)
	{
		if(pVehicle->IsWindowDown((eHierarchyId)(VEH_FIRST_WINDOW + i)))
			pExtensions->m_windowsRolledDown |= 1 << i;
	}

	//---------------------- Extensions V11 ------------------------/
	pExtensions->m_uvAnimValue2X = 0.0f;
	pExtensions->m_uvAnimValue2Y = 0.0f;

	CCustomShaderEffectVehicle* pShaderFx =	static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	if(pShaderFx && pVehicle->m_nVehicleFlags.bEngineOn)
	{
		pExtensions->m_uvAnimValue2X = pShaderFx->Track2UVAnimGet().x;
		pExtensions->m_uvAnimValue2Y = pShaderFx->Track2UVAnimGet().y;
	}

	//---------------------- Extensions V12 ------------------------/
	pExtensions->m_emissiveMultiplier = 1.0f;
	if(pShaderFx)
		pExtensions->m_emissiveMultiplier = pShaderFx->GetUserEmissiveMultiplier();

	//---------------------- Extensions V13 ------------------------/
	pExtensions->m_isBoosting = pVehicle->IsRocketBoosting();
	pExtensions->m_boostCapacity = pVehicle->GetRocketBoostCapacity();
	pExtensions->m_boostRemaining = pVehicle->GetRocketBoostRemaining();
	
	//---------------------- Extensions V14 ------------------------/
	pExtensions->m_uvAmmoAnimValue2X = 0.0f;
	pExtensions->m_uvAmmoAnimValue2Y = 0.0f;

	if(pShaderFx)
	{
		pExtensions->m_uvAmmoAnimValue2X = pShaderFx->TrackAmmoUVAnimGet().x;
		pExtensions->m_uvAmmoAnimValue2Y = pShaderFx->TrackAmmoUVAnimGet().y;
	}

	//---------------------- Extensions V15 ------------------------/
	// Doesn't do anything new....used to determine whether to apply the vehicle suspension to windows


	//---------------------- Extensions V16 ------------------------/
	pExtensions->m_specialFlightModeRatio = pVehicle->GetSpecialFlightModeRatio();

	//---------------------- Extensions V17 ------------------------/
	pExtensions->m_specialFlightModeAngularVelocity = pVehicle->GetVehicleAudioEntity()->GetCachedAngularVelocity().Mag();

	//---------------------- Extensions V19 ------------------------/
	pExtensions->ClearSirenStates();
	if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT ||
		pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
	{
		for (u32 uStateIdx = 0; uStateIdx < VEHICLE_SIREN_COUNT; uStateIdx++)
		{
			pExtensions->StoreSirenState(pVehicle->GetVehicleDamage()->GetSirenStateImmediate(uStateIdx), uStateIdx);
		}
	}

	//---------------------- Extensions V20 ------------------------/
	Assign(pExtensions->m_buoyancyState, pVehicle->m_Buoyancy.GetStatus());

	END_PACKET_EXTENSION_WRITE(pAfterExtensionHeader, CPacketVehicleUpdate);

	(void)uPacketID; 
	(void)packetSize; 
	(void)pVehicle; 
	(void)firstUpdatePacket; 
	(void)derivedPacketVersion; 
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleUpdate::UpdateAfterPartsRecorded(const CVehicle* pVehicle, int numWheelPackets, int numDoorPackets, int numFragPackets)
{
	m_WheelCount		= (u8)numWheelPackets;
	if( m_WheelCount > 0 )
	{
		m_WheelHitMaterialId			= (u8) (pVehicle->GetWheel(0)->m_nHitMaterialId & 0xFF);
		bool isWheelRadiusFrontStored	= false;
		bool isWheelRadiusRearStored	= false;

		for( int i = 0; (i < m_WheelCount) && !(isWheelRadiusFrontStored && isWheelRadiusRearStored); ++i )
		{
			if( !isWheelRadiusRearStored && pVehicle->GetWheel(i)->GetConfigFlags().IsFlagSet( WCF_REARWHEEL) )
			{
				m_WheelRadiusRear		= (u8) (pVehicle->GetWheel(i)->m_fWheelRadius * FLOAT_TO_U8_RANGE_0_TO_1);
				isWheelRadiusRearStored = true;
			}
			else if( !isWheelRadiusFrontStored && !pVehicle->GetWheel(i)->GetConfigFlags().IsFlagSet(WCF_REARWHEEL) )
			{
				m_WheelRadiusFront		 = (u8) (pVehicle->GetWheel(i)->m_fWheelRadius * FLOAT_TO_U8_RANGE_0_TO_1);
				isWheelRadiusFrontStored = true;
			}
		}

		// Check for tyre deform
		CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
		replayAssert(pShader);
		m_bIsTyreDeformOn = (u8)pShader->GetEnableTyreDeform();
	}

	m_DoorCount			= (u8)numDoorPackets;

	m_bContainsFrag		= numFragPackets > 0;
	m_bHasDamageBones	= numFragPackets > 1;
	
	m_bHasModBoneData	= false;
}


void CPacketVehicleUpdate::StoreVehicleGadgetData(const audVehicleAudioEntity* pVehicleAudioEntity)
{
	u8* pPositionsDest = (u8*)this + m_VehiclePacketUpdateSize;
	const atArray<audVehicleGadget*>& pGadgets = pVehicleAudioEntity->GetVehicleGadgets();
	for(int i = 0; i< pGadgets.GetCount(); ++i)
	{		
		tPacketSize packetSize = CreateVehicleGadgetData(pGadgets[i], pPositionsDest);

		if(packetSize > 0)
		{
			AddToPacketSize(packetSize);
			pPositionsDest += packetSize;

			m_iNumGadgets++;
		}
	}
}

tPacketSize CPacketVehicleUpdate::CreateVehicleGadgetData(audVehicleGadget* pGadget, u8* pPointerDest)
{
	switch(pGadget->GetType())
	{
		case audVehicleGadget::AUD_VEHICLE_GADGET_TURRET:
			{
				const audVehicleTurret* pTurret = static_cast<const audVehicleTurret*>(pGadget);
				(reinterpret_cast<CVehicleGadgetTurretData*>(pPointerDest))->Store(pTurret->GetAngularVelocity());
				return sizeof(CVehicleGadgetTurretData);
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_FORKS:
			{
				const audVehicleForks* pForks = static_cast<const audVehicleForks*>(pGadget);
				(reinterpret_cast<CVehicleGadgetForkData*>(pPointerDest))->Store(pForks->GetSpeed(), pForks->GetDesiredAcceleration());
				return sizeof(CVehicleGadgetForkData);
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER:
			{
				const audVehicleDigger* pDigger = static_cast<const audVehicleDigger*>(pGadget);
				CVehicleGadgetDiggerData* pDiggerData = reinterpret_cast<CVehicleGadgetDiggerData*>(pPointerDest);
				for(s16 i = 0; i < DIGGER_JOINT_MAX; ++i)
				{
					pDiggerData->Store(i, pDigger->GetJointSpeed(i), pDigger->GetJointDesiredAcceleration(i));
				}
				return sizeof(CVehicleGadgetDiggerData);
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_TOWTRUCK_ARM:
			{
				const audVehicleTowTruckArm* pTowTruckArm = static_cast<const audVehicleTowTruckArm*>(pGadget);
				(reinterpret_cast<CVehicleGadgetTowTruckArmData*>(pPointerDest))->Store(pTowTruckArm->GetSpeed(), pTowTruckArm->GetDesiredAcceleration(), pTowTruckArm->GetTowedVehicleAngle(), pTowTruckArm->GetHookPosition());
				return sizeof(CVehicleGadgetTowTruckArmData);
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_GRAPPLING_HOOK:
			{
				const audVehicleGrapplingHook* pGrapplingHook = static_cast<const audVehicleGrapplingHook*>(pGadget);
				(reinterpret_cast<CVehicleGadgetGrapplingHookData*>(pPointerDest))->Store(pGrapplingHook->GetEntityAttached(), pGrapplingHook->GetTowedVehicleAngle(), pGrapplingHook->GetHookPosition());
				return sizeof(CVehicleGadgetGrapplingHookData);
			}

		default:
		{
			break;
		}
	}

	return 0;
}

void CPacketVehicleUpdate::ExtractVehicleGadget(audVehicleAudioEntity* pVehicleAudioEntity, CVehicleGadgetDataBase* pGadget) const
{
	switch(pGadget->GetGadgetType())
	{
		case audVehicleGadget::AUD_VEHICLE_GADGET_TURRET:
		{
			const atArray<audVehicleGadget*>& pGadgets = pVehicleAudioEntity->GetVehicleGadgets();
			for(int i = 0; i < pGadgets.GetCount(); ++i)
			{
				if(pGadgets[i]->GetType() == audVehicleGadget::AUD_VEHICLE_GADGET_TURRET)
				{
					Vector3 fAngularVelocity;
					(reinterpret_cast<CVehicleGadgetTurretData*>(pGadget))->GetAngularVeloctiy(fAngularVelocity);

					static_cast<audVehicleTurret*>(pGadgets[i])->SetAngularVelocity(VECTOR3_TO_VEC3V(fAngularVelocity));
					break;
				}
			}
			break;
		}

		case audVehicleGadget::AUD_VEHICLE_GADGET_FORKS:
			{
				const atArray<audVehicleGadget*>& pGadgets = pVehicleAudioEntity->GetVehicleGadgets();
				for(int i = 0; i < pGadgets.GetCount(); ++i)
				{
					if(pGadgets[i]->GetType() == audVehicleGadget::AUD_VEHICLE_GADGET_FORKS)
					{
						CVehicleGadgetForkData* pForks = (reinterpret_cast<CVehicleGadgetForkData*>(pGadget));
						static_cast<audVehicleForks*>(pGadgets[i])->SetSpeed(pForks->GetSpeed(), pForks->GetDesiredAcceleration());
						break;
					}
				}
				break;
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER:
			{
				const atArray<audVehicleGadget*>& pGadgets = pVehicleAudioEntity->GetVehicleGadgets();
				for(int i = 0; i < pGadgets.GetCount(); ++i)
				{
					if(pGadgets[i]->GetType() == audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER)
					{
						CVehicleGadgetDiggerData* pDiggerData = (reinterpret_cast<CVehicleGadgetDiggerData*>(pGadget));
						audVehicleDigger* pDigger = static_cast<audVehicleDigger*>(pGadgets[i]);
						for(s16 i = 0; i < DIGGER_JOINT_MAX; ++i)
						{
							pDigger->SetSpeed(i,pDiggerData->GetJointSpeed(i), pDiggerData->GetJointDesiredAcceleration(i));
						}
						break;
					}
				}
				break;
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_TOWTRUCK_ARM:
			{
				const atArray<audVehicleGadget*>& pGadgets = pVehicleAudioEntity->GetVehicleGadgets();
				for(int i = 0; i < pGadgets.GetCount(); ++i)
				{
					if(pGadgets[i]->GetType() == audVehicleGadget::AUD_VEHICLE_GADGET_TOWTRUCK_ARM)
					{
						CVehicleGadgetTowTruckArmData* pTowTruckData = (reinterpret_cast<CVehicleGadgetTowTruckArmData*>(pGadget));
						audVehicleTowTruckArm* pTowTruck = static_cast<audVehicleTowTruckArm*>(pGadgets[i]);
						pTowTruck->SetSpeed(pTowTruckData->GetSpeed(), pTowTruckData->GetDesiredAcceleration());
						Vector3 position;
						pTowTruckData->GetHookPosition(position);
						pTowTruck->SetHookPosition(position);
						pTowTruck->SetTowedVehicleAngle(pTowTruckData->GetTowVehicleAngle());
						break;
					}
				}
				break;
			}

		case audVehicleGadget::AUD_VEHICLE_GADGET_GRAPPLING_HOOK:
			{
				bool bhasGadget = false;
				const atArray<audVehicleGadget*>& pGadgets = pVehicleAudioEntity->GetVehicleGadgets();
				for(int i = 0; i < pGadgets.GetCount(); ++i)
				{
					if(pGadgets[i]->GetType() == audVehicleGadget::AUD_VEHICLE_GADGET_GRAPPLING_HOOK)
					{
						CVehicleGadgetGrapplingHookData* pGrapplingHookData = (reinterpret_cast<CVehicleGadgetGrapplingHookData*>(pGadget));
						audVehicleGrapplingHook* pGrapplingHook = static_cast<audVehicleGrapplingHook*>(pGadgets[i]);
						Vector3 position;
						pGrapplingHookData->GetHookPosition(position);
						pGrapplingHook->SetHookPosition(position);
						pGrapplingHook->SetTowedVehicleAngle(pGrapplingHookData->GetTowVehicleAngle());
						pGrapplingHook->SetEntityIsAttached(pGrapplingHookData->GetEntityAttached());
						bhasGadget = true;
						break;
					}
				}
				break;
			}

		default:
		{
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleUpdate::PreUpdate(const CInterpEventInfo<CPacketVehicleUpdate, CVehicle>& info) const
{
	CVehicle* pVehicle = info.GetEntity();
	if (pVehicle)
	{
		const CPacketVehicleUpdate* pNextPacket = info.GetNextPacket();
		float interp = info.GetInterp();
		CBasicEntityPacketData::Extract(pVehicle, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags());
		ExtractExtensions(pVehicle, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags());
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleUpdate::Extract(const CInterpEventInfo<CPacketVehicleUpdate, CVehicle>& info) const
{
	CVehicle* pVehicle = info.GetEntity();
	if (pVehicle)
	{
		const CPacketVehicleUpdate* pNextPacket = info.GetNextPacket();
		float interp = info.GetInterp();
		CBasicEntityPacketData::Extract(pVehicle, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags());
		ExtractExtensions(pVehicle, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, info.GetPlaybackFlags());

		// Extract Values That Can Be Interpolated
		if (pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || interp == 0.0f || IsNextDeleted())
		{
			ExtractInterpolateableValues(pVehicle);
		}
		else if (HasNextOffset())
		{
			ExtractInterpolateableValues(pVehicle, pNextPacket, interp);
		}
		else
		{
			replayAssert(0 && "CPacketVehicleUpdate::Load - interpolation couldn't find a next packet");
		}

		if(pVehicle->GetVehicleLodState() != VLS_HD_NA)
		{
			pVehicle->Update_HD_Models();
		}

		pVehicle->SetAngVelocity(Vector3(0.0f,0.0f,0.0f));

		replayAssert(ABS(pVehicle->GetMatrix().GetCol3().GetXf()) < 20000.0f);
		replayAssert(ABS(pVehicle->GetMatrix().GetCol3().GetYf()) < 20000.0f);
		replayAssert(ABS(pVehicle->GetMatrix().GetCol3().GetZf()) < 20000.0f);

		pVehicle->SetHandBrake( m_bHandBrake );

		pVehicle->AssignBaseFlag(fwEntity::HAS_DECAL, m_bDrawDecal);
		pVehicle->AssignBaseFlag(fwEntity::HAS_ALPHA, m_bDrawAlpha);
		pVehicle->AssignBaseFlag(fwEntity::HAS_CUTOUT, m_bDrawCutout);
		pVehicle->GetRenderPhaseVisibilityMask().ClearFlag( m_bDontCastShadows ? VIS_PHASE_MASK_SHADOWS : 0 );

		/*TODO4FIVE pVehicle->m_nFlags.bShadowProxy = m_bShadowProxy;*/
		pVehicle->m_nFlags.bLightsIgnoreDayNightSetting = m_bLightsIgnoreDayNightSetting;
		pVehicle->m_nFlags.bLightsCanCastStaticShadows = m_bLightsCanCastStaticShadows;
		pVehicle->m_nFlags.bLightsCanCastDynamicShadows = m_bLightsCanCastDynamicShadows;

		// Only do this for old clips where the light states weren't stored in the extension
		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) < g_CPacketVehicleUpdate_Extensions_V8)
		{
			if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
				pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
				pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT ||
				pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				for (u32 uStateIdx = 0; uStateIdx < 14; uStateIdx++)
				{
					pVehicle->GetVehicleDamage()->SetLightStateImmediate(uStateIdx, LoadLightState(uStateIdx));
				}
			}
		}

		// Version 19 handles up to 32 (at least the number used in the game) sirens
		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) < g_CPacketVehicleUpdate_Extensions_V19)
		{
			if (pVehicle->UsesSiren())
			{
				for (u32 uStateIdx = 0; uStateIdx < 8; uStateIdx++)
				{
					pVehicle->GetVehicleDamage()->SetSirenStateImmediate(uStateIdx, LoadSirenState(uStateIdx));
				}
			}
		}

		// Recorded bikes are never on their side stand really
		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			((CBike *)pVehicle)->m_nBikeFlags.bOnSideStand = false;
			((CBike *)pVehicle)->m_nBikeFlags.bGettingPickedUp = false;
		}

		pVehicle->SetStatus(m_status);

		replayAssert(pVehicle->GetMatrix().GetCol3().GetZf() > -500.0f);

		// Specific flags should not be set.
		bool bWantHighDetail = pVehicle->m_nVehicleFlags.bDisplayHighDetail;
		bool bIsInVehicleReusePool = pVehicle->m_nVehicleFlags.bIsInVehicleReusePool;
		bool bIsInPopulationCache = pVehicle->m_nVehicleFlags.bIsInPopulationCache;
		bool bCountAsParkedCarForPopulation = pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation;
		bool bForceHd = pVehicle->m_nVehicleFlags.bForceHd;

		sysMemCpy(&(pVehicle->m_nVehicleFlags), m_VehicleFlags, sizeof(m_VehicleFlags));

		pVehicle->m_nVehicleFlags.bAnimateWheels = false;

		// Restore.
		pVehicle->m_nVehicleFlags.bDisplayHighDetail = bWantHighDetail;
		pVehicle->m_nVehicleFlags.bIsInVehicleReusePool = bIsInVehicleReusePool;
		pVehicle->m_nVehicleFlags.bIsInPopulationCache = bIsInPopulationCache;
		pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation = bCountAsParkedCarForPopulation;
		// Preserve force flag if it's not set.
		if(!bForceHd)
			pVehicle->m_nVehicleFlags.bForceHd = bForceHd;

		pVehicle->m_nPhysicalFlags.bRenderScorched = m_bRenderScorched;
		
		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) < g_CPacketVehicleUpdate_Extensions_V2)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			replayAssert(pShader);

			if(info.GetIsFirstFrame())
			{
				float scorchValue = m_bRenderScorched ? 1.0f : 0.0f;
				pShader->SetBurnoutLevel(scorchValue);
			}
		}

		// Wheels
		replayAssertf(m_WheelCount == pVehicle->GetNumWheels(), "Number of wheels does not match the data");
		for( int i = 0; i < rage::Min(m_WheelCount, (u8)pVehicle->GetNumWheels()); ++i )
		{
			pVehicle->GetWheel(i)->m_nHitMaterialId = (u8)m_WheelHitMaterialId;

			if( pVehicle->GetWheel(i)->GetConfigFlags().IsFlagSet( WCF_REARWHEEL) )
			{
				pVehicle->GetWheel(i)->m_fWheelRadius = m_WheelRadiusRear  * U8_TO_FLOAT_RANGE_0_TO_1;
			}
			else // Front Wheel
			{
				pVehicle->GetWheel(i)->m_fWheelRadius = m_WheelRadiusFront * U8_TO_FLOAT_RANGE_0_TO_1;
			}
		}

		if (m_WheelCount > 0)
		{
			// Check for tyre deform
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			replayAssert(pShader);
			bool bIsEnabled = pShader->GetEnableTyreDeform();

			if (m_bIsTyreDeformOn != (u8)bIsEnabled)
			{
				pShader->SetEnableTyreDeform(!bIsEnabled);
			}
		}

		//TODO4FIVE
		//	pVehicle->GetVehicleAudioEntity()->m_HasDriver			= m_HasDriver;
		//	pVehicle->GetVehicleAudioEntity()->m_IsPlayerControlled = m_IsPlayerControlled;
		//	pVehicle->GetVehicleAudioEntity()->m_IsSirenOn			= m_IsSirenOn;
		
		//	pVehicle->GetVehicleAudioEntity()->m_SirenChoice		= m_SirenChoice;
		//	pVehicle->GetVehicleAudioEntity()->m_UseSirenWarning	= m_UseSirenWarning;
		//	pVehicle->GetVehicleAudioEntity()->m_IsHornStateOn		= m_IsHornStateOn;
		
		pVehicle->m_nFlags.bPossiblyTouchesWater	= IsPossiblyTouchesWater;

		// Horn / Sirens / Car Alarms
		pVehicle->SetCarAlarmState(m_CarAlarmState ? CAR_ALARM_DURATION : CAR_ALARM_NONE);
		pVehicle->GetVehicleAudioEntity()->SetForceSiren(m_IsSirenForcedOn);
		pVehicle->GetVehicleAudioEntity()->SetMustPlaySiren(m_mustPlaySiren);
		pVehicle->GetVehicleAudioEntity()->m_AlarmType			= static_cast<audCarAlarmType> (m_alarmType);
		pVehicle->GetVehicleAudioEntity()->m_IsSirenFucked		= m_IsSirenFucked;
		pVehicle->GetVehicleAudioEntity()->m_UseSirenForHorn	= m_UseSirenForHorn;
		pVehicle->GetVehicleAudioEntity()->SetRadioEnabled(false);
		pVehicle->GetVehicleAudioEntity()->SetPlayerHornOn(m_PlayerHornOn);
		

		// Transmission		
		pVehicle->m_Transmission.m_nGear			= m_Gear;
		pVehicle->m_Transmission.m_fClutchRatio		= m_ClutchRatio * U8_TO_FLOAT_RANGE_0_TO_1;
		pVehicle->m_Transmission.m_fRevRatio		= m_revRatio;

		// Handling Data
// Taking this out as per url:bugstar:3780381
// 		pVehicle->pHandling->m_fMass				= m_Mass		* EXTRACT_MASS_FACTOR;
// 		pVehicle->pHandling->m_fInitialDriveMaxVel	= m_DriveMaxVel;

		pVehicle->m_EngineSwitchedOnTime		= m_EngineSwitchedOnTime;
		pVehicle->GetVehicleAudioEntity()->m_IgnitionHoldTime = m_IgnitionHoldTime;

		audVehicleAudioEntity* pVehicleAudioEntity = pVehicle->GetVehicleAudioEntity();
		if(pVehicleAudioEntity)
		{
			//Handle gadget audio
			u8* pGadgetPointer = (u8*)this + m_VehiclePacketUpdateSize;
			for (int i = 0; i < m_iNumGadgets; ++i)
			{
				u8 packetsize = reinterpret_cast<CVehicleGadgetDataBase*>(pGadgetPointer)->GetPacketSize();

				ExtractVehicleGadget(pVehicleAudioEntity, reinterpret_cast<CVehicleGadgetDataBase*>(pGadgetPointer));

				pGadgetPointer += packetsize;
			}

			if(pVehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				static_cast<audCarAudioEntity*>(pVehicleAudioEntity)->GetVehicleEngine()->SetReplayIsHotwiring(m_HotWiringVehicle);
			}
		}

		// Update damage shader vars...
		if(pVehicle->m_nVehicleFlags.bUseDeformation && pVehicle->GetVehicleDamage()->GetDeformation()->HasValidParentVehicle())
		{
			pVehicle->GetVehicleDamage()->GetDeformation()->UpdateShaderDamageVars(true);
		}

		for(int i=VEH_EXTRA_1; i<=VEH_LAST_EXTRA; i++)
		{
			// Is the extra meant to be on?
			if(m_ExtrasTurnedOn & BIT(i-VEH_EXTRA_1))
			{
				// Is it currently off?
				if(pVehicle->GetIsExtraOn((eHierarchyId)i) == false)
				{
					// Turn it on.
					pVehicle->TurnOffExtra((eHierarchyId)i, false);
				}
			}
			else
			{
				// Is it currently on ?
				if(pVehicle->GetIsExtraOn((eHierarchyId)i) == true)
				{
					// Turn it off.
					pVehicle->TurnOffExtra((eHierarchyId)i, true);
				}
			}
		}

		pVehicle->SetIsInWater(m_IsInWater);
		pVehicle->SetIsReplayOverrideWheelsRot(m_IsDummy == 1 || m_IsVisible != 1);
	}
	else
	{
		Printf("Replay:Car wasn't there");
	}
}


void CPacketVehicleUpdate::ExtractExtensions(CVehicle* pVehicle, CPacketVehicleUpdate const* pNextPacket, float interp, const CReplayState& flags) const
{
	(void)flags;

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V1)
	{
		//---------------------- Extensions V1 ------------------------/
		Quaternion rQuatFinal;
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		
		if(!pNextPacket || pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || interp == 0.0f || GetWarpedThisFrame() || pNextPacket->GetWarpedThisFrame())
		{
			pExtensions->m_Quaternion.LoadQuaternion(rQuatFinal);
		}
		else
		{
			Quaternion rQuatPrev, rQuatNext;
			PacketExtensions *pExtensionsNext = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketVehicleUpdate);

			pExtensions->m_Quaternion.LoadQuaternion(rQuatPrev);
			pExtensionsNext->m_Quaternion.LoadQuaternion(rQuatNext);

			rQuatPrev.PrepareSlerp(rQuatNext);
			rQuatFinal.Slerp(interp, rQuatPrev, rQuatNext);
		}

		Matrix34 rMatrix;
		rMatrix.FromQuaternion(rQuatFinal);
		replayAssert(rMatrix.IsOrthonormal());
		rMatrix.d = VEC3V_TO_VECTOR3(pVehicle->GetMatrix().d());
		
		fwAttachmentEntityExtension* pExtension = pVehicle->GetAttachmentExtension();
		bool assertOn = false;
		if (pExtension) 
		{
			assertOn = pExtension->GetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE);
			pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
		}
		pVehicle->SetMatrix(rMatrix);
		if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, assertOn);

	}


	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V2)
	{
		//---------------------- Extensions V2 ------------------------/
		float scorchValue = 0.0f;
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);

		if(pNextPacket)
		{
			PacketExtensions *pExtensionsNext = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketVehicleUpdate);
			scorchValue = ((float)Lerp(interp, pExtensions->m_scorchValue, pExtensionsNext->m_scorchValue)) / 255.0f;
		}
		else
		{
			scorchValue = ((float)pExtensions->m_scorchValue) / 255.0f;
		}

		CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
		replayAssert(pShader);
		pShader->SetBurnoutLevel(scorchValue);

		ReplayVehicleExtension* pExt = ReplayVehicleExtension::GetExtension(pVehicle);
		
		if( !pExt )
		{
			pExt = ReplayVehicleExtension::Add(pVehicle);
		}

		if( pExt )
		{
			pExt->SetIncScorch(false);	// Don't increment the scorch value...we're in control
			if(pNextPacket)
			{
				pExt->SetForceHD(((CVehicleFlags*)(pNextPacket->m_VehicleFlags))->bForceHd);
			}
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V3)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		float dirtLevel = (float)pExtensions->m_bodyDirtLevel;
		pVehicle->SetBodyDirtLevel(dirtLevel);
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V4)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);

		if(pExtensions->m_hasAlphaOverride)
		{
			int currentAlpha = 0;
			bool IsOnAlphaOverrideList = CPostScan::GetAlphaOverride(pVehicle, currentAlpha);
			if(!IsOnAlphaOverrideList || currentAlpha != pExtensions->m_alphaOverrideValue)
			{
				bool useSmoothAlpha = !pVehicle->GetShouldUseScreenDoor();
				entity_commands::CommandSetEntityAlphaEntity(pVehicle, (int)pExtensions->m_alphaOverrideValue, useSmoothAlpha BANK_PARAM(entity_commands::CMD_OVERRIDE_ALPHA_BY_NETCODE));
			}
		}
		else if(CPostScan::IsOnAlphaOverrideList(pVehicle))
		{
			entity_commands::CommandResetEntityAlphaEntity(pVehicle);
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V8)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
			pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
			pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT ||
			pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
		{
			for (u32 uStateIdx = 0; uStateIdx < VEHICLE_LIGHT_COUNT; uStateIdx++)
			{
				pVehicle->GetVehicleDamage()->SetLightStateImmediate(uStateIdx, pExtensions->LoadLightState(uStateIdx));
			}
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V9)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);

		pVehicle->m_Transmission.SetManifoldPressure(pExtensions->m_manifoldPressure);
	}


	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V10)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);

		replayAssert((VEH_LAST_WINDOW - VEH_FIRST_WINDOW) < 8);
		for(int i = 0; i < VEH_LAST_WINDOW - VEH_FIRST_WINDOW; ++i)
		{
			if(pExtensions->m_windowsRolledDown & (1 << i))
			{
				if(!pVehicle->IsWindowDown((eHierarchyId)(VEH_FIRST_WINDOW + i)))
					pVehicle->RolldownWindow((eHierarchyId)(VEH_FIRST_WINDOW + i));
			}
			else
			{
				if(pVehicle->IsWindowDown((eHierarchyId)(VEH_FIRST_WINDOW + i)))
					pVehicle->RollWindowUp((eHierarchyId)(VEH_FIRST_WINDOW + i));
			}
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V11)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		if(pNextPacket)
		{
			PacketExtensions *pExtensionsNext = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketVehicleUpdate);
			if(pExtensionsNext)
			{
				float valueX = ProcessChainValue(pExtensions->m_uvAnimValue2X, pExtensionsNext->m_uvAnimValue2X, interp, true);
				float valueY = ProcessChainValue(pExtensions->m_uvAnimValue2Y, pExtensionsNext->m_uvAnimValue2Y, interp, true);

				CCustomShaderEffectVehicle* pShaderFx =	static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
				if(pShaderFx)
					pShaderFx->Track2UVAnimSet(Vector2(valueX, valueY));
			}
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V12)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		if(pExtensions)
		{
			CCustomShaderEffectVehicle* pShaderFx =	static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			if(pShaderFx)
				pShaderFx->SetUserEmissiveMultiplier(pExtensions->m_emissiveMultiplier);
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V13)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		if(pExtensions)
		{
			pVehicle->SetRocketBoosting(pExtensions->m_isBoosting);
			pVehicle->SetRocketBoostRemaining(pExtensions->m_boostRemaining);
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V14)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		if(pNextPacket)
		{
			PacketExtensions *pExtensionsNext = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketVehicleUpdate);
			if(pExtensionsNext)
			{
				float valueX = ProcessChainValue(pExtensions->m_uvAmmoAnimValue2X, pExtensionsNext->m_uvAmmoAnimValue2X, interp, true) * -1.0f;
				float valueY = ProcessChainValue(pExtensions->m_uvAmmoAnimValue2Y, pExtensionsNext->m_uvAmmoAnimValue2Y, interp, true) * -1.0f;

				CCustomShaderEffectVehicle* pShaderFx =	static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
				if(pShaderFx)
					pShaderFx->TrackAmmoUVAnimSet(Vector2(valueX, valueY));
			}
		}
	}

	// Only apply suspension to windows after this version
	CVfxHelper::SetApplySuspensionOnReplay(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V15);


	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V16)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		pVehicle->SetSpecialFlightModeRatio(pExtensions->m_specialFlightModeRatio);
	}

	if (GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V17)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		pVehicle->GetVehicleAudioEntity()->SetReplaySpecialFlightModeAngVelocity(pExtensions->m_specialFlightModeAngularVelocity);
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V19)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
			pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
			pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT ||
			pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
		{
			for (u32 uStateIdx = 0; uStateIdx < VEHICLE_SIREN_COUNT; uStateIdx++)
			{
				pVehicle->GetVehicleDamage()->SetSirenStateImmediate(uStateIdx, pExtensions->LoadSirenState(uStateIdx));
			}
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate) >= g_CPacketVehicleUpdate_Extensions_V20)
	{
		PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		pVehicle->m_Buoyancy.SetStatus(pExtensions->m_buoyancyState);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleUpdate::FixupVehicleMods(CVehicle* pVehicle, CPacketVehicleUpdate const* pNextPacket, float interp, const CReplayState& flags) const
{
	(void)flags;

	pVehicle->ResetSkeletonForMods();
	
	bool nextPacketWarpedThisFrame = pNextPacket ? pNextPacket->GetWarpedThisFrame() : false;
	bool warpingInUse = GetWarpedThisFrame() || nextPacketWarpedThisFrame;

	u32 extensionVersion = GET_PACKET_EXTENSION_VERSION(CPacketVehicleUpdate);

	//---------------------- Extensions V5 ------------------------/
	if(extensionVersion >= g_CPacketVehicleUpdate_Extensions_V5)
	{
		PacketExtensions* pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleUpdate);
		PacketExtensions* pNextExtensions = pNextPacket ? (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketVehicleUpdate) : nullptr;
		u32 rotationSize = (extensionVersion >= g_CPacketVehicleUpdate_Extensions_V6) ? sizeof(CPacketVector<4>) : sizeof(CPacketQuaternionL);

		u8* pData = (u8*)pExtensions + pExtensions->m_modBoneDataOffset;
		CVehicleVariationInstance& variationInstance = pVehicle->GetVariationInstance();
		if(pExtensions->m_modBoneDataOffset && pExtensions->m_modBoneDataCount)
		{
			if(variationInstance.GetVehicleRenderGfx())
			{
				for(int i = 0; i < pExtensions->m_modBoneDataCount; ++i)
				{
					u8 modSlot = 0;
					s32 boneID = -1;

					if(extensionVersion >= g_CPacketVehicleUpdate_Extensions_V18)
					{
						boneID = *(s32*)pData;
						pData += sizeof(boneID);

						modSlot = variationInstance.GetModSlotFromBone((eHierarchyId)boneID);
					}
					else
					{
						modSlot = *pData;
						pData += sizeof(modSlot);

						if(modSlot < VMT_RENDERABLE)
						{
							boneID = variationInstance.GetBone(modSlot);
						}
						else
						{
							boneID = variationInstance.GetBoneForLinkedMod(modSlot - VMT_RENDERABLE);
						}
					}

					if(modSlot == INVALID_MOD || boneID == -1)
 					{
 						pData += (rotationSize + sizeof(CPacketVector<3>));
 						continue;
 					}

					Quaternion q;
					if(extensionVersion >= g_CPacketVehicleUpdate_Extensions_V6)
					{
						CPacketVector<4>* pRotation = (CPacketVector<4>*)pData;
						pData += sizeof(CPacketVector<4>);
						pRotation->Load(q);
					}
					else
					{
						CPacketQuaternionL* pRotation = (CPacketQuaternionL*)pData;
						pData += sizeof(CPacketQuaternionL);
						pRotation->LoadQuaternion(q);
					}			

					CPacketVector<3>* pPos = (CPacketVector<3>*)pData;
					pData += sizeof(CPacketVector<3>);
					Vector3 pos;
					pPos->Load(pos);

					Vector3 nextPos(Vector3::ZeroType);
					Quaternion nextQ(Quaternion::IdentityType);
					int boneIdx = pVehicle->GetBoneIndex((eHierarchyId)boneID);

					if(pNextExtensions && pNextExtensions->m_modBoneDataOffset && pNextExtensions->m_modBoneDataCount && !warpingInUse)
					{
						u8* pNextData = (u8*)pNextExtensions + pNextExtensions->m_modBoneDataOffset;

						for(int i = 0; i < pNextExtensions->m_modBoneDataCount; ++i)
						{
							if(extensionVersion >= g_CPacketVehicleUpdate_Extensions_V18)
							{
								s32 nextBoneID = *(s32*)pNextData;
								pNextData += sizeof(nextBoneID);

								if(nextBoneID != boneID)
								{
									pNextData += rotationSize + sizeof(CPacketVector<3>);
									continue;
								}
							}
							else
							{
								u8 nextModSlot = *pNextData;
								pNextData += sizeof(nextModSlot);

								if(nextModSlot != modSlot)
								{
									pNextData += rotationSize + sizeof(CPacketVector<3>);
									continue;
								}
							}

							if(extensionVersion >= g_CPacketVehicleUpdate_Extensions_V6)
							{
								CPacketVector<4>* pRotation = (CPacketVector<4>*)pNextData;
								pNextData += sizeof(CPacketVector<4>);
								pRotation->Load(nextQ);
							}
							else
							{
								CPacketQuaternionL* pRotation = (CPacketQuaternionL*)pNextData;
								pNextData += sizeof(CPacketQuaternionL);
								pRotation->LoadQuaternion(nextQ);
							}		
							
							CPacketVector<3>* pNextPos = (CPacketVector<3>*)pNextData;
							pNextData += sizeof(CPacketVector<3>);
							pNextPos->Load(nextPos);

							break;
						}
					}
					else
					{
						crBoneData const* pBoneData = pVehicle->GetSkeletonData().GetBoneData(boneIdx);
					
						Mat34V parentMat;
						if(pBoneData->GetParent())
						{
							pVehicle->GetSkeleton()->GetGlobalMtx(pBoneData->GetParent()->GetIndex(), parentMat);
						}
						else
						{
							parentMat = pVehicle->GetTransform().GetMatrix();
						}

						Mat34V localBoneMat;
						Mat34VFromQuatV(localBoneMat, pBoneData->GetDefaultRotation(), pBoneData->GetDefaultTranslation());

						Mat34V globalMat;
						Transform(globalMat, localBoneMat, parentMat);
						nextQ = QUATV_TO_QUATERNION(QuatVFromMat34V(globalMat)); // Used to set the global matrix on the skeleton

						// This needs to be in local space passed to the variation instance
						nextPos = VEC3V_TO_VECTOR3(pBoneData->GetDefaultTranslation());
					}


					// Check for the bone being nulled out...
					if(nextQ.x != 0.0f || nextQ.y != 0.0f || nextQ.z != 0.0f)
					{
						Quaternion qInterp;
						q.PrepareSlerp(nextQ);
						qInterp.Slerp(interp, q, nextQ);
						q = qInterp;
					}

					pos = (pos * (1.0f-interp)) + (nextPos * interp);

					// If the quaternion loaded was zero then the bone was nulled out (zero-scaled).
					// In this case leave the matrix as zero to do the same
					Matrix33 mat(Matrix33::ZeroType);
					if(q.x != 0.0f || q.y != 0.0f || q.z != 0.0f)
					{
						Matrix34 m; m.FromQuaternion(q);
						mat.Set(m.GetVector(0),m.GetVector(1),m.GetVector(2));
					}

					variationInstance.SetBonePosition(modSlot, pos);

					// Update the global matrix on the skeleton as this is what 'StoreModMatrices' pulls from
					if(pVehicle->GetSkeleton())
					{
						Mat34V mat34 = Mat34V(V_IDENTITY);
						
						pVehicle->GetSkeleton()->GetGlobalMtx(boneIdx, mat34);
						mat34.Set3x3(MATRIX33_TO_MAT33V(mat));
						pVehicle->GetSkeleton()->SetGlobalMtx(boneIdx, mat34);

						// Inverse update this bone... fixes url:bugstar:7780240
						pVehicle->GetSkeleton()->PartialInverseUpdate(boneIdx);

						static bool b = false;
						if(b)
						{
							pVehicle->GetSkeleton()->PartialInverseUpdate(boneIdx);
							Mat34V localMat = pVehicle->GetSkeleton()->GetLocalMtx(boneIdx);
							Matrix34 lm(MAT34V_TO_MATRIX34(localMat));
							Quaternion q2;
							lm.ToQuaternion(q2);
							replayDebugf1("modSlot %u, boneID %d, boneIdx %d, Quat %f, %f, %f, %f", modSlot, boneID, boneIdx, q2.x, q2.y, q2.z, q2.w);
						}
					}
				}
			}
		}

		// Update the render matrices after we've set them on the entity/variation instance	
		CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
		if(var.GetKitIndex() != INVALID_VEHICLE_KIT_INDEX) 
		{
			CVehicleStreamRenderGfx *pRenderGFX = var.GetVehicleRenderGfx();
			if( pRenderGFX )
			{
				pRenderGFX->StoreModMatrices(pVehicle);
			}
		}

		// In this version we may have a list of moded bones that were zero'd out....they'd been broken off the vehicle.
		if(extensionVersion >= g_CPacketVehicleUpdate_Extensions_V7)
		{
			if(variationInstance.GetVehicleRenderGfx())
			{
				for(int i = 0; i < pExtensions->m_modBoneZeroedCount; ++i)
				{
					u8 bone = *pData; pData += sizeof(u8);

					Matrix33 mat(Matrix33::ZeroType);
					variationInstance.GetVehicleRenderGfx()->SetMatrix(bone, mat);
				}
			}
		}
	}
	else
	{
		// Remember to do this for old clips...
		CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
		if(var.GetKitIndex() != INVALID_VEHICLE_KIT_INDEX) 
		{
			CVehicleStreamRenderGfx *pRenderGFX = var.GetVehicleRenderGfx();
			if( pRenderGFX )
			{
				pRenderGFX->StoreModMatrices(pVehicle);
			}
		}
	}
}


//=====================================================================================================
void CPacketVehicleUpdate::ExtractInterpolateableValues(CVehicle* pVehicle, CPacketVehicleUpdate const* pNextPacket, float fInterp) const
{
	Vector3		prevSpeed;
	Vector3		nextSpeed;
	Vector3		interpSpeed;

	f32			interpFireEvo;
	f32			interpThrottle;
	f32			interpRevs;
	f32			interpEngineHealth;
	f32			interpSteerAngle;
	f32			interpGasPedal;
	f32			interpBrakePedal;


	ExtractSpeed( prevSpeed );

	if( pNextPacket && fInterp )
	{
		// Interpolate
		pNextPacket->GetSpeed( nextSpeed );
		interpSpeed			= Lerp( fInterp, prevSpeed, nextSpeed );

		interpFireEvo		= Lerp( fInterp, (f32)(m_FireEvo), (f32)(pNextPacket->GetFireEvo()) );
		interpFireEvo		*= U8_TO_FLOAT_RANGE_0_TO_1;

		interpThrottle		= Lerp( fInterp, (f32)(m_Throttle), (f32)(pNextPacket->GetThrottle()) );
		interpThrottle		*= S8_TO_FLOAT_RANGE_NEG_1_TO_1;

		interpEngineHealth	= Lerp( fInterp, m_EngineHealth, pNextPacket->GetEngineHealth() );

		interpRevs			= Lerp( fInterp, m_Revs, pNextPacket->GetRevs() );

		interpSteerAngle	= Lerp( fInterp, (f32)(m_SteerAngle), (f32)(pNextPacket->GetSteerAngle()) );
		interpSteerAngle	*= S8_TO_FLOAT_DIVISOR_20;

		interpGasPedal		= Lerp( fInterp, (f32)(m_GasPedal), (f32)(pNextPacket->GetGasPedal()) );
		interpGasPedal		*= S8_TO_FLOAT_DIVISOR_100;

		interpBrakePedal	= Lerp( fInterp, (f32)(m_Brake), (f32)(pNextPacket->GetBrake()) );
		interpBrakePedal	*= S8_TO_FLOAT_DIVISOR_100;

		interpRevs			= Lerp( fInterp, m_Revs, pNextPacket->GetRevs()) ;
	}
	else
	{
		// No Interpolation
		interpSpeed						= prevSpeed;
		interpFireEvo					= m_FireEvo		* U8_TO_FLOAT_RANGE_0_TO_1;
		interpThrottle					= m_Throttle	* S8_TO_FLOAT_RANGE_NEG_1_TO_1;
		interpEngineHealth				= m_EngineHealth;
		interpSteerAngle				= m_SteerAngle	* S8_TO_FLOAT_DIVISOR_20;
		interpGasPedal					= m_GasPedal	* S8_TO_FLOAT_DIVISOR_100;
		interpBrakePedal				= m_Brake		* S8_TO_FLOAT_DIVISOR_100;
		interpRevs						= m_Revs;
	}

	// Set Values
	pVehicle->SetVelocity( interpSpeed );

	pVehicle->m_Transmission.m_fFireEvo			= interpFireEvo;
	pVehicle->m_Transmission.m_fThrottle		= interpThrottle;
	pVehicle->m_Transmission.m_fEngineHealth	= interpEngineHealth;
	pVehicle->m_vehControls.m_steerAngle		= interpSteerAngle;
	pVehicle->m_vehControls.m_throttle			= interpGasPedal;
	pVehicle->m_vehControls.m_brake				= interpBrakePedal;

	if(pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->SetReplayRevs(interpRevs);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleUpdate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<ReplayID>0x%X, %d</ReplayID>\n", GetReplayID().ToInt(), GetReplayID().ToInt());
	CFileMgr::Write(handle, str, istrlen(str));

// 	snprintf(str, 1024, "\t\t<VehicleType>%u</VehicleType>\n", m_VehicleTypeOld);
// 	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
CPacketVehicleCreate::CPacketVehicleCreate()
	: CPacketBase(PACKETID_VEHICLECREATE, sizeof(CPacketVehicleCreate))
	, CPacketEntityInfo()
	, m_FrameCreated(FrameRef::INVALID_FRAME_REF)
	, m_envEffScale(0)
	, m_hasCustomPrimaryColor(false)
	, m_hasCustomSecondaryColor(false)
{
	m_customPrimaryColor = Color32(0,0,0);
	m_customSecondaryColor = Color32(0,0,0);
}

//////////////////////////////////////////////////////////////////////////
const tPacketVersion CPacketVehicleCreateExtensions_V1 = 1;
const tPacketVersion CPacketVehicleCreateExtensions_V2 = 2;
const tPacketVersion CPacketVehicleCreateExtensions_V3 = 3;
const tPacketVersion CPacketVehicleCreateExtensions_V4 = 4;
const tPacketVersion CPacketVehicleCreateExtensions_V5 = 5;
const tPacketVersion CPacketVehicleCreateExtensions_V6 = 6;
const tPacketVersion CPacketVehicleCreateExtensions_V7 = 7;
const tPacketVersion CPacketVehicleCreateExtensions_V8 = 8;
const tPacketVersion CPacketVehicleCreateExtensions_V9 = 9;
const tPacketVersion CPacketVehicleCreateExtensions_V10 = 10;
void CPacketVehicleCreate::Store(const CVehicle* pVehicle, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId, tPacketSize packetSize)
{
	(void)sessionBlockIndex;
	PACKET_EXTENSION_RESET(CPacketVehicleCreate);
	CPacketBase::Store(packetId, packetSize);
	CPacketEntityInfo::Store(pVehicle->GetReplayID());

	m_unused0 = 0;

	m_firstCreationPacket = firstCreationPacket;
	SetShouldPreload(firstCreationPacket);

	m_VehicleTypeOld = 0;//	= (u8)pVehicle->GetVehicleType();
	m_ModelNameHash	= pVehicle->GetArchetype()->GetModelNameHash();
	m_LiveryId = pVehicle->GetLiveryId();
	m_DisableExtras = pVehicle->m_nDisableExtras;

	CCustomShaderEffectVehicle* pCSEV = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	sysMemCpy(m_LicencePlateText, pCSEV->GetLicensePlateText(), CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX);

	m_LicencePlateTexIdx = pCSEV->GetLicensePlateTexIndex();

	m_HornHash = const_cast<audVehicleAudioEntity*>(pVehicle->GetVehicleAudioEntity())->GetVehicleHornSoundHash();
	m_HornIndex = pVehicle->GetVehicleAudioEntity()->GetHornSoundIdx();

	m_alarmType	= static_cast<u8>(pVehicle->GetVehicleAudioEntity()->GetAlarmType());
	
	//const CVehicleVariationInstance& VehicleVariationInstance = pVehicle->GetVariationInstance();
// 	u16 kitId = INVALID_VEHICLE_KIT_ID;
// 	if(VehicleVariationInstance.GetKitIndex() < CVehicleModelInfo::GetVehicleColours()->m_Kits.GetCount())
// 	{
// 		kitId = CVehicleModelInfo::GetVehicleColours()->m_Kits[VehicleVariationInstance.GetKitIndex()].GetId();
// 	}
	
	// KitId storing moved to the extension
	m_InitialVariationData.StoreVariations(pVehicle);
	m_LatestVariationData.StoreVariations(pVehicle);

	//Store the kit ID in the kit index to use to lookup the correct index.
// 	m_InitialVariationData.SetKitIndex(kitId);
// 	m_LatestVariationData.SetKitIndex(kitId);

	m_VarationDataVerified = false;

	m_posAndRot.StoreMatrix(MAT34V_TO_MATRIX34(pVehicle->GetMatrix()));

	m_playerInThisVehicle = (pVehicle->GetDriver() == FindPlayerPed());

	if( pCSEV )
	{
		m_envEffScale = pCSEV->GetEnvEffScaleU8();

		m_customPrimaryColor = pCSEV->GetCustomPrimaryColor();
		m_customSecondaryColor = pCSEV->GetCustomSecondaryColor();
		m_hasCustomPrimaryColor = pCSEV->GetIsPrimaryColorCustom();
		m_hasCustomSecondaryColor = pCSEV->GetIsSecondaryColorCustom();
	}
};


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleCreate::StoreExtensions(const CVehicle* pVehicle, bool firstCreationPacket, u16 sessionBlockIndex) 
{ 
	(void)firstCreationPacket;
	PACKET_EXTENSION_RESET(CPacketVehicleCreate);

#if __BANK
	if(CReplayMgr::GetVehicleInterface()->IsRecordingOldStyleWheels() == true)
		return;
#endif //__BANK

	bool isDeletePacket = sessionBlockIndex == 0xffff;
	WheelValueExtensionData *pWheelExtensionData = CPacketWheel::GetExtensionDataForRecording((CVehicle *)pVehicle, isDeletePacket, sessionBlockIndex);

	if(pWheelExtensionData)
	{
		// Update with latest wheel values (non-delete packets only)
		if(isDeletePacket == false)
			pWheelExtensionData->UpdateReferenceSetDuringRecording((CVehicle *)pVehicle, sessionBlockIndex);

		WHEEL_REFERENCE_VALUES_HEADER *pDest = (WHEEL_REFERENCE_VALUES_HEADER *)BEGIN_PACKET_EXTENSION_WRITE(CPacketVehicleCreateExtensions_V10, CPacketVehicleCreate);

		pDest->noOfWheels = pWheelExtensionData->m_NoOfWheels;
		pDest->sessionBlockIndices[0] = pWheelExtensionData->m_sessionBlockIndices[0];
		pDest->sessionBlockIndices[1] = pWheelExtensionData->m_sessionBlockIndices[1];

		// Store the current state of the wheel values.
		for(u32 i=0; i<pWheelExtensionData->m_NoOfWheels; i++)
			pDest->wheelValuePairs[i] = pWheelExtensionData->m_ReferenceWheelValues[i];

		const CVehicleVariationInstance& VehicleVariationInstance = pVehicle->GetVariationInstance();

		VehicleCreateExtension* pExtension = (VehicleCreateExtension*)&pDest->wheelValuePairs[pDest->noOfWheels];
		pExtension->m_parentID = ReplayIDInvalid;
		//---------------------- CPacketVehicleCreateExtensions_V2 ------------------------/
		if(pVehicle->GetDynamicComponent() && pVehicle->GetDynamicComponent()->GetAttachmentExtension() && pVehicle->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())
		{
			pExtension->m_parentID = ((CPhysical*)pVehicle->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())->GetReplayID();
		}	

		//---------------------- CPacketVehicleCreateExtensions_V3 ------------------------/
		ReplayVehicleExtension* pExt = ReplayVehicleExtension::ProcureExtension((CEntity*)pVehicle);
		if( pExt )
		{
			pExtension->m_dialBoneMtx2 = pExt->GetVehicleDialBoneMtx();
		}

		//---------------------- CPacketVehicleCreateExtensions_V4 ------------------------/
		pExtension->m_dirtColour = pVehicle->GetBodyDirtColor();

		//---------------------- CPacketVehicleCreateExtensions_V5 ------------------------/
		pExtension->m_numExtraMods = sizeof(extraVehicleModArrayReplay);

		//---------------------- CPacketVehicleCreateExtensions_V6 ------------------------/
		pExtension->m_Livery2Id= pVehicle->GetLivery2Id();
		pExtension->m_extraModOffset = (u32)sizeof(VehicleCreateExtension);

		//---------------------- CPacketVehicleCreateExtensions_V6 ------------------------/
		pExtension->m_colour5 = VehicleVariationInstance.GetColor5();
		pExtension->m_colour6 = VehicleVariationInstance.GetColor6();
		pExtension->m_unused[0] = 0;

		//---------------------- CPacketVehicleCreateExtensions_V9 ------------------------/
		u16 kitId = INVALID_VEHICLE_KIT_ID;
		if(VehicleVariationInstance.GetKitIndex() < CVehicleModelInfo::GetVehicleColours()->m_Kits.GetCount())
		{
			kitId = CVehicleModelInfo::GetVehicleColours()->m_Kits[VehicleVariationInstance.GetKitIndex()].GetId();
		}
		pExtension->m_initialKitId = kitId;
		pExtension->m_latestKitId = kitId;



		u8* pExtraMods = (u8*)(pExtension+1);
		for(int i = 0; i < sizeof(extraVehicleModArrayReplay); ++i, ++pExtraMods)
		{
			*pExtraMods = VehicleVariationInstance.GetModIndex(static_cast<eVehicleModType>(extraVehicleModArrayReplay[i]));
		}

		//---------------------- CPacketVehicleCreateExtensions_V10 ------------------------/
		pExtension->m_xenonLightColour = pVehicle->GetXenonLightColor();

		END_PACKET_EXTENSION_WRITE(pExtraMods, CPacketVehicleCreate);
	}
}


CReplayID CPacketVehicleCreate::GetParentID() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V2)
	{
		WHEEL_REFERENCE_VALUES_HEADER *pDest = (WHEEL_REFERENCE_VALUES_HEADER *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleCreate);
		VehicleCreateExtension* pExtension = (VehicleCreateExtension*)&pDest->wheelValuePairs[pDest->noOfWheels];
		return pExtension->m_parentID;
	}
	return ReplayIDInvalid;
}


void CPacketVehicleCreate::ExtractReferenceWheelValuesIntoVehicleExtension(CVehicle *pVehicle)
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) != 0)
	{
		WHEEL_REFERENCE_VALUES_HEADER *pDefaultWheelData = (WHEEL_REFERENCE_VALUES_HEADER *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleCreate);
		WheelValueExtensionData *pWheelExtensionData = CPacketWheel::GetExtensionDataForPlayback(pVehicle);

		if(pWheelExtensionData)
			pWheelExtensionData->InitializeForPlayback(pDefaultWheelData->noOfWheels, pDefaultWheelData->sessionBlockIndices[0], pDefaultWheelData->sessionBlockIndices[1], &pDefaultWheelData->wheelValuePairs[0]);
		
		//restore the horn index
		if( pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetHornSoundIdx(m_HornIndex);
		}

		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V3)
		{
			Matrix34 mat;
			WHEEL_REFERENCE_VALUES_HEADER *pDest = (WHEEL_REFERENCE_VALUES_HEADER *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleCreate);
			VehicleCreateExtension* pExtension = (VehicleCreateExtension*)&pDest->wheelValuePairs[pDest->noOfWheels];
			if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V8)
				pExtension->m_dialBoneMtx2.LoadMatrix(mat);
			else
				pExtension->m_dialBoneMtx.LoadMatrix(mat);

			ReplayVehicleExtension* pExt = ReplayVehicleExtension::ProcureExtension(pVehicle);
			if( pExt )
			{
				pExt->SetVehicleDialBoneMtx(mat);
			}
		}

		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V4)
		{
			WHEEL_REFERENCE_VALUES_HEADER *pDest = (WHEEL_REFERENCE_VALUES_HEADER *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleCreate);
			VehicleCreateExtension* pExtension = (VehicleCreateExtension*)&pDest->wheelValuePairs[pDest->noOfWheels];
			pVehicle->SetBodyDirtColor(pExtension->m_dirtColour);
		}
	}
}

void CPacketVehicleCreate::SetVariationData(CVehicle* pVehicle) const
{
	CStoredVehicleVariations var;
	GetVariationData().ToGameStruct(var);

	
	u16 kitId = INVALID_VEHICLE_KIT_ID;
	if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V5)
	{
		WHEEL_REFERENCE_VALUES_HEADER* p = (WHEEL_REFERENCE_VALUES_HEADER*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketVehicleCreate);
		VehicleCreateExtension* pExtension = (VehicleCreateExtension*)&p->wheelValuePairs[p->noOfWheels];

		// We need to get a pointer to the extra mod data off the end of the extension...
		// In V5 this was immediately after m_numExtraMods var...in V6 we added an m_extraModOffset to store this
		const int sizeOfExtensionV5 = 28;	// Size of the extension in V5
		u8* pExtraMods = (u8*)(pExtension) + sizeOfExtensionV5;
		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V6)
		{
			pExtraMods = (u8*)(pExtension) + pExtension->m_extraModOffset;
		}

		for(int i = 0; i < pExtension->m_numExtraMods; ++i, ++pExtraMods)
		{
			var.SetModIndex((eVehicleModType)extraVehicleModArrayReplay[i], *pExtraMods);
		}

		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V6)
		{
			pVehicle->SetLivery2Id(pExtension->m_Livery2Id);
		}

		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V7)
		{
			var.SetColor5(pExtension->m_colour5);
			var.SetColor6(pExtension->m_colour6);
		}

		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V9)
		{
			// The extension stored the kit ID instead of index...
			if(IsVarationDataVerified() == false)
			{
				kitId = pExtension->m_latestKitId;
			}
			else
			{
				kitId = pExtension->m_initialKitId;
			}
		}

		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) >= CPacketVehicleCreateExtensions_V10)
		{
			pVehicle->SetXenonLightColor(pExtension->m_xenonLightColour);
		}
	}

	if( !pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CANNOT_BE_MODDED) )
	{
		if(GET_PACKET_EXTENSION_VERSION(CPacketVehicleCreate) < CPacketVehicleCreateExtensions_V9)
		{
			// Old version can use the u8 kit id stored in the variation data
			kitId = GetVariationData().GetKitIndex();
		}

		u16 kitIndex = INVALID_VEHICLE_KIT_INDEX;
		//Lookup the correct kit index using the ID.
		if(kitId != INVALID_VEHICLE_KIT_ID)
		{
			kitIndex = CVehicleModelInfo::GetVehicleColours()->GetKitIndexById(kitId);
		}

		var.SetKitIndex(kitIndex);			

		pVehicle->SetVariationInstance(var);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleCreate::SetCustomColors(CCustomShaderEffectVehicle* pCSEV) const
{
	if( pCSEV )
	{
		if( m_hasCustomPrimaryColor )
		{
			pCSEV->SetCustomPrimaryColor(m_customPrimaryColor);
		}
		if( m_hasCustomSecondaryColor )
		{
			pCSEV->SetCustomSecondaryColor(m_customSecondaryColor);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleDelete::Store(const CVehicle* pVehicle, const CPacketVehicleCreate* pLastCreatedPacket)
{
	//clone our last know good creation packet and setup the correct new ID and size
	CloneBase<CPacketVehicleDelete>(pLastCreatedPacket);

	// Manually override this...the creation packet in the deletion will never be the first one, but the one
	// that is passed in could well be.
	SetIsFirstCreationPacket(false);

	//We need to use the version from the create packet to version the delete packet as it inherits from it.
	CPacketBase::Store(PACKETID_VEHICLEDELETE, sizeof(CPacketVehicleDelete), pLastCreatedPacket->GetPacketVersion());

	FrameRef creationFrame;
	pVehicle->GetCreationFrameRef(creationFrame);
	SetFrameCreated(creationFrame);

	replayFatalAssertf(GetPacketSize() <= MAX_VEHICLE_DELETION_SIZE, "Size of Vehicle Deletion packet is too large! (%u)", GetPacketSize());
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleDelete::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<ReplayID>0x%08X</ReplayID>\n", GetReplayID().ToInt());
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
void CVehInterp::Init(CPacketVehicleUpdate const* pPrevPacket, CPacketVehicleUpdate const* pNextPacket)
{
	Reset();
	SetPrevPacket(pPrevPacket);

	CPacketVehicleUpdate const* pVehiclePrevPacket = GetPrevPacket();

	m_sWheelCount = pVehiclePrevPacket->GetWheelCount();
	FormCVehInterpData(m_prevData, pVehiclePrevPacket);

	if (pNextPacket)
	{
		SetNextPacket(pNextPacket);
		
		CPacketVehicleUpdate const* pVehicleNextPacket = GetNextPacket();
		replayAssert(pVehiclePrevPacket->GetPacketID() == pVehicleNextPacket->GetPacketID());
		replayAssert(pVehiclePrevPacket->GetReplayID() == pVehicleNextPacket->GetReplayID());
		replayAssert(pVehiclePrevPacket->GetWheelCount() == pVehicleNextPacket->GetWheelCount());

		FormCVehInterpData(m_nextData, pVehicleNextPacket);
	}

	m_bWheelDataOldStyle = false;

	if((m_prevData.m_pWheelPacket && m_prevData.m_pWheelPacket->GetPacketID() == PACKETID_WHEELUPDATE_OLD) || (m_nextData.m_pWheelPacket && m_nextData.m_pWheelPacket->GetPacketID() == PACKETID_WHEELUPDATE_OLD))
		m_bWheelDataOldStyle = true;
}


//////////////////////////////////////////////////////////////////////////
void CVehInterp::FormCVehInterpData(struct CVehInterpData &dest, CPacketVehicleUpdate const* pVehiclePacket)
{
#if REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET
	u32 wheelPktCount = pVehiclePacket->GetWheelCount() ? 1 : 0;
#else // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET
	u32 wheelPktCount = pVehiclePacket->GetWheelCount();
#endif // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

	// Wheels
	if(wheelPktCount > 0)
	{
// 	#if __ASSERT	
// 		u8 vehicleType = pVehiclePacket->GetVehicleType();
// 		replayFatalAssertf(vehicleType != VEHICLE_TYPE_TRAIN && vehicleType != VEHICLE_TYPE_BOAT && vehicleType != VEHICLE_TYPE_SUBMARINE, "Wheels on incorrect vehicle type? (%u)", vehicleType);
// 	#endif // __ASSERT
		dest.m_pWheelPacket = pVehiclePacket->TraversePacketChain(1);
	}

	// Doors
	dest.m_doorCount = pVehiclePacket->GetDoorCount();
	if (dest.m_doorCount > 0)
	{
		dest.m_pDoorPacket = (CPacketVehicleDoorUpdate const*)pVehiclePacket->TraversePacketChain(1 + wheelPktCount);
	} 

	// Frag Data 
	dest.m_hasFragData = pVehiclePacket->HasFragInst();
	if (dest.m_hasFragData)
	{
		dest.m_pFragDataPacket = pVehiclePacket->TraversePacketChain(1 + wheelPktCount + dest.m_doorCount);
		replayAssertf((dest.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS) || (dest.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA) , "CVehInterp::FormCVehInterpData()...Expecting frag data packet.");
	}

	// Frag Bone
	dest.m_bFragHasDamagedBones = pVehiclePacket->DoesFragHaveDamageBones();
	if(dest.m_hasFragData)
	{
		dest.m_pFragBonePacket = (CPacketBase const*)pVehiclePacket->TraversePacketChain(1 + wheelPktCount + dest.m_doorCount + 1);
	}

	u8 *pAfterLastPacket = (u8 *)pVehiclePacket->TraversePacketChain(1 + wheelPktCount + dest.m_doorCount + (dest.m_hasFragData ? 2 : 0), false);
	dest.m_fullPacketSize += (s32)((ptrdiff_t)pAfterLastPacket - (ptrdiff_t)pVehiclePacket);
}


//////////////////////////////////////////////////////////////////////////
bool CVehInterp::GetWheelData(const CVehInterpData& data, s32 wheelID, CWheelFullData &dest) const
{
	replayAssert(wheelID < m_sWheelCount);
	replayAssert(data.m_pWheelPacket != NULL);

#if REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET
	((CPacketVehicleWheelUpdate_Old *)data.m_pWheelPacket)->GetWheelData(wheelID, dest);
#else // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET
	CPacketVehicleWheelUpdate_Old const* pPacketWheelUpdate = (CPacketVehicleWheelUpdate_Old const*)data.m_pWheelPacket->TraversePacketChain(wheelID);
	pPacketWheelUpdate->GetWheelData(dest);
#endif // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CVehInterp::GetPrevWheelDataOld(s32 wheelID, CWheelFullData &dest) const
{
	if(GetPrevPacket() == NULL)	return false;
	return GetWheelData(m_prevData, wheelID, dest);
}

//////////////////////////////////////////////////////////////////////////
bool CVehInterp::GetNextWheelDataOld(s32 wheelID, CWheelFullData &dest) const
{
	if(GetNextPacket() == NULL)	return false;
	return GetWheelData(m_nextData, wheelID, dest);
}


//////////////////////////////////////////////////////////////////////////
CPacketVehicleDoorUpdate const* CVehInterp::GetDoorPacket(const CVehInterpData& data, s32 doorID) const
{
	replayAssert(doorID < data.m_doorCount);
	replayAssert(data.m_pDoorPacket != NULL);

	CPacketVehicleDoorUpdate const* pPacketDoorUpdate = (CPacketVehicleDoorUpdate const*)data.m_pDoorPacket->TraversePacketChain(doorID);
	return pPacketDoorUpdate;
}

//////////////////////////////////////////////////////////////////////////
CPacketVehicleDoorUpdate const* CVehInterp::GetPrevDoorPacket(s32 doorID) const
{
	if(GetPrevPacket() == NULL)	return NULL;
	return GetDoorPacket(m_prevData, doorID);
}

//////////////////////////////////////////////////////////////////////////
CPacketVehicleDoorUpdate const* CVehInterp::GetNextDoorPacket(s32 doorID) const
{
	if(GetNextPacket() == NULL)	return NULL;
	return GetDoorPacket(m_nextData, doorID);
}


//////////////////////////////////////////////////////////////////////////
CPacketBase const* CVehInterp::GetPrevFragDataPacket() const
{
	replayAssert(m_prevData.m_pFragDataPacket != NULL);
	replayAssertf((m_prevData.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS) || (m_prevData.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA), "CVehInterp::GetPrevFragDataPacket()...Expecting frag data packet.");

	if(m_prevData.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS)
		(static_cast < CPacketFragData_NoDamageBits const * >(m_prevData.m_pFragDataPacket))->ValidatePacket();	
	else
		(static_cast < CPacketFragData const * >(m_prevData.m_pFragDataPacket))->ValidatePacket();	

	return m_prevData.m_pFragDataPacket;
}

//////////////////////////////////////////////////////////////////////////
CPacketBase const* CVehInterp::GetNextFragDataPacket() const
{	
	replayAssert(m_nextData.m_pFragDataPacket != NULL);
	replayAssertf((m_nextData.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS) || (m_nextData.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA), "CVehInterp::GetPrevFragDataPacket()...Expecting frag data packet.");

	if(m_nextData.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS)
		(static_cast < CPacketFragData_NoDamageBits const * >(m_nextData.m_pFragDataPacket))->ValidatePacket();	
	else
		(static_cast < CPacketFragData const * >(m_nextData.m_pFragDataPacket))->ValidatePacket();	

	return m_nextData.m_pFragDataPacket;
}

//////////////////////////////////////////////////////////////////////////
CPacketBase const* CVehInterp::GetPrevFragBonePacket() const
{
	replayAssert(m_prevData.m_pFragBonePacket != NULL);
	m_prevData.m_pFragBonePacket->ValidatePacket();	
	return m_prevData.m_pFragBonePacket;
}

//////////////////////////////////////////////////////////////////////////
CPacketBase const* CVehInterp::GetNextFragBonePacket() const
{
	replayAssert(m_nextData.m_pFragBonePacket != NULL);
	m_nextData.m_pFragBonePacket->ValidatePacket();	
	return m_nextData.m_pFragBonePacket;
}

//////////////////////////////////////////////////////////////////////////
void CVehInterp::Reset()
{
	CInterpolator<CPacketVehicleUpdate>::Reset();

	m_sWheelCount = 0;

	m_prevData.Reset();
	m_nextData.Reset();
}

//////////////////////////////////////////////////////////////////////////
CPacketTowTruckArmRotate::CPacketTowTruckArmRotate(float angle)
	: CPacketEvent(PACKETID_TOWTRUCKARMROTATE, sizeof(CPacketTowTruckArmRotate))
	, CPacketEntityInfo()
{
	m_Angle = angle;
}


//////////////////////////////////////////////////////////////////////////
void CPacketTowTruckArmRotate::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);
		
		if(pVehicleGadget && pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
		{
			CVehicleGadgetTowArm* pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
			if(pTowArm->GetPropObject() && pTowArm->GetPropObject()->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
			{
				pTowArm->SnapToAngleForReplay(m_Angle);

 				if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_CURSOR_JUMP)
 				{
 					pTowArm->ProcessPhysics(pVehicle, 0.0f, 0);
 				}
			}
		}
	}
}

CPacketVehicleJumpRechargeTimer::CPacketVehicleJumpRechargeTimer(float rechargeTimer, u32 startOffset, CAutomobile::ECarParachuteState parachuteState)
	: CPacketEvent(PACKETID_VEHICLEJUMPRECHARGE, sizeof(CPacketVehicleJumpRechargeTimer))
	, CPacketEntityInfo()
{
	m_RechargeTimer = rechargeTimer;
	m_StartOffset = startOffset;
	m_ParachuteState = parachuteState;
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleJumpRechargeTimer::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
	{
		CAutomobile* automobile = static_cast<CAutomobile*>(pVehicle);

		// the 0.9f means that we won't start the sound in the last 10%, this prevents the sound from restarting when it ends before the last recorded packet
		if(automobile->GetVehicleAudioEntity() && !automobile->GetVehicleAudioEntity()->IsJumpSoundPlaying() && m_RechargeTimer < (0.95f * CVehicle::ms_fJumpRechargeTime))
		{
			if(!automobile->GetVehicleAudioEntity()->IsJumpSoundPlaying() && m_RechargeTimer < (0.95f * CVehicle::ms_fJumpRechargeTime))
			{
				automobile->SetJumpRechargeTimer(m_RechargeTimer);
				automobile->GetVehicleAudioEntity()->SetRechargeSoundStartOffset(m_StartOffset);
			}
		}
		automobile->SetParachuteState(m_ParachuteState);
	}
}


//////////////////////////////////////////////////////////////////////////
RightHandWheelBones::RightHandWheelBones(CVehicle *pVehicle)
{
	m_NoOfRightHandWheelBones = 0;

	// Collect right-hand wheel bone info from the vehicle.
	CVehicleStructure *pStructure = pVehicle->GetVehicleModelInfo()->GetStructure();

	if(pVehicle->InheritsFromAutomobile())	// Matches check in CVehicle::PreRender2 before the call to ProcessWheelScale
	{
		for(u32 wheelIdx=0; wheelIdx<pVehicle->GetNumWheels(); wheelIdx++)
		{
			CWheel *pWheel = pVehicle->GetWheel(wheelIdx);

			// See CWheel::ProcessWheelScale() in WheelRebdering.cpp.
			if(pWheel->GetConfigFlags().IsFlagSet(WCF_INSTANCED) && !pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
			{
				int nBoneIndex = pStructure->m_nBoneIndices[pWheel->GetHierarchyId()];
				if(nBoneIndex >=0)
					m_RightHandWheelBones[m_NoOfRightHandWheelBones++] = nBoneIndex;
			}
		}
	}
}


bool RightHandWheelBones::ShouldModifyBoneLocalRotation(u32 boneIndex)
{
	for(u32 i=0; i<m_NoOfRightHandWheelBones; i++)
	{
		if(m_RightHandWheelBones[i] == boneIndex)
			return true;
	}
	return false;
}


Quaternion RightHandWheelBones::ModifyBoneLocalRotation(Quaternion &localRotation, Matrix34 const &boneParentMtx, Matrix34 const &entityMtx)
{
	Quaternion ret = localRotation;
	Matrix34 mat;
	mat.FromQuaternion(localRotation);

	Matrix34 boneWorldSpace; 
	boneWorldSpace.Dot(mat, boneParentMtx);

	// Think of the x-axis aligning to the wheel axle, and entityMtx the chassis.
	if(Dot(boneWorldSpace.a, entityMtx.a) > 0.0f)
	{
		mat.a.Scale(-1.0f);
		mat.c.Scale(-1.0f);
		mat.ToQuaternion(ret);
	}
	return ret;
}


CStoredVehicleVariationsREPLAY::CStoredVehicleVariationsREPLAY() : 
	m_color1(0),
	m_color2(0),
	m_color3(0),
	m_color4(0),
	m_smokeColR(0),
	m_smokeColG(0),
	m_smokeColB(0),
	m_neonColR(0),
	m_neonColG(0),
	m_neonColB(0),
	m_neonFlags(0),
	m_windowTint(0),
	m_wheelType(VWT_SPORT),
	m_kitIndex(INVALID_KIT_U8)
{
	for (s32 i = 0; i < VMT_MAX_OLD; ++i)
	{
		m_mods[i] = INVALID_MOD;
	}

	m_modVariation[0] = false;
	m_modVariation[1] = false;
}


void CStoredVehicleVariationsREPLAY::StoreVariations(const CVehicle* pVeh)
{
	if (!pVeh)
		return;

	const CVehicleVariationInstance& VehicleVariationInstance = pVeh->GetVariationInstance();

	m_kitIndex = INVALID_KIT_U8;

	u32 storedModIndex = 0;
	for (u32 mod_loop = 0; mod_loop < sizeof(oldVehicleModArrayReplay1); mod_loop++)
	{
		m_mods[storedModIndex++] = VehicleVariationInstance.GetModIndex(static_cast<eVehicleModType>(oldVehicleModArrayReplay1[mod_loop]));
	}

	for (u32 mod_loop = 0; mod_loop < sizeof(oldVehicleModArrayReplay2); mod_loop++)
	{
		m_mods[storedModIndex++] = VehicleVariationInstance.GetModIndex(static_cast<eVehicleModType>(oldVehicleModArrayReplay2[mod_loop]));
	}

	m_color1 = VehicleVariationInstance.GetColor1();
	m_color2 = VehicleVariationInstance.GetColor2();
	m_color3 = VehicleVariationInstance.GetColor3();
	m_color4 = VehicleVariationInstance.GetColor4();

	m_smokeColR = VehicleVariationInstance.GetSmokeColorR();
	m_smokeColG = VehicleVariationInstance.GetSmokeColorG();
	m_smokeColB = VehicleVariationInstance.GetSmokeColorB();

	m_neonColR = VehicleVariationInstance.GetNeonColour().GetRed();
	m_neonColG = VehicleVariationInstance.GetNeonColour().GetGreen();
	m_neonColB = VehicleVariationInstance.GetNeonColour().GetBlue();
	m_neonFlags = 0;
	if(VehicleVariationInstance.IsNeonLOn())
		m_neonFlags |= 1;
	if(VehicleVariationInstance.IsNeonROn())
		m_neonFlags |= 2;
	if(VehicleVariationInstance.IsNeonFOn())
		m_neonFlags |= 4;
	if(VehicleVariationInstance.IsNeonBOn())
		m_neonFlags |= 8;

	m_windowTint = VehicleVariationInstance.GetWindowTint();

	m_wheelType = (u8)VehicleVariationInstance.GetWheelType(pVeh);

	m_modVariation[0] = VehicleVariationInstance.HasVariation(0);
	m_modVariation[1] = VehicleVariationInstance.HasVariation(1);
}


#endif // GTA_REPLAY
