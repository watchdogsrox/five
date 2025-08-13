//
// camera/helpers/Frame.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/Frame.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "fwscene/world/WorldLimits.h"
#include "math/vecmath.h"

// Game headers
#include "camera/system/CameraManager.h"
#include "camera/viewports/ViewportManager.h"
#include "scene/portals/FrustumDebug.h"

CAMERA_OPTIMISATIONS()

//Motion blur is off by default.
const float g_DefaultMotionBlurStrength	= 0.0f;
const float g_MaxDofBlurRadiusRatio		= 0.9f;

//Scale the world limits to allow for wider limits when validating camera positions.
const float		g_WorldLimitScalingForCamera = 4.0f;
const spdAABB	g_CameraWorldLimits_AABB(
					Vec3V(WORLDLIMITS_XMIN * g_WorldLimitScalingForCamera, WORLDLIMITS_YMIN * g_WorldLimitScalingForCamera, WORLDLIMITS_ZMIN * g_WorldLimitScalingForCamera),
					Vec3V(WORLDLIMITS_XMAX * g_WorldLimitScalingForCamera, WORLDLIMITS_YMAX * g_WorldLimitScalingForCamera, WORLDLIMITS_ZMAX * g_WorldLimitScalingForCamera));
const Vector3	g_InvalidPosition(WORLDLIMITS_XMAX * g_WorldLimitScalingForCamera * 2.0f, WORLDLIMITS_YMAX * g_WorldLimitScalingForCamera * 2.0f,
					WORLDLIMITS_ZMAX * g_WorldLimitScalingForCamera * 2.0f);
const Vector3	g_InvalidRelativePosition(-1000.0f, -1000.0f, -1000.0f);
const Vector3	g_VeryCloseToIdentityVector(1.0f - SMALL_FLOAT, 1.0f - SMALL_FLOAT, 1.0f - SMALL_FLOAT);

bank_float	camFrame::ms_OutOfFocusScalingAtMinDofStrength	= 300.0f;
bank_float	camFrame::ms_OutOfFocusScalingAtMaxDofStrength	= 2.0f;

bank_float	camFrame::ms_DofNearWhenFovWider			= 0.0f;
bank_float	camFrame::ms_DofFarWhenFovWider				= -75.0f;
bank_float	camFrame::ms_FovForDofRangeNearMaxWider		= 70.0f;
bank_float	camFrame::ms_FovForDofRangeFarMaxWider		= 70.0f;

bank_float	camFrame::ms_DofNearWhenFovThinner			= -20.0f;
bank_float	camFrame::ms_DofFarWhenFovThinner			= -150.0f;
bank_float	camFrame::ms_FovForDofRangeNearMaxThinner	= 54.0f;
bank_float	camFrame::ms_FovForDofRangeFarMaxThinner	= 54.0f;

#if __BANK
const Color32	g_DebugRenderFrustumColour				= Color32(0, 0, 255, 32);
const Color32	g_DebugRenderClipColour					= Color32(255, 0, 255, 16);
const Color32	g_DebugRenderDofColour					= Color32(255, 255, 0, 16);
#endif // __BANK


camFrame::camFrame()
: m_DofPlanes(0.0f, g_DefaultNearInFocusDofPlane, g_DefaultFarInFocusDofPlane, 0.0f)
, m_DofFocusDistanceGridScaling(1.0f, 1.0f)
, m_DofSubjectMagPowerNearFar(1.0f, 1.0f)
, m_Fov(g_DefaultFov)
, m_NearClip(g_DefaultNearClip)
, m_FarClip(g_DefaultFarClip)
, m_CameraTimeScaleThisUpdate(1.0f)
, m_DofStrength(g_DefaultDofStrength)
, m_DofBlurDiskRadius(1.0f)
, m_DofMaxPixelDepth(LARGE_FLOAT)
, m_DofMaxPixelDepthBlendLevel(0.0f)
, m_DofPixelDepthPowerFactor(1.0f)
, m_FNumberOfLens(1.0f)
, m_FocalLengthMultiplier(1.0f)
, m_DofFStopsAtMaxExposure(0.0f)
, m_DofFocusDistanceBias(0.0f)
, m_DofMaxNearInFocusDistance(LARGE_FLOAT)
, m_DofMaxNearInFocusDistanceBlendLevel(0.0f)
, m_DofMaxBlurRadiusAtNearInFocusLimit(1.0f)
, m_DofBlurDiskRadiusPowerFactorNear(1.0f)
, m_DofOverriddenFocusDistance(0.0f)
, m_DofOverriddenFocusDistanceBlendLevel(0.0f)
, m_DofFocusDistanceIncreaseSpringConstant(0.0f)
, m_DofFocusDistanceDecreaseSpringConstant(0.0f)
, m_DofPlanesBlendLevel(0.0f)
, m_DofFullScreenBlurBlendLevel(0.0f)
, m_MotionBlurStrength(g_DefaultMotionBlurStrength)
, m_Flags(0)
{
	m_WorldMatrix.Identity3x3();
	m_WorldMatrix.d = g_InvalidPosition;
}

