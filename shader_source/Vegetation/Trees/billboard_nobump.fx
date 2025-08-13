
#ifndef PRAGMA_DCL
	#pragma dcl position normal diffuse texcoord0 
	#define PRAGMA_DCL
#endif
//
// rage_billboard_nobump.fx - default shader used for rendering speedtree billboards;
//
//	16/05/2006	-	Andrzej	- code cleanup, initial;
//
//
//
//
//
#define TREE_DRAW

#define CAN_BE_CUTOUT_SHADER // ?

#define USE_DIFFUSE_SAMPLER

#include "../../common.fxh"
#include "../../Util/skin.fxh"

#define SHADOW_CASTING            (1)
#define SHADOW_CASTING_TECHNIQUES (1 && SHADOW_CASTING_TECHNIQUES_OK)
#define SHADOW_RECEIVING          (0)
#define SHADOW_RECEIVING_VS       (0)
#define SHADOW_USE_TEXTURE
#include "../../Lighting/Shadows/localshadowglobals.fxh"
//#include "../Lighting/Shadows/cascadeshadows.fxh"
#include "../../Lighting/lighting.fxh"

/*
//#if SHADOW_ENABLED
//	#define SPEEDTREE_SHADOW
//	#include "../Lighting/Shadows/localshadowglobals.fxh"
//	#include "../Lighting/Shadows/cascadeshadows.fxh"
//#endif
*/

//
//
//
//

