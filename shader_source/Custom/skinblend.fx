//#pragma strip off

#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal
	#define PRAGMA_DCL
#endif
#include "../common.fxh"

#if __PS3 || RSG_ORBIS || RSG_PC || RSG_DURANGO
#define X360_UNROLL
#else
#define X360_UNROLL [unroll]
#endif

#define MAX_PALETTE_ENTRIES (64) // make sure these match the palette texture and the defines in MeshBlendManager.h
#define MAX_PALETTE_COLORS (4)

// =============================================================================================== //
// VARIABLES
// =============================================================================================== //

BeginConstantBufferPagedDX10(im_cbuffer, b5)

float4 texelSize : TexelSize;
float4 paletteColor0 : PaletteColor0;
float4 paletteColor1 : PaletteColor1;
float4 paletteColor2 : PaletteColor2;
float4 paletteColor3 : PaletteColor3;
float paletteSelector : PaletteSelector;
float paletteSelector2 : PaletteSelector2;
float compressLod : CompressLod;
float paletteIndex : PaletteIndex;
float unsharpAmount : UnsharpAmount = 0.3f;

EndConstantBufferDX10(im_cbuffer)

// =============================================================================================== //
// SAMPLERS
// =============================================================================================== //

#if __PS3
// we don't want rage_diffuse_sampler.fxh to be included, we have our own DiffuseSampler below
#define RAGE_DIFFUSE_SAMPLER_H

BeginSampler(sampler2D,diffuseTexture,DiffuseSampler,DiffuseTex)
	string UIName="Diffuse Texture";
#if DIFFUSE_UV1
	string UIHint="uv1";
#endif
	string TCPTemplate="Diffuse";
	string TextureType="Diffuse";
ContinueSampler(sampler2D,diffuseTexture,DiffuseSampler,DiffuseTex)
	AddressU  = WRAP;
	AddressV  = WRAP;
	AddressW  = WRAP;
	MIPFILTER = MIPLINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
EndSampler;
#endif
#include "../../../rage/base/src/shaderlib/rage_diffuse_sampler.fxh"

/*#include "../Megashader/megashader.fxh"*/

#define ENABLE_LAZYHALF __PS3
#define ENABLE_TEX2DOFFSET 0
#include "../Util/shader_utils.fxh"

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler2D,LinearTexture,LinearSampler,LinearTexture)
ContinueSampler(sampler2D,LinearTexture,LinearSampler,LinearTexture)
   MinFilter = LINEAR;
   MagFilter = LINEAR;
   MipFilter = NONE;
   AddressU  = WRAP;
   AddressV  = WRAP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler2D,PointTexture,PointSampler,PointTexture)
ContinueSampler(sampler2D,PointTexture,PointSampler,PointTexture)
   MinFilter = POINT;
   MagFilter = POINT;
   MipFilter = POINT;
   AddressU  = CLAMP;
   AddressV  = CLAMP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

BeginSampler(sampler2D,PaletteTexture,PaletteSampler,PaletteTexture)
ContinueSampler(sampler2D,PaletteTexture,PaletteSampler,PaletteTexture)
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = MIPLINEAR;
	AddressU  = WRAP;        
	AddressV  = WRAP;
EndSampler;

// ----------------------------------------------------------------------------------------------- //

// =============================================================================================== //
// DATA STRUCTURES
// =============================================================================================== //

//We need to ensure the input matches the vertex declaration correctly so we
//need a different input structure. This breaks DX11 if its wrong, other platforms
//manage to fix it up itself.
struct rageVertexBlitIn {
	float3 pos			: POSITION;
	float3 normal		: NORMAL;
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
};

struct rageVertexBlit {
	// This is used for the simple vertex and pixel shader routines
	DECLARE_POSITION(pos)
	float4 diffuse		: COLOR0;
	float2 texCoord0	: TEXCOORD0;
};

// ----------------------------------------------------------------------------------------------- //

// =============================================================================================== //
// UTILITY / HELPER FUNCTIONS
// =============================================================================================== //

// ----------------------------------------------------------------------------------------------- //

