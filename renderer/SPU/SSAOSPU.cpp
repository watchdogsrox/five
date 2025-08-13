//
//	Custom PM version of BlitSPU processing job:
//
//	22/12/2008	-	Andrzej:	- initial;
//	26/03/2009	-	Andrzej:	- Lightgrid added;
//	30/03/2009	-	Andrzej:	- sharing code directly from BlitSPU.cpp;
//	18/05/2012	-	Andrzej:	- SSAO SPU revitalized;
//
//
//
#if __SPU

#define DEBUG_SPU_JOB_NAME			"spupmSSAOSPU"

#undef	__STRICT_ALIGNED			// new.h compile warnings
#define ATL_INMAP_H					// inmap.h compile warnings
#define VECTOR3_UNPACK1010102
#define MATRIX34_SET_DIAGONAL
#define VECTOR3_SETWASUNSIGNEDINT

#define SYS_DMA_VALIDATION			(1)	// 1 -	General checks on valid alignment, size and DMA source and  destination addresses for all DMA commands. 
#include "system/dma.h"
#include <cell/atomic.h>
#include <spu_printf.h>

#include "renderer\SpuPM\SpuPmMgr.h"
#include "renderer\SpuPM\SpuPmMgrSPU.h"

#include "math/intrinsics.h"
#include "math/vecmath.h"
#include "vector/vector3.h"
#include "vector/matrix34.h"

// SPU helper headers:
#include "basetypes.h"
#include "fwtl/StepAllocator.h"

#include "SSAOSPU.h"

//#define SPU_DEBUGF(X)				X
#define SPU_DEBUGF(x)
#define SPU_DEBUGF1(X)				X

#define VECTORIZED_SSA0				(1)
#define OPTIMIZED_SSAO_DMA			(1)

#define BLITIN_DMA_TAG				(8)
#define BLITOUT_DMA_TAG				(0)
#define BLITOUT_DMA_TAG2			(1)
CompileTimeAssert(BLITOUT_DMA_TAG ==0);
CompileTimeAssert(BLITOUT_DMA_TAG2==1);


#define BUF_SSAO_WIDTH				(640)	// must be divisible by 4	
#define BUF_SSAO_HEIGHT				(360)
#define SPUSSAO_FILTERWIDTH			(32)
#define SPUSSAO_NUMLINES			(1 + SPUSSAO_FILTERWIDTH*2)
#define BUF_SSAO_WIDTH2				(BUF_SSAO_WIDTH/2)
#define BUF_SSAO_HEIGHT2			(BUF_SSAO_HEIGHT/2)

////////////////////////////////////////////////////////////////////////////////////////////
//static u32 local_mini_depth[320*92]	ALIGNED(128) = {0};
//static u32 local_dest_tile[320*60]	ALIGNED(128) = {0};

#define local_mini_depth_SIZE			(BUF_SSAO_WIDTH*SPUSSAO_NUMLINES*sizeof(u32))
#define local_mini_depth_ALIGN			(128)
static u32 *local_mini_depth=0;
#define local_dest_tile_SIZE			(BUF_SSAO_WIDTH*sizeof(u8))
#define local_dest_tile_ALIGN			(128)
static u32 *local_dest_tile=0;

// we have to fit within 192KB:
CompileTimeAssert((local_mini_depth_SIZE+local_dest_tile_SIZE) < 192*1024);

CSPUPostProcessDriverInfo *infoPacket=0;


static SSAOSpuParams *g_inSsaoParams=0;

//
//
// helper functions:
//
inline float getDepth(int x, int y)
{
	u32 idepth=local_mini_depth[y*BUF_SSAO_WIDTH + x];

	u32 d0=((idepth&0x0000ff00)>>8);
	u32 d1=((idepth&0x00ff0000)>>16);
	u32 d2=((idepth&0xff000000)>>24);

	float depth=d0+(d1/256.0f)+(d2/(256.0f*256.0f));

	return depth;
}

inline float getDepth0(int x, int y)
{
	int x0=x;
	if (x<0) x0=0;
	if (x>(BUF_SSAO_WIDTH-1)) x0=(BUF_SSAO_WIDTH-1);

	return getDepth(x0,y+1);
}


__forceinline float getDepthSSAO(u32 *mini_depth, int x, int y)
{
#if VECTORIZED_SSA0
	float *mini_depth_f = (float*)mini_depth;
	return mini_depth_f[y*BUF_SSAO_WIDTH + x];
#else
	return getDepth(x,y);
#endif
}

// grabs 4 depths at time, but x4 must be obviously divisble by 4:
__forceinline vec_float4 getDepthSSAO4(u32* mini_depth, int x4, int y)
{
	float *mini_depth_f = (float*)mini_depth;
	vec_float4 d4 = *((vec_float4*)&mini_depth_f[y*BUF_SSAO_WIDTH + x4]);
	return d4;
}

