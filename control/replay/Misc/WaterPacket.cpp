#include "WaterPacket.h"

#if GTA_REPLAY

#include "control/replay/Compression/Quantization.h"

const static float minQuantizedRange = -5.0f;
const static float maxQuantizedRange = 5.0f;

//////////////////////////////////////////////////////////////////////////

#if DYNAMICGRIDELEMENTS == 128
#define DYNAMICGRIDELEMENTS_LOG2	7
#elif DYNAMICGRIDELEMENTS == 256
#define DYNAMICGRIDELEMENTS_LOG2	8
#elif
CompileTimeAssert(0, "Need to do something here like pad out height field");
#endif

#define WATER_CUBIC_PATCH_SIZE_LOG_2 3
#define WATER_CUBIC_PATCH_STORAGE_NUM_BITS 8

bool CPacketCubicPatchWaterFrame::ms_LeastSquaresMatrixBuilt;
bool CPacketCubicPatchWaterFrame::ms_ControlPointsToCubicBuilt;
Matrix44 CPacketCubicPatchWaterFrame::ms_LeastSquaresMatrix;
Matrix44 CPacketCubicPatchWaterFrame::ms_ControlPointsToCubic;
u32 CPacketCubicPatchWaterFrame::ms_PacketNumber = 0;


#if !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
float CPacketCubicPatchWaterFrame::ms_WaterHeightBuffer[2][DYNAMICGRIDELEMENTS][DYNAMICGRIDELEMENTS];
#else // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
u32 CPacketCubicPatchWaterFrame::ms_NextBufferIndex = 0;
void *CPacketCubicPatchWaterFrame::ms_WaterHeightTextureLockBase[3] = { NULL };
grcTexture *CPacketCubicPatchWaterFrame::ms_WaterHeightTexture[3] = { NULL };
grcRenderTarget *CPacketCubicPatchWaterFrame::ms_WaterHeightRenderTarget[3] = { NULL };
float CPacketCubicPatchWaterFrame::ms_WaterHeightBufferToCompress[DYNAMICGRIDELEMENTS][DYNAMICGRIDELEMENTS];
#endif // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

void CPacketCubicPatchWaterFrame::Store(u32 gameTime)
{
	(void)gameTime;
	PACKET_EXTENSION_RESET(CPacketCubicPatchWaterFrame);
	CPacketBase::Store(PACKETID_CUBIC_PATCH_WATERFRAME, sizeof(CPacketCubicPatchWaterFrame));
	CPacketLinked::Reset();

	float* heightData = Water::GetWaterHeightBufferForRecording();

	if(heightData)
	{
		replayDebugf2("Recording water height map");

		WATER_PATCH *pPatches = (WATER_PATCH *)((u8 *)this + GetPacketSize());
		patchAllocator allocator(pPatches);

		Vector3 centrePos = Water::GetCurrentWaterPos();
		u16 gridWorldBaseX, gridWorldBaseY;
		ComputeStartXAndY(centrePos, m_StartX, m_StartY, gridWorldBaseX, gridWorldBaseY);

		// And we miss out info at the edges.
		for(u32 x = 0; x < (0x1 << (DYNAMICGRIDELEMENTS_LOG2 - WATER_CUBIC_PATCH_SIZE_LOG_2)) - 1; ++x)
		{
			for(u32 y = 0; y < (0x1 << (DYNAMICGRIDELEMENTS_LOG2 - WATER_CUBIC_PATCH_SIZE_LOG_2)) - 1; ++y)
			{
				BuildPatch(m_StartX + (x << WATER_CUBIC_PATCH_SIZE_LOG_2), m_StartY + (y << WATER_CUBIC_PATCH_SIZE_LOG_2), WATER_CUBIC_PATCH_SIZE_LOG_2, allocator, heightData, GetLeastSquaresMatrix(WATER_CUBIC_PATCH_SIZE_LOG_2));
			}
		}

		m_PatchSizeLog2 = WATER_CUBIC_PATCH_SIZE_LOG_2;
		m_PatchCount = ((0x1 << (DYNAMICGRIDELEMENTS_LOG2 - WATER_CUBIC_PATCH_SIZE_LOG_2)) - 1);
		AddToPacketSize(sizeof(WATER_PATCH)*m_PatchCount*m_PatchCount);
	}
	else
	{
		m_PatchSizeLog2 = 0;
		m_PatchCount = 0;
	}
}


const u32 g_CPacketCubicPatchWaterFrame_Extensions_V1 = 1;
const u32 g_CPacketCubicPatchWaterFrame_Extensions_V2 = 2;

