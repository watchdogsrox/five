//=====================================================================================================
// name:		ParticleScriptedFx.cpp
//=====================================================================================================

#include "ParticleScriptedFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "physics/WorldProbe/WorldProbe.h"
#include "script/script.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxattach.h"
#include "control/replay/Compression/Quantization.h"

//========================================================================================================================
void CPacketScriptedFx::StoreMatrix(const Matrix34& rMatrix)
{
	Quaternion q;
	rMatrix.ToQuaternion(q);

	m_ScriptedQuaternion = QuantizeQuaternionS3(q);

	m_Position[0] = rMatrix.d.x;
	m_Position[1] = rMatrix.d.y;
	m_Position[2] = rMatrix.d.z;
}

//========================================================================================================================
void CPacketScriptedFx::LoadMatrix(Matrix34& rMatrix) const
{
	Quaternion q;
	DeQuantizeQuaternionS3(q, m_ScriptedQuaternion);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_Position[2];
}

//========================================================================================================================
CPacketScriptedFx::CPacketScriptedFx(u32 uPfxHash, const Matrix34& /*rMatrix*/, float /*fScale*/, bool /*bUseParent*/, u32 /*uParentID*/, u32 /*uBoneTag*/)
	: CPacketEvent(PACKETID_SCRIPTEDFX, sizeof(CPacketScriptedFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	replayFatalAssertf(false, "Todo");
}

//========================================================================================================================
void CPacketScriptedFx::Extract(const CEventInfo<void>&) const
{
	replayFatalAssertf(false, "Todo");
}


//========================================================================================================================
void CPacketScriptedUpdateFx::StoreMatrix(const Matrix34& rMatrix)
{
	Quaternion q;
	rMatrix.ToQuaternion(q);

	m_ScriptedQuaternion = QuantizeQuaternionS3(q);

	m_Position[0] = rMatrix.d.x;
	m_Position[1] = rMatrix.d.y;
	m_EvolutionValueOrPositionZ = rMatrix.d.z;
}

//========================================================================================================================
void CPacketScriptedUpdateFx::LoadMatrix(Matrix34& rMatrix) const
{
	Quaternion q;
	DeQuantizeQuaternionS3(q, m_ScriptedQuaternion);
	rMatrix.FromQuaternion(q);
	replayAssert(rMatrix.IsOrthonormal());

	rMatrix.d.x = m_Position[0];
	rMatrix.d.y = m_Position[1];
	rMatrix.d.z = m_EvolutionValueOrPositionZ;
}

//========================================================================================================================
CPacketScriptedUpdateFx::CPacketScriptedUpdateFx(u32 uPfxHash, const Matrix34& rMatrix)
	: CPacketEvent(PACKETID_SCRIPTEDUPDATEFX, sizeof(CPacketScriptedUpdateFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	//PTFX_OFFSETS,		m_bIsE2 == 1	m_bExtraFlags == 1
	StoreMatrix(rMatrix);
}

//========================================================================================================================
CPacketScriptedUpdateFx::CPacketScriptedUpdateFx(u32 uPfxHash, char* szEvolutionName, float fEvolutionValue)
	: CPacketEvent(PACKETID_SCRIPTEDUPDATEFX, sizeof(CPacketScriptedUpdateFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	//PTFX_EVOLUTION,	m_bIsE2 == 1	m_bExtraFlags == 0

	strncpy(m_EvolutionName, szEvolutionName, 11);
	m_EvolutionName[11] = '\0';

	m_EvolutionValueOrPositionZ = fEvolutionValue;
}

//========================================================================================================================
CPacketScriptedUpdateFx::CPacketScriptedUpdateFx(u32 uPfxHash, u32 uColor)
	: CPacketEvent(PACKETID_SCRIPTEDUPDATEFX, sizeof(CPacketScriptedUpdateFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	//PTFX_TINT,		m_bIsE2 == 0	m_bExtraFlags == 1

	m_uColor = uColor;
}

//========================================================================================================================
void CPacketScriptedUpdateFx::Extract(const CEventInfo<void>&) const
{
	replayFatalAssertf(false, "Todo");
}


//========================================================================================================================
CPacketBuildingSwap::CPacketBuildingSwap(float fX, float fY, float fZ, float fRadius, u32 uOldHash, u32 uNewHash)
	: CPacketEvent(PACKETID_SCRIPTEDBUILDINGSWAP, sizeof(CPacketBuildingSwap))
{
	m_Position[0] = fX;
	m_Position[1] = fY;
	m_Position[2] = fZ;

	m_Radius = fRadius;
	m_uOldHash = uOldHash;
	m_uNewHash = uNewHash;
}

//========================================================================================================================
void CPacketBuildingSwap::Extract(const CEventInfo<void>&) const
{
	fwModelId sOldModelIndex;
	CModelInfo::GetBaseModelInfoFromHashKey(m_uOldHash, &sOldModelIndex);

	// If invalid return value
	if (sOldModelIndex.IsValid() == false)//AC: Not needed now? >    && sOldModelIndex >= NUM_MODEL_INFOS)
		return;

	fwModelId sNewModelIndex;
	CModelInfo::GetBaseModelInfoFromHashKey(m_uNewHash, &sNewModelIndex);

	// If invalid return value
	if (sNewModelIndex.IsValid() == false)//AC: Not needed now? >    && sNewModelIndex >= NUM_MODEL_INFOS)
		return;

	float fNewZ = m_Position[2];
	if (fNewZ <= -100.0f)
	{
		/*TODO4FIVE fNewZ = CWorldProbe::FindGroundZForCoord(m_Position[0], m_Position[1]);*/
	}

	ClosestObjectDataStruct ClosestObjectData;
	ClosestObjectData.fClosestDistanceSquared = m_Radius * m_Radius * 4.0f;
	ClosestObjectData.pClosestObject = NULL;
	ClosestObjectData.CoordToCalculateDistanceFrom = Vector3(m_Position[0], m_Position[1], fNewZ);
	ClosestObjectData.ModelIndexToCheck = sOldModelIndex.GetModelIndex();
	ClosestObjectData.bCheckModelIndex = true;
	ClosestObjectData.bCheckStealableFlag = false;
	ClosestObjectData.nOwnedByMask = 0xFFFFFFFF;

	/*TODO4FIVE CSphere testSphere(m_Radius, ClosestObjectData.CoordToCalculateDistanceFrom);
	CGameWorld::ForAllEntitiesIntersecting(testSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData, 
		(FAE_TYPE_BUILDINGS|FAE_TYPE_OBJECTS|FAE_TYPE_DUMMIES), (FAE_CTRL_INTERSECT|FAE_CTRL_OUTSIDE|FAE_CTRL_INSIDE));

	if (ClosestObjectData.pClosestObject == NULL)
	{
		//	used to do a 2d distance check with FindLodOfTypeInRange
		CGameWorld::ForAllLodsIntersecting(testSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData);
	}*/

	CEntity* pClosestObj = ClosestObjectData.pClosestObject;

	// Nothing found
	if (pClosestObj == NULL)
		return;

	// Not building, not interested
	if (!pClosestObj->GetIsTypeBuilding())
		return;

	// Sanity checks
	if (pClosestObj->GetModelIndex() == sOldModelIndex.GetModelIndex())
	{
		/*TODO4FIVE CGameWorld::Remove(pClosestObj);
		((CBuilding*)pClosestObj)->ReplaceWithNewModel(sNewModelIndex);

		CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfo(sNewModelIndex);

		if(pModelInfo->GetPhysicsDictionary() >= 0)
			pClosestObj->SetBaseFlag(fwEntity::HAS_PHYSICS_DICT);
		else
			pClosestObj->ClearBaseFlag(fwEntity::HAS_PHYSICS_DICT);

		pClosestObj->ForceWorldInsertToOutside();
		CGameWorld::Add(pClosestObj);*/
	}
}

//========================================================================================================================
void CPacketBuildingSwap::Reset() const
{
	fwModelId sOldModelIndex;
	CModelInfo::GetBaseModelInfoFromHashKey(m_uOldHash, &sOldModelIndex);

	// If invalid return value
	if (sOldModelIndex.IsValid() == false)//AC: Not needed now? >    && sOldModelIndex >= NUM_MODEL_INFOS)
		return;

	fwModelId sNewModelIndex;
	CModelInfo::GetBaseModelInfoFromHashKey(m_uNewHash, &sNewModelIndex);

	// If invalid return value
	if (sNewModelIndex.IsValid() == false)//AC: Not needed now? >    && sOldModelIndex >= NUM_MODEL_INFOS)
		return;

	float fNewZ = m_Position[2];
	if (fNewZ <= -100.0f)
	{
		/*TODO4FIVE fNewZ = CWorldProbe::FindGroundZForCoord(m_Position[0], m_Position[1]);*/
	}

	ClosestObjectDataStruct ClosestObjectData;
	ClosestObjectData.fClosestDistanceSquared = m_Radius * m_Radius * 4.0f;
	ClosestObjectData.pClosestObject = NULL;
	ClosestObjectData.CoordToCalculateDistanceFrom = Vector3(m_Position[0], m_Position[1], fNewZ);
	ClosestObjectData.ModelIndexToCheck = sNewModelIndex.GetModelIndex();
	ClosestObjectData.bCheckModelIndex = true;
	ClosestObjectData.bCheckStealableFlag = false;
	ClosestObjectData.nOwnedByMask = 0xFFFFFFFF;

	/*TODO4FIVE CSphere testSphere(m_Radius, ClosestObjectData.CoordToCalculateDistanceFrom);
	CGameWorld::ForAllEntitiesIntersecting(testSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData, 
		(FAE_TYPE_BUILDINGS|FAE_TYPE_OBJECTS|FAE_TYPE_DUMMIES), (FAE_CTRL_INTERSECT|FAE_CTRL_OUTSIDE|FAE_CTRL_INSIDE));

	if (ClosestObjectData.pClosestObject == NULL)
	{
		//	used to do a 2d distance check with FindLodOfTypeInRange
		CGameWorld::ForAllLodsIntersecting(testSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData);
	}*/

	CEntity* pClosestObj = ClosestObjectData.pClosestObject;

	// Nothing found
	if (pClosestObj == NULL)
		return;

	// Not building, not interested
	if (!pClosestObj->GetIsTypeBuilding())
		return;

	// Sanity checks
	if (pClosestObj->GetModelIndex() == sNewModelIndex.GetModelIndex())
	{
		/*TODO4FIVE CGameWorld::Remove(pClosestObj);
		((CBuilding*)pClosestObj)->ReplaceWithNewModel(sOldModelIndex);

		CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfo(sOldModelIndex);

		if(pModelInfo->GetPhysicsDictionary() >= 0)
			pClosestObj->SetBaseFlag(fwEntity::HAS_PHYSICS_DICT);
		else
			pClosestObj->ClearBaseFlag(fwEntity::HAS_PHYSICS_DICT);

		pClosestObj->ForceWorldInsertToOutside();
		CGameWorld::Add(pClosestObj);*/
	}
}

#endif // GTA_REPLAY
