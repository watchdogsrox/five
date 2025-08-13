//
// camera/marketing/MarketingDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_DIRECTOR_H
#define MARKETING_DIRECTOR_H

#include "camera/base/BaseDirector.h"
#include "camera/helpers/HandShaker.h"

class camMarketingDirectorMetadata;

//The director responsible for the marketing cameras.
class camMarketingDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camMarketingDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camMarketingDirector(const camBaseObjectMetadata& metadata);

#if __BANK //Marketing tools are only available in BANK builds.

public:
	~camMarketingDirector();

	bool			IsCameraActive() const							{ return m_IsActive && m_IsCameraActive; }

	bool			ShouldUseRdrControlMapping() const				{ return m_ShouldUseRdrControlMapping; }
	void 			SetShouldUseRdrControlMapping(bool shouldUse)	{ m_ShouldUseRdrControlMapping = shouldUse; }

	void			DebugSetOverrideMotionBlurThisUpdate(const float blurStrength);

	virtual void	AddWidgetsForInstance();
	virtual void	DebugGetCameras(atArray<camBaseCamera*> &cameraList) const;

private:
	virtual bool	Update();

	void			UpdateControl();
	void			UpdateMotionBlur(camFrame& destinationFrame);

    void            InitMarketingCameras();
    void            ShutdownMarketingCameras();

	const camMarketingDirectorMetadata& m_Metadata;

	camHandShaker				m_Shaker;
	atArray<RegdCamBaseCamera>	m_Cameras;
	atArray<const char*>		m_TextLabels;
	s32							m_ModeIndex;
	float						m_ShakerScaleAngle;
	float						m_DebugOverrideMotionBlurStrengthThisUpdate;
	bool						m_IsActive			: 1;
	bool						m_IsCameraActive	: 1;
	bool						m_ShouldUseRdrControlMapping;
	bool						m_IsShakerEnabled;
    bool                        m_MarketingCamerasInitialised;

#else // !__BANK - Stub out everything.

public:
	bool			IsCameraActive() const { return false; }

private:
	virtual bool	Update() { return true; }

#endif //__BANK

private:
	//Forbid copy construction and assignment.
	camMarketingDirector(const camMarketingDirector& other);
	camMarketingDirector& operator=(const camMarketingDirector& other);
};

#endif // MARKETING_DIRECTOR_H
