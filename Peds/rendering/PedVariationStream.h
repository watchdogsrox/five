// Title	:	PedVariationStream.h
// Author	:	John Whyte
// Started	:	12/09

// Handles the automatic processing & preparation of ped models to generate multiple variations from
// the source dictionaries. Models and textures are loaded per instance to minimize cost for low count instances (ie. the player or cutscene characters)


#ifndef _PEDVARIATIONSTREAM_H_
#define _PEDVARIATIONSTREAM_H_

#include "fwscene/stores/dwdstore.h"

#include "scene/RegdRefTypes.h"
#include "peds\rendering\pedVariationDS.h"
#include "scene/DataFileMgr.h"

#define PED_FP_ANIMPOSTFX_GROUP_ID (64)
#define PED_FP_ANIMPOSTFX_EVENT_TAG_INTRO (1)
#define PED_FP_ANIMPOSTFX_EVENT_TAG_OUTRO (2)

class CCustomShaderEffectBase;
class CPedStreamRenderGfx;
class CPed;
class CCustomShaderEffectPed;
class CCustomShaderEffectPedType;
class CPedStreamRequestGfx;
class CAddCompositeSkeletonCommand;
struct AnimPostFXEvent;

#define PED_FACIAL_OVERLAY_PATH "common:/data/effects/peds/facial_overlays"
class CPedFacialOverlays
{
public:
	atArray<atHashString> m_blemishes;
	atArray<atHashString> m_facialHair;
	atArray<atHashString> m_eyebrow;
	atArray<atHashString> m_aging;
	atArray<atHashString> m_makeup;
	atArray<atHashString> m_blusher;
	atArray<atHashString> m_damage;
	atArray<atHashString> m_baseDetail;
	atArray<atHashString> m_skinDetail1;
	atArray<atHashString> m_skinDetail2;
	atArray<atHashString> m_bodyOverlay1;
	atArray<atHashString> m_bodyOverlay2;
	atArray<atHashString> m_bodyOverlay3;

	PAR_SIMPLE_PARSABLE;
};

enum ePedBlendComponent
{
	PBC_UPPR = 0,
	PBC_LOWR,
	PBC_FEET,
	PBC_MAX
};
CompileTimeAssert(PBC_MAX == 3);

#define PED_SKIN_TONES_PATH "common:/data/effects/peds/ped_skin_tones"
class CPedSkinTones
{
public:
	struct sSkinComp
	{
		atArray<atHashString> m_skin;

		PAR_SIMPLE_PARSABLE;
	};

    atHashString GetSkinToneTexture(u32 comp, u32 id, bool male) const;

	sSkinComp m_comps[3]; // uppr, lowr, feet

	// values in these arrays index into the m_skin arrays
	// NOTE: the order of these arrays and the entries in here need to match the order of the heads in mp_m_freemode_01/mp_f_freemode_01
	atArray<u8> m_males;
	atArray<u8> m_females;
	atArray<u8> m_uniqueMales;
	atArray<u8> m_uniqueFemales;

	PAR_SIMPLE_PARSABLE;
};

#define PED_ALTERNATE_VARIATIONS_PATH "common:/data/pedalternatevariations"
class CAlternateVariations
{
public:
    struct sAlternateVariationComponent
    {
        u8 m_component;
		u8 m_anchor;
        u8 m_index;
		atHashString m_dlcNameHash;

		void Reset()
		{
			m_dlcNameHash.Clear();
		}

		void onPreLoad(parTreeNode*)
		{
			m_dlcNameHash.Clear();
		}

		bool operator == (const sAlternateVariationComponent& rhs) const
		{
			return m_component == rhs.m_component && m_anchor == rhs.m_anchor &&
				m_index == rhs.m_index && m_dlcNameHash == rhs.m_dlcNameHash;
		}

		PAR_SIMPLE_PARSABLE;
    };

    struct sAlternateVariationSwitch
    {
        u8 m_component;
        u8 m_index;
        u8 m_alt;
		atHashString m_dlcNameHash;
        atArray<sAlternateVariationComponent> m_sourceAssets;

		void Reset()
		{
			m_sourceAssets.Reset();
		}

		void onPreLoad(parTreeNode*)
		{
			m_dlcNameHash.Clear();
		}

		bool operator == (const sAlternateVariationSwitch& rhs) const
		{
			return m_component == rhs.m_component && m_index == rhs.m_index &&
				m_alt == rhs.m_alt && m_dlcNameHash == rhs.m_dlcNameHash;
		}

