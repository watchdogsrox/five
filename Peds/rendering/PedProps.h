// Title	:	PedProps.h
// Author	:	John Whyte
// Started	:	5/8/05

#ifndef _PEDPROPS_H_
#define _PEDPROPS_H_

#pragma once
#include "atl/array.h"

#include "parser/macros.h"
#include "renderer/renderer.h"
#include "streaming/streamingrequest.h"
#include "debug/debug.h"
#include "vector/quaternion.h"

#include "fwdrawlist/drawlistmgr.h"
#include "fwnet/netserialisers.h"

#include "control/replay/ReplaySettings.h"

namespace rage {
class crSkeleton;
class crSkeletonData;
#if __BANK
	class bkBank;
#endif
}
class CPed;
class CPedModelInfo;
class CDynamicEntity;
class CPedVariationData;
class CPedVariationInfoCollection;
class CPedPropRenderGfx;
class CPedPropRequestGfx;
class CPedPropRestriction;
class CCustomShaderEffectPed;
class CScriptPreloadData;
class CPedStreamRequestGfx;

enum eAnchorPoints
{
	// Note: if this changes, please remember to update the corresponding <enumdef>
	// in 'pedvariations.psc'.

	ANCHOR_NONE = -1,
	ANCHOR_HEAD = 0,
	ANCHOR_EYES,
	ANCHOR_EARS,
	ANCHOR_MOUTH,
	ANCHOR_LEFT_HAND,
	ANCHOR_RIGHT_HAND,
	ANCHOR_LEFT_WRIST,
	ANCHOR_RIGHT_WRIST,
	ANCHOR_HIP,
	ANCHOR_LEFT_FOOT,
	ANCHOR_RIGHT_FOOT,
	ANCHOR_PH_L_HAND,
	ANCHOR_PH_R_HAND,
	NUM_ANCHORS,
};

// --- defines ---
// maximum no. of props allowed for any slot
// maximum no. of textures allowed for any prop
#define MAX_PED_PROP_TEX	(8)
// max no. of props each ped may have
#define MAX_PROPS_PER_PED	(6)

#define MAX_PROP_EXPRESSION_MODS (5)

#define PED_PROP_NONE	(-1)

#define HEAD_NAME "SKEL_Head"
#define RHAND_NAME "SKEL_R_Hand"
#define LHAND_NAME "SKEL_L_Hand"
// #define LWRIST_NAME "SKEL_L_Forearm"
// #define RWRIST_NAME "SKEL_R_Forearm"
#define LWRIST_NAME "RB_L_ForeArmRoll"
#define RWRIST_NAME "RB_R_ForeArmRoll"
#define HIP_NAME "SKEL_Pelvis"
#define LFOOT_NAME "SKEL_L_Foot"
#define RFOOT_NAME "SKEL_R_Foot"
#define PH_LHAND_NAME "PH_L_Hand"
#define PH_RHAND_NAME "PH_R_Hand"

// --------------

// ---for storing global prop data---
struct propData{
	s32		slot;
	s32		slotIdx;
	s32		flags;
};

struct CPedPropsGlobalData {
	s32				propsFileHashcode;
	atArray<propData>	propsData;
};
// -----------

struct sPropSortedItem 
{
	u8 anchorId;
	u8 propId;
	u8 texId;
	u8 count;
	u8 distribution;
	u8 occurrence;
};

class CPedPropTexData
{
public:
	atFixedBitSet32 inclusions;
	atFixedBitSet32 exclusions;
	u8 texId;
	u8 inclusionId;             // TODO: remove me when tools stop using this
	u8 exclusionId;             // TODO: remove me when tools stop using this
	u8 distribution;	// spawn probability for this texture

	PAR_SIMPLE_PARSABLE;
};

enum ePropRenderFlags
{
	PRF_ALPHA = 0,
	PRF_DECAL,
	PRF_CUTOUT
};

class CPedPropMetaData
{
public:
	atHashString audioId;
	float expressionMods[MAX_PROP_EXPRESSION_MODS];	// expression modifiers
	atArray<CPedPropTexData> texData;
	atFixedBitSet<3, u8> renderFlags;
	u32 propFlags;

	u16 flags;			// bit mask storing the ePedCompFlags for this prop

	u8 anchorId;		// anchor and prop to identify which prop this data applies to
	u8 propId;
	u8 stickyness;		// chance of removing the prop after damage to ped

	PAR_SIMPLE_PARSABLE;
};

