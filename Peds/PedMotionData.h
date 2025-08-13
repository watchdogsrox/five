#ifndef INC_PED_MOTION_DATA_H
#define INC_PED_MOTION_DATA_H

// Rage headers
#include "fwanimation\anim_channel.h"
#include "fwanimation\animdefines.h"
#include "fwsys/timer.h"
#include "fwutil/Flags.h"
#include "vector/vector2.h"

#include "Game/Config.h"
#include "PedMotionState.h"

// Move blend ratio speeds
extern const float MOVEBLENDRATIO_REVERSE;
extern const float MOVEBLENDRATIO_STILL;
extern const float MOVEBLENDRATIO_WALK;
extern const float MOVEBLENDRATIO_RUN;
extern const float MOVEBLENDRATIO_SPRINT;
extern const float ONE_OVER_MOVEBLENDRATIO_SPRINT;

// Move blend ratio boundaries
extern const float MBR_WALK_BOUNDARY;
extern const float MBR_RUN_BOUNDARY;
extern const float MBR_SPRINT_BOUNDARY;

class CPedMotionData
{
public:

	struct AimPoseTransition
	{
	
	public:
	
		AimPoseTransition()
		{
			Clear();
		}
		
	public:
	
		fwMvClipSetId GetClipSetId() const
		{
			return m_ClipSetId;
		}
		
		fwMvClipId GetClipId() const
		{
			return m_ClipId;
		}
		
		float GetRate() const
		{
			return m_fRate;
		}
		
		void SetClipSetId(fwMvClipSetId clipSetId)
		{
			m_ClipSetId = clipSetId;
		}
		
		void SetClipId(fwMvClipId clipId)
		{
			m_ClipId = clipId;
		}
		
		void SetRate(float fRate)
		{
			m_fRate = fRate;
		}
		
	public:

		void Clear()
		{
			m_ClipSetId	= CLIP_SET_ID_INVALID;
			m_ClipId	= CLIP_ID_INVALID;
			m_fRate		= 1.0f;
		}
		
		bool IsClipValid() const
		{
			return ((m_ClipSetId != CLIP_SET_ID_INVALID) && (m_ClipId != CLIP_ID_INVALID));
		}
		
	private:

		fwMvClipSetId	m_ClipSetId;
		fwMvClipId		m_ClipId;
		float			m_fRate;
	};

	struct StationaryAimPose
	{
	
	public:
	
		StationaryAimPose()
		{
			Clear();
		}
		
	public:
	
		bool operator==(const StationaryAimPose& rOther) const
		{
			if(m_LoopClipSetId != rOther.m_LoopClipSetId)
			{
				return false;
			}
			
			if(m_LoopClipId != rOther.m_LoopClipId)
			{
				return false;
			}
			
			return true;
		}
		
		bool operator!=(const StationaryAimPose& rOther) const
		{
			return !(*this == rOther);
		}
		
	public:
	
		fwMvClipSetId GetLoopClipSetId() const
		{
			return m_LoopClipSetId;
		}

		fwMvClipId GetLoopClipId() const
		{
			return m_LoopClipId;
		}
		
		void SetLoopClipSetId(fwMvClipSetId clipSetId)
		{
			m_LoopClipSetId = clipSetId;
		}

		void SetLoopClipId(fwMvClipId clipId)
		{
			m_LoopClipId = clipId;
		}
		
	public:
		
		void Clear()
		{
			m_LoopClipSetId	= CLIP_SET_ID_INVALID;
			m_LoopClipId	= CLIP_ID_INVALID;
		}
		
		bool IsLoopClipValid() const
		{
			return ((m_LoopClipSetId != CLIP_SET_ID_INVALID) && (m_LoopClipId != CLIP_ID_INVALID));
		}
		
	private:
		
		fwMvClipSetId	m_LoopClipSetId;
		fwMvClipId		m_LoopClipId;
		
	};

	CPedMotionData();

	void Reset();

