// 
// Puddle.fxh
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
//



#ifndef	__PUDDLE_FXH__
#define	__PUDDLE_FXH__


#ifdef _NO_PUDDLE_SHADERS_


BeginSampler(sampler, PuddleMask, PuddleMaskSampler, PuddleMask)
ContinueSampler(sampler, PuddleMask, PuddleMaskSampler, PuddleMask)
MinFilter = HQLINEAR;
MagFilter = MAGLINEAR;
MipFilter = MIPLINEAR;
AddressU  = WRAP;
AddressV  = WRAP;
EndSampler;

BeginSampler(sampler2D, PuddleBump, PuddleBumpSampler, PuddleBump)
ContinueSampler(sampler2D, PuddleBump, PuddleBumpSampler, PuddleBump)
ANISOTROPIC_LEVEL(4)
MINFILTER = MINANISOTROPIC;
MipFilter = MIPLINEAR;
MAGFILTER = MAGLINEAR; 
AddressU  = WRAP;
AddressV  = WRAP;
EndSampler;

BeginSampler(sampler2D, PuddlePlayerBump, PuddlePlayerBumpSampler, PuddlePlayerBump)
ContinueSampler(sampler2D, PuddlePlayerBump, PuddlePlayerBumpSampler, PuddlePlayerBump)
AddressU = CLAMP;
AddressV = CLAMP;
MinFilter = HQLINEAR;
MagFilter = MAGLINEAR;
MipFilter = MIPLINEAR;
EndSampler;


BeginConstantBufferDX10(puddle_locals)
float4 g_Puddle_ScaleXY_Range : Puddle_ScaleXY_Range;
float4 g_PuddleParams : PuddleParams;
float4 g_GBufferTexture3Param : GBufferTexture3Param;
EndConstantBufferDX10(puddle_locals)



// Ripple Support
//AO gbuffer to affect puddles

//#define					WindBumpSampler  PuddleBumpSampler



BeginConstantBufferDX10(ripple_locals)
float3 RippleData;
float4 RippleScaleOffset;
EndConstantBufferDX10(ripple_locals)



#define RO 0.5f
#define RO_WATER			0.02037
//0.02037f
#define RO_MURKY_WATER		0.1037f


#define _USE_PROCEDURAL_RIPPLE 1


#if __PS3
#define PlayerBumpSwizzle zw
#else
#define PlayerBumpSwizzle xy
#endif
float3 CalcNormal( float2 bumpPos, float scale)
{
	float3 normal;
#if _USE_PROCEDURAL_RIPPLE
	normal.xy = 2.*tex2D(PuddleBumpSampler, bumpPos * .5f ).xy-1.f;
													 
	// Todo : y flip into the scale and offset.
	float2 playerBumpPos = bumpPos.xy * RippleScaleOffset.xy + RippleScaleOffset.zw;

	float2 playerBump =tex2D(PuddlePlayerBumpSampler, playerBumpPos.xy*float2(1.f,-1.f)+float2(0.,1.) ).PlayerBumpSwizzle;
	normal.xy += 2.*(playerBump.xy-.5/255.f)-1.f;
#else
	// flatten depending rain strength
	float bstrength = g_PuddleParams.y;

	normal.xy = ( 2.0 * (tex2D(PuddleBumpSampler, bumpPos*.5 ).wy) - 1.0)*.5f;  // compress normal maps have data in y & w only...
	normal.xy +=( 2.0 * (tex2D(PuddleBumpSampler, bumpPos * 1.785).wy )- 1.0)*.8 ;  // compress normal maps have data in y & w only...
	normal.xy = normal.xy * bstrength;
#endif
	normal.xy*=scale;
	normal.z =sqrt( ABS_PC(1- dot(normal.xy, normal.xy)));
	return normal;
}

float GetPuddleDepth( float2 worldPos)
{
	float2 uvs = worldPos.xy * g_Puddle_ScaleXY_Range.xx;
	float puddMask = tex2D(PuddleMaskSampler, uvs).r;
	return saturate( puddMask -g_PuddleParams.x);
}
#define _USE_PUDDLE_MASK 1

