//
// PlantsGrassRenderer - constains lowlevel rendering classes for CPlantMgr: CGrassRenderer & CPPTriPlantBuffer;
//
//	30/06/2005	-	Andrzej:	- initial port from SA to Rage;
//
//
//
//
//
#ifndef __CPLANTGRASSRENDERER_H__
#define __CPLANTGRASSRENDERER_H__

struct CellGcmContextData;

#include "renderer\PlantsMgr.h"
#include "renderer\PlantsGrassRendererSwitches.h"

//
//
//
//
#define PPTRIPLANT_MODELS_TAB_SIZE				(4)

//
//
//
// 
#if PSN_PLANTSMGR_SPU_RENDER
	#define	PPTRIPLANT_BUFFER_SIZE				(36)	// 36*112=4032 (fits into 4KB buffer)
#else
	#define	PPTRIPLANT_BUFFER_SIZE				(64)
#endif
	
//
//
//
//
struct PPTriPlant
{
public:
    Vector4		V1, V2, V3;			// 3 vertices
//	V1.w = wind_bend_scale;			// wind bending scale
//	V2.w = wind_bend_var;			// wind bending variation

	Float16Vec4	packedScale;		// x = SxSy, y = Sz, z = scale_var_xy, w = scale_var_z
	Float16Vec4	um_param;
	Float16Vec2	scaleRangeXYZ_Z;	// x=scaleXYZ, y=scaleZ

	// 64
	u16				num_plants;		// num plants to generate
	atFixedBitSet16	flags;			// ProcPlantFlags: bitmask for enabled LODs and misc flags


#if PSN_PLANTSMGR_SPU_RENDER
	u8			texture_id;			// number of texture to use
	u8			textureLOD1_id;		// number of LOD1 texture to use
	
	u8			intensity;			// scale value for the colour
	u8			intensity_var;		// variation of intensity value
#else
	grcTexture	*texture_ptr;		// ptr to texture to use
	grcTexture	*textureLOD1_ptr;	// ptr to textureLOD1 to use

	u8			intensity;			// scale value for the colour
	u8			intensity_var;		// variation of intensity value
	
	u8			ambient_scl;

	u8			__pad00;
#endif
	

    Color32		color;				// color of the model
 
    u32			seed;				// seed starting value for this triangle;

  		
	Float16Vec2	coll_params;		// collision params:  [radiusSqr | invRadiusSqr]

	// 92
	Color32		groundColorV1, groundColorV2, groundColorV3;

	Float16Vec4	skewAxisAngle;		// ground skewing: xyz=skew axis, w=skew angle (if 0, then no skewing)

	s8			loctri_normal[3];	// packed loctri normal: x,y,z
    u8			model_id;			// model_id used to calculate offset address for model in VU mem

#if CPLANT_CLIP_EDGE_VERT
	bool		m_ClipEdge_01	:1;
	bool		m_ClipEdge_12	:1;
	bool		m_ClipEdge_20	:1;
	bool		m_ClipVert_0	:1;
	bool		m_ClipVert_1	:1;
	bool		m_ClipVert_2	:1;
	u8			__pad02			:2;
#endif

#if PSN_PLANTSMGR_SPU_RENDER
	#define PSN_SIZEOF_PPTRIPLANT			(112)	// SPU: size of this struct must be dma'able
#endif

public:
	static inline void GenPointInTriangle(Vector3 *pRes, const Vector3 *V1, const Vector3 *V2, const Vector3 *V3, float s, float t);
	static inline void GenPointInTriangle(Vector4 *pRes, const Vector4 *V1, const Vector4 *V2, const Vector4 *V3, float s, float t);
	static inline void ApplyGroundDensity(float& s, float& t, float densityS, float densityT);
};

//
//
// translated from Andrez's VU code...
//
// Based on Turk's algorithm:
//
// s = rand[0; 1]
// t = rand[0; 1]
//
inline
void PPTriPlant::GenPointInTriangle(Vector3 *pRes, const Vector3 *V1, const Vector3 *V2, const Vector3 *V3, float s, float t)
{
	if((s+t) > 1.0f)
	{
		s = 1.0f - s;
		t = 1.0f - t;
	}
	
	const float a = s;
	const float b = t;
	const float c = 1.0f - s - t;

	pRes->x = a*V1->x + b*V2->x + c*V3->x;
	pRes->y = a*V1->y + b*V2->y + c*V3->y;
	pRes->z = a*V1->z + b*V2->z + c*V3->z;
}

inline
void PPTriPlant::GenPointInTriangle(Vector4 *pRes, const Vector4 *V1, const Vector4 *V2, const Vector4 *V3, float s, float t)
{
	if((s+t) > 1.0f)
	{
		s = 1.0f - s;
		t = 1.0f - t;
	}

	const float a = s;
	const float b = t;
	const float c = 1.0f - s - t;

	pRes->x = a*V1->x + b*V2->x + c*V3->x;
	pRes->y = a*V1->y + b*V2->y + c*V3->y;
	pRes->z = a*V1->z + b*V2->z + c*V3->z;
	pRes->w = a*V1->w + b*V2->w + c*V3->w;
}

//
//
// note: denistyS and densityT must be 2 smallest densities:
//
inline
void PPTriPlant::ApplyGroundDensity(float& s, float& t, float densityS, float densityT)
{
	if((s+t) > 1.0f)
	{
		s = 1.0f - s;
		t = 1.0f - t;
	}
	
	s *= densityS;
	t *= densityT;
}
#if !__SPU
class CPPTriPlantBuffer;	// forward definition


