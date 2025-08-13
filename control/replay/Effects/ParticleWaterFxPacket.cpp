//=====================================================================================================
// name:		ParticleWaterFxPacket.cpp
//=====================================================================================================

#include "ParticleWaterFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/ReplayInternal.h"
#include "audio/boataudioentity.h"
#include "control/gamelogic.h"
#include "game/ModelIndices.h"
#include "peds/ped.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/particles/ptfxdefines.h"
#include "vfx/systems/VfxVehicle.h"
#include "vfx/VfxImmediateInterface.h"
#include "scene/world/GameWorld.h"
#include "vehicles/Submarine.h"
#include "control/replay/Compression/Quantization.h"

//////////////////////////////////////////////////////////////////////////
CPacketWaterSplashHeliFx::CPacketWaterSplashHeliFx(Vec3V_In rPos)
	: CPacketEvent(PACKETID_WATERSPLASHHELIFX, sizeof(CPacketWaterSplashHeliFx))
	, CPacketEntityInfo()
	, m_position(rPos)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterSplashHeliFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();

	Vec3V oVecPos;
	m_position.Load(oVecPos);

	CVfxImmediateInterface::TriggerPtFxSplashHeliBlade(pEntity, oVecPos);
}



//========================================================================================================================
CPacketWaterSplashGenericFx::CPacketWaterSplashGenericFx(u32 uPfxHash, const Vector3& rPos, float fSpeed)
	: CPacketEvent(PACKETID_WATERSPLASHGENERICFX, sizeof(CPacketWaterSplashGenericFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_position.Store(rPos);

	m_fCustom = fSpeed;
}

//========================================================================================================================
void CPacketWaterSplashGenericFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	Vector3 oVecPos;
	m_position.Load(oVecPos);

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		// calc the world space fx matrix 
		Vec3V dir = VECTOR3_TO_VEC3V(oVecPos) - pEntity->GetTransform().GetPosition();
		if (IsZeroAll(dir))
		{
			dir.SetZ(ScalarV(V_ONE));
		}
		dir = Normalize(dir);
		Mat34V fxOffsetMat;
		CVfxHelper::CreateMatFromVecY(fxOffsetMat, VECTOR3_TO_VEC3V(oVecPos), dir);

		// make the fx matrix relative to the entity
		Mat34V invEntityMat = pEntity->GetMatrix();
		Invert3x3Full(invEntityMat, invEntityMat);
		Transform(fxOffsetMat, invEntityMat);

		// set the position etc
		pFxInst->SetOffsetMtx(fxOffsetMat);
		ptfxAttach::Attach(pFxInst, pEntity, -1);

		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_fCustom);

		// start the fx
		pFxInst->Start();
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WATER SPLASH GENERIC FX: %0X: ID: %0X\n", (u32)pEntity, pEntity->GetReplayID());
#endif
	}
}


//========================================================================================================================
CPacketWaterSplashVehicleFx::CPacketWaterSplashVehicleFx(u32 uPfxHash, const Matrix34& mMat, float fSizeEvo, float fLateralSpeedEvo, float fDownwardSpeedEvo)
	: CPacketEvent(PACKETID_WATERSPLASHVEHICLEFX, sizeof(CPacketWaterSplashVehicleFx))
	, m_HashName(uPfxHash)
	, m_SizeEvo(fSizeEvo)
	, m_LateralSpeedEvo(fLateralSpeedEvo)
	, m_DownwardSpeedEvo(fDownwardSpeedEvo)
{
	m_Matrix.StoreMatrix(mMat);
}

//========================================================================================================================
void CPacketWaterSplashVehicleFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity(0);

	if( pVehicle == NULL )
	{
		return;
	}
	
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_HashName);
	if (pFxInst)
	{
		// calc the world matrix
		Matrix34 vFxMtx;
		m_Matrix.LoadMatrix(vFxMtx);
		// set the matrices
		pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(vFxMtx));

		// set the position etc
		ptfxAttach::Attach(pFxInst, pVehicle, -1, PTFXATTACHMODE_INFO_ONLY);

		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), m_SizeEvo);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_l", 0x1ACEB730), m_LateralSpeedEvo);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_d", 0x82B6875E), m_DownwardSpeedEvo);

		// start the fx
		pFxInst->Start();
	}
}

