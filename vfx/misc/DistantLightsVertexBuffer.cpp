
// Rage header
#include "grcore/debugdraw.h"
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"
#include "system/cache.h"
#include "system/endian.h"
#include "system/param.h"
#include "system/interlocked.h"

// Framework headers
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "fwsys/timer.h"
#include "fwscene/stores/txdstore.h"
#include "fwutil/xmacro.h"

// Game header
#include "camera/viewports/ViewportManager.h"
#include "Core/Game.h"
#include "Debug/VectorMap.h"
#include "Game/Clock.h"
#include "Renderer/Occlusion.h"
#include "Renderer/Sprite2d.h"
#include "Renderer/Water.h"
#include "shaders/ShaderLib.h"
#include "streaming/streamingdebug.h"
#include "timecycle/TimeCycle.h"
#include "vfx/misc/DistantLightsVertexBuffer.h"
#include "vfx/misc/DistantLights.h"

grcBlendStateHandle	CDistantLightsVertexBuffer::m_blendState;
grcRasterizerStateHandle CDistantLightsVertexBuffer::m_rasterizerState;
grcDepthStencilStateHandle CDistantLightsVertexBuffer::m_waterDepthState;
grcDepthStencilStateHandle CDistantLightsVertexBuffer::m_sceneDepthState;

int	CDistantLightsVertexBuffer::m_vertexBufferSet;
int	CDistantLightsVertexBuffer::m_vertexBufferIndex;

__THREAD grcVertexBuffer* CDistantLightsVertexBuffer::m_pCurrVertexBuffer = NULL;
__THREAD int CDistantLightsVertexBuffer::m_currVertexCount = 0;

atRangeArray<atFixedArray<grcVertexBuffer*, DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES>, MaxQueuedBuffers>	CDistantLightsVertexBuffer::m_vertexBuffers;

grcVertexDeclaration *CDistantLightsVertexBuffer::m_pDistantLightVertexDeclaration = NULL;
grcVertexBuffer	*CDistantLightsVertexBuffer::m_pDistantLightUVBuffer = NULL;
bool CDistantLightsVertexBuffer::m_vertexBuffersInitialised = false;


void CDistantLightsVertexBuffer::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		m_vertexBufferSet = 0;
		m_vertexBufferIndex =  -1;

		m_pCurrVertexBuffer = NULL;
		m_currVertexCount = 0;

		int i, j;
		for(i = 0; i < MaxQueuedBuffers; i++)
		{
			for(j=0; j<DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES; j++)
			{
				m_vertexBuffers[i].Push(NULL);
			}
		}

		grcVertexElement elements[3] = 
		{
			grcVertexElement(0, grcVertexElement::grcvetPosition,	0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat3), grcFvf::grcdsFloat3, grcFvf::grcsfmIsInstanceStream, 1),
			grcVertexElement(0, grcVertexElement::grcvetColor,		0, grcFvf::GetDataSizeFromType(grcFvf::grcdsColor), grcFvf::grcdsColor, grcFvf::grcsfmIsInstanceStream, 1),
			grcVertexElement(1, grcVertexElement::grcvetTexture,	0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat2), grcFvf::grcdsFloat2),
		};

		m_pDistantLightVertexDeclaration = GRCDEVICE.CreateVertexDeclaration(elements, sizeof(elements) / sizeof(grcVertexElement));
		m_pDistantLightUVBuffer = CreateUVVertexBuffer(0);

		grcBlendStateDesc blendDesc;
		blendDesc.BlendRTDesc[0].BlendEnable = TRUE;
		blendDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		blendDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
		blendDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		m_blendState = grcStateBlock::CreateBlendState(blendDesc);

		grcRasterizerStateDesc rasterizerDesc;
		rasterizerDesc.CullMode = grcRSV::CULL_NONE;
		m_rasterizerState = grcStateBlock::CreateRasterizerState(rasterizerDesc);

		grcDepthStencilStateDesc depthDesc;
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		depthDesc.DepthWriteMask = FALSE;
		m_sceneDepthState = grcStateBlock::CreateDepthStencilState(depthDesc);

		depthDesc.DepthFunc = grcRSV::CMP_LESSEQUAL;
		m_waterDepthState = grcStateBlock::CreateDepthStencilState(depthDesc);

		m_vertexBuffersInitialised = false;

		CreateVertexBuffers();
	}
}

