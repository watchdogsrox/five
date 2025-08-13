// =======================
// Debug/debug_overlay.fxh
// (c) 2011 RockstarNorth
// =======================

#ifndef _DEBUG_OVERLAY_FXH_
#define _DEBUG_OVERLAY_FXH_

#define DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD (0)
#define DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES (0 && DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD) // TODO -- move these features into a separate set of passes

#if defined(__GTA_COMMON_FXH)

#if NORMAL_MAP
	#define NORMAL_MAP_SWITCH(_if_,_else_) _if_
	#define NORMAL_MAP_ONLY(x) x
#else
	#define NORMAL_MAP_SWITCH(_if_,_else_) _else_
	#define NORMAL_MAP_ONLY(x)
#endif

#if __PS3 || __SHADERMODEL >= 40
	#define VPOS_ARG(name) float4 name
	#define VPOS_VAR(pos) pos.xyzw
#else
	#define VPOS_ARG(name) float2 name // vPos.zw not available
	#define VPOS_VAR(pos) pos.xy
#endif

float texSampleDepthBuffer(sampler2D samp, VPOS_ARG(vPos))
{
	const float2 texcoord = (vPos.xy + 0.5)*gooScreenSize.xy;
	return tex2D(samp, texcoord).x;
}

// ================================================================================================

#define gDepthBiasParams gDirectionalLight								// shared var - sm_gvDirectionalLight
#define gColourNear      gDirectionalColour								// shared var - sm_gvDirectionalColor
#define gColourFar       gLightNaturalAmbient0							// shared var - sm_gvLightNaturalAmbient0
#define gzNearFarVis     gLightNaturalAmbient1							// shared var - sm_gvLightNaturalAmbient1
#define gProjParam       gLightArtificialIntAmbient0					// shared var - sm_gvLightArtificialIntAmbient0

// this is required for water.fx, which gets messed up for some reason if i set the ReflectionSampler
#if defined(DEBUG_OVERLAY_DEPTH_BUFFER)
	DECLARE_SAMPLER(sampler2D, DEBUG_OVERLAY_DEPTH_BUFFER, depthBufferSamp,
		AddressU  = CLAMP;
		AddressV  = CLAMP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = LINEAR;
		MAGFILTER = LINEAR;
	);
	#define DEBUG_OVERLAY_DEPTH_BUFFER_SAMPLER depthBufferSamp
#else
	#define DEBUG_OVERLAY_DEPTH_BUFFER_SAMPLER ReflectionSampler
#endif

struct vertexOutputDebugOverlay
{
	float3 worldPos : TEXCOORD0;
	float4 c0       : TEXCOORD1;
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
	float2 uv0      : TEXCOORD2;
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
	DECLARE_POSITION_CLIPPLANES(pos)
};

float4 GetEntitySelectID();

float4 PS_DebugOverlay_internal_PedDamageHack(vertexOutputDebugOverlay IN, bool isPedDamage)
{
	const float3 camPos = +gViewInverse[3].xyz;
	const float3 camDir = -gViewInverse[2].xyz;

	const float camZ = dot(IN.worldPos - camPos, camDir);
	const VPOS_ARG(vPos) = VPOS_VAR(IN.pos);

	const float a = saturate((camZ - gzNearFarVis.x)/(gzNearFarVis.y - gzNearFarVis.x));
	float4 c = lerp(gColourNear, gColourFar, a);

#if defined(ENTITY_SELECT_FXH)
	if (gzNearFarVis.y < 0) // damn i'm clever
	{
		c = GetEntitySelectID();
	}
	else
#endif // defined(ENTITY_SELECT_FXH)
	if (IN.c0.w > 0.99999) // the opacity is either 0 or 1, somehow, an equality test on PC would fuck up and gives us shit.
	{
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
		if (gColourFar.x == -6)
		{
#if defined(DEBUG_OVERLAY_DIFF_SAMPLER)
			c.xyz = tex2D(DEBUG_OVERLAY_DIFF_SAMPLER, IN.uv0).xyz;
			c.w = 1;
#endif // defined(DEBUG_OVERLAY_DIFF_SAMPLER)
		}
		else if (gColourFar.x == -7)
		{
#if defined(DEBUG_OVERLAY_BUMP_SAMPLER)
			c.xyz = tex2D(DEBUG_OVERLAY_BUMP_SAMPLER, IN.uv0).xyz;
			c.w = 1;
#endif // defined(DEBUG_OVERLAY_BUMP_SAMPLER)
		}
		else if (gColourFar.x == -8)
		{
#if defined(DEBUG_OVERLAY_SPEC_SAMPLER)
			c.xyz = tex2D(DEBUG_OVERLAY_SPEC_SAMPLER, IN.uv0).xyz;
			c.w = 1;
#endif // defined(DEBUG_OVERLAY_SPEC_SAMPLER)
		}
		else
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
		{
			c = IN.c0;
		}
	}

	const float depth = texSampleDepthBuffer(DEBUG_OVERLAY_DEPTH_BUFFER_SAMPLER, vPos);
	const float z     = getLinearGBufferDepth(depth, gProjParam.zw);
	const float z0    = gDepthBiasParams.x; // linear depth at which depth bias = d0
	const float z1    = gDepthBiasParams.y; // linear depth at which depth bias = d1
	const float d0    = gDepthBiasParams.z; // depth bias applied when z = z0
	const float d1    = gDepthBiasParams.w; // depth bias applied when z = z1
	const float d     = lerp(d0, d1, saturate((z - z0)/(z1 - z0)));

#if __SHADERMODEL >= 40
	if (depth <= (vPos.z + d))
	{
		c.w *= gzNearFarVis.z;
	}
	else // invisible
	{
		c.w *= gzNearFarVis.w;
	}
#endif // __SHADERMODEL >= 40
	
	return c;
}

