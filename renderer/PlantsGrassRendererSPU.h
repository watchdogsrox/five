//
// PlantsGrassRendererSPU.h - main body of SPU job for plants rendering;
//
//	30/04/2007	-	Andrzej:	- initial;
//
//
// 
#ifndef __PLANTSGRASSRENDERERSPU_H__
#define __PLANTSGRASSRENDERERSPU_H__

// Tasks should only depend on taskheader.h, not task.h (which is the dispatching system)
#include "system/taskheader.h"
#include "spatialdata/transposedplaneset.h"	// spdTransposedPlaneSet8
#include "Debug/debug.h"
#include "sdk_version.h"
#include "shader_source/Vegetation/Grass/grass_regs.h"

DECLARE_TASK_INTERFACE(PlantsGrassRendererSPU);
void spuDrawTriPlants(::rage::sysTaskParameters&);

class PPTriPlant;
struct grassModel;

#define PLANTSMGR_TRIPLANTTAB_IN_BLOCKS				(1 && PSN_PLANTSMGR_SPU_RENDER)	// if 1, then spuTriPlantTab is allocated in 128 blocks of 4KB each
#define PLANTSMGR_BIG_HEAP_IN_BLOCKS				(1 && PSN_PLANTSMGR_SPU_RENDER)	// if 1, then big heap is allocated in N blocks

#define PLANTSMGR_TRIPLANT_BLOCK_SIZE				(4*1024)	// spuTriPlantTab is allocated in chunks of 4KB each
#define PLANTSMGR_LOCAL_HEAP_SIZE					(8*1024)	// local SPU fifo size

#define PLANTSMGR_TRIPLANTTAB_SIZE0					(16*1024)	// initial 16KB (=4 blocks) for PPTriPlantTab

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	#define PLANTSMGR_BIG_HEAP_SIZE0				(16*1024)	// initial 16KB (=2 blocks) for external RSX commandbuffer
#else
	#define PLANTSMGR_BIG_HEAP_SIZE0				(768*1024)	// initial 768KB for external RSX commandbuffer
#endif

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	struct grassHeapBlock
	{
		u32		*ea;	// main ptr
		u32		offset;	// RSX offset
	};
	#define PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE				(160)	// max size of array of big heap blocks
#endif

#define PLANTSMGR_MAX_NUM_OF_RENDER_JOBS					(128)		// max num of simultaneous render jobs launched

#define PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE					(20)		// it usually takes 19+1 commands to set the whole texturestate
#define PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE					(76)
#if CPLANT_WRITE_GRASS_NORMAL
	#define PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE		(840)
	#define PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE		(257)
	#if GRASS_LOD2_BATCHING
		#define PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE	(246)
	#else
		#define PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE	(315)
	#endif
#else
	#define PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE		(752)
	#define PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE		(174)
	#if GRASS_LOD2_BATCHING
		#define PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE	(220)
	#else
		#define PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE	(232)
	#endif
#endif
// size (in bytes) of one full slot for all textureCmds and grassModels:
#define PLANTSMGR_LOCAL_SLOTSIZE_TEXTURE	(CPLANT_SLOT_NUM_TEXTURES*2*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE*sizeof(u32))
#define PLANTSMGR_LOCAL_SLOTSIZE_MODEL		(CPLANT_SLOT_NUM_MODELS*2*sizeof(grassModel))	


//
//
//
//
enum { spuCLIP_PLANE_LEFT, spuCLIP_PLANE_RIGHT, spuCLIP_PLANE_TOP, spuCLIP_PLANE_BOTTOM, spuCLIP_PLANE_NEAR, spuCLIP_PLANE_FAR} ;


//
//
//
//
struct spuGrassParamsStruct
{
	PPTriPlant			*m_pTriPlant;					// 1st ptr to PPTriPlant
	u32					m_pad32_0;
	u32					m_pad32_1;

	u16					m_numTriPlant;					// no of triplants to render
	u8					m_pad8_0[2];

	#define PSN_SIZEOF_SPUGRASSPARAMSTRUCT		(16)	// SPU: sizeof this struct must be dma'able
};


//
//
//
//
struct spuGrassParamsStructMaster
{
	u32						m_nNumGrassJobsLaunched;
	spuGrassParamsStruct	*m_inSpuGrassStructTab;
	u32						m_DeviceFrameCounter;			// GRCDEVICE.GetFrameCounter()
	u32						m_DecrementerTicks;				// amount of ticks used to run SPU task

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	grassHeapBlock			*m_heapBlockArray;
	u32						m_heapBlockArrayCount;
#else
	u32						m_heapBegin;
	u32						m_heapBeginOffset;
	u32						m_heapSize;
#endif
	u32						m_heapID;
	u32						m_rsxLabel5_CurrentTaskID;		// value of wait tag in Label4 for current job

	u32						m_eaMainJTS;					// EA of JTS in startupCB blocking jump to big heap 
	u32						m_ReturnOffsetToMainCB;			// return offset from big heap to main CB
	u32						m_AmountOfBigHeapConsumed;		// out: how much space consumed in big buffer
	u32						m_AmountOfBigHeapOverfilled;	// out: how much big buffer was overfilled

	u32						m_rsxLabel5Index;				// index of RSX Label1
	u32						m_eaRsxLabel5;					// ea of RSX Label1
	float					m_LOD0FarDist2;
	float					m_LOD1CloseDist2;
	
	float					m_LOD1FarDist2;
	float					m_LOD2CloseDist2;
	float					m_LOD2FarDist2;
	Vector2					m_windBending;

	u32						m_BindShaderLOD0Offset;			// RSX offset to call BindShaderLOD0
	u32						m_BindShaderLOD1Offset;			// RSX offset to call BindShaderLOD1
	u32						m_BindShaderLOD2Offset;			// RSX offset to call BindShaderLOD2
	u32						m_GeometryLOD2Offset;			// RSX offset to geometry cmds for LOD2

	grassModel*				m_PlantModelsTab;				// all geometries
	u32						m_PlantTexturesOffsetTab;		// all textures

	Vector4					m_vecCameraPos;
#if CPLANT_PRE_MULTIPLY_LIGHTING
	Vector4					m_mainDirectionalLight_natAmbient;
#endif
	
	spdTransposedPlaneSet8	m_cullFrustum;

#if CPLANT_DYNAMIC_CULL_SPHERES
	const PlantMgr_DynCullSphereArray *m_CullSphereArray;	//Array of dynamic culling spheres
#endif

#if __BANK
	// statistics & debug:
	u32					m_LocTriPlantsLOD0DrawnPerSlot;	// out
	u32					m_LocTriPlantsLOD1DrawnPerSlot;	// out
	u32					m_LocTriPlantsLOD2DrawnPerSlot;	// out

	u32					m_bGeomCullingTestEnabled	: 1;
	u32					m_bPremultiplyNdotL			: 1;
	u32					m_bPlantsFlashLOD0			: 1;
	u32					m_bPlantsFlashLOD1			: 1;
	u32					m_bPlantsFlashLOD2			: 1;
	u32					m_bPlantsDisableLOD0		: 1;
	u32					m_bPlantsDisableLOD1		: 1;
	u32					m_bPlantsDisableLOD2		: 1;
	u32					__pad00						: 24;
	u32					__pad01[3];
#endif //__BANK...
};


#endif //__PLANTSGRASSRENDERERSPU_H__...



