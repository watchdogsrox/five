#include "Peds/rendering/PedVariationDebug.h"

#if __BANK
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
#include "data/growbuffer.h"
#include "diag/art_channel.h"
#include "debug/DebugScene.h"
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
#include "fwnet/httpquery.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/fileExts.h"
#include "fwsys/metadatastore.h"

//game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "debug/DebugGlobals.h"
#include "debug/DebugScene.h"
#include "debug/colorizer.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/NetworkInterface.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped.h"
#include "peds/ped_channel.h"
#include "peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedFactory.h"
#include "Peds/PedPopulation.h"
#include "Peds/pedType.h"
#include "Peds/Ped.h"
#include "peds/PopCycle.h"
#include "peds/rendering/PedVariationPack.h"
#include "Physics/gtaInst.h"
#include "Physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "renderer/MeshBlendManager.h"
#include "renderer/renderer.h"
#include "renderer/RenderThread.h"
#include "shaders/ShaderLib.h"
#include "Script/script.h"
#include "Streaming/streaming.h"
#include "Streaming/streamingmodule.h"
#include "scene/datafilemgr.h"
#include "scene/fileLoader.h"
#include "scene/RegdRefTypes.h"
#include "scene/world/GameWorld.h"
#include "script/script_brains.h"
#include "system/filemgr.h"
#include "System/timer.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "Task/General/TaskBasic.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Tools/smokeTest.h"
#include "Vehicles/VehicleFactory.h"
#include "shaders/ShaderHairSort.h"
#include "Weapons/Info/WeaponInfoManager.h"

//local header
#include "shaders/CustomShaderEffectPed.h"
#include "system/param.h"

#include "debug/GtaPicker.h"
#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UIGadgetInspector.h"
#include "debug/UiGadget/UiGadgetText.h"
#include "fwdebug/picker.h"
#include "Task/Combat/CombatMeleeDebug.h"
#include "peds/PedDebugVisualiser.h"
//#include "Renderer/font.h"

AI_OPTIMISATIONS()

#define PV_SEARCH_PATH		"platform:/models/cdimages"


PARAM(defaultPed,"define default ped (used for '.' spawning");
PARAM(defaultCrowdPed,"define default crowd ped");

#define MAX_NAME_LEN (64)
#define MAX_DRAWABLES (512)
#define MAX_TEXTURES (64)

/* stuff for sorted enum ped names */
static s32* pedSorts;
atArray<const char*> CPedVariationDebug::pedNames;
atArray<const char*> CPedVariationDebug::emptyPedNames;			//hack to get the combo update to work properly
static atArray<const char*> pedNamesCrowd;
static u32	numPedNames;
bkCombo* CPedVariationDebug::pPedCombo = NULL;
static bkCombo*	pCrowdPedCombo = NULL;
static bkCombo*	pCrowdPedCombo2 = NULL;
static bkCombo*	pCrowdPedCombo3 = NULL;
static u32	crowdSetFlags = 0;
static u32	crowdClearFlags = 0;
s32 CPedVariationDebug::currPedNameSelection = 0;
bool CPedVariationDebug::assignScriptBrains = false;
bool CPedVariationDebug::stallForAssetLoad = false;
bool CPedVariationDebug::ms_bLetInvunerablePedsBeAffectedByFire = false;
bool CPedVariationDebug::ms_UseDefaultRelationshipGroups = false;
static u32 pedInfoIndex = fwModelId::MI_INVALID;

atArray<const char*> CPedVariationDebug::pedGrpNames;
bool CPedVariationDebug::displayStreamingRequests = false;

/* end of sorted enum ped names stuff*/

//type
bool bCycleTypeIds = false; // static globals might be better within the scope of the class ? - DW
static bool bWanderAfterCreate = false;
static bool bCreateAsMissionChar = false;
static bool bCreateAsRandomChar = false;
static bool bPersistAfterCreate = false;
static bool bDrownsInstantlyInWater = false;

static u32 numPedsAvail = 0;
static u32 numUnskippedPedsAvail = 0;
static u32	numPedsSkipped = 0;

// general debug stuff
static RegdPed pPedVarDebugPed(NULL);
static RegdPed pDebugPed2(NULL);

#define WIDGET_NAME_LENGTH (24)
//new variation debug stuff
static char varDrawablName[WIDGET_NAME_LENGTH];
static char varDrawablAltName[WIDGET_NAME_LENGTH];
static char varTexName[WIDGET_NAME_LENGTH];
static char dlcCompInformation[RAGE_MAX_PATH] = { 0 };

char* GetPedVariationMgrDrawableName()
{
	return &varDrawablName[0];
}
s32 GetPedVariationMgrDrawableNameLen()
{
	return WIDGET_NAME_LENGTH;
}

static s32 totalVariations = 0;
static s32 crowdPedSelection[3] = {0,0,0};
static s32 pedGrpSelection[5] = { 0, -1, -1, -1, -1};
static u32 percentPedsFromEachGroup[5] = {100, 0, 0, 0, 0};
static s32 crowdSize = 1;
static s32 maxDrawables = 0;
static s32 maxDrawableAlts = 0;
static s32 maxTextures = 0;
static s32 maxPalettes = 0;
static s32 varComponentId = 0;
static u32 varDrawableId = 0;
static s32 varDrawableAltId = 0;
static s32 varTextureId = 0;
static s32 varPaletteId = 0;
static s32 varSetId = 0;
static s32 varNumSets = 1;
static const char* varSetList[128];
static bkCombo*	varSetCombo = NULL;

static bkSlider* pVarDrawablIdSlider = NULL;
static bkSlider* pVarDrawablAltIdSlider = NULL;
static bkSlider* pVarTexIdSlider = NULL;
static bkSlider* pVarPaletteIdSlider = NULL;
static float	scaleFactor = 0.0f;

static bool bRandomizeVars = false;
static bool bCycleVars = false;
//static bool bDrawFragModel = false;

static bool bDisableDiffuse = false;
grcTexture*	pDebugGreyTexture = NULL;

static bool bDisplayVarData = false;
static bool bDisplayPedMetadata = false;
static bool bForceHdAssets = false;
static bool bForceLdAssets = false;
static bool bDisplayMemoryBreakdown = false;
static bool bDebugOutputDependencyNames = false;

static CUiGadgetInspector* pInspectorWindow = NULL;
static bool bIsInspectorAttached = false;

// end of new variation debug stuff

static float distToCreateCrowdFromCamera = 2.5f;
static Vector3 crowdSpawnLocation(0.0f,0.0f,0.0f);
static float crowdUniformDistribution = -1.0f;

static bkCombo* headBlendHeadGeomCombo1 = NULL;
static bkCombo* headBlendHeadTexCombo1 = NULL;
static bkCombo* headBlendHeadGeomCombo2 = NULL;
static bkCombo* headBlendHeadTexCombo2 = NULL;
static bkCombo* headBlendHeadGeomCombo3 = NULL;
static bkCombo* headBlendHeadTexCombo3 = NULL;
static s32 headBlendHeadGeom1 = -1;
static s32 headBlendHeadTex1 = -1;
static s32 headBlendHeadGeom2 = -1;
static s32 headBlendHeadTex2 = -1;
static s32 headBlendHeadGeom3 = -1;
static s32 headBlendHeadTex3 = -1;
static s32 headBlendOverlayBaseDetail = 0;
static s32 headBlendOverlayBlemishes = 0;
static s32 headBlendOverlaySkinDetail1 = 0;
static s32 headBlendOverlaySkinDetail2 = 0;
static s32 headBlendOverlayBody1 = 0;
static s32 headBlendOverlayBody2 = 0;
static s32 headBlendOverlayBody3 = 0;
static s32 headBlendOverlayFacialHair = 0;
static s32 headBlendOverlayEyebrow = 0;
static s32 headBlendOverlayAging = 0;
static s32 headBlendOverlayMakeup = 0;
static s32 headBlendOverlayBlusher = 0;
static s32 headBlendOverlayDamage = 0;
static float headBlendOverlayBaseDetailBlend = 1.f;
static float headBlendOverlayBlemishesBlend = 1.f;
static float headBlendOverlaySkinDetail1Blend = 1.f;
static float headBlendOverlaySkinDetail2Blend = 1.f;
static float headBlendOverlayBody1Blend = 1.f;
static float headBlendOverlayBody2Blend = 1.f;
static float headBlendOverlayBody3Blend = 1.f;
static float headBlendOverlayFacialHairBlend = 1.f;
static float headBlendOverlayEyebrowBlend = 1.f;
static float headBlendOverlayAgingBlend = 1.f;
static float headBlendOverlayMakeupBlend = 1.f;
static float headBlendOverlayBlusherBlend = 1.f;
static float headBlendOverlayDamageBlend = 1.f;
static float headBlendOverlayBaseDetailNormBlend = 1.f;
static float headBlendOverlayBlemishesNormBlend = 1.f;
static float headBlendOverlaySkinDetail1NormBlend = 1.f;
static float headBlendOverlaySkinDetail2NormBlend = 1.f;
static float headBlendOverlayBody1NormBlend = 1.f;
static float headBlendOverlayBody2NormBlend = 1.f;
static float headBlendOverlayBody3NormBlend = 1.f;
static float headBlendOverlayFacialHairNormBlend = 1.f;
static float headBlendOverlayEyebrowNormBlend = 1.f;
static float headBlendOverlayAgingNormBlend = 1.f;
static float headBlendOverlayMakeupNormBlend = 1.f;
static float headBlendOverlayBlusherNormBlend = 1.f;
static float headBlendOverlayDamageNormBlend = 1.f;
static float headBlendGeomBlend = 0.f;
static float headBlendTexBlend = 0.f;
static float headBlendVarBlend = 0.f;
static float headBlendGeomAndTexBlend = 0.f;
static bool headBlendLinkClones = false;
static bool headBlendCreateAsParent = true;
static u32 headBlendHairTintIndex = 0;
static u32 headBlendHairTintIndex2 = 0;
static u32 headBlendOverlayBlusherTint = 0;
static u32 headBlendOverlayBlusherTint2 = 0;
static u32 headBlendOverlayEyebrowTint = 0;
static u32 headBlendOverlayEyebrowTint2 = 0;
static u32 headBlendOverlayFacialHairTint = 0;
static u32 headBlendOverlayFacialHairTint2 = 0;
static u32 headBlendOverlaySkinDetail1Tint = 0;
static u32 headBlendOverlaySkinDetail1Tint2 = 0;
static u32 headBlendOverlayBody1Tint = 0;
static u32 headBlendOverlayBody1Tint2 = 0;
static u32 headBlendOverlayBody2Tint = 0;
static u32 headBlendOverlayBody2Tint2 = 0;
static u32 headBlendOverlayBody3Tint = 0;
static u32 headBlendOverlayBody3Tint2 = 0;
// static bool headBlendDebugTextures = false;
static Color32 headBlendPaletteRgb0;
static Color32 headBlendPaletteRgb1;
static Color32 headBlendPaletteRgb2;
static Color32 headBlendPaletteRgb3;
static bool headBlendPaletteRgbEnabled = false;
static bool headBlendSecondGenEnable = false;
static RegdPed headBlendParent0;
static RegdPed headBlendParent1;
static RegdPed headBlendSecondGenChild;
static float headBlendSecondGenGeomBlend = 0.5f;
static float headBlendSecondGenTexBlend = 0.5f;
static bkSlider* headBlendEyeColorSlider = NULL;
static s32 headBlendEyeColor = -1;

static float microMorphNoseWidthBlend = 0.f;
static float microMorphNoseHeightBlend = 0.f;
static float microMorphNoseLengthBlend = 0.f;
static float microMorphNoseProfileBlend = 0.f;
static float microMorphNoseTipBlend = 0.f;
static float microMorphNoseBrokeBlend = 0.f;
static float microMorphBrowHeightBlend = 0.f;
static float microMorphBrowDepthBlend = 0.f;
static float microMorphCheekHeightBlend = 0.f;
static float microMorphCheekDepthBlend = 0.f;
static float microMorphCheekPuffedBlend = 0.f;
static float microMorphEyesSizeBlend = 0.f;
static float microMorphLipsSizeBlend = 0.f;
static float microMorphJawWidthBlend = 0.f;
static float microMorphJawRoundBlend = 0.f;
static float microMorphChinHeightBlend = 0.f;
static float microMorphChinDepthBlend = 0.f;
static float microMorphChinPointedBlend = 0.f;
static float microMorphChinBumBlend = 0.f;
static float microMorphNeckMaleBlend = 0.f;

// Armament selection
enum eArmament
{
	eUnarmed =0,
	e1Handed,
	e2Handed,
	eProjectile,
	eAssortedGuns,
	eThrown,
	eMaxArmament

};
static const char * pArmamentText[] =
{
	"Unarmed",
	"1 handed",
	"2 handed",
	"Projectile",
	"AssortedGuns",
	"Thrown",
	0
};

static eArmament g_eArmament = eUnarmed;


static ePVRaceType g_eRace = PV_RACE_UNIVERSAL;
static const char* pRaceText[] =
{
	"Universal",
	"White",
	"Black",
	"Chinese",
	"Latino",
	"Arab",
	"Balkan",
	"Jamaican",
	"Korean",
	"Italian",
	"Pakistani",
	0
};

static float g_fStartingHealth = 200.0f;

// Armament selection
enum eGroup
{
	eNoGroup =0,
	ePlayersGroup,
	eNewGroup,
	eGroupCount
};
static const char * pGroupText[] =
{
	"No Group",
	"Players Group",
	"New Group",
	0
};

static eGroup g_eGroup = eNoGroup;
static s32 g_eFormation = CPedFormationTypes::FORMATION_LOOSE;
static bool	g_CreateExactGroupSize = false;
static bool g_MakeInvunerableAndStupid = false;
static bool g_SpawnWithProp = false;
static bool	g_NeverLeaveGroup = false;
static bool	g_DrownsInWater = true;
static bool g_UnholsteredWeapon = true;
static bool	g_GivePedTestHarnessTask = false;
static const char * pRelationshipText[] =
{
	"NONE",
	"RESPECT",
	"LIKE",
	"IGNORE",
	"DISLIKE",
	"WANTED",
	"HATE"
};
static s32 g_iRelationship = 0;

static s32 g_iRelationshipGroup = 1;
static s32 g_iOtherRelationshipGroup = 2;

// Relationship group selection
static const char * pRelationshipGroupText[] =
{
	"PLAYER",
	"TEST1",
	"TEST2",
	"TEST3",
	"TEST4",
	"TEST5",
	"TEST6",
	"TEST7",
	"TEST8",
	0
};

bool bDisplayName = false;
bool bDisplayFadeValue = false;


// Name			:	CPedVariationDebug::CPedVariationDebug
// Purpose		:	Do application level setup
// Parameters	:	None
// Returns		:	Nothing
CPedVariationDebug::CPedVariationDebug(void)
{
}

// Name			:	CPedVariationDebug::~CPedVariationDebug
// Purpose		:	Do application level cleanup
// Parameters	:	None
// Returns		:	Nothing
CPedVariationDebug::~CPedVariationDebug(void)
{
}

// smoketest accessors
void	CPedVariationDebug::SetCrowdSize(const s32 i)								{ crowdSize = i; }
void	CPedVariationDebug::SetCrowdPedSelection(const s32 idx, const s32 id)	{ crowdPedSelection[idx] = id; }
u32		CPedVariationDebug::GetNumPedNames()										{ return numPedNames; }
void	CPedVariationDebug::SetDistToCreateCrowdFromCamera(const float dist)		{ distToCreateCrowdFromCamera=dist; } 
void	CPedVariationDebug::SetWanderAfterCreate(const bool set)					{ bWanderAfterCreate = set; }
void	CPedVariationDebug::SetCreateExactGroupSize(const bool bSet)				{ g_CreateExactGroupSize = bSet; }
void	CPedVariationDebug::SetCrowdSpawnLocation(const Vector3 &pos)				{ crowdSpawnLocation = pos; }
void	CPedVariationDebug::SetCrowdUniformDistribution(const float dist)			{ crowdUniformDistribution = dist; }

// Name			:	CPedVariationDebug::Intialise
// Purpose		:	Do some setup required for restarting game
// Parameters	:	None
// Returns		:	Nothing
bool CPedVariationDebug::Initialise(void)
{
	if (pDebugGreyTexture == NULL){
		grcImage* pMidGreyDebug = grcImage::Create(32,32,1,grcImage::A8R8G8B8,grcImage::STANDARD,0,0);
		pMidGreyDebug->Clear(0xff7f7f7f);
		BANK_ONLY(grcTexture::SetCustomLoadName("DebugGreyTexture");)
		pDebugGreyTexture = grcTextureFactory::GetInstance().Create(pMidGreyDebug);
		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
		pMidGreyDebug->Release();
	}

	return true;
}

// Name			:	CPedVariationDebug::Shutdown
// Purpose		:	Clean up all stuff used during the game, ready for a restart.
// Parameters	:	None
// Returns		:	Nothing
void CPedVariationDebug::Shutdown(void)
{
	if(pDebugGreyTexture)
	{
		(void)AssertVerify(pDebugGreyTexture->Release()ASSERT_ONLY(==0));
		pDebugGreyTexture = NULL;
	}

	pedNames.Reset();
	emptyPedNames.Reset();
	pedNamesCrowd.Reset();
	
	//this is to fix a crash in the script widgets being reset at the start of a new game.
	pedNames.PushAndGrow("Inactive");
	pedNames.PushAndGrow("Activate");

	delete []pedSorts;
	pedSorts = NULL;

	bIsInspectorAttached = false;
	CGtaPickerInterface* pickerInterface = (CGtaPickerInterface*)g_PickerManager.GetInterface();
	if (pickerInterface)
	{
		pickerInterface->DetachInspectorChild(pInspectorWindow);
	}

	delete pInspectorWindow;
	pInspectorWindow = NULL;
}

void CPedVariationDebug::ShutdownLevel(void){
	CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_human"), STRFLAG_DONTDELETE);
	CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_small_quadped"), STRFLAG_DONTDELETE);
	CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_large_quadped"), STRFLAG_DONTDELETE);
}

void PedIDChangeCB(void);
void RefreshVarSet();

void CPedVariationDebug::InitDebugData()
{
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypeArray;
	pedModelInfoStore.GatherPtrs(pedTypeArray);

	numPedsAvail = pedTypeArray.GetCount()-1;
	UpdatePedList();

	CDebugScene::RegisterEntityChangeCallback(datCallback(CFA1(CPedVariationDebug::FocusEntityChanged), 0, true));

//	pPedIdSlider->SetRange(0.0f, float(numPedsAvail));

	// point the debug ped ptr at the player (since no debug ped created yet...)
    pPedVarDebugPed = CGameWorld::FindLocalPlayer();
	//currPedNameSelection = numPedsAvail;	// player should be last in list...

	const char * defaultPedName="";
	if(PARAM_defaultPed.Get())
	{
		PARAM_defaultPed.Get(defaultPedName);
	}
	else if (pPedVarDebugPed)
	{
		defaultPedName = pPedVarDebugPed->GetModelName();
	}

	u32 i=0;
	for(i=0;i<numPedsAvail;i++){
		if (stricmp(defaultPedName,pedNames[i]) == 0){
			break;
		}
	}

	SetDebugPed(pPedVarDebugPed, i-1);

	if(PARAM_defaultCrowdPed.Get())
	{
		const char * defaultCrowdName = NULL;
		PARAM_defaultCrowdPed.Get(defaultCrowdName);
		if(defaultCrowdName)
		{
			for(i=0;i<numPedsAvail;i++){
				if (stricmp(defaultCrowdName,pedNames[i]) == 0){
					crowdPedSelection[0] = i;
					break;
				}
			}
		}
	}

	if(crowdPedSelection[0] == 0)
	{
		// If it hasn't been set, then set the renta crowd to something too.
		for(i=0;i<numPedsAvail;i++){
			CPedModelInfo* pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromHashKey(atStringHash(pedNames[i]), NULL);
			if(pModelInfo && pModelInfo->GetAmbulanceShouldRespondTo()){ // Should only be set for humans
				crowdPedSelection[0] = i;
				break;
			}
		}
	}
}

void CPedVariationDebug::FocusEntityChanged(CEntity* pEntity)
{
	if (pEntity && pEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		
		u32 i=0;
		for(i=0; i<numPedsAvail;i++)
		{
			CPedModelInfo* pModelInfo = pPed->GetPedModelInfo(); 

			if(!stricmp(pModelInfo->GetModelName(), pedNames[i]))
			{
				SetDebugPed(pPed, i-1); 
				break; 
			}
		}
	}
}

bool CPedVariationDebug::IsRaceCompatible(ePVRaceType typeA, ePVRaceType typeB)
{
	// races are compatible if they are the same, or if either one is universal
	if ((typeA != typeB) &&
		(typeA != PV_RACE_UNIVERSAL) &&
		(typeB != PV_RACE_UNIVERSAL))
	{
		return(false);			// non matching race types found
	}

	return(true);
}




// Return if ped has the variation
bool CPedVariationDebug::HasVariation(CPed* pPed, ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId)
{
	Assert(pPed);

	const CPedVariationData& varData = pPed->GetPedDrawHandler().GetVarData();

	if(varData.GetPedCompIdx(slotId) == drawblId && varData.GetPedCompAltIdx(slotId) == drawblAltId && varData.GetPedTexIdx(slotId) == texId)
		return true;
	return false;
}

bool 
CPedVariationDebug::IsVariationValid(CDynamicEntity* pDynamicEntity, 
								   ePedVarComp componentId, 
								   s32 drawableId, 
								   s32 textureId)
{
	Assert(pDynamicEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());

	if (AssertVerify(pModelInfo && pModelInfo->GetVarInfo()))
	{
		if ( drawableId >= pModelInfo->GetVarInfo()->GetMaxNumDrawbls(componentId) || textureId >= pModelInfo->GetVarInfo()->GetMaxNumTex(componentId, drawableId) )
		{
			return false;
		}

		if (!pModelInfo->GetVarInfo()->IsValidDrawbl(componentId, drawableId))
		{
			return false;
		}
	}

	return true;
}

//
//
// determine if the current variation data for the ped is valid (doesn't contain any conflicting race textures)
//
bool CPedVariationDebug::CheckVariation(CPed* pPed)
{
	Assert(pPed);

	CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());

	if (AssertVerify(pModelInfo))
	{
		ePVRaceType		componentRaceTypes[PV_MAX_COMP] = { PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL,
															PV_RACE_UNIVERSAL, PV_RACE_UNIVERSAL,};

		u32	drawableIdx = 0;
		u32	texIdx = 0;

		// grab the race types for all the textures used on this ped
		for(u32 i=0;i<PV_MAX_COMP;i++)
		{
			drawableIdx = pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i];
			if (pModelInfo->GetVarInfo()->IsValidDrawbl(i, drawableIdx))
			{
				texIdx = pPed->GetPedDrawHandler().GetVarData().m_aTexId[i];
				componentRaceTypes[i] = pModelInfo->GetVarInfo()->GetRaceType(i, drawableIdx, texIdx, pModelInfo);
			}
		}

		// check for incompatible textures in all the components
		for(u32 i=0; i<PV_MAX_COMP; i++) {
			for(u32 j=0; j<i; j++) {
				if (!IsRaceCompatible(componentRaceTypes[i], componentRaceTypes[j])) {
					return(false);			// non matching race types found
				}
			}
		}
	}

	return(true);
}

void CreatePed(u32 pedModelIndex, const Vector3& pos, bool bRandomOrientation)
{	
	if (CPed::IsSuperLodFromIndex(pedModelIndex))
	{
		return;
	}

	Matrix34 TempMat;

	TempMat.Identity();
	TempMat.d = pos;

	fwModelId pedModelId((strLocalIndex(pedModelIndex)));

	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		pedWarningf("Ped has not successfully streamed. Cannot create at this point.");
		return;
	}

#if __ASSERT
	CPedModelInfo * pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	Assert(pPedModelInfo);
