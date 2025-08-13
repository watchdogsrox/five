// 
// animation/EventTags.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef EVENT_TAGS_H
#define EVENT_TAGS_H

// Rage headers
#include "atl/hashstring.h"
#include "crmetadata/property.h"
#include "crmetadata/properties.h"
#include "crmetadata/propertyattributes.h"
#include "crmetadata/tag.h"
#include "crmetadata/tagiterators.h"
#include "crclip/clip.h"
#include "fwanimation/move.h"
#include "scene\RegdRefTypes.h"

class CDynamicEntity;

class CClipEventTags
{
public:

	typedef fwMvPropertyId Key;

	//////////////////////////////////////////////////////////////////////////
	//	Event tag definitions
	//////////////////////////////////////////////////////////////////////////

	// PreDelay in visemes 
	static Key TimeOfPreDelay;	

	// Used to identify an important frame
	static Key CriticalFrame;
	
	// A special event containing a bespoke hash string that can be used
	// as an event in the MoVE runtime
	static Key MoveEvent;

	// Triggers a firing behavior on a weapon
	static Key Fire;
	static Key Index;
	static Key CreateOrdnance;

	class CFireEventTag : public rage::crProperty
	{
	public:
		int GetIndex() const;
	};

	// Foot tags - used for tag synchronization of locomotion anims
	static Key Foot;
	static Key Heel;
	static Key Left;
	static Key Right;

	// Triggers the creation / destruction or release of an object (e.g. when pulling out a weapon)
	static Key Object;
	static Key Create;
	static Key Destroy;
	static Key Release;

	class CObjectEventTag : public rage::crProperty
	{
	public:
		bool IsCreate() const;
		bool IsDestroy() const;
		bool IsRelease() const;
	};

	static Key Start;

	// Anim can be interrupted from this point?
	static Key Interruptible;
	static Key WalkInterruptible;
	static Key RunInterruptible;

	// Triggers shell eject vfx on a weapon
	static Key ShellEjected;

	// Door open and close events
	static Key Door;

	// Smash event
	static Key Smash;

	// Blend out upper body anims
	static Key BlendOutWithDuration;

	class CBlendOutEventTag : public rage::crProperty
	{
	public:
		float GetBlendOutDuration() const;
	};

	// Activate or deactivate IK
	static Key Ik;
	static Key On;

	class CIkEventTag : public rage::crProperty
	{
	public:
		bool IsOn() const;
		bool IsRight() const;
	};

	// ArmsIK
	static Key ArmsIk;
	static Key Allowed;
	static Key BlendIn;
	static Key BlendOut;
	class CArmsIKEventTag : public rage::crProperty
	{
	public:
		bool GetAllowed() const;
		bool GetLeft() const;
		bool GetRight() const;
		float GetBlendIn() const;
		float GetBlendOut() const;
	};

	// LegsIK
	static Key LegsIk;
	class CLegsIKEventTag : public rage::crProperty
	{
	public:
		bool GetAllowed() const;
		float GetBlendIn() const;
		float GetBlendOut() const;
	};

	static Key LegsIkOptions;
	static Key LockFootHeight;

	class CLegsIKOptionsEventTag : public rage::crProperty
	{
	public:
		bool GetLeft() const;
		bool GetRight() const;
		bool GetLockFootHeight() const;
	};

	// LookIK
	static Key LookIk;
	static Key LookIkTurnSpeed;
	static Key LookIkBlendInSpeed;
	static Key LookIkBlendOutSpeed;
	static Key EyeIkReferenceDirection;
	static Key HeadIkReferenceDirection;
	static Key NeckIkReferenceDirection;
	static Key TorsoIkReferenceDirection;
	static Key HeadIkYawDeltaLimit;
	static Key HeadIkPitchDeltaLimit;
	static Key NeckIkYawDeltaLimit;
	static Key NeckIkPitchDeltaLimit;
	static Key TorsoIkYawDeltaLimit;
	static Key TorsoIkPitchDeltaLimit;
	static Key HeadIkAttitude;
	static Key TorsoIkLeftArm;
	static Key TorsoIkRightArm;

	class CLookIKEventTag : public rage::crProperty
	{
	public:
		u8 GetTurnSpeed() const;
		u8 GetBlendInSpeed() const;
		u8 GetBlendOutSpeed() const;

		u8 GetEyeReferenceDirection() const;
		u8 GetHeadReferenceDirection() const;
		u8 GetNeckReferenceDirection() const;
		u8 GetTorsoReferenceDirection() const;

		u8 GetHeadYawDeltaLimit() const;
		u8 GetHeadPitchDeltaLimit() const;
		u8 GetNeckYawDeltaLimit() const;
		u8 GetNeckPitchDeltaLimit() const;
		u8 GetTorsoYawDeltaLimit() const;
		u8 GetTorsoPitchDeltaLimit() const;

		u8 GetHeadAttitude() const;

		u8 GetLeftArmCompensation() const;
		u8 GetRightArmCompensation() const;

