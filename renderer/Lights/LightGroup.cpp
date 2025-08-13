// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// Game headers
#include "Renderer/Lights/LightGroup.h"
#include "Debug/Rendering/DebugLighting.h"
#include "renderer/SpuPM/SpuPmMgr.h"
#include "renderer/Lights/Lights.h"
#include "timecycle/TimeCycle.h"

// Framework headers
#include "timecycle/tckeyframe.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //
RENDER_OPTIMISATIONS()

// Shader bindings
grcEffectGlobalVar CLightGroup::sm_gvLightNaturalAmbient0;
grcEffectGlobalVar CLightGroup::sm_gvLightNaturalAmbient1;

grcEffectGlobalVar CLightGroup::sm_gvLightArtificialIntAmbient0;
grcEffectGlobalVar CLightGroup::sm_gvLightArtificialIntAmbient1;

grcEffectGlobalVar CLightGroup::sm_gvLightArtificialExtAmbient0;
grcEffectGlobalVar CLightGroup::sm_gvLightArtificialExtAmbient1;

grcEffectGlobalVar CLightGroup::sm_gvDirectionalAmbientID;

grcEffectGlobalVar CLightGroup::sm_giNumForwardLightsID;

// packed lighting constants 
grcEffectGlobalVar CLightGroup::sm_gLightPositionAndInvDistSqr;
grcEffectGlobalVar CLightGroup::sm_gLightDirectionAndFalloffExponent;
grcEffectGlobalVar CLightGroup::sm_gLightColourAndCapsuleExtent;
grcEffectGlobalVar CLightGroup::sm_gLightConeScale;
grcEffectGlobalVar CLightGroup::sm_gLightConeOffset;

grcEffectGlobalVar CLightGroup::sm_gvDirectionalLight;
grcEffectGlobalVar CLightGroup::sm_gvDirectionalColor;

ScalarV CLightGroup::sm_gvLocalLightForwardIntensityScale(V_ONE);

DECLARE_MTR_THREAD bool CLightGroup::needsSorting = true;

// =============================================================================================== //
// HELPER FUNCTIONS
// =============================================================================================== //

static __forceinline Vector4 MakeVector4(const Vector3& a, float b)
{
	Vector4 v;
	v.SetVector3( a);
	v.w = b;
	return v;
}

// ----------------------------------------------------------------------------------------------- //