//========================================================================================================================
CPacketWaterSplashVehicleTrailFx::CPacketWaterSplashVehicleTrailFx(u32 uPfxHash, s32 uPtfxOffset, const Matrix34& mMat, float fSizeEvo, float fSpeedEvo)
	: CPacketEvent(PACKETID_WATERSPLASHVEHICLETRAILFX, sizeof(CPacketWaterSplashVehicleTrailFx))
	, m_HashName(uPfxHash)
	, m_PtfxOffset(uPtfxOffset)
	, m_SizeEvo(fSizeEvo)
	, m_SpeedEvo(fSpeedEvo)
{
	m_Matrix.StoreMatrix(mMat);
}

//========================================================================================================================
void CPacketWaterSplashVehicleTrailFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity(0);

	if( pVehicle == NULL )
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_TRAIL+m_PtfxOffset, true, m_HashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		Matrix34 vFxMtx;
		m_Matrix.LoadMatrix(vFxMtx);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);
		pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(vFxMtx));
		
		// set evolutions	
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), m_SizeEvo);
			
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), m_SpeedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}

//========================================================================================================================
CPacketWaterSplashPedFx::CPacketWaterSplashPedFx(u32 uPfxHash, float fSpeed, Vec3V waterPos, bool isPlayerVfx)
	: CPacketEvent(PACKETID_WATERSPLASHPEDFX, sizeof(CPacketWaterSplashPedFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_speedEvo = fSpeed;
	m_waterPos.Store(waterPos);
	m_isPlayerVfx = isPlayerVfx;
}

//========================================================================================================================
void CPacketWaterSplashPedFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if(!pPed) // Should probably see why the ped is coming through as NULL sometimes...
		return;

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Vec3V vWaterPos;
		m_waterPos.Load(vWaterPos);
		pFxInst->SetBasePos(vWaterPos);

		// don't cull if this is a player effect
		if (m_isPlayerVfx)
		{
			pFxInst->SetCanBeCulled(false);
		}

		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_speedEvo);

		// start the fx
		pFxInst->Start();
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WATER PED SPLASH FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
	}
}




//////////////////////////////////////////////////////////////////////////
CPacketWaterSplashPedInFx::CPacketWaterSplashPedInFx(Vec3V_In surfacePos, Vec3V_In boneDir, float boneSpeed, s32 boneID, bool isPlayerVfx)
	: CPacketEvent(PACKETID_WATERSPLASHPEDINFX, sizeof(CPacketWaterSplashPedInFx))
	, CPacketEntityInfo()
	, m_surfacePos(surfacePos)
	, m_boneDir(boneDir)
	, m_boneSpeed(boneSpeed)
	, m_skeletonBoneID(boneID)
	, m_isPlayerVfx(isPlayerVfx ? 1 : 0)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterSplashPedInFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();	

	Vec3V surfacePos, boneDir;
	m_surfacePos.Load(surfacePos);
	m_boneDir.Load(boneDir);

	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);

	const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = NULL;
	for(int i = 0; i < pVfxPedInfo->GetPedSkeletonBoneNumInfos(); ++i)
	{
		pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
		if(pSkeletonBoneInfo->m_limbId == (VfxPedLimbId_e)m_skeletonBoneID)
			break;
	}

	VfxWaterSampleData_s sampleData;
	sampleData.isInWater	= true;
	sampleData.vSurfacePos	= surfacePos;

	CVfxImmediateInterface::TriggerPtFxSplashPedIn(pPed, pVfxPedInfo, pSkeletonBoneInfo, NULL, &sampleData, 0, boneDir, m_boneSpeed, m_isPlayerVfx == 1 ? true : false);
}


//////////////////////////////////////////////////////////////////////////
CPacketWaterSplashPedOutFx::CPacketWaterSplashPedOutFx(Vec3V_In boneDir, s32 boneIndex, float boneSpeed, s32 boneID)
	: CPacketEvent(PACKETID_WATERSPLASHPEDOUTFX, sizeof(CPacketWaterSplashPedOutFx))
	, CPacketEntityInfo()
	, m_boneDir(boneDir)
	, m_boneIndex(boneIndex)
	, m_boneSpeed(boneSpeed)
	, m_skeletonBoneID(boneID)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterSplashPedOutFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();

	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);

	Vec3V boneDir;
	m_boneDir.Load(boneDir);

	const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = NULL;
	for(int i = 0; i < pVfxPedInfo->GetPedSkeletonBoneNumInfos(); ++i)
	{
		pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
		if(pSkeletonBoneInfo->m_limbId == (VfxPedLimbId_e)m_skeletonBoneID)
			break;
	}

	CVfxImmediateInterface::TriggerPtFxSplashPedOut(pPed, pVfxPedInfo, pSkeletonBoneInfo, m_boneIndex, boneDir, m_boneSpeed);
}


