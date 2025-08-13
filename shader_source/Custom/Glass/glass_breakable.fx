
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent
	#define PRAGMA_DCL
	#define DISABLE_ALPHA_DOF
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
//#define NO_SKINNING -- what? is this skinned? it uses 'gBoneMtx' below ..
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_NORMAL_MAP
#define USE_ALPHA_FRESNEL_ADJUST
#define USE_GLASS_LIGHTING

#define DIFFUSE_CLAMP Clamp

// Use this to enable SSA for breakable glass
#define SSA_BREAKABLE_GLASS 1

#include "glass_breakable.fxh"

bgVertexOutput VS_BreakableGlassNoCrack(bgVertexInput IN)
{
	return VSBreakableGlass(IN,false, false);
}

half4 PS_BreakableGlassNoCrack_lightweight0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, false, false, false);
}

half4 PS_BreakableGlassNoCrack_lightweight4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, false, false, false);
}

half4 PS_BreakableGlassNoCrack_lightweight8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, false, false, false);
}

half4 PS_BreakableGlassNoCrack(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, false, false, false);
}

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#if HIGHQUALITY_FORWARD_TECHNIQUES

half4 PS_BreakableGlassNoCrack_lightweightHighQuality0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, false, false, false);
}

half4 PS_BreakableGlassNoCrack_lightweightHighQuality4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, false, false, false);
}

half4 PS_BreakableGlassNoCrack_lightweightHighQuality8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, false, false, false);
}

#else

#define PS_BreakableGlassNoCrack_lightweightHighQuality0 PS_BreakableGlassNoCrack_lightweight0
#define PS_BreakableGlassNoCrack_lightweightHighQuality4 PS_BreakableGlassNoCrack_lightweight4
#define PS_BreakableGlassNoCrack_lightweightHighQuality8 PS_BreakableGlassNoCrack

#endif
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

DeferredGBuffer PS_BreakableGlassNoCrackDeferred( bgVertexOutput IN)
{
	return breakableGlassRenderDeferred(IN,false, false, false);
}

half4 PS_BreakableGlassDeferredNoCrack_AlphaClip2( bgVertexOutput IN) : SV_Target0
{
	DeferredGBuffer OUT = PS_BreakableGlassNoCrackDeferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(alphaBlend <= 1./255.);

	OUT = (DeferredGBuffer)0; // We only need the alpha value - this should help the optimizer remove all other calculations
	OUT.col0.a = (half)alphaBlend;
	return OUT.col0;
}

DeferredGBuffer PS_BreakableGlassNoCrackDeferred_SubSampleAlpha( bgVertexOutput IN)
{
	DeferredGBuffer OUT = PS_BreakableGlassNoCrackDeferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(SSAIsOpaquePixel(IN.pos) ||alphaBlend <= 1./255.);

	OUT.col0.a = (half)alphaBlend; 
	return OUT;
}

// ===============================
#if __MAX
	// ===============================
	// Tool technique
	// ===============================
	technique tool_draw
	{
		pass P0
		{
			MAX_TOOL_TECHNIQUE_RS
			// lit:
			VertexShader= compile VERTEXSHADER	VS_MaxTransform();
			PixelShader	= compile PIXELSHADER	PS_MaxPixelTextured();
		}  
	}

#else //__MAX...

#if FORWARD_TECHNIQUES
technique draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack();
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight0_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack_lightweight0();
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight4_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack_lightweight4();
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight8_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack_lightweight8();
	}
}
#endif // FORWARD_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality0_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack_lightweightHighQuality0();
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality4_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack_lightweightHighQuality4();
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality8_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrack_lightweightHighQuality8();
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrackDeferred();
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if SSA_BREAKABLE_GLASS
technique deferredsubsamplewritealpha_draw
{
	pass p0
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassDeferredNoCrack_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // SSA_BREAKABLE_GLASS

#if SSA_BREAKABLE_GLASS
technique deferredsubsamplealpha_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassNoCrack();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassNoCrackDeferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // SSA_BREAKABLE_GLASS

SHADERTECHNIQUE_ENTITY_SELECT(VS_BreakableGlassNoCrack(), VS_BreakableGlassNoCrack())
#define DEBUG_OVERLAY_VS VS_BreakableGlassNoCrack
#include "../../Debug/debug_overlay_breakable.fxh"
#undef DEBUG_OVERLAY_VS

#endif // __MAX...