//
//
// copies memory in 128 bytes chunks:
//
inline
void MemCpy128(u32 *dst, u32 *src, u32 byteSize)
{
	Assert128(dst);
	Assert128(src);
	Assert128(byteSize);

	vec_uint4 *dst128 = (vec_uint4*)dst;
	vec_uint4 *src128 = (vec_uint4*)src;

	const u32 size4 = byteSize / 16;

	for(u32 i=0; i<size4; i+=8)
	{
		vec_uint4 d0 = src128[i+0];
		vec_uint4 d1 = src128[i+1];
		vec_uint4 d2 = src128[i+2];
		vec_uint4 d3 = src128[i+3];
		vec_uint4 d4 = src128[i+4];
		vec_uint4 d5 = src128[i+5];
		vec_uint4 d6 = src128[i+6];
		vec_uint4 d7 = src128[i+7];

		dst128[i+0] = d0;
		dst128[i+1] = d1;
		dst128[i+2] = d2;
		dst128[i+3] = d3;
		dst128[i+4] = d4;
		dst128[i+5] = d5;
		dst128[i+6] = d6;
		dst128[i+7] = d7;
	}
}



#if VECTORIZED_SSA0
//
//
//
//
SPU_INLINE
vec_uint4 InternalProcessSSAO(
		u32 xx, s32 oy, vec_float4 vecStrength,
		vec_float4 vecSY, vec_float4 vecOY, vec_float4 vecPX, vec_float4 vecPY, vec_float4 vecPosYOff,
		vec_float4 vecWidth2, vec_float4 vecHeight2, u32 *mini_depth)
{
	vec_float4	vecXX0123 = spu_splats(float(xx));
	vecXX0123 = spu_add(vecXX0123, vecConst0_1_2_3);

	//			float depth =getDepth(xx,  oy);
	vec_float4 vecDepth	= getDepthSSAO4(mini_depth, xx,		oy);	// grab 4 depths at once
	vec_float4 vecDepth1= getDepthSSAO4(mini_depth, xx+4,	oy);	// grab next 4 depths

	//			float depthA=getDepth(xx+1,oy);
	vec_float4 vecDepthA= spu_shuffle(vecDepth, vecDepth1, shufBCDa);

	//			float depthB=getDepth(xx,  oy+1);
	vec_float4 vecDepthB= getDepthSSAO4(mini_depth, xx, oy+1);

	//			Vector3 pos =Vector3((xx  -BUF_SSAO_WIDTH2)*px, (sy  -BUF_SSAO_HEIGHT2)*py, 1.0f)*depth; 
vec_float4 posX, posY, posZ;
	posX = spu_sub(vecXX0123,vecWidth2);
	posX = spu_mul(posX,	vecPX);

	posY = spu_sub(vecSY,	vecHeight2);
	posY = spu_mul(posY,	vecPY);

	posX = spu_mul(posX,	vecDepth);
	posY = spu_mul(posY,	vecDepth);
	posZ = vecDepth;



	//			Vector3 posA=Vector3((xx+1-BUF_SSAO_WIDTH2)*px, (sy  -BUF_SSAO_HEIGHT2)*py, 1.0f)*depthA; 
vec_float4 posAX, posAY, posAZ;
	posAX = spu_add(vecXX0123,	vecConstOne);
	posAX = spu_sub(posAX,		vecWidth2);
	posAX = spu_mul(posAX,		vecPX);

	posAY = spu_sub(vecSY,		vecHeight2);
	posAY = spu_mul(posAY,		vecPY);

	posAX = spu_mul(posAX,		vecDepthA);
	posAY = spu_mul(posAY,		vecDepthA);
	posAZ = vecDepthA;


	//			Vector3 posB=Vector3((xx  -BUF_SSAO_WIDTH2)*px, (sy+1-BUF_SSAO_HEIGHT2)*py, 1.0f)*depthB;
vec_float4 posBX, posBY, posBZ;
	posBX = spu_sub(vecXX0123,vecWidth2);
	posBX = spu_mul(posBX,	vecPX);

	posBY = spu_add(vecSY,  vecConstOne);
	posBY = spu_sub(posBY,	vecHeight2);
	posBY = spu_mul(posBY,  vecPY);

	posBX = spu_mul(posBX,	vecDepthB);
	posBY = spu_mul(posBY,	vecDepthB);
	posBZ = vecDepthB;


	//			Vector3 norm;
	//			norm.Cross(posA-pos, posB-pos);
	vec_float4 vecCross1X = spu_sub(posAX, posX);
	vec_float4 vecCross1Y = spu_sub(posAY, posY);
	vec_float4 vecCross1Z = spu_sub(posAZ, posZ);
	vec_float4 vecCross2X = spu_sub(posBX, posX);
	vec_float4 vecCross2Y = spu_sub(posBY, posY);
	vec_float4 vecCross2Z = spu_sub(posBZ, posZ);

	// cross: C = V1 x V2
	vec_float4 vecCrossXa = spu_mul(vecCross1Y, vecCross2Z);
	vec_float4 vecCrossXb = spu_mul(vecCross1Z, vecCross2Y);
	vec_float4 vecCrossX  = spu_sub(vecCrossXa, vecCrossXb);	// Cx = V1y*V2z - V1z*V2y;

	vec_float4 vecCrossYa = spu_mul(vecCross1Z, vecCross2X);
	vec_float4 vecCrossYb = spu_mul(vecCross1X, vecCross2Z);
	vec_float4 vecCrossY  = spu_sub(vecCrossYa, vecCrossYb);	// Cy = V1z*V2x - V1x*V2z;

	vec_float4 vecCrossZa = spu_mul(vecCross1X, vecCross2Y);
	vec_float4 vecCrossZb = spu_mul(vecCross1Y, vecCross2X);
	vec_float4 vecCrossZ  = spu_sub(vecCrossZa, vecCrossZb);	// Cz = V1x*V2y - V1y*V2x;

	//			norm.Normalize();
	vec_float4 vecCrossLena		= spu_mul (vecCrossX, vecCrossX);
	vec_float4 vecCrossLenb		= spu_madd(vecCrossY, vecCrossY, vecCrossLena);
	vec_float4 vecCrossLenc		= spu_madd(vecCrossZ, vecCrossZ, vecCrossLenb);
	vec_float4 vecCrossInvLen	= spu_rsqrte(vecCrossLenc);

	vec_float4 vecNormX = spu_mul(vecCrossX, vecCrossInvLen);
	vec_float4 vecNormY = spu_mul(vecCrossY, vecCrossInvLen);
	vec_float4 vecNormZ = spu_mul(vecCrossZ, vecCrossInvLen);

	// todo: invert cross(a,b) = -cross(b,a)
	//			norm *= -1.0f;
	vecNormX = spu_nmsub(vecNormX, vecConstOne, vecConstZero);	// vecNormX = -vecNormX;
	vecNormY = spu_nmsub(vecNormY, vecConstOne, vecConstZero);	// vecNormY = -vecNormY;
	vecNormZ = spu_nmsub(vecNormZ, vecConstOne, vecConstZero);	// vecNormZ = -vecNormZ;


	//			Vector2 tex = Vector2(xx,oy);
	vec_float4 texX		= vecXX0123;
	vec_float4 texY		= vecOY;
	//			const float angle = tex.Dot(Vector2(9.0f, 29.814f));
	vec_float4 anglea	= spu_mul (texX, vecConst9);
	vec_float4 angle	= spu_madd(vecSY, vecConst29_814, anglea);

	//			float scx=rage::Sinf(angle);
	//			float scy=rage::Cosf(angle);
	vec_float4 scx, scy;
	sincosf4Gta(angle, scx, scy);

	/*
	//			float scale = rage::Min(2.0f+(14.0f*4.0f/depth), 16.0f);
	vec_float4 invDepth = spu_re(vecDepth);
	vec_float4 sel_maska= spu_madd(vecConst56, invDepth, vecConstTwo);
	vec_uint4  sel_mask	= spu_cmpgt(sel_maska, vecConst16);
	vec_float4 scale	= spu_sel(sel_maska, vecConst16, sel_mask);
	*/
	const vec_float4 scale	= {32.0f, 32.0f, 32.0f, 32.0f};

vec_float4 scale1_00 = scale;
	vec_float4 scale0_75 = spu_mul(scale, vecConst0_75);
	vec_float4 scale0_50 = spu_mul(scale, vecConstHalf);
	vec_float4 scale0_25 = spu_mul(scale, vecConst0_25);

	//			Vector2 tex0=tex+(Vector2( scy,-scx)*(1.00f*scale));
vec_float4 tex0X, tex0Y;
	tex0X = spu_madd( scy, scale1_00, texX);	// 0X = texX + scy*scale*1.0f;
	tex0Y = spu_nmsub(scx, scale1_00, texY);	// 0Y = texY - scx*scale*1.0f;

	//			Vector2 tex1=tex+(Vector2( scx, scy)*(0.75f*scale));
vec_float4 tex1X, tex1Y;
	tex1X = spu_madd(scx, scale0_75, texX);
	tex1Y = spu_madd(scy, scale0_75, texY);

	//			Vector2 tex2=tex+(Vector2(-scy, scx)*(0.50f*scale));
vec_float4 tex2X, tex2Y;
	tex2X = spu_nmsub(scy, scale0_50, texX);
	tex2Y = spu_madd( scx, scale0_50, texY);

	//			Vector2 tex3=tex+(Vector2(-scx,-scy)*(0.25f*scale));
vec_float4 tex3X, tex3Y;
	tex3X = spu_nmsub(scx, scale0_25, texX);
	tex3Y = spu_nmsub(scy, scale0_25, texY);


	//			float depth0=getDepth(u32(tex0.x),u32(tex0.y));
	vec_uint4 nTex0X = spu_convtu(tex0X, 0);
	vec_uint4 nTex0Y = spu_convtu(tex0Y, 0);
	float depth0a = getDepthSSAO(mini_depth, spu_extract(nTex0X,0), spu_extract(nTex0Y,0));
	float depth0b = getDepthSSAO(mini_depth, spu_extract(nTex0X,1), spu_extract(nTex0Y,1));
	float depth0c = getDepthSSAO(mini_depth, spu_extract(nTex0X,2), spu_extract(nTex0Y,2));
	float depth0d = getDepthSSAO(mini_depth, spu_extract(nTex0X,3), spu_extract(nTex0Y,3));

vec_float4 depth0=vecConstZero;
	depth0 = spu_insert(depth0a, depth0, 0);
	depth0 = spu_insert(depth0b, depth0, 1);
	depth0 = spu_insert(depth0c, depth0, 2);
	depth0 = spu_insert(depth0d, depth0, 3);


	//			float depth1=getDepth(u32(tex1.x),u32(tex1.y));
	vec_uint4 nTex1X = spu_convtu(tex1X, 0);
	vec_uint4 nTex1Y = spu_convtu(tex1Y, 0);
	float depth1a = getDepthSSAO(mini_depth, spu_extract(nTex1X,0), spu_extract(nTex1Y,0));
	float depth1b = getDepthSSAO(mini_depth, spu_extract(nTex1X,1), spu_extract(nTex1Y,1));
	float depth1c = getDepthSSAO(mini_depth, spu_extract(nTex1X,2), spu_extract(nTex1Y,2));
	float depth1d = getDepthSSAO(mini_depth, spu_extract(nTex1X,3), spu_extract(nTex1Y,3));

	vec_float4 depth1=vecConstZero;
	depth1 = spu_insert(depth1a, depth1, 0);
	depth1 = spu_insert(depth1b, depth1, 1);
	depth1 = spu_insert(depth1c, depth1, 2);
	depth1 = spu_insert(depth1d, depth1, 3);

	//			float depth2=getDepth(u32(tex2.x),u32(tex2.y));
	vec_uint4 nTex2X = spu_convtu(tex2X, 0);
	vec_uint4 nTex2Y = spu_convtu(tex2Y, 0);
	float depth2a = getDepthSSAO(mini_depth, spu_extract(nTex2X,0), spu_extract(nTex2Y,0));
	float depth2b = getDepthSSAO(mini_depth, spu_extract(nTex2X,1), spu_extract(nTex2Y,1));
	float depth2c = getDepthSSAO(mini_depth, spu_extract(nTex2X,2), spu_extract(nTex2Y,2));
	float depth2d = getDepthSSAO(mini_depth, spu_extract(nTex2X,3), spu_extract(nTex2Y,3));

	vec_float4 depth2=vecConstZero;
	depth2 = spu_insert(depth2a, depth2, 0);
	depth2 = spu_insert(depth2b, depth2, 1);
	depth2 = spu_insert(depth2c, depth2, 2);
	depth2 = spu_insert(depth2d, depth2, 3);

	//			float depth3=getDepth(u32(tex3.x),u32(tex3.y));
	vec_uint4 nTex3X = spu_convtu(tex3X, 0);
	vec_uint4 nTex3Y = spu_convtu(tex3Y, 0);
	float depth3a = getDepthSSAO(mini_depth, spu_extract(nTex3X,0), spu_extract(nTex3Y,0));
	float depth3b = getDepthSSAO(mini_depth, spu_extract(nTex3X,1), spu_extract(nTex3Y,1));
	float depth3c = getDepthSSAO(mini_depth, spu_extract(nTex3X,2), spu_extract(nTex3Y,2));
	float depth3d = getDepthSSAO(mini_depth, spu_extract(nTex3X,3), spu_extract(nTex3Y,3));

	vec_float4 depth3=vecConstZero;
	depth3 = spu_insert(depth3a, depth3, 0);
	depth3 = spu_insert(depth3b, depth3, 1);
	depth3 = spu_insert(depth3c, depth3, 2);
	depth3 = spu_insert(depth3d, depth3, 3);



	//			Vector3 pos0=Vector3((tex0.x-BUF_SSAO_WIDTH2)*px, (tex0.y-offy+starty-BUF_SSAO_HEIGHT2)*py, 1.0f)*depth0; 
	vec_float4 pos0X, pos0Y, pos0Z;
	vec_float4 pos0Xa	= spu_sub(tex0X, vecWidth2);
	vec_float4 pos0Xb	= spu_mul(pos0Xa, vecPX);

	vec_float4 pos0Ya	= spu_add(tex0Y,	vecPosYOff);
	vec_float4 pos0Yb	= spu_mul(pos0Ya,	vecPY);

	pos0X = spu_mul(pos0Xb, depth0);
	pos0Y = spu_mul(pos0Yb, depth0);
	pos0Z = depth0;

	//			Vector3 pos1=Vector3((tex1.x-BUF_SSAO_WIDTH2)*px, (tex1.y-offy+starty-BUF_SSAO_HEIGHT2)*py, 1.0f)*depth1; 
	vec_float4 pos1X, pos1Y, pos1Z;
	vec_float4 pos1Xa	= spu_sub(tex1X, vecWidth2);
	vec_float4 pos1Xb	= spu_mul(pos1Xa,vecPX);

	vec_float4 pos1Ya	= spu_add(tex1Y,	vecPosYOff);
	vec_float4 pos1Yb	= spu_mul(pos1Ya,	vecPY);

	pos1X	= spu_mul(pos1Xb, depth1);
	pos1Y	= spu_mul(pos1Yb, depth1);
	pos1Z	= depth1;

	//			Vector3 pos2=Vector3((tex2.x-BUF_SSAO_WIDTH2)*px, (tex2.y-offy+starty-BUF_SSAO_HEIGHT2)*py, 1.0f)*depth2; 
	vec_float4 pos2X, pos2Y, pos2Z;
	vec_float4 pos2Xa	= spu_sub(tex2X, vecWidth2);
	vec_float4 pos2Xb	= spu_mul(pos2Xa,vecPX);

	vec_float4 pos2Ya	= spu_add(tex2Y, vecPosYOff);
	vec_float4 pos2Yb	= spu_mul(pos2Ya,vecPY);

	pos2X = spu_mul(pos2Xb,	depth2);
	pos2Y = spu_mul(pos2Yb, depth2);
	pos2Z = depth2;


	//			Vector3 pos3=Vector3((tex3.x-BUF_SSAO_WIDTH2)*px, (tex3.y-offy+starty-BUF_SSAO_HEIGHT2)*py, 1.0f)*depth3; 
	vec_float4 pos3X, pos3Y, pos3Z;
	vec_float4 pos3Xa	= spu_sub(tex3X, vecWidth2);
	vec_float4 pos3Xb	= spu_mul(pos3Xa,vecPX);

	vec_float4 pos3Ya	= spu_add(tex3Y, vecPosYOff);
	vec_float4 pos3Yb	= spu_mul(pos3Ya,vecPY);

	pos3X = spu_mul(pos3Xb, depth3);
	pos3Y = spu_mul(pos3Yb, depth3);
	pos3Z = depth3;


	//			float oodepth = 1.0f/depth;
	vec_float4 oodepth = spu_re(vecDepth);

	//			Vector3 dir0 = (pos0-pos);
	vec_float4 dir0X = spu_sub(pos0X, posX);
	vec_float4 dir0Y = spu_sub(pos0Y, posY);
	vec_float4 dir0Z = spu_sub(pos0Z, posZ);

	//			Vector3 dir1 = (pos1-pos);
	vec_float4 dir1X = spu_sub(pos1X, posX);
	vec_float4 dir1Y = spu_sub(pos1Y, posY);
	vec_float4 dir1Z = spu_sub(pos1Z, posZ);

	//			Vector3 dir2 = (pos2-pos);
	vec_float4 dir2X = spu_sub(pos2X, posX);
	vec_float4 dir2Y = spu_sub(pos2Y, posY);
	vec_float4 dir2Z = spu_sub(pos2Z, posZ);

	//			Vector3 dir3 = (pos3-pos);
	vec_float4 dir3X = spu_sub(pos3X, posX);
	vec_float4 dir3Y = spu_sub(pos3Y, posY);
	vec_float4 dir3Z = spu_sub(pos3Z, posZ);

	//			float len0 = dir0.Mag();
	vec_float4 len0a	= spu_mul (dir0X, dir0X);
	vec_float4 len0b	= spu_madd(dir0Y, dir0Y, len0a);
	vec_float4 len0c	= spu_madd(dir0Z, dir0Z, len0b);
	vec_float4 invlen0	= spu_rsqrte(len0c);
	vec_float4 len0		= spu_re(invlen0);

	//			float len1 = dir1.Mag();
	vec_float4 len1a	= spu_mul (dir1X, dir1X);
	vec_float4 len1b	= spu_madd(dir1Y, dir1Y, len1a);
	vec_float4 len1c	= spu_madd(dir1Z, dir1Z, len1b);
	vec_float4 invlen1	= spu_rsqrte(len1c);
	vec_float4 len1		= spu_re(invlen1);

	//			float len2 = dir2.Mag();
	vec_float4 len2a	= spu_mul (dir2X, dir2X);
	vec_float4 len2b	= spu_madd(dir2Y, dir2Y, len2a);
	vec_float4 len2c	= spu_madd(dir2Z, dir2Z, len2b);
	vec_float4 invlen2	= spu_rsqrte(len2c);
	vec_float4 len2		= spu_re(invlen2);

	//			float len3 = dir3.Mag();
	vec_float4 len3a	= spu_mul (dir3X, dir3X);
	vec_float4 len3b	= spu_madd(dir3Y, dir3Y, len3a);
	vec_float4 len3c	= spu_madd(dir3Z, dir3Z, len3b);
	vec_float4 invlen3	= spu_rsqrte(len3c);
	vec_float4 len3		= spu_re(invlen3);


	const vec_float4 five = {5.0f, 5.0f, 5.0f, 5.0f};

	//		float num=0.0f;
	vec_float4 num = vecConstZero;

	//			num	+=	rage::Min(rage::Max(1.0f-(dir0/len0).Dot(norm), len0*5.0f), 1.0f);
	vec_float4 twoLen0	= spu_mul(len0, five);
	twoLen0				= spu_mul	(twoLen0, oodepth);

	vec_float4 dir0Xn	= spu_mul (dir0X,	invlen0);
	vec_float4 dir0Yn	= spu_mul (dir0Y,	invlen0);
	vec_float4 dir0Zn	= spu_mul (dir0Z,	invlen0);
	vec_float4 num0a	= spu_mul (dir0Xn,	vecNormX);
	vec_float4 num0b	= spu_madd(dir0Yn,	vecNormY,	num0a);
	vec_float4 num0c	= spu_madd(dir0Zn,	vecNormZ,	num0b);
	vec_float4 num0d	= spu_sub (vecConstOne,	num0c);

	vec_uint4  num0_max	= spu_cmpgt(num0d,	twoLen0);
	vec_float4 num0e	= spu_sel  (twoLen0,num0d,		num0_max);

	vec_uint4  num0_min	= spu_cmpgt(num0e,	vecConstOne);
	vec_float4 num0f	= spu_sel  (num0e,	vecConstOne,		num0_min);

	num = spu_add(num, num0f);


	//			num	+=	rage::Min(rage::Max(1.0f-(dir1/len1).Dot(norm), len1*5.0f), 1.0f);
	vec_float4 twoLen1	= spu_mul(len1, five);
	twoLen1				= spu_mul	(twoLen1, oodepth);

	vec_float4 dir1Xn	= spu_mul (dir1X,	invlen1);
	vec_float4 dir1Yn	= spu_mul (dir1Y,	invlen1);
	vec_float4 dir1Zn	= spu_mul (dir1Z,	invlen1);
	vec_float4 num1a	= spu_mul (dir1Xn,	vecNormX);
	vec_float4 num1b	= spu_madd(dir1Yn,	vecNormY,	num1a);
	vec_float4 num1c	= spu_madd(dir1Zn,	vecNormZ,	num1b);
	vec_float4 num1d	= spu_sub (vecConstOne,	num1c);

	vec_uint4  num1_max	= spu_cmpgt(num1d,	twoLen1);
	vec_float4 num1e	= spu_sel (twoLen1,	num1d,		num1_max);

	vec_uint4  num1_min	= spu_cmpgt(num1e,	vecConstOne);
	vec_float4 num1f	= spu_sel  (num1e,	vecConstOne,		num1_min);

	num = spu_add(num, num1f);



	//			num	+=	rage::Min(rage::Max(1.0f-(dir2/len2).Dot(norm), len2*5.0f), 1.0f);
	vec_float4 twoLen2	= spu_mul	(len2, five);
	twoLen2				= spu_mul	(twoLen2, oodepth);

	vec_float4 dir2Xn	= spu_mul (dir2X,	invlen2);
	vec_float4 dir2Yn	= spu_mul (dir2Y,	invlen2);
	vec_float4 dir2Zn	= spu_mul (dir2Z,	invlen2);
	vec_float4 num2a	= spu_mul (dir2Xn,	vecNormX);
	vec_float4 num2b	= spu_madd(dir2Yn,	vecNormY,	num2a);
	vec_float4 num2c	= spu_madd(dir2Zn,	vecNormZ,	num2b);
	vec_float4 num2d	= spu_sub (vecConstOne,	num2c);

	vec_uint4  num2_max	= spu_cmpgt(num2d,	twoLen2);
	vec_float4 num2e	= spu_sel (twoLen2,	num2d,		num2_max);

	vec_uint4  num2_min	= spu_cmpgt(num2e,	vecConstOne);
	vec_float4 num2f	= spu_sel  (num2e,	vecConstOne,		num2_min);

	num = spu_add(num, num2f);

	//			num	+=	rage::Min(rage::Max(1.0f-(dir3/len3).Dot(norm), len3*5.0f), 1.0f);
	vec_float4 twoLen3	= spu_mul	(len3, five);
	twoLen3				= spu_mul	(twoLen3, oodepth);

	vec_float4 dir3Xn	= spu_mul	(dir3X,			invlen3);
	vec_float4 dir3Yn	= spu_mul	(dir3Y,			invlen3);
	vec_float4 dir3Zn	= spu_mul	(dir3Z,			invlen3);
	vec_float4 num3a	= spu_mul	(dir3Xn,		vecNormX);
	vec_float4 num3b	= spu_madd	(dir3Yn,		vecNormY,		num3a);
	vec_float4 num3c	= spu_madd	(dir3Zn,		vecNormZ,		num3b);
	vec_float4 num3d	= spu_sub	(vecConstOne,	num3c);

	vec_uint4  num3_max	= spu_cmpgt	(num3d,			twoLen3);
	vec_float4 num3e	= spu_sel	(twoLen3,		num3d,			num3_max);

	vec_uint4  num3_min	= spu_cmpgt(num3e,			vecConstOne);
	vec_float4 num3f	= spu_sel  (num3e,			vecConstOne,	num3_min);

	num = spu_add(num, num3f);
	//num = vecDepth;

	//			num		*=	0.25f;
	num = spu_mul(num, vecConst0_25);
	//			num		= rage::Powf(num,strength);
	//vec_float4 fnum	= powf4Gta(num, vecStrength);

	//			const u32 inum = u32(num*255);
	//			vec_uint4 inum = spu_convtu(fnum, 8);
	vec_float4 fnum255	= spu_mul(num, vecConst255);
	vec_uint4  inum		= spu_convtu(fnum255, 0);

	return(inum);

}// end of InternalProcessSSAO()...

