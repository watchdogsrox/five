
#include "ReplayWheelValues.h"

#if GTA_REPLAY

#include "vehicles/vehicle.h"
#include "control/replay/ReplayInterfaceVeh.h"

extern f32 InterpolateWrappableValueFromS8(f32 difference, s8 currentValue, s8 nextValue, f32 interpolationStep);
extern f32 InterpolateWrappableValueFrom(f32 currentValue, f32 nextValue, f32 interpolationStep);

//========================================================================================================================
// WheelValues.
//========================================================================================================================

// Using the range [0, 64000] over-comes rounding down errors when decompressing 1000.0f.
converter<u16> u16_f32_0_to_1000_SuspensionAndTyreHealth(0,	64000, 0.0f, 1000.0f);

void ReplayWheelValues::Extract(ReplayController& controller, CVehicle* pVehicle, CWheel* pWheel, ReplayWheelValues const* pNextPacket, f32 fInterpValue) const
{
	if(!pNextPacket)
	{
		// No interpolation
		ExtractInterpolateableValues( pVehicle, pWheel );
	}
	else
	{
		// Interpolate values
		ExtractInterpolateableValues( pVehicle, pWheel, pNextPacket, fInterpValue );
	}

	// Extract values that do not need interpolation
	pWheel->m_nDynamicFlags = m_Values.m_DynamicFlags;
	pWheel->m_nConfigFlags =  (u32)m_Values.m_ConfigFlagsLower16 | ((u32)m_Values.m_ConfigFlagsUpper8 << 16);

	m_Values.m_vecHitPos.Load(pWheel->m_vecHitPos);
	m_Values.m_vecHitCentrePos.Load(pWheel->m_vecHitCentrePos);
	m_Values.m_vecHitNormal.Load(pWheel->m_vecHitNormal);
	m_Values.m_vecGroundContactVelocity.Load(pWheel->m_vecGroundContactVelocity);
	m_Values.m_vecTyreContactVelocity.Load(pWheel->m_vecTyreContactVelocity);

	Vector3 wheelPosition;
	pWheel->GetWheelPosAndRadius(wheelPosition);
	pWheel->m_vecHitPos += wheelPosition;
	pWheel->m_vecHitCentrePos += wheelPosition;

	pWheel->m_fTyreLoad				= GetTyreLoad();
	pWheel->m_fTyreTemp				= GetTyreTemp();

	if( pVehicle->IsReplayOverrideWheelsRot() )
	{
		if(crSkeleton * pSkeleton = pVehicle->GetSkeleton())
		{
			bool leftWheel = pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL);
			bool plyFwd = controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD);

			//get time step to apply to wheel rot, fudge backwards dt.
			float timeStep = plyFwd ? float( fwTimer::GetReplayTimeStepInMilliseconds() / 1000.0f ) : 0.016f;

			//get current bone matrix
			Mat34V boneMtx;
			pSkeleton->GetGlobalMtx(pVehicle->GetBoneIndex(pWheel->GetHierarchyId()), boneMtx);
			
			//don't update if replay is paused
			if( !controller.GetPlaybackFlags().IsSet(REPLAY_STATE_PAUSE) )
			{
				Vector3 vForward = VEC3V_TO_VECTOR3(pVehicle->GetMatrix().GetCol1());
				float fCurrentFwdSpeed = pVehicle->GetVelocity().Dot(vForward);
				fCurrentFwdSpeed =  plyFwd ? -fCurrentFwdSpeed : fCurrentFwdSpeed;

				float newRotAngVel = fCurrentFwdSpeed / pWheel->GetWheelRadius();
	
				float angleVelo = newRotAngVel * timeStep;
				float wheelRot = leftWheel ? pWheel->m_fRotAng + angleVelo : pWheel->m_fRotAng - angleVelo;

				pWheel->SetRotAngle(wheelRot);
			}

			//re apply new matrix rotation using rotang
			Mat34VRotateLocalX(boneMtx,ScalarV(pWheel->m_fRotAng));
			
			//apply that to the skeleton matrix
			pSkeleton->SetGlobalMtx(pVehicle->GetBoneIndex(pWheel->GetHierarchyId()), boneMtx);
		}
	}
}