		PAR_SIMPLE_PARSABLE;
    };

	struct sAlternates
	{
		sAlternates() : altComp(0xff), altIndex(0xff), alt(0xff) { m_dlcNameHash.Clear(); }
		u8 altComp;
		u8 altIndex;
		u8 alt;
		atHashString m_dlcNameHash;
	};

#define MAX_NUM_ALTERNATES (160)
    struct sAlternateVariationPed
    {
        bool GetAlternates(u8 comp, u8 index, u32 dlcNameHash, atArray<sAlternates>& alts) const
        {
			Assert(comp != 0xff);
			alts.ResetCount();
            for (s32 i = 0; i < m_switches.GetCount(); ++i)
            {
                for (s32 f = 0; f < m_switches[i].m_sourceAssets.GetCount(); ++f)
                {
                    if (m_switches[i].m_sourceAssets[f].m_component == comp && 
						m_switches[i].m_sourceAssets[f].m_index == index &&
						m_switches[i].m_sourceAssets[f].m_dlcNameHash == dlcNameHash)
                    {
						Assert(alts.GetCount() < MAX_NUM_ALTERNATES - 1);
						sAlternates newAlt;
                        newAlt.altComp = m_switches[i].m_component;
                        newAlt.altIndex = m_switches[i].m_index;
                        newAlt.alt = m_switches[i].m_alt;
						newAlt.m_dlcNameHash = m_switches[i].m_dlcNameHash;
						
						alts.Push(newAlt);
                    }
                }
            }
            return alts.GetCount() > 0;
        }

		bool GetAlternatesFromProp(u8 anchor, u8 index, u32 dlcNameHash, atArray<sAlternates>& alts) const
		{
			Assert(anchor != 0xff);
			alts.ResetCount();
			for (s32 i = 0; i < m_switches.GetCount(); ++i)
			{
				for (s32 f = 0; f < m_switches[i].m_sourceAssets.GetCount(); ++f)
				{
					if (m_switches[i].m_sourceAssets[f].m_anchor == anchor && 
						m_switches[i].m_sourceAssets[f].m_index == index &&
						m_switches[i].m_sourceAssets[f].m_dlcNameHash == dlcNameHash)
					{
						Assert(alts.GetCount() < MAX_NUM_ALTERNATES - 1);
						sAlternates newAlt;
						newAlt.altComp = m_switches[i].m_component;
						newAlt.altIndex = m_switches[i].m_index;
						newAlt.alt = m_switches[i].m_alt;
						newAlt.m_dlcNameHash = m_switches[i].m_dlcNameHash;
						alts.Push(newAlt);
					}
				}
			}
			return alts.GetCount() > 0;
		}

        atHashString m_name;
        atArray<sAlternateVariationSwitch> m_switches;

		PAR_SIMPLE_PARSABLE;
    };

    const sAlternateVariationPed* GetAltPedVar(u32 pedNameHash) const
    {
        for (s32 i = 0; i < m_peds.GetCount(); ++i)
        {
            if (m_peds[i].m_name.GetHash() == pedNameHash)
                return &m_peds[i];
        }
        return NULL;
    }

	void AddAlternateVariation(sAlternateVariationPed& newAltVar)
	{
		if (sAlternateVariationPed* existingPedAltVars = const_cast<sAlternateVariationPed*>(GetAltPedVar(newAltVar.m_name.GetHash())))
		{
			u16 newSwitchCount = (u16)newAltVar.m_switches.GetCount();

			for (u16 i = 0; i < newSwitchCount; i++)
			{
				sAlternateVariationSwitch& newSwitchValue = newAltVar.m_switches[i];
				s32 existingIndex = existingPedAltVars->m_switches.Find(newSwitchValue);

				if (existingIndex < 0)
					existingPedAltVars->m_switches.PushAndGrow(newSwitchValue, newSwitchCount);
				else
				{
					// Append some new source assets to existing switch
					for (u16 j = 0; j < newSwitchValue.m_sourceAssets.GetCount(); j++)
						existingPedAltVars->m_switches[existingIndex].m_sourceAssets.PushAndGrow(newSwitchValue.m_sourceAssets[j], 1);
				}
			}
		}
		else
		{
			m_peds.PushAndGrow(newAltVar, 1);
		}
	}