	private:
		u8 GetBlendSpeed(const crPropertyAttributeHashString* attrib) const;
		u8 GetReferenceDirection(const crPropertyAttributeHashString* attrib) const;
		u8 GetDeltaLimit(const crPropertyAttributeHashString* attrib) const;
		u8 GetArmCompensation(const crPropertyAttributeHashString* attrib) const;
	};

	// Camera Shake.
	
	static Key CameraShake;
	static Key CameraShakeRef;
	
	class CCameraShakeTag : public rage::crProperty
	{
	public:
		u32 GetShakeRefHash() const;
	};

	// Pad Shake.
	static Key PadShake;
	static Key PadShakeIntensity;
	static Key PadShakeDuration;

	class CPadShakeTag : public rage::crProperty
	{
	public:
		u32 GetPadShakeDuration() const;
		float GetPadShakeIntensity() const;
	};

	// LookIKControl
	static Key LookIkControl;

	class CLookIKControlEventTag : public rage::crProperty
	{
	public:
		bool GetAllowed() const;
	};

	// SelfieHeadControl
	static Key SelfieHeadControl;
	static Key Weight;

	class CSelfieHeadControlEventTag : public rage::crProperty
	{
	public:
		enum eBlendType
		{
			BtSlowest,
			BtSlow,
			BtNormal,
			BtFast,
			BtFastest,
			BtCount
		};

		eBlendType GetBlendIn() const;
		eBlendType GetBlendOut() const;
		float GetWeight() const;

	private:
		eBlendType GetBlendType(u32 hash) const;
	};

	// SelfieLeftArmControl
	static Key SelfieLeftArmControl;
	static Key CameraRelativeWeight;
	static Key JointTargetWeight;
	static Key JointTarget;

	class CSelfieLeftArmControlEventTag : public rage::crProperty
	{
	public:
		enum eJointTarget
		{
			JtHead,
			JtSpine,
			JtCount
		};

		enum eBlendType
		{
			BtSlowest,
			BtSlow,
			BtNormal,
			BtFast,
			BtFastest,
			BtCount
		};

		float GetCameraRelativeWeight() const;
		eJointTarget GetJointTarget() const;
		float GetJointTargetWeight() const;
		eBlendType GetBlendIn() const;
		eBlendType GetBlendOut() const;

	private:
		eBlendType GetBlendType(u32 hash) const;
	};

	// Gestures
	static Key GestureControl;
	static Key OverrideDefaultSettings;
	static Key DefaultRootStabilization;
	static Key DefaultFilterID;

	class CGestureControlEventTag : public rage::crProperty
	{
	public:
		bool GetAllowed() const;
		bool GetOverrideDefaultSettings() const;
		bool GetDefaultRootStabilization() const;
		int GetDefaultFilterID() const;
	};

	// FacialAnim
	static Key Facial;
	static Key NameId;
	static Key IsClipName;
	class CFacialAnimEventTag : public rage::crProperty
	{
	public:
		bool GetIsClipName() const;
		u32 GetNameIdHash() const;
		const atHashString* GetNameIdHashString() const;
	};

	// BlendToVehicleLighting
	static Key BlendToVehicleLighting;
	static Key IsGettingIntoVehicle;
	class CBlendToVehicleLightingAnimEventTag : public rage::crProperty
	{
	public:
		bool GetIsGettingIntoVehicle() const;
	};

	// CutsceneBlendToVehicleLighting
	static Key CutsceneBlendToVehicleLighting;
	static Key TargetValue;
	static Key BlendTime;
	class CCutsceneBlendToVehicleLightingAnimEventTag : public rage::crProperty
	{
	public:
		float GetTargetValue() const;
		float GetBlendTime() const;
	};

	// CutsceneEnterVehicle
	static Key CutsceneEnterExitVehicle;
	static Key VehicleSceneHandle;
	static Key SeatIndex;
	class CCutsceneEnterExitVehicleAnimEventTag : public rage::crProperty
	{
	public:
		const atHashString GetVehicleSceneHandle() const;
		const int GetSeatIndex() const;
		const float GetBlendIn() const;
		const float GetBlendOut() const;
	};
	
	// BlockCutsceneSkipping
	static Key BlockCutsceneSkipping;
	class CBlockCutsceneSkippingTag : public rage::crProperty
	{
	public:
		// No properties required
	};

	// Applying force to a vehicle
	static Key ApplyForce;

	class CApplyForceTag : public rage::crProperty
	{
	public:
		float GetForce() const;
		bool GetXOffset(float &offset) const;
		bool GetYOffset(float &offset) const;
	};

	// Applying force to a vehicle
	static Key BlockRagdoll;
	static Key BlockAll;
	static Key BlockFromThisVehicleMoving;

	class CBlockRagdollTag : public rage::crProperty
	{
	public:
		bool ShouldBlockAll() const;
		bool ShouldBlockFromThisVehicleMoving() const;
	};

	// Extra fixup to GET_IN point tag.
	static Key FixupToGetInPoint;

	class CFixupToGetInPointTag : public rage::crProperty
	{
	public:
		int GetGetInPointIndex() const;
	};