enum CGrassRenderer_PlantLod
{
	PLANTLOD_LOW=0,
	PLANTLOD_MEDIUM,
	PLANTLOD_HIGH,
	PLANTLOD_COUNT
};


class CGrassShadowParams
{
public:
	CGrassShadowParams()
	{
		fadeNear = 10.0f;
		fadeFar = 30.0f;
	}
	~CGrassShadowParams()
	{
	}
public:
	float fadeNear;
	float fadeFar;
	PAR_SIMPLE_PARSABLE;
};


class CGrassShadowParams_AllLODs
{
public:
	CGrassShadowParams_AllLODs()
	{
	}
	~CGrassShadowParams_AllLODs()
	{
	}
public:
	CGrassShadowParams m_AllLods[PLANTLOD_COUNT];
	PAR_SIMPLE_PARSABLE;
};


class CGrassScalingParams
{
public:
	CGrassScalingParams()
	{
		distanceMultiplier = 1.0f;
		densityMultiplier = 1.0f;
	}
	~CGrassScalingParams()
	{
	}
public:
	float distanceMultiplier;
	float densityMultiplier;
	PAR_SIMPLE_PARSABLE;
};

class CGrassScalingParams_AllLODs
{
public:
	CGrassScalingParams_AllLODs()
	{
	}
	~CGrassScalingParams_AllLODs()
	{
	}
public:
	CGrassScalingParams m_AllLods[PLANTLOD_COUNT];
	PAR_SIMPLE_PARSABLE;
};


//
//
//
//
class CGrassRenderer
{
	friend class CPlantMgr;
	friend class CPPTriPlantBuffer;

private:
	struct instGrassDataDrawStruct
	{
		Matrix44	m_WorldMatrix;
		Color32		m_PlantColor32;
		Color32		m_GroundColorAmbient32;
	};

public:

#if GRASS_INSTANCING
	#if PLANTSMGR_MULTI_RENDER
		#define CGRASS_RENDERER_LOD_BUCKET_SIZE		(24)
	#else
		#define CGRASS_RENDERER_LOD_BUCKET_SIZE		(16)
	#endif

	class InstanceBucket
	{
	public:
		InstanceBucket();
		virtual ~InstanceBucket();

	public: 
		void Init(grmShader *pShader, grcEffectVar textureVar);
		void ShutDown();

	public:
#if PLANTSMGR_MULTI_RENDER
		void Reset(u8 rti, grcEffectTechnique techId);
		bool AddInstance(u8 rti, bool bMainRT, grmGeometry *pGeometry, grcTexture *pTexture, struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams);
		virtual void Flush(u8 rti);
		bool IsFull(u8 rti)				{ return(m_PerThreadData0[rti].m_noOfInstances == GRASS_INSTANCING_BUCKET_SIZE); }
		static float PackColour(Vector4 &c);

		grmGeometry* GetGeometry(u8 rti) { return m_PerThreadData0[rti].m_pCurrentGeometry; }
		grcTexture* GetTexture(u8 rti) { return m_PerThreadData0[rti].m_pCurrentTexture; }
		u32 GetCount(u8 rti) { return m_PerThreadData0[rti].m_noOfInstances; }
#else //PLANTSMGR_MULTI_RENDER...
		void Reset(grcEffectTechnique techId);
		void AddInstance(grmGeometry *pGeometry, grcTexture *pTexture, struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams);


		__forceinline void BeginInstance(grmGeometry *pGeometry, grcTexture *pTexture)
		{
			// Have we reached capacity or is the geometry/texture different ?
			if(IsFull() || (pGeometry != m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry) || (pTexture != m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture))
			{
				// Send all instances.
				Flush();

				// Record texture/geometry.
				m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture = pTexture;
				m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry = pGeometry;
			}
		}
		
		__forceinline void LockTexture()
		{
			if(!m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked)
			{
				if (LockTransport())
					m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked = true;
			}
		}

		__forceinline void AddInstanceDirect(struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams)
		{
			if(IsFull())
			{
				Flush();
				LockTexture();
			}

			Vector4 *pDest = GetDataPtr();

			// Pack the instance data.
			Vector4 C0 = pInstance->m_WorldMatrix.a;
			Vector4 C1 = pInstance->m_WorldMatrix.b;
			Vector4 C2 = pInstance->m_WorldMatrix.c;
			Vector4 C3 = pInstance->m_WorldMatrix.d;

			*((Color32*)(&C0.w)) = pInstance->m_PlantColor32;					// pack directly as u32
			*((Color32*)(&C1.w)) = pInstance->m_GroundColorAmbient32;			// pack directly as u32

			C2.w = collParams.GetXf();
			C3.w = collParams.GetYf();

			pDest[0] = C0;
			pDest[1] = C1;
			pDest[2] = C2;
			pDest[3] = C3;
			pDest[4] = VEC4V_TO_VECTOR4(umParams);

			// Increment the count.
			m_PerThreadData[g_RenderThreadIndex].m_noOfInstances++;
		}

		virtual void Flush();
		bool IsFull()					{ return(m_PerThreadData[g_RenderThreadIndex].m_noOfInstances == GRASS_INSTANCING_BUCKET_SIZE); }
		static float PackColour(Vector4 &c);

		grmGeometry *GetGeometry() { return m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry; }
		grcTexture *GetTexture() { return m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture; }
		u32 GetCount() { return m_PerThreadData[g_RenderThreadIndex].m_noOfInstances; }
#endif //PLANTSMGR_MULTI_RENDER...

