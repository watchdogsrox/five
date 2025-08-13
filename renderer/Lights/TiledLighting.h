#ifndef TILED_LIGHTING_H_
#define TILED_LIGHTING_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "atl/bitset.h"
#include "math/float16.h"
#include "grcore/effect_typedefs.h"
#include "grcore/vertexdecl.h"

// game headers
#include "Renderer/Lights/TiledLightingSettings.h"
#include "Renderer/Util/ShmooFile.h"

// framework headers
#include "fwtl/customvertexpushbuffer.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

// tiled lighting doesn't work at 1080p because the dimensions need to be a factor of 16
#define TILED_LIGHTING_ENABLED (1)
// ----------------------------------------------------------------------------------------------- //

namespace rage
{
	class grmShader;
	class grcRenderTarget;
	class grcVertexBuffer;
	class grcIndexBuffer;
	struct grcVertexDeclaration;
}

struct TileVertex;
struct LightShaderInfo;

// ----------------------------------------------------------------------------------------------- //

enum TL_SHADER_VARS_ENUM
{
	TL_SRCTEXTURESIZE = 0,
	TL_DSTTEXTURESIZE,
	TL_SCREENRES,
	TL_GBUFFERDEPTH,
	TL_GBUFFER0,
	TL_GBUFFER1,
	TL_GBUFFER2,
	TL_GBUFFER3,
	TL_GBUFFERSTENCIL,
	TL_TEXTURE0,
	TL_DEFERREDTEXTURE1P,
	TL_DEFERREDTEXTURE0,
	TL_DEFERREDTEXTURE1,
	TL_PROJECTIONPARAMS,
	TL_SCREENSIZE,
	TL_LIGHTSDATA,
	TL_NUMLIGHTS,
	TL_REDUCTION_DEPTH_TEXTURE,
	TL_REDUCTION_GBUFFER_TEXTURE,
	TL_REDUCTION_OUTPUT_TEXTURE,
	TL_NUM_SHADER_VARS
};

// ----------------------------------------------------------------------------------------------- //

enum TL_SHADER_TECHNIQUES_ENUM
{
	TL_TECH_DOWNSAMPLE = 0,
	TL_TECH_MINDEPTH_DOWNSAMPLE=1,
	TL_NUM_TECHNIQUES
};

// =============================================================================================== //
// CLASS
// =============================================================================================== //

// ----------------------------------------------------------------------------------------------- //
#if TILED_LIGHTING_ENABLED
// ----------------------------------------------------------------------------------------------- //

class MinimumDepthTexture
{
	int m_width;
	int m_height;
	float m_srcWidth;
	float m_srcHeight;

#if __D3D11
#if RSG_PC
	#define MAX_MIN_TARGETS	(3 + MAX_GPUS)
	#define NUM_MIN_TARGETS (3 + GRCDEVICE.GetGPUCount())

	grcRenderTarget*	m_rt[MAX_MIN_TARGETS];
	grcTexture*			m_rtTex[MAX_MIN_TARGETS];
	u32					m_CurrentRTIndex;
#else
	grcRenderTarget*	m_rt;
	grcTexture*			m_rtTex;
#endif
	float				m_NearZ;
	float               m_AverageZ;
	float				m_CenterZ;
	bool				m_ComputeNearZ;
	bool				m_UpdateNearZ;
	float               m_ShadowFarClip;
#else
	grcRenderTarget*	m_rt;
	grcTextureLock		m_lock;
#endif

public:
	MinimumDepthTexture( const grcTexture* classificationTexture);
	~MinimumDepthTexture();
	void Generate( grmShader* tiledShader, grcTexture* parentTexture, 
		grcEffectVar srcTextureSizeVar, grcEffectVar dstTextureSizeVar, grcEffectVar srcTextureVarP,grcEffectVar srcTextureVar,
		grcEffectTechnique minDepthTech
#if GENERATE_SHMOO
		, int shmooIdx
#endif // GENERATE_SHMOO
		);
	void CalculateNearZ(float shadowFarClip, float& minDepth, float& avgDepth, float& centerDepth);
	void CalculateNearZValue(const grcTextureLock &lock, float shadowFarClip, float& minDepth, float& avgDepth, float& centerDepth) const;
#if __D3D11
	void CalculateNearZRenderThread();
#endif
};


