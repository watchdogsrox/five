// 
// animation/AnimBones.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#include "AnimBones.h"

// Rage headers
#include "crSkeleton/SkeletonData.h"

// Gta headers
#include "animation/anim_channel.h "

ANIM_OPTIMISATIONS()

void CPedBoneTagConvertor::Init()
{
	/*
	// Print out the bone tags
	Displayf("Print out the bone tags\n");
	for (int i = 0; i<BONE_MASK_INDEX_NUM+1; i++)
	{
		eAnimBoneTag boneTag = (eAnimBoneTag)ms_aBoneIndexToTag[i];
		Displayf("Common bone [%d] = %d = %s",  i, (s32)boneTag, GetBoneNameFromBoneTag(boneTag));
	}

	// Print out the contents of each bonemask
	Displayf("Print out the contents of each bonemask\n");
	for (nMask=0; nMask<EBONEMASK_MAX; nMask++)
	{
		Displayf("Mask : %s\n",  BoneMaskToString((eAnimPriority)nMask));
		for(nBoneEntry=0; nBoneEntry<BONE_MASK_INDEX_NUM; nBoneEntry++)
		{
			eAnimBoneTag boneTag = (eAnimBoneTag)ms_aBoneIndexToTag[nBoneEntry];
			Displayf("BoneMaskIndex[%d] = [%d] = %6.4f = %s = %s", nBoneEntry, (s32)boneTag, saBoneMasks[nMask][nBoneEntry], GetBoneNameFromBoneTag(boneTag), ms_animBoneNames[nBoneEntry]);
		}
	}
	*/
}

void CPedBoneTagConvertor::Shutdown()
{
}

s32 CPedBoneTagConvertor::GetBoneIndexFromBoneTag(const crSkeletonData& skeletonData, const eAnimBoneTag boneTag)
{
	int boneIndex = 0;
	if (skeletonData.HasBoneIds() && skeletonData.ConvertBoneIdToIndex((u16)boneTag, boneIndex))		//! eAnimBoneTag is in u16 range
	{
		animAssert(boneIndex < skeletonData.GetNumBones());
		return boneIndex;
	}
	else
	{
		return -1;
	}
}
