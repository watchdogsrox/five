//=====================================================================================================
// name:		ObjectPacket.h
// description:	Object replay packet.
//=====================================================================================================

#ifndef INC_OBJECTPACKET_H_
#define INC_OBJECTPACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/ReplaySupportClasses.h"
#include "control/replay/Misc/InterpPacket.h"
#include "Control/Replay/PacketBasics.h"
#include "control/replay/ReplayExtensions.h"
#include "Control/replay/Entity/FragmentPacket.h"
#include "objects/Door.h"
#include "streaming/streamingdefs.h"


#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

class CObject;
namespace rage
{ class Vector3; }

//////////////////////////////////////////////////////////////////////////
const u8 ObjFadeOutBit		= 5;
const u8 RenderDamagedBit	= 4;
const u8 ContainsFragBit	= 3;
const u8 IsPropOverrideBit	= 2;

const u8 FragBoneCountU8	= 0;

//////////////////////////////////////////////////////////////////////////
// Packet for creation of an Object
const tPacketVersion CPacketObjectCreate_V1 = 1;
const tPacketVersion CPacketObjectCreate_V2 = 2;
const tPacketVersion CPacketObjectCreate_V3 = 3;
class CPacketObjectCreate : public CPacketBase, public CPacketEntityInfo<1>
{
public:
	CPacketObjectCreate();

	void	Store(const CObject* pObject, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId = PACKETID_OBJECTCREATE, tPacketSize packetSize = sizeof(CPacketObjectCreate));
	void	StoreExtensions(const CObject* pObject, bool firstCreationPacket, u16 sessionBlockIndex);
	void	CloneExtensionData(const CPacketObjectCreate* pSrc) { if(pSrc) { CLONE_PACKET_EXTENSION_DATA(pSrc, this, CPacketObjectCreate); } }

	void	SetFrameCreated(const FrameRef& frame)	{	m_FrameCreated = frame;	}
	FrameRef GetFrameCreated() const				{	return m_FrameCreated; }
	bool	IsFirstCreationPacket() const			{	return m_firstCreationPacket;	}

	void	Reset()						{ SetReplayID(-1); m_FrameCreated = FrameRef::INVALID_FRAME_REF; m_trafficLightData.m_initialTrafficLightsSet = /*m_trafficLightData.m_initialTrafficLightsVerified =*/ false;	}

	u32		GetModelNameHash() const	{	return m_ModelNameHash;	}
	strLocalIndex		GetMapTypeDefIndex() const	{	return (strLocalIndex)m_MapTypeDefIndex; }
	void	SetMapTypeDefIndex(strLocalIndex index) { m_MapTypeDefIndex = index.Get();}
	u32		GetMapTypeDefHash() const { return m_MapTypeDefHash; }
	bool	UseMapTypeDefHash() const { return GetPacketVersion() >= CPacketObjectCreate_V3; }
	u32		GetTintIndex() const		{	return m_TintIndex;		}

	u32		GetMapHash() const				{	return m_mapHash;	}

	fwInteriorLocation	GetInteriorLocation() const	{ return m_interiorLocation.GetInteriorLocation(); }

 	s32		GetFragParentID() const			{	return m_fragParentID;		}
 	bool	HasFragParent() const			{	return (m_fragParentID >= 0); }

	u8		IsPropOverride() const				{	return m_bIsPropOverride;	}
	u8		ShouldPreloadProp() const			{	return m_preloadPropInfo;	}
	s32		GetParentModelID() const			{	return m_parentModelID;		}
	u32		GetPropHash() const					{	return m_uPropHash;			}
	u32		GetTexHash() const					{	return m_uTexHash;			}
	u8		GetAnchorID() const					{	return m_uAnchorID;			}
	bool	GetIsADoor() const					{	return (bool)m_bIsADoor;	}
	bool	GetIsAWheel() const					{	return (bool)m_isAWheel;	}
	bool	GetIsAParachute() const				{	return (bool)m_isAParachute;}

