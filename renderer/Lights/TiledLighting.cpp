// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "math/Float16.h"
#include "grprofile/pix.h"
#include "grcore/vertexbuffer.h"
#include "grcore/texturedefault.h"
#include "grmodel/shader.h"
#include "system/memory.h"
#include "system/xtl.h"

// framework headers
#include "fwmaths/vectorutil.h"

// game headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Debug/Rendering/DebugLights.h"
#include "Debug/Rendering/DebugDeferred.h"
#include "Renderer/rendertargets.h"
#include "Renderer/Deferred/DeferredLighting.h"
#include "Renderer/Lights/lights.h"
#include "Renderer/Lights/LightSource.h"
#include "Renderer/Util/Util.h"
#include "Renderer/Util/ShaderUtil.h"
#include "Renderer/Lights/TiledLighting.h"
#include "Renderer/SSAO.h"
#include "Renderer/render_channel.h"

// ----------------------------------------------------------------------------------------------- //
#if TILED_LIGHTING_ENABLED
// ----------------------------------------------------------------------------------------------- //

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

grcEffectVar CTiledLighting::m_shaderVars[TL_NUM_SHADER_VARS] = {};
grmShader* CTiledLighting::m_tiledShader = NULL;
#if GENERATE_SHMOO
int CTiledLighting::m_tiledShaderShmooIdx;
#endif // GENERATE_SHMOO
grcEffectTechnique CTiledLighting::m_techniques[TL_NUM_TECHNIQUES] = {grcetNONE};

bool CTiledLighting::m_classificationEnabled = true;

grcDepthStencilStateHandle CTiledLighting::m_depthDownSample_DS;
grcRasterizerStateHandle CTiledLighting::m_depthDownSample_RS;

grcRenderTarget* CTiledLighting::m_classificationTex =  NULL;

grcVertexBuffer* CTiledLighting::m_tileVertBuf = NULL;
#if TILED_LIGHTING_INSTANCED_TILES
grcVertexBuffer* CTiledLighting::m_tileInstanceBuf = NULL;
#endif //TILED_LIGHTING_INSTANCED_TILES
grcVertexDeclaration* CTiledLighting::m_vtxDecl = NULL; 

Float16* CTiledLighting::m_rawTileData = NULL;
u32 CTiledLighting::m_tileSize = 16;
grcVertexBuffer* CTiledLighting::m_tileInfoVertBuf = NULL;

MinimumDepthTexture::MinimumDepthTexture( const grcTexture* classificationTexture)
{
	grcTextureFactory::CreateParams qparams;
	qparams.Format						= grctfR32F; // saves conversion from half to float
	qparams.Multisample					= 0;
	qparams.UseFloat					= true;
	qparams.HasParent 					= true; 
	qparams.Parent 						= NULL;
	qparams.MipLevels					= 1;

	m_srcWidth	= (float)classificationTexture->GetWidth();
	m_srcHeight	= (float)classificationTexture->GetHeight();

	m_width = (u32)m_srcWidth / 4;
	m_height = (u32)m_srcHeight / 2;

#if __D3D11
	m_UpdateNearZ = false;
	m_ComputeNearZ= false;
	//On D3D11 we need to make a texture and pass it to the rendertarget so we can lock it and read back the data.
	grcTextureFactory::TextureCreateParams TextureParams(
		grcTextureFactory::TextureCreateParams::SYSTEM, 
		grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead|grcsWrite, NULL, 
		grcTextureFactory::TextureCreateParams::RENDERTARGET, 
		grcTextureFactory::TextureCreateParams::MSAA_NONE);

#if RSG_PC
	for(int i = 0; i < MAX_MIN_TARGETS; ++i)
	{
		m_rtTex[i]	= grcTextureFactory::GetInstance().Create( m_width, m_height, qparams.Format, NULL, 1, &TextureParams );
		m_rt[i]		= grcTextureFactory::GetInstance().CreateRenderTarget("minimum Depth Texture", m_rtTex[i]->GetTexturePtr());		
	}

	m_CurrentRTIndex = 0;
#else
	m_rtTex	= grcTextureFactory::GetInstance().Create( m_width, m_height, qparams.Format, NULL, 1, &TextureParams );
	m_rt = grcTextureFactory::GetInstance().CreateRenderTarget("minimum Depth Texture", m_rtTex->GetTexturePtr());		
#endif

	m_NearZ = -1.0f;
	m_AverageZ = 1.0f;
	m_CenterZ = 1.0f;
#else	// else __D3D11

	qparams.ForceNoTiling = true;

	m_rt = CRenderTargets::CreateRenderTarget(
		RTMEMPOOL_NONE,
		"minimum Depth Texture",
		grcrtPermanent,
		m_width, m_height,
		32, 
		&qparams,
		0);

	m_lock.Base=NULL;
#endif
}

