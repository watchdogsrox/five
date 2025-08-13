//=====================================================================================================
// name:		ParticleVehicleFxPacket.cpp
//=====================================================================================================

#include "ParticleVehicleFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "control/replay/ReplayInterfaceVeh.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "Timecycle/TimeCycle.h"
#include "vehicles/heli.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/systems/VfxVehicle.h"
#include "vfx/systems/VfxWheel.h"
#include "control/replay/Compression/Quantization.h"

//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketVehBackFireFx_1 = 1;
CPacketVehBackFireFx::CPacketVehBackFireFx(u32 uPfxHash, float damageEvo, float bangerEvo, s32 exhaustID)
	: CPacketEvent(PACKETID_VEHBACKFIREFX, sizeof(CPacketVehBackFireFx), CPacketVehBackFireFx_1)
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_damageEvo = damageEvo;
	m_bangerEvo = bangerEvo;

	m_exhaustID = (u8)exhaustID;
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehBackFireFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	Mat34V boneMtx;
	s32 exhaustBoneId;
	u32 exhaustID = m_exhaustID;
	if(GetPacketVersion() >= CPacketVehBackFireFx_1)
		exhaustID = VEH_FIRST_EXHAUST + m_exhaustID;
	pVehicle->GetExhaustMatrix((eHierarchyId)exhaustID, boneMtx, exhaustBoneId);

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, exhaustBoneId);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage",0x431375e1), m_damageEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("banger",0x895ec1f8), m_bangerEvo);

		// start the fx
		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH BACKFIRE FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}


//========================================================================================================================
CPacketVehOverheatFx::CPacketVehOverheatFx(u32 uPfxHash, float fSpeed, float fDamage, float fCustom, float fFire, bool bXAxis)
	: CPacketEventInterp(PACKETID_VEHOVERHEATFX, sizeof(CPacketVehOverheatFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	REPLAY_UNUSEAD_VAR(m_Pad);

	m_fSpeed = fSpeed;
	m_fDamage = fDamage;
	m_fCustom = fCustom;
	m_fFire = fFire;
	m_bXAxis = bXAxis;

	/*s32 sVehicleOffset = -1;
	// Check if it's a car engine
	if (GetHash() == ATSTRINGHASH("veh_overheat_truck_engine", 0xB9B4D322) ||
		GetHash() == ATSTRINGHASH("veh_overheat_car_engine", 0xEEF20D6B))
	{
		TODO4FIVE sVehicleOffset = FXOFFSET_VEH_OVERHEAT_ENGINE;
	}
	// Check if it's a car vent
	else if (GetHash() == ATSTRINGHASH("veh_overheat_bus_vent", 0x9F05424F)||
		GetHash() == ATSTRINGHASH("veh_overheat_truck_vent", 0x84AAA45D) ||
		GetHash() == ATSTRINGHASH("veh_overheat_car_vent", 0xA12E265D))
	{
		TODO4FIVE sVehicleOffset = FXOFFSET_VEH_OVERHEAT_VENT + m_bXAxis;
	}
	// Check if it's a vehicle vent
	else if (GetHash() == ATSTRINGHASH("veh_overheat_bike_vent", 0x81AF5E78) ||
		GetHash() == ATSTRINGHASH("veh_overheat_boat_vent", 0x6B33F331) ||
		GetHash() == ATSTRINGHASH("veh_overheat_blackhawk", 0x2BC59D36) ||
		GetHash() == ATSTRINGHASH("veh_overheat_heli_vent", 0xC4E6E5CF))
	{
		TODO4FIVE sVehicleOffset = FXOFFSET_VEH_OVERHEAT_VENT + m_bXAxis;
	}
	else
		replayAssertf(false, "CPacketVehOverheatFx::CPacketVehOverheatFx");

	m_uPfxKey = (u8)sVehicleOffset;*/
}

//========================================================================================================================
void CPacketVehOverheatFx::Extract(const CInterpEventInfo<CPacketVehOverheatFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	Matrix34 pMatrix;
	s32 sVehicleOffset = -1;
	// Check if it's a car engine
	if (m_pfxHash == ATSTRINGHASH("veh_overheat_truck_engine", 0xB9B4D322) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_car_engine", 0xEEF20D6B))
	{
		s32 engineBoneIndex = pVehicle->GetBoneIndex(VEH_ENGINE);
		if (engineBoneIndex>-1)
		{
			pVehicle->GetGlobalMtx(engineBoneIndex, pMatrix);
		}
		/*TODO4FIVE sVehicleOffset = FXOFFSET_VEH_OVERHEAT_ENGINE;*/
	}
	// Check if it's a car vent
	else if (m_pfxHash == ATSTRINGHASH("veh_overheat_bus_vent", 0x9F05424F) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_truck_vent", 0x84AAA45D) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_car_vent", 0xA12E265D))
	{
		s32 overheatBoneIndex = -1;
		if (m_bXAxis == 0)
		{
			overheatBoneIndex = pVehicle->GetBoneIndex(VEH_OVERHEAT);
			if (overheatBoneIndex>-1)
			{
				pVehicle->GetGlobalMtx(overheatBoneIndex, pMatrix);
			}
		}
		else if (m_bXAxis == 1)
		{
			overheatBoneIndex = pVehicle->GetBoneIndex(VEH_OVERHEAT_2);
			if (overheatBoneIndex>-1)
			{
				pVehicle->GetGlobalMtx(overheatBoneIndex, pMatrix);
			}
		}
		else
			replayAssertf(false, "Invalid Axis 1 in CPacketVehOverheatFx::Extract");

		/*TODO4FIVE sVehicleOffset = FXOFFSET_VEH_OVERHEAT_VENT + m_bXAxis;*/
	}
	// Check if it's a vehicle vent
	else if (m_pfxHash == ATSTRINGHASH("veh_overheat_bike_vent", 0x81AF5E78) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_boat_vent", 0x6B33F331) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_blackhawk", 0x2BC59D36) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_heli_vent", 0xC4E6E5CF))
	{
		s32 overheatBoneIndex = -1;
		if (m_bXAxis == 0)
		{
			overheatBoneIndex = pVehicle->GetBoneIndex(VEH_OVERHEAT);
			if (overheatBoneIndex>-1)
			{
				pVehicle->GetGlobalMtx(overheatBoneIndex, pMatrix);
			}
		}
		else if (m_bXAxis == 1)
		{
			overheatBoneIndex = pVehicle->GetBoneIndex(VEH_OVERHEAT_2);
			if (overheatBoneIndex>-1)
			{
				pVehicle->GetGlobalMtx(overheatBoneIndex, pMatrix);
			}
		}
		else
			replayAssertf(false, "Invalid Axis 2 in CPacketVehOverheatFx::Extract");

		/*TODO4FIVE sVehicleOffset = FXOFFSET_VEH_OVERHEAT_VENT + m_bXAxis;*/
	}
	else
		replayAssertf(false, "Invalid Hash in CPacketVehOverheatFx::Extract");

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pVehicle, sVehicleOffset, true, m_pfxHash, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		if (eventInfo.GetInterp() == 0.0f)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_fSpeed);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage",0x431375e1), m_fDamage);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire",0xd30a036b), m_fFire);

			// Check if it's a car engine
			if (m_pfxHash == ATSTRINGHASH("veh_overheat_truck_engine", 0xB9B4D322) ||
				m_pfxHash == ATSTRINGHASH("veh_overheat_car_engine", 0xEEF20D6B))
			{
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("panel",0x10c1c125), m_fCustom);
			}
			// Check if it's a car vent
			else if (m_pfxHash == ATSTRINGHASH("veh_overheat_bus_vent", 0x9F05424F) ||
				m_pfxHash == ATSTRINGHASH("veh_overheat_truck_vent", 0x84AAA45D) ||
				m_pfxHash == ATSTRINGHASH("veh_overheat_car_vent", 0xA12E265D))
			{
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("reverse",0xae3930d7), m_fCustom);

				if (m_bXAxis == 1)
					pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}
			// Check if it's a vehicle vent
			else if (m_pfxHash == ATSTRINGHASH("veh_overheat_bike_vent", 0x81AF5E78) ||
				m_pfxHash == ATSTRINGHASH("veh_overheat_boat_vent", 0x6B33F331) ||
				m_pfxHash == ATSTRINGHASH("veh_overheat_blackhawk", 0x2BC59D36) ||
				m_pfxHash == ATSTRINGHASH("veh_overheat_heli_vent", 0xC4E6E5CF))
			{
				if (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
					pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rotor",0xbeaa5fd7), m_fCustom);

				if (m_bXAxis == 1)
					pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketVehOverheatFx const* pNext = eventInfo.GetNextPacket();
			ExtractInterpolateableValues(pFxInst, pVehicle, pNext, eventInfo.GetInterp());
		}

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(pVehicle->GetVelocity()));

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();