	void RemoveAlternateVariation(sAlternateVariationPed& newAltVar)
	{
		if (sAlternateVariationPed* existingPedAltVars = const_cast<sAlternateVariationPed*>(GetAltPedVar(newAltVar.m_name.GetHash())))
		{
			for (u32 i = 0; i < newAltVar.m_switches.GetCount(); i++)
			{
				sAlternateVariationSwitch& newSwitchValue = newAltVar.m_switches[i];
				s32 existingSwitchIndex = existingPedAltVars->m_switches.Find(newSwitchValue);

				// Find the switch in the existing data
				if (existingSwitchIndex >= 0)
				{
					sAlternateVariationSwitch& existingSwitch = existingPedAltVars->m_switches[existingSwitchIndex];

					// Find any values from the new switch we need to remove
					for (u16 j = 0; j < newSwitchValue.m_sourceAssets.GetCount(); j++)
					{
						for (u16 k = 0; k < existingSwitch.m_sourceAssets.GetCount(); k++)
						{
							if (newSwitchValue.m_sourceAssets[j] == existingSwitch.m_sourceAssets[k])
							{
								existingSwitch.m_sourceAssets[k].Reset();
								existingSwitch.m_sourceAssets.Delete(k);
								k--;
							}
						}

						// If the switch now no longer has any source assets, remove it
						if (existingSwitch.m_sourceAssets.GetCount() <= 0)
						{
							existingPedAltVars->m_switches[existingSwitchIndex].Reset();
							existingPedAltVars->m_switches.Delete(existingSwitchIndex);
						}
					}
				}
			}
		}
	}

	u32 GetPedCount() { return m_peds.GetCount(); }
	sAlternateVariationPed GetAltPedVarAtIndex(u32 index) { Assert(index < m_peds.GetCount()); return m_peds[index]; }

	PAR_SIMPLE_PARSABLE;

private:
    atArray<sAlternateVariationPed> m_peds;
};


/*************************************************************************
* First person view asset data
**************************************************************************/
struct FirstPersonTimeCycleData
{
    int m_textureId;
    atHashString m_timeCycleMod;
    float m_intensity;

	PAR_SIMPLE_PARSABLE;
};

enum eFPTCInterpType
{
	FPTC_LINEAR = 0,
	FPTC_POW2,
	FPTC_POW4
};

struct FirstPersonPropAnim
{
	atHashString m_timeCycleMod;
	float m_maxIntensity;
	float m_duration;
	float m_delay;

	PAR_SIMPLE_PARSABLE;
};

struct FirstPersonAssetData
{
	FirstPersonAssetData():m_dlcHash(static_cast<u32>(0)),m_eCompType(static_cast<u8>(PV_MAX_COMP)),m_eAnchorPoint(static_cast<u8>(NUM_ANCHORS)),m_localIndex(static_cast<u8>(-1)),m_tcModFadeOffSecs(60.0f),m_tcModInterpType(FPTC_LINEAR){}
	u8				m_eCompType;
	u8				m_eAnchorPoint;
	atHashString	m_dlcHash;
	atArray<FirstPersonTimeCycleData> m_timeCycleMods;
	FirstPersonPropAnim m_putOnAnim;
	FirstPersonPropAnim m_takeOffAnim;
	atHashString	m_audioId;
	atHashString	m_fpsPropModel;
	Vector3			m_fpsPropModelOffset;
	float			m_seWeather;
	float			m_seEnvironment;
	float			m_seDamage;
	float			m_tcModFadeOffSecs;
	eFPTCInterpType m_tcModInterpType;
	u32				m_flags;
	u8				m_localIndex;

	PAR_SIMPLE_PARSABLE;
};

struct FirstPersonPedData
{
    atHashString m_name;
    atArray<FirstPersonAssetData> m_objects;

	PAR_SIMPLE_PARSABLE;
};

#define PED_FIRST_PERSON_SP_ASSET_DATA_PATH "common:/data/effects/peds/sp_first_person"
#define PED_FIRST_PERSON_MP_ASSET_DATA_PATH "common:/data/effects/peds/mp_first_person"
struct FirstPersonAssetDataManager
{
    atArray<FirstPersonPedData> m_peds;

	PAR_SIMPLE_PARSABLE;
};

#define PED_FIRST_PERSON_ALTERNATES_PATH "common:/data/effects/peds/first_person_alternates"
struct FirstPersonAlternate
{
	atHashString m_assetName;
	u8 m_alternate;
	PAR_SIMPLE_PARSABLE;
};
struct FirstPersonAlternateData
{
	atArray<FirstPersonAlternate> m_alternates;
	PAR_SIMPLE_PARSABLE;
};