	float GetCurrentHeading() const { return m_fCurrentHeading; }
	const float& GetCurrentHeadingRef() const { return m_fCurrentHeading; }
	float GetDesiredHeading() const { return m_fDesiredHeading; }
	float GetHeadingChangeRate() const { return m_fHeadingChangeRate; }
	float GetCurrentPitch() const { return m_fCurrentPitch; }
	const float& GetCurrentPitchRef() const { return m_fCurrentPitch; }
	float GetDesiredPitch() const { return m_fDesiredPitch; }
	float GetBoundPitch() const { return m_fBoundPitch; }
	float GetDesiredBoundPitch() const { return m_fDesiredBoundPitch; }
	float GetBoundHeading() const { return m_fBoundHeading; }
	float GetDesiredBoundHeading() const { return m_fDesiredBoundHeading; }
	const Vector3& GetBoundOffset() const { return m_vBoundOffset; }
	const Vector3& GetDesiredBoundOffset() const { return m_vDesiredBoundOffset; }
	bool GetUseExtractedZ() const { return m_bUseExtractedZ; }

	void SetCurrentHeading(const float f) { m_fCurrentHeading = f; }
	void SetDesiredHeading(const float f) { m_fDesiredHeading = f; m_bDesiredHeadingSetThisFrame = true; }
	void SetHeadingChangeRate(const float f) { m_fHeadingChangeRate = f; }
	void SetHeadingChangeRateAccel(const float f) { m_fHeadingChangeRate = f; }
	void SetCurrentPitch(const float f) { m_fCurrentPitch = f; }
	void SetDesiredPitch(const float f) { m_fDesiredPitch = f; }
	void SetPedBoundPitch(const float f) { m_fBoundPitch = f; }
	void SetDesiredPedBoundPitch(const float f) { m_fDesiredBoundPitch = f; }
	void SetPedBoundHeading(const float f) { m_fBoundHeading = f; }
	void SetDesiredPedBoundHeading(const float f) { m_fDesiredBoundHeading = f; }
	void SetPedBoundOffset(const Vector3& v) { m_vBoundOffset = v; }
	void SetDesiredPedBoundOffset(const Vector3& v) { m_vDesiredBoundOffset = v; }
	void SetUseExtractedZ(const bool b) { m_bUseExtractedZ = b; }	// NB: Must be called only through CPed::SetUseExtractedZ()

	void SetCurrentHeadingAndPitchFromMatrix(Mat34V_In mtrx)
	{
		const Vec3V matVec0V = mtrx.GetCol0();
		const Vec3V matVec1V = mtrx.GetCol1();
		const Vec3V matVec2V = mtrx.GetCol2();
		const ScalarV pitchBeforReverseV = MagXY(matVec1V);
		const BoolV reverseV = IsLessThan(matVec2V.GetZ(), ScalarV(V_ZERO));
		const ScalarV pitchV = SelectFT(reverseV, pitchBeforReverseV, Negate(pitchBeforReverseV));
		const Vec2V atanArgYV(matVec0V.GetY(), matVec1V.GetZ());
		const Vec2V atanArgXV(matVec1V.GetY(), pitchV);
		const Vec2V atanResultV = Arctan2(atanArgYV, atanArgXV);

		StoreScalar32FromScalarV(m_fCurrentHeading, atanResultV.GetX());
		StoreScalar32FromScalarV(m_fCurrentPitch, atanResultV.GetY());

		Assertf(m_fCurrentHeading >= -PI && m_fCurrentHeading <= PI, "Invalid fCurrentHeading: %f", m_fCurrentHeading);
	}

	// Called every frame to reset some variables before the AI runs
	void ProcessPreTaskTrees()
	{
		m_bPlayerHadControlOfPedLastFrame = m_bPlayerHasControlOfPedThisFrame;
		m_bPlayerHasControlOfPedThisFrame = false;
		
		m_bForceSteepSlopeTestThisFrame = false;
		
		//Clear the extra change for this frame.
		m_fExtraHeadingChangeThisFrame = 0.0f;
		m_fExtraPitchChangeThisFrame = 0.0f;

		m_bDesiredHeadingSetThisFrame = false;
	}

	// Called every frame to reset some variables after the AI runs
	void ProcessPostTaskTrees()
	{
		m_iStateForcedThisFrame = CPedMotionStates::MotionState_None;
		m_bWasCrouching = GetIsCrouching();
	}

	// Called every frame to reset some variables after the movement has been done
	void ProcessPostMovement()
	{
		// Update independent mover transition
		s32 sIndependentMoverTransition = (s32)GetIndependentMoverTransition();
		sIndependentMoverTransition = Max(sIndependentMoverTransition - 1, 0);
		SetIndependentMoverTransition((u32)sIndependentMoverTransition);

		if(sIndependentMoverTransition == 0)
		{
			SetIndependentMoverHeadingDelta(0.0f);
		}
	}

