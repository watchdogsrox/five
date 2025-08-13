//--------------------------------------------------------------------------------------
// File: PS_DOF.hlsl
//
// This file contains the shaders implementing the Diffusion DOF effect
//
// Contributed by AMD Developer Relations Team
//
// Copyright (c) AMD Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma dcl position

#include "../../renderer/PostFX_shared.h"
#include "../common.fxh"

#if DOF_DIFFUSION
#include "DofDiffusion.fxh"

//--------------------------------------------------------------------------------------
// Input and output structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD;
};

struct VS_OUTPUT
{
    DECLARE_POSITION(Pos)
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    DECLARE_POSITION_PSIN(Pos)
};

struct ReduceO
{
    float4 abc : SV_Target0;
    float4 x   : SV_Target1;
};

BeginConstantBufferDX10( cbReduce_locals )
float	LastBufferSize			: LastBufferSize;
float	BufferSize				: BufferSize;
float	DiffusionRadius			: DiffusionRadius;
EndConstantBufferDX10(dofCompute_locals)

#define REDUCE_IO uint4

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// BeginDX10Sampler(sampler, Texture2D<uint4>, txABC_X_Texture, txABC_X_Sampler, txABC_X_Texture)
// ContinueSampler(sampler, txABC_X_Texture, txABC_X_Sampler, txABC_X_Texture)
// 	AddressU	= CLAMP;
// 	AddressV	= CLAMP;
// 	MINFILTER	= POINT;
// 	MAGFILTER	= POINT;
// EndSampler;

