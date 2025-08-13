
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal
	#define PRAGMA_DCL
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_NORMAL_MAP
#define USE_ALPHA_FRESNEL_ADJUST
#define USE_GLASS_LIGHTING

#define DIFFUSE_CLAMP Clamp

// Use this to enable SSA for breakable glass
#define SSA_BREAKABLE_GLASS 1

// Enable this to get the cascaded shadow calculation handled in the PS for VS lit glass
#define VS_LIT_GLASS_WITH_PS_SHADOWS (!__WIN32PC || __SHADERMODEL >= 40)

#if VS_LIT_GLASS_WITH_PS_SHADOWS
#define USE_SHADOW_RECEIVING_VS (0)
#else
// Support vertex shader shadows for the low quality vertex shader lighting
#define USE_SHADOW_RECEIVING_VS (1)
#endif // VS_LIT_GLASS_WITH_PS_SHADOWS
#include "glass_breakable.fxh"


bgVertexOutput VS_BreakableGlassSelect(bgVertexInput IN)
{
	return VSBreakableGlass(IN,true, false);
}

bgVertexOutput VS_BreakableGlass_Common(bgVertexInputCrack IN, bool renderCrackEdge)
{
	bgVertexInput vtxIn;
	vtxIn.pos = IN.pos;
	vtxIn.diffuse = IN.diffuse;
	vtxIn.texCoord0 = IN.texCoord0;
	vtxIn.texCoord1 = IN.texCoord1;
	vtxIn.normal = IN.normal;
	vtxIn.tangent = float4(0,0,0,1);

	return VSBreakableGlass(vtxIn,true, renderCrackEdge);
}
bgVertexOutput VS_BreakableGlass(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Common(IN, false);
}

bgVertexOutput VS_BreakableGlass_CrackEdge(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Common(IN, true);
}

bgVertexOutputLow VS_BreakableGlass_Low_Common(bgVertexInputCrack IN, int numLights, bool renderCrackEdge)
{
	bgVertexInput vtxIn;
	vtxIn.pos = IN.pos;
	vtxIn.diffuse = IN.diffuse;
	vtxIn.texCoord0 = IN.texCoord0;
	vtxIn.texCoord1 = IN.texCoord1;
	vtxIn.normal = IN.normal;
	vtxIn.tangent = float4(0,0,0,1);

	return VSBreakableGlass_Low(vtxIn, numLights, true, renderCrackEdge);
}
bgVertexOutputLow VS_BreakableGlass_Low_lightweight0(bgVertexInputCrack IN)
{	
	return VS_BreakableGlass_Low_Common(IN, 0, false);
}

bgVertexOutputLow VS_BreakableGlass_CrackEdge_Low_lightweight0(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Low_Common(IN, 0, true);
}

bgVertexOutputLow VS_BreakableGlass_Low_lightweight4(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Low_Common(IN, 4, false);
}

bgVertexOutputLow VS_BreakableGlass_CrackEdge_Low_lightweight4(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Low_Common(IN, 4, true);
}

bgVertexOutputLow VS_BreakableGlass_Low_lightweight8(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Low_Common(IN, 8, false);
}

bgVertexOutputLow VS_BreakableGlass_CrackEdge_Low_lightweight8(bgVertexInputCrack IN)
{
	return VS_BreakableGlass_Low_Common(IN, 8, true);
}

bgVertexOutputLow VS_BreakableGlass_Low(bgVertexInputCrack IN)
{
	// We can only support up to 4 local lights when doing VS lighting
	return VS_BreakableGlass_Low_Common(IN, 4, false);
}

bgVertexOutputLow VS_BreakableGlass_CrackEdge_Low(bgVertexInputCrack IN)
{
	// We can only support up to 4 local lights when doing VS lighting
	return VS_BreakableGlass_Low_Common(IN, 4, true);
}

//////////////////////////////////////////////////////////////////
// Pixel shaders - High Quality

half4 PS_BreakableGlass_lightweight0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, false, false);
}

half4 PS_BreakableGlass_Bump_lightweight0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge_lightweight0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump_lightweight0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, false, true, true, true);
}

half4 PS_BreakableGlass_lightweight4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false, false);
}

half4 PS_BreakableGlass_Bump_lightweight4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge_lightweight4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump_lightweight4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true, true);
}

half4 PS_BreakableGlass_lightweight8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false, false);
}

half4 PS_BreakableGlass_Bump_lightweight8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge_lightweight8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump_lightweight8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true, true);
}

half4 PS_BreakableGlass(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false, false);
}

half4 PS_BreakableGlass_Bump(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, false, true, true, true);
}

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
#if HIGHQUALITY_FORWARD_TECHNIQUES

half4 PS_BreakableGlass_lightweightHighQuality0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true, false, false);
}

