// Title	:	PedVariationDS.h
// Author	:	John Whyte
// Started	:	25/04/07

// broken off some data structures so they can be included easily in render thread headers

#ifndef _PEDVARIATIONDS_H_
#define _PEDVARIATIONDS_H_

#include "atl/map.h"
#include "atl/vector.h"
#include "cloth/charactercloth.h"
#include "fwanimation/boneids.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/clothstore.h"
#include "fwpheffects/clothmanager.h"

#include "fwnet/netserialisers.h"
#include "parser/macros.h"
#include "peds/rendering/PedProps.h"
#include "renderer/MeshBlendManager.h"

class CPedStreamRequestGfx;

#define SCRIPT_PRELOAD_MULTIPLIER (6)

// NOTE: if you update this enum please update the string array in the cpp file too, and the metadata in pedvariations.psc
enum ePedCompFlags
{
	PV_FLAG_NONE				= 0,
	PV_FLAG_BULKY				= 1<<0,
	PV_FLAG_JOB					= 1<<1,
	PV_FLAG_SUNNY				= 1<<2,
	PV_FLAG_WET					= 1<<3,
	PV_FLAG_COLD				= 1<<4,
	PV_FLAG_NOT_IN_CAR			= 1<<5,
	PV_FLAG_BIKE_ONLY			= 1<<6,
	PV_FLAG_NOT_INDOORS			= 1<<7,
	PV_FLAG_FIRE_RETARDENT		= 1<<8,
	PV_FLAG_ARMOURED			= 1<<9,
	PV_FLAG_LIGHTLY_ARMOURED	= 1<<10,
	PV_FLAG_HIGH_DETAIL			= 1<<11,
	PV_FLAG_DEFAULT_HELMET		= 1<<12,
	PV_FLAG_RANDOM_HELMET		= 1<<13,
	PV_FLAG_SCRIPT_HELMET		= 1<<14,
	PV_FLAG_FLIGHT_HELMET		= 1<<15,
	PV_FLAG_HIDE_IN_FIRST_PERSON= 1<<16,
	PV_FLAG_USE_PHYSICS_HAT_2	= 1<<17,
	PV_FLAG_PILOT_HELMET		= 1<<18,
	PV_FLAG_WET_MORE_WET		= 1<<19,
	PV_FLAG_WET_LESS_WET		= 1<<20,
};

//Note: If you update this enum, please update the metadata declaration in pedvariations.psc.
enum ePedVarComp
{
	PV_COMP_INVALID = -1,
	PV_COMP_HEAD = 0,
	PV_COMP_BERD,
	PV_COMP_HAIR,
	PV_COMP_UPPR,
	PV_COMP_LOWR,
	PV_COMP_HAND,
	PV_COMP_FEET,
	PV_COMP_TEEF,
	PV_COMP_ACCS,
	PV_COMP_TASK,
	PV_COMP_DECL,
	PV_COMP_JBIB,
	PV_MAX_COMP
};

enum ePVRaceType
{
	PV_RACE_INVALID = -1,
	PV_RACE_UNIVERSAL = 0,
	PV_RACE_WHITE,
	PV_RACE_BLACK,
	PV_RACE_CHINESE,	
	PV_RACE_LATINO,
	PV_RACE_ARAB,
	PV_RACE_BALKAN,
	PV_RACE_JAMAICAN,
	PV_RACE_KOREAN,
	PV_RACE_ITALIAN,
	PV_RACE_PAKISTANI,
	PV_RACE_MAX_TYPES
};

enum eHeadBlendSlots
{
	HBS_HEAD_0 = 0,
	HBS_HEAD_1,
	HBS_HEAD_2,
	HBS_PARENT_HEAD_0,
	HBS_PARENT_HEAD_1,
	HBS_HEAD_DRAWABLES,
	HBS_TEX_0 = HBS_HEAD_DRAWABLES,
	HBS_TEX_1,
	HBS_TEX_2,
	HBS_OVERLAY_TEX_BLEMISHES,
	HBS_OVERLAY_TEX_FACIAL_HAIR,
	HBS_OVERLAY_TEX_EYEBROW,
	HBS_OVERLAY_TEX_AGING,
	HBS_OVERLAY_TEX_MAKEUP,
	HBS_OVERLAY_TEX_BLUSHER,
	HBS_OVERLAY_TEX_DAMAGE,
	HBS_OVERLAY_TEX_BASE_DETAIL,
	HBS_OVERLAY_TEX_SKIN_DETAIL_1,
	HBS_OVERLAY_TEX_SKIN_DETAIL_2,
	HBS_BODY_OVERLAY_1,
	HBS_BODY_OVERLAY_2,
	HBS_BODY_OVERLAY_3,
	HBS_SKIN_FEET_0,
	HBS_SKIN_FEET_1,
	HBS_SKIN_FEET_2,
	HBS_SKIN_UPPR_0,
	HBS_SKIN_UPPR_1,
	HBS_SKIN_UPPR_2,
	HBS_SKIN_LOWR_0,
	HBS_SKIN_LOWR_1,
	HBS_SKIN_LOWR_2,

	HBS_MICRO_MORPH_SLOTS,
	HBS_MICRO_MORPH_NOSE_WIDTH = HBS_MICRO_MORPH_SLOTS,
	HBS_MICRO_MORPH_NOSE_HEIGHT,
	HBS_MICRO_MORPH_NOSE_LENGTH,
	HBS_MICRO_MORPH_NOSE_PROFILE,
	HBS_MICRO_MORPH_NOSE_TIP,
	HBS_MICRO_MORPH_NOSE_BROKE,
	HBS_MICRO_MORPH_BROW_HEIGHT,
	HBS_MICRO_MORPH_BROW_DEPTH,
	HBS_MICRO_MORPH_CHEEK_HEIGHT,
	HBS_MICRO_MORPH_CHEEK_DEPTH,
	HBS_MICRO_MORPH_CHEEK_PUFFED,
	HBS_MICRO_MORPH_EYES_SIZE,
	HBS_MICRO_MORPH_LIPS_SIZE,
	HBS_MICRO_MORPH_JAW_WIDTH,
	HBS_MICRO_MORPH_JAW_ROUND,
	HBS_MICRO_MORPH_CHIN_HEIGHT,
	HBS_MICRO_MORPH_CHIN_DEPTH,
	HBS_MICRO_MORPH_CHIN_POINTED,
	HBS_MICRO_MORPH_CHIN_BUM,
	HBS_MICRO_MORPH_NECK_MALE,
	HBS_MAX
};

