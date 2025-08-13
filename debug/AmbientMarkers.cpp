/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AmbientMarkers.cpp
// PURPOSE : debug stuff to display ambient markers on the pause map
// AUTHOR  : Derek Payne
// STARTED : 10/07/2006
//
/////////////////////////////////////////////////////////////////////////////////

#if __DEV && 0

// C++ headers

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/bank.h"
#include "bank/combo.h"

// Game headers
#include "debug\AmbientMarkers.h"
#include "debug\debug.h"
#include "system\filemgr.h"

static atArray<const char*> s_MarkerNames;
static s32 s_MarkerSelected;

sMarkerInfo CAmbientMarkers::MarkerInfo[MAX_MARKERS];

s32 CAmbientMarkers::iMaxMarkers;
s32 CAmbientMarkers::iMaxTypes;
bool CAmbientMarkers::bDisplayMarkers;
bool CAmbientMarkers::bShowAllMarkers;

s32 CAmbientMarkers::iMarkerToShow;



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	Init
// PURPOSE:	sets up the class
/////////////////////////////////////////////////////////////////////////////////////
void CAmbientMarkers::Init(unsigned /*initMode*/)
{
	iMaxMarkers = 0;
	iMaxTypes = 0;
	bDisplayMarkers = false;
	bShowAllMarkers = true;
	iMarkerToShow = 1;
	s_MarkerSelected = 0;

	GetDatafromIpl();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	Shutdown
// PURPOSE:	shuts the class down
/////////////////////////////////////////////////////////////////////////////////////
void CAmbientMarkers::Shutdown(unsigned /*shutdownMode*/)
{
	s_MarkerNames.Reset();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	GetDatafromIpl
// PURPOSE:	reads in the data from the "ambient.ipl" file.   Stores it in a structure
/////////////////////////////////////////////////////////////////////////////////////
void CAmbientMarkers::GetDatafromIpl()
{
	FileHandle fid;
	char* pLine;
	s32 c = 0;
	s32 iNewType = 1;
	bool bFound_rtfx_marker = false;

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFile("common:/DATA/MAPS/AMBIENT.IPL", "rb");
	if(!CFileMgr::IsValidFileHandle(fid))
		return;

	while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
	{
		if(*pLine == '#' || *pLine == '\0') continue;

		if (!bFound_rtfx_marker)
		{
			// seek to the correct position in the ipl file...
			if (!strcmp(pLine, "rtfx"))
				bFound_rtfx_marker = true;
		}
		else
		{
			if (!strcmp(pLine, "end"))
			{
				bFound_rtfx_marker = false;
			}
			else
			{
				// grab the data we need from the line, ignoring the stuff we don't need
				sscanf(pLine, "%f%*c %f%*c %*s %*s %*s %*s %*s %s", &MarkerInfo[c].vPos.x, &MarkerInfo[c].vPos.y, &MarkerInfo[c].cName[0]);

				MarkerInfo[c].iType = 0;  // reset the type so we know

				// find out we we have had this type before, if so, use the existing type
				for (s32 c2 = 0; c2 < c; c2++)
				{
					if ((c != c2) && (!strcmp(MarkerInfo[c].cName, MarkerInfo[c2].cName)))
					{
						MarkerInfo[c].iType = MarkerInfo[c2].iType;
						break;  // out of loop
					}
				}

				if (MarkerInfo[c].iType == 0)  // if type is not set, then give it a rage_new type
					MarkerInfo[c].iType = iNewType++;

				c++;  // move onto next marker data
			}
		}
	}

	CFileMgr::CloseFile(fid);

	iMaxMarkers = c;
	iMaxTypes = iNewType;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	InitWidgets
// PURPOSE:	creates a widget bank that uses the data
/////////////////////////////////////////////////////////////////////////////////////
#if __BANK
void CAmbientMarkers::InitWidgets()
{
	if (iMaxMarkers == 0 || iMaxTypes == 0)  // if there are no markers read in, then don't create the bank
		return;

	bkBank& bank = BANKMGR.CreateBank("Ambient Markers");

	bank.AddToggle("Display Markers", &bDisplayMarkers);
	bank.AddToggle("Show All Markers", &bShowAllMarkers);

	s_MarkerNames.Reset();

	for(s32 i=1;i<iMaxTypes;i++)
	{		
		// go thru all the markers and get the 1st one of this type (only need one)
		for(s32 j=0;j<iMaxMarkers;j++)
		{
			if (MarkerInfo[j].iType == i)
			{
				s_MarkerNames.PushAndGrow(MarkerInfo[j].cName);
				break;
			}
		}
	}

	bank.AddCombo("Marker", &s_MarkerSelected, s_MarkerNames.GetCount(), &s_MarkerNames[0], UpdateMarkerToShow);
}
#endif  // __BANK


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	UpdateMarkerToShow
// PURPOSE:	sets the marker we want to show after user has clicked it from the combo
/////////////////////////////////////////////////////////////////////////////////////
void CAmbientMarkers::UpdateMarkerToShow()
{
	bShowAllMarkers = false;

	iMarkerToShow = s_MarkerSelected + 1;  // set the marker id we are going to show
}

#endif // #if __DEV

// eof
