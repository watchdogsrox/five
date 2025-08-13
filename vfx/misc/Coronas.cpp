///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Coronas.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Coronas.h"

// Rage header
#include "audioengine/engine.h"
#include "grcore/debugdraw.h"
#include "grcore/quads.h"
#include "grcore/stateblock.h"
#include "grcore/stateblock_internal.h"
#include "grcore/viewport.h"
#include "system/param.h"

// Framework headers
#include "fwmaths/random.h"
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// Game header 
#include "Core/Game.h"
#include "camera/CamInterface.h"
#include "debug/Rendering/DebugLights.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderTargets.h"
#include "Renderer/Sprite2d.h"
#include "Renderer/Util/ShaderUtil.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderLib.h"
#include "timecycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "Renderer/Water.h"

#if __D3D11
#include "grcore/texturefactory_d3d11.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define CORONAS_DEG2RAD(x) (1.74532925199e-2f*(x))

dev_float	CORONAS_UNDERWATER_FADE_DISTANCE				= 10.0f;

dev_float 	CORONAS_PARABOLOID_REFLECTION_INTENSITY			= 1.0f;

dev_float	FLARES_CORONAS_SIZE_RATIO						= 0.3f;
dev_float	FLARES_MIN_ANGLE_DEG_TO_FADE_IN					= 30.0f;
dev_float	FLARES_SCREEN_CENTER_OFFSET_SIZE_MULT			= 100.0f;

dev_float	FLARES_COLOUR_SHIFT_R							= 0.01f;
dev_float	FLARES_COLOUR_SHIFT_G							= 0.02f;
dev_float	FLARES_COLOUR_SHIFT_B							= 0.05f;

dev_float	FLARES_SCREEN_CENTER_OFFSET_VERTICAL_SIZE_MULT	= 30.0f;
dev_float	FLARES_SCREEN_CENTER_OFFSET_VERTICAL_SIZE_RANGE	= 0.015f;

dev_float	CORONAS_ZBIAS_FROM_FINALSIZE_MULT				= 0.5f;

dev_float	CORONAS_MIN_SCREEN_EDGE_DIST_FOR_FADE			= 0.95f; 

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CCoronas g_coronas;

CoronaRenderInfos_t CCoronas::m_CoronasRenderBuffer = {NULL,0};

volatile unsigned g_coronaRenderInFlight = 0U;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////


// =============================================================================================== //
// HELPER CLASSES
// =============================================================================================== //

class dlCmdSetupCoronasFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_SetupCoronasFrameInfo,
	};

	dlCmdSetupCoronasFrameInfo();
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetupCoronasFrameInfo &) cmd).Execute(); }
	void Execute();

private:			
	Corona_t* newFrameInfo;
	int numCoronas;
	float rotation[2];
};

class dlCmdClearCoronasFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ClearCoronasFrameInfo,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdClearCoronasFrameInfo &) cmd).Execute(); }
	void Execute();
};

dlCmdSetupCoronasFrameInfo::dlCmdSetupCoronasFrameInfo()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "dlCmdSetupLODLightsFrameInfo::dlCmdSetupLODLightsFrameInfo - not called from the update thread");
	numCoronas = g_coronas.GetNumCoronasUpdate();
	if(numCoronas > 0)
	{
		Assertf(numCoronas <= CORONAS_MAX, "dlCmdSetupCoronasFrameInfo corona overflow (%d) - this shouldn't happen", numCoronas);
		const int sizeOfBuffer = sizeof(Corona_t) * (numCoronas + LATE_CORONAS_MAX);
		newFrameInfo = gDCBuffer->AllocateObjectFromPagedMemory<Corona_t>(DPT_LIFETIME_RENDER_THREAD, sizeOfBuffer, true);
		if( Verifyf(newFrameInfo != NULL, "dlCmdSetupCoronasFrameInfo failed to allocate memory for %d coronas - removing all coronas", numCoronas + LATE_CORONAS_MAX) )
		{
			// Memory allocated - copy the data
			sysMemCpy(newFrameInfo, g_coronas.GetCoronasUpdateBuffer(), sizeOfBuffer);
		}
		else
		{
			// Failed to allocate memory - just remove all coronas
			numCoronas = 0;
		}
	}
	else
	{
		newFrameInfo = NULL;
	}
	rotation[0] = g_coronas.GetRotationSlow();
	rotation[1] = g_coronas.GetRotationFast();
}

void dlCmdSetupCoronasFrameInfo::Execute()
{
	vfxAssertf(g_coronas.m_CoronasRenderBuffer.coronas == NULL, "m_CoronasRenderBuffer is not NULL, means that dlCmdClearCoronasFrameInfo was not executed before this");
	g_coronas.m_CoronasRenderBuffer.coronas = newFrameInfo;
	g_coronas.m_CoronasRenderBuffer.numCoronas = numCoronas;
	g_coronas.m_CoronasRenderBuffer.lateCoronas = newFrameInfo ? 0 : LATE_CORONAS_MAX;
	g_coronas.m_CoronasRenderBuffer.rotation[0] = rotation[0];
	g_coronas.m_CoronasRenderBuffer.rotation[1] = rotation[1];
}


void dlCmdClearCoronasFrameInfo::Execute()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "dlCmdClearCoronasFrameInfo::dlCmdClearCoronasFrameInfo - not called from the render thread");
	vfxAssertf( (g_coronas.GetNumCoronasRender() == 0 && g_coronas.m_CoronasRenderBuffer.coronas == NULL) || (g_coronas.GetNumCoronasRender() > 0 && g_coronas.m_CoronasRenderBuffer.coronas != NULL), 
		"m_CoronasRenderBuffer was set incorrectly. Num Coronas = %d, m_CoronasRenderBuffer = %p", g_coronas.GetNumCoronasRender(), g_coronas.m_CoronasRenderBuffer.coronas );
	g_coronas.m_CoronasRenderBuffer.coronas = NULL;
	g_coronas.m_CoronasRenderBuffer.numCoronas = 0;
}
///////////////////////////////////////////////////////////////////////////////
//  CLASS CCoronas
///////////////////////////////////////////////////////////////////////////////

#if GLINTS_ENABLED
PARAM( glints, "enable glints");
#endif

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CCoronas::Init								(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		// init the texture dictionary
		s32 txdSlot = CShaderLib::GetTxdId();

		// load in the textures
		LoadTextures(txdSlot);

#if GLINTS_ENABLED
		SetupGlints();
#endif

		m_screenEdgeMinDistForFade = CORONAS_MIN_SCREEN_EDGE_DIST_FOR_FADE;

		// init the debug info
	#if __BANK
		m_numRegistered = 0;
		m_numRendered = 0;
		m_numFlaresRendered = 0;
		m_numOverflown = 0;
		m_hasOverflown = false;

		m_debugCoronaActive = false;
		m_debugCoronaSize = 0.2f;
		m_debugCoronaColR = 255;
		m_debugCoronaColG = 200;
		m_debugCoronaColB = 0;
		m_debugCoronaIntensity = 1.0f;
		m_debugCoronaDir = false;

		m_overflowCoronas = false;

	    m_disableRender = false;
		m_disableRenderFlares = false;
	    m_renderDebug = false;

		m_debugCoronaForceRenderSimple = false;
		m_debugCoronaForceReflection = false;
		m_debugCoronaForceOmni = false;
		m_debugCoronaForceExterior = false;
		m_debugCoronaZBiasScale = 1.0f;
		m_debugApplyNewColourShift = false;
		m_debugDirLightConeAngleInner = CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER;
		m_debugDirLightConeAngleOuter = CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER;
		m_debugOverrideDirLightConeAngle = false;
	#endif // __BANK

	}
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		// initialise the counts
		m_numCoronas = 0;
	}
	m_rotation[0] = m_rotation[1] = 0.0f;
}

// ----------------------------------------------------------------------------------------------- //

void CCoronas::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetupCoronasFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdClearCoronasFrameInfo);
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CCoronas::Shutdown							(unsigned /*shutdownMode*/)
{
}


///////////////////////////////////////////////////////////////////////////////
//  LoadTextures
///////////////////////////////////////////////////////////////////////////////

void		 	CCoronas::LoadTextures						(s32 txdSlot)
{
	m_pTexCoronaDiffuse = g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("corona", 0x0be35467d));
	m_pTexAnimorphicFlareDiffuse = g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("flare_animorphic", 0x0448eef02));
	vfxAssertf(m_pTexCoronaDiffuse, "CCoronas::LoadTextures - corona diffuse texture load failed");
	vfxAssertf(m_pTexAnimorphicFlareDiffuse, "CCoronas::LoadTextures - animorphic diffuse texture load failed");
}

