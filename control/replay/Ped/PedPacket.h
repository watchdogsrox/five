//=====================================================================================================
// name:		PedPacket.h
// description:	Ped replay packet.
//=====================================================================================================

#ifndef INC_PEDPACKET_H_
#define INC_PEDPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/InterpPacket.h"
#include "control/replay/Misc/DeferredQuaternionBuffer.h"
#include "control/replay/Effects/ParticlePacket.h"
#include "control/replay/PacketBasics.h"
#include "control/replay/ReplayExtensions.h"

// Forward Declarations
class CPed;


#define BONE_SLOTS		256
#define BONE_SLOTS_HP	16

#if INCLUDE_FROM_GAME
#include "peds/rendering/PedVariationDS.h"
#include "audio/pedfootstepaudio.h"
#include "audio/speechmanager.h"
#endif

class ReplayController;
class CVehicle;
class CReplayModelManager;

typedef DeferredQuaternionBuffer<REPLAY_DEFERRED_QUATERION_BUFFER_SIZE_PED> tPedDeferredQuaternionBuffer;

//
//=====================================================================================================
class CReplayPedVariationData
{
public:
	CReplayPedVariationData();

public:
	void Store(const CPed* pPed);
private:
public:
	bool AddPreloadRequests(class CReplayModelManager &modelManager, u32 currGameTime) const;
	void Print() const;
	void Extract(CPed* pPed, const tPacketVersion packetVersion) const;
	void Update(u8 component, u32 variationHash, u32 variationData, u8 texId);

private:

	u8	m_VariationTexId[PV_MAX_COMP];
	u32 m_VariationHash[PV_MAX_COMP];
	u32 m_variationData[PV_MAX_COMP];
};



//////////////////////////////////////////////////////////////////////////
// Packet for creation of a Ped
class CPacketPedCreate : public CPacketBase, public CPacketEntityInfo<1>
{
public:
	CPacketPedCreate();

	void	Store(const CPed* pPed, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId = PACKETID_PEDCREATE, tPacketSize packetSize = sizeof(CPacketPedCreate));
	void	StoreExtensions(const CPed* pPed, bool firstCreationPacket, u16 sessionBlockIndex);
	void	CloneExtensionData(const CPacketPedCreate* pSrc) { if(pSrc) { CLONE_PACKET_EXTENSION_DATA(pSrc, this, CPacketPedCreate); } }

	void	SetFrameCreated(const FrameRef& frame)	{	m_FrameCreated = frame;	}
	FrameRef GetFrameCreated() const				{	return m_FrameCreated; }
	bool	IsFirstCreationPacket() const			{	return m_firstCreationPacket;	}

	void	Reset() 					{ SetReplayID(-1); m_FrameCreated = FrameRef::INVALID_FRAME_REF; }

	u32				GetModelNameHash() const		{	return m_ModelNameHash;	}
	strLocalIndex	GetMapTypeDefIndex() const		{	return strLocalIndex(strLocalIndex::INVALID_INDEX); }
	void			SetMapTypeDefIndex(strLocalIndex /*index*/) {}
	u32				GetMapTypeDefHash() const		{	return 0;}
	bool			UseMapTypeDefHash() const { return false; } 

	bool	IsMainPlayer()	const		{ return m_bIsMainPlayer;	}

	u8		GetPainBank() const			{ return m_painBank;		}
	const u8*		GetPainNextVariations() const	{	return m_NextVariationForPain;	}
	const u8*		GetPainNumTimesPlayed() const	{	return m_NumTimesPainPlayed;	}
	const u32&		GetNextTimeToLoadPain() const	{	return m_NextTimeToLoadPain;	}
	const u8&		GetCurrentPainBank() const		{	return m_CurrentPainBank;		}
	CReplayID		GetParentID() const;

	CReplayPedVariationData const* GetInitialVariationData() const { return &m_InitialVariationData; }
	CReplayPedVariationData const* GetLatestVariationData() const { return &m_LatestVariationData; }
	CReplayPedVariationData *		GetLatestVariationData()		{ return &m_LatestVariationData; }
	CReplayPedVariationData const* GetVariationData() const
	{
		if(m_VariationDataVerified == false)
		{
			return GetLatestVariationData();
		}

		return GetInitialVariationData();
	}

	bool	IsVarationDataVerified() { return m_VariationDataVerified; }
	void	SetVarationDataVerified() { m_VariationDataVerified = true; }

