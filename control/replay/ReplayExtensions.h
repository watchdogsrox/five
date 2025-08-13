#ifndef _REPLAY_EXTENSIONS_H_
#define _REPLAY_EXTENSIONS_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplaySupportClasses.h"
#include "entity/extensionlist.h"
#include "Objects/object.h"
#include "Objects/DummyObject.h"
#include "scene/Entity.h"
#include "scene/Physical.h"
#include "peds/rendering/PedDamage.h"
#include "Control/Replay/Vehicle/ReplayWheelValues.h"


// used in PedPacket.h too
struct ReticulePacketInfo
{
	ReticulePacketInfo()
		: m_iWeaponHash(0)
		, m_iZoomLevel(0)
		, m_bIsFirstPerson(0)
		, m_bReadyToFire(0)
		, m_bHit(0)
	{
	}

	u32 m_iWeaponHash;
	u8	m_iZoomLevel;
	u8	m_bIsFirstPerson	: 1;
	u8	m_bReadyToFire		: 1;
	u8	m_bHit				: 1;
};

struct HUDOverlayPacketInfo
{
	HUDOverlayPacketInfo()
		: m_allOverlays(0)
	{
	}

	union
	{
		u8 m_allOverlays;
		struct
		{
			u8 m_telescopeOn	: 1;
			u8 m_binocularsOn	: 1;
			u8 m_iFruitOn		: 1;
			u8 m_badgerOn		: 1;
			u8 m_facadeOn		: 1;
			u8 m_turretCamOn	: 1;
			u8 m_droneCamOn		: 1;
		} m_overlay;
		u32 m_pad;
	};
};


class IReplayExtensionsController
{
public:
	virtual ~IReplayExtensionsController(){};
	virtual void Remove(CPhysical* pEntity) = 0;
	virtual bool HasExtension(const CPhysical* pEntity) = 0;
	virtual s32	GetNumUsed() = 0;
	virtual bool IsForEntity(const CPhysical* pEntity) = 0;

	virtual void GetStatusString(char*) = 0;

	
};

template<typename EXTTYPE>
class CReplayExtensionsController : public IReplayExtensionsController
{
public:
	virtual ~CReplayExtensionsController()
	{
		replayAssertf(EXTTYPE::GetNumUsed() == 0, "Still some extensions used here... %u", EXTTYPE::GetNumUsed());
	}

	void Remove(CPhysical* pEntity)
	{
		EXTTYPE::Remove(pEntity);
	}

	bool HasExtension(const CPhysical* pEntity)
	{
		return EXTTYPE::HasExtension(pEntity);
	}

	s32	GetNumUsed()
	{
		return EXTTYPE::GetNumUsed();
	}

	bool IsForEntity(const CPhysical* pEntity)
	{
		return EXTTYPE::IsForEntity(pEntity);
	}

	void GetStatusString(char* pString)
	{
		sprintf(pString, "Extension: %s, num used: %u", EXTTYPE::GetName(), EXTTYPE::GetNumUsed());
	}
};


class CReplayExtensionManager
{
public:
	static void	Cleanup()
	{
		sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
		for(int i = 0; i < m_extensionControllers.GetCount(); ++i)
		{
			delete m_extensionControllers[i];
		}
        m_extensionControllers.Reset();
	}

	template<typename EXTTYPE>
	static void	RegisterExtension(const char* name, u32 capacity)
	{
		EXTTYPE::Init(name, capacity);

		sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
		m_extensionControllers.Append() = rage_new CReplayExtensionsController<EXTTYPE>();
	}


	static void	RemoveReplayExtensions(CPhysical* pEntity)
	{
		for(int i = 0; i < m_extensionControllers.GetCount(); ++i)
		{
			if(m_extensionControllers[i] != NULL && m_extensionControllers[i]->IsForEntity(pEntity))
				m_extensionControllers[i]->Remove(pEntity);
		}
	}

	static bool HasAnyExtension(CPhysical* pEntity)
	{
		for(int i = 0; i < m_extensionControllers.GetCount(); ++i)
		{
			if(m_extensionControllers[i] != NULL && m_extensionControllers[i]->IsForEntity(pEntity) && m_extensionControllers[i]->HasExtension(pEntity) == true)
				return true;
		}
		return false;
	}