/*

BeginSampler(sampler2D,diffuseBillboardTexture,DiffuseBillboardSampler,DiffuseBillboardTex)
    string UIName = "Diffuse Billboard Texture";
ContinueSampler(sampler2D,diffuseBillboardTexture,DiffuseBillboardSampler,DiffuseBillboardTex)
		AddressU = CLAMP;
		AddressV = CLAMP;
		AddressW = CLAMP;
		MIPFILTER = NONE;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR; 
		MipMapLodBias = 0;
EndSampler;


BeginSampler(sampler2D,NormalMapBillboardTexture,NormalMapBillboardSampler,NormalMapBillboardTex)
    string UIName = "Normal Map  Billboard Texture";
ContinueSampler(sampler2D,NormalMapBillboardTexture,NormalMapBillboardSampler,NormalMapBillboardTex)
		AddressU = CLAMP;
		AddressV = CLAMP;
		AddressW = CLAMP;
		MIPFILTER = MIPLINEAR;
		MINFILTER = HQLINEAR;
		MAGFILTER = MAGLINEAR; 
EndSampler;

//
//
//
//
float2  g_FastMipMapBlend : FastMipMapBlend;
// end 168

float3 g_vUpVector : UpVector = {0.0f, 1.0f, 0.0f};



	
float4 gWorldPositions[128] : WorldPositions	REGISTER2(vs, c120);
	
struct vertexBillboardIn
{
    float4 pos				: POSITION;
    float4 texCoord0		: TEXCOORD0;
};
	
struct vertexBillboardOut
{
    float4 pos				: POSITION;
    float4 texCoord0		: TEXCOORD0;
//	float3 worldPos			: TEXCOORD1;
//	float3 worldNormal		: TEXCOORD2;
};
	
struct vertexBillboardOutD
{
    float4 pos				: POSITION;
    float4 texCoord0		: TEXCOORD0;
	float3 worldNormal		: TEXCOORD2;
};

struct vertexBillboardOutWD
{
	vertexOutputLD wd;
	float alpha : COLOR0;
};

//
//
//
//
vertexBillboardOutD VS_Transform_Billboard(vertexBillboardIn IN )
{
	vertexBillboardOutD OUT;
    
    // Compute rotation basis matrix
    float4 vWorldPos = float4( IN.pos.xyz,1.0f); 
	    
    // Transform by the view projection matrix
    OUT.pos = (float4)mul(vWorldPos, gWorldViewProj);
    
    OUT.texCoord0.xy = IN.texCoord0.xy;
	OUT.texCoord0.z = IN.pos.w;
	OUT.texCoord0.w = 1.0f;

	 // Compute the new view normal
    float3 viewNrm = normalize(gViewInverse[3].xyz - vWorldPos.xyz);

	OUT.worldNormal	= normalize(viewNrm);

    return(OUT);
}
//
//
//
//
//	
float4 PS_Textured_Billboard(vertexBillboardOut IN): COLOR
{
#if SHADOW_ENABLED
	// RAY - temp
	//float4 diffuse = tex2D(DiffuseBillboardSampler, IN.texCoord0.xy);
	float4 diffuse = tex2D(DiffuseBillboardSampler, IN.texCoord0.xy);
#else
	float4 diffuse = tex2D(DiffuseBillboardSampler, IN.texCoord0.xy);
#endif
	
//	if( diffuse.w < 0.392156862f  )
//		diffuse.w = 0.0f;
		
//diffuse *= float4(1,0,0,1);

	return(diffuse);
}

float FakeStippleMask(float2 vpos, float _alpha)
{
	//return _alpha * (fmod(vpos.x,2.0f) * 0.25f + fmod(vpos.y,2.0f) * 0.5f + 0.25f);
	
	// See CShaderLib::SetAlphaStipple for another example of that amajing
	// technique.
	// The range has been modified to take the alpharef at 48 used in the shader.
	float oddX = 1.0f - fmod(vpos.x,2.0f);
	float oddY = 1.0f - fmod(vpos.y,2.0f);

	float4 isPixel = float4(	(1 - oddX) * (1 - oddY),
						oddX * (1 - oddY),
						(1 - oddX) * oddY,
						oddX * oddY);

	isPixel = 1.0f - isPixel;
	//float4 compare = float4(0.0f/255.0f,76.0f/255.0f,102.0f/255.0f,128.0f/255.0f);
	float4 compare = float4(1.0f/255.0f,64.0f/255.0f,128.0f/255.0f,196.0f/255.0f);
	float4 drawPixel = _alpha.xxxx > compare;
	return saturate( dot(isPixel,drawPixel));
}

//
//
//
//
DeferredGBuffer PS_Textured_Billboard_deferred(vertexBillboardOutD IN, float2 vPos : VPOS)
{
DeferredGBuffer OUT;
		
	//surfaceInfo.surface_worldNormal = normalize(float3(0,0,-1)); //IN.worldNormal;
	float4 sampleC=tex2D(NormalMapBillboardSampler, IN.texCoord0.xy);	

	float3 norm;
	norm.xy = (sampleC.xy*2.0f)-1.0f;
	norm.z = sqrt(1.0f-dot(norm.xy,norm.xy))*(sampleC.w-0.5f)*2.0f;
	
	StandardLightingProperties	prop;
	prop.diffuseColor = tex2D( DiffuseBillboardSampler, IN.texCoord0.xy); 
//	prop.diffuseColor.w = (prop.diffuseColor.w  * prop.diffuseColor.w ) * (  saturate( 2.0f * IN.texCoord0.z  ) );
	//prop.diffuseColor.w*=IN.texCoord0.z;

	prop.naturalAmbientScale = sampleC.b;

#if __PS3
		bool stippled = (FakeStippleMask(vPos,   saturate( IN.texCoord0.z * 2.0f ) ) < 1.0f);
		// PS3: there's no AlphaTest in MRT mode, so we simulate AlphaTest functionality here:
		// hack, use green, not alpha, because alpha isn't coming through
		bool killPix =bool(prop.diffuseColor.w < billboard_alphaRef) || stippled;
		rageDiscard(killPix);
#endif //AlphaTest...

	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(norm, prop);

	return(OUT);
}


//
//
//
//
//
#if FORWARD_TECHNIQUES
technique draw 
{
	pass p0 
	{
//		AlphaBlendEnable = false;
//		AlphaTestEnable = true;
///		AlphaRef = 84;
//		AlphaFunc = Greater;
//		CullMode = None;

		VertexShader = compile VERTEXSHADER VS_Transform_Billboard();
		PixelShader  = compile PIXELSHADER  PS_Textured_Billboard()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES





#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
technique deferred_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Transform_Billboard();
		PixelShader  = compile PIXELSHADER  PS_Textured_Billboard_deferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && __PS3
technique deferredalphaclip_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Transform_Billboard();
		PixelShader  = compile PIXELSHADER  PS_Textured_Billboard_deferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES && __PS3
*/


