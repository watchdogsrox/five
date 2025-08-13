//
//	WaterUpdateDynamicCommon.h (parts taken from Water.cpp):
//
//  This header contains all the functions and data required by Water::UpdateDynamicWater()
//	which are shared among __SPU and other build targets
//	(in other words: this code is included and compiled for __SPU target).
//
//  Note: Don't add any game/system specific #includes (probably they will not compile on __SPU anyway);
//
//  
//	2007/03/14	- Andrzej:	- initial;
//
//
//
//
#ifndef __WATERUPDATEDYNAMICCOMMON_H__
#define __WATERUPDATEDYNAMICCOMMON_H__

#include "math/vecMath.h"
#include "atl/array.h"
#include "waterdefines.h"
#include "control/replay/replay.h"
//
//
// All code & stuff below is shared by SPU and standard target builds:
//
#define __GPUUPDATE (__XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS)
#define __USEVERTEXSTREAMRENDERTARGETS (__PS3 && 1)

#define MAXNUMWATERQUADS		(2048) // actual count for gta5 is currently 769

#define DYNAMICGRIDSIZE			(2)		// 2 meters apart

#define USE_OPTIMISATIONS 1

#if USE_OPTIMISATIONS
	#define FORCE_INLINE_SPU __forceinline
	#define UNROLL_LOOP 1
	#if __SPU
		#define USE_SI_CODE 1
	#else
		#define USE_SI_CODE 0
	#endif

#else
	#define FORCE_INLINE_SPU
	#define UNROLL_LOOP 0
	#define USE_SI_CODE 0
#endif


#if __SPU
	#if DMA_HEIGHT_BUFFER_WITH_JOB 
		#define WATER_HEIGHT_DMA_TAG       (0)
		#define WATER_NOISE_BUFFER_DMA_TAG (1)
		#define WATER_CALM_QUADS_DMA_TAG   (2)
		#define WATER_WAVE_BUFFER_AND_QUADS_DMA_TAG  (3)
	#else	
		#define WATER_HEIGHT_DMA_TAG       (0)
		#define WATER_NOISE_BUFFER_DMA_TAG (0)
		#define WATER_CALM_QUADS_DMA_TAG   (0)
		#define WATER_WAVE_BUFFER_AND_QUADS_DMA_TAG  (0)
	#endif
#endif


#define VALIDATE_SI_CODE 0

static s32			m_nNumOfWaterQuads = 0;
static s32			m_nNumOfWaterCalmingQuads = 0;
static s32			m_nNumOfExtraWaterCalmingQuads = 0;
static s32			m_nNumOfWaveQuads = 0;

#if USE_SI_CODE
static const vec_char16 s_vMaskA =  { 0x80, 0x80, 0x80,  0,
				       	 			  0x80, 0x80, 0x80, 16, 
				         			  0x80, 0x80, 0x80, 0x80, 
				         			  0x80, 0x80, 0x80, 0x80 };

static const vec_char16 s_vMaskB =  { 0x80, 0x80, 0x80, 3,
				       	 			  0x80, 0x80, 0x80, 7, 
				       	 			  0x80, 0x80, 0x80, 19, 
				       	 			  0x80, 0x80, 0x80, 23 };
#endif

#if __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS

#if __SPU
	#define WAVE_NOISE_BUFFER_NUM_ROWS	(DYNAMICGRIDELEMENTS)
	#define WAVE_NOISE_BUFFER_SIZE		(DYNAMICGRIDELEMENTS*sizeof(u8))
	#define WAVE_NOISE_BUFFER_ALIGN		(128)
	static u8 *m_WaveNoiseBuffer[WAVETEXTURERES]={NULL};
#else
	static u8 m_WaveNoiseBuffer [WAVETEXTURERES][WAVETEXTURERES]	ALIGNED(128);
#endif

struct	CWaterUpdateDynamicParams
{
	Vector3	m_CenterCoors;					// camera pos
	float	m_timestep;
	u32		m_timeInMilliseconds;
	float	m_disturbAmount;

	// The following variables used to add a sinus wave to the speed.
	float	m_waveLengthScale;
	float	m_waveMult;
	float	m_ShoreWavesMult;
	s32		m_waveTimePeriod;

	float	(*m_WaterHeightBuffer)[DYNAMICGRIDELEMENTS];	// double buffered read buffer for Render()
};

class CCPUTexture
{
	Vec2V				m_scale;
	Vec2V				m_bias;
	ScalarV				m_WidthV;
	ScalarV				m_MaxWidthV;
	u8*					m_tex;

	Vec4V				m_ActiveV;
	Vec4V				m_offsetsX;
	Vec4V				m_offsetsY;
	Vec4V				m_empty;
public:
	static const int PowWidth = 7;
	static const int Width = 1 << PowWidth;
	static const int NumBytes = Width * Width;

	CCPUTexture( unsigned char* tex = 0 ) 
	{
		m_WidthV =  ScalarVFromF32((float)Width);
		m_MaxWidthV =  ScalarVFromF32((float)Width - 1.0f);
		m_scale = Vec2V( V_ONE);
		m_bias = Vec2V( V_ZERO);

		float TexelSize = 1.0f/((float)Width - 1.0f);
		m_offsetsX = Vec4V( 0.0f,TexelSize , 0.0f, TexelSize);
		m_offsetsY = Vec4V( 0.0f,0.0f , TexelSize, TexelSize);

		Set(tex);
	}


	void Set( unsigned char* tex)
	{
		m_tex = tex;
		m_ActiveV = tex ? Vec4V(V_MASKXYZW) : Vec4V(V_ZERO);
		if (!tex) // point to black texture
		{
			tex =(u8*) &m_empty;
			m_empty= Vec4V( V_ONE);
		}
	}

	void SetTopDown( Vec2V_In scale, Vec2V_In bias)
	{
		m_scale = scale;
		m_bias = bias;
	}
	int GetTiled( int i ) const 
	{
#if __XENON
		int y  = i >> 7;
		int x = i - (y <<7);

		return XGAddress2DTiledOffset<128,1>( x, y);
#else
		return i;
#endif      
	}