BeginDX10Sampler(sampler, Texture2D, txABC_Texture, txABC_Sampler, txABC_Texture)
ContinueSampler(sampler, txABC_Texture, txABC_Sampler, txABC_Texture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginDX10Sampler(sampler, Texture2D, txX_Texture, txX_Sampler, txX_Texture)
ContinueSampler(sampler, txX_Texture, txX_Sampler, txX_Texture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

BeginDX10Sampler(sampler, Texture2D, txYn_Texture, txYn_Sampler, txYn_Texture)
ContinueSampler(sampler, txYn_Texture, txYn_Sampler, txYn_Texture)
	AddressU	= CLAMP;
	AddressV	= CLAMP;
	MINFILTER	= POINT;
	MAGFILTER	= POINT;
EndSampler;

// Texture2D<uint4>                txABC_X        : register( t0 );
// Texture2D                       txABC          : register( t0 );
// Texture2D txX                                  : register( t2 );
// Texture2D txYn                                 : register( t3 );

#define g_vInputImageSize	globalScreenSize

//--------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------

// pack 6 32bit floats into uint4 
// this steals 6 mantissa bits from the 3 32 it fp values
// that hold abc
uint4 pack( float3 abc, float3 x )
{
    uint z = ( f32tof16( x.z ) );
    return uint4(
                  ( ( asuint(abc.x) & 0xffffffc0 ) | ( z & 0x3f ) ),
                  ( ( asuint(abc.y) & 0xffffffc0 ) | ( ( z >> 6 ) & 0x3f ) ),
                  ( ( asuint(abc.z) & 0xffffffc0 ) | ( ( z >> 12 ) & 0x3f ) ),
                  ( f32tof16( x.x ) + ( f32tof16( x.y ) << 16 ) )
                 );
}

struct ABC_X
{
    float3 abc;
    float3 x;
};

ABC_X unpack( uint4 d )
{
    ABC_X res;
    
    res.abc   = asfloat( d.xyz & 0xffffffc0 );
    res.x.xy  = float2( f16tof32( d.w ), f16tof32( d.w >> 16 ) );
    res.x.z   = f16tof32( ( ( d.x & 0x3f ) + ( ( d.y & 0x3f ) << 6 ) + ( ( d.z & 0x3f ) << 12 ) ) );
    
    return res;
}

float det3x3( float3 col1, float3 col2, float3 col3 ) 
{
    return ( col1.x*col2.y*col3.z+col2.x*col3.y*col1.z+col3.x*col1.y*col2.z-col3.x*col2.y*col1.z-col2.x*col1.y*col3.z-col1.x*col3.y*col2.z );
}

// this is the function to override with you own CoC computations
float computeCoC( int3 pos, uniform int2 off )
{
	float coc = DiffusionRadius * txCOC_Texture.Load( pos, off ).y;
   /* float  fCurrentPixelDepth      = txDepthInput.Load( pos, off ).x;
           fCurrentPixelDepth      = ConvertDepthToLinear(fCurrentPixelDepth, g_fQ, g_fQTimesZNear);
    float  fCurrentPixelDOFFactor  = CalculateCircleOfConfusion(g_fDOFAmount, fCurrentPixelDepth, g_fFocalDepth, g_fNoBlurRange, g_fMaxBlurRange);

    return abs(10.0f * fCurrentPixelDOFFactor); // return 10.0f * DOFfactor as CoC*/

	//TODO
	return coc;
}

#define fCoC_4 f4CoC_.w
#define fCoC_3 f4CoC_.z
#define fCoC_2 f4CoC_.y
#define fCoC_1 f4CoC_.x
#define fCoC4 f4CoC.w
#define fCoC3 f4CoC.z
#define fCoC2 f4CoC.y
#define fCoC1 f4CoC.x

#define fRealCoC_4 f4RealCoC_.w
#define fRealCoC_3 f4RealCoC_.z
#define fRealCoC_2 f4RealCoC_.y
#define fRealCoC_1 f4RealCoC_.x
#define fRealCoC3 f4RealCoC.w
#define fRealCoC2 f4RealCoC.z
#define fRealCoC1 f4RealCoC.y
#define fRealCoC0 f4RealCoC.x

#define beta_4 f4Beta_.w
#define beta_3 f4Beta_.z
#define beta_2 f4Beta_.y
#define beta_1 f4Beta_.x
#define beta3 f4Beta.w
#define beta2 f4Beta.z
#define beta1 f4Beta.y
#define beta0 f4Beta.x

// reduce by a factor of 4 getting rid of 4 unkowns in each step
#ifdef PACK_DATA
uint4 InitialReduceHorz4( PS_INPUT input ) : COLOR
#else
ReduceO InitialReduceHorz4( PS_INPUT input )
#endif
{
    ReduceO O;
    float   fPosX       = (floor(input.Pos.x)*4.0f)+3.0f;
    int3    i3LoadPos   = int3( fPosX, input.Pos.y, 0 );
	float4  f4CoC, f4CoC_;
	float4  f4RealCoC, f4RealCoC_;
	float4  f4Beta, f4Beta_;

    fCoC_4  = computeCoC(i3LoadPos, int2( -4, 0) );
    fCoC_3  = computeCoC(i3LoadPos, int2( -3, 0) );
    fCoC_2  = computeCoC(i3LoadPos, int2( -2, 0) );
    fCoC_1  = computeCoC(i3LoadPos, int2( -1, 0 ) );
    float fCoC0   = computeCoC(i3LoadPos, int2(  0 ,0 ) );
    fCoC1   = computeCoC(i3LoadPos, int2(  1, 0 ) );
    fCoC2   = computeCoC(i3LoadPos, int2(  2, 0 ) );
    fCoC3   = computeCoC(i3LoadPos, int2(  3, 0 ) );
    fCoC4   = computeCoC(i3LoadPos, int2(  4, 0 ) );
    
    fCoC_4 = ( fPosX - 4.0f ) >= 0.0f ? fCoC_4 : 0.0f;
	f4CoC = ( fPosX.xxxx + float4( 1.0f, 2.0f, 3.0f, 4.0f ) ) < g_vInputImageSize.xxxx ? f4CoC : (0.0f).xxxx;

    fRealCoC_4   = min( fCoC_4, fCoC_3 );  
    fRealCoC_3   = min( fCoC_3, fCoC_2 );  
    fRealCoC_2   = min( fCoC_2, fCoC_1 );  
    fRealCoC_1   = min( fCoC_1, fCoC0 );  
    fRealCoC0    = min( fCoC0, fCoC1 );  
    fRealCoC1    = min( fCoC1, fCoC2 );  
    fRealCoC2    = min( fCoC2, fCoC3 );  
    fRealCoC3    = min( fCoC3, fCoC4 );  

	f4Beta_ = f4RealCoC_ * f4RealCoC_;
	f4Beta  = f4RealCoC  * f4RealCoC;
   
    float3 abc_3  = float3( -beta_4, 1.0f + beta_3 + beta_4, -beta_3 );
    float3 x_3    = txX_Texture.Load( i3LoadPos, int2( -3, 0 ) ).xyz;

    float3 abc_2  = float3( -beta_3, 1.0f + beta_2 + beta_3, -beta_2 );
    float3 x_2    = txX_Texture.Load( i3LoadPos, int2( -2, 0 ) ).xyz;

    float3 abc_1  = float3( -beta_2, 1.0f + beta_1 + beta_2, -beta_1 );
    float3 x_1    = txX_Texture.Load( i3LoadPos, int2( -1, 0 ) ).xyz;
    
    float3 abc0   = float3( -beta_1, 1.0f + beta0 + beta_1, -beta0 );
    float3 x0     = txX_Texture.Load( i3LoadPos, int2(  0, 0 ) ).xyz;

    float3 abc1   = float3( -beta0, 1.0f + beta1 + beta0, -beta1 );
    float3 x1     = txX_Texture.Load( i3LoadPos, int2(  1, 0 ) ).xyz;

    float3 abc2   = float3( -beta1, 1.0f + beta2 + beta1, -beta2 );
    float3 x2     = txX_Texture.Load( i3LoadPos, int2(  2, 0 ) ).xyz;
    
    float3 abc3   = float3( -beta2, 1.0f + beta3 + beta2, -beta3 );
    float3 x3     = txX_Texture.Load( i3LoadPos, int2(  3, 0 ) ).xyz;
    
    float alpha_1 = -abc_2.x / abc_3.y;
    float gamma_1 = -abc_2.z / abc_1.y;
    
    float alpha0  = -abc0.x / abc_1.y;
    float gamma0  = -abc0.z / abc1.y;
    
    float alpha1  = -abc2.x / abc1.y;
    float gamma1  = -abc2.z / abc3.y;

    float3 abc_p   = float3( alpha_1 * abc_3.x,    
                             abc_2.y + alpha_1 * abc_3.z + gamma_1 * abc_1.x,
                             gamma_1 * abc_1.z );
    float3 x_p     = float3( x_2 + alpha_1 * x_3 + gamma_1 * x_1 );
    
    float3 abc_c   = float3( alpha0 * abc_1.x,    
                             abc0.y + alpha0 * abc_1.z + gamma0 * abc1.x,
                             gamma0 * abc1.z );
    float3 x_c     = float3( x0 + alpha0 * x_1 + gamma0 * x1 );

    float3 abc_n   = float3( alpha1 * abc1.x,    
                             abc2.y + alpha1 * abc1.z + gamma1 * abc3.x,
                             gamma1 * abc3.z );
    float3 x_n     = float3( x2 + alpha1 * x1 + gamma1 * x3 );
    
    float  alpha   = -abc_c.x / abc_p.y;
    float  gamma   = -abc_c.z / abc_n.y;
    
    float3 res0   = float3( alpha * abc_p.x,    
                            abc_c.y + alpha * abc_p.z + gamma * abc_n.x,
                            gamma * abc_n.z );
    float3 res1   = float3( x_c + alpha * x_p + gamma * x_n );

#ifndef PACK_DATA
    O.abc   = float4( res0, 0.0f );
    O.x     = float4( res1, 0.0f );
    
    return O;
#else
    return pack( res0, res1 );
#endif
}

#undef fCoC_4
#undef fCoC_3
#undef fCoC_2
#undef fCoC_1
#undef fCoC4
#undef fCoC3
#undef fCoC2
#undef fCoC1

#undef fRealCoC_4
#undef fRealCoC_3
#undef fRealCoC_2
#undef fRealCoC_1
#undef fRealCoC3 
#undef fRealCoC2 
#undef fRealCoC1 
#undef fRealCoC0 

#undef beta_4 
#undef beta_3 
#undef beta_2 
#undef beta_1 
#undef beta3 
#undef beta2 
#undef beta1 
#undef beta0 

#ifndef PACK_DATA
ReduceO ReduceHorz( PS_INPUT input )
#else
uint4 ReduceHorz( PS_INPUT input ) : COLOR
#endif
{
    ReduceO O;
    float   fPosX       = (floor(input.Pos.x)*2.0f)+1.0f;
    int3    i3LoadPos   = int3( fPosX, input.Pos.y, 0 );
    
#ifndef PACK_DATA
    float3 abc   = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x     = txX_Texture.Load(   i3LoadPos ).xyz;

    float3 abc_1 = txABC_Texture.Load( i3LoadPos, int2( -1, 0 ) ).xyz;
    float3 x_1   = txX_Texture.Load(   i3LoadPos, int2( -1, 0 ) ).xyz;
    
    float3 abc1  = txABC_Texture.Load( i3LoadPos, int2( 1, 0 ) ).xyz;
    float3 x1    = txX_Texture.Load(   i3LoadPos, int2( 1, 0 ) ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc   = data.abc;
    float3 x     = data.x;

    ABC_X data_1 = unpack( txABC_X.Load( i3LoadPos, int2( -1,0 ) ) );
    float3 abc_1 = data_1.abc;
    float3 x_1   = data_1.x;

    ABC_X data1  = unpack( txABC_X.Load( i3LoadPos, int2( 1, 0 ) ) );
    float3 abc1  = data1.abc;
    float3 x1    = data1.x;
#endif

    float alpha = ( -abc.x / abc_1.y );    
    float gamma = ( fPosX + 2.0f ) > LastBufferSize ? 0.0f : ( -abc.z / abc1.y );
    
    float3 res0   = float3( alpha * abc_1.x,    
                            abc.y + alpha * abc_1.z + gamma * abc1.x,
                            gamma * abc1.z );
    float3 res1   = float3( x + alpha * x_1 + gamma * x1 );

#ifndef PACK_DATA
    O.abc   = float4( res0, 0.0f );
    O.x     = float4( res1, 0.0f );
    
    return O;
#else
    return pack( res0, res1 );
#endif
}

// run when there are only two columns left
// this does actually solve for two unknowns
float4 ReduceFinalHorz2( PS_INPUT input ) : COLOR
{
    int     iY = int( input.Pos.y );
    int3    i3LoadPos   = int3( 0, iY, 0 );
#ifndef PACK_DATA
    float3 abc   = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x     = txX_Texture.Load(   i3LoadPos ).xyz;

    float3 abc1  = txABC_Texture.Load( i3LoadPos, int2( 1,0 ) ).xyz;
    float3 x1    = txX_Texture.Load(   i3LoadPos, int2( 1,0 ) ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc   = data.abc;
    float3 x     = data.x;

    ABC_X data1  = unpack( txABC_X.Load( i3LoadPos, int2( 1,0 ) ) );
    float3 abc1  = data1.abc;
    float3 x1    = data1.x;
#endif
    
    float  det0 = abc.y * abc1.y - abc1.x * abc.z;
    float3 det1 = x * abc1.y - x1 * abc.z;
    float3 det2 = abc.y * x1 - abc1.x * x;
     
    float4 res = float4( ( input.Pos.x < 1.0f ? ( det1 / det0 ) : ( det2 / det0 ) ), 0.0f);
    
    return res;
}

// run when there are only three columns left
// this does actually solve for three unknowns
float4 ReduceFinalHorz3( PS_INPUT input ) : COLOR
{
    int3    i3LoadPos   = int3( 0, input.Pos.y, 0 );
                                
#ifndef PACK_DATA
    float3 abc0  = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x0    = txX_Texture.Load(   i3LoadPos ).xyz;

    float3 abc1  = txABC_Texture.Load( i3LoadPos, int2( 1,0 ) ).xyz;
    float3 x1    = txX_Texture.Load(   i3LoadPos, int2( 1,0 ) ).xyz;
    
    float3 abc2  = txABC_Texture.Load( i3LoadPos, int2( 2,0 ) ).xyz;
    float3 x2    = txX_Texture.Load(   i3LoadPos, int2( 2,0 ) ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc0   = data.abc;
    float3 x0     = data.x;

    ABC_X data1  = unpack( txABC_X.Load( i3LoadPos, int2(1,0) ) );
    float3 abc1  = data1.abc;
    float3 x1    = data1.x;

    ABC_X data2  = unpack( txABC_X.Load( i3LoadPos, int2(2,0) ) );
    float3 abc2  = data2.abc;
    float3 x2    = data2.x;
#endif

    float  det0  = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                           float3( abc0.z, abc1.y, abc2.x ),
                           float3( 0.0f,   abc1.z, abc2.y ) );
                           
    float3 det1, det2, det3;
    
    det1.x = det3x3( float3( x0.x, x1.x, x2.x ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );
                     
    det1.y = det3x3( float3( x0.y, x1.y, x2.y ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det1.z = det3x3( float3( x0.z, x1.z, x2.z ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det2.x = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( x0.x, x1.x, x2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det2.y = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( x0.y, x1.y, x2.y ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det2.z = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( x0.z, x1.z, x2.z ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det3.x = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( x0.x, x1.x, x2.x ) );

    det3.y = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( x0.y, x1.y, x2.y ) );

    det3.z = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( x0.z, x1.z, x2.z ) );

    float3 res1 = ( det1 / det0 );
    float3 res2 = ( det2 / det0 );
    float3 res3 = ( det3 / det0 );


    float4 res = float4( ( input.Pos.x < 1.0f ? ( det1 / det0 ) : 
                         ( input.Pos.x < 2.0f ? ( det2 / det0 ) : ( det3 / det0 ) ) ), 
                         0.0f );
    
    return res;
}

float4 SolveHorz( PS_INPUT input ) : COLOR
{
    int     i = int( input.Pos.x );
    int     y = int( input.Pos.y );
    int3    i3LoadPos;
    int3    i3LoadPos_1;
    int3    i3LoadPos1;
    
    [flatten]if( ( i & 1 ) != 0 ) // odd
    {
        i3LoadPos   = int3( i, y, 0 );
        i3LoadPos_1 = 
        i3LoadPos1  = int3(  i >> 1, y, 0 );
    }
    else // even
    {
        i3LoadPos_1 = int3( ( i - 2 ) >> 1, y, 0 );
        i3LoadPos1  = i3LoadPos_1 + int3( 1, 0, 0 );
        i3LoadPos   = int3( i, y, 0 );
    }
    
#ifndef PACK_DATA
    float3 abc = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x   = txX_Texture.Load( i3LoadPos ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc   = data.abc;
    float3 x     = data.x;
#endif
    float3 y_1 = txYn_Texture.Load( i3LoadPos_1 ).xyz;
    float3 y1  = txYn_Texture.Load( i3LoadPos1 ).xyz;
    
    float4 res = float4( ( i & 1 ) != 0 ? 
                        y1 :
                        ( x - abc.x * y_1 - abc.z * y1 ) / abc.y,
                        0.0f );
    
    return res;
}

float4 FinalSolveHorz4( PS_INPUT input ) : COLOR
{
    // first reconstruct the level 1 x, abc
    float   fPosX       = ( floor( input.Pos.x * 0.25f ) * 4.0f ) + 3.0f;
    int3    i3LoadPos   = int3( fPosX, input.Pos.y, 0 );

    float fCoC_5  = computeCoC(i3LoadPos, int2( -5, 0) );
    float fCoC_4  = computeCoC(i3LoadPos, int2( -4, 0) );
    float fCoC_3  = computeCoC(i3LoadPos, int2( -3, 0) );
    float fCoC_2  = computeCoC(i3LoadPos, int2( -2, 0) );
    float fCoC_1  = computeCoC(i3LoadPos, int2( -1, 0 ) );
    float fCoC0   = computeCoC(i3LoadPos, int2(  0 ,0 ) );
    float fCoC1   = computeCoC(i3LoadPos, int2(  1, 0 ) );
    float fCoC2   = computeCoC(i3LoadPos, int2(  2, 0 ) );
    float fCoC3   = computeCoC(i3LoadPos, int2(  3, 0 ) );
    float fCoC4   = computeCoC(i3LoadPos, int2(  4, 0 ) );
    
    fCoC_5 = ( fPosX - 5.0f ) >= 0.0f ? fCoC_5 : 0.0f;
    fCoC_4 = ( fPosX - 4.0f ) >= 0.0f ? fCoC_4 : 0.0f;
    fCoC_3 = ( fPosX - 3.0f ) >= 0.0f ? fCoC_3 : 0.0f;
    fCoC4  = ( fPosX + 4.0f ) < g_vInputImageSize.x ? fCoC4 : 0.0f;
    fCoC3  = ( fPosX + 3.0f ) < g_vInputImageSize.x ? fCoC3 : 0.0f;
    fCoC2  = ( fPosX + 2.0f ) < g_vInputImageSize.x ? fCoC2 : 0.0f;
    fCoC1  = ( fPosX + 1.0f ) < g_vInputImageSize.x ? fCoC1 : 0.0f;

    float fRealCoC_5   = min( fCoC_5, fCoC_4 );  
    float fRealCoC_4   = min( fCoC_4, fCoC_3 );  
    float fRealCoC_3   = min( fCoC_3, fCoC_2 );  
    float fRealCoC_2   = min( fCoC_2, fCoC_1 );  
    float fRealCoC_1   = min( fCoC_1, fCoC0 );  
    float fRealCoC0    = min( fCoC0, fCoC1 );  
    float fRealCoC1    = min( fCoC1, fCoC2 );  
    float fRealCoC2    = min( fCoC2, fCoC3 );  
    float fRealCoC3    = min( fCoC3, fCoC4 );  

    float beta_5  = fRealCoC_5 * fRealCoC_5;
    float beta_4  = fRealCoC_4 * fRealCoC_4;
    float beta_3  = fRealCoC_3 * fRealCoC_3;
    float beta_2  = fRealCoC_2 * fRealCoC_2;
    float beta_1  = fRealCoC_1 * fRealCoC_1;
    float beta0   = fRealCoC0  * fRealCoC0;
    float beta1   = fRealCoC1  * fRealCoC1;
    float beta2   = fRealCoC2  * fRealCoC2;
    float beta3   = fRealCoC3  * fRealCoC3;
    
    float3 abc_4  = float3( -beta_5, 1.0f + beta_4 + beta_5, -beta_4 );
    float3 x_4    = txX_Texture.Load( i3LoadPos, int2( -4, 0 ) ).xyz;

    float3 abc_3  = float3( -beta_4, 1.0f + beta_3 + beta_4, -beta_3 );
    float3 x_3    = txX_Texture.Load( i3LoadPos, int2( -3, 0 ) ).xyz;

    float3 abc_2  = float3( -beta_3, 1.0f + beta_2 + beta_3, -beta_2 );
    float3 x_2    = txX_Texture.Load( i3LoadPos, int2( -2, 0 ) ).xyz;

    float3 abc_1  = float3( -beta_2, 1.0f + beta_1 + beta_2, -beta_1 );
    float3 x_1    = txX_Texture.Load( i3LoadPos, int2( -1, 0 ) ).xyz;
    
    float3 abc0   = float3( -beta_1, 1.0f + beta0 + beta_1, -beta0 );
    float3 x0     = txX_Texture.Load( i3LoadPos, int2(  0, 0 ) ).xyz;

    float3 abc1   = float3( -beta0, 1.0f + beta1 + beta0, -beta1 );
    float3 x1     = txX_Texture.Load( i3LoadPos, int2(  1, 0 ) ).xyz;

    float3 abc2   = float3( -beta1, 1.0f + beta2 + beta1, -beta2 );
    float3 x2     = txX_Texture.Load( i3LoadPos, int2(  2, 0 ) ).xyz;
    
    float3 abc3   = float3( -beta2, 1.0f + beta3 + beta2, -beta3 );
    float3 x3     = txX_Texture.Load( i3LoadPos, int2(  3, 0 ) ).xyz;

    float alpha_2 = -abc_3.x / abc_4.y;
    float gamma_2 = -abc_3.z / abc_2.y;

    float alpha_1 = -abc_2.x / abc_3.y;
    float gamma_1 = -abc_2.z / abc_1.y;
    
    float alpha0  = -abc0.x / abc_1.y;
    float gamma0  = -abc0.z / abc1.y;
    
    float alpha1  = -abc2.x / abc1.y;
    float gamma1  = -abc2.z / abc3.y;

    float3 l1_abc_pp  = float3( alpha_2 * abc_4.x,    
                                abc_3.y + alpha_2 * abc_4.z + gamma_2 * abc_2.x,
                                gamma_2 * abc_2.z );
    float3 l1_x_pp    = float3( x_3 + alpha_2 * x_4 + gamma_2 * x_2 );

    float3 l1_abc_p   = float3( alpha_1 * abc_3.x,    
                                abc_2.y + alpha_1 * abc_3.z + gamma_1 * abc_1.x,
                                gamma_1 * abc_1.z );
    float3 l1_x_p     = float3( x_2 + alpha_1 * x_3 + gamma_1 * x_1 );
    
    float3 l1_abc_c   = float3( alpha0 * abc_1.x,    
                                abc0.y + alpha0 * abc_1.z + gamma0 * abc1.x,
                                gamma0 * abc1.z );
    float3 l1_x_c     = float3( x0 + alpha0 * x_1 + gamma0 * x1 );

    float3 l1_abc_n   = float3( alpha1 * abc1.x,    
                             abc2.y + alpha1 * abc1.z + gamma1 * abc3.x,
                             gamma1 * abc3.z );
    float3 l1_x_n     = float3( x2 + alpha1 * x1 + gamma1 * x3 );

    // now solve level 1
    int3   i3l2_LoadPosC = int3( input.Pos.x * 0.25f, input.Pos.y, 0 );
    float3 l2_y0         = txYn_Texture.Load( i3l2_LoadPosC ).xyz;
    float3 l2_y1         = txYn_Texture.Load( i3l2_LoadPosC, int2(  1, 0 ) ).xyz;
    float3 l2_y_1        = txYn_Texture.Load( i3l2_LoadPosC, int2( -1, 0 ) ).xyz;
    float3 l1_y_c        = l2_y0;
    float3 l1_y_p        = ( l1_x_p  - l1_abc_p.x  * l2_y_1 - l1_abc_p.z  * l2_y0  ) / l1_abc_p.y;
    float3 l1_y_pp       = l2_y_1;
    float3 l1_y_n        = ( l1_x_n  - l1_abc_n.x * l2_y0   - l1_abc_n.z  * l2_y1  ) / l1_abc_n.y;
    
    // now level 0
    float3 fRes3      = l2_y0;
    float3 fRes2      = ( x_1 - abc_1.x * l1_y_p  - abc_1.z * l1_y_c ) / abc_1.y; // y_1
    float3 fRes1      = l1_y_p; // y_2
    float3 fRes0      = ( x_3 - abc_3.x * l1_y_pp - abc_3.z * l1_y_p ) / abc_3.y; // y_3
    
    float3 f3Res[4] = { fRes0, fRes1, fRes2, fRes3 };
    
    float4 f4Res = float4( f3Res[ uint( input.Pos.x ) & 3 ], 0.0f );
    
    return f4Res;
}

#define fCoC_4 f4CoC_.w
#define fCoC_3 f4CoC_.z
#define fCoC_2 f4CoC_.y
#define fCoC_1 f4CoC_.x
#define fCoC4 f4CoC.w
#define fCoC3 f4CoC.z
#define fCoC2 f4CoC.y
#define fCoC1 f4CoC.x

#define fRealCoC_4 f4RealCoC_.w
#define fRealCoC_3 f4RealCoC_.z
#define fRealCoC_2 f4RealCoC_.y
#define fRealCoC_1 f4RealCoC_.x
#define fRealCoC3 f4RealCoC.w
#define fRealCoC2 f4RealCoC.z
#define fRealCoC1 f4RealCoC.y
#define fRealCoC0 f4RealCoC.x

#define beta_4 f4Beta_.w
#define beta_3 f4Beta_.z
#define beta_2 f4Beta_.y
#define beta_1 f4Beta_.x
#define beta3 f4Beta.w
#define beta2 f4Beta.z
#define beta1 f4Beta.y
#define beta0 f4Beta.x

//#define PACK_DATA


// reduce by a factor of 4 getting rid of 4 unkowns in each step
#ifdef PACK_DATA
uint4 InitialReduceVert4( PS_INPUT input ) : COLOR
#else
ReduceO InitialReduceVert4( PS_INPUT input )
#endif
{
    ReduceO O;
    float   fPosY       = (floor(input.Pos.y)*4.0f)+3.0f;
    int3    i3LoadPos   = int3( input.Pos.x, fPosY, 0 );
	float4  f4CoC, f4CoC_;
	float4  f4RealCoC, f4RealCoC_;
	float4  f4Beta, f4Beta_;

    fCoC_4  = computeCoC(i3LoadPos, int2( 0, -4 ) );
    fCoC_3  = computeCoC(i3LoadPos, int2( 0, -3 ) );
    fCoC_2  = computeCoC(i3LoadPos, int2( 0, -2 ) );
    fCoC_1  = computeCoC(i3LoadPos, int2( 0, -1 ) );
    float fCoC0   = computeCoC(i3LoadPos, int2( 0, 0 ) );
    fCoC1   = computeCoC(i3LoadPos, int2( 0, 1 ) );
    fCoC2   = computeCoC(i3LoadPos, int2( 0, 2 ) );
    fCoC3   = computeCoC(i3LoadPos, int2( 0, 3 ) );
    fCoC4   = computeCoC(i3LoadPos, int2( 0, 4 ) );
    
    fCoC_4 = ( fPosY - 4.0f ) >= 0.0f ? fCoC_4 : 0.0f;
	f4CoC = ( fPosY.xxxx + float4( 1.0f, 2.0f, 3.0f, 4.0f ) ) < g_vInputImageSize.xxxx ? f4CoC : (0.0f).xxxx;

    fRealCoC_4   = min( fCoC_4, fCoC_3 );  
    fRealCoC_3   = min( fCoC_3, fCoC_2 );  
    fRealCoC_2   = min( fCoC_2, fCoC_1 );  
    fRealCoC_1   = min( fCoC_1, fCoC0 );  
    fRealCoC0    = min( fCoC0, fCoC1 );  
    fRealCoC1    = min( fCoC1, fCoC2 );  
    fRealCoC2    = min( fCoC2, fCoC3 );  
    fRealCoC3    = min( fCoC3, fCoC4 );  

	f4Beta_ = f4RealCoC_ * f4RealCoC_;
	f4Beta  = f4RealCoC  * f4RealCoC;

    float3 abc_3  = float3( -beta_4, 1.0f + beta_3 + beta_4, -beta_3 );
    float3 x_3    = txX_Texture.Load( i3LoadPos, int2( 0, -3 ) ).xyz;

    float3 abc_2  = float3( -beta_3, 1.0f + beta_2 + beta_3, -beta_2 );
    float3 x_2    = txX_Texture.Load( i3LoadPos, int2( 0, -2 ) ).xyz;

    float3 abc_1  = float3( -beta_2, 1.0f + beta_1 + beta_2, -beta_1 );
    float3 x_1    = txX_Texture.Load( i3LoadPos, int2( 0, -1 ) ).xyz;
    
    float3 abc0   = float3( -beta_1, 1.0f + beta0 + beta_1, -beta0 );
    float3 x0     = txX_Texture.Load( i3LoadPos, int2( 0, 0 ) ).xyz;

    float3 abc1   = float3( -beta0, 1.0f + beta1 + beta0, -beta1 );
    float3 x1     = txX_Texture.Load( i3LoadPos, int2( 0, 1 ) ).xyz;

    float3 abc2   = float3( -beta1, 1.0f + beta2 + beta1, -beta2 );
    float3 x2     = txX_Texture.Load( i3LoadPos, int2( 0, 2 ) ).xyz;
    
    float3 abc3   = float3( -beta2, 1.0f + beta3 + beta2, -beta3 );
    float3 x3     = txX_Texture.Load( i3LoadPos, int2( 0, 3 ) ).xyz;
    
    float alpha_1 = -abc_2.x / abc_3.y;
    float gamma_1 = -abc_2.z / abc_1.y;
    
    float alpha0  = -abc0.x / abc_1.y;
    float gamma0  = -abc0.z / abc1.y;
    
    float alpha1  = -abc2.x / abc1.y;
    float gamma1  = -abc2.z / abc3.y;

    float3 abc_p   = float3( alpha_1 * abc_3.x,    
                             abc_2.y + alpha_1 * abc_3.z + gamma_1 * abc_1.x,
                             gamma_1 * abc_1.z );
    float3 x_p     = float3( x_2 + alpha_1 * x_3 + gamma_1 * x_1 );
    
    float3 abc_c   = float3( alpha0 * abc_1.x,    
                             abc0.y + alpha0 * abc_1.z + gamma0 * abc1.x,
                             gamma0 * abc1.z );
    float3 x_c     = float3( x0 + alpha0 * x_1 + gamma0 * x1 );

    float3 abc_n   = float3( alpha1 * abc1.x,    
                             abc2.y + alpha1 * abc1.z + gamma1 * abc3.x,
                             gamma1 * abc3.z );
    float3 x_n     = float3( x2 + alpha1 * x1 + gamma1 * x3 );
    
    float  alpha   = -abc_c.x / abc_p.y;
    float  gamma   = -abc_c.z / abc_n.y;
    
    float3 res0   = float3( alpha * abc_p.x,    
                            abc_c.y + alpha * abc_p.z + gamma * abc_n.x,
                            gamma * abc_n.z );
    float3 res1   = float3( x_c + alpha * x_p + gamma * x_n );

#ifndef PACK_DATA
    O.abc   = float4( res0, 0.0f );
    O.x     = float4( res1, 0.0f );
    
    return O;
#else
    return pack( res0, res1 );
#endif
}

#undef fCoC_4
#undef fCoC_3
#undef fCoC_2
#undef fCoC_1
#undef fCoC4
#undef fCoC3
#undef fCoC2
#undef fCoC1

#undef fRealCoC_4
#undef fRealCoC_3
#undef fRealCoC_2
#undef fRealCoC_1
#undef fRealCoC3 
#undef fRealCoC2 
#undef fRealCoC1 
#undef fRealCoC0 

#undef beta_4 
#undef beta_3 
#undef beta_2 
#undef beta_1 
#undef beta3 
#undef beta2 
#undef beta1 
#undef beta0 

#ifndef PACK_DATA
ReduceO ReduceVert( PS_INPUT input )
#else
uint4 ReduceVert( PS_INPUT input ) : COLOR
#endif
{
    ReduceO O;
    float   fY          = (floor(input.Pos.y)*2.0f)+1.0f;
    int3    i3LoadPos   = int3( input.Pos.x, 
                                fY, 
                                0 );
#ifndef PACK_DATA
    float3 abc   = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x     = txX_Texture.Load(   i3LoadPos ).xyz;

    float3 abc_1 = txABC_Texture.Load( i3LoadPos, int2( 0,-1 ) ).xyz;
    float3 x_1   = txX_Texture.Load(   i3LoadPos, int2( 0,-1 ) ).xyz;
    
    float3 abc1  = txABC_Texture.Load( i3LoadPos, int2( 0,1 ) ).xyz;
    float3 x1    = txX_Texture.Load(   i3LoadPos, int2( 0,1 ) ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc   = data.abc;
    float3 x     = data.x;

    ABC_X data_1 = unpack( txABC_X.Load( i3LoadPos, int2( 0,-1 ) ) );
    float3 abc_1 = data_1.abc;
    float3 x_1   = data_1.x;

    ABC_X data1  = unpack( txABC_X.Load( i3LoadPos, int2( 0,1 ) ) );
    float3 abc1  = data1.abc;
    float3 x1    = data1.x;
#endif

    float alpha = ( -abc.x / abc_1.y );    
    float gamma = ( fY + 2.0f )  > LastBufferSize ? 0.0f : ( -abc.z / abc1.y );
    
    float3 res0   = float3( alpha * abc_1.x,    
                            abc.y + alpha * abc_1.z + gamma * abc1.x,
                            gamma * abc1.z );
    float3 res1   = float3( x + alpha * x_1 + gamma * x1 );

#ifndef PACK_DATA
    O.abc   = float4( res0, 0.0f );
    O.x        = float4( res1, 0.0f );
    
    return O;
#else
    return pack( res0, res1 );
#endif
}

// run when there are only two rows left
// this does actually solve for two unknowns already
float4 ReduceFinalVert2( PS_INPUT input ) : COLOR
{
    int3    i3LoadPos   = int3( input.Pos.x, 
                                0, 
                                0 );
#ifndef PACK_DATA
    float3 abc   = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x     = txX_Texture.Load(   i3LoadPos ).xyz;

    float3 abc1  = txABC_Texture.Load( i3LoadPos, int2( 0, 1 ) ).xyz;
    float3 x1    = txX_Texture.Load(   i3LoadPos, int2( 0, 1 ) ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc   = data.abc;
    float3 x     = data.x;

    ABC_X data1  = unpack( txABC_X.Load( i3LoadPos, int2( 0, 1 ) ) );
    float3 abc1  = data1.abc;
    float3 x1    = data1.x;
#endif
    
    float  det0 = abc.y * abc1.y - abc1.x * abc.z;
    float3 det1 = x * abc1.y - x1 * abc.z;
    float3 det2 = abc.y * x1 - abc1.x * x;
     
    float4 res = float4( input.Pos.y < 1.0f ? det1 / det0 : det2 / det0, 0.0f);
    
    return res;
}

// run when there are only three rows left
// this does actually solve for three unknowns
float4 ReduceFinalVert3( PS_INPUT input ) : COLOR
{
    int3    i3LoadPos   = int3( input.Pos.x, 
                                0, 
                                0 );
#ifndef PACK_DATA
    float3 abc0  = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 x0    = txX_Texture.Load(   i3LoadPos ).xyz;

    float3 abc1  = txABC_Texture.Load( i3LoadPos, int2( 0, 1 ) ).xyz;
    float3 x1    = txX_Texture.Load(   i3LoadPos, int2( 0, 1 ) ).xyz;
    
    float3 abc2  = txABC_Texture.Load( i3LoadPos, int2( 0, 2 ) ).xyz;
    float3 x2    = txX_Texture.Load(   i3LoadPos, int2( 0, 2 ) ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc0  = data.abc;
    float3 x0    = data.x;

    ABC_X data1  = unpack( txABC_X.Load( i3LoadPos, int2( 0, 1 ) ) );
    float3 abc1  = data1.abc;
    float3 x1    = data1.x;

    ABC_X data2  = unpack( txABC_X.Load( i3LoadPos, int2( 0, 2 ) ) );
    float3 abc2  = data2.abc;
    float3 x2    = data2.x;
#endif

    float  det0  = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                           float3( abc0.z, abc1.y, abc2.x ),
                           float3( 0.0f,   abc1.z, abc2.y ) );
                           
    float3 det1, det2, det3;
    
    det1.x = det3x3( float3( x0.x, x1.x, x2.x ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );
                     
    det1.y = det3x3( float3( x0.y, x1.y, x2.y ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det1.z = det3x3( float3( x0.z, x1.z, x2.z ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det2.x = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( x0.x, x1.x, x2.x ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det2.y = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( x0.y, x1.y, x2.y ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det2.z = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( x0.z, x1.z, x2.z ),
                     float3( 0.0f,   abc1.z, abc2.y ) );

    det3.x = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( x0.x, x1.x, x2.x ) );

    det3.y = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( x0.y, x1.y, x2.y ) );

    det3.z = det3x3( float3( abc0.y, abc1.x, 0.0f ),
                     float3( abc0.z, abc1.y, abc2.x ),
                     float3( x0.z, x1.z, x2.z ) );

    float3 res1 = ( det1 / det0 );
    float3 res2 = ( det2 / det0 );
    float3 res3 = ( det3 / det0 );


    float4 res = float4( ( input.Pos.y < 1.0f ? ( det1 / det0 ) : 
                         ( input.Pos.y < 2.0f ? ( det2 / det0 ) : ( det3 / det0 ) ) ), 
                         0.0f );
    
    return res;
}

float4 SolveVert( PS_INPUT input ) : COLOR
{
    int     i  = int( input.Pos.y );
    int     x  = int( input.Pos.x );
    int3    i3LoadPos;
    int3    i3LoadPos_1;
    int3    i3LoadPos1;
    
    [flatten]if( ( i & 1 ) != 0 ) // odd
    {
        i3LoadPos   = int3( x, i, 0 );
        i3LoadPos_1 = 
        i3LoadPos1  = int3(  x, i >> 1, 0 );
    }
    else // even
    {
        i3LoadPos_1 = int3( x, ( i - 2 ) >> 1, 0 );
        i3LoadPos1  = i3LoadPos_1 + int3( 0, 1, 0 );
        i3LoadPos   = int3( x, i, 0 );
    }
    
#ifndef PACK_DATA
    float3 abc = txABC_Texture.Load( i3LoadPos ).xyz;
    float3 X   = txX_Texture.Load( i3LoadPos ).xyz;
#else
    ABC_X data   = unpack( txABC_X.Load( i3LoadPos ) );
    float3 abc   = data.abc;
    float3 X     = data.x;
#endif
    float3 y_1 = txYn_Texture.Load( i3LoadPos_1 ).xyz;
    float3 y1  = txYn_Texture.Load( i3LoadPos1 ).xyz;
    
    float4 res = float4( ( i & 1 ) != 0 ? 
                         y1 :
                        ( X - abc.x * y_1 - abc.z * y1 ) / abc.y,
                        0.0f );
    
    return res;
}

float4 FinalSolveVert4( PS_INPUT input ) : COLOR
{
    // first reconstruct the level 1 x, abc
    float   fPosY       = ( floor( input.Pos.y * 0.25f ) * 4.0f ) + 3.0f;
    int3    i3LoadPos   = int3( input.Pos.x, fPosY, 0 );

    float fCoC_5  = computeCoC(i3LoadPos, int2( 0, -5) );
    float fCoC_4  = computeCoC(i3LoadPos, int2( 0, -4) );
    float fCoC_3  = computeCoC(i3LoadPos, int2( 0, -3) );
    float fCoC_2  = computeCoC(i3LoadPos, int2( 0, -2) );
    float fCoC_1  = computeCoC(i3LoadPos, int2( 0, -1 ) );
    float fCoC0   = computeCoC(i3LoadPos, int2( 0,  0 ) );
    float fCoC1   = computeCoC(i3LoadPos, int2( 0,  1 ) );
    float fCoC2   = computeCoC(i3LoadPos, int2( 0,  2 ) );
    float fCoC3   = computeCoC(i3LoadPos, int2( 0,  3 ) );
    float fCoC4   = computeCoC(i3LoadPos, int2( 0,  4 ) );
    
    fCoC_5 = ( fPosY - 5.0f ) >= 0.0f ? fCoC_5 : 0.0f;
    fCoC_4 = ( fPosY - 4.0f ) >= 0.0f ? fCoC_4 : 0.0f;
    fCoC_3 = ( fPosY - 3.0f ) >= 0.0f ? fCoC_3 : 0.0f;
    fCoC4  = ( fPosY + 4.0f ) < g_vInputImageSize.y ? fCoC4 : 0.0f;
    fCoC3  = ( fPosY + 3.0f ) < g_vInputImageSize.y ? fCoC3 : 0.0f;
    fCoC2  = ( fPosY + 2.0f ) < g_vInputImageSize.y ? fCoC2 : 0.0f;
    fCoC1  = ( fPosY + 1.0f ) < g_vInputImageSize.y ? fCoC1 : 0.0f;

    float fRealCoC_5   = min( fCoC_5, fCoC_4 );  
    float fRealCoC_4   = min( fCoC_4, fCoC_3 );  
    float fRealCoC_3   = min( fCoC_3, fCoC_2 );  
    float fRealCoC_2   = min( fCoC_2, fCoC_1 );  
    float fRealCoC_1   = min( fCoC_1, fCoC0 );  
    float fRealCoC0    = min( fCoC0, fCoC1 );  
    float fRealCoC1    = min( fCoC1, fCoC2 );  
    float fRealCoC2    = min( fCoC2, fCoC3 );  
    float fRealCoC3    = min( fCoC3, fCoC4 );  

    float beta_5  = fRealCoC_5 * fRealCoC_5;
    float beta_4  = fRealCoC_4 * fRealCoC_4;
    float beta_3  = fRealCoC_3 * fRealCoC_3;
    float beta_2  = fRealCoC_2 * fRealCoC_2;
    float beta_1  = fRealCoC_1 * fRealCoC_1;
    float beta0   = fRealCoC0  * fRealCoC0;
    float beta1   = fRealCoC1  * fRealCoC1;
    float beta2   = fRealCoC2  * fRealCoC2;
    float beta3   = fRealCoC3  * fRealCoC3;
    
    float3 abc_4  = float3( -beta_5, 1.0f + beta_4 + beta_5, -beta_4 );
    float3 x_4    = txX_Texture.Load( i3LoadPos, int2( 0, -4 ) ).xyz;

    float3 abc_3  = float3( -beta_4, 1.0f + beta_3 + beta_4, -beta_3 );
    float3 x_3    = txX_Texture.Load( i3LoadPos, int2( 0, -3 ) ).xyz;

    float3 abc_2  = float3( -beta_3, 1.0f + beta_2 + beta_3, -beta_2 );
    float3 x_2    = txX_Texture.Load( i3LoadPos, int2( 0, -2 ) ).xyz;

    float3 abc_1  = float3( -beta_2, 1.0f + beta_1 + beta_2, -beta_1 );
    float3 x_1    = txX_Texture.Load( i3LoadPos, int2( 0, -1 ) ).xyz;
    
    float3 abc0   = float3( -beta_1, 1.0f + beta0 + beta_1, -beta0 );
    float3 x0     = txX_Texture.Load( i3LoadPos, int2( 0,  0 ) ).xyz;

    float3 abc1   = float3( -beta0, 1.0f + beta1 + beta0, -beta1 );
    float3 x1     = txX_Texture.Load( i3LoadPos, int2( 0,  1 ) ).xyz;

    float3 abc2   = float3( -beta1, 1.0f + beta2 + beta1, -beta2 );
    float3 x2     = txX_Texture.Load( i3LoadPos, int2( 0,  2 ) ).xyz;
    
    float3 abc3   = float3( -beta2, 1.0f + beta3 + beta2, -beta3 );
    float3 x3     = txX_Texture.Load( i3LoadPos, int2( 0,  3 ) ).xyz;

    float alpha_2 = -abc_3.x / abc_4.y;
    float gamma_2 = -abc_3.z / abc_2.y;

    float alpha_1 = -abc_2.x / abc_3.y;
    float gamma_1 = -abc_2.z / abc_1.y;
    
    float alpha0  = -abc0.x / abc_1.y;
    float gamma0  = -abc0.z / abc1.y;
    
    float alpha1  = -abc2.x / abc1.y;
    float gamma1  = -abc2.z / abc3.y;

    float3 l1_abc_pp  = float3( alpha_2 * abc_4.x,    
                                 abc_3.y + alpha_2 * abc_4.z + gamma_2 * abc_2.x,
                                 gamma_2 * abc_2.z );
    float3 l1_x_pp    = float3( x_3 + alpha_2 * x_4 + gamma_2 * x_2 );

    float3 l1_abc_p   = float3( alpha_1 * abc_3.x,    
                                abc_2.y + alpha_1 * abc_3.z + gamma_1 * abc_1.x,
                                gamma_1 * abc_1.z );
    float3 l1_x_p     = float3( x_2 + alpha_1 * x_3 + gamma_1 * x_1 );
    
    float3 l1_abc_c   = float3( alpha0 * abc_1.x,    
                                abc0.y + alpha0 * abc_1.z + gamma0 * abc1.x,
                                gamma0 * abc1.z );
    float3 l1_x_c     = float3( x0 + alpha0 * x_1 + gamma0 * x1 );

    float3 l1_abc_n   = float3( alpha1 * abc1.x,    
                             abc2.y + alpha1 * abc1.z + gamma1 * abc3.x,
                             gamma1 * abc3.z );
    float3 l1_x_n     = float3( x2 + alpha1 * x1 + gamma1 * x3 );

    // now solve level 1
    int3   i3l2_LoadPosC = int3( input.Pos.x, input.Pos.y * 0.25f, 0 );
    float3 l2_y0         = txYn_Texture.Load( i3l2_LoadPosC ).xyz;
    float3 l2_y1         = txYn_Texture.Load( i3l2_LoadPosC, int2( 0,  1 ) ).xyz;
    float3 l2_y_1        = txYn_Texture.Load( i3l2_LoadPosC, int2( 0, -1 ) ).xyz;
    float3 l1_y_c        = l2_y0;
    float3 l1_y_p        = ( l1_x_p  - l1_abc_p.x  * l2_y_1 - l1_abc_p.z  * l2_y0  ) / l1_abc_p.y;
    float3 l1_y_pp       = l2_y_1;
    float3 l1_y_n        = ( l1_x_n  - l1_abc_n.x * l2_y0   - l1_abc_n.z  * l2_y1  ) / l1_abc_n.y;
    
    // now level 0
    float3 fRes3      = l2_y0;
    float3 fRes2      = ( x_1 - abc_1.x * l1_y_p  - abc_1.z * l1_y_c ) / abc_1.y; // y_1
    float3 fRes1      = l1_y_p; // y_2
    float3 fRes0      = ( x_3 - abc_3.x * l1_y_pp - abc_3.z * l1_y_p ) / abc_3.y; // y_3
    
    float3 f3Res[4] = { fRes0, fRes1, fRes2, fRes3 };
    
    float4 f4Res = float4( f3Res[ uint( input.Pos.y ) & 3 ], 0.0f );
    
    return f4Res;
}

VS_OUTPUT VSPassThrough( VS_INPUT input )
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    
    output.Pos = float4(input.Pos.xyz, 1.0);
    output.Tex = input.Tex;
    
    return output;
}

technique11 DOF_Diffusion_sm50
{
	/*Vertical Phase*/
	pass DOF_InitialReduceVert4
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 InitialReduceVert4()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_ReduceVert
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 ReduceVert()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}	
	pass DOF_ReduceFinalVert2
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 ReduceFinalVert2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_ReduceFinalVert3
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 ReduceFinalVert3()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_SolveVert
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 SolveVert()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_FinalSolveVert4
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 FinalSolveVert4()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	/*Horizontal Phase*/
	pass DOF_InitialReduceHorz4
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 InitialReduceHorz4()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_ReduceHorz
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 ReduceHorz()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_ReduceFinalHorz2
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 ReduceFinalHorz2()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_ReduceFinalHorz3
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 ReduceFinalHorz3()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_SolveHorz
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 SolveHorz()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
	pass DOF_FinalSolveHorz4
	{
		VertexShader = compile VERTEXSHADER VSPassThrough();
		PixelShader  = compile ps_5_0 FinalSolveHorz4()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}

}

#else
technique dummy{ pass dummy	{} }
#endif //DOF_DIFFUSION

