/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AmbientMarkers.h
// PURPOSE : debug stuff to display ambient markers on the pause map
// AUTHOR  : Derek Payne
// STARTED : 10/07/2006
//
/////////////////////////////////////////////////////////////////////////////////


#ifndef INC_AMBIENTMARKERS_H
#define INC_AMBIENTMARKERS_H

#if __DEV && 0

// Rage headers
#include "vector/matrix34.h"
//
// defines:
//
#define MAX_MARKERS		(100)  // max number of markers we support
#define MAX_NAME_LEN	(20)   // number of chars a name can have

//
// structure to store the marker data:
//
typedef struct 
{
	Vector2 vPos;  // position
	char cName[MAX_NAME_LEN];  // name
	s32 iType;  // type of marker
} sMarkerInfo;


// CAmbientMarkers class:
class CAmbientMarkers
{

public:

	static sMarkerInfo MarkerInfo[MAX_MARKERS];
	
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	// access functions:
	static bool DisplayMarkers() { return bDisplayMarkers; }
	static bool DisplayAllMarkers() { return bShowAllMarkers; }
	static s32 GetNumberOfMarkers() { return iMaxMarkers; }

	static Vector2 GetPosition(s32 iIndex) { return MarkerInfo[iIndex].vPos; }
	static char *GetName(s32 iIndex) { return MarkerInfo[iIndex].cName; }
	static s32 GetType(s32 iIndex) { return MarkerInfo[iIndex].iType; }

	static s32 GetMarkerToShow() { return iMarkerToShow; }

#if __BANK
	static void InitWidgets();
#endif

private:
	static void GetDatafromIpl();
	static void UpdateMarkerToShow();

	static s32 iMaxMarkers;
	static s32 iMaxTypes;
	static s32 iMarkerToShow;

	static bool bDisplayMarkers;
	static bool bShowAllMarkers;
};

#endif  // #if __DEV

#endif  // INC_AMBIENTMARKERS_H

// eof