#define PED_MP_TINT_DATA_PATH "common:/data/effects/peds/tint_data"
struct MpTintLastGenMapping
{
	u8 m_texIndex;
	u8 m_paletteRow;
	PAR_SIMPLE_PARSABLE;
};
struct MpTintAssetData
{
	atHashString m_assetName;
	bool m_secondaryHairColor;
	bool m_secondaryAccsColor;
	atArray<MpTintLastGenMapping> m_lastGenMapping;
	PAR_SIMPLE_PARSABLE;
};
struct MpTintDualColor
{
	u8 m_primaryColor;
	u8 m_defaultSecondaryColor;
	PAR_SIMPLE_PARSABLE;
};
struct MpTintData
{
	atArray<MpTintDualColor> m_creatorHairColors;
	atArray<u8> m_creatorAccsColors;
	atArray<u8> m_creatorLipstickColors;
	atArray<u8> m_creatorBlushColors;
	atArray<MpTintDualColor> m_barberHairColors;
	atArray<u8> m_barberAccsColors;
	atArray<u8> m_barberLipstickColors;
	atArray<u8> m_barberBlushColors;
	atArray<u8> m_barberBlushFacepaintColors;
	atArray<MpTintAssetData> m_assetData;
	PAR_SIMPLE_PARSABLE;
};

// -----------

class CPedVariationStream
{
friend class CPedVariationDebug;

public:
	CPedVariationStream(void) {}
	~CPedVariationStream(void) {}

	// used to setup and remove some debug stuff for the time being
	static bool Initialise(void);
	static void ShutdownLevel(void);
	static void InitSystem(void);
	static void ShutdownSystem(void);
	static void ShutdownSession(unsigned shutdownMode);

	static void AddAlternateVariations(const char* filePath);
	static void RemoveAlternateVariations(const char* filePath);

	static void LoadFirstPersonData();
	static void AddFirstPersonData(const char* filePath);
	static void RemoveFirstPersonData(const char* filePath);

	static void AddFirstPersonAlternateData(const char* filePath);

	static void InitStreamPedFile();

	static void RegisterStreamPedFile(const CDataFileMgr::DataFile* pFile);

	static void RenderPed(CPedModelInfo *pModelInfo, const Matrix34& rootMatrix, CAddCompositeSkeletonCommand& skeletonCmd, CPedStreamRenderGfx* pGfx, 
		CCustomShaderEffectPed *pShaderEffect, u32 bucketMask, eRenderMode renderMode, u16 drawableStats, u8 lodIdx, u32 perComponentWetness, bool bIsFPSSwimming, bool bIsFpv, bool bEnableMotionBlurMask, bool bSupportsHairTint);

	static void SetVarCycle(CDynamicEntity* pDynamicEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData);
	static void SetVarDefault(CDynamicEntity* pDynamicEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData, u32 streamingFlags = 0);

	static u8	GetTexVar(CPed* pPed, ePedVarComp slotId);
	static u32	GetCompVar(CPed* pPed, ePedVarComp slotId);
	static u8	GetPaletteVar(CPed* pPed, ePedVarComp slotId);

	static bool	HasVariation(CPed* pPed, ePedVarComp slotId, u32 drawblId, u32 texId);
	static bool	SetVariation(CDynamicEntity* pDynamicEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData, 
		ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId, s32 streamFlags, bool force);

	static bool RequestHdAssets(CPed* ped);

	static void ProcessPlayerModelLoading(void);

	static bool	IsRaceCompatible(ePVRaceType typeA, ePVRaceType typeB);


	static rmcDrawable* ExtractComponent(const Dwd* pDwd, ePedVarComp slotIdx, u32 drawblIdx, u32* pDrawblHash);

	static bool IsVariationValid(CDynamicEntity* pDynamicEntity, ePedVarComp componentId, s32 drawableId, s32 textureId);


	static fiDevice* MountTargetDevice(const CDataFileMgr::DataFile* pFile);
	static void UnmountTargetDevice();

	static void RequestStreamPedFiles(CPed* pPed, CPedVariationData* pNewVarData, s32 flags = 0);
	static bool RequestStreamPedFilesDummy(CPed* pPed, CPedVariationData* pNewVarData, CPedStreamRequestGfx* pNewStreamGfx);
	static bool RequestStreamPedFilesInternal(CPed* pPed, CPedVariationData* pNewVarData, bool isDummy, CPedStreamRequestGfx* pNewStreamGfx, s32 flags = 0);

