// ============================
// Debug/debug_overlay_tree.fxh
// (c) 2012 RockstarNorth
// =============================

#if !defined(SHADER_FINAL)

#include "../Vegetation/Trees/trees_common.fxh"

#if NORMAL_MAP
	#define DEBUG_OVERLAY_BUMP_SAMPLER BumpSampler
#endif

#include "debug_overlay.fxh"

#if TREE_LOD

#if PALETTE_TINT
	#define treeVertexInputLodD_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define treeVertexInputLodD_CPV1_SWITCH(_if_,_else_) _else_
#endif

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_TREE(treeInstancedVertexInputD instIN)
{
	int nInstIndex;
	treeVertexInputD IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	TreeVertInstanceData inst;
	GetTreeInstanceData(nInstIndex, inst);

	return VS_DebugOverlay_internal(
		ApplyCompositeWorldTransform(float4(IN.Position, 1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj),
		mul(float4(IN.Position, 1), inst.worldMtx).xyz,
		0, // cpv0
		treeVertexInputLodD_CPV1_SWITCH(IN.Specular, 0),
		IN.TexCoord0.xy,
		mul(IN.Normal, (float3x3)inst.worldMtx),
		NORMAL_MAP_SWITCH(mul(IN.Tangent.xyz, (float3x3)inst.worldMtx), 0)
	);
}

#elif TREE_LOD2

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_TREE(treeInstancedVertexInput2D instIN)
{
	int nInstIndex;
	treeVertexInputLod2D IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	TreeVertInstanceData inst;
	GetTreeInstanceData(nInstIndex, inst);

	const float3 vx = gViewInverse[0].xyz*IN.TexCoord1.x*treeLod2Width;
	const float3 vy = gViewInverse[1].xyz*IN.TexCoord1.y*treeLod2Height;
	const float3 wp = (IN.TexCoord0.x - 0.5)*vx - (IN.TexCoord0.y - 0.5)*vy + IN.Position;
	const float3 lp = mul((float3x3)inst.worldMtx, wp);
	return VS_DebugOverlay_internal(
		ApplyCompositeWorldTransform(float4(lp, 1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj),
		wp + inst.worldMtx[3].xyz,
		IN.Diffuse,
		0, // cpv1
		IN.TexCoord0.xy,
		mul(IN.Normal, (float3x3)inst.worldMtx),
		0 // tangent
	);
}

#else // full micromovements

#if PALETTE_TINT
	#define treeVertexInputD_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define treeVertexInputD_CPV1_SWITCH(_if_,_else_) _else_
#endif

vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_TREE(treeInstancedVertexInputD instIN)
{
	int nInstIndex;
	treeVertexInputD IN;
	FetchTreeInstanceVertexData(instIN, IN, nInstIndex);

	TreeVertInstanceData inst;
	GetTreeInstanceData(nInstIndex, inst);

	float3 pos = IN.Position.xyz;
	pos	= TreesApplyMicromovementsInternal(IN, inst.worldMtx, false);
	pos	= TreesApplyCollisionInternal(pos, IN.Diffuse.rgb, inst.worldMtx, inst.invEntityScaleXY, inst.invEntityScaleZ);
	return VS_DebugOverlay_internal(
		ApplyCompositeWorldTransform(float4(pos, 1), INSTANCING_ONLY_ARG(inst.worldMtx) gWorldViewProj),
		mul(float4(pos, 1), inst.worldMtx).xyz,
		treeVertexInputD_CPV1_SWITCH(IN.Specular, 0),
		IN.Diffuse,
		IN.TexCoord0.xy,
	#if !IS_SHADOW_PROXY
		mul(IN.Normal, (float3x3)inst.worldMtx),
	#else
		float3(0.0f, 0.0f, 0.0f),
	#endif
		NORMAL_MAP_SWITCH(mul(IN.Tangent.xyz, (float3x3)inst.worldMtx), 0)
	);
}

#endif // full micromovements

technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_TREE();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

DRAWSKINNED_TECHNIQUES_ONLY(technique debugoverlay_drawskinned { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_TREE();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}})

#if USE_PN_TRIANGLES && !TREE_LOD && !TREE_LOD2

[domain("tri")]
vertexOutputDebugOverlay DS_DebugOverlay_PNTri_BANK_ONLY_TREE(rage_PNTri_PatchInfo PatchInfo, float3 WUV : SV_DomainLocation, const OutputPatch<treeVertexInputD_CtrlPoint, 3> PatchPoints )
{
	treeVertexInputD IN = PropFoliageDeferred_PNTri_EvaluateAt(PatchInfo, WUV, PatchPoints);
	return VS_DebugOverlay_BANK_ONLY_TREE(IN);
}

technique debugoverlay_draw_sm50 { pass p0
{
	VertexShader = compile VSDS_SHADER VS_PropFoliageDeferred_PNTri();
	SetHullShader(compileshader(hs_5_0, HS_PropFoliageDeferred_PNTri()));
	SetDomainShader(compileshader(ds_5_0, DS_DebugOverlay_PNTri_BANK_ONLY_TREE()));
	PixelShader = compile PIXELSHADER PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

#endif // USE_PN_TRIANGLES && !TREE_LOD && !TREE_LOD2

#endif // !defined(SHADER_FINAL)
