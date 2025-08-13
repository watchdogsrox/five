
#if __SPU || !__PPU

#include "../../basetypes.h"
#include "entity/drawdata.h"
#include "fwtl/LinkList.h"
#include "profile/profiler.h"
#include "system/eaptr.h"
#include "system/interlocked.h"
#include "system/task.h"
#include "../../scene/EntityTypes.h"
#include "../../scene/streamer/SceneStreamerBase.h"

#define _VALIDATE_RETURN_VOID(...)
#define __SHORTSORT(lo, hi, width, comp, context) shortsort(lo, hi, width, comp);
#define __COMPARE(context, p1, p2) comp(p1, p2)
#define STKSIZ (8*sizeof(void*) - 2)
#define CUTOFF 8            /* testing shows that this is good value */

#define ROUNDUP16(x)	(((x)+15)&~15)

class CEntity;

static const int TAG_STRINFO = 1;
static const int TAG_BUCKET = 2;

static void swap (
    char *a,
    char *b,
    size_t /*width*/
    )
{
	if ( a != b ) {
		BucketEntry temp = *(BucketEntry *) a;
		*(BucketEntry *) a = *(BucketEntry *) b;
		*(BucketEntry *) b = temp;
	}
}

static void shortsort (
    char *lo,
    char *hi,
    size_t width,
    int (*comp)(const void *, const void *)
    )
{
    char *p, *max;

    /* Note: in assertions below, i and j are alway inside original bound of
       array to sort. */

    while (hi > lo) {
        /* A[i] <= A[j] for i <= j, j > hi */
        max = lo;
        for (p = lo+width; p <= hi; p += width) {
            /* A[i] <= A[max] for lo <= i < p */
            if (__COMPARE(context, p, max) > 0) {
                max = p;
            }
            /* A[i] <= A[max] for lo <= i <= p */
        }

        /* A[i] <= A[max] for lo <= i <= hi */

        swap(max, hi, width);

        /* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j, j >= hi */

        hi -= width;

        /* A[i] <= A[j] for i <= j, j > hi, loop top condition established */
    }
    /* A[i] <= A[j] for i <= j, j > lo, which implies A[i] <= A[j] for i < j,
       so array is sorted */
}


