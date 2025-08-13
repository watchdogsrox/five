//
// camera/marketing/MarketingAToBCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_A_TO_B_CAMERA_H
#define MARKETING_A_TO_B_CAMERA_H

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingFreeCamera.h"
#include "scene/RegdRefTypes.h"

class camMarketingAToBCameraMetadata;

//A marketing camera that smoothly blends between a series of key-frames.
class camMarketingAToBCamera : public camMarketingFreeCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camMarketingAToBCamera, camMarketingFreeCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camMarketingAToBCamera(const camBaseObjectMetadata& metadata);

public:
	~camMarketingAToBCamera();

	virtual void	AddWidgetsForInstance();

private:
	virtual bool	Update();
	virtual void	UpdateMiscControl(CPad& pad);

	void			ClearAllSplineNodes();
	void			OnOverallSmoothingChange();
	void			OnSmoothingChange();
	void			OnTransitionTimeChange();

	const camMarketingAToBCameraMetadata& m_Metadata;

	RegdCamBaseCamera m_SplineCamera;
	bkGroup*		m_SplineNodeWidgetGroup;
	float*			m_SplineTransitionTimes;
	u32				m_MaxSplineNodes;
	s32				m_OverallSmoothingStyle;
	bool			m_IsSplineActive;
	bool			m_ShouldUseFreeLookAroundMode;
	bool			m_ShouldSmoothRotation;
	bool			m_ShouldSmoothLensParameters;

	//Forbid copy construction and assignment.
	camMarketingAToBCamera(const camMarketingAToBCamera& other);
	camMarketingAToBCamera& operator=(const camMarketingAToBCamera& other);
};

#endif // __BANK

#endif // MARKETING_A_TO_B_CAMERA_H
