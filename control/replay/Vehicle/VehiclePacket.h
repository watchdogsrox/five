//
// name:		VehiclePacket.h
// description:	This class handles all vehicle-related replay packets.
// written by:	Al-Karim Hemraj (R* Toronto)
//


#ifndef VEHICLEPACKET_H
#define VEHICLEPACKET_H

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Control/replay/replay_channel.h"
#include "Control/replay/Misc/InterpPacket.h"
#include "Control/replay/PacketBasics.h"
#include "control/replay/Vehicle/ReplayWheelValues.h"
#include "control/replay/Entity/FragmentPacket.h"

#include "Vehicles/VehicleFlags.h"
#include "vehicles/VehicleGadgets.h"
#include "vehicles/VehicleDefines.h"
#include "vehicles/vehiclestorage.h"
#include "vehicles/Automobile.h"
#include "audio/vehiclegadgetaudio.h"

#if INCLUDE_FROM_GAME
#include "shaders/CustomShaderEffectVehicle.h"
#endif

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS


class CWheel;
class CVehicle;
class CPacketFragBoneData;
template<typename QUATTYPE>
class CPacketFragBoneDataUsingSkeletonPackets;
class CVehicleGadgetDataBase;
class CPacketWheel;


float ProcessChainValue(float currentVal, float nextVal, float interpIn, bool animFwd);


//////////////////////////////////////////////////////////////////////////

#define WHEEL_HAS_DYNAMIC_DATA	0x1
#define WHEEL_HAS_HEALTH_DATA	0x2

class CWheelBaseData
{
public:
	CWheelBaseData() {} 
	~CWheelBaseData(){}
public:
	u32				m_DynamicFlags;
	u32				m_ConfigFlags : 30;
	u32				m_TypeInfo : 2; // Has dynamic/health data.
	valComp<s8, s8_f32_nPI_to_PI>	m_steeringAngle;
	valComp<s8, s8_f32_nPI_to_PI>	m_rotAngle;
#if REPLAY_WHEELS_USE_PACKED_VECTORS
	CPacketVector3Comp<valComp<s16, s16_f32_n4_to_4>>	m_vecHitPos;
	CPacketVector3Comp<valComp<s16, s16_f32_n4_to_4>>	m_vecHitCentrePos;
	CPacketVector3Comp<valComp<s16, s16_f32_n4_to_4>>	m_vecHitNormal;
#else // REPLAY_WHEELS_USE_PACKED_VECTORS
	CPacketVector3	m_vecHitPos;
	CPacketVector3	m_vecHitCentrePos;
	CPacketVector3	m_vecHitNormal;
#endif // REPLAY_WHEELS_USE_PACKED_VECTORS

	void Print() const
	{
		replayDebugf1("m_DynamicFlags:	%u", m_DynamicFlags);
		replayDebugf1("m_ConfigFlags:	%u", m_ConfigFlags);
		replayDebugf1("m_TypeInfo:	%u", m_TypeInfo);
		replayDebugf1("m_steeringAngle:	%f", m_steeringAngle.ToFloat());
		replayDebugf1("m_rotAngle:	%f", m_rotAngle.ToFloat());
		Vector3 vec;
		m_vecHitPos.Load(vec);
		replayDebugf1("m_vecHitPos: %f, %f, %f", vec.GetX(), vec.GetY(), vec.GetZ());
		m_vecHitCentrePos.Load(vec);
		replayDebugf1("m_vecHitCentrePos: %f, %f, %f", vec.GetX(), vec.GetY(), vec.GetZ());
		m_vecHitNormal.Load(vec);
		replayDebugf1("m_vecHitNormal: %f, %f, %f", vec.GetX(), vec.GetY(), vec.GetZ());
	}
};


class CWheelDynamicData
{
public:
	CWheelDynamicData()	{ memset(this, 0, sizeof(*this));	}
	~CWheelDynamicData() {}

	bool operator==(const CWheelDynamicData &other) const
	{
		if(m_tyreTemp != other.m_tyreTemp)
			return false;
		if(m_tyreLoad != other.m_tyreLoad)
			return false;
		if(m_fEffectiveSlipAngle != other.m_fEffectiveSlipAngle)
			return false;
		if(m_fwdSlipAngle != other.m_fwdSlipAngle)
			return false;

		if(m_BrakeForce != other.m_BrakeForce)
			return false;
		if(m_FrictionDamage != other.m_FrictionDamage)
			return false;
		if(m_Compression != other.m_Compression)
			return false;

		if(m_CompressionOld != other.m_CompressionOld)
			return false;
		if(m_RotAngVel != other.m_RotAngVel)
			return false;
		if(m_EffectiveSlipAngle != other.m_EffectiveSlipAngle)
			return false;
		if(m_wheelCompression != other.m_wheelCompression)
			return false;

		// We don`t include m_vecGroundContactVelocity in the comparison. Assume we don`t need they value if all other ones are the default.
		return true;
	}

	void Print() const
	{
		replayDebugf1("m_tyreTemp:	%f", m_tyreTemp);
		replayDebugf1("m_tyreLoad:	%f", m_tyreLoad);
		replayDebugf1("m_fEffectiveSlipAngle:	%f", m_fEffectiveSlipAngle);
		replayDebugf1("m_fwdSlipAngle:	%f", m_fwdSlipAngle);
		replayDebugf1("m_sideSlipAngle:	%f", m_sideSlipAngle);

		float f = m_BrakeForce;
		replayDebugf1("m_BrakeForce:	%f", f);
		f = m_FrictionDamage;
		replayDebugf1("m_FrictionDamage:	%f", f);
		f = m_Compression;
		replayDebugf1("m_Compression:	%f", f);
		f = m_CompressionOld;
		replayDebugf1("m_CompressionOld:	%f", f);
		f = m_RotAngVel;
		replayDebugf1("m_RotAngVel:	%f", f);
		f = m_EffectiveSlipAngle;
		replayDebugf1("m_EffectiveSlipAngle:	%f", f);
		f = m_wheelCompression;
		replayDebugf1("m_wheelCompression:	%f", f);
		Vector3 vec;
		m_vecGroundContactVelocity.Load(vec);
		replayDebugf1("m_vecGroundContactVelocity: %f, %f, %f", vec.GetX(), vec.GetY(), vec.GetZ());
	}

public:
	float	m_tyreTemp;
	float	m_tyreLoad;
	// TODO4FIVE:- This is a duplicate of m_EffectiveSlipAngle.
	float	m_fEffectiveSlipAngle;
	float	m_fwdSlipAngle;
	float	m_sideSlipAngle;
	float	m_Compression;
	float	m_CompressionOld;
	float	m_staticDelta;

	valComp<u8, u8_f32_0_to_5>			m_BrakeForce;			// 0 to 5	   - f32 - See \gta\build\common\data\handling.dat
	valComp<u8, u8_f32_0_to_3_0>		m_FrictionDamage;		// 0 to 3.0	   - f32 - Range obtained by trial & error
	valComp<s16, s16_f32_n650_to_650>	m_RotAngVel;			// -650 to 650 - f32 - Range obtained by trial & error
	valComp<u8, u8_f32_0_to_65>			m_EffectiveSlipAngle;	// 0 to 65     - f32 - Range obtained by trial & error
	valComp<s8, s8_f32_n20_to_20>		m_wheelCompression;
	CPacketVector3	m_vecGroundContactVelocity;
	CPacketVector3	m_vecTyreContactVelocity;
};


