#ifndef _COLLISIONAUDIOPACKET_H_
#define _COLLISIONAUDIOPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Audio/SoundPacket.h"
#include "phcore/materialmgr.h"

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

struct audVehicleCollisionContext;

//////////////////////////////////////////////////////////////////////////
class CPacketCollisionPlayPacket : public CPacketEventTracked, public CPacketEntityInfoStaticAware<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:

	CPacketCollisionPlayPacket(u32 soundHash, const float volume, const Vector3 position[], u8 index);
	CPacketCollisionPlayPacket(u32 soundHash, const s32 pitch, const float volume, const Vector3 position[], u8 index);

	CPacketCollisionPlayPacket(u32 soundHash, const float volume, const Vector3 position[], phMaterialMgr::Id materialId[], u8 index);
	CPacketCollisionPlayPacket(u32 soundHash, const s32 pitch, const float volume, const Vector3 position[], phMaterialMgr::Id materialId[], u8 index);

	void	Extract(CTrackedEventInfo<tTrackedSoundType>& data) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool	Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	
	{	
		replayDebugf2("Start (collision) tracking %d", GetTrackedID());
		return true;
	}

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_COLLISIONPLAY, "Validation of CPacketCollisionPlayPacket Failed!, %d", GetPacketID());
		replayAssertf(m_MaterialId[0] != 0 || m_MaterialId[1] != 0 || CPacketEntityInfoStaticAware::ValidatePacket(), "Validation of CPacketCollisionPlayPacket Failed!, %d", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && (m_MaterialId[0] != 0 || m_MaterialId[1] != 0 || CPacketEntityInfoStaticAware::ValidatePacket()) && GetPacketID() == PACKETID_SOUND_COLLISIONPLAY;
	}

private:
	enum
	{
		eRoll = 0,
		eScrape,
	};

	u32				m_soundHash;
	
	union
	{
		float			m_clientVar;
		s32				m_pitch;
	};

	float			m_volume;
	CPacketVector3	m_position[2];
	phMaterialMgr::Id m_MaterialId[2];
	u8 m_index;
};


//////////////////////////////////////////////////////////////////////////
class CPacketCollisionUpdatePacket : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	
	CPacketCollisionUpdatePacket(const float volume, const Vector3& position);
	CPacketCollisionUpdatePacket(const s32 pitch, const float volume, const Vector3& position);

	void	Extract(CTrackedEventInfo<tTrackedSoundType>& data) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_COLLISIONUPDATE, "Validation of CPacketCollisionUpdatePacket Failed!, %d", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_COLLISIONUPDATE;
	}

private:
	enum
	{
		eRoll = 0,
		eScrape,
	};

	union
	{
		float			m_clientVar;
		s32				m_pitch;
	};

	float			m_volume;
	CPacketVector3	m_position;
};


//////////////////////////////////////////////////////////////////////////
class CPacketVehiclePedCollisionPacket : public CPacketEvent, public  CPacketEntityInfo<2, CEntityChecker, CEntityCheckerAllowNoEntity>
{
public:

	CPacketVehiclePedCollisionPacket(const Vector3 pos, const audVehicleCollisionContext* impactContext);

	void	Extract(const CEventInfo<CEntity>&) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_VEHICLE_PED_COLLSION, "Validation of CPacketVehiclePedCollisionPacket Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_VEHICLE_PED_COLLSION;
	}

private:

	Vector3 m_pos;
	f32 m_ImpactImpactMag;
	u32 m_ImpactType;
	u16 m_ImpactOtherComponent;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedImpactPacket : public CPacketEvent, public  CPacketEntityInfoStaticAware<2, CEntityChecker, CEntityCheckerAllowNoEntity>
{
public:

	CPacketPedImpactPacket(const Vector3 pos, f32 impactVel, u32 collisionEventOtherComponent, bool isMeleeImpact);

	void	Extract(const CEventInfo<CEntity>&) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_PED_IMPACT_COLLSION, "Validation of CPacketPedImpactPacket Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PED_IMPACT_COLLSION;
	}

private:

	Vector3 m_pos;
	f32 m_ImpactVel;
	u32 m_CollisionEventOtherComponent;
	bool m_IsMeleeImpact;
};

//////////////////////////////////////////////////////////////////////////
class CPacketVehicleSplashPacket : public CPacketEvent, public  CPacketEntityInfo<1, CEntityChecker>
{
public:

	CPacketVehicleSplashPacket(const f32 downSpeed,bool outOfWater);

	void	Extract(const CEventInfo<CVehicle>&) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_VEHICLE_SPLASH, "Validation of CPacketVehicleSplashPacket Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_VEHICLE_SPLASH;
	}

private:

	f32 m_DownSpeed;
	bool m_OutOfWater;	
};


#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // _COLLISIONAUDIOPACKET_H_