//////////////////////////////////////////////////////////////////////////
CPacketWaterSplashPedWadeFx::CPacketWaterSplashPedWadeFx(Vec3V_In surfacePos, Vec3V_In boneDir, Vec3V_In riverDir, float boneSpeed, s32 boneID, s32 ptfxOffset)
	: CPacketEvent(PACKETID_WATERSPLASHPEDWADEFX, sizeof(CPacketWaterSplashPedWadeFx))
	, m_surfacePos(surfacePos)
	, m_boneDir(boneDir)
	, m_riverDir(riverDir)
	, m_skeletonBoneID(boneID)
	, m_boneSpeed(boneSpeed)
	, m_ptfxOffset(ptfxOffset)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterSplashPedWadeFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	
	Vec3V surfacePos, boneDir, riverDir;
	m_surfacePos.Load(surfacePos);
	m_boneDir.Load(boneDir);
	m_riverDir.Load(riverDir);

	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);

	const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = NULL;
	for(int i = 0; i < pVfxPedInfo->GetPedSkeletonBoneNumInfos(); ++i)
	{
		pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
		if(pSkeletonBoneInfo->m_limbId == (VfxPedLimbId_e)m_skeletonBoneID)
			break;
	}
	const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();

	VfxWaterSampleData_s sampleData;
	sampleData.isInWater	= true;
	sampleData.vSurfacePos	= surfacePos;

	CVfxImmediateInterface::UpdatePtFxSplashPedWade(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, &sampleData, riverDir, boneDir, m_boneSpeed, m_ptfxOffset);
}


//////////////////////////////////////////////////////////////////////////
CPacketWaterBoatEntryFx::CPacketWaterBoatEntryFx(Vec3V_In rPos)
	: CPacketEvent(PACKETID_WATERBOATENTRYFX, sizeof(CPacketWaterBoatEntryFx))
	, CPacketEntityInfo()
	, m_position(rPos)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterBoatEntryFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	Vec3V oVecPos;
	m_position.Load(oVecPos);

	CVfxImmediateInterface::TriggerPtFxBoatEntry(pVehicle, pVfxVehicleInfo, oVecPos);
}


//////////////////////////////////////////////////////////////////////////
CPacketWaterBoatBowFx::CPacketWaterBoatBowFx(Vec3V_In surfacePos, Vec3V_In dir, u8 uOffset, bool goingForward)
	: CPacketEvent(PACKETID_WATERBOATBOWFX, sizeof(CPacketWaterBoatBowFx))
	, CPacketEntityInfo()
	, m_surfacePosition(surfacePos)
	, m_direction(dir)
	, m_uOffset(uOffset)
	, m_goingForward(goingForward)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterBoatBowFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	 
	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	Vec3V surfacePos, direction;
	m_surfacePosition.Load(surfacePos);
	m_direction.Load(direction);

	VfxWaterSampleData_s sampleData;
	sampleData.isInWater	= true;
	sampleData.vSurfacePos	= surfacePos;

	CVfxImmediateInterface::UpdatePtFxBoatBow(pVehicle, pVfxVehicleInfo, m_uOffset, &sampleData, direction, m_goingForward);
}


//////////////////////////////////////////////////////////////////////////
CPacketWaterBoatWashFx::CPacketWaterBoatWashFx(Vec3V_In surfacePos, s32 uOffset)
	: CPacketEvent(PACKETID_WATERBOATWASHFX, sizeof(CPacketWaterBoatWashFx))
	, CPacketEntityInfo()
	, m_surfacePosition(surfacePos)
	, m_uOffset(uOffset)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterBoatWashFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	Vector3 position;
	m_surfacePosition.Load(position);

	VfxWaterSampleData_s sampleData;
	sampleData.isInWater	= true;
	sampleData.vSurfacePos	= RCC_VEC3V(position);

	CVfxImmediateInterface::UpdatePtFxBoatWash(pVehicle, pVfxVehicleInfo, m_uOffset, &sampleData);
}


