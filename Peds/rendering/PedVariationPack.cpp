//
// PedVariationPack.cpp
//
// Handles the automatic processing & preparation of ped models to generate multiple variations from
// the source dictionaries. Models and textures are loaded as packs in order to generate multiple variations cost effectively
//
// 2009/12/01 - JohnW: splitted from PedVariationMgr.cpp;
//
//
//
#include "Peds/rendering/PedVariationPack.h"


// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/button.h"
#include "bank/group.h"
#include "bank/slider.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletonData.h"
#include "data/callback.h"
#include "diag/art_channel.h"
#include "file/device.h"
#include "file/device_relative.h"
#include "file/diskcache.h"
#include "file/packfile.h"
#include "fragment/cache.h"
#include "fragment/drawable.h"
#include "fragment/tune.h"
#include "grcore/debugdraw.h"
#include "grcore/im.h"
#include "grcore/texture.h"
#include "grcore/texturereference.h"
#include "grcore/image.h"
#include "grmodel/matrixset.h"
#include "rmcore/drawable.h"
#include "grmodel/geometry.h"
#include "grmodel/shaderFx.h"
#include "grmodel/shaderGroup.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/clothstore.h"
#include "fwsys/fileExts.h"
#include "fwpheffects/clothmanager.h"
#include "parser/manager.h"

//game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "core/game.h"
#include "Debug/DebugGlobals.h"
#include "Debug/DebugScene.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/NetworkInterface.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped.h"
#include "peds/ped_channel.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedFactory.h"
#include "Peds/PedPopulation.h"
#include "Peds/pedType.h"
#include "Physics/gtaInst.h"
#include "Physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Renderer/renderer.h"
#include "renderer/RenderThread.h"
#include "shaders/ShaderLib.h"
#include "Script/script.h"
#include "Streaming/streaming.h"
#include "Streaming/streamingmodule.h"
#include "scene/datafilemgr.h"
#include "scene/fileLoader.h"
#include "scene/RegdRefTypes.h"
#include "scene/world/GameWorld.h"
#include "system/filemgr.h"
#include "System/timer.h"
#include "Task\General\TaskBasic.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task\Vehicle\TaskCarAccessories.h"
#include "Tools/smokeTest.h"
#include "Vehicles/VehicleFactory.h"
#include "shaders/ShaderHairSort.h"

//local header
#include "shaders/CustomShaderEffectPed.h"
#include "system/param.h"

#if __BANK
#include "peds/PedDebugVisualiser.h"
#include "peds/rendering/PedVariationDebug.h"
//#include "Renderer/font.h"
#endif //__BANK

static rmcDrawable* lastSlodDrawable = NULL;
static grmShaderGroupVar lastVarDiffText = grmsgvNONE;

PARAM(multiHeadSupport, "Multi Head Support");

AI_OPTIMISATIONS()

#define PV_SEARCH_PATH		"platform:/models/cdimages"

s32	CPedVariationPack::m_BSTexGroupId = -1;
CPedModelInfo* CPedVariationPack::ms_pPedSlodHumanMI = NULL;
CPedModelInfo* CPedVariationPack::ms_pPedSlodSmallQuadpedMI = NULL;
CPedModelInfo* CPedVariationPack::ms_pPedSlodLargeQuadPedMI = NULL;


// Name			:	CPedVariationPack::CPedVariationPack
// Purpose		:	Do application level setup
// Parameters	:	None
// Returns		:	Nothing
CPedVariationPack::CPedVariationPack(void)
{
}

// Name			:	CPedVariationPack::~CPedVariationPack
// Purpose		:	Do application level cleanup
// Parameters	:	None
// Returns		:	Nothing
CPedVariationPack::~CPedVariationPack(void)
{
}

// Name			:	CPedVariationPack::Initialise
// Purpose		:	Do some setup required for restarting game
// Parameters	:	None
// Returns		:	Nothing
bool CPedVariationPack::Initialise(void)
{
	m_BSTexGroupId = grmShaderFx::FindTechniqueGroupId("deferredbs");
	Assert(-1 != m_BSTexGroupId);

	return LoadSlods();
}

void CPedVariationPack::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
		lastSlodDrawable = NULL;

		CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_human"), STRFLAG_DONTDELETE);
		CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_small_quadped"), STRFLAG_DONTDELETE);
		CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_large_quadped"), STRFLAG_DONTDELETE);

	    CPed::Pool *pool = CPed::GetPool();
	    s32 i=pool->GetSize();
	    while(i--)
	    {
		    CPed* pPoolPed = pool->GetSlot(i);
		    if(pPoolPed)
		    {
			    if (pPoolPed->GetDrawHandlerPtr() && pPoolPed->GetDrawHandlerPtr()->GetShaderEffect()){
				    CPedVariationPack::ClearShader(static_cast<CCustomShaderEffectPed*>(pPoolPed->GetDrawHandlerPtr()->GetShaderEffect()));
			    }
		    }
	    }
    }
}

bool CPedVariationPack::LoadSlods(void)
{
	fwModelId modelIds[4];
	ms_pPedSlodHumanMI = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("slod_human",0x3F039CBA), &modelIds[0]));
	ms_pPedSlodSmallQuadpedMI = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("slod_small_quadped",0x2D7030F3), &modelIds[1]));
	ms_pPedSlodLargeQuadPedMI = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("slod_large_quadped",0x856CFB02), &modelIds[2]));

	if (!Verifyf(CModelInfo::RequestAssets(modelIds[0], STRFLAG_DONTDELETE), "Failed to request assets for slod_human") ||
		!Verifyf(CModelInfo::RequestAssets(modelIds[1], STRFLAG_DONTDELETE), "Failed to request assets for slod_small_quadped") ||
		!Verifyf(CModelInfo::RequestAssets(modelIds[2], STRFLAG_DONTDELETE), "Failed to request assets for slod_large_quadped"))
	{
		return false;
	}

	CStreaming::LoadAllRequestedObjects();
	pedAssertf(ms_pPedSlodHumanMI && ms_pPedSlodHumanMI->GetDrawable(), "ped human superlod model has not loaded. This will prove fatal!");
	pedAssertf(ms_pPedSlodSmallQuadpedMI && ms_pPedSlodSmallQuadpedMI->GetDrawable(), "ped small quadruped superlod model has not loaded. This will prove fatal!");
	pedAssertf(ms_pPedSlodLargeQuadPedMI && ms_pPedSlodLargeQuadPedMI->GetDrawable(), "ped large quadruped superlod model has not loaded. This will prove fatal!");

	return true;
}

void CPedVariationPack::FlushSlods(void)
{
	Assertf(ms_pPedSlodHumanMI->GetNumPedModelRefs() == 0, "slod_human - NumPedModelRefs %d", ms_pPedSlodHumanMI->GetNumPedModelRefs());
	Assert(ms_pPedSlodHumanMI->GetDrawable());
	fwModelId slodHuman = fwArchetypeManager::LookupModelId(ms_pPedSlodHumanMI);
	CModelInfo::ClearAssetRequiredFlag(slodHuman, STR_DONTDELETE_MASK);
	CModelInfo::RemoveAssets(slodHuman);

	Assertf(ms_pPedSlodSmallQuadpedMI->GetNumPedModelRefs() == 0, "slod_small_quadped - NumPedModelRefs %d", ms_pPedSlodHumanMI->GetNumPedModelRefs());
	Assert(ms_pPedSlodSmallQuadpedMI->GetDrawable());
	fwModelId slodSmallQuadPed = fwArchetypeManager::LookupModelId(ms_pPedSlodSmallQuadpedMI);
	CModelInfo::ClearAssetRequiredFlag(slodSmallQuadPed, STR_DONTDELETE_MASK);
	CModelInfo::RemoveAssets(slodSmallQuadPed);

	Assertf(ms_pPedSlodLargeQuadPedMI->GetNumPedModelRefs() == 0, "slod_large_quadped - NumPedModelRefs %d", ms_pPedSlodHumanMI->GetNumPedModelRefs());
	Assert(ms_pPedSlodLargeQuadPedMI->GetDrawable());
	fwModelId slodLargeQuadPed = fwArchetypeManager::LookupModelId(ms_pPedSlodLargeQuadPedMI);
	CModelInfo::ClearAssetRequiredFlag(slodLargeQuadPed, STR_DONTDELETE_MASK);
	CModelInfo::RemoveAssets(slodLargeQuadPed);

    lastSlodDrawable = NULL;
    lastVarDiffText = grmsgvNONE;
}

void CPedVariationPack::ClearShader(CCustomShaderEffectPed *pShaderEffect){

	for(u32 i=PV_COMP_HEAD; i<PV_MAX_COMP; i++){
		pShaderEffect->ClearTextures(static_cast<ePedVarComp>(i));
	}
}

void CPedVariationPack::BuildDiffuseTextureName(char buf[DIFF_TEX_NAME_LEN], ePedVarComp component, u32 drawblId, char texVariation, ePVRaceType raceId)
{
	ALIGNAS(8) char texString[10]  = "XXXX_Diff";
	NOTFINAL_ONLY( TrapNZ((size_t)texString & 0x7) );

	// set the texture for the current component
	for (u32 i = 0; i < 4; ++i)
	{
		texString[i] = varSlotNames[component][i];
	}

	// Copy the first 9 chars of the texture name into the lookup string
	*((u64*)buf) = *((u64*)texString); // first 8 bytes
	buf[8] = texString[8];             // 9th byte

	// Convert the drawable ID into a string and put it into both look up strings
	Assert( drawblId <= 999 );
	u32 nHundreds = (drawblId / 100);
	u32 nTens = (drawblId % 100) / 10;
	u32 nSingles = drawblId % 10;
	buf[10] = '0' + (char)nHundreds;
	buf[11] = '0' + (char)nTens;
	buf[12] = '0' + (char)nSingles;

	// Add variation byte, careful not to stomp the two '_'s which should be preserved
	//texLookupString[13] = '_';
	buf[14] = texVariation;
	//texLookupString[15] = '_';

	// Add racial variation ID and don't stomp the termination char.
	Assert(raceId < PV_RACE_MAX_TYPES);
	buf[16] = raceTypeNames[raceId][0];
	buf[17] = raceTypeNames[raceId][1];
	buf[18] = raceTypeNames[raceId][2];
	//texLookupString[19] = '\0';
}

