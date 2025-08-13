// FILE :    PedPerception.h
// PURPOSE : Used to calculate if a ped can perceive another character depending on distance, peripheral vision, motion etc...
// CREATED : 19-11-2008

#ifndef PED_PERCEPTION_H
#define PED_PERCEPTION_H

// rage headers
#include "fwutil/Flags.h"
#include "vector/vector3.h"

// game headers
#if __BANK
#include "ai/debug/system/AIDebugLogManager.h"
#endif

class CPed;
class CEntity;

// Defines an interface for querying perception between peds.
class CPedPerception
{
public:
	CPedPerception(CPed* pPed);
	~CPedPerception();

	enum { FOV_CanUseRearView = BIT0 };

	// More advanced Field of view visibility checks
	bool ComputeFOVVisibility(const CEntity* const subject, const Vector3* positionOverride, fwFlags8 uFlags = 0) const;
	bool ComputeFOVVisibility(const Vector3& subjectPosition) const;
	bool ComputeFOVVisibility(const Vector3& subjectPosition, const float fPeripheralRange, const float fFocusRange) const;
	bool ComputeFOVVisibility(const Vector3& subjectPosition, const float fPeripheralRange, const float fFocusRange, const float fMinAzimuthAngle,
							  const float fMaxAzimuthAngle, const float fCenterOfGazeAngle) const;

	void CalculateViewMatrix(Matrix34& viewMatrix) const;
	void CalculatePerceptionDistances(float &fPeripheralRange, float &fFocusRange) const;

	float GetSenseRange() const {return (m_fHearingRange > m_fSeeingRange ? m_fHearingRange : m_fSeeingRange);}

	// Set methods for perception components
	void SetEncroachmentRange(float fRange) {m_fEncroachmentRange = fRange; }
	void SetEncroachmentCloseRange(float fRange) { m_fEncroachmentCloseRange = fRange; }
	void SetHearingRange(const float fRange) {BANK_ONLY(if (CPedPerception::ms_bLogPerceptionSettings) {AI_LOG_WITH_ARGS("[%p]CPedPerception::SetHearingRange - setting it to %.2f\n",m_pPed,fRange); AILogging::StackTraceLog(); }) m_fHearingRange=fRange;}
	void SetSeeingRange(const float fRange) {BANK_ONLY(if (CPedPerception::ms_bLogPerceptionSettings) {AI_LOG_WITH_ARGS("[%p]CPedPerception::SetSeeingRange - setting it to %.2f\n",m_pPed,fRange); AILogging::StackTraceLog(); }) m_fSeeingRange=fRange;}
	void SetSeeingRangePeripheral(const float fRange) {BANK_ONLY(if (CPedPerception::ms_bLogPerceptionSettings) {AI_LOG_WITH_ARGS("[%p]CPedPerception::SetSeeingRangePeripheral - setting it to %.2f\n",m_pPed,fRange); AILogging::StackTraceLog(); }) m_fSeeingRangePeripheral=fRange;}
	void SetVisualFieldMinAzimuthAngle(const float fAngle) {BANK_ONLY(if (CPedPerception::ms_bLogPerceptionSettings) {AI_LOG_WITH_ARGS("[%p]CPedPerception::SetVisualFieldMinAzimuthAngle - setting it to %.2f\n",m_pPed,fAngle); AILogging::StackTraceLog(); }) m_fVisualFieldMinAzimuthAngle=fAngle;}
	void SetVisualFieldMaxAzimuthAngle(const float fAngle) {BANK_ONLY(if (CPedPerception::ms_bLogPerceptionSettings) {AI_LOG_WITH_ARGS("[%p]CPedPerception::SetVisualFieldMaxAzimuthAngle - setting it to %.2f\n",m_pPed,fAngle); AILogging::StackTraceLog(); }) m_fVisualFieldMaxAzimuthAngle=fAngle;}
	void SetVisualFieldMinElevationAngle(const float fAngle) {m_fVisualFieldMinElevationAngle=fAngle;}
	void SetVisualFieldMaxElevationAngle(const float fAngle) {m_fVisualFieldMaxElevationAngle=fAngle;}
	void SetCentreOfGazeMaxAngle(const float fAngle) {BANK_ONLY(if (CPedPerception::ms_bLogPerceptionSettings) {AI_LOG_WITH_ARGS("[%p]CPedPerception::SetCentreOfGazeMaxAngle - setting it to %.2f\n",m_pPed,fAngle); AILogging::StackTraceLog(); }) m_fCentreOfGazeMaxAngle = fAngle;}
	void SetIdentificationRange(const float fRange) {m_fIdentificationRange=fRange;}
	void SetIsHighlyPerceptive(bool bHighlyPerceptive) { m_bIsHighlyPerceptive = bHighlyPerceptive; }
	void SetCanAlwaysSenseEncroachingPlayers(bool bAlways) { m_bCanAlwaysSenseEncroachingPlayers = bAlways; }
	void SetPerformsEncroachmentChecksIn3D(bool b3D) { m_bUse3DEncroachment = b3D; }
	void SetRearViewRangeOverride(const float fRange) { m_fRearViewSeeingRangeOverride = fRange; }

