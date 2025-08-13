//=====================================================================================================
// name:		FragmentPacket.h
// description:	Fragment replay packets.
//=====================================================================================================

#ifndef INC_FRAGMENTPACKET_H_
#define INC_FRAGMENTPACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/InterpPacket.h"
#include "control/replay/Misc/DeferredQuaternionBuffer.h"
#include "control/replay/PacketBasics.h"
#include "control/replay/ReplayInternal.h"
#include "scene/world/GameWorld.h"
#include "atl/bitset.h"

class CEntity;
struct sVehicleIgnoredBones
{
	Matrix34 m_ignoredBoneOverrideMtx;
	u32 m_boneIdx;

	bool operator==(const sVehicleIgnoredBones& c) const { return c.m_boneIdx == m_boneIdx; }
};

struct sInterpIgnoredBone
{
	sInterpIgnoredBone()
		: m_boneIdx(0)
		, m_onlyIfPrevIsDefault(true)
	{}

	sInterpIgnoredBone(int bone, bool b = true)
		: m_boneIdx(bone)
		, m_onlyIfPrevIsDefault(b)
	{}

	int m_boneIdx;
	bool m_onlyIfPrevIsDefault;

	bool operator==(const sInterpIgnoredBone& c) const { return c.m_boneIdx == m_boneIdx; }
};
namespace rage
{
	struct fragHierarchyInst; 
	class crSkeleton;
	class crBoneData;
	class fragInst;
}

const u8 IsCachedBit = 6;
const u8 IsBrokenBit = 5;
//////////////////////////////////////////////////////////////////////////
const u8 NUMFRAGBITS = 72;
class CPacketFragData : public CPacketBase
{
public:
	void 	Store(CEntity* pEntity);
	void	StoreExtensions(CEntity* pEntity) { PACKET_EXTENSION_RESET(CPacketFragData); (void)pEntity; }
	void 	Extract(CEntity* pEntity) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FRAGDATA, "Validation of CPacketFragData Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_FRAGDATA && CPacketBase::ValidatePacket();
	}

	void	PrintXML(FileHandle handle) const;

protected:
	bool 	IsCached() const		{ return GetPadBool(IsCachedBit) != 0; }
	bool 	IsBroken() const		{ return GetPadBool(IsBrokenBit) != 0; }

 	rage::atFixedBitSet<NUMFRAGBITS, u8>	m_destroyedBits;
 	rage::atFixedBitSet<NUMFRAGBITS, u8>	m_damagedBits;
	DECLARE_PACKET_EXTENSION(CPacketFragData);
};


//////////////////////////////////////////////////////////////////////////
const u8 NUMFRAGBITS_NO_DAMAGE_BITS = 64;
class CPacketFragData_NoDamageBits : public CPacketBase
{
public:
	void 	Store(CEntity* pEntity);
	void	StoreExtensions(CEntity* pEntity) { PACKET_EXTENSION_RESET(CPacketFragData_NoDamageBits); (void)pEntity; }
	void 	Extract(CEntity* pEntity) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS, "Validation of CPacketFragData_NoDamageBits Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS && CPacketBase::ValidatePacket();
	}

	void	PrintXML(FileHandle handle) const;

protected:
	bool 	IsCached() const		{ return GetPadBool(IsCachedBit) != 0; }
	bool 	IsBroken() const		{ return GetPadBool(IsBrokenBit) != 0; }

	rage::atFixedBitSet<NUMFRAGBITS_NO_DAMAGE_BITS, u8>	m_destroyedBits;
	u8													m_ExtraBits;
	DECLARE_PACKET_EXTENSION(CPacketFragData_NoDamageBits);
};


//////////////////////////////////////////////////////////////////////////
class CPacketFragBoneData : public CPacketBase
{
	typedef CPacketPositionAndQuaternion	tBoneData;
	typedef CPacketPositionAndQuaternion20	tHighResBoneData;
public:
	void 	Store(CEntity* pEntity);
	void	StoreExtensions(CEntity* pEntity);
	void 	Extract(CEntity* pEntity, CPacketFragBoneData const* pNextPacket, float fInterpValue, bool bUpdateWorld = true) const;

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FRAGBONEDATA, "Validation of CPacketFragBoneData Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketFragBoneData), "Validation of CPacketFragBoneData extensions Failed!, 0x%08X", GetPacketID());
#if REPLAY_GUARD_ENABLE
		int* pEnd = (int*)((u8*)this + GetPacketSize() - GET_PACKET_EXTENSION_SIZE(CPacketFragBoneData) - sizeof(int));
		replayAssertf((*pEnd == REPLAY_GUARD_END_DATA), "Validation of CPacketFragBoneData Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_FRAGBONEDATA && VALIDATE_PACKET_EXTENSION(CPacketFragBoneData) && CPacketBase::ValidatePacket() && (*pEnd == REPLAY_GUARD_END_DATA);
#else
		return GetPacketID() == PACKETID_FRAGBONEDATA && VALIDATE_PACKET_EXTENSION(CPacketFragBoneData) && CPacketBase::ValidatePacket();
#endif // REPLAY_GUARD_ENABLE
	}

	void	PrintXML(FileHandle handle) const;

	tBoneData*	GetBones(int guard = REPLAY_GUARD_ENABLE) const
	{	
		return (tBoneData*)((u8*)this + GetPacketSize() - m_numDamagedBones * sizeof(tBoneData) - m_numUndamagedBones * sizeof(tBoneData) - sizeof(int) * guard - GET_PACKET_EXTENSION_SIZE(CPacketFragBoneData));
	}
	tBoneData*	GetDamagedBones(int guard = REPLAY_GUARD_ENABLE) const	
	{	
		return (tBoneData*)((u8*)GetBones(guard) + m_numUndamagedBones * sizeof(tBoneData));	
	}

	static bool ShouldStoreHighResBones(const CEntity *pEntity);

	typedef struct FRAG_BONE_DATA_EXTENSIONS
	{
		u32 m_HighResUndamagedBones_Offset;
		u32 m_HighResDamagedBones_Offset;

		tHighResBoneData *GetUndamagedBones()
		{
			if(m_HighResUndamagedBones_Offset)
				return (tHighResBoneData *)((ptrdiff_t)this + (ptrdiff_t)m_HighResUndamagedBones_Offset);
			return NULL;
		}

		tHighResBoneData *GetDamagedBones()
		{
			if(m_HighResDamagedBones_Offset)
				return (tHighResBoneData *)((ptrdiff_t)this + (ptrdiff_t)m_HighResDamagedBones_Offset);
			return NULL;
		}

	} FRAG_BONE_DATA_EXTENSIONS;