void CPedVariationPack::SetModelShaderTex(CCustomShaderEffectPed *pShaderEffect, u32 txdIdx, ePedVarComp component,
								u32 drawblId, char texVariation, ePVRaceType raceId, u8 paletteId, CPedModelInfo* ASSERT_ONLY(pModelInfo))
{
	u32 texHash = 0;
	u32 palTexHash = 0;
	u32 normHash = 0;
	u32 specHash = 0;

#if __ASSERT
	char debugTexName[20] = {0};
#endif // __ASSERT

	{
		// Format: [texture name_Diff] _ [component number] _ [texture variation letter] _ [race identifier]
		// Only the upper case 'X's in the static string should ever be stomped on. The other chars are
		// assumed to persist and if you stomp them, we will start to fail to find texture variations.
		ALIGNAS(8) static char texLookupString[DIFF_TEX_NAME_LEN]  = "XXXX_Diff_000_X_XXX";
		ALIGNAS(8) static char palTexLookupString[16]  = "XXXX_pall_000_x";
		ALIGNAS(8) static char normLookupString[20]  = "XXXX_normal_000";
		ALIGNAS(8) static char specLookupString[20]  = "XXXX_spec_000";

		//sprintf(texLookupString,"%s_%03d_%c_%s", pTexName, drawblId, texVariation , raceTypeNames[raceId]);

		// --- Faster, specialized replacement for the sprintf above ---

			// Addresses must be 8 byte aligned for all this to work correctly, Fredrik is suspicious of the alignment macros 
			// so adding traps to catch misalignment in !__FINAL builds
			NOTFINAL_ONLY( TrapNZ((size_t)texLookupString & 0x7) );
			NOTFINAL_ONLY( TrapNZ((size_t)palTexLookupString & 0x7) );
			NOTFINAL_ONLY( TrapNZ((size_t)normLookupString & 0x7) );
			NOTFINAL_ONLY( TrapNZ((size_t)specLookupString & 0x7) );

            BuildDiffuseTextureName(texLookupString, component, drawblId, texVariation, raceId);

			//sysMemCpy(normLookupString, pTexName, 4);
			//sysMemCpy(specLookupString, pTexName, 4);
			*((u32*)normLookupString) = *((u32*)texLookupString); // first 4 bytes form normal and spec map names
			*((u32*)specLookupString) = *((u32*)texLookupString); // first 4 bytes form normal and spec map names

			// No need to put an underscore back in on every call, it should be preserved in the static char string
			//texLookupString[9] = '_'; 
			//Assert( texLookupString[9] == '_' );

			// Copy the first 4 chars of the texture name in to the pal look up string
			//sysMemCpy(palTexLookupString, pTexName, 4);		
			*((u32*)palTexLookupString) = *((u32*)texLookupString);

			// The ".._pall_.." sub-string should be preserved in the static char string
			//palTexLookupString[4] = '_';
			//palTexLookupString[5] = 'p';
			//palTexLookupString[6] = 'a';
			//palTexLookupString[7] = 'l';
			//palTexLookupString[8] = 'l';
			//palTexLookupString[9] = '_';
			//Assert( 0 == strncmp( &palTexLookupString[4], "_pall_", 6 ) );

			palTexLookupString[10] = texLookupString[10];
			palTexLookupString[11] = texLookupString[11];
			palTexLookupString[12] = texLookupString[12];

			normLookupString[12] = texLookupString[10];
			normLookupString[13] = texLookupString[11];
			normLookupString[14] = texLookupString[12];

			specLookupString[10] = texLookupString[10];
			specLookupString[11] = texLookupString[11];
			specLookupString[12] = texLookupString[12];

			// Preserve the last 3 bytes of the static pal look up string
			//palTexLookupString[13] = '_';
			//palTexLookupString[14] = 'x';
			//palTexLookupString[15] = '\0';

		// --- End of messy sprintf replacement ---

		texHash = fwTxd::ComputeHash(texLookupString);
		palTexHash = fwTxd::ComputeHash(palTexLookupString);

		normHash = fwTxd::ComputeHash(normLookupString);
		specHash = fwTxd::ComputeHash(specLookupString);

		ASSERT_ONLY(safecpy(debugTexName, texLookupString));
	} 
		
	//grcTexture*	pLookupTex = fwTxd::GetCurrent()->Lookup(texHash);
	fwTxd* pLookupTxd = g_TxdStore.Get(strLocalIndex(txdIdx));
	if (!artVerifyf(pLookupTxd, "Missing texture dictionary, unable to look up ped variation texture '%s' for '%s'\n", debugTexName, pModelInfo->GetModelName()))
		return;

	if (pShaderEffect)
	{
		grcTexture*	pLookupTex = pLookupTxd->Lookup(texHash);
		if (artVerifyf(pLookupTex, "Couldn't find texture %s in txd on ped %s!", debugTexName, pModelInfo->GetModelName()))
		{
			pShaderEffect->SetDiffuseTexture(pLookupTex, component);
		}

		grcTexture* pLookupPalTex = pLookupTxd->Lookup(palTexHash);
		if (pLookupPalTex)
		{
			pShaderEffect->SetDiffuseTexturePal(pLookupPalTex, component);
			pShaderEffect->SetDiffuseTexturePalSelector(paletteId, component, false);
		}

		grcTexture*	pLookupNorm = pLookupTxd->Lookup(normHash);
		if (pLookupNorm)
		{
			pShaderEffect->SetNormalTexture(pLookupNorm, component);
		}

		grcTexture*	pLookupSpec = pLookupTxd->Lookup(specHash);
		if (pLookupSpec)
		{
			pShaderEffect->SetSpecularTexture(pLookupSpec, component);
		}
	}
}

// replacement for specific sprintf calls below
void	CPedVariationPack::BuildComponentName(char* pString, u32 slotIdx, u32 drawblIdx, u8 drawblAltIdx, char suffix, bool hd){
	// this is more messy, but it's a damn sight faster
	
	Assert(pString);
	u32 pos = 0;

	for(u32 i=0;i<4;i++,pos++){
		pString[i] = varSlotNames[slotIdx][i];
	}

	pString[pos++] = '_';

	for(u32 i=7;i>=5;i--,pos++){
		pString[i] = ('0' + (char)(drawblIdx%10));
		drawblIdx = drawblIdx - (drawblIdx%10);
		drawblIdx = drawblIdx / 10;
	}

	pString[pos++] = '_';
	pString[pos++] = suffix;

	if (drawblAltIdx > 0)
	{
		pString[pos++] = '_';

		for(u32 i=12;i>=11;i--,pos++){
			pString[i] = ('0' + (char)(drawblAltIdx%10));
			drawblAltIdx = drawblAltIdx - (drawblAltIdx%10);
			drawblAltIdx = drawblAltIdx / 10;
		}
	}

	if (hd)
	{
		pString[pos++] = '_';
		pString[pos++] = 'h';
		pString[pos++] = 'i';
	}

	pString[pos++] = '\0';

	Assertf(strlen(pString) == pos-1, "Component name length (%" SIZETFMT "d) not what expected (%d)", strlen(pString), pos-1);
}


// build the correct name and extract the component. If we already have a stored hash from last time,
// then use that instead!
rmcDrawable* CPedVariationPack::ExtractComponent(const Dwd* pDwd, ePedVarComp slotIdx, u32 drawblIdx,
														u32* pDrawblHash, u8 drawblAltIdx)
{
	char			modelName[40];
	rmcDrawable		*pDrawable = NULL;
	u32			drawblHash = 0;

	Assert(pDrawblHash);

	//need to generate the name of the desired drawable from slot and index
	if (*pDrawblHash == 0){
		//sprintf(modelName,"%s_%03d_U",varSlotNames[slotIdx],drawblIdx);
		BuildComponentName(modelName, slotIdx, drawblIdx, drawblAltIdx, 'U', false);
		// look up the component model from the dictionary
		drawblHash = Dwd::ComputeHash(modelName);
		pDrawable = pDwd->Lookup(drawblHash);

		if (!pDrawable){
			//need to generate the name of the desired drawable from slot and index
			//sprintf(modelName,"%s_%03d_R",varSlotNames[slotIdx],drawblIdx);
			BuildComponentName(modelName, slotIdx, drawblIdx, drawblAltIdx, 'R', false);
			// look up the component model from the dictionary
			drawblHash = Dwd::ComputeHash(modelName);
			pDrawable = pDwd->Lookup(drawblHash);
		}

		if (!pDrawable){
			//need to generate the name of the desired drawable from slot and index
			//sprintf(modelName,"%s_%03d_M",varSlotNames[slotIdx],drawblIdx);
			BuildComponentName(modelName, slotIdx, drawblIdx, drawblAltIdx, 'M', false);
			// look up the component model from the dictionary
			drawblHash = Dwd::ComputeHash(modelName);
			pDrawable = pDwd->Lookup(drawblHash);
		}

		*pDrawblHash = drawblHash;		// store the hash for next time
	} else {
		// stored the hash from last time, so go and get it directly
		pDrawable = pDwd->Lookup(*pDrawblHash);
	}

	return(pDrawable);
}