#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: VEH OVERHEAT FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
		}
		else
		{
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: VEH OVERHEAT UPD FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
		}
	}
}

//========================================================================================================================
void CPacketVehOverheatFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CVehicle* pVehicle, CPacketVehOverheatFx const* pNextPacket, float fInterp) const
{
	float fSpeed = Lerp(fInterp, m_fSpeed, pNextPacket->GetSpeed());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), fSpeed);

	float fDamage = Lerp(fInterp, m_fDamage, pNextPacket->GetDamage());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage",0x431375e1), fDamage);

	float fFire = Lerp(fInterp, m_fFire, pNextPacket->GetFire());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire",0xd30a036b), fFire);

	// Check if it's a car engine
	if (m_pfxHash == ATSTRINGHASH("veh_overheat_truck_engine", 0xB9B4D322) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_car_engine", 0xEEF20D6B))
	{
		float fCustom = Lerp(fInterp, m_fCustom, pNextPacket->GetCustom());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("panel",0x10c1c125), fCustom);
	}
	// Check if it's a car vent
	else if (m_pfxHash == ATSTRINGHASH("veh_overheat_bus_vent", 0x9F05424F) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_truck_vent", 0x84AAA45D) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_car_vent", 0xA12E265D))
	{
		float fCustom = Lerp(fInterp, m_fCustom, pNextPacket->GetCustom());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("reverse",0xae3930d7), fCustom);

		if (m_bXAxis == 1)
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
	}
	// Check if it's a vehicle vent
	else if (m_pfxHash == ATSTRINGHASH("veh_overheat_bike_vent", 0x81AF5E78) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_boat_vent", 0x6B33F331) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_blackhawk", 0x2BC59D36) ||
		m_pfxHash == ATSTRINGHASH("veh_overheat_heli_vent", 0xC4E6E5CF))
	{
		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
		{
			float fCustom = Lerp(fInterp, m_fCustom, pNextPacket->GetCustom());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rotor",0xbeaa5fd7), fCustom);
		}

		if (m_bXAxis == 1)
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
	}
}


//========================================================================================================================
void CPacketVehFirePetrolTankFx::StorePos(const Vector3& rPos)
{
	m_Position[0] = rPos.x;
	m_Position[1] = rPos.y;
	m_Position[2] = rPos.z;
}