	void StopAllMotion(bool UNUSED_PARAM(bImmediately) = true)
	{
		m_vCurrentMoveBlendRatio = Vector2(0.0f, 0.0f);
		m_vDesiredMoveBlendRatio = Vector2(0.0f, 0.0f);
		m_fHeadingChangeRate = 0.0f;
	}

	//***************************************
	// Accessors for current & desired MBRs
	inline const Vector2 & GetDesiredMoveBlendRatio() const { return m_vDesiredMoveBlendRatio; }
	inline const Vector2 & GetCurrentMoveBlendRatio() const { return m_vCurrentMoveBlendRatio; }
	inline void GetDesiredMoveBlendRatio(Vector2 & vDesiredMoveBlendRatio) const { vDesiredMoveBlendRatio = m_vDesiredMoveBlendRatio; }
	inline void GetCurrentMoveBlendRatio(Vector2 & vCurrentMoveBlendRatio) const { vCurrentMoveBlendRatio = m_vCurrentMoveBlendRatio; }
	inline void GetDesiredMoveBlendRatio(float & x, float & y) const { x = m_vDesiredMoveBlendRatio.x; y = m_vDesiredMoveBlendRatio.y; }
	inline void GetCurrentMoveBlendRatio(float & x, float & y) const { x = m_vCurrentMoveBlendRatio.x; y = m_vCurrentMoveBlendRatio.y; }
	inline float GetDesiredMbrX() const { return m_vDesiredMoveBlendRatio.x; }
	inline float GetDesiredMbrY() const { return m_vDesiredMoveBlendRatio.y; }
	inline float GetCurrentMbrX() const { return m_vCurrentMoveBlendRatio.x; }
	inline float GetCurrentMbrY() const { return m_vCurrentMoveBlendRatio.y; }

	void SetDesiredMoveBlendRatio(const float fMoveBlendRatio, const float fLateralMoveBlendRatio = 0.0f)
	{
		SetCurrentMoveBlendRatio(m_vDesiredMoveBlendRatio, fMoveBlendRatio, fLateralMoveBlendRatio);
	}

	//*********************************************************************
	// This forces the current MBR to be the input values
	void SetCurrentMoveBlendRatio(const float fMoveBlendRatio, const float fLateralMoveBlendRatio = 0.0f, const bool bIgnoreScriptedMBR = false)
	{
		SetCurrentMoveBlendRatio(m_vCurrentMoveBlendRatio, fMoveBlendRatio, fLateralMoveBlendRatio, bIgnoreScriptedMBR);
	}

