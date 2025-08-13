// Title	:	PedVariationPack.h
// Author	:	John Whyte
// Started	:	12/09

// Handles the automatic processing & preparation of ped models to generate multiple variations from
// the source dictionaries. Models and textures are loaded as packs in order to generate multiple variations cost effectively


#ifndef _PEDVARIATIONPACK_H_
#define _PEDVARIATIONPACK_H_

#include "fwscene/stores/dwdstore.h"

#include "modelinfo/PedModelInfo.h"
#include "scene/RegdRefTypes.h"
#include "peds\rendering\pedVariationDS.h"

#define DIFF_TEX_NAME_LEN (20)

class CCustomShaderEffectBase;
class CPed;
class CCustomShaderEffectPed;

class CPedVariationPack
{
friend class CPedVariationDebug;

public:
	CPedVariationPack(void);
	~CPedVariationPack(void);

	// used to setup and remove some debug stuff for the time being
	static bool Initialise(void);
	static void Shutdown(unsigned shutdownMode);

	static bool LoadSlods(void);
	static void FlushSlods(void);

	static void RenderShaderSetup(CPedModelInfo* pModelInfo, CPedVariationData& varData, CCustomShaderEffectPed *pShaderEffect);

	static void RenderSLODPed(const Matrix34& rootMatrix, const grmMatrixSet *pMatrixSet, u32 bucketMask, eRenderMode renderMode, CCustomShaderEffectBase* shaderEffect, eSuperlodType slodType);
	static void RenderPed(CPedModelInfo *pModelInfo, const Matrix34& rootMatrix, const grmMatrixSet *pMatrixSet, CPedVariationData& varData, 
		CCustomShaderEffectPed *pShaderEffect, u32 bucketMask, u32 lodIdx, eRenderMode renderMode, bool bUseBlendShapeTechnique, bool bAddToMotionBlurMask, bool bIsFPSSwimming);

	static void SetVarCycle(CDynamicEntity* pDynamicEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData);
	static void SetVarDefault(CDynamicEntity* pDynamicEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData);

	static s32 GetExistingPedVariations( CDynamicEntity* pDynamicEntity, s32& numSameTypePeds, CPedVariationData* * sameTypePeds );
	static void SortTextures(const CPedVariationInfo* pVarInfo, ePedVarComp comp, CPedVariationData** peds, u32 numPeds, atArray<sVarSortedItem>& offsets);
	static void RandomizeTextures(const CPedVariationInfo* pVarInfo, ePedVarComp comp, atArray<sVarSortedItem>& offsets);

	static u8	GetTexVar(const CPed* pPed, ePedVarComp slotId);
	static u32	GetCompVar(const CPed* pPed, ePedVarComp slotId);
	static u8	GetPaletteVar(const CPed* pPed, ePedVarComp slotId);

	static void GetLocalCompData(const CPed* pPed, ePedVarComp compId, u32 compIndx, u32& outVarInfoHash, u32& outLocalIndex);
	static u32 GetGlobalCompData(const CPed* pPed, ePedVarComp compId, u32 varInfoHash, u32 localIndex);

	static bool	HasVariation(CPed* pPed, ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId);
	static bool	SetVariation(CDynamicEntity* pDynamicEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData, ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId);
	static bool CheckVariation(CPed* pPed);

	static bool ApplySelectionSetByHash(CPed* ped, class CPedPropData& pedPropData, CPedVariationData& pedVarData, u32 setHash);

	static void UpdatePedForAlphaComponents(CEntity* pEntity, class CPedPropData& pedPropData, CPedVariationData& pedVarData);

	static void BuildComponentName(char* pString, u32 slotIdx, u32 drawblIdx, u8 drawblAltIdx, char suffix, bool hd);

	static rmcDrawable* ExtractComponent(const Dwd* pDwd, ePedVarComp slotIdx, u32 drawblIdx, u32* pDrawblHash, u8 drawblAltIdx);

	static bool IsVariationValid(CDynamicEntity* pDynamicEntity, ePedVarComp componentId, s32 drawableId, s32 textureId);

	static const crExpressions* GetExpressionForComponent(CPed* ped, ePedVarComp component,crExpressionsDictionary& defaultDictionary,s32 f);

	// Return the specified component for the current ped
	static rmcDrawable* GetComponent(ePedVarComp eComponent, const CEntity* pEntity, CPedVariationData& pedVarData);

	static void Visualise(const CPed&);
	static clothVariationData* CreateCloth(characterCloth* pCClothTemplate, const crSkeleton* skel, const s32 dictSlotId );

	static void BuildDiffuseTextureName(char buf[DIFF_TEX_NAME_LEN], ePedVarComp component, u32 drawblId, char texVariation, ePVRaceType raceId);
private:
	static void	ClearShader(CCustomShaderEffectPed *pShaderEffect);
	static void	SetModelShaderTex(CCustomShaderEffectPed *pShaderEffect, u32 txdIdx, ePedVarComp component, u32 texId, char texVariation, ePVRaceType raceId, u8 paletteId, CPedModelInfo* pModelInfo);

	static bool IsBlendshapesSetup(rmcDrawable* pDrawable);

	static s32	m_BSTexGroupId;

	static CPedModelInfo*	ms_pPedSlodHumanMI;
	static CPedModelInfo* 	ms_pPedSlodSmallQuadpedMI;
	static CPedModelInfo* 	ms_pPedSlodLargeQuadPedMI;

	static bool		ms_aDrawPlayerComp[MAX_DRAWBL_VARS];
};

#endif //_PEDVARIATIONPACK_H_
