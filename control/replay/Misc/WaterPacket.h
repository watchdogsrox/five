#ifndef  WATER_PACKET_H
#define  WATER_PACKET_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/Misc/LinkedPacket.h"
#include "control/replay/Misc/InterpPacket.h"
#include "control/replay/ReplayController.h"

#include "renderer/waterdefines.h"
#include "renderer/Water.h"

#define WATER_CUBIC_PATCH_STORAGE u8
#define REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY	((0x1 && RSG_DURANGO) || (0x1 && RSG_ORBIS))

class CPacketCubicPatchWaterFrame : /*public CPacketBase,*/ public CPacketLinked<CPacketCubicPatchWaterFrame>
{
private:
	template < class T >
	struct CUBIC_CONTROL_POINTS
	{
		T contolPoints[4];
	};

	template < class T >
	struct CUBIC_PATCH
	{
		CUBIC_CONTROL_POINTS < T > patchCubics[4];
	};

	typedef struct WATER_PATCH
	{
		CUBIC_PATCH <WATER_CUBIC_PATCH_STORAGE> patch;
	} WATER_PATCH;

	class patchAllocator
	{
	public:
		patchAllocator(WATER_PATCH *pPatches)
		{
			m_NoOfPatches = 0;
			m_pPatches = pPatches;
		}
		~patchAllocator()
		{
		}

		WATER_PATCH *GetNextFreePatch()
		{
			return &m_pPatches[m_NoOfPatches++];
		}
		u32 GetNoOfPatches()
		{
			return m_NoOfPatches;
		}
	private:
		u32 m_NoOfPatches;
		WATER_PATCH *m_pPatches;
	};

public:
	CPacketCubicPatchWaterFrame()
	: CPacketLinked(PACKETID_CUBIC_PATCH_WATERFRAME, sizeof(CPacketCubicPatchWaterFrame))
	{}

	void	Store(u32 gameTime);
	void	StoreExtensions(u32 gameTime);
	void	BuildPatch(u32 x, u32 y, u32 size, patchAllocator &allocator, float *pHeightMap, Matrix44 &leastSquaresMatrices);
	void	FitLeastSquaresPatch(CUBIC_PATCH < WATER_CUBIC_PATCH_STORAGE > &patchQuantized, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, Matrix44 &leastSquaredMatrix) const;
	Matrix44 &GetLeastSquaresMatrix(u32 sizeLog2) const;
	void	BuildLeastSquaresMatrix(u32 N, Matrix44 &out) const;
	bool	DoesCubicPatchFitHeightMap(CUBIC_PATCH < float > &patch, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, Matrix44 &coefficientMatrix);
	float	GetInterpolatedHeight(u32 x, u32 y, float *pHeightMap) const;

	// Packet level extract functions.
	void	Extract(float interpValue, CPacketCubicPatchWaterFrame const *pNextPacket, float *pDest) const;
	void	ExtractWithGridShift(float interpValue, CPacketCubicPatchWaterFrame const *pNextPacket, float *pDest, u16 gridX, u16 gridY) const;

	// Patch array functions.
	void	ExtractPatches(float *pHeightData, Matrix44 &controlPointToCubic, bool interpolate, float interp) const;
	void	ExtractPatchesWithGridShift(float *pHeightData, Matrix44 &controlPointToCubic, u16 currBaseX, u16 currBaseY) const;
	void	ExtractPatchesWithGridShiftAndInterpolation(float *pHeightData, Matrix44 &controlPointToCubic, float interp, u16 currBaseX, u16 currBaseY) const;

	// Patch rendering functions.
	void	RenderPatch(WATER_PATCH *pNode, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, Matrix44 &controlPointsToCubic) const;
	void	RenderPatchWithInterpolation(WATER_PATCH *pNode, u32 x, u32 y, u32 sizeLog2, float *pHeightMap, float interp, Matrix44 &controlPointsToCubic) const;

	Matrix44 &GetControlPointsToCubic() const;
	void	BuildControlPointsToCubicMatrix(Matrix44 &controlPointsToCubic) const;

	void	GetGridXAndYFromExtensions(u16 &gridX, u16 &gridY) const;
	bool	HasPacketSkippingInfo() const;
	u32		GetGameTime() const;
	u32		GetPacketNumber() const;
	static void ComputeStartXAndY(Vector3 &centrePos, u16 &startX, u16 &startY, u16 &gridBaseX, u16 &gridBaseY);

	WATER_PATCH *GetTreeLeafNodes() const
	{
		return (WATER_PATCH *)((u8 *)this + GetPacketSize() - sizeof(WATER_PATCH)*m_PatchCount*m_PatchCount - GET_PACKET_EXTENSION_SIZE(CPacketCubicPatchWaterFrame));
	}

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_CUBIC_PATCH_WATERFRAME, "Validation of CPacketWaterFrame Failed!, %d", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketCubicPatchWaterFrame), "Validation of CPacketCubicPatchWaterFrame extensions Failed!, 0x%08X", GetPacketID());
		return GetPacketID() == PACKETID_CUBIC_PATCH_WATERFRAME && CPacketBase::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketCubicPatchWaterFrame);
	}

