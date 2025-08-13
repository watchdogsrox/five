// =============================
// Debug/debug_overlay_cable.fxh
// (c) 2012 RockstarNorth
// ==============================

#if !defined(SHADER_FINAL)

#include "debug_overlay.fxh"

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_CABLE(cableVertexInput IN)
{
	const cableVertexOutput OUT = VS_Cable(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos.xyz,
		IN.col,
		0, // cpv1
		0, // uv0
		0, // norm
		IN.tan // tangent
	);
}

technique debugoverlay_draw { pass p0
{
	CullMode     = NONE; // this is necessary to force cables to show up in overdraw mode
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_CABLE();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

#endif // !defined(SHADER_FINAL)
