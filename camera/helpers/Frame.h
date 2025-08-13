//
// camera/helpers/Frame.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_FRAME_H
#define CAMERA_FRAME_H

#include "spatialdata/aabb.h"
#include "vector/matrix34.h"

#include "fwutil/Flags.h"

#include "camera/camera_channel.h"
#include "scene/RegdRefTypes.h"

#include "renderer/PostFX_shared.h"

#include "grcore/config.h"

const float g_MinFov				= 1.0f;		//Degrees.
const float g_MaxFov				= 130.0f;	//Degrees.
const float g_DefaultFov			= 45.0f;	//Keep updated with g_DefaultTanHalfFov in ViewportManager.cpp!
const float g_DefaultFovRecip		= 1.0f / g_DefaultFov;

const float g_MinNearClip			= 0.01f;
const float g_MaxNearClip			= 0.5f;
const float g_DefaultNearClip		= 0.15f;

const float g_DefaultFarClip		= 800.0f;

const float g_DefaultNearInFocusDofPlane					= 0.1f;
const float g_DefaultFarInFocusDofPlane						= 150.0f;
const float g_DefaultDofStrength							= 0.5f;
const float g_MinDofBlurDiskRadius							= 1.0f;
const float g_MaxDofBlurDiskRadius							= 15.0f;
const float g_MaxAdaptiveDofPlaneDistance					= 10000.0f;
const float g_CircleOfConfusionFor35mm						= 0.000029f;
const float g_HeightOf35mmFullFrame							= 0.024f;

const float g_MaxMotionBlurStrength	= 100.0f;

// const float g_MinAbsPitchDeltaFromPolesToAvoidSingularity = 0.1f * DtoR;

extern const spdAABB g_CameraWorldLimits_AABB;
extern const Vector3 g_InvalidPosition;
extern const Vector3 g_InvalidRelativePosition;
extern const Vector3 g_VeryCloseToIdentityVector;

class camDepthOfFieldSettingsMetadata;

namespace rage
{
	class fiStream;
	class bkBank;
	class Color32;
};

//A frame is a representation of a camera's position, orientation and lens parameters.
//NOTE: We inherit from CRefAwareBase in order to use safe references.
class camFrame : public fwRefAwareBase
{
public:
	enum eFlags
	{
		Flag_ShouldUseHighQualityDof_Unused		= BIT0,	//Deprecated.
		Flag_ShouldUseLightDof					= BIT1, //If set, the high-quality depth of field effect will be configured for light DOF, with a weak blur.
		Flag_ShouldUseShallowDof				= BIT2,	//If set, the high-quality depth of field effect will be configured for shallow DOF, with a strong blur.
		Flag_ShouldUseRawDofPlanes				= BIT3,	//If set, the four depth of field planes will be applied directly, rather than the out-of-focus planes being derived from the in-focus planes and the DOF strength.
		Flag_ShouldOverrideStreamingFocus		= BIT4,	//If set, the position and velocity of this frame, when rendered, will be used to override the game streaming focus.
		Flag_ShouldIgnoreTimeCycleFarClip		= BIT5,	//If set, the far-clip specified by the time-cycle will be ignored, should this frame be rendered.
		Flag_HasCutPosition						= BIT6,
		Flag_HasCutOrientation					= BIT7, 
		Flag_ShouldUseSimpleDof					= BIT8,
		Flag_RequestPortalRescan				= BIT9,
		Flag_BypassNearClipScanner				= BIT10,
		Flag_ShouldOverrideDofBlurDiskRadius	= BIT11,
		Flag_ShouldLockAdaptiveDofFocusDistance	= BIT12,
		Flag_ShouldMeasurePostAlphaPixelDepth	= BIT13
	};

	enum eComponentType 
	{
		CAM_FRAME_COMPONENT_BAD = -1,
		CAM_FRAME_COMPONENT_MATRIX,
		CAM_FRAME_COMPONENT_POS,
		CAM_FRAME_COMPONENT_FRUST,
		CAM_FRAME_COMPONENT_NEARCLIP,
		CAM_FRAME_COMPONENT_FARCLIP,
		CAM_FRAME_COMPONENT_DOF,
		CAM_FRAME_COMPONENT_FOV,
		CAM_FRAME_COMPONENT_MOTION_BLUR,
		CAM_FRAME_COMPONENT_HISTORY,
		CAM_FRAME_MAX_COMPONENTS
	};

	enum eDofPlane
	{
		NEAR_OUT_OF_FOCUS_DOF_PLANE = 0,
		NEAR_IN_FOCUS_DOF_PLANE,
		FAR_IN_FOCUS_DOF_PLANE,
		FAR_OUT_OF_FOCUS_DOF_PLANE,
		NUM_DOF_PLANES
	};

	camFrame();
	camFrame(const camFrame& sourceFrame)
	{
		CloneFrom(sourceFrame);
	}

	camFrame &operator=( const camFrame &sourceFrame)
	{
		CloneFrom(sourceFrame);
		return *this;
	}