	protected:
#if PLANTSMGR_MULTI_RENDER
		void InitialiseTransport();
		void CleanUpTransport();
		#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
			bool LockTransport(u8) {};
			void UnlockTransport() {};
			Vector4* GetDataPtr(u8 rti)
			{
				Vector4 *pDest = &m_pLockBase0[rti][ m_PerThreadData0[rti].m_noOfInstances*GRASS_INSTANCING_INSTANCE_SIZE ];
				return pDest;
			}
		#else
			bool LockTransport(u8 rti);
			void UnlockTransport();
			Vector4 *GetDataPtr(u8 rti);
		#endif
		void BindTransport(u8 rti);
#else //PLANTSMGR_MULTI_RENDER...
		void InitialiseTransport();
		void CleanUpTransport();
		#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
			bool LockTransport() { return true; };
			void UnlockTransport() {};
			__forceinline Vector4* GetDataPtr()
			{
				Vector4 *pDest = &m_pLockBase[g_RenderThreadIndex][m_PerThreadData[g_RenderThreadIndex].m_noOfInstances*GRASS_INSTANCING_INSTANCE_SIZE];
				return pDest;
			}
		#else
			bool LockTransport();
			void UnlockTransport();
			Vector4* GetDataPtr();
		#endif

		void BindTransport();
#endif //PLANTSMGR_MULTI_RENDER...

	#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
		void UnbindTransport()
		{
		#if RSG_PC && __D3D11
			GRCDEVICE.SetVertexShaderConstantBufferOverrides(0, NULL, 0);
		#endif
		}
	#else
		void UnbindTransport();
	#endif

	protected:
		grmShader *m_pShader;
		grcEffectVar m_textureVar;

		struct PER_THREAD_DATA
		{
			grcEffectTechnique m_techId;
			u32	m_noOfInstances;
			bool m_isInstanceTextureLocked;
			grmGeometry *m_pCurrentGeometry;
			grcTexture *m_pCurrentTexture;
		};

	#if PLANTSMGR_MULTI_RENDER
		PER_THREAD_DATA m_PerThreadData0[NUMBER_OF_RENDER_THREADS];
	#else
		PER_THREAD_DATA m_PerThreadData[NUMBER_OF_RENDER_THREADS];
	#endif

	#if GRASS_INSTANCING_TRANSPORT_TEXTURE
		grcEffectVar m_instanceTextureVar;
		grcTexture *m_pInstanceTexture;
		grcTextureLock m_instanceTextureLock[NUMBER_OF_RENDER_THREADS];
	#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

	#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
		u32 m_VertexShaderRegister;
		grcCBuffer *m_pCBuffer;
		#if PLANTSMGR_MULTI_RENDER
			Vector4 *m_pLockBase0[NUMBER_OF_RENDER_THREADS];
		#else
			Vector4 *m_pLockBase[NUMBER_OF_RENDER_THREADS];
		#endif
	#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
	};

	class InstanceBucket_LOD2 : public InstanceBucket
	{
	public:
		InstanceBucket_LOD2();
		virtual ~InstanceBucket_LOD2();
	public:
	#if PLANTSMGR_MULTI_RENDER
		void Flush(u8 rti);
	#else
		void Flush();
	#endif
	};

	template <u32 N>
	class LODBucket
	{
	public:
		LODBucket()
		{
			u32 i;
			for(i=0; i<N; i++)
			{
				m_pInstanceBuckets[i] = NULL;
			}
		}
		~LODBucket()
		{
		}
	public:
		void SetBucket(u32 idx, InstanceBucket *pBucket)
		{
			Assert(idx < N);
			m_pInstanceBuckets[idx] = pBucket;
		}

#if PLANTSMGR_MULTI_RENDER
		void Reset(u8 rti, grcEffectTechnique techId)
		{
			for(s32 i=0; i<N; i++)
			{
				m_pInstanceBuckets[i]->Reset(rti, techId);
			}
		}

		void Flush(u8 rti)
		{
			for(s32 i=0; i<N; i++)
			{
				m_pInstanceBuckets[i]->Flush(rti);
			}
		}

		void FlushAlmostFull(u8 rti)
		{
			u32 maxAmount = 0;
			s32 maxIdx = -1;
			u32 minAmount = 0xffffffff;
			s32 minIdx = -1;
			bool bFlushed = false;
			
			for(s32 i=0; i<N; i++)
			{
				//if(m_pInstanceBuckets[i]->IsFull(rti))
				if(m_pInstanceBuckets[i]->GetCount(rti) >= u32(GRASS_INSTANCING_BUCKET_SIZE*0.80f))	// flush if 80% full
				{
					m_pInstanceBuckets[i]->Flush(rti);
					bFlushed = true;
				}

				const u32 bucketCount = m_pInstanceBuckets[i]->GetCount(rti);
				if(bucketCount > maxAmount)
				{
					maxAmount = bucketCount;
					maxIdx = i;
				}

				if(bucketCount < minAmount)
				{
					minAmount = bucketCount;
					minIdx = i;
				}
			}

			// flush the one with most instances if nobody got flushed and no more empty buckets:
			if((!bFlushed) && (maxIdx!=-1) && (minAmount!=0))
			{
				m_pInstanceBuckets[maxIdx]->Flush(rti);
			}
		}

