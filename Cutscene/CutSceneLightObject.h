
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneLightObject.h
// PURPOSE : Header for the cutscene light object, breaking up cut scene object. 
// AUTHOR  : Thomas French
// STARTED : 03/11 /2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_LIGHT_OBJECT_H
#define CUTSCENE_LIGHT_OBJECT_H

//Rage includes

//Game includes
#include "CutScene/CutSceneAnimation.h"
#include "CutScene/CutSceneDefine.h"
#include "CutScene/cutsmanager.h"
#include "cutfile/cutfobject.h"
#include "debug/debugscene.h"
/////////////////////////////////////////////////////////////////////////////////

class CEntity; 

class CCutSceneLight
{
public:
	
	CCutSceneLight(const cutfLightObject* pLightObject);
	~CCutSceneLight();

	//Update the light with the given anim
	void UpdateLight(cutsManager* pManager);

	//Gets the light animation
	CCutsceneAnimation& GetCutSceneAnimation() { return m_Animation; } 
	const CCutsceneAnimation& GetCutSceneAnimation() const { return m_Animation; } 

	const cutfLightObject* GetLightCutObject() const { return m_pCutfLightObj; }

	void SetCanUpdateStaticLight(bool bUpdate) { m_bUpdateStaticLights = bUpdate;} 
	bool CanUpdateStaticLight() const { return m_bUpdateStaticLights; }

	void SetAttachParentId(s32 parentId) { m_AttachParentId = parentId; }
	void SetAttachBoneHash(u16 BoneHash) { m_AttachBoneHash = BoneHash; }
#if __BANK
	void SetLightInfo (SEditCutfLightInfo* pLightInfo) { m_pEditLightInfo = pLightInfo; }
	const SEditCutfLightInfo* GetLightInfo() const { return m_pEditLightInfo;  }

	void OverrideLights (Vector3 &vLightDirection, Vector3 &vLightColor, float &fLightFallOff, float &fLightConeAngle, float &lightIntensity, u32 &LightFlags, u32 &TimeFlags);  
	
	void  DisplayDebugLightinfo(const char* pName, bool isStatic, float gizmoScale = 1.0f) const; 

	void OverrideVolumeSettings(float &VolumeIntensity, float &VolumeSize,  Vector4& OuterColourAndIntensity); 

	void OverrideCoronaSettings(float &fCoronaIntensity, float &fCoronaSize, float &fCoronaZbias); 

	void OverrideExponetiolFallOff(float &fExponentiolFallOff); 

	void OverrideInnerConeAngle(float &fInnerConeAngle);

	bool UpdateLightDebugging(cutsManager* pManager, const Matrix34& ParentMat, const char* Label); 

	Matrix34& GetLightMatWorld() { return m_lightMatWorld; }
#endif

private:
	//Sets the property of the lights, if it cast shadows, environment etc
	void SetLightProperty(); 

	float CalulateIntensityBasedOnHourFlags(u32 TimeFlags); 
	
	CEntity* GetAttachParent(cutsManager* pManager, u32 AttachParentId); 
	
	s32 GetAttachParentsBoneIndex(cutsManager* pManager, u16 BoneHash, u32 AttachParentId); 

private:
	//Animation played on the light used to extract intensity and brightness 
	CCutsceneAnimation m_Animation;

	//Store a pointer to our rage cutf object
	const cutfLightObject* m_pCutfLightObj;
		
	bool m_bUpdateStaticLights : 1; 

	s32 m_AttachParentId; 
	u16 m_AttachBoneHash; 

#if __BANK
	CAuthoringHelper m_Helper; 
	SEditCutfLightInfo *m_pEditLightInfo;
	Matrix34 m_vRenderLightMat; 
	Matrix34 m_lightMatWorld; 
	float m_DebugIntensity; 
#endif

};

#endif