	~camFrame() {}

	inline const Matrix34&	GetWorldMatrix() const	{ return m_WorldMatrix; }
	inline const Vector3&	GetPosition() const		{ return m_WorldMatrix.d; }
	inline const Vector3&	GetFront() const		{ return m_WorldMatrix.b; }
	inline const Vector3&	GetRight() const		{ return m_WorldMatrix.a; }
	inline const Vector3&	GetUp() const			{ return m_WorldMatrix.c; }

	inline float	GetFov() const							{ return m_Fov; }
	inline float	GetNearClip() const						{ return m_NearClip; }
	inline float	GetFarClip() const						{ return m_FarClip; }
	inline float	GetCameraTimeScaleThisUpdate() const	{ return m_CameraTimeScaleThisUpdate; }

	inline float	GetNearInFocusDofPlane() const						{ return m_DofPlanes[NEAR_IN_FOCUS_DOF_PLANE]; }
	inline float	GetFarInFocusDofPlane() const						{ return m_DofPlanes[FAR_IN_FOCUS_DOF_PLANE]; }
	inline float	GetDofStrength() const								{ return m_DofStrength; }
	inline float	GetDofBlurDiskRadius() const						{ return m_DofBlurDiskRadius; }
	inline float	GetDofMaxPixelDepth() const							{ return m_DofMaxPixelDepth; }
	inline float	GetDofMaxPixelDepthBlendLevel() const				{ return m_DofMaxPixelDepthBlendLevel; }
	inline float	GetDofPixelDepthPowerFactor() const					{ return m_DofPixelDepthPowerFactor; }
	inline float	GetFNumberOfLens() const							{ return m_FNumberOfLens; }
	inline float	GetFocalLengthMultiplier() const					{ return m_FocalLengthMultiplier; }
	inline const Vector2& GetDofFocusDistanceGridScaling() const		{ return m_DofFocusDistanceGridScaling; }
	inline const Vector2& GetDofSubjectMagPowerNearFar() const			{ return m_DofSubjectMagPowerNearFar; }
	inline float	GetDofFStopsAtMaxExposure() const					{ return m_DofFStopsAtMaxExposure; }
	inline float	GetDofFocusDistanceBias() const						{ return m_DofFocusDistanceBias; }
	inline float	GetDofMaxNearInFocusDistance() const				{ return m_DofMaxNearInFocusDistance; }
	inline float	GetDofMaxNearInFocusDistanceBlendLevel() const		{ return m_DofMaxNearInFocusDistanceBlendLevel; }
	inline float	GetDofMaxBlurRadiusAtNearInFocusLimit() const		{ return m_DofMaxBlurRadiusAtNearInFocusLimit; }
	inline float	GetDofBlurDiskRadiusPowerFactorNear() const			{ return m_DofBlurDiskRadiusPowerFactorNear; }
	inline float	GetDofOverriddenFocusDistance() const				{ return m_DofOverriddenFocusDistance; }
	inline float	GetDofOverriddenFocusDistanceBlendLevel() const		{ return m_DofOverriddenFocusDistanceBlendLevel; }
	inline float	GetDofFocusDistanceIncreaseSpringConstant() const	{ return m_DofFocusDistanceIncreaseSpringConstant; }
	inline float	GetDofFocusDistanceDecreaseSpringConstant() const	{ return m_DofFocusDistanceDecreaseSpringConstant; }
	inline float	GetDofPlanesBlendLevel() const						{ return m_DofPlanesBlendLevel; }
	inline float	GetDofFullScreenBlurBlendLevel() const				{ return m_DofFullScreenBlurBlendLevel; }
	void			GetDofSettings(camDepthOfFieldSettingsMetadata& settings) const;

	inline float	GetMotionBlurStrength() const	{ return m_MotionBlurStrength; }

	inline const fwFlags16&	GetFlags() const		{ return m_Flags; }
	inline fwFlags16&		GetFlags()				{ return m_Flags; }

	inline float	ComputeHeading() const			{ return ComputeHeadingFromMatrix(m_WorldMatrix); }
	inline float	ComputePitch() const			{ return ComputePitchFromMatrix(m_WorldMatrix); }
	inline float	ComputeRoll() const				{ return ComputeRollFromMatrix(m_WorldMatrix); }
	inline void		ComputeHeadingAndPitch(float& heading, float& pitch) const { return ComputeHeadingAndPitchFromMatrix(m_WorldMatrix, heading, pitch); }
	inline void		ComputeHeadingPitchAndRoll(float& heading, float& pitch, float& roll) const { return ComputeHeadingPitchAndRollFromMatrix(m_WorldMatrix, heading, pitch, roll); }
	inline void		ComputeRageWorldMatrix(Matrix34& rageWorldMatrix) const;
	inline void		ComputeDofFromFovAndClipRange();
	inline void		ComputeDofPlanes(Vector4& planes) const;
#if ADAPTIVE_DOF_CPU
 	void			ComputeDofPlanesAndBlurRadiusUsingLensModel(float distanceToSubject, Vector4& planes, float& blurDiskRadiusToApply) const;
#endif
	void			InterpolateDofSettings(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame);

