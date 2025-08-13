#ifndef LOD_LIGHTS_H_
#define LOD_LIGHTS_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "grcore/array.h"

// framework

// game 
#include "Renderer/Renderer.h"
#include "renderer/Lights/LightSource.h"
#include "renderer/ProcessLodLightVisibility.h"
#include "vfx/misc/LODLightManager.h"
#include "vfx/VisualEffects.h"

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class CLODLights
{
public:

	// Functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();

	static void RenderLODLights();
	static void RenderDistantLODLights(CVisualEffects::eRenderMode mode, float intensityScale);
	static bool IsActive(CLODLight *pLODLight, u32 idx);

	static float GetRenderLodLightRange(eLodLightCategory lightCategory);
	static float GetUpdateLodLightRange(eLodLightCategory lightCategory);
	
	static float GetRenderLodLightCoronaRange(eLodLightCategory lightCategory);
	static float GetUpdateLodLightCoronaRange(eLodLightCategory lightCategory);
	
	static float GetLodLightFadeRange(eLodLightCategory lightCategory) { return m_lodLightRange[lightCategory]; }

	static LodLightVisibility* GetLODLightVisibilityUpdateBuffer();
	static LodLightVisibility* GetLODLightVisibilityRenderBuffer();
	static u32 GetLODLightVisibilityCountUpdateBuffer();
	static u32 GetLODLightVisibilityCountRenderBuffer();

	static float GetScaleMultiplier() { return m_lodLightScaleMultiplier; }
	static float GetCoronaSize() { return m_lodLightCoronaSize; }

	static bool  Enabled() { return m_lodLightsEnabled && !CRenderer::GetDisableArtificialLights(); }
	static void  SetEnabled(bool enabled) { m_lodLightsEnabled = enabled; }

	static void ProcessVisibility();
	static void WaitForProcessVisibilityDependency();

	static float CalculateSphereExpansion(const float distanceToGround);
	static void UpdateVisualDataSettings();

	static int GetRenderBufferIndex() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_bufferIndex; }
	static int GetUpdateBufferIndex() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return m_bufferIndex ^ 0x1; }
	static int GetCurrentBufferIndex() { return (CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) ? (m_bufferIndex ^ 0x1) : m_bufferIndex; }
	static void Synchronise() { m_bufferIndex ^= 1; }

	#define MaxInstBufferCount (RSG_PC ? (3 + 4) : 3)
	static u32 m_maxVertexBuffers;
	
	static grcVertexBuffer* m_pointInstanceVB[MaxInstBufferCount];
	static grcVertexBuffer* m_spotInstanceVB[MaxInstBufferCount];
	static grcVertexBuffer* m_capsuleInstanceVB[MaxInstBufferCount];

	static bool m_vertexBuffersInitalised;
	
	static u32 m_currentPointVBIndex;
	static u32 m_currentSpotVBIndex;
	static u32 m_currentCapsuleVBIndex;

	static grcVertexDeclaration* m_pointVtxDecl;
	static grcVertexDeclaration* m_spotVtxDecl;
	static grcVertexDeclaration* m_capsuleVtxDecl;

#if __BANK
	static float* GetRangePtr(eLodLightCategory lightCategory) { return &m_lodLightRange[lightCategory]; }
	static float* GetFadePtr(eLodLightCategory lightCategory) { return &m_lodLightFade[lightCategory]; }
	static float* GetCoronaRangePtr(eLodLightCategory lightCategory) { return &m_lodLightCoronaRange[lightCategory]; }
	static float* GetScaleMultiplierPtr() { return &m_lodLightScaleMultiplier; }
	static u32	  GetRenderedCount() { return m_lodLightCount; }
	static u32* GetLODLightCoronaCountPtr() { return &m_lodLightCoronaCount; }
	static u32* GetLODVisibleLODLightsCountPtr() { return &m_lodLightCount; }
	static float *GetCoronaSizePtr() { return &m_lodLightCoronaSize; }
#endif

private:

	static void InitInstancedVertexBuffers();

	// Variables
	static bool m_lodLightsEnabled;
	
	static float m_lodLightRange[LODLIGHT_CATEGORY_COUNT];
	static float m_lodLightFade[LODLIGHT_CATEGORY_COUNT];
	static float m_lodLightCoronaRange[LODLIGHT_CATEGORY_COUNT];
	
	static float m_lodLightScaleMultiplier;
	static float m_lodLightCoronaIntensityMult;
	
	static float m_lodLightCoronaSize;
	
	static float m_lodLightSphereExpandSizeMult;
	static float m_lodLightSphereExpandStart;
	static float m_lodLightSphereExpandEnd;

	static int m_bufferIndex;

	#if __BANK
		static u32 m_lodLightCount;
		static u32 m_lodLightCoronaCount;
	#endif 

	#if __ASSERT
		static u32 m_instancedOverflow[3];
	#endif
};

#endif

