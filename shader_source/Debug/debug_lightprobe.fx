#pragma dcl position

#include "../common.fxh"
#include "../Util/macros.fxh"

#if !defined(SHADER_FINAL)

#pragma constant 130

BEGIN_RAGE_CONSTANT_BUFFER(debug_lightprobe_locals, b0)
float4x4 lightProbeRotation;
float4   lightProbeSphere;
float4   lightProbeParams; // x=reflection amount, y=mip index, z=brightness, w=mip blend
EndConstantBufferDX10(debug_lightprobe_locals)

#define MAX_ANISOTROPY 16 // whatever the hardware supports

DECLARE_SAMPLER(samplerCUBE, lightProbeTexture, lightProbeSampler,
	AddressU  = CLAMP;
	AddressV  = CLAMP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	ANISOTROPIC_LEVEL(MAX_ANISOTROPY)
);

// paramsA: xyz=centre, w=radius
// paramsB: xyz=dir, w=length (unused)
// v: view vector (not normalised)
void IntersectCylinder(inout float t0, inout float t1, inout float3 n, float4 paramsA, float4 paramsB, float3 camPos, float3 v)
{
	const float3 a = paramsA.xyz;
	const float3 b = paramsB.xyz;
	const float  r = paramsA.w;
	const float3 w = a - camPos;

	const float vv = dot(v,v);
	const float vw = dot(v,w);
	const float ww = dot(w,w); // const
	const float wb = dot(w,b); // const
	const float vb = dot(v,b);

	const float g2 = ww - wb*wb - r*r; // const

	//if (g2 <= 0) // skip if camera is inside sphere
	{
		const float g1 = vv + vb*vb;
		const float g = vw*vw - g1*g2;

		if (g >= 0)
		{
			const float t0_prev = t0;

			t0 = max(t0, (vw - sqrt(g))/g1);
			t1 = min(t1, (vw + sqrt(g))/g1);

			if (t0 > t0_prev)
			{
				n = -(w - t0*v) + b*dot(b, w - t0*v);
			}
		}
		else // no intersection
		{
			t0 = 1;
			t1 = 0;
		}
	}
}

void IntersectSphere(inout float t0, inout float t1, inout float3 n, float4 paramsA, float3 camPos, float3 v)
{
	float3 a = paramsA.xyz;
	float  r = paramsA.w;
	float3 w = a - camPos;

	float vv = dot(v,v);
	float vw = dot(v,w);
	float ww = dot(w,w); // const

	const float g2 = ww - r*r; // const

	//if (g2 <= 0) // skip if camera is inside sphere
	{
		const float g1 = vv;
		const float g = vw*vw - g1*g2;

		if (g >= 0)
		{
			const float t0_prev = t0;

			t0 = max(t0, (vw - sqrt(g))/g1);
			t1 = min(t1, (vw + sqrt(g))/g1);

			if (t0 > t0_prev)
			{
				n = -(w - t0*v);
			}
		}
		else // no intersection
		{
			t0 = 1;
			t1 = 0;
		}
	}
}

void IntersectSlab(inout float t0, inout float t1, inout float3 n, float4 paramsA, float4 paramsB, float3 camPos, float3 v)
{
	const float3 a = paramsA.xyz;
	const float3 b = paramsB.xyz;
	const float3 w = a - camPos;

	const float wb = dot(w,b); // const
	const float vb = dot(v,b);

	const float t0_prev = t0;

	t0 = max(t0, (wb - paramsB.w)/abs(vb));
	t1 = min(t1, (wb + paramsB.w)/abs(vb));

	if (t0 > t0_prev)
	{
		n = -b;
	}
}

void IntersectCylinderCross(inout float t0, inout float t1, inout float3 n, float4 paramsA, float3 camPos, float3 v)
{
	IntersectCylinder(t0, t1, n, paramsA, float4(1,0,0,1), camPos, v);
	IntersectCylinder(t0, t1, n, paramsA, float4(0,1,0,1), camPos, v);
	IntersectCylinder(t0, t1, n, paramsA, float4(0,0,1,1), camPos, v);
}

