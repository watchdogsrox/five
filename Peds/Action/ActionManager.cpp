/////////////////////////////////////////////////////////////////////////////////
// Title	:	ActionManager.cpp
//
// These classes provide support for the "action manager".
// The "action manager" is a system used to simplify selecting and
// performing sequential "actions" in a lifelike and believable manner.
//
// An "action" is a set of movement, animation, and statistic changes that a
// ped might do in response to player/AI input or in response to collisions.
//
// This is used mainly for melee combat and is heavily data driven.
//
// Many of these classes describe the conditions under which a given action
// is suitable and the resulting movement, animation and statistic changes
// from performing that action.
/////////////////////////////////////////////////////////////////////////////////
#include "ActionManager.h"

// Rage headers
#include "string/string.h"
#include "input/mapper.h"
#include "bank/bank.h"
#include "parser/manager.h"
#include "math/angmath.h"

// Framework headers
#include "ai/aichannel.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/random.h"
#include "fwmaths/geomutil.h"

// Game headers
#include "Animation/MovePed.h"
#include "Animation/AnimManager.h"
#include "Animation/AnimDefines.h"
#include "control/gamelogic.h"
#include "camera/helpers/Collision.h"
#include "debug/debugscene.h"
#include "modelinfo/PedModelInfo.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "peds/ped.h"
#include "peds/ped_channel.h"
#include "Peds/pedintelligence.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedFlagsMeta.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/PedMoveBlend/PedMoveBlendBase.h"
#include "Peds/PedType.h"
#include "Peds/PlayerInfo.h"
#include "Peds/HealthConfig.h"
#include "Physics/GtaMaterialManager.h"
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"
#include "scene/EntityIterator.h"
#include "streaming/streaming.h"
#include "Task/Combat/CombatMeleeDebug.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task\Motion\Locomotion\TaskInWater.h"
#include "task/motion/Locomotion/TaskMotionPed.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "vehicles/vehicle.h"
#include "weapons/WeaponManager.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"

// Parser headers
#include "ActionManager_parser.h"

AI_OPTIMISATIONS()

extern const char* parser_eAnimBoneTag_Strings[];

#define PRINT_MEM_USAGE __DEV && 0

#if __BANK
	#define actionAssertf(condition, msg, ...)	Assertf( condition, msg, __VA_ARGS__ )
	#define actionPrint(display, msg)		if( display ) Printf( msg )
	#define actionPrintf(display, msg, ...)	if( display ) Printf( msg, __VA_ARGS__ )
#else
	#define actionAssertf(condition, msg, ...)
	#define actionPrint(display, msg)
	#define actionPrintf(display, msg, ...)
#endif

