#ifndef SAVEGAME_DATA_MP_TIMESTAMP_H
#define SAVEGAME_DATA_MP_TIMESTAMP_H

// Rage headers
#include "parser/macros.h"



class CPosixTimeStampForMultiplayerSaves
{
public :
	u32 m_PosixHigh;
	u32 m_PosixLow;

	// PURPOSE:	Constructor
	CPosixTimeStampForMultiplayerSaves();

	// PURPOSE:	Destructor
	virtual ~CPosixTimeStampForMultiplayerSaves() {}

	void PreSave();
	void PostLoad();

	PAR_PARSABLE;
};


#endif // SAVEGAME_DATA_MP_TIMESTAMP_H

