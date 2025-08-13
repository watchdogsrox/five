#ifndef _ANIMMANAGER_H_
#define _ANIMMANAGER_H_

// Rage headers
#include "paging/dictionary.h"

// Gta headers
#include "fwanimation/animmanager.h"

// Rage forward declaration
namespace rage
{
	class crClip;
}

class CPed;

// Defines
#define CLIP_DICT_INDEX_INVALID (-1)
#define CLIP_HASH_INVALID (0)

//
// fwAnimManager
// Storage and retrieval of anim files
//
class CGtaAnimManager : public fwAnimManager
{
public:

	// PURPOSE: Called on initialisation in game.cpp
	static void InitClass();
	
	// PURPOSE: 	
	static void Init(unsigned initMode);
    
	// PURPOSE: 
	static void Shutdown(unsigned shutdownMode);

	// PURPOSE: 
	static crFrameFilterMultiWeight* FindFrameFilter(const atHashWithStringNotFinal &hashString, const CPed* pPed);

	// PURPOSE: 
	virtual fwAnimDirector *GetAnimDirectorBeingViewed() const;

private:

};

#endif //_ANIMMANAGER_H_
