//=====================================================================================================
// name:		ParticleMarkFxPacket.cpp
//=====================================================================================================

#include "ParticleMarkFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "vehicles/vehicle.h"
#include "modelinfo/VehicleModelInfo.h"
#include "scene/Physical.h"
#include "control/replay/ReplayInternal.h"
#include "vfx/systems/VfxWheel.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxattach.h"
#include "control/replay/Compression/Quantization.h"

//========================================================================================================================
CPacketMarkScrapeFx::CPacketMarkScrapeFx(u32 uPfxHash, Matrix34& rMatrix, float fTime1, float fTime2, bool bIsDynamic, bool bIsPhysical, u8 uColType, u32 uMagicKey)
	: CPacketEventInterp(PACKETID_MARKSCRAPEFX, sizeof(CPacketMarkScrapeFx))
	, m_pfxHash(uPfxHash)
{
	StoreMatrix(rMatrix);
	m_fEvoTime1 = fTime1;
	m_fEvoTime2 = fTime2;
	m_bIsDynamic = bIsDynamic;
	m_bIsPhysical = bIsPhysical;
	m_uColType = uColType;

	StoreMagicKey(uMagicKey);
}

//========================================================================================================================
void CPacketMarkScrapeFx::StoreMagicKey(u32 uMagicKey)
{
	SetPfxKey((u8)(uMagicKey>>24));
	m_uMagicKey[2] = (u8)((uMagicKey>>16)&0x000000ff);
	m_uMagicKey[1] = (u8)((uMagicKey>>8)&0x000000ff);
	m_uMagicKey[0] = (u8)(uMagicKey&0x000000ff);
}

//========================================================================================================================
u32 CPacketMarkScrapeFx::LoadMagicKey()
{
#if 1
	u32 uLowerBits = ((m_uMagicKey[2])<<16&0x00ff0000) + ((m_uMagicKey[1])<<8&0x0000ff00) + m_uMagicKey[0];
	u32 uHighBits = GetPfxKey();
	u32 uFinalKey = uLowerBits + (uHighBits<<24&0xff000000);
	return uFinalKey;
#else
	return ((((u32)d.m_uPfxKey)>>24) + (((u32)(m_uMagicKey[2]))<<16) + (((u32)(m_uMagicKey[1]))<<8) + ((u32)(m_uMagicKey[0])));
#endif // __DEV
}

//========================================================================================================================
void CPacketMarkScrapeFx::StoreMatrix(const Matrix34& rMatrix)
{
	Quaternion q;
	rMatrix.ToQuaternion(q);

	m_MarkQuaternion = QuantizeQuaternionS3(q);

	m_Position[0] = rMatrix.d.x;
	m_Position[1] = rMatrix.d.y;
	m_Position[2] = rMatrix.d.z;
}

//========================================================================================================================
void CPacketMarkScrapeFx::LoadMatrix(Matrix34& rMatrix)
{
	Quaternion q;
	DeQuantizeQuaternionS3(q, m_MarkQuaternion);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_Position[2];
}

