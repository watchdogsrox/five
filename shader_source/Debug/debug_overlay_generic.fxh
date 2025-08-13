// ===============================
// Debug/debug_overlay_generic.fxh
// (c) 2012 RockstarNorth
// ===============================

#if !defined(SHADER_FINAL)

#include "debug_overlay.fxh"

float4 PS_DebugOverlay_BANK_ONLY_GENERIC(vertexOutput IN) : COLOR
{
	vertexOutputDebugOverlay v = (vertexOutputDebugOverlay)0;

	v.worldPos = IN.worldPos.xyz;
	v.c0 = 0;
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
	v.uv0 = 0;
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD

	return PS_DebugOverlay_internal(v);
}

technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_Transform();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY_GENERIC() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

DRAWSKINNED_TECHNIQUES_ONLY(technique debugoverlay_drawskinned { pass p0
{
	VertexShader = compile VERTEXSHADER VS_Transform();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY_GENERIC() CGC_FLAGS(CGC_DEFAULTFLAGS);
}})

#endif // !defined(SHADER_FINAL)
