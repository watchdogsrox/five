//=====================================================================================================
// name:		ParticleVehicleFxPacket.h
// description:	Vehicle Fx particle replay packet.
//=====================================================================================================

#ifndef INC_VEHFXPACKET_H_
#define INC_VEHFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"
#include "control\replay\packetbasics.h"
#include "control/replay/Vehicle/VehiclePacket.h"
#if INCLUDE_FROM_GAME
#include "Vehicles\VehicleStorage.h"
#include "vfx\decal\decalsettings.h"
#include "vfx/decals/DecalManager.h"
#endif

#if !RSG_ORBIS	
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

class CPed;
class CVehicle;
namespace rage
{class ptxEffectInst;}

//////////////////////////////////////////////////////////////////////////
class CPacketVehBackFireFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehBackFireFx(u32 uPfxHash, float damageEvo, float bangerEvo, s32 exhaustID);

	void			Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHBACKFIREFX, "Validation of CPacketVehBackFireFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHBACKFIREFX;
	}

private:
	u32		m_pfxHash;
	float	m_damageEvo;
	float	m_bangerEvo;
	u8		m_exhaustID;
};

//=====================================================================================================
class CPacketVehOverheatFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:
	CPacketVehOverheatFx(u32 uPfxHash, float fSpeed, float fDamage, float fCustom, float fFire, bool bXAxis);

	void			Extract(const CInterpEventInfo<CPacketVehOverheatFx, CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHOVERHEATFX, "Validation of CPacketVehOverheatFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHOVERHEATFX;
	}

private:

	void ExtractInterpolateableValues(ptxEffectInst* pFxInst, CVehicle* pVeh, CPacketVehOverheatFx const* pNextPacket, float fInterp) const;

	float GetSpeed() const		{ return m_fSpeed; }
	float GetDamage() const		{ return m_fDamage; }
	float GetCustom() const		{ return m_fCustom; }
	float GetFire() const		{ return m_fFire; }

	u32		m_pfxHash;
	float	m_fSpeed;
	float	m_fDamage;
	float	m_fCustom;
	float	m_fFire;
	u8		m_bXAxis		:1;
	u8		m_Pad			:7;
};


//=====================================================================================================
class CPacketVehFirePetrolTankFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:

	CPacketVehFirePetrolTankFx(u32 uPfxHash, u8 uID, const Vector3& rPos);

	void			Extract(const CInterpEventInfo<CPacketVehFirePetrolTankFx, CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHPETROLTANKFX, "Validation of CPacketVehFirePetrolTankFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHPETROLTANKFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StorePos(const Vector3& rVecPos);
	void LoadPos(Vector3& oVecPos) const;

	//-------------------------------------------------------------------------------------------------
	u32				m_pfxHash;
	union
	{
		struct
		{
			float	m_fSpeed;
			float	m_fFire;
			float	m_fRandom;
		};
		float	m_Position[3];
	};
	
	union
	{
		struct
		{
			u8		m_uID				:1;
			u8		m_uWheelId			:3;
			u8		m_bDueToWheelSpin	:1;
			u8		m_bDueToFire		:1;
			u8		m_bLeftWheel		:1;
			u8		m_PadBits			:1;
		};
		u8		m_uUseMe8;
	};

	union
	{
		struct
		{
			s8		m_NormalX;
			s8		m_NormalY;
		};
		s16		m_uUseMe16;
	};

	s8	m_NormalZ;
};

//=====================================================================================================
class CPacketVehHeadlightSmashFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehHeadlightSmashFx(u32 uPfxHash, u8 uBoneIdx, const Vector3& rPos, const Vector3& rNormal);

	void			Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHHEADLIGHTSMASHFX, "Validation of CPacketVehHeadlightSmashFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHHEADLIGHTSMASHFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StorePos(const Vector3& rVecPos);
	void LoadPos(Vector3& oVecPos) const;

	void StoreNormal(const Vector3& rVecNormal);
	void LoadNormal(Vector3& rVecNormal) const;

	//-------------------------------------------------------------------------------------------------
	u32			m_pfxHash;
	union
	{
		struct
		{
			float	m_fSpeed;
			float	m_fFire;
			float	m_fRandom;
		};
		float	m_Position[3];
	};

	union
	{
		struct
		{
			u8		m_uID				:1;
			u8		m_uWheelId			:3;
			u8		m_bDueToWheelSpin	:1;
			u8		m_bDueToFire		:1;
			u8		m_bLeftWheel		:1;
			u8		m_PadBits			:1;
		};
		u8		m_uUseMe8;
	};

	union
	{
		struct
		{
			s8		m_NormalX;
			s8		m_NormalY;
		};
		s16		m_uUseMe16;
	};
	s8	m_NormalZ;
};