	FRAG_BONE_DATA_EXTENSIONS *BeginExtensionWrite();
	void EndExtensionWrite(void *pAddr);

protected:
	void	StoreBone(tBoneData* pBones, rage::crSkeleton* pSkeleton, s32 boneID);
	void	StoreHighResBone(tHighResBoneData* pBones, rage::crSkeleton* pSkeleton, s32 boneID);

	u16								m_numUndamagedBones;
	u16								m_numDamagedBones;
	DECLARE_PACKET_EXTENSION(CPacketFragBoneData);

	static u32 ms_HighResWhiteList[];
};


//////////////////////////////////////////////////////////////////////////
#define SKELETON_BONE_DATA_ALIGNMENT 4
#define SKELETON_BONE_DATA_TRANSLATION_EPSILON 0.01f
#define SKELETON_BONE_DATA_QUATERNION_EPSILON 0.001f
#define SKELETON_BONE_DATA_SCALE_EPSILON 0.0001f

// Used during testing for zero scale matrices (See IsZeroScale()).
#define SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR 0.001f

class BoneModifier
{
public:
	BoneModifier() {};
	virtual ~BoneModifier() {};
	virtual bool ShouldModifyBoneLocalRotation(u32 boneIndex) = 0;
	virtual Quaternion ModifyBoneLocalRotation(Quaternion &localRotation, Matrix34 const &boneParantMtx, Matrix34 const &entityMtx) = 0;
};

typedef DeferredQuaternionBuffer<REPLAY_DEFERRED_QUATERION_BUFFER_SIZE_FRAGMENT> tFragmentDeferredQuaternionBuffer;

template<typename QUATTYPE>
class CSkeletonBoneData : public CPacketBase
{
	typedef u16 ZeroScaleType;

public:
	
	static eReplayPacketId PacketID;
#if REPLAY_DELAYS_QUANTIZATION
	static tFragmentDeferredQuaternionBuffer sm_DeferredQuaternionBuffer2;
#endif // REPLAY_DELAYS_QUANTIZATION

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

		void SetBit(u32 bitIdx)
		{
			u8 bit = 0x1 << (bitIdx & 7);
			m_Bits[bitIdx >> 3] |= bit;
		}
		bool GetBit(u32 bitIdx)
		{
			u8 bit = 0x1 << (bitIdx & 7);
			return (m_Bits[bitIdx >> 3] & bit) != 0;
		}
		static u32 MemorySize(u32 noOfBones) 
		{	
			u32 NoOfBytes = REPLAY_ALIGN(noOfBones*2, 8) >> 3;
			return REPLAY_ALIGN(NoOfBytes, SKELETON_BONE_DATA_ALIGNMENT);
		}
	public:
		u8 m_Bits[1];
	};

	class CBoneDataIndexer
	{
	public:
		CBoneDataIndexer(CPacketVector3* pTranslations, QUATTYPE *pRotations, ZeroScaleType *pZeroScales, CPacketVector3* pScales = NULL)
		{
			m_pTranslations = pTranslations;
			m_NextTranslation = 0;
			m_pRotations = pRotations;
			m_NextRotation = 0;
			m_pScale = pScales;
			m_NextScale = 0;
			m_pZeroScales = pZeroScales;
			m_NextZeroScale = 0;
		}
		void GetNextTranslation(Vector3 &dest)
		{
			m_pTranslations[m_NextTranslation++].Load(dest);
		}
		void GetNextRotation(Quaternion &dest)
		{
			m_pRotations[m_NextRotation++].LoadQuaternion(dest);
		}
		void GetNextScale(Vector3 &dest)
		{
			m_pScale[m_NextScale++].Load(dest);
		}
		bool GetNextZeroScale(ZeroScaleType boneIndex)
		{
			if(m_pZeroScales[m_NextZeroScale] == boneIndex)
			{
				m_NextZeroScale++;
				return true;
			}
			return false;
		}
	public:
		CPacketVector3 *m_pTranslations;
		QUATTYPE * m_pRotations;
		CPacketVector3 *m_pScale;
		ZeroScaleType *m_pZeroScales;
		u32 m_NextTranslation;
		u32 m_NextRotation;
		u32 m_NextScale;
		u32 m_NextZeroScale;
	};

public:
	CSkeletonBoneData();
	~CSkeletonBoneData();

	// Can only  be used once packet size has been written.
	u8 *GetBasePtr() const { return ((u8 *)this + GetPacketSize() - CBoneBitField::MemorySize(m_NoOfBones) - CBoneBitField::MemorySize(m_NoOfBones) - sizeof(QUATTYPE)*m_NoOfRotations - sizeof(CPacketVector3)*m_NoOfTranslations - sizeof(CPacketVector3)*m_NoOfScales - sizeof(ZeroScaleType)*m_NoOfZeroScales); }
	CPacketVector3 *GetTranslations() const { return (CPacketVector3 *)GetBasePtr(); }
	QUATTYPE *GetRotations() const { return (QUATTYPE *)((u8 *)GetTranslations() + sizeof(CPacketVector3)*m_NoOfTranslations); }
	CPacketVector3 *GetScales() const { return (CPacketVector3 *)((u8 *)GetRotations() + sizeof(QUATTYPE)*m_NoOfRotations); }
	ZeroScaleType *GetZeroScales() const { return (ZeroScaleType *)((u8 *)GetScales() + sizeof(CPacketVector3)*m_NoOfScales); }
	CBoneBitField *GetBitField() const { return (CBoneBitField *)((u8 *)GetZeroScales() + sizeof(ZeroScaleType)*m_NoOfZeroScales); }
	CBoneBitField *GetScaleBitField() const { return (CBoneBitField *)((u8 *)GetBitField() + CBoneBitField::MemorySize(m_NoOfBones)); }

	void Store(crSkeleton *pSkeleton, float localTranslationEpsilon, float localQuatEpsilon, float zeroScaleEpsilonSqr, bool forceObjMtx);
	void StoreExtensions(crSkeleton *pSkeleton, float localTranslationEpsilon, float localQuatEpsilon, float zeroScaleEpsilonSqr, bool forceObjMtx = false) { PACKET_EXTENSION_RESET(CSkeletonBoneData); (void)pSkeleton; (void)localTranslationEpsilon; (void)localQuatEpsilon; (void)zeroScaleEpsilonSqr; (void)forceObjMtx; }
	void Extract(CEntity* pEntity, crSkeleton *pSkeleton, float zeroScaleEpsilonSqr, fragInst* pFragInst = NULL, BoneModifier *pModifier = NULL, const atArray<sVehicleIgnoredBones>* ignoredBones = NULL, const atArray<sVehicleIgnoredBones>* zeroScaleIgnoredBones = nullptr, const atArray<sInterpIgnoredBone>* noInterpBones = nullptr) const;
	void Extract(crSkeleton *pSkeleton, class CSkeletonBoneData *pNext, float interp, float zeroScaleEpsilonSqr, fragInst* pFragInst = NULL, BoneModifier *pModifier = NULL, const atArray<sVehicleIgnoredBones>* ignoredBones = NULL, const atArray<sVehicleIgnoredBones>* zeroScaleIgnoredBones = nullptr, const atArray<sInterpIgnoredBone>* noInterpBones = nullptr) const;
	static bool CanRotate(const rage::crBoneData *pBoneData);
	static bool CanTranslate(const rage::crBoneData *pBoneData);
	static s32 EstimateSize(crSkeleton *pSkeleton);

	static bool IsZeroScale(const Mat34V &matrix, float zeroScaleThresholdSqr);

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PacketID, "Validation of CSkeletonBoneData Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PacketID && CPacketBase::ValidatePacket();
	}

	bool ShouldPreventScaleFromBeingSet(fragInst* pFragInst, u16 boneIndex) const;

