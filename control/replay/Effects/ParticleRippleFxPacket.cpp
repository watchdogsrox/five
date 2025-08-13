//=====================================================================================================
// name:		ParticleRippleFxPacket.cpp
//=====================================================================================================

#include "ParticleRippleFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "animation/AnimBones.h"
#include "peds/ped.h"
#include "physics/Floater.h"
#include "renderer/Water.h"
#include "vehicles/boat.h"
#include "vehicles/heli.h"
#include "vfx/Systems/VfxWater.h"
#include "Vfx/Systems/VfxRipple.h"

#if DEBUG_REPLAY_PFX
	#include "control/replay/ReplayInternal.h"
#endif // DEBUG_REPLAY_PFX
// ============================================================================
// ============================================================================
/*TODO4FIVE extern animatedPedData_s g_animatedPedSampleBoneTags[NUM_ANIMATED_PED_SAMPLES];*/
extern CVfxWater	g_vfxWater;
// ============================================================================
// ============================================================================
////// ripples																		ped		boat	heli
////TweakFloat			RIPPLE_SPEED_THRESH_A				[NUM_RIPPLE_TYPES]	= {	1.50f,	0.20f,	0.15f	};
////TweakInt32			RIPPLE_FREQ_A						[NUM_RIPPLE_TYPES]	= {	10,		4,		4		};
////TweakFloat			RIPPLE_START_SIZE_MIN_A				[NUM_RIPPLE_TYPES]	= {	0.00f,	0.30f,	4.0f	};
////TweakFloat			RIPPLE_START_SIZE_MAX_A				[NUM_RIPPLE_TYPES]	= {	0.00f,	0.40f,	7.0f	};
////TweakFloat			RIPPLE_SPEED_MIN_A					[NUM_RIPPLE_TYPES]	= {	0.60f,	0.40f,	21.0f	};
////TweakFloat			RIPPLE_SPEED_MAX_A					[NUM_RIPPLE_TYPES]	= {	0.70f,	0.50f,	25.0f	};
////TweakFloat			RIPPLE_LIFE_MIN_A					[NUM_RIPPLE_TYPES]	= {	1.30f,	1.00f,	0.80f	};
////TweakFloat			RIPPLE_LIFE_MAX_A					[NUM_RIPPLE_TYPES]	= {	1.60f,	2.50f,	0.95f	};
////TweakFloat			RIPPLE_ALPHA_MIN_A					[NUM_RIPPLE_TYPES]	= {	0.22f,	0.40f,	0.20f	};
////TweakFloat			RIPPLE_ALPHA_MAX_A					[NUM_RIPPLE_TYPES]	= {	0.30f,	0.50f,	0.28f	};
////TweakFloat			RIPPLE_PEAK_ALPHA_RATIO_MIN_A		[NUM_RIPPLE_TYPES]	= {	0.0f,	0.25f,	0.22f	};
////TweakFloat			RIPPLE_PEAK_ALPHA_RATIO_MAX_A		[NUM_RIPPLE_TYPES]	= {	0.0f,	0.30f,	0.40f	};
////TweakFloat			RIPPLE_ROT_SPEED_MIN_A				[NUM_RIPPLE_TYPES]	= {	-25.0f, -5.0f,	-30.0f	};
////TweakFloat			RIPPLE_ROT_SPEED_MAX_A				[NUM_RIPPLE_TYPES]	= {	25.0f,	5.0f,	30.0f	};
////TweakFloat			RIPPLE_ROT_INIT_MIN_A				[NUM_RIPPLE_TYPES]	= {	0.0f,	0.0f,	-180.0f	};
////TweakFloat			RIPPLE_ROT_INIT_MAX_A				[NUM_RIPPLE_TYPES]	= {	0.0f,	0.0f,	180.0f	};
////
////TweakFloat			RIPPLE_SPEED_THRESH_B				[NUM_RIPPLE_TYPES]	= {	2.50f,	20.00f,	1.00f	};
////TweakInt32			RIPPLE_FREQ_B						[NUM_RIPPLE_TYPES]	= {	5,		100,	7		};
////TweakFloat			RIPPLE_START_SIZE_MIN_B				[NUM_RIPPLE_TYPES]	= {	0.00f,	0.30f,	8.0f	};
////TweakFloat			RIPPLE_START_SIZE_MAX_B				[NUM_RIPPLE_TYPES]	= {	0.00f,	0.40f,	10.0f	};
////TweakFloat			RIPPLE_SPEED_MIN_B					[NUM_RIPPLE_TYPES]	= {	0.40f,	0.30f,	14.0f	};
////TweakFloat			RIPPLE_SPEED_MAX_B					[NUM_RIPPLE_TYPES]	= {	0.50f,	0.50f,	19.0f	};
////TweakFloat			RIPPLE_LIFE_MIN_B					[NUM_RIPPLE_TYPES]	= {	1.00f,	0.10f,	1.0f	};
////TweakFloat			RIPPLE_LIFE_MAX_B					[NUM_RIPPLE_TYPES]	= {	1.20f,	0.10f,	1.4f	};
////TweakFloat			RIPPLE_ALPHA_MIN_B					[NUM_RIPPLE_TYPES]	= {	0.22f,	0.00f,	0.05f	};
////TweakFloat			RIPPLE_ALPHA_MAX_B					[NUM_RIPPLE_TYPES]	= {	0.30f,	0.00f,	0.09f	};
////TweakFloat			RIPPLE_PEAK_ALPHA_RATIO_MIN_B		[NUM_RIPPLE_TYPES]	= {	0.0f,	0.0f,	0.22f	};
////TweakFloat			RIPPLE_PEAK_ALPHA_RATIO_MAX_B		[NUM_RIPPLE_TYPES]	= {	0.0f,	0.0f,	0.30f	};
////TweakFloat			RIPPLE_ROT_SPEED_MIN_B				[NUM_RIPPLE_TYPES]	= {	0.00f,	-0.0f,	-10.0f	};
////TweakFloat			RIPPLE_ROT_SPEED_MAX_B				[NUM_RIPPLE_TYPES]	= {	0.00f,	0.0f,	10.0f	};
////TweakFloat			RIPPLE_ROT_INIT_MIN_B				[NUM_RIPPLE_TYPES]	= {	0.0f,	0.0f,	-180.0f	};
////TweakFloat			RIPPLE_ROT_INIT_MAX_B				[NUM_RIPPLE_TYPES]	= {	0.0f,	0.0f,	180.0f	};


