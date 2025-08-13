
//rage
#include "bank/bank.h"
#include "math/amath.h"
#include "grcore/allocscope.h"
#include "grcore/viewport.h"
#include "grcore/quads.h"
#include "grcore/wrapper_d3d.h"
#include "grmodel/shaderfx.h"
#include "vectormath/mat34v.h"
#include "fwdebug/picker.h"

//game
#include "UI3DDrawManager.h"

#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/HudTools.h"
#include "game/Clock.h"
#include "Peds/ped.h"
#include "Peds/PedFactory.h"
#include "Peds/rendering/peddamage.h"
#include "renderer/Lights/lights.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "script/script.h"
#include "scene/world/gameworld.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Lights/LightSource.h"
#include "renderer/Util/ShaderUtil.h"

#define UI_SCENE_METADATA_FILE_NAME_FULL_PATH	"common:/data/ui/uiscenes" 

#define UI_START_RENDER_DEBUG		(false)

#include "parser/manager.h"
#include "UI3DDrawManager_parser.h"

//OPTIMISATIONS_OFF();


void DrawUIRect(const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4,
						 float zVal,
						 const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4,
						 const Color32 &color)
{
	grcBegin(drawTriStrip,4);
	grcVertex(pt1.x, pt1.y, zVal, 0, 0, -1, color, uv1.x, uv1.y);
	grcVertex(pt2.x, pt2.y, zVal, 0, 0, -1, color, uv2.x, uv2.y);
	grcVertex(pt3.x, pt3.y, zVal, 0, 0, -1, color, uv3.x, uv3.y);
	grcVertex(pt4.x, pt4.y, zVal, 0, 0, -1, color, uv4.x, uv4.y);
	grcEnd();
}

//////////////////////////////////////////////////////////////////////////
// Globals
static CLightSource gLightSources[4];