void CPedVariationPack::RenderShaderSetup(CPedModelInfo* pModelInfo, CPedVariationData& varData, CCustomShaderEffectPed *pShaderEffect)
{
	varData.SetIsShaderSetup(true);

	// remap components (but not for player - handled elsewhere)
	if (pModelInfo->GetVarInfo()) {

		//Assertf(CheckVariation(pPed), "%s:Ped is set with conflicting race textures", pModelInfo->GetModelName());

		// want to remap the textures on each of the component drawables (and use drawableDict txd)
		strLocalIndex DwdIndex = strLocalIndex(pModelInfo->GetPedComponentFileIndex());
		u32		txdIndex = g_DwdStore.GetParentTxdForSlot(DwdIndex).Get();
		u32		drawblIdx = 0;
		u32		texId = 0;
		char		texVariation = 'a';
		ePVRaceType	raceId = PV_RACE_UNIVERSAL;

		// set the texture accordingly for all of the current components for this ped
		// get current head component

		// do the remapping for all components of this model
		for(ePedVarComp compId=PV_COMP_HEAD;compId<PV_MAX_COMP;compId=ePedVarComp(compId+1)){
			drawblIdx = varData.m_aDrawblId[compId];

			if (drawblIdx == PV_NULL_DRAWBL)
			{
				Assert(compId != PV_COMP_HEAD);
				continue;
			}

			{
				CPVDrawblData* pDrawblData = pModelInfo->GetVarInfo()->GetCollectionDrawable(compId, drawblIdx);
				if ( pDrawblData && pDrawblData->HasActiveDrawbl()){

					texId =  varData.m_aTexId[compId];
					texVariation = 'a' + static_cast<char>(texId);
					raceId = pModelInfo->GetVarInfo()->GetRaceType(compId,drawblIdx,texId,pModelInfo);

					if (pDrawblData->IsProxyTex()){			// proxy tex means picking the textures from variation 000 only
							drawblIdx = 0;
					}

                    if (pShaderEffect)
                        pShaderEffect->ClearTextures(compId);
					SetModelShaderTex(pShaderEffect, txdIndex, compId, drawblIdx, texVariation, raceId, varData.m_aPaletteId[compId], pModelInfo);
				}
			}
		}
	}
}


// determine if blendshapes have been set up on the given drawable or not
bool CPedVariationPack::IsBlendshapesSetup(rmcDrawable* pDrawable){

	// kind of fiddly to determine if blendshapes set up or not...
	grmModel* pModel = pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0);
	Assert(pModel);
	for(s32 n=0; n < pModel->GetGeometryCount(); n++)
	{
		// I'm assuming that I don't need to check further than first LOD & model for player head
		int geomType = pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(n).GetType();
		if (geomType==grmGeometry::GEOMETRYEDGE)
		{
			// something to do here - KS
		}
		else if (geomType==grmGeometry::GEOMETRYQB)
		{
			grmGeometryQB& geom = (grmGeometryQB&)(pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(n));
			if (geom.HasOffsets())
			{
				return(true);
			}
		}
	}
	return(false);
}

#if __BANK
extern bool bShowRenderPedLOD;
extern bool bShowMissingLowLOD;

void DrawPedLODLevel(const Matrix34& matrix, s32 lodLevel, bool bIsSlod, bool bIsStreamed)
{
	if (bShowRenderPedLOD){
		char LODLevel[4] = "X";
		if (bIsStreamed) {
			if (bIsSlod) {
				LODLevel[0] = 'S';
				LODLevel[1] = 'L';
			} else if (lodLevel == 0){
				LODLevel[0] = 'S';
				LODLevel[1] = '0';
			} else if (lodLevel == 1){
				LODLevel[0] = 'S';
				LODLevel[1] = '1';
			} else if (lodLevel == 2){
				LODLevel[0] = 'S';
				LODLevel[1] = '2';
			}
		} else if (bIsSlod) {
			LODLevel[0] = 'L';
		} else if (lodLevel == 0){
			LODLevel[0] = '0';
		} else if (lodLevel == 1){
			LODLevel[0] = '1';
		} else if (lodLevel == 2){
			LODLevel[0] = '2';
		}
		grcDebugDraw::Text(matrix.d + Vector3(0.f,0.f,1.3f), CRGBA(255, 255, 128, 255), LODLevel);
	}	
}
#else //__BANK
void DrawPedLODLevel(const Matrix34&, s32, bool, bool) {}

#endif //__BANK