#endif //VECTORIZED_SSA0...

//
//
//
//
void ProcessSSAOSPU(u8* dest_tile, u32* mini_depth, int starty, float px, float py, int offy, float strength)
{
	const vec_float4 vecPX		= spu_splats(px);
	const vec_float4 vecPY		= spu_splats(py);
	const vec_float4 vecWidth2	= spu_splats(float(BUF_SSAO_WIDTH2));
	const vec_float4 vecHeight2	= spu_splats(float(BUF_SSAO_HEIGHT2));
	const vec_float4 vecPosYOff	= spu_splats(float(-offy+starty-BUF_SSAO_HEIGHT2));
	const vec_float4 vecStrength= spu_splats(strength);

	for(u32 yy=0; yy< 1; yy++)
	{
		const s32 sy = yy+starty;
		const s32 oy = yy+offy;
		const vec_float4 vecSY	= spu_splats(float(sy));
		const vec_float4 vecOY	= spu_splats(float(oy));

		vec_uint4* dest = (vec_uint4*)&dest_tile[yy*BUF_SSAO_WIDTH];

		for(u32 xx=0; xx<BUF_SSAO_WIDTH; xx += 16)
		{
			vec_uint4 ssao0 = InternalProcessSSAO(xx+0, oy, vecStrength, vecSY, vecOY, vecPX, vecPY, vecPosYOff, vecWidth2, vecHeight2, mini_depth);
			vec_uint4 ssao1 = InternalProcessSSAO(xx+4, oy, vecStrength, vecSY, vecOY, vecPX, vecPY, vecPosYOff, vecWidth2, vecHeight2, mini_depth);
			vec_uint4 ssao2 = InternalProcessSSAO(xx+8, oy, vecStrength, vecSY, vecOY, vecPX, vecPY, vecPosYOff, vecWidth2, vecHeight2, mini_depth);
			vec_uint4 ssao3 = InternalProcessSSAO(xx+12,oy, vecStrength, vecSY, vecOY, vecPX, vecPY, vecPosYOff, vecWidth2, vecHeight2, mini_depth);

			vec_uint4 ssao01 = spu_shuffle(ssao0, ssao1, shufDHLP_dhlp_0000_0000);
			vec_uint4 ssao23 = spu_shuffle(ssao2, ssao3, shuf0000_0000_DHLP_dhlp);

			*(dest++) = spu_or(ssao01, ssao23);
		}//for (int xx=0; xx<BUF_SSAO_WIDTH; xx += 16)...

	}//for(u32 yy=0; yy<BUF_SSAO_LINES_PROCESSED; yy++)...

}// end of ProcessSSAOSPU()...

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//

