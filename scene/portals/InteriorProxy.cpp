// Title	:	InteriorProxy.cpp
// Author	:	John Whyte
// Started	:	30/1/2012

#include "scene/portals/InteriorProxy.h"

// Rage headers
#include "diag/channel.h"
#include "parser/macros.h"
#include "parser/manager.h"
#include "streaming/CacheLoader.h"
#include "streaming/packfilemanager.h"
#include "vectormath/mat34v.h"

// framework
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwsys/metadatastore.h"
#include "fwutil/keygen.h"
#include "streaming/streamingvisualize.h"
#include "fwnet/netchannel.h"

// game headers
#include "audio/northaudioengine.h"
#include "control/replay/replay.h"
#include "modelInfo/MloModelInfo.h"
#include "network/General/NetworkUtil.h"
#include "peds/ped.h"
#include "scene/scene_channel.h"
#include "scene/loader/mapFileMgr.h"
#include "scene/lod/lodMgr.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/interiorgroupmanager.h"
#include "scene/portals/Portal.h"
#include "scene/world/GameWorld.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"
#include "scene/dlc_channel.h"

STREAMING_OPTIMISATIONS()

PARAM(NoStaticIntCols, "disable using static bounds for interior collisions");
	
RAGE_DEFINE_SUBCHANNEL(net, assetverifier_interior, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_assetverifier_interior

FW_INSTANTIATE_CLASS_POOL(CInteriorProxy, CONFIGURED_FROM_FILE, atHashString("InteriorProxy",0x529485a1));

fwPtrListSingleLink CInteriorProxy::ms_immediateStateUpdateList;
u32 CInteriorProxy::ms_currExecutingCS = 0;
atArray<SInteriorOrderData> CInteriorProxy::m_orderData;

#if __BANK

extern bool bFilterState;

bool	CInteriorProxy::m_IsSorted = false;
atArray<atHashString> CInteriorProxy::m_unorderedProxies;

#endif //__BANK

class CInteriorProxyFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		dlcDebugf3("CInteriorProxyFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		switch(file.m_fileType)
		{
			case CDataFileMgr::INTERIOR_PROXY_ORDER_FILE: CInteriorProxy::AddProxySortOrder(file.m_filename); break;
			default: return false;
		}

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file)
	{
		dlcDebugf3("CInteriorProxyFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);

		switch(file.m_fileType)
		{
			case CDataFileMgr::INTERIOR_PROXY_ORDER_FILE: CInteriorProxy::RemoveProxySortOrder(file.m_filename); break;
			default: break;
		}

	}

} g_InteriorProxyFileMounter;

void CInteriorProxy::AddProxySortOrder(const char* filePath)
{
	SInteriorOrderData newOrderData;

	if (PARSER.LoadObject(filePath, NULL, newOrderData))
	{
		atHashString filePathHash = atHashString(filePath);

		newOrderData.m_filePathHash = filePathHash.GetHash();

		// We keep the proxy orders around once we have been given them so we can validate overlaps
		// If we have had this one before, just ignore it
		if (m_orderData.Find(newOrderData) != -1)
			return;

		ASSERT_ONLY
		(
			const u32 DLC_INT_BUMP_SIZE = 100;
			const u32 DISK_PROXY_COUNT = 475;
			s32 newOrderDataMaxIndex = newOrderData.m_startFrom + (newOrderData.m_proxies.GetCount() - 1);
			atBinaryMap<u32, u32> dupeCheckMap;

			dupeCheckMap.Reserve(DLC_INT_BUMP_SIZE);

			if (Verifyf(newOrderData.m_startFrom >= DISK_PROXY_COUNT, "AddProxySortOrder - Invalid start from! %i - [%s]", newOrderData.m_startFrom, filePath))
			{
				for (u32 i = 0; i < m_orderData.GetCount(); i++)
				{
					SInteriorOrderData& currOrderData = m_orderData[i];
					s32 currOrderDataMaxIndex = currOrderData.m_startFrom + (currOrderData.m_proxies.GetCount() - 1);
					atHashString currFilePathHash = atHashString(currOrderData.m_filePathHash);
					bool overlap = currOrderData.m_filePathHash != filePathHash.GetHash() &&
						((newOrderData.m_startFrom >= currOrderData.m_startFrom && newOrderData.m_startFrom <= currOrderDataMaxIndex) ||
						(newOrderDataMaxIndex >= currOrderData.m_startFrom && newOrderDataMaxIndex <= currOrderDataMaxIndex));

					if (overlap)
					{
						Quitf(ERR_GEN_INTERIOR_1,"AddProxySortOrder - Indices overlap! [%i -> %i] vs [%i -> %i] - [%s] vs [%s]", newOrderData.m_startFrom, 
							newOrderDataMaxIndex, currOrderData.m_startFrom, currOrderDataMaxIndex, filePath, currFilePathHash.TryGetCStr());
					}

					for (u32 j = 0; j < currOrderData.m_proxies.GetCount(); j++)
						dupeCheckMap.SafeInsert(currOrderData.m_proxies[j].GetHash(), currOrderData.m_filePathHash);
				}
			}
			else
				return;

			dupeCheckMap.FinishInsertion();

			for (u32 i = 0; i < newOrderData.m_proxies.GetCount(); i++)
			{
				if (u32* pathHash = dupeCheckMap.SafeGet(newOrderData.m_proxies[i].GetHash()))
				{
					if ((*pathHash) != newOrderData.m_filePathHash)
					{
						Assertf(false, "AddProxySortOrder - Duplicate interior reference! %s - [%s]",
							newOrderData.m_proxies[i].TryGetCStr(), filePath);

						return;
					}
				}
			}
		)

		m_orderData.PushAndGrow(newOrderData, 1);
	}
}

void CInteriorProxy::RemoveProxySortOrder(const char* /*filePath*/)
{

}

CInteriorProxy::CInteriorProxy(void){

	m_pIntInst = NULL;
	m_mapDataSlotIndex = -1;
	m_staticBoundsSlotIndex = -1;

	m_retainList.Flush();
	m_modelName.Clear();


	m_currentState = PS_NONE;
	m_stateCooldown = 0;
	m_requestingModule = 0;
	ResetStateArray();

	m_groupId = 0;

	m_bIsPinned = false;
	m_bRequiredByLoadScene = false;
	m_bIsStaticBoundsReffed = false;

	m_bSkipShellAssetBlocking = false;

	m_bIsDisabled = false;
	m_bIsCappedAtPartial = false;

	m_bDisabledByCode = false;

	m_bRegenLightsOnAdd = false;

	m_activeEntitySets.Reset();

	//sceneDisplayf("<ENTITY SET RESET> : Reset proxy IDX %d", GetPool()->GetJustIndex(this));

	m_sourceChangeSet = 0;

#if __BANK
	m_disabledByDLC = false;
#endif
}

CInteriorProxy::~CInteriorProxy(void)
{
	m_activeEntitySets.Reset();

	//sceneDisplayf("<ENTITY SET RESET> : Reset  proxy IDX %d", GetPool()->GetJustIndex(this));
}

void CInteriorProxy::RegisterMountInterface()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::INTERIOR_PROXY_ORDER_FILE, &g_InteriorProxyFileMounter, eDFMI_UnloadFirst);
}

CInteriorProxy* CInteriorProxy::Add(CMloInstanceDef* pMLODef, strLocalIndex mapDataSlotIndex, spdAABB& bbox)
{
	Assert(pMLODef);

	CInteriorProxy* pProxy = NULL;

#if __BANK
	atHashString mapStoreHash = fwMapDataStore::GetStore().GetHash(mapDataSlotIndex);
#endif

	// See if this proxy needs to be allocated at a specific index
	for (u32 i = 0; i < m_orderData.GetCount(); i++)
	{
		SInteriorOrderData& currOrderData = m_orderData[i];

		u32 mapStoreHash = fwMapDataStore::GetStore().GetHash(mapDataSlotIndex);

		for (u32 j = 0; j < currOrderData.m_proxies.GetCount(); j++)
		{
			if (mapStoreHash == currOrderData.m_proxies[j])
			{
				s32 allocationIndex = currOrderData.m_startFrom + j;

				Assertf(GetPool()->GetIsFree(allocationIndex),"interiorProxy slot %d is not free for map %s. Probably too many unrecognised interiorProxies! ", allocationIndex, fwMapDataStore::GetStore().GetName(mapDataSlotIndex));

				if (Verifyf(allocationIndex < GetPool()->GetSize(),
						"CInteriorProxy::Add - Allocation index is bigger than pool capacity! [%i / %i]", allocationIndex, GetPool()->GetSize()))
				{
					// Shift the index to account for the flags and call placement new on our returned memory
					pProxy = GetPool()->New(allocationIndex << 8);

					if (pProxy)
					{
						rage_placement_new(pProxy) CInteriorProxy;
						pProxy->SetChangeSetSource(ms_currExecutingCS);
					}
				}

				// Break out of both loops now
				i = m_orderData.GetCount() - 1;
				j = currOrderData.m_proxies.GetCount() - 1;
			}
		}
	}

	if (!pProxy)
	{
		// Only non DLC interiors can be new and not have an associated index entry...
		Assertf(ms_currExecutingCS == 0, "CInteriorProxy::Add - Proxy %s is missing slot order data, this will lead to inconsistent proxy orders",
			fwMapDataStore::GetStore().GetName(mapDataSlotIndex));

	#if __BANK
		if (ms_currExecutingCS != 0)
		{
			if (m_unorderedProxies.Find(mapStoreHash) == -1)
				m_unorderedProxies.PushAndGrow(mapStoreHash);
		}
	#endif

		pProxy = rage_new CInteriorProxy;
	}
	else
	{
	#if __BANK
		if (ms_currExecutingCS != 0)
		{
			m_unorderedProxies.DeleteMatches(mapStoreHash);
		}
	#endif
	}

	Assert(pProxy);

	if (pProxy)
	{
		pProxy->InitFromMLOInstanceDef(pMLODef, mapDataSlotIndex.Get(), bbox);
		sceneDisplayf("add interiorProxy @ slot %d : map %s", GetPool()->GetJustIndex(pProxy) , fwMapDataStore::GetStore().GetName(mapDataSlotIndex));
	}

	return(pProxy);
}

CInteriorProxy* CInteriorProxy::Overwrite(CMloInstanceDef* pMLODef, strLocalIndex mapDataSlotIndex, spdAABB& bbox)
{
	Assert(pMLODef);

	CInteriorProxy* pProxy = FindProxy(mapDataSlotIndex.Get());
	Assert(pProxy);
	if (pProxy){
		pProxy->InitFromMLOInstanceDef(pMLODef, mapDataSlotIndex.Get(), bbox);
	}

	return(pProxy);
}


int CInteriorProxy::SortProxiesByNameHash(const CInteriorProxy *a, const CInteriorProxy *b)
{
	u32 mapDataStoreNameHashA = g_MapDataStore.GetHash(strLocalIndex(a->GetMapDataSlotIndex()));
	u32 mapDataStoreNameHashB = g_MapDataStore.GetHash(strLocalIndex(b->GetMapDataSlotIndex()));
	return ((mapDataStoreNameHashB > mapDataStoreNameHashA) ? 1 : -1);
}


