/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsceneObject.h
// PURPOSE : header for CutsceneObject.cpp
// AUTHOR  : Derek Payne / Thomas French
// STARTED : 09/02/2006
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENEOBJECT_H
#define CUTSCENEOBJECT_H

// rage headers:
#include "vector/vector3.h"

// game headers:

#include "cutscene/CutSceneDefine.h"
#include "Objects/Object.h"
#include "animation/AnimBones.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedProps.h"

enum eCUTSCENE_OBJECT_TYPE
{
	CUTSCENE_OBJECT_UNDEFINED,
	CUTSCENE_OBJECT_TYPE_VEHICLE,
	CUTSCENE_OBJECT_TYPE_OBJECT,
	CUTSCENE_OBJECT_TYPE_LIGHT,
	CUTSCENE_OBJECT_TYPE_PARTICLE_EFFECT
};

////////////////////////////////////////////////////////////////////////////////////
//Cut Scene Object: Virtual base class for all model related cut scene objects
////////////////////////////////////////////////////////////////////////////////////

class CCutsceneAnimatedModelEntity; 

//Possible manager base calls to get access to our cutscene units
class CCutSceneObject : public CObject
{
public:	
	CCutSceneObject(const eEntityOwnedBy ownedBy); 
	~CCutSceneObject(); 

	virtual void Initialise(u32 iModelIndex, const Vector3& pos) = 0; 
	
	// Get entity type		

	//mmm legacy don't know if needed?
	float	fBoundingRadius; //sorry we need to pass the SPU the address of this member so it can DMA it over

	virtual fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);

	//Shaders
	CCustomShaderEffectBaseType* GetCustomShaderEffectType() const {return m_pShaderEffectType;}

protected:
	//Store where the creature was created
	Vector3 m_CreatePos; 
private:
	Vector3 vBoundingBoxMin;
	Vector3 vBoundingBoxMax;

protected:
	
	CCustomShaderEffectBase*		m_pShaderEffect;
	CCustomShaderEffectBaseType*	m_pShaderEffectType;
	eCUTSCENE_OBJECT_TYPE		m_type;
	s32						m_iAnimDict;

	void SetIsInUse(bool bIsInUse) { m_bInUse = bIsInUse; }
	bool GetIsInUse() { return m_bInUse; }

	//	Anim
	bool m_bAnimated;

private:
	bool m_bInUse;
	bool m_bUsingMoverTrack;

};

////////////////////////////////////////////////////////////////////////////////////
//Cut Scene Prop: Cut scene object that is just a prop or model
////////////////////////////////////////////////////////////////////////////////////

class CCutSceneProp : public CCutSceneObject 
{
public:	
	CCutSceneProp(const eEntityOwnedBy ownedBy, u32 ModelIndex, const Vector3& vPos);
	~CCutSceneProp();
	
	virtual void Initialise(u32 iModelIndex, const Vector3& pos);

//	Set the entity as an object
	void SetAsCutsceneObject();	

private:

};

////////////////////////////////////////////////////////////////////////////////////
//Cut Scene Vehicle: A cut scene vehicle, usually requires custom shaders and data. 
////////////////////////////////////////////////////////////////////////////////////

class CCutSceneVehicle : public CCutSceneObject
{
public:
	CCutSceneVehicle(const eEntityOwnedBy ownedBy, u32 ModelIndex, const Vector3& vPos);
	~CCutSceneVehicle();

	//set up the cut scene vehicle
	virtual void Initialise(u32 iModelIndex, const Vector3& pos);

	//	Vehicle Model
	bool IsVehicleModel();

	//	type checking done need possibly can check on object pointer type
	void SetAsCutsceneVehicle();

	//Get a pointer to the info about this vehicle
	CVehicleModelInfo* GetVehicleModelInfo();  

	//	Vehicle Lights
#if 0	
	void DoHeadLightsEffect(s32 BoneLeft, s32 BoneRight, bool leftOn, bool rightOn, float fade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool bExtraBright);
	void DoTailLightsEffect(s32 BoneLeft, s32 BoneMiddle, s32 BoneRight, bool leftOn, bool middleOn, bool rightOn, float fade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool isBrakes);
#endif

	//	Vehicle dirt
	float GetBodyDirtLevel() { return(nBodyDirtLevel); }
	u8 GetBodyDirtLevelUInt8() { return static_cast<u8>( static_cast<u32>(this->nBodyDirtLevel)&0x0F ); }
	void SetBodyDirtLevel(float dirtLevel) { nBodyDirtLevel = dirtLevel; }
	Color32 GetBodyDirtColor() const { return m_bodyDirtColor; }
	void SetBodyDirtColor(Color32 col) { m_bodyDirtColor = col; }

	//	Vehicle Colours
	bool HasVehicleBeenColoured() { return bVehicleHasBeenColoured; }
	inline u8	GetBodyColour1()		{ return(m_colour1); }
	inline u8	GetBodyColour2()		{ return(m_colour2); }
	inline u8	GetBodyColour3()		{ return(m_colour3); }
	inline u8	GetBodyColour4()		{ return(m_colour4); }
	inline void		SetBodyColour1(u8 c)	{ m_colour1 = c; }
	inline void		SetBodyColour2(u8 c)	{ m_colour2 = c; }
	inline void		SetBodyColour3(u8 c)	{ m_colour3 = c; }
	inline void		SetBodyColour4(u8 c)	{ m_colour4 = c; }
	inline s8		GetLiveryIdx()			{ return(m_liveryIdx); }	
	inline void		SetLiveryIdx(s8 idx)	{ m_liveryIdx = idx; }	
	
	void UpdateBodyColourRemapping();

	//	Vehicle Wheels
	void FixupVehicleWheelMatrix(eHierarchyId nWheelId, CVehicleModelInfo* pModelInfo);
	
	//	Vehicle Components
	void GetRemovedFragments (atArray<s32> &RemovedFragments); 
	void HideFragmentComponent(s32 iBoneId);
	void SetComponentToHide(const atArray<s32> &RemovedFragments); 
	void SetComponentToHide(s32 iBoneId, bool bHide);	

#if __DEV
	//Remove individual bone from list for debugging
	void RemoveFragmentFromHideList(s32 iBoneId);

#endif

#if 0
	void GetDefaultBonePositionSimple(s32 boneID, Vector3& vecResult) const;
#endif

	//Vehicle skeleton Access
	const crSkeleton* GetVehicleSkeleton(); 

	//Render
	virtual fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);
	virtual ePrerenderStatus PreRender();
	virtual void PreRender2();
	
private:
	//int m_type;
	// car colours:
	u8 m_colour1;
	u8 m_colour2;
	u8 m_colour3;
	u8 m_colour4;
	bool bVehicleHasBeenColoured;
	s8 m_liveryIdx;
	float nBodyDirtLevel;				// Dirt level of vehicle body texture: 0.0f=fully clean, 15.0f=maximum dirt visible, it may be altered at any time while vehicle's cycle of lige
	Color32 m_bodyDirtColor;
	atArray<s32> m_iBoneOfComponentToRemove; //list of components to remove
};

#endif 

