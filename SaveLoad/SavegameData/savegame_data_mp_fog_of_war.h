#ifndef SAVEGAME_DATA_MP_FOG_OF_WAR_H
#define SAVEGAME_DATA_MP_FOG_OF_WAR_H

// Rage headers
#include "atl/array.h"
#include "parser/macros.h"



// *************************** Island Heist Fog of War MP ************************************************
#define SAVE_MULTIPLAYER_FOG_OF_WAR	(0)

#if SAVE_MULTIPLAYER_FOG_OF_WAR
class CMultiplayerFogOfWarSaveStructure
{
public :
	// This array stores the values for a smaller rectangle within the full 128*128 FoW array
	atArray< u8 > m_FogOfWarValues;

	// The following four values are indices within the full 128*128 FoW array
	u8 m_MinX;
	u8 m_MinY;
	u8 m_MaxX;
	u8 m_MaxY;

	u8 m_FillValueForRestOfMap;
	bool m_Enabled;

	// PURPOSE:	Constructor
	CMultiplayerFogOfWarSaveStructure();

	// PURPOSE:	Destructor
	virtual ~CMultiplayerFogOfWarSaveStructure();

	void PreSave();
	void PostLoad();

	PAR_PARSABLE;
};
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

#endif // SAVEGAME_DATA_MP_FOG_OF_WAR_H