		bool AddInstance(u8 rti, bool bMainRT, grmGeometry *pGeometry, grcTexture *pTexture, struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams)
		{
			for(u32 i=0; i<N; i++)
			{
				// Do we have a bucket for the geometry/texture combination ongoing ?
				if((m_pInstanceBuckets[i]->GetGeometry(rti) == pGeometry) && (m_pInstanceBuckets[i]->GetTexture(rti) == pTexture))
				{
					if(bMainRT)
					{	// MainRT: Use it.
						return m_pInstanceBuckets[i]->AddInstance(rti, bMainRT, pGeometry, pTexture, pInstance, umParams, collParams);
					}
					else
					{
						// subtask: find another not full
						if(!m_pInstanceBuckets[i]->IsFull(rti))
						{
							return m_pInstanceBuckets[i]->AddInstance(rti, bMainRT, pGeometry, pTexture, pInstance, umParams, collParams);
						}
					}
				}
			}

			u32 minCount = 0xffffffff;
			u32 minCountIdx = 0;
			u32 maxCount = 0;
			u32 maxCountIdx = 0;

			// Find a new bucket to use.
			for(u32 i=0; i<N; i++)
			{
				if(m_pInstanceBuckets[i]->GetCount(rti) < minCount)
				{
					minCount = m_pInstanceBuckets[i]->GetCount(rti);
					minCountIdx = i;
				}

				if(m_pInstanceBuckets[i]->GetCount(rti) > maxCount)
				{
					maxCount = m_pInstanceBuckets[i]->GetCount(rti);
					maxCountIdx = i;
				}
			}

			if(minCount == 0)
			{
				// Start a new bucket.
				return m_pInstanceBuckets[minCountIdx]->AddInstance(rti, bMainRT, pGeometry, pTexture, pInstance, umParams, collParams);
			}
			else
			{
				if(bMainRT)
				{	// only MainRT: "Evict" the most full bucket.
					return m_pInstanceBuckets[maxCountIdx]->AddInstance(rti, bMainRT, pGeometry, pTexture, pInstance, umParams, collParams);
				}
				else
				{
					// subtask fails:
					return(false);
				}
			}
		}
#else //PLANTSMGR_MULTI_RENDER...
		void Reset(grcEffectTechnique techId)
		{
			u32 i;

			for(i=0; i<N; i++)
			{
				m_pInstanceBuckets[i]->Reset(techId);
			}
		}
		void Flush()
		{
			u32 i;

			for(i=0; i<N; i++)
			{
				m_pInstanceBuckets[i]->Flush();
			}
		}

		void AddInstance(grmGeometry *pGeometry, grcTexture *pTexture, struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams)
		{
			u32 i;
			u32 minCount = 0xffffffff;
			u32 minCountIdx = 0;
			u32 maxCount = 0;
			u32 maxCountIdx = 0;

			for(i=0; i<N; i++)
			{
				// Do we have a bucket for the geometry/texture combination ongoing ?
				if((m_pInstanceBuckets[i]->GetGeometry() == pGeometry) && (m_pInstanceBuckets[i]->GetTexture() == pTexture))
				{
					// Use it.
					m_pInstanceBuckets[i]->AddInstance(pGeometry, pTexture, pInstance, umParams, collParams);
					return;
				}
			}

			// Find a new bucket to use.
			for(i=0; i<N; i++)
			{
				if(m_pInstanceBuckets[i]->GetCount() < minCount)
				{
					minCount = m_pInstanceBuckets[i]->GetCount();
					minCountIdx = i;
				}
				if(m_pInstanceBuckets[i]->GetCount() > maxCount)
				{
					maxCount = m_pInstanceBuckets[i]->GetCount();
					maxCountIdx = i;
				}
			}

			if(minCount == 0)
			{
				// Start a new bucket.
				m_pInstanceBuckets[minCountIdx]->AddInstance(pGeometry, pTexture, pInstance, umParams, collParams);
			}
			else
			{
				// "Evict" the most full bucket.
				m_pInstanceBuckets[maxCountIdx]->AddInstance(pGeometry, pTexture, pInstance, umParams, collParams);
			}
		}



		// NEW_STORE_OPT:
		InstanceBucket* FindBucket(grmGeometry *pGeometry, grcTexture *pTexture)
		{
			for(u32 i=0; i<N; i++)
			{
				// Do we have a bucket for the geometry/texture combination ongoing ?
				if((m_pInstanceBuckets[i]->GetGeometry() == pGeometry) && (m_pInstanceBuckets[i]->GetTexture() == pTexture))
				{
					// Use it.
					m_pInstanceBuckets[i]->LockTexture();
					return m_pInstanceBuckets[i];
				}
			}

			u32 minCount = 0xffffffff;
			u32 minCountIdx = 0;
			u32 maxCount = 0;
			u32 maxCountIdx = 0;

			// Find a new bucket to use.
			for(u32 i=0; i<N; i++)
			{
				const u32 count = m_pInstanceBuckets[i]->GetCount();
				if(count < minCount)
				{
					minCount = count;
					minCountIdx = i;
				}

				if(count > maxCount)
				{
					maxCount = count;
					maxCountIdx = i;
				}
			}

			if(minCount == 0)
			{
				// Start a new bucket.
				m_pInstanceBuckets[minCountIdx]->BeginInstance(pGeometry, pTexture);
				m_pInstanceBuckets[minCountIdx]->LockTexture();
				return m_pInstanceBuckets[minCountIdx];
			}
			else
			{
				// "Evict" the most full bucket.
				m_pInstanceBuckets[maxCountIdx]->BeginInstance(pGeometry, pTexture);
				m_pInstanceBuckets[maxCountIdx]->LockTexture();
				return m_pInstanceBuckets[maxCountIdx];
			}
		}
#endif //PLANTSMGR_MULTI_RENDER...