s32 UI3DDrawManager::sm_prevForcedTechniqueGroup = -1;
UI3DDrawManager* UI3DDrawManager::smp_Instance = NULL;

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::ClassInit(unsigned UNUSED_PARAM(initMode)) 
{
	smp_Instance = rage_new UI3DDrawManager();
	Assert(smp_Instance);

	if (smp_Instance)
	{
		UI3DDRAWMANAGER.Init();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::ClassShutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	UI3DDRAWMANAGER.Shutdown();

	delete smp_Instance;
	smp_Instance = NULL;
}

//////////////////////////////////////////////////////////////////////////
//
UI3DDrawManager::UI3DDrawManager()
{
	m_vCamPos					= Vec3V(V_ZERO);
	m_vCamAnglesDeg				= Vec3V(0.0f, 90.0f, 180.0f);
	m_CamFieldOfView			= 40.0f;

	m_currentRigIdx				= 0U;

	m_DSSWriteStencilHandle		= grcStateBlock::DSS_Invalid;
	m_DSSTestStencilHandle		= grcStateBlock::DSS_Invalid;

	m_prevInInterior			= false;
	
	m_bKeepCurrentPreset		= false;

#if __BANK
	m_bEnableRendering			= UI_START_RENDER_DEBUG;
	m_bRunSkinPass				= true;
	m_bRunSSAPass				= true;
	m_bRunForwardPass			= true;
	m_bRunFXAAPass				= true;
	m_bRunCompositePasses		= true;
	m_bDrawDebug				= false;
	m_bDisableScissor			= false;
#endif
}

//////////////////////////////////////////////////////////////////////////
//
UI3DDrawManager::~UI3DDrawManager()
{

}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::Shutdown()
{
#if RSG_PC
	if(m_pDepthRT) delete m_pDepthRT; m_pDepthRT = NULL;
	if(m_pDepthAliasRT) delete m_pDepthAliasRT; m_pDepthAliasRT = NULL;
	if(m_pDepthRTCopy) delete m_pDepthRTCopy; m_pDepthRTCopy = NULL;
	if(m_pLightRT) delete m_pLightRT; m_pLightRT = NULL;
	if(m_pLightRTCopy) delete m_pLightRTCopy; m_pLightRTCopy = NULL;
	if(m_pCompositeRT0) delete m_pCompositeRT0; m_pCompositeRT0 = NULL;
	if(m_pCompositeRTCopy) delete m_pCompositeRTCopy; m_pCompositeRTCopy = NULL;
	if(m_pCompositeRT1) delete m_pCompositeRT1; m_pCompositeRT1 = NULL;	
	if(m_pGBufferRTs[0]) delete m_pGBufferRTs[0]; m_pGBufferRTs[0] = NULL;
	if(m_pGBufferRTs[1]) delete m_pGBufferRTs[1]; m_pGBufferRTs[1] = NULL;
	if(m_pGBufferRTs[2]) delete m_pGBufferRTs[2]; m_pGBufferRTs[2] = NULL;
	if(m_pGBufferRTs[3]) delete m_pGBufferRTs[3]; m_pGBufferRTs[3] = NULL;
#endif
}

//////////////////////////////////////////////////////////////////////////
//
bool UI3DDrawManager::AssignPedToSlot(u32 slot, CPed* pPed, float alpha) 
{
	return AssignPedToSlot(slot, pPed, Vector3(VEC3_ZERO), Vector3(VEC3_ZERO), alpha);
}

//////////////////////////////////////////////////////////////////////////
//
bool UI3DDrawManager::AssignPedToSlot(u32 slot, CPed* pPed, Vector3::Vector3Param offset, float alpha) 
{
	return AssignPedToSlot(slot, pPed, offset, Vector3(VEC3_ZERO),alpha);
}

//////////////////////////////////////////////////////////////////////////
//
bool UI3DDrawManager::AssignPedToSlot(u32 slot, CPed* pPed, Vector3::Vector3Param offset, Vector3::Vector3Param eulerOffsets, float alpha) 
{
	if (Verifyf(slot < UI_MAX_SCENE_PRESET_ELEMENTS, "UI3DDrawManager::AssignPedToSlot: slot %u is out of range (%u max.)", slot, UI_MAX_SCENE_PRESET_ELEMENTS))
	{
		m_scenePresetPatchedData[slot].m_pPed = pPed;
		m_scenePresetPatchedData[slot].m_offset = offset;
		m_scenePresetPatchedData[slot].m_rotationXYZ = eulerOffsets;
		m_scenePresetPatchedData[slot].m_alpha = alpha;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool  UI3DDrawManager::AssignPedToSlot(atHashString presetName, u32 slot, CPed* pPed, float alpha)
{
	return AssignPedToSlot(presetName, slot, pPed, Vector3(VEC3_ZERO), Vector3(VEC3_ZERO), alpha);
}

//////////////////////////////////////////////////////////////////////////
//
bool  UI3DDrawManager::AssignPedToSlot(atHashString presetName, u32 slot, CPed* pPed, Vector3::Vector3Param offset, float alpha)
{
	return AssignPedToSlot(presetName, slot, pPed, offset, Vector3(VEC3_ZERO), alpha);
}

//////////////////////////////////////////////////////////////////////////
//
bool  UI3DDrawManager::AssignPedToSlot(atHashString presetName, u32 slot, CPed* pPed, Vector3::Vector3Param offset, Vector3::Vector3Param eulerOffset, float alpha)
{
	if (m_pCurrentPreset == NULL)
	{
		return false;
	}

	const UIScenePreset* pPreset = GetPreset(presetName);
	if (pPreset && pPreset->m_name == presetName)
	{
		return AssignPedToSlot(slot, pPed, offset, eulerOffset, alpha);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void  UI3DDrawManager::AssignGlobalLightIntensityToSlot(atHashString presetName, u32 slot, float intensity)
{
	if (m_pCurrentPreset == NULL)
	{
		return;
	}

	const UIScenePreset* pPreset = GetPreset(presetName);
	if (pPreset && pPreset->m_name == presetName)
	{
		if (Verifyf(slot < UI_MAX_SCENE_PRESET_ELEMENTS, "UI3DDrawManager::AssignGlobalLightIntensityToSlot: slot %u is out of range (%u max.)", slot, UI_MAX_SCENE_PRESET_ELEMENTS))
		{
			m_scenePresetPatchedData[slot].m_globalLightIntensity = Saturate(intensity);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool UI3DDrawManager::PushCurrentPreset(UIScenePreset* pPreset) 
{
	if (m_bKeepCurrentPreset)
	{
		Assertf(0, "UI3DDrawManager::PushCurrentPreset trying to set a new present when the current one has been flagged as persistent");
		return false;
	}

	if (Verifyf(m_pCurrentPreset == NULL, "UI3DDrawManager::PushCurrentPreset trying to set a new preset when one is already in use")) 
	{
		m_pCurrentPreset = pPreset;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool  UI3DDrawManager::PushCurrentPreset(atHashString presetName)
{
	UIScenePreset* pPreset = GetPreset(presetName);
	if (pPreset == NULL)
	{
		return false;
	}

	return PushCurrentPreset(pPreset);
}

bool UI3DDrawManager::IsUsingPreset(atHashString presetName) const
{
	return 	m_pCurrentPreset == GetPreset(presetName);
}

grcRenderTarget* UI3DDrawManager::GetDepthCopy()
{
#if RSG_PC
	return m_pDepthRTCopy;
#else
	return GetDepth();
#endif // RSG_PC
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::PopCurrentPreset() 
{
	if (m_bKeepCurrentPreset)
	{
		return;
	}

	m_pCurrentPreset = 0;

	if (m_bClearPatchedData)
	{
		for (int i = 0; i < UI_MAX_SCENE_PRESET_ELEMENTS; i++)
		{
			m_scenePresetPatchedData[i].Reset();
		}

		m_bClearPatchedData = false;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::InitRenderTargets()
{
#if __PS3
	grcTextureFactory::CreateParams params;

	params.Format		= grctfA16B16G16R16F;
	params.UseFloat		= true;

	m_pLightRT = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Light Buffer",	grcrtPermanent, 640, 360, 64, &params, 1); 
	m_pLightRT->AllocateMemoryFromPool();

	params.Format		= grctfA8R8G8B8;
	params.UseFloat		= false;
	params.IsSRGB		= false;

	m_pGBufferRTs[0] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Buffer 0",	grcrtPermanent, 640, 360, 32, &params, 1); 
	m_pGBufferRTs[0]->AllocateMemoryFromPool();
	
	m_pGBufferRTs[1] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Buffer 1",	grcrtPermanent, 640, 360, 32, &params, 1); 
	m_pGBufferRTs[1]->AllocateMemoryFromPool();
	
	m_pGBufferRTs[2] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Buffer 2",	grcrtPermanent, 640, 360, 32, &params, 1); 
	m_pGBufferRTs[2]->AllocateMemoryFromPool();
	
	m_pGBufferRTs[3] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Buffer 3",	grcrtPermanent, 640, 360, 32, &params, 1); 
	m_pGBufferRTs[3]->AllocateMemoryFromPool();

	m_pCompositeRT0 = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Composite Buffer",	grcrtPermanent, 640, 360, 32, &params, 1);
	m_pCompositeRT0->AllocateMemoryFromPool();

	m_pCompositeRT1 = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Deferred Composite Lum Buffer",	grcrtPermanent, 640, 360, 32, &params, 2);
	m_pCompositeRT1->AllocateMemoryFromPool();

	params.Format				= grctfA8R8G8B8;
	params.UseHierZ				= true;
	params.EnableCompression	= true;
	params.InTiledMemory		= true;
	params.Multisample			= 0;
	params.CreateAABuffer		= false;
	
	m_pDepthRT = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR_BBUFFER, "UI Depth Buffer",	grcrtDepthBuffer, 640, 360, 32, NULL, 3);
	m_pDepthRT->AllocateMemoryFromPool();

	params.InTiledMemory		= true;
	m_pDepthAliasRT = ((grcRenderTargetGCM*)m_pDepthRT)->CreatePrePatchedTarget(false);

#elif __XENON

	grcTextureFactory::CreateParams params;

	// Depth buffer
	params.Format		= grctfA8R8G8B8;
	params.IsResolvable = true;
	params.Multisample	= 0;
	params.HasParent	= true;
	params.Parent		= NULL;
	m_pDepthRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL, "UI Depth Buffer",	grcrtDepthBuffer, 640, 360, 32, &params, kDeferredPedUIHeap3);
	m_pDepthRT->AllocateMemoryFromPool();

	params.Format		= grctfA8R8G8B8;
	params.HasParent	= true;
	params.Parent		= NULL;
	m_pDepthAliasRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL, "UI Depth Buffer Alias", grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap5);
	m_pDepthAliasRT->AllocateMemoryFromPool();
	
	// Light buffer
	params.Format		= grctfA2B10G10R10;
	params.UseFloat		= true;
	params.IsResolvable = true;
	params.HasParent	= true;
	params.Parent		= m_pDepthRT;

	m_pLightRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0, "UI Deferred Light Buffer",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap0); 
	m_pLightRT->AllocateMemoryFromPool();

	// Composite Buffer 0
	params.Format		= grctfA8R8G8B8;
	params.UseFloat		= false;
	params.IsSRGB		= false;
	params.Parent		= m_pLightRT;

	m_pCompositeRT0 = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0, "UI Deferred Composite Buffer",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap0);
	m_pCompositeRT0->AllocateMemoryFromPool();

	// Composite Buffer 1
	params.Parent		= m_pCompositeRT0;
	m_pCompositeRT1 = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0, "UI Deferred Composite Lum Buffer",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap1);
	m_pCompositeRT1->AllocateMemoryFromPool();

	// GBuffer 0
	params.Parent		= m_pDepthRT;	
	m_pGBufferRTs[0] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "UI Deferred Buffer 0",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap2); 
	m_pGBufferRTs[0]->AllocateMemoryFromPool();

	// GBuffer 1
	params.Parent		= m_pGBufferRTs[0];
	m_pGBufferRTs[1] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "UI Deferred Buffer 1",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap2); 
	m_pGBufferRTs[1]->AllocateMemoryFromPool();

	// GBuffer 2
	params.Parent		= m_pGBufferRTs[1];
	m_pGBufferRTs[2] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "UI Deferred Buffer 2",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap2); 
	m_pGBufferRTs[2]->AllocateMemoryFromPool();

	// GBuffer 3
	params.Parent		= m_pGBufferRTs[2];
	m_pGBufferRTs[3] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_PERLIN_NOISE, "UI Deferred Buffer 3",	grcrtPermanent, 640, 360, 32, &params, kDeferredPedUIHeap4); 
	m_pGBufferRTs[3]->AllocateMemoryFromPool();

#else
#if RSG_PC
#define RT_WIDTH  ((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? GRCDEVICE.GetHeight()/2 : GRCDEVICE.GetWidth()/2)	// was 640
#define RT_HEIGHT ((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? GRCDEVICE.GetWidth()/2 : GRCDEVICE.GetHeight()/2)	// was 480
#else
#define RT_WIDTH  (960)	// was 640
#define RT_HEIGHT (540)	// was 480
#endif

	grcTextureFactory::CreateParams params;

	// Depth buffer
	params.Format		= grctfD24S8;
	params.IsResolvable = true;
	params.Multisample	= 0;
	params.HasParent	= true;
	params.Parent		= NULL;
	m_pDepthRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Depth Buffer",	grcrtDepthBuffer, RT_WIDTH, RT_HEIGHT, 32, &params);

	// Readonly
#if RSG_PC
	// GBuffer 0
	params.Parent		= m_pDepthRT;	
	params.IsResolvable = false;
	params.HasParent	= true;

	if(GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 
		m_pDepthRTCopy = textureFactory11.CreateRenderTarget("UI Depth Buffer Only", m_pDepthRT->GetTexturePtr(), params.Format, DepthStencilReadOnly);
	} else
		m_pDepthRTCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Depth Buffer Copy",	grcrtDepthBuffer, RT_WIDTH, RT_HEIGHT, 32, &params);

	params.IsResolvable = true;
#endif // RSG_PC

	params.Format		= grctfA8R8G8B8;
	params.HasParent	= true;
	params.Parent		= NULL;
	m_pDepthAliasRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Depth Buffer Alias", grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params);
	
	// Light buffer
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality == (CSettings::eSettingsLevel) (0))
		params.Format		= grctfA2B10G10R10ATI;
	else
		params.Format		= grctfA16B16G16R16F;
#else
	params.Format		= grctfA16B16G16R16F;
#endif
	params.UseFloat		= true;
	params.IsResolvable = true;
	params.HasParent	= true;
	params.Parent		= m_pDepthRT;

	m_pLightRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Light Buffer", grcrtPermanent, RT_WIDTH, RT_HEIGHT, params.Format == grctfA16B16G16R16F ? 64:32, &params); 

	m_pLightRTCopy = NULL;
#if RSG_PC
	m_pLightRTCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Light Buffer Copy", grcrtPermanent, RT_WIDTH, RT_HEIGHT, 64, &params); 
#endif // RSG_PC

	// Composite Buffer 0
	params.Format		= grctfA8R8G8B8;
	params.UseFloat		= false;
	params.IsSRGB		= false;
	params.Parent		= m_pLightRT;

	m_pCompositeRT0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Composite Buffer", grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params);

	// Composite Buffer 1
	params.Parent		= m_pCompositeRT0;
	m_pCompositeRT1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Composite Lum Buffer",	grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params);
	m_pCompositeRTCopy = NULL;
#if RSG_PC
	m_pCompositeRTCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Composite Lum Buffer Copy",	grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params);
#endif // RSG_PC

	// GBuffer 0
	params.Parent		= m_pDepthRT;	
	m_pGBufferRTs[0] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Buffer 0",	grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params); 

	// GBuffer 1
	params.Parent		= m_pGBufferRTs[0];
	m_pGBufferRTs[1] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Buffer 1",	grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params); 

	// GBuffer 2
	params.Parent		= m_pGBufferRTs[1];
	m_pGBufferRTs[2] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Buffer 2",	grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params); 

	// GBuffer 3
	params.Parent		= m_pGBufferRTs[2];
	m_pGBufferRTs[3] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UI Deferred Buffer 3",	grcrtPermanent, RT_WIDTH, RT_HEIGHT, 32, &params); 
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::Init()
{

	LoadData();

	for (int i = 0; i < UI_MAX_SCENE_PRESET_ELEMENTS; i++)
	{
		m_scenePresetPatchedData[i].Reset();
	}

	m_bClearPatchedData = false;

	m_pCurrentPreset = NULL;
	m_bKeepCurrentPreset = false;

	m_pLastViewport = NULL;
	m_Viewport.SetCameraIdentity(); // UT

	grcDepthStencilStateDesc dsd;
	dsd.StencilEnable = TRUE;
	dsd.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	dsd.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	dsd.DepthFunc = FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_DSSWriteStencilHandle = grcStateBlock::CreateDepthStencilState(dsd);

	grcDepthStencilStateDesc dsdTest;
	dsdTest.DepthEnable = TRUE;
	dsdTest.DepthWriteMask = FALSE;
	dsdTest.DepthFunc = grcRSV::CMP_ALWAYS;
	dsdTest.StencilEnable = TRUE;
	dsdTest.StencilReadMask = 0xFF;
	dsdTest.StencilWriteMask = 0x00;
	dsdTest.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	dsdTest.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_DSSTestStencilHandle = grcStateBlock::CreateDepthStencilState(dsdTest);

#if NV_SUPPORT
	if((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()))
		m_vCamAnglesDeg.SetYf(0.0);
#endif

#if DEFERRED_RENDER_UI

	InitRenderTargets();

#endif

	grcDepthStencilStateDesc insideVolumeDepthStencilState;
	insideVolumeDepthStencilState.DepthWriteMask = FALSE;
	insideVolumeDepthStencilState.DepthEnable = TRUE;
	insideVolumeDepthStencilState.DepthFunc = FixupDepthDirection(grcRSV::CMP_GREATER);
	insideVolumeDepthStencilState.StencilEnable = TRUE;	
	insideVolumeDepthStencilState.StencilReadMask = 0xFF;
	insideVolumeDepthStencilState.StencilWriteMask = 0x00;
	insideVolumeDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	insideVolumeDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	insideVolumeDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	insideVolumeDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	insideVolumeDepthStencilState.BackFace = insideVolumeDepthStencilState.FrontFace; 
	m_insideLightVolumeDSS = grcStateBlock::CreateDepthStencilState(insideVolumeDepthStencilState);

	grcDepthStencilStateDesc outsideVolumeDepthStencilState;
	outsideVolumeDepthStencilState.DepthWriteMask = FALSE;
	outsideVolumeDepthStencilState.DepthEnable = TRUE;
	outsideVolumeDepthStencilState.DepthFunc = FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	outsideVolumeDepthStencilState.StencilEnable = insideVolumeDepthStencilState.StencilEnable;
	outsideVolumeDepthStencilState.StencilReadMask = insideVolumeDepthStencilState.StencilReadMask;
	outsideVolumeDepthStencilState.StencilWriteMask = insideVolumeDepthStencilState.StencilWriteMask;
	outsideVolumeDepthStencilState.FrontFace = insideVolumeDepthStencilState.FrontFace;
	outsideVolumeDepthStencilState.BackFace = insideVolumeDepthStencilState.BackFace;
	m_outsideLightVolumeDSS = grcStateBlock::CreateDepthStencilState(outsideVolumeDepthStencilState);
		
#endif
}

#if RSG_PC
//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DeviceLost()
{
	if (smp_Instance)
	{
		UI3DDRAWMANAGER.Shutdown();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DeviceReset()
{
	if (smp_Instance)
	{
		UI3DDRAWMANAGER.Init();
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::Update()
{
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::PreRender()
{
	if (m_pCurrentPreset)
	{
		for (int i = 0; i < m_pCurrentPreset->m_elements.GetCount(); i++)
		{
			const UISceneObjectData* pObject = &(m_pCurrentPreset->m_elements[i]);

			if (pObject->m_enabled && m_scenePresetPatchedData[i].m_pPed)
			{
				bool bAlreadyPreRendered = false;
				for(int j = 0; j < i; j ++)
				{
					if(m_pCurrentPreset->m_elements[j].m_enabled &&  m_scenePresetPatchedData[j].m_pPed == m_scenePresetPatchedData[i].m_pPed)
					{
						bAlreadyPreRendered = true;
					}
				}

				u8 damageSetId = m_scenePresetPatchedData[i].m_pPed->GetDamageSetID();

				if (!fwTimer::IsGamePaused() && !bAlreadyPreRendered)
				{
					fwAnimDirector* director = m_scenePresetPatchedData[i].m_pPed->GetAnimDirector();
					director->WaitForAnyActiveUpdateToComplete(false);
					director->StartUpdatePreRender(fwTimer::GetTimeStep());
					director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhasePreRender);
				}

				if (damageSetId!=kInvalidPedDamageSet)
					CPedDamageManager::GetInstance().UpdatePriority(damageSetId, 4.0f, true, false);
			}	
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool UI3DDrawManager::LoadPresets()
{
	bool bOk = false;
	bOk = PARSER.LoadObject(UI_SCENE_METADATA_FILE_NAME_FULL_PATH, "meta", *this);

	if (bOk == false)
	{
		Displayf("UI3DDrawManager::LoadPresets: Failed to load data for \"%s\"", UI_SCENE_METADATA_FILE_NAME_FULL_PATH);
		return bOk;
	}

	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK && !__FINAL
bool UI3DDrawManager::SavePresets() const
{
	bool bOk = false;
	bOk = PARSER.SaveObject(UI_SCENE_METADATA_FILE_NAME_FULL_PATH, "meta", this);

	if (bOk == false)
	{
		Displayf("UI3DDrawManager::SavePresets: Failed to save data for \"%s\"", UI_SCENE_METADATA_FILE_NAME_FULL_PATH);
		return bOk;
	}

	return bOk;
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::AddPreset(UIScenePreset& preset)
{
	m_scenePresets.PushAndGrow(preset);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::RemovePreset(atHashString presetName)
{
	for (int i = 0; i < m_scenePresets.GetCount(); i++)
	{
		if (m_scenePresets[i].m_name == presetName)
		{
			m_scenePresets.Delete(i);
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
UIScenePreset* UI3DDrawManager::GetPreset(atHashString presetName)
{
	for (int i = 0; i < m_scenePresets.GetCount(); i++)
	{
		if (m_scenePresets[i].m_name == presetName)
		{
			return &m_scenePresets[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
const UIScenePreset* UI3DDrawManager::GetPreset(atHashString presetName) const
{
	for (int i = 0; i < m_scenePresets.GetCount(); i++)
	{
		if (m_scenePresets[i].m_name == presetName)
		{
			return &m_scenePresets[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
s32 g_LockedTargetIndex = -1;
void UI3DDrawManager::LockRenderTarget(u32 idx, bool bLockDepth)
{
	grcRenderTarget* colorRT;
	g_LockedTargetIndex = idx;
	if(idx)
		colorRT = CRenderTargets::GetOffscreenBuffer2();
	else
		colorRT = CRenderTargets::GetOffscreenBuffer1();

	grcRenderTarget *depthRT = CRenderTargets::GetUIDepthBuffer();

	if (bLockDepth == false)
		depthRT = NULL;

	grcTextureFactory::GetInstance().LockRenderTarget(0, colorRT, depthRT);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::UnlockRenderTarget(bool XENON_ONLY(bResolveCol), bool XENON_ONLY(bResolveDepth))
{
	Assert(g_LockedTargetIndex >= 0);

	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = false;

	if(bResolveCol)
	{
		grcRenderTarget* colorRT;
		if(g_LockedTargetIndex)
			colorRT = CRenderTargets::GetOffscreenBuffer2();
		else
			colorRT = CRenderTargets::GetOffscreenBuffer1();
		colorRT->Realize();

	}

	if (bResolveDepth)
	{
		grcRenderTarget* depthRT = CRenderTargets::GetUIDepthBuffer();
		depthRT->Realize();
	}

#endif //__XENON

	g_LockedTargetIndex = -1;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);
}

#if DEFERRED_RENDER_UI

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::LockRenderTargets(bool bLockDepth)
{
	grcRenderTarget* depthTarget = (bLockDepth) ? UI3DDRAWMANAGER.GetDepth() : NULL;

#if __XENON 
	grcTextureFactory::GetInstance().LockRenderTarget(0, UI3DDRAWMANAGER.GetTarget(0), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(1, UI3DDRAWMANAGER.GetTarget(1), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(2, UI3DDRAWMANAGER.GetTarget(2), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(3, UI3DDRAWMANAGER.GetTarget(3), depthTarget);
#endif //__XENON

#if __PPU || __D3D11 || RSG_ORBIS
	const grcRenderTarget* rendertargets[grcmrtColorCount] = {
		UI3DDRAWMANAGER.GetTarget(0),
		UI3DDRAWMANAGER.GetTarget(1),
		UI3DDRAWMANAGER.GetTarget(2),
		UI3DDRAWMANAGER.GetTarget(3)
	};

	grcTextureFactory::GetInstance().LockMRT(rendertargets, depthTarget);

#elif __WIN32PC
	grcTextureFactory::GetInstance().LockRenderTarget(0, UI3DDRAWMANAGER.GetTarget(0), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(1, UI3DDRAWMANAGER.GetTarget(1), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(2, UI3DDRAWMANAGER.GetTarget(2), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(3, UI3DDRAWMANAGER.GetTarget(3), depthTarget);
#endif //__PPU || __WIN32
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::UnlockRenderTargets(bool XENON_ONLY(bResolveCol), bool XENON_ONLY(bResolveDepth), const Vector4& XENON_ONLY(scissorRect))
{
#if __XENON 

	const int adjustedScissorX = (int)(floorf((scissorRect.x) * 0.125f) * 8.0f);
	const int adjustedScissorY = (int)(floorf((scissorRect.y) * 0.125f) * 8.0f);

	const int adjustedScissorW = (int)(ceilf ((scissorRect.z + 1) * 0.125f) * 8.0f);
	const int adjustedScissorH = (int)(ceilf ((scissorRect.w + 1) * 0.125f) * 8.0f);

	D3DRECT srcRect = { adjustedScissorX, adjustedScissorY, adjustedScissorX + adjustedScissorW, adjustedScissorY + adjustedScissorH };
	D3DPOINT dstPoint = { adjustedScissorX, adjustedScissorY };

	if (bResolveCol)
	{
		GRCDEVICE.GetCurrent()->Resolve( 
			D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, 
			&srcRect, 
			UI3DDRAWMANAGER.GetTarget(0)->GetTexturePtr(), 
			&dstPoint, 
			0, 0, NULL, 0.0f, 0L, NULL);

		GRCDEVICE.GetCurrent()->Resolve( 
			D3DRESOLVE_RENDERTARGET1 | D3DRESOLVE_ALLFRAGMENTS, 
			&srcRect, 
			UI3DDRAWMANAGER.GetTarget(1)->GetTexturePtr(), 
			&dstPoint, 
			0, 0, NULL, 0.0f, 0L, NULL);

		GRCDEVICE.GetCurrent()->Resolve( 
			D3DRESOLVE_RENDERTARGET2 | D3DRESOLVE_ALLFRAGMENTS, 
			&srcRect, 
			UI3DDRAWMANAGER.GetTarget(2)->GetTexturePtr(), 
			&dstPoint, 
			0, 0, NULL, 0.0f, 0L, NULL);

		GRCDEVICE.GetCurrent()->Resolve( 
			D3DRESOLVE_RENDERTARGET3 | D3DRESOLVE_ALLFRAGMENTS, 
			&srcRect, 
			UI3DDRAWMANAGER.GetTarget(3)->GetTexturePtr(), 
			&dstPoint, 
			0, 0, NULL, 0.0f, 0L, NULL);
	}

	if (bResolveDepth)
	{
		GRCDEVICE.GetCurrent()->Resolve( 
			D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_ALLFRAGMENTS, 
			&srcRect, 
			UI3DDRAWMANAGER.GetDepth()->GetTexturePtr(),
			&dstPoint, 
			0, 0, NULL, 0.0f, 0L, NULL);
	}

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(3, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(2, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

	/*
	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = bResolveCol;
	grcTextureFactory::GetInstance().UnlockRenderTarget(3, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(2, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	if (bResolveDepth)
	{
		UI3DDRAWMANAGER.GetDepth()->Realize();
	}
	*/
#elif __PPU || __D3D11 || RSG_ORBIS
	grcTextureFactory::GetInstance().UnlockMRT();
#else
	grcTextureFactory::GetInstance().UnlockRenderTarget(3);
	grcTextureFactory::GetInstance().UnlockRenderTarget(2);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::LockSingleTarget(u32 index, bool bLockDepth)
{
	grcTextureFactory::GetInstance().LockRenderTarget(
		0, 
		UI3DDRAWMANAGER.GetTarget(index), 
		bLockDepth ? UI3DDRAWMANAGER.GetDepth() : NULL);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::UnlockSingleTarget(
	bool XENON_ONLY(bResolveColor), 
	bool XENON_ONLY(bClearColor), 
	bool XENON_ONLY(bResolveDepth), 
	bool XENON_ONLY(bClearDepth),
	const Vector4& XENON_ONLY(scissorRect))
{
	grcResolveFlags resolveFlags;

	#if __XENON
		grcRenderTarget* pDst = UI3DDRAWMANAGER.GetLightingTarget();
		
		DWORD Flags = D3DRESOLVE_ALLFRAGMENTS;
		
		if (bResolveColor)
			Flags |= D3DRESOLVE_RENDERTARGET0;

		if (bClearColor)
			Flags |= D3DRESOLVE_CLEARRENDERTARGET;

		if (bResolveDepth)
			Flags |= D3DRESOLVE_DEPTHSTENCIL;

		if (bClearDepth)
			Flags |= D3DRESOLVE_CLEARDEPTHSTENCIL;

		const int adjustedScissorX = (int)(floorf((scissorRect.x) * 0.125f) * 8.0f);
		const int adjustedScissorY = (int)(floorf((scissorRect.y) * 0.125f) * 8.0f);

		const int adjustedScissorW = (int)(ceilf ((scissorRect.z + 1) * 0.125f) * 8.0f);
		const int adjustedScissorH = (int)(ceilf ((scissorRect.w + 1) * 0.125f) * 8.0f);

		D3DRECT srcRect = { adjustedScissorX, adjustedScissorY, adjustedScissorX + adjustedScissorW, adjustedScissorY + adjustedScissorH };
		D3DPOINT dstPoint = { adjustedScissorX, adjustedScissorY };

		GRCDEVICE.GetCurrent()->Resolve( 
			Flags, 
			&srcRect, 
			pDst->GetTexturePtr(), 
			&dstPoint, 
			0, 0, NULL, 0.0f, 0L, NULL);

		resolveFlags.NeedResolve = false;
	#endif // __XENON

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::LockLightingTarget(bool bLockDepth)
{
	grcTextureFactory::GetInstance().LockRenderTarget(
		0, 
		UI3DDRAWMANAGER.GetLightingTarget(), 
		bLockDepth ? UI3DDRAWMANAGER.GetDepthCopy() : NULL);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::SetGbufferTargets()
{
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_0, UI3DDRAWMANAGER.GetTarget(0));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_1, UI3DDRAWMANAGER.GetTarget(1));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_2, UI3DDRAWMANAGER.GetTarget(2));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_3, UI3DDRAWMANAGER.GetTarget(3));

#if __PS3
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, UI3DDRAWMANAGER.GetDepthAlias());
#else
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, UI3DDRAWMANAGER.GetDepth());
#endif

#if __XENON
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_STENCIL, UI3DDRAWMANAGER.GetDepthAlias());
#elif RSG_PC || RSG_DURANGO || RSG_ORBIS
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_STENCIL, UI3DDRAWMANAGER.GetDepth());
#endif
}

#endif

#if __PS3
// This is needed to avoid edge pixel culling
// causing visible cracks in the ped geometry
extern bool SetEdgeNoPixelTestEnabled(bool bEnabled);
static bool bWasEdgeNoPixelTestEnabled = false;
#endif

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DrawBackgroundSprite(float x, float y, float w, float h, Color32 inCol)
{
	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	
	const Color32 col =  inCol;
	Vector4 rect = Vector4(x, y, w, h);
	rect.z += rect.x;
	rect.w += rect.y;

	CSprite2d::DrawRect(rect.x, rect.y, rect.z, rect.w, 0.0f, col);
	
	// reset viewport
	POP_DEFAULT_SCREEN();

	// reset state
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::SetToneMapScalars()
{
	const float packScalar = 4.0f;
	const float toneMapScaler = 1.0f/packScalar;
	const float unToneMapScaler = packScalar;
	Vector4 scalers(toneMapScaler, unToneMapScaler, toneMapScaler, unToneMapScaler);
	CShaderLib::SetGlobalToneMapScalers(scalers);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::RenderToneMapPass(grcRenderTarget* pDstRT, grcTexture* pSrcRT, const Vector4& scissorRect, const Vector4& colFilter0, const Vector4& colFilter1, bool bClearBackground)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_MARKER("ToneMap");
	
	PostFX::SetLDR8bitHDR16bit();

	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, UI3DDRAWMANAGER.GetDepth());

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	if(bClearBackground WIN32PC_ONLY(|| (GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled())))
	{
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, false, false, DEFERRED_MATERIAL_CLEAR);
	}
	

	GRCDEVICE.SetScissor((int)scissorRect.x, (int)scissorRect.y, (int)scissorRect.z, (int)scissorRect.w);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(UI3DDRAWMANAGER.m_DSSTestStencilHandle, DEFERRED_MATERIAL_CLEAR);

	Vector4 filmic0, filmic1, tonemapParams;
	PostFX::GetDefaultFilmicParams(filmic0, filmic1);

	// set custom exposure
	tonemapParams.z = pow(2.0f, UI3DDRAWMANAGER.m_globalRenderData.m_exposure);
	
	// set custom gamma
	tonemapParams.w = (1.0f/2.2f);

	CSprite2d tonemapSprite;
	tonemapSprite.SetGeneralParams(filmic0, filmic1);
	tonemapSprite.SetRefMipBlurParams(Vector4(0.0f, 0.0f, 1.0f, (1.0f / 2.2f)));
	tonemapSprite.SetColorFilterParams(colFilter0, colFilter1);
	tonemapSprite.BeginCustomList(CSprite2d::CS_TONEMAP, pSrcRT);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	tonemapSprite.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// reset state
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::ClearTarget(bool bSkinPass)
{	
	float depth = USE_INVERTED_PROJECTION_ONLY ? grcDepthClosest : grcDepthFarthest;

	if(bSkinPass)
	{
#if __XENON
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE,		TRUE);
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE,	FALSE);
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC,		D3DHSCMP_NOTEQUAL);
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF,			DEFERRED_MATERIAL_PED);
#else
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, depth, false, DEFERRED_MATERIAL_CLEAR);
#endif
	}
	else
	{
#if __XENON
		GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC);
		GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC);
#endif
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), true, depth, true, DEFERRED_MATERIAL_CLEAR);
	}
}


//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::CompositeToBackbuffer(grcRenderTarget* pSrcRT, const Vector4& rect, float alpha)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_MARKER("CompositeToBackbuffer");

	grcRenderTarget* pDstRT = CRenderTargets::GetUIBackBuffer();

	// center point for our full composite target
	const float centerX = rect.x + rect.z*0.5f;
	const float centerY = rect.y + rect.w*0.5f;

#if __BANK
	if (UI3DDRAWMANAGER.m_bDrawDebug)
	{
		grcDebugDraw::Circle(Vector2(centerX, centerY), 0.01f, Color32(0xffff0000));
	}
#endif

	float quadWidth = (float)pSrcRT->GetHeight();
	float quadHeight = (float)pSrcRT->GetWidth();

	float cX = centerX * (float)pDstRT->GetWidth();
	float cY = centerY * (float)pDstRT->GetHeight();

	// position for the quad
	float x0 = (cX - quadWidth  * 0.5f) / (float)pDstRT->GetWidth();
	float y0 = (cY - quadHeight * 0.5f) / (float)pDstRT->GetHeight();
	float x1 = (cX + quadWidth  * 0.5f) / (float)pDstRT->GetWidth();
	float y1 = (cY + quadHeight * 0.5f) / (float)pDstRT->GetHeight();
	
#if __BANK
	if (UI3DDRAWMANAGER.m_bDrawDebug)
	{
		grcDebugDraw::RectAxisAligned(Vector2(x0, y0), Vector2(x1, y1), Color32(0xff00ff00), false);
	}
#endif

	x0 = x0*2.0f - 1.0f;  
	y0 = (1.0f-y0)*2.0f - 1.0f; 
	x1 = x1*2.0f - 1.0f;
	y1 = (1.0f-y1)*2.0f - 1.0f;

	float dstWidth	= (float)pDstRT->GetWidth();
	float dstHeight = (float)pDstRT->GetHeight();

	// scissor rect
	int scissorX = (int)(rect.x*dstWidth);
	int scissorY = (int)(rect.y*dstHeight);
	int scissorW = (int)(rect.z*dstWidth);
	int scissorH = (int)(rect.w*dstHeight);

#if __BANK
	if (UI3DDRAWMANAGER.m_bDrawDebug)
	{
		grcDebugDraw::RectAxisAligned(Vector2(rect.x, rect.y), Vector2(rect.x+rect.z, rect.y+rect.w), Color32(0xff0000ff), false);
	}
#endif

	// lock target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	//grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(XENON_SWITCH(grcStateBlock::RS_Default_HalfPixelOffset, grcStateBlock::RS_NoBackfaceCull));

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		GRCDEVICE.SetStereoScissor(false);
	}
#endif

	BANK_ONLY(if (UI3DDRAWMANAGER.m_bDisableScissor == false))
	{
		GRCDEVICE.SetScissor(scissorX, scissorY, scissorW, scissorH);
	}

	// blit backbuffer back
	CSprite2d copyBackground;
	copyBackground.SetDepthTexture(UI3DDRAWMANAGER.GetDepth());
	copyBackground.BeginCustomList(CSprite2d::CS_BLIT_DEPTH_TO_ALPHA, pSrcRT );
	
	Vector2 uv0(0.0f, 1.0f);
	Vector2 uv1(1.0f, 1.0f);
	Vector2 uv2(0.0f, 0.0f);
	Vector2 uv3(1.0f, 0.0f);

	Vector2 pt0(x0, y1);
	Vector2 pt1(x1, y1);
	Vector2 pt2(x0, y0);
	Vector2 pt3(x1, y0);

	int a = (int)(alpha * 255.0f);

#if RSG_PC
	if((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()))
		DrawUIRect(pt1, pt3, pt0, pt2, 0.0f, uv1, uv3, uv0, uv2, Color32(255,255,255,a));
	else
#endif
		DrawUIRect(pt0, pt1, pt2, pt3, 0.0f, uv1, uv3, uv0, uv2, Color32(255,255,255,a));

	copyBackground.EndCustomList();

	BANK_ONLY(if (UI3DDRAWMANAGER.m_bDisableScissor == false))
	{
		GRCDEVICE.SetScissor(0, 0, pDstRT->GetWidth(), pDstRT->GetHeight());
	}

#if __XENON
	grcRenderTarget* pDst = CRenderTargets::GetOffscreenBuffer1();
	D3DRECT srcRect = { adjustedScissorX, adjustedScissorY, adjustedScissorX + adjustedScissorW, adjustedScissorY + adjustedScissorH };
	D3DPOINT dstPoint = { adjustedScissorX, adjustedScissorY };
	CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, &srcRect, pDst->GetTexturePtr(), &dstPoint, 0, 0, NULL, 0.0f, 0L, NULL ) );
#endif
	
	POP_DEFAULT_SCREEN();

	// unlock target
	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = false;
#endif
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		GRCDEVICE.SetStereoScissor(true);
	}
#endif

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::BlitBackground(grcRenderTarget* pDstRT, const grcRenderTarget* pSrcRT, const Vector4& rect, const Vector4& scissorRect)
{
	PF_PUSH_MARKER("BlitBackground");

	// center point for our full composite target
	const float centerX = rect.x + rect.z*0.5f;
	const float centerY = rect.y + rect.w*0.5f;

	float quadWidth = (float)pDstRT->GetHeight();
	float quadHeight = (float)pDstRT->GetWidth();

	float fNativeWidth = (float)VideoResManager::GetUIWidth();
	float fNativeHeight = (float)VideoResManager::GetUIHeight();

	float cX = centerX * fNativeWidth;
	float cY = centerY * fNativeHeight;

	// position for the quad
	float x0 = (cX - quadWidth  * 0.5f) / fNativeWidth;
	float y0 = (cY - quadHeight * 0.5f) / fNativeHeight;
	float x1 = (cX + quadWidth  * 0.5f) / fNativeWidth;
	float y1 = (cY + quadHeight * 0.5f) / fNativeHeight;

	// lock composite target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(XENON_SWITCH(grcStateBlock::RS_Default_HalfPixelOffset, grcStateBlock::RS_NoBackfaceCull));

	const float targetWidth = (float)pDstRT->GetWidth();
	const float targetHeight = (float)pDstRT->GetHeight();
	Vector4 adjustedScissorRect  = scissorRect + Vector4(-1.0f, -1.0f, 2.0f, 2.0f);
	adjustedScissorRect.Max(Vector4(0.0f, 0.0f, 0.0f, 0.0f), adjustedScissorRect);
	adjustedScissorRect.Min(Vector4(targetWidth, targetHeight, targetWidth, targetHeight), adjustedScissorRect);
	GRCDEVICE.SetScissor((int)adjustedScissorRect.x, (int)adjustedScissorRect.y, (int)adjustedScissorRect.z, (int)adjustedScissorRect.w);

	// blit backbuffer back
	CSprite2d copyBackground;
	copyBackground.BeginCustomList(CSprite2d::CS_BLIT_NOFILTERING, pSrcRT);

	Vector2 pt0(-1.0f, -1.0f);
	Vector2 pt1(1.0f, -1.0f);
	Vector2 pt2(-1.0f, 1.0f);
	Vector2 pt3(1.0f, 1.0f);

	Vector2 uv0(x0, y1);
	Vector2 uv1(x1, y1);
	Vector2 uv2(x0, y0);
	Vector2 uv3(x1, y0);

	DrawUIRect(pt0, pt1, pt2, pt3, 0.0f, uv2, uv0, uv3, uv1, Color32(255,255,255,255));

	copyBackground.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// unlock composite target
	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = false;
#endif
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::CompositeLuminanceToAlpha(grcRenderTarget* pDstRT, grcRenderTarget* pSrcRT, const Vector4& scissorRect)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_MARKER("CompositeLuminanceToAlpha");

	// lock composite target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(XENON_SWITCH(grcStateBlock::RS_Default_HalfPixelOffset, grcStateBlock::RS_NoBackfaceCull));

	const float targetWidth = (float)pDstRT->GetWidth();
	const float targetHeight = (float)pDstRT->GetHeight();
	Vector4 adjustedScissorRect  = scissorRect + Vector4(-1.0f, -1.0f, 2.0f, 2.0f);
	adjustedScissorRect.Max(Vector4(0.0f, 0.0f, 0.0f, 0.0f), adjustedScissorRect);
	adjustedScissorRect.Min(Vector4(targetWidth, targetHeight, targetWidth, targetHeight), adjustedScissorRect);
	GRCDEVICE.SetScissor((int)adjustedScissorRect.x, (int)adjustedScissorRect.y, (int)adjustedScissorRect.z, (int)adjustedScissorRect.w);

	// blit backbuffer back
	CSprite2d copyBackground;
	copyBackground.BeginCustomList(CSprite2d::CS_LUMINANCE_TO_ALPHA, pSrcRT);
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0, 0, 1, 1, Color32(255,255,255,255));
	copyBackground.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// unlock composite target
	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = true;
#endif
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DrawBackground(grcRenderTarget* pDstRT, Color32 col, const Vector4& scissorRect)
{
	PF_PUSH_MARKER("BackgroundSprite");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// lock composite target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(XENON_SWITCH(grcStateBlock::RS_Default_HalfPixelOffset, grcStateBlock::RS_NoBackfaceCull));

	GRCDEVICE.SetScissor((int)scissorRect.x, (int)scissorRect.y, (int)scissorRect.z, (int)scissorRect.w);

	CSprite2d backgroundSprite;
	backgroundSprite.SetGeneralParams(VEC4V_TO_VECTOR4(col.GetRGBA()), Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	backgroundSprite.BeginCustomList(CSprite2d::CS_BLIT_COLOUR, NULL);
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, col);
	backgroundSprite.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// reset state
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

	// unlock composite target
	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = true;
#endif
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::SetAmbients()
{
	// lerp the values in the default light rig based on time of day (0 is day light right, 1 is night)
	const float time = 1.0f - Abs<float>((2.0f*CClock::GetDayRatio()) - 1.0f);

	Vector4 ambientDownDay = VEC4V_TO_VECTOR4(UI3DDRAWMANAGER.m_lightRigs[0].m_ambientDownCol.GetRGBA());
	Vector4 ambientBaseDay = VEC4V_TO_VECTOR4(UI3DDRAWMANAGER.m_lightRigs[0].m_ambientBaseCol.GetRGBA());
	Vector4 ambientDownNight = VEC4V_TO_VECTOR4(UI3DDRAWMANAGER.m_lightRigs[1].m_ambientDownCol.GetRGBA());
	Vector4 ambientBaseNight = VEC4V_TO_VECTOR4(UI3DDRAWMANAGER.m_lightRigs[1].m_ambientBaseCol.GetRGBA());

	Vector3 ambientMiscDay = Vector3(UI3DDRAWMANAGER.m_lightRigs[0].m_ambientDownMult, 
									UI3DDRAWMANAGER.m_lightRigs[0].m_ambientDownWrap,
									UI3DDRAWMANAGER.m_lightRigs[0].m_ambientBaseMult);

	Vector3 ambientMiscNight= Vector3(UI3DDRAWMANAGER.m_lightRigs[1].m_ambientDownMult, 
									UI3DDRAWMANAGER.m_lightRigs[1].m_ambientDownWrap,
									UI3DDRAWMANAGER.m_lightRigs[1].m_ambientBaseMult);


	Vector4 ambientDownLerped = Lerp(time, ambientDownNight, ambientDownDay);
	Vector4 ambientBaseLerped = Lerp(time, ambientBaseNight, ambientBaseDay);
	Vector3 ambientMiscLerped = Lerp(time, ambientMiscNight, ambientMiscDay);

	Vector4 ambientDown = ambientDownLerped;
	ambientDown *= ambientMiscLerped.x;
	ambientDown.w = ambientMiscLerped.y;

	Vector4 ambientBase = ambientBaseLerped;
	ambientBase *= ambientMiscLerped.z;
	ambientBase.w = 1.0f / (1.0f + ambientMiscLerped.y);

	Vector4 ambientValues[7];
	Vector4 zero = Vector4(Vector4::ZeroType);

	// set up ambient lighting
	ambientValues[0] = ambientDown;
	ambientValues[1] = ambientBase;

	// artificial interior ambient
	ambientValues[2] = zero;
	ambientValues[3] = zero;

	// artificial exterior ambient
	ambientValues[4] = zero;
	ambientValues[5] = zero;

	// directional ambient
	ambientValues[6] = zero;

	Lights::m_lightingGroup.SetAmbientLightingGlobals(1.0f, 1.0f, 1.0f, ambientValues);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::GenerateLights(CLightSource* pLights, const Vector3& objectPos, float globalLightIntensity)
{
	// lerp the values in the default light rig based on time of day (0 is day light right, 1 is night)
	const float time = 1.0f - Abs<float>((2.0f*CClock::GetDayRatio()) - 1.0f);
	m_currentRigIdx = 0;

	for (int i = 0; i < 4; i++)
	{
		const Color32 curColorDay	= m_lightRigs[0].m_lights[i].m_color;
		const Color32 curColorNight = m_lightRigs[1].m_lights[i].m_color;
		const Vector3 colorDay		= Vector3(curColorDay.GetRedf(), curColorDay.GetGreenf(), curColorDay.GetBluef());
		const Vector3 colorNight	= Vector3(curColorNight.GetRedf(), curColorNight.GetGreenf(), curColorNight.GetBluef());
		const Vector3 posDay		= m_lightRigs[0].m_lights[i].m_position;
		const Vector3 posNight		= m_lightRigs[1].m_lights[i].m_position;
		const float radiusDay		= m_lightRigs[0].m_lights[i].m_radius;
		const float radiusNight		= m_lightRigs[1].m_lights[i].m_radius;
		const float intensityDay	= m_lightRigs[0].m_lights[i].m_intensity;
		const float intensityNight	= m_lightRigs[1].m_lights[i].m_intensity;
		const float falloffDay		= m_lightRigs[0].m_lights[i].m_falloff;
		const float falloffNight	= m_lightRigs[1].m_lights[i].m_falloff;
		
		pLights[i].SetCommon(
			LIGHT_TYPE_POINT, 
			LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR, 
			Lerp(time, posNight, posDay),
			Lerp(time, colorNight, colorDay),
			Lerp(time, intensityNight, intensityDay),
			LIGHT_ALWAYS_ON);
		pLights[i].SetRadius(Lerp(time, radiusNight , radiusDay));
		pLights[i].SetFalloffExponent(Lerp(time, falloffNight, falloffDay));
		
		const UILightRig& curLightRig = m_lightRigs[m_currentRigIdx];

		// get the local light position from the current light rig 
		Vector3 localPos = curLightRig.m_lights[i].m_position;

		// translate it to make it relative to the ped
		pLights[i].SetPosition(localPos+objectPos);

		// override light intensity
		const float intensity = pLights[i].GetIntensity();
		pLights[i].SetIntensity(intensity*globalLightIntensity);
	}
}

void UI3DDrawManager::SetLightsForObject(const Vector3& objectPos, float globalLightIntensity)
{
	CLightSource lights[4];
	UI3DDRAWMANAGER.GenerateLights(&lights[0], objectPos, globalLightIntensity);

	Lights::m_lightingGroup.SetForwardLights(lights,4 );
	Lights::SelectLightweightTechniques(4, true);
}

//////////////////////////////////////////////////////////////////////////
//
bool UI3DDrawManager::HasPedsToDraw() const
{
	if (m_pCurrentPreset != NULL)
	{
		for (int i = 0; i < m_pCurrentPreset->m_elements.GetCount(); i++)
		{
			const UISceneObjectData* pObject = &(m_pCurrentPreset->m_elements[i]);	
			if (pObject->m_enabled && m_scenePresetPatchedData[i].m_pPed != NULL)
				return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::AddPresetElementsToDrawList(
	const UIScenePreset* pPreset, 
	const u32 slotIndex,
	u32 renderBucket,
	int renderMode)
{
	const UISceneObjectData* pObject = &(pPreset->m_elements[slotIndex]);
	Vector3 offset = m_scenePresetPatchedData[slotIndex].m_offset;
	Vector3 finalRotationDeg = pObject->m_rotationXYZ+m_scenePresetPatchedData[slotIndex].m_rotationXYZ;

	// update the ped damage variables
	m_scenePresetPatchedData[slotIndex].m_pPed->UpdateDamageCustomShaderVars();

	// update world matrix
	finalRotationDeg.x = DEG2RAD(finalRotationDeg.x);
	finalRotationDeg.y = DEG2RAD(finalRotationDeg.y);
	finalRotationDeg.z = DEG2RAD(finalRotationDeg.z);

	Vector3 position = 	(CHudTools::GetWideScreen() ? pObject->m_position : pObject->m_position_43);

	Vec3V vTranslation	= VECTOR3_TO_VEC3V(position+offset);
	Vec3V vRotationRads = VECTOR3_TO_VEC3V(finalRotationDeg);

	Mat34V vWorldMat(V_IDENTITY);
	Mat34VFromEulersYXZ(vWorldMat, vRotationRads, vTranslation);

	DLC (dlCmdOverrideSkeleton, (vWorldMat));

	DLC (dlCmdSetBucketsAndRenderMode, (renderBucket, renderMode));
	m_scenePresetPatchedData[slotIndex].m_pPed->GetDrawHandler().AddToDrawList(m_scenePresetPatchedData[slotIndex].m_pPed, NULL);

	if (renderBucket == CRenderer::RB_ALPHA)
	{
		CDrawListPrototypeManager::Flush();
	}
}

//#if DEFERRED_RENDER_UI

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::SetUIProjectionParams(eDeferredShaders currentShader, const grcRenderTarget *pDstRT)
{
	const float fWidth = (float)pDstRT->GetWidth();
	const float fHeight = (float)pDstRT->GetHeight();

	const Vector4 screenSize(fWidth, fHeight, 1.0f / fWidth, 1.0f / fHeight);

	Vec4V projParams;
	Vec3V shearProj0, shearProj1, shearProj2;
	DeferredLighting::GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_SCREENSIZE, screenSize, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PROJECTION_PARAMS, projParams, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS0, shearProj0, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS1, shearProj1, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS2, shearProj2, MM_SINGLE);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::SetUILightingGlobals()
{
	Vec4V bakeParams(UI3DDRAWMANAGER.m_globalRenderData.m_dynamicBakeBoostMin,
		UI3DDRAWMANAGER.m_globalRenderData.m_dynamicBakeBoostMax - UI3DDRAWMANAGER.m_globalRenderData.m_dynamicBakeBoostMin,
		0.0f, 0.0f );

	UI3DDRAWMANAGER.m_prevInInterior = CShaderLib::GetGlobalInInterior();
	
	// Set all lighting values to default
	CShaderLib::SetGlobalDynamicBakeBoostAndWetness(bakeParams);
	CShaderLib::SetGlobalEmissiveScale(1.0f,true);
	CShaderLib::DisableGlobalFogAndDepthFx();
	CShaderLib::SetGlobalNaturalAmbientScale(1.0f);
	CShaderLib::SetGlobalArtificialAmbientScale(0.0f);
	CShaderLib::SetGlobalInInterior(false);
	CShaderLib::SetGlobalScreenSize(
		Vector2((float)UI3DDRAWMANAGER.GetLightingTarget()->GetWidth(), 
				(float)UI3DDRAWMANAGER.GetLightingTarget()->GetHeight()));

	Lights::m_lightingGroup.SetDirectionalLightingGlobals(1.0f, false);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::RestoreLightingGlobals()
{
	CShaderLib::EnableGlobalFogAndDepthFx();
	CShaderLib::SetGlobalInInterior(UI3DDRAWMANAGER.m_prevInInterior);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::Initialize()
{
	PF_PUSH_MARKER("Initalize");

	UI3DDRAWMANAGER.m_pLastViewport = grcViewport::GetCurrent();

	CRenderTargets::UnlockUIRenderTargets();

	//const float aspectRatio = (CHudTools::GetWideScreen()) ? CHudTools::GetAspectRatio() : (4.0f / 3.0f) * (16.0f / 9.0f);

	Mat34V vGameCamMtx;
	Mat34VFromEulersYXZ(vGameCamMtx, Scale(UI3DDRAWMANAGER.m_vCamAnglesDeg, ScalarV(V_TO_RADIANS)), UI3DDRAWMANAGER.m_vCamPos);
	Mat34V vRageCamMtx = GameCameraMtxToRageCameraMtx(vGameCamMtx);
#if USE_INVERTED_PROJECTION_ONLY
	UI3DDRAWMANAGER.m_Viewport.SetInvertZInProjectionMatrix(true,false);
#endif // USE_INVERTED_PROJECTION_ONLY
	UI3DDRAWMANAGER.m_Viewport.Perspective(UI3DDRAWMANAGER.m_CamFieldOfView, 0.0f, 0.05f, 1000.0f);
	UI3DDRAWMANAGER.m_Viewport.SetCameraMtx(vRageCamMtx);

	SetUILightingGlobals();
	SetToneMapScalars();

#if __XENON
	PF_PUSH_MARKER("Backbuffer Resolve");
	grcRenderTarget* pBackbuffer = CRenderTargets::GetUIBackBuffer(false);

	grcTextureFactory::GetInstance().LockRenderTarget(0, pBackbuffer, NULL);

	grcRenderTarget* pDst = CRenderTargets::GetOffscreenBuffer1();
	D3DRECT srcRect = { 0, 0, pBackbuffer->GetWidth(), pBackbuffer->GetHeight()};
	D3DPOINT dstPoint = { 0, 0 };
	CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, &srcRect, pDst->GetTexturePtr(), &dstPoint, 0, 0, NULL, 0.0f, 0L, NULL ) );

	grcResolveFlags flags;
	flags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);
	PF_POP_MARKER();
#endif

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::PerSlotSetup(float shearX, float shearY)
{
	UI3DDRAWMANAGER.m_Viewport.SetPerspectiveShear(shearY, shearX);
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::Finalize()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_MARKER("Finalize");

	RestoreLightingGlobals();

	CRenderTargets::LockUIRenderTargets();
	grcViewport::SetCurrent(UI3DDRAWMANAGER.m_pLastViewport);
	Lights::ResetLightingGlobals();
	Lights::m_lightingGroup.SetDirectionalLightingGlobals(true);

	GRCDEVICE.SetScissor(0, 0, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight());

#if __XENON
	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetUIBackBuffer(false), NULL);

	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(XENON_SWITCH(grcStateBlock::RS_Default_HalfPixelOffset, grcStateBlock::RS_NoBackfaceCull));

	// restore backbuffer (we've been trashing edram with all the previous passes)
	CSprite2d restoreBackbuffer;
	restoreBackbuffer.BeginCustomList(CSprite2d::CS_BLIT_NOFILTERING, CRenderTargets::GetOffscreenBuffer1());
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(255,255,255,255));
	restoreBackbuffer.EndCustomList();

	grcResolveFlags resFlags;
	resFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resFlags);
#endif
	
	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DeferredRenderStart()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_MARKER("UI Deferred G-Buffer");
	
	LockRenderTargets(true);

	grcViewport::GetCurrent()->SetWorldIdentity();

	GBuffer::SetRenderingTo(true);
	CRenderPhaseReflection::SetReflectionMapExt(grcTexture::NoneBlack);
	
	SetToneMapScalars();

	#if __PS3
		CRenderTargets::ResetStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xff, false, false);
	#endif

	#if __XENON
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE));
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE));
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, 0 ));
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL ));

		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC));
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC));

		CHECK_HRESULT(GRCDEVICE.GetCurrent()->FlushHiZStencil (D3DFHZS_ASYNCHRONOUS));
	#endif //__XENON

	float depth = USE_INVERTED_PROJECTION_ONLY ? grcDepthClosest : grcDepthFarthest;
	GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), true, depth, true, DEFERRED_MATERIAL_CLEAR);

	grcStateBlock::SetDepthStencilState(UI3DDRAWMANAGER.m_DSSWriteStencilHandle, DEFERRED_MATERIAL_PED);

	sm_prevForcedTechniqueGroup = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(grmShaderFx::FindTechniqueGroupId("deferred"));
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DeferredRenderEnd(const Vector4& scissorRect)
{
	grmShaderFx::SetForcedTechniqueGroupId(sm_prevForcedTechniqueGroup);

	#if __XENON
		CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, FALSE));
	#endif
	UnlockRenderTargets(true, true, scissorRect);
	GBuffer::SetRenderingTo(false);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::ForwardRenderStart()
{
	grcLightState::SetEnabled(true);
	CRenderPhaseReflection::SetReflectionMapExt(grcTexture::NoneBlack);

	SetToneMapScalars();

	grcStateBlock::SetDepthStencilState(UI3DDRAWMANAGER.m_DSSWriteStencilHandle, DEFERRED_MATERIAL_PED);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

	Lights::BeginLightweightTechniques();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::ForwardRenderEnd()
{
	CShaderLib::ResetStippleAlpha();
	CShaderLib::SetGlobalAlpha(1.0f);

	Lights::EndLightweightTechniques();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::DeferredLight(const Vector4 &scissorRect, const Vector3 &objPos, Color32 clearCol, float lightGlobalIntensity)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_MARKER("UI Deferred Lighting");
	
	const grcViewport* vp = &UI3DDRAWMANAGER.m_Viewport;

	const float nearPlaneBoundIncrease = 
		vp->GetNearClip() * 
		sqrtf(1.0f + vp->GetTanHFOV() * vp->GetTanHFOV() + vp->GetTanVFOV() * vp->GetTanVFOV());
	Vector3 camPos = VEC3V_TO_VECTOR3(vp->GetCameraPosition());
	
#if __XENON
	GBuffer::RestoreDepthOptimized(UI3DDRAWMANAGER.GetDepthAlias(), false);
#endif

#if RSG_PC
	if(!GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		CRenderTargets::CopyDepthBuffer(UI3DDRAWMANAGER.GetDepth(), UI3DDRAWMANAGER.GetDepthCopy());
	}
#endif

	grcTextureFactory::GetInstance().LockRenderTarget(0, UI3DDRAWMANAGER.GetLightingTarget(), UI3DDRAWMANAGER.GetDepthCopy());

	grcViewport::GetCurrent()->SetWorldIdentity();

	GRCDEVICE.SetScissor((int)(scissorRect.x), (int)(scissorRect.y), (int)(scissorRect.z), (int)(scissorRect.w));
#if __PS3	// we want to set the scull function and restore the depth compare direction a t the same time to avoid invalidation
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default, DEFERRED_MATERIAL_ALL);
	CRenderTargets::RefreshStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xFF, false);
#endif

#if __XENON
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, TRUE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILREF, 0xFF));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->FlushHiZStencil(D3DFHZS_ASYNCHRONOUS));
#endif

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	GRCDEVICE.Clear(true, clearCol, false, grcDepthFarthest, false, DEFERRED_MATERIAL_CLEAR);

	SetGbufferTargets();

	SetUIProjectionParams(DEFERRED_SHADER_DIRECTIONAL, UI3DDRAWMANAGER.GetLightingTarget());
	SetToneMapScalars();

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(UI3DDRAWMANAGER.m_DSSTestStencilHandle, DEFERRED_MATERIAL_CLEAR);

	// Draw light
	DeferredLighting::RenderDirectionalLight(1, false, MM_SINGLE);


	CLightSource lights[4];
	UI3DDRAWMANAGER.GenerateLights(&lights[0], objPos, lightGlobalIntensity);

	Lights::BeginLightRender();
	for (u32 i = 0; i < 4; i++)
	{
		CLightSource &lightSource = lights[i];
		
		eDeferredShaders currentShader = DeferredLighting::GetShaderType(lightSource.GetType());
		DeferredLighting::SetLightShaderParams(currentShader, &lightSource, NULL, 1.0f, false, false, MM_SINGLE);
		SetUIProjectionParams(currentShader, UI3DDRAWMANAGER.GetLightingTarget());

		const grcEffectTechnique technique = DeferredLighting::GetTechnique(lightSource.GetType(), MM_SINGLE);
		const s32 pass = DeferredLighting::CalculatePass(&lightSource, false, false, false);
		bool cameraInside = lightSource.IsPointInLight(camPos, nearPlaneBoundIncrease);	

		grcStateBlock::SetDepthStencilState(cameraInside ? UI3DDRAWMANAGER.m_insideLightVolumeDSS : UI3DDRAWMANAGER.m_outsideLightVolumeDSS, DEFERRED_MATERIAL_CLEAR);
		grcStateBlock::SetRasterizerState(Lights::m_Volume_R[Lights::kLightTwoPassStencil][cameraInside ? Lights::kLightCameraInside : Lights::kLightCameraOutside][Lights::kLightNoDepthBias]);
		grcStateBlock::SetBlendState(Lights::m_SceneLights_B);

		const grmShader* shader = DeferredLighting::GetShader(currentShader, MM_SINGLE);
		if (ShaderUtil::StartShader("light", shader, technique, pass, false))
		{
			Lights::RenderVolumeLight(lightSource.GetType(), false, false, false);
			ShaderUtil::EndShader(shader, false);
		}
	}
	Lights::EndLightRender();

	grcResolveFlags resolveFlags;

