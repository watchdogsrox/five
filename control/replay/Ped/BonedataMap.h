//=====================================================================================================
// name:		BonedataMap.h
// description:	Mapping for bone data.
//=====================================================================================================

#ifndef INC_BONEDATAMAP_H_
#define INC_BONEDATAMAP_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "fwanimation/boneids.h"

#define PED_BONE_COUNT			(80)
#define PED_XTRA_BONE_COUNT		(10)
#define PLAYER_BONE_COUNT		(90)
#define PED_MAJOR_BONES			(19)	// Max(14,15) == 15 + 4
#define PED_HIGH_PREC_BONES		(16)

class CReplayBonedataMap
{
public:
	static void Init();

	static bool IsMajorBone(u16 uBoneTag);
	static bool IsHighPrecisionBone(eAnimBoneTag uBoneTag);

	static bool IsMinorBone(s32 uBoneTag);
private:
	static s32 sm_aPedBoneMapping[PED_BONE_COUNT];
	static s32 sm_aPlayerBoneMapping[PLAYER_BONE_COUNT];
};

#endif // GTA_REPLAY

#endif // INC_BONEDATAMAP_H_
