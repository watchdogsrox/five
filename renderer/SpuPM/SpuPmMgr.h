//
//
//
//
#ifndef __SPU_PM_MGR__
#define __SPU_PM_MGR__

#define SPUPMMGR_ENABLED		(1 && (__PPU||__SPU))

#define SPUPMMGR_PPU			(SPUPMMGR_ENABLED && __PPU)
#define SPUPMMGR_SPU			(SPUPMMGR_ENABLED && __SPU)
#define SPUPMMGR_PS3			(SPUPMMGR_ENABLED && (__SPU||__PPU))

#define SPUPMMGR_TEST			(SPUPMMGR_ENABLED && SPUPMMGR_PPU && __DEV)	// simple test

#if __PPU
	// PS3 headers
	#include <cell/atomic.h>
	#include <cell/spurs/types.h>
#endif

// Rage headers
#include "system/memops.h"
#include "bank/bank.h"
#include "vector/vector4.h"

// game headers
#if __PPU
	#include "renderer/render_channel.h"
	#include "renderer/DrawLists/drawListMgr.h"
	#define CSPULIGHTCLASSIFICATIONINFO_DEF_STRUCT
		#include "renderer/Lights/TiledLightingSettings.h"
	#include "renderer/lights/LightSource.h"
	#include "renderer/Deferred/DeferredConfig.h"
#endif

//
//
//
//
#if SPUPMMGR_TEST
	void SpuPM_ProcessTestTexture();
	void SpuPM_RenderTestTexture();
#else
	inline void SpuPM_ProcessTestTexture()	{ }
	inline void SpuPM_RenderTestTexture()	{ }
#endif


//
// Class to represent information about a parsed ELF file created by the GtaStripSPUELF tool.
//
class CElfInfo
{
public:
	unsigned int m_LSDestination;	// Destination SPU local store address for SPU ELF.
	unsigned int m_imageSize;		// Total size of loadable section inside SPU ELF.
	unsigned int m_entry;			// Program entry point of SPU ELF.
	unsigned int m_preProcessEntry; // Program entry point for the preProcess function for a PModifier SPU ELF.
};

#ifndef ASSERT16
#define ASSERT16(P)		Assert((u32(P)&0x0F)==0)
#endif
#ifndef ASSERT128
#define ASSERT128(P)	Assert((u32(P)&0x7F)==0)
#endif


#if __PPU || __SPU

#define PM_MAX_WORK_QUEUE_JOBS		(512)

// Description:
// Class in the job queue for the SPU drivers to grab and execute.
class ALIGNAS(16)	CSPUJobQueueEntry
{
public:
	void*			m_jobHandlerEA;     // Effective address of the driver to load.
	u32				m_jobHandlerSize;   // Driver size.
	u32				m_jobHandlerEntry;  // Entry pointer to the job handler.
	void*			m_infoPacketEA;		// Effective address of the job information packet.

	u32				m_infoPacketSize;	// Size of the job information packet.
	void*			m_jobValidSwitch;	// Effective address of the switch used to make this job valid. The unsigned int at this address must be greater than or equal to the valid value.
	u32				m_jobValidValue;	// The value that the job valid switch must be greater than or equal to for the job to be valid.
	u32				m_pad0;

// NOTE: sizeof(this) can't be more than 128 bytes! 
#define CSPUJOBQUEUEENTRY_MAX_SIZEOF	(128)
} ;
CompileTimeAssert(sizeof(CSPUJobQueueEntry)	<= CSPUJOBQUEUEENTRY_MAX_SIZEOF);

// Description:
// State information for the fill and empty pointers of the SPU job queue.
class BEGIN_ALIGNED(128) CSPUJobQueuePos
{
public:
	u32					m_lockObtained;	// Indicates whether the consumer state is locked.
	u32					m_elementSize;	// Alignment padding.
	void*				m_base;			// Base of job queue.
	void*				m_top;			// Top of job queue.
	volatile void*		m_curr;			// Current position in job queue.

// NOTE: sizeof(this) can't be more than 128 bytes! 
#define CSPUJOBQUEUEPOS_MAX_SIZEOF		(128)
} ALIGNED(128);
CompileTimeAssert(sizeof(CSPUJobQueuePos) <= CSPUJOBQUEUEPOS_MAX_SIZEOF);

