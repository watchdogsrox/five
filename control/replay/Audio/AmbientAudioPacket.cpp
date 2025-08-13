//=====================================================================================================CPacketPortalOcclusionOverrides
// name:		AmbientAudioPacket.cpp
//=====================================================================================================

#include "AmbientAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"

//////////////////////////////////////////////////////////////////////////
CPacketPortalOcclusionOverrides::CPacketPortalOcclusionOverrides()
	: CPacketBase(PACKETID_PORTAL_OCCLUSION_OVERRIDES, sizeof(CPacketPortalOcclusionOverrides))
{
	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketPortalOcclusionOverrides)));
}

//////////////////////////////////////////////////////////////////////////
void CPacketPortalOcclusionOverrides::Extract() const
{	
	const u32* portalOverrides =  (const u32*)GetIDsPtr();

	for(u32 loop = 0; loop < m_portalOverridesCount; loop++)
	{
		u32 original = portalOverrides[loop * 2];
		u32 override = portalOverrides[(loop * 2) + 1];
		naOcclusionPortalInfo::SetReplayPortalSettingsOverride(original, override);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPortalOcclusionOverrides::Store(const atMap<u32, u32>& map)
{
	PACKET_EXTENSION_RESET(CPacketPortalOcclusionOverrides);
	CPacketBase::Store(PACKETID_PORTAL_OCCLUSION_OVERRIDES, sizeof(CPacketPortalOcclusionOverrides));	

	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketPortalOcclusionOverrides)));

	u32 numElements = 0;
	u8* p = GetIDsPtr();

	atMap<u32, u32>::ConstIterator it = map.CreateIterator();	
	while(!it.AtEnd())
	{
		u32 original = it.GetKey();
		u32 override = it.GetData();
		sysMemCpy(p, &original, sizeof(u32)); p += sizeof(u32);
		sysMemCpy(p, &override, sizeof(u32)); p += sizeof(u32);
		numElements++;
		it.Next();
	}

	m_portalOverridesCount = numElements;
	AddToPacketSize(sizeof(u32) * 2 * numElements);
}


//////////////////////////////////////////////////////////////////////////
CPacketPerPortalOcclusionOverrides::CPacketPerPortalOcclusionOverrides()
	: CPacketBase(PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES, sizeof(CPacketPerPortalOcclusionOverrides))
{
	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketPerPortalOcclusionOverrides)));
}

//////////////////////////////////////////////////////////////////////////
void CPacketPerPortalOcclusionOverrides::Extract() const
{	
	const u32* portalOverrides = (const u32*)GetIDsPtr();

	for(u32 loop = 0; loop < m_portalOverridesCount; loop++)
	{
		u32 interiorNameHash = portalOverrides[loop * 4];
		s32 roomIndex = (s32)portalOverrides[(loop * 4) + 1];
		s32 portalIndex = (s32)portalOverrides[(loop * 4) + 2];
		u32 portalSettingsOverride = portalOverrides[(loop * 4) + 3];
		naOcclusionPortalInfo::SetReplayPerPortalSettingsOverride(interiorNameHash, roomIndex, portalIndex, portalSettingsOverride);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPerPortalOcclusionOverrides::Store(const atMap<u32, naOcclusionPerPortalSettingsOverrideList>& map)
{
	PACKET_EXTENSION_RESET(CPacketPerPortalOcclusionOverrides);
	CPacketBase::Store(PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES, sizeof(CPacketPerPortalOcclusionOverrides));	

	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketPerPortalOcclusionOverrides)));

	u32 numElements = 0;
	u8* p = GetIDsPtr();

	atMap<u32, naOcclusionPerPortalSettingsOverrideList>::ConstIterator it = map.CreateIterator();	
	while(!it.AtEnd())
	{
		const u32 interiorNameHash = it.GetKey();

		for(const naOcclusionPerPortalSettingsOverride& perPortalSettings : it.GetData())
		{
			const u32 roomIndex = (u32)perPortalSettings.roomIndex;
			const u32 portalIndex = (u32)perPortalSettings.portalIndex;
			const u32 overridenSettings = perPortalSettings.overridenSettingsHash;
			sysMemCpy(p, &interiorNameHash, sizeof(u32)); p += sizeof(u32);
			sysMemCpy(p, &roomIndex, sizeof(s32)); p += sizeof(u32);
			sysMemCpy(p, &portalIndex, sizeof(s32)); p += sizeof(u32);
			sysMemCpy(p, &overridenSettings, sizeof(u32)); p += sizeof(u32);
			numElements++;
		}		
		
		it.Next();
	}

	m_portalOverridesCount = numElements;
	AddToPacketSize(sizeof(u32) * 4 * numElements);
}

//////////////////////////////////////////////////////////////////////////
CPacketModifiedAmbientZoneStates::CPacketModifiedAmbientZoneStates()
	: CPacketBase(PACKETID_MODIFIED_AMBIENT_ZONE_STATES, sizeof(CPacketModifiedAmbientZoneStates))
{
	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketModifiedAmbientZoneStates)));
}