class CWheelHealthData
{
public:
	CWheelHealthData() { m_SuspensionHealth = 1000.0f; m_TyreHealth = 1000.0f; }
	~CWheelHealthData(){}

	bool operator==(const CWheelHealthData &other) const
	{
		if(m_SuspensionHealth != other.m_SuspensionHealth)
			return false;
		if(m_TyreHealth != other.m_TyreHealth)
			return false;
		return true;
	}

	void Print() const
	{
		replayDebugf1("m_SuspensionHealth:	%f", m_SuspensionHealth);
		replayDebugf1("m_TyreHealth:	%f", m_TyreHealth);
	}

public:
	float	m_SuspensionHealth;		// 0 to 1000   - f32 - See wheel.h
	float	m_TyreHealth;			// 0 to 1000   - f32 - See wheel.h
};


class CWheelWithDynamicData : CWheelBaseData
{
public:
	CWheelWithDynamicData() {}
	~CWheelWithDynamicData() {}
public:
	CWheelDynamicData m_DynamicData;
};


class CWheelWithHealthData : CWheelBaseData
{
public:
	CWheelWithHealthData() {}
	~CWheelWithHealthData() {}
public:
	CWheelHealthData m_HealthData;
};


class CWheelFullData : public CWheelBaseData
{
public:
	CWheelFullData() {}
	~CWheelFullData(){}

	void	Extract(CVehicle* pVehicle, CWheel* pWheel, CWheelFullData const* pNextPacket = NULL, f32 fInterpValue = 0.0f) const;
	void	ExtractInterpolateableValues(CWheel* pWheel, CWheelFullData const* pNextPacket = NULL, f32 fInterp = 0.0f) const;

	float	GetWheelCompression() const		{ return m_DynamicData.m_wheelCompression;	}
	float	GetSteeringAngle() const		{ return m_steeringAngle;		}
	float	GetRotAngle() const				{ return m_rotAngle;			}
	float	GetSuspensionHealth() const		{ return m_HealthData.m_SuspensionHealth;	}
	float	GetTyreHealth()	const			{ return m_HealthData.m_TyreHealth;			}
	float	GetBrakeForce()	const			{ return m_DynamicData.m_BrakeForce;			}
	float	GetFrictionDamage()	const		{ return m_DynamicData.m_FrictionDamage;		}
	float	GetCompression() const			{ return m_DynamicData.m_Compression;			}
	float	GetCompressionOld()	const		{ return m_DynamicData.m_CompressionOld;		}
	float	GetStaticDelta() const			{ return m_DynamicData.m_staticDelta;			}
	float	GetRotAngVel() const			{ return m_DynamicData.m_RotAngVel;			}
	float	GetEffectiveSlipAngle() const	{ return m_DynamicData.m_EffectiveSlipAngle;	}

	float	GetTyreLoad() const				{ return m_DynamicData.m_tyreLoad; }
	float	GetTyreTemp() const				{ return m_DynamicData.m_tyreTemp; }
	float	GetfEffectiveSlipAngle() const	{ return m_DynamicData.m_fEffectiveSlipAngle; }
	float	GetFwdSlipAngle() const			{ return m_DynamicData.m_fwdSlipAngle; }
	float	GetSideSlipAngle() const		{ return m_DynamicData.m_sideSlipAngle; }

	void	Print() const
	{
		CWheelBaseData::Print();
		m_DynamicData.Print();
		m_HealthData.Print();
	}
public:
	CWheelDynamicData m_DynamicData;
	CWheelHealthData m_HealthData;
};



	//-----------------------------------------------------------
	// Compressed Values
	//  - Compressed values are commented in the format shown
	//     below. This helps to determine the best approach for
	//     compression with minimal loss of precision. It also 
	//     helps for debugging purposes incase the range changes
	//     in the future.
	//-----------------------------------------------------------
	// New_Type Name;		// Range - Original Type - Comments
	//-----------------------------------------------------------

	/*
	// Base info - flags, wheel rotation, contact points (required for fx rendering).
	u32				m_DynamicFlags;
	u32				m_ConfigFlags;
	valComp<s8, s8_f32_n1_75_to_1_75>	m_steeringAngle;
	valComp<s8, s8_f32_nPI_to_PI>		m_rotAngle;
	s8				m_TempPad[2]; // Will remove when below are packed vectors.
	CPacketVector3	m_vecHitPos;
	CPacketVector3	m_vecHitCentrePos;
	CPacketVector3	m_vecHitNormal;
	CPacketVector3	m_vecGroundContactVelocity;

	// Dynamic data.
	float	m_tyreTemp;
	float	m_tyreLoad;
	float	m_fEffectiveSlipAngle;
	float	m_fwdSlipAngle;
	float	m_sideSlipAngle;
	valComp<u8, u8_f32_0_to_5>			m_BrakeForce;			// 0 to 5	   - f32 - See \gta\build\common\data\handling.dat
	valComp<u8, u8_f32_0_to_1_8>		m_FrictionDamage;		// 0 to 1.8	   - f32 - Range obtained by trial & error
	valComp<s8, s8_f32_n127_to_127>		m_Compression;			// -127 to 127 - f32 - Range obtained by trial & error
	valComp<s8, s8_f32_n127_to_127>		m_CompressionOld;		// -127 to 127 - f32 - Range obtained by trial & error
	valComp<s16, s16_f32_n190_to_190>	m_RotAngVel;			// -190 to 190 - f32 - Range obtained by trial & error
	valComp<u8, u8_f32_0_to_65>			m_EffectiveSlipAngle;	// 0 to 65     - f32 - Range obtained by trial & error
	valComp<s8, s8_f32_n20_to_20>		m_wheelCompression;

	// Health.
	float	m_SuspensionHealth;		// 0 to 1000   - f32 - See wheel.h
	float	m_TyreHealth;			// 0 to 1000   - f32 - See wheel.h
	*/


#if REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

class CPacketVehicleWheelUpdate_Old : public CPacketBase
{
public:
	//-------------------------------------------------------------------------------------------------
	void	Store(const CVehicle *pVehicle);
	void	StoreExtensions(const CVehicle *pVehicle) { PACKET_EXTENSION_RESET(CPacketVehicleWheelUpdate_Old); (void)pVehicle; };
	void	PrintXML(FileHandle handle) const;
	void	GetWheelData(u32 wheelId, CWheelFullData &dest) const;

	static u32 EstimatePacketSize(const CVehicle *pVehicle);

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WHEELUPDATE_OLD, "Validation of CPacketVehicleWheelUpdate_Old Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_WHEELUPDATE_OLD;
	}

private:
	CWheelBaseData *GetCWheelBaseDataPtr(u32 wheelId) const;
	CWheelBaseData *GetBasePtr() const { return (CWheelBaseData *)((u8 *)this + this->GetPadU8(1));}
	DECLARE_PACKET_EXTENSION(CPacketVehicleWheelUpdate_Old);
};

#else // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

class CPacketVehicleWheelUpdate_Old : public CPacketBase
{
public:
	//-------------------------------------------------------------------------------------------------
	void	Store(const CWheel* pWheel);
	void	PrintXML(FileHandle handle) const;
	void	GetWheelData(CWheelFullData &dest) const { dest = m_WheelData; }