#if REPLAY_DELAYS_QUANTIZATION
public:
	void					QuantizeQuaternions();
	static void				ResetDelayedQuaternions();
#endif  // REPLAY_DELAYS_QUANTIZATION

public:
	u16 m_NoOfBones;
	u16 m_NoOfTranslations;
	u16 m_NoOfRotations;
	u16 m_NoOfScales;
	u16 m_NoOfZeroScales;
	bool m_bForceObjMtx;
	DECLARE_PACKET_EXTENSION(CSkeletonBoneData);
};


template<typename QUATTYPE>
CSkeletonBoneData<QUATTYPE>::CSkeletonBoneData()
	: CPacketBase(CSkeletonBoneData<QUATTYPE>::PacketID, sizeof(CSkeletonBoneData))
{
	m_NoOfBones = 0;
	m_NoOfRotations = 0;
	m_NoOfScales = 0;
	m_NoOfTranslations = 0;
}

template<typename QUATTYPE>
CSkeletonBoneData<QUATTYPE>::~CSkeletonBoneData()
{

}


template<typename QUATTYPE>
void CSkeletonBoneData<QUATTYPE>::Store(crSkeleton *pSkeleton, float localTranslationEpsilon, float localQuatEpsilon, float zeroScaleEpsilonSqr, bool forceObjMtx)
{
	PACKET_EXTENSION_RESET(CSkeletonBoneData);
	CPacketBase::Store(CSkeletonBoneData<QUATTYPE>::PacketID, sizeof(CSkeletonBoneData));
	const rage::crSkeletonData *pSkelData = &pSkeleton->GetSkeletonData();

	m_NoOfBones = (u16)pSkeleton->GetBoneCount();
	CPacketVector3 *pTranslations = Alloca(CPacketVector3, m_NoOfBones);
	QUATTYPE *pRotations = Alloca(QUATTYPE, m_NoOfBones);
	CPacketVector3 *pScales = Alloca(CPacketVector3, m_NoOfBones);
	ZeroScaleType *pZeroScales = Alloca(ZeroScaleType, m_NoOfBones+1); 	// We terminate the list with 0xffff.
	u8 *pBitFieldMem = Alloca(u8, CBoneBitField::MemorySize(m_NoOfBones));
	CBoneBitField *pBitField = new (pBitFieldMem) CBoneBitField(m_NoOfBones);
	u8 *pScaleBitFieldMem = Alloca(u8, CBoneBitField::MemorySize(m_NoOfBones));
	CBoneBitField *pScaleBitField = new (pScaleBitFieldMem) CBoneBitField(m_NoOfBones);

#if REPLAY_DELAYS_QUANTIZATION
	Quaternion *pQuaternions = Alloca(Quaternion, m_NoOfBones);
	tFragmentDeferredQuaternionBuffer::TempBucket QuatTempBuffer(m_NoOfBones, pQuaternions);
#endif // REPLAY_DELAYS_QUANTIZATION

	m_NoOfRotations = 0;
	m_NoOfScales = 0;
	m_NoOfTranslations = 0;
	m_NoOfZeroScales = 0; 
	m_bForceObjMtx = forceObjMtx;

	Mat34V *pObjectMatrices = pSkeleton->GetObjectMtxs();
	Vec3V scaleEpsilonSquared(SKELETON_BONE_DATA_SCALE_EPSILON*SKELETON_BONE_DATA_SCALE_EPSILON, SKELETON_BONE_DATA_SCALE_EPSILON*SKELETON_BONE_DATA_SCALE_EPSILON, SKELETON_BONE_DATA_SCALE_EPSILON*SKELETON_BONE_DATA_SCALE_EPSILON);
	ScalarV translationEpsilonSquared = ScalarV(localTranslationEpsilon*localTranslationEpsilon);

	for(u32 boneIndex = 0; boneIndex < m_NoOfBones; boneIndex++)
	{
		Mat34V localMtx, globalMtx;
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		if(CanRotate(pBoneData) || CanTranslate(pBoneData))
		{
			// Transform into space of parent (i.e. make a local matrix).
			if(!forceObjMtx && pBoneData->GetParent() != NULL)
			{
				Mat34V parentMtx = pObjectMatrices[pBoneData->GetParent()->GetIndex()];
				if( !IsZeroScale(parentMtx, zeroScaleEpsilonSqr) )
				{
					/*localMtx = pSkeleton->GetLocalMtx(boneIndex);*/ // Can't do this as it doesn't take scale into account properly.
					UnTransformOrtho(localMtx, parentMtx, pObjectMatrices[boneIndex]);
				}
				else
				{
					localMtx = pObjectMatrices[boneIndex];
				}
			}
			else
			{
				localMtx = pObjectMatrices[boneIndex];
			}

			// Test for zero scale.
			if( IsZeroScale(localMtx, zeroScaleEpsilonSqr) )
			{
				pZeroScales[m_NoOfZeroScales++] = (ZeroScaleType)boneIndex;
			}
			else
			{
				bool	bHasScale = false;
				Vec3V	scale;
				// Is the mtx orthonormal, if not store the scale 
				Matrix34 localMtx2 = MAT34V_TO_MATRIX34(localMtx);
				if(!localMtx2.IsOrthonormal())
				{
					scale = Vec3V(Mag(localMtx.GetCol0()).Getf(), Mag(localMtx.GetCol1()).Getf(), Mag(localMtx.GetCol2()).Getf());
					bHasScale = true;
					// Normalize localMtx
					localMtx2.Normalize();
					localMtx = MATRIX34_TO_MAT34V(localMtx2);
				}

#if REPLAY_DELAYS_QUANTIZATION
				if(BANK_SWITCH(CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag, true))
				{
					QuatV localQ = QuatVFromMat34V(localMtx);
					QuatV_ConstRef defaultQ = pBoneData->GetDefaultRotation();
					float QUATERNION_EPSILON = localQuatEpsilon;

					float delta = Max(fabsf(localQ.GetXf() - defaultQ.GetXf()), fabsf(localQ.GetYf() - defaultQ.GetYf()), fabsf(localQ.GetZf() - defaultQ.GetZf()), fabsf(localQ.GetWf() - defaultQ.GetWf()));

					if (delta > QUATERNION_EPSILON)
					{
						// Store the local one.
						QuatTempBuffer.AddQuaternion(RC_QUATERNION(localQ));
						pBitField->SetBit((boneIndex << 1));
						m_NoOfRotations++;
					}
				}
				else
#endif // REPLAY_DELAYS_QUANTIZATION
				{

					QuatV localQ = QuatVFromMat34V(localMtx);
					QuatV defaultQ = pBoneData->GetDefaultRotation();

					QUATTYPE localQPacket;
					QUATTYPE defaultQPacket;
					localQPacket.StoreQuaternion(RC_QUATERNION(localQ));
					defaultQPacket.StoreQuaternion(RC_QUATERNION(defaultQ));

					// Does the local packet differ from the default one ?
					if(defaultQPacket != localQPacket)
					{
						// Store the local one.
						pRotations[m_NoOfRotations++] = localQPacket;
						pBitField->SetBit((boneIndex << 1));
					}
				}

				if(bHasScale)
				{
					pScales[m_NoOfScales++].Store(RCC_VECTOR3(scale));
					pScaleBitField->SetBit((boneIndex << 1));
				}


				const Vec3V &localT = localMtx.GetCol3();
				const Vec3V &defaultT = pBoneData->GetDefaultTranslation();
				Vec3V diff = localT - defaultT;
				ScalarV epsilonTest = Dot(diff, diff) - translationEpsilonSquared;

				if(epsilonTest.Getf() > 0.0f)
				{
					// Store a translation.
					const Vec3V &objectT = pObjectMatrices[boneIndex].GetCol3();
					pTranslations[m_NoOfTranslations++].Store(RCC_VECTOR3(objectT));
					pBitField->SetBit((boneIndex << 1) + 1);
				}
			}
		}
	}

	// Terminate the zero scales list.
	pZeroScales[m_NoOfZeroScales++] = (ZeroScaleType)0xffff;

	// Account for bone data alignment in the packet size.
	u8 *pBasePtr = (u8 *)REPLAY_ALIGN((ptrdiff_t)((u8 *)this + sizeof(CSkeletonBoneData)), SKELETON_BONE_DATA_ALIGNMENT);
	u32 amountToAdd = (u32)((u8 *)pBasePtr - (u8 *)this) - GetPacketSize();
	AddToPacketSize(amountToAdd);

	// Account for the bone data itself.
	AddToPacketSize(m_NoOfRotations*sizeof(QUATTYPE) + m_NoOfTranslations*sizeof(CPacketVector3) + m_NoOfScales*sizeof(CPacketVector3) + m_NoOfZeroScales*sizeof(ZeroScaleType) + CBoneBitField::MemorySize(m_NoOfBones) + CBoneBitField::MemorySize(m_NoOfBones));

	u32 noOfRotationsToCopy = m_NoOfRotations;

#if REPLAY_DELAYS_QUANTIZATION
	// Add the quaternions to the deferred buffer + remember where they are kept (see QuantizeQuaternions())!
	if(BANK_SWITCH(CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag, true))
	{
		pRotations[0].Set(sm_DeferredQuaternionBuffer2.AddQuaternions(QuatTempBuffer));
		noOfRotationsToCopy = 1;
	}
#endif // REPLAY_DELAYS_QUANTIZATION

	// Write out the data.
	CPacketVector3 *pTranslationsDest = GetTranslations();
	QUATTYPE *pRotationsDest = GetRotations();
	CPacketVector3 *pScalesDest = GetScales();
	ZeroScaleType *pZeroScaleDest = GetZeroScales();
	CBoneBitField *pBitFieldDest = GetBitField();
	CBoneBitField *pScaleBitFieldDest = GetScaleBitField();

	memcpy(pTranslationsDest, pTranslations, m_NoOfTranslations*sizeof(CPacketVector3));
	memcpy(pRotationsDest, pRotations, noOfRotationsToCopy*sizeof(QUATTYPE));
	memcpy(pScalesDest, pScales, m_NoOfScales*sizeof(CPacketVector3));
	memcpy(pZeroScaleDest, pZeroScales, m_NoOfZeroScales*sizeof(ZeroScaleType));
	memcpy(pBitFieldDest, pBitField, CBoneBitField::MemorySize(m_NoOfBones));
	memcpy(pScaleBitFieldDest, pScaleBitField, CBoneBitField::MemorySize(m_NoOfBones));

	PACKET_EXTENSION_RESET(CSkeletonBoneData);
}

