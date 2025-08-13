#ifndef LIGHT_GROUP_H_
#define LIGHT_GROUP_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "vector/vector4.h"
#include "grcore/effect_typedefs.h"

// game headers
#include "renderer/RenderThread.h"
#include "shaders/ShaderLib.h"
#include "renderer/SpuPm/SpuPmMgr.h"
#include "renderer/Lights/LightSource.h"

// framework headers

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

#define MAX_RENDERED_LIGHTS (8)

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class CLightGroup
{
public:

	CLightGroup()
	{
		for(int threadIdx=0; threadIdx < NUMBER_OF_RENDER_THREADS; threadIdx++)
		{
			m_PerThreadData[threadIdx].m_ActiveCount = 0;

			for (int i = 0; i < MAX_RENDERED_LIGHTS; i++) 
			{
				m_PerThreadData[threadIdx].m_LightSources[i] = NULL;
			}
		}

		m_bufferIndex = 0;
	}

	static void InitClass();

	inline s32 GetActiveCount() { return m_PerThreadData[g_RenderThreadIndex].m_ActiveCount;}
	inline void SetActiveCount(s32 count)
	{
		Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	
		Assertf(count <= MAX_RENDERED_LIGHTS, "Active count out of range for this instance");
		m_PerThreadData[g_RenderThreadIndex].m_ActiveCount = count;
	}

	inline const CLightSource* GetLight(int id) { return m_PerThreadData[g_RenderThreadIndex].m_LightSources[id]; }
	inline void SetLight(int id, const CLightSource* light) 
	{ 
		Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	
		m_PerThreadData[g_RenderThreadIndex].m_LightSources[id] = light; 
		m_PerThreadData[g_RenderThreadIndex].m_ActiveCount++;
		needsSorting = true; 
	}

	int  GetRenderBufferIndex() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_bufferIndex; }
	int  GetUpdateBufferIndex() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return m_bufferIndex ^ 0x1; }
	int GetCurrentBufferIndex() { return (CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) ? m_bufferIndex ^ 0x1 : m_bufferIndex; }
	void Synchronise() { m_bufferIndex ^= 1; }

	// Natural ambient
	inline const Vector4& GetNaturalAmbientDown() { return m_NaturalAmbientDown[GetCurrentBufferIndex()]; }
	inline const Vector4& GetNaturalAmbientBase() { return m_NaturalAmbientBase[GetCurrentBufferIndex()]; }
	inline void SetNaturalAmbientDown(const Vector4 &ambCol0) { m_NaturalAmbientDown[GetCurrentBufferIndex()]=ambCol0; }
	inline void SetNaturalAmbientBase(const Vector4 &ambCol1) { m_NaturalAmbientBase[GetCurrentBufferIndex()]=ambCol1; }

	// Artificial interior ambient
	inline const Vector4& GetArtificialIntAmbientDown() { return m_ArtificialIntAmbientDown[GetCurrentBufferIndex()]; }
	inline const Vector4& GetArtificialIntAmbientBase() { return m_ArtificialIntAmbientBase[GetCurrentBufferIndex()]; }
	inline void SetArtificialIntAmbientDown(const Vector4 &ambCol0) { m_ArtificialIntAmbientDown[GetCurrentBufferIndex()]=ambCol0; }
	inline void SetArtificialIntAmbientBase(const Vector4 &ambCol1) { m_ArtificialIntAmbientBase[GetCurrentBufferIndex()]=ambCol1; }

	// Artificial exterior ambient
	inline const Vector4& GetArtificialExtAmbientDown() { return m_ArtificialExtAmbientDown[GetCurrentBufferIndex()]; }
	inline const Vector4& GetArtificialExtAmbientBase() { return m_ArtificialExtAmbientBase[GetCurrentBufferIndex()]; }
	inline void SetArtificialExtAmbientDown(const Vector4 &ambCol0) { m_ArtificialExtAmbientDown[GetCurrentBufferIndex()]=ambCol0; }
	inline void SetArtificialExtAmbientBase(const Vector4 &ambCol1) { m_ArtificialExtAmbientBase[GetCurrentBufferIndex()]=ambCol1; }

	// Directional ambient
	inline void SetDirectionalAmbient(const Vector4 &dirAmb) { m_DirectionalAmbient[GetCurrentBufferIndex()] = dirAmb; }
	inline Vector4& GetDirectionalAmbient() { return m_DirectionalAmbient[GetCurrentBufferIndex()]; }

	void Reset();

	void SetAmbientLightingGlobals(const float multiplier = 1.0f, float artificialIntAmbientScale = 1.0f, float artificialExtAmbientScale = 1.0f);
	void SetAmbientLightingGlobals(const float multiplier, float artificialIntAmbientScale, float artificialExtAmbientScale, Vector4 *values);
	void SetDirectionalLightingGlobals(const float multiplier = 1.0f, bool enableDirectional = true, const Vec4V* reflectionPlane = NULL);

	void SetLocalLightingGlobals( bool sortLights = true, bool forceSetAll8Lights = false );
	void SetForwardLights(const CLightSource** lights, int numLights);
	void SetForwardLights(const CLightSource* lights, int numLights);
	void DisableForwardLights();

	void ResetLightingGlobals();
	void SetFakeLighting(float ambientScalar, float directionalScalar, const Matrix34 &camMtx);

	static void calculateSpotLightScaleBias(const CLightSource *light, float& scale, float& bias);

	static void SetLocalLightForwardIntensityScale(float scale) { sm_gvLocalLightForwardIntensityScale = ScalarV(scale); }