#endif

	// create the debug ped which will have it's textures played with
	const CControlledByInfo localAiControl(false, false);
	if(pPedVarDebugPed)
	{
		pDebugPed2 = pPedVarDebugPed;
		pPedVarDebugPed = NULL;
	}
	pPedVarDebugPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &TempMat, true, true, bCreateAsMissionChar);
	Assertf(pPedVarDebugPed, "PedVariationMgr.pp : CreatePed() - Couldn't create a the ped variation debug ped");

    if(pPedVarDebugPed)
    {
	    // use the settings in the widget for this ped
		if (pPedVarDebugPed->IsVariationValid(static_cast<ePedVarComp>(varComponentId), varDrawableId, varTextureId))
		{
			pPedVarDebugPed->SetVariation(static_cast<ePedVarComp>(varComponentId), varDrawableId, 0, varTextureId, 0, 0, false);
		}
		else
		{
			pPedVarDebugPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
		}


		if(bCreateAsMissionChar)
		{
			pPedVarDebugPed->PopTypeSet(POPTYPE_MISSION);
			pPedVarDebugPed->SetDefaultDecisionMaker();
			pPedVarDebugPed->SetCharParamsBasedOnManagerType();
			pPedVarDebugPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

            if(pPedVarDebugPed->GetNetworkObject())
            {
                pPedVarDebugPed->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true); 
            }
		}
		else if (bCreateAsRandomChar)
		{
			pPedVarDebugPed->PopTypeSet(POPTYPE_RANDOM_PATROL);
		}

	    if (bRandomOrientation){
		    float newHeading = fwRandom::GetRandomNumberInRange(-PI, PI);
		    pPedVarDebugPed->SetHeading(newHeading);
		    pPedVarDebugPed->SetDesiredHeading(newHeading);
	    }

		if( bDrownsInstantlyInWater )
		{
			pPedVarDebugPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming, true );
		}

	    CGameWorld::Add(pPedVarDebugPed, CGameWorld::OUTSIDE );
		pPedVarDebugPed->GetPortalTracker()->ScanUntilProbeTrue();
    }
	RefreshVarSet();
}

u32 CPedVariationDebug::GetWidgetPedType( void )
{
	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypeArray;
	pedModelInfoStore.GatherPtrs(pedTypeArray);

	CPedModelInfo& pedModelInfo = *pedTypeArray[ pedSorts[currPedNameSelection + numPedsSkipped ] ];
	return CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();
}

void DestroyPedCB(void)
{
	if (pPedVarDebugPed && !pPedVarDebugPed->IsAPlayerPed())
	{
		CPedFactory::GetFactory()->DestroyPed(pPedVarDebugPed, true);
		pPedVarDebugPed = NULL;
	}
}

void DestroyAllPedsCB(void)
{
	CPedPopulation::RemoveAllPedsHardNotPlayer();
}

void CPedVariationDebug::CycleTypeId(void)
{
	CPedVariationDebug::currPedNameSelection = (CPedVariationDebug::currPedNameSelection + 1)%(numPedsAvail+1);
	static s32 lastCurrPedNameSelection = -1;
	if (lastCurrPedNameSelection>CPedVariationDebug::currPedNameSelection)
	{
		atDisplayf("Last ped has been loaded.");
	}
	lastCurrPedNameSelection = CPedVariationDebug::currPedNameSelection;
}

void CPedVariationDebug::SetTypeIdByName(const char* szName)
{
	u32 i=0;
	for(i=0;i<numPedsAvail;i++){
		if (stricmp(szName,CPedVariationDebug::pedNames[i]) == 0)
		{
			CPedVariationDebug::currPedNameSelection = i-1;
			break;
		}
	}
}

void CPedVariationDebug::CreateNextPedCB(void)
{
	CycleTypeId();
	DestroyPedCB();
	CreatePedCB();
}

void UpdateVarComponentCB();
void CPedVariationDebug::CreatePedCB(void)
{
	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection + numPedsSkipped ] ];
	pedInfoIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();

	// if this toggle is on then go through each ped type available
	if (bCycleTypeIds) {
		CycleTypeId();
	}

	// generate a location to create the ped from the camera position & orientation
	Vector3 srcPos = camInterface::GetPos();
	Vector3 destPos = srcPos;
	Vector3 viewVector = camInterface::GetFront();

	static dev_float DIST_IN_FRONT = 7.0f;
	destPos += viewVector*DIST_IN_FRONT;		// create at position DIST_IN_FRONT metres in front of current camera position

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(srcPos, destPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	probeDesc.SetExcludeEntity(CGameWorld::FindLocalPlayer());
	probeDesc.SetContext(WorldProbe::LOS_Unspecified);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		destPos = probeResults[0].GetHitPosition();
		static dev_float MOVE_BACK_DIST = 1.f;
		destPos -= viewVector * MOVE_BACK_DIST;
	}

	// try to avoid creating the ped on top of the player
	const Vector3 vLocalPlayerPosition = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	if((vLocalPlayerPosition - destPos).XYMag2() < 0.8f*0.8f)
	{
		if((vLocalPlayerPosition - camInterface::GetPos()).Mag2() < square(3.0f))
			destPos -= viewVector;
	}

	if (destPos.z <= -100.0f)
	{
		destPos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, destPos.x, destPos.y);
	}
	destPos.z += 1.0f;

	CreatePed(pedInfoIndex, destPos, false);

//	pPedVarDebugPed->GetPedIntelligence()->AddTaskDefault(new CTaskSimpleMoveTrackingEntity(PEDMOVE_WALK, FindPlayerPed(),Vector3(0,1.0f,0.0f), false));

    if(pPedVarDebugPed)
    {
		if(CPedVariationDebug::assignScriptBrains)
		{
			CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pPedVarDebugPed, CScriptsForBrains::PED_STREAMED);
		}

	    CPedPropsMgr::WidgetUpdate();
    }
}

void CPedVariationDebug::CreatePedFromSelectedCB(void)
{
	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection + numPedsSkipped ] ];
	pedInfoIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();

	// if this toggle is on then go through each ped type available
	if (bCycleTypeIds) {
		CycleTypeId();
	}

	// generate a location to create the ped from the camera position & orientation
	Vector3 srcPos = camInterface::GetPos();
	Vector3 destPos = srcPos;
	Vector3 viewVector = camInterface::GetFront();

	static dev_float DIST_IN_FRONT = 7.0f;
	destPos += viewVector*DIST_IN_FRONT;		// create at position DIST_IN_FRONT metres in front of current camera position

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(srcPos, destPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	probeDesc.SetExcludeEntity(CGameWorld::FindLocalPlayer());
	probeDesc.SetContext(WorldProbe::LOS_Unspecified);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		destPos = probeResults[0].GetHitPosition();
		static dev_float MOVE_BACK_DIST = 1.f;
		destPos -= viewVector * MOVE_BACK_DIST;
	}

	// try to avoid creating the ped on top of the player
	const Vector3 vLocalPlayerPosition = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	if((vLocalPlayerPosition - destPos).XYMag2() < 0.8f*0.8f)
	{
		if((vLocalPlayerPosition - camInterface::GetPos()).Mag2() < square(3.0f))
			destPos -= viewVector;
	}

	if (destPos.z <= -100.0f)
	{
		destPos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, destPos.x, destPos.y);
	}
	destPos.z += 1.0f;

	if (CPed::IsSuperLodFromIndex(pedInfoIndex))
	{
		return;
	}

	Matrix34 TempMat;

	TempMat.Identity();
	TempMat.d = destPos;

	fwModelId pedModelId((strLocalIndex(pedInfoIndex)));

#if __ASSERT
	CPedModelInfo * pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	Assert(pPedModelInfo);
#endif

	// create the debug ped which will have it's textures played with
	const CControlledByInfo localAiControl(false, false);
	if(pPedVarDebugPed)
	{
		pPedVarDebugPed = NULL;
	}

	CPed* source = NULL;
	CEntity* selected = (CEntity*)g_PickerManager.GetSelectedEntity();
	if (selected && selected->GetIsTypePed())
		source = (CPed*)selected;

	pPedVarDebugPed = CPedFactory::GetFactory()->CreatePedFromSource(localAiControl, pedModelId, &TempMat, source, CPedVariationDebug::stallForAssetLoad, true, bCreateAsMissionChar);
	Assertf(pPedVarDebugPed, "PedVariationMgr.pp : CreatePed() - Couldn't create a the ped variation debug ped");

	if(pPedVarDebugPed)
	{
		if(bCreateAsMissionChar)
		{
			pPedVarDebugPed->PopTypeSet(POPTYPE_MISSION);
			pPedVarDebugPed->SetDefaultDecisionMaker();
			pPedVarDebugPed->SetCharParamsBasedOnManagerType();
			pPedVarDebugPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

			if(pPedVarDebugPed->GetNetworkObject())
			{
				pPedVarDebugPed->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true); 
			}
		}
		else if (bCreateAsRandomChar)
		{
			pPedVarDebugPed->PopTypeSet(POPTYPE_RANDOM_PATROL);
		}

		CGameWorld::Add(pPedVarDebugPed, CGameWorld::OUTSIDE );
		pPedVarDebugPed->GetPortalTracker()->ScanUntilProbeTrue();
	}

	if(pPedVarDebugPed)
	{
		if(CPedVariationDebug::assignScriptBrains)
		{
			CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pPedVarDebugPed, CScriptsForBrains::PED_STREAMED);
		}

		CPedPropsMgr::WidgetUpdate();
	}
	RefreshVarSet();
}

void CPedVariationDebug::ClonePedCB(void)
{
	if (pPedVarDebugPed)
	{
		pPedVarDebugPed = CPedFactory::GetFactory()->ClonePed(pPedVarDebugPed, false, headBlendLinkClones, true);
	}
	RefreshVarSet();
}

void CPedVariationDebug::ClonePedToLastCB(void)
{
	if (pPedVarDebugPed && pDebugPed2)
	{
		CPedFactory::GetFactory()->ClonePedToTarget(pDebugPed2, pPedVarDebugPed, true);
	}
}

#define CORRECT_NUM_RAGDOLL_CHILDREN (21)

/*
void CheckSkeleton(rmcDrawable* pDrawable, CPedModelInfo* pPedModelInfo, bool bCutscene, int &numBadTrans, int &numBadRot)
{
	// Check the orientation of the root bone is consistent (same as the players)
	// e.g. euler 0.000000 -0.000000 1.570813

	if (!pDrawable->GetSkeletonData() || !pPedModelInfo)
	{
		return;
	}

	crSkeletonData &skelData = *pDrawable->GetSkeletonData();
	int rootBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_ROOT);
	if (rootBoneIndex==BONETAG_INVALID)
	{
		Displayf("Missing root bone\n");
	}
	else
	{
		Vector3 eulers = RCC_VECTOR3(skelData.GetBoneData(rootBoneIndex)->GetDefaultRotation());

		float epsilon = 0.01f;
		if (!eulers.IsClose(Vector3(0.000000f, -0.000000f, 1.570813f), epsilon))
		{
			Displayf("Root bone is (%f, %f, %f) should be approximatley (0.0, 0.0, 1.57)\n", eulers.x, eulers.y, eulers.z);
			numBadRot++;
		}

		Vector3 translation = RCC_VECTOR3(skelData.GetBoneData(rootBoneIndex)->GetDefaultTranslation());

		if (pPedModelInfo->IsMale())
		{
			float epsilon = 0.01f;
			//if (!translation.IsClose(Vector3(0.0000f, 0.0227f, 0.9401f), epsilon))
			if (!translation.IsClose(Vector3(0.0000f, 0.0227f, -0.0598f), epsilon))
			{
				Displayf("Root bone translation is (%6.4f, %6.4f, %6.4f) should be approximately (0.0000, 0.0227, -0.0598) for a male \n", translation.x, translation.y, translation.z);
				numBadTrans++;
			}
		}
		else
		{
			float epsilon = 0.01f;
			//if (!translation.IsClose(Vector3(0.0000f, 0.0744f, 0.9052f), epsilon))
			if (!translation.IsClose(Vector3(0.0000f, 0.0744f, -0.0948f), epsilon))
			{
				Displayf("Root bone translation is (%6.4f, %6.4f, %6.4f) should be approximately Vector3(0.0000, 0.0744, 0.0948) for a female \n", translation.x, translation.y, translation.z);
				numBadTrans++;
			}
		}
	}

	//
	// Check that the only certain bones support translation
	//

	// Only check non cutscene peds
	if (!bCutscene)
	{
		bool invalidTranslationDOFs = false;

		// Check the only bones with translation DOFs are the root or the 4 face bones
		for( int i=0; i<skelData.GetNumBones(); i++)
		{
			crBoneData* pBoneData = skelData.GetBoneData(i);
			// Does the bone have any translation channels?
			if (pBoneData->GetNumTransChannels()>0)
			{
				if (  (pBoneData->GetBoneId() != BONETAG_ROOT) )
				{
					invalidTranslationDOFs = true;
					Displayf("The bone tag %d = (%s) should not have translation dofs\n", pBoneData->GetBoneId(), CPedBoneTagConvertor::GetBoneNameFromBoneTag((eAnimBoneTag)pBoneData->GetBoneId()));
				}
			}
		}


		if (invalidTranslationDOFs)
		{
			Displayf("Only the root bone is allowed translation DOFs \n");
		}
	}
}
*/

void ValidateAllPedsCB(void)
{
#if __DEV
	Displayf("Validation started!\n");

	int nPeds = 0;	
	bool bLowLOD = false;
	int nLowLODs = 0;
	bool bBadRot = false;
	int nBadRots = 0;
	u32 nFragtypeChildren = 0;

	bool bPointHelpers = false;
	int nMissingPointHelperCount  = 0;
	bool bIKHelpers = false;
	int nMissingIKHelperCount = 0;

	char pLine[1024];
	FileHandle fid;
	bool fileOpen = false;

	// Try and open a file for the results
	CFileMgr::SetDir("common:/DATA");
	fid = CFileMgr::OpenFileForWriting("PedValidation.csv");
	CFileMgr::SetDir("");
	if ( CFileMgr::IsValidFileHandle(fid) )
	{
		fileOpen = true;
		sprintf(&pLine[0], "model name, skeldata signature, bones, rotation dofs, translation dofs, expression dict name, expression name, expression set name, low lod, bad root rotation, has ik helpers, has point helpers, root rot x, root rot y, root rot z, fragtype children, has meta, num LODs, CS facial rig, IG facial rig, missing dofs, extra dofs\n");
		ASSERT_ONLY(s32 ReturnVal =) CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
		Assert(ReturnVal > 0);
	}
	else
	{
		Displayf("\\common\\data\\PedValidation.csv is read only");
	}

	ASSET.PushFolder("common:/data");

	char szBuffer[1024];

	// Open CSV file
	fiStream *pedValidationDofsFile = ASSET.Create("PedValidationDofs", "csv");

	// Write header
	sprintf(szBuffer, "Ped,Missing or Extra,Track,Id\n");
	pedValidationDofsFile->WriteByte(szBuffer, istrlen(szBuffer));

	const char *defaultMaleModel = "A_M_Y_skater_01";
	fwModelId defaultMaleModelId = CModelInfo::GetModelIdFromName(defaultMaleModel);
	CPedModelInfo *defaultMaleModelInfo = NULL;
	CPed *defaultMalePed = NULL;
	crFrameData defaultMaleFrameData;
	if(pedVerifyf(defaultMaleModelId.IsValid(), "Could not find default male model id!"))
	{
		if(!CModelInfo::HaveAssetsLoaded(defaultMaleModelId))
		{
			CModelInfo::RequestAssets(defaultMaleModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		CBaseModelInfo *baseModelInfo = CModelInfo::GetBaseModelInfo(defaultMaleModelId);
		if(pedVerifyf(baseModelInfo && baseModelInfo->GetModelType() == MI_TYPE_PED, "Could not get base model info for default male model!"))
		{
			defaultMaleModelInfo = static_cast< CPedModelInfo * >(baseModelInfo);

			const CControlledByInfo localAiControl(false, false);
			Matrix34 mat; mat.Identity();
			defaultMalePed = CPedFactory::GetFactory()->CreatePed(localAiControl, defaultMaleModelId, &mat, true, true, false);
			if(pedVerifyf(defaultMalePed, "Could not create default male model ped!"))
			{
				crCreature *defaultMaleCreature = defaultMalePed->GetCreature();
				if(pedVerifyf(defaultMaleCreature, "Could not get default male model ped creature!"))
				{
					defaultMaleCreature->InitDofs(defaultMaleFrameData);
				}
			}
		}
	}

	const char *defaultFemaleModel = "A_F_Y_Beach_01";
	fwModelId defaultFemaleModelId = CModelInfo::GetModelIdFromName(defaultFemaleModel);
	CPedModelInfo *defaultFemaleModelInfo = NULL;
	CPed *defaultFemalePed = NULL;
	crFrameData defaultFemaleFrameData;
	if(pedVerifyf(defaultFemaleModelId.IsValid(), "Could not find default female model id!"))
	{
		if(!CModelInfo::HaveAssetsLoaded(defaultFemaleModelId))
		{
			CModelInfo::RequestAssets(defaultFemaleModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		CBaseModelInfo *baseModelInfo = CModelInfo::GetBaseModelInfo(defaultFemaleModelId);
		if(pedVerifyf(baseModelInfo && baseModelInfo->GetModelType() == MI_TYPE_PED, "Could not get base model info for default female model!"))
		{
			defaultFemaleModelInfo = static_cast< CPedModelInfo * >(baseModelInfo);

			const CControlledByInfo localAiControl(false, false);
			Matrix34 mat; mat.Identity();
			defaultFemalePed = CPedFactory::GetFactory()->CreatePed(localAiControl, defaultFemaleModelId, &mat, true, true, false);
			if(pedVerifyf(defaultFemalePed, "Could not create default female model ped!"))
			{
				crCreature *defaultFemaleCreature = defaultFemalePed->GetCreature();
				if(pedVerifyf(defaultFemaleCreature, "Coult node get default female model ped creature!"))
				{
					defaultFemaleCreature->InitDofs(defaultFemaleFrameData);
				}
			}
		}
	}

	// start at current ped selection
	for(u32 i = CPedVariationDebug::currPedNameSelection+numPedsSkipped; i<numPedsAvail; i++)
	{
		// Reset the flags
		bPointHelpers = false;
		bIKHelpers = false;
		bLowLOD = false;
		bBadRot = false;
		nFragtypeChildren = 0;

		// get the modelInfo associated with this ped ID
		fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
		atArray<CPedModelInfo*> pedTypesArray;
		pedModelInfoStore.GatherPtrs(pedTypesArray);

		CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[i] ];
		fwModelId pedModelId = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName());
		pedInfoIndex = pedModelId.GetModelIndex();

		bool hasMeta = pedModelInfo.GetNumPedMetaDataFiles() > 0;

		// get the name of the model used by this model info and display it
		Displayf("Validating ped. Model name : %s\n", pedModelInfo.GetModelName());

		// ensure that the model is loaded
		fwModelId pedInfoId((strLocalIndex(pedInfoIndex)));
		if (!CModelInfo::HaveAssetsLoaded(pedInfoId))
		{
			CModelInfo::RequestAssets(pedInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}
		
		// Check the low lods
		if (pedModelInfo.GetVarInfo() && pedModelInfo.GetVarInfo()->HasLowLODs())
		{
			bLowLOD = true;
			nLowLODs++;
		}

		// Get the number of fragtype children
		gtaFragType *pFragType = pedModelInfo.GetFragType();
		if(pFragType!=NULL)
		{
			nFragtypeChildren = pFragType->GetPhysics(0)->GetNumChildren();
		}
		else
		{
			pedWarningf("Ped has invalid frag. Model name : %s\n", pedModelInfo.GetModelName());
		}

		rmcDrawable* pDrawable = pedModelInfo.GetDrawable();
		if (pDrawable )
		{
			if (pDrawable->GetSkeletonData())
			{
				crSkeletonData &skelData = *pDrawable->GetSkeletonData();
				Vector3 eulers(VEC3_ZERO);

				int rootBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_ROOT);
				if (rootBoneIndex!=-1)
				{
					const crBoneData* boneData = skelData.GetBoneData(rootBoneIndex);
					Vec3V veulers = QuatVToEulersXYZ(boneData->GetDefaultRotation());
					eulers = RCC_VECTOR3(veulers);
					float epsilon = 0.01f;
					if (!eulers.IsClose(Vector3(0.000000f, 0.000000f, -PI), epsilon))
					{
						bBadRot = true;
						nBadRots++;
					}
				}

				int leftHandIKHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_L_IK_HAND);
				int rightHandIKHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_R_IK_HAND);
				int leftFootIKHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_L_IK_FOOT);
				int rightFootIKHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_R_IK_FOOT);
				int rootIKHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_FACING_DIR);
				int headIKHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_LOOK_DIR);
				if ((leftHandIKHelperBoneIndex == -1) || (rightHandIKHelperBoneIndex == -1) ||
					(leftFootIKHelperBoneIndex == -1) || (rightFootIKHelperBoneIndex == -1) ||
					(rootIKHelperBoneIndex == -1) || (headIKHelperBoneIndex == -1))
				{
					nMissingIKHelperCount++;
				}
				else
				{
					bIKHelpers = true;
				}

				int leftHandPointHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_L_PH_HAND);
				int rightHandPointHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_R_PH_HAND);
				int leftFootPointHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_L_PH_FOOT);
				int rightFootPointHelperBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(skelData, BONETAG_R_PH_FOOT);
				if ((leftHandPointHelperBoneIndex == -1) || (rightHandPointHelperBoneIndex == -1) ||
					(leftFootPointHelperBoneIndex == -1) || (rightFootPointHelperBoneIndex == -1))
				{
					nMissingPointHelperCount++;
				}
				else
				{
					bPointHelpers = true;
				}

				// Missing facial rigs
				int junk;
				bool bHasCSFacialRig = skelData.ConvertBoneIdToIndex(crSkeletonData::ConvertBoneNameToId("FACIAL_facialRoot"), junk);
				bool bHasIGFacialRig = skelData.ConvertBoneIdToIndex(crSkeletonData::ConvertBoneNameToId("BONETAG_FB_Jaw"), junk);

				// Check the skeleton
				//CheckSkeleton(pDrawable, &pedModelInfo, bCutscenePed, numberOfBadTrans, numberOfBadRot);

				bool missingDofs = false;
				bool extraDofs = false;

				atString name(pedModelInfo.GetModelName());
				name.Lowercase();
				if(!name.StartsWith("cs") && !name.StartsWith("a_c"))
				{
					CPed *defaultPed = pedModelInfo.IsMale() ? defaultMalePed : defaultFemalePed;
					if(pedVerifyf(defaultPed, "Could not get default ped!"))
					{
						crFrameData &defaultFrameData = pedModelInfo.IsMale() ? defaultMaleFrameData : defaultFemaleFrameData;

						CPedFactory *pedFactory = CPedFactory::GetFactory();
						if(pedVerifyf(pedFactory, "Could not get ped factory!"))
						{
							const CControlledByInfo localAiControl(false, false);
							Matrix34 mat; mat.Identity();
							CPed *pPed = pedFactory->CreatePed(localAiControl, pedModelId, &mat, true, true, false);
							if(pedVerifyf(pPed, "Could not create ped!"))
							{
								crFrameData frameData;
								crCreature *pedCreature = pPed->GetCreature();
								if(pedVerifyf(pedCreature, "Could not get ped creature!"))
								{
									pedCreature->InitDofs(frameData);
								}

								// Missing dofs

								if(pedCreature)
								{
									for(int dofIndex = 0; dofIndex < defaultFrameData.GetNumDofs(); dofIndex ++)
									{
										const crFrameData::Dof &dof = defaultFrameData.GetDof(dofIndex);

										if(!frameData.HasDof(dof.m_Track, dof.m_Id))
										{
											missingDofs = true;

											sprintf(szBuffer, "%s,Missing,%u,%u\n", pedModelInfo.GetModelName(), dof.m_Track, dof.m_Id);
											pedValidationDofsFile->WriteByte(szBuffer, istrlen(szBuffer));
										}
									}
								}

								// Extra dofs

								if(pedCreature)
								{
									for(int dofIndex = 0; dofIndex < frameData.GetNumDofs(); dofIndex ++)
									{
										const crFrameData::Dof &dof = frameData.GetDof(dofIndex);

										if(!defaultFrameData.HasDof(dof.m_Track, dof.m_Id))
										{
											extraDofs = true;

											sprintf(szBuffer, "%s,Extra,%u,%u\n", pedModelInfo.GetModelName(), dof.m_Track, dof.m_Id);
											pedValidationDofsFile->WriteByte(szBuffer, istrlen(szBuffer));
										}
									}
								}

								pedFactory->DestroyPed(pPed);

								pedValidationDofsFile->Flush();
							}
						}
					}
				}

				if (fileOpen)
				{

					sprintf(&pLine[0], "%s, %u, %d, %d, %d, %s, %s, %s, %s, %s, %s, %s, %6.4f, %6.4f, %6.4f, %d, %s, %d, %s, %s, %s, %s\n",
						pedModelInfo.GetModelName(),
						skelData.GetSignature(),
						skelData.GetNumBones(), 
						skelData.GetNumAnimatableDofs(), 
						skelData.GetNumAnimatableDofs(), // fixme JA
						pedModelInfo.GetExpressionDictionaryIndex().IsValid() ? g_ExpressionDictionaryStore.GetName(strLocalIndex(pedModelInfo.GetExpressionDictionaryIndex())) : "Invalid",
						pedModelInfo.GetExpressionHash().IsNotNull() ? pedModelInfo.GetExpressionHash().GetCStr() : "Invalid",
						pedModelInfo.GetExpressionSet().IsNotNull() ? pedModelInfo.GetExpressionSet().GetCStr(): "Invalid",
						bLowLOD ? "true" : "false", 
						bBadRot ? "true" : "false",
						bIKHelpers ? "true" : "false",
						bPointHelpers ? "true" : "false",
						eulers.x, eulers.y, eulers.z,
						nFragtypeChildren,
						hasMeta ? "yes" : "no",
						pedModelInfo.GetNumAvailableLODs(),
						bHasCSFacialRig ? "yes" : "no",
						bHasIGFacialRig ? "yes" : "no",
						missingDofs ? "yes" : "no",
						extraDofs ? "yes" : "no");

					ASSERT_ONLY(s32 ReturnVal =) CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
					Assert(ReturnVal > 0);
				}


				nPeds++;
			}
			else
			{
				pedWarningf("Ped has invalid skeleton data. Model name : %s\n", pedModelInfo.GetModelName());
			}
		}
		else
		{
			pedWarningf("Ped has invalid drawable. Model name : %s\n", pedModelInfo.GetModelName());
		}

		// remove streamed in modelinfo if we can (ie. ref count is 0 and don't delete flags not set)
		if (pedModelInfo.GetNumRefs() == 0)
		{
			fwModelId pedInfoId((strLocalIndex(pedInfoIndex)));
			if  (!(CModelInfo::GetAssetStreamFlags(pedInfoId) & STR_DONTDELETE_MASK))
			{
				CModelInfo::RemoveAssets(pedInfoId);
			}
		}

	}

	if(defaultMalePed)
	{
		CPedFactory::GetFactory()->DestroyPed(defaultMalePed);
	}

	if(defaultFemalePed)
	{
		CPedFactory::GetFactory()->DestroyPed(defaultFemalePed);
	}

	// Close CSV file
	pedValidationDofsFile->Close();

	ASSET.PopFolder();

	Displayf("Summary\n");
	Displayf("Total number of peds %d\n",							nPeds);
	//Displayf("Number of peds with useable low LODs = %d\n",		nLowLODs);
	//Displayf("Number of peds with skinned heads = %d\n",			nSkinnedHeads);
	//Displayf("Number of peds with bad rotation = %d\n",			nBadRots);

	Displayf("Number of peds with missing ik helpers= %d\n",	nMissingIKHelperCount);
	Displayf("Number of peds with missing point helpers= %d\n", nMissingPointHelperCount);

	if (fileOpen)
	{
		Displayf("Saved \\common\\data\\PedValidation.csv");
		CFileMgr::CloseFile(fid);
	}

	Displayf("Validation complete!\n");
