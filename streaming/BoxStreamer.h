//
// filename:	BoxStreamer.h
// description:	CBoxStreamer is a class for managing when assets are streamed based on the players position in
//				the world. Bounding boxes are setup for each asset and then these assets are streamed based on positions
//				entered into the class. You can stream entries based on multiple positions by calling SetRequired() with
//				multiple positions and then calling RequestRequired() will request/remove all the assets that have been
//				flagged
//

#ifndef STREAMING_BOXSTREAMER_H_
#define STREAMING_BOXSTREAMER_H_

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "vector/color32.h"
#include "atl/array.h"
#include "spatialdata/aabb.h"

// Framework headers
#include "fwscene/stores/boxstreamer.h"
#include "fwutil/quadtree.h"

// Game headers
#include "scene/RegdRefTypes.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

namespace rage { class strStreamingModule;}

struct CGTABoxStreamerInterfaceNew : public fwBoxStreamerInterfaceNew
{
	virtual bool IsExportingCollision();
	virtual void GetGameSpecificSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags);
};

// track most recently piloted aircraft for purposes of requesting collision around it
class CRecentlyPilotedAircraft
{
public:
	static void Update();
	static void AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags);
private:

	static RegdVeh ms_pRecentlyPilotedAircraft;
	static bool ms_bActive;
};

#endif // !STREAMING_BOXSTREAMER_H_