BeginSampler(sampler2D,imposterTexture,imposterSampler,imposterTexture)
    string UIName = "Diffuse Billboard Texture";
ContinueSampler(sampler2D,imposterTexture,imposterSampler,imposterTexture)
		AddressU = CLAMP;
		AddressV = CLAMP;
		AddressW = CLAMP;
		MIPFILTER = NONE;
		MINFILTER = POINT;
		MAGFILTER = POINT; 
#if __XENON || __PS3
		MipMapLodBias = 8.0;
#endif // __XENON || __PS3
EndSampler;


BeginSampler(sampler2D,blitTexture,blitSampler,blitTexture)
    string UIName = "Blit Texture";
ContinueSampler(sampler2D,blitTexture,blitSampler,blitTexture)
		AddressU = WRAP;
		AddressV = WRAP;
		AddressW = WRAP;
		MIPFILTER = NONE;
		MINFILTER = POINT;
		MAGFILTER = POINT; 
#if __XENON || __PS3
		MipMapLodBias = 8.0;
#endif // __XENON || __PS3
EndSampler;

struct vertexImposterIN {
    float3 pos			: POSITION;
    float3 norm			: NORMAL;
	float4 color0		: COLOR0;
    float2 texCoord		: TEXCOORD0;
};

struct vertexImposterOUT {
    DECLARE_POSITION(pos)
	float4 color0		: COLOR0;

	float4 texBlend		: TEXCOORD0;

	float4 mask0A		: TEXCOORD1;
	float4 mask0B		: TEXCOORD2;
	float4 mask1A		: TEXCOORD3;
	float4 mask1B		: TEXCOORD4;	
	
	float3 worldX		: TEXCOORD5;
	float3 worldY		: TEXCOORD6;
	float3 worldZ		: TEXCOORD7;
};

#if __XENON
#if 1 //__PS3
struct vertexImposterShadowOUT {
    float4 pos			: POSITION;
	float4 texBlend		: TEXCOORD0;
};
#else
struct vertexImposterShadowOUT {
    DECLARE_POSITION(pos)
	float4 color0		: COLOR0;
	float4 texBlend		: TEXCOORD0;
	float4 mask0A		: TEXCOORD1;
	float4 mask0B		: TEXCOORD2;
	float4 mask1A		: TEXCOORD3;
	float4 mask1B		: TEXCOORD4;	
};
#endif
#endif // __XENON

/*
float3 imposterDir[6]={	float3(0,0,1), 
						float3(1,0,-0.577), 
						float3(-1,0,-0.577), 
						float3(0,-1.732,-1), 
						float3(1,-2.309,0.577), 
						float3(-1,-2.309,0.577)};*/

#define R3 (0.57735026918962576450914878050196f)

BEGIN_RAGE_CONSTANT_BUFFER(billboard_nobump_locals,b0)
float3 imposterDir[8]={	float3( 1, 0, 0),
						float3( R3, R3,-R3),
						float3( 0, 1, 0), 
						float3(-R3, R3,-R3), 
						float3(-1, 0, 0), 
						float3(-R3,-R3,-R3), 
						float3( 0,-1, 0), 
						float3( R3,-R3,-R3)};

float3 imposterWorldX = float3(1,0,0);
float3 imposterWorldY = float3(0,1,0);
float3 imposterWorldZ = float3(0,0,1);

EndConstantBufferDX10( billboard_nobump_locals )

