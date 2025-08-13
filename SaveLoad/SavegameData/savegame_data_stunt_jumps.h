#ifndef SAVEGAME_DATA_STUNT_JUMPS_H
#define SAVEGAME_DATA_STUNT_JUMPS_H


// Rage headers
#include "parser/macros.h"



class CStuntJumpSaveStructure
{
public :

	bool	m_bActive;

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};


#endif // SAVEGAME_DATA_STUNT_JUMPS_H

