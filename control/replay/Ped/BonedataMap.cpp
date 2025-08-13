//=====================================================================================================
// name:		BonedataMap.cpp
//=====================================================================================================

#include "BonedataMap.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Control/replay/replay_channel.h"
#include "animation/AnimBones.h"
#include "debug/Debug.h"

s32 CReplayBonedataMap::sm_aPedBoneMapping[PED_BONE_COUNT];
s32 CReplayBonedataMap::sm_aPlayerBoneMapping[PLAYER_BONE_COUNT];

//=====================================================================================================
void CReplayBonedataMap::Init()
{
	
}

// u64 orientation and float position
bool CReplayBonedataMap::IsHighPrecisionBone(eAnimBoneTag uBoneTag)
{
	switch(uBoneTag)
	{
	case BONETAG_L_CLAVICLE:
	case BONETAG_L_UPPERARM:
	case BONETAG_L_FOREARM:
	case BONETAG_L_HAND:
	case BONETAG_R_CLAVICLE:
	case BONETAG_R_UPPERARM:
	case BONETAG_R_FOREARM:
	case BONETAG_R_HAND:
	//case BONETAG_SPINE:
	case BONETAG_SPINE1:
	case BONETAG_SPINE2:
	case BONETAG_SPINE3:
	case BONETAG_PELVIS:
	case BONETAG_NECK:
	case BONETAG_HEAD:
	case BONETAG_ROOT:
		return true;
		break;
	default:
		break;
	}

	return false;
}

// float position
bool CReplayBonedataMap::IsMajorBone(u16 uBoneTag)
{
	//todo4five temp as these are causing issues in the final builds
	(void) uBoneTag;

	/*TODO4FIVE switch(uBoneTag)
	{
	case BONETAG_L_THIGH:
	case BONETAG_L_CALF:
	case BONETAG_R_THIGH:
	case BONETAG_R_CALF:

	/// 80bone skel
	case BONETAG_FB_C_BROW:
	case 32692: //cheeks
	case BONETAG_FB_L_BROW:
	case BONETAG_FB_R_BROW:
	case BONETAG_FB_L_EYELID:
	case BONETAG_FB_R_EYELID:
	case BONETAG_POINTFB_C_JAW:
	case BONETAG_FB_C_JAW:
	case BONETAG_FB_L_CRN_MOUTH:
	case BONETAG_FB_R_CRN_MOUTH:
	case BONETAG_FB_R_LIPLOWER:
	case BONETAG_FB_L_LIPLOWER:
	case BONETAG_FB_L_LIPUPPER:
	case BONETAG_FB_R_LIPUPPER:

	/// 90bone skel
	// Brows
	case BONETAG_FB_L_BROWAJNT:
	case BONETAG_FB_R_BROWAJNT:
	// Lips
	case BONETAG_FB_L_LOLIPJNT:
	case BONETAG_FB_R_LOLIPJNT:
	case BONETAG_FB_L_UPPLIPJNT:
	case BONETAG_FB_R_UPPLIPJNT:
	// Lids
	case BONETAG_FB_L_UPPLIDJNT:
	case BONETAG_FB_R_UPPLIDJNT:

	// Cheeks
	case BONETAG_FB_L_UPPCHEEKJNT:
	case BONETAG_FB_R_UPPCHEEKJNT:
	case BONETAG_FB_L_LOCHEEKJNT:
	case BONETAG_FB_R_LOCHEEKJNT:
	case BONETAG_FB_L_CORNERLIPJNT:
	case BONETAG_FB_R_CORNERLIPJNT:
		// 14+1

		return true;
		break;
	default:
		break;

	}*/

	return false;
}

bool CReplayBonedataMap::IsMinorBone(s32 uBoneTag)
{

	switch(uBoneTag)
	{

	case BONETAG_R_FINGER0:
	case BONETAG_R_FINGER01:
	case BONETAG_R_FINGER02:
	case BONETAG_R_FINGER1:
	case BONETAG_R_FINGER11:
	case BONETAG_R_FINGER12:
	case BONETAG_R_FINGER2:
	case BONETAG_R_FINGER21:
	case BONETAG_R_FINGER22:
	case BONETAG_R_FINGER3:
	case BONETAG_R_FINGER31:
	case BONETAG_R_FINGER32:
	case BONETAG_R_FINGER4:
	case BONETAG_R_FINGER41:
	case BONETAG_R_FINGER42:

	case BONETAG_L_FINGER0:
	case BONETAG_L_FINGER01:
	case BONETAG_L_FINGER02:
	case BONETAG_L_FINGER1:
	case BONETAG_L_FINGER11:
	case BONETAG_L_FINGER12:
	case BONETAG_L_FINGER2:
	case BONETAG_L_FINGER21:
	case BONETAG_L_FINGER22:
	case BONETAG_L_FINGER3:
	case BONETAG_L_FINGER31:
	case BONETAG_L_FINGER32:
	case BONETAG_L_FINGER4:
	case BONETAG_L_FINGER41:
	case BONETAG_L_FINGER42:

		////case BONETAG_FB_C_BROW:
		////case BONETAG_FB_C_JAW:
		////case BONETAG_FB_L_LIPLOWER:
		////case BONETAG_FB_R_LIPLOWER:
		////case BONETAG_FB_L_BROW:
		////case BONETAG_FB_L_CRN_MOUTH:
		////case BONETAG_FB_L_EYEBALL:
		////case BONETAG_FB_L_EYELID:
		////case BONETAG_FB_L_LIPUPPER:
		////case BONETAG_FB_R_BROW:
		////case BONETAG_FB_R_CRN_MOUTH:
		////case BONETAG_FB_R_EYEBALL:
		////case BONETAG_FB_R_EYELID:
		////case BONETAG_FB_R_LIPUPPER:

		////case BONETAG_FB_L_BROWBJNT:
		////case BONETAG_FB_L_BROWAJNT:
		////case BONETAG_FB_C_FOREHEAD:
		////case BONETAG_FB_L_EYEJNT:
		////case BONETAG_FB_L_UPPCHEEKJNT:
		////case BONETAG_FB_L_CORNERLIPJNT:
		////case BONETAG_FB_L_LOCHEEKJNT:
		////case BONETAG_FB_L_UPPLIPJNT:
		////case BONETAG_FB_L_UPPLIDJNT:
		////case BONETAG_FB_L_LOLIDJNT:
		////case BONETAG_FB_R_BROWAJNT:

		////case BONETAG_FB_R_EYEJNT:
		////case BONETAG_FB_R_UPPLIDJNT:
		////case BONETAG_FB_R_LOLIDJNT:
		////case BONETAG_FB_R_BROWBJNT:
		////case BONETAG_FB_R_UPPCHEEKJNT:
		////case BONETAG_FB_R_UPPLIPJNT:
		////case BONETAG_FB_R_CORNERLIPJNT:
		////case BONETAG_FB_R_LOCHEEKJNT:
		////case BONETAG_FB_C_JAWJNT:
		////case BONETAG_FB_R_LOLIPJNT:
		////case BONETAG_FB_L_LOLIPJNT:
		////case BONETAG_FB_C_TONGUE_A_JNT:
		////case BONETAG_FB_C_TONGUE_B_JNT:

		////case BONETAG_POINTFB_C_JAW:
		////case BONETAG_POINTFB_R_LIPLOWER:
		////case BONETAG_POINTFB_L_LIPLOWER:
		////case BONETAG_POINTFB_L_LIPUPPER:
		////case BONETAG_POINTFB_R_LIPUPPER:

		return true;
		break;
	default:
		break;

	}

	return false;
}

#endif // GTA_REPLAY