vertexImposterOUT VS_Imposter(vertexImposterIN IN)
{
	vertexImposterOUT OUT;

    OUT.pos = mul(float4(IN.pos,1), gWorldViewProj);

//	OUT.pos.xy*=0.0f;

	OUT.texBlend.x = IN.texCoord.x; 
	OUT.texBlend.y = 1-IN.texCoord.y; 

	OUT.color0 = IN.color0.rgba;
    
	float4 blendA;
	float4 blendB;
	
	float3 t_norm=normalize(IN.norm.xyz);

#if __XENON
	blendA.x=dot(t_norm, normalize(imposterDir[0]))+1.0f;
	blendA.y=dot(t_norm, normalize(imposterDir[2]))+1.0f;
	blendA.z=dot(t_norm, normalize(imposterDir[4]))+1.0f;
	blendA.w=dot(t_norm, normalize(imposterDir[6]))+1.0f;

	blendB.x=dot(t_norm, normalize(imposterDir[1]))+1.0f;
	blendB.y=dot(t_norm, normalize(imposterDir[3]))+1.0f;
	blendB.z=dot(t_norm, normalize(imposterDir[5]))+1.0f;
	blendB.w=dot(t_norm, normalize(imposterDir[7]))+1.0f;
#else
	blendA.x=dot(t_norm.xyz, float3( 1, 0, 0))+1.0f;
	blendA.y=dot(t_norm.xyz, float3( 0, 1, 0))+1.0f;
	blendA.z=dot(t_norm.xyz, float3(-1, 0, 0))+1.0f;
	blendA.w=dot(t_norm.xyz, float3( 0,-1, 0))+1.0f;

	blendB.x=dot(t_norm.xyz, float3( R3, R3,-R3))+1.0f;
	blendB.y=dot(t_norm.xyz, float3(-R3, R3,-R3))+1.0f;
	blendB.z=dot(t_norm.xyz, float3(-R3,-R3,-R3))+1.0f;
	blendB.w=dot(t_norm.xyz, float3( R3,-R3,-R3))+1.0f;
#endif

	float4 tmpAB0=max(blendA,blendB);
	float tmp0=max(max(tmpAB0.x, tmpAB0.y), max(tmpAB0.z, tmpAB0.w));

	OUT.mask0A=GtaStep(blendA, tmp0);
	OUT.mask0B=GtaStep(blendB, tmp0);

	float4 tmpAB1=max(blendA*(1.0f-OUT.mask0A),blendB*(1.0f-OUT.mask0B));
	float tmp1=max(max(tmpAB1.x, tmpAB1.y), max(tmpAB1.z, tmpAB1.w));

	OUT.mask1A=GtaStep(blendA*(1.0f-OUT.mask0A), tmp1);
	OUT.mask1B=GtaStep(blendB*(1.0f-OUT.mask0B), tmp1);

	float4 tmpAB2=max(blendA*(1.0f-OUT.mask0A)*(1.0f-OUT.mask1A),blendB*(1.0f-OUT.mask0B)*(1.0f-OUT.mask1B));
	float tmp2=max(max(tmpAB2.x, tmpAB2.y), max(tmpAB2.z, tmpAB2.w));

	OUT.texBlend.z=dot(OUT.mask0A*blendA, 1)+dot(OUT.mask0B*blendB, 1);
	OUT.texBlend.w=dot(OUT.mask1A*blendA, 1)+dot(OUT.mask1B*blendB, 1);

	OUT.texBlend.zw-=dot(GtaStep(blendA*(1.0f-OUT.mask0A)*(1.0f-OUT.mask1A), tmp2)*blendA, 1)+dot(GtaStep(blendB*(1.0f-OUT.mask0B)*(1.0f-OUT.mask1B), tmp2)*blendB, 1);

	float rlen=1.0f/(OUT.texBlend.z+OUT.texBlend.w);
	OUT.texBlend.zw*=rlen;	

	OUT.worldX=imposterWorldX;
	OUT.worldY=imposterWorldY;
	OUT.worldZ=imposterWorldZ;

	return OUT;	
}

#if __XENON

