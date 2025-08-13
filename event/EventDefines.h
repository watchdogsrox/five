#ifndef INC_EVENT_DEFINES_H
#define INC_EVENT_DEFINES_H

// Game headers
#include "fwtl/pool.h"

//////////////////////////////////////////////////////////////////////////
// Sounds
//////////////////////////////////////////////////////////////////////////

// Various sound ranges, in meters, when perceived by a normal human.
// Most of these were computed to be equivalent with the old volume values in dB,
// with 30 dB being the threshold for audible.

#define SOUNDRANGE_BARELY_AUDIBLE	(1.0f)		// 30 dB
#define SOUNDRANGE_MOSTLY_AUDIBLE	(1.8f)		// 35 dB
#define SOUNDRANGE_CLEARLY_AUDIBLE	(3.1f)		// 40 dB
#define SOUNDRANGE_QUIET			(10.0f)		// 50 dB
#define SOUNDRANGE_MEDIUM			(31.6f)		// 60 dB
#define SOUNDRANGE_LOUD				(100.0f)	// 70 dB
#define SOUNDRANGE_VERY_LOUD		(300.0f)	// was 120 dB, but that would be audible at >2200 m which seems too large to be useful in a game
#define SOUNDRANGE_DEAFENING		(1000.0f)	// was 160 dB, but that would be audible at >5500 m which seems too large to be useful in a game

#define SOUNDRANGE_WHISPER			(1.0f)		// 30 dB
#define SOUNDRANGE_TALKING			(31.6f)		// 60 dB
#define SOUNDRANGE_SHOUT			(100.0f)	// was 100 dB, which would be 1000 m, seemed too large.

#define SOUNDRANGE_NORM_FOOTSTEPS	(5.6f)		// 45 dB
#define SOUNDRANGE_HEAVY_FOOTSTEPS	(17.8f)		// 55 dB

#define SOUNDRANGE_SOFT_HIT			(10.0f)		// 50 dB
#define SOUNDRANGE_MEDIUM_HIT		(17.8f)		// 55 dB
#define SOUNDRANGE_HARD_HIT			(31.6f)		// 60 dB

// Some unused values, still in dB:
//	#define SOUNDVOL_CARTRAFFIC			(60.0f)
//	#define SOUNDVOL_DOORBELL			(70.0f)
//	#define SOUNDVOL_MOTORBIKE			(95.0f)
//	#define SOUNDVOL_CARHORN			(110.0f)
//	#define SOUNDVOL_GUNSHOT			(160.0f)
//	#define SOUNDVOL_EXPLOSION			(170.0f)

//////////////////////////////////////////////////////////////////////////
// REGISTER_CLASS_POOL_EVENT
//////////////////////////////////////////////////////////////////////////

#if __DEV
#define USE_CUSTOM_EVENT_POOL_ALLOCATOR 1
#else
#define USE_CUSTOM_EVENT_POOL_ALLOCATOR 0
#endif // __DEV

#if EVENT_EXTENDED_REF_COUNT_INFO
#define REGISTER_CLASS_POOL_EVENT_EXTENDED_REF_COUNT_INFO_SETFLAGS p->SetRefAwareFlags( fwExtRefAwareBase::F_DynamicallyAllocated );
#else
#define REGISTER_CLASS_POOL_EVENT_EXTENDED_REF_COUNT_INFO_SETFLAGS
#endif // EVENT_EXTENDED_REF_COUNT_INFO

#if USE_CUSTOM_EVENT_POOL_ALLOCATOR
#define FW_REGISTER_CLASS_POOL_EVENT(_T)										\
	/* macro used to register that a class uses a pool to allocate from */		\
	typedef fwPool<_T > Pool;													\
	static Pool* _ms_pPool;														\
	void* operator new(size_t ASSERT_ONLY(nSize) RAGE_NEW_EXTRA_ARGS_UNUSED)	\
	{																			\
		Assert(_ms_pPool);														\
		Assertf(nSize <= _ms_pPool->GetStorageSize(), "Allocating event of size %" SIZETFMT "i.  Max slot size in event pool is %" SIZETFMT "i.", nSize, _ms_pPool->GetStorageSize());	\
		CEvent* p = _ms_pPool->New();											\
		REGISTER_CLASS_POOL_EVENT_EXTENDED_REF_COUNT_INFO_SETFLAGS;				\
		return p;																\
	}																			\
	void* operator new(size_t ASSERT_ONLY(nSize), s32 PoolIndex)				\
	{																			\
		Assert(_ms_pPool);														\
		Assertf(nSize <= _ms_pPool->GetStorageSize(), "Allocating event of size %" SIZETFMT "i.  Max slot size in event pool is %" SIZETFMT "i.", nSize, _ms_pPool->GetStorageSize());	\
		CEvent* p = _ms_pPool->New(PoolIndex);									\
		REGISTER_CLASS_POOL_EVENT_EXTENDED_REF_COUNT_INFO_SETFLAGS;				\
		return p;																\
	}																			\
	void operator delete(void* pVoid)											\
	{																			\
		Assert(_ms_pPool);														\
		_ms_pPool->Delete((_T*)pVoid);											\
	}																			\
	static void InitPool(const MemoryBucketIds membucketId = MEMBUCKET_DEFAULT, int redZone = 0);			\
	static void InitPool(int size, const MemoryBucketIds membucketId = MEMBUCKET_DEFAULT, int redZone = 0);	\
	static void ShutdownPool() { delete _ms_pPool; }							\
	static Pool* GetPool() { return _ms_pPool; }								\
	/* end of FW_REGISTER_CLASS_POOL */
#else
#define FW_REGISTER_CLASS_POOL_EVENT(_T)	FW_REGISTER_CLASS_POOL(_T)
#endif // USE_CUSTOM_EVENT_POOL_ALLOCATOR

#endif // INC_EVENT_DEFINES_H