void camFrame::GetDofSettings(camDepthOfFieldSettingsMetadata& settings) const
{
	settings.m_FocusDistanceGridScaling.Set(m_DofFocusDistanceGridScaling);
	settings.m_SubjectMagnificationPowerFactorNearFar.Set(m_DofSubjectMagPowerNearFar);
	settings.m_MaxPixelDepth						= m_DofMaxPixelDepth;
	settings.m_MaxPixelDepthBlendLevel				= m_DofMaxPixelDepthBlendLevel;
	settings.m_PixelDepthPowerFactor				= m_DofPixelDepthPowerFactor;
	settings.m_FNumberOfLens						= m_FNumberOfLens;
	settings.m_FocalLengthMultiplier				= m_FocalLengthMultiplier;
	settings.m_FStopsAtMaxExposure					= m_DofFStopsAtMaxExposure;
	settings.m_FocusDistanceBias					= m_DofFocusDistanceBias;
	settings.m_MaxNearInFocusDistance				= m_DofMaxNearInFocusDistance;
	settings.m_MaxNearInFocusDistanceBlendLevel		= m_DofMaxNearInFocusDistanceBlendLevel;
	settings.m_MaxBlurRadiusAtNearInFocusLimit		= m_DofMaxBlurRadiusAtNearInFocusLimit;
	settings.m_BlurDiskRadiusPowerFactorNear		= m_DofBlurDiskRadiusPowerFactorNear;
	settings.m_FocusDistanceIncreaseSpringConstant	= m_DofFocusDistanceIncreaseSpringConstant;
	settings.m_FocusDistanceDecreaseSpringConstant	= m_DofFocusDistanceDecreaseSpringConstant;

	settings.m_ShouldMeasurePostAlphaPixelDepth		= m_Flags.IsFlagSet(Flag_ShouldMeasurePostAlphaPixelDepth);
}

void camFrame::SetDofSettings(const camDepthOfFieldSettingsMetadata& settings)
{
	m_DofFocusDistanceGridScaling.Set(settings.m_FocusDistanceGridScaling);
	m_DofSubjectMagPowerNearFar.Set(settings.m_SubjectMagnificationPowerFactorNearFar);
	m_DofMaxPixelDepth							= settings.m_MaxPixelDepth;
	m_DofMaxPixelDepthBlendLevel				= settings.m_MaxPixelDepthBlendLevel;
	m_DofPixelDepthPowerFactor					= settings.m_PixelDepthPowerFactor;
	m_FNumberOfLens								= settings.m_FNumberOfLens;
	m_FocalLengthMultiplier						= settings.m_FocalLengthMultiplier;
	m_DofFStopsAtMaxExposure					= settings.m_FStopsAtMaxExposure;
	m_DofFocusDistanceBias						= settings.m_FocusDistanceBias;
	m_DofMaxNearInFocusDistance					= settings.m_MaxNearInFocusDistance;
	m_DofMaxNearInFocusDistanceBlendLevel		= settings.m_MaxNearInFocusDistanceBlendLevel;
	m_DofMaxBlurRadiusAtNearInFocusLimit		= settings.m_MaxBlurRadiusAtNearInFocusLimit;
	m_DofBlurDiskRadiusPowerFactorNear			= settings.m_BlurDiskRadiusPowerFactorNear;
	m_DofFocusDistanceIncreaseSpringConstant	= settings.m_FocusDistanceIncreaseSpringConstant;
	m_DofFocusDistanceDecreaseSpringConstant	= settings.m_FocusDistanceDecreaseSpringConstant;

	m_Flags.ChangeFlag(Flag_ShouldMeasurePostAlphaPixelDepth, settings.m_ShouldMeasurePostAlphaPixelDepth);
}

