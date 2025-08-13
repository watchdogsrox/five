// 
// audio/music/musicaction.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_MUSICACTION_H
#define AUD_MUSICACTION_H

#include "fwtl/pool.h"

#if !RSG_FINAL
#define Implement_MusicActionName(x) const char *x::GetName() const { return audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_Metadata->NameTableOffset); }
#else
#define Implement_MusicActionName(x)
#endif // !RSG_FINAL

class audMusicAction
{
public:
	FW_REGISTER_CLASS_POOL(audMusicAction);

	virtual ~audMusicAction() {}

	enum PrepareState
	{
		PREPARED,
		PREPARING,
		PREPARE_FAILED
	};

	virtual PrepareState Prepare() { return PREPARED; }
	virtual void Trigger() = 0;
	virtual void Cancel() {}
	
};

#endif // AUD_MUSICACTION_H
