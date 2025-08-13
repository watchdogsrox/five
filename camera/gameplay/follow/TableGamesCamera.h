//
// camera/gameplay/follow/TableGamesCamera.h
// 
// Copyright (C) 1999-2019 Rockstar Games.  All Rights Reserved.
//
#ifndef TABLE_GAMES_CAMERA_H
#define TABLE_GAMES_CAMERA_H

#include "camera/gameplay/ThirdPersonCamera.h"

class camBaseObjectMetadata;
class camTableGamesCameraMetadata;

//////////////////////////////////////////////////////////////////////////
// camTableGamesCamera
//      Specialized third person camera to be used with table games in
//      the Casino (blackjack, roulette, etc)
//////////////////////////////////////////////////////////////////////////
class camTableGamesCamera : public camThirdPersonCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camTableGamesCamera, camThirdPersonCamera);
public:
	template <class _T> friend class camFactoryHelper;	//Allow the factory helper class to access the protected constructor.
protected:
	camTableGamesCamera(const camBaseObjectMetadata& metadata);
public:
	virtual ~camTableGamesCamera();
protected:
	virtual bool	Update() override;
	virtual void	UpdateBasePivotPosition() override;
	virtual float	UpdateOrbitDistance() override;
	virtual void	UpdateOrbitPitchLimits() override;
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch) override;
private:
	bool			ShouldUseSyncSceneAttachParentMatrix() const;
	void			ComputeAttachParentMatrixFromSnycScene(Matrix34& sceneMatrix) const;
private:
	NON_COPYABLE(camTableGamesCamera);
	const camTableGamesCameraMetadata& m_Metadata;
};

#endif //TABLE_GAMES_CAMERA_H