template<typename QUATTYPE>
void CSkeletonBoneData<QUATTYPE>::Extract(CEntity* pEntity, crSkeleton *pSkeleton, float zeroScaleEpsilonSqr, fragInst* pFragInst, BoneModifier *pModifier, const atArray<sVehicleIgnoredBones>* ignoredBones, const atArray<sVehicleIgnoredBones>* /*zeroScaleIgnoredBones*/, const atArray<sInterpIgnoredBone>* /*noInterpBones*/) const
{
	(void)pEntity;

	replayAssert(GET_PACKET_EXTENSION_VERSION(CSkeletonBoneData) == 0);
	const rage::crSkeletonData *pSkelData = &pSkeleton->GetSkeletonData();
	const Matrix34 &pedWorldspaceMtx = RCC_MATRIX34(*(pSkeleton->GetParentMtx()));

	CBoneBitField *pBitField = GetBitField();
	CBoneBitField *pScaleBitField = GetScaleBitField();
	CBoneDataIndexer indexer(GetTranslations(), GetRotations(), GetZeroScales(), GetScales());

	u16 numBones = Min(m_NoOfBones, (u16)pSkelData->GetNumBones());
	if(numBones != m_NoOfBones)
	{
		replayDebugf1("Number of bones doesn't match %u, %u in packet %p, %s", numBones, m_NoOfBones, this, pEntity->GetModelName());
	}
	Matrix34 *pGlobalMatrices = (Matrix34 *)Alloca(Matrix34, numBones);

	Matrix34 zeroMatrix;
	zeroMatrix.Zero();
	sVehicleIgnoredBones bone;

	for (u16 boneIndex = 0; boneIndex < numBones; boneIndex++) 
	{
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		Matrix34 boneMatrix; 
		boneMatrix.Identity();

		Quaternion rot;
		// Extract rotation...Do we have a rotation stored ?
		if(pBitField->GetBit((boneIndex << 1)))
		{
			// Obtain from the packet.
			indexer.GetNextRotation(rot);
		}
		else
		{
			rot = RCC_QUATERNION(pBoneData->GetDefaultRotation());
		}


		Matrix34 const *pBoneParentMatrix;

		if(m_bForceObjMtx || pBoneData->GetParent() == NULL)
			pBoneParentMatrix = &pedWorldspaceMtx;
		else
			pBoneParentMatrix = &pGlobalMatrices[pBoneData->GetParent()->GetIndex()];

		if(pModifier && pModifier->ShouldModifyBoneLocalRotation(boneIndex))
			rot = pModifier->ModifyBoneLocalRotation(rot, *pBoneParentMatrix, pedWorldspaceMtx);

		Vector3 scale;
		// Extract scale...Do we have a scale stored ?
		if(pScaleBitField->GetBit((boneIndex << 1)))
		{
			// Obtain from the packet.
			indexer.GetNextScale(scale);
		}
		else
		{
			scale = RCC_VECTOR3(pBoneData->GetDefaultScale());
		}

		// Form rotational part of local matrix.
		boneMatrix.FromQuaternion(rot);
		replayAssert(boneMatrix.IsOrthonormal());

		if(ShouldPreventScaleFromBeingSet(pFragInst, boneIndex))
		{
			boneMatrix.Scale(RCC_VECTOR3(pBoneData->GetDefaultScale()));
		}
		else
		{
			boneMatrix.Scale(scale);

			// Set to zero scale if needs be.
			if(indexer.GetNextZeroScale((ZeroScaleType)boneIndex))
				boneMatrix = zeroMatrix;
		}

		// Set an initial local default translation.
		boneMatrix.d = RCC_VECTOR3(pBoneData->GetDefaultTranslation());

		// Form a world-space matrix.
		if(m_bForceObjMtx || pBoneData->GetParent() == NULL)
			pGlobalMatrices[boneIndex].Dot(boneMatrix, pedWorldspaceMtx);
		else
		{
			Matrix34 parentMtx = pGlobalMatrices[pBoneData->GetParent()->GetIndex()];

			if( !IsZeroScale(MATRIX34_TO_MAT34V(parentMtx), zeroScaleEpsilonSqr) )
			{
				pGlobalMatrices[boneIndex].Dot(boneMatrix, pGlobalMatrices[pBoneData->GetParent()->GetIndex()]);  // Transform by parent.
			}
			else
			{
				pGlobalMatrices[boneIndex].Dot(boneMatrix, pedWorldspaceMtx);
			}
		}

		if(pBitField->GetBit((boneIndex << 1) + 1))
		{
			Vector3 objectT;
			indexer.GetNextTranslation(objectT);
			pedWorldspaceMtx.Transform(objectT, pGlobalMatrices[boneIndex].d);
		}

		pSkeleton->SetGlobalMtx((s32)boneIndex, RCC_MAT34V(pGlobalMatrices[boneIndex]));
	}

	if(ignoredBones)
	{
		for(int i = 0; i < ignoredBones->GetCount(); ++i)
		{
			const sVehicleIgnoredBones& boneOverride = (*ignoredBones)[i];
			pSkeleton->SetGlobalMtx((s32)boneOverride.m_boneIdx, RCC_MAT34V(boneOverride.m_ignoredBoneOverrideMtx));
		}
	}
}