// Description:
// Pair of job queue positions for implementing a push/pull pair.
class BEGIN_ALIGNED(128) CSPUJobQueuePushPullPos
{
public:
	CSPUJobQueuePos		m_push;			// Queue position that jobs are pushed to. Read and updated by the PPU.
	CSPUJobQueuePos		m_pull;			// Queue position that jobs are pulled from. Read and updated by the SPU.

	CSPUJobQueueEntry	m_entries[PM_MAX_WORK_QUEUE_JOBS];	// Queue of jobs.

	unsigned int		m_maxSPUs;		// Maximum number of SPUs for this workload.


public:
	// Determine if the job queue is empty.
	bool isEmpty() const
	{
		return m_push.m_curr == m_pull.m_curr;
	}

#if __PPU
	void initializeJobQueue(unsigned int maxSPUs);
#endif

} ALIGNED(128);



//
//
//
//
struct ALIGNAS(16) CSPUBaseDriverInfo
{
public:
	u32		m_refCount;			// refCounter
	void	*m_userDataEA;		// Effective address of user data block in main memory.
	u32		m_userDataSize;		// Immediate user data transferred in info packet.
	u32		m_userData1;

	void*	m_gateDecrAddr[4];	// gate: decrement gate (usually refCount on allocated PPU work buffers)

	void*	m_gateAddr;			// gate: Addresses to which to write on completion of processing, NULL for none.
	u32		m_gateValue;		// gate: Data to write to corresponding gateAddr on completion of processing (usually JTS)
//	u32		m_pad1;
//	u32		m_pad2;
} ;

//
//
//
//
struct ALIGNAS(16) CSPUPostProcessDriverInfo : public CSPUBaseDriverInfo
{
public:
	void*	m_sourceData;			// Effective address of the source texture data.
	u32		m_sourceStride;			// Stride of the source texture data.
	u16		m_sourceWidth;			// Width of the source data.
	u16		m_sourceHeight;			// Height of the source data.
	u16		m_sourceBpp;			// BytesPerPixel of the source data.
	u8		m_tileX, m_tileY;		// current tileX & tileY;

	void*	m_sourceDataPrev;		// EA of source texture data from prev block
	void*	m_sourceDataNext;		// EA of source texture data from next block

	void*	m_destData;				// Effective address of the destination texture data.
	u32		m_destStride;			// Stride of the destination texture data.
	u16		m_destWidth;			// Width of the destination data.
	u16		m_destHeight;			// Height of the destination data.
	u16		m_destBpp;				// BytesPerPixel of the destination data.
	u16		m_sourceY;				// Source Y offset that we're starting at;
	
	// shadows v2 test: store shadUVs+projZ+reflectionUVs for fullscreen process:
	void*	m_mrtData[4];
	u32		m_mrtStride[4];			// Stride of the mrt texture data.
	u16		m_mrtWidth[4];			// Width of the mrt data.
	u16		m_mrtHeight[4];			// Height of the mrt data.
	u16		m_mrtBpp[4];			// BytesPerPixel of the mrt data.

public:
#if __PPU
	void	ZeroAll()					{ sysMemSet(this, 0x00, sizeof(CSPUPostProcessDriverInfo)); }
#endif //__PPU
} ;

//
//
//
//
struct CSPUDefLight
{
	enum
	{
		FLAG_SPU_RENDERABLE = (1<<0),	// true=SPU renderable, false=RSX renderable; see LIGHTFLAG_SPU_RENDERABLE
		FLAG_SPU_SPECULAR	= (1<<1),
		FLAG_SPU_INTERIOR	= (1<<2),
		FLAG_SPU_EXTERIOR	= (1<<3),
		FLAG_FREE4			= (1<<4),
		FLAG_FREE5			= (1<<5),
		FLAG_FREE6			= (1<<6),
		FLAG_FREE7			= (1<<7)
	};

