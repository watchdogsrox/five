//
// PlantsGrassRendererSPU.cpp - main body of SPU job for plants rendering;
//
//	30/04/2007	-	Andrzej:	- initial;
//	22/06/2007	-	Andrzej:	- v2.0 master job system; sphere test for individual plants added;
//	26/06/2007	-	Andrzej:	- 4 buffers supported, parametrization of bufferID;
//	26/06/2007	-	Andrzej:	- 4 ppu buffers and 2 spu buffers (for double-buffered dma) supported;
//	14/11/2008	-	Andrzej:	- 1.0: revitalised old SPU renderloop (w/vertex processing);
//	18/11/2008	-	Andrzej:	- SpuRenderloop 2.0: pure commandbuffer builder, no vertex processing, still using old synchronization;
//	26/11/2008	-	Andrzej:	- SpuRenderloop 2.0: one linear big buffer used for output (doublebuffered);
//	10/12/2008	-	Andrzej:	- SpuRenderloop 2.0: output buffer size optimization
//									(using CALLs instead of copying full sub-buffers for BindShaders, Textures and Geometries);
//
//
//
//
//
#if __SPU

//#define SPU_DEBUGF(X)				X
#define SPU_DEBUGF(X)
//#define SPU_DEBUGF1(X)			X
#define SPU_DEBUGF1(X)				

//
// private tagID to use with all DMA stuff here:
//
#define PLANTS_DMATAGID				(20)	// main dma tag
#define PLANTS_DMATAGID1			(21)	// local heap context0
#define PLANTS_DMATAGID2			(22)	// local heap context1
#define PLANTS_DMATAGID3			(23)	// triPlantTab0
#define PLANTS_DMATAGID4			(24)	// triPlantTab1

// useful shortcut for debug prints, etc.
#define SPUTASKID					(inMasterGrassParams->m_rsxLabel5_CurrentTaskID)


#include <cell/gcm_spu.h>
using namespace cell::Gcm;
#include "math/intrinsics.h"
#include "grcore/gcmringbuffer.h"
#include "vector/vector3_consts_spu.cpp"
#include "vector/vector3.h"
#include "vector/matrix34.h"
#include "vector/color32.h"
#include "vector/color32.cpp"
#include "vector/geometry.h"
#include "vector/geometry.cpp"
#include "math/float16.h"
#include "phcore/materialmgr.h"
#include "physics/inst.h"
#include "atl/array.h"
#include "atl/bitset.h"
#include "math/random.cpp"

#include <string.h>
#include <spu_printf.h>
#include <math.h>

#undef	SYS_DMA_VALIDATION
#undef  SYS_DMA_VERBOSE
#define SYS_DMA_VALIDATION	(1)	// 1 -	General checks on valid alignment, size and DMA source and  destination addresses for all DMA commands. 
#define SYS_DMA_VERBOSE		(1)
#include "system/dma.h"


// SPU helper headers:
#include "../basetypes.h"
#include "fwmaths/RandomSPU.h"						// fwRandom()...
#include "fwtl/StepAllocator.h"

#include "../Objects/ProceduralInfo.h"				// PROCPLANT_xxx
#include "../Renderer/PlantsMgr.h"
#include "../Renderer/PlantsGrassRenderer.h"		// PSN_PLANTSMGR_SPU_RENDER, struct PPTriPlant{}
CPLANTLOCTRI_PV_MAP_ARRAYS							// CPlantLocTri::ms_pvMap/DensityScaleZ
#include "../shader_source/Vegetation/Grass/grass_regs.h"



#if PSN_PLANTSMGR_SPU_RENDER
static spuGrassParamsStructMaster	*inMasterGrassParams;
static spuGrassParamsStructMaster	*outMasterGrassParams;

#define IN_GRASS_PARAMS_TAB_SIZE	(PLANTSMGR_MAX_NUM_OF_RENDER_JOBS*sizeof(spuGrassParamsStruct))
#define IN_GRASS_PARAMS_TAB_ALIGN	(128)
spuGrassParamsStruct* inGrassParamsTab=NULL;

#define nNumGrassJobsLaunched	(inMasterGrassParams->m_nNumGrassJobsLaunched)

#define globalLOD0FarDist2		(inMasterGrassParams->m_LOD0FarDist2)

#define globalLOD1CloseDist2	(inMasterGrassParams->m_LOD1CloseDist2)
#define globalLOD1FarDist2		(inMasterGrassParams->m_LOD1FarDist2)

#define globalLOD2CloseDist2	(inMasterGrassParams->m_LOD2CloseDist2)
#define globalLOD2FarDist2		(inMasterGrassParams->m_LOD2FarDist2)

#define globalWindBending		(inMasterGrassParams->m_windBending)

#if __BANK
	#define GRCDEVICE_GetFrameCounter		(inMasterGrassParams->m_DeviceFrameCounter)
	#define bGeomCullingTestEnabled			(inMasterGrassParams->m_bGeomCullingTestEnabled)
	#define bPremultiplyNdotL				(inMasterGrassParams->m_bPremultiplyNdotL)
	#define gbPlantsFlashLOD0				(inMasterGrassParams->m_bPlantsFlashLOD0)
	#define gbPlantsFlashLOD1				(inMasterGrassParams->m_bPlantsFlashLOD1)
	#define gbPlantsFlashLOD2				(inMasterGrassParams->m_bPlantsFlashLOD2)
	#define gbPlantsDisableLOD0				(inMasterGrassParams->m_bPlantsDisableLOD0)
	#define gbPlantsDisableLOD1				(inMasterGrassParams->m_bPlantsDisableLOD1)
	#define gbPlantsDisableLOD2				(inMasterGrassParams->m_bPlantsDisableLOD2)
	#define pLocTriPlantsLOD0DrawnPerSlot	(&outMasterGrassParams->m_LocTriPlantsLOD0DrawnPerSlot)
	#define pLocTriPlantsLOD1DrawnPerSlot	(&outMasterGrassParams->m_LocTriPlantsLOD1DrawnPerSlot)
	#define pLocTriPlantsLOD2DrawnPerSlot	(&outMasterGrassParams->m_LocTriPlantsLOD2DrawnPerSlot)
#else
	#define bGeomCullingTestEnabled			(TRUE)
	#define bPremultiplyNdotL				(TRUE)
#endif	//__BANK...


static  u8					_gCameraPos[1*sizeof(Vector3)];
#define globalCameraPos		(*((Vector3*)_gCameraPos))

#define gCullFrustum		(inMasterGrassParams->m_cullFrustum)

#define LOCAL_TRI_PLANT_TAB_SIZE		(PPTRIPLANT_BUFFER_SIZE*sizeof(PPTriPlant))
#define LOCAL_TRI_PLANT_TAB_ALIGN		(128)
PPTriPlant *_localTriPlantTab0=NULL;
PPTriPlant *_localTriPlantTab1=NULL;
PPTriPlant*	localTriPlantTab[2] = {0};
u32	triPlantTabDmaTagID[2] = {0};
u32 triPlantTabBufferID=0;



u8 _localPlantModelsSlot0[CPLANT_SLOT_NUM_MODELS*2*sizeof(grassModel)]	ALIGNED(128);
#define localPlantModelsSlot0	((grassModel*)&_localPlantModelsSlot0[0])
grassModel	*localPlantModelsTab;

#if CPLANT_DYNAMIC_CULL_SPHERES
	PlantMgr_DynCullSphereArray localDynCullSpheres;
#endif

// RSX offset to LOD2 geometry:
#define		GeometryLOD2Offset			(inMasterGrassParams->m_GeometryLOD2Offset)

// RSX offsets to arrays of texture commands (16 textures per 1 slot):
#define		PlantTexturesSlotOffset0	(inMasterGrassParams->m_PlantTexturesOffsetTab)
u32			*localPlantTexturesTab;


#define BindShaderLOD0cmdOffset		(inMasterGrassParams->m_BindShaderLOD0Offset)	// RSX offset to call BindShaderLOD0
#define BindShaderLOD1cmdOffset		(inMasterGrassParams->m_BindShaderLOD1Offset)	// RSX offset to call BindShaderLOD1
#define BindShaderLOD2cmdOffset		(inMasterGrassParams->m_BindShaderLOD2Offset)	// RSX offset to call BindShaderLOD2


u8 _g_PlantsRendRand[sizeof(mthRandom)];
#define g_PlantsRendRand	(*((mthRandom*)_g_PlantsRendRand))

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
#define HEAP_BLOCK_ARRAY_SIZE		(PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE*sizeof(grassHeapBlock))
#define HEAP_BLOCK_ARRAY_ALIGN		(128)
grassHeapBlock* HeapBlockArray=NULL;
#define HeapBlockArrayCount		(inMasterGrassParams->m_heapBlockArrayCount)
u32 HeapCurrentBlock		= 0;	// current block in big heap array
#else
u32	eaHeapBegin				= 0;	// big buffer ea
u32	heapBeginOffset			= 0;	// big buffer offset
u32	eaHeapBeginCurrent		= 0;	// current dest ea in big buffer
u32	heapBeginOffsetCurrent	= 0;	// big buffer offset
#define heapSize				(inMasterGrassParams->m_heapSize)
#endif

// amount of bytes big heap is too small to fit all commands we want
#define eaHeapOverfilled		(outMasterGrassParams->m_AmountOfBigHeapOverfilled)


u32	spuBufferID				= 0;	// 0-1: current internal SPU buffer (localHeap)

#define LOCAL_SOLID_HEAP_SIZE		(PLANTSMGR_LOCAL_HEAP_SIZE)
#define LOCAL_SOLID_HEAP_ALIGN		(128)
u32 *_localSolidHeap0=0;
u32 *_localSolidHeap1=0;
u32	*localSolidHeap[2] = {0};

u32	localDmaTagID[2] = {0};



u32	eaRsxLabel5;
u32	rsxLabel5Index;

// values of rsx labels used by dma get/put:
u32	rsxLabel5 ALIGNED(128) =-1;


inline void SetupContextData(CellGcmContextData *context, u32 *addr, u32 size, u32 numExtraWordsAtEnd, CellGcmContextCallback callback)
{
	context->begin		= (uint32_t*)addr;
	context->current	= (uint32_t*)addr;
	context->end		= (uint32_t*)addr + (size/sizeof(uint32_t) - numExtraWordsAtEnd);	// leave n words at the end
	context->callback	= callback;
}

#define SPU_IABS(A)		(((A)>0)?(A):(-A))