template<typename QUATTYPE>
void CSkeletonBoneData<QUATTYPE>::Extract(crSkeleton *pSkeleton, class CSkeletonBoneData *pNextPacket, float interp, float zeroScaleEpsilonSqr, fragInst* pFragInst, BoneModifier *pModifier, const atArray<sVehicleIgnoredBones>* ignoredBones, const atArray<sVehicleIgnoredBones>* zeroScaleIgnoredBones, const atArray<sInterpIgnoredBone>* noInterpBones) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CSkeletonBoneData) == 0);
	const rage::crSkeletonData *pSkelData = &pSkeleton->GetSkeletonData();
	const Matrix34 &pedWorldspaceMtx = RCC_MATRIX34(*(pSkeleton->GetParentMtx()));

	CBoneBitField *pBitField = GetBitField();
	CBoneBitField *pScaleBitField = GetScaleBitField();
	CBoneDataIndexer indexer(GetTranslations(), GetRotations(), GetZeroScales(), GetScales());

	CBoneBitField *pBitFieldNext = pNextPacket->GetBitField();
	CBoneBitField *pScaleBitFieldNext = pNextPacket->GetScaleBitField();
	CBoneDataIndexer indexerNext(pNextPacket->GetTranslations(), pNextPacket->GetRotations(), pNextPacket->GetZeroScales(), pNextPacket->GetScales());

	u16 numBones = Min(m_NoOfBones, (u16)pSkelData->GetNumBones());
	if(numBones != m_NoOfBones)
	{
		replayDebugf1("Number of bones doesn't match %u, %u in packet %p", numBones, m_NoOfBones, this);
	}
	Matrix34 *pGlobalMatrices = (Matrix34 *)Alloca(Matrix34, numBones);

	Matrix34 zeroMatrix;
	zeroMatrix.Zero();
	sVehicleIgnoredBones bone;
	sInterpIgnoredBone interpIgnoredBone;

	for (u16 boneIndex = 0; boneIndex < numBones; boneIndex++) 
	{
		const rage::crBoneData *pBoneData = pSkelData->GetBoneData(boneIndex);

		Matrix34 boneMatrix; 
		boneMatrix.Identity();

		Quaternion rQuatNext = RCC_QUATERNION(pBoneData->GetDefaultRotation());
		Quaternion rQuatPrev= RCC_QUATERNION(pBoneData->GetDefaultRotation());

		// Extract rotation.
		bool hasPrevRotation = false;
		bool hasNextRotation = false;
		if(pBitField->GetBit((boneIndex << 1)))
		{
			indexer.GetNextRotation(rQuatPrev);
			hasPrevRotation = true;
		}
		if(pBitFieldNext->GetBit((boneIndex << 1)))
		{
			indexerNext.GetNextRotation(rQuatNext);
			hasNextRotation = true;
		}

		Matrix34 const *pBoneParentMatrix;

		if(m_bForceObjMtx || pBoneData->GetParent() == NULL)
			pBoneParentMatrix = &pedWorldspaceMtx;
		else
			pBoneParentMatrix = &pGlobalMatrices[pBoneData->GetParent()->GetIndex()];

		if(pModifier && pModifier->ShouldModifyBoneLocalRotation(boneIndex))
		{
			rQuatPrev = pModifier->ModifyBoneLocalRotation(rQuatPrev, *pBoneParentMatrix, pedWorldspaceMtx);
			rQuatNext = pModifier->ModifyBoneLocalRotation(rQuatNext, *pBoneParentMatrix, pedWorldspaceMtx);
		}

		bool interpBone = true;
		interpIgnoredBone.m_boneIdx = boneIndex;
		if(noInterpBones)
		{
			int i = noInterpBones->Find(interpIgnoredBone);
			if(i != -1)
			{
				const sInterpIgnoredBone& b = (*noInterpBones)[i];
				// For url:bugstar:3605847
				// The shutters on the SAM launcher...when they open they don't interpolate (or we don't catch it) so on playback we want them to just
				// ping open without interpolation.  However when all missiles are fired the shutters all close slowly which we do capture.  The only way
				// to tell them apart is whether the previous rotation was the default...and if it was avoid interpolation.
				if(b.m_onlyIfPrevIsDefault)
				{
					if(rQuatPrev.IsEqual(RCC_QUATERNION(pBoneData->GetDefaultRotation())))
						interpBone = false;
				}
				else
				{
					interpBone = false;
				}
			}
		}

		// If we know we have a previous rotation but no next rotation (i.e. we're going for default)
		// AND the angle between them is high(ish) then don't interpolate.  This is being used to fix
		// url:bugstar:4500569 where the doors are interpolating shut even though they shouldn't (we 
		// don't have a rotation on the next frame so it interpolates to default).
		float relativeAngle = rQuatPrev.RelAngle(rQuatNext);
		if(hasPrevRotation && !hasNextRotation && relativeAngle > 1.0f)
		{
			interpBone = false;
		}

		// Interpolate the rotation.
		Quaternion rQuatInterp = rQuatPrev;
		if(interpBone)
		{
			rQuatPrev.PrepareSlerp(rQuatNext);
			rQuatInterp.Slerp(interp, rQuatPrev, rQuatNext);
		}

		Vector3 scale;
		// Extract scale...Do we have a scale stored ?
		if(pScaleBitField->GetBit((boneIndex << 1)))
		{
			// Obtain from the packet.
			indexer.GetNextScale(scale);
		}
		else
		{
			scale = RCC_VECTOR3(pBoneData->GetDefaultScale());
		}
		Vector3 scaleNext;
		// Extract scale...Do we have a scale stored ?
		if(pScaleBitFieldNext->GetBit((boneIndex << 1)))
		{
			// Obtain from the packet.
			indexerNext.GetNextScale(scaleNext);
		}
		else
		{
			scaleNext = RCC_VECTOR3(pBoneData->GetDefaultScale());
		}

		// Form the matrix.
		boneMatrix.FromQuaternion(rQuatInterp);
		replayAssert(boneMatrix.IsOrthonormal());

		if(ShouldPreventScaleFromBeingSet(pFragInst, boneIndex))
		{
			boneMatrix.Scale(RCC_VECTOR3(pBoneData->GetDefaultScale()));
		}
		else
		{
			// Interpolate the scale
			Vector3 interpScale = scale*(1.0f - interp) + interp*scaleNext;
			boneMatrix.Scale(interpScale);

			// Set to zero matrix if previous or next has zero scale (can`t interpolate).
			if(indexer.GetNextZeroScale((ZeroScaleType)boneIndex))
			{
				bool applyZero = true;
				if(zeroScaleIgnoredBones)
				{
					bone.m_boneIdx = boneIndex;
					int idx = zeroScaleIgnoredBones->Find(bone);
					if(idx != -1)
					{
						applyZero = false;
						continue;
					}
				}
				if(applyZero)
					boneMatrix = zeroMatrix;
			}
		}

		Vector3 defaultTNext = RCC_VECTOR3(pBoneData->GetDefaultTranslation());
		Vector3 defaultTPrev = RCC_VECTOR3(pBoneData->GetDefaultTranslation());
		// Set an initial local default translation.
		boneMatrix.d = defaultTPrev;

		// Form a world-space matrix.
		if(m_bForceObjMtx || pBoneData->GetParent() == NULL)
			pGlobalMatrices[boneIndex].Dot(boneMatrix, pedWorldspaceMtx);
		else
		{
			Matrix34 parentMtx = pGlobalMatrices[pBoneData->GetParent()->GetIndex()];
			if( !IsZeroScale(MATRIX34_TO_MAT34V(parentMtx), zeroScaleEpsilonSqr) )
			{
				pGlobalMatrices[boneIndex].Dot(boneMatrix, pGlobalMatrices[pBoneData->GetParent()->GetIndex()]);  // Transform by parent.
			}
			else
			{
				pGlobalMatrices[boneIndex].Dot(boneMatrix, pedWorldspaceMtx);
			}
		}

		bool hasTrans = false;

		// Grab world-space position of result of apply default translation.
		defaultTPrev = defaultTNext = pGlobalMatrices[boneIndex].d;
		if(pBitField->GetBit((boneIndex << 1) + 1))
		{
			Vector3 v;
			indexer.GetNextTranslation(v);
			pedWorldspaceMtx.Transform(v, defaultTPrev);
			hasTrans = true;
		}
		if(pBitFieldNext->GetBit((boneIndex << 1) + 1))
		{
			Vector3 v;
			indexerNext.GetNextTranslation(v);
			pedWorldspaceMtx.Transform(v, defaultTNext);
			hasTrans = true;
		}
		if(hasTrans)
			pGlobalMatrices[boneIndex].d = defaultTPrev*(1.0f - interp) + interp*defaultTNext;

		pSkeleton->SetGlobalMtx((s32)boneIndex, RCC_MAT34V(pGlobalMatrices[boneIndex]));
	}

	if(ignoredBones)
	{
		for(int i = 0; i < ignoredBones->GetCount(); ++i)
		{
			const sVehicleIgnoredBones& boneOverride = (*ignoredBones)[i];
			pSkeleton->SetGlobalMtx((s32)boneOverride.m_boneIdx, RCC_MAT34V(boneOverride.m_ignoredBoneOverrideMtx));
		}
	}
}

