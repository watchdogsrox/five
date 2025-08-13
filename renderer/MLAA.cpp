//=============================================================================================
// renderer/MLAA.cpp
// 
// Copyright (C) 2013 Rockstar Games.  All Rights Reserved. 
// 
// Morphological Anti-Aliasing. This follows the algorithm given in:
// "Practical Morphological Anti-Aliasing" by Jimenez et al in GPU Pro II
//
//=============================================================================================

//=============================================================================================
// Includes
//=============================================================================================
#include "rendertargets.h"

// Rage
#include "profile/profiler.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/quads.h" 
#include "system/param.h" 
#include "grprofile/pix.h"
#include "file/asset.h"

// Game
#include "MLAA.h"

RENDER_OPTIMISATIONS()

#if ENABLE_MLAA

//=============================================================================================
// Statics & globals
//=============================================================================================
bool MLAA::m_bEnable = false; // Toggle MLAA on/off

float MLAA::m_fLuminanceThreshold = 0.1f; // Luminance threshold used by edge detection
bool MLAA::m_bDepthEdgeDetection = false;

float MLAA::m_fLuminanceThresholdBase = 0.056f;  //0.035f;
float MLAA::m_fLuminanceThresholdScale = 0.278f; //0.15f;

bool MLAA::m_bVisualizeEdges = false;
bool MLAA::m_bUseHiStencil = true;

#if __BANK
bool MLAA::m_bOverrideFilterKernel = false; // Swap in the FXAA (or whatever) and still use MLAA's edge detection
MLAA::MLAAFilterOverrideCB MLAA::m_pFilterOverrideCB = NULL;
#endif// __BANK

PARAM(mlaa, "MLAA mode");

//=============================================================================================
// Constructor
//=============================================================================================
MLAA::MLAA()
:	m_pShader(NULL),
	m_EdgeDetectionTechnique(grcetNONE),
	m_DepthEdgeDetectionTechnique(grcetNONE),
	m_BlendWeightCalculationTechnique(grcetNONE),
	m_AntiAliasingTechnique(grcetNONE),
	m_NonAliasedCopyTechnique(grcetNONE),
	m_TexelSizeParam(grcevNONE),
	m_LuminanceThresholdParam(grcevNONE),
	m_RelativeLuminanceThresholdParams(grcevNONE),
	m_PointSamplerParam(grcevNONE),
	m_LinearSamplerParam(grcevNONE),
	m_LUTSamplerParam(grcevNONE),
	m_pAreaLUTTexture(NULL),
	m_pScratchRenderTarget(NULL)
{
}

//=============================================================================================
// Destructor
//=============================================================================================
MLAA::~MLAA()
{
}