class CAnchorProps
{
public:
	atArray<u8> props;
	eAnchorPoints anchor;

	PAR_SIMPLE_PARSABLE;
};

// prop data which is required for every instance of a ped
class CPedPropData
{
friend class CPedPropsMgr;
friend class CCutSceneActor;
friend class CPlayerSettingsObject;
friend class CPlayerSettingsMgr;

#if GTA_REPLAY
	friend class CPacketPedUpdate;
#endif

public:
	CPedPropData(void);
	~CPedPropData(void);

	bool	GetIsCurrPropsAlpha(void) const	{return(m_bIsCurrPropsAlpha);	}
	bool	GetIsCurrPropsDecal(void) const	{return(m_bIsCurrPropsDecal);	}
	bool	GetIsCurrPropsCutout(void) const {return(m_bIsCurrPropsCutout);	}

	void	SetSkipRender(bool bState) {m_bSkipRender = bState;}
	bool	GetSkipRender() {return(m_bSkipRender);}

	eAnchorPoints GetAnchor(u32 idx) const { Assert(idx < MAX_PROPS_PER_PED); return m_aPropData[idx].m_anchorID; }
	s32		GetPropId(u32 idx) const { Assert(idx < MAX_PROPS_PER_PED); return m_aPropData[idx].m_propModelID; }
	s32		GetTextureId(u32 idx) const { Assert(idx < MAX_PROPS_PER_PED); return m_aPropData[idx].m_propTexID; }
	

	u32     SetPreloadProp(CPed* pPed, eAnchorPoints anchorID, s32 propID, s32 texID, u32 streamingFlags = 0);
	bool	HasPreloadPropsFinished();
    bool    HasPreloadPropsFinished(u32 handle);
	void	CleanUpPreloadProps();
    void    CleanUpPreloadPropsHandle(u32 handle);

	struct SinglePropData 
	{
		SinglePropData();

		eAnchorPoints	m_anchorID;
		s32				m_propModelID;
		s32				m_propTexID;

		u32				m_propModelHash;
		u32				m_propTexHash;

		eAnchorPoints	m_anchorOverrideID;
		Vector3			m_posOffset;
		Quaternion		m_rotOffset;

		bool			m_delayedDelete;
	};

	SinglePropData&	GetPedPropData(u32 idx) { Assert(idx < MAX_PROPS_PER_PED); return(m_aPropData[idx]); }
	const SinglePropData& GetPedPropData(u32 idx) const { Assert(idx < MAX_PROPS_PER_PED); return(m_aPropData[idx]); }

protected:

	void DelayedDelete(u32 idx);
	eAnchorPoints	GetPedPropActualAnchorPoint(u32 idx) const;
	eAnchorPoints	GetPedPropActualAnchorPoint(eAnchorPoints anchorID) const;
	bool	GetPedPropPosOffset(eAnchorPoints anchorID, Vector3& vec);
	bool	GetPedPropRotOffset(eAnchorPoints anchorID, Quaternion& quat);
	s32	GetPedPropIdx(eAnchorPoints anchorID) const;
	s32	GetPedPropTexIdx(eAnchorPoints anchorID) const;
	bool GetIsPedPropDelayedDelete(eAnchorPoints anchorID) const;
	bool	GetPedPropHashes(eAnchorPoints anchorID, u32& modelHash, u32& texHash);
	
	void		SetPedProp(CPed* pPed, eAnchorPoints anchorID, s32 propID, s32 texID, eAnchorPoints anchorOverrideID, Vector3 *pPosOffset, Quaternion *pRotOffset, u32 streamingFlags = 0);
	void		ClearPedProp(CPed* ped, eAnchorPoints anchorID, bool bForceClear);

	void UndoAlternateDrawable(CPed* pPed, s32 anchor, s32 oldProp);

public:
	bool GetIsAnyPedPropDelayedDeleted() const;
	void		ClearAllPedProps(CPed* ped);
	u8	GetNumActiveProps() const;

private:
	SinglePropData	m_aPropData[MAX_PROPS_PER_PED];
	CScriptPreloadData* m_preloadData;

	u8	m_bIsCurrPropsAlpha		:1;	// true if current prop selection has alpha
	u8	m_bIsCurrPropsDecal		:1;	// true if current prop selection has decal
	u8	m_bIsCurrPropsCutout	:1;	// true if current prop selection has cutout
	u8	m_bSkipRender			:1; // don't render these props for some reaon
	u8	m_padpad				:4;

};