	inline void		SetWorldMatrix(const Matrix34& matrix, const bool shouldSet3x3 = true);
	inline void		SetWorldMatrixFromFront(const Vector3& front);
	inline void		SetWorldMatrixFromFrontAndUp(const Vector3& front, const Vector3& up);
	inline void		SetWorldMatrixFromHeadingPitchAndRoll(const float heading, const float pitch = 0.0f, const float roll = 0.0f);

	inline void 	SetPosition(const Vector3& position)				{ m_WorldMatrix.d = position; }
	inline void 	SetFov(const float fov)								{ m_Fov = (cameraVerifyf((fov >= g_MinFov) && (fov <= g_MaxFov), "An attempt was made to set the FOV of a camera frame to an invalid value (%f)", fov) ? fov : Clamp(fov, g_MinFov, g_MaxFov)); }
	inline void 	SetNearClip(const float nearClip)					{ m_NearClip = Max(nearClip, g_MinNearClip); }
	inline void 	SetFarClip(const float farClip)						{ m_FarClip = farClip; }
	inline void		SetCameraTimeScaleThisUpdate(const float scaling)	{ m_CameraTimeScaleThisUpdate = scaling; }

	inline void 	SetNearInFocusDofPlane(const float distance)					{ m_DofPlanes[NEAR_IN_FOCUS_DOF_PLANE] = distance; m_Flags.ClearFlag(Flag_ShouldUseRawDofPlanes); }
	inline void 	SetFarInFocusDofPlane(const float distance)						{ m_DofPlanes[FAR_IN_FOCUS_DOF_PLANE] = distance; m_Flags.ClearFlag(Flag_ShouldUseRawDofPlanes); }
	inline void 	SetDofStrength(const float strength)							{ m_DofStrength = Clamp(strength, 0.0f, 1.0f); m_Flags.ClearFlag(Flag_ShouldUseRawDofPlanes); }
	inline void		SetDofBlurDiskRadius(float blurDiskRadius)						{ m_DofBlurDiskRadius = Clamp(blurDiskRadius, g_MinDofBlurDiskRadius, g_MaxDofBlurDiskRadius); }
	inline void 	SetDofMaxPixelDepth(const float depth)							{ m_DofMaxPixelDepth = depth; }
	inline void		SetDofMaxPixelDepthBlendLevel(const float blendLevel)			{ m_DofMaxPixelDepthBlendLevel = blendLevel; }
	inline void 	SetDofPixelDepthPowerFactor(const float powerFactor)			{ m_DofPixelDepthPowerFactor = powerFactor; }
	inline void 	SetFNumberOfLens(const float fNumber)							{ m_FNumberOfLens = fNumber; }
	inline void 	SetFocalLengthMultiplier(const float length)					{ m_FocalLengthMultiplier = length; }
	inline void 	SetDofFocusDistanceGridScaling(const Vector2& scaling)			{ m_DofFocusDistanceGridScaling = scaling; }
	inline void 	SetDofSubjectMagPowerNearFar(const Vector2& magPower)			{ m_DofSubjectMagPowerNearFar = magPower; }
	inline void 	SetDofFStopsAtMaxExposure(const float blendLevel)				{ m_DofFStopsAtMaxExposure = blendLevel; }
	inline void 	SetDofFocusDistanceBias(const float bias)						{ m_DofFocusDistanceBias = bias; }
	inline void 	SetDofMaxNearInFocusDistance(const float distance)				{ m_DofMaxNearInFocusDistance = distance; }
	inline void 	SetDofMaxNearInFocusDistanceBlendLevel(const float blendLevel)	{ m_DofMaxNearInFocusDistanceBlendLevel = blendLevel; }
	inline void 	SetDofMaxBlurRadiusAtNearInFocusLimit(const float radius)		{ m_DofMaxBlurRadiusAtNearInFocusLimit = radius; }
	inline void 	SetDofBlurDiskRadiusPowerFactorNear(const float powerFactor)	{ m_DofBlurDiskRadiusPowerFactorNear = powerFactor; }
	inline void 	SetDofOverriddenFocusDistance(const float distance)				{ m_DofOverriddenFocusDistance = distance; }
	inline void 	SetDofOverriddenFocusDistanceBlendLevel(const float blendLevel)	{ m_DofOverriddenFocusDistanceBlendLevel = blendLevel; }
	inline void 	SetDofFocusDistanceIncreaseSpringConstant(const float constant)	{ m_DofFocusDistanceIncreaseSpringConstant = constant; }
	inline void 	SetDofFocusDistanceDecreaseSpringConstant(const float constant)	{ m_DofFocusDistanceDecreaseSpringConstant = constant; }
	inline void 	SetDofPlanesBlendLevel(const float blendLevel)					{ m_DofPlanesBlendLevel = blendLevel; }
	inline void		SetDofFullScreenBlurBlendLevel(const float blendLevel)			{ m_DofFullScreenBlurBlendLevel = Clamp(blendLevel, 0.0f, 1.0f); }
	inline void		SetDofPlanes(const Vector4& planes)								{ m_DofPlanes.Set(planes); m_Flags.SetFlag(Flag_ShouldUseRawDofPlanes); }
	void			SetDofSettings(const camDepthOfFieldSettingsMetadata& settings);
// 	inline void		SetDofPlanesAndBlurRadiusUsingLensModel(float distanceToSubject);