void CPacketCubicPatchWaterFrame::StoreExtensions(u32 gameTime)
{
	Vector3 centrePos = Water::GetCurrentWaterPos();
	u16 unused;
	u16 gridWorldBaseX, gridWorldBaseY;
	ComputeStartXAndY(centrePos, unused, unused, gridWorldBaseX, gridWorldBaseY);

	// Write out grid base coords in the extension.
	WATER_FRAME_PACKET_EXTENSION *pExtension = (WATER_FRAME_PACKET_EXTENSION *)BEGIN_PACKET_EXTENSION_WRITE(g_CPacketCubicPatchWaterFrame_Extensions_V2, CPacketCubicPatchWaterFrame);
	pExtension->gridBaseX = gridWorldBaseX;
	pExtension->gridBaseY = gridWorldBaseY;
	pExtension->gameTime = gameTime;
	// Record and increment the packet number.
	pExtension->packetNumber = ms_PacketNumber++;
	END_PACKET_EXTENSION_WRITE((void *)(pExtension + 1), CPacketCubicPatchWaterFrame);
}


void CPacketCubicPatchWaterFrame::BuildPatch(u32 x, u32 y, u32 sizeLog2, patchAllocator &allocator, float *pHeightMap, Matrix44 &leastSquaresMatrices)
{
	WATER_PATCH leafNode;

	if(sizeLog2 == 2)
	{
		for(u32 j=0; j<4; j++)
		{
			for(u32 i=0; i<4; i++)
			{
				float h = pHeightMap[((y + j) << DYNAMICGRIDELEMENTS_LOG2) +  (x + i)];
				leafNode.patch.patchCubics[i].contolPoints[j] = Quantize<WATER_CUBIC_PATCH_STORAGE>(h, minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			}
		}

		WATER_PATCH *pLeafNode = allocator.GetNextFreePatch();
		*pLeafNode = leafNode;
	}
	else
	{
		FitLeastSquaresPatch(leafNode.patch, x, y, sizeLog2, pHeightMap, leastSquaresMatrices);
		WATER_PATCH *pLeafNode = allocator.GetNextFreePatch();
		*pLeafNode = leafNode;
	}
}


void CPacketCubicPatchWaterFrame::FitLeastSquaresPatch(CUBIC_PATCH < WATER_CUBIC_PATCH_STORAGE > &patchQuantized, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, Matrix44 &leastSquaredMatrix) const
{
	u32 ix = x << 12;
	u32 delta_ix = (((0x1 << sizeLog2) - 1) << 12)/3;
	float delta_alpha = 1.0f/(float)((0x1 << sizeLog2) - 1);

	for(u32 column=0; column<4; column++)
	{
		u32 iy = y << 12;
		float alpha = 0.0f;
		Vector4 v = Vector4(0.0f, 0.0f, 0.0f, 0.0f);

		for(u32 row=0; row<(0x1 << sizeLog2); row++)
		{
			float h = GetInterpolatedHeight(ix, iy, pHeightMap);
			float alphaSquared = alpha*alpha;
			float alphaCubed = alphaSquared*alpha;

			v.x += alphaCubed*h;
			v.y += alphaSquared*h;
			v.z += alpha*h;
			v.w += 1.0f*h;
			// Move down a row.
			iy += (1 << 12);
			alpha += delta_alpha;
		}
		Vector4 cubic;
		leastSquaredMatrix.Transform(v, cubic);

		alpha = 0.0f;

		// Convert the cubic into "control points" by evaluating it at the control points.
		for(u32 k=0; k<4; k++)
		{
			float alphaSquared = alpha*alpha;
			float alphaCubed = alphaSquared*alpha;
			float hStar = cubic.x*alphaCubed + cubic.y*alphaSquared + cubic.z*alpha + cubic.w;
			// Output the quantized patch.
			patchQuantized.patchCubics[column].contolPoints[k] = QuantizeU8(hStar, minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			//patch.patchCubics[column].contolPoints[k] = DeQuantizeU8(patchQuantized.patchCubics[column].contolPoints[k], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			alpha += 1.0f/3.0f;
		}

		// Move across a column.
		ix += delta_ix;
	}
}


float CPacketCubicPatchWaterFrame::GetInterpolatedHeight(u32 x, u32 y, float *pHeightMap) const
{
	u32 ix = x >> 12;
	u32 iy = y >> 12;
	float fx = (float)(x & ((0x1 << 12) - 1))/(float)(0x1 << 12);
	float fy = (float)(y & ((0x1 << 12) - 1))/(float)(0x1 << 12);

	int cornerUpper = (iy << DYNAMICGRIDELEMENTS_LOG2) + ix;
	int cornerLower = ((iy + 1) << DYNAMICGRIDELEMENTS_LOG2) + ix;

// 	replayAssertf(cornerLower >= 0 && cornerLower < (DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS), "Corner Lower is out of bounds %u (x: %u, y: %u)", cornerLower, x, y);
// 	replayAssertf(cornerUpper >= 0 && cornerUpper < (DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS), "Corner Upper is out of bounds %u (x: %u, y: %u)", cornerUpper, x, y);
	if(cornerLower >= 0 && cornerLower < (DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS) &&
		cornerUpper >= 0 && cornerUpper < (DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS))
	{
		float *pCornerUpper = &pHeightMap[cornerUpper];
		float *pCornerLower = &pHeightMap[cornerLower];

		float x0y0 = pCornerUpper[0];
		float x1y0 = pCornerUpper[1];
		float x0y1 = pCornerLower[0];
		float x1y1 = pCornerLower[1];

		return (1.0f - fx)*(1.0f - fy)*x0y0 + fx * (1.0f - fy)*x1y0 + (1.0f - fx)*fy*x0y1 + fx * fy*x1y1;
	}
	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////
Matrix44 &CPacketCubicPatchWaterFrame::GetLeastSquaresMatrix(u32 sizeLog2) const
{
	if(ms_LeastSquaresMatrixBuilt == false)
	{
		BuildLeastSquaresMatrix(sizeLog2, ms_LeastSquaresMatrix);
		ms_LeastSquaresMatrixBuilt = true;
	}
	return ms_LeastSquaresMatrix;
}

void CPacketCubicPatchWaterFrame::BuildLeastSquaresMatrix(u32 sizeLog2, Matrix44 &out) const
{
	float x = 0.0f;
	float delta = 1.0f/((0x1 << sizeLog2) - 1);
	float M[4][4]; 
	sysMemSet(&M[0][0], 0, sizeof(float)*4*4);

	for(u32 i=0; i<(0x1<< sizeLog2); i++)
	{
		float xSquared = x*x;
		float xCubed = xSquared*x;

		// A						B								C						D.
		M[0][0] += xCubed*xCubed;	M[0][1] += xSquared*xCubed;		M[0][2] += x*xCubed;	M[0][3] += 1.0f*xCubed;		// dE/dA
		M[1][0] += xCubed*xSquared;	M[1][1] += xSquared*xSquared;	M[1][2] += x*xSquared;	M[1][3] += 1.0f*xSquared;	// dE/dB
		M[2][0] += xCubed*x;		M[2][1] += xSquared*x;			M[2][2] += x*x;			M[2][3] += 1.0f*x;			// dE/dC
		M[3][0] += xCubed*1.0f;		M[3][1] += xSquared*1.0f;		M[3][2] += x*1.0f;		M[3][3] += 1.0f*1.0f;		// dE/dD.

		// Move onto the next data point.
		x += delta;
	}

	out.a.x = M[0][0]; out.b.x = M[0][1]; out.c.x = M[0][2]; out.d.x = M[0][3];
	out.a.y = M[1][0]; out.b.y = M[1][1]; out.c.y = M[1][2]; out.d.y = M[1][3];
	out.a.z = M[2][0]; out.b.z = M[2][1]; out.c.z = M[2][2]; out.d.z = M[2][3];
	out.a.w = M[3][0]; out.b.w = M[3][1]; out.c.w = M[3][2]; out.d.w = M[3][3];
	out.Inverse();
}


//////////////////////////////////////////////////////////////////////////

#define WATER_PACKET_ALLOW_DEBUG_PATTERN 0

#if WATER_PACKET_ALLOW_DEBUG_PATTERN

float g_DoDebugPattern = 0.0f;
float g_DebugValue = 0.0f;

float OddEvenForDebugPattern(u32 x, u32 y)
{
	u32 bit = 0;
	bit += ((x >> 3) & 0x1);
	bit += ((y >> 3) & 0x1);

	if(bit & 0x1)
		return -1.0f;
	return g_DebugValue;
}

#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

/****************************************************************************************************************************/
/* Packet level extract functions.																							*/
/****************************************************************************************************************************/
void CPacketCubicPatchWaterFrame::Extract(float interpValue, CPacketCubicPatchWaterFrame const *pNextPacket, float *pDest) const
{
#if WATER_PACKET_ALLOW_DEBUG_PATTERN
	g_DebugValue = (((float)m_StartX * 8.0f) + (float)m_StartY)/64.0f;
#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

	// Extract the previous packet.
	ExtractPatches(pDest, GetControlPointsToCubic(), false, 0.0f);

#if WATER_PACKET_ALLOW_DEBUG_PATTERN
	if(g_DoDebugPattern != 0)
		return;
#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

	// Interpolate into next packet.
	if(pNextPacket)
	{
		if(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) == 0)
		{
			// We can only interpolate if the grid positions relate to the same points in space.
			if((pNextPacket->m_StartX == m_StartX) && (pNextPacket->m_StartY == m_StartY))
				pNextPacket->ExtractPatches(pDest, GetControlPointsToCubic(), true, interpValue);
		}
		else
		{
			WATER_FRAME_PACKET_EXTENSION *pExtension = (WATER_FRAME_PACKET_EXTENSION *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketCubicPatchWaterFrame);
			pNextPacket->ExtractPatchesWithGridShiftAndInterpolation(pDest, GetControlPointsToCubic(), interpValue, pExtension->gridBaseX, pExtension->gridBaseY);
		}
	}
}


//----------------------------------------------------------------------------------//
void CPacketCubicPatchWaterFrame::ExtractWithGridShift(float interpValue, CPacketCubicPatchWaterFrame const *pNextPacket, float *pDest, u16 gridX, u16 gridY) const
{
#if WATER_PACKET_ALLOW_DEBUG_PATTERN
	g_DebugValue = (((float)m_StartX * 8.0f) + (float)m_StartY)/64.0f;
#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

	// Extract the previous packet.
	ExtractPatchesWithGridShift(pDest, GetControlPointsToCubic(), gridX, gridY);

#if WATER_PACKET_ALLOW_DEBUG_PATTERN
	if(g_DoDebugPattern != 0)
		return;
#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

	// Interpolate into next packet.
	if(pNextPacket)
		pNextPacket->ExtractPatchesWithGridShiftAndInterpolation(pDest, GetControlPointsToCubic(), interpValue, gridX, gridY);
}


/****************************************************************************************************************************/
/* Patch array functions.																									*/
/****************************************************************************************************************************/
void CPacketCubicPatchWaterFrame::ExtractPatches(float *pHeightData, Matrix44 &controlPointToCubic, bool interpolate, float interp) const
{
	if(m_PatchCount == 0)
		return;

	WATER_PATCH *pPatches = GetTreeLeafNodes();

	for(u32 x = 0; x < m_PatchCount; ++x)
	{
		for(u32 y = 0; y < m_PatchCount; ++y)
		{
			if(interpolate)
			{
				RenderPatchWithInterpolation(pPatches++, m_StartX + (x << m_PatchSizeLog2), m_StartY + (y << m_PatchSizeLog2), m_PatchSizeLog2, pHeightData, interp, controlPointToCubic);
			}
			else
			{
				RenderPatch(pPatches++, m_StartX + (x << m_PatchSizeLog2), m_StartY + (y << m_PatchSizeLog2), m_PatchSizeLog2, pHeightData, controlPointToCubic);
			}
		}
	}
}


//----------------------------------------------------------------------------------//
void CPacketCubicPatchWaterFrame::ExtractPatchesWithGridShift(float *pHeightData, Matrix44 &controlPointToCubic, u16 currBaseX, u16 currBaseY) const
{
	if(m_PatchCount == 0)
		return;

	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) != 0);
	WATER_FRAME_PACKET_EXTENSION *pExtension =  (WATER_FRAME_PACKET_EXTENSION *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketCubicPatchWaterFrame);

	WATER_PATCH *pPatches = GetTreeLeafNodes();

	s32 upperBound = (0x1 << DYNAMICGRIDELEMENTS_LOG2) - (0x1 << WATER_CUBIC_PATCH_SIZE_LOG_2);

	for(u32 x = 0; x < m_PatchCount; ++x)
	{
		for(u32 y = 0; y < m_PatchCount; ++y)
		{
			s32 sX = m_StartX + (x << m_PatchSizeLog2) + pExtension->gridBaseX - currBaseX;
			s32 sY = m_StartY + (y << m_PatchSizeLog2) + pExtension->gridBaseY - currBaseY;

			if((sX >= 0) && (sY >= 0) && (sX <= upperBound) && (sY <= upperBound))
			{
				RenderPatch(pPatches, (u32)sX, (u32)sY, m_PatchSizeLog2, pHeightData, controlPointToCubic);
			}
			pPatches++;
		}
	}
}


