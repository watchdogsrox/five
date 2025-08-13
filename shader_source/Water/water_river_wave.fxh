#define USE_SPU_WATER_TRANSFORM (__PS3 && 0)

#include "../../../rage/base/src/shaderlib/rage_calc_noise.fxh"

CBSHARED BeginConstantBufferPagedDX10(wave_globals, b7)

float DWavePeriod1
<
	string UIName = "Period";
	string UIWidget = "Numeric";
	float UIMin = -100.0;
	float UIMax = 100.0;
	float UIStep = 0.1;	
	//int		nostrip = 1;
> = 2.25;

float DWaveAmp1
<
	string UIName = "Amplitude";
	string UIWidget = "Numeric";
	float UIMin = -10.0;
	float UIMax = 10.0;
	float UIStep = 0.01;	
	//int		nostrip = 1;
> = 1.0;

float2 DWaveDirection1
<
	string UIName = "Direction";
	string UIWidget = "Numeric";
	float UIMin = -1.0;
	float UIMax = 1.0;
	float UIStep = 0.01;	
	//int		nostrip = 1;
> = { -1.0, 0 };

float DWavePeriod2
<
	string UIName = "Period for deep wave 2";
	string UIWidget = "Numeric";
	float UIMin = -100.0;
	float UIMax = 100.0;
	float UIStep = 0.1;	
	//int		nostrip = 1;
> = 2.1;

float DWaveAmp2
<
	string UIName = "Amplitude for deep wave 2";
	string UIWidget = "Numeric";
	float UIMin = -10.0;
	float UIMax = 10.0;
	float UIStep = 0.01;	
	//int		nostrip = 1;
> = 0.15;

float2 DWaveDirection2
<
	string UIName = "Direction for deep wave 2";
	string UIWidget = "Numeric";
	float UIMin = -1.0;
	float UIMax = 1.0;
	float UIStep = 0.01;	
	//int		nostrip = 1;
> = { -1.0, 1.1 };

float DWaveNoiseScale
<
	string UIName = "Noise Scale";
	string UIWidget = "Numeric";
	float UIMin = -1000.0;
	float UIMax = 1000.0;
	float UIStep = 0.01;	
	//int		nostrip = 1;
> = 0.002;

float DWaveNoiseDt
<
	string UIName = "Noise Rate of Change";
	string UIWidget = "Numeric";
	float UIMin = -1000.0;
	float UIMax = 1000.0;
	float UIStep = 0.001;	
	//int		nostrip = 1;
> = 0.008;

float DWaveNoiseMagnitude
<
	string UIName = "Noise Magnitude";
	string UIWidget = "Numeric";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.0005;	
	//int		nostrip = 1;
> = 0.0001;

float DWaveDistanceFalloff
<
	string UIName = "Distance Falloff Threshold";
	string UIWidget = "Numeric";
	float UIMin = -10000.0;
	float UIMax = 10000.0;
	float UIStep = 1.0;	
	//int		nostrip = 1;
> = 100.0f;

EndConstantBufferDX10(wave_globals)


float3 DeepWaterWave_And_Deriv( float3 p, float dtoEye, float period, float waveHeight, float3 waveDirection, float windKludge, float t, float distFalloff, out float deriv)
{
	float waveLength = 9.81*(period*period)/(2.0*PI);
	float k = 2.0*PI/waveLength;
	float falloff = saturate( 1.0 - (dtoEye-distFalloff)/distFalloff );
	float r = waveHeight*falloff;
	float omega = 2.0*PI/period;

	float3 pnew = p;
	
	float waveDist = dot(p.xyz,waveDirection);

	float phase = k*waveDist-omega*t;
	pnew.z -= r*cos(phase);
	deriv = k*r*sin(phase);
	return pnew;
}

void WaterCalculateTangentSpaceBasis( float ddx, float ddy, out float3 N, out float3 T, out float3 B )
{
	const float BumpScale = 1.0000f;

	// compute tangent basis
	T = normalize( float3(BumpScale, 0, -ddx) );
	B = normalize( float3(0, BumpScale, -ddy) );
	N = normalize( float3(-ddx, -ddy, BumpScale ));
}
// a vertex shader function which does one parallel wave
void WaterParallelWaveVS( inout float4 Po, float dtoEye, float ampScale, out float3 N, out float3 T, out float3 B, bool useSpuTransform )
{
	float3 P = Po.xyz;

	DWaveNoiseMagnitude*=100;
	float time=-gUmGlobalTimer;
	time/=15;
	// sum waves
	float dx = 0.0, dy = 0.0;
	/*
#if USE_SPU_WATER_TRANSFORM
	if (useSpuTransform)
	{
		// Eventually this will be the VS, but until we get the new water resouces
		// we need to do most of the shader.
#if 1
		float deriv = Po.w;
		dx += deriv * DWaveDirection1.x;
		dy += deriv * DWaveDirection1.y;
#else
		float mixto = Po.w;
	
		float deriv = 0;
	  	P.xyz += DeepWaterWave_And_Deriv( Po.xyz, dtoEye, DWavePeriod1*mixto, DWaveAmp1*ampScale, float3(DWaveDirection1.x, 0, DWaveDirection1.y), 0, gUmGlobalTimer,DWaveDistanceFalloff, deriv) - Po.xyz;
		dx += deriv * DWaveDirection1.x;
		dy += deriv * DWaveDirection1.y;
#endif
	}
	else
#endif // USE_SPU_WATER_TRANSFORM
	*/
	{
		float mixto = 1.0-DWaveNoiseMagnitude + (noise(Po.xzy*DWaveNoiseScale + float3( 0, time*DWaveNoiseDt,  0))*2.0*DWaveNoiseMagnitude);
	
		float deriv = 0;
	  	P.xyz += DeepWaterWave_And_Deriv( Po.xyz, dtoEye, DWavePeriod1*mixto*3+ampScale*0.002, DWaveAmp1*ampScale, normalize(float3(DWaveDirection1.xy, 0)), 0, time,DWaveDistanceFalloff*4, deriv) - Po.xyz;
		dx += deriv * DWaveDirection1.x;
		dy += deriv * DWaveDirection1.y;
	}

	WaterCalculateTangentSpaceBasis(dx, dy, N, T, B);

	Po.xyz = P;
}

//@RSNE @END -Geordi