	float	m_position[3];			// 12			
	float	m_direction[3];			// 24
	float	m_colour[3];			// 36
	float	m_invSqrRadius;			// 40
	float	m_coneScale;			// 44
	float	m_coneOffset;			// 48
	float	m_clippingPlane[4];		// 64
	float	m_falloffExponent;		// 68
	float	m_capsuleExtents;		// 72
	u8		m_lightPass;			// 73
	u8		m_flags;				// 74
	u8		m_lightType;			// 75
	u8		m_lightIndex;			// 76
	u32		pad_u32[1];				// 80
};

//
//
//
//
#ifdef CSPULIGHTCLASSIFICATIONINFO_DEF_STRUCT
struct ALIGNAS(16) CSPULightClassificationInfo
{
	enum { MAX_LIGHTS=256 };

	Float16*		 m_depthTexturePtr;
	float*			 m_subTileData;
	float*			 m_tileData;
	u32				 m_padding;

	s16				 m_tileStartIndex;
	s16				 m_tileEndIndex;
	s16				 m_subTileStartIndex;
	s16				 m_subTileEndIndex;

	// m_viewport:
	float			 m_vpWidth;
	float			 m_vpHeight;
	float			 m_vpTanHFOV;
	float			 m_vpTanVFOV;

	Mat34V			 m_vpCameraMtx;

	atFixedBitSet<MAX_LIGHTS>*	m_subTileActiveLights;
	atFixedBitSet<MAX_LIGHTS>*	m_tileActiveLights;
	u32				m_timeStamp;
	u32*			m_timeStampDesc;
	LightStats*		m_lightStatsDesc[4];
	u64*			m_lightStatsCoverage;
	u32*			m_lightBalancerTab;
	u16				m_lightBalancerTileLimit;			// global 16x16 tile limit
	u16				m_lightBalancerMaxLightTileLimit;	// max 16x16 tile limit per light

	LightData        m_lightData;
} ;
#endif //CSPULIGHTCLASSIFICATIONINFO_DEF_STRUCT...

#endif //__PPU || __SPU...

enum { SPUPMMGR_MAX_NUM_SPU = 5};

#if SPUPMMGR_ENABLED
//
//
//
//
struct ALIGNAS(16) CJtsBlocker
{
enum { maxNumJTS = 4 };	// max 4 JTSes supported

	u32		m_refCount;				// SPU & destTexture refCounter
	u32*	m_ReturnFromJTSblock;	// address within m_JTS[] to jump back into Rage's CB
	u32		m_pad0;
	u32		m_pad1;

	// it must be aligned to 16 bytes:
	u32		m_JTS[24+4] ;// JTS blockers (max. 4) + clearing commands + jump back

	// -------------------------------------------------------------------------------------
	// keep RSX refCount outside above 128-byte cacheline:
	u32		m_refCountRsx;			// RSX refCounter
	u32		m_pad2[3];

#if __PPU
public:
	// clear all:
	void	ZeroAll()					{ sysMemSet(this, 0x00, sizeof(CJtsBlocker)); }
	void*	SetupJTS(u32 num);
	void*	CallJTS();
#endif //__PPU...
} ;
CompileTimeAssert(sizeof(CJtsBlocker)==(128+16));


//
//
// info header to track down all allocations in BigWorkBuffer;
// particular block will be free'd when refCount==0;
//
struct CWBAllocBlockHeader
{
	u32		m_refCount;			// refCounter (if 0, then block will be free'd)
	void*	m_allocatedPtr;		// ptr to allocated buffer
};

#if __PPU
#include "grcore\texturegcm.h"