// render a ped without the CPed ptr
void CPedVariationPack::RenderPed(CPedModelInfo *pModelInfo, const Matrix34& rootMatrix, const grmMatrixSet *pMatrixSet, CPedVariationData& varData,
						CCustomShaderEffectPed *pShaderEffect, u32 bucketMask,  u32 lodLevel,
						eRenderMode HAS_RENDER_MODE_SHADOWS_ONLY(renderMode), bool bUseBlendShapeTechnique, bool bAddToMotionBlurMask, bool bIsFPSSwimming)
{
	s32			drawblIdx = 0;
	rmcDrawable*	pDrawable = NULL;
	Dwd*		pDwd = NULL;
	s32			j;

	Assert(pModelInfo);
	Assert(pModelInfo->GetVarInfo());


	//const bool bAllowCutscenePeds = true; //PSN: fixes ig_Vlad's expensive hair (when no commandline available)

	// shader hasn't been setup for this vardata
	Assert(varData.IsShaderSetup());

	const bool bRenderHiLOD = (lodLevel == 0);

	const bool bDrawWithShaders = true HAS_RENDER_MODE_SHADOWS_ONLY(&& !IsRenderingShadows(renderMode));
	const bool bDrawSkinned = false HAS_RENDER_MODE_SHADOWS_ONLY(|| IsRenderingShadowsSkinned(renderMode));
#if __BANK
	u16 drawableStats = DRAWLISTMGR->GetDrawableStatContext();
#else
	u16 drawableStats = 0;
#endif

	const bool mirrorPhase = DRAWLISTMGR->IsExecutingMirrorReflectionDrawList();
	const bool shadowPhase = DRAWLISTMGR->IsExecutingShadowDrawList();
	const bool globalReflPhase = DRAWLISTMGR->IsExecutingGlobalReflectionDrawList();
	const bool waterReflPhase = DRAWLISTMGR->IsExecutingWaterReflectionDrawList();
	bool allowStripHead = !mirrorPhase && !shadowPhase && !globalReflPhase && (!waterReflPhase || bIsFPSSwimming);

	const bool bIsPlayerModel = pModelInfo->GetIsPlayerModel();

	const grmMatrixSet* overrideMs = pMatrixSet;
	bool bOverrideMtxSet = dlCmdAddCompositeSkeleton::IsCurrentMatrixSetOverriden();
	bool bOverrideRootMtxOnly = dlCmdAddCompositeSkeleton::IsCurrentRootMatrixOverriden();

	Matrix34 currentRootMatrix(rootMatrix);
	if (bOverrideMtxSet)
	{
		Mat34V newRootMtx;
		dlCmdAddCompositeSkeleton::GetCurrentMatrixSetOverride(overrideMs, newRootMtx);
		currentRootMatrix = RCC_MATRIX34(newRootMtx);
	}
	else if (bOverrideRootMtxOnly)
	{
		Mat34V newRootMtx;
		dlCmdAddCompositeSkeleton::GetCurrentRootMatrixOverride(newRootMtx);
		currentRootMatrix = RCC_MATRIX34(newRootMtx);
	}

	u32 requestedLod = lodLevel;
	bool bSetShaderVarCaching = false;
	CCustomShaderEffectPed::EnableShaderVarCaching(false);
	if (pModelInfo->GetVarInfo())
	{
		// go through all the component slots and draw the component for this ped
		for(j=0;j<PV_MAX_COMP;j++)
		{
			drawblIdx = varData.m_aDrawblId[j];

			if (drawblIdx == PV_NULL_DRAWBL)
			{
				Assert(j != PV_COMP_HEAD);
				continue;
			}

            if (allowStripHead && dlCmdAddSkeleton::GetIsStrippedHead() && (j == PV_COMP_HEAD || j == PV_COMP_TEEF || j == PV_COMP_HAIR))
                continue;

			// skip high detail components if we don't render high detail lod
			if (!bRenderHiLOD && varData.HasComponentHiLOD((u8)drawblIdx))
			{
				continue;
			}

			{
				CPVDrawblData* pDrawblData = pModelInfo->GetVarInfo()->GetCollectionDrawable(j, drawblIdx);
				if (pDrawblData && (pDrawblData->HasActiveDrawbl()))
				{
					// get Dwd through the pedModelInfo
					pDwd = g_DwdStore.Get(strLocalIndex(pModelInfo->GetPedComponentFileIndex()));
					pDrawable = ExtractComponent(pDwd, static_cast<ePedVarComp>(j), drawblIdx,  &varData.m_aDrawblHash[j], varData.m_aDrawblAltId[j]);

					// render the drawable (if it exists for this ped)
					if (pDrawable)
					{
						Matrix34 *parentMatrix = NULL;
	#if __XENON
	 					rmcLodGroup &group = pDrawable->GetLodGroup();
	 					grmModel* model = NULL;
						if (Verifyf(pDrawable->GetLodGroup().ContainsLod(LOD_HIGH), "No lod 0 for slot %d, drawable %d for %s", j, drawblIdx, pModelInfo->GetModelName()))
						{
							model = &group.GetLodModel0(LOD_HIGH);
						}
	#endif
						characterCloth* pCCloth = NULL;
						clothVariationData* clothVarData = varData.GetClothData(static_cast<ePedVarComp>(j));
						if( bRenderHiLOD && clothVarData )
						{
							const int bufferIdx = clothManager::GetBufferIndex(true);
							parentMatrix = (Matrix34*)clothVarData->GetClothParentMatrixPtr(bufferIdx);
							pCCloth = (characterCloth*)clothVarData->GetCloth();
							pCCloth->SetActive(true);
	#if __XENON
							if (model)
							{								
								grcVertexBuffer* clothVtxBuffer = clothVarData->GetClothVtxBuffer(bufferIdx);
		 						Assert( clothVtxBuffer );
		 						// Don't check the length of the secondary stream on cloth garments: they don't match, but it's on purpose.
		 						ASSERT_ONLY(grmGeometry::SetCheckSecondaryStreamVertexLength(false));
								for(int i=0;i<model->GetGeometryCount();i++)
								{
									model->GetGeometry(i).SetOffsets(clothVtxBuffer);
								}
							}
	#endif
	#if __PS3 && SPU_GCM_FIFO
							SPU_COMMAND(GTA4__SetCharacterClothData,clothVarData->GetClothNumVertices());
							cmd->morphData = (void*)clothVarData->GetClothVertices( clothManager::GetBufferIndex(true) );
	#endif // __PS3 && SPU_GCM_FIFO
	#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
							if ( clothVarData )
							{
								const int bufferIdx = clothManager::GetBufferIndex(true);
								pShaderEffect->PedVariationMgr_SetShaderClothVertices(pDrawable, clothVarData->GetClothNumVertices(), clothVarData->GetClothVertices(bufferIdx));
							}
	#endif// __WIN32PC || RSG_DURANGO || RSG_ORBIS
						}
						else if ( clothVarData )
						{
							clothVarData->GetCloth()->SetActive(false);
							clothVarData = NULL;
						}


						bool bForceBlendshapeShader = false;
						// if  we are drawing the models head with blendshapes attached then...
						s32 previousGroupId = grmShaderFx::GetForcedTechniqueGroupId();
						if ((j == PV_COMP_HEAD) && bDrawWithShaders && bUseBlendShapeTechnique) {
							if (m_BSTexGroupId != -1) {

								// kind of fiddly to determine if blendshapes set up or not...
								if (Verifyf(pDrawable->GetLodGroup().ContainsLod(LOD_HIGH), "No lod 0 for head drawable %d for %s", drawblIdx, pModelInfo->GetModelName()))
								{
									grmModel* pModel = pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0);
									Assert(pModel);
									for(s32 n=0; n < pModel->GetGeometryCount(); n++){
										// I'm assuming that I don't need to check further than first LOD & model for player head
										int geomType = pModel->GetGeometry(n).GetType();
										if (geomType==grmGeometry::GEOMETRYEDGE)
										{
											// nothing to do here - EDGE applies blendshapes
										}
										else if (geomType==grmGeometry::GEOMETRYQB)
										{
											grmGeometryQB& geom = (grmGeometryQB&)(pModel->GetGeometry(n));
											if (geom.HasOffsets()){
												grmShaderFx::SetForcedTechniqueGroupId(m_BSTexGroupId);  
												bForceBlendshapeShader = true;
												break;
											}
										}

									}
								}
							}
						}

						lodLevel = requestedLod;
						while (!pDrawable->GetLodGroup().ContainsLod(lodLevel) && lodLevel > 0)
						{
							lodLevel--;
						}

						const bool bRenderPresentHiLOD = (lodLevel == 0);

						bool containsLod = pDrawable->GetLodGroup().ContainsLod(lodLevel);
						if(pShaderEffect /*&& j!= PV_COMP_HAIR*/ && containsLod)
						{
							u32 flags = pModelInfo->GetVarInfo()->LookupCompInfoFlags(j, drawblIdx);
							int wetness = 0;
							if(flags & PV_FLAG_WET_MORE_WET)
								wetness = 1;
							else if(flags & PV_FLAG_WET_LESS_WET)
								wetness = -1;
							pShaderEffect->PedVariationMgr_SetWetnessAdjust(wetness);

	#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
							pShaderEffect->PedVariationMgr_SetShaderVariables(pDrawable, j, parentMatrix);
	#else // __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
							pShaderEffect->PedVariationMgr_SetShaderVariables(pDrawable, j);
	#endif // __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
							if(Unlikely(!bSetShaderVarCaching))
							{
								CCustomShaderEffectPed::EnableShaderVarCaching(true);
								bSetShaderVarCaching = true;
							}
	#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
							if ( clothVarData )
							{
								const int bufferIdx = clothManager::GetBufferIndex(true);
								pShaderEffect->PedVariationMgr_SetShaderClothVertices(pDrawable, clothVarData->GetClothNumVertices(), clothVarData->GetClothVertices(bufferIdx));
							}
	#endif //  __WIN32PC || RSG_DURANGO || RSG_ORBIS
						}

						if (!containsLod)
						{
							//nothing to draw!
						}
						else if(bDrawWithShaders)
						{		
							if(bAddToMotionBlurMask)
							{
								CShaderLib::BeginMotionBlurMask();
							}				

							bool bRenderExpensiveHair			= FALSE;
							bool bRenderExpensiveHairSpiked		= FALSE;
							bool bRenderExpensiveDitheredHair	= FALSE;
							bool bRenderExpensiveFur			= FALSE;
							bool bRenderHairCutout				= FALSE;
							bool bRenderHairNothing				= FALSE;

							if(bRenderPresentHiLOD)
							{
								if(DRAWLISTMGR->IsExecutingGBufDrawList() || DRAWLISTMGR->IsExecutingHudDrawList())	// expensive hair allowed only in GBuffer stage
								{
									if(/*(bUseBlendShapeTechnique || bAllowCutscenePeds) &&*/ ((j==PV_COMP_HAIR) || (j==PV_COMP_BERD) || (j==PV_COMP_ACCS)))
									{
										if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
										{
											bRenderExpensiveHairSpiked = TRUE;
										}
									}

									if(pShaderEffect && pShaderEffect->IsFurShader() && CRenderer::IsBaseBucket(bucketMask,CRenderer::RB_CUTOUT) && pDrawable->GetShaderGroup().LookupShader("ped_fur"))
									{
										bRenderExpensiveFur = TRUE;
									}
								}// if(gDrawListMgr->IsExecutingGBufDrawList())...
								// spiked hair allowed in DL_MIRROR_REFLECTION:
								else if(DRAWLISTMGR->IsExecutingMirrorReflectionDrawList())
								{
									if((j==PV_COMP_HAIR) || (j==PV_COMP_BERD) || (j==PV_COMP_ACCS))
									{
										if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
										{
											bRenderExpensiveHairSpiked = TRUE;
										}
									}
								}
								// spiked hair allowed in DL_WATER_REFLECTION, but only for peds:
								else if(bIsPlayerModel && DRAWLISTMGR->IsExecutingWaterReflectionDrawList())
								{
									if((j==PV_COMP_HAIR) || (j==PV_COMP_BERD) || (j==PV_COMP_ACCS))
									{
										if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
										{
											bRenderExpensiveHairSpiked = TRUE;
										}
									}
								}
								// spiked hair allowed in DL_SHADOWS and DL_HUD stage
								else if(DRAWLISTMGR->IsExecutingShadowDrawList())
								{
									if(j==PV_COMP_HAIR)
									{
										if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
										{
											bRenderExpensiveHairSpiked = TRUE;
										}
									}
									else if((j==PV_COMP_BERD) || (j==PV_COMP_ACCS))
									{
										if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
										{
											bRenderHairNothing = TRUE;
										}
									}
								}// if(gDrawListMgr->IsExecutingShadow)...

								// hair cutout is always rendered, regardless of renderphase:
								if((j==PV_COMP_HAIR) || (j==PV_COMP_BERD) || (j==PV_COMP_ACCS))
								{
									if(pDrawable->GetShaderGroup().LookupShader("ped_hair_cutout_alpha") || pDrawable->GetShaderGroup().LookupShader("ped_hair_cutout_alpha_cloth"))
									{
										bRenderHairCutout = TRUE;
									}
								}
							}// if(bRenderHiLOD)...

							if(bRenderHairNothing)
							{
								// render nothing
							}
							else if(bRenderExpensiveHair)
							{
								CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *overrideMs, bucketMask, lodLevel, CShaderHairSort::HAIRSORT_ORDERED);
							}
							else if(bRenderExpensiveHairSpiked)
							{
								CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *overrideMs, bucketMask, lodLevel, CShaderHairSort::HAIRSORT_SPIKED);
							}
							else if(bRenderHairCutout)
							{
								CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *overrideMs, bucketMask, lodLevel, CShaderHairSort::HAIRSORT_CUTOUT);
							}
							else if(bRenderExpensiveFur)
							{
								Vector3 wind;
								pShaderEffect->GetLocalWindVec(wind);
								CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *overrideMs, bucketMask, lodLevel, CShaderHairSort::HAIRSORT_FUR, &wind);
							}
							else if(bRenderExpensiveDitheredHair)
							{
								CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *overrideMs, bucketMask, lodLevel, CShaderHairSort::HAIRSORT_LONGHAIR);
							}
							else
							{
								DrawPedLODLevel(currentRootMatrix, lodLevel, false, false);
								pDrawable->DrawSkinned(currentRootMatrix, *overrideMs, bucketMask, lodLevel, drawableStats);
							}


							if(bAddToMotionBlurMask)
							{
								CShaderLib::EndMotionBlurMask();
							}
						}
						else
						{
							rmcDrawable::EnumNoShadersSkinned renderOnly;

							if (bDrawSkinned) { 
								renderOnly = rmcDrawable::RENDER_SKINNED;
							} else {
								renderOnly = rmcDrawable::RENDER_NONSKINNED;
							}

							DrawPedLODLevel(currentRootMatrix, lodLevel, false, false);
							pDrawable->DrawSkinnedNoShaders(currentRootMatrix, *overrideMs, bucketMask, lodLevel, renderOnly);
						}

						if(containsLod && pShaderEffect)
						{
							pShaderEffect->ClearShaderVars(pDrawable, j);
						}

						if (bForceBlendshapeShader)
						{
							grmShaderFx::SetForcedTechniqueGroupId(previousGroupId);
						}

						if( pCCloth )
						{
	#if __XENON
							if (model)
							{
								for(int i=0;i<model->GetGeometryCount();i++)
								{
									model->GetGeometry(i).SetOffsets(NULL);
								}
								ASSERT_ONLY(grmGeometry::SetCheckSecondaryStreamVertexLength(true));
							}
	#endif // __XENON
	#if __PS3 && SPU_GCM_FIFO
							SPU_COMMAND(GTA4__SetCharacterClothData,0);
							cmd->morphData = (void*)NULL;
	#endif // __PS3 && SPU_GCM_FIFO
	#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
							if ( clothVarData )
							{
								const int bufferIdx = clothManager::GetBufferIndex(true);
								pShaderEffect->PedVariationMgr_SetShaderClothVertices(pDrawable, clothVarData->GetClothNumVertices(), clothVarData->GetClothVertices(bufferIdx));
							}
	#endif //  __WIN32PC || RSG_DURANGO || RSG_ORBIS
						}

					}
				}
			}
		}
	}
	CCustomShaderEffectPed::EnableShaderVarCaching(false);
	pShaderEffect->ClearGlobalShaderVars();
}