// prop data which is required for the archetype (modelInfo) of a ped
class CPedPropInfo
{
friend class CPedPropsMgr;
friend class CPedVariationInfoCollection;
BANK_ONLY(friend class CPedVariationDebug;)

public:
	CPedPropInfo(void);
	~CPedPropInfo(void);

	bool CheckFlagsOnAnyProp(u8 anchorId, u32 flags) const;

protected:	
	u8 GetMaxNumProps(eAnchorPoints anchorPointIdx) const;

    u8 GetMaxNumPropsTex(eAnchorPoints anchorPointIdx, u32 propId);
    u8 GetMaxNumPropsTex(eAnchorPoints anchorPointIdx, u32 propId) const;

	const atFixedBitSet32* GetInclusions(u8 anchorId, u8 propId, u8 texId) const;
	const atFixedBitSet32* GetExclusions(u8 anchorId, u8 propId, u8 texId) const;

	s32 GetPropIdxWithFlags(u8 anchorId, u32 propFlags) const;

private:
	u8 m_numAvailProps;

	atArray<CPedPropMetaData> m_aPropMetaData;	// stores potential meta data for each prop
	atArray<CAnchorProps> m_aAnchors;

	PAR_SIMPLE_PARSABLE;
};

// Class to encapsulate an array of u32's which describe the props for a ped, in reality we only actually sync SIZEOF_COMBINED + SIZEOF_HASH per prop
class CPackedPedProps
{
public:

	static const u32 SIZEOF_PROP_IDX_BITS = 8;
	static const u32 SIZEOF_ANCHOR_ID_BITS = 3;
	static const u32 SIZEOF_ANCHOR_OVERRIDE_ID_BITS = SIZEOF_ANCHOR_ID_BITS;
	static const u32 SIZEOF_TEXTURE_IDX_BITS = 5;
	static const u32 SIZEOF_COMBINED = SIZEOF_PROP_IDX_BITS + SIZEOF_ANCHOR_ID_BITS + SIZEOF_ANCHOR_OVERRIDE_ID_BITS + SIZEOF_TEXTURE_IDX_BITS;
	static const u32 SIZEOF_HASH = 32;

	CompileTimeAssert(SIZEOF_COMBINED <= 32);

	CPackedPedProps() { Reset(); }

	bool operator == (const CPackedPedProps& rhs) const
	{
		bool equal = true;

		for (int i = 0; i < MAX_PROPS_PER_PED; i++)
		{
			u32 propData = 0;
			u32 propVarInfoHash = 0;

			rhs.GetPackedPedPropData(i, propVarInfoHash, propData);
			equal &= (m_packedPedProps[i] == propData && m_propVarInfoHashes[i] == propVarInfoHash);
		}

		return equal;
	}

	bool operator != (const CPackedPedProps& rhs) const
	{
		return !(*this == rhs);
	}

	CPackedPedProps& operator = (const CPackedPedProps& rhs)
	{
		for (int i = 0; i < MAX_PROPS_PER_PED; i++)
		{
			u32 propData = 0;
			u32 propVarInfoHash = 0;

			rhs.GetPackedPedPropData(i, propVarInfoHash, propData);
			SetPackedPedPropData(i, propVarInfoHash, propData);
		}

		return *this;
	}

	void Reset();

	void Serialise(CSyncDataBase& serialiser);
	void SetPropData(const CPed* pPed, u32 index, s32 anchorID, s32 anchorOverrideID, s32 propIdx, s32 propTexIdx);
	void SetPackedPedPropData(u32 index, u32 varInfoHash, u32 data);

	eAnchorPoints GetAnchorID(u32 index) const;
	eAnchorPoints GetAnchorOverrideID(u32 index) const;
	s32 GetModelIdx(u32 index, const CPed* pPed) const;
	s32 GetPropTextureIdx(u32 index) const;
	void GetPackedPedPropData(u32 index, u32& varInfoHash, u32& data) const;

private:

	inline u32 Extract(u32 data, u32 sizeOfBits, u32 maskShift) const
	{
		u32 mask = ((1 << sizeOfBits) - 1) << maskShift;

		return (data & mask) >> maskShift;
	}

	u32 m_propVarInfoHashes[MAX_PROPS_PER_PED];
	u32 m_packedPedProps[MAX_PROPS_PER_PED];
};

class CPedPropsMgr
{
public:

	CPedPropsMgr(void);
	~CPedPropsMgr(void);