//=====================================================================================================
class CPacketVehResprayFx : public CPacketVehHeadlightSmashFx
{
public:
	CPacketVehResprayFx(u32 uPfxHash, Vector3& rPos, Vector3& rNormal);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHRESPRAYFX, "Validation of CPacketVehResprayFx Failed!, 0x%08X", GetPacketID());
		return CPacketVehHeadlightSmashFx::ValidatePacket() && GetPacketID() == PACKETID_VEHRESPRAYFX;
	}
};

//=====================================================================================================
class CPacketVehTyrePunctureFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehTyrePunctureFx(u8 uBoneIdx, u8 wheelIndex, float speedEvo);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHTYREPUNCTUREFX, "Validation of CPacketVehTyrePunctureFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHTYREPUNCTUREFX;
	}

protected:
	u8		m_boneIndex;
	char	m_wheelIndex;
	float	m_speedEvo;
};

//=====================================================================================================
class CPacketVehTyreFireFx : public CPacketVehFirePetrolTankFx
{
public:
	CPacketVehTyreFireFx(u32 uPfxHash, u8 uWheelId, float fSpeed, float fFire);

	void Extract(const CInterpEventInfo<CPacketVehTyreFireFx, CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHTYREFIREFX, "Validation of CPacketVehTyreFireFx Failed!, 0x%08X", GetPacketID());
		return CPacketVehFirePetrolTankFx::ValidatePacket() && GetPacketID() == PACKETID_VEHTYREFIREFX;
	}
protected:
	//-------------------------------------------------------------------------------------------------
	void ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketVehTyreFireFx const* pNextPacket, float fInterp) const;
	//-------------------------------------------------------------------------------------------------
	float GetSpeed() const			{ return m_fSpeed; }
	float GetFire() const			{ return m_fFire; }
};

//=====================================================================================================
class CPacketVehTyreBurstFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehTyreBurstFx(const Vector3& rPos, u8 uWheelId, bool bDueToWheelSpin, bool bDueToFire);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHTYREBURSTFX, "Validation of CPacketVehTyreBurstFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHTYREBURSTFX;
	}

private:
	CPacketVector<3>	m_vecPos;
	u8					m_wheelIndex;
	u8					m_dueToWheelSpin	: 1;
	u8					m_dueToFire			: 1;
};

//=====================================================================================================
class CPacketVehTrainSparksFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:
	CPacketVehTrainSparksFx(u32 uPfxHash, u8 uStartWheel, float fSqueal, u8 uWheelId, bool bIsLeft);

	void Extract(const CInterpEventInfo<CPacketVehTrainSparksFx, CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRAINSPARKSFX, "Validation of CPacketVehTrainSparksFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_TRAINSPARKSFX;
	}

private:
	//-------------------------------------------------------------------------------------------------
	void ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketVehTrainSparksFx const* pNextPacket, float fInterp) const;

	float GetSqueal() const				{ return m_fSqueal; }
	//-------------------------------------------------------------------------------------------------
	// TODO: compress this 0.0f->1.0f
	u32		m_pfxHash;
	float	m_fSqueal;

	u8		m_bIsLeft;
	u8		m_uStartWheel;
	u8		m_uWheelId;
	u8		m_Pad;
};

class CHeli;

//=====================================================================================================
class CPacketHeliDownWashFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:
	CPacketHeliDownWashFx(u32 uPfxHash, const Matrix34& rMatrix, float fDist, float fAngle, float fAlpha);

	void Extract(const CInterpEventInfo<CPacketHeliDownWashFx, CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_HELIDOWNWASHFX, "Validation of CPacketHeliDownWashFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_HELIDOWNWASHFX;
	}
private:
	//-------------------------------------------------------------------------------------------------
	void ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketHeliDownWashFx const* pNextPacket, float fInterp) const;

	void FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos) const;

	void Store(const Matrix34& rMatrix);
	void Load(Matrix34& rMatrix) const;

	float GetDist() const				{ return m_fDist; }
	float GetAngle() const				{ return m_fAngle; }
	float GetAlpha() const				{ return m_fAlpha; }

	//-------------------------------------------------------------------------------------------------
	// TODO: compress this 0.0f->1.0f
	u32		m_pfxHash;
	float	m_Position[3];
	u32		m_HeliDownwashQuaternion;

	float	m_fDist;
	float	m_fAngle;
	float	m_fAlpha;
};