	bool	GetInitialTrafficLightsSet() const	{	return m_trafficLightData.m_initialTrafficLightsSet;	}
	bool	GetInitialTrafficLightsVerified() const	{	return m_trafficLightData.m_initialTrafficLightsVerified;	}
	void	SetInitialTrafficLightsVerified()	{	m_trafficLightData.m_initialTrafficLightsVerified = true;	}
	const char*	GetTrafficLightCommands() const		{	return m_trafficLightData.m_initialTrafficLights;	}
	void	SetInitialTrafficLightCommands(const char* commands)
	{
		for(int i = 0; i < REPLAY_NUM_TL_CMDS; ++i)
		{
			m_trafficLightData.m_initialTrafficLights[i] = commands[i];
		}
		m_trafficLightData.m_initialTrafficLightsSet = true;
	}

	void	LoadMatrix(Matrix34& matrix) const	{	m_posAndRot.LoadMatrix(matrix);	}

	u32		GetWeaponHash() const				{	return m_weaponHash;			}
	bool	IsWeapon() const					{	return GetWeaponHash() != 0;	}
	u8		GetWeaponTint() const				{	return m_weaponTint;			}
	u32		GetWeaponComponentHash() const		{	return m_weaponComponentHash;	}
	bool    IsWeaponComponent() const			{   return GetWeaponComponentHash() != 0;	}
	s32		GetWeaponParentID() const			{	return m_weaponParentID;		}
	bool	HasWeaponParent() const				{	return (m_weaponParentID >= 0); }
	CReplayID	GetParentID() const;
	Quaternion GetDoorStartRot() const;
	u8		GetWeaponComponentTint() const;

	bool	ValidatePacket() const
	{
		replayAssertf((GetPacketID() == PACKETID_OBJECTCREATE) || (GetPacketID() == PACKETID_OBJECTDELETE), "Validation of CPacketObjectCreate failed");
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketObjectCreate), "Validation of CPacketObjectCreate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketObjectCreate) && ((GetPacketID() == PACKETID_OBJECTCREATE) || (GetPacketID() == PACKETID_OBJECTDELETE));
	}

	void	PrintXML(FileHandle handle) const;

	// CPacketObjectCreateExtensions_V1
	struct ObjectCreateExtension
	{
		CReplayID	m_parentID;

		// CPacketObjectCreateExtensions_V2
		CPacketQuaternionH m_qDoorStartRot;

		// CPacketObjectCreateExtensions_V3
		u8 m_weaponComponentTint;
	};

private:
	FrameRef		m_FrameCreated;
	bool			m_firstCreationPacket;

	u32				m_ModelNameHash;
	union
	{
		s32			m_MapTypeDefIndex; //note: only 12 bit of this is used
		u32			m_MapTypeDefHash;
	};
	u32				m_TintIndex;

	u32				m_mapHash;

	CReplayInteriorProxy	m_interiorLocation;

	s32				m_parentModelID;
	s32				m_fragParentID;
	u32				m_uPropHash;
	u32				m_uTexHash;

	u32				m_weaponComponentHash;
	s32				m_weaponParentID;
	u32				m_weaponHash;
	u8				m_weaponTint;

	u8				m_uAnchorID;

	u8				m_bIsPropOverride		: 1;
	u8				m_bIsWeaponOrComponent	: 1;
	u8				m_bIsADoor				: 1;
	u8				m_preloadPropInfo		: 1;
	u8				m_isAWheel				: 1;
	u8				m_isAParachute			: 1;
	u8				m_unused0				: 2;

	CPacketPositionAndQuaternion	m_posAndRot;

	struct  
	{
		u8		m_initialTrafficLightsSet			: 1;
		u8		m_initialTrafficLightsVerified		: 1;
		u8		m_unused							: 6;
		char	m_initialTrafficLights[REPLAY_NUM_TL_CMDS];
	} m_trafficLightData;

	DECLARE_PACKET_EXTENSION(CPacketObjectCreate);
};