void bucketqsort (
    void *base,
    size_t num,
    size_t width,
    int (*comp)(const void *, const void *)
    )
{
    char *lo, *hi;              /* ends of sub-array currently sorting */
    char *mid;                  /* points to middle of subarray */
    char *loguy, *higuy;        /* traveling pointers for partition step */
    size_t size;                /* size of the sub-array */
    char *lostk[STKSIZ], *histk[STKSIZ];
    int stkptr;                 /* stack for saving sub-array to be processed */

    /* validation section */
    _VALIDATE_RETURN_VOID(base != NULL || num == 0, EINVAL);
    _VALIDATE_RETURN_VOID(width > 0, EINVAL);
    _VALIDATE_RETURN_VOID(comp != NULL, EINVAL);

    if (num < 2)
        return;                 /* nothing to do */

    stkptr = 0;                 /* initialize stack */

    lo = (char *)base;
    hi = (char *)base + width * (num-1);        /* initialize limits */

    /* this entry point is for pseudo-recursion calling: setting
       lo and hi and jumping to here is like recursion, but stkptr is
       preserved, locals aren't, so we preserve stuff on the stack */
recurse:

    size = (hi - lo) / width + 1;        /* number of el's to sort */

    /* below a certain size, it is faster to use a O(n^2) sorting method */
    if (size <= CUTOFF) {
        __SHORTSORT(lo, hi, width, comp, context);
    }
    else {
        /* First we pick a partitioning element.  The efficiency of the
           algorithm demands that we find one that is approximately the median
           of the values, but also that we select one fast.  We choose the
           median of the first, middle, and last elements, to avoid bad
           performance in the face of already sorted data, or data that is made
           up of multiple sorted runs appended together.  Testing shows that a
           median-of-three algorithm provides better performance than simply
           picking the middle element for the latter case. */

        mid = lo + (size / 2) * width;      /* find middle element */

        /* Sort the first, middle, last elements into order */
        if (__COMPARE(context, lo, mid) > 0) {
            swap(lo, mid, width);
        }
        if (__COMPARE(context, lo, hi) > 0) {
            swap(lo, hi, width);
        }
        if (__COMPARE(context, mid, hi) > 0) {
            swap(mid, hi, width);
        }

        /* We now wish to partition the array into three pieces, one consisting
           of elements <= partition element, one of elements equal to the
           partition element, and one of elements > than it.  This is done
           below; comments indicate conditions established at every step. */

        loguy = lo;
        higuy = hi;

        /* Note that higuy decreases and loguy increases on every iteration,
           so loop must terminate. */
        for (;;) {
            /* lo <= loguy < hi, lo < higuy <= hi,
               A[i] <= A[mid] for lo <= i <= loguy,
               A[i] > A[mid] for higuy <= i < hi,
               A[hi] >= A[mid] */

            /* The doubled loop is to avoid calling comp(mid,mid), since some
               existing comparison funcs don't work when passed the same
               value for both pointers. */

            if (mid > loguy) {
                do  {
                    loguy += width;
                } while (loguy < mid && __COMPARE(context, loguy, mid) <= 0);
            }
            if (mid <= loguy) {
                do  {
                    loguy += width;
                } while (loguy <= hi && __COMPARE(context, loguy, mid) <= 0);
            }

            /* lo < loguy <= hi+1, A[i] <= A[mid] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[mid] */

            do  {
                higuy -= width;
            } while (higuy > mid && __COMPARE(context, higuy, mid) > 0);

            /* lo <= higuy < hi, A[i] > A[mid] for higuy < i < hi,
               either higuy == lo or A[higuy] <= A[mid] */

            if (higuy < loguy)
                break;

            /* if loguy > hi or higuy == lo, then we would have exited, so
               A[loguy] > A[mid], A[higuy] <= A[mid],
               loguy <= hi, higuy > lo */

            swap(loguy, higuy, width);

            /* If the partition element was moved, follow it.  Only need
               to check for mid == higuy, since before the swap,
               A[loguy] > A[mid] implies loguy != mid. */

            if (mid == higuy)
                mid = loguy;

            /* A[loguy] <= A[mid], A[higuy] > A[mid]; so condition at top
               of loop is re-established */
        }

        /*     A[i] <= A[mid] for lo <= i < loguy,
               A[i] > A[mid] for higuy < i < hi,
               A[hi] >= A[mid]
               higuy < loguy
           implying:
               higuy == loguy-1
               or higuy == hi - 1, loguy == hi + 1, A[hi] == A[mid] */

        /* Find adjacent elements equal to the partition element.  The
           doubled loop is to avoid calling comp(mid,mid), since some
           existing comparison funcs don't work when passed the same value
           for both pointers. */

        higuy += width;
        if (mid < higuy) {
            do  {
                higuy -= width;
            } while (higuy > mid && __COMPARE(context, higuy, mid) == 0);
        }
        if (mid >= higuy) {
            do  {
                higuy -= width;
            } while (higuy > lo && __COMPARE(context, higuy, mid) == 0);
        }

        /* OK, now we have the following:
              higuy < loguy
              lo <= higuy <= hi
              A[i]  <= A[mid] for lo <= i <= higuy
              A[i]  == A[mid] for higuy < i < loguy
              A[i]  >  A[mid] for loguy <= i < hi
              A[hi] >= A[mid] */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

        if ( higuy - lo >= hi - loguy ) {
            if (lo < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy;
                ++stkptr;
            }                           /* save big recursion for later */

            if (loguy < hi) {
                lo = loguy;
                goto recurse;           /* do small recursion */
            }
        }
        else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;               /* save big recursion for later */
            }

            if (lo < higuy) {
                hi = higuy;
                goto recurse;           /* do small recursion */
            }
        }
    }

    /* We have sorted the array, except for any pending sorts on the stack.
       Check if there are any, and do them. */

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;           /* pop subarray from stack */
    }
    else
        return;                 /* all subarrays done */
}




/* SPU task data input and output:
 *
 * Input - array of BucketEntry elements
 * Output - sorted array of BucketEntry elements
 * UserData[0].asInt - number of elements in Input
 */

int SortFunc(const void *a, const void *b)
{
	float scoreA = ((BucketEntry *) a)->m_Score;
	float scoreB = ((BucketEntry *) b)->m_Score;

	if (scoreA == scoreB)
	{
		return 0;
	}

	return (scoreA > scoreB) ? 1 : -1;
}

namespace SortBucketsStats
{
	PF_PAGE(SortBucketsPage,"SortBuckets");

