// FILE :    PedStealth.h
// PURPOSE : Used to store attributes assoaciated with how stealthly a ped currently is
// CREATED : 19-11-2008
#ifndef PED_STEALTH_H
#define PED_STEALTH_H

// Rage headers
#include "system/bit.h"

// Framework headers
#include "fwutil/Flags.h"

// Game headers


class CPed;

// Defines an individual peds Stealth level.
class CPedStealth
{
public:
#if !__FINAL
	static bool ms_bRenderDebugStealthLevel; 
#endif // !__FINAL

	typedef enum
	{
		SF_TreatedAsActingSuspiciously	= BIT0,
		SF_PedGeneratesFootStepEvents	= BIT1
	} StealthFlags;

	CPedStealth();
	~CPedStealth();

	// Per frame update
	void Update(float fTimeStep);

	// Spotted this frame, marks this ped as having been spotted
	void SpottedThisFrame();

	// Accessors for the stealth flags.
	inline fwFlags32& GetFlags() { return m_stealthFlags; }
	inline const fwFlags32& GetFlags() const { return m_stealthFlags; }

private:
	float		m_fStealthLevel;
	fwFlags32	m_stealthFlags;
};

#endif //PED_STEALTH_H
