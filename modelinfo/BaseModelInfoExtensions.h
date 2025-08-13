#ifndef BASEMODELINFO_EXTENSIONS_H_
#define BASEMODELINFO_EXTENSIONS_H_

// rage headers
namespace rage {
class crSkeletonData;
};
// game headers

// framework headers
#include "entity/extensionlist.h"

// Bone ID extension : allows to store an extra boneid to boneIndex conversion table, storing up to 8 component.
// Bone indices are expected to be less than 254 (255 is used as no index).

#define MAX_BONEIDS 7

class BaseModelInfoBoneIndices : public fwExtension
{
public:
	EXTENSIONID_DECL(BaseModelInfoBoneIndices, fwExtension);

	// Variables
	BaseModelInfoBoneIndices(crSkeletonData * skeletonData, const char *boneNames[]);
	~BaseModelInfoBoneIndices() { /* NoOp */ }

	ASSERT_ONLY(void VerifyBoneIndices(crSkeletonData * skeletonData, const char *boneNames[]);)
	
	__forceinline void SetBoneIndice(u32 componentId, u32 boneId) { modelinfoAssert(boneId<255); modelinfoAssert(componentId < MAX_BONEIDS); m_boneIndices[componentId] = boneId & 0xff; }
	__forceinline u32 GetBoneIndice(u32 componentId) const { modelinfoAssert(componentId < MAX_BONEIDS); return (u32)m_boneIndices[componentId]; }

	void CalculateBoneCount();
	__forceinline u32 GetBoneCount() const { return m_boneCount; }


private:
	u8	m_boneIndices[MAX_BONEIDS];
	u8  m_boneCount;
};

#endif