// render a ped without the CPed ptr
void CPedVariationPack::RenderSLODPed(const Matrix34& rootMatrix, const grmMatrixSet *pMatrixSet, u32 bucketMask, eRenderMode HAS_RENDER_MODE_SHADOWS_ONLY(renderMode), CCustomShaderEffectBase* shaderEffect, eSuperlodType slodType)
{
	//s32			drawblIdx = 0;
	rmcDrawable*	pDrawable = NULL;
	Dwd*		pDwd = NULL;
	//s32			j;

	CPedModelInfo* slodMI = NULL;
	switch (slodType)
	{
	case SLOD_HUMAN:
		Assert(ms_pPedSlodHumanMI);
		slodMI = ms_pPedSlodHumanMI;
		break;
	case SLOD_SMALL_QUADRUPED:
		Assert(ms_pPedSlodSmallQuadpedMI);
		slodMI = ms_pPedSlodSmallQuadpedMI;
		break;
	case SLOD_LARGE_QUADRUPED:
		Assert(ms_pPedSlodLargeQuadPedMI);
		slodMI = ms_pPedSlodLargeQuadPedMI;
		break;
	case SLOD_KEEP_LOWEST:
		// shouldn't end up here
		Assertf(false, "Ped with slod type SLOD_KEEP_LOWEST is trying to render a super lod!");
		return;
	case SLOD_NULL:
		// no slod, skip rendering
		return;
	default:
		Assertf(false, "Unknown ped superlod type %d", slodType);
	}

	const bool bDrawWithShaders = true HAS_RENDER_MODE_SHADOWS_ONLY(&& !IsRenderingShadows(renderMode));
	const bool bDrawSkinned = false HAS_RENDER_MODE_SHADOWS_ONLY(|| IsRenderingShadowsSkinned(renderMode));

#if __BANK
	// TODO -- use m_commonData from drawable datastructs?
	u16 drawableStats = DRAWLISTMGR->GetDrawableStatContext();
#else
	u16 drawableStats = 0;
#endif

	if(slodMI->GetVarInfo())
	{
		if (slodMI->GetVarInfo()->IsValidDrawbl(PV_COMP_UPPR, 0))
		{
			// get Dwd through the pedModelInfo
			pDwd = g_DwdStore.Get(strLocalIndex(slodMI->GetPedComponentFileIndex()));
			static u32 cachedHash = 0;
			pDrawable = ExtractComponent(pDwd, static_cast<ePedVarComp>(PV_COMP_UPPR), 0,  &cachedHash, 0);

			PGHANDLE_REF_COUNT_ONLY(bool needToClearTex = false;)
			if (shaderEffect && bDrawWithShaders)
			{
				CCustomShaderEffectPed* pedShaderEffect = static_cast<CCustomShaderEffectPed*>(shaderEffect);
				if (pedShaderEffect)
				{
					if (lastSlodDrawable != pDrawable)
					{
						lastVarDiffText = pDrawable->GetShaderGroup().LookupVar("DiffuseTex", false);
						lastSlodDrawable = pDrawable;
					}

					if (lastVarDiffText != grmsgvNONE)
					{
						pDrawable->GetShaderGroup().SetVar(lastVarDiffText, pedShaderEffect->GetDiffuseTexture(PV_COMP_UPPR));
						PGHANDLE_REF_COUNT_ONLY(needToClearTex = true;)
					}
#if 0
					grmShaderGroupVar varDiffTexPal = pDrawable->GetShaderGroup().LookupVar("DiffuseTexPal", false);
					if (varDiffTexPal != grmsgvNONE)
					{
						pDrawable->GetShaderGroup().SetVar(varDiffTexPal, pedShaderEffect->GetDiffuseTexturePal(PV_COMP_UPPR));
					}

					grmShaderGroupVar varDiffTexPalSel = pDrawable->GetShaderGroup().LookupVar("DiffuseTexPaletteSelector", false);
					if (varDiffTexPalSel != grmsgvNONE)
					{
						pDrawable->GetShaderGroup().SetVar(varDiffTexPalSel, pedShaderEffect->GetDiffuseTexturePalSelector(PV_COMP_UPPR));
					}
#endif
				}
			}

			if (bDrawWithShaders){
				DrawPedLODLevel(rootMatrix, 0, true, false);
				pDrawable->DrawSkinned(rootMatrix, *pMatrixSet, bucketMask, 0, drawableStats);
			} else {
				rmcDrawable::EnumNoShadersSkinned renderOnly;

				if (bDrawSkinned) { 
					renderOnly = rmcDrawable::RENDER_SKINNED;
				} else {
					renderOnly = rmcDrawable::RENDER_NONSKINNED;
				}
				DrawPedLODLevel(rootMatrix, 0, true, false);
				pDrawable->DrawSkinnedNoShaders(rootMatrix, *pMatrixSet, bucketMask, 0, renderOnly);
			}

#			if PGHANDLE_REF_COUNT
				if (needToClearTex)
				{
					// While it is safe to leave dangling pgHandles inside of
					// slodMI, since they will always be set correctly before
					// rendering again, NULL out the handles to remove false
					// positives in the pgHandle checking code.
					pDrawable->GetShaderGroup().SetVar(lastVarDiffText, (grcTexture*)NULL);
				}
#			endif
		}
	}
}

// set the variation by cycling through all possible combinations (each call sets the ped to the next combination)
void	CPedVariationPack::SetVarCycle(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData)
{
	Assert(pDynamicEntity);

	CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
		return;

	u32	maxTexIdx = 0;
	u32	maxDrawblIdx = 0;
	u32  maxComponentIdx = static_cast<u32>(PV_MAX_COMP-1);

	// iterate through all possible combinations
	pedVarData.m_aTexId[0]++;											//next in cycle...
	for (u32 i=0;i<maxComponentIdx;i++)			//handle overflows...
	{
		if (pedVarData.m_aDrawblId[i] == PV_NULL_DRAWBL)
		{
			Assert(i != PV_COMP_HEAD);
			pedVarData.m_aDrawblId[i] = 0;
		}

		maxTexIdx = pVarInfo->GetMaxNumTex(static_cast<ePedVarComp>(i), pedVarData.m_aDrawblId[i]);
		maxDrawblIdx = pVarInfo->GetMaxNumDrawbls(static_cast<ePedVarComp>(i));

		if (pedVarData.m_aTexId[i] >= maxTexIdx)
		{
			pedVarData.m_aTexId[i] = 0;
			pedVarData.m_aDrawblId[i]++;

			if (pedVarData.m_aDrawblId[i] >= maxDrawblIdx)
			{
				pedVarData.m_aDrawblId[i] = 0;
				if (i < (maxComponentIdx-1)) {
					pedVarData.m_aTexId[i+1]++;		// overflow onto next component
				} else {
												// should reset...
				}
			}
		}
	}

	UpdatePedForAlphaComponents(pDynamicEntity, pedPropData, pedVarData);
}

//	Set all components to 0
void	CPedVariationPack::SetVarDefault(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData)
{
	Assert(pDynamicEntity);

	CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	if (!Verifyf(pModelInfo->GetVarInfo(), "Missing meta data on ped '%s'?", pModelInfo->GetModelName()))
            return;

	if (pModelInfo->GetVarInfo()->IsSuperLod()){
		return;
	}

	if(pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pDynamicEntity);
		for(u32 lod = 0; lod < LOD_COUNT; lod++)
		{
			pPed->SetLodBucketInfoExists(lod, false);
			pPed->SetLodHasAlpha(lod, false);
			pPed->SetLodHasDecal(lod, false);
			pPed->SetLodHasCutout(lod, false);
		}
	}

	for(s32 i = 0; i<PV_MAX_COMP; i++)
	{
		SetVariation(pDynamicEntity, pedPropData, pedVarData, static_cast<ePedVarComp>(i), 0, 0, 0, 0);
	}

	UpdatePedForAlphaComponents(pDynamicEntity, pedPropData, pedVarData);
}

// Takes a global component index and converts it to the variationInfo dlcNameHash and local index of the component
void CPedVariationPack::GetLocalCompData(const CPed* pPed, ePedVarComp compId, u32 compIndx, u32& outVarInfoHash, u32& outLocalIndex)
{
	outVarInfoHash = 0;
	outLocalIndex = 0;

	if (pPed && pPed->GetBaseModelInfo())
	{
		if (CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo()))
		{
			if (CPedVariationInfoCollection* pVarInfoColl = pPedModelInfo->GetVarInfo())
			{
				if (CPedVariationInfo* varInfo = pVarInfoColl->GetVariationInfoFromDrawableIdx((ePedVarComp)compId, compIndx))
				{
					outVarInfoHash = varInfo->GetDlcNameHash();
					outLocalIndex = pVarInfoColl->GetDlcDrawableIdx((ePedVarComp)compId, compIndx);
				}
			}
		}
	}
}