//////////////////////////////////////////////////////////////////////////
//	ComputeAdditionalZBias
//////////////////////////////////////////////////////////////////////////
float			CCoronas::ComputeAdditionalZBias(const CoronaRenderInfo& renderInfo)
{
#if __DEV
	if(!DebugLights::m_debugCoronaUseAdditionalZBiasForReflections)
	{
		return 0.0f;
	}
#endif
	if(renderInfo.mode == CVisualEffects::RM_CUBE_REFLECTION)
	{
		return m_zBiasAdditionalBiasEnvReflection;
	}
	if(renderInfo.mode == CVisualEffects::RM_WATER_REFLECTION)
	{
		return m_zBiasAdditionalBiasWaterReflection;
	}
	if(renderInfo.mode == CVisualEffects::RM_MIRROR_REFLECTION)
	{
		return m_zBiasAdditionalBiasMirrorReflection;
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
//	ComputeFinalProperties
//////////////////////////////////////////////////////////////////////////

void CCoronas::ComputeFinalProperties(const CoronaRenderInfo& renderInfo, CoronaRenderProperties_t* coronaRenderPropeties, float sizeScale, float intensityScale, s32& numToRender, s32& numFlaresToRender, s32 numCoronas)
{
	vfxAssertf((m_CoronasRenderBuffer.numCoronas == 0) || (m_CoronasRenderBuffer.coronas != NULL), "Coronas array in RenderThread is set to NULL and numCoronas > 0 ! Going to crash! ");
	float	flareMinAngleToFadeIn	= CORONAS_DEG2RAD(m_flareMinAngleDegToFadeIn);
	float	cosScaleRampUpAngle		= Cosf(CORONAS_DEG2RAD(m_scaleRampUpAngle));

	const bool IsCamUnderwater = Water::IsCameraUnderwater();

	// alter properties depending on render phase
	if (renderInfo.mode == CVisualEffects::RM_CUBE_REFLECTION)
	{
		sizeScale *= m_sizeScaleParaboloid;
		intensityScale *= CORONAS_PARABOLOID_REFLECTION_INTENSITY; 
	}
	else if (renderInfo.mode == CVisualEffects::RM_WATER_REFLECTION)
	{
		sizeScale *= m_sizeScaleWater;
		intensityScale *= m_intensityScaleWater;
	}
	
	// Throughout the render frame, m_CoronasRenderBuffer.numCoronas can only be equal or greater than numCoronas
	// We use numCoronas instead of accessing m_CoronasRenderBuffer.numCoronas directly because there's
	// a chance that by the time we hit this, LateRegister would've added a corona, which would result 
	// in a stack overflow since coronaRenderPropeties has already been allocated with numCoronas.
	// So, worst case, we don't render a late corona instead of crashing.
	for (s32 idx=0; idx<numCoronas; idx++)
	{
		// cache the current corona
		Corona_t& currCorona = m_CoronasRenderBuffer.coronas[idx];

		coronaRenderPropeties[numToRender].intensity = 0.0f;
		coronaRenderPropeties[numToRender].size = 0.0f;
		coronaRenderPropeties[numToRender].vPos = Vec3V(V_ZERO);
		coronaRenderPropeties[numToRender].bRenderFlare = false;


		const bool bSkipUnderwater = (IsCamUnderwater && currCorona.IsFlagSet(CORONA_DONT_RENDER_UNDERWATER));
		const bool bSkipOnlyInReflection = (renderInfo.mode == CVisualEffects::RM_DEFAULT && currCorona.IsFlagSet(CORONA_ONLY_IN_REFLECTION)) BANK_ONLY(&& !m_debugCoronaForceReflection);

		if (bSkipUnderwater || bSkipOnlyInReflection)
		{	
			continue;
		}
#if NV_SUPPORT
		const bool bStereoEnabled = GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo();		
#endif

		// don't render the corona if the light source is off-screen; ideally this would be done when registering the corona,
		// but since coronas get registered once and potentially get rendered several times in different render phases we have to
		// early out here instead.
		const grcViewport* pVp = grcViewport::GetCurrent();
		if (renderInfo.mode == CVisualEffects::RM_DEFAULT && pVp && pVp->IsPointVisible(currCorona.GetPosition() NV_SUPPORT_ONLY(, bStereoEnabled)) == false)
		{
			continue;
		}

		// consider for rendering if we're not doing a reflection map render OR the corona reflects
		bool renderCorona = renderInfo.mode == CVisualEffects::RM_DEFAULT;
		renderCorona |= renderInfo.mode == CVisualEffects::RM_CUBE_REFLECTION && !currCorona.IsFlagSet(CORONA_FROM_LOD_LIGHT) && !currCorona.IsFlagSet(CORONA_DONT_REFLECT);
		renderCorona |= renderInfo.mode != CVisualEffects::RM_DEFAULT && renderInfo.mode != CVisualEffects::RM_CUBE_REFLECTION && !currCorona.IsFlagSet(CORONA_DONT_REFLECT);

		if(	renderCorona )
		{
			// calc the final hdr 
			float finalIntensity = currCorona.GetIntensity();
			float fViewDirFadeForIntensity = 1.0f;
			float fViewDirFadeForFlare = 1.0f;
			float fViewDirFadeForSize = 1.0f;

			const Vec3V vForward = renderInfo.vCamPos - currCorona.GetPosition();
			const ScalarV vForwardLen = Mag(vForward);
			const Vec3V vForwardDir = InvScale(vForward, vForwardLen);

			const float len = vForwardLen.Getf();
			const float fadeOut = Saturate((len - m_dirMultFadeOffStart) / m_dirMultFadeOffDist);

			if (currCorona.IsFlagSet(CORONA_HAS_DIRECTION) BANK_ONLY(&& !m_debugCoronaForceOmni))
			{
				const float dirLightConeAngleInner = BANK_SWITCH(m_debugOverrideDirLightConeAngle ? m_debugDirLightConeAngleInner : currCorona.GetDirLightConeAngleInner(), currCorona.GetDirLightConeAngleInner());
				const float dirLightConeAngleInnerCos = rage::Cosf(DEG2RAD(dirLightConeAngleInner));

				const float dirLightConeAngleOuter = BANK_SWITCH(m_debugOverrideDirLightConeAngle ? m_debugDirLightConeAngleOuter : currCorona.GetDirLightConeAngleOuter(), currCorona.GetDirLightConeAngleOuter());
				const float dirLightConeAngleOuterCos = rage::Cosf(DEG2RAD(dirLightConeAngleOuter));

				// Use the light cone angle provided to do the fade
				ScalarV vViewDirFade = Dot(currCorona.GetDirection(), vForwardDir);
				const float viewDirFadeInner = Saturate((vViewDirFade.Getf()-dirLightConeAngleInnerCos)/(1.0001f-dirLightConeAngleInnerCos));
				const float viewDirFadeOuter = Saturate((vViewDirFade.Getf()-dirLightConeAngleOuterCos)/(1.0001f-dirLightConeAngleOuterCos));

				fViewDirFadeForSize = Saturate(viewDirFadeInner+viewDirFadeOuter);
				
				// Remap domain->range [threshold,1]->[0,1] so we still fade off smoothly as we approach the threshold
				const float fDirBias = 1.0f - currCorona.GetDirectionThreshold();
				const float fDirScale  = 1.0f / currCorona.GetDirectionThreshold();
				fViewDirFadeForIntensity = (fViewDirFadeForSize - fDirBias) * fDirScale;
				
				//Performing Fade off for directional fade only for LOD Lights
				if (currCorona.IsFlagSet(CORONA_FADEOFF_DIR_MULT))
				{
					fViewDirFadeForIntensity = Lerp(fadeOut, fViewDirFadeForIntensity, 1.0f);
					fViewDirFadeForSize = Lerp(fadeOut, fViewDirFadeForSize, 1.0f);
				}

				finalIntensity *= fViewDirFadeForIntensity * m_intensityScaleGlobal;
				fViewDirFadeForFlare = fViewDirFadeForSize;
			}

			if (IsCamUnderwater)
			{
				const float camWaterDepth = Water::GetCameraWaterDepth();
				const float coronaCamZ = Abs(vForward.GetZf());

				float camCoronaWaterDepthDiff = camWaterDepth-coronaCamZ;

				if (camCoronaWaterDepthDiff < 0.0f)
				{
					camCoronaWaterDepthDiff = 1.0f-Saturate(Abs(camCoronaWaterDepthDiff)/m_underwaterFadeDist);
					finalIntensity *= camCoronaWaterDepthDiff;
				}
			}

			// add to the buffer if the hdr is positive (i.e. don't add corona's with direction that are pointing away from us)
			if (finalIntensity>0.0f)
			{
				Vec3V     vPos         = currCorona.GetPosition();
				Vec3V     vCamToCorona = vPos - renderInfo.vCamPos;
				float     z            = Dot(vCamToCorona, renderInfo.vCamDir).Getf();
				float     size         = currCorona.GetSize() * m_sizeScaleGlobal;
				//Color32   col          = currCorona.col;
				float     zBiasAdj     = (ZBiasClamp(currCorona.GetZBias() BANK_ONLY(*m_debugCoronaZBiasScale)) + ComputeAdditionalZBias(renderInfo)) /z;

#if __DEV
				if (DebugLights::m_debugCoronaUseZBiasMultiplier)
#endif
				{
					zBiasAdj *= (1.0f + (renderInfo.zBiasMultiplier - 1.0f)*Clamp<float>((z - renderInfo.zBiasDistanceNear)/(renderInfo.zBiasDistanceFar - renderInfo.zBiasDistanceNear), 0.0f, 1.0f));
				}

				float zClamp = 0.0f;
				if (renderInfo.mode == CVisualEffects::RM_DEFAULT)
				{
					float sizeAdjustFar      = renderInfo.screenspaceExpansion;
#if __DEV
					if (DebugLights::m_debugCoronaSizeAdjustOverride)
					{
						sizeAdjustFar      = 0.0f;
					}
					sizeAdjustFar      += DebugLights::m_debugCoronaSizeAdjustFar;

					if (DebugLights::m_debugCoronaSizeAdjustEnabled)
#endif // __DEV
					{
						sizeAdjustFar = Lerp(fadeOut, sizeAdjustFar, 0.0f);

						zClamp = Max<float>(0.0f, z);
						size *= z*(sizeAdjustFar*renderInfo.camScreenToWorld + 1.0f/(z + zClamp));
					}

					BANK_ONLY(if (!m_debugCoronaForceOmni))
					{
						size *= fViewDirFadeForSize;
					}
				}
				else if (renderInfo.mode == CVisualEffects::RM_WATER_REFLECTION)
				{
					float sizeAdjustFar		= renderInfo.screenspaceExpansionWater;
					float sizeAdjustScale	= 1.0f;
#if __DEV
					if (DebugLights::m_debugCoronaSizeAdjustOverride)
					{
						sizeAdjustFar      = 0.0f;
					}

					sizeAdjustFar      += DebugLights::m_debugCoronaSizeAdjustFar;

					if (DebugLights::m_debugCoronaSizeAdjustEnabled)
#endif // __DEV
					{
						size *= sizeAdjustScale*(z*sizeAdjustFar*renderInfo.camScreenToWorld + 1.0f);
					}
				}
#if __DEV
				if (DebugLights::m_debugCoronaApplyZBias) // apply zBias
#endif // __DEV
				{
					vPos -= vCamToCorona*ScalarV(zBiasAdj);
					size -= size*zBiasAdj;
				}

				size *= sizeScale;
				finalIntensity *= intensityScale;

				if(size > 0.0f)
				{
					coronaRenderPropeties[numToRender].intensity = finalIntensity;
					coronaRenderPropeties[numToRender].size = size;
					coronaRenderPropeties[numToRender].vPos = vPos;

					if (currCorona.IsFlagSet(CORONA_HAS_DIRECTION) && fViewDirFadeForFlare >= flareMinAngleToFadeIn && !currCorona.IsFlagSet(CORONA_DONT_RENDER_FLARE) )
					{
						coronaRenderPropeties[numToRender].bRenderFlare = true;
						numFlaresToRender++;
					}

					coronaRenderPropeties[numToRender].rotationBlend = 0.0f;

					if(currCorona.IsFlagSet(CORONA_HAS_DIRECTION) && !currCorona.IsFlagSet(CORONA_DONT_RAMP_UP) && (g_coronas.m_doNGCoronas > 0.5f))
					{
						Vec3V dir = currCorona.GetDirection();
						Vec3V lightToCamera = renderInfo.vCamPos - currCorona.GetPosition();
						ScalarV cosAngleNumerator = Dot(dir, lightToCamera);

						if(cosAngleNumerator.Getf() > 0.0f)
						{
							ScalarV cosAngleDenominatorSqr = Dot(lightToCamera, lightToCamera);
							float cosAngle = cosAngleNumerator.Getf()/Sqrtf(cosAngleDenominatorSqr.Getf());
							float scaleRampUpBlend = Clamp((cosAngle - cosScaleRampUpAngle)/(1.0f - cosScaleRampUpAngle), 0.0f, 1.0f);

							// Blend between a scaling of 1 and m_scaleRampUp.
							float scaleRampUpFactor = (1.0f - scaleRampUpBlend)*1.0f + scaleRampUpBlend*m_scaleRampUp;
							// Apply the scale to the corona size.
							coronaRenderPropeties[numToRender].size *= scaleRampUpFactor;
							// Store the blend factor so we can blend between fast/slow rotations later.
							coronaRenderPropeties[numToRender].rotationBlend = scaleRampUpBlend;
						}
					}
					coronaRenderPropeties[numToRender].bDontRotate = currCorona.IsFlagSet(CORONA_DONT_ROTATE);
					coronaRenderPropeties[numToRender].registerIndex = (u16)idx;
					numToRender++;

#if __BANK
					if (m_renderDebug && renderInfo.mode == CVisualEffects::RM_DEFAULT)
					{
						Color32 colour = Color32(255, 255, 0, 128);

#if __DEV
						if (zClamp > 0.0f)
						{
							colour = Color32(255, 0, 0, 128); // show when zClamp is >0 by turning debug geometry red
						}
#endif // __DEV

						Vec3V vRight = renderInfo.vCamRight;
						Vec3V vUp    = renderInfo.vCamUp;

						// alter properties depending on render phase
						if (renderInfo.mode == CVisualEffects::RM_CUBE_REFLECTION)
						{
							vRight = Cross(vCamToCorona, FindMinAbsAxis(vCamToCorona));
							vRight = Normalize(vRight);

							vUp = Cross(vRight, vCamToCorona);
							vUp = Normalize(vUp);
						}

						vRight *= ScalarVFromF32(size);
						vUp *= ScalarVFromF32(size);

						const Vec3V vCorner00 = vPos - vRight - vUp;
						const Vec3V vCorner01 = vPos - vRight + vUp;
						const Vec3V vCorner10 = vPos + vRight - vUp;
						const Vec3V vCorner11 = vPos + vRight + vUp;

						grcDebugDraw::Line(vCorner00, vCorner10, colour);
						grcDebugDraw::Line(vCorner10, vCorner11, colour);
						grcDebugDraw::Line(vCorner11, vCorner01, colour);
						grcDebugDraw::Line(vCorner01, vCorner00, colour);

						if (currCorona.IsFlagSet(CORONA_HAS_DIRECTION)) // render dir line
						{
							grcDebugDraw::Line(currCorona.GetPosition(), vPos + currCorona.GetDirection(), Color32(0, 255, 0, 128));
						}
					}
#endif // __BANK
				}
			}
		}
	}

}

///////////////////////////////////////////////////////////////////////////////
//  SetupRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CCoronas::SetupRenderthreadFrameInfo()
{
	DLC(dlCmdSetupCoronasFrameInfo, ());
	UpdateAfterRender();
}

///////////////////////////////////////////////////////////////////////////////
//  ClearRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CCoronas::ClearRenderthreadFrameInfo()
{
	DLC(dlCmdClearCoronasFrameInfo, ());
}

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void			CCoronas::Render							(CVisualEffects::eRenderMode mode, float sizeScale, float intensityScale)
{
#if __BANK
	if (m_disableRender)
	{
		return;
	}
#endif // __BANK

	// check that we're in the render thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CCoronas::Render - not called from the render thread");

	sysInterlockedIncrement(&g_coronaRenderInFlight);

	// get the number of coronas to consider to render
	s32 numCoronas = m_CoronasRenderBuffer.numCoronas;

	// store some viewport and camera info
	const grcViewport* pCurrViewport = grcViewport::GetCurrent();
	Mat34V_ConstRef camMtx = pCurrViewport->GetCameraMtx();

	CShaderLib::UpdateGlobalGameScreenSize();

#if __BANK
	if (mode == CVisualEffects::RM_DEFAULT) // only update this for default mode
	{
		m_numRegistered = numCoronas;
	}
#endif // __BANK

	const float screenspaceExpansion = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_SPRITE_CORONA_SCREENSPACE_EXPANSION);

	// cache common render data
	CoronaRenderInfo renderInfo;
	renderInfo.mode							= mode;
	renderInfo.camScreenToWorld				= pCurrViewport->GetTanHFOV()/VideoResManager::GetSceneWidth();
	renderInfo.tanHFov						= pCurrViewport->GetTanHFOV();
	renderInfo.tanVFov						= pCurrViewport->GetTanVFOV();
	renderInfo.screenspaceExpansion			= screenspaceExpansion + m_screenspaceExpansion;
	renderInfo.screenspaceExpansionWater	= screenspaceExpansion + m_screenspaceExpansionWater;
	renderInfo.zBiasMultiplier				= m_zBiasMultiplier;
	renderInfo.zBiasDistanceNear			= m_zBiasDistanceNear;
	renderInfo.zBiasDistanceFar				= m_zBiasDistanceFar;
	renderInfo.vCamPos						= camMtx.GetCol3();
	renderInfo.vCamDir						= -camMtx.GetCol2();
	renderInfo.vCamRight					= camMtx.GetCol0();
	renderInfo.vCamUp						= -camMtx.GetCol1();

#if __DEV
	if (DebugLights::m_debugCoronaOverwriteZBiasMultiplier)
	{
		renderInfo.zBiasMultiplier = DebugLights::m_debugCoronaZBiasMultiplier;
		renderInfo.zBiasDistanceNear = DebugLights::m_debugCoronaZBiasDistanceNear;
		renderInfo.zBiasDistanceFar = DebugLights::m_debugCoronaZBiasDistanceFar;
	}
#endif

	// calc the final hdr mults of each corona (a zero or negative hdr means that we don't want to render)
	s32 numToRender = 0;
	s32 numFlaresToRender = 0;
	CoronaRenderProperties_t* coronaRenderProperties = Alloca(CoronaRenderProperties_t, numCoronas);
	ComputeFinalProperties(renderInfo, coronaRenderProperties, sizeScale, intensityScale, numToRender, numFlaresToRender, numCoronas);
	u32 *doubleCoronas = Alloca(u32, numCoronas);

#if __BANK
	if (mode == CVisualEffects::RM_DEFAULT) // only update this for default mode
	{
		m_numRendered = numToRender;
	}
#endif // __BANK

	grcDrawMode drawMode = drawQuads;
	s32 vertsPerQuad = 4;

#if __D3D11
	drawMode = drawTriStrip;
	vertsPerQuad = 6;
	bool restoreDepthBuffer = false;
#endif

	// check if we have some to render
	if (numToRender>0)
	{
		grcViewport::SetCurrentWorldIdentity();
		grcWorldIdentity();

		CSprite2d coronaSprite;

		// set up the custom renderer (this sets a different technique in the "im" shader)
		if (mode == CVisualEffects::RM_WATER_REFLECTION || 
		    mode == CVisualEffects::RM_MIRROR_REFLECTION ||
			mode == CVisualEffects::RM_CUBE_REFLECTION
			BANK_ONLY(|| m_debugCoronaForceRenderSimple))
		{
			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_TestOnly_LessEqual, grcStateBlock::BS_Add);
#if GS_INSTANCED_CUBEMAP
			if (mode == CVisualEffects::RM_CUBE_REFLECTION)
				coronaSprite.BeginCustomList(CSprite2d::CS_CORONA_CUBEINST_REFLECTION, m_pTexCoronaDiffuse);
			else
#endif
			coronaSprite.BeginCustomList(CSprite2d::CS_CORONA_SIMPLE_REFLECTION, m_pTexCoronaDiffuse);
		}
		else
		{
		#if __D3D11
			//Set depth buffer to NULL to avoid read/write conflicts.
			//Depth testing done in the shader.
			GRCDEVICE.SetDepthBuffer(NULL);
			restoreDepthBuffer = true;
		#endif
			// get the depth texture
			grcTexture *depthTex = NULL;

			depthTex = CRenderTargets::GetDepthBufferQuarterLinear();

			Assert(mode == CVisualEffects::RM_DEFAULT);
			coronaSprite.SetDepthTexture(depthTex);

			int pass = 0;
		#if __DEV
			if (mode == CVisualEffects::RM_DEFAULT)
			{
				if (DebugLights::m_debugCoronaShowOcclusion)
				{
					pass = 2;
				}
				else if (!DebugLights::m_debugCoronaApplyNearFade)
				{
					pass = 1;
				}
			}
		#endif // __DEV

			const Vec4V coronaParams = Vec4V(
				m_occlusionSizeScale,
				m_occlusionSizeMax,
				0.0f,
				0.0f
			);

			const Vec4V coronaBaseValue = Vec4V(
				g_coronas.m_baseValueR,
				g_coronas.m_baseValueG,
				g_coronas.m_baseValueB,
				0.0f
			);

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Add);
			coronaSprite.SetGeneralParams(RCC_VECTOR4(coronaParams), RCC_VECTOR4(coronaBaseValue));

			Vector4 refMipBlurParams;
			refMipBlurParams.Zero();
			refMipBlurParams.x = m_screenEdgeMinDistForFade;
			coronaSprite.SetRefMipBlurParams(refMipBlurParams);

			coronaSprite.BeginCustomList(CSprite2d::CS_CORONA_DEFAULT, m_pTexCoronaDiffuse, pass);
		}

		// render the coronas whilst we still have some to render
		int coronaIndex = 0;
		int numDoubleToRender = 0;
		int numCoronasInList = numToRender;
		while (numToRender)
		{
			// calc the size of this render batch
			int batchSize = Min(grcBeginMax/(vertsPerQuad), numToRender);

			// init the render batch
			grcBegin(drawMode, batchSize*(vertsPerQuad));

			// loop through the coronas filling up the batch
			for(int i=0; i<batchSize; i++)
			{
				if(RenderCorona(coronaIndex, mode, renderInfo, coronaRenderProperties, 1.0f, m_layer1Muliplier))
				{
					doubleCoronas[numDoubleToRender++] = coronaIndex;
				}
				coronaIndex++;
			}

			// end this render batch
			grcEnd();

			// update the number left to render
			numToRender -= batchSize;
		}

		// Revisit the coronas which require a double NG one on top.
		u32 doubleIdx = 0;
		while (numDoubleToRender)
		{
			// calc the size of this render batch
			int batchSize = Min(grcBeginMax/(vertsPerQuad), numDoubleToRender);

			// init the render batch
			grcBegin(drawMode, batchSize*(vertsPerQuad));

			// loop through the coronas filling up the batch
			for(int i=0; i<batchSize; i++)
				RenderCorona(doubleCoronas[doubleIdx++], mode, renderInfo, coronaRenderProperties, -1.0f, m_layer2Muliplier);

			// end this render batch
			grcEnd();

			// update the number left to render
			numDoubleToRender -= batchSize;
		}

		// shut down the custom renderer
		coronaSprite.EndCustomList();

		// Render flares
		RenderFlares(renderInfo, coronaRenderProperties, numCoronasInList, numFlaresToRender);

		// reset the render state
		CShaderLib::SetGlobalEmissiveScale(1.0f);
	}

