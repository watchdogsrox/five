#ifndef __RAGE_ALTERNATIVECOLORSPACES_FXH
#define __RAGE_ALTERNATIVECOLORSPACES_FXH

// RGB12 format
// requires two 8-bit per channel render targets
// stores bits 1 - 8 of each color channel in render target 0 and
// bits 4 - 12 of each color channel in render target 1
// the 4 bit overlap is to account for alpha blending

#define VALUE_RANGE 8.0f
#define EIGHT_BIT_RANGE 255.0f
#define FOUR_BIT_RANGE 15.0f // four bits shared between hi and lo byte
#define FOUR_BIT_SHIFT 16.0f

// You need in each shader a similar output structure for encoding
struct RGB12PixelOutput
{
	float4 Color0	: COLOR0;
	float4 Color1	: COLOR1;
};

RGB12PixelOutput EncodeRGB12(float4 ColorAlpha)
{
	RGB12PixelOutput OUT=(RGB12PixelOutput)0;

	float3 fhi;
	modf(ColorAlpha.rgb / VALUE_RANGE * EIGHT_BIT_RANGE / FOUR_BIT_SHIFT, fhi);
	fhi = fhi / (EIGHT_BIT_RANGE / FOUR_BIT_SHIFT);
	float3 flow = (ColorAlpha.rgb / VALUE_RANGE - fhi) * FOUR_BIT_RANGE;

	// alpha channel is stored in both render targets
	OUT.Color0= float4(flow, ColorAlpha.a);
	OUT.Color1= float4(fhi, ColorAlpha.a);
	
	return OUT;
}

float4 DecodeRGB12(sampler2D OneToEight, sampler2D FourToTwelve, float2 Texcoord)
{
	float4 Low = tex2D(OneToEight, Texcoord);
	float4 High = tex2D(FourToTwelve, Texcoord);

	// store 
	return float4(High.xyz * VALUE_RANGE + Low.xyz * (VALUE_RANGE / FOUR_BIT_RANGE), Low.w);
}

float3 DecodeLuv(float4 Luv)
{
	//
	// x = 9u' / (6u'-16v'+12)
	// y = 4v' / (6u'-16v'+12)
	//float2 xy = float2(9.0f * Luv.y, 4.0f * Luv.z) * 1.0 / (6.0f * Luv.y - 16.0f * Luv.z + 12.0f);
	float2 xy = float2(9.0f, 4.0f) * Luv.yz / (dot(Luv.yz, float2(6.0f, -16.0f)) + 12.0f);

	//float Ld = Luv.x * (2.0f / 16777251.0f) + Luv.w * 2.0f;
	//float Ld = Luv.w * 8.0 + Luv.x * (8.0f / 255.0f);
	float Ld = dot(Luv.wx, float2(8.0f, 8.0f / 255.0f));

	// Yxy -> XYZ conversion
	//float3 XYZ = float3(Ld * xy.x / xy.y, Ld, Ld * (1.0f - xy.x - xy.y) / xy.y);
	float3 XYZ = float3(xy.x, Ld, 1.0f - xy.x - xy.y);
	XYZ.xz = XYZ.xz * Ld / xy.y;

	// XYZ -> RGB conversion
	const float3x3 XYZ2RGB =
	{
		2.5651f,	-1.1665f,	-0.3986f,
		-1.0217f,	1.9777f,	0.0439f,
		0.0753f,	-0.2543f,	1.1892f
	};
	
	return mul(XYZ2RGB, XYZ);
}

float4 EncodeLuv(float3 rgba)
{
	// RGB -> XYZ conversion
	const float3x3 RGB2XYZ =
	{
		0.5141364f,	0.3238786f,		0.16036376f,
		0.265068f,	0.67023428f,	0.06409157f,
		0.0241188f,	0.1228178f,		0.84442666f
	};
	float3 XYZ = mul(RGB2XYZ, rgba.rgb); 

	// u and v convert to Luv
	// u' = 4*X/(X + 15*Y + 3*Z)
	// v' = 9*Y/(X + 15*Y + 3*Z)
	//float2 uv = float2(4.0f * XYZ.x, 9.0f * XYZ.y) * (1.0f / (XYZ.x + 15.0f * XYZ.y + 3.0f * XYZ.z));
	float2 uv = float2(4.0f, 9.0f) * XYZ.xy / dot(XYZ, float3(1.0f, 15.0f, 3.0f));

	float fhi;
	modf(XYZ.y / 8.0f * 255.0f, fhi);
	fhi /= 255.0f;
	float flow = (XYZ.y / 8.0f - fhi) * 255.0f;

	// L lower range | u | v | L higher range
	return float4(flow, uv.x, uv.y, fhi);
}

#endif // __RAGE_ALTERNATIVECOLORSPACES_FXH