enum eHeadOverlaySlots
{
	HOS_BLEMISHES = 0,
	HOS_FACIAL_HAIR,
	HOS_EYEBROW,
	HOS_AGING,
	HOS_MAKEUP,
	HOS_BLUSHER,
	HOS_DAMAGE,
	HOS_BASE_DETAIL,
	HOS_SKIN_DETAIL_1,
	HOS_SKIN_DETAIL_2,
	HOS_BODY_1,
	HOS_BODY_2,
	HOS_BODY_3,
	HOS_MAX
};

// --- defines ----

// maximum number of texture variations permitted for a component
#define MAX_TEX_VARS	(32)
// maximum number of model variations permitted for a component
#if __BANK
#define MAX_DRAWBL_VARS	(32)
#else
#define MAX_DRAWBL_VARS	(16)
#endif

#define MAX_PLAYER_DRAWBL_VARS	(128)
#define MAX_PALETTES	(16)

#define MAX_TEX_PER_COMPONENT	(MAX_TEX_VARS*MAX_DRAWBL_VARS)

// props have idxs which are offset in order to distinguish between them and components
#define PROP_IDX_OFFSET (100)

#define MAX_EXPRESSION_MODS (5)

#define NUM_PACKED_PED_VARS  (3)

#define	CREATE_CLOTH_ONLY_IF_RENDERED		0

CompileTimeAssert(PV_MAX_COMP <= NUM_PACKED_PED_VARS*4);

#define PV_NULL_DRAWBL		(0xFFFFFFFF)

extern const char* varSlotNames[PV_MAX_COMP];
extern ePedVarComp varSlotEnums[PV_MAX_COMP];
extern const char* raceTypeNames[PV_RACE_MAX_TYPES];
extern const char* propSlotNames[NUM_ANCHORS]; 
extern const char* propSlotNamesClean[NUM_ANCHORS]; 
extern eAnchorPoints propSlotEnums[NUM_ANCHORS];

#define NUM_PED_COMP_FLAGS (21)
extern const char* pedCompFlagNames[NUM_PED_COMP_FLAGS];

// -----------

class CPedCompRestriction;
class CPedVariationData;
class CPedDrawHandler;
class CPedBigPrototype;
class CPedModelInfo;
class CDynamicEntity;
class CScriptPreloadData;
class CPedHeadBlendData;

struct sVarSortedItem 
{
	float shortestDistanceSqr;
	u8 texId;
	u8 drawblId;
	u8 count;
	u8 distribution;
	u8 useCount;
	u8 occurrence;
};

class CComponentInfo
{
public:
	atHashString	pedXml_audioID;				// audio meta data
	atHashString	pedXml_audioID2;
	float			pedXml_expressionMods[MAX_EXPRESSION_MODS];	// expression modifiers

	u32				flags;						// bit mask storing the ePedCompFlags
    atFixedBitSet32	inclusions;
    atFixedBitSet32	exclusions;

    atFixedBitSet16 pedXml_vfxComps;            // bit mask storing what other components this specific one should be regarded as. (e.g. an ACCS can act as an UPPR to provide extra flags)

	u16				pedXml_flags;				// OBSOLETE!

	u8				pedXml_compIdx;				// component index for this info element
	u8				pedXml_drawblIdx;			// drawable index for this info element
	PAR_SIMPLE_PARSABLE;
};

class CPedVariationData
{
friend class CPed;
friend class CPedVariationInfo;
friend class CPedVariationPack;
friend class CPedVariationStream;
friend class CPedVariationDebug;
friend class CPedDrawHandler;
friend class CPedBigPrototype;

friend class CutSceneManager;
friend class CCutSceneActor;
friend class CPlayerSettingsObject;
friend class CPlayerSettingsMgr;

friend class CPedPropData;

#if __BANK
friend class CShaderEdit;
#endif

public:
	CPedVariationData(void);
	~CPedVariationData(void);

	float	scaleFactor;

	bool			IsCurrVarAlpha(void) const	{return(m_bIsCurrVarAlpha);	}
	bool			IsCurrVarDecal(void) const  {return(m_bIsCurrVarDecal);}
	bool			IsCurrVarCutout(void) const {return(m_bIsCurrVarCutout);}
	bool			IsCurrVarDecalDecoration(void) const {return(m_bIsCurrVarDecalDecoration);}
	bool			IsShaderSetup(void)	const	{return(m_bIsShaderSetup);}

	bool			GetIsHighLOD() const		{ return (m_LodFadeValue > 129); }
	bool			GetIsSuperLOD() const		{ return (m_LodFadeValue == 0); }
	bool			GetIsPropHighLOD() const	{ return m_bIsPropHighLOD; }
	void			SetPropLODState(bool prophigh) {m_bIsPropHighLOD = prophigh;}

	u32				GetLODFadeValue(void) const	{ return((u32)m_LodFadeValue); }
	void			SetLODFadeValue(u32 LODVal)	{ m_LodFadeValue = (u8)LODVal;}

	void			RemoveAllCloth(bool restoreValue = false);
	void			RemoveCloth(ePedVarComp slotId, bool restoreValue = false);

	bool			HasRacialTexture(CPed* ped, ePedVarComp slotId);

    void            UpdateAllHeadBlends(CPed* ped);
	void			UpdateHeadBlend(CPed* ped);
	bool			AssetLoadFinished(CPed* ped, CPedStreamRequestGfx* pStreamReq);
	void			SetHeadBlendData(CPed* ped, const CPedHeadBlendData& data, bool bForce = false);
	void			SetHeadOverlay(CPed* ped, eHeadOverlaySlots slot, u8 tex, float blend, float normBlend);
	void			UpdateHeadOverlayBlend(CPed* ped, eHeadOverlaySlots slot, float blend, float normBlend);
	void			SetHeadOverlayTintIndex(CPed* ped, eHeadOverlaySlots slot, eRampType rampType, u8 tint, u8 tint2);
	void			SetNewHeadBlendValues(CPed* ped, float headBlend, float texBlend, float varBlend);
	bool			HasHeadBlendData(CPed* ped) const;
	bool			HasHeadBlendFinished(CPed* ped) const;
	void			FinalizeHeadBlend(CPed* ped) const;
	bool			SetHeadBlendPaletteColor(CPed* ped, u8 r, u8 g, u8 b, u8 colorIndex, bool bforce = false);
	void			GetHeadBlendPaletteColor(CPed* ped, u8& r, u8& g, u8& b, u8 colorIndex);
	void			SetHeadBlendPaletteColorDisabled(CPed* ped);
	bool			SetHeadBlendRefreshPaletteColor(CPed* ped);
    void            SetHeadBlendEyeColor(CPed* ped, s32 colorIndex);
    s32             GetHeadBlendEyeColor(CPed* ped);
	void			MicroMorph(CPed* ped, eMicroMorphType type, float blend);
	void			SetHairTintIndex(CPed* ped, u32 index, u32 index2);
	bool			GetHairTintIndex(CPed* ped, u32& outIndex, u32& outIndex2);