	void	LoadMatrix(Matrix34& matrix) const	{	m_posAndRot.LoadMatrix(matrix);	}

	bool	ValidatePacket() const
	{
		replayAssertf((GetPacketID() == PACKETID_PEDCREATE) || (GetPacketID() == PACKETID_PEDDELETE), "Validation of CPacketPedCreate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketPedCreate), "Validation of CPacketPedCreate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketPedCreate) && ((GetPacketID() == PACKETID_PEDCREATE) || (GetPacketID() == PACKETID_PEDDELETE));
	}

	void				PrintXML(FileHandle handle) const;

	void 	Extract(CPed* pPed) const;

	u8*		GetHeadBlendDataPtr(u32 pedCreateClassSize) const;
	u8*		GetPedDecorationDataPtr(u32 pedCreateClassSize) const;
	u8*		GetHeadOverlayTintDataPtr(u32 pedCreateClassSize) const;
	u8*		GetHairTintDataPtr(bool hasHeadOverlayData, u32 pedCreateClassSize) const;

private:

	FrameRef						m_FrameCreated;
	bool							m_firstCreationPacket;
	
	u8								m_painBank;

	u8								m_NextVariationForPain[AUD_PAIN_ID_NUM_BANK_LOADED];
	u8								m_NumTimesPainPlayed[AUD_PAIN_ID_NUM_BANK_LOADED];
	u32								m_NextTimeToLoadPain;
	u8								m_CurrentPainBank;

	u32								m_ModelNameHash;
	bool							m_bIsMainPlayer;

	CPacketPositionAndQuaternion	m_posAndRot;

	bool							m_VariationDataVerified;
	CReplayPedVariationData			m_InitialVariationData;
	CReplayPedVariationData			m_LatestVariationData;

	rlClanId m_PedClanId;
	
	u32 m_numPedDecorations;
	u8  m_multiplayerHeadFlags;
	s32 m_eyeColorIndex;

	sReplayHeadBlendPaletteData m_HeadBlendPaletteData;

public:
	DECLARE_PACKET_EXTENSION(CPacketPedCreate);

	struct PedCreateExtension
	{
		// CPacketPedCreateExtensions_V1
		CReplayID	m_parentID;

		// CPacketPedCreateExtensions_V2
		u32	m_RecordedClassSize;
	};

private:
	//Variable size data added at the end of this class

	// sReplayMultiplayerHeadData * m_hasHeadBlendData	(doesnt always have one)
	// sReplayPedDecorationData * m_numPedDecorations 	(variable size)
	// sReplayHeadOverlayTintData * m_hasHeadOverlayTintData (doesnt always have one)
	// sReplayHairTintData * m_hasHairTintData (doesnt always have one)

};

enum eReplayRelevantAITaskFlags
{
	//initialisation constant
	REPLAY_NO_RELEVENT_AI_TASK		= 0,

	//ai task we're interested in go here
	REPLAY_ENTER_VEHICLE_SEAT_TASK	= BIT0,
	REPLAY_TASK_AIM_GUN_ON_FOOT		= BIT1
};

struct ReplayRelevantAITaskInfo
{
	ReplayRelevantAITaskInfo()
	{
		m_relevantAITaskRunning = REPLAY_NO_RELEVENT_AI_TASK;
	}

	bool IsAITaskRunning(u32 flag)
	{
		return (m_relevantAITaskRunning & flag) == flag;
	}

	void SetAITaskRunning(u32 flag)
	{
		m_relevantAITaskRunning |= flag;
	}

	void ClearAITaskRunning(u32 flag)
	{
		m_relevantAITaskRunning &= ~flag;
	}

private:
	u32 m_relevantAITaskRunning;

};


//=====================================================================================================
class CPacketPedUpdate : public CPacketBase, public CPacketInterp, public CBasicEntityPacketData
{
private:
	class CBoneBitField
	{
	public:
		CBoneBitField(u32 noOfBones) 
		{
			u32 size = MemorySize(noOfBones);
			memset((void *)m_Bits, 0, size);
		}
		~CBoneBitField()
		{

		};

		void SetBit(u32 bone)
		{
			u8 bit = 0x1 << (bone & 7);
			m_Bits[bone >> 3] |= bit;
		}
		bool GetBit(u32 bone)
		{
			u8 bit = 0x1 << (bone & 7);
			return (m_Bits[bone >> 3] & bit) != 0;
		}
		static u32 MemorySize(u32 noOfBones) 
		{	
			u32 NoOfBytes = REPLAY_ALIGN(noOfBones, 8) >> 3;
			return REPLAY_ALIGN(NoOfBytes, 16);
		}
	public:
		u8 m_Bits[1];
	};