//=============================================================================================
// Initialize MLAA
//=============================================================================================
bool MLAA::Initialize(grcRenderTarget* poScratchTarget)
{
	RAGE_TRACK(grPostFx);
	RAGE_TRACK(grPostFxRenderTargets);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	ASSET.PushFolder("common:/shaders");

	// Load the MLAA shader
	m_pShader = grmShaderFactory::GetInstance().Create();

	if ( !m_pShader->Load("MLAA") )
	{
		AssertMsg( m_pShader, "Unable to load the MLAA shader!" );
		m_bEnable = false;
		return false;
	}

	ASSET.PopFolder();

	// Find the various MLAA techniques
	m_EdgeDetectionTechnique = m_pShader->LookupTechnique("EdgeDetection", true);
	m_DepthEdgeDetectionTechnique = m_pShader->LookupTechnique("DepthEdgeDetection", true);
	m_BlendWeightCalculationTechnique = m_pShader->LookupTechnique("BlendWeightCalculation", true);
	m_AntiAliasingTechnique = m_pShader->LookupTechnique("AntiAliasing", true);
	m_NonAliasedCopyTechnique = m_pShader->LookupTechnique("NonAliasedCopy", true);

	// Get handles to the shader constants
	m_TexelSizeParam = m_pShader->LookupVar( "g_vTexelSize", true );
	m_LuminanceThresholdParam = m_pShader->LookupVar( "g_fLuminanceThreshold", true );
	m_RelativeLuminanceThresholdParams = m_pShader->LookupVar( "g_vRelativeLuminanceThresholdParams", true );
	m_PointSamplerParam = m_pShader->LookupVar( "MLAAPointSampler", true );
	m_LinearSamplerParam = m_pShader->LookupVar( "MLAALinearSampler", true );
	m_LUTSamplerParam = m_pShader->LookupVar( "MLAAAreaLUTSampler", true );

	// Depth/Stencil state for creating stencil mask
	{
		grcDepthStencilStateDesc SDDesc;
		SDDesc.DepthEnable = false;
		SDDesc.DepthWriteMask = false;
		SDDesc.DepthFunc = grcRSV::CMP_LESS; // CMP_ALWAYS;//DEFAULT
		SDDesc.StencilEnable = true;
		SDDesc.StencilReadMask = 0x00;
		SDDesc.StencilWriteMask = 0x01;
		SDDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		SDDesc.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
		SDDesc.BackFace = SDDesc.FrontFace;

		m_CreateStencilMask_DS = grcStateBlock::CreateDepthStencilState(SDDesc);
	}

	// Depth/Stencil state for using/not using stencil mask
	{
		grcDepthStencilStateDesc SDDesc;
		SDDesc.DepthEnable = false;
		SDDesc.DepthWriteMask = false;
		SDDesc.DepthFunc = grcRSV::CMP_LESS;//DEFAULT
		SDDesc.StencilEnable = true;
		SDDesc.StencilReadMask = 0x1;
		SDDesc.StencilWriteMask = 0x00;
		SDDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		SDDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		SDDesc.BackFace = SDDesc.FrontFace;

		m_UseStencilMask_DS = grcStateBlock::CreateDepthStencilState(SDDesc);

		SDDesc.DepthFunc = grcRSV::CMP_ALWAYS;
		SDDesc.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
		SDDesc.BackFace = SDDesc.FrontFace;
		m_DoNotUseStencilMask_DS  = grcStateBlock::CreateDepthStencilState(SDDesc);
	}

	// Make the look up tables
	if ( !CreateTextures(poScratchTarget) )
	{
		AssertMsg( 0, "Unable to create MLAA look-up tables!" );
		m_bEnable = false;
		return false;
	}

	u32 uMLAA = 0;
	if (PARAM_mlaa.Get(uMLAA))
	{
		m_bEnable = (uMLAA) ? true : false;
	}

	return true;
}

//=============================================================================================
// Shutdown MLAA
//=============================================================================================
void MLAA::Shutdown(bool releaseScratchTarget)
{
	if ( m_pShader )
	{
		delete m_pShader;
		m_pShader = NULL;
	}

	if ( m_pScratchRenderTarget && (__XENON || releaseScratchTarget) )
	{
		m_pScratchRenderTarget->Release();
	}
	m_pScratchRenderTarget = NULL;
	
	if ( m_pAreaLUTTexture )
	{
		m_pAreaLUTTexture->Release();
		m_pAreaLUTTexture = NULL;
	}
}

