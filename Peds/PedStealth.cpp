// FILE :    PedStealth.cpp
// PURPOSE : Used to store attributes associated with how stealthed a ped currently is
// CREATED : 19-11-2008

// associated header
#include "Peds/Ped.h"
#include "Peds/PedStealth.h"
#include "renderer/lights/lights.h"
#include "debug/VectorMap.h"

// rage headers
#include "Profile/timebars.h"

AI_OPTIMISATIONS()

#if !__FINAL
	bool CPedStealth::ms_bRenderDebugStealthLevel = false; 
#endif // !__FINAL

CPedStealth::CPedStealth()
: m_fStealthLevel(1.0f)
, m_stealthFlags(0)
{

}

CPedStealth::~CPedStealth()
{
}

void CPedStealth::Update( float fTimeStep )
{	
	// Updates the peds stealth level
	static float sfStealthChangePerSecond = 0.2f;
	m_fStealthLevel += sfStealthChangePerSecond*fTimeStep;
	m_fStealthLevel = MIN(m_fStealthLevel, 1.0f);
}

void CPedStealth::SpottedThisFrame()
{
	static float sfStealthChangeFromBeingSpotted = -0.05f;
	m_fStealthLevel += sfStealthChangeFromBeingSpotted;
	m_fStealthLevel = MAX(m_fStealthLevel, 0.0f);
}
