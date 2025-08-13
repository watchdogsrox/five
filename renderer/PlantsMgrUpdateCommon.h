//
//	PlantsMgrUpdateCommon.h
//
//  This header contains functions and data shared between PlantsMgr::Update()
//  and PlantsMgrUpdateSPU (which is __SPU build).
//
//	2010/04/26	- Andrzej:	- initial;
//
//
//
//
//
#ifndef __PLANTSMGR_UPDATE_COMMON_H__
#define __PLANTSMGR_UPDATE_COMMON_H__



#if __SPU

// handy SPU Timer:
#define HACK_GTA4_MEASURE_TIME					(0 && __DEV)
#if HACK_GTA4_MEASURE_TIME
	inline void  TimerInit()					{ spu_writech(SPU_WrDec,-1);	}
	inline u32   TimerLap()						{ return spu_readch(SPU_RdDec);	}
	inline float MeasureTime(u32 t1, u32 t2)	{ const float fTimerFreq=80.f*1000.0f; return float(t1-t2)/float(fTimerFreq);}
#else
	inline void	 TimerInit()					{				}
	inline u32	 TimerLap()						{ return 0;		}
	inline float MeasureTime(u32, u32)			{ return 0.0f;	}
#endif


//
//
// fetching helpers:
//
template<class T>
inline void Fetch(const T*& ptr, u32 count, T* dst, u32 tag)
{
	sysDmaLargeGet(dst, (u64)ptr, count * sizeof(T), tag);
	ptr = dst;
}

template<class T>
inline void Fetch(T*& ptr, u32 count, T* dst, u32 tag)
{
	sysDmaLargeGet(dst, (u64)ptr, count * sizeof(T), tag);
	ptr = dst;
}

template<class T>
inline T* FetchSmallUnaligned(void* pBuf, const T* pSrc, u32 size, u32 tag)
{
	sysDmaGet(pBuf, ((u64)pSrc)&~0x0f, size, tag);
	return (T*)(((u8*)pBuf)+(((u64)pSrc)&0x0f));
}


//
//
//
//
class atBitSetSpu : public atBitSet
{
public:
	u32 Size()					{return m_Size;}

	void Fetch(unsigned* pMem, u32 tag)
	{
		sysDmaLargeGet(pMem, ((u64)m_Bits)&~0x0f, ((m_Size+7) * sizeof(unsigned))&~0x0f, tag);
		m_Bits = &pMem[ (((u64)m_Bits)&0x0f)/4 ];
	}
};


//
//
// handy MRU fetch cache for phPolygon - saves on dma direct fetches from memory
//
struct phPolygonMruCache
{
	enum {CACHE_SIZE=16};	// tests showed that this gives best ratio of speed vs memsize than anything bigger (32,64,...)

	phPolygonMruCache()							{ Init(); }
	~phPolygonMruCache()						{}

	void				Init()					{ m_MRU=0; sysMemSet(&m_cache[0], 0x00, sizeof(m_cache)); }
	u32					GetMRU() const			{ return m_MRU; }

	const phPolygon*	GetPolygon(const phPolygon *ppuAddr);


private:
	struct CPolyMruCacheEntry
	{
		phPolygon*		m_PpuAddr;	// ppu addr of cached polygon
		u32				m_Mru;		// MRU counter, refreshed when poly is being fetched
		u32				m_pad0;
		u32				m_pad1;
		phPolygon		m_poly;		// the poly itself
	};
	CompileTimeAssert(sizeof(CPolyMruCacheEntry)==32);	// must be multiple of 16

	CPolyMruCacheEntry	m_cache[CACHE_SIZE] ;
	u32					m_MRU;
};

//
//
// grabs phPolygon from the cache (if it's there)
// if not, then entry with smallest MRU flag is evicted and fresh poly grabbed from PPU memory
//
const phPolygon* phPolygonMruCache::GetPolygon(const phPolygon *polyPpuAddr)
{
	m_MRU++;	// increase MRU counter

	u32 smallestMru		= 0xffffffff;
	u32 smallestMruIdx	= -1;

	for(u32 i=0; i<CACHE_SIZE; i++)
	{
		// requested poly already in cache?
		if(m_cache[i].m_PpuAddr == polyPpuAddr)
		{
			m_cache[i].m_Mru = m_MRU;
			return &m_cache[i].m_poly;
		}

		// keep looking for entry with smallest MRU:
		if(m_cache[i].m_Mru < smallestMru)
		{
			smallestMru		= m_cache[i].m_Mru;
			smallestMruIdx	= i;
		}
	}
	FastAssert((smallestMruIdx>=0) && (smallestMruIdx<CACHE_SIZE));

	// evict cache entry with smallest MRU, fetch there requested poly from PPU memory:
	CPolyMruCacheEntry *entry = &m_cache[ smallestMruIdx ];
	sysDmaGet(&entry->m_poly, (u64)polyPpuAddr, sizeof(phPolygon), PLANTS_DMATAGID);
	entry->m_PpuAddr	= (phPolygon*)polyPpuAddr;
	entry->m_Mru		= m_MRU;
	sysDmaWait(1<<PLANTS_DMATAGID);

	return &entry->m_poly;
}// end of phPolygonMruCache::GetPolygon()...
#endif //__SPU...



//
//
// helper class to gather per-vertex colors:
//
struct CColorStack
{
	enum {MAX_STACK_SIZE=12};

public:
	CColorStack()					{ Init(); }
	~CColorStack()					{ }

	void		Init()				{ m_count=0;			}

	void		Push(Color32 c)		{ FastAssert(m_count<(MAX_STACK_SIZE-1));	m_colors[m_count++]=c;		}
	Color32		Pop()				{ FastAssert(m_count>0);					return m_colors[--m_count];	}

	u32			GetCount()			{ return m_count;		}
	Color32*	GetColors()			{ return &m_colors[0];	}

	Color32		GetMediumColor();

private:
	u32			m_count;
	Color32		m_colors[MAX_STACK_SIZE];
};

//
//
// calculates medium color out of all colors stored on stack's table:
//
Color32 CColorStack::GetMediumColor()
{
	FastAssert(m_count > 0);	// at least 1 stored color needed for medium

	Vector3 mediumColorV3(0,0,0);
	float mediumScaleXYZ	= 0.0f;
	float mediumScaleZ		= 0.0f;
	float mediumDensity		= 0.0f;

	for(u32 i=0; i<m_count; i++)
	{
		mediumColorV3	+= VEC3V_TO_VECTOR3(m_colors[i].GetRGB());

		// unpack scaleXYZ, scaleZ and density weights as ordinary numbers to calculate medium values:
		mediumScaleXYZ	+= (float)CPlantLocTri::pv8UnpackScaleXYZ(m_colors[i].GetAlpha());
		mediumScaleZ	+= (float)CPlantLocTri::pv8UnpackScaleZ(m_colors[i].GetAlpha());
		mediumDensity	+= (float)CPlantLocTri::pv8UnpackDensity(m_colors[i].GetAlpha());
	}
	
	const float invfCount = 1.0f / float(m_count);
	mediumColorV3	*= invfCount;
	mediumScaleXYZ	*= invfCount;
	mediumScaleZ	*= invfCount;
	mediumDensity	*= invfCount;

	Color32 col32(mediumColorV3);
	col32.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ(u8(mediumDensity+0.5f), u8(mediumScaleZ+0.5f), u8(mediumScaleXYZ+0.5f)));

	return col32;
}


//
//
// helper class which maintains list of poly indexes, which contributed to vtxColors:
//
struct CPolyCache
{
	enum {MAX_POLY_CACHE_SIZE=24};	// size of the cache (must be static for quicker stack allocation)

public:
	CPolyCache()		{ Init(); }
	~CPolyCache()		{ }

	void				Init()										{m_count=0;	}
	void				AddPoly(phPolygon::Index idx)				{FastAssert(m_count < (MAX_POLY_CACHE_SIZE-1)); m_cache[m_count++]=idx;	}
	void				AddPolyIfNotAdded(phPolygon::Index idx)
	{
		if(!IsPolyInCache(idx))
			AddPoly(idx);
	}

	bool				IsPolyInCache(phPolygon::Index idx);

private:
	u32					m_count;
	phPolygon::Index	m_cache[MAX_POLY_CACHE_SIZE];
};

bool CPolyCache::IsPolyInCache(phPolygon::Index idx)
{
	for(u32 i=0; i<m_count; i++)
	{
		if(m_cache[i] == idx)
			return(TRUE);
	}
	return(FALSE);
}