	static bool CheckExtensionsAreClean()
	{
		for(int i = 0; i < m_extensionControllers.GetCount(); ++i)
		{
			if(m_extensionControllers[i] != NULL)
			{
				replayFatalAssertf(m_extensionControllers[i]->GetNumUsed() == 0, "Extensions remain when they shouldn't (%d)", m_extensionControllers[i]->GetNumUsed());
			}
		}
		return true;
	}

	static bool GetExtensionStatusString(char* pStr, int index)
	{
		if(index >= m_extensionControllers.GetCount())
			return false;

		m_extensionControllers[index]->GetStatusString(pStr);
		return true;
	}

private:
	static atFixedArray<IReplayExtensionsController*, 16>	m_extensionControllers;
};




template<typename EXTTYPE>
class ReplayExtensionBase
{
public:
	ReplayExtensionBase()
	: m_inUse(false)
	{}
	virtual ~ReplayExtensionBase()	{}

	virtual void Reset()	{}

	static void Init(const char* name, u32 capacity)
	{
		if(m_elements.GetCapacity() != (s32)capacity)
		{
			m_elements.Reset();
			m_elements.Resize(capacity);
		}
		m_name = name;
	}

	static EXTTYPE* Add(CEntity* pEntity)
	{
		replayFatalAssertf(EXTTYPE::IsForEntity(pEntity), "Attempting to add extension to invalid entity");
		replayFatalAssertf(CReplayMgr::ShouldRegisterElement(pEntity), "Attempting to add extension for an entity that should not be registered");

		EXTTYPE* pExt = GetExtInternal(pEntity);
		if(!pExt)
		{
			const CPhysical* pPhysical = pEntity->GetIsPhysical() ? static_cast<const CPhysical*>(pEntity) : NULL;
			if(!pPhysical)
			{
				replayAssertf(false, "Trying to add an extension (%s) to a non-physical...", EXTTYPE::GetName());
			}
			else
			{
				replayAssertf(false, "Failed to add Extension (%s) to entity - replayID: 0x%X, PopType:%u, OwnedBy:%u", EXTTYPE::GetName(), pPhysical->GetReplayID().ToInt(), (u32)((CDynamicEntity*)pEntity)->PopTypeGet(), pEntity->GetOwnedBy());
			}
			
			return NULL;
		}
		
		Assert(pExt->m_inUse == false);

		pExt->m_inUse = true;
		++EXTTYPE::m_numUsed;
		return pExt;
	}


	static void Remove(CEntity* pEntity)
	{
		replayFatalAssertf(EXTTYPE::IsForEntity(pEntity), "Attempting to remove extension to invalid entity");
		EXTTYPE* pExt = GetExtInternal(pEntity);
		if(!pExt || pExt->m_inUse == false)
		{
			return;
		}

		Assert(pExt->m_inUse == true);

		pExt->Reset();
		pExt->m_inUse = false;
		--EXTTYPE::m_numUsed;
	}


	static bool HasExtension(const CEntity* pEntity)
	{
		replayFatalAssertf(EXTTYPE::IsForEntity(pEntity), "Attempting to check extension on invalid entity");
		EXTTYPE* pExt = GetExtInternal(pEntity);
		if(!pExt)
		{
			return false;
		}

		return pExt->m_inUse;
	}

	static EXTTYPE* GetExtension(const CEntity* pEntity)
	{
		replayFatalAssertf(EXTTYPE::IsForEntity(pEntity), "Attempting to get extension on invalid entity");
		EXTTYPE* pExt = GetExtInternal(pEntity);
		if(!pExt || pExt->m_inUse == false)
		{
			return NULL;
		}

		return pExt;
	}

	static EXTTYPE* ProcureExtension(CEntity* pEntity)
	{
		replayFatalAssertf(EXTTYPE::IsForEntity(pEntity), "Attempting to get extension on invalid entity");
		EXTTYPE *pExt = GetExtension(pEntity);

		if(pExt != NULL)
			return pExt;

		return Add(pEntity);
	}

