// rage headers
#include "crskeleton/skeletondata.h"

// game headers
#include "modelinfo_channel.h"
#include "BaseModelInfoExtensions.h"

// framework headers
AUTOID_IMPL(BaseModelInfoBoneIndices);

// Bone ID extension : allows to store an extra boneid to boneIndex conversion table, storing up to 8 component.
// Bone indices are expected to be less than 255.

#if __ASSERT
void BaseModelInfoBoneIndices::VerifyBoneIndices(crSkeletonData * skeletonData, const char *boneNames[])
{
	s32 i=0;
	while(boneNames[i])
	{
		const crBoneData *bone = skeletonData->FindBoneData(boneNames[i]);
		u8 generatedIdx = bone ? (u8) bone->GetIndex() : 0xff;;
		Assertf(generatedIdx == m_boneIndices[i],"Missmatched bone index setup, did the object change between reload ?");
		i++;
		modelinfoAssert(i < MAX_BONEIDS);
	}
}

#endif // __ASSERT
BaseModelInfoBoneIndices::BaseModelInfoBoneIndices(crSkeletonData * skeletonData, const char *boneNames[])
{
	memset(m_boneIndices,0xff,sizeof(u8)*MAX_BONEIDS);
	
	s32 i=0;
	while(boneNames[i])
	{
		const crBoneData *bone = skeletonData->FindBoneData(boneNames[i]);
		m_boneIndices[i] = bone ? (u8) bone->GetIndex() : 0xff;
		i++;
		modelinfoAssert(i < MAX_BONEIDS);
	}
}

void BaseModelInfoBoneIndices::CalculateBoneCount()
{
	int bonecount = 0;
	while(m_boneIndices[bonecount] != 0xff)
	{
		bonecount++;
	}
	
	m_boneCount = (u8)(bonecount & 0xff);
}