u32 _context[8] ALIGNED(128) ={0};
//
//
//
//
static void spuClearJTSinBuffer(u32 offsetJTS, u32 where2JumpOffset)
{
	CellGcmContext con;
	SetupContextData(&con, &_context[0], sizeof(_context), 1, NULL);

	if(where2JumpOffset)
		con.SetJumpCommand(where2JumpOffset);	// insert jump
	else
		con.SetNopCommand(1);					// insert NOP

	sysDmaSmallPut(&_context[0], offsetJTS, 4, PLANTS_DMATAGID);
	sysDmaWaitTagStatusAll(1 << PLANTS_DMATAGID);

	//	SPU_DEBUGF(spu_printf("\n SPU %X: spuClearJTSinBuffer(%X, %X)", SPUTASKID, offsetJTS, where2JumpOffset));
}


#if 0
static void spuSleep(u32 v)
{
	u32 v0 = spu_read_decrementer();
	while(1)
	{
		u32 v1 = spu_read_decrementer();
		// if decrementer overflows, then (v0-v1) will be really big number (so spuSleep() will exit immediately)
		// otherwise it works as expected:
		if(/*SPU_IABS*/(v0-v1) > v)
			break;
	}
}
#endif //#if 0...

//
//
//
//
#if 0
static void spuWaitForLabel(u32 *pRsxLabel, u32 eaRsxLabel, u32 expectedValue)
{
	*(pRsxLabel) = -1;
	sysDmaSmallGet(pRsxLabel, eaRsxLabel, 4, PLANTS_DMATAGID);
	sysDmaWaitTagStatusAll(1 << PLANTS_DMATAGID);

	//	SPU_DEBUGF(spu_printf("\n SPU %X: spuWaitForLabel start: %X, value=%X, expected=%X", SPUTASKID, eaRsxLabel, *pRsxLabel, expectedValue));

	while(*(pRsxLabel) != expectedValue)
	{
		spuSleep(100);	//10
		sysDmaSmallGet(pRsxLabel, eaRsxLabel, 4, PLANTS_DMATAGID);
		sysDmaWaitTagStatusAll(1 << PLANTS_DMATAGID);
	}

	//	SPU_DEBUGF(spu_printf("\n SPU %X: spuWaitForLabel end: %X", SPUTASKID, eaRsxLabel));
}
#endif //#if 0...

//
//
// copies given set of commands into context:
//
#if 0
static void CopyLocalCmdsToContext(CellGcmContext *ctx, u32 *cmd, u32 cmdSize)
{
	CellGcmContextData *conData = (CellGcmContextData*)ctx;

	// check if enough space for copying commands:
	if((conData->current + cmdSize) >= conData->end)
	{
		conData->callback(conData, 0);
	}

	for(u32 i=0; i<cmdSize; i++)
	{
		*(conData->current++) = *(cmd++);
	}
}
#endif //#if 0...



//
//
//
//
const u32 nCGRFExtraWords = 4;	// num of extra words before end of custom context
static bool bCustomGcmReserveFailedProlog	= FALSE;	// prolog flag
static bool bCustomGcmReserveFailedEpilog	= FALSE;	// epilog flag
static bool bHeapLastBlock					= FALSE;	// true, if adding commands to last segment
static int32_t CustomGcmReserveFailed(CellGcmContextData *context, uint32_t /*count*/)
{
//	SPU_DEBUGF(spu_printf("\n SPU %X: kicking commands: bufID=%d, context: begin: %X, current: %X, end: %X", SPUTASKID, bufferID, (u32)context->begin, (u32)context->current, (u32)context->end));

CellGcmContext *curCon		= (CellGcmContext*)context;

	if(bHeapLastBlock)
	{
		// still space for at least 256 words left?
		if((context->end-context->current) > 256)
		{
			return(CELL_OK);
		}
		bHeapLastBlock = false;
	}


	curCon->end += nCGRFExtraWords;	// we still have some space (32 words) before end of real memory buffer...

	if(bCustomGcmReserveFailedEpilog)
	{	// epilog: nothing to dma, only wait for prev buffer dma
		bCustomGcmReserveFailedEpilog = FALSE;
	}
	else
	{
		// jump to next segment:
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
		const u32 nextBlock = HeapCurrentBlock+1;
		if(nextBlock < HeapBlockArrayCount)
		{
			cellGcmSetJumpCommandUnsafeInline(curCon, HeapBlockArray[nextBlock].offset);
		}
		else
		{	// too small output buffer? - then jump back to main CB:
			cellGcmSetJumpCommandUnsafeInline(curCon, inMasterGrassParams->m_ReturnOffsetToMainCB);
		}
#else
		heapBeginOffsetCurrent += PLANTSMGR_LOCAL_HEAP_SIZE;
		if(heapBeginOffsetCurrent < (heapBeginOffset+heapSize))
		{
			cellGcmSetJumpCommandUnsafeInline(curCon, heapBeginOffsetCurrent);
			// fillup rest of buffer with NOPs:
			//u32 numNOPs = curCon->end - curCon->current;
			//while(numNOPs--)
			//{
			//	*(curCon->current++) = 0x00000000;	// NOP
			//}
		}
		else
		{	// too small output buffer? - then jump back to main CB:
			cellGcmSetJumpCommandUnsafeInline(curCon, inMasterGrassParams->m_ReturnOffsetToMainCB);
		}
#endif
		Assertf(curCon->current <= curCon->end, "CustomGcmReserveFailed: Internal context overfilled by %d word(s)!", u32(curCon->current-curCon->end));

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
		// start dmaing current buffer only if enough space:
		if(HeapCurrentBlock < HeapBlockArrayCount)
		{	// start copying current buffer:
			const u32 dmatagID = localDmaTagID[spuBufferID];
			sysDmaPut(localSolidHeap[spuBufferID], (u64)HeapBlockArray[HeapCurrentBlock].ea, PLANTSMGR_LOCAL_HEAP_SIZE, dmatagID);	// copy buffer
		}
		else
		{
			eaHeapOverfilled++;
		}
		
		// get to next block in big buffer array:
		HeapCurrentBlock++;

		// already in last block of big buffer?
		if(HeapCurrentBlock == (HeapBlockArrayCount-1))
		{
			bHeapLastBlock=true;
		}
#else
		// start dmaing current buffer only if enough space:
		if(eaHeapBeginCurrent < (eaHeapBegin+heapSize))
		{	// start copying current buffer:
			const u32 dmatagID = localDmaTagID[spuBufferID];
			sysDmaPut(localSolidHeap[spuBufferID], eaHeapBeginCurrent, PLANTSMGR_LOCAL_HEAP_SIZE, dmatagID);	// copy buffer
			//		SPU_DEBUGF(spu_printf("\n SPU %X: dma to eaHeapBeginCurrent=0x%X, spuBufferID=%X", SPUTASKID, eaHeapBeginCurrent, spuBufferID));
		}
		else
		{
			eaHeapOverfilled++;
			// SPU_DEBUGF(spu_printf("\n SPU %X: emergency: no dma!", SPUTASKID));
		}
		
		// increase big buffer ptr:
		eaHeapBeginCurrent += PLANTSMGR_LOCAL_HEAP_SIZE;

		// already in last segment of big buffer?
		if(eaHeapBeginCurrent == (eaHeapBegin+heapSize-PLANTSMGR_LOCAL_HEAP_SIZE))
		{
			bHeapLastBlock=true;
		}
#endif
	}


	if(bCustomGcmReserveFailedProlog)
	{	// prolog: 1st dma ever started, not necessary to wait for prev buffer's dma
		bCustomGcmReserveFailedProlog = FALSE;
	}
	else
	{
		// wait for previous buffer's dma:
		u32 prevSpuBufferID		= (spuBufferID-1)&0x00000001;
		const u32 prevDmatagID	= localDmaTagID[prevSpuBufferID];
		sysDmaWaitTagStatusAll(1 << prevDmatagID);
	}


	
	spuBufferID++;
	spuBufferID &= 0x00000001;

	// restart local context:
	SetupContextData(context, localSolidHeap[spuBufferID], PLANTSMGR_LOCAL_HEAP_SIZE, nCGRFExtraWords, &CustomGcmReserveFailed);
//	SPU_DEBUGF(spu_printf("\n SPU %X: commands kicked!", SPUTASKID));

	return(CELL_OK);
}// end of CustomGcmReserveFailed()...



//
//
// SPU version of GRCDEVICE.SetVertexShaderConstant():
//
inline void GRCDEVICE_SetVertexShaderConstant(CellGcmContext *gcmCon, int startRegister, const float *data, int regCount)
{
	gcmCon->SetVertexProgramConstants(startRegister,regCount<<2,data);
}

inline void GRCDEVICE_DrawIndexedPrimitive(CellGcmContext *gcmCon, grassModel *model)
{
	gcmCon->SetDrawIndexArray(
				model->m_DrawMode,			//modeMap[dm],
				model->m_IdxCount,			//indexCount,
				CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16,
				model->m_IdxLocation,		//gcm::IsLocalPtr(ptr)?CELL_GCM_LOCATION_LOCAL:CELL_GCM_LOCATION_MAIN,
				model->m_IdxOffset			//offset
				);
}

inline void GRCDEVICE_DrawPrimitive(CellGcmContext *gcmCon, u32 primitive, u32 startVertex, u32 vertexCount)
{
	gcmCon->SetDrawArrays(primitive, startVertex, vertexCount);
}


// 360: Leaving it disabled for now, because this doesn't help much on Xenos for some reason
//      and total GPU execution time/CB size is longer/bigger when enabled (?!?);
//      probably D3D does similar caching tricks under the hood when calling SetVertexShaderConstant();
//
#define OPTIMIZED_VS_GRASS_SETUP	(1 && (__PPU||__SPU))

//
//
//
//
struct instGrassDataDrawStruct
{
	Matrix44				m_WorldMatrix;
	Vector4					m_PlantColor;
	Vector4					m_GroundColor;

	///////////////////////////////////////////////////////
#if OPTIMIZED_VS_GRASS_SETUP
	Matrix34				m_prevWorldMatrix;
#endif

public:
	inline void Reset()		{
#if OPTIMIZED_VS_GRASS_SETUP
		m_prevWorldMatrix.a =
			m_prevWorldMatrix.b =
			m_prevWorldMatrix.c =
			m_prevWorldMatrix.d = Vector3(-1,-1,-1);
#endif //OPTIMIZED_VS_GRASS_SETUP...
	}

