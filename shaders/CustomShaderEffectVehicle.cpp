//
// Filename:	CustomShaderEffectVehicle.h
// Description:	Takes care of all gta_vehicle shader variables (supports vehicle body colours);
// Written by:	Andrzej
//
//
//	07/10/2005	-	Andrzej:	- initial;
//	02/11/2005	-	Andrzej:	- body colours support added;
//	29/01/2007	-	Andrzej:	- Specular2Color support added;
//	12/03/2007	-	Andrzej:	- editable shader values for Jo added:
//	22/06/2007	-	Andrzej:	- storing original Specular added;
//	02/09/2007	-	Andrzej:	- burnout texture support added (burnout texture replaces dirt texture);
//	10/09/2007	-	Andrzej:	- tyre deform control added;
//	20/09/2007	-	Andrzej:	- bodycolor5 added;
//	12/10/2007	-	Andrzej:	- Editable Tire Inner Radius stuff added (__BANK only);
//
//
//
#include "system/nelem.h"
#include "grcore/effect_config.h"

// Framework headers
//#include "fwmaths/maths.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwsys/timer.h"
#include "fwdrawlist/drawlistmgr.h"
#include "streaming/streamingengine.h"
#include "streaming/streaming.h"

// Game headers:
#include "CustomShaderEffectVehicle.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayExtensions.h"
#include "debug/debug.h"
#include "debug/DebugGlobals.h"		// gbDontDisplayDirtOnVehicles...
#include "game/weather.h"			// g_weather
#include "scene/Entity.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "physics/physics.h" // for CPhysics::GetGlassTxdSlot
#include "renderer/DrawLists/drawlist.h"
#include "renderer/Lights/lights.h"
#include "shaders/shaderlib.h"
#include "vfx/misc/Coronas.h"
#include "vehicles/Vehicle.h"
#include "vehicles/wheel.h"
#if !__FINAL
	PARAM(novehicledirt, "Don't display vehicle dirt");
#endif


// #include "system/findsize.h"
// FindSize(CCustomShaderEffectVehicle); was 240 before license plate cleanup, now 156.
// FindSize(CCustomShaderEffectVehicleType); - 20 bytes.

//
//
// body colour remapping:
//
#define BODYCOLOR1		Vector3(2.0f, 1.0f, 1.0f)
#define BODYCOLOR2		Vector3(2.0f, 2.0f, 2.0f)
#define BODYCOLOR3		Vector3(2.0f, 3.0f, 3.0f)
#define BODYCOLOR4		Vector3(2.0f, 4.0f, 4.0f)
#define BODYCOLOR5		Vector3(2.0f, 5.0f, 5.0f)		// special color for interiors
#define BODYCOLOR6		Vector3(2.0f, 6.0f, 6.0f)		// special color for interiors
#define BODYCOLOR7		Vector3(2.0f, 7.0f, 7.0f)		// special color for interiors

#define DEFAULT_DIRTMODWET										(0.60f)		// default dirt mod intensity when et

// --- Globals ------------------------------------------------------------------
#if CSE_VEHICLE_EDITABLEVALUES
	bool	CCustomShaderEffectVehicle::ms_bEVEnabled				= FALSE;
	float	CCustomShaderEffectVehicle::ms_fEVSpecFalloff			= 100.0f;
	float	CCustomShaderEffectVehicle::ms_fEVSpecFresnel			= 0.75f;
	float	CCustomShaderEffectVehicle::ms_fEVSpecIntensity			= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVSpecTexTileUV			= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVSpec2Falloff			= 5.0f;
	float	CCustomShaderEffectVehicle::ms_fEVSpec2Intensity		= 0.0f;
	bool	CCustomShaderEffectVehicle::ms_bEVSpec2DirLerpOverride	= FALSE;
	float	CCustomShaderEffectVehicle::ms_fEVSpec2DirLerp			= 0.0f;
	float	CCustomShaderEffectVehicle::ms_fEVReflectivity			= 0.45f;
	float	CCustomShaderEffectVehicle::ms_fEVBumpiness				= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVDiffuseTexTileUV		= 8.0f;
	float	CCustomShaderEffectVehicle::ms_fEVEnvEffThickness		= 25.0f;
	float	CCustomShaderEffectVehicle::ms_fEVEnvEffScale			= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVEnvEffTexTileUV		= 8.0f;

	bool	CCustomShaderEffectVehicle::ms_fEVOverrideBodyColor		= FALSE;
	Vector3 CCustomShaderEffectVehicle::ms_fEVBodyColor				= Vector3(1.0f, 0.5f, 1.0f);
	bool	CCustomShaderEffectVehicle::ms_fEVOverrideDiffuseColorTint= FALSE;
	Vector3 CCustomShaderEffectVehicle::ms_fEVDiffuseColorTint		= Vector3(1.0f, 0.0f, 0.0f);
	float	CCustomShaderEffectVehicle::ms_fEVDiffuseColorTintAlpha = 0.5f;
	float	CCustomShaderEffectVehicle::ms_fEVDirtLevel				= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVDirtModDry			= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVDirtModWet			= DEFAULT_DIRTMODWET;
	s32		CCustomShaderEffectVehicle::ms_fEVDirtOrBurnout			= 1;
	Vector3	CCustomShaderEffectVehicle::ms_fEVDirtColor				= Vector3(59.0f/255.0f, 57.0f/255.0f, 52.0f/255.0f);	// dirtColor RGB=(59,57,52)

	// editable inner wheel radius stuff:
	bool	CCustomShaderEffectVehicle::ms_bEWREnabled			= FALSE;
	bool	CCustomShaderEffectVehicle::ms_bEWRTyreEnabled		= TRUE;
	float	CCustomShaderEffectVehicle::ms_fEWRInnerRadius		= 1.500f;

	// editables for sunglare:
	bool	CCustomShaderEffectVehicle::ms_bEVSunGlareEnabled			= FALSE;
	float	CCustomShaderEffectVehicle::ms_fEVSunGlareDistMin			= 65.0f;
	float	CCustomShaderEffectVehicle::ms_fEVSunGlareDistMax			= 75.0f;
	u32		CCustomShaderEffectVehicle::ms_nEVSunGlarePow				= 1;
	float	CCustomShaderEffectVehicle::ms_fEVSunGlareSpriteSize		= 0.55f;
	float	CCustomShaderEffectVehicle::ms_fEVSunGlareSpriteZShiftScale	= 0.2f;
	float	CCustomShaderEffectVehicle::ms_fEVSunGlareHdrMult			= 8.0f;
	
	Vector4	CCustomShaderEffectVehicle::ms_fEVLetterIndex1					= Vector4(10, 21, 10, 23);
	Vector4	CCustomShaderEffectVehicle::ms_fEVLetterIndex2					= Vector4(63, 63, 62, 0);
	Vector2	CCustomShaderEffectVehicle::ms_fEVLetterSize					= Vector2(1.0f / 16.0f, 1.0f / 4.0f);
	Vector2	CCustomShaderEffectVehicle::ms_fEVNumLetters					= Vector2(16.0f,4.0f);
	Vector4	CCustomShaderEffectVehicle::ms_fEVLicensePlateFontExtents		= Vector4(0.043f, 0.38f, 0.945f, 0.841f);
	Vector4	CCustomShaderEffectVehicle::ms_fEVLicensePlateFontTint			= Vector4(1.0f, 1.0f, 1.0f, 0.0f);
	float	CCustomShaderEffectVehicle::ms_fEVFontNormalScale				= 1.0f;
	float	CCustomShaderEffectVehicle::ms_fEVDistMapCenterVal				= 0.5f;
	Vector4	CCustomShaderEffectVehicle::ms_fEVDistEpsilonScaleMin			= Vector4(1.0f, 1.0f, 0.0f, 0.0f);
	Vector3	CCustomShaderEffectVehicle::ms_fEVFontOutlineMinMaxDepthEnabled	= Vector3(0.475f, 0.5, 0.0f);
	Vector3	CCustomShaderEffectVehicle::ms_fEVFontOutlineColor				= Vector3(0.0f, 0.0f, 0.0f);

	float	CCustomShaderEffectVehicle::ms_fEVBurnoutSpeed					= 0.01f;	// minimum increase step is 1/255
#endif //CSE_VEHICLE_EDITABLEVALUES...
#if __DEV
	s32		CCustomShaderEffectVehicle::ms_nForceColorCars		= 2;
#endif

#if USE_DISKBRAKEGLOW
	#define DISKBRAKEGLOW_V2 0 
	#define DISKBRAKEGLOW_DEBUG_TEXT 0
	#define DISKBRAKEGLOW_DEBUG_TEXT_FULLWHEELDETAIL 0
	#define DISKBRAKEGLOW_DEBUG_SHADER 0

#if DISKBRAKEGLOW_V2	
	static dev_float g_fDiskBrake_MaxHeatingRate = 3.0f;
	static dev_float g_fDiskBrake_CoolingRate = 0.15f;
	static dev_float g_fDiskBrake_SpeedCap = 80.0f/3.6f; // From kmh-1 to ms-1
#else // DISKBRAKEGLOW_V2
	static dev_float g_fDiskBrake_CoolingRate = 0.15f;
	static dev_float g_fDiskBrake_TempMagicNumber = 1.5f;
	static dev_float g_fDiskBrake_SpeedCap = 70.0f/3.6f; // From kmh-1 to ms-1
#endif // DISKBRAKEGLOW_V2
	static dev_float g_fDiskBrake_EngineAge = 5.0f * 60.0f * 1000.0f;
#endif // USE_DISKBRAKEGLOW

//
// global burnout texture for vehicles:
//
//
#define VEHICLE_GENERIC_BURNOUT_TEXNAME			"vehicle_generic_burnt_out"
#define VEHICLE_GENERIC_BURNOUT_INT_TEXNAME		"vehicle_generic_burnt_int"
#define VEHICLE_GENERIC_BURNOUT_TRUCK_TEXNAME	"vehicle_generic_burnt_out_truck"

// Default initialization to zero, which means NULL
grcTextureIndex CCustomShaderEffectVehicleType::ms_pBurnoutTexture;
grcTextureIndex CCustomShaderEffectVehicleType::ms_pBurnoutIntTexture;
grcTextureIndex CCustomShaderEffectVehicleType::ms_pBurnoutTruckTexture;
s32				CCustomShaderEffectVehicleType::ms_nBurnoutTruckTextureRef	= 0;

#if VEHICLE_SUPPORT_PAINT_RAMP
#define VEHICLE_PAINT_RAMP_DISABLED_VALUE (-1.0f)
static strLocalIndex s_RampTxdId;
#if __BANK
static bool s_RampDiffuseEnabled = true;
static bool s_RampSpecularEnabled = true;
#endif

void CCustomShaderEffectVehicle::LoadRampTxd()
{
	s_RampTxdId = g_TxdStore.FindSlot(strStreamingObjectName("vehicle_paint_ramps"));
	if(s_RampTxdId.IsValid())
	{
		CStreaming::LoadObject(s_RampTxdId, g_TxdStore.GetStreamingModuleId());
		g_TxdStore.AddRef(s_RampTxdId, REF_OTHER);
	}
}

void CCustomShaderEffectVehicle::UnloadRampTxd()
{
	if (s_RampTxdId.IsValid())
	{
		g_TxdStore.RemoveRef(s_RampTxdId, REF_OTHER);
		s_RampTxdId = strLocalIndex();
	}
}

fwTxd * CCustomShaderEffectVehicle::GetRampTxd()
{
	fwTxd *result = s_RampTxdId.IsValid() ? g_TxdStore.Get(s_RampTxdId) : 0;
	return result;
}

#if __BANK
static bool s_DebugRampTextureEnabled = false;
static char s_DebugRampTexturePath[256] = { "C:/Temp/vehicle_paint_ramp" };
static grcTexture* s_DebugRampTexture = 0;
static void ReloadDebugRampTexture()
{
	if(s_DebugRampTextureEnabled)
	{
		sysMemStartTemp();
		grcImage* img = grcImage::LoadDDS(s_DebugRampTexturePath);
		if(!img)
		{
			img = grcImage::LoadJPEG(s_DebugRampTexturePath);
		}
		sysMemEndTemp();

		if(img)
		{
			grcTextureFactory::TextureCreateParams params(grcTextureFactory::TextureCreateParams::VIDEO, grcTextureFactory::TextureCreateParams::TILED);
			s_DebugRampTexture = grcTextureFactory::GetInstance().Create(img, &params);

			sysMemStartTemp();
			img->Release();
			img = 0;
			sysMemEndTemp();
		}
	}
}
#endif
#endif