#if CPLANT_CLIP_EDGE_VERT
#define CLIP_ON_BAD_INDEX 0
//
// - Compute whether a the grass can extend across an edge of our poly.
//
static void _extractPolyEdgeCullingFromNeighbours(phBoundGeometry *pBound, phPolygon::Index polyIdx, bool (&isClip)[3], const atBitSet &boundMatProps)
{
	//Neighbor 0 => edge[v0, v1]
	//Neighbor 1 => edge[v1, v2]
	//Neighbor 2 => edge[v2, v0]

	FastAssert(polyIdx != 0xffff);

#if __SPU
	const phPolygon *pPolyPpu = &pBound->GetPolygon(polyIdx);
	#if USE_POLY_MRU_CACHE
		const phPolygon *pPoly = g_PolygonMruCache.GetPolygon(pPolyPpu);
	#else
		phPolygon poly;
		sysDmaGet(&poly, (u64)pPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
		sysDmaWait(1<<PLANTS_DMATAGID);
		const phPolygon *pPoly = &poly;
	#endif
	DEV_ONLY(g_PolygonsFetched++;)
#else
	const phPolygon *pPoly = &pBound->GetPolygon(polyIdx);
#endif

	phPolygon::Index neighborIdx[3] = {pPoly->GetNeighboringPolyNum(0), pPoly->GetNeighboringPolyNum(1), pPoly->GetNeighboringPolyNum(2)};
	isClip[0] = (neighborIdx[0] == static_cast<phPolygon::Index>(BAD_INDEX) ? static_cast<bool>(CLIP_ON_BAD_INDEX) : !boundMatProps.IsSet(neighborIdx[0] * 2));
	isClip[1] = (neighborIdx[1] == static_cast<phPolygon::Index>(BAD_INDEX) ? static_cast<bool>(CLIP_ON_BAD_INDEX) : !boundMatProps.IsSet(neighborIdx[1] * 2));
	isClip[2] = (neighborIdx[2] == static_cast<phPolygon::Index>(BAD_INDEX) ? static_cast<bool>(CLIP_ON_BAD_INDEX) : !boundMatProps.IsSet(neighborIdx[2] * 2));
}


//
// - Compute whether a the grass can extend across a vert of our poly.
//
static void _extractPolyVertCullingFromNeighbours(	phBoundGeometry *pBound, phPolygon::Index polyIdx,
													phPolygon::Index (&vertIdx)[3], bool (&isClip)[3], const atBitSet &boundMatProps)
{
	FastAssert(polyIdx != 0xffff);

#if __SPU
	const phPolygon *pPolyPpu = &pBound->GetPolygon(polyIdx);
	#if USE_POLY_MRU_CACHE
		const phPolygon *pPoly = g_PolygonMruCache.GetPolygon(pPolyPpu);
	#else
		phPolygon poly;
		sysDmaGet(&poly, (u64)pPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
		sysDmaWait(1<<PLANTS_DMATAGID);
		const phPolygon *pPoly = &poly;
	#endif
	DEV_ONLY(g_PolygonsFetched++;)
#else
	const phPolygon *pPoly = &pBound->GetPolygon(polyIdx);
#endif

	//Compute whether a the grass can extend across a vert of our poly.
	isClip[0] = isClip[1] = isClip[2] = false;
	for(u32 n = 0; n < 3; ++n)
	{
		ASSERT_ONLY(static const u32 sMaxLoops = 1000);
		ASSERT_ONLY(u32 loopCount = 0);
		phPolygon::Index prevIdx = polyIdx;
		phPolygon::Index neighborIdx = pPoly->FindNeighborWithVertex(vertIdx[n]);
		while(neighborIdx != polyIdx && neighborIdx != static_cast<phPolygon::Index>(BAD_INDEX))
		{
			const bool polyCreatesPlants = boundMatProps.IsSet(neighborIdx * 2);
			isClip[n] |= !polyCreatesPlants;

			if(isClip[n]) //If any poly causes the vert to be a clipping vert, then we can break out of this loop.
				break;

			Assertf(loopCount++ < sMaxLoops, "Warning: _extractPolyVertCullingFromNeighbours has looped over %d polys in trifan around vertex %d. Something is probablly wrong.", loopCount - 1, vertIdx[n]);

			//Probably don't need to keep the prev neighbor's index as it seems it will always return the poly in clockwise/counterclockwise
			//order, but doesn't hurt to be safe.
		#if __SPU
			const phPolygon *pNPolyPpu = &pBound->GetPolygon(neighborIdx);
			#if USE_POLY_MRU_CACHE
				const phPolygon *pNPoly = g_PolygonMruCache.GetPolygon(pNPolyPpu);
			#else
				phPolygon poly;
				sysDmaGet(&poly, (u64)pNPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
				sysDmaWait(1<<PLANTS_DMATAGID);
				const phPolygon *pNPoly = &poly;
			#endif
			DEV_ONLY(g_PolygonsFetched++;)
			phPolygon::Index nextIdx = pNPoly->FindNeighborWithVertex(vertIdx[n], prevIdx);
		#else
			phPolygon::Index nextIdx = pBound->GetPolygon(neighborIdx).FindNeighborWithVertex(vertIdx[n], prevIdx);
		#endif
			prevIdx = neighborIdx;
			neighborIdx = nextIdx;
		}

#if CLIP_ON_BAD_INDEX
		//Since our computation is not cumulative, a bad index indicates that we found our solution. No need to loop in the opposite direction.
		isClip[n] |= (neighborIdx == static_cast<phPolygon::Index>(BAD_INDEX));
#else

		//Check if we need to loop again in the reverse direction...
		if(!isClip[n] && neighborIdx == static_cast<phPolygon::Index>(BAD_INDEX))
		{
			ASSERT_ONLY(loopCount = 0);

			//We have not yet definitely concluded whether this is a clipping and the last returned neighbor index was BAD_INDEX meaning
			//that the tri-fan surrounding this vertex does not connect. Therefore, we need to continue our checks in the reverse direction.
			prevIdx = polyIdx;
			phPolygon::Index neighborIdx = pPoly->FindNeighborWithVertex2(vertIdx[n]);
			while(neighborIdx != polyIdx && neighborIdx != static_cast<phPolygon::Index>(BAD_INDEX))
			{
				const bool polyCreatesPlants = boundMatProps.IsSet(neighborIdx*2+0);
				isClip[n] |= !polyCreatesPlants;

				if(isClip[n]) //If any poly causes the vert to be a clipping vert, then we can break out of this loop.
					break;

				Assertf(loopCount++ < sMaxLoops, "Warning: _extractPolyVertCullingFromNeighbours has looped over %d polys in trifan around vertex %d. Something is probablly wrong.", loopCount - 1, vertIdx[n]);

			#if __SPU
				const phPolygon *pNPolyPpu = &pBound->GetPolygon(neighborIdx);
				#if USE_POLY_MRU_CACHE
					const phPolygon *pNPoly = g_PolygonMruCache.GetPolygon(pNPolyPpu);
				#else
					phPolygon poly;
					sysDmaGet(&poly, (u64)pNPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
					sysDmaWait(1<<PLANTS_DMATAGID);
					const phPolygon *pNPoly = &poly;
				#endif
				DEV_ONLY(g_PolygonsFetched++;)
				phPolygon::Index nextIdx = pNPoly->FindNeighborWithVertex2(vertIdx[n], prevIdx);
			#else
				phPolygon::Index nextIdx = pBound->GetPolygon(neighborIdx).FindNeighborWithVertex2(vertIdx[n], prevIdx);
			#endif

				prevIdx = neighborIdx;
				neighborIdx = nextIdx;
			}
		}
#endif
	}
}
#endif //CPLANT_CLIP_EDGE_VERT...


//
// - extracts all neighbours colors of a given poly
// - compares vert idx to know where to add colors
//
static void _extractPolyColorsOfNeighbours(phBoundGeometry *pBound, phPolygon::Index *baseIdx, Color32 baseColor,
										   u32 polyIdx,
										   CColorStack& vtxColors0, CColorStack& vtxColors1, CColorStack& vtxColors2,
										   CPolyCache& polyCache
										   )
{
	FastAssert(polyIdx != 0xffff);

#if __SPU
	const phPolygon *pPolyPpu = &pBound->GetPolygon(polyIdx);
	#if USE_POLY_MRU_CACHE
		const phPolygon *pPoly = g_PolygonMruCache.GetPolygon(pPolyPpu);
	#else
		phPolygon poly;
		sysDmaGet(&poly, (u64)pPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
		sysDmaWait(1<<PLANTS_DMATAGID);
		const phPolygon *pPoly = &poly;
	#endif
	DEV_ONLY(g_PolygonsFetched++;)
#else
	const phPolygon *pPoly = &pBound->GetPolygon(polyIdx);
#endif


	for(u32 i=0; i<3; i++)
	{
		const phPolygon::Index neighbourIdx = pPoly->GetNeighboringPolyNum(i);
		if((neighbourIdx!=0xffff) && (!polyCache.IsPolyInCache(neighbourIdx)))
		{
		#if __SPU
			const phPolygon *pNPolyPpu = &pBound->GetPolygon(neighbourIdx);
			#if USE_POLY_MRU_CACHE
				const phPolygon *pNPoly = g_PolygonMruCache.GetPolygon(pNPolyPpu);
			#else
				phPolygon poly;
				sysDmaGet(&poly, (u64)pNPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
				sysDmaWait(1<<PLANTS_DMATAGID);
				const phPolygon *pNPoly = &poly;
			#endif
			DEV_ONLY(g_PolygonsFetched++;)
		#else
			const phPolygon *pNPoly = &pBound->GetPolygon(neighbourIdx);
		#endif

			// neighbour poly verts:
			phPolygon::Index NPolyIdx[3];
			NPolyIdx[0] = pNPoly->GetVertexIndex(0);
			NPolyIdx[1] = pNPoly->GetVertexIndex(1);
			NPolyIdx[2] = pNPoly->GetVertexIndex(2);

			// neighbour poly color:
			const phMaterialIndex	boundMaterialID	= pBound->GetPolygonMaterialIndex(neighbourIdx);
			const phMaterialMgr::Id	actualMaterialID= pBound->GetMaterialId(boundMaterialID);
			#if __SPU
				const u32			materialColorIdx= phMaterialMgrGta::UnpackMtlColour(actualMaterialID);
			#else
				const u32			materialColorIdx= PGTAMATERIALMGR->UnpackMtlColour(actualMaterialID);
			#endif
			const Color32			NColor			= materialColorIdx? pBound->GetMaterialColor(materialColorIdx) : baseColor;

			for(u32 v=0; v<3; v++)
			{
				if( NPolyIdx[v] == baseIdx[0] )
				{
					vtxColors0.Push(NColor);
					polyCache.AddPolyIfNotAdded(neighbourIdx);
				}
				else if( NPolyIdx[v] == baseIdx[1] )
				{
					vtxColors1.Push(NColor);
					polyCache.AddPolyIfNotAdded(neighbourIdx);
				}
				else if( NPolyIdx[v] == baseIdx[2] )
				{
					vtxColors2.Push(NColor);
					polyCache.AddPolyIfNotAdded(neighbourIdx);
				}
			}
		}//if(!polyCache.IsPolyInCache(neighbourIdx))...
	}//for(u32 i=0; i<3; i++)...

}// end of _extractGroundColors()....






#if __SPU
	phInst* CPlantColBoundEntry::GetPhysInst()
	{ 
	#if 1
		return NULL;  // todo
	#else
		return m__pEntity? ((phInst*)sysDmaGetUInt32((u64)m__pEntity->GetCurrentPhysicsInstPtr(),PLANTS_DMATAGID)) : NULL;
	#endif
	}
	phBoundGeometry* CPlantColBoundEntry::GetBound()
	{
	#if 1
		return NULL; // todo
	#else
		phInst* pPpuInst = GetPhysInst();
		if (!pPpuInst)
			return NULL;

		// shouldn't really dma across the whole phInst & phArchetype here..
		phInst* pInst = Alloca(phInst, 1);
		sysDmaGet(pInst, (u64)pPpuInst, sizeof(phInst),PLANTS_DMATAGID);
		sysDmaWait(1<<PLANTS_DMATAGID);

		phArchetype* pArchetype = Alloca(phArchetype, 1);
		sysDmaGet(pArchetype, (u64)pInst->GetArchetype(), sizeof(phArchetype),PLANTS_DMATAGID);
		sysDmaWait(1<<PLANTS_DMATAGID);

		const u32 nPhBoundGeometrySize16 = (sizeof(phBoundGeometry)+15)&(~0xf);
		static u8 _sm_CachedBoundGeom[nPhBoundGeometrySize16] ;	// must be static/global memory as ptr to this memory is returned by this method
		#define sm_CachedBoundGeom	((phBoundGeometry*)&_sm_CachedBoundGeom[0])

		sysDmaGet(sm_CachedBoundGeom, (u64)pArchetype->GetBound(), nPhBoundGeometrySize16,PLANTS_DMATAGID);
		sysDmaWait(1<<PLANTS_DMATAGID);

		return sm_CachedBoundGeom;
	#endif
	}
#else
	inline phInst* CPlantColBoundEntry::GetPhysInst()
	{
		return IsMatBound()? NULL : (m__pEntity? m__pEntity->GetCurrentPhysicsInst() : NULL);
	}
	inline phBoundGeometry* CPlantColBoundEntry::GetBound()
	{
		if(IsMatBound())
		{
			if((m_nBParentIndex!=-1) && (m_nBChildIndex!=-1) && (g_StaticBoundsStore.GetPtr(strLocalIndex(m_nBParentIndex))!=NULL))
			{
				fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(strLocalIndex(m_nBParentIndex));
				if (pDef && pDef->m_pObject && pDef->m_pObject->GetType()==phBound::COMPOSITE)
				{
					phBoundComposite* pComposite = (phBoundComposite*)(pDef->m_pObject);
					return (phBoundGeometry*)(pComposite->GetBound(m_nBChildIndex));
				}
				else
				{
					return NULL;
				}
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			return GetPhysInst()? static_cast<phBoundGeometry*>(GetPhysInst()->GetArchetype()->GetBound()) : NULL;
		}
	}
	inline const Matrix34* CPlantColBoundEntry::GetBoundMatrix()	
	{
		return IsMatBound()? NULL : (GetPhysInst()? &RCC_MATRIX34(GetPhysInst()->GetMatrix()) : NULL);
	}
#endif //!__SPU...


//#pragma mark -
//#pragma mark --- CPlantLocTri stuff ---

#if __SPU
	void g_procObjMan_AddTriToRemoveList(CPlantLocTriArray& triTab, CPlantLocTri* pLocTri);
	void g_procObjMan_AddObjectToAddList(CPlantLocTriArray& triTab, Vector3 pos, Vector3 normal, CProcObjInfo* pProcObjInfo, CPlantLocTri* pLocTri);
#endif
//
//
//
//
CPlantLocTri* CPlantLocTri::Add(CPlantLocTriArray& triTab, const Vector3& v1, const Vector3& v2, const Vector3& v3, phMaterialMgr::Id nSurfaceType, bool bCreatesPlants, bool bCreatesObjects, CEntity* pParentEntity, u16 nColEntIdx, bool bIsFarDrawTri)
{
	FastAssert(triTab.m_UnusedListHead);

	this->m_V1.SetVector3(v1);
	this->m_V2.SetVector3(v2);
	this->m_V3.SetVector3(v3);

	this->m_nSurfaceType	= nSurfaceType;
	this->m_nAmbientScale[0]= 255;
	this->m_nAmbientScale[1]= 255;
	this->m_bRequireAmbScale= true;				// mark as requiring Ambient Scale TC sampling - done on Main Thread after parallell update
	
	triTab.m_bRequireAmbScale=true;				// mark parent list too

	this->m_bCreatesPlants	= bCreatesPlants;
	this->m_bCreatesObjects	= bCreatesObjects;
	this->m_bCreatedObjects	= false;
#if CPLANT_USE_OCCLUSION
	this->m_bOccluded		= false;
	this->m_bNeedsAABB		= true;
#endif

#if CPLANT_CLIP_EDGE_VERT
	this->m_ClipEdge_01 = this->m_ClipEdge_12 = this->m_ClipEdge_20 = false;
	this->m_ClipVert_0 = this->m_ClipVert_1 = this->m_ClipVert_2 = false;
#endif

	Vector3 vecSphRadius(this->GetCenter() - this->GetV1());
	float SphereRadius = vecSphRadius.Mag();
	SphereRadius *= 1.75f;	// make sphere radius 75% bigger (for better sphere visibility detection)
	SetSphereRadius(SphereRadius);

	this->m_pParentEntity	= pParentEntity;
	FastAssert(nColEntIdx < 255);	// must fit into u8
	this->m_nColEntIdx		= (u8)nColEntIdx;

	// set to far tri if tri creates proc objects which require high min/max creation range:
	if(bCreatesObjects && (!bIsFarDrawTri))
	{
		s32 procTagId = PGTAMATERIALMGR->UnpackProcId(nSurfaceType);
		s32 procObjInfoIndex = g_procInfo.m_procTagTable[procTagId].procObjIndex;
		Assert(procObjInfoIndex!=-1);
		CProcObjInfo *pProcObjInfo = &g_procInfo.m_procObjInfos[procObjInfoIndex]; 

		if(pProcObjInfo->m_Flags.IsSet(PROCOBJ_FARTRI))
		{
			bIsFarDrawTri = true;	// must be far tri
		}
	}

	this->m_bDrawFarTri		= bIsFarDrawTri;

	if(m_bCreatesObjects && !m_bCreatesPlants)
	{
		// deal with ONLY procedurally generated objects
		gPlantMgr.MoveLocTriToList(triTab, &triTab.m_UnusedListHead, &triTab.m_CloseListHead, this);
		return(this);
	}
	else
	{		
	#if __SPU
		s32 procTagId = phMaterialMgrGta::UnpackProcId(nSurfaceType);
	#else
		s32 procTagId = PGTAMATERIALMGR->UnpackProcId(nSurfaceType);
	#endif
		s32 plantInfoIndex = g_procInfo.m_procTagTable[procTagId].plantIndex;
	#if PLANTSMGR_DATA_EDITOR
		#if __SPU
			if(g_jobParams->m_AllCollisionSelectable)
		#else
			if(gPlantMgr.GetAllCollisionSelectableUT())
		#endif
			{
				if(plantInfoIndex==-1)
					plantInfoIndex=0;	// hack: select something valid
			}
	#endif //PLANTSMGR_DATA_EDITOR...
		CPlantInfo *pPlantInfo = &g_procInfo.m_plantInfos[plantInfoIndex];

		const float triArea	= this->CalcArea();
		const float fNumPlants = triArea * pPlantInfo->m_Density.GetFloat32_FromFloat16();

		// only update the list if fNumPlants is big enough or it's furgrass poly:
		if((fNumPlants>0.05f) 
				#if FURGRASS_TEST_V4
					|| (pPlantInfo->m_Flags.IsSet(PROCPLANT_FURGRASS))
				#endif
			)
		{
			this->SetTriArea(triArea);

			// generate static seed from input pos data:
			this->SetSeed( (v1.ux+v1.uy^v1.uz) | ((v2.ux+v2.uy^v2.uz)<<8) | ((v3.ux+v3.uy^v3.uz)<<16) );

			// calculate skewing angle and axis to match ground slope:
			Vector3 triV12, triV13, triVnorm;
			triV12 = v2 - v1;
			triV13 = v3 - v1;
			triV12.Normalize();
			triV13.Normalize();
			triVnorm.Cross(triV12, triV13);
			triVnorm.Normalize();

			m_normal[0] = s8(triVnorm.x * 127.0f);
			m_normal[1] = s8(triVnorm.y * 127.0f);
			m_normal[2] = s8(triVnorm.z * 127.0f);

			const Vector3 standardNormal(0.0f, 0.0f, 1.0f);

			Vector3 axis; float angle;
			m_skewAxisAngle.w.SetFloat16_Zero();
			if(ComputeRotation(standardNormal, triVnorm, axis, angle))
			{	
				if(angle > PlantsMinGroundAngleSlope)
				{
					Float16Vec4Pack(&m_skewAxisAngle, Vec4V(RCC_VEC3V(axis),ScalarV(angle)) );
				}
			}

			// get ground colors from procedural.dat:
			m_GroundColorV1 =
			m_GroundColorV2 =
			m_GroundColorV3 = pPlantInfo->m_GroundColor;
			#if __SPU
				#if __BANK
					if(g_jobParams->m_gbForceDefaultGroundColor)
					{
						m_GroundColorV1 =
						m_GroundColorV2 =
						m_GroundColorV3 = g_jobParams->m_gbDefaultGroundColor;
					}
				#endif
			#else
				#if __BANK
					if(gbForceDefaultGroundColor)
					{
						m_GroundColorV1 =
						m_GroundColorV2 =
						m_GroundColorV3 = gbDefaultGroundColor;
					}
				#endif
			#endif //__SPU...

			m_bCameraDontCull	= pPlantInfo->m_Flags.IsSet(PROCPLANT_CAMERADONOTCULL);
			m_bUnderwater		= pPlantInfo->m_Flags.IsSet(PROCPLANT_UNDERWATER);
			m_bGroundScale1Vert	= pPlantInfo->m_Flags.IsSet(PROCPLANT_GROUNDSCALE1VERT);

			gPlantMgr.MoveLocTriToList(triTab, &triTab.m_UnusedListHead, &triTab.m_CloseListHead, this);
			return(this);
		}
		else if(m_bCreatesObjects)
		{
			this->m_bCreatesPlants = false;
			gPlantMgr.MoveLocTriToList(triTab, &triTab.m_UnusedListHead, &triTab.m_CloseListHead, this);
			return(this);
		}

		return(NULL);
	}
	
}// end of CPlantLocTri::Add()...

//
//
//
//
void CPlantLocTri::Release(CPlantLocTriArray& triTab)
{
	SetTriArea(0.0f);

	if (m_bCreatedObjects)
	{
	#if __SPU
		g_procObjMan_AddTriToRemoveList(triTab, this);
	#else
		g_procObjMan.AddTriToRemoveList(this);
	#endif
	}

	gPlantMgr.MoveLocTriToList(triTab, &triTab.m_CloseListHead, &triTab.m_UnusedListHead, this);

	if (m_bCreatesObjects && !m_bCreatesPlants)
	{
		this->m_nSurfaceType	= 0xFE;		// simple tag to show who released this
	}
	else
	{
		this->m_nSurfaceType	= 0xFF;		// simple tag to show who released this
	}
	
	m_skewAxisAngle.w.SetFloat16_Zero();

	this->m_bCreatesPlants	= false;
	this->m_bCreatesObjects = false;
	this->m_bCreatedObjects = false;
	this->m_pParentEntity	= NULL;
	
}// end of CPlantLocTri::Release()...


//
// 1) way no. 1:
// Area = |P1xP2 + P2xP3 + P3xP1| / 2;
//
//
// 2) way no. 2:
// V1 = P2-P1, V2 = P3-P1
// Area = |V1xV2| / 2;
//
//
//
float CPlantLocTri::CalcArea()
{
//	const CVector p1p2 = CrossProduct(pLocTri->m_V1, pLocTri->m_V2);
//	const CVector p2p3 = CrossProduct(pLocTri->m_V2, pLocTri->m_V3);
//	const CVector p3p1 = CrossProduct(pLocTri->m_V3, pLocTri->m_V1);
//	const CVector p(p1p2 + p2p3 + p3p1);
//	const float area1 = p.Magnitude() * 0.5f;

	const Vector3 V1(this->GetV2() - this->GetV1());
	const Vector3 V2(this->GetV3() - this->GetV1());
	Vector3 c;
	c.Cross(V1, V2);
	const float area2 = c.Mag() * 0.5f;

	return(area2);
}

//
//
//  IsPtInTriangle2D
//
bool CPlantLocTri::IsPtInTriangle2D(float x, float y, const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& normal, float* z)
{
	// calc the vectors from the point to each vertex
	float dot1 = (v1.x-x)*(v2.y-v1.y) + (v1.y-y)*(v1.x-v2.x);
	float dot2 = (v2.x-x)*(v3.y-v2.y) + (v2.y-y)*(v2.x-v3.x);
	float dot3 = (v3.x-x)*(v1.y-v3.y) + (v3.y-y)*(v3.x-v1.x);

	// check if the point is inside the 2d triangle
	if (dot1>=0.0f && dot2>=0.0f && dot3>=0.0f)
	{
		// we're inside - calculate the z of the point
		float d = -(normal.x*v1.x + normal.y*v1.y + normal.z*v1.z);
		*z = (-normal.x*x - normal.y*y - d) / normal.z;

		return true;
	}

	// we're outside
	return false;
}

#if FURGRASS_TEST_V4
//
//
// go through ColEnt entries and store furgrass info to render table (for RT):
//
bool CPlantMgr::FurGrassStoreRenderInfo(CPlantColBoundEntryFurGrassInfo *dst, u32 SPU_ONLY(dmaTag))
{
	u16 entry = m_ColEntCache.m_CloseListHead;
	while(entry)
	{
		CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];

		// HACKME: change entryptr to index:
		const u16 idx = entry-1;
		FastAssert(idx < CPLANT_COL_ENTITY_CACHE_SIZE);
#if __SPU
		sysDmaPut(&pEntry->m_furInfo, (u64)&dst[idx], sizeof(CPlantColBoundEntryFurGrassInfo), dmaTag);
#else
		sysMemCpy(&dst[idx], &pEntry->m_furInfo, sizeof(CPlantColBoundEntryFurGrassInfo));
#endif
		entry = pEntry->m_NextEntry;
	}

	return(TRUE);
}
#endif // FURGRASS_TEST_V4...


//
//
// goes through active LocTris list, tries to reject
// some located far away and generate new LocTris from ColModel's triangles:
//
//
bool CPlantMgr::UpdateAllLocTris(CPlantLocTriArray&	triTab, const Vector3& camPos, s32 iTriProcessSkipMask, u32 *pFurgrassTagPresent)
{
#if PLANTSMGR_MULTI_UPDATE

	u64	visitedEntries=0;															// 1=non-visited, 0=visited
	CompileTimeAssert(CPLANT_COL_ENTITY_CACHE_SIZE <= sizeof(visitedEntries)*8);	// must fit

	u16 entry = m_ColEntCache.m_CloseListHead;
	while(entry)
	{
		const u32 bitIdx = (u32)(entry-1);	// hack: convert entry into index

		CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];
		visitedEntries |= (u64(1)<<bitIdx);	// mark unvisited nodes in the bitfield
		entry = pEntry->m_NextEntry;
	}


	// 1: first single loop over collision entities using TryLock() and to allow to spread the work evenly among subtasks:
	if(true)
	{
		u16 entry = m_ColEntCache.m_CloseListHead;
		while(entry)
		{
			const u32 bitIdx = (u32)(entry-1);
			CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];

			if(visitedEntries & (u64(1)<<bitIdx))
			{
				if(pEntry->TryLock())
				{
					_ProcessEntryCollisionData(triTab, pEntry, camPos, iTriProcessSkipMask, pFurgrassTagPresent);
				
					visitedEntries &= ~(u64(1)<<bitIdx);	// mark as visited
					pEntry->Unlock();						// unlock bound for other subtasks
				}
			}
			entry = pEntry->m_NextEntry;
		}
	}

	// 2: main loop using blocking Lock() until it's all done:
	while( visitedEntries != u64(0) )
	{
		u16 entry = m_ColEntCache.m_CloseListHead;
		while(entry)
		{
			const u32 bitIdx = (u32)(entry-1);
			CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];

			if(visitedEntries & (u64(1)<<bitIdx))
			{
				pEntry->Lock();
				// this subtask has exclusive possesion of the bound entity:
				// TODO: quick bitfield rejection test on entry using listID's
				_ProcessEntryCollisionData(triTab, pEntry, camPos, iTriProcessSkipMask, pFurgrassTagPresent);
				
				visitedEntries &= ~(u64(1)<<bitIdx);	// mark as visited
				pEntry->Unlock();						// unlock bound for other subtasks
			}

			entry = pEntry->m_NextEntry;
		}
	}


#else //PLANTSMGR_MULTI_UPDATE

	#if __SPU
		// nop
	#else
		g_procObjMan.LockListAccess();
	#endif

	u16 entry = m_ColEntCache.m_CloseListHead;
	while(entry)
	{
		CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];

		// TODO: quick bitfield rejection test on entry using listID's
		_ProcessEntryCollisionData(triTab, pEntry, camPos, iTriProcessSkipMask, pFurgrassTagPresent);

		entry = pEntry->m_NextEntry;
	}

	#if __SPU
		// nop
	#else
		g_procObjMan.UnlockListAccess();
	#endif
#endif //PLANTSMGR_MULTI_UPDATE...

	return(TRUE);
}// end of CPlantMgr::_UpdateLocTris()...


//
//
//
//
bool CPlantMgr::_CullDistanceCheck(const Vec3V positions[4], Vec3V_In camPos, bool bIsDrawFarTri, bool bCheckAllCulled)
{
#if PLANTS_USE_LOD_SETTINGS
	float fFarDistSqr	= CPLANT_TRILOC_FAR_DIST_SQR		*CGrassRenderer::GetDistanceMultiplier()*CGrassRenderer::GetDistanceMultiplier();
	float fShortDistSqr = CPLANT_TRILOC_SHORT_FAR_DIST_SQR	*CGrassRenderer::GetDistanceMultiplier()*CGrassRenderer::GetDistanceMultiplier();
#else
	float fFarDistSqr	= CPLANT_TRILOC_FAR_DIST_SQR;
	float fShortDistSqr = CPLANT_TRILOC_SHORT_FAR_DIST_SQR;
#endif
#if PLANTSMGR_DATA_EDITOR
	#if __SPU
		if(g_jobParams->m_AllCollisionSelectable)
	#else
		if(gPlantMgr.GetAllCollisionSelectableUT())
	#endif
		{
			fShortDistSqr *= 0.333f;		// make these smaller when editing with "select all" mode
			fFarDistSqr = fShortDistSqr;
		}
#endif //PLANTSMGR_DATA_EDITOR...

	const ScalarV cullDistance = ScalarV(bIsDrawFarTri ? fFarDistSqr : fShortDistSqr);
	const Vec4V vCullDistance(cullDistance);

	bool bResult;

	//Phase 1  - check only for vertices
#if CPLANT_USE_COLLISION_2D_DIST
	const Vec4V colDistSqrSet1(
		MagSquared((camPos - positions[0]).GetXY()), 
		MagSquared((camPos - positions[1]).GetXY()), 
		MagSquared((camPos - positions[2]).GetXY()), 
		MagSquared((camPos - positions[3]).GetXY()));
#else
	const Vec4V colDistSqrSet1(
		MagSquared(camPos - positions[0]), 
		MagSquared(camPos - positions[1]), 
		MagSquared(camPos - positions[2]), 
		MagSquared(camPos - positions[3]));
#endif

	if(bCheckAllCulled)
	{
		VecBoolV vAllCulled = IsGreaterThanOrEqual(colDistSqrSet1, vCullDistance);
		bResult = IsTrueAll(vAllCulled);//  IsEqualIntAll(vAllCulled, VecBoolV(V_TRUE))? true : false;
	}
	else
	{
		VecBoolV vAnyNotCulled = IsLessThan(colDistSqrSet1, vCullDistance);
		bResult = !IsFalseAll(vAnyNotCulled);//!IsEqualIntAll(vAnyNotCulled, VecBoolV(V_FALSE));
	}

	//Phase 2  - check only triangle center and midpoint if previous conditions are met
	// In case of all culled check, early out if bResult = false (equivalent to doing an '&&' with all points)
	// In case of any not culled check, early out if bResult = true (equivalent to doing an '||' with all points)
	if((bCheckAllCulled && bResult) || (!bCheckAllCulled && !bResult))
	{
		// calc middle points distances (distance between camera and tri edges' middle points):
	#if CPLANT_USE_COLLISION_2D_DIST
		const Vec4V colDistSqrSet2(
			MagSquared((camPos - (positions[0]+positions[1]) * ScalarV(V_HALF)).GetXY()), // V1-2
			MagSquared((camPos - (positions[1]+positions[2]) * ScalarV(V_HALF)).GetXY()), // V2-3
			MagSquared((camPos - (positions[2]+positions[0]) * ScalarV(V_HALF)).GetXY()), // V3-1
			ScalarV(FLT_MAX)) ;
	#else
		const Vec4V colDistSqrSet2(
			MagSquared(camPos -(positions[0]+positions[1]) * ScalarV(V_HALF)),		// V1-2
			MagSquared(camPos -(positions[1]+positions[2]) * ScalarV(V_HALF)), 		// V2-3
			MagSquared(camPos -(positions[2]+positions[0]) * ScalarV(V_HALF)), 		// V3-1
			ScalarV(FLT_MAX)) ;
	#endif

		if(bCheckAllCulled)
		{
			VecBoolV vAllCulled = IsGreaterThanOrEqual(colDistSqrSet2, vCullDistance);
			bResult = IsTrueAll(vAllCulled); //IsEqualIntAll(vAllCulled, VecBoolV(V_TRUE))? true : false;
		}
		else
		{
			VecBoolV vAnyNotCulled = IsLessThan(colDistSqrSet2, vCullDistance);
			bResult = !IsFalseAll(vAnyNotCulled); //!IsEqualIntAll(vAnyNotCulled, VecBoolV(V_FALSE));
		}
	}

	return bResult;
}// end of _CullDistanceCheck()...


//
//
//
//
static bool _CullSphereCheckInternal(const Vector4& cullSphereV4, const Vec3V positions[4])
{
	const Vec3V spherePos = RCC_VEC3V(cullSphereV4);

	const ScalarV cullDistance = ScalarV(cullSphereV4.w*cullSphereV4.w);
	const Vec4V vCullDistance(cullDistance);

	// check only for vertices
#if CPLANT_USE_COLLISION_2D_DIST
	const Vec4V colDistSqrSet1(
		MagSquared((spherePos - positions[0]).GetXY()), 
		MagSquared((spherePos - positions[1]).GetXY()), 
		MagSquared((spherePos - positions[2]).GetXY()), 
		MagSquared((spherePos - positions[3]).GetXY()));
#else
	const Vec4V colDistSqrSet1(
		MagSquared(spherePos - positions[0]), 
		MagSquared(spherePos - positions[1]), 
		MagSquared(spherePos - positions[2]), 
		MagSquared(spherePos - positions[3]));
#endif

	VecBoolV vAnyCulled = IsLessThanOrEqual(colDistSqrSet1, vCullDistance);
	bool bResult = !IsFalseAll(vAnyCulled);

	if(!bResult)
	{
		// calc middle points distances (distance between camera and tri edges' middle points):
	#if CPLANT_USE_COLLISION_2D_DIST
		const Vec4V colDistSqrSet2(
			MagSquared((spherePos - (positions[0]+positions[1]) * ScalarV(V_HALF)).GetXY()), // V1-2
			MagSquared((spherePos - (positions[1]+positions[2]) * ScalarV(V_HALF)).GetXY()), // V2-3
			MagSquared((spherePos - (positions[2]+positions[0]) * ScalarV(V_HALF)).GetXY()), // V3-1
			ScalarV(FLT_MAX)) ;
	#else
		const Vec4V colDistSqrSet2(
			MagSquared(spherePos -(positions[0]+positions[1]) * ScalarV(V_HALF)),		// V1-2
			MagSquared(spherePos -(positions[1]+positions[2]) * ScalarV(V_HALF)), 		// V2-3
			MagSquared(spherePos -(positions[2]+positions[0]) * ScalarV(V_HALF)), 		// V3-1
			ScalarV(FLT_MAX)) ;
	#endif

		VecBoolV vAnyCulled = IsLessThanOrEqual(colDistSqrSet2, vCullDistance);
		bResult = !IsFalseAll(vAnyCulled);
	}

	return(bResult);
}

//
// Cullsphere distance check:
// true = culled
// false = not culled
//
bool CPlantMgr::_CullSphereCheck(const Vec3V positions[4])
{
#if __SPU
	if(g_jobParams->m_bCullSphereEnabled0)
	{
		const Vector4& cullSphereV4a = g_jobParams->m_cullSphere[0];
		if(_CullSphereCheckInternal(cullSphereV4a, positions))
		{
			return(true);	// already culled by sphere0 - no point checking for sphere1
		}
	}

	if(g_jobParams->m_bCullSphereEnabled1)
	{
		const Vector4& cullSphereV4b = g_jobParams->m_cullSphere[1];
		if(_CullSphereCheckInternal(cullSphereV4b, positions))
		{
			return(true);	// culled by sphere1
		}
	}

	return(false);	// not culled by any of spheres

#else // __SPU

	for(u32 n=0; n<CPLANT_CULLSPHERES_MAX; n++)
	{
		if(m_CullSphereEnabled0[n])
		{
			const Vector4& cullSphereV4a = m_CullSphere0[n];
			if(_CullSphereCheckInternal(cullSphereV4a, positions))
			{
				return(true);	// already culled by sphere[N] - no point checking for sphere[N+1]
			}
		}
	}

	return(false);	// not culled by any of spheres

#endif //!__SPU...
}// end of _CullSphereCheck()...

//
//
//
//
bool CPlantMgr::_ProcessEntryCollisionData(CPlantLocTriArray& triTab, CPlantColBoundEntry *pEntry, const Vector3& camPos, s32 iTriProcessSkipMask, u32 *pFurgrassTagPresent)
{
	phBoundGeometry *pBound	= pEntry->GetBound();
	if(!pBound)
		return(FALSE);

	Assertf(pEntry->m_nNumTris > 0, "Not valid number of triangles in entry!");
	// sometimes collision data is not streamed in (???):
	//Assertf(pColModel->m_nNoOfTriangles == pEntry->m_nNumTris, "Different number of ColTris in CEntity & cached entry!");
	if(pBound->GetNumPolygons() != pEntry->m_nNumTris)
	{
	#if !__FINAL
		Displayf("\n [*]_ProcessEntryCollisionData(): entry was skipped because no collision data was found!\n"); 
		Displayf("Different number of ColTris in CEntity & cached entry! (pBound: %d, pEntry: %d).", pBound->GetNumPolygons(), pEntry->m_nNumTris); 
	#endif
		return(FALSE);
	}

	Mat34V vBoundMat(V_IDENTITY);
	if(!pEntry->m_bBoundMatIdentity)
	{
		vBoundMat = RCC_MAT34V(pEntry->m_BoundMat);
	}
	const Vec3V vCamPos = RCC_VEC3V(camPos);
	const u32 count = pEntry->m_nNumTris;

#if __SPU
	atBitSetSpu& boundMatProps = *Alloca(atBitSetSpu,1);
	sysMemCpy(&boundMatProps, &pEntry->m_BoundMatProps, sizeof(atBitSet));	
	boundMatProps.Fetch(Alloca(unsigned, boundMatProps.Size()+8), PLANTS_DMATAGID);
	
	CTriHashIdx16* pPpuLocTriArray = pEntry->m_LocTriArray;
	pEntry->m_LocTriArray = Alloca(CTriHashIdx16, count);
	sysDmaLargeGet(pEntry->m_LocTriArray, (u64)pPpuLocTriArray, count * sizeof(CTriHashIdx16), PLANTS_DMATAGID);

	Fetch(pBound->m_MaterialIds, pBound->m_NumMaterials, Alloca(phMaterialMgr::Id, pBound->m_NumMaterials), PLANTS_DMATAGID);

	Fetch(pBound->m_PolyMatIndexList, pBound->m_NumPolygons, Alloca(u8, pBound->m_NumPolygons), PLANTS_DMATAGID);

	#if HACK_GTA4_64BIT_MATERIAL_ID_COLORS
		if(pBound->m_NumMaterialColors)
		{
			// round m_NumMaterialColors off to be multiple of 4 (to dma multiple of 16 bytes):
			const u32 numMaterialColors4 = (pBound->m_NumMaterialColors+3)&0xfc;
			Fetch(pBound->m_MaterialColors, numMaterialColors4, Alloca(u32, numMaterialColors4), PLANTS_DMATAGID);
		}
	#endif //HACK_GTA4_64BIT_MATERIAL_ID_COLORS...

	sysDmaWait(1<<PLANTS_DMATAGID);
#else
	atBitSet32& boundMatProps = pEntry->m_BoundMatProps;
#endif //__SPU...

	const u32 count_1_8	= count / CPLANT_ENTRY_TRILOC_PROCESS_UPDATE;	// 1/8 of everything to process
	u32 startCount0	= 0;
	u32 stopCount0	= 0;
	if(count_1_8 && (iTriProcessSkipMask!=CPLANT_ENTRY_TRILOC_PROCESS_ALWAYS))
	{
		startCount0	= iTriProcessSkipMask * count_1_8;
		stopCount0	= (iTriProcessSkipMask!=CPLANT_ENTRY_TRILOC_PROCESS_UPDATE-1)?((iTriProcessSkipMask+1)*count_1_8):(count);
	}
	else
	{	// case when count < CPLANT_ENTRY_TRILOC_PROCESS_UPDATE:
		startCount0	= 0;
		stopCount0	= count;
	}
	const u32 startCount= startCount0;
	const u32 stopCount	= stopCount0;
#if !__SPU
	PrefetchDC(&(pEntry->m_LocTriArray[startCount]));
	const CTriHashIdx16* pLocTriArrayPrefetch = pEntry->m_LocTriArray + 1;
	const phPolygon* pPolyArrayPrefetch = pBound->GetPolygonPointer() + 1;
#endif
	for(u32 i=startCount; i<stopCount; i++)
	{

#if !__SPU
		PrefetchDC(pLocTriArrayPrefetch + i);
		PrefetchDC(pPolyArrayPrefetch + i);
#endif
		const CTriHashIdx16 hashIdx	= pEntry->m_LocTriArray[i];
		const u16 loctri	= hashIdx.GetIdx();
		const u16 listID	= hashIdx.GetListID();

#if PLANTSMGR_MULTI_UPDATE
		if(pEntry->m_processedTris.IsSet(i))	// was already processed by other subtask this frame?
			continue;
#endif

		if(loctri)
		{	
			if(triTab.m_listID==listID)	// do rejection test only if tri belongs to currently processed list
			{
			#if PLANTSMGR_MULTI_UPDATE
				pEntry->m_processedTris.Set(i);	// mark tri as processed/visited
			#endif

				CPlantLocTri* pLocTri = &triTab[loctri];

				// check if this triangle creates objects but hasn't created any yet and re process
				if(pLocTri->m_bCreatesObjects)
				{
					if(!pLocTri->m_bCreatedObjects)
					{
						if(!m_bSuppressObjCreation && CProcObjectMan::ProcessTriangleAdded(triTab, pLocTri))					
						{
							// objects have been created on this triangle ok
							pLocTri->m_bCreatedObjects = true;					
						}
					}
					else
					{	// see if tri is too far away and try to remove it:
						const float distTriToCamSq = (camPos - pLocTri->GetCenter()).Mag2();
					#if __SPU
						s32 procTagId = phMaterialMgrGta::UnpackProcId(pLocTri->m_nSurfaceType);
					#else
						s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
					#endif
						s32 procObjInfoIndex = g_procInfo.m_procTagTable[procTagId].procObjIndex;
						// grab first maxDistSq from the group:
						const float maxDistSq = g_procInfo.m_procObjInfos[procObjInfoIndex].GetMaxDistSq() * (1.15f*1.15f);	// make it 15% bigger
						
						// check if the triangle is out of range
						if(distTriToCamSq > maxDistSq)
						{
						#if __SPU
							g_procObjMan_AddTriToRemoveList(triTab, pLocTri);
						#else
							g_procObjMan.AddTriToRemoveList(pLocTri);
						#endif
							pLocTri->m_bCreatedObjects = false;
						}
					}
				}		

				//
				// check if LocTri is in range - if not, remove it:
				//
				const Vector3 v1 = pLocTri->GetV1();
				const Vector3 v2 = pLocTri->GetV2();
				const Vector3 v3 = pLocTri->GetV3();
				const Vector3 center = pLocTri->GetCenter();
				const Vec3V positions[4] = 
				{
					RCC_VEC3V(v1),
					RCC_VEC3V(v2),
					RCC_VEC3V(v3),
					RCC_VEC3V(center)
				};
				const bool bIsAllCulled = _CullDistanceCheck(positions, vCamPos, pLocTri->m_bDrawFarTri, true) || _CullSphereCheck(positions);

				if(bIsAllCulled)
				{
					pLocTri->Release(triTab);
					pEntry->m_LocTriArray[i] = 0;
				}
			}//if(triTab.m_listID==listID)...
		}
		else
		{
			if(triTab.m_UnusedListHead)			// is there any space in trilist?
			{
			#if PLANTSMGR_MULTI_UPDATE
				pEntry->m_processedTris.Set(i);	// mark tri as processed/visited
			#endif

				bool bCreatesPlants  = boundMatProps.IsSet(i*2+0);
				const bool bCreatesObjects = boundMatProps.IsSet(i*2+1);
			#if PLANTSMGR_DATA_EDITOR
				#if __SPU
					if(g_jobParams->m_AllCollisionSelectable)
				#else
					if(GetAllCollisionSelectableUT())
				#endif
					{
						bCreatesPlants = true;
					}
			#endif //PLANTSMGR_DATA_EDITOR...

				if(!bCreatesPlants && !bCreatesObjects)
					continue;

			#if __SPU
				const phPolygon *pPolyPpu = &pBound->GetPolygon(i);
				#if 0 && USE_POLY_MRU_CACHE // direct poly fetch is actually quicker here!
					const phPolygon *pPoly = g_PolygonMruCache.GetPolygon(pPolyPpu);
				#else
					phPolygon _poly;
					sysDmaGet(&_poly, (u64)pPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
					sysDmaWait(1<<PLANTS_DMATAGID);
					const phPolygon *pPoly = &_poly;
				#endif
				DEV_ONLY(g_PolygonsFetched++;)
			#else
				const phPolygon *pPoly = &pBound->GetPolygon(i);
			#endif //__SPU...

				const phMaterialIndex boundMaterialID = pBound->GetPolygonMaterialIndex(i);
				const phMaterialMgr::Id actualMaterialID = pBound->GetMaterialId(boundMaterialID);

				CompileTimeAssert(POLY_MAX_VERTICES==CPLANT_MAX_POLY_POINTS);
				
				// generate new LocTri for this polygon (if necessary):
				Vec3V tv[3];
				phPolygon::Index ti[3];
				const phPolygon::Index vIndex0 = ti[0] = pPoly->GetVertexIndex(0);
				const phPolygon::Index vIndex1 = ti[1] = pPoly->GetVertexIndex(1);
				const phPolygon::Index vIndex2 = ti[2] = pPoly->GetVertexIndex(2);
			#if __SPU
				const CompressedVertexType* pVerts = pBound->GetCompressedVertexPointer();
				qword _buf[3][2];
				const CompressedVertexType* v0 = FetchSmallUnaligned(_buf[0], &pVerts[vIndex0*3], sizeof(qword)*2, PLANTS_DMATAGID);
				const CompressedVertexType* v1 = FetchSmallUnaligned(_buf[1], &pVerts[vIndex1*3], sizeof(qword)*2, PLANTS_DMATAGID);
				const CompressedVertexType* v2 = FetchSmallUnaligned(_buf[2], &pVerts[vIndex2*3], sizeof(qword)*2, PLANTS_DMATAGID);
				sysDmaWait(1<<PLANTS_DMATAGID);
				tv[0] = pBound->DecompressVertex(v0);
				tv[1] = pBound->DecompressVertex(v1);
				tv[2] = pBound->DecompressVertex(v2);
			#else
				tv[0] = pBound->GetVertex(vIndex0);
				tv[1] = pBound->GetVertex(vIndex1);
				tv[2] = pBound->GetVertex(vIndex2);
			#endif

				// transform points if required
				if(!pEntry->m_bBoundMatIdentity)
				{
					tv[0] = Transform(vBoundMat, tv[0]);
					tv[1] = Transform(vBoundMat, tv[1]);
					tv[2] = Transform(vBoundMat, tv[2]);
				}

				// Select distance to use randomly
				const bool bIsDrawFarTri	= (vIndex0&0x03)==3;

				const Vec3V positions[4] = 
				{
					tv[0],
					tv[1],
					tv[2],
					(tv[0]+tv[1]+tv[2]) * ScalarV(V_THIRD)
				};
				const bool bIsNotCulled = _CullDistanceCheck(positions, vCamPos, bIsDrawFarTri, false) && (!_CullSphereCheck(positions));
		
				if(bIsNotCulled)
				{
					// this triangle creates plants or objects - add it to the list
					const u16 loctri = triTab.m_UnusedListHead;
					CPlantLocTri *pLocTri = &triTab[loctri];

					if(pLocTri->Add(triTab, RCC_VECTOR3(tv[0]), RCC_VECTOR3(tv[1]), RCC_VECTOR3(tv[2]), actualMaterialID, bCreatesPlants, bCreatesObjects, pEntry->m__pEntity, m_ColEntCache.GetIdx(pEntry), bIsDrawFarTri))
					{
						// it's added to the list
						FastAssert(pEntry->m_LocTriArray[i]==(u16)0);
						pEntry->m_LocTriArray[i].Make( triTab.m_listID, loctri );

					#if FURGRASS_TEST_V4
						if(bCreatesPlants && pFurgrassTagPresent)
						{
							const s32 procTagId = PGTAMATERIALMGR->UnpackProcId(actualMaterialID);
							const s32 plantInfoIndex = g_procInfo.m_procTagTable[procTagId].plantIndex;
							CPlantInfo *pPlantInfo = &g_procInfo.m_plantInfos[plantInfoIndex];
							if(pPlantInfo->m_Flags.IsSet(PROCPLANT_FURGRASS))
							{
								*pFurgrassTagPresent = 0x1;
							}
						}
					#endif

					#if CPLANT_CLIP_EDGE_VERT
						bool isClippingEdge[3] = {false, false, false};
						bool isClippingVert[3] = {false, false, false};
						_extractPolyEdgeCullingFromNeighbours(pBound, static_cast<phPolygon::Index>(i), isClippingEdge, boundMatProps);
						_extractPolyVertCullingFromNeighbours(pBound, static_cast<phPolygon::Index>(i), ti, isClippingVert, boundMatProps);

						//Save off results
						pLocTri->m_ClipEdge_01 = isClippingEdge[0]; pLocTri->m_ClipEdge_12 = isClippingEdge[1]; pLocTri->m_ClipEdge_20 = isClippingEdge[2];
						pLocTri->m_ClipVert_0 = isClippingVert[0]; pLocTri->m_ClipVert_1 = isClippingVert[1]; pLocTri->m_ClipVert_2 = isClippingVert[2];
					#endif

						// unpack collision ground colors directly (if they exist):
						if(pBound->GetNumPerVertexAttribs())
						{
						#if __ASSERT
							const u32 numVertAttribs = pBound->GetNumPerVertexAttribs();
							Assert(numVertAttribs >= 1);	// at least 1 attrib must exist
						#endif
							const u32 vertAttrib=0;	// want to extract attrib 0

							// groundColorV1:
							pLocTri->m_GroundColorV1 = pBound->GetVertexAttrib(vIndex0, vertAttrib);
							// groundColorV2:
							pLocTri->m_GroundColorV2 = pBound->GetVertexAttrib(vIndex1, vertAttrib);
							// groundColorV3:
							pLocTri->m_GroundColorV3 = pBound->GetVertexAttrib(vIndex2, vertAttrib);

							// special feature: apply ScaleXYZ to 1 vert and 0 to everything else:
							if(pLocTri->m_bGroundScale1Vert)
							{
								const u8 alpha	  = pLocTri->m_GroundColorV1.GetAlpha();
								const u8 density  = CPlantLocTri::pv8UnpackDensity(alpha);
								const u8 scaleXYZ = CPlantLocTri::pv8UnpackScaleXYZ(alpha);
								const u8 scaleZ	  = CPlantLocTri::pv8UnpackScaleZ(alpha);
								pLocTri->m_GroundColorV1.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(density, scaleZ, scaleXYZ) );
								pLocTri->m_GroundColorV2.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(density, scaleZ, 0) );
								pLocTri->m_GroundColorV3.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(density, scaleZ, 0) );
							}
						}//if(pBound->GetNumPerVertexAttribs())...
						else
						{
						#if HACK_GTA4_64BIT_MATERIAL_ID_COLORS
						// unpack collision ground colors from palettes (if they exist):
						#if __SPU
							const u32 materialColorIdx = phMaterialMgrGta::UnpackMtlColour(actualMaterialID);
						#else
							const u32 materialColorIdx = PGTAMATERIALMGR->UnpackMtlColour(actualMaterialID);
						#endif
							if(materialColorIdx && pBound->m_NumMaterialColors)
							{
							#if 1
								// color arrays gathering all poly colors contributing to given vertex:
								CColorStack vtxColors0, vtxColors1, vtxColors2;

								// list of poly indexes, which contributed to vtxColors[]
								// (to avoid duplicates when deeper levels of neighbours are analised):
								CPolyCache polyCache;

								// main poly:
								const Color32 baseColor = pBound->GetMaterialColor(materialColorIdx);
								vtxColors0.Push(baseColor);
								vtxColors1.Push(baseColor);
								vtxColors2.Push(baseColor);
								polyCache.AddPoly(static_cast<phPolygon::Index>(i));

								// 1st level: 3 neighbours to base poly:
								_extractPolyColorsOfNeighbours(	pBound, &ti[0], baseColor,
																i,
																vtxColors0, vtxColors1, vtxColors2, polyCache);

								// 2nd level: all neighbours of 3 basic neighbours:
								for(u32 n=0; n<3; n++)
								{
									phPolygon::Index idxN = pPoly->GetNeighboringPolyNum(n);
									if(idxN != 0xffff)
									{
										_extractPolyColorsOfNeighbours(	pBound, &ti[0], baseColor,
																		idxN,
																		vtxColors0, vtxColors1, vtxColors2, polyCache);

										// 3rd level: all neighbours of neighbours of 3 basic neighbours:
										#if __SPU
											const phPolygon *pNPolyPpu = &pBound->GetPolygon(idxN);
											#if USE_POLY_MRU_CACHE
												const phPolygon *pNPoly = g_PolygonMruCache.GetPolygon(pNPolyPpu);
											#else
												phPolygon poly;
												sysDmaGet(&poly, (u64)pNPolyPpu, sizeof(phPolygon),PLANTS_DMATAGID);
												sysDmaWait(1<<PLANTS_DMATAGID);
												const phPolygon *pNPoly = &poly;
											#endif
											DEV_ONLY(g_PolygonsFetched++;)
										#else
											const phPolygon *pNPoly = &pBound->GetPolygon(idxN);
										#endif
										for(u32 nn=0; nn<3; nn++)
										{
											phPolygon::Index idxNN = pNPoly->GetNeighboringPolyNum(nn);
											if(idxNN != 0xffff)
											{
												_extractPolyColorsOfNeighbours(	pBound, &ti[0], baseColor,
																				idxNN,
																				vtxColors0, vtxColors1, vtxColors2, polyCache);
											}
										} // 3rd: for(u32 nn=0; nn<3; nn++)...

									}//if(idxN != 0xffff)...

								}// 2nd: end of for(u32 n=0; n<3; n++)...

								// groundColorV1:
								pLocTri->m_GroundColorV1 = vtxColors0.GetMediumColor();
								// groundColorV2:
								pLocTri->m_GroundColorV2 = vtxColors1.GetMediumColor();
								// groundColorV3:
								pLocTri->m_GroundColorV3 = vtxColors2.GetMediumColor();

								if(1)
								{
								#if 1
									// BS#1231422: disable interpolation for Density, ScaleXYZ and ScaleZ:
									const u8 alpha = baseColor.GetAlpha();
									pLocTri->m_GroundColorV1.SetAlpha(alpha);
									pLocTri->m_GroundColorV2.SetAlpha(alpha);
									pLocTri->m_GroundColorV3.SetAlpha(alpha);

									// special feature: apply ScaleXYZ to 1 vert and 0 to everything else:
									if(pLocTri->m_bGroundScale1Vert)
									{
										const u8 density  = CPlantLocTri::pv8UnpackDensity(alpha);
										const u8 scaleXYZ = CPlantLocTri::pv8UnpackScaleXYZ(alpha);
										const u8 scaleZ	  = CPlantLocTri::pv8UnpackScaleZ(alpha);
										pLocTri->m_GroundColorV1.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(density, scaleZ, scaleXYZ) );
										pLocTri->m_GroundColorV2.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(density, scaleZ, 0) );
										pLocTri->m_GroundColorV3.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(density, scaleZ, 0) );
									}
								#else
									u8 alpha[3];
									alpha[0] = pLocTri->m_GroundColorV1.GetAlpha();
									alpha[1] = pLocTri->m_GroundColorV2.GetAlpha();
									alpha[2] = pLocTri->m_GroundColorV3.GetAlpha();
									
									// calculate medium density for all 3 tri verts:
									const float fDensity1 = CPlantLocTri::pv8UnpackDensity(alpha[0]);
									const float fDensity2 = CPlantLocTri::pv8UnpackDensity(alpha[1]);
									const float fDensity3 = CPlantLocTri::pv8UnpackDensity(alpha[2]);
									const float fDensityM = (fDensity1+fDensity2+fDensity3) / 3.0f;
									const u8    nDensityM = u8(fDensityM+0.5f);

									u8	scaleXYZ[3], scaleZ[3];
									scaleXYZ[0] = CPlantLocTri::pv8UnpackScaleXYZ(alpha[0]);
									scaleXYZ[1] = CPlantLocTri::pv8UnpackScaleXYZ(alpha[1]);
									scaleXYZ[2] = CPlantLocTri::pv8UnpackScaleXYZ(alpha[2]);
									scaleZ[0]	= CPlantLocTri::pv8UnpackScaleZ(alpha[0]);
									scaleZ[1]	= CPlantLocTri::pv8UnpackScaleZ(alpha[1]);
									scaleZ[2]	= CPlantLocTri::pv8UnpackScaleZ(alpha[2]);

									//... and pack it back:
									pLocTri->m_GroundColorV1.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(nDensityM, scaleZ[0], scaleXYZ[0]) );
									pLocTri->m_GroundColorV2.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(nDensityM, scaleZ[1], scaleXYZ[1]) );
									pLocTri->m_GroundColorV3.SetAlpha( CPlantLocTri::pv8PackDensityScaleZScaleXYZ(nDensityM, scaleZ[2], scaleXYZ[2]) );
								#endif
								}
							#else
								// single per face color version:
								pLocTri->m_GroundColorV1 =
								pLocTri->m_GroundColorV2 =
								pLocTri->m_GroundColorV3 = pBound->GetMaterialColor(materialColorIdx);
							#endif
							}// if(!bGroundColorsUnpacked && materialColorIdx && pBound->m_NumMaterialColors)...
						#endif //HACK_GTA4_64BIT_MATERIAL_ID_COLORS...
						}
					}//if(pLocTri->Add(...)...
				}// if(colDistSqrXX < CPLANT_TRILOC_FAR_DIST_SQR))...
	
			} //if(triTab.m_UnusedListHead)...
		} //if(loctri)...
	} //for(u32 i=startCount; i<stopCount; i++)...

