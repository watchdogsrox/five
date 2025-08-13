// FILE :    PedPerception.cpp
// PURPOSE : Used to calculate if a ped can perceive another character depending on distance, peripheral vision, motion etc...
// CREATED : 19-11-2008

// class header
#include "Peds/PedIntelligence/PedPerception.h"

// Framework headers
#include "ai/aichannel.h"
#include "grcore/debugdraw.h"

// Game headers
#include "Debug/VectorMap.h"
#include "Peds/Ped.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "Scene/World/GameWorld.h"
// rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"

AI_OPTIMISATIONS()

//The seeing/hearing range of random peds, mission peds, and gang peds.
const float CPedPerception::ms_fSenseRange=60.0f;
const float CPedPerception::ms_fSenseRangePeripheral=5.0f;
const float CPedPerception::ms_fSenseRangeOfMissionPeds=60.0f;
const float CPedPerception::ms_fIdentificationRange=20.0f;
bank_float CPedPerception::ms_fSenseRangeOfPedsInCombat = 60.0f;

//Note: the full human visual field (for both eyes) is +/- 100deg in azimuth (around the vertical meridian)
//and +60deg, -75deg in elevation (around the horizontal meridian.)
bank_float CPedPerception::ms_fVisualFieldMinAzimuthAngle = -90.0f;
bank_float CPedPerception::ms_fVisualFieldMaxAzimuthAngle = 90.0f;
bank_float CPedPerception::ms_fVisualFieldMinElevationAngle = -75.0f;
bank_float CPedPerception::ms_fVisualFieldMaxElevationAngle = 60.0f;
//Peripheral vision generally begins at angles greater than 6% from the centre of vision, but this is a little narrow for our use, as we are
//more concerned with the ability to see a person, than accurately read text. We'll go with a cone of 30deg to describe the centre of gaze, assuming
//that the eyes are scanning around.
bank_float CPedPerception::ms_fCentreOfGazeMaxAngle = 60.0f;
bank_bool CPedPerception::ms_bUseHeadOrientationForPerceptionTests = false;
#if __BANK
bank_bool CPedPerception::ms_bLogPerceptionSettings = false;
#endif

//Cop-specific perception overrides specified by script.
bool CPedPerception::ms_bCopOverridesSet							= false;
float CPedPerception::ms_fCopHearingRangeOverride					= CPedPerception::ms_fSenseRange;
float CPedPerception::ms_fCopSeeingRangeOverride					= CPedPerception::ms_fSenseRange;
float CPedPerception::ms_fCopSeeingRangePeripheralOverride			= CPedPerception::ms_fSenseRangePeripheral;
float CPedPerception::ms_fCopVisualFieldMinAzimuthAngleOverride		= CPedPerception::ms_fVisualFieldMinAzimuthAngle;
float CPedPerception::ms_fCopVisualFieldMaxAzimuthAngleOverride		= CPedPerception::ms_fVisualFieldMaxAzimuthAngle;
float CPedPerception::ms_fCopCentreOfGazeMaxAngleOverride			= CPedPerception::ms_fCentreOfGazeMaxAngle;
float CPedPerception::ms_fCopRearViewRangeOverride					= -1.0f;

//bank_float CPedPerception::ms_fLightingMultiplier = 4.0f;

bank_bool CPedPerception::ms_bUseMovementModifier = true;
bank_bool CPedPerception::ms_bUseEnvironmentModifier = true;
bank_bool CPedPerception::ms_bUsePoseModifier= true;
bank_bool CPedPerception::ms_bUseAlertnessModifier= true;

