#define SCALEFORM_SHADER

#include "../common.fxh" 

#if HACK_GTA4
	#if __WIN32PC
		#pragma constant 161
	#else
		#pragma constant 216
	#endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Constants

// set constant buffer reg to b11 since nvstereo doesn't like if it happens to be b12
BeginConstantBufferPagedDX10(scaleform_shaders_locals, b11)
float4 UIPosMtx[2]			: UIPosMtx;				
float4 UITex0Mtx[2]			: UITex0Mtx;				
float4 UITex1Mtx[2]			: UITex1Mtx;				
float4 UIColor				: UIColor;				
float4 UIColorXformOffset	: UIColorXformOffset;
float4 UIColorXformScale	: UIColorXformScale;		
float  UIPremultiplyAlpha	: UIPremultiplyAlpha;	
#ifdef NVSTEREO
float  UIStereoFlag			: UIStereoFlag;
#endif
EndConstantBufferDX10(scaleform_shaders_locals)


////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Texture samplers
BeginSampler(		sampler2D,UITexture0_samp,UITexture0,UITexture0_samp)
ContinueSampler(	sampler2D,UITexture0_samp,UITexture0,UITexture0_samp)
EndSampler;

BeginSampler(		sampler2D,UITexture1_samp,UITexture1,UITexture1_samp)
ContinueSampler(	sampler2D,UITexture1_samp,UITexture1,UITexture1_samp)
EndSampler;

BeginSampler(		sampler2D,UITexture2_samp,UITexture2,UITexture2_samp)
ContinueSampler(	sampler2D,UITexture2_samp,UITexture2,UITexture2_samp)
EndSampler;


////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Input Vertex formats

#if __SHADERMODEL < 40
#define XY16i float4
#else
#define XY16i int4
#endif

// This one gets used by drawlinestrip or if edgeAA is turned off. 
// Depending on fill mode we need to generate texture coords from the positon and tex. matrices
// or need to pull CPV color from a shader var.
struct VSIn_SimpleFill { // Vertex_XY16i, Vertex_XY32f
	XY16i position : POSITION;
};

// Sprites come from DrawBitmap. texCoord and color are given directly so no need to specify them
// anywhere else.
struct VSIn_TexturedSprite {
	float4 position : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 color	: COLOR0;	
};

// This is used by edgeAA - texture coords might need to be generated from the position
// (depending on the gouraud fill mode) and color needs to be transformed by the color xform
struct VSIn_GFill_ColorPremult { // Vertex_XY16iC32
	XY16i position	: POSITION;
	float4 color	: COLOR0;
};

// This is used by edgeAA - texture coords might need to be generated from the position
// (depending on the gouraud fill mode) and color needs to be transformed by the color xform
struct VSIn_GFill_ColorBlendfactor { // Vertex_XY16iCF32, Vertex_XY32fCF32
	XY16i position		: POSITION;
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
};

// Not used?
#if 0
struct VSIn_GFill_1TexPrexform { // Vertex_XYUV32fCF32
	float2 position		: POSITION;
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 prexformUV	: TEXCOORD0;
};

struct VSIn_GFill_2TexPrexform { // Vertex_XYUVUV32fCF32
	float2 position		: POSITION;
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 prexformUV0	: TEXCOORD0;
	float2 prexformUV1	: TEXCOORD1;
};
#endif

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Output formats for pixel shader
struct VSOut_SolidFill {
	DECLARE_POSITION(position)
};

struct VSOut_TextureFill {
	DECLARE_POSITION(position)
	float2 texCoord : TEXCOORD0;
};

struct VSOut_TexturedSprite
{
	DECLARE_POSITION(position)
	float2 texCoord : TEXCOORD0;
	float4 color : COLOR0;
};

struct VSOut_GFill_ColorPremult {
	DECLARE_POSITION(position)
	float4 color	: COLOR0;
};