//========================================================================================================================
void CPacketMarkScrapeFx::Extract(CEntity* pEntity, float fInterp)
{
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, LoadMagicKey(), true, m_pfxHash, justCreated);
	if (pFxInst)
	{
		Matrix34 oFxMat;
		LoadMatrix(oFxMat);

		if (fInterp == 0.0f)
		{
			if (m_bIsDynamic==false)
			{
				pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(oFxMat));
			}
			else
			{
				ptfxAttach::Attach(pFxInst, pEntity, -1);
			}

			replayAssert(m_fEvoTime1>=0.0f);
			replayAssert(m_fEvoTime1<=1.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("velocity",0x8ff7e5a7), m_fEvoTime1);

			replayAssert(m_fEvoTime2>=0.0f);
			replayAssert(m_fEvoTime2<=1.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("impetus",0xc415e804), m_fEvoTime2);

			// set up any required collisions
			if (m_uColType==1 || m_uColType==2)
			{
				/*TODO4FIVE g_ptFxManager.GetColnInterface().Register(pFxInst, pEntity);*/
			}
			////replayAssert(m_fEvoTime1>=0.0f);
			////replayAssert(m_fEvoTime1<=1.0f);
			////pFxInst->SetEvolutionTime("velocity", m_fEvoTime1);

			////replayAssert(m_fEvoTime2>=0.0f);
			////replayAssert(m_fEvoTime2<=1.0f);
			////pFxInst->SetEvolutionTime("impetus", m_fEvoTime2);

			////// set up any required collisions
			////if (m_uColType==1 || m_uColType==2)
			////{
			////	pFxInst->ClearCollisionSet();
			////	CFxHelper::AddGroundPlaneToFxInst(pFxInst, oFxMat.d, 4.0f, 6.0f);
			////}

			if (oFxMat.c.z<0.0f)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Z);
			}
		}
		else if (!justCreated && HasNextOffset())
		{
			// TODO
			replayFatalAssertf(false, "TODO");
// 			CPacketMarkScrapeFx* pNext = NULL;
// 			GetNextPacket(this, pNext);
// 
// 			replayAssert(pNext);
// 			replayAssert(pNext->IsInterpPacket());
// 
// 			ExtractInterpolateableValues(pFxInst, pNext, fInterp);

			if (m_bIsDynamic==true)
			{
				ptfxAttach::Attach(pFxInst, pEntity, -1);
			}
		}

		if (m_bIsPhysical && m_bIsDynamic)
		{
			CPhysical* pPhysical = (CPhysical*)pEntity;
			pFxInst->SetVelocityAdd(VECTOR3_TO_VEC3V(pPhysical->GetVelocity()));
		}
		////if (m_bIsPhysical)
		////	{
		////	CPhysical* pPhysical = (CPhysical*)pEntity;
		////	pFxInst->SetVelocity(pPhysical->GetMoveSpeed());
		////}

		// check if the effect has just been created 
		if (justCreated)
		{
			if (m_bIsDynamic)
			{
				// make the fx matrix relative to the entity
				Mat34V invEntityMat = pEntity->GetMatrix();
				Invert3x3Full(invEntityMat, invEntityMat);

				Mat34V relFxMat = MATRIX34_TO_MAT34V(oFxMat);
				Transform(relFxMat, invEntityMat);

				pFxInst->SetOffsetMtx(relFxMat);
			}	

			// check for scrapes coming from the heli rotor system
			if (m_fEvoTime1==100000.0f && m_fEvoTime2==100000.0f)
			{
				/*TODO4FIVE pFxInst->SetUserZoom(MTLFX_HELI_SCRAPE_SCALE_MULT);
				pFxInst->OverrideSpawnDist(MTLFX_HELI_SPAWNDIST_OVERRIDE);*/
			}

			if (m_bIsDynamic && pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = (CVehicle*)pEntity;
				if (pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
				{
					pFxInst->SetLODScalar(3.0f);
				}
			}

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
/*TODO4FIVE #if __ASSERT && ENABLE_ENTITYPTR_CHECK
		else
		{
			if (m_bIsDynamic)
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				replayAssert(pFxInst->GetFlag(PTFX_RESERVED_ENTITYPTR));
			}
		}
#endif*/

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: MARK FX: %0X: ID: %0X\n", (u32)pEntity, pEntity->GetReplayID());
#endif
	}
}

//========================================================================================================================
void CPacketMarkScrapeFx::FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos)
{
	DeQuantizeQuaternionS3(rQuat, m_MarkQuaternion);

	rPos.x = m_Position[0];
	rPos.y = m_Position[1];
	rPos.z = m_Position[2];
}

//========================================================================================================================
void CPacketMarkScrapeFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketMarkScrapeFx *pNextPacket, float fInterp)
{
	Quaternion rQuatNext;
	Vector3	rPosNext;
	pNextPacket->FetchQuaternionAndPos(rQuatNext, rPosNext);

	Quaternion rQuatPrev;
	DeQuantizeQuaternionS3(rQuatPrev, m_MarkQuaternion);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNext);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNext);

	Matrix34 rMatrix;
	rMatrix.FromQuaternion(rQuatInterp);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_Position[2];

	rMatrix.d = rMatrix.d * (1.0f - fInterp) + rPosNext * fInterp;

	if (m_bIsDynamic==false)
	{
		pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(rMatrix));
	}

	float fEvoTime1 = Lerp(fInterp, m_fEvoTime1, pNextPacket->GetEvoTime1());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("velocity",0x8ff7e5a7), fEvoTime1);

	float fEvoTime2 = Lerp(fInterp, m_fEvoTime2, pNextPacket->GetEvoTime2());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("impetus",0xc415e804), fEvoTime2);

	// set up any required collisions
	if (m_uColType==1 || m_uColType==2)
	{
		/*TODO4FIVE g_ptFxManager.GetColnInterface().Register(pFxInst, NULL);*/
	}

	////float fEvoTime1 = Lerp(fInterp, m_fEvoTime1, pNextPacket->GetEvoTime1());
	////pFxInst->SetEvolutionTime("velocity", fEvoTime1);

	////float fEvoTime2 = Lerp(fInterp, m_fEvoTime2, pNextPacket->GetEvoTime2());
	////pFxInst->SetEvolutionTime("impetus", fEvoTime2);

	////// set up any required collisions
	////if (m_uColType==1 || m_uColType==2)
	////{
	////	pFxInst->ClearCollisionSet();
	////	CFxHelper::AddGroundPlaneToFxInst(pFxInst, rMatrix.d, 4.0f, 6.0f);
	////}

	if (rMatrix.c.z<0.0f)
	{
		pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Z);
	}
}



#endif // GTA_REPLAY