CPedPerception::CPedPerception(CPed* pPed)
: m_pPed(pPed)
, m_fHearingRange(ms_fSenseRange)
, m_fSeeingRange(ms_fSenseRange)
, m_fSeeingRangePeripheral(ms_fSenseRangePeripheral)
, m_fVisualFieldMinAzimuthAngle(ms_fVisualFieldMinAzimuthAngle)
, m_fVisualFieldMaxAzimuthAngle(ms_fVisualFieldMaxAzimuthAngle)
, m_fVisualFieldMinElevationAngle(ms_fVisualFieldMinElevationAngle)
, m_fVisualFieldMaxElevationAngle(ms_fVisualFieldMaxElevationAngle)
, m_fCentreOfGazeMaxAngle(ms_fCentreOfGazeMaxAngle)
, m_fIdentificationRange(ms_fIdentificationRange)
, m_bIsHighlyPerceptive(false)
, m_fRearViewSeeingRangeOverride(-1.0f)
{

}

CPedPerception::~CPedPerception()
{

}

// Return the seeing range and peripheral ranges for this ped.
// If the ped is highly perceptive, then the peripheral range is equal to the standard seeing range.
void CPedPerception::CalculatePerceptionDistances(float &fPeripheralRange, float &fFocusRange) const
{
	fPeripheralRange = m_bIsHighlyPerceptive ? m_fSeeingRange : m_fSeeingRangePeripheral;
	fFocusRange = m_fSeeingRange;
}


bool CPedPerception::ComputeFOVVisibility(const CEntity* const subject, const Vector3* positionOverride, fwFlags8 uFlags) const
{
#if !__FINAL
	if(CPlayerInfo::ms_bDebugPlayerInvisible && subject->GetIsTypePed() && ((CPed *)subject)->IsLocalPlayer())
	{
		return false;
	}
#endif // !__FINAL

	//We would ideally consider every visible point on the subject, but let's just use base position for now.
	Vector3 subjectPosition = VEC3V_TO_VECTOR3(subject->GetTransform().GetPosition());
	if( positionOverride )
	{
		subjectPosition = *positionOverride;
	}

	float fRearViewSeeingRange = (m_fRearViewSeeingRangeOverride != -1.0f) ? m_fRearViewSeeingRangeOverride : m_fSeeingRange;
	if(uFlags.IsFlagSet(FOV_CanUseRearView) && m_pPed->GetIsDrivingVehicle() && IsVisibleInRearView(subject, subjectPosition, fRearViewSeeingRange))
	{
		return true;
	}

	// Calculate the peripheral and focus ranges
	float fPeripheralRange = m_fSeeingRangePeripheral;
	float fFocusRange = m_fSeeingRange;
	CalculatePerceptionDistances(fPeripheralRange, fFocusRange);

	if( subject->GetIsTypePed() )
	{
		const CPed* pedSubject = static_cast<const CPed*>(subject);
		if( pedSubject->GetPlayerInfo() && pedSubject->GetMotionData()->GetUsingStealth() )
		{
			fFocusRange *= pedSubject->GetPlayerInfo()->GetStealthPerceptionModifier();
		}
	}

	bool isVisible = ComputeFOVVisibility(subjectPosition, fPeripheralRange, fFocusRange);
	return isVisible;
}

bool CPedPerception::ComputeFOVVisibility(const Vector3& subjectPosition) const
{
	return ComputeFOVVisibility(subjectPosition, GetSeeingRangePeripheral(), GetSeeingRange());
}

bool CPedPerception::ComputeFOVVisibility(const Vector3& subjectPosition, const float fPeripheralRange, const float fFocusRange) const
{
	const float fVisualFieldMinAzimuthAngle	= m_bIsHighlyPerceptive ? -PI : m_fVisualFieldMinAzimuthAngle * DtoR;
	const float fVisualFieldMaxAzimuthAngle	= m_bIsHighlyPerceptive ? PI : m_fVisualFieldMaxAzimuthAngle * DtoR;
	const float fCentreOfGazeMaxAngle		= m_fCentreOfGazeMaxAngle * DtoR;

	return ComputeFOVVisibility(subjectPosition, fPeripheralRange, fFocusRange, fVisualFieldMinAzimuthAngle, fVisualFieldMaxAzimuthAngle, fCentreOfGazeMaxAngle);
}

