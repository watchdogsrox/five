// 
// audio/music/musicaction.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 


#include "musicaction.h"
#include "musicactionfactory.h"
#include "musicevent.h"

AUDIO_MUSIC_OPTIMISATIONS()

enum {kMaxActiveActions = 96};
FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(audMusicAction, kMaxActiveActions, 0.73f, atHashString("MusicAction",0xd31fcabc), (s32)audMusicActionFactory::GetMaxSize());
