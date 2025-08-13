#pragma dcl position

#include "../common.fxh"
#if (__WIN32PC && __SHADERMODEL >= 40)
//--------------------------------------------------------------------------------------
// 
// BlurCS_50.fx
// 
// code snippets from AMD SeparableFilter sample codes
// Gaussian blur filter (horizontal/vertical) implemented
//--------------------------------------------------------------------------------------

#define LDS_PRECISION               32
#define USE_APPROXIMATE_FILTER      1        
#define KERNEL_RADIUS               6   // Must be an even number

// Defines that control the CS logic of the kernel 
#define STEP_SIZE					2
#define KERNEL_DIAMETER             (KERNEL_RADIUS * 2 + 1)//33
#define RUN_LINES                   2//2//2*2   // Needs to match g_uRunLines in SeparableFilter11.cpp  
#define RUN_SIZE                    128//128*2 // Needs to match g_uRunSize in SeparableFilter11.cpp  
#define KERNEL_DIAMETER_MINUS_ONE	(KERNEL_DIAMETER - 1)//32
#define RUN_SIZE_PLUS_KERNEL	    (RUN_SIZE + KERNEL_DIAMETER_MINUS_ONE)//160
#define PIXELS_PER_THREAD           2
#define NUM_BLUR_THREADS            RUN_SIZE / PIXELS_PER_THREAD
#define SAMPLES_PER_THREAD          (RUN_SIZE_PLUS_KERNEL / (NUM_BLUR_THREADS))
#define EXTRA_SAMPLES               ( RUN_SIZE_PLUS_KERNEL - ( (NUM_BLUR_THREADS) * SAMPLES_PER_THREAD ) )
#define PI                      3.1415927f
#define GAUSSIAN_DEVIATION      (/*KERNEL_RADIUS*/16 * 0.5f)

Texture2D<float4> InputTex : register( t0 );
RWTexture2D<float4> Result : register( u0 );

// The samplers
SamplerState g_PointSampler : register(s1);

SamplerState g_LinearClampSampler : register(s2);

cbuffer cb0
{
    float4  g_f4SampleWeights[15];
    float4  g_f4RTSize;
}

struct LDS
{
	float3 Item[RUN_LINES][RUN_SIZE_PLUS_KERNEL];
};

groupshared LDS g_f3LDS;

//groupshared float3  g_f3LDS[RUN_LINES][RUN_SIZE_PLUS_KERNEL];

#define WRITE_TO_LDS( _f3Color, _iLineOffset, _iPixelOffset ) \
    g_f3LDS.Item[_iLineOffset][_iPixelOffset] = _f3Color;

#define READ_FROM_LDS( _iLineOffset, _iPixelOffset, _f3Color ) \
    _f3Color = g_f3LDS.Item[_iLineOffset][_iPixelOffset];

// CS output structure
struct CS_Output
{
    float4 f4Color[PIXELS_PER_THREAD]; 
};

struct KernelData
{
    float fWeight;
    float fWeightSum;
    float fCenterDepth;
    float fCenterFocal;
};

//--------------------------------------------------------------------------------------
// Get a Gaussian weight
// The weights get compiled to constants, so the cost for this macro disappears 
//--------------------------------------------------------------------------------------
#define GAUSSIAN_WEIGHT( _fX, _fDeviation, _fWeight ) \
    _fWeight = 1.0f / sqrt( 2.0f * PI * _fDeviation * _fDeviation ); \
    _fWeight *= exp( -( _fX * _fX ) / ( 2.0f * _fDeviation * _fDeviation ) );


//--------------------------------------------------------------------------------------
// Sample from chosen input(s)
//--------------------------------------------------------------------------------------
#define SAMPLE_FROM_INPUT( _Sampler, _f2SamplePosition, _f3Color ) \
    _f3Color = InputTex.SampleLevel( _Sampler, _f2SamplePosition, 0 ).xyz;