    bool            HasFirstVariationBeenSet() const { return m_bVariationSet; }

	void			SetHasComponentHiLOD(u8 componentIdx, bool value) { m_componentHiLod.Set(componentIdx, value);  }
	bool			HasComponentHiLOD(u8 componentIdx) { return m_componentHiLod.IsSet(componentIdx); }
	void			ResetHasComponentHiLOD() { m_componentHiLod.Reset(); } 

	static ePedVarComp GetComponentSlotFromBoneTag(eAnimBoneTag tag);

	clothVariationData* GetClothData(ePedVarComp slotId) { return m_clothVarData[slotId]; }
	void SetClothData(ePedVarComp slotId, clothVariationData* clothVData )
	{
		Assert( !m_clothVarData[slotId] );
		m_clothVarData[slotId] = clothVData;
	}

// TODO: this should be used only for DEBUG/TEST purposes
	void ResetClothData(ePedVarComp slotId )
	{
		Assert( m_clothVarData[slotId] );
		m_clothVarData[slotId] = NULL;
	}


#if CREATE_CLOTH_ONLY_IF_RENDERED
	const bool ShouldCreateCloth(ePedVarComp slotId) const { return m_aCreateCloth[slotId]; }
	void SetFlagClothCreated(ePedVarComp slotId) { Assert( m_aCreateCloth[slotId] ); m_aCreateCloth[slotId] = false; }
	void SetFlagCreateCloth(ePedVarComp slotId) { Assert( !m_aCreateCloth[slotId] ); m_aCreateCloth[slotId] = true; }
	void SetPedSkeleton(const crSkeleton* skel)	{ m_pedSkeleton = skel;	}
	const crSkeleton* GetPedSkeleton() const { return m_pedSkeleton; }
#endif

	u8			GetPedTexIdx(ePedVarComp slotId) const {return(m_aTexId[slotId]);}
	u32			GetPedCompIdx(ePedVarComp slotId) const {return(m_aDrawblId[slotId]);}
	u8			GetPedCompAltIdx(ePedVarComp slotId) const {return(m_aDrawblAltId[slotId]);}
	u8			GetPedPaletteIdx(ePedVarComp slotId) const {return(m_aPaletteId[slotId]);}

	u32         SetPreloadData(CPed* ped, ePedVarComp slotId, u32 drawableId, u32 textureId, u32 streamingFlags);
	bool		HasPreloadDataFinished();
	bool		HasPreloadDataFinished(u32 handle);
	void		CleanUpPreloadData();
	void		CleanUpPreloadData(u32 handle);

#if !__NO_OUTPUT
	static const char* GetVarOrPropSlotName(int component);
#endif //!__NO_OUTPUT

	void		SetOverrideCrewLogoTxdHash(const char *txdName)	{ m_overrideCrewLogoTxd.SetFromString(txdName);	}
	void		SetOverrideCrewLogoTxdHash(u32 hash)	        { m_overrideCrewLogoTxd.SetHash(hash);	        }
	u32			GetOverrideCrewLogoTxdHash() const				{ return m_overrideCrewLogoTxd.GetHash();		}
	void		ClearOverrideCrewLogoTxdHash()					{ m_overrideCrewLogoTxd.Clear();				}

	void		SetOverrideCrewLogoTexHash(const char *texName)	{ m_overrideCrewLogoTex.SetFromString(texName);	}
	void		SetOverrideCrewLogoTexHash(u32 hash)	        { m_overrideCrewLogoTex.SetHash(hash);	        }
	u32			GetOverrideCrewLogoTexHash() const				{ return m_overrideCrewLogoTex.GetHash();		}
	void		ClearOverrideCrewLogoTexHash()					{ m_overrideCrewLogoTex.Clear();				}

protected:
	
	u32*		GetPedCompHashPtr(ePedVarComp slotId) {return &m_aDrawblHash[slotId];}

	bool		IsThisSettingSameAs(ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId, void* clothPtr, s32 dictionarySlotId);
	void		SetPedVariation(ePedVarComp slotId, u32 drawblId, u32 texId, u32 paletteId, u32 drawblAltId, clothVariationData* clothVData = NULL, bool requireShaderSetup = true);

	void		SetIsShaderSetup(bool bIsShaderSetup) { m_bIsShaderSetup = bIsShaderSetup; }

private:
	u32	m_aDrawblHash[PV_MAX_COMP];		// store hash of current drawables for each component

	clothVariationData* m_clothVarData[PV_MAX_COMP];

	CScriptPreloadData* m_preloadData;

#if CREATE_CLOTH_ONLY_IF_RENDERED
	const crSkeleton*	m_pedSkeleton;			// skeleton is needed for character cloth
	bool m_aCreateCloth[PV_MAX_COMP];
#endif

	strStreamingObjectName			m_overrideCrewLogoTxd;
	strStreamingObjectName			m_overrideCrewLogoTex;

	u32	m_aDrawblId[PV_MAX_COMP];		// current drawable for each component
	u8	m_aDrawblAltId[PV_MAX_COMP];	// current alternative of each drawable
	u8	m_aTexId[PV_MAX_COMP];			// current texture (applied to drawable) for each component

	u8	m_aPaletteId[PV_MAX_COMP];		 // current palette to use for each component
	atFixedBitSet16 m_componentHiLod;	// There are 4 extra bits available in this object

	u8	m_LodFadeValue;					// current LOD fade value

	u8	m_bIsCurrVarAlpha			: 1;		// flag true if the current variation contains alpha
	u8	m_bIsCurrVarDecal			: 1;		// flag true if the current variation contains decal
	u8	m_bIsCurrVarCutout			: 1;		// flag true if the current variation contains cutout
	u8	m_bIsCurrVarDecalDecoration	: 1;		// flag true if the current variation contains decal decoration
	u8 	m_bIsShaderSetup			: 1;		// true if shader setup was called on these values (ready to render)
	u8	m_bIsPropHighLOD			: 1;
    u8  m_bVariationSet     		: 1;        // keeps track of when the first variation has been set
	u8	m_padpad					: 1;

};

// ped variation data synced over the network
class CSyncedPedVarData
{
public:
	CSyncedPedVarData()
	{
		Reset();
	}

