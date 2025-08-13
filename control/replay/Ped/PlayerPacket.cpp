//=====================================================================================================
// name:		PlayerPacket.cpp
//=====================================================================================================
#if 0
// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ped/PlayerPacket.h"
////#include "control/replay/ReplayMgr.h"
////#include "debug/Debug.h"
////#include "maths/vector.h"
#include "Peds/PlayerPed.h"
////#include "scene/portals/portal.h"

#if __BANK
#include "control/replay/ReplayInternal.h"
#endif // __BANK

#include "control/replay/Compression/Quantization.h"

//========================================================================================================================
CPacketPlayerUpdate::CPacketPlayerUpdate()
{
}

//========================================================================================================================
CPacketPlayerUpdate::~CPacketPlayerUpdate()
{
}

//=====================================================================================================
void CPacketPlayerUpdate::StoreXtraBoneRotation(const Matrix34& rMatrix, u8 uBoneID)
{
	Quaternion q;
	rMatrix.ToQuaternion(q);

	m_XtraBoneQuaternion[uBoneID] = QuantizeQuaternionS3(q);
}

//=====================================================================================================
void CPacketPlayerUpdate::LoadXtraBoneRotation(Matrix34& rMatrix, u8 sBoneID)
{
	Quaternion q;
	DeQuantizeQuaternionS3(q, m_XtraBoneQuaternion[sBoneID]);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());
}

//=====================================================================================================
void CPacketPlayerUpdate::LoadXtraBoneRotation(Matrix34& rMatrix, Quaternion& rQuatNew, float fInterp, u8 sBoneID)
{
	Quaternion rQuatPrev;
	DeQuantizeQuaternionS3(rQuatPrev, m_XtraBoneQuaternion[sBoneID]);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNew);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNew);

	rMatrix.FromQuaternion(rQuatInterp);
	replayAssert(rMatrix.IsOrthonormal());
}

//=====================================================================================================
void CPacketPlayerUpdate::FetchXtraBoneQuaternion(Quaternion& rQuat, u8 sBoneID)
{
	DeQuantizeQuaternionS3(rQuat, m_XtraBoneQuaternion[sBoneID]);
}

//=====================================================================================================
void CPacketPlayerUpdate::Store(CPed* pPed)
{
	PACKET_EXTENSION_RESET(CPacketPlayerUpdate);
	((CPacketPedUpdate*)this)->Store((CPed*)pPed, true);
	replayAssert(m_uBoneCount == PED_BONE_COUNT+PED_XTRA_BONE_COUNT);

	// Overwrite the packetId
	((CPacketInterp*)this)->Init((u8)PACKETID_PLAYERUPDATE);
#if 0
	// Bones (Xtra)
	const crSkeleton *pSkel = pPed->GetSkeleton();
	const crSkeletonData *pSkelData = &pPed->GetSkeletonData();

	for (u8 boneIndex = PED_BONE_COUNT; boneIndex < m_uBoneCount; boneIndex++) 
	{
		Matrix34 globalMat;
		pSkel->GetGlobalMtx((s32)boneIndex, RC_MAT34V(globalMat));
		const crBoneData *pParentBoneData = pSkelData->GetBoneData((s32)boneIndex)->GetParent();

		Matrix34 globalMatParent;
		pSkel->GetGlobalMtx(pParentBoneData->GetBoneId(), RC_MAT34V(globalMatParent));

		Matrix34 oBoneMatrix;
		oBoneMatrix = globalMat;
		oBoneMatrix.DotTranspose(globalMatParent);

		StoreXtraBoneRotation(oBoneMatrix, boneIndex-PED_BONE_COUNT);
	}
#endif
}