void CDistantLightsVertexBuffer::Shutdown(unsigned shutdownMode)
{	
	if (shutdownMode == SHUTDOWN_CORE)
	{
		int i, j;

		for(i = 0; i < MaxQueuedBuffers; i++)
		{
			for(j=0; j<DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES; j++)
			{
				if(m_vertexBuffers[i][j])
				{
					delete m_vertexBuffers[i][j];
					m_vertexBuffers[i][j] = NULL;
				}
			}
		}

		m_vertexBuffersInitialised = false;
	}
}

void CDistantLightsVertexBuffer::FlipBuffers()
{
	m_vertexBufferSet = (m_vertexBufferSet + 1) % MaxQueuedBuffers;
	m_vertexBufferIndex = -1;
}

extern distLightsParam g_distLightsParam;
static CSprite2d g_distLightsSprite;

void CDistantLightsVertexBuffer::RenderBuffer(bool bEndList)
{
	if (m_pCurrVertexBuffer)
	{
		#if RSG_DURANGO || RSG_ORBIS
			void* lockPtr = m_pCurrVertexBuffer->GetLockPtr();
		#endif

		// Unlock the current vertex buffer.
		m_pCurrVertexBuffer->UnlockWO();

		if(m_currVertexCount > 0)
		{
			#if RSG_DURANGO || RSG_ORBIS
				// Make sure that vertex buffer changes are visible to the GPU.
				DistantLightVertex_t* buffer = reinterpret_cast<DistantLightVertex_t*>(lockPtr);
				WritebackDC(buffer, DISTANTLIGHTS_VERTEXBUFFER_VERTEXBUFFER_SIZE);
			#endif

			GRCDEVICE.SetVertexDeclaration(m_pDistantLightVertexDeclaration);
			GRCDEVICE.SetStreamSource(0, *m_pCurrVertexBuffer, 0, sizeof(DistantLightVertex_t));
			GRCDEVICE.SetStreamSource(1, *m_pDistantLightUVBuffer, 0, sizeof(float)*2);

			GRCDEVICE.DrawInstancedPrimitive(drawTriStrip, 4, m_currVertexCount, 0, 0);

			GRCDEVICE.ClearStreamSource(0);
			GRCDEVICE.ClearStreamSource(1);

			GRCDEVICE.SetVertexDeclaration(NULL);
		}

		m_currVertexCount = 0;
		m_pCurrVertexBuffer = NULL;
	}
	else
	{
		m_currVertexCount = 0;
	}

	if (bEndList)
	{
		g_distLightsSprite.EndCustomList();
	}
}

//#undef SET_VERT