#if ADAPTIVE_DOF_CPU
 void camFrame::ComputeDofPlanesAndBlurRadiusUsingLensModel(float distanceToSubject, Vector4& planes, float& blurDiskRadiusToApply) const
 {
 	const float focalLengthOfLens		= m_FocalLengthMultiplier * g_HeightOf35mmFullFrame / (2.0f * Tanf(0.5f * m_Fov * DtoR));
 
 	distanceToSubject					+= m_DofFocusDistanceBias;
 	distanceToSubject					= Max(distanceToSubject, focalLengthOfLens * 2.0f);
 
 	const float magnificationOfSubject	= focalLengthOfLens / (distanceToSubject - focalLengthOfLens);
 
 	const float magnificationOfSubjectNear		= pow(magnificationOfSubject, m_DofSubjectMagPowerNearFar.x);
 	const float effectiveFocalLengthOfLensNear	= magnificationOfSubjectNear * distanceToSubject / (magnificationOfSubjectNear + 1.0f);
 	const float magnificationOfSubjectFar		= pow(magnificationOfSubject, m_DofSubjectMagPowerNearFar.y);
 	const float effectiveFocalLengthOfLensFar	= magnificationOfSubjectFar * distanceToSubject / (magnificationOfSubjectFar + 1.0f);
 
 	const float hyperFocalDistanceNear			= square(effectiveFocalLengthOfLensNear) / (m_FNumberOfLens * g_CircleOfConfusionFor35mm);
 	const float hyperFocalDistanceFar			= square(effectiveFocalLengthOfLensFar) / (m_FNumberOfLens * g_CircleOfConfusionFor35mm);
 
 	float minDistanceToSubject					= 2.0f * Max(effectiveFocalLengthOfLensNear, effectiveFocalLengthOfLensFar);
 	distanceToSubject							= Max(distanceToSubject, minDistanceToSubject);
 	//NOTE: We do not 'waste' DOF by focusing beyond the hyperfocal distance.
 // 	distanceToSubject							= Min(distanceToSubject, hyperFocalDistanceNear);
 	distanceToSubject							= Min(distanceToSubject, hyperFocalDistanceFar);
 
 	const float nearInFocusPlane	= hyperFocalDistanceNear * distanceToSubject / Max(hyperFocalDistanceNear + distanceToSubject, SMALL_FLOAT);
 	const float maxNearInFocusPlane	= Max(m_DofMaxNearInFocusDistance, SMALL_FLOAT);
 	planes[NEAR_IN_FOCUS_DOF_PLANE]	= Min(nearInFocusPlane, maxNearInFocusPlane);
 	const float nearPlaneScaling	= planes[NEAR_IN_FOCUS_DOF_PLANE] / Max(nearInFocusPlane, SMALL_FLOAT);
 
 	float denominator				= hyperFocalDistanceFar - distanceToSubject;
 	denominator						= Max(denominator, SMALL_FLOAT);
 	planes[FAR_IN_FOCUS_DOF_PLANE]	= hyperFocalDistanceFar * distanceToSubject / denominator;
 
 // 	magnificationOfSubject					= focalLengthOfLens / (distanceToSubject - focalLengthOfLens);
 
 	const float maxBlurDiskDiameterMetresFar	= effectiveFocalLengthOfLensFar * magnificationOfSubjectFar / m_FNumberOfLens;
 	const float maxBlurDiskRadiusFar			= maxBlurDiskDiameterMetresFar / (2.0f * g_CircleOfConfusionFor35mm);
 	blurDiskRadiusToApply						= Min(g_MaxDofBlurDiskRadius, maxBlurDiskRadiusFar * g_MaxDofBlurRadiusRatio);
 	blurDiskRadiusToApply						= Max(blurDiskRadiusToApply, 1.0f);
 	const float blurDiskRadiusRatioFar			= blurDiskRadiusToApply / maxBlurDiskRadiusFar;
 
 	const float maxBlurDiskDiameterMetresNear	= effectiveFocalLengthOfLensNear * magnificationOfSubjectNear / m_FNumberOfLens;
 	const float maxBlurDiskRadiusNear			= maxBlurDiskDiameterMetresNear / (2.0f * g_CircleOfConfusionFor35mm);
 	const float blurDiskRadiusRatioNear			= blurDiskRadiusToApply / maxBlurDiskRadiusNear;
 
 	//NOTE: We apply any scaling we applied to limit the near in-focus plane to the out-of-focus plane also.
 	planes[NEAR_OUT_OF_FOCUS_DOF_PLANE]		= nearPlaneScaling * distanceToSubject / (1.0f + blurDiskRadiusRatioNear);
 
 	denominator								= 1.0f - blurDiskRadiusRatioFar;
 	denominator								= Max(denominator, SMALL_FLOAT);
 	planes[FAR_OUT_OF_FOCUS_DOF_PLANE]		= distanceToSubject / denominator;
 
 	//NOTE: Due to the limitations of how the blur disk radius (ratio) is being constrained (and divide-by-zero avoided), it is possible to derive a
 	//far out-of-focus plane that is nearer than the far in-focus plane in extreme cases, so we explicitly inhibit this here.
 	planes[FAR_OUT_OF_FOCUS_DOF_PLANE]		= Max(planes[FAR_OUT_OF_FOCUS_DOF_PLANE], planes[FAR_IN_FOCUS_DOF_PLANE]);
 }