float4 PS_DebugOverlay_internal(vertexOutputDebugOverlay IN)
{
	return PS_DebugOverlay_internal_PedDamageHack(IN, false);  // this can go away when the gDepthBiasParams for peds is removed... 
}

vertexOutputDebugOverlay VS_DebugOverlay_internal(float4 pos, float3 worldPos, float4 cpv0, float4 cpv1, float2 uv0, float3 normal, float3 tangent)
{
	vertexOutputDebugOverlay OUT;
	OUT.pos = pos;
	OUT.worldPos = worldPos;
	OUT.c0 = 0.0;
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD
	OUT.uv0 = uv0;
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_COORD

	if (dot(gColourNear + gColourFar, 1) < 0) // cpv or texture mode
	{
		float opacity = 1.0;

		if      (gColourFar.x >= -1) { OUT.c0.xyz = dot(-gColourNear, cpv0) + dot(-gColourFar, cpv1); }
		else if (gColourFar.x == -2) { OUT.c0.xyz = cpv0.xyz; }
		else if (gColourFar.x == -3) { OUT.c0.xyz = cpv1.xyz; }
		else if (gColourFar.x == -4) { OUT.c0.xyz = (normal + 1)/2; }
		else if (gColourFar.x == -5) { OUT.c0.xyz = (tangent + 1)/2; }
		else if (gColourFar.x == -9)
		{
#if defined(IS_TERRAIN_SHADER)
#if PER_MATERIAL_WETNESS_MULTIPLIER
			float isWetMult = any4(materialWetnessMultiplier.xyzw);
			float wetMult = dot( materialWetnessMultiplier, 0.25f.xxxx);
			OUT.c0.xyz = (isWetMult ? float3(1,0,0) : float3(0,0,wetMult));
#else
			OUT.c0.xyz = (wetnessMultiplier == 0 ? float3(1,0,0) : float3(0,0,wetnessMultiplier));
#endif			
#else
			opacity = 0.0; // don't draw non-terrain shaders in terrain wetness mode
#endif
		}
		else if (gColourFar.x == -10)
		{
#if defined(__GTA_PED_COMMON_FXH__)
			// colour is computed in the pixel shader
#else
			opacity = 0.0; // don't draw non-ped shaders in ped damage mode
#endif
		}
		else if (gColourFar.x == -11)
		{
			float edgeV = cpv0.z;
			OUT.c0.xyz = edgeV.xxx;
			
			if( edgeV > 0.000001f && edgeV < 0.999999f )
				OUT.c0.xyz *= float3(1,0,0);
		}
					
		OUT.c0.w = opacity;
	}

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);
	return OUT;
}

float4 PS_DebugOverlay_BANK_ONLY(vertexOutputDebugOverlay IN) : COLOR
{
	return PS_DebugOverlay_internal(IN);
}

#endif // defined(__GTA_COMMON_FXH)
#endif // _DEBUG_OVERLAY_FXH_
