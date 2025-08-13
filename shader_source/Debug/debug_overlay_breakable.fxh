// =================================
// Debug/debug_overlay_breakable.fxh
// (c) 2012 RockstarNorth
// =================================

#if !defined(SHADER_FINAL)

#include "debug_overlay.fxh"

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_BREAKABLE(bgVertexInput IN)
{
	const bgVertexOutput OUT = DEBUG_OVERLAY_VS(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		0, // cpv1
		OUT.texCoord,
		OUT.worldNormal,
		OUT.worldTangent.xyz
	);
}

technique debugoverlay_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_BREAKABLE();
		PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_BREAKABLE();
		PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2
	{
		VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_BREAKABLE();
		PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3
	{
		VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_BREAKABLE();
		PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // !defined(SHADER_FINAL)
