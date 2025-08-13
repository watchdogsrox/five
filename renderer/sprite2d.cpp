//
// Title	:	Sprite2d.cpp
// Author	:	
// Started	:	
//
//	2008/06/24	-	Andrzej:	- v3.0:	cleanup & refactoring, gBlitMatrix has been retired,
//										current grcViewport modified for 2D ortho projection,
//										CustomVertexPushBuffer support added;
//	2010/03/22	-	Andrzej:	- all sprite rendering methods use normalised ((0,0)->(1,1)) screen coords mapping;
//
//
//
//
//
//
#include "renderer/sprite2d.h"

// Rage headers
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/texture.h"
#include "grcore/im.h"
#include "grcore/quads.h"
#include "paging/dictionary.h"
#include "grcore/effect.h"
#include "grcore/fvf.h"
#include "grcore/vertexbuffer.h"
#include "grcore/vertexbuffereditor.h"
#include "grcore/indexbuffer.h"
#include "grcore/device.h"
#if RSG_ORBIS
#include "grcore/texture_gnm.h"
#include "grcore/rendertarget_gnm.h"
#endif
#include "grmodel/modelfactory.h"
#include "system/system.h"
#include "system/xtl.h"

// Framework headers
#include "fwmaths/Rect.h"							// fwRect()...
#include "fwtl/customvertexpushbuffer.h"
#include "fwscene/stores/txdstore.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "Frontend/ui_channel.h"
#include "Renderer/rendertargets.h"
#include "Renderer/Deferred/GBuffer.h"
#include "shaders/ShaderLib.h"
#include "streaming/streaming.h"

RENDER_OPTIMISATIONS()

//
//
//
//
grmShader						*CSprite2d::m_imShader = NULL;
#if DEVICE_MSAA
grmShader						*CSprite2d::m_imShaderMS = NULL;
#endif
static grcEffectTechnique		m_blitTechniqueGta = grcetNONE;
static grcEffectTechnique		m_blitTechniqueGtaDO = grcetNONE;

#if GS_INSTANCED_CUBEMAP
static grcEffectTechnique		m_blitTechniqueCoronaCubeInst = grcetNONE;
#endif
static grcEffectTechnique		m_blitTechniqueCorona = grcetNONE;
static grcEffectTechnique		m_blitTechniqueColorCorrected = grcetNONE;
static grcEffectTechnique		m_blitTechniqueColorCorrectedGradient = grcetNONE;
static grcEffectTechnique		m_blitTechniqueSeeThrough = grcetNONE;
static grcEffectTechnique		m_blitTechniqueRefMipBlur = grcetNONE;
static grcEffectTechnique		m_blitTechniqueRefMipBlurBlend = grcetNONE;
static grcEffectTechnique		m_blitTechniqueRefSkyPortal = grcetNONE;
static grcEffectTechnique		m_blitTechniqueDistantLights = grcetNONE;
PPU_ONLY(static grcEffectTechnique		m_resolveTechniqueMSAA2x = grcetNONE);
PPU_ONLY(static grcEffectTechnique		m_resolveTechniqueMSAA4x = grcetNONE);
PPU_ONLY(static grcEffectTechnique		m_resolveTechnique = grcetNONE);
static grcEffectTechnique		m_resolveDepthTechnique = grcetNONE;
static grcEffectTechnique		m_copyGBufferTechnique = grcetNONE;
static grcEffectTechnique		m_blitTechniqueGameGlows = grcetNONE;
static grcEffectTechnique		m_toneMapTechnique = grcetNONE;
static grcEffectTechnique		m_luminanceToAlphaTechnique = grcetNONE;
static grcEffectTechnique		m_gBufferSplatTechnique = grcetNONE;
static grcEffectTechnique		m_UIWorldIconTechnique = grcetNONE;

static grcEffectTechnique		m_blitTechniqueMipMap = grcetNONE;

#if __BANK
static grcEffectTechnique		m_blitTechniqueSignedNormalize = grcetNONE;
#endif

#if USE_HQ_ANTIALIASING
static grcEffectTechnique		m_highQualityAATechnique = grcetNONE;
#endif // USE_HQ_ANTIALIASING

#if __PPU
static grcEffectTechnique		m_zCullReloadTechnique = grcetNONE;
#endif

static grcEffectTechnique		m_alphaMaskToStencilMaskTechnique = grcetNONE;

#if __XENON
static grcEffectTechnique		m_restoreHiZTechnique = grcetNONE;
static grcEffectTechnique		m_restoreDepthTechnique = grcetNONE;
#endif

static grcEffectVar				m_shdDiffuseTextureID = grcevNONE;
static grcEffectVar				m_shdRefMipBlurParamsID = grcevNONE;
static grcEffectVar				m_shdLightningConstants = grcevNONE;

#if USE_HQ_ANTIALIASING
static grcEffectVar			    m_shdTransparentSrcMapParamsID = grcevNONE;
#endif // USE_HQ_ANTIALIASING

static grcEffectVar				m_shdCoronasDepthMapID = grcevNONE;

static grcEffectVar				m_shdDepthPointMapID = grcevNONE;

static grcEffectVar				m_shdRenderPointMapID = grcevNONE;
static grcEffectVar				m_shdRenderPointMapINTID = grcevNONE;
static grcEffectVar				m_shdRenderPointMapINTParamID = grcevNONE;
static grcEffectVar				m_shdRenderCubeMapID = grcevNONE;
static grcEffectVar				m_shdRenderTexArrayID = grcevNONE;
static grcEffectVar				m_shdRefMipBlurMapID = grcevNONE;
#if DEVICE_MSAA
static grcEffectVar				m_shdRenderTexMsaaID = grcevNONE;
static grcEffectVar				m_shdRenderTexMsaaParamID = grcevNONE;
static grcEffectVar				m_shdRenderTexMsaaINTID = grcevNONE;
static grcEffectVar				m_shdRenderTexMsaaDepth = grcevNONE;
#endif
#if DEVICE_EQAA
static grcEffectVar				m_shdRenderTexFmaskID = grcevNONE;
#endif

static grcEffectVar				m_shdGeneralParams0ID = grcevNONE;
static grcEffectVar				m_shdGeneralParams1ID = grcevNONE;

#if __XENON
static grcEffectVar				m_shdDepthTextureID = grcevNONE;
#endif

static grcEffectVar				m_shdTexelSizeID = grcevNONE;

static grcEffectVar				m_shdNoiseTextureID = grcevNONE;

static grcEffectVar				m_shdTonemapColorFilterParams0ID = grcevNONE;
static grcEffectVar				m_shdTonemapColorFilterParams1ID = grcevNONE;


#if __PPU
static grcEffectVar				m_shdRenderQuincunxMapID = grcevNONE;
static grcEffectVar				m_shdRenderQuincunxAltMapID = grcevNONE;
static grcEffectVar				m_shdRenderGaussMapID = grcevNONE;
#if 0 == PSN_USE_FP16
static grcEffectTechnique		m_shdRenderIMLitPackedID = grcetNONE;
static grcEffectTechnique		m_shdRenderIMLitUnpackedID = grcetNONE;
#endif // 0 == PSN_USE_FP16

#endif // __PPU

#if DEVICE_MSAA
static grcEffectTechnique		m_resolveDepthMSTechnique = grcetNONE;
#endif //DEVICE_MSAA

bool CUIWorldIconQuad::sm_initializedCommon = false;
grcBlendStateHandle CUIWorldIconQuad::sm_StandardSpriteBlendStateHandle = grcStateBlock::BS_Invalid;
float CUIWorldIconQuad::sm_screenRatio = 1.0f;

static grcEffectTechnique		m_blitTechniqueSprite3d = grcetNONE;


#if USE_MULTIHEAD_FADE
#include "fwsys/timer.h"

namespace fade
{
	static grcEffectTechnique		m_technique		= grcetNONE;

	static grcEffectVar				m_shdAreaParams	= grcevNONE;
	static grcEffectVar				m_shdTimeParams	= grcevNONE;
	static grcEffectVar				m_shdWaveParams	= grcevNONE;
	