bool CPedPerception::ComputeFOVVisibility(const Vector3& subjectPosition, const float fPeripheralRange, const float fFocusRange, const float fMinAzimuthAngle,
										  const float fMaxAzimuthAngle, const float fCenterOfGazeAngle) const
{
	const float fVisualFieldMinElevationAngle	= m_fVisualFieldMinElevationAngle * DtoR;
	const float fVisualFieldMaxElevationAngle	= m_fVisualFieldMaxElevationAngle * DtoR;

	Vector3 diff = subjectPosition - VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());

	Matrix34 viewMatrix;
	CalculateViewMatrix(viewMatrix);

	Vector3 viewDiff;
	viewMatrix.UnTransform3x3(diff, viewDiff);

	Vector3 viewDiffXYNorm(viewDiff);
	viewDiffXYNorm.z = 0.0f;
	viewDiffXYNorm.Normalize();
	float azimuthAngle = rage::Atan2f(-viewDiffXYNorm.x, viewDiffXYNorm.y);

	if((azimuthAngle >= fMinAzimuthAngle) && (azimuthAngle <= fMaxAzimuthAngle))
	{
		Vector3 viewDiffNorm(viewDiff);
		viewDiffNorm.Normalize();
		float elevationAngle = rage::Atan2f(viewDiffNorm.z, viewDiffNorm.XYMag());

		if((elevationAngle >= fVisualFieldMinElevationAngle) && (elevationAngle <= fVisualFieldMaxElevationAngle))
		{
			//Check if the subject is within the centre of gaze.
			if((azimuthAngle > -fCenterOfGazeAngle) && (azimuthAngle < fCenterOfGazeAngle) &&
				(elevationAngle > -fCenterOfGazeAngle) && (elevationAngle < fCenterOfGazeAngle))
			{
				// Within the focus range, only percieve if it is moving beyond the focus range
				bool isVisible = diff.Mag2() < rage::square(fFocusRange);
				return isVisible;
			}
			else
			{
				//Only perceive a peripheral subject if it's within the peripheral range
				bool isVisible = diff.Mag2() < rage::square(fPeripheralRange);
				return isVisible;
			}
		}
	}

	return false;
}


bool CPedPerception::IsInSeeingRange(const Vector3& vTarget) const
{
	Vector3 vDiff = vTarget - VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	return ((vDiff.Mag2()<(m_fSeeingRange*m_fSeeingRange))&&(DotProduct(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetB()),vDiff)>0));
}

/*
JB : this version of the function takes account of the entity's radius by
including it in the range test, and then modifying the vector from ped to
entity by the radius - to account for the size of the entity when testing
against the ped's peripheral vision.  Used when testing if a ped can see
an approaching vehicle.
*/
bool CPedPerception::IsInSeeingRange(const CEntity * entity) const
{
	Assert(entity);
	Assert(m_pPed);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	Vector3 vTarget = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
	Vector3 vDiff = vTarget - vPedPosition;
	float r2 = entity->GetBoundRadius() * entity->GetBoundRadius();

	// check if edge of entity's bounding radius is visible ?
	if(vDiff.Mag2() - r2 > m_fSeeingRange*m_fSeeingRange)
	{
		return false;
	}
	// nudge target forwards by bounding radius, to account for it's size
	Vector3 vecPedForward(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetB()));
	vTarget += vecPedForward * entity->GetBoundRadius();
	vDiff = vTarget - vPedPosition;
	return DotProduct(vecPedForward, vDiff) > 0;
}