	static u32		GetNumUsed()	{	return m_numUsed;	}
	static const char*	GetName()	{	return m_name.c_str();	}

protected:

	static s16		GetExtIndex(const CEntity* pEntity)
	{
		const CPhysical* pPhysical = pEntity->GetIsPhysical() ? static_cast<const CPhysical*>(pEntity) : NULL;
		if(!pPhysical)
		{
			Assert(false);
			return -1;
		}

		const CReplayID& id = pPhysical->GetReplayID();
		return id.GetEntityID();
	}

	static EXTTYPE*	GetExtInternal(const CEntity* pEntity)
	{
		s16 index = EXTTYPE::GetExtIndex(pEntity);
		if(index < 0 || index >= m_elements.GetCapacity())
		{
			return NULL;
		}

		return &m_elements[index];
	}

	static atArray<EXTTYPE>	m_elements;
	static u32				m_numUsed;
	static atString			m_name;

	bool					m_inUse;
};

template<typename T>
atArray<T>			ReplayExtensionBase<T>::m_elements;
template<typename T>
u32					ReplayExtensionBase<T>::m_numUsed = 0;
template<typename T>
atString			ReplayExtensionBase<T>::m_name;

class ReplayEntityExtension : public ReplayExtensionBase<ReplayEntityExtension>
{
public:
	ReplayEntityExtension()
		: m_CreationFrameRef(FrameRef::INVALID_FRAME_REF)
		, m_DeletionFrameRef(FrameRef::INVALID_FRAME_REF)
		, mp_Entity(NULL)
		, m_Orientation(V_IDENTITY)
	{
	}

	void ResetTracking()
	{
		m_CreationFrameRef = FrameRef::INVALID_FRAME_REF;
	}

	void SetMatrix(Mat34V &mat)
	{
		m_Orientation = QuatVFromMat34V(mat);
	}

	QuatV GetOrientation()
	{
		return m_Orientation;
	}

	FrameRef	m_CreationFrameRef;
	FrameRef	m_DeletionFrameRef;

	CEntity*	mp_Entity;

	// TODO:- Compress.
	QuatV		m_Orientation;

	static bool		AllowCreationFrameResetting;

	static s16		GetExtIndex(const CEntity* pEntity);

	static bool IsForEntity(const CEntity*)
	{
		return true;
	}
};


class ReplayObjectExtension : public ReplayExtensionBase<ReplayObjectExtension>
{
public:
	ReplayObjectExtension()
	: m_uPropHash(0)
	, m_uTexHash(0)
	, m_uAnchorID(0)
	, m_streamedPedProp(false)
	, m_mapHash(REPLAY_INVALID_OBJECT_HASH)
	, m_addStamp(-1)
	, m_IsMapObjectToBeRecorded(false)
	, m_deletedAsConvertedToDummy(false)
	{
	}

	~ReplayObjectExtension();

	void Reset()
	{
		m_strModelRequest.Release();
		m_addStamp = -1;
		m_IsMapObjectToBeRecorded = false;
		m_deletedAsConvertedToDummy = false;
	}

	static u32	GetPropHash(const CEntity* pObject);
	static void SetPropHash(CEntity* pObject, u32 id);

	static u32	GetTexHash(const CEntity* pObject);
	static void SetTextHash(CEntity* pObject, u32 id);

	static u8	GetAnchorID(const CEntity* pObject);
	static void SetAnchorID(CEntity* pObject, u8 id);

	static bool	IsStreamedPedProp(const CEntity* pObject);
	static void SetStreamedPedProp(CEntity* pObject, bool b);

	void		SetStrModelRequest(const strModelRequest& req);

	void		SetMapHash(u32 hash)		{	m_mapHash = hash;	}
	u32			GetMapHash() const			{	return m_mapHash;	}

	void		SetIsMapObjectToBeRecorded(bool val) { m_IsMapObjectToBeRecorded = val; }
	bool		GetIsMapObjectToBeRecorded() { return m_IsMapObjectToBeRecorded; }