	private:
		InstanceBucket *m_pInstanceBuckets[N];
	};

#endif // GRASS_INSTANCING

public:
	enum { NUM_COL_VEH = 4	};	// support up to 4 vehicles deforming grass

public:
	CGrassRenderer();
	~CGrassRenderer();

public:
	static bool 			Initialise();
	static void 			Shutdown();
	static bool				UpdateStr();

#if	PSN_PLANTSMGR_SPU_RENDER
	static bool				SpuInitialisePerRenderFrame();
	static bool				SpuFinalisePerRenderFrame();
	static bool				SpuUpdateStrStats(spuGrassParamsStructMaster *p);
	static bool				SpuRecordGrassTexture(u32 texIndex, grcTexture *pTexture);
	static bool				SpuRecordGeometries();
	static u32				SpuRecordGeometry(CellGcmContextData *con1, grmGeometry *pGeometry, u32 *dstCmd, u32 maxCmdSize);
	static u32				SpuRecordGeometry(CellGcmContextData *con1, grcVertexBuffer* pVertexBuffer, grcVertexDeclaration* pVertexDeclaration, u32 *dstCmd, u32 maxCmdSize);
	static bool				SpuRecordShaders(u32 bufID);
	static u32				SpuRecordShaderBind(CellGcmContextData *con1, grcEffectTechnique forcedTech, u32 *dstCmd, u32 maxCmdSize);
	static u32				SpuRecordShaderUnBind(CellGcmContextData *con1);
	static u32				SpuGetAmountOfJobs()															{ return sm_nNumGrassJobsAddedTotal;}
	static u32				SpuGetAmountOfAllocatedJobs()													{ return spuTriPlantBlockTabCount;	}
	static u32				SpuGetBigHeapAllocatedSize()													{ return(sm_BigHeapAllocatedSize[sm_BigHeapID]);	}
	static u32				SpuGetBigHeapConsumed()															{ return(sm_BigHeapConsumed[sm_BigHeapID]);			}
	static u32				SpuGetBigHeapBiggestConsumed()													{ return(sm_BigHeapBiggestConsumed[sm_BigHeapID]);	}
	static u32				SpuGetBigHeapOverfilled()														{ return(sm_BigHeapOverfilled[sm_BigHeapID]);		}
#else
	static bool				SpuInitialisePerRenderFrame()													{ return(TRUE); }
	static bool				SpuFinalisePerRenderFrame()														{ return(TRUE);	}
	static bool				SpuRecordGrassTexture(u32,grcTexture*)											{ return(TRUE); }
	static bool				SpuRecordGeometries()															{ return(TRUE);	}
	static u32				SpuRecordGeometry(void*,grmGeometry*,u32*,u32)									{ return(TRUE);	}
	static u32				SpuRecordGeometry(void*,grcVertexBuffer*,grcVertexDeclaration*,u32*,u32)		{ return(TRUE);	}
	static bool				SpuRecordShaders(u32)															{ return(TRUE);	}
	static u32				SpuRecordShaderBind(void*, grcEffectTechnique, u32*, u32)						{ return(TRUE);	}
	static u32				SpuRecordShaderUnBind(void*)													{ return(TRUE);	}
	static u32				SpuGetAmountOfJobs()															{ return(0);	}
	static u32				SpuGetAmountOfAllocatedJobs()													{ return(0);	}
	static u32				SpuGetBigHeapAllocatedSize()													{ return(0);	}
	static u32				SpuGetBigHeapConsumed()															{ return(0);	}
	static u32				SpuGetBigHeapBiggestConsumed()													{ return(0);	}
	static u32				SpuGetBigHeapOverfilled()														{ return(0);	}
#endif

public:
	static void				AddTriPlant(PPTriPlant *pPlant, u32 ePlantModelSet);
	static void				FlushTriPlantBuffer();
#if !PSN_PLANTSMGR_SPU_RENDER
	static void				BeginUseNormalTechniques(spdTransposedPlaneSet8 &cullFrustum);
	static void				EndUseNormalTechniques();
#endif

#if PLANTS_CAST_SHADOWS
#if !PSN_PLANTSMGR_SPU_RENDER
	static void				BeginUseShadowTechniques(spdTransposedPlaneSet8 &cullFrustum);
	static void				EndUseShadowTechniques();
#endif
#endif //PLANTS_CAST_SHADOWS


private:
#if PSN_PLANTSMGR_SPU_RENDER
	static bool				DrawTriPlantsSPU(PPTriPlant *triPlants, const u32 numTriPlants, u32 slotID);
#else
	static bool 			DrawTriPlants(PPTriPlant *triPlants, const u32 numTriPlants, grassModel* plantModelsTab);
	#if PLANTSMGR_MULTI_RENDER
	public:
		struct CParamsDrawTriPlantsMulti
		{
			Vector3	m_CameraPos;
			Vector3 m_PlayerPos;
			Vector3 m_SunDir;
			Vector2	m_WindBending;
			float	m_NaturalAmbient;

			float	m_LOD0FarDist2;
			float	m_LOD1CloseDist2;
			float	m_LOD1FarDist2;
			float	m_LOD2CloseDist2;
			float	m_LOD2FarDist2;
		};