half4 PS_BreakableGlass_Bump_lightweightHighQuality0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge_lightweightHighQuality0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality0(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 0, USE_DIRECTIONAL_LIGHT_FWD_0_LIGHTS, false, HIGHQUALITY_FORWARD_TECHNIQUES, true, true, true);
}

half4 PS_BreakableGlass_lightweightHighQuality4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false, false);
}

half4 PS_BreakableGlass_Bump_lightweightHighQuality4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge_lightweightHighQuality4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality4(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 4, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, true, true);
}

half4 PS_BreakableGlass_lightweightHighQuality8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false, false);
}

half4 PS_BreakableGlass_Bump_lightweightHighQuality8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, false, true);
}

half4 PS_BreakableGlass_CrackEdge_lightweightHighQuality8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, true, false);
}

half4 PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality8(bgVertexOutput IN) : COLOR
{
	return breakableGlassRender(IN, 8, USE_DIRECTIONAL_LIGHT_FWD_4_OR_8_LIGHTS, true, HIGHQUALITY_FORWARD_TECHNIQUES, true, true, true);
}

#else

#define PS_BreakableGlass_lightweightHighQuality0 PS_BreakableGlass_lightweight0
#define PS_BreakableGlass_Bump_lightweightHighQuality0 PS_BreakableGlass_Bump_lightweight0
#define PS_BreakableGlass_CrackEdge_lightweightHighQuality0 PS_BreakableGlass_CrackEdge_lightweight0
#define PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality0 PS_BreakableGlass_CrackEdge_Bump_lightweight0
#define PS_BreakableGlass_lightweightHighQuality4 PS_BreakableGlass_lightweight4
#define PS_BreakableGlass_Bump_lightweightHighQuality4 PS_BreakableGlass_Bump_lightweight4
#define PS_BreakableGlass_CrackEdge_lightweightHighQuality4 PS_BreakableGlass_CrackEdge_lightweight4
#define PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality4 PS_BreakableGlass_CrackEdge_Bump_lightweight4
#define PS_BreakableGlass_lightweightHighQuality8 PS_BreakableGlass
#define PS_BreakableGlass_Bump_lightweightHighQuality8 PS_BreakableGlass_Bump
#define PS_BreakableGlass_CrackEdge_lightweightHighQuality8 PS_BreakableGlass_CrackEdge
#define PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality8 PS_BreakableGlass_CrackEdge_Bump

#endif // HIGHQUALITY_FORWARD_TECHNIQUES
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

DeferredGBuffer PS_BreakableGlass_Deferred( bgVertexOutput IN)
{
	return breakableGlassRenderDeferred(IN,true, false, false);
}

DeferredGBuffer PS_BreakableGlass_Bump_Deferred( bgVertexOutput IN)
{
	return breakableGlassRenderDeferred(IN,true, false, true);
}

DeferredGBuffer PS_BreakableGlass_CrackEdge_Deferred( bgVertexOutput IN)
{
	return breakableGlassRenderDeferred(IN,true, true, false);
}

DeferredGBuffer PS_BreakableGlass_CrackEdge_Bump_Deferred( bgVertexOutput IN)
{
	return breakableGlassRenderDeferred(IN,true, true, true);
}

half4 PS_BreakableGlass_Deferred_AlphaClip2( bgVertexOutput IN) : SV_Target0
{
	DeferredGBuffer OUT = PS_BreakableGlass_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(alphaBlend <= 1./255.);

	OUT = (DeferredGBuffer)0; // We only need the alpha value - this should help the optimizer remove all other calculations
	OUT.col0.a = (half)alphaBlend;
	return OUT.col0.a;
}

half4 PS_BreakableGlass_Bump_Deferred_AlphaClip2( bgVertexOutput IN) : SV_Target0
{
	DeferredGBuffer OUT = PS_BreakableGlass_Bump_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(alphaBlend <= 1./255.);

	OUT = (DeferredGBuffer)0; // We only need the alpha value - this should help the optimizer remove all other calculations
	OUT.col0.a = (half)alphaBlend;
	return OUT.col0.a;
}

half4 PS_BreakableGlass_CrackEdge_Deferred_AlphaClip2( bgVertexOutput IN) : SV_Target0
{
	DeferredGBuffer OUT = PS_BreakableGlass_CrackEdge_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(alphaBlend <= 1./255.);

	OUT = (DeferredGBuffer)0; // We only need the alpha value - this should help the optimizer remove all other calculations
	OUT.col0.a = (half)alphaBlend;
	return OUT.col0.a;
}

half4 PS_BreakableGlass_CrackEdge_Bump_Deferred_AlphaClip2( bgVertexOutput IN) : SV_Target0
{
	DeferredGBuffer OUT = PS_BreakableGlass_CrackEdge_Bump_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(alphaBlend <= 1./255.);

	OUT = (DeferredGBuffer)0; // We only need the alpha value - this should help the optimizer remove all other calculations
	OUT.col0.a = (half)alphaBlend;
	return OUT.col0.a;
}

