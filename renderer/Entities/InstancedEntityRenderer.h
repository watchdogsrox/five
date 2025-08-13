#ifndef _INSTANCED_ENTITY_RENDERER_H_INCLUDED_
#define _INSTANCED_ENTITY_RENDERER_H_INCLUDED_

#include "fwrenderer/instancing.h"
#include "entity/drawdata.h"
#include "renderer/DrawLists/drawList.h"
#include "scene/Entity.h"
#include "atl/singleton.h"

//Forward declare
namespace rage
{
	class fwDrawData;
	class bkBank;
}

class InstancedEntityCache
{
public:
	InstancedEntityCache();
	InstancedEntityCache(CEntity *entity, fwDrawData *drawHandler);

	void SetEntity(CEntity *entity, fwDrawData *drawHandler);

	inline bool IsValid() const				{ return mEntity != NULL; }
	inline u32 GetModelIndex() const		{ return IsValid() ? mEntity->GetModelIndex() : -1; }

	inline bool IsCrossFading() const		{ return mCrossFade > -1; }
	inline bool IsCurrLODInstanced() const	{ return mIsLODModelInstanced; }
	inline bool IsNextLODInstanced() const	{ return mIsNextLODModelInstanced; }
	inline bool IsInstanced() const			{ return IsCurrLODInstanced() || IsNextLODInstanced(); }

	inline bool ShouldSetStipple() const	{ return mSetStipple; }
	inline u32 GetStippleAlpha() const		{ return (IsCrossFading() ? (mCrossFade * mActualAlphaFade) / 255 : mActualAlphaFade); }

	inline CEntity *GetEntity() const			{ return mEntity; }
	inline fwDrawData *GetDrawHandler() const	{ return mDrawHandler; }

	//Helper for partition
	inline bool DrawsLODLevel(u32 lodLevel) const						{ return (lodLevel == mLODlevel || (mCrossFade > -1 && lodLevel == mLODlevel + 1)); }
	inline bool IsInSameBatch(const InstancedEntityCache &other) const	{ return GetModelIndex() == other.GetModelIndex() && mLODlevel == other.mLODlevel && IsCrossFading() == other.IsCrossFading() && (!mSetStipple || mStippleMask == other.mStippleMask); }

	//Useful for tagging instanced batches
	static void PushPixTagsForBatch(u32 modelIndex, u32 batchSize);
	static void PopPixTagsForBatch();

private:
	template<int a> friend class CInstancedRendererBatcher;

	//Can hopefully remove at some point - this function helps the instance renderer decide if the entity should also be drawn using the non-intanced draw pipeline.
	bool OnlyNeedsInstancedRenderer() const;

	void RecomputeLODLevel();

	CEntity *mEntity;
	fwDrawData *mDrawHandler;

	u32 mLODlevel;
	s32 mCrossFade;
	u16 mStippleMask;
	u32 mActualAlphaFade					: 8;
	bool mIsLODModelInstanced				: 1;
	bool mIsNextLODModelInstanced			: 1;
	bool mLODModelHasNonInstancedGeom		: 1;
	bool mNextLODModelHasNonInstancedGeom	: 1;
	bool mSetStipple						: 1;
};

#ifndef _NOEXCEPT
#define _NOEXCEPT
#endif // _NOEXCEPT


template <int EntityCacheSize>
class CInstancedRendererBatcher
{
public:
	static const int sEntityCacheSize = EntityCacheSize;

	bool AddInstancedData(CEntity *entity, fwDrawData *drawHandler);
	void Flush();

private:
	typedef atFixedArray<InstancedEntityCache, EntityCacheSize> EntityCacheList;
	typedef atRangeArray<InstancedEntityCache, EntityCacheSize> StablePartitionBuffer;
	template <typename T> friend std::pair<T *, ptrdiff_t> std::get_temporary_buffer(ptrdiff_t _Count) _NOEXCEPT;

	void RenderBatch(typename EntityCacheList::const_iterator start, typename EntityCacheList::const_iterator end, u32 lod);
	StablePartitionBuffer &GetStablePartitionBuffer() const	{ return mStablePartitionBuffer; }

	EntityCacheList mEntityCache;
	mutable StablePartitionBuffer mStablePartitionBuffer;
};

struct InstancedRendererConfig
{
	static void Init();

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

	static BankBool sRenderInstancedGeometry; //Useful for seeing/debugging what goes through the instanced renderer.
	static BankBool sAddCSEToDrawlist;
	static BankBool sCSESetBatchRenderStates;
	static BankBool sAddPixBatchAnnotation;

	//Cross Fade debugging
	static BankBool sRenderNonCrossFadingGeometry;
	static BankBool sRenderFadingOutGeometry;
	static BankBool sRenderFadingInGeometry;

#if __BANK
	//LOD debugging
	typedef atRangeArray<bool, LOD_COUNT> LODRenderArray;
	static LODRenderArray sRenderLodGeometry;
#endif

	//Temp - Output instanced draw list
	static BankBool sOutputGbufferInstancedDrawList1Frame;
	static BankBool sOutputGbufferInstancedBatches1Frame;
};

//Instance of our instanced renderer batcher
typedef atSingleton<CInstancedRendererBatcher<1024> > InstancedRenderManager;
#define g_InstancedBatcher InstancedRenderManager::GetInstance()

#include "InstancedEntityRenderer_inline.h"

#endif //_INSTANCED_ENTITY_RENDERER_H_INCLUDED_