//
//
// "grcTexture with jtsBlocker":
// - simple implementation of destination texture for results of SPU postprocessing:
// 
//
struct grcSpuDestTexture
{
	grcSpuDestTexture() : m_Texture(NULL), m_jtsBlocker(NULL)		{}


public:
	grcTexture*		GetTexture()						{	return m_Texture;	}
	void			SetTexture(grcTexture	 *tex)		{	m_Texture=tex;		}

	void BindJtsBlocker(CJtsBlocker *jtsBlocker, bool bCallOldBlocker=FALSE);
	void CallJtsBlocker();

	// destTex: it should be called when destTex is about to be used:
	void WaitForSPU()
	{
		this->CallJtsBlocker();
	}

private:
	grcTexture		*m_Texture;
	CJtsBlocker		*m_jtsBlocker;		// synchronizing jtsBlocker
};
#endif //__PPU...


#define SPUPM_LIGHTBALANCER_CMD_BUF_SIZE		(4)		// 4 words total: 2 words for 1st part (jump over|jump back to main cmdbuf), 2 words for 2nd part (nop|jump back to main cmdbuf)

#if __PPU

#include "templates\LinkList.h"
#include "system\simpleallocator.h"

typedef CLink<CSPUPostProcessDriverInfo>	CSPUPostProcessDriverInfoLink;
typedef CLink<CWBAllocBlockHeader>			CWBAllocBlockHeaderLink;
typedef CLink<CJtsBlocker>					CJtsBlockerLink;


extern const CElfInfo spupmProcessSPU[];	// test SPU processing job
extern const CElfInfo spupmSSAOSPU[];
extern const CElfInfo spupmLightClassificationSPU[];
extern const CElfInfo spupmRainUpdateSPU[];


//
//
//
//
class CSpuPmMgr
{
enum { PM_MAX_NUM_SPU = SPUPMMGR_MAX_NUM_SPU};

public:
	static bool Init();
	static bool Shutdown();
	static bool Update();

#if __BANK
	static bool InitWidgets(bkBank& bank);
#endif

#if __DEV
	static void ReloadElf();
	static CElfInfo* GetReloadedElf(const u32 slot);
#endif

	// src is grcTextureGCM*
	inline
	static bool PostProcessTexture(const CElfInfo *spuJobElf, u32 numSpuThreads,
					u32 *split,
					void *customInputDataPtr, u32 customInputDataSize,
					grcSpuDestTexture *destSpuTex, grcTextureGCM* srcTex,
					u16 opStartX, u16 opStartY, u16 opWidth, u16 opHeight,
					u16 opBytesPerPixel
					#if ENABLE_PIX_TAGGING
					,const char *tagMarkerName
					#endif
					)
	{
		CellGcmTexture *cellSrcTex0 = (CellGcmTexture*)srcTex->GetTexturePtr();
		Assert(cellSrcTex0);

		return PostProcessTexture(spuJobElf, numSpuThreads, split, customInputDataPtr, customInputDataSize,
			destSpuTex, cellSrcTex0,
			opStartX, opStartY, opWidth, opHeight, opBytesPerPixel
			#if ENABLE_PIX_TAGGING
			,tagMarkerName
			#endif
			);
	}

	// src is grcRenderTargetGCM*
	inline
	static bool PostProcessTexture(const CElfInfo *spuJobElf, u32 numSpuThreads, u32* split,
					void *customInputDataPtr, u32 customInputDataSize,
					grcSpuDestTexture *destSpuTex, grcRenderTargetGCM* srcRT,
					u16 opStartX, u16 opStartY, u16 opWidth, u16 opHeight,
					u16 opBytesPerPixel
					#if ENABLE_PIX_TAGGING
					,const char *tagMarkerName
					#endif
					)
	{
		CellGcmTexture *cellSrcTex0 = (CellGcmTexture*)srcRT->GetTexturePtr();
		Assert(cellSrcTex0);

		return PostProcessTexture(spuJobElf, numSpuThreads, split, customInputDataPtr, customInputDataSize,
									destSpuTex, cellSrcTex0,
									opStartX, opStartY, opWidth, opHeight, opBytesPerPixel
									#if ENABLE_PIX_TAGGING
									,tagMarkerName
									#endif
									);
	}