//========================================================================================================================
void CPacketVehFirePetrolTankFx::LoadPos(Vector3& rPos) const
{
	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
CPacketVehFirePetrolTankFx::CPacketVehFirePetrolTankFx(u32 uPfxHash, u8 uID, const Vector3& rPos)
	: CPacketEventInterp(PACKETID_VEHPETROLTANKFX, sizeof(CPacketVehFirePetrolTankFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	StorePos(rPos);
	m_uID = uID;

	SetPfxKey(m_uID + (u8)PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK);
}

//========================================================================================================================
void CPacketVehFirePetrolTankFx::Extract(const CInterpEventInfo<CPacketVehFirePetrolTankFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	// register the fx system
	bool justCreated = false;
	u32 effectUniqueID = PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK+m_uID;
	ptxEffectInst* pFxInst = NULL;

	if(pVehicle->m_nVehicleFlags.bIsBig)
		pFxInst = ptfxManager::GetRegisteredInst(pVehicle, effectUniqueID, true, ATSTRINGHASH("veh_petroltank_truck", 0x1EBC32B1), justCreated);
	else if (pVehicle->GetVehicleType()==VEHICLE_TYPE_CAR)
		pFxInst = ptfxManager::GetRegisteredInst(pVehicle, effectUniqueID, true, ATSTRINGHASH("veh_petroltank_car", 0xE71262A5), justCreated);
	else if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE)
		pFxInst = ptfxManager::GetRegisteredInst(pVehicle, effectUniqueID, true, ATSTRINGHASH("veh_petroltank_bike", 0xE43167A2), justCreated);
	else if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT)
		pFxInst = ptfxManager::GetRegisteredInst(pVehicle, effectUniqueID, true, ATSTRINGHASH("veh_petroltank_boat", 0xAF1198EF), justCreated);
	else if (pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
		pFxInst = ptfxManager::GetRegisteredInst(pVehicle, effectUniqueID, true, ATSTRINGHASH("veh_petroltank_heli", 0xF55F3265), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(pVehicle->GetVelocity()));

		float speedEvo = pVehicle->GetVelocity().Mag()/10.0f;
		speedEvo = Min(speedEvo, 1.0f);
		replayAssert(speedEvo>=0.0f);
		replayAssert(speedEvo<=1.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), speedEvo);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// check if the effect has just been created 
		if (justCreated)
		{	
			Vector3 oVecPos;
			LoadPos(oVecPos);

			// get the petrol tank position relative to the vehicle matrix
			Vec3V petrolTankOffset;
			/*pVeh->GetMatrix().UnTransform(oVecPos, petrolTankOffset);*/
			petrolTankOffset = UnTransform3x3Full(pVehicle->GetMatrix(), VECTOR3_TO_VEC3V(oVecPos));
			// if this is the right side effect then flip the x offset and axis
			if (m_uID==1)
			{
				petrolTankOffset.SetXf(-petrolTankOffset.GetXf());
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}

			pFxInst->SetOffsetPos(petrolTankOffset);

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
/*TODO4FIVE #if __ASSERT && ENABLE_ENTITYPTR_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			replayAssert(pFxInst->GetFlag(PTFX_RESERVED_ENTITYPTR));
		}
#endif*/
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH PETROL FIRE FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif

	}
}


//========================================================================================================================
void CPacketVehHeadlightSmashFx::StorePos(const Vector3& rPos)
{
	m_Position[0] = rPos.x;
	m_Position[1] = rPos.y;
	m_Position[2] = rPos.z;
}

//========================================================================================================================
void CPacketVehHeadlightSmashFx::LoadPos(Vector3& rPos) const
{
	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
void CPacketVehHeadlightSmashFx::StoreNormal(const Vector3& rNormal)
{
	const float SCALAR = 127.0f;

	m_NormalX = (s8)(rNormal.x * SCALAR);
	m_NormalY = (s8)(rNormal.y * SCALAR);
	m_NormalZ = (s8)(rNormal.z * SCALAR);
}

//========================================================================================================================
void CPacketVehHeadlightSmashFx::LoadNormal(Vector3& rNormal) const
{
	const float SCALAR = 1.0f / 127.0f;

	rNormal.x = m_NormalX * SCALAR;
	rNormal.y = m_NormalY * SCALAR;
	rNormal.z = m_NormalZ * SCALAR;
}

//========================================================================================================================
CPacketVehHeadlightSmashFx::CPacketVehHeadlightSmashFx(u32 uPfxHash, u8 uBoneIdx, const Vector3& rPos, const Vector3& rNormal)
	: CPacketEvent(PACKETID_VEHHEADLIGHTSMASHFX, sizeof(CPacketVehHeadlightSmashFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	StorePos(rPos);
	StoreNormal(rNormal);
	m_uUseMe8 = uBoneIdx;
}

//========================================================================================================================
void CPacketVehHeadlightSmashFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	ptxEffectInst* pFxInst = NULL;
	switch (m_uUseMe8)
	{
		case VEH_HEADLIGHT_L:
		case VEH_HEADLIGHT_R:
		case VEH_REVERSINGLIGHT_L:
		case VEH_REVERSINGLIGHT_R:
		case VEH_NEON_L:
		case VEH_NEON_R:
		case VEH_NEON_F:
		case VEH_NEON_B:
		case VEH_EXTRALIGHT_1:
		case VEH_EXTRALIGHT_2:
		case VEH_EXTRALIGHT_3:
		case VEH_EXTRALIGHT_4:
			pFxInst = ptfxManager::GetTriggeredInst(ATSTRINGHASH("veh_light_clear", 0xC1F2D517));
			break;

		case VEH_TAILLIGHT_L:
		case VEH_TAILLIGHT_R:
		case VEH_BRAKELIGHT_L:
		case VEH_BRAKELIGHT_R:
		case VEH_BRAKELIGHT_M:
			pFxInst = ptfxManager::GetTriggeredInst(ATSTRINGHASH("veh_light_red", 0x35B434D6));
			break;

		case VEH_INDICATOR_LF:
		case VEH_INDICATOR_RF:
		case VEH_INDICATOR_LR:
		case VEH_INDICATOR_RR:
			pFxInst = ptfxManager::GetTriggeredInst(ATSTRINGHASH("veh_light_amber", 0x60AA9FD8));
			break;

		//case VEH_EXTRA_5: // This is now > sizeof(u8)...
		//case VEH_EXTRA_6: // FA: these are now >sizeof(u8) and clang doesn't like that
		//case VEH_EXTRA_9:	
			// Those are only valid for taxis, but they don't have an effect...
			// No Op 
		//	break;
		default:
			replayAssertf(false, "Invalid Light in CPacketVehHeadlightSmashFx::Extract");
	}

	if (pFxInst)
	{
		Vector3 oVecPos, oVecNormal;
		LoadPos(oVecPos);
		LoadNormal(oVecNormal);

		Mat34V fxOffsetMat;
		CVfxHelper::CreateMatFromVecZ(fxOffsetMat, VECTOR3_TO_VEC3V(oVecPos), VECTOR3_TO_VEC3V(oVecNormal));

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(fxOffsetMat);

		pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(pVehicle->GetVelocity()));

		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH HEADL SMASH FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}


//========================================================================================================================
CPacketVehResprayFx::CPacketVehResprayFx(u32 uPfxHash, Vector3& rPos, Vector3& rNormal)
	: CPacketVehHeadlightSmashFx(uPfxHash, 0, rPos, rNormal)
{
	CPacketBase::Store(PACKETID_VEHRESPRAYFX, sizeof(CPacketVehResprayFx));
}

//========================================================================================================================
void CPacketVehResprayFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;


	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(ATSTRINGHASH("veh_respray_smoke", 0x22DF303A));
	if (pFxInst)
	{
		Vector3 oVecPos, oVecNormal;
		LoadPos(oVecPos);
		LoadNormal(oVecNormal);

		// set up the fx inst
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecY(fxMat, VECTOR3_TO_VEC3V(oVecPos), VECTOR3_TO_VEC3V(oVecNormal));
		pFxInst->SetBaseMtx(fxMat);

		// set the colour tint
		CRGBA rgba = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1());
		pFxInst->SetColourTint(Vec3V(rgba.GetRedf(), rgba.GetGreenf(), rgba.GetBluef()));

		// start the fx
		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH RESPRAY FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}

