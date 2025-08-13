// SortPVSAsync
// jlandry: 02/10/2012

#if __WIN32 || __SPU || RSG_ORBIS

#define g_auxDmaTag		(0)

#include <algorithm>
#include "profile/profiler.h"
#include "system/eaptr.h"
#include "../renderer/PostScan.h"

static bool CompareLodSortVal(const CGtaPvsEntry& a, const CGtaPvsEntry& b) {
	return a.GetVisFlags().GetLodSortVal() < b.GetVisFlags().GetLodSortVal();
}

inline bool isValidEntry(const CGtaPvsEntry& entry)
{
	return entry.GetEntity() != NULL;
}

static u32 g_CurrentSortVal = 0;
inline bool IsEqualToCurrentSortVal(const CGtaPvsEntry& entry)
{
	return entry.GetVisFlags().GetLodSortVal() == g_CurrentSortVal;
}

namespace SortPVSAsyncStats
{
	PF_PAGE(SortPVSAsyncPage,"SortPVSAsync");

	PF_GROUP(SortPVSAsync);
	PF_LINK(SortPVSAsyncPage, SortPVSAsync);
	PF_TIMER(SortPVSAsyncRun, SortPVSAsync);
}
using namespace SortPVSAsyncStats;


bool SortPVSAsync_Dependency(const sysDependency& dependency)
{
	PF_FUNC(SortPVSAsyncRun);

	CGtaPvsEntry* const pStart	= (CGtaPvsEntry*)(dependency.m_Params[0].m_AsPtr);
	CGtaPvsEntry* const pStop	= (CGtaPvsEntry*)((u8*)pStart + dependency.m_DataSizes[0]);

	sysEaPtr< CGtaPvsEntry* > pvsEndEa	= static_cast< CGtaPvsEntry** >( dependency.m_Params[1].m_AsPtr );
	sysEaPtr< u32 >	dependencyRunningEa	= static_cast< u32* >( dependency.m_Params[2].m_AsPtr );

	// First, partition out the invalid entries.
	CGtaPvsEntry* const startInvalidEntries = std::partition(pStart, pStop, isValidEntry);

	const bool bDoPartitionInsteadOfSort = dependency.m_Params[3].m_AsBool;
	if(bDoPartitionInsteadOfSort)
	{
		const u32 minSortVal = (u32) dependency.m_Params[4].m_AsInt;
		const u32 maxSortVal = (u32) dependency.m_Params[5].m_AsInt;
		 
		// Next, loop from min sort val -> max sort val, partitioning the valid PVS entries from low -> high 
		// LOD sort value. We actually don't need to include the value of maxSortVal, since once we get to the 
		// last sort val, all the remaining PVS entries should be of that sort value anyways, by design of the 
		// partitioning.
		CGtaPvsEntry* currentPartitionStart = pStart;
		for(u32 i = minSortVal; i < maxSortVal; i++)
		{
			g_CurrentSortVal = i;
			currentPartitionStart = std::partition(currentPartitionStart, startInvalidEntries, IsEqualToCurrentSortVal);
		}
	}
	else
	{
		// If we're using the old std::sort() code-path, then sort from start up until the invalidated entries.
		std::sort(pStart, startInvalidEntries, &CompareLodSortVal);
	}

#if __SPU
	sysDmaLargePutAndWait(pStart, uptr(dependency.m_Params[6].m_AsPtr), dependency.m_DataSizes[0], 1);
#endif

	// Finally, we can also be efficient and write-back the pvs end pointer as the start of all the invalidated 
	// entries, so that any subsequent loops don't need to loop over the junk at the end.
	sysInterlockedAddPointer((volatile void**)pvsEndEa.ToPtr(), sizeof(CGtaPvsEntry) * (startInvalidEntries - pStop));

	// Signal that the sort job is done.
	sysInterlockedExchange(dependencyRunningEa.ToPtr(), 0);

	return true;
}

#endif