	//************************************************************************************
	// Set the max move blend ratio
	void SetScriptedMaxMoveBlendRatio(float fScriptedMaxMoveBlendRatio) { m_fScriptedMaxMoveBlendRatio = fScriptedMaxMoveBlendRatio; }
	void SetScriptedMinMoveBlendRatio(float fScriptedMinMoveBlendRatio) { m_fScriptedMinMoveBlendRatio = fScriptedMinMoveBlendRatio; }	
	void SetGaitReducedMaxMoveBlendRatio(float fGaitReducedMaxMoveBlendRatio) { m_fGaitReducedMaxMoveBlendRatio = fGaitReducedMaxMoveBlendRatio; }		
	float GetGaitReducedMaxMoveBlendRatio() const { return m_fGaitReducedMaxMoveBlendRatio; }		
	Vector2 GetGaitReducedDesiredMoveBlendRatio() const {return Vector2(Sign(m_vDesiredMoveBlendRatio.x) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.x)), Sign(m_vDesiredMoveBlendRatio.y) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.y)));}
	void GetGaitReducedDesiredMoveBlendRatio(Vector2 & vDesiredMoveBlendRatio) const { vDesiredMoveBlendRatio.Set( Sign(m_vDesiredMoveBlendRatio.x) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.x)), Sign(m_vDesiredMoveBlendRatio.y) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.y))); }
	void GetGaitReducedDesiredMoveBlendRatio(float & x, float & y) const { x = Sign(m_vDesiredMoveBlendRatio.x) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.x)); y = Sign(m_vDesiredMoveBlendRatio.y) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.y)); }
	inline float GetGaitReducedDesiredMbrX() const {return Sign(m_vDesiredMoveBlendRatio.x) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.x));}
	inline float GetGaitReducedDesiredMbrY() const {return Sign(m_vDesiredMoveBlendRatio.y) * Min(m_fGaitReducedMaxMoveBlendRatio, fabs(m_vDesiredMoveBlendRatio.y));}
	inline float GetScriptedMaxMoveBlendRatio() const { return m_fScriptedMaxMoveBlendRatio; }		
	inline float GetScriptedMinMoveBlendRatio() const { return m_fScriptedMinMoveBlendRatio; }		

	//************************************************************************************
	// Gait reduction data
	// Gait reduction flags
	enum GaitReductionFlags
	{
		GR_HitIncline						= BIT0,		

		// Bit sets
		DEFAULT_GAIT_REDUCTION_FLAGS		= 0,
	};

	void SetGaitReduction(float fGaitReduction) { m_fGaitReduction = fGaitReduction; }		
	float GetGaitReduction() const { return m_fGaitReduction; }
	void SetGaitReductionHeading(float fGaitReductionHeading) { m_fGaitReductionHeading = fGaitReductionHeading; }		
	float GetGaitReductionHeading() const { return m_fGaitReductionHeading; }	
	void SetGaitReductionBlockTime() { m_GaitReductionBlockTime = fwTimer::GetTimeInMilliseconds();}
	u32 GetGaitReductionBlockTime() const {return m_GaitReductionBlockTime;}
	void SetGaitReductionFlag(GaitReductionFlags iFlag, bool bSet) { m_iGaitReductionFlags.ChangeFlag((u8)iFlag, bSet); }
	bool GetGaitReductionFlag(GaitReductionFlags iFlag) { return m_iGaitReductionFlags.IsFlagSet((u8)iFlag); }
	void ResetGaitReductionFlags() { m_iGaitReductionFlags.ClearAllFlags();}

	//************************************************************************************
	// Query the move blend ratio for preset speed ranges
	inline bool GetIsMbrInRange(const float fMin, const float fMax) const { return GetIsMbrInRange(GetCurrentMbrY(), fMin, fMax); }
	inline bool GetIsStill() const { return GetIsStrafing() ? GetIsStill(m_vCurrentMoveBlendRatio.Mag()) : GetIsStill(Abs(GetCurrentMbrY())); } 
	inline bool GetIsWalking() const { return GetIsStrafing() ? GetIsWalking(m_vCurrentMoveBlendRatio.Mag()) : GetIsWalking(Abs(GetCurrentMbrY())); }
	inline bool GetIsRunning() const { return GetIsStrafing() ? GetIsRunning(m_vCurrentMoveBlendRatio.Mag()) : GetIsRunning(Abs(GetCurrentMbrY())); }
	inline bool GetIsSprinting() const { return GetIsStrafing() ? GetIsSprinting(m_vCurrentMoveBlendRatio.Mag()) : GetIsSprinting(Abs(GetCurrentMbrY())); }
	inline bool GetIsLessThanRun() const { return GetIsStrafing() ? GetIsLessThanRun(m_vCurrentMoveBlendRatio.Mag()) : GetIsLessThanRun(Abs(GetCurrentMbrY())); }
	inline bool GetIsLessThanSprint() const { return GetIsStrafing() ? GetIsLessThanSprint(m_vCurrentMoveBlendRatio.Mag()) : GetIsLessThanSprint(Abs(GetCurrentMbrY())); }

	bool GetIsDesiredSprinting(const bool bTestRun) const;

	//************************************************************************************
	// Query if the passed in MBR is in the preset range
	static inline bool GetIsMbrInRange(const float fMBR, const float fMin, const float fMax) { return fMBR >= fMin && fMBR < fMax; }
	static inline bool GetIsStill(const float fMBR) { return fMBR < MBR_WALK_BOUNDARY; } 
	static inline bool GetIsWalking(const float fMBR) { return GetIsMbrInRange(fMBR, MBR_WALK_BOUNDARY, MBR_RUN_BOUNDARY); }
	static inline bool GetIsRunning(const float fMBR) { return GetIsMbrInRange(fMBR, MBR_RUN_BOUNDARY, MBR_SPRINT_BOUNDARY); }
	static inline bool GetIsSprinting(const float fMBR) { return fMBR >= MBR_SPRINT_BOUNDARY; }
	static inline bool GetIsLessThanRun( const float fMBR ) { return fMBR < MBR_RUN_BOUNDARY; }
	static inline bool GetIsLessThanSprint( const float fMBR ) { return fMBR < MBR_SPRINT_BOUNDARY; }

#if __BANK
	static const char* GetMotionStateString(s32 iMotionStateHash);
