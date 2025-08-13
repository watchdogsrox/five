// Title	:	PostScan.h
// Author	:	John Whyte
// Started	:	4/8/2009

#ifndef _POSTSCAN_H_
#define _POSTSCAN_H_

// Framework headers
#include "fwutil/PtrList.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/VisibilityFlags.h"
#include "fwscene/scan/ScanResults.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanDebug.h"

// Rage headers
#include "vector/vector3.h"
#include "vectormath/vec3v.h"

// Game headers
#if !__SPU
#include "renderer/Renderer.h"
#endif

// classes
class CEntity;
class CRenderPhase;

extern u32	g_SortPVSDependencyRunning;

class CGtaPvsEntry : public fwPvsEntry
{
public:
	inline CEntity* GetEntity() const { return reinterpret_cast<CEntity*> (m_pEntity); }
};

// defines
// size of store for all entities scanned out of world by the scan tasks
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
#define MAX_PVS_SIZE		(32000)
#define MAX_RESULT_BLOCKS	(2* MAX_SCAN_ENTITIES_DEPENDENCY_COUNT)
#else
#define MAX_PVS_SIZE		(7000)
#define MAX_RESULT_BLOCKS	(MAX_SCAN_ENTITIES_DEPENDENCY_COUNT)
#endif

class CPostScan
{
public:
	CPostScan();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned initMode);

	void Reset(void);

	void BuildRenderListsNew(void);

	inline CGtaPvsEntry* GetPVSBase()						{ return &m_PVS[0]; }
	// This returns m_pCurrPVSEntry, which is one past the last entry. Also not that the 
	// PVS sort can now update m_pCurrPVSEntry, as it sorts invalidated entries to the back 
	// of the PVS and updates m_pCurrPVSEntry so they aren't included anymore. If you use 
	// GetPVSEnd(), please make sure that you have waited for the sort to finish first.
	inline CGtaPvsEntry* GetPVSEnd()						{ Assert(g_SortPVSDependencyRunning == 0); return m_pCurrPVSEntry; }
	inline CGtaPvsEntry* volatile* GetPVSCurrentPointer()	{ return &m_pCurrPVSEntry; }
	unsigned int volatile* GetNextResultBlockPointer()		{ return &m_NextResultBlock; }
	unsigned int volatile* GetResultBlockCountPointer()		{ return &m_ResultBlockCount; }
	fwScanResultBlock * GetResultBlocksPointer()			{ return m_ResultBlocks; }

	const fwScanResults& GetScanResults() const { return fwScan::GetScanResults(); }

	void ProcessPvsWhileScanning();
	void BuildRenderListsFromPVS();

	void AddToAlwaysPreRenderList(CEntity* pEntity);
	void RemoveFromAlwaysPreRenderList(CEntity* pEntity);
	void AddToNonVisiblePreRenderList(CEntity* pEntity);

	static void StartAsyncSearchForAdditionalPreRenderEntities();
	static void EndAsyncSearchForAdditionalPreRenderEntities();

	static void WaitForSortPVSDependency();
	static void WaitForPostScanHelper();
	
	static bool AddAlphaOverride(CEntity *pEntity, int alpha BANK_PARAM(const atString& ownerString) );
	static int GetAlphaOverride(CEntity *pEntity);
	static bool GetAlphaOverride(const CEntity *pEntity, int& alpha);
	static void RemoveAlphaOverride(CEntity *pEntity);

	static bool IsOnAlphaOverrideList(CEntity* pEntity);

#if __BANK
	static const char* GetAlphaOverrideOwner(CEntity* pEntity);
#endif	

	static void AddEntitySortBias(CEntity *pEntity, float bias);
	static void RemoveEntitySortBias(CEntity *pEntity);

	static void PossiblyAddToLateLightList(CEntity* pEntity);

#if !__FINAL
	u32		GetEntityCount() { return m_EntityCount; }
#endif	//!__FINAL

private:
	void ProcessPVSForRenderPhase();
	void FinishPreRender();
	void ProcessPostVisibility();
	bool RejectTimedEntity(CEntity* RESTRICT pEntity, const bool visibleInGbuf);
	void ProcessResultBlock(const int blockIndex, const u32 viewportBits, const u32 lastVisibleFrame);
	void CleanAndSortPreRenderLists();
	void PreRenderNonVisibleEntities();
	void StartSortPVSJob();

	// vars
	// storage
	ALIGNAS(16) CGtaPvsEntry		m_PVS[MAX_PVS_SIZE] ;	// scanned entities out of the world
	CGtaPvsEntry*		m_pCurrPVSEntry;					// curr ptr into the scanned entities array

	unsigned int		m_ResultBlockCount;					// Number of valid result blocks
	ALIGNAS(8) fwScanResultBlock	m_ResultBlocks[MAX_RESULT_BLOCKS] ;	// This is where the results will come in
	unsigned int		m_NextResultBlock;					// Used by producers to decide where to write the next block to

	u32 m_cameraInInterior;

	fwPtrListSingleLink m_alwaysPreRenderList;
	fwEntityPreRenderList m_nonVisiblePreRenderList;

	atArray< std::pair< CEntity*, fwVisibilityFlags > >	m_lights;

	static bool ms_bAsyncSearchStarted;


	static bool ms_bAllowLateLightEntities;
	static spdSphere  ms_LateLightsNearCameraSphere;
	static atArray< CEntity*>	ms_lateLights;

	
#if __BANK
	friend class CPostScanDebug;
#endif	//__BANK

#if !__FINAL
	u32	m_EntityCount;
#endif	//!__FINAL
};

class CPostScanExteriorHdCull
{
public:
	void Init() { m_bCullSphereEnabled=false; m_cullList.Reset(); m_shadowCullList.Reset(); }
	void Process(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop);

	bool IsModelCulledThisFrame(u32 modelHash) const { return m_cullList.Find(modelHash) != -1; }
	bool IsModelShadowCulledThisFrame(u32 modelHash) const { return m_shadowCullList.Find(modelHash) != -1; }
	void EnableSphereThisFrame(const spdSphere& sphere) { m_bCullSphereEnabled=true; m_cullSphere=sphere; }
	void EnableModelCullThisFrame(const u32 modelHash)
	{
		if ( Verifyf(m_cullList.GetAvailable()>0, "Too many models culled this frame - limit is %d", m_cullList.GetMaxCount()) )
		{
			m_cullList.Append() = modelHash;
		}
	}

	void EnableModelShadowCullThisFrame(const u32 modelHash)
	{
		if ( Verifyf(m_shadowCullList.GetAvailable()>0, "Too many models shadow culled this frame - limit is %d", m_shadowCullList.GetMaxCount()) )
		{
			m_shadowCullList.Append() = modelHash;
		}
	}

private:
	bool IsActive() { return ( m_bCullSphereEnabled || m_cullList.GetCount()>0 || m_shadowCullList.GetCount()>0 ); }

	bool ShouldCullEntity(CEntity* pEntity);
	bool ShouldShadowCullEntity(CEntity* pEntity);

	bool m_bCullSphereEnabled;
	spdSphere m_cullSphere;
	atFixedArray<u32, 32> m_cullList;
	atFixedArray<u32, 32> m_shadowCullList;
};

extern CPostScan	gPostScan;
extern CPostScanExteriorHdCull g_PostScanCull;

#endif // _POSTSCAN_H_