	void		SetDeletedAsConvertedToDummy()			{	m_deletedAsConvertedToDummy = true; }
	bool		GetDeletedAsConvertedToDummy() const	{	return m_deletedAsConvertedToDummy;	}

	u32			m_uPropHash;
	u32			m_uTexHash;
	u8			m_uAnchorID;
	bool		m_streamedPedProp;

	strModelRequest	m_strModelRequest;

	u32			m_mapHash;

	// Used when we need to re-add objects to the world because of eviction from interiors by game code (interior swaps).
	s32			m_addStamp;
	fwInteriorLocation m_location;

	bool		m_IsMapObjectToBeRecorded;

	bool		m_deletedAsConvertedToDummy;

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypeObject() && (((CObject*)pEntity)->IsPickup() == false);
	}
};


struct sReplayHeadPedBlendData
{
	void SetData(int head0, int head1, int head2, int tex0, int tex1, int tex2, float headBlend, float texBlend, float varBlend, bool isParent)
	{
		m_head0 = head0; m_head1 = head1; m_head2 = head2;
		m_tex0 = tex0; m_tex1 = tex1, m_tex2 = tex2;
		m_headBlend = headBlend;
		m_texBlend = texBlend;
		m_varBlend = varBlend;
		m_isParent = isParent;
	}

	int m_head0;
	int m_head1;
	int m_head2;
	int m_tex0;
	int m_tex1;
	int m_tex2;
	float m_headBlend;
	float m_texBlend;
	float m_varBlend;
	bool m_isParent;
};

struct sReplayMultiplayerHeadData
{
	sReplayHeadPedBlendData m_pedHeadBlendData;
	
	u8 m_overlayAlpha[HOS_MAX];
	u8 m_overlayNormAlpha[HOS_MAX];
	u8 m_overlayTex[HOS_MAX];

	float m_microMorphBlends[MMT_MAX];		
};

struct sReplayMultiplayerHeadData_OLD
{
	sReplayHeadPedBlendData m_pedHeadBlendData;

	u8 m_overlayAlpha[HOS_MAX];
	u8 m_overlayNormAlpha[HOS_MAX];
	u8 m_overlayTex[HOS_MAX];

	u8 m_microMorphBlends[MMT_MAX];		
};

struct sReplayHeadOverlayTintData
{
	u8 m_overlayTintIndex[HOS_MAX];
	u8 m_overlayTintIndex2[HOS_MAX];
	u8 m_overlayRampType[HOS_MAX];
};

struct sReplayHairTintData
{
	u8 m_HairTintIndex;
	u8 m_HairTintIndex2;
};

struct sReplayPedDecorationData
{
	bool operator!=(const sReplayPedDecorationData& other) const
	{
		return collectionName != other.collectionName || presetName != other.presetName || crewEmblemVariation != other.crewEmblemVariation || compressed != other.compressed || alpha != other.alpha;
	}
	u32 collectionName;
	u32 presetName;
	int crewEmblemVariation;
	bool compressed;
	float alpha;
};

struct sReplayHeadBlendPaletteData
{
	//Head blend palette Data
	u8 m_paletteRed[MAX_PALETTE_COLORS];
	u8 m_paletteGreen[MAX_PALETTE_COLORS];
	u8 m_paletteBlue[MAX_PALETTE_COLORS];
	bool m_forcePalette[MAX_PALETTE_COLORS];
	bool m_paletteSet[MAX_PALETTE_COLORS];
};

enum eMultiplayHeadFlags
{
	REPLAY_HEAD_BLEND						= 1 << 0,
	REPLAY_HEAD_OVERLAY						= 1 << 1,
	REPLAY_HEAD_MICROMORPH					= 1 << 2,
	REPLAY_HEAD_OVERLAYTINTDATA				= 1 << 3,
	REPLAY_HEAD_HAIRTINTDATA				= 1 << 4,
};

#define MAX_REPLAY_PED_DECORATION_DATA	(CPedDamageSetBase::kMaxTattooBlits + CPedDamageSetBase::kMaxDecorationBlits)

