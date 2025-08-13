// ====================================
// Debug/debug_overlay_distance_map.fxh
// (c) 2012 RockstarNorth
// ====================================

#if !defined(SHADER_FINAL)

#include "debug_overlay.fxh"

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_DISTANCE_MAP(vertexInput IN)
{
	const vertexOutput OUT = VS_Transform(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		0, // cpv1
		OUT.texCoord.xy,
		OUT.worldNormal,
		0 // tangent
	);
}

technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_DISTANCE_MAP();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

#endif // !defined(SHADER_FINAL)