void SSA0SPU_main(SSAOSpuParams *params)
{
	const float strength = params->m_ssaoStrength;
	const float projX = params->m_px / float(BUF_SSAO_WIDTH2);
	const float projY = params->m_py / float(BUF_SSAO_HEIGHT2);

	u8* mini_depthEA	= (u8*)infoPacket->m_sourceData;
	u32 mini_depthEndEA	= u32(mini_depthEA) + infoPacket->m_sourceStride*infoPacket->m_sourceHeight;
	u8* mini_depthPrevEA= (u8*)infoPacket->m_sourceDataPrev;
	u8* mini_depthNextEA= (u8*)infoPacket->m_sourceDataNext;
	u8* dest_bufferEA	= (u8*)infoPacket->m_destData;
	u32 linesLeftTODO	= infoPacket->m_destHeight;

	u32 batchNo=0;

	u32	outDmaTag = BLITOUT_DMA_TAG;

	u32* headDepthStart = &local_mini_depth[0];
	u32* currDepthStart = &local_mini_depth[SPUSSAO_FILTERWIDTH*BUF_SSAO_WIDTH];
	// well, for first batch read some random trash from area before mini_depth buffer
	if(mini_depthPrevEA)
	{
		const u32 mini_depthPrevEndEA = u32(&mini_depthPrevEA[0]) + infoPacket->m_sourceStride*infoPacket->m_sourceHeight - SPUSSAO_FILTERWIDTH*BUF_SSAO_WIDTH*sizeof(float);
		//get leading depth from previous batch
		sysDmaLargeGet(headDepthStart, mini_depthPrevEndEA, SPUSSAO_FILTERWIDTH*BUF_SSAO_WIDTH*sizeof(float), BLITIN_DMA_TAG);
		//get fill the rest of the mini depth buffer starting from the current batch to the end of the current batch
		sysDmaLargeGet(currDepthStart, u32(&mini_depthEA[0]), (SPUSSAO_FILTERWIDTH + 1)*BUF_SSAO_WIDTH*sizeof(float), BLITIN_DMA_TAG);
	}
	else
	{
		sysDmaLargeGet(headDepthStart, u32(&mini_depthEA[0]) - SPUSSAO_FILTERWIDTH*BUF_SSAO_WIDTH*sizeof(float), (SPUSSAO_FILTERWIDTH*2 + 1)*BUF_SSAO_WIDTH*sizeof(float), BLITIN_DMA_TAG);
	}
	
	sysDmaWait(1<<BLITIN_DMA_TAG);

	u32* tailDepth	= &local_mini_depth[BUF_SSAO_WIDTH*SPUSSAO_NUMLINES - BUF_SSAO_WIDTH]; //final line

	// middle batches:
	u32 nextFetchOffset = 0;
	while(linesLeftTODO)
	{
		//fetch next line of depth
		u32 fetchEA = (u32)&mini_depthEA[BUF_SSAO_WIDTH*(1*batchNo + SPUSSAO_FILTERWIDTH)*sizeof(u32)];
		if(fetchEA >= mini_depthEndEA && mini_depthNextEA)//going off the last line of current batch and start reading depth from next batch
		{	
			// hack - do not try this at home:
			// current fetch address outside current buffer?
			fetchEA = (u32)mini_depthNextEA; // hacky assumption for the last batch
			sysDmaLargeGet(tailDepth, fetchEA + nextFetchOffset, BUF_SSAO_WIDTH*sizeof(u32), BLITIN_DMA_TAG);
			nextFetchOffset += BUF_SSAO_WIDTH*sizeof(u32);
		}
		else //read next line of depth from current batch or read random trash from off the tail of the current depth batch
		{
			sysDmaLargeGet(tailDepth, fetchEA, BUF_SSAO_WIDTH*sizeof(u32), BLITIN_DMA_TAG);
		}

		sysDmaWait(1<<BLITIN_DMA_TAG);
		sysDmaWait(1<<outDmaTag);

		ProcessSSAOSPU((u8*)&local_dest_tile[0], local_mini_depth, batchNo, projX, projY, SPUSSAO_FILTERWIDTH, strength);
		sysDmaLargePut(&local_dest_tile[0], (u32)&dest_bufferEA[BUF_SSAO_WIDTH*batchNo], BUF_SSAO_WIDTH*1, outDmaTag);

		//re-center mini depth buffer around next center depth line
		for(int i = 0; i < SPUSSAO_NUMLINES - 1; i++)
			MemCpy128(&local_mini_depth[BUF_SSAO_WIDTH*i], &local_mini_depth[BUF_SSAO_WIDTH*(i+1)], BUF_SSAO_WIDTH*1*sizeof(u32));


		linesLeftTODO	-= 1;
		batchNo++;

		outDmaTag++;
		outDmaTag&=0x00000001;
	}

	sysDmaWait(1<<outDmaTag);
}// end of SSA0SPU_main()...

