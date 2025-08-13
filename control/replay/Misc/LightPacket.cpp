#include "LightPacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayLightingManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "peds/ped.h"
#include "timecycle/TimeCycle.h"
#include "control/replay/ReplaySupportClasses.h"

//////////////////////////////////////////////////////////////////////////
void CPacketCutsceneLight::Store(u32 recordDataIndex)
{		
	PACKET_EXTENSION_RESET(CPacketCutsceneLight);
	CPacketBase::Store(PACKETID_CUTSCENELIGHT, sizeof(CPacketCutsceneLight));

	m_PacketData = ReplayLightingManager::GetCutsceneLightData(recordDataIndex);
}

void CPacketCutsceneLight::Extract(const CReplayState&) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketCutsceneLight) == 0);
	ReplayLightingManager::AddCutsceneLight(m_PacketData);
}

//////////////////////////////////////////////////////////////////////////
CPacketCutsceneCharacterLightParams::CPacketCutsceneCharacterLightParams(const cutfCameraCutCharacterLightParams &params)
	: CPacketEvent(PACKETID_CUTSCENECHARACTERLIGHTPARAMS, sizeof(CPacketCutsceneCharacterLightParams))
	, CPacketEntityInfo()
	, m_fIntensity(params.m_fIntensity)
	, m_fNightIntensity(params.m_fNightIntensity)
	, m_bUseTimeCycleValues(params.m_bUseTimeCycleValues)
	, m_bUseAsIntensityModifier(params.m_bUseAsIntensityModifier)
{
	m_vDirection.Store(params.m_vDirection);
	m_vColour.Store(params.m_vColour);
}

void CPacketCutsceneCharacterLightParams::Extract(CEventInfo<void>&) const
{
	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	if( csMgr )
	{
		cutfCameraCutCharacterLightParams params;
		m_vDirection.Load(params.m_vDirection);
		m_vColour.Load(params.m_vColour);
		params.m_fIntensity = m_fIntensity;
		params.m_fNightIntensity = m_fNightIntensity;
		params.m_bUseTimeCycleValues = m_bUseTimeCycleValues;
		params.m_bUseAsIntensityModifier = m_bUseAsIntensityModifier;

		csMgr->SetReplayCutsceneCharacterLightParams(params);
	}
}
//////////////////////////////////////////////////////////////////////////
CPacketCutsceneCameraArgs::CPacketCutsceneCameraArgs(const cutfCameraCutEventArgs* cameraCutEventArgs)
: CPacketEvent(PACKETID_CUTSCENECAMERAARGS, sizeof(CPacketCutsceneCameraArgs))
, CPacketEntityInfo()
{
	m_vPosition.Store(cameraCutEventArgs->GetPosition());

	Vector4 rotation = cameraCutEventArgs->GetRotationQuaternion();
	Quaternion asQuaternion = Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);

	m_vRotationQuaternion.StoreQuaternion(asQuaternion);

	m_fNearDrawDistance = cameraCutEventArgs->GetNearDrawDistance();
	m_fFarDrawDistance = cameraCutEventArgs->GetFarDrawDistance();
	m_fMapLodScale  = cameraCutEventArgs->GetMapLodScale();
	m_ReflectionLodRangeStart = cameraCutEventArgs->GetReflectionLodRangeStart();
	m_ReflectionLodRangeEnd = cameraCutEventArgs->GetReflectionSLodRangeEnd();
	m_ReflectionSLodRangeStart = cameraCutEventArgs->GetReflectionSLodRangeStart();
	m_ReflectionSLodRangeEnd = cameraCutEventArgs->GetReflectionSLodRangeEnd();
	m_LodMultHD = cameraCutEventArgs->GetLodMultHD();
	m_LodMultOrphanedHD = cameraCutEventArgs->GetLodMultOrphanHD();
	m_LodMultLod = cameraCutEventArgs->GetLodMultLod();
	m_LodMultSLod1 = cameraCutEventArgs->GetLodMultSlod1();
	m_LodMultSLod2 = cameraCutEventArgs->GetLodMultSlod2();
	m_LodMultSLod3 = cameraCutEventArgs->GetLodMultSlod3();
	m_LodMultSLod4 = cameraCutEventArgs->GetLodMultSlod4();
	m_WaterReflectionFarClip = cameraCutEventArgs->GetWaterReflectionFarClip();
	m_SSAOLightInten = cameraCutEventArgs->GetLightSSAOInten();
	m_ExposurePush = cameraCutEventArgs->GetExposurePush();
	m_LightFadeDistanceMult  = cameraCutEventArgs->GetLightFadeDistanceMult();
	m_LightShadowFadeDistanceMult = cameraCutEventArgs->GetLightShadowFadeDistanceMult();
	m_LightSpecularFadeDistMult = cameraCutEventArgs->GetLightShadowFadeDistanceMult();
	m_LightVolumetricFadeDistanceMult = cameraCutEventArgs->GetLightVolumetricFadeDistanceMult(); 
	m_DirectionalLightMultiplier = cameraCutEventArgs->GetDirectionalLightMultiplier(); 
	m_LensArtefactMultiplier =  cameraCutEventArgs->GetLensArtefactMultiplier();
	m_BloomMax = cameraCutEventArgs->GetBloomMax();

	m_DisableHighQualityDof = cameraCutEventArgs->GetDisableHighQualityDof() ? 1 : 0; 
	m_FreezeReflectionMap = cameraCutEventArgs->IsReflectionMapFrozen() ? 1 : 0; 
	m_DisableDirectionalLighting = cameraCutEventArgs->IsDirectionalLightDisabled() ? 1 : 0; 
	
	m_AbsoluteIntensityEnabled = cameraCutEventArgs->IsAbsoluteIntensityEnabled() ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////