class ReplayPedExtension : public ReplayExtensionBase<ReplayPedExtension>
{
public:

	ReplayPedExtension()
		: m_isRecordedVisible(false),
		m_MultiplayerHeadFlags(0),
		m_EyeColorIndex(-1),
		m_PedShelteredInVehicle(false),
		m_PedSetForcePin(true)
	{
		Reset();
	}

	void Reset();
	void ResetHeadBlendData();

	void	SetIsRecordedVisible(const bool b)		{	m_isRecordedVisible	= b;	}
	bool	GetIsRecordedVisible() const			{	return m_isRecordedVisible;	}

	static void CloneHeadBlendData(CEntity* pPed, const CPedHeadBlendData* cloneData);
	static void	SetHeadBlendData(CEntity* pPed, const sReplayHeadPedBlendData& replayHeadBlendData);
	static void	SetHeadOverlayData(CEntity* pPed, u32 slot, u8 overlayTex, float overlayAlpha, float overlayNormAlpha);
	static void SetHeadMicroMorphData(CEntity* pPed, u32 type, float blend);	
	static void SetHeadOverlayTintData(CEntity* pPed, eHeadOverlaySlots slot, eRampType rampType, u8 tint, u8 tint2);
	static void SetHairTintIndex(CEntity* pPed, u8 index, u8 index2);
	static void SetPedDecorationData(CEntity* pPed, u32 collectionName, u32 presetName, int crewEmblemVariation, bool compressed, float alpha);
	static void ClonePedDecorationData(const CPed* source, CPed* target);
	static void SetHeadBlendDataPaletteColor(CEntity* pPed, u8 r, u8 g, u8 b, u8 colorIndex, bool bforce);
	static void SetEyeColor(CEntity* pPed, s32 colorIndex);

	const sReplayMultiplayerHeadData& GetReplayMultiplayerHeadData() { return m_ReplayMultiplayerHeadData;}
	const sReplayHeadOverlayTintData& GetHeadOverlayTintData() { return m_ReplayHeadOverlayTintData;}
	const sReplayHairTintData& GetHairTintData() { return m_ReplayHairTintData;}
	sReplayPedDecorationData& GetPedDecorationData(u32 index) { return m_PedDecorationData[index];}
	void GetHeadBlendPaletteColor(u8 colorIndex, u8 &r, u8 &g, u8 &b, bool &force, bool &hasBeenSet);
	s32 GetEyeColor() { return m_EyeColorIndex;}

	u8	 GetMultiplayerHeadDataFlags() { return m_MultiplayerHeadFlags;}
	void SetMultiplayerHeadDataFlags(u8 flags) { m_MultiplayerHeadFlags |= flags;}

	static void ClearPedDecorationData(CEntity* pPed);
	u32 GetNumPedDecorations() { return m_PedDecorationData.size(); }	

	void ApplyHeadBlendDataPostBlendHandleCreate(CPed* pPed);

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypePed();
	}

	void SetPedShelteredInVehicle(bool sheltered) { m_PedShelteredInVehicle = sheltered;}
	bool GetPedShelteredInVehicle() { return m_PedShelteredInVehicle;}
	void SetPedSetForcePin(bool val) { m_PedSetForcePin = val; }
	bool GetPedSetForcePin() { return m_PedSetForcePin;}

	void RewindVariationSet(int bit) { m_rewindVariationSet.Set(bit); }
	bool IsRewindVariationSet(int bit) const { return m_rewindVariationSet.IsSet(bit); }
	static void ResetVariationSet() 
	{
		for(int i = 0; i < m_elements.GetCount(); ++i)
		{
			m_elements.GetElements()[i].m_rewindVariationSet.Reset();
		}
	}