//=============================================================================================
// Anti-Alias a render target. 
//=============================================================================================
void MLAA::AntiAlias( grcRenderTarget* pDstColor, grcRenderTarget* pDstDepthReadOnly, const grcTexture* pSrcColor, const grcRenderTarget* pSrcDepthStencil )
{
	// Early out if MLAA is disabled
	if ( !m_bEnable )
	{
		return;
	}

	Assert( pDstColor != m_pScratchRenderTarget );
	Assert( pSrcColor && pDstDepthReadOnly && pSrcDepthStencil && m_pScratchRenderTarget && m_pAreaLUTTexture );

	const float fBufferWidth = static_cast<float>(pSrcColor->GetWidth());
	const float fBufferHeight = static_cast<float>(pSrcColor->GetHeight());
	const Vector4 vTexelSize( 1.f / fBufferWidth, 1.f / fBufferHeight, fBufferWidth, fBufferHeight );

	// Set the parameters
	m_pShader->SetVar( m_TexelSizeParam, vTexelSize );
	m_pShader->SetVar( m_LuminanceThresholdParam, m_fLuminanceThreshold );
	m_pShader->SetVar( m_RelativeLuminanceThresholdParams, Vector4(m_fLuminanceThresholdBase,m_fLuminanceThresholdScale,0,0));
	m_pShader->SetVar( m_LUTSamplerParam, m_pAreaLUTTexture );

	// Resolve color
	//pColor->Realize();

	if ( m_bDepthEdgeDetection )
	{
		// Resolve depth
		//pDepthStencil->Realize();  // Hack: Commenting out resolve so that that we get old depth data from before the transparency/forward pass. Gives better results.
	}

	Color32 color(1.0f,1.0f,1.0f,1.0f);
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstColor, pDstDepthReadOnly); 
	GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 0, true, 0);

#if __XENON
	// Create the stencil mask
	if ( m_bUseHiStencil && !m_bVisualizeEdges )
	{
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, false );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, false );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, 0x1 );
	}
#endif // __XENON
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_CreateStencilMask_DS, 0x1);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);


	// Edge detection pass
	m_pShader->SetVar( m_PointSamplerParam, m_bDepthEdgeDetection ? pSrcDepthStencil : pSrcColor );
	MLAA::DrawScreenQuad( /*m_bVisualizeEdges ? pSrcColor :*/ m_pScratchRenderTarget, 
		pDstDepthReadOnly, 
		m_pShader, 
		m_bDepthEdgeDetection ? m_DepthEdgeDetectionTechnique : m_EdgeDetectionTechnique );
#if __XENON
	GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_ALLFRAGMENTS, NULL, static_cast<D3DTexture*>((m_bVisualizeEdges ? pColor : m_pScratchRenderTarget)->GetTexturePtr()), NULL, 0, 0, NULL, 0.f, 0, NULL );
#endif // __XENON
	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);	

#if __XENON
	if ( m_bUseHiStencil && !m_bVisualizeEdges )
	{
		GBuffer::BeginUseHiStencil(0x1,true,false);
	}
#endif // __XENON
	if ( m_bVisualizeEdges )
	{
		return;
	}

	// Use the stencil mask
	grcTextureFactory::GetInstance().LockRenderTarget( 0, /*pSrcColor*/ m_pScratchRenderTarget, pDstDepthReadOnly ); 
	GRCDEVICE.Clear( true, Color32(0,0,0,0), false, 0, 0x0 ); // Debug only
#if __XENON
	if ( m_bUseHiStencil )
	{
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, TRUE );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_EQUAL );
	}
#endif // __XENON

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_UseStencilMask_DS, 0x1);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

#if __BANK
	if ( !m_bOverrideFilterKernel )
#endif // __BANK
	{
		// Blend weight calculation pass
		m_pShader->SetVar( m_PointSamplerParam, pDstColor); // m_pScratchRenderTarget );
		m_pShader->SetVar( m_LinearSamplerParam, pDstColor); // m_pScratchRenderTarget );
		MLAA::DrawScreenQuad( m_pScratchRenderTarget, pDstDepthReadOnly, m_pShader, m_BlendWeightCalculationTechnique );
#if __XENON
		GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_ALLFRAGMENTS, NULL, static_cast<D3DTexture*>(m_pScratchRenderTarget->GetTexturePtr()), NULL, 0, 0, NULL, 0.f, 0, NULL );
#endif // __XENON
	}

	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);


	// TODO: Fix this! The previous passes have stomped on the frame buffer in EDRAM so we have to reload the frame as the next
	// pass (applying the anti-aliasing filter) uses stencil to only write to edge pixels and we'll need the non-edge pixels to
	// already be in EDRAM. We need to find a way to make it all fit so we can avoid the reload...