#endif

void camFrame::InterpolateDofSettings(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame)
{
	//NOTE: We don't strictly need to blend the DOF strength, as we take this into account in blending the four planes below. However, we
	//maintain an approximate associated strength for information purposes.
	//NOTE: We must set the strength prior to setting the DOF plane distances, in order to ensure that we respect the four planes, rather
	//than deriving the out-of-focus planes based upon the blended strength.
	const float sourceDofStrength		= sourceFrame.GetDofStrength();
	const float destinationDofStrength	= destinationFrame.GetDofStrength();
	const float dofStrengthToApply		= Lerp(blendLevel, sourceDofStrength, destinationDofStrength);
	SetDofStrength(dofStrengthToApply);

	const float sourceDofPlanesBlendLevel		= sourceFrame.GetDofPlanesBlendLevel();
	const float destinationDofPlanesBlendLevel	= destinationFrame.GetDofPlanesBlendLevel();
	m_DofPlanesBlendLevel						= Lerp(blendLevel, sourceDofPlanesBlendLevel, destinationDofPlanesBlendLevel);

	//NOTE: We must avoid interpolating to/from a invalid (blended out) DOF planes.

	const bool areSourceDofPlanesValid			= (sourceDofPlanesBlendLevel >= SMALL_FLOAT);
	const bool areDestinationDofPlanesValid		= (destinationDofPlanesBlendLevel >= SMALL_FLOAT);

	Vector4 sourceDofPlanes;
	sourceFrame.ComputeDofPlanes(sourceDofPlanes);
	Vector4 destinationDofPlanes;
	destinationFrame.ComputeDofPlanes(destinationDofPlanes);

	Vector4 dofPlanesToApply;
	//NOTE: We also blend the DOF planes if neither frame reports valid planes, for safety. The DOF planes blend level is typically set at director level,
	//which would not be detected during interpolation between camera frames within that director.
	if(areSourceDofPlanesValid == areDestinationDofPlanesValid)
	{
		dofPlanesToApply.Lerp(blendLevel, sourceDofPlanes, destinationDofPlanes);
	}
	else if(areSourceDofPlanesValid)
	{
		dofPlanesToApply.Set(sourceDofPlanes);
	}
	else
	{
		dofPlanesToApply.Set(destinationDofPlanes);
	}

	SetDofPlanes(dofPlanesToApply);

	//NOTE: We must avoid interpolating to/from a invalid (non-overridden) radii.

	const bool isSourceBlurDiskRadiusValid		= sourceFrame.GetFlags().IsFlagSet(camFrame::Flag_ShouldOverrideDofBlurDiskRadius);
	const bool isDestinationBlurDiskRadiusValid	= destinationFrame.GetFlags().IsFlagSet(camFrame::Flag_ShouldOverrideDofBlurDiskRadius);
	const float sourceBlurDiskRadius			= sourceFrame.GetDofBlurDiskRadius();
	const float destinationBlurDiskRadius		= destinationFrame.GetDofBlurDiskRadius();

	float blurDiskRadiusToApply;
	if(isSourceBlurDiskRadiusValid && isDestinationBlurDiskRadiusValid)
	{
		blurDiskRadiusToApply = Lerp(blendLevel, sourceBlurDiskRadius, destinationBlurDiskRadius);
	}
	else if(isSourceBlurDiskRadiusValid)
	{
		blurDiskRadiusToApply = sourceBlurDiskRadius;
	}
	else
	{
		blurDiskRadiusToApply = destinationBlurDiskRadius;
	}

	SetDofBlurDiskRadius(blurDiskRadiusToApply);

	const Vector2& sourceDofFocusDistanceGridScaling		= sourceFrame.GetDofFocusDistanceGridScaling();
	const Vector2& destinationDofFocusDistanceGridScaling	= destinationFrame.GetDofFocusDistanceGridScaling();
	m_DofFocusDistanceGridScaling.Lerp(blendLevel, sourceDofFocusDistanceGridScaling, destinationDofFocusDistanceGridScaling);

	const Vector2& sourceDofSubjectMagPowerNearFar		= sourceFrame.GetDofSubjectMagPowerNearFar();
	const Vector2& destinationDofSubjectMagPowerNearFar	= destinationFrame.GetDofSubjectMagPowerNearFar();
	m_DofSubjectMagPowerNearFar.Lerp(blendLevel, sourceDofSubjectMagPowerNearFar, destinationDofSubjectMagPowerNearFar);

	const float sourceDofMaxPixelDepthBlendLevel		= sourceFrame.GetDofMaxPixelDepthBlendLevel();
	const float destinationDofMaxPixelDepthBlendLevel	= destinationFrame.GetDofMaxPixelDepthBlendLevel();
	m_DofMaxPixelDepthBlendLevel						= Lerp(blendLevel, sourceDofMaxPixelDepthBlendLevel, destinationDofMaxPixelDepthBlendLevel);

	//NOTE: We must avoid interpolating to/from an invalid (blended out) maximum pixel depth.

	const bool isSourceMaxPixelDepthValid				= (sourceDofMaxPixelDepthBlendLevel >= SMALL_FLOAT);
	const bool isDestinationMaxPixelDepthValid			= (destinationDofMaxPixelDepthBlendLevel >= SMALL_FLOAT);
	const float sourceDofMaxPixelDepth					= sourceFrame.GetDofMaxPixelDepth();
	const float destinationDofMaxPixelDepth				= destinationFrame.GetDofMaxPixelDepth();

	if(isSourceMaxPixelDepthValid && isDestinationMaxPixelDepthValid)
	{
		m_DofMaxPixelDepth = Lerp(blendLevel, sourceDofMaxPixelDepth, destinationDofMaxPixelDepth);
	}
	else if(isSourceMaxPixelDepthValid)
	{
		m_DofMaxPixelDepth = sourceDofMaxPixelDepth;
	}
	else
	{
		m_DofMaxPixelDepth = destinationDofMaxPixelDepth;
	}

	const float sourceDofPixelDepthPowerFactor			= sourceFrame.GetDofPixelDepthPowerFactor();
	const float destinationDofPixelDepthPowerFactor		= destinationFrame.GetDofPixelDepthPowerFactor();
	m_DofPixelDepthPowerFactor							= Lerp(blendLevel, sourceDofPixelDepthPowerFactor, destinationDofPixelDepthPowerFactor);

	const float sourceFNumber							= sourceFrame.GetFNumberOfLens();
	const float destinationFNumber						= destinationFrame.GetFNumberOfLens();
	m_FNumberOfLens										= InterpolateFNumberOfLens(blendLevel, sourceFNumber, destinationFNumber);

	const float sourceFocalLengthMultiplier				= sourceFrame.GetFocalLengthMultiplier();
	const float destinationFocalLengthMultiplier		= destinationFrame.GetFocalLengthMultiplier();
	m_FocalLengthMultiplier								= Lerp(blendLevel, sourceFocalLengthMultiplier, destinationFocalLengthMultiplier);

	const float sourceDofFStopsAtMaxExposure			= sourceFrame.GetDofFStopsAtMaxExposure();
	const float destinationDofFStopsAtMaxExposure		= destinationFrame.GetDofFStopsAtMaxExposure();
	m_DofFStopsAtMaxExposure							= Lerp(blendLevel, sourceDofFStopsAtMaxExposure, destinationDofFStopsAtMaxExposure);

	const float sourceDofFocusDistanceBias				= sourceFrame.GetDofFocusDistanceBias();
	const float destinationDofFocusDistanceBias			= destinationFrame.GetDofFocusDistanceBias();
	m_DofFocusDistanceBias								= Lerp(blendLevel, sourceDofFocusDistanceBias, destinationDofFocusDistanceBias);

	const float sourceDofMaxNearInFocusDistanceBlendLevel		= sourceFrame.GetDofMaxNearInFocusDistanceBlendLevel();
	const float destinationDofMaxNearInFocusDistanceBlendLevel	= destinationFrame.GetDofMaxNearInFocusDistanceBlendLevel();
	m_DofMaxNearInFocusDistanceBlendLevel						= Lerp(blendLevel, sourceDofMaxNearInFocusDistanceBlendLevel, destinationDofMaxNearInFocusDistanceBlendLevel);

	//NOTE: We must avoid interpolating to/from an invalid (blended out) maximum near in-focus distance.

	const bool isSourceMaxNearInFocusDistanceValid		= (sourceDofMaxNearInFocusDistanceBlendLevel >= SMALL_FLOAT);
	const bool isDestinationMaxNearInFocusDistanceValid	= (destinationDofMaxNearInFocusDistanceBlendLevel >= SMALL_FLOAT);
	const float sourceDofMaxNearInFocusDistance			= sourceFrame.GetDofMaxNearInFocusDistance();
	const float destinationDofMaxNearInFocusDistance	= destinationFrame.GetDofMaxNearInFocusDistance();

	if(isSourceMaxNearInFocusDistanceValid && isDestinationMaxNearInFocusDistanceValid)
	{
		m_DofMaxNearInFocusDistance = Lerp(blendLevel, sourceDofMaxNearInFocusDistance, destinationDofMaxNearInFocusDistance);
	}
	else if(isSourceMaxNearInFocusDistanceValid)
	{
		m_DofMaxNearInFocusDistance = sourceDofMaxNearInFocusDistance;
	}
	else
	{
		m_DofMaxNearInFocusDistance = destinationDofMaxNearInFocusDistance;
	}

	const float sourceDofMaxBlurRadiusAtNearInFocusLimit	= sourceFrame.GetDofMaxBlurRadiusAtNearInFocusLimit();
	const float destinationMaxBlurRadiusAtNearInFocusLimit	= destinationFrame.GetDofMaxBlurRadiusAtNearInFocusLimit();
	m_DofMaxBlurRadiusAtNearInFocusLimit					= Lerp(blendLevel, sourceDofMaxBlurRadiusAtNearInFocusLimit, destinationMaxBlurRadiusAtNearInFocusLimit);

	const float sourceDofBlurDiskRadiusPowerFactorNear		= sourceFrame.GetDofBlurDiskRadiusPowerFactorNear();
	const float destinationDofBlurDiskRadiusPowerFactorNear	= destinationFrame.GetDofBlurDiskRadiusPowerFactorNear();
	m_DofBlurDiskRadiusPowerFactorNear						= Lerp(blendLevel, sourceDofBlurDiskRadiusPowerFactorNear, destinationDofBlurDiskRadiusPowerFactorNear);

	const float sourceOverriddenFocusDistanceBlendLevel			= sourceFrame.GetDofOverriddenFocusDistanceBlendLevel();
	const float destinationOverriddenFocusDistanceBlendLevel	= destinationFrame.GetDofOverriddenFocusDistanceBlendLevel();
	m_DofOverriddenFocusDistanceBlendLevel						= Lerp(blendLevel, sourceOverriddenFocusDistanceBlendLevel, destinationOverriddenFocusDistanceBlendLevel);

	//NOTE: We must avoid interpolating to/from an invalid (blended out) overridden focus distance.

	const bool isSourceOverriddenFocusDistanceValid			= (sourceOverriddenFocusDistanceBlendLevel >= SMALL_FLOAT);
	const bool isDestinationOverriddenFocusDistanceValid	= (destinationOverriddenFocusDistanceBlendLevel >= SMALL_FLOAT);
	const float sourceOverriddenFocusDistance				= sourceFrame.GetDofOverriddenFocusDistance();
	const float destinationOverriddenFocusDistance			= destinationFrame.GetDofOverriddenFocusDistance();

	if(isSourceOverriddenFocusDistanceValid && isDestinationOverriddenFocusDistanceValid)
	{
		m_DofOverriddenFocusDistance = Lerp(blendLevel, sourceOverriddenFocusDistance, destinationOverriddenFocusDistance);
	}
	else if(isSourceOverriddenFocusDistanceValid)
	{
		m_DofOverriddenFocusDistance = sourceOverriddenFocusDistance;
	}
	else
	{
		m_DofOverriddenFocusDistance = destinationOverriddenFocusDistance;
	}

	const float sourceFocusDistanceIncreaseSpringConstant		= sourceFrame.GetDofFocusDistanceIncreaseSpringConstant();
	const float destinationFocusDistanceIncreaseSpringConstant	= destinationFrame.GetDofFocusDistanceIncreaseSpringConstant();
	m_DofFocusDistanceIncreaseSpringConstant					= Lerp(blendLevel, sourceFocusDistanceIncreaseSpringConstant, destinationFocusDistanceIncreaseSpringConstant);

	const float sourceFocusDistanceDecreaseSpringConstant		= sourceFrame.GetDofFocusDistanceDecreaseSpringConstant();
	const float destinationFocusDistanceDecreaseSpringConstant	= destinationFrame.GetDofFocusDistanceDecreaseSpringConstant();
	m_DofFocusDistanceDecreaseSpringConstant					= Lerp(blendLevel, sourceFocusDistanceDecreaseSpringConstant, destinationFocusDistanceDecreaseSpringConstant);

	const float sourceFullScreenBlurBlendLevel		= sourceFrame.GetDofFullScreenBlurBlendLevel();
	const float destinationFullScreenBlurBlendLevel	= destinationFrame.GetDofFullScreenBlurBlendLevel();
	m_DofFullScreenBlurBlendLevel					= Lerp(blendLevel, sourceFullScreenBlurBlendLevel, destinationFullScreenBlurBlendLevel);
}

