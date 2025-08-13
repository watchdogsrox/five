#ifndef GUN_IK_INFO_H
#define GUN_IK_INFO_H

// Forward declarations
class CPed;

//////////////////////////////////////////////////////////////////////////
// CGunIkInfo
//////////////////////////////////////////////////////////////////////////

class CGunIkInfo
{
public:
	CGunIkInfo();
	CGunIkInfo(float fTorsoOffsetYaw, float fTorsoMinYaw, float fTorsoMaxYaw, float fTorsoMinPitch, float fTorsoMaxPitch);

	// Set the default, yaw and pitch to be used by the torso Ik
	void ApplyIkInfo(CPed* pPed);

	//
	// Settors
	//

	// Set the offset yaw
	void SetTorsoOffsetYaw(float fTorsoOffsetYaw) { m_fTorsoOffsetYaw = fTorsoOffsetYaw; }

	// Set the min/max yaw
	void SetTorsoMinMaxYaw(float fTorsoMinYaw, float fTorsoMaxYaw) { m_fTorsoMinYaw = fTorsoMinYaw; m_fTorsoMaxYaw = fTorsoMaxYaw; }

	// Set the min/max pitch
	void SetTorsoMinMaxPitch(float fTorsoMinPitch, float fTorsoMaxPitch) { m_fTorsoMinPitch = fTorsoMinPitch; m_fTorsoMaxPitch = fTorsoMaxPitch; }

	// Enable/disable pitch ik fixup
	void SetDisablePitchFixUp(bool disablePitchFixUp) { m_bDisablePitchFixUp = disablePitchFixUp; }

	// Override spine matrix interp rate
	void SetSpineMatrixInterpRate(float fInterpRate) { m_fSpineMatrixInterpRate = fInterpRate; }

private:

	// The weapon is rotated by this much in the animation
	// rotate the torso with respect to this offset
	float m_fTorsoOffsetYaw;

	// The min and max angle the torso can rotate
	float m_fTorsoMinYaw;
	float m_fTorsoMaxYaw;
	float m_fTorsoMinPitch;
	float m_fTorsoMaxPitch;
	float m_fSpineMatrixInterpRate;
	bool  m_bDisablePitchFixUp;
};

#endif // GUN_IK_INFO_H
