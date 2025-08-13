/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/worldpoints.h
// PURPOSE : Holds an array of coords (read from the 2d effects data) ordered by sector
//				These coords will never change so the array is only suitable for holding 2d effects
//				attached to objects that can't be moved.
// AUTHOR :  Graeme
// CREATED : 9/3/06
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _WORLDPOINTS_H_
#define _WORLDPOINTS_H_

#include "fwscene/world/WorldLimits.h"

#define WORLDPOINT_SCAN_RANGE	(210.0f)

// GSW - NUM_WORLD_2D_EFFECTS is currently 10900. Dividing the world into a 20x20 grid seems reasonable.
//	If the 2d effects are fairly evenly distributed then the lists for each sector should be < 30 and we're not
//	wasting too much memory on the array of CWorld2dEffect_Sectors either.
#define WORLD2DEFFECT_WIDTHINSECTORS	(20)
#define WORLD2DEFFECT_DEPTHINSECTORS	(20)

#define WORLD2DEFFECT_WIDTHOFSECTOR ( (WORLDLIMITS_REP_XMAX - WORLDLIMITS_REP_XMIN) / WORLD2DEFFECT_WIDTHINSECTORS)
#define WORLD2DEFFECT_DEPTHOFSECTOR ( (WORLDLIMITS_REP_YMAX - WORLDLIMITS_REP_YMIN) / WORLD2DEFFECT_DEPTHINSECTORS)

#define WORLD2DEFFECT_WORLDTOSECTORX(x) ((s32) rage::Floorf(((x) / WORLD2DEFFECT_WIDTHOFSECTOR) + (WORLD2DEFFECT_WIDTHINSECTORS / 2.0f)))
#define WORLD2DEFFECT_WORLDTOSECTORY(y) ((s32) rage::Floorf(((y) / WORLD2DEFFECT_DEPTHOFSECTOR) + (WORLD2DEFFECT_DEPTHINSECTORS / 2.0f)))


class CWorld2dEffect_Sector
{
protected:
	s16 FirstWorld2dEffectIndex;
	s16 LastWorld2dEffectIndex;

public:
	void SetFirstWorld2dEffectIndex(int FirstIndex) { Assert(FirstIndex<32768);FirstWorld2dEffectIndex = (s16)FirstIndex; };
	void SetLastWorld2dEffectIndex(int LastIndex) { Assert(LastIndex<32768); LastWorld2dEffectIndex = (s16)LastIndex; }

	int GetFirstWorld2dEffectIndex() const { return FirstWorld2dEffectIndex; }
	int GetLastWorld2dEffectIndex() const { return LastWorld2dEffectIndex; }
};

class CWorldPoints
{
public:

enum {	OUT_OF_BRAIN_RANGE = 0,
		WAITING_TILL_OUT_OF_BRAIN_RANGE,
		WAITING_FOR_BRAIN_TO_LOAD,
		RUNNING_BRAIN };

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void RemoveScriptBrain(s16 brainIndex);

	static void SortWorld2dEffects();

	static void ScanForScriptTriggerWorldPoints(const Vector3 &vecCentre);

	//	Brenda asked for a command to help with debugging that will immediately set brains back to OUT_OF_BRAIN_RANGE
	//	so that she doesn't have to warp away from a coord and then back again to re-trigger the brain.
	static void ReactivateAllWorldPointBrainsThatAreWaitingTillOutOfRange();

	static void ReactivateNamedWorldPointBrainsWaitingTillOutOfRange(const char *pNameToSearchFor);

#if __DEV
	static void RenderDebug();
#endif

public:
	static CWorld2dEffect_Sector& GetWorld2dEffectSector(s32 x, s32 y);

private:
	static CWorld2dEffect_Sector m_aWorld2dEffectSectors[WORLD2DEFFECT_WIDTHINSECTORS * WORLD2DEFFECT_DEPTHINSECTORS] ;
	static s32 ms_currentSectorX;
	static s32 ms_currentSectorY;
	static s32 ms_currentSectorCurrentIndex;
};

inline CWorld2dEffect_Sector& CWorldPoints::GetWorld2dEffectSector(s32 x, s32 y)
{
	x = MAX(x, 0);
	x = MIN(x, WORLD2DEFFECT_WIDTHINSECTORS-1);
	y = MAX(y, 0);
	y = MIN(y, WORLD2DEFFECT_DEPTHINSECTORS-1);


	return m_aWorld2dEffectSectors[x + y * WORLD2DEFFECT_WIDTHINSECTORS];
}

#endif