	typedef struct BONE_AND_SCALE
	{
		u16 boneIndex;
		ReplayCompressedFloat scale[3];
	} BONE_AND_SCALE;

public:


#define USE_NEW_POSITIONS_RANGE	35.0f
#define PED_BONE_DATA_ALIGNMENT	16

	typedef CPacketVector3Comp<valComp<s16, s16_f32_n35_to_35>>	tBonePositionType;

	class CBoneDataIndexer
	{
	public:
		CBoneDataIndexer(tBonePositionType const*pTranslations,
			CPacketVector3 const* pTranslationsHigh,
			CPacketQuaternionH const* pQuaternionH, CPacketQuaternionL const*pQuaternionL)
		{
			m_pTranslations = pTranslations;
			m_pTranslationsHigh = pTranslationsHigh;
			m_pQuaternionsHigh = pQuaternionH;
			m_pQuaternionsLow = pQuaternionL;
			m_NextTranslation = 0;
			m_NextTranslationHigh = 0;
			m_NextQuaternionHigh = 0;
			m_NextQuaternionLow = 0;
		}
		void GetNextTranslation(Vector3 &dest)
		{
			m_pTranslations[m_NextTranslation++].Load(dest);
		}
		void GetNextTranslationHigh(Vector3 &dest)
		{
			m_pTranslationsHigh[m_NextTranslationHigh++].Load(dest);
		}
		void GetNextQuaternionsHigh(Quaternion &dest)
		{
			m_pQuaternionsHigh[m_NextQuaternionHigh++].LoadQuaternion(dest);
		}
		void GetNextQuaternionsLow(Quaternion &dest)
		{
			m_pQuaternionsLow[m_NextQuaternionLow++].LoadQuaternion(dest);
		}
	public:
		tBonePositionType const* m_pTranslations;
		CPacketVector3 const* m_pTranslationsHigh;
		CPacketQuaternionH const* m_pQuaternionsHigh;
		CPacketQuaternionL const* m_pQuaternionsLow;
		u32 m_NextTranslation;
		u32 m_NextQuaternionHigh;
		u32 m_NextQuaternionLow;
		u32 m_NextTranslationHigh;
	};

	typedef struct BONE_TRANSFORMATION
	{
		Quaternion rotation;
		Vector3 translation;
	} BONE_TRANSFORMATION;

	void				Store(const CPed* pPed, bool firstUpdatePacket);
	static bool			IsTranslationCloseToDefault(Vec3V_ConstRef translation, Vec3V_ConstRef defaultTranslation, Vec3V_ConstRef epsilon);
	void				StoreExtensions(CPed* pPed, bool firstUpdatePacket);
	void *				StoreBoneAndScales(const CPed* pPed, u32 &numberStored, BONE_AND_SCALE *pDest);
	void				PreUpdate(const CInterpEventInfo<CPacketPedUpdate, CPed>& info) const;
	void				Extract(const CInterpEventInfo<CPacketPedUpdate, CPed>& info, CVehicle* pVehicle = NULL) const;
	void				ExtractExtensions(CPed* pPed, const CInterpEventInfo<CPacketPedUpdate, CPed>& eventInfo) const;

	void				Print() const;

	bool 				IsMainPlayer()	const			{ return m_bIsMainPlayer; }
	u16	 				GetBoneCount() const			{ return m_uBoneCount; }

	bool				IsVisible() const				{ return m_isVisible != 0;	}

	CReplayID			GetVehicleID() const			{	return m_MyVehicle;	}

	bool				ShouldPreload() const;
	u32					GetOverrideCrewLogoTxd() const;

	bool				ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_PEDUPDATE, "Validation of CPacketPedCreate Failed!, 0x%x", GetPacketID());
		return CPacketBase::ValidatePacket() && 
				CPacketInterp::ValidatePacket() && 
				CBasicEntityPacketData::ValidatePacket() && 
				GetPacketID() == PACKETID_PEDUPDATE;
	}
	
	void				PrintXML(FileHandle handle) const;