// Takes a local component index and converts it to global
u32 CPedVariationPack::GetGlobalCompData(const CPed* pPed, ePedVarComp compId, u32 varInfoHash, u32 localIndex)
{
	u32 compIdx = 0;

	if (CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo()))
	{
		if (CPedVariationInfoCollection* pVarInfoColl = pPedModelInfo->GetVarInfo())
		{
			// If the variation exists globalise the index here, otherwise leave it as default.
			if (pVarInfoColl->GetVariationInfoFromDLCName(varInfoHash) != NULL)
				compIdx = pVarInfoColl->GetGlobalDrawableIndex(localIndex, compId, varInfoHash);
			else
			{
				Assertf(varInfoHash == 0, 
					"CPedVariationPack::GetGlobalCompData - No CPedVariationInfo found for non-zero hash! [%u/%s] [%s]", varInfoHash, atHashString(varInfoHash).TryGetCStr(), pPedModelInfo->GetModelName());
			}
		}
	}

	return compIdx;
}

u8 CPedVariationPack::GetPaletteVar(const CPed* pPed, ePedVarComp slotId)
{
	Assert(pPed);
	return(pPed->GetPedDrawHandler().GetVarData().GetPedPaletteIdx(slotId));
}

u8 CPedVariationPack::GetTexVar(const CPed* pPed, ePedVarComp slotId)
{
	Assert(pPed);
	return(pPed->GetPedDrawHandler().GetVarData().GetPedTexIdx(slotId));
}

u32 CPedVariationPack::GetCompVar(const CPed* pPed, ePedVarComp slotId)
{
	Assert(pPed);	
	return(pPed->GetPedDrawHandler().GetVarData().GetPedCompIdx(slotId));
}

// Return if ped has the variation
bool CPedVariationPack::HasVariation(CPed* pPed, ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId)
{
	Assert(pPed);

	const CPedVariationData& varData = pPed->GetPedDrawHandler().GetVarData();

	return (varData.GetPedCompIdx(slotId) == drawblId && varData.GetPedCompAltIdx(slotId) == drawblAltId && varData.GetPedTexIdx(slotId) == texId);
}

bool 
CPedVariationPack::IsVariationValid(CDynamicEntity* pDynamicEntity, 
								   ePedVarComp componentId, 
								   s32 drawableId, 
								   s32 textureId)
{
	Assert(pDynamicEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	if (!Verifyf(pVarInfo, "No variation info on ped '%s'", pDynamicEntity->GetModelName()))
        return false;

	if ( drawableId >= pVarInfo->GetMaxNumDrawbls(componentId) || textureId >= pVarInfo->GetMaxNumTex(componentId, drawableId) )
	{
		return false;
	}

	if (!pVarInfo->IsValidDrawbl(componentId, drawableId))
	{
		return false;
	}

	// check texture restrictions
	CPed* pPed = (CPed*)pDynamicEntity;
	Assert(pPed);

	ePVRaceType componentRaceTypes[PV_MAX_COMP] = {PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL,
													PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL,
													PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL};

	// grab the race types for all the textures used on this ped
	for (u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		u32	drawableIdx = pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i];

		if (drawableIdx == PV_NULL_DRAWBL)
		{
			Assert(i != PV_COMP_HEAD);
			continue;
		}

		if (pVarInfo->IsValidDrawbl(i, drawableIdx))
		{
			u32	texIdx = pPed->GetPedDrawHandler().GetVarData().m_aTexId[i];
			componentRaceTypes[i] = pVarInfo->GetRaceType(i, drawableIdx, texIdx, pModelInfo);
		}
	}

	// get race type for new requested setting
	ePVRaceType newRaceType = pVarInfo->GetRaceType(componentId, drawableId, textureId, pModelInfo);

	// check if new setting conflicts with what's already on the ped
	for (u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		// skip the component we'll overwrite since it doesn't matter if that one conflicts
		if (i == (u32)componentId)
			continue;

		if (!CPedVariationInfo::IsRaceCompatible(componentRaceTypes[i], newRaceType))
			return false;
	}

	return true;
}

const crExpressions*	CPedVariationPack::GetExpressionForComponent(CPed* ped, ePedVarComp component,crExpressionsDictionary& defaultDictionary, s32 f)
{
	const crExpressions* expr = NULL;
	u32 dlcHash =0;
	CPedVariationData& pedVarData = ped->GetPedDrawHandler().GetVarData();
	char compName[RAGE_MAX_PATH];
	CPedVariationInfoCollection* pVarInfoColl = NULL;
	if (CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(ped->GetBaseModelInfo()))
	{
		pVarInfoColl = pPedModelInfo->GetVarInfo();
		dlcHash = pVarInfoColl->GetDlcNameHashFromDrawableIdx(component,pedVarData.GetPedCompIdx(component));
	}
	if(dlcHash==0)
	{
		CPedVariationPack::BuildComponentName(compName, component, pedVarData.GetPedCompIdx(component), pedVarData.GetPedCompAltIdx(component), pedVarData.HasRacialTexture(ped, component)?'r':'u', f == 1 /*GetIsCurrentlyHD()*/);
		expr = defaultDictionary.Lookup(compName);
	}
	else
	{
		if(pVarInfoColl->GetVariationInfoFromDrawableIdx(component,pedVarData.GetPedCompIdx(component)))
		{
			s32 localIndex = pVarInfoColl->GetDlcDrawableIdx(component,pedVarData.GetPedCompIdx(component));
			strLocalIndex slot = g_ExpressionDictionaryStore.FindSlotFromHashKey(dlcHash);
			if(slot.IsValid())
			{
				CPedVariationPack::BuildComponentName(compName, component,localIndex , pedVarData.GetPedCompAltIdx(component), pedVarData.HasRacialTexture(ped, component)?'r':'u', f == 1 /*GetIsCurrentlyHD()*/);
				expr = g_ExpressionDictionaryStore.Get(slot)->Lookup(compName);
			}
		}				
	}
	return expr;
}

