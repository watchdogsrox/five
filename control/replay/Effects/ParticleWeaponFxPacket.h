//=====================================================================================================
// name:		ParticleWeaponFxPacket.h
// description:	Weapon Fx particle replay packet.
//=====================================================================================================

#ifndef INC_WEAPONFXPACKET_H_
#define INC_WEAPONFXPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Effects/ParticlePacket.h"
#include "control/replay/PacketBasics.h"
#include "vector/matrix34.h"

namespace rage
{class ptxEffectInst;}
class CEntity;
class CObject;


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponProjTrailFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponProjTrailFx(u32 uPfxHash, int ptfxOffset, s32 trailBoneID, float trailEvo, Vec3V_In colour);

	void				Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONPROJTRAILFX, "Validation of CPacketWeaponProjTrailFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONPROJTRAILFX;
	}

private:

	u32					m_pfxHash;
	int					m_ptfxOffset;
	s32					m_trailBoneID;
	float				m_trailEvo;
	CPacketVector<3>	m_Colour;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponBulletImpactFx : public CPacketEvent, public CPacketEntityInfoStaticAware<1,CEntityCheckerAllowNoEntity>
{
public:
	CPacketWeaponBulletImpactFx(u32 pfxHash, Vec3V_In rPos, Vec3V_In rNormal, float lodScale, float scale);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult				Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	// Overriding these base functions

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONBULLETIMPACTFX, "Validation of CPacketWeaponBulletImpactFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_WEAPONBULLETIMPACTFX;
	}

private:

	u32					m_pfxHash;
	CPacketVector<3>	m_Position;
	CPacketVector3Comp<valComp<s8, s8_f32_n1_to_1>>	m_Normal;
	float				m_lodScale;
	float				m_scale;
};



//////////////////////////////////////////////////////////////////////////
class CPacketWeaponMuzzleFlashFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponMuzzleFlashFx(u32 pfxHash, s32 muzzleBoneIndex, f32 muzzleFlashScale, Vec3V_In muzzleOffset);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult				Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONMUZZLEFLASHFX, "Validation of CPacketWeaponMuzzleFlashFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONMUZZLEFLASHFX;
	}

private:

	u32					m_pfxHash;
	s32					m_muzzleBoneIndex;
	f32					m_muzzleFlashScale;
	CPacketVector<3>	m_muzzleOffset;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponMuzzleSmokeFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponMuzzleSmokeFx(u32 pfxHash, s32 muzzleBoneIndex, float smokeLevel, u32 weaponOffset);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult				Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONMUZZLESMOKEFX, "Validation of CPacketWeaponMuzzleSmokeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONMUZZLESMOKEFX;
	}

private:

	u32					m_pfxHash;
	s32					m_muzzleBoneIndex;
	float				m_smokeLevel;
	u32					m_weaponOffset;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponGunShellFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponGunShellFx(u32 pfxHash, s32 weaponIndex, s32 gunShellBoneIndex);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONGUNSHELLFX, "Validation of CPacketWeaponGunShellFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONGUNSHELLFX;
	}

private:

	u32					m_pfxHash;
	s32					m_weaponIndex;
	s32					m_gunShellBoneIndex;
};

//////////////////////////////////////////////////////////////////////////
class CPacketWeaponBulletTraceFx : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketWeaponBulletTraceFx(u32 uPfxHash, const Vector3& startPos, const Vector3& traceDir, float traceLength);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONBULLETTRACEFX, "Validation of CPacketWeaponBulletTraceFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONBULLETTRACEFX;
	}

private:

	u32					m_pfxHash;
	CPacketVector3Comp<valComp<s8, s8_f32_n1_to_1>>	m_traceDir;
	CPacketVector<3>	m_startPos;
	float				m_traceLen;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponBulletTraceFx2 : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponBulletTraceFx2(u32 uPfxHash, const Vector3& traceDir, float traceLength, const Vec3V& colour);

	void				Extract(const CEventInfo<CObject>&) const;
	ePreloadResult		Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONBULLETTRACEFX2, "Validation of CPacketWeaponBulletTraceFx2 Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONBULLETTRACEFX2;
	}

private:

	u32					m_pfxHash;
	CPacketVector<3>	m_traceDir;
	float				m_traceLen;

	CPacketVector<3>	m_colour;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponExplosionWaterFx : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketWeaponExplosionWaterFx(u32 uPfxHash, const Vector3& rPos, float fDepth, float fRiverEvo);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult				Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONEXPLOSIONWATERFX, "Validation of CPacketWeaponExplosionWaterFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONEXPLOSIONWATERFX;
	}

protected:

	u32					m_pfxHash;
	CPacketVector3		m_Position;
	float				m_depth;
	float				m_riverEvo;
};



//////////////////////////////////////////////////////////////////////////
class CPacketWeaponExplosionFx_Old : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketWeaponExplosionFx_Old(u32 uPfxHash, const Matrix34& rMatrix, s32 boneIndex, float fScale);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult				Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				Print() const;
	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONEXPLOSIONFX_OLD, "Validation of CPacketWeaponExplosionFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONEXPLOSIONFX_OLD;
	}

private:

	u32					m_pfxHash;
	CPacketPositionAndQuaternion m_posAndQuat;
	s32					m_boneIndex;
	float				m_scale;
};

//////////////////////////////////////////////////////////////////////////
class CPacketWeaponExplosionFx : public CPacketEvent, public CPacketEntityInfoStaticAware<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketWeaponExplosionFx(u32 uPfxHash, const Matrix34& rMatrix, s32 boneIndex, float fScale);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult				Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				Print() const;
	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONEXPLOSIONFX, "Validation of CPacketWeaponExplosionFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_WEAPONEXPLOSIONFX;
	}

private:

	u32					m_pfxHash;
	CPacketPositionAndQuaternion m_posAndQuat;
	s32					m_boneIndex;
	float				m_scale;
};

//////////////////////////////////////////////////////////////////////////
class CPacketVolumetricFx : public CPacketEvent, public CPacketEntityInfoStaticAware<1,CEntityCheckerAllowNoEntity>
{
public:
	CPacketVolumetricFx(u32 pfxHash, s32 muzzleBoneIndex, u32 weaponOffset);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	// Overriding these base functions

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONVOLUMETRICFX, "Validation of CPacketVolumetricFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_WEAPONVOLUMETRICFX;
	}

private:

	u32					m_pfxHash;
	u32					m_weaponOffset;
	s32					m_muzzleBoneIndex;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponFlashLight : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWeaponFlashLight(bool active, u8 lod);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	// Overriding these base functions

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPONFLASHLIGHT, "Validation of CPacketWeaponFlashLight Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPONFLASHLIGHT;
	}

private:

	u8 m_lod;
	bool m_Active : 1;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWeaponThermalVision : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketWeaponThermalVision(bool active);

	void				Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const						{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const						{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfoBase&) const;

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WEAPON_THERMAL_VISION, "Validation of CPacketWeaponThermalVision Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WEAPON_THERMAL_VISION;
	}
};

#endif // GTA_REPLAY
#endif // INC_WEAPONFXPACKET_H_