		static bool 		DrawTriPlantsMulti(const u8 rti, const bool bMainRT, PPTriPlant *triPlants, const u32 numTriPlants, grassModel* plantModelsTab,
												CParamsDrawTriPlantsMulti *pParamsDrawMulti);
	private:
	#endif //PLANTSMGR_MULTI_RENDER...
#endif //PSN_PLANTSMGR_SPU_RENDER...
	static void				BindPlantShader(grcEffectTechnique forcedTech);
	static void				UnBindPlantShader();

public:
	static bool				SetPlantModelsTab(u32 index, grassModel *plantModels);
	static grassModel*		GetPlantModelsTab(u32 index);

public:
	static void				SetGlobalCameraPos(const Vector3& camPos);
	static const Vector3&	GetGlobalCameraPos();
	static void				SetGlobalCameraFront(const Vector3& camPos);
	static const Vector3&	GetGlobalCameraFront();
	static void				SetGlobalCameraFppEnabled(bool b);
	static bool				GetGlobalCameraFppEnabled();
	static void				SetGlobalCameraUnderwater(bool b);
	static bool				GetGlobalCameraUnderwater();
	static void				SetGlobalForceHDGrassGeometry(bool b);
	static bool				GetGlobalForceHDGrassGeometry();
	
	static void				SetGlobalPlayerPos(const Vector3& playerPos);
	static const Vector3&	GetGlobalPlayerPos();
	
	static void				SetGlobalPlayerCollisionRSqr(float radSqr);
	static float			GetGlobalPlayerCollisionRSqr();

	static void				SetGlobalCullFrustum(const spdTransposedPlaneSet8& frustum);
	static void				GetGlobalCullFrustum(spdTransposedPlaneSet8 *frustum);

	static void				SetGlobalInteriorCullAll(bool intCullAll);
	static bool				GetGlobalInteriorCullAll();

	static void				SetGlobalVehCollisionParams(u32 n, bool bEnable, const Vector3& vecB, const Vector3& vecM, float radius, float groundZ);
	static void				GetGlobalVehCollisionParams(u32 n, bool *bEnable, Vector4 *vecB, Vector4 *vecM, Vector4 *vecR);

	static void				SetGlobalWindBending(const Vector2& bending);
	static void				GetGlobalWindBending(Vector2 &outBending);
	static void 			GetGlobalWindBendingUT(Vector2 &outBending);

	static void				SetFakeGrassNormal(const Vector3& n);
	static const Vector3&	GetFakeGrassNormal();
#if __BANK
	static void				SetDebugModeEnable(bool enable);
	static bool				GetDebugModeEnable();
#endif
#if FURGRASS_TEST_V4
	static void				SetGlobalPlayerFeetPos(const Vector4& LFootPos, const Vector4& RFootPos);
	static const Vector4&	GetGlobalPlayerLFootPos();
	static const Vector4&	GetGlobalPlayerRFootPos();
#endif //FURGRASS_TEST_V4...

#if __BANK
	static void				InitWidgets(bkBank& bank);
#endif

#if PLANTS_USE_LOD_SETTINGS
	static float			GetDistanceMultiplier();
	static void				SetDistanceMultiplier( float d ){ ms_currentScalingParams.distanceMultiplier = d; }
	static float			GetDensityMultiplier();
	static void				SetPlantLODToUse(CGrassRenderer_PlantLod plantLOD);
#if __BANK
	static void				OnComboBox_LODToEdit(void);
	static void				SaveParams(void);
	static void				LoadParams(void);
#endif
#endif //PLANTS_USE_LOD_SETTINGS

#if PLANTS_CAST_SHADOWS
	static void				SetDepthBiasOn(); 
	static void				SetDepthBiasOff();
	static float			GetShadowFadeNearRadius();
	static float			GetShadowFadeFarRadius();
#endif //PLANTS_CAST_SHADOWS

public:
	static void				SetGeometryLOD2(grcVertexBuffer	*vb, grcVertexDeclaration *decl);

	static grmShaderGroup*		GetGrassShaderGroup();
	static grmShader*			GetGrassShader();
	static grcEffectTechnique	GetFakedGBufTechniqueID();
	static grcEffectVar			GetGBuf0TextureID();

private:
	static Vector3				ms_vecCameraPos[2];
	static Vector3				ms_vecCameraFront[2];
	static Vector3				ms_vecPlayerPos[2];
	static float				ms_fPlayerCollRSqr[2];
	static bool					ms_cameraFppEnabled[2];
	static bool					ms_cameraUnderwater[2];
	static bool					ms_forceHdGrassGeom[2];

	static spdTransposedPlaneSet8	ms_cullFrustum[2];
	static bool						ms_interiorCullAll[2];

	static bool					ms_bVehCollisionEnabled[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH];
	static Vector4				ms_vecVehCollisionB[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH];
	static Vector4				ms_vecVehCollisionM[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH];
	static Vector4				ms_vecVehCollisionR[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH];

	static Vector2				ms_windBending[2];
	static Vector3				ms_fakeGrassNormal[2];
#if __BANK
	static bool					ms_enableDebugDraw[2];
#endif
#if FURGRASS_TEST_V4
	static Vector4				ms_vecPlayerLFootPos[2];
	static Vector4				ms_vecPlayerRFootPos[2];
#endif

	// LOD2 vertex buffer:
	static grcVertexBuffer		*ms_plantLOD2VertexBuffer;
	static grcVertexDeclaration	*ms_plantLOD2VertexDecl;


	static grmShaderGroup		*ms_ShaderGroup;
	static grmShader			*ms_Shader;
	static grcEffectVar			ms_shdTextureID;
	static grcEffectVar			ms_shdGBuf0TextureID;
#if PSN_PLANTSMGR_SPU_RENDER
	static u32					ms_shdTextureTexUnit;
#endif

	static grcEffectVar			ms_grassRegTransform;
	static grcEffectVar			ms_grassRegPlantCol;
	static grcEffectVar			ms_grassRegGroundCol;