// set the component part of the given ped to use the desired drawable and desired texture
// return false if unable to set the variation
bool	CPedVariationPack::SetVariation(CDynamicEntity* pDynamicEntity,  CPedPropData& pedPropData, CPedVariationData& pedVarData, 
									   ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId)
{
	Assert(pDynamicEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);
	Assert(!pModelInfo->GetIsStreamedGfx());

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	Assert(pVarInfo);

	if (!pVarInfo)
	{
		return false;
	}

	CPVDrawblData* pDrawblData = NULL;
	if (drawblId != PV_NULL_DRAWBL)
	{
		pDrawblData = pVarInfo->GetCollectionDrawable(slotId, drawblId);
		if( !pDrawblData || !pDrawblData->HasActiveDrawbl() )
		{
			return false;
		}
	}

	Assertf(drawblId != PV_NULL_DRAWBL || slotId != PV_COMP_HEAD, "Empty drawable cannot be set on head component! (%s)", pDynamicEntity->GetModelName());

	Assertf(drawblId == PV_NULL_DRAWBL || (drawblId <  pVarInfo->GetMaxNumDrawbls(slotId)) && texId < pVarInfo->GetMaxNumTex(slotId, drawblId), 
		"Invalid variation: %s : %s_%3d_%c", pModelInfo->GetModelName(), varSlotNames[slotId], drawblId, 'a' + texId);


	Assertf(drawblId == PV_NULL_DRAWBL || drawblId < pVarInfo->GetMaxNumDrawbls(slotId), " DrawableId(%d) > (%d)MaxNumDrawbls || Invalid variation: %s : %s_%3d_%c", 
																 drawblId, 
																 pVarInfo->GetMaxNumDrawbls(slotId), 
																 pModelInfo->GetModelName(), 
																 varSlotNames[slotId], 
																 drawblId, 
																 'a' + texId);

	Assertf(drawblId == PV_NULL_DRAWBL || texId < pVarInfo->GetMaxNumTex(slotId, drawblId), " texId(%d) > (%d)MaxNumTex || Invalid variation: %s : %s_%3d_%c", 
																	texId, 
																	pVarInfo->GetMaxNumTex(slotId, drawblId), 
																	pModelInfo->GetModelName(), 
																	varSlotNames[slotId], 
																	drawblId, 
																	'a' + texId);

	Assertf(drawblId == PV_NULL_DRAWBL || drawblAltId <= pDrawblData->GetNumAlternatives(), " drawblAltId(%d) > (%d)NumAlternatives || Invalid variation: %s : %s_%3d_%c", 
																	drawblAltId, 
																	pDrawblData->GetNumAlternatives(), 
																	pModelInfo->GetModelName(), 
																	varSlotNames[slotId], 
																	drawblId, 
																	'a' + texId);
	

	clothVariationData* clothVarData = NULL;

	Assert(drawblId == PV_NULL_DRAWBL || drawblId < MAX_DRAWBL_VARS); 

	pedVarData.RemoveCloth(slotId);

	CPed* pPed = (CPed*)pDynamicEntity;
	Assert( pPed );	
	pPed->m_CClothController[slotId] = NULL;
	
	if( pDrawblData && pDrawblData->m_clothData.HasCloth() )
	{
#if CREATE_CLOTH_ONLY_IF_RENDERED
		pedVarData.SetPedSkeleton(pPed->GetSkeleton());
		pedVarData.SetFlagCreateCloth(slotId);
#else
		clothVarData = CreateCloth( pDrawblData->m_clothData.m_charCloth, pPed->GetSkeleton(), pDrawblData->m_clothData.m_dictSlotId );
#endif


#if HAS_RENDER_MODE_SHADOWS
 		// We have to do the shadows differently for clothed peds...
 		pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
#endif // HAS_RENDER_MODE_SHADOWS
	}

	if(pPed->IsLocalPlayer())
	{
	#if HAS_RENDER_MODE_SHADOWS
 		// force custom shadows for player ped (using um, etc.)
 		pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
	#endif // HAS_RENDER_MODE_SHADOWS
	}

	const CPedModelInfo* pPedMI = pPed->GetPedModelInfo();
	if(pPedMI && pPedMI->GetIsPlayerModel())
	{
	#if HAS_RENDER_MODE_SHADOWS
		// force custom shadows for all 3 player peds (using um, etc.)
 		pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
	#endif // HAS_RENDER_MODE_SHADOWS
	}


	pedVarData.SetPedVariation(slotId, drawblId, drawblAltId, texId, paletteId, clothVarData );
    if (pDrawblData)
        pDrawblData->IncrementUseCount(texId);

	Dwd* pDwd = g_DwdStore.Get(strLocalIndex(pModelInfo->GetPedComponentFileIndex()));
	if (!Verifyf(pDwd, "Missing dwd assets for ped '%s'", pModelInfo->GetModelName()))
        return false;

	rmcDrawable* pDrawable = ExtractComponent(pDwd, slotId, drawblId, &pedVarData.m_aDrawblHash[slotId], pedVarData.m_aDrawblAltId[slotId]);
	if (pDrawable)
	{
		// update render flags
		pDrawable->GetLodGroup().ComputeBucketMask(pDrawable->GetShaderGroup());

        pDrawblData->SetHasAlpha(false);
        pDrawblData->SetHasDecal(false);
        pDrawblData->SetHasCutout(false);

		// check if this ped has a decal decoration
		if (pDrawable && pDrawable->GetShaderGroup().LookupShader("ped_decal_decoration"))
		{
			pedVarData.m_bIsCurrVarDecalDecoration = true;
		}

		// update flags for LOD0 and LOD1:
		for (u32 lod=0; lod<2; lod++)
		{
			if(pDrawable->GetLodGroup().ContainsLod(lod))
			{
				pPed->SetLodBucketInfoExists(lod, true);

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_ALPHA))
				{
					pDrawblData->SetHasAlpha(true);
					pPed->SetLodHasAlpha(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_DECAL))
				{
					pDrawblData->SetHasDecal(true);
					pPed->SetLodHasDecal(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_CUTOUT))
				{
					pDrawblData->SetHasCutout(true);
					pPed->SetLodHasCutout(lod, true);
				}
			}
		}

		// update ped bucket mask bits only for LOD2/LOD3
		for (u32 lod=2; lod<LOD_COUNT; lod++)
		{
			if(pDrawable->GetLodGroup().ContainsLod(lod))
			{
				pPed->SetLodBucketInfoExists(lod, true);

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_ALPHA))
				{
					pPed->SetLodHasAlpha(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_DECAL))
				{
					pPed->SetLodHasDecal(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_CUTOUT))
				{
					pPed->SetLodHasCutout(lod, true);
				}
			}
		}
	}

	// was a large block here if CLONE_EFFECTS was disabled, but CLONE_EFFECTS
	// is effectively always available now

	UpdatePedForAlphaComponents(pDynamicEntity, pedPropData, pedVarData);


	// Support for having different ethnic origin heads in the same ped pack
	// Different ethnic origin heads have the same number of bones, the same hierarchy and the same boneids
	// But the facial bones may be in slightly different positions and of slightly different lengths
	// The skeletondata loaded with frag (body model) may not be correct for the given head model
	// So we change the skeletondata to use the skeletondata from the head model
	if (pDynamicEntity->GetIsTypePed() && (slotId == PV_COMP_HEAD))
	{
		rmcDrawable* pHeadComponent = CPedVariationPack::GetComponent(PV_COMP_HEAD, pDynamicEntity, pedVarData);
		if (pHeadComponent && pDynamicEntity->GetFragInst() && pDynamicEntity->GetFragInst()->GetCacheEntry())
		{
#if __ASSERT
			if (pDynamicEntity->GetSkeletonData().GetNumBones() != pHeadComponent->GetSkeletonData()->GetNumBones())
			{
				Displayf("         Entity skeleton\t\tHead skeleton");
				u32 numBones = rage::Max(pDynamicEntity->GetSkeletonData().GetNumBones(), pHeadComponent->GetSkeletonData()->GetNumBones());
				for (s32 i = 0; i < numBones; ++i)
				{
					bool a = i < pDynamicEntity->GetSkeletonData().GetNumBones();
					bool b = i < pHeadComponent->GetSkeletonData()->GetNumBones();
					Displayf("Bone %3d %s (0x%04x)\t\t%s (0x%04x)", i, a ? pDynamicEntity->GetSkeletonData().GetBoneData(i)->GetName() : "missing",
						a ? pDynamicEntity->GetSkeletonData().GetBoneData(i)->GetDofs() : 0,
						b ? pHeadComponent->GetSkeletonData()->GetBoneData(i)->GetName() : "missing",
						b ? pHeadComponent->GetSkeletonData()->GetBoneData(i)->GetDofs() : 0);
				}
			}

			artAssertf(pDynamicEntity->GetSkeletonData().GetNumBones() ==  pHeadComponent->GetSkeletonData()->GetNumBones(), 
				"Ped %s has mismatched number of bones in frag and head component %d. See tty for more info.", pModelInfo->GetModelName(), drawblId);
#endif // __ASSERT

			if (artVerifyf(pDynamicEntity->GetSkeletonData().GetSignature() ==  pHeadComponent->GetSkeletonData()->GetSignature(), 
				"Ped %s has mismatched skeleton signature in frag and head component %d (ie. it has different DOFs). See tty for more info.", pModelInfo->GetModelName(), drawblId))
			{
				// Dirty hack until we re-export the skeletondatas, make these bones animatable in translation
				pPed->SetupSkeletonData(const_cast<crSkeletonData&>(*pHeadComponent->GetSkeletonData()));

				pDynamicEntity->GetFragInst()->GetCacheEntry()->SetSkeletonData(*pHeadComponent->GetSkeletonData());
			}
		}
	}	

	RenderShaderSetup(pModelInfo, pedVarData, static_cast<CCustomShaderEffectPed*>(pDynamicEntity->GetDrawHandler().GetShaderEffect()));

	if(pPed && pPed->GetPedAudioEntity())
	{
		pPed->GetPedAudioEntity()->GetFootStepAudio().UpdateVariationData(slotId);
	}
	return(true);
}


clothVariationData* CPedVariationPack::CreateCloth(characterCloth* pCClothTemplate, const crSkeleton* skel, const s32 dictSlotId)
{
	// NOTE: there is ptr to the cloth, no need to query requests, slots in the store or anything like it, just grab the ptr
	Assert( pCClothTemplate );

//sysMemUseMemoryBucket b(MEMBUCKET_CLOTH);

	characterCloth* charCloth = rage_new characterCloth();
	Assert( charCloth );
	charCloth->InstanceFromTemplate( pCClothTemplate );		

	characterClothController* cccontroller = charCloth->GetClothController();
	Assert( cccontroller );
	Assert( cccontroller->GetCurrentCloth() );
	cccontroller->SetOwner( charCloth );
	cccontroller->ResetW();
	cccontroller->SetForcePin(1);

	clothManager* cManager = CPhysics::GetClothManager();
	Assert( cManager );
	cManager->AddCharacterCloth( cccontroller, skel );

	clothVariationData* clothVarData = cManager->AllocateVarData();
	Assert( clothVarData );

	const int numVerts = cccontroller->GetCurrentCloth()->GetNumVertices();
	clothVarData->Set( numVerts, charCloth, dictSlotId );
	const int bufferIdx = clothManager::GetBufferIndex();
	charCloth->SetEntityVerticesPtr( clothVarData->GetClothVertices(bufferIdx) );
	charCloth->SetEntityParentMtxPtr( clothVarData->GetClothParentMatrixPtr(bufferIdx) );

	cccontroller->SkinMesh( skel );

	const Mat34V* parentMtx = skel->GetParentMtx();
	Assert( parentMtx );
	charCloth->SetParentMtx( *parentMtx );

	for(int bufferIndex = 0; bufferIndex < grmGeometry::GetVertexMultiBufferCount(); bufferIndex++)
	{
		*(clothVarData->GetClothParentMatrixPtr(bufferIndex)) = *parentMtx;
	}

	for(int i = 0; i < NUM_CLOTH_BUFFERS; ++i)
	{
		Vec3V* RESTRICT verts = clothVarData->GetClothVertices(i);
		Assert( verts );
// NOTE: don't want to crash if the game run out of streaming memory 
		if( verts )
		{
			Vec3V* RESTRICT origVerts = cccontroller->GetCloth(0)->GetClothData().GetVertexPointer();
			Assert( origVerts );
			// origVerts should exists, if this fails there is bug somewhere in cloth instancing
			for(int j=0; j<numVerts; ++j)
			{
				verts[j] = origVerts[j];
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
				// clear out W channel to prevent NaNs in shader vars
				verts[j].SetW(MAGIC_ZERO);
#endif
			}
		}
	}

	gDrawListMgr->CharClothAdd( clothVarData );

	return clothVarData;
}