//
//
//
//
CCustomShaderEffectVehicleType* CCustomShaderEffectVehicleType::Create(rmcDrawable *pDrawable, CVehicleModelInfo *pVehicleMI, grmShaderGroup *pClonedShaderGroup)
{
	Assert(pDrawable);

	// Initialize structure to all zero.
	CCustomShaderEffectVehicleType *pType = rage_new CCustomShaderEffectVehicleType();

	// BS#2551385: vehicle mods: use cloned shaderGroup (if exists) rather than drawable's original shaderGroup:
	grmShaderGroup* shaderGroup	= pClonedShaderGroup? pClonedShaderGroup : &pDrawable->GetShaderGroup();
	const s32 shaderCount		= shaderGroup->GetCount();
	atArray<CCustomShaderEffectVehicleType::structDiffColEffect>	tmpTabDiffCol(0, shaderCount);		// temp table to gather all variable info's
	atArray<CCustomShaderEffectVehicleType::structSpecColEffect>	tmpTabSpecularCol(0, shaderCount);
	atArray<CCustomShaderEffectVehicleType::structDirtTexEffect>	tmpTabDirtTex(0, shaderCount);
	atArray<CCustomShaderEffectVehicleType::structTex2Effect>		tmpTabTex2(0, shaderCount);

	pType->m_bHasBodyColor1 =
	pType->m_bHasBodyColor2 =
	pType->m_bHasBodyColor3 =
	pType->m_bHasBodyColor4 =
	pType->m_bHasBodyColor5 =
	pType->m_bHasBodyColor6 =
	pType->m_bHasBodyColor7 = false;

	
	bool bFoundEmissiveMult		= false;
	pType->m_DefEmissiveMult	= 1.0f;

	if(pVehicleMI)
	{	
		const u32 vehNameHash = pVehicleMI->GetModelNameHash();
		if(	(vehNameHash == 0xE7D2A16E)	||	// "shotaro"
			(vehNameHash == 0x3AF76F4A)	||	// "voltic2"
			(vehNameHash == 0xE6E967F8)	||	// "patriot2"
			(vehNameHash == 0x93F09558)	)	// "deathbike2"
		{
			// enable only for shotaro & voltic2:
			Assert(pType->m_idVarEmissiveMult != grmsgvNONE);
		}
		else
		{
			// disable for everything else
			pType->m_idVarEmissiveMult = grmsgvNONE;
		}
	}
	else
	{
		// disable for everything else
		pType->m_idVarEmissiveMult = grmsgvNONE;
	}


	for(s32 i=0; i<shaderCount; i++)
	{
		grmShader *pShader = shaderGroup->GetShaderPtr(i);
		Assert(pShader);
		// grcEffect *pEffect = &pShader->GetEffect();

		u8 colorIdx = 255;	 // invalid value

		// 1. DiffuseColor:
		grcEffectVar idvarDifCol = pShader->LookupVar("DiffuseColor", FALSE);
		if(idvarDifCol)
		{
			Vector3					diffCol;
			pShader->GetVar(idvarDifCol, diffCol);

			Assert((int)idvarDifCol < 256);	// must fit into u8
			Assert(i < 256);			// must fit into u8

			CCustomShaderEffectVehicleType::structDiffColEffect dcEffect;
			dcEffect.idvarDiffColor = (u8)idvarDifCol;
#if VEHICLE_SUPPORT_PAINT_RAMP
			dcEffect.idvarDiffuseRampTexture = (u8)pShader->LookupVar("DiffuseRampTexture", FALSE);
			dcEffect.idvarSpecularRampTexture = (u8)pShader->LookupVar("SpecularRampTexture", FALSE);
			dcEffect.idvarDiffuseSpecularRampEnabled = (u8)pShader->LookupVar("matDiffuseSpecularRampEnabled", FALSE);
#endif
			dcEffect.shaderIdx = (u8)i;

			// find color mapping
			if(diffCol == BODYCOLOR1)
			{
				dcEffect.colorIdx		= colorIdx = 0;
				pType->m_bHasBodyColor1 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			else if(diffCol == BODYCOLOR2)
			{
				dcEffect.colorIdx		= colorIdx = 1;
				pType->m_bHasBodyColor2 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			else if(diffCol == BODYCOLOR3)
			{
				dcEffect.colorIdx		= colorIdx = 2;
				pType->m_bHasBodyColor3 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			else if(diffCol == BODYCOLOR4)
			{
				dcEffect.colorIdx		= colorIdx = 3;
				pType->m_bHasBodyColor4 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			else if(diffCol == BODYCOLOR5)
			{
				dcEffect.colorIdx		= colorIdx = 4;
				pType->m_bHasBodyColor5 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			else if(diffCol == BODYCOLOR6)
			{
				dcEffect.colorIdx		= colorIdx = 5;
				pType->m_bHasBodyColor6 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			else if(diffCol == BODYCOLOR7)
			{
				dcEffect.colorIdx		= colorIdx = 6;
				pType->m_bHasBodyColor7 = true;
				tmpTabDiffCol.Append() = dcEffect;
			}
			
		}//if(idvarDifCol)...
		
		// 2. Specular values: Intensity, Falloff and Fresnel:
		grcEffectVar idvarSpecInt		= pShader->LookupVar("SpecularColor", FALSE);
		grcEffectVar idvarSpecFalloff	= pShader->LookupVar("Specular", FALSE);
		grcEffectVar idvarSpecFresnel	= pShader->LookupVar("Fresnel", FALSE);
		
		if(idvarSpecInt || idvarSpecFalloff || idvarSpecFresnel)
		{
			CCustomShaderEffectVehicleType::structSpecColEffect	&dcEffect = tmpTabSpecularCol.Append();

			dcEffect.idvarSpecInt		=
			dcEffect.idvarSpecFalloff	= 
			dcEffect.idvarSpecFresnel	= grcevNONE;

			if(idvarSpecInt)
			{
				Assert((int)idvarSpecInt < 256);	// must fit into u8
				dcEffect.idvarSpecInt		= (u8)idvarSpecInt;

				float originalSpecInt=1.0f;
				pShader->GetVar(idvarSpecInt, originalSpecInt);
				dcEffect.origSpecInt		= originalSpecInt;
			}

			if(idvarSpecFalloff)
			{
				Assert((int)idvarSpecFalloff < 256);	// must fit into u8
				dcEffect.idvarSpecFalloff	= (u8)idvarSpecFalloff;

				float originalSpecFalloff=1.0f;
				pShader->GetVar(idvarSpecFalloff, originalSpecFalloff);
				dcEffect.origSpecFalloff	= originalSpecFalloff;
			}

			if(idvarSpecFresnel)
			{
				Assert((int)idvarSpecFresnel < 256);	// must fit into u8
				dcEffect.idvarSpecFresnel	= (u8)idvarSpecFresnel;

				float originalSpecFresnel=1.0f;
				pShader->GetVar(idvarSpecFresnel, originalSpecFresnel);
				dcEffect.origSpecFresnel	= originalSpecFresnel;
			}

			Assert(i < 256);			// must fit into u8
			dcEffect.shaderIdx			= (u8)i;
			Assert((int)colorIdx < 256);		// must fit into u8
			dcEffect.colorIdx			= (u8)colorIdx;
		}

		// 3. Dirt texture:
		bool bDirtIsInterior = false;
		grcEffectVar idvarDirtTex = pShader->LookupVar("DirtTex", FALSE);
		if(!idvarDirtTex)
		{	// replace main diffuse tex when dirt texture not present (interiors, wheels, etc.):
			idvarDirtTex	= pShader->LookupVar("DiffuseTex", FALSE);
			bDirtIsInterior = true;
		}

		// don't setup burnout texture for badges & cutout:
		if(idvarDirtTex)
		{
			const char *pShaderName = pShader->GetName();
			if(pShaderName)
			{
				if( (!strcmp("vehicle_badges", pShaderName))	||
					(!strcmp("vehicle_cutout", pShaderName))	)
				{
					idvarDirtTex = grcevNONE;
				}
			}
		}

		// tentative "patch" for B*2275234, B*2137345 and B*2272964:
		Assertf((int(idvarDirtTex) <= 255), "Vehicle: %s: idvarDirtTex is far too big!", pVehicleMI->GetModelName());
		if(int(idvarDirtTex) > 255)
		{
			idvarDirtTex = grcevNONE;
		}

		if(idvarDirtTex)
		{
			CCustomShaderEffectVehicleType::structDirtTexEffect		&dcEffect = tmpTabDirtTex.Append();
			grcTexture *pOrigDirtTex=NULL;
			pShader->GetVar(idvarDirtTex, pOrigDirtTex);
			#if __ASSERT
				if(pOrigDirtTex)
				{	// make sure it's not texture reference:
					Assert(pOrigDirtTex->GetResourceType()!=grcTexture::REFERENCE);
				}
			#endif //__ASSERT...

			Assert((int)idvarDirtTex < 256);	// must fit into u8
			Assert(i < 256);			// must fit into u8

			strStreamingEngine::AddRefByPtr(pOrigDirtTex, REF_OTHER);

			dcEffect.pOrigDirtTex		= pOrigDirtTex;
			dcEffect.idvarDirtTex		= (u8)idvarDirtTex;
			dcEffect.shaderIdx			= (u8)i;
			dcEffect.bIsInterior		= bDirtIsInterior;
		}

		grcEffectVar idvarDirtCol = pShader->LookupVar("DirtColor", FALSE);
		if(idvarDirtCol)
		{
			Vector3 origDirtCol;
			pShader->GetVar(idvarDirtCol, origDirtCol);
			pType->m_DefDirtColor.Setf(origDirtCol.x, origDirtCol.y, origDirtCol.z, 1.0f);
		}

		// 4. overlay texture:
		grcEffectVar idvarTex2 = pShader->LookupVar("DiffuseTex2", FALSE);
		if(idvarTex2)
		{
			CCustomShaderEffectVehicleType::structTex2Effect		&dcEffect = tmpTabTex2.Append();
			grcTexture *pOrigTex2=NULL;
			pShader->GetVar(idvarTex2, pOrigTex2);
			#if __ASSERT
				if(pOrigTex2)
				{	// make sure it's not texture reference:
					Assert(pOrigTex2->GetResourceType()!=grcTexture::REFERENCE);
				}
			#endif //__ASSERT...

			Assert((int)idvarTex2 < 256);	// must fit into u8
			Assert(i < 256);				// must fit into u8

			strStreamingEngine::AddRefByPtr(pOrigTex2, REF_OTHER);

			dcEffect.pOrigTex2			= pOrigTex2;
			dcEffect.idvarTex2			= (u8)idvarTex2;
			dcEffect.shaderIdx			= (u8)i;
		}


		if(pType->m_idVarEmissiveMult)
		{
			grcEffectVar idvarEmissiveMult = pShader->LookupVar("EmissiveMultiplier", FALSE);
			if(idvarEmissiveMult)
			{
				float fEmissiveMult;
				pShader->GetVar(idvarEmissiveMult, fEmissiveMult);

				if(!bFoundEmissiveMult)
				{
					bFoundEmissiveMult = true;
					pType->m_DefEmissiveMult = fEmissiveMult;
				}
				else
				{
					Assertf(pType->m_DefEmissiveMult == fEmissiveMult, "%s: EmissiveMultiplier (=%.2f) is not the same for all parts of the vehicle: found %.2f.",pVehicleMI->GetModelName(), pType->m_DefEmissiveMult, fEmissiveMult);
				}
			}
		}

	}// for(s32 i=0; i<shaderCount; i++)...

	if(pVehicleMI && (!pVehicleMI->GetAllowBodyColorMapping()))
	{
		// block color mapping for scripts if requested
		pType->m_bHasBodyColor1 =
		pType->m_bHasBodyColor2 =
		pType->m_bHasBodyColor3 =
		pType->m_bHasBodyColor4 =
		pType->m_bHasBodyColor5 =
		pType->m_bHasBodyColor6 =
		pType->m_bHasBodyColor7 = false;
	}

	pType->m_DiffColEffect = tmpTabDiffCol;
	#if __ASSERT
		extern char* CCustomShaderEffectBase_EntityName;
		Assertf(pType->m_DiffColEffect.GetCount() > 0, "BS#124925: %s (drawable=%p, shaderGroup=%p): DiffColEffect array is empty! This vehicle's materials do not have a valid color variation.  Andrzej would love to see this assert.", CCustomShaderEffectBase_EntityName, pDrawable, shaderGroup);
	#endif
	pType->m_SpecColEffect	= tmpTabSpecularCol;
	pType->m_DirtTexEffect	= tmpTabDirtTex;
	pType->m_Tex2Effect		= tmpTabTex2;

	// burnout truck texture setup:
	// 1. vehshare_truck is dynamically streamed in and is usually parent to vehicle's main txd
	// 2. normal vehicle txd dependency:	vehicle_txd <- vehshare
	// 3. truck txd structure dependency:	vehicle_txd <- vehshare_truck <- vehshare
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));	// truck burnout tex referencing allowed only on update thread

	pType->m_bHasTruckTex = false;
	if(false && pVehicleMI) // separate burnout tex for trucks has been blocked - they share burnout tex with everybody else now
	{
		const strLocalIndex txdIdx = strLocalIndex(pVehicleMI->GetAssetParentTxdIndex());
		if(txdIdx != -1)
		{
			const strLocalIndex vehshareTxdIdx = strLocalIndex(pVehicleMI->GetCommonTxd());	// vehshare is always resident at index 0
			Assert(vehshareTxdIdx != -1);
			const strLocalIndex parentTxdIdx = g_TxdStore.GetParentTxdSlot(txdIdx);

			// assume first level parent to be vehshare_truck:
			if( (parentTxdIdx!=-1) && (parentTxdIdx!=vehshareTxdIdx) )
			{
				const strLocalIndex vehshareTruckTxdIdx = parentTxdIdx;
				grcTexture *burnoutTruckTex = g_TxdStore.Get(vehshareTruckTxdIdx)->Lookup(VEHICLE_GENERIC_BURNOUT_TRUCK_TEXNAME);
				Assert(burnoutTruckTex);
			
				if(ms_nBurnoutTruckTextureRef)
				{	// same ptr as previously fetched?
					Assert(ms_pBurnoutTruckTexture == burnoutTruckTex);
				}
				else
				{	
					// update static shared pointer with burnouttrucktex ptr:
					ms_pBurnoutTruckTexture = burnoutTruckTex;
				}
				ms_nBurnoutTruckTextureRef++;
			
				pType->m_bHasTruckTex = true;
			}
		}
	}

	return pType;
}// end of CCustomShaderEffectVehicle::Create()...


//
//
// Recreates Type structures using texture ptr:
// - m_DirtTexEffect;
//
bool CCustomShaderEffectVehicleType::Recreate(rmcDrawable *pDrawable, CVehicleModelInfo* /*pVehicleMI*/)
{
	Assert(pDrawable);

	grmShaderGroup* shaderGroup	= &pDrawable->GetShaderGroup();
	const s32 shaderCount		= shaderGroup->GetCount();
	atArray<CCustomShaderEffectVehicleType::structDirtTexEffect>	tmpTabDirtTex(0, shaderCount);

	for(s32 i=0; i<shaderCount; i++)
	{
		grmShader *pShader = shaderGroup->GetShaderPtr(i);
		Assert(pShader);

		// 3. Dirt texture:
		bool bDirtIsInterior = false;
		grcEffectVar idvarDirtTex = pShader->LookupVar("DirtTex", FALSE);
		if(!idvarDirtTex)
		{	// replace main diffuse tex when dirt texture not present (interiors, wheels, etc.):
			idvarDirtTex	= pShader->LookupVar("DiffuseTex", FALSE);
			bDirtIsInterior = true;
		}

		// don't setup burnout texture for badges:
		if(idvarDirtTex)
		{
			const char *pShaderName = pShader->GetName();
			if(pShaderName && !strcmp("vehicle_badges", pShaderName))
			{
				idvarDirtTex = grcevNONE;
			}
		}

		if(idvarDirtTex)
		{
			CCustomShaderEffectVehicleType::structDirtTexEffect		&dcEffect = tmpTabDirtTex.Append();
			grcTexture *pOrigDirtTex=NULL;
			pShader->GetVar(idvarDirtTex, pOrigDirtTex);
			#if __ASSERT
				if(pOrigDirtTex)
				{	// make sure it's not texture reference:
					Assert(pOrigDirtTex->GetResourceType()!=grcTexture::REFERENCE);
				}
			#endif //__ASSERT...

			Assert((int)idvarDirtTex < 256);	// must fit into u8
			Assert(i < 256);			// must fit into u8

			strStreamingEngine::AddRefByPtr(pOrigDirtTex, REF_OTHER);

			dcEffect.pOrigDirtTex		= pOrigDirtTex;
			dcEffect.idvarDirtTex		= (u8)idvarDirtTex;
			dcEffect.shaderIdx			= (u8)i;
			dcEffect.bIsInterior		= bDirtIsInterior;
		}

	}// for(s32 i=0; i<shaderCount; i++)...


bool bRecreate=true;
	// check if new dirtTexEffect tab contains burnout textures
	// (if yes, then drawable was NOT re-streamed (burnout textures are only set by CSE code)
	// and CSEType doesn't need to be recreated):
	Assert(this->m_DirtTexEffect.GetCount()==tmpTabDirtTex.GetCount());
	const u32 count = tmpTabDirtTex.GetCount();
	for(u32 i=0; i<count; i++)
	{
		CCustomShaderEffectVehicleType::structDirtTexEffect *newPtr = &tmpTabDirtTex[i];
	#if __ASSERT
		CCustomShaderEffectVehicleType::structDirtTexEffect *oldPtr = &this->m_DirtTexEffect[i];
		Assert(oldPtr->idvarDirtTex	== newPtr->idvarDirtTex);
		Assert(oldPtr->shaderIdx	== newPtr->shaderIdx);
		Assert(oldPtr->bIsInterior	== newPtr->bIsInterior);
	#endif

		grcTexture *burnoutTex = m_bHasTruckTex? CCustomShaderEffectVehicleType::ms_pBurnoutTruckTexture : CCustomShaderEffectVehicleType::ms_pBurnoutTexture;
		burnoutTex = newPtr->bIsInterior? CCustomShaderEffectVehicleType::ms_pBurnoutIntTexture : burnoutTex;
		if(newPtr->pOrigDirtTex == burnoutTex)
		{
			bRecreate = false;	// burnout tex detected: drawable was NOT re-streamed
		}
	}

	atArray<CCustomShaderEffectVehicleType::structDirtTexEffect> *const removeRefs = bRecreate? &this->m_DirtTexEffect : &tmpTabDirtTex;
	for(u32 i=0; i<count; i++)
	{
		grcTexture *const pTex = (*removeRefs)[i].pOrigDirtTex;
		(*removeRefs)[i].pOrigDirtTex = NULL;
		strStreamingEngine::RemoveRefByPtr(pTex, REF_OTHER);
	}

	if(bRecreate)
	{
		this->m_DirtTexEffect = tmpTabDirtTex;
	}

	return(TRUE);
}// end of CCustomShaderEffectVehicleType::Recreate()...

//
// restores drawable's shaderVars values to their original (initial) state:
// - to be executed right before MasterCSE for modelInfo is destroyed;
// - note: uses RecordSetVar() API (directly modifies InstanceData memory without drawablespu, etc.);
// - that way if drawable is NOT reloaded from disk by streaming and directly reused for MasterCSE creation
//   it makes it possible to correctly initialise/create given MasterCSE
//   (which may rely on shaderVar values - e.g. CSEVehicle);
//
bool CCustomShaderEffectVehicleType::RestoreModelInfoDrawable(rmcDrawable *pDrawable)
{
	Assert(pDrawable);
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));	// allowed on Update Thread only

	if(!pDrawable)
		return(false);

	grmShaderGroup* pShaderGroup	= &pDrawable->GetShaderGroup();
	Assert(pShaderGroup);
	const u32 shaderCount			= pShaderGroup->GetCount();

	// restore "DiffuseColor":
	const u32 count = m_DiffColEffect.GetCount();
	if(count)
	{
		Vector3 defaultDiffuseCol[7] =
		{
			BODYCOLOR1,	// 0
			BODYCOLOR2,	// 1
			BODYCOLOR3,	// 2
			BODYCOLOR4,	// 3
			BODYCOLOR5,	// 4
			BODYCOLOR6,	// 5
			BODYCOLOR7	// 6
		};

		for(u32 i=0; i<count; i++)
		{
			CCustomShaderEffectVehicleType::structDiffColEffect *ptr = &m_DiffColEffect[i];
			pShaderGroup->GetShader(ptr->shaderIdx).RecordSetVar((grcEffectVar)ptr->idvarDiffColor, defaultDiffuseCol[ptr->colorIdx]);

#if VEHICLE_SUPPORT_PAINT_RAMP
			if(ptr->idvarDiffuseRampTexture)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffuseRampTexture, const_cast<grcTexture*>(grcTexture::NoneBlack));
			}

			if(ptr->idvarSpecularRampTexture)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecularRampTexture, const_cast<grcTexture*>(grcTexture::NoneBlack));
			}

			if(ptr->idvarDiffuseSpecularRampEnabled)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffuseSpecularRampEnabled, VEHICLE_PAINT_RAMP_DISABLED_VALUE);
			}
#endif
		}
	}

	// restore original "SpecularColor":
	const s32 specCount = m_SpecColEffect.GetCount();
	for(u32 i=0; i<specCount; i++)
	{
		CCustomShaderEffectVehicleType::structSpecColEffect *ptr = &m_SpecColEffect[i];
		if(ptr->idvarSpecInt)
			pShaderGroup->GetShader(ptr->shaderIdx).RecordSetVar((grcEffectVar)ptr->idvarSpecInt, float(ptr->origSpecInt));

		if(ptr->idvarSpecFalloff)
			pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFalloff, float(ptr->origSpecFalloff));

		if(ptr->idvarSpecFresnel)
			pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFresnel, float(ptr->origSpecFresnel));
	}

	// restore original DirtTexture:
	const u32 dirtTexCount = m_DirtTexEffect.GetCount();
	for(u32 i=0; i<dirtTexCount; i++)
	{
		CCustomShaderEffectVehicleType::structDirtTexEffect *ptr = &m_DirtTexEffect[i];
	#if __PPU
		// Hack: manually clear FLAG_SET_FROM_SPU to shut up assert in RecordSetVar():
		grcInstanceData& data = pShaderGroup->GetShader(ptr->shaderIdx).GetInstanceData();
		data.Flags &= ~grcInstanceData::FLAG_SET_FROM_SPU;
	#endif
		pShaderGroup->GetShader(ptr->shaderIdx).RecordSetVar((grcEffectVar)ptr->idvarDirtTex, (grcTexture*)ptr->pOrigDirtTex);
	}

	for(u32 i=0; i<shaderCount; i++)
	{
		grmShader *pShader = pShaderGroup->GetShaderPtr(i);
		Assert(pShader);

		// restore DirtColor:
		grcEffectVar idvarDirtCol = pShader->LookupVar("DirtColor", FALSE);
		if(idvarDirtCol)
		{
			Vector3 origDirtCol(VEC3V_TO_VECTOR3(m_DefDirtColor.GetRGB()));
			pShader->RecordSetVar(idvarDirtCol, origDirtCol);
		}

		// restore Emissive Multiplier:
		if(m_idVarEmissiveMult)
		{
			grcEffectVar idvarEmissiveMult = pShader->LookupVar("EmissiveMultiplier", FALSE);
			if(idvarEmissiveMult)
			{
				pShader->RecordSetVar(idvarEmissiveMult, m_DefEmissiveMult);
			}
		}
	}

	// restore overlay texture:
	const u32 tex2Count = m_Tex2Effect.GetCount();
	for(u32 i=0; i<tex2Count; i++)
	{
		CCustomShaderEffectVehicleType::structTex2Effect *ptr = &m_Tex2Effect[i];
	#if __PPU
		// Hack: manually clear FLAG_SET_FROM_SPU to shut up assert in RecordSetVar():
		grcInstanceData& data = pShaderGroup->GetShader(ptr->shaderIdx).GetInstanceData();
		data.Flags &= ~grcInstanceData::FLAG_SET_FROM_SPU;
	#endif
		pShaderGroup->GetShader(ptr->shaderIdx).RecordSetVar((grcEffectVar)ptr->idvarTex2, (grcTexture*)ptr->pOrigTex2);
	}

	return(true);
}// end of CCustomShaderEffectVehicle::RestoreDrawable()...