	// Object vfx events
	static Key ObjectVfx;
	static Key Register;
	static Key Trigger;
	static Key VFXName;

	// Denotes a point in the anim when a switch to nm can occur
	static Key CanSwitchToNm;

	// Start or stop adjusting the animated velocity in the vaulting task
	static Key AdjustAnimVel;

	// An instruction to play an audio event
	static Key Audio;
	static Key Id;
	static Key RelevantWeightThreshold;

	class CAudioEventTag : public rage::crProperty
	{
	public:
		u32 GetId() const;
		float GetRelevantWeightThreshold() const;
	};

	static Key LoopingAudio;

	class CLoopingAudioEventTag : public rage::crProperty
	{
	public:
		u32 GetId() const;
		float GetRelevantWeightThreshold() const;
		bool IsStart() const;
	};

	// Denotes whether or not the mover should be fixed up by the runtime
	// during this portion of the animation. Currently used in the ambient
	//  and vehicle get in systems.
	static Key MoverFixup;
	static Key Translation;
	static Key Rotation;
	static Key TransX;
	static Key TransY;
	static Key TransZ;
	static Key VehicleModel;

	class CMoverFixupEventTag : public rage::crProperty
	{
	public:

		enum eAxisType
		{
			eXAxis,
			eYAxis,
			eZAxis,
			eAnyAxis
		};

		bool ShouldFixupTranslation() const;
		bool ShouldFixupRotation() const;

		bool ShouldFixupX() const;
		bool ShouldFixupY() const;
		bool ShouldFixupZ() const;

		u32 GetSpecificVehicleModelHashToApplyFixUpTo() const;
	};

	static Key DistanceMarker;
	static Key Distance;

	class CDistanceMarkerTag : public rage::crProperty
	{
	public:
		float GetDistance() const;
	};


	static Key SweepMarker;
	static Key SweepMinYaw;
	static Key SweepMaxYaw;
	static Key SweepPitch;

	class CSweepMarkerTag : public rage::crProperty
	{
		public:
			float GetMinYaw() const;
			float GetMaxYaw() const;
			float GetPitch() const;
	};

	// An instruction used to determine the melee homing/separation window
	static Key MeleeHoming;

	// An instruction used to determine the melee collision window
	static Key MeleeCollision;

	// An instruction used to determine if we are invulnerable to melee strikes
	static Key MeleeInvulnerability;

	// An instruction used to determine if we should allow an opponent to dodge
	static Key MeleeDodge;

	// An optional instruction used to determine the moment a melee action should make contact
	static Key MeleeContact;

	// An optional instruction that will define a set of facial anims that can be played on the acting ped
	static Key MeleeFacialAnim;

	// An instruction used to determine when we should switch to Ragdoll
	static Key SwitchToRagdoll;

	// An instruction used to determine when we should blow up explosives
	static Key Detonation;

	// An instruction used to determine when we should start the vehicle engine
	static Key StartEngine;

	// Used to pick random start and end phases 
	static Key RandomizeStartPhase;
	static Key MinPhase;
	static Key MaxPhase;

	// Clips can specify the paired clip that should be played with it
	static Key PairedClip;
	static Key PairedClipName;
	static Key PairedClipSetName;

	class CPairedClipTag : public rage::crProperty
	{
	public:
		u32 GetPairedClipNameHash() const;
		u32 GetPairedClipSetNameHash() const;
	};


	class CRandomizeStartPhaseProperty : public rage::crProperty
	{
	public:
		float PickRandomStartPhase(u32 seed = 0) const;

		float GetMinStartPhase() const;
		float GetMaxStartPhase() const;
	};

	static Key RandomizeRate;
	static Key MinRate;
	static Key MaxRate;

	class CRandomizeRateProperty : public rage::crProperty
	{
	public:
		float PickRandomRate(u32 seed = 0) const;

		float GetMinRate() const;
		float GetMaxRate() const;
	};

	//Tag sync
	static Key TagSyncBlendOut; 
	
	//Whether an animation ends in a walk
	static Key MoveEndsInWalk;

	//Blend in upper body aim anims
	static Key BlendInUpperBodyAim;

	//Blend in blind fire aim anims
	static Key BlendInBlindFireAim;

	//Play a phone ring tone
	static Key PhoneRing;

	//Ped is exiting a wall sit, this is the phase to determine how to fix up the exit if the wall is not the height the anim was built with.
	static Key FallExit;

	//Tag on the synch scene for shark attack - this is the phase when the ped damage should be applied.
	static Key SharkAttackPedDamage;

	//Tag on take off helmet animation - use to override the peds hair back to 0.0f scale.
	static Key RestoreHair;

	// Flashlight toggle
	static Key FlashLight;
	static Key FlashLightName;
	static Key FlashLightOn;

	// Branching tags for CTaskGetup
	static Key BranchGetup;
	static Key BlendOutItem;
	class CBranchGetupTag : public rage::crProperty
	{
	public:
		atHashString GetTrigger() const;
		atHashString GetBlendOutItemId() const;
	};