//
//
// determine if the current variation data for the ped is valid (doesn't contain any conflicting race textures)
//
bool CPedVariationPack::CheckVariation(CPed* pPed)
{
	Assert(pPed);

	CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	Assert(pModelInfo);

	ePVRaceType		componentRaceTypes[PV_MAX_COMP] = { PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL,
														PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL,};

	u32	drawableIdx = 0;
	u32	texIdx = 0;

	// grab the race types for all the textures used on this ped
	for(u32 i=0;i<PV_MAX_COMP;i++)
	{
		drawableIdx = pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i];

		if (drawableIdx == PV_NULL_DRAWBL)
		{
			Assert(i != PV_COMP_HEAD);
			continue;
		}

		if (pModelInfo->GetVarInfo()->IsValidDrawbl(i, drawableIdx))
		{
			texIdx = pPed->GetPedDrawHandler().GetVarData().m_aTexId[i];
			componentRaceTypes[i] = pModelInfo->GetVarInfo()->GetRaceType(i, drawableIdx, texIdx, pModelInfo);
		}
	}

	// check for incompatible textures in all the components
	for(u32 i=0; i<PV_MAX_COMP; i++) {
		for(u32 j=0; j<i; j++) {
			if (!CPedVariationInfo::IsRaceCompatible(componentRaceTypes[i], componentRaceTypes[j])) {
				return(false);			// non matching race types found
			}
		}
	}

	return(true);
}

bool CPedVariationPack::ApplySelectionSetByHash(CPed* ped, class CPedPropData& pedPropData, CPedVariationData& pedVarData, u32 setHash)
{
	Assert(ped);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(ped->GetBaseModelInfo());
	Assert(pModelInfo);

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
		return false;

	const CPedSelectionSet* selectionSet = pVarInfo->GetSelectionSetByHash(setHash);
	if (!Verifyf(selectionSet != NULL, "Selection set with hash %d wasn't found", setHash))
		return false;

	if ((ped->GetModelNameHash() == ATSTRINGHASH("mp_m_freemode_01", 0x705e61f2)) ||
		(ped->GetModelNameHash() == ATSTRINGHASH("mp_f_freemode_01", 0x9c9effd8)) )
	{
		Assertf(false, "Nerfing ApplySelectionSetByHash call : can't apply selection set to the MP peds - they have too many possible drawables to be covered by the 8 bit values!");
		return false;
	}

	for(u32 lod = 0; lod < LOD_COUNT; lod++)
	{
		ped->SetLodBucketInfoExists(lod, false);
		ped->SetLodHasAlpha(lod, false);
		ped->SetLodHasDecal(lod, false);
		ped->SetLodHasCutout(lod, false);
	}

	for (u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		u8 drawable = selectionSet->m_compDrawableId[i];
		u8 texture = selectionSet->m_compTexId[i];

		// if no drawable we have two options:
		// 1. we have no texture set either, we randomize both
		// 2. we have a texture set, so we need to choose between the drawables that have enough textures
		// to satisfy the texture set
		if (drawable == /*PV_NULL_DRAWBL !JW_FIXME_HACK!*/0xFF)
		{
			u32 numDrawables = pVarInfo->GetMaxNumDrawbls((ePedVarComp)i);
			if (numDrawables == 0)
				continue;

			if (texture == 255)
			{
				drawable = (u8)fwRandom::GetRandomNumberInRange(0, numDrawables);
			}
			else
			{
				// pick a random drawable and iterate through all to find one that has enough textures
				u32 start = fwRandom::GetRandomNumberInRange(0, numDrawables);
				for (u32 f = 0; f < numDrawables; ++f, ++start)
				{
					if (start == numDrawables)
						start = 0;

					u32 numTex = pVarInfo->GetMaxNumTex((ePedVarComp)i, start);
					if (texture < numTex)
					{
						drawable = (u8)start;
						break;
					}
				}

				if (!Verifyf(drawable != /*PV_NULL_DRAWBL !JW_FIXME_HACK!*/0xFF, "Selection set '%s' on '%s' has invalid settings on component '%s' (couldn't find drawable with enough textures)", selectionSet->m_name.TryGetCStr(), pModelInfo->GetModelName(), varSlotNames[i]))
					continue;
			}
		}

		// if no texture is specified, randomly pick one
		if (texture == 255)
		{
			u32 numTex = pVarInfo->GetMaxNumTex((ePedVarComp)i, drawable);
			if (!Verifyf(numTex > 0, "Selection set '%s' on '%s' has invalid settings on component '%s' (couldn't find texture)", selectionSet->m_name.TryGetCStr(), pModelInfo->GetModelName(), varSlotNames[i]))
				continue;

			texture = (u8)fwRandom::GetRandomNumberInRange(0, numTex);
		}

		SetVariation(ped, pedPropData, pedVarData, (ePedVarComp)i, drawable, 0, texture, 0);
	}

	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		eAnchorPoints anchor = (eAnchorPoints)selectionSet->m_propAnchorId[i];
		u8 prop = selectionSet->m_propDrawableId[i];
		u8 tex = selectionSet->m_propTexId[i];

		if (anchor == ANCHOR_NONE)
			continue;

		// if no drawable set, randomize one
		if (selectionSet->m_propDrawableId[i] == 255)
		{
			u32 numProps = pVarInfo->GetMaxNumProps(anchor);
			if (numProps == 0)
				continue;

			prop = (u8)fwRandom::GetRandomNumberInRange(0, numProps);
		}

		// if no texture, randomize one
		if (tex == 255)
		{
			u32 numTex = pVarInfo->GetMaxNumPropsTex(anchor, prop);
			if (!Verifyf(numTex > 0, "Selection set '%s' on '%s' has invalid settings on prop anchor '%s' (couldn't find textures)", selectionSet->m_name.TryGetCStr(), pModelInfo->GetModelName(), propSlotNames[anchor]))
				continue;

			tex = (u8)fwRandom::GetRandomNumberInRange(0, numTex);
		}

		CPedPropsMgr::SetPedProp(ped, anchor, prop, tex, ANCHOR_NONE, NULL, NULL);
	}

	return true;
}

// if ped using alpha components then set draw last flag
// Complex signature is so it can work with dummy peds as well.
void CPedVariationPack::UpdatePedForAlphaComponents(CEntity* pEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData){
	Assert(pEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	// get Dwd through the pedModelInfo
	const Dwd* pDwd = NULL;
	if (pModelInfo->GetPedComponentFileIndex() != -1)
		pDwd = g_DwdStore.Get(strLocalIndex(pModelInfo->GetPedComponentFileIndex()));

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();

	pedVarData.m_bIsCurrVarAlpha	= false;
	pedVarData.m_bIsCurrVarDecal	= false;
	pedVarData.m_bIsCurrVarCutout	= false;		

	if (!Verifyf(pVarInfo, "No pedVarInfo data on ped '%s'!", pModelInfo->GetModelName()))
        return;

	for(u32 i=0;i<PV_MAX_COMP;i++)
	{
		u32 compIdx = pedVarData.GetPedCompIdx((ePedVarComp)i);

		if (compIdx == PV_NULL_DRAWBL)
		{
			continue;
		}

		{
			CPVDrawblData* pDrawblData = pVarInfo->GetCollectionDrawable(i, compIdx);
			if(pDrawblData)
			{
				if(pDrawblData->HasAlpha())
				{
					pedVarData.m_bIsCurrVarAlpha = true;
				}
				if(pDrawblData->HasDecal())
				{
					pedVarData.m_bIsCurrVarDecal = true;
				}
				if(pDrawblData->HasCutout())
				{
					pedVarData.m_bIsCurrVarCutout = true;
				}

				// check if this ped has a decal decoration
				if (pDwd && pDrawblData->HasActiveDrawbl())
				{
					rmcDrawable* pDrawable = ExtractComponent(pDwd, static_cast<ePedVarComp>(i), pedVarData.m_aDrawblId[i],  &pedVarData.m_aDrawblHash[i], pedVarData.m_aDrawblAltId[i]);

					if (pDrawable && pDrawable->GetShaderGroup().LookupShader("ped_decal_decoration"))
					{
						pedVarData.m_bIsCurrVarDecalDecoration = true;
					}
				}
			}
		}
	}

	pEntity->ClearBaseFlag(fwEntity::HAS_ALPHA | fwEntity::HAS_DECAL | fwEntity::HAS_CUTOUT);
	

	if( pedPropData.GetIsCurrPropsAlpha() || pedVarData.m_bIsCurrVarAlpha)
	{
		pEntity->SetBaseFlag(fwEntity::HAS_ALPHA);
	}
	
	if( pedPropData.GetIsCurrPropsDecal() || pedVarData.m_bIsCurrVarDecal)
	{
		pEntity->SetBaseFlag(fwEntity::HAS_DECAL);
	}

	if( pedPropData.GetIsCurrPropsCutout() || pedVarData.m_bIsCurrVarCutout)
	{
		pEntity->SetBaseFlag(fwEntity::HAS_CUTOUT);
	}
}


// Return the specified component for the current ped
rmcDrawable* CPedVariationPack::GetComponent(ePedVarComp eComponent, const CEntity* pEntity, CPedVariationData& pedVarData)
{
	Assert(pEntity);

	rmcDrawable* pComponent = NULL;

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

// 	if(isPlayer)
// 	{
// 		CPlayerPed* pPlayerPed = static_cast<CPlayerPed *>(pEntity);
// 		Assert(pPlayerPed);
// 		Assert(pPlayerPed->m_pGfxData);
// 		pComponent = pPlayerPed->m_pGfxData->m_pComponents[eComponent];
// 	}
// 	else
	{

		strLocalIndex	DwdIdx	= strLocalIndex(pModelInfo->GetPedComponentFileIndex());
		Dwd* pDwd		= g_DwdStore.Get(DwdIdx);
		Assert(pDwd);
		if(pDwd)
		{
			u32	    drawblIdx       = pedVarData.GetPedCompIdx(eComponent);
			pComponent     = ExtractComponent(pDwd, eComponent, drawblIdx, &(pedVarData.m_aDrawblHash[eComponent]), pedVarData.m_aDrawblAltId[eComponent]);
		}
	}


	return pComponent;
}

//------------------------------------------------------------------------------------------