//========================================================================================================================
CPacketVehTyrePunctureFx::CPacketVehTyrePunctureFx(u8 uBoneIdx, u8 wheelIndex, float speedEvo)
	: CPacketEvent(PACKETID_VEHTYREPUNCTUREFX, sizeof(CPacketVehTyrePunctureFx))
	, CPacketEntityInfo()
{
	m_boneIndex = uBoneIdx;
	m_wheelIndex = wheelIndex;
	m_speedEvo = speedEvo;
}

//========================================================================================================================
void CPacketVehTyrePunctureFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetWheelPuncturePtFxName());
	if (pFxInst)
	{
		// get the parent matrix (from the component hit)
		Mat34V boneMtx;
		CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, (eAnimBoneTag)m_boneIndex);

		CWheel* pWheel = pVehicle->GetWheel(m_wheelIndex);

		Vec3V vOffsetPos(V_ZERO);
		if (pVehicle->GetVehicleType()!=VEHICLE_TYPE_BIKE && pVehicle->GetVehicleType()!=VEHICLE_TYPE_BICYCLE)
		{
			vOffsetPos.SetXf(-pWheel->GetWheelWidth()*0.5f);
		}

		// set up the fx inst
		ptfxAttach::Attach(pFxInst, pVehicle, m_boneIndex);

		pFxInst->SetOffsetPos(vOffsetPos);

		pFxInst->SetUserZoom(pWheel->GetWheelRadius());

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_speedEvo);

		// start the fx
		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH TYRE PUNCT FX: %0X: ID: %0X\n", (u32)pVeh, pVeh->GetReplayID());
#endif
	}
}

//========================================================================================================================
CPacketVehTyreFireFx::CPacketVehTyreFireFx(u32 uPfxHash, u8 uWheelId, float fSpeed, float fFire)
	: CPacketVehFirePetrolTankFx(uPfxHash, 0, Vector3(0.0f, 0.0f, 0.0f))
{
	CPacketBase::Store(PACKETID_VEHTYREFIREFX, sizeof(CPacketVehTyreFireFx));

	m_uWheelId = uWheelId-VEH_WHEEL_LF;
	m_fSpeed = fSpeed;
	m_fFire = fFire;

	/*TODO4FIVE m_uPfxKey = (u8)FXOFFSET_WHEEL_FIRE;*/
}

//========================================================================================================================
void CPacketVehTyreFireFx::Extract(const CInterpEventInfo<CPacketVehTyreFireFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	/*TODO4FIVE CWheel* pWheel = pVeh->GetWheelFromId((eHierarchyId)(b.m_uWheelId+VEH_WHEEL_LF));

	// register the fx system
	bool justCreated = false;
	fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pWheel, FXOFFSET_WHEEL_FIRE);

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectUniqueID, ATSTRINGHASH("veh_tyre_fire", 0xB804C620), justCreated);
	//veh_tyre_puncture

	if (pFxInst)
	{
		pFxInst->SetVelocityAdd(pVeh->GetVelocity());

		if (fInterp == 0.0f)
		{
			replayAssert(m_fSpeed>=0.0f);
			replayAssert(m_fSpeed<=1.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), a.m_fSpeed);

			replayAssert(m_fFire>=0.0f);
			replayAssert(m_fFire<=1.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("timer",0x7e909ec5), a.m_fFire);	
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketParticleInterp* pNext;
			CReplayMgr::GetNextAddress(&pNext, (CPacketParticleInterp*)this);

			replayAssert(pNext);
			replayAssert(pNext->IsInterpPacket());

			ExtractInterpolateableValues(pFxInst, (CPacketVehTyreFireFx*)pNext, fInterp);
		}

		Matrix34* pWheelMtx = &pVeh->GetGlobalMtx(pVeh->GetBoneIndex((eHierarchyId)(b.m_uWheelId+VEH_WHEEL_LF)));

		ptfxAttach::Attach(pFxInst, pVeh, -1);

		if (justCreated)
		{
			float xOffset = -pWheel->GetWheelWidth()*0.5f;

			if (pVeh->GetVehicleType()==VEHICLE_TYPE_BIKE)
			{
				xOffset = 0.0f;
			}
			pFxInst->SetOffsetPos(Vec3V(xOffset, 0.0f, 0.0f));

			pFxInst->SetUserZoom(pWheel->GetWheelRadius());

			pFxInst->Start();
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: VEH TYRE FIRE FX: %0X: ID: %0X\n", (u32)pVeh, pVeh->GetReplayID());
#endif
		}
#if __ASSERT && ENABLE_ENTITYPTR_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			replayAssert(pFxInst->GetFlag(PTFX_RESERVED_ENTITYPTR));
		}
#endif
	}*/
}

//========================================================================================================================
void CPacketVehTyreFireFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketVehTyreFireFx const* pNextPacket, float fInterp) const
{
	float fSpeed = Lerp(fInterp, m_fSpeed, pNextPacket->GetSpeed());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), fSpeed);

	float fFire = Lerp(fInterp, m_fFire, pNextPacket->GetFire());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("timer",0x7e909ec5), fFire);
}


//========================================================================================================================
CPacketVehTyreBurstFx::CPacketVehTyreBurstFx(const Vector3& rPos, u8 uWheelIndex, bool bDueToWheelSpin, bool bDueToFire)
	: CPacketEvent(PACKETID_VEHTYREBURSTFX, sizeof(CPacketVehTyreBurstFx))
	, CPacketEntityInfo()
{
	m_vecPos.Store(rPos);

	m_dueToWheelSpin	= bDueToWheelSpin;
	m_dueToFire			= bDueToFire;
	m_wheelIndex		= uWheelIndex;
}

