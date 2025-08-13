
#ifndef SCRIPT_SAVE_PSO_FILE_TRACKER_H
#define SCRIPT_SAVE_PSO_FILE_TRACKER_H

// Rage headers
#include "parser/psodata.h"


// Game headers
#include "SaveLoad/savegame_defines.h"

#if SAVEGAME_USES_PSO

// PURPOSE: Specialized hash function functor for atHashValue keys.
template <> struct atMapHashFn<atLiteralHashValue>
{
	unsigned operator ()(atLiteralHashValue key) const { return key.GetHash(); }
};

class CScriptSavePsoFileTracker
{
private:
	atMap<atLiteralHashValue, psoStruct> StructMap;

public:
	void Shutdown();
	void ForceCleanup();

	void Append(atLiteralHashValue name, psoStruct& str, u8* workBuffer);

	psoStruct* GetStruct(atLiteralHashValue name);

	void DeleteStruct(atLiteralHashValue name);

	//	atMap<atLiteralHashValue, psoStruct>& GetStructMap(){return StructMap;}

	//	bool IsEmpty();
};

#endif // SAVEGAME_USES_PSO

#endif // SCRIPT_SAVE_PSO_FILE_TRACKER_H