	void Reset();
	void ExtractFromPed(CPed& ped);
	void ApplyToPed(CPed& ped, bool forceApply = false) const;

	void Serialise(CSyncDataBase& serialiser);
	void Log(netLoggingInterface& log);

	u32		m_usedComponents;
	bool	m_playerData;
	u32		m_varInfoHash[PV_MAX_COMP];
	u32		m_componentData[PV_MAX_COMP];
	u32		m_textureData[PV_MAX_COMP];
	u32		m_paletteData[PV_MAX_COMP];
	u32     m_crewLogoTxdHash;
	u32     m_crewLogoTexHash;
};

class CPedHeadBlendData : public fwExtension
{
public:
	EXTENSIONID_DECL(CPedHeadBlendData, fwExtension);

    struct sParent
    {
        sParent() : parentBlend(0.f), head0Parent0(0xff), head0Parent1(0xff) {}

        float parentBlend;
        u8 head0Parent0;
        u8 head0Parent1;
    };

	CPedHeadBlendData();
	CPedHeadBlendData(const CPedHeadBlendData& other);
	~CPedHeadBlendData();

	CPedHeadBlendData& operator=(const CPedHeadBlendData& other);

	void Reset();

	bool HasDuplicateHeadData(const CPedHeadBlendData& other) const;
    bool HasParents() const { return m_hasParents; }
    void UpdateFromParents();

    sParent m_extraParentData;

	MeshBlendHandle m_blendHandle;

    RegdPed m_parentPed0;
    RegdPed m_parentPed1;

	float m_headBlend;					// head geometry blend
	float m_texBlend;					// head texture blend
	float m_varBlend;					// third head blend, used for "genetic variation"
	float m_parentBlend2;				// second grandparent blend
	float m_overlayAlpha[HOS_MAX];
	float m_overlayNormAlpha[HOS_MAX];
	float m_microMorphBlends[MMT_MAX];

	u8 m_overlayTintIndex[HOS_MAX];
	u8 m_overlayTintIndex2[HOS_MAX];
	u8 m_overlayRampType[HOS_MAX];

	u8 m_head0;						    // head blend data
	u8 m_head1;
	u8 m_head2;
	u8 m_tex0;
	u8 m_tex1;
	u8 m_tex2;
	u8 m_overlayTex[HOS_MAX];
	u8 m_paletteRed[MAX_PALETTE_COLORS];
	u8 m_paletteGreen[MAX_PALETTE_COLORS];
	u8 m_paletteBlue[MAX_PALETTE_COLORS];
    s16 m_eyeColor;
	u8 m_hairTintIndex;
	u8 m_hairTintIndex2;
	bool m_async;
	bool m_usePaletteRgb;
    bool m_hasParents;
	bool m_isParent;
    bool m_needsFinalizing;

	void Serialise(CSyncDataBase& serialiser);
};

class CPVTextureData
{
public:
	CPVTextureData() : texId(PV_RACE_UNIVERSAL), distribution(255), useCount(0) {}
	CPVTextureData(u8 texId, u8 distribution, u8 useCount) : texId(texId), distribution(distribution), useCount(useCount) {}

	void PostLoad() { useCount = 0; }

	u8 texId;
	u8 distribution;
	u8 useCount;

	PAR_SIMPLE_PARSABLE;
};

// --- new stuff (shared between files) - data about the variations per instance
class CPVDrawblData
{
public:
	struct CPVClothComponentData
	{
		bool					m_ownsCloth;	    // cloth pieces available for this component and each variation, 
		u8						m_Padding;		
		pgRef<characterCloth> 	m_charCloth;		// character cloth loaded for this component
		s32						m_dictSlotId;		// slot id in the dictionary, used for reference counting
		
		void Reset()
		{
			m_charCloth		= NULL;	
			m_dictSlotId	= -1;
			m_ownsCloth		= false;
		}		

		void PostLoad() 
		{
			m_charCloth		= NULL;	
			m_dictSlotId	= -1;
		}
		static void PostPsoPlace(void* inst) 
		{
			CPVClothComponentData* ccdInst = reinterpret_cast<CPVClothComponentData*>(inst);
			ccdInst->PostLoad();
		}

		bool HasCloth() const { return m_ownsCloth; }

		PAR_SIMPLE_PARSABLE;
	};

			CPVDrawblData(void);
			~CPVDrawblData(void);

	bool	HasActiveDrawbl() const		{ return (m_propMask & PV_DRAWBL_ACTIVE) != 0; }
	bool	HasAlpha() const			{ return (m_propMask & PV_DRAWBL_ALPHA) != 0; }
	bool	HasDecal() const			{ return (m_propMask & PV_DRAWBL_DECAL) != 0; }
	bool	HasCutout() const			{ return (m_propMask & PV_DRAWBL_CUTOUT) != 0; }
	bool	IsUsingRaceTex() const		{ return (m_propMask & PV_DRAWBL_RACE_TEX) != 0; }
	bool	IsMatchingPrev() const		{ return (m_propMask & PV_DRAWBL_MATCH_PREV) != 0; }
	bool	HasPalette() const			{ return (m_propMask & PV_DRAWBL_PALETTE) != 0; }
	bool	IsProxyTex() const			{ return (m_propMask & PV_DRAWBL_PROXY_TEX) != 0; }

	void	SetHasActiveDrawbl(bool val){ SetPropertyFlag(val, PV_DRAWBL_ACTIVE); }
	void	SetHasAlpha(bool val)		{ SetPropertyFlag(val, PV_DRAWBL_ALPHA); }
	void	SetHasDecal(bool val)		{ SetPropertyFlag(val, PV_DRAWBL_DECAL); }
	void	SetHasCutout(bool val)		{ SetPropertyFlag(val, PV_DRAWBL_CUTOUT); }
	void	SetIsUsingRaceTex(bool val)	{ SetPropertyFlag(val, PV_DRAWBL_RACE_TEX); }
	void	SetIsMatchingPrev(bool val)	{ SetPropertyFlag(val, PV_DRAWBL_MATCH_PREV); }
	void	SetHasPalette(bool val)		{ SetPropertyFlag(val, PV_DRAWBL_PALETTE); }
	void	SetIsProxyTex(bool val)		{ SetPropertyFlag(val, PV_DRAWBL_PROXY_TEX); }

	u8		GetNumAlternatives() const	{ return m_numAlternatives; }
	u32		GetNumTex() const			{ return m_aTexData.GetCount(); }

	void	SetTexData(u32 index, u8 val)		{ m_aTexData[index].texId = val; }
	u8		GetTexData(u32 index) const			{ return m_aTexData[index].texId; }
	u8		GetTexDistrib(u32 index) const		{ return m_aTexData[index].distribution; }
	u8		GetUseCount(u32 index) const		{ return m_aTexData[index].useCount; }