#endif //__DEV
}

static u32	frameNumber = 0;
static bool		bNextPed = false;
static bool		bCompleted = true;


// kick off the render test of all components for peds
void PedComponentCheckCB(void){

	bCompleted = false;
	frameNumber = 0;
	bNextPed = true;

	if (CPedVariationDebug::currPedNameSelection+numPedsSkipped >= numPedsAvail){
		CPedVariationDebug::currPedNameSelection = 0;
	}
}


// go through all peds and display every available component to check rendering is OK
void CPedVariationDebug::PedComponentCheckUpdate(){

	if (bCompleted){
		return;
	}

	// if we need to, load the next ped
	// start at current ped selection
	if (bNextPed){
		bNextPed = false;
	//for(i=currPedNameSelection+numPedsSkipped;i<numPedsAvail;i++)

		//get the modelInfo associated with this ped ID
		fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
		atArray<CPedModelInfo*> pedTypesArray;
		pedModelInfoStore.GatherPtrs(pedTypesArray);

		CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection+numPedsSkipped] ];

		fwModelId pedInfoId =  CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName());
		pedInfoIndex = pedInfoId.GetModelIndex();

		//bool cutscenePed = (strncmp(pedModelInfo.GetModelName(),"CS_",3) == 0);

		Displayf("___________________________________\n");

		//get the name of the model used by this model info and display it
		Displayf("Model Name : %s\n", pedModelInfo.GetModelName());

		// ensure that the model is loaded and ready for drawing for this ped
		if (!CModelInfo::HaveAssetsLoaded(pedInfoId))
		{
			CModelInfo::RequestAssets(pedInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		CreatePedCB();
	}

	bool	bShownAllVariations = false;

	if (pPedVarDebugPed){
		for (u32 i=0; i< PV_MAX_COMP; i++){
			if (pPedVarDebugPed->SetVariation((ePedVarComp)i, frameNumber, 0, 0, 0, 0, false)){
				Displayf("Component : (%d,%d) is set\n", i, frameNumber);
			}
		}
	}


	if (frameNumber++ >= MAX_DRAWBL_VARS){
		frameNumber = 0;
		bShownAllVariations = true;
	}

	if (bShownAllVariations){
		CPedFactory::GetFactory()->DestroyPed(pPedVarDebugPed);

		pPedVarDebugPed = NULL;

		bNextPed = true;

		CPedVariationDebug::currPedNameSelection++;
		if (CPedVariationDebug::currPedNameSelection+numPedsSkipped >= numPedsAvail){
			bCompleted = true;
		}
	}

}

// ped ID has changed, so load it in and scan txd for available variations
void CPedVariationDebug::PedIDChangeCB(void)
{
	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection+numPedsSkipped] ];
	pedInfoIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();

	//get the name of the model used by this model info and display it
	Displayf("Loading ped model: %s\n",pedModelInfo.GetModelName());

	fwModelId pedInfoId((strLocalIndex(pedInfoIndex)));
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedInfoId))
	{
		CModelInfo::RequestAssets(pedInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (pedModelInfo.GetVarInfo()){
		totalVariations = (s32)pedModelInfo.GetVarInfo()->GetNumTotalVariations();
		UpdateVarComponentCB();
	} else {
		totalVariations = 1;
	}

	if (pPedVarDebugPed) {
		scaleFactor = pPedVarDebugPed->GetPedDrawHandler().GetVarData().scaleFactor;
	}
}



// Handle a change in palette for the current debug ped
void UpdateVarPaletteCB(void)
{
	// 16 palettes allowed
	if (varPaletteId > MAX_PALETTES){
		varPaletteId = MAX_PALETTES;
	}

	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection + numPedsSkipped ] ];

	maxPalettes = 0;
	if (Verifyf(pedModelInfo.GetVarInfo(), "Ped '%s' has no variation info!", pedModelInfo.GetModelName()) &&
		pedModelInfo.GetVarInfo()->IsValidDrawbl(varComponentId,varDrawableId))
	{
		if (pedModelInfo.GetVarInfo()->HasPaletteTextures(varComponentId, varDrawableId))
		{
			// turn on palette selection
			if (pVarPaletteIdSlider != NULL)
			{
				pVarPaletteIdSlider->SetRange(0.0f, 15.0f);
			}
			maxPalettes = 15;

			//set the updated values for this ped (if it is the same type as being edited in the widget!)
			if(pPedVarDebugPed && (pPedVarDebugPed->GetBaseModelInfo()==&pedModelInfo))
			{
				pPedVarDebugPed->SetVariation(static_cast<ePedVarComp>(varComponentId), varDrawableId, varDrawableAltId, varTextureId, varPaletteId, 0, false);
				pPedVarDebugPed->GetAnimDirector()->InitExpressions();
			}
		} else {
			// disable palette selection
			if (pVarPaletteIdSlider != NULL)
			{
				pVarPaletteIdSlider->SetRange(0.0f, 0.0f);
			}
			varPaletteId = 0;
		}
	}
	else
	{
		sprintf(varTexName,"Invalid Texture");
		sprintf(varDrawablName,"Invalid Drawable");
		sprintf(varDrawablAltName,"No Variation");
		sprintf(dlcCompInformation,"No Variation");

		pVarDrawablIdSlider->SetRange(0.0f, 0.0f);
		pVarDrawablAltIdSlider->SetRange(0.0f, 0.0f);
		pVarTexIdSlider->SetRange(0.0f, 0.0f);
		pVarPaletteIdSlider->SetRange(0.0f, 0.0f);
	}
}

// Handle a change in the texture for the current debug ped.
//
void UpdateVarTexCB(void)
{

	u32	maxVariations = 0;
	char	texVariation = 'a' + static_cast<char>(varTextureId);
	char	drawblRaceType = 'U';
	u32	raceId = 0;

	if (varTextureId > maxTextures){
		varTextureId = maxTextures;
	}

	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection + numPedsSkipped ]];

	if (Verifyf(pedModelInfo.GetVarInfo(), "Ped '%s' has no variation info!", pedModelInfo.GetModelName()) &&
		pedModelInfo.GetVarInfo()->IsValidDrawbl(varComponentId,varDrawableId))
	{
		pedInfoIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();
		raceId = pedModelInfo.GetVarInfo()->GetRaceType(varComponentId, varDrawableId, varTextureId, &pedModelInfo);
		if (pedModelInfo.GetVarInfo()->HasRaceTextures(varComponentId, varDrawableId)){
			drawblRaceType = 'R';
		}

		if (pedModelInfo.GetVarInfo()->IsMatchComponent(varComponentId, varDrawableId, &pedModelInfo)){
			drawblRaceType = 'M';
		}

		// update the texture and drawable names
		sprintf(varTexName,"%s_diff_%03d_%c_%s", varSlotNames[varComponentId], varDrawableId, texVariation, raceTypeNames[raceId]);
		sprintf(varDrawablName,"%s_%03d_%c",varSlotNames[varComponentId],varDrawableId, drawblRaceType);
		if (varDrawableAltId > 0)
		{
			sprintf(varDrawablAltName, "%s_%03d_%c_%02d", varSlotNames[varComponentId], varDrawableId, drawblRaceType, varDrawableAltId);
		}
		else 
		{
			sprintf(varDrawablAltName, "No Alternative");
		}
		
		u32 varInfoHash = 0;
		u32 localIndex = 0;

		CPedVariationPack::GetLocalCompData(pPedVarDebugPed.Get(), (ePedVarComp)varComponentId, varDrawableId, varInfoHash, localIndex);
		sprintf(dlcCompInformation, "DLC Hash = %u || Local Index = %i || Global Index = %i", varInfoHash, localIndex, varDrawableId);

		// update the max values in the sliders
		if (pVarDrawablIdSlider != NULL)
		{
			maxVariations = pedModelInfo.GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(varComponentId));
			pVarDrawablIdSlider->SetRange(0.0f, (float)maxVariations-1);
		}

		if (pVarDrawablAltIdSlider != NULL)
		{
			maxVariations = pedModelInfo.GetVarInfo()->GetMaxNumDrawblAlts(static_cast<ePedVarComp>(varComponentId), varDrawableId);
			pVarDrawablAltIdSlider->SetRange(0.0f, (float)maxVariations);
		}

		if (pVarTexIdSlider != NULL)
		{
			maxVariations = pedModelInfo.GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(varComponentId), varDrawableId);
			pVarTexIdSlider->SetRange(0.0f, (float)maxVariations-1);
		}

		//set the updated values for this ped (if it is the same type as being edited in the widget!)
		if(pPedVarDebugPed && (pPedVarDebugPed->GetBaseModelInfo()==&pedModelInfo))
		{
			if(!CPedVariationPack::HasVariation(pPedVarDebugPed, static_cast<ePedVarComp>(varComponentId), varDrawableId, varDrawableAltId, varTextureId))
			{
				pPedVarDebugPed->SetVariation(static_cast<ePedVarComp>(varComponentId), varDrawableId, varDrawableAltId, varTextureId, 0, 0, false);
				pPedVarDebugPed->GetAnimDirector()->InitExpressions();
			}
		}
	}
	else
	{
		sprintf(varTexName,"Invalid Texture");
		sprintf(varDrawablName,"Invalid Drawable");
		sprintf(varDrawablAltName, "No Variation");

		pVarDrawablIdSlider->SetRange(0.0f, 0.0f);
		pVarDrawablAltIdSlider->SetRange(0.0f, 0.0f);
		pVarTexIdSlider->SetRange(0.0f, 0.0f);
	}

	UpdateVarPaletteCB();
}

// Handle a change in drawable for the current debug ped
//
void UpdateVarDrawblCB(void){

	if (varDrawableId > maxDrawables){
		varDrawableId = maxDrawables;
	}

	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection + numPedsSkipped] ];

	//if debug ped is same type and has same drawable as being edited, grab the texture from the drawable
	if (pPedVarDebugPed && (pPedVarDebugPed->GetBaseModelInfo() == &pedModelInfo) &&
			(CPedVariationPack::GetCompVar(pPedVarDebugPed,static_cast<ePedVarComp>(varComponentId)) == varDrawableId))
	{
		varTextureId = CPedVariationPack::GetTexVar(pPedVarDebugPed, static_cast<ePedVarComp>(varComponentId));
		// want to pick up current palette ID from the current debug ped (for this drawable)
		varPaletteId = CPedVariationPack::GetPaletteVar(pPedVarDebugPed, static_cast<ePedVarComp>(varComponentId));
	} else {
		varTextureId = 0;
	}
	
	if (Verifyf(pedModelInfo.GetVarInfo(), "Ped '%s' has no variation info!", pedModelInfo.GetModelName()))
	{
		maxTextures = pedModelInfo.GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(varComponentId), varDrawableId)-1;
		maxTextures = maxTextures < 0 ? 0 : maxTextures;
		maxDrawableAlts = pedModelInfo.GetVarInfo()->GetMaxNumDrawblAlts(static_cast<ePedVarComp>(varComponentId), varDrawableId);
	}

	UpdateVarTexCB();
}

void UpdateVarComponentCB(void){

	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	CPedModelInfo& pedModelInfo = *pedTypesArray[ pedSorts[CPedVariationDebug::currPedNameSelection + numPedsSkipped] ];

	//if debug ped is same as being edited, then grab the current settings from the ped and copy to widgets
	if (pPedVarDebugPed && (pPedVarDebugPed->GetBaseModelInfo() == &pedModelInfo)) {
		varDrawableId = CPedVariationPack::GetCompVar(pPedVarDebugPed, static_cast<ePedVarComp>(varComponentId));
	} else {
		varDrawableId = 0;
	}

	if (Verifyf(pedModelInfo.GetVarInfo(), "Ped '%s' has no variation info!", pedModelInfo.GetModelName()))
		maxDrawables = pedModelInfo.GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(varComponentId))-1;
	
	UpdateVarDrawblCB();
}

void UpdateVarSetCB()
{
	if (!pPedVarDebugPed)
		return;
	
	if (varSetId == 0)
		pPedVarDebugPed->SetVarRandom(0, 0, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
	else if ((varSetId - 1) < pPedVarDebugPed->GetPedModelInfo()->GetVarInfo()->GetSelectionSetCount())
		pPedVarDebugPed->ApplySelectionSet(varSetId - 1);
}

void RefreshVarSet()
{
	if (!pPedVarDebugPed || !varSetCombo)
		return;

	CPedModelInfo* pmi = pPedVarDebugPed->GetPedModelInfo();
	varNumSets = pmi->GetVarInfo()->GetSelectionSetCount() + 1;
	Assert(varNumSets < 128); // limit of static name array for list
	varSetId = 0;

	varSetList[0] = "<random>";
	for (s32 i = 0; i < varNumSets - 1; ++i)
		if (Verifyf(pmi->GetVarInfo()->GetSelectionSet((u32)i), "Missing selection set %d for %s", i, pmi->GetModelName()))
			varSetList[i + 1] = pmi->GetVarInfo()->GetSelectionSet((u32)i)->m_name.TryGetCStr();
	varSetCombo->UpdateCombo("Sets", &varSetId, varNumSets, varSetList, UpdateVarSetCB);
}


void CyclePedCB(void){

	if (pPedVarDebugPed){
		CPedVariationPack::SetVarCycle(pPedVarDebugPed, pPedVarDebugPed->GetPedDrawHandler().GetPropData(), pPedVarDebugPed->GetPedDrawHandler().GetVarData());
	}
}

void RandomizePedCB(void){

	if (pPedVarDebugPed){
		pPedVarDebugPed->SetVarRandom(crowdSetFlags, crowdClearFlags, NULL, NULL, NULL, g_eRace);
		pPedVarDebugPed->GetAnimDirector()->InitExpressions();
	}
}

void UpdateScaleCB(void){
	if (pPedVarDebugPed){
		pPedVarDebugPed->GetPedDrawHandler().GetVarData().scaleFactor = scaleFactor;
	}
}

void StoreScriptCommandCB(void)
{
	if (pPedVarDebugPed)
	{
		CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
		
		char str[256] = {0};
		for(int index=0; index < PV_MAX_COMP ; index++)
		{
			u32 DrawableId = 0; 
			
			DrawableId = CPedVariationPack::GetCompVar(pPedVarDebugPed, static_cast<ePedVarComp>(varSlotEnums[index]));
				
			if (pModelInfo->GetVarInfo()->IsValidDrawbl(index, DrawableId))
			{
				u8 TextureId = 0; 
				u8 PaletteId = 0; 

				TextureId = CPedVariationPack::GetTexVar(pPedVarDebugPed, static_cast<ePedVarComp>(varSlotEnums[index]));
				PaletteId = CPedVariationPack::GetPaletteVar(pPedVarDebugPed, static_cast<ePedVarComp>(varSlotEnums[index]));
				
				formatf(str, sizeof(str), "SET_PED_COMPONENT_VARIATION(PedIndex, INT_TO_ENUM(PED_COMPONENT,%d), %d, %d, %d) //(%s)" , index, DrawableId, TextureId,PaletteId, varSlotNames[index] ); 
				Displayf("%s", str); 
			}
		}

		const CPedPropData& propData = pPedVarDebugPed->GetPedDrawHandler().GetPropData();
		for (s32 i = 0; i < MAX_PROPS_PER_PED; ++i)
		{
			eAnchorPoints anchor = propData.GetAnchor(i);
			if (anchor == ANCHOR_NONE)
				continue;
			
			s32 prop = propData.GetPropId(i);
			s32 tex = propData.GetTextureId(i);

			formatf(str, sizeof(str), "SET_PED_PROP_INDEX(PedIndex, INT_TO_ENUM(PED_PROP_POSITION,%d), %d, %d)", anchor, prop, tex);
			Displayf("%s", str);
		}
	}
}

CPedGroup* g_pGroupToAddPedTo = NULL;

void SetupPed(CPed* pPed)
{
	if( !pPed )
	{
		return;
	}

	TUNE_GROUP_BOOL(COVER_DEBUG, ONLY_BLINDFIRE, false);
	if (ONLY_BLINDFIRE)
	{
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatBlindFireChance, 1.0f);
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_BlindFireWhenInCover);
	}

	pPed->SetHealth(g_fStartingHealth);
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater, g_DrownsInWater );

	u32 iWeaponHash = pPed->GetDefaultUnarmedWeaponHash();
	const u32 i1Handed = CWeaponInfoManager::GetRandomOneHandedWeaponHash();
	const u32 i2Handed = CWeaponInfoManager::GetRandomTwoHandedWeaponHash();

	// Arm ped as specified in combo box
	switch(g_eArmament)
	{
	case eUnarmed:
		break;
	case e1Handed:
		iWeaponHash = i1Handed;
		break;
	case e2Handed:
		iWeaponHash = i2Handed;
		break;
	case eProjectile:
		iWeaponHash = CWeaponInfoManager::GetRandomProjectileWeaponHash();
		break;
	case eThrown:
		iWeaponHash = CWeaponInfoManager::GetRandomThrownWeaponHash();
		break;
	case eAssortedGuns:
	default:
		iWeaponHash = fwRandom::GetRandomTrueFalse() ? i1Handed : i2Handed;
		break;
	}

	if( iWeaponHash != 0 )
	{
		const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(iWeaponHash);
		if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
		{
			if(pPed->GetInventory())
				pPed->GetInventory()->AddWeaponAndAmmo(iWeaponHash, 1000);
			
			if (g_UnholsteredWeapon && pPed->GetWeaponManager())
			{
				pPed->GetWeaponManager()->EquipWeapon(iWeaponHash, -1, true);
			}
		}
	}

	if (!CPedVariationDebug::ms_UseDefaultRelationshipGroups)
	{
		atHashString groupName(pRelationshipGroupText[g_iRelationshipGroup]);
		CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(groupName);
		if( pGroup == NULL )
		{
			pGroup = CRelationshipManager::AddRelationshipGroup(groupName, RT_random);
		}
		if( pGroup != NULL )
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup( pGroup );
		}
	}

	if (g_MakeInvunerableAndStupid)
	{
		pPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_DEFAULT);
		pPed->SetBlockingOfNonTemporaryEvents(true);
		pPed->m_nPhysicalFlags.bNotDamagedByAnything = true;
	}
	else
	{
		// If this toggle is set then get ped to wander off after creation
		CTask* pDefaultTask;
		if (bWanderAfterCreate)
		{
			pDefaultTask = pPedVarDebugPed->ComputeDefaultTask(*pPedVarDebugPed);
		}
		else
		{
			pDefaultTask = rage_new CTaskDoNothing(-1);
		}

		if (g_SpawnWithProp)
		{
			const int taskType = pDefaultTask->GetTaskType();
			if(taskType == CTaskTypes::TASK_UNALERTED)
			{
				static_cast<CTaskUnalerted*>(pDefaultTask)->CreateProp();
			}
			else if(taskType == CTaskTypes::TASK_WANDER)
			{
				static_cast<CTaskWander*>(pDefaultTask)->CreateProp();
			}
		}
		// Add the default task to the ped
		pPed->GetPedIntelligence()->AddTaskDefault(pDefaultTask);
		// Possibly assign a scenario to this ped
		//	CScenarioManager::GiveScenarioToWanderingPed( pPed );
		const Vector3	pos		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const u32	modelIndex	= pPed->GetModelIndex();
		DefaultScenarioInfo scenarioInfo = CScenarioManager::GetDefaultScenarioInfoForRandomPed(pos, modelIndex);
		if(scenarioInfo.GetType() != DS_Invalid)
		{
			CScenarioManager::ApplyDefaultScenarioInfoForRandomPed(scenarioInfo, pPed);
		}
	}

	if( g_pGroupToAddPedTo )
	{
		if( g_pGroupToAddPedTo->GetGroupMembership()->GetLeader() == NULL )
		{
			g_pGroupToAddPedTo->GetGroupMembership()->SetLeader(pPed);
		}
		else
		{
			g_pGroupToAddPedTo->GetGroupMembership()->AddFollower(pPed);
		}

		g_pGroupToAddPedTo->Process();
	}

	if( g_GivePedTestHarnessTask )
	{
		CDebugScene::FocusEntities_Set(pPed,0);
		CPedDebugVisualiserMenu::TestHarnessCB();
	}
}