	inline bool UseOptimizedVsSetup(const Matrix34& m34)
	{
#if OPTIMIZED_VS_GRASS_SETUP
		const bool bDifferent =	m_prevWorldMatrix.a.IsNotEqual(m34.a) ||
			m_prevWorldMatrix.b.IsNotEqual(m34.b) ||
			m_prevWorldMatrix.c.IsNotEqual(m34.c);
		if(bDifferent)
		{
			m_prevWorldMatrix.a = m34.a;
			m_prevWorldMatrix.b = m34.b;
			m_prevWorldMatrix.c = m34.c;
			return(FALSE);
		}
		return(TRUE);
#else //OPTIMIZED_VS_GRASS_SETUP
		return(FALSE);
#endif //OPTIMIZED_VS_GRASS_SETUP
	}

	// full setup: 6 registers
	inline s32			GetRegisterCountFull() const	{ return( (sizeof(m_WorldMatrix)/16) + (sizeof(m_PlantColor)/16) + (sizeof(m_GroundColor)/16) );}
	inline const float*		GetRegisterPointerFull() const	{ return( (const float*)&m_WorldMatrix	);														}

	// optimized setup: 3 registers (only position from matrix is taken):
	inline s32			GetRegisterCountOpt() const		{ return( (sizeof(m_WorldMatrix.d)/16) + (sizeof(m_PlantColor)/16) + (sizeof(m_GroundColor)/16) );}
	inline const float*		GetRegisterPointerOpt() const	{ return( (const float*)&m_WorldMatrix.d	);														}
};


#if GRASS_LOD2_BATCHING
struct CBatcherLOD2
{
	enum	{ BatchSize=16	};	// 16 primitives = 1536 bytes
	enum	{ Float4Count=6	};	// store 6xfloat4 per primitive

private:
	Vector4				m_storage[Float4Count*BatchSize];	// 6xfloat4=96 bytes * 16 = 1536 bytes
	u32					m_count;
	CellGcmContext*		m_GcmCon;

public:
	CBatcherLOD2(CellGcmContext *gcmCon)	{ m_count=0; m_GcmCon=gcmCon; Assert(m_GcmCon);	}
	~CBatcherLOD2()							{												}


	void	Add(const Matrix34& plantMat, const Vector4& plantCol, const Vector4& groundCol)
	{
		Matrix44 m44;
		m44.FromMatrix34(plantMat);

		Vector4 *dst = &m_storage[m_count*Float4Count];
		*(dst++) = m44.a;
		*(dst++) = m44.b;
		*(dst++) = m44.c;
		*(dst++) = m44.d;
		*(dst++) = plantCol;
		*(dst++) = groundCol;
		m_count++;

		if(m_count >= BatchSize)
		{
			Flush();
		}
	}

	void	Flush();
};

void CBatcherLOD2::Flush()
{
	if(m_count > 0)
	{
		const u32 currBatchSize =	m_count*Float4Count*sizeof(Vector4)
									+ sizeof(u32)	/*jumpCommand*/
									+ 16			/*alignment*/;
		const u32 bytesLeft = ((u8*)m_GcmCon->end) - ((u8*)m_GcmCon->current);
		if(currBatchSize > bytesLeft)
		{
			if(bHeapLastBlock)
			{	// if last block and not enough space - just skip the work:
				m_count = 0;
				return;
			}

			// force gcm context to flush:
			m_GcmCon->callback(m_GcmCon, 0);
		#if __ASSERT
			const u32 bytesLeft = ((u8*)m_GcmCon->end) - ((u8*)m_GcmCon->current);
			Assertf(currBatchSize <= bytesLeft, "currBatchSize (%d) <= bytesLeft (%d)", currBatchSize, bytesLeft);	// current batch MUST fit after gcm context flush!
		#endif
		}

		const u32 beginOffsetRsx = HeapBlockArray[HeapCurrentBlock].offset;			// RSX offset to beginning of current gcm context
		const u32 currOffset  = ((u8*)m_GcmCon->current) - ((u8*)m_GcmCon->begin);	// how deep inside current context
			
		m_GcmCon->SetJumpCommand(beginOffsetRsx + currOffset + currBatchSize);			// jump over the vertex data
//		spu_printf("\nHeapCurrentBlock=%d (max: %d), beginOffset: 0x%x, currOffset: 0x%x, currBatchSize: %d", HeapCurrentBlock, HeapBlockArrayCount, beginOffset, currOffset, currBatchSize);

		const u32 dataDest = ( (u32(m_GcmCon->current)+15)&~0x0f );					// align dataDest to 16 bytes
		Assert16(dataDest);
		const u32 dataDestRsx = beginOffsetRsx + (dataDest-u32(m_GcmCon->begin));	// dataDest in RSX space

		m_GcmCon->current += (m_count*Float4Count*sizeof(Vector4)+16)>>2;			// move context current ptr

		// store raw float4's into command buffer:
		Vector4 *src = &m_storage[0];
		Vector4 *dst = (Vector4*)dataDest;
		for(u32 i=0; i<m_count; i++)
		{
			*(dst++) = *(src++);
			*(dst++) = *(src++);
			*(dst++) = *(src++);
			*(dst++) = *(src++);
			*(dst++) = *(src++);
			*(dst++) = *(src++);
		}

		// setup vertex data arrays:
		m_GcmCon->SetVertexDataArray(/*TEXCOORD1*/8+1, /*frequency*/4, /*stride*/sizeof(Vector4)*Float4Count, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, dataDestRsx + 0*sizeof(Vector4));
		m_GcmCon->SetVertexDataArray(/*TEXCOORD2*/8+2, /*frequency*/4, /*stride*/sizeof(Vector4)*Float4Count, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, dataDestRsx + 1*sizeof(Vector4));
		m_GcmCon->SetVertexDataArray(/*TEXCOORD3*/8+3, /*frequency*/4, /*stride*/sizeof(Vector4)*Float4Count, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, dataDestRsx + 2*sizeof(Vector4));
		m_GcmCon->SetVertexDataArray(/*TEXCOORD4*/8+4, /*frequency*/4, /*stride*/sizeof(Vector4)*Float4Count, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, dataDestRsx + 3*sizeof(Vector4));
		m_GcmCon->SetVertexDataArray(/*TEXCOORD5*/8+5, /*frequency*/4, /*stride*/sizeof(Vector4)*Float4Count, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, dataDestRsx + 4*sizeof(Vector4));
		m_GcmCon->SetVertexDataArray(/*TEXCOORD6*/8+6, /*frequency*/4, /*stride*/sizeof(Vector4)*Float4Count, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, dataDestRsx + 5*sizeof(Vector4));

		m_GcmCon->SetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, /*startVertex*/0, /*vertexCount*/m_count*4);

		m_count=0;
	}
}// CBatcherLOD2::Flush()...
#endif //GRASS_LOD2_BATCHING...


//
//
// process all PPTriPlants in given batch
//
static void processGrassTris(spuGrassParamsStruct *inGrassParams, PPTriPlant* triPlantTab, CellGcmContext *gcmCon)
{

// models and textures for this slot:
grassModel	*localPlantModels	= localPlantModelsTab;
u32			*localPlantTextures = localPlantTexturesTab;

#if CPLANT_DYNAMIC_CULL_SPHERES
	// cull sphere array
	const CPlantMgr::DynCullSphereArray &cullSpheres = localDynCullSpheres;
#endif

#if CPLANT_PRE_MULTIPLY_LIGHTING
	const Vector3 sunDir		= inMasterGrassParams->m_mainDirectionalLight_natAmbient.GetVector3();
	const float naturalAmbient	= inMasterGrassParams->m_mainDirectionalLight_natAmbient.w;
#endif 

instGrassDataDrawStruct	customInstDraw;
#if GRASS_LOD2_BATCHING
	CBatcherLOD2		BatcherLod2(gcmCon);
#endif
Matrix34				plantMatrix;
Vector4					plantColourf;
Vector4					fakeNormal;
Matrix34				skewingMtx;

const bool bEnableCallCaching = true;
u32	cachedShaderOffset	=u32(-1);
u32 cachedTextureOffset	=u32(-1);
u32 cachedGeometryOffset=u32(-1);

	customInstDraw.Reset();

	const u32 numTriPlants = inGrassParams->m_numTriPlant;
	for(u32 i=0; i<numTriPlants; i++)
	{
		PPTriPlant *pCurrTriPlant = &triPlantTab[i];

		const float wind_bend_scale	= pCurrTriPlant->V1.w;	// wind bending scale
		const float wind_bend_var	= pCurrTriPlant->V2.w;	// wind bending variation

		const bool bApplyGroundSkewing		= (pCurrTriPlant->skewAxisAngle.w.GetBinaryData()!=0); // technically this tests for +0 but not -0
		const bool bApplyGroundSkewingLod0	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD0);
		const bool bApplyGroundSkewingLod1	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD1);
		const bool bApplyGroundSkewingLod2	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD2);
		if(bApplyGroundSkewing)
		{
			const Vec4V unpacked_skewAxisAngle = Float16Vec4Unpack(&pCurrTriPlant->skewAxisAngle);
			skewingMtx.MakeRotateUnitAxis(RCC_VECTOR3(unpacked_skewAxisAngle), unpacked_skewAxisAngle.GetWf());
		}

		const bool bEnableCulling = pCurrTriPlant->flags.IsClear(PROCPLANT_CAMERADONOTCULL);

		float NdotL = 1.0f;
#if CPLANT_WRITE_GRASS_NORMAL || CPLANT_PRE_MULTIPLY_LIGHTING
		// set normal for whole pTriLoc:
		Vector4 groundNormal;
		groundNormal.SetX(float(pCurrTriPlant->loctri_normal[0])/127.0f);
		groundNormal.SetY(float(pCurrTriPlant->loctri_normal[1])/127.0f);
		groundNormal.SetZ(float(pCurrTriPlant->loctri_normal[2])/127.0f);
		groundNormal.SetW(0.0f);
	#if CPLANT_PRE_MULTIPLY_LIGHTING
		if(bPremultiplyNdotL)
		{
			// For pre-multiplying lighting into the color channel
			NdotL = Saturate(groundNormal.Dot(sunDir)) + naturalAmbient;
		}
	#endif