	static	bool	Init(void);
	static	void	InitSystem(void);
	static	void	Shutdown(void);
	static	void	ShutdownSystem(void);

	static void InitDebugData();

	// EJ: Memory Optimization
	// EJ: Pass in additional parameters so that our ped prop draw command objects don't have unnecessary data
	static u32 GetPropBoneCount(CPedVariationInfoCollection* pVarInfo, const CPedPropData& propData);
	static u32 GetPropBoneIdxs(s32 *pBoneIndices, size_t boneCount, CPedVariationInfoCollection* pVarInfo, CPedPropData& propData, s32* propDataIndices);
	static void GetPropMatrices(Matrix34 *pMatrices, const crSkeleton* pSkel, const s32* boneIdx, u32 boneCount, CPedPropData& propData, s32* propDataIndices, bool localMatrix);

	static	void	RenderPropsInternal(CPedModelInfo *pModelInfo, const Matrix34& rootMat, CPedPropRenderGfx* pGfx, const Matrix34 *pMatrices, const u32 matrixCount, CPedPropData& propData, s32* propDataIndices, u32 bucketMask, eRenderMode renderMode, bool bAddToMotionBlurMask, u8 lodIdx, const CCustomShaderEffectPed* pPedShaderEffect, bool bBlockFPSVisibleFlag = false);
	static	void	RenderPropAtPosn(CPedModelInfo *pModelInfo, const Matrix34& pMat, s32 propIdxHash, s32 texIdxHash, u32 bucketMask, eRenderMode renderMode, bool bAddToMotionBlurMask, const CCustomShaderEffectPed* pPedShaderEffect, eAnchorPoints anchorId);

	void ScanForAnchorPoints(crSkeleton *pSkel);
	static void DrawDebugAnchorPoints(CPed *pPed, bool drawAnchors[NUM_ANCHORS]);

	static void GetLocalPropData(const CPed* pPed, eAnchorPoints anchorId, s32 propIndx, u32& outVarInfoHash, s32& outLocalIndex);
	static s32 GetGlobalPropData(const CPed* pPed, eAnchorPoints anchorId, u32 varInfoHash, s32 localIndex);

	static bool IsPedPropDelayedDelete(const CPed* pPed, eAnchorPoints anchorID);
	static s32	GetPedPropIdx(const CPed *pPed, eAnchorPoints anchorID);
	static s32	GetPedPropActualAnchorPoint(CPed* pPed, eAnchorPoints anchorID);
	static s32	GetPedPropTexIdx(const CPed *pPed, eAnchorPoints anchorID);
	static s32	GetPropIdxWithFlags(CPed* pPed, eAnchorPoints anchorId, u32 propFlags);
	static void		SetPedProp(CPed *pPed, eAnchorPoints anchorID, s32 propID, s32 texID = 0, eAnchorPoints anchorOverrideID = ANCHOR_NONE, Vector3 *pPosOffset = NULL, Quaternion *pRotOffset = NULL, u32 streamingFlags = 0);
	static s32		GetPedCompAltForProp(CPed *pPed, eAnchorPoints anchorID, s32 propID, s32 localDrawIdx, u32 uDlcNameHash, u8 comp);
	static CPackedPedProps	GetPedPropsPacked(const CDynamicEntity *pPedOrDummyPed, bool skipDelayedDeleteProps=false);
	static void		SetPedPropsPacked(CDynamicEntity *pPedOrDummyPed, CPackedPedProps packedProps);

	static void		ClearPedProp(CPed *pEntity, eAnchorPoints anchorID, bool bForceClear = false);
	static void		ClearAllPedProps(CPed *pEntity);
	static bool		CheckPropFlags(CPed* ped, eAnchorPoints anchor, u32 flags);
	static bool		DoesPedHavePropOrComponentWithFlagSet(const CPed& ped, u32 pedCompFlags);
	static bool		IsAnyPedPropDelayedDelete(const CPed* pPed);

	static u32      SetPreloadProp(CPed* pPed, eAnchorPoints anchorID, s32 propID, s32 texID, u32 streamingFlags = 0);
	static bool		HasPreloadPropsFinished(CPed* pPed);
	static bool		HasPreloadPropsFinished(CPed* pPed, u32 handle);
    static void		CleanUpPreloadProps(CPed* pPed);
    static void     CleanUpPreloadPropsHandle(CPed* pPed, u32 handle);