	PF_GROUP(SortBuckets);
	PF_LINK(SortBucketsPage, SortBuckets);
	PF_TIMER(SortBucketsRun, SortBuckets);
}
using namespace SortBucketsStats;

// We're assuming that the 16 bytes in streamingBase will either get us an entire instance - either over the entire 16
// bytes (bank builds) or within those 16 bytes (release builds).
CompileTimeAssert(sizeof(strStreamingInfo) == 16 || sizeof(strStreamingInfo) == 8);

bool sortbucketsspu_Dependency(const rage::sysDependency& dependency)
{
	PF_FUNC(SortBucketsRun);

	sysEaPtr< u32 >	dependencyRunningEa	= static_cast< u32* >( dependency.m_Params[1].m_AsPtr );
	u32 elementCount = (u32)dependency.m_Params[3].m_AsInt;

	if(dependency.m_Params[4].m_AsPtr != NULL)
	{
		AddActiveEntitiesData* pData = static_cast<AddActiveEntitiesData*>(dependency.m_Params[4].m_AsPtr);
#if __BANK
		u32 localBucketCounts[STREAMBUCKET_COUNT];
		sysMemSet(localBucketCounts, 0, sizeof(localBucketCounts));
		u32* pBucketCounts = pData->m_BucketCounts;
#endif

#if USE_PAGED_POOLS_FOR_STREAMING
		strStreamingInfoPageHeader *baseInfoPageHeaders = (strStreamingInfoPageHeader *) dependency.m_Params[5].m_AsPtr;
#else // USE_PAGED_POOLS_FOR_STREAMING
		strStreamingInfo *baseInfoPtr = (strStreamingInfo *) dependency.m_Params[5].m_AsPtr;
#endif // USE_PAGED_POOLS_FOR_STREAMING
#if __SPU
		char *streamableBase = AllocaAligned(char, 16, 16); 
#endif // __SPU

		u32 capacity = dependency.m_Params[6].m_AsInt;

#if __SPU
		int storeSize = dependency.m_Params[7].m_AsInt;
		u8* scratchBuffer = static_cast<u8*>(dependency.m_Params[0].m_AsPtr);
		sysDmaLargeGetAndWait(scratchBuffer, (u32)pData->m_pStore, storeSize, TAG_BUCKET);
		u8* remainingScratch = scratchBuffer + storeSize;
		BucketEntry *pLocalBuckets = (BucketEntry*)remainingScratch;
		
		fwLink<ActiveEntity>* pStore = (fwLink<ActiveEntity>*)(scratchBuffer);
		u32 offset = ptrdiff_t_to_int((fwLink<ActiveEntity>*)(pData->m_pStore) - pStore);
#else
		BucketEntry *pBuckets = static_cast<BucketEntry*>(dependency.m_Params[2].m_AsPtr);
		u32 offset = 0;
#endif

		fwLink<ActiveEntity>* pFirst = (fwLink<ActiveEntity>*)(pData->m_pFirst) - offset;
		fwLink<ActiveEntity>* pLast = (fwLink<ActiveEntity>*)(pData->m_pLast) - offset;

		StreamBucket outsideBucket = STREAMBUCKET_ACTIVE_OUTSIDE_PVS;
		u32 frameCount = pData->m_frameCount;

		u32 localElementCount = 0;

		for (fwLink<ActiveEntity>* pLink = pFirst; pLink != pLast; pLink = pLink->GetNext() - offset)
		{
			Assert(pLink != NULL);
			Assert(pLink->GetNext() != NULL);

#if USE_PAGED_POOLS_FOR_STREAMING
			strIndex index = pLink->item.GetArchetypeIndex();
			strStreamingInfo *ppuStreamingInfo = baseInfoPageHeaders[index.GetPage()].GetElement(index.GetElement());
#else // USE_PAGED_POOLS_FOR_STREAMING
			strStreamingInfo *ppuStreamingInfo = &baseInfoPtr[pLink->item.GetArchetypeIndex().Get()];
#endif  // USE_PAGED_POOLS_FOR_STREAMING


#if __SPU
			// In non-bank builds, a streamable is only 8 bytes, so we need to move our address accordingly.
			strStreamingInfo *streamable = (strStreamingInfo *) (streamableBase + (((u32) ppuStreamingInfo) & 15));

			// We only need the first two words - that's where the flags are.
			sysDmaSmallGet(streamable, (u32) ppuStreamingInfo, sizeof(u32) * 2, TAG_STRINFO);
#else // __SPU
			strStreamingInfo *streamable = ppuStreamingInfo;
#endif // __SPU

			int timeSinceLastVisible = frameCount - pLink->item.GetLastVisibleTimestamp();
			float score = (timeSinceLastVisible > 0) ? outsideBucket + (1.0f - Min(((float)timeSinceLastVisible * 0.01f), 1.0f))
				: pLink->item.GetScore();

			Assert((score >= 0.0f) && (score < float(STREAMBUCKET_COUNT)));

#if __SPU
			sysDmaWait(1 << TAG_STRINFO);
#endif // __SPU
			if (!(streamable->GetFlags() & STR_DONTDELETEARCHETYPE_MASK))
			{
				if(elementCount + localElementCount < capacity)
				{
#if __SPU
					BucketEntry &entry = pLocalBuckets[localElementCount++];
#else
					BucketEntry &entry = pBuckets[elementCount++];
#endif
					entry.SetEntity((CEntity*)pLink->item.GetEntity(), pLink->item.GetEntityID());
					entry.m_Score = score;
#if __SPU
					if(localElementCount == BucketEntry::BUCKET_ENTRY_BATCH_SIZE)
					{
						BucketEntry* pBucketEA = static_cast<BucketEntry*>(dependency.m_Params[2].m_AsPtr) + elementCount;
						sysDmaLargePutAndWait(pLocalBuckets, (u32)pBucketEA, sizeof(BucketEntry)*localElementCount, TAG_BUCKET);
						elementCount += localElementCount;
						localElementCount = 0;
					}
#endif

#if __BANK
					localBucketCounts[(int)score]++;
#endif // __BANK
				}
				else
				{
#if __ASSERT
					// List is full. Is that because of a streaming volume? If so, it's kind of to be expected.
					bool bIsAnyVolumeActive = pData->m_bIsAnyVolumeActive;
					Assertf(bIsAnyVolumeActive, "StreamingBucketSet - cannot add entity as list is full. List dump in the TTY.");
#endif

#if !__FINAL
					u32* pDumpAllEntities = pData->m_DumpAllEntitiesPtr;
					sysInterlockedExchange(pDumpAllEntities, 1);
#endif
					// break out of the loop since we won't be able to fit any more entries
					break;
				}
			}
		}

#if __SPU
		if(localElementCount)
		{
			BucketEntry* pBucketEA = static_cast<BucketEntry*>(dependency.m_Params[2].m_AsPtr) + elementCount;
			sysDmaLargePutAndWait(pLocalBuckets, (u32)pBucketEA, ROUNDUP16(sizeof(BucketEntry)*localElementCount), TAG_BUCKET);
			elementCount += localElementCount;
			localElementCount = 0;
		}
#endif

		u32* pArrayCount = pData->m_EntityListCountPtr;
		sysInterlockedExchange(pArrayCount, elementCount);

#if __BANK
		for(int i = 0; i < STREAMBUCKET_COUNT; i++)
		{
			sysInterlockedAdd(pBucketCounts + i, localBucketCounts[i]);
		}
#endif

#if __ASSERT
		sysInterlockedExchange(pData->m_DataInUsePtr, 0);
#endif
	}

#if __SPU
	// we're done using the linked list store, so we can now re-use the scratch
	// buffer for the BucketEntry array
	BucketEntry* pBuckets = static_cast<BucketEntry*>(dependency.m_Params[0].m_AsPtr);
	int size = ROUNDUP16( sizeof(BucketEntry) * elementCount );
	sysDmaLargeGetAndWait(pBuckets, (u32)dependency.m_Params[2].m_AsPtr, size, TAG_BUCKET);
#else
	BucketEntry* pBuckets = static_cast<BucketEntry*>(dependency.m_Params[2].m_AsPtr);
#endif

	if(elementCount)
	{
		bucketqsort((char *) pBuckets, elementCount, sizeof(BucketEntry), SortFunc);
	}

#if __SPU
	sysDmaLargePutAndWait(pBuckets, (u32)dependency.m_Params[2].m_AsPtr, size, TAG_BUCKET);
#endif

	sysInterlockedExchange(dependencyRunningEa.ToPtr(), 0);

	return true;
}



#endif // __SPU || !__PPU

