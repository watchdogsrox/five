// 
// debug_DTQ.fx
// 
// Copyright (C) 2010-2010 Rockstar Games.  All Rights Reserved.
// 

// DTQ stands for "Draw Textured Quad"

#if !defined(SHADER_FINAL)

#if RSG_PC && __SHADERMODEL == 40
	#undef __SHADERMODEL
	#define __SHADERMODEL 41 // cubemap arrays require this
#endif

#pragma dcl position diffuse texcoord0
#include "../common.fxh"
#include "../Util/macros.fxh"

#define MAX_ANISOTROPY 16 // whatever the hardware supports

DECLARE_SAMPLER(sampler2D, previewTexture2D, previewSampler2D,
	AddressU  = WRAP;
	AddressV  = WRAP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);
DECLARE_SAMPLER(sampler2D, previewTexture2DFiltered, previewSampler2DFiltered,
	AddressU  = WRAP;
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	ANISOTROPIC_LEVEL(MAX_ANISOTROPY)
);
DECLARE_SAMPLER(sampler3D, previewTexture3D, previewSampler3D,
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);
DECLARE_SAMPLER(sampler3D, previewTexture3DFiltered, previewSampler3DFiltered,
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	ANISOTROPIC_LEVEL(MAX_ANISOTROPY)
);
DECLARE_SAMPLER(samplerCUBE, previewTextureCube, previewSamplerCube,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);
DECLARE_SAMPLER(samplerCUBE, previewTextureCubeFiltered, previewSamplerCubeFiltered,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	ANISOTROPIC_LEVEL(MAX_ANISOTROPY)
);
#if __SHADERMODEL >= 40
DECLARE_DX10_SAMPLER_ARRAY(float4, previewTexture2DArray, previewSampler2DArray, 
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);
DECLARE_DX10_SAMPLER_ARRAY(float4, previewTexture2DArrayFiltered, previewSampler2DArrayFiltered, 
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	ANISOTROPIC_LEVEL(MAX_ANISOTROPY)
);

#if __SHADERMODEL >= 41
DECLARE_DX10_SAMPLER_INTERNAL(sampler, TextureCubeArray<float4>, previewTextureCubeArray, previewSamplerCubeArray, 
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = POINT;
	MINFILTER = POINT;
	MAGFILTER = POINT;
);
DECLARE_DX10_SAMPLER_INTERNAL(sampler, TextureCubeArray<float4>, previewTextureCubeArrayFiltered, previewSamplerCubeArrayFiltered, 
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	AddressW  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	ANISOTROPIC_LEVEL(MAX_ANISOTROPY)
);
#endif // __SHADERMODEL >= 41
#endif // __SHADERMODEL >= 40

BeginConstantBufferDX10(dqt_globals);

// colour matrix
float4 param0;
float4 param1;
float4 param2;
float4 param3;
float4 param4;

// stuff
float4 param5; // x=LOD, y=type, z=layer, w=slice
float4 param6; // xyz=L, w=bumpiness (FilteredSpecular)
float4 param7; // x=exponent, y=scale (FilteredSpecular), z=cubezoom/volume coord swap, w=cubedots
float4 param8; // x=cylinderradius, y=cylinderheight

float4x4 cubemapRotation;

EndConstantBufferDX10(dqt_globals);

struct DTQVertexIn
{
	float3 pos		: POSITION;
	float4 colour   : COLOR0;
	float2 texcoord : TEXCOORD0;
};

struct DTQVertexOut
{
	DECLARE_POSITION(pos)
	float4 colour   : COLOR0;
	float2 texcoord : TEXCOORD0;
};

DTQVertexOut VS_DTQ_BANK_ONLY(DTQVertexIn IN)
{
	DTQVertexOut OUT;
	OUT.pos = float4(IN.pos.xyz,1.0f);
	OUT.colour = IN.colour;
	OUT.texcoord = IN.texcoord;
	
	return OUT;
}

float4 DTQApplyColourTransform(float4 c)
{
	return float4(dot(c,param0),dot(c,param1),dot(c,param2),dot(c,param3)) + param4;
}

float4 DTQApplySpecular(float4 c, float2 texcoord)
{
	const float3 n = CalculateWorldNormal(c.xy, param6.w, float3(1,0,0), float3(0,1,0), float3(0,0,1));
	const float3 l = normalize(float3(texcoord*2 - 1 - param6.xy, param6.z));

	float4 result = pow(dot(n, l), param7.x)*param7.y;

	result.xyz = max(0, result.xyz - (1 - float3(1,0,0))*pow(l.z, param7.x*8)); // red dot in the centre
	result.w = 1; // no alpha

	return result;
}