vertexImposterShadowOUT VS_ImposterShadow(vertexImposterIN IN)
{
	vertexImposterShadowOUT OUT;

	float4 wpos=float4(IN.pos.xyz,1);
	OUT.pos=0; // this used to work for faceted shadows
	OUT.texBlend.x = IN.texCoord.x; 
	OUT.texBlend.y = 1-IN.texCoord.y; 

	OUT.texBlend.z=(IN.color0.r*16.0f)+8.0f;
	OUT.texBlend.w=(OUT.texBlend.z/64.0f)*0.1f;

//	OUT.color0 = IN.color0.rgba;
    
//#if __PS3
	return OUT;	
//#else
/*
	float4 blendA;
	float4 blendB;
	
	float3 t_norm=normalize(IN.norm.xyz);

#if __XENON
	blendA.x=dot(t_norm, normalize(imposterDir[0]))+1.0f;
	blendA.y=dot(t_norm, normalize(imposterDir[2]))+1.0f;
	blendA.z=dot(t_norm, normalize(imposterDir[4]))+1.0f;
	blendA.w=dot(t_norm, normalize(imposterDir[6]))+1.0f;

	blendB.x=dot(t_norm, normalize(imposterDir[1]))+1.0f;
	blendB.y=dot(t_norm, normalize(imposterDir[3]))+1.0f;
	blendB.z=dot(t_norm, normalize(imposterDir[5]))+1.0f;
	blendB.w=dot(t_norm, normalize(imposterDir[7]))+1.0f;
#else
	blendA.x=dot(t_norm.xyz, float3( 1, 0, 0))+1.0f;
	blendA.y=dot(t_norm.xyz, float3( 0, 1, 0))+1.0f;
	blendA.z=dot(t_norm.xyz, float3(-1, 0, 0))+1.0f;
	blendA.w=dot(t_norm.xyz, float3( 0,-1, 0))+1.0f;

	blendB.x=dot(t_norm.xyz, float3( R3, R3,-R3))+1.0f;
	blendB.y=dot(t_norm.xyz, float3(-R3, R3,-R3))+1.0f;
	blendB.z=dot(t_norm.xyz, float3(-R3,-R3,-R3))+1.0f;
	blendB.w=dot(t_norm.xyz, float3( R3,-R3,-R3))+1.0f;
#endif
	float4 tmpAB0=max(blendA,blendB);
	float tmp0=max(max(tmpAB0.x, tmpAB0.y), max(tmpAB0.z, tmpAB0.w));

	OUT.mask0A=GtaStep(blendA, tmp0);
	OUT.mask0B=GtaStep(blendB, tmp0);

	float4 tmpAB1=max(blendA*(1.0f-OUT.mask0A),blendB*(1.0f-OUT.mask0B));
	float tmp1=max(max(tmpAB1.x, tmpAB1.y), max(tmpAB1.z, tmpAB1.w));

	OUT.mask1A=GtaStep(blendA*(1.0f-OUT.mask0A), tmp1);
	OUT.mask1B=GtaStep(blendB*(1.0f-OUT.mask0B), tmp1);

	float4 tmpAB2=max(blendA*(1.0f-OUT.mask0A)*(1.0f-OUT.mask1A),blendB*(1.0f-OUT.mask0B)*(1.0f-OUT.mask1B));
	float tmp2=max(max(tmpAB2.x, tmpAB2.y), max(tmpAB2.z, tmpAB2.w));

	OUT.texBlend.z=dot(OUT.mask0A*blendA, 1)+dot(OUT.mask0B*blendB, 1);
	OUT.texBlend.w=dot(OUT.mask1A*blendA, 1)+dot(OUT.mask1B*blendB, 1);

	OUT.texBlend.zw-=dot(GtaStep(blendA*(1.0f-OUT.mask0A)*(1.0f-OUT.mask1A), tmp2)*blendA, 1)+dot(GtaStep(blendB*(1.0f-OUT.mask0B)*(1.0f-OUT.mask1B), tmp2)*blendB, 1);

//	float rz=OUT.texBlend.z/(OUT.texBlend.z+OUT.texBlend.w);
//	float rw=1.0f-rz;
//	OUT.texBlend.z=rz;	
//	OUT.texBlend.w=rw; 	

	float rlen=1.0f/(OUT.texBlend.z+OUT.texBlend.w);
	OUT.texBlend.zw*=rlen;	

	return OUT;	
#endif*/
}