#if __XENON
	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetUIBackBuffer(), NULL); 
	CSprite2d copyBuffer;
	copyBuffer.BeginCustomList(CSprite2d::CS_COPY_GBUFFER, pColor);

	grcBeginQuads(1);
	grcDrawQuadf(0,0,1,1, 0, 0.0f,1.0f,1.0f,0.0f, Color32(255,255,255,255));
	grcEndQuads();
	copyBuffer.EndCustomList();

	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
#endif // __XENON

	// Anti-aliasing pass
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstColor, pDstDepthReadOnly);

#if __XENON
	if ( m_bUseHiStencil )
	{
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, TRUE );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_EQUAL );
	}
#endif // __XENON

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_UseStencilMask_DS, 0x1);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

#if __BANK
	if ( m_bOverrideFilterKernel )
	{
		Assert(NULL != m_pFilterOverrideCB);
		m_pFilterOverrideCB(pSrcColor);
	}
	else
#endif // __BANK
	{
#if !__XENON // Need to copy in the non-edge data
		grcStateBlock::SetDepthStencilState(m_DoNotUseStencilMask_DS, 0x1);
		m_pShader->SetVar( m_PointSamplerParam, pSrcColor );
		MLAA::DrawScreenQuad( pDstColor, pDstDepthReadOnly, m_pShader, m_NonAliasedCopyTechnique );
#endif // !__XENON

		grcStateBlock::SetDepthStencilState(m_UseStencilMask_DS, 0x1);
		m_pShader->SetVar( m_PointSamplerParam, m_pScratchRenderTarget );
		m_pShader->SetVar( m_LinearSamplerParam, pSrcColor );

		MLAA::DrawScreenQuad( pDstColor, pDstDepthReadOnly, m_pShader, m_AntiAliasingTechnique );
	}

	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
#if __XENON
	if ( m_bUseHiStencil )
	{
		GBuffer::EndUseHiStencil(0x1);
		//GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
	}
#endif // __XENON
}