// NOTE: this enum must be kept in sync between
// %RS_CODEBRANCH%\game\debug\TextureViewer\TextureViewerDTQ.h, and
// %RS_CODEBRANCH%\game\shader_source\Debug\debug_DTQ.fx
#define eDTQTT_2D        0
#define eDTQTT_3D        1
#define eDTQTT_Cube      2
#define eDTQTT_2DArray   3
#define eDTQTT_3DArray   4 // not used
#define eDTQTT_CubeArray 5

// NOTE: this enum must be kept in sync between
// %RS_CODEBRANCH%\game\debug\TextureViewer\TextureViewerDTQ.h, and
// %RS_CODEBRANCH%\game\shader_source\Debug\debug_DTQ.fx
#define eDTQCP_Faces            0
#define eDTQCP_4x3Cross         1
#define eDTQCP_Panorama         2
#define eDTQCP_Cylinder         3
#define eDTQCP_MirrorBall       4
#define eDTQCP_Spherical        5
#define eDTQCP_SphericalSquare  6
#define eDTQCP_Paraboloid       7
#define eDTQCP_ParaboloidSquare 8

float3 DTQCubemapProject(float2 texcoord, int cubeProjection, bool cubeProjClip, float faceZoom, float cylinderRadius, float cylinderHeight)
{
	float3 dir = 0;

	if (cubeProjection == eDTQCP_Faces)
	{
		dir.xz = float2(-1,1)*(texcoord*2 - 1)*faceZoom;
		dir.y = -1;
	}
	else if (cubeProjection == eDTQCP_4x3Cross) // show 4x3 cross
	{
		const float2 tc = texcoord*float2(4,3); // [0..4],[0..3]
		const float u = frac(tc.x)*2 - 1; // [-1..1]
		const float v = frac(tc.y)*2 - 1; // [-1..1]
		const int2 face = (int2)floor(tc);

		if (face.x == 1)
		{
			if (face.y == 0) { dir = float3(-u,-v,-1); } // -Z
			if (face.y == 1) { dir = float3(-u,-1,+v); } // -Y
			if (face.y == 2) { dir = float3(-u,+v,+1); } // +Z
		}
		else if (face.y == 1)
		{
			if (face.x == 0) { dir = float3(+1,-u,+v); } // +X
			if (face.x == 2) { dir = float3(-1,+u,+v); } // -X
			if (face.x == 3) { dir = float3(+u,+1,+v); } // +Y
		}

		if (all(dir.xyz == 0))
		{
			//rageDiscard(true);
			return 0; // transparent
		}
	}
	else if (cubeProjection == eDTQCP_Panorama)
	{
		const float sinTheta = sin(-2.0*PI*(texcoord.x - 0.25));
		const float cosTheta = cos(-2.0*PI*(texcoord.x - 0.25));
		const float sinPhi   = sin(0.5*PI*(texcoord.y*2 - 1));
		const float cosPhi   = cos(0.5*PI*(texcoord.y*2 - 1));

		dir.x = cosPhi*cosTheta;
		dir.y = cosPhi*sinTheta;
		dir.z = sinPhi;
	}
	else if (cubeProjection == eDTQCP_Cylinder)
	{
		const float sinTheta = sin(-2.0*PI*(texcoord.x - 0.25));
		const float cosTheta = cos(-2.0*PI*(texcoord.x - 0.25));

		dir.x = cylinderRadius*cosTheta;
		dir.y = cylinderRadius*sinTheta;
		dir.z = cylinderHeight*(texcoord.y - 0.5);
	}
	else if (cubeProjection == eDTQCP_MirrorBall || cubeProjection == eDTQCP_Spherical || cubeProjection == eDTQCP_SphericalSquare)
	{
		const float u = texcoord.x*2 - 1; // [-1..1]
		const float v = texcoord.y*2 - 1; // [-1..1]

		const float d2 = (cubeProjection == eDTQCP_SphericalSquare) ? max(u*u, v*v) : (u*u + v*v);

		if (cubeProjClip && d2 > 1)
		{
			//rageDiscard(true);
			return 0; // transparent
		}

		const float z = (cubeProjection == eDTQCP_MirrorBall) ? (1 - 2*d2) : cos(PI*sqrt(d2));

		dir.y = -z;
		dir.x = -u*sqrt(abs(1.0f - z*z)/d2);
		dir.z = +v*sqrt(abs(1.0f - z*z)/d2);
	}
	else if (cubeProjection == eDTQCP_Paraboloid || cubeProjection == eDTQCP_ParaboloidSquare)
	{
		const float u = frac(texcoord.x*2)*2 - 1; // [-1..1,-1..1]
		const float v = texcoord.y*2 - 1; // [-1..1]

		const float d = (cubeProjection == eDTQCP_ParaboloidSquare) ? max(abs(u), abs(v)) : sqrt(u*u + v*v);

		if (cubeProjClip && d > 1)
		{
			//rageDiscard(true);
			return 0; // transparent
		}

		dir.y = 1 - d;
		dir.x = u*2;
		dir.z = v*2;

		if (texcoord.x < 0.5)
		{
			dir.xy *= -1;
		}
	}

	return dir;
}

