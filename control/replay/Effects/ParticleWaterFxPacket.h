//=====================================================================================================
// name:		ParticleWaterFxPacket.h
// description:	Water Fx particle replay packet.
//=====================================================================================================

#ifndef INC_WATERFXPACKET_H_
#define INC_WATERFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\PacketBasics.h"
#include "control\replay\Effects\ParticlePacket.h"

namespace rage
{ class ptxEffectInst; }
class CEntity;
class CBoat;
class CVehicle;
class CPed;



//////////////////////////////////////////////////////////////////////////
class CPacketWaterSplashHeliFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashHeliFx(Vec3V_In rPos);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHHELIFX, "Validation of CPacketWaterSplashHeliFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHHELIFX;
	}

private:
	
	CPacketVector<3>	m_position;
};


//=====================================================================================================
class CPacketWaterSplashGenericFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashGenericFx(u32 uPfxHash, const Vector3& rPos, float fSpeed);
		
	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHGENERICFX, "Validation of CPacketWaterSplashGenericFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHGENERICFX;
	}

private:
	
	u32		m_pfxHash;
	CPacketVector3 m_position;
	float	m_fCustom;
};

//=====================================================================================================
class CPacketWaterSplashVehicleFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashVehicleFx(u32 uPfxHash, const Matrix34& mMat, float fSizeEvo, float fLateralSpeedEvo, float fDownwardSpeedEvo);
	
	void				Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHVEHICLEFX, "Validation of CPacketWaterSplashVehicleFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHVEHICLEFX;
	}

private:

	CPacketPositionAndQuaternion m_Matrix;
	u32 m_HashName;
	float m_SizeEvo;
	float m_LateralSpeedEvo;
	float m_DownwardSpeedEvo;
};

//=====================================================================================================
class CPacketWaterSplashVehicleTrailFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashVehicleTrailFx(u32 uPfxHash, s32 uPtfxOffset, const Matrix34& mMat, float fSizeEvo, float fSpeedEvo);

	void				Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHVEHICLETRAILFX, "Validation of CPacketWaterSplashVehicleTrailFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHVEHICLETRAILFX;
	}

private:	

	CPacketPositionAndQuaternion m_Matrix;
	u32 m_HashName;
	s32 m_PtfxOffset;
	float m_SizeEvo;
	float m_SpeedEvo;
};

//=====================================================================================================
class CPacketWaterSplashPedFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashPedFx(u32 uPfxHash, float fSpeed, Vec3V waterPos, bool isPlayerVfx);

	void				Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHPEDFX, "Validation of CPacketWaterSplashPedFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHPEDFX;
	}

private:
	
	u32					m_pfxHash;
	float				m_speedEvo;
	CPacketVector<3>	m_waterPos;
	bool				m_isPlayerVfx;
};

//////////////////////////////////////////////////////////////////////////
class CPacketWaterSplashPedInFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashPedInFx(Vec3V_In surfacePos, Vec3V_In boneDir, float boneSpeed, s32 boneID, bool isPlayerVfx);
	void				Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHPEDINFX, "Validation of CPacketWaterSplashPedInFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHPEDINFX;
	}

private:

	CPacketVector<3>	m_surfacePos;
	CPacketVector<3>	m_boneDir;
	float				m_boneSpeed;
	s32					m_skeletonBoneID	: 31;
	s32					m_isPlayerVfx		: 1;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWaterSplashPedOutFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashPedOutFx(Vec3V_In boneDir, s32 boneIndex, float boneSpeed, s32 boneID);
	void				Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHPEDOUTFX, "Validation of CPacketWaterSplashPedOutFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHPEDOUTFX;
	}

private:
	
	CPacketVector<3>	m_boneDir;

	s32					m_boneIndex;
	float				m_boneSpeed;
	s32					m_skeletonBoneID;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWaterSplashPedWadeFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashPedWadeFx(Vec3V_In surfacePos, Vec3V_In boneDir, Vec3V_In riverDir, float boneSpeed, s32 boneID, s32 ptfxOffset);

	void				Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHPEDWADEFX, "Validation of CPacketWaterSplashPedWadeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHPEDWADEFX;
	}

