// 
// animation/AnimBones.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef ANIM_BONES_H
#define ANIM_BONES_H

// Gta headers
#include "AnimDefines.h"
#include "fwanimation/boneids.h"

// Rage forward declarations
namespace rage
{
	class crSkeletonData;
}

// Enum denoting the skeleton used to export an anim from motion builder
// used to calculate root translation fixup, etc
enum eAnimSkeletonType
{
	SKELETON_TYPE_INVALID = -1,
	SKELETON_TYPE_MALE = 0,
	SKELETON_TYPE_FEMALE,
	SKELETON_TYPE_MAX
};

// Enum to identify clock bones
enum eSingleAxisRotationTags
{
	BONETAG_HOUR_HAND	= 419,
	BONETAG_MINUTE_HAND	= 418,
	BONETAG_SECOND_HAND	= 420,
	
	BONETAG_X_AXIS_SLOW	= 421,
	BONETAG_Y_AXIS_SLOW	= 422,
	BONETAG_Z_AXIS_SLOW	= 423,

	BONETAG_X_AXIS_FAST	= 424,
	BONETAG_Y_AXIS_FAST	= 425,
	BONETAG_Z_AXIS_FAST	= 426,
};

// Bone tags are used to identify bones in a skeleton
// Even if the bone name (in max) changed the bone tag would stay the same
class CPedBoneTagConvertor
{
public:

	static void Init();
	static void Shutdown();

	// 'bone index' is the index into the skeleton bone array that corresponds to a specific bone tag
	// 'bone tag' is a number used to identify bones in the anims (used in *.skel and *.anim files)
	// 'bone mask index' is the index into the array of bone masks

	// Get the bone index from bone tag (and a 'peds' skeleton data)
	// Returns -1 if the bone tag does not exist
	static s32 GetBoneIndexFromBoneTag(const crSkeletonData& skeletonData, const eAnimBoneTag boneTag);
};

#endif




















