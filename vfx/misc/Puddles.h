///////////////////////////////////////////////////////////////////////////////

#ifndef PUDDLES_H
#define PUDDLES_H

#include "grcore/effect_typedefs.h"
#include "grcore/texture.h"
#include "atl/singleton.h"
#include "grmodel/shader.h"
#include "vfx/vfx_shared.h"
// ----------------------------------------------------------------------------------------------- //

//fwd ref
class Ripples;
class SplashQueries;


struct SplashData
{
	Vec3V	position;
	float	vel;
	SplashData() {}
	SplashData( Vec3V_In p, float v ) : position(p), vel(v) {}
};

typedef void (*SplashCallback)(const SplashData& spdData );

class PuddlePass
{
	
public:

	PuddlePass();
	~PuddlePass();
	const grcTexture* GetRippleTexture();
	grcEffectTechnique GetTechnique() const;
	void Draw(grmShader* shader, bool generateRipples = true );
	bool IsEnabled() { return scale > 0.0f; }

	// Adds a player ripple at a given location. Size is dependant on velocity.
	// if velocity is very low will randomly create it or not.
	void AddPlayerRipple( Vec3V_In rippleLocation, float velocity );
#if __BANK
	void InitWidgets();
#endif

	void AddSplashCallback( SplashCallback cb) { m_callbacks.Append()  = cb; }
	bool GenerateRipples( grmShader* shader, SplashData& testLocation);
	void GetQueriesResults();

	static void UpdateVisualDataSettings();

	void ResetShaders() { m_PuddleBumpTextureLocalVar=grcevNONE; }

private:  

	void SetShaderVars( grmShader* shader );
	void GetSettings();
	grcEffectVar m_PuddleBumpTextureLocalVar;
	grcEffectVar m_Puddle_ScaleXY_RangeLocalVar;
	grcEffectVar m_PuddleParamsLocalVar;
	grcEffectVar m_PuddleMaskTextureLocalVar;
	grcEffectVar m_AOTextureLocalVar;
	grcEffectVar m_GBufferTexture3ParamLocalVar;

	grcEffectTechnique m_puddleTechnique;
	grcEffectTechnique m_puddleTestTechnique;
	BANK_ONLY(grcEffectTechnique m_puddleDebugTechnique);
	grcEffectTechnique m_puddleMaskTechnique;
#if USE_COMBINED_PUDDLEMASK_PASS
	grcEffectTechnique m_puddleMaskPassCombinedTechnique;
#endif

	atFixedArray<SplashCallback,8>			m_callbacks;

	float scale;
	float range;

	float depth;
	float depthTest;
	float reflectivity;
	float reflectTransitionPoint;
	bool useRippleGen;
	bool usePuddleMask;
	float minAlpha;
	float aoCutoff;
	BANK_ONLY(float m_wetness;)

	grcRasterizerStateHandle  m_rastDepthBias_RS;
	grcDepthStencilStateHandle m_StencilOutInterior_DS;
	grcDepthStencilStateHandle m_StencilSetupMask_DS;	
	grcDepthStencilStateHandle m_StencilUseMask_DS;
	grcBlendStateHandle			m_StencilSetupMask_BS;
	grcBlendStateHandle			m_applyPuddles_BS;
	Ripples*					m_ripples;
	SplashQueries*				m_splashes;
};


typedef atSingleton<PuddlePass> PuddlePassSingleton;

#endif