	FORCE_INLINE_SPU Vec4V_Out Fetch4( Vec4V_In u, Vec4V_In v ) const
	{
		Vec4V tu = u - RoundToNearestIntNegInf(u);  // wrap operation
		Vec4V tv = v - RoundToNearestIntNegInf(v);

		Vec4V itu = RoundToNearestIntZero(tu * m_MaxWidthV); // convert to int between 0 - 128
		Vec4V itv = RoundToNearestIntZero(tv * m_MaxWidthV);

		Vec4V idxF =itu;
		idxF +=  itv * m_WidthV;     // calculate address

		idxF = idxF & m_ActiveV;  // clear to
		// convert to blocksize
		Vec4V idxV = FloatToIntRaw<0>(idxF);  // convert address to int

		// add m_tex
#if !USE_SI_CODE
		int* idx = (int*)&idxV;					// convert vector int to 4 ints
 		Assert( idx[0] >=0 && idx[0] < NumBytes);
 		Assert( idx[1] >=0 && idx[1] < NumBytes);
 		Assert( idx[2] >=0 && idx[2] < NumBytes);
 		Assert( idx[3] >=0 && idx[3] < NumBytes);
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		Vec4V r;
		if (m_tex)
		{
			r = Vec4V(	ScalarVFromU32(m_tex[GetTiled(idx[0])]),
						ScalarVFromU32(m_tex[GetTiled(idx[1])]),
						ScalarVFromU32(m_tex[GetTiled(idx[2])]),
						ScalarVFromU32(m_tex[GetTiled(idx[3])]));
		}
		else
		{
			r = Vec4V(V_ZERO);
		}
#else
		Vec4V r = Vec4V(ScalarVFromU32(m_tex[GetTiled(idx[0])]),
						ScalarVFromU32(m_tex[GetTiled(idx[1])]),
						ScalarVFromU32(m_tex[GetTiled(idx[2])]),
						ScalarVFromU32(m_tex[GetTiled(idx[3])]));
#endif
#else

		vec_char16 vIndices = (vec_char16)idxV.GetIntrin128();
		
		vec_char16 address = (vec_char16)spu_splats((u32)&m_tex[0]);

		address = si_a(address, vIndices);

		vec_char16 lowBits = si_andi(address, 0xf);
		vec_char16 vByte0 = si_lqd(address, 0);
		vByte0 = si_shlqby(vByte0, lowBits);


		vec_char16 address1 = si_shlqbyi(address, 4);
		vec_char16 lowBits1 = si_shlqbyi(lowBits, 4);
		vec_char16 vByte1 = si_lqd(address1, 0);
		vByte1 = si_shlqby(vByte1, lowBits1);


		vec_char16 address2 = si_shlqbyi(address, 8);
		vec_char16 lowBits2 = si_shlqbyi(lowBits, 8);
		vec_char16 vByte2 = si_lqd(address2, 0);
		vByte2 = si_shlqby(vByte2, lowBits2);

		vec_char16 address3 = si_shlqbyi(address, 12);
		vec_char16 lowBits3 = si_shlqbyi(lowBits, 12);
		vec_char16 vByte3 = si_lqd(address3, 0);
		vByte3 = si_shlqby(vByte3, lowBits3);

		vec_char16 vByte01 = si_shufb(vByte0, vByte1, s_vMaskA);

		vec_char16 vByte23 = si_shufb(vByte2, vByte3, s_vMaskA);
		vec_char16 vByte0123 = si_shufb(vByte01, vByte23, s_vMaskB);

		Vec4V r((vec_float4)vByte0123);

		#if VALIDATE_SI_CODE 
			int* idx = (int*)&idxV;
			int* vidx = (int*)&r;
			Assert( m_tex[idx[0]] == vidx[0]);
			Assert( m_tex[idx[1]] == vidx[1]);
			Assert( m_tex[idx[2]] == vidx[2]);
			Assert( m_tex[idx[3]] == vidx[3]);
		#endif

#endif
		return IntToFloatRaw<8>(r);  // convert vector int 0..255 to float 0..1
	}


	FORCE_INLINE_SPU ScalarV_Out tex2D( Vec2V_In tuv )  const
	{
		Vec2V uv = tuv;
 
		Vec4V u4 = Vec4V( uv.GetX() ) + m_offsetsX;
		Vec4V v4 = Vec4V( uv.GetY() ) + m_offsetsY;
		Vec4V r = Fetch4( u4, v4);

		// calculate weights
		uv = uv * m_MaxWidthV;
		uv = uv - RoundToNearestIntZero(uv);

		Vec2V invuv = Vec2V(V_ONE) - uv;

		ScalarV u2 = uv.GetX();
		ScalarV v2 = uv.GetY();

		ScalarV iu2 = invuv.GetX();
		ScalarV iv2 = invuv.GetY();

		Vec4V weights = Vec4V(iu2 * iv2, 
							  u2  * iv2, 
							  iu2 * v2,    
							  u2  * v2);
		return Dot(r, weights);
	}
};
#endif //__PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS

#if __SPU
	#define DYNAMIC_WATER_HEIGHT_NUM_ROWS		(DYNAMICGRIDELEMENTS)
	#define DYNAMIC_WATER_HEIGHT_SIZE			(DYNAMICGRIDELEMENTS*sizeof(float))
	#define DYNAMIC_WATER_HEIGHT_ALIGN			(128)
	static float		*m_DynamicWater_Height	[DYNAMICGRIDELEMENTS]={NULL};		// Water height as an offset to the height as defined by the artists (2 values for double buffering)

	#define DYNAMIC_WATER_DHEIGHT_NUM_ROWS		(DYNAMICGRIDELEMENTS)
	#define DYNAMIC_WATER_DHEIGHT_SIZE			(DYNAMICGRIDELEMENTS*sizeof(float))
	#define DYNAMIC_WATER_DHEIGHT_ALIGN			(128)
	static float		*m_DynamicWater_dHeight	[DYNAMICGRIDELEMENTS]={NULL};	// Basically the speed of the water going up or down. (meter/sec)

	#define MAXNUMWATERCALMINGQUADS	(512)
	#define MAXNUMWATERWAVEQUADS	(512)
	static CCalmingQuad	m_aCalmingQuads			[MAXNUMWATERCALMINGQUADS]	ALIGNED(128);
	static CWaveQuad	m_aWaveQuads			[MAXNUMWATERWAVEQUADS]		ALIGNED(128);

	#define WAVE_BUFFER_NUM_ROWS				(DYNAMICGRIDELEMENTS)
	#define WAVE_BUFFER_SIZE					(DYNAMICGRIDELEMENTS*sizeof(u8))
	#define WAVE_BUFFER_ALIGN					(128)
	static u8			*m_WaveBuffer			[WAVETEXTURERES]={NULL};