private:
	bool m_isRecordedVisible;

	//Head data
	sReplayMultiplayerHeadData m_ReplayMultiplayerHeadData;
	u8 m_MultiplayerHeadFlags;	

	//Ped decorations
	atFixedArray<sReplayPedDecorationData, MAX_REPLAY_PED_DECORATION_DATA> m_PedDecorationData;

	//Head Blend palette
	sReplayHeadBlendPaletteData m_ReplayHeadBlendPaletteData;

	//Head Overlay Tint data
	sReplayHeadOverlayTintData m_ReplayHeadOverlayTintData;

	//Hair tint data
	sReplayHairTintData m_ReplayHairTintData;

	//Eye color
	s32 m_EyeColorIndex;
	
	//Used on playback
	bool m_PedShelteredInVehicle;	
	bool m_PedSetForcePin;

	atFixedBitSet<PV_MAX_COMP> m_rewindVariationSet;
};

template<typename EXTTYPE>
class ReplayVehicleWithWheelsExtension : public ReplayExtensionBase<EXTTYPE>
{
public:
	ReplayVehicleWithWheelsExtension() {}
	~ReplayVehicleWithWheelsExtension() {}

	void Reset() { m_WheelData.Reset(); }

private:
	WheelValueExtensionData m_WheelData;

	friend class CPacketWheel;
};


class ReplayVehicleExtension : public ReplayVehicleWithWheelsExtension<ReplayVehicleExtension>
{
public:
	ReplayVehicleExtension()
		: m_ImpactsAppliedOffset(0)
		, m_fixed(false)
		, m_incrementScorch(true)
		, m_ForceHD(false)
	{
		Matrix34 mat;
		mat.Zero();
		m_dialBoneMtx.StoreMatrix(mat);
	}

	void Reset();
	void ResetOnRewind();
	void RemoveDeformation();
	void RemoveAllDeformations();
	void SetFixed(bool fixed) { m_fixed = fixed; }
	bool HasBeenFixed() {return m_fixed; }

	void SetIncScorch(bool b)	{	m_incrementScorch = b;	}
	bool GetIncScorch()			{	bool b = m_incrementScorch; m_incrementScorch = true;  return b; }

	void StoreVehicleDeformationData(const Vector4& damage, const Vector3& offset);
	void ApplyVehicleDeformation(CVehicle* pVehicle);
	bool HasFinishedApplyingDamage();	

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypeVehicle();
	}

	static void SetVehicleAsDamaged(CVehicle* pVehicle);
	static bool IsVehicleUndamaged(const CVehicle* pVehicle);

	void SetForceHD(bool force);
	bool GetForceHD();

	void SetVehicleDialBoneMtx(const Matrix34& mtx);
	void GetVehicleDialBoneMtx(Matrix34& mtx);
	CPacketPositionAndQuaternion20& GetVehicleDialBoneMtx();

private:

	struct vehicleImpactData
	{
		Vector4 damage;
		Vector3 offset;
		bool hasBeenApplied;
	};

	#define MAX_REPLAY_VEHICLE_IMPACTS	128
	atFixedArray<vehicleImpactData, MAX_REPLAY_VEHICLE_IMPACTS> m_Impacts;

	u32 m_ImpactsAppliedOffset;
	bool m_fixed;
	bool m_ForceHD;

	bool m_incrementScorch;
	CPacketPositionAndQuaternion20 m_dialBoneMtx;
};

class ReplayBicycleExtension : public ReplayVehicleWithWheelsExtension<ReplayBicycleExtension>
{
public:
	ReplayBicycleExtension()
		: m_bIsPedalling(0)
		, m_fPedallingRate(0.0f)
	{
	}

	static bool GetIsPedalling(const CEntity* pVehicle);
	static void SetIsPedalling(CEntity* pVehicle, bool val);

	static f32 GetPedallingRate(const CEntity* pVehicle);
	static void SetPedallingRate(CEntity* pVehicle, f32 rate);

	bool		m_bIsPedalling;
	f32			m_fPedallingRate;

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypeVehicle();
	}
};

class ReplayGlassExtension : public ReplayExtensionBase<ReplayGlassExtension>
{
public:
	ReplayGlassExtension()
		: m_glassHandle(-1)
	{
	}

	static bgGlassHandle	GetGlassHandle(const CEntity* pEntity);
	static void				SetGlassHandle(CEntity* pEntity, bgGlassHandle handle);

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypeObject() && (((CObject*)pEntity)->IsPickup() == false);
	}