	static Vector4 m_areaParams	(0.0f, 0.0f, 1.0f, 1.0f);
	static Vector4 m_timeParams	(0.0f, 1.0f, 0.0f, 0.0f);
	static Vector4 m_waveParams	(1.0f, 1.0f, 0.0f, 0.0f);
}

u32 CSprite2d::GetFadeCurrentTime()
{
	return fwTimer::GetSystemTimeInMilliseconds();
}

// name:		CSprite2d::SetMultiFadeParams
// description:	Initialize mult-head fade parameters
// returns:		End time in milliseconds

u32 CSprite2d::SetMultiFadeParams(const fwRect &safeArea, bool bIn, u32 uDurationSec,
		float fAcceleration, float fWaveSize, float fAlphaOffset, bool bInstantShudders, u32 uExtraTime)
{
	int iBlinderDelay = bIn ? 0 : uExtraTime;
	const u32 uTimeMs = GetFadeCurrentTime() + iBlinderDelay;
	const u32 uDurationMs = bInstantShudders ? 0 : 1000*uDurationSec + static_cast<int>( 1000.f*fWaveSize+0.5f );
	float cx=0.f,cy=0.f;
	safeArea.GetCentre(cx,cy);

	fade::m_areaParams	= Vector4( cx, 1.f-cy, 0.5f*safeArea.GetWidth(), 0.5f*safeArea.GetHeight() );
	fade::m_timeParams	= Vector4( 0.001f * uTimeMs, uDurationSec && !bInstantShudders ? 1.f/uDurationSec : 0.f, 0.5f*fAcceleration, bIn ? 1.f:0.f );
	fade::m_waveParams	= Vector4( 1.f / fWaveSize, fAlphaOffset, 0.f, 0.f );

	return uTimeMs + uDurationMs;
}

void CSprite2d::ResetMultiFadeArea()
{
	fwRect area(0.f, 0.f, 1.f, 1.f);
	if (GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
	}
	else
	{
		const float SIXTEEN_BY_NINE = 16.0f/9.0f;
		float fAspect = CHudTools::GetAspectRatio();
		if(fAspect > SIXTEEN_BY_NINE)
		{
			float fDifference = (SIXTEEN_BY_NINE / fAspect) * 0.5f;
			area = fwRect(0.5f - fDifference, 0, 0.5f + fDifference, 1.0f);
		}
	}
	
	float cx=0.f,cy=0.f;
	area.GetCentre(cx,cy);
	fade::m_areaParams	= Vector4( cx, 1.f-cy, 0.5f*area.GetWidth(), 0.5f*area.GetHeight() );
}

// name:		CSprite2d::DrawMultiFade
// description:	Draw a full-screen multi-head fade
//				used for movie-cutscene transitions in multi-head configurations

void CSprite2d::DrawMultiFade(bool bUseTime)
{
	Vector4 timePar;
	
	if(bUseTime)
	{
		const float curTime = 0.001f * GetFadeCurrentTime();
		timePar.x = (curTime - fade::m_timeParams.x) * fade::m_timeParams.y;
		timePar.y = 0.f;
		timePar.z = fade::m_timeParams.z;
		timePar.w = fade::m_timeParams.w;
	}
	else
	{
		timePar.x = 1.0f;
		timePar.y = 0.f;
		timePar.z = fade::m_timeParams.z;
		timePar.w = fade::m_timeParams.w;
	}

	AssertVerify(m_imShader->BeginDraw(grmShader::RMC_DRAW, FALSE, fade::m_technique));
	m_imShader->SetVar(fade::m_shdAreaParams,	fade::m_areaParams);
	m_imShader->SetVar(fade::m_shdTimeParams,	timePar);
	m_imShader->SetVar(fade::m_shdWaveParams,	fade::m_waveParams);

	m_imShader->Bind(0);
#if 0	//INPUT_UNIT_QUAD
	GRCDEVICE.DrawUnitQuad(true);
#else
	grcDrawSingleQuadf( 0.f,0.f, 1.f,1.f, 0.f, 0.f,0.f,1.f,1.f, Color32() );
#endif
	m_imShader->UnBind();
	m_imShader->EndDraw();
}
#endif	//USE_MULTIHEAD_FADE

//
// name:		Delete
// description:	Delete texture
void CSprite2d::Delete()
{
	if (m_pTexture != NULL )
	{
		m_pTexture->Release();
		m_pTexture = NULL;
	}
}



//
// Name			:	SetTexture
// Purpose		:	Loads a texture
// Parameters	:	None
// Returns		:	Nothing
// 
void CSprite2d::SetTexture(const char *pTextureName)
{
	// check we aren't trying to change the texture while on the render thread
	Assert(!m_pTexture || !CSystem::IsThisThreadId(SYS_THREAD_RENDER));		

	Delete();
	if(pTextureName)
	{
		m_pTexture = fwTxd::GetCurrent()->Lookup(pTextureName);
		if(m_pTexture == NULL)
		{
			//m_pTexture = grcTextureFactory::GetInstance().Create(pTextureName);
		}
		else
		{
			m_pTexture->AddRef();
		}
	}
}

//
//
//
//
void CSprite2d::SetTexture(grcTexture *pTexture)
{
	Delete();
	m_pTexture = pTexture;
	if(m_pTexture)
	{
		m_pTexture->AddRef();
	}
}


//
// name:		CSprite2d::SetRenderState
// description:	set render state to draw this sprite
void CSprite2d::SetRenderState()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread
	CSprite2d::SetRenderState(m_pTexture);
}

//
// name:		CSprite2d::SetRenderState
// description:	set render state to draw a sprite with these params
void CSprite2d::SetRenderState(const grcTexture* pTex)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

	m_imShader->SetVar(m_shdDiffuseTextureID, pTex);
	grcBindTexture(pTex);

	if(grcViewport::GetCurrent())
	{
		grcWorldIdentity();
	}
}

void CSprite2d::ClearRenderState()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	m_imShader->SetVar(m_shdDiffuseTextureID, grcTexture::None);
	grcBindTexture(grcTexture::None);
}


void CSprite2d::SetRefMipBlurParams(const Vector4& params, const grcTexture *pTex)
{
	m_imShader->SetVar(m_shdRefMipBlurParamsID, params);
	m_imShader->SetVar(m_shdRenderCubeMapID, pTex);
}

void CSprite2d::SetRefMipBlurMap(grcTexture *tex)
{
	m_imShader->SetVar(m_shdRefMipBlurMapID, tex);
}

void CSprite2d::SetColorFilterParams(const Vector4 &colorFilter0, const Vector4 &colorFilter1)
{
	m_imShader->SetVar(m_shdTonemapColorFilterParams0ID, colorFilter0);
	m_imShader->SetVar(m_shdTonemapColorFilterParams1ID, colorFilter1);
}

void CSprite2d::SetLightningConstants(const Vector2& params)
{
	m_imShader->SetVar(m_shdLightningConstants, params);
}

void CSprite2d::SetTexelSize(const Vector2 &size)
{
	const Vector4 texSize(size.x,size.y,0.0f,0.0f);
	m_imShader->SetVar(m_shdTexelSizeID,texSize);
	
}

void CSprite2d::SetTexelSize(const Vector4 &size)
{
	m_imShader->SetVar(m_shdTexelSizeID,size);
}

void CSprite2d::SetGeneralParams(const Vector4 &params0, const Vector4 &params1)
{
	m_imShader->SetVar(m_shdGeneralParams0ID, params0);
	m_imShader->SetVar(m_shdGeneralParams1ID, params1);
}

void CSprite2d::SetDepthTexture(grcTexture *tex)
{
	m_imShader->SetVar(m_shdCoronasDepthMapID,tex);
	m_imShader->SetVar(m_shdDepthPointMapID,tex);
}

void CSprite2d::SetNoiseTexture(grcTexture *tex)
{
	m_imShader->SetVar(m_shdNoiseTextureID, tex);
}

GRC_ALLOC_SCOPE_DECLARE_GLOBAL(static, s_CustomListAllocScope)

