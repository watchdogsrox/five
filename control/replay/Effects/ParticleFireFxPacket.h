//=====================================================================================================
// name:		ParticleFireFxPacket.h
// description:	Fire Fx particle replay packet.
//=====================================================================================================

#ifndef INC_FIREFXPACKET_H_
#define INC_FIREFXPACKET_H_

#include "Control/replay/ReplayEventManager.h"
#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"
#include "phcore/constants.h"

namespace rage
{class ptxEffectInst;}
class CFireSettings;
class CPed;
class CVehicle;

//=====================================================================================================
class CPacketFireSmokeFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketFireSmokeFx(u32 uPfxHash);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FIRESMOKEPEDFX, "Validation of CPacketFireSmokeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FIRESMOKEPEDFX;
	}

private:
	u32		m_pfxHash;
};

//=====================================================================================================
class CPacketFireVehWreckedFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:
	CPacketFireVehWreckedFx(u32 uPfxHash, float fStrength);

	void	Extract(const CInterpEventInfo<CPacketFireVehWreckedFx, CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventInterp::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FIREVEHWRECKEDFX, "Validation of CPacketFireVehWreckedFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FIREVEHWRECKEDFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	float	ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketFireVehWreckedFx const* pNextPacket, float fInterp) const;

	float	GetStrength()	const			{ return m_fStrength; }
	//-------------------------------------------------------------------------------------------------
	// TODO: compress this 0.0f->1.0f
	float	m_fStrength;
	u32		m_pfxHash;
};

//=====================================================================================================
class CPacketFireSteamFx : public CPacketFireVehWreckedFx
{
public:
	CPacketFireSteamFx(u32 uPfxHash, float fStrength, Vector3& rVec);

	void	Extract(const CInterpEventInfo<CPacketFireSteamFx, CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FIRESTEAMFX, "Validation of CPacketFireSteamFx Failed!, 0x%08X", GetPacketID());
		return CPacketFireVehWreckedFx::ValidatePacket() && GetPacketID() == PACKETID_FIRESTEAMFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void	ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketFireSteamFx const* pNextPacket, float fInterp) const;
	//-------------------------------------------------------------------------------------------------
	float	m_Position[3];
};

//=====================================================================================================
class CPacketFirePedBoneFx : public CPacketFireVehWreckedFx
{
public:
	CPacketFirePedBoneFx(u32 uPfxHash, u8 uBodyID, float fStrength, float fSpeed, u8 uBoneIdx1, u8 uBoneIdx2, bool bContainsLight);

	void	Extract(const CInterpEventInfo<CPacketFirePedBoneFx, CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FIREPEDFX, "Validation of CPacketFirePedBoneFx Failed!, 0x%08X", GetPacketID());
		return CPacketFireVehWreckedFx::ValidatePacket() && GetPacketID() == PACKETID_FIREPEDFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void	ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketFirePedBoneFx const* pNextPacket, float fInterp) const;

	float	GetSpeed() const			{ return m_fSpeed; }
	//-------------------------------------------------------------------------------------------------
	// TODO: compress this 0.0f->1.0f
	float	m_fSpeed;

	// TODO: compress this into u32 bitfield (boneIdx's don't go beyond 100ish, bodyID = 1-5max)
	u8		m_uBoneIdx1;
	u8		m_uBoneIdx2;
	u8		m_uBodyID;
	u8		m_bContainsLight	:1;
	u8		m_Pad				:7;
	u32		m_pfxHash;
};



//////////////////////////////////////////////////////////////////////////
class CPacketFireFx_OLD : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketFireFx_OLD(const CFireSettings& fireSettings, s32 fireIndex);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FIREFX, "Validation of CPacketFireFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FIREFX;
	}

	eShouldExtract ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const
	{
		if( flags & REPLAY_DIRECTION_BACK )
			return REPLAY_EXTRACT_SUCCESS;
		else
			return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
	}

	bool HasExpired(const CEventInfo<CEntity>&) const;

protected:

	CPacketVector<3>	m_PosLcl;
	CPacketVector<3>	m_NormLcl;
	CPacketVector<3>	m_ParentPos;	
	u8					m_fireType;			// FireType_e
	u8					m_parentFireType;
	Id					m_mtlId;				// the material that this fire is on (only set on map fires)
	float 				m_burnTime;							// the time that this fire burns for
	float 				m_burnStrength;						// the strength at which this fire burns (at maximum)
	float 				m_peakTime;							// the time at which the fire reaches peak strength
	s32					m_numGenerations; 
	float 				m_fuelLevel;						// the current amount of fuel this fire has
	float 				m_fuelBurnRate;						// how much fuel burns per second
	s32					m_culpritID;
	bool				m_isFromScript;
	bool				m_isPetrolFire;

	s32					m_FireIndex;		//Used for expiry check, not valid on playback
};

