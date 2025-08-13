/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodScale.h
// PURPOSE : manages all lod scales
// AUTHOR :  Ian Kiigan
// CREATED : 07/03/13
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_LOD_LODSCALE_H_
#define _SCENE_LOD_LODSCALE_H_

#include "fwscene/lod/LodTypes.h"
#include "scene/EntityTypes.h"
#include "system/system.h"

class CLodScale
{
public:
	CLodScale();

	void Init();
	void Update();

	float GetGlobalScale() const { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return m_fGlobal_Out; }
	float GetGlobalScaleIgnoreSettings() const { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return m_fGlobalIgnoreSettings_Out; }
	float GetGlobalScaleForRenderThread() const { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_fGlobalRT_Out; }
	void SetGlobalScaleForRenderthread(float scale) { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_fGlobalRT_Out = scale; }
	void SetGrassScaleForRenderthread(float scale) { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_fGrassRT_Out = scale; }

	
	float GetPerLodLevelScale(u32 lodLevel) const { return m_afPerLodLevel_Out[lodLevel]; }
	float GetGrassBatchScale() const { return m_fGrass_Out; }	// TODO - replace with per-entity-type scales
	float GetGrassBatchScaleForRenderThread() const { return m_fGrassRT_Out; }
	float GetEntityTypeScale(u32 entityType) const { return (entityType==ENTITY_TYPE_GRASS_INSTANCE_LIST) ? m_fGrass_Out : 1.0f; }
	void SetGlobalScaleFromScript(const float fGlobalScale);
	void SetRemappingFromScript(const float fOldMin, const float fOldMax, const float fNewMin, const float fNewMax);

	float GetRootScale();	// this is the baseline scale applied to the global scale and all others. it is ONLY queried by the visibility jobs.
	void SetRootScale(float fScale) { m_fRoot_In = fScale; }
	
	float GetGrassQualityScale() { return m_fGrassQualityScale; }
	void SetGrassQualityScale(float fScale) { m_fGrassQualityScale = fScale; }
	
private:
	void CalculateGlobalScale(float& fGlobalScale, float& fCameraScale);
	void GetTimeCycleValues();
	void GetGrassBatchValues();

#if RSG_PC
	void ProcessOverrides(float& fGlobalScale, const float fCameraScale, const float fPrevScale);
#else
	void ProcessOverrides(float& fGlobalScale, const float fCameraScale);
#endif

	void BlendOutAfterOverride(float& fGlobalScale);
	void HardenAllOutputScales(const float fGlobalScale);
	void PostUpdate(const float fGlobalScale);

	float m_fRoot_In;
	float m_fMarketing_In;
	float m_fScript_In;
	float m_afPerLodLevel_In[LODTYPES_DEPTH_TOTAL];
	float m_fGrass_In;
	float m_fTimeCycleGlobal_In;

	float m_fGlobalIgnoreSettings_Out;
	float m_fGlobal_Out;
	float m_fGlobalRT_Out;
	float m_fGrassRT_Out;
	float m_afPerLodLevel_Out[LODTYPES_DEPTH_TOTAL];
	float m_fGrass_Out;

	u32 m_bScriptOverride : 1;
	u32 m_bBlendAfterOverride : 1;
	u32 m_bScriptOverrideThisFrame : 1;
	u32 m_bScriptRemappingThisFrame : 1;

	float m_fGrassQualityScale;

	float m_blendOutStart;
	float m_blendOutCurrentTime;
	float m_blendOutDuration;
	
	float m_fRemapOldMin;
	float m_fRemapOldMax;
	float m_fRemapNewMin;
	float m_fRemapNewMax;

#if __BANK
public:
	// marketing camera
	float GetMarketingCameraScale_In() const { return m_fMarketing_In; }
	void SetMarketingCameraScale_In(float fScale) { m_fMarketing_In = fScale; }

	// debug widgets
	void SetGrassScale_In(float fScale) { m_fGrass_In = fScale; }
	float GetGrassScale_In() { return m_fGrass_In; }
	void SetPerLodLevelScale_In(u32 lodLevel, float fScale) { m_afPerLodLevel_In[lodLevel] = fScale; }
	float GetPerLodLevelScale_In(u32 lodLevel) { return m_afPerLodLevel_In[lodLevel]; }
#endif	//__BANK
};

extern CLodScale g_LodScale;

#endif	//_SCENE_LOD_LODSCALE_H_