#if __SPU
	sysDmaLargePut(pEntry->m_LocTriArray, (u64)pPpuLocTriArray, count * sizeof(CTriHashIdx16),PLANTS_DMATAGID);
	sysDmaWait(1<<PLANTS_DMATAGID);
	pEntry->m_LocTriArray = pPpuLocTriArray;
#endif

	return(TRUE);
}// end of CPlantMgr::_ProcessEntryCollsionData()...


//
//
//
//
CPlantLocTri* CPlantMgr::MoveLocTriToList(CPlantLocTriArray& triTab, u16* ppCurrentList, u16* ppNewList, CPlantLocTri *pTri)
{
	Assertf(*ppCurrentList, "CPlantMgr::MoveLocTriToList(): m_CurrentList==NULL!");

	// First - Cut out of old list
	if(!pTri->m_PrevTri)
	{
		// if at head of old list
		*ppCurrentList = pTri->m_NextTri;
		if(*ppCurrentList)
			triTab[*ppCurrentList].m_PrevTri = 0;
	}
	else if(!pTri->m_NextTri)
	{
		// if at tail of old list
		triTab[pTri->m_PrevTri].m_NextTri = 0;
	}
	else
	{	// else if in middle of old list
		triTab[pTri->m_NextTri].m_PrevTri = pTri->m_PrevTri;
		triTab[pTri->m_PrevTri].m_NextTri = pTri->m_NextTri;
	}

	// Second - Insert at start of new list
	pTri->m_NextTri	= *ppNewList;
	pTri->m_PrevTri	= 0;
	*ppNewList = triTab.GetIdx(pTri);
	
	if(pTri->m_NextTri)
		triTab[pTri->m_NextTri].m_PrevTri = *ppNewList;

	return(pTri);
}// end of CPlantMgr::MoveLocTriToList()...



