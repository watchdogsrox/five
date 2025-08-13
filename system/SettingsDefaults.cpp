#include "grcore/config.h"
#if __WIN32PC && __D3D11

#include "diag/diagerrorcodes.h"
#include "grcore/device.h"

//#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
//#include "renderer/ParaboloidShadow.h"
//#include "renderer/SpotShadows.h"
//#include "system/systemInfo.h"

#include "grcore/d3dwrapper.h"
#include "grcore/resourcecache.h"
#include "grcore/adapter_d3d11.h"
#include "grcore/texture.h"
#include "grcore/texturefactory_d3d11.h"
#include "math/amath.h"

#include "system/SettingsDefaults.h"
#include "camera/viewports/ViewportManager.h"
#include "Peds/rendering/PedDamage.h"
#include "renderer/rendertargets.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Water.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/Mirrors.h"
#include "renderer/SSAO.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "renderer/Shadows/ParaboloidShadows.h"
#include "vfx/particles/PtFxManager.h"

#include "../../shader_source/Lighting/Shadows/cascadeshadows_common.fxh" // for CASCADE_SHADOWS_DO_SOFT_FILTERING

#include "frontend/MobilePhone.h"

#if NV_SUPPORT
#if __D3D11
#define NO_STEREO_D3D9
#define NO_STEREO_D3D10
#else
#define NO_STEREO_D3D10
#define NO_STEREO_D3D11
#endif // __D3D11

#include "../../3rdParty/NVidia/nvapi.h"
#include "../../3rdParty/NVidia/nvstereo.h"
#endif

#if ATI_SUPPORT
#include "../../3rdParty/amd/ADL_SDK_7.0/include/adl_sdk.h"
#endif

PARAM(MinPower, "Minimum Rating Considered Low End");
PARAM(MaxPower, "Maximum Rating Considered High End");
PARAM(SuperMaxPower, "Super Maximum Rating Considered Ultrahigh End");
PARAM(MinWidth, "Lowest Allowed Resolution (Width)");
PARAM(MinHeight, "Lowest Allowed Resolution (Height)");

XPARAM(highColorPrecision);
XPARAM(DX11Use8BitTargets);
XPARAM(UseDefaultResReflection);
XPARAM(dx11shadowres);
XPARAM(disableBokeh);

XPARAM(postfxusesFP16);

XPARAM(runscript);

NOSTRIP_XPARAM(availablevidmem);

 namespace rage
{
 	NOSTRIP_XPARAM(width);
	NOSTRIP_XPARAM(height);
}

namespace SmokeTests
{
	XPARAM(smoketest);
};

s64 totalTextureSize = 0;
extern float g_fPercentForStreamer;

#if __DEV
fiStream* SettingsDefaults::m_pLoggingStream = NULL;
#endif // __DEV

#if RSG_PC
namespace PostFX
{
	extern bool	g_EnableFilmEffect;
};
#endif // RSG_PC

float SettingsDefaults::sm_fMaxLodScaleMinRange = 1.0f;
float SettingsDefaults::sm_fMaxLodScaleMaxRange = 1.75f;

float SettingsDefaults::sm_fPedVarietyMultMinRange = 1.0f;
float SettingsDefaults::sm_fPedVarietyMultMaxRange = 6.0f;
float SettingsDefaults::sm_fVehVarietyMultMinRange = 1.0f;
float SettingsDefaults::sm_fVehVarietyMultMaxRange = 6.0f;

// Minimum System Hardware Requirements
const int MINREQ_CPU_CORE_COUNT			= 4;
const s64 MINREQ_AMD_CPU_FREQUENCY		= 2400000;
const s64 MINREQ_INTEL_CPU_FREQUENCY	= 2300000;
const u64 MINREQ_PHYSICAL_MEMORY		= ((u64)4 * 1024 * 1024 * 1024) - ((u64)500 * 1024 * 1024);	// 4 GB minus slack as queries always return less
const s64 MINREQ_VIDEO_MEMORY			= (1024 * 1024 * 1024) - (80 * 1024 * 1024);				// 1024 MB minus slack as queries always return less

bool renderTargetStereoMode = false;
static int iTargetAdapter = 0;

#if RSG_PC
void applySceneScale(int scaleMode, int& iWidth, int& iHeight)
{
	int scaleNumer,scaleDenom;
	VideoResManager::GetSceneScale(scaleMode,scaleNumer,scaleDenom);
	iWidth  *= scaleNumer;
	iHeight *= scaleNumer;
	iWidth  /= scaleDenom;
	iHeight /= scaleDenom;	
}
#endif

void CreateRenderTarget(const char * DEV_ONLY(pName), grcRenderTargetType , int nWidth, int nHeight, int nBitsPerPixel, grcTextureFactory::CreateParams *params = NULL)
{
	if (((grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetGPUCount() == 1 && renderTargetStereoMode)
	{
		if (params && params->StereoRTMode != grcDevice::MONO)
			nWidth *= 2;
	}
#if __DEV
	if (SettingsDefaults::GetLoggingStream())
	{
		char theText[200];
		formatf(theText, 200, "%s width: %d height: %d bits: %d multisample: %d\r\n", pName, nWidth, nHeight, nBitsPerPixel, (params ? (params->Multisample ? params->Multisample : 1) : 1));
		SettingsDefaults::GetLoggingStream()->Write(theText, (int)strlen(theText));
	}
#endif
	s64 textureSize = nWidth * nHeight * (nBitsPerPixel / 8) * (params ? (params->Multisample ? params->Multisample : 1) : 1);
	const u64 RequiredAlignment =  (nWidth < 256 || nHeight < 256) ? D3DRUNTIME_MEMORYALIGNMENT : D3D_AMD_LARGEALIGNMENT;
	textureSize = (textureSize + (RequiredAlignment - 1)) & ~(RequiredAlignment - 1);

	totalTextureSize += textureSize;
	if (params && params->Lockable && !params->Multisample)
	{
		totalTextureSize += textureSize;
	}
}


void calculateRenderTargetsSize(Settings &settings)
{
	int iWidth		= settings.m_video.m_ScreenWidth;
	int iHeight		= settings.m_video.m_ScreenHeight;
#if RSG_PC
	applySceneScale(settings.m_graphics.m_SamplingMode, iWidth, iHeight);
#endif
	int UIWidth		= settings.m_video.m_ScreenWidth;
	int UIHeight	= settings.m_video.m_ScreenHeight;

	grcTextureFactory::CreateParams params;
	// color
	params.Multisample = 0;
	u32 bpp = 64;
	if (!PARAM_highColorPrecision.Get())
	{
		bpp = 32;
	}

#if RSG_DURANGO && LIGHT_VOLUME_USE_LOW_RESOLUTION
	grcTextureFactory::CreateParams volParams;
	volParams.Multisample = 0;
	volParams.UseFloat = true;
	volParams.Format = grctfA16B16G16R16F; // up-sampling & reconstruction optimizations require that we have an alpha channel  
	const int nVolumeBPP = 64;
	const int nVolWidth = iWidth/2;
	const int nVolHeight =iHeight/2;

	CreateRenderTarget("Volume Lights Offscreen buffer", grcrtPermanent, nVolWidth, nVolHeight, nVolumeBPP, &volParams);
	CreateRenderTarget("Volume Lights Reconstruction buffer", grcrtPermanent, nVolWidth, nVolHeight, nVolumeBPP, &volParams);
#endif

	// set render target descriptions
	params.Multisample = 0;
	bpp = 64;

#if !__FINAL && (__D3D11 || RSG_ORBIS)
	if( PARAM_DX11Use8BitTargets.Get() )
	{
		bpp = 32;
	}
	else
#endif // !__FINAL
	{
		bpp = 64;
		// Back Buffer needs alpha but BackBUfferCopy only needs alpha for fxaa
	}


	grcDevice::MSAAMode multisample = settings.m_graphics.m_MSAA;
	params.Multisample = multisample;

	CreateRenderTarget("BackBuffer", grcrtPermanent, iWidth, iHeight, bpp, &params);

#if DEVICE_MSAA
	if(multisample)
	{
		CreateRenderTarget("BackBufferCopyAA", grcrtPermanent, iWidth, iHeight, bpp, &params);
		grcTextureFactory::CreateParams ResolvedParams = params;
		ResolvedParams.Multisample = grcDevice::MSAA_None;
		CreateRenderTarget("BackBuffer_Resolved", grcrtPermanent, iWidth, iHeight, bpp, &ResolvedParams);
	}
#endif // DEVICE_MSAA

	if (!multisample)
		CreateRenderTarget("BackBufferCopy", grcrtPermanent, iWidth, iHeight, bpp, &params);

	params.UseHierZ = true;
	params.Format = grctfD32FS8;
	params.Lockable = 0;
	int depthBpp	= 40;

	// DX11: TODO: Could set this up so the lockable textures are created on first use.
	// Current method creates extra lockable render targets with associated extra memory usage
	ENTITYSELECT_ONLY(params.Lockable = __BANK);	// In __BANK these are lockable as they're used for entity select.

	//We need a copy of the depth buffer pre alpha for the depth of field through windows, etc...
	CreateRenderTarget("DepthBuffer Pre Alpha",	grcrtDepthBuffer, iWidth, iHeight, depthBpp, &params);

	params.Multisample			= multisample;
	CreateRenderTarget("DepthBuffer",	grcrtDepthBuffer, iWidth, iHeight, depthBpp, &params);
#if RSG_PC
	if (VideoResManager::IsSceneScaled()) 
		CreateRenderTarget("UIDepthBuffer",	grcrtDepthBuffer, UIWidth, UIHeight, depthBpp, &params);
#endif

	if( GRCDEVICE.IsReadOnlyDepthAllowed() )
	{
		// no need for the copy, we use read-only depth instead
//		ms_DepthBufferCopy = textureFactory11.CreateRenderTarget("DepthBufferCopy (read-only)", depthObjectMSAA, params.Format, DepthReadOnly WIN32PC_ONLY(, ms_DepthBufferCopy) DURANGO_ONLY(, ms_DepthBufferCopy));
	}else
	{
		CreateRenderTarget("DepthBufferCopy",	grcrtDepthBuffer, iWidth, iHeight, depthBpp, &params);
	}

#if DEVICE_MSAA
	if (multisample)
	{
		grcTextureFactory::CreateParams ResolvedParams = params;
		ResolvedParams.Multisample = grcDevice::MSAA_None;
		CreateRenderTarget("DepthBuffer_Resolved", grcrtPermanent, iWidth, iHeight, depthBpp, &ResolvedParams);
#if RSG_PC
		if (VideoResManager::IsSceneScaled()) 
			CreateRenderTarget("UIDepthBuffer_Resolved", grcrtPermanent, UIWidth, UIHeight, depthBpp, &ResolvedParams);
#endif

		if( GRCDEVICE.IsReadOnlyDepthAllowed())
		{
//			ms_DepthBuffer_Resolved_ReadOnly = textureFactory11.CreateRenderTarget("DepthBuffer_Resolved (read-only)", depthObject, ResolvedParams.Format, DepthReadOnly WIN32PC_ONLY(, ms_DepthBuffer_Resolved_ReadOnly) DURANGO_ONLY(, ms_DepthBuffer_Resolved_ReadOnly));
		}
	}
#endif // DEVICE_MSAA

	grcTextureFactory::CreateParams depthParams;
	CreateRenderTarget("Depth Quarter", grcrtDepthBuffer, iWidth/2, iHeight/2, 32, &depthParams);

	//R32F Version
	depthParams.StereoRTMode	= grcDevice::STEREO;
	CreateRenderTarget("Depth Quarter Linear", grcrtPermanent, iWidth/2, iHeight/2, 32, &depthParams);

#if	DEVICE_MSAA
	grcTextureFactory::CreateParams osbparams;

#if RSG_DURANGO
	osbparams.Multisample			= multisample;
	CreateRenderTarget("OffscreenBuffer1",	grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams);
#endif // RSG_DURANGO
	osbparams.Multisample	= multisample;
	CreateRenderTarget("OffscreenBuffer2",	grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams);
	osbparams.Multisample	= 0;
	CreateRenderTarget("UI Buffer",			grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams);

	osbparams.Multisample = multisample;

	grcTextureFactory::CreateParams resolvedParams = osbparams;
	resolvedParams.Multisample			= grcDevice::MSAA_None;

	if (settings.m_graphics.m_MSAA) {
		CreateRenderTarget("OffscreenBuffer1_Resolved",	grcrtPermanent,	UIWidth, UIHeight, 32, &resolvedParams);
		CreateRenderTarget("OffscreenBuffer2_Resolved",	grcrtPermanent,	UIWidth, UIHeight, 32, &resolvedParams);
	}
#endif // DEVICE_MSAA

//	ms_DepthBuffer_Stencil = textureFactory11.CreateRenderTarget( "StencilBuffer", depthObjectMSAA, eStencilFormat, DepthStencilRW WIN32PC_ONLY(, ms_DepthBuffer_Stencil) );
//	ms_DepthBufferCopy_Stencil = textureFactory11.CreateRenderTarget( "StencilBufferCopy",	depthCopy ? depthCopy->GetTexturePtr() : NULL, eStencilFormat );
//	ms_DepthBuffer_ReadOnly = textureFactory11.CreateRenderTarget( "DepthBufferReadOnly", depthObjectMSAA, params.Format, DepthStencilReadOnly WIN32PC_ONLY(, ms_DepthBuffer_ReadOnly) );

#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
//	grcTextureFactory::CreateParams params;

	const int ptfxDepthbpp = 16;
	const grcTextureFormat ptfxDepthFormat = grctfR16F;
		
	const int ptfxAlphabpp = 8;
	const grcTextureFormat ptfxAlphaFormat = grctfL8;
	const grcRenderTargetType type = grcrtPermanent;

	multisample = GRCDEVICE.GetMSAA();
	params.Multisample = multisample;
#if DEVICE_EQAA
	multisample.DisableCoverage();
	params.ForceFragmentMask = false;
	params.SupersampleFrequency = multisample.m_uFragments;
#else // DEVICE_EQAA
	u32 multisampleQuality = GRCDEVICE.GetMSAAQuality();
	params.MultisampleQuality = (u8)multisampleQuality;
#endif // DEVICE_EQAA

	//Create as an MSAA render target as its bound as a secondary target alongside the MSAA backbuffer. May be able to get around this on PS4 and Xbox?
	//Also using RGBA format even though only need R and A channels. Needs to be R and A as different blends to both, isnt a target that has just R and A, might be better having 2 targets.
	//added bug url:bugstar:1852743 to look at this.
	params.Format = ptfxDepthFormat;
	CreateRenderTarget("PARTICLE_DEPTH_MAP", type, iWidth, iHeight, ptfxDepthbpp, &params);

	params.Format = ptfxAlphaFormat;
	CreateRenderTarget("PARTICLE_ALPHA_MAP", type, iWidth, iHeight, ptfxAlphabpp, &params);
	}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

}

void calculateGBufferRenderTargetSize(Settings &settings)
{
	int iWidth		= settings.m_video.m_ScreenWidth;
	int iHeight		= settings.m_video.m_ScreenHeight;
#if RSG_PC
	applySceneScale(settings.m_graphics.m_SamplingMode, iWidth, iHeight);
#endif
	u32 bitsPerPixel = 32;

	grcTextureFactory::CreateParams Params;
	grcDevice::MSAAMode multisample = settings.m_graphics.m_MSAA;
	Params.Multisample = multisample;

#if DEVICE_EQAA
	bool needFmask = GRCDEVICE.IsEQAA();
	Params.ForceFragmentMask = needFmask;
	Params.SupersampleFrequency = multisample.m_uFragments;
#else // DEVICE_EQAA
	u32 multisampleQuality = GRCDEVICE.GetMSAAQuality();
	Params.MultisampleQuality = (u8)multisampleQuality;
#endif // DEVICE_EQAA
	CreateRenderTarget("GBUFFER_0", grcrtPermanent, iWidth, iHeight, bitsPerPixel, &Params);
	CreateRenderTarget("GBUFFER_1", grcrtPermanent, iWidth, iHeight, bitsPerPixel, &Params);
	CreateRenderTarget("GBUFFER_3", grcrtPermanent, iWidth, iHeight, bitsPerPixel, &Params);
	CreateRenderTarget("GBUFFER_2", grcrtPermanent, iWidth, iHeight, bitsPerPixel, &Params);
}

void calculateCascadeShadowsRenderTargetSize(Settings &settings)
{
		s32 iWidth	= settings.m_video.m_ScreenWidth;
		s32 iHeight	= settings.m_video.m_ScreenHeight;
#if RSG_PC
		applySceneScale(settings.m_graphics.m_SamplingMode, iWidth, iHeight);
#endif
		int res = 0;
		if (!PARAM_dx11shadowres.Get(res))
		{
			#define SHADOW_RES_BASELINE 256
			res = settings.m_graphics.m_ShadowQuality >=3 && settings.m_graphics.m_UltraShadows_Enabled ? 4096 : SHADOW_RES_BASELINE << settings.m_graphics.m_ShadowQuality;
			if (settings.m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0)) res = 1;
		}

		const int shadowResX = res;
#if CASCADE_SHADOW_TEXARRAY
		const int shadowResY = res;
#else
		const int shadowResY = res*CASCADE_SHADOWS_COUNT;
#endif

		if (1) // create depth target (shadow texture)
		{
			grcTextureFactory::CreateParams params;
			int bpp = 32;

			const grcRenderTargetType type = (RSG_PC || RSG_DURANGO /*|| RSG_ORBIS*/) ? grcrtPermanent : grcrtShadowMap;

#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 4;
			params.PerArraySliceRTV = CRenderPhaseCascadeShadowsInterface::IsInstShadowEnabled() ? 0 : params.ArraySize;
#endif

			CreateRenderTarget("SHADOW_MAP_CSM_DEPTH", type, shadowResX, shadowResY, bpp, &params);

#if RMPTFX_USE_PARTICLE_SHADOWS
			//			m_shadowMapReadOnly = textureFactory11.CreateRenderTarget("SHADOW_MAP_CSM_DEPTH", m_shadowMapBase->GetTexturePtr(), params.Format, DepthStencilReadOnly, NULL, &params);
#endif // RMPTFX_USE_PARTICLE_SHADOWS && __D3D11

#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 1;
			params.PerArraySliceRTV = 0;
#endif

			const int resX = CASCADE_SHADOWS_RES_MINI_X;
			const int resY = CASCADE_SHADOWS_RES_MINI_Y;

			if (resX != 0 && resY != 0)
			{
				grcTextureFactory::CreateParams params;
				int bpp = 0;
				bpp             = 32;

				const grcRenderTargetType type = PS3_SWITCH(grcrtShadowMap, grcrtPermanent);

				CreateRenderTarget("SHADOW_MAP_CSM_MINI", type, resX, resY, bpp, &params);

				CreateRenderTarget("SHADOW_MAP_CSM_MINI 2", type, resX, resY * 4, bpp, &params);
			}
		}

#if 0 //CSM_USE_DUMMY_COLOUR_TARGET
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 32;
			const grcRenderTargetType type = grcrtPermanent;

			CreateRenderTarget("SHADOW_MAP_CSM_DUMMY", type, shadowResX, shadowResY, bpp, &params);
		}