#if GLINTS_ENABLED
	RenderGlints();
#endif

#if __D3D11
	if( restoreDepthBuffer )
	{
		GRCDEVICE.SetDepthBuffer( GRCDEVICE.GetPreviousDepthBuffer() );
	}
#endif

	sysInterlockedDecrement(&g_coronaRenderInFlight);
}

bool				CCoronas::RenderCorona				(int coronaIndex, CVisualEffects::eRenderMode mode, CCoronas::CoronaRenderInfo &renderInfo, CoronaRenderProperties_t *coronaRenderProperties, float sign, float layerMultiplier)
{
	bool ret = false;

	// cache the corona and some properties
	const Corona_t& currCorona		= m_CoronasRenderBuffer.coronas[coronaRenderProperties[coronaIndex].registerIndex];
	const Vec3V& vPos				= coronaRenderProperties[coronaIndex].vPos;		
	Vec3V	vCamToCorona			= vPos - renderInfo.vCamPos;				
	const float	size				= coronaRenderProperties[coronaIndex].size;
	Color32	col						= currCorona.GetColor();					
	const float	intensity			= coronaRenderProperties[coronaIndex].intensity;

	// calc the render vectors
	Vec3V vRight = renderInfo.vCamRight;
	Vec3V vUp    = renderInfo.vCamUp;

	// alter properties depending on render phase
	if (mode == CVisualEffects::RM_CUBE_REFLECTION)
	{
		vRight = Cross(vCamToCorona, FindMinAbsAxis(vCamToCorona));
		vRight = Normalize(vRight);

		vUp = Cross(vRight, vCamToCorona);
		vUp = Normalize(vUp);
	}

	vRight *= ScalarVFromF32(size);
	vUp *= ScalarVFromF32(size);

	if((coronaRenderProperties[coronaIndex].bDontRotate) || (g_coronas.m_doNGCoronas < 0.5f))
	{
		vfxAssertf(sign == 1.0f, "CCoronas::RenderCorona()...Shouldn`t reach here when rendering 2nd overlay corona." );
		AddCoronaVerts(vPos, vRight, vUp, col, layerMultiplier*intensity);

	}
	else
	{
		// Compute the angle (a blend between the slow and fast rotation angles).
		float angle = (1.0f - coronaRenderProperties[coronaIndex].rotationBlend)*g_coronas.m_CoronasRenderBuffer.rotation[0] + coronaRenderProperties[coronaIndex].rotationBlend*g_coronas.m_CoronasRenderBuffer.rotation[1];
		float sinRotation = sign*Sinf(angle);
		float cosRotation = Cosf(angle);

		ScalarV s(sinRotation);
		ScalarV c(cosRotation);
		const Vec3V vRight_toUse = c*vRight + s*vUp;
		const Vec3V vUp_toUse = c*vUp - s*vRight;
		AddCoronaVerts(vPos, vRight_toUse, vUp_toUse, col, layerMultiplier*intensity);
		ret = true;
	}
	return ret;
}


