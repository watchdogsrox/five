#ifndef SAVEGAME_DATA_CAMERA_H
#define SAVEGAME_DATA_CAMERA_H

// Rage headers
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Game headers
#include "SaveLoad/savegame_defines.h"


// Forward declarations
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
class CCameraSaveStructure_Migration;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES



class CCameraSaveStructure
{	
public:
	atBinaryMap<atHashValue, atHashValue> m_ContextViewModeMap;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void Initialize(CCameraSaveStructure_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
class CCameraSaveStructure_Migration
{	
public:
//	atBinaryMap<atHashValue, atHashValue> m_ContextViewModeMap;
	atBinaryMap<u32, u32> m_ContextViewModeMap;	//	Use u32 instead of atHashValue

	void Initialize(CCameraSaveStructure &copyFrom);

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_DATA_CAMERA_H