float4 DTQSample(float2 texcoord, bool bFiltered)
{
	const float mip     = param5.x;
	const float type    = param5.y;
	const int   layer   = (int)param5.z;
	const bool  opaque  = (param5.z != floor(param5.z));
	const float slice   = param5.w;
	const float zoom    = param7.z; // also used for volume texture coordinate swap
	const float dots    = param7.w;
	const float cradius = param8.x;
	const float cheight = param8.y;

	const int cubeProjection = param7.z < 0 ? -(int)param7.z : eDTQCP_Faces;
	const bool cubeProjClip = (param7.z != floor(param7.z)); // if true, reject pixels outside of cube projection

	float4 c = 0;
	float4 texcoord2D = float4(texcoord, (float)layer, mip);
	float4 texcoord3D = float4(texcoord, slice, mip);
	float4 texcoordCube = float4(0, 0, 0, mip);

	if (type == eDTQTT_Cube || type == eDTQTT_CubeArray)
	{
		texcoordCube.xyz = DTQCubemapProject(texcoord, cubeProjection, cubeProjClip, zoom, cradius, cheight);
		texcoordCube.xyz = mul((float3x3)cubemapRotation, texcoordCube.xyz);

		if (all(texcoordCube.xyz == 0))
		{
			return float4(0,0,0,0.3); // dim
		}
	}

	if (zoom == 1) { texcoord3D.xyz = texcoord3D.zyx; }
	if (zoom == 2) { texcoord3D.xyz = texcoord3D.xzy; }

	if (bFiltered)
	{
		if (type == eDTQTT_2D)        { c = tex2Dlod(previewSampler2DFiltered, texcoord2D); }
		if (type == eDTQTT_3D)        { c = tex3Dlod(previewSampler3DFiltered, texcoord3D); }
		if (type == eDTQTT_Cube)      { c = texCUBElod(previewSamplerCubeFiltered, texcoordCube); }
#if __SHADERMODEL >= 40
		if (type == eDTQTT_2DArray)   { c = previewTexture2DArrayFiltered.SampleLevel(previewSampler2DArrayFiltered, texcoord2D.xyz, mip); }
		if (type == eDTQTT_3DArray)   { c = 0; }
#if __SHADERMODEL >= 41
		if (type == eDTQTT_CubeArray) { c = previewTextureCubeArrayFiltered.SampleLevel(previewSamplerCubeArrayFiltered, float4(texcoordCube.xyz, layer), mip); }
#endif // __SHADERMODEL >= 41
#endif // __SHADERMODEL >= 40
	}
	else
	{
		if (type == eDTQTT_2D)        { c = tex2Dlod(previewSampler2D, texcoord2D); }
		if (type == eDTQTT_3D)        { c = tex3Dlod(previewSampler3D, texcoord3D); }
		if (type == eDTQTT_Cube)      { c = texCUBElod(previewSamplerCube, texcoordCube); }
#if __SHADERMODEL >= 40
		if (type == eDTQTT_2DArray)   { c = previewTexture2DArray.SampleLevel(previewSampler2DArray, texcoord2D.xyz, mip); }
		if (type == eDTQTT_3DArray)   { c = 0; }
#if __SHADERMODEL >= 41
		if (type == eDTQTT_CubeArray) { c = previewTextureCubeArray.SampleLevel(previewSamplerCubeArray, float4(texcoordCube.xyz, layer), mip); }
#endif // __SHADERMODEL >= 41
#endif // __SHADERMODEL >= 40
	}

	if (mip == -1 && type == eDTQTT_Cube)
	{
		c = lerp(c, texCUBE(previewSamplerCubeFiltered, texcoordCube.xyz),  bFiltered);
		c = lerp(c, texCUBE(previewSamplerCube        , texcoordCube.xyz), !bFiltered);
	}

#if 0 && __SHADERMODEL >= 41 // TODO -- not sure why this won't compile ..
	if (mip == -1 && type == eDTQTT_CubeArray)
	{
		c = lerp(c, previewTextureCubeArrayFiltered.Sample(previewSamplerCubeArrayFiltered, float4(texcoordCube.xyz, layer)),  bFiltered);
		c = lerp(c, previewTextureCubeArray        .Sample(previewSamplerCubeArray        , float4(texcoordCube.xyz, layer)), !bFiltered);
	}
#endif // __SHADERMODEL >= 41

	if (type == eDTQTT_Cube || type == eDTQTT_CubeArray) // overlay dots to show cubemap directions
	{
		const float3 dir = normalize(texcoordCube.xyz);

		c.xyz += (float3(1.0f,0.0f,0.0f) - c.xyz)*dots*saturate(1.5*pow(max(0, +dir.x), 32)); // +X=red
		c.xyz += (float3(0.0f,1.0f,0.0f) - c.xyz)*dots*saturate(1.5*pow(max(0, +dir.y), 32)); // +Y=green
		c.xyz += (float3(0.0f,0.0f,1.0f) - c.xyz)*dots*saturate(1.5*pow(max(0, +dir.z), 32)); // +Z=blue
		c.xyz += (float3(0.0f,1.0f,1.0f) - c.xyz)*dots*saturate(1.5*pow(max(0, -dir.x), 32)); // -X=cyan
		c.xyz += (float3(1.0f,1.0f,0.0f) - c.xyz)*dots*saturate(1.5*pow(max(0, -dir.y), 32)); // -Y=yellow
		c.xyz += (float3(1.0f,0.0f,1.0f) - c.xyz)*dots*saturate(1.5*pow(max(0, -dir.z), 32)); // -Z=magenta

		if (opaque)
		{
			c.w = 1;
		}
	}

	return c;
}