template<typename QUATTYPE>
bool CSkeletonBoneData<QUATTYPE>::CanRotate(const rage::crBoneData *pBoneData)
{
	if(pBoneData->GetDofs() & ((u16)rage::crBoneData::ROTATE_X | (u16)rage::crBoneData::ROTATE_Y | (u16)rage::crBoneData::ROTATE_Z))
		return true;
	return false;
}

template<typename QUATTYPE>
bool CSkeletonBoneData<QUATTYPE>::CanTranslate(const rage::crBoneData *pBoneData)
{
	if(pBoneData->GetDofs() & ((u16)rage::crBoneData::TRANSLATE_X | (u16)rage::crBoneData::TRANSLATE_Y | (u16)rage::crBoneData::TRANSLATE_Z))
		return true;
	return false;
}

template<typename QUATTYPE>
s32 CSkeletonBoneData<QUATTYPE>::EstimateSize(crSkeleton *pSkeleton)
{
	s32 sizeThis = sizeof(CSkeletonBoneData);
	s32 sizeBoneData = CBoneBitField::MemorySize(pSkeleton->GetBoneCount()) + pSkeleton->GetBoneCount()*(sizeof(CPacketVector3) + sizeof(QUATTYPE));
	// Allow for alignment of bone data.
	return sizeThis + sizeBoneData + (SKELETON_BONE_DATA_ALIGNMENT-1);
}