#endif // CSM_USE_DUMMY_COLOUR_TARGET

#if RMPTFX_USE_PARTICLE_SHADOWS
		if (settings.m_graphics.m_ShadowQuality != (CSettings::eSettingsLevel) (0))
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 32;
			const grcRenderTargetType type = grcrtPermanent;
#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 4;
			params.PerArraySliceRTV = CRenderPhaseCascadeShadowsInterface::IsInstShadowEnabled() ? 0 : params.ArraySize;
#endif
			CreateRenderTarget("SHADOW_MAP_PARTICLES", type, shadowResX, shadowResY, bpp, &params);
#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 1;
			params.PerArraySliceRTV = 0;
#endif
		}
#endif // RMPTFX_USE_PARTICLE_SHADOWS

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 32; // could be 32 or 16
			const grcRenderTargetType type = grcrtPermanent;

			CreateRenderTarget("SHADOW_MAP_CSM_ENTITY_ID", type, shadowResX, shadowResY, bpp, &params);
		}
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
		if ((settings.m_graphics.m_ShaderQuality != CSettings::Low) && (settings.m_graphics.m_Shadow_SoftShadows != CSettings::Low) &&
			(settings.m_graphics.m_Shadow_SoftShadows != CSettings::Custom) && (settings.m_graphics.m_Shadow_SoftShadows != CSettings::Special))
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 8;
			params.Multisample = GRCDEVICE.GetMSAA();
# if DEVICE_EQAA
			params.Multisample.DisableCoverage();
# else 
			params.MultisampleQuality = (u8)GRCDEVICE.GetMSAAQuality();
# endif // DEVICE_EQAA

			const grcRenderTargetType type = grcrtPermanent;

			CreateRenderTarget("SHADOW_MAP_CSM_SOFT_SHADOW_INTERMEDIATE1", type, iWidth, iHeight, bpp, &params);

			params.Multisample = 0;

#if DEVICE_MSAA			
			CreateRenderTarget("SHADOW_MAP_CSM_SOFT_SHADOW_INTERMEDIATE1_RESOLVED", type, iWidth, iHeight, bpp, &params);
#endif	//DEVICE_MSAA

 			CreateRenderTarget("SHADOW_MAP_CSM_SOFT_SHADOW_EARLY_OUT1", type, iWidth/8, iHeight/8, bpp, &params);
// 
 			CreateRenderTarget("SHADOW_MAP_CSM_SOFT_SHADOW_EARLY_OUT2", type, iWidth/8, iHeight/8, bpp, &params);
		}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

		{
			#define smoothStepRTSize 128
			grcTextureFactory::CreateParams params;
			params.Format    = grctfL8;

			CreateRenderTarget("Smooth Step RT", grcrtPermanent, smoothStepRTSize, 1, 8, &params);
		}
}

void calcualteUIRenderTargetSize(Settings &settings)
{
	int rt_Width = settings.m_video.m_ScreenWidth / 2;
	int rt_Height = settings.m_video.m_ScreenHeight / 2;

	grcTextureFactory::CreateParams params;

	// Depth buffer
	params.Format		= grctfD24S8;
	params.IsResolvable = true;
	params.Multisample	= 0;
	params.HasParent	= true;
	params.Parent		= NULL;
	CreateRenderTarget("UI Depth Buffer", grcrtDepthBuffer, rt_Width, rt_Height, 32, &params);

	// Readonly
#if RSG_PC
	// GBuffer 0
	params.IsResolvable = false;
	params.HasParent	= true;

	if(GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		CreateRenderTarget("UI Depth Buffer Only", grcrtDepthBuffer, rt_Width, rt_Height, 32, &params);
	} else
		CreateRenderTarget("UI Depth Buffer Copy",	grcrtDepthBuffer, rt_Width, rt_Height, 32, &params);

	params.IsResolvable = true;
#endif // RSG_PC

	params.Format		= grctfA8R8G8B8;
	params.HasParent	= true;
	params.Parent		= NULL;
	CreateRenderTarget("UI Depth Buffer Alias", grcrtPermanent, rt_Width, rt_Height, 32, &params);

	// Light buffer
#if RSG_PC
	if (settings.m_graphics.m_ShaderQuality == (CSettings::eSettingsLevel) (0))
		params.Format		= grctfA2B10G10R10ATI;
	else
		params.Format		= grctfA16B16G16R16F;
#else
	params.Format		= grctfA16B16G16R16F;
#endif
	params.UseFloat		= true;
	params.IsResolvable = true;
	params.HasParent	= true;

	CreateRenderTarget("UI Deferred Light Buffer", grcrtPermanent, rt_Width, rt_Height, params.Format == grctfA16B16G16R16F ? 64:32, &params); 

#if RSG_PC
	CreateRenderTarget("UI Deferred Light Buffer Copy", grcrtPermanent, rt_Width, rt_Height, 64, &params); 
#endif // RSG_PC

	// Composite Buffer 0
	params.Format		= grctfA8R8G8B8;
	params.UseFloat		= false;
	params.IsSRGB		= false;

	CreateRenderTarget("UI Deferred Composite Buffer", grcrtPermanent, rt_Width, rt_Height, 32, &params);

	// Composite Buffer 1
	CreateRenderTarget("UI Deferred Composite Lum Buffer",	grcrtPermanent, rt_Width, rt_Height, 32, &params);
#if RSG_PC
	CreateRenderTarget("UI Deferred Composite Lum Buffer Copy",	grcrtPermanent, rt_Width, rt_Height, 32, &params);
#endif // RSG_PC

	// GBuffer 0
	CreateRenderTarget("UI Deferred Buffer 0",	grcrtPermanent, rt_Width, rt_Height, 32, &params); 

	// GBuffer 1
	CreateRenderTarget("UI Deferred Buffer 1",	grcrtPermanent, rt_Width, rt_Height, 32, &params); 

	// GBuffer 2
	CreateRenderTarget("UI Deferred Buffer 2",	grcrtPermanent, rt_Width, rt_Height, 32, &params); 

	// GBuffer 3
	CreateRenderTarget("UI Deferred Buffer 3",	grcrtPermanent, rt_Width, rt_Height, 32, &params); 

}