//========================================================================================================================
CPacketWaterSplashVehWadeFx::CPacketWaterSplashVehWadeFx(u32 uPfxHash, const Vector3& rPos, const Vector3& rSamplePos, s32 sIndex, float fMaxLevel)
	: CPacketEventInterp(PACKETID_WATERSPLASHVEHWADEFX, sizeof(CPacketWaterSplashVehWadeFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_position.Store(rPos);
	m_samplePosition.Store(rSamplePos);

	m_sIndex = sIndex;
	m_fMaxLevel = fMaxLevel;
	
	replayAssert(m_sIndex < 0xff);
	SetPfxKey((u8)m_sIndex + (u8)PTFXOFFSET_VEHICLE_WADE);
}

//========================================================================================================================
void CPacketWaterSplashVehWadeFx::Extract(const CInterpEventInfo<CPacketWaterSplashVehWadeFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_WADE+m_sIndex, true, m_pfxHash, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		if (eventInfo.GetInterp() == 0.0f)
		{
			Vector3 oVecPos, oVecSamplePos;
			m_position.Load(oVecPos);
			m_samplePosition.Load(oVecSamplePos);

			Vec3V sampleDirWld;

			sampleDirWld = VECTOR3_TO_VEC3V(oVecSamplePos) - pVehicle->GetTransform().GetPosition();
			sampleDirWld = UnTransform3x3Full(pVehicle->GetMatrix(), sampleDirWld);
			sampleDirWld.SetZ(ScalarV(V_ZERO));
			sampleDirWld = Transform3x3(pVehicle->GetMatrix(), sampleDirWld);
			sampleDirWld = NormalizeSafe(sampleDirWld, Vec3V(V_Y_AXIS_WZERO));

			Vector3 vehMoveSpeed = pVehicle->GetVelocity();
			vehMoveSpeed.z = 0.0f;

			float dot = DotProduct(VEC3V_TO_VECTOR3(sampleDirWld), vehMoveSpeed);

			Mat34V fxMat;
			Vec3V fxDir = sampleDirWld;

			CVfxHelper::CreateMatFromVecY(fxMat, VECTOR3_TO_VEC3V(oVecPos), fxDir);

			pFxInst->SetBaseMtx(fxMat);

			// set evolutions
			float speedEvo = Abs(dot)*0.2f;
			speedEvo = Min(speedEvo, 1.0f);
			speedEvo = Max(speedEvo, 0.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), speedEvo);

			pFxInst->SetUserZoom(m_fMaxLevel);
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketWaterSplashVehWadeFx const* pNext = eventInfo.GetNextPacket();
			ExtractInterpolateableValues(pFxInst, pVehicle, pNext, eventInfo.GetInterp());
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size",0xc052dea7), 0.4f);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WATER SPLASH VEHICLE WADE FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}

//========================================================================================================================
void CPacketWaterSplashVehWadeFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CVehicle* pVehicle, CPacketWaterSplashVehWadeFx const* pNextPacket, float fInterp) const
{
	Vector3	oPrevPos, oNextPos, oFinalPos;
	m_position.Load(oPrevPos);
	pNextPacket->m_position.Load( oNextPos );
	oFinalPos = Lerp(fInterp, oPrevPos, oNextPos);

	Vector3	oPrevSamplePos, oNextSamplePos, oFinalSamplePos;
	m_samplePosition.Load(oPrevSamplePos);
	pNextPacket->m_samplePosition.Load( oNextSamplePos );
	oFinalSamplePos = Lerp(fInterp, oPrevSamplePos, oNextSamplePos);

	Vector3 sampleDirWld;
	sampleDirWld = oFinalSamplePos - VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	MAT34V_TO_MATRIX34(pVehicle->GetMatrix()).UnTransform3x3(sampleDirWld);
	sampleDirWld.z = 0.0f;
	MAT34V_TO_MATRIX34(pVehicle->GetMatrix()).Transform3x3(sampleDirWld);
	sampleDirWld.NormalizeSafe();

	Vector3 vehMoveSpeed = pVehicle->GetVelocity();
	vehMoveSpeed.z = 0.0f;

	float dot = DotProduct(sampleDirWld, vehMoveSpeed);

	Mat34V fxMat;
	Vector3 fxDir = sampleDirWld;

	CVfxHelper::CreateMatFromVecY(fxMat, VECTOR3_TO_VEC3V(oFinalPos), VECTOR3_TO_VEC3V(fxDir));

	pFxInst->SetBaseMtx(fxMat);

	// set evolutions
	float speedEvo = Abs(dot)*0.2f;
	speedEvo = Min(speedEvo, 1.0f);
	speedEvo = Max(speedEvo, 0.0f);
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), speedEvo);

	float fMaxLevel = Lerp(fInterp, m_fMaxLevel, pNextPacket->GetMaxLevel());
	pFxInst->SetUserZoom(fMaxLevel);
}

