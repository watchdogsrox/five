//=====================================================================================================
// name:		ParticleBloodFxPacket.cpp
//=====================================================================================================

#include "ParticleBloodFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "peds/ped.h"
#include "control/replay/ReplayInternal.h"
#include "vfx/decals/DecalManager.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "scene/world/GameWorld.h"

#include "vfx/systems/vfxBlood.h"


//////////////////////////////////////////////////////////////////////////
// Recorded in CVFXBlood::TriggerPtFxFallToDeath
CPacketBloodFallDeathFx::CPacketBloodFallDeathFx(u32 uPfxHash)
	: CPacketEvent(PACKETID_BLOODFALLDEATHFX, sizeof(CPacketBloodFallDeathFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketBloodFallDeathFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pPed, -1);

		if (pPed==(CPed*)CGameWorld::FindLocalPlayer())
		{
			pFxInst->SetCanBeCulled(false);
		}

		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: BLOOD FALL DEATH FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
// Recorded in CVFXBlood::TriggerPtFxFallToDeath
CPacketBloodSharkBiteFx::CPacketBloodSharkBiteFx(u32 uPfxHash, u32 boneIndex)
	: CPacketEvent(PACKETID_BLOODSHARKBITEFX, sizeof(CPacketBloodSharkBiteFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
	, m_boneIndex(boneIndex)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketBloodSharkBiteFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pPed, m_boneIndex);

		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: BLOOD SHARK BITE FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
	}
}

//========================================================================================================================
CPacketBloodPigeonShotFx::CPacketBloodPigeonShotFx(u32 uPfxHash)
	: CPacketEvent(PACKETID_BLOODPIGEONFX, sizeof(CPacketBloodPigeonShotFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{}

//========================================================================================================================
void CPacketBloodPigeonShotFx::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pObject = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		pFxInst->SetBaseMtx(pObject->GetMatrix());

		// blood evo
		float bloodEvo = 1.0f;
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("blood",0xca1d58f8), bloodEvo);
		
		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: BLOOD PIGEON FX: %0X: ID: %0X\n", (u32)pObject, pObject->GetReplayID());
#endif
	}
}


//========================================================================================================================
CPacketBloodMouthFx::CPacketBloodMouthFx(u32 uPfxHash)
	: CPacketEvent(PACKETID_BLOODMOUTHFX, sizeof(CPacketBloodMouthFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{}

//========================================================================================================================
void CPacketBloodMouthFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	(void)pPed;

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		/*TODO4FIVE s32 sBoneIndex = CPedBoneTagConvertor::GetBoneIndex(pPed->GetSkeletonData(), (BONETAG_FB_L_CRN_MOUTH));
		if (sBoneIndex != -1)
		{
			Matrix34* pMouthMtx = &pPed->GetGlobalMtx(sBoneIndex);
			if (pMouthMtx)
			{
				pFxInst->SetPosMatrix34(*pMouthMtx);
				////g_ptFx.SetMatrixPointerOwner(pFxInst, pPed);	

				pFxInst->Start();

#if DEBUG_REPLAY_PFX
				replayDebugf1("REPLAY: BLOOD MOUTH FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
			}
		}*/
	}
}


//========================================================================================================================
void CPacketBloodWheelSquashFx::StorePosition(const Vector3& rPosition)
{
	m_Position[0] = rPosition.x;
	m_Position[1] = rPosition.y;
	m_Position[2] = rPosition.z;
}

//========================================================================================================================
void CPacketBloodWheelSquashFx::LoadPosition(Vector3& rPosition) const
{
	rPosition.x = m_Position[0];
	rPosition.y = m_Position[1];
	rPosition.z = m_Position[2];
}

//========================================================================================================================
CPacketBloodWheelSquashFx::CPacketBloodWheelSquashFx(u32 uPfxHash, const Vector3& rPosition)
	: CPacketEvent(PACKETID_BLOODWHEELFX, sizeof(CPacketBloodWheelSquashFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	StorePosition(rPosition);
}

//========================================================================================================================
void CPacketBloodWheelSquashFx::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Vector3 fxPos;
		LoadPosition(fxPos);

		Mat34V fxMat = pVehicle->GetMatrix();
		fxMat.Setd(VECTOR3_TO_VEC3V(fxPos));

		pFxInst->SetBaseMtx(fxMat);

		float speedEvo = pVehicle->GetVelocity().Mag()/10.0f;
		speedEvo = Min(speedEvo, 1.0f);
		replayAssert(speedEvo>=0.0f);
		replayAssert(speedEvo<=1.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), speedEvo);

		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: BLOOD WHEEL FX: %0X: ID: %0X\n", (u32)pVehicle, pVehicle->GetReplayID());
#endif
	}
}