#endif

		// want same values for plant positions each time...
		g_PlantsRendRand.Reset(pCurrTriPlant->seed);


				// intensity support:  final_I = I + VarI*rand01()
				const s16 intensityVar = pCurrTriPlant->intensity_var;
				u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand.GetRanged(0.0f, 1.0f));
				intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

				Color32 col32;
				col32.SetAlpha(	pCurrTriPlant->color.GetAlpha());
	#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
				col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
				col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
				col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
	#undef CALC_COLOR_INTENSITY

				plantColourf.x = col32.GetRedf();
				plantColourf.y = col32.GetGreenf();
				plantColourf.z = col32.GetBluef();
				plantColourf.w = col32.GetAlphaf();


	//			grcTexture *pPlantTextureLOD0 = pCurrTriPlant->texture_ptr;
	//			Assert(pPlantTextureLOD0);
				u32 TextureLOD0cmdOffset	= (u32)&localPlantTextures[pCurrTriPlant->texture_id*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE];

				//SPU_DEBUGF(spu_printf("\nSPU %X: setting texture: %d", SPUTASKID, textureID));
	//			grcTexture *pPlantTextureLOD1 = pCurrTriPlant->textureLOD1_ptr;
	//			Assert(pPlantTextureLOD1);
				u32 TextureLOD1cmdOffset	= (u32)&localPlantTextures[pCurrTriPlant->textureLOD1_id*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE];

	//			grmGeometry *pGeometryLOD0		= plantModelsTab[pCurrTriPlant->model_id].m_pGeometry;
	//			const Vector4 boundSphereLOD0	= plantModelsTab[pCurrTriPlant->model_id].m_BoundingSphere; 
				const u32 modelID			= (u32)pCurrTriPlant->model_id;
				grassModel *pPlantModelLOD0		= &localPlantModels[modelID];
				u32 GeometryLOD0Offset		= pPlantModelLOD0->m_GeometryCmdOffset;
				const Vector4 boundSphereLOD0	= pPlantModelLOD0->m_BoundingSphere;
			
	//			grmGeometry *pGeometryLOD1		= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_pGeometry;
	//			const Vector4 boundSphereLOD1	= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_BoundingSphere; 
				grassModel *pPlantModelLOD1		= &localPlantModels[modelID+CPLANT_SLOT_NUM_MODELS];
				u32 GeometryLOD1Offset		= pPlantModelLOD1->m_GeometryCmdOffset;
				const Vector4 boundSphereLOD1	= pPlantModelLOD1->m_BoundingSphere;

//				const Vector2 GeometryDimLOD2	= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_dimensionLOD2;
				const Vector2 GeometryDimLOD2	= pPlantModelLOD1->m_dimensionLOD2;
				const Vector4 boundSphereLOD2(0,0,0,1.0f);


				

				// set color for whole pTriLoc:
	//			m_Shader->SetVar(m_shdPlantColorID,		plantColourf);

				// set global micro-movement params: [ scaleH | scaleV | freqH | freqV ]
				const Vec4V unpacked_um_params		= Float16Vec4Unpack(&pCurrTriPlant->um_param);
	//			m_Shader->SetVar(m_shdUMovementParamsID, pCurrTriPlant->um_param);

				// set global collision radius scale:
				const Vec2V unpacked_coll_params	= Float16Vec2Unpack(&pCurrTriPlant->coll_params);
	//			m_Shader->SetVar(m_shdCollParamsID,		pCurrTriPlant->coll_params);

				// LOD2 billboard dimensions:
	//			Vector4 dimLOD2;
	//			dimLOD2.x = GeometryDimLOD2.x;
	//			dimLOD2.y = GeometryDimLOD2.y;
	//			dimLOD2.z = 0.0f;
	//			dimLOD2.w = 0.0f;
	//			m_Shader->SetVar(m_shdDimensionLOD2ID,	dimLOD2);


				Vector3 *v1 = (Vector3*)&pCurrTriPlant->V1;
				Vector3 *v2 = (Vector3*)&pCurrTriPlant->V2;
				Vector3 *v3 = (Vector3*)&pCurrTriPlant->V3;

				Color32 *c1 = &pCurrTriPlant->groundColorV1;
				Color32 *c2 = &pCurrTriPlant->groundColorV2;
				Color32 *c3 = &pCurrTriPlant->groundColorV3;

				// local per-vertex data: ground color(XYZ) + scaleZ/XYZ:
				Vector4 groundColorV1, groundColorV2, groundColorV3;
				Vector3 groundScaleV1, groundScaleV2, groundScaleV3;
				Vector4 groundColorf;
				Vector3 groundScalef;

				groundColorV1.x = c1->GetRedf();
				groundColorV1.y = c1->GetGreenf();
				groundColorV1.z = c1->GetBluef();
				groundColorV1.w = 1.0f;
				groundScaleV1.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c1->GetAlpha());
				groundScaleV1.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c1->GetAlpha());
				groundScaleV1.z = 0.0f;

				groundColorV2.x = c2->GetRedf();
				groundColorV2.y = c2->GetGreenf();
				groundColorV2.z = c2->GetBluef();
				groundColorV2.w = 1.0f;
				groundScaleV2.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c2->GetAlpha());
				groundScaleV2.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c2->GetAlpha());
				groundScaleV2.z = 0.0f;

				groundColorV3.x = c3->GetRedf();
				groundColorV3.y = c3->GetGreenf();
				groundColorV3.z = c3->GetBluef();
				groundColorV3.w = 1.0f;
				groundScaleV3.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c3->GetAlpha());
				groundScaleV3.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c3->GetAlpha());
				groundScaleV3.z = 0.0f;

				// unpack ground scale ranges:
				const Vec2V unpacked_range_params	= Float16Vec2Unpack(&pCurrTriPlant->scaleRangeXYZ_Z);
				const float fGroundScaleRangeXYZ	= unpacked_range_params.GetXf();
				const float fGroundScaleRangeZ		= unpacked_range_params.GetYf();

				// unpack ScaleXY, ScaleZ, ScaleVarXY, ScaleVarZ:
				const Vec4V unpacked_scale_params	= Float16Vec4Unpack(&pCurrTriPlant->packedScale);
				const Vector4 ScaleV4				= RCC_VECTOR4(unpacked_scale_params);


#if CPLANT_STORE_LOCTRI_NORMAL
				// set normal for whole pTriLoc:
				fakeNormal.SetX(float(pCurrTriPlant->loctri_normal[0])/127.0f);
				fakeNormal.SetY(float(pCurrTriPlant->loctri_normal[1])/127.0f);
				fakeNormal.SetZ(float(pCurrTriPlant->loctri_normal[2])/127.0f);
				fakeNormal.SetW(0.0f);
#endif


