#ifndef __LODLIGHTMANAGER_H__
#define __LODLIGHTMANAGER_H__

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

#if !__SPU
#include "parser/manager.h"
#include "parser/macros.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "atl/singleton.h"
#include "fwscene/stores/mapdatastore.h"
#include "string/string.h"
#include "system/bit.h"

#include "bank/bank.h"
#include "bank/button.h"
#include "bank/bkmgr.h"
#include "scene/loader/MapData_Misc.h"
#include "scene/Entity.h"
#include "game/Clock.h"
#include "renderer/lights/lights.h"
#include "camera/viewports/Viewport.h"
#endif

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

class CLightEntity;

// ----------------------------------------------------------------------------------------------- //

// The maximum there can possibly be, to prevent B*405958 constantly returning.
#define MAX_NUM_LOD_LIGHT_BUCKETS (350) 

// ----------------------------------------------------------------------------------------------- //

// Used in the toolside light extractor, don't comment out!
#define LODLIGHT_NUM_SECTORS_X	(50)
#define LODLIGHT_NUM_SECTORS_Y	(50)

// ----------------------------------------------------------------------------------------------- //

#define	MAX_LODLIGHT_INTENSITY	(48.0f)

// ----------------------------------------------------------------------------------------------- //

#define	MAX_LODLIGHT_CONE_ANGLE	(180.0f)

// ----------------------------------------------------------------------------------------------- //

#define	MAX_LODLIGHT_CORONA_INTENSITY	(32.0f)

// ----------------------------------------------------------------------------------------------- //

#define	MAX_LODLIGHT_CAPSULE_EXTENT		(140.0f)

// ----------------------------------------------------------------------------------------------- //
#define	LODLIGHT_TURN_OFF_TIME (7)		// 7 am
#define	LODLIGHT_TURN_ON_TIME (19)		// 7 pm

// ----------------------------------------------------------------------------------------------- //

// NOTE: Assumes packing floats
#define LL_PACK_U8(val, range)		( (u8)(val *(255.0f/range)) )
#define LL_UNPACK_U8(val, range)	( (float)(val *(range/255.0f)) )

// =============================================================================================== //
// STRUCTS
// =============================================================================================== //

enum eLodLightCategory
{
	LODLIGHT_CATEGORY_SMALL = 0,
	LODLIGHT_CATEGORY_MEDIUM,
	LODLIGHT_CATEGORY_LARGE,
	LODLIGHT_CATEGORY_COUNT
};

enum eLodLightType
{
	LODLIGHT_STANDARD = 0,
	LODLIGHT_DISTANT,
	LODLIGHT_COUNT
};

enum eLodLightImapFilter
{
	LODLIGHT_IMAP_OWNED_BY_MAP_DATA = BIT(0),
	LODLIGHT_IMAP_OWNED_BY_SCRIPT = BIT(1),
	LODLIGHT_IMAP_ANY = LODLIGHT_IMAP_OWNED_BY_MAP_DATA | LODLIGHT_IMAP_OWNED_BY_SCRIPT,
};

// ----------------------------------------------------------------------------------------------- //

typedef	struct _DISTANTLODLIGHTBUCKET
{
	CDistantLODLight *pLights;
	u16	mapDataSlotIndex;
	u16	lightCount;

}	CDistantLODLightBucket;

// ----------------------------------------------------------------------------------------------- //

typedef	struct _LODLIGHTBUCKET
{
	CLODLight *pLights;
	CDistantLODLight *pDistLights;
	u16	lightCount;
	u16	mapDataSlotIndex;
	u16 parentMapDataSlotIndex;
	u16 pad;
}	CLODLightBucket;

// ----------------------------------------------------------------------------------------------- //

typedef struct  
{
	u32 DECLARE_BITFIELD_5( 
		timeFlags, 24,
		bIsStreetLight, 1,
		bIsCoronaOnly, 1,
		lightType, 5,
		bDontUseInCutscene, 1);
} packedTimeAndStateFlags;

// ----------------------------------------------------------------------------------------------- //

typedef struct
{
	atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> m_LODLightsToRender;
	atFixedArray<CDistantLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> m_DistLODLightsToRender[LODLIGHT_CATEGORY_COUNT];
	bool m_disableThisFrame;

} LODLightData;

// =============================================================================================== //
// CLASS
// =============================================================================================== //

#if !__SPU
class CLODLightManager
{
public:

	CLODLightManager();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void	Update();

	// LOD Lights
	static void	AddLODLight(CLODLight &LODLights, s32 mapDataSlotIndex, s32 parentMapDataSlotIndex );
	static void	RemoveLODLight(s32 mapDataSlotIndex);
	static CDistantLODLightBucket *FindDistantLODLightBucket(s32 mapDataSlotIndex);

	// Distant LOD Lights
	static void	AddDistantLODLight(CDistantLODLight &distLODLights, s32 mapDataSlotIndex);
	static void	RemoveDistantLODLight(s32 mapDataSlotIndex);

	// Hash
	static u32	GenerateLODLightHash(const CLightEntity *pLightEntity );			// Calls down to the method below
	static u32	GenerateLODLightHash(const CEntity *pSourceEntity, s32 lightID );