//=====================================================================================================
class CPacketHeliPartsDestroyFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketHeliPartsDestroyFx(bool isTailRotor);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_HELIPARTSDESTROYFX, "Validation of CPacketHeliPartsDestroyFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_HELIPARTSDESTROYFX;
	}

private:
	bool m_IsTailRotor;
};


//////////////////////////////////////////////////////////////////////////
class CPacketVehPartFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehPartFx(u32 uPfxHash, const Vector3& pos, float size);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHPARTFX, "Validation of CPacketVehPartFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHPARTFX;
	}

private:
	u32					m_pfxHash;
	CPacketVector<3>	m_position;
	float				m_size;
};


//////////////////////////////////////////////////////////////////////////
class CPacketVehVariationChange : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehVariationChange(CVehicle* pVehicle);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHVARIATIONCHANGE, "Validation of CPacketVehVariationChange Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHVARIATIONCHANGE;
	}

private:
	CStoredVehicleVariationsREPLAY	m_NewVariation;

	// Added in version 1
	u32	m_numExtraMods;
	u8 m_extraMods[sizeof(extraVehicleModArrayReplay)];

	// Version 2
	u16 m_kitId;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class CPacketPlaneWingtipFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPlaneWingtipFx(u32 id, u32 effectRuleNameHash, float speedEvo);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PLANEWINGTIPFX, "Validation of CPacketVehVariationChange Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PLANEWINGTIPFX;
	}

private:
	u8		m_tipID;				
	u32		m_pfxEffectRuleHashName;
	float	m_speedEvo;
};

//////////////////////////////////////////////////////////////////////////
class CPacketTrailDecal : public CPacketDecalEvent, public CPacketEntityInfo<0>
{
public:
	CPacketTrailDecal(decalUserSettings decalSettings, u32 matId, int decalId);

	void			Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult			Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRAILDECAL, "Validation of CPacketTrailDecal Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_TRAILDECAL;
	}

	void UpdateMonitorPacket()
	{
		m_decalId = g_decalMan.ProcessMergedDecals(m_decalId);
	}

private:
	CPacketVector<3>	m_WorldPos;
	CPacketVector<3>	m_Normal;
	CPacketVector<3>	m_Side;
	CPacketVector<3>	m_Dimentions;

	CPacketVector<3>	m_PassedJoinVerts1;
	CPacketVector<3>	m_PassedJoinVerts2;
	CPacketVector<3>	m_PassedJoinVerts3;
	CPacketVector<3>	m_PassedJoinVerts4;

	u32					m_RenderSettingIndex;
	u32					m_MatID;	
	u8					m_IsScripted;	
	u8					m_AlphaBack;	
	u8					m_colR;
	u8					m_colG;
	u8					m_colB;
	u8					m_colA;
	s8					m_LiquidType;
	u16					m_DecalType;
	float				m_DecalLife;	
	float				m_Length;
};


class CPacketVehicleSlipstreamFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleSlipstreamFx(u32 id, u32 pfxEffectRuleHashName, float slipstreamEvo);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHICLESLIPSTREAMFX, "Validation of CPacketVehVariationChange Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHICLESLIPSTREAMFX;
	}

private:
	u8		m_ID;
	u32		m_pfxEffectRuleHashName;
	float	m_slipstreamEvo;
};

class CPacketVehicleBadgeRequest : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleBadgeRequest(const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHICLE_BADGE_REQUEST, "Validation of CPacketVehicleBadgeRequest Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHICLE_BADGE_REQUEST;
	}

	bool NeedsEntitiesForExpiryCheck() const { return true; }
	bool HasExpired(const CEventInfo<CVehicle>& eventInfo) const;

private:
	CPacketVector<3>	m_offsetPos;
	CPacketVector<3>	m_direction;
	CPacketVector<3>	m_side;

	u32		m_badgeIndex;
	s32		m_boneIndex;
	u32		m_emblemId;
	u32		m_emblemType;
	u8		m_emblemSize;
	u8		m_alpha;

	float	m_size;

	u32		m_txdHashName;
	u32		m_textureHashName;
};



class CPacketVehicleWeaponCharge : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleWeaponCharge(eHierarchyId weaponBone, float chargePhase);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHICLE_WEAPON_CHARGE, "Validation of CPacketVehicleWeaponCharge Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHICLE_WEAPON_CHARGE;
	}

private:

	u32		m_boneId;
	float	m_chargePhase;
};


#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_VEHFXPACKET_H_