#else
	static float		(*m_DynamicWater_Height)[DYNAMICGRIDELEMENTS];						// Water height as an offset to the height as defined by the artists (2 values for double buffering)
	static float		(*m_DynamicWater_dHeight)[DYNAMICGRIDELEMENTS];						// Basically the speed of the water going up or down. (meter/sec)
	static CCalmingQuad (*m_aCalmingQuads);
#if __WIN32PC || __PPU || RSG_DURANGO || RSG_ORBIS
	static CWaveQuad	(*m_aWaveQuads);
	static CCalmingQuad (*m_aExtraCalmingQuads);
#endif // __WIN32PC || __PPU || RSG_DURANGO || RSG_ORBIS
	static u8			(*m_WaveBuffer)[WAVETEXTURERES];
#endif //__SPU

static s32	m_WorldBaseX, m_WorldBaseY;
static s32	m_GridBaseX, m_GridBaseY;

#if !__SPU
static mthRandom g_waterRand;
#endif

#define DYNAMICGRIDSIZE_POW2 1

inline u32 FindGridXFromWorldX(s32 WorldX, s32 WorldBaseX){	return (((WorldX - WorldBaseX)>>DYNAMICGRIDSIZE_POW2) + DYNAMICGRIDELEMENTS*1000)&(DYNAMICGRIDELEMENTS-1); }
inline u32 FindGridYFromWorldY(s32 WorldY, s32 WorldBaseY){	return (((WorldY - WorldBaseY)>>DYNAMICGRIDSIZE_POW2) + DYNAMICGRIDELEMENTS*1000)&(DYNAMICGRIDELEMENTS-1); }
inline u32 FindGridXFromWorldX(s32 WorldX){	return (((WorldX - m_WorldBaseX)>>DYNAMICGRIDSIZE_POW2) + DYNAMICGRIDELEMENTS*1000)&(DYNAMICGRIDELEMENTS-1); }
inline u32 FindGridYFromWorldY(s32 WorldY){	return (((WorldY - m_WorldBaseY)>>DYNAMICGRIDSIZE_POW2) + DYNAMICGRIDELEMENTS*1000)&(DYNAMICGRIDELEMENTS-1); }

/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : ClearDynamicWater
// PURPOSE :  Sets all of the dynamic water to 0.
//
/////////////////////////////////////////////////////////////////////////////////