float4 PS_DTQ_BANK_ONLY(DTQVertexOut IN) : COLOR
{
	float4 OUT;
#if 0 // render as graph, for debugging linear/sRGB gradients
	const float w = 0.25;
	const float y = (1 - (IN.texcoord.y - 0.5)*2)*w;
	const float4 c = tex2Dlod(previewSampler2D, float4(IN.texcoord*float2(w,1), 0, param5.x));
	if (IN.texcoord.y > 0.5) { return float4(float3(1,1,1)*(y < c.x ? 1 : 0), 1); }
	OUT = IN.colour*DTQApplyColourTransform(c)/w;
#else
	const float4 c = DTQSample(IN.texcoord, false);
	OUT = IN.colour*DTQApplyColourTransform(c);
#endif
	OUT.w = 1 - OUT.w;
	return OUT;
}

float4 PS_DTQ_BANK_ONLY_NoTexture(DTQVertexOut IN) : COLOR
{
	float4 OUT = IN.colour;
	OUT.w = 1 - OUT.w;
	return OUT;
}

float4 PS_DTQ_BANK_ONLY_Filtered(DTQVertexOut IN) : COLOR
{
	const float4 c = DTQSample(IN.texcoord, true);
	float4 OUT = IN.colour*DTQApplyColourTransform(c);
	OUT.w = 1 - OUT.w;
	return OUT;
}

float4 PS_DTQ_BANK_ONLY_Specular(DTQVertexOut IN) : COLOR
{
	const float4 c = DTQSample(IN.texcoord, false);
	float4 OUT = DTQApplySpecular(c, IN.texcoord);
	OUT.w = 1 - OUT.w;
	return OUT;
}

float4 PS_DTQ_BANK_ONLY_FilteredSpecular(DTQVertexOut IN) : COLOR
{
	const float4 c = DTQSample(IN.texcoord, true);
	float4 OUT = DTQApplySpecular(c, IN.texcoord);
	OUT.w = 1 - OUT.w;
	return OUT;
}

technique DTQ
{
	pass p0 // default
	{
		VertexShader = compile VERTEXSHADER VS_DTQ_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DTQ_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p1
	{
		VertexShader = compile VERTEXSHADER VS_DTQ_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DTQ_BANK_ONLY_NoTexture() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p2
	{
		VertexShader = compile VERTEXSHADER VS_DTQ_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DTQ_BANK_ONLY_Filtered() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p3
	{
		VertexShader = compile VERTEXSHADER VS_DTQ_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DTQ_BANK_ONLY_Specular() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

	pass p4
	{
		VertexShader = compile VERTEXSHADER VS_DTQ_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_DTQ_BANK_ONLY_FilteredSpecular() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ================================================================================================

struct DTQDebugVertexIn
{
	float3 pos : POSITION;
};

struct DTQDebugVertexOut
{
	DECLARE_POSITION(pos)
};

DTQDebugVertexOut VS_dbg_BANK_ONLY(DTQDebugVertexIn IN)
{
	DTQDebugVertexOut OUT;
	//OUT.pos = mulWorldViewProj(IN.pos.xyz);
	OUT.pos = mul(float4(IN.pos.xyz, 1), gWorldViewProj);
	return OUT;
}

float4 PS_dbg_BANK_ONLY() : COLOR
{
	return float4(1,0,0,1);
}

technique dbg
{
	pass p0 // default
	{
		VertexShader = compile VERTEXSHADER VS_dbg_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_dbg_BANK_ONLY() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // !defined(SHADER_FINAL)

