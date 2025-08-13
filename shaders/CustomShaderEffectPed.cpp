//
// Filename:	CustomShaderEffectPed.cpp
// Description:	Class for controlling bone damage display in ped shaders;
// Written by:	Andrzej
//
//	10/08/2005	-	Andrzej:	- initial;
//	30/09/2005	-	Andrzej:	- update to Rage WR146;
//	06/04/2006	-	Andrzej:	- added support for CLONE_EFFECTS=0;
//	02/04/2009	-	Andrzej:	- added support for ped_enveff (snow scale);
//	30/06/2010	-	Andrzej:	- added support for ped_fat (fat scale);
//
//
//

// Rage headers
#include "grmodel\ShaderFx.h"
#include "grmodel\Geometry.h"
#include "grcore\effect_config.h" // edge
#include "grcore/texturereference.h"

#include "creature/creature.h"
#include "creature/componentshadervars.h"
#include "creature/componentblendshapes.h"
#include "grblendshapes/manager.h"
#include "pheffects/wind.h"
#include "rmcore/drawable.h"

#include "fwdrawlist/drawlistmgr.h"

// Game headers
#include "camera\viewports\viewportmanager.h"
#include "debug\debug.h"
#include "scene\Entity.h"
#include "modelinfo\PedModelInfo.h"
#include "peds\Ped.h"
#include "peds/PopCycle.h"
#include "peds\rendering\PedDamage.h"
#include "peds\rendering\PedOverlay.h"
#include "peds\rendering\pedVariationPack.h"
#include "vehicleAi\task\TaskVehicleAnimation.h"
#include "renderer/Water.h"
#include "renderer/Mirrors.h"
#include "renderer\RenderPhases/RenderPhaseFX.h"
#include "renderer\Deferred\DeferredLighting.h"
#include "vfx\Systems\VfxFire.h"
#include "CustomShaderEffectPed.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "Cutscene/CutSceneCameraEntity.h"
#include "camera\CamInterface.h"
#include "timecycle\TimeCycle.h"
#include "Vfx\Misc\Fire.h"	
#include "vehicles/vehicle.h"
#include "ShaderHairSort.h"
#include "shader_source/Peds/ped_common_values.h"

#if __BANK
#include "peds/rendering/PedVariationDebug.h"
#include "shaders/ShaderEdit.h"
#endif

#include "system/findsize.h"
// FindSize(CCustomShaderEffectBase); // 16
// FindSize(CCustomShaderEffectPed); 
// 2512, zoiks!  
// 2368 with texture registry.  
// 1968 with u16 blend weight.
// 816 with bone array nobody uses removed?
// 656 with wrinkle maps packed from floats to u8's
// 640 with proper separation of "type" and "per instance" data (sigh)
// 224 by basically disabling blendshape weights; code is supposed to know when creating instance whether it needs blendshapes or not.
// 176 by deleting unused textures, rearranging by size, and doing ped color scale entirely based on config bools
// 192 by packing 4xfloat vars into packed 4xubytes
// 208 with extra 2 wrinkle masks
// 224 with um override params
#if !PGHANDLEINDEX_IS_A_PGHANDLE && !GRCTEXTUREHANDLE_IS_PGREF// PGHANDLEINDEX_IS_A_PGHANDLE is going to bloat things, but don't really care by how much
CompileTimeAssertSize(CCustomShaderEffectPed,272,320);
#endif

#define PED_DEFAULT_WIND_MOTION_AMP_X		(5.0f)
#define PED_DEFAULT_WIND_MOTION_AMP_Y		(3.0f)
#define PED_DEFAULT_WIND_MOTION_FREQ		(0.8f)
#define PED_DEFAULT_WIND_RESPONSE_VEL_MIN	(1.0f)
#define PED_DEFAULT_WIND_RESPONSE_VEL_MAX	(100.0f)
#define PED_DEFAULT_WIND_FLUTTER_INTENSITY	(4.0f)

#if CSE_PED_EDITABLEVALUES
	bool	CCustomShaderEffectPed::ms_bEVEnabled						= false;

	float	CCustomShaderEffectPed::ms_fEVSpecFalloff					= 250.0f;
	float	CCustomShaderEffectPed::ms_fEVSpecIntensity					= 1.0f;
	float	CCustomShaderEffectPed::ms_fEVSpecFresnel					= 0.950f;
	float	CCustomShaderEffectPed::ms_fEVReflectivity					= 0.45f;
	float	CCustomShaderEffectPed::ms_fEVBumpiness						= 0.75f;
	float	CCustomShaderEffectPed::ms_fEVBurnoutLevelSpeed				= 0.02f;

	float	CCustomShaderEffectPed::ms_fEVDetailIntensity				= 0.1f;;
	float	CCustomShaderEffectPed::ms_fEVDetailBumpIntensity			= 0.750f;
	float	CCustomShaderEffectPed::ms_fEVDetailScale					= 60.0f;

	float	CCustomShaderEffectPed::ms_fEVEnvEffScale					= 0.0f;
	float	CCustomShaderEffectPed::ms_fEVEnvEffCpvAdd					= 0.0f;
	float	CCustomShaderEffectPed::ms_fEVEnvEffThickness				= 25.0f;
	Color32	CCustomShaderEffectPed::ms_fEVEnvEffColorMod				= Color32(255,255,255,255);
	float	CCustomShaderEffectPed::ms_fEVEnvEffZoneScale				= 0.0f;
	u32		CCustomShaderEffectPed::ms_fEVEnvEffZoneColorModR			= 255;
	u32		CCustomShaderEffectPed::ms_fEVEnvEffZoneColorModG			= 255;
	u32		CCustomShaderEffectPed::ms_fEVEnvEffZoneColorModB			= 255;
	float	CCustomShaderEffectPed::ms_fEVFatScale						= 0.0f;
	float	CCustomShaderEffectPed::ms_fEVFatThickness					= 25.0f;
	float	CCustomShaderEffectPed::ms_fEVSweatScale					= 0.0f;
	
	float	CCustomShaderEffectPed::ms_fEVNormalBlend					= 0.0f;
	float   CCustomShaderEffectPed::ms_fEVStubbleStrength				= 0.8f;

	float   CCustomShaderEffectPed::ms_fEVStubbleIndex					= 10.0f;
	float   CCustomShaderEffectPed::ms_fEVStubbleTile					= 1.f;
	Vector4	CCustomShaderEffectPed::ms_fEVUmGlobalParams				= Vector4(0.0025f, 0.0025f, 7.000f, 7.000f);
	bool	CCustomShaderEffectPed::ms_bEVUseUmGlobalOverrideValues		= false;
	Vector4	CCustomShaderEffectPed::ms_fEVUmGlobalOverrideParams		= Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	
	bool		CCustomShaderEffectPed::ms_bEVWindMotionEnabled		  = true;
	float		CCustomShaderEffectPed::ms_fEVWindMotionAmpX		  = PED_DEFAULT_WIND_MOTION_AMP_X;
	float		CCustomShaderEffectPed::ms_fEVWindMotionAmpY          = PED_DEFAULT_WIND_MOTION_AMP_Y;
	float		CCustomShaderEffectPed::ms_fEVWindMotionFreq		  = PED_DEFAULT_WIND_MOTION_FREQ;
	
	bool		CCustomShaderEffectPed::ms_bEVWindResponseEnabled	  = true;
	float		CCustomShaderEffectPed::ms_fEVWindResponseVelMin	  = PED_DEFAULT_WIND_RESPONSE_VEL_MIN;
	float		CCustomShaderEffectPed::ms_fEVWindResponseVelMax      = PED_DEFAULT_WIND_RESPONSE_VEL_MAX;
	float		CCustomShaderEffectPed::ms_fEVWindFlutterIntensity    = PED_DEFAULT_WIND_FLUTTER_INTENSITY;

	Vector2	CCustomShaderEffectPed::ms_fEVAnisotropicSpecularIntensity	= Vector2(0.1f, 0.15f);
	Vector2	CCustomShaderEffectPed::ms_fEVAnisotropicSpecularExponent	= Vector2(16.0f, 32.0f);
	Color32	CCustomShaderEffectPed::ms_fEVAnisotropicSpecularColour		= Color32(25, 25, 25, 255);
	Vector4	CCustomShaderEffectPed::ms_fEVSpecularNoiseMapUVScaleFactor	= Vector4(2.0f, 1.0f, 3.0f, 1.0f);
	float	CCustomShaderEffectPed::ms_fEVAnisotropicAlphaBias			= 1.0f;

	#define SHADER_SETTINGS_FNAME										"x:\\ped_shader_settings.txt"
	extern char* GetPedVariationMgrDrawableName();						// defined in PedVariationMgr.cpp
	extern s32 GetPedVariationMgrDrawableNameLen();						// defined in PedVariationMgr.cpp
#endif //CSE_PED_EDITABLEVALUES...

Vector4	CCustomShaderEffectPed::ms_fEVWetClothesAdjust				= Vector3(0.35f, 8.0f, 0.35f, 0.1f);
Vector4	CCustomShaderEffectPed::ms_fEVWetClothesAdjustMoreWet		= Vector3(0.656f, 16.1f, 0.40f, 0.205f);
Vector4	CCustomShaderEffectPed::ms_fEVWetClothesAdjustLessWet		= Vector3(0.157f, 8.8f, 0.1f, 0.104f);

static BankBool									g_UsePedShaderVarCaching							= true;
bool											CCustomShaderEffectPed::ms_EnableShaderVarCaching[NUMBER_OF_RENDER_THREADS]	= { false };
CCustomShaderEffectPed::CCachedCSEPedShaderVars	CCustomShaderEffectPed::ms_CachedCSEPedShaderVars[NUMBER_OF_RENDER_THREADS];


Vector2	CCustomShaderEffectPed::ms_DetailFadeOut[2] = { Vector2(1.7f,1.8f),		// std
														Vector2(20.0f,21.f)		// burnout		
														};						

static s8 s_wrinkleTrans[] = 
{
	0,	// PV_COMP_HEAD = 0,
	-1,	// PV_COMP_BERD,
	-1,	// PV_COMP_HAIR,
	1,	// PV_COMP_UPPR,
	2,	// PV_COMP_LOWR,
	3,	// PV_COMP_HAND,
	-1,	// PV_COMP_FEET,
	-1,	// PV_COMP_TEEF,
	-1,	// PV_COMP_ACCS,
	-1,	// PV_COMP_TASK,
	-1,	// PV_COMP_DECL,
	-1,	// PV_COMP_JBIB,
	-1	// PV_MAX_COMP
};



#if __BANK
	bool	CCustomShaderEffectPed::ms_bBlendshapeForceAll			= false;
	bool	CCustomShaderEffectPed::ms_bBlendshapeForceSpecific		= false;
	int		CCustomShaderEffectPed::ms_nBlendshapeForceSpecificIdx	= 0;
	float	CCustomShaderEffectPed::ms_fBlendshapeForceSpecificBlend= 1.0f;

	bool	CCustomShaderEffectPed::ms_wrinkleEnableOverride		= false;
	bool	CCustomShaderEffectPed::ms_wrinkleEnableSliders			= false;
	Vector4	CCustomShaderEffectPed::ms_wrinkleMaskStrengths[6]		= {0};
	int		CCustomShaderEffectPed::ms_wrinkleDebugComponent		= 0;
	atArray<const char*> CCustomShaderEffectPed::ms_wrinkleDebugComponentStrings;
	bool	g_useOptimisedCutsceneTechnique = true;
#endif // __BANK


#if __ASSERT
static s32 s_ComponentID = 0;
char* CCustomShaderEffectPed_EntityName = NULL;	// entity name for debugging purposes only:

static void ASSERTMSG_DETAILED(u8 assertCondition, const char *varName, s32 componentID)
{
	if(assertCondition)
		return;	// nothing to do?

	const char* componentName;
	switch(componentID)
	{
		case(PV_COMP_HEAD):	componentName="head";	break;
		case(PV_COMP_UPPR):	componentName="uppr";	break;
		case(PV_COMP_LOWR):	componentName="lowr";	break;
		case(PV_COMP_HAND):	componentName="hand";	break;
		case(PV_COMP_FEET):	componentName="feet";	break;
		case(PV_COMP_HAIR):	componentName="hair";	break;
		default:			componentName="unknown";break;
	}
	Assertf(assertCondition, "Object '%s', component %d ('%s'): %s variable not found!",CCustomShaderEffectPed_EntityName,s_ComponentID,componentName,varName);
}
#endif // __ASSERT...

