//
// camera/debug/DebugDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef DEBUG_DIRECTOR_H
#define DEBUG_DIRECTOR_H

#include "camera/base/BaseDirector.h"

class camBaseCamera;
class camDebugDirectorMetadata;
class camFreeCamera;

//The director responsible for the debug cameras.
class camDebugDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camDebugDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camDebugDirector(const camBaseObjectMetadata& metadata);

#if !__FINAL //No debug cameras in final builds.

public:
	~camDebugDirector();

	bool			IsFreeCamActive() const				{ return m_ActiveMode == DEBUG_CAMERA_MODE_FREE; }

	const camFrame&	GetFreeCamFrame() const				{ return GetCamFrame((camBaseCamera*)m_FreeCamera); }
	camFrame&		GetFreeCamFrameNonConst()			{ return GetCamFrameNonConst((camBaseCamera*)m_FreeCamera); }

	camFreeCamera*	GetFreeCam()						{ return (camFreeCamera*)m_FreeCamera.Get(); }	//This should be used sparingly.

	void			ActivateFreeCam()					{ m_ActiveMode = DEBUG_CAMERA_MODE_FREE; }
	void			DeactivateFreeCam()					{ m_ActiveMode = DEBUG_CAMERA_MODE_NONE; }
	void			SetFreeCamActiveState(bool state)	{ state ? ActivateFreeCam() : DeactivateFreeCam(); }

	void			SetShouldIgnoreDebugPadCameraToggle(bool state)	{ m_ShouldIgnoreDebugPadCameraToggle = state; }

#if __BANK
	virtual void	AddWidgetsForInstance();
	virtual void	DebugGetCameras(atArray<camBaseCamera*> &cameraList) const;
#endif // __BANK

	void			ActivateFreeCamAndCloneFrame();

private:
	virtual bool	Update();

	void			UpdateControl();
	camBaseCamera*	ComputeActiveCamera();

	const camFrame&	GetCamFrame(camBaseCamera* camera) const;
	camFrame&		GetCamFrameNonConst(camBaseCamera* camera);

	enum eDebugCameraModes
	{
		DEBUG_CAMERA_MODE_FREE,
		DEBUG_CAMERA_MODE_NONE,
		NUM_DEBUG_CAMERA_MODES
	};

	const camDebugDirectorMetadata& m_Metadata;

	RegdCamBaseCamera	m_FreeCamera;
	eDebugCameraModes	m_ActiveMode;
	bool				m_ShouldIgnoreDebugPadCameraToggle;
	bool				m_ShouldUseMouseControl;

#else // __FINAL - Stub out everything.

public:
	inline bool		IsFreeCamActive() const							{ return false;	}

	//These should never be called, but because they return references, use the base camera's scratch frame to avoid #if !__FINAL everywhere.
	const camFrame&	GetFreeCamFrame() const;
	camFrame&		GetFreeCamFrameNonConst();

	inline camFreeCamera* GetFreeCam()								{ return NULL; }

	inline void		ActivateFreeCam()								{}
	inline void		DeactivateFreeCam()								{}
	inline void		SetFreeCamActiveState(bool UNUSED_PARAM(state))	{}

	inline void		SetShouldIgnoreDebugPadCameraToggle(bool UNUSED_PARAM(state)) {};

private:
	virtual bool	Update() { return true; }

#endif //__FINAL

private:
	//Forbid copy construction and assignment.
	camDebugDirector(const camDebugDirector& other);
	camDebugDirector& operator=(const camDebugDirector& other);
};

#endif // DEBUG_DIRECTOR_H