void SetArmedCombatDefaults(void)
{
	g_eArmament = eAssortedGuns;
	g_iRelationshipGroup = 1;
	g_iRelationship = 0;
	g_iOtherRelationshipGroup = 0;
	g_eGroup = eNoGroup;
	g_CreateExactGroupSize = true;
}

void SetHatesPlayerCombatDefaults(void)
{
	g_eArmament = eAssortedGuns;
	g_iRelationshipGroup = 1;
	g_iRelationship = ACQUAINTANCE_TYPE_PED_HATE + 1;
	g_iOtherRelationshipGroup = 0;
	g_eGroup = eNoGroup;
	g_CreateExactGroupSize = true;
}

void SetInPlayerGroupCombatDefaults(void)
{
	g_eArmament = eAssortedGuns;
	g_iRelationshipGroup = 0;
	g_iRelationship = 0;
	g_iOtherRelationshipGroup = 0;
	g_eGroup = ePlayersGroup;
	g_CreateExactGroupSize = true;
}

void SetupDefaultGroupRelationshipStuff()
{
	g_pGroupToAddPedTo = NULL;
	if( g_eGroup == eNewGroup )
	{
		s32 iNewGroup = CPedGroups::AddGroup(POPTYPE_RANDOM_AMBIENT);
		if( iNewGroup != -1 )
		{
			g_pGroupToAddPedTo = &CPedGroups::ms_groups[iNewGroup];
		}
	}
	else if(  g_eGroup == ePlayersGroup )
	{
		g_pGroupToAddPedTo = CGameWorld::FindLocalPlayer()->GetPedsGroup();
	}

	if(g_pGroupToAddPedTo && g_NeverLeaveGroup)
	{
		g_pGroupToAddPedTo->GetGroupMembership()->SetMaxSeparation(9999.0f, true);
	}

	if(g_pGroupToAddPedTo)
	{
		g_pGroupToAddPedTo->SetFormation(g_eFormation);
	}

	if( g_iRelationship != 0 )
	{
		atHashString groupName(pRelationshipGroupText[g_iRelationshipGroup]);
		atHashString otherName(pRelationshipGroupText[g_iOtherRelationshipGroup]);
		CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(groupName);
		CRelationshipGroup* pOtherGroup = CRelationshipManager::FindRelationshipGroup(otherName);
		if( pGroup == NULL )
		{
			pGroup = CRelationshipManager::AddRelationshipGroup(groupName, RT_random);
		}
		if( pOtherGroup == NULL )
		{
			pOtherGroup = CRelationshipManager::AddRelationshipGroup(groupName, RT_random);
		}

		if( pGroup != NULL && pOtherGroup != NULL)
		{
			pGroup->SetRelationship((eRelationType)(g_iRelationship - 1), pOtherGroup);
		}
	}
}

void createPedGrpsCB(void){

	const CPopGroupList& pedPopGroups = CPopCycle::GetPopGroups();
	for(u32 i=0; i<5; i++){
		if (pedGrpSelection[i] != -1){
			const CPopulationGroup& pedGroup = pedPopGroups.GetPedGroup(pedGrpSelection[i]);
			Displayf("  %s\n",pedGroup.GetName().GetCStr());
			for(u32 j=0; j< pedGroup.GetCount(); j++){
				Displayf("    %s\n", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(pedGroup.GetModelIndex(j)))) );
			}
		}
	}

	Displayf("---\n");

	strLocalIndex PedModelIndex;
	float	NewX,NewY,NewZ;
	u32	numGroupsInCrowd = 0;
	s32	i,j;
	s32	offset = crowdSize / 2;
	s32	availableGroups[5] = {-1,};
	s32	rndIdx = -1;

	for(i=0;i<5;i++){
		if (pedGrpSelection[i] != -1){
			availableGroups[numGroupsInCrowd]=pedGrpSelection[i];
			numGroupsInCrowd++;
		}
	}

	if( numGroupsInCrowd == 0 )
	{
		Assertf( 0, "No ped groups specified for crowd!" );
		return;
	}

	SetupDefaultGroupRelationshipStuff();

	s32 iNumberCreated = 0;
	s32 iPotentialSquaresLeft = crowdSize * crowdSize;

	// map to represent the probability intervals in 0..100 that each group is assigned
	u32 groupMap[6] = {0,};
	groupMap[0] = 0;
	for(i = 1; i<=5; i++){
		groupMap[i] = groupMap[i-1] + percentPedsFromEachGroup[i-1];
	}

	for(i=0;i<crowdSize;i++)
	{
		for(j=0;j<crowdSize;j++)
		{
			// select a group according to given percentages
			u32 rndGroupProb = fwRandom::GetRandomNumberInRange(0, 100);
			u32 rndGrpSelection = 0;

			for(u32 k = 0; k <=5; k++){
				if (groupMap[k] >= rndGroupProb){
					break;
				}
				rndGrpSelection = k;
			}

			// since total percent to create can be less than 100% then handle 'no create' correctly
			if (rndGrpSelection == 5){
				++iNumberCreated;
				continue;
			}

			// select a ped randomly from the desired group
			const CPopulationGroup& pedGroup = pedPopGroups.GetPedGroup(availableGroups[rndGrpSelection]);
			rndIdx = fwRandom::GetRandomNumberInRange(0, pedGroup.GetCount());
			PedModelIndex = pedGroup.GetModelIndex(rndIdx);

			// ensure that the model is loaded and ready for drawing for this ped
			fwModelId modelId(PedModelIndex);
			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				CStreaming::LoadAllRequestedObjects(true);
				Assert(CModelInfo::HaveAssetsLoaded(modelId));
			}

			// generate a location to create the ped from the camera position & orientation
			Vector3 destPos = camInterface::GetPos();
			Vector3 viewVector = (camInterface::GetFront());
			destPos.Add(viewVector*(distToCreateCrowdFromCamera+offset));		// create at position in front of current camera position

			if (crowdSpawnLocation.IsNonZero())
			{
				destPos = crowdSpawnLocation;
			}

			NewX = destPos.GetX();
			NewX += static_cast<float>(i - offset) * 1.5f;
			NewX += fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			NewY = destPos.GetY();
			NewY += static_cast<float>(j - offset) * 1.5f;
			NewY += fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			NewZ = destPos.GetZ();

			Vector3 vTestFromPos(NewX, NewY, camInterface::GetPos().z + 1.0f);
			WorldProbe::CShapeTestFixedResults<> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vTestFromPos, vTestFromPos - Vector3(0.0f, 0.0f, 1000.0f));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetExcludeEntity(NULL);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				physicsAssert(probeResult[0].GetHitDetected());
				NewZ = probeResult[0].GetHitPosition().z;
				NewZ += 1.0f;
				destPos.SetX(NewX);
				destPos.SetY(NewY);
				destPos.SetZ(NewZ);

				// If the crowd size to be specified is exactly as specified, make sure we have enough created, but not too many
				if ( (!g_CreateExactGroupSize || iNumberCreated < crowdSize ) &&
					( ( g_CreateExactGroupSize && iPotentialSquaresLeft == (crowdSize - iNumberCreated) )
					|| ( fwRandom::GetRandomNumberInRange(0.0f,1.0f) < 0.5f) ) )
				{
					pPedVarDebugPed = NULL;
					CreatePed(PedModelIndex.Get(), destPos, true);
					if (pPedVarDebugPed){
						//CPedVariationPack::SetVarRandom(pPedVarDebugPed, pPedVarDebugPed->GetPropData(), pPedVarDebugPed->GetVarData(), crowdSetFlags, crowdClearFlags);
						atFixedBitSet32 incs;
						atFixedBitSet32 excs;
						pPedVarDebugPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, &incs, &excs, NULL, PV_RACE_UNIVERSAL);
						CPedVariationDebug::CheckVariation(pPedVarDebugPed);
						//CPedPropsMgr::SetPedPropsRandomly(pPedVarDebugPed, 0.4f, 0.25f, PV_FLAG_NONE, PV_FLAG_NONE);
						CPedPopulation::EquipPed(pPedVarDebugPed, false, true, false, &incs, &excs, PV_FLAG_NONE, PV_FLAG_NONE);
						CPedVariationDebug::CheckVariation(pPedVarDebugPed);
						pPedVarDebugPed->PopTypeSet(POPTYPE_TOOL);
						SetupPed(pPedVarDebugPed);

						++iNumberCreated;
					}
				}
				--iPotentialSquaresLeft;
			}
		}
	}
}

void createCrowdCB(void){
	float	NewX,NewY,NewZ;
	u32	numTypesInCrowd = 0;
	s32	i,j;
	s32	offset = crowdSize / 2;
	s32	availableTypes[3] = {-1,};
	s32	rndPedId = -1;

	for(i=0;i<3;i++){
		if (crowdPedSelection[i] != 0){
			availableTypes[numTypesInCrowd]=pedSorts[crowdPedSelection[i]-1 + numPedsSkipped];
			numTypesInCrowd++;
		}
	}

	if( numTypesInCrowd == 0 )
	{
		Assertf( 0, "No pedtypes specified for crowd!" );
		return;
	}

	SetupDefaultGroupRelationshipStuff();

	s32 iNumberCreated = 0;
	s32 iPotentialSquaresLeft = crowdSize * crowdSize;

	for(i=0;i<crowdSize;i++)
	{
		for(j=0;j<crowdSize;j++)
		{
			rndPedId = fwRandom::GetRandomNumberInRange(0, numTypesInCrowd);
			//get the modelInfo associated with this ped ID
			fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
			atArray<CPedModelInfo*> pedTypesArray;
			pedModelInfoStore.GatherPtrs(pedTypesArray);

			CPedModelInfo& pedModelInfo = *pedTypesArray[ availableTypes[rndPedId] ];
			fwModelId modelId = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName());

			// ensure that the model is loaded and ready for drawing for this ped
			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
				CStreaming::LoadAllRequestedObjects(true);
				Assert(CModelInfo::HaveAssetsLoaded(modelId));
			}

			// generate a location to create the ped from the camera position & orientation
			Vector3 destPos = camInterface::GetPos();
			Vector3 viewVector = (camInterface::GetFront());
			destPos.Add(viewVector*(distToCreateCrowdFromCamera+offset));		// create at position in front of current camera position

			if (crowdSpawnLocation.IsNonZero())
			{
				destPos = crowdSpawnLocation;
			}

			if (crowdUniformDistribution >= 0.0f)
			{
				float xPos = float(-offset + i);
				float yPos = float(-offset + j);
				float xOffset = xPos * crowdUniformDistribution;
				float yOffset = yPos * crowdUniformDistribution;
				NewX = destPos.GetX() + xOffset;
				NewY = destPos.GetY() + yOffset;
				NewZ = destPos.GetZ();
			}
			else
			{
				NewX = destPos.GetX();
				NewX += static_cast<float>(i - offset) * 1.5f;
				NewX += fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
				NewY = destPos.GetY();
				NewY += static_cast<float>(j - offset) * 1.5f;
				NewY += fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
				NewZ = destPos.GetZ();
			}
//			if (NewZ <= -100.0f)
//			{
//				NewZ = WorldProbe::FindGroundZForCoord(NewX, NewY);
//			}

			Vector3 vTestFromPos(NewX, NewY, camInterface::GetPos().z + 1.0f);
			WorldProbe::CShapeTestFixedResults<> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vTestFromPos, vTestFromPos - Vector3(0.0f, 0.0f, 1000.0f));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetExcludeEntity(NULL);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				physicsAssert(probeResult[0].GetHitDetected());
				NewZ = probeResult[0].GetHitPosition().z;
				NewZ += 1.0f;
				destPos.SetX(NewX);
				destPos.SetY(NewY);
				destPos.SetZ(NewZ);

				// If the crowd size to be specified is exactly as specified, make sure we have enough created, but not too many
				if ( (!g_CreateExactGroupSize || iNumberCreated < crowdSize ) &&
					( ( g_CreateExactGroupSize && iPotentialSquaresLeft == (crowdSize - iNumberCreated) )
					|| ( fwRandom::GetRandomNumberInRange(0.0f,1.0f) < 0.5f) ) )
				{
					pPedVarDebugPed = NULL;
					CreatePed(modelId.GetModelIndex(), destPos, true);
					if (pPedVarDebugPed){
						//CPedVariationPack::SetVarRandom(pPedVarDebugPed, pPedVarDebugPed->GetPropData(), pPedVarDebugPed->GetVarData(), crowdSetFlags, crowdClearFlags);
						atFixedBitSet32 incs;
						atFixedBitSet32 excs;
						pPedVarDebugPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, &incs, &excs, NULL, PV_RACE_UNIVERSAL);
						CPedVariationDebug::CheckVariation(pPedVarDebugPed);
						//CPedPropsMgr::SetPedPropsRandomly(pPedVarDebugPed, 0.4f, 0.25f, PV_FLAG_NONE, PV_FLAG_NONE);
						CPedPopulation::EquipPed(pPedVarDebugPed, false, true, false, &incs, &excs, PV_FLAG_NONE, PV_FLAG_NONE);
						CPedVariationDebug::CheckVariation(pPedVarDebugPed);
						pPedVarDebugPed->PopTypeSet(bCreateAsRandomChar ? POPTYPE_RANDOM_AMBIENT : POPTYPE_TOOL);
						SetupPed(pPedVarDebugPed);

						++iNumberCreated;
					}
				}
				--iPotentialSquaresLeft;
			}

			CModelInfo::SetAssetsAreDeletable(modelId);
		}
	}
}

void CPedVariationDebug::CreateOneMeleeOpponentCB(void)
{
	// Generate a tentative location to create the ped from the camera position & orientation
	float	NewX,NewY,NewZ;
	Vector3 destPos = camInterface::GetPos();
	Vector3 viewVector = camInterface::GetFront();
	destPos.Add(viewVector*(3.0f));		// create at position in front of current camera position
	NewX = destPos.GetX() + fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	NewY = destPos.GetY() + fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	NewZ = destPos.GetZ();

	// Do a test probe against the world at the location to find the ground to set the ped on.
	Vector3 vTestFromPos(NewX, NewY, camInterface::GetPos().z + 1.0f);
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vTestFromPos, vTestFromPos - Vector3(0.0f, 0.0f, 1000.0f));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		physicsAssert(probeResult[0].GetHitDetected());
		// Set the actual position to create the ped at.
		NewZ = probeResult[0].GetHitPosition().z;
		NewZ += 1.0f;
		destPos.SetX(NewX);
		destPos.SetY(NewY);
		destPos.SetZ(NewZ);
	}

	// Pick a random ped type to use.
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	// Get the modelInfo associated with this ped type.
	CPedModelInfo* pPedModelInfo = NULL;
	if( pedSorts )
		pPedModelInfo = pedTypesArray[ pedSorts[ CPedVariationDebug::currPedNameSelection + numPedsSkipped ] ];
	else
		pPedModelInfo = pedTypesArray[ numPedsSkipped ];

	u32 PedModelIndex = CModelInfo::GetModelIdFromName(pPedModelInfo->GetModelName()).GetModelIndex();

	// Ensure that the model is loaded and ready for drawing for this ped.
	fwModelId pedInfoId((strLocalIndex(PedModelIndex)));
	if (!CModelInfo::HaveAssetsLoaded(pedInfoId))
	{
		CModelInfo::RequestAssets(pedInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	// Set the globals used to setup a new ped in the variation manager.
	g_eArmament					= eUnarmed;
	g_fStartingHealth			= 200.0f;
	g_eGroup					= eNoGroup;
	g_eFormation				= CPedFormationTypes::FORMATION_LOOSE;
	g_pGroupToAddPedTo			= NULL;
	g_iRelationshipGroup		= 0;
	g_iRelationship				= 0;
	g_iOtherRelationshipGroup	= 0;

	// Create the ped.
	CreatePed(PedModelIndex, destPos, true);
	pPedVarDebugPed->PopTypeSet(POPTYPE_TOOL);
	SetHatesPlayerCombatDefaults();
	SetupDefaultGroupRelationshipStuff();
	g_eArmament = eUnarmed;
	Assert( pPedVarDebugPed->GetPedIntelligence() );
	pPedVarDebugPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag( CCombatData::BF_AlwaysFight );
	SetupPed(pPedVarDebugPed);
}


void CPedVariationDebug::CreateOneBrainDeadMeleeOpponentCB(void)
{
	// Generate a tentative location to create the ped from the camera position & orientation
	float	NewX,NewY,NewZ;
	Vector3 destPos = camInterface::GetPos();
	Vector3 viewVector = camInterface::GetFront();
	destPos.Add(viewVector*(3.0f));		// create at position in front of current camera position
	NewX = destPos.GetX() + fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	NewY = destPos.GetY() + fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	NewZ = destPos.GetZ();

	// Do a test probe against the world at the location to find the ground to set the ped on.
	Vector3 vTestFromPos(NewX, NewY, camInterface::GetPos().z + 1.0f);
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vTestFromPos, vTestFromPos - Vector3(0.0f, 0.0f, 1000.0f));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		physicsAssert(probeResult[0].GetHitDetected());
		// Set the actual position to create the ped at.
		NewZ = probeResult[0].GetHitPosition().z;
		NewZ += 1.0f;
		destPos.SetX(NewX);
		destPos.SetY(NewY);
		destPos.SetZ(NewZ);
	}

	// Pick a random ped type to use.
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	// Get the modelInfo associated with this ped type.
	CPedModelInfo* pPedModelInfo = NULL;
	if( pedSorts )
		pPedModelInfo = pedTypesArray[ pedSorts[ CPedVariationDebug::currPedNameSelection + numPedsSkipped ] ];
	else
		pPedModelInfo = pedTypesArray[ numPedsSkipped ];

	u32 PedModelIndex = CModelInfo::GetModelIdFromName(pPedModelInfo->GetModelName()).GetModelIndex();

	// Ensure that the model is loaded and ready for drawing for this ped.
	fwModelId pedModelId((strLocalIndex(PedModelIndex)));
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	// Set the globals used to setup a new ped in the variation manager.
	g_eArmament					= eUnarmed;
	g_fStartingHealth			= CCombatMeleeDebug::sm_fBrainDeadPedsHealth;
	g_eGroup					= eNoGroup;
	g_eFormation				= CPedFormationTypes::FORMATION_LOOSE;
	g_pGroupToAddPedTo			= NULL;
	g_iRelationshipGroup		= 0;
	g_iRelationship				= 0;
	g_iOtherRelationshipGroup	= 0;

	// Create the ped.
	CreatePed(PedModelIndex, destPos, true);
	pPedVarDebugPed->PopTypeSet(POPTYPE_TOOL);
	SetHatesPlayerCombatDefaults();
	SetupDefaultGroupRelationshipStuff();
	g_eArmament = eUnarmed;
	SetupPed(pPedVarDebugPed);

	pPedVarDebugPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_DEFAULT);

	pPedVarDebugPed->SetBlockingOfNonTemporaryEvents( true);
	//if(pPedVarDebugPed->GetRagdollState() > RAGDOLL_STATE_ANIM)
	//{
	//	pPedVarDebugPed->SwitchToAnimated();
	//}
	//pPedVarDebugPed->SetRagdollState(RAGDOLL_STATE_ANIM_LOCKED);
}

//
void CPedVariationDebug::populateCarCB(int nForceCrowdSelection, int nNumberPeds, bool bMakeDumb)
{
	s32 i;
	u32 numTypesInCrowd = 0;
	s32 availableTypes[3] = {-1,};
	s32 rndPedId = -1;

	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

    if(NetworkUtils::IsNetworkCloneOrMigrating(CVehicleFactory::ms_pLastCreatedVehicle))
    {
        Assertf(0, "Trying to populate a car when it is not locally controlled!" );
        return;
    }

	for(i=0; i<CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetMaxSeats(); i++)
	{
		if(CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetPedInSeat(i) && !CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetPedInSeat(i)->IsAPlayerPed())
		{
			CPedFactory::GetFactory()->DestroyPed(CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetPedInSeat(i));
		}
	}

	// Make ped friendly with player, and part of his group
	if(nForceCrowdSelection == 2)
	{
		SetInPlayerGroupCombatDefaults();
		g_eArmament = e2Handed;
	}
	// Make ped hate the player, and part of a new group
	else if(nForceCrowdSelection == 3)
	{
		SetHatesPlayerCombatDefaults();
		g_eArmament = e2Handed;
	}

	SetupDefaultGroupRelationshipStuff();

	for(i=0;i<3;i++){
		if (pedSorts && crowdPedSelection[i] != 0 && (nForceCrowdSelection==-1 || nForceCrowdSelection==i))
		{
			availableTypes[numTypesInCrowd]=pedSorts[crowdPedSelection[i]-1 + numPedsSkipped];
			numTypesInCrowd++;
		}
	}

	if( numTypesInCrowd == 0 )
	{
		Assertf( 0, "No pedtypes specified for crowd!" );
		return;
	}

	// if there's already a player in the driver's seat then start from the 1st passenger
	if(CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetDriver() && CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetDriver()->IsPlayer())
		i=1;
	else
		i=0;

	s32 iSpecificSeat = -1;
	if ( nNumberPeds == 0)
	{
		nNumberPeds = CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetMaxSeats();

		TUNE_GROUP_BOOL(FLEE_EXIT_TUNE, FRONT_TWO_SEATS_ONLY, false);
		if (FRONT_TWO_SEATS_ONLY)
		{
			nNumberPeds = nNumberPeds > 2 ? 2 : nNumberPeds;
		}
		TUNE_GROUP_BOOL(FLEE_EXIT_TUNE, ONE_SEAT_ONLY, false);
		if (ONE_SEAT_ONLY)
		{
			TUNE_GROUP_INT(FLEE_EXIT_TUNE, SPECIFIC_SEAT, 0, 0, 10, 1);
			iSpecificSeat = SPECIFIC_SEAT;
			nNumberPeds = 1;
		}
	}

	for(; i<nNumberPeds; i++)
	{
		if( CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetPedInSeat(i)== NULL ||
			!CVehicleFactory::ms_pLastCreatedVehicle->GetSeatManager()->GetPedInSeat(i)->IsAPlayerPed() )
		{
			rndPedId = fwRandom::GetRandomNumberInRange(0, numTypesInCrowd);
			//get the modelInfo associated with this ped ID
			fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
			atArray<CPedModelInfo*> pedTypesArray;
			pedModelInfoStore.GatherPtrs(pedTypesArray);

			CPedModelInfo& pedModelInfo = *pedTypesArray[availableTypes[rndPedId]];
			fwModelId modelId = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName());

			// ensure that the model is loaded and ready for drawing for this ped
			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				CStreaming::LoadAllRequestedObjects(true);
			}

			CreatePed(modelId.GetModelIndex(), VEC3V_TO_VECTOR3(CVehicleFactory::ms_pLastCreatedVehicle->GetTransform().GetPosition()), true);
			//CPedVariationDebug::SetVarRandom(pPedVarDebugPed, pPedVarDebugPed->GetPropData(), pPedVarDebugPed->GetVarData(), pPedVarDebugPed->IsPlayer());
			
			if (pPedVarDebugPed)
			{
				atFixedBitSet32 incs;
				atFixedBitSet32 excs;

				// use same code as used to normally populate vehicles (so hats on bikes, no bags in cars etc.)
				if (CVehicleFactory::ms_pLastCreatedVehicle->InheritsFromBike()){
					// Make sure the PedVariation used does include any tagged 'job' components (should be helmets)
					pPedVarDebugPed->SetVarRandom(PV_FLAG_JOB, PV_FLAG_NONE, &incs, &excs, NULL, PV_RACE_UNIVERSAL);
				} else {
					// Make sure the PedVariation used doesn't contain any 'bulky' items (backpack, big afro etc)
					pPedVarDebugPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_BULKY, &incs, &excs, NULL, PV_RACE_UNIVERSAL);
				}
				
				CPedVariationDebug::CheckVariation(pPedVarDebugPed);
				CPedPropsMgr::SetPedPropsRandomly(pPedVarDebugPed, 0.4f, 0.25f, PV_FLAG_NONE, PV_FLAG_NONE, &incs, &excs);
				CPedVariationDebug::CheckVariation(pPedVarDebugPed);
				SetupPed(pPedVarDebugPed);

				pPedVarDebugPed->GetPedIntelligence()->AddTaskAtPriority(NULL, PED_TASK_PRIORITY_PRIMARY);
				pPedVarDebugPed->SetPedInVehicle(CVehicleFactory::ms_pLastCreatedVehicle, iSpecificSeat > -1 ? iSpecificSeat : i, CPed::PVF_IfForcedTestVehConversionCollision);
				aiTask* pTask = NULL;
				if (bMakeDumb)
				{
					pPedVarDebugPed->SetBlockingOfNonTemporaryEvents(true);
					pTask = rage_new CTaskDoNothing(-1);
				}
				else
				{
					pTask = pPedVarDebugPed->ComputeDefaultTask(*pPedVarDebugPed);
				}
				pPedVarDebugPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, pTask, PED_TASK_PRIORITY_DEFAULT,true);
				pPedVarDebugPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
			}
		}
	}
}