float camFrame::ComputeHeadingDeltaBetweenFronts(const Vector3& front1, const Vector3& front2)
{
	float heading1	= ComputeHeadingFromFront(front1);
	float heading2	= ComputeHeadingFromFront(front2);
	float delta		= fwAngle::LimitRadianAngle(heading2 - heading1);

	return delta;
}

float camFrame::ComputePitchDeltaBetweenFronts(const Vector3& front1, const Vector3& front2)
{
	float pitch1	= ComputePitchFromFront(front1);
	float pitch2	= ComputePitchFromFront(front2);
	float delta		= fwAngle::LimitRadianAngle(pitch2 - pitch1);

	return delta;
}

void camFrame::SlerpOrientation(float t, const Matrix34& sourceWorldMatrix, const Matrix34& destinationWorldMatrix, Matrix34& resultWorldMatrix, bool shouldMaintainDirection,
	Quaternion* previousOrientationDelta)
{
	Quaternion sourceOrientation;
	sourceOrientation.FromMatrix34(sourceWorldMatrix);
	Quaternion destinationOrientation;
	destinationOrientation.FromMatrix34(destinationWorldMatrix);

	if(sourceOrientation.Dot(destinationOrientation) < 0.0f)
	{
 		destinationOrientation.Negate();
	}

	if(shouldMaintainDirection && previousOrientationDelta)
	{
		Quaternion orientationDelta;
		orientationDelta.MultiplyInverse(destinationOrientation, sourceOrientation);

		if(orientationDelta.Dot(*previousOrientationDelta) < 0.0f)
		{
			destinationOrientation.Negate();
		}
	}

	Quaternion resultOrientation;
	resultOrientation.Slerp(t, sourceOrientation, destinationOrientation);

	if(previousOrientationDelta)
	{
		previousOrientationDelta->MultiplyInverse(destinationOrientation, sourceOrientation);
	}

	resultWorldMatrix.FromQuaternion(resultOrientation);
}