static FORCE_INLINE_SPU void ClearDynamicWater()
{
#if USE_OPTIMISATIONS
	const s32 inc = 4;
	Vec4V zero = Vec4V(V_ZERO);
#else
	const s32 inc = 1;
#endif

	for (s32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
	{
		for (s32 Y = 0; Y < DYNAMICGRIDELEMENTS; Y += inc)
		{
			#if USE_OPTIMISATIONS
				*(Vec4V*)&m_DynamicWater_Height[X][Y]  = zero;
				*(Vec4V*)&m_DynamicWater_dHeight[X][Y] = zero;
			#else
				m_DynamicWater_Height[X][Y] = 0.0f;
				m_DynamicWater_dHeight[X][Y] = 0.0f;
			#endif
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : ClearOneWaterStripX
// PURPOSE :  Clears one strip that has just moved into our area.
//
/////////////////////////////////////////////////////////////////////////////////

static void	FORCE_INLINE_SPU ClearOneWaterStripX(s32 worldY)
{
	s32 Y = FindGridYFromWorldY(worldY);

	for (s32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
	{
		m_DynamicWater_Height[X][Y] = 0.0f;
		m_DynamicWater_dHeight[X][Y] = 0.0f;
	}
}


/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : ClearOneWaterStripY
// PURPOSE :  Clears one strip that has just moved into our area.
//
/////////////////////////////////////////////////////////////////////////////////
void FORCE_INLINE_SPU ClearOneWaterStripY(s32 worldX)
{	
#if USE_OPTIMISATIONS
	const s32 inc = 4;
	Vec4V zero = Vec4V(V_ZERO);
#else
	const s32 inc = 1;
#endif
	s32 X = FindGridXFromWorldX(worldX);

	for (s32 Y = 0; Y < DYNAMICGRIDELEMENTS; Y += inc)
	{
		#if USE_OPTIMISATIONS
			*(Vec4V*)&m_DynamicWater_Height[X][Y] = zero;
			*(Vec4V*)&m_DynamicWater_dHeight[X][Y] = zero;
		#else
			m_DynamicWater_Height[X][Y] = 0.0f;
			m_DynamicWater_dHeight[X][Y] = 0.0f;
		#endif
	}
}


/////////////////////////////////////////////////////////////////////////////////
//
// PURPOSE :  Grid shifting for PC.
//
/////////////////////////////////////////////////////////////////////////////////

static void ShiftGridsNegativeY(s32 columns)
{
	for (s32 X=DYNAMICGRIDELEMENTS - 1; X >= columns; X--) 
	{
		sysMemCpy( &m_DynamicWater_Height[X][0], &m_DynamicWater_Height[X - columns][0], sizeof(float)*DYNAMICGRIDELEMENTS);
		sysMemCpy( &m_DynamicWater_dHeight[X][0], &m_DynamicWater_dHeight[X - columns][0], sizeof(float)*DYNAMICGRIDELEMENTS);
	}

#if USE_OPTIMISATIONS
	const s32 inc = 4;
	Vec4V zero = Vec4V(V_ZERO);
#else
	const s32 inc = 1;
#endif
	for (s32 X = columns - 1; X >= 0; X--)
		for (s32 Y = 0; Y < DYNAMICGRIDELEMENTS; Y += inc)
		{
#if USE_OPTIMISATIONS
			*(Vec4V*)&m_DynamicWater_Height[X][Y] = zero;
			*(Vec4V*)&m_DynamicWater_dHeight[X][Y] = zero;
#else
			m_DynamicWater_Height[X][Y] = 0.0f;
			m_DynamicWater_dHeight[X][Y] = 0.0f;
#endif
		}
}

static void ShiftGridsPositiveY(s32 columns)
{
	for (s32 X=0; X<DYNAMICGRIDELEMENTS - columns; X++) 
	{
		sysMemCpy( &m_DynamicWater_Height[X][0], &m_DynamicWater_Height[X + columns][0], sizeof(float)*DYNAMICGRIDELEMENTS);
		sysMemCpy( &m_DynamicWater_dHeight[X][0], &m_DynamicWater_dHeight[X + columns][0], sizeof(float)*DYNAMICGRIDELEMENTS);
	}

#if USE_OPTIMISATIONS
	const s32 inc = 4;
	Vec4V zero = Vec4V(V_ZERO);
#else
	const s32 inc = 1;
#endif
	for (s32 X = DYNAMICGRIDELEMENTS - columns; X < DYNAMICGRIDELEMENTS; X++)
		for (s32 Y = 0; Y < DYNAMICGRIDELEMENTS; Y += inc)
		{
			#if USE_OPTIMISATIONS
				*(Vec4V*)&m_DynamicWater_Height[X][Y] = zero;
				*(Vec4V*)&m_DynamicWater_dHeight[X][Y] = zero;
			#else
				m_DynamicWater_Height[X][Y] = 0.0f;
				m_DynamicWater_dHeight[X][Y] = 0.0f;
			#endif
		}
}


static void ShiftGridsNegativeX(s32 rows)
{
	for (s32 Y=DYNAMICGRIDELEMENTS - 1; Y>=rows; Y--)
		for (s32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
		{
			m_DynamicWater_Height[X][Y] = m_DynamicWater_Height[X][Y - rows];
			m_DynamicWater_dHeight[X][Y] = m_DynamicWater_dHeight[X][Y - rows];
		}

	for (s32 Y = rows - 1; Y >= 0; Y--)
		for (s32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
		{
			m_DynamicWater_Height[X][Y] = 0.0f;
			m_DynamicWater_dHeight[X][Y] = 0.0f;
		}
}

static void ShiftGridsPositiveX(s32 rows)
{
	for (s32 Y=0; Y<DYNAMICGRIDELEMENTS - rows; Y++)
		for (s32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
		{
			m_DynamicWater_Height[X][Y] = m_DynamicWater_Height[X][Y + rows];
			m_DynamicWater_dHeight[X][Y] = m_DynamicWater_dHeight[X][Y + rows];
		}

	for (s32 Y = DYNAMICGRIDELEMENTS - rows; Y < DYNAMICGRIDELEMENTS; Y++)
		for (s32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
		{
			m_DynamicWater_Height[X][Y] = 0.0f;
			m_DynamicWater_dHeight[X][Y] = 0.0f;
		}
}

/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : UpdateDynamicWater
// PURPOSE :  Updates the water. Processes physics and stuff.
//
/////////////////////////////////////////////////////////////////////////////////
static __forceinline void ApplyCalmingQuad(const CCalmingQuad &quad, float timestep, int ASSERT_ONLY(quadIdx))
{
	s32 minX = quad.minX;
	if (minX < m_WorldBaseX + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE))
	{
		s32 maxX = quad.maxX;
		if (maxX >= m_WorldBaseX)
		{
			Assertf(minX < maxX, "[Process Calming Quads #%d] %d, %d", quadIdx, minX, maxX);

			s32 minY = quad.minY;
			if (minY < m_WorldBaseY + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE))
			{
				s32 maxY = quad.maxY;
				if (maxY >= m_WorldBaseY)
				{
					Assertf(minY < maxY, "[Process Calming Quads #%d] %d, %d", quadIdx, minY, maxY);

					// Clip the min and max values to the size of the dynamic area.
					minX = MAX(minX, m_WorldBaseX);
					maxX = MIN(maxX, m_WorldBaseX + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE));
					minY = MAX(minY, m_WorldBaseY);
					maxY = MIN(maxY, m_WorldBaseY + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE));

					// Make sure we have an area left to apply the dampening to.
					Assert(minX <= maxX);
					Assert(minY <= maxY);

					s32	gridX = FindGridXFromWorldX(minX);
					s32	gridY = FindGridYFromWorldY(minY);
					s32 maxGridX	= gridX + (maxX - minX)/DYNAMICGRIDSIZE;
					s32 maxGridY	= gridY + (maxY - minY)/DYNAMICGRIDSIZE;
					Assertf(maxGridX <= DYNAMICGRIDELEMENTS,	"[Process Calming Quads #%d maxGridX %d]", quadIdx, maxGridX);
					Assertf(maxGridY <= DYNAMICGRIDELEMENTS,	"[Process Calming Quads #%d maxGridY %d]", quadIdx, maxGridY);

					float dampenMult = rage::Powf(quad.m_fDampening, timestep);


#if UNROLL_LOOP
					s32 firstLoopXStart		= gridX;
					s32 firstLoopXEnd		= MIN(((gridX + 3)/4)*4, maxGridX);
					s32 secondLoopXStart	= firstLoopXEnd;
					s32 secondLoopXEnd		= firstLoopXEnd + ((maxGridX - firstLoopXEnd)/4)*4;
					s32 thirdLoopXStart		= secondLoopXEnd;
					s32 thirdLoopXEnd		= maxGridX;

					ScalarV	dampenMultScalar(dampenMult);

					for (s32 loopY = gridY; loopY < maxGridY; loopY++)
					{
						for(s32 loopX = firstLoopXStart; loopX < firstLoopXEnd; loopX++)
							m_DynamicWater_dHeight[loopY][loopX] *= dampenMult;

						for(s32 loopX = secondLoopXStart; loopX < secondLoopXEnd; loopX += 4)
							*(Vec4V*)&m_DynamicWater_dHeight[loopY][loopX] *= dampenMultScalar;
			
						for(s32 loopX = thirdLoopXStart; loopX < thirdLoopXEnd; loopX++)
							m_DynamicWater_dHeight[loopY][loopX] *= dampenMult;
					}
#else
					for (s32 loopY = gridY; loopY < maxGridY; loopY++)
						for (s32 loopX = gridX; loopX <maxGridX; loopX++)
							m_DynamicWater_dHeight[loopY][loopX] *= dampenMult;
#endif //UNROLL_LOOP
				}
			}
		}
	}
}
static void	UpdateDynamicWater(CWaterUpdateDynamicParams *pWaterParams)
{	
	Vector3	CenterCoors = pWaterParams->m_CenterCoors;

	s32	OldWorldBaseX = m_WorldBaseX;
	s32	OldWorldBaseY = m_WorldBaseY;


	m_GridBaseX = FindGridXFromWorldX(m_WorldBaseX);
	m_GridBaseY = FindGridYFromWorldY(m_WorldBaseY);

	s32 cameraRelX		= ((s32)floor(CenterCoors.x/DYNAMICGRIDSIZE))*DYNAMICGRIDSIZE;
	s32 cameraRelY		= ((s32)floor(CenterCoors.y/DYNAMICGRIDSIZE))*DYNAMICGRIDSIZE;
	s32 cameraRangeMinX = cameraRelX - DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
	s32 cameraRangeMinY	= cameraRelY - DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
	m_WorldBaseX		= cameraRangeMinX;
	m_WorldBaseY		= cameraRangeMinY;
	
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif

	sysMemCpy(m_DynamicWater_Height, pWaterParams->m_WaterHeightBuffer, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);

	if (m_WorldBaseX != OldWorldBaseX || m_WorldBaseY != OldWorldBaseY)
	{
		#if __SPU && DMA_HEIGHT_BUFFER_WITH_JOB
			sysDmaWait(1 << WATER_HEIGHT_DMA_TAG);
		#endif

		if ( (ABS(OldWorldBaseX - m_WorldBaseX) > DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE) ||
			(ABS(OldWorldBaseY - m_WorldBaseY) > DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE))
		{
			ClearDynamicWater();	// Clear the whole grid in one go
		}
		else
		{
			// Clear the strip that has scrolled into our area
			if (OldWorldBaseX < m_WorldBaseX)
			{
				ShiftGridsPositiveX((m_WorldBaseX - OldWorldBaseX) / DYNAMICGRIDSIZE);
			}
			if (OldWorldBaseX > m_WorldBaseX)
			{
				ShiftGridsNegativeX((OldWorldBaseX - m_WorldBaseX) / DYNAMICGRIDSIZE);
			}
			if (OldWorldBaseY < m_WorldBaseY)
			{
				ShiftGridsPositiveY((m_WorldBaseY - OldWorldBaseY) / DYNAMICGRIDSIZE);
			}
			if (OldWorldBaseY > m_WorldBaseY)
			{
				ShiftGridsNegativeY((OldWorldBaseY - m_WorldBaseY) / DYNAMICGRIDSIZE);
			}
		}
	}

#if REPLAY_WATER_ENABLE_DEBUG
	static char debugString[256];
	sprintf(debugString, "[CPUW] POS %f %f WB %i %i", CenterCoors.GetX(), CenterCoors.GetY(), m_WorldBaseX, m_WorldBaseY);
	grcDebugDraw::Text(Vec2V(0.01f, 0.5f), Color32(0.0f, 0.0f, 0.0f), debugString, true, 1.5f);
#endif

	const float timestep = pWaterParams->m_timestep;

	// Pre-calculate spring value
	static dev_float SPRING = (40.0f);
	const float	PreCalcSpring = timestep * SPRING;

	// Pre-calculate fall back value
	static dev_float FALLBACK = (0.5f);
	const float	PreCalcFallBack = timestep * FALLBACK;

	// Pre-calculate friction value 
	static dev_float FRICTION = (0.7f);
	const float	PreCalcFriction = rage::Powf(FRICTION, timestep);  

#if __SPU
	mthRandom g_waterRand(spu_read_decrementer());
#endif

#if __DEV
	if (pWaterParams->m_waveTimePeriod<1) pWaterParams->m_waveTimePeriod=1;
#endif

#if __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS
	float mult = pWaterParams->m_waveMult*timestep;
	float timeFactorScale = ( (2.0f * PI) / float(pWaterParams->m_waveTimePeriod))/64.0f;
	float timeFactor0 = (pWaterParams->m_timeInMilliseconds % pWaterParams->m_waveTimePeriod) * timeFactorScale;

#if __SPU && DMA_HEIGHT_BUFFER_WITH_JOB
	sysDmaWait((1 << WATER_NOISE_BUFFER_DMA_TAG) | (1 << WATER_HEIGHT_DMA_TAG));
#endif
	CCPUTexture	waterTex(&m_WaveNoiseBuffer[0][0]);
	
	const float texScale = DYNAMICGRIDELEMENTS/WAVETEXTURERES;
	
	// Add waves
	float scale0 = (2.0f/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS)*pWaterParams->m_waveLengthScale*texScale;

	float noiseOffsetX	= ((float)g_waterRand.GetRanged(0, WAVETEXTURERES))/WAVETEXTURERES;
	float noiseOffsetY	= ((float)g_waterRand.GetRanged(0, WAVETEXTURERES))/WAVETEXTURERES;
	float noiseX		= noiseOffsetX;
	static float noiseStep = 4.0f/WAVETEXTURERES;

	float disturbScale	= pWaterParams->m_disturbAmount*timestep;

	float wtxO		= ((float)m_WorldBaseX)/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS*pWaterParams->m_waveLengthScale*texScale;
	float wtyO		= ((float)m_WorldBaseY)/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS*pWaterParams->m_waveLengthScale*texScale;
	float wtx		= wtxO + pWaterParams->m_timeInMilliseconds/400000.0f;
	float wty		= wtyO + timeFactor0;

	float ty		= wty;

	for (s32 loopY = 0; loopY < DYNAMICGRIDELEMENTS; loopY++)
	{
		float tx				= wtx;
		float noiseY			= noiseOffsetY;

		const float offsetHack	= 2/255.0f; //Old texture was not normalized, but we want to preserve the old heights for now...
		const float offset		= -0.5f + offsetHack;
		
#if UNROLL_LOOP
		for (s32 loopX = 0; loopX < DYNAMICGRIDELEMENTS;)
		{
			float waveA		= waterTex.tex2D(Vec2V(tx, ty)).Getf() + offset;
			float disturbA	= waterTex.tex2D(Vec2V(noiseX, noiseY)).Getf() + offset;
			float pushA		= disturbA*disturbScale + mult*waveA;
			m_DynamicWater_dHeight[loopY][loopX] += pushA;
			loopX++;
			tx				+= scale0;
			noiseY			+= noiseStep;

			float waveB		= waterTex.tex2D(Vec2V(tx, ty)).Getf() + offset;
			float disturbB	= waterTex.tex2D(Vec2V(noiseX, noiseY)).Getf() + offset;
			float pushB		= disturbB*disturbScale + mult*waveB;
			m_DynamicWater_dHeight[loopY][loopX] += pushB;
			loopX++;
			tx				+= scale0;
			noiseY			+= noiseStep;

			float waveC		= waterTex.tex2D(Vec2V(tx, ty)).Getf() + offset;
			float disturbC	= waterTex.tex2D(Vec2V(noiseX, noiseY)).Getf() + offset;
			float pushC		= disturbC*disturbScale + mult*waveC;
			m_DynamicWater_dHeight[loopY][loopX] += pushC;
			loopX++;
			tx				+= scale0;
			noiseY			+= noiseStep;

			float waveD		= waterTex.tex2D(Vec2V(tx, ty)).Getf() + offset;
			float disturbD	= waterTex.tex2D(Vec2V(noiseX, noiseY)).Getf() + offset;
			float pushD		= disturbD*disturbScale + mult*waveD;
			m_DynamicWater_dHeight[loopY][loopX] += pushD;
			loopX++;
			tx				+= scale0;
			noiseY			+= noiseStep;
		}
#else
		for (s32 loopX = 0; loopX < DYNAMICGRIDELEMENTS; loopX++)
		{
			float disturb	= waterTex.tex2D(Vec2V(noiseX, noiseY)).Getf() + offset;
			float wave		= waterTex.tex2D(Vec2V(tx, ty)).Getf() + offset;
			float push		= disturb*disturbScale + wave*mult;
			m_DynamicWater_dHeight[loopY][loopX] += push;
			tx				+= scale0;
			noiseY			+= noiseStep;
		}
#endif

		ty		+= scale0;
		noiseX	+= noiseStep;
	}
#endif //__PS3 || _WIN32PC

#if UNROLL_LOOP
	ScalarV PreCalcSpringScalar(-1.0f*PreCalcSpring);
	ScalarV PreCalcFallBackScalar(-1.0f*PreCalcFallBack);
	ScalarV PreCalcFrictionScalar(PreCalcFriction);

	for (int xx = 0; xx < DYNAMICGRIDELEMENTS; xx++)
	{
		int X = xx;
		
		u32 XPlus;
		u32 XMin;

		if(xx == DYNAMICGRIDELEMENTS-1)
			XPlus	= xx;
		else
			XPlus	= xx + 1;

		if(xx == 0)
			XMin	= xx;
		else
			XMin	= xx - 1;

		for (int yy = 0; yy < DYNAMICGRIDELEMENTS; yy += 4)
		{
			u32 Y1 = yy + 0;
			u32 Y2 = yy + 1;
			u32 Y3 = yy + 2;
			u32 Y4 = yy + 3;

			u32 YMin1;
			u32 YPlus1;
			u32 YMin2;
			u32 YPlus2;
			u32 YMin3;
			u32 YPlus3;
			u32 YMin4;
			u32 YPlus4;

			if(Y1 == 0)
				YMin1 = Y1;
			else
				YMin1 = Y1 - 1;

			YPlus1	= Y2;

			YMin2	= Y1;
			YPlus2	= Y3;

			YMin3	= Y2;
			YPlus3	= Y4;

			YMin4	= Y3;

			if(Y4 == DYNAMICGRIDELEMENTS - 1)
				YPlus4 = Y4;
			else
				YPlus4 = Y4 + 1;

			float	AverageHeight1 = 0.17f * (m_DynamicWater_Height[XMin][Y1] +
				m_DynamicWater_Height[X][YMin1] +
				m_DynamicWater_Height[X][YPlus1] +
				m_DynamicWater_Height[XPlus][Y1]) +
				0.08f * (m_DynamicWater_Height[XMin][YMin1] +
				m_DynamicWater_Height[XMin][YPlus1] +
				m_DynamicWater_Height[XPlus][YMin1] +
				m_DynamicWater_Height[XPlus][YPlus1]);
			float	AverageHeight2 = 0.17f * (m_DynamicWater_Height[XMin][Y2] +
				m_DynamicWater_Height[X][YMin2] +
				m_DynamicWater_Height[X][YPlus2] +
				m_DynamicWater_Height[XPlus][Y2]) +
				0.08f * (m_DynamicWater_Height[XMin][YMin2] +
				m_DynamicWater_Height[XMin][YPlus2] +
				m_DynamicWater_Height[XPlus][YMin2] +
				m_DynamicWater_Height[XPlus][YPlus2]);
			float	AverageHeight3 = 0.17f * (m_DynamicWater_Height[XMin][Y3] +
				m_DynamicWater_Height[X][YMin3] +
				m_DynamicWater_Height[X][YPlus3] +
				m_DynamicWater_Height[XPlus][Y3]) +
				0.08f * (m_DynamicWater_Height[XMin][YMin3] +
				m_DynamicWater_Height[XMin][YPlus3] +
				m_DynamicWater_Height[XPlus][YMin3] +
				m_DynamicWater_Height[XPlus][YPlus3]);
			float	AverageHeight4 = 0.17f * (m_DynamicWater_Height[XMin][Y4] +
				m_DynamicWater_Height[X][YMin4] +
				m_DynamicWater_Height[X][YPlus4] +
				m_DynamicWater_Height[XPlus][Y4]) +
				0.08f * (m_DynamicWater_Height[XMin][YMin4] +
				m_DynamicWater_Height[XMin][YPlus4] +
				m_DynamicWater_Height[XPlus][YMin4] +
				m_DynamicWater_Height[XPlus][YPlus4]);

			Vec4V AverageHeight(AverageHeight1, AverageHeight2, AverageHeight3, AverageHeight4);
			Vec4V HeightDiff = *(Vec4V*)&m_DynamicWater_Height[X][Y1] - AverageHeight;
				
			// Spring value
			*(Vec4V*)&m_DynamicWater_dHeight[X][Y1] = AddScaled(*(Vec4V*)&m_DynamicWater_dHeight[X][Y1], HeightDiff, PreCalcSpringScalar);
				
			// fall back to equilibrium (0.0)
			*(Vec4V*)&m_DynamicWater_dHeight[X][Y1] = AddScaled(*(Vec4V*)&m_DynamicWater_dHeight[X][Y1], *(Vec4V*)&m_DynamicWater_Height[X][Y1], PreCalcFallBackScalar);

			// Do some friction too
			*(Vec4V*)&m_DynamicWater_dHeight[X][Y1] *= PreCalcFrictionScalar;
		}
	}
#else
	// Go through the array and update the water velocities.
	for (int X = 0; X < DYNAMICGRIDELEMENTS; X++)
	{
		u32 XPlus;
		u32 XMin;

		if(X == DYNAMICGRIDELEMENTS-1)
			XPlus	= X;
		else
			XPlus	= X + 1;

		if(X == 0)
			XMin	= X;
		else
			XMin	= X - 1;

		for (int Y = 0; Y < DYNAMICGRIDELEMENTS; Y++)
		{
			u32	YPlus;
			u32	YMin;

			if(Y == DYNAMICGRIDELEMENTS-1)
				YPlus	= Y;
			else
				YPlus	= Y + 1;

			if(Y == 0)
				YMin	= Y;
			else
				YMin	= Y - 1;


			float	AverageHeight = 0.17f*(	m_DynamicWater_Height[XMin][Y] +
											m_DynamicWater_Height[XPlus][Y] +
											m_DynamicWater_Height[X][YMin] +
											m_DynamicWater_Height[X][YPlus]) +
									0.08f*(	m_DynamicWater_Height[XPlus][YMin] +
											m_DynamicWater_Height[XPlus][YPlus] +
											m_DynamicWater_Height[XMin][YMin] +
											m_DynamicWater_Height[XMin][YPlus]);



			// Spring value
			float	HeightDiff = m_DynamicWater_Height[X][Y] - AverageHeight;
			m_DynamicWater_dHeight[X][Y] -= HeightDiff * PreCalcSpring;

			// fall back to equilibrium (0.0)
			m_DynamicWater_dHeight[X][Y] -= m_DynamicWater_Height[X][Y] * PreCalcFallBack;


			// Do some friction too
			m_DynamicWater_dHeight[X][Y] *= PreCalcFriction;
		}
	}
#endif // UNROLL_LOOP


#if __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS

#if __SPU  && DMA_HEIGHT_BUFFER_WITH_JOB
	sysDmaWait(1 << WATER_WAVE_BUFFER_AND_QUADS_DMA_TAG);
	Assert(m_nNumOfWaveQuads <= MAXNUMWATERWAVEQUADS);
#endif

	CCPUTexture	waveTex(&m_WaveBuffer[0][0]);
	float txO		= ((float)m_WorldBaseX)/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS;
	float tyO		= ((float)m_WorldBaseY)/DYNAMICGRIDSIZE/DYNAMICGRIDELEMENTS;

	// We go through the wave polys and if they happen to stretch into the dynamic water area their wave effect is applied.
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	if (m_aWaveQuads != NULL)
#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS 
		for (int quad = 0; quad < m_nNumOfWaveQuads; quad++)
		{
			s32 minX = m_aWaveQuads[quad].minX;
			if (minX < m_WorldBaseX + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE))
			{
				s32 maxX = m_aWaveQuads[quad].maxX;
				if (maxX >= m_WorldBaseX)
				{
					Assertf(minX < maxX, "[Process Wave Quads #%d] %d, %d", quad, minX, maxX);

					s32 minY = m_aWaveQuads[quad].minY;
					if (minY < m_WorldBaseY + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE))
					{
						s32 maxY = m_aWaveQuads[quad].maxY;
						if (maxY >= m_WorldBaseY)
						{
							Assertf(minY < maxY, "[Process Wave Quads #%d] %d, %d", quad, minY, maxY);

							// Clip the min and max values to the size of the dynamic area.
							minX = MAX(minX, m_WorldBaseX);
							maxX = MIN(maxX, m_WorldBaseX + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE));
							minY = MAX(minY, m_WorldBaseY);
							maxY = MIN(maxY, m_WorldBaseY + (DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE));

							// Make sure we have an area left to apply the dampening to.
							Assert(minX <= maxX);
							Assert(minY <= maxY);

							s32	gridX = FindGridXFromWorldX(minX);
							s32	gridY = FindGridYFromWorldY(minY);
							s32 maxGridX	= gridX + (maxX - minX)/DYNAMICGRIDSIZE;
							s32 maxGridY	= gridY + (maxY - minY)/DYNAMICGRIDSIZE;
							Assertf(maxGridX <= DYNAMICGRIDELEMENTS,	"[Process Wave Quads #%d maxGridX %d]", quad, maxGridX);
							Assertf(maxGridY <= DYNAMICGRIDELEMENTS,	"[Process Wave Quads #%d maxGridY %d]", quad, maxGridY);

							Vec3V waveDirN	= Vec3V(-m_aWaveQuads[quad].GetXDirection(), -m_aWaveQuads[quad].GetYDirection(), 0.0f);
							waveDirN		= Normalize(waveDirN);
							Vec3V y			= Vec3V(V_Z_AXIS_WZERO);
							Vec3V waveDirT	= Normalize(Cross(waveDirN, y));

							Vec2V waveTexO		= Vec2V(txO, tyO);
							Vec2V waveDirNV2	= waveDirN.GetXY();
							Vec2V waveDirTV2	= waveDirT.GetXY();
							
							float texScale = 1.0f/WAVETEXTURERES;
							float tx	= Dot(waveTexO, waveDirNV2).Getf();
							float ty	= Dot(waveTexO, waveDirTV2).Getf();
							tx			= tx + pWaterParams->m_timeInMilliseconds/50000.0f;
							float dxX	= waveDirN.GetXf()*texScale;
							float dxY	= waveDirN.GetYf()*texScale;
							float dyX	= waveDirT.GetXf()*texScale;
							float dyY	= waveDirT.GetYf()*texScale;


							float shoreWavesMult = pWaterParams->m_ShoreWavesMult * timestep * m_aWaveQuads[quad].GetAmplitude();

							const float waveMult	= 1.723f;
							const float waveAdd		= -0.723f;

#if UNROLL_LOOP
							ScalarV shoreWavesMultScalar(shoreWavesMult);
							ScalarV vWaveMult(waveMult);
							Vec4V vWaveAdd(waveAdd, waveAdd, waveAdd, waveAdd);

							s32 firstLoopXStart		= gridX;
							s32 firstLoopXEnd		= MIN(((gridX + 3)/4)*4, maxGridX);
							s32 secondLoopXStart	= firstLoopXEnd;
							s32 secondLoopXEnd		= firstLoopXEnd + ((maxGridX - firstLoopXEnd)/4)*4;
							s32 thirdLoopXStart		= secondLoopXEnd;
							s32 thirdLoopXEnd		= maxGridX;

							Vec2V txV				= Vec2V(tx, ty);
							Vec2V dtxXV				= Vec2V(dxX, dxY);
							Vec2V dtxYV				= Vec2V(dyX, dyY);

							for (s32 loopY = gridY; loopY < maxGridY; loopY++)
							{
								Vec2V loopXTXV	= txV;

								for(s32 loopX = firstLoopXStart; loopX < firstLoopXEnd; loopX++)
								{
									float wave	= waveTex.tex2D(loopXTXV).Getf();
									m_DynamicWater_dHeight[loopY][loopX] += (waveMult*wave + waveAdd)*shoreWavesMult;
									loopXTXV	+= dtxXV;
								}

								for(s32 loopX = secondLoopXStart; loopX < secondLoopXEnd; loopX += 4)
								{
									ScalarV wave1 = waveTex.tex2D(loopXTXV);
									loopXTXV	+= dtxXV;
									ScalarV wave2 = waveTex.tex2D(loopXTXV);
									loopXTXV	+= dtxXV;
									ScalarV wave3 = waveTex.tex2D(loopXTXV);
									loopXTXV	+= dtxXV;
									ScalarV wave4 = waveTex.tex2D(loopXTXV);
									loopXTXV	+= dtxXV;

									Vec4V vWave(wave1, wave2, wave3, wave4);
									vWave = AddScaled(vWaveAdd, vWave, vWaveMult);

									*(Vec4V*)&m_DynamicWater_dHeight[loopY][loopX] = AddScaled(*(Vec4V*)&m_DynamicWater_dHeight[loopY][loopX], vWave, shoreWavesMultScalar);
								}

								for(s32 loopX = thirdLoopXStart; loopX < thirdLoopXEnd; loopX++)
								{
									float wave	= waveTex.tex2D(loopXTXV).Getf();
									m_DynamicWater_dHeight[loopY][loopX] += (waveMult*wave + waveAdd)*shoreWavesMult;
									loopXTXV	+= dtxXV;
								}

								txV	= txV + dtxYV;
							}
#else
							for (s32 loopY = gridY; loopY < maxGridY; loopY++)
							{
								float loopXTX	= tx;
								float loopXTY	= ty;

								for (s32 loopX = gridX; loopX <maxGridX; loopX++)
								{
									float wave = waveTex.tex2D(Vec2V(loopXTX, loopXTY)).Getf();
									m_DynamicWater_dHeight[loopY][loopX] += (waveMult*wave + waveAdd)*shoreWavesMult;
									loopXTX += dxX;
									loopXTY += dxY;
								}

								tx += dyX;
								ty += dyY;
							}
#endif
						}
					}
				}
			}
		}
#endif //__PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS

#if __SPU && DMA_HEIGHT_BUFFER_WITH_JOB
	sysDmaWait(1 << WATER_CALM_QUADS_DMA_TAG);
	Assert(m_nNumOfWaterCalmingQuads <= MAXNUMWATERCALMINGQUADS);
#endif

	// We go through the calming polys and if they happen to stretch into the dynamic water area their calming effect is applied.
#if RSG_PC || RSG_DURANGO || RSG_XENON || RSG_ORBIS
	if (m_aCalmingQuads != NULL)
#endif
	for (int quadIdx = 0; quadIdx < m_nNumOfWaterCalmingQuads; quadIdx++)
	{
		CCalmingQuad quad = m_aCalmingQuads[quadIdx];
		ApplyCalmingQuad(quad,timestep,quadIdx);
	}

#if RSG_PC || RSG_DURANGO || RSG_XENON || RSG_ORBIS
	if (m_aExtraCalmingQuads != NULL)
	{
		for (int quadIdx = 0; quadIdx < m_nNumOfExtraWaterCalmingQuads; quadIdx++)
		{
			CCalmingQuad quad = m_aExtraCalmingQuads[quadIdx];
			if( quad.m_fDampening < 1.0f )
			{
				ApplyCalmingQuad(quad,timestep,quadIdx);
			}
		}
	}
#endif

	//====================== Update Height =========================
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	if (m_DynamicWater_Height != NULL)
#endif
	{
#if UNROLL_LOOP
		ScalarV timestepScalar(timestep);

		for (u32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
		{
			for (u32 Y = 0; Y < DYNAMICGRIDELEMENTS; Y+=4)
			{
				*(Vec4V*)&m_DynamicWater_Height[X][Y] = AddScaled(*(Vec4V*)&m_DynamicWater_Height[X][Y], *(Vec4V*)&m_DynamicWater_dHeight[X][Y], timestepScalar);
			}
		}
#else
		for (u32 X = 0; X < DYNAMICGRIDELEMENTS; X++)
		{
			for (u32 Y = 0; Y < DYNAMICGRIDELEMENTS; Y++)
			{
				m_DynamicWater_Height[X][Y] += m_DynamicWater_dHeight[X][Y] * timestep;
			}
		}
#endif // UNROLL_LOOP
	}
}

#endif //__WATERUPDATEDYNAMICCOMMON_H__...