//
//
//
//
inline u8 PedShaderGroupLookupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

inline u8 PedEffectLookupGlobalVar(const char *name, bool mustExist)
{
	grcEffectGlobalVar varID = grcEffect::LookupGlobalVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}


//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectPedType::Initialise(rmcDrawable *pDrawable)
{
	Assert(pDrawable);

	// make sure array indices are correct:
	Assert(PV_COMP_HEAD < PV_COMP_UPPR);
	Assert(PV_COMP_UPPR < PV_COMP_LOWR);
	Assert(PV_COMP_LOWR < PV_MAX_COMP);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	Assert(pShaderGroup);

	// find indices to variables changed by this class, etc.
	#if __ASSERT
		#define GET_SHADER_VAR_INDEX(I, NAME, PRINTASSERT)	{	I = PedShaderGroupLookupVar(pShaderGroup,NAME, FALSE); if(PRINTASSERT)	ASSERTMSG_DETAILED(I, NAME, s_ComponentID); }
	#else
		#define GET_SHADER_VAR_INDEX(I, NAME, PRINTASSERT)	{	I = PedShaderGroupLookupVar(pShaderGroup,NAME, FALSE); if(PRINTASSERT)	{ Assert(I); }	}
	#endif

		GET_SHADER_VAR_INDEX(m_idVarDiffuseTex,				"DiffuseTex",						TRUE);
		GET_SHADER_VAR_INDEX(m_idVarNormalTex,				"BumpTex",							TRUE);
		GET_SHADER_VAR_INDEX(m_idVarSpecularTex,			"SpecularTex",						TRUE);

		m_idVarMaterialColorScale		 = PedEffectLookupGlobalVar("MaterialColorScale",		TRUE);
		m_idVarWetClothesData			 = PedEffectLookupGlobalVar("WetClothesData",			TRUE);

		m_idVarPedBloodTarget			 = PedEffectLookupGlobalVar("pedBloodTarget",			TRUE);
		m_idVarPedTattooTarget			 = PedEffectLookupGlobalVar("pedTattooTarget",			TRUE);
		m_idVarPedDamageData			 = PedEffectLookupGlobalVar("PedDamageData",			TRUE);
		m_idVarPedDamageColors			 = PedEffectLookupGlobalVar("pedDamageColors",			FALSE);	// depends on USE_VARIABLE_BLOOD_COLORS=1 in ped_common.fxh
		m_idVarWetnessAdjust			 = PedEffectLookupGlobalVar("WetnessAdjust",			TRUE);

		m_idVarAlphaToCoverageScale		 = PedEffectLookupGlobalVar("AlphaToCoverageScale",		FALSE);
		
		m_idVarDiffuseTexPal			= PedEffectLookupGlobalVar("DiffuseTexPal",				TRUE);
		m_idVarDiffuseTexPalSelector	= PedEffectLookupGlobalVar("DiffuseTexPaletteSelector",	TRUE);
		m_idVarEnvEffFatSweatScale		= PedEffectLookupGlobalVar("envEffFatSweatScale0",		TRUE);
		m_idVarEnvEffColorModCpvAdd		= PedEffectLookupGlobalVar("envEffColorModCpvAdd0",		TRUE);

#if __XENON
		m_idVarClothParentMatrix		= PedShaderGroupLookupVar(pShaderGroup, "clothParentMatrix", TRUE);
#endif // __XENON

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		m_idVarClothParentMatrix		= PedEffectLookupGlobalVar("clothParentMatrix",	TRUE);
		m_idVarClothVertices			= PedEffectLookupGlobalVar("clothVertices",		FALSE);	// optional
#endif

		m_idVarWrinkleMaskStrengths0	= PedEffectLookupGlobalVar("WrinkleMaskStrengths0", TRUE);
		m_idVarWrinkleMaskStrengths1	= PedEffectLookupGlobalVar("WrinkleMaskStrengths1", TRUE);
		m_idVarWrinkleMaskStrengths2	= PedEffectLookupGlobalVar("WrinkleMaskStrengths2", TRUE);
		m_idVarWrinkleMaskStrengths3	= PedEffectLookupGlobalVar("WrinkleMaskStrengths3", TRUE);
		m_idVarWrinkleMaskStrengths4	= PedEffectLookupGlobalVar("WrinkleMaskStrengths4", TRUE);
		m_idVarWrinkleMaskStrengths5	= PedEffectLookupGlobalVar("WrinkleMaskStrengths5", TRUE);

		m_idVarStubbleGrowthDetailScale	= PedEffectLookupGlobalVar("StubbleGrowth",					TRUE);
		m_idVarUmGlobalOverrideParams	= PedEffectLookupGlobalVar("umPedGlobalOverrideParams0",	TRUE);
		
		m_bIsPaletteShader				= bool(pShaderGroup->LookupShader("ped_palette") != 0);
		m_bIsFurShader					= bool(pShaderGroup->LookupShader("ped_fur") != 0);

	#undef GET_SHADER_VAR_INDEX

	return(TRUE);
}// end of CCustomShaderEffectPedType::Initialise()...

//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectPedType::CreateInstance(CEntity* UNUSED_PARAM(pEntity))
{
	// TODO - figure out how to determine this at instantiation time.
	bool hasWeights = false;
	u32 size = sizeof(CCustomShaderEffectPed) + (hasWeights? sizeof(CCustomShaderEffectPed::ComponentBlendWeights) : 0);
	CCustomShaderEffectPed *pNewShaderEffect = rage_placement_new(rage_alignof_new(CCustomShaderEffectPed) u8[size]) CCustomShaderEffectPed(size);

	if (hasWeights)
	{
		pNewShaderEffect->m_bHasBlendshapes = TRUE;
		CCustomShaderEffectPed::ComponentBlendWeights *blendWeights = (CCustomShaderEffectPed::ComponentBlendWeights*)(pNewShaderEffect + 1);
		blendWeights[0].m_Exists = false; // MAX_PED_COMPONENTS
		blendWeights[0].m_Processed = false; // MAX_PED_COMPONENTS
	}

	pNewShaderEffect->m_pType = this;


	// everything else is initialized by the ctor already
	return(pNewShaderEffect);
}

//
// name:		CCustomShaderEffectPed::AreShadersValid
// description:	Return if drawable has valid shaders for this shader effect
//
#if __DEV
bool CCustomShaderEffectPedType::AreShadersValid(rmcDrawable* pDrawable)
{
	bool bShadersOK = TRUE;

	Assert(pDrawable);
	if(!pDrawable)
		return(FALSE);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	Assert(pShaderGroup);

	if(!pShaderGroup)
		return(FALSE);

	const s32 shaderCount	= pShaderGroup->GetCount();
	Assert(shaderCount >= 1);
	for(s32 j=0; j<shaderCount; j++)
	{
		grmShader *pShader = &pShaderGroup->GetShader(j);
		Assert(pShader);
		const char *pShaderName = pShader->GetName();

		if(	::strcmpi(pShaderName, "ped")							&&
			::strcmpi(pShaderName, "ped_alpha")						&&
			::strcmpi(pShaderName, "ped_cloth")						&&
			::strcmpi(pShaderName, "ped_decal")						&&
			::strcmpi(pShaderName, "ped_decal_decoration")			&&
			::strcmpi(pShaderName, "ped_decal_exp")					&&
			::strcmpi(pShaderName, "ped_decal_medals")				&&
			::strcmpi(pShaderName, "ped_decal_nodiff")				&&
			::strcmpi(pShaderName, "ped_default")					&&
			::strcmpi(pShaderName, "ped_default_cloth")				&&
			::strcmpi(pShaderName, "ped_default_enveff")			&&
			::strcmpi(pShaderName, "ped_default_palette")			&&
			::strcmpi(pShaderName, "ped_emissive")					&&
			::strcmpi(pShaderName, "ped_enveff")					&&
			::strcmpi(pShaderName, "ped_fur")						&&
			::strcmpi(pShaderName, "ped_hair_cutout_alpha")			&&
			::strcmpi(pShaderName, "ped_hair_cutout_alpha_cloth")	&&
			::strcmpi(pShaderName, "ped_hair_spiked")				&&
			::strcmpi(pShaderName, "ped_hair_spiked_enveff")		&&
			::strcmpi(pShaderName, "ped_hair_spiked_mask")			&&
			::strcmpi(pShaderName, "ped_palette")					&&
			::strcmpi(pShaderName, "ped_stubble")					&&
			::strcmpi(pShaderName, "ped_wrinkle")					&&
			::strcmpi(pShaderName, "ped_wrinkle_cloth")				&&
			::strcmpi(pShaderName, "ped_wrinkle_cs")				&&
			::strcmpi(pShaderName, "default")						)
		{
			// bad shader found!
			bShadersOK = FALSE;
			break;
		}
	}

	return(bShadersOK);
}
#endif //__DEV...


//
//
//
//
CCustomShaderEffectPed::CCustomShaderEffectPed(u32 size) : CCustomShaderEffectBase(size, CSE_PED)
{

	for(s32 i=0; i<MAX_PED_COMPONENTS;i++)
	{
		m_varhDiffuseTex[i]				= 0;
		m_varhDiffuseTexPal[i]			= 0;
		m_varDiffuseTexPalSelector[i]	= 0;
		m_varhNormalTex[i]				= 0;
		m_varhSpecularTex[i]			= 0;
	}
	m_secondHairPalSelector = 0;

	this->SetEnvEffScale(0.0f);
	this->SetFatScale(0.0f);
	this->SetStubbleGrowth(0.0f);
	this->SetSweatScale(0.0f);

	this->SetEnvEffColorModulator(255,255,255);
	this->SetEnvEffCpvAdd(0.0f);

	SetUmGlobalOverrideParams(1.0f, 1.0f, 1.0f, 1.0f);

	m_varWetClothesData.Set(-2,-2,0,0);
	m_bRenderBurnout = FALSE;
	m_bRenderVehBurnout = FALSE;
	this->SetBurnoutLevel(0.0f);
	this->SetEmissiveScale(1.0f);
	m_bRenderDead = FALSE;
	m_bTriggeredSmokeFx = FALSE;
	m_bHasBlendshapes = FALSE;
	m_bCustomEnvEffScale = FALSE;
	m_varHeat = 255;
	
	m_PedBloodTarget = NULL;
	m_PedTattooTarget = NULL;
	m_PedTattooTargetOverride = NULL;
	m_PedDamageData.Set(0,0,0,0);
	m_usePedDamageInMirror = false;

	sysMemSet(m_wrinkleMaskStrengths, 0x00, sizeof(m_wrinkleMaskStrengths));

	// these need to be the same!
	CompileTimeAssert(MAX_PED_COMPONENTS == PV_MAX_COMP);

	m_pType = NULL;
}

//
//
//
//
CCustomShaderEffectPed::~CCustomShaderEffectPed()
{
	for (int i=0; i<MAX_PED_COMPONENTS; i++)
		ClearTextures(i);
}

//
//
//
//
void CCustomShaderEffectPed::InitClass()
{
	InvalidateShaderVarCache();
}

