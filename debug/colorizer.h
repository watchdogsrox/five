/////////////////////////////////////////////////////////////////////////////////
// FILE		: colorizer.h
// PURPOSE	: To provide in game visual helpers for debugging.
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_COLORIZER_H_
#define INC_COLORIZER_H_

#if __BANK

#include "rmcore/lodgroup.h"

namespace rage
{
	class rmcDrawable;
}

class CBaseModelInfo;
class CEntity;

namespace ColorizerUtils
{
	void GetDrawablePolyCount(const rmcDrawable* pDrawable, u32* pResult, u32 lodLevel = LOD_HIGH);
	void GetDrawableInfo(const rmcDrawable* pDrawable, u32* pPolyCount, u32* pVertexCount, u32* pVertexSize, u32* pIndexCount, u32* pIndexSize, u32 lodLevel = LOD_HIGH);
	void GetPolyCountFrag(CEntity *pEntity, CBaseModelInfo *pModelInfo, u32* pResult, u32 lodLevel = LOD_HIGH);
	void GetFragInfo(CEntity *pEntity, CBaseModelInfo *pModelInfo, u32* pPolyCount, u32* pVertexCount, u32* pVertexSize, u32* pIndexCount, u32* pIndexSize, u32 lodLevel = LOD_HIGH);
	u32 GetPolyCount(CEntity* pEntity);
	u32 GetBoneCount(CEntity* pEntity);
	u32 GetShaderCount(CEntity* pEntity);

	void GetVVRData(const rmcDrawable *drawable, float &vtxCount, float &volume);
	void FragGetVVRData(const CEntity *entity, const CBaseModelInfo *modelinfo, float &vtxCount, float &volume);
#if __PPU
	bool GetEdgeUseage(const rmcDrawable *drawable);
	bool FragGetEdgeUseage(const CEntity *entity, const CBaseModelInfo *modelinfo);
#endif // __PPU
};

#endif // __BANK
#endif // INC_COLORIZER_H_