	// Accessors
#if __BANK
	inline static u32 GetNumLodLightBuckets() { return m_LODLightBuckets.GetNumUsed(); }
	__forceinline static CLODLightBucket* GetLodLightBucket(const u32 index) 
	{ 
		CLODLightBucketNode* pNode = m_LODLightBuckets.GetFirst()->GetNext();
		for(int i=0; i < index; i++)
		{
			pNode = pNode->GetNext();
		}
		return &(pNode->item); 
	}
		
	inline static u32 GetNumDistantLodLightBuckets() { return m_DistantLODLightBuckets.GetNumUsed();}
	inline static CDistantLODLightBucket* GetDistantLodLightBucket(const u32 index) 
	{ 
		CDistantLODLightBucketNode* pNode = m_DistantLODLightBuckets.GetFirst()->GetNext();
		for(int i=0; i < index; i++)
		{
			pNode = pNode->GetNext();
		}
		return &(pNode->item); 
	}
#endif
	// LODLight Bucket Visibility buffer generation and access
	static void ResetLODLightsBucketVisibility();
	static void	BuildLODLightsBucketVisibility(CViewport *pViewport);
	inline static atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &GetLODLightsToRenderUpdateBuffer() { return m_LODLightData.m_LODLightsToRender; }
	inline static atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &GetLODLightsToRenderRenderBuffer() { return m_currentFrameInfo->m_LODLightsToRender; }

	// DistantLODLight Bucket Visibility buffer generation and access
	static void	BuildDistLODLightsVisibility(CViewport *pViewport);
	inline static atFixedArray<CDistantLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS>* GetDistLODLightsToRenderUpdateBuffers() { return m_LODLightData.m_DistLODLightsToRender; }
	inline static atFixedArray<CDistantLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &GetDistLODLightsToRenderUpdateBuffer(eLodLightCategory lightCategory) { return m_LODLightData.m_DistLODLightsToRender[lightCategory]; }
	inline static atFixedArray<CDistantLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &GetDistLODLightsToRenderRenderBuffer(eLodLightCategory lightCategory) { return m_currentFrameInfo->m_DistLODLightsToRender[lightCategory]; }

	static bool GetRenderLODLightsDisabledThisFrame() { return m_currentFrameInfo->m_disableThisFrame; }
	
	static bool GetUpdateLODLightsDisabledThisFrame() { return m_LODLightData.m_disableThisFrame; }
	static void SetUpdateLODLightsDisabledThisFrame() { m_LODLightData.m_disableThisFrame = true; }
	static void ResetUpdateLODLightsDisabledThisFrame() { m_LODLightData.m_disableThisFrame = false; } 

	static void RegisterBrokenLightsForEntity(CEntity *pEntity);
	static void UnregisterBrokenLightsForEntity(CEntity *pEntity);
	static void CheckFragsForBrokenLights();
	static void UnbreakLODLight(CLightEntity *pEntity);
	
	// Imap management
	static void CreateLODLightImapIDXCache();

	// PURPOSE
	// Regenerates the LOD light imap index cache withe the current loaded LOD light imap files.
	static void CacheLoadedLODLightImapIDX();

	// PURPOSE
	// Loads all LOD lights cached meeting the filter mask requirements.
	// The filter mask can specify whether to filter only LOD lights owned by map data, only owned by script or any.
	static void LoadLODLightImapIDXCache(u32 filterMask = LODLIGHT_IMAP_ANY);

	// Unloads all LOD lights cached meeting the filter mask requirements.
	// The filter mask can specify whether to filter only LOD lights owned by map data, only owned by script or any.
	// NOTES
	// Ensure gRenderThreadInterface.Flush() is called before unloading any IMAPs in use.
	static void UnloadLODLightImapIDXCache(u32 filterMask = LODLIGHT_IMAP_ANY);

	static bool GetIfCanLoad() { return m_LODLightsCanLoad; }

	static void SetupRenderthreadFrameInfo();
	static void ClearRenderthreadFrameInfo();
	static void InitDLCCommands();

	static LODLightData* GetLODLightData() { return &m_LODLightData; }

	static LODLightData* m_currentFrameInfo;
	static bool m_requireRenderthreadFrameInfoClear;
public:

	// CGlassPaneManager uses this too for positional hashing.
	static u32	hash(const u32	*k,	u32	length, u32	initval);

private:
	static bool HasLODLightImapFilterMask(strLocalIndex imapIndex, u32 filterMask);
	static void SetIfCanLoad();
	static void	TurnLODLightLoadingOnOff(bool bOn);

	static bool	m_LODLightsCanLoad;

	static fwLinkList<CLODLightBucket> m_LODLightBuckets;
	typedef fwLink<CLODLightBucket>	CLODLightBucketNode;

	static fwLinkList<CDistantLODLightBucket> m_DistantLODLightBuckets;
	typedef fwLink<CDistantLODLightBucket>	CDistantLODLightBucketNode;

	// Stores the indexs of the imaps that contain LODLight data, to speedup turning their loading off during the day.
	static atArray<u16> m_LODLightImapIDXs;

	static LODLightData m_LODLightData ;
};
#endif

#endif	// __LODLIGHTMANAGER_H__

