//
// renderer/psnvramrendertargettotal.h
//
// Copyright (C) 2013-2013 Rockstar Games.  All Rights Reserved.
//

////////////////////////////////////////////////////////////////////////////////
// This file needs to be standalone so that Natural Motion builds do not      //
// require shipping all the header files.                                     //
////////////////////////////////////////////////////////////////////////////////

#ifndef RENDERER_PSNVRAMRENDERTARGETTOTAL_H
#define RENDERER_PSNVRAMRENDERTARGETTOTAL_H

#if RSG_PS3

#include "renderer/UseTreeImposters.h"

#if USE_TREE_IMPOSTERS
#	define PSN_VRAM_RENDER_TARGET_TOTAL_KB          (48896)
#else
#	define PSN_VRAM_RENDER_TARGET_TOTAL_KB          (47552)
#endif

#endif // RSG_PS3

#endif // RENDERER_PSNVRAMRENDERTARGETTOTAL_H