void				CCoronas::AddCoronaVerts				(Vec3V_In vPos, Vec3V_In vRight, Vec3V_In vUp, Color32 col, float intensity)
{
	const Vec3V vCorner00 = vPos - vRight - vUp;
	const Vec3V vCorner01 = vPos - vRight + vUp;
	const Vec3V vCorner10 = vPos + vRight - vUp;
	const Vec3V vCorner11 = vPos + vRight + vUp;

	const float centreX = vPos.GetXf(); // technically, these are only necessary for RM_DEFAULT
	const float centreY = vPos.GetYf();
	const float centreZ = vPos.GetZf();

	// send the verts 
	// NOTE: we send the hdr multiplier in the v tex coord slot (so it can be greater than 1.0)
	//       we therefore have to send the v tex coord in the alpha slot and we presume that the actual alpha is always 255

#if __D3D11
	col.SetAlpha(255);
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, 0.0f, intensity); //degenerate vert
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, col, 1.0f, intensity);

	col.SetAlpha(0);
	grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, col, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, 1.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, 1.0f, intensity); //degenerate vert
#else
	col.SetAlpha(255);
	grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, 0.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, col, 1.0f, intensity);

	col.SetAlpha(0);
	grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, 1.0f, intensity);
	grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, col, 0.0f, intensity);
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  RenderFlares
///////////////////////////////////////////////////////////////////////////////

