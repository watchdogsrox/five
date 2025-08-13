// ======================================
// game/vfx/misc/MovieMeshManager.h
// (c) 2011 RockstarNorth
// ======================================

#ifndef MOVIE_MESH_MANAGER_H
#define MOVIE_MESH_MANAGER_H

// rage
#include "atl/array.h"
#include "atl/string.h"
#include "atl/bitset.h"
#include "grcore/effect.h"
#include "vectormath/classes.h"

// framework
#include "fwscene/world/InteriorLocation.h"

// game
#include "MovieMeshInfo.h"
#include "scene/RegdRefTypes.h"
#include "script/script_memory_tracking.h"
#include "control/replay/ReplaySettings.h"

// defines
#define INVALID_MOVIE_MESH_HANDLE		(0)
#define INVALID_MOVIE_MESH_SET_HANDLE	(0)
#define INVALID_MOVIE_MESH_SCRIPT_ID	(0)
#define MOVIE_MESH_MAX_HANDLES			(8)
#define MOVIE_MESH_MAX_SETS				(16)

// forward declarations
namespace rage {
class grcRenderTarget;
};
class CObject;
class CInteriorInst;
typedef u32 MovieMeshSetHandle;

enum eMovieMeshSetState
{
	MMS_FAILED = -1,
	MMS_PENDING_LOAD,
	MMS_LOADED,
	MMS_READY,
	MMS_PENDING_DELETE
};

// internal structure to CMovieMeshManager
// provides reference counting for render target/bink movie pairs
// these are paired because any given movie can only be associated to one
// named render target
struct MovieMeshHandle 
{
	u32		rtHandle;
	u32		rtNameHash;
	u32		movieHandle;
	u16		refCount;
	bool	bInUse;

	MovieMeshHandle() : rtHandle(0), movieHandle(0), refCount(0), bInUse(false) {};

	void IncRef() { refCount++; };
	void Release();
	void Reset() { rtHandle = 0; movieHandle = 0; refCount = 0; bInUse = false; };
};

// internal structure to CMovieMeshManager
// xml data is unserialised onto this structure
// it is also a container for all the resources used by
// a given set
struct MovieMeshSet
{
	struct MeshObject
	{
		MeshObject() : m_pObj(NULL), m_bIsReady(false) {};
		RegdEnt		m_pObj;
		bool		m_bIsReady;
	};

	MovieMeshSet() : m_handle(INVALID_MOVIE_MESH_SET_HANDLE), m_state(MMS_FAILED), m_scriptId(INVALID_MOVIE_MESH_SCRIPT_ID) {};
	~MovieMeshSet() { Reset(); }

	void Reset() { m_handle = INVALID_MOVIE_MESH_SET_HANDLE; m_state = MMS_FAILED; m_scriptId = INVALID_MOVIE_MESH_SCRIPT_ID; }

	MovieMeshSetHandle					m_handle;				// handle for the whole set
	MovieMeshInfoList					m_info;					// unserialised info used to load and create runtime resources
	atArray<MeshObject>					m_pMeshes;				// array of models (expected N)
	atArray<MovieMeshHandle*>			m_pHandles;				// handles to bink movies held by this set (expected N)
	eMovieMeshSetState					m_state;				// current state of the set
	int									m_scriptId;
	atString							m_loadRequestName;		// tbr
};

// internal structure to CMovieMeshManager
// helper structure to do housekeeping of rt/bink pairs
// it does not perform any loading/allocation logic
// Update will release resources when there are no references
// to a given handle, models are handled separately by CMovieMeshManager
// as these can share N rt/bink pairs
struct MovieMeshGlobalPool 
{
	MovieMeshGlobalPool();

	bool GetFreeHandle(MovieMeshHandle*& handle);
	bool FindHandle(MovieMeshHandle*& handle, u32 movieHandle);
	bool HandleExists(const MovieMeshHandle* const pHandle) const;

	bool AddHandle(MovieMeshHandle*& pHandle, u32 movieHandle);
	void ModifyHandle(MovieMeshHandle* const pHandle, u32 movieHandle, u32 rtHandle, u32 rtNameHash);

	void Update();

	MovieMeshHandle	m_uniqueHandles[MOVIE_MESH_MAX_HANDLES];	// array of unique movie handles (expected N or less)
};

struct MovieMeshRenderTarget
{
	enum eState 
	{
		kInvalid = 0,
		kLoading,
		kReady,
		kWaitingForRelease,
	};

