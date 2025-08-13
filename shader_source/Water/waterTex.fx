#include "../../renderer/waterdefines.h"

#ifndef PRAGMA_DCL
	#pragma dcl position
	#define PRAGMA_DCL
#endif

#include "../common.fxh"
#include "../../Renderer/Lights/TiledLightingSettings.h"
#include "../../../rage/base/src/shaderlib/rage_xplatformtexturefetchmacros.fxh"
#include "../Lighting/Shadows/cascadeshadows_common.fxh"
#define SHADOW_CASTING            (0)
#define SHADOW_CASTING_TECHNIQUES (0)
#define SHADOW_RECEIVING          (1)
#define SHADOW_PROCESSING         (0)
#include "../Lighting/Shadows/cascadeshadows.fxh"


#define WATER_USEHALF 1
#if WATER_USEHALF
#define wfloat		half
#define wfloat2		half2
#define wfloat3		half3
#define wfloat4		half4
#define wfloat3x3	half3x3
half	w1tex2D(sampler Sampler, half2 TexCoord){ return h1tex2D(Sampler, TexCoord); }
half2	w2tex2D(sampler Sampler, half2 TexCoord){ return h2tex2D(Sampler, TexCoord); }
half3	w3tex2D(sampler Sampler, half2 TexCoord){ return h3tex2D(Sampler, TexCoord); }
half4	w4tex2D(sampler Sampler, half2 TexCoord){ return h4tex2D(Sampler, TexCoord); }
half4	wCalcScreenSpacePosition(half4 Position)
{
	half4	ScreenPosition;

	ScreenPosition.x = Position.x * 0.5 + Position.w * 0.5;
	ScreenPosition.y = Position.w * 0.5 - Position.y * 0.5;
	ScreenPosition.z = Position.w;
	ScreenPosition.w = Position.w;

	return(ScreenPosition);
}
#else //WATER_USEHALF
#define wfloat		float
#define wfloat2		float2
#define wfloat3		float3
#define wfloat4		float4
#define wfloat3x3	float3x3
float	w1tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).x; }
float2	w2tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).xy; }
float3	w3tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).xyz; }
float4	w4tex2D(sampler Sampler, float2 TexCoord){ return tex2D(Sampler, TexCoord).xyzw; }
#define	wCalcScreenSpacePosition rageCalcScreenSpacePosition
#endif

wfloat3 UnpackBackbufferColor(wfloat3 color)
{
#if __XENON
	return UnpackHdr_3h(color);
#else
	return color;
#endif
}


BEGIN_RAGE_CONSTANT_BUFFER(waterTex_locals,b0)
//Changed from an array of floats. On PC this gets compiled into an array of float4's with just the x components set which is wastefull
// - changed to smaller array of float4's with all components set.
float4 WaterBumpParams[2];
#define VelocityScale	WaterBumpParams[0].y
#define WaveMultiplier	WaterBumpParams[1].y

float4 gProjParams	: ProjParams;

float4 gFogCompositeParams           : FogCompositeParams;
float4 gFogCompositeAmbientColor     : FogCompositeAmbientColor;
float4 gFogCompositeDirectionalColor : FogCompositeDirectionalColor;
float4 gFogCompositeTexOffset		 : FogCompositeTexOffset;
#define FogPierceIntensity	gFogCompositeParams.x
#define FogLightIntensity	gFogCompositeParams.y
#define Wetness				gFogCompositeParams.x
#define CausticsOffset		gFogCompositeParams.z

float4 UpdateParams0;//m_timestep; m_timeInMilliseconds; m_TimeFactorOffset; m_disturbAmount;
float4 UpdateParams1;//m_waveLengthScale; m_waveMult; m_ShoreWavesMult; noiseoffset;
float4 UpdateParams2;//m_ShoreWavesMult
float4 UpdateOffset;
#define gTimeStep			UpdateParams0.x
#define gTimeInMilliseconds	UpdateParams0.y
#define gTimeFactorOffset	UpdateParams0.z
#define gDisturbAmount		UpdateParams0.w
#define gOceanFoamScale		UpdateParams0.y
#define gWaveLengthScale	UpdateParams1.x
#define gWaveMult			UpdateParams1.y
#define gShoreWavesMult		UpdateParams1.z
#define gNoiseOffset		UpdateParams1.w
#define gWorldBase			UpdateOffset.xy
#define gScaledBaseOffset	UpdateOffset.zw

#define gReflectionIntensity	UpdateParams0.x
#define gRefractionBlend		UpdateParams0.y
#define gRefractionExponent		UpdateParams0.z
#define gWaterHeight			UpdateParams0.w

EndConstantBufferDX10(waterTex_locals)

//------------------------------------
struct vertexInput {
	// This covers the default rage vertex format (non-skinned)
    float3 pos			: POSITION;
    float3 wind			: NORMAL;
    float2 texCoord0	: TEXCOORD0;
    float4 color		: COLOR0;
};

struct vertexOutput {
    DECLARE_POSITION(worldPos)
    float2 texCoord0	: TEXCOORD0;
	float4 color		: COLOR0;
};

#if __PS3
struct VS_INPUT
{
	float2 TexCoord		: TEXCOORD0;
	float4 Color		: TEXCOORD1;
};
#else
struct VS_INPUT
{
	float2 TexCoord		: TEXCOORD0;
};
#endif

struct VS_OUTPUT
{
	DECLARE_POSITION(Position)
	float2 TexCoord		: TEXCOORD0;
};

struct VS_OUTPUTREFRACTION
{
	DECLARE_POSITION(Position)
	float2 TexCoord		: TEXCOORD0;
	float4 View			: TEXCOORD1;
};

struct PS_OUTPUT
{
	float4 HeightAndVelocity	: SV_Target0;
	float4 Bump					: SV_Target1;
};