#endif // __BANK

	//**************************************************************************************************
	// This function gets the 'm_fExtraHeadingChangeThisFrame' - an amount of rotation which is applied
	// to the ped in addition to any rotation which is extracted from animations.
	// This is used to force peds to turn faster than may be achievable via their movement anims.
	float GetExtraHeadingChangeThisFrame() const { return m_fExtraHeadingChangeThisFrame; }
	void SetExtraHeadingChangeThisFrame(float fTheta) 
	{
		if (Verifyf(fTheta == fTheta, "Invalid heading passed in to CPedMotionData::SetExtraHeadingChangeThisFrame"))
			m_fExtraHeadingChangeThisFrame = fTheta; 	
	}

	float GetExtraPitchChangeThisFrame() const { return m_fExtraPitchChangeThisFrame; }
	void SetExtraPitchChangeThisFrame(float fExtraPitchChangeThisFrame) { m_fExtraPitchChangeThisFrame = fExtraPitchChangeThisFrame; }

	//**************************************************************************************************
	// This is set by the CTaskSimpleMovePlayer::ProcessPed function to indicate that this ped is under
	// direct player control (so we can change some of the movement params accordingly, since we know we
	// are not autonomously following a route).  This is done in preference of the CPed::IsPedInControl
	// function which only tells you whether the ped is not dead/jumping/falling, etc.

	bool GetPlayerHasControlOfPedThisFrame() const { return m_bPlayerHasControlOfPedThisFrame; }
	void SetPlayerHasControlOfPedThisFrame(bool b) { m_bPlayerHasControlOfPedThisFrame = b; }

	bool GetPlayerHadControlOfPedLastFrame() const { return m_bPlayerHadControlOfPedLastFrame; }

	bool GetForceSteepSlopeTestThisFrame() const { return m_bForceSteepSlopeTestThisFrame; }
	void SetForceSteepSlopeTestThisFrame(bool b) { m_bForceSteepSlopeTestThisFrame = b; }

	float GetCurrentTurnVelocity() const { return m_fCurrentTurnVelocity; }
	void SetCurrentTurnVelocity(float fCurrentTurnVelocity) { m_fCurrentTurnVelocity = fCurrentTurnVelocity; }

	bool GetIsCrouching() const { return m_iCrouchStartTime && (m_iCrouchDuration==-1 || ((m_iCrouchStartTime+m_iCrouchDuration)>fwTimer::GetTimeInMilliseconds())); }
	
	// Duration should be -1 for infinite
	void SetIsCrouching(u32 iStartTimeMsecs, s32 iDurationMs, bool bForceAllow = false)
	{
		if(bForceAllow || CGameConfig::Get().AllowCrouchedMovement())
		{
			m_iCrouchStartTime = iStartTimeMsecs;
			m_iCrouchDuration = iDurationMs;
		}
		else
		{
			m_iCrouchStartTime = 0;
			m_iCrouchDuration = -1;
		}
	}

	bool GetWasCrouching() const { return m_bWasCrouching; }

	bool GetUsingStealth() const { return m_bUsingStealth; }
	void SetUsingStealth(bool s)
	{
		if(!CGameConfig::Get().AllowStealthMovement())
			return;
		m_bUsingStealth = s;
	}

	inline bool GetIsStrafing() const { return m_bStrafing; }
	inline bool SetIsStrafing(bool s) { m_bStrafing = s; return true; }

	bool GetIsFlying() const { return (m_iFlyingState != FLYING_INACTIVE); }
	bool GetIsFlyingForwards() const { return (m_iFlyingState == FLYING_FORWARDS); }
	bool GetIsFlyingUpwards() const { return (m_iFlyingState == FLYING_UPWARDS); }
	bool GetIsFlyingDownwards() const { return (m_iFlyingState == FLYING_DOWNWARDS); }
	void SetIsFlyingForwards() { m_iFlyingState = FLYING_FORWARDS; }
	void SetIsFlyingUpwards() { m_iFlyingState = FLYING_UPWARDS; }
	void SetIsFlyingDownwards() { m_iFlyingState = FLYING_DOWNWARDS; }

	void SetIsNotFlying() { m_iFlyingState = FLYING_INACTIVE; }

	int GetFPSState() const { return m_iFPSState; }
	int GetPreviousFPSState() const { return m_iFPSPreviousState; }
	bool GetIsFPSIdle() const { return m_iFPSState == FPS_IDLE; }
	bool GetIsFPSRNG() const { return m_iFPSState == FPS_RNG; }
	bool GetIsFPSLT() const { return m_iFPSState == FPS_LT; }
	bool GetIsFPSScope() const { return m_iFPSState == FPS_SCOPE; }
	bool GetIsFPSUnholster() const { return m_iFPSState == FPS_UNHOLSTER; }
	bool GetWasFPSIdle() const { return m_iFPSPreviousState == FPS_IDLE; }
	bool GetWasFPSRNG() const { return m_iFPSPreviousState == FPS_RNG; }
	bool GetWasFPSLT() const { return m_iFPSPreviousState == FPS_LT; } 
	bool GetWasFPSScope() const { return m_iFPSPreviousState == FPS_SCOPE; } 
	bool GetWasFPSUnholster() const { return m_iFPSPreviousState == FPS_UNHOLSTER; } 
	bool GetWasUsingStealthInFPS() const { return m_bWasUsingStealthInFPS; }
	bool GetIsUsingStealthInFPS() const { return m_bIsUsingStealthInFPS; }
	bool GetSwitchedToStealthInFPS() const { return m_bIsUsingStealthInFPS && !m_bWasUsingStealthInFPS; }
	bool GetSwitchedFromStealthInFPS() const { return !m_bIsUsingStealthInFPS && m_bWasUsingStealthInFPS; }
	bool GetToggledStealthWhileFPSIdle() const { return m_bIsUsingStealthInFPS != m_bWasUsingStealthInFPS && GetIsFPSIdle() && GetWasFPSIdle(); }
	void SetCurrentFPSState(int iState);
	void SetPreviousFPSState(int iState);
	void SetIsUsingStealthInFPS(bool s);

	bool GetUsingFPSMode() const { return m_bUsingFPSMode; }
	bool GetWasUsingFPSMode() const { return m_bWasUsingFPSMode; }
	void SetUsingFPSMode(bool bFPSMode) { m_bWasUsingFPSMode = m_bUsingFPSMode; m_bUsingFPSMode = bFPSMode; }

	s32 GetForcedMotionStateThisFrame() { return m_iStateForcedThisFrame; }
	void SetForcedMotionStateThisFrame(s32 state) { m_iStateForcedThisFrame = state; }

	// Move speed override access
	void SetDesiredMoveRateOverride(float fSpeed) { m_fDesiredMoveRateOverride = fSpeed; }
	float GetDesiredMoveRateOverride() const { return m_fDesiredMoveRateOverride; }
	void SetCurrentMoveRateOverride(float fSpeed) { m_fCurrentMoveRateOverride = fSpeed; }
	float GetCurrentMoveRateOverride() const { return m_fCurrentMoveRateOverride; }

	// In water move speed override access
	void SetDesiredMoveRateInWaterOverride(float fSpeed) { m_fDesiredMoveRateInWaterOverride = fSpeed; }
	float GetDesiredMoveRateInWaterOverride() const { return m_fDesiredMoveRateInWaterOverride; }
	void SetCurrentMoveRateInWaterOverride(float fSpeed) { m_fCurrentMoveRateInWaterOverride = fSpeed; }
	float GetCurrentMoveRateInWaterOverride() const { return m_fCurrentMoveRateInWaterOverride; }
	
			StationaryAimPose& GetStationaryAimPose()		{ return m_StationaryAimPose; }
	const	StationaryAimPose& GetStationaryAimPose() const	{ return m_StationaryAimPose; }
	
			AimPoseTransition& GetAimPoseTransition()		{ return m_AimPoseTransition; }
	const	AimPoseTransition& GetAimPoseTransition() const	{ return m_AimPoseTransition; }

	void SetRepositionMoveTarget(const bool bUseRepositionMoveTarget, const Vector3* pvRepositionMoveTarget = NULL) { m_bUseRepositionMoveTarget = bUseRepositionMoveTarget; if(pvRepositionMoveTarget) m_vRepositionMoveTarget = *pvRepositionMoveTarget; }
	bool GetUseRepositionMoveTarget() const { return m_bUseRepositionMoveTarget; }
	const Vector3& GetRepositionMoveTarget() const { return m_vRepositionMoveTarget; }

	fwMvClipSetId GetOverrideDriveByClipSet() const { return m_overrideDriveByClipSet; }	
	void SetOverrideDriveByClipSet(const fwMvClipSetId &clipSetId) { animAssert(clipSetId != CLIP_SET_ID_INVALID); m_overrideDriveByClipSet = clipSetId; }	
	void ResetOverrideDriveByClipSet() {m_overrideDriveByClipSet = CLIP_SET_ID_INVALID;}

	fwMvClipSetId GetOverrideMotionInCoverClipSet() const { return m_overrideMotionInCoverClipSet; }	
	void SetOverrideMotionInCoverClipSet(const fwMvClipSetId &clipSetId) { animAssert(clipSetId != CLIP_SET_ID_INVALID); m_overrideMotionInCoverClipSet = clipSetId; }	
	void ResetOverrideMotionInCoverClipSet() {m_overrideMotionInCoverClipSet = CLIP_SET_ID_INVALID;}

	fwMvClipSetId GetOverrideFallUpperBodyClipSet() const { return m_overrideFallUpperBodyClipSet; }	
	void SetOverrideFallUpperBodyClipSet(const fwMvClipSetId &clipSetId) { animAssert(clipSetId != CLIP_SET_ID_INVALID); m_overrideFallUpperBodyClipSet = clipSetId; }	
	void ResetOverrideFallUpperBodyClipSet() {m_overrideFallUpperBodyClipSet = CLIP_SET_ID_INVALID;}

	void GetCustomIdleClip(fwMvClipSetId &clipSetId, fwMvClipId &clipId) const { clipSetId = m_CustomIdleRequestClipSet;  clipId = m_CustomIdleRequestClip;}	
	void RequestCustomIdle(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId) { animAssert(clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID); m_CustomIdleRequestClipSet = clipSetId; m_CustomIdleRequestClip = clipId;}	
	void ResetCustomIdleClipSet() {m_CustomIdleRequestClipSet = CLIP_SET_ID_INVALID;}
	bool IsCustomIdleClipRequested() {return m_CustomIdleRequestClipSet != CLIP_SET_ID_INVALID;}

	bool GetFacingLeftInCover() const { return m_bFaceLeftInCover; }
	void SetFacingLeftInCover(bool bFaceLeftInCover) { m_bFaceLeftInCover = bFaceLeftInCover; }

	bool GetDesiredHeadingSetThisFrame() const { return m_bDesiredHeadingSetThisFrame; }

	bool GetCombatRoll() const { return m_bCombatRoll; }
	void SetCombatRoll(bool bCombatRoll) { m_bCombatRoll = bCombatRoll; }

	u32 GetIndependentMoverTransition() const { return m_uIndependentMoverTransition; }
	void SetIndependentMoverTransition(const u32 uValue) { m_uIndependentMoverTransition = uValue; }
	float GetIndependentMoverHeadingDelta() const { return m_fIndependentMoverHeadingDelta; }
	void SetIndependentMoverHeadingDelta(const float fValue) { m_fIndependentMoverHeadingDelta = fValue; }

	// FPS motion states
	enum eFPSState
	{
		FPS_IDLE,
		FPS_RNG,
		FPS_LT,
		FPS_SCOPE,
		FPS_UNHOLSTER,
		FPS_MAX,
	};

	void SetSteerBias(float fSteerBias) { m_fSteerBias = fSteerBias; }
	float GetSteerBias() const { return m_fSteerBias; }

	void SetDesiredSteerBias(float fDesiredSteerBias) { m_fDesiredSteerBias = fDesiredSteerBias; }
	float GetDesiredSteerBias() const { return m_fDesiredSteerBias; }

	void SetUsingAnalogStickRun(bool b) { m_bUsingAnalogStickRun = b; }
	bool GetUsingAnalogStickRun() const { return m_bUsingAnalogStickRun; }

	void SetFPSCheckRunInsteadOfSprint(bool b) { m_bFPSCheckRunInsteadOfSprint = b; }
	bool GetFPSCheckRunInsteadOfSprint() const { return m_bFPSCheckRunInsteadOfSprint; }