	static void ApplyStreamPedFiles(CPed* pPed, CPedVariationData* pNewVarData, CPedStreamRequestGfx* pPSGfx);
	static void ApplyHdAssets(CPed* pPed, CPedStreamRequestGfx* pPSGfx);
	static void RemoveHdAssets(CPed* pPed);
	static void SetupSkeletonMap(CPedModelInfo* pmi, CPedStreamRenderGfx* gfx, bool hd);

	static void TriggerHDTxdUpgrade(CPed* pPed);

	// Return the specified component for the current ped
	static rmcDrawable* GetComponent(ePedVarComp eComponent, CEntity* pEntity, CPedVariationData& pedVarData);

	static void Visualise(const CPed&);

	static void	SetDrawPlayerComp(u32 compIdx, bool bEnable) { if (compIdx < MAX_DRAWBL_VARS) {ms_aDrawPlayerComp[compIdx] = bEnable; }}

	static void ProcessStreamRequests(void);
	static void ProcessSingleStreamRequest(CPedStreamRequestGfx* pStreamReq);

	static const CPedFacialOverlays& GetFacialOverlays() { return ms_facialOverlays; } 
	static const CPedSkinTones& GetSkinTones() { return ms_skinTones; } 
	static const CAlternateVariations& GetAlternateVariations() { return ms_alternateVariations; } 

	static const MpTintData& GetMpTintData() { return ms_mpTintData; } 

	static bool IsBlendshapesSetup(rmcDrawable* pDrawable);

	static void SetPaletteTexture(CPed* pPed, u32 comp);

	static const char* CustomIToA3(u32 val, char* result);
	static const char* CustomIToA(u32 val, char* result);

	// All this should really be pulled out into another class...
	static bool HasFirstPersonTimeCycleModifier();
	static atHashString GetFirstPersonTimeCycleModifierName();
	static float GetFirstPersonTimeCycleModifierIntensity();
	static void PreScriptUpdateFirstPersonProp();
	static void UpdateFirstPersonProp();
	static void AddFirstPersonEntitiesToRenderList();
	static void UpdateFirstPersonPropFromCamera();
	static float GetFirstPersonPropEnvironmentSE();
	static float GetFirstPersonPropWeatherSE();

#if __BANK
	static void ReloadFirstPersonMeta();
#endif

	static void OnAnimPostFXEvent(AnimPostFXEvent* pEvent);

	// Maximum number of first person prop models that a ped can have on at once
	static const size_t MAX_FP_PROPS = 3;

private:
	struct FirstPersonPropData
	{
		enum AnimState
		{
			STATE_Idle,					// No anim in progress
			STATE_DelayOn,				// Delay before "put on" anim
			STATE_AnimOnIn,				// Lerp TC modifier in (0..1) for "put on" anim
			STATE_AnimOnOut,			// Lerp TC modifier out (1..0) for "put on" anim
			STATE_DelayOff,				// Delay before "take off" anim
			STATE_AnimOffIn,			// Lerp TC modifier in (0..1) for "take off" anim
			STATE_AnimOffOut,			// Lerp TC modifier out (1..0) for "take off" anim
			STATE_AnimOffComplete		// "Take off" anim complete, prepare for idle state
		};

		FirstPersonPropData() : pPrevData(NULL), pData(NULL), pTC(NULL), pModelInfo(NULL), animState(STATE_Idle), animTimer(0), animT(0.0f), anchorOverride(ANCHOR_NONE), renderThisFrame(false), inEffectActive(false), tcModTriggered(false), triggerFXOutro(false) {}
		void Cleanup();

		FirstPersonAssetData* pPrevData;
		FirstPersonAssetData* pData;
		FirstPersonTimeCycleData* pTC; // May be null if no TC mod in effect
		fwModelId modelId;
		atHashString propModel; // Hash string name for modelId
		RegdObj pObject;
		CBaseModelInfo* pModelInfo;
		AnimState animState;
		u32 animTimer;
		float animT; // 0..1 range
		eAnchorPoints anchorOverride;
		bool renderThisFrame;
		bool inEffectActive;
		bool tcModTriggered;
		bool triggerFXOutro;
		bool hasConsumedTriggerFXOutro;
	};
	static void UpdateFirstPersonObjectData(); // Only updates ms_firstPersonProp
	static void UpdateFirstPersonPropSlot(FirstPersonPropData& propData BANK_ONLY(, size_t idx));
	static size_t GetHighestWeightedTCModFirstPersonSlotIdx();
	static void AddBodySkinRequest(CPedStreamRequestGfx* pNewStreamGfx, u32 id, u32 comp, u32 slot, u32 streamFlags, CPedModelInfo* modelInfo);
	static bool AddComponentRequests(CPedStreamRequestGfx* gfx, CPedModelInfo* pmi, u8 drawableId, u8 texDrawableId, u8 texId, s32 streamFlags, eHeadBlendSlots headSlot, eHeadBlendSlots feetSlot, eHeadBlendSlots upprSlot, eHeadBlendSlots lowrSlot, eHeadBlendSlots texSlot, u32 headHash);