/////////////////////////////////////////////////////////////////////////////////
// Global helper functions.
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DegToRadMoreAccurate
// PURPOSE	:	Helper function to avoid problems with deg to rad conversion on
//				360 vs. PS3 due to slight floating point operation differences.
// PARAMS	:	fAngleInDegrees - the angle to convert to radians.
// RETURNS	:	The angle in radians.
/////////////////////////////////////////////////////////////////////////////////
float DegToRadMoreAccurate( float fAngleInDegrees )
{
	if( fAngleInDegrees == 180.0f )
		return PI;
	else if( fAngleInDegrees == -180.0f )
		return -PI;

	return ( fAngleInDegrees * DtoR );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RadToDegMoreAccurate
// PURPOSE	:	Helper function to avoid problems with deg to rad conversion on
//				360 vs. PS3 due to slight floating point operation differences.
// PARAMS	:	fAngleInRadians - the angle to convert to degrees.
// RETURNS	:	The angle in degrees.
/////////////////////////////////////////////////////////////////////////////////
float RadToDegMoreAccurate( float fAngleInRadians )
{
	if( fAngleInRadians == 1.57 )
		return 180.0f;
	else if( fAngleInRadians == -1.57 )
		return -180.0f;

	return ( fAngleInRadians * RtoD );
}

/////////////////////////////////////////////////////////////////////////////////
// CActionTestCache
//
// This class is intended to speed up the testing of action conditions.  It
// does this by caching previous tests intermediate results for future use.
// All members are public to enhance the classes simplicity and usefulness.
// It is cleared at the begging of each action table sweep.
/////////////////////////////////////////////////////////////////////////////////
CActionTestCache::CActionTestCache()
	:m_bHaveHeightDiff				(false)
	,m_fHeightDiff					(0.0f)
	,m_bHaveDistance				(false)
	,m_fDistanceDiff				(0.0f)
	,m_bHaveAngleDiff				(false)
	,m_fAngularDiff					(0.0f)
	,m_bHaveGroundHeightDiff		(false)
	,m_fGroundHeightDiff			(0.0f)
	,m_bHaveViewerGroundHeight		(false)
	,m_fViewerGroundHeight			(0.0f)
	,m_bHavePedLosTest				(false)
	,m_bHavePedLosTestSimple		(false)
	,m_bPedLosTestClear				(false)
	,m_bPedLosTestClearSimple		(false)
	,m_bHaveFOVTest					(false)
	,m_bFOVClear					(false)
	,m_bHaveSurfaceCollisionResults	(false)
	,m_bFoundBreakableSurface		(false)
	,m_bFoundVerticalSurface		(false)
	,m_bFoundHorizontalSurface		(false)
	,m_bHaveFriendlyTargetTestResults(false)
	,m_bFriendlyIsInDesiredDirection (false)
	,m_bHaveAiTargetTestResults		(false)
	,m_bAiIsInDesiredDirection		(false)
	,m_eEstimatePose				(POSE_UNKNOWN)

{
	m_fRandomProbability = fwRandom::GetRandomNumberInRange( 0.0f, 1.0f );
}	

/////////////////////////////////////////////////////////////////////////////////
// CRelativeRange
//
// This class is intended to help simplify relative position and homing tests.
/////////////////////////////////////////////////////////////////////////////////
CRelativeRange::CRelativeRange()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsViewerLocalPositionInRange
// PURPOSE	:	Test whether or not the viewed position is in the ranges specified.
// PARAMS	:	pViewerPed - the entity doing the test.
//				pViewedEntity - the entity to check if is in range.
//				targetTypeToGetPosDirFrom - the thing being targeted (head, body, etc.)
//				pReusableResults - a cached set of results to speed up calculations.
//				bDisplayDebug - Used to determine if this test is being debugged
// RETURNS	:	Whether or not the entity is in the ranges specified.
/////////////////////////////////////////////////////////////////////////////////
bool CRelativeRange::IsViewerLocalPositionInRange(const CPed*						pViewerPed,
												  const float						fRootOffsetZ,
												  const CEntity* 					pViewedEntity,
												  CActionTestCache*					pReusableResults,
												  const float						fPredictedTargetTime,
												  const bool						BANK_ONLY( bDisplayDebug ) ) const
{
	if( !pViewerPed )
	{
		actionPrint( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Invalid viewer ped\n" );
		return false;
	}

	if( !pViewedEntity )
	{
		actionPrint( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Invalid viewed entity\n" );
		return false;
	}

	Vec3V v3CurrentViewedPos( VEC3_ZERO );
	Vec3V v3CurrentViewedDir( VEC3_ZERO );
	
	Vec3V v3ViewerPos = pViewerPed->GetTransform().Transform3x3( Vec3V( 0.0f, 0.0f, fRootOffsetZ ) );
	v3ViewerPos += pViewerPed->GetTransform().GetPosition();

	CActionFlags::TargetType targetType = CActionFlags::TT_ENTITY_ROOT;
	const CPed* pViewedPed = NULL;
	if( pViewedEntity->GetIsTypePed() )
	{
		pViewedPed = static_cast<const CPed*>( pViewedEntity );
		targetType = CActionFlags::TT_PED_NECK;
	}

	int nBoneIdxCache = -1;
	if( !CActionManager::TargetTypeGetPosDirFromEntity( v3CurrentViewedPos,
														v3CurrentViewedDir,
														targetType,
														pViewedEntity,
														pViewerPed,
														v3ViewerPos,
														fPredictedTargetTime,
														nBoneIdxCache ) )
	{
		actionPrint( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Unable to attain position from entity\n" );
		return false;
	}

	// Ground height is useful to test the targets height from the ground to allow moves that can be performed on a slope
	if( GetCheckGroundHeightRange() )
	{
		// Super special case for get up task as the ped root is always warped at the start of the task
		bool bSkipCheck = false;
		if( pViewedPed )
		{
			if( ShouldAllowPedDeadGroundOverride() )
				bSkipCheck |= ( pViewedPed->ShouldBeDead() || pViewedPed->GetPedResetFlag( CPED_RESET_FLAG_KnockedToTheFloorByPlayer ) );
		}

		if( !bSkipCheck )
		{
			// Check if the height calculations have been cached previously.
			if( pReusableResults && pReusableResults->m_bHaveGroundHeightDiff )
			{
				// Use them for this test.
				if( !IsGroundHeightInRange( pReusableResults->m_fGroundHeightDiff ) )
				{
					actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Ground height is not in range (difference=%f range[%f,%f])\n", pReusableResults->m_fGroundHeightDiff, m_GroundHeightRange.x, m_GroundHeightRange.y );
					return false;
				}
				else
				{
					actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Ground height is in range (difference=%f range[%f,%f])\n", pReusableResults->m_fGroundHeightDiff, m_GroundHeightRange.x, m_GroundHeightRange.y );
				}
			}
			else
			{
				static dev_float sfProbePositionZOffset = -10.0f;
				float fViewerGroundHeight = CActionManager::GetEntityGroundHeight( pViewerPed, v3ViewerPos, sfProbePositionZOffset );
				if( pReusableResults )
				{
					if( !pReusableResults->m_bHaveViewerGroundHeight )
					{
						pReusableResults->m_bHaveViewerGroundHeight = true;
						pReusableResults->m_fViewerGroundHeight = fViewerGroundHeight;
					}
					else 
						fViewerGroundHeight = pReusableResults->m_fViewerGroundHeight;
				}

				float fGroundHeight = CActionManager::GetEntityGroundHeight( pViewedEntity, v3CurrentViewedPos, sfProbePositionZOffset );
				float fGroundHeightDiff = fGroundHeight - fViewerGroundHeight;
				
				// Cache the results of this calculation.
				if( pReusableResults )
				{
					pReusableResults->m_bHaveGroundHeightDiff = true;
					pReusableResults->m_fGroundHeightDiff = fGroundHeightDiff;
				}
				
				// Do the test.
				if( !IsGroundHeightInRange( fGroundHeightDiff ) )
				{
					actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Ground height is not in range (difference=%f range[%f,%f])\n", fGroundHeightDiff, m_GroundHeightRange.x, m_GroundHeightRange.y );
					return false;
				}
				else
				{
					actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Ground height is in range (difference=%f range[%f,%f])\n", fGroundHeightDiff, m_GroundHeightRange.x, m_GroundHeightRange.y );
				}
			}
		}
	}

	if( GetCheckDistanceRange() )
	{
		// Check if the distance calculations have been cached previously.
		if( pReusableResults && pReusableResults->m_bHaveDistance )
		{
			// Use them for this test.
			if( !IsDistanceInRange( pReusableResults->m_fDistanceDiff ) )
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Distance is not in range (difference=%f range[%f,%f])\n", pReusableResults->m_fDistanceDiff, m_DistanceRange.x, m_DistanceRange.y );
				return false;
			}
			else
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Distance is in range (difference=%f range[%f,%f])\n", pReusableResults->m_fDistanceDiff, m_DistanceRange.x, m_DistanceRange.y );
			}
		}
		else
		{
			// Do the calculations to get the distance.
			Vec3V v3ViewerToViewedDeltaXY = v3CurrentViewedPos - v3ViewerPos;
			v3ViewerToViewedDeltaXY.SetZf( 0.0f );
			float fViewerToViewedDistanceXY = Mag( v3ViewerToViewedDeltaXY ).Getf();

			// Cache the results of this calculation.
			if( pReusableResults )
			{
				pReusableResults->m_bHaveDistance = true;
				pReusableResults->m_fDistanceDiff = fViewerToViewedDistanceXY;
			}

			// Do the test.
			if( !IsDistanceInRange( fViewerToViewedDistanceXY ) )
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Distance is not in range (difference=%f range[%f,%f])\n", fViewerToViewedDistanceXY, m_DistanceRange.x, m_DistanceRange.y );
				return false;
			}
			else
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Distance is in range (difference=%f range[%f,%f])\n", fViewerToViewedDistanceXY, m_DistanceRange.x, m_DistanceRange.y );
			}
		}
	}

	if( GetCheckPlanarAngularRange() )
	{
		// Check if the heading calculations have been cached previously.
		if( pReusableResults && pReusableResults->m_bHaveAngleDiff )
		{
			// Use them for this test.
			if( !IsAngleInRange( pReusableResults->m_fAngularDiff ) )
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Angle is not in range (difference=%f range[%f,%f])\n", pReusableResults->m_fAngularDiff, m_PlanarAngularRange.x, m_PlanarAngularRange.y );
				return false;
			}
			else
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Angle is in range (difference=%f range[%f,%f])\n", pReusableResults->m_fAngularDiff, m_PlanarAngularRange.x, m_PlanarAngularRange.y );
			}
		}
		else
		{
			// Compare view angles, making sure to handle special cases of wraparound
			// and angular values that span across the zero line.
			const float	fViewerHeadingRads = fwAngle::LimitRadianAngle( pViewerPed->GetTransform().GetHeading() );
			const float	fViewerToViewedHeadingRads = fwAngle::LimitRadianAngle( rage::Atan2f( -v3CurrentViewedDir.GetXf(), v3CurrentViewedDir.GetYf() ) );
			float fViewerToViewedAngleDiffRads = fwAngle::LimitRadianAngle( fViewerToViewedHeadingRads - fViewerHeadingRads );

			// Cache the results of this calculation.
			if( pReusableResults )
			{
				pReusableResults->m_bHaveAngleDiff = true;
				pReusableResults->m_fAngularDiff = fViewerToViewedAngleDiffRads;
			}

			// Do the test.
			if( !IsAngleInRange( fViewerToViewedAngleDiffRads ) )
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Angle is not in range (difference=%f range[%f,%f])\n", fViewerToViewedAngleDiffRads, m_PlanarAngularRange.x, m_PlanarAngularRange.y );
				return false;
			}
			else
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Angle is in range (difference=%f range[%f,%f])\n", fViewerToViewedAngleDiffRads, m_PlanarAngularRange.x, m_PlanarAngularRange.y );
			}
		}
	}

	// Height difference range is used for moves that have strict guidelines on how high/low above the target they can be (synced animations)
	if( GetCheckHeightRange() )
	{
		// Check if the height calculations have been cached previously.
		if( pReusableResults && pReusableResults->m_bHaveHeightDiff )
		{
			// Use them for this test.
			if( !IsHeightInRange( pReusableResults->m_fHeightDiff ) )
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Height is not in range (difference=%f range[%f,%f])\n", pReusableResults->m_fHeightDiff, -m_HeightDifference, m_HeightDifference );
				return false;
			}
			else
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Height is in range (difference=%f range[%f,%f])\n", pReusableResults->m_fHeightDiff, -m_HeightDifference, m_HeightDifference );
			}
		}
		else
		{
			// Do the calculations to get the height difference.
			Vec3V v3ViewerToViewedDelta = v3CurrentViewedPos - v3ViewerPos;
			float fHeightDiff = v3ViewerToViewedDelta.GetZf();

			// Cache the results of this calculation.
			if( pReusableResults )
			{
				pReusableResults->m_bHaveHeightDiff = true;
				pReusableResults->m_fHeightDiff = fHeightDiff;
			}

			// Do the test.
			if( !IsHeightInRange( fHeightDiff ) )
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (FAILED) - Height is not in range (difference=%f range[%f,%f])\n", fHeightDiff, -m_HeightDifference, m_HeightDifference );
				return false;
			}
			else
			{
				actionPrintf( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS) - Height is in range (difference=%f range[%f,%f])\n", fHeightDiff, -m_HeightDifference, m_HeightDifference );
			}
		}
	}

	actionPrint( bDisplayDebug, "CRelativeRange::IsViewerLocalPositionInRange (SUCCESS)\n" );

	// The ped must be within the area since it is within all the
	// ranges specified, so let the user know.
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CRelativeRange::PostLoad( void )
{
	// Fix up ranges to always define the lowest value first
	if( m_GroundHeightRange.x > m_GroundHeightRange.y )
		SwapEm( m_GroundHeightRange.x, m_GroundHeightRange.y );

	if( m_DistanceRange.x > m_DistanceRange.y )
		SwapEm( m_DistanceRange.x, m_DistanceRange.y );

	// Fix up height difference to always be positive
	m_HeightDifference = Abs( m_HeightDifference );

	// Set and fix up the angle range.
	// The angles always sweep from A to B in a right handed (anti-clockwise) fashion.
	// Keep their current order since that lets up know which area to be swept
	// out (of the two possibilities).
	// Make sure both angles are on positive -pi to pi.
	m_PlanarAngularRange.x = fwAngle::LimitRadianAngle( DegToRadMoreAccurate( m_PlanarAngularRange.x ) );
	m_PlanarAngularRange.y = fwAngle::LimitRadianAngle( DegToRadMoreAccurate( m_PlanarAngularRange.y ) );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CRelativeRange::Reset( void )
{
	m_GroundHeightRange.Zero();
	m_DistanceRange.Zero();
	m_PlanarAngularRange.Zero();
	m_HeightDifference = 0.0f;
	m_RelativeRangeAttrs.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsGroundHeightInRange
// PURPOSE	:	Compare ground heights; check if is either above or below the 
//				ground height difference.
// PARAMS	:	fViewerToViewedGroundHeightDiff - the current ground height difference.
// RETURNS	:	Whether or not the entity is in the range specified.
/////////////////////////////////////////////////////////////////////////////////
bool CRelativeRange::IsGroundHeightInRange( const float fViewerToViewedGroundHeightDiff ) const
{
	if( fViewerToViewedGroundHeightDiff < m_GroundHeightRange.x || 
		fViewerToViewedGroundHeightDiff > m_GroundHeightRange.y )
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsDistanceInRange
// PURPOSE	:	Compare distance; check if it is either closer or farther than
//				the distance range specified.
// PARAMS	:	fViewerToViewedDistanceXY - the current distance.
// RETURNS	:	Whether or not the entity is in the range specified.
/////////////////////////////////////////////////////////////////////////////////
bool CRelativeRange::IsDistanceInRange( const float fViewerToViewedDistanceXY ) const
{
	if( fViewerToViewedDistanceXY < m_DistanceRange.x || 
		fViewerToViewedDistanceXY > m_DistanceRange.y )
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsAngleInRange
// PURPOSE	:	Compare view angles, making sure to handle special cases of
//				wraparound and angular values that span across the -PI/PI line.
// PARAMS	:	fViewerToViewedAngleDiffRads - the current angular difference.
// RETURNS	:	Whether or not the entity is in the range specified.
/////////////////////////////////////////////////////////////////////////////////
bool CRelativeRange::IsAngleInRange( const float fViewerToViewedAngleDiffRads ) const
{
	const float testAngleRads = fwAngle::LimitRadianAngle( fViewerToViewedAngleDiffRads );

	// Test the angle to see if it is in the swept range.
	// The angular sweep should always go from x to y in a counter clockwise fashion.
	if( m_PlanarAngularRange.x <= m_PlanarAngularRange.y )
	{
		if(	testAngleRads >= m_PlanarAngularRange.x &&
			testAngleRads <= m_PlanarAngularRange.y )
			return true;
	}
	else
	{
		if(	testAngleRads >= m_PlanarAngularRange.x ||
			testAngleRads <= m_PlanarAngularRange.y )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsHeightInRange
// PURPOSE	:	Compare heights; check if is either above or below the height
//				range specified.
// PARAMS	:	fViewerToViewedHeightDiff - the current height difference.
// RETURNS	:	Whether or not the entity is in the range specified.
/////////////////////////////////////////////////////////////////////////////////
bool CRelativeRange::IsHeightInRange( const float fViewerToViewedHeightDiff ) const
{
	if( fViewerToViewedHeightDiff < -m_HeightDifference || 
		fViewerToViewedHeightDiff >  m_HeightDifference )
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// CHoming
//
// This class is intended to help with the placement and moving of peds while
// the perform actions.  This is so that actions can achieve believable looking
// results without having to have absurdly tight trigger conditions.
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Apply
// PURPOSE	:	Make the ped move according to the position and direction of the
//				target entity and the desired position and direction of it.
// PARAMS	:	pHomingPed - the ped that is possibly going to be moved.
//				pTargetEntity - Entity we are targeting and would like to home into
//				rvHomingPosition - Position we would like to home into
//				rvHomingDirection - Direction we would like to update heading towards
//				vMoverDisplacement - Total animated mover displacement that will be used to calculate the ideal position
//				rnBoneCacheIdx - Used to cache off a bone index which is used over multiple frames
//				pClip - the anim currently being played (as it may do some movement itself).
//				fCurrentPhase - Current phase of the pClip
//				bPerformDistanceHoming - Whether or not we should perform distance homing changes
//				bDidPerformDistanceHoming - Whether or not the distance homing was applied last frame
//				bPerformHeadingHoming - Whether or not we should perform heading homing changes
//				bHasStruckSomething - Whether or not we have struck anything yet
//				bUpdateDesiredHeading - Should we force the calculated desired heading on the homing ped?
//				fTimeStep - Physics time step
//				nTimeSlice - Time slice
/////////////////////////////////////////////////////////////////////////////////
void CHoming::Apply( CPed* pHomingPed, const CEntity* pTargetEntity, Vec3V_InOut rvHomingPosition, Vec3V_InOut rvHomingDirection, Vec3V_In vMoverDisplacement, int& rnBoneCacheIdx, const crClip* pClip, const float fCurrentPhase, bool bPerformDistanceHoming, bool bDidPerformDistanceHoming, bool bPerformHeadingHoming, bool bHasStruckSomething, bool bUpdateDesiredHeading, const float fTimeStep, const int nTimeSlice, Vec3V_In vCachedPedPosition )
{
	if( !pHomingPed  )
		return;

	const Matrix34 homingPedMat(MAT34V_TO_MATRIX34(pHomingPed->GetTransform().GetMatrix()));

	float fEndPhase = 0.0f;
	float fCriticalFrameTimeRemaining = 0.0f;
	float fHeadingUpdateTimeRemaining = 0.0f;
	Vector3 v3MoverDisplacement(Vector3::ZeroType);
	float fMoverRotationDelta = 0.0f;
	if( pClip )
	{
		// Homing is applied so we should be in the correct position by the critical frame, or the start of the melee collision tag.  Then we continue homing up to the end of the homing tag
		const crTag* pTag = CClipEventTags::FindFirstEventTag( pClip, CClipEventTags::CriticalFrame );
		if( !pTag )
			pTag = CClipEventTags::FindFirstEventTag( pClip, CClipEventTags::MeleeCollision );

		if( pTag )
		{
			fEndPhase = pTag->GetStart();
			if( fCurrentPhase > fEndPhase )
			{
				// If we have gone past our tag, which is the case if we want to continue homing, move the end phase on one frame
				fEndPhase = fCurrentPhase + fTimeStep * pClip->GetDuration();
			}

			fCriticalFrameTimeRemaining  = pClip->ConvertPhaseToTime( MAX( fEndPhase - fCurrentPhase, 0.0f ) );
			fCriticalFrameTimeRemaining -= fTimeStep * nTimeSlice;
			fCriticalFrameTimeRemaining  = Max( fCriticalFrameTimeRemaining, 0.0f );
		}

		// Get the translation
		v3MoverDisplacement = fwAnimHelpers::GetMoverTrackDisplacement( *pClip, fCurrentPhase, fEndPhase );
		// Transform into ped space
		homingPedMat.Transform3x3( v3MoverDisplacement );

		// And rotation
		Quaternion q = fwAnimHelpers::GetMoverTrackRotationDelta( *pClip, fCurrentPhase, fEndPhase );
		Vector3 vEulers;
		q.ToEulers( vEulers );
		fMoverRotationDelta = vEulers.z;

		// We only care about lerped heading update if we have a critical frame
		if( fCriticalFrameTimeRemaining > 0.0f )
		{
			pTag = CClipEventTags::FindFirstEventTag( pClip, CClipEventTags::MeleeHoming );
			if( pTag )
			{
				float fMeleeHomingEndPhase = pTag->GetEnd();
				if( fCurrentPhase < fMeleeHomingEndPhase )
				{
					fHeadingUpdateTimeRemaining  = pClip->ConvertPhaseToTime( MAX( fMeleeHomingEndPhase - fCurrentPhase, 0.0f ) );
					fHeadingUpdateTimeRemaining -= fTimeStep * nTimeSlice;
					fHeadingUpdateTimeRemaining  = Max( fHeadingUpdateTimeRemaining, 0.0f );
				}
			}
			else 
				fHeadingUpdateTimeRemaining = fCriticalFrameTimeRemaining;
		}
	}

	// Should we cache off a new homing position?
	if( !ShouldUseCachedPosition() )
	{
		if( !CActionManager::TargetTypeGetPosDirFromEntity( rvHomingPosition,
															rvHomingDirection,
															GetTargetType(),
															pTargetEntity,
															pHomingPed,
															GetCheckTargetDistance() ? vCachedPedPosition : pHomingPed->GetTransform().GetPosition(),
															fCriticalFrameTimeRemaining,
															rnBoneCacheIdx ) )
		{
			// Default to use the target entity position
			rvHomingPosition = pTargetEntity->GetTransform().GetPosition();
			rvHomingDirection = rvHomingPosition - pHomingPed->GetTransform().GetPosition();
		}
	}

	bool bIgnoreAnimDisplacement = false;
	if( ShouldMaintainIdealDistance() )
	{
		float fDist = Mag(rvHomingPosition - VECTOR3_TO_VEC3V( homingPedMat.d)).Getf();
		bool bTooClose = fDist < GetTargetOffset().y;
		bIgnoreAnimDisplacement = bTooClose && !bPerformDistanceHoming && bHasStruckSomething;
	}

	// Collect the paired homing offset
	Vec3V v3DesiredWorldPosition( VEC3_ZERO );
	float fDesiredHeadingRads = 0.0f;
	CalculateHomingWorldOffset( pHomingPed, rvHomingPosition, rvHomingDirection, vMoverDisplacement, v3DesiredWorldPosition, fDesiredHeadingRads, false, bIgnoreAnimDisplacement );

	if( bUpdateDesiredHeading )
		pHomingPed->SetDesiredHeading( fDesiredHeadingRads );

#if __DEV
	if( CCombatMeleeDebug::sm_bDrawHomingDesiredPosDir )
	{
		if( (CCombatMeleeDebug::sm_bVisualisePlayer && pHomingPed->IsLocalPlayer()) ||
			(CCombatMeleeDebug::sm_bVisualiseNPCs && !pHomingPed->IsLocalPlayer()) )
		{
			CCombatMeleeDebug::sm_debugDraw.AddSphere( v3DesiredWorldPosition, 0.1f, (GetCheckTargetDistance() && bPerformDistanceHoming) ? Color_green : Color_red, 1 );

			Vec3V vDesiredHeading( -rage::Sinf( fDesiredHeadingRads ), rage::Cosf( fDesiredHeadingRads ), 0.0f );
			CCombatMeleeDebug::sm_debugDraw.AddArrow( v3DesiredWorldPosition, ( v3DesiredWorldPosition + vDesiredHeading ), 0.5f, (GetCheckTargetHeading() && bPerformHeadingHoming) ? Color_green : Color_red, 1 );

			if(GetCheckTargetDistance())
				CCombatMeleeDebug::sm_debugDraw.AddSphere( vCachedPedPosition, 0.1f, Color_orange, 1 );
			else
				CCombatMeleeDebug::sm_debugDraw.AddSphere( pHomingPed->GetTransform().GetPosition(), 0.1f, Color_blue, 1 );
		}
	}
#endif

	// Try to get our desires.
	ApplyWorldPosAndHeadingHoming( pHomingPed,
								   v3DesiredWorldPosition,
								   fDesiredHeadingRads, 
								   fCriticalFrameTimeRemaining,
								   fHeadingUpdateTimeRemaining,
								   RCC_VEC3V( v3MoverDisplacement ),
								   fMoverRotationDelta,
								   bPerformDistanceHoming,
								   bDidPerformDistanceHoming,
								   bPerformHeadingHoming,
								   fTimeStep );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CalculateHomingWorldOffset
// PURPOSE	:	Determine, in world space coords, where we want to move the 
//				designated homing ped.
// PARAMS	:	pHomingPed - Ped which we would like to move
//				vHomingPosition - Position we would like to home to
//				vHomingDirection - Direction we would like to achieve
//				vMoverDisplacment - Precalculated animated mover displacement 
//				rvDesiredWorldPosition - Returns the desired homing position
//				rfDesiredHeading - Returns the desired heading
//				bIgnoreXOffset - Whether or not we want to ignore the data driven x offset
//				bIgnoreAnimDisplacement - Whether or not we want to ignore the anim displacement compensation
/////////////////////////////////////////////////////////////////////////////////
void CHoming::CalculateHomingWorldOffset( CPed* pHomingPed, Vec3V_In vHomingPosition, Vec3V_In vHomingDirection, Vec3V_In vMoverDisplacment, Vec3V_InOut rvDesiredWorldPosition, float& rfDesiredHeading, const bool bIgnoreXOffset, const bool bIgnoreAnimDisplacement )
{
	Assert( pHomingPed );

	Vec3V vHomingPositionToPed( Negate( vHomingDirection ) );
	float fHomingPositionToPedHeading = fwAngle::LimitRadianAngle( rage::Atan2f( -vHomingPositionToPed.GetXf(), vHomingPositionToPed.GetYf() ) );

	if( GetCheckTargetDistance() )
	{		
		// Initialize the x/y offset
		rvDesiredWorldPosition = Vec3V( bIgnoreXOffset ? 0.0f : m_TargetOffset.x, m_TargetOffset.y, 0.0f );

		// Remove mover displacement so we do not double the translation
		if( !bIgnoreAnimDisplacement )
		{
			if( ShouldUseDisplacementMagnitude() )
				rvDesiredWorldPosition.SetYf( rvDesiredWorldPosition.GetYf() - MagXY( vMoverDisplacment ).Getf() );
			else
			{
				rvDesiredWorldPosition.SetYf( rvDesiredWorldPosition.GetYf() - vMoverDisplacment.GetYf() );
				rvDesiredWorldPosition.SetXf( rvDesiredWorldPosition.GetXf() - vMoverDisplacment.GetXf() );
			}			
		}

		rvDesiredWorldPosition = RotateAboutZAxis( rvDesiredWorldPosition, ScalarV( fHomingPositionToPedHeading ) );
		rvDesiredWorldPosition += vHomingPosition;
	}
	else
		rvDesiredWorldPosition = vHomingPosition;

	// Determine desired heading
	if( GetCheckTargetHeading() )
	{
		float fBaseHeadingRads = fwAngle::LimitRadianAngle( rage::Atan2f( -vHomingDirection.GetXf(), vHomingDirection.GetYf() ) );
		rfDesiredHeading = fwAngle::LimitRadianAngle( fBaseHeadingRads + DegToRadMoreAccurate( GetTargetHeading() ) );
	}
	else
		rfDesiredHeading = pHomingPed->GetTransform().GetHeading();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ApplyWorldPosAndHeadingHoming
// PURPOSE	:	Move the ped according to our desired world position and
//				direction (calculated above).
// PARAMS	:	pHomingPed - the ped that is possibly going to be moved
//				vWorldPosDesired - the position to try to move toward
//				fWorldHeadingDesired - the heading to try to achieve
//				fCriticalFrameTimeRemaining - Amount of time left to home (seconds)
//				fHeadingUpdateTimeRemaining - Amount of time left to update the heading (seconds)
//				fDistanceRemaining - Animated distance remaining (meters)
//				bPerformDistanceHoming - Whether or not we should perform distance homing
//				bDidPerformDistanceHoming - Whether or not we performed distance homing last frame
//				bPerformHeadingHoming - Whether or not we should update heading
/////////////////////////////////////////////////////////////////////////////////
void CHoming::ApplyWorldPosAndHeadingHoming( CPed* pHomingPed, Vec3V_In vWorldPosDesired, const float fWorldHeadingDesired, float fCriticalFrameTimeRemaining, float fHeadingUpdateTimeRemaining, Vec3V_In vAnimMoverDisplacement, const float fMoverRotationDelta, const bool bPerformDistanceHoming, const bool bDidPerformDistanceHoming, const bool bPerformHeadingHoming, const float fTimeStep )
{
	// Do position homing.
	if( GetCheckTargetDistance() )
	{
		if( bPerformDistanceHoming )
		{
			Vec3V vPedPosition = pHomingPed->GetTransform().GetPosition();
			Vec3V v3WorldDeltaDesired = vWorldPosDesired - vPedPosition;

			// Work out the new desired velocity
			Vector3 vDesiredVel = VEC3V_TO_VECTOR3( pHomingPed->GetTransform().Transform3x3( VECTOR3_TO_VEC3V( pHomingPed->GetAnimatedVelocity() ) ) );

			if( Abs( vAnimMoverDisplacement.GetXf() ) > 0.05f && Sign( vAnimMoverDisplacement.GetXf() ) == Sign( v3WorldDeltaDesired.GetXf() ) )
			{
				// If the animated velocity if moving in the direction we want to move, scale it to try and nail the distance
				vDesiredVel.x *= v3WorldDeltaDesired.GetXf() / vAnimMoverDisplacement.GetXf();
			}
			else
			{
				// Otherwise move their over the time remaining
				vDesiredVel.x = v3WorldDeltaDesired.GetXf() / Max( fCriticalFrameTimeRemaining, fTimeStep );
			}

			if( Abs( vAnimMoverDisplacement.GetYf() ) > 0.05f && Sign( vAnimMoverDisplacement.GetYf() ) == Sign( v3WorldDeltaDesired.GetYf() ) )
			{
				// If the animated velocity if moving in the direction we want to move, scale it to try and nail the distance
				vDesiredVel.y *= v3WorldDeltaDesired.GetYf() / vAnimMoverDisplacement.GetYf();
			}
			else
			{
				// Otherwise move their over the time remaining
				vDesiredVel.y = v3WorldDeltaDesired.GetYf() / Max( fCriticalFrameTimeRemaining, fTimeStep );
			}

			// Z for swimming attacks
			if( pHomingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) )
			{
				if( Abs( vAnimMoverDisplacement.GetZf() ) > 0.05f && Sign( vAnimMoverDisplacement.GetZf() ) == Sign( v3WorldDeltaDesired.GetZf() ) )
				{
					// If the animated velocity if moving in the direction we want to move, scale it to try and nail the distance
					vDesiredVel.z *= v3WorldDeltaDesired.GetZf() / vAnimMoverDisplacement.GetZf();
				}
				else
				{
					// Otherwise move their over the time remaining
					vDesiredVel.z = v3WorldDeltaDesired.GetZf() / Max( fCriticalFrameTimeRemaining, fTimeStep );
				}
			}

			// Clamp to the max speed
			if( vDesiredVel.Mag2() > square(GetMaxSpeed()) )
			{
				taskDebugf1( "%d: Melee: 0x%p Clamping velocity [%.2f][%.2f]", fwTimer::GetFrameCount(), pHomingPed, vDesiredVel.Mag(), GetMaxSpeed() );
				vDesiredVel.Normalize();
				vDesiredVel *= GetMaxSpeed();
			}

			//Account for the ground.
			// NOTE: This must be done before preserving the z-velocity, otherwise we accumulate ground z-velocity each frame
			if( pHomingPed->GetGroundPhysical() )
				vDesiredVel += VEC3V_TO_VECTOR3( pHomingPed->GetGroundVelocityIntegrated() );

			// Preserve any current Z velocity
			if( !pHomingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) && !pHomingPed->GetIsFPSSwimming() )
			{
				vDesiredVel.z = pHomingPed->GetDesiredVelocity().z;
			}

			if(!pHomingPed->IsOnGround() && vDesiredVel.z > 0.0f)
			{
				vDesiredVel.z = 0.0f;
			}

			pHomingPed->SetDesiredVelocity( vDesiredVel );
		}
		else
		{
			// We only want to do this once to square up the desired velocity
			if( bDidPerformDistanceHoming )
			{
				Vector3 vDesiredVel = VEC3V_TO_VECTOR3( pHomingPed->GetTransform().Transform3x3( VECTOR3_TO_VEC3V( pHomingPed->GetAnimatedVelocity() ) ) );
				vDesiredVel.z = pHomingPed->GetDesiredVelocity().z;

				//Account for the ground.
				if( pHomingPed->GetGroundPhysical() )
					vDesiredVel += VEC3V_TO_VECTOR3( pHomingPed->GetGroundVelocityIntegrated() );

				if(!pHomingPed->IsOnGround() && vDesiredVel.z > 0.0f)
				{
					vDesiredVel.z = 0.0f;
				}

				pHomingPed->SetDesiredVelocity( vDesiredVel );
			}
		}
	}

	if( bPerformHeadingHoming && GetCheckTargetHeading() )
	{
		const float fCurrentHeading = fwAngle::LimitRadianAngleSafe( pHomingPed->GetCurrentHeading() );
		float fHeadingDelta = SubtractAngleShorter( fWorldHeadingDesired, fCurrentHeading );

		// Remove the amount we are going to rotate in the animation
		fHeadingDelta = SubtractAngleShorter( fHeadingDelta, fMoverRotationDelta );
		float fHeadingChangeRate = 0.0f;
		if( fHeadingUpdateTimeRemaining > 0.0f )
			fHeadingChangeRate = fHeadingDelta / Max( fHeadingUpdateTimeRemaining, fTimeStep );
		else
		{
			// Lerp the change multiplier to give it a smoother feel
			TUNE_GROUP_FLOAT( MELEE, fHeadingChangeMultiplierMin, 1.5f, 0.0f, 20.0f, 0.001f );
			TUNE_GROUP_FLOAT( MELEE, fHeadingChangeMultiplierMax, 5.0f, 0.0f, 100.0f, 0.001f );

			float fHeadingDeltaABS = Abs( fHeadingDelta );
			float fHeadingRatio = fHeadingDeltaABS / PI;
			fHeadingChangeRate = Lerp( fHeadingRatio, fHeadingChangeMultiplierMin, fHeadingChangeMultiplierMax ) * Sign( fHeadingDelta );

			float fHeadingDeltaClamp = fHeadingDeltaABS / fTimeStep;
			fHeadingChangeRate = Clamp( fHeadingChangeRate, -fHeadingDeltaClamp, fHeadingDeltaClamp);
		}

		// Add to the animated angular velocity
		Vector3 vDesiredAngVel = Vector3( 0.f, 0.0f, pHomingPed->GetAnimatedAngularVelocity() );
		vDesiredAngVel.z += fHeadingChangeRate;

		CTaskMotionDiving* pDiveTask = static_cast<CTaskMotionDiving*>( pHomingPed->GetPedIntelligence()->FindTaskActiveMotionByType( CTaskTypes::TASK_MOTION_DIVING ) );

		if( pDiveTask && pHomingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) )
		{
			Vector3 vPedPosition = VEC3V_TO_VECTOR3( pHomingPed->GetTransform().GetPosition() );
			Vector3 v3WorldDeltaDesired = VEC3V_TO_VECTOR3( vWorldPosDesired ) - vPedPosition;

			static dev_float MIN_HEIGHT_FOR_PITCH = 0.1f;
			if( Abs( v3WorldDeltaDesired.z ) > MIN_HEIGHT_FOR_PITCH )
			{
				Vector2 v3WorldDeltaDesiredXY;
				v3WorldDeltaDesired.GetVector2XY( v3WorldDeltaDesiredXY );

				float fDesiredPitch = Atan2f( v3WorldDeltaDesired.z, v3WorldDeltaDesiredXY.Mag() );
				pHomingPed->SetDesiredPitch(fDesiredPitch);
				
				const float fAngDiff = fwAngle::LimitRadianAngle( fDesiredPitch - pHomingPed->GetTransform().GetPitch() );
				float fPitchVel = fAngDiff / fTimeStep;

				static dev_float MAX_PITCH_CHANGE = 2.5f;
				fPitchVel = Clamp( fPitchVel, -MAX_PITCH_CHANGE, MAX_PITCH_CHANGE );
				vDesiredAngVel += fPitchVel * VEC3V_TO_VECTOR3( pHomingPed->GetTransform().GetA() );
			}
		}

		pHomingPed->SetDesiredAngularVelocity( vDesiredAngVel );
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CHoming::Reset( void )
{
	m_Name.Clear();
	m_TargetType = CActionFlags::TT_NONE;
	m_TargetOffset.Zero();
	m_ActivationDistance = 0.0f;
	m_MaxSpeed = 0.0f;
	m_TargetHeading = 0.0f;
	m_HomingAttrs.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CHoming::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CHoming: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CDamageAndReaction
//
// this class is to package up the damage and reaction for a given action
// result since many results share the same strike bones and damage amounts.
// This should also help make the action results easier to read in Excel.
/////////////////////////////////////////////////////////////////////////////////
CDamageAndReaction::CDamageAndReaction()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CDamageAndReaction::PostLoad( void )
{
	u32 nIndexFound = 0;

	if( GetActionRumbleID() != 0 )
	{
		const CActionRumble* pRumble = ACTIONMGR.FindActionRumble( nIndexFound, GetActionRumbleID() );
		SetActionRumble( const_cast<CActionRumble*>(pRumble) );
		Assertf( m_pRumble, "CDamageAndReaction::PostLoad - Rumble (%s) was not found", GetActionRumbleName() );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CDamageAndReaction::Reset( void )
{
	m_pRumble = NULL;
	m_OnHitDamageAmountMin = 0.0f;
	m_OnHitDamageAmountMax = 0.0f;
	m_SelfDamageAmount = 0.0f;
	m_ForcedSelfDamageBoneTag = BONETAG_INVALID;
	m_DamageReactionAttrs.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CDamageAndReaction::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CDamageAndReaction: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CStrikeBone
//
// this class defines the strike bone info for a given action.
/////////////////////////////////////////////////////////////////////////////////
CStrikeBone::CStrikeBone()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CStrikeBone::Reset( void )
{
	m_StrikeBoneTag = BONETAG_INVALID;
	m_Radius = 0.0f;
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CStrikeBone::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CStrikeBone: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", parser_eAnimBoneTag_Strings[ GetStrikeBoneTag() ], mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CStrikeBoneSet
//
// this class is to package up the strike bone info for a given action.
// This should also help make the action results easier to read in Excel.
/////////////////////////////////////////////////////////////////////////////////
CStrikeBoneSet::CStrikeBoneSet()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetStrikeBoneTag
// PURPOSE	:	Get the BoneTag of the strike bone we can turn on.
// PARAMS	:	nStrikeBoneNum - the bone slot we're interested in.
// RETURNS	:	The AnimBoneTag of the strike bone used in this slot.
/////////////////////////////////////////////////////////////////////////////////
eAnimBoneTag CStrikeBoneSet::GetStrikeBoneTag( const u32 nStrikeBoneNum ) const
{
	Assertf( nStrikeBoneNum < GetNumStrikeBones(), "Strike bone number for GetStrikeBoneTag is greater than allowed maximum." );
	return m_aStrikeBones[nStrikeBoneNum]->GetStrikeBoneTag();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetStrikeBoneRadius
// PURPOSE	:	Get the radius to use when approximating the shape of the strike
//				bone we can turn on.
// PARAMS	:	nStrikeBoneNum - the bone slot we're interested in.
// RETURNS	:	The radius to use.
/////////////////////////////////////////////////////////////////////////////////
float CStrikeBoneSet::GetStrikeBoneRadius( const u32 nStrikeBoneNum ) const
{
	Assertf( nStrikeBoneNum < GetNumStrikeBones(), "Strike bone number for GetStrikeBoneRadius is greater than allowed maximum." );
	return m_aStrikeBones[nStrikeBoneNum]->GetRadius();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CStrikeBoneSet::PostLoad( void )
{
	for( int i = 0; i < m_aStrikeBones.GetCount(); i++ )
		m_aStrikeBones[i]->PostLoad();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CStrikeBoneSet::Reset( void )
{
	m_Name.Clear();
	for(int i = 0; i < m_aStrikeBones.GetCount(); i++)
		m_aStrikeBones[i]->Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CStrikeBoneSet::Clear(void)
{
	for(int i = 0; i < m_aStrikeBones.GetCount(); i++)
	{
		m_aStrikeBones[i]->Clear();
		delete m_aStrikeBones[i];
	}
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CStrikeBoneSet::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	for(int i = 0; i < m_aStrikeBones.GetCount(); i++)
		m_aStrikeBones[i]->PrintMemoryUsage( rTotalSize );
	Printf( "CStrikeBoneSet: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CActionVfx
//
// this class is to package up the vfx animation info for a given action.
/////////////////////////////////////////////////////////////////////////////////
CActionVfx::CActionVfx()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionVfx::Reset( void )
{
	m_Name.Clear();
	m_VfxName.Clear();
	m_Offset.Zero();
	m_Rotation.Zero();
	m_BoneTag = BONETAG_INVALID;
	m_Scale = 1.0f;
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionVfx::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CActionVfx: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CActionFacialAnimSet
// this class is to package up the facial animation info for a given action.
/////////////////////////////////////////////////////////////////////////////////
CActionFacialAnimSet::CActionFacialAnimSet()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionFacialAnimSet::Reset( void )
{
	m_Name.Clear();
	m_aFacialAnimClipIds.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionFacialAnimSet::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CActionFacialAnimSet: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CActionBranchSet
/////////////////////////////////////////////////////////////////////////////////
CActionBranchSet::CActionBranchSet()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionBranchSet::Reset( void )
{
	m_Name.Clear();

	int nActionBranchCount = m_aActionBranches.GetCount();
	for( int nActionBranchIdx = 0; nActionBranchIdx < nActionBranchCount; nActionBranchIdx++ )
		m_aActionBranches[ nActionBranchIdx ]->Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CActionBranchSet::Clear( void )
{
	int nActionBranchCount = m_aActionBranches.GetCount();
	for( int nActionBranchIdx = 0; nActionBranchIdx < nActionBranchCount; nActionBranchIdx++ )
	{
		m_aActionBranches[ nActionBranchIdx ]->Clear();
		delete m_aActionBranches[ nActionBranchIdx ];
	}
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionBranchSet::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	int nActionBranchCount = m_aActionBranches.GetCount();
	for( int nActionBranchIdx = 0; nActionBranchIdx < nActionBranchCount; nActionBranchIdx++ )
		m_aActionBranches[ nActionBranchIdx ]->PrintMemoryUsage( rTotalSize );
	Printf( "CActionBranchSet: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBranchActionName
// PURPOSE	:	Get the name of the action we can possibly branch to
// PARAMS	:	nBranchNum - Designated index you would like to query
// RETURNS	:	Name of the index designated action branch
/////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char* CActionBranchSet::GetBranchActionName( u32 nBranchNum ) const
{
	Assertf( nBranchNum < GetNumBranches(), "Branch number for GetBranchActionName is greater than allowed maximum." );
	return m_aActionBranches[ nBranchNum ]->GetName();
}
#endif	//	!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBranchActionID
// PURPOSE	:	Get the Id of the action we can possibly branch to
// PARAMS	:	nBranchNum - Designated index you would like to query
// RETURNS	:	Id of the queried branch
/////////////////////////////////////////////////////////////////////////////////
u32 CActionBranchSet::GetBranchActionID( const u32 nBranchNum ) const
{
	Assertf( nBranchNum < GetNumBranches(), "Branch number for GetBranchActionID is greater than allowed maximum." );
	return m_aActionBranches[ nBranchNum ]->GetID();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBranchFilterType
// PURPOSE	:	Test to see if it passes the branch type based on designated ped
// PARAMS	:	nBranchNum - Designated index you would like to query
// RETURNS	:	The filter type for the designated branch
/////////////////////////////////////////////////////////////////////////////////
CActionFlags::PlayerAiFilterTypeBitSetData CActionBranchSet::GetBranchFilterType( const u32 nBranchNum ) const
{
	Assertf( nBranchNum < GetNumBranches(), "Branch number for GetBranchActionID is greater than allowed maximum." );
	return m_aActionBranches[ nBranchNum ]->GetFilterType();
}

/////////////////////////////////////////////////////////////////////////////////
// CActionBranch
/////////////////////////////////////////////////////////////////////////////////
CActionBranch::CActionBranch()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionBranch::Reset( void )
{
	m_Name.Clear();
	m_Filter.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionBranch::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CActionBranch: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CActionResult
//
// This class is intended to encapsulate all of what occurs when an action is
// being done.  Many of its members simply define exactly what should be done.
/////////////////////////////////////////////////////////////////////////////////
CActionResult::CActionResult()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CActionResult::PostLoad( void )
{
	u32 nIndexFound = 0;

	if( GetHomingID() != 0 )
	{
		const CHoming* pHoming = ACTIONMGR.FindHoming( nIndexFound, GetHomingID() );
		SetHoming( const_cast<CHoming*>(pHoming) );
		Assertf( m_pHoming, "CActionManager::PostLoad - Homing (%s) was not found", GetHomingName() );
	}

	if( GetActionBranchSetID() != 0 )
	{
		const CActionBranchSet* pActionBranchSet = ACTIONMGR.FindActionBranchSet( nIndexFound, GetActionBranchSetID() );
		SetActionBranchSet( const_cast<CActionBranchSet*>(pActionBranchSet) );
		Assertf( pActionBranchSet, "CActionManager::PostLoad - Action Branch Set (%s) was not found", GetActionBranchSetName() );
	}

	if( GetStrikeBoneSetID() != 0 )
	{
		const CStrikeBoneSet* pStrikeBoneSet = ACTIONMGR.FindStrikeBoneSet( nIndexFound, GetStrikeBoneSetID() );
		SetStrikeBoneSet( const_cast<CStrikeBoneSet*>(pStrikeBoneSet) );
		Assertf( m_pStrikeBoneSet, "CActionManager::PostLoad - Strike Bone Set (%s) was not found", GetStrikeBoneSetName() );
	}

	if( GetDamageAndReactionID() != 0 )
	{
		const CDamageAndReaction* pDamageAndReaction = ACTIONMGR.FindDamageAndReaction( nIndexFound, GetDamageAndReactionID() );
		SetDamageAndReaction( const_cast<CDamageAndReaction*>(pDamageAndReaction) );
		Assertf( m_pDamageAndReaction, "CActionManager::PostLoad - Damage and Reaction (%s) was not found", GetDamageAndReactionName() );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionResult::Reset( void )
{
	m_pHoming = NULL;
	m_pActionBranchSet = NULL;
	m_pStrikeBoneSet = NULL;
	m_pDamageAndReaction = NULL;
	m_Name.Clear();
	m_ClipSet.Clear();
	m_FirstPersonClipSet.Clear();
	m_Anim.Clear();
	m_AnimBlendInRate = NORMAL_BLEND_DURATION;
	m_AnimBlendOutRate = SLOW_BLEND_DURATION;
	m_Homing.Clear();
	m_DamageAndReaction.Clear();
	m_StrikeBoneSet.Clear();
	m_Priority = 0;
	m_ResultAttrs.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionResult::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CActionResult: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CWeaponActionResult
/////////////////////////////////////////////////////////////////////////////////
CWeaponActionResult::CWeaponActionResult()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResult::PostLoad( void )
{
	u32 nIndexFound = 0;
	if( GetActionResultID() != 0 )
	{
		const CActionResult* pActionResult = ACTIONMGR.FindActionResult( nIndexFound, GetActionResultID() );
		Assertf( pActionResult, "CWeaponActionResult::PostLoad - ActionResult %s not found.\n", GetActionResultName() );
		SetActionResult( pActionResult );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResult::Reset( void )
{
	m_pActionResult = NULL;
	m_WeaponType.Reset();
	m_ActionResult.Clear();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResult::PrintMemoryUsage( size_t& rTotalSize )
{
	rTotalSize += sizeof( *this );
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CWeaponActionResultList
/////////////////////////////////////////////////////////////////////////////////
CWeaponActionResultList::CWeaponActionResultList()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResultList::PostLoad( void )
{
	u32 nIndexFound = 0;
	int nNumActionResults = m_aActionResults.GetCount();

	m_apActionResults.Resize( nNumActionResults );

	for ( int i = 0; i < nNumActionResults; i++ )
	{
		u32 nActionResultId = GetActionResultID( i );
		if( nActionResultId != 0 )
		{
			const CActionResult* pActionResult = ACTIONMGR.FindActionResult( nIndexFound, nActionResultId );
			Assertf( pActionResult, "CWeaponActionResult::PostLoad - ActionResult %s not found.\n", GetActionResultName( i ) );
			SetActionResult( i, pActionResult );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResultList::Reset( void )
{
	m_WeaponType.Reset();
	m_aActionResults.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResultList::PrintMemoryUsage( size_t& rTotalSize )
{
	rTotalSize += sizeof( *this );
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResultHashString
// PURPOSE	:	Get the hash string struct designated list action branch
// PARAMS	:	nActionResultIdx - Designated index you would like to query
// RETURNS	:	atHashString of the designated list index 
/////////////////////////////////////////////////////////////////////////////////
atHashString CWeaponActionResultList::GetActionResultHashString( int nActionResultIdx ) const
{
	Assertf( nActionResultIdx < m_aActionResults.GetCount(), "Invalid index %d with count of %d", nActionResultIdx, m_aActionResults.GetCount() );
	return m_aActionResults[ nActionResultIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResultID
// PURPOSE	:	Get the id(hash value) string designated list action branch
// PARAMS	:	nActionResultIdx - Designated index you would like to query
// RETURNS	:	Hash value(u32) of the designated list index 
/////////////////////////////////////////////////////////////////////////////////
u32 CWeaponActionResultList::GetActionResultID( int nActionResultIdx ) const
{
	Assertf( nActionResultIdx < m_aActionResults.GetCount(), "Invalid index %d with count of %d", nActionResultIdx, m_aActionResults.GetCount() );
	return m_aActionResults[ nActionResultIdx ].GetHash();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResult
// PURPOSE	:	Get ptr to the actual CActionResult which is stored in PostLoad
// PARAMS	:	nActionResultIdx - Designated index you would like to query
// RETURNS	:	Ptr to the CActionResult which is defined in action_table.meta
/////////////////////////////////////////////////////////////////////////////////
const CActionResult* CWeaponActionResultList::GetActionResult( int nActionResultIdx ) const
{
	Assertf( nActionResultIdx < m_apActionResults.GetCount(), "Invalid index %d with count of %d", nActionResultIdx, m_apActionResults.GetCount() );
	return m_apActionResults[ nActionResultIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetActionResult
// PURPOSE	:	Assigns the CActionResult given the list index
// PARAMS	:	nActionResultIdx - Designated index you would to store
//				pActionResult - Pointer to the action result we would like to store
/////////////////////////////////////////////////////////////////////////////////
void CWeaponActionResultList::SetActionResult( int nActionResultIdx, const CActionResult* pActionResult )
{
	Assertf( nActionResultIdx < m_apActionResults.GetCount(), "Invalid index %d with count of %d", nActionResultIdx, m_apActionResults.GetCount() );
	m_apActionResults[ nActionResultIdx ] = pActionResult;
}

#if !__FINAL
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResultName
// PURPOSE	:	Get the string name (only final) for the designated list action branch
// PARAMS	:	nActionResultIdx - Designated index you would like to query
// RETURNS	:	Actual string of the designated list index 
/////////////////////////////////////////////////////////////////////////////////
const char* CWeaponActionResultList::GetActionResultName( int nActionResultIdx ) const
{
	Assertf( nActionResultIdx < m_aActionResults.GetCount(), "Invalid index %d with count of %d", nActionResultIdx, m_aActionResults.GetCount() );
	return m_aActionResults[ nActionResultIdx ].GetCStr();

}
#endif	//	!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// CActionCondPedOther
//
// This handles the action suitability test on the state of the other
// ped (inspection).
/////////////////////////////////////////////////////////////////////////////////
CActionCondPedOther::CActionCondPedOther()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CActionCondPedOther::PostLoad( void )
{
	u32 nIndexFound = 0;
	if( GetInterrelationTestID() != 0 )
	{
		const CInterrelationTest* pInterrelationTest = ACTIONMGR.FindInterrelationTest( nIndexFound, GetInterrelationTestID() );
		SetInterrelationTest( const_cast<CInterrelationTest*>(pInterrelationTest) );
		Assertf( m_pInterrelationTest, "CActionDefinition::PostLoad - InterrelationTest (%s) was not found", GetInterrelationTestName() );
	}

	int nNumSpecialConditions = m_aCondSpecials.GetCount();
	for( int i = 0; i < nNumSpecialConditions; i++ )
		m_aCondSpecials[i]->PostLoad();

	int nNumActionResultLists = m_aWeaponActionResultList.GetCount();
	for( int i = 0; i < nNumActionResultLists; i++ )
		m_aWeaponActionResultList[i]->PostLoad();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionCondPedOther::Reset( void )
{
	int nNumSpecialConditions = m_aCondSpecials.GetCount();
	for( int i = 0; i < nNumSpecialConditions; i++ )
		m_aCondSpecials[i]->Reset();

	int nNumActionResultLists = m_aWeaponActionResultList.GetCount();
	for( int i = 0; i < nNumActionResultLists; i++ )
		m_aWeaponActionResultList[i]->Reset();

	m_aBrawlingStyles.Reset();
	m_pInterrelationTest = NULL;
	m_InterrelationTest.Clear();
	m_MovementSpeed.Reset();
	m_SelfFilter.Reset();
	m_CheckAnimTime = false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CActionCondPedOther::Clear( void )
{
	int nNumSpecialConditions = m_aCondSpecials.GetCount();
	for( int i = 0; i < nNumSpecialConditions; i++ )
	{
		m_aCondSpecials[i]->Clear();
		delete m_aCondSpecials[i];
	}

	int nNumActionResultLists = m_aWeaponActionResultList.GetCount();
	for( int i = 0; i < nNumActionResultLists; i++ )
	{
		m_aWeaponActionResultList[i]->Clear();
		delete m_aWeaponActionResultList[i];
	}
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionCondPedOther::PrintMemoryUsage( size_t& rTotalSize )
{
	rTotalSize += sizeof( *this );
	int nNumSpecialConditions = m_aCondSpecials.GetCount();
	for( int i = 0; i < nNumSpecialConditions; i++ )
		m_aCondSpecials[i]->PrintMemoryUsage( rTotalSize );
		
	int nNumActionResultLists = m_aWeaponActionResultList.GetCount();
	for( int i = 0; i < nNumActionResultLists; i++ )
		m_aWeaponActionResultList[i]->PrintMemoryUsage( rTotalSize );
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetWeaponActionResultList
// PURPOSE	:	Returns the CWeaponActionResultList given an array index
// PARAMS	:	nIdx - Direct index we will dereference
// RETURNS	:	The index associated CWeaponActionResult
/////////////////////////////////////////////////////////////////////////////////
const CWeaponActionResultList* CActionCondPedOther::GetWeaponActionResultList( const CPed* pViewedPed ) const
{
	int nNumActionResultLists = m_aWeaponActionResultList.GetCount();
	for( int i = 0; i < nNumActionResultLists; i++ )
	{
		if( CActionManager::ShouldAllowWeaponType( pViewedPed, m_aWeaponActionResultList[i]->GetWeaponType() ) )
			return m_aWeaponActionResultList[i];
	}

	return NULL;
}

const CWeaponActionResultList* CActionCondPedOther::GetWeaponActionResultList( const int nIdx ) const
{
	Assertf( nIdx < m_aWeaponActionResultList.GetCount(), "CActionCondPedOther::GetWeaponActionResultList - nIdx (%d) is out of range (%d).", nIdx, m_aWeaponActionResultList.GetCount() );
	return m_aWeaponActionResultList[nIdx];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBrawlingStyleName
// PURPOSE	:	Returns the brawling style cstr given an array index
// PARAMS	:	nIdx - Direct index we will dereference
// RETURNS	:	The index associated atStringHash
/////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char*	CActionCondPedOther::GetBrawlingStyleName( const u32 nIdx ) const
{
	Assertf( nIdx < m_aBrawlingStyles.GetCount(), "CActionCondPedOther::GetBrawlingStyleName - nIdx (%d) is out of range (%d).", nIdx, m_aBrawlingStyles.GetCount() );
	return m_aBrawlingStyles[nIdx].GetCStr();
}
#endif	//	!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBrawlingStyleID
// PURPOSE	:	Returns the brawling style hash id given an array index
// PARAMS	:	nIdx - Direct index we will dereference
// RETURNS	:	The index associated atStringHash
/////////////////////////////////////////////////////////////////////////////////
u32 CActionCondPedOther::GetBrawlingStyleID( const u32 nIdx ) const
{
	Assertf( nIdx < m_aBrawlingStyles.GetCount(), "CActionCondPedOther::GetBrawlingStyleID - nIdx (%d) is out of range (%d).", nIdx, m_aBrawlingStyles.GetCount() );
	return m_aBrawlingStyles[nIdx].GetHash();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetSpecialCondition
// PURPOSE	:	returns a CActionCondSpecial given an index
// PARAMS	:	nIdx - Index we would like to reference
// RETURNS	:	CActionCondSpecial associated with the designated index
/////////////////////////////////////////////////////////////////////////////////
const CActionCondSpecial* CActionCondPedOther::GetSpecialCondition( const u32 nIdx ) const
{
	Assertf( nIdx < GetNumSpecialConditions(), "CActionCondPedOther::GetSpecialCondition - nIdx (%d) is out of range (%d).", nIdx, GetNumSpecialConditions() );
	return m_aCondSpecials[ nIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DoTest
// PURPOSE	:	Test to determine if this object is suitable given the parameters
// PARAMS	:	pViewerPed - Ped doing the test
//				pViewedPed - Ped we are looking at
//				pViewerResult - Action result associated with the viewer ped
//				nForcingResultId - A result id that may have been previously stored for use
//				pReusableResults - Cached results that will be reused
//				bDisplayDebug - Used to determine if this test is being debugged
// RETURNS	:	Whether or not the test has passed
/////////////////////////////////////////////////////////////////////////////////
bool CActionCondPedOther::DoTest( const CPed* pViewerPed, const CPed* pViewedPed, const CActionResult* pViewerResult, const u32 nForcingResultId, CActionTestCache* pReusableResults, const bool bDisplayDebug ) const
{
	Assert( pViewerPed );
	Assert( pViewedPed );

	// Check the ped self test (introspection).
	const bool bPedSelfTestPassed = CActionManager::PlayerAiFilterTypeDoesCatchPed( GetSelfFilter(), pViewedPed );
	if( !bPedSelfTestPassed )
	{
		actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - !bPedSelfTestPassed\n" );
		return false;
	}

	// Check the brawling style
	u32 nPedBrawlingStyleId = pViewedPed->GetBrawlingStyle().GetHash();
	if( !ShouldAllowBrawlingStyle( nPedBrawlingStyleId ) )
	{
		actionPrintf( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - nBrawlingStyleId(%d) not allowed\n", nPedBrawlingStyleId );
		return false;
	}

	// Check against movement speed
	if( !CActionManager::WithinMovementSpeed( pViewedPed, m_MovementSpeed ) )
	{
		actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - !CActionManager::WithinMovementSpeed()\n" );
		return false;
	}

	// First determine if the viewer ped action matches any of the desired actions
	const CWeaponActionResultList* pActionResultList = GetWeaponActionResultList( pViewedPed );
	if( pActionResultList )
	{
		int nActionResultIndex = -1;
		int nNumActionResults = pActionResultList->GetNumActionResults();
		for ( int i = 0; i < nNumActionResults; i++ )
		{
			u32 nResultId = pActionResultList->GetActionResultID( i );
			if( nResultId != 0 )
			{
				// Check if there is stored result Id for us to use.
				if( nForcingResultId != 0 )
				{
					if( nForcingResultId == nResultId )
					{
						nActionResultIndex = i;
						break;
					}
				}
				// Try to get the value from what the pViewedPed is currently doing.
				else
				{
					// Make sure a simple action task is playing.
					Assertf( pViewedPed->GetPedIntelligence(), "Viewed ped GetPedIntelligence() failed." );
					CQueriableInterface* pQueriableInterface = pViewedPed->GetPedIntelligence()->GetQueriableInterface();
					Assertf( pQueriableInterface, "Viewed ped QueriableInterface is null." );
					const bool bIsInMeleeCombat = pQueriableInterface->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE_ACTION_RESULT );
					if( !bIsInMeleeCombat )
						continue;
					
					// Get the current res (if there is one).
					// DMKH. For cloned peds, it's possible to run without a CTaskMelee, so go to CTaskMeleeActionResult
					// in that case.
					CTaskMelee* pTaskMelee = pViewedPed->GetPedIntelligence()->GetTaskMelee();
					u32 nActingPedResultId = 0;
					if( pTaskMelee )
						nActingPedResultId = pTaskMelee->GetActionResultId();
					else
					{
						CTaskMeleeActionResult *pMeleeActionResult = pViewedPed->GetPedIntelligence()->GetTaskMeleeActionResult();
						if( pMeleeActionResult )
							nActingPedResultId = pMeleeActionResult->GetResultId();
						else
							continue;
					}

					// Determine if they are the same res.
					if( nActingPedResultId == nResultId )
					{
						nActionResultIndex = i;
						break;
					}
				}
			}
		}

		// Determine if we were able to locate the associated action result index
		if( nActionResultIndex == -1 )
		{
			actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - Unable to find a matching weapon action result\n"  );
			return false;
		}
	}
	else if( GetNumWeaponActionResultLists() > 0 )
	{
		actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - There are weapon action results but none match the viewed ped weapon\n" );
		return false;
	}

	// Only check the opponent anim time if we are supposed to.
	if( GetCheckAnimTime() )
	{
		// Make sure a simple action task is playing.
		Assertf( pViewedPed->GetPedIntelligence(), "Viewed ped GetPedIntelligence() failed in CActionCondPedOther::DoTest." );
		CTaskMeleeActionResult* pMeleeActionResult = pViewedPed->GetPedIntelligence()->GetTaskMeleeActionResult();
		if( !pMeleeActionResult )
		{
			actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - Invalid CTaskMeleeActionResult\n" );
			return false;
		}

		if( !pMeleeActionResult->ShouldAllowDodge() )
		{
			actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (FAILED) - CTaskMeleeActionResult doesn't allow dodges\n" );
			return false;
		}
	}

	// Determine if it passes the interrelation test (based on the target peds perspective)
	const bool bInterrelationTestPassed = !m_pInterrelationTest || m_pInterrelationTest->DoTest( pViewedPed, pViewerPed, NULL, bDisplayDebug );
	if( !bInterrelationTestPassed )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - Interrelation test was rejected\n", m_pInterrelationTest->GetName() );
		return false;
	}

	// Iterate through special tests 
	for( int i = 0; i < m_aCondSpecials.GetCount(); i++ )
	{
		if( !CActionManager::PlayerAiFilterTypeDoesCatchPed( m_aCondSpecials[i]->GetFilter(), pViewedPed ) )
			continue;

		// Set these up so it is tested from the target peds perspective
		if( !m_aCondSpecials[i]->DoTest( pViewedPed, pViewerPed, pViewerResult, pReusableResults, bDisplayDebug ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Special Condition [%d] rejected\n", m_aCondSpecials[i]->GetSpecialTestName(), i );
			return false;
		}	
	}

	actionPrint( bDisplayDebug, "CActionCondPedOther::DoTest (SUCCESS)\n" );

	// Let the caller know it passed all the tests.
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAllowBrawlingStyle
// PURPOSE	:	Test to determine if the designated brawling style hash matches any of the allowed cases
// PARAMS	:	nBrawlingStyleHash - atStringHash of the brawling style we are asking about
// RETURNS	:	Whether or not this CActionDefinition is suitable given the brawling style hash
/////////////////////////////////////////////////////////////////////////////////
bool CActionCondPedOther::ShouldAllowBrawlingStyle(u32 nBrawlingStyleHash) const
{
	u32 nNumBrawlingStyles = m_aBrawlingStyles.GetCount();
	if( nNumBrawlingStyles == 0 )
		return true;

	for( u32 i = 0; i < nNumBrawlingStyles; i++ )
	{
		if( nBrawlingStyleHash == m_aBrawlingStyles[i].GetHash() )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// CActionRumble
//
// This handles the action rumble
/////////////////////////////////////////////////////////////////////////////////
CActionRumble::CActionRumble()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionRumble::Reset( void )
{
	m_Duration = 0;
	m_Intensity = 1.0f;
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionRumble::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CActionRumble: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif


/////////////////////////////////////////////////////////////////////////////////
// CInterrelationTest
//
// This handles the action suitability test on the relative
// attributes of the two entities (interrelation).
/////////////////////////////////////////////////////////////////////////////////
CInterrelationTest::CInterrelationTest()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DoTest
// PURPOSE	:	Test to determine if this object is suitable given the parameters
// PARAMS	:	pViewerPed - the entity doing the test.
//				pViewedEntity - Entity we are looking at
//				pReusableResults - Cached results that will be reused
//				bDisplayDebug - Used to determine if this test is being debugged
// RETURNS :	Whether or not the test has passed
/////////////////////////////////////////////////////////////////////////////////
bool CInterrelationTest::DoTest( const CPed* pViewerPed, const CEntity* pViewedEntity, CActionTestCache* pReusableResults, const bool bDisplayDebug ) const
{
	if( !pViewedEntity )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - Invalid viewed entity\n", GetName() );
		return false;
	}

	// Check if it it at the correct position?
	if( !m_RelativeRange.IsViewerLocalPositionInRange( pViewerPed, GetRootOffsetZ(), pViewedEntity, pReusableResults, GetPredictedTargetTime(), bDisplayDebug ) )
		return false;
	
	actionPrintf( bDisplayDebug, "SUCCESS (%s)\n", GetName() );

	// Let he caller know it passed all the tests.
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CInterrelationTest::PostLoad(void)
{
	m_RelativeRange.PostLoad();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CInterrelationTest::Reset( void )
{
	m_RootOffsetZ = 0.0f;
	m_PredictedTargetTime = 0.0f;
	m_RelativeRange.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CInterrelationTest::Clear(void)
{
	m_RelativeRange.Clear();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CInterrelationTest::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CInterrelationTest: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CImpulse
//
// Storage class for the actual impulse (button press)
/////////////////////////////////////////////////////////////////////////////////
CImpulse::CImpulse( CSimpleImpulseTest::Impulse impulse, bool bExecuted )
: m_Impulse( impulse )
, m_RecordedTime( 0 )
, m_bExecuted( bExecuted )
{
}

CImpulse::CImpulse()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CImpulse::Reset()
{
	m_Impulse = CSimpleImpulseTest::ImpulseNone;
	m_RecordedTime = 0;
	m_bExecuted = false;
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetImpulseName
// PURPOSE	:	Helper function that will return string given an impulse type
/////////////////////////////////////////////////////////////////////////////////
const char*  CImpulse::GetImpulseName(s32 impulse)
{
	switch (impulse)
	{
		case CSimpleImpulseTest::ImpulseNone : return "None";
		case CSimpleImpulseTest::ImpulseAttackLight : return "Light";
		case CSimpleImpulseTest::ImpulseAttackHeavy : return "Heavy";
		case CSimpleImpulseTest::ImpulseAttackAlternate : return "Alternate";
		case CSimpleImpulseTest::ImpulseBlock : return "Block";
		case CSimpleImpulseTest::ImpulseFire : return "Fire";
		case CSimpleImpulseTest::ImpulseSpecialAbility : return "SpecialAbility";
		case CSimpleImpulseTest::ImpulseSpecialAbilitySecondary : return "SpecialAbilitySecondary";
		case CSimpleImpulseTest::ImpulseAnalogLeftRight : return "AnalogLeftRight";
		case CSimpleImpulseTest::ImpulseAnalogUpDown : return "AnalogUpDown";
		case CSimpleImpulseTest::ImpulseAnalogFromCenter : return "AnalogFromCenter";
		default : return "Invalid";
	}
}
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// CImpulseCombo
//
// This stores the current combo input state, e.g. which impulses have been 
// generated and in which order.
/////////////////////////////////////////////////////////////////////////////////
CImpulseCombo::CImpulseCombo()
{
	Reset();
}

CImpulseCombo::CImpulseCombo(CSimpleImpulseTest::Impulse initialImpulse)
{	
	if (initialImpulse != CSimpleImpulseTest::ImpulseNone)
		RecordFirstImpulse(initialImpulse);
}

bool CImpulseCombo::operator==( const CImpulseCombo& rhs ) const
{
	if( m_aImpulses.GetCount() == rhs.GetNumImpulses() )
	{
		for(s32 i = 0; i < m_aImpulses.GetCount(); i++)
		{
			if( m_aImpulses[i].GetImpulse() != rhs.GetImpulseAtIndex(i) )
				return false;
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CImpulseCombo::PostLoad( void )
{
	for(s32 i = 0; i < m_aImpulses.GetCount(); i++)
		m_aImpulses[ i ].PostLoad();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CImpulseCombo::Reset( void )
{
	for(s32 i = 0; i < m_aImpulses.GetCount(); i++)
		m_aImpulses[ i ].Reset();

	m_aImpulses.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CImpulseCombo::Clear( void )
{
	for(s32 i = 0; i < m_aImpulses.GetCount(); i++)
		m_aImpulses[ i ].Clear();

	m_aImpulses.Reset();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CImpulseCombo::PrintMemoryUsage( size_t& rTotalSize )
{
	rTotalSize += sizeof( *this );
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RecordFirstImpulse
// PURPOSE	:	Reset the current combo and log the first impulse
// PARAMS	:	impulse - Button impulse to log for the new combo
/////////////////////////////////////////////////////////////////////////////////
void CImpulseCombo::RecordFirstImpulse( CSimpleImpulseTest::Impulse impulse )
{
	Reset();
	RecordImpulse( impulse, true );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ScanForImpulses
// PURPOSE	:	Check the given CControl for any button impulses and log them
// PARAMS	:	pController - Pointer to the physical controller 
// RETURNS	:	Whether or not we recorded an impulse
/////////////////////////////////////////////////////////////////////////////////
bool CImpulseCombo::ScanForImpulses( const CControl* pController )
{
	if( pController )
	{
		for( s32 iImpulse = CSimpleImpulseTest::ImpulseAttackLight; iImpulse <= CSimpleImpulseTest::ImpulseFire; iImpulse++)
		{
			ioValue const * ioVal0 = NULL;
			ioValue const * ioVal1 = NULL;
			if( CSimpleImpulseTest::GetIOValuesForImpulse(pController, (CSimpleImpulseTest::Impulse)iImpulse, ioVal0, ioVal1) )
			{
				aiFatalAssertf(ioVal0, "ioVal0 expected!");
				if( ioVal0->IsPressed() )
				{
					RecordImpulse((CSimpleImpulseTest::Impulse)iImpulse);
					return true;
				}
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HasPendingImpulse
// PURPOSE	:	Check if any of the impulses have not yet been executed
// RETURNS	:	Whether or not there is a pending impulse 
/////////////////////////////////////////////////////////////////////////////////
bool CImpulseCombo::HasPendingImpulse( void ) const
{
	const int nNumImpulses = GetNumImpulses();
	for( s32 i = 0; i < nNumImpulses; i++ )
	{
		if( !m_aImpulses[ i ].HasExecuted() && m_aImpulses[ i ].GetImpulse() != CSimpleImpulseTest::ImpulseNone )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetLatestRecordedImpulseTime
// PURPOSE	:	Find the most recent recorded impulse
// RETURNS	:	Frame of the last known recorded impulse
/////////////////////////////////////////////////////////////////////////////////
u32 CImpulseCombo::GetLatestRecordedImpulseTime( void ) const
{
	const int nNumImpulses = GetNumImpulses();
	for( s32 i = nNumImpulses - 1; i >= 0; i-- )
	{
		if( m_aImpulses[ i ].GetImpulse() != CSimpleImpulseTest::ImpulseNone )
			return m_aImpulses[ i ].GetRecordedTime();
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetImpulseAtIndex
// PURPOSE	:	Return the recorded impulse at a given index
// PARAMS	:	iComboIndex - Index we would like to reference
// RETURNS	:	The impulse that has been recorded at the given index
/////////////////////////////////////////////////////////////////////////////////
CSimpleImpulseTest::Impulse CImpulseCombo::GetImpulseAtIndex( s32 iComboIndex ) const
{
	if( iComboIndex >= GetNumImpulses() )
		return CSimpleImpulseTest::ImpulseNone;

	return m_aImpulses[ iComboIndex ].GetImpulse();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetImpulsePtrAtIndex
// PURPOSE	:	Returns a pointer to the recorded button impulse
// PARAMS	:	iComboIndex - Index we would like to reference
// RETURNS	:	The impulse pointer that has been recorded at the given index
/////////////////////////////////////////////////////////////////////////////////
CImpulse* CImpulseCombo::GetImpulsePtrAtIndex( s32 iComboIndex )
{
	if( iComboIndex >= GetNumImpulses() )
		return NULL;

	return &m_aImpulses[ iComboIndex ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetImpulsePtrAtIndex
// PURPOSE	:	Const version of the above function
// PARAMS	:	iComboIndex - Index we would like to reference
// RETURNS	:	The impulse pointer that has been recorded at the given index
/////////////////////////////////////////////////////////////////////////////////
const CImpulse*	CImpulseCombo::GetImpulsePtrAtIndex(s32 iComboIndex) const
{
	if( iComboIndex >= GetNumImpulses() )
		return NULL;

	return &m_aImpulses[ iComboIndex ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RecordImpulse
// PURPOSE	:	Logs a button impulse into the combo array
// PARAMS	:	impulse - Index we would like to reference
//				bMarkExecuted - whether or not we want to mark the impulse as executed
// RETURNS	:	Whether or not we successfully recorded a new impulse
/////////////////////////////////////////////////////////////////////////////////
bool CImpulseCombo::RecordImpulse( CSimpleImpulseTest::Impulse impulse, bool bMarkExecuted )
{
	if( !m_aImpulses.IsFull() )
	{
		CImpulse &rImpulse = m_aImpulses.Append();
		rImpulse.SetImpulse( impulse );
		rImpulse.SetRecordedTime( fwTimer::GetTimeInMilliseconds() );
		rImpulse.SetHasExecuted( bMarkExecuted );
	}
	// Otherwise lets see if we should interrupt
	else
	{
		// Walk through each impulse and determine if we should interrupt previously 
		// stored impulses
		const int nNumImpulses = GetNumImpulses();
		s32 iReplacementIdx = -1;
		for( s32 i = 0; i < nNumImpulses; i++ )
		{
			if( !m_aImpulses[ i ].HasExecuted() )
			{
				if( m_aImpulses[ i ].GetImpulse() != impulse )
				{
					// replace the existing impulse and remove the rest
					m_aImpulses[ i ].Reset();
					m_aImpulses[ i ].SetRecordedTime( fwTimer::GetTimeInMilliseconds() );
					m_aImpulses[ i ].SetImpulse( impulse );
					iReplacementIdx = i;
					break;
				}
				// Just update last button press time
				else
					m_aImpulses[ i ].SetRecordedTime( fwTimer::GetTimeInMilliseconds() );
			}
		}

		if( iReplacementIdx != -1 )
		{
			// Walk backwards and release the impulses that have not been executed
			for(s32 i = nNumImpulses - 1; i > iReplacementIdx; i--)
			{
				m_aImpulses.DeleteFast( i );
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// CStealthKillTest
/////////////////////////////////////////////////////////////////////////////////
CStealthKillTest::CStealthKillTest()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CStealthKillTest::Reset( void )
{
	m_Name.Clear();
	m_ActionType.Reset();
	m_StealthKillAction.Clear();
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CStealthKillTest::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	Printf( "CStealthKillTest: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// CSimpleImpulseTest
//
// This handles the action suitability data about how to test the pController.
// i.e. did they press X and then hold it down within the last 10 frames?
/////////////////////////////////////////////////////////////////////////////////
CSimpleImpulseTest::CSimpleImpulseTest()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CSimpleImpulseTest::Reset( void )
{
	m_Impulse = ImpulseNone;
	m_State = ImpulseStateNone;
	m_AnalogRange.Zero();
	m_Duration = 0.0f;
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CSimpleImpulseTest::PrintMemoryUsage( size_t& rTotalSize )
{
	rTotalSize += sizeof( *this );
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DoTest
// PURPOSE	:	Test to determine if this object is suitable given the parameters
// PARAMS	:	pController - Pointer to the physical controller 
//				pImpulseCombo - Cached version of the current button combo
//				pReusableResults - Cache store to save off logic for future use
//				bDisplayDebug - Used to determine if this test is being debugged
// RETURNS	:	Whether or not the test has passed
/////////////////////////////////////////////////////////////////////////////////
bool CSimpleImpulseTest::DoTest( const CControl* pController, const CImpulseCombo* pImpulseCombo, CActionTestCache*	UNUSED_PARAM(pReusableResults), const bool BANK_ONLY( bDisplayDebug ) ) const
{
	Assertf( pController, "CSimpleImpulseTest::DoTest - Invalide CControl." );

	// Combo states are checked differently
	ImpulseState eImpulseState = GetImpulseState();
	if( eImpulseState >= ImpulseStateCombo1 && eImpulseState <= ImpulseStateComboAny )
	{
		// If a combo set has been passed through, check to see if the combo requested is set
		if( pImpulseCombo )
		{
			// Should we allow this impulse on any part of the combo?
			if( eImpulseState == ImpulseStateComboAny )
			{
				// Find the first non-executed impulses to see if we have a match
				for ( s32 i = 0; i< pImpulseCombo->GetNumImpulses(); i++ )
				{
					const CImpulse* pImpulse = pImpulseCombo->GetImpulsePtrAtIndex(i);
					if( pImpulse && !pImpulse->HasExecuted() )
					{
						if( pImpulse->GetImpulse() == GetImpulse() )
							return true;

						// Otherwise break out and fail test
						break;
					}
				}
			}
			// Otherwise specifically test the impulse state index
			else
			{
				s32 iIndex = eImpulseState - ImpulseStateCombo1;

				// If a valid impulse has been recorded at this point, check its the same as the one requested
				actionPrintf( bDisplayDebug, "%s (%s) - eImpulseState == ImpulseStateComboAny\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ), pImpulseCombo->GetImpulseAtIndex(iIndex) == GetImpulse() ? "SUCCESS" : "FAILED" );
				return pImpulseCombo->GetImpulseAtIndex(iIndex) == GetImpulse();
			}
		}

		actionPrintf( bDisplayDebug, "%s (FAILED) - Invalid impulse combo\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ) );
		return false;
	}

	// Get the pController element(s) to test.
	ioValue const * ioVal0 = NULL;
	ioValue const * ioVal1 = NULL;
	Impulse impulse = GetImpulse();
	if( !GetIOValuesForImpulse(pController, impulse, ioVal0, ioVal1) )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - !GetIOValuesForImpulse(pController, impulse, ioVal0, ioVal1)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ) );
		return false;
	}

	Assert( ioVal0 );

	// Convert the duration into milliseconds.
	const u32 durationMS = static_cast<u32>(GetDuration() * 1000.0f);

	// Set the range of ioValues defined as "down".
	float downRangeMin = ioValue::BUTTON_DOWN_THRESHOLD;
	float downRangeMax = 1.0f;
	if(	(impulse == ImpulseAnalogLeftRight) ||
		(impulse == ImpulseAnalogUpDown))
	{
		// The analog range values are from -1.0f to 1.0f.
		downRangeMin = GetAnalogRange().x;
		downRangeMax = GetAnalogRange().y;
	}
	else if(impulse == ImpulseAnalogFromCenter)
	{
		// The analog range values for the from center test are from 0.0f to 1.0f.
		downRangeMin = rage::Abs(GetAnalogRange().x);
		downRangeMax = rage::Abs(GetAnalogRange().y);
	}

	// Do the test (ImpulseAnalogFromCenter is handled differently since it
	// relies on two ioValues instead of just one).
	if(impulse != ImpulseAnalogFromCenter)
	{
		switch( GetImpulseState() )
		{
			case ImpulseStateNone:
			{
				actionPrint( bDisplayDebug, "CSimpleImpulseTest::DoTest - Impulse State = ImpulseStateNone\n" );
				return true;
			}
			case ImpulseStateDown:
			{	
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateDown) - ioVal0->HistoryHeldDown\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ), ioVal0->IsDown(downRangeMin) ? "SUCCESS" : "FAILED" );
				return ioVal0->IsDown(downRangeMin);
			}
			case ImpulseStateHeldDown:
			{	
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateHeldDown) - ioVal0->HistoryHeldDown\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ), ioVal0->HistoryHeldDown(durationMS, downRangeMin, downRangeMax) ? "SUCCESS" : "FAILED" );
				return ioVal0->HistoryHeldDown(durationMS, downRangeMin, downRangeMax);
			}
			case ImpulseStateHeldUp:
			{
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateHeldUp) - ioVal0->HistoryHeldUp\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),ioVal0->HistoryHeldUp(durationMS, downRangeMin, downRangeMax) ? "SUCCESS" : "FAILED" );
				return ioVal0->HistoryHeldUp(durationMS, downRangeMin, downRangeMax);
			}	
			case ImpulseStatePressedAndHeld:
			{
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStatePressedAndHeld) - ioVal0->HistoryPressedAndHeld\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),ioVal0->HistoryPressedAndHeld(durationMS, NULL, downRangeMin, downRangeMax) ? "SUCCESS" : "FAILED" );
				return ioVal0->HistoryPressedAndHeld(durationMS, NULL, downRangeMin, downRangeMax);
			}	
			case ImpulseStateReleasedAndHeld:
			{
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateReleasedAndHeld) - ioVal0->HistoryReleasedAndHeld\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),ioVal0->HistoryReleasedAndHeld(durationMS, NULL, downRangeMin, downRangeMax) ? "SUCCESS" : "FAILED" );
				return ioVal0->HistoryReleasedAndHeld(durationMS, NULL, downRangeMin, downRangeMax);
			}	
			case ImpulseStatePressed:
			{
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStatePressed) - ioVal0->HistoryPressed\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),ioVal0->HistoryPressed(durationMS, NULL, downRangeMin, downRangeMax) ? "SUCCESS" : "FAILED" );
				return ioVal0->HistoryPressed(durationMS, NULL, downRangeMin, downRangeMax);
			}	
			case ImpulseStatePressedNoHistory:
			{
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStatePressedNoHistory) - ioVal0->IsDown\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ), ioVal0->IsDown(downRangeMin) ? "SUCCESS" : "FAILED" );
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStatePressedNoHistory) - !ioVal0->WasDown\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ), !ioVal0->WasDown(downRangeMin) ? "SUCCESS" : "FAILED" );
				return ioVal0->IsDown(downRangeMin) && !ioVal0->WasDown(downRangeMin);
			}
			case ImpulseStateReleased:
			{
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateReleased) - ioVal0->HistoryReleased\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),ioVal0->HistoryReleased(durationMS, NULL, downRangeMin, downRangeMax) ? "SUCCESS" : "FAILED" );
				return ioVal0->HistoryReleased(durationMS, NULL, downRangeMin, downRangeMax);
			}	
			default:
			{
				actionPrintf( bDisplayDebug, "%s (FAILED) - Impulse State (default)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ) );
				return false;
			}
		}
	}
	else
	{
		Assert(ioVal1);
		switch(GetImpulseState())
		{
			case ImpulseStateNone:
			{
				actionPrintf( bDisplayDebug, "%s (SUCCESS) - Impulse State (ImpulseStateNone)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ) );
				return true;
			}
			case ImpulseStateHeldDown:
			{
				const bool io0Side0Pass = ioVal0->HistoryHeldDown(durationMS, downRangeMin, downRangeMax);
				const bool io0Side1Pass = ioVal0->HistoryHeldDown(durationMS, -downRangeMax, -downRangeMin);
				const bool io1Side0Pass = ioVal1->HistoryHeldDown(durationMS, downRangeMin, downRangeMax);
				const bool io1Side1Pass = ioVal1->HistoryHeldDown(durationMS, -downRangeMax, -downRangeMin);
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateHeldDown) - (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),(io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass) ? "SUCCESS" : "FAILED" );
				return (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass);
			}
			case ImpulseStateHeldUp:
			{
				const bool io0Side0Pass = ioVal0->HistoryHeldUp(durationMS, downRangeMin, downRangeMax);
				const bool io0Side1Pass = ioVal0->HistoryHeldUp(durationMS, -downRangeMax, -downRangeMin);
				const bool io1Side0Pass = ioVal1->HistoryHeldUp(durationMS, downRangeMin, downRangeMax);
				const bool io1Side1Pass = ioVal1->HistoryHeldUp(durationMS, -downRangeMax, -downRangeMin);
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateHeldUp) - (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),(io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass) ? "SUCCESS" : "FAILED" );
				return (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass);
			}
			case ImpulseStatePressedAndHeld:
			{
				const bool io0Side0Pass = ioVal0->HistoryPressedAndHeld(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io0Side1Pass = ioVal0->HistoryPressedAndHeld(durationMS, NULL, -downRangeMax, -downRangeMin);
				const bool io1Side0Pass = ioVal1->HistoryPressedAndHeld(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io1Side1Pass = ioVal1->HistoryPressedAndHeld(durationMS, NULL, -downRangeMax, -downRangeMin);
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStatePressedAndHeld) - (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),(io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass) ? "SUCCESS" : "FAILED" );
				return (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass);
			}
			case ImpulseStateReleasedAndHeld:
			{
				const bool io0Side0Pass = ioVal0->HistoryReleasedAndHeld(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io0Side1Pass = ioVal0->HistoryReleasedAndHeld(durationMS, NULL, -downRangeMax, -downRangeMin);
				const bool io1Side0Pass = ioVal1->HistoryReleasedAndHeld(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io1Side1Pass = ioVal1->HistoryReleasedAndHeld(durationMS, NULL, -downRangeMax, -downRangeMin);
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateReleasedAndHeld) - (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),(io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass) ? "SUCCESS" : "FAILED" );
				return (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass);
			}
			case ImpulseStatePressed:
			{
				const bool io0Side0Pass = ioVal0->HistoryPressed(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io0Side1Pass = ioVal0->HistoryPressed(durationMS, NULL, -downRangeMax, -downRangeMin);
				const bool io1Side0Pass = ioVal1->HistoryPressed(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io1Side1Pass = ioVal1->HistoryPressed(durationMS, NULL, -downRangeMax, -downRangeMin);
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStatePressed) - (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),(io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass) ? "SUCCESS" : "FAILED" );
				return (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass);
			}
			case ImpulseStateReleased:
			{
				const bool io0Side0Pass = ioVal0->HistoryReleased(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io0Side1Pass = ioVal0->HistoryReleased(durationMS, NULL, -downRangeMax, -downRangeMin);
				const bool io1Side0Pass = ioVal1->HistoryReleased(durationMS, NULL, downRangeMin, downRangeMax);
				const bool io1Side1Pass = ioVal1->HistoryReleased(durationMS, NULL, -downRangeMax, -downRangeMin);
				actionPrintf( bDisplayDebug, "%s (%s) - Impulse State (ImpulseStateReleased) - (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass)\n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ),(io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass) ? "SUCCESS" : "FAILED" );
				return (io0Side0Pass || io0Side1Pass || io1Side0Pass || io1Side1Pass);
			}
			default:
			{
				actionPrintf( bDisplayDebug, "%s (FAILED) - Impulse State (default B) - \n", parser_CActionFlags__Impulse_Data.NameFromValue( GetImpulse() ) );
				return false;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetIOValuesForImpulse
// PURPOSE	:	Returns whether or not the button impulse passes given the Impulse
// PARAMS	:	pController - Pointer to the physical controller 
//				impulse - Button impulse we would like to test against 
//				ioVal0 - First button state we pass back out to user
//				ioVal1 - Second button state we pass back out to user
// RETURNS	:	Whether or not the test has passed
/////////////////////////////////////////////////////////////////////////////////
bool CSimpleImpulseTest::GetIOValuesForImpulse( const CControl* pController, Impulse impulse, ioValue const * &ioVal0, ioValue const * &ioVal1 )
{
	Assert( pController );

	switch(impulse)
	{
		case ImpulseNone:						return false;
		case ImpulseAttackLight:				ioVal0 = &(pController->GetMeleeAttackLight()); break;
		case ImpulseAttackHeavy:				ioVal0 = &(pController->GetMeleeAttackHeavy()); break;	 
		case ImpulseAttackAlternate:			ioVal0 = &(pController->GetMeleeAttackAlternate()); break;	 
		case ImpulseBlock:						ioVal0 = &(pController->GetMeleeBlock()); break;
		case ImpulseFire:						ioVal0 = &(pController->GetPedAttack()); break;
		case ImpulseSpecialAbility:				ioVal0 = &(pController->GetPedSpecialAbility()); break;
		case ImpulseSpecialAbilitySecondary:	ioVal0 = &(pController->GetPedSpecialAbilitySecondary()); break;
		case ImpulseAnalogLeftRight:			ioVal0 = &(pController->GetPedWalkLeftRight()); break;
		case ImpulseAnalogUpDown:				ioVal0 = &(pController->GetPedWalkUpDown()); break;
		case ImpulseAnalogFromCenter:			ioVal0 = &(pController->GetPedWalkLeftRight());
												ioVal1 = &(pController->GetPedWalkUpDown()); break;
		default:								return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// CImpulseTest
/////////////////////////////////////////////////////////////////////////////////
CImpulseTest::CImpulseTest()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CImpulseTest::PostLoad( void )
{
	for(int i = 0; i < m_aImpulses.GetCount(); i++)
		m_aImpulses[i]->PostLoad();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CImpulseTest::Reset( void )
{
	m_ImpulseOp = CActionFlags::IO_OR_IMPULSE_1;
	for(int i = 0; i < m_aImpulses.GetCount(); i++)
		m_aImpulses[i]->Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CImpulseTest::Clear( void )
{
	for(int i = 0; i < m_aImpulses.GetCount(); i++)
	{
		m_aImpulses[i]->Clear();
		delete m_aImpulses[i];
	}
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CImpulseTest::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	for(int i = 0; i < m_aImpulses.GetCount(); i++)
		m_aImpulses[i]->PrintMemoryUsage( rTotalSize );
		
	Printf( "CImpulseTest: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DoTest
// PURPOSE	:	Test to determine if this object is suitable given the parameters
// PARAMS	:	pController - Pointer to the physical controller 
//				pImpulseCombo - Cached version of the current button combo
//				pReusableResults - Cache store to save off logic for future use
//				bDisplayDebug - Used to determine if this test is being debugged
// RETURNS	:	Whether or not the test has passed
/////////////////////////////////////////////////////////////////////////////////
bool CImpulseTest::DoTest( const CControl* pController, const CImpulseCombo* pImpulseCombo, CActionTestCache* pReusableResults, const bool bDisplayDebug ) const
{
	bool bImpulseTestsPassed = false;
	switch( GetImpulseOp() )
	{
		case CActionFlags::IO_OR_IMPULSE_1:
		{
			for(int i = 0; i < m_aImpulses.GetCount(); i++)
				bImpulseTestsPassed |= m_aImpulses[i]->DoTest( pController, pImpulseCombo, pReusableResults, bDisplayDebug );
			break;
		}
		case CActionFlags::IO_AND_IMPULSE_1:
		{
			bImpulseTestsPassed = true;
			for(int i = 0; i < m_aImpulses.GetCount(); i++)
				bImpulseTestsPassed &= m_aImpulses[i]->DoTest( pController, pImpulseCombo, pReusableResults, bDisplayDebug );
			break;
		}
		case CActionFlags::IO_XOR_IMPULSE_1:
		{
			// Can't allow more than 2 comparisons with bools
			if( m_aImpulses.GetCount() != 2 )
				break;

			bImpulseTestsPassed = m_aImpulses[0]->DoTest( pController, pImpulseCombo, pReusableResults, bDisplayDebug ) !=
								  m_aImpulses[1]->DoTest( pController, pImpulseCombo, pReusableResults, bDisplayDebug );
			break;
		}
		default:// Unknown CActionFlags::ImpulseOp type...
			break;
	};

	actionPrintf( bDisplayDebug, "CImpulseTest::DoTest - (%s) bImpulseTestsPassed = %s\n", GetName(), bImpulseTestsPassed ? "SUCCESS" : "FAILED" );
	return bImpulseTestsPassed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetImpulseByIndex
// PURPOSE	:	Return the CSimpleImpulseTest given the index
// PARAMS	:	nImpulseIdx - Index of the impulse we want
// RETURNS	:	Impulse test we desire
/////////////////////////////////////////////////////////////////////////////////
const CSimpleImpulseTest* CImpulseTest::GetImpulseByIndex( const u32 nImpulseIdx ) const
{
	Assertf( nImpulseIdx < GetNumImpulses(), "CImpulseTest::GetImpulseByIndex - Index (%d) is out of range (%d).", nImpulseIdx, GetNumImpulses() );
	return m_aImpulses[ nImpulseIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// CActionCondSpecial
//
// This handles the action suitability test for handling environmental and special
// case tests...
/////////////////////////////////////////////////////////////////////////////////
CActionCondSpecial::CActionCondSpecial()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionCondSpecial::Reset(void)
{
	m_SpecialTest.Clear();
	m_Filter.Reset();
	m_InvertSense = false;
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionCondSpecial::PrintMemoryUsage( size_t& rTotalSize )
{
	rTotalSize += sizeof( *this );
}
#endif

// Perform the test. If it passes it returns true.
const u32 SPECIAL_COND_CAN_BE_KNOCKED_OUT				= ATSTRINGHASH("CanBeKnockedOut", 0x0c06f249e);
const u32 SPECIAL_COND_CAN_PERFORM_DOG_TAKEDOWN			= ATSTRINGHASH("CanPerformDogTakedown", 0x6ba956cc);
const u32 SPECIAL_COND_CAN_BE_TAKEN_DOWN				= ATSTRINGHASH("CanBeTakenDown", 0x0185efa7f);
const u32 SPECIAL_COND_CAN_PREVENT_TAKE_DOWN			= ATSTRINGHASH("CanPreventTakeDown", 0xe9aec51a);
const u32 SPECIAL_COND_CAN_BE_STEALTH_KILLED			= ATSTRINGHASH("CanBeStealthKilled", 0x047441d9c);
const u32 SPECIAL_COND_CAN_PERFORM_CLOSE_QUARTER_KILL	= ATSTRINGHASH("CanPerformCloseQuarterKill", 0x4f2c6386);
const u32 SPECIAL_COND_TARGET_COLLISION_PROBE_TEST		= ATSTRINGHASH("TargetCollisionProbeTest", 0x0dcc938c5);
const u32 SPECIAL_COND_TARGET_COLLISION_PROBE_TEST_SIMPLE = ATSTRINGHASH("TargetCollisionProbeTestSimple", 0x902b8424);
const u32 SPECIAL_COND_IS_TARGET_WITHIN_FOV				= ATSTRINGHASH("IsTargetWithinFOV", 0x2deeb1ec);
const u32 SPECIAL_COND_HAS_FOUND_BREAKABLE_SURFACE		= ATSTRINGHASH("HasFoundBreakableSurface", 0x505b387f);
const u32 SPECIAL_COND_HAS_FOUND_VERTICAL_SURFACE		= ATSTRINGHASH("HasFoundVerticalSurface", 0x4413adba);
const u32 SPECIAL_COND_HAS_FOUND_HORITONTAL_SURFACE		= ATSTRINGHASH("HasFoundHorizontalSurface", 0x6e3cff8e);
const u32 SPECIAL_COND_CAN_RELOAD						= ATSTRINGHASH("CanReload", 0x06699fb64);
const u32 SPECIAL_COND_IS_RELOADING						= ATSTRINGHASH("IsReloading", 0x29f3f6ac);
const u32 SPECIAL_COND_IS_AWARE_OF_US					= ATSTRINGHASH("IsAwareOfUs", 0x005b70ed3);
const u32 SPECIAL_COND_IS_DEAD							= ATSTRINGHASH("IsDead", 0x0e2625733);
const u32 SPECIAL_COND_IS_ON_FOOT						= ATSTRINGHASH("IsOnFoot", 0x0d9c12bdc);
const u32 SPECIAL_COND_IS_FRIENDLY						= ATSTRINGHASH("IsFriendly", 0x07d789511);
const u32 SPECIAL_COND_IS_IN_COMBAT						= ATSTRINGHASH("IsInCombat", 0x1535de56);
const u32 SPECIAL_COND_IS_IN_COVER						= ATSTRINGHASH("IsInCover", 0xbff67458);
const u32 SPECIAL_COND_IS_USING_RAGDOLL					= ATSTRINGHASH("IsUsingRagdoll", 0x04ae9e467);
const u32 SPECIAL_COND_CAN_USE_MELEE_RAGDOLL			= ATSTRINGHASH("CanUseMeleeRagdoll", 0x0ba1d91b6);
const u32 SPECIAL_COND_IS_USING_NM						= ATSTRINGHASH("IsUsingNM", 0x0aa393553);
const u32 SPECIAL_COND_IS_CROUCHING						= ATSTRINGHASH("IsCrouching", 0x0ddbf0e0d);
const u32 SPECIAL_COND_IS_AIMING						= ATSTRINGHASH("IsAiming", 0x9d10c0bd);
const u32 SPECIAL_COND_IS_WEAPON_UNARMED				= ATSTRINGHASH("IsWeaponUnarmed", 0x471baa9c);
const u32 SPECIAL_COND_IS_WEAPON_VIOLENT				= ATSTRINGHASH("IsWeaponViolent", 0x0f3a03619);
const u32 SPECIAL_COND_IS_WEAPON_HEAVY					= ATSTRINGHASH("IsWeaponHeavy", 0x0c6e77ca5);
const u32 SPECIAL_COND_IS_IN_STEALTH_MODE				= ATSTRINGHASH("IsInStealthMode", 0x02b4279d6);
const u32 SPECIAL_COND_IS_ACTIVE_COMBATANT				= ATSTRINGHASH("IsActiveCombatant", 0x08e73a656);
const u32 SPECIAL_COND_IS_SUPPORT_COMBATANT				= ATSTRINGHASH("IsSupportCombatant", 0x091b492e1);
const u32 SPECIAL_COND_IS_INACTIVE_OBSERVER				= ATSTRINGHASH("IsInactiveObserver", 0xfecbdbcc);
const u32 SPECIAL_COND_IS_IN_MELEE						= ATSTRINGHASH("IsInMelee", 0x00b936a7e);
const u32 SPECIAL_COND_IS_USING_RAGE_ABILITY			= ATSTRINGHASH("IsUsingRageAbility", 0xdb37f8ad);
const u32 SPECIAL_COND_IS_ON_TOP_OF_CAR					= ATSTRINGHASH("IsOnTopOfCar", 0xac1b539d);
const u32 SPECIAL_COND_HAS_RAGE_ABILITY_EQUIPPED		= ATSTRINGHASH("HasRageAbilityEquipped", 0xbbbb6ae6);
const u32 SPECIAL_COND_FRIENDLY_TARGET_TEST				= ATSTRINGHASH("FriendlyTargetTest", 0x0f233601b);
const u32 SPECIAL_COND_ARMED_MELEE_TARGET_TEST			= ATSTRINGHASH("ArmedMeleeTargetTest", 0xb8a531bf);
const u32 SPECIAL_COND_HAS_SUPPORT_ALLY_TO_RIGHT		= ATSTRINGHASH("HasSupportAllyToRight", 0x969ccf9e);
const u32 SPECIAL_COND_HAS_SUPPORT_ALLY_TO_LEFT			= ATSTRINGHASH("HasSupportAllyToLeft", 0xb2d2bfd);
const u32 SPECIAL_COND_IS_UNDERWATER					= ATSTRINGHASH("IsUnderwater", 0xac1afaf2);
const u32 SPECIAL_COND_LAST_ACTION_WAS_A_RECOIL			= ATSTRINGHASH("LastActionWasARecoil", 0x77b18f37);
#if CNC_MODE_ENABLED
const u32 SPECIAL_COND_IS_ARCADE_TEAM_CNC_COP			= ATSTRINGHASH("IsArcadeTeamCNCCop", 0xC7701E04);
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DoTest
// PURPOSE	:	Test to determine if this object is suitable given the parameters
// PARAMS	:	pViewerPed - Ped that is doing the testing 
//				pTargetEntity - Ped that the testing ped is targeting
//				pActionResult - Action result associated with the viewer ped
//				pReusableResults - Cache store to save off logic for future use
//				bDisplayDebug - Used to determine if this test is being debugged
// RETURNS	:	Whether or not the test has passed
/////////////////////////////////////////////////////////////////////////////////
bool CActionCondSpecial::DoTest( const CPed* pViewerPed, const CEntity* pTargetEntity, const CActionResult* pActionResult, CActionTestCache* pReusableResults, const bool BANK_ONLY( bDisplayDebug ) ) const
{
	Assertf( pViewerPed, "CActionCondSpecial::DoTest - Invalid pViewerPed." );

	u32 nSpecialTestId = GetSpecialTestID();
	bool bInvertSense = GetInvertSense();
	
	if( SPECIAL_COND_IS_IN_MELEE == nSpecialTestId )
	{
		bool bIsRunningMelee = pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning );
		if( bIsRunningMelee )
		{
			CTaskMeleeActionResult* pMeleeActionResult = pViewerPed->GetPedIntelligence() ? pViewerPed->GetPedIntelligence()->GetTaskMeleeActionResult() : NULL;
			if( pMeleeActionResult )
			{
				if( pMeleeActionResult->IsNonMeleeHitReaction() )
				{
					bIsRunningMelee = false;
				}
				// Ignore armed standard attacks as "running melee"
				else if( pMeleeActionResult->IsAStandardAttack() )
				{
					const CObject* pVisibleWeaponObject = pViewerPed->GetWeaponManager() ? pViewerPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
					const CWeapon* pVisibleWeapon = pVisibleWeaponObject ? pVisibleWeaponObject->GetWeapon() : NULL;
					const CWeaponInfo* pVisibleWeaponInfo = pVisibleWeapon ? pVisibleWeapon->GetWeaponInfo() : NULL;
					if( pVisibleWeaponInfo && !pVisibleWeaponInfo->GetIsMelee() )
					{
						if( pViewerPed->IsPlayer() )
							bIsRunningMelee = !pMeleeActionResult->IsPastCriticalFrame();
						else
							bIsRunningMelee = false;
					}
				}
			}
		}

#if __BANK
		TUNE_GROUP_BOOL(MELEE_TEST, bTreatEveryoneAsRunningMelee, false);
		if(bTreatEveryoneAsRunningMelee)
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped (%s) %s in melee\n", GetSpecialTestName(), pViewerPed->GetDebugName(), bInvertSense ? "is" : "is not" );
			return false;
		}
#endif

		const bool bTestPassed = bInvertSense ? !bIsRunningMelee : bIsRunningMelee;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped (%s) %s in melee\n", GetSpecialTestName(), pViewerPed->GetDebugName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_BE_STEALTH_KILLED == nSpecialTestId ||
			 SPECIAL_COND_CAN_BE_TAKEN_DOWN == nSpecialTestId )
	{
		if( pViewerPed->m_nPhysicalFlags.bNotDamagedByMelee NOTFINAL_ONLY( || pViewerPed->IsDebugInvincible()) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Player is invincible\n", GetSpecialTestName() );
			return false;
		}

		if( SPECIAL_COND_CAN_BE_TAKEN_DOWN == nSpecialTestId && pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_PreventAllMeleeTakedowns ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Takedowns disabled on this ped\n", GetSpecialTestName() );
			return false;
		}

		
		if( SPECIAL_COND_CAN_BE_STEALTH_KILLED == nSpecialTestId && pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_PreventAllStealthKills ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Stealth takedowns disabled on this ped\n", GetSpecialTestName() );
			return false;
		}

		// Do not stealth kill if ped can't be damaged.
		if( pViewerPed->m_nPhysicalFlags.bNotDamagedByAnything )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped can't be damaged - bNotDamagedByAnything is set\n", GetSpecialTestName() );
			return false;
		}

		// No kills if player is currently respawning.
		CNetObjPlayer* netObjPlayer = pViewerPed->IsPlayer() ? static_cast<CNetObjPlayer *>(pViewerPed->GetNetworkObject()) : NULL;
		if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped can't be damaged - currently respawning!\n", GetSpecialTestName() );
			return false;
		}

		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>( pViewerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_USE_SCENARIO ) );
		if( pTaskScenario && pTaskScenario->GetScenarioInfo().GetIsFlagSet( CScenarioInfoFlags::NoMeleeTakedowns ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Playing a scenario that doesn't allow stealth kills\n", GetSpecialTestName() );
			return false;
		}

		// B*2566325: MP; Prevent melee takedowns if target player has been set as not targetable. 
		if (NetworkInterface::IsGameInProgress() && pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			const CPed *pTargetPed = static_cast<const CPed*>(pTargetEntity);
			if (pTargetPed && pTargetPed->IsPlayer() && pTargetPed->GetNetworkObject() && pViewerPed->GetPlayerInfo())
			{
				CNetObjPed* pPedTargetObj = SafeCast(CNetObjPed, pTargetPed->GetNetworkObject());
				if (pPedTargetObj && !pPedTargetObj->GetIsDamagableByPlayer(pViewerPed->GetPlayerInfo()->GetPhysicalPlayerIndex()))
				{
					actionPrintf( bDisplayDebug, "%s (FAILED) - Cannot use takedown on target player ped; !GetIsTargettableByPlayer() \n", GetSpecialTestName() );
					return false;
				}
			}
		}

		const bool bTestPassed = bInvertSense ? false : true;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s be %s\n", GetSpecialTestName(), bInvertSense ? "can" : "cannot", SPECIAL_COND_CAN_BE_STEALTH_KILLED == nSpecialTestId ? "stealth killed" : "taken down" );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_BE_KNOCKED_OUT == nSpecialTestId )
	{
		if( pViewerPed->m_nPhysicalFlags.bNotDamagedByMelee NOTFINAL_ONLY( || pViewerPed->IsDebugInvincible()) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Player is invincible\n", GetSpecialTestName() );
			return false;
		}

		if( pViewerPed->m_nPhysicalFlags.bNotDamagedByAnything )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped can't be damaged\n", GetSpecialTestName() );
			return false;
		}

		CNetObjPlayer* netObjPlayer = pViewerPed->IsPlayer() ? static_cast<CNetObjPlayer *>(pViewerPed->GetNetworkObject()) : NULL;
		if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped can't be damaged - currently respawning!\n", GetSpecialTestName() );
			return false;
		}

		// B*2566325: MP; Prevent melee takedowns if target player has been set as not targetable. 
		if (NetworkInterface::IsGameInProgress() && pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			const CPed *pTargetPed = static_cast<const CPed*>(pTargetEntity);
			if (pTargetPed && pTargetPed->IsPlayer() && pTargetPed->GetNetworkObject() && pViewerPed->GetPlayerInfo())
			{
				CNetObjPed* pPedTargetObj = SafeCast(CNetObjPed, pTargetPed->GetNetworkObject());
				if (pPedTargetObj && !pPedTargetObj->GetIsDamagableByPlayer(pViewerPed->GetPlayerInfo()->GetPhysicalPlayerIndex()))
				{
					actionPrintf( bDisplayDebug, "%s (FAILED) - Cannot use takedown on target player ped; !GetIsTargettableByPlayer() \n", GetSpecialTestName() );
					return false;
				}
			}
		}

		const CHealthConfigInfo* pHealthInfo = pViewerPed->GetPedHealthInfo();
		bool bHealthThresholdPassed = pViewerPed->GetHealth() <= ( pHealthInfo->GetInjuredHealthThreshold() + 1.0f );
		const bool bCanBeKnockedOut = pViewerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceScriptControlledKnockout ) || bHealthThresholdPassed ? true : false;
		const bool bTestPassed = bInvertSense ? !bCanBeKnockedOut : bCanBeKnockedOut;
		if( !bTestPassed  )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped cannot be knocked out (current=%f required=%f)\n", GetSpecialTestName(), pViewerPed->GetHealth(), pHealthInfo->GetInjuredHealthThreshold() + 1.0f );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_PERFORM_DOG_TAKEDOWN == nSpecialTestId )
	{
		if( pViewerPed->m_nPhysicalFlags.bNotDamagedByMelee NOTFINAL_ONLY( || pViewerPed->IsDebugInvincible()) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Player is invincible\n", GetSpecialTestName() );
			return false;
		}

		if( pViewerPed->m_nPhysicalFlags.bNotDamagedByAnything )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped can't be damaged\n", GetSpecialTestName() );
			return false;
		}

		CNetObjPlayer* netObjPlayer = pViewerPed->IsPlayer() ? static_cast<CNetObjPlayer *>(pViewerPed->GetNetworkObject()) : NULL;
		if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped can't be damaged - currently respawning!\n", GetSpecialTestName() );
			return false;
		}

		const CHealthConfigInfo* pHealthInfo = pViewerPed->GetPedHealthInfo();
		bool bCanPerformDogTakedown = pViewerPed->GetHealth() <= pHealthInfo->GetDogTakedownThreshold();
		const bool bTestPassed = bInvertSense ? !bCanPerformDogTakedown : bCanPerformDogTakedown;
		if( !bTestPassed  )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - %s perform dog takedown (current=%f threshold=%f)\n", GetSpecialTestName(), bInvertSense ? "Can" : "Cannot", pViewerPed->GetHealth(), pHealthInfo->GetDogTakedownThreshold() );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_PERFORM_CLOSE_QUARTER_KILL == nSpecialTestId )
	{
		// Check against allowed lethal moves
		const bool bAreLethalMovesDisabled = pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_SuppressLethalMeleeActions );
		if( bAreLethalMovesDisabled && pActionResult && pActionResult->GetIsLethal() )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Lethal actions are disabled\n", GetSpecialTestName() );
			return false;
		}

		// Check against allowed takedown moves
		const bool bAreTakedownMovesDisabled = pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_SuppressTakedownMeleeActions );
		if( bAreTakedownMovesDisabled && pActionResult && ( pActionResult->GetIsATakedown() || pActionResult->GetIsAStealthKill() ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Takedown and stealth actions are disabled\n", GetSpecialTestName() );
			return false;
		}

		if(NetworkInterface::IsGameInProgress() && pActionResult && pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			const CPed *pTargetPed = static_cast<const CPed*>(pTargetEntity);

			if( (pActionResult->GetIsLethal() || pActionResult->GetIsATakedown() || pActionResult->GetIsAStealthKill()) )
			{
				// Check lethal moves against friendly peds in MP.
				if(pViewerPed->IsLocalPlayer() && 
					pViewerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMPFriendlyLethalMeleeActions) && 
					pTargetPed->GetPlayerInfo())
				{
					bool bIsFriend = false;
					if (pTargetPed->GetPlayerInfo()->m_FriendStatus == CPlayerInfo::FRIEND_UNKNOWN)
					{
						// This is a slow call and should only be done once per ped, and then cached. Cannot be polled every frame.
						bIsFriend = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), pTargetPed->GetPlayerInfo()->m_GamerInfo.GetGamerHandle());
						pTargetPed->GetPlayerInfo()->m_FriendStatus = bIsFriend ? CPlayerInfo::FRIEND_ISFRIEND : CPlayerInfo::FRIEND_NOTFRIEND;
					}
					else if (pTargetPed->GetPlayerInfo()->m_FriendStatus == CPlayerInfo::FRIEND_ISFRIEND)
					{
						bIsFriend = true;
					}

					if (bIsFriend)
					{
						actionPrintf( bDisplayDebug, "%s (FAILED) - Lethal MP friendly actions are disabled\n", GetSpecialTestName() );
						return false;
					}
				}

				bool bFriendly = pViewerPed->GetPedIntelligence()->IsFriendlyWith(*static_cast<const CPed*>(pTargetEntity));	
				if(bFriendly)
				{
					//! Override Angle if this is a friendly ped, so that it isn't as severe as normal.
					TUNE_GROUP_FLOAT( MELEE, fFriendlyKillMoveAngularRangeX, -45.0f, -180.0f, 180.0f, 0.001f );
					TUNE_GROUP_FLOAT( MELEE, fFriendlyKillMoveAngularRangeY, 45.0f, -180.0f, 180.0f, 0.001f );
					
					float fAngularRangeX = fwAngle::LimitRadianAngle( DegToRadMoreAccurate( fFriendlyKillMoveAngularRangeX ) );
					float fAngularRangeY = fwAngle::LimitRadianAngle( DegToRadMoreAccurate( fFriendlyKillMoveAngularRangeY ) );

					Vec3V ViewedPosition = static_cast<const CPed*>(pTargetEntity)->GetBonePositionCachedV( BONETAG_NECK );
					Vec3V ViewedDirection = NormalizeSafe( ViewedPosition - pViewerPed->GetTransform().GetPosition(), pViewerPed->GetTransform().GetB() );

					const float	fViewerHeadingRads = fwAngle::LimitRadianAngle( pViewerPed->GetTransform().GetHeading() );
					const float	fViewerToViewedHeadingRads = fwAngle::LimitRadianAngle( rage::Atan2f( -ViewedDirection.GetXf(), ViewedDirection.GetYf() ) );
					float testAngleRads = fwAngle::LimitRadianAngle( fViewerToViewedHeadingRads - fViewerHeadingRads );

					bool bInFriendlyAngle = false;
					
					if( fAngularRangeX <= fAngularRangeY )
					{
						if(	testAngleRads >= fAngularRangeX &&
							testAngleRads <= fAngularRangeY )
							bInFriendlyAngle = true;
					}
					else
					{
						if(	testAngleRads >= fAngularRangeX ||
							testAngleRads <= fAngularRangeY )
							bInFriendlyAngle = true;
					}

					// Do the test.
					if( !bInFriendlyAngle )
					{
						actionPrintf( bDisplayDebug, "%s (FAILED) - Lethal MP friendly actions out of angular range\n", GetSpecialTestName() );
						return false;
					}
				}
			}	
		}

		// Explosive melee
		if( pViewerPed->IsPlayer() && pViewerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet( CPlayerResetFlags::PRF_EXPLOSIVE_MELEE_ON ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Explosive melee is on\n", GetSpecialTestName() );
			return false;
		}
		
		// Do not allow weapons that do not support close quarter kills
		const CWeaponInfo* pWeaponInfo = pViewerPed->GetWeaponManager() ? pViewerPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		bool bCanStealthKill = pWeaponInfo ? pWeaponInfo->GetAllowCloseQuarterKills() : false;
		if( !bCanStealthKill )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Weapon doesn't allow close quarter kills\n", GetSpecialTestName() );
			return false;
		}

		// Do not allow stealth kills unless the equipped weapon is ready
		const CWeapon* pWeaponUsable = pViewerPed->GetWeaponManager() ? pViewerPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if( pActionResult && pActionResult->GetRequiresAmmo() && pWeaponUsable && pWeaponUsable->GetState() != CWeapon::STATE_READY )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Weapon %s ready\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_TARGET_COLLISION_PROBE_TEST == nSpecialTestId )
	{
		bool bCollisionDetected = false;
		if( pTargetEntity )
		{
			bool bIsPed = pTargetEntity->GetIsTypePed();

			// Only perform probe if we need to
			if( pReusableResults && pReusableResults->m_bHavePedLosTest )				
				bCollisionDetected = !pReusableResults->m_bPedLosTestClear;
			else
			{	
				float fTimeToReachEntity = 0.0f;
				int nBoneIdxCache = -1;

				Vec3V vStartPos( VEC3_ZERO );
				Vec3V vEndPos( VEC3_ZERO );
				Vec3V vDirection( VEC3_ZERO );

				// Knee height collision detection
				CActionManager::TargetTypeGetPosDirFromEntity( vStartPos, vDirection, CActionFlags::TT_PED_KNEE_RIGHT, pViewerPed, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );

				if( bIsPed )
					CActionManager::TargetTypeGetPosDirFromEntity( vEndPos, vDirection, CActionFlags::TT_PED_KNEE_RIGHT, pTargetEntity, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );
				else
					CActionManager::TargetTypeGetPosDirFromEntity( vEndPos, vDirection, CActionFlags::TT_ENTITY_ROOT, pTargetEntity, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );
				
				// Slightly scale the start/end positions to compensate for the ped capsule
				static dev_float CAPSULE_RADIUS = 0.2f;
				vStartPos += Scale( vDirection, ScalarV( CAPSULE_RADIUS ) );
				vEndPos += Scale( Negate( vDirection ), ScalarV( CAPSULE_RADIUS ) );

				WorldProbe::CShapeTestFixedResults<> pedCapsuleResults;
				WorldProbe::CShapeTestCapsuleDesc pedCapsuleDesc;

				pedCapsuleDesc.SetResultsStructure( &pedCapsuleResults );
				pedCapsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStartPos ), VEC3V_TO_VECTOR3( vEndPos ), CAPSULE_RADIUS );
				pedCapsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE );

				const int PED_EXCLUSION_MAX = 2;
				const CEntity* paExclusionList[PED_EXCLUSION_MAX];

				paExclusionList[0] = pViewerPed;
				paExclusionList[1] = pTargetEntity;
				pedCapsuleDesc.SetExcludeEntities( paExclusionList, PED_EXCLUSION_MAX );
				pedCapsuleDesc.SetContext( WorldProbe::LOS_CombatAI );
#if DEBUG_DRAW
				char szText[128];
				static dev_bool bRenderPedCapsuleTest = false;
				if( bRenderPedCapsuleTest )
					CCombatMeleeDebug::sm_debugDraw.AddCapsule( vStartPos, vEndPos, CAPSULE_RADIUS, Color_green, 250, atStringHash( "KNEE_COLLISION_CAPSULE" ), false );
#endif
				if( WorldProbe::GetShapeTestManager()->SubmitTest( pedCapsuleDesc ) )
				{
					for( WorldProbe::ResultIterator it = pedCapsuleResults.begin(); it < pedCapsuleResults.last_result(); ++it )
					{
						if( PGTAMATERIALMGR->UnpackMtlId( it->GetHitMaterialId() ) == PGTAMATERIALMGR->g_idCarVoid )
							continue;

						CEntity* pHitEntity = CPhysics::GetEntityFromInst( it->GetHitInst() );
						if( pHitEntity )
						{
							bCollisionDetected = true;
#if DEBUG_DRAW
							if( bRenderPedCapsuleTest )
							{
								formatf( szText, "KNEE_COLLISION_CAPSULE_HIT" );
								CCombatMeleeDebug::sm_debugDraw.AddSphere( it->GetHitPositionV(), 0.05f, Color_red, atStringHash( szText ), true );
							}
#endif
							break;
						}
					}
				}

				// Only perform the secondary collision detection if necessary
				if( !bCollisionDetected && bIsPed )
				{
					pedCapsuleResults.Reset();

					// Chest height collision detection
					CActionManager::TargetTypeGetPosDirFromEntity( vStartPos, vDirection, CActionFlags::TT_PED_CHEST, pViewerPed, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );
					CActionManager::TargetTypeGetPosDirFromEntity( vEndPos, vDirection, CActionFlags::TT_PED_CHEST, pTargetEntity, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );

					// Slightly scale the start/end positions to compensate for the ped capsule
					vStartPos += Scale( vDirection, ScalarV( CAPSULE_RADIUS ) );
					vEndPos += Scale( Negate( vDirection ), ScalarV( CAPSULE_RADIUS ) );

					pedCapsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStartPos ), VEC3V_TO_VECTOR3( vEndPos ), CAPSULE_RADIUS );
#if DEBUG_DRAW
					if( bRenderPedCapsuleTest )
						CCombatMeleeDebug::sm_debugDraw.AddCapsule( vStartPos, vEndPos, CAPSULE_RADIUS, Color_green, 250, atStringHash( "CHEST_COLLISION_CAPSULE" ), false );
#endif
					if( WorldProbe::GetShapeTestManager()->SubmitTest( pedCapsuleDesc ) )
					{
						for( WorldProbe::ResultIterator it = pedCapsuleResults.begin(); it < pedCapsuleResults.last_result(); ++it )
						{
							if( PGTAMATERIALMGR->UnpackMtlId( it->GetHitMaterialId() ) == PGTAMATERIALMGR->g_idCarVoid )
								continue;

							CEntity* pHitEntity = CPhysics::GetEntityFromInst( it->GetHitInst() );
							if( pHitEntity )
							{
								bCollisionDetected = true;
#if DEBUG_DRAW
								if( bRenderPedCapsuleTest )
								{
									formatf( szText, "CHEST_COLLISION_CAPSULE_HIT" );
									CCombatMeleeDebug::sm_debugDraw.AddSphere( it->GetHitPositionV(), 0.05f, Color_red, atStringHash( szText ), true );
								}
#endif
								break;
							}
						}
					}
				}

				// Cache off the result
				if( pReusableResults )
				{
					pReusableResults->m_bHavePedLosTest = true;
					pReusableResults->m_bPedLosTestClear = !bCollisionDetected;
				}
			}
		}

		const bool bTestPassed = bInvertSense ? bCollisionDetected : !bCollisionDetected;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Collision %s detected\n", GetSpecialTestName(), bInvertSense ? "was not" : "was" );
			return false;
		}
	}
	else if( SPECIAL_COND_TARGET_COLLISION_PROBE_TEST_SIMPLE == nSpecialTestId )
	{
		bool bCollisionDetected = false;
		if( pTargetEntity )
		{
			// Only perform probe if we need to
			if( pReusableResults && pReusableResults->m_bHavePedLosTestSimple )				
				bCollisionDetected = !pReusableResults->m_bPedLosTestClearSimple;
			else
			{	
				float fTimeToReachEntity = 0.0f;
				int nBoneIdxCache = -1;

				Vec3V vStartPos( VEC3_ZERO );
				Vec3V vEndPos( VEC3_ZERO );
				Vec3V vDirection( VEC3_ZERO );

				CActionManager::TargetTypeGetPosDirFromEntity( vStartPos, vDirection, CActionFlags::TT_ENTITY_ROOT, pViewerPed, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );
				// While ragdolled or dead and using a static pose we maintain whatever offset there had been between the root and mover while in an animated state which means the entity position won't be
				// very representative of the character's 'root'.  Using the root bone (or SPINE1, in this case) position should be much more consistent
				if( pTargetEntity->GetIsTypePed() && (static_cast<const CPed*>(pTargetEntity)->IsDead() || static_cast<const CPed*>(pTargetEntity)->GetUsingRagdoll() ) )
				{
					CActionManager::TargetTypeGetPosDirFromEntity( vEndPos, vDirection, CActionFlags::TT_PED_ROOT, pTargetEntity, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );
				}
				else
				{
					CActionManager::TargetTypeGetPosDirFromEntity( vEndPos, vDirection, CActionFlags::TT_ENTITY_ROOT, pTargetEntity, pViewerPed, pViewerPed->GetTransform().GetPosition(), fTimeToReachEntity, nBoneIdxCache );
				}

				static dev_float CAPSULE_RADIUS = 0.2f;

				// Slightly scale the start/end positions to compensate for the ped capsule
				vStartPos += Scale( vDirection, ScalarV( CAPSULE_RADIUS ) );
				vEndPos += Scale( Negate( vDirection ), ScalarV( CAPSULE_RADIUS ) );

				WorldProbe::CShapeTestFixedResults<> pedCapsuleResults;
				WorldProbe::CShapeTestCapsuleDesc pedCapsuleDesc;

				pedCapsuleDesc.SetResultsStructure( &pedCapsuleResults );
				pedCapsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStartPos ), VEC3V_TO_VECTOR3( vEndPos ), CAPSULE_RADIUS );
				pedCapsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE );

				const int PED_EXCLUSION_MAX = 2;
				const CEntity* paExclusionList[PED_EXCLUSION_MAX];

				paExclusionList[0] = pViewerPed;
				paExclusionList[1] = pTargetEntity;
				pedCapsuleDesc.SetExcludeEntities( paExclusionList, PED_EXCLUSION_MAX );
				pedCapsuleDesc.SetContext( WorldProbe::LOS_CombatAI );
#if DEBUG_DRAW
				char szText[128];
				static dev_bool bRenderPedCapsuleTestSimple = false;
				if( bRenderPedCapsuleTestSimple )
					CCombatMeleeDebug::sm_debugDraw.AddCapsule( vStartPos, vEndPos, CAPSULE_RADIUS, Color_green, 250, atStringHash( "ROOT_COLLISION_CAPSULE" ), false );
#endif
				if( WorldProbe::GetShapeTestManager()->SubmitTest( pedCapsuleDesc ) )
				{
					for( WorldProbe::ResultIterator it = pedCapsuleResults.begin(); it < pedCapsuleResults.last_result(); ++it )
					{
						if( PGTAMATERIALMGR->UnpackMtlId( it->GetHitMaterialId() ) == PGTAMATERIALMGR->g_idCarVoid )
							continue;

						CEntity* pHitEntity = CPhysics::GetEntityFromInst( it->GetHitInst() );
						if( pHitEntity )
						{
							bCollisionDetected = true;
#if DEBUG_DRAW
							if( bRenderPedCapsuleTestSimple )
							{
								formatf( szText, "ROOT_COLLISION_CAPSULE_HIT" );
								CCombatMeleeDebug::sm_debugDraw.AddSphere( it->GetHitPositionV(), 0.05f, Color_red, 250, atStringHash( szText ), true );
							}
#endif
							break;
						}
					}
				}

				// Cache off the result
				if( pReusableResults )
				{
					pReusableResults->m_bHavePedLosTestSimple = true;
					pReusableResults->m_bPedLosTestClearSimple = !bCollisionDetected;
				}
			}
		}

		const bool bTestPassed = bInvertSense ? bCollisionDetected : !bCollisionDetected;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Simple collision %s detected\n", GetSpecialTestName(), bInvertSense ? "was not" : "was" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_TARGET_WITHIN_FOV == nSpecialTestId )
	{
		bool bIsVisible = false;
		if( pTargetEntity )
		{
			// Only perform probe if we need to
			if( pReusableResults && pReusableResults->m_bHaveFOVTest )				
				bIsVisible = pReusableResults->m_bFOVClear;
			else
			{	
				const CPedIntelligence* pPedIntelligence = pViewerPed->GetPedIntelligence();
				if( pPedIntelligence )
					bIsVisible = pPedIntelligence->GetPedPerception().ComputeFOVVisibility( pTargetEntity, NULL );

				// Cache off the result
				if( pReusableResults )
				{
					pReusableResults->m_bHaveFOVTest = true;
					pReusableResults->m_bFOVClear = bIsVisible;
				}
			}
		}

		const bool bTestPassed = bInvertSense ? !bIsVisible : bIsVisible;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Target %s in FOV\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_HAS_FOUND_BREAKABLE_SURFACE == nSpecialTestId ||
			 SPECIAL_COND_HAS_FOUND_VERTICAL_SURFACE == nSpecialTestId || 
			 SPECIAL_COND_HAS_FOUND_HORITONTAL_SURFACE == nSpecialTestId )
	{
		// Do not allow it while on stairs
		if( pViewerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnStairs ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped is on stairs\n", GetSpecialTestName() );
			return false;
		}

		bool bHasFoundSurface = false;

		// Only perform probe if we need to
		if( pReusableResults && pReusableResults->m_bHaveSurfaceCollisionResults ) 
		{
			if( SPECIAL_COND_HAS_FOUND_BREAKABLE_SURFACE == nSpecialTestId )	
				bHasFoundSurface = ( pReusableResults->m_bFoundVerticalSurface && pReusableResults->m_bFoundBreakableSurface );
			else if( SPECIAL_COND_HAS_FOUND_VERTICAL_SURFACE == nSpecialTestId )
				bHasFoundSurface = pReusableResults->m_bFoundVerticalSurface;
			else
				bHasFoundSurface = pReusableResults->m_bFoundHorizontalSurface;
		}
		else
		{	
			// Super special case
			Vec3V vRootSurfacePosition( VEC3_ZERO ); 
			Vec3V vHighSurfacePosition( VEC3_ZERO ); 
			Vec3V vDirection( VEC3_ZERO ); 
			int rnBoneCacheIdx = -1;

			// Right now this length is set to be the activation distance of surface attacks
			static dev_float sfDefaultSurfaceProbeLength = 1.75f;
			CHoming* pHoming = pActionResult ? pActionResult->GetHoming() : NULL;
			float fSurfaceProbeLength = pHoming ? pHoming->GetActivationDistance() : sfDefaultSurfaceProbeLength;
			bool bFoundBreakableSurface = false, bFoundHorizontalSurface = false, bFoundVerticalSurface = false;
			if ( CActionManager::TargetTypeGetPosDirFromEntity( vRootSurfacePosition,
																vDirection,
																CActionFlags::TT_SURFACE_PROBE,
																NULL,
																pViewerPed,
																pViewerPed->GetTransform().GetPosition(),
																0.0f,
																rnBoneCacheIdx, 
																fSurfaceProbeLength,
																CActionManager::ms_fLowSurfaceRootOffsetZ ) )
			{
				bFoundHorizontalSurface = true;

				// Test to see if we can find a high surface
				if( CActionManager::TargetTypeGetPosDirFromEntity( vHighSurfacePosition,
																   vDirection,
																   CActionFlags::TT_SURFACE_PROBE,
																   NULL,
																   pViewerPed,
																   pViewerPed->GetTransform().GetPosition(),
																   0.0f,
																   rnBoneCacheIdx, 
																   fSurfaceProbeLength,
																   CActionManager::ms_fHighSurfaceRootOffsetZ,
																   &bFoundBreakableSurface ) )
				{
					// Zero out the height difference as we only care about x/y
					vRootSurfacePosition.SetZ( 0.0f );
					vHighSurfacePosition.SetZ( 0.0f );

					static dev_float sfDistanceThresholdSq = 0.15f;
					float fSurfaceDiffDistSq = MagSquared( vRootSurfacePosition - vHighSurfacePosition ).Getf();
					if( fSurfaceDiffDistSq < sfDistanceThresholdSq )
					{
						bFoundVerticalSurface = true;
						bFoundHorizontalSurface = false;
					}
					else
						bFoundBreakableSurface = false;
				}
			}
		
			// Cache off the result
			if( pReusableResults )
			{
				pReusableResults->m_bHaveSurfaceCollisionResults = true;
				pReusableResults->m_bFoundBreakableSurface = bFoundBreakableSurface;
				pReusableResults->m_bFoundVerticalSurface = bFoundVerticalSurface;
				pReusableResults->m_bFoundHorizontalSurface = bFoundHorizontalSurface;
			}

			// Cache off the temp result
			if( SPECIAL_COND_HAS_FOUND_BREAKABLE_SURFACE == nSpecialTestId )	
				bHasFoundSurface = ( bFoundBreakableSurface && bFoundVerticalSurface );
			else if( SPECIAL_COND_HAS_FOUND_VERTICAL_SURFACE == nSpecialTestId )
				bHasFoundSurface = bFoundVerticalSurface;
			else
				bHasFoundSurface = bFoundHorizontalSurface;
		}

		const bool bTestPassed = bInvertSense ? !bHasFoundSurface : bHasFoundSurface;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - %s surface collision %s detected\n", GetSpecialTestName(), ( SPECIAL_COND_HAS_FOUND_BREAKABLE_SURFACE == nSpecialTestId ) ? "Breakable" : ( SPECIAL_COND_HAS_FOUND_VERTICAL_SURFACE == nSpecialTestId ) ? "Vertical" : "Horizontal", bInvertSense ? "was" : "was not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_ON_FOOT == nSpecialTestId )
	{
		EstimatedPose eEstimatePose = POSE_UNKNOWN;
		if( pReusableResults && pReusableResults->m_eEstimatePose != POSE_UNKNOWN ) 
			eEstimatePose = pReusableResults->m_eEstimatePose;
		else
			eEstimatePose = pViewerPed->EstimatePose(); 

		bool bEnteringVehicle = pViewerPed->GetIsInVehicle() ||
			pViewerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE_SEAT );

		if (!bEnteringVehicle && pViewerPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) && pViewerPed->GetIsAttached())
		{
			//! If a clone is attached, don't consider as on foot. owner may be further on in task, and it won't be possible to interrupt safely (e.g.
			//! it may be ok on our machine, but probably not ok on theirs).
			if(pViewerPed->IsNetworkClone())
			{
				bEnteringVehicle = true;
			}
			else
			{
				const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pViewerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if(pEnterVehicleTask)
				{
					const aiTask* pSubTask = pEnterVehicleTask->GetSubTask();
					const s32 iState = pEnterVehicleTask->GetState();

					if(iState > CTaskEnterVehicle::State_OpenDoor)
					{	
						bEnteringVehicle = true;
					}
					else if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
					{
						bool bOkToAbort = (pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoor 
											|| pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorBlend 
											|| pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorCombat 
											|| pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorWater );
						bEnteringVehicle = !bOkToAbort;
					}
				}
			}
		}

		const bool bIsOnFoot = !bEnteringVehicle && (pViewerPed->GetIsOnFoot() || eEstimatePose == POSE_STANDING) ;
		const bool bTestPassed = bInvertSense ? !bIsOnFoot : bIsOnFoot;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s on foot\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_FRIENDLY == nSpecialTestId )
	{
		// Friendly checks
		bool bIsFriendly = false;
		if( pTargetEntity && pTargetEntity->GetIsTypePed() )
		{
			const CPed* pTargetPed = static_cast<const CPed*>(pTargetEntity);
			bIsFriendly = CActionManager::ArePedsFriendlyWithEachOther( pViewerPed, pTargetPed ) && !pViewerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackFriendly );

			const bool bTestPassed = bInvertSense ? !bIsFriendly : bIsFriendly;
			if( !bTestPassed )
			{
				actionPrintf( bDisplayDebug, "%s (FAILED) - Acting ped %s friendly with target ped\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
				return false;
			}
		}
	}
	else if( SPECIAL_COND_IS_IN_COMBAT == nSpecialTestId )
	{
		const bool bIsInCombat = pViewerPed->GetPedIntelligence() ? pViewerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COMBAT ) : false;
		const bool bTestPassed = bInvertSense ? !bIsInCombat : bIsInCombat;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s in combat\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_IN_COVER == nSpecialTestId )
	{
		const bool bIsInCover = pViewerPed->GetPedIntelligence() ? pViewerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COVER ) : false;
		const bool bTestPassed = bInvertSense ? !bIsInCover : bIsInCover;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s in cover\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_RELOAD == nSpecialTestId )
	{
		const CWeapon* pWeaponUsable = pViewerPed->GetWeaponManager() ? pViewerPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		bool bCanReload = pWeaponUsable ? pWeaponUsable->GetCanReload() : true;
		const bool bTestPassed = bInvertSense ? !bCanReload : bCanReload;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Weapon %s reload\n", GetSpecialTestName(), bInvertSense ? "can" : "cannot" );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_PREVENT_TAKE_DOWN == nSpecialTestId )
	{
		if( pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_PreventAllMeleeTakedowns ) ||
			pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_PreventFailedMeleeTakedowns ) ||
			pViewerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Failed takedowns disabled on this ped\n", GetSpecialTestName() );
			return false;
		}

		Assertf( pViewerPed->GetPedIntelligence(), "Invalid opponent ped intelligence." );
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>( pViewerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_USE_SCENARIO ) );
		if( pTaskScenario && pTaskScenario->GetScenarioInfo().GetIsFlagSet( CScenarioInfoFlags::NoMeleeTakedowns ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Playing a scenario that doesn't allow takedowns\n", GetSpecialTestName() );
			return false;
		}

		Assertf( pViewerPed->GetPedModelInfo(), "Invalid ped model info." );
		float fTakedownProbability = pViewerPed->GetPedModelInfo()->GetPersonalitySettings().GetTakedownProbability();
		bool bCanPreventTakedown = ( pReusableResults && pReusableResults->m_fRandomProbability > fTakedownProbability );
		const bool bTestPassed = bInvertSense ? !bCanPreventTakedown : bCanPreventTakedown;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s prevent takedown\n", GetSpecialTestName(), bInvertSense ? "can" : "cannot" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_RELOADING == nSpecialTestId )
	{
		bool bIsReloading = false;
		if( pViewerPed->GetPedIntelligence() && pViewerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_RELOAD_GUN ) )
			bIsReloading = true;

		const bool bTestPassed = bInvertSense ? !bIsReloading : bIsReloading;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s reloading\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_AWARE_OF_US == nSpecialTestId )
	{
		bool bOpponentIsAwareOfUs = false;
		if( pViewerPed && pViewerPed->GetPedIntelligence() )
		{
			CPedPerception& pedPercention = pViewerPed->GetPedIntelligence()->GetPedPerception();
			if( pedPercention.ComputeFOVVisibility( pTargetEntity, NULL ) )
			{
				bOpponentIsAwareOfUs = true;
			}
		}

		const bool bTestPassed = bInvertSense ? !bOpponentIsAwareOfUs : bOpponentIsAwareOfUs;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s aware of us\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_DEAD == nSpecialTestId )
	{
		bool bIsDead = pViewerPed->IsDead() || pViewerPed->IsDeadByMelee();
		const bool bTestPassed = bInvertSense ? !bIsDead : bIsDead;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s dead\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_FRIENDLY_TARGET_TEST == nSpecialTestId )
	{
		bool bFriendlyIsInDesiredDirection = false;
		if( pReusableResults && pReusableResults->m_bHaveFriendlyTargetTestResults )
			bFriendlyIsInDesiredDirection = pReusableResults->m_bFriendlyIsInDesiredDirection;
		else
		{
			static dev_float sfFriendlyDistThresholdSq = rage::square( 2.5f );
			static dev_float sfMinFriendlyDotThreshold = 0.35f;
			static dev_float sfMaxFriendlyDotThreshold = 0.85f;	
			Vec3V vTestPosition( V_ZERO );
			bFriendlyIsInDesiredDirection = CActionManager::IsPedTypeWithinAngularThreshold( pViewerPed, vTestPosition, pTargetEntity, CPedModelInfo::ms_fPedRadius, sfFriendlyDistThresholdSq, sfMinFriendlyDotThreshold, sfMaxFriendlyDotThreshold );
			
			// Cache off the result
			if( pReusableResults )
			{
				pReusableResults->m_bHaveFriendlyTargetTestResults = true;
				pReusableResults->m_bFriendlyIsInDesiredDirection = bFriendlyIsInDesiredDirection;
			}
		}

		const bool bTestPassed = bInvertSense ? bFriendlyIsInDesiredDirection : !bFriendlyIsInDesiredDirection;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Friendly ped %s in the desired direction\n", GetSpecialTestName(), bInvertSense ? "is not" : "is" );
			return false;
		}
	}
	else if( SPECIAL_COND_ARMED_MELEE_TARGET_TEST == nSpecialTestId )
	{
		bool bAiIsInDesiredDirection = false;
		if( pReusableResults && pReusableResults->m_bHaveAiTargetTestResults )
			bAiIsInDesiredDirection = pReusableResults->m_bAiIsInDesiredDirection;
		else
		{
			static dev_float sfAiDistThresholdSq = rage::square( 2.0f );
			static dev_float sfMinAiDotThreshold = 0.0f;
			static dev_float sfMaxAiDotThreshold = 0.85f;
			static dev_float sfLookAheadTime = 0.25f;

			Vec3V vTestPosition( 0.15f, -0.2f, 0.0f );
			bAiIsInDesiredDirection = CActionManager::IsPedTypeWithinAngularThreshold( pViewerPed, vTestPosition, pTargetEntity, CPedModelInfo::ms_fPedRadius, sfAiDistThresholdSq, sfMinAiDotThreshold, sfMaxAiDotThreshold, sfLookAheadTime, false, true );
			
			// Cache off the result
			if( pReusableResults )
			{
				pReusableResults->m_bHaveAiTargetTestResults = true;
				pReusableResults->m_bAiIsInDesiredDirection = bAiIsInDesiredDirection;
			}
		}

		const bool bTestPassed = bInvertSense ? bAiIsInDesiredDirection : !bAiIsInDesiredDirection;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ai ped %s in the desired direction\n", GetSpecialTestName(), bInvertSense ? "is not" : "is" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_IN_STEALTH_MODE == nSpecialTestId )
	{
		bool bForcePassTest = false;
#if FPS_MODE_SUPPORTED
		TUNE_GROUP_BOOL(BUG_2068006, IGNORE_STEALTH_CONDITION_FPS, true);
		if (IGNORE_STEALTH_CONDITION_FPS && pViewerPed->IsFirstPersonShooterModeEnabledForPlayer(false) && bInvertSense == true)
		{
			bForcePassTest = true;
		}
#endif // FPS_MODE_SUPPORTED

		bool bInStealthMode = pViewerPed->GetMotionData()->GetUsingStealth();
		const bool bTestPassed = bForcePassTest ? true : (bInvertSense ? !bInStealthMode : bInStealthMode);
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s using stealth clip animations\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_ACTIVE_COMBATANT == nSpecialTestId )
	{
		bool bIsActiveCombatant = false;
		CTaskMelee*	pMeleeTask = pViewerPed->GetPedIntelligence()->GetTaskMelee(); 
		if( pMeleeTask )
			bIsActiveCombatant = pMeleeTask->GetQueueType() == CTaskMelee::QT_ActiveCombatant;
		
		const bool bTestPassed = bInvertSense ? !bIsActiveCombatant : bIsActiveCombatant;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s an active combatant\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_SUPPORT_COMBATANT == nSpecialTestId )
	{
		bool bIsSupportCombatant = false;
		CTaskMelee*	pMeleeTask = pViewerPed->GetPedIntelligence()->GetTaskMelee(); 
		if( pMeleeTask )
			bIsSupportCombatant = pMeleeTask->GetQueueType() == CTaskMelee::QT_SupportCombatant;

		const bool bTestPassed = bInvertSense ? !bIsSupportCombatant : bIsSupportCombatant;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s a support combatant\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
		
	}
	else if( SPECIAL_COND_IS_INACTIVE_OBSERVER == nSpecialTestId )
	{
		bool bInactiveObserver = false; 
		CTaskMelee*	pMeleeTask = pViewerPed->GetPedIntelligence()->GetTaskMelee(); 
		if( pMeleeTask )
			bInactiveObserver = pMeleeTask->GetQueueType() <= CTaskMelee::QT_InactiveObserver || pMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_ForceInactiveTauntMode );

		const bool bTestPassed = bInvertSense ? !bInactiveObserver : bInactiveObserver;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s an inactive observer\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_USING_RAGE_ABILITY == nSpecialTestId )
	{
		const CPlayerSpecialAbility* pAbility = pViewerPed->GetSpecialAbility();
		bool bUsingRageAbility = pAbility && pAbility->IsActive() && pAbility->GetType() == SAT_RAGE;
		const bool bTestPassed = bInvertSense ? !bUsingRageAbility : bUsingRageAbility;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s using the RAGE ability\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_ON_TOP_OF_CAR == nSpecialTestId )
	{
		CPhysical* pGroundPhysical = pViewerPed->GetGroundPhysical();
		if( !pGroundPhysical )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - No ground physical or is not of vehicle type\n", GetSpecialTestName() );
			return false;
		}
		
		bool bIsACar = pGroundPhysical->GetIsTypeVehicle() && static_cast<CVehicle*>( pGroundPhysical )->GetVehicleType() == VEHICLE_TYPE_CAR;
		const bool bTestPassed = bInvertSense ? !bIsACar : bIsACar;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ground physical %s a car\n", GetSpecialTestName(), bInvertSense ? "is not" : "is" );
			return false;
		}
	}
	else if( SPECIAL_COND_HAS_RAGE_ABILITY_EQUIPPED == nSpecialTestId )
	{
		const CPlayerSpecialAbility* pAbility = pViewerPed->GetSpecialAbility();
		bool bHasRageAbilityEquipped = pAbility && pAbility->GetType() == SAT_RAGE && pAbility->IsAbilityEnabled() && pAbility->IsAbilityUnlocked();
		const bool bTestPassed = bInvertSense ? !bHasRageAbilityEquipped : bHasRageAbilityEquipped;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s the RAGE ability equipped\n", GetSpecialTestName(), bInvertSense ? "has" : "does not have" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_WEAPON_UNARMED == nSpecialTestId )
	{
		const CWeaponInfo* pWeaponInfo = pViewerPed->GetWeaponManager() ? pViewerPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		bool bWeaponIsUnarmed = pWeaponInfo ? pWeaponInfo->GetIsUnarmed() : false;
		const bool bTestPassed = bInvertSense ? !bWeaponIsUnarmed : bWeaponIsUnarmed;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped weapon %s unarmed\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_WEAPON_VIOLENT == nSpecialTestId )
	{
		const CWeaponInfo* pWeaponInfo = pViewerPed->GetWeaponManager() ? pViewerPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		bool bWeaponIsViolent = pWeaponInfo ? !pWeaponInfo->GetIsNonViolent() : false;
		const bool bTestPassed = bInvertSense ? !bWeaponIsViolent : bWeaponIsViolent;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped weapon %s violent\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_WEAPON_HEAVY == nSpecialTestId )
	{
		const CWeaponInfo* pWeaponInfo = pViewerPed->GetWeaponManager()->GetEquippedWeaponInfo();
		bool bWeaponIsHeavy = pWeaponInfo ? pWeaponInfo->GetIsHeavy() : false;
		const bool bTestPassed = bInvertSense ? !bWeaponIsHeavy : bWeaponIsHeavy;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped weapon %s heavy\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_USING_RAGDOLL == nSpecialTestId )
	{
		Assertf( pViewerPed, "Invalid opponent ped." );
		bool bIsUsingRagdoll = pViewerPed->GetUsingRagdoll();
		const bool bTestPassed = bInvertSense ? !bIsUsingRagdoll : bIsUsingRagdoll;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s using ragdoll\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_CAN_USE_MELEE_RAGDOLL == nSpecialTestId )
	{
		Assertf( pViewerPed, "Invalid opponent ped." );
		bool bCanUseRagdoll = CTaskNMBehaviour::CanUseRagdoll( const_cast<CPed*>(pViewerPed), RAGDOLL_TRIGGER_MELEE );
		const bool bTestPassed = bInvertSense ? !bCanUseRagdoll : bCanUseRagdoll;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s use ragdoll\n", GetSpecialTestName(), bInvertSense ? "can" : "can not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_USING_NM == nSpecialTestId )
	{
		Assertf( pViewerPed->GetPedIntelligence(), "Invalid opponent ped intelligence." );
		bool bIsUsingNm = pViewerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_NM_CONTROL );
		const bool bTestPassed = bInvertSense ? !bIsUsingNm : bIsUsingNm;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s using natural motion\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_CROUCHING == nSpecialTestId )
	{
		bool bIsCrouching = pViewerPed->GetIsCrouching();
		const bool bTestPassed = bInvertSense ? !bIsCrouching : bIsCrouching;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s crouching\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_IS_AIMING == nSpecialTestId )
	{
		bool bIsAiming = pViewerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsAimingGun ) || pViewerPed->GetPedResetFlag( CPED_RESET_FLAG_IsAiming ) || ( pViewerPed->IsLocalPlayer() && CPlayerInfo::IsAiming() );
		const bool bTestPassed = bInvertSense ? !bIsAiming : bIsAiming;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s aiming\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_HAS_SUPPORT_ALLY_TO_RIGHT == nSpecialTestId ||
		     SPECIAL_COND_HAS_SUPPORT_ALLY_TO_LEFT == nSpecialTestId )
	{
		bool bHasSuitableSupportAlly = false;
		CTaskMelee*	pMeleeTask = pViewerPed->GetPedIntelligence()->GetTaskMelee(); 
		if( pMeleeTask && pTargetEntity )
		{
			CCombatMeleeGroup* pMeleeGroup = CCombatManager::GetMeleeCombatManager()->FindMeleeGroup( *pTargetEntity );
			if( pMeleeGroup )
			{
				if( SPECIAL_COND_HAS_SUPPORT_ALLY_TO_RIGHT == nSpecialTestId )
					bHasSuitableSupportAlly = pMeleeGroup->HasAllyInDirection( pViewerPed, CCombatMeleeGroup::AD_BACK_RIGHT );
				else
					bHasSuitableSupportAlly = pMeleeGroup->HasAllyInDirection( pViewerPed, CCombatMeleeGroup::AD_BACK_LEFT );
			}
		}

		const bool bTestPassed = bInvertSense ? !bHasSuitableSupportAlly : bHasSuitableSupportAlly;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s have a support ally to the %s\n", GetSpecialTestName(), bInvertSense ? "does" : "does not", SPECIAL_COND_HAS_SUPPORT_ALLY_TO_RIGHT == nSpecialTestId ? "right" : "left" );
			return false;
		}
	}
	else if (SPECIAL_COND_IS_UNDERWATER == nSpecialTestId)
	{
		bool bTestPassed = true;
		if( !pViewerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) && !pViewerPed->GetIsFPSSwimming() )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Failed swim tasks not running\n", GetSpecialTestName() );
			bTestPassed = false;
		}

		Assertf( pViewerPed->GetPedIntelligence(), "Invalid ped intelligence." );
		CTaskMotionDiving* pDiveTask = static_cast<CTaskMotionDiving*>( pViewerPed->GetPedIntelligence()->FindTaskActiveMotionByType( CTaskTypes::TASK_MOTION_DIVING ) );
		
		bool bFPSDiveStrafing = false;
#if FPS_MODE_SUPPORTED
		if (pViewerPed->GetIsFPSSwimming())
		{
			CTaskMotionBase* pPrimaryTask = pViewerPed->GetPrimaryMotionTask();
			if( pPrimaryTask && pPrimaryTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED )
			{
				CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pPrimaryTask);
				if (pTask)
				{
					bFPSDiveStrafing = pTask->CheckForDiving();
				}
			}
		}
#endif	//FPS_MODE_SUPPORTED

		if( !bFPSDiveStrafing && (!pDiveTask || pDiveTask->IsDivingDown()) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Not underwater\n", GetSpecialTestName() );
			bTestPassed = false;
		}
		bTestPassed = bInvertSense ? !bTestPassed : bTestPassed;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Ped %s underwater\n", GetSpecialTestName(), bInvertSense ? "is" : "is not" );
			return false;
		}
	}
	else if( SPECIAL_COND_LAST_ACTION_WAS_A_RECOIL == nSpecialTestId )
	{
		CTaskMelee*	pMeleeTask = pViewerPed->GetPedIntelligence() ? pViewerPed->GetPedIntelligence()->GetTaskMelee() : NULL; 
		bool bTestPassed = pMeleeTask ? pMeleeTask->WasLastActionARecoil() : false;
		bTestPassed = bInvertSense ? !bTestPassed : bTestPassed;
		if( !bTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Last action %s a recoil\n", GetSpecialTestName(), bInvertSense ? "was" : "was not" );
			return false;
		}
	}
#if CNC_MODE_ENABLED
	else if (SPECIAL_COND_IS_ARCADE_TEAM_CNC_COP == nSpecialTestId)
	{
		bool bIsPedACncCop = NetworkInterface::IsInCopsAndCrooks()
			&& pViewerPed->IsAPlayerPed()
			&& pViewerPed->GetPlayerInfo()
			&& pViewerPed->GetPlayerInfo()->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP;
		bool bTestPassed = bInvertSense ? !bIsPedACncCop : bIsPedACncCop;
		if (!bTestPassed)
		{
			actionPrintf(bDisplayDebug, "%s (FAILED) - Ped %s on the CNC Cop Arcade team\n", GetSpecialTestName(), bInvertSense ? "is" : "is not");
			return false;
		}
	}
#endif
	else if( nSpecialTestId != 0 )
	{
		Assertf( false, "Unknown special test type - %s.", GetSpecialTestName() );
		actionPrintf( bDisplayDebug, "%s (FAILED) - Unknown special test (id=%d)\n", GetSpecialTestName(), nSpecialTestId );
		return false;
	}

	// Everything seems to have gone fine...
	actionPrintf( bDisplayDebug, "%s (SUCESS)\n", GetSpecialTestName() );
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// CActionDefinition
//
// Actions are possible things to do.  When requested they are tested to see
// if invoking them is appropriate.  If so they are invoked.
// 
// Usually the action manager will get a generic request for an action from
// idle.  In this case usually multiple different actions will be able to be
// invoked.  In this case priority values and bias values are used to select
// only one of them to be invoked.
//
// This is a container that holds the tests needed for a a actionResult to be
// suitable, which actionResult to trigger, what else to do when it is
// triggered, and information about when to choose this action over other
// valid actions.
/////////////////////////////////////////////////////////////////////////////////
CActionDefinition::CActionDefinition()
{
	Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CActionDefinition::PostLoad(void)
{
	u32 nIndexFound = 0;
	if( GetImpulseTestID() != 0 )
	{
		const CImpulseTest* pImpulseTest = ACTIONMGR.FindImpulseTest( nIndexFound, GetImpulseTestID() );
		SetImpulseTest( const_cast<CImpulseTest*>(pImpulseTest) );
		Assertf( m_pImpulseTest, "ImpulseTest (%s) was not found", GetImpulseTestName() );
	}

	if( GetInterrelationTestID() != 0 )
	{
		const CInterrelationTest* pInterrelationTest = ACTIONMGR.FindInterrelationTest( nIndexFound, GetInterrelationTestID() );
		SetInterrelationTest( const_cast<CInterrelationTest*>(pInterrelationTest) );
		Assertf( m_pInterrelationTest, "InterrelationTest (%s) was not found", GetInterrelationTestName() );
	}

	m_CondPedOther.PostLoad();
	for(int i = 0; i < m_aCondSpecials.GetCount(); i++)
		m_aCondSpecials[i]->PostLoad();

	for(int i = 0; i < m_aWeaponActionResults.GetCount(); i++)
		m_aWeaponActionResults[i]->PostLoad();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Reset
// PURPOSE	:	Simply initializes all variables back to default value
/////////////////////////////////////////////////////////////////////////////////
void CActionDefinition::Reset(void)
{
	m_pImpulseTest = NULL;
	m_pInterrelationTest = NULL;			
	m_Name.Clear();
	m_Priority = 0;
	m_MaxImpulseTestDelay = 0;
	m_SelectionRouletteBias = 0;
	m_ActionType.Reset();
	m_SelfFilter.Reset();
	m_TargetFilter.Reset();
	m_MovementSpeed.Reset();
	m_DesiredDirection.Reset();
	m_DefinitionAttrs.Reset();
	m_CondPedOther.Reset();
	m_ImpulseTest.Clear();
	m_InterrelationTest.Clear();

	for(int i = 0; i < m_aCondSpecials.GetCount(); i++)
		m_aCondSpecials[i]->Reset();
	
	for(int i = 0; i < m_aWeaponActionResults.GetCount(); i++)
		m_aWeaponActionResults[i]->Reset();

	m_aBrawlingStyles.Reset();
	m_Debug = false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clear
// PURPOSE	:	Deletes what was previously allocated
/////////////////////////////////////////////////////////////////////////////////
void CActionDefinition::Clear(void)
{
	m_CondPedOther.Clear();
	for(int i = 0; i < m_aCondSpecials.GetCount(); i++)
	{
		m_aCondSpecials[i]->Clear();
		delete m_aCondSpecials[i];
	}

	for(int i = 0; i < m_aWeaponActionResults.GetCount(); i++)
	{
		m_aWeaponActionResults[i]->Clear();
		delete m_aWeaponActionResults[i];
	}
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionDefinition::PrintMemoryUsage( size_t& rTotalSize )
{
	size_t mySize = sizeof( *this );
	rTotalSize += mySize;
	for( int i = 0; i < m_aCondSpecials.GetCount(); i++ )
		m_aCondSpecials[i]->PrintMemoryUsage( rTotalSize );
	
	for( int i = 0; i < m_aWeaponActionResults.GetCount(); i++ )
		m_aWeaponActionResults[i]->PrintMemoryUsage( rTotalSize );
	
	Printf( "CActionDefinition: %s - size:%" SIZETFMT "d total:%" SIZETFMT "d\n", m_Name.GetCStr(), mySize, rTotalSize );	
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetSpecialCondition
// PURPOSE	:	returns a CActionCondSpecial given an index
// PARAMS	:	nIdx - Index we would like to reference
// RETURNS	:	CActionCondSpecial associated with the designated index
/////////////////////////////////////////////////////////////////////////////////
const CActionCondSpecial* CActionDefinition::GetSpecialCondition( const u32 nIdx ) const
{
	Assertf( nIdx < GetNumSpecialConditions(), "CActionDefinition::GetSpecialCondition - nIdx (%d) is out of range (%d).", nIdx, GetNumSpecialConditions() );
	return m_aCondSpecials[ nIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResult
// PURPOSE	:	Checks the given Peds' weapon type against the available weapon
//				actions. 
// PARAMS	:	pPed - Ped we would like to test against
// RETURNS	:	CActionResult associated with the ped and their currently equipped
//				weapon.
/////////////////////////////////////////////////////////////////////////////////
const CActionResult* CActionDefinition::GetActionResult( const CPed* pPed ) const
{
	for( u32 i = 0; i < m_aWeaponActionResults.GetCount(); i++ )
	{
		if( CActionManager::ShouldAllowWeaponType( pPed, m_aWeaponActionResults[i]->GetWeaponType() ) )
			return m_aWeaponActionResults[i]->GetActionResult();
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetWeaponActionResult
// PURPOSE	:	returns a CWeaponActionResult given an index
// PARAMS	:	nIdx - Index we would like to reference
// RETURNS	:	CWeaponActionResult associated with the designated index
/////////////////////////////////////////////////////////////////////////////////
const CWeaponActionResult* CActionDefinition::GetWeaponActionResult( const u32 nIdx ) const
{
	Assertf(nIdx < GetNumWeaponActionResults(), "CActionDefinition::GetWeaponActionResult - nIdx (%d) is out of range (%d).", nIdx, GetNumWeaponActionResults() );
	return m_aWeaponActionResults[ nIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBrawlingStyleName
// PURPOSE	:	Returns the brawling style cstr given an array index
// PARAMS	:	nIdx - Direct index we will dereference
// RETURNS	:	The index associated atStringHash
/////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char*	CActionDefinition::GetBrawlingStyleName( const u32 nIdx ) const
{
	Assertf( nIdx < m_aBrawlingStyles.GetCount(), "CActionCondPedOther::GetBrawlingStyleName - nIdx (%d) is out of range (%d).", nIdx, m_aBrawlingStyles.GetCount() );
	return m_aBrawlingStyles[nIdx].GetCStr();
}
#endif	//	!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetBrawlingStyleID
// PURPOSE	:	Returns the brawling style hash id given an array index
// PARAMS	:	nIdx - Direct index we will dereference
// RETURNS	:	The index associated atStringHash
/////////////////////////////////////////////////////////////////////////////////
u32 CActionDefinition::GetBrawlingStyleID( const u32 nIdx ) const
{
	Assertf( nIdx < m_aBrawlingStyles.GetCount(), "CActionCondPedOther::GetBrawlingStyleID - nIdx (%d) is out of range (%d).", nIdx, m_aBrawlingStyles.GetCount() );
	return m_aBrawlingStyles[nIdx].GetHash();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAllowActionType
// PURPOSE	:	Test to determine if the designated flags are allowed for this 
//				CActionDefinition.
// PARAMS	:	actionTypeBitSet - Flags we would like to check against
// RETURNS	:	Whether or not this CActionDefinition is suitable given the flags
/////////////////////////////////////////////////////////////////////////////////
bool CActionDefinition::ShouldAllowActionType( const CActionFlags::ActionTypeBitSetData& typeToLookFor ) const
{
	if( typeToLookFor.IsSet( CActionFlags::AT_ANY ) )
		return true;

	for( u32 i = 0; i < CActionFlags::ActionType_NUM_ENUMS; i++ )
	{
		if( m_ActionType.IsSet( i ) && typeToLookFor.IsSet( i ) )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAllowBrawlingStyle
// PURPOSE	:	Test to determine if the designated brawling style hash matches any of the allowed cases
// PARAMS	:	nBrawlingStyleHash - atStringHash of the brawling style we are asking about
// RETURNS	:	Whether or not this CActionDefinition is suitable given the brawling style hash
/////////////////////////////////////////////////////////////////////////////////
bool CActionDefinition::ShouldAllowBrawlingStyle(u32 nBrawlingStyleHash) const
{
	u32 nNumBrawlingStyles = m_aBrawlingStyles.GetCount();
	if( nNumBrawlingStyles == 0 )
		return true;

	for( u32 i = 0; i < nNumBrawlingStyles; i++ )
	{
		if( nBrawlingStyleHash == m_aBrawlingStyles[i].GetHash() )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsSuitable
// PURPOSE	:	Check if this action is suitable for the given ped under the given conditions.
//				This performs each of the suitability tests in order, if any fail then the
//				then the whole action fails it suitability test.
// PARAMS	:	
// RETURNS	:	Whether or not the test is suitable
/////////////////////////////////////////////////////////////////////////////////
bool CActionDefinition::IsSuitable(	const CActionFlags::ActionTypeBitSetData& typeToLookFor,
								    const CImpulseCombo* pImpulseCombo,
									const bool			bUseAvailabilityFromIdleTest,
								    const bool			bDesiredAvailabilityFromIdle,
								    const CPed*			pActingPed,
								    const u32			nForcingResultId,
								    const s32			nPriority,
								    const CEntity*		pTargetEntity,
								    const bool			bHasLockOnTargetEntity,
								    const bool			bImpulseInitiatedMovesOnly,
								    const bool			bUseImpulseTest,// For AI
								    const CControl*		pController,
								    CActionTestCache*	pReusableResults,
								    const bool			bDisplayDebug ) const
{
	Assert( pActingPed );

	// Check to see if outside system disabled this action
	if( !GetIsEnabled() )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - Action disabled\n", GetName() );
		return false;
	}

	// Check the brawling style
	u32 nPedBrawlingStyleId = pActingPed->GetBrawlingStyle().GetHash();
	if( !ShouldAllowBrawlingStyle( nPedBrawlingStyleId ) )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - nBrawlingStyleId(%d) not allowed\n", GetName(), nPedBrawlingStyleId );
		return false;
	}

	// Check if the type test is necessary.
	if( !ShouldAllowActionType( typeToLookFor ) )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - !ShouldAllowActionType( typeToLookFor )\n", GetName() );
		return false;
	}

	// Check if the entry test is necessary.
	if( bUseAvailabilityFromIdleTest && ( pActingPed->IsPlayer() ? GetIsPlayerEntryAction() != bDesiredAvailabilityFromIdle : GetIsAiEntryAction() != bDesiredAvailabilityFromIdle ) )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - bUseAvailabilityFromIdleTest && ( GetIsEntryAction() != bDesiredAvailabilityFromIdle)\n", GetName() );
		return false;
	}

	// Check if the clip set of the move is one of the ones this ped is
	// allowed to use and also if it is currently loaded.
	bool bSetIsAllowed = IsActionsClipSetAllowedForPed( pActingPed );
	if( !bSetIsAllowed )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - !bSetIsAllowed\n", GetName() );
		return false;
	}

	// Check the ped self test (introspection).
	const bool bPedSelfTestPassed = CActionManager::PlayerAiFilterTypeDoesCatchPed( GetSelfFilter(), pActingPed );
	if( !bPedSelfTestPassed )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - !bPedSelfTestPassed\n", GetName() );
		return false;
	}

	// Do not pick ragdoll results for clones.
	if(PreferRagdollResult() && (pActingPed->IsNetworkClone() || (NetworkInterface::IsGameInProgress() && pActingPed->IsLocalPlayer())) && !pActingPed->GetUsingRagdoll())
	{
		//bool bIsUsingNm = pActingPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_NM_CONTROL );
		if(/*!bIsUsingNm && */!pActingPed->GetIsDeadOrDying())
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !PreferRagdollResult Clone or Local Player in MP Test\n", GetName() );
			return false;
		}
	}

	// Check if we should only allow this move if it is normally impulse initiated.
	if( bImpulseInitiatedMovesOnly && !m_pImpulseTest )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - bImpulseInitiatedMovesOnly && !m_pImpulseTest\n", GetName() );
		return false;
	}

	// If in water, don't allow if definition says so (unless specifically searching for swimming types).
	if((pActingPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) || pActingPed->GetIsFPSSwimming()) && !AllowInWater() && !typeToLookFor.IsSet( CActionFlags::AT_SWIMMING ) )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - CPED_CONFIG_FLAG_SwimmingTasksRunning/FPSSwimming && !AllowInWater\n", GetName() );
		return false;
	}

	const CPed* pTargetPed = pTargetEntity && pTargetEntity->GetIsTypePed() ? static_cast<const CPed*>( pTargetEntity ) : nullptr;

#if FPS_MODE_SUPPORTED
	const bool bIsFPSMode = pActingPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	// If in first person, don't allow if definition says so.
	if( bIsFPSMode && IsDisabledInFirstPerson() )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - pActingPed->IsFirstPersonShooterModeEnabledForPlayer(false) && IsDisabledInFirstPerson()\n", GetName() );
		return false;
	}

	// Don't allow if target ped is in 1st person.
	if( pTargetPed && pTargetPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && 
		IsDisabledIfTargetInFirstPerson() )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - pTargetEntity->IsFirstPersonShooterModeEnabledForPlayer(false, true) && IsDisabledIfTargetInFirstPerson()\n", GetName() );
		return false;
	}
#endif

	// Check the impulse test (input).
	if( bUseImpulseTest )
	{
		// Check against the allowed action duration
		if( pImpulseCombo )
		{
			u32 nImpulseDelay = GetMaxImpulseDelay();
			if( nImpulseDelay != 0 && pImpulseCombo->GetLatestRecordedImpulseTime() + GetMaxImpulseDelay() < fwTimer::GetTimeInMilliseconds() )
			{
				actionPrintf( bDisplayDebug, "%s (FAILED) - Last impulse too long ago [%d < %d]\n", GetName(), pImpulseCombo->GetLatestRecordedImpulseTime() + GetMaxImpulseDelay(), fwTimer::GetTimeInMilliseconds() );
				return false;
			}
		}

		const bool bImpulseTestsPassed = !m_pImpulseTest || m_pImpulseTest->DoTest( pController, pImpulseCombo, pReusableResults, bDisplayDebug );
		if( !bImpulseTestsPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !bImpulseTestsPassed\n", GetName() );
			return false;
		}
	}

	actionAssertf( !( GetRequireTarget() && GetRequireNoTarget() ), "DA_REQUIRE_TARGET and DA_REQUIRE_NO_TARGET flags are both set for %s. Please fix in definitions.meta\n", GetName()	);

	// Determine if a target is required, if so test that one exists.
	if( GetRequireTarget() && !pTargetEntity )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - GetRequireTarget() && !pTargetEntity\n", GetName() );
		return false;
	}
	else if( GetRequireNoTarget() && pTargetEntity )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - GetRequireNoTarget() && pTargetEntity\n", GetName() );
		return false;
	}

	actionAssertf( !( GetRequireTargetLockOn() && GetRequireNoTargetLockOn() ), "DA_REQUIRE_TARGET_LOCK_ON and DA_REQUIRE_NO_TARGET_LOCK_ON flags are both set for %s. Please fix in definitions.meta\n", GetName()	);

	bool bAllowStealthAttack = pTargetPed && pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_PreventAllStealthKills);

	// Determine if a lock on target is required, if so test that one exists.
	if( GetRequireTargetLockOn() && !bHasLockOnTargetEntity )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - bRequireLockOn && !bHasLockOnTargetEntity\n", GetName() );
		return false;
	}
	// Determine if we require no lock on but have one
	else if( GetRequireNoTargetLockOn() && bHasLockOnTargetEntity && !bAllowStealthAttack)
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - GetRequireNoTargetLockOn() && bHasLockOnTargetEntity\n", GetName() );
		return false;
	}

	const CActionResult* pActionResult = GetActionResult( pActingPed );
	if( pActionResult && pActionResult->GetPriority() < nPriority )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - Lower priority (current=%d selected=%d)\n", GetName(), nPriority, pActionResult->GetPriority() );
		return false;
	}


#if FPS_MODE_SUPPORTED
	if( bIsFPSMode && m_ActionType.IsSet( CActionFlags::AT_HIT_REACTION ) && !m_MovementSpeed.IsSet( CActionFlags::MS_ANY ) )
	{
		// The moving hit reactions don't turn the ped to face the attacker. Since we really need to face the attacker let's try to use the still hit reactions in first-person
		float fMoveBlendRatioSq = pActingPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
		if( fMoveBlendRatioSq < rage::square( MBR_SPRINT_BOUNDARY ) && !m_MovementSpeed.IsSet( CActionFlags::MS_STILL ) && 
			( m_MovementSpeed.IsSet( CActionFlags::MS_WALKING ) || m_MovementSpeed.IsSet( CActionFlags::MS_RUNNING ) ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Only use exclusive walk/run hit reactions when sprinting in first person\n", GetName() );
			return false;
		}
		
		if( fMoveBlendRatioSq >= rage::square( MBR_SPRINT_BOUNDARY ) && m_MovementSpeed.IsSet( CActionFlags::MS_STILL ) &&
			!m_MovementSpeed.IsSet( CActionFlags::MS_WALKING ) && !m_MovementSpeed.IsSet( CActionFlags::MS_RUNNING ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - Only use exclusive still hit reactions when not sprinting in first person\n", GetName() );
			return false;
		}
	}
	else
#endif
	{
		// Check against movement speed
		if( !CActionManager::WithinMovementSpeed( pActingPed, m_MovementSpeed ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !CActionManager::WithinMovementSpeed()\n", GetName() );
			return false;
		}
	}

	// Determine if the remaining tests are necessary.  If not the just continue
	// without testing the "pedOther" and "interrelation" tests, as they pass implicitly.
	if( pTargetEntity )
	{
		if( !CActionManager::TargetEntityTypeDoesCatchEntity( GetTargetEntity(), pTargetEntity ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !CActionManager::TargetEntityTypeDoesCatchEntity(m_TargetEntity, pTargetEntity)\n", GetName() );
			return false;
		}

		// Determine if it passes the interrelation test.
		const bool bInterrelationTestPassed = !m_pInterrelationTest || m_pInterrelationTest->DoTest( pActingPed, pTargetEntity, pReusableResults, bDisplayDebug );
		if( !bInterrelationTestPassed )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !bInterrelationTestPassed\n", GetName() );
			return false;
		}

		// Determine if it passes the pedOther Test.
		if( pTargetPed )
		{
			if( !m_CondPedOther.DoTest( pActingPed, pTargetPed, pActionResult, nForcingResultId, pReusableResults, bDisplayDebug ) )
			{
				actionPrintf( bDisplayDebug, "%s (FAILED) - !m_CondPedOther\n", GetName() );
				return false;
			}
		}
	}
	else
	{
		// Check against desired direction
		if( !CActionManager::WithinDesiredDirection( pActingPed, m_DesiredDirection ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !CActionManager::WithinDesiredDirection()\n", GetName() );
			return false;
		}
	}

	// Iterate through special tests 
	for( int i = 0; i < m_aCondSpecials.GetCount(); i++ )
	{
		if( !CActionManager::PlayerAiFilterTypeDoesCatchPed( m_aCondSpecials[i]->GetFilter(), pActingPed ) )
			continue;

		// Determine if we passed any special tests.
		if( !m_aCondSpecials[i]->DoTest( pActingPed, pTargetEntity, pActionResult, pReusableResults, bDisplayDebug ) )
		{
			actionPrintf( bDisplayDebug, "%s (FAILED) - !m_aCondSpecials [%s][%d]\n", GetName(), m_aCondSpecials[i]->GetSpecialTestName(), i );
			return false;
		}	
	}

	// Determine if we have a suitable action result given the acting ped's currently equipped weapon
	if( GetNumWeaponActionResults() > 0 && !pActionResult )
	{
		actionPrintf( bDisplayDebug, "%s (FAILED) - GetActionResult( pActingPed )\n", GetName() );
		return false;
	}

	DEV_BREAK_IF_FOCUS( GetIsDebug() && CCombatMeleeDebug::sm_bFocusPedBreakOnSelectedMeleeAction, pActingPed );
	actionPrintf( bDisplayDebug, "SUCCESS (%s)\n", GetName() );

	// All tests have passed so it must be suitable.
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsActionsClipSetAllowedForPed
// PURPOSE	:	Determines whether or not the CActionDefinition clip set is ready
//				and allowable for the designated ped
// PARAMS	:	pActingPed - Ped we would like to test against
// RETURNS	:	Whether or not the CActionoDefinition clip set is suitable
/////////////////////////////////////////////////////////////////////////////////
bool CActionDefinition::IsActionsClipSetAllowedForPed( const CPed* pActingPed ) const
{
	// Check if this is an action test with no result.
	const CActionResult* pActionResult = GetActionResult( pActingPed );
	if( pActionResult == NULL )
	{
		// There is no action result associated with the action, so allow it.
		return true;
	}

	const fwMvClipSetId actionResultClipSetId = (fwMvClipSetId)pActionResult->GetClipSetID();

	// Check if this action result had any clip set supplied, if not then
	// it is implicitly allowed.  This lets us have results that play no animations.
	if( actionResultClipSetId == CLIP_SET_ID_INVALID )
	{
		return true;
	}

	// Determine if the group is one of the ones this ped is allowed to use.
	bool bIsValidSetForThisPed = true;
	if( m_ActionType.IsSet( CActionFlags::AT_HIT_REACTION ) )
	{
		// Just assume that any reaction is that is being invoked comes from the same
		// clip set as the res causing it (and thus is already loaded).
		bIsValidSetForThisPed = true;
	}

	// See if we should bother to check if the clip set is loaded.
	// Note that as soon as a ped goes into melee combat they will send requests
	// for all of the melee attack dictionaries they have and also for any
	// weapon they have.
	if( !bIsValidSetForThisPed )
	{
		return false;
	}

	// Check if the set is loaded.
	// Get the clip sets corresponding streaming block of anims and check if the block is loaded.
	strLocalIndex nAnimBlockIndex = strLocalIndex(-1); 	
	if( actionResultClipSetId != CLIP_SET_ID_INVALID )
	{
		nAnimBlockIndex = fwClipSetManager::GetClipDictionaryIndex(actionResultClipSetId);
	}

	if( nAnimBlockIndex == -1 || !CStreaming::HasObjectLoaded(nAnimBlockIndex, fwAnimManager::GetStreamingModuleId() ) )
	{
		return false;
	}

	// It seems to be a valid and loaded set.
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CActionInfoDataFileMounter
//
// Inherits from CDataFileMountInterface 
// Overrides CActionManager's data using specified files.
////////////////////////////////////////////////////////////////////////////////

class CActionInfoDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CActionInfoDataFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		// Override using the specified file.
		ACTIONMGR.OverrideActionDefinitions(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & OUTPUT_ONLY(file)) 
	{
		dlcDebugf3("CActionInfoDataFileMounter::UnloadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		// Load the original file.
		ACTIONMGR.RevertActionDefinitions();
	}

} g_ActionInfoDataFileMounter;


/////////////////////////////////////////////////////////////////////////////////
// CActionManager
//
// This is the main store for all possible actions that a ped might take.
// It stores actions that can be accessed from idle, combat idle, and from
// other actions.
//
// It also handles loading of this data from a CSV file in such a way that
// it can be dynamically reloaded at run time (for easier tuning).
/////////////////////////////////////////////////////////////////////////////////

// The version number of the action_table must match this number
dev_float CActionManager::ms_fMBRWalkBoundary = 0.75f;
dev_float CActionManager::ms_fLowSurfaceRootOffsetZ = -0.2f;
dev_float CActionManager::ms_fHighSurfaceRootOffsetZ = 0.5f;
atQueue<u32, ACTION_HISTORY_LENGTH> CActionManager::ms_qMeleeActionHistory;
atFixedArray<sSuitableActionInfo, MAX_SUITABLE_DEFINITIONS>  CActionManager::ms_aSuitableActionInfo;

#if __BANK
bool CActionManager::ms_DebugFocusPed = false;
#endif

CActionManager::CActionManager() 
 : m_bLoadActionTableDataOnInit(true) 
{
}

CActionManager::~CActionManager() 
{
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	OverrideActionDefinitions
// PURPOSE	:	Overrides action definitions with data from a specified file.
//				Used by CDataFileMountInterface.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::OverrideActionDefinitions(const char* fileName)
{
	// Make sure the actions are totally clear.
	const int nActionDefCount = m_aActionDefinitions.GetCount();
	for(int i = 0; i < nActionDefCount; ++i)
		m_aActionDefinitions[i].Clear();
	m_aActionDefinitions.Reset();

	// Allocate memory
	const int nMaxDefinitions = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Definitions", 0x23b59b40 ), CONFIGURED_FROM_FILE );
	m_aActionDefinitions.Reserve( nMaxDefinitions );

	// TODO: Templatize this once you get a chance
	parTree* pTree = NULL;
	parTreeNode* pRootNode = NULL;

	// CActionDefition parsing
	pTree = PARSER.LoadTree( fileName, "meta" );
	if( pTree )
	{
		pRootNode = pTree->GetRoot();
		pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
		if( pRootNode )
		{
			parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
			for(; itr != pRootNode->EndChildren(); ++itr )
			{
				Assertf( ACTIONMGR.m_aActionDefinitions.GetCount() < ACTIONMGR.m_aActionDefinitions.GetCapacity(), "CActionManager: Run out of ActionTable_Definitions (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionDefinitions.GetCount() );
				CActionDefinition& rActionDefinition = ACTIONMGR.m_aActionDefinitions.Append();
				PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rActionDefinition ), rActionDefinition.parser_GetPointer(), false );
			}
		}

		delete pTree;
	}

	// Post load fix-up
	const int nActionDefinitionCount = m_aActionDefinitions.GetCount();
	for(int i = 0; i < nActionDefinitionCount; ++i)
	{
		m_aActionDefinitions[i].PostLoad();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RevertActionDefinitions
// PURPOSE	:	Reverts action definitions back to their original values.
//				Used by CDataFileMountInterface. 
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::RevertActionDefinitions()
{
	OverrideActionDefinitions("common:/data/action/definitions.meta");
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Init
// PURPOSE	:	Our initialization function (not in the constructor so that we have finer control
//				of memory allocation and file access).
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::Init( unsigned )
{
	INIT_ACTIONMGR;

	// Only load data on initial load or if otherwise inidicated. B*1582386. This avoids re-initialising
	// unnecessarily. 
	if(CActionManagerSingleton::GetInstance().m_bLoadActionTableDataOnInit)
	{
		CDataFileMount::RegisterMountInterface(CDataFileMgr::ACTION_TABLE_DEFINITIONS, &g_ActionInfoDataFileMounter, eDFMI_UnloadFirst);

		ACTIONMGR.LoadOrReloadActionTableData();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Shutdown
// PURPOSE	:	Our shutdown function that allows us to release all the previously
//				allocated from Init. Gives us teh ability to reload the CActionManager
//				on the fly.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::Shutdown( unsigned )
{
	ACTIONMGR.ClearActionTableData();
	SHUTDOWN_ACTIONMGR;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PostLoad
// PURPOSE	:	Called after ActionManager has been loaded from disk. Used to 
//				fix-up objects to avoid runtime costs.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::PostLoad(void)
{
	s32 i = 0;
	const int nImpulseTestCount = m_aImpulseTests.GetCount();
	for(i = 0; i < nImpulseTestCount; ++i)
	{
		m_aImpulseTests[i].PostLoad();
	}

	const int stealthKillTestCount = m_aStealthKillTests.GetCount();
	for(i = 0; i < stealthKillTestCount; ++i)
	{
		m_aStealthKillTests[i].PostLoad();
	}

	const int nInterrelationTestCount = m_aInterrelationTests.GetCount();
	for(i = 0; i < nInterrelationTestCount; ++i)
	{
		m_aInterrelationTests[i].PostLoad();
	}

	const int homingsCount = m_aHomings.GetCount();
	for(i = 0; i < homingsCount; ++i)
	{
		m_aHomings[i].PostLoad();
	}

	const int nDamageAndReactionCount = m_aDamageAndReactions.GetCount();
	for(i = 0; i < nDamageAndReactionCount; ++i)
	{
		m_aDamageAndReactions[i].PostLoad();
	}

	const int nStrikeBoneSetCount = m_aStrikeBoneSets.GetCount();
	for(i = 0; i < nStrikeBoneSetCount; ++i)
	{
		m_aStrikeBoneSets[i].PostLoad();
	}

	const int nRumbleCount = m_aActionRumbles.GetCount();
	for(i = 0; i < nRumbleCount; ++i)
	{
		m_aActionRumbles[i].PostLoad();
	}

	const int nVfxCount = m_aActionVfx.GetCount();
	for(i = 0; i < nVfxCount; ++i)
	{
		m_aActionVfx[i].PostLoad();
	}

	const int nFacialAnimSetCount = m_aActionFacialAnimSets.GetCount();
	for(i = 0; i < nFacialAnimSetCount; ++i)
	{
		m_aActionFacialAnimSets[i].PostLoad();
	}

	const int nActionBranchSetCount = m_aActionBranchSets.GetCount();
	for(i = 0; i < nActionBranchSetCount; ++i)
	{
		m_aActionBranchSets[i].PostLoad();	
	}

	const int nActionResultCount = m_aActionResults.GetCount();
	for(i = 0; i < nActionResultCount; ++i)
	{
		m_aActionResults[i].PostLoad();	
	}

	const int nActionDefinitionCount = m_aActionDefinitions.GetCount();
	for(i = 0; i < nActionDefinitionCount; ++i)
	{
		m_aActionDefinitions[i].PostLoad();
	}

#if __DEV
	Assert(IsValid());
#endif // __DEV

}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrintMemoryUsage
// PURPOSE	:	Walk through and calculate the total amount of memory used
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::PrintMemoryUsage( void )
{
	Printf( "===========ACTION MEMORY===========\n");
	size_t nTotalMemory = 0; 
	s32 i = 0;
	const int nImpulseTestCount = m_aImpulseTests.GetCount();
	for(i = 0; i < nImpulseTestCount; ++i)
	{
		m_aImpulseTests[i].PrintMemoryUsage( nTotalMemory );
	}

	const int stealthKillTestCount = m_aStealthKillTests.GetCount();
	for(i = 0; i < stealthKillTestCount; ++i)
	{
		m_aStealthKillTests[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nInterrelationTestCount = m_aInterrelationTests.GetCount();
	for(i = 0; i < nInterrelationTestCount; ++i)
	{
		m_aInterrelationTests[i].PrintMemoryUsage( nTotalMemory );
	}

	const int homingsCount = m_aHomings.GetCount();
	for(i = 0; i < homingsCount; ++i)
	{
		m_aHomings[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nDamageAndReactionCount = m_aDamageAndReactions.GetCount();
	for(i = 0; i < nDamageAndReactionCount; ++i)
	{
		m_aDamageAndReactions[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nStrikeBoneSetCount = m_aStrikeBoneSets.GetCount();
	for(i = 0; i < nStrikeBoneSetCount; ++i)
	{
		m_aStrikeBoneSets[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nRumbleCount = m_aActionRumbles.GetCount();
	for(i = 0; i < nRumbleCount; ++i)
	{
		m_aActionRumbles[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nVfxCount = m_aActionVfx.GetCount();
	for(i = 0; i < nVfxCount; ++i)
	{
		m_aActionVfx[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nFacialAnimSetCount = m_aActionFacialAnimSets.GetCount();
	for(i = 0; i < nFacialAnimSetCount; ++i)
	{
		m_aActionFacialAnimSets[i].PrintMemoryUsage( nTotalMemory );
	}

	const int nActionBranchSetCount = m_aActionBranchSets.GetCount();
	for(i = 0; i < nActionBranchSetCount; ++i)
	{
		m_aActionBranchSets[i].PrintMemoryUsage( nTotalMemory );	
	}

	const int nActionResultCount = m_aActionResults.GetCount();
	for(i = 0; i < nActionResultCount; ++i)
	{
		m_aActionResults[i].PrintMemoryUsage( nTotalMemory );	
	}

	const int nActionDefinitionCount = m_aActionDefinitions.GetCount();
	for(i = 0; i < nActionDefinitionCount; ++i)
	{
		m_aActionDefinitions[i].PrintMemoryUsage( nTotalMemory );
	}
	Printf( "TOTAL MEMORY = %" SIZETFMT "d\n", nTotalMemory );
	Printf( "================END================\n\n");
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	LoadOrReloadActionTableData
// PURPOSE	:	Load all action_table data files, including those from the current episodic pack (if any)
//				It has multiple data checks to assure that the loaded data is as expected.
//				This is designed to handle dynamic reloading of data at run time (for easier tuning).
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::LoadOrReloadActionTableData( void )
{
	// Delete all previously allocated memory
	ClearActionTableData();

	const int nMaxDefinitions = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Definitions", 0x23b59b40 ), CONFIGURED_FROM_FILE );
	m_aActionDefinitions.Reserve( nMaxDefinitions );

	const int nMaxResults = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Results", 0x489e6ac0 ), CONFIGURED_FROM_FILE );
	m_aActionResults.Reserve( nMaxResults );

	const int nMaxImpulses = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Impulses", 0x7cd838ac ), CONFIGURED_FROM_FILE );
	m_aImpulseTests.Reserve( nMaxImpulses );

	const int nMaxInterrelationals = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Interrelations", 0x1a77b5af ), CONFIGURED_FROM_FILE );
	m_aInterrelationTests.Reserve( nMaxInterrelationals );

	const int nMaxHomings = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Homings", 0x69aaa6f7 ), CONFIGURED_FROM_FILE );
	m_aHomings.Reserve( nMaxHomings );

	const int nMaxDamages = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Damages", 0x940e3abf ), CONFIGURED_FROM_FILE );
	m_aDamageAndReactions.Reserve( nMaxDamages );

	const int nMaxStrikeBones = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_StrikeBones", 0x7cb690f5 ), CONFIGURED_FROM_FILE );
	m_aStrikeBoneSets.Reserve( nMaxStrikeBones );

	const int nMaxActionRumbles = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Rumbles", 0xf9e1eccc ), CONFIGURED_FROM_FILE );
	m_aActionRumbles.Reserve( nMaxActionRumbles );

	const int nMaxVfx = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Vfx", 0xb8ede0cc ), CONFIGURED_FROM_FILE );
	m_aActionVfx.Reserve( nMaxVfx );

	const int nMaxFacialAnimSets = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_FacialAnimSets", 0xd855984a ), CONFIGURED_FROM_FILE );
	m_aActionFacialAnimSets.Reserve( nMaxFacialAnimSets );

	const int nMaxBranches = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_Branches", 0x91323726 ), CONFIGURED_FROM_FILE );
	m_aActionBranchSets.Reserve( nMaxBranches );

	const int nMaxStealthKills = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "ActionTable_StealthKills", 0x7d347ee2 ), CONFIGURED_FROM_FILE );
	m_aStealthKillTests.Reserve( nMaxStealthKills );

	// TODO: Templatize this once you get a chance
	parTree* pTree = NULL;
	parTreeNode* pRootNode = NULL;

	// CActionDefition parsing
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_DEFINITIONS );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aActionDefinitions.GetCount() < ACTIONMGR.m_aActionDefinitions.GetCapacity(), "CActionManager: Run out of ActionTable_Definitions (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionDefinitions.GetCount() );
					CActionDefinition& rActionDefinition = ACTIONMGR.m_aActionDefinitions.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rActionDefinition ), rActionDefinition.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CActionResult parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_RESULTS );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aActionResults.GetCount() < ACTIONMGR.m_aActionResults.GetCapacity(), "CActionManager: Run out of ActionTable_Results (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionResults.GetCount() );
					CActionResult& rActionResult = ACTIONMGR.m_aActionResults.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure(&rActionResult), rActionResult.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}
		
		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CImpulseTest parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_IMPULSES );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aImpulseTests.GetCount() < ACTIONMGR.m_aImpulseTests.GetCapacity(), "CActionManager: Run out of ActionTable_Impulses (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aImpulseTests.GetCount() );
					CImpulseTest& rImpulseTest = ACTIONMGR.m_aImpulseTests.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rImpulseTest ), rImpulseTest.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}
		
		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CInterrelationTest parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_INTERRELATIONS );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aInterrelationTests.GetCount() < ACTIONMGR.m_aInterrelationTests.GetCapacity(), "CActionManager: Run out of ActionTable_Interrelations (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aInterrelationTests.GetCount() );
					CInterrelationTest& rInterrelationalTest = ACTIONMGR.m_aInterrelationTests.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rInterrelationalTest ), rInterrelationalTest.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CHoming parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_HOMINGS );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aHomings.GetCount() < ACTIONMGR.m_aHomings.GetCapacity(), "CActionManager: Run out of ActionTable_Homings (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aHomings.GetCount() );
					CHoming& rHoming = ACTIONMGR.m_aHomings.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rHoming ), rHoming.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CDamageAndReaction parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_DAMAGES );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aDamageAndReactions.GetCount() < ACTIONMGR.m_aDamageAndReactions.GetCapacity(), "CActionManager: Run out of ActionTable_Damages (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aDamageAndReactions.GetCount() );
					CDamageAndReaction& rDamage = ACTIONMGR.m_aDamageAndReactions.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rDamage ), rDamage.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CStrikeBoneSet parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_STRIKE_BONES );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aStrikeBoneSets.GetCount() < ACTIONMGR.m_aStrikeBoneSets.GetCapacity(), "CActionManager: Run out of ActionTable_StrikeBones (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aStrikeBoneSets.GetCount() );
					CStrikeBoneSet& rStrikeBoneSet = ACTIONMGR.m_aStrikeBoneSets.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rStrikeBoneSet ), rStrikeBoneSet.parser_GetPointer(), false );					
				}
			}

			delete pTree;
		}
		
		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CActionRumble parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_RUMBLES );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aActionRumbles.GetCount() < ACTIONMGR.m_aActionRumbles.GetCapacity(), "CActionManager: Run out of ActionTable_Rumbles (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionRumbles.GetCount() );
					CActionRumble& rActionRumble = ACTIONMGR.m_aActionRumbles.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rActionRumble ), rActionRumble.parser_GetPointer(), false );					
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CActionVfx parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_VFX );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aActionVfx.GetCount() < ACTIONMGR.m_aActionVfx.GetCapacity(), "CActionManager: Run out of ActionTable_Vfx (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionVfx.GetCount() );
					CActionVfx& rActionVfx = ACTIONMGR.m_aActionVfx.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rActionVfx ), rActionVfx.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}


	// CActionFacialAnimSet parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_FACIAL_ANIM_SETS );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aActionFacialAnimSets.GetCount() < ACTIONMGR.m_aActionFacialAnimSets.GetCapacity(), "CActionManager: Run out of ActionTable_FacialAnimSets (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionFacialAnimSets.GetCount() );
					CActionFacialAnimSet& rActionFacialAnimSet = ACTIONMGR.m_aActionFacialAnimSets.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rActionFacialAnimSet ), rActionFacialAnimSet.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}

		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CActionBranchSet parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_BRANCHES );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aActionBranchSets.GetCount() < ACTIONMGR.m_aActionBranchSets.GetCapacity(), "CActionManager: Run out of ActionTable_Branches (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aActionBranchSets.GetCount() );
					CActionBranchSet& rActionBranchSet = ACTIONMGR.m_aActionBranchSets.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rActionBranchSet ), rActionBranchSet.parser_GetPointer(), false );					
				}
			}

			delete pTree;
		}
		
		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// CStealthKillTest parsing
	pData = DATAFILEMGR.GetFirstFile( CDataFileMgr::ACTION_TABLE_STEALTH_KILLS );
	while( DATAFILEMGR.IsValid( pData ) )
	{
		pTree = PARSER.LoadTree( pData->m_filename, "meta" );
		if( pTree )
		{
			pRootNode = pTree->GetRoot();
			pRootNode = pRootNode ? pRootNode->GetChild() : NULL;
			if( pRootNode )
			{
				parTreeNode::ChildNodeIterator itr = pRootNode->BeginChildren();
				for(; itr != pRootNode->EndChildren(); ++itr )
				{
					Assertf( ACTIONMGR.m_aStealthKillTests.GetCount() < ACTIONMGR.m_aStealthKillTests.GetCapacity(), "CActionManager: Run out of ActionTable_StealthKills (max=%d). Go to gameconfig.xml to increase pool", ACTIONMGR.m_aStealthKillTests.GetCount() );
					CStealthKillTest& rStealthKillTest = ACTIONMGR.m_aStealthKillTests.Append();
					PARSER.LoadFromStructure( (*itr), *PARSER.GetStructure( &rStealthKillTest ), rStealthKillTest.parser_GetPointer(), false );
				}
			}

			delete pTree;
		}
		
		pData = DATAFILEMGR.GetNextFile( pData );
	}

	// Post load fix-up
	PostLoad();

	m_bLoadActionTableDataOnInit = false;

#if PRINT_MEM_USAGE
	// Calculate the total memory used
	PrintMemoryUsage();
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ClearActionTableData
// PURPOSE	:	Clear all the action table file derived data.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::ClearActionTableData( void )
{
	s32 i = 0;

	// Make sure the impulse tests are totally clear.
	const int nImpulseTestCount = m_aImpulseTests.GetCount();
	for(i = 0; i < nImpulseTestCount; ++i)
		m_aImpulseTests[i].Clear();
	m_aImpulseTests.Reset();

	// Make sure the stealth kill tests are totally clear.
	const int stealthKillTestCount = m_aStealthKillTests.GetCount();
	for(i = 0; i < stealthKillTestCount; ++i)
		m_aStealthKillTests[i].Clear();
	m_aStealthKillTests.Reset();

	// Make sure the interrelation tests are totally clear.
	const int nInterrelationTestCount = m_aInterrelationTests.GetCount();
	for(i = 0; i < nInterrelationTestCount; ++i)
		m_aInterrelationTests[i].Clear();
	m_aInterrelationTests.Reset();

	// Make sure the Homings are totally clear.
	const int m_aHomingsCount = m_aHomings.GetCount();
	for(i = 0; i < m_aHomingsCount; ++i)
		m_aHomings[i].Clear();
	m_aHomings.Reset();

	// Make sure the damage and reaction are totally clear.
	const int nDamageAndReactionCount = m_aDamageAndReactions.GetCount();
	for(i = 0; i < nDamageAndReactionCount; ++i)
		m_aDamageAndReactions[i].Clear();
	m_aDamageAndReactions.Reset();

	// Make sure the strike bone sets are totally clear.
	const int nStrikeBoneSetCount = m_aStrikeBoneSets.GetCount();
	for(i = 0; i < nStrikeBoneSetCount; ++i)
		m_aStrikeBoneSets[i].Clear();
	m_aStrikeBoneSets.Reset();

	// Make sure the rumbles are totally clear.
	const int nRumbleCount = m_aActionRumbles.GetCount();
	for(i = 0; i < nRumbleCount; ++i)
		m_aActionRumbles[i].Clear();
	m_aActionRumbles.Reset();

	// Make sure the vfx are totally clear.
	const int nVfxCount = m_aActionVfx.GetCount();
	for(i = 0; i < nVfxCount; ++i)
		m_aActionVfx[i].Clear();
	m_aActionVfx.Reset();

	// Make sure the facial anim sets are totally clear.
	const int nFacialAnimSetCount = m_aActionFacialAnimSets.GetCount();
	for(i = 0; i < nFacialAnimSetCount; ++i)
		m_aActionFacialAnimSets[i].Clear();
	m_aActionFacialAnimSets.Reset();

	// Make sure the actions are totally clear.
	const int nActionDefCount = m_aActionDefinitions.GetCount();
	for(i = 0; i < nActionDefCount; ++i)
		m_aActionDefinitions[i].Clear();
	m_aActionDefinitions.Reset();

	// Make sure the action results are totally clear.
	const int nActionResCount = m_aActionResults.GetCount();
	for(i = 0; i < nActionResCount; ++i)
		m_aActionResults[i].Clear();
	m_aActionResults.Reset();

	// CActionBranchSet
	const int nActionBranchSetCount = m_aActionBranchSets.GetCount();
	for(i = 0; i < nActionBranchSetCount; ++i)
		m_aActionBranchSets[i].Clear();
	m_aActionBranchSets.Reset();

	m_bLoadActionTableDataOnInit = true;
}

#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	VerifyMeleeAnimUsage
// PURPOSE	:	Used to prevent run time errors and unnecessary memory usage.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::VerifyMeleeAnimUsage(void) const
{
	bool bBadDataFound = false;

	// Check that the anims used in the action table exist in the anim metadata
	const int nActionResCount = m_aActionResults.GetCount();
	for( s32 i = 0; i < nActionResCount; ++i )
	{
		// Get a local pointer to the result (to make debugging easier).
		const CActionResult* pResult = &m_aActionResults[i];
		Assertf( pResult, "ActionTable Validation: Result list appears to have been incorrectly populated by the code." );

		const fwMvClipSetId	clipSetId	= (fwMvClipSetId)pResult->GetClipSetID();
		const fwMvClipId	clipId		= (fwMvClipId)pResult->GetAnimID();
		if( clipSetId != CLIP_SET_ID_INVALID && clipId != ANIM_ID_INVALID )
		{
			const fwClipSet *clipSet = fwClipSetManager::GetClipSet( clipSetId );
			Assert( clipSet );
			if( !clipSet )
			{
				bBadDataFound = true;
				Warningf( "ActionTable Validation: Cannot find clip set (%s) (it probably isn't in the anim metadata).", pResult->GetClipSetName() );
			}

			const fwClipItem* pAnimItem = NULL;
			pAnimItem = fwClipSetManager::GetClipItem( clipSetId, clipId );
			Assert( pAnimItem );
			if( !pAnimItem )
			{
				bBadDataFound = true;
				Warningf( "ActionTable Validation: Anim Name (%s, %s) does not have an anim entry (in the anim metadata).", pResult->GetClipSetName(), pResult->GetAnimName() );
			}
		}
	}

	// Check that the anims used in the anim metadata exist in the action table
	for( u32 i = 0; i < fwClipSetManager::GetClipSetCount(); i++ )
	{
		fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(i);
		const fwClipSet* clipSet = fwClipSetManager::GetClipSetByIndex(i);
		Assert(clipSet);
		if( clipSet )
		{
			for(s32 i = 0; i < clipSet->GetClipItemCount(); ++i)
			{	
				fwMvClipId clipItemId = clipSet->GetClipItemIdByIndex(i);
				const fwClipItem *clipItem = clipSet->GetClipItemByIndex(i);
				Assert(clipItem);
				if (clipItem)
				{
					// Skip any movement anims as they are not referenced directly in the action table.
					// (Though they are used by the melee combat system.)
					if(	(clipItemId == fwMvClipId("idle",0x71C21326)) ||
						(clipItemId == fwMvClipId("run",0x1109B569)) ||		
						(clipItemId == fwMvClipId("run_strafe_b",0xE9B082FA)) ||		
						(clipItemId == fwMvClipId("run_strafe_l",0x20166FBD)) ||		
						(clipItemId == fwMvClipId("run_strafe_r",0xCD3ACA13)) ||		
						(clipItemId == fwMvClipId("walk",0x83504C9C)) ||		
						(clipItemId == fwMvClipId("walk_strafe_b",0x9ED00576)) ||		
						(clipItemId == fwMvClipId("walk_strafe_l",0x51BCEB51)) ||		
						(clipItemId == fwMvClipId("run",0x1109B569)) ||		
						(clipItemId == fwMvClipId("walk_strafe_r",0xFA603C91)) )
					{
						continue;
					}

					// Check if any of the results actually use this animation.
					bool usedByARes = false;
					const int nActionResCount = m_aActionResults.GetCount();
					for(s32 j = 0; j < nActionResCount; ++j)
					{
						// Get a local pointer to the result (to make debugging easier).
						const CActionResult* pResult = &m_aActionResults[j];
						Assertf(pResult, "ActionTable Validation: Result list appears to have been incorrectly populated by the code.");

						if(((fwMvClipSetId)pResult->GetClipSetID() == clipSetId) && ((fwMvClipId)pResult->GetAnimID() == clipItemId ))
						{
							usedByARes = true;
							break;
						}
					}

					// Make sure we found a res that uses it.
					if(!usedByARes)
					{
						bBadDataFound = true;
						Warningf("ActionTable Validation: Anim meta data (%s, %s) found which does not appear in actiontable (it should probably be removed).", clipSetId.GetCStr(), clipItemId.GetCStr());
					}
				}

				if(!bBadDataFound)
				{
					Displayf("ActionTable Validation: Anim data appears to be fine");
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsValid
// PURPOSE	:	Make sure that the table is internally valid. Check all internal 
//				references to make sure that they reference only existing items 
//				in the table.
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::IsValid( void ) const
{
	// Validate the actions.
	const int nActionDefCount = m_aActionDefinitions.GetCount();
	for( s32 nActionDefIdx = 0; nActionDefIdx < nActionDefCount; ++nActionDefIdx )
	{
		// Get a local pointer to the action (to make debugging easier).
		const CActionDefinition* pActionDef = &m_aActionDefinitions[ nActionDefIdx ];
		Assertf( pActionDef, "ActionTable Validation: Action list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this action appears.
		// Note, we only check the previous actions in the list, but since
		// this is done for all actions it should catch any duplicates.
		for( s32 previousActionDefIndex = 0; previousActionDefIndex < nActionDefIdx; ++previousActionDefIndex )
		{
			const CActionDefinition* pPreviousActionDef = &m_aActionDefinitions[ previousActionDefIndex ];

			bool bHashesAreDifferent = (pActionDef->GetID() != pPreviousActionDef->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: Action (%s) has the same name hash as a previous action(%s).", pActionDef->GetName(),pPreviousActionDef->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the action validate itself internally.
		Assertf( pActionDef->IsValid(), "ActionTable Validation: Action (%s) failed it's field validation test.", pActionDef->GetName() );
		if( !pActionDef->IsValid() )
		{
			return false;
		}

		// Make sure the impulseTest exists.
		u32 nRequiredImpulseTestId = pActionDef->GetImpulseTestID();
		if( nRequiredImpulseTestId != 0 )
		{
			u32 nIndexFound = 0;
			const CImpulseTest* pRequiredImpulseTest = FindImpulseTest( nIndexFound, nRequiredImpulseTestId );
			Assertf( pRequiredImpulseTest, "ActionTable Validation: Action (%s) failed finding the Required ImpulseTest (%s) (maybe the result name is misspelled).", pActionDef->GetName(), pActionDef->GetImpulseTestName() );
			if( !pRequiredImpulseTest )
			{
				return false;
			}
		}

		// Make sure the interrelation exists.
		u32 nRequiredInterrelationTestId = pActionDef->GetInterrelationTestID();
		if( nRequiredInterrelationTestId != 0 )
		{
			u32 nIndexFound = 0;
			const CInterrelationTest* pRequiredInterrelationTest = FindInterrelationTest( nIndexFound, nRequiredInterrelationTestId );
			Assertf( pRequiredInterrelationTest, "ActionTable Validation: Action (%s) failed finding the Required InterrelationTest (%s) (maybe the result name is misspelled).", pActionDef->GetName(), pActionDef->GetInterrelationTestName() );
			if( !pRequiredInterrelationTest )
			{
				return false;
			}
		}

		// Make sure that the RequiredOpponentRes exists.
		const CActionCondPedOther* pOtherPed = pActionDef->GetCondPedOther();
		if( pOtherPed )
		{
			// Make sure the interrelation exists.
			nRequiredInterrelationTestId = pOtherPed->GetInterrelationTestID();
			if( nRequiredInterrelationTestId != 0 )
			{
				u32 nIndexFound = 0;
				const CInterrelationTest* pRequiredInterrelationTest = FindInterrelationTest( nIndexFound, nRequiredInterrelationTestId );
				Assertf( pRequiredInterrelationTest, "ActionTable Validation: Action (%s) failed finding the Required InterrelationTest (%s) (maybe the result name is misspelled).", pActionDef->GetName(), pActionDef->GetInterrelationTestName() );
				if( !pRequiredInterrelationTest )
				{
					return false;
				}
			}

			int nNumActionResultLists = pOtherPed->GetNumWeaponActionResultLists();
			for( int i = 0; i < nNumActionResultLists; i++ )
			{
				const CWeaponActionResultList* pWeaponActionResultList = pOtherPed->GetWeaponActionResultList( i );
				if( pWeaponActionResultList )
				{
					int nNumActionResults = pWeaponActionResultList->GetNumActionResults();
					for( int j = 0; j < nNumActionResults; j++ )
					{
						if( !pWeaponActionResultList->GetActionResult( j ) )
						{
							Assertf(0, "ActionTable Validation: Action (%s) failed finding the Required Opponent Result (%s) (maybe the result name is misspelled).", pActionDef->GetName(), pWeaponActionResultList->GetActionResultName( i ) );
							return false;
						}
					}
				}
			}
		}

		// Make sure that the all the action results exist.
		for( u32 nActionResultIdx = 0; nActionResultIdx < pActionDef->GetNumWeaponActionResults(); nActionResultIdx++ )
		{
			const CWeaponActionResult* pWeaponActionResult = pActionDef->GetWeaponActionResult( nActionResultIdx );
			if ( pWeaponActionResult )
			{
				if( !pWeaponActionResult->GetActionResult() )
				{
					Assertf( 0, "ActionTable Validation: Action (%s) failed finding it's result (%s) (maybe the result name is misspelled).", pActionDef->GetName(), pWeaponActionResult->GetActionResultName());
					return false;
				}
			}
		}
	}

	// Validate the results.
	const int nActionResCount = m_aActionResults.GetCount();
	for( s32 nActionResIdx = 0; nActionResIdx < nActionResCount; ++nActionResIdx )
	{
		// Get a local pointer to the result (to make debugging easier).
		const CActionResult* pActionRes = &m_aActionResults[ nActionResIdx ];
		Assertf( pActionRes, "ActionTable Validation: Result list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this result appears.
		// Note, we only check the previous results in the list, but since
		// this is done for all actions it should catch any duplicates.
		for( s32 nPreviousActionResIdx = 0; nPreviousActionResIdx < nActionResIdx; ++nPreviousActionResIdx )
		{
			const CActionResult* pPreviousActionRes = &m_aActionResults[ nPreviousActionResIdx ];
			bool bHashesAreDifferent = (pActionRes->GetID() != pPreviousActionRes->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: Result (%s) has the same name hash as a previous result(%s).", pActionRes->GetName(), pPreviousActionRes->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the action result validate itself internally.
		Assertf( pActionRes->IsValid(), "ActionTable Validation: Result (%s) failed it's field validation test.", pActionRes->GetName() );
		if( !pActionRes->IsValid() )
		{
			return false;
		}

		// Make sure the damageAndReaction exists.
		u32 nRequiredDamageAndReactionId = pActionRes->GetDamageAndReactionID();
		if( nRequiredDamageAndReactionId != 0 )
		{
			u32 nIndexFound = 0;
			const CDamageAndReaction* pRequiredDamageAndReaction = FindDamageAndReaction( nIndexFound, nRequiredDamageAndReactionId );
			Assertf( pRequiredDamageAndReaction, "ActionTable Validation: Result (%s) failed finding the Required DamageAndReaction (%s) (maybe the result name is misspelled).", pActionRes->GetName(), pActionRes->GetDamageAndReactionName() );
			if( !pRequiredDamageAndReaction )
			{
				return false;
			}
		}

		// Make sure the strikeBoneSet exists.
		u32 nRequiredStrikeBoneSetId = pActionRes->GetStrikeBoneSetID();
		if( nRequiredStrikeBoneSetId != 0 )
		{
			u32 nIndexFound = 0;
			const CStrikeBoneSet* pRequiredStrikeBoneSet = FindStrikeBoneSet( nIndexFound, nRequiredStrikeBoneSetId );
			Assertf( pRequiredStrikeBoneSet, "ActionTable Validation: Result (%s) failed finding the Required StrikeBoneSet (%s) (maybe the result name is misspelled).", pActionRes->GetName(), pActionRes->GetStrikeBoneSetName() );
			if( !pRequiredStrikeBoneSet )
			{
				return false;
			}
		}

		// Make sure the res is used by at least one action...
		bool bIsUsedByAnAction = false;
		for( s32 nActionDefIdx = 0; nActionDefIdx < nActionDefCount; ++nActionDefIdx )
		{
			// Get a local pointer to the action (to make debugging easier).
			const CActionDefinition* pActionDef = &m_aActionDefinitions[ nActionDefIdx ];
			Assertf( pActionDef, "ActionTable Validation: Action list appears to have been incorrectly populated by the code." );
			for( u32 nActionResultIdx = 0; nActionResultIdx < pActionDef->GetNumWeaponActionResults(); nActionResultIdx++ )
			{
				const CWeaponActionResult* pWeaponActionResult = pActionDef->GetWeaponActionResult( nActionResultIdx );
				if( pWeaponActionResult->GetActionResultID() == pActionRes->GetID() )
				{
					bIsUsedByAnAction = true;
					break;
				}
			}

			if( bIsUsedByAnAction )
				break;
		}

		Assertf( bIsUsedByAnAction, "ActionTable Validation: Res (%s) appears to be unused.", pActionRes->GetName() );
		if( !bIsUsedByAnAction )
		{
			return false;
		}
	}

	// Validate the action branch sets.
	const int nActionBranchSetCount = m_aActionBranchSets.GetCount();
	for( s32 nActionBranchSetIdx = 0; nActionBranchSetIdx < nActionBranchSetCount; ++nActionBranchSetIdx )
	{
		// Get a local pointer to the action (to make debugging easier).
		const CActionBranchSet* pActionBranchSet = &m_aActionBranchSets[ nActionBranchSetIdx ];
		Assertf( pActionBranchSet, "ActionTable Validation: Action branch set appears to have been incorrectly populated by the code." );

		// Check for duplicates
		for( s32 nPrevActionBranchIdx = 0; nPrevActionBranchIdx < nActionBranchSetIdx; ++nPrevActionBranchIdx )
		{
			const CActionBranchSet* pPrevActionBranchSet = &m_aActionBranchSets[ nPrevActionBranchIdx ];

			bool bHashesAreDifferent = ( pActionBranchSet->GetID() != pPrevActionBranchSet->GetID() );
			Assertf( bHashesAreDifferent, "ActionTable Validation: Action Branch Set (%s) has the same name hash as a previous set (%s).", pActionBranchSet->GetName(), pPrevActionBranchSet->GetName() );
			if( !bHashesAreDifferent )
				return false;
		}

		// Have the action validate itself internally.
		Assertf( pActionBranchSet->IsValid(), "ActionTable Validation: Action Branch Set (%s) failed it's introspective validation test.", pActionBranchSet->GetName() );
		if( !pActionBranchSet->IsValid() )
			return false;

		// Check that its child branches exist.
		s32 nNumBranches = pActionBranchSet->GetNumBranches();
		for( s32 nBranchIdx = 0; nBranchIdx < nNumBranches; ++nBranchIdx )
		{	
			// Make sure this action branch is present.
			u32 nBranchActionId = pActionBranchSet->GetBranchActionID( nBranchIdx );
			if( nBranchActionId != 0 )
			{
				u32 nIndexFound = 0;
				const CActionDefinition* pAction = FindActionDefinition( nIndexFound, nBranchActionId );
				Assertf( pAction, "ActionTable Validation: Action Branch Set (%s) failed finding an action definition (%s) (maybe the action name is misspelled).", pActionBranchSet->GetName(), pActionBranchSet->GetBranchActionName( nBranchIdx ) );
				if( !pAction )
					return false;
			}
		}

		// Is it used?
		bool bIsUsedByAnActionResult = false;
		for( s32 nActionResIdx = 0; nActionResIdx < nActionResCount; ++nActionResIdx )
		{
			// Get a local pointer to the action (to make debugging easier).
			const CActionResult* pActionResult = &m_aActionResults[ nActionResIdx ];
			Assertf( pActionResult, "ActionTable Validation: ActionResult list appears to have been incorrectly populated by the code." );
			if( pActionResult->GetActionBranchSetID() == pActionBranchSet->GetID() )
			{
				bIsUsedByAnActionResult = true;
				break;
			}
		}

		Assertf( bIsUsedByAnActionResult, "ActionTable Validation: Action Branch Set (%s) appears to be unused.", pActionBranchSet->GetName() );
		if( !bIsUsedByAnActionResult )
			return false;
	}

	// Validate the impulse tests.
	const int nImpulseTestCount = m_aImpulseTests.GetCount();
	for( s32 nImpulseTestIdx = 0; nImpulseTestIdx < nImpulseTestCount; ++nImpulseTestIdx )
	{
		// Get a local pointer to the impulseTest (to make debugging easier).
		const CImpulseTest* pImpulseTest = &m_aImpulseTests[ nImpulseTestIdx ];
		Assertf( pImpulseTest, "ActionTable Validation: ImpulseTest list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this impulseTest appears.
		// Note, we only check the previous impulseTests in the list, but since
		// this is done for all impulseTests it should catch any duplicates.
		for( s32 nPreviousDualImpuseTestIdx = 0; nPreviousDualImpuseTestIdx < nImpulseTestIdx; ++nPreviousDualImpuseTestIdx)
		{
			const CImpulseTest* pPreviousDualImpuseTest = &m_aImpulseTests[ nPreviousDualImpuseTestIdx ];
			bool bHashesAreDifferent = (pImpulseTest->GetID() != pPreviousDualImpuseTest->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: ImpulseTest (%s) has the same name hash as a previous impulseTest(%s).", pImpulseTest->GetName(), pPreviousDualImpuseTest->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the ImpulseTest validate itself internally.
		Assertf( pImpulseTest->IsValid(), "ActionTable Validation: ImpulseTest (%s) failed it's field validation test.", pImpulseTest->GetName() );
		if( !pImpulseTest->IsValid() )
		{
			return false;
		}

		// Make sure the impulseTest is used by at least one action...
		bool bIsUsedByAnAction = false;
		for( s32 nActionDefIdx = 0; nActionDefIdx < nActionDefCount; ++nActionDefIdx )
		{
			// Get a local pointer to the action (to make debugging easier).
			const CActionDefinition* pActionDef = &m_aActionDefinitions[ nActionDefIdx ];
			Assertf( pActionDef, "ActionTable Validation: Action list appears to have been incorrectly populated by the code." );
			if( pActionDef->GetImpulseTestID() == pImpulseTest->GetID())
			{
				bIsUsedByAnAction = true;
				break;
			}
		}
		Assertf( bIsUsedByAnAction, "ActionTable Validation: ImpulseTest (%s) appears to be unused.", pImpulseTest->GetName() );
		if( !bIsUsedByAnAction )
		{
			return false;
		}
	}

	// Validate the interrelation tests.
	const int nInterrelationTestCount = m_aInterrelationTests.GetCount();
	for( s32 nInterrelationTestIdx = 0; nInterrelationTestIdx < nInterrelationTestCount; ++nInterrelationTestIdx )
	{
		// Get a local pointer to the interrelationTest (to make debugging easier).
		const CInterrelationTest* pInterrelationTest = &m_aInterrelationTests[ nInterrelationTestIdx ];
		Assertf( pInterrelationTest, "ActionTable Validation: InterrelationTest list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this interrelationTest appears.
		// Note, we only check the previous interrelationTests in the list, but since
		// this is done for all interrelationTests it should catch any duplicates.
		for( s32 nPreviousInterrelationTestIdx = 0; nPreviousInterrelationTestIdx < nInterrelationTestIdx; ++nPreviousInterrelationTestIdx )
		{
			const CInterrelationTest* pPreviousInterrelationTest = &m_aInterrelationTests[ nPreviousInterrelationTestIdx ];
			bool bHashesAreDifferent = (pInterrelationTest->GetID() != pPreviousInterrelationTest->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: InterrelationTest (%s) has the same name hash as a previous interrelationTest(%s).", pInterrelationTest->GetName(), pPreviousInterrelationTest->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the InterrelationTest validate itself internally.
		Assertf( pInterrelationTest->IsValid(), "ActionTable Validation: InterrelationTest (%s) failed it's field validation test.", pInterrelationTest->GetName() );
		if( !pInterrelationTest->IsValid() )
		{
			return false;
		}

		// Make sure interrelation test is used by at least one res or action.
		bool bIsUsedByAnActionOrARes = false;
		for( s32 nActionDefIdx = 0; nActionDefIdx < nActionDefCount; ++nActionDefIdx )
		{
			// Get a local pointer to the action (to make debugging easier).
			const CActionDefinition* pActionDef = &m_aActionDefinitions[ nActionDefIdx ];
			Assertf( pActionDef, "ActionTable Validation: Action list appears to have been incorrectly populated by the code." );

			if( pActionDef->GetInterrelationTestID() == pInterrelationTest->GetID() )
			{
				bIsUsedByAnActionOrARes = true;
				break;
			}

			const CActionCondPedOther* pCondPedOther = pActionDef->GetCondPedOther();
			Assertf( pCondPedOther, "ActionTable Validation: Other ped appears to have been incorrectly populated by the code." );

			if( pCondPedOther->GetInterrelationTestID() == pInterrelationTest->GetID() )
			{
				bIsUsedByAnActionOrARes = true;
				break;
			}
		}

		Assertf( bIsUsedByAnActionOrARes, "ActionTable Validation: InterrelationTest (%s) appears to be unused.", pInterrelationTest->GetName() );
		if( !bIsUsedByAnActionOrARes )
		{
			return false;
		}
	}

	// Validate the homings.
	const int nHomingCount = m_aHomings.GetCount();
	for( s32 nHomingIdx = 0; nHomingIdx < nHomingCount; ++nHomingIdx )
	{
		// Get a local pointer to the homing (to make debugging easier).
		const CHoming* pHoming = &m_aHomings[ nHomingIdx ];
		Assertf( pHoming, "ActionTable Validation: Homing list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this homing appears.
		// Note, we only check the previous homing in the list, but since
		// this is done for all homing it should catch any duplicates.
		for( s32 nPreviousHomingIdx = 0; nPreviousHomingIdx < nHomingIdx; ++nPreviousHomingIdx )
		{
			const CHoming* pPreviousHoming = &m_aHomings[ nPreviousHomingIdx ];
			bool bHashesAreDifferent = (pHoming->GetID() != pPreviousHoming->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: Homing (%s) has the same name hash as a previous homing(%s).", pHoming->GetName(), pPreviousHoming->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the Homing validate itself internally.
		Assertf( pHoming->IsValid(), "ActionTable Validation: Homing (%s) failed it's field validation test.", pHoming->GetName() );
		if( !pHoming->IsValid() )
		{
			return false;
		}

		// Make sure the homing is used by at least one res...
		bool bIsUsedByARes = false;
		for( s32 nActionResIdx = 0; nActionResIdx < nActionResCount; ++nActionResIdx )
		{
			// Get a local pointer to the res (to make debugging easier).
			const CActionResult* pActionRes = &m_aActionResults[ nActionResIdx ];
			Assertf( pActionRes, "ActionTable Validation: Res list appears to have been incorrectly populated by the code." );
			if( pActionRes->GetHomingID() == pHoming->GetID() )
			{
				bIsUsedByARes = true;
				break;
			}
		}

		Assertf( bIsUsedByARes, "ActionTable Validation: Homing (%s) appears to be unused.", pHoming->GetName() );
		if( !bIsUsedByARes )
		{
			return false;
		}
	}

	// Validate the damage and reaction.
	const int nDamageAndReactionCount = m_aDamageAndReactions.GetCount();
	for( s32 nDamageAndReactionIdx = 0; nDamageAndReactionIdx < nDamageAndReactionCount; ++nDamageAndReactionIdx )
	{
		// Get a local pointer to the damageAndReaction (to make debugging easier).
		const CDamageAndReaction* pDamageAndReaction = &m_aDamageAndReactions[ nDamageAndReactionIdx ];
		Assertf( pDamageAndReaction, "ActionTable Validation: DamageAndReaction list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this damageAndReaction appears.
		// Note, we only check the previous damageAndReaction in the list, but since
		// this is done for all damageAndReaction it should catch any duplicates.
		for( s32 nPreviousDamageAndReactionIdx = 0; nPreviousDamageAndReactionIdx < nDamageAndReactionIdx; ++nPreviousDamageAndReactionIdx )
		{
			const CDamageAndReaction* pPreviousDamageAndReaction = &m_aDamageAndReactions[ nPreviousDamageAndReactionIdx ];
			bool bHashesAreDifferent = (pDamageAndReaction->GetID() != pPreviousDamageAndReaction->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: DamageAndReaction (%s) has the same name hash as a previous damageAndReaction(%s).", pDamageAndReaction->GetName(), pPreviousDamageAndReaction->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the DamageAndReaction validate itself internally.
		Assertf( pDamageAndReaction->IsValid(), "ActionTable Validation: DamageAndReaction (%s) failed it's field validation test.", pDamageAndReaction->GetName() );
		if( !pDamageAndReaction->IsValid() )
		{
			return false;
		}

		// Make sure the damageAndReaction is used by at least one res...
		bool bIsUsedByARes = false;
		for( s32 nActionResIdx = 0; nActionResIdx < nActionResCount; ++nActionResIdx )
		{
			// Get a local pointer to the res (to make debugging easier).
			const CActionResult* pActionRes = &m_aActionResults[ nActionResIdx ];
			Assertf( pActionRes, "ActionTable Validation: Res list appears to have been incorrectly populated by the code." );
			if( pActionRes->GetDamageAndReactionID() == pDamageAndReaction->GetID() )
			{
				bIsUsedByARes = true;
				break;
			}
		}
		Assertf( bIsUsedByARes, "ActionTable Validation: DamageAndReaction- (%s) appears to be unused.", pDamageAndReaction->GetName() );
		if( !bIsUsedByARes )
		{
			return false;
		}
	}

	// Validate the strike bone sets.
	const int nStrikeBoneSetCount = m_aStrikeBoneSets.GetCount();
	for( s32 nStrikeBoneSetIdx = 0; nStrikeBoneSetIdx < nStrikeBoneSetCount; ++nStrikeBoneSetIdx )
	{
		// Get a local pointer to the strikeBoneSet (to make debugging easier).
		const CStrikeBoneSet* pStrikeBoneSet = &m_aStrikeBoneSets[ nStrikeBoneSetIdx ];
		Assertf( pStrikeBoneSet, "ActionTable Validation: StrikeBoneSet list appears to have been incorrectly populated by the code." );

		// Make sure this is the only time this strikeBoneSet appears.
		// Note, we only check the previous strikeBoneSets in the list, but since
		// this is done for all strikeBoneSets it should catch any duplicates.
		for( s32 nPreviousStrikeBoneSetIdx = 0; nPreviousStrikeBoneSetIdx < nStrikeBoneSetIdx; ++nPreviousStrikeBoneSetIdx )
		{
			const CStrikeBoneSet* pPreviousStrikeBoneSet = &m_aStrikeBoneSets[ nPreviousStrikeBoneSetIdx ];
			bool bHashesAreDifferent = (pStrikeBoneSet->GetID() != pPreviousStrikeBoneSet->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: StrikeBoneSet (%s) has the same name hash as a previous strikeBoneSet(%s).", pStrikeBoneSet->GetName(), pPreviousStrikeBoneSet->GetName() );
			if( !bHashesAreDifferent )
			{
				return false;
			}
		}

		// Have the StrikeBoneSet validate itself internally.
		Assertf( pStrikeBoneSet->IsValid(), "ActionTable Validation: StrikeBoneSet (%s) failed it's field validation test.", pStrikeBoneSet->GetName() );
		if( !pStrikeBoneSet->IsValid() )
		{
			return false;
		}

		bool bIgnoreUseCheck = pStrikeBoneSet->GetNameHashString() == atHashString("SB_left_arm_melee_weapon",0xFE136F8B);
		// Make sure the strikeBoneSet is used by at least one res...
		bool bIsUsedByARes = false;
		if (!bIgnoreUseCheck)
		{
			for( s32 nActionResIdx = 0; nActionResIdx < nActionResCount; ++nActionResIdx )
			{
				// Get a local pointer to the res (to make debugging easier).
				const CActionResult* pRes = &m_aActionResults[ nActionResIdx ];
				Assertf( pRes, "ActionTable Validation: Res list appears to have been incorrectly populated by the code." );
				if( pRes->GetStrikeBoneSetID() == pStrikeBoneSet->GetID() )
				{
					bIsUsedByARes = true;
					break;
				}
			}
		}
		else
		{
			bIsUsedByARes = true;
		}

		Assertf( bIsUsedByARes, "ActionTable Validation: StrikeBoneSet- (%s) appears to be unused.", pStrikeBoneSet->GetName() );
		if( !bIsUsedByARes )
		{
			return false;
		}
	}

	// Validate the rumbles
	const int nRumbleCount = m_aActionRumbles.GetCount();
	for( s32 nRumbleIdx = 0; nRumbleIdx < nRumbleCount; ++nRumbleIdx )
	{
		const CActionRumble* pRumble = &m_aActionRumbles[ nRumbleIdx ];
		Assertf( pRumble, "ActionTable Validation: Rumble list appears to have been incorrectly populated by the code." );

		for( s32 nPreviousRumbleIdx = 0; nPreviousRumbleIdx < nRumbleIdx; ++nPreviousRumbleIdx )
		{
			const CActionRumble* pPreviousRumble = &m_aActionRumbles[ nPreviousRumbleIdx ];
			bool bHashesAreDifferent = (pRumble->GetID() != pPreviousRumble->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: Rumble (%s) has the same name hash as a previous rumble(%s).", pRumble->GetName(), pPreviousRumble->GetName() );
			if( !bHashesAreDifferent )
				return false;
		}

		// Have the rumble validate itself internally.
		Assertf( pRumble->IsValid(), "ActionTable Validation: Rumble (%s) failed it's field validation test.", pRumble->GetName() );
		if( !pRumble->IsValid() )
			return false;

		// Make sure it is at least used once
		bool bIsUsedByARes = false;
		for( s32 nDamageAndReactionIdx = 0; nDamageAndReactionIdx < nDamageAndReactionCount; ++nDamageAndReactionIdx )
		{
			const CDamageAndReaction* pDamageAndReaction = &m_aDamageAndReactions[ nDamageAndReactionIdx ];
			Assertf( pDamageAndReaction, "ActionTable Validation: Dmaage and Reaction list appears to have been incorrectly populated by the code." );
			if( pDamageAndReaction->GetActionRumbleID() == pRumble->GetID() )
			{
				bIsUsedByARes = true;
				break;
			}
		}

		Assertf( bIsUsedByARes, "ActionTable Validation: Rumble (%s) appears to be unused.", pRumble->GetName() );
		if( !bIsUsedByARes )
			return false;
	}

	// Validate the vfx objects
	const int nVfxCount = m_aActionVfx.GetCount();
	for( s32 nVfxIdx = 0; nVfxIdx < nVfxCount; ++nVfxIdx )
	{
		const CActionVfx* pVfx = &m_aActionVfx[ nVfxIdx ];
		Assertf( pVfx, "ActionTable Validation: Vfx list appears to have been incorrectly populated by the code." );

		for( s32 nPreviousVfxIdx = 0; nPreviousVfxIdx < nVfxIdx; ++nPreviousVfxIdx )
		{
			const CActionVfx* pPreviousVfx = &m_aActionVfx[ nPreviousVfxIdx ];
			bool bHashesAreDifferent = (pVfx->GetID() != pPreviousVfx->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: Vfx (%s) has the same name hash as a previous vfx(%s).", pVfx->GetName(), pPreviousVfx->GetName() );
			if( !bHashesAreDifferent )
				return false;
		}

		// Have the vfx object validate itself internally.
		Assertf( pVfx->IsValid(), "ActionTable Validation: Vfx (%s) failed it's field validation test.", pVfx->GetName() );
		if( !pVfx->IsValid() )
			return false;

		// Since these are only defined in anim events... we can't validate if they are used
	}

	// Validate the facial anim sets
	const int nFacialAnimSetCount = m_aActionFacialAnimSets.GetCount();
	for( s32 nFacialAnimSetIdx = 0; nFacialAnimSetIdx < nFacialAnimSetCount; ++nFacialAnimSetIdx )
	{
		const CActionFacialAnimSet* pFacialAnimSet = &m_aActionFacialAnimSets[ nFacialAnimSetIdx ];
		Assertf( pFacialAnimSet, "ActionTable Validation: Facial animation set list appears to have been incorrectly populated by the code." );

		for( s32 nPreviousFacialAnimSetIdx = 0; nPreviousFacialAnimSetIdx < nFacialAnimSetIdx; ++nPreviousFacialAnimSetIdx )
		{
			const CActionFacialAnimSet* pPreviousFacialAnimSet = &m_aActionFacialAnimSets[ nPreviousFacialAnimSetIdx ];
			bool bHashesAreDifferent = (pFacialAnimSet->GetID() != pPreviousFacialAnimSet->GetID());
			Assertf( bHashesAreDifferent, "ActionTable Validation: Facial anim set (%s) has the same name hash as a previous facial anim set(%s).", pFacialAnimSet->GetName(), pPreviousFacialAnimSet->GetName() );
			if( !bHashesAreDifferent )
				return false;
		}

		Assertf( pFacialAnimSet->IsValid(), "ActionTable Validation: Facial anim set (%s) failed it's field validation test.", pFacialAnimSet->GetName() );
		if( !pFacialAnimSet->IsValid() )
			return false;

		// Since these are only defined in anim events... we can't validate if they are used
	}

	// Everything seems to have gone fine...
	return true;
}
#endif 

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindImpulseTest
// PURPOSE	:	Find a impulseTest given just an impulseTest Id.
// PARAMS	:	impulseTestId - impulseTest we are interested in finding.
// RETURNS	:	The address of the impulseTest.
/////////////////////////////////////////////////////////////////////////////////
const CImpulseTest* CActionManager::FindImpulseTest(u32& indexOut, const u32 impulseTestId) const
{
	// Try to find the impulseTest (with a simple linear search).
	const int nImpulseTestCount = m_aImpulseTests.GetCount();
	for(s32 i = 0; i < nImpulseTestCount; ++i)
	{
		if(impulseTestId == m_aImpulseTests[i].GetID())
		{
			indexOut = i;
			return &m_aImpulseTests[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindInterrelationTest
// PURPOSE	:	Find a interrelationTest given just an interrelationTest Id.
// PARAMS	:	interrelationTestId - interrelationTest we are interested in finding.
// RETURNS	:	The address of the interrelationTest.
/////////////////////////////////////////////////////////////////////////////////
const CInterrelationTest* CActionManager::FindInterrelationTest(u32& indexOut, const u32 interrelationTestId) const
{
	// Try to find the interrelationTest (with a simple linear search).
	const int nInterrelationTestCount = m_aInterrelationTests.GetCount();
	for(s32 i = 0; i < nInterrelationTestCount; ++i)
	{
		if(interrelationTestId == m_aInterrelationTests[i].GetID())
		{
			indexOut = i;
			return &m_aInterrelationTests[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindHoming
// PURPOSE	:	Find a homing given just an homing Id.
// PARAMS	:	homingId - homing we are interested in finding.
// RETURNS	:	The address of the homing.
/////////////////////////////////////////////////////////////////////////////////
const CHoming* CActionManager::FindHoming(u32& indexOut, const u32 homingId) const
{
	// Try to find the homingId (with a simple linear search).
	const int nHomingCount = m_aHomings.GetCount();
	for(s32 i = 0; i < nHomingCount; ++i)
	{
		if(homingId == m_aHomings[i].GetID())
		{
			indexOut = i;
			return &m_aHomings[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindDamageAndReaction
// PURPOSE	:	Find a damageAndReaction given just an damageAndReaction Id.
// PARAMS	:	damageAndReactionId - damageAndReaction we are interested in finding.
// RETURNS	:	The address of the damageAndReaction.
/////////////////////////////////////////////////////////////////////////////////
const CDamageAndReaction* CActionManager::FindDamageAndReaction(u32& indexOut, const u32 damageAndReactionId) const
{
	// Try to find the damageAndReaction (with a simple linear search).
	const int nDamageAndReactionCount = m_aDamageAndReactions.GetCount();
	for(s32 i = 0; i < nDamageAndReactionCount; ++i)
	{
		if(damageAndReactionId == m_aDamageAndReactions[i].GetID())
		{
			indexOut = i;
			return &m_aDamageAndReactions[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindStrikeBoneSet
// PURPOSE	:	Find a strikeBoneSet given just an strikeBoneSet Id.
// PARAMS	:	strikeBoneSetId - strikeBoneSet we are interested in finding.
// RETURNS	:	The address of the strikeBoneSet.
/////////////////////////////////////////////////////////////////////////////////
const CStrikeBoneSet* CActionManager::FindStrikeBoneSet(u32& indexOut, const u32 strikeBoneSetId) const
{
	// Try to find the strikeBoneSetId (with a simple linear search).
	const int nStrikeBoneSetCount = m_aStrikeBoneSets.GetCount();
	for(s32 i = 0; i < nStrikeBoneSetCount; ++i)
	{
		if(strikeBoneSetId == m_aStrikeBoneSets[i].GetID())
		{
			indexOut = i;
			return &m_aStrikeBoneSets[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionRumble
// PURPOSE	:	Find a rumble data struct
// PARAMS	:	rnIndexOut - return index of the rumble you would like to look for.
//				nRumbleId - Hash id of the rumble struct you are looking for
// RETURNS	:	Ptr to the rumble
/////////////////////////////////////////////////////////////////////////////////
const CActionRumble* CActionManager::FindActionRumble(u32& rnIndexOut, const u32 nRumbleId) const
{
	const int nRumbleCount = m_aActionRumbles.GetCount();
	for(s32 i = 0; i < nRumbleCount; ++i)
	{
		if(nRumbleId == m_aActionRumbles[i].GetID())
		{
			rnIndexOut = i;
			return &m_aActionRumbles[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionVfx
// PURPOSE	:	Find a vfx data set
// PARAMS	:	rnIndexOut - return index of the vfx you would like to look for.
//				nVfxSetId - Hash id of the vfx struct you are looking for
// RETURNS	:	Ptr to the vfx
/////////////////////////////////////////////////////////////////////////////////
const CActionVfx* CActionManager::FindActionVfx( u32& rnIndexOut, const u32 nVfxId ) const
{
	const int nVfxCount = m_aActionVfx.GetCount();
	for( s32 nVfxIdx = 0; nVfxIdx < nVfxCount; ++nVfxIdx )
	{
		if( nVfxId == m_aActionVfx[nVfxIdx].GetID() )
		{
			rnIndexOut = nVfxIdx;
			return &m_aActionVfx[nVfxIdx];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionFacialAnimSet
// PURPOSE	:	Find a facial animation set
// PARAMS	:	rnIndexOut - return index of the facial anim set you would like to look for.
//				nFacialAnimSetId - Hash id of the facial anim struct you are looking for
// RETURNS	:	Ptr to the facial anim set
/////////////////////////////////////////////////////////////////////////////////
const CActionFacialAnimSet*	CActionManager::FindActionFacialAnimSet( u32& rnIndexOut, const u32 nFacialAnimSetId ) const
{
	const int nFacialAnimCount = m_aActionFacialAnimSets.GetCount();
	for( s32 nFacialAnimIdx = 0; nFacialAnimIdx < nFacialAnimCount; ++nFacialAnimIdx )
	{
		if( nFacialAnimSetId == m_aActionFacialAnimSets[nFacialAnimIdx].GetID() )
		{
			rnIndexOut = nFacialAnimIdx;
			return &m_aActionFacialAnimSets[nFacialAnimIdx];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionDefinition
// PURPOSE	:	Find an action given just an action Id.
//				This uses a simple linear search through the elements.
// PARAMS	:	nActionId - action we are interested in finding.
// RETURNS	:	The address of the action.
/////////////////////////////////////////////////////////////////////////////////
const CActionDefinition* CActionManager::FindActionDefinition(u32& indexOut, const u32 actionDefinitionId) const
{
	// Try to find the action (with a simple linear search).
	const int nActionDefCount = m_aActionDefinitions.GetCount();
	for(s32 i = 0; i < nActionDefCount; ++i)
	{
		if(actionDefinitionId == m_aActionDefinitions[i].GetID())
		{
			indexOut = i;
			return &m_aActionDefinitions[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionBranchSet
// PURPOSE	:	Find an action branch set given an id
// PARAMS	:	actionBranchSetId - action branch set we are interested in finding.
// RETURNS	:	The address of the action branch set.
/////////////////////////////////////////////////////////////////////////////////
const CActionBranchSet*	CActionManager::FindActionBranchSet( u32& rnIndexOut, const u32 actionBranchSetId ) const
{
	const int nActionBranchSetCount = m_aActionBranchSets.GetCount();
	for( s32 i = 0; i < nActionBranchSetCount; ++i )
	{
		if( actionBranchSetId == m_aActionBranchSets[i].GetID() )
		{
			rnIndexOut = i;
			return &m_aActionBranchSets[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionResult
// PURPOSE	:	Find an action result given just an action result Id.
//				This uses a simple linear search through the elements.
// PARAMS	:	actionResultId - action result we are interested in finding.
// RETURNS	:	The address of the action result.
/////////////////////////////////////////////////////////////////////////////////
const CActionResult* CActionManager::FindActionResult(u32& indexOut, const u32 actionResultId) const
{
	// Try to find the actionResult (with a simple linear search).
	const int nActionResCount = m_aActionResults.GetCount();
	for(s32 i = 0; i < nActionResCount; ++i)
	{
		if(actionResultId == m_aActionResults[i].GetID())
		{
			indexOut = i;
			return &m_aActionResults[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindActionResultFromActionId
// PURPOSE	:	Find an action result given just an action Id.  All actions
//				reference exactly one action result so this just involves getting
//				the action result Id from the action and then searching for that.
// PARAMS	:	nActionId - action of the action result we are interested in finding.
// RETURNS	:	The address of the action result.
/////////////////////////////////////////////////////////////////////////////////
const CActionResult* CActionManager::FindActionResultFromActionId(CPed* pPed, const u32 nActionId) const
{
	u32 nIndexFound = 0;
	const CActionDefinition* pAction = ACTIONMGR.FindActionDefinition(nIndexFound, nActionId);
	Assertf( pAction, "pAction was not found from nActionId in CActionManager::FindActionResultFromActionId." );
	return pAction->GetActionResult( pPed );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SelectSuitableStealthKillAction
// PURPOSE	:	Try to find and select a suitable stealth kill action result
// PARAMS	:	pPed - Acting ped that we which to perform the tests with
//				pTargetEntity - Ped that we which to perform the tests against
//				bHasLockOnTarget - Whether or not we are fully locked on
//				pImpulseCombo - Button impulse combo that we would like to use for the tests
//				actionTypeBitSet - Action type flags that we would like to use for the tests
// RETURNS	:	the best fit CActionDefinition given the designated params
/////////////////////////////////////////////////////////////////////////////////
const CActionDefinition* CActionManager::SelectSuitableStealthKillAction( const CPed* pPed,
																		  const CEntity* pTargetEntity,
																		  const bool bHasLockOnTarget,
																		  const CImpulseCombo* pImpulseCombo,
																		  const CActionFlags::ActionTypeBitSetData& typeToLookFor ) const
{
	if( !( pTargetEntity && pTargetEntity->GetIsTypePed() ) )
	{
		return NULL;
	}
	
	if( ((CPed*)pTargetEntity)->GetIsInVehicle() ||
		((CPed*)pTargetEntity)->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COMPLEX_EVASIVE_STEP ) )
	{
		return NULL;
	}

	// Find all suitable actions under the current conditions.
	CActionTestCache reusableResults;

	const int nStealthKillComboCount = m_aStealthKillTests.GetCount();
	for( s32 i = 0; i < nStealthKillComboCount; ++i )
	{
		if( typeToLookFor.IsEqual( m_aStealthKillTests[i].GetActionType() ) )
		{
			u32 nIndexFound = 0;
			const CActionDefinition* pActionDef = ACTIONMGR.FindActionDefinition( nIndexFound, m_aStealthKillTests[i].GetStealthKillActionID() );
			if( pActionDef )
			{
				// Get the pController if this is a player.
				const CControl* pController = NULL;
				if( pPed->IsLocalPlayer() ) 
				{
					pController = pPed->GetControlFromPlayer();
				}

				// Check to see if this stealth action is suitable given the current game state
				if( pActionDef->IsSuitable( typeToLookFor, pImpulseCombo, true, true, pPed, 0, -1,
					pTargetEntity, bHasLockOnTarget, false, false, pController, &reusableResults, ShouldDebugPed( pPed ) && pActionDef->GetIsDebug() ) )
				{
					actionPrintf( pActionDef->GetIsDebug(), "CActionManager::SelectSuitableStealthKillAction - Selected (%s)\n", pActionDef->GetName() );
					return pActionDef;
				}
			}		
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SelectSuitableAction
// PURPOSE	:	Try to find and select a suitable action for a given ped under the given circumstances.
//				This runs through each and every action in a simple linear fashion testing each
//				one for suitability (given the input parameters).
//				The suitability tests are sped up with early outs and an internal caching system.
// PARAMS	:	
// RETURNS	:	the best fit CActionDefinition given the designated params
/////////////////////////////////////////////////////////////////////////////////
const CActionDefinition* CActionManager::SelectSuitableAction(	const CActionFlags::ActionTypeBitSetData& typeToLookFor,
																const CImpulseCombo* pImpulseCombo,
																const bool			bUseAvailabilityFromIdleTest,
																const bool			bDesiredAvailabilityFromIdle,
																const CPed*			pActingPed,
																const u32			nForcingResultId,
																const s32			nPriority,
																const CEntity*		pTargetEntity,
																const bool			bHasLockOnTargetEntity,
																const bool			bImpulseInitiatedMovesOnly,
																const bool			bUseImpulseTest,
																const bool			bDisplayDebug) const
{
	// Get the pController if this is a player.
	CControl* pController = NULL;
	if( pActingPed->IsLocalPlayer() ) 
	{
		pController = const_cast<CPed*>(pActingPed)->GetControlFromPlayer();
	}

	// Find all suitable actions under the current conditions.
	atFixedArray< const CActionDefinition*, MAX_SUITABLE_DEFINITIONS > aSuitableActions;
	CActionTestCache reusableResults;

	const int nActionDefCount = m_aActionDefinitions.GetCount();
	for( s32 i = 0; i < nActionDefCount; ++i )
	{
		bool isSuitable = m_aActionDefinitions[i].IsSuitable( typeToLookFor,
														 	  pImpulseCombo,
															  bUseAvailabilityFromIdleTest,
															  bDesiredAvailabilityFromIdle,
															  pActingPed,
															  nForcingResultId,
															  nPriority,
															  pTargetEntity,
															  bHasLockOnTargetEntity,
															  bImpulseInitiatedMovesOnly,
															  bUseImpulseTest,
															  pController,
															  &reusableResults,
															  bDisplayDebug && m_aActionDefinitions[i].GetIsDebug() );

		if(isSuitable && aSuitableActions.GetCount() < aSuitableActions.GetMaxCount() )
		{
			aSuitableActions.Append() = &m_aActionDefinitions[i];
		}
	}

	return SelectActionFromSet( &aSuitableActions, pActingPed, bDisplayDebug );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsActionSuitable
// PURPOSE	:	Testing a single action for suitability (given the input parameters).
// PARAMS	:	
// RETURNS	:	Whether or not this a designated action passes given the params
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::IsActionSuitable(	const u32			nActionId,
										const CActionFlags::ActionTypeBitSetData& typeToLookFor,
										const CImpulseCombo* pImpulseCombo,
										const bool			bUseAvailabilityFromIdleTest,
										const bool			bDesiredAvailabilityFromIdle,
										const CPed*			pActingPed,
										const u32			nForcingResultId,
										const s32			nPriority,
										const CEntity*		pTargetEntity,
										const bool			bHasLockOnTargetEntity,
										const bool			bImpulseInitiatedMovesOnly,
										const bool			bUseImpulseTest,
										bool				bDisplayDebug ) const
{
	// Try to find the action (with a simple linear search).
	u32 nIndexFound = 0;
	const CActionDefinition* pAction = FindActionDefinition( nIndexFound, nActionId );
	if( !pAction )
	{
		return false;
	}

	// Get the pController if this is a player.
	CControl* pController = NULL;
	if( bUseImpulseTest && pActingPed->IsLocalPlayer() ) 
	{
		pController = const_cast<CPed*>(pActingPed)->GetControlFromPlayer();
	}

	// Find all suitable actions under the current conditions.
	CActionTestCache reusableResults;

	// Debug logic
	bDisplayDebug = bDisplayDebug && pAction->GetIsDebug();

	// Check if it is suitable under the current conditions and let the
	// caller know.
	return pAction->IsSuitable(	typeToLookFor,
								pImpulseCombo,
								bUseAvailabilityFromIdleTest,
								bDesiredAvailabilityFromIdle,
								pActingPed,
								nForcingResultId,
								nPriority,
								pTargetEntity,
								bHasLockOnTargetEntity,
								bImpulseInitiatedMovesOnly,
								bUseImpulseTest,
								pController,
								&reusableResults,
								bDisplayDebug );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CompareActionHistory
// PURPOSE	:	Used as a quick sort function to order the most suitable actions
//				based on history.
// PARAMS	:	infoA - left side info struct ptr
//				infoB
// RETURNS	:	Difference between history values 
/////////////////////////////////////////////////////////////////////////////////
s32 CActionManager::CompareActionHistory( const sSuitableActionInfo* pActionInfoA, const sSuitableActionInfo* pActionInfoB )
{
	return (pActionInfoA->nHistoryIdx - pActionInfoB->nHistoryIdx);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SelectActionFromSet
// PURPOSE	:	Select a single action out of  a set using both absolute priority filtering and biased
//				roulette wheel (random) selection.
// PARAMS	:	paSuitableActionSet - Array of all the suitable actions
// RETURNS	:	The selected CActionDefinition
/////////////////////////////////////////////////////////////////////////////////
const CActionDefinition* CActionManager::SelectActionFromSet( atFixedArray<const CActionDefinition*,MAX_SUITABLE_DEFINITIONS>* paSuitableActionSet, const CPed* pActingPed, const bool BANK_ONLY( bDisplayDebug ) ) const
{
	Assertf( paSuitableActionSet, "paSuitableActionSet was null in CActionManager::SelectActionFromSet.");

	// Store this here for readability and efficiency.
	s32 nSuitableActionCount = paSuitableActionSet->GetCount();

	// Check if no suitable actions have been found, if so let
	// the caller know.
	if( nSuitableActionCount == 0 )
	{
		return NULL;
	}
	
	// Check if there was only one suitable action, if so then
	// just return it.
	if( nSuitableActionCount == 1 )
	{
		actionPrintf( bDisplayDebug, "CActionManager::SelectActionFromSet - Selected %s\n", ((CActionDefinition*)(*paSuitableActionSet)[0])->GetName() );
		return (*paSuitableActionSet)[0];
	}

	// Sort the candidate actions by priority.
	paSuitableActionSet->QSort( 0, -1, CActionManager::CompareActionPriorities );
	
	// Get the highest priority value from candidate actions.
	u32 nHighestPriority = (*paSuitableActionSet)[0]->GetPriority();

	// Remove all the lower priority items from candidate actions.
	s32 i = 0;
	while( i < nSuitableActionCount )
	{
		if( (*paSuitableActionSet)[i]->GetPriority() < nHighestPriority )
		{
			actionPrintf( bDisplayDebug, "CActionManager::SelectActionFromSet - Removed based on priority %s\n", ((CActionDefinition*)(*paSuitableActionSet)[0])->GetName() );
			paSuitableActionSet->Delete(i);
			--nSuitableActionCount;
		}
		else
		{
			++i;
		}			
	}

	// Make sure everything is okay.
	Assertf( nSuitableActionCount == paSuitableActionSet->GetCount(), "Something went wrong in the logic of CActionManager::SelectActionFromSet." );

	// Check if there was only one suitable action after priority culling, if
	// so then just return it.
	if( nSuitableActionCount == 1 )
	{
		actionPrintf( bDisplayDebug, "CActionManager::SelectActionFromSet - Selected %s\n", ((CActionDefinition*)(*paSuitableActionSet)[0])->GetName() );
		return (*paSuitableActionSet)[0];
	}
	
	ms_aSuitableActionInfo.Resize( nSuitableActionCount );

	s32 nSelectedActionIdx = 0;
	bool bFound = false;
	for( u8 i = 0; i < nSuitableActionCount; ++i )
	{
		ms_aSuitableActionInfo[i].Reset();

		bFound = false;
		ms_aSuitableActionInfo[i].fBias = (*paSuitableActionSet)[i]->GetBias();
		ms_aSuitableActionInfo[i].nSelectionIdx = i;

		const CActionResult* pActionResult = (*paSuitableActionSet)[i]->GetActionResult( pActingPed );
		Assert( pActionResult );
		if( pActionResult )
		{
			// find the last occurrence index of the action
			for( u8 j = 0; j < ms_qMeleeActionHistory.GetCount(); j++ )
			{
				// If found influence the bias by the action history
				if( ms_qMeleeActionHistory[j] == pActionResult->GetID() )
				{
					if( !bFound )
					{
						bFound = true;
						ms_aSuitableActionInfo[i].nHistoryIdx = j;
					}
					else
						ms_aSuitableActionInfo[i].nConsecutiveCount++;
				}
				// Otherwise test against consecutive number break
				else if( bFound )
					break;
			}	
		}
	}

	// Sort the suitable actions based on history
	ms_aSuitableActionInfo.QSort( 0, -1, CActionManager::CompareActionHistory );

	// Calculate the overall bias with consideration of the sorted history
	float fRouletteTotal = 0.0f;
	for( s32 i = 0; i < nSuitableActionCount; ++i )
	{
		if( ms_aSuitableActionInfo[i].nHistoryIdx != ACTION_HISTORY_LENGTH )
			ms_aSuitableActionInfo[i].fBiasInfluence = rage::Powf( ( (f32)(i + 1) / (f32)nSuitableActionCount ), (f32)ms_aSuitableActionInfo[i].nConsecutiveCount );

		ms_aSuitableActionInfo[i].fBias *= ms_aSuitableActionInfo[i].fBiasInfluence;
		fRouletteTotal += ms_aSuitableActionInfo[i].fBias;
	}

	// Determine where on the roulette wheel we want to stop.
	float fRouletteSelectionValue = fwRandom::GetRandomNumberInRange( 0.0f, fRouletteTotal );

	// Determine the action at that point.
	float fTotalSoFar = 0.0f;
	for( s32 i = 0; fTotalSoFar < fRouletteSelectionValue; ++i )
	{
		fTotalSoFar += ms_aSuitableActionInfo[i].fBias;
		nSelectedActionIdx = i;
	}

	actionPrintf( bDisplayDebug, "CActionManager::SelectActionFromSet - Selected %s\n", ((CActionDefinition*)(*paSuitableActionSet)[nSelectedActionIdx])->GetName() );

	// Return the selected action.
	return (*paSuitableActionSet)[ ms_aSuitableActionInfo[ nSelectedActionIdx ].nSelectionIdx ];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CompareActionPriorities
// PURPOSE	:	Compare two actions priorities. This is used for sorting and filtering sets of actions.
// PARAMS	:	a1 - First CActionDefinition
//				a2 - Second CActionDefinition
// RETURNS	:	Int that will determine which action to choose
/////////////////////////////////////////////////////////////////////////////////
int CActionManager::CompareActionPriorities(const CActionDefinition* const* a1, const CActionDefinition* const* a2)
{
	return (static_cast<s32>((*a2)->GetPriority()) - static_cast<s32>((*a1)->GetPriority()));
}	

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	TargetEntityTypeDoesCatchEntity
// PURPOSE	:	Determines whether or not the designated target flags are suitable with 
//				the given entity.
// PARAMS	:	targetEntityTypeBitSet - Flags we will use to compare against the entity
//				pEntity = Target entity to compare against
// RETURNS	:	Whether or not this entity is suitable given the the target flags
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::TargetEntityTypeDoesCatchEntity( const CActionFlags::TargetEntityTypeBitSetData targetEntityTypeBitSet, const CEntity* pEntity )
{
	if( !pEntity )
		return false;

	if( !targetEntityTypeBitSet.AreAnySet() )
		return true;

	if( targetEntityTypeBitSet.IsSet( CActionFlags::TET_ANY ) )
		return true;

	bool bBiped = targetEntityTypeBitSet.IsSet( CActionFlags::TET_BIPED );
	bool bQuadruped = targetEntityTypeBitSet.IsSet( CActionFlags::TET_QUADRUPED );
	if( ( bBiped || bQuadruped ) && pEntity->GetIsTypePed() )
	{
		const CPed* pPed = static_cast<const CPed*>(pEntity);
		const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();

		// Biped test
		if( bBiped && pCapsuleInfo && pCapsuleInfo->IsBiped() )
			return true;

		// Quadruped test
		if( bQuadruped && pCapsuleInfo && pCapsuleInfo->IsQuadruped() )
			return true;
	}

	// Only accept surfaces when no entity is passed in
	if( targetEntityTypeBitSet.IsSet( CActionFlags::TET_SURFACE ) && ( pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeObject() || pEntity->GetIsTypeVehicle() ) )
		return true;
	
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	TargetTypeDoesCatchEntity
// PURPOSE	:	Determine if a given entity is caught with a given CActionFlags::TargetType.
//				This is used in several of the action suitability and homing tests.
// PARAMS	:	targetType - Type we will use to compare against the entity
//				pEntity = Entity to compare against
// RETURNS	:	Whether or not this entity is suitable given the the target type
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::TargetTypeDoesCatchEntity( const CActionFlags::TargetType targetType, const CEntity* pEntity )
{	
	switch( targetType )
	{
		case CActionFlags::TT_ENTITY_ROOT:					return pEntity != NULL;
		case CActionFlags::TT_ENTITY_ROOT_PREDICTED:		return pEntity ? pEntity->GetIsPhysical() : false;
		case CActionFlags::TT_ENTITY_CLOSEST_BOX_POINT:		return pEntity != NULL;
		case CActionFlags::TT_SURFACE_PROBE:				return true;
		case CActionFlags::TT_PED_ROOT:						return pEntity ? pEntity->GetIsTypePed() : false;
		case CActionFlags::TT_PED_NECK:						return pEntity ? pEntity->GetIsTypePed() : false;
		case CActionFlags::TT_PED_CHEST:					return pEntity ? pEntity->GetIsTypePed() : false;
		case CActionFlags::TT_PED_KNEE_LEFT:				return pEntity ? pEntity->GetIsTypePed() : false;
		case CActionFlags::TT_PED_KNEE_RIGHT:				return pEntity ? pEntity->GetIsTypePed() : false;
		case CActionFlags::TT_PED_CLOSEST_BONE:				return pEntity ? pEntity->GetIsTypePed() : false;
		default:											return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PlayerAiFilterTypeDoesCatchPed
// PURPOSE	:	Determine if a given ped is caught with a given CActionFlags::PlayerAiFilterType.
//				This is used in several of the action suitability tests.
// PARAMS	:	filterTypeBitSet - Filter flags we will use to compare against the ped
//				pPed = Ped to compare against
// RETURNS	:	Whether or not this entity is suitable given the the target type
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::PlayerAiFilterTypeDoesCatchPed( const CActionFlags::PlayerAiFilterTypeBitSetData filterTypeBitSet, const CPed* pPed )
{
	if( !pPed )
		return false;

	if( !filterTypeBitSet.AreAnySet() )
		return true;

	if( filterTypeBitSet.IsSet( CActionFlags::PAFT_NOT_ANIMAL ) &&
		pPed->GetPedType() == PEDTYPE_ANIMAL )
		return false;

	if ( filterTypeBitSet.IsSet( CActionFlags::PAFT_NOT_FISH ) )
	{
		const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
		if ( pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH )
		{
			return false;
		}
	}

	if( filterTypeBitSet.IsSet( CActionFlags::PAFT_ANY ) )
		return true;

#if __BANK
	TUNE_GROUP_BOOL(MELEE_TEST, bTreatEveryoneAsMPPlayer, false);
	if(bTreatEveryoneAsMPPlayer)
	{
		if( filterTypeBitSet.IsSet( CActionFlags::PAFT_PLAYER_MP ) )
			return true;
	}

	TUNE_GROUP_BOOL(MELEE_TEST, bTreatEveryoneAsSPPlayer, false);
	if(bTreatEveryoneAsSPPlayer)
	{
		if( filterTypeBitSet.IsSet( CActionFlags::PAFT_PLAYER_SP ) )
			return true;
	}
#endif

	if( NetworkInterface::IsGameInProgress() )
	{
		if( filterTypeBitSet.IsSet( CActionFlags::PAFT_PLAYER_MP ) &&
			pPed->IsPlayer() )
			return true;
	}
	else
	{
		if( filterTypeBitSet.IsSet( CActionFlags::PAFT_PLAYER_SP ) &&
			pPed->IsPlayer() )
			return true;
	}

	if( filterTypeBitSet.IsSet( CActionFlags::PAFT_AI ) &&
		!pPed->IsPlayer() )
		return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetClosestPointAndDirectionOnBox
// PURPOSE	:	Given a point in space and an oriented box find the closest point on that
//				box to the point and direction to the closest point.
// PARAMS	:	
// RETURNS	:	Closest point on a bounding box and the direction towards that position
/////////////////////////////////////////////////////////////////////////////////
Vec3V_Out CActionManager::GetClosestPointOnBox( const phInst*	pInst,
												Vec3V_In		v3TestPos,
												bool&			bContainsPoint )
{
	Vec3V v3Position( v3TestPos );
	if( pInst && pInst->GetClassType() >= fragInst::PH_FRAG_INST )
	{
		const fragInst* pfragInst = static_cast<const fragInst*>(pInst);
		const fragPhysicsLOD* pFragPhys = pfragInst->GetTypePhysics(); 
		const phArchetype* pArch = pFragPhys ? pFragPhys->GetArchetype() : NULL;
		const phBound* pBound = pArch ? pArch->GetBound() : NULL;
		if( pBound )
		{
			Vec3V vHalfWidth;
			Vec3V vCenter;
			pBound->GetBoundingBoxHalfWidthAndCenter( vHalfWidth, vCenter );
			Mat34V_ConstRef temp = pInst->GetMatrix();
			Mat34V current( temp );
			current.SetCol3( current.GetCol3() + vCenter );
			Vec3V vboxspacepos = UnTransformFull( current, v3TestPos );
			Vec3V vPos = Clamp( vboxspacepos, -vHalfWidth, vHalfWidth );
			Vec3V vDiff = vboxspacepos - vPos;
			float fDiffSquared = MagSquared( vDiff ).Getf();
			bContainsPoint = fDiffSquared < rage::square( VERY_SMALL_FLOAT );
			v3Position = Transform( current, vPos );
			v3Position.SetZ( v3TestPos.GetZ() );
		}
	}

	return v3Position;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ArePedsFriendlyWithEachOther
// PURPOSE	:	Compares to peds to determine if they are fighting for the same cause
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::ArePedsFriendlyWithEachOther( const CPed* pPedInflictor, const CPed* pPedTarget )
{
	//! NOTE: PED B must be target.

	taskAssertf( pPedInflictor, "CActionManager::ArePedsFriendlyWithEachOther - Ped A is invalid" );
	taskAssertf( pPedInflictor->GetPedIntelligence(), "CActionManager::ArePedsFriendlyWithEachOther - Ped A has no ped intelligence" );
	taskAssertf( pPedTarget, "CActionManager::ArePedsFriendlyWithEachOther - Ped B is invalid" );
	taskAssertf( pPedTarget->GetPedIntelligence(), "CActionManager::ArePedsFriendlyWithEachOther - Ped B has no ped intelligence" );

	// Same ped is always fighting for the same cause
	if( pPedInflictor == pPedTarget )
		return true;

	bool bIsFriendly = false;
	if( NetworkInterface::IsGameInProgress() && pPedInflictor->IsAPlayerPed() && pPedTarget->IsAPlayerPed() )
	{
		bool bCanDamage;
		//! DMKH. Check passive flags 1st. 
		if( !NetworkInterface::IsFriendlyFireAllowed( pPedTarget, pPedInflictor ) )
		{
			bCanDamage = false; 
		}
		else
		{
			bCanDamage = ( NetworkInterface::FriendlyFireAllowed() || NetworkInterface::IsPedAllowedToDamagePed( *pPedInflictor, *pPedTarget ) );
		}

		bIsFriendly = !bCanDamage;
	}
	// if either ped is a random ped, just ignore the CPedIntelligence::IsFriendly query
	else if( !pPedInflictor->PopTypeIsRandom() && !pPedTarget->PopTypeIsRandom() )
		bIsFriendly = !pPedInflictor->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers) && pPedInflictor->GetPedIntelligence()->IsFriendlyWith( *pPedTarget );

	if( !bIsFriendly )
	{
		//! Don't treat players as friendly as it may mean we get stuck without a move when involved in a big stramash.
		if(!pPedInflictor->IsLocalPlayer())
		{
#if __BANK
			TUNE_GROUP_BOOL(MELEE_TEST, bForceShareSameTarget, false);
			if(bForceShareSameTarget)
			{
				bIsFriendly = true;
			}
#endif

			// Check to see if the swinging ped and hit ped have the same target
			CTaskMelee* pOpponentTaskMelee = pPedTarget->GetPedIntelligence()->GetTaskMelee();
			CEntity* pOpponentMeleeTarget = pOpponentTaskMelee ? pOpponentTaskMelee->GetTargetEntity() : NULL;
			if( pOpponentMeleeTarget )
			{
				CTaskMelee* pTaskMelee = pPedInflictor->GetPedIntelligence()->GetTaskMelee();
				CEntity* pMeleeTarget = pTaskMelee ? pTaskMelee->GetTargetEntity() : NULL;
				if( pMeleeTarget && pOpponentMeleeTarget == pMeleeTarget )
					bIsFriendly = true;
			}
		}
	}

	return bIsFriendly;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RecordAction
// PURPOSE	:	Function that will store the order of the most recent actions
// PARAMS	:	uActionToRecord = Id of the CActionResult we would like store
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::RecordAction( u32 uActionToRecord )
{
	if( ms_qMeleeActionHistory.IsFull() )
	{
		ms_qMeleeActionHistory.PopEnd();
	}

	ms_qMeleeActionHistory.PushTop( uActionToRecord );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldDebugPed
// PURPOSE	:	Function that will determine if we should debug the designated ped
// PARAMS	:	pPed = Ped we are asking about
// RETURNS	:	Whether or not we should debug the designated ped
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::ShouldDebugPed( const CPed* BANK_ONLY( pPed ) )
{
#if __BANK
	CPed* pFocusPed = CPedDebugVisualiserMenu::GetFocusPed();
	if( ms_DebugFocusPed && ( ( !pFocusPed && pPed->IsLocalPlayer() ) || ( pFocusPed && pPed == pFocusPed ) ) )
		return true;
#endif

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetEntityGroundHeight
// PURPOSE	:	Helper function to calculate the ground height of the entity 
/////////////////////////////////////////////////////////////////////////////////
float CActionManager::GetEntityGroundHeight( const CEntity* pEntity, Vec3V_In vProbePosition, float fProbeZOffset )
{
	Assertf( pEntity, "CRelativeRange::GetEntityGroundHeight - Invalid entity!" );
	Assertf( !IsZeroAll( vProbePosition ), "CRelativeRange::GetEntityGroundHeight - Probe position is ZERO!" );
	Assertf( fProbeZOffset != 0.0f, "CRelativeRange::GetEntityGroundHeight - Probe z offset 0.0f!" );

	float fGroundHeight = 0.0f;
	if( pEntity )
	{
		if( pEntity->GetIsTypePed() )
		{
			fGroundHeight = static_cast<const CPed*>(pEntity)->GetGroundPos().z;
			if( fGroundHeight == PED_GROUNDPOS_RESET_Z )
				fGroundHeight = -1.0f;
		}

		if( fGroundHeight == -1.0f )
		{
			phIntersection testIntersections[4];
			phSegment testSegment;
			testSegment.Set( VEC3V_TO_VECTOR3( vProbePosition ), VEC3V_TO_VECTOR3( vProbePosition + Vec3V( 0.0f, 0.0f, fProbeZOffset ) ) );
			u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
			int nNumResults = CPhysics::GetLevel()->TestProbe( testSegment, testIntersections, pEntity->GetCurrentPhysicsInst(), nFlags, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, 1 );
			if( nNumResults > 0 )
				fGroundHeight = testIntersections[0].GetPosition().GetZf();
			else 
				fGroundHeight = vProbePosition.GetZf();
		}

		fGroundHeight = vProbePosition.GetZf() - fGroundHeight;
	}

	return fGroundHeight;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CalculateDesiredHeading
// PURPOSE	:	Helper function to form the desired heading
/////////////////////////////////////////////////////////////////////////////////
float CActionManager::CalculateDesiredHeading( const CPed* pPed )
{
	// By default use the stick directions
	float fDesiredHeading = 0.0f;

	bool bIsAiming = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsAimingGun ) || pPed->GetPedResetFlag( CPED_RESET_FLAG_IsAiming );
#if FPS_MODE_SUPPORTED
	if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())
	{
		bIsAiming = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->IsAiming();
	}
#endif
	
	if( bIsAiming )
	{
		fDesiredHeading = fwAngle::LimitRadianAngleSafe( pPed->GetCurrentHeading() );
	}
	else
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if( pControl && !CControlMgr::IsDisabledControl( const_cast<CControl*>(pControl) ) )
		{
			Vector3 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f );
			const float fInputMag = vecStickInput.Mag2();
			if( fInputMag > rage::square( 0.5f ) )
			{
				float fStickDesiredHeading = rage::Atan2f( -vecStickInput.x, vecStickInput.y );
				fStickDesiredHeading += camInterface::GetHeading();
				fDesiredHeading = fwAngle::LimitRadianAngle( fStickDesiredHeading );
			}
			else
			{
				fDesiredHeading = fwAngle::LimitRadianAngleSafe( pPed->GetDesiredHeading() );
			}
		}
		// Otherwise construct the direction from the desired heading
		else
		{
			fDesiredHeading = fwAngle::LimitRadianAngleSafe( pPed->GetDesiredHeading() );
		}
	}

	return fDesiredHeading;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsPedTypeWithinAngularThreshold
// PURPOSE	:	Given the designated thresholds, is there a ped in the general direction?
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::IsPedTypeWithinAngularThreshold( const CPed* pTestPed, 
													  Vec3V_In vTestOffset, 
													  const CEntity* pTargetEntity,
													  const float fMinDistThresholdSq, 
													  const float fMaxDistThresholdSq, 
													  const float fMinDotThreshold,
													  const float fMaxDotThreshold, 
													  const float fLookAheadTime, 
													  bool bFriendlyCheck, 
													  bool bOnlyCheckAi )
{
	float fDesiredHeading = fwAngle::LimitRadianAngleSafe( pTestPed->GetDesiredHeading() );
	Vec3V vTestDirection( -rage::Sinf( fDesiredHeading ), rage::Cosf( fDesiredHeading ), 0.0f );

	Vec3V vPedPosition = pTestPed->GetTransform().Transform3x3( vTestOffset );
	vPedPosition += pTestPed->GetTransform().GetPosition();

	Vec3V vFuturePosition( V_ZERO );
	Vec3V v3ViewerToNearby( V_ZERO );

	float fDistanceSqToTargetEntity = 0.0f;
	if( pTargetEntity )
	{
		fDistanceSqToTargetEntity = DistSquared( vPedPosition, pTargetEntity->GetTransform().GetPosition() ).Getf();
	}

#if __BANK
	TUNE_GROUP_BOOL(MELEE_TEST, bForcePedInAngThreshold, false);
	if(bForcePedInAngThreshold)
	{
		return true;
	}
#endif

	// Get the list of the near enemies.
	CEntityScannerIterator entityList = pTestPed->GetPedIntelligence()->GetNearbyPeds();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		if( pNearbyPed && pNearbyPed != pTargetEntity && ( !bOnlyCheckAi || !pNearbyPed->IsPlayer() ) )
		{
			// Grab the distSq to the nearby ped
			vFuturePosition = pNearbyPed->GetTransform().GetPosition() + Scale( RCC_VEC3V( pNearbyPed->GetVelocity() ), ScalarV( fLookAheadTime ) );				
			v3ViewerToNearby = vFuturePosition - vPedPosition;
			float fViewerToNearbyDistSq = MagSquared( v3ViewerToNearby ).Getf();
			if( fViewerToNearbyDistSq < fMaxDistThresholdSq &&
				( ( fDistanceSqToTargetEntity < SMALL_FLOAT ) || ( fViewerToNearbyDistSq < fDistanceSqToTargetEntity ) ) )
			{
				const float fLerpRatio = Clamp( ( fMaxDistThresholdSq - fViewerToNearbyDistSq ) / ( fMaxDistThresholdSq - fMinDistThresholdSq ), 0.0f, 1.0f); 
				float fDotThreshold = Lerp( fLerpRatio, fMaxDotThreshold, fMinDotThreshold );
				
				// Grab the direction to the nearby ped
				v3ViewerToNearby = Normalize( v3ViewerToNearby );

				// Construct the direction from the desired heading
				float fViewerToNearbyDir = Dot( v3ViewerToNearby, vTestDirection ).Getf();
				if( fViewerToNearbyDir > fDotThreshold )
				{
					if( !bFriendlyCheck || CActionManager::ArePedsFriendlyWithEachOther( pTestPed, pNearbyPed ) )
						return true;
				}
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	EnableAction
// PURPOSE	:	Function that will toggle the enable status of an ActionDefinition
// PARAMS	:	uActionToEnable = Id of the CActionDefinition we would like enable
//				bEnable - Whether or not we want to enable/disable 
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::EnableAction( u32 uActionToEnable, bool bEnable )
{
	u32 uActionIdx = 0;
	const CActionDefinition* pAction = FindActionDefinition( uActionIdx, uActionToEnable );
	if( pAction )
	{
		m_aActionDefinitions[ uActionIdx ].SetIsEnabled( bEnable );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsActionEnabled
// PURPOSE	:	Returns whether or not the designated action is currently ENABLED
// PARAMS	:	uActionId = Id of the CActionDefinition we would like to query
// RETURNS	:   bool - whether or not the designated action is currently ENABLED
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::IsActionEnabled( u32 uActionId )
{
	u32 uActionIdx = 0;
	const CActionDefinition* pAction = FindActionDefinition( uActionIdx, uActionId );
	if( pAction )
	{
		return pAction->GetIsEnabled();
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	TargetTypeGetPosDirFromEntity
// PURPOSE	:	Given a entity and a desired target type try to find the closest point and normal
//				on the entity for the target type.
// PARAMS	:	
// RETURNS	:	Whether or not we successfully gathered the information
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::TargetTypeGetPosDirFromEntity(	Vec3V_InOut			rv3Position,
												    Vec3V_InOut			rv3Direction,
													const CActionFlags::TargetType	targetType,
													const CEntity*		pEntity,
													const CPed*			pViewerPed,
													Vec3V_In			v3ViewerPos,
													float				fTimeToReachEntity,
													int&				nBoneIdxCache,
													float				fMaxProbeDistance,
													float				fProbeRootOffsetZ,
													bool*				pbFoundBreakableSurface )
{
	// Make sure the entity is the target type we expected.
	if( !TargetTypeDoesCatchEntity( targetType, pEntity ) )
		return false;

	// Try to find the position and facing dir for the target type.
	switch( targetType )
	{
		// Handle the entity cases.
		case CActionFlags::TT_ENTITY_ROOT:
		{
			rv3Position = pEntity->GetTransform().GetPosition();
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_ENTITY_ROOT_PREDICTED:
		{
			rv3Position = CalculatePredictedTargetPosition( pViewerPed, pEntity, fTimeToReachEntity, BONETAG_ROOT );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_ENTITY_CLOSEST_BOX_POINT:
		{
			bool bContainsPoint = false;
			rv3Position = GetClosestPointOnBox( pEntity->GetCurrentPhysicsInst(), v3ViewerPos, bContainsPoint );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;			
		}	
		case CActionFlags::TT_PED_ROOT:
		{
			Assert( pEntity->GetIsTypePed() );
			rv3Position = ((CPed*)pEntity)->GetBonePositionCachedV( BONETAG_SPINE1 );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_PED_NECK:
		{
			Assert( pEntity->GetIsTypePed() );
			rv3Position = ((CPed*)pEntity)->GetBonePositionCachedV( BONETAG_NECK );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_PED_CHEST:
		{
			Assert( pEntity->GetIsTypePed() );
			((CPed*)pEntity)->GetBonePositionVec3V( rv3Position, BONETAG_SPINE3 );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_PED_KNEE_LEFT:
		{
			Assert( pEntity->GetIsTypePed() );
			((CPed*)pEntity)->GetBonePositionVec3V( rv3Position, BONETAG_L_CALF );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_PED_KNEE_RIGHT:
		{
			Assert( pEntity->GetIsTypePed() );
			((CPed*)pEntity)->GetBonePositionVec3V( rv3Position, BONETAG_R_CALF );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_PED_CLOSEST_BONE:
		{
			Assert( pEntity->GetIsTypePed() );

			// B*1881170: Melee attacks against fleeing cats target bone at end of tail, so tend to miss. Override and do target towards TT_PED_ROOT instead.
			if (((CPed*)pEntity)->GetPedModelInfo()->GetHashKey() == ATSTRINGHASH("A_C_CAT_01", 0x573201B8))
			{
				rv3Position = ((CPed*)pEntity)->GetBonePositionCachedV( BONETAG_SPINE1 );
				rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
				return true;
			}

			const int nNumBones = 3;
			eAnimBoneTag aBoneTag[ nNumBones ] = { BONETAG_SPINE3, BONETAG_HEAD, BONETAG_PELVIS };
			if( nBoneIdxCache == -1 )
			{
				Vector3 vPedPosition = VEC3V_TO_VECTOR3( pViewerPed->GetTransform().GetPosition());
				Vector3 vBonePos;
				float fBoneDist2 = 0.0f;
				float fBestBoneDist2 = 10000.0f;
				for( int i = 0; i < nNumBones; i++ )
				{
					((CPed*)pEntity)->GetBonePosition( vBonePos, aBoneTag[ i ] );
					fBoneDist2 = vPedPosition.Dist2( vBonePos );
					if( fBoneDist2 < fBestBoneDist2 )
					{
						fBestBoneDist2 = fBoneDist2;
						nBoneIdxCache = i;
					}
				}
			}

			((CPed*)pEntity)->GetBonePositionVec3V( rv3Position, aBoneTag[ nBoneIdxCache ] );
			rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
			return true;
		}
		case CActionFlags::TT_SURFACE_PROBE:
		{
			rv3Position = pViewerPed->GetTransform().Transform3x3( Vec3V( 0.0f, 0.0f, fProbeRootOffsetZ ) );
			rv3Position += pViewerPed->GetTransform().GetPosition();
			
			// Construct the direction from the desired heading
			float fDesiredHeading = fwAngle::LimitRadianAngleSafe( pViewerPed->GetDesiredHeading() );
			rv3Direction.SetXf( -rage::Sinf( fDesiredHeading ) );
			rv3Direction.SetYf( rage::Cosf( fDesiredHeading ) );

			Vec3V vEndPos( VEC3_ZERO ); 
			vEndPos = rv3Position + Scale( rv3Direction, ScalarV( fMaxProbeDistance ) );

			const s32 MAX_INTERSECTIONS = 8;
			WorldProbe::CShapeTestFixedResults<MAX_INTERSECTIONS> probeTestResults;
			WorldProbe::CShapeTestCapsuleDesc probeTestDesc;

			static dev_float CAPSULE_RADIUS = 0.15f;
			probeTestDesc.SetResultsStructure( &probeTestResults );
			probeTestDesc.SetCapsule( VEC3V_TO_VECTOR3( rv3Position ), VEC3V_TO_VECTOR3( vEndPos ), CAPSULE_RADIUS );
			probeTestDesc.SetIncludeFlags( ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE );
			probeTestDesc.SetExcludeTypeFlags( ArchetypeFlags::GTA_PICKUP_TYPE | ArchetypeFlags::GTA_PROJECTILE_TYPE );
			probeTestDesc.SetContext( WorldProbe::LOS_CombatAI );
			probeTestDesc.SetIsDirected( true );

#if DEBUG_DRAW
			char szText[128];
			static dev_bool bRenderSurfaceCapsuleTest = false;
			if( bRenderSurfaceCapsuleTest )
				CCombatMeleeDebug::sm_debugDraw.AddCapsule( rv3Position, vEndPos, CAPSULE_RADIUS, Color_green, 250, atStringHash( "SurfaceCapsuleTest" ), false );
#endif

			if( WorldProbe::GetShapeTestManager()->SubmitTest( probeTestDesc ) )
			{
				int iResultIndex = 0;
				static dev_float sfSurfaceHitDotThreshold = -0.5f;
				CEntity* pHitEntity = NULL;
				Vec3V vHitNormal( VEC3_ZERO );
				for( WorldProbe::ResultIterator it = probeTestResults.begin(); it < probeTestResults.last_result(); ++it, ++iResultIndex )
				{
					if( PGTAMATERIALMGR->UnpackMtlId( it->GetHitMaterialId() ) == PGTAMATERIALMGR->g_idCarVoid )
						continue;

					if( PGTAMATERIALMGR->GetPolyFlagStairs( it->GetHitMaterialId() ) )
						continue;
					
					pHitEntity = CPhysics::GetEntityFromInst( it->GetHitInst() );
					if( pHitEntity )
					{
						if( pHitEntity->GetIsTypeVehicle() )
						{
							// Do not allow targeting moving vehicles
							float fVelocityMagSq = static_cast<CVehicle*>(pHitEntity)->GetVelocity().Mag2();
							if( fVelocityMagSq > 0.25f )
								continue;

							if(NetworkInterface::IsAGhostVehicle(*static_cast<CVehicle*>(pHitEntity), false))
							{
								continue;
							}
						}
						else if( pHitEntity->GetAttachParent() && static_cast<CEntity*>(pHitEntity->GetAttachParent())->GetIsTypePed() )
							continue;
					}

					// Do not allow surfaces that are in the same direction of the attack
					if( Dot( rv3Direction, it->GetHitNormalV() ).Getf() > sfSurfaceHitDotThreshold )
						continue;
#if DEBUG_DRAW
					if( bRenderSurfaceCapsuleTest )
					{
						formatf( szText, "SURFACE_COLLISION_CAPSULE_%u", iResultIndex );
						CCombatMeleeDebug::sm_debugDraw.AddSphere( it->GetHitPositionV(), 0.05f, Color_red, atStringHash( szText ), true );
					}
#endif
					// Cache off whether or not this surface is breakable
					if( pbFoundBreakableSurface && pHitEntity->GetIsTypeObject() )
					{
						if( IsFragInst( it->GetHitInst() ) && it->GetHitInst()->IsBreakable( NULL ) )
							*pbFoundBreakableSurface = true;
					}

					rv3Position = it->GetHitPositionV();
					rv3Direction = NormalizeSafe( rv3Position - v3ViewerPos, pViewerPed->GetTransform().GetB() );
					return true;	
				}
			}

			return false;
		}
		// We have encountered an unknown case.
		default:
			return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetMatrixFromBoneTag
// PURPOSE	:	Helper function that will get the matrix given a designated bone tag
// PARAMS	:	boneMtx - Matrix that will be returned
//				pEntity - Entity you are querying for
//				eBoneTag - Bone tag you are querying for
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::GetMatrixFromBoneTag( Mat34V_InOut boneMtx, const CEntity* pEntity, eAnimBoneTag eBoneTag )
{
	if( pEntity->GetIsDynamic() )
	{
		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);

		// Grab the skeleton data so we can convert the bone tag to an index
		const crSkeletonData* pSkeletonData = NULL;
		const crSkeleton* pSkeleton = pDynamicEntity->GetSkeleton();
		if( pSkeleton )
			pSkeletonData = &pSkeleton->GetSkeletonData();
		else if( pDynamicEntity->GetFragInst() )
			pSkeletonData = &pDynamicEntity->GetFragInst()->GetType()->GetSkeletonData();

		if( pSkeletonData )
		{
			int nBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag( *pSkeletonData, eBoneTag );
			if( nBoneIndex > -1 )
			{
				if( pDynamicEntity->GetIsTypeVehicle() )
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(pDynamicEntity);
					if( pVehicle->GetExhaustMatrixFromBoneIndex( boneMtx, nBoneIndex ) )
						return;
				}
				else if( pSkeleton )
				{
					pSkeleton->GetGlobalMtx( nBoneIndex, boneMtx );
					return;
				}
				else if( pDynamicEntity->GetFragInst() )
				{
					Mat34V objectMtx;
					pSkeletonData->ComputeObjectTransform( nBoneIndex, objectMtx );
					Transform( boneMtx, pEntity->GetMatrix(), objectMtx );
					return;
				}
			}
		}
	}

	boneMtx = pEntity->GetMatrix();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CalculatePredictedTargetPosition
// PURPOSE	:	Calculate the predicted position of the target entity based off the
//				distance from that entity
// PARAMS	:	pPed - Ped which we would like to predict the position from
//				pTargetEntity - Entity which we would like to predict the position
//				fTimeToReachEntity - Amount of time before we reach the target entity
//				eBoneTag - Bone tag we would like to predict from
// RETURNS	:	Predicted position based on distance from the entity
/////////////////////////////////////////////////////////////////////////////////
Vec3V_Out CActionManager::CalculatePredictedTargetPosition( const CPed* DEV_ONLY( pPed ), const CEntity* pTargetEntity, float fTimeToReachEntity, eAnimBoneTag eBoneTag )
{
	Assert( pTargetEntity );

	// First grab the correct entity matrix
	Mat34V entityMtx;
	GetMatrixFromBoneTag( entityMtx, pTargetEntity, eBoneTag );

	Vec3V vPredictedPosition( entityMtx.GetCol3() );
	if( fTimeToReachEntity > 0.0f && pTargetEntity->GetIsPhysical() )
	{
		const CPhysical* pPhysical = static_cast<const CPhysical*>( pTargetEntity );

		// If the target is running the melee task, don't predict their velocity, as this produces weird results
		bool bTargetMeleeing = false;
		if( pTargetEntity->GetIsTypePed() )
		{
			const CPed* pTargetPed = static_cast<const CPed*>( pPhysical );

			const CPedIntelligence* pIntelligence = pTargetPed->GetPedIntelligence();
			if( pIntelligence )
			{
				const CQueriableInterface* pQueriableInterface = pIntelligence->GetQueriableInterface();
				if( pQueriableInterface && pQueriableInterface->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE, true ) )
				{
					bTargetMeleeing = true;
				}
			}
		}

		if( !bTargetMeleeing )
		{
			// Calculate how far ahead we need to be in order to reach the object at time 0
			Vec3V vVelocity = VECTOR3_TO_VEC3V( pPhysical->GetVelocity() );
			Vec3V vDir = Scale( vVelocity, ScalarV( fTimeToReachEntity ) );

			// Add the current position plus the predicted direction
			vPredictedPosition = entityMtx.GetCol3() + vDir;
		}
	}

	// Add in head offset for peds
	if( pTargetEntity->GetIsTypePed() )
	{
		const CPed* pTargetPed = static_cast<const CPed*>( pTargetEntity );
		Vec3V vHeadPosition = pTargetPed->GetBonePositionCachedV( BONETAG_NECK );
		Vec3V vRootToHeadDiff = vHeadPosition - pTargetPed->GetTransform().GetPosition();
		vRootToHeadDiff.SetZf( 0.0f );
		vPredictedPosition += vRootToHeadDiff;
	}

#if __DEV
	if( CCombatMeleeDebug::sm_bDrawHomingDesiredPosDir )
	{
		if( (CCombatMeleeDebug::sm_bVisualisePlayer && pPed->IsLocalPlayer()) ||
			(CCombatMeleeDebug::sm_bVisualiseNPCs && !pPed->IsLocalPlayer()) )
		{
			CCombatMeleeDebug::sm_debugDraw.AddSphere( vPredictedPosition, 0.1f, Color_black, 1 );
		}
	}
#endif

	return vPredictedPosition;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WithinMovementSpeed
// PURPOSE	:	Determines whether or not the designated ped qualifies for any of
//				the given movement flags.
// PARAMS	:	pPed - Acting ped we would like to test
//				movementSpeed - movement speed flags we will use to test against the acting ped
// RETURNS	:	Whether or not the ped qualifies
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::WithinMovementSpeed( const CPed* pPed, const CActionFlags::MovementSpeedBitSetData movementSpeed )
{
	Assert( pPed );
	Assert( pPed->GetPedIntelligence() );

	if( !movementSpeed.AreAnySet() )
		return true;

	if( movementSpeed.IsSet( CActionFlags::MS_ANY )  )
		return true;

	bool bIsFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer( false );

	bool bIsStrafing = pPed->GetPedIntelligence()->FindTaskActiveMotionByType( CTaskTypes::TASK_MOTION_STRAFING ) != NULL
#if FPS_MODE_SUPPORTED
		|| (bIsFPSMode && pPed->GetPedIntelligence()->FindTaskActiveMotionByType( CTaskTypes::TASK_MOTION_AIMING ) != NULL ); 
#endif
	
	//! Clones don't run strafing and melee in sync. Ensure clones are also running melee tasks to be considered as strafing.
	if(pPed->IsNetworkClone() && bIsStrafing)
	{
		if(!pPed->GetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning ))
		{
			bIsStrafing = false;
		}
	}

	if( bIsStrafing )
	{
		if( movementSpeed.IsSet( CActionFlags::MS_STRAFING ) )
			return true;
	}
	else
	{
		// Local walk boundary since MBR_WALK_BOUNDARY is too small
		float fMoveBlendRatioSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
		if( movementSpeed.IsSet( CActionFlags::MS_STILL ) && 
			fMoveBlendRatioSq < rage::square( ms_fMBRWalkBoundary ) )
			return true;

		if( movementSpeed.IsSet( CActionFlags::MS_WALKING ) && 
			( fMoveBlendRatioSq < rage::square( MBR_RUN_BOUNDARY ) 
			// In first person we move at a run speed by default when strafing so consider walking actions valid as long as we're not sprinting
			FPS_MODE_SUPPORTED_ONLY( || ( bIsFPSMode && fMoveBlendRatioSq < rage::square( MBR_SPRINT_BOUNDARY ) ) ) ) &&
			fMoveBlendRatioSq >= rage::square( ms_fMBRWalkBoundary ) )
			return true;

		if( movementSpeed.IsSet( CActionFlags::MS_RUNNING ) && 
			fMoveBlendRatioSq >= rage::square( MBR_RUN_BOUNDARY ) )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WithinDesiredDirection
// PURPOSE	:	Determines whether or not the designated ped qualifies for any of
//				the given directions.
// PARAMS	:	pPed - Acting ped we would like to test
//				desiredDirection - flags of which directions we should test against
// RETURNS	:	Whether or not the ped qualifies
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::WithinDesiredDirection( const CPed* pPed, const CActionFlags::DesiredDirectionBitSetData desiredDirection )
{
	if( !desiredDirection.AreAnySet() )
		return true;

	if( desiredDirection.IsSet( CActionFlags::DD_ANY ) )
		return true;

	Assert( pPed );
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe( CActionManager::CalculateDesiredHeading( pPed ) );
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe( pPed->GetCurrentHeading() );
	const float	fHeadingDelta = SubtractAngleShorter( fDesiredHeading, fCurrentHeading );

	if( desiredDirection.IsSet( CActionFlags::DD_FORWARD ) &&
		fHeadingDelta > (QUARTER_PI) &&
		fHeadingDelta < (QUARTER_PI) )
		return true;

	if( desiredDirection.IsSet( CActionFlags::DD_LEFT ) &&
		fHeadingDelta > (QUARTER_PI) &&
		fHeadingDelta < (HALF_PI + QUARTER_PI) )
		return true;

	if( desiredDirection.IsSet( CActionFlags::DD_RIGHT ) &&
		fHeadingDelta < -(QUARTER_PI) &&
		fHeadingDelta > -(HALF_PI + QUARTER_PI) )
		return true;

	if( desiredDirection.IsSet( CActionFlags::DD_BACKWARD_LEFT ) &&
		fHeadingDelta >  (HALF_PI + QUARTER_PI) )
		return true;

	if( desiredDirection.IsSet( CActionFlags::DD_BACKWARD_RIGHT ) &&
		fHeadingDelta < -(HALF_PI + QUARTER_PI) )
		return true;

	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAllowWeaponType
// PURPOSE	:	Determines whether or not the designated ped qualifies for any of
//				the given weapon flags.
// PARAMS	:	pPed - Acting ped we would like to test
//				weaponType - Weapon type flags we will use to test against the acting ped
// RETURNS	:	Whether or not the ped qualifies
/////////////////////////////////////////////////////////////////////////////////
bool CActionManager::ShouldAllowWeaponType( const CPed* pPed, CActionFlags::RequiredWeaponTypeBitSetData weaponType )
{
	Assert( pPed );

	if( !weaponType.AreAnySet() )
		return true;

	if( weaponType.IsSet( CActionFlags::RWT_ANY ) )
		return true;

	// There are cases where the player changes their weapon mid action
	const CObject* pWeaponObject = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	const CWeapon* pWeapon = pWeaponObject ? pWeaponObject->GetWeapon() : NULL;
	if( !pWeapon )
		pWeapon = CWeaponManager::GetWeaponUnarmed( pPed->GetDefaultUnarmedWeaponHash() );

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	if( !pWeaponInfo )
		return false;

	// Unarmed
	if( weaponType.IsSet( CActionFlags::RWT_UNARMED ) )
	{
		if( pWeaponInfo->GetIsUnarmed() )
			return true;
	}

	// Knife
	if( weaponType.IsSet( CActionFlags::RWT_KNIFE ) )
	{
		if( pWeaponInfo->GetIsBlade() )
			return true;
	}

	// One handed club
	if( weaponType.IsSet( CActionFlags::RWT_ONE_HANDED_CLUB ) )
	{
		if( pWeaponInfo->GetIsClub() && !pWeaponInfo->GetIsTwoHanded() )
			return true;
	}

	// Two handed club
	if( weaponType.IsSet( CActionFlags::RWT_TWO_HANDED_CLUB ) )
	{
		if( pWeaponInfo->GetIsClub() && pWeaponInfo->GetIsTwoHanded() )
			return true;
	}

	// One handed gun
	if( weaponType.IsSet( CActionFlags::RWT_ONE_HANDED_GUN ) )
	{
		if( pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsTwoHanded() )
			return true;
	}

	// Two handed gun
	if( weaponType.IsSet( CActionFlags::RWT_TWO_HANDED_GUN ) )
	{
		if( pWeaponInfo->GetIsGun() && pWeaponInfo->GetIsTwoHanded() )
			return true;
	}

	// Pistol
	if( weaponType.IsSet( CActionFlags::RWT_PISTOL ) )
	{
		if( pWeaponInfo->GetIsGun1Handed() && !pWeaponInfo->GetIsAutomatic() )
			return true;
	}

	// Thrown
	if( weaponType.IsSet( CActionFlags::RWT_THROWN ) )
	{
		if( pWeaponInfo->GetIsThrownWeapon() )
			return true;
	}

	// Thrown Special (fire exstinguisher, jerry can, etc)
	if( weaponType.IsSet( CActionFlags::RWT_THROWN_SPECIAL ) )
	{
		if( pWeaponInfo->GetWeaponWheelSlot() == WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL )
			return true;
	}

	// Hatchet
	if( weaponType.IsSet( CActionFlags::RWT_HATCHET ) )
	{
		if( pWeaponInfo->GetIsHatchet() )
			return true;
	}

	if( weaponType.IsSet( CActionFlags::RWT_MACHETE ) )
	{
		if( pWeaponInfo->GetIsMachete() )
			return true;
	}

	// One handed club (without hatchet)
	if( weaponType.IsSet( CActionFlags::RWT_ONE_HANDED_CLUB_NO_HATCHET ) )
	{
		if( pWeaponInfo->GetIsClub() && !pWeaponInfo->GetIsHatchet() && !pWeaponInfo->GetIsTwoHanded() )
			return true;
	}

	// Melee Fist (ie Knuckle Dusters)
	if( weaponType.IsSet( CActionFlags::RWT_MELEE_FIST ) )
	{
		if( pWeaponInfo->GetIsMeleeFist() )
			return true;
	}

	return false;
}

#if __DEV
//-------------------------------------------------------------------------
// Get the string for a pose (used for debug display).
//-------------------------------------------------------------------------
const char* CActionManager::TypeGetStringFromPoseType(const EstimatedPose pose)
{
	switch(pose)
	{
		case POSE_UNKNOWN:
			return "POSE_UNKNOWN";
		case POSE_ON_FRONT:
			return "POSE_ON_FRONT";
		case POSE_ON_RHS:
			return "POSE_ON_RHS";
		case POSE_ON_BACK:
			return "POSE_ON_BACK";
		case POSE_ON_LHS:
			return "POSE_ON_LHS";
		case POSE_STANDING:
			return "POSE_STANDING";
		default:
			return "POSE_STANDING";
	}
}
#endif // __DEV

#if __BANK
bkBank*		CActionManager::ms_pBank = NULL;
bkButton*	CActionManager::ms_pCreateButton = NULL;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitWidgets
// PURPOSE	:	RAG widgets for the action manager
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::InitWidgets( void )
{
	if(ms_pBank)
	{
		ShutdownWidgets();
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("Action Manager", 0, 0, false); 
	if(Verifyf(ms_pBank, "Failed to create Action Manager bank"))
	{
		ms_pBank->AddButton("Reload Action Table", ReloadActionTable, "Reload the action_table.meta file and start using it (note that current moves in action will continue)!");
		ms_pBank->AddButton("Verify Action Table Anim Usage", VerifyActionTableAnimationUsage, "Asserts if any animations referenced in meleeanims.dat are unused in the action table and vice versa.");
		ms_pBank->AddToggle("Debug Focus Ped", &ms_DebugFocusPed, NullCB, "Whether or not we only want to debug for the focus ped"); 
		ms_pCreateButton = ms_pBank->AddButton("Create Action Manager Widgets", &CActionManager::CreateActionManagerWidgetsCB);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ReloadActionTable
// PURPOSE	:	Causes the action table to be reloaded and used.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::ReloadActionTable( void )
{
#if __BANK
	ACTIONMGR.ShutdownWidgets();
#endif

#if __DEV
	// Go through all the peds, make sure they stop doing CTaskMeleeActionResult.
	CEntityIterator entityIterator(IteratePeds);
	CPed* pPed = (CPed*)entityIterator.GetNext();
	while( pPed )
	{
		// Check if this ped is doing something that needs to be flushed with the
		// action table reload.
		CTaskMeleeActionResult* pPedSimpleMeleeActionResult =
			pPed->GetPedIntelligence()->GetTaskMeleeActionResult();
		if(pPedSimpleMeleeActionResult )
		{
			pPedSimpleMeleeActionResult->MakeCurrentResultGoStale();
		}

		pPed = (CPed*)entityIterator.GetNext();
	}

	// Reload the table.
	ACTIONMGR.LoadOrReloadActionTableData();
#endif

#if __BANK
	ACTIONMGR.InitWidgets();
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	VerifyActionTableAnimationUsage
// PURPOSE	:	Used to prevent run time errors and unnecessary memory usage.
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::VerifyActionTableAnimationUsage( void )
{
#if __DEV
	ACTIONMGR.VerifyMeleeAnimUsage();
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateActionManagerWidgetsCB
// PURPOSE	:	Callback button  used to create the CActionManager widgets
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::CreateActionManagerWidgetsCB( void )
{
	ACTIONMGR.CreateActionManagerWidgets();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateActionManagerWidgets
// PURPOSE	:	Creates all CActionManager RAG widgets
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::CreateActionManagerWidgets( void )
{
	aiAssertf(ms_pBank, "Action Manager bank needs to be created first");
	aiAssertf(ms_pCreateButton, "Action Manager bank needs to be created first");

	ms_pCreateButton->Destroy();
	ms_pCreateButton = NULL;

	bkBank& bank = *ms_pBank;

	s32 i = 0;
	bank.PushGroup("CImpulseTest");
	const int nImpulseTestCount = m_aImpulseTests.GetCount();
	for(i = 0; i < nImpulseTestCount; ++i)
	{
		bank.PushGroup(m_aImpulseTests[i].GetName());
		PARSER.AddWidgets(bank, &m_aImpulseTests[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CStealthKillTest");
	const int stealthKillTestCount = m_aStealthKillTests.GetCount();
	for(i = 0; i < stealthKillTestCount; ++i)
	{
		bank.PushGroup(m_aStealthKillTests[i].GetName());
		PARSER.AddWidgets(bank, &m_aStealthKillTests[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CInterrelationTest");
	const int nInterrelationTestCount = m_aInterrelationTests.GetCount();
	for(i = 0; i < nInterrelationTestCount; ++i)
	{
		bank.PushGroup(m_aInterrelationTests[i].GetName());
		PARSER.AddWidgets(bank, &m_aInterrelationTests[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CHoming");
	const int m_aHomingsCount = m_aHomings.GetCount();
	for(i = 0; i < m_aHomingsCount; ++i)
	{
		bank.PushGroup(m_aHomings[i].GetName());
		PARSER.AddWidgets(bank, &m_aHomings[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CDamageAndReaction");
	const int nDamageAndReactionCount = m_aDamageAndReactions.GetCount();
	for(i = 0; i < nDamageAndReactionCount; ++i)
	{
		bank.PushGroup(m_aDamageAndReactions[i].GetName());
		PARSER.AddWidgets(bank, &m_aDamageAndReactions[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CStrikeBoneSet");
	const int nStrikeBoneSetCount = m_aStrikeBoneSets.GetCount();
	for(i = 0; i < nStrikeBoneSetCount; ++i)
	{
		bank.PushGroup(m_aStrikeBoneSets[i].GetName());
		PARSER.AddWidgets(bank, &m_aStrikeBoneSets[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CActionRumble");
	const int nRumbleCount = m_aActionRumbles.GetCount();
	for(i = 0; i < nRumbleCount; ++i)
	{
		bank.PushGroup(m_aActionRumbles[i].GetName());
		PARSER.AddWidgets(bank, &m_aActionRumbles[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CActionVfx");
	const int nVfxCount = m_aActionVfx.GetCount();
	for(i = 0; i < nVfxCount; ++i)
	{
		bank.PushGroup(m_aActionVfx[i].GetName());
		PARSER.AddWidgets(bank, &m_aActionVfx[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CActionFacialAnimSet");
	const int nFacialAnimSetCount = m_aActionFacialAnimSets.GetCount();
	for(i = 0; i < nFacialAnimSetCount; ++i)
	{
		bank.PushGroup(m_aActionFacialAnimSets[i].GetName());
		PARSER.AddWidgets(bank, &m_aActionFacialAnimSets[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CActionDefinition");
	const int nActionDefCount = m_aActionDefinitions.GetCount();
	for(i = 0; i < nActionDefCount; ++i)
	{
		bank.PushGroup(m_aActionDefinitions[i].GetName());
		PARSER.AddWidgets(bank, &m_aActionDefinitions[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CActionResult");
	const int nActionResCount = m_aActionResults.GetCount();
	for(i = 0; i < nActionResCount; ++i)
	{
		bank.PushGroup(m_aActionResults[i].GetName());
		PARSER.AddWidgets(bank, &m_aActionResults[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("CActionBranchSet");
	const int nActionBranchSetCount = m_aActionBranchSets.GetCount();
	for(i = 0; i < nActionBranchSetCount; ++i)
	{
		bank.PushGroup(m_aActionBranchSets[i].GetName());
		PARSER.AddWidgets(bank, &m_aActionBranchSets[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateActionManagerWidgets
// PURPOSE	:	Destroy all previously allocated CActionManager RAG widgets
/////////////////////////////////////////////////////////////////////////////////
void CActionManager::ShutdownWidgets(void)
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
	ms_pCreateButton = NULL;
}

#endif // __BANK
