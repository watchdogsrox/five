// 
// audio/music/wantedmusic.h 
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_WANTEDMUSIC_H
#define AUD_WANTEDMUSIC_H

class audScene;

class audWantedMusic
{
public:

	void Init();
	void Shutdown();
	void StopMusic();

	void Update(u32 timeInMs);

	void NotifyArrested();
	void NotifyDied();
	void NotifyCloseNetwork();

private:

	bool TriggerEvent(const char *eventName);
	void Reset();

	void StartScene();
	void StopScene();

	audScene *m_Scene;
	s32 m_LastWantedLevel;
	float m_DisabledTimer;
	bool m_StartedMusic;
	bool m_WereCopsSearching;
	bool m_WasOutsideCircle;
	bool m_TriggeredEnterVehicle;

};

#endif // AUD_WANTEDMUSIC_H