#endif // __XENON

float3 normTable[16]={	float3( 0, 0, 0),

						float3( 0, 0,-1),
						float3( 0, 0.5,-0.8660254),
						float3( 0.4330127,-0.25,-0.8660254), 
						float3(-0.4330127,-0.25,-0.8660254),

						float3(0,0.93969262,0.34202014), 
						float3(0.81379768,0.46984631, -0.34202014), 
						float3(0.81379768,-0.46984631, 0.34202014), 
						float3(0,-0.93969262,-0.34202014), 
						float3(-0.81379768,-0.46984631, 0.34202014),
						float3(-0.81379768, 0.46984631,-0.34202014),

						float3(-0.4330127, 0.25,0.8660254), 
						float3( 0.4330127, 0.25,0.8660254),
						float3( 0, -0.5,-0.8660254),
						float3( 0, 0, 1),

						float3( 0, 0, 0)}; 

DeferredGBuffer PS_ImposterDeferred(vertexImposterOUT IN)
{
	DeferredGBuffer OUT;

#if __PS3
	float3 leafColour = tex2D(imposterSampler, float2(1.0f-(1.0f/128.0f),(1.0f/128.0f))).rgb;
#else
	float3 leafColour = tex2D(imposterSampler, float2(1.0f-(1.0f/128.0f),(1.0f/128.0f))).rgb;
#endif

#if __PS3 || __WIN32PC || RSG_ORBIS
	float rand=0.5f;//tex2D(blitSampler, frac(IN.texBlend.xy*8.0f)).a;

//    float4 imposterSample = tex2D(imposterSampler, IN.texBlend.xy+(rand-0.5f)/32.0f)*255.004f;

//FIXME: float4 imposterSample = tex2D(imposterSampler, IN.texBlend.xy)*255.004f;
    float4 imposterSample = tex2D(imposterSampler, IN.texBlend.xy).aaaa*255.004f;
#else
//FIXME: float4 imposterSample = tex2D(imposterSampler, IN.texBlend.xy)*255.004f;
    float4 imposterSample = tex2D(imposterSampler, IN.texBlend.xy).aaaa*255.004f;
#endif

	float4 angA=frac(imposterSample/16.0f)*16.0f;
	float4 angB=(imposterSample-angA)/16.0f;

	float ang0=dot(angA,IN.mask0A)+dot(angB,IN.mask0B);
	float ang1=dot(angA,IN.mask1A)+dot(angB,IN.mask1B);

	float4 col;
	col.rgb=leafColour;//*IN.color0.rgb;
	col.a=(saturate(ang0)*IN.texBlend.z+saturate(ang1)*IN.texBlend.w)*IN.color0.a;

#if __PS3 || __WIN32PC || RSG_ORBIS
	float3 norm0=(tex2D(blitSampler, float2((floor(ang0)/16.0f),rand)).rgb*2.0f)-1.0f;
	float3 norm1=(tex2D(blitSampler, float2((floor(ang1)/16.0f),rand)).rgb*2.0f)-1.0f;
	float3 worldnorm=normalize(norm0*IN.texBlend.z+norm1*IN.texBlend.w);

//	col.rgb*=(rand+0.33333f)*0.75f;
//	col.a+=(rand-0.5f)*0.3333f;
#else
	float3 worldnorm=normalize(normTable[ang0]*IN.texBlend.z+normTable[ang1]*IN.texBlend.w);
#endif
	worldnorm=(((IN.worldX*worldnorm.x)+(IN.worldY*worldnorm.y)+(IN.worldZ*worldnorm.z))*0.5f)+0.5f;

	OUT.col0=col;
	OUT.col1=float4(worldnorm,0.0f);
	OUT.col2=float4(0.0f, 0.5f, 0.5f, 0.0f);
	OUT.col3=float4(1.0f, 1.0f, 0.0f, 0.0f);

#if __PS3 || RSG_ORBIS
	rageDiscard(col.a < 0.5f);
#endif
#if __XENON
	rageDiscard(col.a < 0.01f);
#endif

	return OUT;
}