	static bool m_bUseStreamPedFile;
	static fiDevice* m_pDevice;

	static s32	m_BSTexGroupId;

	static s32	ms_playerFragModelIdx;

	static bool		ms_aDrawPlayerComp[MAX_DRAWBL_VARS];

	static CPedFacialOverlays ms_facialOverlays;
	static CPedSkinTones ms_skinTones;
    static CAlternateVariations ms_alternateVariations;
	static FirstPersonAssetDataManager ms_firstPersonData;
	static FirstPersonPropData ms_firstPersonProp[MAX_FP_PROPS];
	static FirstPersonAlternateData ms_firstPersonAlternateData;
	static MpTintData ms_mpTintData;
	static bool	ms_animPostFXEventTriggered;

};

// ---for storing the graphics that we need to render the player---
class CPedStreamRenderGfx : public fwExtension 
{
public:
	EXTENSIONID_DECL(CPedStreamRenderGfx, fwExtension);

	s32			m_dwdIdx[PV_MAX_COMP]; 
	s32			m_txdIdx[PV_MAX_COMP];						// index for texture dict containing 
	s32			m_hdTxdIdx[PV_MAX_COMP];
	s32			m_hdDwdIdx[PV_MAX_COMP];
	s32			m_cldIdx[PV_MAX_COMP];						// index for cloth dict
	s32			m_headBlendIdx[HBS_MAX];					// indices for head blend assets
	s32			m_fpAltIdx[PV_MAX_COMP];					// indices for potential first person alternate drawables
	u16*		m_skelMap[PV_MAX_COMP];
	u8			m_skelMapBoneCount[PV_MAX_COMP];

	u16			m_culledComponents;							// mask each bit represents a component, bit is set to 1 if that drawable is culled by lod
	u16			m_cullInFirstPerson;

	u16			m_hasHighTxdForComponentTxd;
	u16			m_hasHighTxdForComponentDwd;

	clothVariationData* m_clothData[PV_MAX_COMP];
	CCustomShaderEffectPed*	 m_pShaderEffect;
	CCustomShaderEffectPedType*	 m_pShaderEffectType;

	// ref count used to safely use pointer to this class on render thread
	s16			m_refCount;
	bool		m_used;

	bool		m_bDisableBulkyItems;

	CPedStreamRenderGfx();
	CPedStreamRenderGfx(CPedStreamRequestGfx *pReq);
	~CPedStreamRenderGfx();

	FW_REGISTER_CLASS_POOL(CPedStreamRenderGfx); 

	void ReleaseGfx();
	void ReleaseHeadBlendAssets();

	grcTexture* GetTexture(u32 compSlot);
	grcTexture* GetHDTexture(u32 compSlot);
	s32 GetTxdIndex(u32 slot) const;
	s32 GetHDTxdIndex(u32 slot) const;
	s32 GetDwdIndex(u32 slot) const;
	s32 GetHDDwdIndex(u32 slot) const;
	rmcDrawable* GetDrawable(u32 compSlot) const;
	rmcDrawable* GetHDDrawable(u32 compSlot);
	const rmcDrawable* GetDrawableConst(u32 compSlot) const;
	characterCloth* GetCharacterCloth(u32 compSlot);

	rmcDrawable* GetFpAlt(u32 slot) const;
	s32 GetFpAltIndex(u32 slot) const { Assert(slot < PV_MAX_COMP); return m_fpAltIdx[slot]; }

	s32 GetHeadBlendAssetIndex(u32 slot) const { Assert(slot < HBS_MAX); return m_headBlendIdx[slot]; }
	grcTexture* GetHeadBlendTexture(u32 slot) const;
	rmcDrawable* GetHeadBlendDrawable(u32 slot) const;

	void ReplaceTexture(u32 compSlot, s32 newAsset);
	void ReplaceDrawable(u32 compSlot, s32 newAsset);
	void ApplyNewAssets(CPed* pPed);

	void DisableAllCloth();
	void DeleteCloth(u32 compSlot);
	clothVariationData* GetClothData(u32 compSlot);