	static u32 EstimatePacketSize(const CVehicle *pVehicle) { return sizeof(CPacketVehicleWheelUpdate_Old)*pVehicle->GetNumWheels(); }

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WHEELUPDATE_OLD, "Validation of CPacketVehicleWheelUpdate_Old Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_WHEELUPDATE_OLD;
	}

private:
	CWheelFullData m_WheelData;
};

#endif // REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET

//////////////////////////////////////////////////////////////////////////
class CPacketVehicleDoorUpdate : public CPacketBase
{
public:

	u8		GetDoorIdx() const		{ return GetPadU8(0); }

	void	Store(CVehicle* pVehicle, s32 doorIdx);
	void	StoreExtensions(const CVehicle* pVehicle, s32 doorIdx);
	bool	Extract(CVehicle* pVehicle, CPacketVehicleDoorUpdate const* pNextPacket = NULL, f32 fInterp = 0.0f) const;

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_DOORUPDATE, "Validation of CPacketVehicleDoorUpdate Failed!, 0x%08X", GetPacketID());
		return (GetPacketID() == PACKETID_DOORUPDATE) && CPacketBase::ValidatePacket();	
	}

	static u32 EstimatePacketSize() { return sizeof(CPacketVehicleDoorUpdate) + sizeof(PacketExtensions); }

	void	PrintXML(FileHandle handle) const;

private:

	union
	{
		CPacketQuaternionL	m_DoorQuaternion;
		float				m_fTranslation;
	};

	float			m_doorRatio;
	float			m_doorOldAudioRatio;
	DECLARE_PACKET_EXTENSION(CPacketVehicleDoorUpdate);

	struct PacketExtensions
	{
		//---------------------- Extensions V1 ------------------------/
		u32	m_doorFlags;
	};

	CPacketVector<3>	m_doorTranslation;
};

//////////////////////////////////////////////////////////////////////////
class CPacketVehDamageUpdate : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehDamageUpdate(const Vector4& rDamage, const Vector4& rOffset, CVehicle* pVehicle, eReplayPacketId packetId = PACKETID_VEHICLEDMGUPDATE);

	void Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_VEHICLEDMGUPDATE, "Validation of CPacketVehicleDamageUpdate Failed!, 0x%08X", GetPacketID());
		return (GetPacketID() == PACKETID_VEHICLEDMGUPDATE) && CPacketBase::ValidatePacket();	
	}

	void	Print() const	{}

	bool	NeedsEntitiesForExpiryCheck() const		{ return true; }
	bool	HasExpired(const CEventInfo<CVehicle>& info) const;

private:
	CPacketVector3Comp<valComp<s8, s8_f32_n1_to_1>>	m_OffsetVector;
	CPacketVector4Comp	m_DamageVector;
};

//////////////////////////////////////////////////////////////////////////
class CPacketVehDamageUpdate_PlayerVeh : public CPacketVehDamageUpdate
{
public:
	CPacketVehDamageUpdate_PlayerVeh(const Vector4& rDamage, const Vector4& rOffset, CVehicle* pVehicle);

	bool ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_VEHICLEDMGUPDATE_PLAYERVEH, "Validation of CPacketVehDamageUpdate_PlayerVeh Failed!, 0x%08X", GetPacketID());
		return (GetPacketID() == PACKETID_VEHICLEDMGUPDATE_PLAYERVEH) && CPacketBase::ValidatePacket();	
	}
private:
};


//////////////////////////////////////////////////////////////////////////
// *** DO NOT CHANGE THIS *** 
static const u8 oldVehicleModArrayReplay1[] =
{
	VMT_SPOILER,
	VMT_BUMPER_F,
	VMT_BUMPER_R,
	VMT_SKIRT,
	VMT_EXHAUST,
	VMT_CHASSIS,
	VMT_GRILL,
	VMT_BONNET,
	VMT_WING_L,
	VMT_WING_R,
	VMT_ROOF,
};

//////////////////////////////////////////////////////////////////////////
// *** DO NOT CHANGE THIS *** 
static const u8 oldVehicleModArrayReplay2[] =
{
	VMT_ENGINE,
	VMT_BRAKES,
	VMT_GEARBOX,
	VMT_HORN,
	VMT_SUSPENSION,
	VMT_ARMOUR,

	VMT_NITROUS,
	VMT_TURBO,
	VMT_SUBWOOFER,
	VMT_TYRE_SMOKE,
	VMT_HYDRAULICS,
	VMT_XENON_LIGHTS,

	VMT_WHEELS,
	VMT_WHEELS_REAR_OR_HYDRAULICS,
};

const int VMT_MAX_OLD = 25;
class CStoredVehicleVariationsREPLAY
{
public:

	CStoredVehicleVariationsREPLAY();

	void StoreVariations(const CVehicle* pVeh);

 	//void				SetKitIndex(u8 kitIndex)	{ m_kitIdx = kitIndex; }//

 	bool				IsEqual(const CStoredVehicleVariationsREPLAY& rhs) const
 	{
 		for(int i = 0; i < VMT_MAX_OLD; ++i)
 		{
 			if(m_mods[i] != rhs.m_mods[i])
 			{
 				return false;
 			}
 		}
 
 		return m_kitIndex == rhs.m_kitIndex && 
 			m_color1 == rhs.m_color1 && 
 			m_color2 == rhs.m_color2 && 
 			m_color3 == rhs.m_color3 && 
 			m_color4 == rhs.m_color4 && 
 			m_smokeColR == rhs.m_smokeColR &&
 			m_smokeColG == rhs.m_smokeColG && 
 			m_smokeColB == rhs.m_smokeColB && 
 			m_neonColR == rhs.m_neonColR &&
 			m_neonColG == rhs.m_neonColG && 
 			m_neonColB == rhs.m_neonColB && 
 			m_neonFlags == rhs.m_neonFlags && 
 			m_windowTint == rhs.m_windowTint &&
 			m_wheelType == rhs.m_wheelType &&
 			m_modVariation[0] == rhs.m_modVariation[0] &&
 			m_modVariation[1] == rhs.m_modVariation[1];
 	}

	void ToGameStruct(CStoredVehicleVariations& dest) const
	{
		dest.SetKitIndex(m_kitIndex);
		dest.SetColor1(m_color1);
		dest.SetColor2(m_color2);
		dest.SetColor3(m_color3);
		dest.SetColor4(m_color4);
		dest.SetSmokeColorR(m_smokeColR);
		dest.SetSmokeColorG(m_smokeColG);
		dest.SetSmokeColorB(m_smokeColB);
		dest.SetNeonColorR(m_neonColR);
		dest.SetNeonColorG(m_neonColG);
		dest.SetNeonColorB(m_neonColB);
		dest.SetNeonFlags(m_neonFlags);
		dest.SetWindowTint(m_windowTint);
		dest.SetWheelType(m_wheelType);
		dest.SetModVariation(0, m_modVariation[0]);
		dest.SetModVariation(1, m_modVariation[1]);

		u32 storedModIndex = 0;
		for (u32 mod_loop = 0; mod_loop < sizeof(oldVehicleModArrayReplay1); mod_loop++)
		{
			dest.SetModIndex((eVehicleModType)oldVehicleModArrayReplay1[mod_loop], m_mods[storedModIndex++]);
		}

		for (u32 mod_loop = 0; mod_loop < sizeof(oldVehicleModArrayReplay2); mod_loop++)
		{
			dest.SetModIndex((eVehicleModType)oldVehicleModArrayReplay2[mod_loop], m_mods[storedModIndex++]);
		}
	}