	//Note: There currently no asserts on motion blur input as some camera system components can provide an out of range input.
	inline void		SetMotionBlurStrength(const float strength)	{ m_MotionBlurStrength = Clamp(strength, 0.0f, g_MaxMotionBlurStrength); }

	inline void		CloneFrom(const camFrame& sourceFrame);

	inline static void	ComputeFrontFromHeadingAndPitch(const float heading, const float pitch, Vector3 &front);
	inline static void	ComputeWorldMatrixFromFront(const Vector3& front, Matrix34 &worldMatrix);
	inline static void	ComputeWorldMatrixFromFrontAndUp(const Vector3& front, const Vector3& up, Matrix34 &worldMatrix);
	inline static void	ComputeWorldMatrixFromHeadingPitchAndRoll(float heading, float pitch, float roll, Matrix34 &worldMatrix);

	inline static float	ComputeHeadingFromMatrix(const Matrix34& worldMatrix);
	inline static float	ComputePitchFromMatrix(const Matrix34& worldMatrix);
	inline static float	ComputeRollFromMatrix(const Matrix34& worldMatrix);
	inline static void	ComputeHeadingAndPitchFromMatrix(const Matrix34& worldMatrix, float& heading, float& pitch);
	inline static void	ComputeHeadingPitchAndRollFromMatrix(const Matrix34& worldMatrix, float& heading, float& pitch, float& roll);
	inline static void	ComputeHeadingPitchAndRollFromMatrixUsingEulers(const Matrix34& worldMatrix, float& heading, float& pitch, float& roll);

	inline static float	ComputeHeadingFromFront(const Vector3& front);
	inline static float	ComputePitchFromFront(const Vector3& front);
	inline static void	ComputeHeadingAndPitchFromFront(const Vector3& front, float& heading, float& pitch);
	inline static void	ComputeNormalisedXYFromFront(const Vector3& front, float &normalisedX, float &normalisedY);
	static float		ComputeHeadingDeltaBetweenFronts(const Vector3& front1, const Vector3& front2);
	static float		ComputePitchDeltaBetweenFronts(const Vector3& front1, const Vector3& front2);

	inline static void	SlerpOrientation(float t, const Vector3& sourceFront, const Vector3& destinationFront, Vector3& resultFront,
							bool shouldMaintainDirection = false, Quaternion* previousOrientationDelta = NULL);
	static void			SlerpOrientation(float t, const Matrix34& sourceWorldMatrix, const Matrix34& destinationWorldMatrix, Matrix34& resultWorldMatrix,
							bool shouldMaintainDirection = false, Quaternion* previousOrientationDelta = NULL);

	inline static float	InterpolateFNumberOfLens(float blendLevel, float sourceFNumber, float destinationFNumber);

	//NOTE: This is based upon ComputeRotation in vector3.h and has been modified as the RAGE version excluded small rotations.
	inline static void	ComputeAxisAngleRotation(const Vector3& unitFrom, const Vector3& unitTo, Vector3& unitAxis, float& angle);

#if __BANK
	void		SetDebugFrameHistory(const s32 numFrames);

	void		DebugRender(bool shouldRenderExtraInfo) const;
	void		DebugRenderFrustum(const Color32 colour) const;
	void		DebugRenderNearClip(const Color32 colour) const;

	static void	AddWidgets(bkBank &bank);
#endif // __BANK

private:
// 	inline static void EnsureWorldMatrixIsValid(Matrix34 &matrix);

#if __BANK
	void	DebugRenderFarClip(const Color32 colour) const;
	void	DebugRenderNearDof(const Color32 colour) const;
	void	DebugRenderFarDof(const Color32 colour) const;
	void	DebugRenderPlane(const Color32 colour, const float distance, const float scale) const;
#endif // __BANK