void populateCarWithDumbRandomsCB(void)
{
	CPedVariationDebug::populateCarCB(-1, 0, true);
}
void populateCarWithRandomsCB(void)
{
	CPedVariationDebug::populateCarCB(-1);
}
void populateCarWithCrowd0CB(void)
{
	CPedVariationDebug::populateCarCB(0);
}
void populateCarWithCrowd1CB(void)
{
	CPedVariationDebug::populateCarCB(1);
}

static atArray<CPedModelInfo*> g_typeArray;

////////////////////////////////////////////////////////////////////////////
static int CbCompareModelInfos(const void* pVoidA, const void* pVoidB)
{
	const s32* pA = (const s32*)pVoidA;
	const s32* pB = (const s32*)pVoidB;

	return stricmp(g_typeArray[*pA]->GetModelName(), g_typeArray[*pB]->GetModelName());
}

////////////////////////////////////////////////////////////////////////////
// name:	UpdateMloList
// purpose:
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CPedVariationDebug::UpdatePedList()
{
	u32 i=0;

	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	pedModelInfoStore.GatherPtrs(g_typeArray);

	numPedNames = g_typeArray.GetCount();

	delete []pedSorts;
	pedSorts = rage_new s32[numPedNames];

	for(i=0;i<numPedNames;i++) {
		pedSorts[i] = i;
	}

	qsort(pedSorts,numPedNames,sizeof(s32),CbCompareModelInfos);
	pedNames.Reset();
	emptyPedNames.Reset();

	pedNames.PushAndGrow(" - Empty -");
	emptyPedNames.PushAndGrow("");

	u32 count = 0;
	numPedsSkipped = 0;
	for(i=0;i<numPedNames;i++)
	{
		const char* pName = g_typeArray[pedSorts[i]]->GetModelName();

		bool allowCutscenePeds = true;
#if __DEV
		// Force the player to start in the world if that parameter is set
		XPARAM(allowCutscenePeds);
		if( PARAM_allowCutscenePeds.Get() )
		{
			allowCutscenePeds = true;
		}
#endif

		// skip cutscene peds
		if ((strncmp(pName,"CS_",3) != 0) || allowCutscenePeds){
			pedNames.PushAndGrow(g_typeArray[pedSorts[i]]->GetModelName());
			emptyPedNames.PushAndGrow(NULL);
			count++;
		} else {
			numPedsSkipped++;
		}
	}
	numPedNames = count;
	numUnskippedPedsAvail = numPedsAvail - numPedsSkipped;

	if (pPedCombo)
	{
		if(numPedNames == 0)
		{
			pPedCombo->UpdateCombo("Ped Model", &CPedVariationDebug::currPedNameSelection, numPedNames, NULL);

			pCrowdPedCombo->UpdateCombo("Crowd Ped 1", &crowdPedSelection[0], numPedNames+1, NULL);
			pCrowdPedCombo2->UpdateCombo("Crowd Ped 2", &crowdPedSelection[1], numPedNames+1, NULL);
			pCrowdPedCombo3->UpdateCombo("Crowd Ped 3", &crowdPedSelection[2], numPedNames+1, NULL);
		}
		else
		{
			// have to limit number of names sent to combo box
	//		if(numPedNames > 230)
	//			numPedNames = 230;
			// this breaks!
	//		pPedCombo->UpdateCombo("Ped name", &CPedVariationDebug::currPedNameSelection, numPedNames, &pedNames[1], PedIDChangeCB);

			// this works!
			pPedCombo->UpdateCombo("Ped Model", &CPedVariationDebug::currPedNameSelection, numPedNames, &emptyPedNames[1], PedIDChangeCB, NULL);

			pCrowdPedCombo->UpdateCombo("Crowd Ped 1", &crowdPedSelection[0], numPedNames, &emptyPedNames[0], NullCB, NULL);
			pCrowdPedCombo2->UpdateCombo("Crowd Ped 2", &crowdPedSelection[1], numPedNames, &emptyPedNames[0], NullCB, NULL);
			pCrowdPedCombo3->UpdateCombo("Crowd Ped 3", &crowdPedSelection[2], numPedNames, &emptyPedNames[0], NullCB, NULL);

			for(i=0;i<numPedNames;i++)
			{
				pPedCombo->SetString(i, pedNames[i+1]);

				pCrowdPedCombo->SetString(i, pedNames[i]);
				pCrowdPedCombo2->SetString(i, pedNames[i]);
				pCrowdPedCombo3->SetString(i, pedNames[i]);
			}
		}
	}

	g_typeArray.Reset();
}

void LimitPercentagesCB(void){

	static u32 oldPercents[5] = {100,0,0,0,0};
	u32 sum = 0;

	for (u32 i=0; i<5; i++){
		sum += percentPedsFromEachGroup[i];
	}

	if (sum <= 100){
		// allow this change
		for (u32 i=0; i<5; i++){
			oldPercents[i] = percentPedsFromEachGroup[i];
		}
	} else {
		// don't allow this change
		for (u32 i=0; i<5; i++){
			percentPedsFromEachGroup[i] = oldPercents[i];
		}
	}
}

void UpdateCompVarInfoIdxCB()
{
	UpdateVarComponentCB();
}

bool CPedVariationDebug::SetupWidgets(bkBank& bank)
{
	pedNames.Reset();
	pedNames.PushAndGrow("Inactive");
	pedNames.PushAndGrow("Activate");

	//-- Ped type group -- //
	{
		bank.PushGroup("Ped Type",false);
			bank.AddSlider("No. ped types available", &numUnskippedPedsAvail, 1, 10, 0);
			pPedCombo = bank.AddCombo("Ped Model", &CPedVariationDebug::currPedNameSelection, 2, &pedNames[0], UpdatePedList);
			bank.AddSlider("Ped model ID:", &pedInfoIndex, 0, 1000, 0);
			bank.AddButton("Create Ped",CreatePedCB);
			bank.AddButton("Create Next Ped",CreateNextPedCB);
			bank.AddButton("Validate All Peds",ValidateAllPedsCB);
			bank.AddButton("Render all ped components",PedComponentCheckCB);
			bank.AddButton("Destroy debug ped", DestroyPedCB);
			bank.AddButton("Destroy all peds except player", DestroyAllPedsCB);
			bank.AddToggle("Assign script brains", &CPedVariationDebug::assignScriptBrains);
			bank.AddButton("Create ped from selected",CreatePedFromSelectedCB);
			bank.AddToggle("Stall for asset load", &CPedVariationDebug::stallForAssetLoad);
			bank.AddButton("Clone ped", ClonePedCB);
			bank.AddButton("Clone ped to last ped", ClonePedToLastCB);
			bank.AddToggle("Link clones", &headBlendLinkClones);
		bank.PopGroup();
	}

	/// -- Ped variations Group -- //
	// widget setup for variations
	{
		bank.PushGroup("Ped Variations",false);
	
			bank.AddSlider("Total variations available", &totalVariations, 0, 10, 0);
	
			bank.AddText("Drawable name:",&varDrawablName[0], WIDGET_NAME_LENGTH, true);
			bank.AddText("Drawable variation name:", &varDrawablAltName[0], WIDGET_NAME_LENGTH, true);
			bank.AddText("Texture name:",&varTexName[0], WIDGET_NAME_LENGTH, true);
			bank.AddText("DLC Information:",(char*)dlcCompInformation,RAGE_MAX_PATH,true);
	
			const char* varSlotList[128];
			for (u32 i=0; i<PV_MAX_COMP; i++)
			{
				varSlotList[i] = varSlotNames[i];
			}
	
			bank.AddCombo("Component", &varComponentId, (PV_MAX_COMP), varSlotList, UpdateVarComponentCB);
	
			bank.AddSlider("Max Drawable:", &maxDrawables, 0, MAX_DRAWABLES, 0);
			pVarDrawablIdSlider = bank.AddSlider("Drawable Id", &varDrawableId, 0, MAX_DRAWABLES, 1, UpdateVarDrawblCB);

			bank.AddSlider("Max Drawable Alternatives:", &maxDrawableAlts, 0, MAX_DRAWABLES, 0);
			pVarDrawablAltIdSlider = bank.AddSlider("Drawable Alt Id", &varDrawableAltId, 0, MAX_DRAWABLES, 1, UpdateVarDrawblCB);
	
			bank.AddSlider("Max Texture:", &maxTextures, 0, MAX_TEXTURES, 0);
			pVarTexIdSlider = bank.AddSlider("Texture Id", &varTextureId, 0, MAX_TEXTURES, 1, UpdateVarTexCB);
	
			bank.AddSlider("Max Palette:", &maxPalettes, 0, 15, 0);
			pVarPaletteIdSlider =  bank.AddSlider("Palette Id", &varPaletteId, 0, 15, 1, UpdateVarPaletteCB);
	
			bank.AddToggle("Randomize new ped variations",&bRandomizeVars);
			bank.AddToggle("Cycle new ped variations",&bCycleVars);
	
			bank.AddButton("Cycle Through Variations",CyclePedCB);
			bank.AddCombo("Race", (int*)&g_eRace, PV_RACE_MAX_TYPES, pRaceText, NullCB);
			bank.AddButton("Random Variation",RandomizePedCB);
			
			bank.AddButton("Print Script Command", StoreScriptCommandCB); 
			bank.AddToggle("Display Ped Variation",&bDisplayVarData);
			bank.AddToggle("Display Ped Metadata", &bDisplayPedMetadata);

			bank.AddButton("Send to Workbench (SHIFT-E)", EditMetadataInWorkbenchCB);

			bank.AddToggle("Force HD assets", &bForceHdAssets);
			bank.AddToggle("Force LD assets", &bForceLdAssets);
			bank.AddToggle("Display Ped Streaming Requests", &CPedVariationDebug::displayStreamingRequests);

			bank.PushGroup("Selection sets", false);
				varSetCombo = bank.AddCombo("Sets", &varSetId, varNumSets, varSetList, UpdateVarSetCB);
			bank.PopGroup();
		bank.PopGroup();
	}

	//-- Rentacrowd group -- //
		{
			bank.PushGroup("RentaCrowd(tm)",false);
	
			// can't put this in rentacrowd group - causes an assert when try and update it later...
			pCrowdPedCombo = bank.AddCombo("Crowd Ped 1", &crowdPedSelection[0], 2, &pedNames[0], UpdatePedList);
			pCrowdPedCombo2 = bank.AddCombo("Crowd Ped 2", &crowdPedSelection[1], 2, &pedNames[0], UpdatePedList);
			pCrowdPedCombo3 = bank.AddCombo("Crowd Ped 3", &crowdPedSelection[2], 2, &pedNames[0], UpdatePedList);
	
			bank.PushGroup("Advanced...",false);
				bank.AddSlider("'Clear' restrict flags", &crowdClearFlags, 0, 0xffff, 1);
				bank.AddSlider("'Set' restrict flags", &crowdSetFlags, 0, 0xffff, 1);

				bank.AddButton("Set armed ped defaults", &SetArmedCombatDefaults);
				bank.AddButton("Set Hates Player defaults", &SetHatesPlayerCombatDefaults);
				bank.AddButton("Set In Player group defaults", &SetInPlayerGroupCombatDefaults);
				bank.AddCombo("Armament", (int*)&g_eArmament, eMaxArmament, pArmamentText, NullCB );
				bank.AddToggle("Use Default Relationship Groups",&CPedVariationDebug::ms_UseDefaultRelationshipGroups);
				bank.AddCombo("Rel group", (int*)&g_iRelationshipGroup, 9, pRelationshipGroupText, NullCB );
				bank.AddCombo("Add relationship", (int*)&g_iRelationship, 7, pRelationshipText, NullCB );
				bank.AddCombo("Other Rel group", (int*)&g_iOtherRelationshipGroup, 9, pRelationshipGroupText, NullCB );
				bank.AddSlider( "Starting health", &g_fStartingHealth, 0.0f, 900.0f, 1.0f );
				bank.AddCombo("Group", (int*)&g_eGroup, eGroupCount, pGroupText, NullCB );
#if __DEV
				bank.AddCombo("Formation", (int*)&g_eFormation, CPedFormationTypes::NUM_FORMATION_TYPES, CPedFormation::sz_PedFormationTypes, NullCB );
#endif
				bank.AddToggle("Create exact crowd size",&g_CreateExactGroupSize);
				bank.AddToggle("Never leave group due to distance",&g_NeverLeaveGroup);
				bank.AddToggle("Drowns in water",&g_DrownsInWater);
				bank.AddToggle("Create ped with weapon unholstered",&g_UnholsteredWeapon);
				bank.AddToggle("Make Invunerable And Stupid",&g_MakeInvunerableAndStupid);
				bank.AddToggle("Let Invunerable peds be set on fire",&ms_bLetInvunerablePedsBeAffectedByFire);
				bank.AddToggle("Spawn with random prop", &g_SpawnWithProp);
				bank.AddToggle("Give Ped test harness task",&g_GivePedTestHarnessTask);
			bank.PopGroup();
	
			bank.AddSlider("Crowd size", &crowdSize, 1, 25, 1);
			bank.AddButton("Spawn crowd", &createCrowdCB);
			bank.AddButton("RentaCarFull", &populateCarWithRandomsCB);
	
			bank.PopGroup();
		}

		//-- pedgrp group -- //
		{
			if (CPopCycle::GetPopGroups().GetPedCount()>0)
			{
				bank.PushGroup("ped group",false);

				const CPopGroupList& pedPopGroups = CPopCycle::GetPopGroups();
	
				for(u32 i=0; i<pedPopGroups.GetPedCount(); i++){
					pedGrpNames.PushAndGrow(pedPopGroups.GetPedGroup(i).GetName().GetCStr());
				}

				// can't put this in rentacrowd group - causes an assert when try and update it later...
				bank.AddCombo("Ped group 1", &pedGrpSelection[0], pedGrpNames.GetCount(), &pedGrpNames[0]);
				bank.AddSlider("% from group 1", &percentPedsFromEachGroup[0], 0, 100, 10, LimitPercentagesCB );
				bank.AddCombo("Ped group 2", &pedGrpSelection[1], pedGrpNames.GetCount(), &pedGrpNames[0]);
				bank.AddSlider("% from group 2", &percentPedsFromEachGroup[1], 0, 100, 10, LimitPercentagesCB );			
				bank.AddCombo("Ped group 3", &pedGrpSelection[2], pedGrpNames.GetCount(), &pedGrpNames[0]);
				bank.AddSlider("% from group 3", &percentPedsFromEachGroup[2], 0, 100, 10, LimitPercentagesCB );
				bank.AddCombo("Ped group 4", &pedGrpSelection[3], pedGrpNames.GetCount(), &pedGrpNames[0]);
				bank.AddSlider("% from group 4", &percentPedsFromEachGroup[3], 0, 100, 10, LimitPercentagesCB );
				bank.AddCombo("Ped group 5", &pedGrpSelection[4], pedGrpNames.GetCount(), &pedGrpNames[0]);
				bank.AddSlider("% from group 5", &percentPedsFromEachGroup[4], 0, 100, 10, LimitPercentagesCB );

				bank.AddSlider("Crowd size", &crowdSize, 1, 25, 1);
				bank.AddButton("Spawn crowd", &createPedGrpsCB);
			}

			bank.PopGroup();
		}

	bank.PushGroup("Scaling (maybe unstable!)", false);
		bank.AddSlider("Scale", &scaleFactor, -0.03f, 0.03f, 0.001f, UpdateScaleCB);
//		bank.AddSlider("Height scale", &heightScale, 0.98f, 1.02f, 0.001f, UpdateScaleCB);
	bank.PopGroup();

	bank.AddToggle("Cycle New Ped Types",&bCycleTypeIds);
	bank.AddToggle("Wander after creation",&bWanderAfterCreate);
	bank.AddToggle("Create as mission char",&bCreateAsMissionChar);
	bank.AddToggle("Create as random char",&bCreateAsRandomChar);
	bank.AddToggle("Persistent after creation",&bPersistAfterCreate);
	bank.AddToggle("Disable diffuse textures",&bDisableDiffuse);
	bank.AddToggle("Display ped names", &bDisplayName);
	bank.AddToggle("Display fade value", &bDisplayFadeValue);
	bank.AddToggle("Display var data", &bDisplayVarData);
	bank.AddToggle("Display memory breakdown",&bDisplayMemoryBreakdown);
	bank.AddToggle("Debug output dependency names",&bDebugOutputDependencyNames);
	bank.AddToggle("Drowns instantly in water",&bDrownsInstantlyInWater);

	bank.AddToggle("Toggle AllowPeds",			&gbAllowPedGeneration);
	bank.AddToggle("Toggle AllowAmbientPeds",	&gbAllowAmbientPedGeneration);
	bank.AddToggle("Toggle NoRagdolls",			&CPedPopulation::ms_bAllowRagdolls);
	bank.AddToggle("Toggle AllowScriptBrains",	&gbAllowScriptBrains);

	CPedCapsuleInfoManager::AddWidgets(bank);

	//-- Head blending -- //
	{
		s32 numBlemishes = 0;
		s32 numFacialHair = 0;
		s32 numEyebrow = 0;
		s32 numAging = 0;
		s32 numMakeup = 0;
		s32 numBlusher = 0;
		s32 numDamage = 0;
		s32 numBaseDetail = 0;
		s32 numSkinDetail1 = 0;
		s32 numSkinDetail2 = 0;
		s32 numBody1 = 0;
		s32 numBody2 = 0;
		s32 numBody3 = 0;
		if (CPedVariationStream::GetFacialOverlays().m_blemishes.GetCount() > 0)
			numBlemishes = CPedVariationStream::GetFacialOverlays().m_blemishes.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_facialHair.GetCount() > 0)
			numFacialHair = CPedVariationStream::GetFacialOverlays().m_facialHair.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_eyebrow.GetCount() > 0)
			numEyebrow = CPedVariationStream::GetFacialOverlays().m_eyebrow.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_aging.GetCount() > 0)
			numAging = CPedVariationStream::GetFacialOverlays().m_aging.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_makeup.GetCount() > 0)
			numMakeup = CPedVariationStream::GetFacialOverlays().m_makeup.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_blusher.GetCount() > 0)
			numBlusher = CPedVariationStream::GetFacialOverlays().m_blusher.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_damage.GetCount() > 0)
			numDamage = CPedVariationStream::GetFacialOverlays().m_damage.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_baseDetail.GetCount() > 0)
			numBaseDetail = CPedVariationStream::GetFacialOverlays().m_baseDetail.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_skinDetail1.GetCount() > 0)
			numSkinDetail1 = CPedVariationStream::GetFacialOverlays().m_skinDetail1.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_skinDetail2.GetCount() > 0)
			numSkinDetail2 = CPedVariationStream::GetFacialOverlays().m_skinDetail2.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_bodyOverlay1.GetCount() > 0)
			numBody1 = CPedVariationStream::GetFacialOverlays().m_bodyOverlay1.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_bodyOverlay2.GetCount() > 0)
			numBody2 = CPedVariationStream::GetFacialOverlays().m_bodyOverlay2.GetCount();
		if (CPedVariationStream::GetFacialOverlays().m_bodyOverlay3.GetCount() > 0)
			numBody3 = CPedVariationStream::GetFacialOverlays().m_bodyOverlay3.GetCount();

		bank.PushGroup("Head blending");
		bank.AddToggle("Create as parent (beauty/ugly blend)", &headBlendCreateAsParent);
		bank.AddSlider("Unsharp Pass", &MeshBlendManager::ms_unsharpPass, 0.f, 1.f, 0.1f, HeadBlendForceTextureBlendCB);
		bank.AddButton("Refresh", HeadBlendRefreshCB);
		headBlendHeadGeomCombo1 = bank.AddCombo("Parent geometry 1", &headBlendHeadGeom1, 0, NULL, HeadBlendComboChangeCB);
		headBlendHeadTexCombo1 = bank.AddCombo("Parent texture 1", &headBlendHeadTex1, 0, NULL, HeadBlendComboChangeCB);
		headBlendHeadGeomCombo2 = bank.AddCombo("Parent geometry 2", &headBlendHeadGeom2, 0, NULL, HeadBlendComboChangeCB);
		headBlendHeadTexCombo2 = bank.AddCombo("Parent texture 2", &headBlendHeadTex2, 0, NULL, HeadBlendComboChangeCB);
		headBlendHeadGeomCombo3 = bank.AddCombo("Genetic variation geometry", &headBlendHeadGeom3, 0, NULL, HeadBlendComboChangeCB);
		headBlendHeadTexCombo3 = bank.AddCombo("Genetic variation  texture", &headBlendHeadTex3, 0, NULL, HeadBlendComboChangeCB);
		bank.AddSlider("Geometry blend", &headBlendGeomBlend, 0.f, 1.f, 0.1f, HeadBlendSliderChangeCB);
		bank.AddSlider("Texture blend", &headBlendTexBlend, 0.f, 1.f, 0.1f, HeadBlendSliderChangeCB);
		bank.AddSlider("Geom & Tex blend", &headBlendGeomAndTexBlend, 0.f, 1.f, 0.1f, HeadBlendGeomAndTexSliderChangeCB);
		bank.AddSlider("Genetic variation blend", &headBlendVarBlend, 0.f, 1.f, 0.1f, HeadBlendSliderChangeCB);

		bank.AddSlider("Base detail overlay", &headBlendOverlayBaseDetail, 0, numBaseDetail, 1, HeadBlendOverlayBaseDetailChangeCB);
		bank.AddSlider("Base detail overlay blend", &headBlendOverlayBaseDetailBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBaseDetailChangeCB);
		bank.AddSlider("Base detail overlay normal blend", &headBlendOverlayBaseDetailNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBaseDetailChangeCB);

		bank.AddSlider("Blemishes overlay", &headBlendOverlayBlemishes, 0, numBlemishes, 1, HeadBlendOverlayBlemishesChangeCB);
		bank.AddSlider("Blemishes overlay blend", &headBlendOverlayBlemishesBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBlemishesChangeCB);
		bank.AddSlider("Blemishes overlay normal blend", &headBlendOverlayBlemishesNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBlemishesChangeCB);

		bank.AddSlider("Skin detail 1", &headBlendOverlaySkinDetail1, 0, numSkinDetail1, 1, HeadBlendOverlaySkinDetail1ChangeCB);
		bank.AddSlider("Skin detail 1 blend", &headBlendOverlaySkinDetail1Blend, 0.f, 1.f, 0.1f, HeadBlendOverlaySkinDetail1ChangeCB);
		bank.AddSlider("Skin detail 1 normal blend", &headBlendOverlaySkinDetail1NormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlaySkinDetail1ChangeCB);
		bank.AddSlider("Skin detail 1 tint", &headBlendOverlaySkinDetail1Tint, 0, MeshBlendManager::GetRampCount(RT_MAKEUP) - 1, 1, HeadBlendOverlaySkinDetail1ChangeCB);
		bank.AddSlider("Skin detail 1 second tint", &headBlendOverlaySkinDetail1Tint2, 0, MeshBlendManager::GetRampCount(RT_MAKEUP) - 1, 1, HeadBlendOverlaySkinDetail1ChangeCB);

		bank.AddSlider("Skin detail 2", &headBlendOverlaySkinDetail2, 0, numSkinDetail2, 1, HeadBlendOverlaySkinDetail2ChangeCB);
		bank.AddSlider("Skin detail 2 blend", &headBlendOverlaySkinDetail2Blend, 0.f, 1.f, 0.1f, HeadBlendOverlaySkinDetail2ChangeCB);
		bank.AddSlider("Skin detail 2 normal blend", &headBlendOverlaySkinDetail2NormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlaySkinDetail2ChangeCB);

		bank.AddSlider("Facial hair overlay", &headBlendOverlayFacialHair, 0, numFacialHair, 1, HeadBlendOverlayFacialHairChangeCB);
		bank.AddSlider("Facial hair overlay blend", &headBlendOverlayFacialHairBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayFacialHairChangeCB);
		bank.AddSlider("Facial hair overlay normal blend", &headBlendOverlayFacialHairNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayFacialHairChangeCB);
		bank.AddSlider("Facial hair tint", &headBlendOverlayFacialHairTint, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayFacialHairChangeCB);
		bank.AddSlider("Facial hair second tint", &headBlendOverlayFacialHairTint2, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayFacialHairChangeCB);

		bank.AddSlider("Eyebrow overlay", &headBlendOverlayEyebrow, 0, numEyebrow, 1, HeadBlendOverlayEyebrowChangeCB);
		bank.AddSlider("Eyebrow overlay blend", &headBlendOverlayEyebrowBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayEyebrowChangeCB);
		bank.AddSlider("Eyebrow overlay normal blend", &headBlendOverlayEyebrowNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayEyebrowChangeCB);
		bank.AddSlider("Eyebrow tint", &headBlendOverlayEyebrowTint, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayEyebrowChangeCB);
		bank.AddSlider("Eyebrow second tint", &headBlendOverlayEyebrowTint2, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayEyebrowChangeCB);

		bank.AddSlider("Aging overlay", &headBlendOverlayAging, 0, numAging, 1, HeadBlendOverlayAgingChangeCB);
		bank.AddSlider("Aging overlay blend", &headBlendOverlayAgingBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayAgingChangeCB);
		bank.AddSlider("Aging overlay normal blend", &headBlendOverlayAgingNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayAgingChangeCB);

		bank.AddSlider("Makeup overlay", &headBlendOverlayMakeup, 0, numMakeup, 1, HeadBlendOverlayMakeupChangeCB);
		bank.AddSlider("Makeup overlay blend", &headBlendOverlayMakeupBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayMakeupChangeCB);
		bank.AddSlider("Makeup overlay normal blend", &headBlendOverlayMakeupNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayMakeupChangeCB);

		bank.AddSlider("Blusher overlay", &headBlendOverlayBlusher, 0, numMakeup, 1, HeadBlendOverlayBlusherChangeCB);
		bank.AddSlider("Blusher overlay blend", &headBlendOverlayBlusherBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBlusherChangeCB);
		bank.AddSlider("Blusher overlay normal blend", &headBlendOverlayBlusherNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBlusherChangeCB);
		bank.AddSlider("Blusher tint", &headBlendOverlayBlusherTint, 0, MeshBlendManager::GetRampCount(RT_MAKEUP) - 1, 1, HeadBlendOverlayBlusherChangeCB);
		bank.AddSlider("Blusher second tint", &headBlendOverlayBlusherTint2, 0, MeshBlendManager::GetRampCount(RT_MAKEUP) - 1, 1, HeadBlendOverlayBlusherChangeCB);

		bank.AddSlider("Damage overlay", &headBlendOverlayDamage, 0, numDamage, 1, HeadBlendOverlayDamageChangeCB);
		bank.AddSlider("Damage overlay blend", &headBlendOverlayDamageBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayDamageChangeCB);
		bank.AddSlider("Damage overlay normal blend", &headBlendOverlayDamageNormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayDamageChangeCB);

		bank.PushGroup("Body overlays");
			bank.AddSlider("Body 1", &headBlendOverlayBody1, 0, numBody1, 1, HeadBlendOverlayBody1ChangeCB);
			bank.AddSlider("Body 1 blend", &headBlendOverlayBody1Blend, 0.f, 1.f, 0.1f, HeadBlendOverlayBody1ChangeCB);
			bank.AddSlider("Body 1 normal blend", &headBlendOverlayBody1NormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBody1ChangeCB);
			bank.AddSlider("Body 1 tint", &headBlendOverlayBody1Tint, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayBody1ChangeCB);
			bank.AddSlider("Body 1 second tint", &headBlendOverlayBody1Tint2, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayBody1ChangeCB);

			bank.AddSlider("Body 2", &headBlendOverlayBody2, 0, numBody2, 1, HeadBlendOverlayBody2ChangeCB);
			bank.AddSlider("Body 2 blend", &headBlendOverlayBody2Blend, 0.f, 1.f, 0.1f, HeadBlendOverlayBody2ChangeCB);
			bank.AddSlider("Body 2 normal blend", &headBlendOverlayBody2NormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBody2ChangeCB);
			bank.AddSlider("Body 2 tint", &headBlendOverlayBody2Tint, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayBody2ChangeCB);
			bank.AddSlider("Body 2 second tint", &headBlendOverlayBody2Tint2, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayBody2ChangeCB);

			bank.AddSlider("Body 3", &headBlendOverlayBody3, 0, numBody3, 1, HeadBlendOverlayBody3ChangeCB);
			bank.AddSlider("Body 3 blend", &headBlendOverlayBody3Blend, 0.f, 1.f, 0.1f, HeadBlendOverlayBody3ChangeCB);
			bank.AddSlider("Body 3 normal blend", &headBlendOverlayBody3NormBlend, 0.f, 1.f, 0.1f, HeadBlendOverlayBody3ChangeCB);
			bank.AddSlider("Body 3 tint", &headBlendOverlayBody3Tint, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayBody3ChangeCB);
			bank.AddSlider("Body 3 second tint", &headBlendOverlayBody3Tint2, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendOverlayBody3ChangeCB);
		bank.PopGroup();

		bank.AddSlider("Hair tint index", &headBlendHairTintIndex, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendHairRampChangeCB);
		bank.AddSlider("Hair tint second index", &headBlendHairTintIndex2, 0, MeshBlendManager::GetRampCount(RT_HAIR) - 1, 1, HeadBlendHairRampChangeCB);

		bank.AddColor("Palette RGB color 0", &headBlendPaletteRgb0, HeadBlendPaletteRgbCB);
		bank.AddColor("Palette RGB color 1", &headBlendPaletteRgb1, HeadBlendPaletteRgbCB);
		bank.AddColor("Palette RGB color 2", &headBlendPaletteRgb2, HeadBlendPaletteRgbCB);
		bank.AddColor("Palette RGB color 3", &headBlendPaletteRgb3, HeadBlendPaletteRgbCB);
		bank.AddToggle("Enable palette RGB", &headBlendPaletteRgbEnabled, HeadBlendPaletteRgbCB);
		bank.AddButton("Finalize blend", HeadBlendFinalizeCB);
		bank.AddToggle("Show debug textures", &MeshBlendManager::ms_showBlendTextures);
		bank.AddToggle("Show debug text", &MeshBlendManager::ms_showDebugText);
		bank.AddToggle("Show palette", &MeshBlendManager::ms_showPalette);
		bank.AddSlider("Palette scale", &MeshBlendManager::ms_paletteScale, 0.f, 10.f, 0.1f);
        headBlendEyeColorSlider = bank.AddSlider("Eye color", &headBlendEyeColor, -1, MESHBLENDMANAGER.GetNumEyeColors() - 1, 1, HeadBlendEyeColorChangeCB);

		bank.PushGroup("2nd gen blends");
		bank.AddToggle("Enable", &headBlendSecondGenEnable);
		bank.AddSlider("Parent geom blend", &headBlendSecondGenGeomBlend, 0.f, 1.f, 0.1f, HeadBlendUpdateSecondGenBlendCB);
		bank.AddSlider("Parent tex blend", &headBlendSecondGenTexBlend, 0.f, 1.f, 0.1f, HeadBlendUpdateSecondGenBlendCB);
        bank.AddButton("Assign parent 0", HeadBlendAssignParent0CB);
        bank.AddButton("Assign parent 1", HeadBlendAssignParent1CB);
        bank.AddButton("Clear parents", HeadBlendClearParentsCB);
        bank.AddButton("Create 2nd gen child", HeadBlendCreateSecondGenChildCB);
        bank.AddButton("Update 2nd gen child", HeadBlendUpdateSecondGenChildCB);
		bank.PopGroup();

		bank.PushGroup("Micro morphs");
		bank.AddSlider("Nose width", &microMorphNoseWidthBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Nose height", &microMorphNoseHeightBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Nose length", &microMorphNoseLengthBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Nose profile", &microMorphNoseProfileBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Nose tip", &microMorphNoseTipBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Nose broken", &microMorphNoseBrokeBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Brow height", &microMorphBrowHeightBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Brow depth", &microMorphBrowDepthBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Cheek height", &microMorphCheekHeightBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Cheek depth", &microMorphCheekDepthBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Cheek puffed", &microMorphCheekPuffedBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Eye size", &microMorphEyesSizeBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Lip size", &microMorphLipsSizeBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Jaw width", &microMorphJawWidthBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Jaw roundness", &microMorphJawRoundBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Chin Height", &microMorphChinHeightBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Chin depth", &microMorphChinDepthBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Chin pointed", &microMorphChinPointedBlend, -1.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Bum chin", &microMorphChinBumBlend, 0.f, 1.f, 0.1f, UpdateMicroMorphsCB);
		bank.AddSlider("Male neck fix", &microMorphNeckMaleBlend, 0.f, 1.f, 0.1f, UpdateMicroMorphsCB);
        bank.AddButton("Reset values", ResetMicroMorphsCB);
		bank.AddToggle("Bypass", &MeshBlendManager::ms_bypassMicroMorphs, BypassMicroMorphsCB);
		bank.PopGroup();

		bank.AddToggle("Identify non-finalized peds", &MeshBlendManager::ms_showNonFinalizedPeds);
		bank.PopGroup();
	}

	return true;
}