//----------------------------------------------------------------------------------//
void CPacketCubicPatchWaterFrame::ExtractPatchesWithGridShiftAndInterpolation(float *pHeightData, Matrix44 &controlPointToCubic, float interp, u16 currBaseX, u16 currBaseY) const
{
	if(m_PatchCount == 0)
		return;

	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) != 0);
	WATER_FRAME_PACKET_EXTENSION *pExtension =  (WATER_FRAME_PACKET_EXTENSION *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketCubicPatchWaterFrame);

	WATER_PATCH *pPatches = GetTreeLeafNodes();

	s32 upperBound = (0x1 << DYNAMICGRIDELEMENTS_LOG2) - (0x1 << WATER_CUBIC_PATCH_SIZE_LOG_2);

	for(u32 x = 0; x < m_PatchCount; ++x)
	{
		for(u32 y = 0; y < m_PatchCount; ++y)
		{
			s32 sX = m_StartX + (x << m_PatchSizeLog2) + pExtension->gridBaseX - currBaseX;
			s32 sY = m_StartY + (y << m_PatchSizeLog2) + pExtension->gridBaseY - currBaseY;

			if((sX >= 0) && (sY >= 0) && (sX <= upperBound) && (sY <= upperBound))
			{
				RenderPatchWithInterpolation(pPatches, (u32)sX, (u32)sY, m_PatchSizeLog2, pHeightData, interp, controlPointToCubic);
			}
			pPatches++;
		}
	}
}


