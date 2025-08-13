// 
// audio/music/vehiclemusic.h 
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_VEHICLEMUSIC_H
#define AUD_VEHICLEMUSIC_H

class audVehicleMusic
{
public:

	void Init();
	void Shutdown();
	void StopMusic();	

	void Update(u32 timeInMs);
	void NotifyPlaneCrashed();
	void NotifyPlayerBailed();

private:

	bool TriggerEvent(const char *eventName);
	void Reset();

	void StartMusic();
	void StopMusic(const char *eventName);

	float m_InhibitTimer;
	float m_StopTimer;
	bool m_StartedMusic;

	enum MusicType
	{
		Flying = 0,
		Underwater,

		None
	}m_MusicType;
};

#endif // AUD_VEHICLEMUSIC_H