//========================================================================================================================
CPacketWaterCannonJetFx::CPacketWaterCannonJetFx(s32 boneIndex, Vec3V_In vOffsetPos, float speed)
	: CPacketEvent(PACKETID_WATERCANNONJETFX, sizeof(CPacketWaterCannonJetFx))
	, CPacketEntityInfo()
{
	m_BoneIndex = boneIndex;
	m_Position.Store(VEC3V_TO_VECTOR3(vOffsetPos));
	m_Speed = speed;
}

//========================================================================================================================
void CPacketWaterCannonJetFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();

	Vector3 position;
	m_Position.Load(position);

	//Needed for the audio position
	//Mat34V boneMtx;
	//CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, m_BoneIndex);
	//Vec3V vWldPos = Transform(boneMtx, VECTOR3_TO_VEC3V(position));

	// register the fx system
	bool justCreated = false;
	atHashWithStringNotFinal hashName = atHashWithStringNotFinal("water_cannon_jet",0x7E44F582);
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_WATERCANNON_JET, true, hashName, justCreated);

	if (pFxInst)
	{
		pFxInst->SetOffsetPos(VECTOR3_TO_VEC3V(position));
	
		float speedEvo = CVfxHelper::GetInterpValue(m_Speed, 0.0f, 20.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		ptfxAttach::Attach(pFxInst, pVehicle, m_BoneIndex);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
			
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WATER CANNON SPRAY FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}

	//Audio still needs to be hooked up.
	/*fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pVehicle, PTFXOFFSET_WATERCANNON_JET);
	g_AmbientAudioEntity.RegisterEffectAudio(hashName, effectUniqueID, RCC_VECTOR3(vWldPos), pVehicle);*/
}

//========================================================================================================================
CPacketWaterCannonSprayFx::CPacketWaterCannonSprayFx(Mat34V matrix, float angleEvo, float distEvo)
	: CPacketEvent(PACKETID_WATERCANNONSPRAYFX, sizeof(CPacketWaterCannonSprayFx))
	, CPacketEntityInfo()
{
	m_Matrix.StoreMatrix(MAT34V_TO_MATRIX34(matrix));
	m_AngleEvo = angleEvo;
	m_DistEvo = distEvo;
}

//========================================================================================================================
void CPacketWaterCannonSprayFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_WATERCANNON_SPRAY, false, atHashWithStringNotFinal("water_cannon_spray",0xC4881C2F), justCreated);

	if (pFxInst)
	{
		Matrix34 fxMat;
		m_Matrix.LoadMatrix(fxMat);
		pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(fxMat));

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0x0f3805b5), m_AngleEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dist", 0xd27ac355), m_DistEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


//========================================================================================================================

CPacketSubDiveFx::CPacketSubDiveFx(float diveEvo)
	: CPacketEvent(PACKETID_SUBDIVEFX, sizeof(CPacketSubDiveFx))
	, CPacketEntityInfo()
{
	m_diveEvo = diveEvo;
}

void CPacketSubDiveFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVeh = eventInfo.GetEntity();
	if(pVeh)
	{
		Assertf(pVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINE, "CPacketSubDiveFx::Extract() - Expecting submarine! But this entity isn't one!");

		CSubmarine* pSubmarine = static_cast<CSubmarine*>(pVeh);

		bool justCreated;
		ptxEffectInst* pFxInst = NULL;
		pFxInst = ptfxManager::GetRegisteredInst(pSubmarine, PTFXOFFSET_VEHICLE_SUBMARINE_DIVE, false, atHashWithStringNotFinal("veh_sub_dive",0x9FCE7CEA), justCreated);
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pSubmarine, -1);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dive", 0xE7E43AD4), m_diveEvo);

			// check if the effect has just been created 
			if (justCreated)
			{
				// it has just been created - start it and set it's position
				pFxInst->Start();
			}
		}
	}
}

#endif // GTA_REPLAY