MinimumDepthTexture::~MinimumDepthTexture()
{
#if RSG_PC
	for(int i = 0; i < MAX_MIN_TARGETS; ++i)
	{
		delete m_rt[i];
		m_rt[i] = NULL;
	}
#else
	delete m_rt;
#endif
}

void MinimumDepthTexture::Generate( grmShader* tiledShader, grcTexture* parentTexture, 
		grcEffectVar srcTextureSizeVar, grcEffectVar dstTextureSizeVar, grcEffectVar srcTextureVarP,grcEffectVar srcTextureVar,
		grcEffectTechnique minDepthTech 
#if GENERATE_SHMOO
		, int shmooIdx
#endif // GENERATE_SHMOO
		)
{
	PF_PUSH_MARKER("Calculate Minimum Depth");

	tiledShader->SetVar(srcTextureSizeVar, Vector4(m_srcWidth, m_srcHeight, 1.0f / m_srcWidth, 1.0f / m_srcHeight));
	tiledShader->SetVar(dstTextureSizeVar, Vector4((float)m_width, (float)m_height, 1.0f / (float)m_width, 1.0f / (float)m_height));

	tiledShader->SetVar(srcTextureVarP, parentTexture);
	tiledShader->SetVar(srcTextureVar,  parentTexture);
	
#if __D3D11
	if(m_ComputeNearZ)
	{
		CalculateNearZRenderThread();
	}
#endif

	grcRenderTarget* currentMinDepthRT = NULL;

#if RSG_PC
	currentMinDepthRT = m_rt[m_CurrentRTIndex];
#else
	currentMinDepthRT = m_rt;
#endif

	grcTextureFactory::GetInstance().LockRenderTarget(0, currentMinDepthRT, NULL);

	if (tiledShader->BeginDraw(grmShader::RMC_DRAW, true, minDepthTech))
	{
		GENSHMOO_ONLY(ShmooHandling::BeginShmoo(shmooIdx,tiledShader,0));
		tiledShader->Bind(0);
		grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
		GENSHMOO_ONLY(ShmooHandling::EndShmoo(shmooIdx));
		tiledShader->UnBind();
	}
	tiledShader->EndDraw();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if __D3D11
	if(m_UpdateNearZ)
	{
		grcTextureD3D11* minDepthTex11 = NULL;
		
#if RSG_PC
		minDepthTex11 = (grcTextureD3D11*)m_rtTex[m_CurrentRTIndex];
#else
		minDepthTex11 = (grcTextureD3D11*)m_rtTex;
#endif
		minDepthTex11->CopyFromGPUToStagingBuffer();		

		m_UpdateNearZ = false;
		m_ComputeNearZ = true;

#if RSG_PC
		m_CurrentRTIndex = (m_CurrentRTIndex + 1) % NUM_MIN_TARGETS;
#endif
	}
#endif

	PF_POP_MARKER();
}