class CTiledLighting
{
public:

	// General Functions
	static void Init();
	static void Shutdown();
	static void Update();
	static void Render();

	static u32 GetNumTilesX();
	static u32 GetNumTilesY();
	static u32 GetTotalNumTiles();
	static u32 GetNumTileComponents();

	static bool IsEnabled() { return m_classificationEnabled; }

	static grcVertexBuffer* GetTileVertBuffer() { return m_tileVertBuf; }
	static grcVertexDeclaration* GetTileVertDecl() { return m_vtxDecl; }
#if TILED_LIGHTING_INSTANCED_TILES
	static grcVertexBuffer* GetTileInstanceBuffer() { return m_tileInstanceBuf; }
#endif //TILED_LIGHTING_INSTANCED_TILES

	static void SetupTileData();
	static void DepthDownsample();
	static void CalculateNearZ(float shadowFarClip, float& minDepth, float& avgDepth, float& centerDepth);
#if __D3D11
	static void CalculateNearZRenderThread();
#endif

	static void RenderTiles(grcRenderTarget* target, grmShader* pShader, grcEffectTechnique technique, u32 pass, 
							const char* pLabelStr, bool clearTarget = false, bool lockDepth = false, bool clearColor = false);

	static grcVertexBuffer* GetTileInfoVertBuffer() { return m_tileInfoVertBuf; }

	static grcRenderTarget* GetClassificationTexture() { return m_classificationTex; }

	static Float16* GetRawTileData() { return m_rawTileData; }

	static void GenerateMinimumDepth();

	static u32 GetTileSize() { return m_tileSize; }

#if __BANK
	static bool* GetClassificationEnabledPtr() { return &m_classificationEnabled; }
#endif

	static void SetupShader();

private:

	static bool GetPlatformSupport();
	static void PlatformShutdown();

	// Tiled Lighting specific functions
	static void LightClassification();

	// Shader info
	static grcEffectVar m_shaderVars[];
	static grcEffectTechnique m_techniques[];
	static grmShader* m_tiledShader;
#if GENERATE_SHMOO
	static int m_tiledShaderShmooIdx;
#else
	#define m_tiledShaderShmooIdx (-1)	
#endif // GENERATE_SHMOO

	static bool m_classificationEnabled;

	// State blocks
	static grcDepthStencilStateHandle	m_depthDownSample_DS;
	static grcRasterizerStateHandle		m_depthDownSample_RS;

	// Classification data
	static grcRenderTarget* m_classificationTex;

	static grcVertexBuffer* m_tileVertBuf;
	static grcVertexBuffer* m_tileInfoVertBuf;
#if TILED_LIGHTING_INSTANCED_TILES
	static grcVertexBuffer* m_tileInstanceBuf;
#endif //TILED_LIGHTING_INSTANCED_TILES

	static grcVertexDeclaration* m_vtxDecl; 

	static Float16* m_rawTileData;

	static u32 m_tileSize;

// ----------------------------------------------------------------------------------------------- //
#if __PS3
// ----------------------------------------------------------------------------------------------- //
	static const grcVertexElement m_vtxElements[];
// ----------------------------------------------------------------------------------------------- //		
#endif
// ----------------------------------------------------------------------------------------------- //
};

extern MinimumDepthTexture *g_minimumDepthTexture;

// ----------------------------------------------------------------------------------------------- //
#endif // TILED_LIGHTING_ENABLED
// ----------------------------------------------------------------------------------------------- //

#endif