void			CCoronas::RenderFlares						(const CoronaRenderInfo& renderInfo, CoronaRenderProperties_t* coronaRenderProperties, s32 ASSERT_ONLY(numCoronas), s32 numToRender)
{
#if __BANK
	if (m_disableRenderFlares)
	{
		return;
	}
#endif

	if (numToRender == 0 || renderInfo.mode != CVisualEffects::RM_DEFAULT)
	{
		return;
	}

#if __BANK
	m_numFlaresRendered = numToRender;
#endif

	grcDrawMode drawMode = drawQuads;
	s32 vertsPerQuad = 4;

#if __D3D11
	drawMode = drawTriStrip;
	vertsPerQuad = 6;
#endif


	grcViewport::SetCurrentWorldIdentity();
	grcWorldIdentity();

	CSprite2d coronaSprite;

	// get the depth texture
	grcTexture *depthTex = NULL;

	depthTex = CRenderTargets::GetDepthBufferQuarterLinear();

	Assert(renderInfo.mode == CVisualEffects::RM_DEFAULT);
	coronaSprite.SetDepthTexture(depthTex);

	// use pass with no near fade, otherwise flares won't
	// get occluded when off-screen
	int pass = 1;
#if __DEV
	if (DebugLights::m_debugCoronaShowOcclusion)
	{
		pass = 2;
	}
#endif // __DEV

	const Vec4V coronaParams = Vec4V(
		m_occlusionSizeScale,
		m_occlusionSizeMax,
		0.0f,
		0.0f
		);

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Add);
	coronaSprite.SetGeneralParams(RCC_VECTOR4(coronaParams), VEC4V_TO_VECTOR4(Vec4V(V_ZERO)));
	Vector4 refMipBlurParams;
	refMipBlurParams.Zero();
	refMipBlurParams.x = m_screenEdgeMinDistForFade;
	coronaSprite.SetRefMipBlurParams(refMipBlurParams);
	coronaSprite.BeginCustomList(CSprite2d::CS_CORONA_DEFAULT, m_pTexAnimorphicFlareDiffuse, pass);

	const grcViewport* pCurrViewport = grcViewport::GetCurrent();
	Mat44V_ConstRef projMtx = pCurrViewport->GetCompositeMtx();

	// render the flates whilst we still have some to render
	int coronaIndex = 0;
	float	  flareMinAngleToFadeIn	= CORONAS_DEG2RAD(m_flareMinAngleDegToFadeIn);
	while (numToRender)
	{
		// calc the size of this render batch
		int batchSize = Min(grcBeginMax/vertsPerQuad, numToRender);

		// init the render batch
		grcBegin(drawMode, batchSize*vertsPerQuad);

		// loop through the coronas filling up the batch
		s32 batchIndex = 0;
		while (batchIndex<batchSize)
		{
			vfxAssertf(coronaIndex<numCoronas, "CCoronas::Render - invalid corona index found (batchIndex=%d, batchSize=%d, coronaIndex=%d, numCoronas=%d)", batchIndex, batchSize, coronaIndex, numCoronas);
			//ASSERT_ONLY(if (coronaIndex>=numCoronas) break;)

			Assert(m_CoronasRenderBuffer.coronas != NULL);
			const Corona_t& currCorona   = m_CoronasRenderBuffer.coronas[coronaRenderProperties[coronaIndex].registerIndex];
			const CoronaRenderProperties_t& currCoronaRenderProperties = coronaRenderProperties[coronaIndex];

			const u8 bRenderFlare = currCoronaRenderProperties.bRenderFlare;

			if (bRenderFlare)
			{
				// cache the corona and some properties
				Vec3V     vPos         	= currCoronaRenderProperties.vPos;
				Vec3V	  vCoronaToCam	= Normalize(renderInfo.vCamPos-vPos);
				float     size         	= currCoronaRenderProperties.size;
				Color32   col          	= currCorona.GetColor();
				float     intensity    	= currCoronaRenderProperties.intensity;
				float	  camCoronaAngleMult	= Saturate(Dot(currCorona.GetDirection(), vCoronaToCam)).Getf();

				// apply colour shift
			#if __BANK
				if (m_debugApplyNewColourShift)
				{
					Vec3V vCol = col.GetRGB();
					Vec3V vShiftedCol = vCol*Vec3V(m_flareColShiftR, m_flareColShiftG, m_flareColShiftB);
					ScalarV vColShift= Scale(Dot(vCol, vCol), ScalarV(V_THIRD));
					vShiftedCol = Lerp(vColShift, vCol, vShiftedCol);
					col = Color32(vShiftedCol);
				}
				else
			#endif
				{
					col.Setf(col.GetRedf()*m_flareColShiftR, col.GetGreenf()*m_flareColShiftG, col.GetBluef()*m_flareColShiftB);
				}

				// Convert to NDC space
				Vec3V vProjPos = TransformProjective(projMtx, vPos);
	
				// Compute size scalar
				ScalarV vSizeScalar = Saturate(Abs(Abs(vProjPos.GetX())-Abs(vProjPos.GetY())));
				vSizeScalar*=vSizeScalar;

				// Rotate our position by -45 degrees so quadrant test gets simplified
				ScalarV vCosSin45 = ScalarV(0.707f);
				Vec2V vRotatedPos;
				vRotatedPos.SetX(vProjPos.GetX()*vCosSin45-vProjPos.GetY()*(-vCosSin45));
				vRotatedPos.SetY(vProjPos.GetY()*vCosSin45+vProjPos.GetX()*(-vCosSin45));
	
				// Quadrant test to decide what multiplier applies (horizontal or vertial)
				BoolV vSameSign = SameSign(vRotatedPos.GetX(), vRotatedPos.GetY());
				ScalarV vHSizeScaler = SelectFT(vSameSign, vSizeScalar, ScalarV(V_ZERO));
				ScalarV vVSizeScaler = SelectFT(vSameSign, ScalarV(V_ZERO), vSizeScalar);

				// size scalers [0, 1] -> [1, FLARES_SCREEN_CENTER_OFFSET_SIZE_MULT]
				vHSizeScaler = ScalarV(V_ONE) + vHSizeScaler*(ScalarVFromF32(m_flareScreenCenterOffsetSizeMult)-ScalarV(V_ONE));
				vVSizeScaler = ScalarV(V_ONE) + vVSizeScaler*(ScalarVFromF32(m_flareScreenCenterOffsetVerticalSizeMult)-ScalarV(V_ONE));


				// make flares fade based on FLARES_MIN_ANGLE_DEG_TO_FADE_IN
				const float angleBasedMult = Clamp<float>((camCoronaAngleMult-flareMinAngleToFadeIn), 0.0f, 1.0f);
				intensity *= angleBasedMult;

				// calc the render vectors
				const Vec3V vRight = renderInfo.vCamRight*ScalarVFromF32(size*m_flareCoronaSizeRatio) * vHSizeScaler;
				const Vec3V vUp    = renderInfo.vCamUp   *ScalarVFromF32(size*m_flareCoronaSizeRatio) * vVSizeScaler;

				const Vec3V vCorner00 = vPos - vRight - vUp;
				const Vec3V vCorner01 = vPos - vRight + vUp;
				const Vec3V vCorner10 = vPos + vRight - vUp;
				const Vec3V vCorner11 = vPos + vRight + vUp;

				const float centreX = vPos.GetXf(); // technically, these are only necessary for CVisualEffects::RM_DEFAULT
				const float centreY = vPos.GetYf();
				const float centreZ = vPos.GetZf();

				// rotate UVs 90 degrees when rendering vertical flares to use texture correctly
				const bool bRotate90D = (vVSizeScaler.Getf() > 1.0f);

													//[0]		//[1]		//[2]		//[3]
													//BL		//BR		//TR		//TL
													//0,1		//1,1		//1,0		//0,0		
				const float texCoordS[2][4] =	{	{0.0f, 		1.0f, 		1.0f,		0.0f },		// no rotation
													{1.0f, 		1.0f, 		0.0f,		0.0f }	};	// 90 degrees rotation

													//[0]		//[1]		//[2]		//[3]
													//BL		//BR		//TR		//TL
													//0,1		//1,1		//1,0		//0,0		
				const int	texCoordT[2][4] =	{	{255, 		255, 		0,			0	 },		// no rotation
													{255, 		0, 			0,			255	 }	};	// 90 degrees rotation

				// send the verts 
				// NOTE: we send the hdr multiplier in the v tex coord slot (so it can be greater than 1.0)
				//       we therefore have to send the v tex coord in the alpha slot and we presume that the actual alpha is always 255
			#if __D3D11
				col.SetAlpha(texCoordT[bRotate90D][0]);				
				grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, texCoordS[bRotate90D][0], intensity); //degenerate vert				
				grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, texCoordS[bRotate90D][0], intensity);		
				col.SetAlpha(texCoordT[bRotate90D][1]);
				grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, col, texCoordS[bRotate90D][1], intensity);


				col.SetAlpha(texCoordT[bRotate90D][3]);
				grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, col, texCoordS[bRotate90D][3], intensity);
				col.SetAlpha(texCoordT[bRotate90D][2]);
				grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, texCoordS[bRotate90D][2], intensity);
				grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, texCoordS[bRotate90D][2], intensity); //degenerate vert
			#else
				col.SetAlpha(texCoordT[bRotate90D][0]);
				grcVertex(VEC3V_ARGS(vCorner01), centreX, centreY, centreZ, col, texCoordS[bRotate90D][0], intensity);
				col.SetAlpha(texCoordT[bRotate90D][1]);			
				grcVertex(VEC3V_ARGS(vCorner11), centreX, centreY, centreZ, col, texCoordS[bRotate90D][1], intensity);

				col.SetAlpha(texCoordT[bRotate90D][2]);
				grcVertex(VEC3V_ARGS(vCorner10), centreX, centreY, centreZ, col, texCoordS[bRotate90D][2], intensity);
				col.SetAlpha(texCoordT[bRotate90D][3]);
				grcVertex(VEC3V_ARGS(vCorner00), centreX, centreY, centreZ, col, texCoordS[bRotate90D][3], intensity);
			#endif


			#if __BANK
				if (m_renderDebug)
				{
					Color32 colour = Color32(0, 255, 255, 128);

					grcDebugDraw::Line(vCorner00, vCorner10, colour);
					grcDebugDraw::Line(vCorner10, vCorner11, colour);
					grcDebugDraw::Line(vCorner11, vCorner01, colour);
					grcDebugDraw::Line(vCorner01, vCorner00, colour);
				}
			#endif // __BANK

				// move onto the next space in the batch
				batchIndex++;
			}

			// move onto the next corona
			coronaIndex++;
		}

		// end this render batch
		grcEnd();

		// update the number left to render
		numToRender -= batchSize;
	}

	// shut down the custom renderer
	coronaSprite.EndCustomList();
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateAfterRender
///////////////////////////////////////////////////////////////////////////////