half4 PuddleGetColor(float flattness, float3 worldPos, float AOVal, float puddleBump )
{
	float waterDepth = GetPuddleDepth( worldPos.xy) * flattness ;

	float4 Diffuse = 0.0f;
#if !_USE_PUDDLE_MASK
	if ( waterDepth >20.0f/256.f)
#else
	// blend to zero over 
	waterDepth=saturate((waterDepth-20.0f/256.f)*1./(1.-40.0f/256.f));	
#endif
	{
		// Calculate the normal from the bump amp
		float3 view = gViewInverse[3].xyz - worldPos.xyz;
		view = normalize(view);

		float3 normal =  CalcNormal(worldPos.xy, puddleBump);
		// look up environment map
		
		float fresnel = 1.0 - saturate(dot(view, normal));
		fresnel = pow( fresnel, 5);		
		fresnel = saturate(fresnel * (1.0 - RO_WATER) + RO_WATER);
		
		float3 env1 = calcReflectionFlatClamped(ReflectionSampler, -view, normal,  8192.0f);

		float3 dcol = lerp(0.0f, env1, fresnel);
		float AOfade = saturate((AOVal-g_PuddleParams.w)*1.5);

		float alpha = (g_PuddleParams.z + (1.0 - g_PuddleParams.z) * fresnel) * saturate(waterDepth * 3.0) * AOfade;

		Diffuse.a=alpha;
		Diffuse.rgb=dcol;
	}
	return PackHdr(Diffuse);
}

half4 PuddleGetBump(float flattness, float3 worldPos, float AOVal )
{
	float2 uvs = worldPos.xy * g_Puddle_ScaleXY_Range.xx;
	float puddTex = saturate( tex2D(PuddleMaskSampler, uvs).r -g_PuddleParams.x);
	float waterDepth = puddTex * flattness ;

	float4 Diffuse = 0.0f;
#if __XENON
	[branch]	
#endif
	if ( waterDepth >20.0f/256.f)
	{
		float3 normal =  CalcNormal(worldPos.xy,1.f);

		Diffuse.a=1.* gInvColorExpBias;
		Diffuse.rgb=normal.xyz*.5+.5;		
	}
	return PackHdr(Diffuse);
}


// Ripples

struct vertexInputRipple
{
	float4 pos		: POSITION;		// input expected in viewport space 0 to 1
	float4 wave		: TEXCOORD0;
};

struct pixelInputRipple
{
	DECLARE_POSITION(pos)		// input expected in viewport space 0 to 1
	float2 screenPos			: TEXCOORD0;	
	float4 wave		: TEXCOORD1;
	float waveData	: TEXCOORD2;
};

// Ripples
half4 PS_windRipples(pixelInputRipple IN ) : COLOR
{
/*
	// wind velocity code
	float ang = 2*PI*gUmGlobalTimer.x/ 360.f;
	float3 windVel=float3( sin(ang),cos(ang),0.);
	windVel= normalize(windVel);
	float2 wind2d = windVel.xy;
	float2 r0 = float2(wind2d.x, -wind2d.y) ;
	float2 r1 = float2(wind2d.y, wind2d.x);

	// may need to translate only y
	float2 t = gUmGlobalTimer.x*0.1 ;
	
	float2 spos = IN.screenPos.xy;//*float2(1.f,1.f)+float2(0.f,1.);
//	spos = spos*2.-1;
	float2 uv =  float2( dot(r0, spos ), dot( r1, spos));
*/
	float2 uv = IN.screenPos.xy * float2(3.f,1.f); 
	float2 offset = float2(RippleData.x,0.f);

	float2 sum= (tex2D( PuddleBumpSampler /*WindBumpSampler*/, uv + offset).wy *2.-1.f);
    sum += (tex2D( PuddleBumpSampler /*WindBumpSampler*/, uv *2.f+ offset*4.f).wy * 2. - 1.);
	float2 v =  sum.xy* RippleData.y *.5+.5;
#if __PS3
	return float4(v.xy,.5,.5);
#else
	return float4( sum.xy* RippleData.y *.5+.5, 0.f,0.f);
#endif
}


pixelInputRipple VS_screenTransformRipple(vertexInputRipple IN)
{
    pixelInputRipple OUT;
	OUT.pos	= float4(IN.pos.xy*2.f-1.f, 0, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1)
	OUT.screenPos = IN.pos.xy;
	OUT.wave = IN.wave;
	OUT.waveData = IN.pos.z;
	return(OUT);
}
vertexOutputLPS VS_screenTransformClearRipple(vertexInputLP IN)
{
     vertexOutputLPS OUT;
	OUT.pos	= float4(IN.pos.xy*2.f-1.f, 0, 1); //adjust from viewport space (0 to 1) to screen space (-1 to 1)
	OUT.screenPos = float4(IN.pos.xy,0.,0.);
	OUT.eyeRay = 0;
	
	return(OUT);
}