//========================================================================================================================
void CPacketRippleWakeFx::StorePos(const Vector3& rPos)
{
	m_Position[0] = rPos.x;
	m_Position[1] = rPos.y;
	m_Position[2] = rPos.z;
}

//========================================================================================================================
void CPacketRippleWakeFx::LoadPos(Vector3& rPos) const
{
	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
CPacketRippleWakeFx::CPacketRippleWakeFx(const Vector3& rPos, bool bFlipTexture, s32 eWakeType, u16 uBoneIdx)
: CPacketEvent(PACKETID_RIPPLEWAKEFX, sizeof(CPacketRippleWakeFx))
, CPacketEntityInfo()
{
	StorePos(rPos);
	m_bFlipTexture = bFlipTexture;
	m_eWakeType = (u8)eWakeType;
	m_uBoneIdx = uBoneIdx;
}

//========================================================================================================================
void CPacketRippleWakeFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	(void) pEntity;

	/*TODO4FIVE
	Vector3 oVecPos;
	LoadPos(oVecPos);

	if (m_eWakeType == VFXRIPPLE_TYPE_BOAT_WAKE)
	{
		CBoat* pBoat = (CBoat*)pEntity;

		float rippleRatio = 0.0f;
		float vehSpeed = pBoat->GetVelocity().Mag();
		if (vehSpeed<=VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_BOAT_WAKE])
		{
			rippleRatio = 0.0f;
		}
		else if (vehSpeed>=VFXRIPPLE_SPEED_THRESH_B[VFXRIPPLE_TYPE_BOAT_WAKE])
		{
			rippleRatio = 1.0f;
		}
		else
		{
			rippleRatio = (vehSpeed-VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_BOAT_WAKE])/(VFXRIPPLE_SPEED_THRESH_B[VFXRIPPLE_TYPE_BOAT_WAKE]-VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_BOAT_WAKE]);
			replayAssert(rippleRatio>0.0f && rippleRatio<1.0f);
		}

		g_vfxWater.GetRippleInterface().Register(VFXRIPPLE_TYPE_BOAT_WAKE, VECTOR3_TO_VEC3V(oVecPos), pBoat->GetMatrix().b(), 1.0f, rippleRatio, m_bFlipTexture);
	}
	else if (m_eWakeType == VFXRIPPLE_TYPE_HELI)
	{
		CHeli* pHeli = (CHeli*)pEntity;

		// check if the rotor is in water
		CVehicleModelInfo* pModelInfo = static_cast<CVehicleModelInfo*>(CModelInfo::GetBaseModelInfo(fwModelId(pHeli->GetModelIndex())));
		(void)pModelInfo;
		s32 rotorBoneId = pModelInfo->GetBoneId(HELI_MOVING_ROTOR);
		if (rotorBoneId==-1)
		{
			// try getting the static rotor instead
			rotorBoneId = pModelInfo->GetBoneId(HELI_STATIC_ROTOR);

			if (rotorBoneId==-1)
			{
				replayAssert(0 && "CPacketRippleWakeFx::Extract - No Rotors on Heli? see KH");
			}
		}

		Matrix34 pMatRotor;
		pHeli->GetGlobalMtx(rotorBoneId, pMatRotor);

		Vector3 vec = pMatRotor.d - oVecPos;
		float distEvo = vec.Mag() / 20.0f;
		replayAssert(distEvo>=0.0f);
		replayAssert(distEvo<=1.0f);

		float rippleRatio = 0.0f;
		if (distEvo<=VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_HELI])
		{
			rippleRatio = 0.0f;
		}
		else if (distEvo>=VFXRIPPLE_SPEED_THRESH_B[VFXRIPPLE_TYPE_HELI])
		{
			rippleRatio = 1.0f;
		}
		else
		{
			rippleRatio = (distEvo-VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_HELI])/(VFXRIPPLE_SPEED_THRESH_B[VFXRIPPLE_TYPE_HELI]-VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_HELI]);
			replayAssert(rippleRatio>0.0f && rippleRatio<1.0f);
		}

		g_vfxWater.GetRippleInterface().Register(VFXRIPPLE_TYPE_BOAT_WAKE, VECTOR3_TO_VEC3V(oVecPos), pHeli->GetMatrix().b(), 1.0f, rippleRatio, false);

		// disturb the water surface
		float disturbVal = -0.5f * (1.0f-rippleRatio);
		s32 worldXInt = static_cast<s32>(oVecPos.x);
		s32 worldYInt = static_cast<s32>(oVecPos.y);
		CWater::AddToDynamicWaterSpeed(worldXInt, worldYInt, disturbVal, true);
		CWater::AddToDynamicWaterSpeed(worldXInt, worldYInt+4, disturbVal, true);
		CWater::AddToDynamicWaterSpeed(worldXInt, worldYInt-4, disturbVal, true);
		CWater::AddToDynamicWaterSpeed(worldXInt+4, worldYInt, disturbVal, true);
		CWater::AddToDynamicWaterSpeed(worldXInt-4, worldYInt, disturbVal, true);

	}
	else if (m_eWakeType == VFXRIPPLE_TYPE_PED)
	{
		CPed* pPed = (CPed*)pEntity;

		float pedSpeed = pPed->GetVelocity().Mag();
		float rippleRatio = 0.0f;
		if (pedSpeed<=VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_PED])
		{
			rippleRatio = 0.0f;
		}
		else if (pedSpeed>=VFXRIPPLE_SPEED_THRESH_B[VFXRIPPLE_TYPE_PED])
		{
			rippleRatio = 1.0f;
		}
		else
		{
			rippleRatio = (pedSpeed-VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_PED])/(VFXRIPPLE_SPEED_THRESH_B[VFXRIPPLE_TYPE_PED]-VFXRIPPLE_SPEED_THRESH_A[VFXRIPPLE_TYPE_PED]);
			replayAssert(rippleRatio>0.0f && rippleRatio<1.0f);
		}

		g_vfxWater.GetRippleInterface().Register(VFXRIPPLE_TYPE_BOAT_WAKE, VECTOR3_TO_VEC3V(, pPed->GetMatrix().b, g_animatedPedSampleBoneTags[m_uBoneIdx].rippleLifeScale, rippleRatio, false);
	}
	else
		replayAssert(0 && "CPacketRippleWakeFx::Extract - Invalid wake type, see KH");
*/
#if DEBUG_REPLAY_PFX
	replayDebugf1("REPLAY: RIPPLE WAKE FX: %0X: ID: %0X Frame: %d\n", (u32)pEntity, pEntity->GetReplayID(), CReplayMgr::GetCurrentFrame());
#endif
}