//
//
//
//
CCustomShaderEffectVehicleType::~CCustomShaderEffectVehicleType()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));	// truck burnout tex referencing allowed only on update thread

	if(this->m_bHasTruckTex)
	{
		Assert(ms_pBurnoutTruckTexture);	// must be valid
		ms_nBurnoutTruckTextureRef--;
		Assert(ms_nBurnoutTruckTextureRef>=0);
		if(ms_nBurnoutTruckTextureRef==0)
		{
			ms_pBurnoutTruckTexture = NULL;
		}
	}

	const u32 dirtTexCount = m_DirtTexEffect.GetCount();
	for(u32 i=0; i<dirtTexCount; i++)
	{
		grcTexture *const pTex = this->m_DirtTexEffect[i].pOrigDirtTex;
		this->m_DirtTexEffect[i].pOrigDirtTex = NULL;
		strStreamingEngine::RemoveRefByPtr(pTex, REF_OTHER);
	}

	const u32 tex2Count = m_Tex2Effect.GetCount();
	for(u32 i=0; i<tex2Count; i++)
	{
		grcTexture *const pTex2 = this->m_Tex2Effect[i].pOrigTex2;
		this->m_Tex2Effect[i].pOrigTex2 = NULL;
		strStreamingEngine::RemoveRefByPtr(pTex2, REF_OTHER);
	}
}


//
//
//
//
inline u8 VehShaderGroupLookupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

inline u8 VehEffectLookupGlobalVar(const char *name, bool mustExist)
{
	grcEffectGlobalVar  varID = grcEffect::LookupGlobalVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}


//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectVehicleType::Initialise(rmcDrawable *pDrawable)
{
	return Initialise(pDrawable, NULL);
}

bool CCustomShaderEffectVehicleType::Initialise(rmcDrawable *pDrawable, grmShaderGroup *pClonedShaderGroup)
{
	Assert(pDrawable);

	// BS#2551385: vehicle mods: use cloned shaderGroup (if exists) rather than drawable's original shaderGroup:
	grmShaderGroup* pShaderGroup= pClonedShaderGroup? pClonedShaderGroup : &pDrawable->GetShaderGroup();
	Assert(pShaderGroup);

	m_idVarDirtLevelMod			= VehShaderGroupLookupVar(pShaderGroup, "DirtLevelMod",	FALSE);
	m_idVarDirtColor			= VehShaderGroupLookupVar(pShaderGroup, "DirtColor",	FALSE);

	m_idVarSpec2Color			= VehShaderGroupLookupVar(pShaderGroup, "Specular2Color",	FALSE);
	m_idVarDiffuse2Color		= VehShaderGroupLookupVar(pShaderGroup, "DiffuseColor2",	FALSE);
	m_idVarDiffuseColorTint		= VehShaderGroupLookupVar(pShaderGroup, "DiffuseColorTint",	FALSE);
#if USE_DISKBRAKEGLOW
	m_idVarDiskBrakeGlow		= VehShaderGroupLookupVar(pShaderGroup, "DiskBrakeGlow",FALSE);
#endif

	// headlights
	m_idVarDimmer				= VehShaderGroupLookupVar(pShaderGroup, "dimmerSetPacked", FALSE);


	// tyre deformation on/off control:
	m_idVarTyreDeformSwitchOn	= VehEffectLookupGlobalVar("tyreDeformSwitchOn", TRUE);

	// vehicle deformation
#if USE_GPU_VEHICLEDEFORMATION
	m_idVarDamageSwitchOn	= VehEffectLookupGlobalVar("switchOn",	TRUE);
	m_idVarDamageTex		= VehShaderGroupLookupVar(pShaderGroup, "damagetexture",	false);
	m_idVarBoundRadius		= VehShaderGroupLookupVar(pShaderGroup, "BoundRadius",		false);
	m_idVarDamageMult		= VehShaderGroupLookupVar(pShaderGroup, "DamageMultiplier",	false);
	m_idVarOffset			= VehShaderGroupLookupVar(pShaderGroup, "DamageTextureOffset", false);
	m_idVarDamagedFrontWheelOffsets = VehShaderGroupLookupVar(pShaderGroup, "DamagedWheelOffsets", false);
#if __BANK
	m_idVarDebugDamageMap	= VehShaderGroupLookupVar(pShaderGroup, "bDebugDisplayDamageMap",	false);
	m_idVarDebugDamageMult	= VehShaderGroupLookupVar(pShaderGroup, "bDebugDisplayDamageScale",	false);
#endif
#endif

	m_idVarDiffuseTex2		= VehShaderGroupLookupVar(pShaderGroup, "DiffuseTex2", false);
	m_idVarDiffuseTex3		= VehShaderGroupLookupVar(pShaderGroup, "DiffuseTex3", false);
	m_idVarTrackAnimUV		= VehShaderGroupLookupVar(pShaderGroup, "TrackAnimUV0", FALSE);
	m_idVarTrack2AnimUV		= VehShaderGroupLookupVar(pShaderGroup, "Track2AnimUV0", FALSE);
	m_idVarTrackAmmoAnimUV	= VehShaderGroupLookupVar(pShaderGroup, "TrackAmmoAnimUV0", FALSE);
 	m_idVarEnvEffScale		= VehShaderGroupLookupVar(pShaderGroup, "envEffScale0", FALSE);
	m_idVarEmissiveMult		= VehShaderGroupLookupVar(pShaderGroup, "EmissiveMultiplier", false);

	// Just spew how much space it *would* take...
	// pShaderGroup->Clone(NULL);
	// Displayf("...and the rest of the object is %u bytes",Size());

	m_idLicensePlateBgTex		= VehShaderGroupLookupVar(pShaderGroup, "PlateBgTex", false);
	m_idLicensePlateBgNormTex	= VehShaderGroupLookupVar(pShaderGroup, "PlateBgBumpTex", false);
	m_idLicensePlateFontColor	= VehShaderGroupLookupVar(pShaderGroup, "LicensePlateFontTint", false);
	m_idLicensePlateText1		= VehShaderGroupLookupVar(pShaderGroup, "LetterIndex1", false);
	m_idLicensePlateText2		= VehShaderGroupLookupVar(pShaderGroup, "LetterIndex2", false);

	m_idLicensePlateFontOutlineColor				= VehShaderGroupLookupVar(pShaderGroup, "FontOutlineColor", false);
	m_idLicensePlateFontExtents						= VehShaderGroupLookupVar(pShaderGroup, "LicensePlateFontExtents", false);
	m_idLicensePlateFontOutlineMinMaxDepth_Enabled	= VehShaderGroupLookupVar(pShaderGroup, "FontOutlineMinMaxDepthEnabled", false);
	m_idLicensePlateMaxLettersOnPlate				= VehShaderGroupLookupVar(pShaderGroup, "LicensePlateMaxLetters", false);

#if USE_DISKBRAKEGLOW
	m_varDiskBrakeGlow			= 0.0f;
#endif // USE_DISKBRAKEGLOW

	m_bIsHighDetail				= false;

	return(TRUE);
}// end of Initialise()...


//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectVehicleType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectVehicle *pNewShaderEffect = rage_new CCustomShaderEffectVehicle();

	CVehicleModelInfo *pVMI = (CVehicleModelInfo*)(pEntity->GetBaseModelInfo());
	Assert(pVMI);

	for (int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		pNewShaderEffect->m_pType[i] = this;
	}

	pNewShaderEffect->m_vDamagedFrontWheelOffsets[0] = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	pNewShaderEffect->m_vDamagedFrontWheelOffsets[1] = Vector4(0.0f, 0.0f, 0.0f, 0.0f);

	pNewShaderEffect->SetDirtLevel(0.0f);
	pNewShaderEffect->SetDirtColor(this->GetDefaultDirtColor());
	pNewShaderEffect->SetBurnoutLevel(0.0f);

	for(s32 i=0; i<7; i++)
	{
		pNewShaderEffect->m_varDifCol[i]		= Color32(0,0,0);
		pNewShaderEffect->m_varMetallicID[i]	= (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;
	}
	pNewShaderEffect->SetSpec2Color(0,0,0);
	pNewShaderEffect->SetSpec2DirLerp(0.0f);
	pNewShaderEffect->SetDiffuseTint(pVMI->GetDiffuseTint());
	pNewShaderEffect->m_bRenderScorched			= FALSE;
	pNewShaderEffect->m_pLiveryTexture			= NULL;

	pNewShaderEffect->m_liveryFragSlotIdx		= strLocalIndex::INVALID_INDEX;

	pNewShaderEffect->UpdateVehicleColors(pEntity);

	pNewShaderEffect->m_bTyreDeformSwitchOn		= false;
	pNewShaderEffect->m_bBurstWheel				= false;
	pNewShaderEffect->m_bDamageSwitchOn			= false;
	pNewShaderEffect->m_pDamageTexture			= NULL;

#if VEHICLE_SUPPORT_PAINT_RAMP
	for(u32 i = 0; i < NELEM(pNewShaderEffect->m_pRampTextures); ++i)
	{
		pNewShaderEffect->m_pRampTextures[i] = 0;
	}
#endif

	pNewShaderEffect->m_bCustomPrimaryColor		= false;
	pNewShaderEffect->m_bCustomSecondaryColor	= false;
	pNewShaderEffect->m_bIsBrokenOffPart		= false;
	pNewShaderEffect->m_bHasLiveryMod			= false;

	pNewShaderEffect->m_uvTrackAnim				= Vector2(0.0f,0.0f);
	pNewShaderEffect->m_uvTrack2Anim			= Vector2(0.0f,0.0f);
	pNewShaderEffect->m_uvTrackAmmoAnim			= Vector2(0.0f,0.0f);

	pNewShaderEffect->m_fUserEmissiveMultiplier	= 1.0f;

	for(u32 i=0; i<(4*CVehicleLightSwitch::LW_LIGHTVECTORS+1); i++)
	{
		pNewShaderEffect->m_fLightDimmer[i]		= 1.0f;
	}

float envEff1=0.0f;
	const float envEffScaleMin = pVMI->GetEnvEffScaleMin();
	const float envEffScaleMax = pVMI->GetEnvEffScaleMax();
	if(envEffScaleMin < envEffScaleMax)
		envEff1 = g_DrawRand.GetRanged(envEffScaleMin, envEffScaleMax);
	else
		envEff1 = envEffScaleMin;	// do not rand() if min==max

float envEff2=0.0f;
	const float envEffScaleMin2 = pVMI->GetEnvEffScaleMin2();
	const float envEffScaleMax2 = pVMI->GetEnvEffScaleMax2();
	if(envEffScaleMin2 < envEffScaleMax2)
		envEff2 = g_DrawRand.GetRanged(envEffScaleMin2, envEffScaleMax2);
	else
		envEff2 = envEffScaleMin2;	// do not rand() if min==max

	pNewShaderEffect->SetEnvEffScale(g_DrawRand.GetBool()? envEff1 : envEff2);	// equally select from 1st or 2nd range


	pNewShaderEffect->m_pLicensePlateDiffuseTex	= NULL;
	pNewShaderEffect->m_pLicensePlateNormalTex	= NULL;

	int seed = 1;
	if(pEntity->GetIsTypeVehicle())
		seed = static_cast<CVehicle*>(pEntity)->GetRandomSeed();

	pNewShaderEffect->GenerateLicensePlateText(seed);

	pNewShaderEffect->AddKnownRefs();

	//The license plate textures to use should be driven later by script. (Same as the livery stuff) However, since this functionality is
	//new, this is not currently set up. So I have added the code block below to set the license plate textures to the default textures. (If
	//a default is defined.) This way, the license plates will draw correctly in the mean time, and as a proof of concept, any instance of a
	//vehicle can be spawned with a different license plate texture just by changing the default texture index in Rag and spawning a new instance.
	//
	//Once this is hooked up through code, it may make sense to remove this functionality completely. However, if a we do that and script misses a
	//call to set the plate background texture, the resulting visual may or may not look very strange and completely incorrect depending on what is
	//drawn before it. So perhaps to identify this easily, in all __BANK or !__FINAL builds we could have some default error texture to make any
	//problems very obvious. If we go with this method, this code block should probably be moved to the Initialise() function.
	//
	//IMPORTANT Note: This code *must* come after the AddKnownRefs() call, or the game may assert and become unstable. -AJG
	pNewShaderEffect->SelectLicensePlateByProbability(pEntity);
	InitialiseClothDrawable(pEntity);

	pNewShaderEffect->SetDamageTex(const_cast<grcTexture*>(grcTexture::NoneBlack));
	return(pNewShaderEffect);
}

//
//
// initializes vehicle cloth drawable:
//
rmcDrawable* CCustomShaderEffectVehicleType::InitialiseClothDrawable(CEntity *pEntity) const
{
	// initialize vehicle cloth drawable instance (only for SD vehicle):
	fragInst* pFragInst = pEntity->GetFragInst();
	if(pFragInst && pEntity->m_nFlags.bIsFrag && (!this->GetIsHighDetail()))
	{
		// Search for a cloth drawable
		rmcDrawable *pClothDrawable = NULL;
		fragCacheEntry *cacheEntry = pFragInst->GetCacheEntry();
		if(cacheEntry)
		{
			fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
			Assertf( hierInst,"No HierInst on a cached frag" );
			environmentCloth* pEnvCloth = hierInst->envCloth;
			if( pEnvCloth )
			{
				pClothDrawable = pEnvCloth->GetDrawable();
			}
		}

		if(pClothDrawable)
		{
			grmShaderGroup *pClothShaderGroup = &pClothDrawable->GetShaderGroup();
			if(pClothShaderGroup->GetShaderGroupVarCount()==0)
			{
				CCustomShaderEffectVehicleType *pClothCseType = CCustomShaderEffectVehicleType::Create(pClothDrawable, NULL, NULL);
				if(!pClothCseType->Initialise(pClothDrawable))
				{
					Assertf(FALSE, "Error initialising CustomShaderEffect!");
				}

				// check if clothCSE matches SD vehicleCSE:
				#if __ASSERT
				#define ASSERT_COMPARE_VAR0(VAR) Assertf(this->VAR == pClothCseType->VAR, "Value of "#VAR" doesn't match for vehicle SD CSE and vehicle cloth !");
				#define ASSERT_COMPARE_VAR(VAR)  Assertf(this->VAR == pClothCseType->VAR, "Value of "#VAR" doesn't match for vehicle's SD CSE (=%d) and vehicle's cloth CSE (=%d)!",this->VAR,pClothCseType->VAR);
					ASSERT_COMPARE_VAR0(m_DefDirtColor);
					ASSERT_COMPARE_VAR(m_idVarDiffuse2Color);
					ASSERT_COMPARE_VAR(m_idVarDirtLevelMod);
					ASSERT_COMPARE_VAR(m_idVarDirtColor);
					ASSERT_COMPARE_VAR(m_idVarDiffuseColorTint);

					ASSERT_COMPARE_VAR(m_idVarDiffuseTex2);
					ASSERT_COMPARE_VAR(m_idVarDiffuseTex3);
					ASSERT_COMPARE_VAR(m_idVarTyreDeformSwitchOn);
					ASSERT_COMPARE_VAR(m_idVarTrackAnimUV);

					ASSERT_COMPARE_VAR(m_idVarTrack2AnimUV);
					ASSERT_COMPARE_VAR(m_idVarTrackAmmoAnimUV);
					ASSERT_COMPARE_VAR(m_idVarEnvEffScale);
					ASSERT_COMPARE_VAR(m_idLicensePlateBgTex);

					ASSERT_COMPARE_VAR(m_idLicensePlateBgNormTex);
					ASSERT_COMPARE_VAR(m_idLicensePlateFontColor);
					ASSERT_COMPARE_VAR(m_idLicensePlateText1);
					ASSERT_COMPARE_VAR(m_idLicensePlateText2);

					ASSERT_COMPARE_VAR(m_idLicensePlateFontOutlineColor);
					ASSERT_COMPARE_VAR(m_idLicensePlateFontExtents);
					ASSERT_COMPARE_VAR(m_idLicensePlateFontOutlineMinMaxDepth_Enabled);
					ASSERT_COMPARE_VAR(m_idLicensePlateMaxLettersOnPlate);

					ASSERT_COMPARE_VAR(m_idVarDimmer);
				#if USE_GPU_VEHICLEDEFORMATION
					ASSERT_COMPARE_VAR(m_idVarDamageSwitchOn);
					ASSERT_COMPARE_VAR(m_idVarBoundRadius);
					ASSERT_COMPARE_VAR(m_idVarDamageMult);
					ASSERT_COMPARE_VAR(m_idVarDamageTex);
				#endif
				#if USE_DISKBRAKEGLOW
					ASSERT_COMPARE_VAR(m_idVarDiskBrakeGlow);
				#endif
					ASSERT_COMPARE_VAR(m_bHasTruckTex);
					ASSERT_COMPARE_VAR(m_bIsHighDetail);
					ASSERT_COMPARE_VAR(m_bHasBodyColor1);
					ASSERT_COMPARE_VAR(m_bHasBodyColor2);
					ASSERT_COMPARE_VAR(m_bHasBodyColor3);
					ASSERT_COMPARE_VAR(m_bHasBodyColor4);
					ASSERT_COMPARE_VAR(m_bHasBodyColor5);
					ASSERT_COMPARE_VAR(m_bHasBodyColor6);
					ASSERT_COMPARE_VAR(m_bHasBodyColor7);

					ASSERT_COMPARE_VAR(m_DiffColEffect.GetCount());
					ASSERT_COMPARE_VAR(m_SpecColEffect.GetCount());
					ASSERT_COMPARE_VAR(m_DirtTexEffect.GetCount());
					ASSERT_COMPARE_VAR(m_Tex2Effect.GetCount());
				#undef ASSERT_COMPARE_VAR0
				#undef ASSERT_COMPARE_VAR
				#endif //__ASSERT...

				delete pClothCseType;
				pClothCseType = NULL;
				return(pClothDrawable);
			}
		}
	}// vehicle cloth drawable instance...

	return(NULL);
}

//
//
//
//
grcTexture* CCustomShaderEffectVehicleType::GetDirtTexture(u32 shaderIdx) const
{
	const u32 dirtTexCount = m_DirtTexEffect.GetCount();
	for(u32 i=0; i<dirtTexCount; i++)
	{
		const CCustomShaderEffectVehicleType::structDirtTexEffect *ptr = &m_DirtTexEffect[i];
		if(ptr->shaderIdx == shaderIdx)
		{
			return (grcTexture*)ptr->pOrigDirtTex;
		}
	}
	return(NULL);
}