template<typename QUATTYPE>
bool CSkeletonBoneData<QUATTYPE>::IsZeroScale(const Mat34V &matrix, float zeroScaleThresholdSqr)
{
	if( MagSquared(matrix.GetCol0()).Getf() < zeroScaleThresholdSqr && MagSquared(matrix.GetCol1()).Getf() < zeroScaleThresholdSqr && MagSquared(matrix.GetCol2()).Getf() < zeroScaleThresholdSqr )
	{
		return true;
	}
	return false;
}
template<typename QUATTYPE>
bool CSkeletonBoneData<QUATTYPE>::ShouldPreventScaleFromBeingSet(fragInst* pFragInst, u16 boneIndex) const
{
	bool preventScalFromBeingSet = false;

	if(pFragInst && pFragInst->GetTypePhysics())
	{
		int group = pFragInst->GetGroupFromBoneIndex(boneIndex);

		//make sure a bone with glass on is broken before we allow the scale to be zeroed out
		if(group != -1)
			preventScalFromBeingSet = pFragInst->GetTypePhysics()->GetGroup(group)->GetMadeOfGlass() && pFragInst->GetGroupBroken(group) == false;
	}

	return preventScalFromBeingSet;
}


#if REPLAY_DELAYS_QUANTIZATION
template<typename QUATTYPE>
void CSkeletonBoneData<QUATTYPE>::QuantizeQuaternions()
{
	BANK_ONLY(if(CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag))
	{
		if(m_NoOfRotations > 0)
		{
			QUATTYPE* pRotations = GetRotations();
			sm_DeferredQuaternionBuffer2.PackQuaternions((u32)pRotations[0].Get(), m_NoOfRotations, pRotations);
		}
	}
}

template<typename QUATTYPE>
void CSkeletonBoneData<QUATTYPE>::ResetDelayedQuaternions()
{
	sm_DeferredQuaternionBuffer2.Reset();
}
#endif // REPLAY_DELAYS_QUANTIZATION



template<typename QUATTYPE>
class CPacketFragBoneDataUsingSkeletonPackets : public CPacketBase
{
public:

	static eReplayPacketId PacketID;

	void 	Store(CEntity* pEntity, float translationEpsilon, float quaternionEpsilon);
	void 	StoreExtensions(CEntity* pEntity, float translationEpsilon, float quaternionEpsilon) { PACKET_EXTENSION_RESET(CPacketFragBoneDataUsingSkeletonPackets); (void)pEntity; (void)translationEpsilon; (void)quaternionEpsilon; }
	void 	Extract(CEntity* pEntity, CPacketFragBoneDataUsingSkeletonPackets const* pNextPacket, float fInterpValue, bool bUpdateWorld = true, BoneModifier *pModifier = NULL, const atArray<sVehicleIgnoredBones>* ignoredBones = NULL, const atArray<sVehicleIgnoredBones>* zeroScaleIgnoredBones = nullptr, const atArray<sInterpIgnoredBone>* noInterpBones = nullptr) const;

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PacketID, "Validation of CPacketFragBoneDataUsingSkeletonPackets Failed!, 0x%08X", GetPacketID());
	#if REPLAY_GUARD_ENABLE
		int* pEnd = (int*)((u8*)this + GetPacketSize() - sizeof(int));
		replayAssertf((*pEnd == REPLAY_GUARD_END_DATA), "Validation of CPacketFragBoneDataUsingSkeletonPackets Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PacketID && CPacketBase::ValidatePacket() && (*pEnd == REPLAY_GUARD_END_DATA);
	#else
		return GetPacketID() == PacketID && CPacketBase::ValidatePacket();
	#endif // REPLAY_GUARD_ENABLE
	}

	bool HasDamagedSkeleton() const { return GetPadBool(0); }
	CSkeletonBoneData<QUATTYPE> *GetUndamaged() const { return (CSkeletonBoneData<QUATTYPE> *)((ptrdiff_t)this + (ptrdiff_t)this->m_OffsetToUndamaged); }
	CSkeletonBoneData<QUATTYPE> *GetDamaged() const { return HasDamagedSkeleton() ? (CSkeletonBoneData<QUATTYPE> *)(GetUndamaged()->TraversePacketChain(1)) : NULL; }

	static s32 EstimatePacketSize(const CEntity* pEntity);

	u16	m_OffsetToUndamaged;
	DECLARE_PACKET_EXTENSION(CPacketFragBoneDataUsingSkeletonPackets);
};