#if __D3D11
void MinimumDepthTexture::CalculateNearZRenderThread()
{
	if(m_ComputeNearZ)
	{
		grcTextureLock lock;
		//Needs to be on render thread.
		grcTextureD3D11* minDepthTex11 = NULL;
	
	#if RSG_PC
		u32 prevIndex = ((CurExportRTIndex - 1) + NUM_MIN_TARGETS) % NUM_MIN_TARGETS;
		minDepthTex11 = (grcTextureD3D11*)m_rtTex[prevIndex];
	#else
		minDepthTex11 = (grcTextureD3D11*)m_rtTex;
	#endif

		if(minDepthTex11->MapStagingBufferToBackingStore(true))
		{
			if (minDepthTex11->LockRect( 0, 0, lock, grcsRead ))
			{
				CalculateNearZValue(lock, m_ShadowFarClip, m_NearZ, m_AverageZ, m_CenterZ);
				minDepthTex11->UnlockRect(lock);
			}
		}
		m_ComputeNearZ = false;
	}
}
#endif // __D3D11


void MinimumDepthTexture::CalculateNearZ(float shadowFarClip, float& minDepth, float& avgDepth, float& centerDepth)
{
#if __D3D11
	m_ShadowFarClip = shadowFarClip;
	m_UpdateNearZ = true;
	minDepth = m_NearZ;
	avgDepth = m_AverageZ;
	centerDepth = m_CenterZ;
#else

	grcTextureLock lock;
	bool bLocked = true;

	if (!m_lock.Base)
		bLocked = m_rt->LockRect(0,0,lock, grcsRead);
	else
		lock = m_lock;

	if (bLocked)
	{
		CalculateNearZValue(lock, shadowFarClip, minDepth, avgDepth, centerDepth);

		if (!m_lock.Base)
			m_rt->UnlockRect(lock);
	}
#endif //__D3D11
}

void MinimumDepthTexture::CalculateNearZValue(const grcTextureLock &lock, float shadowFarClip, float& minDepth, float& avgDepth, float& centerDepth) const
{
	float minVal = FLT_MAX;
	float* vals = (float*)lock.Base;  

	int height = m_height - 2;
	int width  = m_width  - 2;
	float sum = 0.0f;
	int values = 0;
	float centerSum = 0.0f;
	int centerCount = 0;
	
	centerDepth = shadowFarClip;

	for (int y = 2 ; y < height; y++)
	{
		for (int x = 2; x < width; x++)
		{
			float val = vals[x + (lock.Pitch >> 2) * y];
			if(val < shadowFarClip)
			{
				sum += val;
				values++;
			}
			minVal = Min(minVal, val);
		}
	}

	// compute approx. in-focus depth 5x5 samples
	float* ptr = &vals[((width/2)-2) + (lock.Pitch>>2) * ((height/2)-2)];

	for (int y = 0; y < 5; ++y)
	{
		float* current = ptr;
		for(int x = 0; x < 5; ++x)
		{
			if(*current < shadowFarClip)
			{
				centerSum += *current;
				centerCount++;
			}
			current++;
		}
		ptr += (lock.Pitch>>2);
	}

	minDepth = minVal;
	if(values)
		avgDepth = sum/values;
	else
		avgDepth = shadowFarClip;
	if(centerCount)
	{
		centerDepth = centerSum/centerCount;
		//	modulate with min depth to approx. a better in-focus of nearest objects (hacky)
		centerDepth = (centerDepth+minDepth)/2.0f;
	}
	else
	{
		centerDepth = minDepth;
	}
}


