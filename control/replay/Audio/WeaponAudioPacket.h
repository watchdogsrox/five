//=====================================================================================================
// name:		WeaponAudioPacket.h
// description:	Weapon audio replay packet.
//=====================================================================================================

#ifndef INC_WEAPONAUDIOPACKET_H_
#define INC_WEAPONAUDIOPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/audio/SoundPacket.h"
#include "audio/weaponaudioentity.h"

#include "audiosoundtypes/soundcontrol.h"
#include "weapons/WeaponEnums.h"

//#define DEBUG_REPLAY_PFX		(0)
struct audWeaponFireInfo;

//////////////////////////////////////////////////////////////////////////
class CPacketWeaponSoundFireEvent : public CPacketEvent, public CPacketEntityInfo<2, CEntityChecker, CEntityCheckerAllowNoEntity>
{
public:
	CPacketWeaponSoundFireEvent(const audWeaponFireInfo& info, f32 fgunFightVolumeOffset, u32 weaponHash, int seatIdx);

	void	Extract(CEventInfo<CEntity>& pData) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const;
	void	PreplayCore(const CEventInfo<CEntity>&pData, const audWeaponFireInfo::PresuckInfo &presuckInfo) const;
	u32				GetPreplayTime(const CEventInfo<CEntity>&) const;


	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_FIRE_EVENT, "Validation of CPacketWeaponSoundFireEvent Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_FIRE_EVENT;
	}
	
private:
	CWeapon* GetVehicleWeapon(CVehicle* pVehicle, int seatIdx) const;

	u32					m_settingsHash;
	bool				m_isSuppressed;
	f32					m_gunFightVolumeOffset;
	s32					m_ammo;
	u32					m_weaponHash;
	int					m_seatIdx;
	mutable audScene	*m_PresuckScene;
	mutable bool		m_HasDonePreplay;

	//Version 1
	CPacketVector3		m_explicitPosition;
};

//////////////////////////////////////////////////////////////////////////
class CPacketWeaponSoundFireEventOld : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponSoundFireEventOld(const audWeaponFireInfo& info, f32 fgunFightVolumeOffset, u32 weaponHash, int seatIdx);

	void	Extract(CEventInfo<CEntity>& pData) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const;
	u32				GetPreplayTime(const CEventInfo<CEntity>&) const;


	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_FIRE_EVENT_OLD, "Validation of CPacketWeaponSoundFireEventOld Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_FIRE_EVENT_OLD;
	}
	
private:
	CWeapon* GetVehicleWeapon(CVehicle* pVehicle, int seatIdx) const;

	u32					m_settingsHash;
	bool				m_isSuppressed;
	f32					m_gunFightVolumeOffset;
	s32					m_ammo;

	//Version 1
	u32					m_weaponHash;
	int					m_seatIdx;
	mutable audScene	*m_PresuckScene;
	mutable bool		m_HasDonePreplay;
};



//////////////////////////////////////////////////////////////////////////
class CPacketWeaponPersistant : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponPersistant(const audWeaponFireInfo& info, eWeaponEffectGroup weaponEffectGroup, f32 fgunFightVolumeOffset, bool forceSingleShot, u32 lastPlayerHitTime, audSimpleSmoother hitPlayerSmoother);

	void	Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldStartTracking() const						{	return true;	}
	bool	Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))
	{	
		replayDebugf2("Start (weapon) tracking %d", GetTrackedID());
		return true;
	}

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_PERSIST, "Validation of CPacketWeaponPersistant Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_PERSIST;
	}

private:
	u32					m_settingsHash;
	bool				m_isSuppressed;
	eWeaponEffectGroup	m_weaponEffectGroup;
	f32					m_gunFightVolumeOffset;
	bool				m_forceSingleShot;
	u32					m_lastPlayerHitTime;
	audSimpleSmoother   m_hitPlayerSmoother;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponPersistantStop : public CPacketEventTracked, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketWeaponPersistantStop(const audWeaponFireInfo& info);

	void	Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldEndTracking() const		{	return true;	}

	bool	Setup(bool tracked)
	{
		replayDebugf2("Stop (weapon) tracking %d", GetTrackedID());
		if(!tracked)
		{
			replayDebugf2("Sound not started so cancelling stop");
			Cancel();
		}
		else
		{
			replayDebugf2("Stop is fine");
		}
		return false;
	}

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_PERSIST_STOP, "Validation of CPacketWeaponPersistantStop Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_PERSIST_STOP;
	}