void CPedVariationDebug::HeadBlendSliderChangeCB()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed || !pPedVarDebugPed->GetPedDrawHandler().GetPedRenderGfx())
		return;

	rmcDrawable* currentHead = pPedVarDebugPed->GetPedDrawHandler().GetPedRenderGfx()->GetDrawable(PV_COMP_HEAD);
	rmcDrawable* head1 = pPedVarDebugPed->GetPedDrawHandler().GetPedRenderGfx()->GetHeadBlendDrawable(HBS_HEAD_0);
	rmcDrawable* head2 = pPedVarDebugPed->GetPedDrawHandler().GetPedRenderGfx()->GetHeadBlendDrawable(HBS_HEAD_1);

	if (!currentHead || !head1 || !head2)
		return;

	pPedVarDebugPed->SetNewHeadBlendValues(headBlendGeomBlend, headBlendTexBlend, headBlendVarBlend);

	// update child if the changed ped happens to have one
	if (pPedVarDebugPed == headBlendParent0 || pPedVarDebugPed == headBlendParent1)
	{
		if (headBlendSecondGenChild)
		{
			headBlendSecondGenChild->UpdateChildBlend();
		}
	}
}

void CPedVariationDebug::HeadBlendGeomAndTexSliderChangeCB()
{
	headBlendGeomBlend = headBlendTexBlend = headBlendGeomAndTexBlend;
	HeadBlendSliderChangeCB();
}

char s_headBlendDrawableNames[MAX_DRAWABLES][MAX_NAME_LEN] = {0};
const char* s_headBlendDrawableNamePtrs[MAX_DRAWABLES] = {0};
char s_headBlendTextureNames[MAX_DRAWABLES][MAX_NAME_LEN] = {0};
const char* s_headBlendTextureNamePtrs[MAX_TEXTURES] = {0};
void CPedVariationDebug::HeadBlendComboChangeCB()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
	{
		headBlendHeadGeom1 = headBlendHeadTex1 = headBlendHeadGeom2 = headBlendHeadTex2 = headBlendHeadGeom3 = headBlendHeadTex3 = -1;
		if (headBlendHeadGeomCombo1)
			headBlendHeadGeomCombo1->UpdateCombo("Parent geometry 1", &headBlendHeadGeom1, 0, s_headBlendDrawableNamePtrs, HeadBlendComboChangeCB);
		if (headBlendHeadGeomCombo2)
			headBlendHeadGeomCombo2->UpdateCombo("Parent geometry 2", &headBlendHeadGeom2, 0, s_headBlendDrawableNamePtrs, HeadBlendComboChangeCB);
		if (headBlendHeadGeomCombo3)
			headBlendHeadGeomCombo3->UpdateCombo("Genetic variation geometry", &headBlendHeadGeom3, 0, s_headBlendDrawableNamePtrs, HeadBlendComboChangeCB);

		return;
	}

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
	{
		return;
	}

	if (!headBlendHeadGeomCombo1 || !headBlendHeadTexCombo1 || !headBlendHeadGeomCombo2 || !headBlendHeadTexCombo2 || !headBlendHeadGeomCombo3 || !headBlendHeadTexCombo3)
		return;

	Assert(mi->GetVarInfo());
	if (!mi->GetVarInfo())
		return;

	u32 currentHeadDrawable = pPedVarDebugPed->GetPedDrawHandler().GetVarData().GetPedCompIdx(PV_COMP_HEAD);
	u8 currentHeadTex = pPedVarDebugPed->GetPedDrawHandler().GetVarData().GetPedTexIdx(PV_COMP_HEAD);

	const char*	pedFolder = mi->GetStreamFolder();

	// build list of available dwds
	const u32 numDrawables = mi->GetVarInfo()->GetMaxNumDrawbls(PV_COMP_HEAD);
	Assert(numDrawables <= MAX_DRAWABLES);
	for (s32 i = 0; i < numDrawables; ++i)
	{
		CPVDrawblData* drawableData = mi->GetVarInfo()->GetCollectionDrawable(PV_COMP_HEAD, i);
		Assertf(drawableData, "Null drawable with index %d on component %d", i, PV_COMP_HEAD);

		if (drawableData->IsUsingRaceTex())
		{
			sprintf(s_headBlendDrawableNames[i], "%s/head_%03d_r", pedFolder, i);
		} else {
			sprintf(s_headBlendDrawableNames[i], "%s/head_%03d_u", pedFolder, i);
		}

		s_headBlendDrawableNamePtrs[i] = s_headBlendDrawableNames[i];
	}

	if (headBlendHeadGeom1 < 0 || headBlendHeadGeom1 >= numDrawables)
		headBlendHeadGeom1 = currentHeadDrawable;

	if (headBlendHeadGeom2 < 0 || headBlendHeadGeom2 >= numDrawables)
		headBlendHeadGeom2 = currentHeadDrawable;

	if (headBlendHeadGeom3 < 0 || headBlendHeadGeom3 >= numDrawables)
		headBlendHeadGeom3 = currentHeadDrawable;

	// build list of available textures
	u32 numTextures = numDrawables;
	for (s32 i = 0; i < numTextures; ++i)
	{
		CPVDrawblData* drawableData = mi->GetVarInfo()->GetCollectionDrawable(PV_COMP_HEAD, i);
		Assertf(drawableData, "Null drawable with index %d on component %d", i, PV_COMP_HEAD);

		if (drawableData->IsUsingRaceTex())
		{
			sprintf(s_headBlendTextureNames[i], "%s/head_diff_%03d_%c", pedFolder, i, 'a');
		}

		s_headBlendTextureNamePtrs[i] = s_headBlendTextureNames[i];
	}

	if (headBlendHeadTex1 < 0 || headBlendHeadTex1 >= numTextures)
		headBlendHeadTex1 = currentHeadTex;

	if (headBlendHeadTex2 < 0 || headBlendHeadTex2 >= numTextures)
		headBlendHeadTex2 = currentHeadTex;

	if (headBlendHeadTex3 < 0 || headBlendHeadTex3 >= numTextures)
		headBlendHeadTex3 = currentHeadTex;

	// update widgets
	headBlendHeadGeomCombo1->UpdateCombo("Parent geometry 1", &headBlendHeadGeom1, numDrawables, s_headBlendDrawableNamePtrs, HeadBlendComboChangeCB);
	headBlendHeadGeomCombo2->UpdateCombo("Parent geometry 2", &headBlendHeadGeom2, numDrawables, s_headBlendDrawableNamePtrs, HeadBlendComboChangeCB);
	headBlendHeadGeomCombo3->UpdateCombo("Genetic variation geometry", &headBlendHeadGeom3, numDrawables, s_headBlendDrawableNamePtrs, HeadBlendComboChangeCB);
	headBlendHeadTexCombo1->UpdateCombo("Parent texture 1", &headBlendHeadTex1, numTextures, s_headBlendTextureNamePtrs, HeadBlendComboChangeCB);
	headBlendHeadTexCombo2->UpdateCombo("Parent texture 2", &headBlendHeadTex2, numTextures, s_headBlendTextureNamePtrs, HeadBlendComboChangeCB);
	headBlendHeadTexCombo3->UpdateCombo("Genetic variation texture", &headBlendHeadTex3, numTextures, s_headBlendTextureNamePtrs, HeadBlendComboChangeCB);

	// request assets
	CPedHeadBlendData data;
	data.m_head0 = (u8) headBlendHeadGeom1;
	data.m_head1 = (u8) headBlendHeadGeom2;
	data.m_head2 = (u8) headBlendHeadGeom3;
	data.m_tex0  = (u8) headBlendHeadTex1;
	data.m_tex1  = (u8) headBlendHeadTex2;
	data.m_tex2  = (u8) headBlendHeadTex3;
	data.m_headBlend = headBlendGeomBlend;
	data.m_texBlend  = headBlendTexBlend;
	data.m_varBlend  = headBlendVarBlend;
	data.m_isParent  = headBlendCreateAsParent;
	pPedVarDebugPed->SetHeadBlendData(data);

	// update child if the changed ped happens to have one
	if (pPedVarDebugPed == headBlendParent0 || pPedVarDebugPed == headBlendParent1)
	{
		if (headBlendSecondGenChild)
		{
			headBlendSecondGenChild->UpdateChildBlend();
		}
	}
}

void CPedVariationDebug::HeadBlendRefreshCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (pPedVarDebugPed && pPedVarDebugPed->GetPedModelInfo()->GetIsStreamedGfx())
	{
		CPedHeadBlendData* blendData = pPedVarDebugPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
		if (blendData)
		{
			headBlendHeadGeom1 = blendData->m_head0;
			headBlendHeadGeom2 = blendData->m_head1;
			headBlendHeadGeom3 = blendData->m_head2;

			headBlendHeadTex1 = blendData->m_tex0;
			headBlendHeadTex2 = blendData->m_tex1;
			headBlendHeadTex3 = blendData->m_tex2;

			headBlendGeomBlend = blendData->m_headBlend;
			headBlendTexBlend = blendData->m_texBlend;
			headBlendVarBlend = blendData->m_varBlend;

			headBlendOverlayBlemishes = blendData->m_overlayTex[HOS_BLEMISHES];
			headBlendOverlayBlemishesBlend = blendData->m_overlayAlpha[HOS_BLEMISHES];
			headBlendOverlayBlemishesNormBlend = blendData->m_overlayNormAlpha[HOS_BLEMISHES];

			headBlendOverlayFacialHair = blendData->m_overlayTex[HOS_FACIAL_HAIR];
			headBlendOverlayFacialHairBlend = blendData->m_overlayAlpha[HOS_FACIAL_HAIR];
			headBlendOverlayFacialHairNormBlend = blendData->m_overlayNormAlpha[HOS_FACIAL_HAIR];

			headBlendOverlayEyebrow = blendData->m_overlayTex[HOS_EYEBROW];
			headBlendOverlayEyebrowBlend = blendData->m_overlayAlpha[HOS_EYEBROW];
			headBlendOverlayEyebrowNormBlend = blendData->m_overlayNormAlpha[HOS_EYEBROW];

			headBlendOverlayAging = blendData->m_overlayTex[HOS_AGING];
			headBlendOverlayAgingBlend = blendData->m_overlayAlpha[HOS_AGING];
			headBlendOverlayAgingNormBlend = blendData->m_overlayNormAlpha[HOS_AGING];

			headBlendOverlayMakeup = blendData->m_overlayTex[HOS_MAKEUP];
			headBlendOverlayMakeupBlend = blendData->m_overlayAlpha[HOS_MAKEUP];
			headBlendOverlayMakeupNormBlend = blendData->m_overlayNormAlpha[HOS_MAKEUP];

			headBlendOverlayBlusher = blendData->m_overlayTex[HOS_BLUSHER];
			headBlendOverlayBlusherBlend = blendData->m_overlayAlpha[HOS_BLUSHER];
			headBlendOverlayBlusherNormBlend = blendData->m_overlayNormAlpha[HOS_BLUSHER];

			headBlendOverlayDamage = blendData->m_overlayTex[HOS_DAMAGE];
			headBlendOverlayDamageBlend = blendData->m_overlayAlpha[HOS_DAMAGE];
			headBlendOverlayDamageNormBlend = blendData->m_overlayNormAlpha[HOS_DAMAGE];

			headBlendOverlayBaseDetail = blendData->m_overlayTex[HOS_BASE_DETAIL];
			headBlendOverlayBaseDetailBlend = blendData->m_overlayAlpha[HOS_BASE_DETAIL];
			headBlendOverlayBaseDetailNormBlend = blendData->m_overlayNormAlpha[HOS_BASE_DETAIL];

			headBlendOverlaySkinDetail1 = blendData->m_overlayTex[HOS_SKIN_DETAIL_1];
			headBlendOverlaySkinDetail1Blend = blendData->m_overlayAlpha[HOS_SKIN_DETAIL_1];
			headBlendOverlaySkinDetail1NormBlend = blendData->m_overlayNormAlpha[HOS_SKIN_DETAIL_1];

			headBlendOverlaySkinDetail2 = blendData->m_overlayTex[HOS_SKIN_DETAIL_2];
			headBlendOverlaySkinDetail2Blend = blendData->m_overlayAlpha[HOS_SKIN_DETAIL_2];
			headBlendOverlaySkinDetail2NormBlend = blendData->m_overlayNormAlpha[HOS_SKIN_DETAIL_2];

			headBlendOverlayBody1 = blendData->m_overlayTex[HOS_BODY_1];
			headBlendOverlayBody1Blend = blendData->m_overlayAlpha[HOS_BODY_1];
			headBlendOverlayBody1NormBlend = blendData->m_overlayNormAlpha[HOS_BODY_1];

			headBlendOverlayBody2 = blendData->m_overlayTex[HOS_BODY_2];
			headBlendOverlayBody2Blend = blendData->m_overlayAlpha[HOS_BODY_2];
			headBlendOverlayBody2NormBlend = blendData->m_overlayNormAlpha[HOS_BODY_2];

			headBlendOverlayBody3 = blendData->m_overlayTex[HOS_BODY_3];
			headBlendOverlayBody3Blend = blendData->m_overlayAlpha[HOS_BODY_3];
			headBlendOverlayBody3NormBlend = blendData->m_overlayNormAlpha[HOS_BODY_3];
		}
	}
	
	HeadBlendComboChangeCB();
}

void CPedVariationDebug::HeadBlendAssignParent0CB(void)
{
	if (!headBlendSecondGenEnable)
		return;

	headBlendParent0 = NULL;

	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
	{
		headBlendParent0 = (CPed*)pEntity;
		pPedVarDebugPed = (CPed*)pEntity;
	}
}