/****************************************************************************************************************************/
/* Patch rendering functions.																								*/
/****************************************************************************************************************************/
void CPacketCubicPatchWaterFrame::RenderPatch(WATER_PATCH *pNode, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, Matrix44 &controlPointsToCubic) const
{
	float *pH = &pHeightMap[x + (y << DYNAMICGRIDELEMENTS_LOG2)];

#if WATER_PACKET_ALLOW_DEBUG_PATTERN
	u16 gridX, gridY;
	GetGridXAndYFromExtensions(gridX, gridY);
	float debugPattern = OddEvenForDebugPattern(x - m_StartX + gridX, y - m_StartY + gridY);
#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

	if(sizeLog2 == 2)
	{
		for(u32 i=0; i<4; i++)
		{
			pH[0] = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[0].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH[1] = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[1].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH[2] = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[2].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH[3] = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[3].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH += (0x1 << DYNAMICGRIDELEMENTS_LOG2);
		}
	}
	else
	{
		Vector4 cubics[4];

		for(u32 i=0; i<4; i++)
		{
			Vector4 points;
			points.x = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[0], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			points.y = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[1], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			points.z = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[2], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			points.w = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[3], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			controlPointsToCubic.Transform(points, cubics[i]);
		}

		float alpha = 0.0f;
		float delta = 1.0f/(float)((0x1 << sizeLog2) - 1);

		// Alpha/i goes "down".
		for(u32 i=0; i<(0x1 << sizeLog2); i++)
		{
			float alphaSquared = alpha*alpha;
			float AlphaCubed = alphaSquared*alpha;
			Vector4 alphaV  = Vector4(AlphaCubed, alphaSquared, alpha, 1.0f);
			Vector4 alphaControlPoints = Vector4(alphaV.Dot(cubics[0]), alphaV.Dot(cubics[1]), alphaV.Dot(cubics[2]), alphaV.Dot(cubics[3]));

			Vector4 cubicAlongBeta;
			controlPointsToCubic.Transform(alphaControlPoints, cubicAlongBeta);

			float beta = 0.0f;
			float *pInnerH = pH;

			// Beta/j goes "across",
			for(u32 j=0; j<(0x1 << sizeLog2); j++)
			{
				float betaSquared = beta*beta;
				float betaCubed = betaSquared*beta;
				Vector4 BetaV  = Vector4(betaCubed, betaSquared, beta, 1.0f);

				float patchH = BetaV.Dot(cubicAlongBeta);
			#if WATER_PACKET_ALLOW_DEBUG_PATTERN
				patchH = (1.0f - g_DoDebugPattern)*patchH + g_DoDebugPattern*debugPattern;
			#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN
				*pInnerH++ = patchH;

				// Move across in beta.
				beta += delta;
			}
			// Move down in alpha.
			pH += (0x1 << DYNAMICGRIDELEMENTS_LOG2);
			alpha += delta;
		}
	}
}