	static void		SetPedPropsRandomly(CPed* pEntity, float firstPropFreq, float secondPropFreq, u32 setRestrictionFlags, u32 clearRestrictionFlags, atFixedBitSet32* incs, atFixedBitSet32* excs, const CPedPropRestriction* pRestrictions = NULL, int numRestrictions = 0, u32 wantHelmetType = 0, u32 streamingFlags = 0);

	static void		SetRandomHelmet(CPed* ped, u32 helmetType);
	static void		SetRandomHat(CPed* ped);

	static u8	GetNumActiveProps(const CPed *pEntity);
	static u8	GetMaxNumProps(CDynamicEntity *pEntity, eAnchorPoints anchorPointIdx);
	static u8	GetMaxNumPropsTex(CDynamicEntity *pEntity, eAnchorPoints anchorPointIdx, u32 propIdx);

	// if using alpha props then set draw last flag
	// Complex signature is so it can work with dummy peds as well.
	static void		UpdatePedForAlphaProps(CEntity* pEntity, CPedPropData& pedPropData, const CPedVariationData& pedVarData);

	static bool     IsPropValid(CPed* pPed, eAnchorPoints anchorId, s32 propId, s32 textId);

#if __BANK
	static void PopulatePropVariationInfos();
	static bool			SetupWidgets(bkBank& bank);
	static void			WidgetUpdate(void);
	static Vector3&		GetFirstPersonEyewearOffset(size_t idx);
#endif //__BANK

	static void			TryKnockOffProp(CPed* pPed, eAnchorPoints propSlot);
	static void			KnockOffProps(CPed* pPed, bool bDamaged, bool bHats, bool bGlasses, bool bHelmets = false, const Vector3* impulse = NULL, const Vector3* impulseOffset = NULL, const Vector3* angImpulse = NULL);

	static void			ProcessStreamRequests();
	static void			ProcessSingleStreamRequest(CPedPropRequestGfx* pPropReq);

	static void		UpdatePropExpressions(CPed* pPed);

	static bool			HasProcessedAllRequests();
private:
	static void			ApplyStreamPropFiles(CPed* pPed, CPedPropRequestGfx* pPSGfx);
	
	static void SortPropTextures(CPedVariationInfoCollection* varInfo, const CPed** peds, u32 numPeds, atArray<sPropSortedItem>& offsets, u8* anchorDistrib);
	static void RandomizeTextures(CPedVariationInfoCollection* varInfo, atArray<sPropSortedItem>& offsets, u8* anchorDistrib);

	static bool IsPropRestricted(eAnchorPoints propSlot, int propIndex, const CPedPropRestriction* pRestrictions, int numRestrictions);



	static CBaseModelInfo*	ms_pGenericHat;
	static CBaseModelInfo*	ms_pGenericHat2;
	static CBaseModelInfo*	ms_pGenericGlasses;
};

class CPedPropRenderGfx
{
public:
	s32			m_dwdIdx[MAX_PROPS_PER_PED];
	s32			m_txdIdx[MAX_PROPS_PER_PED];

#if !__NO_OUTPUT
	RegdPed		m_pTargetEntity;
	u32			m_frameCountCreated;
#endif

	u16			m_blendIndex;

	// ref count used to safely use pointer to this class on render thread
	s16			m_refCount;
	bool		m_used;

	CPedPropRenderGfx();
	CPedPropRenderGfx(CPedPropRequestGfx *pReq);
	~CPedPropRenderGfx();

	static void Init();
	static void Shutdown();

	FW_REGISTER_CLASS_POOL(CPedPropRenderGfx); 

	grcTexture* GetTexture(u32 propSlot);
	s32 GetTxdIndex(u32 slot) const;
	rmcDrawable* GetDrawable(u32 propSlot) const;
	const rmcDrawable* GetDrawableConst(u32 propSlot) const;

	void AddRefsToGfx();
	bool ValidateStreamingIndexHasBeenCached();

	void AddRef() { m_refCount++; }
	void SetUsed(bool val);

	static void RemoveRefs();
	static atArray<CPedPropRenderGfx*> ms_renderRefs[FENCE_COUNT];
	static s32 ms_fenceUpdateIdx;

private:
	void ReleaseGfx();
};

#if !__DEV
inline bool CPedPropRenderGfx::ValidateStreamingIndexHasBeenCached() { return true;}
#endif

// -----------