//--------------------------------------------------------------------------------------
// Compute what happens at the kernels center 
//--------------------------------------------------------------------------------------
#define KERNEL_CENTER( _KernelData, _iPixel, _iNumPixels, _O, _f3Color ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) { \
        GAUSSIAN_WEIGHT( 0, GAUSSIAN_DEVIATION, _KernelData[_iPixel].fWeight ) \
        _KernelData[_iPixel].fWeightSum = _KernelData[_iPixel].fWeight; \
        _O.f4Color[_iPixel].xyz = _f3Color[_iPixel] * _KernelData[_iPixel].fWeight; }     


//--------------------------------------------------------------------------------------
// Compute what happens for each iteration of the kernel 
//--------------------------------------------------------------------------------------
#define KERNEL_ITERATION( _iIteration, _KernelData, _iPixel, _iNumPixels, _O, _f3Color ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) { \
        GAUSSIAN_WEIGHT( ( _iIteration - KERNEL_RADIUS + ( 1.0f - 1.0f / float( STEP_SIZE ) ) ), GAUSSIAN_DEVIATION, _KernelData[_iPixel].fWeight ) \
        _KernelData[_iPixel].fWeightSum += _KernelData[_iPixel].fWeight; \
        _O.f4Color[_iPixel].xyz += _f3Color[iPixel] * _KernelData[_iPixel].fWeight; }


//--------------------------------------------------------------------------------------
// Perform final weighting operation 
//--------------------------------------------------------------------------------------
#define KERNEL_FINAL_WEIGHT( _KernelData, _iPixel, _iNumPixels, _O ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) { \
        _O.f4Color[_iPixel].xyz /= _KernelData[_iPixel].fWeightSum; \
        _O.f4Color[_iPixel].w = 1.0f; }
        

//--------------------------------------------------------------------------------------
// Output to chosen UAV 
//--------------------------------------------------------------------------------------
#define KERNEL_OUTPUT( _i2Center, _i2Inc, _iPixel, _iNumPixels, _O, _KernelData ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) \
        Result[_i2Center + _iPixel * _i2Inc] = _O.f4Color[_iPixel];

//--------------------------------------------------------------------------------------
// Samples from inputs defined by the SampleFromInput macro
//--------------------------------------------------------------------------------------
float3 Sample( int2 i2Position, float2 f2Offset )
{
    float3 RDI;
    float2 f2SamplePosition = float2( i2Position ) + float2( 0.5f, 0.5f );

    f2SamplePosition += f2Offset;

    f2SamplePosition *= g_f4RTSize.zw;
    SAMPLE_FROM_INPUT( g_LinearClampSampler, f2SamplePosition, RDI )
          
    return RDI;
}


//--------------------------------------------------------------------------------------
// Macro for caching LDS reads, this has the effect of drastically reducing reads from the 
// LDS by up to 4x
//--------------------------------------------------------------------------------------
#define CACHE_LDS_READS( _iIteration, _iLineOffset, _iPixelOffset, _RDI ) \
    /* Trickle LDS values down within the GPRs*/ \
    [unroll] for( iPixel = 0; iPixel < PIXELS_PER_THREAD - STEP_SIZE; ++iPixel ) { \
        _RDI[iPixel] = _RDI[iPixel + STEP_SIZE]; } \
    /* Load new LDS value(s) */ \
    [unroll] for( iPixel = 0; iPixel < STEP_SIZE; ++iPixel ) { \
        READ_FROM_LDS( _iLineOffset, ( _iPixelOffset + _iIteration + iPixel ), _RDI[(PIXELS_PER_THREAD - STEP_SIZE + iPixel)] ) }