void CPedVariationDebug::HeadBlendAssignParent1CB(void)
{
	if (!headBlendSecondGenEnable)
		return;

	headBlendParent1 = NULL;

	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
	{
		headBlendParent1 = (CPed*)pEntity;
		pPedVarDebugPed = (CPed*)pEntity;
	}
}

void CPedVariationDebug::HeadBlendClearParentsCB(void)
{
	headBlendParent0 = NULL;
	headBlendParent1 = NULL;
}

void CPedVariationDebug::HeadBlendCreateSecondGenChildCB(void)
{
	if (!headBlendSecondGenEnable)
		return;

	if (!headBlendParent0 || !headBlendParent1)
		return;

    CreatePedCB();
    headBlendSecondGenChild = pPedVarDebugPed;

    if (headBlendSecondGenChild)
    {
        headBlendSecondGenChild->RequestBlendFromParents(headBlendParent0, headBlendParent1, headBlendSecondGenGeomBlend, headBlendSecondGenTexBlend);
    }
}

void CPedVariationDebug::UpdateMicroMorphsCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	pPedVarDebugPed->MicroMorph(MMT_NOSE_WIDTH, microMorphNoseWidthBlend);
	pPedVarDebugPed->MicroMorph(MMT_NOSE_HEIGHT, microMorphNoseHeightBlend);
	pPedVarDebugPed->MicroMorph(MMT_NOSE_LENGTH, microMorphNoseLengthBlend);
	pPedVarDebugPed->MicroMorph(MMT_NOSE_PROFILE, microMorphNoseProfileBlend);
	pPedVarDebugPed->MicroMorph(MMT_NOSE_TIP, microMorphNoseTipBlend);
	pPedVarDebugPed->MicroMorph(MMT_NOSE_BROKE, microMorphNoseBrokeBlend);
	pPedVarDebugPed->MicroMorph(MMT_BROW_HEIGHT, microMorphBrowHeightBlend);
	pPedVarDebugPed->MicroMorph(MMT_BROW_DEPTH, microMorphBrowDepthBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHEEK_HEIGHT, microMorphCheekHeightBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHEEK_DEPTH, microMorphCheekDepthBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHEEK_PUFFED, microMorphCheekPuffedBlend);
	pPedVarDebugPed->MicroMorph(MMT_EYES_SIZE, microMorphEyesSizeBlend);
	pPedVarDebugPed->MicroMorph(MMT_LIPS_SIZE, microMorphLipsSizeBlend);
	pPedVarDebugPed->MicroMorph(MMT_JAW_WIDTH, microMorphJawWidthBlend);
	pPedVarDebugPed->MicroMorph(MMT_JAW_ROUND, microMorphJawRoundBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHIN_HEIGHT, microMorphChinHeightBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHIN_DEPTH, microMorphChinDepthBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHIN_POINTED, microMorphChinPointedBlend);
	pPedVarDebugPed->MicroMorph(MMT_CHIN_BUM, microMorphChinBumBlend);
	pPedVarDebugPed->MicroMorph(MMT_NECK_MALE, microMorphNeckMaleBlend);
}

void CPedVariationDebug::ResetMicroMorphsCB(void)
{
	microMorphNoseWidthBlend = 0.f;
	microMorphNoseHeightBlend = 0.f;
	microMorphNoseLengthBlend = 0.f;
	microMorphNoseProfileBlend = 0.f;
	microMorphNoseTipBlend = 0.f;
	microMorphNoseBrokeBlend = 0.f;
	microMorphBrowHeightBlend = 0.f;
	microMorphBrowDepthBlend = 0.f;
	microMorphCheekHeightBlend = 0.f;
	microMorphCheekDepthBlend = 0.f;
	microMorphCheekPuffedBlend = 0.f;
	microMorphEyesSizeBlend = 0.f;
	microMorphLipsSizeBlend = 0.f;
	microMorphJawWidthBlend = 0.f;
	microMorphJawRoundBlend = 0.f;
	microMorphChinHeightBlend = 0.f;
	microMorphChinDepthBlend = 0.f;
	microMorphChinPointedBlend = 0.f;
	microMorphChinBumBlend = 0.f;
	microMorphNeckMaleBlend = 0.f;

	UpdateMicroMorphsCB();
}

void CPedVariationDebug::BypassMicroMorphsCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedHeadBlendData* blendData = pPedVarDebugPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return;

	MESHBLENDMANAGER.ForceMeshBlend(blendData->m_blendHandle);
}

void CPedVariationDebug::HeadBlendUpdateSecondGenChildCB(void)
{
	if (!headBlendSecondGenEnable)
		return;

	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

    if (pPedVarDebugPed)
        pPedVarDebugPed->UpdateChildBlend();
}

void CPedVariationDebug::HeadBlendUpdateSecondGenBlendCB(void)
{
	if (!headBlendSecondGenEnable)
		return;

	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (pPedVarDebugPed)
		pPedVarDebugPed->SetNewHeadBlendValues(headBlendSecondGenGeomBlend, headBlendSecondGenTexBlend, 0.f);
}

void CPedVariationDebug::HeadBlendFinalizeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (pPedVarDebugPed)
		pPedVarDebugPed->FinalizeHeadBlend();
}

void CPedVariationDebug::HeadBlendOverlayBaseDetailChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayBaseDetail > -1 && headBlendOverlayBaseDetail <= CPedVariationStream::GetFacialOverlays().m_baseDetail.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_BASE_DETAIL, (u8)(headBlendOverlayBaseDetail - 1), headBlendOverlayBaseDetailBlend, headBlendOverlayBaseDetailNormBlend);
}

void CPedVariationDebug::HeadBlendOverlayBlemishesChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayBlemishes > -1 && headBlendOverlayBlemishes <= CPedVariationStream::GetFacialOverlays().m_blemishes.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_BLEMISHES, (u8)(headBlendOverlayBlemishes - 1), headBlendOverlayBlemishesBlend, headBlendOverlayBlemishesNormBlend);
}

void CPedVariationDebug::HeadBlendOverlaySkinDetail1ChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlaySkinDetail1 > -1 && headBlendOverlaySkinDetail1 <= CPedVariationStream::GetFacialOverlays().m_skinDetail1.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_SKIN_DETAIL_1, (u8)(headBlendOverlaySkinDetail1 - 1), headBlendOverlaySkinDetail1Blend, headBlendOverlaySkinDetail1NormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_SKIN_DETAIL_1, RT_MAKEUP, headBlendOverlaySkinDetail1Tint, headBlendOverlaySkinDetail1Tint2);
}

void CPedVariationDebug::HeadBlendOverlaySkinDetail2ChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlaySkinDetail2 > -1 && headBlendOverlaySkinDetail2 <= CPedVariationStream::GetFacialOverlays().m_skinDetail2.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_SKIN_DETAIL_2, (u8)(headBlendOverlaySkinDetail2 - 1), headBlendOverlaySkinDetail2Blend, headBlendOverlaySkinDetail2NormBlend);
}

void CPedVariationDebug::HeadBlendOverlayBody1ChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayBody1 > -1 && headBlendOverlayBody1 <= CPedVariationStream::GetFacialOverlays().m_bodyOverlay1.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_BODY_1, (u8)(headBlendOverlayBody1 - 1), headBlendOverlayBody1Blend, headBlendOverlayBody1NormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_BODY_1, RT_HAIR, headBlendOverlayBody1Tint, headBlendOverlayBody1Tint2);
}

void CPedVariationDebug::HeadBlendOverlayBody2ChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayBody2 > -1 && headBlendOverlayBody2 <= CPedVariationStream::GetFacialOverlays().m_bodyOverlay2.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_BODY_2, (u8)(headBlendOverlayBody2 - 1), headBlendOverlayBody2Blend, headBlendOverlayBody2NormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_BODY_2, RT_HAIR, headBlendOverlayBody2Tint, headBlendOverlayBody2Tint2);
}

void CPedVariationDebug::HeadBlendOverlayBody3ChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayBody3 > -1 && headBlendOverlayBody3 <= CPedVariationStream::GetFacialOverlays().m_bodyOverlay3.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_BODY_3, (u8)(headBlendOverlayBody3 - 1), headBlendOverlayBody3Blend, headBlendOverlayBody3NormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_BODY_3, RT_HAIR, headBlendOverlayBody3Tint, headBlendOverlayBody3Tint2);
}

void CPedVariationDebug::HeadBlendHairRampChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	pPedVarDebugPed->SetHairTintIndex(headBlendHairTintIndex, headBlendHairTintIndex2);
}

void CPedVariationDebug::HeadBlendOverlayFacialHairChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayFacialHair > -1 && headBlendOverlayFacialHair <= CPedVariationStream::GetFacialOverlays().m_facialHair.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_FACIAL_HAIR, (u8)(headBlendOverlayFacialHair - 1), headBlendOverlayFacialHairBlend, headBlendOverlayFacialHairNormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_FACIAL_HAIR, RT_HAIR, headBlendOverlayFacialHairTint, headBlendOverlayFacialHairTint2);
}

void CPedVariationDebug::HeadBlendOverlayEyebrowChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayEyebrow > -1 && headBlendOverlayEyebrow <= CPedVariationStream::GetFacialOverlays().m_eyebrow.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_EYEBROW, (u8)(headBlendOverlayEyebrow - 1), headBlendOverlayEyebrowBlend, headBlendOverlayEyebrowNormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_EYEBROW, RT_HAIR, headBlendOverlayEyebrowTint, headBlendOverlayEyebrowTint2);
}

void CPedVariationDebug::HeadBlendOverlayAgingChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayAging > -1 && headBlendOverlayAging <= CPedVariationStream::GetFacialOverlays().m_aging.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_AGING, (u8)(headBlendOverlayAging - 1), headBlendOverlayAgingBlend, headBlendOverlayAgingNormBlend);
}

void CPedVariationDebug::HeadBlendOverlayMakeupChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayMakeup > -1 && headBlendOverlayMakeup <= CPedVariationStream::GetFacialOverlays().m_makeup.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_MAKEUP, (u8)(headBlendOverlayMakeup - 1), headBlendOverlayMakeupBlend, headBlendOverlayMakeupNormBlend);
}

void CPedVariationDebug::HeadBlendOverlayBlusherChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayBlusher > -1 && headBlendOverlayBlusher <= CPedVariationStream::GetFacialOverlays().m_makeup.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_BLUSHER, (u8)(headBlendOverlayBlusher - 1), headBlendOverlayBlusherBlend, headBlendOverlayBlusherNormBlend);
	pPedVarDebugPed->SetHeadOverlayTintIndex(HOS_BLUSHER, RT_MAKEUP, headBlendOverlayBlusherTint, headBlendOverlayBlusherTint2);
}

void CPedVariationDebug::HeadBlendOverlayDamageChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

	if (headBlendOverlayDamage > -1 && headBlendOverlayDamage <= CPedVariationStream::GetFacialOverlays().m_damage.GetCount())
		pPedVarDebugPed->SetHeadOverlay(HOS_DAMAGE, (u8)(headBlendOverlayDamage - 1), headBlendOverlayDamageBlend, headBlendOverlayDamageNormBlend);
}

void CPedVariationDebug::HeadBlendPaletteRgbCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

    if (headBlendPaletteRgbEnabled)
	{
        pPedVarDebugPed->SetHeadBlendPaletteColor(headBlendPaletteRgb0.GetRed(), headBlendPaletteRgb0.GetGreen(), headBlendPaletteRgb0.GetBlue(), 0);
        pPedVarDebugPed->SetHeadBlendPaletteColor(headBlendPaletteRgb1.GetRed(), headBlendPaletteRgb1.GetGreen(), headBlendPaletteRgb1.GetBlue(), 1);
        pPedVarDebugPed->SetHeadBlendPaletteColor(headBlendPaletteRgb2.GetRed(), headBlendPaletteRgb2.GetGreen(), headBlendPaletteRgb2.GetBlue(), 2);
        pPedVarDebugPed->SetHeadBlendPaletteColor(headBlendPaletteRgb3.GetRed(), headBlendPaletteRgb3.GetGreen(), headBlendPaletteRgb3.GetBlue(), 3);
	}
    else
        pPedVarDebugPed->SetHeadBlendPaletteColorDisabled();
}

void CPedVariationDebug::HeadBlendEyeColorChangeCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedModelInfo* mi = static_cast<CPedModelInfo*>(pPedVarDebugPed->GetBaseModelInfo());
	Assert(mi);
	if (!mi->GetIsStreamedGfx())
		return;

    pPedVarDebugPed->SetHeadBlendEyeColor(headBlendEyeColor);
}

void CPedVariationDebug::HeadBlendForceTextureBlendCB(void)
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
		pPedVarDebugPed = (CPed*)pEntity;

	if (!pPedVarDebugPed)
		return;

	CPedHeadBlendData* blendData = pPedVarDebugPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
    if (!blendData)
        return;

    MESHBLENDMANAGER.ForceTextureBlend(blendData->m_blendHandle);
}

void CPedVariationDebug::UpdateHeadBlendEyeColorSlider(void)
{
    if (headBlendEyeColorSlider)
        headBlendEyeColorSlider->SetRange(-1.f, (float)MESHBLENDMANAGER.GetNumEyeColors() - 1.f);
}

CPed*	CPedVariationDebug::GetDebugPed(void)
{
	return(pPedVarDebugPed);
}

void	CPedVariationDebug::SetDebugPed(CPed* pPed, s32 modelIndex)
{
	pPedVarDebugPed = pPed;
	CPedVariationDebug::currPedNameSelection = modelIndex;		// need the idx in the sorted name list to set up correctly...

	if (pedSorts == NULL)
	{
		UpdatePedList();
	}

	PedIDChangeCB();
	RefreshVarSet();
}

grcTexture* CPedVariationDebug::GetGreyDebugTexture(void){
	return(pDebugGreyTexture);
}

bool CPedVariationDebug::GetDisableDiffuse(void){
	return bDisableDiffuse;
}

bool CPedVariationDebug::GetForceHdAssets(void){
	return bForceHdAssets;
}

bool CPedVariationDebug::GetForceLdAssets(void){
	return bForceLdAssets;
}

// Get player vtxcount/volume info.
void CPedVariationDebug::GetPolyCountPlayerInternal(const CPedStreamRenderGfx* pGfx, u32* pPolyCount, u32 lodLevel, u32* pVertexCount, u32* pVertexSize, u32* pIndexCount, u32* pIndexSize)
{
	const rmcDrawable* pDrawable = NULL;

	// go through all the component slots and draw the component for this ped
	for(u32 i=0;i<PV_MAX_COMP;i++)	
	{
		// check the don't draw array - see if anything needs skipped
		if (CPedVariationStream::ms_aDrawPlayerComp[i] == false){
			continue;
		}

		pDrawable = pGfx->GetDrawableConst(i);
		if (pDrawable)
		{
			if (pVertexCount && pVertexSize && pIndexCount && pIndexSize)
				ColorizerUtils::GetDrawableInfo(pDrawable, pPolyCount, pVertexCount, pVertexSize, pIndexCount, pIndexSize, lodLevel);
			else
				ColorizerUtils::GetDrawablePolyCount(pDrawable, pPolyCount, lodLevel);
		}
	}
}

void CPedVariationDebug::GetPolyCountPedInternal(const CPedModelInfo* pModelInfo, const CPedVariationData& varData, u32* pPolyCount, u32 lodLevel, u32* pVertexCount, u32* pVertexSize, u32* pIndexCount, u32* pIndexSize)
{
	s32 drawblIdx = 0;
	s32 j;

	if (AssertVerify(pModelInfo && pModelInfo->GetVarInfo()))
	{
		const strLocalIndex dwdSlot = strLocalIndex(pModelInfo->GetPedComponentFileIndex());
		const Dwd* pDwd = g_DwdStore.Get(dwdSlot);

		if (Verifyf(pDwd, "CPedVariationDebug::GetPolyCountPedInternal encountered NULL dwd for model %s, dwd slot %d (%s)", pModelInfo->GetModelName(), dwdSlot.Get(), g_DwdStore.GetName(dwdSlot)))
		{
			// go through all the component slots and draw the component for this ped
			for(j=0;j<PV_MAX_COMP;j++)
			{
				drawblIdx = varData.m_aDrawblId[j];
				CPedVariationInfoCollection* varInfo = const_cast<CPedVariationInfoCollection*>(pModelInfo->GetVarInfo());
				if (varInfo && varInfo->IsValidDrawbl(j, drawblIdx))
				{
					u32 hash = varData.m_aDrawblHash[j];
					const rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, static_cast<ePedVarComp>(j), drawblIdx,  &hash, varData.m_aDrawblAltId[j]);

					// render the drawable (if it exists for this ped)
					if (pDrawable)
					{
						if (pVertexCount && pVertexSize && pIndexCount && pIndexSize)
							ColorizerUtils::GetDrawableInfo(pDrawable, pPolyCount, pVertexCount, pVertexSize, pIndexCount, pIndexSize, lodLevel);
						else
							ColorizerUtils::GetDrawablePolyCount(pDrawable, pPolyCount, lodLevel);
					}
				}
			}
		}
	}
}

// Get player vtxcount/volume info.
void CPedVariationDebug::GetVtxVolumeCompPlayerInternal(const CPedStreamRenderGfx* pGfx, float &vtxCount, float &volume)
{
	const rmcDrawable* pDrawable = NULL;

	// go through all the component slots and draw the component for this ped
	for(u32 i=0;i<PV_MAX_COMP;i++)	
	{
		// check the don't draw array - see if anything needs skipped
		if (CPedVariationStream::ms_aDrawPlayerComp[i] == false){
			continue;
		}

	//	pDrawable = pGfx->m_pComponents[i];
		pDrawable = pGfx->GetDrawableConst(i);
		if (pDrawable)
		{
			ColorizerUtils::GetVVRData(pDrawable,vtxCount,volume);
		}
	}
}

void CPedVariationDebug::GetVtxVolumeCompPedInternal(const CPedModelInfo *pModelInfo, const CPedVariationData& varData, float &vtxCount, float &volume)
{
	s32	drawblIdx = 0;
	s32	j;

	if (AssertVerify(pModelInfo && pModelInfo->GetVarInfo()))
	{
		const strLocalIndex dwdSlot = strLocalIndex(pModelInfo->GetPedComponentFileIndex());
		const Dwd* pDwd = g_DwdStore.Get(dwdSlot);

		if (Verifyf(pDwd, "CPedVariationDebug::GetVtxVolumeCompPedInternal encountered NULL dwd for model %s, dwd slot %d (%s)", pModelInfo->GetModelName(), dwdSlot.Get(), g_DwdStore.GetName(dwdSlot)))
		{
			// go through all the component slots and draw the component for this ped
			for(j=0;j<PV_MAX_COMP;j++)
			{
				drawblIdx = varData.m_aDrawblId[j];
				CPedVariationInfoCollection* varInfo = const_cast<CPedVariationInfoCollection*>(pModelInfo->GetVarInfo());
				if (varInfo && varInfo->IsValidDrawbl(j, drawblIdx))
				{
					u32 hash = varData.m_aDrawblHash[j];
					const rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, static_cast<ePedVarComp>(j), drawblIdx,  &hash, varData.m_aDrawblAltId[j]);

					// render the drawable (if it exists for this ped)
					if (pDrawable)
					{
						ColorizerUtils::GetVVRData(pDrawable,vtxCount,volume);
					}
				}
			}
		}
	}
}

void CPedVariationDebug::GetDrawables(const CPedModelInfo* pModelInfo, const CPedVariationData* pVarData, atArray<rmcDrawable*>& aDrawables)
{
	s32	drawblIdx = 0;
	s32	j;

	if (AssertVerify(pModelInfo && pModelInfo->GetVarInfo()))
	{
		const strLocalIndex dwdSlot = strLocalIndex(pModelInfo->GetPedComponentFileIndex());
		const Dwd* pDwd = g_DwdStore.Get(dwdSlot);

		if (Verifyf(pDwd, "CPedVariationDebug::GetDrawables encountered NULL dwd for model %s, dwd slot %d (%s)", pModelInfo->GetModelName(), dwdSlot.Get(), g_DwdStore.GetName(dwdSlot)))
		{
			for(j=0; j<PV_MAX_COMP; j++)
			{
				drawblIdx = pVarData->m_aDrawblId[j];
				CPedVariationInfoCollection* varInfo = const_cast<CPedVariationInfoCollection*>(pModelInfo->GetVarInfo());
				if (varInfo && varInfo->IsValidDrawbl(j, drawblIdx))
				{
					u32 hash = pVarData->m_aDrawblHash[j];
					rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, static_cast<ePedVarComp>(j), drawblIdx,  &hash, pVarData->m_aDrawblAltId[j]);
					if (pDrawable)
					{
						aDrawables.PushAndGrow(pDrawable);
					}
				}
			}
		}
	}
}

#if __PPU
// Get Edge usage info
bool CPedVariationDebug::GetVtxEdgeUsageCompPlayerInternal(const CPedStreamRenderGfx* pGfx)
{
	// go through all the component slots and draw the component for this ped
	for(u32 i=0;i<PV_MAX_COMP;i++)	
	{
		// check the don't draw array - see if anything needs skipped
		if (CPedVariationStream::ms_aDrawPlayerComp[i] == false){
			continue;
		}

		const rmcDrawable* pDrawable = pGfx->GetDrawableConst(i);
		if (pDrawable && ColorizerUtils::GetEdgeUseage(pDrawable))
		{
			return true;
		}
	}
	
	return false;
}