	MovieMeshRenderTarget() { Reset(); }
	~MovieMeshRenderTarget() { Reset(); }

	void Reset() { m_bTxdRefAdded=false; m_name.Null(); m_txdIdx=-1; m_pTexture=NULL; m_pRenderTarget=NULL; m_state=kInvalid; }

	atFinalHashString	m_name;
	grcTextureHandle	m_pTexture;
	grcRenderTarget*	m_pRenderTarget;
	s32					m_txdIdx;
	u32					m_numRefs;
	eState				m_state;
	bool				m_bTxdRefAdded;

};

struct MovieMeshRenderTargetBufferedData
{
	rage::grcRenderTarget*	pRt;
	rage::grcTextureHandle  tex;
	u32						id;
};

class CMovieMeshRenderTargetManager
{
public:

	void Init();
	void Shutdown();
	void Update();

	u32 Register(atFinalHashString name);	
	void Release(atFinalHashString name);
	void Release(u32 rtHash);
	bool Exists(atFinalHashString name) const;

	bool IsReady(u32 rtHash) const;

	void BufferDataForDrawList(MovieMeshRenderTargetBufferedData* pOut);

	u32 GetCount() const { return (u32)m_renderTargets.GetCount(); }

private:

	MovieMeshRenderTarget* Get(atFinalHashString name);
	const MovieMeshRenderTarget* Get(atFinalHashString name) const;
	MovieMeshRenderTarget* Get(u32 rtHash);

	bool ProcessWaitingOnReleaseState(MovieMeshRenderTarget* pTarget);
	void ProcessLoadingState(MovieMeshRenderTarget* pTarget);
	void ProcessReadyState(MovieMeshRenderTarget* pTarget);

	void ReleaseOnFailure(MovieMeshRenderTarget* pTarget);
	grcRenderTarget* CreateRenderTarget(MovieMeshRenderTarget* pTarget, grcTexture* pTexture); 

	static const int MAX_MOVIE_MESH_RT = 2;

	atFixedArray<MovieMeshRenderTarget, MAX_MOVIE_MESH_RT> m_renderTargets;

	// Render targets need to be kept around long enough for the render thread
	// to have finished processing them.  Do not need to wait for GPU completion
	// though (we are only freeing up the render target structure, not the
	// actual memory that is renderred into).  So waiting until the second
	// update call is sufficient.
	grcRenderTarget *m_delayedReleaseRts[2][MAX_MOVIE_MESH_RT];
	unsigned m_numDelayedReleaseRts[2];
	unsigned m_delayedReleaseRtsFrame;
};

// CMovieMeshManager offers functionality to load, setup and manage 
// a set of meshes with Bink movies attached as their diffuse texture.
// Data comes from a MovieMeshInfoList, which is unserialised from an XML file.
// It must be used through the g_movieMeshMgr global instance.
class CMovieMeshManager
{

public:


	CMovieMeshManager();
	~CMovieMeshManager();

	// main public API
	MovieMeshSetHandle	LoadSet(const char* pFilename);
	void 				ReleaseSet(MovieMeshSetHandle handle);

	eMovieMeshSetState	QuerySetStatus(MovieMeshSetHandle handle) const;
	bool 				IsHandleValid(MovieMeshSetHandle handle) const { return (handle != INVALID_MOVIE_MESH_SET_HANDLE); }
	bool 				HandleExists(MovieMeshSetHandle handle) const;

	CMovieMeshRenderTargetManager& GetRTManager() { return m_renderTargetMgr; }

	// script exclusive interface
	int					GetScriptIdFromHandle(MovieMeshSetHandle handle) const;
	MovieMeshSetHandle	GetHandleFromScriptId(int idx) const;

	// common interface
	static void Init (unsigned initMode);
	static void Shutdown (unsigned shutdownMode);

	void Update();

#if GTA_REPLAY
	MovieMeshSet &GetSet(int i)
	{
		return m_setPool[i];
	}
#endif	//GTA_REPLAY

	// interface for debug tool
#if __BANK
	static void InitWidgets();

	int			GetNumMeshes(MovieMeshSetHandle handle) const;
	const char* GetSetName(MovieMeshSetHandle handle) const;
	void		SetMeshIsVisible(MovieMeshSetHandle handle, int i, bool isVisible);
	bool		GetMeshIsVisible(MovieMeshSetHandle handle, int i) const;
	void		SetIgnoreAnimDataOnLoad(bool bIgnore) { m_bDebugIgnoreAnimData = bIgnore; };

