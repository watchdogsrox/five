#ifndef BASE_CINEMATIC_CONTEXT_H
#define BASE_CINEMATIC_CONTEXT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"
#include "camera/helpers/Envelope.h"
#include "atl/queue.h"
#include "atl/vector.h"
class camCinematicContextMetadata;

enum CameraSelectorStatus
{
	CSS_CAN_CHANGE_CAMERA,
	CSS_MUST_CHANGE_CAMERA,
	CSS_DONT_CHANGE_CAMERA,
	CSS_MAX_NUM_STATUS
};

enum CameraSelectorController
{
	UseFallbackShots, 
	DontUseSubsequentShots,
	CanUseSubsequentShots, 
	OverrideShot, 
	CanUseSubsequentShotsWithDifferentSubModes, 
	DontUseSubsequentShotsBasedOnFrequency,
	DontUseSubsequentShotsNotBasedFrequency,
};

#define CINEMATIC_SHOT_HISTORY_LENGTH (20)

class camBaseCinematicContext : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseCinematicContext, camBaseObject);
public:
	friend class camCinematicDirector; 
	
	~camBaseCinematicContext(); 

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const  = 0; 
	
	const camBaseCinematicShot* GetCurrentShot() const; 
	
	s32 GetShotIndex(const u32 hash) const; 

	float GetShotInitalSlowValue() const; 
	
	u32 GetShotSlowMotionBlendOutDuration() const; 

	bool CanBlendOutSlowMotionOnShotTermination() const; 

	bool IsInitialSlowMoValueValid(float slowMo)const; 

	const camBaseCamera* GetCurrentShotsCamera() { return GetCamera(); }
	
	bool IsUsingGameCameraAsFallBack() const { return m_IsRunningGameCameraAsFallBack; }

protected:
	virtual bool Update(); 
	
	virtual void PreUpdate() {;} 

	CameraSelectorStatus CanUpdate(); 
	
	virtual void ClearContext(); 
	
	bool CanAbortOtherContexts() const  { return m_Metadata.m_CanAbortOtherContexts; } 
	
	virtual bool UpdateContext(); 

	s32 SelectShot(bool canUseSameShotTwice); 
	
	bool IsCameraValid(); 
	
	camBaseCamera* GetCamera(); 
	
	virtual bool UpdateShotSelector(); 
	
	CameraSelectorStatus GetChangeShotStatus() const; 
	
	void SetShotStartTimes(u32 Time);

	camBaseCinematicShot* GetCurrentShot(); 

	virtual bool CanActivateUsingQuickToggle() { return m_Metadata.m_CanActivateUsingQuickToggle; }

	void PreUpdateShots(); 

	u32 GetCurrentShotDuration() { return fwTimer::GetTimeInMilliseconds() - m_PreviousCameraChangeTime; }
	
	float GetMinShotScalar() const  { return m_Metadata.m_MinShotTimeScalarForAbortingContexts; }
	
	u32 GetNumberOfPrefferedShotsForInputType(u32 InputType); 
	
	void GetShotHashesForInputType(u32 InputType, atArray<u32>& Shots) const; 

	void CreateRandomIndexArray(u32 SizeOfArray, atArray<u32>& RandomIndexs);
private:
	u32 ComputeNumOfSelectableShots(); 
	
	void ComputeShotTargets(); 

	void ClearUnusedShots(s32 ActiveShot); 

	void ClearAllShots(); 

	bool ComputeShouldChangeForHigherPriorityShot(CameraSelectorStatus &currentStatus); 

	bool TryAndCreateANewCamera(s32 shotIndex); 
protected:
	camBaseCinematicContext(const camBaseObjectMetadata& metadata);

	atVector<camBaseCinematicShot*> m_Shots; 
	atVector<u32> m_ListOfCamerasAttemptedToCreate; 
	
	//static atVector<u32> m_ShotHistory; 
	static atQueue<u32, CINEMATIC_SHOT_HISTORY_LENGTH> m_ShotHistory; 
	u32 m_PreviousCameraChangeTime;
	u32 m_NumSelectableShots; 
	s32	m_CurrentShotIndex; 

	CameraSelectorController m_SelectorControl; 
	
	bool m_IsRunningGameCameraAsFallBack : 1; 

private:
	struct AvalibleShotsToSelect
	{
		s32 ShotIndex; 
		s32 ShotApperances; 
	};

	const camCinematicContextMetadata&	m_Metadata;
};


#endif