//
//
//
//
void CCustomShaderEffectPed::InvalidateShaderVarCache()
{
	for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		ms_CachedCSEPedShaderVars[i].PedBloodTarget				= 0;
		ms_CachedCSEPedShaderVars[i].PedTattooTarget			= 0;
		ms_CachedCSEPedShaderVars[i].DiffuseTexPal				= 0;
		ms_CachedCSEPedShaderVars[i].DiffuseTexPalSelector		= 0;
		ms_CachedCSEPedShaderVars[i].MaterialColorScale[0]		= Vec4V(V_ZERO);
		ms_CachedCSEPedShaderVars[i].MaterialColorScale[1]		= Vec4V(V_ZERO);
		ms_CachedCSEPedShaderVars[i].WetClothesData				= Vec4V(V_ZERO);
		ms_CachedCSEPedShaderVars[i].WetnessAdjust				= Vec4V(V_ZERO);
		ms_CachedCSEPedShaderVars[i].PedDamageData				= Vec4V(V_ZERO);
		ms_CachedCSEPedShaderVars[i].EnvEffFatSweatScale		= Vec4V(V_ZERO);
		ms_CachedCSEPedShaderVars[i].EnvEffColorMod_CpvAdd		= Vec4V(V_ZERO);

		ms_CachedCSEPedShaderVars[i].PedDamageColors[0]			=
		ms_CachedCSEPedShaderVars[i].PedDamageColors[1]			=
		ms_CachedCSEPedShaderVars[i].PedDamageColors[2]			= Vec4V(V_ZERO);

		ms_CachedCSEPedShaderVars[i].WrinkleMaskStrenghts[0]	= 
		ms_CachedCSEPedShaderVars[i].WrinkleMaskStrenghts[1]	= 
		ms_CachedCSEPedShaderVars[i].WrinkleMaskStrenghts[2]	= 
		ms_CachedCSEPedShaderVars[i].WrinkleMaskStrenghts[3]	= 
		ms_CachedCSEPedShaderVars[i].WrinkleMaskStrenghts[4]	= 
		ms_CachedCSEPedShaderVars[i].WrinkleMaskStrenghts[5]	= Vec4V(V_ZERO);

		ms_CachedCSEPedShaderVars[i].StubbleGrowthDetScale		= Vec2V(V_ZERO);

		ms_CachedCSEPedShaderVars[i].umGlobalOverrideParams.x.SetFloat16_Zero();
		ms_CachedCSEPedShaderVars[i].umGlobalOverrideParams.y.SetFloat16_Zero();
		ms_CachedCSEPedShaderVars[i].umGlobalOverrideParams.z.SetFloat16_Zero();
		ms_CachedCSEPedShaderVars[i].umGlobalOverrideParams.w.SetFloat16_Zero();
	}
}


//
// special version for PedVariationMgr:
// it's called from CPedVariationInfo::GetShaderEffectType()
//
CCustomShaderEffectBaseType* CCustomShaderEffectPed::SetupForDrawableComponent(rmcDrawable* pDrawable, s32 ASSERT_ONLY(componentID), CCustomShaderEffectBaseType *pShaderEffectType)
{

	Assert(componentID >= 0);
	Assert(componentID <  MAX_PED_COMPONENTS);
#if __ASSERT
	s_ComponentID = componentID;	// save component ID for Initialise() below
#endif

	if(!pShaderEffectType)	// create shader effect only once
	{
		pShaderEffectType = CCustomShaderEffectBaseType::SetupForDrawable(pDrawable, MI_TYPE_PED, NULL);
	}
	else
	{	// hack for Rage:
		// call for variables look-up routines at least once per Drawable to initialize internal ShaderGroup var arrays:
		CCustomShaderEffectBaseType *pTempShaderEffectType = CCustomShaderEffectBaseType::SetupForDrawable(pDrawable, MI_TYPE_PED, NULL);
		if(pTempShaderEffectType)
		{
			Assert(pShaderEffectType);

			CCustomShaderEffectPedType* pTempPedEffectType = static_cast<CCustomShaderEffectPedType*>(pTempShaderEffectType);
			CCustomShaderEffectPedType* pPedEffectType = static_cast<CCustomShaderEffectPedType*>(pShaderEffectType);

			// safety check whether shaderGroupVars match for both ped component drawables:
			#define ASSERT_COMPARE_VAR(VARNAME)	Assertf((static_cast<CCustomShaderEffectPedType*>(pTempShaderEffectType))->VARNAME == (static_cast<CCustomShaderEffectPedType*>(pShaderEffectType))->VARNAME, "CSE Ped: Incompatible shaderGroupVar "#VARNAME" found for ped component %d.",componentID);
				ASSERT_COMPARE_VAR(m_idVarDiffuseTex);
				ASSERT_COMPARE_VAR(m_idVarDiffuseTexPal);
				ASSERT_COMPARE_VAR(m_idVarDiffuseTexPalSelector);
				ASSERT_COMPARE_VAR(m_idVarWrinkleMaskStrengths0);
				ASSERT_COMPARE_VAR(m_idVarWrinkleMaskStrengths1);
				ASSERT_COMPARE_VAR(m_idVarWrinkleMaskStrengths2);
				ASSERT_COMPARE_VAR(m_idVarWrinkleMaskStrengths3);
				ASSERT_COMPARE_VAR(m_idVarWrinkleMaskStrengths4);
				ASSERT_COMPARE_VAR(m_idVarWrinkleMaskStrengths5);
				ASSERT_COMPARE_VAR(m_idVarMaterialColorScale);
				ASSERT_COMPARE_VAR(m_idVarEnvEffFatSweatScale);
				ASSERT_COMPARE_VAR(m_idVarEnvEffColorModCpvAdd);
				ASSERT_COMPARE_VAR(m_idVarWetClothesData);
				ASSERT_COMPARE_VAR(m_idVarWetnessAdjust);
				ASSERT_COMPARE_VAR(m_idVarPedBloodTarget);
				ASSERT_COMPARE_VAR(m_idVarPedTattooTarget);
				ASSERT_COMPARE_VAR(m_idVarPedDamageData);
				ASSERT_COMPARE_VAR(m_idVarPedDamageColors);	
				ASSERT_COMPARE_VAR(m_idVarStubbleGrowthDetailScale);	
				ASSERT_COMPARE_VAR(m_idVarUmGlobalOverrideParams);
				#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
					ASSERT_COMPARE_VAR(m_idVarClothParentMatrix);
				#endif
				#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
					ASSERT_COMPARE_VAR(m_idVarClothVertices);
				#endif
				ASSERT_COMPARE_VAR(m_idVarNormalTex);
				ASSERT_COMPARE_VAR(m_idVarSpecularTex);
			#undef ASSERT_COMPARE_VAR

			// the fur and skin bits just need to be turned on if any part of the model has that shader on it
			pPedEffectType->m_bIsFurShader |= pTempPedEffectType->m_bIsFurShader;
			
			pTempShaderEffectType->RemoveRef();
			pTempShaderEffectType = NULL;
		}
	}

	return(pShaderEffectType);
}

u32 CCustomShaderEffectPed::AddDataForPrototype(void * address)
{
	u32 size = 0;
	DrawListAddress drawListOffset;
	CopyOffShader(this, this->Size(), drawListOffset, false, m_pType);
	drawListOffset = gDCBuffer->LookupSharedData(DL_MEMTYPE_SHADER, GetSharedDataOffset());
	*static_cast<DrawListAddress*>(address) = drawListOffset;
	size += sizeof(DrawListAddress);
	return size;
}

u8 PackToU8( float v){
	Assert( v>=0. && v<=1.);
	return (u8)(v*255.);
}
//
//
//
//
void CCustomShaderEffectPed::Update(fwEntity* pEntityBase)
{
	CEntity* pEntity = static_cast<CEntity*>(pEntityBase);
	Assert (pEntity);
		
	//Don't update the shader for special cutscene peds
	CPed* pPed=NULL;
	CPedModelInfo* pModelInfo=NULL;
	if(pEntity->GetIsTypePed())
	{
		pPed = (CPed*)pEntity;
		pModelInfo = pPed->GetPedModelInfo();
	}
	else
	{
#if __DEV
		CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
		Assert(pMI);
		Assertf(false, "Entity shouldn't use this CSE Ped: %s (type:%d)",pMI->GetModelName(),pEntity->GetType());
#endif //__DEV

		Assertf(false,"Unhandled entity type in CCustomShaderEffectPed::Update");
		return;
	}

	//
	//
	// check if ped should be rendered burnout:
	//
	this->SetWetClothesData(Vector4(-2,-2,0,0));
	if(pPed)
	{
		fwDynamicEntityComponent *pedDynComp = pPed->GetDynamicComponent();

 		this->m_bRenderVehBurnout = FALSE; // this should be false if we're not in a vehicle or we are but it's not on fire...
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->IsInjured())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pPed->GetMyVehicle());
			if(pVehicle->m_nPhysicalFlags.bRenderScorched)
			{
				this->m_bRenderVehBurnout = TRUE;
			}
		}

		if(pPed->m_nPhysicalFlags.bRenderScorched)
		{
			if (pPed->IsNetworkClone())
			{
				this->m_bRenderBurnout = TRUE;
			}
			else
			{
				if( m_bRenderBurnout == FALSE && pPed->IsDead())
				{
					// apply burnt damage pack first time through only
					PEDDAMAGEMANAGER.AddPedDamagePack(pPed, ATSTRINGHASH("Burnt_Ped_0",0xde3cdfba), 0.0f, 1.0f);
				}
				// mark burnt out if he's both scorched and dead
				this->m_bRenderBurnout = pPed->IsDead();
			}
		}

		this->m_bRenderDead = FALSE;  
		if(this->m_bRenderBurnout)
		{	// is ped dead & was on fire?
			if(pPed->IsDead())
			{
				this->m_bRenderDead = TRUE;
	
				// B*3038261 : We need to render the helmet when you are burning
				if( !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet) )
					pPed->GetPedDrawHandler().GetPropData().SetSkipRender(true);	// clear ped props if ped is burnout
				
				if (m_bTriggeredSmokeFx==FALSE)
				{
					g_vfxFire.TriggerPtFxPedFireSmoulder(pPed);
					m_bTriggeredSmokeFx = TRUE;
				}
			}
		}
		else if(this->m_bRenderVehBurnout)
		{
			// B*3038261 : We need to render the helmet when you are burning
			if(pPed->IsDead() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet))
			{
				pPed->GetPedDrawHandler().GetPropData().SetSkipRender(true);	// clear ped props if ped is burnout
			}
		}
		else if(pPed->IsClothingWet())
		{
			this->m_varWetClothesData = pPed->GetWetClothingData();
		}

		if(this->m_bRenderBurnout || this->m_bRenderVehBurnout)
		{
			const float fBurnoutLevelInc =
											#if CSE_PED_EDITABLEVALUES
												ms_fEVBurnoutLevelSpeed;
											#else
												0.02f;
											#endif
			this->SetBurnoutLevel(GetBurnoutLevel()+fBurnoutLevelInc);
		}
		else
		{
			this->SetBurnoutLevel(0.0f);
		}

		m_varHeat = (u8)(CCustomShaderEffectPed::GetPedHeat(pPed, pModelInfo->GetThermalBehaviour()));
	
		crCreature* creature = pedDynComp->GetCreature();
		
		UpdateExpression(creature, pedDynComp->GetTargetManager()); 

		this->SetStubbleGrowth(pPed->GetCurrentStubbleGrowth());

		const float fWindyClothingScale = pPed->GetWindyClothingScale();
		if(fWindyClothingScale > 0.0f)
		{
			const float umOverride = Lerp(fWindyClothingScale, 1.0f, 21.0f);
			this->SetUmGlobalOverrideParams(umOverride, umOverride, 1.0f, 1.0f);
		}
		else
		{
			//if nothing is affecting micromovement then allow wind to do it
			bool bApplyWinUm = true;

			if(pPed->IsLocalPlayer())
			{
				// is player inside vehicle?
				if(pPed->IsInFirstPersonVehicleCamera() && pPed->GetIsInVehicle())
				{
					bApplyWinUm = false;	// disable wind um if in FPP inside vehicle

					CVehicle *pPedVehicle = pPed->GetVehiclePedInside();
					if(pPedVehicle && pPedVehicle->GetVehicleType()==VEHICLE_TYPE_CAR)
					{
						enum { MID_TORNADO4 = 0x86CF7CDD };	// "tornado4"

						// FPP player driving roofless car or one with open roof?
						if(pPedVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
						{	
							// convertible cars with lowered roof:	
							const s32 roofState = pPedVehicle->GetConvertibleRoofState();
							bApplyWinUm =	(roofState==CTaskVehicleConvertibleRoof::STATE_LOWERED)			||
											(roofState==CTaskVehicleConvertibleRoof::STATE_LOWERING)		||
											(roofState==CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_LOWERED);
						}
						else if(pPedVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_CONVERTIBLE))
						{
							// cars with removable roof as an extra:
							bApplyWinUm = !pPedVehicle->CarHasRoof(false);
						}
						else if(pPedVehicle->GetBaseModelInfo()->GetModelNameHash()==MID_TORNADO4)
						{
							bApplyWinUm = true;
						}
					}
				}
			}// if(IsLocalPlayer())...

			if(bApplyWinUm)
			{
				//initial flutter intensity is zero as we dont have an ambient micromovement like plants and trees
				float flutterIntentity = 0.0f;
			#if CSE_PED_EDITABLEVALUES
				if (ms_bEVWindResponseEnabled && (ms_fEVWindFlutterIntensity > 0.0f))
			#endif
				{
					Vec3V wind;
					//Using global velocity as ped causes disturbances in local velocity
					//const Vec3V entityPos = pEntity->GetTransform().GetPosition();
					//WIND.GetLocalVelocity(entityPos, wind);
					WIND.GetGlobalVelocity(WIND_TYPE_AIR, wind);
					float windVel = Mag(wind).Getf();
					const float windVelMin = BANK_SWITCH(ms_fEVWindResponseVelMin, PED_DEFAULT_WIND_RESPONSE_VEL_MIN);
					const float windVelMax = BANK_SWITCH(ms_fEVWindResponseVelMax, PED_DEFAULT_WIND_RESPONSE_VEL_MAX);

					// Calculate wind response in [0.0, 1.0f]
					const float windResponse = Clamp<float>((windVel - windVelMin)/(windVelMax - windVelMin), 0.0f, 1.0f);
				
					//Flutter Intensity is used for scaling the amplitude of the micromovement
					flutterIntentity += windResponse * BANK_SWITCH(ms_fEVWindFlutterIntensity, PED_DEFAULT_WIND_FLUTTER_INTENSITY);
				}
						
				//set override params based on some parameters
				// Higher winds causes higher amplitude and higher frequency (looks like cloth flutter)
				this->SetUmGlobalOverrideParams(
						BANK_SWITCH(ms_fEVWindMotionAmpX,	PED_DEFAULT_WIND_MOTION_AMP_X ) BANK_ONLY(*(ms_bEVWindMotionEnabled? 1.0f : 0.0f)) * flutterIntentity,
						BANK_SWITCH(ms_fEVWindMotionAmpY,	PED_DEFAULT_WIND_MOTION_AMP_Y ) BANK_ONLY(*(ms_bEVWindMotionEnabled? 1.0f : 0.0f)) * flutterIntentity,
						BANK_SWITCH(ms_fEVWindMotionFreq,	PED_DEFAULT_WIND_MOTION_FREQ),
						BANK_SWITCH(ms_fEVWindMotionFreq,	PED_DEFAULT_WIND_MOTION_FREQ)
						);
			}// if(bApplyWinUm)...
			else
			{
				this->SetUmGlobalOverrideParams(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}

 		UpdatePedDamage(pPed);
		SetEnvEffColorModulator(pPed->GetDirtColor());
		
		if(!m_bCustomEnvEffScale)
		{
			SetEnvEffScale(pPed->GetDirtScale());
		}

		if(IsFurShader())
		{
			if(pPed->GetIsInVehicle())
			{
				m_vLocalWind[0] = m_vLocalWind[1] = m_vLocalWind[2] = 0;
			}
			else
			{
				// cache the local wind vector in the shader
				const Vec3V entityPos = pEntity->GetTransform().GetPosition();
				Vec3V localWind;
				WIND.GetLocalVelocity(entityPos, localWind);
				m_vLocalWind[0] = s16(localWind.GetXf() * (1<<PED_WIND_FIXED_POINT));
				m_vLocalWind[1] = s16(localWind.GetYf() * (1<<PED_WIND_FIXED_POINT));
				m_vLocalWind[2] = s16(localWind.GetZf() * (1<<PED_WIND_FIXED_POINT));
			}
		}
		SetSweatScale(pPed->GetSweatScale());

		// calculate detail scale
		const grcViewport *viewport = gVpMan.GetUpdateGameGrcViewport();
		const Vec3V camPos = viewport->GetCameraMtx().d();
		const Vector3 pos = pPed->GetPreviousPosition();
		const Vec3V vpos = VECTOR3_TO_VEC3V(pos);
		float distToCamera = Dist(camPos, vpos).Getf();
		Vector2 fadeOut = this->m_bRenderBurnout? ms_DetailFadeOut[1] : ms_DetailFadeOut[0];
		float detailScale = 1.0f - Clamp( (distToCamera - fadeOut.x)/(fadeOut.y - fadeOut.x), 0.0f, 1.0f);
		m_PedDetailScale = PackToU8(detailScale);

		bool cutScenePlaying = camInterface::GetCutsceneDirector().IsCutScenePlaying();
		m_useCutsceneTech = cutScenePlaying BANK_ONLY(&& g_useOptimisedCutsceneTechnique );
	}//	if(pPed)...


}// end of Update()...


