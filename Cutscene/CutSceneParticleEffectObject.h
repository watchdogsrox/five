
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneParticleObject.h
// PURPOSE : Header for the cutscene particle object, breaking up cutsceneobject.h 
// AUTHOR  : Thomas French
// STARTED : 03/11 /2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_PARTICLE_EFFECT_H
#define CUTSCENE_PARTICLE_EFFECT_H

//Rage includes

//Game includes
#include "CutScene/CutSceneAnimation.h"
#include "CutScene/CutSceneDefine.h"

class CCutSceneParticleEffect
{
public:
	CCutSceneParticleEffect();

	virtual ~CCutSceneParticleEffect();

	virtual s32 GetType()=0; 
	
	virtual void Update(cutsManager* pManager)=0; 

#if __BANK
	void DisplayDebugInfo(const char* pName) const;

	Vector3 GetEffectPosition() const { return m_mParticleEffect.d; } 
	void SetEffectOverridePosition(const Vector3& vPos) { m_vOverridenParticleEffectPos = vPos; }
	
	Vector3 GetEffectRotation() const;
	void SetEffectOverrideRotation(const Vector3& vRot){ m_vOverridenParticleEffectRot = vRot; }

	void SetCanOverrideEffect(bool bOverride) { m_bCanOverrideEffect = bOverride; }
	bool CanOverrideEffect() const { return m_bCanOverrideEffect; }
#endif
	
	bool IsActive() const { return m_IsActive; }
	
	void SetIsActive(bool CanActivate) { m_IsActive = CanActivate; } 
	
	void SetAttachParentAndBoneAttach(s32 attachParent, u16 boneHash) { m_AttachParent = attachParent; m_BoneHash = boneHash; }

protected:
	s32 GetAttachParentsBoneIndex(cutsManager* pManager, u16 BoneHash, u32 AttachParentId); 

	CEntity* GetAttachParent(cutsManager* pManager, u32 AttachParentId); 
	
#if __BANK
	bool m_bCanOverrideEffect; 
	Vector3 m_vOverridenParticleEffectPos;
	Vector3 m_vOverridenParticleEffectRot; 
	Matrix34 m_mParticleEffectWorld; 
#endif

protected:
	Matrix34 m_mParticleEffect; 
	
	//Hash of the particle name
	atHashWithStringNotFinal m_EffectRuleHashName;
	atHashWithStringNotFinal m_EffectListHashName; 
	
	s32 m_iEffectId;

	bool m_IsActive; 

	s32 m_AttachParent; 
	u16 m_BoneHash; 
};

class CCutSceneAnimatedParticleEffect : public CCutSceneParticleEffect
{
public:
	CCutSceneAnimatedParticleEffect(const cutfAnimatedParticleEffectObject* pObject); 
	
	~CCutSceneAnimatedParticleEffect(); 
	
	void Update(cutsManager* pManager); //Update a particle effect that evolves over time.

	//Gets the light animation
	CCutsceneAnimation& GetCutSceneAnimation() { return m_Animation; } 
	const CCutsceneAnimation& GetCutSceneAnimation() const { return m_Animation; } 
	
	s32 GetType() { return CUTSCENE_ANIMATED_PARTICLE_EFFECT; }
	
	void StopParticleEffect(const cutsManager* pManager); 
	
private:
	CCutsceneAnimation m_Animation;
	
	const cutfAnimatedParticleEffectObject* m_pCutfObject; 	
};

class CCutSceneTriggeredParticleEffect : public CCutSceneParticleEffect
{
public:
	CCutSceneTriggeredParticleEffect(const cutfParticleEffectObject* pEffect);
	~CCutSceneTriggeredParticleEffect();
	
	void Update(cutsManager* pManager); //Trigger a single particle effect that does not evolve over time.
	
	s32 GetType() { return CUTSCENE_TRIGGERED_PARTICLE_EFFECT; }
	
	void SetInitialPosition(const Vector3& pos){ m_Position = pos;}
	void SetInitialRotation(const Quaternion& rot) {m_InitialRotation = rot;}  

private:
	Quaternion m_InitialRotation; 
	Vector3 m_Position; 

	const cutfParticleEffectObject* m_pCutfObject; 
};


#endif