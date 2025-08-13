//=====================================================================================================
// name:		ParticleMaterialFxPacket.cpp
//=====================================================================================================

#include "ParticleMaterialFxPacket.h"

#if GTA_REPLAY

#include "control/replay/replay_channel.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "vehicles/vehicle.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxwrapper.h"


//////////////////////////////////////////////////////////////////////////
CPacketMaterialBangFx::CPacketMaterialBangFx(u32 uPfxHash, const rage::Matrix34& rMatrix, float velocityEvo, float impulseEvo, float lodRangeScale)
	: CPacketEvent(PACKETID_MATERIALBANGFX, sizeof(CPacketMaterialBangFx))
	, CPacketEntityInfoStaticAware()
	, m_posAndRot(rMatrix)
	, m_pfxHash(uPfxHash)
	, m_velocityEvo(velocityEvo)
	, m_impulseEvo(impulseEvo)
	, m_bangScrapePtFxLodRangeScale(lodRangeScale)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketMaterialBangFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		rage::Matrix34 matrix;
		m_posAndRot.LoadMatrix(matrix);
		Mat34V mat = MATRIX34_TO_MAT34V(matrix);

		// set the position of the effect
		if (pEntity->GetIsDynamic())
		{
			// make the fx matrix relative to the entity
			Mat34V relFxMat;
			CVfxHelper::CreateRelativeMat(relFxMat, pEntity->GetMatrix(), mat);

			ptfxAttach::Attach(pFxInst, pEntity, -1);

			pFxInst->SetOffsetMtx(relFxMat);
		}
		else
		{
			pFxInst->SetBaseMtx(mat);
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("velocity",0x8ff7e5a7), m_velocityEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("impulse",0xb05d6c92), m_impulseEvo);

		// if this is a vehicle then override the colour of the effect with that of the vehicle
		if (pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			CRGBA rgba = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1());
			ptfxWrapper::SetColourTint(pFxInst, rgba.GetRGBA().GetXYZ());

			CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
			if (pVfxVehicleInfo)
			{
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("vehicle",0xdd245b9c), pVfxVehicleInfo->GetMtlBangPtFxVehicleEvo());
			}
		}
		else
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("vehicle",0xdd245b9c), 0.0f);
		}

		// if the z component is pointing down then flip it
		if (mat.GetCol2().GetZf()<0.0f)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Z);
		}

		if (pEntity->GetIsPhysical())
		{
			CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
			ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
		}

		pFxInst->SetLODScalar(m_bangScrapePtFxLodRangeScale);

		pFxInst->Start();
	}
}

#endif // GTA_REPLAY