//
//
//
//
void CCustomShaderEffectPed::EnableShaderVarCaching(bool value)
{
	ms_EnableShaderVarCaching[g_RenderThreadIndex] = value && g_UsePedShaderVarCaching;

	if(ms_EnableShaderVarCaching[g_RenderThreadIndex])
	{
		//InvalidateShaderVarCache();	// not necessary due to as CPedVariationStream/Pack::RenderPed() uses var caching
	}
}


inline Vector4 ToVector4(u8 packed[4])
{
	const float scale = 1.0f / 255;
	return Vector4(packed[0] * scale, packed[1] * scale, packed[2] * scale, packed[3] * scale);
}

inline Vec4V ToVec4V(u8 packed[4])
{
	const ScalarV vScale(1.0f / 255.0f);
	return Vec4V((float)packed[0], (float)packed[1], (float)packed[2], (float)packed[3]) * vScale;
}

inline void FromVector4(u8 packed[4],const Vector4 &from)
{
	packed[0] = u8(from.x * 255);
	packed[1] = u8(from.y * 255);
	packed[2] = u8(from.z * 255);
	packed[3] = u8(from.w * 255);
}


#define m_ComponentBlendWeights		((ComponentBlendWeights*)(this+1))
//
//
//
//
//
void CCustomShaderEffectPed::UpdateExpression(crCreature* pCreature, grbTargetManager* ENABLE_BLENDSHAPES_ONLY(pManager))
{
#if ENABLE_BLENDSHAPES
	bool blendShapeExistence[1 /*MAX_PED_COMPONENTS*/] = {0};

	if (pManager)
	{
		m_Manager = pManager; 
	}
#endif // ENABLE_BLENDSHAPES
	
	if (pCreature)
		{
			const int numComponents = pCreature->GetNumComponents();
			for (int i = 0; i < numComponents; ++i)
			{
				crCreatureComponent* component = pCreature->GetComponentByIndex(i);
				if (component->GetType() == crCreatureComponent::kCreatureComponentTypeShaderVars)
				{
					crCreatureComponentShaderVars* shaderVarComponent = static_cast<crCreatureComponentShaderVars*>(component);

					const u32 sWrinkleWeightHash = ATSTRINGHASH("WrinkleWeights", 0x0f09864d6);
					const u32 sFatPedShaderHash = ATSTRINGHASH("FatStrength", 0x03b4cfb00);
					//const u32 sNormalMapBlendHash = ATSTRINGHASH("NormalMapBlend", 0x0310e4a57);

					const u32 numPairs = shaderVarComponent->GetNumShaderVarDofPairs();
					for(u32 n=0; n<numPairs; ++n)
					{
						u8 track;
						u16 id;
						fwShaderVarComponentData data;
						float value;
						shaderVarComponent->GetShaderVarDofPair(n, track, id, data.m_Packed, value);

						if(data.unpack.m_Hash == sWrinkleWeightHash)
						{
							const int componentWr = s_wrinkleTrans[data.unpack.m_PedCompId];
							if(componentWr >= 0)
							{
								const int wrinkleMask = data.unpack.m_MaskId;

								const int idx = data.unpack.m_Component;
								const float fVal = FPClamp(value, 0.f, 1.f);
								const int iVal = (int)(fVal * 255.0f);

								Assert((iVal & 0xffffff00) == 0);
								Assert(componentWr<4);
								Assert(wrinkleMask<6);
								Assert(idx<4);
								Assign(m_wrinkleMaskStrengths[componentWr][wrinkleMask][idx], (u8)iVal);

#if __BANK
								if (!ms_wrinkleEnableSliders)
								{
									ms_wrinkleMaskStrengths[wrinkleMask] = ToVector4(m_wrinkleMaskStrengths[componentWr][wrinkleMask]);
								}
#endif // __BANK
							}
						}
						else if(data.unpack.m_Hash == sFatPedShaderHash)
						{
							SetFatScale(value);
						}
					}	
				}
#if ENABLE_BLENDSHAPES
				else if (pManager && component->GetType() == crCreatureComponent::kCreatureComponentTypeBlendShapes)
				{
					crCreatureComponentBlendShapes* blendshapeComponent = static_cast<crCreatureComponentBlendShapes*>(component);

					const int numBlendWeights = blendshapeComponent->GetBlendWeightCount();
					int component = PV_COMP_HEAD; // blendshapeComponent->GetComponentType(); todo
					
					if (!m_bHasBlendshapes)
						Errorf("CCustomShaderEffectPed allocated without blend weights, cannot update");
					else
					{
						ComponentBlendWeights *componentBlendWeights = (ComponentBlendWeights*)(this + 1);

						if (!componentBlendWeights[component].m_BlendWeights.GetCount())
						{
							componentBlendWeights[component].m_BlendWeights.Resize(numBlendWeights);
							Displayf("********* setting ped blend weight array size to %u",numBlendWeights);
						}

						blendShapeExistence[component] = true;
						componentBlendWeights[component].m_Exists = true;

						for (int j = 0 ; j < numBlendWeights; ++j)
						{
							const crCreatureComponentBlendShapes::BlendWeight& blendweight = blendshapeComponent->GetBlendWeight(j);

							componentBlendWeights[component].m_BlendWeights[j].m_Id = blendweight.m_Id;
							int iWeight = (int)(blendweight.m_Weight * 32768.0f);
							componentBlendWeights[component].m_BlendWeights[j].m_FixedWeight = u16(iWeight<0?0:iWeight>65535?65536:iWeight);
						}
					}
				}
#endif // ENABLE_BLENDSHAPES
			}
		}

#if __BANK
		const int componentWr = ms_wrinkleDebugComponent;
		if (ms_wrinkleEnableSliders && componentWr >= 0 && componentWr <= 2)
		{
			FromVector4(m_wrinkleMaskStrengths[componentWr][0], ms_wrinkleMaskStrengths[0]);
			FromVector4(m_wrinkleMaskStrengths[componentWr][1], ms_wrinkleMaskStrengths[1]);
			FromVector4(m_wrinkleMaskStrengths[componentWr][2], ms_wrinkleMaskStrengths[2]);
			FromVector4(m_wrinkleMaskStrengths[componentWr][3], ms_wrinkleMaskStrengths[3]);
			FromVector4(m_wrinkleMaskStrengths[componentWr][4], ms_wrinkleMaskStrengths[4]);
			FromVector4(m_wrinkleMaskStrengths[componentWr][5], ms_wrinkleMaskStrengths[5]);
		}
#endif // __BANK

#if ENABLE_BLENDSHAPES
		for (int i = 0; i < 1 /*MAX_PED_COMPONENTS*/; ++i)
		{
			if (blendShapeExistence[i] == false && m_bHasBlendshapes) 
			{
				m_ComponentBlendWeights[i].m_BlendWeights.Reset();
			}
		}
#endif // ENABLE_BLENDSHAPES
}

void CCustomShaderEffectPed::AddToDrawList(u32 modelIndex, bool bExecute)
{
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
}

//
//
//
//
void CCustomShaderEffectPed::SetShaderVariables(rmcDrawable*)
{
//
// note:
// this method is not called from CDynamic\Entity::Render()
// instead, PedVariationMgr_SetShaderVariables() is called from CPedVariationMgr::RenderCompositePed()
//
	Assert(0);
}// end of SetShadersVariables()...

// helper function for masking off un-marked areas
float CalcPedDamageEnable(float maskIn, int component)
{
	// shortcut - no blood/decal
	if( maskIn == 0.0f )
	{
		return 0.0f;
	}
	// store signbit for later
	float signBit = Sign(maskIn); 

	/// zone mask values
	//kDamageZoneTorso=1,	
	//kDamageZoneHead=2,
	//kDamageZoneLeftArm=4,
	//kDamageZoneRightArm=8,
	//kDamageZoneLeftLeg=16,
	//kDamageZoneRightLeg=32,

	// we combine zone mask with component mask
	u16 originalMask = static_cast<u16>(signBit*maskIn);
	static u16 componentMasks[PV_MAX_COMP] = {	1|2,    		// PV_COMP_HEAD
												2,				// PV_COMP_BERD
												2,				// PV_COMP_HAIR
												1|2|4|8|16|32,	// PV_COMP_UPPR
												1|16|32,		// PV_COMP_LOWR
												4|8,			// PV_COMP_HAND
												16|32,			// PV_COMP_FEET
												2,				// PV_COMP_TEEF
												0xff,			// PV_COMP_ACCS
												0xff,			// PV_COMP_TASK
												0xff,			// PV_COMP_DECL
												0xff };			// PV_COMP_JBIB
														 
	return static_cast<float>(componentMasks[component] & originalMask)*signBit;	
}