///////////// Render LOD0: /////////////////////////////////////////////////////////////////////////
#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0) && !gbPlantsDisableLOD0 && (!gbPlantsFlashLOD0 || (GRCDEVICE_GetFrameCounter&0x01)))
#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0))
#endif
				{
					// want same values for plant positions each time...
					g_PlantsRendRand.Reset(pCurrTriPlant->seed);

					customInstDraw.Reset();

					bool bShaderCmdBufOut = false;
					//
					// render a single triPlant full of the desired model
					const u32 count = pCurrTriPlant->num_plants;
					for(u32 j=0; j<count; j++)
					{
						const float s = g_PlantsRendRand.GetRanged(0.0f, 1.0f);
						const float t = g_PlantsRendRand.GetRanged(0.0f, 1.0f);

						Vector3 posPlant;
						PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

						// calculate LOD0 distance:
					#if CPLANT_USE_COLLISION_2D_DIST
						Vector3 LodDistV3 = posPlant - globalCameraPos;
						Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
					#else
						Vector3 LodDistV = posPlant - globalCameraPos;
					#endif
						const float fLodDist2 = LodDistV.Mag2();
						const bool bLodVisible = (fLodDist2 < globalLOD0FarDist2);

						// early skip if current geometry not visible as LOD0:
						if(!bLodVisible)
						{
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							if(intensityVar)
								g_PlantsRendRand.GetInt();

							continue;
						}


						// interpolate ground color(XYZ):
						PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

						// interpolate ground scaleXYZ/Z weights:
						PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
						const float groundScaleXYZ	= groundScalef.x;
						const float groundScaleZ	= groundScalef.y;

						// ---- apply various amounts of scaling to the matrix -----
						// calculate an x/y scaling value and apply to matrix
						float scaleXY	= ScaleV4.x;
						float scaleZ	= ScaleV4.y;

						// scale variation:
						const float scaleRand01 = g_PlantsRendRand.GetRanged(0.0f, 1.0f);
						const float	scvariationXY	= ScaleV4.z;
						scaleXY		+= scvariationXY*scaleRand01;
						const float	scvariationZ	= ScaleV4.w;
						scaleZ		+= scvariationZ*scaleRand01;

						// apply ground scale variation:
						const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
						if(_grndScaleXYZ > 0.0f)
						{
							scaleXY *= _grndScaleXYZ;
							scaleZ  *= _grndScaleXYZ;
						}
						
						const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
						if(_grndScaleZ > 0.0f)
						{
							scaleZ  *= _grndScaleZ;
						}

						//// ----- end of scaling stuff ------


						// choose bigger scale for bound frustum check:
						const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

						Vector4 sphereV4;
						sphereV4.Set(boundSphereLOD0);
						sphereV4.AddVector3XYZ(posPlant);
						sphereV4.w *= boundRadiusScale;
						spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

						// skip current geometry if not visible:
						bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!gCullFrustum.IntersectsOrContainsSphere(sphere));

					#if CPLANT_DYNAMIC_CULL_SPHERES
						//Check dynamic cull spheres and cull if grass is in a cull sphere.
						CPlantMgr::DynCullSphereArray::const_iterator iter;
						CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
						for(iter = cullSpheres.begin(); iter != end && !isCulled; ++iter)
							isCulled |= iter->IntersectsSphere(sphere);
					#endif

					#if CPLANT_CLIP_EDGE_VERT
						//Skip current geometry if it intersects a clipping edge/vert:
						isCulled =	isCulled ||
									(pCurrTriPlant->m_ClipVert_0 && sphere.ContainsPoint(RCC_VEC3V(*v1))) ||
									(pCurrTriPlant->m_ClipVert_1 && sphere.ContainsPoint(RCC_VEC3V(*v2))) ||
									(pCurrTriPlant->m_ClipVert_2 && sphere.ContainsPoint(RCC_VEC3V(*v3))) ||
									(pCurrTriPlant->m_ClipEdge_01 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v1), RCC_VEC3V(*v2))) ||
									(pCurrTriPlant->m_ClipEdge_12 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v2), RCC_VEC3V(*v3))) ||
									(pCurrTriPlant->m_ClipEdge_20 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v3), RCC_VEC3V(*v1)));
					#endif

						if(isCulled)
						{
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							if(intensityVar)
								g_PlantsRendRand.GetInt();

							continue;
						}

						plantMatrix.MakeRotateZ(g_PlantsRendRand.GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
						plantMatrix.MakeTranslate(posPlant);									// overwrites d
						plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

						if(bApplyGroundSkewingLod0)
						{
							plantMatrix.Dot3x3(skewingMtx);
						}

						// ---- muck about with the matrix to make atomic look blown by the wind --------
						// use here wind_bend_scale & wind_bend_var
						//	final_bend = [ 1.0 + (bend_var*rand(-1,1)) ] * [ global_wind * bend_scale ]
						const float variationBend = wind_bend_var;
						const float bending =	(1.0f + (variationBend*g_PlantsRendRand.GetRanged(0.0f, 1.0f))) *
												(wind_bend_scale);
						plantMatrix.c.x = bending * globalWindBending.x * g_PlantsRendRand.GetRanged(-1.0f, 1.0f);
						plantMatrix.c.y = bending * globalWindBending.y * g_PlantsRendRand.GetRanged(-1.0f, 1.0f);
						// ----- end of wind stuff -----


						// color variation:
						if(intensityVar)
						{
							// intensity support:  final_I = I + VarI*rand01()
							u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand.GetRanged(0.0f, 1.0f));
							intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

							Color32 col32;
	#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
							col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
							col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
							col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
	#undef CALC_COLOR_INTENSITY

							plantColourf.x = col32.GetRedf();
							plantColourf.y = col32.GetGreenf();
							plantColourf.z = col32.GetBluef();
						} // if(intensityVar)...


						customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
						customInstDraw.m_PlantColor.Set(plantColourf);
						customInstDraw.m_GroundColor.Set(groundColorf);

						if(!bShaderCmdBufOut)
						{
							bShaderCmdBufOut = true;
							// bind plant shader			
							//	BindPlantShader(m_shdDeferredLOD0TechniqueID);
							if((!bEnableCallCaching) || (cachedShaderOffset != BindShaderLOD0cmdOffset))
							{
								cachedShaderOffset	= BindShaderLOD0cmdOffset;
								cachedGeometryOffset= (u32)-1;
								cachedTextureOffset = (u32)-1;
								gcmCon->SetCallCommand(BindShaderLOD0cmdOffset);
							}
							// after binding shader - set other VS local variables now:

							// set color for whole pTriLoc:
							// m_Shader->SetVar(m_shdPlantColorID,		plantColourf);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_PLANTCOLOR, (const float*)&plantColourf, 1);

							// set global micro-movement params: [ scaleH | scaleV | freqH | freqV ]
							// m_Shader->SetVar(m_shdUMovementParamsID, pCurrTriPlant->um_param.Get());
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_UMPARAMS, (const float*)&unpacked_um_params, 1);

							// set global collision radius scale:
							// m_Shader->SetVar(m_shdCollParamsID,		pCurrTriPlant->coll_params);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_PLAYERCOLLPARAMS, (const float*)&unpacked_coll_params, 1);

						#if CPLANT_WRITE_GRASS_NORMAL
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TERRAINNORMAL, (const float*)&groundNormal, 1);
						#endif

						#if CPLANT_STORE_LOCTRI_NORMAL
							// set global faked normal:
							// m_Shader->SetVar(m_shdFakedGrassNormal, fakeNormal);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_FAKEDNORMAL, (const float*)&fakeNormal, 1);
						#endif

							// setup texture:
							//m_Shader->SetVar(m_shdTextureID,		pPlantTextureLOD0);
							if((!bEnableCallCaching) || (cachedTextureOffset != TextureLOD0cmdOffset))
							{
								cachedTextureOffset = TextureLOD0cmdOffset;
								gcmCon->SetCallCommand(TextureLOD0cmdOffset);
							}

							// Setup the vertex and index buffers
							//grcIndexBuffer* pIndexBuffer			= pGeometryLOD0->GetIndexBuffer(true);
							//grcVertexBuffer* pVertexBuffer			= pGeometryLOD0->GetVertexBuffer(true);
							//const u16 nPrimType						= pGeometryLOD0->GetPrimitiveType();
							//const u32 nIdxCount						= pIndexBuffer->GetIndexCount();
							//grcVertexDeclaration* vertexDeclaration	= pGeometryLOD0->GetDecl();
							//GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
							//GRCDEVICE.SetIndices(*pIndexBuffer);
							//grcCurrentIndexBuffer = reinterpret_cast<const grcIndexBufferGCM*>(&pBuffer);
							//GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
							if((!bEnableCallCaching) || (cachedGeometryOffset != GeometryLOD0Offset))
							{
								cachedGeometryOffset = GeometryLOD0Offset;
								gcmCon->SetCallCommand(GeometryLOD0Offset);
							}
						}

						const bool bOptimizedSetup = customInstDraw.UseOptimizedVsSetup(plantMatrix);
						if(bOptimizedSetup)
						{	// optimized setup: 3 registers:
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TRANSFORM+3, customInstDraw.GetRegisterPointerOpt(), customInstDraw.GetRegisterCountOpt());
						}
						else
						{	// full setup: 6 registers:
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TRANSFORM, customInstDraw.GetRegisterPointerFull(), customInstDraw.GetRegisterCountFull());
						}
						GRCDEVICE_DrawIndexedPrimitive(gcmCon, pPlantModelLOD0);
#if __BANK
						// rendering stats:
						(*pLocTriPlantsLOD0DrawnPerSlot)++;
#endif //__BANK
						if(bHeapLastBlock)
						{
							CustomGcmReserveFailed(gcmCon, 0);	// try to flush last segment
						}
					}// for(u32 j=0; j<count; j++)...

					if(bShaderCmdBufOut)
					{
					//UnBindPlantShader();
					}

				}// render LOD0...
////////////////////////////////////////////////////////////////////////////////////////////////////