	void	IncrementUseCount(u32 index);

private:
	u8 m_propMask;							// bit mask containing the above properties for this drawable
	u8 m_numAlternatives;					// number of alts for this drawable			

public:
	atArray<CPVTextureData> m_aTexData;		// store data about all the textures
	CPVClothComponentData	m_clothData;

private:
	enum ePVDrawblProperty
	{
		PV_DRAWBL_ACTIVE		= 1 << 0,	// sometimes the component won't have an active drawable
		PV_DRAWBL_ALPHA			= 1 << 1,	// has some alpha'ed bits in it (so needs to draw last)
		PV_DRAWBL_DECAL			= 1 << 2,	// uses decal pass (usually ped_decal stuff)
		PV_DRAWBL_CUTOUT		= 1 << 3,	// uses cutout pass (usually hair)
		PV_DRAWBL_RACE_TEX		= 1 << 4,	// name of the drawable states if race textures are used on it
		PV_DRAWBL_MATCH_PREV	= 1 << 5,	// this component must select the same texture id as used in the previous component for this ped
		PV_DRAWBL_PALETTE		= 1 << 6,	// this drawable can use the colourising palette to recolour masked areas in texture
		PV_DRAWBL_PROXY_TEX		= 1 << 7,	// this drawable should remap its textures to the set on variation 000
	};

	void	SetPropertyFlag(bool val, ePVDrawblProperty prop);	

	PAR_SIMPLE_PARSABLE;
};

class CPVComponentData
{
public:
	u8						m_numAvailTex;			// num of tex for this component (sum of drawables tex for this component)

	atArray<CPVDrawblData>	m_aDrawblData3;			// stores data about all the drawables for this component

                            CPVComponentData() : m_numAvailTex(0) {}
                            ~CPVComponentData() {}

	CPVDrawblData*			ObtainDrawblMapEntry(u32 i);

	CPVDrawblData*			GetDrawbl(u32 i)			{ return i < m_aDrawblData3.GetCount() ? &m_aDrawblData3[i] : NULL; }
	const CPVDrawblData*	GetDrawbl(u32 i) const		{ return i < m_aDrawblData3.GetCount() ? &m_aDrawblData3[i] : NULL; }

	u32						GetNumDrawbls() const		{ return m_aDrawblData3.GetCount(); }

private:

	PAR_SIMPLE_PARSABLE;
};

// defines a complete outfit
class CPedSelectionSet
{
public:
	atHashString m_name;					// set name
	u8 m_compDrawableId[PV_MAX_COMP];		// drawable index for each component
	u8 m_compTexId[PV_MAX_COMP];			// texture index for each component

	u8 m_propAnchorId[MAX_PROPS_PER_PED];	// anchor id for each prop
	u8 m_propDrawableId[MAX_PROPS_PER_PED];	// drawable id for each prop
	u8 m_propTexId[MAX_PROPS_PER_PED];		// anchor id for each prop

	PAR_SIMPLE_PARSABLE;
};

// a ped which has variation textures will have one of these classes to determine
// what it's current variation is
class CPedVariationInfo
{
	friend class CPedVariationPack;
	friend class CPedVariationStream;
	friend class CPedVariationDebug;
	friend class CPedVariationInfoCollection;

public:
	CPedVariationInfo(void);
	~CPedVariationInfo(void);

#if !__FINAL
	void UpdateAndSave(const char* modelName);
#endif

	ePVRaceType		GetRaceType(u32 compId, u32 drawblId, u32 texId, CPedModelInfo* pModelInfo);
	bool			IsMatchComponent(u32 compId, u32 drawblId, CPedModelInfo* pModelInfo);

	u8	GetMaxNumTex(ePedVarComp compType, u32 drawblIdx) const;
	u8	GetMaxNumDrawbls(ePedVarComp compType) const;
	u8	GetMaxNumDrawblAlts(ePedVarComp compType, u32 drawblIdx) const;

	bool	HasRaceTextures(u32 compId, u32 drawblId);
	bool	HasPaletteTextures(u32 compId, u32 drawblId);
	bool	IsValidDrawbl(u32 compId, u32 drawblId);


	CPVComponentData*	GetCompData(u32 compId) { return const_cast<CPVComponentData*>(GetAvailCompData(compId)); }
	const CPVComponentData*	GetCompData(u32 compId) const { return GetAvailCompData(compId); }

	atHashString LookupCompInfoAudioID(s32 slotId, s32 drawblId, bool second) const;
	u32	LookupCompInfoFlags(s32 slotId, s32 drawblId) const;
	u32	LookupSecondaryFlags(s32 slotId, s32 drawblId, s32 targetSlot) const;

	CPedPropInfo* GetPropInfo() { return &m_propInfo; }
	const CPedPropInfo* GetPropInfo() const { return &m_propInfo; }

	const float* GetExpressionMods(ePedVarComp comp, u32 drawblId) const;

	const CPedSelectionSet* GetSelectionSetByHash(u32 hash) const;
	u32 GetSelectionSetCount() const { return m_aSelectionSets.GetCount(); }
	const CPedSelectionSet* GetSelectionSet(u32 index) const;

	bool HasComponentFlagsSet(ePedVarComp compId, u32 drawableId, u32 flags);

	static bool IsRaceCompatible(ePVRaceType typeA, ePVRaceType typeB);

	u8 GetTextureDistribution(ePedVarComp compId, u32 drawableId, u32 texture) const;
	u8 GetTextureUseCount(ePedVarComp compId, u32 drawableId, u32 texture) const;

	u32 GetDlcNameHash() const;
	const char* GetDlcName() const;

	void RemoveFromStore();

private:
	static bool VariationTextureScanCB(grcTexture& pTex, u32 hash, void* pData);

	const CPVComponentData*	GetAvailCompData(u32 compId) const;

	const atFixedBitSet32* GetInclusions(u32 compId, u32 drawblId) const;
	const atFixedBitSet32* GetExclusions(u32 compId, u32 drawblId) const;

	static bool IsComponentRestricted(ePedVarComp compId, int drawableId, const CPedCompRestriction* pRestrictions, int numRestrictions);

	// FA: dummy fields until tools fixes the metadata exporter
	bool m_bHasTexVariations;
	bool m_bHasDrawblVariations;
	bool m_bHasLowLODs;
	bool m_bIsSuperLOD;