void			CCoronas::UpdateAfterRender					()
{
	m_numCoronas = 0;
#if __BANK
	// register the debug corona?
	if (m_debugCoronaActive)
	{
		Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

		Vec3V vDir = vPlayerCoords - m_vDebugCoronaPos;
		Color32 col(m_debugCoronaColR, m_debugCoronaColG, m_debugCoronaColB, 255);

		Register(
			m_vDebugCoronaPos, 
			m_debugCoronaSize, 
			col,  
			m_debugCoronaIntensity, 
			0.0f, 
			vDir, 
			1.0f,
			CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
		    CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
			(m_debugCoronaDir) ? CORONA_HAS_DIRECTION : 0);
	}
	else
	{
		if(m_overflowCoronas)
		{
			Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

			Vec3V vDir = vPlayerCoords - m_vDebugCoronaPos;
			Color32 col(m_debugCoronaColR, m_debugCoronaColG, m_debugCoronaColB, 255);

			for(int i = 0; i < CORONAS_MAX + 100; i++)
			{
				Register(
					m_vDebugCoronaPos, 
					m_debugCoronaSize, 
					col,  
					m_debugCoronaIntensity, 
					0.0f, 
					vDir, 
					1.0f,
					CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
					CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
					(m_debugCoronaDir) ? CORONA_HAS_DIRECTION : 0);
			}
		}
	}

	// reset the overflow count
	m_numOverflown = 0;
#endif // __BANK

	// Update rotation of coronas.
	for(int i=0; i<2; i++)
	{
		m_rotation[i] += TIME.GetSeconds()*m_rotationSpeed[i];
		m_rotation[i] = Wrap(m_rotation[i], 0.0f, 2.0f*PI);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////

bool			CCoronas::LateRegister						(Vec3V_In vPos,
															 const float size,
															 const Color32 col,
															 const float intensity,
															 const float zBias,
															 Vec3V_In   vDir,
															 const float dirViewThreshold,
															 const float dirLightConeAngleInner,
															 const float dirLightConeAngleOuter,
															 const u16 coronaFlags)
{
	// check that we're in the update thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CCoronas::Register - not called from the update thread");

#if __BANK
	if( m_disableLateRegistration ) 
	{
		return false;
	}
#endif // __BANK

	if (size == 0.0f || intensity <= 0.0f)
	{
		return false;
	}

	int& numCoronas  = g_coronas.m_CoronasRenderBuffer.numCoronas;
	int& lateCoronas = g_coronas.m_CoronasRenderBuffer.lateCoronas;

	if ((numCoronas < 1) || (lateCoronas >= LATE_CORONAS_MAX))
	{
		return false;
	}

	Corona_t* coronas = g_coronas.m_CoronasRenderBuffer.coronas;
	Assert(coronas != NULL);

	if (coronas != NULL)
	{
		Vec3V vNormDir = Normalize(vDir);

		Corona_t currCorona;
		currCorona.SetPosition(vPos);
		currCorona.SetSize(size);
		currCorona.SetColor(col);
		currCorona.SetIntensity(intensity);
		currCorona.SetZBias(zBias);
		currCorona.SetDirectionViewThreshold(Vec4V(vNormDir,ScalarV(dirViewThreshold)));

		u8	uDirLightConeAngleInner = (u8)Clamp(dirLightConeAngleInner, 0.0f, CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_INNER);
		u8	uDirLightConeAngleOuter = (u8)Clamp(dirLightConeAngleOuter, 0.0f, CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_OUTER);

		currCorona.SetCoronaFlagsAndDirLightConeAngles(coronaFlags, uDirLightConeAngleInner, uDirLightConeAngleOuter);	

		// If CCoronas::Render is being executed, reject the registration
		if (sysInterlockedRead(&g_coronaRenderInFlight) != 0U)
			return false;

		// This is still not safe, since we're copying and incrementing the number of coronas and a call
		// to CCoronas::Render might be executing in parallel. However, CCoronas::Render caches numCoronas early,  
		// so worst case LateRegister succeeds when it shouldn't, but CCoronas::Render won't process the late coronas
		// Alternative would be using a mutex or having late coronas on a separate buffer.
		coronas[numCoronas] = currCorona;
		numCoronas++;
		lateCoronas++;
		return true;
	}

	return false;
}

// ----------------------------------------------------------------------------------------------- //
// If any changes are made to this function, please make the same changes in 
// ProcessLodLightVisibility::ProcessVisibility() and CoronaAndLightList::RegisterCorona()
// as we add coronas from there also asynchronously
//-------------------------------------------------------------------------------------------------//
void			CCoronas::Register							(Vec3V_In vPos,
															 const float size,
															 const Color32 col,
															 const float intensity,
															 const float zBias,
															 Vec3V_In   vDir,
															 const float dirViewThreshold,
															 const float dirLightConeAngleInner,
															 const float dirLightConeAngleOuter,
															 const u16 coronaFlags)
{
	// check that we're in the update thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CCoronas::Register - not called from the update thread");

	if (size == 0.0f || intensity <= 0.0f || CRenderer::GetDisableArtificialLights())
	{
		return;
	}

	while(true)
	{
		u32 numCoronas = m_numCoronas;
		if(numCoronas >= CORONAS_MAX)
		{
#if __BANK
			m_hasOverflown = true;
			m_numOverflown++;
			Vector3 vCamPos = camInterface::GetPos();
			Assertf(!m_hasOverflown, "Corona overflow at %.3f %.3f %.3f", vCamPos.x, vCamPos.y, vCamPos.z);
#endif // __BANK
			break;
		}

		if(sysInterlockedCompareExchange(&(m_numCoronas), numCoronas+1, numCoronas) == numCoronas)
		{
			u8	uDirLightConeAngleInner			= (u8)Clamp(dirLightConeAngleInner, 0.0f, CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_INNER);
			u8	uDirLightConeAngleOuter			= (u8)Clamp(dirLightConeAngleOuter, 0.0f, CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_OUTER);
		

			Vec3V vNormDir = Normalize(vDir);

			Corona_t& currCorona		= m_CoronasUpdateBuffer[numCoronas];
			currCorona.SetPosition(vPos);
			currCorona.SetSize(size);
			currCorona.SetColor(col);
			currCorona.SetIntensity(intensity);
			currCorona.SetZBias(zBias);
			currCorona.SetDirectionViewThreshold(Vec4V(vNormDir,ScalarV(dirViewThreshold)));
			currCorona.SetCoronaFlagsAndDirLightConeAngles(coronaFlags, uDirLightConeAngleInner, uDirLightConeAngleOuter);	
			
			break;
		}
	}
}

int CCoronas::GetNumCoronasUpdate()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CCoronas::GetNumCoronasUpdate - not called from the update thread");
	return m_numCoronas;
}
int CCoronas::GetNumCoronasRender()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CCoronas::GetNumCoronasRender - not called from the render thread");
	return m_CoronasRenderBuffer.numCoronas;
}


void CCoronas::GetDataForAsyncJob(Corona_t*& pCoronaArray, u32*& pCount BANK_ONLY(, u32*& pOverflown, float& zBiasScale))
{
	pCoronaArray = &(m_CoronasUpdateBuffer[0]);
	pCount = &(m_numCoronas);

#if __BANK
	pOverflown = &m_numOverflown;
	zBiasScale = m_debugCoronaZBiasScale;
#endif
}

#if GLINTS_ENABLED

void CCoronas::SetupGlints()
{
	grmShader* imShader = CSprite2d::GetShader();
	if( imShader )
	{
		//Default to disabled for now until I can get this hooked up to the settings.
		m_GlintsEnabled = PARAM_glints.Get();
#if RSG_PC
		if (!GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50))
			m_GlintsEnabled = false;
#endif // RSG_PC

		m_GlintTechnique = grcetNONE;
		m_GlintBackbufferPointVar = grcevNONE;
		m_GlintBackbufferLinearVar = grcevNONE;
		m_GlintDepthVar = grcevNONE;
		m_GlintPointBufferVar = grcevNONE;
		m_GlintParamsVar0 = grcevNONE;
		m_GlintParamsVar1 = grcevNONE;

		m_GlintBrightnessThreshold = 100.0f;
		m_GlintSizeX = 1000.0f;
		m_GlintSizeY = 5.0f;
		m_GlintAlphaScale = 525.0f;
		m_GlintDepthScaleRange = 0.338f;

		if (m_GlintsEnabled)
		{
			//DX11 TODO: This will need resizing when changing resolution in game.
			u32 width = GRCDEVICE.GetWidth();
			u32 height = GRCDEVICE.GetHeight();

			m_GlintAccumBuffer.Initialise( width*height, 28, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured );

			ALIGNAS(16) u32 bufferInit[4] = {0,1,0,0};
			m_GlintRenderBuffer.Initialise(1, 16, grcBindNone, grcsBufferCreate_ReadWrite, grcsBufferSync_None, &bufferInit, false, grcBufferMisc_DrawIndirectArgs );

			m_GlintTechnique = imShader->LookupTechnique("Glints");
			m_GlintBackbufferPointVar = imShader->LookupVar("RenderPointMap");
			m_GlintBackbufferLinearVar = imShader->LookupVar("RefMipBlurMap");
			m_GlintDepthVar = imShader->LookupVar("DepthPointMap");		
			m_GlintPointBufferVar = imShader->LookupVar("GlintSpritePointBuffer");
			m_GlintParamsVar0 = imShader->LookupVar("GeneralParams0");
			m_GlintParamsVar1 = imShader->LookupVar("GeneralParams1");
		}
	}
}
void CCoronas::RenderGlints()
{
	if( !m_GlintsEnabled )
		return;

	enum passes
	{
		pGlintGenerate = 0,
		pGlintDrawSprites
	};

	Color32 color(1.0f,1.0f,1.0f,1.0f);

	PIXBegin(0, "Glints");
	grmShader* imShader = CSprite2d::GetShader();

	Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	imShader->SetVar(m_GlintParamsVar0, projParams);

	grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 

	// Generate Glint positions in UAV

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default_WriteMaskNone);

	imShader->SetVar(m_GlintParamsVar1, Vector4(m_GlintBrightnessThreshold, m_GlintSizeX, m_GlintSizeY, m_GlintAlphaScale));

	imShader->SetVar(m_GlintBackbufferPointVar, CRenderTargets::GetBackBuffer());
	imShader->SetVar(m_GlintBackbufferLinearVar, CRenderTargets::GetBackBuffer());
	imShader->SetVar(m_GlintDepthVar, CRenderTargets::GetDepthBuffer());

	const grcBufferUAV* UAVs[grcMaxUAVViews] = {
		&m_GlintAccumBuffer
	};	
	
	const grcRenderTarget* RenderTargets[grcmrtColorCount] = { CRenderTargets::GetBackBufferCopy() };

	UINT initialCounts[grcMaxUAVViews] = { 0 };
	textureFactory11.LockMRTWithUAV(RenderTargets, NULL, NULL, NULL, UAVs, initialCounts);

	AssertVerify(imShader->BeginDraw(grmShader::RMC_DRAW,true,m_GlintTechnique));
	imShader->Bind((int)pGlintGenerate);

	grcBeginQuads(1);
	grcDrawQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,color);
	grcEndQuads();

	imShader->UnBind();
	imShader->EndDraw();

	grcResolveFlags resolveFlags;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

	//Draw Glint Sprites

	projParams.x = m_GlintDepthScaleRange;
	imShader->SetVar(m_GlintParamsVar0, projParams);

	imShader->SetVar(m_GlintBackbufferLinearVar, m_pTexAnimorphicFlareDiffuse);

	// Copy the count from the bokeh point AppendStructuredBuffer to our indirect arguments buffer.
	// This lets us issue a draw call with number of vertices == the number of points in
	// the buffer, without having to copy anything back to the CPU.
	GRCDEVICE.CopyStructureCount( &m_GlintRenderBuffer, 0, &m_GlintAccumBuffer );

	imShader->SetVar(m_GlintPointBufferVar, &m_GlintAccumBuffer);

	AssertVerify(imShader->BeginDraw(grmShader::RMC_DRAW,true,m_GlintTechnique)); // MIGRATE_FIXME
	imShader->Bind((int)pGlintDrawSprites);

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Add);

	GRCDEVICE.DrawWithGeometryShader( &m_GlintRenderBuffer );

	imShader->UnBind();
	imShader->EndDraw();

	imShader->SetVar(m_GlintParamsVar0, Vector4(1.0, 1.0, 1.0, 0.0));
	imShader->SetVar(m_GlintParamsVar1, Vector4(0.0, 0.0, 0.0, 0.0));

	PIXEnd();
}
#endif //GLINTS_ENABLED