#if __XENON

float4 PS_ImposterShadow(vertexImposterShadowOUT IN): COLOR
{
	float rand=tex2D(blitSampler, IN.texBlend.xy*IN.texBlend.z).a*IN.texBlend.w;
    float4 imposterSample = tex2D(imposterSampler, IN.texBlend.xy+rand);

#if __PS3
	bool killPix = bool(dot(imposterSample,1.0f) == 0.0f);
	rageDiscard(killPix);
	return 1;
#else
	return float4(0,0,0,dot(imposterSample,1.0f));
#endif
}

#endif // __XENON

struct vertexBlitIN {
    float3 pos			: POSITION;
 //   float2 texCoord		: TEXCOORD0;
};

struct vertexBlitOUT {
    DECLARE_POSITION(pos)
//	float2 texCoord		: TEXCOORD0;
};

vertexBlitOUT VS_Blit(vertexBlitIN IN)
{
	vertexBlitOUT OUT;

	OUT.pos=float4(IN.pos.xyz,1);
//	OUT.texCoord=IN.texCoord;

	return OUT;
}

float4 PS_BlitAverage(vertexBlitOUT IN): COLOR
{
	float4 average=0;

	for (int i=0;i<1;i++)
	{
		float x=i/16.0f;

		float4 sample0=tex2D(blitSampler, float2(x,0.0f/16.0f)+(1.0f/32.0f));
		float4 sample1=tex2D(blitSampler, float2(x,1.0f/16.0f)+(1.0f/32.0f));
		float4 sample2=tex2D(blitSampler, float2(x,2.0f/16.0f)+(1.0f/32.0f));
		float4 sample3=tex2D(blitSampler, float2(x,3.0f/16.0f)+(1.0f/32.0f));
		float4 sample4=tex2D(blitSampler, float2(x,4.0f/16.0f)+(1.0f/32.0f));
		float4 sample5=tex2D(blitSampler, float2(x,5.0f/16.0f)+(1.0f/32.0f));
		float4 sample6=tex2D(blitSampler, float2(x,6.0f/16.0f)+(1.0f/32.0f));
		float4 sample7=tex2D(blitSampler, float2(x,7.0f/16.0f)+(1.0f/32.0f));

		float4 sample10=tex2D(blitSampler, float2(x,8.0f/16.0f)+(1.0f/32.0f));
		float4 sample11=tex2D(blitSampler, float2(x,9.0f/16.0f)+(1.0f/32.0f));
		float4 sample12=tex2D(blitSampler, float2(x,10.0f/16.0f)+(1.0f/32.0f));
		float4 sample13=tex2D(blitSampler, float2(x,11.0f/16.0f)+(1.0f/32.0f));
		float4 sample14=tex2D(blitSampler, float2(x,12.0f/16.0f)+(1.0f/32.0f));
		float4 sample15=tex2D(blitSampler, float2(x,13.0f/16.0f)+(1.0f/32.0f));
		float4 sample16=tex2D(blitSampler, float2(x,14.0f/16.0f)+(1.0f/32.0f));
		float4 sample17=tex2D(blitSampler, float2(x,15.0f/16.0f)+(1.0f/32.0f));
#if __XENON
		float4 A=float4(sample0.a, sample1.a, sample2.a, sample3.a);
		float4 B=float4(sample4.a, sample5.a, sample6.a, sample7.a);

		float4 C=float4(sample10.a, sample11.a, sample12.a, sample13.a);
		float4 D=float4(sample14.a, sample15.a, sample16.a, sample17.a);

		float4 bA=GtaStep(A,1.0f);
		float4 bB=GtaStep(B,1.0f);
		float4 bC=GtaStep(C,1.0f);
		float4 bD=GtaStep(D,1.0f);
#else
		float4 A=float4(dot(sample0.rgb,1), dot(sample1.rgb,1), dot(sample2.rgb,1), dot(sample3.rgb,1));
		float4 B=float4(dot(sample4.rgb,1), dot(sample5.rgb,1), dot(sample6.rgb,1), dot(sample7.rgb,1));

		float4 C=float4(dot(sample10.rgb,1), dot(sample11.rgb,1), dot(sample12.rgb,1), dot(sample13.rgb,1));
		float4 D=float4(dot(sample14.rgb,1), dot(sample15.rgb,1), dot(sample16.rgb,1), dot(sample17.rgb,1));

//		float4 A=float4(sample0.a, sample1.a, sample2.a, sample3.a);
//		float4 B=float4(sample4.a, sample5.a, sample6.a, sample7.a);

//		float4 C=float4(sample10.a, sample11.a, sample12.a, sample13.a);
//		float4 D=float4(sample14.a, sample15.a, sample16.a, sample17.a);

		float threshold=0.5f;

		float4 bA=GtaStep(A,threshold);
		float4 bB=GtaStep(B,threshold);
		float4 bC=GtaStep(C,threshold);
		float4 bD=GtaStep(D,threshold);
#endif
		average.xyz+=sample0.xyz*bA.x;
		average.xyz+=sample1.xyz*bA.y;
		average.xyz+=sample2.xyz*bA.z;
		average.xyz+=sample3.xyz*bA.w;

		average.xyz+=sample4.xyz*bB.x;
		average.xyz+=sample5.xyz*bB.y;
		average.xyz+=sample6.xyz*bB.z;
		average.xyz+=sample7.xyz*bB.w;

		average.xyz+=sample10.xyz*bC.x;
		average.xyz+=sample11.xyz*bC.y;
		average.xyz+=sample12.xyz*bC.z;
		average.xyz+=sample13.xyz*bC.w;

		average.xyz+=sample14.xyz*bD.x;
		average.xyz+=sample15.xyz*bD.y;
		average.xyz+=sample16.xyz*bD.z;
		average.xyz+=sample17.xyz*bD.w;

		average.w+=dot(bA,1.0f)+dot(bB,1.0f);
		average.w+=dot(bC,1.0f)+dot(bD,1.0f);
	}

	return float4((average.xyz/average.w)*1.0f,1.0f);
}

