//
// Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SCRIPTED_GUN_TASK_INFO_H
#define INC_SCRIPTED_GUN_TASK_INFO_H

////////////////////////////////////////////////////////////////////////////////

// Rage Headers
#include "fwanimation/animdefines.h"
#include "fwtl/pool.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

/////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Scripted gun task data driven info
////////////////////////////////////////////////////////////////////////////////

class CScriptedGunTaskInfo
{
public:

	// Scripted Gun Task Flags
	enum eScriptedGunTaskFlags
	{
		AllowWrapping					= BIT0,
		AbortOnIdle						= BIT1,
		UseAdditiveFiring				= BIT2,
		UseDirectionalBreatheAdditives	= BIT3,
		TrackRopeOrientation			= BIT4,
		ForcedAiming					= BIT5,
		ForceFireFromCamera				= BIT6
	};

	// PURPOSE: Constructor
	CScriptedGunTaskInfo();

	// PURPOSE: Destructor
	~CScriptedGunTaskInfo();

	// PURPOSE: Get the name of this scripted gun task info
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// PURPOSE: Clip authored min/max sweep values needed to determine the normalised clip phase parameter passed into the move network
	float GetMinAimSweepHeadingAngleDegs() const { return m_MinAimSweepHeadingAngleDegs; }						
	float GetMaxAimSweepHeadingAngleDegs() const { return m_MaxAimSweepHeadingAngleDegs; }
	float GetMinAimSweepPitchAngleDegs() const { return m_MinAimSweepPitchAngleDegs; }						
	float GetMaxAimSweepPitchAngleDegs() const { return m_MaxAimSweepPitchAngleDegs; }
	float GetHeadingOffset() const { return m_fHeadingOffset; }

	// PURPOSE: Get the clipset hash associated with the gun task
	fwMvClipSetId GetClipSet() const { return fwMvClipSetId(m_ClipSet.GetHash()); }

	atFinalHashString GetClipSetName() const { return m_ClipSet; }

	// PURPOSE: Get the idle camera associated with the gun task
	atHashWithStringNotFinal GetIdleCamera() const { return m_IdleCamera; }

	// PURPOSE: Get the camera associated with the gun task
	atHashWithStringNotFinal GetCamera() const { return m_Camera; }

	// PURPOSE: Ignore smoothing at extreme angles (to allow 360 aiming without returning to default)
	bool GetAllowWrapping() const { return m_Flags.IsFlagSet(AllowWrapping); }

	// PURPOSE: Modify ped's matrix to follow rope orientation when attached and dangling from one
	bool GetTrackRopeOrientation() const { return m_Flags.IsFlagSet(TrackRopeOrientation); }

	// PURPOSE: Abort the task if the ped returns to the idle state
	bool ShouldAbortOnIdle() const { return m_Flags.IsFlagSet(AbortOnIdle); }

	// PURPOSE: Decide whether to use additives
	bool ShouldUseAdditiveFiring() const { return m_Flags.IsFlagSet(UseAdditiveFiring); }
	bool ShouldUseDirectionalBreatheAdditives() const { return m_Flags.IsFlagSet(UseDirectionalBreatheAdditives); }

	// PURPOSE: Force the player in the aiming anim state
	bool GetForcedAiming() const { return m_Flags.IsFlagSet(ForcedAiming); }

	// PURPOSE: Force the player to not update aim alterations when aiming
	bool GetForceFireFromCamera() const { return m_Flags.IsFlagSet(ForceFireFromCamera); }

private:

	atHashWithStringNotFinal	m_Name;
	float						m_MinAimSweepHeadingAngleDegs;
	float						m_MaxAimSweepHeadingAngleDegs;
	float						m_MinAimSweepPitchAngleDegs;
	float						m_MaxAimSweepPitchAngleDegs;
	float						m_fHeadingOffset;
	atFinalHashString			m_ClipSet;	// needs to be available in final builds so that script can request the clip set name
	atHashWithStringNotFinal	m_IdleCamera;
	atHashWithStringNotFinal	m_Camera;
	fwFlags32					m_Flags;

	PAR_SIMPLE_PARSABLE
};

/////////////////////////////////////////////////////////////////////////////////

#endif // INC_SCRIPTED_GUN_TASK_INFO_H