protected:
	static DECLARE_MTR_THREAD bool needsSorting;

	struct PER_THREAD_DATA
	{
		int	m_ActiveCount;												// Number of active lights
		const CLightSource* m_LightSources[MAX_RENDERED_LIGHTS];
	};
	PER_THREAD_DATA m_PerThreadData[NUMBER_OF_RENDER_THREADS];

	int m_bufferIndex;

	Vector4 m_NaturalAmbientDown[2];								// Natural Ambient Down
	Vector4 m_NaturalAmbientBase[2];								// Natural Ambient Up

	static grcEffectGlobalVar sm_gvLightNaturalAmbient0;
	static grcEffectGlobalVar sm_gvLightNaturalAmbient1;

	Vector4 m_ArtificialIntAmbientDown[2];							// Artificial Interior Ambient Down 
	Vector4 m_ArtificialIntAmbientBase[2];							// Artificial Interior Ambient Up
	Vector4 m_ArtificialExtAmbientDown[2];							// Artificial Exterior Ambient Down 
	Vector4 m_ArtificialExtAmbientBase[2];							// Artificial Exterior Ambient Up
	Vector4 m_DirectionalAmbient[2];

	static grcEffectGlobalVar sm_gvLightArtificialIntAmbient0;
	static grcEffectGlobalVar sm_gvLightArtificialIntAmbient1;

	static grcEffectGlobalVar sm_gvLightArtificialExtAmbient0;
	static grcEffectGlobalVar sm_gvLightArtificialExtAmbient1;

	static grcEffectGlobalVar sm_gvDirectionalAmbientID;

	static grcEffectGlobalVar sm_giNumForwardLightsID;

	// packed lighting constants 
	static grcEffectGlobalVar sm_gLightPositionAndInvDistSqr;
	static grcEffectGlobalVar sm_gLightDirectionAndFalloffExponent;
	static grcEffectGlobalVar sm_gLightColourAndCapsuleExtent;
	static grcEffectGlobalVar sm_gLightConeScale;
	static grcEffectGlobalVar sm_gLightConeOffset;

	static grcEffectGlobalVar sm_gvDirectionalLight;
	static grcEffectGlobalVar sm_gvDirectionalColor;

//	static grcEffectGlobalVar sm_gvParticleLightingParams;

	static ScalarV sm_gvLocalLightForwardIntensityScale;
};

#endif