	void AddRefsToGfx();
	bool ValidateStreamingIndexHasBeenCached();

	void AddRef() { m_refCount++; }
	void SetUsed(bool val);

	void SetHdAssets(CPedStreamRequestGfx* reqGfx);

	static void Init();
	static void Shutdown();

	static void RemoveRefs();
	static atArray<CPedStreamRenderGfx*> ms_renderRefs[FENCE_COUNT];
	static s32 ms_fenceUpdateIdx;

#if !__NO_OUTPUT
	RegdPed m_targetEntity;
#endif // !__NO_OUTPUT 
};

#if !__DEV
inline bool CPedStreamRenderGfx::ValidateStreamingIndexHasBeenCached() { return true;}
#endif

// -----------

class CPedStreamRequestGfx : public fwExtension
{
	enum	state{ PS_INIT, PS_REQ, PS_BLEND, PS_USE, PS_FREED };

public:
	EXTENSIONID_DECL(CPedStreamRequestGfx, fwExtension);

	CPedStreamRequestGfx(REPLAY_ONLY(bool isDummy = false));
	~CPedStreamRequestGfx();

	FW_REGISTER_CLASS_POOL(CPedStreamRequestGfx); 

	void RequestsComplete();
	void Release();

	bool HaveAllLoaded() 
	{ 
		return(	m_reqDwds.HaveAllLoaded() && 
				m_reqTxds.HaveAllLoaded() && 
				m_reqClds.HaveAllLoaded() && 
				m_reqHeadBlend.HaveAllLoaded() &&
				m_reqFpAlts.HaveAllLoaded() &&
				m_bDelayCompletion==false
			); 
	}

	bool WasLoading() { return m_state == PS_REQ; }
	void SetBlendingStarted() { Assert(m_state == PS_REQ); m_state = PS_BLEND; }
	bool WasBlending() { return m_state == PS_BLEND; }
	void SetBlendingCompleted() { Assert(m_state == PS_BLEND); m_state = PS_USE; }
	bool IsReady() { return m_state == PS_USE; }

	bool HaveHeadBlendReqsLoaded() { return m_reqHeadBlend.HaveAllLoaded(); }

	void PushRequest() { Assert((m_state == PS_INIT) || (m_state == PS_FREED)); m_state = PS_REQ; }

	bool AddTxd(u32 componentSlot, const strStreamingObjectName txdName, s32 streamFlags, CPedModelInfo* pPedModelInfo);
	void AddDwd(u32 componentSlot, const strStreamingObjectName dwdName, s32 streamFlags, CPedModelInfo* pPedModelInfo);
	void AddCld(u32 componentSlot, const strStreamingObjectName cldName, s32 streamFlags, CPedModelInfo* pPedModelInfo);
	void AddHeadBlendReq(u32 slot, const strStreamingObjectName assetName, s32 streamFlags, CPedModelInfo* pPedModelInfo);
	void AddFpAlt(u32 slot, const strStreamingObjectName fpAltName, s32 flags, CPedModelInfo* pmi ASSERT_ONLY(, const char* assetName, const char* drawablName));

	s32 GetTxdSlot(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return(m_reqTxds.GetRequestId(compSlot)); }
	strLocalIndex GetDwdSlot(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return(strLocalIndex(m_reqDwds.GetRequestId(compSlot))); }
	s32 GetCldSlot(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return(m_reqClds.GetRequestId(compSlot)); }
	//s32 GetHeadBlendSlot(u32 slot) { Assert(slot < HBS_MAX); return(m_reqHeadBlend.GetRequestId(slot)); }
	strLocalIndex GetHeadBlendSlot(u32 slot) { Assert(slot < HBS_MAX); return(strLocalIndex(m_reqHeadBlend.GetRequestId(slot))); }
	strLocalIndex GetFpAltSlot(u32 slot) { Assert(slot < PV_MAX_COMP); return strLocalIndex(m_reqFpAlts.GetRequestId(slot)); }

