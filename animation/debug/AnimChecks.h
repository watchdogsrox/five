// 
// animation/debug/AnimChecks.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __DEV

#ifndef ANIMCHECKS_H
#define ANIMCHECKS_H

//***************************************************************************************
//	Filename : AnimChecks.h
//	Purpose : A set of functions which can be used to check that anims & sets have
//	been authored & exported correctly.
//***************************************************************************************

// Rage includes
#include "crskeleton/skeletondata.h"
#include "crskeleton/skeleton.h"
#include "fwanimation/anim_channel.h"
#include "fwanimation/animdefines.h"
#include "vector/matrix34.h"

#include "animation/AnimBones.h"

#define ANIMCHK_OUT(bAssert, pStream, ...)						{ animAssertf(bAssert==false, __VA_ARGS__); animDebugf1(__VA_ARGS__); animDebugf1("\n"); if(pStream) fprintf(pStream, __VA_ARGS__); }

class CAnimChecks
{
public:
	CAnimChecks();
	~CAnimChecks();

	//****************************
	// General purpose functions

	// Compares the local-space positions of a bone in input anims at the specified phases
	static bool CompareBonePositions(eAnimBoneTag iBoneTag, const crClip * pAnim1, const crClip * pAnim2, float fPhase1, float fPhase2, float fMaxBonePosDiff = 0.5f);

	// Compares the bone positions in two anims at the specified phases, and check if all bones are within given distance
	static bool ComparePoses(const crClip * pAnim1, const crClip * pAnim2, float fPhase1, float fPhase2, float fMaxBonePosDiff = 0.5f);

	// Returns which foot is forwards (furthest in the -Y direction) at the given phase. (-1 for left, +1 for right, or 0 if couldn't be determined)
	static int WhichFootIsForwards(const crClip * pAnim, float fPhase, float fRequiredDiff = 0.125f);

	// Counts how many complete walk-cycles there are in this anim
	static int CountNumWalkLoops(const crClip * pAnim);

	// Returns the amount that this anim turns the ped (rotates around Z)
	static float GetTotalTurnAmount(const crClip * pAnim);

	// Test the all the anims
	static bool TestAllAnims();
};












#endif
#endif	// __DEV