//
//
// called from CPedVariationMgr::RenderCompositePed():
//
#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
void CCustomShaderEffectPed::PedVariationMgr_SetShaderVariables(rmcDrawable *pDrawable, s32 componentID, const Matrix34 *pClothParentMatrix )
#else
void CCustomShaderEffectPed::PedVariationMgr_SetShaderVariables(rmcDrawable *pDrawable, s32 componentID )
#endif
{
	Assert(pDrawable);
	Assert(componentID >= 0);
	Assert(componentID <  PV_MAX_COMP);
	if(!pDrawable)
		return;

	if(grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();
	Assert(shaderGroup.GetShaderGroupVarCount() > 0);

	CCustomShaderEffectPedType *pType = m_pType;

	// find the mask for this component
	float revisedPedDamageDataX = CalcPedDamageEnable(m_PedDamageData.GetX(), componentID);

	// set the cutsceneFlag per component for possible cheap technique
	if (m_useCutsceneTech && DRAWLISTMGR->IsExecutingGBufDrawList())
	{
		if(revisedPedDamageDataX == 0.0f)
			DeferredLighting::PushCutscenePedTechnique();
	}

	if(m_varhDiffuseTex[componentID] BANK_ONLY(&& !g_ShaderEdit::GetInstance().IsTextureDebugOn()))
	{
#if __BANK
		if (CPedVariationDebug::GetDisableDiffuse() && CPedVariationDebug::GetGreyDebugTexture())
		{
			shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex, CPedVariationDebug::GetGreyDebugTexture());
		}
		else
#endif
		{
			shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex,		m_varhDiffuseTex[componentID]);
		}
	}

	if(m_varhNormalTex[componentID])
	{
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarNormalTex, m_varhNormalTex[componentID]);
	}

	if(m_varhSpecularTex[componentID])
	{
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarSpecularTex, m_varhSpecularTex[componentID]);
	}

	if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (ms_CachedCSEPedShaderVars[g_RenderThreadIndex].umGlobalOverrideParams != m_umGlobalOverrideParams))
	{
		Assert(pType->m_idVarUmGlobalOverrideParams);

		const Vec4V umOverrideParamsV4(
			m_umGlobalOverrideParams.x.GetFloat32_FromFloat16(),
			m_umGlobalOverrideParams.y.GetFloat32_FromFloat16(),
			m_umGlobalOverrideParams.z.GetFloat32_FromFloat16(),
			m_umGlobalOverrideParams.w.GetFloat32_FromFloat16());
		ms_CachedCSEPedShaderVars[g_RenderThreadIndex].umGlobalOverrideParams = m_umGlobalOverrideParams;
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarUmGlobalOverrideParams, umOverrideParamsV4);
	}

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	if(pClothParentMatrix)
	{
		Assert(pType->m_idVarClothParentMatrix);
		Mat44V *pMatrix44 = (Mat44V*)pClothParentMatrix;
		pMatrix44->SetM30f(0.0f);
		pMatrix44->SetM31f(0.0f);
		pMatrix44->SetM32f(0.0f);
		pMatrix44->SetM33f(1.0f);
		grcEffect::SetGlobalVar((rage::grcEffectGlobalVar)pType->m_idVarClothParentMatrix,pMatrix44,1);
	}
#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS

	const float fEnvEffScale = this->GetEnvEffScale();
	const float fFatScale = this->GetFatScale();
	const Vec4V envEffFatSweatScale(
					fEnvEffScale,
					fEnvEffScale * (1.0f/1000.0f),
					fFatScale * (1.0f/1000.0f),
					this->GetSweatScale()			
				);

	if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].EnvEffFatSweatScale, envEffFatSweatScale)))
	{
		Assert(pType->m_idVarEnvEffFatSweatScale);
		ms_CachedCSEPedShaderVars[g_RenderThreadIndex].EnvEffFatSweatScale = envEffFatSweatScale;
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarEnvEffFatSweatScale, envEffFatSweatScale);
	}

	const float pedDetailScale = (float)m_PedDetailScale / 255.0f;



	if(DRAWLISTMGR->IsExecutingShadowDrawList())
	{
		// do nothing
	}
	else
	{
		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (ms_CachedCSEPedShaderVars[g_RenderThreadIndex].DiffuseTexPal != m_varhDiffuseTexPal[componentID]))
		{
			Assert(pType->m_idVarDiffuseTexPal);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].DiffuseTexPal = m_varhDiffuseTexPal[componentID];
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarDiffuseTexPal,	m_varhDiffuseTexPal[componentID]);
		}

		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (ms_CachedCSEPedShaderVars[g_RenderThreadIndex].DiffuseTexPalSelector != m_varDiffuseTexPalSelector[componentID]))
		{
			Assert(pType->m_idVarDiffuseTexPalSelector);

			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].DiffuseTexPalSelector = m_varDiffuseTexPalSelector[componentID];
			
			// convert palette selector to UV coordinate:
			// uv = (paletteSelector + 0.5f) / 8.0f;	// palette row select: 0-7
			const float divider = 1.0f / (GetDiffuseTexturePalLarge(componentID) ? PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM_LARGE : PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM);
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarDiffuseTexPalSelector,	(float(GetDiffuseTexturePalSelector(componentID))+0.5f) * divider);
		}

		const Vec4V vEnvEffColorMod_CpvAdd(GetEnvEffColorModulator().GetRGB(), ScalarV(GetEnvEffCpvAdd()));
		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].EnvEffColorMod_CpvAdd, vEnvEffColorMod_CpvAdd)))
		{
			Assert(pType->m_idVarEnvEffColorModCpvAdd);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].EnvEffColorMod_CpvAdd = vEnvEffColorMod_CpvAdd;
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarEnvEffColorModCpvAdd, vEnvEffColorMod_CpvAdd);
		}

	#if __XENON
		if(pClothParentMatrix)
		{
			Assert(pType->m_idVarClothParentMatrix);
			shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarClothParentMatrix, pClothParentMatrix, 1);
		}
	#endif // __XENON

	//#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	//	if(pClothParentMatrix)
	//	{
	//		Assert(pType->m_idVarClothParentMatrix);
	//		grcEffect::SetGlobalVar((rage::grcEffectGlobalVar)pType->m_idVarClothParentMatrix,(Mat44V*)pClothParentMatrix,1);
	//	}
	//#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS


		Assert(pType->m_idVarWrinkleMaskStrengths0);
		const int componentWr = s_wrinkleTrans[componentID];
		if(componentWr >= 0)
		{
			Vec4V values[6];
			values[0] = ToVec4V(m_wrinkleMaskStrengths[componentWr][0]);
			values[1] = ToVec4V(m_wrinkleMaskStrengths[componentWr][1]);
			values[2] = ToVec4V(m_wrinkleMaskStrengths[componentWr][2]);
			values[3] = ToVec4V(m_wrinkleMaskStrengths[componentWr][3]);
			values[4] = ToVec4V(m_wrinkleMaskStrengths[componentWr][4]);
			values[5] = ToVec4V(m_wrinkleMaskStrengths[componentWr][5]);

			const bool bSetShaderVars = (!ms_EnableShaderVarCaching[g_RenderThreadIndex])													||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[0], values[0]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[1], values[1]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[2], values[2]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[3], values[3]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[4], values[4]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[5], values[5]))	;
			if(bSetShaderVars)
			{
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[0] = values[0];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[1] = values[1];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[2] = values[2];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[3] = values[3];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[4] = values[4];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[5] = values[5];
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarWrinkleMaskStrengths0, values, 6);
			}
		}
		else
		{
			const Vec4V values[6] = { Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO) };

			const bool bSetShaderVars = (!ms_EnableShaderVarCaching[g_RenderThreadIndex])													||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[0], values[0]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[1], values[1]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[2], values[2]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[3], values[3]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[4], values[4]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[5], values[5]))	;	
			if(bSetShaderVars)
			{
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[0] = values[0];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[1] = values[1];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[2] = values[2];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[3] = values[3];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[4] = values[4];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WrinkleMaskStrenghts[5] = values[5];
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarWrinkleMaskStrengths0, values, 6);
			}
		}

	#if ENABLE_BLENDSHAPES
		if (m_Manager && componentID == PV_COMP_HEAD && m_bHasBlendshapes && m_ComponentBlendWeights[componentID].m_Exists && !m_ComponentBlendWeights[componentID].m_Processed)
		{
			SetupBlendshapeWeights(pDrawable, componentID);
			m_ComponentBlendWeights[componentID].m_Processed = true;
		}
	#endif // ENABLE_BLENDSHAPES

		const Vec2V vStubbleGrowthDetScale(1.0f-GetStubbleGrowth(), pedDetailScale);
		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].StubbleGrowthDetScale, vStubbleGrowthDetScale)))
		{
			Assert(pType->m_idVarStubbleGrowthDetailScale);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].StubbleGrowthDetScale = vStubbleGrowthDetScale;
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarStubbleGrowthDetailScale, vStubbleGrowthDetScale);
		}

		if(componentID == PV_COMP_HAIR)
		{
			Assert(pType->m_idVarStubbleGrowthDetailScale);

			// convert palette selector to UV coordinate:
			const float divider = 1.0f / (GetDiffuseTexturePalLarge(componentID) ? PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM_LARGE : PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM);

			Vec2V vec;
			vec.SetX(((float)m_secondHairPalSelector + 0.5f) * divider);
			vec.SetY(pedDetailScale);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].StubbleGrowthDetScale = vec; // nuke the cache for the stubble so we don't end up using this value
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarStubbleGrowthDetailScale, vec);
		}
	

		const ScalarV vHeat((float)m_varHeat/255.0f);
		const ScalarV vEmissiveScale(GetEmissiveScale());
		const Vec4V pedNormal(ScalarV(1.0f), vEmissiveScale, ScalarV(1.0f), vHeat);
		const Vec4V pedNormal2(V_ONE);
		const Vec4V pedVehBurnout(ScalarV(0.05f),Vec2V(V_ZERO), vHeat);
		const Vec4V pedVehBurnout2(V_ZERO);
		const Vec4V pedDead(ScalarV(0.20f),Vec2V(V_ZERO), vHeat);
		const Vec4V pedDead2(V_ZERO);
		const ScalarV burnLevel(this->GetBurnoutLevel());
		const Vec4V vColorScale0 = m_bRenderDead? Lerp(burnLevel,pedNormal,pedDead)   : m_bRenderVehBurnout? Lerp(burnLevel,pedNormal,pedVehBurnout)   : pedNormal;
		const Vec4V vColorScale1 = m_bRenderDead? Lerp(burnLevel,pedNormal2,pedDead2) : m_bRenderVehBurnout? Lerp(burnLevel,pedNormal2,pedVehBurnout2) : pedNormal2;
		if(	(!ms_EnableShaderVarCaching[g_RenderThreadIndex])													||
			(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].MaterialColorScale[0], vColorScale0))	||
			(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].MaterialColorScale[1], vColorScale1))	)
		{
			Assert(pType->m_idVarMaterialColorScale);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].MaterialColorScale[0] = vColorScale0;
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].MaterialColorScale[1] = vColorScale1;

			Vec4V vColorScaleAll[2] = {vColorScale0, vColorScale1};
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarMaterialColorScale, &vColorScaleAll[0], 2);
		}

		const Vec4V vWetClothesData = RCC_VEC4V(m_varWetClothesData);
		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WetClothesData, vWetClothesData)))
		{
			Assert(pType->m_idVarWetClothesData);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WetClothesData = vWetClothesData;
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarWetClothesData, vWetClothesData);
		}

		// if we are in the GBUF or Hud passes we should set the Data Data And samplers
		bool bIsGbufPass = DRAWLISTMGR->IsExecutingGBufDrawList();
		bool bIsHUDPass = DRAWLISTMGR->IsExecutingHudDrawList();
		bool bIsMirrorPass = m_usePedDamageInMirror && DRAWLISTMGR->IsExecutingMirrorReflectionDrawList();

		if (bIsHUDPass || bIsGbufPass || bIsMirrorPass)
		{
			// outside of GBUF pass, s15 is used for shadows, so don't mess with it
			if(bIsGbufPass || bIsHUDPass)
			{
				if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedBloodTarget != m_PedBloodTarget))
				{
					Assert(pType->m_idVarPedBloodTarget);
					ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedBloodTarget = m_PedBloodTarget;
					grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedBloodTarget, m_PedBloodTarget);
				}
			}

			// check whether there's a decal component that needs to override the ped tattoo target
			if (componentID == PV_COMP_DECL && m_PedTattooTargetOverride != NULL)
			{
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedTattooTarget = m_PedTattooTargetOverride;
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedTattooTarget, m_PedTattooTargetOverride);

				shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex, grcTexture::NoneBlack);
				shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarNormalTex, grcTexture::NoneWhite);
				shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarSpecularTex, grcTexture::NoneBlack);
			}
			// we can set tattoo global sampler, since it's in s14 and that is not used by the shadows
			else if (bIsMirrorPass)
			{
				// skip the cache for now, since I don't have a grcTextureHandle, just a plan old grctexture.
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedTattooTarget, (m_PedDamageData.x!=0.0f) ? CPedDamageSetBase::GetMirrorDecorationTarget() : NULL);
			}
			else if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedTattooTarget != m_PedTattooTarget))		
			{
				Assert(pType->m_idVarPedTattooTarget);
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedTattooTarget = m_PedTattooTarget;
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedTattooTarget, m_PedTattooTarget);
			}
		}
		else if(DRAWLISTMGR->IsExecutingMirrorReflectionDrawList() && !m_usePedDamageInMirror)
		{
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedTattooTarget, grcTexture::NoneBlack);
		}

		Vec4V vPedDamageData;
		if (bIsHUDPass || bIsGbufPass || bIsMirrorPass)
		{
			vPedDamageData = RCC_VEC4V(m_PedDamageData); // only set the blood damage data for the gbuffer and hud passes (for UI peds)

			// if the tattoo target has been overridden, force ped damage on
			if (componentID == PV_COMP_DECL && m_PedTattooTargetOverride != NULL)
			{
	#if __XENON
				vPedDamageData.SetX(ScalarV(0xFF));
	#else
				vPedDamageData.SetX(ScalarV(V_ONE));
	#endif
			}
			else
			{
	#if __XENON
				vPedDamageData.SetX(revisedPedDamageDataX);
	#else
				vPedDamageData.SetX(Sign(revisedPedDamageDataX));
	#endif
				if (bIsMirrorPass)
				{
					// need to update the damage data since thge target is different.
					CPedDamageSetBase::AdjustPedDataForMirror(vPedDamageData);
				}
			}
		}
		else
			vPedDamageData = Vec4V(V_ZERO); // for all other passes, make sure this is off, so shaders access the in valid render targets

		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageData, vPedDamageData)))
		{
			Assert(pType->m_idVarPedDamageData);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageData = vPedDamageData;
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedDamageData, vPedDamageData);
		}

		const Vec4V wetnessAdjustOverWater = Vec4V(0.35f, 8.0f, 0.35f, 0.1f);
		const Vec4V wetnessAdjustUnderWater = wetnessAdjustOverWater * Vec4V(V_X_AXIS_WONE);
		const Vec4V wetnessAdjust = (Water::IsCameraUnderwater()) ? wetnessAdjustUnderWater :  wetnessAdjustOverWater;
		if((!ms_EnableShaderVarCaching[g_RenderThreadIndex]) || (!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WetnessAdjust, wetnessAdjust)))
		{
			Assert(pType->m_idVarWetnessAdjust);
			ms_CachedCSEPedShaderVars[g_RenderThreadIndex].WetnessAdjust = wetnessAdjust;
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarWetnessAdjust, wetnessAdjust);
		}

	#if DEVICE_MSAA
		if (!GRCDEVICE.GetMSAA())
	#endif
		{
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarAlphaToCoverageScale, 0.0f);
		}

		// NOTE: this could go away if we ever pick a fixed set of colors or we make them globals		
		if(pType->m_idVarPedDamageColors)
		{
			Vec4V* pPedDamageColors = (Vec4V*)CPedDamageManager::GetInstance().GetPedDamageColors();
			const bool bSetShaderVars = (!ms_EnableShaderVarCaching[g_RenderThreadIndex])														||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageColors[0], pPedDamageColors[0]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageColors[1], pPedDamageColors[1]))	||
										(!IsEqualAll(ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageColors[2], pPedDamageColors[2]))	;
			if(bSetShaderVars)
			{
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageColors[0] = pPedDamageColors[0];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageColors[1] = pPedDamageColors[1];
				ms_CachedCSEPedShaderVars[g_RenderThreadIndex].PedDamageColors[2] = pPedDamageColors[2];
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedDamageColors, pPedDamageColors, 3);
			}
		}

	}//if(!DRAWLISTMGR->IsExecutingShadowDrawList())...