//========================================================================================================================
void CPacketDynamicWaveFx::StorePosInt(const s32& x, const s32& y, const float& z)
{
	m_sPosition[0] = x;
	m_sPosition[1] = y;
	m_fPosition[2] = z;
}

//========================================================================================================================
void CPacketDynamicWaveFx::LoadPosInt(s32& x, s32& y, float& z) const
{
	x = m_sPosition[0];	// s32
	y = m_sPosition[1];	// s32
	z = m_fPosition[2];	// float
}

//========================================================================================================================
CPacketDynamicWaveFx::CPacketDynamicWaveFx(s32 x, s32 y, float fSpeedDown, float fCustom)
	: CPacketEvent(PACKETID_DYNAMICWAVEFX, sizeof(CPacketDynamicWaveFx))
{
	StorePosInt(x, y, fSpeedDown);
	m_fCustom = fCustom;
}

//========================================================================================================================
void CPacketDynamicWaveFx::Extract(const CEventInfo<void>&) const
{
	s32 x, y;
	float fSpeed;
	LoadPosInt(x, y, fSpeed);

	/*TODO4FIVE CWater::ModifyDynamicWaterSpeed(x, y, fSpeed, m_fCustom, true);*/
#if DEBUG_REPLAY_PFX
	replayDebugf1("REPLAY: DYN WAVE FX: \n");
#endif
}

#endif // GTA_REPLAY