//
//
//
//
CCustomShaderEffectVehicle::CCustomShaderEffectVehicle() : CCustomShaderEffectBase(sizeof(*this), CSE_VEHICLE)
{
	m_nLicensePlateTexIdx		=
	m_nLiveryIdx				=
	m_nLivery2Idx				= -1;

	m_pLicensePlateDiffuseTex	= NULL;
	m_pLicensePlateNormalTex	= NULL;

	m_bCustomPrimaryColor		= false;
	m_bCustomSecondaryColor		= false; //ensure this is properly initialized

	memset(m_LicensePlateText,0,sizeof(m_LicensePlateText));
}

//
//
//
//
CCustomShaderEffectVehicle::~CCustomShaderEffectVehicle()
{
	RemoveKnownRefs();
	// m_pType is cleaned up in ShutdownMaster
}

//
//
//
//
void CCustomShaderEffectVehicle::CopySettings(CCustomShaderEffectVehicle *src, CEntity* /*pEntity*/)
{
	sysMemCpy(m_fLightDimmer, src->m_fLightDimmer, sizeof(m_fLightDimmer));
	this->m_fBoundRadius			= src->m_fBoundRadius;
	this->m_fDamageMultiplier		= src->m_fDamageMultiplier;
	sysMemCpy(this->m_vDamagedFrontWheelOffsets, src->m_vDamagedFrontWheelOffsets, sizeof(src->m_vDamagedFrontWheelOffsets));

	sysMemCpy(this->m_varDifCol, src->m_varDifCol, sizeof(src->m_varDifCol));
	this->m_varSpec2Color_DirLerp	= src->m_varSpec2Color_DirLerp;
	this->m_varDiffColorTint		= src->m_varDiffColorTint;
	this->m_varDirtColor_DirtLevel	= src->m_varDirtColor_DirtLevel;
	sysMemCpy(this->m_varMetallicID, src->m_varMetallicID, sizeof(src->m_varMetallicID));

	this->m_pDamageTexture			= src->m_pDamageTexture;
#if VEHICLE_SUPPORT_PAINT_RAMP
	for(u32 i = 0; i < NELEM(this->m_pRampTextures); ++i)
	{
		this->m_pRampTextures[i] = src->m_pRampTextures[i];
	}
#endif
//	grcTextureIndex					m_pLiveryTexture;
	this->m_liveryFragSlotIdx		= src->m_liveryFragSlotIdx;
	this->m_nLiveryIdx				= src->m_nLiveryIdx;
	this->m_nLivery2Idx				= src->m_nLivery2Idx;
	this->m_bCustomPrimaryColor		= src->m_bCustomPrimaryColor;
	this->m_bCustomSecondaryColor	= src->m_bCustomSecondaryColor;
	this->m_bIsBrokenOffPart		= src->m_bIsBrokenOffPart;
	this->m_bHasLiveryMod			= src->m_bHasLiveryMod;
	this->m_uvTrackAnim				= src->m_uvTrackAnim;
	this->m_uvTrack2Anim			= src->m_uvTrack2Anim;
	this->m_uvTrackAmmoAnim			= src->m_uvTrackAmmoAnim;

	this->m_fUserEmissiveMultiplier	= src->m_fUserEmissiveMultiplier;

	SetLicensePlateTexIndex(src->m_nLicensePlateTexIdx);
//	grcTextureIndex					m_pLicensePlateDiffuseTex;
//	grcTextureIndex					m_pLicensePlateNormalTex;
	sysMemCpy(this->m_LicensePlateText, src->m_LicensePlateText, sizeof(src->m_LicensePlateText));

	sysMemCpy(this->m_wheelBurstSideRatios, src->m_wheelBurstSideRatios, sizeof(src->m_wheelBurstSideRatios));

#if USE_DISKBRAKEGLOW
	m_varDiskBrakeGlow				= src->m_varDiskBrakeGlow;
#endif
	this->m_varEnvEffScale			= src->m_varEnvEffScale;
	this->m_varBurnoutLevel			= src->m_varBurnoutLevel;

	this->m_bRenderScorched			= src->m_bRenderScorched;
	this->m_bTyreDeformSwitchOn		= src->m_bTyreDeformSwitchOn;
	this->m_bBurstWheel				= src->m_bBurstWheel;
	this->m_bDamageSwitchOn			= src->m_bDamageSwitchOn;
}


//
//
//
//
void CCustomShaderEffectVehicle::Update(fwEntity* pEntityBase)
{
	CEntity* pEntity = static_cast<CEntity*>(pEntityBase);
	Assert(pEntity);
	if(pEntity->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(pEntity);

		// dirt test:
//		const float time = float(fwTimer::GetTimeInMilliseconds()) * 0.001f;
//		this->SetDirtLevel(rage::Sinf(time)*0.5f + 0.5f);
		this->SetDirtLevel(pVehicle->GetBodyDirtLevel() / 15.0f);
		this->SetDirtColor(pVehicle->GetBodyDirtColor());

		this->m_bHasLiveryMod = pVehicle->GetVariationInstance().GetNumModsForType(VMT_LIVERY_MOD, pVehicle) > 0;

#if !__FINAL
		if(gbDontDisplayDirtOnVehicles || PARAM_novehicledirt.Get())
			this->SetDirtLevel(0.0f);
#endif //__DEV...


		if(pVehicle->m_nPhysicalFlags.bRenderScorched)	// vehicle burntout/exploded?
		{
			this->m_bRenderScorched = TRUE;
			float fBurnoutLevelInc =
										#if CSE_VEHICLE_EDITABLEVALUES
											ms_fEVBurnoutSpeed;
										#else
											 0.01f;
										#endif
#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				ReplayVehicleExtension* pExt = ReplayVehicleExtension::GetExtension(pVehicle);
				if(pExt && pExt->GetIncScorch() == false)
				{
					fBurnoutLevelInc = 0.0f;	// Don't increment scorch...replay is in control
				}
			}
#endif // GTA_REPLAY

			this->SetBurnoutLevel(GetBurnoutLevel()+fBurnoutLevelInc);
			this->SetDirtLevel(1.0f);
			this->UpdateVehicleColors(pVehicle);
		}
		else
		{
			this->m_bRenderScorched = FALSE;
			this->SetBurnoutLevel(0.0f);
		}

		Color32 newWindowTint = GetDiffuseTint();
		if (pVehicle->GetVariationInstance().GetKitIndex() != INVALID_VEHICLE_KIT_INDEX && pVehicle->GetVariationInstance().GetWindowTint() != 0xff)
		{
			newWindowTint = CVehicleModelInfo::GetVehicleColours()->GetVehicleWindowColor(pVehicle->GetVariationInstance().GetWindowTint());
		}
		SetDiffuseTint(newWindowTint);

		const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
		this->SetSpec2DirLerp(currKeyframe.GetVar(TCVAR_LIGHT_VEHICLE_SECOND_SPEC_OVERRIDE));

#if USE_DISKBRAKEGLOW
		if( GetCseType()->m_idVarDiskBrakeGlow )
		{
			// That's our current temperature
			float wheelTemperature = m_varDiskBrakeGlow;

			// Update glow level if needed
			if( pVehicle->GetHasGlowingBrakes() )
			{
				const float engineOn = (float)(fwTimer::GetTimeInMilliseconds() - pVehicle->m_EngineSwitchedOnTime);
				
				// Gather linear speed and brake force from the wheels
				const int numWheels = pVehicle->GetNumWheels();

#if DISKBRAKEGLOW_DEBUG_TEXT_FULLWHEELDETAIL
				float wheelBrakes[20];
				float wheelSpeed[20];
				float wheelAngleVel[20];
				float wheelSlipRatio[20];
#endif // DISKBRAKEGLOW_DEBUG_TEXT
				float accumWheelSpeed = 0.0f;
				float accumBrakeForce = 0.0f;

				for(int i=0;i<numWheels;i++)
				{
					const CWheel *wheel = pVehicle->GetWheel(i);
					if( wheel )
					{
						accumWheelSpeed += wheel->GetGroundSpeed();
						accumBrakeForce += wheel->GetBrakeForce();
#if DISKBRAKEGLOW_DEBUG_TEXT_FULLWHEELDETAIL
						wheelBrakes[i] = wheel->GetBrakeForce();
						wheelSpeed[i] = wheel->GetGroundSpeed();
						wheelAngleVel[i] = wheel->GetRotSpeed();
						wheelSlipRatio[i] = wheel->GetRotSlipRatio();
#endif // DISKBRAKEGLOW_DEBUG_TEXT_FULLWHEELDETAIL
					}
				}

				const float fnumWheels = (float)numWheels;
				accumBrakeForce /= fnumWheels;
				accumWheelSpeed /= fnumWheels;
#if DISKBRAKEGLOW_DEBUG_TEXT
				{
					char msg[1024];
					static int StartX = 100;
					static int StartY = 10;

					int y = StartY;

					sprintf(msg,"breakped %f",pVehicle->GetBrake());
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"HandBreak %d",pVehicle->GetHandBrake());
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"BrakeForce %f",accumBrakeForce);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"WheelSpeed %f",accumWheelSpeed);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"SpeedCap %f",g_fDiskBrake_SpeedCap);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"MagicNumber %f",g_fDiskBrake_TempMagicNumber);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"EngineOn %f",engineOn);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					const float EngineAge = rage::Clamp((engineOn - g_fDiskBrake_EngineAge) * (1.0f / g_fDiskBrake_EngineAge),0.0f,1.0f);
					sprintf(msg,"EngineAge %f",EngineAge);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
#if DISKBRAKEGLOW_V2
					const float nWheelSpeed = rage::Max(0.0f,(accumWheelSpeed - g_fDiskBrake_SpeedCap) * (1.0f / g_fDiskBrake_SpeedCap));
					const float sWheelSpeed = rage::square(nWheelSpeed);
					const float nHeatingRate = g_fDiskBrake_MaxHeatingRate * nWheelSpeed;
					const float sHeatingRate = g_fDiskBrake_MaxHeatingRate * sWheelSpeed;
					const float heatingRate = rage::Lerp(accumBrakeForce,nHeatingRate,sHeatingRate);
#else // DISKBRAKEGLOW_V2
					const float heatingRate = (accumWheelSpeed / g_fDiskBrake_TempMagicNumber) * accumBrakeForce;
#endif // DISKBRAKEGLOW_V2
					sprintf(msg,"heatingRate %f",heatingRate);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"WheelTemp %f",wheelTemperature);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					static float maxwheelTemperature = 0.0f;
					static float minwheelTemperature = FLT_MAX;
					if( wheelTemperature > maxwheelTemperature )
					{
						maxwheelTemperature = wheelTemperature;
					}
					
					if( wheelTemperature < minwheelTemperature )
					{
						minwheelTemperature = wheelTemperature;
					}
					sprintf(msg,"minwheelTemperature %f",minwheelTemperature);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					sprintf(msg,"maxwheelTemperature %f",maxwheelTemperature);
					grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
					y++;
					
#if DISKBRAKEGLOW_DEBUG_TEXT_FULLWHEELDETAIL
					for(int i=0;i<numWheels;i++)
					{
						const CWheel *wheel = pVehicle->GetWheel(i);
						if( wheel )
						{
							sprintf(msg,"Wheel Speed %d:%f",i,wheelSpeed[i]);
							grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
							y++;
							sprintf(msg,"Wheel Break %d:%f",i,wheelBrakes[i]);
							grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
							y++;
							sprintf(msg,"wheelAngleVel %d:%f",i,wheelAngleVel[i]);
							grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
							y++;
							sprintf(msg,"wheelSlipRatio %d:%f",i,wheelSlipRatio[i]);
							grcDebugDraw::PrintToScreenCoors(msg,StartX,y);
							y++;
						}
					}
#endif // DISKBRAKEGLOW_DEBUG_TEXT_FULLWHEELDETAIL
				}
#endif // DISKBRAKEGLOW_DEBUG_TEXT

				// Scale the disk "temperature" (actually 0 to 1) based on the vehicle's behaviour
				const float timestep = fwTimer::GetTimeStep();
				if( (pVehicle->GetBrake() > 0.05f) &&
					(false == pVehicle->GetHandBrake()) &&
					(accumBrakeForce > 0.0f) &&
					(accumWheelSpeed > g_fDiskBrake_SpeedCap) )
				{
#if DISKBRAKEGLOW_V2
					const float nWheelSpeed = rage::Max(0.0f,(accumWheelSpeed - g_fDiskBrake_SpeedCap) * (1.0f / g_fDiskBrake_SpeedCap));
					const float sWheelSpeed = rage::square(nWheelSpeed);
					const float nHeatingRate = g_fDiskBrake_MaxHeatingRate * nWheelSpeed;
					const float sHeatingRate = g_fDiskBrake_MaxHeatingRate * sWheelSpeed;
					const float heatingRate = rage::Lerp(accumBrakeForce,nHeatingRate,sHeatingRate);
#else // DISKBRAKEGLOW_V2
					const float heatingRate = (accumWheelSpeed / g_fDiskBrake_TempMagicNumber) * accumBrakeForce;
#endif // DISKBRAKEGLOW_V2
					const float EngineAge = rage::Clamp((engineOn - g_fDiskBrake_EngineAge) * (1.0f / g_fDiskBrake_EngineAge),0.0f,1.0f);
					wheelTemperature = rage::Min(0.5f,wheelTemperature + (timestep * heatingRate * EngineAge));
				}
				else
				{
					wheelTemperature = rage::Max(0.0f,wheelTemperature - (timestep * g_fDiskBrake_CoolingRate));
				}

			}
			else
			{
				wheelTemperature = 0.0f;
			}

#if DISKBRAKEGLOW_DEBUG_SHADER
			static float frequency = 1500.0f;
			static float maxLum = 1.0f;

			wheelTemperature = rage::Abs((rage::Sinf(((float)fwTimer::GetTimeInMilliseconds()/frequency)*PI) * maxLum));
#endif // DISKBRAKEGLOW_DEBUG_SHADER
		
			m_varDiskBrakeGlow = wheelTemperature;
		}
#endif // USE_DISKBRAKEGLOW


// debug test tank track anim:
#if 0
static dev_bool	gbDbgTrackAnim			= TRUE;
static dev_float	gbDbgTrackAnimAddRight	= -0.01f;
static dev_float	gbDbgTrackAnimAddLeft	= 0.01f;
	if(gbDbgTrackAnim)
	{
		if(m_idVarTrackAnimUV)
		{
			Vector2 a;
			a.x = gbDbgTrackAnimAddRight;
			a.y = gbDbgTrackAnimAddLeft;
			this->TrackUVAnimAdd( a );
		}
	}
#endif //#if 0...


		// calculate sun glare factor:
#if CSE_VEHICLE_EDITABLEVALUES
		if(ms_bEVSunGlareEnabled)
#else
		if(0)
#endif
		{
			float glare_s = 1.0f;
			const Vector3& cameraPos	= camInterface::GetPos();
			const Vector3& cameraFront	= camInterface::GetFront();
			const Vector3 vehiclePos	= VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

			// distance factor:
			#if CSE_VEHICLE_EDITABLEVALUES
				const float glareMinDist  = ms_fEVSunGlareDistMin;
				const float glareMaxDist  =	ms_fEVSunGlareDistMax;
			#else
				const float glareMinDist  =	65.0f;
				const float glareMaxDist  =	75.0f;
			#endif
			
			const Vector3 distV(cameraPos - vehiclePos);
			const float dist2 = distV.Mag2();
			glare_s *= 1.0f - rage::Clamp((glareMaxDist*glareMaxDist-dist2)/(glareMaxDist*glareMaxDist-glareMinDist*glareMinDist), 0.0f, 1.0f);

			const CLightSource &dirLight = Lights::GetRenderDirectionalLight();

			// "only during day" factor:
			glare_s *= rage::Clamp(dirLight.GetIntensity()-2.0f, 0.0f, 1.0f);

			// view/sun angle factor:
			Vector3 surface_worldNormal(cameraPos - vehiclePos);	// fake "normal"
			surface_worldNormal.Normalize();

			Vector3 surfaceToEyeDirection(-cameraFront);
			surfaceToEyeDirection.Normalize();

			Vector3 halfVector = surfaceToEyeDirection - dirLight.GetDirection();
			halfVector.Normalize();

			float glareSpec = rage::Clamp(surface_worldNormal.Dot(halfVector), 0.0f, 1.0f);

			#if CSE_VEHICLE_EDITABLEVALUES
				const u32 powCount = ms_nEVSunGlarePow;
			#else
				const u32 powCount = 32;
			#endif
			float glareSpecFactor = 1.0f;
			for(u32 i=0; i<powCount; i++)
			{
				glareSpecFactor *= glareSpec;
			}
			glare_s *= glareSpecFactor;

			glare_s = rage::Clamp(glare_s, 0.0f, 1.0f);
		
			// SunGlareFx v2:
			if(glare_s > 0.0f)
			{
				Vector3 boundCentre;
				const float boundRadius = pVehicle->GetBoundCentreAndRadius(boundCentre);

				Vector3 coronaPos = boundCentre;

				#if CSE_VEHICLE_EDITABLEVALUES
					const float spriteZShiftScale = ms_fEVSunGlareSpriteZShiftScale;
				#else
					const float spriteZShiftScale = 0.2f;
				#endif
				coronaPos.z += boundRadius*spriteZShiftScale;	// shift glare sprite a little upwards to cover windscreen, etc.

				#if CSE_VEHICLE_EDITABLEVALUES
					const float coronaSizeMult = ms_fEVSunGlareSpriteSize;
				#else
					const float coronaSizeMult = 0.55f;
				#endif
				const float coronaSize = boundRadius * coronaSizeMult * glare_s;
				
				u32 colorInt = u32(255.0f*glare_s);
				Color32 coronaColor(colorInt,colorInt,colorInt,colorInt); 

				#if CSE_VEHICLE_EDITABLEVALUES
					const float coronaHdrIntensityMult = ms_fEVSunGlareHdrMult;
				#else
					const float coronaHdrIntensityMult = 8.0f;
				#endif
				const float coronaHdrIntensity = coronaHdrIntensityMult * glare_s * glare_s;
				const float zBias = boundRadius*1.5f;

				g_coronas.Register(RCC_VEC3V(coronaPos),
								   coronaSize, 
								   coronaColor,
								   coronaHdrIntensity, 
								   zBias, 
								   Vec3V(V_ZERO),
								   1.0f,
								   CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
								   CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
								   CORONA_DONT_REFLECT);
			}
		}

		// update tyre burst/side ratios:
		this->m_bBurstWheel = false;
		const u32 numWheels = pVehicle->GetNumWheels();
		if(numWheels)
		{
			float aBurstRatios[NUM_VEH_CWHEELS_MAX];
			float aSideRatios[NUM_VEH_CWHEELS_MAX];
			sysMemSet(aBurstRatios, 0x00, sizeof(aBurstRatios));
			sysMemSet(aSideRatios,  0x00, sizeof(aSideRatios));

			for(int i=0; i<numWheels; i++)
			{
				CWheel* pWheel = pVehicle->GetWheel(i);
				if(pWheel->GetTyreHealth() < TYRE_HEALTH_DEFAULT)
				{
					aBurstRatios[pWheel->GetHierarchyId() - VEH_WHEEL_LF] = pWheel->GetTyreBurstRatio();
					aSideRatios[pWheel->GetHierarchyId() - VEH_WHEEL_LF] = pWheel->GetTyreBurstSideRatio();
					this->m_bBurstWheel = true;
				}
			}

			// if any tyres are burst remap burst/side ratios to <0;255> (format directly used by DLC):
			if(this->m_bBurstWheel)
			{
				for(int i=0; i<NUM_VEH_CWHEELS_MAX; i++)
				{
					m_wheelBurstSideRatios[i][0] = (u8)(rage::Clamp(aBurstRatios[i], 0.0f, 1.0f) * 255.0f);
					m_wheelBurstSideRatios[i][1] = (u8)(rage::Clamp(0.5f * (aSideRatios[i] + 1.0f), 0.0f, 1.0f) * 255.0f);
				}
			}
		}// if(pVehicle->GetNumWheels())...

	}//if(pEntity->GetIsTypeVehicle())...
	else
	{
		// Assuming this is a wheel that's broken off
		this->m_bBurstWheel = GetEnableTyreDeform();
		if(this->m_bBurstWheel)
		{
			for(int i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
			{
				m_wheelBurstSideRatios[i][0] = 255;
				m_wheelBurstSideRatios[i][1] = 127;
			}
		}
	}
}// end of Update()...