#if __XENON
	const int adjustedScissorX = (int)(floorf((scissorRect.x) * 0.125f) * 8.0f);
	const int adjustedScissorY = (int)(floorf((scissorRect.y) * 0.125f) * 8.0f);

	const int adjustedScissorW = (int)(ceilf ((scissorRect.z + 1) * 0.125f) * 8.0f);
	const int adjustedScissorH = (int)(ceilf ((scissorRect.w + 1) * 0.125f) * 8.0f);

	D3DRECT srcRect = { adjustedScissorX, adjustedScissorY, adjustedScissorX + adjustedScissorW, adjustedScissorY + adjustedScissorH };
	D3DPOINT dstPoint = { adjustedScissorX, adjustedScissorY };

	GRCDEVICE.GetCurrent()->Resolve( 
		D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, 
		&srcRect, 
		UI3DDRAWMANAGER.GetLightingTarget()->GetTexturePtr(), 
		&dstPoint, 
		0, 0, NULL, 0.0f, 0L, NULL);

	resolveFlags.NeedResolve = false;
#endif

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

	PF_POP_MARKER();

	#if __PPU
		grcFenceHandle fence = GRCDEVICE.InsertFence();
		GRCDEVICE.GPUBlockOnFence(fence);
	#endif // __PPU
}
//#endif

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::GetPostFXDataForObject(const UISceneObjectData* pObject, Vector4& colFilter0, Vector4& colFilter1)
{
	colFilter0.x = pObject->m_postfxData.m_blackWhiteWeights.GetRedf();
	colFilter0.y = pObject->m_postfxData.m_blackWhiteWeights.GetGreenf();
	colFilter0.z = pObject->m_postfxData.m_blackWhiteWeights.GetBluef();
	colFilter0.w = pObject->m_postfxData.m_blend;
	colFilter1.x = pObject->m_postfxData.m_tintColor.GetRedf();
	colFilter1.y = pObject->m_postfxData.m_tintColor.GetGreenf();
	colFilter1.z = pObject->m_postfxData.m_tintColor.GetBluef();
	colFilter1.w = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::AddToDrawList()
{

#if __BANK
	m_editTool.Update();
#endif
	const UIScenePreset* pPreset = m_pCurrentPreset;

	if (pPreset != NULL)
	{
		DLCPushMarker("Ped UI Rendering");
		DLC_Add(Initialize);

		#if __PS3
			bWasEdgeNoPixelTestEnabled = SetEdgeNoPixelTestEnabled(false);
		#endif

		DLC_Add(SetAmbients);

		for (int slotIdx = 0; slotIdx < pPreset->m_elements.GetCount(); slotIdx++)
		{
			// Skip empty slots
			const UISceneObjectData* pObject = &(pPreset->m_elements[slotIdx]);
			if (pObject->m_enabled == false || m_scenePresetPatchedData[slotIdx].m_pPed == NULL)
			{
				DLC_Add(PerSlotSetup,0.0f, 0.0f);
				continue;
			}

#if RSG_PC
			// compensate for fixed field-of-view HUD slots
			Vector4 xywh = pObject->m_bgRectXYWH;
			float fixedAspect = 16.0f/9.0f;
			float aspect = CHudTools::GetAspectRatio(true);
			float scale = fixedAspect/aspect;
			xywh.x *= scale;
			xywh.z *= scale;
			xywh.x -= 0.5f*(scale-1.0f);
			const Vector4 rect = xywh;
#else
			const Vector4 rect = GRCDEVICE.GetWideScreen() ? pObject->m_bgRectXYWH : pObject->m_bgRectXYWH_43;;
#endif

		 	Vector3 objPos = CHudTools::GetWideScreen() ? pPreset->m_elements[slotIdx].m_position : pPreset->m_elements[slotIdx].m_position_43;
			Color32 clearColor = pObject->m_blendColor;

			// Cache postfx filter data
			Vector4 colFilter0, colFilter1;
			GetPostFXDataForObject(pObject, colFilter0, colFilter1);

		#if __BANK
			if (m_editTool.IsPostFXDataOverrideEnabled())
				m_editTool.GetPostFXDataOverride(colFilter0, colFilter1);
		#endif // __BANK

			DLCPushMarker("Slot");

			DLC_Add(PerSlotSetup,pObject->m_perspectiveShear.x, pObject->m_perspectiveShear.y);

			float fNativeWidth = WIN32PC_ONLY((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? (float)VideoResManager::GetUIHeight() :) (float)VideoResManager::GetUIWidth();
			float fNativeHeight = WIN32PC_ONLY((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? (float)VideoResManager::GetUIWidth() :) (float)VideoResManager::GetUIHeight();			

			float fRenderWidth = WIN32PC_ONLY((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? (float)UI3DDRAWMANAGER.GetLightingTarget()->GetHeight() :) (float)UI3DDRAWMANAGER.GetLightingTarget()->GetWidth();
			float fRenderHeight = WIN32PC_ONLY((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? (float)UI3DDRAWMANAGER.GetLightingTarget()->GetWidth() :) (float)UI3DDRAWMANAGER.GetLightingTarget()->GetHeight();

			Vector4 nativeWH = Vector4(fNativeWidth, fNativeHeight, fNativeWidth, fNativeHeight);
			Vector4 renderWH = Vector4(fRenderWidth, fRenderHeight, fRenderWidth, fRenderHeight);
			Vector4 renderWHdiv2 = Vector4(fRenderWidth * 0.5f, fRenderHeight * 0.5f, 0.0f, 0.0f);

			Vector4 scissorRect;
			scissorRect.Permute<VEC_PERM_Z, VEC_PERM_W, VEC_PERM_Z, VEC_PERM_W>(rect);
			scissorRect = scissorRect * Vector4(-0.5f, -0.5f, 1.0f, 1.0f);
			scissorRect = scissorRect * nativeWH + renderWHdiv2;
			
			scissorRect = Vector4(floorf(scissorRect.x), floorf(scissorRect.y), ceilf(scissorRect.z + 1), ceilf(scissorRect.w + 1));
						
			Vector4 originalRectWidthHeight; 
			originalRectWidthHeight.Permute<VEC_PERM_Z, VEC_PERM_W, VEC_PERM_Z, VEC_PERM_W>(scissorRect);
			Vector4 originalRectCentre = scissorRect + originalRectWidthHeight * 0.5;
			Vector4 rectWidthHeight; 
			rectWidthHeight.Permute<VEC_PERM_W, VEC_PERM_Z, VEC_PERM_W, VEC_PERM_Z>(scissorRect);
			Vector4 newScissorRect = originalRectCentre - rectWidthHeight * 0.5;
			newScissorRect.z = rectWidthHeight.z;
			newScissorRect.w = rectWidthHeight.w;

			newScissorRect.Max(Vector4(0.0f, 0.0f, 0.0f, 0.0f), newScissorRect);
			newScissorRect.Min(renderWH, newScissorRect);

			scissorRect = newScissorRect;
#if RSG_PC
			if((GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()))
			{		
				Vector4 tempRect = scissorRect;
				scissorRect.x = tempRect.y;
				scissorRect.y = tempRect.x;			
				scissorRect.z = tempRect.w;
				scissorRect.w = tempRect.z;
			}

			// adjust viewport for deferred render target
			grcViewport vp = UI3DDRAWMANAGER.m_Viewport;
			float fovY = UI3DDRAWMANAGER.m_CamFieldOfView;
			vp.Perspective(fovY, (GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled()) ? 0.0f : fRenderWidth/fRenderHeight,vp.GetNearClip(),vp.GetFarClip());
			DLC(dlCmdSetCurrentViewport, (&vp));
#else
			DLC(dlCmdSetCurrentViewport, (&UI3DDRAWMANAGER.m_Viewport));
#endif

			// Deferred
			DLC_Add(UI3DDrawManager::DeferredRenderStart);
			{
				DLC(dlCmdGrcDeviceSetScissor, ((int)(scissorRect.x), (int)(scissorRect.y), (int)(scissorRect.z), (int)(scissorRect.w)));

				DLC_Add(DeferredLighting::PrepareDefaultPass, (RPASS_VISIBLE));
				AddPresetElementsToDrawList(pPreset, slotIdx, CRenderer::RB_OPAQUE, rmStandard);
				DLC_Add(DeferredLighting::FinishDefaultPass);

				#if __PS3
				if (PostFX::UseSubSampledAlpha())
				{
					DLC_Add(UnlockRenderTargets, false, false, scissorRect);
					DLC_Add(LockSingleTarget, 0, true);
				}
				#endif // __PS3

				DLC_Add(DeferredLighting::PrepareCutoutPass, false, false, false);
				AddPresetElementsToDrawList(pPreset, slotIdx, CRenderer::RB_CUTOUT, rmStandard);
				DLC_Add(DeferredLighting::FinishCutoutPass);

				#if __PS3
				if (PostFX::UseSubSampledAlpha())
				{
					DLC_Add(UnlockSingleTarget, false, false, false, false, scissorRect);
					DLC_Add(LockRenderTargets, true);
				}
				#endif // __PS3

				DLC_Add(DeferredLighting::PrepareCutoutPass, true, false, false);
				AddPresetElementsToDrawList(pPreset, slotIdx, CRenderer::RB_CUTOUT, rmStandard);
				DLC_Add(DeferredLighting::FinishCutoutPass);

				DLC_Add(DeferredLighting::PrepareDecalPass, false);
				AddPresetElementsToDrawList(pPreset, slotIdx, CRenderer::RB_DECAL, rmStandard);
				DLC_Add(DeferredLighting::FinishDecalPass);
			}
			DLC_Add(UI3DDrawManager::DeferredRenderEnd, scissorRect);
		
			// Lighting
			DLC_Add(UI3DDrawManager::DeferredLight, scissorRect, objPos, clearColor, m_scenePresetPatchedData[slotIdx].m_globalLightIntensity);

			// Skin pass

#if RSG_PC
			DLC_Add(CRenderTargets::CopyTarget, UI3DDRAWMANAGER.GetLightingTarget(), UI3DDRAWMANAGER.GetLightingTargetCopy(), (rage::grcRenderTarget *)NULL, (u8)0xFF);
#endif // RSG_PC

			DLC_Add(LockLightingTarget, true);
			DLC(dlCmdGrcDeviceSetScissor, ((int)(scissorRect.x), (int)(scissorRect.y), (int)(scissorRect.z), (int)(scissorRect.w)));
			
			BANK_ONLY(if(m_bRunSkinPass))
			{
				DLCPushMarker("Skin Lighting");
				DLC_Add(SetUIProjectionParams, DEFERRED_SHADER_LIGHTING, UI3DDRAWMANAGER.GetLightingTarget());
				DLC_Add(SetGbufferTargets);
				DLC_Add(DeferredLighting::ExecuteUISkinLightPass, UI3DDRAWMANAGER.GetLightingTarget(), UI3DDRAWMANAGER.GetLightingTargetCopy());
				DLCPopMarker();
			}

			// Forward 
			BANK_ONLY(if(m_bRunForwardPass))
			{
				DLCPushMarker("UI Forward Render");
				DLC_Add(UI3DDrawManager::ForwardRenderStart);
				{

					DLC_Add(SetLightsForObject, objPos, m_scenePresetPatchedData[slotIdx].m_globalLightIntensity);
					AddPresetElementsToDrawList(pPreset, slotIdx, CRenderer::RB_ALPHA, rmStandard);
				}
				DLC_Add(UI3DDrawManager::ForwardRenderEnd);
				DLCPopMarker();
			}

			DLC_Add(UnlockSingleTarget, true, false, false, false, scissorRect);
			
			DLC (dlCmdOverrideSkeleton, (true));

			BANK_ONLY(if(m_bRunCompositePasses))
			{
				DLCPushMarker("Composite");			
				/*
				#if __XENON
					DLC_Add(BlitBackground, UI3DDRAWMANAGER.GetCompositeTarget0(), CRenderTargets::GetOffscreenBuffer1(), rect, scissorRect);
				#elif RSG_ORBIS  && 1// consider DEVICE_MSAA
					DLC_Add(BlitBackground, UI3DDRAWMANAGER.GetCompositeTarget0(), CRenderTargets::GetUIFrontBuffer(), rect, scissorRect);
				#else
					DLC_Add(BlitBackground, UI3DDRAWMANAGER.GetCompositeTarget0(), CRenderTargets::GetUIBackBuffer(), rect, scissorRect);
				#endif
				*/

				// Background rect
				Color32 bgCol = pObject->m_bgRectColor;
				const bool bDrawBackground = (rect.z > 0.0f && rect.w > 0.0f && bgCol.GetColor() != 0U);
				if (bDrawBackground)
				{
#if RSG_DURANGO || RSG_ORBIS || RSG_PC
					grcRenderTarget* pTarget = CRenderTargets::GetUIBackBuffer();
					int dstWidth = pTarget->GetWidth();
					int dstHeight = pTarget->GetHeight();
					Vector4 scissor( rect.x*dstWidth, rect.y*dstHeight, rect.z*dstWidth, rect.w*dstHeight );
					DLC_Add(DrawBackground, pTarget, bgCol, scissor);
#else
					DLC_Add(DrawBackground, UI3DDRAWMANAGER.GetCompositeTarget0(), bgCol, scissorRect);
#endif
				}

				// Tonemap
				DLC_Add(RenderToneMapPass, UI3DDRAWMANAGER.GetCompositeTarget0(), UI3DDRAWMANAGER.GetLightingTarget(), scissorRect, colFilter0, colFilter1, bDrawBackground);
		
#if RSG_PC
				DLC_Add(CRenderTargets::CopyTarget, UI3DDRAWMANAGER.GetCompositeTarget0(), UI3DDRAWMANAGER.GetCompositeTargetCopy(), (rage::grcRenderTarget *)NULL, (u8)0xFF);
#endif // RSG_PC

				// SSA
				BANK_ONLY(if(m_bRunSSAPass))
				{
					DLC_Add(PostFX::ProcessUISubSampleAlpha,
						UI3DDRAWMANAGER.GetCompositeTarget0(),
						((UI3DDRAWMANAGER.GetCompositeTargetCopy() != NULL) ? UI3DDRAWMANAGER.GetCompositeTargetCopy() : UI3DDRAWMANAGER.GetCompositeTarget0()),
						UI3DDRAWMANAGER.GetTarget(0),
						UI3DDRAWMANAGER.GetDepth(),
						scissorRect);
				}

				// FXAA
				#if __BANK
				if (m_bRunFXAAPass)
				#endif
				{
					DLC_Add(CompositeLuminanceToAlpha, UI3DDRAWMANAGER.GetCompositeTarget1(), UI3DDRAWMANAGER.GetCompositeTarget0(), scissorRect);
					DLC_Add(PostFX::ApplyFXAA, UI3DDRAWMANAGER.GetCompositeTarget0(), UI3DDRAWMANAGER.GetCompositeTarget1(), scissorRect);
				}

#if RSG_PC
				// restore UI viewport
				DLC(dlCmdSetCurrentViewport, (&UI3DDRAWMANAGER.m_Viewport));
#endif
				// Composite
				DLC_Add(CompositeToBackbuffer, UI3DDRAWMANAGER.GetCompositeTarget0(), rect, m_scenePresetPatchedData[slotIdx].m_alpha);

				DLCPopMarker(); // "Composite"
			}

			DLCPopMarker(); // "Slot"
		}

		DLC_Add(Finalize);
		DLCPopMarker();

		#if __PS3
		if (bWasEdgeNoPixelTestEnabled)
		{	
			SetEdgeNoPixelTestEnabled(true);
		}
		#endif

		PopCurrentPreset();
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawManager::LoadData() 
{
	// load data for light rigs 
	bool bFileExists = ASSET.Exists(UI_SCENE_METADATA_FILE_NAME_FULL_PATH, "meta");
#if __ASSERT
	bool bFileParsedOk = false;
#endif

	if (Verifyf(bFileExists, "UI3DDrawManager::LoadData: couldn't find \"%s\"", UI_SCENE_METADATA_FILE_NAME_FULL_PATH))
	{
	#if __ASSERT
		bFileParsedOk =
	#endif
		PARSER.LoadObject(UI_SCENE_METADATA_FILE_NAME_FULL_PATH, "meta", *this);
		Assertf(bFileParsedOk, "UI3DDrawManager::LoadData: failed to parse \"%s\"", UI_SCENE_METADATA_FILE_NAME_FULL_PATH);
	}
}


//////////////////////////////////////////////////////////////////////////
//
void UISceneObjectData::Reset()
{
	m_pPed = NULL;
	m_enabled = false; 
	m_position.Zero();
	m_position_43.Zero();
	m_rotationXYZ.Zero();
	m_bgRectXYWH.Zero();
	m_bgRectXYWH_43.Zero();
	m_perspectiveShear.Zero();
}

//////////////////////////////////////////////////////////////////////////
//
void UIScenePreset::Reset()
{
	m_elements.Reset(); 
	m_elements.SetCount(UI_MAX_SCENE_PRESET_ELEMENTS); 
	m_name = 0U;

	for (int i = 0; i < m_elements.GetCount(); i++)
	{
		m_elements[i].Reset();
	}

}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void UI3DDrawManager::AddWidgets(bkBank *bank)
{
	if (bank)
	{
		bank->PushGroup("UI 3D Draw Manager");
			bank->AddToggle("Enable rendering", &m_bEnableRendering);
			bank->AddSeparator();
			bank->AddToggle("Debug Draw", &m_bDrawDebug);
			bank->AddToggle("Disable Scissoring", &m_bDisableScissor);
			bank->AddSeparator();
			bank->AddVector("Camera Position", &m_vCamPos, -1000.0f, 1000.0f, 0.01f);
			bank->AddVector("Camera Angles Degrees", &m_vCamAnglesDeg, -180.0f, 180.0f, 0.001f);
			bank->AddSlider("Camera Field of View", &m_CamFieldOfView, 0.0f, 180.0f, 0.1f);
			bank->AddSeparator();
			bank->AddToggle("Run Skin Pass", &m_bRunSkinPass);
			bank->AddToggle("Run SSA Pass", &m_bRunSSAPass);
			bank->AddToggle("Run Forward Pass", &m_bRunForwardPass);
			bank->AddToggle("Run FXAA Pass", &m_bRunFXAAPass);
			bank->AddToggle("Run Composite Passes", &m_bRunCompositePasses);
			bank->AddSeparator();
			bank->PushGroup("Lighting Settings");
				bank->AddSeparator();
				bank->AddSlider("Current Light Rig Index", &m_currentRigIdx, 0, ((u32)UI_LIGHT_RIG_COUNT-1), 1);
				PARSER.AddWidgets(*bank, this);
			bank->PopGroup();
		
			m_editTool.Init();
			m_editTool.AddWidgets(*bank);

		bank->PopGroup();
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
// Edit tool
#if __BANK

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::Init()
{
	m_pPresetNamesCombo = NULL;
	m_presetNamesComboIdx = 0;
	m_newPresetName = "[New Preset]";
	m_editablePreset.Reset();

	UpdatePresetNamesList(UI3DDRAWMANAGER.GetPresetsArray());
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::Update()
{
	bool bPresetActive = false;

	// Add player ped by default
	for (int i = 0; i < m_editablePreset.m_elements.GetCount(); i++)
	{
		if (m_editablePreset.m_elements[i].m_enabled)
		{
			bPresetActive = true;
			CPed* pPed = CGameWorld::FindLocalPlayer();
			UI3DDRAWMANAGER.AssignPedToSlot(i, pPed, 1.0f);
		}
	}

	// Check if picker is active
	if (bPresetActive && m_bUsePedsFromPicker)
	{
		CPed* pPeds[UI_MAX_SCENE_PRESET_ELEMENTS] = { NULL, NULL, NULL, NULL, NULL };
		bool bPedFound = false;

		s32 numEntities = rage::Min<s32>(g_PickerManager.GetNumberOfEntities(), 4);

		for (s32 i = 0; i < numEntities; i++)
		{
			fwEntity* pEntity = g_PickerManager.GetEntity(i);

			if (pEntity != NULL && pEntity->GetType() == ENTITY_TYPE_PED)
			{
				pPeds[i] = static_cast<CPed*>(pEntity);
				bPedFound = true;
			}
		}

		if (bPedFound)
		{
			for (int i = 0; i < m_editablePreset.m_elements.GetCount(); i++)
			{
				if (m_editablePreset.m_elements[i].m_enabled && pPeds[i] != NULL)
				{
					UI3DDRAWMANAGER.AssignPedToSlot(i, pPeds[i], 1.0f);
				}
			}
		}
	}

	if (UI3DDRAWMANAGER.IsRenderingEnabled())
	{
		atHashString selectionName = atHashString(m_presetNames[m_presetNamesComboIdx]);
		UIScenePreset* pPreset = UI3DDRAWMANAGER.GetPreset(selectionName);
		if (pPreset != NULL) 
		{
			*pPreset = m_editablePreset;
		}
	}
	
	if (UI3DDRAWMANAGER.IsRenderingEnabled() && UI3DDRAWMANAGER.IsAvailable() && bPresetActive)
	{
		UI3DDRAWMANAGER.PushCurrentPreset(&m_editablePreset);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::GetPostFXDataOverride(Vector4& colFilter0, Vector4& colFilter1)
{
	colFilter0.x = m_postfxBWWeights.GetRedf();
	colFilter0.y = m_postfxBWWeights.GetGreenf();
	colFilter0.z = m_postfxBWWeights.GetBluef();
	colFilter0.w = m_postfxBlendFilter;
	colFilter1.x = m_postfxTintCol.GetRedf();
	colFilter1.y = m_postfxTintCol.GetGreenf();
	colFilter1.z = m_postfxTintCol.GetBluef();
	colFilter1.w = 0.0f;

}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::AddWidgets(rage::bkBank& bank)
{
	Assert(m_presetNames.GetCount() > 0);

	bank.PushGroup("Scene Presets Edit");
		bank.AddToggle("Use peds selected with picker", &m_bUsePedsFromPicker);
		bank.PushGroup("Post FX");
			bank.AddToggle("Override Object PostFX Data", &m_postfxOverrideObjectData);
			bank.AddColor("Black&White Weights", &m_postfxBWWeights);
			bank.AddColor("Tint Color", &m_postfxTintCol);
			bank.AddSlider("Blend Amount", &m_postfxBlendFilter, 0.0f, 1.0f, 0.001f);
		bank.PopGroup();
		bank.AddSeparator();
		m_pPresetNamesCombo = bank.AddCombo("Preset Select", &m_presetNamesComboIdx, m_presetNames.GetCount(), &m_presetNames[0], datCallback(MFA(UI3DDrawEditor::OnPresetNameSelected), (datBase*)this));
		bank.AddSeparator();
		bank.AddTitle("Current Preset:");
		PARSER.AddWidgets(bank, &m_editablePreset);
		bank.AddButton("Save Current Preset", datCallback(MFA(UI3DDrawEditor::OnSaveCurrentPreset), (datBase*)this));
		bank.AddButton("Delete Current Preset", datCallback(MFA(UI3DDrawEditor::OnDeleteCurrentPreset), (datBase*)this));
		bank.AddSeparator();
		bank.AddButton("SAVE", datCallback(MFA(UI3DDrawEditor::OnSavePresets), (datBase*)this));
		bank.AddButton("RELOAD", datCallback(MFA(UI3DDrawEditor::OnLoadPresets), (datBase*)this));
		bank.AddSeparator();
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::OnSaveCurrentPreset()
{
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected, add the entry to the collection
	atHashString selectionName = atHashString(m_presetNames[idx]);
	if (selectionName == m_newPresetName)
	{
		// only add to the collection is another preset with the same
		// name doesn't already exist
		if (UI3DDRAWMANAGER.GetPreset(selectionName) == NULL)
		{
			UI3DDRAWMANAGER.AddPreset(m_editablePreset);
		}
	}
	else
	{
		// otherwise, try finding the preset in the collection
		// and update it
		UIScenePreset* pPreset = UI3DDRAWMANAGER.GetPreset(selectionName);
		if (pPreset != NULL) 
		{
			*pPreset = m_editablePreset;
		}
	}

	UpdatePresetNamesList(UI3DDRAWMANAGER.GetPresetsArray());
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::OnDeleteCurrentPreset()
{
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(m_presetNames[idx]);
	if (selectionName == m_newPresetName)
	{
		m_editablePreset.Reset();
	}
	else
	{
		// otherwise, try finding the preset in the collection
		// and remove it
		UIScenePreset* pPreset = UI3DDRAWMANAGER.GetPreset(selectionName);
		if (pPreset != NULL) 
		{
			UI3DDRAWMANAGER.RemovePreset(pPreset->m_name);
			m_editablePreset.Reset();
		}
	}

	UpdatePresetNamesList(UI3DDRAWMANAGER.GetPresetsArray());
}


//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::OnSavePresets()
{
#if !__FINAL
	if (UI3DDRAWMANAGER.SavePresets())
	{
		UpdatePresetNamesList(UI3DDRAWMANAGER.GetPresetsArray());
	}
#endif
}


//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::OnLoadPresets()
{
	if (UI3DDRAWMANAGER.LoadPresets())
	{
		UpdatePresetNamesList(UI3DDRAWMANAGER.GetPresetsArray());
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::OnPresetNameSelected()
{
	m_presetChanged = true;
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(m_presetNames[idx]);
	if (selectionName == m_newPresetName)
	{
		m_editablePreset.Reset();
	}
	else
	{
		// otherwise, try finding the preset in the collection
		UIScenePreset* pPreset = UI3DDRAWMANAGER.GetPreset(selectionName);
		if (pPreset != NULL) 
		{
			m_editablePreset = *pPreset;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void UI3DDrawEditor::UpdatePresetNamesList(const atArray<UIScenePreset>& presets)
{
	m_presetNames.Reset();
	m_presetNames.PushAndGrow(m_newPresetName.TryGetCStr());
	
	for (int i = 0; i < presets.GetCount(); i++)
	{
		m_presetNames.PushAndGrow(presets[i].m_name.TryGetCStr());
	}

	m_presetNamesComboIdx = (m_presetNames.GetCount()==2)?1:0; // the list has changed, the old index is probably worthless If there is only one entry, use it. 
	if (m_presetNamesComboIdx == 1)
	{
		OnPresetNameSelected();
	}

	if (m_pPresetNamesCombo != NULL)
	{
		m_pPresetNamesCombo->UpdateCombo("Preset Select", &m_presetNamesComboIdx, m_presetNames.GetCount(), &m_presetNames[0], datCallback(MFA(UI3DDrawEditor::OnPresetNameSelected), (datBase*)this));
	}
}

#endif // __BANK