//=====================================================================================================
void ReplayWheelValues::ExtractInterpolateableValues(CVehicle* pVehicle, CWheel* pWheel, ReplayWheelValues const* pNextPacket, f32 fInterp) const
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

	if( !pVehicle->IsReplayOverrideWheelsRot() )
	{
		pWheel->SetRotAngle(interpRotAng);
	}

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


//////////////////////////////////////////////////////////////////////////


#if 0
// LDS DMC TEMP:-
void TestFloat(float v)
{
	ReplayCompressedFloat rF;
	rF = v;

	float fOut = (float)rF;

	replayDisplayf("Original %f, Replay %f (%f percent error)", v, fOut, fabsf(v - fOut)*100.0f/fabsf(v));
}
#endif 

void ReplayWheelValues::CollectFromWheel(CVehicle *pVehicle, CWheel *pWheel)
{
    (void)pVehicle;

	// Safety Checks
	// - Making sure that the ranges we assume when compressing values are true
	replayAssertf( pWheel->m_fSuspensionHealth		>= 0	&& pWheel->m_fSuspensionHealth		<= 1000,"WheelValues::CollectFromWheel(): Suspension Health %f",		pWheel->m_fSuspensionHealth		);
	replayAssertf( pWheel->m_fTyreHealth			>= 0	&& pWheel->m_fTyreHealth			<= 1000,"WheelValues::CollectFromWheel(): Tyre Health %f",				pWheel->m_fTyreHealth			);
	replayAssertf( pWheel->m_fBrakeForce			>= 0	&& pWheel->m_fBrakeForce			<= 5,	"WheelValues::CollectFromWheel(): Brake Force %f",				pWheel->m_fBrakeForce			);

	// Base data.
	m_Values.m_DynamicFlags			= pWheel->m_nDynamicFlags;
	m_Values.m_ConfigFlagsLower16	= (u16)(pWheel->m_nConfigFlags & 0xffff);
	m_Values.m_ConfigFlagsUpper8	= (u8)((pWheel->m_nConfigFlags >> 16) & 0xff);
	m_Values.m_steeringAngle		= pWheel->GetSteeringAngle();
	m_Values.m_rotAngle				= rage::Wrap(pWheel->m_fRotAng, -PI, PI);

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
	replayAssertf(IsLessThanOrEqualAll(Abs(hitPosMinusWheelPos), Vec3V(V_TWO)), "WheelValues::CollectFromWheel()...Wheel radius too high (> 2m).");
	replayAssertf(IsLessThanOrEqualAll(Abs(hitCentrePosMinusWheelPos), Vec3V(V_TWO)), "WheelValues::CollectFromWheel()...Wheel radius too high (> 2m).");
	m_Values.m_vecHitPos.Store(RCC_VECTOR3(hitPosMinusWheelPos));
	m_Values.m_vecHitCentrePos.Store(RCC_VECTOR3(hitCentrePosMinusWheelPos));
	m_Values.m_vecHitNormal.Store(pWheel->m_vecHitNormal);

	// Dynamic data.
	m_Values.m_BrakeForce			= pWheel->m_fBrakeForce;
	m_Values.m_FrictionDamage		= pWheel->m_fFrictionDamage;
	m_Values.m_Compression			= pWheel->m_fCompression;
	m_Values.m_CompressionOld		= pWheel->m_fCompressionOld;
	m_Values.m_staticDelta			= pWheel->m_fStaticDelta;
	m_Values.m_RotAngVel			= Clamp(pWheel->m_fRotAngVel, m_Values.m_RotAngVel.GetMin(), m_Values.m_RotAngVel.GetMax());

	// B* 2214047 - Very occasionally we go out of range, we can't alter range without breaking clips, so just clamp!
	float effectiveSlipAngle = pWheel->m_fEffectiveSlipAngle;
	m_Values.m_EffectiveSlipAngle	= Clamp(effectiveSlipAngle, u8_f32_0_to_65.GetMin(), u8_f32_0_to_65.GetMax());

	float wheelCompression = pWheel->GetWheelCompression();
#if __BANK
	if(wheelCompression < s8_f32_n20_to_20.GetMin() || wheelCompression > s8_f32_n20_to_20.GetMax())
		replayDebugf1("ReplayWheelValues::CollectFromWheel()..Wheel compression out of range %f (%f, %f).", wheelCompression, s8_f32_n20_to_20.GetMin(), s8_f32_n20_to_20.GetMax());
#endif //__BANK
	m_Values.m_wheelCompression		= Clamp(wheelCompression, s8_f32_n20_to_20.GetMin(), s8_f32_n20_to_20.GetMax());
	m_Values.m_fwdSlipAngle			= pWheel->GetFwdSlipAngle();
	m_Values.m_sideSlipAngle		= pWheel->GetSideSlipAngle();
	m_Values.m_tyreLoad				= pWheel->m_fTyreLoad;
	m_Values.m_tyreTemp				= pWheel->m_fTyreTemp;
	m_Values.m_vecGroundContactVelocity.Store(pWheel->m_vecGroundContactVelocity);
	m_Values.m_vecTyreContactVelocity.Store(pWheel->m_vecTyreContactVelocity);

	// Health data.
	m_Values.m_SuspensionHealth	= pWheel->m_fSuspensionHealth;
	m_Values.m_TyreHealth		= pWheel->m_fTyreHealth;

#if 0
	// LDS DMC TEMP:-
	if(pVehicle->ContainsPlayer())
	{
		TestFloat(pWheel->GetWheelCompression());
		TestFloat(pWheel->GetFwdSlipAngle());
		TestFloat(pWheel->GetSideSlipAngle());
		TestFloat(pWheel->m_fTyreLoad);
		TestFloat(pWheel->m_fTyreTemp);
		TestFloat(pWheel->m_vecGroundContactVelocity.GetX());
		TestFloat(pWheel->m_vecGroundContactVelocity.GetY());
		TestFloat(pWheel->m_vecGroundContactVelocity.GetZ());
		TestFloat(pWheel->m_vecTyreContactVelocity.GetX());
		TestFloat(pWheel->m_vecTyreContactVelocity.GetY());
		TestFloat(pWheel->m_vecTyreContactVelocity.GetZ());
	}
#endif // 0
}