//============================ Samplers ================================
BeginSampler	(sampler, LinearTexture, LinearSampler, LinearTexture)
ContinueSampler	(sampler, LinearTexture, LinearSampler, LinearTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginSampler	(sampler, LinearTexture2, LinearSampler2, LinearTexture2)
ContinueSampler	(sampler, LinearTexture2, LinearSampler2, LinearTexture2)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginSampler	(sampler, BumpTexture, BumpSampler, BumpTexture)
ContinueSampler	(sampler, BumpTexture, BumpSampler, BumpTexture)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= NONE;
	MINFILTER	= HQLINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginSampler	(sampler, HeightTexture, HeightSampler, HeightTexture)
ContinueSampler	(sampler, HeightTexture, HeightSampler, HeightTexture)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= NONE;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginSampler	(sampler, NoiseTexture, NoiseSampler, NoiseTexture)
ContinueSampler	(sampler, NoiseTexture, NoiseSampler, NoiseTexture)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= NONE;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginSampler	(sampler, FullTexture, FullSampler, FullTexture)
ContinueSampler	(sampler, FullTexture, FullSampler, FullTexture)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= NONE;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginSampler	(sampler, DepthTexture, DepthSampler, DepthTexture)
ContinueSampler	(sampler, DepthTexture, DepthSampler, DepthTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginSampler	(sampler, PointTexture, PointSampler, PointTexture)
ContinueSampler	(sampler, PointTexture, PointSampler, PointTexture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginSampler	(sampler, PointTexture2, PointSampler2, PointTexture2)
ContinueSampler	(sampler, PointTexture2, PointSampler2, PointTexture2)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= POINT;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginSampler	(sampler, LinearWrapTexture, LinearWrapSampler, LinearWrapTexture)
ContinueSampler	(sampler, LinearWrapTexture, LinearWrapSampler, LinearWrapTexture)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= LINEAR;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

BeginSampler	(sampler, LinearWrapTexture2, LinearWrapSampler2, LinearWrapTexture2)
ContinueSampler	(sampler, LinearWrapTexture2, LinearWrapSampler2, LinearWrapTexture2)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= LINEAR;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;

#if __SHADERMODEL >= 40
BeginDX10Sampler(sampler, Texture2D, LinearWrapTexture3, LinearWrapSampler3, LinearWrapTexture3)
#else
BeginSampler	(sampler,            LinearWrapTexture3, LinearWrapSampler3, LinearWrapTexture3)
#endif
ContinueSampler( sampler,            LinearWrapTexture3, LinearWrapSampler3, LinearWrapTexture3)
	AddressU	= WRAP;
	AddressV	= WRAP;
	MIPFILTER	= LINEAR;
	MINFILTER	= LINEAR;
	MAGFILTER	= LINEAR;
EndSampler;


//=========================================================================

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ******************************
//	DRAW METHODS
// ******************************
vertexOutput VS_watertex(vertexInput IN)
{
	vertexOutput OUT;
	OUT.worldPos = float4(IN.pos,1);
	OUT.texCoord0.x = IN.texCoord0.x;
	OUT.texCoord0.y = IN.texCoord0.y;
	OUT.color = IN.color;
	return OUT;
}

VS_OUTPUT VS(vertexInput IN)
{
	VS_OUTPUT OUT;
	OUT.TexCoord = IN.texCoord0;
	OUT.Position = float4(IN.pos, 1);
	return OUT;
}

VS_OUTPUTREFRACTION VS_FogComposite(vertexInput IN)
{
	VS_OUTPUTREFRACTION OUT;
	OUT.TexCoord	= IN.texCoord0;
	OUT.Position	= float4(IN.pos, 1);
	OUT.View		= float4(mul(float4(OUT.Position.xy*gProjParams.xy, -1, 0), gViewInverse).xyz, 1.0f);
	return OUT;
}

struct vertexInputTile
{
	half4 pos			: POSITION;		
#if TILED_LIGHTING_INSTANCED_TILES
	half4 instanceData	: TEXCOORD0;
#endif //TILED_LIGHTING_INSTANCED_TILES
};

VS_OUTPUTREFRACTION VS_FogCompositeTiled(vertexInputTile IN)
{
	VS_OUTPUTREFRACTION OUT;

#if TILED_LIGHTING_INSTANCED_TILES
	float widthMult  = IN.pos.z;
	float heightMult = IN.pos.w;
	IN.pos.x += (IN.instanceData.x * widthMult);
	IN.pos.y += 1.0f - (IN.instanceData.y * heightMult);
	IN.pos.zw = IN.instanceData.zw;
#endif //TILED_LIGHTING_INSTANCED_TILES

	float2 pos = IN.pos.xy;

	OUT.Position	= float4((pos - 0.5f)*2, 0, 1); 
	OUT.TexCoord	= pos;
	OUT.TexCoord.y	= 1 - OUT.TexCoord.y;
	OUT.View		= float4(mul(float4(OUT.Position.xy*gProjParams.xy, -1, 0), gViewInverse).xyz, 1.0f);	

	// Get tile data
#if __SHADERMODEL >= 40
	//float4 tileData = tex2Dlod(LinearWrapSampler3, float4(IN.pos.zw, 0, 0)).r;
	float tileData = LinearWrapTexture3.Load(int3(IN.pos.zw/float2(widthMult, heightMult), 0.0f)).r;
#else
	float tileData = 0;
#endif

#if RSG_DURANGO || RSG_ORBIS
	OUT.Position.x = tileData? OUT.Position.x : sqrt(-1);
#else
	OUT.Position = OUT.Position*tileData;
#endif

	return(OUT);
}

float GetSSAODepth(sampler2D depthSampler, float2 tex)
{
	return tex2D(depthSampler, tex).x;
}
/* ====== PIXEL SHADERS =========== */
//-----------------------------------

wfloat4 UpdateBumpCommon(vertexOutput IN, sampler HVSampler)
{
	wfloat t			= 1.0/256;
	wfloat2 tex			= IN.texCoord0.xy;

	wfloat3 offsets		= wfloat3(t, -t, 0);

	wfloat4 color		= w4tex2D(HVSampler, tex);
	wfloat height		= color.r;

	wfloat4 aHeights0	= wfloat4(	w1tex2D(HVSampler, tex + offsets.xz).r,
									w1tex2D(HVSampler, tex + offsets.zx).r,
									w1tex2D(HVSampler, tex + offsets.yz).r,
									w1tex2D(HVSampler, tex + offsets.zy).r);

	wfloat4 aHeights1	= wfloat4(	w1tex2D(HVSampler, tex + offsets.xx).r,
									w1tex2D(HVSampler, tex + offsets.xy).r,
									w1tex2D(HVSampler, tex + offsets.yy).r,
									w1tex2D(HVSampler, tex + offsets.yx).r);

	wfloat4 minHeights;
	wfloat4 pHeights = aHeights0;

	offsets				*= 2;
	minHeights			= wfloat4(	w1tex2D(HVSampler, tex + offsets.xz).r,
									aHeights1.r,
									height,
									aHeights1.g);	
	pHeights			= min(pHeights, minHeights);


	minHeights			= wfloat4(	aHeights1.r,
									w1tex2D(HVSampler, tex + offsets.zx).r,
									aHeights1.a,
									height);
	pHeights			= min(pHeights, minHeights);

	minHeights			= wfloat4(	height,
									aHeights1.a,
									w1tex2D(HVSampler, tex + offsets.yz).r,
									aHeights1.b);
	pHeights			= min(pHeights, minHeights);

	minHeights			= wfloat4(	aHeights1.g,
									height,
									aHeights1.b,
									w1tex2D(HVSampler, tex + offsets.zy).r);
	pHeights			= min(pHeights, minHeights);

	wfloat2 bump = (pHeights.zw - pHeights.xy)*3;

	aHeights0			*= 0.18;
	aHeights1			*= 0.07;
	aHeights0			+= aHeights1;
	wfloat averageHeight = dot(aHeights0, 1);

	wfloat push	= w2tex2D(NoiseSampler, IN.texCoord0.xy + IN.color.xy).g;
 
	wfloat disturb = 2*push - 1;

	color.zw		= bump;

	wfloat velocity	= (0.5f-color.x)*WaterBumpParams[0].y;
	velocity		+= (averageHeight-color.x)*WaterBumpParams[0].x;
	velocity		+= disturb*WaterBumpParams[1].y;

	color.x			+=((color.y-0.5f)*WaterBumpParams[1].x*WaterBumpParams[1].w);
	color.x			+=(velocity*0.5f*WaterBumpParams[0].w*WaterBumpParams[1].w*WaterBumpParams[1].w);

	color.y			= color.y + (velocity*WaterBumpParams[0].w*WaterBumpParams[1].w);

	color.zw		+= 0.5f;

	return color;
}

wfloat4 PS_UpdateBump( vertexOutput IN) : COLOR
{
	return UpdateBumpCommon(IN, BumpSampler);
}

PS_OUTPUT PSUpdateMRT(vertexOutput IN)
{
	float4 color = UpdateBumpCommon(IN, HeightSampler);

	PS_OUTPUT OUT;

//Output the colours this way on PC sp that its visually the same as the consoles which
//are using a different texture format.
#if __WIN32PC && __SHADERMODEL >= 40
	OUT.HeightAndVelocity	= float4( color.xy, 1.0, 1.0);
	OUT.Bump				= float4( color.zw, 1.0, 1.0);
#else
	OUT.HeightAndVelocity	= color.xyxy;
	OUT.Bump				= color.zwzw;
#endif
	return OUT;
}

wfloat4 PSUpdateTemp(vertexOutput IN) : COLOR
{
	return UpdateBumpCommon(IN, HeightSampler);
}

float4 PSCopyHeight(VS_OUTPUT IN) : COLOR
{
	return tex2D(FullSampler, IN.TexCoord).rgrg;
}

float4 PSCopyBump(VS_OUTPUT IN): COLOR
{
	return tex2D(FullSampler, IN.TexCoord).baba;
}

float4 PSCopyRain(VS_OUTPUT IN): COLOR
{
#if __PS3
	float2 bump	= 2*tex2D(FullSampler, IN.TexCoord).ba - 1;
#else
	float2 bump	= 2*tex2D(FullSampler, IN.TexCoord).rg - 1;
#endif

	const int rainScale			= 2;
	const int rainTextureScale	= BUMPRTSIZE/256;
	const float texMip			= log2(rainScale/rainTextureScale);

	float2 rainBump				= 2*tex2Dlod(PointSampler2, float4(IN.TexCoord*rainScale, 0, texMip)).rg - 1;
	float2 bumpWithRain			= bump + 4*rainBump*Wetness;

#if __PS3
	return float4(bumpWithRain, bump)*.5 + .5;
#else
   	return float4(bumpWithRain, 1.0, 1.0)*.5 + .5;
#endif
}

#define CAUSTICSDEPTH 7
float2 SampleCausticsBump(float2 tex)
{
#if __PS3
	return 2*tex2D(BumpSampler, tex).rg - 1;
#else //__PS3
	return 2*tex2Dlod(BumpSampler, float4(tex, 0, 0)).rg - 1;
#endif //__PS3
}
float3 CausticsCommon(float2 texCoord)
{
	float texelSize = 1.0/128;

	float2 pos = texCoord;
	float2 tex = pos/2;

	//texScale should be 1 for best tiling but I liked the scaled look more, I really should be using a lower mip but it wasn't setup properly
	float texScale = .25;
	tex *= texScale;
	tex	+=.5;
	float2 bump = SampleCausticsBump(tex);
	//move the vertex position based on the water bump map and depth for the refraction effect
	pos = pos + bump*CAUSTICSDEPTH*texelSize;

	//check the refracted positions of your neighbors
	float2 posUp	= pos + float2(0,  texelSize);
	float2 bump0	= SampleCausticsBump(tex + float2(0,  texelSize*texScale));
	posUp			= posUp + bump0*CAUSTICSDEPTH*texelSize;

	float2 posDown	= pos + float2(0, -texelSize);
	float2 bump1	= SampleCausticsBump(tex + float2(0, -texelSize*texScale));
	posDown			= posDown + bump1*CAUSTICSDEPTH*texelSize;

	float2 posLeft	= pos + float2(-texelSize, 0);
	float2 bump2	= SampleCausticsBump(tex + float2(-texelSize*texScale, 0));
	posLeft			= posLeft + bump2*CAUSTICSDEPTH*texelSize;

	float2 posRight	= pos + float2( texelSize, 0);
	float2 bump3	= SampleCausticsBump(tex + float2( texelSize*texScale, 0));
	posRight		= posRight + bump3*CAUSTICSDEPTH*texelSize;

	pos = pos + bump*CAUSTICSDEPTH*texelSize;

	float lum = 0;
	//check the distances to your neighbors to get your luminance value
	lum += 1/(.1 + 128*distance(pos, posUp));
	lum += 1/(.1 + 128*distance(pos, posDown));
	lum += 1/(.1 + 128*distance(pos, posLeft));
	lum += 1/(.1 + 128*distance(pos, posRight));

	return float3(bump*2, lum*.1);
}

VS_OUTPUT VS_Caustics(VS_INPUT IN)
{
	float texelSize = 1.0/128;

	VS_OUTPUT OUT;

#if __PS3
	float2 position	= IN.TexCoord/128;
	float3 caustics = float3((IN.Color.ab*2 - 1)/.5, IN.Color.g);
#else //__PS3
	float2 position = IN.TexCoord/128;
	float3 caustics = CausticsCommon(position);
#endif //__PS3

	OUT.TexCoord = caustics.z;
	OUT.Position = float4(position + caustics.xy*CAUSTICSDEPTH*texelSize, 0, 1);
	return OUT;
}

float4 PS_Caustics(VS_OUTPUT IN) : COLOR
{
	return pow(IN.TexCoord.r, 1.5);
}

#if __PS3
float4 PS_CausticsVerts(VS_OUTPUT IN) : COLOR
{
	float2 position = (IN.TexCoord - .5)*2;
	float3 caustics = CausticsCommon(position);
	return float4((caustics.rg*.5)*.5 + .5, caustics.b, 1);
}
#endif //__PS3

// ===============================
// technique
// ===============================
technique bump
{
	pass fullupdate
	{
		VertexShader	= compile VERTEXSHADER	VS_watertex();
		PixelShader		= compile PIXELSHADER	PS_UpdateBump()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
	pass copyrain
	{
		VertexShader	= compile VERTEXSHADER	VS();
		PixelShader		= compile PIXELSHADER	PSCopyRain()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
#if __XENON || __SHADERMODEL >= 40
	pass fullupdatemrt
	{
		VertexShader	= compile VERTEXSHADER	VS_watertex();
		PixelShader		= compile PIXELSHADER	PSUpdateMRT()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
#else
	pass updatetemp
	{
		VertexShader	= compile VERTEXSHADER	VS_watertex();
		PixelShader		= compile PIXELSHADER	PSUpdateTemp()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
	pass copyheight
	{
		VertexShader	= compile VERTEXSHADER	VS();
		PixelShader		= compile PIXELSHADER	PSCopyHeight()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
	pass copybump
	{
		VertexShader	= compile VERTEXSHADER	VS();
		PixelShader		= compile PIXELSHADER	PSCopyBump()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
#endif //__XENON
}

/*
technique caustics
{
	pass caustics
	{
		alphablendenable	= true;
		srcblend			= one;
		destblend			= one;
		cullmode			= none;
		//fillmode			= wireframe;
		VertexShader	= compile VERTEXSHADER	VS_Caustics();
		PixelShader		= compile PIXELSHADER	PS_Caustics()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass causticsprepass
	{
		VertexShader	= compile VERTEXSHADER	VS_Caustics();
		PixelShader		= compile PIXELSHADER	PS_Caustics()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#if __PS3
	pass causticsverts
	{
		VertexShader	= compile VERTEXSHADER	VS();
		PixelShader		= compile PIXELSHADER	PS_CausticsVerts()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
#endif //__PS3
};
*/

half4 PS_UnderwaterRefractionDownSampleSoft(VS_OUTPUT IN) : COLOR
{
#if __XENON
	float4 color = float4(PointSample7e3(PointSampler,	IN.TexCoord.xy, true).rgb, 1);
#else
	float4 color = float4(tex2D(LinearSampler,	IN.TexCoord.xy).rgb, 1);
#endif //__XENON || __WIN32PC
	//using a max here because on Orbis we get negative colors from sky 
	return UnpackHdr(max(color, 0.0f));
}

half4 PS_UnderwaterRefractionDownSampleDepth(VS_OUTPUT IN) : COLOR
{
#if __XENON
	float4 color = float4(PointSample7e3(PointSampler,	IN.TexCoord.xy, true).rgb, GetSSAODepth(DepthSampler, IN.TexCoord.xy)/512);
#else
	float4 color = float4(tex2D(LinearSampler,	IN.TexCoord.xy).rgb, GetSSAODepth(DepthSampler, IN.TexCoord.xy)/512);
#endif //__XENON || __WIN32PC

	return UnpackHdr(color);
}

float4 PS_FogCompositeHelper(VS_OUTPUTREFRACTION IN, wfloat3 screenColor)
{
	wfloat2 tex					= IN.TexCoord.xy;

	wfloat4 sample1				= w4tex2D(LinearSampler2, tex);

	wfloat4 outColor			= 0;
	
#if __SHADERMODEL >= 40
	if(sample1.g > 0)
#endif //__XENON	|| __SHADERMODEL >= 40
	{
		wfloat4 sample0				= w4tex2D(PointSampler2,	tex);
		wfloat4 sample2				= w4tex2D(PointSampler,		tex);

		wfloat4 refractionColor		= wfloat4(screenColor, screenColor.g? 0 : 1);
		wfloat	saturatedDepth		= sample2.g;

		//Add caustics =============================================================
		float depth					= GetSSAODepth(DepthSampler, tex);
		float3 V					= IN.View.xyz;
#ifdef NVSTEREO
		float fStereoScalar			= StereoToMonoScalar(depth);
		fStereoScalar				*= gProjParams.x;
		float3 StereorizedCamPos	= gViewInverse[3].xyz + gViewInverse[0].xyz * fStereoScalar * -1.0f;
		float3 worldPos				= depth*V + StereorizedCamPos;
#else
		float3 worldPos				= depth*V + gViewInverse[3].xyz;
#endif
		float time					= CausticsOffset;
		float2 bumpTex				= worldPos.xy/50 + time;
		float2 bump					= tex2D(LinearWrapSampler, bumpTex.xy).ga;
		const float2 bumpScale		= float2(1, .3);
		bump						= (bump - 0.5)*bumpScale;

		wfloat3 causticIntensity	= w3tex2D(LinearWrapSampler2, (worldPos.xy - 15*time)*1 + bump)*
									  w3tex2D(LinearWrapSampler2, (worldPos.yx + 15*time)*0.64546 + bump);
		causticIntensity			= causticIntensity*saturatedDepth*10 + 1;
		refractionColor.rgb			= screenColor.rgb*causticIntensity;
		//==========================================================================

		wfloat3	nFogColor			= sample0.rgb;

		wfloat3 fogColor			= pow(nFogColor, 2);
		float waterSurfaceDepth     = getLinearGBufferDepth(tex2D(FullSampler, tex).x, gProjParams.zw);
		wfloat2	depthBlend			= nFogColor.g? GetWaterDepthBlend(max(depth - waterSurfaceDepth, 0), pow(sample2.r, 2), abs(V.z)) : 1;
		saturatedDepth				= nFogColor.g? saturatedDepth : refractionColor.a;

		refractionColor.rgb			= lerp(2*nFogColor.rgb*refractionColor.rgb, refractionColor.rgb, depthBlend.y);

		wfloat	shadow				= sample1.a;

		wfloat3 sunColor			= gFogCompositeDirectionalColor;
		wfloat3 sunDirection		= gDirectionalLight.xyz;
		wfloat3 ambientColor		= gFogCompositeAmbientColor;
		wfloat3 fogLight			= ambientColor + shadow*FogLightIntensity*sunColor;

		wfloat3 waterColor			= fogColor*fogLight;
		//add pierce
		waterColor					= waterColor + FogPierceIntensity*sample2.b*shadow*nFogColor*sunColor;


		waterColor					= lerp(waterColor, refractionColor.xyz, depthBlend.x);

		wfloat4 color				= wfloat4(waterColor, saturatedDepth);

		outColor					= color;
	}
	
	return outColor;
}

float4 PS_FogComposite(VS_OUTPUTREFRACTION IN): COLOR
{
	wfloat3 screenColor = 
#if __XENON
		UnpackBackbufferColor(PointSample7e3Offset(LinearSampler, IN.TexCoord.xy, true, -.5).rgb +
		  					  PointSample7e3Offset(LinearSampler, IN.TexCoord.xy, true,  .5).rgb)*0.5; 
#else
		UnpackBackbufferColor(w3tex2D(LinearSampler, IN.TexCoord.xy + gFogCompositeTexOffset.xy).rgb);
#endif //__XENON

	return PS_FogCompositeHelper(IN, screenColor);
}

half4 PS_Downsample(VS_OUTPUT IN) : COLOR
{
#if __XENON
	return _Tex2DOffset(LinearSampler, IN.TexCoord.xy, 1.0.xx);
#else
	return tex2D(LinearSampler, IN.TexCoord.xy);
#endif //__XENON
}

half4 PS_DownsampleTile(VS_OUTPUT IN) : COLOR
{
	float4 color;
#if __XENON
	color = _Tex2DOffset(LinearSampler, IN.TexCoord.xy, 1.0.xx);
#else
	color = tex2D(LinearSampler, IN.TexCoord.xy);
#endif //__XENON

	color.g = color.g > 0? 1 : 0;
	
	return color.g;
}

half4 PS_ShadowBlur(VS_OUTPUT IN) : COLOR
{
	float4 samples[4];

#if __XENON
	samples[0] = _Tex2DOffset(LinearSampler, IN.TexCoord.xy, 0.5.xx + float2( .5,  .5));
	samples[1] = _Tex2DOffset(LinearSampler, IN.TexCoord.xy, 0.5.xx + float2( .5, -.5));
	samples[2] = _Tex2DOffset(LinearSampler, IN.TexCoord.xy, 0.5.xx + float2(-.5,  .5));
	samples[3] = _Tex2DOffset(LinearSampler, IN.TexCoord.xy, 0.5.xx + float2(-.5, -.5));
#else
	float2 texelSize = gooScreenSize*8;
	samples[0] = tex2D(LinearSampler, IN.TexCoord.xy + float2( .5,  .5)*texelSize);
	samples[1] = tex2D(LinearSampler, IN.TexCoord.xy + float2( .5, -.5)*texelSize);
	samples[2] = tex2D(LinearSampler, IN.TexCoord.xy + float2(-.5,  .5)*texelSize);
	samples[3] = tex2D(LinearSampler, IN.TexCoord.xy + float2(-.5, -.5)*texelSize);
#endif

	float4 color = 0;

	color = samples[0] + samples[1] + samples[2] + samples[3];

	color = color*0.25;

	color.g = color.g > 0? max(color.g, 1.0/255) : 0;

	color.a = color.a*(1 - CalcFogShadowDensity(gWaterHeight));

	return color;

}

float GetReflectionCoefficient(float cosineAngle, float n1, float n2)
{
	float term1 = n1*cosineAngle;
	float term2 = n2*(sqrt(1 - pow((n1/n2*sin(acos(cosineAngle))), 2)));
	return pow((term1 - term2)/(term1 + term2), 2);
}

half4 PS_WaterBlend(VS_OUTPUT IN) : COLOR
{
	float interp				= IN.TexCoord.x;
	float cosA					= interp + 0.0001;
	float reflectionCoefficient = GetReflectionCoefficient(cosA, 1, 1.33);
	float foam					= smoothstep(0.5, .7, interp);

	float4 color				= float4(reflectionCoefficient, foam, 0, 0);
	
#if __PS3
	return unpack_4ubyte(pack_2ushort(color.gr));
#endif //__PS3

	return float4(reflectionCoefficient, foam, 0, 0);					
}

float4 PS_RefractionMask(VS_OUTPUT IN) : COLOR
{
	float4 linearDepths;

#if __SHADERMODEL > 40
	linearDepths = LinearWrapTexture3.GatherGreen(LinearWrapSampler3, IN.TexCoord.xy);
#elif __SHADERMODEL == 40
	int2 texCoord = IN.Position.xy*2;
	linearDepths.x = LinearWrapTexture3.Load(int3(texCoord - int2( 0,  0), 0)).g;
	linearDepths.y = LinearWrapTexture3.Load(int3(texCoord - int2( 1,  0), 0)).g;
	linearDepths.z = LinearWrapTexture3.Load(int3(texCoord - int2( 0,  1), 0)).g;
	linearDepths.w = LinearWrapTexture3.Load(int3(texCoord - int2( 1,  1), 0)).g;
#else
	linearDepths = 0;
#endif

	return 1 - all(linearDepths);
}

half4 PS_HiZSetup(VS_OUTPUTREFRACTION IN, out float depth : DEPTH) : COLOR
{
	float linDepth	= tex2D(PointSampler, IN.TexCoord).g;
	float depthBias = 1.00001;
	depth			= saturate((gProjParams.z/linDepth - gProjParams.w)*depthBias);
	return 0;
}

float4 PS_UpscaleShadowMask(VS_OUTPUT IN) : COLOR
{
	return float4(0, 0, tex2D(LinearWrapSampler, IN.TexCoord).r, tex2D(LinearSampler, IN.TexCoord).a);
}

half4 PS_Downsample4x(VS_OUTPUT IN) : COLOR
{
	float4 color	= 0;
	float2 offset	= 1/float2(640, 360);
	color += tex2D(LinearSampler, IN.TexCoord.xy + float2( offset.x,  offset.y));
	color += tex2D(LinearSampler, IN.TexCoord.xy + float2(-offset.x,  offset.y));
	color += tex2D(LinearSampler, IN.TexCoord.xy + float2( offset.x, -offset.y));
	color += tex2D(LinearSampler, IN.TexCoord.xy + float2(-offset.x, -offset.y));
	return color*0.25;
}

technique downsample
{
	pass fogcomposite
	{
		VertexShader		= compile VERTEXSHADER	VS_FogComposite();
		PixelShader			= compile PIXELSHADER	PS_FogComposite()							CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(2));
	}
	pass fogcompositetiled
	{
		VertexShader		= compile VERTEXSHADER	VS_FogCompositeTiled();
		PixelShader			= compile PIXELSHADER	PS_FogComposite()							CGC_FLAGS(CGC_DEFAULTFLAGS_NPC(2));
	}
	pass underwaterdownsampledepth
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterRefractionDownSampleDepth()	CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass underwaterdownsamplesoft
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UnderwaterRefractionDownSampleSoft()		CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass dowmsample
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_Downsample()								CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass dowmsampletile
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_DownsampleTile()							CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass blur
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_ShadowBlur()								CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass waterblend
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_WaterBlend()								CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_UNORM16_ABGR);
	}
	pass refractionmask
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_RefractionMask()							CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass hizsetup
	{
		VertexShader		= compile VERTEXSHADER	VS_FogComposite();
		PixelShader			= compile PIXELSHADER	PS_HiZSetup()								CGC_FLAGS(CGC_DEFAULTFLAGS) PS4_TARGET(FMT_UNORM16_ABGR);
	}
	pass upscalealpha
	{
		VertexShader		= compile VERTEXSHADER	VS();
		PixelShader			= compile PIXELSHADER	PS_UpscaleShadowMask()							CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}


//======================================== From water.fx ==========================================

BeginSampler	(sampler, HeightMapTexture, HeightMapSampler, HeightMapTexture)
ContinueSampler	(sampler, HeightMapTexture, HeightMapSampler, HeightMapTexture)
	AddressU	= BORDER;
	AddressV	= BORDER;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
	BORDERCOLOR = 0x00000000;
EndSampler;

BeginSampler	(sampler, VelocityMapTexture, VelocityMapSampler, VelocityMapTexture)
ContinueSampler	(sampler, VelocityMapTexture, VelocityMapSampler, VelocityMapTexture)
	AddressU	= BORDER;
	AddressV	= BORDER;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
	BORDERCOLOR	= 0x00000000;
EndSampler;


struct VSINPUT {
	float3 pos			: POSITION;
	float2 tex			: TEXCOORD0;
};

struct VSOUTPUT {
	DECLARE_POSITION(pos)
	float4 tex			: TEXCOORD0;
	float4 ScreenPos	: TEXCOORD1;
};

struct WATER_VSINPUT {
	float3 pos			: POSITION;
	float2 tex			: TEXCOORD0;
	float4 color		: COLOR0;
};

VSOUTPUT VS_blit(VSINPUT IN)
{
	VSOUTPUT OUT;
	OUT.pos = float4(float3(IN.pos.xy, 0),1);
	OUT.tex = float4(IN.tex.xy, IN.pos.zz);
	OUT.ScreenPos = rageCalcScreenSpacePosition(OUT.pos);
	return OUT;
}

#if __PS3
float4 FindGridFromWorld(float4 WorldXYXY){ return fmod(((WorldXYXY/DYNAMICGRIDSIZE) + DYNAMICGRIDELEMENTS*1000),DYNAMICGRIDELEMENTS); }
#endif //__PS3

//This technique is not GPU update only
VSOUTPUT VS_UpdateDisturb(WATER_VSINPUT IN)
{
	VSOUTPUT OUT;
	OUT.pos = mul(float4(float3(IN.pos.xy, 0.0),1), gWorldViewProj);
	OUT.tex = float4(IN.tex.xy, IN.color.xy);
	OUT.ScreenPos = rageCalcScreenSpacePosition(OUT.pos);
	return OUT;
}

#define __GPUUPDATE (__XENON || RSG_PC || RSG_DURANGO || RSG_ORBIS)
#if __GPUUPDATE
//=========================== GPU Update Techniques ==============================
VSOUTPUT VS_Update(VSINPUT IN)
{
	VSOUTPUT OUT;

	OUT.pos = mul(float4(float3(IN.pos.xy, 0.0),1), gWorldViewProj);

	OUT.tex = float4(IN.tex.xy, IN.pos.zz);
	OUT.ScreenPos = rageCalcScreenSpacePosition(OUT.pos);
	return OUT;
}

VSOUTPUT VS_UpdateWave(VSINPUT IN)
{
	VSOUTPUT OUT;

	OUT.pos = mul(float4(float3(IN.pos.xy, 0.0),1), gWorldViewProj);

	float2 screenPos= rageCalcScreenSpacePosition(OUT.pos).xy;

#if __XENON
	screenPos = screenPos + .5/DYNAMICGRIDELEMENTS;
#endif

	float2 tex		= IN.tex.xy;

	float2 waveDir	= normalize(tex);
	float waveAmp	= IN.pos.z;
	float mult		= gShoreWavesMult*waveAmp*gTimeStep;
	float3 waveDirN	= float3(-waveDir, 0);
	float3 Y		= float3(0, 0, 1);
	float3 waveDirT	= normalize(cross(waveDirN, Y));
	float2 waveTex	= screenPos + gWorldBase.xy/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS;
	float2 waveTexR;
	waveTexR.x = dot(waveTex, waveDirN.xy);
	waveTexR.y = dot(waveTex, waveDirT.xy);

	float texScale	= DYNAMICGRIDELEMENTS/WAVETEXTURERES;
	OUT.tex.xy		= waveTexR*texScale + gTimeInMilliseconds/50000*float2(1,0);
	OUT.tex.zw		= mult;
	OUT.ScreenPos	= screenPos.xyxy;

	return OUT;
}

float2 TexTile2Lin(float2 tex, float2 xy)
{
	const float2 tileXY=float2(32, 32);
	const float2 blockXY=float2(4, 2);

	tex=floor(tex*xy);

	float tilesPerLine=xy.x/tileXY.x;
	float2 tXY=floor(tex/tileXY);
	float tileNumber= tXY.x+tXY.y*tilesPerLine;
	tXY=tex-tXY*tileXY;

	float blocksPerTile=tileXY.x/blockXY.x;
	float2 bXY=floor(tXY/blockXY);
	float blockNumber=bXY.y*blocksPerTile;
	if(fmod(bXY.y, 8)>3.5)
	{
		blockNumber+=fmod(bXY.x+4, 8);
		//blockNumber+=(fmod(bXY.y, 8)>3.5)?-16:16;
	}
	else
		blockNumber+=bXY.x;

	bXY=tXY-bXY*blockXY;

	float pixelNumber=tileNumber*tileXY.x*tileXY.y+blockNumber*blockXY.x*blockXY.y+bXY.x+bXY.y*blockXY.x;
	return float2(fmod(pixelNumber, xy.x), floor(pixelNumber/xy.x))/xy;
}

OutFloatColor PS_WaterUpdateVelocity(VSOUTPUT IN) : COLOR
{
	float mult=gWaveMult*gTimeStep;

	float2 gridBase =0;

	const float SPRING		= (40.0f);
	float PreCalcSpring		= gTimeStep * SPRING;
	const float FALLBACK	= (0.5f);
	float PreCalcFallBack	= gTimeStep * FALLBACK;
	const float FRICTION	= (0.7);
	float PreCalcFriction	= pow(FRICTION, gTimeStep); 

	float2 tex				= IN.ScreenPos.xy+gScaledBaseOffset;

	//GET VELOCITY
//	float velocity			= clamp(tex2D(VelocityMapSampler, tex.xy).r, -2000, 2000);
	float velocity = tex2D(VelocityMapSampler, tex.xy).r;

	//NOISE
	float offsetHack = 2/255.0; //Old texture was not normalized, but we want to preserve the old heights for now...
	float offset = -0.5;
	offset = offset + offsetHack;

	float texScale = DYNAMICGRIDELEMENTS/WAVETEXTURERES;
	//Read from the r channel of LinearWrapSampler as it only has an r channel, on current gen console this gets propagated to all channels.
	velocity	+= (tex2D(LinearWrapSampler, 4*tex*texScale + gNoiseOffset) + offset).r*gDisturbAmount*gTimeStep;

	float2 world = (IN.tex.xy*DYNAMICGRIDSIZE*DYNAMICGRIDELEMENTS+gWorldBase.xy);

	float2 ht	= tex*texScale + gWorldBase.xy/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS;
	float push	= tex2D(LinearWrapSampler, ht*gWaveLengthScale + float2(gTimeInMilliseconds/400000, gTimeFactorOffset)).r;
	push		= (push + offset)*mult;

	velocity += push;
	//SPRING SIM
	float height=tex2D(HeightMapSampler, tex.xy).r;
	float4 offsets;
	offsets=float4(1,-1,1,-1);
	offsets/=DYNAMICGRIDELEMENTS;

	float4 aHeights0=float4(tex2D(HeightMapSampler, tex+float2(offsets.x, 0)).r,
		tex2D(HeightMapSampler, tex+float2(0, offsets.z)).r,
		tex2D(HeightMapSampler, tex+float2(offsets.y, 0)).r,
		tex2D(HeightMapSampler, tex+float2(0, offsets.w)).r);
	float4 aHeights1=float4(tex2D(HeightMapSampler, tex+float2(offsets.x, offsets.z)).r,
		tex2D(HeightMapSampler, tex+float2(offsets.y, offsets.w)).r,
		tex2D(HeightMapSampler, tex+float2(offsets.y, offsets.z)).r,
		tex2D(HeightMapSampler, tex+float2(offsets.x, offsets.w)).r);
	aHeights0*=0.18;
	aHeights1*=0.07;
	aHeights0+=aHeights1;
	float AverageHeight=aHeights0.x+aHeights0.y+aHeights0.z+aHeights0.w;

	// Spring value
	float	HeightDiff = height - AverageHeight;
	velocity -= HeightDiff * PreCalcSpring;
	// fall back to equilibrium (0.0)
	velocity -= height * PreCalcFallBack;
	// Do some friction too
	velocity *= PreCalcFriction;

	// Velocity can become indefinite (-1.#IND) or arbitrarily large sometimes. Emergency fix.
	velocity = clamp(velocity, -10, 10);
	if(!(abs(velocity) < 9.0f))
	{
	   velocity = 0.0f;
	}

	return CastOutFloatColor(velocity);
}

OutFloatColor PS_WaterUpdateHeight(VSOUTPUT IN) : COLOR
{
#if __XENON
	float2 tex = TexTile2Lin(IN.tex.xy, 128.0.xx);
#else
	float2 tex = IN.tex.xy;
#endif //__XENON
	float2 tex2=tex+gScaledBaseOffset;
	return CastOutFloatColor(clamp(tex2D(HeightMapSampler, tex2).r+gTimeStep*tex2D(VelocityMapSampler, tex.xy).r, -2000, 2000)); 
}

OutFloatColor PS_WaterUpdateDisturb(VSOUTPUT IN) : COLOR
{
#if __SHADERMODEL >= 40
	float2 tex			= IN.ScreenPos.xy/IN.ScreenPos.w;
#else
	float2 tex			= IN.ScreenPos.xy/IN.ScreenPos.w + .5/DYNAMICGRIDELEMENTS;
#endif

	float maxSpeedAllowed	= 5.0;

	float NewSpeed;
	float ChangePercentage;
	float oldDHeight;
	float newDHeight;

	NewSpeed			= IN.tex.x;
	ChangePercentage	= IN.tex.z;

	rageDiscard(abs(tex2D(HeightMapSampler, tex).r) > 4.0f);

	oldDHeight			= tex2D(VelocityMapSampler, tex).r;
	newDHeight			= (NewSpeed * ChangePercentage) + (oldDHeight * (1.0f - ChangePercentage));
	newDHeight			= clamp(newDHeight, min(-maxSpeedAllowed, oldDHeight), max(maxSpeedAllowed, oldDHeight));
	newDHeight			= clamp(newDHeight, -20.0, 20.0);

	oldDHeight			= newDHeight;
	NewSpeed			= IN.tex.y;
	ChangePercentage	= IN.tex.w;

	newDHeight			= (NewSpeed * ChangePercentage) + (oldDHeight * (1.0f - ChangePercentage));
	newDHeight			= clamp(newDHeight, min(-maxSpeedAllowed, oldDHeight), max(maxSpeedAllowed, oldDHeight));
	newDHeight			= clamp(newDHeight, -20.0, 20.0);

	// Velocity can become indefinite (-1.#IND) or arbitrarily large sometimes. Emergency fix.
	newDHeight = clamp(newDHeight, -10, 10);
	if(!(abs(newDHeight) < 9.0f))
	{
	   newDHeight = 0.0f;
	}

	return CastOutFloatColor(newDHeight);
}

OutFloatColor PS_WaterUpdateCalming(VSOUTPUT IN) : COLOR { 

#if __SHADERMODEL >= 40
	float2 tex			= IN.ScreenPos.xy/IN.ScreenPos.w;
#else
	float2 tex			= IN.ScreenPos.xy/IN.ScreenPos.w  + .5/DYNAMICGRIDELEMENTS;
#endif

	float dampening		= IN.tex.x;
	float dampenMult	= pow(dampening, gTimeStep);

	float dampedVel = tex2D(VelocityMapSampler, tex).r * dampenMult;

	// Velocity can become indefinite (-1.#IND) or arbitrarily large sometimes. Emergency fix.
	dampedVel = clamp(dampedVel, -10, 10);
	if(!(abs(dampedVel) < 9.0f))
	{
	   dampedVel = 0.0f;
	}

	return CastOutFloatColor(dampedVel);
}

#if __SHADERMODEL >= 40
	float
#else
	float4
#endif
	PS_WaterUpdateWave(VSOUTPUT IN) : COLOR {

	float2 screenPos	= IN.ScreenPos.xy;
	float2 tex			= IN.tex.xy;
	float mult			= IN.tex.z;

	float push	= (1.723*tex2D(LinearWrapSampler, tex).r - 0.723)*mult;
	//float push	= (2*tex2D(LinearWrapSampler, tex).r - 1)*mult;

	float velocity = tex2D(VelocityMapSampler, screenPos).r + push;

	// Velocity can become indefinite (-1.#IND) or arbitrarily large sometimes. Emergency fix.
	velocity = clamp(velocity, -10, 10);
	if(!(abs(velocity) < 9.0f))
	{
	   velocity = 0.0f;
	}

	return velocity;
}

float4 PS_WaterUpdateWorldBase(VSOUTPUT IN) : COLOR {
	return float4(gWorldBase.xyxy);
}

technique waterupdate
{
	pass updatevelocity //update water velocity height map
	{
		VertexShader = compile VERTEXSHADER	VS_blit();
		PixelShader  = compile PIXELSHADER	PS_WaterUpdateVelocity()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
	}
	pass updateheight //update water height map
	{
		VertexShader = compile VERTEXSHADER	VS_blit();
		PixelShader  = compile PIXELSHADER	PS_WaterUpdateHeight()	CGC_FLAGS(CGC_DEFAULTFLAGS)		PS4_TARGET(FMT_32_R);
	}
	pass updatedisturb //disturb technique
	{
		VertexShader = compile VERTEXSHADER	VS_UpdateDisturb();
		PixelShader  = compile PIXELSHADER	PS_WaterUpdateDisturb()	CGC_FLAGS(CGC_DEFAULTFLAGS)		PS4_TARGET(FMT_32_R);
	}
	pass updatecalming //calming quads technique
	{
		VertexShader = compile VERTEXSHADER	VS_Update();
		PixelShader  = compile PIXELSHADER	PS_WaterUpdateCalming()	CGC_FLAGS(CGC_DEFAULTFLAGS)		PS4_TARGET(FMT_32_R);
	}
	pass updateshorewave //shore wave technique;
	{
		VertexShader = compile VERTEXSHADER	VS_UpdateWave();
		PixelShader  = compile PIXELSHADER	PS_WaterUpdateWave()		CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_R);
	}
	pass updateworldbase
	{
		VertexShader = compile VERTEXSHADER	VS_blit();
		PixelShader  = compile PIXELSHADER	PS_WaterUpdateWorldBase()	CGC_FLAGS(CGC_DEFAULTFLAGS)	PS4_TARGET(FMT_32_GR);
	}
}
#endif //__GPUUPDATE

float4 PS_WaterUpdateWet(VSOUTPUT IN) : COLOR
{
	float texelSize	= 1.0/DYNAMICGRIDELEMENTS;

#if __PS3
	float2 heightTex	= FindGridFromWorld(IN.tex.yxyx*DYNAMICGRIDSIZE*DYNAMICGRIDELEMENTS + gWorldBase.yxyx).xy/DYNAMICGRIDELEMENTS;
	float2 heightTex1	= FindGridFromWorld(IN.tex.yxyx*DYNAMICGRIDSIZE*DYNAMICGRIDELEMENTS + gWorldBase.yxyx + float2( DYNAMICGRIDSIZE, 0).xyxy).xy/DYNAMICGRIDELEMENTS;
	float2 heightTex2	= FindGridFromWorld(IN.tex.yxyx*DYNAMICGRIDSIZE*DYNAMICGRIDELEMENTS + gWorldBase.yxyx + float2(0,  DYNAMICGRIDSIZE).xyxy).xy/DYNAMICGRIDELEMENTS;
	float2 heightTex3	= FindGridFromWorld(IN.tex.yxyx*DYNAMICGRIDSIZE*DYNAMICGRIDELEMENTS + gWorldBase.yxyx + float2(-DYNAMICGRIDSIZE, 0).xyxy).xy/DYNAMICGRIDELEMENTS;
	float2 heightTex4	= FindGridFromWorld(IN.tex.yxyx*DYNAMICGRIDSIZE*DYNAMICGRIDELEMENTS + gWorldBase.yxyx + float2(0, -DYNAMICGRIDSIZE).xyxy).xy/DYNAMICGRIDELEMENTS;
	float2 tex			= IN.tex.xy+gScaledBaseOffset;
#else
	float offset	= 0;
#if __XENON || (__WIN32PC && __SHADERMODEL < 40)
	offset			= 0.5*texelSize;
#endif //__XENON || (__WIN32PC && __SHADERMODEL < 40)
	float2 heightTex	= IN.tex.xy + offset;
	float2 heightTex1	= heightTex + float2( texelSize, 0);
	float2 heightTex2	= heightTex + float2(0,  texelSize);
	float2 heightTex3	= heightTex + float2(-texelSize, 0);
	float2 heightTex4	= heightTex + float2(0, -texelSize);
	float2 tex			= IN.tex.xy + gScaledBaseOffset + offset;
#endif

	float2 wetSample	= tex2D(LinearWrapSampler, tex).rg;
	float oldHeight		= wetSample.x;
	float oldTension	= wetSample.y;
	float4 oldHeights;
	float4 oldTensions;
	wetSample			= tex2D(LinearWrapSampler, tex + float2( texelSize, 0)).rg;
	oldHeights.x		= wetSample.x;
	oldTensions.x		= wetSample.y;
	wetSample			= tex2D(LinearWrapSampler, tex + float2(0,  texelSize)).rg;
	oldHeights.y		= wetSample.x;
	oldTensions.y		= wetSample.y;
	wetSample			= tex2D(LinearWrapSampler, tex + float2(-texelSize, 0)).rg;
	oldHeights.z		= wetSample.x;
	oldTensions.z		= wetSample.y;
	wetSample			= tex2D(LinearWrapSampler, tex + float2(0, -texelSize)).rg;
	oldHeights.w		= wetSample.x;
	oldTensions.w		= wetSample.y;

	float2 lerpFactors	= saturate(10*float2(.25, .5)*gTimeStep);
	float avgOldHeight	= dot(oldHeights,	.25);
	float avgOldTension	= dot(oldTensions,	.25);
	float2 oldVal		= lerp(float2(oldHeight, oldTension), float2(avgOldHeight, avgOldTension), lerpFactors);
	oldHeight			= oldVal.x;
	oldTension			= oldVal.y;
	oldTension			= lerp(oldTension, 0, gTimeStep*.15);

	float height		= tex2D(HeightMapSampler, heightTex).r;
	float4 heights;
	heights.x			= tex2D(HeightMapSampler, heightTex1).r;
	heights.y			= tex2D(HeightMapSampler, heightTex2).r;
	heights.z			= tex2D(HeightMapSampler, heightTex3).r;
	heights.w			= tex2D(HeightMapSampler, heightTex4).r;
	float newHeight		= height/2;
	float newTension	= clamp(height - .1, 0, 2)*gOceanFoamScale;


	float2 lerpFactors2	= saturate(10*float2(0.1, .3)*gTimeStep);

	float2 blendAmounts	= lerp(float2(0.01, 0.001), lerpFactors2, step(float2(oldHeight, oldTension), float2(newHeight, newTension)));
	float2 returnVal	= lerp(float2(oldHeight, oldTension), float2(newHeight, newTension), blendAmounts);

#if __PS3
	return unpack_4ubyte(pack_2ushort(returnVal.gr));
#else
	return float4(returnVal.rgrg);
#endif //__PS3
}

float4 PS_AddFoam(VSOUTPUT IN) : COLOR
{
	float newSpeed;
	float changePercentage;
	float newDHeight;
	float addedFoam		= 0;
	newSpeed			= abs(IN.tex.x);
	changePercentage	= IN.tex.z;
	addedFoam			+= newSpeed*changePercentage;
	newSpeed			= abs(IN.tex.y);
	changePercentage	= IN.tex.w;
	addedFoam			+= newSpeed*changePercentage;

#if __PS3
	return unpack_4ubyte(pack_2ushort(float2(addedFoam, 0)));
#else
	return float4(0, addedFoam, 0, 0);
#endif //__PS3
}

float4 PS_FoamIntensity(VSOUTPUT IN) : COLOR
{
	float2 tex	= IN.tex.xy;

	float fade	= saturate(.5 - length(tex - .5))*2;

#if __XENON
	tex = tex + 1/float2( 640,  360);
#endif
	return saturate(tex2D(PointSampler, tex).g*4)*pow(fade, .25);
}

float4 PS_CopyWetMap(VSOUTPUT IN) : COLOR
{
#if __PS3
	return unpack_4ubyte(pack_2ushort(tex2D(LinearWrapSampler, IN.tex.xy).gr));
#else
	return tex2D(LinearWrapSampler, IN.tex.xy);
#endif //__PS3
}

float4 PS_CombineFoamAndWetMap(VSOUTPUT IN) : COLOR
{
	return tex2D(LinearWrapSampler, IN.tex.xy) + tex2D(LinearWrapSampler2, IN.tex.xy);
}


technique wetupdate
{
	pass updatewet
	{
		VertexShader	= compile VERTEXSHADER	VS_blit();
		PixelShader		= compile PIXELSHADER	PS_WaterUpdateWet()			PS4_TARGET(FMT_UNORM16_ABGR);
	}
	pass addfoam
	{
		VertexShader	= compile VERTEXSHADER	VS_UpdateDisturb();
		PixelShader		= compile PIXELSHADER	PS_AddFoam()				PS4_TARGET(FMT_FP16_ABGR);
	}
	pass foamintensity
	{
		VertexShader	= compile VERTEXSHADER	VS_blit();
		PixelShader		= compile PIXELSHADER	PS_FoamIntensity()			PS4_TARGET(FMT_FP16_ABGR);//Should be FMT_32_R(L8) but throws an assert for now
	}
#if !__XENON
	pass copywet
	{
		VertexShader	= compile VERTEXSHADER	VS_blit();
		PixelShader		= compile PIXELSHADER	PS_CopyWetMap()				PS4_TARGET(FMT_UNORM16_ABGR);
	}
#endif //!__XENON
#if RSG_ORBIS
	pass combinefoamandwetmap
	{
		VertexShader	= compile VERTEXSHADER	VS_blit();
		PixelShader		= compile PIXELSHADER	PS_CombineFoamAndWetMap()	PS4_TARGET(FMT_UNORM16_ABGR);
	}
#endif
}