	Matrix34 m_WorldMatrix;
	Vector4	m_DofPlanes;			//Depth of field plane distances (near out-of-focus, near in-focus, far in-focus and far out-of-focus.)
	Vector2	m_DofFocusDistanceGridScaling;
	Vector2	m_DofSubjectMagPowerNearFar;
	float   m_Fov;  				//Field of view, in degrees.
	float   m_NearClip;
	float   m_FarClip;
	float	m_CameraTimeScaleThisUpdate;
	float   m_DofStrength;			//The strength of the high-quality depth of field effect, which is used to derive the two out-of-focus planes.
	float	m_DofBlurDiskRadius;	//Dof blur radius in pixels.
	float	m_DofMaxPixelDepth;
	float	m_DofMaxPixelDepthBlendLevel;
	float	m_DofPixelDepthPowerFactor;
	float	m_FNumberOfLens;
	float	m_FocalLengthMultiplier;
	float	m_DofFStopsAtMaxExposure;
	float	m_DofFocusDistanceBias;
	float	m_DofMaxNearInFocusDistance;
	float	m_DofMaxNearInFocusDistanceBlendLevel;
	float	m_DofMaxBlurRadiusAtNearInFocusLimit;
	float	m_DofBlurDiskRadiusPowerFactorNear;
	float	m_DofOverriddenFocusDistance;
	float	m_DofOverriddenFocusDistanceBlendLevel;
	float	m_DofFocusDistanceIncreaseSpringConstant;
	float	m_DofFocusDistanceDecreaseSpringConstant;
	float	m_DofPlanesBlendLevel;
	float	m_DofFullScreenBlurBlendLevel;
	float   m_MotionBlurStrength;	//(0.0 = no motion blur, 1.0 = full motion blur, >1.0 = extreme motion blur)
	fwFlags16 m_Flags;

	//Depth of field strength mapping.
	static bank_float	ms_OutOfFocusScalingAtMinDofStrength;
	static bank_float	ms_OutOfFocusScalingAtMaxDofStrength;

	//Values used when FOV widens over the default:
	static bank_float	ms_DofNearWhenFovWider;
	static bank_float	ms_DofFarWhenFovWider;
	static bank_float	ms_FovForDofRangeNearMaxWider;
	static bank_float	ms_FovForDofRangeFarMaxWider;

	//Values used when FOV gets thinner over the default:
	static bank_float	ms_DofNearWhenFovThinner;
	static bank_float	ms_DofFarWhenFovThinner;
	static bank_float	ms_FovForDofRangeNearMaxThinner;
	static bank_float	ms_FovForDofRangeFarMaxThinner;
};

inline void camFrame::ComputeRageWorldMatrix(Matrix34& rageWorldMatrix) const
{
	rageWorldMatrix.a = m_WorldMatrix.a;
	rageWorldMatrix.b = m_WorldMatrix.c;
	rageWorldMatrix.c = -m_WorldMatrix.b; //90 degree rotation...confirmed..see Sandy!
	rageWorldMatrix.d = m_WorldMatrix.d;
}

inline void camFrame::SetWorldMatrix(const Matrix34& matrix, const bool shouldSet3x3)
{
	//Matrix34 localMatrix(matrix);
	//EnsureWorldMatrixIsValid(localMatrix);

	//Catch bad matrix values at the front door.
	if(cameraVerifyf(matrix.IsOrthonormal() && matrix.a.FiniteElements() && matrix.b.FiniteElements() && matrix.c.FiniteElements(),
		"An attempt was made to set the world matrix of a camera frame to invalid/infinite values"))
	{
		if(shouldSet3x3)
		{
			m_WorldMatrix.Set3x3(matrix);
		}
		//Catch bad matrix values at the front door.
		else if(cameraVerifyf(matrix.d.FiniteElements(),
			"An attempt was made to set the world matrix of a camera frame to an invalid/infinite position"))
		{
			m_WorldMatrix.Set(matrix);
		}
	}
}

inline void camFrame::SetWorldMatrixFromFront(const Vector3& front)
{
	Matrix34 worldMatrix;
	ComputeWorldMatrixFromFront(front, worldMatrix);
	SetWorldMatrix(worldMatrix); //Use the accessor for validation.
}

inline void camFrame::SetWorldMatrixFromFrontAndUp(const Vector3& front, const Vector3& up)
{
	Matrix34 worldMatrix;
	ComputeWorldMatrixFromFrontAndUp(front, up, worldMatrix);
	SetWorldMatrix(worldMatrix); //Use the accessor for validation.
}

inline void camFrame::SetWorldMatrixFromHeadingPitchAndRoll(const float heading, const float pitch, const float roll)
{
	Matrix34 worldMatrix;
	ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, worldMatrix);
	SetWorldMatrix(worldMatrix); //Use the accessor for validation.
}

//A common method to make the DOF FOV-relative to give the impression of lens focus etc (it's based on defaults though.)
//TODO: Rewrite/clean-up.
inline void camFrame::ComputeDofFromFovAndClipRange()
{
	float nearDof	= g_DefaultNearInFocusDofPlane;
	float farDof	= g_DefaultFarInFocusDofPlane;

	const float fovDelta = m_Fov - g_DefaultFov;
	if(fovDelta > 0.0f)
	{
		//Zoom out
		nearDof	+= Min(fovDelta / ms_FovForDofRangeNearMaxWider, 1.0f) * ms_DofNearWhenFovWider;																		//  The DOF is its default plus this change
		farDof	+= Min(fovDelta / ms_FovForDofRangeFarMaxWider, 1.0f) * ms_DofFarWhenFovWider;																		//  The DOF is its default plus this change
	}
	else
	{
		//Zoom in
		nearDof	+= Max(fovDelta / ms_FovForDofRangeNearMaxThinner, -1.0f) * ms_DofNearWhenFovThinner;																		//  The DOF is its default plus this change
		farDof	+= Max(fovDelta / ms_FovForDofRangeFarMaxThinner, -1.0f) * ms_DofFarWhenFovThinner;																		//  The DOF is its default plus this change
	}

	SetNearInFocusDofPlane(nearDof);
	SetFarInFocusDofPlane(farDof);
}

