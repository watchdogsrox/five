//  
// audio/vehiclegadgetaudio.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLEGADGETAUDIO_H
#define AUD_VEHICLEGADGETAUDIO_H

#include "audiodefines.h"
#include "audioengine/widgets.h"
#include "audiohardware/waveslot.h"
#include "audioengine/soundset.h"
#include "audioentity.h"
#include "Vehicles/VehicleGadgets.h"

class audVehicleAudioEntity;

// ----------------------------------------------------------------
// Base class for vehicle audio gadgets
// ----------------------------------------------------------------
class audVehicleGadget
{
public:
	typedef enum
	{
		AUD_VEHICLE_GADGET_TURRET,
		AUD_VEHICLE_GADGET_FORKS,
		AUD_VEHICLE_GADGET_TOWTRUCK_ARM,
		AUD_VEHICLE_GADGET_ROOF,
		AUD_VEHICLE_GADGET_DIGGER,
		AUD_VEHICLE_GADGET_HANDLER_FRAME,
		AUD_VEHICLE_GADGET_GRAPPLING_HOOK,
		AUD_VEHICLE_GADGET_MAGNET,
		AUD_VEHICLE_GADGET_DYNAMIC_SPOILER,
	} audVehicleGadgetType;

	audVehicleGadget();
	virtual ~audVehicleGadget() {};
	
	virtual void Init(audVehicleAudioEntity* parent);
	virtual void Update() {};
	virtual void Reset() {};
	virtual void StopAllSounds() {};
	virtual bool HandleAnimationTrigger(u32 UNUSED_PARAM(trigger)) { return false; }

	virtual audVehicleGadgetType GetType() const = 0;

protected:
	audVehicleAudioEntity* m_Parent;
};

// ----------------------------------------------------------------
// Turret audio
// ----------------------------------------------------------------
class audVehicleTurret : public audVehicleGadget
{
public:
	audVehicleTurret();
	virtual ~audVehicleTurret();

	void Init(audVehicleAudioEntity* parent);
	void Update();
	void Reset();
	void StopAllSounds();

	inline void SetAimAngle(Quaternion aimAngle) { m_AimAngle = aimAngle; }
	void SetTurretLocalPosition(const Vector3 localPosition);
	inline void SetAngularVelocity(Vec3V_ConstRef angularVelocity) { m_TurretAngularVelocity = RCC_VECTOR3(angularVelocity); }
	rage::Vector3 GetAngularVelocity() const { return m_TurretAngularVelocity; }
	void SetSmoothersEnabled(bool enabled) { m_SmoothersEnabled = enabled; }
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_TURRET; }

private:
	audSound* m_MotorSound;
	audSoundSet m_TurretSoundSet;
	audSimpleSmootherDiscrete m_HorizontalSmoother;
	audSimpleSmootherDiscrete m_VerticalSmoother;
	Quaternion m_AimAngle;
	Quaternion m_PrevAimAngle;
	Vector3 m_TurretAngularVelocity;
	Vector3 m_TurretLocalPosition;
	bool m_SmoothersEnabled;
};

// ----------------------------------------------------------------
// Forks audio
// ----------------------------------------------------------------
class audVehicleForks : public audVehicleGadget
{
public:
	audVehicleForks();
	virtual ~audVehicleForks();

	virtual void Update();
	virtual void Reset();
	virtual void StopAllSounds();

	inline void SetSpeed(f32 forkliftSpeed, f32 desiredAcceleration = 0.0f) { m_ForkliftSpeed = forkliftSpeed; m_DesiredAcceleration = desiredAcceleration; }
	f32 GetSpeed() const { return m_ForkliftSpeedLastFrame; }
	f32 GetDesiredAcceleration() const { return m_DesiredAcceleration; }
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_FORKS; }

protected:
	audSoundSet m_SoundSet;

private:
	virtual void InitSoundSet();
	virtual f32 GetMinMovementSpeed() { return 0.05f; }
private:
	audSound* m_MotorSound;
	f32 m_ForkliftSpeed;
	f32 m_DesiredAcceleration;
	f32 m_ForkliftSpeedLastFrame;
	u32 m_LastStopTime;
	u32 m_MotorPlayTime;
};

// ----------------------------------------------------------------
// Handler Frame audio
// ----------------------------------------------------------------
class audVehicleHandlerFrame : public audVehicleForks
{
public:
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_HANDLER_FRAME; }

private:
	virtual f32 GetMinMovementSpeed() { return 0.02f; }
};