class CPedPropRequestGfx
{
	enum	state{ PS_INIT, PS_REQ, PS_USE, PS_FREED };

public:
	CPedPropRequestGfx();
	~CPedPropRequestGfx();

	FW_REGISTER_CLASS_POOL(CPedPropRequestGfx); 

	void RequestsComplete();
	void Release();
	bool HaveAllLoaded() 
	{ 
		return(	m_reqDwds.HaveAllLoaded() && 
			m_reqTxds.HaveAllLoaded() && 
			(m_state == PS_REQ) && (m_bDelayCompletion==false)
			); 
	}

	void PushRequest() { Assert((m_state == PS_INIT) || (m_state == PS_FREED)); m_state = PS_REQ; }

	void AddTxd(u32 componentSlot, const strStreamingObjectName txdName, s32 streamFlags, CPedModelInfo* pPedModelInfo);
	void AddDwd(u32 componentSlot, const strStreamingObjectName dwdName, s32 streamFlags, CPedModelInfo* pPedModelInfo);

	s32 GetTxdSlot(u32 propSlot) { Assert(propSlot < MAX_PROPS_PER_PED); return(m_reqTxds.GetRequestId(propSlot)); }
	s32 GetDwdSlot(u32 propSlot) { Assert(propSlot < MAX_PROPS_PER_PED); return(m_reqDwds.GetRequestId(propSlot)); }

	void SetTargetEntity(CPed *pEntity);
	CPed* GetTargetEntity(void) { return(m_pTargetEntity); }

	void SetDelayCompletion(bool bDelay) { m_bDelayCompletion = bDelay; }

	void SetWaitForGfx(CPedStreamRequestGfx* gfx) { m_waitForGfx = gfx; }
	CPedStreamRequestGfx* GetWaitForGfx() { return m_waitForGfx; }
	void ClearWaitForGfx();
	bool IsSyncGfxLoaded() const;

private:
	strRequestArray<MAX_PROPS_PER_PED>  m_reqDwds;
	strRequestArray<MAX_PROPS_PER_PED>  m_reqTxds;

	CPed*	m_pTargetEntity;

	CPedStreamRequestGfx* m_waitForGfx;

	state	m_state;
	bool	m_bDelayCompletion;
};

/*
PURPOSE
	Information about a specific restriction on the ped prop selection process,
	either specifying that a certain prop may not be used, or that it must be used.
	The user can pass in an array of these when generating random props for a ped.
*/
class CPedPropRestriction
{
public:
	// PURPOSE:	Specifies which type of restriction this is.
	enum Restriction
	{
		CantUse,	// Can only use this prop drawable for this prop slot.
		MustUse		// May not use this prop drawable for this prop slot.
	};

	CPedPropRestriction() {}
	CPedPropRestriction(eAnchorPoints propSlot, int propIndex, Restriction restriction) 
		: m_PropSlot(propSlot)
		, m_PropIndex(propIndex)
		, m_Restriction(restriction)
	{	}

	// PURPOSE:	Check if a specific prop index can be used in a prop slot.
	bool IsRestricted(eAnchorPoints propSlot, int propIndex) const
	{
		if(m_PropSlot != propSlot)
		{
			// The restriction doesn't apply to this slot.
			return false;
		}
		if(m_Restriction == CantUse)
		{
			// If the drawable index matches, we can't use this.
			if(propIndex == m_PropIndex)
			{
				return true;
			}
		}
		else
		{
			Assert(m_Restriction == MustUse);

			// If the drawable index doesn't match, we can't use this.
			if(propIndex != m_PropIndex)
			{
				return true;
			}
		}
		// No restriction.
		return false;
	}

	Restriction GetRestriction() const { return m_Restriction; }
	eAnchorPoints GetPropSlot() const { return m_PropSlot; }
	int GetPropIndex() const { return m_PropIndex; }

	// PURPOSE:	Sets the restriction data when object isn't loaded from metadata
	void SetRestriction(eAnchorPoints anchor, int propIndex, Restriction restriction)
	{
		m_PropSlot = anchor;
		m_PropIndex = propIndex;
		m_Restriction = restriction;
	}

protected:
	// PURPOSE:	The prop slot this restriction applies for.
	eAnchorPoints	m_PropSlot;

	// PURPOSE:	The prop index within the prop slot that this restriction is for.
	int				m_PropIndex;

	// PURPOSE:	The type of restriction this is.
	Restriction		m_Restriction;

	PAR_SIMPLE_PARSABLE;
};

#endif //_PEDPROPS_H_
