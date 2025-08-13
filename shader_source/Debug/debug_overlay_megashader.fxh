// ==================================
// Debug/debug_overlay_megashader.fxh
// (c) 2012 RockstarNorth
// ==================================

#if !defined(SHADER_FINAL)

//#include "../Megashader/megashader.fxh"

#if !defined(NO_DIFFUSE_TEXTURE)
	#define DEBUG_OVERLAY_DIFF_SAMPLER DiffuseSampler
#endif

#if NORMAL_MAP
	#define DEBUG_OVERLAY_BUMP_SAMPLER BumpSampler
#endif

#if SPEC_MAP
	#define DEBUG_OVERLAY_SPEC_SAMPLER SpecSampler
#endif

#include "debug_overlay.fxh"

#if PALETTE_TINT_EDGE || UMOVEMENTS
	#define northVertexInputBump_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define northVertexInputBump_CPV1_SWITCH(_if_,_else_) _else_
#endif

#if PED_CPV_WIND || UMOVEMENTS || PALETTE_TINT_EDGE || PEDSHADER_FAT
	#define northSkinVertexInputBump_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define northSkinVertexInputBump_CPV1_SWITCH(_if_,_else_) _else_
#endif

#if DIFFUSE_TEXTURE
	#define DIFFUSE_TEXTURE_SWITCH(_if_,_else_) _if_
#else
	#define DIFFUSE_TEXTURE_SWITCH(_if_,_else_) _else_
#endif

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_MEGASHADER_Base(northVertexInputBump IN, megaVertInstanceData inst, int nInstIndex)
{
	const vertexOutput OUT = VS_Transform_Base(IN, inst, nInstIndex);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		northVertexInputBump_CPV1_SWITCH(IN.specular, 0),
		DIFFUSE_TEXTURE_SWITCH(OUT.texCoord.xy, 0),
		OUT.worldNormal,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);
}

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_MEGASHADER(northInstancedVertexInputBump instIN)
{
	int nInstIndex;
	northVertexInputBump IN;
	FetchInstanceVertexData(instIN, IN, nInstIndex);

	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	return VS_DebugOverlay_BANK_ONLY_MEGASHADER_Base(IN, inst, nInstIndex);
}

DRAWSKINNED_TECHNIQUES_ONLY(vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_skinned_MEGASHADER(northSkinVertexInputBump IN)
{
	const vertexOutput OUT = VS_TransformSkin(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		northSkinVertexInputBump_CPV1_SWITCH(IN.specular, 0),
		DIFFUSE_TEXTURE_SWITCH(OUT.texCoord.xy, 0),
		OUT.worldNormal,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);
})

technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_MEGASHADER();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

DRAWSKINNED_TECHNIQUES_ONLY(technique debugoverlay_drawskinned { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_skinned_MEGASHADER();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}})

#if USE_PN_TRIANGLES

[domain("tri")]
vertexOutputDebugOverlay DS_DebugOverlay_BANK_ONLY_MEGASHADER(rage_PNTri_PatchInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<northVertexInputBump_CtrlPoint, 3> PatchPoints)
{
	int nInstIndex = PNTri_EvaluateInstanceID(WUV, PatchPoints);
	megaVertInstanceData inst;
	GetInstanceData(nInstIndex, inst);

	northVertexInputBump IN = northVertexInputBump_PNTri_EvaluateAt(PatchInfo, WUV, PatchPoints);
	return VS_DebugOverlay_BANK_ONLY_MEGASHADER_Base(IN, inst, nInstIndex);
}

SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique debugoverlay_drawtessellated { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_MEGASHADER();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}},
technique SHADER_MODEL_50_OVERRIDE(debugoverlay_drawtessellated) { pass p0
{
	VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
	SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
	SetDomainShader(compileshader(ds_5_0, DS_DebugOverlay_BANK_ONLY_MEGASHADER()));
	PixelShader = compile PIXELSHADER PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}})

DRAWSKINNED_TECHNIQUES_ONLY(SHADER_MODEL_50_OVERRIDE_TECHNIQUES(technique debugoverlay_drawskinnedtessellated { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_skinned_MEGASHADER();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}},
technique SHADER_MODEL_50_OVERRIDE(debugoverlay_drawskinnedtessellated) { pass p0
{
	VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
	SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
	SetDomainShader(compileshader(ds_5_0, DS_DebugOverlay_BANK_ONLY_MEGASHADER()));
	PixelShader = compile PIXELSHADER PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}))

#endif // USE_PN_TRIANGLES

#endif // !defined(SHADER_FINAL)
