
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    audiozones.cpp
// PURPOSE : Stuff to deal with zones (boxes/spheres) that have sounds played for them.
// AUTHOR :  Obbe.
// CREATED : 9/6/2004
//
/////////////////////////////////////////////////////////////////////////////////

#include "audiozones.h"
#include "audio_channel.h"

#include "grcore/debugdraw.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "fwsys/timer.h"

AUDIO_OPTIMISATIONS()

CAudioSphere	CAudioZones::m_aSpheres[MAX_NUM_SPHERES];
CAudioBox		CAudioZones::m_aBoxes[MAX_NUM_BOXES];

s32	CAudioZones::m_NumSpheres;
s32	CAudioZones::m_NumBoxes;

s32	CAudioZones::m_NumActiveSpheres;
s32	CAudioZones::m_NumActiveBoxes;
	
s32	CAudioZones::m_aActiveSpheres[MAX_NUM_AT_ANY_ONE_TIME];
s32	CAudioZones::m_aActiveBoxes[MAX_NUM_AT_ANY_ONE_TIME];

bool	CAudioZones::m_bRenderAudioZones;


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :  Clears them all.
//
/////////////////////////////////////////////////////////////////////////////////

void CAudioZones::Init()
{
	m_NumSpheres = 0;
	m_NumBoxes = 0;
	m_NumActiveSpheres = 0;
	m_NumActiveBoxes = 0;
#if !__FINAL
// Var console has been removed
//	VarConsole.Add("Render Audio Zones",&m_bRenderAudioZones, true);
#endif
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Works out which zones are active.
//
/////////////////////////////////////////////////////////////////////////////////

Vector3	LastUpdateCoors;

void CAudioZones::Update(bool bForceUpdate, const Vector3& TestCoors)
{
	if ((!bForceUpdate) && ((fwTimer::GetSystemFrameCount() & 15) != 6) &&
		(TestCoors - LastUpdateCoors).Mag() < 20.0f) return;		// Only do this once in a while (slow)

	LastUpdateCoors = TestCoors;
	
	m_NumActiveSpheres = 0;
	m_NumActiveBoxes = 0;

	for (s32 s = 0; s < m_NumSpheres; s++)
	{
		if (m_aSpheres[s].m_bActive)
		{
			if ((TestCoors - m_aSpheres[s].m_Coors).Mag() < m_aSpheres[s].m_Range)
			{
				if (m_NumActiveSpheres < MAX_NUM_AT_ANY_ONE_TIME)
				{
					m_aActiveSpheres[m_NumActiveSpheres++] = s;
				}
			}
		}
	}

	for (s32 b = 0; b < m_NumBoxes; b++)
	{
		if (m_aBoxes[b].m_bActive)
		{
			if (TestCoors.x > m_aBoxes[b].GetMinX() && TestCoors.x < m_aBoxes[b].GetMaxX())
			{
				if (TestCoors.y > m_aBoxes[b].GetMinY() && TestCoors.y < m_aBoxes[b].GetMaxY())
				{
					if (TestCoors.z > m_aBoxes[b].GetMinZ() && TestCoors.z < m_aBoxes[b].GetMaxZ())
					{
						if (m_NumActiveBoxes < MAX_NUM_AT_ANY_ONE_TIME)
						{
							m_aActiveBoxes[m_NumActiveBoxes++] = b;
						}
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RegisterAudioSphere
// PURPOSE :  Adds an audiosphere to the list.
//
/////////////////////////////////////////////////////////////////////////////////

void CAudioZones::RegisterAudioSphere(char *pName, s32 Sound, bool bActive, float CoorsX, float CoorsY, float CoorsZ, float Range)
{
	naAssertf(m_NumSpheres < MAX_NUM_SPHERES, "We have exceeded the maximum number of audiospheres");
	naAssertf(strlen(pName) <= AZMAXNAMELENGTH, "Sphere name passed in as a parameter to RegisterAudioSphere is too long");

	strcpy(m_aSpheres[m_NumSpheres].m_Name, pName);
	m_aSpheres[m_NumSpheres].m_Sound = static_cast<s16>(Sound);
	m_aSpheres[m_NumSpheres].m_bActive = bActive;
	m_aSpheres[m_NumSpheres].m_Coors.x = CoorsX;
	m_aSpheres[m_NumSpheres].m_Coors.y = CoorsY;
	m_aSpheres[m_NumSpheres].m_Coors.z = CoorsZ;
	m_aSpheres[m_NumSpheres].m_Range = Range;
	m_NumSpheres++;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RegisterAudioBox
// PURPOSE :  Adds an audiobox to the list.
//
/////////////////////////////////////////////////////////////////////////////////

void CAudioZones::RegisterAudioBox(char *pName, s32 Sound, bool bActive, float MinCoorsX, float MinCoorsY, float MinCoorsZ, float MaxCoorsX, float MaxCoorsY, float MaxCoorsZ)
{
	naAssertf(m_NumBoxes < MAX_NUM_BOXES, "We have exceeded the maximum number of audioboxes");
	naAssertf(strlen(pName) <= AZMAXNAMELENGTH, "Box name passed in as a parameter to RegisterAudioSphere is too long");

	strcpy(m_aBoxes[m_NumBoxes].m_Name, pName);
	m_aBoxes[m_NumBoxes].m_Sound = static_cast<s16>(Sound);
	m_aBoxes[m_NumBoxes].m_bActive = bActive;
	m_aBoxes[m_NumBoxes].SetMinCoors( Vector3(MinCoorsX, MinCoorsY, MinCoorsZ) );
	m_aBoxes[m_NumBoxes].SetMaxCoors( Vector3(MaxCoorsX, MaxCoorsY, MaxCoorsZ) );
	m_NumBoxes++;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SwitchAudioZone
// PURPOSE :  Switches an audio zone on or off.
//
/////////////////////////////////////////////////////////////////////////////////

void CAudioZones::SwitchAudioZone(char *pName, bool bNewActive)
{
	bool	bChangeMade = false;
	// Find the zone.
	// Look in spheres first
	for (s32 s = 0; s < m_NumSpheres; s++)
	{
		if (!stricmp(pName, m_aSpheres[s].m_Name))
		{
			naCErrorf(!bChangeMade, "There are multiple audio zones with the same name\n");
			m_aSpheres[s].m_bActive = bNewActive;
			bChangeMade = true;
		}
	}
	// Now look in boxes
	for (s32 b = 0; b < m_NumBoxes; b++)
	{
		if (!stricmp(pName, m_aBoxes[b].m_Name))
		{
			naCErrorf(!bChangeMade, "There are multiple audio zones with the same name\n");
			m_aBoxes[b].m_bActive = bNewActive;
			bChangeMade = true;
		}
	}
	//Assertf(bChangeMade, "SwitchAudioZone called with name that doesn't exist\n");
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Render
// PURPOSE :  Renders some debug shit for them.
//
/////////////////////////////////////////////////////////////////////////////////

