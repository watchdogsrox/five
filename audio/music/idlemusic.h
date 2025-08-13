// 
// audio/music/idlemusic.h 
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_IDLEMUSIC_H
#define AUD_IDLEMUSIC_H

class audIdleMusic
{
public:

	void Init();
	void Shutdown();
	void StopMusic();
	void Update(u32 timeInMs);

	void NotifyPlayerFiredWeapon();
	
private:

	bool TriggerEvent(const char *eventName);
	void Reset();
	
	void StartMusic();
	void StopMusic(const char *eventName);

	u32 m_LastMusicTime;
	u32 m_NextMusicTime;
	u32 m_LastWeaponFireTime;
	u32 m_MusicPlayDuration;
	bool m_StartedMusic;
};

#endif // AUD_IDLEMUSIC_H
