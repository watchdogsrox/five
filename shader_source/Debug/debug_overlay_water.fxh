// =============================
// Debug/debug_overlay_water.fxh
// (c) 2012 RockstarNorth
// ==============================

#if !defined(SHADER_FINAL)

//#include "../Water/water.fx"
//#include "../Water/water_common.fxh"

#if defined(IS_OCEAN)

#define DEBUG_OVERLAY_DEPTH_BUFFER depthBuffer // we can't use ReflectionSampler for the depth buffer here, so define a new one
#include "debug_overlay.fxh"

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_WATER(WATER_VSINPUT IN)
{
	const WATER_VSOUTPUT OUT = VS_Water(IN);
	return VS_DebugOverlay_internal(
		OUT.Position,
		OUT.PSWorldPos,
		0, // cpv0
		0, // cpv1
		OUT.TexCoord.xy,
		float3(0,0,1), // norm
		0 // tangent
	);
}

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_WATER_TESS(WATERTESS_VSINPUT IN)
{
	const WATER_VSOUTPUT OUT = VS_WaterTess(IN);
	return VS_DebugOverlay_internal(
		OUT.Position,
		OUT.PSWorldPos,
		0, // cpv0
		0, // cpv1
		OUT.TexCoord.xy,
		float3(0,0,1), // normal
		0 // tangent
	);
}

technique debugoverlay_water_ocean
{
	pass p0 // not tessellated
	{
		VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_WATER();
		PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // tessellated
	{
		VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_WATER_TESS();
		PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#else // not defined(IS_OCEAN)

#include "debug_overlay.fxh"

#if defined(IS_FOUNTAIN)
	#define FOUNTAIN_SWITCH(_if_,_else_) _if_
#else
	#define FOUNTAIN_SWITCH(_if_,_else_) _else_
#endif

#if !defined(IS_FOUNTAIN) && !defined(IS_FOAM) && !defined(IS_RIVERSHALLOW) && !defined(IS_RIVEROCEAN)
	#define VERTEX_NORMAL_SWITCH(_if_,_else_) _if_
#else
	#define VERTEX_NORMAL_SWITCH(_if_,_else_) _else_
#endif

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_WATER(VS_INPUT IN)
{
	const VS_OUTPUT OUT = VS(IN);
	return VS_DebugOverlay_internal(
		OUT.Position,
		OUT.PSWorldPos,
		0, // cpv0
		0, // cpv1
		FOUNTAIN_SWITCH(0, OUT.PSTexCoord),
		VERTEX_NORMAL_SWITCH(IN.Normal, 0),
		0 // tangent
	);
}

technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_WATER();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

#endif // not defined(IS_OCEAN)
#else // defined(SHADER_FINAL)
#if defined(IS_OCEAN)
	// Simply declare it to avoid constant/sampler order changes
	DECLARE_SAMPLER(sampler2D, DEBUG_OVERLAY_DEPTH_BUFFER, depthBufferSamp,
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR;
	);
	#define DEBUG_OVERLAY_DEPTH_BUFFER_SAMPLER depthBufferSamp
#endif // defined(IS_OCEAN)

#endif // !defined(SHADER_FINAL)