//
//
//
//
CSPUPostProcessDriverInfo* SSA0SPU_cr0(char* _end, void *infoPacketSource, unsigned int infoPacketSize)
{	
	const u32 jobTimeStart = spu_read_decrementer();

	//	sysLocalStoreInit(16*1024);

	// Start SPU memory allocator.  Align to info packet alignment to get best DMA performance.
	u32 spuAddr = matchAlignment128((u32)_end, infoPacketSource);
	// Get the job packet
	u32 alignedSize = AlignSize16(infoPacketSize);
	infoPacket = (CSPUPostProcessDriverInfo*)spuAddr;
	sysDmaGet(infoPacket, (u64)infoPacketSource, alignedSize, 0);
	spuAddr = stepLSAddr(spuAddr, alignedSize);
	sysDmaWaitTagStatusAll(1<<0);

	// Grab userdata packet (if any):
	if(infoPacket->m_userDataEA)
	{
		g_inSsaoParams = NULL;
		alignedSize = AlignSize16(infoPacket->m_userDataSize);
		u8 *userData = (u8*)spuAddr;
		sysDmaGet(userData, (u64)infoPacket->m_userDataEA, alignedSize, 0);
		spuAddr = stepLSAddr(spuAddr, alignedSize);
		sysDmaWaitTagStatusAll(1<<0);
		//spu_printf("\n CustomProcessSPU: userData: 0=%d 1=%d 2=%d 3=%d", userData[0], userData[1], userData[2], userData[3]);
		g_inSsaoParams = (SSAOSpuParams*)userData;
	}
	Assert(g_inSsaoParams);

	// allocate big static tables by hand (to minimize elf image size):
	spuAddr = Align(spuAddr, local_mini_depth_ALIGN);
	local_mini_depth = (u32*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, local_mini_depth_SIZE);
	//spu_printf("\n local_mini_depth=0x%X", (u32)local_mini_depth);

	spuAddr = Align(spuAddr, local_dest_tile_ALIGN);
	local_dest_tile	= (u32*)spuAddr;
	Assert(local_dest_tile);
	spuAddr = stepLSAddr(spuAddr, local_dest_tile_SIZE);
	//spu_printf("\n local_dest_tile=0x%X", (u32)local_dest_tile); 


	SPU_DEBUGF(spu_printf(DEBUG_SPU_JOB_NAME": process: infoPacketSource=0x%X, infoPacketSize=%d", (u32)infoPacketSource, infoPacketSize));
	SPU_DEBUGF(spu_printf(DEBUG_SPU_JOB_NAME": _end=0x%X, _etext=0x%X", u32(_end), u32(_etext)));



	SSA0SPU_main(g_inSsaoParams);

	if(g_inSsaoParams->m_spuProcessingTime)
	{
		const u32 spuId = (cellSpursGetCurrentSpuId()&0x03);	// 0-3
		cellAtomicAdd32((u32*)buffer128, (u64)&g_inSsaoParams->m_spuProcessingTime[spuId], (jobTimeStart-spu_read_decrementer())*1000/79800);
	}

	return(infoPacket);
}// end of SSA0SPU_cr0()...

#endif //__SPU....