//////////////////////////////////////////////////////////////////////////
class CPacketObjectUpdate : public CPacketBase, public CPacketInterp, public CBasicEntityPacketData
{
public:

	void	Store(CObject* pObject, bool firstUpdatePacket);
	void	StoreExtensions(CObject* pObject, bool firstUpdatePacket);
	void	Store(eReplayPacketId uPacketID, u32 packetSize, CObject* pObject, bool firstUpdatePacket);
	void	PreUpdate(const CInterpEventInfo<CPacketObjectUpdate, CObject>& eventInfo) const;
	void	Extract(const CInterpEventInfo<CPacketObjectUpdate, CObject>& eventInfo) const;
	void	ExtractExtensions(CObject* pObject, const CInterpEventInfo<CPacketObjectUpdate, CObject>& eventInfo) const;

	bool	HasFragInst() const			{	return GetPadBool(ContainsFragBit); 	}
	u8		GetFragBoneCount() const	{	return GetPadU8(FragBoneCountU8);		}
	u8		GetBoneCount() const		{	return m_boneCount;						}

	float   IsUsingRecordAnimPhase() const { return m_AnimatedBuildingPhase != -1;	}
	void	UpdateAnimatedBuilding(CObject* pReplayObject, CObject* pObject, float fPhase, float fRate) const;

	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_OBJECTUPDATE || GetPacketID() == PACKETID_OBJECTDOORUPDATE, "Validation of CPacketObjectUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && 
				CPacketInterp::ValidatePacket() && 
				CBasicEntityPacketData::ValidatePacket() &&
				(GetPacketID() == PACKETID_OBJECTUPDATE || GetPacketID() == PACKETID_OBJECTDOORUPDATE);
	}

	void	PrintXML(FileHandle handle) const;

private:
	void	SetObjectDestroyed(CObject* pObject) const;

	u8		m_boneCount;
	u8		m_weaponTint;
	u8		m_isPlayerWeapon	: 1;
	u8		m_hasEnvCloth		: 1; // Added with g_CPacketObjectUpdate_Extensions_V1.
	u8		m_hasAlphaOverride	: 1;
	u8		m_unused0			: 5;
	float	m_AnimatedBuildingPhase;
	float	m_AnimatedBuildingRate;	

	DECLARE_PACKET_EXTENSION(CPacketObjectUpdate);

	float	m_scaleXY;
	float	m_scaleZ;

	struct PacketExtensions
	{
		//---------------------- Extensions V1 ------------------------/
		u8		m_alphaOverrideValue;

		//---------------------- Extensions V2 ------------------------/
		u8		m_muzzlePosOffset;

		//---------------------- Extensions V3 ------------------------/
		u8		m_vehModSlot;

		u8		m_unused0;
	};

public:
	// Hack for B*2341690!
	static atHashString ms_LuxorPlaneObjects[];
	static bool		IsLuxorPlaneObjectByModelNameHash(CEntity *pObject);
	static bool		IsLuxorPlaneObjectByAttachment(CEntity *pObject);
	static bool		ShouldIgnoreWarpingFlags(CEntity *pObject);
	static void		UpdateBoundingBoxForLuxorPlane(CObject *pObject);
};


//////////////////////////////////////////////////////////////////////////
// Packet for deletion of an Object
class CPacketObjectDelete : public CPacketObjectCreate
{
public:
	void	Store(const CObject* pObject, const CPacketObjectCreate* pLastCreatedPacket);
	void	StoreExtensions(const CObject*, const CPacketObjectCreate* pLastCreatedPacket) { CPacketObjectCreate::CloneExtensionData(pLastCreatedPacket); PACKET_EXTENSION_RESET(CPacketObjectDelete); }
	void	Cancel()					{	SetReplayID(-1);	}