	// src is CellGcmTexture*
	static bool PostProcessTexture(const CElfInfo *spuJobElf, u32 numSpuThreads,
					u32 *split,
					void *customInputDataPtr, u32 customInputDataSize,
					grcSpuDestTexture *destSpuTex, CellGcmTexture		*srcCellTex,
					u16 opStartX, u16 opStartY, u16 opWidth, u16 opHeight,
					u16 opBytesPerPixel
					#if ENABLE_PIX_TAGGING
					,const char *tagMarkerName
					#endif
					);

private:
	// queues:
	static void enqueueJob(const CElfInfo *jobElfInfo, void* jobPacket, u32 jobPacketSize,
						volatile u32 *conditionSwitch, u32 conditionValue);

	static inline bool compareAndSwap(u32 *value, u32 oldVal, u32 newVal)
	{
		return (cellAtomicCompareAndSwap32(value, oldVal, newVal) == oldVal);
	}

	// lists handling:
	static CWBAllocBlockHeader*			AllocateWorkBufferForSPU(u32 size, bool bCleanAllocList=FALSE);
	static void							CleanList_WorkBufferForSPU();
	static CSPUPostProcessDriverInfo*	AllocateSPUPostProcessDriverInfo(bool bCleanAllocList=FALSE);
	static void							CleanList_SPUPostProcessDriverInfo();
	static CJtsBlocker*					AllocateJtsBlocker(bool bCleanAllocList=FALSE);
	static void							CleanList_JtsBlocker();

	// RSX fences:
	static bool							GetRsxFence(volatile u32 **fenceAddr, u32 *fenceValue);


	// SPURS workload handling:
	static bool createWorkload(CellSpurs *pSpurs);
	static bool wakeupWorkload(CellSpurs *pSpurs, u32 workloadID);
	static bool suspendCurrentWorkload(CellSpurs *pSpurs);

	// SPURS workload:
	static u32						sm_currentWorkloadID;
	static u32						PM_args[8] ALIGNED(128);

	// lists:
	static CLinkList<CWBAllocBlockHeader>			gbWBAllocList;		// list of allocated buffers in BigWorkBuffer:
	static CLinkList<CSPUPostProcessDriverInfo>		gbPostProcessDriverInfoList;
	static CLinkList<CJtsBlocker>					gbJtsBlockerList;

	// job queues:
	static CSPUJobQueuePushPullPos					ms_jobQueueStandard;
	static CSPUJobQueuePushPullPos					ms_jobQueueGPU;

	// RSX wait labels:
	static u32				rsxWaitTextureLabelIndex;
	static volatile u32*	rsxWaitTextureLabelPtr;
	static u32				sm_waitTextureValue;

	// RSX fences:
	static u32				sm_gpuFenceValue;
	static u32*				sm_gpuFencePtr;
	static u32				sm_gpuFenceOffset;

	// workload flag:
	static u32				sm_workloadFlagIndex;

#if __DEV
	#define RELOADED_ELF_MAX_SIZE (128*1024)	// 128KB should be enough for everybody :)
	static CElfInfo* spupmReloadedElf;
	static u32 reloadSlot;
	static bool bReloadElfBuf;
	static u8*  pReloadElf[2];
#endif
};
#endif //__PPU...

#else	// SPUPMMGR_ENABLED...
//
//
//
//
class CSpuPmMgr
{
public:
	static bool Init()						{ return(true);	}
	static bool Shutdown()					{ return(true);	}
	static bool Update()					{ return(true);	}
#if __BANK
	static bool InitWidgets(bkBank&)		{ return(true);	}
#endif
};
#endif	// SPUPMMGR_ENABLED...
#endif //__SPU_PM_MGR__...