float4 PalettizeColor(float4 IN_diffuseColor, bool rgb)
{
	float4 OUT = IN_diffuseColor;

	const float texAlpha = floor(IN_diffuseColor.a * 255.01f);

	const float alphaMin	= 32;
	const float alphaMax	= 160;

	if ((texAlpha >= (alphaMin)) && (texAlpha <= (alphaMax)))
	{
        float texAlpha0 = (texAlpha-alphaMin) / (alphaMax-alphaMin);

        float4 newDiffuse;
        if (rgb)
        {
            if (texAlpha0 < 0.25f)
                newDiffuse = paletteColor0;
            else if (texAlpha0 < 0.5f)
                newDiffuse = paletteColor1;
            else if (texAlpha0 < 0.75)
                newDiffuse = paletteColor2;
            else
                newDiffuse = paletteColor3;
        }
        else
        {
#if __SHADERMODEL >= 40
            const float paletteRow = paletteSelector;	// this must be converted in higher CustomShaderFX code
            newDiffuse = tex2D(PaletteSampler, float2(texAlpha0, paletteRow));
#endif

#if __SHADERMODEL < 40
            const float paletteRow = paletteSelector;	// this must be converted in higher CustomShaderFX code
            newDiffuse = tex2D(PaletteSampler, float2(texAlpha0, paletteRow));
#endif
        }

		const float alphablend = newDiffuse.a;
		OUT.rgb	= newDiffuse.rgb*alphablend + IN_diffuseColor.rgb*(1.0f-alphablend);
		OUT.a	= 1.0f;
	}

	return OUT;
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_ReorientedNormalBlend(rageVertexBlit IN) : COLOR
{
	float3 base = tex2D_NormalMap(DiffuseSampler, IN.texCoord0).xyz;
	float3 detail = tex2D_NormalMap(LinearSampler, IN.texCoord0).xyz;
	return half4(ReorientedNormalBlend(base, detail, IN.diffuse.a), 1.f);
}

// ----------------------------------------------------------------------------------------------- //

float4 ReinterpretCastUnsignedToSigned_16_16_16_16(float4 vUnsigned)
{
    // Return 4 signed 16-bit integers which are bitwise equivalent to the unsigned integer data.
    // This conversion is required for EDRAM writes, since the 64-bit render targets are all signed.
    // [2 vector ops]
    return vUnsigned - ((vUnsigned >= 32768.f.xxxx) ? 65536.f.xxxx : 0.f.xxxx); 
}

// ----------------------------------------------------------------------------------------------- //

#if __PS3
float2 EndianSwap(float2 col)
{
    float2 newCol;
    newCol.x = floor(col.x / 256.f) + floor(fmod(col.x, 256.f)) * 256.f;
    newCol.y = floor(col.y / 256.f) + floor(fmod(col.y, 256.f)) * 256.f;
    return newCol;
}
#endif // __PS3

// ----------------------------------------------------------------------------------------------- //

// takes a 4x4 block of texels and returns a dxt block to be output to a signed abgr16 target
// on ps3 this function alternates between returning colors and indices so needs to be output
// to a double width gr16 target
float4 CompressDxt1Block(in float3 taps[16], float uRemainder)
{
    bool g_bAnchorChoice = true; // false uses min/max luminance and true uses covarience

    // find min/max colors and encode to 565
    float3 minColor = taps[0];
    float3 maxColor = taps[0];
    for (int i = 1; i < 16; ++i)
    {
        minColor = min(minColor, taps[i]);
        maxColor = max(maxColor, taps[i]);
    }

    float3 vCenter = 0.5f * (minColor + maxColor);
    
    // Anchors are endpoints of one of the two diagonals of the bounding box.  Which 
    // diagonal is determined by the sign of the covariances.
    // The actual covariance uses the mean, but we substitute the center.  
    float fCovarianceRG = 0;
    float fCovarianceBG = 0;
    if( g_bAnchorChoice )
    {
        for( int i = 0; i < 16; ++i )
        {
            float3 vDiffCenter = taps[i] - vCenter;

            fCovarianceRG += vDiffCenter.x * vDiffCenter.y;
            fCovarianceBG += vDiffCenter.z * vDiffCenter.y;
        }
    }

    float3 vAnchor[2];
    vAnchor[0].x = ( fCovarianceRG >= 0.0f ) ? maxColor.x : minColor.x;
    vAnchor[0].y = maxColor.y;
    vAnchor[0].z = ( fCovarianceBG >= 0.0f ) ? maxColor.z : minColor.z;
    vAnchor[1] = ( minColor + maxColor ) - vAnchor[0];
    
    float3 vRGBIncr = float3( 31.0f, 63.0f, 31.0f );
    float3 vRGBShift = float3( 32.0f, 64.0f, 32.0f );
    float3 vAnchor565[2];
    float fPackedAnchor565[2];
    vAnchor565[0] = vAnchor[0];
    vAnchor565[0] *= vRGBIncr;
    vAnchor565[0] = round( vAnchor565[0] );
    fPackedAnchor565[0] = vAnchor565[0].b + vRGBShift.b * ( vAnchor565[0].g + vRGBShift.g * vAnchor565[0].r );
    vAnchor565[1] = vAnchor[1];
    vAnchor565[1] *= vRGBIncr;
    vAnchor565[1] = round( vAnchor565[1] );
    fPackedAnchor565[1] = vAnchor565[1].b + vRGBShift.b * ( vAnchor565[1].g + vRGBShift.g * vAnchor565[1].r );
    
    // Handle case where anchors are mis-ordered.  This can only happen when we 
    // allow multiple anchor choices
    if( g_bAnchorChoice )
    {
        if( fPackedAnchor565[0] < fPackedAnchor565[1] )
        {
            float fTemp = fPackedAnchor565[0];
            fPackedAnchor565[0] = fPackedAnchor565[1];
            fPackedAnchor565[1] = fTemp;
            
            float3 vTemp = vAnchor[0];
            vAnchor[0] = vAnchor[1];
            vAnchor[1] = vTemp;
        }
    }

#if __PS3
    if (uRemainder < 0.5f)
    {
        // endian swap
        float4 ret = float4(EndianSwap(float2(fPackedAnchor565[0], fPackedAnchor565[1])), 0.f, 0.f);
        ret /= 65535.f;
        return ret;
    }
#endif
     
    // Pack 32 2-bit palette indices into 2 16-bit unsigned integers
    float fPackedIndices[2];
    if( fPackedAnchor565[0] == fPackedAnchor565[1] )
    {
        // Handle case where anchors are equal
        fPackedIndices[0] = fPackedIndices[1] = 0.0f; 
    }
    else
    {
        // For each input color, find the closest representable step along the line joining the anchors.
        float3 vDiag = vAnchor[1] - vAnchor[0];
        float fStepInc = 3.0f / dot( vDiag, vDiag );
        float fPaletteIndices[16];
        fPaletteIndices[ 0] = round( dot( taps[ 0] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 1] = round( dot( taps[ 1] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 2] = round( dot( taps[ 2] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 3] = round( dot( taps[ 3] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 4] = round( dot( taps[ 4] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 5] = round( dot( taps[ 5] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 6] = round( dot( taps[ 6] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 7] = round( dot( taps[ 7] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 8] = round( dot( taps[ 8] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[ 9] = round( dot( taps[ 9] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[10] = round( dot( taps[10] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[11] = round( dot( taps[11] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[12] = round( dot( taps[12] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[13] = round( dot( taps[13] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[14] = round( dot( taps[14] - vAnchor[0], vDiag ) * fStepInc );
        fPaletteIndices[15] = round( dot( taps[15] - vAnchor[0], vDiag ) * fStepInc );

#if !RSG_ORBIS && !RSG_DURANGO && __SHADERMODEL >= 40
#define REFINE_THRESHOLD (0.04f)
		// refinement step
		float3 diffColor = maxColor - minColor;
		if( diffColor.x * diffColor.y > REFINE_THRESHOLD || diffColor.y * diffColor.z > REFINE_THRESHOLD  || diffColor.z * diffColor.x > REFINE_THRESHOLD )
		{
	        float3 vBase[4];
			vBase[0] = vAnchor[0];
			vBase[1] = vAnchor[0]*2.0f/3.0f + vAnchor[1]*1.0f/3.0f;
			vBase[2] = vAnchor[0]*1.0f/3.0f + vAnchor[1]*2.0f/3.0f;
			vBase[3] = vAnchor[1];
	        float3 vDiff[4];
			vDiff[0] = vDiff[1] = vDiff[2] = vDiff[3] = float3(0.0f, 0.0f, 0.0f);
			float vBaseNum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
			// sum errors at each base color
		    for (int i = 0; i < 16; ++i)
			{
				int id = (int)fPaletteIndices[i];
				vBaseNum[id] += 1.0f;
				vDiff[id] += (taps[i] - vBase[id]);
			}

			float vBaseNum0 = vBaseNum[0] + vBaseNum[1] + vBaseNum[2];
			float vBaseNum1 = vBaseNum[3] + vBaseNum[2] + vBaseNum[1];
			// adjust anchors by (magically) weighted mean errors
			vAnchor[0] += vBaseNum0 != 0.0f ? (vDiff[0]*1.0f/2.0f + vDiff[1]*1.0f/3.0f + vDiff[2]*1.0f/6.0f)/vBaseNum0 : 0.0f;
			vAnchor[1] += vBaseNum1 != 0.0f ? (vDiff[3]*1.0f/2.0f + vDiff[2]*1.0f/3.0f + vDiff[1]*1.0f/6.0f)/vBaseNum1 : 0.0f;

			vAnchor[0] = saturate(vAnchor[0]);
			vAnchor[1] = saturate(vAnchor[1]);

			bool update = true;
			vDiag = vAnchor[1] - vAnchor[0];
			if( dot( vDiag, vDiag ) < 0.001f )
				update = false;
			if (update)
			{
				// re-pack anchors
				vAnchor565[0] = vAnchor[0];
				vAnchor565[0] *= vRGBIncr;
				vAnchor565[0] = round( vAnchor565[0] );
				fPackedAnchor565[0] = vAnchor565[0].b + vRGBShift.b * ( vAnchor565[0].g + vRGBShift.g * vAnchor565[0].r );
				vAnchor565[1] = vAnchor[1];
				vAnchor565[1] *= vRGBIncr;
				vAnchor565[1] = round( vAnchor565[1] );
				fPackedAnchor565[1] = vAnchor565[1].b + vRGBShift.b * ( vAnchor565[1].g + vRGBShift.g * vAnchor565[1].r );


				// re-calc indices
			    if( fPackedAnchor565[0] == fPackedAnchor565[1] )
			    {
			        // Handle case where anchors are equal
			        fPackedIndices[0] = fPackedIndices[1] = 0.0f; 
			    }
			    else
			    {
			        fStepInc = 3.0f / dot( vDiag, vDiag );
		
			        fPaletteIndices[ 0] = round( dot( taps[ 0] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 1] = round( dot( taps[ 1] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 2] = round( dot( taps[ 2] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 3] = round( dot( taps[ 3] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 4] = round( dot( taps[ 4] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 5] = round( dot( taps[ 5] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 6] = round( dot( taps[ 6] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 7] = round( dot( taps[ 7] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 8] = round( dot( taps[ 8] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[ 9] = round( dot( taps[ 9] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[10] = round( dot( taps[10] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[11] = round( dot( taps[11] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[12] = round( dot( taps[12] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[13] = round( dot( taps[13] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[14] = round( dot( taps[14] - vAnchor[0], vDiag ) * fStepInc );
			        fPaletteIndices[15] = round( dot( taps[15] - vAnchor[0], vDiag ) * fStepInc );
				}
			}
		}
#endif	// !RSG_ORBIS && !RSG_DURANGO


        // Remap palette indices according to DXT1 convention
        fPaletteIndices[ 0] = ( fPaletteIndices[ 0] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 0] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 0] + 1 );
        fPaletteIndices[ 1] = ( fPaletteIndices[ 1] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 1] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 1] + 1 );
        fPaletteIndices[ 2] = ( fPaletteIndices[ 2] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 2] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 2] + 1 );
        fPaletteIndices[ 3] = ( fPaletteIndices[ 3] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 3] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 3] + 1 );
        fPaletteIndices[ 4] = ( fPaletteIndices[ 4] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 4] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 4] + 1 );
        fPaletteIndices[ 5] = ( fPaletteIndices[ 5] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 5] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 5] + 1 );
        fPaletteIndices[ 6] = ( fPaletteIndices[ 6] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 6] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 6] + 1 );
        fPaletteIndices[ 7] = ( fPaletteIndices[ 7] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 7] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 7] + 1 );
        fPaletteIndices[ 8] = ( fPaletteIndices[ 8] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 8] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 8] + 1 );
        fPaletteIndices[ 9] = ( fPaletteIndices[ 9] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 9] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 9] + 1 );
        fPaletteIndices[10] = ( fPaletteIndices[10] == 0.0f ) ? 0.0f : ( fPaletteIndices[10] == 3.0f ) ? 1.0f : ( fPaletteIndices[10] + 1 );
        fPaletteIndices[11] = ( fPaletteIndices[11] == 0.0f ) ? 0.0f : ( fPaletteIndices[11] == 3.0f ) ? 1.0f : ( fPaletteIndices[11] + 1 );
        fPaletteIndices[12] = ( fPaletteIndices[12] == 0.0f ) ? 0.0f : ( fPaletteIndices[12] == 3.0f ) ? 1.0f : ( fPaletteIndices[12] + 1 );
        fPaletteIndices[13] = ( fPaletteIndices[13] == 0.0f ) ? 0.0f : ( fPaletteIndices[13] == 3.0f ) ? 1.0f : ( fPaletteIndices[13] + 1 );
        fPaletteIndices[14] = ( fPaletteIndices[14] == 0.0f ) ? 0.0f : ( fPaletteIndices[14] == 3.0f ) ? 1.0f : ( fPaletteIndices[14] + 1 );
        fPaletteIndices[15] = ( fPaletteIndices[15] == 0.0f ) ? 0.0f : ( fPaletteIndices[15] == 3.0f ) ? 1.0f : ( fPaletteIndices[15] + 1 );

        fPackedIndices[0] =  fPaletteIndices[ 0] + 
                    4.0f * ( fPaletteIndices[ 1] + 
                    4.0f * ( fPaletteIndices[ 2] + 
                    4.0f * ( fPaletteIndices[ 3] + 
                    4.0f * ( fPaletteIndices[ 4] + 
                    4.0f * ( fPaletteIndices[ 5] + 
                    4.0f * ( fPaletteIndices[ 6] + 
                    4.0f * ( fPaletteIndices[ 7] ))))))); 
        fPackedIndices[1] =  fPaletteIndices[ 8] + 
                    4.0f * ( fPaletteIndices[ 9] + 
                    4.0f * ( fPaletteIndices[10] + 
                    4.0f * ( fPaletteIndices[11] + 
                    4.0f * ( fPaletteIndices[12] + 
                    4.0f * ( fPaletteIndices[13] + 
                    4.0f * ( fPaletteIndices[14] + 
                    4.0f * ( fPaletteIndices[15] ))))))); 
    }
        
    float4 ret = float4(fPackedAnchor565[0], fPackedAnchor565[1], fPackedIndices[0], fPackedIndices[1]);
#if __PS3
    ret = float4(fPackedAnchor565[0], fPackedAnchor565[1], EndianSwap(ret.ba));
    ret /= 65535.f;
    return ret;
#elif __XENON
    return ReinterpretCastUnsignedToSigned_16_16_16_16(ret);
#else
    ret /= 65535.f;
    return ret;
#endif
}

// =============================================================================================== //
// TECHNIQUE FUNCTIONS
// =============================================================================================== //

rageVertexBlit VS_GTABlit(rageVertexBlitIn IN)
{
rageVertexBlit OUT;

	OUT.pos			= mul(float4(IN.pos.xyz, 1.0f), gWorldViewProj);
	OUT.diffuse		= IN.diffuse;
	OUT.texCoord0	= IN.texCoord0;

	return(OUT);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_SkinToneBlend(rageVertexBlit IN) : COLOR
{
	float4 clothSample = tex2D(DiffuseSampler, IN.texCoord0);
	float4 skinMaskSample = tex2D(LinearSampler, IN.texCoord0);

	float4 finalColor = float4(1.f, 1.f, 1.f, 0.f);
	if (skinMaskSample.b < (208.f / 255.f))
    {
        finalColor = PalettizeColor(clothSample, false);
    }

	return half4(finalColor * IN.diffuse);
}

half4 PS_SkinToneBlendRgb(rageVertexBlit IN) : COLOR
{
	float4 clothSample = tex2D(DiffuseSampler, IN.texCoord0);
	float4 skinMaskSample = tex2D(LinearSampler, IN.texCoord0);

	float4 finalColor = float4(1.f, 1.f, 1.f, 0.f);
	if (skinMaskSample.b < (208.f / 255.f))
    {
        finalColor = PalettizeColor(clothSample, true);
    }

	return half4(finalColor * IN.diffuse);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_Overlay(rageVertexBlit IN) : COLOR
{
	float4 diffuse = tex2D(DiffuseSampler, IN.texCoord0);
	float4 target = tex2D(LinearSampler, IN.texCoord0);

	float4 finalColor = target;
	finalColor.r = (target.r < 0.5f) ? (2.f * target.r * diffuse.r) : (1.f - 2.f * (1.f - target.r) * (1.f - diffuse.r));
	finalColor.g = (target.g < 0.5f) ? (2.f * target.g * diffuse.g) : (1.f - 2.f * (1.f - target.g) * (1.f - diffuse.g));
	finalColor.b = (target.b < 0.5f) ? (2.f * target.b * diffuse.b) : (1.f - 2.f * (1.f - target.b) * (1.f - diffuse.b));

	// opacity
	finalColor.rgb = IN.diffuse.a * finalColor.rgb + (1.f - IN.diffuse.a) * target.rgb;

	return half4(finalColor);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_SpecBlend(rageVertexBlit IN) : COLOR
{
	float4 base = tex2D(DiffuseSampler, IN.texCoord0);
	float4 spec = tex2D(LinearSampler, IN.texCoord0);

	float4 finalColor = any(spec.rgb) ? spec : base;
	finalColor.b = base.b; // preserve cloth mask

	return half4(finalColor);
}

// ----------------------------------------------------------------------------------------------- //

half4 PS_PaletteBlend(rageVertexBlit IN) : COLOR
{
	float4 detail = tex2D(DiffuseSampler, IN.texCoord0);
	half3 newDiffuse = h4tex2D(PointSampler, float2(detail.g, paletteSelector)).rgb;

	if (paletteSelector2 != paletteSelector)
	{
		half3 secondDiffuse = h4tex2D(PointSampler, float2(detail.g, paletteSelector2)).rgb;
		newDiffuse = secondDiffuse * detail.r + newDiffuse * (1.f - detail.r);
	}

	return half4(newDiffuse, detail.a * IN.diffuse.a);
}

// ----------------------------------------------------------------------------------------------- //

// Single-pass photoshop-like Unsharp filter
half4 PS_Unsharp(rageVertexBlit IN) : COLOR
{
	float4 o = tex2D(DiffuseSampler, IN.texCoord0);
	float4 gb = 41.0f/273.0f * o +
				33/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2(-1,  0)*texelSize.xy) +
				33/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2( 1,  0)*texelSize.xy) +
				33/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2( 0, -1)*texelSize.xy) +
				33/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2( 0,  1)*texelSize.xy) +
				25/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2(-1, -1)*texelSize.xy) +
				25/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2( 1, -1)*texelSize.xy) +
				25/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2( 1,  1)*texelSize.xy) +
				25/273.0f * tex2D(DiffuseSampler, IN.texCoord0 + float2(-1,  1)*texelSize.xy);

	gb = o + (2 * unsharpAmount * (o - gb));

	return half4(gb);
}

// ----------------------------------------------------------------------------------------------- //

float4 PS_CompressDxt1(rageVertexBlit IN) : COLOR
{
#if __PS3
    float4 rtTexelSize = texelSize * float4(2.f, 4.f, 2.f, 4.f);
    float rem = frac(IN.pos.x * 0.5f);
    if (rem >= 0.5f)
    {
        IN.texCoord0.x -= rtTexelSize.x;
    }
    float2 texCoord = IN.texCoord0 - float2(texelSize.x * 0.5f, texelSize.y * 1.5f);
#elif __XENON
    float2 texCoord = IN.texCoord0 + texelSize.xy * 0.5f;
#else
    float2 texCoord = IN.texCoord0 - texelSize.xy * 1.5f;
#endif

    // sample the (4+1)x(4+1) texel block on the uncompressed texture
	// need extra row and column for dither
    float3 texels[25];

    float lod = compressLod;
    lod = 0;


    for (int i = 0; i < 5; ++i)
    {        
        for (int j = 0; j < 5; ++j)
        {     
			float2 uv = texCoord + float2((j-1), (i-1)) * texelSize.xy;
            texels[i * 5 + j] = tex2Dlod(PointSampler, float4(uv, 0, (int)lod)).rgb;
        }    
    }

	// Floyd-Steinberg dither
    for (int y = 0; y < 5; ++y)
    {        
        for (int x = 0; x < 5; ++x)
        {     
            float3 pixel = texels[y * 5 + x];
			float3 quantized;
			quantized.r = round(pixel.r * 31.0f)/31.0f;
			quantized.b = round(pixel.b * 31.0f)/31.0f;
			quantized.g = round(pixel.g * 63.0f)/63.0f;
			float3 error = pixel - quantized;
			if (x < 4)
			{
					texels[(y+0) * 5 + (x+1)] += error * 7.0f/16.0f;
			}
			if (y < 4)
			{
				if (x > 0)
					texels[(y+1) * 5 + (x-1)] += error * 3.0f/16.0f;

					texels[(y+1) * 5 + (x+0)] += error * 5.0f/16.0f;

				if (x < 4)
					texels[(y+1) * 5 + (x+1)] += error * 1.0f/16.0f;
			}
        }    
    }

	// get 4x4 dithered texel block
	float3 taps[16];

    for (int v = 0; v < 4; ++v)
    {        
        for (int u = 0; u < 4; ++u)
		{
			taps[v * 4 + u] = saturate(texels[(v+1) * 5 + (u+1)]);
		}
	}

#if __PS3
    if (rem < 0.5f)
    {
        return unpack_4ubyte(pack_2ushort(CompressDxt1Block(taps, rem).gr));
    }
    else
    {
        return unpack_4ubyte(pack_2ushort(CompressDxt1Block(taps, rem).ab));
    }
#else
    return CompressDxt1Block(taps, 0.f);
#endif
}

// ----------------------------------------------------------------------------------------------- //

float4 PS_CreatePalette(rageVertexBlit IN) : COLOR
{
    float4 col = float4(0.f, 0.f, 0.f, 0.f);

    float y = paletteIndex / MAX_PALETTE_ENTRIES;
    float ystep = 1.f / MAX_PALETTE_ENTRIES;

    if (IN.texCoord0.y >= y && IN.texCoord0.y < (y + ystep))
    {
        if (IN.texCoord0.x < 0.25f)
            col = paletteColor0;
        else if (IN.texCoord0.x < 0.5f)
            col = paletteColor1;
        else if (IN.texCoord0.x < 0.75)
            col = paletteColor2;
        else
            col = paletteColor3;
    }

    return col;
}

// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //

// =============================================================================================== //
//	TECHNIQUES
// =============================================================================================== //

// ----------------------------------------------------------------------------------------------- //

technique skinToneBlendPaletteTex
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_SkinToneBlend()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique skinToneBlendPaletteRgb
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_SkinToneBlendRgb()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psOverlay
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_Overlay()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psSpecBlend
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_SpecBlend()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psPaletteBlend
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_PaletteBlend()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psUnsharp
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader	 = compile PIXELSHADER PS_Unsharp() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psReorientedNormalBlend
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_ReorientedNormalBlend()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psCompressDxt1
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
#if RSG_ORBIS
		PixelShader  = compile PIXELSHADER PS_CompressDxt1()  PS4_TARGET(FMT_UNORM16_ABGR);
#else
		PixelShader  = compile PIXELSHADER PS_CompressDxt1()  CGC_FLAGS(CGC_DEFAULTFLAGS);
#endif
	}
}

// ----------------------------------------------------------------------------------------------- //

technique psCreatePalette
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_GTABlit();
		PixelShader  = compile PIXELSHADER PS_CreatePalette()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// ----------------------------------------------------------------------------------------------- //