//========================================================================================================================
void CPacketVehTyreBurstFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetWheelBurstPtFxName());
	if (pFxInst)
	{
		Vector3 oVecPos;
		m_vecPos.Load(oVecPos);

		CWheel* pWheel = pVehicle->GetWheel(m_wheelIndex);
		REPLAY_CHECK(pWheel, NO_RETURN_VALUE, "Failed to find correct wheel - packet:%p, wheelIndex:%u", this, m_wheelIndex);

		Mat34V fxMat = pVehicle->GetMatrix();
		fxMat.SetCol3(VECTOR3_TO_VEC3V(oVecPos));

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), fxMat);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(relFxMat);

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), 0.0f, 20.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xF997622B), speedEvo);

		float spinEvo = 0.0f;
		if (m_dueToWheelSpin)
		{
			spinEvo = 1.0f;
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spin",0xCCE74BD9), spinEvo);

		float fireEvo = 0.0f;
		if (m_dueToFire)
		{
			fireEvo = 1.0f;
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire",0xD30A036B), fireEvo);

		// flip the x axis for left hand effects
		if (pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

		pFxInst->SetUserZoom(pWheel->GetWheelRadius());

		pFxInst->Start();

		if(pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->TriggerTyrePuncture(pWheel->GetHierarchyId(), false);
		}

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH TYRE BURST FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}


//========================================================================================================================
CPacketVehTrainSparksFx::CPacketVehTrainSparksFx(u32 uPfxHash, u8 uStartWheel, float fSqueal, u8 uWheelId, bool bIsLeft)
: CPacketEventInterp(PACKETID_TRAINSPARKSFX, sizeof(CPacketVehTrainSparksFx))
, m_pfxHash(uPfxHash)
{
	REPLAY_UNUSEAD_VAR(m_pfxHash);
	REPLAY_UNUSEAD_VAR(m_Pad);

	m_fSqueal = fSqueal;
	m_bIsLeft = bIsLeft;
	m_uStartWheel = uStartWheel;
	m_uWheelId = uWheelId;

	SetPfxKey(m_uWheelId);
	/*TODO4FIVE m_uPfxKey += (u8)FXOFFSET_TRAIN_SPARKS_1;*/
}

//========================================================================================================================
void CPacketVehTrainSparksFx::Extract(const CInterpEventInfo<CPacketVehTrainSparksFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_TRAIN_SPARKS+m_uWheelId, true, ATSTRINGHASH("veh_train_sparks", 0x73A6DDE6), justCreated);

	if (pFxInst)
	{
		if (eventInfo.GetInterp() == 0.0f)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("squeal",0xb5448572), m_fSqueal);		
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketVehTrainSparksFx const* pNext = eventInfo.GetNextPacket();
			ExtractInterpolateableValues(pFxInst, pNext, eventInfo.GetInterp());
		}

		Matrix34 pWheelMtx;

		CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
		replayAssert(pVehicleModelInfo);

		s32 boneId = pVehicleModelInfo->GetBoneIndex(m_uStartWheel+2*m_uWheelId);
		if (boneId>=0)
		{
			pVehicle->GetGlobalMtx(boneId, pWheelMtx);
		}

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		if (justCreated)
		{		
			if (m_bIsLeft)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}

			Vec3V wheelOffset(0.0f, 0.0f, -pVehicleModelInfo->GetRimRadius());

			pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(pVehicle->GetVelocity()));

			pFxInst->SetOffsetPos(wheelOffset);

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: VEH TRAIN SPARKS FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}

//========================================================================================================================
void CPacketVehTrainSparksFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketVehTrainSparksFx const* pNextPacket, float fInterp) const
{
	float fSqueal = Lerp(fInterp, m_fSqueal, pNextPacket->GetSqueal());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("squeal",0xb5448572), fSqueal);
}


//========================================================================================================================
void CPacketHeliDownWashFx::Store(const Matrix34& rMatrix)
{
	Quaternion q;
	rMatrix.ToQuaternion(q);

	m_HeliDownwashQuaternion = QuantizeQuaternionS3(q);

	m_Position[0] = rMatrix.d.x;
	m_Position[1] = rMatrix.d.y;
	m_Position[2] = rMatrix.d.z;
}

//========================================================================================================================
void CPacketHeliDownWashFx::Load(Matrix34& rMatrix) const
{
	Quaternion q;
	DeQuantizeQuaternionS3(q, m_HeliDownwashQuaternion);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_Position[2];
}

//========================================================================================================================
CPacketHeliDownWashFx::CPacketHeliDownWashFx(u32 uPfxHash, const Matrix34& rMatrix, float fDist, float fAngle, float fAlpha)
	: CPacketEventInterp(PACKETID_HELIDOWNWASHFX, sizeof(CPacketHeliDownWashFx))
	, m_pfxHash(uPfxHash)
{
	REPLAY_UNUSEAD_VAR(m_pfxHash);

	Store(rMatrix);

	m_fDist = fDist;
	m_fAngle = fAngle;
	m_fAlpha = fAlpha;
	
	/*TODO4FIVE m_uPfxKey = (u8)PTFXOFFSET_VEHICLE_HELI_DOWNWASH;*/
}

//========================================================================================================================
void CPacketHeliDownWashFx::Extract(const CInterpEventInfo<CPacketHeliDownWashFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	/*TODO4FIVE bool justCreated = false;
	fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pHeli, PTFXOFFSET_VEHICLE_HELI_DOWNWASH);
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pHeli, effectUniqueID, true, m_PfxHash, justCreated);
	if (pFxInst)
	{
		if (fInterp == 0.0f)
		{
			// calc the world matrix
			Matrix34 fxMat;
			Load(fxMat);

			pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(fxMat));

			replayAssert(m_fDist>=0.0f);
			replayAssert(m_fDist<=1.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("distance",0xd6ff9582), m_fDist);

			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle",0xf3805b5), m_fAngle);

			pFxInst->SetAlphaTint(m_fAlpha);
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketParticleInterp* pNext;
			CReplayMgr::GetNextAddress(&pNext, (CPacketParticleInterp*)this);

			replayAssert(pNext);
			replayAssert(pNext->IsInterpPacket());

			ExtractInterpolateableValues(pFxInst, (CPacketHeliDownWashFx*)pNext, fInterp);
		}

		float speedEvo = pHeli->GetVelocity().Mag()/20.0f;
		speedEvo = Min(speedEvo, 1.0f);
		replayAssert(speedEvo>=0.0f);
		replayAssert(speedEvo<=1.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), speedEvo);

		// set the rotor speed evolution
		float rotorEvo = pHeli->m_fEngineSpeed;
		rotorEvo = Min(rotorEvo, 1.0f);
		replayAssert(rotorEvo>=0.0f);
		replayAssert(rotorEvo<=1.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rotor",0xbeaa5fd7), rotorEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created
			pFxInst->Start();
		}
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: HELI DOWN WASH FX: %0X: ID: %0X\n", (u32)pHeli, pHeli->GetReplayID());
#endif
	}*/
}