//////////////////////////////////////////////////////////////////////////
// Recorded in CVfxBlood::DoVfxBlood
CPacketBloodFx::CPacketBloodFx(const VfxCollInfo_s& vfxCollInfo, bool bodyArmour, bool hOrLArmour, bool isUnderwater, bool isPlayer, s32 fxInfoIndex, float distSqr, bool doFx, bool doDamage, bool isEntry)
	: CPacketEvent(PACKETID_BLOODFX, sizeof(CPacketBloodFx))
	, CPacketEntityInfo()
{
	m_infoIndex				= (s8)fxInfoIndex;
	m_weaponGroup			= (u8)vfxCollInfo.weaponGroup;

	m_bodyArmour			= bodyArmour;
	m_heavyOrLightArmour	= hOrLArmour;
	m_isUnderwater			= isUnderwater;
	m_isPlayer				= isPlayer;

	m_position.Store(RCC_VECTOR3(vfxCollInfo.vPositionWld));
	m_direction.Store(RCC_VECTOR3(vfxCollInfo.vDirectionWld));
	m_componentID			= vfxCollInfo.componentId;

	m_distSqr				= distSqr;
	m_doPtfx				= doFx;
	m_doDamage				= doDamage;

	m_isEntry				= isEntry;
}


//////////////////////////////////////////////////////////////////////////
void CPacketBloodFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity				= eventInfo.GetEntity(0);
	const CEntity* pFiringEntity	= eventInfo.GetEntity(1);

	Vector3 position, direction;
	m_position.Load(position);
	m_direction.Load(direction);

	VfxCollInfo_s vfxCollInfo;
	vfxCollInfo.weaponGroup		= (eWeaponEffectGroup)m_weaponGroup;
	vfxCollInfo.vPositionWld	= VECTOR3_TO_VEC3V(position);
	vfxCollInfo.vDirectionWld	= VECTOR3_TO_VEC3V(direction);
	vfxCollInfo.componentId		= m_componentID;
	vfxCollInfo.regdEnt			= pEntity;

	VfxBloodInfo_s* pVfxBloodInfo = NULL;
	
	if(m_isEntry)
		pVfxBloodInfo = g_vfxBlood.GetEntryInfo(m_infoIndex);
	else
		pVfxBloodInfo = g_vfxBlood.GetExitInfo(m_infoIndex);

	if(m_doPtfx)
	{
		// get the vfx ped info
		CPed* pPed					= verify_cast<CPed*>(pEntity);
		const CPed* pFiringPed = NULL;
		if(pFiringEntity)
		{
			pFiringPed		= verify_cast<const CPed*>(pFiringEntity);
		}

		CVfxPedInfo* pVfxPedInfo	= g_vfxPedInfoMgr.GetInfo(pPed);
		if (pVfxPedInfo)
		{
			bool producedPtFx;
			bool producedDamage;
			g_vfxBlood.TriggerPtFxBlood(pVfxPedInfo, pVfxBloodInfo, vfxCollInfo, m_distSqr, false, m_bodyArmour, m_heavyOrLightArmour, m_isUnderwater, m_isPlayer, pFiringPed, m_doPtfx, m_doDamage, producedPtFx, producedDamage);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketBloodDecal::CPacketBloodDecal(const decalUserSettings& decalSettings, int decalID)
	: CPacketDecalEvent(decalID, PACKETID_BLOODDECAL, sizeof(CPacketBloodDecal))
	, CPacketEntityInfoStaticAware()
{	
	m_positionWld.Store(decalSettings.instSettings.vPosition);
	m_directionWld.Store(decalSettings.instSettings.vDirection);
	m_Side.Store(decalSettings.instSettings.vSide);
	m_vDimensions.Store(decalSettings.instSettings.vDimensions);

	m_RenderSettingsIndex	= decalSettings.bucketSettings.renderSettingsIndex;	
	m_TotalLife				= decalSettings.instSettings.totalLife ;
	m_FadeInTime			= decalSettings.instSettings.fadeInTime;
	m_uvMultStart			= decalSettings.instSettings.uvMultStart;
	m_uvMultTime			= decalSettings.instSettings.uvMultTime;
	m_colR					= decalSettings.instSettings.colR;
	m_colG					= decalSettings.instSettings.colG;
	m_colB					= decalSettings.instSettings.colB;
	m_duplicateRejectDist	= decalSettings.pipelineSettings.duplicateRejectDist;
	m_componentId			= decalSettings.pipelineSettings.colnComponentId;
}

//////////////////////////////////////////////////////////////////////////
void CPacketBloodDecal::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}

	CEntity* pEntity = eventInfo.GetEntity(0);

	// set up the decal settings
	decalUserSettings decalSettings;
	decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_BLOOD_SPLAT;
	decalSettings.bucketSettings.renderSettingsIndex = m_RenderSettingsIndex;
	m_positionWld.Load(decalSettings.instSettings.vPosition);
	m_directionWld.Load(decalSettings.instSettings.vDirection);
	m_Side.Load(decalSettings.instSettings.vSide);
	m_vDimensions.Load(decalSettings.instSettings.vDimensions);
	decalSettings.instSettings.totalLife = m_TotalLife;

	if( eventInfo.GetIsFirstFrame() )
		decalSettings.instSettings.fadeInTime = 0.0f;
	else
		decalSettings.instSettings.fadeInTime = m_FadeInTime;

	decalSettings.instSettings.uvMultStart = m_uvMultStart;
	decalSettings.instSettings.uvMultEnd = 1.0f;
	decalSettings.instSettings.uvMultTime = m_uvMultTime;
	decalSettings.instSettings.colR = m_colR;
	decalSettings.instSettings.colG = m_colG;
	decalSettings.instSettings.colB = m_colB;			
	decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
	decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
	decalSettings.pipelineSettings.duplicateRejectDist = m_duplicateRejectDist;
	decalSettings.pipelineSettings.colnComponentId = (s16)m_componentId;

	m_decalId = g_decalMan.AddSettingsReplay(pEntity, decalSettings);
}