	u16 GetKitIndex() const { return m_kitIndex;}

private:
	//	u8 m_numMods;
	u8 m_kitIndex; //
	u8 m_mods[VMT_MAX_OLD];
	u8 m_color1, m_color2, m_color3, m_color4;
	u8 m_smokeColR, m_smokeColG, m_smokeColB;
	u8 m_neonColR, m_neonColG, m_neonColB, m_neonFlags;
	u8 m_windowTint;
	u8 m_wheelType;
	bool m_modVariation[2]; // variations for the two types of wheels
};


//////////////////////////////////////////////////////////////////////////
class CPacketVehicleCreate : public CPacketBase, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleCreate();

	void	Store(const CVehicle* pVehicle, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId = PACKETID_VEHICLECREATE, tPacketSize packetSize = sizeof(CPacketVehicleCreate));
	void	StoreExtensions(const CVehicle* pVehicle, bool firstCreationPacket, u16 sessionBlockIndex);
	void	CloneExtensionData(const CPacketVehicleCreate* pSrc) { if(pSrc) { CLONE_PACKET_EXTENSION_DATA(pSrc, this, CPacketVehicleCreate); } }
	void	ExtractReferenceWheelValuesIntoVehicleExtension(CVehicle *pVehicle);

	void	SetFrameCreated(const FrameRef& frame)	{	m_FrameCreated = frame;	}
	FrameRef GetFrameCreated() const				{	return m_FrameCreated; }
	void	SetIsFirstCreationPacket(bool b)		{	m_firstCreationPacket = b; }
	bool	IsFirstCreationPacket() const			{	return m_firstCreationPacket;	}

	void	Reset() 						{ SetReplayID(-1);	m_FrameCreated = FrameRef::INVALID_FRAME_REF;	}

	u32				GetModelNameHash() const		{	return m_ModelNameHash;	}
	strLocalIndex	GetMapTypeDefIndex() const		{	return strLocalIndex(strLocalIndex::INVALID_INDEX); }
	void			SetMapTypeDefIndex(strLocalIndex /*index*/)	{}
	u32				GetMapTypeDefHash() const		{ return 0; }
	bool	UseMapTypeDefHash() const { return false; } 
	s32		GetLiveryId() const				{ return m_LiveryId;		}
	s32		GetDisableExtras() const		{ return m_DisableExtras;	}
	const char* GetLicencePlateText() const	{ return m_LicencePlateText; }
	s32		GetLicencePlateTexIdx() const	{ return m_LicencePlateTexIdx; }
	u32		GetHornHash() const				{ return m_HornHash;		}
	s16		GetHornIndex() const			{ return m_HornIndex;		}

	u8		GetAlarmType() const			{ return m_alarmType;		}

	const CStoredVehicleVariationsREPLAY& GetInitialVariationData() const	{ return m_InitialVariationData; }
	const CStoredVehicleVariationsREPLAY& GetLatestVariationData() const		{ return m_LatestVariationData; }
		  CStoredVehicleVariationsREPLAY& GetLatestVariationData()			{ return m_LatestVariationData; }
	const CStoredVehicleVariationsREPLAY& GetVariationData() const
	{
		if(m_VarationDataVerified == false)
		{
			return GetLatestVariationData();
		}

		return GetInitialVariationData();
	}

	bool	IsVarationDataVerified() const { return m_VarationDataVerified; }
	void	SetVarationDataVerified() { m_VarationDataVerified = true; }

	bool	GetPlayerInThisVehicle() const		{	return m_playerInThisVehicle;	}

	void	LoadMatrix(Matrix34& matrix) const	{	m_posAndRot.LoadMatrix(matrix);	}

	

	bool	ValidatePacket() const
	{
		replayAssertf((GetPacketID() == PACKETID_VEHICLECREATE) || (GetPacketID() == PACKETID_VEHICLEDELETE) , "Validation of CPacketVehicleCreate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketVehicleCreate), "Validation of CPacketVehicleCreate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketVehicleCreate) && ((GetPacketID() == PACKETID_VEHICLECREATE) || (GetPacketID() == PACKETID_VEHICLEDELETE));
	}	

	void	PrintXML(FileHandle handle) const;

	u8		GetEnvEffScale() const			{	return m_envEffScale;	}
	void	SetCustomColors(CCustomShaderEffectVehicle* pCSEV) const;

	void	SetVariationData(CVehicle* pVehicle) const;
	CReplayID	GetParentID() const;

private:
	FrameRef	m_FrameCreated;
	bool		m_firstCreationPacket;

	u8		m_VehicleTypeOld;
	u32		m_ModelNameHash;
	s32		m_LiveryId;
	s32		m_DisableExtras;
	char	m_LicencePlateText[CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX];
	s32		m_LicencePlateTexIdx;

	CPacketPositionAndQuaternion	m_posAndRot;

	u32		m_HornHash;
	s16		m_HornIndex;
	u8		m_alarmType;
	u8		m_envEffScale;

	Color32 m_customPrimaryColor;
	Color32 m_customSecondaryColor;
	u8		m_hasCustomPrimaryColor					: 1;
	u8		m_hasCustomSecondaryColor				: 1;
	u8		m_playerInThisVehicle					: 1;

	u8						m_VarationDataVerified	: 1;
	u8						m_unused0				: 4;
	CStoredVehicleVariationsREPLAY	m_InitialVariationData;
	CStoredVehicleVariationsREPLAY	m_LatestVariationData;

public:
	// Extensions.
	typedef struct WHEEL_REFERENCE_VALUES_HEADER
	{
		u32 noOfWheels;
		u16 sessionBlockIndices[2];
		ReplayWheelValuesPair wheelValuePairs[0];
	} WHEEL_REFERENCE_VALUES_HEADER;

	DECLARE_PACKET_EXTENSION(CPacketVehicleCreate);

	struct VehicleCreateExtension
	{
		// CPacketVehicleCreateExtensions_V2
		CReplayID	m_parentID;

		// CPacketVehicleCreateExtensions_V3
		CPacketPositionAndQuaternion	m_dialBoneMtx;	// Defunct on new clips...not enough precision (url:bugstar:2814768)

		// CPacketVehicleCreateExtensions_V4
		Color32	m_dirtColour;

		// CPacketVehicleCreateExtensions_V5
		u32	m_numExtraMods;

		// CPacketVehicleCreateExtensions_V6
		u32 m_extraModOffset;
		s32	m_Livery2Id;

		// CPacketVehicleCreateExtensions_V7
		u8	m_colour5;
		u8	m_colour6;

		// CPacketVehicleCreateExtensions_V10
		u8	m_xenonLightColour;
		u8	m_unused[1];

		// CPacketVehicleCreateExtensions_V8
		CPacketPositionAndQuaternion20	m_dialBoneMtx2;

		// CPacketVehicleCreateExtensions_V9
		u16 m_initialKitId;
		u16 m_latestKitId;
	};

	VehicleCreateExtension* GetExtension()
	{
		WHEEL_REFERENCE_VALUES_HEADER *pDest = (WHEEL_REFERENCE_VALUES_HEADER *)GET_PACKET_EXTENSION_READ_PTR_OTHER((CPacketVehicleCreate*)this, CPacketVehicleCreate);

		return (VehicleCreateExtension*)&pDest->wheelValuePairs[pDest->noOfWheels];
	}

	void RecomputeExtensionCRC()
	{
		PACKET_EXTENSION_RECOMPUTE_DATA_CRC(CPacketVehicleCreate);
	}
};