//////////////////////////////////////////////////////////////////////////
void CPacketModifiedAmbientZoneStates::Extract() const
{
	g_AmbientAudioEntity.ClearAmbientZoneReplayStates();

	const audAmbientZoneReplayState* enabledStates =  (const audAmbientZoneReplayState*)GetIDsPtr();

	for(u32 loop = 0; loop < m_ambientZoneStateCount; loop++)
	{
		g_AmbientAudioEntity.AddAmbientZoneReplayState(enabledStates[loop]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketModifiedAmbientZoneStates::Store(const atArray<audAmbientZoneReplayState>& ambientZoneStates)
{
	PACKET_EXTENSION_RESET(CPacketModifiedAmbientZoneStates);
	CPacketBase::Store(PACKETID_MODIFIED_AMBIENT_ZONE_STATES, sizeof(CPacketModifiedAmbientZoneStates));

	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketModifiedAmbientZoneStates)));

	u8* p = GetIDsPtr();
	sysMemCpy(p, ambientZoneStates.GetElements(), sizeof(audAmbientZoneReplayState) * ambientZoneStates.GetCount());

	m_ambientZoneStateCount = ambientZoneStates.GetCount();

	AddToPacketSize(sizeof(audAmbientZoneReplayState) * ambientZoneStates.GetCount());
}

//////////////////////////////////////////////////////////////////////////
CPacketActiveAlarms::CPacketActiveAlarms()
	: CPacketBase(PACKETID_ACTIVE_ALARMS, sizeof(CPacketActiveAlarms))
{	
	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketActiveAlarms)));	
}

//////////////////////////////////////////////////////////////////////////
void CPacketActiveAlarms::Extract() const
{	
	g_AmbientAudioEntity.ClearReplayActiveAlarms();

	const audReplayAlarmState* activeAlarms = (const audReplayAlarmState*)GetIDsPtr();

	for(u32 loop = 0; loop < m_activeAlarmCount; loop++)
	{
		g_AmbientAudioEntity.AddReplayActiveAlarm(activeAlarms[loop]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketActiveAlarms::Store(const atArray<audReplayAlarmState>& replayAlarmStates)
{
	PACKET_EXTENSION_RESET(CPacketActiveAlarms);
	CPacketBase::Store(PACKETID_ACTIVE_ALARMS, sizeof(CPacketActiveAlarms));
	
	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketActiveAlarms)));

	u8* p = GetIDsPtr();
	sysMemCpy(p, replayAlarmStates.GetElements(), sizeof(audReplayAlarmState) * replayAlarmStates.GetCount());

	m_activeAlarmCount = replayAlarmStates.GetCount();

	AddToPacketSize(sizeof(audReplayAlarmState) * replayAlarmStates.GetCount());
}

//////////////////////////////////////////////////////////////////////////
CPacketAudioPedScenarios::CPacketAudioPedScenarios()
	: CPacketBase(PACKETID_AUD_PED_SCENARIOS, sizeof(CPacketAudioPedScenarios))
{	
	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketAudioPedScenarios)));	
}

//////////////////////////////////////////////////////////////////////////
void CPacketAudioPedScenarios::Extract() const
{	
	g_PedScenarioManager.ClearReplayActiveScenarios();

	const audReplayScenarioState* activeScenarios = (const audReplayScenarioState*)GetIDsPtr();

	for(u32 loop = 0; loop < m_activeScenarioCount; loop++)
	{
 		g_PedScenarioManager.AddReplayActiveScenario(activeScenarios[loop]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketAudioPedScenarios::Store(const atArray<audReplayScenarioState>& replayScenarioStates)
{
	PACKET_EXTENSION_RESET(CPacketAudioPedScenarios);
	CPacketBase::Store(PACKETID_AUD_PED_SCENARIOS, sizeof(CPacketAudioPedScenarios));

	u8 temp = 0;
	SetPadU8(PacketSizeU8, Assign(temp, sizeof(CPacketAudioPedScenarios)));

	u8* p = GetIDsPtr();
	sysMemCpy(p, replayScenarioStates.GetElements(), sizeof(audReplayScenarioState) * replayScenarioStates.GetCount());

	m_activeScenarioCount = replayScenarioStates.GetCount();

	AddToPacketSize(sizeof(audReplayScenarioState) * replayScenarioStates.GetCount());
}

//////////////////////////////////////////////////////////////////////////
CPacketOverrideUnderWaterStream::CPacketOverrideUnderWaterStream(const u32 hashName, const bool override)
	: CPacketEvent(PACKETID_SET_OVERRIDE_UNDERWATER_STREAM, sizeof(CPacketOverrideUnderWaterStream))
	, CPacketEntityInfo()
	, m_UnderwaterStreamHash (hashName)
	, m_Override(override)
{
		
}

void CPacketOverrideUnderWaterStream::Extract(const CEventInfo<void>&) const
{
	g_AmbientAudioEntity.OverrideUnderWaterStream(m_UnderwaterStreamHash,m_Override);
}


//////////////////////////////////////////////////////////////////////////
CPacketSetUnderWaterStreamVariable::CPacketSetUnderWaterStreamVariable(u32 varNameHash, f32 varValue)
	: CPacketEvent(PACKETID_SET_UNDERWATER_STREAM_VARIABLE, sizeof(CPacketSetUnderWaterStreamVariable))
	, CPacketEntityInfo()
	, m_VariableNameHash(varNameHash)
	, m_VariableValue(varValue)
{
}

void CPacketSetUnderWaterStreamVariable::Extract(const CEventInfo<void>&) const
{
	g_AmbientAudioEntity.SetVariableOnUnderWaterStream(m_VariableNameHash,m_VariableValue);
}

#endif