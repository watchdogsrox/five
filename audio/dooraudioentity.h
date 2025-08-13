#ifndef AUD_DOORAUDIOENTITY_H
#define AUD_DOORAUDIOENTITY_H

#include "audioentity.h"
#include "gameobjects.h"

#include "audioengine/soundset.h"
#include "bank/bkmgr.h"
#include "control/replay/ReplaySettings.h"

//class CGarage;
class CDoor;
namespace rage
{
	class audSound;
}
class audDoorAudioEntity : public naAudioEntity
{
private:
#if __BANK
	struct DoorAudioSettingsSetup
	{
		char doorAudioSettingsName[64];
		char doorTuningParamsName[64];
		f32 openThresh;
		f32 headingThresh;
		f32 closedThresh;
		f32 speedThresh;
		f32 speedScale;
		f32 headingDeltaThreshold;
		f32 angularVelThreshold;
		bool hasToCreateDTP;
	};
	static DoorAudioSettingsSetup sm_DoorSettingsCreator;
#endif
public: 

	AUDIO_ENTITY_NAME(audDoorAudioEntity);

	audDoorAudioEntity();
	~audDoorAudioEntity(){}

	void InitDoor(CDoor *door);
	void ProcessAudio(f32 velocity,f32 heading = 0.f ,f32 deltaHeading = 0.f);
	void ProcessAudio();
	void ProcessCollisionSound(f32 impulseMagnitude, bool shouldPlayIt = false, const CEntity *otherEntity = NULL);
	virtual void PreUpdateService(u32 timeInMs);
	void EntityWantsToOpenDoor(Vector3::Param position, f32 speed);

	void SetScriptUpdate(bool update) {m_ScriptUpdate = update;};

	void UpdatePositionForPushSound(const Vector3 &pos) {m_PosForPushSound = pos;}

	const DoorAudioSettings *GetDoorAudioSettings() const { return m_DoorSettings; }
	f32 GetPreviousHeading() {return m_PreviousHeadingOffset;};
	bool GetWasShut() { return m_WasShut;};
	static void InitClass();
	static void UpdateClass();

	virtual Vec3V_Out GetPosition() const;
	virtual audCompressedQuat GetOrientation() const;

	audSoundSet &GetSoundSet() { return m_DoorSounds; }

	/*void StartDoorOpening(CGarage *garage);
	void StopDoorOpening();
	void StartRespray();
	void StopRespray();

	bool IsDoorOpeningPlaying() const
	{
		return (m_DoorLoop != NULL);
	}

	bool IsResprayPlaying() const
	{
		return (m_ResprayLoop != NULL);
	}*/

#if __BANK
	//Will send a message to RAVE to create a door game object;
	static void CreateDoorAudioSettings();
	//Macs: Model Audio Collision Settings
	//xmlMsg: is a char array that will be filled with the formatted message to be sent to RAVE. This needs to be pretty big; 4096 seems to be enough at the moment;
	//settings: the name of the Macs object to be (usually MOD_<model name>)
	static void FormatDoorsAudioSettingsXml(char * xmlMsg, const char * settingsName,const char * soundsRef,const char * tuningParamsRef);
	static void FormatDoorsTuningParamsXml(char * xmlMsg, DoorAudioSettingsSetup settings);

	static void AddWidgets(bkBank &bank);
	static void AddDoorTuningParamsWidgets();
	static void AddDoorSoundSetWidgets();
	static rage::bkGroup*			sm_WidgetGroup; 
#endif

	static f32		  sm_PedHandPushDistance;


	void TriggerDoorImpact(const CPed * ped, Vec3V_In pos, bool isPush = false);

#if GTA_REPLAY
	void SetReplayDoorUpdating(bool val) { m_ReplayDoorUpdating = val; }
	bool IsDoorUpdating();
	f32 GetCurrentVelocity() { return m_CurrentVelocity; }
	f32 GetHeading() { return m_Heading; }
	f32 GetOriginalHeading() { return m_OriginalHeading; }
#endif

private:

	audMetadataRef FindSound(u32 soundName);

	void ProcessStdDoor(f32 openRatio);
	void ProcessNonStdDoor(f32 openRatio);
	void UpdateNonStdDoor();
	void UpdateStdDoor(f32 speedScale);

	naEnvironmentGroup* CreateDoorEnvironmentGroup(const CDoor* door);
	
	virtual void ProcessAnimEvents();



	DoorAudioSettings *m_DoorSettings; 
	DoorTuningParams  *m_DoorTuningParams; 
	CDoor			  *m_Door;
	
	audSoundSet		  m_DoorSounds;
	audSound		  *m_DoorOpening;
	audSound		  *m_DoorClosing;

	Vector3			  m_DoorPosition;
	Vector3			  m_PosForPushSound;
	Vector3			  m_DoorLastPosition;
	f32				  m_OldOpenRatio;		
	f32				  m_CurrentVelocity;
	f32				  m_Heading;
	f32				  m_OriginalHeading;
	f32				  m_PreviousHeadingOffset;	
	u32				  m_LastDoorImpactTime;
	bool			  m_WasShut;
	bool			  m_WasOpen;
	bool			  m_WasOpening;
	bool			  m_WasShutting;
	bool			  m_WasAtLimit;
	bool			  m_StopForced;
	bool			  m_ScriptUpdate;
	REPLAY_ONLY(bool  m_ReplayDoorUpdating;)
	BANK_ONLY(bool	  m_UsingDefaultSettings;)

	static audCurve sm_DoorCollisionPlayerCurve;
	static audCurve sm_DoorCollisionNpcCurve;
	static audCurve sm_DoorCollisionVehicleCurve;

	static f32        sm_DeltaOpeningThreshold;
	static f32        sm_ChangeDirThreshold;
	static f32        sm_ImpulseThreshold;
	static f32        sm_HeadingThreshold;
	static f32        sm_HeadingDeltaThreshold;
	static f32        sm_AngularVelThreshold;
	static f32        sm_PunchCollisionScale;
	
	BANK_ONLY(static bool	  sm_NeedToInitialize;)
	BANK_ONLY(static bool	  sm_PauseOnFullyOpen;)
	BANK_ONLY(static bool	  sm_PauseOnFullyClose;)
	BANK_ONLY(static bool	  sm_PauseOnChange;)
	BANK_ONLY(static bool	  sm_PauseOnOpening;)
	BANK_ONLY(static bool	  sm_PauseOnClosing;)
	BANK_ONLY(static bool	  sm_PauseOnBrush;)
	BANK_ONLY(static bool	  sm_FindDoor;)
	BANK_ONLY(static bool	  sm_IsMovingForAudio;)
	BANK_ONLY(static bool	  sm_ShowDoorsUpdating;)
	BANK_ONLY(static bool	  sm_ShowCurrentDoorVelocity;)
	//audSound *m_DoorLoop;
	//audSound *m_ResprayLoop;

	static u32 sm_DoorImpactTimeFilter;


};
#endif // AUD_DOORAUDIOENTITY_H