// ----------------------------------------------------------------
// Tow truck arm audio
// ----------------------------------------------------------------
class audVehicleTowTruckArm : public audVehicleForks
{
public:
	audVehicleTowTruckArm();
	virtual ~audVehicleTowTruckArm();

	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_TOWTRUCK_ARM; }

	void Update();
	void Reset();
	void StopAllSounds();

	inline void SetHookPosition(const Vector3& position)	{ m_HookPosition = position; }
	inline void SetTowedVehicleAngle(f32 angle)				{ m_TowedVehicleAngle = angle; }
	f32 GetTowedVehicleAngle() const { return m_TowedVehicleAngle; }
	Vector3 GetHookPosition() const { return m_HookPosition; }

	static void InitClass();

private:
	virtual void InitSoundSet();
	virtual f32 GetMinMovementSpeed() { return 0.02f; }

private:
	Vector3 m_HookPosition;
	audSimpleSmootherDiscrete m_HookRattleVolumeSmoother;
	audSimpleSmoother m_TowAngleChangeRateSmoother;
	audSimpleSmootherDiscrete m_TowAngleVolSmoother;
	audSound* m_LinkStressSound;
	audSound* m_ChainRattleSound;
	f32 m_HookDistanceLastFrame;
	f32 m_TowAngleChangeRate;
	f32 m_TowAngleChangeRateLastFrame;
	f32 m_TowedVehicleAngle;
	f32 m_TowedVehicleAngleLastFrame;
	bool m_FirstUpdate;

	static audCurve sm_VehicleAngleToVolumeCurve;
	static audCurve sm_HookSpeedToVolumeCurve;
};

// ----------------------------------------------------------------
// Heli grappling hook on chains audio
// ----------------------------------------------------------------
class audVehicleGrapplingHook : public audVehicleGadget
{
public:
	audVehicleGrapplingHook();
	virtual ~audVehicleGrapplingHook();

	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_GRAPPLING_HOOK; }

	void Update();
	void Reset();
	void StopAllSounds();

	inline void SetHookPosition(const Vector3& position)	{ m_HookPosition = position; }
	inline void SetTowedVehicleAngle(f32 angle)				{ m_TowedVehicleAngle = angle; }
	inline void SetEntityIsAttached(bool attached)			{ m_EntityAttached = attached; }
	f32 GetTowedVehicleAngle() const { return m_TowedVehicleAngle; }
	Vector3 GetHookPosition() const { return m_HookPosition; }
	bool GetEntityAttached() const { return m_EntityAttached; } 

	static void InitClass();

private:
	virtual void InitSoundSet();
	virtual f32 GetMinMovementSpeed() { return 0.02f; }

private:
	Vector3 m_HookPosition;
	audSimpleSmootherDiscrete m_HookRattleVolumeSmoother;
	audSmoother m_TowAngleChangeRateSmoother;
	audSound* m_LinkStressSound;
	audSound* m_ChainRattleSound;
	f32 m_HookDistanceLastFrame;
	f32 m_TowAngleChangeRate;
	f32 m_TowAngleChangeRateLastFrame;
	f32 m_TowedVehicleAngle;
	f32 m_TowedVehicleAngleLastFrame;
	bool m_FirstUpdate;
	bool m_EntityAttached;

	audSoundSet m_SoundSet;

	static audCurve sm_VehicleAngleToVolumeCurve;	// this is for the creaking when hooked
	static audCurve sm_HookAngleToPitchCurve;		// this is for the creaking when hooked
	static audCurve sm_HookSpeedToVolumeCurve;		// this is for the chain rattle when unhooked


};

// ----------------------------------------------------------------
// Digger audio
// ----------------------------------------------------------------
class audVehicleDigger : public audVehicleGadget
{
public:
	audVehicleDigger();
	virtual ~audVehicleDigger();

	void Update();
	void Reset();
	void StopAllSounds();
	void SetSpeed(s16 joint, f32 speed, f32 desiredAcceleration = 0.0f)	{ m_JointSpeed[joint] = speed; m_JointDesiredAcceleration[joint] = desiredAcceleration; }
	f32 GetJointSpeed(s16 index) const { return m_JointSpeedLastFrame[index]; }
	f32 GetJointDesiredAcceleration(s16 index) const { return m_JointDesiredAcceleration[index]; }
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_DIGGER; }

private:
	audSoundSet m_DiggerSoundSet;
	audSound* m_JointSounds[DIGGER_JOINT_MAX];
	f32 m_JointSpeed[DIGGER_JOINT_MAX];
	f32 m_JointSpeedLastFrame[DIGGER_JOINT_MAX];
	f32 m_JointDesiredAcceleration[DIGGER_JOINT_MAX];
	u32 m_LastStopTime[DIGGER_JOINT_MAX];
	u32 m_JointPlayTime[DIGGER_JOINT_MAX];
};

