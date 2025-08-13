//=====================================================================================================
// name:		ParticleBloodFxPacket.cpp
//=====================================================================================================

#include "ParticleEntityFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "vfx/systems/VfxEntity.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/VfxHelper.h"
#include "scene/physical.h"
#include "control/replay/Compression/Quantization.h"

CPacketEntityAmbientFx::CPacketEntityAmbientFx(const CParticleAttr* pPtFxAttr, s32 effectId, bool isTrigger)
	: CPacketEvent(PACKETID_ENTITYAMBIENTFX, sizeof(CPacketEntityAmbientFx))
	, CPacketEntityInfo()
{
	Vector3 pos;
	pPtFxAttr->GetPos(pos);

	m_PosAndRot.StorePosQuat(pos, Quaternion(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));
	m_tagHash = pPtFxAttr->m_tagHashName.GetHash();

	m_fxType		= pPtFxAttr->m_fxType;
	m_boneTag		= pPtFxAttr->m_boneTag;
	m_scale			= pPtFxAttr->m_scale;
	m_probability	= pPtFxAttr->m_probability;
	m_tintR			= pPtFxAttr->m_tintR;
	m_tintG			= pPtFxAttr->m_tintG;
	m_tintB			= pPtFxAttr->m_tintB;
	m_tintA			= pPtFxAttr->m_tintA;

	m_hasTint					= (u8)pPtFxAttr->m_hasTint;
	m_ignoreDamageModel			= (u8)pPtFxAttr->m_ignoreDamageModel;
	m_playOnParent				= (u8)pPtFxAttr->m_playOnParent;
	m_onlyOnDamageModel			= (u8)pPtFxAttr->m_onlyOnDamageModel;
	m_allowRubberBulletShotFx	= (u8)pPtFxAttr->m_allowRubberBulletShotFx;
	m_allowElectricBulletShotFx = (u8)pPtFxAttr->m_allowElectricBulletShotFx;
	m_isTrigger					= (u8)isTrigger;
	m_unused					= 0;

	m_effectId = u16(effectId);
}

void CPacketEntityAmbientFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(eventInfo.GetEntity())
	{
		CParticleAttr decompressedVer;

		Vector3 uncompresedPosition;
		Quaternion uncompresedRotation;

		m_PosAndRot.FetchQuaternionAndPos(uncompresedRotation, uncompresedPosition);

		decompressedVer.SetPos(uncompresedPosition);

		decompressedVer.qX = uncompresedRotation.x;
		decompressedVer.qY = uncompresedRotation.y;
		decompressedVer.qZ = uncompresedRotation.z;
		decompressedVer.qW = uncompresedRotation.w;

		decompressedVer.m_tagHashName.SetHash(m_tagHash);

		decompressedVer.m_fxType		= m_fxType;
		decompressedVer.m_boneTag		= m_boneTag;	
		decompressedVer.m_scale			= m_scale;		
		decompressedVer.m_probability	= m_probability;
		decompressedVer.m_tintR			= m_tintR;	
		decompressedVer.m_tintG			= m_tintG;	
		decompressedVer.m_tintB			= m_tintB;		
		decompressedVer.m_tintA			= m_tintA;	

		decompressedVer.m_hasTint					= (m_hasTint != 0);					
		decompressedVer.m_ignoreDamageModel			= (m_ignoreDamageModel != 0);			
		decompressedVer.m_playOnParent				= (m_playOnParent != 0);				
		decompressedVer.m_onlyOnDamageModel			= (m_onlyOnDamageModel != 0);		
		decompressedVer.m_allowRubberBulletShotFx	= (m_allowRubberBulletShotFx != 0);	
		decompressedVer.m_allowElectricBulletShotFx	= (m_allowElectricBulletShotFx != 0);

		if(m_isTrigger != 0)
		{
			g_vfxEntity.TriggerPtFxEntityAnim(eventInfo.GetEntity(), &decompressedVer);
		}
		else
		{
			g_vfxEntity.UpdatePtFxEntityAnim(eventInfo.GetEntity(), &decompressedVer, (s32)m_effectId);
		}
	}
}

//========================================================================================================================
void CPacketEntityFragBreakFx::StorePosition(const Vector3& rPosition)
{
	m_Position[0] = rPosition.x;
	m_Position[1] = rPosition.y;
	m_Position[2] = rPosition.z;
}

//========================================================================================================================
void CPacketEntityFragBreakFx::LoadPosition(Vector3& rPosition) const
{
	rPosition.x = m_Position[0];
	rPosition.y = m_Position[1];
	rPosition.z = m_Position[2];
}

//========================================================================================================================
void CPacketEntityFragBreakFx::StoreQuaternion(const Quaternion& rQuatRot)
{
	m_QuatRot = QuantizeQuaternionS3(rQuatRot);
}

//========================================================================================================================
void CPacketEntityFragBreakFx::LoadQuaternion(Quaternion& rQuatRot) const
{
	DeQuantizeQuaternionS3(rQuatRot, m_QuatRot);
}