//////////////////////////////////////////////////////////////////////////

// Set this to true to allow x64 to read Orbis clips correctly.
#define CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64 (0 && RSG_PC && !__FINAL)

#if CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
#define CPACKETVEHICLEUPDATE_M_STATUS_TYPE u8
#else // CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
#define CPACKETVEHICLEUPDATE_M_STATUS_TYPE u16
#endif // CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64


class CPacketVehicleUpdate : public CPacketBase, public CPacketInterp, public CBasicEntityPacketData
{
public:

#if REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	typedef CPacketFragBoneDataUsingSkeletonPackets<CPacketQuaternionH>	tfragBoneDataType_HighQuality;
	typedef CSkeletonBoneData<CPacketQuaternionH>						tSkeletonBoneDataType_HighQuality;
	const static eReplayPacketId fragBoneDataPacketIdHQ =  PACKETID_FRAGBONEDATA_USINGSKELETONPACKETS_HIGHQUALITY;

	typedef CPacketFragBoneDataUsingSkeletonPackets<CPacketQuaternionL>	tfragBoneDataType;
	typedef CSkeletonBoneData<CPacketQuaternionL>						tSkeletonBoneDataType;
	const static eReplayPacketId fragBoneDataPacketId =  PACKETID_FRAGBONEDATA_USINGSKELETONPACKETS;

#else // REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	typedef CPacketFragBoneData		tfragBoneDataType;
	const static eReplayPacketId fragBoneDataPacketId =  PACKETID_FRAGBONEDATA;
#endif // REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET

public:

	void	Print() const;

	u8	 GetWheelCount() const	{ return m_WheelCount; }
	u8	 GetDoorCount() const	{ return m_DoorCount; }
	
	void GetSpeed(Vector3& rSpeed) const	{ ExtractSpeed(rSpeed); }

	bool HasFragInst() const	{ return m_bContainsFrag; }
	bool DoesFragHaveDamageBones() const	{ return m_bHasDamageBones; }

	bool HasModBoneData() const { return m_bHasModBoneData; }

	s8	 GetSteerAngle()	const	{ return m_SteerAngle;		}
	s8	 GetGasPedal()		const	{ return m_GasPedal;		}
	s8	 GetBrake()			const	{ return m_Brake;			}
	u8	 GetFireEvo()		const	{ return m_FireEvo;			}
	s8	 GetThrottle()		const	{ return m_Throttle;		}
	f32	 GetRevs()			const	{ return m_Revs;			}
	f32	 GetEngineHealth()	const	{ return m_EngineHealth;	}

	bool IsScorched()		const	{ return m_bRenderScorched; }
	void SetScorched(bool bScorch)	{ m_bRenderScorched = (u8)bScorch; }

	void RemoveAndAdd(CVehicle* pVehicle, const fwInteriorLocation& interiorLoc, u32 playbackFlags, bool inInterior, bool bIgnorePhysics) const;

	void Store(eReplayPacketId uPacketID, u32 packetSize, const CVehicle* pVehicle, bool firstUpdatePacket, tPacketVersion derivedPacketVersion = InitialVersion);
	void StoreExtensions(eReplayPacketId uPacketID, u32 packetSize, const CVehicle* pVehicle, bool firstUpdatePacket, tPacketVersion derivedPacketVersion = InitialVersion);
	void Store(const CVehicle*, bool) {	replayAssert(false && "CPacketVehicleUpdate::Store(CVehicle*) should never be called");	}
	void StoreExtensions(const CVehicle*, bool) { PACKET_EXTENSION_RESET(CPacketVehicleUpdate); replayAssert(false && "CPacketVehicleUpdate::StoreExtensions(CVehicle*) should never be called"); }

	void UpdateAfterPartsRecorded(const CVehicle* pVehicle, int numWheelPackets, int numDoorPackets, int numFragPackets);

	void PreUpdate(const CInterpEventInfo<CPacketVehicleUpdate, CVehicle>& info) const;
	void Extract(const CInterpEventInfo<CPacketVehicleUpdate, CVehicle>& info) const;
	void ExtractExtensions(CVehicle* pVehicle, CPacketVehicleUpdate const* pNextPacketData, float interp, const CReplayState& flags) const;
	void FixupVehicleMods(CVehicle* pVehicle, CPacketVehicleUpdate const* pNextPacketData, float interp, const CReplayState& flags) const;
	