void CDistantLightsVertexBuffer::RenderBufferBegin(CVisualEffects::eRenderMode mode, float fadeNearDist, float fadeFarDist, float intensity, bool dist2d /* = false */, Vector4* distLightFadeParams)
{
	// calc the size of the light
	const Vec3V vCamPos = gVpMan.GetRenderGameGrcViewport()->GetCameraPosition();
	float sizeMult = 1.0f + Clamp((vCamPos.GetZf()-g_distLightsParam.sizeDistStart)/g_distLightsParam.sizeDist, 0.0f, 1.0f);

	// get the texture we are going to use
	grcTexture* pTexture = CShaderLib::LookupTexture("distant_light");
	vfxAssertf(pTexture, "CDistantLights::Render - corona texture lookup failed");

	SetStateBlocks(BANK_SWITCH(g_distantLights.m_disableDepthTest, false),mode == CVisualEffects::RM_WATER_REFLECTION);
	grcWorldIdentity();
	const tcKeyframeUncompressed& keyFrame = g_timeCycle.GetCurrRenderKeyframe();

	Vector4 shaderParams0, shaderParams1;
	shaderParams0.Zero();
	shaderParams1.Zero();

	const float fadeDiff = (fadeFarDist == fadeNearDist) ? 1.0f : (fadeFarDist - fadeNearDist);
	
	shaderParams0.x = (mode == CVisualEffects::RM_DEFAULT ? g_distLightsParam.size : g_distLightsParam.sizeReflections) * sizeMult * 2.0f; // The 2.0f is because we used to calculate radius, now the shader needs diameter
	shaderParams0.y = fadeNearDist;
	shaderParams0.z = 1.0f / fadeDiff;
	shaderParams0.w = intensity * keyFrame.GetVar(TCVAR_SPRITE_BRIGHTNESS);

	shaderParams1.x = (1024.0f / g_distLightsParam.sizeMin) / (mode == CVisualEffects::RM_DEFAULT ? g_distLightsParam.sizeUpscale : g_distLightsParam.sizeUpscaleReflections);
	shaderParams1.y = (1.0f - g_distLightsParam.flicker); // flickerMin
	shaderParams1.z = float(fwTimer::GetTimeInMilliseconds()) * g_distLightsParam.twinkleSpeed / 256.0f; // elapsedTime 
	shaderParams1.w = (1.0f - (g_distLightsParam.twinkleAmount * keyFrame.GetVar(TCVAR_SPRITE_DISTANT_LIGHT_TWINKLE))); // twinkleAmount

	// tweak sizeUpscale for optimised VS_DistantLights
	shaderParams1.x = gVpMan.GetRenderGameGrcViewport()->GetTanHFOV()/(shaderParams1.x*shaderParams0.x);

	g_distLightsSprite.SetGeneralParams(shaderParams0, shaderParams1);
	g_distLightsSprite.SetNoiseTexture(Water::GetNoiseTexture());

#if USE_DISTANT_LIGHTS_2
	//Values to allow the carlights to fade in at a different range to the street lights
	Vector4 refmipBlurParams;
	refmipBlurParams.Zero();
	if( distLightFadeParams )
		refmipBlurParams = *distLightFadeParams;
	g_distLightsSprite.SetRefMipBlurParams(refmipBlurParams);
#endif //USE_DISTANT_LIGHTS_2

	u32 pass = dist2d ? 1 : 0;
	pass += (mode == CVisualEffects::RM_WATER_REFLECTION) ? 2 : 0;

	g_distLightsSprite.BeginCustomList(CSprite2d::CS_DISTANT_LIGHTS, pTexture, pass);
}

void	CDistantLightsVertexBuffer::SetStateBlocks(bool bIgnoreDepth, bool isWaterReflection)
{
	grcStateBlock::SetBlendState(m_blendState);
	grcStateBlock::SetRasterizerState(m_rasterizerState);
	grcStateBlock::SetDepthStencilState(bIgnoreDepth ? grcStateBlock::DSS_IgnoreDepth : isWaterReflection ? m_waterDepthState : m_sceneDepthState);
}

static sysCriticalSectionToken g_lockUpdateVertexBufferIndex;