//////////////////////////////////////////////////////////////////////////
u32 *ReplayWheelValues::PackDifferences(class ReplayWheelValues *pOther, u32 *pDest)
{
	u32 *pBase = (u32 *)&m_Values;
	u32 *pOtherOne = (u32 *)&pOther->m_Values;

	u32 bitHeader = 0;
	u32 *pBitHeader = pDest++;

	for(u32 i=0; i<(sizeof(REPLAY_WHEEL_VALUES) >> 0x2); i++)
	{
		if(pBase[i] != pOtherOne[i])
		{
			bitHeader |= 0x1 << i;
			*pDest++ = pOtherOne[i];
		}
	}
	*pBitHeader = bitHeader;

	return pDest;
}


u32 *ReplayWheelValues::Unpack(u32 *pPackedValues, class ReplayWheelValues *pOut)
{
	u32 *pBase = (u32 *)&m_Values;
	u32 *pDest = (u32 *)&pOut->m_Values;
	u32 bitHeader = *pPackedValues++;

	for(u32 i=0; i<(sizeof(REPLAY_WHEEL_VALUES) >> 0x2); i++)
	{
		if(bitHeader & (0x1 << i))
			*pDest++ = *pPackedValues++;
		else
			*pDest++ = *pBase;

		pBase++;
	}
	return pPackedValues;
}

//========================================================================================================================
// WheelValueExtensionData.
//========================================================================================================================