inline void camFrame::ComputeDofPlanes(Vector4& planes) const
{
	planes.Set(m_DofPlanes);

	if(m_Flags.IsFlagSet(Flag_ShouldUseRawDofPlanes))
	{
		return;
	}

	//Compute the depth between the out-of-focus planes, to be split between the near and far planes, (ab)using the 1/3 front 2/3 back focus guide.

	const float depthOfField				= m_DofPlanes[FAR_IN_FOCUS_DOF_PLANE] - m_DofPlanes[NEAR_IN_FOCUS_DOF_PLANE];
	const float focusDistance				= m_DofPlanes[NEAR_IN_FOCUS_DOF_PLANE] + (depthOfField / 3.0f);
	//NOTE: We use a non-linear mapping of the strength, as this allows for fine control at high strength with a wide range of scaling factors.
	const float dofStrengthToApply			= Sqrtf(Sqrtf(m_DofStrength));
	const float outOfFocusScalingFactor		= Lerp(dofStrengthToApply, ms_OutOfFocusScalingAtMinDofStrength, ms_OutOfFocusScalingAtMaxDofStrength);
	const float outOfFocusToOutOfFocusDepth	= depthOfField * outOfFocusScalingFactor;

	planes[NEAR_OUT_OF_FOCUS_DOF_PLANE]		= focusDistance - (outOfFocusToOutOfFocusDepth / 3.0f);
	//Inhibit negative distances, as this presently undermines the effect.
	planes[NEAR_OUT_OF_FOCUS_DOF_PLANE]		= Max(planes[NEAR_OUT_OF_FOCUS_DOF_PLANE], 0.0f);

	planes[FAR_OUT_OF_FOCUS_DOF_PLANE]		= focusDistance + (outOfFocusToOutOfFocusDepth * 2.0f / 3.0f);
}

// inline void camFrame::SetDofPlanesAndBlurRadiusUsingLensModel(float distanceToSubject)
// {
// 	float blurDiskRadiusToApply;
// 	ComputeDofPlanesAndBlurRadiusUsingLensModel(distanceToSubject, m_DofPlanes, blurDiskRadiusToApply);
// 
// 	SetDofBlurDiskRadius(blurDiskRadiusToApply);
// 
// 	m_Flags.SetFlag(Flag_ShouldUseRawDofPlanes);
// }

inline void camFrame::CloneFrom(const camFrame& sourceFrame)
{
	//NOTE: We use the accessors here to benefit from asserts and clamping.
	SetPosition(								sourceFrame.GetPosition());
	SetWorldMatrix(								sourceFrame.GetWorldMatrix());
	SetFov(										sourceFrame.GetFov());
	SetNearClip(								sourceFrame.GetNearClip());
	SetFarClip(									sourceFrame.GetFarClip());
	SetCameraTimeScaleThisUpdate(				sourceFrame.GetCameraTimeScaleThisUpdate());
	SetDofStrength(								sourceFrame.GetDofStrength());
	SetDofBlurDiskRadius(						sourceFrame.GetDofBlurDiskRadius());
	SetDofMaxPixelDepth(						sourceFrame.GetDofMaxPixelDepth());
	SetDofMaxPixelDepthBlendLevel(				sourceFrame.GetDofMaxPixelDepthBlendLevel());
	SetDofPixelDepthPowerFactor(				sourceFrame.GetDofPixelDepthPowerFactor());
	SetFNumberOfLens(							sourceFrame.GetFNumberOfLens());
	SetFocalLengthMultiplier(					sourceFrame.GetFocalLengthMultiplier());
	SetDofFocusDistanceGridScaling(				sourceFrame.GetDofFocusDistanceGridScaling());
	SetDofSubjectMagPowerNearFar(				sourceFrame.GetDofSubjectMagPowerNearFar());
	SetDofFStopsAtMaxExposure(					sourceFrame.GetDofFStopsAtMaxExposure());
	SetDofFocusDistanceBias(					sourceFrame.GetDofFocusDistanceBias());
	SetDofMaxNearInFocusDistance(				sourceFrame.GetDofMaxNearInFocusDistance());
	SetDofMaxNearInFocusDistanceBlendLevel(		sourceFrame.GetDofMaxNearInFocusDistanceBlendLevel());
	SetDofMaxBlurRadiusAtNearInFocusLimit(		sourceFrame.GetDofMaxBlurRadiusAtNearInFocusLimit());
	SetDofBlurDiskRadiusPowerFactorNear(		sourceFrame.GetDofBlurDiskRadiusPowerFactorNear());
	SetDofOverriddenFocusDistance(				sourceFrame.GetDofOverriddenFocusDistance());
	SetDofOverriddenFocusDistanceBlendLevel(	sourceFrame.GetDofOverriddenFocusDistanceBlendLevel());
	SetDofFocusDistanceIncreaseSpringConstant(	sourceFrame.GetDofFocusDistanceIncreaseSpringConstant());
	SetDofFocusDistanceDecreaseSpringConstant(	sourceFrame.GetDofFocusDistanceDecreaseSpringConstant());
	SetDofPlanesBlendLevel(						sourceFrame.GetDofPlanesBlendLevel());
	SetDofPlanes(								sourceFrame.m_DofPlanes);
	SetDofFullScreenBlurBlendLevel(				sourceFrame.GetDofFullScreenBlurBlendLevel());
	SetMotionBlurStrength(						sourceFrame.GetMotionBlurStrength());

	m_Flags.SetAllFlags(sourceFrame.GetFlags().GetAllFlags());
}