void CInteriorProxy::Sort()
{
	atArray<CInteriorProxy> intProxiesArray;

	for(s32 i=0; i<GetPool()->GetSize(); i++)
	{
		CInteriorProxy* pProxy = GetPool()->GetSlot(i);
		if (pProxy)
		{
			if( GetPool()->IsValidPtr(pProxy))
			{
				intProxiesArray.PushAndGrow(*pProxy);
			}
		}
	}

	Assert(ms_immediateStateUpdateList.GetHeadPtr() == NULL);

	// Sort our new array
	intProxiesArray.QSort(0,-1, SortProxiesByNameHash);

#if __ASSERT
	// Check for overlapping proxies (B*896812)
	for(int i=0;i<intProxiesArray.size();i++)
	{
		CInteriorProxy *thisProxy = &intProxiesArray[i];
		for(int j=0;j<intProxiesArray.size();j++)
		{
			if( i != j )
			{
				CInteriorProxy *otherProxy = &intProxiesArray[j];

				if( thisProxy->m_modelName == otherProxy->m_modelName )
				{
					// Check the distance between them
					Vector3 thisProxyCentre = VEC3V_TO_VECTOR3(thisProxy->m_boundBox.GetCenter());
					Vector3 otherProxyCentre = VEC3V_TO_VECTOR3(otherProxy->m_boundBox.GetCenter());
					float dist = (thisProxyCentre - otherProxyCentre).Mag();

					Assertf(dist >= 3.0f, "Interior Proxies named %s within 5m of each other (%f,%f,%f) - (%f,%f,%f)",	thisProxy->m_modelName.GetCStr(),
						thisProxyCentre.x, thisProxyCentre.y, thisProxyCentre.z,
						otherProxyCentre.x, otherProxyCentre.y, otherProxyCentre.z);
				}
			}
		}
	}
#endif	//__ASSERT

	// Reset and re-init the original pool.
	ShutdownPool();
	InitPool();

	int count = intProxiesArray.size();
	for(s32 i =0; i<count; i++)
	{
		CInteriorProxy* pSortedProxy = &intProxiesArray[i];
		CInteriorProxy* pProxy = rage_new CInteriorProxy;
		*pProxy = *pSortedProxy;
	}

#if __BANK
	m_IsSorted = true;
	bool bSortCheck = true;

	sceneDisplayf("Verify interior proxy sort");
	for(s32 i=0; i<GetPool()->GetSize()-1; i++)
	{
		//sceneDebugf1("slot %d : name hash 0x%x : name %s", i,g_MapDataStore.GetHash(strLocalIndex(pProxyA->GetMapDataSlotIndex())), g_MapDataStore.GetName(strLocalIndex(pProxyA->GetMapDataSlotIndex())));
		CInteriorProxy* pProxyA = GetPool()->GetSlot(i);
		CInteriorProxy* pProxyB = GetPool()->GetSlot(i+1);
		if ((pProxyA != NULL) && (pProxyB != NULL))
		{
			sceneDebugf1("slot %d : name hash 0x%x : name %s", i,g_MapDataStore.GetHash(strLocalIndex(pProxyA->GetMapDataSlotIndex())), g_MapDataStore.GetName(strLocalIndex(pProxyA->GetMapDataSlotIndex())));
			
			u32 mapDataStoreNameHashA = g_MapDataStore.GetHash(strLocalIndex(pProxyA->GetMapDataSlotIndex()));
			u32 mapDataStoreNameHashB = g_MapDataStore.GetHash(strLocalIndex(pProxyB->GetMapDataSlotIndex()));
			
			sceneDebugf2("Compare A (%x) with B (%x) : B - A = (%x) (%d)",mapDataStoreNameHashA, mapDataStoreNameHashB, mapDataStoreNameHashB - mapDataStoreNameHashA, SortProxiesByNameHash(pProxyA,pProxyB) );
			if (SortProxiesByNameHash(pProxyA,pProxyB) > 0)
			{
				bSortCheck = false;
			}
		}
	}
	if (bSortCheck == false)
	{
		sceneDisplayf("interior proxy sort check FAILED!");
	} else
	{
		sceneDisplayf("interior proxy sort check VERIFIED");
	}
#endif	//__BANK

	sceneDisplayf("---");
	sceneDisplayf("interior proxy count : %d", GetCount());
	sceneDisplayf("interior proxy checksum : 0x%x", GetChecksum());
	sceneDisplayf("---");
}

void CInteriorProxy::InitFromMLOInstanceDef(CMloInstanceDef* pMLODef, s32 mapDataSlotIndex, spdAABB& bbox)
{
	fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(mapDataSlotIndex));

	// intercept interiors setup with .ityp dependencies here
	if (pDef->GetNumParentTypes() != 0)
	{
		SetMapDataSlotIndex(mapDataSlotIndex);

#if !defined(_MSC_VER) || !defined(_M_X64)
		m_numExitPortals = (u16) pMLODef->m_numExitPortals;
#else
		m_numExitPortals = pMLODef->m_numExitPortals;
#endif
		Assert(pMLODef->m_groupId < 255);
		m_groupId = pMLODef->m_groupId;

#if __FINAL
		m_modelName = pMLODef->m_archetypeName.GetHash();
#else
		m_modelName = pMLODef->m_archetypeName.GetCStr();
#endif

		//HACK ATTACK - interiors with only 1 way portals to the exterior get forced to have no exit portals
		// bug 1562471
		u32 archetypeHash = pMLODef->m_archetypeName.GetHash();
		if ((archetypeHash == ATSTRINGHASH("V_31_tun_swap",  0xb8d94a4f))		||
			(archetypeHash == ATSTRINGHASH("V_31_tun_04",  0x5634b76c))			||
			(archetypeHash == ATSTRINGHASH("id1_11_tunnel4_int",  0x8513eb72))	||
			(archetypeHash == ATSTRINGHASH("id1_11_tunnel5_int",  0x4b979056))	||
			(archetypeHash == ATSTRINGHASH("id1_11_tunnel7_int",  0xfba635c4))
			)
		{
			m_numExitPortals = 0;
		}

		// HACK ATTACK - the arcade interior will add lights (if needed) to script created entities which go into this interior
		if (archetypeHash == ATSTRINGHASH("ch_dlc_arcade",0x380cf46e))
		{
			m_bRegenLightsOnAdd = true;
		}

		m_initialBoundBox = bbox;

		// --- setup transform
		QuatV q(pMLODef->m_rotation.x, pMLODef->m_rotation.y, pMLODef->m_rotation.z, pMLODef->m_rotation.w);
		q = Normalize(q);
		Vec3V pos = RCC_VEC3V(pMLODef->m_position);

		m_quat = q;
		m_pos = pos;

		// get the initial data bounding sphere & transform to current position
		m_boundSphere.Set(bbox.GetCenter(), ScalarV(V_ONE));
		const ScalarV radius = m_boundSphere.InflateAABB(bbox.GetMin(), bbox.GetMax());
		m_boundSphere.GrowUniform((radius + ScalarV(V_TEN)));

 		m_boundBox.Invalidate();
 		m_boundBox.GrowSphere(m_boundSphere);

		char occlusionFileName[32];
		formatf(occlusionFileName, "%u", audNorthAudioEngine::GetOcclusionManager()->GetUniqueProxyHashkey(this));

		m_audioOcclusionMetadataFileIndex = static_cast<s16>(g_fwMetaDataStore.FindSlot(occlusionFileName).Get());

		return;
	}

	fwModelId modelId = CModelInfo::GetModelIdFromName(pMLODef->m_archetypeName);
	Assert(modelId.IsValid());

	if (!modelId.IsValid()){
		return;					// this can happen if .ityp is made non-permanent via one .imap, but other .imap doesn't
	}

	Assert(false); //needed?
}

void	CInteriorProxy::SetInteriorInst(CInteriorInst* pIntInst)
{
	m_pIntInst = pIntInst;
}

CInteriorProxy* CInteriorProxy::GetFromProxyIndex(s32 idx)
{
	Assert((idx >= 0) && (idx < CInteriorProxy::GetPool()->GetSize()));

	if ((idx >= 0) && (idx < CInteriorProxy::GetPool()->GetSize()))
	{
		return CInteriorProxy::GetPool()->GetSlot( idx);
	}

	return NULL;
}

CInteriorProxy* CInteriorProxy::GetFromLocation(const fwInteriorLocation& intLoc)
{
	Assert(intLoc.GetInteriorProxyIndex() < CInteriorProxy::GetPool()->GetSize());

	if ( intLoc.IsValid() && (intLoc.GetInteriorProxyIndex() < CInteriorProxy::GetPool()->GetSize()))
	{
		return CInteriorProxy::GetPool()->GetSlot( intLoc.GetInteriorProxyIndex() );
	}

	return NULL;
}

u64 CInteriorProxy::GenerateUniqueHashFromLocation(const fwInteriorLocation& intLoc)
{
	if(intLoc.IsValid())
	{
		CInteriorProxy* pProxy = GetFromLocation(intLoc);
		Assert(intLoc.GetInteriorProxyIndex() < CInteriorProxy::GetPool()->GetSize());

		Assertf(pProxy, "Unable to fetch proxy from interior location");

		if(pProxy)
		{
			Vector3 pos;
			//using a 64 bit hash
			//each component will use 16 bits
			pProxy->GetPosition(RC_VEC3V(pos));
			
			//let's add 32768.0f to make it positive
			pos.z += 32768.0f;
			pos.y += 32768.0f;
			pos.x += 32768.0f;
			//using these limits as we know for sure that the positions will be in 0-65535 range (16 bits)
			Assert(pos.z >= 0.0f);
			Assert(pos.z <= 65535.0f);
			Assert(pos.x >= 0.0f);	
			Assert(pos.x <= 65535.0f);
			Assert(pos.y >= 0.0f);	
			Assert(pos.y <= 65535.0f);
			
			//This will give us 1m accuracy
			u64	zPart = ((u64)(rage::Floorf(pos.z + 0.5f)));
			u64	yPart = ((u64)(rage::Floorf(pos.y + 0.5f)));
			u64	xPart = ((u64)(rage::Floorf(pos.x + 0.5f)));

			u64 hash = zPart << 48; //bit 48 to bit 63
			hash = hash | (yPart << 32); //bit 32 to bit 
			hash = hash | (xPart << 16); //bit 16 to bit 31

			hash = hash ^ ((u64)pProxy->GetNameHash());

			Assertf(hash != 0, "Generated hash is 0");

			return(hash);
		}
	}
	
	return 0;
}

float CInteriorProxy::GetHeading()
{
	Mat34V mat = GetMatrix();
	Matrix34 matrix;
	matrix = MAT34V_TO_MATRIX34(mat);
	//NOTE: An alternate method is used when the front vector is aligned with world up or down, due to Gimbal lock.
	return Selectf(Abs(matrix.b.z) - 1.0f + VERY_SMALL_FLOAT, rage::Atan2f(matrix.c.x,matrix.a.x), rage::Atan2f(-matrix.b.x, matrix.b.y));
}