	u8		m_availComp[PV_MAX_COMP];				// stores which components are allocated
	atArray<CPVComponentData> m_aComponentData3;	// store data about each component
	atArray<CPedSelectionSet> m_aSelectionSets;
	atArray<CComponentInfo> m_compInfos;
	CPedPropInfo m_propInfo;

	atFinalHashString m_dlcName;
	u8		m_seqHeadTexIdx;			// for 'random' variations we generate heads in sequence & randomize the rest
	u8		m_seqHeadDrawblIdx;

	PAR_SIMPLE_PARSABLE;
};

// This class holds all CPedVariationInfos for a ped. Each ped has at least one info and additional DLC ones.
// The first entry in m_infos will be the base variation for that ped, with additional entries representing DLC.
// When calculating total number of assets the sum of all infos will be used.
// When indexing for a specific drawable or texture the index will apply across all infos as if the data was in one big array.

class CPedVariationInfoCollection
{
    friend class CPedModelInfo;

public:
	CPedVariationInfoCollection() :
	  m_bHasLowLODs(false),
	  m_bIsStreamed(false),
	  m_bIsSuperLOD(false)
	  {
		  for (u32 i = 0; i < NUM_ANCHORS; ++i)
			  m_aAnchorBoneIdxs[i] = PED_PROP_NONE;
	  }

	CPedVariationInfo* GetBaseInfo()	{ return m_infos.GetCount() > 0 ? m_infos[0] : NULL; }

	void SetVarRandom(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData, u32 setRestrictionFlags, u32 clearRestrictionFlags, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, const CPedCompRestriction* pRestrictions = NULL, int numRestrictions = 0, ePVRaceType race = PV_RACE_UNIVERSAL, u32 streamingFlags = 0);
	void SortTextures(CDynamicEntity* pDynamicEntity, ePedVarComp comp, const CPed** peds, u32 numPeds, atArray<sVarSortedItem>& offsets);
	void RandomizeTextures(ePedVarComp comp, atArray<sVarSortedItem>& offsets, CPed* ped);

	bool HasLowLODs() const				{ return m_bHasLowLODs; }
	bool GetIsStreamed() const			{ return m_bIsStreamed; }
	bool IsSuperLod() const				{ return m_bIsSuperLOD; }

	class CCustomShaderEffectBaseType* GetShaderEffectType(Dwd* pDwd, CPedModelInfo* pModelInfo);

	void EnumCloth(CPedModelInfo* pModelInfo);
	void EnumTargetFolder(const char* pFolderName, CPedModelInfo* pModelInfo);
	void EnumClothTargetFolder(const char* pFolderName);

	s32 GetExistingPeds( CDynamicEntity* pDynamicEntity, s32& numSameTypePeds, const CPed** sameTypePeds ) const;

	u32 GetGlobalDrawableIndex(u32 localIndex, ePedVarComp comp, u32 dlcNameHash) const
	{
		u32 globalIndex = localIndex;

		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (m_infos[i]->GetDlcNameHash() != dlcNameHash)
				globalIndex += m_infos[i]->GetMaxNumDrawbls(comp);
			else
				break;
		}

