/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodDrawable.cpp
// PURPOSE : Drawable LODs related code.
// AUTHOR :  Alex Hadjadj
// CREATED : 29/10/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "math/amath.h"

#include "scene/lod/LodDrawable.h"
#include "renderer/Renderer.h"
#include "scene/lod/LodScale.h"
#include "modelinfo/BaseModelInfo.h"

SCENE_OPTIMISATIONS();

DECLARE_MTR_THREAD bool LODDrawable::s_isRenderingLowestCascade = false;

float g_crossFadeDistance[2] = {5.0f,20.0f};

#if __BANK
int s_ForceLODOneOnShadow = -1;
int s_ForceLODCrossFade = -1;
int s_ForceLODCrossFadeHighLod = -1;
#endif // __BANK

u32 LODDrawable::CalculateDrawableLODFlags(const rmcLodGroup &lodGroup, u32 bucketType)
{
	u32 onlyIdx = LODFLAG_NONE;
	int flagCount = 0;
	int lodCount = 0;
	const u32 subMask = CRenderer::GenerateSubBucketMask(bucketType) << 8;

	for(int i=0;i<LOD_COUNT;i++)
	{
		if( lodGroup.ContainsLod(i) )
		{
			lodCount++;
			// Check bucketMask
			if( ((lodGroup.GetBucketMask(i) & 0xFF00) & subMask) != 0 )
			{
				flagCount++;
				if( onlyIdx == LODFLAG_NONE )
					onlyIdx = i;
			}
		}
	}

	if( lodCount == 1 || flagCount == lodCount) // All proxy
	{
		onlyIdx = LODFLAG_NONE;
	}
	else if( flagCount == 1 ) // 1 proxy
	{
		onlyIdx |= LODFLAG_FORCE_LOD_LEVEL;
	}

	return onlyIdx;
}

#if 0 // might be useful if we need to support crossfading and entity alpha together
void LODDrawable::CalcCrossFadeAlphas_DEPRECATED(s32 crossFade, u32 entityAlpha, u32& fadeAlphaN1, u32& fadeAlphaN2)
{
	Assert(crossFade >= 0 && crossFade <= 255);

	u32 fadeAlphaN1_ = entityAlpha * crossFade;
	u32 fadeAlphaN2_ = entityAlpha * (255 - crossFade);

	fadeAlphaN1 = (fadeAlphaN1_ * 66051UL)>>24;
	fadeAlphaN2 = (fadeAlphaN2_ * 66051UL)>>24;

#if __ASSERT
	{
		float fcrossFade = ((float)crossFade)/255.0f;
		float fentityAlpha = ((float)entityAlpha)/255.0f;

		float ffadeAlphaN1_ = fentityAlpha * fcrossFade;
		float ffadeAlphaN2_ = fentityAlpha * (1.0f - fcrossFade);

		u32 ffadeAlphaN1 = (u32)(ffadeAlphaN1_ * 255.0f);
		u32 ffadeAlphaN2 = (u32)(ffadeAlphaN2_ * 255.0f);
		int diffN1 = (int)ffadeAlphaN1 - (int)fadeAlphaN1;
		int diffN2 = (int)ffadeAlphaN2 - (int)fadeAlphaN2;
		Assertf(Abs(diffN1) < 2, "Trying to be clever : ffadeAlphaN1=%d, fadeAlphaN1=%d, crossFade=%d, entityAlpha=%d, diffN1=%d", ffadeAlphaN1, fadeAlphaN1, crossFade, entityAlpha, diffN1);
		Assertf(Abs(diffN2) < 2, "Trying to be clever : ffadeAlphaN2=%d, fadeAlphaN2=%d, crossFade=%d, entityAlpha=%d, diffN2=%d", ffadeAlphaN2, fadeAlphaN2, crossFade, entityAlpha, diffN2);
	}
#endif //__ASSERT
}
#endif