//
// CProcObjectMan:
//
///////////////////////////////////////////////////////////////////////////////
//  AddObjects
///////////////////////////////////////////////////////////////////////////////
s32 CProcObjectMan::AddObjects(CPlantLocTriArray& SPU_ONLY(triTab), CProcObjInfo* pProcObjInfo, CPlantLocTri* pLocTri)
{
	// check if the triangle is out of range
#if __SPU
	float distTriToCamSq = (g_jobParams->m_camPos  - pLocTri->GetCenter()).Mag2();
#else
	float distTriToCamSq = (camInterface::GetPos() - pLocTri->GetCenter()).Mag2();
#endif

	// minDist:
	if(distTriToCamSq < pProcObjInfo->GetMinDistSq() REPLAY_ONLY(&& !CReplayMgr::IsReplayInControlOfWorld()))
	{
#if __BANK
	#if __SPU
		if (!g_jobParams->m_ignoreMinDist)
	#else
		if (!g_procObjMan.m_ignoreMinDist)
	#endif
		{
			return 0;
		}
#else // __BANK
		return 0;
#endif // __BANK
	}

	// maxDist:
	if(distTriToCamSq > pProcObjInfo->GetMaxDistSq())
	{
		return 0;	// do not create objects if they are too far away
	}

const Vector3 v1 = pLocTri->GetV1();
const Vector3 v2 = pLocTri->GetV2();
const Vector3 v3 = pLocTri->GetV3();

	// get the vectors along the sides of the triangle
	Vector3 p1p2 = v2-v1;
	// Vector3 p2p3 = v3-v2;
	Vector3 p1p3 = v3-v1;
	Vector3 normal;
	normal.Cross(p1p2, p1p3);

// keep track of how many objects are added
s32 numObjectsAdded = 0;

	// check if they should be placed in a regular grid formation
	if (pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_GRID))
	{
		// placed in regular grid - find the min and max axis aligned bounding box of this triangle
		float minX = rage::Min(v1.x, v2.x);		minX = rage::Min(minX, v3.x);
		float minY = rage::Min(v1.y, v2.y);		minY = rage::Min(minY, v3.y);
		float maxX = rage::Max(v1.x, v2.x);		maxX = rage::Max(maxX, v3.x);
		float maxY = rage::Max(v1.y, v2.y);		maxY = rage::Max(maxY, v3.y);

		// calculate the grid start and end loops
		const float spacing = pProcObjInfo->m_Spacing.GetFloat32_FromFloat16();
		float startX = (s32)(minX/spacing)*spacing;
		float endX =   ((s32)(maxX/spacing)+1)*spacing;
		float startY = (s32)(minY/spacing)*spacing;
		float endY =   ((s32)(maxY/spacing)+1)*spacing;

		normal.Normalize();

		for (float i=startX; i<endX; i+=spacing)
		{
			for (float j=startY; j<endY; j+=spacing)
			{
				float z;
				if (CPlantLocTri::IsPtInTriangle2D(i, j, v1, v2, v3, normal, &z))
				{
					Vector3 currPos(i, j, z);
#if __SPU
					g_procObjMan_AddObjectToAddList(triTab, currPos, normal, pProcObjInfo, pLocTri);
#else
					g_procObjMan.AddObjectToAddList(currPos, normal, pProcObjInfo, pLocTri);
#endif
					numObjectsAdded++;
				}
			}
		}
	}
	else
	{
		// randomly placed - calculate the area of the triangle
		float triArea = normal.Mag() * 0.5f;
		float numObjects = triArea * pProcObjInfo->GetDensity();

#if __BANK
	#if __SPU
		if (g_jobParams->m_forceOneObjPerTri)
	#else
		if (g_procObjMan.m_forceOneObjPerTri)
	#endif
		{
			numObjects = 1.0f;
		}
#endif

		// set the seed for this triangle
		#if __SPU
			mthRandom& drawRand = g_DrawRandSpu;
		#else
			mthRandom& drawRand = g_DrawRand;
		#endif
		s32 storedSeed = 0;
#if __BANK
	#if __SPU
		if((pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_SEED) && g_jobParams->m_ignoreSeeding==false) || g_jobParams->m_enableSeeding)
	#else
		if((pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_SEED) && g_procObjMan.m_ignoreSeeding==false) || g_procObjMan.m_enableSeeding)
	#endif