private:

	bool 				IsDead() const					{ return m_bDead;}
	
	u16					GetNumQuaternions() const		{	return m_numQuaternions;		}
	u16					GetNumQuaternionsH() const		{	return m_numQuaternionsHigh;	}
	u16					GetNumTranslations() const		{	return m_numTranslations;		}
	u16					GetNumTranslationsH() const		{	return m_numTranslationsHigh;	}
	
	static bool			ShouldStoreHighQuaternion(eAnimBoneTag uBoneTag);
	static bool			ShouldStroreTranslations(const rage::crBoneData *pBoneData);
	static bool			ShouldStoreScales(const rage::crBoneData *pBoneData);

	static atHashString m_ForceHQPedModelTable[];

	static bool			ShouldStoreHightQualityPed(const CPed *pPed);

	// Size related.
	u32					GetExtraBoneSize() const;
	static u32			GetBonePositionSize(u32 numTranslations)	{ return REPLAY_ALIGN(sizeof(tBonePositionType)*numTranslations, 16); }
	static u32			GetBonePositionHSize(u32 numTranslations)	{ return REPLAY_ALIGN(sizeof(CPacketVector3)*numTranslations, 16); }
	static u32			GetRotationsHSize(u32 numRotations)			{ return numRotations * sizeof(CPacketQuaternionH); }
	static u32			GetRotationsSize(u32 numRotations)			{ return numRotations * sizeof(CPacketQuaternionL); }
	static u32			GetBoneBitFieldSize(u32 numBones)			{ return CBoneBitField::MemorySize(numBones); }

	// Bone transformation info.
	u8 *GetBoneDataBasePtr() const;

	tBonePositionType *GetBonePositionsL() const
	{	
		ptrdiff_t pBase = (ptrdiff_t)GetBoneDataBasePtr();
		return (tBonePositionType *)REPLAY_ALIGN(pBase, PED_BONE_DATA_ALIGNMENT);
	}

	// These are the order they are stored in packet memory.
	CPacketQuaternionH*	GetBoneRotationsH() const					{	return (CPacketQuaternionH*)((u8*)GetBonePositionsL() + GetBonePositionSize(GetNumTranslations()));	}
	CPacketQuaternionL*	GetBoneRotationsL() const					{	return (CPacketQuaternionL*)((u8*)GetBoneRotationsH() + GetRotationsHSize(GetNumQuaternionsH()));	}
	CBoneBitField	 *	GetBoneRotationStoredBitField() const		{	return (CBoneBitField *)((u8*)GetBoneRotationsL() + GetRotationsSize(GetNumQuaternions())); }
	CBoneBitField	 *	GetBonePositionIsHOrLBitField() const	{	return (CBoneBitField *)((u8*)GetBoneRotationStoredBitField() + GetBoneBitFieldSize(GetBoneCount())); }
	CPacketVector3   *  GetBonePositionsH() const					{   return (CPacketVector3 *)((u8*)GetBonePositionIsHOrLBitField() + GetBoneBitFieldSize(GetBoneCount())); }
	CBoneBitField*		GetBoneRotationIsHOrLBitField() const	{	return (CBoneBitField*)((u8*)GetBonePositionsH() + GetBonePositionHSize(GetNumTranslationsH()));	}
	CBoneBitField*		GetBonePositionStoredBitField() const		{	return (CBoneBitField*)((u8*)GetBoneRotationIsHOrLBitField() + GetBoneBitFieldSize(GetBoneCount()));	}

	void				StoreBoneMatrixData(const CPed* pPed);

	void				ProcessVehicle(CPed* pPed, CVehicle* pVehicle, const CInterpEventInfo<CPacketPedUpdate, CPed>& info) const;

	void				ExtractBoneMatrices(CPed* pPed) const;
	void				ExtractBoneMatrices(CPed* pPed, const CPacketPedUpdate *pNextPacket, float interp) const;
	void				BuildLocalTransformationsFromObjectMatrices(CPed *pPed, u16  boneCount, CBoneBitField *pRotationIsHOrLBitField, CBoneDataIndexer &indexer, Matrix34 &pedMatrixFromPacket, BONE_TRANSFORMATION *pLocalTransformations) const;
#if __BANK
	void				InterpolateHighQualityInWorldSpace(CPed* pPed, float interp, u16 boneCount, CBoneBitField *pRotationIsHOrLBitField, CBoneBitField *pRotationIsHOrLBitFieldNext, CBoneDataIndexer &indexer, CBoneDataIndexer &indexerNext, Matrix34 &pedMatrixPrev, Matrix34 &pedMatrixNext,  Matrix34 *pDest) const;