void IntersectCylinderCross2(inout float t0, inout float t1, inout float3 n, float4 paramsA, float3 camPos, float3 v)
{
	const float b = sqrt(1.0/3.0);

	IntersectCylinder(t0, t1, n, paramsA, float4(+b,+b,+b,1), camPos, v);
	IntersectCylinder(t0, t1, n, paramsA, float4(+b,-b,+b,1), camPos, v);
	IntersectCylinder(t0, t1, n, paramsA, float4(+b,+b,-b,1), camPos, v);
	IntersectCylinder(t0, t1, n, paramsA, float4(+b,-b,-b,1), camPos, v);
}

void IntersectBoxSphere(inout float t0, inout float t1, inout float3 n, float4 paramsA, float3 camPos, float3 v)
{
	const float boxextent = paramsA.w;
	const float boxfactor = 0.5; // hook up to slider

	// r^2 = (d + 1)^2 + 2
	// boxextent: extent of box
	// boxfactor: 1=box, ->0=sphere
	float d = boxextent/boxfactor;
	float r = sqrt((d + 1)*(d + 1) + 2);
 	const float spherescale = boxextent/(r - d);
	d *= spherescale;
	r *= spherescale;

	// note: for dodecahedron, try this:
	// r = sqrt((d + incr)*(d + incr) + diag^2)
	// incr: dodecahedron incircle radius
	// diag: length of diagonal from centre of pentagonal face to corner = 0.5/sin(PI/5)
	
	IntersectSphere(t0, t1, n, float4(paramsA.xyz + float3(d,0,0), r), camPos, v);
	IntersectSphere(t0, t1, n, float4(paramsA.xyz + float3(0,d,0), r), camPos, v);
	IntersectSphere(t0, t1, n, float4(paramsA.xyz + float3(0,0,d), r), camPos, v);
	IntersectSphere(t0, t1, n, float4(paramsA.xyz - float3(d,0,0), r), camPos, v);
	IntersectSphere(t0, t1, n, float4(paramsA.xyz - float3(0,d,0), r), camPos, v);
	IntersectSphere(t0, t1, n, float4(paramsA.xyz - float3(0,0,d), r), camPos, v);
}

struct vertexInput
{
	float3 pos : POSITION;
	float4 col : COLOR0;
};

struct vertexOutput
{
	float4 pos		: SV_Position;
	float4 col      : COLOR0;
	float3 worldPos : TEXCOORD0;
};

vertexOutput VS_LightProbe_BANK_ONLY(vertexInput IN)
{
	vertexOutput OUT;

	OUT.pos      = mul(float4(IN.pos.xyz, 1), gWorldViewProj);
	OUT.col      = IN.col;
	OUT.worldPos = IN.pos;
	
	return OUT;
}

float4 PS_LightProbe_BANK_ONLY(vertexOutput IN) : COLOR
{
	const float3 camPos = gViewInverse[3].xyz;

	const float3 a = IN.worldPos;
	const float3 b = lightProbeSphere.xyz;
	const float  r = lightProbeSphere.w;
	const float3 v = a - camPos; // view vector
	const float3 w = b - camPos;

	const float vv = dot(v,v);
	const float vw = dot(v,w);
	const float ww = dot(w,w);

	const float g = vw*vw - (ww - r*r)*vv;

	if (g >= 0)
	{
		const float t = (vw - sqrt(g))/vv;
		const float3 p = camPos + v*t; // worldspace position of intersection point
		const float3 n = normalize(p - lightProbeSphere.xyz);
		const float4 R = float4(mul((float3x3)lightProbeRotation, -lerp(n, reflect(v, n), lightProbeParams.x)).xyz, lightProbeParams.y);

		const float4 c0 = texCUBE(lightProbeSampler, R.xyz);
		const float4 c1 = texCUBElod(lightProbeSampler, R);

		return IN.col*float4(lerp(c0, c1, lightProbeParams.w).xyz*lightProbeParams.z, 1);
	}

	return 0;
}

technique lightprobe
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_LightProbe_BANK_ONLY();
		PixelShader  = compile PIXELSHADER  PS_LightProbe_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif // !defined(SHADER_FINAL)