struct VSOut_GFill_Color {
	DECLARE_POSITION(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
};

struct VSOut_GFill_1Texture {
	DECLARE_POSITION(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 texCoord		: TEXCOORD0;
};

struct VSOut_GFill_2Texture {
	DECLARE_POSITION(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
};

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Standard transform functions. Most of the vertex and pixel shaders will be various combinations
// of these functions.

#ifdef NVSTEREO
float4 xformPosition(float4 position,bool bReticle, float ApparentDepth) {
#else
float4 xformPosition(float4 position) {
#endif
	float4 ret = position;
	ret.x = dot(position, UIPosMtx[0]);
	ret.y = dot(position, UIPosMtx[1]);
	ret = mul(ret, gWorldViewProj);
#ifdef NVSTEREO
	if (((UIStereoFlag % 2) == 0) && UIStereoFlag > 0.0f)
	{
		// hack for weapon wheel scaleform ui
		float2 stereoParms = StereoParmsTexture.Load(int3(0,0,0)).xy;
		ret.x -= stereoParms.x * (ret.w - stereoParms.y) * gWWheelDepthScale;
	}
	if ((bReticle || gStereorizeReticule) && (ret.w != 0.0f) && (ApparentDepth > 0.0f))
	{
		ret *= (ApparentDepth / ret.w);
	}
	else if ((gMinimapRendering == 1.0f) && ((ApparentDepth == 0.0f) && (ret.w != 1.0f)))//	// needed for minimap
	{
		ret = StereoToMonoClipSpace(ret);
	}
	else if (gStereorizeHud)
		ret = MonoToStereoClipSpace(ret);
#endif
	return ret;
}

float2 xformTexCoord0(float4 position) {
	float2 ret;
	ret.x = dot(position, UITex0Mtx[0]);
	ret.y = dot(position, UITex0Mtx[1]);
	return ret;
}

float2 xformTexCoord1(float4 position) {
	float2 ret;
	ret.x = dot(position, UITex1Mtx[0]);
	ret.y = dot(position, UITex1Mtx[1]);
	return ret;
}

float4 xformColor(float4 color) {
	return color * UIColorXformScale + UIColorXformOffset;
}

float4 adjustColorForBlending(float4 color) {
	// Maybe premultiply the color by the alpha
	float4 outcolor;
	outcolor.a = color.a;
	outcolor.rgb = UIPremultiplyAlpha == 0.0f ? color.rgb : color.rgb * color.aaa;

#if HACK_GTA4
	// support for custom blending of emissive scaleform movies
	const float emissiveMult = gEmissiveScale*outcolor.a;
	outcolor.b = (gEmissiveScale != 1.0f ? emissiveMult*emissiveMult : outcolor.b);
	
	outcolor.a *= globalAlpha * gInvColorExpBias;
#endif

	// Clamp alpha 0 to 1, who knows the fuck they're doing to it, but it jumps up to 2.5 on 360...
	outcolor.a = saturate(outcolor.a);

	return outcolor;
}

float4 sfToPlatformColor(float4 color)
{
#if __XENON || (__WIN32PC && __SHADERMODEL < 40)
	return color.bgra;
#elif (__WIN32PC && __SHADERMODEL >= 40)
	return color.rgba;
#elif __PS3
	// ARGB -> RGBA
	return color.abgr;
#else
	// ??
	return color.rgba;
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Vertex shaders

VSOut_SolidFill VS_SolidFill(VSIn_SimpleFill IN)
{
	VSOut_SolidFill OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	return OUT;
}

VSOut_TextureFill VS_BitmapFill(VSIn_SimpleFill IN)
{
	VSOut_TextureFill OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	OUT.texCoord = xformTexCoord0(IN.position);
	return OUT;
}

VSOut_TexturedSprite VS_TexturedSprite(VSIn_TexturedSprite IN)
{
	VSOut_TexturedSprite OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	OUT.texCoord = IN.texCoord; // no transformation on texcoord
#if __PS3
	OUT.color.argb = IN.color.rgba; // Source color is ARGB. Convert to RGBA
#elif __WIN32PC && __SHADERMODEL >= 40
	OUT.color = IN.color.bgra;
#elif RSG_ORBIS
	OUT.color = IN.color.bgra;
#else
	OUT.color = IN.color;
#endif
	return OUT;
}

VSOut_GFill_ColorPremult VS_GFill_ColorPremult(VSIn_GFill_ColorPremult IN)
{
	VSOut_GFill_ColorPremult OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	OUT.color = sfToPlatformColor(IN.color);
	return OUT;
}

VSOut_GFill_Color VS_GFill_Color(VSIn_GFill_ColorBlendfactor IN)
{
	VSOut_GFill_Color OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	OUT.color = sfToPlatformColor(IN.color);
	OUT.blendFactor = sfToPlatformColor(IN.blendFactor);
	return OUT;
}


VSOut_GFill_1Texture VS_GFill_1Texture(VSIn_GFill_ColorBlendfactor IN)
{
	VSOut_GFill_1Texture OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	OUT.texCoord = xformTexCoord0(IN.position);
	OUT.color = sfToPlatformColor(IN.color);
	OUT.blendFactor = sfToPlatformColor(IN.blendFactor);
	return OUT;
}

VSOut_GFill_2Texture VS_GFill_2Texture(VSIn_GFill_ColorBlendfactor IN)
{
	VSOut_GFill_2Texture OUT;
#ifdef NVSTEREO
	float ApparentDepth = StereoReticuleDistTexture.Load(int3(0,0,0)).x;
	bool bReticule = (UIStereoFlag == 1.0f);
	OUT.position = xformPosition(IN.position,bReticule,ApparentDepth);
#else
	OUT.position = xformPosition(IN.position);
#endif
	OUT.texCoord0 = xformTexCoord0(IN.position);
	OUT.texCoord1 = xformTexCoord1(IN.position);
	OUT.color = sfToPlatformColor(IN.color);
	OUT.blendFactor = sfToPlatformColor(IN.blendFactor);
	return OUT;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Pixel shader input
struct PSIn_TextureFill {
	DECLARE_POSITION_PSIN(position)
	float2 texCoord : TEXCOORD0;
};

struct PSIn_TexturedSprite {
	DECLARE_POSITION_PSIN(position)
	float2 texCoord : TEXCOORD0;
	float4 color	: COLOR0;
};

struct PSIn_GFill_ColorPremult {
	DECLARE_POSITION_PSIN(position)
	float4 color	: COLOR0;
};

struct PSIn_GFill_Color {
	DECLARE_POSITION_PSIN(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
};

struct PSIn_GFill_1Texture {
	DECLARE_POSITION_PSIN(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 texCoord		: TEXCOORD0;
};

struct PSIn_GFill_1TextureColor {
	DECLARE_POSITION_PSIN(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 texCoord		: TEXCOORD0;
};

struct PSIn_GFill_2Texture {
	DECLARE_POSITION_PSIN(position)
	float4 color		: COLOR0;
	float4 blendFactor	: COLOR1;
	float2 texCoord0	: TEXCOORD0;
	float2 texCoord1	: TEXCOORD1;
};


////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// Pixel shaders


// DrawLineStrip
// DrawIndexedTriList + FillStyleColor
float4 PS_SolidFill() : COLOR {
	return adjustColorForBlending(xformColor(UIColor));
}

// DrawIndexedTriList + FillStyleBitmap
float4 PS_BitmapFill(PSIn_TextureFill IN) : COLOR {
	float4 color = xformColor(tex2D(UITexture0, IN.texCoord));
	return adjustColorForBlending(color);
}

// DrawBitmaps + (texture is Alpha-only)
float4 PS_TexturedSprite_Alpha(PSIn_TexturedSprite IN) : COLOR {
	float4 color = xformColor(IN.color);
	color.a = color.a * tex2D(UITexture0, IN.texCoord).a;
	return adjustColorForBlending(color);
}

// DrawBitmaps + (texture is RGB)
float4 PS_TexturedSprite_Color(PSIn_TextureFill IN) : COLOR {
	float4 color = xformColor(tex2D(UITexture0, IN.texCoord));
	return adjustColorForBlending(color);
}

// DrawIndexedTriList + FillStyleGouraud + GFill_Color + vtx has no blendFactor
float4 PS_GFill_ColorPremult(PSIn_GFill_ColorPremult IN) : COLOR {
	float4 color = xformColor(IN.color);
	return adjustColorForBlending(color);
}

// DrawIndexedTriList + FillStyleGouraud + GFill_Color + vtx has blendFactor
float4 PS_GFill_Color(PSIn_GFill_Color IN) : COLOR {
	float4 color = xformColor(IN.color);
	color.a *= IN.blendFactor.a;
	return adjustColorForBlending(color);
}

// DrawIndedexTriList + FillStyleGouraud + GFill_1Texture + vtx has a blendFactor
float4 PS_GFill_1Texture(PSIn_GFill_1Texture IN) : COLOR {
	float4 color = xformColor(tex2D(UITexture0, IN.texCoord));
	color.a *= IN.blendFactor.a;
	return adjustColorForBlending(color);
}

// DrawIndedexTriList + FillStyleGouraud + GFill_1TextureColor + vtx has a blendFactor
float4 PS_GFill_1TextureColor(PSIn_GFill_1TextureColor IN) : COLOR {
	float4 color = lerp(
		IN.color, 
		tex2D(UITexture0, IN.texCoord), 
		IN.blendFactor.b);
	color = xformColor(color);
	color.a *= IN.blendFactor.a;
	return adjustColorForBlending(color);
}

// DrawIndedexTriList + FillStyleGouraud + GFill_2Texture + vtx has a blendFactor
// Currently also works for GFill_2TextureColor and GFill_3Texture
float4 PS_GFill_2Texture(PSIn_GFill_2Texture IN) : COLOR {
	float4 color = lerp(
		tex2D(UITexture1, IN.texCoord1), 
		tex2D(UITexture0, IN.texCoord0), 
		IN.blendFactor.b);
	color = xformColor(color);
	color.a *= IN.blendFactor.a;
	return adjustColorForBlending(color);
}

/////////////////////////
// Need special shaders for multiplicative or darkening blend modes?

technique sfTechSolidFill
{
    pass P0
    {
		VertexShader =	compile VERTEXSHADER	VS_SolidFill();
		PixelShader =	compile PIXELSHADER		PS_SolidFill();
	}
}

technique sfTechTextureFill
{
    pass P0
    {
		VertexShader =	compile VERTEXSHADER	VS_BitmapFill();
		PixelShader =	compile PIXELSHADER		PS_BitmapFill();
	}
}

technique sfTechAlphaSprite
{
    pass P0
    {
		VertexShader =	compile VERTEXSHADER	VS_TexturedSprite();
		PixelShader =	compile PIXELSHADER		PS_TexturedSprite_Alpha();
	}
}

technique sfTechColorSprite
{
    pass P0
    {
		VertexShader =	compile VERTEXSHADER	VS_TexturedSprite();
		PixelShader =	compile PIXELSHADER		PS_TexturedSprite_Color();
	}
}

technique sfTechGFill_ColorPremult
{
    pass P0
    {
		VertexShader =	compile VERTEXSHADER	VS_GFill_ColorPremult();
		PixelShader =	compile PIXELSHADER		PS_GFill_ColorPremult();
	}
}

technique sfTechGFill_Color
{
    pass P0
    {
		VertexShader =	compile VERTEXSHADER	VS_GFill_Color();
		PixelShader =	compile PIXELSHADER		PS_GFill_Color();
	}
}

technique sfTechGFill_1Texture
{
	pass P0
	{
		VertexShader =	compile VERTEXSHADER	VS_GFill_1Texture();
		PixelShader =	compile PIXELSHADER		PS_GFill_1Texture();
	}
}

technique sfTechGFill_1TextureColor
{
	pass P0
	{
		VertexShader =	compile VERTEXSHADER	VS_GFill_1Texture();
		PixelShader =	compile PIXELSHADER		PS_GFill_1TextureColor();
	}
}

technique sfTechGFill_2Texture
{
	pass P0
	{
		VertexShader =	compile VERTEXSHADER	VS_GFill_2Texture();
		PixelShader =	compile PIXELSHADER		PS_GFill_2Texture();
	}
}

// Gouraud fill types:
//GFill_Color,            // Interpolated with alpha channel is applied.
//GFill_1Texture,         // Texture0 is applied, with alpha modulation from Color.Alpha
//GFill_1TextureColor,    // Texture0 is mixed with vertex Colors based on blendFactors.Alpha.
//GFill_2Texture,         // Texture0 is mixed with Texture1 based on Colors.Alpha.
//GFill_2TextureColor,    // Texture0 is mixed with Texture1 and with vertex colors.
//GFill_3Texture,         // Texture0, Texture1, and Texture2 are mixed together. (not used?)

// PS3 has 3 vertex programs, depending on format and # textures:

//										0			1			2
// PositionOnly							text		1tex		2tex
// PositionColor						0			1tex		2tex
// PositionColorBlendblendFactor				0			1tex		2tex
// PositionColorBlendblendFactor1/2tex		text		1tex		2tex
//

// If color is not specified on the vertex (pos only):
//   If using a texture fill, color is treated as white
//   Else color will come from the FillColor command

// Simple cases: 
// FillTypeColor
//		vertex format will be PositionOnly
//		color specified w/ fill type - local shader var.
//		just transform position, run a color-only pixel shader

// FillTypeTexture
//		vertex format will be position only
//		color should be white
//		tex coord 0 should be generated from position and tex matrix, run 1-texture pixel shader

// DrawBitmaps
//		vertex format will be SpriteVertex
//		transform vertices, color and texture are pass-through
//		Run a 1-texture pixel shader

// GFill_Color:
//		vertex format will be sfPositionColor format (always?)
//		transform vertices, no tex coord. color is specified

// GFill_1Texture:
//		vertex format will be VSIn_GFill_ColorBlendfactor