///////////// Render LOD1: /////////////////////////////////////////////////////////////////////////
	#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1) && !gbPlantsDisableLOD1 && (!gbPlantsFlashLOD1 || (GRCDEVICE_GetFrameCounter&0x01)))
	#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1))
	#endif
				{
					g_PlantsRendRand.Reset(pCurrTriPlant->seed);

					customInstDraw.Reset();

					bool bShaderCmdBufOut = false;
					//
					// render a single triPlant full of the desired model
					const u32 count = pCurrTriPlant->num_plants;
					for(u32 j=0; j<count; j++)
					{
						const float s = g_PlantsRendRand.GetRanged(0.0f, 1.0f);
						const float t = g_PlantsRendRand.GetRanged(0.0f, 1.0f);

						Vector3 posPlant;
						PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

						// calculate LOD1 distance:
					#if CPLANT_USE_COLLISION_2D_DIST
						Vector3 LodDistV3 = posPlant - globalCameraPos;
						Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
					#else
						Vector3 LodDistV = posPlant - globalCameraPos;
					#endif
						const float fLodDist2 = LodDistV.Mag2();
						const bool bLodVisible = (fLodDist2 > globalLOD1CloseDist2)	&& (fLodDist2 < globalLOD1FarDist2);

						// early skip if current geometry not visible as LOD1:
						if(!bLodVisible)
						{
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							if(intensityVar)
								g_PlantsRendRand.GetInt();

							continue;
						}

						// interpolate ground color(XYZ):
						PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

						// interpolate ground scaleXYZ/Z weights:
						PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
						const float groundScaleXYZ	= groundScalef.x;
						const float groundScaleZ	= groundScalef.y;

						// ---- apply various amounts of scaling to the matrix -----
						// calculate an x/y scaling value and apply to matrix
						float scaleXY	= ScaleV4.x;
						float scaleZ	= ScaleV4.y;

						// scale variation:
						const float scaleRand01 = g_PlantsRendRand.GetRanged(0.0f, 1.0f);
						const float	scvariationXY	= ScaleV4.z;
						scaleXY		+= scvariationXY*scaleRand01;;
						const float	scvariationZ	= ScaleV4.w;
						scaleZ		+= scvariationZ*scaleRand01;

						// apply ground scale variation:
						const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
						if(_grndScaleXYZ > 0.0f)
						{
							scaleXY *= _grndScaleXYZ;
							scaleZ  *= _grndScaleXYZ;
						}
						
						const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
						if(_grndScaleZ > 0.0f)
						{
							scaleZ  *= _grndScaleZ;
						}

						//// ----- end of scaling stuff ------


						// choose bigger scale for bound frustum check:
						const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

						Vector4 sphereV4;
						sphereV4.Set(boundSphereLOD1);
						sphereV4.AddVector3XYZ(posPlant);
						sphereV4.w *= boundRadiusScale;
						spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

						// skip current geometry if not visible:
						bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!gCullFrustum.IntersectsOrContainsSphere(sphere));

					#if CPLANT_DYNAMIC_CULL_SPHERES
						//Check dynamic cull spheres and cull if grass is in a cull sphere.
						CPlantMgr::DynCullSphereArray::const_iterator iter;
						CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
						for(iter = cullSpheres.begin(); iter != end && !isCulled; ++iter)
							isCulled |= iter->IntersectsSphere(sphere);
					#endif

					#if CPLANT_CLIP_EDGE_VERT
						//Skip current geometry if it intersects a clipping edge/vert:
						isCulled =	isCulled ||
									(pCurrTriPlant->m_ClipVert_0 && sphere.ContainsPoint(RCC_VEC3V(*v1))) ||
									(pCurrTriPlant->m_ClipVert_1 && sphere.ContainsPoint(RCC_VEC3V(*v2))) ||
									(pCurrTriPlant->m_ClipVert_2 && sphere.ContainsPoint(RCC_VEC3V(*v3))) ||
									(pCurrTriPlant->m_ClipEdge_01 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v1), RCC_VEC3V(*v2))) ||
									(pCurrTriPlant->m_ClipEdge_12 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v2), RCC_VEC3V(*v3))) ||
									(pCurrTriPlant->m_ClipEdge_20 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v3), RCC_VEC3V(*v1)));
					#endif
					
						if(isCulled)
						{
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							if(intensityVar)
								g_PlantsRendRand.GetInt();

							continue;
						}

						plantMatrix.MakeRotateZ(g_PlantsRendRand.GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
						plantMatrix.MakeTranslate(posPlant);									// overwrites d
						plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

						if(bApplyGroundSkewingLod1)
						{
							plantMatrix.Dot3x3(skewingMtx);
						}

						// ---- muck about with the matrix to make atomic look blown by the wind --------
						g_PlantsRendRand.GetInt();
						g_PlantsRendRand.GetInt();
						g_PlantsRendRand.GetInt();
						// ----- end of wind stuff -----

						// color variation:
						if(intensityVar)
						{
							// intensity support:  final_I = I + VarI*rand01()
							u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand.GetRanged(0.0f, 1.0f));
							intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

							Color32 col32;
	#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
							col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
							col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
							col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
	#undef CALC_COLOR_INTENSITY

							plantColourf.x = col32.GetRedf();
							plantColourf.y = col32.GetGreenf();
							plantColourf.z = col32.GetBluef();
						} // if(intensityVar)...


						customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
						customInstDraw.m_PlantColor.Set(plantColourf);
						customInstDraw.m_GroundColor.Set(groundColorf);

						if(!bShaderCmdBufOut)
						{
							bShaderCmdBufOut = true;
							// Bind the grass shader
							//BindPlantShader(m_shdDeferredLOD1TechniqueID);
							if((!bEnableCallCaching) || (cachedShaderOffset != BindShaderLOD1cmdOffset))
							{
								cachedShaderOffset  = BindShaderLOD1cmdOffset;
								cachedGeometryOffset= (u32)-1;
								cachedTextureOffset = (u32)-1;
								gcmCon->SetCallCommand(BindShaderLOD1cmdOffset);
							}
							// after binding shader - set other VS local variables now:

							// set color for whole pTriLoc:
							// m_Shader->SetVar(m_shdPlantColorID,		plantColourf);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_PLANTCOLOR, (const float*)&plantColourf, 1);

							// set global micro-movement params: [ scaleH | scaleV | freqH | freqV ]
							// m_Shader->SetVar(m_shdUMovementParamsID, pCurrTriPlant->um_param);
							//GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_UMPARAMS, (const float*)&unpacked_um_params, 1);

							// set global collision radius scale:
							// m_Shader->SetVar(m_shdCollParamsID,		pCurrTriPlant->coll_params);
							//GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_PLAYERCOLLPARAMS, (const float*)&unpacked_coll_params, 1);

						#if CPLANT_WRITE_GRASS_NORMAL
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TERRAINNORMAL, (const float*)&groundNormal, 1);
						#endif

						#if CPLANT_STORE_LOCTRI_NORMAL
							// set global faked normal:
							// m_Shader->SetVar(m_shdFakedGrassNormal, fakeNormal);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_FAKEDNORMAL, (const float*)&fakeNormal, 1);
						#endif

							// setup texture:
							// m_Shader->SetVar(m_shdTextureID,	pPlantTextureLOD1);
							if((!bEnableCallCaching) || (cachedTextureOffset != TextureLOD1cmdOffset))
							{
								cachedTextureOffset = TextureLOD1cmdOffset;
								gcmCon->SetCallCommand(TextureLOD1cmdOffset);
							}

							// Setup the vertex and index buffers
							//grcIndexBuffer* pIndexBuffer			= pGeometryLOD1->GetIndexBuffer(true);
							//grcVertexBuffer* pVertexBuffer			= pGeometryLOD1->GetVertexBuffer(true);
							//const u16 nPrimType						= pGeometryLOD1->GetPrimitiveType();
							//const u32 nIdxCount						= pIndexBuffer->GetIndexCount();
							//grcVertexDeclaration* vertexDeclaration	= pGeometryLOD1->GetDecl();
							//GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
							//GRCDEVICE.SetIndices(*pIndexBuffer);
							//GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
							if((!bEnableCallCaching) || (cachedGeometryOffset != GeometryLOD1Offset))
							{
								cachedGeometryOffset = GeometryLOD1Offset;
								gcmCon->SetCallCommand(GeometryLOD1Offset);
							}
						}

						const bool bOptimizedSetup = customInstDraw.UseOptimizedVsSetup(plantMatrix);
						if(bOptimizedSetup)
						{	// optimized setup: 3 registers:
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TRANSFORM+3, customInstDraw.GetRegisterPointerOpt(), customInstDraw.GetRegisterCountOpt());
						}
						else
						{	// full setup: 6 registers:
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TRANSFORM, customInstDraw.GetRegisterPointerFull(), customInstDraw.GetRegisterCountFull());
						}
						GRCDEVICE_DrawIndexedPrimitive(gcmCon, pPlantModelLOD1);
#if __BANK
						// rendering stats:
						(*pLocTriPlantsLOD1DrawnPerSlot)++;
#endif //__BANK
						if(bHeapLastBlock)
						{
							CustomGcmReserveFailed(gcmCon, 0);	// try to flush last segment
						}
					}// for(u32 j=0; j<count; j++)...
					//////////////////////////////////////////////////////////////////

					if(bShaderCmdBufOut)
					{
					//UnBindPlantShader();
					}
				}// renderLOD1...
////////////////////////////////////////////////////////////////////////////////////////////////////