#if VEHICLE_SUPPORT_PAINT_RAMP
static grcTexture* GetVehicleColourRampTexture(CVehicleModelInfoVarGlobal* pVehColours, s32 col)
{
	grcTexture* result = 0;
	fwTxd *rampTxd = CCustomShaderEffectVehicle::GetRampTxd();
	const char *rampTextureName = pVehColours->GetVehicleColourRampTextureName(col);
	if (rampTextureName && rampTextureName[0] && Verifyf(rampTxd, "Missing ramp Txd"))
	{
		u32 rampHash = fwTxd::ComputeHash(rampTextureName);
		result = rampTxd->Lookup(rampHash);
		Assert(result);
	}
	return result;
}
#endif

//
//
//
//
bool CCustomShaderEffectVehicle::UpdateVehicleColors(CEntity *pEntity)
{
	// needs to be at a multiple of 16
	AlignedCompileTimeAssert(OffsetOf(CCustomShaderEffectVehicle,m_fLightDimmer), 16);

	Assert(pEntity);

	// might not be a vehicle (fragments break off vehicles as CObjects but still point to the vehicle model info)
	// need to work out how to pass the parent vehicle through to these objects
	CRGBA cols[7];
	float fBodyColorScale = 1.0f;
	u8	  metallicId[7];
#if VEHICLE_SUPPORT_PAINT_RAMP
	grcTexture* ramps[NELEM(m_pRampTextures)] = {};
#endif

	cols[4] = CRGBA(255,255,255,255);	// default interior color
	metallicId[4] = (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;

	CCustomShaderEffectVehicleType *pType = GetCseTypeInternal();

	bool bColorChanged = false;

	if(pEntity->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(pEntity);
		CVehicleModelInfoVarGlobal* pVehColours = CVehicleModelInfo::GetVehicleColours();
		Assert(pVehColours);

		cols[0] = pVehColours->GetVehicleColour(pVehicle->GetBodyColour1());
		cols[1] = pVehColours->GetVehicleColour(pVehicle->GetBodyColour2());
		cols[2] = pVehColours->GetVehicleColour(pVehicle->GetBodyColour3());
		cols[3] = pVehColours->GetVehicleColour(pVehicle->GetBodyColour4());
		cols[5] = pVehColours->GetVehicleColour(pVehicle->GetBodyColour5());					// color5 mapped to bodycolor6
		cols[6] = pVehColours->GetVehicleColour(pVehicle->GetBodyColour6());					// color6 mapped to bodycolor7

		metallicId[0] = pVehColours->GetVehicleColourMetallicID(pVehicle->GetBodyColour1());
		metallicId[1] = pVehColours->GetVehicleColourMetallicID(pVehicle->GetBodyColour2());
		metallicId[2] = pVehColours->GetVehicleColourMetallicID(pVehicle->GetBodyColour3());
		metallicId[3] = pVehColours->GetVehicleColourMetallicID(pVehicle->GetBodyColour4());
		metallicId[5] = pVehColours->GetVehicleColourMetallicID(pVehicle->GetBodyColour5());	// color5 mapped to bodycolor6
		metallicId[6] = pVehColours->GetVehicleColourMetallicID(pVehicle->GetBodyColour6());	// color6 mapped to bodycolor7

#if VEHICLE_SUPPORT_PAINT_RAMP
		ramps[0] = GetVehicleColourRampTexture(pVehColours, pVehicle->GetBodyColour1());
		ramps[1] = GetVehicleColourRampTexture(pVehColours, pVehicle->GetBodyColour2());
		ramps[2] = GetVehicleColourRampTexture(pVehColours, pVehicle->GetBodyColour3());
		ramps[3] = GetVehicleColourRampTexture(pVehColours, pVehicle->GetBodyColour4());
#endif

		if(pVehicle->m_nPhysicalFlags.bRenderScorched)
		{
			// vehicle burntout/exploded:
			this->m_bRenderScorched = TRUE;

			// fBodyColorScale = 0.1f;

			// default interior (bodycolor5) color:
			cols[4] = CRGBA(128,128,128);
		}

		
		if( pType->m_idVarDiffuseTex2 )
		{
			CVehicleModelInfo *modelInfo = pVehicle->GetVehicleModelInfo();
			Assert(modelInfo);

			if( modelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LIVERY) )
			{
				Assertf(modelInfo->GetLiveriesCount() >0, "%s:Livery flag but no liveries ?", modelInfo->GetModelName());

				if(m_nLiveryIdx != -1) // livery ID to use has already by decided
				{
					u32 liveryHash = modelInfo->GetLiveryHash(m_nLiveryIdx);
					Assertf(liveryHash != 0xFFFFFFFF, "Invalid livery texture on vehicle '%s'", modelInfo->GetModelName());

					grcTexture* liveryTex = NULL;
					//if (m_pType->GetIsHighDetail())
					//{
					//      s32 hdTxdIdx = modelInfo->GetHDTxdIndex(); 
					//      fwTxd* hdTxd = g_TxdStore.Get(hdTxdIdx); 
					//      Assert(hdTxd);
					//
					//      if (hdTxd)
					//          liveryTex = hdTxd->Lookup(liveryHash);
					//}

					if (!liveryTex)
					{
						strLocalIndex txdIdx = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						fwTxd* txd = g_TxdStore.Get(txdIdx);

						if (AssertVerify(txd))
							liveryTex = txd->Lookup(liveryHash);
					}

					m_pLiveryTexture = liveryTex;
					Assert(m_pLiveryTexture);
				}
				else
				{
					if( modelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LIVERY_MATCH_EXTRA) ) // Match extras to livery
					{
						m_nLiveryIdx = -1;
						int extraIdx = -1;

						if( pVehicle->m_nDisableExtras ) // anything turned on ?
						{
							for(int i=0; i<(VEH_LAST_EXTRA - VEH_EXTRA_1); i++)
							{
								if( !(pVehicle->m_nDisableExtras &BIT(i)) )
								{
									// Enabled
									extraIdx = i;
									break;
								}
							}
							m_nLiveryIdx = extraIdx;
						}

						if( m_nLiveryIdx == -1 )
						{
							if( extraIdx == -1 )
							{ // extra with no matching textures, hence just use the last texture (supposedly blank)
								m_nLiveryIdx = modelInfo->GetLiveriesCount()-1;
							}
							else
							{ // No extras, just pick a random one
								m_nLiveryIdx = fwRandom::GetRandomNumberInRange(0, modelInfo->GetLiveriesCount());
							}
						}
						else
						{ // One extra : are we using the matching or the last texture (blank)
							bool noLivery = (0 != (fwRandom::GetRandomNumberInRange(0, 3) & 1));
							if( noLivery )
							{
								m_nLiveryIdx = modelInfo->GetLiveriesCount()-1;
							}
						}

						u32 liveryHash = modelInfo->GetLiveryHash(m_nLiveryIdx);
						Assertf(liveryHash != 0xFFFFFFFF, "Invalid livery texture on vehicle '%s'", modelInfo->GetModelName());

						strLocalIndex txdIdx = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						fwTxd *txd = g_TxdStore.Get(txdIdx);

						if (AssertVerify(txd))
						{
							grcTexture *liveryTex = txd->Lookup(liveryHash);
							m_pLiveryTexture = liveryTex;
							Assert(m_pLiveryTexture);
						}
					}
					else
					{
						m_nLiveryIdx = fwRandom::GetRandomNumberInRange(0, modelInfo->GetLiveriesCount());

						u32 liveryHash = modelInfo->GetLiveryHash(m_nLiveryIdx);
						Assertf(liveryHash != 0xFFFFFFFF, "Invalid livery texture on vehicle '%s'", modelInfo->GetModelName());

						strLocalIndex txdIdx = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						fwTxd *txd = g_TxdStore.Get(txdIdx);

						if (AssertVerify(txd))
						{
							grcTexture *liveryTex = txd->Lookup(liveryHash);
							m_pLiveryTexture = liveryTex;
							Assert(m_pLiveryTexture);
						}
					}
				}
			}
			else
			{
				m_pLiveryTexture = NULL;
			}
		}// if(pType->m_idVarDiffuseTex2)...

		if( pType->m_idVarDiffuseTex3 )
		{
			CVehicleModelInfo *modelInfo = pVehicle->GetVehicleModelInfo();
			Assert(modelInfo);

			if( modelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LIVERY) )
			{
				Assertf(modelInfo->GetLiveries2Count() >0, "%s:Livery flag but no liveries2 ?", modelInfo->GetModelName());

				if(m_nLivery2Idx != -1) // livery ID to use has already by decided
				{
					u32 livery2Hash = modelInfo->GetLivery2Hash(m_nLivery2Idx);
					Assertf(livery2Hash != 0xFFFFFFFF, "Invalid livery2 texture on vehicle '%s'", modelInfo->GetModelName());

					grcTexture* livery2Tex = NULL;
					if (!livery2Tex)
					{
						strLocalIndex txdIdx = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						fwTxd* txd = g_TxdStore.Get(txdIdx);

						if (AssertVerify(txd))
							livery2Tex = txd->Lookup(livery2Hash);
					}

					m_pLivery2Texture = livery2Tex;
					Assert(m_pLivery2Texture);
				}
				else
				{
					if( modelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LIVERY_MATCH_EXTRA) ) // Match extras to livery
					{
						m_nLivery2Idx = -1;
						int extraIdx = -1;

						if( pVehicle->m_nDisableExtras ) // anything turned on ?
						{
							for(int i=0; i<(VEH_LAST_EXTRA - VEH_EXTRA_1); i++)
							{
								if( !(pVehicle->m_nDisableExtras &BIT(i)) )
								{
									// Enabled
									extraIdx = i;
									break;
								}
							}
							m_nLivery2Idx = extraIdx;
						}

						if( m_nLivery2Idx == -1 )
						{
							if( extraIdx == -1 )
							{ // extra with no matching textures, hence just use the last texture (supposedly blank)
								m_nLivery2Idx = modelInfo->GetLiveries2Count()-1;
							}
							else
							{ // No extras, just pick a random one
								m_nLivery2Idx = fwRandom::GetRandomNumberInRange(0, modelInfo->GetLiveries2Count());
							}
						}
						else
						{ // One extra : are we using the matching or the last texture (blank)
							bool noLivery = (0 != (fwRandom::GetRandomNumberInRange(0, 3) & 1));
							if( noLivery )
							{
								m_nLivery2Idx = modelInfo->GetLiveries2Count()-1;
							}
						}

						u32 livery2Hash = modelInfo->GetLivery2Hash(m_nLivery2Idx);
						Assertf(livery2Hash != 0xFFFFFFFF, "Invalid livery2 texture on vehicle '%s'", modelInfo->GetModelName());

						strLocalIndex txdIdx = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						fwTxd *txd = g_TxdStore.Get(txdIdx);

						if (AssertVerify(txd))
						{
							grcTexture *livery2Tex = txd->Lookup(livery2Hash);
							m_pLivery2Texture = livery2Tex;
							Assert(m_pLivery2Texture);
						}
					}
					else
					{
						m_nLivery2Idx = fwRandom::GetRandomNumberInRange(0, modelInfo->GetLiveries2Count());

						u32 livery2Hash = modelInfo->GetLivery2Hash(m_nLivery2Idx);
						Assertf(livery2Hash != 0xFFFFFFFF, "Invalid livery2 texture on vehicle '%s'", modelInfo->GetModelName());

						strLocalIndex txdIdx = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						fwTxd *txd = g_TxdStore.Get(txdIdx);

						if (AssertVerify(txd))
						{
							grcTexture *livery2Tex = txd->Lookup(livery2Hash);
							m_pLivery2Texture = livery2Tex;
							Assert(m_pLivery2Texture);
						}
					}
				}
			}
			else
			{
				m_pLivery2Texture = NULL;
			}
		}// if(pType->m_idVarDiffuseTex3)...

	}
	else
	{
		cols[0] = cols[1] = cols[2] = cols[3] = cols[5] = cols[6] = CRGBA(150, 150,150, 255);
		metallicId[0] = metallicId[1] = metallicId[2] = metallicId[3] = metallicId[5] = metallicId[6] = (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;
	}

	Color32 primaryColor = m_varDifCol[0];
	Color32 secondaryColor = m_varDifCol[1];
	
	for(s32 i=0; i<7; i++)
	{
		if(i==4)		// skip bodycolor5 as it's interior hardcoded color
			continue;	

		Color32 newColor = Color32(cols[i].GetRedf()*fBodyColorScale, cols[i].GetGreenf()*fBodyColorScale, cols[i].GetBluef()*fBodyColorScale); 

		if(newColor != this->m_varDifCol[i])
		{
			bColorChanged = true;
		}

		this->m_varDifCol[i] = newColor;
		this->m_varMetallicID[i] = metallicId[i];
	}

#if VEHICLE_SUPPORT_PAINT_RAMP
	for(u32 i = 0; i < NELEM(ramps); ++i)
	{
		if(ramps[i] != m_pRampTextures[i])
		{
			bColorChanged = true;
		}

		this->m_pRampTextures[i] = ramps[i];
	}
#endif
	
	// Restore custom colors
	if( m_bCustomPrimaryColor )
		m_varDifCol[0] = primaryColor;
	if( m_bCustomSecondaryColor )
		m_varDifCol[1] = secondaryColor;
		
	// and interior color with no body color scale:
	this->m_varDifCol[4]		= cols[4];
	this->m_varMetallicID[4]	= metallicId[4];

	// this is specular color:
	this->SetSpec2Color(cols[2].GetRed(), cols[2].GetGreen(), cols[2].GetBlue());

	return bColorChanged;
}// end of UpdateVehicleColors()...

void CCustomShaderEffectVehicle::SetCustomPrimaryColor(Color32 col)
{
	m_varDifCol[0] = col;
	m_bCustomPrimaryColor = true;
}

Color32 CCustomShaderEffectVehicle::GetCustomPrimaryColor()
{
	return m_varDifCol[0];
}

void CCustomShaderEffectVehicle::SetCustomSecondaryColor(Color32 col)
{
	m_varDifCol[1] = col;
	m_bCustomSecondaryColor = true;
}

Color32 CCustomShaderEffectVehicle::GetCustomSecondaryColor()
{
	return m_varDifCol[1];
}