#endif //__BANK
	void				FormWorlSpaceMatricesFromPacketLocalMatricies(u16 boneCount, const rage::crSkeletonData *pSkelData, const Matrix34 &pedWorldspaceMtx, CBoneBitField *pRotationStoredBitField, CBoneBitField *pRotationIsHOrLBitField, CBoneBitField *pPositionStoredBitField, CBoneBitField *pPositionIsHOrLBitField, CBoneDataIndexer &indexer, Matrix34 *pDest) const;
	void				ApplyStoredScales(CPed *pPed, u16 boneCount, BONE_AND_SCALE *pBonesAndScales, Matrix34 *pDest) const;
	void				ApplyStoredScales(CPed *pPed, u16 boneCount, BONE_AND_SCALE *pBonesAndScalesPrev, BONE_AND_SCALE *pBonesAndScalesNext, float interp,  Matrix34 *pDest) const;
	void 				ExtractOnlyPosition(Matrix34& rMatrix, const Vector3& rVecNew, float fInterp, const tBonePositionType& storeLoc) const;

	// Other bones (rotation only)
	void 				ExtractBoneRotation(Matrix34& rMatrix, const CPacketQuaternionL& storeLoc) const;
	void 				ExtractBoneRotationHigh(Matrix34& rMatrix, const CPacketQuaternionH& storeLoc) const;
	void 				ExtractBoneRotation(Matrix34& rMatrix, const CPacketQuaternionL& storeLoc, const CPacketQuaternionL& storeLocNext, float fInterp) const;
	void 				ExtractBoneRotationHigh(Matrix34& rMatrix, const CPacketQuaternionH& storeLoc, const CPacketQuaternionH& storeLocNext, float fInterp) const;
	void 				StoreBoneRotation(const Mat34V& rMatrix, CPacketQuaternionL& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket);
	void 				StoreBoneRotation(const QuatV &rQuat, CPacketQuaternionL& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket);
	void 				StoreBoneRotationHigh(const Mat34V& rMatrix, CPacketQuaternionH& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket);
	void				StoreBoneRotationHigh(const QuatV &rQuat, CPacketQuaternionH& storeLoc, tPedDeferredQuaternionBuffer::TempBucket &tempBucket);

#if REPLAY_DELAYS_QUANTIZATION
public:
	void				QuantizeQuaternions();
	static void			ResetDelayedQuaternions();
#endif // REPLAY_DELAYS_QUANTIZATION

	bool				ComputeIsBuoyant(const CPed* pPed) const;

	CReplayID				m_MyVehicle; // -1 = none

	u8						m_PedType;
	
	u8						m_bDead				:1;
	u8						m_bStanding			:1;
	u8						m_bSitting			:1;
	u8						m_bIsMainPlayer		:1;		// Moved to CPacketPedCreate really, still seems to be used does it need to be in both?
	u8						m_bHeadInvisible	:1;
	u8						m_bBuoyant			:1;
	u8						m_renderScorched	:1;
	u8						m_highQualityPed	:1;

	u8						m_SeatInVehicle		:6;
	u8						m_bIsExitingVehicle	:1;
	u8						m_isVisible			:1;
	
	u8						m_MoveState			:3;
	u8						m_JitterScale		:5;

	u8						m_bIsParachuting	: 1;	
	u8						m_ParachutingState	: 4;
	u8						m_DisableHelmetCullFPS : 1;
	u8						m_ShelteredInVehicle :1;
	u8						m_IsFiring			: 1;

	u8						m_bSpecialAbilityActive : 1;
	u8						m_bSpecialAbilityFadingOut : 1;	
	u8						m_specialAbilityFractionRemaining : 6;
	CPacketVector<3>		m_Velocity;

	CPacketVector<4>		m_wetClothingInfo;
	u8						m_wetClothingFlags;

	u16						m_uBoneCount;

	u8						m_footWaterInfo[MAX_NUM_FEET];	// audFootWaterInfo
	float					m_footWetness[MAX_NUM_FEET];

	// Props
	CPackedPedProps			m_PropsPacked;

	u16						m_numQuaternions;
	u16						m_numQuaternionsHigh;
	u16						m_numTranslations;
	u16						m_numTranslationsHigh;
	
	ReticulePacketInfo		m_ReticulePacketInfo;
	HUDOverlayPacketInfo	m_hudOverlayPacketInfo;

#if REPLAY_DELAYS_QUANTIZATION
	static tPedDeferredQuaternionBuffer sm_DeferredQuaternionBuffer;
