#include "ReplayLightingManager.h"

#if GTA_REPLAY

#include "control/replay/replay.h"
#include "Peds/ped.h"

atFixedArray<PedLightInfo, NUM_OF_PED_LIGHTS_PER_FRAME> ReplayLightingManager::m_pedLightInfo;
atFixedArray<sLightPacketRecordData, NUM_OF_CUTSCENE_LIGHTS_PER_FRAME> ReplayLightingManager::m_recordedCutsceneLights;

void ReplayLightingManager::Reset()
{
	m_pedLightInfo.Reset();
	m_recordedCutsceneLights.Reset();
}

void ReplayLightingManager::RecordPackets(CReplayRecorder &recorder)
{
	//only cut scene light are handled this way at the moment
	for( u32 i = 0; i < m_recordedCutsceneLights.size(); i++ )
	{
		recorder.RecordPacketWithParam<CPacketCutsceneLight>(i);
	}
}

void ReplayLightingManager::AddLightsToScene()
{
	if(CReplayMgr::IsEditModeActive())
	{
		for(u32 i = 0; i < m_pedLightInfo.size(); i++)
		{
			AddPedLightsToGame(m_pedLightInfo[i]);
		}

		for(u32 i = 0; i < m_recordedCutsceneLights.size(); i++)
		{
			AddLightToGame(m_recordedCutsceneLights[i]);
		}
	}
}

void ReplayLightingManager::AddPhoneLight(CPed* pPed, CEntity* pPhoneLightSource)
{
	PedLightInfo* pInfo = GetPedLightInfo(pPed);
	if(pInfo)
		pInfo->mp_PhoneLightSource = pPhoneLightSource;
}

void ReplayLightingManager::AddPedLight(CPed* pPed)
{
	PedLightInfo* pInfo = GetPedLightInfo(pPed);
	if(pInfo)
		pInfo->m_HasPedLight = true;
}


void ReplayLightingManager::RemovePed(CPed* pPed)
{
	for(u32 i = 0; i < m_pedLightInfo.size(); ++i)
	{
		if(m_pedLightInfo[i].mp_Ped == pPed)
		{
			m_pedLightInfo.Delete(i);
			return;
		}
	}
}


size_t ReplayLightingManager::GetNumberPedLights()
{
	size_t numberOfPedLights = 0;

	for(u32 i = 0; i < m_pedLightInfo.size(); i++)
	{
		if(m_pedLightInfo[i].m_HasPedLight)
		{
			++numberOfPedLights;
		}

		if(m_pedLightInfo[i].mp_PhoneLightSource != NULL)
		{
			++numberOfPedLights;
		}
	}

	return numberOfPedLights;
}

void ReplayLightingManager::RecordCutsceneLight(const lightPacketData& packetData)
{
	//if we've already recorded this then we can skip it
	for( u32 i = 0; i < m_recordedCutsceneLights.size(); i++)
	{
		if( m_recordedCutsceneLights[i].data.m_nameHash == packetData.m_nameHash )
		{
			return;
		}
	}

	//If we've not already found the light then add it to the list
	if(m_recordedCutsceneLights.GetAvailable() > 0)
	{
		sLightPacketRecordData &lightData = m_recordedCutsceneLights.Append();
		lightData.data = packetData;
	}
}

void ReplayLightingManager::AddCutsceneLight(const sLightPacketRecordData &packetData)
{
	if( m_recordedCutsceneLights.size() >= NUM_OF_CUTSCENE_LIGHTS_PER_FRAME )
	{
		replayAssertf(false, "replay cutscene light data not big enough");
		return;
	}

	for( u32 i = 0; i < m_recordedCutsceneLights.size(); i++ )
	{
		if( m_recordedCutsceneLights[i].data.m_nameHash == packetData.data.m_nameHash)
		{
			replayDebugf2("Already added %u", packetData.data.m_nameHash);
			return;
		}
	}

	if(m_recordedCutsceneLights.GetAvailable() > 0)
	{
		sLightPacketRecordData &lightData = m_recordedCutsceneLights.Append();
		lightData = packetData;

		replayDebugf2("Adding new light %u, total is now %u,", packetData.data.m_nameHash, m_recordedCutsceneLights.GetCount());
	}
}


PedLightInfo* ReplayLightingManager::GetPedLightInfo(CPed* pPed)
{
	replayAssertf(pPed, "Can't get light info for NULL ped");

	//if we have a one from before reuse it
	for( u32 i = 0; i < m_pedLightInfo.size(); i++)
	{
		if(m_pedLightInfo[i].mp_Ped == pPed)
		{
			return &m_pedLightInfo[i];
		}
	}

	//otherwise add a new one
	PedLightInfo* pNewPedLightInfo = nullptr;
	if(m_pedLightInfo.GetAvailable() > 0)
	{
		pNewPedLightInfo = &m_pedLightInfo.Append();
		pNewPedLightInfo->mp_Ped = pPed;
	}
	else
	{
		replayDebugf1("Ran out of Ped Light Infos...increase the size of m_pedLightInfo?");
	}

	return pNewPedLightInfo;
}

void ReplayLightingManager::AddPedLightsToGame(const PedLightInfo& pedLightInfo)
{
	if(pedLightInfo.mp_PhoneLightSource)
	{
		pedLightInfo.mp_Ped->RenderLightOnPhone(pedLightInfo.mp_PhoneLightSource);
	}

	if(pedLightInfo.m_HasPedLight)
	{
		pedLightInfo.mp_Ped->RenderPedLight(true);
	}
}

void ReplayLightingManager::AddLightToGame(const sLightPacketRecordData& lightPacketRecordData)
{
	const lightPacketData &lightData = lightPacketRecordData.data;

	//shadow tracking
	Vector3 pos;
	lightData.m_position.Load(pos);
	Vector3 col;
	lightData.m_col.Load(col);
	Vector3 lightDirection;
	lightData.m_lightDirection.Load(lightDirection);
	Vector3 lightTan;
	lightData.m_lightTangent.Load(lightTan);
	Vector4 volumeColIntensity;
	lightData.m_volumeOuterColourAndIntensity.Load(volumeColIntensity);

	CLightSource light(LIGHT_TYPE_NONE, lightData.m_lightFlags, pos, col, lightData.m_intensity, lightData.m_hourFlags);

	light.SetType(lightData.m_lightType);

	// direction
	light.SetDirTangent(lightDirection, lightTan);

	//radius
	light.SetRadius(lightData.m_lightFalloff);

	// set the spot light parameters. this MUST be after Radius is set
	if (lightData.m_lightType==LIGHT_TYPE_SPOT)
	{
		light.SetSpotlight(lightData.m_innerConeAngle, lightData.m_lightConeAngle);
	}

	//Volume
	if(lightData.m_drawVolume)
	{
		light.SetLightVolumeIntensity(lightData.m_volumeIntensity, lightData.m_volumeSizeScale, VECTOR4_TO_VEC4V(volumeColIntensity)); 	
	}

	//exponential fall off
	light.SetFalloffExponent(lightData.m_falloffExp); 

	light.SetShadowBlur(lightData.m_shadowBlur); 

	//Ambient
	if (!lightData.m_pushLightsEarly)
	{
		if(lightData.m_addAmbientLight)
		{
			Lights::CalculateFromAmbient(light); 
		}
	}

	fwUniqueObjId shadowTrackingId = fwIdKeyGenerator::Get(&lightPacketRecordData);
	light.SetShadowTrackingId(shadowTrackingId);
	Lights::AddSceneLight(light, NULL, lightData.m_pushLightsEarly);
}

#endif // GTA_REPLAY