	bool ValidatePacket() const
	{
		replayAssertf((GetPacketID() >= PACKETID_CARUPDATE && GetPacketID() <= PACKETID_SUBUPDATE) || GetPacketID() == PACKETID_AMPHAUTOUPDATE || GetPacketID() == PACKETID_AMPHQUADUPDATE || GetPacketID() == PACKETID_SUBCARUPDATE, "Validation of CPacketVehicleUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketVehicleUpdate), "Validation of CPacketVehicleUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && 
				CPacketInterp::ValidatePacket() && 
				CBasicEntityPacketData::ValidatePacket() &&
				((GetPacketID() >= PACKETID_CARUPDATE && GetPacketID() <= PACKETID_SUBUPDATE) || GetPacketID() == PACKETID_AMPHAUTOUPDATE || GetPacketID() == PACKETID_AMPHQUADUPDATE || GetPacketID() == PACKETID_SUBCARUPDATE);
	}

	void				PrintXML(FileHandle handle) const;

private:

	void StoreSpeed(const Vector3& rSpeed);
	void ExtractSpeed(Vector3& rSpeed) const;

	void ClearLightStates()								{	m_LightStates = 0;								}
	bool LoadLightState(u32 uStateIdx) const			{	return (m_LightStates & (1 << uStateIdx)) != 0;	}
	void StoreLightState(bool bState, u32 uStateIdx)	{	m_LightStates |= ((u16)bState << uStateIdx);	}

	void ClearSirenStates()								{	m_SirenStates = 0;								}
	bool LoadSirenState(u32 uStateIdx) const			{	return (m_SirenStates & (1 << uStateIdx)) != 0;	}
	void StoreSirenState(bool bState, u32 uStateIdx)	{	m_SirenStates |= ((u16)bState << uStateIdx);	}

	void ExtractInterpolateableValues(CVehicle* pVehicle, CPacketVehicleUpdate  const* pNextPacket = NULL, float fInterp = 0.0f) const;
	
	void StoreVehicleGadgetData(const audVehicleAudioEntity* pVehicleAudioEntity);
	tPacketSize CreateVehicleGadgetData(audVehicleGadget* pGadget, u8* pPointerDest);
	void ExtractVehicleGadget(audVehicleAudioEntity* pVehicleAudioEntity, CVehicleGadgetDataBase* pGadget) const;

	// Car Alarm States
	enum
	{
		ALARM_OFF		= 0,
		ALARM_TRIGGERED	= 1,
		ALARM_SET		= 2
	};

	//-------------------------------------------------------------------------------------------------

	//-----------------------------------------------------------
	// Compressed Values
	//  - Compressed values are commented in the format shown
	//     below. This helps to determine the best approach for
	//     compression with minimal loss of precision. It also 
	//     helps for debugging purposes incase the range changes
	//     in the future.
	//-----------------------------------------------------------
	// New_Type Name;		// Range - Original Type - Comments
	//-----------------------------------------------------------

	s8		m_SteerAngle;
	s8		m_GasPedal;
	s8		m_Brake;

	s16		m_Speed[3];
	u8		m_VehicleTypeOld;
	u8		m_FireEvo;					// 0 to 1 - f32

	// TODO: are we really keeping all this? Figure this out (Shakes head in despair)
	// CVehicleFlags has grown over 36 bytes so playing back old clips this data would be all out of whack...
	// Fixing by replacing with a hard set 36 bytes
	//CVehicleFlags		m_VehicleFlags;
	char	m_VehicleFlags[36];

	u8		IsPossiblyTouchesWater			: 1;
	u8		m_bHandBrake					: 1;
	u8		m_bRenderScorched				: 1;
	u8		m_HasDamage						: 1;
	u8		m_IsPlayerControlled			: 1;
	u8		m_HasDriver						: 1;    // DL: this might be unused now
	u8		m_bContainsFrag					: 1;	// Used for fragData
	u8		m_EngineStarting				: 1;

	u8		m_bDrawAlpha					: 1;
	u8		m_bDrawCutout					: 1;
	u8		m_bDrawEmissive					: 1;		// AC: Unused
	u8		m_bDontCastShadows				: 1;
	u8		m_bShadowProxy					: 1;
	u8		m_bLightsIgnoreDayNightSetting	: 1;
	u8		m_bLightsCanCastStaticShadows	: 1;
	u8		m_bLightsCanCastDynamicShadows	: 1;

	u8		m_DoorCount;

	// Common Wheel Values
	u8		m_WheelCount;
	u8		m_WheelHitMaterialId;		// 0 to ? - u32 - See phMaterialMgrGta::UnpackMtlId() - ID is masked with 0xff
	u8		m_WheelRadiusFront;			// 0 to 1 - f32 - See \gta\build\common\data\vehicles.ide
	u8		m_WheelRadiusRear;			// 0 to 1 - f32 - See \gta\build\common\data\vehicles.ide

	u16		m_LightStates;
	u8		m_SirenStates;

	// Transmission
	s8		m_Gear;						// 0  to 7 - s16 - See handlingMgr.h for MAX_NUM_GEARS
	u8		m_ClutchRatio;				// 0  to 1 - f32
	s8		m_Throttle;					// -1 to 1 - f32

	f32		m_Revs;
	f32		m_revRatio;
	f32		m_EngineHealth;				// -4000 to 1000 - f32 - See Transmission.h for engine health defines
										//  Note: Need to clamp max at 1000 since script can set it over

	// Horn / Sirens / Car Alarms
	u8		m_CarAlarmState		: 1;	// Only need to know whether it should be on or off
	u8		m_IsSirenForcedOn	: 1;
	u8		m_mustPlaySiren		: 1;
	u8		m_alarmType			: 3;
	u8		m_IsSirenFucked		: 1;
	u8		m_UseSirenForHorn	: 1;
	
	u8		m_bIsTyreDeformOn	: 1;
	u8		m_bDrawDecal		: 1;
	u8		m_PlayerHornOn		: 1;
	CPACKETVEHICLEUPDATE_M_STATUS_TYPE m_status : 3;
	u8		m_bHasDamageBones	: 1;	// Flag to say the packet has damage.
	u8		m_HotWiringVehicle	: 1;

	// Handling Data - (See \gta\build\common\data\handling.dat for possible values)
	u16		m_Mass;						// 0 to 50 000	- f32 - It seems to be rounded to nearest hundred
	f32		m_DriveMaxVel;				// 0 to 53.3	- f32 - (m_fDriveMaxVel = m_fDriveMaxFlatVel / 3.6 * 1.2)

	// New ones to be optimised
	u32		m_EngineSwitchedOnTime;
	float	m_IgnitionHoldTime;
	u8		m_iNumGadgets;

	u8		m_bHasModBoneData	: 1;	
	u8		m_IsInWater			: 1;
	u8		m_IsDummy			: 1;
	u8		m_IsVisible			: 1;
	u8		m_unused0			: 4;

	//  Bits for "extras".
	u32 m_ExtrasTurnedOn;

protected:
	u16		m_VehiclePacketUpdateSize;

	DECLARE_PACKET_EXTENSION(CPacketVehicleUpdate);
	
#if CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	union 
	{
		u32 unused;
		struct // CPacketCarUpdate
		{
			u8 m_bOnRevLimiter : 1; // Moved down from CPacketCarUpdate (PS4 compiler packs this in here).
			u8 m_Pad_CPacketCarUpdate[3];
		};
		struct  // CPacketHeliUpdate
		{
			float		m_WheelAngularVelocity;	// TODO: Compress
		};
		struct // CPacketBicycleUpdate & CPacketBikeUpdate
		{
			s8			m_LeanAngle; // Shared between CPacketBicycleUpdate & CPacketBikeUpdate
			bool		m_IsPedalling;
			u8			m_Pad_CPacketBicycleUpdate[2];
		};
		struct  // CPacketPlaneUpdate
		{
			float m_SectionHealth0;
		};
	};
#endif // CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64

	struct PacketExtensions
	{
		void ClearLightStates()								{	m_lightStates = 0;								}
		bool LoadLightState(u32 uStateIdx) const			{	replayAssert(uStateIdx < 32); return (m_lightStates & (1 << uStateIdx)) != 0;	}
		void StoreLightState(bool bState, u32 uStateIdx)	{	replayAssert(uStateIdx < 32); m_lightStates |= ((u16)bState << uStateIdx);	}

		void ClearSirenStates()								{	m_sirenStates = 0;								}
		bool LoadSirenState(u32 uStateIdx) const			{	replayAssert(uStateIdx < 32); return (m_sirenStates & (1 << uStateIdx)) != 0;	}
		void StoreSirenState(bool bState, u32 uStateIdx)	{	replayAssert(uStateIdx < 32); m_sirenStates |= ((u16)bState << uStateIdx);	}

		//---------------------- Extensions V1 ------------------------/

		// Hi res quaternion to prevent jitter in FP view.
		CPacketQuaternionH m_Quaternion;

		//---------------------- Extensions V2 ------------------------/
		u8					m_scorchValue;
		//---------------------- Extensions V3 ------------------------/
		u8					m_bodyDirtLevel;

		//---------------------- Extensions V4 ------------------------/
		u8					m_hasAlphaOverride;
		u8					m_alphaOverrideValue;

		//---------------------- Extensions V5 ------------------------/
		u8					m_modBoneDataCount;
		u8					m_modBoneDataOffset;

		//---------------------- Extensions V7 ------------------------/
		u8					m_modBoneZeroedCount;

		//---------------------- Extensions V10 ------------------------/
		u8					m_windowsRolledDown;

		//---------------------- Extensions V8 ------------------------/
		u32					m_lightStates;

		//---------------------- Extensions V9 ------------------------/
		float				m_manifoldPressure;

		//---------------------- Extensions V11 ------------------------/
		float				m_uvAnimValue2X;
		float				m_uvAnimValue2Y;

		//---------------------- Extensions V12 ------------------------/
		float				m_emissiveMultiplier;

		//---------------------- Extensions V13 ------------------------/
		bool				m_isBoosting;
		float				m_boostCapacity;
		float				m_boostRemaining;

		//---------------------- Extensions V14 ------------------------/
		float				m_uvAmmoAnimValue2X;
		float				m_uvAmmoAnimValue2Y;

		//---------------------- Extensions V16 ------------------------/
		float				m_specialFlightModeRatio;
		//---------------------- Extensions V17 ------------------------/
		float				m_specialFlightModeAngularVelocity;

		//---------------------- Extensions V19 ------------------------/
		u32					m_sirenStates;

		//---------------------- Extensions V20 ------------------------/
		u8					m_buoyancyState;
	};
};

//////////////////////////////////////////////////////////////////////////
class CPacketVehicleDelete : public CPacketVehicleCreate
{
public:

	void	Store(const CVehicle* pVehicle, const CPacketVehicleCreate* pLastCreatedPacket);
	void	StoreExtensions(const CVehicle* , const CPacketVehicleCreate* pLastCreatedPacket) { CPacketVehicleCreate::CloneExtensionData(pLastCreatedPacket), PACKET_EXTENSION_RESET(CPacketVehicleDelete); }
	void	Cancel()					{	SetReplayID(-1);	}

	void	PrintXML(FileHandle handle) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHICLEDELETE, "Validation of CPacketVehicleDelete Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketVehicleDelete), "Validation of CPacketVehicleDelete extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleCreate::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketVehicleDelete) && (GetPacketID() == PACKETID_VEHICLEDELETE);
	}

	const CPacketVehicleCreate*	GetCreatePacket() const	{ return this; }

private:
	DECLARE_PACKET_EXTENSION(CPacketVehicleDelete);
};

// Allow room for CPacketVehicleCreate extensions.
#define VEHICLE_PACKET_CREATE_EXTENSIONS_MAX_SIZE (sizeof(CPacketVehicleCreate::WHEEL_REFERENCE_VALUES_HEADER) + sizeof(ReplayWheelValuesPair)*NUM_VEH_CWHEELS_MAX + sizeof(CPacketExtension::PACKET_EXTENSION_GUARD) + sizeof(CPacketVehicleCreate::VehicleCreateExtension) + PACKET_EXTENSION_DEFAULT_ALIGNMENT + sizeof(extraVehicleModArrayReplay))
#define MAX_VEHICLE_DELETION_SIZE (sizeof(CPacketVehicleDelete) + VEHICLE_PACKET_CREATE_EXTENSIONS_MAX_SIZE)

class CReplayInterfaceVeh;
class CPacketFragData;
class CPacketFragBoneData;

//////////////////////////////////////////////////////////////////////////
class CVehInterp : public CInterpolator<CPacketVehicleUpdate>
{
	struct CVehInterpData;
public:
	CVehInterp() { Reset(); }

	void Init(const ReplayController&, CPacketVehicleUpdate const*) {	replayFatalAssertf(false, "Shouldn't get into here");	}
	void Init(CPacketVehicleUpdate const* pPrevPacket, CPacketVehicleUpdate const* pNextPacket);
	static void FormCVehInterpData(struct CVehInterpData &dest, CPacketVehicleUpdate const* pVehiclePacket);


	void Reset();


	s32	GetPrevDoorCount() const	{ return m_prevData.m_doorCount; }
	s32	GetNextDoorCount() const	{ return m_nextData.m_doorCount; }

	bool HasPrevFragData() const	{ return m_prevData.m_hasFragData; }
	bool HasNextFragData() const	{ return m_nextData.m_hasFragData; }

	bool GetPrevFragHasDamagedBones() const	{ return m_prevData.m_bFragHasDamagedBones; }
	bool GetNextFragHasDamagedBones() const	{ return m_nextData.m_bFragHasDamagedBones; }

	s32	GetPrevFullPacketSize() const	{ return m_prevData.m_fullPacketSize; }
	s32	GetNextFullPacketSize() const	{ return m_nextData.m_fullPacketSize; }

	s32	GetWheelCount()	const		{ return m_sWheelCount; }
	bool IsWheelDataOld() const		{ return m_bWheelDataOldStyle; }
	bool GetPrevWheelDataOld(s32 wheelID, CWheelFullData &dest) const;
	bool GetNextWheelDataOld(s32 wheelID, CWheelFullData &dest) const;
	CPacketWheel *GetPrevWheelPacket() const { return (CPacketWheel *)m_prevData.m_pWheelPacket; } 
	CPacketWheel *GetNextWheelPacket() const { return (CPacketWheel *)m_nextData.m_pWheelPacket; } 

	CPacketVehicleDoorUpdate const* GetPrevDoorPacket(s32 doorID = 0) const;
	CPacketVehicleDoorUpdate const* GetNextDoorPacket(s32 doorID = 0) const;

	CPacketBase const* GetPrevFragDataPacket() const;
	CPacketBase const* GetNextFragDataPacket() const;

	CPacketBase const* GetPrevFragBonePacket() const;
	CPacketBase const* GetNextFragBonePacket() const;

private:
	bool GetWheelData(const CVehInterpData& data, s32 wheelID, CWheelFullData &dest) const;
	CPacketVehicleDoorUpdate const*	GetDoorPacket(const CVehInterpData& data, s32 doorID) const;

	s32							m_sWheelCount;
	bool						m_bWheelDataOldStyle;

	struct CVehInterpData  
	{
		void Reset()
		{
			m_fullPacketSize	= 0;
			m_pWheelPacket		= NULL;
			m_doorCount			= 0;
			m_pDoorPacket		= NULL;
			m_hasFragData		= false;
			m_pFragDataPacket	= NULL;
			m_bFragHasDamagedBones	= false;
			m_pFragBonePacket	= NULL;
		}
		s32								m_fullPacketSize;
		CPacketBase const*				m_pWheelPacket;
		s32								m_doorCount;
		CPacketVehicleDoorUpdate const*	m_pDoorPacket;
		bool							m_hasFragData;
		CPacketBase const*				m_pFragDataPacket;
		bool							m_bFragHasDamagedBones;
		CPacketBase const*				m_pFragBonePacket;
	} m_prevData, m_nextData;
};

class CVehicleGadgetDataBase
{
public: 
	CVehicleGadgetDataBase() {}
	audVehicleGadget::audVehicleGadgetType GetGadgetType() const { return (audVehicleGadget::audVehicleGadgetType)m_GadgetType; }
	void SetGadgetType(audVehicleGadget::audVehicleGadgetType gadgetType) { m_GadgetType = (u8)gadgetType; }
	u8 GetPacketSize() { return m_PacketSize; }

protected:
	u8 m_PacketSize;
private:
	u8 m_GadgetType;
};

class CVehicleGadgetTurretData : public CVehicleGadgetDataBase
{
public:
	CVehicleGadgetTurretData();
	