// go through the pool and return the list of intersections
void CInteriorProxy::FindActiveIntersecting(const fwIsSphereIntersecting &testSphere, fwPtrListSingleLink&	scanList, bool activeOnly){

	CInteriorProxy* pIntProxy = NULL;
	CInteriorProxy* pIntProxyNext = NULL;
	s32 poolSize=CInteriorProxy::GetPool()->GetSize();

	pIntProxyNext = CInteriorProxy::GetPool()->GetSlot(0);

	s32 i = 0;
	while(i<poolSize)
	{
		pIntProxy = pIntProxyNext;

		// spin to next valid entry
		while((++i < poolSize) && (CInteriorProxy::GetPool()->GetSlot(i) == NULL));

		if (i != poolSize)
			pIntProxyNext = CInteriorProxy::GetPool()->GetSlot(i);

		// do any required processing on each interior
		// Note for the offline audio occlusion build, we need to build all paths, so grab the interior regardless of if it's active
		bool process = false;

#if __BANK
		if (audNorthAudioEngine::GetOcclusionManager()->GetIsOcclusionBuildEnabled() && pIntProxy)
		{
			process = true;
		}
		else
#endif
		{
			if (pIntProxy && (!activeOnly || pIntProxy->IsContainingImapActive()))
			{
				process = true;
			}
		}

		if (process)
		{
			spdAABB proxyBox;
			spdSphere proxySphere;
			pIntProxy->GetBoundBox( proxyBox );
			pIntProxy->GetBoundSphere( proxySphere );

			if (testSphere( proxySphere, proxyBox )){
				scanList.Add(pIntProxy);
			}
		}
	}
}

bool CInteriorProxy::IsContainingImapActive() const
{
	return( !fwMapDataStore::GetStore().GetBoxStreamer().GetIsIgnored(m_mapDataSlotIndex.Get()) );
}

CInteriorProxy*	CInteriorProxy::FindProxy(s32 mapDataDefIndex)
{
	CInteriorProxy* pIntProxy = NULL;
	CInteriorProxy* pIntProxyNext = NULL;
	s32 poolSize=CInteriorProxy::GetPool()->GetSize();

	pIntProxyNext = CInteriorProxy::GetPool()->GetSlot(0);

	s32 i = 0;
	while(i<poolSize)
	{
		pIntProxy = pIntProxyNext;

		// spin to next valid entry
		while((++i < poolSize) && (CInteriorProxy::GetPool()->GetSlot(i) == NULL));

		if (i != poolSize)
			pIntProxyNext = CInteriorProxy::GetPool()->GetSlot(i);

		// do any required processing on each interior
		if (pIntProxy && pIntProxy->m_mapDataSlotIndex == mapDataDefIndex)
		{
			return(pIntProxy);
		}
	}
	return(NULL);
}

CInteriorProxy*	CInteriorProxy::FindProxy(const u32 modelHash, const Vector3& vPositionToCheck)
{
	CInteriorProxy* pIntProxy = NULL;
	CInteriorProxy* pIntProxyNext = NULL;
	s32 poolSize=CInteriorProxy::GetPool()->GetSize();

	pIntProxyNext = CInteriorProxy::GetPool()->GetSlot(0);

	s32 i = 0;
	CInteriorProxy* pClosest = NULL;
	float closestDist2 = FLT_MAX;
	bool active = false;
	while(i<poolSize)
	{
		pIntProxy = pIntProxyNext;

		// spin to next valid entry
		while((++i < poolSize) && (CInteriorProxy::GetPool()->GetSlot(i) == NULL));

		if (i != poolSize)
			pIntProxyNext = CInteriorProxy::GetPool()->GetSlot(i);

		// do any required processing on each interior
		if (pIntProxy && pIntProxy->GetNameHash() == modelHash)
		{
			//Find the closest proxy of given model hash
			Vec3V proxyPosition;
			pIntProxy->GetPosition(proxyPosition);
			float dist2 = DistSquared(proxyPosition, VECTOR3_TO_VEC3V(vPositionToCheck)).Getf();
			// Prefer an interiorproxy that has an active imap over one that doesn't.
			// It's possible that the imap might be activated later, still allow as a last resort.
			if ((dist2 < closestDist2 && !active) || (dist2 <= closestDist2 && pIntProxy->IsContainingImapActive() && pIntProxy->GetInteriorInst()))
			{
				active = pIntProxy->IsContainingImapActive();
				pClosest = pIntProxy;
				closestDist2 = dist2;
			}
		}
	}

	return(pClosest);
}


void CInteriorProxy::GetMoverBoundsName(char* instancedBoundsName, u32 index)
{
#if __BANK
	sprintf(instancedBoundsName, "_%s_%d",GetName().GetCStr(),index);
#else //__BANK
	sprintf(instancedBoundsName, "_Proxy_%d",index);
#endif //__BANK
}

void CInteriorProxy::GetWeaponBoundsName(char* instancedBoundsName, u32 index)
{
#if __BANK
	sprintf(instancedBoundsName, "_hi@%s__%d",GetName().GetCStr(),index);
#else //__BANK
	sprintf(instancedBoundsName, "_hi@Proxy_%d",index);
#endif //__BANK
}

// add a static bound entry for every proxy. Each entry refers back to it's source proxy
void	CInteriorProxy::AddAllDummyBounds(u32 changeSetSource)
{
	if (PARAM_NoStaticIntCols.Get())
	{
		return;
	}

	atArray<atHashString>	interiorBoundsNames;

	for(u32 i=0; i< CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* pProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if (pProxy)
		{
#if !__ASSERT
			if (pProxy->GetStaticBoundsSlotIndex() != strLocalIndex::INVALID_INDEX)
			{
				continue;
			}
#endif //!__ASSERT

			if (pProxy->GetNameHash() == atHashString("dt1_03_carpark"))
			{
				u32 slotIdx = i;
				printf("found dt1_03_carpark at slot idx %d, changetSet %d", slotIdx, changeSetSource);
			}

			interiorBoundsNames.Reset();
			interiorBoundsNames.Reserve(4);

			// for the time being, archetype name == mover bounds name (if we ever have to split bounds for an interior, then need to use manifest data)
			strLocalIndex moverBoundSlot = g_StaticBoundsStore.FindSlotFromHashKey(pProxy->GetNameHash());
			if (moverBoundSlot.IsValid())
			{
				char instancedBoundsName[80] = { 0 };
				strLocalIndex instancedBoundsSlotIdx = strLocalIndex(-1);
				bool skipMover = false;

				//disable the mover bounds slot from dynamically streaming in				
				//it is instead a dependency of the instanced bounds
				g_StaticBoundsStore.GetBoxStreamer().SetIsIgnored(moverBoundSlot.Get(), true);

				pProxy->GetMoverBoundsName(instancedBoundsName, i);
				instancedBoundsSlotIdx = g_StaticBoundsStore.FindSlot(instancedBoundsName);

				// Add a slot if one doesn't exist
				if (instancedBoundsSlotIdx == -1)
					instancedBoundsSlotIdx = g_StaticBoundsStore.AddSlot(instancedBoundsName);
				else
					skipMover = (ms_currExecutingCS == 0); // Slot already exists, but no DLC is executing!

				if (!skipMover)
				{
					if (pProxy->GetStaticBoundsSlotIndex().Get() != strLocalIndex::INVALID_INDEX)
					{
						Assert(pProxy->GetStaticBoundsSlotIndex() == instancedBoundsSlotIdx);
						continue;
					}

					interiorBoundsNames.Push(instancedBoundsName);
					g_StaticBoundsStore.SetAsDummyBound(instancedBoundsSlotIdx, (InteriorProxyIndex)i, moverBoundSlot);

					pProxy->SetStaticBoundsSlotIndex(instancedBoundsSlotIdx.Get());

					spdAABB& instanceBoundingBox = g_StaticBoundsStore.GetBoxStreamer().GetBounds(instancedBoundsSlotIdx.Get());
					instanceBoundingBox = spdAABB(g_StaticBoundsStore.GetBoxStreamer().GetBounds(moverBoundSlot.Get()));
					instanceBoundingBox.Transform(pProxy->GetMatrix());

				}
				else
					Assertf(false, "CInteriorProxy::AddAllDummyBounds - Mover slot %s already exists already and we aren't executing DLC!", instancedBoundsName);
			}

			// generate weapons bounds dummy for this interior too
			u32 weaponBoundsNameHash = CMapFileMgr::GetInstance().LookupInteriorWeaponBound(pProxy->GetNameHash());
			strLocalIndex weaponBoundSlot = g_StaticBoundsStore.FindSlotFromHashKey(weaponBoundsNameHash);
			if (weaponBoundSlot.IsValid())
			{
				char instancedBoundsName[80] = { 0 };
				strLocalIndex instancedBoundsSlotIdx = strLocalIndex(-1);
				bool skipWeapon = false;

				//disable the mover bounds slot from dynamically streaming in				
				//it is instead a dependency of the instanced bounds
				g_StaticBoundsStore.GetBoxStreamer().SetIsIgnored(weaponBoundSlot.Get(), true);

				pProxy->GetWeaponBoundsName(instancedBoundsName, i);
				instancedBoundsSlotIdx = g_StaticBoundsStore.FindSlot(instancedBoundsName);

				// Add a slot if one doesn't exist
				if (instancedBoundsSlotIdx == -1)
					instancedBoundsSlotIdx = g_StaticBoundsStore.AddSlot(instancedBoundsName);
				else
					skipWeapon = (ms_currExecutingCS == 0); // Slot already exists, but no DLC is executing!

				if (!skipWeapon)
				{
					interiorBoundsNames.Push(instancedBoundsName);
					g_StaticBoundsStore.SetAsDummyBound(instancedBoundsSlotIdx, (InteriorProxyIndex)i, weaponBoundSlot);

					spdAABB& instanceBoundingBox = g_StaticBoundsStore.GetBoxStreamer().GetBounds(instancedBoundsSlotIdx.Get());
					instanceBoundingBox = spdAABB(g_StaticBoundsStore.GetBoxStreamer().GetBounds(weaponBoundSlot.Get()));
					instanceBoundingBox.Transform(pProxy->GetMatrix());

					g_StaticBoundsStore.GetBoxStreamer().SetAssetType(instancedBoundsSlotIdx.Get(), fwBoxStreamerAsset::ASSET_STATICBOUNDS_WEAPONS);
				}
				else
					Assertf(false, "CInteriorProxy::AddAllDummyBounds - Weapon bounds slot %s exists already and we aren't executing DLC!", instancedBoundsName);
			}

			if (INSTANCE_STORE.IsScriptManaged(strLocalIndex(pProxy->GetMapDataSlotIndex())) && 
				(changeSetSource == 0 || changeSetSource == pProxy->GetChangeSetSource()))
			{
				atHashString proxyImapName = INSTANCE_STORE.GetHash(strLocalIndex(pProxy->GetMapDataSlotIndex()));
				Assert(INSTANCE_STORE.FindSlotFromHashKey(proxyImapName) != -1);

				if (proxyImapName != -1)
				{
					g_StaticBoundsStore.StoreImapGroupBoundsList(proxyImapName, interiorBoundsNames);
					g_StaticBoundsStore.SetEnabled(proxyImapName.GetHash(), false);			// init bounds for IMAP groups to off
				}
			}

//			if (pProxy->GetIsDisabled())
//			{
//				pProxy->DisableAllCollisions(true);
//			}
		}
	}
}