//=============================================================================================
// Create the textures
//=============================================================================================
bool MLAA::CreateTextures(grcRenderTarget* poScratchTarget)
{
	static const s32 AREA_LUT_SIZE = 32; // If you change this, you must also change MLAA_NUM_DISTANCES in the mlaa.fx shader

	// Table generation taken from "Practical Morphological Anti-Aliasing" by Jimenez et al in GPU Pro II

	double AreaLUTTmp[AREA_LUT_SIZE*2][AREA_LUT_SIZE*2];
	for ( s32 i=0; i<AREA_LUT_SIZE*2; i++ )
	{
		for ( s32 j=0; j<AREA_LUT_SIZE*2; j++ )
		{
			AreaLUTTmp[i][j] = 0.0;
		}
	}

	for ( s32 i=0; i<(AREA_LUT_SIZE*2); i++ )
	{
		double fLeft = 0.5;
		for ( s32 j=0; j<i; j++ )
		{
			double fX = i / 2.0;
			double fRight = 0.5 * (fX - (double)j - 1.) / fX;

			double fA = 0.5 * rage::Abs(fLeft / 2. );
			if ( ( Sign(fLeft) == Sign(fRight) ) || ( rage::Abs(fLeft) != rage::Abs(fRight) ) )
			{
				fA = fabs((fLeft + fRight) / 2.);
			}

			fLeft = fRight;
			AreaLUTTmp[i][j] = fA;
		}
	}

	u8 AreaLUT[AREA_LUT_SIZE][AREA_LUT_SIZE];
	for (s32 nLeft = 0; nLeft < AREA_LUT_SIZE; nLeft++)
	{
		for (s32 nRight = 0; nRight < AREA_LUT_SIZE; nRight++)
		{
			int nX = nLeft + nRight + 1;
			AreaLUT[nLeft][nRight] = static_cast<u8>(AreaLUTTmp[nX][nLeft] * 255.);
		}
	}

	// Use a 16bit Alpha + Luminance texture for the LUT. If you change this, also change MLAA_USE_A8L8_FOR_LUT in the mlaa.fx shader
#define USE_A8L8 !RSG_ORBIS

	// Make the 4D area LUT 
	grcImage* pAreaLUTImage = grcImage::Create(AREA_LUT_SIZE*5, AREA_LUT_SIZE*5, 1, USE_A8L8 ? grcImage::A8L8 : grcImage::A8R8G8B8, grcImage::STANDARD, 0, 0);
	if ( NULL == pAreaLUTImage )
	{
		AssertMsg( 0, "Unable to create MLAA look-up tables!" );
		return false;
	}


#if USE_A8L8
#define COLOR_SWIZZLE(r,g,b,a) Color32(g,g,g,r)
#else
#define COLOR_SWIZZLE(r,g,b,a) Color32(r,g,0,0)
#endif

	for ( s32 nE2=0; nE2<5; nE2++ )
	{
		for ( s32 nE1=0; nE1<5; nE1++ )
		{
			for ( s32 nLeft=0; nLeft<AREA_LUT_SIZE; nLeft++ )
			{
				for ( s32 nRight=0; nRight<AREA_LUT_SIZE; nRight++ )
				{
					u8 nArea = AreaLUT[nLeft][nRight];

					s32 p[2];
					p[0] = nLeft  + nE1 * AREA_LUT_SIZE;
					p[1] = nRight + nE2 * AREA_LUT_SIZE;

					if ((nE1 == 2) || (nE2 == 2))
					{
						pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,0,0,0).GetColor());
					}
					else if (nLeft > nRight)
					{
						if (nE2 == 0)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,0,0,0).GetColor());
						else if (nE2 == 1)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,nArea,0,0).GetColor());
						else if (nE2 == 3)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,0,0,0).GetColor());
						else
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,nArea,0,0).GetColor());
					}
					else if (nLeft < nRight)
					{
						if (nE1 == 0)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,0,0,0).GetColor());
						else if (nE1 == 1)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,nArea,0,0).GetColor());
						else if (nE1 == 3)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,0,0,0).GetColor());
						else
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,nArea,0,0).GetColor());
					}
					else
					{
						if ((nE1+nE2) == 0)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,0,0,0).GetColor());
						else if ((nE1+nE2) == 1)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,nArea,0,0).GetColor());
						else if ((nE1+nE2) == 2)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(0,2*nArea,0,0).GetColor());
						else if ((nE1+nE2) == 3)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,0,0,0).GetColor());
						else if ((nE1+nE2) == 4)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,nArea,0,0).GetColor());
						else if ((nE1+nE2) == 5)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(nArea,2*nArea,0,0).GetColor());
						else if ((nE1+nE2) == 6)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(2*nArea,0,0,0).GetColor());
						else if ((nE1+nE2) == 7)
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(2*nArea,nArea,0,0).GetColor());
						else
							pAreaLUTImage->SetPixel(p[0], p[1], COLOR_SWIZZLE(2*nArea,2*nArea,0,0).GetColor());
					}
				}
			}
		}
	}

#undef USE_A8L8
#undef COLOR_SWIZZLE

	grcTextureFactory::TextureCreateParams textureParams(grcTextureFactory::TextureCreateParams::VIDEO, grcTextureFactory::TextureCreateParams::TILED);

	// This fails silently if the stride is not a multiple of 256 bytes!
	BANK_ONLY(grcTexture::SetCustomLoadName("AreaLUTImage");)
	m_pAreaLUTTexture = rage::grcTextureFactory::GetInstance().Create(pAreaLUTImage, &textureParams);
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)

	LastSafeRelease(pAreaLUTImage);

	if ( NULL == m_pAreaLUTTexture )
	{
		AssertMsg( 0, "Unable to create MLAA look-up table texture!" );
		return false;
	}
	
	// Make the scratch buffer	
	grcTextureFactory::CreateParams params;
	params.HasParent = true;
	params.Parent = CRenderTargets::GetDepthBuffer();
	params.IsRenderable = true;
	params.Format = grctfA8R8G8B8;
	params.IsResolvable = true;
