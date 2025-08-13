// Title	:	PedVariationDebug.h
// Author	:	John Whyte
// Started	:	12/09

// Moved debug functionality all out to separate file to avoid contamination with game code


#ifndef _PEDVARIATIONDEBUG_H_
#define _PEDVARIATIONDEBUG_H_

#if __BANK

#include "fwscene/stores/dwdstore.h"

#include "scene/RegdRefTypes.h"
#include "peds/rendering/pedVariationDS.h"
#include "peds/rendering/PedVariationStream.h"

class CPed;

// -----------


class CPedVariationDebug
{
public:

	CPedVariationDebug(void);
	~CPedVariationDebug(void);

	// used to setup and remove some debug stuff for the time being
	static bool Initialise(void);
	static void Shutdown(void);
	static void ShutdownLevel(void);

	static void InitDebugData();

	static bool	HasVariation(CPed* pPed, ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId);
	static bool CheckVariation(CPed* pPed);

	static bool	IsRaceCompatible(ePVRaceType typeA, ePVRaceType typeB);
	
	static void	FocusEntityChanged(CEntity* pEntity); 

	static bool IsVariationValid(CDynamicEntity* pDynamicEntity, ePedVarComp componentId, s32 drawableId, s32 textureId);

	// PURPOSE: Add debug widgets for ped variations
	static void PopulatePedVariationInfos();
	static bool SetupWidgets(bkBank& bank);
	static void PedIDChangeCB(void);
	static void UpdatePedList(void);
	static void SetRelationship(void);
	static u32 GetWidgetPedType(void);

	static void HeadBlendSliderChangeCB(void);
	static void HeadBlendGeomAndTexSliderChangeCB(void);
	static void HeadBlendComboChangeCB(void);
	static void HeadBlendRefreshCB(void);
    static void HeadBlendCreateSecondGenChildCB(void);
    static void HeadBlendUpdateSecondGenChildCB(void);
	static void HeadBlendUpdateSecondGenBlendCB(void);
	static void HeadBlendAssignParent0CB(void);
	static void HeadBlendAssignParent1CB(void);
	static void HeadBlendClearParentsCB(void);
	static void HeadBlendFinalizeCB(void);
	static void HeadBlendOverlayBaseDetailChangeCB(void);
	static void HeadBlendOverlayBlemishesChangeCB(void);
	static void HeadBlendOverlaySkinDetail1ChangeCB(void);
	static void HeadBlendOverlaySkinDetail2ChangeCB(void);
	static void HeadBlendOverlayBody1ChangeCB(void);
	static void HeadBlendOverlayBody2ChangeCB(void);
	static void HeadBlendOverlayBody3ChangeCB(void);
	static void HeadBlendHairRampChangeCB(void);
	static void HeadBlendOverlayFacialHairChangeCB(void);
	static void HeadBlendOverlayEyebrowChangeCB(void);
	static void HeadBlendOverlayAgingChangeCB(void);
	static void HeadBlendOverlayMakeupChangeCB(void);
	static void HeadBlendOverlayBlusherChangeCB(void);
	static void HeadBlendOverlayDamageChangeCB(void);
    static void HeadBlendPaletteRgbCB(void);
    static void HeadBlendEyeColorChangeCB(void);
    static void HeadBlendForceTextureBlendCB(void);
	static void UpdateMicroMorphsCB(void);
	static void ResetMicroMorphsCB(void);
	static void BypassMicroMorphsCB(void);

    static void UpdateHeadBlendEyeColorSlider(void);

	static CPed*	GetDebugPed(void);		// debug damage map stuff wants this
	static void		SetDebugPed(CPed* pPed, s32 modelIndex);

	static grcTexture* GetGreyDebugTexture(void);
	static bool GetDisableDiffuse(void);

	static bool GetForceHdAssets(void);
	static bool GetForceLdAssets(void);

	static void PedComponentCheckUpdate(void);
	static void DebugUpdate(void);
	static atArray<const char*>pedNames; 
	static atArray<const char*> emptyPedNames;
	static s32	currPedNameSelection;
	static bool assignScriptBrains;
	static bool stallForAssetLoad;
	static bool ms_bLetInvunerablePedsBeAffectedByFire;
	static bool ms_UseDefaultRelationshipGroups;
	static bkCombo*	pPedCombo;
	
	static atArray<const char*>pedGrpNames; 

	static bool displayStreamingRequests;

	static void GetPolyCountPlayerInternal(const CPedStreamRenderGfx* pGfx, u32* pPolyCount, u32 lodLevel = 0, u32* pVertexCount = NULL, u32* pVertexSize = NULL, u32* pIndexCount = NULL, u32* pIndexSize = NULL);
	static void GetPolyCountPedInternal(const CPedModelInfo *pModelInfo, const CPedVariationData& varData, u32* pPolyCount, u32 lodLevel = 0, u32* pVertexCount = NULL, u32* pVertexSize = NULL, u32* pIndexCount = NULL, u32* pIndexSize = NULL);
	static void GetVtxVolumeCompPlayerInternal(const CPedStreamRenderGfx* pGfx, float &vtxCount, float &volume);
	static void GetVtxVolumeCompPedInternal(const CPedModelInfo *pModelInfo, const CPedVariationData& varData, float &vtxCount, float &volume);
	static void GetDrawables(const CPedModelInfo* pModelInfo, const CPedVariationData* pVarData, atArray<rmcDrawable*>& aDrawables);
	
#if __PPU
	static bool GetVtxEdgeUsageCompPlayerInternal(const CPedStreamRenderGfx* pGfx);
	static bool GetVtxEdgeUsageCompPedInternal(const CPedModelInfo *pModelInfo, const CPedVariationData& varData);
	static bool GetIsSPUPipelined(const CPedModelInfo *pModelInfo, const CPedVariationData& varData);
	static bool GetIsSPUPipelinedPlayer(const CPedStreamRenderGfx* pGfx);
#endif // __PPU
	
	static void EditMetadataInWorkbenchCB();

	static void CreatePedCB();
	static void CreatePedFromSelectedCB(void);
	static void ClonePedCB(void);
	static void ClonePedToLastCB(void);
	static void CreateNextPedCB(void);
	static void CreateOneMeleeOpponentCB(void);
	static void CreateOneBrainDeadMeleeOpponentCB(void);

	static void populateCarCB(int nForceCrowdSelection, int nNumberPeds = 0, bool bMakeDumb = false);

	static void CycleTypeId(void);
	static void SetTypeIdByName(const char* szName);

	// Return the specified component for the current ped
	static rmcDrawable* GetComponent(ePedVarComp eComponent, CEntity* pEntity, CPedVariationData& pedVarData);

	static void Visualise(const CPed&);
	static void VisualisePedMetadata(const CPed& pedRef);
	
	// accessors for use by smoke tests
	static void SetCrowdSize(const s32 i);
	static void SetCrowdPedSelection(const s32 idx, const s32 id);
	static u32 GetNumPedNames();
	static void SetDistToCreateCrowdFromCamera(const float dist);
	static void SetWanderAfterCreate(const bool set);
	static void SetCreateExactGroupSize(const bool bSet);
	static void SetCrowdSpawnLocation(const Vector3 &pos);
	static void	SetCrowdUniformDistribution(const float dist);

private:
	static bool IsBlendshapesSetup(rmcDrawable* pDrawable);
};

extern void createCrowdCB(void);

#endif // __BANK
#endif //_PEDVARIATIONDEBUG_H_
