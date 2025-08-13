/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneParticleEffectEntity.h
// PURPOSE : The enitiy class is the interface between rage and the game side
//			 cut scene objects. For each object a entity is created, this 
//			 entity recieves events from the cut scene manager invokes the correct function
//			 on the game object. Cut scene objects have unique ids and are mapped to their
//			 entity which is stored in the manager.
//			 Particle effects entity: Passes the anim to the object
//			 Calls the update function.
// AUTHOR  : Thomas French
// STARTED : 13/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_PARTICLEFXENTITY_H 
#define CUTSCENE_PARTICLEFXENTITY_H

//Rage Headers

//Game Headers
#include "cutscene/CutSceneParticleEffectObject.h"

/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define cutsceneParticleEffectEntityDebugf(fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && GetCutfObject() ) { char debugStr[512]; CCutSceneParticleEffectsEntity::CommonDebugStr(GetCutfObject(), debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#else
#define cutsceneParticleEffectEntityDebugf(fmt,...) do {} while(false)
#endif //__BANK

class CCutSceneParticleEffectsEntity : public cutsUniqueEntity
{
public:	
	CCutSceneParticleEffectsEntity(const cutfObject* pObject);
	
	~CCutSceneParticleEffectsEntity();

	virtual void DispatchEvent(cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0 );

	CCutSceneParticleEffect* GetParticleEffect() { return  m_pParticleEffect;} 
	
	const CCutSceneParticleEffect* GetParticleEffect() const { return  m_pParticleEffect;} 
		
	s32 GetType() const { return CUTSCENE_PARTICLE_EFFECT_GAME_ENTITY; } 

#if __BANK
	virtual void DebugDraw()const;

	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif

	const cutfObject* GetCutfObject() const { return GetObject(); }

	

private:
	bool HasParticleEffectLoaded(cutsManager *pManager); 

private:
	CCutSceneParticleEffect* m_pParticleEffect;  

	bool  m_bIsLoaded;  

};


#if __BANK
#define cutsceneDecalEntityDebugf(fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && GetObject() ) { char debugStr[512]; CCutSceneDecalEntity::CommonDebugStr(GetObject(), debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#else
#define cutsceneDecalEntityDebugf(fmt,...) do {} while(false)
#endif //__BANK

class CCutSceneDecalEntity : public cutsUniqueEntity
{
public:	
	CCutSceneDecalEntity(const cutfObject* pObject);

	~CCutSceneDecalEntity();

	virtual void DispatchEvent(cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0 );

	s32 GetType() const { return CUTSCENE_DECAL_GAME_ENTITY; } 

	const cutfDecalObject* GetObject() const; 

#if __BANK
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
	void DebugDraw() const;
#endif

	static bool sm_DecalAddedThisFrame; 

private: 
	s32 m_DecalId; 
	float m_LifeTime; 

#if __BANK
	Matrix34 m_DebugDrawDecalMatrix; 
	Vector3 m_DebugBoxSize; 
#endif

};



#endif