/*
CS: this version of the function increases the angle test based on how close vTarget is to m_pPed.
If vTarget is within fRangeToIncreaseAngle then the angle test is increased from fSeeAngle up to a maximum angle of fMaxAngle.
If fSeeRange is positive then it overrides the peds m_fSeeingRange value.
*/
bool CPedPerception::IsInSeeingRange(const Vector3& vTarget, float fRangeToIncreaseAngle, float fMaxAngle, float fSeeRange, float fSeeAngle) const
{
	// If fSeeRange is negative, we are using the default value defined by m_fSeeingRange, 
	// otherwise we are using the value stored in the parameter
	if(fSeeRange < 0.0f)
	{
		fSeeRange = m_fSeeingRange;
	}

	Vector3 vDiff = vTarget - VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	float fDist = vDiff.Mag();

	// Is vTarget within the "see" range?
	if(fDist < fSeeRange)
	{
		// Clamp the increase angle range between 0.0001f and fSeeRange to avoid possible divide by 0
		fRangeToIncreaseAngle = Clamp(fRangeToIncreaseAngle, 0.0001f, fSeeRange);

		// Is vTarget within the increase angle range?
		if(fDist < fRangeToIncreaseAngle)
		{
			float t = 1.0f - (fDist / fRangeToIncreaseAngle);

			fSeeAngle = Lerp(t, fSeeAngle, fMaxAngle);
		}
		else
		{
			if(!ComputeFOVVisibility(vTarget))
			{
				return false;
			}
		}

		Matrix34 viewMatrix;
		CalculateViewMatrix(viewMatrix);
		Vector3 vForward = viewMatrix.b;
		vForward.z = 0.0f;
		vForward.NormalizeSafe(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetForward()));

		vDiff.NormalizeFast();
		if(DotProduct(vForward, vDiff) > fSeeAngle)
		{
			return true;
		}
	}

	return false;
}