//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketBloodPool_V1 = 1;

CPacketBloodPool::CPacketBloodPool(const decalUserSettings& decalSettings, int decalID)
	: CPacketDecalEvent(decalID, PACKETID_PTEXBLOODPOOL, sizeof(CPacketBloodPool), CPacketBloodPool_V1)
	, CPacketEntityInfo()
{	
	m_positionWld.Store(decalSettings.instSettings.vPosition);
	m_directionWld.Store(decalSettings.instSettings.vDirection);
	m_Side.Store(decalSettings.instSettings.vSide);
	m_vDimensions.Store(decalSettings.instSettings.vDimensions);

	m_RenderSettingsIndex	= decalSettings.bucketSettings.renderSettingsIndex;	
	m_TotalLife				= decalSettings.instSettings.totalLife;
	m_FadeInTime			= decalSettings.instSettings.fadeInTime;
	m_uvMultStart			= decalSettings.instSettings.uvMultStart;
	m_uvMultEnd				= decalSettings.instSettings.uvMultEnd;
	m_uvMultTime			= decalSettings.instSettings.uvMultTime;
	m_colR					= decalSettings.instSettings.colR;
	m_colG					= decalSettings.instSettings.colG;
	m_colB					= decalSettings.instSettings.colB;
	m_duplicateRejectDist	= decalSettings.pipelineSettings.duplicateRejectDist;
	m_componentId			= decalSettings.pipelineSettings.colnComponentId;
	m_LiquidType			= decalSettings.instSettings.liquidType;
}

void CPacketBloodPool::UpdateMonitorPacket()
{
	Vec3V vPos;
	m_positionWld.Load(vPos);
	float UVMult = g_decalMan.ProcessUVMultChanges(m_decalId, vPos);
	if( UVMult != -1.0f)
	{
		m_uvMultStart = Max(UVMult, m_uvMultStart);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketBloodPool::Extract(const CEventInfo<void>& eventInfo) const
{
	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}

	// set up the decal settings
	decalUserSettings decalSettings;

	decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_POOL_LIQUID;
	decalSettings.bucketSettings.renderSettingsIndex = m_RenderSettingsIndex;
	m_positionWld.Load(decalSettings.instSettings.vPosition);			
	m_directionWld.Load(decalSettings.instSettings.vDirection);			
	m_Side.Load(decalSettings.instSettings.vSide);		
	m_vDimensions.Load(decalSettings.instSettings.vDimensions);		
	decalSettings.instSettings.uvMultStart = m_uvMultStart;
	decalSettings.instSettings.uvMultEnd = m_uvMultEnd;
	decalSettings.instSettings.colR = m_colR;
	decalSettings.instSettings.colG = m_colG;
	decalSettings.instSettings.colB = m_colB;
	decalSettings.instSettings.alphaFront = 255;
	decalSettings.instSettings.alphaBack = 255;
	if(GetPacketVersion() >= CPacketBloodPool_V1)
		decalSettings.instSettings.liquidType = m_LiquidType;	
	else
		decalSettings.instSettings.liquidType = VFXLIQUID_TYPE_BLOOD;	
	decalSettings.instSettings.isLiquidPool = true;			

	decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
	decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;

	if( eventInfo.GetIsFirstFrame() )
		decalSettings.lodSettings.currLife = DECAL_INST_INV_FADE_TIME_UNITS_HZ;

	m_decalId = g_decalMan.AddSettingsReplay(NULL, decalSettings);
}

#endif // GTA_REPLAY