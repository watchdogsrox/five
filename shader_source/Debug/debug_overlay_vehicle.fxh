// ===============================
// Debug/debug_overlay_vehicle.fxh
// (c) 2012 RockstarNorth
// ===============================

#if !defined(SHADER_FINAL)

#include "../Vehicles/vehicle_common.fxh"

#if !defined(NO_DIFFUSE_TEXTURE)
	#define DEBUG_OVERLAY_DIFF_SAMPLER DiffuseSampler
#endif

#if NORMAL_MAP
	#define DEBUG_OVERLAY_BUMP_SAMPLER BumpSampler
#endif

#include "debug_overlay.fxh"

#if UMOVEMENTS
	#define vehicleVertexInputBump_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define vehicleVertexInputBump_CPV1_SWITCH(_if_,_else_) _else_
#endif

#if NONSKINNED_VEHICLE_TECHNIQUES
vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_VEHICLE(vehicleVertexInputBump IN)
{
	const vehicleVertexOutput OUT = VS_VehicleTransform(IN);
	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		vehicleVertexInputBump_CPV1_SWITCH(IN.specular, 0),
		OUT.texCoord.xy,
		OUT.worldNormal,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);
}
#endif // NONSKINNED_VEHICLE_TECHNIQUES

DRAWSKINNED_TECHNIQUES_ONLY(vertexOutputDebugOverlay VS_DebugOverlay_BANK_ONLY_skinned_VEHICLE(vehicleSkinVertexInputBump IN)
{
	const vehicleVertexOutput OUT = VS_VehicleTransformSkin(IN);

	return VS_DebugOverlay_internal(
		OUT.pos,
		OUT.worldPos,
		IN.diffuse,
		vehicleVertexInputBump_CPV1_SWITCH(IN.specular, 0),
		OUT.texCoord.xy,
		OUT.worldNormal,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);
})

#if NONSKINNED_VEHICLE_TECHNIQUES
technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_VEHICLE();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}
#endif // NONSKINNED_VEHICLE_TECHNIQUES

DRAWSKINNED_TECHNIQUES_ONLY(technique debugoverlay_drawskinned { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_skinned_VEHICLE();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}})

#if defined(DO_WHEEL_ROUNDING_TESSELLATION) && NONSKINNED_VEHICLE_TECHNIQUES

[domain("tri")]
vertexOutputDebugOverlay DS_WR_DebugOverlay_BANK_ONLY_VEHICLE( patchConstantInfo PatchInfo, float3 UVW : SV_DomainLocation, const OutputPatch<vehicleVertexInputBump_CtrlPoint, 3> PatchPoints )
{
	vehicleVertexInputBump IN = DS_WR_vehicleVertexInputBump(PatchInfo, UVW, PatchPoints);
	return VS_DebugOverlay_BANK_ONLY_VEHICLE(IN);
}

technique debugoverlay_draw_sm50  { pass p0
{
	VertexShader = compile VSDS_SHADER VS_WR_vehicleVertexInputBump();
	SetHullShader(compileshader(hs_5_0, HS_WRND_vehicleVertexInputBump()));
	SetDomainShader(compileshader(ds_5_0, DS_WR_DebugOverlay_BANK_ONLY_VEHICLE()));
	PixelShader = compile PIXELSHADER PS_DebugOverlay_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}

#endif // defined(DO_WHEEL_ROUNDING_TESSELLATION) && NONSKINNED_VEHICLE_TECHNIQUES

#endif // !defined(SHADER_FINAL)