//----------------------------------------------------------------------------------//
void CPacketCubicPatchWaterFrame::RenderPatchWithInterpolation(WATER_PATCH *pNode, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, float interp, Matrix44 &controlPointsToCubic) const
{
	float OneMinusInterp = 1.0f - interp;
	float *pH = &pHeightMap[x + (y << DYNAMICGRIDELEMENTS_LOG2)];

#if WATER_PACKET_ALLOW_DEBUG_PATTERN
	u16 gridX, gridY;
	GetGridXAndYFromExtensions(gridX, gridY);
	float debugPattern = OddEvenForDebugPattern(x - m_StartX + gridX, y - m_StartY + gridY);
#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN

	if(sizeLog2 == 2)
	{
		for(u32 i=0; i<4; i++)
		{
			pH[0] = OneMinusInterp*pH[0] + interp*DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[0].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH[1] = OneMinusInterp*pH[1] + interp*DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[1].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH[2] = OneMinusInterp*pH[2] + interp*DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[2].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH[3] = OneMinusInterp*pH[3] + interp*DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[3].contolPoints[i], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			pH += (0x1 << DYNAMICGRIDELEMENTS_LOG2);
		}
	}
	else
	{
		Vector4 cubics[4];

		for(u32 i=0; i<4; i++)
		{
			Vector4 points;
			points.x = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[0], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			points.y = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[1], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			points.z = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[2], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			points.w = DeQuantize<WATER_CUBIC_PATCH_STORAGE>(pNode->patch.patchCubics[i].contolPoints[3], minQuantizedRange, maxQuantizedRange, WATER_CUBIC_PATCH_STORAGE_NUM_BITS);
			controlPointsToCubic.Transform(points, cubics[i]);
		}

		float alpha = 0.0f;
		float delta = 1.0f/(float)((0x1 << sizeLog2) - 1);

		for(u32 i=0; i<(0x1 << sizeLog2); i++)
		{
			float alphaSquared = alpha*alpha;
			float AlphaCubed = alphaSquared*alpha;
			Vector4 alphaV  = Vector4(AlphaCubed, alphaSquared, alpha, 1.0f);
			Vector4 alphaControlPoints = Vector4(alphaV.Dot(cubics[0]), alphaV.Dot(cubics[1]), alphaV.Dot(cubics[2]), alphaV.Dot(cubics[3]));

			Vector4 cubicAlongBeta;
			controlPointsToCubic.Transform(alphaControlPoints, cubicAlongBeta);

			float beta = 0.0f;
			float *pInnerH = pH;

			for(u32 j=0; j<(0x1 << sizeLog2); j++)
			{
				float betaSquared = beta*beta;
				float betaCubed = betaSquared*beta;
				Vector4 BetaV  = Vector4(betaCubed, betaSquared, beta, 1.0f);

				float patchH = BetaV.Dot(cubicAlongBeta);
			#if WATER_PACKET_ALLOW_DEBUG_PATTERN
				patchH = (1.0f - g_DoDebugPattern)*patchH + g_DoDebugPattern*debugPattern;
			#endif // WATER_PACKET_ALLOW_DEBUG_PATTERN
				pInnerH[0] = pInnerH[0]*OneMinusInterp + interp*patchH;
				pInnerH++;

				// Move across in beta.
				beta += delta;
			}
			// Move down in alpha.
			pH += (0x1 << DYNAMICGRIDELEMENTS_LOG2);
			alpha += delta;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
Matrix44 &CPacketCubicPatchWaterFrame::GetControlPointsToCubic() const
{
	if(ms_ControlPointsToCubicBuilt == false)
	{
		BuildControlPointsToCubicMatrix(ms_ControlPointsToCubic);
		ms_ControlPointsToCubicBuilt = true;
	}
	return ms_ControlPointsToCubic;
}


void CPacketCubicPatchWaterFrame::BuildControlPointsToCubicMatrix(Matrix44 &controlPointsToCubic) const
{
	// Set each row to be x^3, x^2, x^1, 1 for x=0, 1/3, 2/3, 1.
	float x0 = 0.0f;
	float x1 = 1.0f/3.0f;
	float x2 = 2.0f/3.0f;
	float x3 = 1.0f;

	controlPointsToCubic.a = Vector4(x0*x0*x0,	x1*x1*x1,	x2*x2*x2,	x3*x3*x3);
	controlPointsToCubic.b = Vector4(x0*x0,		x1*x1,		x2*x2,		x3*x3);
	controlPointsToCubic.c = Vector4(x0,		x1,			x2,			x3);
	controlPointsToCubic.d = Vector4(1.0f,		1.0f,		1.0f,		1.0f);

	// Thus form the matrix we can use to solve for cubic coefficients A, B, C, D given 4 control points (x0, x1, x2, x3).
	controlPointsToCubic.Inverse();
}


//////////////////////////////////////////////////////////////////////////
void CPacketCubicPatchWaterFrame::GetGridXAndYFromExtensions(u16 &gridX, u16 &gridY) const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) != 0)
	{
		WATER_FRAME_PACKET_EXTENSION *pExtension = (WATER_FRAME_PACKET_EXTENSION *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketCubicPatchWaterFrame);
		gridX = pExtension->gridBaseX;
		gridY = pExtension->gridBaseY;
	}
	else
	{
		gridX = 0;
		gridY = 0;
	}
}