#if CSE_PED_EDITABLEVALUES
	ms_fEVEnvEffZoneScale		= this->GetEnvEffScale();
	ms_fEVEnvEffZoneColorModR	= this->GetEnvEffColorModulator().GetRed();
	ms_fEVEnvEffZoneColorModG	= this->GetEnvEffColorModulator().GetGreen();
	ms_fEVEnvEffZoneColorModB	= this->GetEnvEffColorModulator().GetBlue();
	CCustomShaderEffectPed::SetEditableShaderValues(&shaderGroup, pDrawable, pType, pedDetailScale);
#endif //CSE_PED_EDITABLEVALUES...

}// end of PedVariation_SetShaderVariables()...

void CCustomShaderEffectPed::PedVariationMgr_SetWetnessAdjust(int wetness)
{
	Assert(m_pType->m_idVarWetnessAdjust);
	Vector4 wetnessAdjust;
	if(wetness == -1)
		wetnessAdjust = ms_fEVWetClothesAdjustLessWet;
	else if(wetness == 1)
		wetnessAdjust = ms_fEVWetClothesAdjustMoreWet;
	else
		wetnessAdjust = ms_fEVWetClothesAdjust;
	wetnessAdjust = (Water::IsCameraUnderwater()) ? wetnessAdjust * Vector4(1.0f, 0.0f, 0.0f, 1.0f) :  wetnessAdjust;
	grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarWetnessAdjust, VECTOR4_TO_VEC4V(wetnessAdjust));
}

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
void CCustomShaderEffectPed::PedVariationMgr_SetShaderClothVertices(rmcDrawable *UNUSED_PARAM(pDrawable), u32 numVertices, const Vec3V *pClothVertices )
{
	Assertf(numVertices <= DX11_CLOTH_MAX_VERTICES, "CCustomShaderEffectPed::PedVariationMgr_SetShaderClothVertices()...Ped cloth has too many vertices.");
	CCustomShaderEffectPedType *pType = m_pType;
	if(pType && pType->m_idVarClothVertices)
	{
		grcEffect::SetGlobalVar((rage::grcEffectGlobalVar)pType->m_idVarClothVertices,pClothVertices,numVertices);
	}
}
#endif	// RSG_PC || RSG_DURANGO || RSG_ORBIS

#if ENABLE_BLENDSHAPES
//
//
//
//
void CCustomShaderEffectPed::SetupBlendshapeWeights(rmcDrawable *pDrawable, s32 componentID)
{
	Assert(m_Manager);

	atFixedArray<BlendWeight,MAX_PED_BLEND_WEIGHTS>& blendWeights = m_ComponentBlendWeights[componentID].m_BlendWeights;
	const int numBlendWeights = blendWeights.GetCount();

	m_Manager->PrepareBlendShapeOffsets();

	m_Manager->ResetBlendShapeOffsets();

	for(int idx=0; idx<numBlendWeights; ++idx)
	{
		BlendWeight& blendWeight = blendWeights[idx];
		float currentWeight = blendWeight.m_FixedWeight * (1.0f / 32768.0f);

#if __BANK
		if (ms_bBlendshapeForceAll || (ms_bBlendshapeForceSpecific && (ms_nBlendshapeForceSpecificIdx == idx)))
		{
			currentWeight = ms_fBlendshapeForceSpecificBlend;
		}
#endif // __BANK

		m_Manager->BlendTarget(blendWeight.m_Id, currentWeight);
	}

	m_Manager->ReleaseBlendShapeOffsets();
	
	m_Manager->SwapBuffers();

	pDrawable->ApplyBlendShapeOffsets(*m_Manager);
}
#endif // ENABLE_BLENDSHAPES

void CCustomShaderEffectPed::ClearShaderVars(rmcDrawable *pDrawable, s32 componentID)
{
	Assert(pDrawable);

	if(grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;

    if (g_ShaderEdit::GetInstance().IsTextureDebugOn())
        return;
#endif

	if(DRAWLISTMGR->IsExecutingGBufDrawList())
		DeferredLighting::PopCutscenePedTechnique();

	grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();
	if (!Verifyf(shaderGroup.GetShaderGroupVarCount() > 0, "No shader vars on ped drawable for component %d, was there a diffuse missing?", componentID))
		return;

	CCustomShaderEffectPedType *pType = m_pType;

	shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex,			(grcTexture*)NULL);

	if(m_varhNormalTex[componentID])
	{
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarNormalTex,      (grcTexture*)NULL);
	}

	if(m_varhSpecularTex[componentID])
	{
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarSpecularTex,    (grcTexture*)NULL);
	}
}

void CCustomShaderEffectPed::ClearGlobalShaderVars()
{
	if(DRAWLISTMGR->IsExecutingShadowDrawList())
		return;

	CCustomShaderEffectPedType *pType = m_pType;

	Assert(pType->m_idVarDiffuseTexPal);
	grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarDiffuseTexPal,	(grcTexture*)NULL);

	if(DRAWLISTMGR->IsExecutingGBufDrawList())
	{
		Assert(pType->m_idVarPedBloodTarget);
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedBloodTarget,	(grcTexture*)NULL);
		Assert(pType->m_idVarPedTattooTarget);
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarPedTattooTarget,	(grcTexture*)NULL);
	}
}

float CCustomShaderEffectPed::GetPedHeat(CPed *ped, u32 thermalBehaviour)
{
	const u32 coldMeatOffset = 2000;
	const u32 currentTime = fwTimer::GetTimeInMilliseconds();
	
	float heat = 1.0f;
	if( ped->IsDead() && currentTime - ped->GetTimeOfDeath() >= coldMeatOffset)
	{
		const float coldMeatTiming = 10000.0f;

		const float msSinceDead = (float)(fwTimer::GetTimeInMilliseconds() - (ped->GetTimeOfDeath() + coldMeatOffset));
		heat = 1.0f - Clamp(msSinceDead/coldMeatTiming,0.0f,1.0f);
	}

	// a value of zero means no override
	float heatScaleOverride = Clamp(ped->GetHeatScaleOverride(), SEETHROUGH_HEAT_SCALE_COLD, SEETHROUGH_HEAT_SCALE_WARM);
	const float heatScale = (heatScaleOverride==0.0f) ? RenderPhaseSeeThrough::GetHeatScale(thermalBehaviour) * 255.0f : heatScaleOverride * 255.0f;
	return heat * heatScale;
}


// um global override params:
void CCustomShaderEffectPed::SetUmGlobalOverrideParams(float scaleH, float scaleV, float argFreqH, float argFreqV)
{
	m_umGlobalOverrideParams.x.SetFloat16_FromFloat32(scaleH);
	m_umGlobalOverrideParams.y.SetFloat16_FromFloat32(scaleV);
	m_umGlobalOverrideParams.z.SetFloat16_FromFloat32(argFreqH);
	m_umGlobalOverrideParams.w.SetFloat16_FromFloat32(argFreqV);
}

// um global override params:
void CCustomShaderEffectPed::SetUmGlobalOverrideParams(float s)
{
Float16 s16(s);
	m_umGlobalOverrideParams.x =
	m_umGlobalOverrideParams.y =
	m_umGlobalOverrideParams.z =
	m_umGlobalOverrideParams.w = s16;
}