void WheelValueExtensionData::Reset()
{
	m_RecordSession = 0;
	m_NoOfWheels = 0;
	m_sessionBlockIndices[0] = 0;
	m_sessionBlockIndices[1] = 0;
	sysMemSet(m_ReferenceWheelValues, 0, sizeof(m_ReferenceWheelValues));
}

void WheelValueExtensionData::InitializeForRecording(CVehicle *pVehicle, u16 sessionBlockIndex)
{
	m_NoOfWheels = pVehicle->GetNumWheels();

	m_NoOfWheels = pVehicle->GetNumWheels();
	m_sessionBlockIndices[0] = m_sessionBlockIndices[1] = 0xffff;
	m_sessionBlockIndices[sessionBlockIndex & 0x1] = sessionBlockIndex;

	for(u32 i=0; i<m_NoOfWheels; i++)
	{
		CWheel *pWheel = pVehicle->GetWheel(i);

		if(pWheel)
			m_ReferenceWheelValues[i].Initialize(pVehicle, pWheel);
	}
}


// Updates current wheel values from a vehicle.
void WheelValueExtensionData::UpdateReferenceSetDuringRecording(CVehicle *pVehicle, u16 sessionBlockIndex)
{
	// If we`re on an "odd" block, update the "even" values (we use the odd ones as the base/defaults for this current session block).
	u32 destIndex = (sessionBlockIndex + 1) & 0x1;
	u32 currentIndex = sessionBlockIndex & 0x1;

	m_sessionBlockIndices[currentIndex] = sessionBlockIndex;

	for(u32 i=0; i<m_NoOfWheels; i++)
	{
		CWheel *pWheel = pVehicle->GetWheel(i);

		if(pWheel)
			m_ReferenceWheelValues[i].m_OddAndEven[destIndex].CollectFromWheel(pVehicle, pWheel);
	}
}


void WheelValueExtensionData::InitializeForPlayback(u32 noOfWheels, u32 sessionBlockIndex0, u32 sessionBlockIndex1, ReplayWheelValuesPair *pWheelValuePairs)
{
	m_NoOfWheels = noOfWheels;
	m_sessionBlockIndices[0] = (u16)sessionBlockIndex0;
	m_sessionBlockIndices[1] = (u16)sessionBlockIndex1;

	for(u32 i=0; i<noOfWheels; i++)
		m_ReferenceWheelValues[i] = pWheelValuePairs[i];
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stores compressed version of current wheel values as differences from current "default".
u32 WheelValueExtensionData::StoreDifferencesFromReferenceSetInPackedStream(u16 sessionBlockIndex, u32 *pDestPackedData)
{
	u32 noOf4ByteBlocks = 0;
	u32 baseIndex = sessionBlockIndex & 0x1;
	u32 latestValuesIndex = (baseIndex + 1) & 0x1;

	for(u32 i=0; i<m_NoOfWheels; i++)
	{
		u32 *pDestAfter = m_ReferenceWheelValues[i].m_OddAndEven[baseIndex].PackDifferences(&m_ReferenceWheelValues[i].m_OddAndEven[latestValuesIndex], pDestPackedData);
		noOf4ByteBlocks += (u32)(pDestAfter - pDestPackedData);
		pDestPackedData = pDestAfter;
	}
	return noOf4ByteBlocks;
}


// Re-constitutes compressed wheel values. 
void WheelValueExtensionData::FormWheelValuesFromPackedStreamAndReferenceSet(u32 sessionBlockIndex, u32 noOfWheels, u32 *pSrcPackedData, ReplayWheelValues *pDest)
{
	u32 baseIndex = sessionBlockIndex & 0x1;

	for(u32 i=0; i<noOfWheels; i++)
	{
		u32 *pSrcPackedDataAfter = m_ReferenceWheelValues[i].m_OddAndEven[baseIndex].Unpack(pSrcPackedData, &pDest[i]);
		pSrcPackedData = pSrcPackedDataAfter;
	}
}

#endif // GTA_REPLAY