bool CPacketCubicPatchWaterFrame::HasPacketSkippingInfo() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) >= g_CPacketCubicPatchWaterFrame_Extensions_V2)
	{
		return true;
	}
	return false;
}


u32 CPacketCubicPatchWaterFrame::GetGameTime() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) >= g_CPacketCubicPatchWaterFrame_Extensions_V2)
	{
		WATER_FRAME_PACKET_EXTENSION *pExtension = (WATER_FRAME_PACKET_EXTENSION *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketCubicPatchWaterFrame);
		return pExtension->gameTime;
	}
	return 0;
}


u32 CPacketCubicPatchWaterFrame::GetPacketNumber() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketCubicPatchWaterFrame) >= g_CPacketCubicPatchWaterFrame_Extensions_V2)
	{
		WATER_FRAME_PACKET_EXTENSION *pExtension = (WATER_FRAME_PACKET_EXTENSION *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketCubicPatchWaterFrame);
		return pExtension->packetNumber;
	}
	return 0;
}


void CPacketCubicPatchWaterFrame::ComputeStartXAndY(Vector3 &centrePos, u16 &startX, u16 &startY, u16 &gridBaseX, u16 &gridBaseY)
{
	s32 cameraRelX		= ((s32)floor(centrePos.x/DYNAMICGRIDSIZE))*DYNAMICGRIDSIZE;
	s32 cameraRelY		= ((s32)floor(centrePos.y/DYNAMICGRIDSIZE))*DYNAMICGRIDSIZE;
	s32 cameraRangeMinX = cameraRelX - DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
	s32 cameraRangeMinY	= cameraRelY - DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
	s32 worldBaseX		= cameraRangeMinX;
	s32 worldBaseY		= cameraRangeMinY;

	s32 gridWorldBaseX = worldBaseX/DYNAMICGRIDSIZE;
	s32 gridWorldBaseY = worldBaseY/DYNAMICGRIDSIZE;

	// We store patches aligned to world-space patch widths.
	startX = (0x1 << WATER_CUBIC_PATCH_SIZE_LOG_2) - (gridWorldBaseX & ((0x1 << WATER_CUBIC_PATCH_SIZE_LOG_2) - 1));
	startY = (0x1 << WATER_CUBIC_PATCH_SIZE_LOG_2) - (gridWorldBaseY & ((0x1 << WATER_CUBIC_PATCH_SIZE_LOG_2) - 1));
	gridBaseX = (u16)gridWorldBaseX;
	gridBaseY = (u16)gridWorldBaseY;
}