		return globalIndex;
	}

	s32 GetGlobalPropIndex(s32 localIndex, eAnchorPoints anchor, u32 dlcNameHash) const
	{
		s32 globalIndex = localIndex;

		// Don't globalise any negatives...
		if (globalIndex >= 0)
		{
			for (int i = 0; i < m_infos.GetCount(); i++)
			{
				if (m_infos[i]->GetDlcNameHash() != dlcNameHash)
					globalIndex += m_infos[i]->GetPropInfo()->GetMaxNumProps(anchor);
				else
					break;
			}
		}

		return globalIndex;
	}

	u32 GetMaxNumDrawbls(ePedVarComp compType) const
	{
		u32 numDrawbls = 0;

		for (int i = 0; i < m_infos.GetCount(); i++)
			numDrawbls += m_infos[i]->GetMaxNumDrawbls(compType);

		return numDrawbls;
	}

	u32 GetMaxNumTex(ePedVarComp compType, u32 drawblIdx)
	{
		if (CPVDrawblData* drawData = GetCollectionDrawable(compType, drawblIdx))
			return drawData->GetNumTex();
		return 0;
	}
	u32 GetMaxNumDrawblAlts(ePedVarComp compType, u32 drawblIdx)
	{
		if (CPVDrawblData* drawData = GetCollectionDrawable(compType, drawblIdx))
			return drawData->GetNumAlternatives();
		return 0;
	}
	bool HasRaceTextures(u32 compId, u32 drawblIdx)
	{
		if (CPVDrawblData* drawData = GetCollectionDrawable(compId, drawblIdx))
			return drawData->IsUsingRaceTex();
		return false;
	}
	bool HasPaletteTextures(u32 compId, u32 drawblIdx)
	{
		if (CPVDrawblData* drawData = GetCollectionDrawable(compId, drawblIdx))
			return drawData->HasPalette();
		return false;
	}
	bool IsValidDrawbl(u32 compId, u32 drawblIdx)
	{
		if (CPVDrawblData* drawData = GetCollectionDrawable(compId, drawblIdx))
			return drawData->HasActiveDrawbl();
		return false;
	}

	ePVRaceType	GetRaceType(u32 compId, u32 drawblId, u32 texId, CPedModelInfo* pModelInfo)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->GetRaceType(compId, drawblId, texId, pModelInfo);
		}
		return PV_RACE_UNIVERSAL;
	}
	bool IsMatchComponent(u32 compId, u32 drawblId, CPedModelInfo* pModelInfo)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->IsMatchComponent(compId, drawblId, pModelInfo);
		}
		return false;
	}
	bool HasComponentFlagsSet(ePedVarComp compId, u32 drawblId, u32 flags)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->HasComponentFlagsSet(compId, drawblId, flags);
		}
		return false;
	}
	atHashString LookupCompInfoAudioID(s32 compId, s32 drawblId, bool second)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->LookupCompInfoAudioID(compId, drawblId, second);
		}
		return atHashString::Null();
	}
	u32	LookupCompInfoFlags(s32 compId, s32 drawblId)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->LookupCompInfoFlags(compId, drawblId);
		}
		return 0;
	}

	u32	LookupSecondaryFlags(const CPed* ped, s32 slotId);

	const atFixedBitSet32* GetInclusions(u32 compId, u32 drawblId)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->GetInclusions(compId, drawblId);
		}
		return NULL;
	}
	const atFixedBitSet32* GetExclusions(u32 compId, u32 drawblId)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->GetExclusions(compId, drawblId);
		}
		return NULL;
	}
	u8 GetTextureDistribution(ePedVarComp compId, u32 drawblId, u32 texture)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->GetTextureDistribution(compId, drawblId, texture);
		}
		return 255;
	}
	u8 GetTextureUseCount(ePedVarComp compId, u32 drawblId, u32 texture)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->GetTextureUseCount(compId, drawblId, texture);
		}
		return 255;
	}

	const float* GetCompExpressionsMods(u32 compId, u32 drawblId)
	{
		CPedVariationInfo* info = GetCollectionInfo(compId, drawblId);
		if (info)
		{
			drawblId = GetDlcDrawableIdx((ePedVarComp)compId, drawblId);
			return info->GetExpressionMods((ePedVarComp)compId, drawblId);
		}
		return NULL;
	}

	// try to find a selection set with the given hash in any of our variation infos
	// iterate from the back so DLC has a chance to override an old set
	const CPedSelectionSet* GetSelectionSetByHash(u32 hash) const
	{
		for (s32 i = m_infos.GetCount() - 1; i >= 0; --i)
		{
			if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
			{
				const CPedSelectionSet* set = m_infos[i]->GetSelectionSetByHash(hash);
				if (set)
					return set;
			}
		}
		return NULL;
	}

	u32 GetSelectionSetCount() const
	{
		u32 count = 0;
		for (s32 i = m_infos.GetCount() - 1; i >= 0; --i)
		{
			if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
			{
				count += m_infos[i]->GetSelectionSetCount();
			}
		}
		return count;
	}

	const CPedSelectionSet* GetSelectionSet(u32 index) const
	{
		for (s32 i = 0; i < m_infos.GetCount(); i++)
		{
			if (index < m_infos[i]->GetSelectionSetCount())
				return m_infos[i]->GetSelectionSet(index);
			else
				index -= m_infos[i]->GetSelectionSetCount();
		}
		return NULL;
	}

	// returns total number of textures across all drawables for the given component
	u32 GetNumAvailTex(u32 compId)
	{
		u32 numTex = 0;
		for (s32 i = 0; i < m_infos.GetCount(); ++i)
			if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
				if (m_infos[i]->GetCompData(compId))
					numTex += m_infos[i]->GetCompData(compId)->m_numAvailTex;
		return numTex;
	}

	// returns a drawable by simulating a merge of all infos into one big array and indexing that one
	CPVDrawblData* GetCollectionDrawable(u32 compId, u32 drawblIdx)
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (drawblIdx < m_infos[i]->GetMaxNumDrawbls((ePedVarComp)compId))
				return m_infos[i]->GetCompData(compId)->GetDrawbl(drawblIdx);
			else
				drawblIdx -= m_infos[i]->GetMaxNumDrawbls((ePedVarComp)compId);
		}

		return NULL;
	}

	CPedVariationInfo* GetVariationInfoFromPropIdx(eAnchorPoints anchorPoint, u32 propIdx)
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (propIdx < m_infos[i]->GetPropInfo()->GetMaxNumProps(anchorPoint))
				return m_infos[i];
			else
				propIdx -= m_infos[i]->GetPropInfo()->GetMaxNumProps(anchorPoint);
		}

		return NULL;
	}

	const CPedVariationInfo* GetVariationInfoFromPropIdx(eAnchorPoints anchorPoint, u32 propIdx) const
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (propIdx < m_infos[i]->GetPropInfo()->GetMaxNumProps(anchorPoint))
				return m_infos[i];
			else
				propIdx -= m_infos[i]->GetPropInfo()->GetMaxNumProps(anchorPoint);
		}

		return NULL;
	}

	CPedVariationInfo* GetVariationInfoFromDLCName(u32 dlcNameHash)
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (m_infos[i]->GetDlcNameHash() == dlcNameHash)
				return m_infos[i];
		}

		return NULL;
	}

	CPedVariationInfo* GetVariationInfoFromDrawableIdx(ePedVarComp component, u32 drawableIdx)
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (drawableIdx < m_infos[i]->GetMaxNumDrawbls(component))
			{
				return m_infos[i];
			}
			else
				drawableIdx -= m_infos[i]->GetMaxNumDrawbls((ePedVarComp)component);
		}

		return NULL;
	}

	const CPedVariationInfo* GetVariationInfoFromDrawableIdx(ePedVarComp component, u32 drawableIdx) const
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (drawableIdx < m_infos[i]->GetMaxNumDrawbls(component))
				return m_infos[i];
			else
				drawableIdx -= m_infos[i]->GetMaxNumDrawbls((ePedVarComp)component);
		}

		return NULL;
	}

	s32 GetDlcDrawableIdx(ePedVarComp component, u32 drawableIdx)
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (drawableIdx < m_infos[i]->GetMaxNumDrawbls(component))
				return drawableIdx;
			else
				drawableIdx -= m_infos[i]->GetMaxNumDrawbls(component);
		}

		return 0;
	}

	s32 GetDlcPropIdx(eAnchorPoints anchorPoint, u32 propIdx) const
	{
		for (int i = 0; i < m_infos.GetCount(); i++)
		{
			if (propIdx < m_infos[i]->GetPropInfo()->GetMaxNumProps(anchorPoint))
				return propIdx;
			else
				propIdx -= m_infos[i]->GetPropInfo()->GetMaxNumProps(anchorPoint);
		}

		return 0;
	}

	const char* GetDlcNameFromDrawableIdx(ePedVarComp component, u32 drawableIdx)
	{
		if (CPedVariationInfo* pVarInfo = GetVariationInfoFromDrawableIdx(component, drawableIdx))
			return pVarInfo->GetDlcName();

		return NULL;
	}

	u32 GetDlcNameHashFromDrawableIdx(ePedVarComp component, u32 drawableIdx)
	{
		if (CPedVariationInfo* pVarInfo = GetVariationInfoFromDrawableIdx(component, drawableIdx))
			return pVarInfo->GetDlcNameHash();

		return 0;
	}

	const char* GetDlcNameFromPropIdx(eAnchorPoints anchor, u32 propIdx)
	{
		if (CPedVariationInfo* pVarInfo = GetVariationInfoFromPropIdx(anchor, propIdx))
			return pVarInfo->GetDlcName();

		return NULL;
	}

	u32 GetDlcNameHashFromPropIdx(eAnchorPoints anchor, u32 propIdx)
	{
		if (CPedVariationInfo* pVarInfo = GetVariationInfoFromPropIdx(anchor, propIdx))
			return pVarInfo->GetDlcNameHash();

		return 0;
	}

	// return the info containing the specified drawable index
	CPedVariationInfo* GetCollectionInfo(u32 compId, u32 drawblIdx)
	{
		return GetVariationInfoFromDrawableIdx((ePedVarComp)compId, drawblIdx);
	}