// from an intersection, return the correct interior proxy (if the collision belongs to an interior)
CInteriorProxy*		CInteriorProxy::GetInteriorProxyFromCollisionData(CEntity* pIntersectionEntity, Vec3V const* ASSERT_ONLY(pVec))
{
		//pIntersectionEntity = CPhysics::GetEntityFromInst(pPhysInst);
		Assert(pIntersectionEntity);
		CBaseModelInfo *pModelInfo = pIntersectionEntity->GetBaseModelInfo();

		// old style - the interior physics is pushed in by the MLO entity
		if(pModelInfo && pModelInfo->GetModelType() == MI_TYPE_MLO)
		{
			CInteriorInst* pIntInst = reinterpret_cast<CInteriorInst*>(pIntersectionEntity);
			if(pIntInst)
			{
				return(pIntInst->GetProxy());
			}
		}
		else if (pIntersectionEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
		{
			strLocalIndex boundsStoreIndex = strLocalIndex(pIntersectionEntity->GetIplIndex());
			Assert(g_StaticBoundsStore.IsValidSlot(boundsStoreIndex));

			InteriorProxyIndex proxyId = -1;
			strLocalIndex depSlot;
			g_StaticBoundsStore.GetDummyBoundData(boundsStoreIndex, proxyId, depSlot);

			if (proxyId == -1)
			{
				Assertf(pVec==NULL, "[art] Room index has been set to non-zero in exterior collision data at (%.2f, %.2f, %.2f)", pVec->GetXf(), pVec->GetYf(), pVec->GetZf());
				return(NULL);
			}

			CInteriorProxy* pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(proxyId);
			Assert(pInteriorProxy);
			Assert(pInteriorProxy->GetStaticBoundsSlotIndex() == boundsStoreIndex);
			return(pInteriorProxy);
		}
		else
		{
			Assertf(false, "roomId shouldn't be set for these collisions");
// 			Assertf( false, "[Code:interiors] Room id but invalid interior entity found at (%.1f,%.1f,%.1f), Bounds file: %s",
// 				vecLineStart.x,vecLineStart.y, vecLineStart.z, pPhInst->GetArchetype()->GetFilename());
		}

		return(NULL);
}

CEntity* pDebugEntity = NULL;
s32 debugProxyID = -1;

// find the interior which is retaining this entity and remove it's reference back to the entity
void CInteriorProxy::CleanupRetainedEntity(CEntity* pEntity)
{
	sceneAssert(pEntity);
	sceneAssert(pEntity->GetIsRetainedByInteriorProxy());

	pDebugEntity = pEntity;

	s32 proxyIdx = pEntity->GetRetainingInteriorProxy();
	debugProxyID = proxyIdx;

	if ((proxyIdx > -1) && (proxyIdx < CInteriorProxy::GetPool()->GetSize())){
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot(pEntity->GetRetainingInteriorProxy());
		sceneAssert(pIntProxy);
		if (pIntProxy != NULL)
		{
			pIntProxy->RemoveFromRetainList(pEntity);
			pEntity->m_nFlags.bInMloRoom = false;
		}
	} else {
		sceneAssertf(false,"Entity %s marked as retained, but not correctly assigned to interior proxy.", pEntity->GetModelName());
	}
}

// remove reference to the entity from the retain array
void	CInteriorProxy::RemoveFromRetainList(CEntity* pEntity)
{
	sceneAssert(pEntity);
	if (pEntity){
		pEntity->SetRetainingInteriorProxy(-1);
		pEntity->SetIsRetainedByInteriorProxy(false);
		m_retainList.Remove(pEntity);
		pEntity->SetOwnerEntityContainer(NULL);		//ensure that the retain list ID isn't interpreted as a ptr later!
		if(pEntity->GetIsDynamic()) 
		{

#if __BANK
			if ((Channel_scene.FileLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_scene.TtyLevel >= DIAG_SEVERITY_DISPLAY))
			{
				netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
				if (pNetObj)
				{
					sceneDisplayf("<RETAIN> : Remove from proxy IDX %d : %s", GetPool()->GetJustIndex(this), pEntity->GetModelName());
					sceneDisplayf("net obj : %s",pNetObj->GetLogName());
					sceneDisplayf("Stack to follow:");
					sysStack::PrintStackTrace();
				}
			}
#endif //__BANK

			((CDynamicEntity*)pEntity)->RemoveFromSceneUpdate();

			// For peds, we need to make sure these cached flags are kept up to date.
			// If we want something cleaner, we could make RemoveFromSceneUpdate() virtual.
			if(pEntity->GetIsTypePed())
			{
				static_cast<CPed*>(pEntity)->UpdateCachedSceneUpdateFlags();
			}
		}
	}
}

// retained objects are code generated objects in the interior which cannot be immediately inserted (interior not in correct state)
// Usually something like script or network objects which can't be safely handled elsewhere
void CInteriorProxy::MoveToRetainList(CEntity* pEntity){

	if (pEntity && (pEntity->GetIsRetainedByInteriorProxy()==false))
	{
		if (pEntity->GetOwnerEntityContainer() != NULL){
			CGameWorld::Remove(pEntity, true);
		}

		GetRetainList().Add(pEntity);
		pEntity->SetIsRetainedByInteriorProxy(true);
		pEntity->SetRetainingInteriorProxy(static_cast<s16>( CInteriorProxy::GetPool()->GetJustIndex(this) ));

		sceneAssert( CInteriorProxy::GetPool()->GetJustIndex(this) > -1 );

		if (pEntity->GetIsPhysical()){
			CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
			if(pPhysical->RequiresProcessControl())
			{
				if(!pPhysical->GetIsOnSceneUpdate()){
					pPhysical->AddToSceneUpdate();

					// For peds, we need to make sure these cached flags are kept up to date.
					// If we want something cleaner, we could make AddToSceneUpdate() virtual.
					if(pPhysical->GetIsTypePed())
					{
						static_cast<CPed*>(pPhysical)->UpdateCachedSceneUpdateFlags();
					}
				}
			}
		}
	}
}

// process the contents of the retained entities array (add to interior if available, or forcing outside)
void CInteriorProxy::ExtractRetainedEntities(){

	fwPtrNode* pLinkNode = GetRetainList().GetHeadPtr();
	while(pLinkNode)
	{
		CDynamicEntity*	pEntity = static_cast<CDynamicEntity*>(pLinkNode->GetPtr());
		pLinkNode = pLinkNode->GetNextPtr();

		if (pEntity && pEntity->CanLeaveInteriorRetainList())
		{
			CPortalTracker *pTracker = pEntity->GetPortalTracker();
			pTracker->SetFlushedFromRetainList(true);

			REPLAY_ONLY(if(pEntity->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
			{
				pTracker->ScanUntilProbeTrue();
			}

			pTracker->Update(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
		}
	}
}

// determine if this entity can be safely removed from an interior (or not - because it belongs to script or network etc.)
bool CInteriorProxy::ShouldEntityFreezeAndRetain(CEntity *pEntity)
{
	CVehicle* pVehicle = NULL;
	CPed *pPed = NULL;
	CObject *pObject = NULL;

	netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

	switch (pEntity->GetType())
	{
	case ENTITY_TYPE_VEHICLE :
		pVehicle = (CVehicle *)pEntity;

		if (pNetObj)
		{
			if (pNetObj->IsClone())
			{
				return false;
			}
			else if (!static_cast<CNetObjProximityMigrateable*>(pNetObj)->TryToPassControlOutOfScope() || !pNetObj->CanDelete())
			{
				// we can't delete entities in the process of migrating, or which can migrate to nearby players
				return true;
			}
		}

		if (!pVehicle->PopTypeIsMission())
		{
			return false;
		}

		if (pVehicle->m_nPhysicalFlags.bDontLoadCollision == false)
		{
			return false;
		}

		break;

	case ENTITY_TYPE_PED :
		pPed = (CPed *)pEntity;

		if (pNetObj)
		{
			if (pNetObj->IsClone())
			{
				return false;
			}
			else if (!static_cast<CNetObjProximityMigrateable*>(pNetObj)->TryToPassControlOutOfScope() || !pNetObj->CanDelete())
			{
				// we can't delete entities in the process of migrating, or which can migrate to nearby players
				return true;
			}
		}

		if (!pPed->PopTypeIsMission()){
			return false;
		}
		if (pPed->IsPlayer()){
			return false;
		}

		if (pPed->m_nPhysicalFlags.bDontLoadCollision == false){
			return false;
		}

		if (pPed->GetIsAttached()){
			return false;
		}

		break;

	case ENTITY_TYPE_OBJECT :
		pObject = (CObject *)pEntity;

		// doors are never retained
		if (pObject->IsADoor()){
			return false;
		}

		if (pNetObj)
		{
			bool bAmbientObject = CNetObjObject::IsAmbientObjectType(pObject->GetOwnedBy()) && !pObject->m_nObjectFlags.bIsPickUp;

			if (!bAmbientObject)
			{
				if (pNetObj->IsClone())
				{
					return false;
				}
				else if (!static_cast<CNetObjProximityMigrateable*>(pNetObj)->TryToPassControlOutOfScope() || !pNetObj->CanDelete())
				{
					// we can't delete entities in the process of migrating, or which can migrate to nearby players
					return true;
				}
			}
		}

		// network clones and script owned objects are retained.
		if (!NetworkUtils::IsNetworkClone(pEntity)){
			if (pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT) {
				return false;
			}
		}

		break;

	default :
		return false;
	}

	return true;
}

// would be nicer as fwInteriorLocation::Set(CInteriorProxy*, s32) - but can't do that until CInteriorProxy is fwInteriorProxy
fwInteriorLocation CInteriorProxy::CreateLocation(const CInteriorProxy* pIntProxy, const s32 roomIndex)
{
	fwInteriorLocation	intLoc;

	if (pIntProxy){
		intLoc.SetInteriorProxyIndex( CInteriorProxy::GetPool()->GetJustIndex( pIntProxy ) );
	}

	if (roomIndex > -1)
	{
		Assertf(roomIndex < INTLOC_MAX_INTERIOR_ROOMS_COUNT, "%s : invalid room index : %d (max is 31)", pIntProxy->GetModelName(), roomIndex);
		intLoc.SetRoomIndex( roomIndex );
	}

	return intLoc;
}


void CInteriorProxy::ValidateLocation(fwInteriorLocation ASSERT_ONLY(loc))
{
#if __ASSERT
	if (!loc.IsValid())
	{
		return;
	}

	s32 proxyIdx = loc.GetInteriorProxyIndex();
	Assert(proxyIdx != -1);

	CInteriorProxy* pProxy = CInteriorProxy::GetPool()->GetSlot(proxyIdx);
	Assert(pProxy);

	CMloModelInfo* pMloModelInfo = dynamic_cast<CMloModelInfo*>(CModelInfo::GetBaseModelInfoFromName(pProxy->m_modelName, NULL));
	Assert(pMloModelInfo);

	if (loc.IsAttachedToRoom())
	{
		u32 roomIdx = loc.GetRoomIndex();
		Assert(roomIdx < pMloModelInfo->GetRooms().GetCount());
	}
	else if (loc.IsAttachedToPortal())
	{
		u32 portalIdx = loc.GetPortalIndex();
		Assert(portalIdx < pMloModelInfo->GetPortals().GetCount());
	}
	else
	{
		Assert(false);
	}
#endif //__ASSERT
}
// --- state management ----

void CInteriorProxy::SetRequestedState(eRequest_Module module, eProxy_State instState, bool doStateCooldown) {

	if (m_bIsDisabled || m_bDisabledByCode)
	{
		return;
	}

	sceneAssertf((instState >= PS_NONE && instState <= PS_FULL_WITH_COLLISIONS), "attempting to set invalid state for interior");
	sceneAssertf((module < RM_NUM_MODULES), "attempting to set interior state from invalid module");

	u32 stateBits = 1<<instState;
	if ((m_statesRequestedByModules[module] & stateBits) == stateBits){
		return;				// this module has already made this request
	}

	u8 oldRequestedState = m_statesRequestedByModules[module];
	m_statesRequestedByModules[module] = ( 1<< instState);
	m_stateCooldown = doStateCooldown ? STATE_COOLDOWN : 0;

	if (!ms_immediateStateUpdateList.IsMemberOfList(this)){
		ms_immediateStateUpdateList.Add(this);
	}

	if (oldRequestedState < (1 << instState))
	{
		UpdateImapPinState();
	}

}

// remove everything possible from this interior - get it into a deletable state immediately
void CInteriorProxy::CleanupInteriorImmediately(void){

	if (m_pIntInst)
	{
		m_pIntInst->DeleteDetailDrawables();
	}

	ResetStateArray();

	//we don't use ms_immediateStateUpdateList as it may contain other interiors awaiting construction
	fwPtrListSingleLink tempList;
	tempList.Add(this);

	while(m_currentState != PS_NONE){
		ImmediateStateUpdate(&tempList);
	}
}

#if __BANK
void DebugRegisterTransition(CInteriorProxy::eProxy_State fromState, CInteriorProxy::eProxy_State toState, CInteriorProxy* pProxy )
{
	if (bFilterState)
	{
		sceneDisplayf("%03d : %d -> %d \n", CInteriorProxy::GetPool()->GetJustIndex(pProxy), fromState, toState);
	}
}
#else //__BANK
void DebugRegisterTransition(CInteriorProxy::eProxy_State , CInteriorProxy::eProxy_State , CInteriorProxy*  ) {}
#endif //__BANK

void CInteriorProxy::UpdateImapPinState(void)
{
	if (m_mapDataSlotIndex == -1 || !IsContainingImapActive())
	{
		return; // bail out if we're running with -NoDynArch
	}

	bool bPinnedByScript = false;
	bool bPinnedByPlayerInGroup = false;
	bool bPinnedByPlayer = false;
	bool bPinnedByLoadScene = m_bRequiredByLoadScene;
	bool bPinnedByActiveGroup = false;
	bool bPinnedByCutscene = false;
	bool bPinnedBySpectatorCam = false;

	// check if script wants this pinned
	if (m_statesRequestedByModules[RM_SCRIPT] >= PS_FULL)
	{
		bPinnedByScript = true;
	}

	// check if cutscene wants this pinned
	if (m_statesRequestedByModules[RM_CUTSCENE] >= PS_FULL)
	{
		bPinnedByCutscene = true;
	}

	// check if shutting down this interior
	CInteriorInst* pInteriorInst = GetInteriorInst();
	if (pInteriorInst && pInteriorInst->GetIsShuttingDown())
	{
		if (fwMapDataStore::GetStore().GetBoxStreamer().GetIsPinnedByUser(m_mapDataSlotIndex.Get()))
		{
			Assertf( (!bPinnedByScript || INSTANCE_STORE.IsRemovingAllMapData())  ,"something shutting down an interior explicitly pinned by script!");
			m_bIsPinned = false;
			fwMapDataStore::GetStore().GetBoxStreamer().SetNotRequired(m_mapDataSlotIndex.Get());
			fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByUser(m_mapDataSlotIndex.Get(), false);

			if (Verifyf(m_staticBoundsSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
				g_StaticBoundsStore.GetBoxStreamer().SetIsPinnedByUser(m_staticBoundsSlotIndex.Get(), false);

			ReleaseStaticBoundsFile();
		}
		return;
	}

	// it may be necessary to choose the spectator camera _instead_ of the spectated player, if the pools blow
	if (NetworkInterface::IsInSpectatorMode() &&
		CPortalVisTracker::GetPrimaryInteriorInst() &&
		CPortalVisTracker::GetPrimaryInteriorInst()->GetGroupId() == GetGroupId() &&
		(m_statesRequestedByModules[RM_GROUP] >= PS_FULL))
	{
		bPinnedBySpectatorCam = true;
	}

	if (NetworkInterface::IsInSpectatorMode() &&
		CPortalVisTracker::GetPrimaryInteriorInst() &&
		(CPortalVisTracker::GetPrimaryInteriorInst() == GetInteriorInst()) &&
		CPortalVisTracker::GetPrimaryInteriorInst()->GetGroupId() == 0 &&
		(GetCurrentState() >= PS_FULL))
	{
		bPinnedBySpectatorCam = true;
	}

	// check if player is inside a MLO group - if so, then keep the requests MLOs pinned
	// If in MP spectator mode, this uses the ped we're spectating; otherwise it's the local player ped
	CPed* pPlayer = CGameWorld::FindFollowPlayer();
	CInteriorInst* pPlayerIntInst = pPlayer ? (pPlayer->GetPortalTracker()->GetInteriorInst()) : NULL;
	u32 playerInteriorGroupId = pPlayerIntInst ? pPlayerIntInst->GetGroupId() : 0;
	if ((m_statesRequestedByModules[RM_GROUP] >= PS_FULL) && (GetGroupId() == playerInteriorGroupId))
	{
		bPinnedByPlayerInGroup = true;
	}

	if ((m_statesRequestedByModules[RM_SHORT_TUNNEL] >= PS_FULL) && (GetGroupId() == playerInteriorGroupId))
	{
		bPinnedByPlayerInGroup = true;
	}

	// if the local player is inside a interior, then make sure it is pinned too
	if (pPlayerIntInst && pPlayerIntInst->GetProxy() == this){
		bPinnedByPlayer = true;
	}

	if (CPortal::IsActiveGroup(GetGroupId()))
	{
		bPinnedByActiveGroup = true;
	}

	if (bPinnedByScript || bPinnedByPlayerInGroup || bPinnedByPlayer || bPinnedByLoadScene || bPinnedByActiveGroup || bPinnedByCutscene || bPinnedBySpectatorCam)
	{
		sceneAssertf(!fwMapDataStore::GetStore().GetBoxStreamer().GetIsIgnored(m_mapDataSlotIndex.Get()),"Can't pin %s : .imap is disabled",m_modelName.GetCStr());

		fwMapDataStore::GetStore().StreamingRequest(m_mapDataSlotIndex, 0);
		if (!fwMapDataStore::GetStore().GetBoxStreamer().GetIsPinnedByUser(m_mapDataSlotIndex.Get()))
		{
			m_bIsPinned = true;
			fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByUser(m_mapDataSlotIndex.Get(), true);
			if (Verifyf(m_staticBoundsSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
				g_StaticBoundsStore.GetBoxStreamer().SetIsPinnedByUser(m_staticBoundsSlotIndex.Get(), true);
		}
	} else
	{
		if (fwMapDataStore::GetStore().GetBoxStreamer().GetIsPinnedByUser(m_mapDataSlotIndex.Get()))
		{
			m_bIsPinned = false;
			fwMapDataStore::GetStore().GetBoxStreamer().SetNotRequired(m_mapDataSlotIndex.Get());
			fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByUser(m_mapDataSlotIndex.Get(), false);
			if (Verifyf(m_staticBoundsSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
				g_StaticBoundsStore.GetBoxStreamer().SetIsPinnedByUser(m_staticBoundsSlotIndex.Get(), false);
		}
	}

	if(Verifyf(m_staticBoundsSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
	{
		// bring in collisions for pinned interiors
		if (g_StaticBoundsStore.GetBoxStreamer().GetIsPinnedByUser(m_staticBoundsSlotIndex.Get()))
		{
			g_StaticBoundsStore.StreamingRequest(m_staticBoundsSlotIndex, STRFLAG_INTERIOR_REQUIRED);
		} else
		{
			g_StaticBoundsStore.ClearRequiredFlag(m_staticBoundsSlotIndex.Get(), STRFLAG_INTERIOR_REQUIRED);
		}
	}
}

Mat34V_Out	CInteriorProxy::GetMatrix()						const
{
	Mat34V		matrix;

	Mat34VFromQuatV(matrix, m_quat, m_pos);

	return(matrix);
}

void CInteriorProxy::RequestContainingImapFile(void){
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::INTERIORPROXY);

	if (Verifyf(m_mapDataSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
		fwMapDataStore::GetStore().StreamingRequest(m_mapDataSlotIndex, 0);
}

void CInteriorProxy::RequestStaticBoundsFile(void){
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::INTERIORPROXY);

	if (Verifyf(m_staticBoundsSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
		g_StaticBoundsStore.StreamingRequest(m_staticBoundsSlotIndex, STRFLAG_INTERIOR_REQUIRED);
}

void CInteriorProxy::ReleaseStaticBoundsFile(void){
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::INTERIORPROXY);

	if (Verifyf(m_staticBoundsSlotIndex.Get() >= 0, "interior %s has no static bounds", GetModelName()))
		g_StaticBoundsStore.ClearRequiredFlag(m_staticBoundsSlotIndex.Get(), STRFLAG_INTERIOR_REQUIRED);
}

//go over the entire list of interiors requiring state changes and process any state transitions each frame
void CInteriorProxy::ImmediateStateUpdate(fwPtrListSingleLink* pList)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::INTERIORPROXY);

	fwPtrListSingleLink& stateUpdateList = (pList == NULL) ? ms_immediateStateUpdateList : *pList;

	fwPtrNode* pLinkNode = stateUpdateList.GetHeadPtr();	

	while(pLinkNode)
	{
		CInteriorProxy*	pIntProxy = static_cast<CInteriorProxy*>(pLinkNode->GetPtr());
		pLinkNode = pLinkNode->GetNextPtr();

		if (pIntProxy && !pIntProxy->GetIsDisabled())
		{
			if ( pIntProxy->m_stateCooldown > 0 )
			{
				--(pIntProxy->m_stateCooldown);
				continue;
			}

			eProxy_State currentState = static_cast<eProxy_State>(pIntProxy->m_currentState);
			eProxy_State targetState = static_cast<eProxy_State>(pIntProxy->m_currentState);

			// for each state (starting at highest)
			for(s32 state=(PS_NUM_STATES-1); state >= 0; state--)
			{
				// check if any of the modules is requesting this state
				for(u32 i=0; i < RM_NUM_MODULES; i++)
				{
					if (pIntProxy->m_statesRequestedByModules[i] == static_cast<u32>(1<<state)) {
						// have found the highest state requested by a module - determine target state from it.
						if (state < targetState) {
							targetState = static_cast<eProxy_State>(targetState - 1);
						} else if (state > targetState) {
							targetState = static_cast<eProxy_State>(targetState + 1);
						}
						pIntProxy->m_requestingModule = i;
						goto exitStateCheck;
					}
				}
			}

			worldAssertf(false,"Could not find any requested states");

exitStateCheck:

			// no change in state - no action needs to be taken
			if (targetState == currentState){
				pIntProxy->UpdateImapPinState();
				continue;
			}

			// we have a new target state - may need to perform some actions
			switch(targetState){
				case PS_NONE :
					{
						if (currentState == PS_PARTIAL)
						{
							// state transition {PS_PARTIAL -> PS_NONE}
							DebugRegisterTransition(PS_PARTIAL, PS_NONE, pIntProxy);

							CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
							if (pIntInst)
							{
								CPortal::RemoveFromActiveInteriorList(pIntInst);
								pIntInst->DepopulateInterior();

								if ( pIntInst->GetGroupId() && pIntInst->GetNumExitPortals() )
								{
									CInteriorGroupManager::UnregisterGroupExit( pIntInst );
								}
							}

							strLocalIndex audOccIndex = pIntProxy->GetAudioOcclusionMetadataFileIndex();
							if(g_fwMetaDataStore.IsValidSlot(audOccIndex) && g_fwMetaDataStore.IsObjectInImage(audOccIndex))
							{
								g_fwMetaDataStore.ClearRequiredFlag(audOccIndex.Get(), STRFLAG_DONTDELETE);
								g_fwMetaDataStore.StreamingRemove(audOccIndex);
							}

							pIntProxy->SetCurrentState(targetState);
							stateUpdateList.Remove(pIntProxy);		// no more immediate processing for this interior (until state changes)
						} else {
							sceneAssertf(false,"Invalid state transition");
						}
						break;
					}
				case PS_PARTIAL :
					{
						// state transition {PS_NONE -> PS_PARTIAL}
						if (currentState == PS_NONE)
						{
#if __BANK
							if(audNorthAudioEngine::GetOcclusionManager()->GetIsOcclusionBuildEnabled() && !audNorthAudioEngine::GetOcclusionManager()->ShouldAllowInteriorToLoad(pIntProxy))
							{
								break;
							}
#endif

							DebugRegisterTransition(PS_NONE, PS_PARTIAL, pIntProxy);

							CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
							if (pIntInst)
							{
								pIntInst->PopulateInteriorShell();
								pIntProxy->SetCurrentState(targetState);
							}
						} else if (currentState == PS_FULL)
						{
							// state transition {PS_FULL -> PS_PARTIAL}
							DebugRegisterTransition(PS_FULL, PS_PARTIAL, pIntProxy);

							pIntProxy->SetCurrentState(targetState);
						} else {
							sceneAssertf(false,"Invalid state transition");
						}
						break;
					}
				case PS_FULL	:
					{
						CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
						// state transition {PS_PARTIAL -> PS_FULL}
						if (currentState == PS_PARTIAL)
						{
							if (!pIntProxy->GetIsCappedAtPartial())
							{
								if (pIntInst && pIntInst->IsPopulated())
								{
									pIntProxy->SetCurrentState(targetState);		// because we could have gone : FULL -> PARTIAL -> FULL
								}
								else if (pIntProxy->GetSkipShellAssetBlocking()  || (pIntInst && pIntInst->HasShellLoaded()))
								{
									DebugRegisterTransition(PS_PARTIAL, PS_FULL, pIntProxy);

									pIntInst->PopulateInteriorContents();
									CPortal::AddToActiveInteriorList(pIntInst);

									if (pIntInst->GetGroupId() && pIntInst->GetNumExitPortals() )
									{
										CInteriorGroupManager::RegisterGroupExit( pIntInst );
									}

									strLocalIndex audOccIndex = pIntProxy->GetAudioOcclusionMetadataFileIndex();
									if(g_fwMetaDataStore.IsValidSlot(audOccIndex) && g_fwMetaDataStore.IsObjectInImage(audOccIndex))
									{
										g_fwMetaDataStore.StreamingRequest(audOccIndex, STRFLAG_DONTDELETE | STRFLAG_FORCE_LOAD);
										audNorthAudioEngine::GetOcclusionManager()->AddInteriorToMetadataLoadList(pIntProxy);
									}

									pIntProxy->SkipShellAssetBlocking(false);
									pIntProxy->SetCurrentState(targetState);
								}
							}
						} else if (currentState == PS_FULL_WITH_COLLISIONS) {
							// state transition {PS_FULL_WITH_COLLISIONS -> PS_FULL}
							DebugRegisterTransition(PS_FULL_WITH_COLLISIONS, PS_FULL, pIntProxy);
							pIntProxy->SetCurrentState(targetState);
						} else {
							sceneAssertf(false,"Invalid state transition");
						}

						break;
					}
				case PS_FULL_WITH_COLLISIONS	:
					{
						if (currentState == PS_FULL)
						{
							// state transition {PS_FULL -> PS_FULL_WITH_COLLISIONS}
							CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
							if (pIntInst)
							{
								if (pIntInst->GetCurrentPhysicsInst() || pIntProxy->HasStaticBounds()){
									DebugRegisterTransition(PS_FULL, PS_FULL_WITH_COLLISIONS, pIntProxy);
							 		pIntProxy->SetCurrentState(targetState);
								}
							}
						} else {
							sceneAssertf(false,"Invalid state transition");
						}

						break;
					}
				default :
					sceneAssert(false);
			}

			pIntProxy->UpdateImapPinState();
		}
	}
}

// this is nonsense - we should populate at some fixed distance beyond that fade dist for the MLO
// 50m say, which lets portal attached stuff fade up before the contents.
// when the .imap creates the interior, it should do so further out than the population distance too, or we'll get pops.
float CInteriorProxy::GetPopulationDistance(bool bScaled) const
{
	float		LODScale = ( bScaled ? g_LodScale.GetGlobalScale() : 1.0f );
	float		instDist;

	if (GetGroupId() == 0)
	{
		instDist = m_boundSphere.GetRadiusf() + (PORTAL_INSTANCE_DIST * LODScale);						// normal interiors
	}
	else if (GetGroupId() == 1)
	{
		instDist = m_boundSphere.GetRadiusf() + (GROUP_PORTAL_EXTERNAL_INST_DIST * LODScale);			// metro entrances
	}
	else
	{
		instDist = Max(m_boundSphere.GetRadiusf() + ((GROUP_PORTAL_EXTERNAL_INST_DIST * 2) * LODScale), m_boundSphere.GetRadiusf() + CVehiclePopulation::GetCreationDistance());		// all other tunnels
	}

	return(instDist);
}

bool CInteriorProxy::AreStaticCollisionsLoaded()
{
	if (HasStaticBounds())
	{
		return(g_StaticBoundsStore.HasObjectLoaded(m_staticBoundsSlotIndex));
	}

	return(false);
}

// -- Entity Set stuff --

// if, during population, we find an entity set with the given names, then we create those entities & add to interior also
void CInteriorProxy::ActivateEntitySet(atHashString setName)
{
	if (m_activeEntitySets.GetCapacity() == 0)
	{
		m_activeEntitySets.Reserve(MAX_ENTITY_SETS);
	}

	if (FindEntitySetByName(setName) == -1)
	{
		m_activeEntitySets.Push(SEntitySet(setName,MAX_ENTITY_SETS));

		sceneDisplayf("<ENTITY SET ACTIVATE : %s> : Add to proxy IDX %d", setName.TryGetCStr() ? setName.TryGetCStr() : "???", GetPool()->GetJustIndex(this));

// 		if(m_pIntInst && m_pIntInst->IsPopulated())
// 		{
// 			m_pIntInst->ActivateEntitySet(setName);
// 		}
	}
}

void CInteriorProxy::DeactivateEntitySet(atHashString setName)
{
	DeleteEntitySet(setName);

// 	if(bImmediate && m_pIntInst && m_pIntInst->IsPopulated())
// 	{
// 		m_pIntInst->DeactivateEntitySet(setName);
// 	}
}

int CInteriorProxy::FindEntitySetByName(atHashString setName) const
{
	for(int i=0; i<m_activeEntitySets.GetCount(); ++i)
	{
		if(m_activeEntitySets[i].m_name == setName)
		{
			sceneDisplayf("<ENTITY SET FOUND : %s> : Found in proxy IDX %d", setName.TryGetCStr() ? setName.TryGetCStr() : "NULL", GetPool()->GetJustIndex(this));
			return i;
		}
	}
	return -1;
}

void CInteriorProxy::DeleteEntitySet(atHashString setName)
{
	for(int i=0; i<m_activeEntitySets.GetCount(); ++i)
	{
		if(m_activeEntitySets[i].m_name == setName)
		{
			m_activeEntitySets.Delete(i);

			sceneDisplayf("<ENTITY SET DELETE : %s> : Remove from proxy IDX %d", setName.TryGetCStr() ? setName.TryGetCStr() : "???", GetPool()->GetJustIndex(this));
		}
	}
}

//
//
// sets tint index for given entity set:
//
// bool CInteriorProxy::SetEntitySetTintIndex(atHashString setName, u8 tintIdx)
// {
// 	CMloModelInfo* pMloModelInfo = GetInteriorInst()->GetMloModelInfo();
// 	Assert(pMloModelInfo);
// 
// 	atArray<CMloEntitySet>& archetypeEntitySets = pMloModelInfo->GetEntitySetsNC();
// 
// 	bool bFound = false;
// 	for(u32 i=0; i<archetypeEntitySets.GetCount(); i++)
// 	{
// 		if (archetypeEntitySets[i].m_name == setName)
// 		{
// 			bFound = true;
// 			archetypeEntitySets[i].m_tintIdx = tintIdx;
// 			break;
// 		}
// 	}
// 
// 	sceneAssertf(bFound, "[script] Couldn't set tint index for entity set %s.", setName.GetCStr());
// 
// 	return bFound;
// }

// bool CInteriorProxy::SetEntitySetTintIndex(atHashString setName, UInt32 tintIdx)
// { 
// 	bool bFound = false;
// 
// 	int i = m_activeEntitySets.Find(setName); 
// 	if(i != -1) 
// 	{
// 		m_activeEntitySets[i].m_tintIdx = tintIdx; 
// 		bFound = true;
// 	}
// 
// 	sceneAssertf(bFound, "[script] Couldn't set tint index for entity set %s in %s", setName.GetCStr(), GetName().TryGetCStr());
// 
// 	return bFound;
// }


// force an interior to undergo a depopulate and populate cycle (to allow new entity sets to activate)
void CInteriorProxy::RefreshInterior()
{
	CleanupInteriorImmediately();
}

void CInteriorProxy::ValidateActiveSets()
{
#if !__FINAL
	if (GetInteriorInst())
	{
		CMloModelInfo* pMloModelInfo = GetInteriorInst()->GetMloModelInfo();
		Assert(pMloModelInfo);

		const atArray<CMloEntitySet>& archetypeEntitySets = pMloModelInfo->GetEntitySets();
		bool bFound = false;

		for(u32 i=0; i<m_activeEntitySets.GetCount(); i++)
		{
			for(u32 j=0; j<archetypeEntitySets.GetCount(); j++)
			{
				if (archetypeEntitySets[j].m_name == m_activeEntitySets[i].m_name)
				{
					bFound = true;
				}
			}
			sceneAssertf(bFound, "[script] entity set %s does not appear in interior %s", m_activeEntitySets[i].m_name.GetCStr(), pMloModelInfo->GetModelName());
			bFound = false;
		}
	} else {
		Assertf(false,"Can't validate active sets unless interior is loaded");
	}
#endif //!__FINAL
}

void	CInteriorProxy::DisableAllCollisions(bool bDisable)
{
	if (this->GetInteriorInst() && this->GetInteriorInst() == CPortalVisTracker::GetPrimaryInteriorInst() REPLAY_ONLY( && !CReplayMgr::IsReplayInControlOfWorld()))
	{
		Warningf("%s being disabled whilst being used for primary view!", GetModelName());
		Assertf(false, "%s being disabled whilst being used for primary view!", GetModelName());
	}

	if (m_staticBoundsSlotIndex != -1)
		g_StaticBoundsStore.GetBoxStreamer().SetIsIgnored(m_staticBoundsSlotIndex.Get(), bDisable);

	char	instancedBoundsName[80];
	GetWeaponBoundsName(instancedBoundsName, GetPool()->GetJustIndex(this));

	strLocalIndex weaponsBoundSlotIndex = g_StaticBoundsStore.FindSlot(instancedBoundsName);
	if (weaponsBoundSlotIndex != -1)
	{
		g_StaticBoundsStore.GetBoxStreamer().SetIsIgnored(weaponsBoundSlotIndex.Get(), bDisable);
	}

	if (bDisable)
	{
		CleanupInteriorImmediately();

		if (m_staticBoundsSlotIndex != -1 && g_StaticBoundsStore.HasObjectLoaded(m_staticBoundsSlotIndex))
		{
			g_StaticBoundsStore.ClearRequiredFlag(m_staticBoundsSlotIndex.Get(), STRFLAG_DONTDELETE | STRFLAG_INTERIOR_REQUIRED);
			g_StaticBoundsStore.StreamingRemove(m_staticBoundsSlotIndex);
		}

		if (weaponsBoundSlotIndex != -1 && g_StaticBoundsStore.HasObjectLoaded(weaponsBoundSlotIndex))
		{
			g_StaticBoundsStore.ClearRequiredFlag(weaponsBoundSlotIndex.Get(), STRFLAG_DONTDELETE | STRFLAG_INTERIOR_REQUIRED);
			g_StaticBoundsStore.StreamingRemove(weaponsBoundSlotIndex);
		}
	}
}

bool CInteriorProxy::GetIsDisabledForSlot(strLocalIndex mapDataSlotIndex)
{
	if (CInteriorProxy* pProxy = FindProxy(mapDataSlotIndex.Get()))
	{
		return pProxy->GetIsDisabled();
	}

	return false;
}

void CInteriorProxy::SetIsDisabledDLC(strLocalIndex mapDataSlotIndex, bool disable)
{
	if (CInteriorProxy* pProxy = FindProxy(mapDataSlotIndex.Get()))
	{
		pProxy->SetIsDisabledDLC(disable);
	}
}

void CInteriorProxy::SetIsDisabledDLC(bool disable)
{
	strLocalIndex mapDataIndex = GetMapDataSlotIndex();
	strLocalIndex boundsIndex = GetStaticBoundsSlotIndex();

#if __BANK
	if (audNorthAudioEngine::GetOcclusionManager()->GetIsOcclusionBuildEnabled())
	{
		m_disabledByDLC = disable;
	}
#endif

	SetIsDisabled(disable);

	Assertf(mapDataIndex.IsValid(), "CInteriorProxy::SetIsDisabledDLC - Invalid mapDataIndex! Name: %s, Model: %s", 
		GetName().GetCStr(), GetModelName());

	Assertf(boundsIndex.IsValid(), "CInteriorProxy::SetIsDisabledDLC - Invalid boundsIndex! Name: %s, Model: %s", 
		GetName().GetCStr(), GetModelName());

	if (mapDataIndex.IsValid())
		fwMapDataStore::GetStore().GetBoxStreamer().SetIsIgnored(mapDataIndex.Get(), disable);

	if (boundsIndex.IsValid())
		g_StaticBoundsStore.GetBoxStreamer().SetIsIgnored(boundsIndex.Get(), disable);

	if (mapDataIndex.IsValid())
		fwMapDataStore::GetStore().ModifyHierarchyStatus(mapDataIndex, HMT_FLUSH);
}

void	CInteriorProxy::SetIsDisabled(bool bDisable)
{
	//CleanupInteriorImmediately();
	DisableAllCollisions(bDisable);
	m_bIsDisabled = bDisable;
}

void	CInteriorProxy::SetIsCappedAtPartial(bool val)
{
	//CleanupInteriorImmediately();
	DisableAllCollisions(val);
	m_bIsCappedAtPartial = val;
}

void	CInteriorProxy::DisableMetro(bool val)
{
	for(s32 i=0; i<GetPool()->GetSize(); i++)
	{
		CInteriorProxy* pProxy = GetPool()->GetSlot(i);
		if (pProxy && (pProxy->GetGroupId() == 1))
		{
			pProxy->m_bDisabledByCode = val;
		}
	}
}
//////////////////////////////////////////////////////////////////////////
// CACHELOADER related

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CInteriorProxyCacheEntry
// PURPOSE:		ctor caches elements from specified CInteriorProxy
//////////////////////////////////////////////////////////////////////////
CInteriorProxyCacheEntry::CInteriorProxyCacheEntry(CInteriorProxy* pProxy)
{
	Assert(pProxy);

	m_proxyNameHash = pProxy->GetName().GetHash();

	Assert(strlen(pProxy->GetName().GetCStr()) < PROXY_NAME_LENGTH);

	memset(m_proxyName, '\0', sizeof(char) * PROXY_NAME_LENGTH);

#if __BANK
	strncpy(m_proxyName, pProxy->GetName().GetCStr(), PROXY_NAME_LENGTH);			// otherwise interior widgets are fatally broken
#endif //__BANK

	m_proxyName[31] = '\0';

	m_mapDataStoreNameHash = g_MapDataStore.GetHash(strLocalIndex(pProxy->GetMapDataSlotIndex()));

	m_groupId = pProxy->GetGroupId();
	m_floorId = pProxy->GetFloorId();

	Assert(m_floorId < 10);
	Assert(m_groupId < 255);

#if !defined(_MSC_VER) || !defined(_M_X64)
	m_numExitPortals = (u16) pProxy->GetNumExitPortals();
#else
	m_numExitPortals = pProxy->GetNumExitPortals();
#endif

	spdAABB bbox;
	pProxy->GetInitialBoundBox(bbox);

	m_bboxMinX = bbox.GetMinVector3().x;
	m_bboxMinY = bbox.GetMinVector3().y;
	m_bboxMinZ = bbox.GetMinVector3().z;
	m_bboxMaxX = bbox.GetMaxVector3().x;
	m_bboxMaxY = bbox.GetMaxVector3().y;
	m_bboxMaxZ = bbox.GetMaxVector3().z;

	Assertf(ABS(m_bboxMaxX - m_bboxMinX) < 650, "No interiors should be larger than 650m - Check '%s' in %s.imap",m_proxyName, g_MapDataStore.GetName(strLocalIndex(pProxy->GetMapDataSlotIndex())));

	QuatV quat;
	pProxy->GetQuaternion(quat);
	Quaternion proxyQuat = QUATV_TO_QUATERNION(quat);

	m_quatX = proxyQuat.x;
	m_quatY = proxyQuat.y;
	m_quatZ = proxyQuat.z;
	m_quatW = proxyQuat.w;

	Vec3V pos;
	pProxy->GetPosition(pos);
	Vector3 proxyPos = VEC3V_TO_VECTOR3(pos);

	m_posX = proxyPos.GetX();
	m_posY = proxyPos.GetY();
	m_posZ = proxyPos.GetZ();
}

void WriteInteriorProxyCacheEntryDetailsToTextFile(const CInteriorProxyCacheEntry *pCacheEntry, fiStream *pTextFile)
{
	if (pTextFile)
	{
		if (Verifyf(pCacheEntry, "WriteInteriorProxyCacheEntryDetailsToTextFile - pCacheEntry is NULL"))
		{
			char outputString[256];

			formatf(outputString, "m_groupId=%u\n", pCacheEntry->m_groupId);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_floorId=%u\n", pCacheEntry->m_floorId);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_numExitPortals=%u\n", pCacheEntry->m_numExitPortals);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_proxyNameHash=%u\n", pCacheEntry->m_proxyNameHash);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_mapDataStoreNameHash=%u\n", pCacheEntry->m_mapDataStoreNameHash);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_pos=%.2f,%.2f,%.2f\n", pCacheEntry->m_posX, pCacheEntry->m_posY, pCacheEntry->m_posZ);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_quat=%.2f,%.2f,%.2f,%.2f\n", pCacheEntry->m_quatX, pCacheEntry->m_quatY, pCacheEntry->m_quatZ, pCacheEntry->m_quatW);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_bboxMin=%.2f,%.2f,%.2f\n", pCacheEntry->m_bboxMinX, pCacheEntry->m_bboxMinY, pCacheEntry->m_bboxMinZ);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_bboxMax=%.2f,%.2f,%.2f\n", pCacheEntry->m_bboxMaxX, pCacheEntry->m_bboxMaxY, pCacheEntry->m_bboxMaxZ);
			pTextFile->Write(outputString, (s32) strlen(outputString));

			formatf(outputString, "m_proxyName=%s\n\n", pCacheEntry->m_proxyName);
			pTextFile->Write(outputString, (s32) strlen(outputString));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ReadFromCacheFile
// PURPOSE:		Processes a line taken from the cache file (called from within a loop for this module)
//////////////////////////////////////////////////////////////////////////
bool CInteriorProxy::ReadFromCacheFile(const void* const pEntry, fiStream* BANK_ONLY(pDebugTextFileToWriteTo))
{
	Vector3 vMin;
	Vector3 vMax;

	CInteriorProxyCacheEntry cacheEntry(pEntry);

#if __BANK
	if (pDebugTextFileToWriteTo)
	{
		WriteInteriorProxyCacheEntryDetailsToTextFile(&cacheEntry, pDebugTextFileToWriteTo);
	}
#endif // __BANK

	strLocalIndex mapDataStoreIndex = strLocalIndex(g_MapDataStore.FindSlotFromHashKey(cacheEntry.m_mapDataStoreNameHash));

	if (mapDataStoreIndex != -1)
	{
		strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(g_MapDataStore.GetStreamingIndex(mapDataStoreIndex));
		strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(info->GetHandle());
		bool readEntry = !pFile->m_bNew;

		if (strCacheLoader::GetMode() == SCL_DLC)
		{
			atMap<s32, bool> dlcArchives = strCacheLoader::GetDLCArchives();
			int archiveIndex = strPackfileManager::GetImageFileIndexFromHandle(info->GetHandle()).Get();

			readEntry = dlcArchives.Access(archiveIndex) != NULL;
		}

		if ( Verifyf(pFile, "NULL packfile found for IMAP %s (slot %d) handle %d", g_MapDataStore.GetName(mapDataStoreIndex), mapDataStoreIndex.Get(), info->GetHandle() )
			&& readEntry)
		{
			CMloInstanceDef temp;
#if __BANK
			temp.m_archetypeName.SetFromString(cacheEntry.m_proxyName);			// some widgets need this
#endif //__BANK

			temp.m_archetypeName = cacheEntry.m_proxyNameHash;

			temp.m_position = Vector3(cacheEntry.m_posX, cacheEntry.m_posY, cacheEntry.m_posZ);
			temp.m_rotation = Vector4(cacheEntry.m_quatX, cacheEntry.m_quatY,cacheEntry. m_quatZ, cacheEntry.m_quatW);

			Assert(cacheEntry.m_floorId < 10);
			temp.m_floorId = cacheEntry.m_floorId;

			Assert(cacheEntry.m_groupId < 255);
			temp.m_groupId = cacheEntry.m_groupId;

#if !defined(_MSC_VER) || !defined(_M_X64)
			temp.m_numExitPortals = (u16) cacheEntry.m_numExitPortals;
#else
			temp.m_numExitPortals = cacheEntry.m_numExitPortals;
#endif
			Assertf(ABS(cacheEntry.m_bboxMaxX - cacheEntry.m_bboxMinX) < 650, "No interiors should be larger than 650m - Check '%s' in %s.imap",temp.m_archetypeName.GetCStr(), g_MapDataStore.GetName(mapDataStoreIndex));

			spdAABB bbox(
				Vec3V(cacheEntry.m_bboxMinX, cacheEntry.m_bboxMinY, cacheEntry.m_bboxMinZ),
				Vec3V(cacheEntry.m_bboxMaxX, cacheEntry.m_bboxMaxY, cacheEntry.m_bboxMaxZ)
				);

			CInteriorProxy::Add(&temp, mapDataStoreIndex, bbox);
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddToCacheFile
// PURPOSE:		adds data based on the passed index into the cache file
//////////////////////////////////////////////////////////////////////////
void CInteriorProxy::AddToCacheFile(fiStream* pDebugTextFileToWriteTo)
{
	bool addEntry = false;

	for(s32 i=0; i<GetPool()->GetSize(); i++)
	{
		CInteriorProxy* pProxy = GetPool()->GetSlot(i);

		addEntry = false;

		if (pProxy)
		{
			if (strCacheLoader::GetMode() == SCL_DLC)
			{
				atMap<s32, bool> dlcArchives = strCacheLoader::GetDLCArchives();
				strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(INSTANCE_STORE.GetStreamingIndex(pProxy->GetMapDataSlotIndex()));
				int index = strPackfileManager::GetImageFileIndexFromHandle(info->GetHandle()).Get();

				addEntry = dlcArchives.Access(index) != NULL;
			}
			else
			{
				addEntry = true;
			}

			if (addEntry)
			{
				spdAABB bbox;
				pProxy->GetInitialBoundBox(bbox);
				if (!bbox.IsValid())
				{
					Quitf(0, "Attempting to write invalid bounds data for valid slot! It isn't safe to continue, please discard any cache data that has been generated, redeploy, and try again. If the issue persists add a bug for *Code (Engine)*");
				}
				CInteriorProxyCacheEntry entry(pProxy);
				if (pDebugTextFileToWriteTo)
				{
					WriteInteriorProxyCacheEntryDetailsToTextFile(&entry, pDebugTextFileToWriteTo);
				}
				strCacheLoader::WriteDataToBuffer(&entry, sizeof(entry));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitCacheLoaderModule
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CInteriorProxy::InitCacheLoaderModule()
{
	strCacheLoader::GetModules().AddModule(
		"CInteriorProxy",
		&CInteriorProxy::ReadFromCacheFile,	// called when we read from the cache
		&CInteriorProxy::AddToCacheFile,		// called when we write to the cache
		sizeof(CInteriorProxyCacheEntry));
}


// get checksum of all interior proxy names
u32 CInteriorProxy::GetChecksum()
{
	const CInteriorProxy* currProxy = NULL;
	atArray<u32> nameHashes;
	u32 numProxies = CInteriorProxy::GetPool()->GetSize();

	nameHashes.Reset();
	nameHashes.Resize(numProxies);

	gnetDebug3("CInteriorProxy::GetChecksum :: Number of Proxies = 0x%08x", numProxies);

	for(u32 i=0; i<numProxies; i++)
	{
		currProxy = CInteriorProxy::GetPool()->GetSlot(i);

		if (currProxy && currProxy->GetChangeSetSource() == 0)
		{
			nameHashes[i] = currProxy->GetNameHash();

	#if !__FINAL
			const char* name = currProxy->GetName().TryGetCStr();
			if(!name)
			{
				name = "<Unknown>";
			}
			gnetDebug3("GetChecksum :: Adding interior proxy %d with checksum 0x%08x (%s)", i, nameHashes[i], name);
	#endif
		}
		else
		{
			nameHashes[i] = 0;
			gnetDebug3("GetChecksum :: Interior proxy %d has 0 hash (%s)", i, currProxy ? currProxy->GetName().TryGetCStr() : "NULL");
		}
	}

	const u32* pData = nameHashes.GetElements();
	u32 checkSum = fwKeyGen::GetKey(reinterpret_cast<const char*>(pData), (numProxies*4));

	return checkSum;
}

u32	CInteriorProxy::GetCheckSumCount()
{
	const CInteriorProxy* currProxy = NULL;
	u32 numProxies = CInteriorProxy::GetPool()->GetSize();
	u32 count = 0;

	for(u32 i=0; i<numProxies; i++)
	{
		currProxy = CInteriorProxy::GetPool()->GetSlot(i);

		if (currProxy && currProxy->GetChangeSetSource() == 0)
			count++;
	}

	return count;
}

// count interior proxies
u32		CInteriorProxy::GetCount(void)
{
	u32 numProxies = CInteriorProxy::GetPool()->GetSize();
	u32 count = 0;

	for(u32 i=0; i<numProxies; i++)
	{
		if (CInteriorProxy::GetPool()->GetSlot(i))
			count++;
	}

	return(count);
}

#if !__NO_OUTPUT
const char* CInteriorProxy::GetContainerName() const
{
	if (m_mapDataSlotIndex == -1)
	{
		return "UNKNOWN";
	}

	return(fwMapDataStore::GetStore().GetName(strLocalIndex(m_mapDataSlotIndex)));
}

#endif //!__NO_OUTPUT

typedef struct _InteriorPosOrientInfo
{
	atString	name;
	atString	archetypeName;
	Vector3		position;
	float		heading;
	u32			mapsDataSlotIDX;
	PAR_SIMPLE_PARSABLE;
}	InteriorPosOrientInfo;

class InteriorPosOrientContainer
{
public:
	atArray < InteriorPosOrientInfo > info;
	PAR_SIMPLE_PARSABLE;
};

#include "InteriorProxy_parser.h"

#if __BANK

#if !__FINAL
void CInteriorProxy::EnableAndUncapAllInteriors()
{
	int poolSize = GetPool()->GetSize();
	for(int i = 0; i < poolSize; i++)
	{
		CInteriorProxy *pIntProxy = GetPool()->GetSlot(i);
		if (pIntProxy)
		{
			if(pIntProxy->GetIsDisabled())
			{
				pIntProxy->SetIsDisabled(false);
			}
			if(pIntProxy->GetIsCappedAtPartial())
			{
				pIntProxy->SetIsCappedAtPartial(false);
			}
		}
	}
}
#endif

bool bDisableMetroSystem = false;
void DisableMetroSystemCB(void)
{
	CInteriorProxy::DisableMetro(bDisableMetroSystem);
}

void CInteriorProxy::AddWidgets(bkBank& bank)
{
	bank.AddButton("Dump Interior Positions & Orientation", DumpPositionsAndOrientation);
	bank.AddButton("Dump Interior Proxy Info", DumpInteriorProxyInfo);
	bank.AddToggle("Disable metro system", &bDisableMetroSystem, DisableMetroSystemCB);
}

void CInteriorProxy::DumpPositionsAndOrientation()
{
InteriorPosOrientContainer	infoContainer;

	s32 poolSize=CInteriorProxy::GetPool()->GetSize();
	for(int i=0;i<poolSize;i++)
	{
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(pIntProxy)
		{
			// Get orientation matrix
			Matrix34 matrix = MAT34V_TO_MATRIX34(pIntProxy->GetMatrix());

			// Output Position & Orientation to a text file (XML)
			Vector3	position = matrix.GetVector(3);

			// Get the heading in radians
			// We assume the rotation is only about one axis
			Vector3 ahead(0,1,0);
			Vector3 direction;
			matrix.Transform3x3(ahead, direction );
			float heading = -rage::Atan2f(direction.x, direction.y);

			// Get the name of the interior via mapdatastore
			fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(pIntProxy->m_mapDataSlotIndex));


			InteriorPosOrientInfo info;
			info.mapsDataSlotIDX = pIntProxy->m_mapDataSlotIndex.Get();
			info.name = atString(pDef->m_name.GetCStr());
			info.archetypeName = atString( pIntProxy->GetModelName() );
			info.heading = RtoD * heading;
			info.position = position;
			infoContainer.info.PushAndGrow(info);

			/*
			Displayf("//////////////////////////////////////////////" );
			Displayf("Pool SlotID = %d", i );
			Displayf("MapDataSlotIDX = %d", pIntProxy->m_mapDataSlotIndex );
			Displayf("Slot Name = %s", pDef->m_name.GetCStr());
			Displayf("Position = %f,%f,%f", position.x, position.y, position.z );
			Displayf("Heading = %f (degrees)", RtoD * heading );
			Displayf("//////////////////////////////////////////////" );
			*/
		}
	}

	PARSER.SaveObject("X:/gta5/build/dev/common/data/script/xml/InteriorLocationInfo/InteriorsInfo", "XML", &infoContainer, parManager::XML);

}
#endif	//__BANK

#if !__FINAL
void CInteriorProxy::DumpInteriorProxyInfo()
{
	s32 poolSize=CInteriorProxy::GetPool()->GetSize();

	for(int i=0;i<poolSize;i++)
	{
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(pIntProxy)
		{
			const char* name = pIntProxy->GetName().TryGetCStr();
			if(!name)
			{
				name = "Unknown";
			}
			Displayf("Proxy %d: <%s> hash: 0x%x", i, name, pIntProxy->GetNameHash() );
		}
	}
}
#endif // !__FINAL