	bool	IsConvertedToDummyObject() const		{	return GetPacketVersion() >= CPacketObjectCreate_V1 ? m_convertedToDummyObject : false;	}

	void	PrintXML(FileHandle handle) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_OBJECTDELETE, "Validation of CPacketObjectDelete Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketObjectDelete), "Validation of CPacketObjectDelete extensions Failed!, 0x%08X", GetPacketID());
		return CPacketObjectCreate::ValidatePacket() && (GetPacketID() == PACKETID_OBJECTDELETE);
	}

	const CPacketObjectCreate*	GetCreatePacket() const	{ return this; }
private:
	DECLARE_PACKET_EXTENSION(CPacketObjectDelete);

	u8		m_convertedToDummyObject	: 1;
	u8		m_unused		: 7;
	u8		m_unusedA[3];
};

#define MAX_OBJECT_DELETION_SIZE  sizeof(CPacketObjectDelete) + sizeof(CPacketObjectCreate::ObjectCreateExtension) + sizeof(CPacketExtension::PACKET_EXTENSION_GUARD)

//////////////////////////////////////////////////////////////////////////

class CPacketDoorUpdate : public CPacketObjectUpdate
{
public:
	void	Store(CDoor* pDoor, bool storeCreateInfo);
	void	StoreExtensions(CDoor* pDoor, bool storeCreateInfo) { PACKET_EXTENSION_RESET(CPacketDoorUpdate); CPacketObjectUpdate::StoreExtensions(pDoor, storeCreateInfo); (void)pDoor; (void)storeCreateInfo; }
	void	Extract(const CInterpEventInfo<CPacketDoorUpdate, CDoor>& info) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_OBJECTDOORUPDATE, "Validation of CPacketDoorUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketObjectUpdate::ValidatePacket() && 
			GetPacketID() == PACKETID_OBJECTDOORUPDATE;
	}

private:
	f32 m_CurrentVelocity;
	f32	m_Heading;
	f32	m_OriginalHeading;
	bool m_ReplayDoorUpdating;
	DECLARE_PACKET_EXTENSION(CPacketDoorUpdate);
};

//=====================================================================================================
//=====================================================================================================
class CPacketAnimatedObjBoneData : public CPacketBase
{

public:

	void Store(const CObject* pObject);
	void StoreExtensions(const CObject* pObject) { PACKET_EXTENSION_RESET(CPacketAnimatedObjBoneData); (void)pObject; }

	void Extract(CObject* pObject, CPacketAnimatedObjBoneData const* pNextPacket, float fInterpValue, bool bUpdateWorld = true, const atArray<sInterpIgnoredBone>* noInterpBones = nullptr) const;

	static s32 EstimatePacketSize(const CObject* pEntity);

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ANIMATEDOBJDATA, "Validation of CPacketAnimatedObjBoneData Failed!, 0x%08X", GetPacketID());
#if REPLAY_GUARD_ENABLE
		int* pEnd = (int*)((u8*)this + GetPacketSize() - sizeof(int));
		replayAssertf((*pEnd == REPLAY_GUARD_END_DATA), "Validation of CPacketAnimatedObjBoneData Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_ANIMATEDOBJDATA && CPacketBase::ValidatePacket() && (*pEnd == REPLAY_GUARD_END_DATA);
#else
		return GetPacketID() == PACKETID_ANIMATEDOBJDATA && CPacketBase::ValidatePacket();
#endif // REPLAY_GUARD_ENABLE
	}

	CSkeletonBoneData<CPacketQuaternionL> *GetBoneData() const { return (CSkeletonBoneData<CPacketQuaternionL> *)((ptrdiff_t)this + (ptrdiff_t)this->m_OffsetToBoneData); }

	u16	m_OffsetToBoneData;
	DECLARE_PACKET_EXTENSION(CPacketAnimatedObjBoneData);