#if __BANK
	u32 GetNumTotalVariations()
	{
		u32 numHeadTex = 0;
		u32 numUpprTex = 0;
		u32 numLowrTex = 0;
		for (s32 i = 0; i < m_infos.GetCount(); ++i)
		{
			if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
			{
				if (m_infos[i]->GetCompData(PV_COMP_HEAD))
					numHeadTex += m_infos[i]->GetCompData(PV_COMP_HEAD)->m_numAvailTex;
				if (m_infos[i]->GetCompData(PV_COMP_UPPR))
					numUpprTex += m_infos[i]->GetCompData(PV_COMP_UPPR)->m_numAvailTex;
				if (m_infos[i]->GetCompData(PV_COMP_LOWR))
					numLowrTex += m_infos[i]->GetCompData(PV_COMP_LOWR)->m_numAvailTex;
			}
		}
		return numHeadTex * numUpprTex * numLowrTex;
	}
#endif // __BANK

	//////////////////////////////////////////////////////////////
	// PROPS
	bool InitAnchorPoints(crSkeletonData *pSkelData);
	s32 GetAnchorBoneIdx(eAnchorPoints anchorPointIdx) const
	{ 
		FastAssert(anchorPointIdx >= 0 && anchorPointIdx < NUM_ANCHORS); 
		return m_aAnchorBoneIdxs[anchorPointIdx]; 
	}

	// these functions generate the model info data
	ASSERT_ONLY(bool ValidateProps(s32 DwdId, u8 varInfoIdx);)
	u32	CountTextures(u32 slotId, u32 propId, s32 DwdId);
	void EnumTargetPropFolder(const char* pFolderName, CPedModelInfo* pModelInfo);

    const CPedPropMetaData* GetPropCollectionMetaData(u32 anchorId, u32 propId) const;

	float GetPropKnockOffProb(u32 anchorId, u32 propId) const;

	const float* GetExpressionMods(u32 anchorId, u32 propId) const;

	s32 GetPropIdxWithFlags(u8 anchorId, u32 propFlags) const;

	u32 GetMaxNumProps(eAnchorPoints anchorPointIdx) const;
	u32 GetMaxNumPropsTex(eAnchorPoints anchorPointIdx, u32 propId);

	u32 GetPropFlags(u8 anchorId, u8 propId) const;

	const CPedPropTexData* GetPropTexData(u8 anchorId, u8 propId, u8 texId, const CPedPropMetaData** propData) const;

	bool HasAnyProps() const;
    bool HasHelmetProp() const;

	bool HasPropAlpha(u8 anchorId, u8 propId) const;
	bool HasPropDecal(u8 anchorId, u8 propId) const;
	bool HasPropCutout(u8 anchorId, u8 propId) const;
	void SetPropFlag(u8 anchorId, u8 propId, ePropRenderFlags flag, bool value); // overwrite metadata entry, we don't trust it

	atHashString GetAudioId(u32 anchorId, u32 propId) const;

	u8 GetPropStickyness(u8 anchorId, u8 propId) const;
	u8 GetPropTexDistribution(u8 anchorId, u8 propId, u8 texId) const;

private:
    atArray<CPedVariationInfo*>& GetInfos() { return m_infos; }

	// PURPOSE: Helper function to retrieve a valid palette id for a component
	u32 GetPaletteId(u32 compId, s32 drawable, s32& texture, CPedModelInfo* pModelInfo, s32& forceTexMatchId, s32& forcePaletteId);

	// PURPOSE: Helper function to update the component inclusion/exclusion sets
	void UpdateInclusionAndExclusionSets(const atFixedBitSet32* compInclusions, const atFixedBitSet32* compExclusions, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, atFixedBitSet32& inclusionsSet, atFixedBitSet32& exclusionsSet);

	// PURPOSE: Helper function to set MustUse component variations
	void SetRequiredComponentVariations(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData, CPedModelInfo* pModelInfo, const CPed** sameTypePeds, u32 numSameTypePeds, s32& forceTexMatchId, s32& forcePaletteId, atFixedBitSet32& inclusionsSet, atFixedBitSet32& exclusionsSet, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, ePVRaceType& race, const CPedCompRestriction* pRestrictions = NULL, int numRestrictions = 0, u32 satreamingFlags = 0);



    atArray<CPedVariationInfo*> m_infos;

	bool						m_bHasLowLODs;
	bool						m_bIsStreamed;
	bool						m_bIsSuperLOD;


	//////////////////////////////////////////////////////////////
	// PROPS
	s32							m_aAnchorBoneIdxs[NUM_ANCHORS];		// store the bone idxs at init to avoid look up
};

/*
PURPOSE
	A single restriction on a ped component, which either must use a certain
	drawable, or can't use a certain drawable. Used as an input parameter
	to SetVarRandom(), for restricting which variations can be used for certain
	scenarios.
*/
class CPedCompRestriction
{
public:
	// PURPOSE:	Specifies which type of restriction this is.
	enum Restriction
	{
		CantUse,	// Can only use this drawable index for this component.
		MustUse		// May not use this drawable index for this component.
	};

	// PURPOSE:	Check if a specific drawable index can be used for a component.
	bool IsRestricted(ePedVarComp comp, int drawableIndex) const
	{
		if(m_Component != comp)
		{
			// The restriction doesn't apply to this component.
			return false;
		}
		if(m_Restriction == CantUse)
		{
			// If the drawable index matches, we can't use this.
			if(drawableIndex == m_DrawableIndex)
			{
				return true;
			}
		}
		else
		{
			Assert(m_Restriction == MustUse);

			// If the drawable index doesn't match, we can't use this.
			if(drawableIndex != m_DrawableIndex)
			{
				return true;
			}
		}
		// No restriction.
		return false;
	}

	ePedVarComp GetComponentSlot() const { return m_Component; }
	int GetDrawableIndex() const { return m_DrawableIndex; }
	int GetTextureIndex() const { return m_TextureIndex; }
	Restriction GetRestriction() const { return m_Restriction; }

protected:
	// PURPOSE:	The component this restriction applies for.
	ePedVarComp	m_Component;

	// PURPOSE:	The drawable index within the component that this restriction is for.
	int			m_DrawableIndex;

	// PURPOSE:	The drawable index within the component that this restriction is for.
	int			m_TextureIndex;

	// PURPOSE:	The type of restriction this is.
	Restriction	m_Restriction;

	PAR_SIMPLE_PARSABLE;
};

#endif //_PEDVARIATIONDS_H_