//////////////////////////////////////////////////////////////////////////
#if REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
float *CPacketCubicPatchWaterFrame::GetWaterHeightMapTextureLockBase(int idx) 
{ 
	replayAssert(idx < 3); 
	return (float *)ms_WaterHeightTextureLockBase[idx]; 
}

grcTexture *CPacketCubicPatchWaterFrame::GetWaterHeightMapTexture(void *pLockBase) 
{ 
	for(u32 i=0; i<3; i++)
	{
		if(pLockBase == ms_WaterHeightTextureLockBase[i])
			return ms_WaterHeightTexture[i];
	}
	return NULL;
}

grcRenderTarget *CPacketCubicPatchWaterFrame::GetWaterHeightMapRenderTarget(void *pLockBase) 
{ 
	for(u32 i=0; i<3; i++)
	{
		if(pLockBase == ms_WaterHeightTextureLockBase[i])
			return ms_WaterHeightRenderTarget[i];
	}
	return NULL;
}
#endif // REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

//////////////////////////////////////////////////////////////////////////
void CPacketWaterSimulate::Store()
{
	PACKET_EXTENSION_RESET(CPacketWaterSimulate);
	CPacketBase::Store(PACKETID_WATERSIMULATE, sizeof(CPacketWaterSimulate));
	CPacketInterp::Store();

	replayDebugf2("Recording water pos");

	const CSecondaryWaterTune &secTunings = Water::GetSecondaryWaterTunings();

	m_RippleBumpiness			= secTunings.m_RippleBumpiness;
	m_RippleMinBumpiness		= secTunings.m_RippleMinBumpiness;
	m_RippleMaxBumpiness		= secTunings.m_RippleMaxBumpiness;
	m_RippleBumpinessWindScale	= secTunings.m_RippleBumpinessWindScale;
	m_RippleSpeed				= secTunings.m_RippleSpeed;
	m_RippleDisturb				= secTunings.m_RippleDisturb;
	m_RippleVelocityTransfer	= secTunings.m_RippleVelocityTransfer;
	m_OceanBumpiness			= secTunings.m_OceanBumpiness;
	m_DeepOceanScale			= secTunings.m_DeepOceanScale;
	m_OceanNoiseMinAmplitude	= secTunings.m_OceanNoiseMinAmplitude;
	m_OceanWaveAmplitude		= secTunings.m_OceanWaveAmplitude;
	m_ShoreWaveAmplitude		= secTunings.m_ShoreWaveAmplitude;
	m_OceanWaveWindScale		= secTunings.m_OceanWaveWindScale;
	m_ShoreWaveWindScale		= secTunings.m_ShoreWaveWindScale;
	m_OceanWaveMinAmplitude		= secTunings.m_OceanWaveMinAmplitude;
	m_ShoreWaveMinAmplitude		= secTunings.m_ShoreWaveMinAmplitude;
	m_OceanWaveMaxAmplitude		= secTunings.m_OceanWaveMaxAmplitude;
	m_ShoreWaveMaxAmplitude		= secTunings.m_ShoreWaveMaxAmplitude;
	m_OceanFoamIntensity		= secTunings.m_OceanFoamIntensity;
	m_CurrentWaterRand			= Water::GetWaterRand();
	m_ReplayWaterPos.Store(Water::GetCurrentWaterPos());
}