#if FORWARD_TECHNIQUES
technique draw 
{
	pass p0 
	{
		//SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);

		AlphaTestEnable = false; 
		AlphaBlendEnable = false; 
		CullMode = None;

		VertexShader = compile VERTEXSHADER VS_Imposter();
		PixelShader  = compile PIXELSHADER  PS_ImposterDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // FORWARD_TECHNIQUES

#if DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES
//technique deferred_imposter
technique deferred_draw
{
	pass p0
	{
		//SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);

		AlphaTestEnable = false; 
		AlphaBlendEnable = false; 
		CullMode = None;

		VertexShader = compile VERTEXSHADER VS_Imposter();
		PixelShader  = compile PIXELSHADER  PS_ImposterDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && NON_CUTOUT_DEFERRED_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredalphaclip_draw
{
	pass p0
	{
		//SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);

		AlphaTestEnable = false; 
		AlphaBlendEnable = false; 
		CullMode = None;

		VertexShader = compile VERTEXSHADER VS_Imposter();
		PixelShader  = compile PIXELSHADER  PS_ImposterDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

#if DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES
technique deferredsubsamplealpha_draw
{
	pass p0
	{
		//SetGlobalDeferredMaterial(DEFERRED_MATERIAL_TREE);

		AlphaTestEnable = false; 
		AlphaBlendEnable = false; 
		CullMode = None;

		VertexShader = compile VERTEXSHADER VS_Imposter();
		PixelShader  = compile PIXELSHADER  PS_ImposterDeferred()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // DEFERRED_TECHNIQUES && CUTOUT_OR_SSA_TECHNIQUES

//technique shadow_imposter

technique blit_average
{
	pass p0
	{
		AlphaTestEnable = false; 
		AlphaBlendEnable = false; 
		CullMode = None;

		VertexShader = compile VERTEXSHADER VS_Blit();
		PixelShader  = compile PIXELSHADER  PS_BlitAverage()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
