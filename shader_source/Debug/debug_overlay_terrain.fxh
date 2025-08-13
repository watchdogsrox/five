// ===============================
// Debug/debug_overlay_terrain.fxh
// (c) 2012 RockstarNorth
// ===============================

#if !defined(SHADER_FINAL)

//#include "../Terrain/terrain_cb_common.fxh"

#include "debug_overlay.fxh"

#if !USE_TEXTURE_LOOKUP	|| USE_DOUBLE_LOOKUP || PALETTE_TINT_EDGE
	#define vertexInput_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define vertexInput_CPV1_SWITCH(_if_,_else_) _else_
#endif

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_TERRAIN(vertexInput IN)
{
	const vertexOutput OUT = VS_Transform(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		vertexInput_CPV1_SWITCH(IN.specular, 0),
		OUT.texCoord.xy,
		OUT.worldNormal,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);
}

#if TERRAIN_TESSELLATION_SHADER_CODE
[domain("tri")]
vertexOutputDebugOverlay DS_DebugOverlay_BANK_ONLY(PatchInfo_TerrainTessellation PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<vertexOutputDeferred_CtrlPoint, 3> PatchPoints)
{
	float3 modelPosition;
	vertexOutputDeferred OUT = DS_TransformDeferred_Tess_Func(PatchInfo, PatchPoints, WUV, modelPosition);
	float3 worldPosition = mul(float4(modelPosition, 1), gWorld).xyz;

	return VS_DebugOverlay_internal(
		OUT.pos,
		worldPosition,
		float4(1.0f, 1.0f, 1.0f, 1.0f),
		vertexInput_CPV1_SWITCH(float4(1.0f, 1.0f, 1.0f, 1.0f), 0),
		OUT.texCoord.xy,
		OUT.worldNormal,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);
}
#endif // TERRAIN_TESSELLATION_SHADER_CODE


technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_TERRAIN();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}
TERRAIN_TESSELLATION_SHADER_CODE_ONLY(SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique debugoverlay_drawtessellated { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_TERRAIN();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}},
technique SHADER_MODEL_50_OVERRIDE(debugoverlay_drawtessellated) { pass p0
{
	VertexShader = compile VSDS_SHADER	VS_TransformDeferred_Tess();
	SetHullShader(compileshader(hs_5_0, HS_TransformDeferred_Tess()));
	SetDomainShader(compileshader(ds_5_0, DS_DebugOverlay_BANK_ONLY()));
	PixelShader = compile PIXELSHADER PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}))

#endif // !defined(SHADER_FINAL)