Vec4V* CDistantLightsVertexBuffer::ReserveRenderBufferSpace(int count)
{
	bool needNewBuffer = false;
	const u32 additionalVerts = 3;
	const u32 neededVerts = count + additionalVerts;
	const u32 maxVertexCount = DISTANTLIGHTS_VERTEXBUFFER_VERTEXBUFFER_SIZE / sizeof(DistantLightVertex_t);

	// Beginning of new frame
	if (m_vertexBufferIndex == -1)
	{
		m_currVertexCount = 0;
	}

	if(neededVerts > maxVertexCount)
	{
		// HACK - just return for now so we don't crash! TBD, this needs to be fixed the right way.
		Errorf("Too many lights (%d) for a single buffer allocation! Skipping rendering this batch of lights...", count);
		return NULL;
	} 
	
	needNewBuffer = (m_pCurrVertexBuffer == NULL) || (m_currVertexCount + neededVerts >= maxVertexCount);

	if (needNewBuffer)
	{
		Assertf(m_vertexBufferIndex + 1 < DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES, "We're going to get flashing distant lights");

		RenderBuffer(false);

		sysCriticalSection mutex(g_lockUpdateVertexBufferIndex);
		m_vertexBufferIndex = (m_vertexBufferIndex + 1) % DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES; // Move onto the next vertex buffer.
		m_pCurrVertexBuffer = m_vertexBuffers[m_vertexBufferSet][m_vertexBufferIndex];
		mutex.Exit();

		u32 lockFlags = 0;
		lockFlags = MULTIPLE_RENDER_THREADS ? rage::grcsDiscard : rage::grcsNoOverwrite;
		Verifyf(m_pCurrVertexBuffer->Lock(lockFlags), "Couldn't lock vertex buffer");
	}

	Vec4V* reservedSpace = NULL;
	DistantLightVertex_t* buffer = reinterpret_cast<DistantLightVertex_t*>(m_pCurrVertexBuffer->GetLockPtr());
	reservedSpace = reinterpret_cast<Vec4V*>(buffer + m_currVertexCount);
	m_currVertexCount += count;

	return reservedSpace;
}


grcVertexBuffer* CDistantLightsVertexBuffer::CreateUVVertexBuffer(int channel)
{
	// These represent a mini strip making a quad.
	ALIGNAS(16) const Vector2 QuadVerts[4] = 
	{
		Vector2(1.0f, 0.0f), 
		Vector2(0.0f, 0.0f), 
		Vector2(1.0f, 1.0f), 
		Vector2(0.0f, 1.0f)
	};

	grcFvf fvf0;
	fvf0.SetTextureChannel(channel, true, grcFvf::grcdsFloat2);
	fvf0.SetPosChannel(false);

	// Create the vertex buffer.
	#if RSG_PC
		grcVertexBuffer* pVB = grcVertexBuffer::CreateWithData(4, fvf0, grcsBufferCreate_NeitherReadNorWrite, (const void *)&QuadVerts[0]);
	#else // RSG_PC
		grcVertexBuffer* pVB = grcVertexBuffer::CreateWithData(4, fvf0, (const void *)&QuadVerts[0]);
	#endif // RSG_PC

	return pVB;
}

void CDistantLightsVertexBuffer::CreateVertexBuffers()
{
	if (!m_vertexBuffersInitialised)
	{
		grcFvf fvf;
		fvf.SetPosChannel(true, grcFvf::grcdsFloat3);
		fvf.SetDiffuseChannel(true, grcFvf::grcdsColor);

		int vertexCount = DISTANTLIGHTS_VERTEXBUFFER_VERTEXBUFFER_SIZE / sizeof(DistantLightVertex_t);

		for (int i = 0; i < MaxQueuedBuffers; i++)
		{
			for (int j = 0; j < DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES; j++)
			{
				grcVertexBuffer* vb = NULL;

				// Create a new vertex buffer to put lights into
				#if RSG_DURANGO
					const bool bReadWrite = true;
					const bool bDynamic = false;
					vb = grcVertexBuffer::Create(vertexCount, fvf, bReadWrite, bDynamic, NULL);
				#else
					vb = grcVertexBuffer::Create(vertexCount, fvf, rage::grcsBufferCreate_DynamicWrite, rage::grcsBufferSync_None, NULL);
				#endif

				// Add it to our list of existing vertex buffers.
				m_vertexBuffers[i][j] = vb;
			}
		}
		m_vertexBuffersInitialised = true;
	}
}