// ----------------------------------------------------------------
// Convertible roof audio
// ----------------------------------------------------------------
class audVehicleConvertibleRoof : public audVehicleGadget
{
public:
	audVehicleConvertibleRoof();
	virtual ~audVehicleConvertibleRoof();

	enum audConvertibleRoofSoundType
	{
		AUD_ROOF_SOUND_TYPE_START,
		AUD_ROOF_SOUND_TYPE_STOP,
		AUD_ROOF_SOUND_TYPE_LOOP,		
		AUD_ROOF_SOUND_TYPE_MAX
	};

	enum audConvertibleRoofComponent
	{
		AUD_ROOF_MAIN,
		AUD_ROOF_FLAPS,
		AUD_ROOF_BOOT_OPEN,
		AUD_ROOF_BOOT_CLOSE,
		AUD_ROOF_WINDOWS,
		AUD_ROOF_COMPONENT_MAX
	};

	enum audConvertibleRoofCameraType
	{
		AUD_ROOF_INTERIOR,
		AUD_ROOF_EXTERIOR,
		AUD_ROOF_CAMERA_MAX,
	};
	
	bool HandleAnimationTrigger(u32 trigger);
	void StopAllSounds();
	void Update();	
	audMetadataRef GetRoofSound(u32 soundHash, audConvertibleRoofCameraType interiorType);
	static void InitClass();
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_ROOF; }

private:
	void UpdateVolumes();

private:
	static audCategory* s_PedRoofCategory;
	static audCategory* s_PlayerRoofCategory;
	audSound* m_RoofSounds[AUD_ROOF_COMPONENT_MAX][AUD_ROOF_SOUND_TYPE_MAX][AUD_ROOF_CAMERA_MAX];
	audSoundSet m_ConvertibleRoofSoundSet;
	audSoundSet m_ConvertibleRoofSoundSetInterior;
	audSimpleSmoother m_PlayerNPCVolumeSmoother;
	u32 m_LastRoofActiveTime;
	bool m_RoofWasActive;
};

// ----------------------------------------------------------------
// Magnet audio
// ----------------------------------------------------------------
class audVehicleGadgetMagnet : public audVehicleGadget
{
public:
	audVehicleGadgetMagnet();
	virtual ~audVehicleGadgetMagnet();

	virtual void Update();
	virtual void StopAllSounds();	
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_MAGNET; }

	void UpdateMagnetStatus(Vec3V_In position, bool magnetOn, f32 targetDistance);
	void TriggerMagnetAttach(CEntity* attachedVehicle);
	void TriggerMagnetDetach(CEntity* attachedVehicle);
	bool IsMagnetActive() { return m_IsMagnetActive; }
	
private:
	virtual void InitSoundSet();

private:
	audSoundSet m_SoundSet;
	audSound* m_MagnetHumSound;
	Vec3V m_MagnetPosition;
	bool m_IsMagnetActive;
	f32 m_MagnetTargetDistance;
};

// ----------------------------------------------------------------
// Dynamic spoiler audio
// ----------------------------------------------------------------
class audVehicleGadgetDynamicSpoiler : public audVehicleGadget
{
public:
	enum audDynamicSpoilerStrutState
	{
		AUD_STRUT_LOWERED,
		AUD_STRUT_RAISING,
		AUD_STRUT_RAISED,
		AUD_STRUT_LOWERING,		
	};

	enum audDynamicSpoilerAngleState
	{
		AUD_SPOILER_ROTATING_FORWARDS,
		AUD_SPOILER_ROTATED_FORWARDS,
		AUD_SPOILER_ROTATING_BACKWARDS,
		AUD_SPOILER_ROTATED_BACKWARDS,
	};

public:
	audVehicleGadgetDynamicSpoiler();
	virtual ~audVehicleGadgetDynamicSpoiler();
	audVehicleGadgetType GetType() const { return AUD_VEHICLE_GADGET_DYNAMIC_SPOILER; }

	virtual void Update();
	virtual void StopAllSounds();
	virtual void Init(audVehicleAudioEntity* parent);

	void SetSpoilerStrutState(audDynamicSpoilerStrutState strutState);
	void SetSpoilerAngleState(audDynamicSpoilerAngleState angleState);

	void SetFrozen(bool frozen) { m_Frozen = frozen; }

private:
	void InitSoundSet();

private:
	audSound* m_RaiseSound;
	audSound* m_RotateSound;
	audSoundSet m_SoundSet;
	audDynamicSpoilerStrutState m_StrutState;
	audDynamicSpoilerAngleState m_AngleState;
	bool m_Frozen;
};

#endif