	static Key FirstPersonCamera;
	class CFirstPersonCameraEventTag : public rage::crProperty
	{
	public:
		bool  GetAllowed() const; 
		float GetBlendInDuration() const;
		float GetBlendOutDuration() const;
	};

	static Key FirstPersonCameraInput;
	class CFirstPersonCameraInputEventTag : public rage::crProperty
	{
	public:
		bool GetYawSpringInput() const;
		bool GetPitchSpringInput() const;
	};

	static Key MotionBlur;
	class CMotionBlurEventTag : public rage::crProperty
	{
	public:
		u16 GetBoneId() const;
		float GetBlurAmount() const;
		float GetBlendInDuration() const;
		float GetBlendOutDuration() const;

		float GetVelocityBlend() const;
		float GetNoisiness() const;
		float GetMaximumVelocity() const;

		struct MotionBlurData
		{
			MotionBlurData()
			: m_currentPosition(V_ZERO)
			, m_lastPosition(V_ZERO)
			{ 
				Clear(); 
			}

			friend class CMotionBlurEventTag;

			float GetBlend() const { return m_blend; }

			float GetVelocityBlend() const {return m_VelocityBlend; }
			float GetNoisiness() const {return m_Noisiness; }
			float GetMaximumVelocity() const {return m_MaximumVelocity; }

			float GetCurrentBlurAmount() const { return m_blend*m_totalBlurAmount; }
			Vec3V GetCurrentPosition() const { return m_currentPosition; }
			Vec3V GetLastPosition() const { return m_lastPosition; }
			u16 GetBoneId() const { return m_boneId; }
			const CDynamicEntity* GetTargetEntity() const { return m_pCameraTarget.Get(); }
			bool IsValid() const { return m_blend>0.0f && m_timesLastPositionSet>1; }
			
			void Clear();

		private:

			float m_blend;
			float m_blendOutDuration;
			float m_totalBlurAmount;

			float m_VelocityBlend;
			float m_Noisiness;
			float m_MaximumVelocity;

			u16 m_boneId;
			Vec3V m_currentPosition;
			Vec3V m_lastPosition;
			s32 m_timesLastPositionSet;

			const CMotionBlurEventTag* m_pActiveTag;
			RegdConstDyn m_pCameraTarget;
		};

		static const MotionBlurData* GetCurrentMotionBlurIfValid();
		static void Update();
		static void PostPreRender();

	private:
		static MotionBlurData m_blurData;
	};

	
	static Key FirstPersonHelmetCull;
	static Key FirstPersonHelmetCullVehicleName;
	class CFirstPersonHelmetCullEventTag : public rage::crProperty
	{
	public:
		u32 GetVehicleNameHash() const;
	};

	//////////////////////////////////////////////////////////////////////////
	//	Iterators and searching functions
	//////////////////////////////////////////////////////////////////////////

	template <class T>
	class CEventTag : public rage::crTag
	{
	public:
		const T* GetData() const
		{
			return static_cast<const T*>(&GetProperty());
		}
	};


	//////////////////////////////////////////////////////////////////////////
	//	Basic tag iterator. Searches for tags by key value. 
	//	Templated to allow automatic casting to a requested return value.
	//////////////////////////////////////////////////////////////////////////
	template <class T>
	class CTagIterator : public crTagIterator
	{
	public:

		// PURPOSE: Iterate tags with tag type
		CTagIterator(const crTags& tags, CClipEventTags::Key key)
			: crTagIterator(tags, key.GetHash())
		{
		}

		// PURPOSE: Iterate tags with phase offset and tag type
		CTagIterator(const crTags& tags, float startPhase, CClipEventTags::Key key)
			: crTagIterator(tags, startPhase, key.GetHash())
		{
		}

		// PURPOSE: Iterate tags with phase range and tag type
		CTagIterator(const crTags& tags, float startPhase, float endPhase, CClipEventTags::Key key)
			: crTagIterator(tags, startPhase, endPhase, key.GetHash())
		{
		}

		// PURPOSE: Get the current tag
		const CEventTag<T>* operator*() const
		{
			return static_cast<const CEventTag<T>*>(GetCurrent());
		}
	};

	//////////////////////////////////////////////////////////////////////////
	//	Advanced tag iterator. Matches against the tag's key, and an 
	//	attribute key (and type). The value of the attribute is not matched.
	//	Templated to allow automatic casting to a requested return value.
	//////////////////////////////////////////////////////////////////////////

	template <class Return_T, class Attribute_T>
	class CTagIteratorAttribute : public crTagIteratorPropertyAttribute<>
	{
	public:

		// PURPOSE: Iterate tags with tag and attribute keys
		CTagIteratorAttribute(const crTags& tags, CClipEventTags::Key propertyKey, CClipEventTags::Key attributeKey)
			: crTagIteratorPropertyAttribute<>(tags, propertyKey, attributeKey)
		{
			crTagIteratorPropertyAttribute<>::Init();
		}