void CCustomShaderEffectPed::GetUmGlobalOverrideParams(float *pScaleH, float *pScaleV, float *pArgFreqH, float *pArgFreqV)
{
	if(pScaleH)
	{
		*pScaleH = m_umGlobalOverrideParams.x.GetFloat32_FromFloat16(); 
	}

	if(pScaleV)
	{
		*pScaleV = m_umGlobalOverrideParams.y.GetFloat32_FromFloat16();
	}

	if(pArgFreqH)
	{
		*pArgFreqH = m_umGlobalOverrideParams.z.GetFloat32_FromFloat16();
	}

	if(pArgFreqV)
	{
		*pArgFreqV = m_umGlobalOverrideParams.w.GetFloat32_FromFloat16();
	}
}

void CCustomShaderEffectPed::UpdatePedDamage(const CPed * pPed)
{
	grcTexture* pBloodTarget;
	grcTexture* pTattooTarget;

	CPedDamageManager::GetInstance().GetRenderData(pPed->GetDamageSetID(), pPed->GetCompressedDamageSetID(), pBloodTarget, pTattooTarget, m_PedDamageData);

	m_PedBloodTarget = pBloodTarget;
	m_PedTattooTarget = pTattooTarget;
	m_PedTattooTargetOverride = NULL;
	m_usePedDamageInMirror = (pPed->IsLocalPlayer() || CPedDamageManager::GetInstance().WasPedDamageClonedFromLocalPlayer(pPed->GetDamageSetID())) && (m_PedDamageData.x!=0.0f) && CMirrors::GetIsMirrorVisible(true) && !Water::IsUsingMirrorWaterSurface();

	// We only want to set the tattoo target override (used for crew emblem decals) if the ped actually
	// has a component using one
	const CPedVariationData& pedVarData = pPed->GetPedDrawHandler().GetVarData();
	if (pPed->GetEnableCrewEmblem() && pedVarData.IsCurrVarDecalDecoration())
	{		
		grcTexture* pCrewEmblemDecal = NULL;

		// is crew logo overrided by script?
		if(pedVarData.GetOverrideCrewLogoTxdHash() && pedVarData.GetOverrideCrewLogoTexHash())
		{
			strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlot(pedVarData.GetOverrideCrewLogoTxdHash()));
			if(Verifyf(txdId != -1, "Invalid / non streamed txd! Connected with use of OVERRIDE_PED_CREW_LOGO_TEXTURE() script command!"))
			{
				if (g_TxdStore.IsValidSlot(txdId))
				{
					fwTxd* pTxd = g_TxdStore.Get(txdId);
					if (Verifyf(pTxd, "Invalid txd pointer! Connected with use of OVERRIDE_PED_CREW_LOGO_TEXTURE() script command!"))
					{
						pCrewEmblemDecal = pTxd->Lookup(pedVarData.GetOverrideCrewLogoTexHash());
						Assertf(pCrewEmblemDecal, "Invalid crew emblem override texture! Connected with use of OVERRIDE_PED_CREW_LOGO_TEXTURE() script command!");

						if(pCrewEmblemDecal)
						{
							m_PedTattooTargetOverride = pCrewEmblemDecal;
						}
					}// if(pTxd)...
				} // if(g_TxdStore.IsValidSlot(txdId))...
			}// if(txdId != -1)...
		}
		else if(PEDDECORATIONMGR.GetCrewEmblemDecal(pPed, pCrewEmblemDecal))
		{
			m_PedTattooTargetOverride = pCrewEmblemDecal;
		}
	}
}

#if CSE_PED_EDITABLEVALUES
//
//
//
//
void CCustomShaderEffectPed::SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* UNUSED_PARAM(pDrawable), CCustomShaderEffectPedType* pType, float pedDetailScale)
{
	Assert(pShaderGroup);

	if(!ms_bEVEnabled)
		return;

#if 0
	// old way of doing things:
	if(m_idVarEVSpecFalloff)
		pShaderGroup->SetVar(m_idVarEVSpecFalloff,		ms_fEVSpecFalloff);

	if(m_idVarEVSpecIntensity)
		pShaderGroup->SetVar(m_idVarEVSpecIntensity,	ms_fEVSpecIntensity);

	if(m_idVarHairSpecularFactor2)
		pShaderGroup->SetVar(m_idVarHairSpecularFactor2,ms_fEVHairSpecularFactor2);
	// etc. etc.
#endif
	// ...and the new way:
	SetFloatShaderGroupVar(pShaderGroup,	"Specular",			ms_fEVSpecFalloff);
	SetFloatShaderGroupVar(pShaderGroup,	"SpecularColor",	ms_fEVSpecIntensity);
	SetFloatShaderGroupVar(pShaderGroup,	"Reflectivity",		ms_fEVReflectivity);
	SetFloatShaderGroupVar(pShaderGroup,	"Fresnel",			ms_fEVSpecFresnel);
	SetFloatShaderGroupVar(pShaderGroup,	"Bumpiness",		ms_fEVBumpiness);
	SetVector3ShaderGroupVar(pShaderGroup,	"detailSettings",	Vector3(ms_fEVDetailIntensity, ms_fEVDetailBumpIntensity, ms_fEVDetailScale));

	if(pType)
	{
		Assert(pType->m_idVarEnvEffFatSweatScale);
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarEnvEffFatSweatScale, Vec4V(ms_fEVEnvEffScale, ms_fEVEnvEffScale / 1000.0f, ms_fEVFatScale / 1000.0f, ms_fEVSweatScale));

		Assert(pType->m_idVarEnvEffColorModCpvAdd);
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarEnvEffColorModCpvAdd, Vec4V(ms_fEVEnvEffColorMod.GetRGB(),ScalarV(ms_fEVEnvEffCpvAdd)));

		Assert(pType->m_idVarStubbleGrowthDetailScale);
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarStubbleGrowthDetailScale, Vec2V(1.0f-ms_fEVStubbleStrength, pedDetailScale));

		if(ms_bEVUseUmGlobalOverrideValues)
		{
			Assert(pType->m_idVarUmGlobalOverrideParams);
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarUmGlobalOverrideParams, VECTOR4_TO_VEC4V(ms_fEVUmGlobalOverrideParams));
		}
	}

	SetVector2ShaderGroupVar(pShaderGroup,	"envEffFatThickness0",	Vector2(ms_fEVEnvEffThickness, ms_fEVFatThickness));

	SetFloatShaderGroupVar(pShaderGroup,	"normalMapBlendRatio",	ms_fEVNormalBlend);

	SetVector4ShaderGroupVar(pShaderGroup,	"umGlobalParams0",		ms_fEVUmGlobalParams);

	
	SetVector2ShaderGroupVar(pShaderGroup,	"anisotropicSpecularIntensity",	ms_fEVAnisotropicSpecularIntensity);
	SetVector2ShaderGroupVar(pShaderGroup,	"anisotropicSpecularExponent",	ms_fEVAnisotropicSpecularExponent);
	SetVector4ShaderGroupVar(pShaderGroup,	"anisotropicSpecularColour",	VEC4V_TO_VECTOR4(ms_fEVAnisotropicSpecularColour.GetRGBA()));
	SetVector4ShaderGroupVar(pShaderGroup,	"specularNoiseMapUVScaleFactor",ms_fEVSpecularNoiseMapUVScaleFactor);
	SetFloatShaderGroupVar(pShaderGroup,	"anisotropicAlphaBias",			ms_fEVAnisotropicAlphaBias );

	SetVector2ShaderGroupVar( pShaderGroup,	"StubbleControl",	Vector2(ms_fEVStubbleIndex, ms_fEVStubbleTile));
}// end of SetEditableShaderValues()...

static
void _SaveDataCB()
{
	CCustomShaderEffectPed::SaveCurrentShaderSettingsToFile();
}