void CCustomShaderEffectVehicle::SetRed()
{
	m_varDifCol[0] =
	m_varDifCol[1] =
	m_varDifCol[2] =
	m_varDifCol[3] =
	m_varDifCol[4] =
	m_varDifCol[5] = 
	m_varDifCol[6] = Color32(255,0,0);

	m_varMetallicID[0] =
	m_varMetallicID[1] =
	m_varMetallicID[2] =
	m_varMetallicID[3] =
	m_varMetallicID[4] =
	m_varMetallicID[5] =
	m_varMetallicID[6] = (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;
}

void CCustomShaderEffectVehicle::SetGreen()
{
	m_varDifCol[0] =
	m_varDifCol[1] =
	m_varDifCol[2] =
	m_varDifCol[3] =
	m_varDifCol[4] =
	m_varDifCol[5] =
	m_varDifCol[6] = Color32(0,255,0);

	m_varMetallicID[0] =
	m_varMetallicID[1] =
	m_varMetallicID[2] =
	m_varMetallicID[3] =
	m_varMetallicID[4] =
	m_varMetallicID[5] =
	m_varMetallicID[6] = (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;
}

void CCustomShaderEffectVehicle::SetBlue()
{
	m_varDifCol[0] =
	m_varDifCol[1] =
	m_varDifCol[2] =
	m_varDifCol[3] =
	m_varDifCol[4] =
	m_varDifCol[5] =
	m_varDifCol[6] = Color32(0,0,255);

	m_varMetallicID[0] =
	m_varMetallicID[1] =
	m_varMetallicID[2] =
	m_varMetallicID[3] =
	m_varMetallicID[4] =
	m_varMetallicID[5] =
	m_varMetallicID[6] = (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;
}

//
//
//
//
void CCustomShaderEffectVehicle::AddToDrawList(u32 modelIndex, bool bExecute)
{
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, GetCseTypeInternal()));
}

//
//
//
// special version of SetShaderVariables() method for JohnW's multicore renderer
// as we don't have entities there, but only drawables;
//
void CCustomShaderEffectVehicle::SetShaderVariables(rmcDrawable* pDrawable)
{
	SetShaderVariables(pDrawable, Vector3(0.0, 0.0, 0.0));
}

void CCustomShaderEffectVehicle::SetShaderVariables(rmcDrawable* pDrawable, Vector3 boneOffset, bool isBrokenOffPart)
{

// 	rmcDrawable* pDrawable = NULL;
// 	if (GetIsHighDetail()){
// 		CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(m_pBaseModelInfo);
// 		if (pVMI->GetHDFragType()){
// 			pDrawable = pVMI->GetHDFragType()->GetCommonDrawable();
// 		} else {
// 			return;
// 		}
// 	} else {
// 		Assert(m_pBaseModelInfo);
// 		pDrawable = m_pBaseModelInfo->GetDrawable();
// 		Assert(pDrawable);
// 	}

	if(!pDrawable)
		return;

#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	CCustomShaderEffectVehicleType *pType = GetCseTypeInternal();

	grmShaderGroup* pShaderGroup = &pDrawable->GetShaderGroup();

	// setup shaders for vehicle deformation
	// Do this before the early out for the shadow pass so its setup correctly for shadows.
#if USE_GPU_VEHICLEDEFORMATION
	if(pType->m_idVarDamageSwitchOn)
	{
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarDamageSwitchOn, GTASHADERVARBOOL(m_bDamageSwitchOn));

		// no need to set other vars if damage isn't switched on
		if(m_bDamageSwitchOn)
		{
			if(m_pDamageTexture && pType->m_idVarDamageTex && pType->m_idVarBoundRadius && pType->m_idVarDamageMult && pType->m_idVarDamagedFrontWheelOffsets && pType->m_idVarOffset BANK_ONLY( && pType->m_idVarDebugDamageMap && pType->m_idVarDebugDamageMult))
			{
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDamageTex,	m_pDamageTexture);
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarBoundRadius,	m_fBoundRadius);
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDamageMult,	m_fDamageMultiplier);
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDamagedFrontWheelOffsets, m_vDamagedFrontWheelOffsets, 2);
#if __BANK
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDebugDamageMap, CVehicleDeformation::ms_bDisplayDamageMap);
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDebugDamageMult, CVehicleDeformation::ms_bDisplayDamageMult);
#endif

			#if __BANK
 				boneOffset += Vector3(CVehicleDeformation::m_DamageTextureOffset_X, CVehicleDeformation::m_DamageTextureOffset_Y, CVehicleDeformation::m_DamageTextureOffset_Z);
			#endif
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarOffset,		boneOffset);
			}
			else
			{	// disable damage if invalid settings
				grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarDamageSwitchOn, false);

				if(pType->m_idVarDamageTex)
				{
					pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDamageTex,	const_cast<grcTexture*>(grcTexture::NoneBlack));
				}
			}
		}
		else
		{
			if(pType->m_idVarDamageTex)
			{
				pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDamageTex, m_pDamageTexture ? m_pDamageTexture : const_cast<grcTexture*>(grcTexture::NoneBlack));
			}
		}
	}
	else
	{
		if(pType->m_idVarDamageTex)
		{
			pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDamageTex, m_pDamageTexture ? m_pDamageTexture : const_cast<grcTexture*>(grcTexture::NoneBlack));
		}
	}
#endif //USE_GPU_VEHICLEDEFORMATION...

	// tyre deform on/off control:
	if(pType->m_idVarTyreDeformSwitchOn)
	{
		grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarTyreDeformSwitchOn, GTASHADERVARBOOL(m_bTyreDeformSwitchOn));
		
		#if CSE_VEHICLE_EDITABLEVALUES
			// editable inner wheel radius stuff:
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarTyreDeformSwitchOn, GTASHADERVARBOOL(m_bTyreDeformSwitchOn || GetEWREnabled()));
		#endif
	}

	if(grmModel::GetForceShader())
		return;

	const u32 dirtTexCount = pType->m_DirtTexEffect.GetCount();
	if(m_bRenderScorched)
	{
		// Dirt texture (replaced with burnout when vehicle scorched):
		for(u32 i=0; i<dirtTexCount; i++)
		{
			CCustomShaderEffectVehicleType::structDirtTexEffect *ptr = &pType->m_DirtTexEffect[i];

			Assert(!GetCseType()->m_bHasTruckTex || CCustomShaderEffectVehicleType::ms_pBurnoutTruckTexture);	// truck expects a valid burnout texture

			grcTexture *burnoutTex = GetCseType()->m_bHasTruckTex? CCustomShaderEffectVehicleType::ms_pBurnoutTruckTexture : CCustomShaderEffectVehicleType::ms_pBurnoutTexture;
			burnoutTex = ptr->bIsInterior? CCustomShaderEffectVehicleType::ms_pBurnoutIntTexture : burnoutTex;
			pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDirtTex, (grcTexture*)burnoutTex);
		}
		
		// burnout vehicle: full dirt level + darker dirt + dirtOrBurnout=0 (nonDirtSpecIntScale (for non dirt shaders) to 0)
		if(pType->m_idVarDirtLevelMod)
			pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDirtLevelMod, Vector4(GetBurnoutLevel(), 0.25f, /*dirtOrBurnout*/0.0f, 0.10f));
	}
	else
	{	// restore original dirt texture:
		for(u32 i=0; i<dirtTexCount; i++)
		{
			CCustomShaderEffectVehicleType::structDirtTexEffect *ptr = &pType->m_DirtTexEffect[i];
			pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDirtTex, (grcTexture*)ptr->pOrigDirtTex);
		}

		const float dirtModulator = rage::Lerp(rage::Clamp(g_weather.GetWetness(),0.0f,1.0f), 1.0f, DEFAULT_DIRTMODWET);
		if(pType->m_idVarDirtLevelMod)
			pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDirtLevelMod, Vector4(this->GetDirtLevel(), dirtModulator, /*dirtOrBurnout*/1.0f, 1.0f));
	}

	if(DRAWLISTMGR->IsExecutingShadowDrawList())
		return;

	if(pType->m_idVarDirtColor)
	{
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDirtColor, this->GetDirtColor());
	}

	if(pType->m_idVarSpec2Color)
	{
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarSpec2Color, this->GetSpec2Color());	// rgb=spec2Color, a=spec2DirLerp
	}

#if USE_DISKBRAKEGLOW
	if( pType->m_idVarDiskBrakeGlow )
	{
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiskBrakeGlow,m_varDiskBrakeGlow);
	}
#endif

	// setup shader for headlights
	if(pType->m_idVarDimmer)
	{
		float lightValues[4*CVehicleLightSwitch::LW_LIGHTVECTORS] = {0.0f};

		const float dayNightFade = CVehicle::GetDayNightFade();

		for (u32 i=0; i < CVehicleLightSwitch::LW_LIGHTCOUNT; ++i)
		{
			lightValues[i] = isBrokenOffPart ? CVehicle::GetLightOffValue(i, dayNightFade) : m_fLightDimmer[i];
		}
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDimmer, (Vector4*)lightValues, CVehicleLightSwitch::LW_LIGHTVECTORS);
	}

	// SpecularColor:
	const s32 specCount = pType->m_SpecColEffect.GetCount();

	if(false && m_bRenderScorched)
	{
		static dev_bool bEnableSpecularBurnout=true;
		for(s32 i=0; i<specCount; i++)
		{
			CCustomShaderEffectVehicleType::structSpecColEffect *ptr = &pType->m_SpecColEffect[i];
			if(bEnableSpecularBurnout)
			{
				if(ptr->idvarSpecInt)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecInt, float(ptr->origSpecInt));

				if(ptr->idvarSpecFalloff)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFalloff, float(ptr->origSpecFalloff));

				if(ptr->idvarSpecFresnel)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFresnel, float(ptr->origSpecFresnel));
			}
			else
			{
				if(ptr->idvarSpecInt)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecInt, 0.0f);
			}
		}
	}
	else
	{
		for(s32 i=0; i<specCount; i++)
		{
			CCustomShaderEffectVehicleType::structSpecColEffect *ptr = &pType->m_SpecColEffect[i];
			
			if((ptr->colorIdx != 255) && (m_varMetallicID[ptr->colorIdx] != (u8)CVehicleModelColor::EVehicleModelColorMetallic_none))
			{	// setup metallic settings:
				CVehicleMetallicSetting *metallicSet = CVehicleModelInfo::GetVehicleColours()->GetMetallicSetting( m_varMetallicID[ptr->colorIdx] );		
				Assert(metallicSet);

				if(ptr->idvarSpecInt)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecInt, metallicSet->m_specInt);

				if(ptr->idvarSpecFalloff)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFalloff, metallicSet->m_specFalloff);

				if(ptr->idvarSpecFresnel)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFresnel, metallicSet->m_specFresnel);
			}
			else
			{	// restore original specular colors:
				if(ptr->idvarSpecInt)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecInt, float(ptr->origSpecInt));

				if(ptr->idvarSpecFalloff)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFalloff, float(ptr->origSpecFalloff));

				if(ptr->idvarSpecFresnel)
					pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecFresnel, float(ptr->origSpecFresnel));
			}
			
		}
	}

	// overlay color:
	if(pType->m_idVarDiffuse2Color)
	{
		if(m_bRenderScorched)
			pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiffuse2Color, Vector4(0.75f, 0.75f, 0.75f, 1.0f));
		else
			pShaderGroup->SetVarByRef((grmShaderGroupVar)pType->m_idVarDiffuse2Color, VECTOR4_IDENTITY);
	}

	// diffuse tint color (used mainly for vehicle glass):
	if(pType->m_idVarDiffuseColorTint)
	{
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiffuseColorTint, m_varDiffColorTint);
	}

	if(pType->m_idVarDiffuseTex2)
	{
		if (m_bHasLiveryMod) 
		{
			grcTexture *pLiveryModTex = (grcTexture*)grcTexture::NoneBlackTransparent;

			if (m_liveryFragSlotIdx != strLocalIndex::INVALID_INDEX)
			{
				if (g_FragmentStore.GetSlot(m_liveryFragSlotIdx)->m_pObject != NULL)
				{
					grmShaderGroup *pFragShaderGroup = g_FragmentStore.Get(m_liveryFragSlotIdx)->GetCommonDrawable()->GetShaderGroupPtr();
					fwTxd const* pTxd = pFragShaderGroup ? pFragShaderGroup->GetTexDict() : NULL; // frag's internal txd

					Assertf(pTxd,"livery mod %s, does not have a .txd embedded in the mod frag", g_FragmentStore.GetName(m_liveryFragSlotIdx)); 
					if (pTxd && pTxd->GetCount() > 0)
					{
						pLiveryModTex = pTxd->GetEntry(0);
					}
				}
			}
			pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex2, pLiveryModTex);
		}
		else if(m_pLiveryTexture)
		{
			pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex2, m_pLiveryTexture);
		}
		else
		{
			// restore original overlay texture(s):
			const u32 tex2Count = pType->m_Tex2Effect.GetCount();
			for(u32 i=0; i<tex2Count; i++)
			{
				CCustomShaderEffectVehicleType::structTex2Effect *ptr = &pType->m_Tex2Effect[i];
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarTex2, (grcTexture*)ptr->pOrigTex2);
			}
		}
	}

	if(pType->m_idVarDiffuseTex3 && m_pLivery2Texture)
	{
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex3, m_pLivery2Texture);
	}

	const u32 count = pType->m_DiffColEffect.GetCount();
	#if __ASSERT
		Assertf(count > 0, "BS#124925: DiffColEffect array is empty! Andrzej would love to see this assert.");
	#endif
	if(count)
	{
		for(u32 i=0; i<count; i++)
		{
			CCustomShaderEffectVehicleType::structDiffColEffect *ptr = &pType->m_DiffColEffect[i];
#if __DEV
			if(ms_nForceColorCars==0)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVarByRef((grcEffectVar)ptr->idvarDiffColor, ORIGIN);
			}
			else if(ms_nForceColorCars==1)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVarByRef((grcEffectVar)ptr->idvarDiffColor, VEC3_IDENTITY);
			}
			else
#endif // __DEV
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffColor, m_varDifCol[ptr->colorIdx]);
			}

#if VEHICLE_SUPPORT_PAINT_RAMP
			grcTexture* diffuseRampTex = 0;
			grcTexture* specularRampTex = 0;
			if(ptr->colorIdx < NELEM(m_pRampTextures))
			{
				diffuseRampTex = m_pRampTextures[ptr->colorIdx];
				specularRampTex = m_pRampTextures[2]; // Albert: 2=BodyColor3=Specular
#if __BANK
				if(s_DebugRampTextureEnabled)
				{
					diffuseRampTex = s_DebugRampTexture;
					specularRampTex = s_DebugRampTexture;
				}

				if(!s_RampDiffuseEnabled)
				{
					diffuseRampTex = 0;
				}
				if(!s_RampSpecularEnabled)
				{
					specularRampTex = 0;
				}
#endif
			}
#if __DEV
			if(ms_nForceColorCars==0 || ms_nForceColorCars==1)
			{
				diffuseRampTex = 0;
				specularRampTex = 0;
			}
#endif

			if (ptr->idvarDiffuseRampTexture)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffuseRampTexture, diffuseRampTex ? diffuseRampTex : const_cast<grcTexture*>(grcTexture::NoneBlack));
			}

			if (ptr->idvarSpecularRampTexture)
			{
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarSpecularRampTexture, specularRampTex ? specularRampTex : const_cast<grcTexture*>(grcTexture::NoneBlack));
			}

			if (ptr->idvarDiffuseSpecularRampEnabled)
			{
				float rampEnabled = specularRampTex ? 2.0f : 1.0f;
				rampEnabled = diffuseRampTex ? rampEnabled : -rampEnabled;
				pShaderGroup->GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffuseSpecularRampEnabled, rampEnabled);
			}
#endif
		}
	}

	// track UV anim for tank:
	if(pType->m_idVarTrackAnimUV)
	{
		Vector4 uv4(m_uvTrackAnim.x, m_uvTrackAnim.y, 0.0f, 0.0f);
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarTrackAnimUV, uv4);
	}

	// track UV anim for tank:
	if(pType->m_idVarTrack2AnimUV)
	{
		Vector4 uv4(m_uvTrack2Anim.x, m_uvTrack2Anim.y, 0.0f, 0.0f);
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarTrack2AnimUV, uv4);
	}

	// track UV anim for ammo:
	if(pType->m_idVarTrackAmmoAnimUV)
	{
		Vector4 uv4(m_uvTrackAmmoAnim.x, m_uvTrackAmmoAnim.y, 0.0f, 0.0f);
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarTrackAmmoAnimUV, uv4);
	}

	// emissive control:
	if(pType->m_idVarEmissiveMult)
	{
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarEmissiveMult, m_fUserEmissiveMultiplier * pType->m_DefEmissiveMult);
	}

	// enveff scale:
	if(pType->m_idVarEnvEffScale)
	{
		float envEffScale = GetEnvEffScale();
		Vector2 envEffScaleV2(envEffScale, envEffScale/1000.0f);
		pShaderGroup->SetVar((grmShaderGroupVar)pType->m_idVarEnvEffScale, envEffScaleV2);
	}

	//Set the license plate shader vars
	const CVehicleModelInfoPlates *plates = &CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData();
	if(plates && m_nLicensePlateTexIdx >= 0)
	{
		//Find the right textures to set to license plate.
		const CVehicleModelInfoPlates::TextureArray &textures = plates->GetTextureArray();

		if(m_pLicensePlateDiffuseTex && pType->m_idLicensePlateBgTex != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateBgTex), m_pLicensePlateDiffuseTex);

		if(m_pLicensePlateNormalTex && pType->m_idLicensePlateBgNormTex != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateBgNormTex), m_pLicensePlateNormalTex);

		if(pType->m_idLicensePlateFontColor != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateFontColor), textures[m_nLicensePlateTexIdx].GetFontColor());

		if(pType->m_idLicensePlateText1 != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateText1), Vector4(m_LicensePlateText[0],m_LicensePlateText[1],m_LicensePlateText[2],m_LicensePlateText[3]));

		if(pType->m_idLicensePlateText2 != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateText2), Vector4(m_LicensePlateText[4],m_LicensePlateText[5],m_LicensePlateText[6],m_LicensePlateText[7]));

		if(pType->m_idLicensePlateFontOutlineColor != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateFontOutlineColor), textures[m_nLicensePlateTexIdx].GetFontOutlineColor());

		if(pType->m_idLicensePlateFontExtents != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateFontExtents), textures[m_nLicensePlateTexIdx].GetFontExtents());

		if(pType->m_idLicensePlateFontOutlineMinMaxDepth_Enabled != grmsgvNONE)
		{
			const Vector2 &outlineMinMaxDepth = textures[m_nLicensePlateTexIdx].GetFontOutlineMinMaxDepth();
			Vector4 enabled(outlineMinMaxDepth.x, outlineMinMaxDepth.y, (textures[m_nLicensePlateTexIdx].IsFontOutlineEnabled() ? 1.0f : 0.0f), 0.0f);
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateFontOutlineMinMaxDepth_Enabled), enabled);
		}

		if(pType->m_idLicensePlateMaxLettersOnPlate != grmsgvNONE)
			pShaderGroup->SetVar(static_cast<grmShaderGroupVar>(pType->m_idLicensePlateMaxLettersOnPlate), textures[m_nLicensePlateTexIdx].GetMaxLettersOnPlate());
	}

	#if CSE_VEHICLE_EDITABLEVALUES
		this->SetEditableShaderValues(pShaderGroup, pDrawable);
	#endif

}// end of SetShaderVariables()...

//
//
// this is called from CVehicleModelInfo::SetupCommonData():
//

void CCustomShaderEffectVehicle::SetupBurnoutTexture(s32 txdSlot)
{
	if (txdSlot == -1)
	{
		CCustomShaderEffectVehicleType::ms_pBurnoutTexture = NULL;
		CCustomShaderEffectVehicleType::ms_pBurnoutIntTexture = NULL;
	}
	else
	{
		const fwTxd *burnoutTxd = g_TxdStore.Get(strLocalIndex(txdSlot));

		if (Verifyf(burnoutTxd, "vehicle burnout txd slot %d (%s) was NULL", txdSlot, g_TxdStore.GetName(strLocalIndex(txdSlot))))
		{
			grcTexture *burnout = burnoutTxd->Lookup(VEHICLE_GENERIC_BURNOUT_TEXNAME);
			CCustomShaderEffectVehicleType::ms_pBurnoutTexture = burnout;
			Assert(CCustomShaderEffectVehicleType::ms_pBurnoutTexture);

			grcTexture *burnoutInt = burnoutTxd->Lookup(VEHICLE_GENERIC_BURNOUT_INT_TEXNAME);
			CCustomShaderEffectVehicleType::ms_pBurnoutIntTexture = burnoutInt;
			Assert(CCustomShaderEffectVehicleType::ms_pBurnoutIntTexture);
		}
	}
}// end of SetupBurnoutTexture()...




