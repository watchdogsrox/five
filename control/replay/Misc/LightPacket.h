#ifndef LIGHT_PACKET_H
#define LIGHT_PACKET_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "Control/replay/ReplayEventManager.h"
#include "renderer/Lights/LightCommon.h"
#include "renderer/Lights/lights.h"
#include "cutfile/cutfeventargs.h"

//////////////////////////////////////////////////////////////////////////

enum eReplaylightPacketState
{
	LIGHTPACKET_ADDED = 0,
	LIGHTPACKET_UPDATED,
	LIGHTPACKET_NOTUPDATED,
	LIGHTPACKET_FORDELETION
};

struct lightPacketData
{
	void PackageData(u32 nameHash, u32 lightFlags, const Vector3& pos, const Vector3& col, float intensity, u32 hourFlags, eLightType lightType,										   
		const Vector3& lightDir, const Vector3& lightTangent, float lightFallOff, float innerConeAngle, float lightConeAngle,
		bool drawVolume, float volumeIntensity, float volumeSizeScale, const Vector4& volumeColIntensity, float falloffExp,
		float shadowBlur, bool pushLightsEarly, bool addAmbientLight)
	{
		m_position.Store(pos);
		m_col.Store(col);
		m_lightDirection.Store(lightDir);
		m_lightTangent.Store(lightTangent);
		m_volumeOuterColourAndIntensity.Store(volumeColIntensity);

		m_intensity = intensity; 
		m_lightFalloff = lightFallOff;
		m_innerConeAngle = innerConeAngle;
		m_lightConeAngle = lightConeAngle;
		m_falloffExp = falloffExp;
		m_shadowBlur = shadowBlur;
		m_volumeIntensity = volumeIntensity;
		m_volumeSizeScale = volumeSizeScale;

		m_lightFlags = lightFlags;
		m_hourFlags = hourFlags;

		m_drawVolume = drawVolume ? 1 : 0;
		m_pushLightsEarly = pushLightsEarly ? 1 : 0;
		m_addAmbientLight = addAmbientLight ? 1 : 0;

		m_lightType = lightType;
		m_nameHash = nameHash;
	};

	CPacketVector<3>	m_position;
	CPacketVector<3>	m_col;
	CPacketVector<3>	m_lightDirection;
	CPacketVector<3>	m_lightTangent;
	CPacketVector<4>	m_volumeOuterColourAndIntensity;

	float m_intensity;
	float m_lightFalloff;
	float m_innerConeAngle;
	float m_lightConeAngle;
	float m_falloffExp;
	float m_shadowBlur;
	float m_volumeIntensity;
	float m_volumeSizeScale;

	u32 m_lightFlags;
	u32 m_hourFlags;

	u8 m_drawVolume : 1;
	u8 m_pushLightsEarly : 1;
	u8 m_addAmbientLight : 1;

	eLightType m_lightType : 5;

	u32 m_nameHash;
};

struct sLightPacketRecordData
{
	lightPacketData data;
	eReplaylightPacketState dataState;
};

class CPacketCutsceneLight : public CPacketBase
{
public:
	void	Store(u32 recordDataIndex);
	void	StoreExtensions(u32 recordDataIndex) { PACKET_EXTENSION_RESET(CPacketCutsceneLight); (void)recordDataIndex; };
	void 	Extract(const CReplayState& playbackState) const;

	void	PrintXML(FileHandle handle) const	{	CPacketBase::PrintXML(handle);	}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CUTSCENELIGHT, "Validation of CPacketCutsceneLight Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket()  && GetPacketID() == PACKETID_CUTSCENELIGHT;
	}

private:

	sLightPacketRecordData m_PacketData;
	DECLARE_PACKET_EXTENSION(CPacketCutsceneLight);
};

//////////////////////////////////////////////////////////////////////////
class CPacketCutsceneCharacterLightParams : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketCutsceneCharacterLightParams(const cutfCameraCutCharacterLightParams &params);

	void	Extract(CEventInfo<void>& pEventInfo) const;
	ePreloadResult	Preload(CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CUTSCENECHARACTERLIGHTPARAMS, "Validation of CPacketCutsceneCharacterLightParams Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_CUTSCENECHARACTERLIGHTPARAMS;
	}

private:
	CPacketVector<3> m_vDirection;
	CPacketVector<3> m_vColour;
	float m_fIntensity;
	float m_fNightIntensity; 
	u8 m_bUseTimeCycleValues : 1;
	u8 m_bUseAsIntensityModifier : 1; 
};

//////////////////////////////////////////////////////////////////////////
class CPacketCutsceneCameraArgs : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketCutsceneCameraArgs(const cutfCameraCutEventArgs* cameraCutEventArgs);

	void	Extract(CEventInfo<void>& pEventInfo) const;
	ePreloadResult	Preload(CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CUTSCENECAMERAARGS, "Validation of CPacketCutsceneCharacterLightParams Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_CUTSCENECAMERAARGS;
	}

private:
	//do we need all this? could it be quantized?
	CPacketVector<3> m_vPosition;
	CPacketQuaternionH m_vRotationQuaternion;

	float m_fNearDrawDistance;
	float m_fFarDrawDistance;
	float m_fMapLodScale;
	float m_ReflectionLodRangeStart;
	float m_ReflectionLodRangeEnd;
	float m_ReflectionSLodRangeStart;
	float m_ReflectionSLodRangeEnd;
	float m_LodMultHD;
	float m_LodMultOrphanedHD;
	float m_LodMultLod;
	float m_LodMultSLod1;
	float m_LodMultSLod2;
	float m_LodMultSLod3;
	float m_LodMultSLod4;
	float m_WaterReflectionFarClip;
	float m_SSAOLightInten;
	float m_ExposurePush;
	float m_LightFadeDistanceMult; 
	float m_LightShadowFadeDistanceMult; 
	float m_LightSpecularFadeDistMult; 
	float m_LightVolumetricFadeDistanceMult; 
	float m_DirectionalLightMultiplier; 
	float m_LensArtefactMultiplier; 
	float m_BloomMax; 

	u8 m_DisableHighQualityDof : 1;
	u8 m_FreezeReflectionMap : 1;  
	u8 m_DisableDirectionalLighting : 1; 
	u8 m_AbsoluteIntensityEnabled : 1; 
};

class CPacketLightBroken : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketLightBroken(s32 lightHash);

	void	Extract(CEventInfo<void>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfo<void>&) const { return Lights::IsLightBroken(m_LightHash) == false; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_LIGHT_BROKEN, "Validation of CPacketLightBroken Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_LIGHT_BROKEN;
	}

private:
	s32 m_LightHash;

};

class CPacketPhoneLight : public CPacketEvent, public CPacketEntityInfo<2>
{
public:
	CPacketPhoneLight();

	void	Extract(CEventInfo<CEntity>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PHONE_LIGHT, "Validation of CPacketPhoneLight Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PHONE_LIGHT;
	}
};

class CPacketPedLight : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedLight();

	void	Extract(CEventInfo<CEntity>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PED_LIGHT, "Validation of CPacketPedLight Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PED_LIGHT;
	}
};

#endif // GTA_REPLAY

#endif // LIGHT_PACKET_H
