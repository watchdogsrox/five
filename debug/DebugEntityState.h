#ifndef DEBUG_ENTITY_STATE_H 
#define DEBUG_ENTITY_STATE_H 

#include "physics/debugPlayback.h"
#if DR_ENABLED

#include "entity/entity.h"

namespace rage
{
	namespace debugPlayback
	{	
		void RecordFixedStateChange(const fwEntity& rEntity, const bool bFixed, const bool bNetwork);
	}
}

#endif //DR_ENABLED
#endif //DEBUG_ENTITY_STATE_H