//--------------------------------------------------------------------------------------
// Defines the filter kernel logic. User supplies macro's for custom filter
//--------------------------------------------------------------------------------------
void ComputeFilterKernel( int iPixelOffset, int iLineOffset, int2 i2Center, int2 i2Inc )
{
    CS_Output O = (CS_Output)0;
    KernelData KD[PIXELS_PER_THREAD];
    int iPixel, iIteration;
    float3 RDI[PIXELS_PER_THREAD];

    // Read the kernel center values in directly from the input surface(s), as the LDS
    // values are pre-filtered, and therefore do not represent the kernel center
    [unroll] 
    for( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )  
    {
        float2 f2SamplePosition = ( float2( i2Center + ( iPixel * i2Inc ) ) + float2( 0.5f, 0.5f ) ) * g_f4RTSize.zw;
        SAMPLE_FROM_INPUT( g_PointSampler, f2SamplePosition, RDI[iPixel] )
    }

    // Macro defines what happens at the kernel center
    KERNEL_CENTER( KD, iPixel, PIXELS_PER_THREAD, O, RDI )
        
    // Prime the GPRs for the first half of the kernel
    [unroll]
    for( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )
    {
        READ_FROM_LDS( iLineOffset, ( iPixelOffset + iPixel ), RDI[iPixel] )
    }

    // Increment the LDS offset by PIXELS_PER_THREAD
    iPixelOffset += PIXELS_PER_THREAD;

    // First half of the kernel
    [unroll]
    for( iIteration = 0; iIteration < KERNEL_RADIUS; iIteration += STEP_SIZE )
    {
        // Macro defines what happens for each kernel iteration  
        KERNEL_ITERATION( iIteration, KD, iPixel, PIXELS_PER_THREAD, O, RDI )

        // Macro to cache LDS reads in GPRs
        CACHE_LDS_READS( iIteration, iLineOffset, iPixelOffset, RDI ) 
    }

    // Prime the GPRs for the second half of the kernel
    [unroll]
    for( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )
    {
        READ_FROM_LDS( iLineOffset, ( iPixelOffset - PIXELS_PER_THREAD + iIteration + 1 + iPixel ), RDI[iPixel] )
    }
    
    // Second half of the kernel
    [unroll]
    for( iIteration = KERNEL_RADIUS + 1; iIteration < KERNEL_DIAMETER; iIteration += STEP_SIZE )
    {
        // Macro defines what happens for each kernel iteration  
        KERNEL_ITERATION( iIteration, KD, iPixel, PIXELS_PER_THREAD, O, RDI )

        // Macro to cache LDS reads in GPRs
        CACHE_LDS_READS( iIteration, iLineOffset, iPixelOffset, RDI )
    }
    
    // Macros define final weighting and output 
    KERNEL_FINAL_WEIGHT( KD, iPixel, PIXELS_PER_THREAD, O )
    KERNEL_OUTPUT( i2Center, i2Inc, iPixel, PIXELS_PER_THREAD, O, KD )
}

[numthreads( NUM_BLUR_THREADS, RUN_LINES, 1 )]
void CSVerticalFilter( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID )
{
    // Sampling and line offsets from group thread IDs
    int iSampleOffset = GTid.x * SAMPLES_PER_THREAD;
    int iLineOffset = GTid.y;
    
    // Group and pixel coords from group IDs
    int2 i2GroupCoord = int2( ( Gid.x * RUN_LINES ), ( Gid.y * RUN_SIZE ) - KERNEL_RADIUS );
    int2 i2Coord = int2( i2GroupCoord.x, i2GroupCoord.y + iSampleOffset );
    
    // Sample and store to LDS
    [unroll]
    for( int i = 0; i < SAMPLES_PER_THREAD; ++i )
    {
        WRITE_TO_LDS( Sample( i2Coord + int2( GTid.y, i ), float2( 0.0f, 0.5f ) ), iLineOffset, iSampleOffset + i )
    }
                   
    // Optionally load some extra texels as required by the exact kernel size 
    if( GTid.x < EXTRA_SAMPLES )
    {
        WRITE_TO_LDS( Sample( i2GroupCoord + int2( GTid.y, RUN_SIZE_PLUS_KERNEL - 1 - GTid.x ), float2( 0.0f, 0.5f ) ), iLineOffset, RUN_SIZE_PLUS_KERNEL - 1 - GTid.x )
    }
    
    // Sync threads
    GroupMemoryBarrierWithGroupSync();

    // Adjust pixel offset for computing at PIXELS_PER_THREAD
    int iPixelOffset = GTid.x * PIXELS_PER_THREAD;
    i2Coord = int2( i2GroupCoord.x, i2GroupCoord.y + iPixelOffset );

    // Since we start with the first thread position, we need to increment the coord by KERNEL_RADIUS 
    i2Coord.y += KERNEL_RADIUS;

    // Ensure we don't compute pixels off screen
    if( i2Coord.y < g_f4RTSize.y  )
    {
        int2 i2Center = i2Coord + int2( GTid.y, 0 );
        int2 i2Inc = int2( 0, 1 );
        
        // Compute the filter kernel using LDS values
        ComputeFilterKernel( iPixelOffset, iLineOffset, i2Center, i2Inc );
    }
}

