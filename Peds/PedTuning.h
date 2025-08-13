/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedTuning.h
// PURPOSE :	A class to store global tuning data related to peds.  Data pulled
//				out of CPedBase.
// AUTHOR :		Mike Currington.
// CREATED :	29/10/10
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_PEDTUNING_H
#define INC_PEDTUNING_H

// rage includes
#include "vector/vector3.h"

// game includes

// forward declarations

class CPedTuning
{
public:
	static float	GetLodThreshold()						{ return ms_lodThreshold; }
	static float	GetLowLodThreshold()					{ return ms_lowLodThreshold; }
	static float	GetSuperLodThreshold()					{ return ms_superLodThreshold; }

	static float	GetInCarLodThreshold()					{ return ms_inCarLodThreshold; }
	static float	GetPlayerInCarLodThreshold()			{ return ms_playerInCarLodThreshold; }
	static float	GetDrivebyLodThreshold()				{ return ms_drivebyLodThreshold; }
	static float	GetPropsLodThreshold()					{ return ms_propsLodThreshold; }

	static float	GetLodCrossFadeDist()					{ return ms_LodCrossFadeDist; }
	static float	GetSlodCrossFadeDist()					{ return ms_SlodCrossFadeDist; }
	static void		SetLodThresholds(float hiLod, float medLod, float lowLod, float inCarLod)	
							{ ms_lodThreshold = hiLod; ms_lowLodThreshold = medLod; ms_superLodThreshold = lowLod;  ms_inCarLodThreshold = inCarLod; }

	static void		SetDebugLodThresholds(float hiLod, float lowLod, float superLod)	
							{ ms_lodThreshold = hiLod; ms_lowLodThreshold = lowLod; ms_superLodThreshold = superLod;}

private:

	static float	ms_superLodThreshold;
	static float	ms_lowLodThreshold;
	static float	ms_lodThreshold;

	static float	ms_inCarLodThreshold;
	static float	ms_playerInCarLodThreshold;
	static float	ms_drivebyLodThreshold;
	static float	ms_propsLodThreshold;
	static float	ms_LodCrossFadeDist;
	static float	ms_SlodCrossFadeDist;
};

#endif // INC_PEDTUNING_H