private:
	u32					m_settingsHash;
	bool				m_IsSuppressed;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponSpinUp : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponSpinUp(bool skipSpinUp, s32 bneIndex, bool forceNPCVersion);

	void	Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldStartTracking() const						{	return true;	}
	bool	Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	
	{	
		replayDebugf1("Start (spinup) tracking %d", GetTrackedID());
		return true;
	}

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_SPINUP, "Validation of CPacketWeaponSpinUp Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_SPINUP;
	}

	bool m_SkipSpinUp;
	s32 m_BoneIndex;
	bool m_ForceNPCVersion;
};

//////////////////////////////////////////////////////////////////////////
class CPacketWeaponAutoFireStop : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponAutoFireStop(bool stop);

	void Extract(CEventInfo<CEntity>& pData) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_AUTO_FIRE_STOP, "Validation of CPacketWeaponAutoFireStop Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_AUTO_FIRE_STOP;
	}

private:

	bool m_Stop;
};


//////////////////////////////////////////////////////////////////////////
class CPacketBulletImpact : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketBulletImpact(const u32 hash, const u32 slowMoSoundHash, const Vector3 &position, const f32 volume);

	void	Extract(CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const	{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_BULLETIMPACT, "Validation of CPacketBulletImpact Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_BULLETIMPACT;
	}

private:

	CPacketVector3		m_Position;
	float				m_Volume;
	u32					m_SoundHash;
	u32					m_SlowMoSoundHash;
};

//////////////////////////////////////////////////////////////////////////
class CPacketBulletImpactNew : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketBulletImpactNew(const audSoundSetFieldRef soundRef, const audSoundSetFieldRef slowMoSoundRef, const Vector3 &position, const f32 volume);

	void	Extract(CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const	{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_BULLETIMPACT_NEW, "Validation of CPacketBulletImpactNew Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_BULLETIMPACT_NEW;
	}

private:

	CPacketVector3		m_Position;
	float				m_Volume;
	u64					m_SoundSetFieldRef;
	u64					m_SlowMoSoundsetFieldRef;
};

//////////////////////////////////////////////////////////////////////////
class CPacketThermalScopeAudio : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketThermalScopeAudio(const bool activate);

	void	Extract(CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const	{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_THERMAL_SCOPE, "Validation of CPacketThermalScopeAudio Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_THERMAL_SCOPE;
	}

private:

	bool	m_Activate;
};


//////////////////////////////////////////////////////////////////////////
class CPacketSoundPlayerWeaponReport : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:
	CPacketSoundPlayerWeaponReport(audSoundInitParams& initParams, u32 soundHash, f32* fVolume, s32* sPredelay, Vector3* vPos, const Vec3V& firePos, float fDist, f32 interiorVolumeScaling);

	bool	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_PLAYERWEAPONREPORT, "Validation of CPacketSoundPlayerWeaponReport Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PLAYERWEAPONREPORT;
	}

private:

	audSoundInitParams	m_initParams;
	u32					m_soundHash;
	f32					m_fVolume[4];
	s32					m_sPredelay[4];
	CPacketVector3		m_vPos[4];
	CPacketVector3		m_firePos;
	float				m_dist;
	f32					m_interiorVolumeScaling;
};


//////////////////////////////////////////////////////////////////////////
class CPacketSndLoadWeaponData : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSndLoadWeaponData(u32 weaponHash, u32 audioHash, u32 previousHash);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	//The expiry relies on REPLAY_MONITOR_PACKET_JUST_ONE removing the previous
	bool				HasExpired(const CEventInfoBase&) const { return false; }
	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SND_LOADWEAPONDATA, "Validation of CPacketSndLoadWeaponData Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_LOADWEAPONDATA;
	}

private:
	u32					m_weaponHash;
	u32					m_audioHash;
	u32					m_previousHash;
};


//////////////////////////////////////////////////////////////////////////
class CPacketSoundNitrousActive : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketSoundNitrousActive(bool active);

	void Extract(CEventInfo<CVehicle>& pData) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_VEHICLE_NITROUS_ACTIVE, "Validation of CPacketSoundNitrousActive Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_VEHICLE_NITROUS_ACTIVE;
	}

private:

	bool m_Active;
};


#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