private:
	bgGlassHandle	m_glassHandle;
};

class ReplayParachuteExtension : public ReplayExtensionBase<ReplayParachuteExtension>
{
public:
	ReplayParachuteExtension()
		: m_bIsParachuting(0)
		, m_iState(0)
	{
	}

	static bool GetIsParachuting(const CEntity* pPed);
	static void SetIsParachuting(CEntity* pPed, bool val);

	static u8 GetParachutingState(const CEntity* pPed);
	static void SetParachutingState(CEntity* pPed, u8 val);

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypePed();
	}

	u32	m_bIsParachuting : 1;
	u32	m_iState		 : 7;
};


class ReplayReticuleExtension : public ReplayExtensionBase<ReplayReticuleExtension>
{
public:
	ReplayReticuleExtension()
	{
	}

	static u32 GetWeaponHash(const CEntity* pPed);
	static void SetWeaponHash(CEntity* pPed, u32 val);

	static bool GetReadyToFire(const CEntity* pPed);
	static void SetReadyToFire(CEntity* pPed, bool val);

	static bool GetHit(const CEntity* pPed);
	static void SetHit(CEntity* pPed, bool val);

	static bool GetIsFirstPerson(const CEntity* pPed);
	static void SetIsFirstPerson(CEntity* pPed, bool val);

	static u8 GetZoomLevel(const CEntity* pPed);
	static void SetZoomLevel(CEntity* pPed, u8 val);

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypePed();
	}

private:
	ReticulePacketInfo m_ReticulePacketInfo;
};

class ReplayHUDOverlayExtension : public ReplayExtensionBase<ReplayHUDOverlayExtension>
{
public:
	ReplayHUDOverlayExtension()
	{
	}

	static bool GetTelescope(const CEntity* pPed);
	static void SetTelescope(CEntity* pPed, bool val);

	static bool GetBinoculars(const CEntity* pPed);
	static void SetBinoculars(CEntity* pPed, bool val);

	static u8 GetCellphone(const CEntity* pPed);
	static void SetCellphone(CEntity* pPed, u8 val);

	static bool GetTurretCam(const CEntity* pPed);
	static void SetTurretCam(CEntity* pPed, bool val);

	static bool GetDroneCam(const CEntity* pPed);
	static void SetDroneCam(CEntity* pPed, bool val);

	static u8 GetFlagsValue(const CEntity* pPed);
	static void SetFlagsValue(CEntity* pPed, u8 val);

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypePed();
	}

private:
	HUDOverlayPacketInfo m_hudOverlayPacketInfo;
};


static const int REPLAY_NUM_TL_CMDS = 8;
class ReplayTrafficLightExtension : public ReplayExtensionBase<ReplayTrafficLightExtension>
{
public:
	ReplayTrafficLightExtension()
	{
		for(int i = 0; i < REPLAY_NUM_TL_CMDS; ++i)
			lightCommandsCurrent[i] = 0;
	}

	bool		HaveTrafficLightCommandsChanged(const char* commands) const
	{
		for(int i = 0; i < REPLAY_NUM_TL_CMDS; ++i)
		{
			if(lightCommandsCurrent[i] != commands[i])
			{
				return true;
			}
		}
		return false;
	}

	bool		SetTrafficLightCommands(const char* commands)
	{
		bool commandsChanged = false;
		for(int i = 0; i < REPLAY_NUM_TL_CMDS; ++i)
		{
			if(lightCommandsCurrent[i] != commands[i])
			{
				commandsChanged = true;
				lightCommandsCurrent[i] = commands[i];
			}
		}
		return commandsChanged;
	}

	const char*		GetTrafficLightCommands() const
	{
		return lightCommandsCurrent;
	}

	static bool IsForEntity(const CEntity* pEntity)
	{
		return pEntity->GetIsTypeObject() && (((CObject*)pEntity)->IsPickup() == false);
	}
	
	char		lightCommandsCurrent[REPLAY_NUM_TL_CMDS];
};

#endif //GTA_REPLAY

#endif //_REPLAY_EXTENSIONS_H_