//========================================================================================================================

CPacketEntityFragBreakFx::CPacketEntityFragBreakFx(u32 uPfxHash, const Vector3& rEffectPos, const Quaternion& rQuatRot, float fUserScale, u16 uBoneTag, bool bPlayOnParent, bool bHasTint, u8 uR, u8 uG, u8 uB)
	: CPacketEvent(PACKETID_ENTITYFRAGBREAKFX, sizeof(CPacketEntityFragBreakFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	StorePosition(rEffectPos);
	StoreQuaternion(rQuatRot);
	m_fUserScale = fUserScale;
	m_uBoneTag = uBoneTag;
	m_bHasTint = bHasTint;
	m_Tint[0] = uR;
	m_Tint[1] = uG;
	m_Tint[2] = uB;

	m_bPlayOnParent = bPlayOnParent;

	// Just in case
	m_PadBits = 0;
	m_Pad = 0;
}

//========================================================================================================================
void CPacketEntityFragBreakFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	VfxEntityBreakPtFxInfo_s* pBreakInfo = g_vfxEntity.GetBreakPtFxInfo(m_pfxHash);
	if (pBreakInfo)
	{
		// get the matrix of the broken piece
		bool ownsFxMtx = true;
		Mat34V pFxMatrix;
		CVfxHelper::GetMatrixFromBoneTag(pFxMatrix, pEntity, (eAnimBoneTag)m_uBoneTag);

		Vector3 oVecPos;
		Quaternion oQuatRot;
		LoadPosition(oVecPos);
		LoadQuaternion(oQuatRot);

		// get the final position where this going to be 
		Vec3V fxPos;
		if (m_bPlayOnParent)
		{
			fxPos = pFxMatrix.d();
		}
		else
		{
			fxPos = Transform3x3(pFxMatrix, VECTOR3_TO_VEC3V(oVecPos));
			fxPos = fxPos + pFxMatrix.d(); 
		}

		// try to create the fx system
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pBreakInfo->ptFxHashName);
		if (pFxInst)
		{
			// fx system created ok - start it up and set its position and orientation
			if (ownsFxMtx)
			{
				ptfxAttach::Attach(pFxInst, pEntity, -1);
			}
			else
			{
				pFxInst->SetBaseMtx(pFxMatrix);
			}

			// play offset from the bone (unless we're playing on the parent)
			if (!m_bPlayOnParent)
			{
				pFxInst->SetOffsetPos(VECTOR3_TO_VEC3V(oVecPos));
				pFxInst->SetOffsetRot(QUATERNION_TO_QUATV(oQuatRot));
			}

			pFxInst->SetUserZoom(m_fUserScale);	

			/*TODO4FIVE g_ptFxManager.GetColnInterface().Register(pFxInst, pEntity);*/

			if (m_bHasTint)
			{
				pFxInst->SetColourTint(Vec3V(m_Tint[0]/255.0f, m_Tint[1]/255.0f, m_Tint[2]/255.0f));
			}

			pFxInst->Start();
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: ENTITY FRAG BREAK FX: %0X: ID: %0X\n", (u32)pEntity, pEntity->GetReplayID());
#endif
		}
	}
}

//========================================================================================================================
void CPacketEntityShotFx_Old::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_fxHashName);
	if (pFxInst)
	{
		// fx system created ok - start it up and set its matrix
		if (pEntity)
		{
			ptfxAttach::Attach(pFxInst, pEntity, m_boneIndex);
		}

		if (m_atCollisionPoint)
		{
			Matrix34 matrix;
			m_posAndQuat.LoadMatrix(matrix);
			pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(matrix));
		}
		else
		{
			Vector3 pos;
			Quaternion quat;
			m_posAndQuat.LoadPosition(pos);
			m_posAndQuat.GetQuaternion(quat);
			pFxInst->SetOffsetPos(VECTOR3_TO_VEC3V(pos));
			pFxInst->SetOffsetRot(QUATERNION_TO_QUATV(quat));
		}

		if (m_hasTint)
		{
			Vec3V vColourTint(m_tintR/255.0f, m_tintG/255.0f, m_tintB/255.0f);
			ptfxWrapper::SetColourTint(pFxInst, vColourTint);
			ptfxWrapper::SetAlphaTint(pFxInst, m_tintA/255.0f);
		}

		pFxInst->SetUserZoom(m_scale);	
		pFxInst->Start();
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketEntityShotFx::CPacketEntityShotFx(u32 fxHashName, s32 boneIndex, bool atCollisionPoint, const Mat34V_In& relFxMat, Vec3V_In vEffectPos, QuatV_In vEffectQuat, 
										 bool hasTint, float tintR, float tintG, float tintB, float tintA, float scale)
										 : CPacketEvent(PACKETID_ENTITYSHOTFX, sizeof(CPacketEntityShotFx))
										 , CPacketEntityInfoStaticAware()
										 , m_fxHashName(fxHashName)
										 , m_boneIndex(boneIndex)
										 , m_atCollisionPoint(atCollisionPoint)
										 , m_scale(scale)
										 , m_hasTint(hasTint)
										 , m_tintR(tintR)
										 , m_tintG(tintG)
										 , m_tintB(tintB)
										 , m_tintA(tintA)
{
	if( atCollisionPoint )
		m_posAndQuat.StoreMatrix(MAT34V_TO_MATRIX34(relFxMat));
	else
		m_posAndQuat.StorePosQuat(VEC3V_TO_VECTOR3(vEffectPos), QUATV_TO_QUATERNION(vEffectQuat));
}

void CPacketEntityShotFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_fxHashName);
	if (pFxInst)
	{
		// fx system created ok - start it up and set its matrix
		if (pEntity)
		{
			ptfxAttach::Attach(pFxInst, pEntity, m_boneIndex);
		}

		if (m_atCollisionPoint)
		{
			Matrix34 matrix;
			m_posAndQuat.LoadMatrix(matrix);
			pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(matrix));
		}
		else
		{
			Vector3 pos;
			Quaternion quat;
			m_posAndQuat.LoadPosition(pos);
			m_posAndQuat.GetQuaternion(quat);
			pFxInst->SetOffsetPos(VECTOR3_TO_VEC3V(pos));
			pFxInst->SetOffsetRot(QUATERNION_TO_QUATV(quat));
		}

		if (m_hasTint)
		{
			Vec3V vColourTint(m_tintR/255.0f, m_tintG/255.0f, m_tintB/255.0f);
			ptfxWrapper::SetColourTint(pFxInst, vColourTint);
			ptfxWrapper::SetAlphaTint(pFxInst, m_tintA/255.0f);
		}

		pFxInst->SetUserZoom(m_scale);	
		pFxInst->Start();
	}
}


//========================================================================================================================

CPacketPtFxFragmentDestroy::CPacketPtFxFragmentDestroy(u32 fxHashName, Mat34V_In &posAndRot, Vec3V_In &offsetPos, QuatV_In &offsetRot, float scale, bool hasTint, u8 tintR, u8 tintG, u8 tintB, u8 tintA)
	: CPacketEvent(PACKETID_PTFX_FRAGMENTDESTROY, sizeof(CPacketPtFxFragmentDestroy))
	, CPacketEntityInfo()
{
	m_fxHashName = fxHashName;
	m_posAndRot.StoreMatrix(MAT34V_TO_MATRIX34(posAndRot));
	m_OffsetPos.Store(VEC3V_TO_VECTOR3(offsetPos));
	m_OffsetRot.StoreQuaternion(QUATV_TO_QUATERNION(offsetRot));
	m_scale = scale;
	m_hasTint = hasTint;
	m_tintR = tintR;
	m_tintG = tintG;
	m_tintB = tintB;
	m_tintA = tintA;
}

void	CPacketPtFxFragmentDestroy::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
	// try to create the fx system
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_fxHashName);
	if( pFxInst )
	{
		Matrix34 mat;
		m_posAndRot.LoadMatrix(mat);
		Vector3 vEffectPos;
		m_OffsetPos.Load(vEffectPos);
		Quaternion quat;
		m_OffsetRot.LoadQuaternion(quat);

		// fx system created ok - start it up and set its position and orientation
		pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(mat));

		// play offset from the bone 
		pFxInst->SetOffsetPos(VECTOR3_TO_VEC3V(vEffectPos));

		pFxInst->SetOffsetRot(QUATERNION_TO_QUATV(quat));

		pFxInst->SetUserZoom(m_scale);	

		if (m_hasTint)
		{
			Vec3V vColourTint(m_tintR/255.0f, m_tintG/255.0f, m_tintB/255.0f);
			ptfxWrapper::SetColourTint(pFxInst, vColourTint);
			ptfxWrapper::SetAlphaTint(pFxInst, m_tintA/255.0f);
		}

		pFxInst->Start();

		// Can't add audio as we have no entity.
	}
}


//========================================================================================================================
CPacketDecalFragmentDestroy::CPacketDecalFragmentDestroy(u32 tagHashName, Vec3V_In &pos, float scale)
	: CPacketEvent(PACKETID_DECAL_FRAGMENTDESTROY, sizeof(CPacketDecalFragmentDestroy))
	, CPacketEntityInfo()
{
	m_tagHashName = tagHashName;
	m_pos.Store(VEC3V_TO_VECTOR3(pos));
	m_scale = scale;
}

void	CPacketDecalFragmentDestroy::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
	VfxEntityBreakDecalInfo_s* pDecalInfo = g_vfxEntity.GetBreakDecalInfo(m_tagHashName);
	if (pDecalInfo)
	{
		Vector3 vDecalPosWld;
		m_pos.Load(vDecalPosWld);

		g_vfxEntity.TriggerDecalGeneric(pDecalInfo, VECTOR3_TO_VEC3V(vDecalPosWld), m_scale);
	}
}

#endif // GTA_REPLAY
