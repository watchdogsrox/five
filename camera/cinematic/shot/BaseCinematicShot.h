#ifndef BASE_CINEMATIC_SHOT_H
#define BASE_CINEMATIC_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/system/CameraMetadata.h"
#include "camera/base/BaseCamera.h"
#include "camera/system/CameraFactory.h"
#include "atl/vector.h"
#include "fwsys/timer.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"

class camBaseCinematicContext; 

class camBaseCinematicShot : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseCinematicShot, camBaseObject);
public:
	friend class camBaseCinematicContext; 
	friend class camCinematicScriptContext; 

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void Init(camBaseCinematicContext* pContext, u32 Priority, float Probability); 
	
	virtual void PreUpdate(bool UNUSED_PARAM(IsCurrentShot) = true) {;} 

	virtual bool IsValid() const;   
	
	const camCinematicShotMetadata&	m_Metadata;

	camBaseCamera* GetCamera() const { return m_UpdatedCamera; }
	
	const CEntity* GetAttachEntity() const { return m_AttachEntity; }

	const CEntity* GetLookAtEntity() const { return m_LookAtEntity; }
	
	void SetAttachEntity(const CEntity* pEntity); 

	void SetLookAtEntity(const CEntity* pEntity); 
	
	const camBaseCinematicContext* GetContext() const { return m_pContext; }

	float GetMinimumShotDuration() const { return m_Metadata.m_ShotDurationLimits.x; }
	
	float GetMinShotScalar() const  { return m_Metadata.m_MinShotTimeScalarForAbortingShots; }

protected:
	virtual bool IsValidForGameState() const;

	virtual void InitCamera(){;}
	
	virtual bool CreateAndTestCamera(u32 hash);

	virtual u32 GetPriority() const { return m_Priority; } 
	
	virtual float GetProbability() const { return m_ProbabilityWeighting; }

	virtual bool CanCreate() ; 
	
	virtual const CEntity* ComputeAttachEntity() const;
	virtual const CEntity* ComputeLookAtEntity() const; 

	virtual void Update() {;}

	void SetAttachEntity(); 
	void SetLookAtEntity(); 

	bool IsFollowPedTargetUnderWater() const; 

	virtual void ClearShot(bool ResetCameraEndTime = false); 
	
	void ResetShot(); 

	void DeleteCamera(bool ResetCameraEndTime = false);
	
	virtual u32 GetCameraRef() const { return m_Metadata.m_CameraRef.GetHash(); }

	void SetShotStartTime(u32 timeOflastShot) { m_ShotStartTime = timeOflastShot; }
	
	void SetShotEndTime(u32 timeOflastShot) { m_ShotEndTime = timeOflastShot; }
	
	bool IsValidForVehicleType(const CVehicle* pVehicle) const ; 

	bool IsValidForCustomVehicleSettings(const CVehicle* pVehilce ) const; 

protected:
	camBaseCinematicShot(const camBaseObjectMetadata& metadata);
	
	RegdCamBaseCamera m_UpdatedCamera;
	RegdConstEnt	m_AttachEntity;
	RegdConstEnt	m_LookAtEntity; 
	camBaseCinematicContext* m_pContext; 
	u32 m_CurrentMode; 
	u32 m_ShotStartTime; 
	u32 m_ShotEndTime; 
	u32 m_Priority; 
	float m_ProbabilityWeighting; 
private:
	
};


#endif