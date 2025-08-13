#ifndef __DISTANTLIGHTS_RENDER_H
#define __DISTANTLIGHTS_RENDER_H

// Rage headers
#include "grcore/vertexbuffer.h"
#include "vfx/VisualEffects.h" // for eRenderMode

#if __WIN32PC && __D3D9
//Make vertex buffer size bigger on PC as we need to send more vertex data for each vert of the tris as we cant use quads. 
//Probably wants changing to use something like instancing to reduce amount of data we need to send.
#define		DISTANTLIGHTS_VERTEXBUFFER_VERTEXBUFFER_SIZE			(64 * 1024 * 6)
#else
#define		DISTANTLIGHTS_VERTEXBUFFER_VERTEXBUFFER_SIZE			(64 * 1024) // use 32k blocks of vertex buffer
#endif

struct ALIGNAS(16) DistantLightVertex_t
{
	float x,y,z;
	u32 color;
}	;

namespace rage { struct grcVertexDeclaration; }

class CDistantLightsVertexBuffer
{
public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

	static void FlipBuffers();
	static void RenderBuffer(bool bEndList);
	static void RenderBufferBegin(CVisualEffects::eRenderMode mode, float fadeNearDist, float fadeFarDist, float intensity, bool dist2d = false, Vector4* distLightFadeParams = NULL);
	static void SetStateBlocks(bool bIgnoreDepth, bool isWaterReflection);
	static Vec4V* ReserveRenderBufferSpace(int count);
	static grcVertexBuffer* CreateUVVertexBuffer(int channel);

	static void CreateVertexBuffers();

private:

	static grcVertexDeclaration *m_pDistantLightVertexDeclaration;
	static grcVertexBuffer	*m_pDistantLightUVBuffer;
	static bool m_vertexBuffersInitialised;

	#define MaxQueuedBuffers 3 // Triple buffer

	static atRangeArray<atFixedArray<grcVertexBuffer*, DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES>, MaxQueuedBuffers> m_vertexBuffers;
	
	static __THREAD grcVertexBuffer* m_pCurrVertexBuffer;
	static __THREAD int	m_currVertexCount;

	static int m_vertexBufferSet;
	static int m_vertexBufferIndex;

	static grcBlendStateHandle m_blendState;
	static grcRasterizerStateHandle m_rasterizerState;
	static grcDepthStencilStateHandle m_waterDepthState;
	static grcDepthStencilStateHandle m_sceneDepthState;

};

#endif	//__DISTANTLIGHTS_RENDERER_H