#if __BANK
// Optionally (debug) render each component of the camera frame.
void camFrame::DebugRender(bool shouldRenderExtraInfo) const
{
	grcDebugDraw::Axis(m_WorldMatrix, 0.3f);
	DebugRenderFrustum(g_DebugRenderFrustumColour);

	if(shouldRenderExtraInfo)
	{
		if(camManager::ms_ShouldDebugRenderCameraFarClip)
		{
			DebugRenderFarClip(g_DebugRenderClipColour);
		}

		if(camManager::ms_ShouldDebugRenderCameraNearDof)
		{
			DebugRenderNearDof(g_DebugRenderDofColour);
		}

		if(camManager::ms_ShouldDebugRenderCameraFarDof)
		{
			DebugRenderFarDof(g_DebugRenderDofColour);
		}
	}
}

//Render the plane intersections of the frustum (calculates the plane intersections strictly to ensure this chunk of data is valid in its own
//right entirely.)
//Note: This function now creates a frustum object on the stack each time it's called.
void camFrame::DebugRenderFrustum(const Color32 colour) const
{
	//Assume the game viewport here for simplicity, as most cameras we will want to debug render will be propagating to this viewport.
	const CViewport* gameViewport = gVpMan.GetGameViewport();
	if(gameViewport)
	{
		const grcViewport& viewport = gameViewport->GetGrcViewport();

		//Clone this frame and override the near and far clip to generate a visually meaningful frustum, given that the actual clip planes are
		//rendered separately.
		camFrame cloneFrame;
		cloneFrame.CloneFrom(*this);
		cloneFrame.SetFarClip(0.3f);

		CFrustumDebug frustum(cloneFrame.GetWorldMatrix(), cloneFrame.GetNearClip(), cloneFrame.GetFarClip(), cloneFrame.GetFov(), (float)viewport.GetWidth(), (float)viewport.GetHeight());
		CFrustumDebug::DebugRender(&frustum, colour);
	}
}