//--------------------------------------------------------------------------------------
// Compute shader implementing the horizontal pass of a separable filter
//--------------------------------------------------------------------------------------
[numthreads( NUM_BLUR_THREADS, RUN_LINES, 1 )]
void CSHorizFilter( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID )
{
    // Sampling and line offsets from group thread IDs
    int iSampleOffset = GTid.x * SAMPLES_PER_THREAD;
    int iLineOffset = GTid.y;
            
    // Group and pixel coords from group IDs
    int2 i2GroupCoord = int2( ( Gid.x * RUN_SIZE ) - KERNEL_RADIUS, ( Gid.y * RUN_LINES ) );
    int2 i2Coord = int2( i2GroupCoord.x + iSampleOffset, i2GroupCoord.y );
    
    // Sample and store to LDS
    [unroll]
    for( int i = 0; i < SAMPLES_PER_THREAD; ++i )
    {
        WRITE_TO_LDS( Sample( i2Coord + int2( i, GTid.y ), float2( 0.5f, 0.0f ) ), iLineOffset, iSampleOffset + i )
    }

    // Optionally load some extra texels as required by the exact kernel size
    if( GTid.x < EXTRA_SAMPLES )
    {
        WRITE_TO_LDS( Sample( i2GroupCoord + int2( RUN_SIZE_PLUS_KERNEL - 1 - GTid.x, GTid.y ), float2( 0.5f, 0.0f ) ), iLineOffset, RUN_SIZE_PLUS_KERNEL - 1 - GTid.x )
    }
       	
    // Sync threads
    GroupMemoryBarrierWithGroupSync();

    // Adjust pixel offset for computing at PIXELS_PER_THREAD
    int iPixelOffset = GTid.x * PIXELS_PER_THREAD;
    i2Coord = int2( i2GroupCoord.x + iPixelOffset, i2GroupCoord.y );

    // Since we start with the first thread position, we need to increment the coord by KERNEL_RADIUS 
    i2Coord.x += KERNEL_RADIUS;

    // Ensure we don't compute pixels off screen
    if( i2Coord.x < g_f4RTSize.x )
    {
        int2 i2Center = i2Coord + int2( 0, GTid.y );
        int2 i2Inc = int2( 1, 0 );
        
        // Compute the filter kernel using LDS values
        ComputeFilterKernel( iPixelOffset, iLineOffset, i2Center, i2Inc );
    }
}


technique11 draw
{
	pass  p0 //Red Channel: Horizontal Pass 
	{
		SetComputeShader( compileshader( cs_5_0, CSHorizFilter() ) );
	}
	pass p1 //Red Channel: Vertical Pass 
	{
		SetComputeShader( compileshader( cs_5_0, CSVerticalFilter() ) );
	}
}

#else
technique dummy{ pass dummy	{} }
#endif	// (__WIN32PC && __SHADERMODEL >= 40)
