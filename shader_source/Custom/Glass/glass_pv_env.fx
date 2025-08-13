
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent
	#define PRAGMA_DCL
	#define DISABLE_ALPHA_DOF
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_ALPHA_FRESNEL_ADJUST
#define USE_GLASS_LIGHTING
#define FORCE_RAIN_COLLISION_TECHNIQUE

// Enable this to get the cascaded shadow calculation handled in the PS for VS lit glass
#define VS_LIT_GLASS_WITH_PS_SHADOWS (!__WIN32PC || __SHADERMODEL >= 40)

#if VS_LIT_GLASS_WITH_PS_SHADOWS
#define USE_SHADOW_RECEIVING_VS (0)
#else
// Support vertex shader shadows for the low quality vertex shader lighting
#define USE_SHADOW_RECEIVING_VS (1)
#endif // VS_LIT_GLASS_WITH_PS_SHADOWS

// Support vertex alpha
#define VS_LIT_WITH_VERT_ALPHA

#ifdef USE_PV_AMBIENT_BAKES
	#define PV_AMBIENT_BAKES (1)
#else
	#define PV_AMBIENT_BAKES (0)
#endif

#include "glass_breakable.fxh"

bgVertexOutput VS_GlassSelect(bgVertexInput IN)
{
	return VSBreakableGlass(IN,false, false);
}

bgVertexOutputLow VS_GlassEnvLow_lightweight0(bgVertexInputLow IN)
{
	bgVertexInput vtxIN;
	vtxIN.pos = IN.pos;
	vtxIN.diffuse = IN.diffuse;
	vtxIN.texCoord0 = IN.texCoord0;
	vtxIN.texCoord1 = float2(0,0);
	vtxIN.normal = IN.normal;
	vtxIN.tangent = IN.tangent;
	
	return VSBreakableGlass_Low(vtxIN, 0, false, false);
}

bgVertexOutputLow VS_GlassEnvLow_lightweight4(bgVertexInputLow IN)
{
	bgVertexInput vtxIN;
	vtxIN.pos = IN.pos;
	vtxIN.diffuse = IN.diffuse;
	vtxIN.texCoord0 = IN.texCoord0;
	vtxIN.texCoord1 = float2(0,0);
	vtxIN.normal = IN.normal;
	vtxIN.tangent = IN.tangent;
	
	return VSBreakableGlass_Low(vtxIN, 4, false, false);
}

bgVertexOutputLow VS_GlassEnvLow_lightweight8(bgVertexInputLow IN)
{
	bgVertexInput vtxIN;
	vtxIN.pos = IN.pos;
	vtxIN.diffuse = IN.diffuse;
	vtxIN.texCoord0 = IN.texCoord0;
	vtxIN.texCoord1 = float2(0,0);
	vtxIN.normal = IN.normal;
	vtxIN.tangent = IN.tangent;
	
	return VSBreakableGlass_Low(vtxIN, 8, false, false);
}

bgVertexOutputLow VS_BreakableGlassLow(bgVertexInputLow IN)
{
	bgVertexInput vtxIN;
	vtxIN.pos = IN.pos;
	vtxIN.diffuse = IN.diffuse;
	vtxIN.texCoord0 = IN.texCoord0;
	vtxIN.texCoord1 = float2(0,0);
	vtxIN.normal = IN.normal;
	vtxIN.tangent = IN.tangent;
	
	return VSBreakableGlass_Low(vtxIN, 4, false, false); // We can only support up to 4 local lights when doing VS lighting
}

//////////////////////////////////////////////////////////////////
// Pixel shaders - Low Quality

half4 PS_BreakableGlassLow(bgVertexOutputLow IN) : COLOR
{
	return breakableGlassRenderLow(IN,false);
}

DeferredGBuffer PS_BreakableGlassDeferredLow(bgVertexOutputLow IN)
{
	// Not supported
	DeferredGBuffer OUT =(DeferredGBuffer)0;
	return OUT;
}

half4 PS_BreakableGlassDeferredLow_SubSampleAlpha(bgVertexOutputLow IN) : COLOR0
{
	// Not supported
	DeferredGBuffer OUT =(DeferredGBuffer)0;
	return OUT.col0;
}

DeferredGBuffer PS_BreakableGlassDeferredLow_AlphaClip2( bgVertexOutputLow IN)
{
	// Not supported
	DeferredGBuffer OUT =(DeferredGBuffer)0;
	return OUT;
}

//////////////////////////////////////////////////////////////////
// Techniques

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
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassLow();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique unlit_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassLow();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight0_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_GlassEnvLow_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight4_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_GlassEnvLow_lightweight4();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight8_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_GlassEnvLow_lightweight8();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality0_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_GlassEnvLow_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality4_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_GlassEnvLow_lightweight4();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality8_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassLow();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassLow();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassDeferredLow() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplewritealpha_draw
{
	pass p0
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassLow();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassDeferredLow_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplealpha_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlassLow();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlassDeferredLow_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

SHADERTECHNIQUE_CASCADE_SHADOWS()
SHADERTECHNIQUE_ENTITY_SELECT(VS_GlassSelect(), VS_GlassSelect())
#define DEBUG_OVERLAY_VS VS_GlassSelect
#include "../../Debug/debug_overlay_breakable.fxh"
#undef DEBUG_OVERLAY_VS

#endif // __MAX...