void camFrame::DebugRenderNearClip(const Color32 colour) const
{
	const float size = 1.0f; //Arbitrary size to draw what is an infinite plane.
	const float distance = GetNearClip();
	DebugRenderPlane(colour, distance, size);
}

void camFrame::DebugRenderFarClip(const Color32 colour) const
{
	const float size = 1000.0f; //Arbitrary size to draw what is an infinite plane.
	const float distance = GetFarClip();
	DebugRenderPlane(colour, distance, size);
}

void camFrame::DebugRenderNearDof(const Color32 colour) const
{
	const float size = 1.0f; //Arbitrary size to draw what is an infinite plane.
	const float distance = GetNearInFocusDofPlane();
	DebugRenderPlane(colour, distance, size);
}

void camFrame::DebugRenderFarDof(const Color32 colour) const
{
	const float size = 1000.0f; //Arbitrary size to draw what is an infinite plane.
	const float distance = GetFarInFocusDofPlane();
	DebugRenderPlane(colour, distance, size);
}

//Renders a plane at a given distance to the camera, the scale is just an arbitrary value not designed to show how the plane intersects the frustum.
void camFrame::DebugRenderPlane(const Color32 colour, const float distance, const float scale) const
{
	const u32 numVerts = 4;
	const u32 numPolys = 2;
	const u32 numLines = 4;

	const Vector3 right = m_WorldMatrix.a * scale;
	const Vector3 forward = m_WorldMatrix.b * distance;
	const Vector3 up = m_WorldMatrix.c	* scale;

	const Vector3 quad[numVerts] =
	{
		forward - right + up,	// 0 top left
		forward + right + up,	// 1 top right
		forward + right - up,	// 2 bot right
		forward - right - up	// 3 bot left
	};

	const s32 lines[numLines][2] =
	{
		{0,1},	// 0
		{1,2},	// 1
		{2,3},	// 2
		{3,0}	// 3
	};

	const s32 vi[3 * numPolys] =
	{
		0,1,2,	// 0
		2,3,0	// 1
	};

	//Scale, transform & translate verts.
	Vector3 quadTransformed[numVerts];
	const Vector3& position = m_WorldMatrix.d;
	for(u32 i=0; i<numVerts; i++)
	{
		quadTransformed[i] = quad[i] + position;
	}

	//Render wire frame.
	CFrustumDebug::DebugDrawArrayOfLines(numLines, quadTransformed, (const s32* const )lines, colour, colour);
	//NOW draw immediate mode polys.
	CFrustumDebug::DebugDrawPolys(quadTransformed, vi, numPolys, colour);
}

