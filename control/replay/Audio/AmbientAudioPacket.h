//=====================================================================================================
// name:		AmbientAudioPacket.h
// description:	Ambient audio replay packet.
//=====================================================================================================

#ifndef INC_AMBIENTAUDIOPACKET_H_
#define INC_AMBIENTAUDIOPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/audio/SoundPacket.h"
#include "audio/scriptaudioentity.h"
#include "audio/ambience/ambientaudioentity.h"
#include "audio/pedscenariomanager.h"
#include "Control/Replay/PacketBasics.h"

const u8 PacketSizeU8 = 0;
//////////////////////////////////////////////////////////////////////////
class CPacketPortalOcclusionOverrides : public CPacketBase
{
public:
	CPacketPortalOcclusionOverrides();
	
	void				Extract() const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	void				Store(const atMap<u32, u32>& map);
	void				StoreExtensions(const atMap<u32, u32>& map) { PACKET_EXTENSION_RESET(CPacketPortalOcclusionOverrides); (void)map; }
	
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PORTAL_OCCLUSION_OVERRIDES, "Validation of CPacketPortalOcclusionOverrides Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_PORTAL_OCCLUSION_OVERRIDES;
	}

private:
	u8*	GetIDsPtr()
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);
	}
	const u8*	GetIDsPtr() const
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);
	}
private:	
	u32 m_portalOverridesCount;
	DECLARE_PACKET_EXTENSION(CPacketPortalOcclusionOverrides);
};

//////////////////////////////////////////////////////////////////////////
class CPacketPerPortalOcclusionOverrides : public CPacketBase
{
public:
	CPacketPerPortalOcclusionOverrides();

	void				Extract() const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	void				Store(const atMap<u32, naOcclusionPerPortalSettingsOverrideList>& map);
	void				StoreExtensions(const atMap<u32, naOcclusionPerPortalSettingsOverrideList>& map) { PACKET_EXTENSION_RESET(CPacketPerPortalOcclusionOverrides); (void)map; }

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES, "Validation of CPacketPerPortalOcclusionOverrides Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES;
	}

private:
	u8*	GetIDsPtr()
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);
	}
	const u8*	GetIDsPtr() const
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);
	}
private:	
	u32 m_portalOverridesCount;
	DECLARE_PACKET_EXTENSION(CPacketPerPortalOcclusionOverrides);
};

//////////////////////////////////////////////////////////////////////////
class CPacketModifiedAmbientZoneStates : public CPacketBase
{
public:
	CPacketModifiedAmbientZoneStates();

	void				Extract() const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	void				Store(const atArray<audAmbientZoneReplayState>& ambientZoneStates);
	void				StoreExtensions(const atArray<audAmbientZoneReplayState>& ambientZoneStates) { PACKET_EXTENSION_RESET(CPacketModifiedAmbientZoneStates); (void)ambientZoneStates; }	

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MODIFIED_AMBIENT_ZONE_STATES, "Validation of CPacketModifiedAmbientZoneStates Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_MODIFIED_AMBIENT_ZONE_STATES;
	}

private:
	u8*	GetIDsPtr()
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);
	}
	const u8*	GetIDsPtr() const
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);
	}

private:	
	u32 m_ambientZoneStateCount;
	DECLARE_PACKET_EXTENSION(CPacketModifiedAmbientZoneStates);
};

//////////////////////////////////////////////////////////////////////////
class CPacketActiveAlarms : public CPacketBase
{
public:
	CPacketActiveAlarms();

	void				Extract() const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	void				Store(const atArray<audReplayAlarmState>& replayAlarmStates);
	void				StoreExtensions(const atArray<audReplayAlarmState>& replayAlarmStates) { PACKET_EXTENSION_RESET(CPacketActiveAlarms); (void)replayAlarmStates; }	

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ACTIVE_ALARMS, "Validation of CPacketActiveAlarms Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_ACTIVE_ALARMS;
	}

private:
	u8*	GetIDsPtr()
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);		
	}
	const u8*	GetIDsPtr() const
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);		
	}

private:	
	u32 m_activeAlarmCount;
	DECLARE_PACKET_EXTENSION(CPacketActiveAlarms);
};

//////////////////////////////////////////////////////////////////////////
class CPacketAudioPedScenarios : public CPacketBase
{
public:
	CPacketAudioPedScenarios();

	void				Extract() const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	void				Store(const atArray<audReplayScenarioState>& activeScenarios);
	void				StoreExtensions(const atArray<audReplayScenarioState>& activeScenarios) { PACKET_EXTENSION_RESET(CPacketAudioPedScenarios); (void)activeScenarios; }	

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_AUD_PED_SCENARIOS, "Validation of CPacketAudioPedScenarios Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_AUD_PED_SCENARIOS;
	}

private:
	u8*	GetIDsPtr()
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);		
	}
	const u8*	GetIDsPtr() const
	{	
		return (u8*)this + GetPadU8(PacketSizeU8);		
	}

private:	
	u32 m_activeScenarioCount;
	DECLARE_PACKET_EXTENSION(CPacketAudioPedScenarios);
};

//////////////////////////////////////////////////////////////////////////
class CPacketOverrideUnderWaterStream : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketOverrideUnderWaterStream(const u32 hashName, const bool override);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const					{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfoBase&) const     { return true; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SET_OVERRIDE_UNDERWATER_STREAM, "Validation of CPacketUnderwaterAudio Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SET_OVERRIDE_UNDERWATER_STREAM;
	}

private:
	u32 m_UnderwaterStreamHash;
	bool m_Override;
};

//////////////////////////////////////////////////////////////////////////
class CPacketSetUnderWaterStreamVariable : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CPacketSetUnderWaterStreamVariable(u32 varNameHash, f32 varValue);

	void			Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const								{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const								{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SET_UNDERWATER_STREAM_VARIABLE, "Validation of CPacketSetUnderWaterStreamVariable Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SET_UNDERWATER_STREAM_VARIABLE;
	}

protected:	
	f32 m_VariableValue;
	u32 m_VariableNameHash;
};
#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