inline void camFrame::ComputeFrontFromHeadingAndPitch(const float heading, const float pitch, Vector3 &front)
{
	const float sinHeading	= rage::Sinf(heading);
	const float cosHeading	= rage::Cosf(heading);
	const float sinPitch	= rage::Sinf(pitch);
	const float cosPitch	= rage::Cosf(pitch);

	front = Vector3(-cosPitch * sinHeading, cosPitch * cosHeading, sinPitch);
}

inline void camFrame::ComputeWorldMatrixFromFront(const Vector3& front, Matrix34 &worldMatrix)
{
	//Ensure front is normalised and valid.
	Vector3 localFront(front);
	localFront.Normalize();

	//Safely derive the corresponding up vector assuming no roll.
	// - First derive a temporary right vector.
	Vector3 crossedRight;
	crossedRight.Cross(localFront, ZAXIS);
	Vector3 frontZ;
	frontZ.SplatZ(localFront);
	frontZ.Abs();
	Vector3 selector = frontZ.IsGreaterThanV(g_VeryCloseToIdentityVector);
	//NOTE: The right vector is undefined when the front vector is aligned with world up or down, as a front vector alone provides too little
	//information, so we arbitrarily use the world X axis.
	Vector3 right(selector.Select(crossedRight, XAXIS));

	Vector3 up;
	up.Cross(right, localFront);

	ComputeWorldMatrixFromFrontAndUp(localFront, up, worldMatrix);
}

inline void camFrame::ComputeWorldMatrixFromFrontAndUp(const Vector3& front, const Vector3& up, Matrix34 &worldMatrix)
{
	worldMatrix.b = front;
	worldMatrix.c = up;
	worldMatrix.a.Cross(front, up);

	worldMatrix.Normalize(); //Ensure this is an orthonormal matrix.
}

inline void	camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(float heading, float pitch, float roll, Matrix34 &worldMatrix)
{
	const Vector3 eulers(pitch, roll, heading);
	worldMatrix.FromEulersYXZ(eulers); //NOTE: YXZ rotation ordering is used to avoid aliasing of the roll.
}

inline float camFrame::ComputeHeadingFromMatrix(const Matrix34& worldMatrix)
{
	//NOTE: An alternate method is used when the front vector is aligned with world up or down, due to Gimbal lock.
	return Selectf(Abs(worldMatrix.b.z) - 1.0f + VERY_SMALL_FLOAT, rage::Atan2f(worldMatrix.c.x,worldMatrix.a.x), rage::Atan2f(-worldMatrix.b.x, worldMatrix.b.y));
}

inline float camFrame::ComputePitchFromMatrix(const Matrix34& worldMatrix)
{
	return AsinfSafe(worldMatrix.b.z);
}

inline float camFrame::ComputeRollFromMatrix(const Matrix34& worldMatrix)
{
	//NOTE: The roll is zeroed when the front vector is aligned with world up or down, due to Gimbal lock.
	return Selectf(Abs(worldMatrix.b.z) - 1.0f + VERY_SMALL_FLOAT, 0.0f, rage::Atan2f(-worldMatrix.a.z, worldMatrix.c.z));
}

inline void camFrame::ComputeHeadingAndPitchFromMatrix(const Matrix34& worldMatrix, float& heading, float& pitch)
{
	heading	= ComputeHeadingFromMatrix(worldMatrix);
	pitch	= ComputePitchFromMatrix(worldMatrix);
}

inline void camFrame::ComputeHeadingPitchAndRollFromMatrix(const Matrix34& worldMatrix, float& heading, float& pitch, float& roll)
{
	heading	= ComputeHeadingFromMatrix(worldMatrix);
	pitch	= ComputePitchFromMatrix(worldMatrix);
	roll	= ComputeRollFromMatrix(worldMatrix);
}