//=====================================================================================================
void CPacketPlayerUpdate::Extract(CPed* pPed, CPacketPlayerUpdate* pNextPacket, float fInterp)
{
	if (pPed)
	{
		((CPacketPedUpdate*)this)->Load((CPed*)pPed, (CPacketPedUpdate*)pNextPacket, fInterp, true);
#if 0
		if ( pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterp == 0.0f || IsNextDeleted())
		{
			// Bones (Xtra)
			const crSkeleton *pSkel = pPed->GetSkeleton();
			const crSkeletonData *pSkelData = &pPed->GetSkeletonData();

			for (u8 boneIndex = PED_BONE_COUNT; boneIndex < m_uBoneCount; boneIndex++) 
			{
				u16 uBoneTag = CReplayBonedataMap::GetBoneTagFromIndex(boneIndex, true);
				if (CReplayBonedataMap::IsMajorBone(uBoneTag))
				{
					Matrix34 globalMat;
					pSkel->GetGlobalMtx((s32)boneIndex, RC_MAT34V(globalMat));
					const crBoneData *pParentBoneData = pSkelData->GetBoneData((s32)boneIndex)->GetParent();

					Matrix34 globalMatParent;
					pSkel->GetGlobalMtx(pParentBoneData->GetBoneId(), RC_MAT34V(globalMatParent));

					Matrix34 oBoneMatrix, oBoneRotation;
					LoadXtraBoneRotation(oBoneRotation, boneIndex-PED_BONE_COUNT);

					oBoneRotation.d = globalMat.d;
#if __BANK
					if (CReplayMgr::s_bTurnOnDebugBones && CReplayMgr::s_uBoneId == uBoneTag)
					{
						const crBoneData *pBoneData = pSkelData->GetBoneData((s32)boneIndex);
						oBoneRotation.d = RCC_VECTOR3( pBoneData->GetDefaultTranslation() );
					}
#endif // __BANK
					oBoneMatrix.Dot(oBoneRotation, globalMatParent);

					sysMemCpy(&globalMat, &oBoneMatrix, sizeof(Matrix34));
				}
				else
				{
					Matrix34 pedGlobalMatrix;
					pPed->GetGlobalMtx((s32)boneIndex, pedGlobalMatrix);
					LoadXtraBoneRotation(pedGlobalMatrix, boneIndex-PED_BONE_COUNT);

					Matrix34 globalMat;
					pSkel->GetGlobalMtx((s32)boneIndex, RC_MAT34V(globalMat));
					const crBoneData *pBoneData = pSkelData->GetBoneData((s32)boneIndex);
					const crBoneData *pParentBoneData = pSkelData->GetBoneData((s32)boneIndex)->GetParent();

					Matrix34 globalMatParent;
					pSkel->GetGlobalMtx(pParentBoneData->GetBoneId(), RC_MAT34V(globalMatParent));

					Matrix34 oBoneMatrix, oBoneRotation;
					LoadXtraBoneRotation(oBoneRotation, boneIndex-PED_BONE_COUNT);
					oBoneRotation.d = RCC_VECTOR3( pBoneData->GetDefaultTranslation() );
					oBoneMatrix.Dot(oBoneRotation, globalMatParent);

					sysMemCpy(&globalMat, &oBoneMatrix, sizeof(Matrix34));
				}
			}
		}
		else if (HasNextOffset() && pNextPacket)
		{
			// Bones (Xtra)
			const crSkeleton *pSkel = pPed->GetSkeleton();
			const crSkeletonData *pSkelData = &pPed->GetSkeletonData();

			for (u8 boneIndex = PED_BONE_COUNT; boneIndex < m_uBoneCount; boneIndex++) 
			{
				u16 uBoneTag = CReplayBonedataMap::GetBoneTagFromIndex(boneIndex, true);
				if (CReplayBonedataMap::IsMajorBone(uBoneTag))
				{
					Matrix34 globalMat;
					pSkel->GetGlobalMtx((s32)boneIndex, RC_MAT34V(globalMat));
					const crBoneData *pParentBoneData = pSkelData->GetBoneData((s32)boneIndex)->GetParent();

					Matrix34 globalMatParent;
					pSkel->GetGlobalMtx(pParentBoneData->GetBoneId(), RC_MAT34V(globalMatParent));

					Matrix34 oBoneMatrix, oBoneRotation;
					Quaternion rQuatBoneNew;
					pNextPacket->FetchXtraBoneQuaternion(rQuatBoneNew, boneIndex-PED_BONE_COUNT);
					LoadXtraBoneRotation(oBoneRotation, rQuatBoneNew, fInterp, boneIndex-PED_BONE_COUNT);

					oBoneRotation.d = globalMat.d;
#if __BANK
					if (CReplayMgr::s_bTurnOnDebugBones && CReplayMgr::s_uBoneId == uBoneTag)
					{
						const crBoneData *pBoneData = pSkelData->GetBoneData((s32)boneIndex);
						oBoneRotation.d = RCC_VECTOR3( pBoneData->GetDefaultTranslation() );
					}
#endif // __BANK
					oBoneMatrix.Dot(oBoneRotation, globalMatParent);

					sysMemCpy(&globalMat, &oBoneMatrix, sizeof(Matrix34));	
				}
				else
				{
					Matrix34 globalMat;
					pSkel->GetGlobalMtx((s32)boneIndex, RC_MAT34V(globalMat));
					const crBoneData *pBoneData = pSkelData->GetBoneData((s32)boneIndex);
					const crBoneData *pParentBoneData = pSkelData->GetBoneData((s32)boneIndex)->GetParent();

					Matrix34 globalMatParent;
					pSkel->GetGlobalMtx(pParentBoneData->GetBoneId(), RC_MAT34V(globalMatParent));

					Matrix34 oBoneMatrix, oBoneRotation;
					Quaternion rQuatBoneNew;
					pNextPacket->FetchXtraBoneQuaternion(rQuatBoneNew, boneIndex-PED_BONE_COUNT);
					LoadXtraBoneRotation(oBoneRotation, rQuatBoneNew, fInterp, boneIndex-PED_BONE_COUNT);

					oBoneRotation.d = RCC_VECTOR3( pBoneData->GetDefaultTranslation() );
					oBoneMatrix.Dot(oBoneRotation, globalMatParent);

					sysMemCpy(&globalMat, &oBoneMatrix, sizeof(Matrix34));	
				}
			}
		}
		else
		{
			replayAssert(0 && "CPacketPlayerUpdate::Load - someone pooped the player interpolation packet");
		}
#endif
	}
}

#endif // GTA_REPLAY
#endif