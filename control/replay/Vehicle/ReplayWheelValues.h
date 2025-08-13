#ifndef WHEEL_VALUES_H
#define WHEEL_VALUES_H

#include "Control/replay/PacketBasics.h"

#if GTA_REPLAY
#include "vehicles/wheel.h"

class CVehicle;

extern converter<u16> u16_f32_0_to_1000_SuspensionAndTyreHealth;

//========================================================================================================================
// WheelValues.
//========================================================================================================================

typedef ReplayCompressedFloat fReplayWheelValues;

// Per wheel info Replay records. It`s used as a "keyframe" on a per block basis and changes from these are recorded in CPacketWheel.
class ReplayWheelValues
{
public:
	typedef struct REPLAY_WHEEL_VALUES
	{
		// Base data.
		u32	m_DynamicFlags;
		// Dynamic data.
		fReplayWheelValues	m_tyreTemp;
		fReplayWheelValues	m_tyreLoad;
		fReplayWheelValues	m_fwdSlipAngle;
		fReplayWheelValues	m_sideSlipAngle;
		fReplayWheelValues	m_Compression;
		fReplayWheelValues	m_CompressionOld;
		fReplayWheelValues	m_staticDelta;
		// More dynamic data.
		CPacketVector3Comp <fReplayWheelValues>	m_vecGroundContactVelocity;
		CPacketVector3Comp <fReplayWheelValues> m_vecTyreContactVelocity;

		valComp<s8, s8_f32_nPI_to_PI>	m_steeringAngle;
		valComp<s8, s8_f32_nPI_to_PI>	m_rotAngle;
		valComp<u8, u8_f32_0_to_5>			m_BrakeForce;			// 0 to 5	   - f32 - See \gta\build\common\data\handling.dat
		valComp<u8, u8_f32_0_to_3_0>		m_FrictionDamage;		// 0 to 3.0	   - f32 - Range obtained by trial & error
		valComp<s8, s8_f32_n20_to_20>		m_wheelCompression;

		CPacketVector3Comp<valComp<s16, s16_f32_n4_to_4>>	m_vecHitPos;
		CPacketVector3Comp<valComp<s16, s16_f32_n4_to_4>>	m_vecHitCentrePos;
		CPacketVector3Comp<valComp<s16, s16_f32_n4_to_4>>	m_vecHitNormal;
		// Health data.
		valComp<u16, u16_f32_0_to_1000_SuspensionAndTyreHealth>		m_SuspensionHealth; // 0 to 1000   - f32 - See wheel.h
		valComp<u16, u16_f32_0_to_1000_SuspensionAndTyreHealth>		m_TyreHealth;		// 0 to 1000   - f32 - See wheel.h
		valComp<s16, s16_f32_n650_to_650>	m_RotAngVel;			// -650 to 650 - f32 - Range obtained by trial & error

		u16	m_ConfigFlagsLower16;	// There are only 22 config flags (see wheel.h).
		u8  m_ConfigFlagsUpper8;
		valComp<u8, u8_f32_0_to_65>			m_EffectiveSlipAngle;	// 0 to 65     - f32 - Range obtained by trial & error
	} REPLAY_WHEEL_VALUES;

	CompileTimeAssert((sizeof(REPLAY_WHEEL_VALUES) % 4) == 0); // We require 4 byte size alignment.
	CompileTimeAssert((sizeof(REPLAY_WHEEL_VALUES) >> 2) <= 32); // We use 1 bit per 4 bytes. Must be less than 32.
	
public:
	ReplayWheelValues() {}
	~ReplayWheelValues() {}

public:
	void	Extract(ReplayController& controller, CVehicle* pVehicle, CWheel* pWheel, ReplayWheelValues const* pNextPacket = NULL, f32 fInterpValue = 0.0f) const;
	void	ExtractInterpolateableValues(CVehicle* pVehicle, CWheel* pWheel, ReplayWheelValues const* pNextPacket = NULL, f32 fInterp = 0.0f) const;

	float	GetWheelCompression() const		{ return m_Values.m_wheelCompression;	}
	float	GetSteeringAngle() const		{ return m_Values.m_steeringAngle;		}
	float	GetRotAngle() const				{ return m_Values.m_rotAngle;			}
	float	GetSuspensionHealth() const		{ return m_Values.m_SuspensionHealth;	}
	float	GetTyreHealth()	const			{ return m_Values.m_TyreHealth;			}
	float	GetBrakeForce()	const			{ return m_Values.m_BrakeForce;			}
	float	GetFrictionDamage()	const		{ return m_Values.m_FrictionDamage;		}
	float	GetCompression() const			{ return m_Values.m_Compression;			}
	float	GetCompressionOld()	const		{ return m_Values.m_CompressionOld;		}
	float	GetStaticDelta() const			{ return m_Values.m_staticDelta;			}
	float	GetRotAngVel() const			{ return m_Values.m_RotAngVel;			}
	float	GetEffectiveSlipAngle() const	{ return m_Values.m_EffectiveSlipAngle;	}

	float	GetTyreLoad() const				{ return m_Values.m_tyreLoad; }
	float	GetTyreTemp() const				{ return m_Values.m_tyreTemp; }
	float	GetFwdSlipAngle() const			{ return m_Values.m_fwdSlipAngle; }
	float	GetSideSlipAngle() const		{ return m_Values.m_sideSlipAngle; }

public:
	void CollectFromWheel(CVehicle *pVehicle, CWheel *pWheel);

public:
	u32 *PackDifferences(class ReplayWheelValues *pOther, u32 *pDest);
	u32 *Unpack(u32 *pPackedValues, class ReplayWheelValues *pDest);

private:
	REPLAY_WHEEL_VALUES m_Values;
};


//////////////////////////////////////////////////////////////////////////
class ReplayWheelValuesPair
{
public:
	ReplayWheelValuesPair() {}
	~ReplayWheelValuesPair() {}
public:
	void Initialize(CVehicle *pVehicle, CWheel *pWheel) { m_OddAndEven[0].CollectFromWheel(pVehicle, pWheel); m_OddAndEven[1] = m_OddAndEven[0]; }
public:
	ReplayWheelValues m_OddAndEven[2];
};

//========================================================================================================================
// WheelValueExtensionData.
//========================================================================================================================

class WheelValueExtensionData
{
public:
	WheelValueExtensionData() 
	{
		m_RecordSession = 0;
	};
	~WheelValueExtensionData() 
	{
	};

public:
	void Reset();
	void InitializeForRecording(CVehicle *pVehicle, u16 sessionBlockIndex);
	void UpdateReferenceSetDuringRecording(CVehicle *pVehicle, u16 sessionBlockIndex);
	void InitializeForPlayback(u32 noOfWheels, u32 sessionBlockIndex0, u32 sessionBlockIndex1, ReplayWheelValuesPair *pWheelValuePairs);

public:
	u32 StoreDifferencesFromReferenceSetInPackedStream(u16 sessionBlockIndex, u32 *pDest);
	void FormWheelValuesFromPackedStreamAndReferenceSet(u32 sessionBlockIndex, u32 noOfWheels, u32 *pSrcPackedData, ReplayWheelValues *pDest);

public:
	u32						m_RecordSession;
	u32						m_NoOfWheels;
	u16						m_sessionBlockIndices[2];
	ReplayWheelValuesPair	m_ReferenceWheelValues[NUM_VEH_CWHEELS_MAX];
};

#endif // GTA_REPLAY
#endif // WHEEL_VALUES_H