#if __XENON
	const int nScreenWidth=GRCDEVICE.GetWidth(), nScreenHeight=GRCDEVICE.GetHeight();
	m_pScratchRenderTarget = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER1, "MLAA_Scratch_Buffer", grcrtPermanent, nScreenWidth, nScreenHeight, 32, &params, kMLAAScratchBuffer);
	m_pScratchRenderTarget->AllocateMemoryFromPool();
#else
	if (poScratchTarget)
	{
		m_pScratchRenderTarget = poScratchTarget;
	#if !RSG_ORBIS
		Assertf((m_pScratchRenderTarget->GetImageFormat() == grcImage::A8R8G8B8), "Expecting Format %d but %d in target format", grcImage::A8R8G8B8, m_pScratchRenderTarget->GetImageFormat());
	#endif // !RSG_ORBIS
		Assert(m_pScratchRenderTarget->GetWidth() == GRCDEVICE.GetWidth());
		Assert(m_pScratchRenderTarget->GetHeight() == GRCDEVICE.GetHeight());
	}else
	{
		m_pScratchRenderTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "MLAA Scratch Target", grcrtPermanent, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 32,
			&params WIN32PC_ONLY(, 0, m_pScratchRenderTarget));
	}
#endif //__XENON
	
	return true;
}

//=============================================================================================
// Draw a screen aligned quad with the specified technique
//=============================================================================================
bool MLAA::DrawScreenQuad ( grcRenderTarget* XENON_ONLY(pColorTarget), grcRenderTarget* /*pDepthStencilTarget*/, grmShader* pShader, grcEffectTechnique technique )
{
	// Offset to align texels with pixels
	float fOffsetX = 0.f;
	float fOffsetY = 0.f;

#if __XENON
	fOffsetX = 0.5f/pColorTarget->GetWidth();
	fOffsetY = 0.5f/pColorTarget->GetHeight();
#elif (RSG_PC && !__D3D11)
	//if (GRCDEVICE.GetDxVersion() < 1000)
	{
		fOffsetX = 0.5f/pColorTarget->GetWidth();
		fOffsetY = 0.5f/pColorTarget->GetHeight();
	}
#endif

	if ( pShader->BeginDraw( grmShader::RMC_DRAW, true, technique ) )
	{
		pShader->Bind();

		grcBeginQuads(1);
		grcDrawQuadf( -1.f, 1.f, 1.f, -1.f, 0.f, fOffsetX, fOffsetY, 1.f+fOffsetX, 1.f+fOffsetY, Color32(1.0f,1.0f,1.0f,1.0f) );
		grcEndQuads();

		pShader->UnBind();
	}
	pShader->EndDraw();

	return true;
}

#if __BANK
//=============================================================================================
// Setup BANK widgets
//=============================================================================================
void MLAA::AddWidgets ( bkBank &bk )
{
	bk.PushGroup("MLAA", false);

	bk.AddToggle("Enable MLAA", &m_bEnable);
	bk.AddToggle("Use FXAA's Kernel", &m_bOverrideFilterKernel);
	bk.AddToggle("Depth-based Edge Detection", &m_bDepthEdgeDetection );
	bk.AddSlider("Luminance threshold", &m_fLuminanceThreshold, 0, 1.f, 1.f/255.f );

	bk.AddSlider("Relative Luminance Base", &m_fLuminanceThresholdBase, 0, 1.f, 0.001f );
	bk.AddSlider("Relative Luminance Scale", &m_fLuminanceThresholdScale, 0, 1.f, 0.001f );

	bk.AddToggle("Visualize Edges", &m_bVisualizeEdges );
	bk.AddToggle("Use HiStencil", &m_bUseHiStencil );

	bk.PopGroup();
}

#endif // __BANK
#endif // ENABLE_MLAA
