///////////// Render LOD2: /////////////////////////////////////////////////////////////////////////
	#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2) && !gbPlantsDisableLOD2 && (!gbPlantsFlashLOD2 || (GRCDEVICE_GetFrameCounter&0x01)))
	#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2))
	#endif
				{
					g_PlantsRendRand.Reset(pCurrTriPlant->seed);

					customInstDraw.Reset();

					bool bShaderCmdBufOut = false;
					//
					// render a single triPlant full of the desired model
					const u32 count = pCurrTriPlant->num_plants;
					for(u32 j=0; j<count; j++)
					{
						const float s = g_PlantsRendRand.GetRanged(0.0f, 1.0f);
						const float t = g_PlantsRendRand.GetRanged(0.0f, 1.0f);

						Vector3 posPlant;
						PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

						// calculate LOD2 distance:
					#if CPLANT_USE_COLLISION_2D_DIST
						Vector3 LodDistV3 = posPlant - globalCameraPos;
						Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
					#else
						Vector3 LodDistV = posPlant - globalCameraPos;
					#endif
						const float fLodDist2 = LodDistV.Mag2();
						const bool bLodVisible = (fLodDist2 > globalLOD2CloseDist2)	&& (fLodDist2 < globalLOD2FarDist2);

						// early skip if current geometry not visible as LOD2:
						if(!bLodVisible)
						{
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							if(intensityVar)
								g_PlantsRendRand.GetInt();

							continue;
						}

						// interpolate ground color(XYZ):
						PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

						// interpolate ground scaleXYZ/Z weights:
						PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
						const float groundScaleXYZ	= groundScalef.x;
						const float groundScaleZ	= groundScalef.y;

						// ---- apply various amounts of scaling to the matrix -----
						// calculate an x/y scaling value and apply to matrix
						float scaleXY	= ScaleV4.x;
						float scaleZ	= ScaleV4.y;

						// scale variation:
						const float scaleRand01 = g_PlantsRendRand.GetRanged(0.0f, 1.0f);
						const float	scvariationXY	= ScaleV4.z;
						scaleXY		+= scvariationXY*scaleRand01;
						const float	scvariationZ	= ScaleV4.w;
						scaleZ		+= scvariationZ*scaleRand01;

						// apply ground scale variation:
						const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
						if(_grndScaleXYZ > 0.0f)
						{
							scaleXY *= _grndScaleXYZ;
							scaleZ  *= _grndScaleXYZ;
						}
						
						const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
						if(_grndScaleZ > 0.0f)
						{
							scaleZ  *= _grndScaleZ;
						}

						//// ----- end of scaling stuff ------


						// choose bigger scale for bound frustum check:
						//const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);
			
						Vector4 sphereV4;
						sphereV4.Set(boundSphereLOD2);
						sphereV4.AddVector3XYZ(posPlant);
						//sphereV4.w *= boundRadiusScale;
						spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

						// skip current geometry if not visible:
						bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!gCullFrustum.IntersectsOrContainsSphere(sphere));

					#if CPLANT_DYNAMIC_CULL_SPHERES || CPLANT_CLIP_EDGE_VERT
						// choose bigger scale for bound frustum check:
						const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

						//The above sphere is not a tight enough bound for DynSphereCull/Poly Cull. Recompute a more accurate bounding sphere.
						float lod2MaxOffset = Max(GeometryDimLOD2.x, GeometryDimLOD2.y);
						Vec3V lod2halfOffset = Vec3V(lod2MaxOffset, lod2MaxOffset, lod2MaxOffset) * ScalarV(V_HALF);
						Vec3V lod2SphereCenter = sphere.GetCenter();// + lod2halfOffset;
						ScalarV lod2SphereRadius = lod2halfOffset.GetX() * LoadScalar32IntoScalarV(boundRadiusScale);
						spdSphere lod2Sphere(lod2SphereCenter, lod2SphereRadius);
					#endif

					#if CPLANT_DYNAMIC_CULL_SPHERES
						//Check dynamic cull spheres and cull if grass is in a cull sphere.
						CPlantMgr::DynCullSphereArray::const_iterator iter;
						CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
						for(iter = cullSpheres.begin(); iter != end && !isCulled; ++iter)
							isCulled |= iter->IntersectsSphere(lod2Sphere);
					#endif

					#if CPLANT_CLIP_EDGE_VERT
						//Skip current geometry if it intersects a clipping edge/vert:
						isCulled =	isCulled ||
									(pCurrTriPlant->m_ClipVert_0 && lod2Sphere.ContainsPoint(RCC_VEC3V(*v1))) ||
									(pCurrTriPlant->m_ClipVert_1 && lod2Sphere.ContainsPoint(RCC_VEC3V(*v2))) ||
									(pCurrTriPlant->m_ClipVert_2 && lod2Sphere.ContainsPoint(RCC_VEC3V(*v3))) ||
									(pCurrTriPlant->m_ClipEdge_01 && geomSpheres::TestSphereToSeg(lod2Sphere.GetCenter(), lod2Sphere.GetRadius(), RCC_VEC3V(*v1), RCC_VEC3V(*v2))) ||
									(pCurrTriPlant->m_ClipEdge_12 && geomSpheres::TestSphereToSeg(lod2Sphere.GetCenter(), lod2Sphere.GetRadius(), RCC_VEC3V(*v2), RCC_VEC3V(*v3))) ||
									(pCurrTriPlant->m_ClipEdge_20 && geomSpheres::TestSphereToSeg(lod2Sphere.GetCenter(), lod2Sphere.GetRadius(), RCC_VEC3V(*v3), RCC_VEC3V(*v1)));
					#endif

						if(isCulled)
						{
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							g_PlantsRendRand.GetInt();
							if(intensityVar)
								g_PlantsRendRand.GetInt();

							continue;
						}

						//	plantMatrix.MakeRotateZ(g_PlantsRendRand.GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
						g_PlantsRendRand.GetInt();
						plantMatrix.Identity3x3();
						plantMatrix.MakeTranslate(posPlant);			// overwrites d
						plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

						if(bApplyGroundSkewingLod2)
						{
							plantMatrix.Dot3x3(skewingMtx);
						}

						// ---- muck about with the matrix to make atomic look blown by the wind --------
						g_PlantsRendRand.GetInt();
						g_PlantsRendRand.GetInt();
						g_PlantsRendRand.GetInt();
						// ----- end of wind stuff -----

						// color variation:
						if(intensityVar)
						{
						#if 1
							// intensity support:  final_I = I + VarI*rand01()
							u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand.GetRanged(0.0f, 1.0f));
							intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

							Color32 col32;
							#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
								col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
								col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
								col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
							#undef CALC_COLOR_INTENSITY

							plantColourf.x = col32.GetRedf();
							plantColourf.y = col32.GetGreenf();
							plantColourf.z = col32.GetBluef();
						#else
							g_PlantsRendRand.GetInt();
						#endif
						} // if(intensityVar)...

						if(!bShaderCmdBufOut)
						{
							bShaderCmdBufOut = true;
							// Bind the grass shader
							//BindPlantShader(m_shdDeferredLOD2TechniqueID);
							if((!bEnableCallCaching) || (cachedShaderOffset != BindShaderLOD2cmdOffset))
							{
								cachedShaderOffset  = BindShaderLOD2cmdOffset;
								cachedGeometryOffset= (u32)-1;
								cachedTextureOffset = (u32)-1;
								gcmCon->SetCallCommand(BindShaderLOD2cmdOffset);
							}
							// after binding shader - set other VS local variables now:

							// set color for whole pTriLoc:
							// m_Shader->SetVar(m_shdPlantColorID,		plantColourf);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_PLANTCOLOR, (const float*)&plantColourf, 1);

							// set global micro-movement params: [ scaleH | scaleV | freqH | freqV ]
							// m_Shader->SetVar(m_shdUMovementParamsID, pCurrTriPlant->um_param);
							//GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_UMPARAMS, (const float*)&unpacked_um_params, 1);

							// set global collision radius scale:
							// m_Shader->SetVar(m_shdCollParamsID,		pCurrTriPlant->coll_params);
							//GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_PLAYERCOLLPARAMS, (const float*)&unpacked_coll_params, 1);

						#if CPLANT_WRITE_GRASS_NORMAL
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TERRAINNORMAL, (const float*)&groundNormal, 1);
						#endif

						#if CPLANT_STORE_LOCTRI_NORMAL
							// set global faked normal:
							// m_Shader->SetVar(m_shdFakedGrassNormal, fakeNormal);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_FAKEDNORMAL, (const float*)&fakeNormal, 1);
						#endif

							// LOD2 billboard dimensions:
							Vector4 dimLOD2;
							dimLOD2.x = GeometryDimLOD2.x;
							dimLOD2.y = GeometryDimLOD2.y;
							dimLOD2.z = pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2FARFADE)? 1.0f : 0.0f;
							dimLOD2.w = 0.0f;
							// m_Shader->SetVar(m_shdDimensionLOD2ID,	dimLOD2);
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_DIMENSIONLOD2, (const float*)&dimLOD2, 1);

							// setup texture:
							// m_Shader->SetVar(m_shdTextureID,	pPlantTextureLOD1);
							if((!bEnableCallCaching) || (cachedTextureOffset != TextureLOD1cmdOffset))
							{
								cachedTextureOffset = TextureLOD1cmdOffset;
								gcmCon->SetCallCommand(TextureLOD1cmdOffset);
							}

							// Setup the vertex and index buffers
							//grcVertexBuffer* pVertexBuffer			= ms_plantLOD2VertexBuffer;
							//grcVertexDeclaration* vertexDeclaration	= ms_plantLOD2VertexDecl;
							//GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
							//GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
							if((!bEnableCallCaching) || (cachedGeometryOffset != GeometryLOD2Offset))
							{
								cachedGeometryOffset = GeometryLOD2Offset;
								gcmCon->SetCallCommand(GeometryLOD2Offset);
							}
						}

					#if GRASS_LOD2_BATCHING
						// store all per-instance data 
						BatcherLod2.Add(plantMatrix, plantColourf, groundColorf);
					#else
						customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
						customInstDraw.m_PlantColor.Set(plantColourf);
						customInstDraw.m_GroundColor.Set(groundColorf);

						const bool bOptimizedSetup = customInstDraw.UseOptimizedVsSetup(plantMatrix);
						if(bOptimizedSetup)
						{	// optimized setup: 3 registers:
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TRANSFORM+3, customInstDraw.GetRegisterPointerOpt(), customInstDraw.GetRegisterCountOpt());
							GRCDEVICE_DrawPrimitive(gcmCon, CELL_GCM_PRIMITIVE_QUADS, /*startVertex*/0, /*vertexCount*/4 );
						}
						else
						{	// full setup: 6 registers:
							GRCDEVICE_SetVertexShaderConstant(gcmCon, GRASS_REG_TRANSFORM, customInstDraw.GetRegisterPointerFull(), customInstDraw.GetRegisterCountFull());
							GRCDEVICE_DrawPrimitive(gcmCon, CELL_GCM_PRIMITIVE_QUADS, /*startVertex*/0, /*vertexCount*/4 );
						}
					#endif

	#if __BANK
						// rendering stats:
						(*pLocTriPlantsLOD2DrawnPerSlot)++;
	#endif //__BANK
						if(bHeapLastBlock)
						{
							CustomGcmReserveFailed(gcmCon, 0);	// try to flush last segment
						}
					}// for(u32 j=0; j<count; j++)...
					//////////////////////////////////////////////////////////////////
					
					if(bShaderCmdBufOut)
					{
					// UnBindPlantShader();
					}

					#if GRASS_LOD2_BATCHING
						BatcherLod2.Flush();
					#endif

				}// renderLOD2's...
////////////////////////////////////////////////////////////////////////////////////////////////////

	}// for(s32 i=0; i<numTriPlants; i++)...


//	SPU_DEBUGF(spu_printf("\n SPU %X: numPlants=%d, drawn=%d", SPUTASKID, numPlants, numPlantsDrawn));

}// end of processGrassTris()...