	float GetEncroachmentRange() const { return m_fEncroachmentRange; }
	float GetEncroachmentCloseRange() const { return m_fEncroachmentCloseRange; }
	float GetHearingRange() const {return m_fHearingRange;}
	float GetSeeingRange() const {return m_fSeeingRange;}
	float GetSeeingRangePeripheral() const { return m_fSeeingRangePeripheral; }
	float GetVisualFieldMinAzimuthAngle() const { return m_fVisualFieldMinAzimuthAngle;}
	float GetVisualFieldMaxAzimuthAngle() const { return m_fVisualFieldMaxAzimuthAngle;}
	float GetVisualFieldMinElevationAngle() const { return m_fVisualFieldMinElevationAngle;}
	float GetVisualFieldMaxElevationAngle() const { return m_fVisualFieldMaxElevationAngle;}
	float GetCentreOfGazeMaxAngle() const { return m_fCentreOfGazeMaxAngle;}
	bool GetCanAlwaysSenseEncroachingPlayers() const { return m_bCanAlwaysSenseEncroachingPlayers; }
	bool GetPerformsEncroachmentChecksIn3D() const { return m_bUse3DEncroachment; }
	float GetRearViewRangeOverride() const { return m_fRearViewSeeingRangeOverride; }

	float GetIdentificationRange() const { return m_fIdentificationRange; }
	bool GetIsHighlyPerceptive() const { return m_bIsHighlyPerceptive; }

	// Basic ranged based queries
	bool IsInSeeingRange(const Vector3& vTarget) const;
	bool IsInSeeingRange(const CEntity * entity) const;
	bool IsInSeeingRange(const Vector3& vTarget, float fRangeToIncreaseAngle, float fMaxAngle, float fSeeRange = -1.0f, float fSeeAngle = 0.0f) const;

	bool IsVisibleInRearView(const CEntity* const pSubject, const Vector3& vPosition, float fSeeingRange = 45.0f) const;

	static void ResetCopOverrideParameters();

	// Per frame process function
	void Update() {}

#if !__FINAL
	static bool ms_bDrawVisualField;
	static bool ms_bDrawVisualFieldToScale;
	static bool ms_bRestrictToPedsThatHateThePlayer;
	void DrawVisualField();
	static void InitWidgets();
#endif // __DEV
	enum { VF_HeadPos = 0, VF_LeftPeripheral, VF_RightPeripheral, VF_LeftFocus, VF_RightFocus, VF_Max};
	void GetVisualFieldLines(Vector3 avVisualFieldLines[VF_Max], float fMaxVisualRange = 999.0f);

	// public static variables
	//The seeing/hearing range of random peds, mission peds, and gang peds.
	static const float ms_fSenseRange;
	static const float ms_fSenseRangePeripheral;
	static const float ms_fSenseRangeOfMissionPeds;
	static bank_float  ms_fSenseRangeOfPedsInCombat;
	static const float ms_fIdentificationRange;

	//Note: the full human visual field (for both eyes) is +/- 100deg in azimuth (around the vertical meridian)
	//and +60deg, -75deg in elevation (around the horizontal meridian.)
	static bank_float ms_fVisualFieldMinAzimuthAngle;
	static bank_float ms_fVisualFieldMaxAzimuthAngle;
	static bank_float ms_fVisualFieldMinElevationAngle;
	static bank_float ms_fVisualFieldMaxElevationAngle;

	//Peripheral vision generally begins at angles greater than 6% from the centre of vision, but this is a little narrow for our use, as we are
	//more concerned with the ability to see a person, than accurately read text. We'll go with a cone of 30deg to describe the centre of gaze, assuming
	//that the eyes are scanning around.
	static bank_float ms_fCentreOfGazeMaxAngle;


	// Should the head matrix be used for perception tests.
	static bank_bool ms_bUseHeadOrientationForPerceptionTests;

	static bank_bool ms_bLogPerceptionSettings;

	// B*2343564: Static perception overrides defined by script for cop peds.
	static bool ms_bCopOverridesSet;
	static float ms_fCopHearingRangeOverride;
	static float ms_fCopSeeingRangeOverride;
	static float ms_fCopSeeingRangePeripheralOverride;
	static float ms_fCopVisualFieldMinAzimuthAngleOverride;
	static float ms_fCopVisualFieldMaxAzimuthAngleOverride;
	static float ms_fCopCentreOfGazeMaxAngleOverride;
	static float ms_fCopRearViewRangeOverride;

	static bank_bool	 ms_bUseMovementModifier;
	static bank_bool	 ms_bUseEnvironmentModifier;
	static bank_bool	 ms_bUsePoseModifier;
	static bank_bool	 ms_bUseAlertnessModifier;

private:
	CPed* m_pPed;

	float m_fEncroachmentRange;
	float m_fEncroachmentCloseRange;

	float m_fHearingRange;
	float m_fSeeingRange;
	float m_fSeeingRangePeripheral;

	// angles for perception are specified in degrees
	float m_fVisualFieldMinAzimuthAngle;
	float m_fVisualFieldMaxAzimuthAngle;
	float m_fVisualFieldMinElevationAngle;
	float m_fVisualFieldMaxElevationAngle;
	float m_fCentreOfGazeMaxAngle;

	float m_fIdentificationRange;

	// This essentially will give the ped a visual field azimuth value from -180 to 180 degrees 
	// and a peripheral seeing range equal to their standard seeing range
	bool m_bIsHighlyPerceptive;

	// When scanning for ped encroachment events, this bool lets a ped always perceive the player if he is within the ped's personal space.
	bool m_bCanAlwaysSenseEncroachingPlayers;

	// When scanning for ped encroachment events, force the check to be made in 3D not 2D space.
	bool m_bUse3DEncroachment;

	// Allows script to override rear view seeing range. Currently only set for cops with overridden perception params.
	float m_fRearViewSeeingRangeOverride;
};


#endif