		// PURPOSE: Iterate tags with phase offset, tag and attribute keys
		CTagIteratorAttribute(const crTags& tags, float startPhase, CClipEventTags::Key propertyKey, CClipEventTags::Key attributeKey)
			: crTagIteratorPropertyAttribute<>(tags, startPhase, propertyKey, attributeKey)
		{
			crTagIteratorPropertyAttribute<>::Init();
		}

		// PURPOSE: Iterate tags with phase range, tag and attribute keys
		CTagIteratorAttribute(const crTags& tags, float startPhase, float endPhase, CClipEventTags::Key propertyKey, CClipEventTags::Key attributeKey)
			: crTagIteratorPropertyAttribute<>(tags, startPhase, endPhase, propertyKey, attributeKey)
		{
			crTagIteratorPropertyAttribute<>::Init();
		}

		// PURPOSE: Get the current tag
		const CEventTag<Return_T>* operator*() const
		{
			return static_cast<const CEventTag<Return_T>*>(crTagIteratorPropertyAttribute<>::GetCurrent());
		}
	};

	//////////////////////////////////////////////////////////////////////////
	//	Advanced tag iterator. Matches against the tag's key, an attribute key, and
	//	the value of the attribute.
	//	Templated to allow automatic casting to a requested return value.
	//////////////////////////////////////////////////////////////////////////
	template <class Return_T, class Attribute_T, class Data_T>
	class CTagIteratorAttributeValue : public crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>
	{
	public:

		// PURPOSE: Iterate tags with tag and attribute keys and value
		CTagIteratorAttributeValue(const crTags& tags, CClipEventTags::Key propertyKey, CClipEventTags::Key attributeKey, Data_T attributeValue)
			: crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>(tags, propertyKey.GetHash(), attributeKey.GetHash(), attributeValue)
		{
			crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>::Init();
		}

		// PURPOSE: Iterate tags with phase offset, tag and attribute keys and value
		CTagIteratorAttributeValue(const crTags& tags, float startPhase, CClipEventTags::Key propertyKey, CClipEventTags::Key attributeKey, Data_T attributeValue)
			: crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>(tags, startPhase, propertyKey.GetHash(), attributeKey.GetHash(), attributeValue)
		{
			crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>::Init();
		}

		// PURPOSE: Iterate tags with phase range, tag and attribute keys and value
		CTagIteratorAttributeValue(const crTags& tags, float startPhase, float endPhase, CClipEventTags::Key propertyKey, CClipEventTags::Key attributeKey, Data_T attributeValue)
			: crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>(tags, startPhase, endPhase, propertyKey.GetHash(), attributeKey.GetHash(), attributeValue)
		{
			crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>::Init();
		}

		// PURPOSE: Get the current tag
		const CEventTag<Return_T>* operator*() const
		{
			return static_cast<const CEventTag<Return_T>*>(crTagIteratorPropertyAttributeValue<Attribute_T, Data_T>::GetCurrent());
		}
	};

	//////////////////////////////////////////////////////////////////////////
	//	Static methods - use these for easy searching clips for events
	//////////////////////////////////////////////////////////////////////////

public:

	// find and return the first tag with the given key
	static const crTag* FindFirstEventTag(const crClip* pClip, Key propertyKey, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			crTagIterator it(*pClip->GetTags(), startPhase, endPhase, propertyKey.GetHash());
			return *it;
		}

		return NULL;
	}

	template <class Return_T>
	static const CEventTag<Return_T>* FindFirstEventTag(const crClip* pClip, Key propertyKey, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIterator<Return_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey);
			return *it;
		}

		return NULL;
	}

	// find and return the first tag with the given key and attribute. Automatically casts the result to the Return_T.
	// e.g. FindFirstEventTag<CObjectEventTag, crPropertyAttributeBool>(AnimEventTag::Object, AnimEventTag::Create);
	//		will return the first event tag named "Object" which has a boolean type attribute called "Create".
	//		Does not check the value of the attribute.
	template <class Return_T, class Attribute_T>
	static const CEventTag<Return_T>* FindFirstEventTag(const crClip* pClip, Key propertyKey, Key attributeKey, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttribute<Return_T, Attribute_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey);
			return *it;
		}

		return NULL;
	}

	// find and return the first tag with the given key and attribute, including checking the value of the attribute. 
	//		Automatically casts the result to the Return_T.
	// e.g. FindFirstEventTag<CObjectEventTag, crPropertyAttributeBool, bool>(AnimEventTag::Object, AnimEventTag::Create, true);
	//		will return the first event tag named "Object" which has a boolean type attribute called "Create", with the value true.
	template <class Return_T, class Attribute_T, class Data_T>
	static const CEventTag<Return_T>* FindFirstEventTag(const crClip* pClip, Key propertyKey, Key attributeKey, Data_T value, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttributeValue<Return_T, Attribute_T, Data_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey, value);
			return *it;
		}

		return NULL;
	}

	// find and return the first tag with the given key
	static const crTag* FindLastEventTag(const crClip* pClip, Key propertyKey, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			crTagIterator it(*pClip->GetTags(), startPhase, endPhase, propertyKey.GetHash());
			it.End();
			return *it;
		}
		return NULL;
	}

	// find and return the first tag with the given key. Automatically casts the result to the specified tag type.
	template <class T>
	static const CEventTag<T>* FindLastEventTag(const crClip* pClip, Key propertyKey, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIterator<T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey);
			it.End();
			return *it;
		}
		return NULL;
	}