void LODDrawable::CalcLODLevelAndCrossfade(const rmcDrawable* pDrawable, float dist, u32 &LODLevel, s32 &crossFade, crossFadeDistanceIdx distIdx, u32 lastLODIdx ASSERT_ONLY(, const CBaseModelInfo *modelInfo), float typeScale)
{
	Assert(LODLevel == 0);
	Assert(crossFade == -1);

	// check if this drawable contains any LOD data : if so then do LOD selection
	if (pDrawable->GetLodGroup().ContainsLod(1))
	{
		// calc LOD from camera position
		float fScale = typeScale;
		
		if( CSystem::IsThisThreadId(SYS_THREAD_RENDER) )
			fScale *= g_LodScale.GetGlobalScaleForRenderThread();
		else
			fScale *= g_LodScale.GetGlobalScale();

		LODLevel = lastLODIdx;

		for (u32 i = 0; i < lastLODIdx; i++)
		{
			float LODDist = fScale * pDrawable->GetLodGroup().GetLodThresh(i);

			if (dist < LODDist)
			{
				LODLevel = i;

				if (i < lastLODIdx)
				{
					float fadeDist = LODDist - dist;
					float fadeRange = i == lastLODIdx-1 ? g_crossFadeDistance[distIdx] : g_crossFadeDistance[0];

					if (fadeDist < fadeRange)
					{
						float fCrossFade = fadeDist / fadeRange;
						crossFade = (u32)(255.0f * fCrossFade);
					}
				}

				break;
			}
		}

		// adjust LOD based on current renderphase settings etc.
		const u32 prevLODLevel = LODLevel;
		gDrawListMgr->AdjustDrawableLOD(CSystem::IsThisThreadId(SYS_THREAD_RENDER), LODLevel);
		LODLevel = Min<u32>(lastLODIdx, LODLevel);
		if (LODLevel != prevLODLevel)
			crossFade = -1;
	}

#if __BANK
	if( s_ForceLODCrossFade > -1 )
	{
		crossFade = s_ForceLODCrossFade;
	}
	
	if( s_ForceLODCrossFadeHighLod > -1 )
	{
		LODLevel = s_ForceLODCrossFadeHighLod;
	}
#endif // __BANK

	Assertf(LODLevel < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel), "%s wants Lod level %d but doesn't have it",modelInfo->GetModelName(), LODLevel);
	Assert(crossFade == -1 || (LODLevel+1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel+1)));
}

void LODDrawable::CalcLODLevelAndCrossfadeForProxyLod(const rmcDrawable* pDrawable, float dist, u32 selectedLODLevel, u32 &LODLevel, s32 &crossFade, crossFadeDistanceIdx distIdx, u32 lastLODIdx, float typeScale)
{
	Assert(LODLevel == 0);
	Assert(crossFade == -1);

	// check if this drawable contains any LOD data : if so then do LOD selection
	if (pDrawable->GetLodGroup().ContainsLod(1))
	{
		// calc LOD from camera position
		float fScale = typeScale;

		if( CSystem::IsThisThreadId(SYS_THREAD_RENDER) )
			fScale *= g_LodScale.GetGlobalScaleForRenderThread();
		else
			fScale *= g_LodScale.GetGlobalScale();

		LODLevel = lastLODIdx;

		for (u32 i = 0; i < lastLODIdx; i++)
		{
			float LODDist = fScale * pDrawable->GetLodGroup().GetLodThresh(i);

			if (dist < LODDist)
			{
				LODLevel = i;

				float fadeDist = LODDist - dist;
				float fadeRange = LODLevel == lastLODIdx-1 ? g_crossFadeDistance[distIdx] : g_crossFadeDistance[0];

				if (fadeDist < fadeRange)
				{
					float fCrossFade = fadeDist / fadeRange;
					crossFade = (s32)(255.0f * fCrossFade);
				}

				break;
			}
		}

		if (LODLevel < selectedLODLevel)
		{
			LODLevel = selectedLODLevel;
			crossFade = -1;
		}

#if __BANK
		if (s_ForceLODOneOnShadow > -1)
		{
			LODLevel = s_ForceLODOneOnShadow;
			crossFade = -1;
		}
#endif // __BANK

	}

	Assert(LODLevel < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel));
	Assert(crossFade == -1 || (LODLevel+1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel+1)));
}