bool CPedPerception::IsVisibleInRearView(const CEntity* const pSubject, const Vector3& vPosition, float fSeeingRange) const
{
	CVehicle* pMyVehicle = m_pPed->GetVehiclePedInside();

	//Ensure we are driving a vehicle.
	if(!pMyVehicle || !pMyVehicle->IsDriver(m_pPed))
	{
		return false;
	}

	const CPed* pSubjectPed = pSubject->GetIsTypePed() ? static_cast<const CPed*>(pSubject) : NULL;
	bool bSkipAngleCheck = pSubjectPed && pSubjectPed->GetIsOnFoot() && pMyVehicle->InheritsFromBoat();
	Vec3V vVehicleToPosition = Subtract(VECTOR3_TO_VEC3V(vPosition), m_pPed->GetVehiclePedInside()->GetTransform().GetPosition());

	if(!bSkipAngleCheck)
	{
		//Ensure the direction is valid.
		Vec3V vVehicleToPositionDirection = NormalizeFastSafe(vVehicleToPosition, Vec3V(V_ZERO));
		Vec3V vVehicleBack = Negate(m_pPed->GetVehiclePedInside()->GetTransform().GetForward());
		ScalarV scDot = Dot(vVehicleBack, vVehicleToPositionDirection);
		static dev_float s_fMinDot = 0.707f;
		ScalarV scMinDot = ScalarVFromF32(s_fMinDot);
		if(IsLessThanAll(scDot, scMinDot))
		{
			return false;
		}
	}

	//Ensure the distance is valid.
	ScalarV scDistSq = MagSquared(vVehicleToPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(fSeeingRange));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

#if !__FINAL
DEBUG_DRAW_ONLY(bank_float g_VfLineLength = 2.0f;)
bool CPedPerception::ms_bDrawVisualField = false;
bool CPedPerception::ms_bDrawVisualFieldToScale = false;
bool CPedPerception::ms_bRestrictToPedsThatHateThePlayer = true;
void CPedPerception::DrawVisualField()
{
#if DEBUG_DRAW
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if( !CPedPerception::ms_bDrawVisualField )
	{
		return;
	}
	const bool bPedHatesPlayer = (pPlayerPed && m_pPed && m_pPed->GetPedIntelligence() && m_pPed->GetPedIntelligence()->IsThreatenedBy(*pPlayerPed) );
	if( CPedPerception::ms_bRestrictToPedsThatHateThePlayer && !bPedHatesPlayer )
	{
		return;
	}
	const float fVisualFieldMinAzimuthAngle	= m_bIsHighlyPerceptive ? -PI : m_fVisualFieldMinAzimuthAngle * DtoR;
	const float fVisualFieldMaxAzimuthAngle	= m_bIsHighlyPerceptive ? PI : m_fVisualFieldMaxAzimuthAngle * DtoR;
	// 	const float fVisualFieldMinElevationAngle	= ms_fVisualFieldMinElevationAngle * DtoR;
	// 	const float fVisualFieldMaxElevationAngle	= ms_fVisualFieldMaxElevationAngle * DtoR;
	const float fCentreOfGazeMaxAngle			= m_fCentreOfGazeMaxAngle * DtoR;

	//Reorder the head matrix to match the normal convention /sigh.
	Matrix34 viewMatrix;
	CalculateViewMatrix(viewMatrix);

	Vector3 startPosition = viewMatrix.d;
	Color32 focusColour = bPedHatesPlayer?Color_DeepPink:Color_green;
	if( bPedHatesPlayer )
	{
		if( ComputeFOVVisibility( pPlayerPed, NULL ) )
		{
			focusColour = Color_red;
		}
	}

	// Render just the exterior perception ranges and interior focus boundaries so its simpler to understand
	float aAsimuthAngles[4] = {fVisualFieldMinAzimuthAngle, -fCentreOfGazeMaxAngle, fCentreOfGazeMaxAngle, fVisualFieldMaxAzimuthAngle};
	u32 iNumAngles = 4;
	for(u32 azimuthIndex=0; azimuthIndex<iNumAngles; azimuthIndex++)
	{
		float azimuthAngle = aAsimuthAngles[azimuthIndex];
		float fNextAzimuthAngle = (azimuthIndex < (iNumAngles-1))?aAsimuthAngles[azimuthIndex+1]:azimuthAngle;
		bool isArcCentreOfGaze = azimuthIndex == 1;
		bool isCentreOfGaze = azimuthIndex == 1 || azimuthIndex == 2;
		float elevationAngle = 0.0f;
		Color32 colour = isCentreOfGaze ? focusColour : Color_orange;
		Color32 arcColour = (isArcCentreOfGaze) ? focusColour : Color_orange;
		// Calculate the peripheral and focus ranges to render the debug lines
		float fPeripheralRange = m_fSeeingRangePeripheral;
		float fFocusRange = m_fSeeingRange;
		CalculatePerceptionDistances(fPeripheralRange, fFocusRange);
			
		// 3d line
		{
			{
				Vector3 line(0.0f, CPedPerception::ms_bDrawVisualFieldToScale?(isCentreOfGaze?fFocusRange:fPeripheralRange):g_VfLineLength, 0.0f);
				line.RotateX(elevationAngle);
				line.RotateZ(azimuthAngle);
				viewMatrix.Transform(line);

				grcDebugDraw::Line(startPosition, line, colour, colour);
			}
				// Draw an arc towards the next angle
			float fAngle = azimuthAngle;
			static float ANGLE_RENDER_DIFF = PI/32.0f;
			while( fAngle < fNextAzimuthAngle )
			{
				float fNextAngle = Min(fAngle + ANGLE_RENDER_DIFF, fNextAzimuthAngle);
				Vector3 line1(0.0f, CPedPerception::ms_bDrawVisualFieldToScale?(isArcCentreOfGaze?fFocusRange:fPeripheralRange):g_VfLineLength, 0.0f);
				line1.RotateX(elevationAngle);
				line1.RotateZ(fAngle);
				viewMatrix.Transform(line1);
				Vector3 line2(0.0f, CPedPerception::ms_bDrawVisualFieldToScale?(isArcCentreOfGaze?fFocusRange:fPeripheralRange):g_VfLineLength, 0.0f);
				line2.RotateX(elevationAngle);
				line2.RotateZ(fNextAngle);
				viewMatrix.Transform(line2);
				grcDebugDraw::Line(line1, line2, arcColour, arcColour);
				fAngle = fNextAngle;
			}
		}

		// Render the lines with accurate scales on the vectormap
		{
			{
				// Scale the line to the correct range
				Vector3 line(0.0f, isCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);
				line.RotateX(elevationAngle);
				line.RotateZ(azimuthAngle);
				viewMatrix.Transform(line);

				// Draw onto the vectormap
				CVectorMap::DrawLine(startPosition, line, colour, colour);
			}
				// Draw an arc towards the next angle
			float fAngle = azimuthAngle;
			static float ANGLE_RENDER_DIFF = PI/32.0f;
			while( fAngle < fNextAzimuthAngle )
			{
				float fNextAngle = Min(fAngle + ANGLE_RENDER_DIFF, fNextAzimuthAngle);
				Vector3 line1(0.0f, isArcCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);
				line1.RotateX(elevationAngle);
				line1.RotateZ(fAngle);
				viewMatrix.Transform(line1);
				Vector3 line2(0.0f, isArcCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);
				line2.RotateX(elevationAngle);
				line2.RotateZ(fNextAngle);
				viewMatrix.Transform(line2);
				CVectorMap::DrawLine(line1, line2, arcColour, arcColour);
				fAngle = fNextAngle;
			}
		}

		// Text rendering:
		s32 noOfTexts = 0;
		char outText[256] = "";
		formatf( outText, "Focus: %.2f, Peripheral %.2f", fFocusRange, fPeripheralRange);
		grcDebugDraw::Text( startPosition, Color_green, 0, noOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), outText);
	}

	// If a measuring tool pos is specified, see if it is visible
	Vector3 vTestPos(0.0f,0.0f,0.0f);
	if( pPlayerPed && CPhysics::GetMeasuringToolPos( 0, vTestPos ) )
	{
		if( ComputeFOVVisibility( pPlayerPed, &vTestPos ) )
		{
			grcDebugDraw::Line(startPosition, vTestPos, Color_green, Color_green);
		}
		else
		{
			grcDebugDraw::Line(startPosition, vTestPos, Color_red, Color_red);
		}
	}
#endif
}