void CSprite2d::BeginCustomList(CustomShaderType type, const grcTexture *pTex, const int pass /*= -1*/)
{
	GRC_ALLOC_SCOPE_PUSH(s_CustomListAllocScope)

	// set the default render state and texture variable
	m_imShader->SetVar(m_shdDiffuseTextureID, pTex);
	
	float TexSizeX = 1.0f; 
	float TexSizeY = 1.0f;
	float InvTexSizeX = 1.0f;
	float InvTexSizeY = 1.0f;

	if(pTex)
	{
		TexSizeX = (float)pTex->GetWidth();
		TexSizeY = (float)pTex->GetHeight();		
		InvTexSizeX = 1.0f / TexSizeX;
		InvTexSizeY = 1.0f / TexSizeY;
	}	

#if PGHANDLE_REF_COUNT
	m_type = type;
#endif

	// Select technique and set any extra texture / renderstate required
	grcEffectTechnique	t_shaderTechnique=grcetNONE;
	int					t_shaderPass=0;

	switch(type)
	{
		case CS_CORONA_DEFAULT:
			Assert(pass == 0 || pass == 1 || pass == 2);
			t_shaderTechnique=m_blitTechniqueCorona;
			t_shaderPass=pass; // default=0
			break;
		case CS_CORONA_SIMPLE_REFLECTION:
			t_shaderTechnique=m_blitTechniqueCorona;
			t_shaderPass=3;
			break;
		case CS_CORONA_PARABOLOID_REFLECTION:
			t_shaderTechnique=m_blitTechniqueCorona;
			t_shaderPass=4;
			break;
#if GS_INSTANCED_CUBEMAP
		case CS_CORONA_CUBEINST_REFLECTION:
			t_shaderTechnique=m_blitTechniqueCoronaCubeInst;
			t_shaderPass=0;
			break;
#endif
		case CS_CORONA_LIGHTNING:
			t_shaderTechnique=m_blitTechniqueCorona;
			t_shaderPass=5;
			break;
		case CS_COLORCORRECTED:
			t_shaderTechnique=m_blitTechniqueColorCorrected;
			t_shaderPass=0;
			break;
		case CS_COLORCORRECTED_GRADIENT:
			t_shaderTechnique=m_blitTechniqueColorCorrectedGradient;
			t_shaderPass=0;
			break;
		case CS_SEETHROUGH:
			t_shaderTechnique=m_blitTechniqueSeeThrough;
			t_shaderPass=0;
			break;
		case CS_MIPMAP_BLUR_0:
			t_shaderTechnique=m_blitTechniqueRefMipBlur;
			t_shaderPass=pass;
			m_imShader->SetVar(m_shdRenderPointMapID, pTex);
			break;
		case CS_MIPMAP_BLUR_BLEND:
			t_shaderTechnique=m_blitTechniqueRefMipBlurBlend;
			t_shaderPass=0;
			break;
//#if SCAN_SKY_PORTALS_FOR_REFLECTION
		case CS_REFLECTION_SKY_PORTALS:
			t_shaderTechnique=m_blitTechniqueRefSkyPortal;
			t_shaderPass=0;
			break;
//#endif // SCAN_SKY_PORTALS_FOR_REFLECTION
#if __PPU
		case CS_RESOLVE_MSAA2X:
			m_imShader->SetVar(m_shdRenderQuincunxMapID,pTex);
			m_imShader->SetVar(m_shdRenderQuincunxAltMapID,pTex);
			t_shaderTechnique = m_resolveTechniqueMSAA2x;
			t_shaderPass = 0;
			break;
		case CS_RESOLVE_MSAA4X:
   			m_imShader->SetVar(m_shdRenderGaussMapID,pTex);
			t_shaderTechnique = m_resolveTechniqueMSAA4x;
			t_shaderPass = 0;
			break;
		case CS_RESOLVE_SDDOWNSAMPLEFILTER:
			t_shaderTechnique = m_resolveTechnique;
			t_shaderPass = 0;
			break;
		case CS_RESOLVE_DEPTHTOVS:
			m_imShader->SetVar(m_shdRenderPointMapID,pTex);
			t_shaderTechnique = m_resolveTechnique;
			t_shaderPass = 1;
			break;
		case CS_RESOLVE_ALPHASTENCIL:
			t_shaderTechnique = m_resolveTechnique;
			t_shaderPass = 2;
			break;			
#if 0 == PSN_USE_FP16
		case CS_IM_LIT_PACKED:
			t_shaderTechnique = m_shdRenderIMLitPackedID;
			t_shaderPass = 0;
			break;
		case CS_IM_UNLIT_PACKED:
			t_shaderTechnique = m_shdRenderIMLitUnpackedID;
			t_shaderPass = 0;
			break;
#endif // 0 == PSN_USE_FP16
		case CS_SCULL_RELOAD:
			t_shaderTechnique = m_zCullReloadTechnique;
			t_shaderPass = 0;
			break;
		case CS_ZCULL_RELOAD:
			t_shaderTechnique = m_zCullReloadTechnique;
			t_shaderPass = 1;
			break;
		case CS_ZCULL_RELOAD_GT:
			t_shaderTechnique = m_zCullReloadTechnique;
			t_shaderPass = 2;
			break;
		case CS_ZCULL_NEAR_RELOAD:
			t_shaderTechnique = m_zCullReloadTechnique;
			t_shaderPass = 3;
			break;
#endif // __PPU
		case CS_ALPHA_MASK_TO_STENCIL_MASK:
			m_imShader->SetVar(m_shdRenderPointMapID,pTex);
			t_shaderTechnique = m_alphaMaskToStencilMaskTechnique;
			t_shaderPass = 0;
			break;
#if __XENON
		case CS_ALPHA_MASK_TO_STENCIL_MASK_STENCIL_AS_COLOR:
			m_imShader->SetVar(m_shdRenderPointMapID,pTex);
			t_shaderTechnique = m_alphaMaskToStencilMaskTechnique;
			t_shaderPass = 1;
			break;
		case CS_RESTORE_HIZ:
			t_shaderTechnique = m_restoreHiZTechnique;
			t_shaderPass = 0;
			break;
		case CS_RSTORE_GBUFFER_DEPTH:
			t_shaderTechnique = m_restoreDepthTechnique;
			t_shaderPass = pass;
			m_imShader->SetVar(m_shdDepthTextureID, pTex);
			break;
#endif
		case CS_COPY_GBUFFER:
			t_shaderTechnique = m_copyGBufferTechnique;
			t_shaderPass = 0;
			break;
		case CS_RESOLVE_DEPTH:
			t_shaderTechnique = m_resolveDepthTechnique;
			t_shaderPass = 0;
			break;
		case CS_BLIT:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 1;
			break;
		case CS_BLIT_NOFILTERING:
			m_imShader->SetVar(m_shdRenderPointMapID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 2;
			break;
		case CS_BLIT_DEPTH:
			m_imShader->SetVar(m_shdRenderPointMapID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 3;
			break;
		case CS_BLIT_STENCIL:
			m_imShader->SetVar(m_shdRenderPointMapINTParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
			m_imShader->SetVar(m_shdRenderPointMapINTID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 4;
			break;
		case CS_BLIT_SHOW_ALPHA:
			m_imShader->SetVar(m_shdRenderPointMapID, pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 5;
			break;
		case CS_BLIT_GAMMA:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 6;
			break;
		case CS_BLIT_POW:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 7;
			break;
		case CS_BLIT_CALIBRATION:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 8;
			break;
		case CS_BLIT_CUBE_MAP:
			m_imShader->SetVar(m_shdRenderCubeMapID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 9;
			break;
		case CS_BLIT_PARABOLOID:
			m_imShader->SetVar(m_shdRenderCubeMapID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 10;
			break;
		case CS_BLIT_AA:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 11;
			break;
		case CS_BLIT_AAAlpha:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 12;
			break;
		case CS_BLIT_COLOUR:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 13;
			break;
		case CS_BLIT_TO_ALPHA:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 14;
			break;
		case CS_BLIT_TO_ALPHA_BLUR:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 15;
			break;
		case CS_BLIT_DEPTH_TO_ALPHA:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 16;
			break;
		case CS_FOWCOUNT:
			m_imShader->SetVar(m_shdRenderPointMapID, pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 17;
			break;
		case CS_FOWAVERAGE:
			m_imShader->SetVar(m_shdRenderPointMapID, pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 18;
			break;
		case CS_FOWFILL:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 19;
			break;
#if RSG_ORBIS || __D3D11
		case CS_BLIT_TEX_ARRAY:
			m_imShader->SetVar(m_shdRenderTexArrayID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 20;
			break;
#endif // RSG_ORBIS || __D3D11
#if DEVICE_MSAA
		case CS_BLIT_MSAA_DEPTH:
			m_imShader->SetVar(m_shdRenderTexMsaaParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
			m_imShader->SetVar(m_shdRenderTexMsaaID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 21;
			break;
		case CS_BLIT_MSAA_STENCIL:
			m_imShader->SetVar(m_shdRenderTexMsaaINTID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 22;
			break;
		case CS_BLIT_MSAA:
			m_imShader->SetVar(m_shdRenderTexMsaaParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
			m_imShader->SetVar(m_shdRenderTexMsaaID,pTex);
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 23;
			break;
		case CS_BLIT_FMASK:
			m_imShader->SetVar(m_shdRenderTexMsaaParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
			m_imShader->SetVar(m_shdRenderTexMsaaID, pTex);
# if DEVICE_EQAA
			m_imShader->SetVar(m_shdRenderTexFmaskID, static_cast<const grcRenderTargetMSAA*>(pTex)->GetResolveFragmentMask());
# endif // DEVICE_EQAA
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 24;
			break;
#endif // DEVICE_MSAA
		case CS_BLIT_XZ_SWIZZLE:
			t_shaderTechnique = m_blitTechniqueGta;
			t_shaderPass = 21 + 4*DEVICE_MSAA;
			break;
		case CS_MIPMAP:
			t_shaderTechnique = m_blitTechniqueMipMap;
			t_shaderPass = pass;
			break;
		case CS_DISTANT_LIGHTS:
			t_shaderTechnique = m_blitTechniqueDistantLights;
			t_shaderPass = pass == -1 ? 0 : pass;
			break;
		case CS_GAMEGLOWS:
			t_shaderTechnique = m_blitTechniqueGameGlows;
#if RSG_PC
			if( GRCDEVICE.GetMSAA()>1 )
			{
				m_imShader->SetVar(m_shdRenderTexMsaaParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
				m_imShader->SetVar(m_shdRenderTexMsaaID, pTex);
				t_shaderPass = 1;
			}
			else
#endif // RSG_ PC
			{
				t_shaderPass = 0;
			}
			break;
		case CS_FSGAMEGLOWS:
			t_shaderTechnique = m_blitTechniqueGameGlows;

#if RSG_PC
			if( GRCDEVICE.GetMSAA()>1 )
			{
				m_imShader->SetVar(m_shdRenderTexMsaaParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
				m_imShader->SetVar(m_shdRenderTexMsaaID, pTex);
				t_shaderPass = 3;
			}
			else
			{
				t_shaderPass = 2;
			}
#else
			t_shaderPass = 1;
#endif
			break;
		case CS_FSGAMEGLOWSMARKER:
			t_shaderTechnique = m_blitTechniqueGameGlows;

#if RSG_PC
			// We're using the resolved post alpha depth buffer, so no need for the MSAA version.
			//if( GRCDEVICE.GetMSAA()>1 )
			//{
			//	m_imShader->SetVar(m_shdRenderTexMsaaParamID, Vec4V(TexSizeX, TexSizeY, InvTexSizeX, InvTexSizeY));
			//	m_imShader->SetVar(m_shdRenderTexMsaaID, pTex);
			//	t_shaderPass = 5;
			//}
			//else
			{
				t_shaderPass = 4;
			}
#else
			t_shaderPass = 2;
#endif
			break;
		case CS_COPYDEPTH:
			t_shaderTechnique = m_resolveDepthTechnique;
			t_shaderPass = 0;
			break;
		case CS_TONEMAP:
			m_imShader->SetVar(m_shdRenderPointMapID, pTex);
			t_shaderTechnique = m_toneMapTechnique;
			t_shaderPass = pass == -1 ? 0 : pass;
			t_shaderPass = 0;
			break;
		case CS_LUMINANCE_TO_ALPHA:
			m_imShader->SetVar(m_shdRenderPointMapID, pTex);
			t_shaderTechnique = m_luminanceToAlphaTechnique;
			t_shaderPass = 0;
			break;
		case CS_GBUFFER_SPLAT:
			t_shaderTechnique = m_gBufferSplatTechnique;
			t_shaderPass = 0;
			break;
#if USE_HQ_ANTIALIASING
		case CS_HIGHQUALITY_AA:
			m_imShader->SetVar(m_shdTransparentSrcMapParamsID, pTex);
			t_shaderTechnique = m_highQualityAATechnique;
			t_shaderPass = 0;
			break;
#endif
#if __BANK
		case CS_SIGNED_AS_UNSIGNED:
			t_shaderTechnique = m_blitTechniqueSignedNormalize;
			t_shaderPass = 0;
			break;
#endif

		case CS_COUNT:
		default:
			Assertf(false,"CSprite2d::BeginCustomList: Using unkown shader type");
			break;
	}
	
	AssertVerify(m_imShader->BeginDraw(grmShader::RMC_DRAW, FALSE, t_shaderTechnique));
	m_imShader->Bind(t_shaderPass);

}

void CSprite2d::EndCustomList()
{
	m_imShader->UnBind();
	m_imShader->EndDraw();

	m_imShader->SetVar(m_shdDiffuseTextureID, grcTexture::None);
	m_imShader->SetVar(m_shdGeneralParams0ID, Vector4(1.0, 1.0, 1.0, 0.0));
	m_imShader->SetVar(m_shdGeneralParams1ID, Vector4(0.0, 0.0, 0.0, 0.0));

#	if PGHANDLE_REF_COUNT
		switch(m_type)
		{
			case CS_MIPMAP_BLUR_0:
#		if __PPU
			case CS_RESOLVE_DEPTHTOVS:
#		endif
			case CS_ALPHA_MASK_TO_STENCIL_MASK:
			XENON_ONLY(case CS_ALPHA_MASK_TO_STENCIL_MASK_STENCIL_AS_COLOR: )
			case CS_BLIT_NOFILTERING:
			case CS_BLIT_DEPTH:
			case CS_BLIT_SHOW_ALPHA:
			case CS_TONEMAP:
				m_imShader->SetVar(m_shdRenderPointMapID,       grcTexture::None);
				break;
			case CS_BLIT_STENCIL:
				m_imShader->SetVar(m_shdRenderPointMapINTID,	grcTexture::None);
				break;
#		if __PPU
			case CS_RESOLVE_MSAA2X:
				m_imShader->SetVar(m_shdRenderQuincunxMapID,    grcTexture::None);
				m_imShader->SetVar(m_shdRenderQuincunxAltMapID, grcTexture::None);
				break;
			case CS_RESOLVE_MSAA4X:
				m_imShader->SetVar(m_shdRenderGaussMapID,    grcTexture::None);
				break;
#		endif
#		if __XENON
			case CS_RSTORE_GBUFFER_DEPTH:
			case CS_COPY_GBUFFER_DEPTH:
				m_imShader->SetVar(m_shdDepthTextureID,         grcTexture::None);
				break;
#		endif
			case CS_BLIT_CUBE_MAP:
				m_imShader->SetVar(m_shdRenderCubeMapID,        grcTexture::None);
				break;
			default:;
		}
#	endif

	GRC_ALLOC_SCOPE_POP(s_CustomListAllocScope)
}

#if DEVICE_MSAA
void CSprite2d::BeginCustomListMS(CustomShaderType type, const grcTexture *pTex, const int /*pass*/ /*= -1*/)
{
	GRC_ALLOC_SCOPE_PUSH(s_CustomListAllocScope)

	// set the default render state and texture variable
	m_imShaderMS->SetVar(m_shdDiffuseTextureID, pTex);

#if PGHANDLE_REF_COUNT
	m_type = type;
#endif

	// Select technique and set any extra texture / renderstate required
	grcEffectTechnique	t_shaderTechnique=grcetNONE;
	int					t_shaderPass=0;

	switch(type)
	{
	case CS_RESOLVE_DEPTH_MS_SAMPLE0:
		t_shaderTechnique = m_resolveDepthMSTechnique;
		t_shaderPass = 0;
		m_imShaderMS->SetVar(m_shdRenderTexMsaaDepth,pTex);
		break;
	case CS_RESOLVE_DEPTH_MS_MIN:
		t_shaderTechnique = m_resolveDepthMSTechnique;
		t_shaderPass = 1;
		m_imShaderMS->SetVar(m_shdRenderTexMsaaDepth,pTex);
		break;
	case CS_RESOLVE_DEPTH_MS_MAX:
		t_shaderTechnique = m_resolveDepthMSTechnique;
		t_shaderPass = 2;
		m_imShaderMS->SetVar(m_shdRenderTexMsaaDepth,pTex);
		break;
	case CS_RESOLVE_DEPTH_DOMINANT:
		t_shaderTechnique = m_resolveDepthMSTechnique;
		t_shaderPass = 3;
		m_imShaderMS->SetVar(m_shdRenderTexMsaaDepth,pTex);
		break;
	case CS_COUNT:
	default:
		Assertf(false,"CSprite2d::BeginCustomList: Using unkown shader type");
		break;
	}

	AssertVerify(m_imShaderMS->BeginDraw(grmShader::RMC_DRAW, FALSE, t_shaderTechnique));
	m_imShaderMS->Bind(t_shaderPass);

}

void CSprite2d::EndCustomListMS()
{
	m_imShaderMS->UnBind();
	m_imShaderMS->EndDraw();

	GRC_ALLOC_SCOPE_POP(s_CustomListAllocScope)
}

#endif //DEVISE_MSAA

// ================================================================================================
// ================================================================================================
// ================================================================================================
// TODO -- move this stuff out of sprite2d.cpp

fwRect PixelsToNormalised(const fwRect& rect)
{
	const float oow = 1.0f/GRCDEVICE.GetWidth();
	const float ooh = 1.0f/GRCDEVICE.GetHeight();

	fwRect r;

	// the reason i'm not using the fwRect ctor here is because the order of the arguments is *not* LEFT-TOP-RIGHT-BOTTOM
	r.left   = +(2.0f*oow*rect.left   - 1.0f);
	r.top    = -(2.0f*ooh*rect.top    - 1.0f);
	r.right  = +(2.0f*oow*rect.right  - 1.0f);
	r.bottom = -(2.0f*ooh*rect.bottom - 1.0f);

	return r;
}

fwRect TexelsToNormalised(const fwRect& rect, const grcTexture* texture)
{
	const float oow = 1.0f/texture->GetWidth();
	const float ooh = 1.0f/texture->GetHeight();

	fwRect r;

	// the reason i'm not using the fwRect ctor here is because the order of the arguments is *not* LEFT-TOP-RIGHT-BOTTOM
	r.left   = oow*rect.left;
	r.top    = ooh*rect.top;
	r.right  = oow*rect.right;
	r.bottom = ooh*rect.bottom;

	return r;
}



// ================================================================================================
// ================================================================================================
// ================================================================================================



//
//
// new BlitRectf uses (0,0)->(1,1) vp mapping
//
void CSprite2d::DrawUIWorldIcon(const Vector3 &pt1, const Vector3 &pt2, const Vector3 &pt3, const Vector3 &pt4,
						 float UNUSED_PARAM(zVal),
						 const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4,
						 const Color32 &color)
{
	grcWorldIdentity();
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_Invert);

	AssertVerify(m_imShader->BeginDraw(grmShader::RMC_DRAW, FALSE, m_UIWorldIconTechnique));

	m_imShader->Bind(0);
	{
		grcBegin(drawTriStrip,4);
		grcVertex(pt1.x, pt1.y, pt1.z, 0, 0, -1, color, uv1.x, uv1.y);
		grcVertex(pt2.x, pt2.y, pt2.z, 0, 0, -1, color, uv2.x, uv2.y);
		grcVertex(pt3.x, pt3.y, pt3.z, 0, 0, -1, color, uv3.x, uv3.y);
		grcVertex(pt4.x, pt4.y, pt4.z, 0, 0, -1, color, uv4.x, uv4.y);
		grcEnd();
	}
	m_imShader->UnBind();
	m_imShader->EndDraw();
}


//
//
// new BlitRectf uses (0,0)->(1,1) vp mapping
//
void CSprite2d::DrawRect(const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4,
						  float zVal,
						  const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4,
						  const Color32 &color)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PreDrawShape();

	{
		grcBegin(drawTriStrip,4);
			grcVertex(pt1.x, pt1.y, zVal, 0, 0, -1, color, uv1.x, uv1.y);
			grcVertex(pt2.x, pt2.y, zVal, 0, 0, -1, color, uv2.x, uv2.y);
			grcVertex(pt3.x, pt3.y, zVal, 0, 0, -1, color, uv3.x, uv3.y);
			grcVertex(pt4.x, pt4.y, zVal, 0, 0, -1, color, uv4.x, uv4.y);
		grcEnd();
	}

	PostDrawShape();
}

//
//
// description:	Draw a 2d rectangle with solid colour.
// Rectf() uses (0,0)->(1,1) vp mapping
//
void CSprite2d::DrawRect(float x1, float y1, float x2, float y2, float zVal, const Color32& color)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	fwRect rect01(0,1,1,0);
	SetBlitMatrix(NULL, &rect01, false);

	// diffuse-only technique:
	AssertVerify(m_imShader->BeginDraw(grmShader::RMC_DRAW, FALSE, m_blitTechniqueGtaDO));

	m_imShader->Bind(0);
	{
		grcBegin(drawTriStrip,4);
			grcVertex(x1, y1, zVal, 0.0f, 0.0f, -1.0f, color, 0, 0);
			grcVertex(x1, y2, zVal, 0.0f, 0.0f, -1.0f, color, 0, 0);
			grcVertex(x2, y1, zVal, 0.0f, 0.0f, -1.0f, color, 0, 0);
			grcVertex(x2, y2, zVal, 0.0f, 0.0f, -1.0f, color, 0, 0);
		grcEnd();
	}

	PostDrawShape();

}

void CSprite2d::PreDrawShape()
{
	fwRect rect01(0,1,1,0);
	SetBlitMatrix(NULL, &rect01, false);

	AssertVerify(m_imShader->BeginDraw(grmShader::RMC_DRAW, FALSE, m_blitTechniqueGta));

	m_imShader->Bind(0);
}

void CSprite2d::PostDrawShape()
{
	m_imShader->UnBind();
	m_imShader->EndDraw();

	CSprite2d::RestoreLastViewport(FALSE);
}

//
//
// description:	Draw a 2d rectangle into GUI area (preferred screen).
// Rectf() uses (0,0)->(1,1) vp mapping
//
void CSprite2d::DrawRectGUI(const fwRect &r, const Color32 &rgba)
{
#if SUPPORT_MULTI_MONITOR
	const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
#else
	const fwRect area = GRCDEVICE.GetMonitorConfig().m_WholeScreen.getArea();
#endif	//SUPPORT_MULTI_MONITOR
	
	DrawRect( fwRect(
		area.left + area.GetWidth()*r.left,
		area.bottom + area.GetHeight()*r.bottom,
		area.left + area.GetWidth()*r.right,
		area.bottom + area.GetHeight()*r.top
		), rgba);
}

//
// name:		CSprite2d::Draw
// description:	Draw a 2d sprite
void CSprite2d::Draw(const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, const Color32 &rgba)
{
	grcBegin(drawTriStrip,4);
		grcVertex(pt1.x, pt1.y, 0.0f, 0, 0, -1, rgba, 0.0f, 1.0f);
		grcVertex(pt2.x, pt2.y, 0.0f, 0, 0, -1, rgba, 0.0f, 0.0f);
		grcVertex(pt3.x, pt3.y, 0.0f, 0, 0, -1, rgba, 1.0f, 1.0f);
		grcVertex(pt4.x, pt4.y, 0.0f, 0, 0, -1, rgba, 1.0f, 0.0f);
	grcEnd();
}


void CSprite2d::Draw(const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4, const Color32 &rgba)
{
	grcBegin(drawTriStrip,4);
		grcVertex(pt1.x, pt1.y, 0.0f, 0, 0, -1, rgba, uv1.x, uv1.y);
		grcVertex(pt2.x, pt2.y, 0.0f, 0, 0, -1, rgba, uv2.x, uv2.y);
		grcVertex(pt3.x, pt3.y, 0.0f, 0, 0, -1, rgba, uv3.x, uv3.y);
		grcVertex(pt4.x, pt4.y, 0.0f, 0, 0, -1, rgba, uv4.x, uv4.y);
	grcEnd();
}


void CSprite2d::DrawSwitch(	bool MULTI_MONITOR_ONLY(bGUI), bool bRect, float zVal,
							const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4,
							const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4,
							const Color32 &rgba)
{
	Vector2 tmp[] = {pt1,pt2,pt3,pt4};
#if SUPPORT_MULTI_MONITOR
	if (bGUI)
	{
		MoveToScreenGUI(tmp);
	}
#endif	//SUPPORT_MULTI_MONITOR
	if (bRect)
	{
		DrawRect( tmp[0],tmp[1],tmp[2],tmp[3], zVal, uv1,uv2,uv3,uv4, rgba );
	}else
	{
		Draw( tmp[0],tmp[1],tmp[2],tmp[3], uv1,uv2,uv3,uv4, rgba );
	}
}

#if SUPPORT_MULTI_MONITOR
bool CSprite2d::MoveToScreenGUI(Vector2 *const points, const int number)
{
	const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
	const Vector2 vSize( area.GetWidth(), area.GetHeight() );
	const Vector2 vOffset( area.left, area.bottom );

	for (int i=0; i<number; ++i)
	{
		points[i] = points[i] * vSize + vOffset;
	}
	return vSize.Dot(vSize) < 0.99;
}

void CSprite2d::MovePosVectorToScreenGUI(Vector2 &pos, Vector2 &vec)
{
	const fwRect &area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
	pos.x = pos.x * area.GetWidth() + area.left;
	pos.y = pos.y * area.GetHeight() + area.bottom;
	vec.x *= area.GetWidth();
	vec.y *= area.GetHeight();
}
#endif	//SUPPORT_MULTI_MONITOR


//
//
// renders 3D sprite:
//
void CSprite2d::DrawSprite3D(float x, float y, float z, float width, float height, const Color32& color, const grcTexture* pTex, const Vector2& uv0, const Vector2& uv1)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	const Vector4 vecWidthHeight(width, height, 0.0f, 0.0f);
	m_imShader->SetVar(m_shdGeneralParams0ID, vecWidthHeight);

	m_imShader->SetVar(m_shdDiffuseTextureID, pTex);
	grcBindTexture(pTex);

	AssertVerify(m_imShader->BeginDraw(grmShader::RMC_DRAW, FALSE, m_blitTechniqueSprite3d));
	m_imShader->Bind(0);
	{
		grcBegin(drawTriStrip,4);
			grcVertex(x, y, z, uv0.x, uv0.y, 0.0f, color, 0.0f, 0.0f); // (x0,y0)
			grcVertex(x, y, z, uv0.x, uv1.y, 0.0f, color, 0.0f, 1.0f); // (x0,y1)
			grcVertex(x, y, z, uv1.x, uv0.y, 0.0f, color, 1.0f, 0.0f); // (x1,y0)
			grcVertex(x, y, z, uv1.x, uv1.y, 0.0f, color, 1.0f, 1.0f); // (x1,y1)
		grcEnd();
	}

	m_imShader->UnBind();
	m_imShader->EndDraw();

	m_imShader->SetVar(m_shdDiffuseTextureID, grcTexture::None);
	m_imShader->SetVar(m_shdGeneralParams0ID, Vector4(1.0, 1.0, 1.0, 0.0));
	grcBindTexture(grcTexture::None);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
void CSprite2d::Init(unsigned /*initMode*/)
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	//
	// initialize shader stuff:
	//
	m_imShader = grmShaderFactory::GetInstance().Create(/*"im"*/);
	Assert(m_imShader);
	m_imShader->Load("im"); 

	m_blitTechniqueGta = m_imShader->LookupTechnique("gtadrawblit");
	m_blitTechniqueGtaDO = m_imShader->LookupTechnique("gtadrawblit_do");

	// new 2d techniques moved from deferred lighting 
#if GS_INSTANCED_CUBEMAP
	m_blitTechniqueCoronaCubeInst = m_imShader->LookupTechnique("corona_cubeinst_draw");
#endif
	m_blitTechniqueCorona = m_imShader->LookupTechnique("corona");
	m_blitTechniqueColorCorrected = m_imShader->LookupTechnique("colorcorrectedunlit");
	m_blitTechniqueColorCorrectedGradient = m_imShader->LookupTechnique("colorcorrectedunlitgradient");
	m_blitTechniqueSeeThrough = m_imShader->LookupTechnique("seethrough");
	m_blitTechniqueRefMipBlur = m_imShader->LookupTechnique("refMipBlur");
	m_blitTechniqueRefMipBlurBlend = m_imShader->LookupTechnique("refMipBlurBlend");
	m_blitTechniqueRefSkyPortal = m_imShader->LookupTechnique("refSkyPortal");
	m_resolveDepthTechnique = m_imShader->LookupTechnique("CopyDepth");
	m_copyGBufferTechnique = m_imShader->LookupTechnique("CopyGBuffer");
	m_blitTechniqueDistantLights = m_imShader->LookupTechnique("DistantLights");
	m_blitTechniqueGameGlows = m_imShader->LookupTechnique("GameGlows");
	m_toneMapTechnique = m_imShader->LookupTechnique("ToneMap");
	m_UIWorldIconTechnique = m_imShader->LookupTechnique("unlit_draw");
	m_luminanceToAlphaTechnique = m_imShader->LookupTechnique("LuminanceToAlpha");
	m_blitTechniqueMipMap = m_imShader->LookupTechnique("mipMap");

#if __BANK
	m_blitTechniqueSignedNormalize = m_imShader->LookupTechnique("visualizeSignedTexture");
#endif

#if __XENON
	m_restoreHiZTechnique = m_imShader->LookupTechnique("RestoreHiZ");
	m_restoreDepthTechnique = m_imShader->LookupTechnique("RestoreDepth");
#endif

#if __PPU
	m_gBufferSplatTechnique = m_imShader->LookupTechnique("gbuffersplat");
	m_resolveTechniqueMSAA2x = m_imShader->LookupTechnique("Msaa2xAccuview");
	m_resolveTechniqueMSAA4x = m_imShader->LookupTechnique("Msaa4x");
	m_zCullReloadTechnique = m_imShader->LookupTechnique("ZCullReload");
	m_resolveTechnique = m_imShader->LookupTechnique("psnResolves");
#endif // __PPU

#if USE_HQ_ANTIALIASING
	m_highQualityAATechnique = m_imShader->LookupTechnique("HighQualityAA");
#endif // USE_HQ_ANTIALIASING

	m_alphaMaskToStencilMaskTechnique = m_imShader->LookupTechnique("AlphaMaskToStencilMask");

	m_shdDiffuseTextureID	= m_imShader->LookupVar("DiffuseTex");
	m_shdRefMipBlurParamsID	= m_imShader->LookupVar("RefMipBlurParams");
	m_shdLightningConstants	= m_imShader->LookupVar("LightningConstants");
	m_shdCoronasDepthMapID	= m_imShader->LookupVar("CoronasDepthMap");
	m_shdDepthPointMapID	= m_imShader->LookupVar("DepthPointMap");
	m_shdRenderPointMapID	= m_imShader->LookupVar("RenderPointMap");
	m_shdRenderPointMapINTID = m_imShader->LookupVar("RenderPointMapINT");
	m_shdRenderCubeMapID	= m_imShader->LookupVar("RenderCubeMap");
	m_shdRenderTexArrayID	= m_imShader->LookupVar("RenderTexArray");
	m_shdRefMipBlurMapID	= m_imShader->LookupVar("RefMipBlurMap");
	m_shdGeneralParams0ID	= m_imShader->LookupVar("GeneralParams0");
	m_shdGeneralParams1ID	= m_imShader->LookupVar("GeneralParams1");

	m_shdRenderPointMapINTParamID = m_imShader->LookupVar("RenderPointMapINTParam");

	m_shdTonemapColorFilterParams0ID	= m_imShader->LookupVar("tonemapColorFilterParams0");
	m_shdTonemapColorFilterParams1ID	= m_imShader->LookupVar("tonemapColorFilterParams1");

#if DEVICE_MSAA
	m_shdRenderTexMsaaID	= m_imShader->LookupVar("RenderTexMSAA");
	m_shdRenderTexMsaaINTID	= m_imShader->LookupVar("RenderTexMSAAINT");
	m_shdRenderTexMsaaParamID = m_imShader->LookupVar("RenderTexMSAAParam");
#endif
#if DEVICE_EQAA
	m_shdRenderTexFmaskID	= m_imShader->LookupVar("RenderTexFmask");
#endif
#if USE_HQ_ANTIALIASING
	m_shdTransparentSrcMapParamsID	= m_imShader->LookupVar("TransparentSrcMap");
#endif // USE_HQ_ANTIALIASING

#if __XENON
	m_shdDepthTextureID = m_imShader->LookupVar("DepthTexture");
#endif

	m_shdNoiseTextureID = m_imShader->LookupVar("NoiseTexture");

	m_shdTexelSizeID = m_imShader->LookupVar("TexelSize");
	
#if __PPU	

	m_shdRenderQuincunxMapID = m_imShader->LookupVar("RenderQuincunxMap");
	m_shdRenderQuincunxAltMapID = m_imShader->LookupVar("RenderQuincunxAltMap");
	m_shdRenderGaussMapID = m_imShader->LookupVar("RenderGaussMap");

#if 0 == PSN_USE_FP16
	m_shdRenderIMLitPackedID = m_imShader->LookupTechnique("packed_draw");
	m_shdRenderIMLitUnpackedID = m_imShader->LookupTechnique("packed_unlit_draw");
#endif // 0 == PSN_USE_FP16
	
#endif // __PPU	

#if USE_MULTIHEAD_FADE
	fade::m_technique		= m_imShader->LookupTechnique("RadialFade");
	fade::m_shdAreaParams	= m_imShader->LookupVar("RadialFadeArea");
	fade::m_shdTimeParams	= m_imShader->LookupVar("RadialFadeTime");
	fade::m_shdWaveParams	= m_imShader->LookupVar("RadialWaveParam");
#endif	//USE_MULTIHEAD_FADE

#if DEVICE_MSAA
	ASSET.PushFolder("common:/shaders");
	m_imShaderMS = grmShaderFactory::GetInstance().Create(/*"imMS"*/);
	Assert(m_imShaderMS);
	m_imShaderMS->Load("imMS"); 

	m_shdRenderTexMsaaDepth	= m_imShaderMS->LookupVar("DepthTextureMS");
	m_resolveDepthMSTechnique = m_imShaderMS->LookupTechnique("ResolveDepthMS");
	ASSET.PopFolder();
#endif //DEVICE_MSAA

	m_blitTechniqueSprite3d = m_imShader->LookupTechnique("draw_sprite3d");
	Assert(m_blitTechniqueSprite3d);
}

//
//
//
//
void CSprite2d::Shutdown(unsigned /*shutdownMode*/)
{
	// shutdown shader:
	m_imShader->SetVar(m_shdDiffuseTextureID,	(grcTexture*)NULL);

	if(m_imShader)
	{
		delete m_imShader;
		m_imShader		= NULL;
	}

#if DEVICE_MSAA
	if(m_imShaderMS)
	{
		delete m_imShaderMS;
		m_imShaderMS		= NULL;
	}	
#endif //DEVICE_MSAA

}



static bool ms_bAutoRestoreLastViewport[NUMBER_OF_RENDER_THREADS] = {FALSE};

static grcViewport *lastViewport[NUMBER_OF_RENDER_THREADS] = {NULL};

//
//
// sets sprite2d blit matrix (for m_blitTechniqueGta)
// it's done only once per frame (it's global variable)
//
void CSprite2d::SetBlitMatrix(grcViewport *vp, fwRect *rect, bool bForce)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread
	Assert(grcViewport::GetCurrent());	// current viewport must be present

	if(!vp)
		vp = grcViewport::GetCurrent();
	Assert(vp);

	// reset current viewport to ortho2D projection:
	const unsigned rti = g_RenderThreadIndex;
	if(bForce || (vp!=lastViewport[rti]) || (vp->IsPerspective()))
	{
		lastViewport[rti] = vp;

		if(ms_bAutoRestoreLastViewport[rti])
		{
			CSprite2d::StoreViewport(lastViewport[rti]);
		}

		vp->SetWorldIdentity();
		vp->SetCameraIdentity();
	
		if(rect)
		{
			vp->Ortho(rect->left, rect->right, rect->bottom, rect->top, 0.0f, 1.0f);
		}
		else
		{
			// change projection to Ortho(x,x+width,y+height,y,0,+1) based on current window transform
			vp->Screen();
		}
	}

}//end of SetBlitMatrix()...

//
//
// resets default VP mapping to (0,0)->(W,H)
// (as majority of Rage-level stuff requires it to work - e.g. mouse RAG, timebars, etc.):
//
void CSprite2d::ResetDefaultScreenVP()
{
	grcViewport* m_StoredViewport = grcViewport::SetCurrent(grcViewport::GetDefaultScreen());
	if(grcViewport::GetCurrent())
		grcViewport::GetCurrent()->Screen();
	grcViewport::SetCurrent(m_StoredViewport);
}


// stored viewport values:
#define INIT_IDENTITY_MATRIX(...)                                              \
	__VA_ARGS__                                                                \
	1.f, 0.f, 0.f, 0.f,                                                        \
	0.f, 1.f, 0.f, 0.f,                                                        \
	0.f, 0.f, 1.f, 0.f,                                                        \
	0.f, 0.f, 0.f, 1.f,
#define INIT_IDENTITY_MATRIX_1      INIT_IDENTITY_MATRIX()
#define INIT_IDENTITY_MATRIX_2      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_1)
#define INIT_IDENTITY_MATRIX_3      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_2)
#define INIT_IDENTITY_MATRIX_4      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_3)
#define INIT_IDENTITY_MATRIX_5      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_4)
#define INIT_IDENTITY_MATRIX_6      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_5)
#define INIT_IDENTITY_MATRIX_7      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_6)
#define INIT_IDENTITY_MATRIX_8      INIT_IDENTITY_MATRIX(INIT_IDENTITY_MATRIX_7)
#if   NUMBER_OF_RENDER_THREADS == 1
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_1
#elif NUMBER_OF_RENDER_THREADS == 2
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_2
#elif NUMBER_OF_RENDER_THREADS == 3
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_3
#elif NUMBER_OF_RENDER_THREADS == 4
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_4
#elif NUMBER_OF_RENDER_THREADS == 5
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_5
#elif NUMBER_OF_RENDER_THREADS == 6
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_6
#elif NUMBER_OF_RENDER_THREADS == 7
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_7
#elif NUMBER_OF_RENDER_THREADS == 8
#	define INIT_IDENTITY_MATRICES   INIT_IDENTITY_MATRIX_8
#else
#	error "number of render threads not currently supported"
#endif

static ALIGNAS(16) float ms_lastProjectionMtxF32[] = { INIT_IDENTITY_MATRICES };
static ALIGNAS(16) float ms_lastCameraMtxF32[]     = { INIT_IDENTITY_MATRICES };
static ALIGNAS(16) float ms_lastWorldMtxF32[]      = { INIT_IDENTITY_MATRICES };
static Mat44V* const ms_lastProjectionMtx = (Mat44V*)ms_lastProjectionMtxF32;
static Mat34V* const ms_lastCameraMtx     = (Mat34V*)ms_lastCameraMtxF32;
static Mat34V* const ms_lastWorldMtx      = (Mat34V*)ms_lastWorldMtxF32;
static grcViewport	*ms_lastStoredViewport[NUMBER_OF_RENDER_THREADS] = {NULL};

void CSprite2d::StoreViewport(grcViewport *vp)
{
	if(vp)
	{
		const unsigned rti = g_RenderThreadIndex;

		Assertf(ms_lastStoredViewport[rti]==NULL, "Sprite2d: Previously stored viewport has not been restored!");

		ms_lastStoredViewport[rti]	= vp;
		ms_lastProjectionMtx[rti]	= vp->GetProjection();
		ms_lastCameraMtx[rti]		= vp->GetCameraMtx();
		ms_lastWorldMtx[rti]		= vp->GetWorldMtx();
	}
}

//
//
//
//
void CSprite2d::RestoreLastViewport(bool bForce)
{
	const unsigned rti = g_RenderThreadIndex;
	if((bForce || ms_bAutoRestoreLastViewport[rti]) && ms_lastStoredViewport[rti])
	{
		ms_lastStoredViewport[rti]->SetCameraMtx(ms_lastCameraMtx[rti]);
		ms_lastStoredViewport[rti]->SetWorldMtx(ms_lastWorldMtx[rti]);
		ms_lastStoredViewport[rti]->SetProjection(ms_lastProjectionMtx[rti]);

		ms_lastStoredViewport[rti] = NULL;
	}
}


//
// use this with care, at it slows down sprite2d rendering:
// if TRUE, then viewport is restored to perspective after EVERY sprite2d call
// use only for debug rendering and when 2d/3d font text is mixed with 3d lines
// (e.g. CRenderPhaseDebug3d::AddToDrawList())
//
// best practice is to separate 2d text rendering and any 3d rendering
// (render them in different renderphases, using separate viewports, etc.)
//
void CSprite2d::SetAutoRestoreViewport(bool bRestore)
{
	ms_bAutoRestoreLastViewport[g_RenderThreadIndex] = bRestore;
}


//
//
//
//
void CSprite2d::OverrideBlitMatrix(grcViewport& vp)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

fwRect rect01(0,1,1,0);
	SetBlitMatrix(&vp, &rect01, TRUE);
}

//
//
//
//
void CSprite2d::OverrideBlitMatrix(float width, float height)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread
	CSprite2d::OverrideBlitMatrix(0.0f, width, 0.0f, height);
}

//
//
//
//
void CSprite2d::OverrideBlitMatrix(float left, float right, float top, float bottom)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

fwRect rect(left, bottom, right, top);
	SetBlitMatrix(NULL, &rect, TRUE);
}

//
//
//
//
void CSprite2d::ResetBlitMatrix()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

fwRect rect01(0,1,1,0);
	SetBlitMatrix(NULL, &rect01, TRUE);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIWorldIconQuad::CUIWorldIconQuad()
: m_worldPosition(0.0f, 0.0f, 0.0f)
, m_size(0.0f, 0.0f)
, m_color(255, 255, 255, 255)
, m_useAlpha(true)
, m_isTextureSet(false)
, m_pTextureName(NULL)
, m_pTextureDictionary(NULL)
, m_textureSlot(-1)
{
	Clear();
	InitCommon();
}

CUIWorldIconQuad::~CUIWorldIconQuad()
{
	Clear();
}

void CUIWorldIconQuad::Clear()
{
	m_sprite.Delete();

	m_useAlpha = true;
	m_worldPosition.Set(0.0f, 0.0f, 0.0f);
	m_size.Set(0.0f, 0.0f);
	m_color.Set(255, 255, 255, 255);

	if(m_isTextureSet)
		g_TxdStore.RemoveRef(m_textureSlot, REF_OTHER);
	m_isTextureSet = false;
	m_textureSlot = -1;
	m_pTextureName = NULL;
	m_pTextureDictionary = NULL;
}

void CUIWorldIconQuad::Update()
{
	if(!m_isTextureSet)
	{
		if(m_textureSlot != -1 && m_pTextureName)
		{
			LoadTexure();
		}
		else if(m_pTextureName && m_pTextureDictionary)
		{
			SetTexture(m_pTextureName, m_pTextureDictionary);
		}
	}
}

float CUIWorldIconQuad::GetWidth() const
{
	return float(m_sprite.GetTexture()->GetWidth()) * m_size.x;
}

float CUIWorldIconQuad::GetHeight() const
{
	return float(m_sprite.GetTexture()->GetHeight()) * m_size.y;
}

bool CUIWorldIconQuad::HasTexture() const
{
	return m_sprite.HasTexture();
}

void CUIWorldIconQuad::AddToDrawList()
{
	if(!m_isTextureSet)
	{
		return;
	}

	if(!uiVerify(m_sprite.HasTexture()))
	{
		return;
	}

 	float halfWidth = GetWidth() * 0.5f;
 	float halfHeight = GetHeight() * 0.5f;

	Vector3 camRight = camInterface::GetRight();
	Vector3 camUp = camInterface::GetUp();

	Vector3 r1 = m_worldPosition - (camRight*halfWidth) + (camUp*halfHeight);
	Vector3 r2 = m_worldPosition - (camRight*halfWidth) - (camUp*halfHeight);
	Vector3 r3 = m_worldPosition + (camRight*halfWidth) + (camUp*halfHeight);
	Vector3 r4 = m_worldPosition + (camRight*halfWidth) - (camUp*halfHeight);

	Vector2 uv1(0.0f, 0.0f);
	Vector2 uv2(0.0f, 1.0f);
	Vector2 uv3(1.0f, 0.0f);
	Vector2 uv4(1.0f, 1.0f);

	DLC(CDrawUIWorldIcon, (r1, r2, r3, r4, uv1, uv2, uv3, uv4, m_color, m_sprite.GetTexture(), m_textureSlot.Get(),
		m_useAlpha ? sm_StandardSpriteBlendStateHandle : grcStateBlock::BS_Default));
}

void CUIWorldIconQuad::SetTexture(const char* pTextName, const char* pTextDictionary)
{
	bool shouldLoad = true;

	if(m_isTextureSet)
	{
		if(stricmp(m_pTextureName, pTextName) != 0 ||
			stricmp(m_pTextureDictionary, pTextDictionary) != 0 ||
			!uiVerifyf(CStreaming::HasObjectLoaded(m_textureSlot, g_TxdStore.GetStreamingModuleId()), "CUIWorldIconQuad::SetTexture - For some reason the texture isn't loaded, forcing it to be loaded again. refCount=%d", g_TxdStore.GetNumRefs(m_textureSlot))
			)
		{
			g_TxdStore.RemoveRef(m_textureSlot, REF_OTHER);
			m_isTextureSet = false;

			m_textureSlot = -1;
		}
		else
		{
			shouldLoad = false;
		}
	}
	else
	{
		m_textureSlot = -1;
	}

	m_pTextureName = pTextName;
	m_pTextureDictionary = pTextDictionary;

	if(shouldLoad && uiVerify(m_pTextureDictionary))
	{
		m_textureSlot = g_TxdStore.FindSlot(m_pTextureDictionary);
		LoadTexure();
	}
}

void CUIWorldIconQuad::LoadTexure()
{
	if(!uiVerify(m_textureSlot.Get() >= 0) && uiVerify(m_pTextureDictionary))
	{
		m_textureSlot = g_TxdStore.FindSlot(m_pTextureDictionary);
	}

	if(uiVerify(m_pTextureName) && m_textureSlot.Get() >= 0)
	{
		if(CStreaming::HasObjectLoaded(m_textureSlot, g_TxdStore.GetStreamingModuleId()))
		{
			g_TxdStore.PushCurrentTxd();
			g_TxdStore.SetCurrentTxd(m_textureSlot);
			m_sprite.SetTexture(m_pTextureName);
			g_TxdStore.PopCurrentTxd();

			g_TxdStore.AddRef(m_textureSlot, REF_OTHER);
			m_isTextureSet = true;
		}
		else
		{
			CStreaming::RequestObject(m_textureSlot, g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
		}
	}
}

void CUIWorldIconQuad::InitCommon()
{
	if(!sm_initializedCommon)
	{
		grcBlendStateDesc textQuadBlendState;

		textQuadBlendState.BlendRTDesc[0].BlendEnable = true;
		textQuadBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		textQuadBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		textQuadBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		textQuadBlendState.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
		textQuadBlendState.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		textQuadBlendState.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		textQuadBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;

		sm_StandardSpriteBlendStateHandle = grcStateBlock::CreateBlendState(textQuadBlendState);
	}
}

//eof




