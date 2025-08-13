// ==================================
// Debug/debug_overlay_blend_2lyr.fxh
// (c) 2012 RockstarNorth
// ==================================

#if !defined(SHADER_FINAL)

#include "debug_overlay.fxh"

#if PALETTE_TINT_EDGE
	#define vertexInput_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define vertexInput_CPV1_SWITCH(_if_,_else_) _else_
#endif

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_BLEND_2LYR(vertexInput IN)
{
	const vertexOutput OUT = VS_Transform(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		vertexInput_CPV1_SWITCH(IN.specular, 0),
		OUT.texCoord.xy,
		OUT.worldNormal,
		OUT.worldTangent.xyz
	);
}

technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_BLEND_2LYR();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

#endif // !defined(SHADER_FINAL)
