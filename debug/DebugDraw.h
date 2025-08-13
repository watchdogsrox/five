/////////////////////////////////////////////////////////////////////////////////
// FILE		: DebugDraw.h
// PURPOSE	: To provide in game visual helpers for debugging.
// AUTHOR	: G. Speirs, Adam Croston, Alex Hadjadj
// STARTED	: 10/08/99
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_DEBUGDRAW_H_
#define INC_DEBUGDRAW_H_

#include "grcore/debugdraw.h"

// rage headers
#if DEBUG_DRAW
namespace DebugDraw
{
	void Init();

	void Shutdown();

	#if __BANK
	void RenderLastGen();
	#endif
};
#endif // DEBUG_DRAW

#endif // INC_DEBUGDRAW_H_