//////////////////////////////////////////////////////////////////////////
void CPacketWaterSimulate::Extract() const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketWaterSimulate) == 0);
	CSecondaryWaterTune secTunings;
	secTunings.m_RippleBumpiness		= m_RippleBumpiness;
	secTunings.m_RippleMinBumpiness		= m_RippleMinBumpiness;
	secTunings.m_RippleMaxBumpiness		= m_RippleMaxBumpiness;
	secTunings.m_RippleBumpinessWindScale	= m_RippleBumpinessWindScale;
	secTunings.m_RippleSpeed			= m_RippleSpeed;
	secTunings.m_RippleDisturb			= m_RippleDisturb;
	secTunings.m_RippleVelocityTransfer	= m_RippleVelocityTransfer;
	secTunings.m_OceanBumpiness			= m_OceanBumpiness;
	secTunings.m_DeepOceanScale			= m_DeepOceanScale;
	secTunings.m_OceanNoiseMinAmplitude	= m_OceanNoiseMinAmplitude;
	secTunings.m_OceanWaveAmplitude		= m_OceanWaveAmplitude;
	secTunings.m_ShoreWaveAmplitude		= m_ShoreWaveAmplitude;
	secTunings.m_OceanWaveWindScale		= m_OceanWaveWindScale;
	secTunings.m_ShoreWaveWindScale		= m_ShoreWaveWindScale;
	secTunings.m_OceanWaveMinAmplitude	= m_OceanWaveMinAmplitude;
	secTunings.m_ShoreWaveMinAmplitude	= m_ShoreWaveMinAmplitude;
	secTunings.m_OceanWaveMaxAmplitude	= m_OceanWaveMaxAmplitude;
	secTunings.m_ShoreWaveMaxAmplitude	= m_ShoreWaveMaxAmplitude;
	secTunings.m_OceanFoamIntensity		= m_OceanFoamIntensity;
	Water::SetSecondaryWaterTunings(secTunings);
	
	Vector3		currentPos;

	m_ReplayWaterPos.Load(currentPos);
	Water::SetCurrentWaterPos(currentPos);
	Water::SetCurrentWaterRand(m_CurrentWaterRand);

#if REPLAY_WATER_ENABLE_DEBUG
	#define WATERGRIDSIZE (2)

	s32 worldBaseX	= (((s32)floor(currentPos.x/WATERGRIDSIZE))*WATERGRIDSIZE) - DYNAMICGRIDELEMENTS;
	s32 worldBaseY = (((s32)floor(currentPos.y/WATERGRIDSIZE))*WATERGRIDSIZE) - DYNAMICGRIDELEMENTS;

	static char debugString[256];
	sprintf(debugString, "[RPL] POS %f %f WB %i %i", currentPos.GetX(), currentPos.GetY(), worldBaseX, worldBaseY);
	grcDebugDraw::Text(Vec2V(0.01f, 0.49f), Color32(0.0f, 0.0f, 0.0f), debugString, true, 1.5f);
#endif
}

//////////////////////////////////////////////////////////////////////////
void WaterSimInterp::Init(const ReplayController& controller, CPacketWaterSimulate const* pPrevPacket)
{
	// Setup the previous packet
	SetPrevPacket(pPrevPacket);

	m_sPrevFullPacketSize += pPrevPacket->GetPacketSize();

	// Look for/Setup the next packet
	if (pPrevPacket->HasNextOffset())
	{
		CPacketWaterSimulate const* pNextPacket = NULL;
		controller.GetNextPacket(pPrevPacket, pNextPacket);
		SetNextPacket(pNextPacket);

		m_sNextFullPacketSize += pNextPacket->GetPacketSize();
	}
	else
	{
		replayAssertf(false, "CInterpolator::Init");
	}
}

CPacketWaterFoam::CPacketWaterFoam(float worldX, float worldY, float amount)
: CPacketEvent(PACKETID_WATERFOAM, sizeof(CPacketWaterFoam))
{
	Vector3 posAmount(worldX, worldY, amount);
	m_PosAmount.Store(posAmount);
}

void CPacketWaterFoam::Extract(const CEventInfo<void>&) const
{
	Vector3 posAmount;
	m_PosAmount.Load(posAmount);

	Water::AddFoam(posAmount.x, posAmount.y, posAmount.z);
}

#endif