#if 0 // doesn't compile
	// find and return the first tag with the given key and attribute. Automatically casts the result to the Return_T.
	// e.g. FindFirstEventTag<CObjectEventTag, crPropertyAttributeBool>(AnimEventTag::Object, AnimEventTag::Create);
	//		will return the first event tag named "Object" which has a boolean type attribute called "Create".
	//		Does not check the value of the attribute.
	template <class Return_T, class Attribute_T>
	static const CEventTag<Return_T>* FindLastEventTag(const crClip* pClip, Key propertyKey, Key attributeKey, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags)
		{
			CTagIteratorAttribute<Return_T, Attribute_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey);
			it.End();
			return *it;
		}
		return NULL;
	}
#endif

	// find and return the first tag with the given key and attribute, including checking the value of the attribute. 
	//		Automatically casts the result to the Return_T.
	// e.g. FindFirstEventTag<CObjectEventTag, crPropertyAttributeBool, bool>(AnimEventTag::Object, AnimEventTag::Create, true);
	//		will return the first event tag named "Object" which has a boolean type attribute called "Create", with the value true.
	template <class Return_T, class Attribute_T, class Data_T>
	static const CEventTag<Return_T>* FindLastEventTag(const crClip* pClip, Key propertyKey, Key attributeKey, Data_T value, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttributeValue<Return_T, Attribute_T, Data_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey, value);
			it.End();
			return *it;
		}

		return NULL;
	}

	static bool FindEventPhase(const crClip* pClip, Key propertyKey, float& phase, float startPhase = 0.0f, float endPhase = 1.0f )
	{
		const crTag* pTag = FindFirstEventTag(pClip, propertyKey, startPhase, endPhase);

		if (pTag)
		{
			phase = pTag->GetStart();
			return true;
		}
		else
		{
			return false;
		}
	}

	template <class Attribute_T>
	static bool FindEventPhase(const crClip* pClip, Key propertyKey, Key attributeKey, float& phase, float startPhase = 0.0f, float endPhase = 1.0f )
	{
		const crTag* pTag = FindFirstEventTag<crTag, Attribute_T>(pClip, propertyKey, attributeKey, startPhase, endPhase);

		if (pTag)
		{
			phase = pTag->GetStart();
			return true;
		}
		else
		{
			return false;
		}
	}

	template <class Attribute_T, class Data_T>
	static bool FindEventPhase(const crClip* pClip, Key propertyKey, Key attributeKey, Data_T value, float& phase, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		const crTag* pTag = FindFirstEventTag<crTag, Attribute_T, Data_T>(pClip, propertyKey, attributeKey, value, startPhase, endPhase);

		if (pTag)
		{
			phase = pTag->GetStart();
			return true;
		}
		else
		{
			return false;
		}
	}

	static u32 CountEvent(const crClip* pClip, Key propertyKey, float startPhase = 0.0f, float endPhase = 1.0f )
	{
		if (pClip && pClip->GetTags())
		{
			crTagIterator it(*pClip->GetTags(), startPhase, endPhase, propertyKey.GetHash());
			return it.Count();
		}
		return 0;
	}

	template <class Attribute_T>
	static u32 CountEvent(const crClip* pClip, Key propertyKey, Key attributeKey, float startPhase = 0.0f, float endPhase = 1.0f )
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttribute<crTag, Attribute_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey);
			return it.Count();
		}
		return 0;
	}

	template <class Attribute_T, class Data_T>
	static u32 CountEvent(const crClip* pClip, Key propertyKey, Key attributeKey, Data_T value, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttributeValue<crTag, Attribute_T, Data_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey, value);
			return it.Count();
		}
		return 0;
	}

	static bool HasEvent(const crClip* pClip, Key propertyKey, float startPhase = 0.0f, float endPhase = 1.0f )
	{
		if (pClip && pClip->GetTags())
		{
			crTagIterator it(*pClip->GetTags(), startPhase, endPhase, propertyKey.GetHash());
			return !it.Empty();
		}
		return false;
	}

	template <class Attribute_T>
	static bool HasEvent(const crClip* pClip, Key propertyKey, Key attributeKey, float startPhase = 0.0f, float endPhase = 1.0f )
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttribute<crTag, Attribute_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey);
			return !it.Empty();
		}
		return false;
	}

	template <class Attribute_T, class Data_T>
	static bool HasEvent(const crClip* pClip, Key propertyKey, Key attributeKey, Data_T value, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttributeValue<crTag, Attribute_T, Data_T> it(*pClip->GetTags(), startPhase, endPhase, propertyKey, attributeKey, value);
			return !it.Empty();
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Property searching
	//////////////////////////////////////////////////////////////////////////

	// find and return the first tag with the given key
	static const crProperty* FindProperty(const crClip* pClip, Key propertyKey)
	{
		if (pClip && pClip->GetProperties())
		{
			return pClip->GetProperties()->FindProperty(propertyKey);
		}

		return NULL;
	}

	template <class Return_T>
	static const Return_T* FindProperty(const crClip* pClip, Key propertyKey)
	{
		if (pClip && pClip->GetProperties())
		{
			return static_cast<const Return_T*>(pClip->GetProperties()->FindProperty(propertyKey));
		}

		return NULL;
	}

	// find and return the first property with the given key and attribute. Automatically casts the result to the Return_T.
	//		Does not check the value of the attribute.
	template <class Return_T, class Attribute_T>
	static const Return_T* FindProperty(const crClip* pClip, Key propertyKey, Key attributeKey)
	{
		if (pClip && pClip->GetProperties())
		{
			const crProperty* pProp = pClip->GetProperties()->FindProperty(propertyKey);
			if (pProp)
			{
				const Attribute_T* pAttrib = static_cast<const Attribute_T*>(pProp->GetAttribute(attributeKey));
				if (pAttrib)
				{
					return static_cast<const Return_T*>(pProp);
				}
			}
		}
		return NULL;
	}

	//	find and return the property with the given key and returns the attribute matching attributeKey. 
	//	Checks the attribute is of the expected type before returning.
	template <class Attribute_T>
	static const Attribute_T* FindPropertyAttribute(const crClip* pClip, Key propertyKey, Key attributeKey)
	{
		if (pClip && pClip->GetProperties())
		{
			const crProperty* pProp = pClip->GetProperties()->FindProperty(propertyKey);
			if (pProp)
			{
				const crPropertyAttribute* pAttrib = pProp->GetAttribute(attributeKey);
				if (pAttrib && pAttrib->GetType() == Attribute_T::sm_TypeInfo.GetType())
				{
					return static_cast<const Attribute_T*>(pAttrib);
				}
			}
		}
		return NULL;
	}

	//		find and return the first property matching the given key and attribute, including checking the value of the attribute. 
	//		Automatically casts the result to the Return_T.
	template <class Return_T, class Attribute_T, class Data_T>
	static const Return_T* FindProperty(const crClip* pClip, Key propertyKey, Key attributeKey, Data_T value)
	{
		if (pClip && pClip->GetProperties())
		{
			const crProperty* pProp = pClip->GetProperties()->FindProperty(propertyKey);
			if (pProp)
			{
				crPropertyAttributeAccessor<Attribute_T, Data_T> accessor(pProp->GetAttribute(attributeKey));
				if(accessor.Compare(value))
				{
					return static_cast<const Return_T*>(pProp);
				}
			}
		}
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	// Type specific event tag searches.
	//////////////////////////////////////////////////////////////////////////

	static bool FindMoverFixUpStartEndPhases(const crClip* pClip, float& fStartPhase, float& fEndPhase, bool bTranslation = true, CMoverFixupEventTag::eAxisType axisType = CMoverFixupEventTag::eAnyAxis)
	{
		if (pClip && pClip->GetTags())
		{
			Key FixupKey = bTranslation ? Translation : Rotation;

			if (fStartPhase < 0.0f)
				fStartPhase = 0.0f;
			if (fEndPhase < 0.0f)
				fEndPhase = 1.0f;

			CTagIteratorAttributeValue<CMoverFixupEventTag, crPropertyAttributeBool, bool> it(*pClip->GetTags(), fStartPhase, fEndPhase, MoverFixup, FixupKey, true);

			while (*it)
			{
				if ((*it)->GetData()->ShouldFixupX() && (axisType == CMoverFixupEventTag::eAnyAxis || axisType == CMoverFixupEventTag::eXAxis))
				{
					fStartPhase = (*it)->GetStart();
					fEndPhase = (*it)->GetEnd();
					return true;
				}
				else if ((*it)->GetData()->ShouldFixupY() && (axisType == CMoverFixupEventTag::eAnyAxis || axisType == CMoverFixupEventTag::eYAxis))
				{
					fStartPhase = (*it)->GetStart();
					fEndPhase = (*it)->GetEnd();
					return true;
				}
				else if (!bTranslation && axisType == CMoverFixupEventTag::eAnyAxis)
				{
					fStartPhase = (*it)->GetStart();
					fEndPhase = (*it)->GetEnd();
					return true;
				}
				++it;
			}
		}
		return false;
	}

	static bool FindIkTags(const crClip* pClip, float& fStartPhase, float& fEndPhase, bool& bRight, bool bOn)
	{
		if (pClip && pClip->GetTags())
		{
			const CEventTag<CIkEventTag>* pEventTag = FindFirstEventTag<CIkEventTag, crPropertyAttributeBool>(pClip, Ik, On, bOn);
			if (pEventTag)
			{
				bRight = pEventTag->GetData()->IsRight();
				fStartPhase = pEventTag->GetStart();
				fEndPhase = pEventTag->GetEnd();
				return true;
			}
		}
		return false;
	}

	static bool FindSpecificIkTags(const crClip* pClip, float& fStartPhase, float& fEndPhase, bool bRight, bool bOn)
	{
		if (pClip && pClip->GetTags())
		{
			CTagIteratorAttributeValue<CIkEventTag, crPropertyAttributeBool, bool> it(*pClip->GetTags(), fStartPhase, fEndPhase, Ik, On, bOn);

			while (*it)
			{
				if ((bRight && (*it)->GetData()->IsRight()) || (!bRight && !(*it)->GetData()->IsRight()))
				{
					fStartPhase = (*it)->GetStart();
					fEndPhase = (*it)->GetEnd();
					return true;
				}
				++it;
			}	
		}
		return false;
	}
		
	static bool FindEventStartEndPhases(const crClip* pClip, Key propertyKey, float& fStartPhase, float& fEndPhase, float startPhase = 0.0f, float endPhase = 1.0f)
	{
		const crTag* pTag = FindFirstEventTag(pClip, propertyKey, startPhase, endPhase);

		if (pTag)
		{
			fStartPhase = pTag->GetStart();
			fEndPhase = pTag->GetEnd();
			return true;
		}
		else
		{
			return false;
		}
	}	

	static bool FindPairedClipAndClipSetNameHashes(const crClip* pClip, u32& uClipSetNameHash, u32& uClipNameHash)
	{
		uClipSetNameHash = 0;
		uClipNameHash = 0;

		const CEventTag<CPairedClipTag>* pPairedClipSetTag = FindFirstEventTag<CPairedClipTag, crPropertyAttributeHashString>(pClip, PairedClip, PairedClipSetName);
		if (pPairedClipSetTag)
		{
			uClipSetNameHash = pPairedClipSetTag->GetData()->GetPairedClipSetNameHash();
		}

		const CEventTag<CPairedClipTag>* pPairedClipTag = FindFirstEventTag<CPairedClipTag, crPropertyAttributeHashString>(pClip, PairedClip, PairedClipName);
		if (pPairedClipTag)
		{
			uClipNameHash = pPairedClipTag->GetData()->GetPairedClipNameHash();
		}
		return (uClipSetNameHash != 0 && uClipNameHash != 0);
	}

	static bool HasMoveEvent(const crClip* pClip, atHashString eventNameHashString)
	{
		if (pClip && pClip->GetTags())
		{
			const CEventTag<crTag>* pTag = FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(pClip, MoveEvent, MoveEvent, eventNameHashString);
			if (pTag)
			{
				return true;
			}
		}
		return false;
	}

	static bool GetMoveEventStartAndEndPhases(const crClip* pClip, atHashString eventNameHashString, float& fStartPhase, float& fEndPhase)
	{
		if (pClip && pClip->GetTags())
		{
			const CEventTag<crTag>* pTag = FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(pClip, MoveEvent, MoveEvent, eventNameHashString);
			if (pTag)
			{
				fStartPhase = pTag->GetStart();
				fEndPhase = pTag->GetEnd();
				return true;
			}
		}
		return false;
	}

	static bool FindWalkInterruptStartEndPhases(const crClip* pClip, float& fStartPhase, float& fEndPhase)
	{
		if (pClip && pClip->GetTags())
		{
			const crTag* pTag = FindFirstEventTag(pClip, WalkInterruptible);
			if (pTag)
			{
				fStartPhase = pTag->GetStart();
				fEndPhase = pTag->GetEnd();
				return true;
			}
		}
		return false;
	}

	static bool FindRunInterruptStartEndPhases(const crClip* pClip, float& fStartPhase, float& fEndPhase)
	{
		if (pClip && pClip->GetTags())
		{
			const crTag* pTag = FindFirstEventTag(pClip, RunInterruptible);
			if (pTag)
			{
				fStartPhase = pTag->GetStart();
				fEndPhase = pTag->GetEnd();
				return true;
			}
		}
		return false;
	}

	static bool FindBlendInUpperBodyAimStartEndPhases(const crClip* pClip, float& fStartPhase, float& fEndPhase)
	{
		if (pClip && pClip->GetTags())
		{
			const crTag* pTag = FindFirstEventTag(pClip, BlendInUpperBodyAim);
			if (pTag)
			{
				fStartPhase = pTag->GetStart();
				fEndPhase = pTag->GetEnd();
				return true;
			}
		}
		return false;
	}

	static bool FindBlendInBlindFireAimStartEndPhases(const crClip* pClip, float& fStartPhase, float& fEndPhase)
	{
		if (pClip && pClip->GetTags())
		{
			const crTag* pTag = FindFirstEventTag(pClip, BlendInBlindFireAim);
			if (pTag)
			{
				fStartPhase = pTag->GetStart();
				fEndPhase = pTag->GetEnd();
				return true;
			}
		}
		return false;
	}

	// RETURNS: the currently active motion blur event tag on the active camera target entity.
	static const CMotionBlurEventTag::MotionBlurData* GetCurrentMotionBlur();
	
};

#endif // EVENT_TAGS_H