#else
		if (pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_SEED))
#endif
		{
			storedSeed = drawRand.GetSeed();

//			const float seedF = (v1.x + v2.y + v3.z + pProcObjInfo->m_ModelIndex) * v1.x * pProcObjInfo->m_ModelIndex;
//			const u32 seed = (u32)seedF;
			drawRand.Reset((pLocTri->GetSeed() ^ (pProcObjInfo->m_ModelName.GetHash() >> (pLocTri->GetSeed()&0x000f))));
		}

		// try to place the objects
		while (numObjects>0)
		{
			float chance = 1.0f;
			if (numObjects<1.0f)
			{
				chance = drawRand.GetRanged(0.0f, 1.0f);
			}

			if (chance<=numObjects)
			{
				float s = drawRand.GetRanged(0.0f, 1.0f);
				float t = drawRand.GetRanged(0.0f, 1.0f);
				if((s+t) > 1.0f)
				{
					s = 1.0f-s;
					t = 1.0f-t;
				}
				FastAssert(s+t<=1.0f);

//				Vector3 pos = v1 + s*p1p2 + t*p1p3;
				const float a = s;
				const float b = t;
				const float c = 1.0f - s - t;
				Vector3 pos;
				pos.x = a*v1.x + b*v2.x + c*v3.x;
				pos.y = a*v1.y + b*v2.y + c*v3.y;
				pos.z = a*v1.z + b*v2.z + c*v3.z;

				normal.Normalize();
#if __SPU
				g_procObjMan_AddObjectToAddList(triTab, pos, normal, pProcObjInfo, pLocTri);
#else
				g_procObjMan.AddObjectToAddList(pos, normal, pProcObjInfo, pLocTri);
#endif
				numObjectsAdded++;
			}

			numObjects -= 1.0f;
		}