//
//
//
//
void CCustomShaderEffectVehicle::SelectLicensePlateByProbability(CEntity *pEntity)
{
	if(Verifyf(pEntity, "Null entity passed in to SelectLicensePlateByProbability()!"))
	{
		int defaultTexIdx = CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData().GetDefaultTextureIndex();

		CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
		CVehicleModelInfo *modelInfo = pVehicle->GetVehicleModelInfo();
		if(Verifyf(modelInfo, "Got invalid model info! (pEntity = 0x%p | modelIndex = %d)", pEntity, pEntity->GetModelIndex()) && modelInfo->GetModelType() == MI_TYPE_VEHICLE)
		{
			defaultTexIdx = modelInfo->SelectLicensePlateTextureIndex();
		}

		SetLicensePlateTexIndex(defaultTexIdx);
	}
}

void CCustomShaderEffectVehicle::SetLicensePlateText(const char *str)
{
	//Convert text to char array indices. 
	const CVehicleModelInfoPlates *plates = &CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData();
	u8 *strIndices = m_LicensePlateText;
	bool endOfStr = false;

	// B*1325679 - Can you add the required number of spaces when the name is shorter than the 8 characters it is. 
	// 8 chars max, not zero terminated if max?
	int length;
	for(length=0; length<LICENCE_PLATE_LETTERS_MAX; length++)
	{
		if( str[length] == 0 )
			break;
	}
	int lead = (LICENCE_PLATE_LETTERS_MAX - length)>>1;

	for(u32 i=0; i<LICENCE_PLATE_LETTERS_MAX; i++)
	{
		endOfStr = endOfStr || i>=(length + lead);
		if(endOfStr || i<lead)
		{
			strIndices[i] = plates->GetFontSpaceOffset();
		}
		else
		{
			char c = str[i - lead];

			if(c >= '0' && c <= '9')
			{
				strIndices[i] = plates->GetFontNumericOffset() + (c - '0');
			}
			else if(c >= 'A' && c <= 'Z')
			{
				strIndices[i] = plates->GetFontAlphabeticOffset() + (c - 'A');
			}
			else if(c >= 'a' && c <= 'z')
			{
				strIndices[i] = plates->GetFontAlphabeticOffset() + (c - 'a');
			}
			else if(c == ' ')
			{
				strIndices[i] = plates->GetFontSpaceOffset();
			}
			else
			{
				//All the rest go here...
				strIndices[i] = plates->GetFontRandomCharOffset() + (c % plates->GetFontNumRandomChar());
			}
		}

		//Make sure the index is with-in the proper range
		#if __ASSERT
			static u8 sMaxGlyphs = 64;
			Assertf(strIndices[i] < sMaxGlyphs, "Invalid font glyph index value: %i", static_cast<int>(strIndices[i]));
		#endif
	}
}

static char sDecodedLicenseText[ CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX+1 ];
CompileTimeAssert(sizeof(sDecodedLicenseText) > CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX);

const char* CCustomShaderEffectVehicle::GetLicensePlateText() const
{
	const CVehicleModelInfoPlates *plates = &CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData();

	int	inIDX = 0;
	int	outIDX = 0;
	while(inIDX < LICENCE_PLATE_LETTERS_MAX)
	{
		if (m_LicensePlateText[inIDX] >= plates->GetFontRandomCharOffset())
		{
			// we can't convert back the random characters unfortunately
			sDecodedLicenseText[outIDX++] = ' ';
		}
		else if (m_LicensePlateText[inIDX] >= plates->GetFontAlphabeticOffset())
		{
			sDecodedLicenseText[outIDX++] = 'A' + (m_LicensePlateText[inIDX] - plates->GetFontAlphabeticOffset());
		}
		else if (m_LicensePlateText[inIDX] >= plates->GetFontNumericOffset())
		{
			sDecodedLicenseText[outIDX++] = '0' + (m_LicensePlateText[inIDX] - plates->GetFontNumericOffset());
		}
		else if (m_LicensePlateText[inIDX] == 0)
		{
			sDecodedLicenseText[outIDX++] = 0;
			break;
		}
		inIDX++;
	}
	for(int i = outIDX; i<=LICENCE_PLATE_LETTERS_MAX; i++)
	{
		sDecodedLicenseText[i] = 0;
	}

	return sDecodedLicenseText;
}

void CCustomShaderEffectVehicle::GenerateLicensePlateText(u32 seed)
{
	// California legal is 1ABC234 since early 80s
	// We'll use 12ABC345
	mthRandom rnd(seed);
	
	char tag[9];

	tag[0] = static_cast<char>(rnd.GetRanged('0','9'));
	tag[1] = static_cast<char>(rnd.GetRanged('0','9'));
	tag[2]	= static_cast<char>(rnd.GetRanged('A','Z'));
	tag[3]	= static_cast<char>(rnd.GetRanged('A','Z'));
	tag[4]	= static_cast<char>(rnd.GetRanged('A','Z'));
	tag[5] = static_cast<char>(rnd.GetRanged('0','9'));
	tag[6] = static_cast<char>(rnd.GetRanged('0','9'));
	tag[7] = static_cast<char>(rnd.GetRanged('0','9'));
	tag[8] = '\0';

	SetLicensePlateText(tag);
}

void CCustomShaderEffectVehicle::SetLicensePlateTexIndex(s32 index)
{
	const CVehicleModelInfoPlates *plates = &CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData();
	if(Verifyf(index >= -1 && index < plates->GetTextureArray().size(),
		"Invalid index license plate texture index requested: %d. Max number of known plate textures = %d",
		index, plates->GetTextureArray().size() ))
	{
		if(m_nLicensePlateTexIdx != index)
		{
			//Update the license plate index
			m_nLicensePlateTexIdx = index;

			if(m_nLicensePlateTexIdx >= 0)
			{
				//Find the right textures to set to license plate.
				const CVehicleModelInfoPlates::TextureArray &textures = plates->GetTextureArray();
				u32 diffuseHash = textures[m_nLicensePlateTexIdx].GetDiffuseMapStrHash().GetHash();
				u32 normalHash = textures[m_nLicensePlateTexIdx].GetNormalMapStrHash().GetHash();

				grcTexture *diffuseMap = NULL;
				grcTexture *normalMap = NULL;
				strLocalIndex txdIdx = strLocalIndex(CVehicleModelInfo::GetCommonTxd());
				fwTxd *txd = g_TxdStore.Get(txdIdx);

				if(Verifyf(txd, "Encountered a NULL texture dictionary while trying to iterate the loaded global vehicle texture dictionaries!"))
				{
					if(!diffuseMap)
						diffuseMap = txd->Lookup(diffuseHash);

					if(!normalMap)
						normalMap = txd->Lookup(normalHash);
				}

				//Update our texture pointers to point to new license plate background textures.
				Assertf(diffuseMap, "SetLicensePlateTexIndex was unable to find the requested diffuse texture (%s <hash=0x%x>) for license plate background texture set index %d.",
					(textures[m_nLicensePlateTexIdx].GetDiffuseMapStrHash().GetCStr() ? textures[m_nLicensePlateTexIdx].GetDiffuseMapStrHash().GetCStr() : "<Unknown Hash>"),
					textures[m_nLicensePlateTexIdx].GetDiffuseMapStrHash().GetHash(), m_nLicensePlateTexIdx);
				Assertf(normalMap, "SetLicensePlateTexIndex was unable to find the requested normal texture (%s <hash=0x%x>) for license plate background texture set index %d.",
					(textures[m_nLicensePlateTexIdx].GetNormalMapStrHash().GetCStr() ? textures[m_nLicensePlateTexIdx].GetNormalMapStrHash().GetCStr() : "<Unknown Hash>"),
					textures[m_nLicensePlateTexIdx].GetNormalMapStrHash().GetHash(), m_nLicensePlateTexIdx);

				m_pLicensePlateDiffuseTex = diffuseMap;
				m_pLicensePlateNormalTex = normalMap;
			}
		}
	}
}


inline float wrapOverOne(float a)
{
	if(a > 1.0f)
	{
		float ipart;
		a = rage::Modf(a, &ipart);
	}
	return(a);
}

inline float wrapBelowZero(float a)
{
	if(a < 0.0f)
	{
		float ipart;
		a = rage::Modf(a, &ipart);
		a += 1.0f;
	}
	return(a);
}

//
// tanks tracks:
// adds given amount of U shift to overall UV anim:
//
void CCustomShaderEffectVehicle::TrackUVAnimAdd(const Vector2& a)
{
	this->m_uvTrackAnim.x += a.x;	// right shift
	this->m_uvTrackAnim.y += a.y;	// left shift

	// wrap to <0; 1.0f>
	m_uvTrackAnim.x = wrapOverOne(m_uvTrackAnim.x);
	m_uvTrackAnim.y = wrapOverOne(m_uvTrackAnim.y);
	m_uvTrackAnim.x = wrapBelowZero(m_uvTrackAnim.x);
	m_uvTrackAnim.y = wrapBelowZero(m_uvTrackAnim.y);
}

void CCustomShaderEffectVehicle::Track2UVAnimAdd(const Vector2& a)
{
	this->m_uvTrack2Anim.x += a.x;	// right shift
	this->m_uvTrack2Anim.y += a.y;	// left shift

	// wrap to <0; 1.0f>
	m_uvTrack2Anim.x = wrapOverOne(m_uvTrack2Anim.x);
	m_uvTrack2Anim.y = wrapOverOne(m_uvTrack2Anim.y);
	m_uvTrack2Anim.x = wrapBelowZero(m_uvTrack2Anim.x);
	m_uvTrack2Anim.y = wrapBelowZero(m_uvTrack2Anim.y);
}

void CCustomShaderEffectVehicle::TrackAmmoUVAnimAdd(const Vector2& a)
{
	this->m_uvTrackAmmoAnim.x += a.x;	// right shift
	this->m_uvTrackAmmoAnim.y += a.y;	// left shift

	// wrap to <0; 1.0f>
	m_uvTrackAmmoAnim.x = wrapOverOne(m_uvTrackAmmoAnim.x);
	m_uvTrackAmmoAnim.y = wrapOverOne(m_uvTrackAmmoAnim.y);
	m_uvTrackAmmoAnim.x = wrapBelowZero(m_uvTrackAmmoAnim.x);
	m_uvTrackAmmoAnim.y = wrapBelowZero(m_uvTrackAmmoAnim.y);
}

const u32 g_vehicleGlassVarHashDiffuseTex               = ATSTRINGHASH("DiffuseTex", 0xC8E8C282);
const u32 g_vehicleGlassVarHashDirtTex                  = ATSTRINGHASH("DirtTex", 0xA6F4CF58);
const u32 g_vehicleGlassVarHashSpecularTex              = ATSTRINGHASH("SpecularTex", 0xC6F46E79);
const u32 g_vehicleGlassVarHashSpecularMapIntensityMask = ATSTRINGHASH("SpecularMapIntensityMask", 0x92DB11FB);
const u32 g_vehicleGlassVarHashEnvEffTexTileUV          = ATSTRINGHASH("envEffTexTileUV", 0xDAC8F5E7);
const u32 g_vehicleGlassVarHashEnvEffThickness          = ATSTRINGHASH("envEffThickness", 0xA114F369);

const u32 g_vehicleGlassVarHashDirtLevelMod             = ATSTRINGHASH("DirtLevelMod", 0xEC247F19);
const u32 g_vehicleGlassVarHashDirtColor                = ATSTRINGHASH("DirtColor", 0x44546346);
const u32 g_vehicleGlassVarHashDiffuseColorTint         = ATSTRINGHASH("DiffuseColorTint", 0xE297F9DA);
const u32 g_vehicleGlassVarHashEnvEffScale0             = ATSTRINGHASH("envEffScale0", 0xC788FA2A);

void CVehicleGlassShaderData::GetStandardShaderVariables(const grmShader* pShader)
{
	Assertf(strcmp(pShader->GetName(), "vehicle_vehglass") == 0, "CVehicleGlassShaderData::GetStandardShaderVariables called on shader \"%s\"", pShader->GetName());

	const grcEffectVar idVarDiffuseTex               = pShader->LookupVarByHash(g_vehicleGlassVarHashDiffuseTex);
	const grcEffectVar idVarDirtTex                  = pShader->LookupVarByHash(g_vehicleGlassVarHashDirtTex);
	const grcEffectVar idVarSpecularTex              = pShader->LookupVarByHash(g_vehicleGlassVarHashSpecularTex);
	const grcEffectVar idVarSpecularMapIntensityMask = pShader->LookupVarByHash(g_vehicleGlassVarHashSpecularMapIntensityMask);
	const grcEffectVar idVarEnvEffTexTileUV          = pShader->LookupVarByHash(g_vehicleGlassVarHashEnvEffTexTileUV);
	const grcEffectVar idVarEnvEffThickness          = pShader->LookupVarByHash(g_vehicleGlassVarHashEnvEffThickness);

	const grcEffectVar idVarDirtLevelMod             = pShader->LookupVarByHash(g_vehicleGlassVarHashDirtLevelMod);
	const grcEffectVar idVarDirtColor                = pShader->LookupVarByHash(g_vehicleGlassVarHashDirtColor);
	const grcEffectVar idVarDiffuseColorTint         = pShader->LookupVarByHash(g_vehicleGlassVarHashDiffuseColorTint);
	const grcEffectVar idVarEnvEffScale0             = pShader->LookupVarByHash(g_vehicleGlassVarHashEnvEffScale0);

	grcTexture* pDiffuseTex  = NULL;
	grcTexture* pDirtTex     = NULL;
	grcTexture* pSpecularTex = NULL;

	pShader->GetVar(idVarDiffuseTex , pDiffuseTex);
	pShader->GetVar(idVarDirtTex    , pDirtTex);
	pShader->GetVar(idVarSpecularTex, pSpecularTex);

	m_DiffuseTex  = pDiffuseTex;
	m_DirtTex     = pDirtTex;
	m_SpecularTex = pSpecularTex;

	pShader->GetVar(idVarSpecularMapIntensityMask, m_SpecularMapIntensityMask);
	pShader->GetVar(idVarEnvEffTexTileUV         , m_EnvEffTexTileUV);
	pShader->GetVar(idVarEnvEffThickness         , m_EnvEffThickness);

	pShader->GetVar(idVarDirtLevelMod            , m_DirtLevelMod);
	pShader->GetVar(idVarDirtColor               , m_DirtColor);
	pShader->GetVar(idVarDiffuseColorTint        , m_DiffuseColorTint);
	pShader->GetVar(idVarEnvEffScale0            , m_EnvEffScale0);

#if USE_GPU_VEHICLEDEFORMATION
	m_bDamageOn = false;
	m_pDamageTex = NULL; // Set this to NULL in case we can't get the damage texture from the custom shader
#endif // USE_GPU_VEHICLEDEFORMATION
}

void CVehicleGlassShaderData::GetCustomShaderVariables(const CCustomShaderEffectVehicle* pCSE)
{
	m_DirtLevelMod		= pCSE->m_bRenderScorched ? Vector4(1.0f, 0.25f, 0.0f, 0.0f) : Vector4(pCSE->GetDirtLevel(), 1.0f, 1.0f, 1.0f);
	m_DirtColor			= VEC4V_TO_VECTOR4(pCSE->GetDirtColor().GetRGBA());
	m_DiffuseColorTint	= VEC4V_TO_VECTOR4(pCSE->GetDiffuseTint().GetRGBA());
	m_EnvEffScale0		= Vector2(pCSE->GetEnvEffScale(), pCSE->GetEnvEffScale()/1000.0f);

#if CSE_VEHICLE_EDITABLEVALUES
	// Overwrite the shader value with the override value
	if (CCustomShaderEffectVehicle::GetEVOverrideDiffuseColorTint())
	{
		CCustomShaderEffectVehicle::GetEVDiffuseColorTint(m_DiffuseColorTint);
	}
#endif // CSE_VEHICLE_EDITABLEVALUES
}

void CVehicleGlassShaderData::GetDirtTexture(const CCustomShaderEffectVehicle* pCSE, int shaderIndex ASSERT_ONLY(, const char* vehicleModelName))
{
	const CCustomShaderEffectVehicleType* pType = pCSE->GetCseType();

	m_DirtTex = pType->GetDirtTexture(shaderIndex);

	Assertf(m_DirtTex, "%s: m_DirtTex is NULL", vehicleModelName);
}

#if USE_GPU_VEHICLEDEFORMATION
void CVehicleGlassShaderData::GetDamageShaderVariables(const CCustomShaderEffectVehicle* pCSE)
{
	m_bDamageOn = pCSE->GetEnableDamage();
	if(m_bDamageOn)
	{
		m_pDamageTex = pCSE->GetDamageTex();
		m_fBoundRadius = pCSE->GetBoundRadius();
		m_fDamageMultiplier	= pCSE->GetDamageMultiplier();
	}
	else
	{
		m_pDamageTex = NULL;
	}
}
#endif // USE_GPU_VEHICLEDEFORMATION