	void Store(const rage::Vector3& turretAngularVelocity) 
	{
		m_TurretAngularVelocity.Store(turretAngularVelocity);
		m_PacketSize = sizeof(CVehicleGadgetTurretData);
		SetGadgetType(audVehicleGadget::AUD_VEHICLE_GADGET_TURRET);
	}
	void GetAngularVeloctiy(Vector3& velocity) { m_TurretAngularVelocity.Load(velocity); }

private:
	CPacketVector3 m_TurretAngularVelocity;
};

class CVehicleGadgetForkData : public CVehicleGadgetDataBase
{
public:
	CVehicleGadgetForkData();

	void Store(const f32 forkliftSpeed, const f32 desiredAcceleration) 
	{
		m_ForkliftSpeed = forkliftSpeed;
		m_DesiredAcceleration = desiredAcceleration;
		m_PacketSize = sizeof(CVehicleGadgetForkData);
		SetGadgetType(audVehicleGadget::AUD_VEHICLE_GADGET_FORKS);
	}
	f32 GetSpeed() { return m_ForkliftSpeed; }
	f32 GetDesiredAcceleration() { return m_DesiredAcceleration; }

private:
	f32 m_ForkliftSpeed;
	f32 m_DesiredAcceleration;
};

class CVehicleGadgetGrapplingHookData : public CVehicleGadgetDataBase
{
public:
	CVehicleGadgetGrapplingHookData();

	void Store(const bool entityAttached, const f32 towVehicleAngle, const Vector3& hookPosition) 
	{
		m_EntityAttached = entityAttached;
		m_TowedVehicleAngle = towVehicleAngle;
		m_HookPosition.Store(hookPosition);
		m_PacketSize = sizeof(CVehicleGadgetGrapplingHookData);
		SetGadgetType(audVehicleGadget::AUD_VEHICLE_GADGET_GRAPPLING_HOOK);
	}
	bool GetEntityAttached() { return m_EntityAttached; }
	f32 GetTowVehicleAngle() { return m_TowedVehicleAngle; }
	void GetHookPosition(Vector3& hookPosition) { m_HookPosition.Load(hookPosition); }

private:
	f32 m_TowedVehicleAngle;
	CPacketVector3 m_HookPosition;
	bool m_EntityAttached;
};

class CVehicleGadgetTowTruckArmData : public CVehicleGadgetDataBase
{
public:
	CVehicleGadgetTowTruckArmData();

	void Store(const f32 forkliftSpeed, const f32 desiredAcceleration, const f32 towVehicleAngle, const Vector3& hookPosition ) 
	{
		m_ForkliftSpeed = forkliftSpeed;
		m_DesiredAcceleration = desiredAcceleration;
		m_TowedVehicleAngle = towVehicleAngle;
		m_HookPosition.Store(hookPosition);
		m_PacketSize = sizeof(CVehicleGadgetTowTruckArmData);
		SetGadgetType(audVehicleGadget::AUD_VEHICLE_GADGET_TOWTRUCK_ARM);
	}
	f32 GetSpeed() { return m_ForkliftSpeed; }
	f32 GetDesiredAcceleration() { return m_DesiredAcceleration; }
	f32 GetTowVehicleAngle() { return m_TowedVehicleAngle; }
	void GetHookPosition(Vector3& hookPosition) { m_HookPosition.Load(hookPosition); }

private:
	f32 m_ForkliftSpeed;
	f32 m_DesiredAcceleration;
	f32 m_TowedVehicleAngle;
	CPacketVector3 m_HookPosition;
};

class CVehicleGadgetDiggerData : public CVehicleGadgetDataBase
{
public:
	CVehicleGadgetDiggerData();

	void Store(int i , const f32 jointSpeed, const f32 jointDesiredAcceleration) 
	{
		m_JointSpeed[i] = jointSpeed;
		m_JointDesiredAcceleration[i] = jointDesiredAcceleration;
		m_PacketSize = sizeof(CVehicleGadgetDiggerData);
		SetGadgetType(audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER);
	}

	f32 GetJointSpeed(s16 index) { return m_JointSpeed[index]; }
	f32 GetJointDesiredAcceleration(s16 index) { return m_JointDesiredAcceleration[index]; }

private:
	f32 m_JointSpeed[DIGGER_JOINT_MAX];
	f32 m_JointDesiredAcceleration[DIGGER_JOINT_MAX];
};


//////////////////////////////////////////////////////////////////////////
class CPacketTowTruckArmRotate : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketTowTruckArmRotate(float angle);

	void			Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TOWTRUCKARMROTATE, "Validation of CPacketTowTruckArmRotate Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_TOWTRUCKARMROTATE;
	}

private:
	float m_Angle;
};

//////////////////////////////////////////////////////////////////////////
class CPacketVehicleJumpRechargeTimer : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleJumpRechargeTimer(float rechargeTimer, u32 startOffset, CAutomobile::ECarParachuteState parachuteState);

	void			Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_VEHICLEJUMPRECHARGE, "Validation of CPacketVehicleJumpRechargeFraction Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_VEHICLEJUMPRECHARGE;
	}

private:
	float m_RechargeTimer;
	u32 m_StartOffset;
	CAutomobile::ECarParachuteState m_ParachuteState;
};



//////////////////////////////////////////////////////////////////////////
class RightHandWheelBones : public BoneModifier
{
public:
	RightHandWheelBones(CVehicle *pVehicle);
	~RightHandWheelBones() {}
	bool ShouldModifyBoneLocalRotation(u32 boneIndex);
	Quaternion ModifyBoneLocalRotation(Quaternion &localRotation, Matrix34 const &boneParentMtx, Matrix34 const &entityMtx);

public:
	u32 m_NoOfRightHandWheelBones;
	u32 m_RightHandWheelBones[NUM_VEH_CWHEELS_MAX];
};



static const u8 extraVehicleModArrayReplay[] = 
{
	VMT_PLTHOLDER,
	VMT_PLTVANITY,
 
	VMT_INTERIOR1,
	VMT_INTERIOR2,
	VMT_INTERIOR3,
	VMT_INTERIOR4,
	VMT_INTERIOR5,
	VMT_SEATS,
	VMT_STEERING,
	VMT_KNOB,
	VMT_PLAQUE,
	VMT_ICE,
 
	VMT_TRUNK,
	VMT_HYDRO,
 
	VMT_ENGINEBAY1,
	VMT_ENGINEBAY2,
	VMT_ENGINEBAY3,
 
	VMT_CHASSIS2,
	VMT_CHASSIS3,
	VMT_CHASSIS4,
	VMT_CHASSIS5,
 
	VMT_DOOR_L,
	VMT_DOOR_R,
 
	VMT_LIGHTBAR,

	VMT_LIVERY_MOD,

	// Add any new ones here!
};

// If you've changed the contents of eVehicleModType then you need to also add the new values to the END of the above array.
// Please buddy the change with Andrew Collinson as this could break Replay clips!
CompileTimeAssert(sizeof(extraVehicleModArrayReplay) == (VMT_MAX - VMT_MAX_OLD));
CompileTimeAssert(sizeof(extraVehicleModArrayReplay) < 256);

// If either of these have fired then you've been very bad and added an element to the wrong array.
// Use the extraVehicleModArrayReplay above!
CompileTimeAssert(sizeof(oldVehicleModArrayReplay1) == sizeof(u8) * 11);
CompileTimeAssert(sizeof(oldVehicleModArrayReplay2) == sizeof(u8) * 14);

#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif  // VEHICLEPACKET_H