#if __BANK
	#if __SPU
		if((pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_SEED) && g_jobParams->m_ignoreSeeding==false) || g_jobParams->m_enableSeeding)
	#else
		if((pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_SEED) && g_procObjMan.m_ignoreSeeding==false) || g_procObjMan.m_enableSeeding)
	#endif
#else
		if (pProcObjInfo->m_Flags.IsSet(PROCOBJ_USE_SEED))
#endif
		{
			drawRand.Reset(storedSeed);
		}
	}

#if __BANK
	#if __SPU
		if (g_jobParams->m_forceOneObjPerTri)
	#else
		if (g_procObjMan.m_forceOneObjPerTri)
	#endif
		{
			FastAssert(numObjectsAdded==1);
		}
#endif

	return numObjectsAdded;
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessTriangleAdded
///////////////////////////////////////////////////////////////////////////////
s32 CProcObjectMan::ProcessTriangleAdded(CPlantLocTriArray& triTab, CPlantLocTri* pLocTri)
{
	FastAssert(pLocTri);

#if __BANK
	#if __SPU
		if (g_jobParams->m_disableCollisionObjects)
	#else
		if (g_procObjMan.m_disableCollisionObjects)
	#endif
		{
			return 0;
		}
#endif // __BANK

#if __SPU
	// don't add objects if there are less than this many free slots in the AddList
	const s32 THRESHOLD_DONT_ADD_OBJECTS = 512;
	
	if (g_jobParams->m_maxAdd < THRESHOLD_DONT_ADD_OBJECTS)
		return 0;
#endif

s32 numObjectsAdded = 0;

	// add an object here
#if __SPU
	s32 procTagId = phMaterialMgrGta::UnpackProcId(pLocTri->m_nSurfaceType);
#else
	s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
#endif
	s32 procObjInfoIndex = g_procInfo.m_procTagTable[procTagId].procObjIndex;
	atHashValue procTagHash = g_procInfo.m_procObjInfos[procObjInfoIndex].m_Tag;

	while (procObjInfoIndex<g_procInfo.m_procObjInfos.GetCount() && g_procInfo.m_procObjInfos[procObjInfoIndex].m_Tag == procTagHash)
	{
		CProcObjInfo *pProcObjInfo = &g_procInfo.m_procObjInfos[procObjInfoIndex];

	#if __SPU
		if(pProcObjInfo->m_Flags.IsSet(PROCOBJ_NETWORK_GAME) || (!g_jobParams->m_IsNetworkGameInProgress))
	#else
		if(pProcObjInfo->m_Flags.IsSet(PROCOBJ_NETWORK_GAME) || (!NetworkInterface::IsGameInProgress()))
	#endif
		{
			numObjectsAdded += CProcObjectMan::AddObjects(triTab, pProcObjInfo, pLocTri);
		}

		procObjInfoIndex++;
	}

#if __SPU
	// assert if bail out threshold is too low for this triangle
//	FastAssert(numObjectsAdded <= (s32)THRESHOLD_DONT_ADD_OBJECTS);
#endif

	return numObjectsAdded;
}

#if !__SPU
s32 CProcObjectMan::ProcessTriangleAdded(CPlantLocTri* pLocTri)
{
	CPlantLocTriArray* fakeLocTriArray = (CPlantLocTriArray*)NULL;	// HACK, but locTriArray is used only by SPU version of ProcessTriangleAdded()
	return CProcObjectMan::ProcessTriangleAdded(*fakeLocTriArray, pLocTri);
}
#endif

#endif //__PLANTSMGR_UPDATE_COMMON_H__....