DeferredGBuffer PS_BreakableGlass_Deferred_SubSampleAlpha( bgVertexOutput IN)
{
	DeferredGBuffer OUT = PS_BreakableGlass_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(SSAIsOpaquePixel(IN.pos) ||alphaBlend <= 1./255.);

	OUT.col0.a = (half)alphaBlend; 

	return OUT;
}

DeferredGBuffer PS_BreakableGlass_Bump_Deferred_SubSampleAlpha( bgVertexOutput IN)
{
	DeferredGBuffer OUT = PS_BreakableGlass_Bump_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(SSAIsOpaquePixel(IN.pos) ||alphaBlend <= 1./255.);

	OUT.col0.a = (half)alphaBlend; 

	return OUT;
}

DeferredGBuffer PS_BreakableGlass_CrackEdge_Deferred_SubSampleAlpha( bgVertexOutput IN)
{
	DeferredGBuffer OUT = PS_BreakableGlass_CrackEdge_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(SSAIsOpaquePixel(IN.pos) ||alphaBlend <= 1./255.);

	OUT.col0.a = (half)alphaBlend; 

	return OUT;
}

DeferredGBuffer PS_BreakableGlass_CrackEdge_Bump_Deferred_SubSampleAlpha( bgVertexOutput IN)
{
	DeferredGBuffer OUT = PS_BreakableGlass_CrackEdge_Bump_Deferred(IN);
	float alphaBlend = OUT.col0.a;

	rageDiscard(SSAIsOpaquePixel(IN.pos) ||alphaBlend <= 1./255.);

	OUT.col0.a = (half)alphaBlend; 
	return OUT;
}

//////////////////////////////////////////////////////////////////
// Pixel shaders - Low Quality

half4 PS_BreakableGlass_Low(bgVertexOutputLow IN) : COLOR
{
	return breakableGlassRenderLow(IN,true);
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
	pass p0 // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality with No Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge with No Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight0_draw
{
	pass p0 // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_lightweight0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p0 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_lightweight0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p1 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweight0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_lightweight0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight4_draw
{
	pass p0 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_lightweight4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_lightweight4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low_lightweight4();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweight4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_lightweight4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low_lightweight4();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if FORWARD_TECHNIQUES
technique lightweight8_draw
{
	pass p0 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_lightweight8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_lightweight8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low_lightweight8();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweight8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweight8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low_lightweight8();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES


#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality0_draw
{
	pass p0 // High quality With Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_lightweightHighQuality0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_lightweightHighQuality0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_lightweightHighQuality0() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low_lightweight0();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality4_draw
{
	pass p0 // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_lightweightHighQuality4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_lightweightHighQuality4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low_lightweight4();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_lightweightHighQuality4() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low_lightweight4();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED
technique lightweightHighQuality8_draw
{
	pass p0 // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_lightweightHighQuality8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_lightweightHighQuality8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_lightweightHighQuality8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_lightweightHighQuality8() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Low() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // HIGHQUALITY_FORWARD_TECHNIQUES_ENABLED

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0 // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_Low();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge_Low();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Deferred() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if SSA_BREAKABLE_GLASS
technique deferredsubsamplewritealpha_draw
{
	pass p0 // High quality with Bump
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_Deferred_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
			VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Deferred_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p2// Low quality - Same as high quality
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Deferred_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_Deferred_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
			VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Deferred_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge- Same as high quality crack Edge
	{
		DEFERRED_SUBSAMPLEWRITEALPHA_RENDERSTATES_TRANSPARENT()
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Deferred_AlphaClip2() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // SSA_BREAKABLE_GLASS

#if SSA_BREAKABLE_GLASS
technique deferredsubsamplealpha_draw
{
	pass p0 // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Bump_Deferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1 // High quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Deferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p2 // Low quality - Same as high quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_Deferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3 // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Bump_Deferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4 // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Deferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge - Same as high quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_BreakableGlass_CrackEdge_Deferred_SubSampleAlpha() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // SSA_BREAKABLE_GLASS

#if !defined(SHADER_FINAL)
technique entityselect_draw
{
	pass p0  // High quality with Bump
	{
		VertexShader = compile VERTEXSHADER VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1  // High quality
	{
		VertexShader = compile VERTEXSHADER VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	
	pass p2 // Low quality
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass();
		PixelShader  = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3  // High quality crack Edge with Bump
	{
		VertexShader = compile VERTEXSHADER VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4  // High quality crack Edge
	{
		VertexShader = compile VERTEXSHADER VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER  PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p5 // Low quality crack Edge
	{
		VertexShader = compile VERTEXSHADER	VS_BreakableGlass_CrackEdge();
		PixelShader  = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // !defined(SHADER_FINAL)

#define DEBUG_OVERLAY_VS VS_BreakableGlassSelect
#include "../../Debug/debug_overlay_breakable.fxh"
#undef DEBUG_OVERLAY_VS

#endif // __MAX...