//
//
//
//
void spuDrawTriPlants(::rage::sysTaskParameters &taskParams)
{
fwStepAllocator scratchAllocator(taskParams.Scratch.Data, taskParams.Scratch.Size);

	// Prolog:
	inMasterGrassParams		= (spuGrassParamsStructMaster*)taskParams.Input.Data;
	Assert(inMasterGrassParams);
	outMasterGrassParams	= (spuGrassParamsStructMaster*)taskParams.Output.Data;
	Assert(outMasterGrassParams);

	// synchronize in-out data:
	sysMemCpy(outMasterGrassParams, inMasterGrassParams, sizeof(spuGrassParamsStructMaster));

	outMasterGrassParams->m_DecrementerTicks = spu_read_decrementer();


	//	spu_write_decrementer(0x08000000);	// initialize decrementer to some known value
	g_PlantsRendRand.Reset(spu_read_decrementer());


	inGrassParamsTab = (spuGrassParamsStruct*)scratchAllocator.Alloc(IN_GRASS_PARAMS_TAB_SIZE, IN_GRASS_PARAMS_TAB_ALIGN);
	// grab all grass job params:
	sysDmaGet(inGrassParamsTab, (u32)(inMasterGrassParams->m_inSpuGrassStructTab), sizeof(spuGrassParamsStruct)*nNumGrassJobsLaunched, PLANTS_DMATAGID);


	CompileTimeAssert(sizeof(_localPlantModelsSlot0)== PLANTSMGR_LOCAL_SLOTSIZE_MODEL);

	// grab all models:
	sysDmaGet(&localPlantModelsSlot0[0], (u32)(inMasterGrassParams->m_PlantModelsTab), PLANTSMGR_LOCAL_SLOTSIZE_MODEL, PLANTS_DMATAGID);

	// resolve models pointers:
	localPlantModelsTab	= &localPlantModelsSlot0[0];

	// resolve texture offsets to pointers:
	localPlantTexturesTab = (u32*)PlantTexturesSlotOffset0;

#if CPLANT_DYNAMIC_CULL_SPHERES
	// grab cull spheres
	sysDmaGet(&localDynCullSpheres, reinterpret_cast<u32>(inMasterGrassParams->m_CullSphereArray), sizeof(PlantMgr_DynCullSphereArray), PLANTS_DMATAGID);
#endif

	SPU_DEBUGF(spu_printf("\n SPU %X: taskid=%X", SPUTASKID, SPUTASKID));
//	SPU_DEBUGF(spu_printf("\n SPU %X: numjobs=%d", SPUTASKID, nNumGrassJobsLaunched));

	eaRsxLabel5				= inMasterGrassParams->m_eaRsxLabel5;
	rsxLabel5Index			= inMasterGrassParams->m_rsxLabel5Index;

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	HeapBlockArray			= (grassHeapBlock*)scratchAllocator.Alloc(HEAP_BLOCK_ARRAY_SIZE, HEAP_BLOCK_ARRAY_ALIGN);
	sysDmaGet(HeapBlockArray, (u32)(inMasterGrassParams->m_heapBlockArray), PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE*sizeof(grassHeapBlock), PLANTS_DMATAGID);
	HeapCurrentBlock		= 0;
#else
	eaHeapBegin				= inMasterGrassParams->m_heapBegin;
	heapBeginOffset			= inMasterGrassParams->m_heapBeginOffset;
	eaHeapBeginCurrent		= eaHeapBegin;
	heapBeginOffsetCurrent	= heapBeginOffset;
#endif
	bHeapLastBlock			= false;

	eaHeapOverfilled		= 0;


	_localSolidHeap0	= (u32*)scratchAllocator.Alloc(LOCAL_SOLID_HEAP_SIZE, LOCAL_SOLID_HEAP_ALIGN);
	_localSolidHeap1	= (u32*)scratchAllocator.Alloc(LOCAL_SOLID_HEAP_SIZE, LOCAL_SOLID_HEAP_ALIGN);
	localSolidHeap[0]	= _localSolidHeap0;
	localSolidHeap[1]	= _localSolidHeap1;

	localDmaTagID[0]	= PLANTS_DMATAGID1;
	localDmaTagID[1]	= PLANTS_DMATAGID2;


	_localTriPlantTab0	= (PPTriPlant*)scratchAllocator.Alloc(LOCAL_TRI_PLANT_TAB_SIZE, LOCAL_TRI_PLANT_TAB_ALIGN);
	_localTriPlantTab1	= (PPTriPlant*)scratchAllocator.Alloc(LOCAL_TRI_PLANT_TAB_SIZE, LOCAL_TRI_PLANT_TAB_ALIGN);
	localTriPlantTab[0] = _localTriPlantTab0;
	localTriPlantTab[1] = _localTriPlantTab1;
	triPlantTabDmaTagID[0] = PLANTS_DMATAGID3;
	triPlantTabDmaTagID[1] = PLANTS_DMATAGID4;
	triPlantTabBufferID	= 0;

	globalCameraPos.x	= inMasterGrassParams->m_vecCameraPos.x;
	globalCameraPos.y	= inMasterGrassParams->m_vecCameraPos.y;
	globalCameraPos.z	= inMasterGrassParams->m_vecCameraPos.z;
	globalCameraPos.w	= 0.0f;


	// finish all loading to LS:
	sysDmaWait(1<<PLANTS_DMATAGID);


	// setup main local context:
	CellGcmContext		localCon;
	SetupContextData(&localCon, localSolidHeap[0], PLANTSMGR_LOCAL_HEAP_SIZE, 32, &CustomGcmReserveFailed);
	spuBufferID	= 0;
	bCustomGcmReserveFailedProlog	= TRUE;	// prolog flag

	// invalidate texture cache?
//	localCon.SetInvalidateTextureCache(CELL_GCM_INVALIDATE_TEXTURE);

	// start fetching triPlantTab[0]:
	spuGrassParamsStruct *jobParams0 = &inGrassParamsTab[0];
	sysDmaGet(localTriPlantTab[triPlantTabBufferID], (u32)(jobParams0->m_pTriPlant), sizeof(PPTriPlant)*jobParams0->m_numTriPlant, triPlantTabDmaTagID[triPlantTabBufferID]);


//////////////////////////////////////////////////////////////////

	// make sure RSX really executed prevPrev command buffer:
	if(0) // now done on PPU
	{
		const u32 heapID					= inMasterGrassParams->m_heapID;
		const u32 rsxLabel5_CurrentTaskID	= inMasterGrassParams->m_rsxLabel5_CurrentTaskID;

		const u32 prevPrevTaskID= 0xFF000000|(((heapID  )&0x1)<<8)|	((rsxLabel5_CurrentTaskID-2)&0xFF);
		const u32 prevTaskID0	=									((rsxLabel5_CurrentTaskID-1)&0xFF);
		const u32 prevTaskID1	= 0xFF000000|(((heapID-1)&0x1)<<8)|	((rsxLabel5_CurrentTaskID-1)&0xFF);
		const u32 currTaskID0	=									((rsxLabel5_CurrentTaskID  )&0xFF);
		const u32 currTaskID1	= 0xFF000000|(((heapID  )&0x1)<<8)|	((rsxLabel5_CurrentTaskID  )&0xFF);

		u32 label5 = sysDmaGetUInt32(eaRsxLabel5, PLANTS_DMATAGID);
		//Printf("\n**Grass: label5=%X, prevTaskID=%X", label5, prevTaskID);

		u32 deadman=40;
		while(	(label5 != prevPrevTaskID)							&&
				(label5 != prevTaskID0) && (label5 != prevTaskID1)	&&
				(label5 != currTaskID0)	&& (label5 != currTaskID1)	)
		{
			RSX_SPIN_WAIT;	// Sleep a bit - this stops us hitting the bus so hard.
			if(!(--deadman))
			{
				deadman=40;
				Printf("\nPrevPrevJob %X still not executed by RSX [label5=%08X]!", prevPrevTaskID,label5);
			}
			label5 = sysDmaGetUInt32(eaRsxLabel5, PLANTS_DMATAGID);
		}
	}
	
//////////////////////////////////////////////////////////////////
	for(u32 jobID=0; jobID<nNumGrassJobsLaunched; jobID++)
	{
		// start fetching triPlantTab for next job:
		const u32 nextJobID = jobID+1; 
		if(nextJobID < nNumGrassJobsLaunched)
		{
			spuGrassParamsStruct *nextJobParams	= &inGrassParamsTab[nextJobID];
			const u32 nextBufID = (triPlantTabBufferID+1)&0x00000001;
			sysDmaGet(localTriPlantTab[nextBufID], (u32)(nextJobParams->m_pTriPlant), sizeof(PPTriPlant)*nextJobParams->m_numTriPlant, triPlantTabDmaTagID[nextBufID]);
		}

		// execute single job here:
		spuGrassParamsStruct *jobParams	= &inGrassParamsTab[jobID];

		// wait for current triPlantTab and execute:
		sysDmaWait(1<<triPlantTabDmaTagID[triPlantTabBufferID]);
		processGrassTris(jobParams, localTriPlantTab[triPlantTabBufferID], &localCon);

		// flip bufferID:
		(++triPlantTabBufferID) &= 0x00000001;
	}//	for(u32 jobID=0; jobID<nNumGrassJobsLaunched; jobID++)...
////////////////////////////////////////////////////////////////////////


	// Epilog:
	//localCon.SetWriteCommandLabel(rsxLabelIndex5, SPUTASKID+0x1000000);	// write debug value so we know RSX finished executing and returned to main CB
	localCon.SetJumpCommand(inMasterGrassParams->m_ReturnOffsetToMainCB);	// jump back to main CB
	bHeapLastBlock = false;
	CustomGcmReserveFailed((CellGcmContextData*)&localCon, 0);	// start copying current buffer

	bCustomGcmReserveFailedEpilog	= TRUE;	// this is last call
	CustomGcmReserveFailed((CellGcmContextData*)&localCon, 0);	// wait for end of current buffer's dma copy

	// synchronize with RSX:
//	u32 timeWaitForRSX = spu_read_decrementer();
//	{
//		spuWaitForLabel(&rsxLabel5, eaRsxLabel5, inMasterGrassParams->m_rsxLabel5_CurrentTaskID);
//		SPU_DEBUGF(spu_printf("\n SPU %X: Waiting for label5 finished!", SPUTASKID));
//	}
//	timeWaitForRSX -= spu_read_decrementer();

	if(eaHeapOverfilled)
	{	// clear to NOP and do not execute (possibly incomplete) this big buffer:
		//spuClearJTSinBuffer(inMasterGrassParams->m_eaMainJTS, 0x00000000);
		// clear JTS + jump to our big buffer:
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
		spuClearJTSinBuffer(inMasterGrassParams->m_eaMainJTS, HeapBlockArray[0].offset);
#else
		spuClearJTSinBuffer(inMasterGrassParams->m_eaMainJTS, heapBeginOffset);
#endif
	}
	else
	{	// clear JTS + jump to our big buffer:
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
		spuClearJTSinBuffer(inMasterGrassParams->m_eaMainJTS, HeapBlockArray[0].offset);
#else
		spuClearJTSinBuffer(inMasterGrassParams->m_eaMainJTS, heapBeginOffset);
#endif
	}


	SPU_DEBUGF(spu_printf("\n SPU %X: end: returnOffset=0x%X, eaMainJTS=0x%X", SPUTASKID, inMasterGrassParams->m_ReturnOffsetToMainCB,inMasterGrassParams->m_eaMainJTS));
	SPU_DEBUGF(spu_printf("\n SPU %X: end: BigBuffer: PPU: s=0x%X, c=0x%X, e=0x%X", SPUTASKID, eaHeapBegin,eaHeapBeginCurrent,eaHeapBegin+heapSize));
	SPU_DEBUGF(spu_printf("\n SPU %X: end: BigBuffer: RSX: s=0x%X, c=0x%X, e=0x%X", SPUTASKID, heapBeginOffset,heapBeginOffsetCurrent,heapBeginOffset+heapSize));

#if 0
	sys_time_get_timebase_frequency();
	Anyway I tried on 085 and 092 with this code in an SPU job:
	// Wait 5 seconds at 80 MHz
	uint32_t start = spu_readch(SPU_RdDec);
	uint32_t time;
	do {
		time = spu_readch(SPU_RdDec);
	} while(start-time<79800000*5);
	and the job takes exactly 5 seconds
#endif

	// amount of consumed bytes in big buffer:
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	outMasterGrassParams->m_AmountOfBigHeapConsumed = HeapCurrentBlock;
#else
	outMasterGrassParams->m_AmountOfBigHeapConsumed = (eaHeapBeginCurrent - eaHeapBegin)/PLANTSMGR_LOCAL_HEAP_SIZE;
#endif
	// number of ticks it took to execute this job:
	outMasterGrassParams->m_DecrementerTicks = outMasterGrassParams->m_DecrementerTicks - spu_read_decrementer();

	SPU_DEBUGF(spu_printf("\n SPU%X: waited for RSX for %d ticks (%.2f%% of total job time %d ticks).", SPUTASKID, timeWaitForRSX, float(timeWaitForRSX)/float(outMasterGrassParams->m_DecrementerTicks)*100.0f, outMasterGrassParams->m_DecrementerTicks));

	SPU_DEBUGF(spu_printf("\n SPU%X: consumed %dKB", SPUTASKID, (eaHeapBeginCurrent-eaHeapBegin)/1024));
	if(eaHeapOverfilled)
	{
		SPU_DEBUGF1(spu_printf("\n SPU%X: overfilled by %dKB!", SPUTASKID, eaHeapOverfilled/1024));
	}

	return;
}// end of spuDrawTriPlants()...

#else //PSN_PLANTSMGR_SPU_RENDER...
void spuDrawTriPlants(::rage::sysTaskParameters &)
{
	return;
}
#endif //PSN_PLANTSMGR_SPU_RENDER...

#endif //__SPU....