///////////////////////////////////////////////////////////////////////////////
//  UpdateVisualDataSettings
///////////////////////////////////////////////////////////////////////////////

void			CCoronas::UpdateVisualDataSettings			()
{
	if (g_visualSettings.GetIsLoaded())
	{
		g_coronas.m_sizeScaleGlobal							= g_visualSettings.Get("misc.coronas.sizeScaleGlobal", 1.0f);
		g_coronas.m_intensityScaleGlobal					= g_visualSettings.Get("misc.coronas.intensityScaleGlobal", 1.0f);
		g_coronas.m_intensityScaleWater						= g_visualSettings.Get("misc.coronas.intensityScaleWater", 5.0f);
		g_coronas.m_sizeScaleWater							= g_visualSettings.Get("misc.coronas.sizeScaleWater", 1.0f);
		g_coronas.m_sizeScaleParaboloid						= g_visualSettings.Get("misc.coronas.sizeScaleParaboloid", 2.0f);
		g_coronas.m_screenspaceExpansion					= g_visualSettings.Get("misc.coronas.screenspaceExpansion", 0.0f);
		g_coronas.m_screenspaceExpansionWater				= g_visualSettings.Get("misc.coronas.screenspaceExpansionWater", 0.0f);
		g_coronas.m_zBiasMultiplier							= g_visualSettings.Get("misc.coronas.zBiasMultiplier", 10.0f);
		g_coronas.m_zBiasDistanceNear						= g_visualSettings.Get("misc.coronas.zBiasDistanceNear", 20.0f);
		g_coronas.m_zBiasDistanceFar						= g_visualSettings.Get("misc.coronas.zBiasDistanceFar", 50.0f);
		g_coronas.m_zBiasMin								= g_visualSettings.Get("misc.coronas.zBias.min", 0.0f);
		g_coronas.m_zBiasMax								= g_visualSettings.Get("misc.coronas.zBias.max", 1.0f);
		g_coronas.m_zBiasAdditionalBiasMirrorReflection		= g_visualSettings.Get("misc.coronas.zBias.additionalBias.MirrorReflection", 0.0f);
		g_coronas.m_zBiasAdditionalBiasEnvReflection		= g_visualSettings.Get("misc.coronas.zBias.additionalBias.EnvReflection", 0.0f);
		g_coronas.m_zBiasAdditionalBiasWaterReflection		= g_visualSettings.Get("misc.coronas.zBias.additionalBias.WaterReflection", 0.0f);
		g_coronas.m_zBiasFromFinalSizeMult					= g_visualSettings.Get("misc.coronas.zBias.fromFinalSizeMultiplier", CORONAS_ZBIAS_FROM_FINALSIZE_MULT);
		g_coronas.m_occlusionSizeScale						= g_visualSettings.Get("misc.coronas.occlusionSizeScale", 0.5f);
		g_coronas.m_occlusionSizeMax						= g_visualSettings.Get("misc.coronas.occlusionSizeMax", 32.0f);

		float screenEdgeMinDistForFade						= g_visualSettings.Get("misc.coronas.screenEdgeMinDistForFade", CORONAS_MIN_SCREEN_EDGE_DIST_FOR_FADE);
		g_coronas.m_screenEdgeMinDistForFade				= Saturate(screenEdgeMinDistForFade);

		g_coronas.m_doNGCoronas								= g_visualSettings.Get("misc.coronas.m_doNGCoronas", 0.0f);
		g_coronas.m_rotationSpeed[0]						= g_visualSettings.Get("misc.coronas.m_rotationSpeed", 0.0f);
		g_coronas.m_rotationSpeed[1]						= g_visualSettings.Get("misc.coronas.m_rotationSpeedRampedUp", 0.0f);
		g_coronas.m_layer1Muliplier							= g_visualSettings.Get("misc.coronas.m_layer1Muliplier", 1.0f);
		g_coronas.m_layer2Muliplier							= g_visualSettings.Get("misc.coronas.m_layer2Muliplier", 1.0f);
		g_coronas.m_scaleRampUp								= g_visualSettings.Get("misc.coronas.m_scaleRampUp", 2.0f);
		g_coronas.m_scaleRampUpAngle						= g_visualSettings.Get("misc.coronas.m_scaleRampUpAngle", 30.0f);

		g_coronas.m_flareCoronaSizeRatio					= g_visualSettings.Get("misc.coronas.flareCoronaSizeRatio", FLARES_CORONAS_SIZE_RATIO);
		g_coronas.m_flareMinAngleDegToFadeIn				= g_visualSettings.Get("misc.coronas.flareMinAngleDegToFadeIn", FLARES_MIN_ANGLE_DEG_TO_FADE_IN);
		g_coronas.m_flareScreenCenterOffsetSizeMult			= g_visualSettings.Get("misc.coronas.flareScreenCenterOffsetSizeMult", FLARES_SCREEN_CENTER_OFFSET_SIZE_MULT);
		g_coronas.m_flareColShiftR							= g_visualSettings.Get("misc.coronas.flareColShiftR", FLARES_COLOUR_SHIFT_R);
		g_coronas.m_flareColShiftG							= g_visualSettings.Get("misc.coronas.flareColShiftG", FLARES_COLOUR_SHIFT_G);
		g_coronas.m_flareColShiftB							= g_visualSettings.Get("misc.coronas.flareColShiftB", FLARES_COLOUR_SHIFT_B);
		g_coronas.m_baseValueR								= g_visualSettings.Get("misc.coronas.baseValueR", 0.0f);
		g_coronas.m_baseValueG								= g_visualSettings.Get("misc.coronas.baseValueG", 0.0f);
		g_coronas.m_baseValueB								= g_visualSettings.Get("misc.coronas.baseValueB", 0.0f);

		g_coronas.m_dirMultFadeOffStart						= g_visualSettings.Get("misc.coronas.dir.mult.fadeoff.start", 200.0f);
		g_coronas.m_dirMultFadeOffDist						= g_visualSettings.Get("misc.coronas.dir.mult.fadeoff.dist", 50.0f);

		g_coronas.m_flareScreenCenterOffsetVerticalSizeMult	= g_visualSettings.Get("misc.coronas.flareScreenCenterOffsetVerticalSizeMult", FLARES_SCREEN_CENTER_OFFSET_VERTICAL_SIZE_MULT);
	
		g_coronas.m_underwaterFadeDist						= g_visualSettings.Get("misc.coronas.underwaterFadeDist", CORONAS_UNDERWATER_FADE_DISTANCE);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCoronas::InitWidgets					()
{
	char txt[128];

	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Coronas", false);
	{
		pVfxBank->AddTitle("INFO");
		sprintf(txt, "Num Registered (%d)", CORONAS_MAX);
		pVfxBank->AddSlider(txt, &m_numRegistered, 0, CORONAS_MAX, 0);
		pVfxBank->AddSlider("Num Rendered", &m_numRendered, 0, CORONAS_MAX, 0);
		pVfxBank->AddSlider("Num Flares Rendered", &m_numFlaresRendered, 0, CORONAS_MAX, 0);
		pVfxBank->AddSlider("Num Overflown", &m_numOverflown, 0, CORONAS_MAX, 0);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
	#if __DEV
		pVfxBank->AddSlider("Paraboloid Reflection Intensity", &CORONAS_PARABOLOID_REFLECTION_INTENSITY, 0.0f, 50.0f, 0.1f);
	#endif // __DEV
	
		pVfxBank->AddSlider("Global Scale", &m_sizeScaleGlobal, 0.0f, 10.0f, 0.1f);
		pVfxBank->AddSlider("Global Intensity", &m_intensityScaleGlobal, 0.0f, 10.0f, 0.1f);

		pVfxBank->AddSlider("ZBias Min", &m_zBiasMin, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("ZBias Max", &m_zBiasMax, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("Occlusion Size Scale", &m_occlusionSizeScale, 0.0f, 1.0f, 0.0001f);
				
		pVfxBank->AddSlider("Do NG Coronas", &m_doNGCoronas, 0.0f, 1.0f, 1.0f);
		pVfxBank->AddSlider("Rotation Speed", &m_rotationSpeed[0], 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("Rotation Speed Ramped", &m_rotationSpeed[1], 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("Layer 1 multiplier", &m_layer1Muliplier, 0.0f, 1.0f, 0.001f);
		pVfxBank->AddSlider("Layer 2 multiplier", &m_layer2Muliplier, 0.0f, 1.0f, 0.001f);
		pVfxBank->AddSlider("Scale ramp up", &m_scaleRampUp, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("Scale ramp up angle", &m_scaleRampUpAngle, 0.0f, 90.0f, 0.001f);

		pVfxBank->AddSlider("ZBias Water Reflection Extra Bias", &m_zBiasAdditionalBiasWaterReflection, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("ZBias Mirror Reflection Extra Bias", &m_zBiasAdditionalBiasMirrorReflection, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("ZBias Environment Reflection Extra Bias", &m_zBiasAdditionalBiasEnvReflection, 0.0f, 5.0f, 0.001f);

		pVfxBank->AddSlider("Underwater fade distance", &m_underwaterFadeDist, 0.0f, 4000.0f, 0.001f);

		pVfxBank->AddToggle("Disable Late Registration", &m_disableLateRegistration);
		
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG CORONA");
		pVfxBank->AddButton("Add", DebugAdd);
		pVfxBank->AddButton("Remove", DebugRemove);
		pVfxBank->AddButton("OverflowStart", DebugOverflowStart);
		pVfxBank->AddButton("OverflowEnd", DebugOverflowEnd);
		pVfxBank->AddSlider("Size", &m_debugCoronaSize, 0.0f, 15.0f, 0.1f);
		pVfxBank->AddSlider("Red", &m_debugCoronaColR, 0, 255, 1);
		pVfxBank->AddSlider("Green", &m_debugCoronaColG, 0, 255, 1);
		pVfxBank->AddSlider("Blue", &m_debugCoronaColB, 0, 255, 1);
		pVfxBank->AddSlider("Intensity", &m_debugCoronaIntensity, 0.0f, 255.0f, 0.5f);
		pVfxBank->AddToggle("Direction", &m_debugCoronaDir);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG FLARES");
		pVfxBank->AddToggle("Disable Render Flares", &m_disableRenderFlares);

		pVfxBank->AddSlider("Corona to Flare Size Ratio", &m_flareCoronaSizeRatio, 0.0f, 10.0f, 0.01f);
		pVfxBank->AddSlider("Flare Min. Angle To Fade In", &m_flareMinAngleDegToFadeIn, 0.0f, 90.0f, 0.5f);
		pVfxBank->AddSlider("Flare Screen Centre Offset Horizontal Size Multiplier", &m_flareScreenCenterOffsetSizeMult, 1.0f, 500.0f, 0.01f);
		pVfxBank->AddSlider("Flare Screen Centre Offset Vertical Size Multiplier", &m_flareScreenCenterOffsetVerticalSizeMult, 1.0f, 500.0f, 0.01f);

		pVfxBank->AddSlider("Flare Colour Shift Red",	&m_flareColShiftR, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Flare Colour Shift Green", &m_flareColShiftG, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Flare Colour Shift Blue",	&m_flareColShiftB, 0.0f, 1.0f, 0.01f);

		pVfxBank->AddSlider("Base Colour Red",	&g_coronas.m_baseValueR, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Base Colour Green", &g_coronas.m_baseValueG, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Base Colour Blue",	&g_coronas.m_baseValueB, 0.0f, 1.0f, 0.01f);

		pVfxBank->AddSlider("Directional multiplier fade-off start", &g_coronas.m_dirMultFadeOffStart, 0.0f, 1024.0f, 0.1f);
		pVfxBank->AddSlider("Directional multiplier fade-off distance", &g_coronas.m_dirMultFadeOffDist, 0.0f, 1024.0f, 0.1f);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Disable Render", &m_disableRender);
		pVfxBank->AddToggle("Render Debug", &m_renderDebug);

		pVfxBank->AddTitle("");
		pVfxBank->AddToggle("Force Render Simple", &m_debugCoronaForceRenderSimple);
		pVfxBank->AddToggle("Force Reflection", &m_debugCoronaForceReflection);
		pVfxBank->AddToggle("Force Omni", &m_debugCoronaForceOmni);
		pVfxBank->AddToggle("Force Exterior", &m_debugCoronaForceExterior);
		pVfxBank->AddSlider("Z Bias Scale", &m_debugCoronaZBiasScale, 0.0f, 100.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Z Bias From Final Size Multiplier", &m_zBiasFromFinalSizeMult, 0.0f, 2.0f, 0.01f);
		pVfxBank->AddToggle("Apply New Colour Shift", &m_debugApplyNewColourShift);

		pVfxBank->AddToggle("Override Dir Light Cone Angle", &m_debugOverrideDirLightConeAngle);
		pVfxBank->AddSlider("Dir Light Cone Angle Inner", &m_debugDirLightConeAngleInner, 0.0f, 90.0f, 0.1f);
		pVfxBank->AddSlider("Dir Light Cone Angle Outer", &m_debugDirLightConeAngleOuter, 0.0f, 90.0f, 0.1f);

		pVfxBank->AddSlider("Min. Screen Edge Distance For Fade", &m_screenEdgeMinDistForFade, 0.0f, 1.0f, 0.0001f);

	#if __DEV
		pVfxBank->AddSeparator();

		pVfxBank->PushGroup("Corona test (experimental)");
		{
			pVfxBank->AddToggle("Size adjust enabled"		, &DebugLights::m_debugCoronaSizeAdjustEnabled);
			pVfxBank->AddToggle("Size adjust override"		, &DebugLights::m_debugCoronaSizeAdjustOverride);
			pVfxBank->AddSlider("Size adjust far"			, &DebugLights::m_debugCoronaSizeAdjustFar, 0.0f, 256.0f, 1.0f/32.0f);
			pVfxBank->AddToggle("Apply near fade"			, &DebugLights::m_debugCoronaApplyNearFade);
			pVfxBank->AddToggle("Apply Zbias"				, &DebugLights::m_debugCoronaApplyZBias);
			pVfxBank->AddToggle("Use z bias multiplier"	    , &DebugLights::m_debugCoronaUseZBiasMultiplier);
			pVfxBank->AddSlider("Zbias multiplier"			, &DebugLights::m_debugCoronaZBiasMultiplier, 0.0, 16.0f, 0.1f);
			pVfxBank->AddSlider("Zbias near distance"	    , &DebugLights::m_debugCoronaZBiasDistanceNear, 0.0, 2048.0f, 0.1f);
			pVfxBank->AddSlider("Zbias far distance"	    , &DebugLights::m_debugCoronaZBiasDistanceFar, 0.0, 2048.0f, 0.1f);
			pVfxBank->AddToggle("Show occlusion"            , &DebugLights::m_debugCoronaShowOcclusion);
		}
		pVfxBank->PopGroup();
	#endif // __DEV
	}
	pVfxBank->PopGroup();

#if GLINTS_ENABLED
	pVfxBank->PushGroup("Glints", false);
	{
		pVfxBank->AddToggle("Enabled", &m_GlintsEnabled);
		pVfxBank->AddSlider("Brightness Threshold", &m_GlintBrightnessThreshold, 10.0f, 200.0f, 1.0f);
		pVfxBank->AddSlider("SizeX", &m_GlintSizeX, 0.0f, 1000.0f, 1.0f);
		pVfxBank->AddSlider("SizeY", &m_GlintSizeY, 0.0f, 1000.0f, 1.0f);
		pVfxBank->AddSlider("alpha scale divider", &m_GlintAlphaScale, 0.0f, 5000.0f, 1.0);
		pVfxBank->AddSlider("Depth scale range", &m_GlintDepthScaleRange, 0.0f, 1.0f, 0.001f);		
	}
	pVfxBank->PopGroup();
#endif //GLINTS_ENABLED
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  DebugAdd
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCoronas::DebugAdd					()
{
	Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

	g_coronas.m_debugCoronaActive = true;
	g_coronas.m_vDebugCoronaPos = vPlayerCoords + Vec3V(0.0f, 0.0f, 0.5f);
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  DebugRemove
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCoronas::DebugRemove				()
{
	g_coronas.m_debugCoronaActive = false;
}
#endif // __BANK



#if __BANK
void CCoronas::DebugOverflowStart()
{
	Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

	g_coronas.m_overflowCoronas = true;
	g_coronas.m_vDebugCoronaPos = vPlayerCoords + Vec3V(0.0f, 0.0f, 0.5f);
}

void CCoronas::DebugOverflowEnd()
{
	g_coronas.m_overflowCoronas = false;
}
#endif // __BANK