void CPacketCutsceneCameraArgs::Extract(CEventInfo<void>& ) const
{
	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	if( csMgr )
	{
		cutfCameraCutEventArgs params;

		Vector3 position;
		m_vPosition.Load(position);
		params.SetPosition(position);

		Quaternion asQuaternion;
		m_vRotationQuaternion.LoadQuaternion(asQuaternion);
		Vector4 rotation(asQuaternion.x, asQuaternion.y, asQuaternion.x, asQuaternion.w);
		params.SetRotationQuaternion(rotation);

		params.SetNearDrawDistance(m_fNearDrawDistance);
		params.SetFarDrawDistance(m_fFarDrawDistance);
		params.SetMapLodScale(m_fMapLodScale);
		params.SetReflectionLodRangeStart(m_ReflectionLodRangeStart);
		params.SetReflectionSLodRangeEnd(m_ReflectionLodRangeEnd);
		params.SetReflectionSLodRangeStart(m_ReflectionSLodRangeStart);
		params.SetReflectionSLodRangeEnd(m_ReflectionSLodRangeEnd);
		params.SetLodMultHD(m_LodMultHD);
		params.SetLodMultOrphanHD(m_LodMultOrphanedHD);
		params.SetLodMultLod(m_LodMultLod);
		params.SetLodMultSlod1(m_LodMultSLod1);
		params.SetLodMultSlod2(m_LodMultSLod2);
		params.SetLodMultSlod3(m_LodMultSLod3);
		params.SetLodMultSlod4(m_LodMultSLod4);
		params.SetWaterReflectionFarClip(m_WaterReflectionFarClip);
		params.SetLightSSAOInten(m_SSAOLightInten);
		params.SetExposurePush(m_ExposurePush);
		params.SetLightFadeDistanceMult(m_LightFadeDistanceMult);
		params.SetLightShadowFadeDistanceMult(m_LightShadowFadeDistanceMult);
		params.SetLightShadowFadeDistanceMult(m_LightSpecularFadeDistMult);
		params.SetLightVolumetricFadeDistanceMult(m_LightVolumetricFadeDistanceMult); 
		params.SetDirectionalLightMultiplier(m_DirectionalLightMultiplier); 
		params.SetLensArtefactMultiplier(m_LensArtefactMultiplier);
		params.SetBloomMax(m_BloomMax);

		params.SetDisableHighQualityDof(m_DisableHighQualityDof);
		params.SetReflectionMapFrozen(m_FreezeReflectionMap);
		params.SetDirectionalLightDisabled(m_DisableDirectionalLighting);

		params.SetAbsoluteIntensityEnabled(m_AbsoluteIntensityEnabled);

		csMgr->SetReplayCutsceneCameraArgs(params);
	}
}

//////////////////////////////////////////////////////////////////////////

CPacketLightBroken::CPacketLightBroken(s32 lightHash)
:	CPacketEvent(PACKETID_LIGHT_BROKEN, sizeof(CPacketLightBroken))
,	CPacketEntityInfo()
,	m_LightHash(lightHash) 
{

}

void CPacketLightBroken::Extract(CEventInfo<void>& pEventInfo) const
{
	if(pEventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		Lights::RegisterBrokenLight(m_LightHash);
	}
	else
	{
		Lights::RemoveRegisteredBrokenLight(m_LightHash);
	}
}



CPacketPhoneLight::CPacketPhoneLight()
	:	CPacketEvent(PACKETID_PHONE_LIGHT, sizeof(CPacketPhoneLight))
	,	CPacketEntityInfo()
{
	//Nothing to store, just need the ped and phone entities
}

void CPacketPhoneLight::Extract(CEventInfo<CEntity>& pEventInfo) const
{
	if(pEventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
	{
		return;
	}

	CEntity* pEntity = pEventInfo.GetEntity(0);
	if(pEntity && pEntity->GetIsTypePed() && pEventInfo.GetEntity(1))
	{
		ReplayLightingManager::AddPhoneLight(static_cast<CPed*>(pEntity), pEventInfo.GetEntity(1));
	}
}

CPacketPedLight::CPacketPedLight()
	:	CPacketEvent(PACKETID_PED_LIGHT, sizeof(CPacketPedLight))
	,	CPacketEntityInfo()
{
	//Nothing to store, just need the ped
}

void CPacketPedLight::Extract(CEventInfo<CEntity>& pEventInfo) const
{
	if(pEventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
	{
		return;
	}

	CEntity* pEntity = pEventInfo.GetEntity(0);
	if(pEntity && pEntity->GetIsTypePed() ASSERT_ONLY(&& g_timeCycle.CanAccess()))
	{
		ReplayLightingManager::AddPedLight(static_cast<CPed*>(pEntity));
	}
}




#endif // GTA_REPLAY
