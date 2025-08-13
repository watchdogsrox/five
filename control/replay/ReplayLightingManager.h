#ifndef REPLAY_LIGHTING_MANAGER_H
#define REPLAY_LIGHTING_MANAGER_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/LightPacket.h"

struct PedLightInfo
{
	PedLightInfo()
		: mp_Ped(NULL)
		, mp_PhoneLightSource(NULL)
		, m_HasPedLight(false)
	{}


	CPed* mp_Ped;
	CEntity* mp_PhoneLightSource;
	bool m_HasPedLight;
};

const static u32 NUM_OF_PED_LIGHTS_PER_FRAME = 32;
const static u32 NUM_OF_CUTSCENE_LIGHTS_PER_FRAME = 32;

class ReplayLightingManager 
{
public:
	//general light functions
	static void Reset();
	static void RecordPackets(CReplayRecorder &recorder);
	static void AddLightsToScene();

	//ped lights
	static void AddPhoneLight(CPed* pPed, CEntity* pPhoneLightSource);
	static void AddPedLight(CPed* pPed);
	static void RemovePed(CPed* pPed);

	static size_t GetNumberPedLights();

	//cut scene lights
	static void RecordCutsceneLight(const lightPacketData& packetData);

	static void AddCutsceneLight(const sLightPacketRecordData &packetData);

	static const sLightPacketRecordData& GetCutsceneLightData(u32 index) { return m_recordedCutsceneLights[index];}
	static size_t GetNumberCutsceneLights() { return m_recordedCutsceneLights.GetCount(); }

private:
	static PedLightInfo* GetPedLightInfo(CPed* pPed);
	static void AddPedLightsToGame(const PedLightInfo& pedLightInfo);
	static void AddLightToGame(const sLightPacketRecordData& lightPacketRecordData);

	static atFixedArray<PedLightInfo, NUM_OF_PED_LIGHTS_PER_FRAME> m_pedLightInfo;
	static atFixedArray<sLightPacketRecordData, NUM_OF_CUTSCENE_LIGHTS_PER_FRAME> m_recordedCutsceneLights;

};

#endif // GTA_REPLAY

#endif // REPLAY_LIGHTING_MANAGER_H