#if !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	static float *GetWaterHeightBuffer(u32 idx) { return &ms_WaterHeightBuffer[idx][0][0]; }
#else // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	static void SetWaterHeightMapTextureAndLockBasePtr(int idx, void *pPtr, grcTexture *pTexture, grcRenderTarget *pRenderTarget) { replayAssert(idx < 3); ms_WaterHeightTextureLockBase[idx] = pPtr; ms_WaterHeightTexture[idx] = pTexture; ms_WaterHeightRenderTarget[idx] = pRenderTarget; }
	static float *GetWaterHeightMapTextureLockBase(int idx);
	static grcTexture *GetWaterHeightMapTexture(void *pLockBase);
	static grcRenderTarget *GetWaterHeightMapRenderTarget(void *pLockBase);
	static float *GetWaterHeightTempWorkBuffer() { return &ms_WaterHeightBufferToCompress[0][0]; }
	static u32 GetNextBufferIndex() { u32 ret = ms_NextBufferIndex; ms_NextBufferIndex = (ms_NextBufferIndex + 1) % 3; return ret; }
#endif // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

private:
	typedef struct WATER_FRAME_PACKET_EXTENSION
	{
		// Version 1.
		u16 gridBaseX;
		u16 gridBaseY;
		// Added version 2.
		u32 gameTime;
		u32 packetNumber;
	} WATER_FRAME_PACKET_EXTENSION;

private:
	u16 m_PatchCount;
	u16 m_PatchSizeLog2;
	u16 m_StartX, m_StartY;
	DECLARE_PACKET_EXTENSION(CPacketCubicPatchWaterFrame);

	static bool ms_LeastSquaresMatrixBuilt;
	static bool ms_ControlPointsToCubicBuilt;
	static Matrix44 ms_LeastSquaresMatrix;
	static Matrix44 ms_ControlPointsToCubic;
	static u32 ms_PacketNumber;

#if !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	static float ms_WaterHeightBuffer[2][DYNAMICGRIDELEMENTS][DYNAMICGRIDELEMENTS];
#else // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	static u32 ms_NextBufferIndex;
	static void *ms_WaterHeightTextureLockBase[3];
	static grcTexture *ms_WaterHeightTexture[3];
	static grcRenderTarget *ms_WaterHeightRenderTarget[3];
	static float ms_WaterHeightBufferToCompress[DYNAMICGRIDELEMENTS][DYNAMICGRIDELEMENTS];
#endif // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
};

//////////////////////////////////////////////////////////////////////////
class CPacketWaterSimulate : public CPacketBase, public CPacketInterp
{
public:
	CPacketWaterSimulate()
		: CPacketBase(PACKETID_WATERSIMULATE, sizeof(CPacketWaterSimulate))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketWaterSimulate); }
	void	Extract() const;

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_WATERSIMULATE, "Validation of CPacketWaterSimulate Failed!, %d", GetPacketID());

		return GetPacketID() == PACKETID_WATERSIMULATE && CPacketBase::ValidatePacket();
	}

	float	m_RippleBumpiness;
	float	m_RippleMinBumpiness;
	float	m_RippleMaxBumpiness;
	float	m_RippleBumpinessWindScale;
	float	m_RippleSpeed;
	float	m_RippleDisturb;
	float	m_RippleVelocityTransfer;
	float	m_OceanBumpiness;
	float	m_DeepOceanScale;
	float	m_OceanNoiseMinAmplitude;
	float	m_OceanWaveAmplitude;
	float	m_ShoreWaveAmplitude;
	float	m_OceanWaveWindScale;
	float	m_ShoreWaveWindScale;
	float	m_OceanWaveMinAmplitude;
	float	m_ShoreWaveMinAmplitude;
	float	m_OceanWaveMaxAmplitude;
	float	m_ShoreWaveMaxAmplitude;
	float	m_OceanFoamIntensity;
	float	m_CurrentWaterRand;

	CPacketVector3	m_ReplayWaterPos;
	DECLARE_PACKET_EXTENSION(CPacketWaterSimulate);
};

//////////////////////////////////////////////////////////////////////////
class WaterSimInterp : public CInterpolator<CPacketWaterSimulate>
{
public:
	WaterSimInterp() { Reset(); }

	void Init(const ReplayController& controller, CPacketWaterSimulate const* pPrevPacket);

	void Reset()
	{
		CInterpolator<CPacketWaterSimulate>::Reset();

		m_sPrevFullPacketSize = m_sNextFullPacketSize = 0;
	}

	s32	GetPrevFullPacketSize() const	{ return m_sPrevFullPacketSize; }
	s32	GetNextFullPacketSize() const	{ return m_sNextFullPacketSize; }

protected:
	s32							m_sPrevFullPacketSize;
	s32							m_sNextFullPacketSize;
};

class CPacketWaterFoam : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketWaterFoam(float worldX, float worldY, float amount);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const  { return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const { return PREPLAY_SUCCESSFUL; }
	void				PrintXML(FileHandle handle) const {	CPacketEvent::PrintXML(handle); }

	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_WATERFOAM, "Validation of CPacketWaterFoam Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_WATERFOAM;
	}

	CPacketVector3 m_PosAmount;
};

#endif // GTA_REPLAY
#endif // WATER_PACKET_H