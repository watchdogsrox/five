//=====================================================================================================
// name:		ParticleBloodFxPacket.h
// description:	Blood Fx particle replay packet.
//=====================================================================================================

#ifndef INC_BLOODFXPACKET_H_
#define INC_BLOODFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"
#include "control\replay\packetbasics.h"
#include "vfx\decal\decalsettings.h"

class CPed;
class CObject;
class CVehicle;
struct VfxCollInfo_s;

//=====================================================================================================
class CPacketBloodFallDeathFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketBloodFallDeathFx(u32 uPfxHash);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODFALLDEATHFX, "Validation of CPacketBloodFallDeathFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_BLOODFALLDEATHFX;
	}

private:
	u32				m_pfxHash;
};

//=====================================================================================================
class CPacketBloodSharkBiteFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketBloodSharkBiteFx(u32 uPfxHash, u32 boneIndex);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODSHARKBITEFX, "Validation of CPacketBloodSharkBiteFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_BLOODSHARKBITEFX;
	}

private:
	u32				m_pfxHash;
	u32				m_boneIndex;
};

//=====================================================================================================
class CPacketBloodPigeonShotFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketBloodPigeonShotFx(u32 uPfxHash);

	void			Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODPIGEONFX, "Validation of CPacketBloodPigeonShotFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_BLOODPIGEONFX;
	}

private:
	u32				m_pfxHash;
};

//=====================================================================================================
class CPacketBloodMouthFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketBloodMouthFx(u32 uPfxHash);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODMOUTHFX, "Validation of CPacketBloodMouthFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_BLOODMOUTHFX;
	}

private:
	u32				m_pfxHash;
};

//=====================================================================================================
class CPacketBloodWheelSquashFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketBloodWheelSquashFx(u32 uPfxHash, const Vector3& rPosition);

	void			Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODWHEELFX, "Validation of CPacketBloodWheelSquashFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_BLOODWHEELFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void			StorePosition(const Vector3& rPosition);
	void			LoadPosition(Vector3& rPosition) const;

	//-------------------------------------------------------------------------------------------------
	float			m_Position[3];
	u32				m_pfxHash;
};



//////////////////////////////////////////////////////////////////////////
class CPacketBloodFx : public CPacketEvent, public CPacketEntityInfo<2, CEntityChecker, CEntityCheckerAllowNoEntity>
{
public:
	CPacketBloodFx(const VfxCollInfo_s& vfxCollInfo, bool bodyArmour, bool hOrLArmour, bool isUnderwater, bool isPlayer, s32 fxInfoIndex, float distSqr, bool doFx, bool doDamage, bool isEntry);

	void			Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODFX, "Validation of CPacketBloodFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_BLOODFX;
	}

private:

	s8				m_infoIndex;
	u8				m_weaponGroup;

	bool			m_bodyArmour;
	bool			m_heavyOrLightArmour;
	bool			m_isUnderwater;
	bool			m_isPlayer;

	CPacketVector3	m_position;
	CPacketVector3	m_direction;
	s32				m_componentID;

	float			m_distSqr;

	bool			m_doPtfx;
	bool			m_doDamage;

	bool			m_isEntry;
};

//////////////////////////////////////////////////////////////////////////
class CPacketBloodDecal : public CPacketDecalEvent, public CPacketEntityInfoStaticAware<1>
{
public:
	CPacketBloodDecal(const decalUserSettings& decalSettings, int decalID);

	void			Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	
	{	
		CPacketDecalEvent::PrintXML(handle);	
		CPacketEntityInfoStaticAware::PrintXML(handle);		

		char str[1024];
		snprintf(str, 1024, "\t\t<m_positionWld>%f, %f, %f</m_positionWld>\n", m_positionWld[0], m_positionWld[1], m_positionWld[2]);
		CFileMgr::Write(handle, str, istrlen(str));
	}

	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLOODDECAL, "Validation of CPacketBloodDecal Failed!, 0x%08X", GetPacketID());
		return CPacketDecalEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_BLOODDECAL;
	}

private:

	u16					m_RenderSettingsIndex;
	CPacketVector<3>	m_Side;
	CPacketVector<3>	m_vDimensions;
	float				m_TotalLife;
	float				m_FadeInTime;
	float				m_uvMultStart;
	float				m_uvMultTime;
	u8					m_colR;
	u8					m_colG;
	u8					m_colB;
	float				m_duplicateRejectDist;

	CPacketVector<3>	m_positionWld;
	CPacketVector<3>	m_directionWld;
	s32					m_componentId;
};

//////////////////////////////////////////////////////////////////////////
class CPacketBloodPool : public CPacketDecalEvent, public CPacketEntityInfo<0>
{
public:
	CPacketBloodPool(const decalUserSettings& decalSettings, int decalID);

	void			Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXBLOODPOOL, "Validation of CPacketBloodPool Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PTEXBLOODPOOL;
	}

	void			UpdateMonitorPacket();

private:

	u16					m_RenderSettingsIndex;
	CPacketVector<3>	m_Side;
	CPacketVector<3>	m_vDimensions;
	float				m_TotalLife;
	float				m_FadeInTime;
	float				m_uvMultStart;
	float				m_uvMultEnd;
	float				m_uvMultTime;
	u8					m_colR;
	u8					m_colG;
	u8					m_colB;
	float				m_duplicateRejectDist;

	CPacketVector<3>	m_positionWld;
	CPacketVector<3>	m_directionWld;
	s32					m_componentId;

	//Version 1
	s8					m_LiquidType;
};

#endif // GTA_REPLAY

#endif // INC_BLOODFXPACKET_H_