private:

	//*********************************************************************
	// This sets the passed in move blend ratio vector 
	void SetCurrentMoveBlendRatio(Vector2& vMoveBlendRatio, const float fMoveBlendRatio, const float fLateralMoveBlendRatio, const bool bIgnoreScriptedMBR = false);

	Vector3 m_vRepositionMoveTarget;

	// Offset the ped's capsule bounds if the animation suits
	// e.g. falling down
	Vector3 m_vBoundOffset;
	Vector3 m_vDesiredBoundOffset;

	// Heading
	float m_fCurrentHeading;
	float m_fDesiredHeading;
	float m_fHeadingChangeRate;

	// Pitch
	float m_fCurrentPitch;
	float m_fDesiredPitch;

	// Pitch/yaw the ped's capsule bounds if the animation suits
	// e.g. swimming, falling down, etc.
	float m_fBoundPitch;
	float m_fDesiredBoundPitch;
	float m_fBoundHeading;
	float m_fDesiredBoundHeading;

	// The ratio of movement where (0=still, 1=walk, 2=run, 3=sprint)
	Vector2 m_vCurrentMoveBlendRatio;
	// This member is the variable which ped movement tasks interface with to set a ped's desired speed
	Vector2 m_vDesiredMoveBlendRatio;
	// The minimum/maximum move blend ratio we are allowed to move at, reset every frame
	float m_fScriptedMaxMoveBlendRatio;
	float m_fScriptedMinMoveBlendRatio;

	//Gait reduction data
	u32   m_GaitReductionBlockTime;
	float m_fGaitReduction;	
	float m_fGaitReducedMaxMoveBlendRatio; // The maximum move blend ratio we are allowed to move at	
	float m_fGaitReductionHeading;

	// The current turn (heading change) velocity that should be obtained from anims + extra turn
	float m_fCurrentTurnVelocity;
	// Sometimes we may need to manually force the ped to turn faster than their move-blend may allow.
	// We add this value onto the the ped's heading each frame inside CPed::CalculateNewVelocity(),
	// as well as the m_animHeadingChange value derived from the animation.
	float m_fExtraHeadingChangeThisFrame;
	// Sometimes we may need to manually force the ped to pitch. We add this value onto the the ped's pitch each frame inside CPed::SetHeading.
	float m_fExtraPitchChangeThisFrame;
	// Store the heading delta for the independent mover transition
	float m_fIndependentMoverHeadingDelta;

	// TODO - should these live in the ped? Or in the On Foot motion task?
	u32 m_iCrouchStartTime;
	s32 m_iCrouchDuration;

	// Stores the override motion state set for the entity this frame
	s32 m_iStateForcedThisFrame;

	// For optionally overriding the driveby Clipset
	fwMvClipSetId	m_overrideDriveByClipSet;

	fwMvClipSetId	m_overrideMotionInCoverClipSet;

	// Custom Idle requests
	fwMvClipSetId	m_CustomIdleRequestClipSet;
	fwMvClipId		m_CustomIdleRequestClip;

	// Override fall clip set.
	fwMvClipSetId	m_overrideFallUpperBodyClipSet;

	// Flying motion status
	enum eFlyingState
	{
		FLYING_INACTIVE,
		FLYING_FORWARDS,
		FLYING_UPWARDS,
		FLYING_DOWNWARDS,
	};
	u32 m_iFlyingState;

	u32 m_bUseExtractedZ					:1;
	u32 m_bPlayerHasControlOfPedThisFrame	:1;
	u32 m_bPlayerHadControlOfPedLastFrame	:1;
	u32 m_bForceSteepSlopeTestThisFrame		:1;
	u32 m_bWasCrouching						:1;		// Was the ped crouching last frame? Used to trigger canned crouch->idle transition anims
	u32 m_bUsingStealth						:1;
	u32 m_bStrafing							:1;		// The ped is in strafing mode
	u32 m_bUseRepositionMoveTarget			:1;
	u32	m_bFaceLeftInCover					:1;
	u32 m_uIndependentMoverTransition		:2;
	u32 m_bDesiredHeadingSetThisFrame		:1;
	u32 m_bCombatRoll						:1;

	float	m_fCurrentMoveRateOverride;
	float	m_fDesiredMoveRateOverride;
	float	m_fCurrentMoveRateInWaterOverride;
	float	m_fDesiredMoveRateInWaterOverride;
	
	StationaryAimPose	m_StationaryAimPose;
	AimPoseTransition	m_AimPoseTransition;

	fwFlags8 m_iGaitReductionFlags;

	// FPS motion state variables
	u32 m_iFPSState;
	u32 m_iFPSPreviousState;
	bool m_bIsUsingStealthInFPS;
	bool m_bWasUsingStealthInFPS;
	float m_fSteerBias;
	float m_fDesiredSteerBias;
	bool  m_bUsingFPSMode;
	bool  m_bWasUsingFPSMode;
	bool  m_bUsingAnalogStickRun;
	bool  m_bFPSCheckRunInsteadOfSprint;
};

#endif // INC_PED_MOTION_DATA_H