inline void camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(const Matrix34& worldMatrix, float& heading, float& pitch, float& roll)
{
	Vector3 eulers;
	worldMatrix.ToEulersYXZ(eulers);
	heading	= eulers.z;
	pitch	= eulers.x;
	roll	= eulers.y;
}

inline float camFrame::ComputeHeadingFromFront(const Vector3& front)
{
	//NOTE: The heading is undefined when the front vector is aligned with world up or down, as a front vector alone provides too little information.
	return Selectf(Abs(front.z) - 1.0f + VERY_SMALL_FLOAT, 0.0f, rage::Atan2f(-front.x, front.y));
}

inline float camFrame::ComputePitchFromFront(const Vector3& front)
{
	return AsinfSafe(front.z);
}

inline void camFrame::ComputeHeadingAndPitchFromFront(const Vector3& front, float& heading, float& pitch)
{
	heading	= ComputeHeadingFromFront(front);
	pitch	= ComputePitchFromFront(front);
}

inline void camFrame::ComputeNormalisedXYFromFront(const Vector3& front, float &normalisedX, float &normalisedY)
{
	//NOTE: The XY vector is undefined when the front vector is aligned with world up or down, so we arbitrarily return the world Y axis.
	Vector3 localFront(front);
	localFront.z = 0.0f;
	localFront.NormalizeSafe(YAXIS);
	normalisedX = localFront.x;
	normalisedY = localFront.y;
}

inline void camFrame::SlerpOrientation(float t, const Vector3& sourceFront, const Vector3& destinationFront, Vector3& resultFront,
	bool shouldMaintainDirection, Quaternion* previousOrientationDelta)
{
	Matrix34 sourceWorldMatrix;
	camFrame::ComputeWorldMatrixFromFront(sourceFront, sourceWorldMatrix);
	Matrix34 destinationWorldMatrix;
	camFrame::ComputeWorldMatrixFromFront(destinationFront, destinationWorldMatrix);

	Matrix34 resultWorldMatrix;
	SlerpOrientation(t, sourceWorldMatrix, destinationWorldMatrix, resultWorldMatrix, shouldMaintainDirection, previousOrientationDelta);

	resultFront = resultWorldMatrix.b;
}

inline float camFrame::InterpolateFNumberOfLens(float blendLevel, float sourceFNumber, float destinationFNumber)
{
	const float sourceFStops		= log10(sourceFNumber) / log10(M_SQRT2);
	const float destinationFStops	= log10(destinationFNumber) / log10(M_SQRT2);
	const float fStopsToApply		= Lerp(blendLevel, sourceFStops, destinationFStops);
	const float fNumberToApply		= pow(M_SQRT2, fStopsToApply);

	return fNumberToApply;
}

inline void camFrame::ComputeAxisAngleRotation(const Vector3& unitFrom, const Vector3& unitTo, Vector3& unitAxis, float& angle)
{
	unitAxis.Set(unitFrom);
	angle = PI;

	const float dot			= unitFrom.Dot(unitTo);
	const float underOne	= square(0.999f);
	if(dot > -underOne)
	{
		//The vectors defining the rotation are not nearly anti-parallel.
		unitAxis.Cross(unitTo);
		float sine = unitAxis.Mag();
		if (sine != 0.0f)
		{
			unitAxis.InvScale(sine);
			angle = (dot > 0.0f) ? AsinfSafe(sine) : (PI - AsinfSafe(sine));
		}
		else
		{
			unitAxis.Set(YAXIS);
			angle = 0.0f;
		}
	}
	else
	{
		//The vectors defining the rotation are in nearly opposite directions, so choose an axis for a 180 degree rotation.
		Vector3 perpendicular = (fabsf(unitFrom.x) < SQRT3INVERSE ? XAXIS : (fabsf(unitFrom.y) < SQRT3INVERSE ? YAXIS : ZAXIS));
		unitAxis.Cross(perpendicular);
		unitAxis.Normalize();
	}
}

// inline void camFrame::EnsureWorldMatrixIsValid(Matrix34 &matrix)
// {
// 	matrix.Normalize(); //Ensure the world matrix is orthonormal.
// 
// 	//Prevent the world matrix being assigned a front that is aligned with world up or down, as this would incur aliasing and trigonometric asserts.
// 	const float pitch = AsinfSafe(matrix.b.z);
// 	const float absPitchDeltaToNearestPole = HALF_PI - Abs(pitch);
// 	if(absPitchDeltaToNearestPole < g_MinAbsPitchDeltaFromPolesToAvoidSingularity)
// 	{
// 		//Rotate away from the pole to a safe orientation.
// 		const float pitchDeltaToApply = -(g_MinAbsPitchDeltaFromPolesToAvoidSingularity - absPitchDeltaToNearestPole) * Sign(pitch);
// 		matrix.RotateLocalX(pitchDeltaToApply);
// 	}
// }

#endif // CAMERA_FRAME_H