void CPedPerception::InitWidgets()
{
#if __BANK
	bkBank *pBank = BANKMGR.FindBank("A.I.");
	Assert(pBank);
	pBank->PushGroup("PedPerception", false);
		pBank->AddToggle("Display FOV perception tests", &CPedPerception::ms_bDrawVisualField, NullCB, ".");
		pBank->AddToggle("Display FOV perception test to scale", &CPedPerception::ms_bDrawVisualFieldToScale, NullCB, ".");
		pBank->AddToggle("Only render peds that hate the player", &CPedPerception::ms_bRestrictToPedsThatHateThePlayer, NullCB, ".");
		pBank->AddToggle("Use Head orientation for perception tests", &CPedPerception::ms_bUseHeadOrientationForPerceptionTests, NullCB, ".");
		pBank->AddToggle("Enable perception settings logging", &CPedPerception::ms_bLogPerceptionSettings, NullCB, ".");
	pBank->PopGroup();
#endif
}

#endif // !__FINAL

void CPedPerception::GetVisualFieldLines(Vector3 avVisualFieldLines[VF_Max], float fMaxVisualRange)
{
	const float fVisualFieldMinAzimuthAngle	= m_bIsHighlyPerceptive ? -PI : m_fVisualFieldMinAzimuthAngle * DtoR;
	const float fVisualFieldMaxAzimuthAngle	= m_bIsHighlyPerceptive ? PI : m_fVisualFieldMaxAzimuthAngle * DtoR;
	const float fVisualFieldMinElevationAngle	= m_fVisualFieldMinElevationAngle * DtoR;
	const float fVisualFieldMaxElevationAngle	= m_fVisualFieldMaxElevationAngle * DtoR;
	const float fCentreOfGazeMaxAngle			= m_fCentreOfGazeMaxAngle * DtoR;

	Matrix34 viewMatrix;
	CalculateViewMatrix(viewMatrix);

	Vector3 startPosition = viewMatrix.d;

	avVisualFieldLines[VF_HeadPos] = startPosition;

	// Render just the exterior perception ranges and interior focus boundaries so its simpler to understand
	float aAsimuthAngles[4] = {fVisualFieldMinAzimuthAngle, fVisualFieldMaxAzimuthAngle, -fCentreOfGazeMaxAngle, fCentreOfGazeMaxAngle};
	u32 iNumAngles = 4;
	for(u32 azimuthIndex=0; azimuthIndex<iNumAngles; azimuthIndex++)
	{
		float azimuthAngle = aAsimuthAngles[azimuthIndex];
		bool isCentreOfGaze = azimuthIndex > 1;
		float elevationAngle = (fVisualFieldMaxElevationAngle+fVisualFieldMinElevationAngle) / 2.0f;
		// Render the lines with accurate scales on the vectormap
		{
			// Calculate the peripheral and focus ranges to render the debug lines
			float fPeripheralRange = m_fSeeingRangePeripheral;
			float fFocusRange = m_fSeeingRange;
			CalculatePerceptionDistances(fPeripheralRange, fFocusRange);

			fPeripheralRange	= MIN(fMaxVisualRange, fPeripheralRange);
			fFocusRange			= MIN(fMaxVisualRange, fFocusRange);

			// Scale the line to the correct range
			Vector3 line(0.0f, isCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);
			line.RotateX(elevationAngle);
			line.RotateZ(azimuthAngle);
			viewMatrix.Transform(line);

			// Draw onto the vectormap
			avVisualFieldLines[azimuthIndex+1] = line;
			//CVectorMap::DrawLine(startPosition, line, colour, colour);
		}
	}
}