private:
	static float GetZeroScaleEpsilon(const CObject *pObject);

	typedef struct EPSILON_AND_HASH
	{
		float			epsilon;
		atHashString	hashString;
	} EPSILON_AND_HASH;

	static EPSILON_AND_HASH ms_ZeroScaleEpsilonAndHash[];
};


class CPacketFragData;
class CPacketFragData;
class CPacketFragBoneData;

//////////////////////////////////////////////////////////////////////////
class CObjInterp : public CInterpolator<CPacketObjectUpdate>
{
public:
	CObjInterp() { Reset(); }
	virtual ~CObjInterp() {}

	void Init(const ReplayController& controller, CPacketObjectUpdate const* pPrevPacket);

	void Reset();

	bool HasPrevFragData() const	{ return m_prevData.m_hasFragData; }
	bool HasNextFragData() const	{ return m_nextData.m_hasFragData; }

	s32	GetPrevFragBoneCount() const	{ return m_prevData.m_fragBoneCount; }
	s32	GetNextFragBoneCount() const	{ return m_nextData.m_fragBoneCount; }

	s32	GetPrevFullPacketSize() const	{ return m_prevData.m_fullPacketSize; }
	s32	GetNextFullPacketSize() const	{ return m_nextData.m_fullPacketSize; }

	CPacketBase const* GetPrevFragDataPacket()	{ return m_prevData.m_pFragDataPacket; }
	CPacketBase const* GetNextFragDataPacket()	{ return m_nextData.m_pFragDataPacket; }

	CPacketFragBoneData const* GetPrevFragBonePacket();
	CPacketFragBoneData const* GetNextFragBonePacket();

	s32	GetPrevAnimObjBoneCount() const	{ return m_prevData.m_animObjBoneCount; }
	s32	GetNextAnimObjBoneCount() const	{ return m_nextData.m_animObjBoneCount; }

	CPacketAnimatedObjBoneData const* GetPrevAnimObjBonePacket();
	CPacketAnimatedObjBoneData const* GetNextAnimObjBonePacket();

protected:

	struct CObjInterpData;
	void	HookUpAnimBoneAndFrags(CPacketObjectUpdate const* pObjectPacket, CObjInterpData& data);

	struct CObjInterpData  
	{
		void Reset()
		{
			memset(this, 0, sizeof(CObjInterpData));
		}
		s32							m_fullPacketSize;
		bool						m_hasFragData;
		CPacketBase const*			m_pFragDataPacket;
		s32							m_fragBoneCount;
		CPacketFragBoneData const*	m_pFragBonePacket;
		s32							m_animObjBoneCount;
		CPacketAnimatedObjBoneData const*	m_pAnimObjBonePacket;
	} m_prevData, m_nextData;
};


//////////////////////////////////////////////////////////////////////////
struct mapObjectID 
{
	mapObjectID() { objectHash = 0;}
	mapObjectID(u32 hash) { objectHash = hash; }

	u32 objectHash;

	bool operator==(const mapObjectID& other) const
	{
		return objectHash == other.objectHash;
	}
};

class CPacketHiddenMapObjects : public CPacketBase
{
public:

	void Store(const atArray<mapObjectID>& ids);
	void StoreExtensions(const atArray<mapObjectID>& ids) { PACKET_EXTENSION_RESET(CPacketHiddenMapObjects); (void)ids; };
	void Extract(atArray<mapObjectID>& ids) const;

	void	PrintXML(FileHandle handle) const;
private:

	u8*	GetIDsPtr()					{	return (u8*)this + m_sizeOfBasePacket;	}
	const u8*	GetIDsPtr() const	{	return (u8*)this + m_sizeOfBasePacket;	}
	
	u8	m_sizeOfBasePacket;
	u32 m_hiddenMapObjectCount;
	DECLARE_PACKET_EXTENSION(CPacketHiddenMapObjects);
};