	bool IsDwdSlotPending(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return m_reqDwds.GetRequestId(compSlot) >= 0 && !m_reqDwds.GetRequest(compSlot).HasLoaded(); }
	bool IsTxdSlotPending(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return m_reqTxds.GetRequestId(compSlot) >= 0 && !m_reqTxds.GetRequest(compSlot).HasLoaded(); }
	bool IsCldSlotPending(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return m_reqClds.GetRequestId(compSlot) >= 0 && !m_reqClds.GetRequest(compSlot).HasLoaded(); }
	bool IsHeadBlendSlotPending(u32 slot) { Assert(slot < HBS_MAX); return m_reqHeadBlend.GetRequestId(slot) >= 0 && !m_reqHeadBlend.GetRequest(slot).HasLoaded(); }
	bool IsFpAltSlotPending(u32 compSlot) { Assert(compSlot < PV_MAX_COMP); return m_reqFpAlts.GetRequestId(compSlot) >= 0 && !m_reqFpAlts.GetRequest(compSlot).HasLoaded(); }

#if GTA_REPLAY
	bool m_isDummy;
	atFixedArray<strStreamingObjectName, PV_MAX_COMP> &GetTxdNames() { return m_reqTxdsNames; }
	atFixedArray<strStreamingObjectName, PV_MAX_COMP> &GetDwdNames() { return m_reqDwdsNames; }
	atFixedArray<strStreamingObjectName, PV_MAX_COMP> &GetCldNames() { return m_reqCldsNames; }
	atFixedArray<strStreamingObjectName, HBS_MAX> &GetHeadBlendNames() { return m_reqHeadBlendNames; }
	atFixedArray<strStreamingObjectName, PV_MAX_COMP> &GetFpAltNames() { return m_reqFpAltsNames; }
#endif // GTA_REPLAY

	void SetTargetEntity(CPed *pEntity);
	CPed* GetTargetEntity(void) { return(m_pTargetEntity); }

	void SetHasHdAssets(bool val) { m_bHdAssets = val; }
	bool HasHdAssets() const { return m_bHdAssets; }

	void SetHasHeadBlendAssets(u32 blendHandle) { m_blendHandle = blendHandle; }
		 
	void SetDelayCompletion(bool bDelay) { m_bDelayCompletion = bDelay; }

	void SetWaitForGfx(CPedPropRequestGfx* gfx) { m_waitForGfx = gfx; }
	CPedPropRequestGfx* GetWaitForGfx() { return m_waitForGfx; }
	void ClearWaitForGfx();
	bool IsSyncGfxLoaded() const { return !m_waitForGfx || m_waitForGfx->HaveAllLoaded(); }

	void	SetHighTxdForComponentTxd(u32 bit)	{ m_hasHighTxdForComponentTxd |= 1<<bit; }
	void	SetHighTxdForComponentDwd(u32 bit)	{ m_hasHighTxdForComponentDwd |= 1<<bit; }
	u16		HasHighTxdForComponentTxd()	const	{ return (m_hasHighTxdForComponentTxd); }
	u16		HasHighTxdForComponentDwd()	const	{ return (m_hasHighTxdForComponentDwd); }

private:
	strRequestArray<PV_MAX_COMP>	m_reqDwds;
	strRequestArray<PV_MAX_COMP>	m_reqTxds;
	strRequestArray<PV_MAX_COMP>	m_reqClds;
	strRequestArray<HBS_MAX>		m_reqHeadBlend;
	strRequestArray<PV_MAX_COMP>	m_reqFpAlts;
#if GTA_REPLAY
	atFixedArray<strStreamingObjectName, PV_MAX_COMP>	m_reqDwdsNames;
	atFixedArray<strStreamingObjectName, PV_MAX_COMP>	m_reqTxdsNames;
	atFixedArray<strStreamingObjectName, PV_MAX_COMP>	m_reqCldsNames;
	atFixedArray<strStreamingObjectName, HBS_MAX>   	m_reqHeadBlendNames;
	atFixedArray<strStreamingObjectName, PV_MAX_COMP>	m_reqFpAltsNames;
#endif // GTA_REPLAY

	CPed*	m_pTargetEntity;

    CompileTimeAssert(HBS_MAX <= 64);
    u64		m_dontDeleteMaskBlend;

	state	m_state;

	CPedPropRequestGfx* m_waitForGfx;

    u32     m_blendHandle;

	CompileTimeAssert(PV_MAX_COMP <= 16);
	u16		m_dontDeleteMaskTxd;
	u16		m_dontDeleteMaskDwd;
	u16		m_dontDeleteMaskCld;
	u16		m_dontDeleteMaskFpAlt;

	u16		m_hasHighTxdForComponentTxd;
	u16		m_hasHighTxdForComponentDwd;

	bool	m_bDelayCompletion;
	bool	m_bHdAssets;

};

#if __ASSERT
extern const char* parser_eAnchorPoints_Strings[];
extern const char* parser_ePedVarComp_Strings[];
#endif // __ASSERT

#endif //_PEDVARIATIONSTREAM_H_