//////////////////////////////////////////////////////////////////////////
// CPacketFragBoneDataUsingSkeletonPackets.
//////////////////////////////////////////////////////////////////////////
template<typename QUATTYPE>
void CPacketFragBoneDataUsingSkeletonPackets<QUATTYPE>::Store(CEntity* pEntity, float translationEpsilon, float quaternionEpsilon)
{
	PACKET_EXTENSION_RESET(CPacketFragBoneDataUsingSkeletonPackets);
	replayAssert(pEntity && "CPacketFragBoneData2::Store: Invalid entity");
	replayAssert(pEntity->GetFragInst() && "CPacketFragBoneData2::Store: Invalid frag inst");

	CPacketBase::Store(CPacketFragBoneDataUsingSkeletonPackets<QUATTYPE>::PacketID, sizeof(CPacketFragBoneDataUsingSkeletonPackets));

	SetPadBool(0, false);
	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if(pFragInst)
	{
		fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

		if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
		{
			// Form an aligned pointer to the undamaged skeleton packet.
			CSkeletonBoneData<QUATTYPE> *pSkeletonPacket = (CSkeletonBoneData<QUATTYPE> *)REPLAY_ALIGN((ptrdiff_t)(&this[1]), SKELETON_BONE_DATA_ALIGNMENT);
			// Record the offset.
			this->m_OffsetToUndamaged = (u16)((ptrdiff_t)pSkeletonPacket - (ptrdiff_t)this);
			// Store the data.
			pSkeletonPacket->Store(pFragCacheEntry->GetHierInst()->skeleton, translationEpsilon, quaternionEpsilon, SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR, false);

			if(pFragCacheEntry->GetHierInst()->damagedSkeleton)
			{
				// Form a pointer to the damaged version.
				pSkeletonPacket = (CSkeletonBoneData<QUATTYPE> *)pSkeletonPacket->TraversePacketChain(1);
				pSkeletonPacket->Store(pFragCacheEntry->GetHierInst()->damagedSkeleton, translationEpsilon, quaternionEpsilon, SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR, false);
				SetPadBool(0, true);
			}

			// Account for all the skeleton data written.
			pSkeletonPacket = (CSkeletonBoneData<QUATTYPE> *)pSkeletonPacket->TraversePacketChain(1, false);
			u32 amountToAdd = (u32)((u8 *)pSkeletonPacket - (u8 *)this) - GetPacketSize();
			AddToPacketSize(amountToAdd);
		}
	}

#if REPLAY_GUARD_ENABLE
	AddToPacketSize(sizeof(int));
	int* pEnd = (int*)((u8*)this + GetPacketSize() - sizeof(int));
	*pEnd = REPLAY_GUARD_END_DATA;
#endif // REPLAY_GUARD_ENABLE
}


//////////////////////////////////////////////////////////////////////////
template<typename QUATTYPE>
void CPacketFragBoneDataUsingSkeletonPackets<QUATTYPE>::Extract(CEntity* pEntity, CPacketFragBoneDataUsingSkeletonPackets const* pNextPacket, float fInterpValue, bool bUpdateWorld, BoneModifier * pModifier, const atArray<sVehicleIgnoredBones>* ignoredBones, const atArray<sVehicleIgnoredBones>* zeroScaleIgnoredBones, const atArray<sInterpIgnoredBone>* noInterpBones) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketFragBoneDataUsingSkeletonPackets) == 0);
	replayAssert(pEntity);

	if(pEntity && pEntity->GetFragInst() && pEntity->GetFragInst()->GetCacheEntry())
	{
		fragCacheEntry* pFragCacheEntry = pEntity->GetFragInst()->GetCacheEntry();

		if (pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterpValue == 0.0f)
		{
			// Just use "previous" frame.
			GetUndamaged()->Extract(pEntity, pFragCacheEntry->GetHierInst()->skeleton, SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR, pEntity->GetFragInst(), pModifier, ignoredBones, zeroScaleIgnoredBones, noInterpBones);

			// Same for damaged skeleton.
			if(GetDamaged() && pFragCacheEntry->GetHierInst()->damagedSkeleton)
				GetDamaged()->Extract(pEntity, pFragCacheEntry->GetHierInst()->damagedSkeleton, SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR, pEntity->GetFragInst(), pModifier, ignoredBones, zeroScaleIgnoredBones, noInterpBones);

		}
		else if (pNextPacket)
		{
			// Interpolate between frames.
			GetUndamaged()->Extract(pFragCacheEntry->GetHierInst()->skeleton, pNextPacket->GetUndamaged(), fInterpValue, SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR, pEntity->GetFragInst(), pModifier, ignoredBones, zeroScaleIgnoredBones, noInterpBones);

			// Same for damaged skeleton.
			if(GetDamaged()  && pNextPacket->GetDamaged() && pFragCacheEntry->GetHierInst()->damagedSkeleton)
				GetDamaged()->Extract(pFragCacheEntry->GetHierInst()->damagedSkeleton, pNextPacket->GetDamaged(), fInterpValue, SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR, pEntity->GetFragInst(), pModifier, ignoredBones, zeroScaleIgnoredBones, noInterpBones);
		}
		else
		{
			replayAssert(0 && "CPacketFragBoneDataUsingSkeletonPackets::Load - interpolation couldn't find a next packet");
		}
		if (bUpdateWorld)
		{
			CGameWorld::Remove(pEntity);
			CGameWorld::Add(pEntity, CGameWorld::OUTSIDE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename QUATTYPE>
s32 CPacketFragBoneDataUsingSkeletonPackets<QUATTYPE>::EstimatePacketSize(const CEntity* pEntity)
{
	replayAssert(pEntity && "CPacketFragBoneDataUsingSkeletonPackets::EstimatePacketSize: Invalid entity");

	u32 ret = 0;
	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if (pFragInst)
	{
		fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

		if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
		{
			// Allow for alignment of skeleton packets.
			ret += SKELETON_BONE_DATA_ALIGNMENT - 1;
			ret += CSkeletonBoneData<QUATTYPE>::EstimateSize(pFragCacheEntry->GetHierInst()->skeleton);

			if (pFragCacheEntry->GetHierInst()->damagedSkeleton)
				ret += CSkeletonBoneData<QUATTYPE>::EstimateSize(pFragCacheEntry->GetHierInst()->damagedSkeleton);
		}
	}
#if REPLAY_GUARD_ENABLE
	// Adding an int on the end as a guard
	ret += sizeof(int);
#endif // REPLAY_GUARD_ENABLE
	return ret + sizeof(CPacketFragBoneDataUsingSkeletonPackets);	
}



#endif // GTA_REPLAY

#endif // INC_FRAGMENTPACKET_H_
