// ===========================
// Debug/debug_overlay_ped.fxh
// (c) 2012 RockstarNorth
// ===========================

#if !defined(SHADER_FINAL)

#include "../Peds/ped_common.fxh"

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

#if PED_CPV_WIND || PEDSHADER_FAT
	#define pedVertexInputBump_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define pedVertexInputBump_CPV1_SWITCH(_if_,_else_) _else_
#endif

#if PED_CPV_WIND || UMOVEMENTS || PALETTE_TINT_EDGE || PEDSHADER_FAT
	#define northSkinVertexInputBump_CPV1_SWITCH(_if_,_else_) _if_
#else
	#define northSkinVertexInputBump_CPV1_SWITCH(_if_,_else_) _else_
#endif

#if DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
	#define DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD_ONLY(x) x
#else
	#define DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD_ONLY(x)
#endif

#define DEBUG_OVERLAY_PED_VERTEX_STRUCT(dstType, dst, src) \
	dstType dst; \
	dst.pos      = src.pos; \
	dst.worldPos = src.worldPos; \
	dst.c0       = src.c0; \
	DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD_ONLY(dst.uv0 = src.uv0;)

struct vertexOutputDebugOverlay_PED
{
	float3 worldPos   : TEXCOORD0;
	float4 c0         : TEXCOORD1;
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
	float2 uv0        : TEXCOORD2;
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
#if PED_DAMAGE_TARGETS
	float4 damageData : TEXCOORD3;
#endif // PED_DAMAGE_TARGETS
	DECLARE_POSITION_CLIPPLANES(pos)
};

#if NONSKINNED_PED_TECHNIQUES
vertexOutputDebugOverlay_PED VS_DebugOverlay_BANK_ONLY_PED(pedVertexInputBump IN)
{
	float3 OUT_worldPos;
	const pedVertexOutputD OUT = VS_PedTransformD0(IN, OUT_worldPos, NORMAL_MAP_ONLY(IN.tangent COMMA) IN.pos, IN.normal);
	const vertexOutputDebugOverlay temp = VS_DebugOverlay_internal(
		OUT.pos,
		OUT_worldPos,
		IN.diffuse,
		pedVertexInputBump_CPV1_SWITCH(IN.specular, 0),
		OUT.texCoord.xy,
		OUT.worldNormal.xyz,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);

	DEBUG_OVERLAY_PED_VERTEX_STRUCT(vertexOutputDebugOverlay_PED, OUT2, temp);
#if PED_DAMAGE_TARGETS
	float2 extraData;
	OUT2.damageData = CalcPedDamageData(IN.PED_DAMAGE_INPUT, extraData.x, extraData.y, true, false); // until we be based on second UV SET
#endif // PED_DAMAGE_TARGETS

 	RAGE_COMPUTE_CLIP_DISTANCES(OUT2.pos);

	return OUT2;
}
#endif // NONSKINNED_PED_TECHNIQUES

#if DRAWSKINNED_TECHNIQUES
vertexOutputDebugOverlay_PED VS_DebugOverlay_BANK_ONLY_skinned_PED(northSkinVertexInputBump IN)
{
	float3 OUT_worldPos;
	const pedVertexOutputD OUT = VS_PedTransformSkinD0(IN, OUT_worldPos, NORMAL_MAP_ONLY(IN.tangent COMMA) IN.pos, IN.normal);
	const vertexOutputDebugOverlay temp = VS_DebugOverlay_internal(
		OUT.pos,
		OUT_worldPos,
		IN.diffuse,
		northSkinVertexInputBump_CPV1_SWITCH(IN.specular, 0),
		OUT.texCoord.xy,
		OUT.worldNormal.xyz,
		NORMAL_MAP_SWITCH(OUT.worldTangent.xyz, 0)
	);

	DEBUG_OVERLAY_PED_VERTEX_STRUCT(vertexOutputDebugOverlay_PED, OUT2, temp);
#if PED_DAMAGE_TARGETS
	float2 extraData;
	OUT2.damageData = CalcPedDamageData(IN.PED_DAMAGE_INPUT, extraData.x, extraData.y, true, false); // until we be based on second UV SET
#endif // PED_DAMAGE_TARGETS

 	RAGE_COMPUTE_CLIP_DISTANCES(OUT2.pos);

	return OUT2;
}
#endif // DRAWSKINNED_TECHNIQUES

float4 PS_DebugOverlay_BANK_ONLY_PED(vertexOutputDebugOverlay_PED IN) : COLOR
{
	DEBUG_OVERLAY_PED_VERTEX_STRUCT(vertexOutputDebugOverlay, temp, IN);

	float4 OUT = PS_DebugOverlay_internal_PedDamageHack(temp,true);
	const float opacity = OUT.w;

	if (gColourFar.x == -10) // ped damage channel
	{
		OUT.w = 0;

#if PED_DAMAGE_TARGETS
		{
			float mode = gColourNear.x;
			if (mode>0)
			{
				// compute zone from scale;
				float zone = (abs(IN.damageData.w)*10.0f) + 1.0001f;//1.01f;
				if (zone>2) zone-=3; // close 0-4 gap
				if (zone>3) zone-=1; // close 4-6 gap
				
				float2 uv = frac((zone>=3) ? IN.damageData.yx : IN.damageData.xy);  // the zone is really zone + 1 so arms legs start at 3
				float2 uvgrid = pow(max(frac(-uv*16), frac(uv*16)), 16);

				float2 uvmask = float2(mode==1||mode==2, mode==3);
				float gradiant;
				if (mode==1)
					gradiant = uvmask*0.1 + 0.9*dot(uvmask, uv);
				else 
					gradiant = dot(uvmask, uv);

				float3 uvcolour = gradiant;
				uvcolour += (mode==4)*(float3(uv, 0) + float3(.25,.25,1)*max(uvgrid.x, uvgrid.y));
				float3 zoneColor = fmod(trunc(zone*float3(1,.5,.25)),2)*gradiant;
				
				if (mode==1)
				{
					// add grid lines at the .25, .5 an .75  marks, and shade so we know which is which
					uvgrid =  pow(max(frac(-uv*4), frac(uv*4)), 64);
					float gridIntensity = saturate(dot(uvgrid,1));
					float gridGradiant = gridIntensity * ((uvgrid.y>uvgrid.x)?saturate(2*uv.y-.5):1); // pow(((uvgrid.x>uvgrid.y)? uv.x : uv.y),1);
					OUT.xyz = zoneColor * (1-gridIntensity) + gridGradiant.xxx;
				}
				else
				{
					OUT.xyz = uvcolour;
				}
				OUT.w = opacity>0;
			}
		}
#endif // PED_DAMAGE_TARGETS
	}

	return OUT;
}

#if NONSKINNED_PED_TECHNIQUES
technique debugoverlay_draw { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_PED(); 
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY_PED() CGC_FLAGS(CGC_DEFAULTFLAGS);
}}
#endif // NONSKINNED_PED_TECHNIQUES

DRAWSKINNED_TECHNIQUES_ONLY(technique debugoverlay_drawskinned { pass p0
{
	VertexShader = compile VERTEXSHADER VS_DebugOverlay_BANK_ONLY_skinned_PED();
	PixelShader  = compile PIXELSHADER  PS_DebugOverlay_BANK_ONLY_PED() CGC_FLAGS(CGC_DEFAULTFLAGS);
}})

#endif // !defined(SHADER_FINAL)