	static grcEffectVar			ms_shdCameraPosID;
	static grcEffectVar			ms_shdPlayerPosID;
	static grcEffectGlobalVar	ms_shdVehCollisionEnabledID[NUM_COL_VEH];
	static grcEffectVar			ms_shdVehCollisionBID[NUM_COL_VEH];
	static grcEffectVar			ms_shdVehCollisionMID[NUM_COL_VEH];
	static grcEffectVar			ms_shdVehCollisionRID[NUM_COL_VEH];
	static grcEffectVar			ms_shdFadeAlphaDistUmTimerID;
	static grcEffectVar			ms_shdFadeAlphaLOD1DistID;
	static grcEffectVar			ms_shdFadeAlphaLOD2DistID;
	static grcEffectVar			ms_shdFadeAlphaLOD2DistFarID;

#if !PSN_PLANTSMGR_SPU_RENDER
	static grcEffectVar			ms_shdPlantColorID;
	static grcEffectVar			ms_shdUMovementParamsID;
	static grcEffectVar			ms_shdDimensionLOD2ID;
	static grcEffectVar			ms_shdCollParamsID;
#endif
	static grcEffectVar			ms_shdFakedGrassNormal;
#if CPLANT_WRITE_GRASS_NORMAL
	static grcEffectVar			ms_shdTerrainNormal;
#endif
#if 0 && __DEV && __PPU
	static grcEffectVar			ms_shdGBuf0AlphaScaleID;
#endif
#if DEVICE_MSAA
	static grcEffectVar			ms_shdAlphaToCoverageScale;
#endif
	static grcEffectTechnique	ms_shdDeferredLOD0TechniqueID;
	static grcEffectTechnique	ms_shdDeferredLOD1TechniqueID;
	static grcEffectTechnique	ms_shdDeferredLOD2TechniqueID;
#if __BANK
	static grcEffectTechnique	ms_shdDeferredLOD0DbgTechniqueID;
	static grcEffectTechnique	ms_shdDeferredLOD1DbgTechniqueID;
	static grcEffectTechnique	ms_shdDeferredLOD2DbgTechniqueID;
#endif
	static grcEffectTechnique	ms_shdBlitFakedGBufID;

#if !PSN_PLANTSMGR_SPU_RENDER
	static DECLARE_MTR_THREAD grcEffectTechnique	ms_shdDeferredLOD0TechniqueID_ToUse;
	static DECLARE_MTR_THREAD grcEffectTechnique	ms_shdDeferredLOD1TechniqueID_ToUse;
	static DECLARE_MTR_THREAD grcEffectTechnique	ms_shdDeferredLOD2TechniqueID_ToUse;
	static spdTransposedPlaneSet8 ms_cullingFrustum[NUMBER_OF_RENDER_THREADS];
#endif

#if GRASS_INSTANCING
	static InstanceBucket		ms_instanceBucketsLOD0[CGRASS_RENDERER_LOD_BUCKET_SIZE];
	static InstanceBucket		ms_instanceBucketsLOD1[CGRASS_RENDERER_LOD_BUCKET_SIZE];
	static InstanceBucket_LOD2	ms_instanceBucketsLOD2[CGRASS_RENDERER_LOD_BUCKET_SIZE];

	static LODBucket < CGRASS_RENDERER_LOD_BUCKET_SIZE > ms_LOD0Bucket;
	static LODBucket < CGRASS_RENDERER_LOD_BUCKET_SIZE > ms_LOD1Bucket;
	static LODBucket < CGRASS_RENDERER_LOD_BUCKET_SIZE > ms_LOD2Bucket;
#endif // GRASS_INSTANCING

#if PLANTS_CAST_SHADOWS
#if __BANK
	static bool					ms_drawShadows;
#endif
	static grcEffectGlobalVar	ms_depthValueBias;
	static grcEffectTechnique	ms_shdDeferredLOD0ShadowTechniqueID;
	static grcEffectTechnique	ms_shdDeferredLOD1ShadowTechniqueID;
	static grcEffectTechnique	ms_shdDeferredLOD2ShadowTechniqueID;

	static CGrassShadowParams	ms_currentShadowEnvelope;
#if PLANTS_USE_LOD_SETTINGS
	static CGrassShadowParams_AllLODs ms_shadowEnvelope_AllLods;
#endif
#endif //PLANTS_CAST_SHADOWS

#if PLANTS_USE_LOD_SETTINGS
	static CGrassScalingParams	ms_currentScalingParams;
	static CGrassScalingParams_AllLODs ms_scalingParams_AllLods;
	static int					ms_LODToInitialiseWith;
	static bool					ms_isInitialized;
#if __BANK
	static int					ms_LODBeingEdited;
	static int					ms_LODNextToEdit;
#endif
#endif

#if PSN_PLANTSMGR_SPU_RENDER
	static u32					sm_nNumGrassJobsAdded;
	static u32					sm_nNumGrassJobsAddedTotal;
	static sysTaskHandle		sm_GrassTaskHandle;
	static u32					sm_BigHeapID;
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	static grassHeapBlock		sm_BigHeapBlockArray[2][PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE] ALIGNED(128);	// 64*8 = 512 bytes
	static s16					sm_BigHeapBlockArrayCount[2];	// how many valid block allocated
	static bool					BigHeapBlockArrayContainsOffset(u32 heapID, u32 blockSize, u32 offset);
#else
	static u32*					sm_BigHeap[2];
	static u32					sm_BigHeapOffset[2];
	static u32					sm_BigHeapSize[2];
#endif
	static u32					sm_BigHeapAllocatedSize[2], sm_BigHeapConsumed[2], sm_BigHeapBiggestConsumed[2], sm_BigHeapOverfilled[2];

	#if SPU_GCM_FIFO
		static u32*				sm_StartupHeapPtr[2];
		static u32				sm_StartupHeapOffset[2];
	#endif