float4 CalcRipple( float2 screenPos, float4 wave, float lifeScale, float rlife ) 
{
   float2 duv = screenPos-wave.xy;  // constant
	float life =  rlife *lifeScale;
   float range = 1.5;
   float len = sqrt(dot(duv,duv));
   float freq=len*wave.z-wave.w;
    
    float rs = min(max( freq, -1.f), range);
    float r = cos(2.f*PI*rs);
	duv *= r/len;
	float amp = saturate( abs(freq)/1.5);
	amp = amp * amp;
	life *=1.- amp*amp;
	float alphaVal = freq  >-1.f ? (freq<range ? 1 : 0 ) : 0;
	return float4( duv.xy*.5+.5, 0.f,alphaVal*life*  RippleData.z);
}
half4 PS_Ripple(pixelInputRipple IN ) : COLOR
{
	return CalcRipple( IN.screenPos.xy, IN.wave, 1.f, IN.waveData.x) ;  
}
half4 PS_FootRipple(pixelInputRipple IN ) : COLOR
{
  // calculate ripple height
   float4 wave = IN.wave;

   // transform waves into puddle tex map
	float2 waveWorldPos = wave.xy * RippleScaleOffset.xy + RippleScaleOffset.zw;
   float pmask =  tex2D(PuddleMaskSampler, waveWorldPos.xy * g_Puddle_ScaleXY_Range.xx ).r;

   float4 fr= CalcRipple( IN.screenPos.xy, wave, pmask > 0.6 ? 1. : 0.0, IN.waveData.x) ;
#if __PS3  // can't use alpha blending on ps3
   rageDiscard( fr.a<.1);
   fr.xy = (fr.xy*2.-1)*fr.a;
   fr.xy =fr.xy*.5 + .5;
   return fr.xyxy;
#else
	return fr;
#endif
}
half4 PS_clearRipple(pixelInputLPS IN ) : COLOR
{
	return float4(0.5, 0.5, 1.0,0.0);
}
 technique MSAA_NAME(WindRippleTechnique)
{
	pass MSAA_NAME(p0) 
	{
		VertexShader	= compile VERTEXSHADER		VS_screenTransformRipple();
		PixelShader		= compile MSAA_PIXEL_SHADER	PS_windRipples()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}
technique MSAA_NAME(RippleTechnique)
{
	pass MSAA_NAME(p0) 
	{
		AlphaBlendEnable=true;
		AlphaTestEnable  = false; 
		BlendOp          = ADD; 
		SrcBlend         = SRCALPHA; 
		DestBlend        = INVSRCALPHA;
#if	__PS3
		ColorWriteEnable = RED+GREEN;
#endif
		VertexShader	= compile VERTEXSHADER		VS_screenTransformRipple();
		PixelShader		= compile MSAA_PIXEL_SHADER	PS_Ripple()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}
technique MSAA_NAME(FootRippleTechnique)
{
	pass MSAA_NAME(p0) 
	{
		
		AlphaTestEnable  = false; 
		BlendOp          = ADD; 
		SrcBlend         = SRCALPHA; 
		DestBlend        = INVSRCALPHA;
#if	__PS3
		AlphaBlendEnable=false;
		ColorWriteEnable = BLUE+ALPHA;
#else
		AlphaBlendEnable=true;
#endif
		VertexShader	= compile VERTEXSHADER		VS_screenTransformRipple();
		PixelShader		= compile MSAA_PIXEL_SHADER	PS_FootRipple()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}
technique MSAA_NAME(ClearRippleTechnique)
{
	pass MSAA_NAME(p0) 
	{
		
		AlphaBlendEnable=false;
#if	__PS3
		ColorWriteEnable = BLUE+ALPHA;
#endif
		VertexShader	= compile VERTEXSHADER		VS_screenTransformClearRipple();
		PixelShader		= compile MSAA_PIXEL_SHADER	PS_clearRipple()  CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(1));
	}
}

#endif // _NO_PUDDLE_SHADERS_

#endif //__PUDDLE_FXH__...