void calculateParaboloidShadowRenderTargetsSize(Settings &settings)
{
	int res;
	if (settings.m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
		res = 1;
	else
		res = (settings.m_graphics.m_ShadowQuality < 2) ? 256 : 512;

	int bpp = 16;

	{
		grcTextureFactory::CreateParams params;

		params.Multisample  = grcDevice::MSAA_None;

#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
		char name[256];
		for (int i = 0; i < MAX_CACHED_PARABOLOID_SHADOWS; i++)
		{
			int cmRes = Max(res/(CParaboloidShadow::IsCacheEntryHighRes(i)?1:2), 1);

			formatf(name,256,"POINT_SHADOW_%02d",i);

			CreateRenderTarget(name, grcrtCubeMap, cmRes, cmRes, bpp, &params);	

			// extra one that is 2x for spot light shadows, this should over lap the 6, but I don't know how to do that on win32
			formatf(name,256,"SPOT_SHADOW_%02d",i);
			CreateRenderTarget(name, grcrtPermanent, cmRes*2, cmRes*3, bpp, &params);	
		}
#else
		char nameBuffer[256];

		int loRes = Max(1, res/2);

		u32 count = MAX_STATIC_CACHED_LORES_PARABOLOID_SHADOWS+MAX_ACTIVE_LORES_PARABOLOID_SHADOWS+MAX_ACTIVE_LORES_OVERLAPPING;
		for (int i=0;i<count;i++)
		{
			params.ArraySize = (u8)6;
			params.PerArraySliceRTV = params.ArraySize;

			formatf(nameBuffer, 256, "POINT_SHADOW_%s_%d","LORES",i);

			CreateRenderTarget(nameBuffer, RSG_ORBIS?grcrtDepthBufferCubeMap:grcrtCubeMap, loRes, loRes, bpp, &params);	

			// TODO: this could overlap with the cubemap version above
			params.ArraySize = (u8)1;
			params.PerArraySliceRTV = params.ArraySize;

			formatf(nameBuffer, 256, "SPOT_SHADOW_%s_%d","LORES",i);

			CreateRenderTarget(nameBuffer, RSG_ORBIS?grcrtShadowMap:grcrtPermanent, loRes*2, loRes*3, bpp, &params);	
		}


		count = MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS+MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS;
		for (int i=0;i<count;i++)
		{
			params.ArraySize = (u8)6;
			params.PerArraySliceRTV = params.ArraySize;

			formatf(nameBuffer, 256, "POINT_SHADOW_%s_%d","HIRES",i);

			CreateRenderTarget(nameBuffer, RSG_ORBIS?grcrtDepthBufferCubeMap:grcrtCubeMap, res, res, bpp, &params);	

			// TODO: this could overlap with the cubemap version above
			params.ArraySize = (u8)1;
			params.PerArraySliceRTV = params.ArraySize;

			formatf(nameBuffer, 256, "SPOT_SHADOW_%s_%d","HIRES",i);

			CreateRenderTarget(nameBuffer, RSG_ORBIS?grcrtShadowMap:grcrtPermanent, res*2, res*3, bpp, &params);	
		}
#endif
	}
}

void calculateSSAORenderTargetsSize(Settings &settings)
{
#define SSOAPositionSize 16
#define SSAODitherSize 32
	grcTextureFactory::CreateParams params;
	CreateRenderTarget(	"SSAO Dither", grcrtPermanent, SSAODitherSize, SSAODitherSize, 32, &params);

	CreateRenderTarget(	"SSAO Position", grcrtPermanent, SSOAPositionSize, SSOAPositionSize, 64, &params);

	s32 width	= settings.m_video.m_ScreenWidth;
	s32 height	= settings.m_video.m_ScreenHeight;
#if RSG_PC
	applySceneScale(settings.m_graphics.m_SamplingMode, width, height);
#endif

#if __WIN32PC
	params.StereoRTMode = grcDevice::STEREO;
#endif
	CreateRenderTarget(	"SSAO 0", grcrtPermanent, width/2, height/2, 8, &params);
	CreateRenderTarget(	"SSAO 1", grcrtPermanent, width/2, height/2, 8, &params);

	CreateRenderTarget(	"CP/QS mix SSAO final", grcrtPermanent, width, height, 8, &params);

#if SSAO_USE_LINEAR_DEPTH_TARGETS
	params.Format				= grctfR32F;

	params.StereoRTMode = grcDevice::MONO;

	CreateRenderTarget( "SSAO Full Screen linear", grcrtPermanent, width, height, 32, &params);

#endif // SSAO_USE_LINEAR_DEPTH_TARGETS


#if SUPPORT_MRSSAO

	if (__DEV)
	{
		grcTextureFactory::CreateParams params;
		params.HasParent		= true;
		params.Parent			= NULL;
		params.Format			= grctfR32F;

		grcTextureFactory::CreateParams paramsNormalTexture;
		paramsNormalTexture.HasParent	= true;
		paramsNormalTexture.Parent		= NULL;
		paramsNormalTexture.Format		= MR_SSAO_USE_PACKED_NORMALS  ? grctfR32F : grctfA8R8G8B8;

		const unsigned screenWidth	= width;
		const unsigned screenHeight	= height;

		for(int i=0; i<MRSSAO_NUM_RESOLUTIONS; i++)
		{
			char DepthName[256];
			char NormalName[256];
			char AOName[256];
			char BlurredAOName[256];
			char PosXName[256];
			char PosYName[256];

			sprintf(DepthName, "SSAO Viewspace positions LOD %d", i);
			sprintf(NormalName, "SSAO Normal LOD %d", i);
			sprintf(AOName, "SSAO AO LOD %d", i);
			sprintf(BlurredAOName, "Blurred SSAO AO LOD %d", i);
			sprintf(PosXName, "SSAO Position X LOD %d", i);
			sprintf(PosYName, "SSAO Position Y LOD %d", i);

			if(i==0)
			{
			}
			else
			{
				params.Format = grctfR32F;

				if(i!=1)
				{
					CreateRenderTarget(DepthName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);
				}
				else
				{
					// Use the console half-size depth buffer.
				}

				CreateRenderTarget(NormalName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &paramsNormalTexture);

				CreateRenderTarget(PosXName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);

				CreateRenderTarget(PosYName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);
			}

	#ifdef MR_SSAO_USE_FULL_COLOUR_TARGETS
			params.Format = grctfA8R8G8B8;
	#else
			params.Format = grctfL8;
	#endif

			if(i==0)
			{
	#define MR_SSAO_FORCE_MAX_LOD1 0
	#if MR_SSAO_FORCE_MAX_LOD1
				m_AOs[i] = NULL;
				m_BlurredAOs[i] = NULL;
	#else //MR_SSAO_FORCE_MAX_LOD1

				CreateRenderTarget(AOName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);

	#define MR_SSAO_BLUR_TO_TARGET 1
	#if !MR_SSAO_BLUR_TO_TARGET || MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
				CreateRenderTarget(BlurredAOName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);
	#endif // !MR_SSAO_BLUR_TO_TARGET || MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE

	#endif //MR_SSAO_FORCE_MAX_LOD1
			}
			else
			{
				CreateRenderTarget(AOName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);

				CreateRenderTarget(BlurredAOName, grcrtPermanent, screenWidth >> i, screenHeight >> i, 32, &params);
			}
		}
	}
#endif //SUPPORT_MRSSAO


	if (__DEV)
	{
		int HDAOwidth	= width >> 1;
		int HDAOheight	= height >> 1;
		int bpp = 32;
		params.HasParent	= true;
		params.Parent		= NULL;
		params.Format		= grctfR11G11B10F;
		CreateRenderTarget( "HDAO2_Normal",	grcrtPermanent, HDAOwidth, HDAOheight, bpp, &params);
		params.Format		= SSAO_OUTPUT_DEPTH ? grctfD24S8 : grctfR32F;
		CreateRenderTarget( "HDAO2_Depth",	SSAO_OUTPUT_DEPTH ? grcrtDepthBuffer : grcrtPermanent, HDAOwidth, HDAOheight, bpp, &params);

		params.Format		= grctfA8R8G8B8;
		if (settings.m_graphics.m_MSAA > 1) CreateRenderTarget( "HDAO2_GBuffer1_Resolved",	grcrtPermanent, width, height, bpp, &params);

		if (settings.m_graphics.m_MSAA>1 && HDAO2_MULTISAMPLE_FILTER)
		{
			params.Format		= grctfA8R8G8B8;
			bpp = 32;
		}else
		{
			params.Format		= grctfL8;
			bpp = 8;
		}

		params.UseAsUAV		= true;
		CreateRenderTarget( "HDAO2_Result",	grcrtPermanent, HDAOwidth, HDAOheight, bpp, &params);
		CreateRenderTarget( "HDAO2_Temp1",	grcrtPermanent, width, height, bpp, &params);
		CreateRenderTarget( "HDAO2_Temp2",	grcrtPermanent, width, height, bpp, &params);
	}

#if SUPPORT_HBAO
	if (SSAO::UseNextGeneration(settings))
	{
		grcTextureFactory::CreateParams params;
		params.HasParent				= true;
	//	params.Parent					= GBuffer::GetTarget(GBUFFER_RT_3);
		params.AllocateFromPoolOnCreate	= false;
		params.Format			= grctfL8;
		CreateRenderTarget(	"SSAO Continuity mask", grcrtPermanent, width/2, height/2, 8, &params);
		CreateRenderTarget(	"SSAO Continuity mask Dilated", grcrtPermanent, width/2, height/2, 8, &params);
		params.Format			= grctfG16R16F;

		if (((grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetGPUCount() == 1)
		{
			CreateRenderTarget(	"SSAO History", grcrtPermanent, width/2, height/2, 32, &params);
		}
	}
#endif
}

int GetMirrorResolutionSetting(Settings &settings) {
	//	#define MIRROR_REFLECTION_RES_BASELINE 128
	//	return MIRROR_REFLECTION_RES_BASELINE << CSettingsManager::GetInstance().GetSettings().m_graphics.m_ReflectionQuality;
	/*int reflectionQuality = settings.m_graphics.m_ReflectionQuality;

	int reflectionSizeX = Max(32, settings.m_video.m_ScreenWidth / (1 << (3 - Min(3, reflectionQuality))));
	int reflectionSizeY = Max(32, settings.m_video.m_ScreenHeight / (1 << (3 - Min(3, reflectionQuality))));
	u32 dimension = (u32) rage::Sqrtf((float)(reflectionSizeX * reflectionSizeY));
	u32 iMipLevels = 0;
	while (dimension)
	{
		dimension >>= 1;
		iMipLevels++;
	}
	iMipLevels--;

	return 1 << iMipLevels;
	*/
	bool s_requirePowerOf2 = false;

	int reflectionQuality = settings.m_graphics.m_ReflectionQuality;
	int reflectionSize = Max(32, (settings.m_video.m_ScreenWidth+settings.m_video.m_ScreenHeight) >> (4 - Min(3, reflectionQuality)));

	if (s_requirePowerOf2)
	{
		u32 closestPow2Size = 32;
		while (reflectionSize > 3*closestPow2Size/2)
			closestPow2Size <<= 1;
		return closestPow2Size;
	}else
	{
		return reflectionSize & ~0xF;
	}

}

void calculateMirrorRenderTargetSizes(Settings &settings)
{
#define MIRROR_RT_NAME "MIRROR_RT"
#define MIRROR_DT_NAME "MIRROR_DT"
#define MIRROR_DT_NAME_RO "MIRROR_DT_READ_ONLY"
#define MIRROR_DT_NAME_COPY "MIRROR_DT_COPY"
	grcTextureFactory::CreateParams params;

	const int mirrorResX = GetMirrorResolutionSetting(settings);
	const int mirrorResY = mirrorResX / 2;

	int tbpp       = 64;
	if (!PARAM_highColorPrecision.Get())
	{
		tbpp = 32;
	}

	CreateRenderTarget(MIRROR_RT_NAME, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params);

	int depthBpp      = 40;

	CreateRenderTarget(MIRROR_DT_NAME, grcrtDepthBuffer, mirrorResX, mirrorResY, depthBpp, &params);

	if (GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		//		CreateRenderTarget(MIRROR_DT_NAME_RO, ms_pDepthTarget->GetTexturePtr(), params.Format, DepthStencilReadOnly);
	}
	else
		CreateRenderTarget(MIRROR_DT_NAME_COPY, grcrtDepthBuffer, mirrorResX, mirrorResY, depthBpp, NULL);
}

void calculateWaterRenderTargetsSizes(Settings &settings)
{
	grcTextureFactory::CreateParams createParams;
	createParams.StereoRTMode = grcDevice::STEREO;

	//Changing the resolution of these targets breaks the fog shadows.
	//Not sure if we want them at lower resolutions so disabling for now.
	bool m_EnableWaterLighting = settings.m_graphics.m_WaterQuality != (CSettings::eSettingsLevel) (0);

	u32 resWidth			= settings.m_video.m_ScreenWidth / 2;
	u32 resHeight			= settings.m_video.m_ScreenHeight / 2;

	if (m_EnableWaterLighting)
	{
		CreateRenderTarget("Water Fog+Shadow",				grcrtPermanent, resWidth, resHeight, 32, &createParams);
		CreateRenderTarget("Water Fog+Shadow 1/4",			grcrtPermanent, resWidth/2, resHeight/2, 32, &createParams);
		CreateRenderTarget("Water Fog+Shadow 1/16 Temp",	grcrtPermanent, resWidth/4, resHeight/4, 32, &createParams);
		CreateRenderTarget("Water Fog+Shadow 1/16",		grcrtPermanent, resWidth/4, resHeight/4, 32, &createParams);\
		CreateRenderTarget("Water Params",               grcrtPermanent, resWidth,   resHeight,   32, &createParams);

 		CreateRenderTarget("Water Refraction Mask",			grcrtPermanent, resWidth/2, resHeight/2, 32, &createParams);

		CreateRenderTarget("Water Refraction Depth",	grcrtDepthBuffer,	resWidth, resHeight, 32, &createParams);
	}

	CreateRenderTarget("Water Refraction",	grcrtPermanent,	resWidth, resHeight, 32, &createParams);

#if 1 //WATER_TILED_LIGHTING
	grcTextureFactory::CreateParams classificationParams;
	classificationParams.Format = grctfA8R8G8B8;

	u32 tileSize = settings.m_graphics.m_MSAA > 4 ? 8 : 16;
	u32 numTilesX = (settings.m_video.m_ScreenWidth + tileSize - 1) / tileSize;
	u32 numTilesY = (settings.m_video.m_ScreenHeight + tileSize - 1) / tileSize;

	CreateRenderTarget("Water Classification", grcrtPermanent, numTilesX, numTilesY, 32, &classificationParams);
#endif //WATER_TILED_LIGHTING

}

void calculatePedDamageRenderTargetSizes(Settings &settings)
{
	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	int bpp = 32;

	int iTargetSizeW, iTargetSizeH;

	u32 dimension = (u32) rage::Sqrtf((float)(settings.m_video.m_ScreenWidth * settings.m_video.m_ScreenHeight));

	s32 iMipLevels = 0;
	while (dimension)
	{
		dimension >>= 1;
		iMipLevels++;
	}
	iMipLevels = Max(1, (iMipLevels - 8));
	iTargetSizeW = Min((int)(CPedDamageSetBase::kTargetSizeW / 4) * iMipLevels, (int)CPedDamageSetBase::kTargetSizeW);
	iTargetSizeH = Min((int)(CPedDamageSetBase::kTargetSizeH / 4) * iMipLevels, (int)CPedDamageSetBase::kTargetSizeH);

	int vSize = iTargetSizeW*2; // swap the width and height to take advantage of the ps3 Stride
	int hSize = iTargetSizeH*2;

	char targetName[256] = "\0";

	// allocate high resolution render targets for the player only damage and decorations
	for (int i=0;i<kMaxHiResBloodRenderTargets;i++)
	{
		formatf(targetName,sizeof(targetName),"BloodTargetHiRes%d",i);
		CreateRenderTarget(targetName, grcrtPermanent, hSize, vSize, bpp, &params);
	}

	hSize = iTargetSizeH; // Side ways makes it more like the ps3 targets (except the last 2)
	vSize = iTargetSizeW;

	// allocate medium resolution render targets
	for (int i=0;i<kMaxMedResBloodRenderTargets;i++)
	{
		formatf(targetName,sizeof(targetName),"BloodTargetMedRes%d",i);
		CreateRenderTarget(targetName, grcrtPermanent, hSize, vSize, bpp, &params);
	}

	// allocate low resolution render targets (these overlap with the medium res targets...)
	hSize = iTargetSizeH/2;
	vSize = iTargetSizeW/2;

	for (int i=0;i<kMaxLowResBloodRenderTargets;i++)
	{
		formatf(targetName,sizeof(targetName),"BloodTargetLowRes%d",i);
		CreateRenderTarget(targetName, grcrtPermanent, hSize, vSize, bpp, &params);
	}

	hSize = iTargetSizeH/2;	// side ways small, so 2 can fit side by side
	vSize = iTargetSizeW/2;

	CreateRenderTarget("PedMirrorDepth", grcrtDepthBuffer, hSize*2, vSize, 32, &params);

	CreateRenderTarget("Mirror Ped Decoration Target", grcrtPermanent, hSize*2, vSize, bpp, &params);

	int rotatedDepthTargetWidth = iTargetSizeH;
	int rotatedDepthTargetHeight = iTargetSizeW;

	CreateRenderTarget("PedDamageLowResDepth", grcrtDepthBuffer, rotatedDepthTargetWidth/2, rotatedDepthTargetHeight/2, 32, &params);
	CreateRenderTarget("PedDamageMedResDepth", grcrtDepthBuffer, rotatedDepthTargetWidth, rotatedDepthTargetHeight, 32, &params);
	CreateRenderTarget("PedDamageHiResDepth", grcrtDepthBuffer, iTargetSizeH*2, iTargetSizeW*2, 32, &params);

	{
		bool m_bEnableHiresCompressedDecorations = true;//PEDDECORATIONBUILDER.IsUsingHiResCompressedMPDecorations();

/*		u32 dimension = (u32) rage::Sqrtf((float)(settings.m_video.m_ScreenWidth * settings.m_video.m_ScreenHeight));
		float dimensionResolution = 0;
		if (dimension < 900) 
		{
			dimensionResolution = ((float)(m_bEnableHiresCompressedDecorations?2:1))/4;
		}
		else
		{
			u32 iMipLevels = 0;
			while (dimension)
			{
				dimension >>= 1;
				iMipLevels++;
			}
			iMipLevels -= 10;

			if (iMipLevels == 0)
			{
				dimensionResolution = ((float)(m_bEnableHiresCompressedDecorations?2:1))/2;
			}
			else
			{
				dimensionResolution = ((float)(m_bEnableHiresCompressedDecorations?2:1))*iMipLevels;
			}
		}

		u32 compWidth = (u32)(CompressedDecorationBuilder::kDecorationTargetWidth * dimensionResolution);
		u32 compHeight = (u32)(CompressedDecorationBuilder::kDecorationTargetHeight * dimensionResolution);
*/

		u32 compHeight = (m_bEnableHiresCompressedDecorations?CompressedDecorationBuilder::kDecorationHiResTargetRatio:1)*CompressedDecorationBuilder::kDecorationTargetHeight;
		if (settings.m_graphics.m_TextureQuality == 0) compHeight = compHeight >> 1;
		else if (settings.m_graphics.m_TextureQuality >1) compHeight = compHeight << 1;

		u32 compWidth = (m_bEnableHiresCompressedDecorations?CompressedDecorationBuilder::kDecorationHiResTargetRatio:1)*CompressedDecorationBuilder::kDecorationTargetWidth;
		if (settings.m_graphics.m_TextureQuality == 0) compWidth = compWidth >> 1;
		else if (settings.m_graphics.m_TextureQuality >1) compWidth = compWidth << 1;


// 		u32 compWidth = (u32)(m_bEnableHiresCompressedDecorations?2:1)*CompressedDecorationBuilder::kDecorationTargetWidth;
// 		u32 compHeight = (u32)(m_bEnableHiresCompressedDecorations?2:1)*CompressedDecorationBuilder::kDecorationTargetHeight;

		CreateRenderTarget("PedDamagedCompressedDepth", grcrtDepthBuffer, compWidth, compHeight, 32, &params);

/*
These don't get re-created
		if (!CompressedDecorationBuilder::IsUsingUncompressedMPDecorations())
		{
			CreateRenderTarget("CompressedDecorationBuilder", grcrtPermanent, compWidth, compHeight, CompressedDecorationBuilder::kDecorationTargetBpp, &params);
		}

		if (m_bEnableHiresCompressedDecorations)
		{
			for (int i = 0; i < kMaxCompressedTextures; i++)
			{
				char name[RAGE_MAX_PATH];
				formatf(name,RAGE_MAX_PATH,"tat_rt_%03d_a_uni_hires", i);
				CreateRenderTarget(name, grcrtDepthBuffer, compWidth, compHeight, 32, &params);
			}
		}
*/
	}
}

void calculatePhoneMgrRenderTargetSizes(Settings &settings)
{
#define PHOTO_X_SIZE		256
#define PHOTO_Y_SIZE		256
#define PHONE_SCREEN_RES 256
#define SCREEN_RT_NAME	"PHONE_SCREEN"
#define	PHOTO_RT_NAME	"PHOTO"

	const s32 resX = PHONE_SCREEN_RES;
	const s32 resY = PHONE_SCREEN_RES;
	const s32 rtWidth = (s32)(((float)resX/1280.0) * settings.m_video.m_ScreenWidth);
	const s32 rtHeight = (s32)(((float)resY/720.0) * settings.m_video.m_ScreenHeight);
	CreateRenderTarget(SCREEN_RT_NAME, grcrtPermanent, rtWidth, rtHeight, 32);

	grcTextureFactory::CreateParams params;
	params.Multisample	= 0;

	CreateRenderTarget(PHOTO_RT_NAME, grcrtPermanent, PHOTO_X_SIZE, PHOTO_Y_SIZE, 32, &params);
}

void calculateViewportManagerRenderTargetSizes(Settings &settings)
{
#if __BANK
	for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
	{
		grcTextureFactory::CreateParams params;
		params.UseFloat = true;
		params.Multisample = 0;
		params.HasParent = true;
		params.Parent = NULL;
		params.IsResolvable = false;
		params.IsRenderable = true;
		params.UseHierZ = true;
		params.Format = grctfNone;

		static const int sBPP = 32;

#if ENTITYSELECT_NO_OF_PLANES == 1
		//Create EDRAM depth target
		bool createFullsizeDepth = true;
		if(createFullsizeDepth)
			CreateRenderTarget("Entity Select Depth Target", grcrtDepthBuffer, settings.m_video.m_ScreenWidth, settings.m_video.m_ScreenHeight, sBPP, &params);
#else // ENTITYSELECT_NO_OF_PLANES == 1
		CreateRenderTarget("Entity Select Depth Target", grcrtDepthBuffer, settings.m_video.m_ScreenWidth, settings.m_video.m_ScreenHeight, sBPP, &params);
#endif // ENTITYSELECT_NO_OF_PLANES == 1

		params.Format = grctfA8R8G8B8;

#if ENTITYSELECT_NO_OF_PLANES == 1
		//Create EDRAM RGB target
		bool createFullsizeRt = true;
		if(createFullsizeRt)
			CreateRenderTarget("Entity Select EDRAM target", grcrtPermanent, settings.m_video.m_ScreenWidth, settings.m_video.m_ScreenHeight, sBPP, &params);
#else // ENTITYSELECT_NO_OF_PLANES == 1
		CreateRenderTarget("Entity Select target", grcrtPermanent, settings.m_video.m_ScreenWidth, settings.m_video.m_ScreenHeight, sBPP, &params);
#endif // ENTITYSELECT_NO_OF_PLANES == 1

		params.HasParent = false;
		params.Parent = NULL;
		params.IsResolvable = true;
		params.IsRenderable = false;
		PS3_ONLY(params.InLocalMemory = false);	//We need to read this texture from the CPU, so don't create it in VRAM.

#if RSG_PC || RSG_ORBIS || RSG_DURANGO
		params.Lockable = true;
#endif
#if RSG_ORBIS
		params.ForceNoTiling = true;
#endif // RSG_ORBIS

#define ENTITYSELECT_TARGET_SIZE  64 //Must be at least 32 because of 32-pixel alignment requirement. (on Xenon, for resolve)

		//Create resolve target.
		CreateRenderTarget("Entity Select Resolve Target 0", grcrtPermanent, ENTITYSELECT_TARGET_SIZE, ENTITYSELECT_TARGET_SIZE, sBPP, &params);
		CreateRenderTarget("Entity Select Resolve Target 1", grcrtPermanent, ENTITYSELECT_TARGET_SIZE, ENTITYSELECT_TARGET_SIZE, sBPP, &params);
	}
#endif //__BANK

	{
		grcTextureFactory::CreateParams params;
		int reflectionResolution = REFLECTION_TARGET_HEIGHT;

#if !__FINAL
		int BPP = 0;
#if !RSG_ORBIS
		if( PARAM_DX11Use8BitTargets.Get() )
		{
			BPP = 32;
		}
		else
#endif // !RSG_ORBIS
		{
			BPP = 32;
		}
		const int tbpp = BPP;
#else //!__FINAL
		const int tbpp=32;
#endif //!__FINAL

		params.Multisample	= grcDevice::MSAA_None;
		params.MipLevels	= 4;

		if ( !PARAM_UseDefaultResReflection.Get())
		{
			int reflectionSizeX = (((int)settings.m_graphics.m_ReflectionQuality > 0) ? Max(1, (int)settings.m_video.m_ScreenWidth / (1 << (3 - Min(3, (int)settings.m_graphics.m_ReflectionQuality)))) : 32);
			int reflectionSizeY = (((int)settings.m_graphics.m_ReflectionQuality > 0) ? Max(1, (int)settings.m_video.m_ScreenHeight / (1 << (3 - Min(3, (int)settings.m_graphics.m_ReflectionQuality)))) : 32);
			u32 dimension = (u32) rage::Sqrtf((float)(reflectionSizeX * reflectionSizeY));
			u32 iMipLevels = 0;
			while (dimension)
			{
				dimension >>= 1;
				iMipLevels++;
			}
			iMipLevels--;

			reflectionResolution = 1 << iMipLevels;

			//We still need to mip down to the same resolution as console so adjust increase the number of mip levels depending on our resolution.
			u32 mipResolution = reflectionResolution;
			while( mipResolution > REFLECTION_TARGET_HEIGHT)
			{
				mipResolution = mipResolution >>1;
				params.MipLevels++;
			}

			CreateRenderTarget("REFLECTION_MAP_COLOUR", grcrtPermanent, reflectionResolution * 2, reflectionResolution, tbpp, &params);
//			CreateRenderTarget("REFLECTION_MAP_DEPTH", grcrtDepthBuffer, reflectionResolution * 2, reflectionResolution, 32);
			CreateRenderTarget("REFLECTION_MAP_COLOUR COPY", grcrtPermanent, reflectionResolution * 2, reflectionResolution, tbpp, &params);
		}
		else
		{
			CreateRenderTarget("REFLECTION_MAP_COLOUR", grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, tbpp, &params);
			// this corrupts gbuf0 when reflection phase is placed after cascade shadow
			CreateRenderTarget("REFLECTION_MAP_COLOUR COPY", grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, tbpp, &params);
		}

		{
			grcTextureFactory::CreateParams params;

			#define CUBEMAPREF_MIP			4
			params.Format = grctfR11G11B10F;
			params.MipLevels = CUBEMAPREF_MIP;
			params.ArraySize = 6;

			const int bpp = 32;//grcTextureFactory::GetInstance().GetBitsPerPixel(params.Format);

			//REFLECTION CUBE MAPS
#if REFLECTION_CUBEMAP_SAMPLING
			CreateRenderTarget("REFLECTION_MAP_COLOUR_CUBE", grcrtCubeMap, reflectionResolution, reflectionResolution, bpp, &params);
#else
			CreateRenderTarget("REFLECTION_MAP_COLOUR_CUBE", grcrtCubeMap, reflectionResolution, reflectionResolution, bpp, &params);
#endif

#if GS_INSTANCED_CUBEMAP
			params.Format = grctfD24S8;
			params.MipLevels = 1;
			params.ArraySize = 6;
			CreateRenderTarget("REFLECTION_MAP_COLOUR_CUBE_DEPTH", grcrtDepthBuffer, reflectionResolution, reflectionResolution, 32, &params);
#else

			params.ArraySize = 1;
			params.MipLevels = 1;
			params.PerArraySliceRTV = 1;
			params.HasParent = true;
			params.Parent = NULL;
			params.IsResolvable = true;
			params.IsRenderable = true;
			params.Lockable = 0;
			params.Multisample = grcDevice::MSAAMode(settings.m_graphics.m_ReflectionMSAA);

#if 0//DOUBLE_BUFFER_COLOR
			const int numColorTargets = 2;
#else
			const int numColorTargets = 1;
#endif
			for (int i = 0; i < numColorTargets; i++)
			{
				char targetFacetName[256];
				formatf(targetFacetName, 254, "REFLECTION_MAP_COLOUR_FACET %d", i);

				CreateRenderTarget(targetFacetName, grcrtPermanent, reflectionResolution, reflectionResolution, bpp, &params);
			}

			params.Format = grctfD24S8;
			params.UseHierZ = true;
			params.IsResolvable = false;
			params.Lockable = 0;

#if 0//DOUBLE_BUFFER_DEPTH
			const int numDepthTargets = 2;
#else
			const int numDepthTargets = 1;
#endif
			for (int i = 0; i < numDepthTargets; i++)
			{
					CreateRenderTarget("REFLECTION_MAP_DEPTH", grcrtDepthBuffer, reflectionResolution, reflectionResolution, 32, &params);
			}
#endif
		}
	}


	// R A I N  H E I G H T  M A P / H E I G H T  M A P
	{
		grcTextureFactory::CreateParams params;
// 		CreateRenderTarget("HEIGHT_MAP_COLOR",		grcrtPermanent,	HGHT_TARGET_SIZE, HGHT_TARGET_SIZE, 32);

		params.Multisample = grcDevice::MSAA_None;
		CreateRenderTarget("HEIGHT_MAP_DEPTH",		RSG_ORBIS ? grcrtShadowMap : grcrtPermanent,	HGHT_TARGET_SIZE, HGHT_TARGET_SIZE, 32, &params);
	}


	//WATER REFLECTION
	{
		grcTextureFactory::CreateParams params;
		params.Multisample = grcDevice::MSAA_None;
		int tbpp=32;
		params.Format = grctfR11G11B10F;

		const int resX = GetMirrorResolutionSetting(settings);
		const int resY = resX / 2;

		CreateRenderTarget("WATER_REFLECTION_COLOUR",	grcrtPermanent,		resX, resY, tbpp, &params);

		if((settings.m_graphics.m_ReflectionMSAA > 1) && GRCDEVICE.GetDxFeatureLevel() >= 1010)
		{
			//	create a MSAA rendertarget and keep MSAA for depth
			params.Multisample = settings.m_graphics.m_ReflectionMSAA;
			params.MipLevels = 1;

			CViewportManager::ComputeVendorMSAA(params);

			CreateRenderTarget("WATER_REFLECTION_COLOUR_MSAA",	grcrtPermanent,	  resX, resY, 32, &params);
		}

		tbpp = 40;
		params.Format = grctfD32FS8;

		CreateRenderTarget("WATER_REFLECTION_DEPTH",	grcrtDepthBuffer,	resX, resY, tbpp, &params);

		if(GRCDEVICE.IsReadOnlyDepthAllowed())
		{
			//			CreateRenderTarget("WATER_REFLECTION_DEPTH", pDepthTarget->GetTexturePtr(), params.Format, DepthStencilReadOnly);
		}
		else
			CreateRenderTarget("WATER_REFLECTION_DEPTH_COPY",	grcrtDepthBuffer,	resX, resY, 32);
	}

	// SeeThrough FX
	{
		grcTextureFactory::CreateParams params;
		params.Multisample = grcDevice::MSAA_None;
		params.Format = grctfA8R8G8B8;

		CreateRenderTarget("FX_SEETHROUGH_COLOR", grcrtPermanent, SEETHROUGH_LAYER_WIDTH, SEETHROUGH_LAYER_HEIGHT, 32, &params);
		CreateRenderTarget("FX_SEETHROUGH_BLUR", grcrtPermanent, SEETHROUGH_LAYER_WIDTH, SEETHROUGH_LAYER_HEIGHT, 32, &params);
		CreateRenderTarget("FX_SEETHROUGH_DEPTH", grcrtDepthBuffer, SEETHROUGH_LAYER_WIDTH, SEETHROUGH_LAYER_HEIGHT, 32);
	}
}


void calculatePostFXRenderTargetsSize(Settings &settings)
{
	//Create render targets.
	int iWidth = settings.m_video.m_ScreenWidth;
	int iHeight = settings.m_video.m_ScreenHeight;
#if RSG_PC
	applySceneScale(settings.m_graphics.m_SamplingMode, iWidth, iHeight);
#endif

	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	int bpp = 64;

#if __BANK
	if (PARAM_postfxusesFP16.Get())
	{
		bpp = 64;
		params.Format = grctfA16B16G16R16F;
	}
	else
#endif
	{
		bpp = 32;
		params.Format = grctfR11G11B10F;
	}

#if RSG_PC
	params.StereoRTMode = grcDevice::STEREO;
#endif // RSG_PC

	CreateRenderTarget("Half Screen 0", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);

	if (settings.m_graphics.m_PostFX >= CSettings::Medium)
	{
		CreateRenderTarget("Bloom Buffer", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);
		CreateRenderTarget("Bloom Buffer Unfiltered", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);
	}

	u32 dofTargetWidth = iWidth/4;
	u32 dofTargetHeight = iHeight/4;
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
	if( (settings.m_graphics.m_PostFX >= CSettings::High) && (GRCDEVICE.IsStereoEnabled() == false))
	{
		dofTargetWidth = iWidth;
		dofTargetHeight = iHeight;
	}
#endif //DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION

	CreateRenderTarget("DOF 1", grcrtPermanent, dofTargetWidth, dofTargetHeight, bpp, &params);
	CreateRenderTarget("DOF 2", grcrtPermanent, dofTargetWidth, dofTargetHeight, bpp, &params);

	DURANGO_ONLY(CreateRenderTarget("Bloom Buffer Half 0", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);)
	CreateRenderTarget("Bloom Buffer Half 1", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);
	CreateRenderTarget("Bloom Buffer Quarter 0", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params);
	CreateRenderTarget("Bloom Buffer Quarter 1", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params);
	CreateRenderTarget("Bloom Buffer Eighth 0", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params);
	CreateRenderTarget("Bloom Buffer Eighth 1", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params);
	CreateRenderTarget("Bloom Buffer Sixteenth 0", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params);
	CreateRenderTarget("Bloom Buffer Sixteenth 1", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params);

	if (settings.m_graphics.m_PostFX >= CSettings::High)
	{
		CreateRenderTarget("Lens Artefact Final Buffer", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);
#if 1 || __DEV
		CreateRenderTarget("Lens Artefact 4x Buffer 0", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params);
		CreateRenderTarget("Lens Artefact 4x Buffer 1", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params);
		CreateRenderTarget("Lens Artefact 8x Buffer 0", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params);
		CreateRenderTarget("Lens Artefact 8x Buffer 1", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params);
		CreateRenderTarget("Lens Artefact 16x Buffer 0", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params);
		CreateRenderTarget("Lens Artefact 16x Buffer 1", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params);
#endif // __DEV
	}
#if RSG_PC
	if (PostFX::g_EnableFilmEffect)
#endif // RSG_PC
		CreateRenderTarget("LensDistortion", grcrtPermanent, iWidth, iHeight, bpp, &params);

#if BOKEH_SUPPORT
	bool BokehEnable = (GRCDEVICE.GetDxFeatureLevel() >= 1100)  NOTFINAL_ONLY(&& !PARAM_disableBokeh.Get());
	BokehEnable &= (settings.m_graphics.m_PostFX >= CSettings::High) ? true : false;

	if( BokehEnable )
	{
		CreateRenderTarget("Bokeh Points", grcrtPermanent, iWidth, iHeight, 64, &params);
		// Shared with BloomBuffer now
		DURANGO_ONLY(CreateRenderTarget("Bokeh Points Temp", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params);)

		CreateRenderTarget("BokehDepthBlur", grcrtPermanent, iWidth, iHeight, bpp, &params);

		CreateRenderTarget("BokehDepthBlurHalfRes", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params);

#if COC_SPREAD
		if( true /*CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion*/)
		{
			CreateRenderTarget("BokehDepthBlurHalfResTemp", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params);
		}
#endif
	}
#endif

	CreateRenderTarget("HeatHaze 1", grcrtPermanent, iWidth/8, iHeight/8, 8, &params);
	CreateRenderTarget("HeatHaze 2", grcrtPermanent, iWidth/8, iHeight/8, 8, &params);

	if (settings.m_graphics.m_ShaderQuality > 0)
	{
		CreateRenderTarget("Global FogRay", grcrtPermanent, iWidth/2, iHeight/2, 32, &params);
		CreateRenderTarget("Global FogRay 2", grcrtPermanent, iWidth/2, iHeight/2, 32, &params);
	}

#if RSG_PC
	params.StereoRTMode = grcDevice::STEREO;
#endif

	CreateRenderTarget("SSLR Cutout", grcrtPermanent, 256, 256, 8, &params);
	CreateRenderTarget("SSLR Intensity", grcrtPermanent, iWidth/4, iHeight/4, 8, &params);
	CreateRenderTarget("SSLR Intensity2", grcrtPermanent, iWidth/4, iHeight/4, 8, &params);

	params.StereoRTMode = grcDevice::AUTO;

	CreateRenderTarget("Fog of War 0", grcrtPermanent, 64, 64, 32, &params);
	CreateRenderTarget("Fog of War 1", grcrtPermanent, 16, 16, 32, &params);
	CreateRenderTarget("Fog of War 2", grcrtPermanent, 4, 4, 32, &params);

	// Depending on the device resolution, we need a different number of passes and filter sizes
	// to correctly downsample the luminance; the code below can only handle 1080, 900 and 720.
	// Any other resolution defaults to the 720 behaviour.

	int deviceHeight = iHeight;

	if (deviceHeight > 1080)
	{
		deviceHeight = 1080;
	}
	else if (deviceHeight > 900)
	{
		deviceHeight = 900;
	}
	else
	{
		deviceHeight = 720;
	}

	int LumDownsampleRTWidth[5];
	int LumDownsampleRTHeight[5];
	int LumDownsampleRTCount;

	switch (deviceHeight)
	{
	case 1080:
		{
			LumDownsampleRTWidth[0] = 120; LumDownsampleRTHeight[0] = 90; LumDownsampleRTWidth[1] = 30; LumDownsampleRTHeight[1] = 30; LumDownsampleRTWidth[2] = 10; LumDownsampleRTHeight[2] = 10; LumDownsampleRTWidth[3] = 5; LumDownsampleRTHeight[3] = 5; LumDownsampleRTWidth[4] = 1; LumDownsampleRTHeight[4] = 1;
			LumDownsampleRTCount = 5;
			break;
		}
	case 900:
		{
			LumDownsampleRTWidth[0] = 100; LumDownsampleRTHeight[0] = 75; LumDownsampleRTWidth[1] = 25; LumDownsampleRTHeight[1] = 25; LumDownsampleRTWidth[2] = 5; LumDownsampleRTHeight[2] = 5; LumDownsampleRTWidth[3] = 1; LumDownsampleRTHeight[3] = 1;
			LumDownsampleRTCount = 4;
			break;
		}

	case 720:
	default:
		{
			LumDownsampleRTWidth[0] = 80; LumDownsampleRTHeight[0] = 60; LumDownsampleRTWidth[1] = 20; LumDownsampleRTHeight[1] = 20; LumDownsampleRTWidth[2] = 10; LumDownsampleRTHeight[2] = 10; LumDownsampleRTWidth[3] = 5; LumDownsampleRTHeight[3] = 5; LumDownsampleRTWidth[4] = 1; LumDownsampleRTHeight[4] = 1;
			LumDownsampleRTCount = 5;
			break;
		}
	}

#if AVG_LUMINANCE_COMPUTE
	if (settings.m_graphics.m_DX_Version >= 2)
	{		
		float fdeviceWidth = (float)(iWidth/4);
		float fdeviceHeight = (float)(iHeight/4);

		float maxSize = rage::Max(fdeviceWidth, fdeviceHeight);

		int LumCSDownsampleUAVCount = 0;

		params.UseAsUAV = true;
		while ( maxSize > 16.0f )
		{			
			maxSize = ceilf(maxSize / 16.0f);

			fdeviceWidth = ceilf(fdeviceWidth / 16.0f);
			fdeviceHeight = ceilf(fdeviceHeight / 16.0f); 

			char szLumDownName[256];
			formatf(szLumDownName, 254, "LumDownsampleUAV %d", LumCSDownsampleUAVCount);

			CreateRenderTarget(szLumDownName, grcrtPermanent, (int)fdeviceWidth, (int)fdeviceHeight, 32, &params);			

			LumCSDownsampleUAVCount++;
		}				

		//1x1 
		params.UseFloat = true; 
		CreateRenderTarget("LumDownsampleUAV Final", grcrtPermanent, 1, 1, 32, &params);			

		params.UseAsUAV = false;
	}
	else
#endif //AVG_LUMINANCE_COMPUTE
	{
		int iWidth4 = iWidth/4;
		int iHeight4 = iHeight/4;

		int lumWidth = LumDownsampleRTWidth[0];
		int lumHeight = LumDownsampleRTHeight[0];

		int ImLumDownsampleRTCount = 0;

		params.Format = grctfR11G11B10F;

		while (iWidth4/4 > lumWidth || iHeight4/4 > lumHeight)
		{
			iWidth4 /= 4;
			iHeight4 /= 4;

			int imLumRTWidth = rage::Max(lumWidth*4, iWidth4);
			int imLumRTHeight = rage::Max(lumHeight*3, iHeight4);

			char szLumDownName[256];
			formatf(szLumDownName, 254, "ImLumDownsample %d", ImLumDownsampleRTCount);


			CreateRenderTarget(szLumDownName, grcrtPermanent, imLumRTWidth, imLumRTHeight, bpp, &params);
			ImLumDownsampleRTCount++;
		}


		for (int i = 0; i < LumDownsampleRTCount; i++)
		{
			if (i == LumDownsampleRTCount - 1) 
			{ 
				params.UseFloat = true; 
	#if RSG_DURANGO
				params.ESRAMPhase = 0;	// final buffer is tiny and hangs around too long 
	#endif
			}
			char szLumDownName[256];
			formatf(szLumDownName, 254, "LumDownsample %d", i);
			CreateRenderTarget(szLumDownName, grcrtPermanent, LumDownsampleRTWidth[i], LumDownsampleRTHeight[i], 32, &params);
		}
	}
	

	params.Format = grctfR32F;
	CreateRenderTarget("Center Reticule Dist", grcrtPermanent, 1, 1, 32, &params);

#if ADAPTIVE_DOF
#if ADAPTIVE_DOF_GPU
	params.StereoRTMode = grcDevice::AUTO;
	grcTextureFactory::TextureCreateParams TextureParams(grcTextureFactory::TextureCreateParams::SYSTEM, 
		grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead|grcsWrite, NULL, 
		grcTextureFactory::TextureCreateParams::RENDERTARGET, 
		grcTextureFactory::TextureCreateParams::MSAA_NONE);

	CreateRenderTarget("depth reduc 0", grcrtPermanent, 1024, 1024, 32, &params);
	CreateRenderTarget("depth reduc 1", grcrtPermanent, 32, 32, 32, &params);
	CreateRenderTarget("depth reduc 2", grcrtPermanent, 1, 1, 32, &params);

//	grcTextureFactory::GetInstance().Create( ADAPTIVE_DOF_GPU_HISTORY_SIZE, 1, params.Format, NULL, 1, &TextureParams );
//	grcTextureFactory::GetInstance().CreateRenderTarget( "adaptiveDofParams", adaptiveDofParamsTexture->GetTexturePtr());
#endif // ADAPTIVE_DOF_GPU
#endif

	grcTextureFactory::CreateParams mb_params;
	mb_params.Multisample = 0;
	mb_params.HasParent = true;
	mb_params.Parent = NULL; 
	mb_params.Format = grctfA8R8G8B8;
#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING;
#endif

	if (settings.m_graphics.m_PostFX >= CSettings::Medium)
	{
		for (int i = 0; i < 2; i++)
		{
			CreateRenderTarget("FPV Motion Blur Targets", grcrtPermanent, iWidth / 2, iHeight / 2, 32, &mb_params);			
		}
	}
}

/*	SettingsDefaults implementation	*/
float SettingsDefaults::CalcFinalLodRootScale(const Settings &settings)
{
	return Lerp(settings.m_graphics.m_MaxLodScale, sm_fMaxLodScaleMinRange, sm_fMaxLodScaleMaxRange);
}

s64 SettingsDefaults::renderTargetMemSizeFor(Settings settings) {
	totalTextureSize = 0;

	renderTargetStereoMode = settings.m_video.m_Stereo != 0;

	calculateRenderTargetsSize(settings);

	calculateGBufferRenderTargetSize(settings);

	calculateCascadeShadowsRenderTargetSize(settings);

	calculateParaboloidShadowRenderTargetsSize(settings);

	calculateSSAORenderTargetsSize(settings);

	calculateMirrorRenderTargetSizes(settings);

	calculateWaterRenderTargetsSizes(settings);

	calculatePedDamageRenderTargetSizes(settings);

	calculatePhoneMgrRenderTargetSizes(settings);

	calculateViewportManagerRenderTargetSizes(settings);

	calculatePostFXRenderTargetsSize(settings);

	calcualteUIRenderTargetSize(settings);

	return totalTextureSize;
}


s64 SettingsDefaults::totalVideoMemory() {return GRCDEVICE.GetAvailableVideoMemory();}


s64 SettingsDefaults::videoMemoryUsage(CSettings::eSettingsLevel textureQuality, const Settings &settings)
{
	// Calculated from max memory used with min/max lod scale factors at -5.5,-1399,29.3
	const float fLodRange[2] = { 1.0f, 2.5f };  // Range used for calculating these numbers

	const s64 baseRenderTargetValue = 100; // 147 - Uses alignment in settings calculation now
	const s64 misc = (settings.m_graphics.m_DX_Version == 2 ? 11 + 47 : 0); // Indirect / Inorder - DX11 Only - Grass

	// Index and Vertex Buffer LOD 1.0 - Low Variety, LOD 1.0 - High Variety, LOD 2.5 - Low Variety, LOD 2.5 High Variety
	const s64 aIndexAndVertexBuffers[4] = { 60 + 298, 86 + 447, 126 + 548, 152 + 697};  // LOD 1.0, LOD 2.5

	s64 aTextureSpace[4]; // LOD 1.0 - Low Variety, LOD 1.0 - High Variety, LOD 2.5 - Low Variety, LOD 2.5 High Variety
	if (textureQuality == CSettings::High)
	{
		aTextureSpace[0] = 1105; 
		aTextureSpace[1] = 1249; // HD Pack 1628
		aTextureSpace[2] = 1700; 
		aTextureSpace[3] = 1846; // HD Pack 2523
	}
	else if (textureQuality == CSettings::Medium)
	{
		aTextureSpace[0] = 967;
		aTextureSpace[1] = 1156;
		aTextureSpace[2] = 1387;
		aTextureSpace[3] = 1576;
	}
	else
	{
		aTextureSpace[0] = 305;
		aTextureSpace[1] = 360;
		aTextureSpace[2] = 425;
		aTextureSpace[3] = 480;
	}
	const float fLODRange = (fLodRange[1] - fLodRange[0]);
	Assertf((CalcFinalLodRootScale(settings) >= fLodRange[0]), "LOD Max Range %f must be greater than Min LOD Range %f", settings.m_graphics.m_MaxLodScale, fLodRange[0]);
	const float fMaxLOD = Max(CalcFinalLodRootScale(settings), fLodRange[0]);
	const float fLODScale = (fMaxLOD - fLodRange[0]) / fLODRange; // 0 - 2.5 or higher
	
	const s64 aIbBbRequirements[2] = { (s64)Lerp(settings.m_graphics.m_VehicleVarietyMultiplier, (float)aIndexAndVertexBuffers[0], (float)aIndexAndVertexBuffers[1]),
									   (s64)Lerp(settings.m_graphics.m_VehicleVarietyMultiplier, (float)aIndexAndVertexBuffers[2], (float)aIndexAndVertexBuffers[3]) };
	const s64 ibvbRequirements = (s64)Lerp(fLODScale, aIbBbRequirements[0], aIbBbRequirements[1]);

	const s64 aTexRequirements[2] = { (s64)Lerp(settings.m_graphics.m_VehicleVarietyMultiplier, (float)aTextureSpace[0], (float)aTextureSpace[1]),
									  (s64)Lerp(settings.m_graphics.m_VehicleVarietyMultiplier, (float)aTextureSpace[2], (float)aTextureSpace[3]) };
	const s64 texRequirements = (s64)Lerp(fLODScale, aTexRequirements[0], aTexRequirements[1]);

	// Not really accounting for Ped separate from vehicles yet.  I don't image we need two settings for this
	Assert(settings.m_graphics.m_VehicleVarietyMultiplier == settings.m_graphics.m_PedVarietyMultiplier);	

	// LOD scale probably won't account
	return (baseRenderTargetValue + misc + ibvbRequirements + texRequirements) * 1024 * 1024;
}

s64 SettingsDefaults::videoMemSizeFor(Settings settings) {
	return renderTargetMemSizeFor(settings) + videoMemoryUsage(settings.m_graphics.m_TextureQuality, settings);
}

bool SettingsDefaults::testSettingsForVideoMemorySpace(Settings settings) {
	return totalVideoMemory() > renderTargetMemSizeFor(settings);
}

SettingsDefaults& SettingsDefaults::GetInstance()
{
	static SettingsDefaults oSettingsDefaults;
	return oSettingsDefaults;
}












struct IntelDeviceInfoHeader
{
	DWORD Size;
	DWORD Version;
};

// New device dependent structure
struct IntelDeviceInfoV1
{
	DWORD GPUMaxFreq;
	DWORD GPUMinFreq;
};

struct IntelDeviceInfoV2
{
	DWORD GPUMaxFreq;
	DWORD GPUMinFreq;
	DWORD GTGeneration;
	DWORD EUCount;
	DWORD PackageTDP;
	DWORD MaxFillRate;
};

#define INTEL_VENDOR_ID 0x8086

// The new device dependent counter
#define INTEL_DEVICE_INFO_COUNTERS         "Intel Device Information"


#define GGF_SUCCESS 0
#define GGF_ERROR					-1
#define GGF_E_UNSUPPORTED_HARDWARE	-2
#define GGF_E_UNSUPPORTED_DRIVER	-3
#define GGF_E_D3D_ERROR				-4
long SettingsDefaults::getIntelDeviceInfo( unsigned int VendorId, IntelDeviceInfoHeader *pIntelDeviceInfoHeader, void *pIntelDeviceInfoBuffer )
{
	// The device information is stored in a D3D counter.
	// We must create a D3D device, find the Intel counter 
	// and query the counter info
	ID3D11Device *pDevice = NULL;
	ID3D11DeviceContext *pImmediateContext = NULL;
	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = NULL;

	if ( pIntelDeviceInfoBuffer == NULL )
		return GGF_ERROR;

	if ( VendorId != INTEL_VENDOR_ID )
		return GGF_E_UNSUPPORTED_HARDWARE;

	ZeroMemory( &featureLevel, sizeof(D3D_FEATURE_LEVEL) );

	HMODULE hD3D11 = ::LoadLibrary("D3D11.DLL");
	if (!hD3D11) return GGF_ERROR;

	HRESULT (WINAPI *l_D3D11CreateDevice)(
		IDXGIAdapter* pAdapter,D3D_DRIVER_TYPE DriverType,HMODULE Software,
		UINT Flags,CONST D3D_FEATURE_LEVEL* pFeatureLevels,UINT FeatureLevels,
		UINT SDKVersion,ID3D11Device** ppDevice,D3D_FEATURE_LEVEL* pFeatureLevel,
		ID3D11DeviceContext** ppImmediateContext );

	l_D3D11CreateDevice = (HRESULT (WINAPI *)(
		IDXGIAdapter* pAdapter,D3D_DRIVER_TYPE DriverType,HMODULE Software,
		UINT Flags,CONST D3D_FEATURE_LEVEL* pFeatureLevels,UINT FeatureLevels,
		UINT SDKVersion,ID3D11Device** ppDevice,D3D_FEATURE_LEVEL* pFeatureLevel,
		ID3D11DeviceContext** ppImmediateContext ))::GetProcAddress(hD3D11,"D3D11CreateDevice");

	// First create the Device, must be SandyBridge or later to create D3D11 device
	hr = l_D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL,
		D3D11_SDK_VERSION, &pDevice, &featureLevel, &pImmediateContext);

	if ( FAILED(hr) )
	{
		SAFE_RELEASE( pImmediateContext );
		SAFE_RELEASE( pDevice );

		printf("D3D11CreateDevice failed\n");

		return FALSE;
	}

	// The counter is in a device dependent counter
	D3D11_COUNTER_INFO counterInfo;
	D3D11_COUNTER_DESC pIntelCounterDesc;

	int numDependentCounters;
	UINT uiSlotsRequired, uiNameLength, uiUnitsLength, uiDescLength;
	LPSTR sName, sUnits, sDesc;

	ZeroMemory( &counterInfo, sizeof(D3D11_COUNTER_INFO) );
	ZeroMemory( &pIntelCounterDesc, sizeof(D3D11_COUNTER_DESC) );

	// Query the device to find the number of device dependent counters.
	pDevice->CheckCounterInfo( &counterInfo );

	if ( counterInfo.LastDeviceDependentCounter == 0 )
	{
		SAFE_RELEASE( pImmediateContext );
		SAFE_RELEASE( pDevice );

		printf("No device dependent counters\n");

		// The driver does not support the Device Info Counter.
		return GGF_E_UNSUPPORTED_DRIVER;
	}

	numDependentCounters = counterInfo.LastDeviceDependentCounter - D3D11_COUNTER_DEVICE_DEPENDENT_0 + 1;

	// Search for the apporpriate counter - INTEL_DEVICE_INFO_COUNTERS
	for ( int i = 0; i < numDependentCounters; ++i )
	{
		D3D11_COUNTER_DESC counterDescription;
		D3D11_COUNTER_TYPE counterType;

		counterDescription.Counter = static_cast<D3D11_COUNTER>(i+D3D11_COUNTER_DEVICE_DEPENDENT_0);
		counterDescription.MiscFlags = 0;
		counterType = static_cast<D3D11_COUNTER_TYPE>(0);
		uiSlotsRequired = uiNameLength = uiUnitsLength = uiDescLength = 0;
		sName = sUnits = sDesc = NULL;

		if( SUCCEEDED( hr = pDevice->CheckCounter( &counterDescription, &counterType, &uiSlotsRequired, NULL, &uiNameLength, NULL, &uiUnitsLength, NULL, &uiDescLength ) ) )
		{
			LPSTR sName  = rage_new char[uiNameLength];
			LPSTR sUnits = rage_new char[uiUnitsLength];
			LPSTR sDesc  = rage_new char[uiDescLength];

			if( SUCCEEDED( hr = pDevice->CheckCounter( &counterDescription, &counterType, &uiSlotsRequired, sName, &uiNameLength, sUnits, &uiUnitsLength, sDesc, &uiDescLength ) ) )
			{
				if ( strcmp( sName, INTEL_DEVICE_INFO_COUNTERS ) == 0 )
				{
					int IntelCounterMajorVersion;
					int IntelCounterSize;
					int argsFilled = 0;

					pIntelCounterDesc.Counter = counterDescription.Counter;

					argsFilled = sscanf_s( sDesc, "Version %d", &IntelCounterMajorVersion);

					if ( argsFilled == 1 )
					{
						sscanf_s( sUnits, "Size %d", &IntelCounterSize);
					}
					else // Fall back to version 1.0
					{
						IntelCounterMajorVersion = 1;
						IntelCounterSize = sizeof( IntelDeviceInfoV1 );
					}

					pIntelDeviceInfoHeader->Version = IntelCounterMajorVersion;
					pIntelDeviceInfoHeader->Size = IntelCounterSize;
				}
			}

			SAFE_DELETE_ARRAY( sName );
			SAFE_DELETE_ARRAY( sUnits );
			SAFE_DELETE_ARRAY( sDesc );
		}
	}

	// Check if device info counter was found
	if ( pIntelCounterDesc.Counter == NULL )
	{
		SAFE_RELEASE( pImmediateContext );
		SAFE_RELEASE( pDevice );

		printf("Could not find counter\n");

		// The driver does not support the Device Info Counter.
		return GGF_E_UNSUPPORTED_DRIVER;
	}

	// Intel Device Counter //
	ID3D11Counter *pIntelCounter = NULL;

	// Create the appropriate counter
	hr = pDevice->CreateCounter(&pIntelCounterDesc, &pIntelCounter);
	if ( FAILED(hr) )
	{
		SAFE_RELEASE( pIntelCounter );
		SAFE_RELEASE( pImmediateContext );
		SAFE_RELEASE( pDevice );

		printf("CreateCounter failed\n");

		return GGF_E_D3D_ERROR;
	}

	// Begin and end counter capture
	pImmediateContext->Begin(pIntelCounter);
	pImmediateContext->End(pIntelCounter);

	// Check for available data
	hr = pImmediateContext->GetData( pIntelCounter, NULL, NULL, NULL );
	if ( FAILED(hr) || hr == S_FALSE )
	{
		SAFE_RELEASE( pIntelCounter );
		SAFE_RELEASE( pImmediateContext );
		SAFE_RELEASE( pDevice );

		printf("Getdata failed \n");
		return GGF_E_D3D_ERROR;
	}

	DWORD pData[2] = {0};
	// Get pointer to structure
	hr = pImmediateContext->GetData(pIntelCounter, pData, 2*sizeof(DWORD), NULL);

	if ( FAILED(hr) || hr == S_FALSE )
	{
		SAFE_RELEASE( pIntelCounter );
		SAFE_RELEASE( pImmediateContext );
		SAFE_RELEASE( pDevice );

		printf("Getdata failed \n");
		return GGF_E_D3D_ERROR;
	}

	//
	// Prepare data to be returned //
	//
	// Copy data to passed in parameter
	void *pDeviceInfoBuffer = *(void**)pData;

	memcpy( pIntelDeviceInfoBuffer, pDeviceInfoBuffer, pIntelDeviceInfoHeader->Size );

	//
	// Clean up //
	//
	SAFE_RELEASE( pIntelCounter );
	SAFE_RELEASE( pImmediateContext );
	SAFE_RELEASE( pDevice );

	return GGF_SUCCESS;
}

typedef enum
{ 
	IGFX_UNKNOWN     = 0x0, 
	IGFX_SANDYBRIDGE = 0xC, 
	IGFX_IVYBRIDGE,    
	IGFX_HASWELL,
} PRODUCT_FAMILY;

void SettingsDefaults::GetIntelDisplayInfo()
{

	const s64 oneMillion = 1000000;

	IntelDeviceInfoHeader intelDeviceInfoHeader = { 0 };
	byte intelDeviceInfoBuffer[1024];

	long getStatus = getIntelDeviceInfo(m_oAdapterDesc.VendorId, &intelDeviceInfoHeader, intelDeviceInfoBuffer);

	IntelDeviceInfoV2 intelDeviceInfo;
	memcpy( &intelDeviceInfo, intelDeviceInfoBuffer, intelDeviceInfoHeader.Size );

	if ( getStatus == GGF_SUCCESS && intelDeviceInfoHeader.Version >= 2 )
	{
		if (intelDeviceInfo.GTGeneration > IGFX_HASWELL)
		{
			m_processingPower = 41 * oneMillion;
			m_memBandwidth = 41 * oneMillion;
			Displayf("Better than Haswell Generation");
		}
		else if (intelDeviceInfo.GTGeneration == IGFX_HASWELL)
		{
			m_processingPower = 21 * oneMillion;
			m_memBandwidth = 21 * oneMillion;
			Displayf("Haswell Generation");
		}
		else
		{
			m_processingPower = 1 * oneMillion;
			m_memBandwidth = 1 * oneMillion;
			Displayf("Lower than Haswell");
		}
	}
	else
	{
		Assert((m_oAdapterDesc.VendorId == 0x8086) || (m_oAdapterDesc.VendorId == 0x8087));
		const u32 cuHaswelLowestChipset = 0x402;
		if (m_oAdapterDesc.DeviceId < cuHaswelLowestChipset)
		{
			m_processingPower = 1 * oneMillion;
			m_memBandwidth = 1 * oneMillion;
			Displayf("Lower than Haswell");
		}
		else
		{
			m_processingPower = 21 * oneMillion;
			m_memBandwidth = 21 * oneMillion;
			Displayf("Haswell or Higher");
		}
	}
	//Displayf("Cores %d GPU Clock %d Processing Power %d Mem Bandwidth %d", (s32)numOfCores, (s32)gpuClockMax, (s32)m_processingPower, (s32)m_memBandwidth);
}

void SettingsDefaults::initAdapter()
{
	if (grcAdapterManager::GetInstance() == NULL) 
	{
		m_hDXGI = LoadLibrary("dxgi.dll");
		grcAdapterManager::InitClass(DXGI_FORMAT_R8G8B8A8_UNORM, 1100);
		m_bcreatedAdapter = true;
	}
}

void SettingsDefaults::shutdownAdapter()
{
	if (!m_bcreatedAdapter) return;

	if (!m_osVersion)
	{
		((IDirect3D9*)m_pD3D9)->Release();
	}
	else
	{
		grcAdapterManager::ShutdownClass();
		FreeLibrary((HINSTANCE)m_hDXGI);
	}
}

void SettingsDefaults::initCardManufacturer()
{
	if (iTargetAdapter >= grcAdapterManager::GetInstance()->GetAdapterCount())
		iTargetAdapter = 0;

	IDXGIAdapter* pDeviceAdapter = ((const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetDeviceAdapter();
	AssertMsg(pDeviceAdapter, "Could not get the current device adapter");

	pDeviceAdapter->GetDesc(&m_oAdapterDesc);

	if (wcsstr(((const wchar_t*)&m_oAdapterDesc.Description), L"NVIDIA") || m_oAdapterDesc.VendorId == 0x10DE)
	{
		m_cardManufacturer = NVIDIA;
	}
	else if (wcsstr(((const wchar_t*)&m_oAdapterDesc.Description), L"ATI") || wcsstr(((const wchar_t*)&m_oAdapterDesc.Description), L"AMD") || m_oAdapterDesc.VendorId == 0x1002 || m_oAdapterDesc.VendorId == 0x1022)
	{
		m_cardManufacturer = ATI;
	}
	else if (wcsstr(((const wchar_t*)&m_oAdapterDesc.Description), L"Intel") || m_oAdapterDesc.VendorId == 0x8086 || m_oAdapterDesc.VendorId == 0x8087)
	{
		m_cardManufacturer = INTEL;
	}
	else
	{
		m_cardManufacturer = UNKNOWN_DEVICE;
	}
}

void SettingsDefaults::initResolutionToMax()
{
	if (iTargetAdapter >= grcAdapterManager::GetInstance()->GetAdapterCount())
		iTargetAdapter = 0;

	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter);//(s64)sm_AdapterOrdinal);
	AssertMsg(pAdapter, "Could not get the current adapter");

	int iDisplayModes = pAdapter->GetModeCount();
	grcDisplayWindow oDisplayWindow;
	for (int mode = 0; mode < iDisplayModes; mode++)
	{
		pAdapter->GetMode(&oDisplayWindow, mode);
		m_defaultDisplayWindow.uWidth = max(oDisplayWindow.uWidth, m_defaultDisplayWindow.uWidth);
		m_defaultDisplayWindow.uHeight = max(oDisplayWindow.uHeight, m_defaultDisplayWindow.uHeight);
		m_defaultDisplayWindow.uRefreshRate = max(oDisplayWindow.uRefreshRate, m_defaultDisplayWindow.uRefreshRate);
	}
	pAdapter->GetWidestMode (&m_defaultDisplayWindow);
	m_defaultAspectRatio = (float)m_defaultDisplayWindow.uWidth / (float)m_defaultDisplayWindow.uHeight;
}


void SettingsDefaults::SetResolutionToClosestMode()
{
	grcAdapterManager::GetInstance()->InitClass(D3DFMT_X8R8G8B8);
	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter);//(s64)sm_AdapterOrdinal);
	AssertMsg(pAdapter, "Could not get the current adapter");

	pAdapter->GetClosestModeMatchingAspect (&m_defaultDisplayWindow, m_defaultAspectRatio);
}

void SettingsDefaults::GetResolutionToClosestMode(grcDisplayWindow& currentSettings)
{
	grcAdapterManager::GetInstance()->InitClass(D3DFMT_X8R8G8B8);
	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter);//(s64)sm_AdapterOrdinal);
	AssertMsg(pAdapter, "Could not get the current adapter");

	pAdapter->GetClosestMode (&currentSettings);
}

void SettingsDefaults::SetDXVersion()
{
	int directXVersion = 1100;
	switch (directXVersion) 
	{
		case 900:
			{
				m_defaultDirectXVersion = 900;
				return;
			}
		case 1000:
		case 1100:
			{
				HMODULE hD3D11 = ::LoadLibrary("D3D11.DLL");

				HMODULE hD3D10 = ::LoadLibrary("D3D10_1.DLL");

				if (hD3D11)
				{
				HRESULT (WINAPI *l_D3D11CreateDevice)(
						IDXGIAdapter* pAdapter,D3D_DRIVER_TYPE DriverType,HMODULE Software,
						UINT Flags,CONST D3D_FEATURE_LEVEL* pFeatureLevels,UINT FeatureLevels,
						UINT SDKVersion,ID3D11Device** ppDevice,D3D_FEATURE_LEVEL* pFeatureLevel,
						ID3D11DeviceContext** ppImmediateContext );
						
					l_D3D11CreateDevice = (HRESULT (WINAPI *)(
						IDXGIAdapter* pAdapter,D3D_DRIVER_TYPE DriverType,HMODULE Software,
						UINT Flags,CONST D3D_FEATURE_LEVEL* pFeatureLevels,UINT FeatureLevels,
						UINT SDKVersion,ID3D11Device** ppDevice,D3D_FEATURE_LEVEL* pFeatureLevel,
						ID3D11DeviceContext** ppImmediateContext ))::GetProcAddress(hD3D11,"D3D11CreateDevice");

					D3D_FEATURE_LEVEL chosenFeatureLevel;
					HRESULT hr;
					hr = l_D3D11CreateDevice( NULL,
						D3D_DRIVER_TYPE_HARDWARE,
						NULL,
						0,
						NULL, 
						0,
						D3D11_SDK_VERSION, 
						NULL, 
						&chosenFeatureLevel, 
						NULL );
					
					if (FAILED(hr)) 
					{
						m_defaultDirectXVersion = 900;
						return;
					}
					switch(chosenFeatureLevel)
					{
						case D3D_FEATURE_LEVEL_10_0:
							m_defaultDirectXVersion = 1000;
							break;
						case D3D_FEATURE_LEVEL_10_1:
							m_defaultDirectXVersion = 1010;
							break;
						case D3D_FEATURE_LEVEL_11_0:
							m_defaultDirectXVersion = 1100;
							break;
						default:
							m_defaultDirectXVersion = 0;
					}
					return;
				}

				if (hD3D10) { //TODO: figure out how to determine dxversion of card if using DX10/DX10.1 runtime
					m_defaultDirectXVersion = 1000;
					return;
				}
				m_defaultDirectXVersion = 900;
			}
	}

}

void SettingsDefaults::GetDisplayInfo()
{
	m_setMinTextureQuality = false;

	SetDXVersion();

#if USE_RESOURCE_CACHE
	m_osVersion = grcResourceCache::GetOSVersion();
#endif

	m_bcreatedAdapter = false;

	initAdapter();
	initCardManufacturer();
	initResolutionToMax();

	rage::grcDisplayInfo info;
	info.oAdapterDesc = &m_oAdapterDesc;
	info.iTargetAdapter = iTargetAdapter;
	info.numOfCores = 100;
	info.gpuClockMax = 1000;
	info.memClockMax = 1000;
	info.videoMemSize = info.sharedMemSize = 512<<20;
	info.memBandwidth = 20<<20;

	u32 uVidMem = 0;
	PARAM_availablevidmem.Get(uVidMem);
	
	bool initDone = false;

	if (GRCDEVICE.IsUsingVendorAPI())
	{
#if NV_SUPPORT
		if(m_cardManufacturer == NVIDIA)
		{
			GRCDEVICE.InitNVIDIA();
		}

		if(GRCDEVICE.IsUsingNVidiaAPI() && m_cardManufacturer == NVIDIA)
		{
			info.videoMemSize = -1;
			grcAdapterD3D11::GetNvidiaDisplayInfo(info);

			if (info.videoMemSize == -1) 
			{
				info.videoMemSize = m_oAdapterDesc.DedicatedVideoMemory;
				info.sharedMemSize = m_oAdapterDesc.SharedSystemMemory;
			}

			m_numOfCores = info.numOfCores;
			m_gpuClockMax = info.gpuClockMax;
			m_memClockMax = info.memClockMax;
			m_videoMemSize = info.videoMemSize;
			m_sharedMemSize = info.sharedMemSize;
			m_memBandwidth = info.memBandwidth;

			m_memBandwidth = 16 * info.memClockMax;
			m_processingPower = info.numOfCores * info.gpuClockMax;// * numGPUs; I think numGPUs is taken into account in numOfCores

			const float fNVidiaScalingOnAdditionalGPUs = 1.0f;
			m_memBandwidth = m_memBandwidth + (s64)((float)m_memBandwidth * (((float)((grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetGPUCount() - 1) * fNVidiaScalingOnAdditionalGPUs));

			Displayf("GPU Count %d Cores %" I64FMT "d GPU Clock %" I64FMT "d Processing Power %" I64FMT "d Mem Bandwidth %" I64FMT "d", 
				((grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetGPUCount(), m_numOfCores, m_gpuClockMax, m_processingPower, m_memBandwidth);


			m_videoMemSize = (uVidMem) ? (s64)uVidMem * (s64)1024 * (s64)1024 : m_videoMemSize;
			getNVidiaDefaults();

			initDone = true;
		}
#endif

#if ATI_SUPPORT
		if(m_cardManufacturer == ATI)
		{
			info.videoMemSize = -1;
			grcAdapterD3D11::GetATIDisplayInfo(info);

			if (info.videoMemSize == -1) 
			{
				info.videoMemSize = m_oAdapterDesc.DedicatedVideoMemory;
				info.sharedMemSize = m_oAdapterDesc.SharedSystemMemory;
			}

			m_numOfCores = info.numOfCores;
			m_gpuClockMax = info.gpuClockMax;
			m_memClockMax = info.memClockMax;
			m_videoMemSize = info.videoMemSize;
			m_sharedMemSize = info.sharedMemSize;
			m_memBandwidth = info.memBandwidth;

			const SIZE_T uMinVideoMemory = 900 * 1024 * 1024;
			m_videoMemSize = (s64)(( m_videoMemSize < uMinVideoMemory ) ? ((m_sharedMemSize < uMinVideoMemory) ? (2048ULL * 1024ULL * 1024ULL) : m_sharedMemSize) : m_videoMemSize);

			m_numOfCores = 240;
			m_processingPower = m_numOfCores * m_gpuClockMax;

			const float fCrossfireScalingOnAdditionalGPUs = 1.0f;
			m_memBandwidth = m_memBandwidth + (s64)((float)m_memBandwidth * (((float)((grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetGPUCount() - 1) * fCrossfireScalingOnAdditionalGPUs));

			Displayf("GPU Count %d Cores %" I64FMT "d GPU Clock %" I64FMT "d Processing Power %" I64FMT "d Mem Bandwidth %" I64FMT "d", 
				((grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(iTargetAdapter))->GetGPUCount(), m_numOfCores, m_gpuClockMax, m_processingPower, m_memBandwidth);

			m_videoMemSize = (uVidMem) ? (s64)uVidMem * (s64)1024 * (s64)1024 : m_videoMemSize;
			getATIDefaults();

			initDone = true;
		}
#endif

/*		if(m_cardManufacturer == INTEL)
		{
			GetIntelDisplayInfo();

			m_videoMemSize = (s64)GRCDEVICE.RetrieveVideoMemory();
			m_videoMemSize = (uVidMem) ? (s64)uVidMem * (s64)1024 * (s64)1024 : m_videoMemSize;
			getIntelDefaults();

			initDone = true;
		}*/
	}
	
	if(!initDone)
	{
		m_videoMemSize = (s64)GRCDEVICE.RetrieveVideoMemory();
		m_videoMemSize = (uVidMem) ? (s64)uVidMem * (s64)1024 * (s64)1024 : m_videoMemSize;
		minimumSpecNoCheapShaders();
		
		initDone = true;
	}

	if (IsLowQualitySystem(m_Defaults))
	{
		m_Defaults.m_graphics.m_ReflectionQuality = CSettings::Low;
	}

	DetermineTextureDefaults();
	LimitDefaultsForWeakCpu();

	shutdownAdapter();
}


SettingsDefaults::SettingsDefaults()
{
	m_Defaults = CSettingsManager::GetInstance().GetSafeModeSettings();
#if !RSG_PC
	m_Defaults.m_video.m_ScreenWidth = 1920;
	m_Defaults.m_video.m_ScreenHeight = 1080;
#endif // !RSG_PC

	GRCDEVICE.InitAdapterOrdinal();
	iTargetAdapter = GRCDEVICE.GetAdapterOrdinal();
// 	if (PARAM_adapter.Get())
// 		PARAM_adapter.Get(iTargetAdapter);
	//	s64 videoMemory = GRCDEVICE.GetAvailableVideoMemory();
	GetDisplayInfo();

	switch (m_defaultDirectXVersion)
	{
		case 900:
		case 1000:
			m_Defaults.m_graphics.m_DX_Version = 0;
			break;
		case 1010:
			m_Defaults.m_graphics.m_DX_Version = 1;
			break;
		case 1100:
			m_Defaults.m_graphics.m_DX_Version = 2;
			break;
		default:
			m_Defaults.m_graphics.m_DX_Version = 0;
			break;
	}
//	m_Defaults.m_graphics.UpdateFromSliders();
}

void SettingsDefaults::DetermineTextureDefaults()
{
	m_defaultVSync = 1;

	s64 renderTargetSize = renderTargetMemSizeFor(m_Defaults);
	s64 fragmentationVideoMemSize = (s64)(m_videoMemSize * g_fPercentForStreamer);

	if (m_setMinTextureQuality) {
		m_Defaults.m_graphics.m_TextureQuality = CSettings::Low;
	}
	else if (fragmentationVideoMemSize > (videoMemoryUsage(CSettings::High, m_Defaults) + renderTargetSize))
	{
		m_Defaults.m_graphics.m_TextureQuality = CSettings::High;

		s64 slushMemory = fragmentationVideoMemSize - (videoMemoryUsage(CSettings::High, m_Defaults) + renderTargetSize);
		const u64 maxSpace = 200 * 1024 * 1024;
		if (slushMemory > maxSpace)
		{
			m_Defaults.m_graphics.m_PedVarietyMultiplier = 1.0f;
			m_Defaults.m_graphics.m_VehicleVarietyMultiplier = 1.0f;
		}
	}
	else if (fragmentationVideoMemSize > (videoMemoryUsage(CSettings::Medium, m_Defaults) + renderTargetSize))
	{
		m_Defaults.m_graphics.m_TextureQuality = CSettings::Medium;
	}
	else
	{
		if ((fragmentationVideoMemSize < (videoMemoryUsage(CSettings::Low, m_Defaults) + renderTargetSize)))
		{
			Warningf("Insufficient fragmented memory for your settings %dMB requires %dMB - Lowering Vehicle Variety", (u32)(fragmentationVideoMemSize / (1024 * 1024)), ((videoMemoryUsage(CSettings::Low, m_Defaults) + renderTargetSize) / (1024 * 1024)));

			while(m_Defaults.m_graphics.m_PedVarietyMultiplier > 0.0)
			{
				m_Defaults.m_graphics.m_PedVarietyMultiplier -= 0.1f;
				m_Defaults.m_graphics.m_VehicleVarietyMultiplier -= 0.1f;

				if (fragmentationVideoMemSize > (videoMemoryUsage(CSettings::Low, m_Defaults) + renderTargetSize))
					break;
			}
			m_Defaults.m_graphics.m_PedVarietyMultiplier = Clamp(m_Defaults.m_graphics.m_PedVarietyMultiplier, 0.0f, 1.0f);
			m_Defaults.m_graphics.m_VehicleVarietyMultiplier = Clamp(m_Defaults.m_graphics.m_VehicleVarietyMultiplier, 0.0f, 1.0f);
		}

		// TODO - Lower render targets until we fit
		while (m_currentDefaultSettingsLevel > 0 && fragmentationVideoMemSize < videoMemoryUsage(CSettings::Low, m_Defaults) + renderTargetMemSizeFor(m_Defaults))
		{
			switch (m_currentDefaultSettingsLevel)
			{
				case 0:
					downgradeToLevel0Settings();
					break;
				case 1:
					downgradeToLevel0Settings();
					break;
				case 2:
					downgradeToLevel1Settings();
					break;
				case 3:
					downgradeToLevel2Settings();
					break;
				case 4:
					downgradeToLevel3Settings();
					break;
				case 5:
					downgradeToLevel4Settings();
					break;
				default:
					downgradeToLevel0Settings();
					break;
			}
			renderTargetSize = renderTargetMemSizeFor(m_Defaults);
		}

		Assertf((m_videoMemSize > (videoMemoryUsage(CSettings::Low, m_Defaults) + renderTargetSize)), "Insufficient memory for your settings %dMB requires %dMB", (u32)(m_videoMemSize / (1024 * 1024)), ((videoMemoryUsage(CSettings::Low, m_Defaults) + renderTargetSize) / (1024 * 1024)));
		m_Defaults.m_graphics.m_TextureQuality = CSettings::Low;
	}

	m_defaultStereo3DConvergence = 7.0f; 
	m_defaultStereo3DSeparation = 10.0f;
	Displayf("Default Selected Level %d for GPU", m_currentDefaultSettingsLevel);

/*
water-1 3%
water-2 5%
shadow-3 120%
shadow-2 60%
shadow-1 50%
fxaa-4/3/2 10%
fxaa-1 5%
ssao 20%
reflection-3 10%
reflection-2 5%
reflection-1 3%
*/
}


void SettingsDefaults::getNVidiaDefaults() {
	// defaultStereo3DEnabled = grcDevice::StereoIsPossible() ? 1 : 0;
	s64 oneMillion = 1000000;
	s64 resolution = m_defaultDisplayWindow.uWidth * m_defaultDisplayWindow.uHeight;
	float baseResolution = 1024 * 1600;
	float resFactor = resolution / baseResolution; // * (defaultStereo3DEnabled + 1);

	InitializeToMaximumDefaultSettings();

	if ((m_processingPower < 30 * oneMillion) || (m_videoMemSize < 1024 * 1024 * 900)) { //Also use lowest settings if less than a gig of video memory
		downgradeToLevel0Settings();
		return;
	} //minimum specs

	static int minResSetting = 100;
	static int maxResSetting = 500;
	static int superMax = 2000;
	static int iMinWidth = 1024;
	static int iMinHeight = 768;

	PARAM_MinPower.Get(minResSetting);
	PARAM_MaxPower.Get(maxResSetting);
	PARAM_SuperMaxPower.Get(superMax);
	PARAM_MinWidth.Get(iMinWidth);
	PARAM_MinHeight.Get(iMinHeight);

	if (m_processingPower < minResSetting * oneMillion) {
		downgradeToLevel1Settings();
		return;
	} //minimum resolution console settings

	if (m_processingPower < maxResSetting * oneMillion * resFactor) {
		downgradeToLevel3Settings();

		s64 powerOverMin = m_processingPower - minResSetting * oneMillion;
		float percentageOfMaxRes = powerOverMin / float((maxResSetting - minResSetting) * oneMillion * resFactor);
		m_defaultDisplayWindow.uWidth = iMinWidth + (s64)((m_defaultDisplayWindow.uWidth - iMinWidth) * percentageOfMaxRes);
		m_defaultDisplayWindow.uHeight = iMinHeight + (s64)((m_defaultDisplayWindow.uHeight - iMinHeight) * percentageOfMaxRes);

		SetResolutionToClosestMode ();

		m_Defaults.m_video.m_ScreenWidth = m_defaultDisplayWindow.uWidth;
		m_Defaults.m_video.m_ScreenHeight = m_defaultDisplayWindow.uHeight;
		m_Defaults.m_video.m_RefreshRate = m_defaultDisplayWindow.uRefreshRate;

		return;
	} //maximum resolution console settings

	if (m_processingPower < (s64)((s64)superMax * oneMillion * resFactor)) {
		downgradeToLevel4Settings();
		return;
	} //maximum resolution high settings

	return; //maximum resolution max settings
}

void SettingsDefaults::getIntelDefaults()
{
	const s64 oneMillion = 1000000;
	/*
	s64 resolution = m_defaultDisplayWindow.uWidth * m_defaultDisplayWindow.uHeight;
	float baseResolution = 1024 * 1600;
	float resFactor = resolution / baseResolution;// * (defaultStereo3DEnabled + 1);
	*/

	InitializeToMaximumDefaultSettings();

	m_currentDefaultSettingsLevel = 1;

	if (m_memBandwidth < 20 * oneMillion) {
		m_Defaults.m_graphics.m_Tessellation = CSettings::Low;
		m_Defaults.m_graphics.m_ShadowQuality = CSettings::Medium;
		m_Defaults.m_graphics.m_ReflectionQuality = CSettings::Medium;
		m_Defaults.m_graphics.m_ReflectionMSAA = 0;
		m_Defaults.m_graphics.m_ParticleQuality = CSettings::Low;
		m_Defaults.m_graphics.m_WaterQuality = CSettings::Low;
		m_Defaults.m_graphics.m_AnisotropicFiltering = 0;
		m_Defaults.m_graphics.m_LodScale = 0.0f;
		m_Defaults.m_graphics.m_CityDensity = 0.0f;
		m_Defaults.m_graphics.m_PedLodBias = 0.0f;
		m_Defaults.m_graphics.m_SSAO = CSettings::Low;
		m_Defaults.m_graphics.m_FXAA_Enabled = false;
		m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
		m_Defaults.m_graphics.m_ShaderQuality = CSettings::Low;
		m_Defaults.m_graphics.m_PostFX = CSettings::Low;
		m_Defaults.m_graphics.m_DoF = false;
		m_Defaults.m_graphics.m_HdStreamingInFlight = false;
		m_Defaults.m_graphics.m_MaxLodScale = 0.0f;
		m_Defaults.m_graphics.m_GrassQuality = CSettings::Low;

		m_Defaults.m_video.m_ScreenWidth = 800;
		m_Defaults.m_video.m_ScreenHeight = 600;
		//m_setMinTextureQuality = true;

		return;
	} //minimum specs

	if (m_memBandwidth < 40 * oneMillion)
	{
		m_Defaults.m_graphics.m_Tessellation = CSettings::Low;
		m_Defaults.m_graphics.m_ShadowQuality = CSettings::Medium;
		m_Defaults.m_graphics.m_ReflectionQuality = CSettings::Medium;
		m_Defaults.m_graphics.m_ReflectionMSAA = 0;
		m_Defaults.m_graphics.m_ParticleQuality = CSettings::Low;
		m_Defaults.m_graphics.m_WaterQuality = CSettings::Medium;
		m_Defaults.m_graphics.m_AnisotropicFiltering = 0;
		m_Defaults.m_graphics.m_LodScale = 1.0f;
		m_Defaults.m_graphics.m_CityDensity = 1.0f;
		m_Defaults.m_graphics.m_SSAO = CSettings::Medium;
		m_Defaults.m_graphics.m_FXAA_Enabled = true;
		m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
		m_Defaults.m_graphics.m_ShaderQuality = CSettings::Medium;
		m_Defaults.m_graphics.m_PostFX = CSettings::Low;
		m_Defaults.m_graphics.m_DoF = false;
		m_Defaults.m_graphics.m_HdStreamingInFlight = false;
		m_Defaults.m_graphics.m_MaxLodScale = 0.0f;
		m_Defaults.m_graphics.m_MotionBlurStrength = 0.0f;
		m_Defaults.m_graphics.m_GrassQuality = CSettings::Medium;

		m_defaultDisplayWindow.uWidth = 1280;
		m_defaultDisplayWindow.uHeight = 720;

		SetResolutionToClosestMode ();

		m_Defaults.m_video.m_ScreenWidth = m_defaultDisplayWindow.uWidth;
		m_Defaults.m_video.m_ScreenHeight = m_defaultDisplayWindow.uHeight;
		m_Defaults.m_video.m_RefreshRate = m_defaultDisplayWindow.uRefreshRate;

		return;
	}

	m_Defaults.m_graphics.m_Tessellation = CSettings::Low;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionMSAA = 0;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_AnisotropicFiltering = 0;
	m_Defaults.m_graphics.m_LodScale = 1.0f;
	m_Defaults.m_graphics.m_CityDensity = 1.0f;
	m_Defaults.m_graphics.m_SSAO = CSettings::Medium;
	m_Defaults.m_graphics.m_FXAA_Enabled = true;
	m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
	m_Defaults.m_graphics.m_ShaderQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_PostFX = CSettings::Medium;
	m_Defaults.m_graphics.m_DoF = false;
	m_Defaults.m_graphics.m_HdStreamingInFlight = false;
	m_Defaults.m_graphics.m_MaxLodScale = 0.0f;
	m_Defaults.m_graphics.m_MotionBlurStrength = 0.0f;
	m_Defaults.m_graphics.m_GrassQuality = CSettings::Medium;

	m_defaultDisplayWindow.uWidth = 1280;
	m_defaultDisplayWindow.uHeight = 720;

	SetResolutionToClosestMode ();

	m_Defaults.m_video.m_ScreenWidth = m_defaultDisplayWindow.uWidth;
	m_Defaults.m_video.m_ScreenHeight = m_defaultDisplayWindow.uHeight;
	m_Defaults.m_video.m_RefreshRate = m_defaultDisplayWindow.uRefreshRate;
}

void SettingsDefaults::getATIDefaults() {
//	defaultStereo3DEnabled = 0;

	s64 oneMillion = 1000000;
	s64 resolution = m_defaultDisplayWindow.uWidth * m_defaultDisplayWindow.uHeight;
	float baseResolution = 1024 * 1600;
	float resFactor = resolution / baseResolution;// * (defaultStereo3DEnabled + 1);

	InitializeToMaximumDefaultSettings();

	if ((m_memBandwidth < 30 * oneMillion) || (m_videoMemSize < 1024 * 1024 * 900)) { //Also use lowest settings if less than a gig of video memory
		downgradeToLevel0Settings();
		return;
	} //minimum specs

	int minResSetting = 80;
	int maxResSetting = 200;
	static int iMinWidth = 1024;
	static int iMinHeight = 768;
	PARAM_MinWidth.Get(iMinWidth);
	PARAM_MinHeight.Get(iMinHeight);

	if (m_memBandwidth < minResSetting * oneMillion) {
		downgradeToLevel1Settings();
		return;
	} //minimum resolution console settings

	if (m_memBandwidth < maxResSetting * oneMillion * resFactor) {
		downgradeToLevel3Settings();

		s64 powerOverMin = m_memBandwidth - minResSetting * oneMillion;
		float percentageOfMaxRes = powerOverMin / float((maxResSetting - minResSetting) * oneMillion * resFactor);
		m_defaultDisplayWindow.uWidth = iMinWidth + (s64)((m_defaultDisplayWindow.uWidth - iMinWidth) * percentageOfMaxRes);
		m_defaultDisplayWindow.uHeight = iMinHeight + (s64)((m_defaultDisplayWindow.uHeight - iMinHeight) * percentageOfMaxRes);

		SetResolutionToClosestMode ();

		m_Defaults.m_video.m_ScreenWidth = m_defaultDisplayWindow.uWidth;
		m_Defaults.m_video.m_ScreenHeight = m_defaultDisplayWindow.uHeight;
		m_Defaults.m_video.m_RefreshRate = m_defaultDisplayWindow.uRefreshRate;

		return;
	} //maximum resolution console settings

	if (m_memBandwidth < 300 * oneMillion * resFactor) {
		downgradeToLevel4Settings();
		return;
	} //maximum resolution high settings

	return; //maximum resolution max settings
}

void SettingsDefaults::downgradeToLevel0Settings()
{
	m_Defaults.m_graphics.m_Tessellation = CSettings::Low;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_ReflectionQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_ReflectionMSAA = 0;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::Low;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::Low;
	m_Defaults.m_graphics.m_AnisotropicFiltering = 0;
	m_Defaults.m_graphics.m_LodScale = 0.0f;
	m_Defaults.m_graphics.m_CityDensity = 0.0f;
	m_Defaults.m_graphics.m_PedLodBias = 0.0f;
	m_Defaults.m_graphics.m_SSAO = CSettings::Low;
	m_Defaults.m_graphics.m_FXAA_Enabled = false;
	m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
	m_Defaults.m_graphics.m_ShaderQuality = CSettings::Low;
	m_Defaults.m_graphics.m_PostFX = CSettings::Low;
	m_Defaults.m_graphics.m_DoF = false;
	m_Defaults.m_graphics.m_HdStreamingInFlight = false;
	m_Defaults.m_graphics.m_MaxLodScale = 0.0f;
	m_Defaults.m_graphics.m_MotionBlurStrength = 0.0f;
	m_Defaults.m_graphics.m_GrassQuality = CSettings::Low;

	m_Defaults.m_video.m_ScreenWidth = 800;
	m_Defaults.m_video.m_ScreenHeight = 600;
	m_Defaults.m_video.m_VSync = 2;

	m_currentDefaultSettingsLevel = 0;
}

void SettingsDefaults::downgradeToLevel1Settings()
{
	m_Defaults.m_graphics.m_Tessellation = CSettings::Low;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_ReflectionQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionMSAA = 0;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::Low;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_AnisotropicFiltering = 4;
	m_Defaults.m_graphics.m_SSAO = CSettings::Medium;
	m_Defaults.m_graphics.m_LodScale = 0.7f;
	m_Defaults.m_graphics.m_CityDensity = 0.5f;
	m_Defaults.m_graphics.m_FXAA_Enabled = false;
	m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
	m_Defaults.m_graphics.m_ShaderQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_PostFX = CSettings::Low;
	m_Defaults.m_graphics.m_DoF = false;
	m_Defaults.m_graphics.m_HdStreamingInFlight = false;
	m_Defaults.m_graphics.m_MaxLodScale = 0.0f;
	m_Defaults.m_graphics.m_MotionBlurStrength = 0.0f;
	m_Defaults.m_graphics.m_GrassQuality = CSettings::Low;

	static int iMinWidth = 1024;
	static int iMinHeight = 768;
	PARAM_MinWidth.Get(iMinWidth);
	PARAM_MinHeight.Get(iMinHeight);
	m_Defaults.m_video.m_ScreenWidth = iMinWidth;
	m_Defaults.m_video.m_ScreenHeight = iMinHeight;
	m_Defaults.m_video.m_VSync = 2;

	m_currentDefaultSettingsLevel = 1;
}

void SettingsDefaults::downgradeToLevel2Settings()
{
	static int iMinWidth = 1024;
	static int iMinHeight = 768;
	PARAM_MinWidth.Get(iMinWidth);
	PARAM_MinHeight.Get(iMinHeight);
	m_Defaults.m_video.m_ScreenWidth = iMinWidth;
	m_Defaults.m_video.m_ScreenHeight = iMinHeight;
	m_Defaults.m_video.m_VSync = 2;
	m_currentDefaultSettingsLevel = 2;
}

void SettingsDefaults::downgradeToLevel3Settings()
{
	m_Defaults.m_graphics.m_ShaderQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_Tessellation = CSettings::High;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionMSAA = 0;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_SSAO = CSettings::High;
	m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Medium;
	m_Defaults.m_graphics.m_PostFX = CSettings::High;
	m_Defaults.m_graphics.m_GrassQuality = CSettings::Low;

	m_currentDefaultSettingsLevel = 3;
}

void SettingsDefaults::downgradeToLevel4Settings()
{
	m_Defaults.m_graphics.m_ReflectionMSAA = 0;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::High;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_Tessellation = CSettings::High;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::High;
	m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::High;
	m_Defaults.m_graphics.m_SSAO = CSettings::High;
	m_Defaults.m_graphics.m_PostFX = CSettings::High;
	m_Defaults.m_graphics.m_GrassQuality = CSettings::Medium;

	m_currentDefaultSettingsLevel = 4;
}

void SettingsDefaults::InitializeToMaximumDefaultSettings()
{
	m_Defaults.m_video.m_ScreenWidth = m_defaultDisplayWindow.uWidth;
	m_Defaults.m_video.m_ScreenHeight = m_defaultDisplayWindow.uHeight;
	m_Defaults.m_video.m_Windowed = 0;
	m_Defaults.m_video.m_VSync = 1;

	m_Defaults.m_graphics.m_Tessellation = CSettings::Ultra;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionQuality = CSettings::High;
	m_Defaults.m_graphics.m_ReflectionMSAA = 4;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::High;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::High;
	m_Defaults.m_graphics.m_ShaderQuality = CSettings::High;
	m_Defaults.m_graphics.m_PostFX = CSettings::High;
	m_Defaults.m_graphics.m_LodScale = 1.0f;
	m_Defaults.m_graphics.m_CityDensity = 1.0f;
	m_Defaults.m_graphics.m_PedVarietyMultiplier = 0.8f;
	m_Defaults.m_graphics.m_VehicleVarietyMultiplier = 0.8f;
	m_Defaults.m_graphics.m_PedLodBias = 0.2f;
	m_Defaults.m_graphics.m_VehicleLodBias = 0.0f;
	m_Defaults.m_graphics.m_SSAO = CSettings::High;
	m_Defaults.m_graphics.m_AnisotropicFiltering = 16;
	m_Defaults.m_graphics.m_MSAA = 0;
	m_Defaults.m_graphics.m_MSAAFragments = 0;
	m_Defaults.m_graphics.m_MSAAQuality = 0;
	m_Defaults.m_graphics.m_SamplingMode = 0;
	m_Defaults.m_graphics.m_FXAA_Enabled = true;
	m_Defaults.m_graphics.m_Shadow_SoftShadows = CSettings::Ultra;
	m_Defaults.m_graphics.m_HdStreamingInFlight = false;
	m_Defaults.m_graphics.m_MaxLodScale = 0.0f;
	m_Defaults.m_graphics.m_MotionBlurStrength = 0.0f;
	m_Defaults.m_graphics.m_DoF = true;

	// Allow shadow angle past 5pm to cast really long shadows
	m_Defaults.m_graphics.m_Shadow_LongShadows = false; // No GUI/Command line control - Could hook this up to ultra shadows
	// Allow casting distance to increase.  
	m_Defaults.m_graphics.m_Shadow_Distance = 1.0f; // // No GUI/Command line control - Could hook this up to ultra shadows
	// Can turn off reflection blur
	m_Defaults.m_graphics.m_Reflection_MipBlur = true;
	// Fog Light Volumes
	m_Defaults.m_graphics.m_Lighting_FogVolumes = true;

	m_Defaults.m_graphics.m_Shadow_ParticleShadows = true; // No GUI/Command line control - Shadow overrides it
	m_Defaults.m_graphics.m_GrassQuality = CSettings::High;
	m_Defaults.m_graphics.m_Shader_SSA = true; // No GUI/Command line control - MSAA has the ability to disable it

	m_currentDefaultSettingsLevel = 5;
}

void SettingsDefaults::LimitDefaultsForWeakCpu()
{
	char vendorStr[12];
	u32 cpuInfo[4];
	__cpuid((int*)cpuInfo, 0);

	*(u32*)(&vendorStr[0]) = cpuInfo[1];
	*(u32*)(&vendorStr[4]) = cpuInfo[3];
	*(u32*)(&vendorStr[8]) = cpuInfo[2];

	LARGE_INTEGER cpuFrequency;
	QueryPerformanceFrequency(&cpuFrequency);

	// Lower settings for older CPUs if they have a low clock frequency.
	if ((!strncmp(vendorStr, "GenuineIntel", sizeof(vendorStr)) && (cpuFrequency.QuadPart < MINREQ_INTEL_CPU_FREQUENCY)) ||
		(!strncmp(vendorStr, "AuthenticAMD", sizeof(vendorStr)) && (cpuFrequency.QuadPart < MINREQ_AMD_CPU_FREQUENCY)))
	{
		__cpuid((int*)cpuInfo, 1);
		bool isAvxSupported = (cpuInfo[2] & (1 << 28)) != 0;

		// If AVX instructions aren't supported, this is likely a pre-2011 CPU. AMD introduced
		//  AVX support with Bulldozer and Intel with Sandy Bridge.
		if (!isAvxSupported)
		{
			m_Defaults.m_graphics.m_LodScale = 0.0f;
			m_Defaults.m_graphics.m_ShaderQuality = CSettings::Low;
			m_Defaults.m_graphics.m_CityDensity = 0.0f;
			m_Defaults.m_graphics.m_GrassQuality = CSettings::Low;
		}
	}
}

void SettingsDefaults::minimumSpecNoCheapShaders() {
//	defaultStereo3DEnabled = 0;

	m_Defaults.m_graphics.m_Tessellation = CSettings::Low;
	m_Defaults.m_graphics.m_ShadowQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_ReflectionQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_ReflectionMSAA = 0;
	m_Defaults.m_graphics.m_ParticleQuality = CSettings::Low;
	m_Defaults.m_graphics.m_WaterQuality = CSettings::Low;
	m_Defaults.m_graphics.m_ShaderQuality = CSettings::Medium;
	m_Defaults.m_graphics.m_AnisotropicFiltering = 0;
	m_Defaults.m_graphics.m_SSAO = CSettings::Low;
	m_Defaults.m_graphics.m_PostFX = CSettings::Low;
	m_Defaults.m_graphics.m_DoF = false;
	m_Defaults.m_graphics.m_GrassQuality = CSettings::Medium;

	m_Defaults.m_video.m_ScreenWidth = 800;
	m_Defaults.m_video.m_ScreenHeight = 600;

	// m_setMinTextureQuality = true;
}

void SettingsDefaults::PerformMinSpecTests()
{
	// Check if the PC meets minimum requirements
	if (sysMemTotalMemory() < MINREQ_PHYSICAL_MEMORY)
	{
		DisplayMinReqMsgBox(ERR_SYS_MINREQ_MEM);
	}

	VerifyCpuSpecs();

	DXGI_ADAPTER_DESC oAdapterDesc;

	IDXGIAdapter* pDXGIAdapter = ((const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(GRCDEVICE.GetAdapterOrdinal()))->GetDeviceAdapter();
	AssertMsg(pDXGIAdapter, "Could not get the current device adapter");

	if( SUCCEEDED( pDXGIAdapter->GetDesc(&oAdapterDesc) ) )
	{
		const SIZE_T uMinVideoMemory = 900 * 1024 * 1024;
		const s64 dedicatedVideoMemory = (s64)(( oAdapterDesc.DedicatedVideoMemory < uMinVideoMemory ) ? ((oAdapterDesc.SharedSystemMemory < uMinVideoMemory) ? (2048ULL * 1024ULL * 1024ULL) : oAdapterDesc.SharedSystemMemory) : oAdapterDesc.DedicatedVideoMemory);
		if ( dedicatedVideoMemory < MINREQ_VIDEO_MEMORY)
		{
			if (dedicatedVideoMemory > 0 &&
				wcsstr((const wchar_t*)&oAdapterDesc.Description, L"Intel"))
			{
				// For now assume that any Intel solutions with dedicated video memory are Iris or newer.
				// If any older solutions pass our min spec rating, we can explicitly check for them via description or ID.
			}
			else
			{
				DisplayMinReqMsgBox(ERR_SYS_MINREQ_VID_MEM);
			}
		}
	}
}

void SettingsDefaults::VerifyCpuSpecs()
{
	char vendorStr[12];
	u32 cpuInfo[4];
	__cpuid((int*)cpuInfo, 0);

	*(u32*)(&vendorStr[0]) = cpuInfo[1];
	*(u32*)(&vendorStr[4]) = cpuInfo[3];
	*(u32*)(&vendorStr[8]) = cpuInfo[2];

	if (!strncmp(vendorStr, "GenuineIntel", sizeof(vendorStr)))
	{
		__cpuid((int*)cpuInfo, 1);

		if (sysIpcGetProcessorCount() < MINREQ_CPU_CORE_COUNT)
		{
			bool isHyperThreaded = (cpuInfo[3] & (1 << 28)) != 0;
			if ((sysIpcGetProcessorCount() * 2) >= MINREQ_CPU_CORE_COUNT && isHyperThreaded)
			{
				// Dual core with Hyper-threading is okay
			}
			else
			{
				DisplayMinReqMsgBox(ERR_SYS_MINREQ_CPU_CORE);
			}
		}

		LARGE_INTEGER cpuFrequency;
		QueryPerformanceFrequency(&cpuFrequency);

		if (cpuFrequency.QuadPart < MINREQ_INTEL_CPU_FREQUENCY)
		{
			DisplayMinReqMsgBox(ERR_SYS_MINREQ_CPU_FREQ);
		}

		// TODO: We may need to let lower clocked but newer CPUs through.
		//        Use __cpuid to check for supported extensions like AVX2.
	}
	else if (!strncmp(vendorStr, "AuthenticAMD", sizeof(vendorStr)))
	{
		if (sysIpcGetProcessorCount() < MINREQ_CPU_CORE_COUNT)
		{
			DisplayMinReqMsgBox(ERR_SYS_MINREQ_CPU_CORE);
		}

		LARGE_INTEGER cpuFrequency;
		QueryPerformanceFrequency(&cpuFrequency);

		if (cpuFrequency.QuadPart < MINREQ_AMD_CPU_FREQUENCY)
		{
			DisplayMinReqMsgBox(ERR_SYS_MINREQ_CPU_FREQ);
		}

		// TODO: We may need to let lower clocked but newer CPUs through.
		//        Use __cpuid to check for supported extensions like ABM.
	}
	else
	{
		// Unknown CPU Vendor. Might not be able to run the game.
		DisplayMinReqMsgBox(ERR_SYS_MINREQ_CPU);
	}
}

void SettingsDefaults::DisplayMinReqMsgBox(int errorCode)
{
#if !__FINAL
	const char* pSmokeTestName = NULL;
	if(SmokeTests::PARAM_smoketest.Get(pSmokeTestName))
		return;

	const char* pScriptName = NULL;
	PARAM_runscript.Get(pScriptName);
	if(pScriptName && !stricmp(pScriptName,"fps_test"))
		return;
#endif

	int msgBoxResult =  MessageBoxW(NULL, rage::diagErrorCodes::GetErrorMessageString(static_cast<rage::FatalErrorCode>(errorCode)), rage::diagErrorCodes::GetErrorMessageString(ERR_SYS_MINREQ_TITLE), MB_OKCANCEL | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
	if (msgBoxResult == IDCANCEL)
	{
		ShellExecuteW(NULL, L"open", rage::diagErrorCodes::GetErrorMessageString(ERR_SYS_MINREQ_URL), NULL, NULL, SW_SHOWMAXIMIZED);
		ExitProcess(1);
	}
}

bool SettingsDefaults::IsLowQualitySystem(const Settings &settings)
{
	const grcAdapterD3D11* adapterTest = (grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(settings.m_video.m_AdapterIndex);

	IDXGIAdapter* pDeviceAdapter = adapterTest->GetDeviceAdapter();
	AssertMsg(pDeviceAdapter, "Could not get the current device adapter");

	DXGI_ADAPTER_DESC oAdapterDesc;
	pDeviceAdapter->GetDesc(&oAdapterDesc);


	if (oAdapterDesc.VendorId == 0x1002 || oAdapterDesc.VendorId == 0x1022)
	{
		if ((oAdapterDesc.DeviceId == 0x9440 ||  //AMD Radeon HD 4870
			oAdapterDesc.DeviceId == 0x9441 ||  //AMD Radeon HD 4870 X2
			oAdapterDesc.DeviceId == 0x9442 ||  //AMD Radeon HD 4850
			oAdapterDesc.DeviceId == 0x9443 ||  //AMD Radeon HD 4850 X2
			oAdapterDesc.DeviceId == 0x9460 ||  //AMD Radeon HD 4890
			oAdapterDesc.DeviceId == 0x68c8     //ATI FirePro V4800 (FireGL) Graphics Adapter
			) &&
			(sysIpcGetProcessorCount() <= 4)
			)
		{
			return true;
		}
	}
	return false;
}
#endif //__WIN32PC