//
//
//
//
bool CCustomShaderEffectPed::InitWidgets(bkBank& bank)
{
	bank.AddToggle("Use ped shaders var caching",  &g_UsePedShaderVarCaching);
	bank.AddToggle("Use cutscene techniques", &g_useOptimisedCutsceneTechnique);
	// debug widgets:
	bank.PushGroup("Ped Editable Shaders Values", false);
		bank.AddToggle("Enable",				&ms_bEVEnabled);

		bank.AddSlider("Specular falloff",		&ms_fEVSpecFalloff,		0.0f, 500.0f, 0.1f);
		bank.AddSlider("Specular intensity",	&ms_fEVSpecIntensity,	0.0f, 1.0f, 0.01f);
		bank.AddSlider("Specular Fresnel",		&ms_fEVSpecFresnel,		0.0f, 1.0f, 0.01f);
		bank.AddSlider("Reflectivity",			&ms_fEVReflectivity,	-10.0f, 10.0f, 0.1f);
		bank.AddSlider("Bumpiness",				&ms_fEVBumpiness,		0.0f, 10.0f, 0.01f);
		bank.AddSlider("BurnoutFX Speed",		&ms_fEVBurnoutLevelSpeed,0.0f,1.0f,	0.005f); 

		bank.AddSlider("Detail Intensity",		&ms_fEVDetailIntensity,		0.0f, 100.0f, 0.001f);
		bank.AddSlider("Detail Bump Intensity",	&ms_fEVDetailBumpIntensity,	0.0f, 100.0f, 0.001f);
		bank.AddSlider("Detail Scale",			&ms_fEVDetailScale,			0.0f, 100.0f, 0.001f);

		bank.AddSlider("EnvEff: Scale",			&ms_fEVEnvEffScale,		0.0f, 1.0f, 0.1f);
		bank.AddSlider("EnvEff: CPV Add",		&ms_fEVEnvEffCpvAdd,	0.0f, 1.0f, 0.1f);
		bank.AddSlider("EnvEff: Thickness",		&ms_fEVEnvEffThickness,	0.0f, 100.0f, 0.1f);
		bank.AddColor("EnvEff: Color Modulator",&ms_fEVEnvEffColorMod);

		bank.PushGroup("EnvEff: Zone values (read only)", false);
			bank.AddSlider("Scale",				&ms_fEVEnvEffZoneScale,		0.0f, 1.0f, 0.1f);
			bank.AddSlider("Color Modulator R",	&ms_fEVEnvEffZoneColorModR, 0, 255, 1);
			bank.AddSlider("Color Modulator G",	&ms_fEVEnvEffZoneColorModG, 0, 255, 1);
			bank.AddSlider("Color Modulator B",	&ms_fEVEnvEffZoneColorModB, 0, 255, 1);
		bank.PopGroup();

		bank.AddSlider("Fat: Scale",			&ms_fEVFatScale,		0.0f, 1.0f, 0.1f);
		bank.AddSlider("Fat: Thickness",		&ms_fEVFatThickness,	0.0f, 100.0f, 0.1f);

		bank.AddSlider("Normal Map: Blend",		&ms_fEVNormalBlend,		0.0f, 1.0f, 0.1f);

		bank.AddSlider("Stubble Strength",		&ms_fEVStubbleStrength,	0.0f,1.0f,0.01f);
		bank.AddSlider("Stubble Index",			&ms_fEVStubbleIndex,	0.0f,16.0f,1.f);
		bank.AddSlider("Stubble Tile",			&ms_fEVStubbleTile,		0.0f,100.0f,0.001f);
		
		bank.PushGroup("uMovements", true);
			bank.AddSlider("Global ScaleH",				(float*)&ms_fEVUmGlobalParams.x,	0.0f, 0.1f,  0.001f);
			bank.AddSlider("Global ScaleV",				(float*)&ms_fEVUmGlobalParams.y,	0.0f, 0.1f,  0.001f);
			bank.AddSlider("Global ArgFreqH",			(float*)&ms_fEVUmGlobalParams.z,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global ArgFreqV",			(float*)&ms_fEVUmGlobalParams.w,	0.0f, 10.0f, 0.001f);
			bank.AddSeparator();
			bank.AddToggle("Use Global Override Values", &ms_bEVUseUmGlobalOverrideValues);
			bank.AddSlider("Global Override ScaleH",	(float*)&ms_fEVUmGlobalOverrideParams.x,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global Override ScaleV",	(float*)&ms_fEVUmGlobalOverrideParams.y,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global Override ArgFreqH",	(float*)&ms_fEVUmGlobalOverrideParams.z,	0.0f, 10.0f, 0.001f);
			bank.AddSlider("Global Override ArgFreqV",	(float*)&ms_fEVUmGlobalOverrideParams.w,	0.0f, 10.0f, 0.001f);
		bank.PopGroup();

		bank.PushGroup("Wind Movement", true);
			bank.AddTitle("Motion determines how wind should affect the ped mesh");
			bank.AddTitle("Changing these slider will change the way the vertices move on the ped (more of a wave pattern)");
			bank.AddToggle("Motion Enabled",		&ms_bEVWindMotionEnabled, NullCB, "Determines if wind should affect the Ped's Mesh");
			bank.AddSlider("Motion Amp X",			&ms_fEVWindMotionAmpX,			0.0f, 256.0f, 1.0f/256.0f, NullCB, "Scaler to control Horizontal size of movement (amplitude)");
			bank.AddSlider("Motion Amp Y",			&ms_fEVWindMotionAmpY,			0.0f, 256.0f, 1.0f/256.0f, NullCB, "Scaler to control Vertical size of movement (amplitude)");
			bank.AddSlider("Motion Freq",			&ms_fEVWindMotionFreq,			0.0f, 256.0f, 1.0f/256.0f, NullCB, "Scaler to control Frequency");
			bank.AddSeparator();
			bank.AddTitle("Response determines the amount of wind to affect the ped mesh");
			bank.AddTitle("Vel min/max determines the curve for the wind factor (between 0-1)");
			bank.AddToggle("Wind Response Enabled",				&ms_bEVWindResponseEnabled, NullCB, "Determines if wind response should be enabled");
			bank.AddSlider("Min Wind Speed for cloth movement",	&ms_fEVWindResponseVelMin,	0.0f, 100.0f, 1.0f/256.0f);
			bank.AddSlider("Max Wind Speed for cloth movement",	&ms_fEVWindResponseVelMax,	0.0f, 100.0f, 1.0f/256.0f);
			bank.AddSlider("Wind Flutter Intensity",			&ms_fEVWindFlutterIntensity,0.0f, 100.0f, 1.0f/256.0f, NullCB, "Determines amplitude(size) of movement");
		bank.PopGroup();

		bank.PushGroup("Aniso Hair");
			bank.AddSlider("Aniso Intensities",	&ms_fEVAnisotropicSpecularIntensity.x,	0.0f,0.2f,0.01f);
			bank.AddSlider("Aniso Intensities",	&ms_fEVAnisotropicSpecularIntensity.y,	0.0f,0.2f,0.01f);
	
			bank.AddSlider("Aniso Exponents",	&ms_fEVAnisotropicSpecularExponent.x,	0.001f,100.0f,0.01f);
			bank.AddSlider("Aniso Exponents",	&ms_fEVAnisotropicSpecularExponent.y,	0.001f,100.0f,0.01f);
	
			bank.AddColor("2nd Aniso Colour",	&ms_fEVAnisotropicSpecularColour);

			bank.AddSlider("Aniso UV scales",	&ms_fEVSpecularNoiseMapUVScaleFactor.x,	0.01f,10.0f,0.01f);
			bank.AddSlider("Aniso UV scales",	&ms_fEVSpecularNoiseMapUVScaleFactor.y,	0.01f,10.0f,0.01f);
			bank.AddSlider("Aniso UV scales",	&ms_fEVSpecularNoiseMapUVScaleFactor.z,	0.01f,10.0f,0.01f);
			bank.AddSlider("Aniso UV scales",	&ms_fEVSpecularNoiseMapUVScaleFactor.w,	0.01f,10.0f,0.01f);

			bank.AddSlider("Aniso Alpha Bias",	&ms_fEVAnisotropicAlphaBias,	0.0f,1.0f,0.01f);			
		bank.PopGroup();
		
		bank.AddSlider("Sweat: Scale",					&ms_fEVSweatScale,			0.0f, 1.0f, 0.1f);
		bank.AddSlider("Wet Cloth Diffuse Adjust",		&ms_fEVWetClothesAdjust.x,	0.0f,1.0f,0.01f);
		bank.AddSlider("Wet Specular Exponent Adjust",	&ms_fEVWetClothesAdjust.y,	0.0f,100.0f,0.01f);
		bank.AddSlider("Wet Specular Intensity Adjust",	&ms_fEVWetClothesAdjust.z,	0.0f,100.0f,0.01f);
		bank.AddSlider("Wet Fade range",				&ms_fEVWetClothesAdjust.w,	0.0f,0.3f,0.01f);

		bank.AddSlider("More Wet Cloth Diffuse Adjust",		&ms_fEVWetClothesAdjustMoreWet.x,	0.0f,1.0f,0.01f);
		bank.AddSlider("More Wet Specular Exponent Adjust",	&ms_fEVWetClothesAdjustMoreWet.y,	0.0f,100.0f,0.01f);
		bank.AddSlider("More Wet Specular Intensity Adjust",&ms_fEVWetClothesAdjustMoreWet.z,	0.0f,100.0f,0.01f);
		bank.AddSlider("More Wet Fade range",				&ms_fEVWetClothesAdjustMoreWet.w,	0.0f,0.3f,0.01f);

		bank.AddSlider("Less Wet Cloth Diffuse Adjust",		&ms_fEVWetClothesAdjustLessWet.x,	0.0f,1.0f,0.01f);
		bank.AddSlider("Less Wet Specular Exponent Adjust",	&ms_fEVWetClothesAdjustLessWet.y,	0.0f,100.0f,0.01f);
		bank.AddSlider("Less Wet Specular Intensity Adjust",&ms_fEVWetClothesAdjustLessWet.z,	0.0f,100.0f,0.01f);
		bank.AddSlider("Less Wet Fade range",				&ms_fEVWetClothesAdjustLessWet.w,	0.0f,0.3f,0.01f);

		// have the hair system init some widgets too
		CShaderHairSort::InitWidgets(bank);

		bank.AddText("Current drawable name:", GetPedVariationMgrDrawableName(), GetPedVariationMgrDrawableNameLen(), true);
		bank.AddButton("Save settings for current drawable to '" SHADER_SETTINGS_FNAME "'.", _SaveDataCB,	"Saves current shader settings to a file.");
	bank.PopGroup();

	bank.PushGroup("Ped Blendshapes", false);
		bank.AddToggle("Force all weights",		&ms_bBlendshapeForceAll,			NullCB, "");
		bank.AddToggle("Force specific weight", &ms_bBlendshapeForceSpecific,		NullCB, "");
		bank.AddSlider("Weight index",			&ms_nBlendshapeForceSpecificIdx,	0, 100, 1, NullCB, "");
		bank.AddSlider("Weight value",			&ms_fBlendshapeForceSpecificBlend,	0.0f, 100.0f, 0.1f, NullCB, "");
	bank.PopGroup();

	bank.PushGroup("Ped Detail");
		bank.AddSlider("Fade Range Start",			&ms_DetailFadeOut[0].x, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Fade Range End",			&ms_DetailFadeOut[0].y, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Burnout: Fade Range Start", &ms_DetailFadeOut[1].x, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Burnout: Fade Range End",   &ms_DetailFadeOut[1].y, 0.0f, 64.0f, 1.0f/256.0f);
	bank.PopGroup();
	return(TRUE);
}// end of InitWidgets()...



//
//
//
//
bool CCustomShaderEffectPed::SaveCurrentShaderSettingsToFile()
{
char buf[2048];

	FileHandle fd = CFileMgr::OpenFileForAppending(SHADER_SETTINGS_FNAME);
	if(fd == INVALID_FILE_HANDLE)
	{	// new file opened:
		fd = CFileMgr::OpenFileForWriting(SHADER_SETTINGS_FNAME);
		if(fd == INVALID_FILE_HANDLE)
			return(FALSE);
		
		::sprintf(buf, "#(\n");	// opening global bracket
		CFileMgr::Write(fd, buf, istrlen(buf));
	}
	else
	{	// appending to existing file:
		s32 currPos = CFileMgr::Tell(fd);
		CFileMgr::Seek(fd, currPos-3);	// eat up closing global bracket '\n)\n'

		::sprintf(buf, ",\n");	// attach to global array
		CFileMgr::Write(fd, buf, istrlen(buf));
	}


		::sprintf(buf, "#(\n");	// opening local bracket
		CFileMgr::Write(fd, buf, istrlen(buf));

		::sprintf(buf,	
				"#(\"ped_name\", \"%s\"),\n"
				"#(\"drawable_name\", \"%s\"),\n"
				""
				"#(\"specular falloff\", %.5f),\n"
				"#(\"specular intensity\", %.5f),\n"
				"#(\"specular fresnel\", %.5f),\n"
				"#(\"bumpiness\", %.5f),\n"
				""
				"#(\"detail inten bump scale\", [%.5f, %.5f, %.5f]),\n"
				"#(\"EnvEfFatMaxThick\", [%.5f, %.5f]),\n"
				""
				"#(\"Stubble Growth\", %.5f),\n"
				"#(\"Index,Tiling\", [%.5f, %.5f]),\n"
				""
				"#(\"wind:sclhv/frqhv\", [%.5f, %.5f, %.5f, %.5f]),\n"
				""
				"#(\"aniso intensities\", [%.5f, %.5f]),\n"
				"#(\"aniso exponents\", [%.5f, %.5f]),\n"
				"#(\"2nd aniso colour\", [%.5f, %.5f, %.5f, %.5f]),\n"
				"#(\"aniso uv scales\", [%.5f, %.5f, %.5f, %.5f]),\n"
				"#(\"aniso alpha bias\", %.5f)\n"
				"#(\"wet clothes adjust\", [%.5f, %.5f, %.5f, %.5f]),\n"
				"#(\"more wet clothes adjust\", [%.5f, %.5f, %.5f, %.5f]),\n"
				"#(\"less wet clothes adjust\", [%.5f, %.5f, %.5f, %.5f]),\n"
				""
				,
		CPedVariationDebug::pedNames[CPedVariationDebug::currPedNameSelection+1],
		GetPedVariationMgrDrawableName(),

		ms_fEVSpecFalloff,
		ms_fEVSpecIntensity,
		ms_fEVSpecFresnel,
		ms_fEVBumpiness,

		ms_fEVDetailIntensity,
		ms_fEVDetailBumpIntensity,
		ms_fEVDetailScale,
		ms_fEVEnvEffThickness, ms_fEVFatThickness,

		ms_fEVStubbleStrength,
		ms_fEVStubbleIndex, ms_fEVStubbleTile,

		ms_fEVUmGlobalParams.x, ms_fEVUmGlobalParams.y, ms_fEVUmGlobalParams.z, ms_fEVUmGlobalParams.w,

		ms_fEVAnisotropicSpecularIntensity.x, ms_fEVAnisotropicSpecularIntensity.y,
		ms_fEVAnisotropicSpecularExponent.x, ms_fEVAnisotropicSpecularExponent.y,
		ms_fEVAnisotropicSpecularColour.GetRedf(), ms_fEVAnisotropicSpecularColour.GetGreenf(), ms_fEVAnisotropicSpecularColour.GetBluef(),	ms_fEVAnisotropicSpecularColour.GetAlphaf(),
		ms_fEVSpecularNoiseMapUVScaleFactor.x,	ms_fEVSpecularNoiseMapUVScaleFactor.y,	ms_fEVSpecularNoiseMapUVScaleFactor.z, ms_fEVSpecularNoiseMapUVScaleFactor.w,
		ms_fEVAnisotropicAlphaBias,
		ms_fEVWetClothesAdjust.x,ms_fEVWetClothesAdjust.y,ms_fEVWetClothesAdjust.z,ms_fEVWetClothesAdjust.w,
		ms_fEVWetClothesAdjustMoreWet.x,ms_fEVWetClothesAdjustMoreWet.y,ms_fEVWetClothesAdjustMoreWet.z,ms_fEVWetClothesAdjustMoreWet.w,
		ms_fEVWetClothesAdjustLessWet.x,ms_fEVWetClothesAdjustLessWet.y,ms_fEVWetClothesAdjustLessWet.z,ms_fEVWetClothesAdjustLessWet.w
		);
		CFileMgr::Write(fd, buf, istrlen(buf));

		::sprintf(buf, ")\n");	// closing local bracket
		CFileMgr::Write(fd, buf, istrlen(buf));


		::sprintf(buf, ")\n");	// closing global bracket
		CFileMgr::Write(fd, buf, istrlen(buf));


	CFileMgr::CloseFile(fd);
	fd = INVALID_FILE_HANDLE;

	return(TRUE);
}
#endif //CSE_PED_EDITABLEVALUES...