//========================================================================================================================
void CPacketHeliDownWashFx::FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos) const
{
	DeQuantizeQuaternionS3(rQuat, m_HeliDownwashQuaternion);

	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
void CPacketHeliDownWashFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketHeliDownWashFx const* pNextPacket, float fInterp) const
{
	Quaternion rQuatNext;
	Vector3	rPosNext;
	pNextPacket->FetchQuaternionAndPos(rQuatNext, rPosNext);

	Quaternion rQuatPrev;
	DeQuantizeQuaternionS3(rQuatPrev, m_HeliDownwashQuaternion);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNext);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNext);

	Matrix34 rFxMat;
	rFxMat.FromQuaternion(rQuatInterp);
	replayAssert(rFxMat.IsOrthonormal());

	rFxMat.d.x = m_Position[0];
	rFxMat.d.y = m_Position[1];
	rFxMat.d.z = m_Position[2];

	rFxMat.d = rFxMat.d * (1.0f - fInterp) + rPosNext * fInterp;

	pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(rFxMat));

	float fDist = Lerp(fInterp, m_fDist, pNextPacket->GetDist());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("distance",0xd6ff9582), fDist);

	float fAngle = Lerp(fInterp, m_fAngle, pNextPacket->GetAngle());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle",0xf3805b5), fAngle);

	float fAlpha = Lerp(fInterp, m_fAlpha, pNextPacket->GetAlpha());
	pFxInst->SetAlphaTint(fAlpha);
}

//========================================================================================================================
CPacketHeliPartsDestroyFx::CPacketHeliPartsDestroyFx(bool isTailRotor)
	: CPacketEvent(PACKETID_HELIPARTSDESTROYFX, sizeof(CPacketHeliPartsDestroyFx))
	, CPacketEntityInfo()
	, m_IsTailRotor(isTailRotor)
{}

//========================================================================================================================
void CPacketHeliPartsDestroyFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();

	if (pVehicle == NULL || !pVehicle->GetIsRotaryAircraft())
		return;

	CRotaryWingAircraft* pAircraft = static_cast<CRotaryWingAircraft*>(pVehicle);

	g_vfxVehicle.TriggerPtFxHeliRotorDestroy(pAircraft, m_IsTailRotor);
}


