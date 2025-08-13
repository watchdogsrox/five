#if defined(SHADER_FINAL)

#define SHADERTECHNIQUE_ENTITY_SELECT(VS_draw, VS_drawskinned)
#define SHADERTECHNIQUE_ENTITY_SELECT_SKINNED_ONLY(VS_drawskinned)

#else // not defined(SHADER_FINAL)

#include "../common.fxh"

#define ENTITY_SELECT_FXH

#ifndef ES_DEBUG_TILING
#	define ES_DEBUG_TILING 0
#endif

#ifndef ES_ALPHACLIP_FUNC
#	define ES_ALPHACLIP_FUNC(IN)
#	define ES_ALPHACLIP_VS_OUT
#	define ES_ALPHACLIP_VS_OUT_VAR
#else
#	ifdef ES_ALPHACLIP_FUNC_INPUT
#		if ES_DEBUG_TILING
#			define ES_ALPHACLIP_VS_OUT ES_ALPHACLIP_FUNC_INPUT IN,
#		else
#			define ES_ALPHACLIP_VS_OUT ES_ALPHACLIP_FUNC_INPUT IN
#		endif
#		define ES_ALPHACLIP_VS_OUT_VAR IN
#	else
#		define ES_ALPHACLIP_VS_OUT
#		define ES_ALPHACLIP_VS_OUT_VAR 0.0f
#	endif
#endif

float4 GetEntitySelectID()
{
	//Overlapping registers seems to cause an error when building the shaders in the main project. Not sure why.
#if !__WIN32PC || __SHADERMODEL >= 40
	return gEntitySelectColor[0];
#else
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
#endif
}


struct EntitySelectID_Out
{
	half4	col0	: SV_Target0;
#if ENTITYSELECT_NO_OF_PLANES > 1
	half4	col1	: SV_Target1;
#endif // ENTITYSELECT_NO_OF_PLANES > 1
};


EntitySelectID_Out GetEntitySelectID_Out()
{
	EntitySelectID_Out ret;

#if !__WIN32PC || __SHADERMODEL >= 40
	ret.col0 = gEntitySelectColor[0];
#if ENTITYSELECT_NO_OF_PLANES > 1
	ret.col1 = gEntitySelectColor[1];
#endif // ENTITYSELECT_NO_OF_PLANES > 1

#else
	ret.col0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
#if ENTITYSELECT_NO_OF_PLANES > 1
	ret.col1 = float4(1.0f, 1.0f, 1.0f, 1.0f);
#endif // ENTITYSELECT_NO_OF_PLANES > 1

#endif

	return ret;
}

float GetEntitySelectClip()
{
	bool clipEntityId = dot(step(GetEntitySelectID(),0.0f.xxxx), 1.0f.xxxx) > 0.0f;
	return (clipEntityId ? -1.0f : 1.0f);
}

EntitySelectID_Out PS_EntitySelectID_BANK_ONLY(	ES_ALPHACLIP_VS_OUT
#	if ES_DEBUG_TILING
									float2 vPos : VPOS
#	endif
									)
{
	clip(GetEntitySelectClip());
	ES_ALPHACLIP_FUNC(ES_ALPHACLIP_VS_OUT_VAR);
	EntitySelectID_Out esId = GetEntitySelectID_Out();

#if ES_DEBUG_TILING
	if(dot(esId > 1.0f.xxxx, 1.0f.xxxx) > 0.0f)
	{
		float2 resolveStartPixel = GetEntitySelectID().xy;
		float2 rtSize = GetEntitySelectID().zw; //gScreenSize.xy;
		float2 pixelPos = vPos;

		//pixelPos is in pixel space. (0 - width/height)
		float2 offsetPixel = pixelPos - resolveStartPixel;

		//green = 2D row index & blue = 2D column index.
		esId = float4(0.0f, (offsetPixel.yx / 255.0f.xx), 1.0f);
	}
#endif

	return esId;
}

// ================================================================================================

#define SHADERTECHNIQUE_ENTITY_SELECT(VS_draw, VS_drawskinned) \
	technique entityselect_draw { pass p0 \
	{ \
		AlphaBlendEnable = false; \
		AlphaTestEnable  = false; \
		ZFunc			 = FixedupLESSEQUAL; \
		VertexShader = compile VERTEXSHADER VS_draw; \
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS); \
	}} \
	DRAWSKINNED_TECHNIQUES_ONLY(technique entityselect_drawskinned { pass p0 \
	{ \
		AlphaBlendEnable = false; \
		AlphaTestEnable  = false; \
		ZFunc			 = FixedupLESSEQUAL; \
		VertexShader = compile VERTEXSHADER VS_drawskinned; \
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS); \
	}})

#define SHADERTECHNIQUE_ENTITY_SELECT_SKINNED_ONLY(VS_drawskinned) \
	DRAWSKINNED_TECHNIQUES_ONLY(technique entityselect_drawskinned { pass p0 \
	{ \
		AlphaBlendEnable = false; \
		AlphaTestEnable  = false; \
		ZFunc			 = FixedupLESSEQUAL; \
		VertexShader = compile VERTEXSHADER VS_drawskinned; \
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS); \
	}})

#endif // !defined(SHADER_FINAL)
