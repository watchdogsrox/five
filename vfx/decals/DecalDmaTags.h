//
// vfx/decals/DecalDmaTags.h
//
// Copyright (C) 2012-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef DECALS_DECALDMATAGS_H
#define DECALS_DECALDMATAGS_H

#include "vfx/decal/decaldmatags.h"

#define DMATAG_GAME_GETSMASHGROUPVTXS   (DMATAG_INIT_GAME_BEGIN+1)
#define DMATAG_GAME_GETBONEFLAGS        (DMATAG_INIT_GAME_BEGIN+2)
#if DMATAG_GAME_GETBONEFLAGS >= DMATAG_INIT_GAME_END
#	error "out of range dma tag"
#endif

#endif // DECALS_DECALDMATAGS_H