#endif // REPLAY_DELAYS_QUANTIZATION

	DECLARE_PACKET_EXTENSION(CPacketPedUpdate);

	// Packet extension.
	typedef struct PacketExtensions
	{
		//Version 1
		u32 noOfScales;
		u32 offsetToBoneAndScales;

		//Version 2
		bool isUsingRagdoll;

		//Version 3
		ReplayRelevantAITaskInfo aiTasksRunning;

		//Version 4
		u8	m_bIsEnteringVehicle	:1;

		u8	m_hasAlphaOverride		:1;	// Version 6
		u8	m_unused				:6;
		u8	m_alphaOverrideValue;		// Version 6
		u8	m_heatScaleOverride;		// Version 8
		u8	m_numDecorations;			// Version 9
		
		// Version 5.
		u32 m_noOfCumulativeInvMatrices;
		u32 m_offsetToCumulativeInvMatrices;

		// Version 9
		u32 m_offsetToDecorations;

		// Version 10
		u32 m_overrideCrewLogoTxdHash;
		u32 m_overrideCrewLogoTexHash;

		// Version 11
		u32 m_timeOfDeath;
	} PacketExtensions;


public:
	void *WriteHeadBlendCumulativeInverseMatrices(CPed *pPed, PacketExtensions *pExtensions, void *pAfterExtensionHeader);
	void ExtractHeadBlendCumulativeInverseMatrices(CPed *pPed) const;

	void* WritePedDecorations(CPed* pPed, PacketExtensions* pExtension, void* pAfterExtensionHeader);
	void ExtractPedDecorations(CPed* pPed) const;
private:

#if __BANK
	static bool ms_InterpolateHighQualityInWorlSpace;
#endif //__BANK

private:
	static bool IsPedInsideLuxorPlane(CPed *pPed);
};

//////////////////////////////////////////////////////////////////////////
// Packet for deletion of a Ped
class CPacketPedDelete : public CPacketPedCreate
{
public:
	void	Store(const CPed* pPed, const CPacketPedCreate* pLastCreatedPacket);
	void	StoreExtensions(const CPed*, const CPacketPedCreate* pLastCreatedPacket);
	void	Cancel()					{	SetReplayID(-1);	}

	void	PrintXML(FileHandle handle) const;
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_PEDDELETE, "Validation of CPacketPedDelete Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketPedDelete), "Validation of CPacketVehicleDelete extensions Failed!, 0x%08X", GetPacketID());
		return  CPacketPedCreate::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketPedDelete) && GetPacketID() == PACKETID_PEDDELETE;
	}

	const CPacketPedCreate*	GetCreatePacket() const	{ return this; }

private:
	DECLARE_PACKET_EXTENSION(CPacketPedDelete);
};

#define MAX_PED_DELETION_SIZE 2536	//		 sizeof(CPacketPedDelete) // Ped deletion can be bigger

//////////////////////////////////////////////////////////////////////////
class CPedInterp : public CInterpolator<CPacketPedUpdate>
{
public:
	CPedInterp() { Reset(); }

	void Init(const ReplayController& controller, CPacketPedUpdate const* pPrevPacket);

	void Reset();

	s32	GetPrevFullPacketSize() const	{ return m_sPrevFullPacketSize; }
	s32	GetNextFullPacketSize() const	{ return m_sNextFullPacketSize; }

protected:
	s32							m_sPrevFullPacketSize;
	s32							m_sNextFullPacketSize;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedVariationChange : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedVariationChange(CPed* pPed, u8 component, u32 oldVariationHash, u32 oldVarationData, u8 oldTexId);
	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:

	struct varData
	{
		u32	m_variationHash;
		u32	m_variationData;
		u8	m_texId;
	};

	varData m_prevValues;
	varData m_nextValues;

	u8	m_component;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedMicroMorphChange : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedMicroMorphChange(u32 type, float blend, float previousMicroMorphBlend);
	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:

	u32 m_type;
	float m_blend;
	float m_previousBlend;

};

//////////////////////////////////////////////////////////////////////////
class CPacketPedHeadBlendChange : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedHeadBlendChange(const sReplayHeadPedBlendData& replayData, const sReplayHeadPedBlendData& previousReplayData);
	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:

	sReplayHeadPedBlendData m_data;
	sReplayHeadPedBlendData m_previousData;
};

#endif // GTA_REPLAY

#endif // INC_PEDPACKET_H_
