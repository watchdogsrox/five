/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedTuning.h
// PURPOSE :	A class to store global tuning data related to peds.  Data pulled
//				out of CPedBase.
// AUTHOR :		Mike Currington.
// CREATED :	29/10/10
/////////////////////////////////////////////////////////////////////////////////
#include "Peds/PedTuning.h"

// rage includes

// game includes
#include "modelinfo/PedModelInfo.h"

AI_OPTIMISATIONS()

#define PED_IN_CAR_LOD_THRESHOLD		(15.0f)
#define PLAYER_PED_CAR_LOD_THRESHOLD	(30.0f)
#define DRIVEBY_LOD_THRESHOLD			(55.0f)
#define PED_PROPS_LOD_THRESHOLD			(35.0f)
#define LOD_CROSSFADE_DIST				(3.0f)
#define SLOD_CROSSFADE_DIST				(5.0f)


float CPedTuning::ms_inCarLodThreshold = PED_IN_CAR_LOD_THRESHOLD;
float CPedTuning::ms_playerInCarLodThreshold = PLAYER_PED_CAR_LOD_THRESHOLD;
float CPedTuning::ms_drivebyLodThreshold = DRIVEBY_LOD_THRESHOLD;
float CPedTuning::ms_propsLodThreshold = PED_PROPS_LOD_THRESHOLD;
float CPedTuning::ms_LodCrossFadeDist = LOD_CROSSFADE_DIST;
float CPedTuning::ms_SlodCrossFadeDist = SLOD_CROSSFADE_DIST;

float CPedTuning::ms_superLodThreshold = PED_SUPER_LOD_THRESHOLD;
float CPedTuning::ms_lowLodThreshold = PED_LOW_LOD_THRESHOLD;
float CPedTuning::ms_lodThreshold = PED_LOD_THRESHOLD;