//////////////////////////////////////////////////////////////////////////
CPacketVehPartFx::CPacketVehPartFx(u32 uPfxHash, const Vector3& pos, float size)
	: CPacketEvent(PACKETID_VEHPARTFX, sizeof(CPacketVehPartFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
	, m_position(pos)
	, m_size(size)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketVehPartFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	if (pEntity == NULL)
		return;

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Vector3 oVecPos;
		m_position.Load(oVecPos);

		ptfxAttach::Attach(pFxInst, pEntity, -1);
		pFxInst->SetOffsetPos(VECTOR3_TO_VEC3V(oVecPos));

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size",0xc052dea7), m_size);

		pFxInst->Start();
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion g_PacketVehVariationChange_v1 = 1;
tPacketVersion g_PacketVehVariationChange_v2 = 2;
CPacketVehVariationChange::CPacketVehVariationChange(CVehicle* pVehicle)
	: CPacketEvent(PACKETID_VEHVARIATIONCHANGE, sizeof(CPacketVehVariationChange), g_PacketVehVariationChange_v2)
	, CPacketEntityInfo()
{
	const CVehicleVariationInstance& VehicleVariationInstance = pVehicle->GetVariationInstance();
	const CVehicleKit* pKit = VehicleVariationInstance.GetKit();
	u16 kitId = INVALID_VEHICLE_KIT_ID;
	if( pKit )
		kitId = pKit->GetId();

	m_NewVariation.StoreVariations(pVehicle);

	//Store the kit ID in the kit index to use to lookup the correct index.
	//m_NewVariation.SetKitIndex(kitId);
	m_kitId = kitId;

	// Added extra mods for version 1
	m_numExtraMods = 0;
	for(int i = 0; i < sizeof(extraVehicleModArrayReplay); ++i)
	{
		m_extraMods[i] = VehicleVariationInstance.GetModIndex(static_cast<eVehicleModType>(extraVehicleModArrayReplay[i]));
		++m_numExtraMods;
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketVehVariationChange::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(pVehicle)
	{
		CStoredVehicleVariations variation;
		m_NewVariation.ToGameStruct(variation);

		u16 kitId = variation.GetKitIndex();
		if(GetPacketVersion() >= g_PacketVehVariationChange_v2)
		{
			kitId = m_kitId;
		}

		//Lookup the correct kit index using the ID.
		if(kitId != INVALID_VEHICLE_KIT_ID)
			variation.SetKitIndex(CVehicleModelInfo::GetVehicleColours()->GetKitIndexById(kitId));

		// If we have version 1 of this packet then extract the Extra mods
		if(GetPacketVersion() >= g_PacketVehVariationChange_v1)
		{
			for(int i = 0; i < m_numExtraMods; ++i)
			{
				variation.SetModIndex((eVehicleModType)extraVehicleModArrayReplay[i], m_extraMods[i]);
			}
		}

		//Clear linked mods otherwise we end up filling the array
		pVehicle->GetVehicleDrawHandler().GetVariationInstance().ClearLinkedModsForReplay();

		pVehicle->SetVariationInstance(variation);
		pVehicle->UpdateBodyColourRemapping(); //make sure the colors propagated
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CPacketPlaneWingtipFx::CPacketPlaneWingtipFx(u32 id, u32 effectRuleNameHash, float speedEvo)
	: CPacketEvent(PACKETID_PLANEWINGTIPFX, sizeof(CPacketPlaneWingtipFx))
	, CPacketEntityInfo()
{
	m_tipID = (u8)id;
	m_pfxEffectRuleHashName = effectRuleNameHash;
	m_speedEvo = speedEvo;
}


void CPacketPlaneWingtipFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVeh = eventInfo.GetEntity();
	if(pVeh)
	{
		Assertf(pVeh->GetVehicleType() == VEHICLE_TYPE_PLANE, "CPacketPlaneWingtipFx::Extract() - Trying to set wingtip FX on something that's not a plane?");

		CPlane *pPlane = static_cast<CPlane*>(pVeh);
		CVehicleModelInfo* pModelInfo = pPlane->GetVehicleModelInfo();
		if( Verifyf(pModelInfo, "CPacketPlaneWingtipFx::Extract() - Unable to find modelinfo"))
		{
			s32 wingTipBoneId = pModelInfo->GetBoneIndex(PLANE_WINGTIP_1+m_tipID);
			if (wingTipBoneId==-1)
			{
				return;
			}

			Mat34V boneMtx;
			CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pPlane, wingTipBoneId);
			if (IsZeroAll(boneMtx.GetCol0()))
			{
				return;
			}

			// check which side of the plane this is on
			Mat34V relFxMat;
			CVfxHelper::CreateRelativeMat(relFxMat, pPlane->GetMatrix(), boneMtx);
			bool isLeft = relFxMat.GetCol3().GetXf()<0.0f;

			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPlane, PTFXOFFSET_VEHICLE_PLANE_WINGTIP+m_tipID, true, m_pfxEffectRuleHashName, justCreated);
			if (pFxInst)
			{
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), m_speedEvo);
				ptfxAttach::Attach(pFxInst, pPlane, wingTipBoneId);

				// check if the effect has just been created 
				if (justCreated)
				{
					// flip the x axis for left hand effects
					if (isLeft)
					{
						pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
					}
					pFxInst->Start();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CPacketTrailDecal::CPacketTrailDecal(decalUserSettings decalSettings, u32 matId, int decalId)
	: CPacketDecalEvent(decalId, PACKETID_TRAILDECAL, sizeof(CPacketTrailDecal))
	, CPacketEntityInfo()
{	

	m_WorldPos.Store(decalSettings.instSettings.vPosition);
	m_Normal.Store(decalSettings.instSettings.vDirection);
	m_Side.Store(decalSettings.instSettings.vSide);
	m_Dimentions.Store(decalSettings.instSettings.vDimensions);

	m_DecalType = decalSettings.bucketSettings.typeSettingsIndex;
	m_RenderSettingIndex = decalSettings.bucketSettings.renderSettingsIndex;
	m_IsScripted = decalSettings.bucketSettings.isScripted;

	m_Length = decalSettings.instSettings.currWrapLength;
	m_DecalLife = decalSettings.instSettings.totalLife;
	m_colR = decalSettings.instSettings.colR;
	m_colG = decalSettings.instSettings.colG;
	m_colB = decalSettings.instSettings.colB;
	m_colA = decalSettings.instSettings.alphaFront;
	m_AlphaBack = decalSettings.instSettings.alphaBack;
	m_LiquidType = decalSettings.instSettings.liquidType;

	m_PassedJoinVerts1.Store(decalSettings.pipelineSettings.vPassedJoinVerts[0]);
	m_PassedJoinVerts2.Store(decalSettings.pipelineSettings.vPassedJoinVerts[1]);
	m_PassedJoinVerts3.Store(decalSettings.pipelineSettings.vPassedJoinVerts[2]);
	m_PassedJoinVerts4.Store(decalSettings.pipelineSettings.vPassedJoinVerts[3]);

	m_MatID						= matId;
}

//////////////////////////////////////////////////////////////////////////
void CPacketTrailDecal::Extract(const CEventInfo<void>& eventInfo) const
{
	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}
	
	Vec3V worldPos;
	m_WorldPos.Load(worldPos);

	Vec3V normal;
	m_Normal.Load(normal);
	
	Vec3V side;
	m_Side.Load(side);

	Vec3V dimentions;
	m_Dimentions.Load(dimentions);

	decalUserSettings decalSettings;

	decalSettings.bucketSettings.typeSettingsIndex = (u16)m_DecalType;
	decalSettings.bucketSettings.renderSettingsIndex = (u16)m_RenderSettingIndex;
	decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;

	//Replay doesn't need to worry about marking FX as scripted.
	decalSettings.bucketSettings.isScripted = false;

	decalSettings.instSettings.vPosition = worldPos;
	decalSettings.instSettings.vDirection = normal;
	decalSettings.instSettings.vSide = side;
	decalSettings.instSettings.vDimensions = dimentions;
	decalSettings.instSettings.currWrapLength = m_Length;
	decalSettings.instSettings.totalLife = m_DecalLife;
	decalSettings.instSettings.colR = m_colR;
	decalSettings.instSettings.colG = m_colG;
	decalSettings.instSettings.colB = m_colB;
	decalSettings.instSettings.alphaFront = m_colA;
	decalSettings.instSettings.alphaBack = m_AlphaBack;
	decalSettings.instSettings.liquidType = (s8)m_LiquidType;

	if( eventInfo.GetIsFirstFrame() || eventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
	{
		decalSettings.instSettings.fadeInTime = 0.0f;
		decalSettings.lodSettings.currLife = DECAL_INST_INV_FADE_TIME_UNITS_HZ;
	}

	decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
	decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
	decalSettings.pipelineSettings.hasJoinVerts = true;

	Vec3V passedJoinVerts1;
	m_PassedJoinVerts1.Load(passedJoinVerts1);
	decalSettings.pipelineSettings.vPassedJoinVerts[0] = passedJoinVerts1;

	Vec3V passedJoinVerts2;
	m_PassedJoinVerts2.Load(passedJoinVerts2);
	decalSettings.pipelineSettings.vPassedJoinVerts[1] = passedJoinVerts2;

	Vec3V passedJoinVerts3;
	m_PassedJoinVerts3.Load(passedJoinVerts3);
	decalSettings.pipelineSettings.vPassedJoinVerts[2] = passedJoinVerts3;

	Vec3V passedJoinVerts4;
	m_PassedJoinVerts4.Load(passedJoinVerts4);
	decalSettings.pipelineSettings.vPassedJoinVerts[3] = passedJoinVerts4;
	
	// override the depth for 'deep' materials
	if (PGTAMATERIALMGR->GetIsDeepNonWading(m_MatID))
	{
		decalSettings.instSettings.vDimensions.SetZ(ScalarV(V_HALF));
	}

	m_decalId = g_decalMan.AddSettingsReplay(NULL, decalSettings);
}


//////////////////////////////////////////////////////////////////////////


CPacketVehicleSlipstreamFx::CPacketVehicleSlipstreamFx(u32 id, u32 pfxEffectRuleHashName, float slipstreamEvo)
	: CPacketEvent(PACKETID_VEHICLESLIPSTREAMFX, sizeof(CPacketVehicleSlipstreamFx))
	, CPacketEntityInfo()
{
	m_ID = (u8)id;
	m_pfxEffectRuleHashName = pfxEffectRuleHashName;
	m_slipstreamEvo = slipstreamEvo;
}


void CPacketVehicleSlipstreamFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(pVehicle)
	{
		s32	boneIdx = pVehicle->GetBoneIndex((eHierarchyId)(VEH_SLIPSTREAM_L+m_ID));
		if (boneIdx==-1)
		{
			boneIdx = pVehicle->GetBoneIndex((eHierarchyId)(VEH_TAILLIGHT_L+m_ID));
		}

		//url:bugstar:3790609 - we forgot to add slipstream or taillight bones to oppressor.
		if (boneIdx==-1 && MI_BIKE_OPPRESSOR.IsValid() && pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR)
		{
			boneIdx = pVehicle->GetBoneIndex((eHierarchyId)(VEH_ROCKET_BOOST));
		}

		if (boneIdx>-1)
		{
			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_SLIPSTREAM+m_ID, true, m_pfxEffectRuleHashName, justCreated);
			// check the effect exists
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pVehicle, boneIdx);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("slipstream", 0x8AC10C7A), m_slipstreamEvo);
				// check if the effect has just been created 
				if (justCreated)
				{		
					// it has just been created - start it and set it's position
					pFxInst->Start();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketVehicleBadgeRequest_V1 = 1;
CPacketVehicleBadgeRequest::CPacketVehicleBadgeRequest(const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool /*isForLocalPlayer*/, u32 badgeIndex, u8 alpha)
	: CPacketEvent(PACKETID_VEHICLE_BADGE_REQUEST, sizeof(CPacketVehicleBadgeRequest), CPacketVehicleBadgeRequest_V1)
	, CPacketEntityInfo()
{
	m_offsetPos		= vOffsetPos;
	m_direction		= vDir;
	m_side			= vSide;

	m_badgeIndex	= badgeIndex;
	m_boneIndex		= boneIndex;

	m_emblemId		= badgeDesc.GetEmblemDesc().m_id;
	m_emblemType	= badgeDesc.GetEmblemDesc().m_type;
	m_emblemSize	= (u8)badgeDesc.GetEmblemDesc().m_size;
	m_txdHashName	= badgeDesc.GetTxdHashName().GetHash();
	m_textureHashName = badgeDesc.GetTexHashName().GetHash();

	m_size			= size;
	m_alpha			= alpha;
}

//////////////////////////////////////////////////////////////////////////
void CPacketVehicleBadgeRequest::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(pVehicle)
	{
		if( !g_decalMan.DoesVehicleHaveBadge(pVehicle, m_badgeIndex) )
		{
			EmblemDescriptor emblemDesc((EmblemType)m_emblemType, (EmblemId)m_emblemId, (rlClanEmblemSize)m_emblemSize);

			CVehicleBadgeDesc badgeDesc;
			if(GetPacketVersion() >= CPacketVehicleBadgeRequest_V1)
			{
				badgeDesc.Set(emblemDesc, atHashWithStringNotFinal(m_txdHashName), atHashWithStringNotFinal(m_textureHashName));
			}
			else
			{
				badgeDesc.Set(emblemDesc);
			}

			g_decalMan.RequestReplayVehicleBadge(pVehicle, badgeDesc, m_boneIndex, m_offsetPos, m_direction, m_side, m_size, true, m_badgeIndex, m_alpha);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPacketVehicleBadgeRequest::HasExpired(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(pVehicle)
	{
		if( g_decalMan.GetVehicleBadgeRequestState(pVehicle, m_badgeIndex) == DECALREQUESTSTATE_NOT_ACTIVE )
		{
			return !g_decalMan.DoesVehicleHaveBadge(pVehicle, m_badgeIndex);
		}
		else
		{
			return false;
		}
	}

	return true;
}


tPacketVersion CPacketVehicleWeaponCharge_V1 = 1;
CPacketVehicleWeaponCharge::CPacketVehicleWeaponCharge(eHierarchyId weaponBone, float chargePhase)
	: CPacketEvent(PACKETID_VEHICLE_WEAPON_CHARGE, sizeof(CPacketVehicleWeaponCharge), CPacketVehicleWeaponCharge_V1)
	, CPacketEntityInfo()
{
	m_boneId = (u32)weaponBone;
	m_chargePhase = chargePhase;
}

void CPacketVehicleWeaponCharge::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(pVehicle)
	{
		u32 boneID = m_boneId;
		// In CL#21985785 16 eHierarchyIds were inserted to allow for extra Exhausts
		// This will have knocked on all the following ids.  Added a version number to this 
		// packet so that old clips should offset the stored bone ID by the number of
		// ids that were inserted.  Fix for url:bugstar:5501062
		if(GetPacketVersion() < CPacketVehicleWeaponCharge_V1 && boneID > VEH_EXHAUST_16)
		{
			boneID += 16;
		}

		s32 boneIndex = pVehicle->GetBoneIndex((eHierarchyId)boneID);
		if (boneIndex>-1)
		{
			g_vfxVehicle.UpdatePtFxVehicleWeaponCharge(pVehicle, boneIndex, m_chargePhase);
			pVehicle->GetVehicleAudioEntity()->RequestWeaponChargeSfx(boneIndex, m_chargePhase);
		}
	}

}

#endif // GTA_REPLAY