	static u32*					gpShaderLOD0Cmd0[2];
	static u32*					gpShaderLOD1Cmd0[2];
	static u32*					gpShaderLOD2Cmd0[2];
	static u32					gpShaderLOD0Offset[2];
	static u32					gpShaderLOD1Offset[2];
	static u32					gpShaderLOD2Offset[2];
	static u32*					gpPlantLOD2GeometryCmd0;
	static u32					gpPlantLOD2GeometryOffset;
	
	static u32					rsxLabel5Index;
	static volatile u32*		rsxLabel5Ptr;
	static u32					rsxLabel5_CurrentTaskID;

#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
	static PPTriPlant*			spuTriPlantBlockTab[PLANTSMGR_MAX_NUM_OF_RENDER_JOBS];
	static s32					spuTriPlantBlockTabCount;
#else
	static PPTriPlant			spuTriPlantTab[PLANTSMGR_MAX_NUM_OF_RENDER_JOBS][PPTRIPLANT_BUFFER_SIZE]	ALIGNED(128);
#endif
	static spuGrassParamsStruct	inSpuGrassStructTab[PLANTSMGR_MAX_NUM_OF_RENDER_JOBS]						ALIGNED(128);
#endif //PSN_PLANTSMGR_SPU_RENDER...
};




//
//
// 
//
enum
{
	PPPLANTBUF_MODEL_SET0	= 0,
	PPPLANTBUF_MODEL_SET1	= 1,
	PPPLANTBUF_MODEL_SET2	= 2,
	PPPLANTBUF_MODEL_SET3	= 3	
};


//
//
//
//
class CPPTriPlantBuffer
{
public:
	CPPTriPlantBuffer();
	~CPPTriPlantBuffer();

public:
	void			Flush();

	PPTriPlant*		GetPPTriPlantPtr(s32 amountToAdd=1);

	void			ChangeCurrentPlantModelsSet(s32 newSet);
	void			IncreaseBufferIndex(s32 pipeMode, s32 amount=1);


public:
	//
	// current plant models set:
	//
	void			SetPlantModelsSet(s32 set)			{ this->m_PerThreadData[g_RenderThreadIndex].m_plantModelsSet=set;		}
	s32				GetPlantModelsSet()					{ return(this->m_PerThreadData[g_RenderThreadIndex].m_plantModelsSet);	}

	grassModel*		GetPlantModels(u32 i)				{ Assert(i<PPTRIPLANT_MODELS_TAB_SIZE); return m_pPlantModelsTab[i]; }



public:
	//
	// access to m_pPlantModelsTab[]:
	//
	bool			SetPlantModelsTab(u32 index, grassModel* pPlantModels);
	grassModel*		GetPlantModelsTab(u32 index);
#if PSN_PLANTSMGR_SPU_RENDER
	bool			AllocateTextureBuffers();
	void			DestroyTextureBuffers();
	bool			SetPlantTexturesCmd(u32 slotid, u32 index, u32 *cmd);
	u32*			GetPlantTexturesCmd(u32 slotid, u32 index);
	u32				GetPlantTexturesCmdOffset(u32 slotid, u32 index);

	bool			AllocateModelsBuffers();
	void			DestroyModelsBuffers();
	bool			SetPlantModelsCmd(u32 slotid, u32 index, u32 *cmd);
	u32*			GetPlantModelsCmd(u32 slotid, u32 index);
	u32				GetPlantModelsCmdOffset(u32 slotid, u32 index);
#endif //PSN_PLANTSMGR_SPU_RENDER...



private:
	struct PER_THREAD_DATA
	{
		u32				m_currentIndex;
		PPTriPlant		m_Buffer[PPTRIPLANT_BUFFER_SIZE];
		s32				m_plantModelsSet;
	};

	PER_THREAD_DATA m_PerThreadData[NUMBER_OF_RENDER_THREADS];

#if PSN_PLANTSMGR_SPU_RENDER
	u32				*m_pPlantsTexturesCmd0;
	u32				*m_pPlantsTexturesCmd1;
	u32				*m_pPlantsTexturesCmd2;
	u32				*m_pPlantsTexturesCmd3;
	u32				*m_pPlantsTexturesCmdTab[PPTRIPLANT_MODELS_TAB_SIZE];

	u32				m_pPlantsTexturesCmdOffset0;
	u32				m_pPlantsTexturesCmdOffset1;
	u32				m_pPlantsTexturesCmdOffset2;
	u32				m_pPlantsTexturesCmdOffset3;
	u32				m_pPlantsTexturesCmdOffsetTab[PPTRIPLANT_MODELS_TAB_SIZE];

	u32				*m_pPlantsModelsCmd0;
	u32				*m_pPlantsModelsCmd1;
	u32				*m_pPlantsModelsCmd2;
	u32				*m_pPlantsModelsCmd3;
	u32				*m_pPlantsModelsCmdTab[PPTRIPLANT_MODELS_TAB_SIZE];

	u32				m_pPlantsModelsCmdOffset0;
	u32				m_pPlantsModelsCmdOffset1;
	u32				m_pPlantsModelsCmdOffset2;
	u32				m_pPlantsModelsCmdOffset3;
	u32				m_pPlantsModelsCmdOffsetTab[PPTRIPLANT_MODELS_TAB_SIZE];
#endif //PSN_PLANTSMGR_SPU_RENDER...

	grassModel*		m_pPlantModelsTab[PPTRIPLANT_MODELS_TAB_SIZE];
};
#endif //!__SPU...
#endif//__CPLANTGRASSRENDERER_H__...