private:

	CPacketVector<3>	m_surfacePos;
	CPacketVector<3>	m_boneDir;
	CPacketVector<3>	m_riverDir;
	s32					m_skeletonBoneID;
	float				m_boneSpeed;
	s32					m_ptfxOffset;

	
};


//////////////////////////////////////////////////////////////////////////
class CPacketWaterBoatEntryFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterBoatEntryFx(Vec3V_In rPos);

	void				Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERBOATENTRYFX, "Validation of CPacketWaterBoatEntryFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERBOATENTRYFX;
	}

private:

	CPacketVector<3>	m_position;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWaterBoatBowFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterBoatBowFx(Vec3V_In surfacePos, Vec3V_In dir, u8 uOffset, bool goingForward);

	void				Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERBOATBOWFX, "Validation of CPacketWaterBoatBowFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERBOATBOWFX;
	}

private:

	CPacketVector<3>	m_surfacePosition;
	CPacketVector<3>	m_direction;
	s8					m_uOffset;
	bool				m_goingForward;
};


//////////////////////////////////////////////////////////////////////////
class CPacketWaterBoatWashFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterBoatWashFx(Vec3V_In surfacePos, s32 uOffset);

	void				Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERBOATWASHFX, "Validation of CPacketWaterBoatWashFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERBOATWASHFX;
	}

private:

	CPacketVector<3>	m_surfacePosition;
	s32					m_uOffset;
};


//=====================================================================================================
class CPacketWaterSplashVehWadeFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:
	CPacketWaterSplashVehWadeFx(u32 uPfxHash, const Vector3& rPos, const Vector3& rSamplePos, s32 sIndex, float fMaxLevel);

	void Extract(const CInterpEventInfo<CPacketWaterSplashVehWadeFx, CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	float GetMaxLevel() const			{ return m_fMaxLevel; }

	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERSPLASHVEHWADEFX, "Validation of CPacketWaterSplashVehWadeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERSPLASHVEHWADEFX;
	}

protected:
	
	void ExtractInterpolateableValues(ptxEffectInst* pFxInst, CVehicle* pVehicle, CPacketWaterSplashVehWadeFx const* pNextPacket, float fInterp) const;

	u32		m_pfxHash;
	CPacketVector3 m_position;
	CPacketVector3 m_samplePosition;
	float	m_fMaxLevel;
	s32		m_sIndex;
};

//=====================================================================================================
class CPacketWaterCannonJetFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterCannonJetFx(s32 boneIndex, Vec3V_In vOffsetPos, float speed);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const	{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERCANNONJETFX, "Validation of CPacketWaterCannonJetFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && GetPacketID() == PACKETID_WATERCANNONJETFX;
	}

protected:
	s32				m_BoneIndex;
	CPacketVector3	m_Position;
	float			m_Speed;
};


//=====================================================================================================
class CPacketWaterCannonSprayFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketWaterCannonSprayFx(Mat34V matrix, float angleEvo, float distEvo);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const	{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERCANNONSPRAYFX, "Validation of CPacketWaterCannonSprayFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && GetPacketID() == PACKETID_WATERCANNONSPRAYFX;
	}

protected:
	CPacketPositionAndQuaternion	m_Matrix;
	float							m_AngleEvo;
	float							m_DistEvo;
};




//////////////////////////////////////////////////////////////////////////
class CPacketSubDiveFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketSubDiveFx(float diveEvo);

	void				Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SUBDIVEFX, "Validation of CPacketSubDiveFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SUBDIVEFX;
	}

private:

	float m_diveEvo;
};



#endif // GTA_REPLAY

#endif // INC_WATERFXPACKET_H_