void CPedPerception::CalculateViewMatrix( Matrix34& viewMatrix ) const
{
	if( CPedPerception::ms_bUseHeadOrientationForPerceptionTests ||
		(m_pPed && m_pPed->GetPedResetFlag(CPED_RESET_FLAG_UseHeadOrientationForPerception)) )
	{
		Matrix34 headMatrix;
		m_pPed->GetBoneMatrix(headMatrix, BONETAG_HEAD);
		viewMatrix.a = headMatrix.c;
		viewMatrix.b = headMatrix.b;
		viewMatrix.c = headMatrix.a;
		viewMatrix.d = headMatrix.d;
	}
	else
	{
		viewMatrix = MAT34V_TO_MATRIX34(m_pPed->GetMatrix());
		viewMatrix.d = m_pPed->GetBonePositionCached(BONETAG_HEAD);
	}
}

void CPedPerception::ResetCopOverrideParameters()
{
	ms_bCopOverridesSet							= false;
	ms_fCopSeeingRangeOverride					= CPedPerception::ms_fSenseRange;
	ms_fCopSeeingRangePeripheralOverride		= CPedPerception::ms_fSenseRangePeripheral;
	ms_fCopHearingRangeOverride					= CPedPerception::ms_fSenseRange;
	ms_fCopVisualFieldMinAzimuthAngleOverride	= CPedPerception::ms_fVisualFieldMinAzimuthAngle;
	ms_fCopVisualFieldMaxAzimuthAngleOverride	= CPedPerception::ms_fVisualFieldMaxAzimuthAngle;
	ms_fCopCentreOfGazeMaxAngleOverride			= CPedPerception::ms_fCentreOfGazeMaxAngle;
	ms_fCopRearViewRangeOverride				= -1.0f;
}