void CVehicleGlassShader::Create(grmShader* pShader)
{
	Assertf(strcmp(pShader->GetName(), "vehicle_vehglass_crack") == 0, "CVehicleGlassShader::Create called on shader \"%s\"", pShader->GetName());

	m_shader = pShader;

	m_idVarDiffuseTex               = pShader->LookupVarByHash(g_vehicleGlassVarHashDiffuseTex);
	m_idVarDirtTex                  = pShader->LookupVarByHash(g_vehicleGlassVarHashDirtTex);
	m_idVarSpecularTex              = pShader->LookupVarByHash(g_vehicleGlassVarHashSpecularTex);
	m_idVarSpecularMapIntensityMask = pShader->LookupVarByHash(g_vehicleGlassVarHashSpecularMapIntensityMask);
	m_idVarEnvEffTexTileUV          = pShader->LookupVarByHash(g_vehicleGlassVarHashEnvEffTexTileUV);
	m_idVarEnvEffThickness          = pShader->LookupVarByHash(g_vehicleGlassVarHashEnvEffThickness);

	m_idVarDirtLevelMod             = pShader->LookupVarByHash(g_vehicleGlassVarHashDirtLevelMod);
	m_idVarDirtColor                = pShader->LookupVarByHash(g_vehicleGlassVarHashDirtColor);
	m_idVarDiffuseColorTint         = pShader->LookupVarByHash(g_vehicleGlassVarHashDiffuseColorTint);
	m_idVarEnvEffScale0             = pShader->LookupVarByHash(g_vehicleGlassVarHashEnvEffScale0);

	m_idVarCrackTexture             = pShader->LookupVar("vehglassCrackTexture");
	m_idVarCrackTextureParams       = pShader->LookupVar("vehglassCrackTextureParams");

#if USE_GPU_VEHICLEDEFORMATION
	m_idVarDamageSwitchOn = grcEffect::LookupGlobalVar("switchOn");
	m_idVarDamageTex = m_shader->LookupVar("damagetexture");
	m_idVarBoundRadius = m_shader->LookupVar("BoundRadius");
	m_idVarDamageMult = m_shader->LookupVar("DamageMultiplier");
	m_idVarOffset = m_shader->LookupVar("DamageTextureOffset");
#endif // USE_GPU_VEHICLEDEFORMATION
}

void CVehicleGlassShader::Shutdown()
{
	Assertf(m_shader, "Create not called, or Shutdown called twice");
	delete m_shader;
	ASSERT_ONLY(m_shader = NULL;)
}

void CVehicleGlassShader::SetShaderVariables(const CVehicleGlassShaderData& data) const
{
	m_shader->SetVar(m_idVarDiffuseTex              , data.m_DiffuseTex);
	m_shader->SetVar(m_idVarDirtTex                 , data.m_DirtTex);
	m_shader->SetVar(m_idVarSpecularTex             , data.m_SpecularTex);
	m_shader->SetVar(m_idVarSpecularMapIntensityMask, data.m_SpecularMapIntensityMask);
	m_shader->SetVar(m_idVarEnvEffTexTileUV         , data.m_EnvEffTexTileUV);
	m_shader->SetVar(m_idVarEnvEffThickness         , data.m_EnvEffThickness);

	m_shader->SetVar(m_idVarDirtLevelMod            , data.m_DirtLevelMod);
	m_shader->SetVar(m_idVarDirtColor               , data.m_DirtColor);
	m_shader->SetVar(m_idVarDiffuseColorTint        , data.m_DiffuseColorTint);
	m_shader->SetVar(m_idVarEnvEffScale0            , data.m_EnvEffScale0);

#if USE_GPU_VEHICLEDEFORMATION
	if (data.m_bDamageOn && data.m_pDamageTex != NULL)
	{
		grcEffect::SetGlobalVar((grcEffectGlobalVar)m_idVarDamageSwitchOn, true);

		m_shader->SetVar(m_idVarBoundRadius, data.m_fBoundRadius);
		m_shader->SetVar(m_idVarDamageMult, data.m_fDamageMultiplier);
		m_shader->SetVar(m_idVarDamageTex, data.m_pDamageTex);
		m_shader->SetVar(m_idVarOffset, Vector3(0.0, 0.0, 0.0));
	}
	else
	{
		grcEffect::SetGlobalVar((grcEffectGlobalVar)m_idVarDamageSwitchOn, false);
		m_shader->SetVar(m_idVarDamageTex, const_cast<grcTexture*>(grcTexture::NoneBlack));
	}
#endif // USE_GPU_VEHICLEDEFORMATION
}

void CVehicleGlassShader::SetTintVariable(const Vector4& diffuseColorTint) const
{
	m_shader->SetVar(m_idVarDiffuseColorTint, diffuseColorTint);
}

void CVehicleGlassShader::ClearShaderVariables() const
{
	m_shader->SetVar(m_idVarDiffuseTex      , (grcTexture*)NULL);
	m_shader->SetVar(m_idVarDirtTex         , (grcTexture*)NULL);
	m_shader->SetVar(m_idVarSpecularTex     , (grcTexture*)NULL);

#if USE_GPU_VEHICLEDEFORMATION
	grcEffect::SetGlobalVar((grcEffectGlobalVar)m_idVarDamageSwitchOn, false);
	m_shader->SetVar(m_idVarDamageTex       , (grcTexture*)NULL);
#endif // USE_GPU_VEHICLEDEFORMATION
}

void CVehicleGlassShader::SetCrackTextureParams(grcTexture* crackTexture, float amount, float scale, float bumpAmount, float bumpiness, Vec4V_In tint) const
{
	if (m_idVarCrackTexture != grcevNONE)
	{
		m_shader->SetVar(m_idVarCrackTexture, (amount > 0.0f) ? crackTexture : grcTexture::None);
	}

	if (m_idVarCrackTextureParams != grcevNONE)
	{
		const Vec4V params[] =
		{
			Vec4V(amount, scale, bumpAmount, bumpiness),
			tint
		};
		m_shader->SetVar(m_idVarCrackTextureParams, params, NELEM(params));
	}
}

#if CSE_VEHICLE_EDITABLEVALUES
//
//
//
//
void CCustomShaderEffectVehicle::SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable)
{
	Assert(pShaderGroup);

	if(ms_bEVSpec2DirLerpOverride)
	{
		Color32 spec2Color = this->GetSpec2Color();
		Vector4 spec2ColorV4(spec2Color.GetRedf(), spec2Color.GetGreenf(), spec2Color.GetBluef(), ms_fEVSpec2DirLerp);
		SetVector4ShaderGroupVar(pShaderGroup,	"Specular2Color",			spec2ColorV4);
	}

	if(!ms_bEVEnabled)
		return;

#if 0
	// old way of doing things:
	if(m_idVarEVSpecFalloff)
		pShaderGroup->SetVar(m_idVarEVSpecFalloff,		ms_fEVSpecFalloff);

	if(m_idVarEVSpecIntensity)
		pShaderGroup->SetVar(m_idVarEVSpecIntensity,	ms_fEVSpecIntensity);

	if(m_idVarEVSpec2Falloff)
		pShaderGroup->SetVar(m_idVarEVSpec2Falloff,		ms_fEVSpec2Falloff);
	// etc., etc.
#endif //#if 0...

	// ... and the new way of setting editable shader variables:
	SetFloatShaderGroupVar(pShaderGroup,	"Fresnel",					ms_fEVSpecFresnel);
	SetFloatShaderGroupVar(pShaderGroup,	"Specular",					ms_fEVSpecFalloff);
	SetFloatShaderGroupVar(pShaderGroup,	"SpecularColor",			ms_fEVSpecIntensity);
	SetFloatShaderGroupVar(pShaderGroup,	"SpecTexTileUV",			ms_fEVSpecTexTileUV);
	SetFloatShaderGroupVar(pShaderGroup,	"Specular2Factor",			ms_fEVSpec2Falloff);
	SetFloatShaderGroupVar(pShaderGroup,	"specular2ColorIntensity",	ms_fEVSpec2Intensity);

	Color32 spec2Color = this->GetSpec2Color();
	Vector4 spec2ColorV4(spec2Color.GetRedf(), spec2Color.GetGreenf(), spec2Color.GetBluef(), ms_fEVSpec2DirLerp);
	SetVector4ShaderGroupVar(pShaderGroup,	"Specular2Color",			spec2ColorV4);

	SetFloatShaderGroupVar(pShaderGroup,	"Reflectivity",				ms_fEVReflectivity);
	SetFloatShaderGroupVar(pShaderGroup,	"Bumpiness",				ms_fEVBumpiness);
	SetFloatShaderGroupVar(pShaderGroup,	"DiffuseTexTileUV",			ms_fEVDiffuseTexTileUV);

	const float dirtModulator = rage::Lerp(rage::Clamp(g_weather.GetWetness(),0.0f,1.0f), ms_fEVDirtModDry, ms_fEVDirtModWet);
	SetVector4ShaderGroupVar(pShaderGroup,	"DirtLevelMod",				Vector4(ms_fEVDirtLevel,dirtModulator,(float)ms_fEVDirtOrBurnout,1.0f));
	SetVector3ShaderGroupVar(pShaderGroup,	"DirtColor",				ms_fEVDirtColor);
	SetFloatShaderGroupVar(pShaderGroup,	"envEffThickness0",			ms_fEVEnvEffThickness);
	SetVector2ShaderGroupVar(pShaderGroup,	"envEffScale0",				Vector2(ms_fEVEnvEffScale, ms_fEVEnvEffScale / 1000.0f));
	SetFloatShaderGroupVar(pShaderGroup,	"envEffTexTileUV",			ms_fEVEnvEffTexTileUV);

	SetVector4ShaderGroupVar(pShaderGroup,	"LetterIndex1",						ms_fEVLetterIndex1);
	SetVector4ShaderGroupVar(pShaderGroup,	"LetterIndex2",						ms_fEVLetterIndex2);
	SetVector2ShaderGroupVar(pShaderGroup,	"LetterSize",						ms_fEVLetterSize);
	SetVector2ShaderGroupVar(pShaderGroup,	"NumLetters",						ms_fEVNumLetters);
	SetVector4ShaderGroupVar(pShaderGroup,	"LicensePlateFontExtents",			ms_fEVLicensePlateFontExtents);
	SetVector4ShaderGroupVar(pShaderGroup,	"LicensePlateFontTint",				ms_fEVLicensePlateFontTint);
	SetFloatShaderGroupVar(pShaderGroup,	"FontNormalScale",					ms_fEVFontNormalScale);
	SetFloatShaderGroupVar(pShaderGroup,	"DistMapCenterVal",					ms_fEVDistMapCenterVal);
	SetVector4ShaderGroupVar(pShaderGroup,	"DistEpsilonScaleMin",				ms_fEVDistEpsilonScaleMin);
	SetVector3ShaderGroupVar(pShaderGroup,	"FontOutlineMinMaxDepthEnabled",	ms_fEVFontOutlineMinMaxDepthEnabled);
	SetVector3ShaderGroupVar(pShaderGroup,	"FontOutlineColor",					ms_fEVFontOutlineColor);

	if(ms_fEVOverrideDiffuseColorTint)
	{
		SetVector4ShaderGroupVar(pShaderGroup, "DiffuseColorTint", Vector4(ms_fEVDiffuseColorTint.x, ms_fEVDiffuseColorTint.y, ms_fEVDiffuseColorTint.z, ms_fEVDiffuseColorTintAlpha) );
	}

	// set body color 0:
	CCustomShaderEffectVehicleType::structDiffColEffect *ptr = &GetCseTypeInternal()->m_DiffColEffect[0];
	if(ms_fEVOverrideBodyColor)
	{
		pDrawable->GetShaderGroup().GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffColor, ms_fEVBodyColor);
	}
	else
	{
		pDrawable->GetShaderGroup().GetShader(ptr->shaderIdx).SetVar((grcEffectVar)ptr->idvarDiffColor, m_varDifCol[ptr->colorIdx]);
	}

}// end of SetEditableShaderValues()...


//
//
//
//
bool CCustomShaderEffectVehicle::InitWidgets(bkBank& bank)
{
	// debug widgets:
	bank.PushGroup("Vehicle Editable Inner Tyre Radius", false);
		bank.AddToggle("Enable",				&ms_bEWREnabled);
		bank.AddToggle("Tyre rendering enable",	&ms_bEWRTyreEnabled);
		bank.AddSlider("Inner radius (divide it by 10 for real value)",	&ms_fEWRInnerRadius,	0.0f, 50.0f, 0.001f);
	bank.PopGroup();

	bank.PushGroup("Vehicle Editable Shaders Values", false);
		bank.AddToggle("Enable",									&ms_bEVEnabled);
		bank.AddSlider("Specular falloff",							&ms_fEVSpecFalloff,		0.0f, 512.0f, 0.1f);
		bank.AddSlider("Specular Fresnel",							&ms_fEVSpecFresnel,		0.0f, 1.0f, 0.01f);
		bank.AddSlider("Specular intensity",						&ms_fEVSpecIntensity,	0.0f, 1.0f, 0.01f);
		bank.AddSlider("Spec Texture TileUV",						&ms_fEVSpecTexTileUV,	1.0f, 32.0f, 0.1f);
		bank.AddSlider("Specular 2 falloff",						&ms_fEVSpec2Falloff,	0.0f, 512.0f, 0.1f);
		bank.AddSlider("Specular 2 intensity",						&ms_fEVSpec2Intensity,	0.0f, 3.0f, 0.1f);
		bank.AddSlider("Specular 2 dir (0=GlobalDirLight, 1=Up)",	&ms_fEVSpec2DirLerp,	0.0f, 1.0f, 0.01f);
		bank.AddSlider("Reflectivity",								&ms_fEVReflectivity,	0, 100.0f, 0.1f);
		bank.AddSlider("Bumpiness",									&ms_fEVBumpiness,		0, 10.0f, 0.01f);

		bank.AddToggle("Override Body Color",			&ms_fEVOverrideBodyColor);
		bank.AddColor("Body Color 0",					&ms_fEVBodyColor);

		bank.AddToggle("Override Diffuse Color Tint",	&ms_fEVOverrideDiffuseColorTint);
		bank.AddColor("Diffuse Tint RGB",				&ms_fEVDiffuseColorTint);
		bank.AddSlider("Diffuse Tint Alpha",			&ms_fEVDiffuseColorTintAlpha,	0.0f, 1.0f, 0.01f);

		bank.AddSlider("Diffuse Texture TileUV",		&ms_fEVDiffuseTexTileUV,	1.0f, 32.0f, 0.1f);
		bank.AddSlider("Dirt Level",					&ms_fEVDirtLevel,			0.0f, 1.0f, 0.1f);
		bank.AddSlider("Dirt Color Intensity (Dry)",	&ms_fEVDirtModDry,			0.0f, 1.0f, 0.1f);
		bank.AddSlider("Dirt Color Intensity (Wet)",	&ms_fEVDirtModWet,			0.0f, 1.0f, 0.1f);
		bank.AddSlider("Mode: Dirt (1) / Burnout (0)",	&ms_fEVDirtOrBurnout,		0, 1, 1);
		bank.AddColor("Dirt Color",						&ms_fEVDirtColor);

		bank.AddSlider("EnvEff: Scale",					&ms_fEVEnvEffScale,		0.0f, 1.0f, 0.1f);
		bank.AddSlider("EnvEff: Thickness",				&ms_fEVEnvEffThickness,	0.0f, 100.0f, 0.1f);
		bank.AddSlider("EnvEff: Texture TileUV",		&ms_fEVEnvEffTexTileUV,	1.0f, 32.0f, 0.1f);

		bank.PushGroup("2nd Spec Direction");
			bank.AddToggle("Override",									&ms_bEVSpec2DirLerpOverride);
			bank.AddSlider("Spec 2 direction (0=GlobalDirLight, 1=Up)",	&ms_fEVSpec2DirLerp,	0.0f, 1.0f, 0.01f);
		bank.PopGroup();

		bank.PushGroup("Plate:", false);
			bank.AddSlider("LetterIndex1",					&ms_fEVLetterIndex1,					0.0f, 255.0f, 1.0f);
			bank.AddSlider("LetterIndex2",					&ms_fEVLetterIndex2,					0.0f, 255.0f, 1.0f);
			bank.AddSlider("LetterSize",					&ms_fEVLetterSize,						0.0f, 1.0f, 0.1f);
			bank.AddSlider("NumLetters",					&ms_fEVNumLetters,						0.0f, 255.0f, 1.0f);
			bank.AddSlider("LicensePlateFontExtents",		&ms_fEVLicensePlateFontExtents,			0.0f, 1.0f,0.1f);
			bank.AddColor( "LicensePlateFontTint",			&ms_fEVLicensePlateFontTint);
			bank.AddSlider("FontNormalScale",				&ms_fEVFontNormalScale,					0.0f, 100.0f, 0.1f);
			bank.AddSlider("DistMapCenterVal",				&ms_fEVDistMapCenterVal,				0.0f, 1.0f, 0.1f);
			bank.AddSlider("DistEpsilonScaleMin",			&ms_fEVDistEpsilonScaleMin,				0.0f, 50.0f, 1.0f);
			bank.AddSlider("FontOutlineMinMaxDepthEnabled",	&ms_fEVFontOutlineMinMaxDepthEnabled,	0.0f, 1.0f, 0.1f);
			bank.AddColor( "FontOutlineColor",				&ms_fEVFontOutlineColor);
		bank.PopGroup();
	bank.PopGroup();

	bank.PushGroup("Vehicle Burnout VFX", false);
		bank.AddSlider("Burnout VFX speed",					&ms_fEVBurnoutSpeed,				1.0f/255.0f, 1.0f, 1.0f/255.0f);
	bank.PopGroup();

	bank.PushGroup("Vehicle Sun Glare Fx", false);
		bank.AddToggle("Enable",				&ms_bEVSunGlareEnabled);
		bank.AddSlider("Fade min distance",		&ms_fEVSunGlareDistMin,				0.0f, 100.0f, 0.1f);
		bank.AddSlider("Fade max distance",		&ms_fEVSunGlareDistMax,				0.0f, 100.0f, 0.1f);
		bank.AddSlider("Sun glare spec power",	&ms_nEVSunGlarePow,					1, 96, 1);
		bank.AddSlider("Sprite size",			&ms_fEVSunGlareSpriteSize,			0.0f, 2.0f, 0.01f);
		bank.AddSlider("Sprite Z shift scale",	&ms_fEVSunGlareSpriteZShiftScale,	0.0f, 1.0f, 0.05f);
		bank.AddSlider("Sprite intensity",		&ms_fEVSunGlareHdrMult,				0.0f, 16.0f, 0.1f);
	bank.PopGroup();

#if VEHICLE_SUPPORT_PAINT_RAMP
	bank.PushGroup("Vehicle Chameleon Paint");
		bank.AddToggle("Enable Debug Ramp", &s_DebugRampTextureEnabled, ReloadDebugRampTexture);
		bank.AddText("Debug Ramp Path", &s_DebugRampTexturePath[0], sizeof(s_DebugRampTexturePath) - 1, false, ReloadDebugRampTexture);
		bank.AddButton("Reload Debug Ramp", ReloadDebugRampTexture);
		bank.AddToggle("Diffuse Ramp Enabled", &s_RampDiffuseEnabled);
		bank.AddToggle("Specular Ramp Enabled", &s_RampSpecularEnabled);
	bank.PopGroup();
#endif

	return(TRUE);
}// end of InitWidgets()...
#endif //CSE_VEHICLE_EDITABLEVALUES...