class CPacketObjectSniped : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketObjectSniped();

	void	Extract(CEventInfo<CObject>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CObject>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfo<CObject>& info) const;
	bool	NeedsEntitiesForExpiryCheck() const { return true; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_OBJECT_SNIPED, "Validation of CPacketObjectSniped Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_OBJECT_SNIPED;
	}
};


class CPacketSetDummyObjectTint : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSetDummyObjectTint(Vec3V pos, u32 hash, int tint, CDummyObject* pDummy);

	void	Extract(CEventInfo<void>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfo<void>& info) const;
	bool	NeedsEntitiesForExpiryCheck() const { return false; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SETDUMMYOBJECTTINT, "Validation of CPacketSetDummyObjectTint Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SETDUMMYOBJECTTINT;
	}


private:
	void	SetTintOnEntity(CEntity* pEntity) const;

	CPacketVector3		m_position;
	u32					m_objectModelHash;
	int					m_tint;
	CDummyObject*		m_pDummy;	// Only used for expiry...NOT on playback

	static atMap<CDummyObject*, int>	sm_dummyObjects;
};


class CPacketSetObjectTint : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketSetObjectTint(int tint, CObject* pObject);

	void	Extract(CEventInfo<CObject>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CObject>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfo<CObject>& info) const;
	bool	NeedsEntitiesForExpiryCheck() const { return true; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SETOBJECTTINT, "Validation of CPacketSetObjectTint Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SETOBJECTTINT;
	}


private:
	void	SetTintOnEntity(CEntity* pEntity, bool forwards) const;

	int					m_tint;
	mutable u32			m_prevTint;

	static atMap<CReplayID, int>	sm_objects;
};


class CPacketOverrideObjectLightColour : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketOverrideObjectLightColour(Color32 previousCol, Color32 newCol);

	void	Extract(CEventInfo<CObject>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CObject>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfo<CObject>& info) const;
	bool	NeedsEntitiesForExpiryCheck() const { return true; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_OVERRIDEOBJECTLIGHTCOLOUR, "Validation of CPacketOverrideObjectLightColour Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_OVERRIDEOBJECTLIGHTCOLOUR;
	}

	static void StoreColourOverride(CReplayID objectID, Color32 colour);
	static bool GetColourOverride(CReplayID objectID, Color32*& colour);
	static void RemoveColourOverride(CReplayID objectID);
	static int GetColourOverrideCount();
private:
	Color32 m_colour;
	Color32 m_prevColour;

	static atMap<int, Color32>	sm_colourOverrides;
};




class CPacketSetBuildingTint : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSetBuildingTint(Vec3V pos, float radius, u32 hash, int tint, CBuilding* pBuilding);

	void	Extract(CEventInfo<void>& pEventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfo<void>& info) const;
	bool	NeedsEntitiesForExpiryCheck() const { return false; }

	void	PrintXML(FileHandle handle) const { CPacketEvent::PrintXML(handle); }
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SETBUILDINGTINT, "Validation of CPacketSetBuildingTint Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SETBUILDINGTINT;
	}

	static void ProcessDeferredPackets()
	{
		for(int i = 0; i < sm_deferredPackets.GetCount();)
		{
			sm_tintSuccess = false;
			CEventInfo<void> info;
			sm_deferredPackets[i]->Extract(info);
			if(sm_tintSuccess)
			{
				sm_deferredPackets.Delete(i);
			}
			else
			{
				++i;
			}
		}
	}
	static void ClearDeferredPackets()
	{
		sm_deferredPackets.clear();
	}

private:
	void	SetTintOnEntity(CEntity* pEntity) const;

	CPacketVector3		m_position;
	float				m_radius;
	u32					m_objectModelHash;
	int					m_tint;
	CBuilding*			m_pBuilding;	// Only used for expiry...NOT on playback

	static atMap<CBuilding*, int>	sm_buildings;

	static bool sm_tintSuccess;
	static atArray<CPacketSetBuildingTint const*> sm_deferredPackets;
};

#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_OBJECTPACKET_H_