MinimumDepthTexture* g_minimumDepthTexture = NULL;

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void CalculateFactors(atBitSet& factors, u32 N)
{
	u32 i = 2;

	while(i*i < N)
	{
		if(N % i == 0) 
		{
			factors.Set(i);
			factors.Set(N/i);
		}
		i++;
	}
	if(i*i == N)
		factors.Set(i);
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::Init()
{
#if RSG_PC
	if (!GetPlatformSupport())
	{
		m_classificationEnabled = false;
		return;
	}
#endif // RSG_PC

	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// Work out tile size for any resolution
	int bitSetSize = rage::Max(VideoResManager::GetSceneWidth(), VideoResManager::GetSceneHeight());
	atBitSet widthFactors(bitSetSize);
	atBitSet heightFactors(bitSetSize);
	atBitSet commonFactors(bitSetSize);

	CalculateFactors(widthFactors, VideoResManager::GetSceneWidth());
	CalculateFactors(heightFactors, VideoResManager::GetSceneHeight());

	commonFactors.Intersect(widthFactors, heightFactors);

	SetupShader();
	
#if __XENON || __PS3 || __D3D11 || RSG_ORBIS
	SetupTileData();
#endif

	// State blocks
	grcDepthStencilStateDesc depthDownSampleDepthState;
	depthDownSampleDepthState.DepthEnable		= TRUE;
	depthDownSampleDepthState.DepthWriteMask	= TRUE;
	depthDownSampleDepthState.DepthFunc			= grcRSV::CMP_ALWAYS;
	m_depthDownSample_DS = grcStateBlock::CreateDepthStencilState(depthDownSampleDepthState);

	grcRasterizerStateDesc rasterizerDownSampleDepthState;
	rasterizerDownSampleDepthState.CullMode = grcRSV::CULL_NONE;
	m_depthDownSample_RS = grcStateBlock::CreateRasterizerState(rasterizerDownSampleDepthState);

	g_minimumDepthTexture = rage_new MinimumDepthTexture(GetClassificationTexture());
}

// ----------------------------------------------------------------------------------------------- //

u32 CTiledLighting::GetNumTilesX()
{
	return ((VideoResManager::GetSceneWidth() + m_tileSize - 1) / m_tileSize);
}

u32 CTiledLighting::GetNumTilesY()
{
	return ((VideoResManager::GetSceneHeight() + m_tileSize - 1) / m_tileSize);
}

u32 CTiledLighting::GetTotalNumTiles()
{
	return GetNumTilesX() * GetNumTilesY();
}

u32 CTiledLighting::GetNumTileComponents()
{
	return NUM_TILE_COMPONENTS;
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::Shutdown() 
{
	SAFE_RELEASE(m_classificationTex);
	SAFE_DELETE(m_tileVertBuf);
	SAFE_DELETE(m_tileInfoVertBuf);
#if TILED_LIGHTING_INSTANCED_TILES
	SAFE_DELETE(m_tileInstanceBuf);
#endif //TILED_LIGHTING_INSTANCED_TILES

	if (m_vtxDecl)
	{
		GRCDEVICE.DestroyVertexDeclaration(m_vtxDecl);
		m_vtxDecl = NULL;
	}

	SAFE_DELETE(m_tiledShader);

	PlatformShutdown();

	SAFE_DELETE(g_minimumDepthTexture);
}

// ----------------------------------------------------------------------------------------------- //
bool CTiledLighting::GetPlatformSupport()
{
#if RSG_PC
	return	(GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50)) && 
			(GRCDEVICE.GetManufacturer() != INTEL) // Perhap it should be Ivy Bridge only - Profile - SettingsDefaults::GetInstance().oAdapterDesc.DeviceId) )
			;
#else
	return true;
#endif
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::SetupShader()
{
#if RSG_PC
	if (!GetPlatformSupport())
	{
		Assertf(m_classificationEnabled == false,"CTiledLighting::SetupShader");
		return;
	}
#endif // RSG_PC

#if RSG_PC
	m_tileSize = GRCDEVICE.GetMSAA() > 4 ? 8 : 16;
#else
	m_tileSize = 20;
#endif
	renderWarningf("Tile size selected for tiled rendering is %d (%d x %d)", 
		m_tileSize,  
		VideoResManager::GetSceneWidth(), 
		VideoResManager::GetSceneHeight());

	m_tiledShader = DeferredLighting::GetShader(DEFERRED_SHADER_TILED);
#if GENERATE_SHMOO
	m_tiledShaderShmooIdx = DeferredLighting::GetShaderShmooIdx(DEFERRED_SHADER_TILED);
#endif // GENERATE_SHMOO

	// Use shader from the deferred renderer
	m_techniques[TL_TECH_DOWNSAMPLE] = m_tiledShader->LookupTechnique("depthDownScale", true);
	m_techniques[TL_TECH_MINDEPTH_DOWNSAMPLE] = m_tiledShader->LookupTechnique("minimumDepthDownScale", true);
		
	m_shaderVars[TL_SRCTEXTURESIZE] = m_tiledShader->LookupVar("srcTextureSize", true);
	m_shaderVars[TL_DSTTEXTURESIZE] = m_tiledShader->LookupVar("dstTextureSize", true);
	m_shaderVars[TL_SCREENRES] = m_tiledShader->LookupVar("screenRes", true);
	m_shaderVars[TL_GBUFFERDEPTH] = m_tiledShader->LookupVar("gbufferTextureDepth", true);
	m_shaderVars[TL_GBUFFER0] = m_tiledShader->LookupVar("gbufferTexture0", true);
	m_shaderVars[TL_GBUFFER1] = m_tiledShader->LookupVar("gbufferTexture1", true);
	m_shaderVars[TL_GBUFFER2] = m_tiledShader->LookupVar("gbufferTexture2", true);
	m_shaderVars[TL_GBUFFER3] = m_tiledShader->LookupVar("gbufferTexture3", true);
	m_shaderVars[TL_TEXTURE0] = m_tiledShader->LookupVar("TiledLightingTexture", true);
	m_shaderVars[TL_DEFERREDTEXTURE1P] = m_tiledShader->LookupVar("deferredLightTexture1P", true);
	m_shaderVars[TL_DEFERREDTEXTURE0] = m_tiledShader->LookupVar("deferredLightTexture", true);
	m_shaderVars[TL_DEFERREDTEXTURE1] = m_tiledShader->LookupVar("deferredLightTexture1", true);
	m_shaderVars[TL_PROJECTIONPARAMS] = m_tiledShader->LookupVar("deferredProjectionParams", true);
	m_shaderVars[TL_SCREENSIZE] = m_tiledShader->LookupVar("deferredLightScreenSize", true);
	m_shaderVars[TL_GBUFFERSTENCIL] = m_tiledShader->LookupVar("gbufferStencilTexture", true);
	m_shaderVars[TL_LIGHTSDATA] = m_tiledShader->LookupVar("lightsData", true);
	m_shaderVars[TL_NUMLIGHTS] = m_tiledShader->LookupVar("numLights", true);

	m_shaderVars[TL_REDUCTION_DEPTH_TEXTURE] = m_tiledShader->LookupVar("reductionDepthTexture", true);
	m_shaderVars[TL_REDUCTION_GBUFFER_TEXTURE] = m_tiledShader->LookupVar("reductionGBufferTexture", true);
	m_shaderVars[TL_REDUCTION_OUTPUT_TEXTURE] = m_tiledShader->LookupVar("ReductionOutputTexture", true);
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::Render()
{
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::Update()
{
}


// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::RenderTiles(	grcRenderTarget* target, grmShader* pShader, grcEffectTechnique technique, u32 pass, 
									const char* RAGE_TIMEBARS_ONLY(pLabelStr), bool clearTarget, bool lockDepth, bool bClearColour)
{

	Assertf(m_classificationEnabled, "Attempting to utilise tiled rendering when classification data is not being updated");

#if RAGE_TIMEBARS
	if (pLabelStr != NULL) 
	{
		PF_PUSH_MARKER(pLabelStr);
	}
	else
	{
		PF_PUSH_MARKER("Tiled Lighting");
	}
#endif // RAGE_TIMEBARS

	Color32 color(0.0f,0.0f,0.0f,1.0f);

	if (target)
	{
		if(lockDepth)
			grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, target);
		else
			grcTextureFactory::GetInstance().LockRenderTarget(0, target, NULL);
	}

	if (clearTarget)
	{
		GRCDEVICE.Clear(true, color, false, 0.0f, false);
	}

	GRCDEVICE.SetVertexDeclaration(m_vtxDecl);
	GRCDEVICE.SetStreamSource(0, *m_tileVertBuf, 0, m_tileVertBuf->GetVertexStride());
	#if TILED_LIGHTING_INSTANCED_TILES
		GRCDEVICE.SetStreamSource(1, *m_tileInstanceBuf, 0, m_tileInstanceBuf->GetVertexStride());
	#else
		GRCDEVICE.SetStreamSource(1, *m_tileInfoVertBuf, 0, m_tileInfoVertBuf->GetVertexStride());
	#endif //TILED_LIGHTING_INSTANCED_TILES

	if (pShader->BeginDraw(grmShader::RMC_DRAW, true, technique))
	{
		pShader->Bind((int)pass); 
		#if TILED_LIGHTING_INSTANCED_TILES
				GRCDEVICE.DrawInstancedPrimitive(drawTris, 6, GetTotalNumTiles(), 0, 0);
		#else
				GRCDEVICE.DrawPrimitive(drawTris,	0, GetTotalNumTiles()*6);
		#endif //TILED_LIGHTING_INSTANCED_TILES
		pShader->UnBind(); 
		pShader->EndDraw();
	}

	GRCDEVICE.ClearStreamSource(1);
	GRCDEVICE.ClearStreamSource(0);

	if (target)
	{
		grcResolveFlags resolveFlags;
		resolveFlags.ClearColor = true;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, (bClearColour ? &resolveFlags : NULL));

	}

	PF_POP_MARKER();

}

void CTiledLighting::GenerateMinimumDepth()
{
	if (g_minimumDepthTexture != NULL)
	{
		g_minimumDepthTexture->Generate(m_tiledShader, m_classificationTex, 
			m_shaderVars[TL_SRCTEXTURESIZE],m_shaderVars[TL_DSTTEXTURESIZE], 
			m_shaderVars[TL_TEXTURE0],m_shaderVars[TL_DEFERREDTEXTURE0],
			m_techniques[TL_TECH_MINDEPTH_DOWNSAMPLE]
			#if GENERATE_SHMOO
			, m_tiledShaderShmooIdx
			#endif // GENERATE_SHMOO
		);
	}
}

void CTiledLighting::CalculateNearZ(float shadowNearClip, float& minDepth, float& avgDepth, float& centerDepth)
{
	if(g_minimumDepthTexture != NULL)
		g_minimumDepthTexture->CalculateNearZ(shadowNearClip, minDepth, avgDepth, centerDepth);
	else
		minDepth = avgDepth = centerDepth = 0.0f;
}

#if __D3D11
void CTiledLighting::CalculateNearZRenderThread()
{
	if (g_minimumDepthTexture != NULL)
	{
		g_minimumDepthTexture->CalculateNearZRenderThread();
	}
}
#endif

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::SetupTileData()
{
	// Textures
	grcTextureFactory::CreateParams qparams;	
	qparams.Format = grctfA16B16G16R16F_NoExpand;
	qparams.UseFloat = true;
	qparams.HasParent = true; 
	qparams.Parent = NULL;
	qparams.MipLevels = 1;
	qparams.Multisample = 0;
	qparams.UseHierZ = false;
	qparams.AllocateFromPoolOnCreate = true;

	qparams.PoolID = kRTPoolIDAuto;
	qparams.PoolHeap = 0;
	qparams.UseAsUAV = true;

#if RSG_PC
	qparams.StereoRTMode = grcDevice::MONO;
#endif

	m_classificationTex = grcTextureFactory::GetInstance().CreateRenderTarget(
		"Classification", 
		grcrtPermanent, 
		GetNumTilesX(), 
		GetNumTilesY(), 
		64, 
		&qparams);
	Assert(m_classificationTex);

	// Tile vertex buffer
	grcFvf fvf0;
	fvf0.SetPosChannel(true, grcFvf::grcdsHalf4);
	bool bReadWrite = true;
	bool bDynamic = false;
	const u32 numVertsInQuad = 6;
	{
		s32 numTilesX = GetNumTilesX();
		s32 numTilesY = GetNumTilesY();

#if TILED_LIGHTING_INSTANCED_TILES
		const float widthMult	= m_tileSize / (float)VideoResManager::GetSceneWidth();
		const float heightMult	= m_tileSize / (float)VideoResManager::GetSceneHeight();

		//Vertex tile buffer
		{
			m_tileVertBuf = grcVertexBuffer::Create(numVertsInQuad, fvf0, bReadWrite, bDynamic, NULL);

			grcVertexBufferEditor tileEditor(m_tileVertBuf);

			//Put widthMult and heightMult in zw components to use in the shader to reconstruct the instance
			// TL, TR, BL | BL, TR, BR
			tileEditor.SetPositionAndBinding(0, Vector4(0.0f, 0.0f, widthMult, heightMult));	// BL
			tileEditor.SetPositionAndBinding(1, Vector4(widthMult, 0.0f, widthMult, heightMult));	// BR
			tileEditor.SetPositionAndBinding(2, Vector4(0.0f, heightMult, widthMult, heightMult));	// TL

			tileEditor.SetPositionAndBinding(3, Vector4(0.0f, heightMult, widthMult, heightMult));	// TL
			tileEditor.SetPositionAndBinding(4, Vector4(widthMult, 0.0f, widthMult, heightMult));	// BR
			tileEditor.SetPositionAndBinding(5, Vector4(widthMult, heightMult, widthMult, heightMult));	// TR
		}

		//Instance buffer
		{
			m_tileInstanceBuf = grcVertexBuffer::Create(GetTotalNumTiles(), fvf0, bReadWrite, bDynamic, NULL);

			grcVertexBufferEditor instanceEditor(m_tileInstanceBuf);

			for (s32 y = 0; y < numTilesY; y++)
			{
				for (s32 x = 0; x < numTilesX; x++)
				{
					const s32 tileIdx = ((y * numTilesX) + x);

					const float pixelWidth = (1.0f/(float)(numTilesX));
					const float pixelHeight = (1.0f/(float)(numTilesY));
					const float tiledX = (pixelWidth*x)+(pixelWidth/2.0f);
					const float tiledY = (pixelHeight*y)+(pixelHeight/2.0f);

					// TL, TR, BL | BL, TR, BR
					instanceEditor.SetPositionAndBinding(tileIdx, Vector4((float)x, (float)(y + 1), tiledX, tiledY));	// BL
				}
			}
		}
#else
		m_tileVertBuf = grcVertexBuffer::Create(GetTotalNumTiles() * numVertsInQuad, fvf0, bReadWrite, bDynamic, NULL);

		grcVertexBufferEditor tileEditor(m_tileVertBuf);

		const float widthMult	= m_tileSize / (float)VideoResManager::GetSceneWidth();
		const float heightMult	= m_tileSize / (float)VideoResManager::GetSceneHeight();

		for (s32 y = 0; y < numTilesY; y++)
		{
			for (s32 x = 0; x < numTilesX; x++)
			{
				const s32 tileIdx = ((y * numTilesX) + x) * numVertsInQuad;

				const float pixelWidth = (1.0f/(float)(numTilesX));
				const float pixelHeight = (1.0f/(float)(numTilesY));
				const float tiledX = (pixelWidth*x)+(pixelWidth/2.0f);
				const float tiledY = (pixelHeight*y)+(pixelHeight/2.0f);

				// TL, TR, BL | BL, TR, BR
				tileEditor.SetPositionAndBinding(tileIdx + 0, Vector4((x + 0) * widthMult, 1.0f - ((y + 1) * heightMult), tiledX, tiledY));	// BL
				tileEditor.SetPositionAndBinding(tileIdx + 1, Vector4((x + 1) * widthMult, 1.0f - ((y + 1) * heightMult), tiledX, tiledY));	// BR
				tileEditor.SetPositionAndBinding(tileIdx + 2, Vector4((x + 0) * widthMult, 1.0f - ((y + 0) * heightMult), tiledX, tiledY));	// TL

				tileEditor.SetPositionAndBinding(tileIdx + 3, Vector4((x + 0) * widthMult, 1.0f - ((y + 0) * heightMult), tiledX, tiledY));	// TL
				tileEditor.SetPositionAndBinding(tileIdx + 4, Vector4((x + 1) * widthMult, 1.0f - ((y + 1) * heightMult), tiledX, tiledY));	// BR
				tileEditor.SetPositionAndBinding(tileIdx + 5, Vector4((x + 1) * widthMult, 1.0f - ((y + 0) * heightMult), tiledX, tiledY));	// TR
			}
		}
#endif //TILED_LIGHTING_INSTANCED_TILES
	}	

#if TILED_LIGHTING_INSTANCED_TILES
	const grcVertexElement InstancedVtxElements[] =
	{
		grcVertexElement(0, grcVertexElement::grcvetPosition, 0, 8, grcFvf::grcdsHalf4),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  0, 8,  grcFvf::grcdsHalf4,  grcFvf::grcsfmIsInstanceStream, 1),
	};
	m_vtxDecl = GRCDEVICE.CreateVertexDeclaration(InstancedVtxElements, NELEM(InstancedVtxElements));
#else
	m_vtxDecl = grmModelFactory::BuildDeclarator(&fvf0, NULL, NULL, NULL);
#endif //TILED_LIGHTING_INSTANCED_TILES
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::PlatformShutdown()
{
}

// ----------------------------------------------------------------------------------------------- //

void CTiledLighting::DepthDownsample()
{
	if (!m_classificationEnabled)
		return;

	PF_PUSH_TIMEBAR_DETAIL("Depth Downsample");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

	const grcViewport *vp = gVpMan.GetCurrentGameGrcViewport();
	Vector4 projParams = ShaderUtil::CalculateProjectionParams(vp);
	m_tiledShader->SetVar(m_shaderVars[TL_PROJECTIONPARAMS], projParams);

#if ENABLE_PIX_TAGGING
	PF_PUSH_MARKER("Classification downsample compute");
#endif

	//Compute shaders for parallel reduction
	m_tiledShader->SetVar(m_shaderVars[TL_REDUCTION_DEPTH_TEXTURE], GBuffer::GetDepthTarget());
	m_tiledShader->SetVar(m_shaderVars[TL_REDUCTION_GBUFFER_TEXTURE], GBuffer::GetTarget(GBUFFER_RT_2));

	m_tiledShader->SetVarUAV(m_shaderVars[TL_REDUCTION_OUTPUT_TEXTURE], static_cast<grcTextureUAV*>(m_classificationTex));

	IntVector screeRes = { VideoResManager::GetSceneWidth(), VideoResManager::GetSceneHeight(), 0, 0 };
	m_tiledShader->SetVar(m_shaderVars[TL_SCREENRES], screeRes);

	u32 groupsX = GetNumTilesX();
	u32 groupsY = GetNumTilesY();


#if RSG_PC
	if (GRCDEVICE.GetDxFeatureLevel() < 1100)
	{
		Assertf(false, "Need to implement shader version of depth downsample for DX10");
	}
	else
#endif // RSG_PC
	{
		enum ClassificationPass
		{
			CP_DEFAULT,
			CP_MSAA_4,
			CP_MSAA_16,
		}pass = CP_DEFAULT;

#if DEVICE_MSAA
		if (GRCDEVICE.GetMSAA() > 4)
		{
			grcAssertf(m_tileSize == 8, "Tiled Lighting CS downsample requires tile size of 8 when under higher MSAA counts");
			pass = CP_MSAA_16;
		}else
		if (GRCDEVICE.GetMSAA() > 1)
		{
			grcAssertf(m_tileSize == 16, "Tiled Lighting CS downsample requires tile size of 16 when under lower MSAA counts");
			pass = CP_MSAA_4;
		}
#endif //DEVICE_MSAA

		GRCDEVICE.RunComputation("Classification", *m_tiledShader, pass, groupsX, groupsY, 1);
	}
	

#if ENABLE_PIX_TAGGING
	PF_POP_MARKER();
#endif

	GenerateMinimumDepth();

	PF_POP_TIMEBAR_DETAIL();
}


// ----------------------------------------------------------------------------------------------- //
#endif // TILED_LIGHTING_ENABLED
// ----------------------------------------------------------------------------------------------- //