//////////////////////////////////////////////////////////////////////////
class CPacketFireFx : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketFireFx(const CFireSettings& fireSettings, s32 fireIndex, Vec3V_In worldPos);

	void			Extract(CTrackedEventInfo<ptxFireRef>& eventInfo) const;
	ePreloadResult	Preload(const CTrackedEventInfo<ptxFireRef>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxFireRef>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRACKED_FIREFX, "Validation of CPacketFireFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_TRACKED_FIREFX;
	}
	
	bool		ShouldEndTracking() const						{	return false;				}
	bool		ShouldStartTracking() const						{	return true;				}

	bool	Setup(bool)
	{
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxFireRef>&) const;

protected:

	CPacketVector<3>	m_PosLcl;
	CPacketVector<3>	m_NormLcl;
	CPacketVector<3>	m_ParentPos;	
	u8					m_fireType;			// FireType_e
	u8					m_parentFireType;
	Id					m_mtlId;				// the material that this fire is on (only set on map fires)
	float 				m_burnTime;							// the time that this fire burns for
	float 				m_burnStrength;						// the strength at which this fire burns (at maximum)
	float 				m_peakTime;							// the time at which the fire reaches peak strength
	s32					m_numGenerations; 
	float 				m_fuelLevel;						// the current amount of fuel this fire has
	float 				m_fuelBurnRate;						// how much fuel burns per second
	s32					m_culpritID;
	bool				m_isFromScript;
	bool				m_isPetrolFire;

	s32			m_FireIndex;		//Used for expiry check, not valid on playback

	CPacketVector<3>	m_WorldPos;
};

//////////////////////////////////////////////////////////////////////////
class CPacketStopFireFx : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketStopFireFx();

	void			Extract(CTrackedEventInfo<ptxFireRef>& eventInfo) const;
	ePreloadResult	Preload(const CTrackedEventInfo<ptxFireRef>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxFireRef>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRACKED_STOPFIREFX, "Validation of CPacketStopFireFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_TRACKED_STOPFIREFX;
	}

	bool		ShouldEndTracking() const						{	return true;				}
	bool		ShouldStartTracking() const						{	return false;				}

	bool	Setup(bool)
	{
		return true;
	}

protected:

};

//////////////////////////////////////////////////////////////////////////
class CPacketFireEntityFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketFireEntityFx(size_t id, const Matrix34& matrix, float strength, int boneIndex);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FIREENTITYFX, "Validation of CPacketFireEntityFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FIREENTITYFX;
	}

protected:
	CPacketPositionAndQuaternion	m_Matrix;
	float							m_Strength;
	int								m_BoneIndex;
	u64								m_Id;
};

//////////////////////////////////////////////////////////////////////////
class CPacketFireProjectileFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketFireProjectileFx(bool isPrime, bool isFirstPersonActive, bool isFirstPersonPass, u32 effectHashName);

	void	Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CObject>&) const									{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PROJECTILEFX, "Validation of CPacketFireProjectileFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PROJECTILEFX;
	}

protected:
	bool			m_IsPrime;
	bool			m_FirstPersonActive;
	bool			m_IsFirstPersonPass;
	u32				m_EffectHashName;
};

//////////////////////////////////////////////////////////////////////////
class CPacketExplosionFx : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketExplosionFx(size_t id, u32 hashName, const Matrix34& matrix, float radius, int boneIndex, float scale);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_EXPLOSIONFX, "Validation of CPacketExplosionFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_EXPLOSIONFX;
	}

protected:
	CPacketPositionAndQuaternion	m_Matrix;
	u32								m_HashName;
	float							m_Radius;
	float							m_Scale;
	int								m_BoneIndex;
	u64								m_Id;
};


class CPacketRegisterVehicleFire : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	enum 
	{
		TANK_FIRE_0 = 0,
		TANK_FIRE_1,
		TYRE_FIRE,
	};

	CPacketRegisterVehicleFire(int boneIndex, const Vec3V& localPos, u8 wheelIndex, float evo, u8 fireType);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_REGVEHICLEFIRE, "Validation of CPacketRegisterVehicleFire Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_REGVEHICLEFIRE;
	}

private:
	
	int					m_boneIndex;
	CPacketVector<3>	m_localPos;
	u8					m_wheelIndex;
	u8					m_fireType;
	u8					m_unused[2];
	float				m_evo;

};


#endif // GTA_REPLAY

#endif // INC_FIREFXPACKET_H_