struct LightSorter
{
	// Make sure all black lights are at the back of the array
	bool operator()(const CLightSource* light1, const CLightSource* light2) const
	{
		if (light1 == &Lights::m_blackLight && light2 == &Lights::m_blackLight)
		{
			return false;
		}
		else if (light1 == &Lights::m_blackLight)
		{
			return false;
		}
		else if (light2 == &Lights::m_blackLight)
		{
			return true;
		}

		return (size_t)light1 < (size_t)light2;
	}
};

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void CLightGroup::InitClass()
{
	sm_gLightPositionAndInvDistSqr  = grcEffect::LookupGlobalVar("gLightPositionAndInvDistSqr");
	sm_gLightDirectionAndFalloffExponent = grcEffect::LookupGlobalVar("gLightDirectionAndFalloffExponent");
	sm_gLightColourAndCapsuleExtent = grcEffect::LookupGlobalVar("gLightColourAndCapsuleExtent");
	sm_gLightConeScale = grcEffect::LookupGlobalVar("gLightConeScale");
	sm_gLightConeOffset = grcEffect::LookupGlobalVar("gLightConeOffset");

	// Directional
	sm_gvDirectionalLight = grcEffect::LookupGlobalVar("gDirectionalLight");
	sm_gvDirectionalColor  = grcEffect::LookupGlobalVar("gDirectionalColour");

	// Ambient
	sm_gvLightNaturalAmbient0 = grcEffect::LookupGlobalVar("gLightNaturalAmbient0", true);
	sm_gvLightNaturalAmbient1 = grcEffect::LookupGlobalVar("gLightNaturalAmbient1", true);

	sm_gvLightArtificialIntAmbient0 = grcEffect::LookupGlobalVar("gLightArtificialIntAmbient0", true);
	sm_gvLightArtificialIntAmbient1 = grcEffect::LookupGlobalVar("gLightArtificialIntAmbient1", true);

	sm_gvLightArtificialExtAmbient0 = grcEffect::LookupGlobalVar("gLightArtificialExtAmbient0", true);
	sm_gvLightArtificialExtAmbient1 = grcEffect::LookupGlobalVar("gLightArtificialExtAmbient1", true);

	sm_gvDirectionalAmbientID = grcEffect::LookupGlobalVar("gDirectionalAmbientColour", true);

	sm_giNumForwardLightsID = grcEffect::LookupGlobalVar("gNumForwardLights", true);
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::calculateSpotLightScaleBias(const CLightSource *light, float& scale, float& bias)
{
	if (!(light->GetType() == LIGHT_TYPE_SPOT && light->GetConeCosOuterAngle() > 0.01f))
	{
		bias = 1.0f;
		scale = 0.0f;
		return;
	}

	scale = 1.0f / rage::Max((light->GetConeCosInnerAngle() - light->GetConeCosOuterAngle()), 0.000001f);
	bias = -light->GetConeCosOuterAngle() * scale;
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::Reset()
{
	std::fill_n(m_PerThreadData[g_RenderThreadIndex].m_LightSources, MAX_RENDERED_LIGHTS, &Lights::m_blackLight);
	m_PerThreadData[g_RenderThreadIndex].m_ActiveCount = 0;
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::SetDirectionalLightingGlobals(const float multiplier, bool enableDirectional, const Vec4V* reflectionPlane)
{
	// AJH_TODO check context use
	const CLightSource &dirLight = Lights::GetRenderDirectionalLight();

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
	Vec3V lightDirection = VECTOR3_TO_VEC3V(dirLight.GetDirection());

	if (reflectionPlane)
	{
		lightDirection = PlaneReflectVector(*reflectionPlane, lightDirection);
	}

	const Vec4V lightDirectionAndMult(lightDirection, ScalarV(currKeyframe.GetVar(TCVAR_REFLECTION_TWEAK_DIRECTIONAL)));
	CShaderLib::SetGlobalVarInContext(sm_gvDirectionalLight, lightDirectionAndMult); 

	Vector4 dirLightCol;
	dirLightCol.SetVector3ClearW(dirLight.GetColor() * (dirLight.GetIntensity() * multiplier * ((enableDirectional) ? 1.0f : 0.0f)));
	BANK_ONLY(DebugLighting::OverrideDirectional(&dirLightCol));
	CShaderLib::SetGlobalVarInContext(sm_gvDirectionalColor, RCC_VEC4V(dirLightCol));
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::SetAmbientLightingGlobals(const float multiplier, float artificialIntAmbientScale, float artificialExtAmbientScale)
{
	const s32 bufferIdx = GetCurrentBufferIndex();

	Vector4 values[7];
	values[0] = m_NaturalAmbientDown[bufferIdx];
	values[1] = m_NaturalAmbientBase[bufferIdx];
	values[2] = m_ArtificialIntAmbientDown[bufferIdx];
	values[3] = m_ArtificialIntAmbientBase[bufferIdx];
	values[4] = m_ArtificialExtAmbientDown[bufferIdx];
	values[5] = m_ArtificialExtAmbientBase[bufferIdx];
	values[6] = m_DirectionalAmbient[bufferIdx];

	SetAmbientLightingGlobals(multiplier, artificialIntAmbientScale, artificialExtAmbientScale, values);
}

void CLightGroup::SetAmbientLightingGlobals(const float multiplier, float artificialIntAmbientScale, float artificialExtAmbientScale, Vector4* values)
{
	// AJH_TODO check context use
	const Vector4 mult = Vector4(multiplier, multiplier, multiplier, 1.0f);
	const Vector4 multArtificialIntAmbient = Vector4(artificialIntAmbientScale * multiplier, artificialIntAmbientScale * multiplier, artificialIntAmbientScale * multiplier, 1.0f);
	const Vector4 multArtificialExtAmbientScale = Vector4(artificialExtAmbientScale * multiplier, artificialExtAmbientScale * multiplier, artificialExtAmbientScale * multiplier, 1.0f);

	Vector4 amb[2];

	// Natural Ambient
	amb[0]  = values[0] * mult;
	amb[1]  = values[1] * mult;
	BANK_ONLY(DebugLighting::OverrideAmbient(amb, AMBIENT_OVERRIDE_NATURAL));

	Assert(grcEffect::AreGlobalVarsAdjacent(sm_gvLightNaturalAmbient0, sm_gvLightNaturalAmbient1));
	CShaderLib::SetGlobalVarInContext(sm_gvLightNaturalAmbient0, (Vec4V*)amb, 2);

	// Artificial Interior Ambient
	amb[0]  = values[2] * multArtificialIntAmbient;
	amb[1]  = values[3] * multArtificialIntAmbient;
	BANK_ONLY(DebugLighting::OverrideAmbient(amb, AMBIENT_OVERRIDE_ARTIFICLAL_INTERIOR));

	Assert(grcEffect::AreGlobalVarsAdjacent(sm_gvLightArtificialIntAmbient0, sm_gvLightArtificialIntAmbient1));
	CShaderLib::SetGlobalVarInContext(sm_gvLightArtificialIntAmbient0, (Vec4V*)amb, 2);

	// Artificial Exterior Ambient
	amb[0]  = values[4] * multArtificialExtAmbientScale;
	amb[1]  = values[5] * multArtificialExtAmbientScale;
	BANK_ONLY(DebugLighting::OverrideAmbient(amb, AMBIENT_OVERRIDE_ARTIFICIAL_EXTERIOR));

	Assert(grcEffect::AreGlobalVarsAdjacent(sm_gvLightArtificialExtAmbient0, sm_gvLightArtificialExtAmbient1));
	CShaderLib::SetGlobalVarInContext(sm_gvLightArtificialExtAmbient0, (Vec4V*)amb, 2);

	const tcKeyframeUncompressed& currKeyframe = CSystem::IsThisThreadId(SYS_THREAD_RENDER) ? g_timeCycle.GetCurrRenderKeyframe() : g_timeCycle.GetCurrUpdateKeyframe();
	const float ambientBakeRamp = currKeyframe.GetVar(TCVAR_LIGHT_AMBIENT_BAKE_RAMP);

	// Directional ambient
	Vector4 directionalAmb = values[6];
	directionalAmb.SetW(ambientBakeRamp);
	BANK_ONLY(DebugLighting::OverrideAmbient(&directionalAmb, AMBIENT_OVERRIDE_DIRECTIONAL));
	CShaderLib::SetGlobalVarInContext(sm_gvDirectionalAmbientID, RCC_VEC4V(directionalAmb));
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::DisableForwardLights()
{
	CShaderLib::SetGlobalVarInContext(sm_giNumForwardLightsID, 0);
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::SetForwardLights(const CLightSource* lights, int numLights)
{
	const CLightSource* lightPtrs[8];
	for (int i = 0; i < numLights; i++)
	{
		lightPtrs[i] = &(lights[i]);
	}
	SetForwardLights(lightPtrs, numLights);
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::SetForwardLights(const CLightSource** lights, int numLights)
{
	Vec4V lightPositionAndInvDistSqr[8];
	Vec4V lightDirectionAndFalloffExponent[8];
	Vec4V lightColourAndCapsuleExtent[8];
	float lightConeScale[8];
	float lightConeOffset[8];
	int   shadowIndices[8];

	for (int i = 0; i < numLights; i++)
	{
		Vector3 adjustedPos = lights[i]->GetPosition() - lights[i]->GetDirection() * lights[i]->GetCapsuleExtent() * 0.5f;
		float radius = lights[i]->GetRadius();
		float invSqrRadius = (radius > 0.0f) ? 1.0f / (radius*radius) : 0.0f;

		Vector3 direction = lights[i]->GetDirection();
				
		Vector3 colourTimesIntensity = lights[i]->GetColor() * lights[i]->GetIntensity();
		Assert(sm_gvLocalLightForwardIntensityScale.Getf() == 1.0f || DRAWLISTMGR->IsExecutingMirrorReflectionDrawList());
		colourTimesIntensity *= sm_gvLocalLightForwardIntensityScale.Getf();

		float coneScale, coneOffset;
		calculateSpotLightScaleBias(lights[i], coneScale, coneOffset);
		float capsuleExtent = lights[i]->GetCapsuleExtent();
		float falloffExponent = lights[i]->GetFalloffExponent();
		
		lightPositionAndInvDistSqr[i] = Vec4V(VECTOR3_TO_VEC3V(adjustedPos), ScalarV(invSqrRadius));
		lightDirectionAndFalloffExponent[i] = Vec4V(VECTOR3_TO_VEC3V(direction), ScalarV(falloffExponent));
		lightColourAndCapsuleExtent[i] = Vec4V(VECTOR3_TO_VEC3V(colourTimesIntensity), ScalarV(capsuleExtent));
		lightConeScale[i] = coneScale;
		lightConeOffset[i] = coneOffset;

		shadowIndices[i] = CParaboloidShadow::GetLightShadowIndex(*(lights[i]), false);
	}

	CShaderLib::SetGlobalVarInContext(sm_gLightPositionAndInvDistSqr, lightPositionAndInvDistSqr, numLights);
	CShaderLib::SetGlobalVarInContext(sm_gLightDirectionAndFalloffExponent, lightDirectionAndFalloffExponent, numLights);
	CShaderLib::SetGlobalVarInContext(sm_gLightColourAndCapsuleExtent, lightColourAndCapsuleExtent, numLights);

	CShaderLib::SetGlobalVarInContext(sm_gLightConeScale, lightConeScale, numLights);
	CShaderLib::SetGlobalVarInContext(sm_gLightConeOffset, lightConeOffset, numLights);

	CShaderLib::SetGlobalVarInContext(sm_giNumForwardLightsID, numLights);
	CParaboloidShadow::SetForwardLightShadowData(shadowIndices, numLights);
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::SetLocalLightingGlobals( bool sortLights, bool forceSetAll8Lights )
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	// Setup local lights
	if (sortLights == true && needsSorting == true)
	{
		std::sort(m_PerThreadData[g_RenderThreadIndex].m_LightSources, m_PerThreadData[g_RenderThreadIndex].m_LightSources + MAX_RENDERED_LIGHTS, LightSorter());
		needsSorting = false;
	}

#if __ASSERT
	for ( int i = 0; i < m_PerThreadData[g_RenderThreadIndex].m_ActiveCount; i++ )
	{
		Assert( m_PerThreadData[g_RenderThreadIndex].m_LightSources[i] != 0 );
	}
#endif // __ASSERT

	const CLightSource** lights = m_PerThreadData[g_RenderThreadIndex].m_LightSources;

	if (!Lights::s_UseLightweightTechniques || m_PerThreadData[g_RenderThreadIndex].m_ActiveCount > 0)
	{
		SetForwardLights(lights, m_PerThreadData[g_RenderThreadIndex].m_ActiveCount);
	}
	else if (forceSetAll8Lights)
	{
		SetForwardLights(lights, 8);
	}
}

// ----------------------------------------------------------------------------------------------- //

// reset the Lighting Globals
void CLightGroup::ResetLightingGlobals()
{ 
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	SetDirectionalLightingGlobals();
	SetAmbientLightingGlobals(); 
	SetLocalLightingGlobals(); 
}

// ----------------------------------------------------------------------------------------------- //

void CLightGroup::SetFakeLighting(float ambientScalar, float directionalScalar, const Matrix34 &camMtx)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	
	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();

	SetAmbientLightingGlobals(ambientScalar);

	// Directional light
	const CLightSource &dirLight = Lights::GetRenderDirectionalLight();
	Vector3 dir;
	camMtx.Transform3x3( dirLight.GetDirection(), dir );
	Vector4 dirAndMult = Vector4(dir);
	dirAndMult.SetW(currKeyframe.GetVar(TCVAR_REFLECTION_TWEAK_DIRECTIONAL));
	grcEffect::SetGlobalVar(sm_gvDirectionalLight, RCC_VEC4V(dirAndMult)); 

	Vector4 dirLightCol;
	dirLightCol.SetVector3ClearW(dirLight.GetColor() * (directionalScalar * (dirLight.GetIntensity() > 0.0f)));
	BANK_ONLY(DebugLighting::OverrideDirectional(&dirLightCol));
	CShaderLib::SetGlobalVarInContext(sm_gvDirectionalColor, RCC_VEC4V(dirLightCol));
}

// ----------------------------------------------------------------------------------------------- //