bool CPedVariationDebug::GetVtxEdgeUsageCompPedInternal(const CPedModelInfo *pModelInfo, const CPedVariationData& varData)
{
	s32	drawblIdx = 0;
	s32	j;

	if (AssertVerify(pModelInfo && pModelInfo->GetVarInfo()))
	{
		const int dwdSlot = pModelInfo->GetPedComponentFileIndex();
		const Dwd* pDwd = g_DwdStore.Get(dwdSlot);

		if (Verifyf(pDwd, "CPedVariationDebug::GetVtxEdgeUsageCompPedInternal encountered NULL dwd for model %s, dwd slot %d (%s)", pModelInfo->GetModelName(), dwdSlot, g_DwdStore.GetName(dwdSlot)))
		{
			// go through all the component slots and draw the component for this ped
			for(j=0;j<PV_MAX_COMP;j++)
			{
				drawblIdx = varData.m_aDrawblId[j];
				CPedVariationInfoCollection* varInfo = const_cast<CPedVariationInfoCollection*>(pModelInfo->GetVarInfo());
				if (varInfo && varInfo->IsValidDrawbl(j, drawblIdx))
				{
					u32 hash = varData.m_aDrawblHash[j];
					const rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, static_cast<ePedVarComp>(j), drawblIdx, &hash, varData.m_aDrawblAltId[j]);

					// render the drawable (if it exists for this ped)
					if (pDrawable && ColorizerUtils::GetEdgeUseage(pDrawable))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


bool CPedVariationDebug::GetIsSPUPipelinedPlayer(const CPedStreamRenderGfx* pGfx)
{
	// go through all the component slots and draw the component for this ped
	for(u32 i=0;i<PV_MAX_COMP;i++)	
	{
		// check the don't draw array - see if anything needs skipped
		if (CPedVariationStream::ms_aDrawPlayerComp[i] == false){
			continue;
		}

		const rmcDrawable* pDrawable = pGfx->GetDrawableConst(i);
		if (pDrawable && pDrawable->GetPpuOnly())
		{
			return false;
		}
	}
	
	return true;
}

bool CPedVariationDebug::GetIsSPUPipelined(const CPedModelInfo *pModelInfo, const CPedVariationData& varData)
{
	s32	drawblIdx = 0;
	s32	j;

	if (AssertVerify(pModelInfo && pModelInfo->GetVarInfo()))
	{
		const int dwdSlot = pModelInfo->GetPedComponentFileIndex();
		const Dwd* pDwd = g_DwdStore.Get(dwdSlot);

		if (Verifyf(pDwd, "CPedVariationDebug::GetIsSPUPipelined encountered NULL dwd for model %s, dwd slot %d (%s)", pModelInfo->GetModelName(), dwdSlot, g_DwdStore.GetName(dwdSlot)))
		{
			// go through all the component slots and draw the component for this ped
			for(j=0;j<PV_MAX_COMP;j++)
			{
				drawblIdx = varData.m_aDrawblId[j];
				CPedVariationInfoCollection* varInfo = const_cast<CPedVariationInfoCollection*>(pModelInfo->GetVarInfo());
				if (varInfo && varInfo->IsValidDrawbl(j, drawblIdx))
				{
					u32 hash = varData.m_aDrawblHash[j];
					const rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, static_cast<ePedVarComp>(j), drawblIdx, &hash, varData.m_aDrawblAltId[j]);

					// render the drawable (if it exists for this ped)
					if (pDrawable && pDrawable->GetPpuOnly())
					{
						return false;
					}
				}
			}
		}
	}
		
	return true;
}

#endif // __PPU


// Return the specified component for the current ped
rmcDrawable* CPedVariationDebug::GetComponent(ePedVarComp eComponent, CEntity* pEntity, CPedVariationData& pedVarData)
{
	Assert(pEntity);

	rmcDrawable* pComponent = NULL;

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	if(pModelInfo->GetIsStreamedGfx())
	{
		CPed* pPlayerPed = static_cast<CPed *>(pEntity);
		Assert(pPlayerPed);
		const CPedStreamRenderGfx* pGfxData = pPlayerPed->GetPedDrawHandler().GetConstPedRenderGfx();
		Assert(pGfxData);
		pComponent = pGfxData->GetDrawable(eComponent);
	}
	else
	{
		strLocalIndex DwdIdx = strLocalIndex(pModelInfo->GetPedComponentFileIndex());
		Dwd* pDwd = g_DwdStore.Get(DwdIdx);

		if (AssertVerify(pDwd))
		{
			u32 drawblIdx = pedVarData.GetPedCompIdx(eComponent);
			pComponent = CPedVariationPack::ExtractComponent(pDwd, eComponent, drawblIdx, &(pedVarData.m_aDrawblHash[eComponent]), pedVarData.m_aDrawblAltId[eComponent]);
		}
	}


	return pComponent;
}

#undef VISUALISE_RANGE // ubfix
#define VISUALISE_RANGE	(20.0f)
#define VISUALISE_RANGE_NO_ALPHA (5.0f)

static const int NUM_INSPECTOR_ROWS = 11;
static const char* inspectorEntries[NUM_INSPECTOR_ROWS] = {"", "Comp models", "Comp textures", "Comp frags", "Subtotal", "", "Prop models", "Prop textures", "Subtotal", "", "Total ped cost"};

void PopulateInspectorWindowCB(CUiGadgetText* pResult, u32 row, u32 col)
{
	if (pResult && row < NUM_INSPECTOR_ROWS)
	{
		CEntity *pEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
		if (pEntity)
		{
			if (!pEntity->GetIsTypePed())
			{
				return;
			}

			// gather data
			atArray<strIndex> deps;
			fwModelId modelId = pEntity->GetModelId();
			const atArray<strIndex>& ignoreList = CPedModelInfo::GetResidentObjects();
			CModelInfo::GetObjectAndDependencies(modelId, deps, ignoreList.GetElements(), ignoreList.GetCount());

            enum
            {
                COMP_DRAWABLE = 0,
                COMP_TEXTURE,
                COMP_FRAG,
                COMP_SUBTOTAL,
                PROP_DRAWABLE,
                PROP_TEXTURE,
                PROP_SUBTOTAL,
                SIZE_NUM
            };
            s32 virtSize[SIZE_NUM] = {0};
            s32 physSize[SIZE_NUM] = {0};

			for (s32 i = 0; i < deps.GetCount(); ++i)
			{
				s32 itemVirtSize = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
				s32 itemPhysSize = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
				const char* name = strStreamingEngine::GetInfo().GetObjectName(deps[i]);
				if (bDebugOutputDependencyNames)
				{
					Displayf("Dependency %d: %s - virt: %d phys: %d\n", i, name, itemVirtSize, itemPhysSize);
				}

                if (strstr(name, "_p.cdd") || strstr(name, "_p.xdd") || strstr(name, "_p.wdd"))
                {
                    virtSize[PROP_DRAWABLE] += itemVirtSize;
                    physSize[PROP_DRAWABLE] += itemPhysSize;
                    virtSize[PROP_SUBTOTAL] += itemVirtSize;
                    physSize[PROP_SUBTOTAL] += itemPhysSize;
                }
                else if (strstr(name, "_p.ctd") || strstr(name, "_p.xtd") || strstr(name, "_p.wtd"))
                {
                    virtSize[PROP_TEXTURE] += itemVirtSize;
                    physSize[PROP_TEXTURE] += itemPhysSize;
                    virtSize[PROP_SUBTOTAL] += itemVirtSize;
                    physSize[PROP_SUBTOTAL] += itemPhysSize;
                }
                else if (strstr(name, ".cdd") || strstr(name, ".xdd") || strstr(name, ".wdd"))
                {
                    virtSize[COMP_DRAWABLE] += itemVirtSize;
                    physSize[COMP_DRAWABLE] += itemPhysSize;
                    virtSize[COMP_SUBTOTAL] += itemVirtSize;
                    physSize[COMP_SUBTOTAL] += itemPhysSize;
                }
                else if (strstr(name, ".ctd") || strstr(name, ".xtd") || strstr(name, ".wtd"))
                {
                    virtSize[COMP_TEXTURE] += itemVirtSize;
                    physSize[COMP_TEXTURE] += itemPhysSize;
                    virtSize[COMP_SUBTOTAL] += itemVirtSize;
                    physSize[COMP_SUBTOTAL] += itemPhysSize;
                }
                else if (strstr(name, ".cft") || strstr(name, ".xft") || strstr(name, ".wft"))
                {
                    virtSize[COMP_FRAG] += itemVirtSize;
                    physSize[COMP_FRAG] += itemPhysSize;
                    virtSize[COMP_SUBTOTAL] += itemVirtSize;
                    physSize[COMP_SUBTOTAL] += itemPhysSize;
                }
			}
			
			if (col == 0)
			{
				if (row == 0)	
				{
					pResult->SetString(pEntity->GetModelName());
				}
				else
				{
					pResult->SetString(inspectorEntries[row]);
				}
			}
			else
			{
				char buf[256] = {0};
				if (row == 0)	
				{
					formatf(buf, "Main Ram\t Video Ram\t Total Cost");
					pResult->SetString(buf);
				}
				else
				{
					switch (row)
					{
					case 1:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[COMP_DRAWABLE] / 1024.f, physSize[COMP_DRAWABLE] / 1024.f, (virtSize[COMP_DRAWABLE] + physSize[COMP_DRAWABLE]) / 1024.f);
						break;
					case 2:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[COMP_TEXTURE] / 1024.f, physSize[COMP_TEXTURE] / 1024.f, (virtSize[COMP_TEXTURE] + physSize[COMP_TEXTURE]) / 1024.f);
						break;
					case 3:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[COMP_FRAG] / 1024.f, physSize[COMP_FRAG] / 1024.f, (virtSize[COMP_FRAG] + physSize[COMP_FRAG]) / 1024.f);
						break;
					case 4:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[COMP_SUBTOTAL] / 1024.f, physSize[COMP_SUBTOTAL] / 1024.f, (virtSize[COMP_SUBTOTAL] + physSize[COMP_SUBTOTAL]) / 1024.f);
						break;
					case 5:
						break;
					case 6:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[PROP_DRAWABLE] / 1024.f, physSize[PROP_DRAWABLE] / 1024.f, (virtSize[PROP_DRAWABLE] + physSize[PROP_DRAWABLE]) / 1024.f);
						break;
					case 7:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[PROP_TEXTURE] / 1024.f, physSize[PROP_TEXTURE] / 1024.f, (virtSize[PROP_TEXTURE] + physSize[PROP_TEXTURE]) / 1024.f);
						break;
					case 8:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", virtSize[PROP_SUBTOTAL] / 1024.f, physSize[PROP_SUBTOTAL] / 1024.f, (virtSize[PROP_SUBTOTAL] + physSize[PROP_SUBTOTAL]) / 1024.f);
						break;
					case 9:
						break;
					case 10:
						formatf(buf, "%07.2fk\t %07.2fk\t\t %07.2fk", (virtSize[PROP_SUBTOTAL] + virtSize[COMP_SUBTOTAL]) / 1024.f, (physSize[PROP_SUBTOTAL] + physSize[COMP_SUBTOTAL]) / 1024.f, ((virtSize[PROP_SUBTOTAL] + virtSize[COMP_SUBTOTAL]) + (physSize[PROP_SUBTOTAL] + physSize[COMP_SUBTOTAL])) / 1024.f);
						break;
					default:
						formatf(buf, "wrong number of rows", row);
					}
					pResult->SetString(buf);
				}
			}
		}
	}
}

void CPedVariationDebug::Visualise(const CPed& BANK_ONLY(ped))
{
	if (bDisplayMemoryBreakdown)
	{
		if (!pInspectorWindow)
		{
			CUiColorScheme colorScheme;
			float afColumnOffsets[] = { 0.0f, 100.0f };

			pInspectorWindow = rage_new CUiGadgetInspector(0.f, 0.f, 300.0f, NUM_INSPECTOR_ROWS, 2, afColumnOffsets, colorScheme);

			pInspectorWindow->SetUpdateCellCB(PopulateInspectorWindowCB);
			pInspectorWindow->SetNumEntries(11);
		}

		if (!bIsInspectorAttached)
		{
			bIsInspectorAttached = true;
			CGtaPickerInterface* pickerInterface = (CGtaPickerInterface*)g_PickerManager.GetInterface();
			if (pickerInterface)
			{
				pickerInterface->AttachInspectorChild(pInspectorWindow);
			}
		}
	}
	else if (bIsInspectorAttached)
	{
		bIsInspectorAttached = false;
		CGtaPickerInterface* pickerInterface = (CGtaPickerInterface*)g_PickerManager.GetInterface();
		if (pickerInterface)
		{
			pickerInterface->DetachInspectorChild(pInspectorWindow);
		}
	}

	VisualisePedMetadata(ped);

	if (!bDisplayVarData){
		return;
	}

	CPed* pPed = (CPed*)&ped;

	// only want to do text for peds close to camera
	// Set origin to be the debug cam, or the player..
#if __DEV//def CAM_DEBUG
	// camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	// Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#else
	// Vector3 vOrigin = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#endif

	Vector3	pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vDiff = pos - camInterface::GetPos();
	float fDist = vDiff.Mag();

	if(fDist >= VISUALISE_RANGE) 
		return;

	s32 iNumLines = 0;
	float fScale = 1.0f - ( Max(fDist - VISUALISE_RANGE_NO_ALPHA, 0.0f) / (VISUALISE_RANGE - VISUALISE_RANGE_NO_ALPHA) );
	Vector3 WorldCoors = pos + Vector3(0,0,1.1f);
	u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);
	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());

	if (bDisplayVarData)
	{
		// iterate through all possible combinations									//next in cycle...
		for (u32 i=0;i<MAX_PED_COMPONENTS;i++)
		{
			if (pModelInfo->GetVarInfo()->IsValidDrawbl(i, pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i]))
			{
				char dataText[50];
				sprintf(dataText, "%s: (D:%d, T:%d, P:%d)", varSlotNames[i], pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i], pPed->GetPedDrawHandler().GetVarData().m_aTexId[i], pPed->GetPedDrawHandler().GetVarData().m_aPaletteId[i]);

				if(pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i] > 0 ||pPed->GetPedDrawHandler().GetVarData().m_aTexId[i] > 0 || pPed->GetPedDrawHandler().GetVarData().m_aTexId[i] >0 ||pPed->GetPedDrawHandler().GetVarData().m_aPaletteId[i] > 0  )
				{
					grcDebugDraw::Text(WorldCoors, CRGBA(127,255,255,iAlpha), 0, iNumLines*grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
				}
				else
				{
					grcDebugDraw::Text(WorldCoors, CRGBA(255,255,255,iAlpha), 0, iNumLines*grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
				}
				
		
				++iNumLines;
			}
		}
	}
}

void CPedVariationDebug::VisualisePedMetadata(const CPed& pedRef)
{
	if (!bDisplayPedMetadata)
		return;

	CPed* ped = (CPed*)&pedRef;
	// camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	// Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());

	Vector3	pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
	Vector3 vDiff = pos - camInterface::GetPos();
	float fDist = vDiff.Mag();

	if(fDist >= VISUALISE_RANGE) 
		return;

	s32 iNumLines = 0;
	Vector3 WorldCoors = pos + Vector3(0,0,1.1f);
	Color32 color = CRGBA_White();
	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(ped->GetBaseModelInfo());
	fwMetaDataFile* metadata = NULL;
	CPedVariationInfo* pedVarInfo = NULL;

	bool hasMetadata = true;
	if (pModelInfo->GetNumPedMetaDataFiles() <= 0)
		hasMetadata = false;

	if (hasMetadata)
	{
		metadata = g_fwMetaDataStore.Get(strLocalIndex(pModelInfo->GetPedMetaDataFileIndex(0)));
		if (!metadata)
			hasMetadata = false;
	}

	if (hasMetadata)
	{
		pedVarInfo = metadata->GetObject<CPedVariationInfo>();
		if (!pedVarInfo)
			hasMetadata = false;
	}

	char flagText[128];
	char dataText[256];
	if (!hasMetadata)
	{
		formatf(dataText, "No metadata for '%s'", pModelInfo->GetModelName());
		grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
	}
	else
	{
		formatf(dataText, "Components");
		grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);

		for (u32 i = 0; i < MAX_PED_COMPONENTS; ++i)
		{
			u32 drawable = ped->GetPedDrawHandler().GetVarData().m_aDrawblId[i];
			u32 texture = ped->GetPedDrawHandler().GetVarData().m_aTexId[i];
			u32 palette = ped->GetPedDrawHandler().GetVarData().m_aPaletteId[i];
			ePedVarComp comp = (ePedVarComp)i;

			if (pModelInfo->GetVarInfo()->IsValidDrawbl(i, drawable))
			{
				u8 texDistrib = pModelInfo->GetVarInfo()->GetTextureDistribution(comp, drawable, texture);
				atHashString aud1 = pModelInfo->GetVarInfo()->LookupCompInfoAudioID(i, drawable, false); 
				atHashString aud2 = pModelInfo->GetVarInfo()->LookupCompInfoAudioID(i, drawable, true); 
				u32 flags = pModelInfo->GetVarInfo()->LookupCompInfoFlags(i, drawable);
				const float* exp = pModelInfo->GetVarInfo()->GetCompExpressionsMods((u8)comp, (u8)drawable);

				u32 incId = 0;
				const atFixedBitSet<32, u32>* incs = pedVarInfo->GetInclusions(i, drawable);
				if (incs)
				{
					incId = incs->GetRawBlock(0);
				}

				u32 excId = 0;
				const atFixedBitSet<32, u32>* excs = pedVarInfo->GetExclusions(i, drawable);
				if (excs)
				{
					excId = excs->GetRawBlock(0);
				}

				flagText[0] = '\0';
				for (s32 f = 0; f < NUM_PED_COMP_FLAGS; ++f)
				{
					if ((flags & (1 << f)) != 0)
					{
						safecat(flagText, " ");
						safecat(flagText, pedCompFlagNames[f]);
					}
				}

				formatf(dataText, "%s: (D:%d, T:%d (%d), P:%d) (a1:%s a2:%s) (i/e: %d/%d) (exp:%.3f, %.3f, %.3f, %.3f, %.3f) %s", varSlotNames[i], drawable, texture, texDistrib, palette,
						aud1.GetCStr(), aud2.GetCStr(), incId, excId, exp[0], exp[1], exp[2], exp[3], exp[4], flagText);

				if(ped->GetPedDrawHandler().GetVarData().m_aDrawblId[i] > 0 || ped->GetPedDrawHandler().GetVarData().m_aTexId[i] > 0 || ped->GetPedDrawHandler().GetVarData().m_aTexId[i] >0 || ped->GetPedDrawHandler().GetVarData().m_aPaletteId[i] > 0)
				{
					grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
				}
				else
				{
					grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
				}
			}
		}

		formatf(dataText, "Props");
		grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);

		for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
		{
			eAnchorPoints anchor = ped->GetPedDrawHandler().GetPropData().GetAnchor(i);
			s32 prop = ped->GetPedDrawHandler().GetPropData().GetPropId(i);
			s32 texture = ped->GetPedDrawHandler().GetPropData().GetTextureId(i);
			
			if (anchor == ANCHOR_NONE)
				continue;

			u32 flags = pModelInfo->GetVarInfo()->GetPropFlags((u8)anchor, (u8)prop);
			u8 stickyness = pModelInfo->GetVarInfo()->GetPropStickyness((u8)anchor, (u8)prop);
			u8 distribution = pModelInfo->GetVarInfo()->GetPropTexDistribution((u8)anchor, (u8)prop, (u8)texture);
			const float* exp = pModelInfo->GetVarInfo()->GetExpressionMods((u32)anchor, (u32)prop);

            u32 incId = 0; 
            const atFixedBitSet<32, u32>* incs = pedVarInfo->GetPropInfo()->GetInclusions((u8)anchor, (u8)prop, (u8)texture);
            if (incs)
                incId = incs->GetRawBlock(0);

            u32 excId = 0;
            const atFixedBitSet<32, u32>* excs = pedVarInfo->GetPropInfo()->GetExclusions((u8)anchor, (u8)prop, (u8)texture);
            if (excs)
                excId = excs->GetRawBlock(0);

            float expDefault[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
            if (!exp) exp = expDefault;

			flagText[0] = '\0';
			for (s32 f = 0; f < NUM_PED_COMP_FLAGS; ++f)
			{
				if ((flags & (1 << f)) != 0)
				{
					safecat(flagText, " ");
					safecat(flagText, pedCompFlagNames[f]);
				}
			}

            if (pModelInfo->GetVarInfo()->HasPropAlpha((u8)anchor, (u8)prop))
                safecat(flagText, " alpha");
            if (pModelInfo->GetVarInfo()->HasPropDecal((u8)anchor, (u8)prop))
                safecat(flagText, " decal");
            if (pModelInfo->GetVarInfo()->HasPropCutout((u8)anchor, (u8)prop))
                safecat(flagText, " cutout");

			formatf(dataText, "%s: (P:%d, T:%d (%d), S:%d) (i/e: %d/%d) (exp:%.3f, %.3f, %.3f, %.3f, %.3f) %s", propSlotNamesClean[anchor], prop, texture, distribution, stickyness,
				incId, excId, exp[0], exp[1], exp[2], exp[3], exp[4], flagText);

			grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
		}

		u32 numSelectionSets = pModelInfo->GetVarInfo()->GetSelectionSetCount();
		if (numSelectionSets > 0)
		{
			formatf(dataText, "Selection sets");
			grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);

			for (u32 i = 0; i < numSelectionSets; ++i)
			{
				const CPedSelectionSet* set = pModelInfo->GetVarInfo()->GetSelectionSet(i);
				if (!set)
					continue;

				formatf(dataText, "%s", set->m_name.TryGetCStr());
				grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
			}
		}

		if(ped)
		{
			formatf(dataText, "Current Hair Scale :%.3f", ped->GetCurrentHairScale());
			grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
			formatf(dataText, "Target Hair Scale :%.3f", ped->GetTargetHairScale());
			grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
			formatf(dataText, "Hair Height :%.3f", ped->GetHairHeight());
			grcDebugDraw::Text(WorldCoors, color, 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), dataText);
		}
	}
}

void CPedVariationDebug::DebugUpdate(){
	PedComponentCheckUpdate();
	if(pPedVarDebugPed)
	{
		CPedVariationDebug::Visualise(*pPedVarDebugPed);
	}

	if (headBlendSecondGenEnable)
	{
		if (headBlendParent0)
		{
			Vector3	pos = VEC3V_TO_VECTOR3(headBlendParent0->GetTransform().GetPosition());
			Vector3 worldPos = pos + Vector3(0.f, 0.f, 1.3f);

			char buf[256];
			safecpy(buf, "parent 0");
			grcDebugDraw::Text(worldPos, CRGBA(255, 0, 0, 255), 0, grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (headBlendParent1)
		{
			Vector3	pos = VEC3V_TO_VECTOR3(headBlendParent1->GetTransform().GetPosition());
			Vector3 worldPos = pos + Vector3(0.f, 0.f, 1.3f);

			char buf[256];
			safecpy(buf, "parent 1");
			grcDebugDraw::Text(worldPos, CRGBA(255, 0, 0, 255), 0, grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (headBlendSecondGenChild)
		{
			Vector3	pos = VEC3V_TO_VECTOR3(headBlendSecondGenChild->GetTransform().GetPosition());
			Vector3 worldPos = pos + Vector3(0.f, 0.f, 1.3f);

			char buf[256];
			safecpy(buf, "child");
			grcDebugDraw::Text(worldPos, CRGBA(255, 0, 0, 255), 0, grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}
	}
}

void CPedVariationDebug::EditMetadataInWorkbenchCB()
{
	CPed* ped = pPedVarDebugPed;
	if (!ped)
	{
		CEntity* ent = (CEntity*)g_PickerManager.GetSelectedEntity();
		if (ent && ent->GetIsTypePed())
		{
			ped = (CPed*)ent;
		}
	}

	if (ped)
	{
		CPedModelInfo* mi = ped->GetPedModelInfo();
		strLocalIndex metaIndex = strLocalIndex(mi->GetPedMetaDataFileIndex(0));
		if (Verifyf(metaIndex != -1, "Ped '%s' has no metadata!", mi->GetModelName()))
		{
			if (Verifyf(g_fwMetaDataStore.Get(metaIndex), "Ped '%s' has no metadata!", mi->GetModelName()))
			{
				char fullPath[RAGE_MAX_PATH] = {0};
				formatf(fullPath, "X:\\gta5\\assets\\metadata\\characters\\%s.pso.meta", g_fwMetaDataStore.GetName(metaIndex));

				// Generate the tree that represents the xml to post
				parTree* pTree = rage_new parTree();
				parTreeNode* pRoot = rage_new parTreeNode("Files");
				pTree->SetRoot(pRoot);

				parTreeNode* pAssetName = rage_new parTreeNode("File");
				pAssetName->SetData(fullPath, ustrlen(fullPath));
				pAssetName->AppendAsChildOf(pRoot);

				// Convert the tree to an xml string
				datGrowBuffer gb;
				gb.Init(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL), datGrowBuffer::NULL_TERMINATE);

				char gbName[RAGE_MAX_PATH];
				fiDevice::MakeGrowBufferFileName(gbName, RAGE_MAX_PATH, &gb);
				fiStream* stream = ASSET.Create(gbName, "");
				PARSER.SaveTree(stream, pTree, parManager::XML);
				stream->Flush();
				stream->Close();
				const char* pXml = (const char*)gb.GetBuffer();

				// Run the query
				char workbenchIPAddress[32];
				sprintf(workbenchIPAddress, "%s:12357", bkRemotePacket::GetRagSocketAddress());

				char workbenchRESTAddress[64];
				sprintf(workbenchRESTAddress, "http://%s/MetaDataService-1.0/", bkRemotePacket::GetRagSocketAddress());

				fwHttpQuery query(workbenchIPAddress, workbenchRESTAddress, true);
				query.InitPost("OpenFiles", 60);
				query.AppendXmlContent(pXml, istrlen(pXml));

				bool querySucceeded = false;
				if(query.CommitPost())
				{
					while(query.Pending())
					{
						sysIpcSleep(100);
					}

					querySucceeded = query.Succeeded();
				}

				if (!querySucceeded)
				{
					// Tell the user that the query failed (this is most likely because the workbench isn't open)
					Assertf(false, "An error occurred while attempting to edit the metadata file.  This is most likely because the workbench isn't open.");
				}
			}
		}
	}
	else
	{
		Assertf(false, "Create a debug ped or select one with the picker.");
	}
}

//------------------------------------------------------------------------------------------

#endif // __BANK