	void		RenderDebug3D();

#endif // __BANK

#if __SCRIPT_MEM_CALC
	void		CountMemoryUsageForMovies(MovieMeshSetHandle handle);
	void		CountMemoryUsageForRenderTargets(MovieMeshSetHandle handle);
	void		CountMemoryUsageForObjects(MovieMeshSetHandle handle);
#endif	//	__SCRIPT_MEM_CALC

private:

#if __BANK
	bool		m_bDebugIgnoreAnimData;
#endif // __BANK

private:

	void InitInternal		(unsigned initMode);
	void ShutdownInternal	(unsigned shutdownMode);

	// set requests
	MovieMeshSet*		GetFreeSet(int& scriptIdx);
	MovieMeshSet*		FindSet(MovieMeshSetHandle handle);
	const MovieMeshSet* FindSet(MovieMeshSetHandle handle) const;
	MovieMeshSetHandle	GetTemporaryHandle(const char* pFilename) const;
	void				SetupBoundingSpheres(MovieMeshSet* pCurSet);

	// deferred processing
	void ProcessLoadRequests();
	void ProcessLoadRequest(MovieMeshSet* pCurSet);
	void ProcessDeleteRequests();
	void UpdateSets();
	void UpdateAnimDataForSet(MovieMeshSet* pCurSet);
	void UpdateSet(MovieMeshSet* pCurSet);
	
	void Render();

	// loading functions
	bool LoadMovieMeshInfoData(MovieMeshSet* pCurSet);
	bool LoadMovieMeshObjects(MovieMeshSet* pCurSet);
	bool LoadMovies(MovieMeshSet* pCurSet);
	bool LoadNamedRenderTargets(MovieMeshSet* pSet);

	void AddRenderTargetHandle(MovieMeshHandle* pHandle, u32 rtId, u32 rtNameHash);
	bool AddMovieHandle(MovieMeshSet* pCurSet, u32 movieHandle);


	// release functions
	void ReleaseSet(MovieMeshSet* pCurSet);
	void ReleaseObjectsFromSet(MovieMeshSet* pSet);
	void ReleaseMoviesFromSet(MovieMeshSet* pSet);

	// helper functions
	static CObject*			CreateMovieMeshObject(u32 modelIndex, Mat34V_In vTransform, fwInteriorLocation intLoc);
	static CInteriorInst*	GetInteriorAtCoordsOfType(const Vector3& scrVecInCoors, const char* typeName);
	static bool				IsInteriorReady(CInteriorInst* pInteriorInst);
	static void				ExtractRenderTargetName(const char* pInputName, const char*& pOutputName);
	
	void Reset(bool bReleaseResources=false);

	CMovieMeshRenderTargetManager	m_renderTargetMgr;

	MovieMeshSet					m_setPool[MOVIE_MESH_MAX_SETS];
	MovieMeshGlobalPool				m_globalPool;
	bool							m_loadRequestPending;
	bool							m_releaseRequestPending;

	static u32						ms_globalRtHandle;
};

#if __BANK
namespace rage
{
	class bkText;
	class bkCombo;
	class bkButton;
	class bkGroup;
	class bkToggle;
};

class CMovieMeshDebugTool 
{
public:

	static void InitWidgets();
	static void CreateWidgetsOnDemand();

	static void LoadSet();
	static void ReleaseSet();

	static void Update();
	static void Render(const MovieMeshSet* pCurSet);

	static void	OnComboSelectionChange();
	static void OnIgnoreAnimDataToggleChange();

private:
	
	static void RefreshSetCombo();
	static bool IsInputStringValid();

	static bkText*						ms_pLoadRequestBox;
	static bkButton*					ms_pLoadSetButton;
	static bkButton*					ms_pReleaseSetButton;
	static bkCombo*						ms_pLoadedSetsCombo;
	static bkButton*					ms_pCreateButton;
	static bkGroup*						ms_pBankGroup;
	static char							ms_strSetName[128];
	static bool							ms_bActiveSetInitialised;
	static int							ms_comboSelIndex;
	static const char*					ms_comboEmptyMessage;
	static MovieMeshSetHandle			ms_setHandles[MOVIE_MESH_MAX_SETS];
	static const char*					ms_comboSetNames[MOVIE_MESH_MAX_SETS]; 
	static bool							ms_bIgnoreAnimData;
	static bool							ms_bShowFireBoundingSpheres;
};

#endif //__BANK


// global, single instance of CMovieMeshManager
extern CMovieMeshManager	g_movieMeshMgr;

#endif //MOVIE_MESH_MANAGER_H