void camFrame::AddWidgets(bkBank &bank)
{
	const float floatMin = -1000.0f;
	const float floatMax = 1000.0f;
	const float floatInc = 0.001f;

	bank.PushGroup("Depth of field strength mapping", false);
	{
		bank.AddSlider("Out-of-focus scaling at min DOF strength",	(float*)&ms_OutOfFocusScalingAtMinDofStrength,	floatMin, floatMax, floatInc);
		bank.AddSlider("Out-of-focus scaling at max DOF strength",	(float*)&ms_OutOfFocusScalingAtMaxDofStrength,	floatMin, floatMax, floatInc);
	}
	bank.PopGroup(); //Depth of field strength mapping

	bank.PushGroup("Automatic depth of field", false);
	{
		bank.PushGroup("When FOV gets wider", false);
		{
			bank.AddSlider("Near DOF",				(float*)&ms_DofNearWhenFovWider,			floatMin, floatMax, floatInc);
			bank.AddSlider("Far DOF",				(float*)&ms_DofFarWhenFovWider,				floatMin, floatMax, floatInc);
			bank.AddSlider("FOV for near DOF max",	(float*)&ms_FovForDofRangeNearMaxWider,		floatMin, floatMax, floatInc);
			bank.AddSlider("FOV for far DOF max",	(float*)&ms_FovForDofRangeFarMaxWider,		floatMin, floatMax, floatInc);
		}
		bank.PopGroup();

		bank.PushGroup("When FOV gets thinner", false);
		{
			bank.AddSlider("Near DOF",				(float*)&ms_DofNearWhenFovThinner,			floatMin, floatMax, floatInc);
			bank.AddSlider("Far DOF",				(float*)&ms_DofFarWhenFovThinner,			floatMin, floatMax, floatInc);
			bank.AddSlider("FOV for near DOF max",	(float*)&ms_FovForDofRangeNearMaxThinner,	floatMin, floatMax, floatInc);
			bank.AddSlider("FOV for far DOF max",	(float*)&ms_FovForDofRangeFarMaxThinner,	floatMin, floatMax, floatInc);
		}
		bank.PopGroup();
	}
	bank.PopGroup();
}
#endif // __BANK
