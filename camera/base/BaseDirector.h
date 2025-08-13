//
// camera/base/BaseDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_DIRECTOR_H
#define BASE_DIRECTOR_H

#include "fwsys/timer.h"

#include "camera/base/BaseObject.h"
#include "camera/helpers/Frame.h"
#include "scene/RegdRefTypes.h"

class camBaseDirectorMetadata;
class camFrameInterpolator;

//The base director class that describes the basic interface.
class camBaseDirector : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseDirector, camBaseObject);

protected:
	camBaseDirector(const camBaseObjectMetadata& metadata);

public:
	enum eRenderStates
	{
		RS_INTERPOLATING_IN,
		RS_FULLY_RENDERING,
		RS_INTERPOLATING_OUT,
		RS_NOT_RENDERING,
		RS_INVALID
	};

	virtual ~camBaseDirector();

	const camFrame&	GetFrame() const					{ return m_PostEffectFrame ; }
	const camFrame& GetFrameNoPostEffects() const		{ return m_Frame; }

	const camBaseCamera* GetRenderedCamera() const	{ return m_RenderedCamera.Get(); }
	camBaseCamera*	GetRenderedCamera()				{ return m_RenderedCamera.Get(); }

	bool			IsRendering() const				{ return (!m_IsRenderingBypassed && (m_ActiveRenderState != RS_NOT_RENDERING)); }
	bool			IsInterpolating() const			{ return (!m_IsRenderingBypassed && ((m_ActiveRenderState == RS_INTERPOLATING_IN) ||
														(m_ActiveRenderState == RS_INTERPOLATING_OUT))); }
	eRenderStates	GetRenderState() const			{ return (m_IsRenderingBypassed ? RS_NOT_RENDERING : m_ActiveRenderState); }
	float 			GetInterpolationBlendLevel() const;
	s32				GetInterpolationTime() const;

	virtual void	Render(u32 interpolationDuration = 0, bool shouldLockInterpolationSourceFrame = false);
	virtual void	StopRendering(u32 interpolationDuration = 0, bool shouldLockInterpolationSourceFrame = false);

	void			BypassRenderingThisUpdate()		{ m_ShouldBypassRenderingThisUpdate = true; }

	bool			BaseUpdate(camFrame& destinationFrame);
#if __BANK
	virtual void	DebugGetCameras(atArray<camBaseCamera*>& cameraList) const;
	virtual void	DebugRender() const				{ }
#endif // __BANK

	bool			Shake(const char* shakeName, float amplitude = 1.0f);
	bool			Shake(const char* animDictionary, const char* animClip, const char* metadataName, float amplitude = 1.0f);
	bool			IsShaking() const;
	bool			SetShakeAmplitude(float amplitude = 1.0f);
	void			StopShaking(bool shouldStopImmediately = true);
	camBaseFrameShaker* GetFrameShaker() const { return m_FrameShaker; }

	void			RegisterWeaponFire(const CWeapon& weapon, const CEntity& parentEntity, const bool isNetworkClone = false);

	void			SetHighQualityDofThisUpdate(bool enable)	{ m_ShouldForceHighQualityDofThisUpdate = enable; }
	
	bool			IsForcingHighQualityDofThisUpdate() const { return m_ShouldForceHighQualityDofThisUpdate; }

private:
	virtual void	RegisterWeaponFireLocal(const CWeapon& UNUSED_PARAM(weapon), const CEntity& UNUSED_PARAM(firingEntity)) {};
	virtual void	RegisterWeaponFireRemote(const CWeapon& UNUSED_PARAM(weapon), const CEntity& UNUSED_PARAM(firingEntity)) {};

protected:
	virtual bool	Update() = 0;
	virtual void	PostUpdate(); 
	virtual void	UpdateActiveRenderState();
	virtual void	UpdatePostRender() {}

	inline virtual u32 GetTimeInMilliseconds() const { return fwTimer::GetCamTimeInMilliseconds(); }

	void			BaseReset();

	camFrame				m_Frame;
	camFrame				m_CachedInitialDestinationFrame;
	camFrame				m_PostEffectFrame;  
	RegdCamBaseCamera		m_UpdatedCamera;
	RegdCamBaseCamera		m_RenderedCamera;
	RegdCamBaseFrameShaker	m_FrameShaker;
	camFrameInterpolator*	m_FrameInterpolator;
	eRenderStates			m_ActiveRenderState;
	bool					m_ShouldLockInterpolationSourceFrame;
	bool					m_ShouldForceHighQualityDofThisUpdate;
	bool					m_ShouldBypassRenderingThisUpdate;
	bool					m_IsRenderingBypassed;

	const camBaseDirectorMetadata& m_Metadata;

private:
	//Forbid copy construction and assignment.
	camBaseDirector(const camBaseDirector& other);
	camBaseDirector& operator=(const camBaseDirector& other);
};

#endif // BASE_DIRECTOR_H
