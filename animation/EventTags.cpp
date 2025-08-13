#include "fwmaths/Random.h"
#include "animation/EventTags.h"
#include "optimisations.h"


//rage includes
#include "crmetadata/propertyattributes.h"
#include "crskeleton/skeleton.h"

// framework includes
#include "fwanimation/animdirector.h"

//game includes
#include "animation/anim_channel.h"
#include "camera/viewports/ViewportManager.h"
#include "ik/IkManager.h"
#include "ik/IkRequest.h"
#include "peds/Ped.h"
#include "scene/DynamicEntity.h"
#include "scene/world/GameWorld.h"

ANIM_OPTIMISATIONS()

CClipEventTags::Key CClipEventTags::TimeOfPreDelay("TimeOfPreDelay",0xAAA6AF06);
CClipEventTags::Key CClipEventTags::CriticalFrame("CriticalFrame",0xFE8E637);
CClipEventTags::Key CClipEventTags::MoveEvent("MoveEvent",0x7EA9DFC4);
CClipEventTags::Key CClipEventTags::Foot("Foot",0xB82E430B);
CClipEventTags::Key CClipEventTags::Heel("Heel",0x2BC03824);
CClipEventTags::Key CClipEventTags::Left("Left",0x6FA34840);
CClipEventTags::Key CClipEventTags::Right("Right",0xB8CCC339);
CClipEventTags::Key CClipEventTags::Fire("Fire",0xD30A036B);
CClipEventTags::Key CClipEventTags::Index("Index",0x91501139);
CClipEventTags::Key CClipEventTags::CreateOrdnance("CreateOrdnance",0x40BCB3F4);
CClipEventTags::Key CClipEventTags::Object("Object",0x39958261);
CClipEventTags::Key CClipEventTags::Create("Create",0x629712B0);
CClipEventTags::Key CClipEventTags::Destroy("Destroy",0x6D957FEE);
CClipEventTags::Key CClipEventTags::Release("Release",0xBC7C13B5);
CClipEventTags::Key CClipEventTags::Start("Start",0x84DC271F);
CClipEventTags::Key CClipEventTags::Interruptible("Interruptible",0x550A14CF);
CClipEventTags::Key CClipEventTags::WalkInterruptible("WalkInterruptible",0xEB04B18E);
CClipEventTags::Key CClipEventTags::RunInterruptible("RunInterruptible",0x5F9531C);
CClipEventTags::Key CClipEventTags::ShellEjected("ShellEjected",0xB98A4092);
CClipEventTags::Key CClipEventTags::Door("Door",0xBEF83F1B);
CClipEventTags::Key CClipEventTags::Smash("Smash",0xDEFF292E);
CClipEventTags::Key CClipEventTags::BlendOutWithDuration("BlendOutWithDuration",0xE8703510);
CClipEventTags::Key CClipEventTags::Ik("Ik",0xA5F6D578);
CClipEventTags::Key CClipEventTags::On("On",0x88F29488);
CClipEventTags::Key CClipEventTags::ArmsIk("ArmsIk",0x540C5A2F);
CClipEventTags::Key CClipEventTags::LegsIk("LegsIk",0xD56454B0);
CClipEventTags::Key CClipEventTags::Allowed("Allowed",0xA7C897B9);
CClipEventTags::Key CClipEventTags::BlendIn("BlendIn",0xE36964B6);
CClipEventTags::Key CClipEventTags::BlendOut("BlendOut",0x6DD1ABBC);
CClipEventTags::Key CClipEventTags::LegsIkOptions("LegsIkOptions",0xEEBC9B6E);
CClipEventTags::Key CClipEventTags::LockFootHeight("LockFootHeight",0xF84DF5A);

CClipEventTags::Key CClipEventTags::LookIk("LookIk",0x4977AB78);
CClipEventTags::Key CClipEventTags::LookIkControl("LookIkControl",0x1EFF20B5);
CClipEventTags::Key CClipEventTags::LookIkTurnSpeed("LookIkTurnSpeed",0x2C21BE1D);
CClipEventTags::Key CClipEventTags::LookIkBlendInSpeed("LookIkBlendInSpeed",0x16CDA67);
CClipEventTags::Key CClipEventTags::LookIkBlendOutSpeed("LookIkBlendOutSpeed",0x7793A049);
CClipEventTags::Key CClipEventTags::EyeIkReferenceDirection("EyeIkReferenceDirection",0x354DB74A);
CClipEventTags::Key CClipEventTags::HeadIkReferenceDirection("HeadIkReferenceDirection",0x8768A201);
CClipEventTags::Key CClipEventTags::NeckIkReferenceDirection("NeckIkReferenceDirection",0x98B66472);
CClipEventTags::Key CClipEventTags::TorsoIkReferenceDirection("TorsoIkReferenceDirection",0x3557F18A);
CClipEventTags::Key CClipEventTags::HeadIkYawDeltaLimit("HeadIkYawDeltaLimit",0x18BD13A);
CClipEventTags::Key CClipEventTags::HeadIkPitchDeltaLimit("HeadIkPitchDeltaLimit",0x82932818);
CClipEventTags::Key CClipEventTags::NeckIkYawDeltaLimit("NeckIkYawDeltaLimit",0x7E00417D);
CClipEventTags::Key CClipEventTags::NeckIkPitchDeltaLimit("NeckIkPitchDeltaLimit",0xF61030C3);
CClipEventTags::Key CClipEventTags::TorsoIkYawDeltaLimit("TorsoIkYawDeltaLimit",0x60980860);
CClipEventTags::Key CClipEventTags::TorsoIkPitchDeltaLimit("TorsoIkPitchDeltaLimit",0xC74C7EC0);
CClipEventTags::Key CClipEventTags::HeadIkAttitude("HeadIkAttitude",0xEB0FBB8B);
CClipEventTags::Key CClipEventTags::TorsoIkLeftArm("TorsoIkLeftArm",0x3852473C);
CClipEventTags::Key CClipEventTags::TorsoIkRightArm("TorsoIkRightArm",0x8C5AF2E);

CClipEventTags::Key CClipEventTags::CameraShake("CameraShake",0xe34a0cb0);
CClipEventTags::Key CClipEventTags::CameraShakeRef("CameraShakeRef",0xc3cb3a12);

CClipEventTags::Key CClipEventTags::PadShake("PadShake",0x44ad78be);
CClipEventTags::Key CClipEventTags::PadShakeIntensity("PadShakeIntensity",0xf3559cd7);
CClipEventTags::Key CClipEventTags::PadShakeDuration("PadShakeDurationMS",0x3966e7ec);

CClipEventTags::Key CClipEventTags::SelfieHeadControl("SelfieHeadControl",0x378904C7);
CClipEventTags::Key CClipEventTags::Weight("Weight",0x9C0E774C);
CClipEventTags::Key CClipEventTags::SelfieLeftArmControl("SelfieLeftArmControl",0xE61C5647);
CClipEventTags::Key CClipEventTags::CameraRelativeWeight("CameraRelativeWeight",0xBB51C49B);
CClipEventTags::Key CClipEventTags::JointTargetWeight("JointTargetWeight",0x7777B06C);
CClipEventTags::Key CClipEventTags::JointTarget("JointTarget",0xBEDB3AC4);

CClipEventTags::Key CClipEventTags::GestureControl("GestureControl",0x11F0C2D9);
CClipEventTags::Key CClipEventTags::OverrideDefaultSettings("OverrideDefaultSettings",0xCF1A7DC6);
CClipEventTags::Key CClipEventTags::DefaultRootStabilization("DefaultRootStabilization",0x8F992A37);
CClipEventTags::Key CClipEventTags::DefaultFilterID("DefaultFilterID",0x869461A2);

CClipEventTags::Key CClipEventTags::ApplyForce("ApplyForce",0x6A287745);
CClipEventTags::Key CClipEventTags::BlockRagdoll("BlockRagdoll",0xDE53B1C9);
CClipEventTags::Key CClipEventTags::BlockAll("BlockAll",0xCBE33849);
CClipEventTags::Key CClipEventTags::BlockFromThisVehicleMoving("BlockFromThisVehicleMoving",0xCFC25609);
CClipEventTags::Key CClipEventTags::ObjectVfx("ObjectVfx",0xEEA25B57);
CClipEventTags::Key CClipEventTags::Register("Register",0x344F0278);
CClipEventTags::Key CClipEventTags::Trigger("Trigger",0xC3AFD061);
CClipEventTags::Key CClipEventTags::VFXName("VFXName",0xAB95F43A);
CClipEventTags::Key CClipEventTags::CanSwitchToNm("CanSwitchToNm",0x95955F82);
CClipEventTags::Key CClipEventTags::AdjustAnimVel("AdjustAnimVel",0x1D52EF13);
CClipEventTags::Key CClipEventTags::Audio("Audio",0xB80C6AE8);
CClipEventTags::Key CClipEventTags::LoopingAudio("LoopingAudio",0xA31D8F23);
CClipEventTags::Key CClipEventTags::Id("Id",0x1B60404D);
CClipEventTags::Key CClipEventTags::RelevantWeightThreshold("RelevantWeightThreshold",0x368D9C33);
CClipEventTags::Key CClipEventTags::MoverFixup("MoverFixup",0x5B76264C);
CClipEventTags::Key CClipEventTags::Translation("Translation",0x8CC5DC4D);
CClipEventTags::Key CClipEventTags::Rotation("Rotation",0x77DD4010);
CClipEventTags::Key CClipEventTags::TransX("TransX",0x5B3D4194);
CClipEventTags::Key CClipEventTags::TransY("TransY",0x79897E2C);
CClipEventTags::Key CClipEventTags::TransZ("TransZ",0x47C79AA9);
CClipEventTags::Key CClipEventTags::VehicleModel("VehicleModel",0x125abad6);
CClipEventTags::Key CClipEventTags::DistanceMarker("DistanceMarker",0x3234DE90);
CClipEventTags::Key CClipEventTags::Distance("Distance",0xD6FF9582);
CClipEventTags::Key CClipEventTags::FixupToGetInPoint("FixupToGetInPoint",0xadbfe4e1);

CClipEventTags::Key CClipEventTags::Facial("Facial",0x511E56C2);
CClipEventTags::Key CClipEventTags::NameId("NameId",0x804A1E4A);
CClipEventTags::Key CClipEventTags::IsClipName("IsClipName",0x0C905639);

CClipEventTags::Key CClipEventTags::BlendToVehicleLighting("BlendToVehicleLighting",0xF974222B);
CClipEventTags::Key CClipEventTags::IsGettingIntoVehicle("IsGettingIntoVehicle",0xFEA1EE9D);

CClipEventTags::Key CClipEventTags::CutsceneBlendToVehicleLighting("CutsceneBlendToVehicleLighting",0xB8336AF5);
CClipEventTags::Key CClipEventTags::TargetValue("TargetValue",0x4A8C01E3);
CClipEventTags::Key CClipEventTags::BlendTime("BlendTime",0x28CFF008);

CClipEventTags::Key CClipEventTags::CutsceneEnterExitVehicle("CutsceneEnterExitVehicle",0x50b00990);
CClipEventTags::Key CClipEventTags::VehicleSceneHandle("VehicleSceneHandle",0x2e886b52);
CClipEventTags::Key CClipEventTags::SeatIndex("SeatIndex",0xc642d247);

CClipEventTags::Key CClipEventTags::BlockCutsceneSkipping("BlockCutsceneSkipping", 0xf870c7af);

CClipEventTags::Key CClipEventTags::SweepMarker("SweepMarker",0xA095728D);
CClipEventTags::Key CClipEventTags::SweepMinYaw("SweepMinYaw",0x7099E484);
CClipEventTags::Key CClipEventTags::SweepMaxYaw("SweepMaxYaw",0x1EF27FAB);
CClipEventTags::Key CClipEventTags::SweepPitch("SweepPitch",0xDF7F7B);

CClipEventTags::Key CClipEventTags::RandomizeStartPhase("RandomiseStartPhase",0xECFE7FC4);
CClipEventTags::Key CClipEventTags::MinPhase("MinPhase",0xFEC0EAB9);
CClipEventTags::Key CClipEventTags::MaxPhase("MaxPhase",0x6531A000);
CClipEventTags::Key CClipEventTags::RandomizeRate("RandomiseRate",0xF9A6A06C);
CClipEventTags::Key CClipEventTags::MinRate("MinRate",0x82A2788B);
CClipEventTags::Key CClipEventTags::MaxRate("MaxRate",0x6013C65D);
CClipEventTags::Key CClipEventTags::MeleeHoming("MeleeHoming",0x7D508457);
CClipEventTags::Key CClipEventTags::MeleeCollision("MeleeCollision",0xDFAFBD8C);
CClipEventTags::Key CClipEventTags::MeleeInvulnerability("MeleeInvulnerability",0x577687C7);
CClipEventTags::Key CClipEventTags::MeleeDodge("MeleeDodge",0x3E141A7C);
CClipEventTags::Key CClipEventTags::MeleeContact("MeleeContact",0xC1E9C6E6);
CClipEventTags::Key CClipEventTags::MeleeFacialAnim("MeleeFacialAnim",0XEED4EAB0);
CClipEventTags::Key CClipEventTags::SwitchToRagdoll("SwitchToRagdoll",0xC38BDAB2);
CClipEventTags::Key CClipEventTags::Detonation("Detonation",0x856E9FA2);
CClipEventTags::Key CClipEventTags::StartEngine("StartEngine",0x43377B8E);
CClipEventTags::Key CClipEventTags::PairedClip("PairedClip",0x44D39D32);
CClipEventTags::Key CClipEventTags::PairedClipName("PairedClipName",0x398BB95);
CClipEventTags::Key CClipEventTags::PairedClipSetName("PairedClipSetName",0x70DB63EC);
CClipEventTags::Key CClipEventTags::TagSyncBlendOut("TagSyncBlendOut",0x7E446A54);
CClipEventTags::Key CClipEventTags::MoveEndsInWalk("MOVE_ENDS_IN_WALK",0x6F5B3AB6);
CClipEventTags::Key CClipEventTags::BlendInUpperBodyAim("BlendInUpperBodyAim",0xC69950E0);
CClipEventTags::Key CClipEventTags::BlendInBlindFireAim("BlendInBlindFireAim",0x187118E);
CClipEventTags::Key CClipEventTags::PhoneRing("PhoneRing",0xD93A7EB6);
CClipEventTags::Key CClipEventTags::FallExit("FallExit",0x3D169B5E);
CClipEventTags::Key CClipEventTags::FlashLight("FlashLight",0x630AA491);
CClipEventTags::Key CClipEventTags::FlashLightName("FlashLightName",0x5172E73D);
CClipEventTags::Key CClipEventTags::FlashLightOn("FlashLightOn",0x5EF7274F);
CClipEventTags::Key CClipEventTags::SharkAttackPedDamage("SharkAttackPedDamage", 0x7576EDDB);
CClipEventTags::Key CClipEventTags::RestoreHair("RestoreHair", 0x547494E6);

CClipEventTags::Key CClipEventTags::BranchGetup("BranchGetup",0xBB289BE7);
CClipEventTags::Key CClipEventTags::BlendOutItem("BlendOutItem",0x76F011BA);

CClipEventTags::Key CClipEventTags::FirstPersonCamera("FirstPersonCamera",0xAFF55091);
CClipEventTags::Key CClipEventTags::FirstPersonCameraInput("FirstPersonCameraInput",0xC261B2D3);
CClipEventTags::Key CClipEventTags::MotionBlur("MotionBlur",0xfdbaead5); 
CClipEventTags::Key CClipEventTags::FirstPersonHelmetCull("FirstPersonHelmetCull", 0xae6df4aa); 
CClipEventTags::Key CClipEventTags::FirstPersonHelmetCullVehicleName("VehicleModelName", 0xad957bf7); 

const atHashValue LookIKHashSlowest("Slowest");
const atHashValue LookIKHashSlow("Slow");
const atHashValue LookIKHashNorm("Normal");
const atHashValue LookIKHashFast("Fast");
const atHashValue LookIKHashFastest("Fastest");
const atHashValue LookIKHashRefDirAbsolute("Look Target");
const atHashValue LookIKHashRefDirHeadFwd("Head Forward");
const atHashValue LookIKHashRefDirFacingDir("Facing Direction");
const atHashValue LookIKHashRefDirMoverFwd("Mover forward");
const atHashValue LookIKHashDeltaLimitDisabled("Disabled");
const atHashValue LookIKHashDeltaLimitNarrowest("Narrowest");
const atHashValue LookIKHashDeltaLimitNarrow("Narrow");
const atHashValue LookIKHashDeltaLimitWide("Wide");
const atHashValue LookIKHashDeltaLimitWidest("Widest");
const atHashValue LookIKHashHeadAttDisabled("Disabled");
const atHashValue LookIKHashHeadAttLow("Low");
const atHashValue LookIKHashHeadAttMed("Medium");
const atHashValue LookIKHashHeadAttFull("Full");
const atHashValue LookIKHashArmCompDisabled("Disabled");
const atHashValue LookIKHashArmCompLow("Low");
const atHashValue LookIKHashArmCompMed("Medium");
const atHashValue LookIKHashArmCompFull("Full");
const atHashValue LookIKHashArmCompIK("IK");

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CObjectEventTag::IsCreate() const
{
	animAssert(GetKey()==Object.GetHash());
	
	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Create.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CObjectEventTag::IsDestroy() const
{
	animAssert(GetKey()==Object.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(CClipEventTags::Destroy.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CObjectEventTag::IsRelease() const
{
	animAssert(GetKey()==Object.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Release.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

int CClipEventTags::CFireEventTag::GetIndex() const
{
	animAssert(GetKey()==Fire.GetHash());

	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(Index.GetHash()));
	if (attrib)
	{
		return attrib->GetInt();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CBlendOutEventTag::GetBlendOutDuration() const
{
	animAssert(GetKey()==BlendOutWithDuration.GetHash());

	static atHashValue blendOutDurationHash("BlendOutDuration");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(blendOutDurationHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CIkEventTag::IsOn() const
{
	animAssert(GetKey()==Ik.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(On.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CIkEventTag::IsRight() const
{
	animAssert(GetKey()==Ik.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Right.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CArmsIKEventTag::GetAllowed() const
{
	animAssert(GetKey()==ArmsIk.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Allowed.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CArmsIKEventTag::GetLeft() const
{
	animAssert(GetKey()==ArmsIk.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Left.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CArmsIKEventTag::GetRight() const
{
	animAssert(GetKey()==ArmsIk.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Right.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CArmsIKEventTag::GetBlendIn() const
{
	animAssert(GetKey()==ArmsIk.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendIn.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 1.0f/SLOW_BLEND_IN_DELTA;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CArmsIKEventTag::GetBlendOut() const
{
	animAssert(GetKey()==ArmsIk.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendOut.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 1.0f/SLOW_BLEND_OUT_DELTA;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CLegsIKEventTag::GetAllowed() const
{
	animAssert(GetKey()==LegsIk.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Allowed.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CLegsIKEventTag::GetBlendIn() const
{
	animAssert(GetKey()==LegsIk.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendIn.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 1.0f/SLOW_BLEND_IN_DELTA;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CLegsIKEventTag::GetBlendOut() const
{
	animAssert(GetKey()==LegsIk.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendOut.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 1.0f/SLOW_BLEND_OUT_DELTA;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CLegsIKOptionsEventTag::GetLeft() const
{
	animAssert(GetKey()==LegsIkOptions.GetHash());
	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Right.GetHash()));
	if (attrib)
	{
		return !attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CLegsIKOptionsEventTag::GetRight() const
{
	animAssert(GetKey()==LegsIkOptions.GetHash());
	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Right.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CLegsIKOptionsEventTag::GetLockFootHeight() const
{
	animAssert(GetKey()==LegsIkOptions.GetHash());
	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(LockFootHeight.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetTurnSpeed() const
{
	animAssert(GetKey()==LookIk.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(LookIkTurnSpeed.GetHash()));

	if (attrib)
	{
		u32 uHash = attrib->GetHashString().GetHash();

		if (uHash == LookIKHashSlow)
		{
			return (u8)LOOKIK_TURN_RATE_SLOW;
		}
		else if (uHash == LookIKHashNorm)
		{
			return (u8)LOOKIK_TURN_RATE_NORMAL;
		}
		else if (uHash == LookIKHashFast)
		{
			return (u8)LOOKIK_TURN_RATE_FAST;
		}
		else if (uHash == LookIKHashFastest)
		{
			return (u8)LOOKIK_TURN_RATE_FASTEST;
		}
	}

	return (u8)LOOKIK_TURN_RATE_NORMAL;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetBlendSpeed(const crPropertyAttributeHashString* attrib) const
{
	if (attrib)
	{
		u32 uHash = attrib->GetHashString().GetHash();

		if (uHash == LookIKHashSlowest)
		{
			return (u8)LOOKIK_BLEND_RATE_SLOWEST;
		}
		else if (uHash == LookIKHashSlow)
		{
			return (u8)LOOKIK_BLEND_RATE_SLOW;
		}
		else if (uHash == LookIKHashNorm)
		{
			return (u8)LOOKIK_BLEND_RATE_NORMAL;
		}
		else if (uHash == LookIKHashFast)
		{
			return (u8)LOOKIK_BLEND_RATE_FAST;
		}
		else if (uHash == LookIKHashFastest)
		{
			return (u8)LOOKIK_BLEND_RATE_FASTEST;
		}
	}

	return (u8)LOOKIK_BLEND_RATE_NORMAL;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetBlendInSpeed() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetBlendSpeed(static_cast<const crPropertyAttributeHashString*>(GetAttribute(LookIkBlendInSpeed.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetBlendOutSpeed() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetBlendSpeed(static_cast<const crPropertyAttributeHashString*>(GetAttribute(LookIkBlendOutSpeed.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetReferenceDirection(const crPropertyAttributeHashString* attrib) const
{
	if (attrib)
	{
		u32 uHash = attrib->GetHashString().GetHash();

		if (uHash == LookIKHashRefDirAbsolute)
		{
			return (u8)LOOKIK_REF_DIR_ABSOLUTE;
		}
		else if (uHash == LookIKHashRefDirHeadFwd)
		{
			return (u8)LOOKIK_REF_DIR_HEAD;
		}
		else if (uHash == LookIKHashRefDirFacingDir)
		{
			return (u8)LOOKIK_REF_DIR_FACING;
		}
		else if (uHash == LookIKHashRefDirMoverFwd)
		{
			return (u8)LOOKIK_REF_DIR_MOVER;
		}
	}

	return (u8)LOOKIK_REF_DIR_ABSOLUTE;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetEyeReferenceDirection() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetReferenceDirection(static_cast<const crPropertyAttributeHashString*>(GetAttribute(EyeIkReferenceDirection.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetHeadReferenceDirection() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetReferenceDirection(static_cast<const crPropertyAttributeHashString*>(GetAttribute(HeadIkReferenceDirection.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetNeckReferenceDirection() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetReferenceDirection(static_cast<const crPropertyAttributeHashString*>(GetAttribute(NeckIkReferenceDirection.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetTorsoReferenceDirection() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetReferenceDirection(static_cast<const crPropertyAttributeHashString*>(GetAttribute(TorsoIkReferenceDirection.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetDeltaLimit(const crPropertyAttributeHashString* attrib) const
{
	if (attrib)
	{
		u32 uHash = attrib->GetHashString().GetHash();

		if (uHash == LookIKHashDeltaLimitDisabled)
		{
			return (u8)LOOKIK_ROT_LIM_OFF;
		}
		else if (uHash == LookIKHashDeltaLimitNarrowest)
		{
			return (u8)LOOKIK_ROT_LIM_NARROWEST;
		}
		else if (uHash == LookIKHashDeltaLimitNarrow)
		{
			return (u8)LOOKIK_ROT_LIM_NARROW;
		}
		else if (uHash == LookIKHashDeltaLimitWide)
		{
			return (u8)LOOKIK_ROT_LIM_WIDE;
		}
		else if (uHash == LookIKHashDeltaLimitWidest)
		{
			return (u8)LOOKIK_ROT_LIM_WIDEST;
		}
	}

	return (u8)LOOKIK_ROT_LIM_OFF;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetHeadYawDeltaLimit() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetDeltaLimit(static_cast<const crPropertyAttributeHashString*>(GetAttribute(HeadIkYawDeltaLimit.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetHeadPitchDeltaLimit() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetDeltaLimit(static_cast<const crPropertyAttributeHashString*>(GetAttribute(HeadIkPitchDeltaLimit.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetNeckYawDeltaLimit() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetDeltaLimit(static_cast<const crPropertyAttributeHashString*>(GetAttribute(NeckIkYawDeltaLimit.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetNeckPitchDeltaLimit() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetDeltaLimit(static_cast<const crPropertyAttributeHashString*>(GetAttribute(NeckIkPitchDeltaLimit.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetTorsoYawDeltaLimit() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetDeltaLimit(static_cast<const crPropertyAttributeHashString*>(GetAttribute(TorsoIkYawDeltaLimit.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetTorsoPitchDeltaLimit() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetDeltaLimit(static_cast<const crPropertyAttributeHashString*>(GetAttribute(TorsoIkPitchDeltaLimit.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetHeadAttitude() const
{
	animAssert(GetKey()==LookIk.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(HeadIkAttitude.GetHash()));

	if (attrib)
	{
		u32 uHash = attrib->GetHashString().GetHash();

		if (uHash == LookIKHashHeadAttDisabled)
		{
			return (u8)LOOKIK_HEAD_ATT_OFF;
		}
		else if (uHash == LookIKHashHeadAttLow)
		{
			return (u8)LOOKIK_HEAD_ATT_LOW;
		}
		else if (uHash == LookIKHashHeadAttMed)
		{
			return (u8)LOOKIK_HEAD_ATT_MED;
		}
		else if (uHash == LookIKHashHeadAttFull)
		{
			return (u8)LOOKIK_HEAD_ATT_FULL;
		}
	}

	return (u8)LOOKIK_HEAD_ATT_OFF;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetArmCompensation(const crPropertyAttributeHashString* attrib) const
{
	if (attrib)
	{
		u32 uHash = attrib->GetHashString().GetHash();

		if (uHash == LookIKHashArmCompDisabled)
		{
			return (u8)LOOKIK_ARM_COMP_OFF;
		}
		else if (uHash == LookIKHashArmCompLow)
		{
			return (u8)LOOKIK_ARM_COMP_LOW;
		}
		else if (uHash == LookIKHashArmCompMed)
		{
			return (u8)LOOKIK_ARM_COMP_MED;
		}
		else if (uHash == LookIKHashArmCompFull)
		{
			return (u8)LOOKIK_ARM_COMP_FULL;
		}
		else if (uHash == LookIKHashArmCompIK)
		{
			return (u8)LOOKIK_ARM_COMP_IK;
		}
	}
	
	return (u8)LOOKIK_ARM_COMP_OFF;
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetLeftArmCompensation() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetArmCompensation(static_cast<const crPropertyAttributeHashString*>(GetAttribute(TorsoIkLeftArm.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u8 CClipEventTags::CLookIKEventTag::GetRightArmCompensation() const
{
	animAssert(GetKey()==LookIk.GetHash());
	return GetArmCompensation(static_cast<const crPropertyAttributeHashString*>(GetAttribute(TorsoIkRightArm.GetHash())));
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CCameraShakeTag::GetShakeRefHash() const
{
	animAssert(GetKey()==CameraShake.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(CameraShakeRef.GetHash()));

	if (attrib)
	{
		return attrib->GetHashString().GetHash();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CPadShakeTag::GetPadShakeDuration() const
{
	animAssert(GetKey()==PadShake.GetHash());

	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(PadShakeDuration.GetHash()));

	if (attrib)
	{
		return attrib->GetInt();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CPadShakeTag::GetPadShakeIntensity() const
{
	animAssert(GetKey()==PadShake.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(PadShakeIntensity.GetHash()));

	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CLookIKControlEventTag::GetAllowed() const
{
	animAssert(GetKey()==LookIkControl.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Allowed.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieHeadControlEventTag::eBlendType CClipEventTags::CSelfieHeadControlEventTag::GetBlendIn() const
{
	animAssert(GetKey()==SelfieHeadControl.GetHash());
	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(BlendIn.GetHash()));
	return attrib ? GetBlendType(attrib->GetHashString().GetHash()) : BtNormal;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieHeadControlEventTag::eBlendType CClipEventTags::CSelfieHeadControlEventTag::GetBlendOut() const
{
	animAssert(GetKey()==SelfieHeadControl.GetHash());
	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(BlendOut.GetHash()));
	return attrib ? GetBlendType(attrib->GetHashString().GetHash()) : BtNormal;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CSelfieHeadControlEventTag::GetWeight() const
{
	animAssert(GetKey()==SelfieHeadControl.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(Weight.GetHash()));
	return attrib ? attrib->GetFloat() : 1.0f;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieHeadControlEventTag::eBlendType CClipEventTags::CSelfieHeadControlEventTag::GetBlendType(u32 hash) const
{
	static const atHashValue aBlendTypeHash[BtCount] =
	{
		atHashValue("Slowest", 0x1AEFFE7E),
		atHashValue("Slow", 0xC4EE147F),
		atHashValue("Normal", 0x4F485502),
		atHashValue("Fast", 0x64B8047E),
		atHashValue("Fastest", 0x8546C8E8)
	};

	for (int blendType = 0; blendType < BtCount; ++blendType)
	{
		if (aBlendTypeHash[blendType] == hash)
		{
			return (eBlendType)blendType;
		}
	}

	return BtNormal;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CSelfieLeftArmControlEventTag::GetCameraRelativeWeight() const
{
	animAssert(GetKey()==SelfieLeftArmControl.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(CameraRelativeWeight.GetHash()));
	return attrib ? attrib->GetFloat() : 0.0f;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieLeftArmControlEventTag::eJointTarget CClipEventTags::CSelfieLeftArmControlEventTag::GetJointTarget() const
{
	animAssert(GetKey()==SelfieLeftArmControl.GetHash());
	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(JointTarget.GetHash()));
	return attrib ? (attrib->GetHashString().GetHash() == atHashValue("Head", 0x84533F51) ? JtHead : JtSpine) : JtHead;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CSelfieLeftArmControlEventTag::GetJointTargetWeight() const
{
	animAssert(GetKey()==SelfieLeftArmControl.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(JointTargetWeight.GetHash()));
	return attrib ? attrib->GetFloat() : 0.0f;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieLeftArmControlEventTag::eBlendType CClipEventTags::CSelfieLeftArmControlEventTag::GetBlendIn() const
{
	animAssert(GetKey()==SelfieLeftArmControl.GetHash());
	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(BlendIn.GetHash()));
	return attrib ? GetBlendType(attrib->GetHashString().GetHash()) : BtNormal;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieLeftArmControlEventTag::eBlendType CClipEventTags::CSelfieLeftArmControlEventTag::GetBlendOut() const
{
	animAssert(GetKey()==SelfieLeftArmControl.GetHash());
	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(BlendOut.GetHash()));
	return attrib ? GetBlendType(attrib->GetHashString().GetHash()) : BtNormal;
}

//////////////////////////////////////////////////////////////////////////

CClipEventTags::CSelfieLeftArmControlEventTag::eBlendType CClipEventTags::CSelfieLeftArmControlEventTag::GetBlendType(u32 hash) const
{
	static const atHashValue aBlendTypeHash[BtCount] =
	{
		atHashValue("Slowest", 0x1AEFFE7E),
		atHashValue("Slow", 0xC4EE147F),
		atHashValue("Normal", 0x4F485502),
		atHashValue("Fast", 0x64B8047E),
		atHashValue("Fastest", 0x8546C8E8)
	};

	for (int blendType = 0; blendType < BtCount; ++blendType)
	{
		if (aBlendTypeHash[blendType] == hash)
		{
			return (eBlendType)blendType;
		}
	}

	return BtNormal;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CFacialAnimEventTag::GetIsClipName() const
{
	animAssert(GetKey()==Facial.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(IsClipName.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CFacialAnimEventTag::GetNameIdHash() const
{
	animAssert(GetKey()==Facial.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(NameId.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString().GetHash();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

const atHashString* CClipEventTags::CFacialAnimEventTag::GetNameIdHashString() const
{
	animAssert(GetKey()==Facial.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(NameId.GetHash()));
	if (attrib)
	{
		return &attrib->GetHashString();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CBlendToVehicleLightingAnimEventTag::GetIsGettingIntoVehicle() const
{
	animAssert(GetKey()==BlendToVehicleLighting.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(IsGettingIntoVehicle.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	// Default is blend in.
	return true;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CCutsceneBlendToVehicleLightingAnimEventTag::GetTargetValue() const
{
	animAssert(GetKey()==CutsceneBlendToVehicleLighting.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(TargetValue.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	// Default is not in car
	return 0.0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CCutsceneBlendToVehicleLightingAnimEventTag::GetBlendTime() const
{
	animAssert(GetKey()==CutsceneBlendToVehicleLighting.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendTime.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	// Default is instant blend
	return 1000.0;
}

//////////////////////////////////////////////////////////////////////////

const atHashString CClipEventTags::CCutsceneEnterExitVehicleAnimEventTag::GetVehicleSceneHandle() const
{
	animAssert(GetKey()==CutsceneEnterExitVehicle.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(VehicleSceneHandle.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString();
	}

	return atHashString((u32)0);
}

//////////////////////////////////////////////////////////////////////////

const int CClipEventTags::CCutsceneEnterExitVehicleAnimEventTag::GetSeatIndex() const
{
	animAssert(GetKey()==CutsceneEnterExitVehicle.GetHash());

	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(SeatIndex.GetHash()));
	if (attrib)
	{
		return attrib->GetInt();
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////

const float CClipEventTags::CCutsceneEnterExitVehicleAnimEventTag::GetBlendIn() const
{
	animAssert(GetKey()==CutsceneEnterExitVehicle.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendIn.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

const float CClipEventTags::CCutsceneEnterExitVehicleAnimEventTag::GetBlendOut() const
{
	animAssert(GetKey()==CutsceneEnterExitVehicle.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(BlendOut.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CBlockRagdollTag::ShouldBlockAll() const
{
	animAssert(GetKey()==BlockRagdoll.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(BlockAll.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CBlockRagdollTag::ShouldBlockFromThisVehicleMoving() const
{
	animAssert(GetKey()==BlockRagdoll.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(BlockFromThisVehicleMoving.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

int CClipEventTags::CFixupToGetInPointTag::GetGetInPointIndex() const
{
	static atHashValue getInPointHash("GetInPoint");

	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(getInPointHash.GetHash()));
	if (attrib)
	{
		return attrib->GetInt();
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CGestureControlEventTag::GetAllowed() const
{
	animAssert(GetKey()==GestureControl.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Allowed.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CGestureControlEventTag::GetOverrideDefaultSettings() const
{
	animAssert(GetKey()==GestureControl.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(OverrideDefaultSettings.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CGestureControlEventTag::GetDefaultRootStabilization() const
{
	animAssert(GetKey()==GestureControl.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(DefaultRootStabilization.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

int CClipEventTags::CGestureControlEventTag::GetDefaultFilterID() const
{
	animAssert(GetKey()==GestureControl.GetHash());

	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(DefaultFilterID.GetHash()));
	if (attrib)
	{
		return attrib->GetInt();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CApplyForceTag::GetForce() const
{
	animAssert(GetKey()==ApplyForce.GetHash());

	static atHashValue forceHash("Force");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(forceHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

bool CClipEventTags::CApplyForceTag::GetXOffset(float &offset) const
{
	animAssert(GetKey()==ApplyForce.GetHash());

	static atHashValue forceHash("XOffset");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(forceHash.GetHash()));
	if (attrib)
	{
		offset = attrib->GetFloat();
		return true;
	}

	return false;
}

bool CClipEventTags::CApplyForceTag::GetYOffset(float &offset) const
{
	animAssert(GetKey()==ApplyForce.GetHash());

	static atHashValue forceHash("YOffset");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(forceHash.GetHash()));
	if (attrib)
	{
		offset = attrib->GetFloat();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CAudioEventTag::GetId() const
{
	animAssert(GetKey()==Audio.GetHash());
	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(Id.GetHash()));
	if (attrib)
	{
		return attrib->GetInt();
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CAudioEventTag::GetRelevantWeightThreshold() const
{
	animAssert(GetKey()==Audio.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(RelevantWeightThreshold.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CLoopingAudioEventTag::GetId() const
{
	animAssert(GetKey()==Audio.GetHash());
	const crPropertyAttributeInt* attrib = static_cast<const crPropertyAttributeInt*>(GetAttribute(Id.GetHash()));
	if (attrib)
	{
		return attrib->GetInt();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CLoopingAudioEventTag::GetRelevantWeightThreshold() const
{
	animAssert(GetKey()==Audio.GetHash());
	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(RelevantWeightThreshold.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CLoopingAudioEventTag::IsStart() const
{
	animAssert(GetKey()==Audio.GetHash());
	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Start.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CMoverFixupEventTag::ShouldFixupTranslation() const
{
	animAssert(GetKey()==MoverFixup.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Translation.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CMoverFixupEventTag::ShouldFixupRotation() const
{
	animAssert(GetKey()==MoverFixup.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Rotation.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CMoverFixupEventTag::ShouldFixupX() const
{
	animAssert(GetKey()==MoverFixup.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(TransX.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CMoverFixupEventTag::ShouldFixupY() const
{
	animAssert(GetKey()==MoverFixup.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(TransY.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CClipEventTags::CMoverFixupEventTag::ShouldFixupZ() const
{
	animAssert(GetKey()==MoverFixup.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(TransZ.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CMoverFixupEventTag::GetSpecificVehicleModelHashToApplyFixUpTo() const
{
	animAssert(GetKey()==MoverFixup.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(VehicleModel.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString().GetHash();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CDistanceMarkerTag::GetDistance() const
{
	animAssert(GetKey()==DistanceMarker.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(Distance.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CSweepMarkerTag::GetMinYaw() const
{
	animAssert(GetKey()==SweepMarker.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(SweepMinYaw.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

float CClipEventTags::CSweepMarkerTag::GetMaxYaw() const
{
	animAssert(GetKey()==SweepMarker.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(SweepMaxYaw.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

float CClipEventTags::CSweepMarkerTag::GetPitch() const
{
	animAssert(GetKey()==SweepMarker.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(SweepPitch.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CPairedClipTag::GetPairedClipNameHash() const
{
	animAssert(GetKey()==PairedClip.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(PairedClipName.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString().GetHash();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

u32 CClipEventTags::CPairedClipTag::GetPairedClipSetNameHash() const
{
	animAssert(GetKey()==PairedClip.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(PairedClipSetName.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString().GetHash();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CRandomizeStartPhaseProperty::PickRandomStartPhase(u32 seed/*=0*/) const
{
	animAssert(GetKey()==RandomizeStartPhase.GetHash());

	float minPhase = GetMinStartPhase();
	float maxPhase = GetMaxStartPhase();

	if (seed)
	{
		return (((float)((u8)seed)/255.0f) * (maxPhase-minPhase)) + minPhase; 
	}
	else
	{
		return fwRandom::GetRandomNumberInRange(minPhase, maxPhase);
	}
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CRandomizeStartPhaseProperty::GetMinStartPhase() const
{
	animAssert(GetKey()==RandomizeStartPhase.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(MinPhase.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 0.0f;

}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CRandomizeStartPhaseProperty::GetMaxStartPhase() const
{
	animAssert(GetKey()==RandomizeStartPhase.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(MaxPhase.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CRandomizeRateProperty::PickRandomRate(u32 seed/*=0*/) const
{
	animAssert(GetKey()==RandomizeRate.GetHash());

	float minRate = GetMinRate();
	float maxRate = GetMaxRate();
	if (seed)
	{
		return (((float)((u8)seed)/255.0f) * (maxRate-minRate)) + minRate; 
	}
	else
	{
		return fwRandom::GetRandomNumberInRange(minRate, maxRate);
	}
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CRandomizeRateProperty::GetMinRate() const
{
	animAssert(GetKey()==RandomizeRate.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(MinRate.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////

float CClipEventTags::CRandomizeRateProperty::GetMaxRate() const
{
	animAssert(GetKey()==RandomizeRate.GetHash());

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(MaxRate.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////

atHashString CClipEventTags::CBranchGetupTag::GetTrigger() const
{
	animAssert(GetKey()==BranchGetup.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(Trigger.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString();
	}

	return atHashString((u32)0);
}


//////////////////////////////////////////////////////////////////////////

atHashString CClipEventTags::CBranchGetupTag::GetBlendOutItemId() const
{
	animAssert(GetKey()==BranchGetup.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(BlendOutItem.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString();
	}

	return atHashString((u32)0);
}

//////////////////////////////////////////////////////////////////////////
bool CClipEventTags::CFirstPersonCameraEventTag::GetAllowed() const
{
	animAssert(GetKey()==FirstPersonCamera.GetHash());

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(Allowed.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CFirstPersonCameraEventTag::GetBlendInDuration() const
{
	animAssert(GetKey()==FirstPersonCamera.GetHash());

	static atHashValue blendInDurationHash("BlendInDuration");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(blendInDurationHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return -1.0f;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CFirstPersonCameraEventTag::GetBlendOutDuration() const
{
	animAssert(GetKey()==FirstPersonCamera.GetHash());

	static atHashValue blendOutDurationHash("BlendOutDuration");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(blendOutDurationHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return -1.0f;
}

//////////////////////////////////////////////////////////////////////////
bool CClipEventTags::CFirstPersonCameraInputEventTag::GetYawSpringInput() const
{
	animAssert(GetKey()==FirstPersonCameraInput.GetHash());

	static atHashValue yawSpringInputHash("YawSpringInput");

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(yawSpringInputHash.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CClipEventTags::CFirstPersonCameraInputEventTag::GetPitchSpringInput() const
{
	animAssert(GetKey()==FirstPersonCameraInput.GetHash());

	static atHashValue pitchSpringInputHash("PitchSpringInput");

	const crPropertyAttributeBool* attrib = static_cast<const crPropertyAttributeBool*>(GetAttribute(pitchSpringInputHash.GetHash()));
	if (attrib)
	{
		return attrib->GetBool();
	}

	return false;
}

CClipEventTags::CMotionBlurEventTag::MotionBlurData CClipEventTags::CMotionBlurEventTag::m_blurData;

//////////////////////////////////////////////////////////////////////////
u16 CClipEventTags::CMotionBlurEventTag::GetBoneId() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue boneIdHash("BoneId");

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(boneIdHash.GetHash()));
	if (attrib)
	{
		static atHashValue leftHandHash("SKEL_L_Hand");
		static atHashValue rightHandHash("SKEL_R_Hand");
		static atHashValue leftFootHash("SKEL_L_Foot");
		static atHashValue rightFootHash("SKEL_R_Foot");

		if (attrib->GetHashString().GetHash()==leftHandHash.GetHash())	return (u16)BONETAG_L_HAND;
		if (attrib->GetHashString().GetHash()==rightHandHash.GetHash())	return (u16)BONETAG_R_HAND;
		if (attrib->GetHashString().GetHash()==leftFootHash.GetHash())	return (u16)BONETAG_L_FOOT;
		if (attrib->GetHashString().GetHash()==rightFootHash.GetHash())	return (u16)BONETAG_R_FOOT;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CMotionBlurEventTag::GetBlendInDuration() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue blendInDurationHash("BlendInDuration");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(blendInDurationHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return -1.0f;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CMotionBlurEventTag::GetBlendOutDuration() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue blendOutDurationHash("BlendOutDuration");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(blendOutDurationHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return -1.0f;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CMotionBlurEventTag::GetBlurAmount() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue blurAmountHash("BlurAmount");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(blurAmountHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 256.0f;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CMotionBlurEventTag::GetVelocityBlend() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue velocityBlendHash("VelocityBlend");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(velocityBlendHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 0.9f;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CMotionBlurEventTag::GetNoisiness() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue noisinessHash("Noisiness");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(noisinessHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////
float CClipEventTags::CMotionBlurEventTag::GetMaximumVelocity() const
{
	animAssert(GetKey()==MotionBlur.GetHash());

	static atHashValue maximumVelocityHash("MaximumVelocity");

	const crPropertyAttributeFloat* attrib = static_cast<const crPropertyAttributeFloat*>(GetAttribute(maximumVelocityHash.GetHash()));
	if (attrib)
	{
		return attrib->GetFloat();
	}

	return 50.0f;
}

void CClipEventTags::CMotionBlurEventTag::PostPreRender()
{
	// Update the velocity
	if (m_blurData.m_pCameraTarget && !fwTimer::IsGamePaused())
	{
		const CDynamicEntity* pTarget = m_blurData.m_pCameraTarget;
		if (pTarget->GetSkeleton())
		{
			s32 boneIdx(-1);
			pTarget->GetSkeleton()->GetSkeletonData().ConvertBoneIdToIndex(m_blurData.m_boneId, boneIdx);

			if (boneIdx>-1 && boneIdx<pTarget->GetSkeleton()->GetBoneCount())
			{
				Mat34V newMatrix(V_IDENTITY);
				pTarget->GetSkeleton()->GetGlobalMtx(boneIdx, newMatrix);

				m_blurData.m_lastPosition = m_blurData.m_currentPosition;
				m_blurData.m_timesLastPositionSet++;

				const grcViewport* vp = gVpMan.GetUpdateGameGrcViewport();
				const Mat44V view = vp->GetViewMtx();
				m_blurData.m_currentPosition = Multiply(view, Vec4V(newMatrix.d(), ScalarV(V_ONE))).GetXYZ();
			}
		}
	}	
}

void CClipEventTags::CMotionBlurEventTag::Update()
{
	// Find the first person camera target:
	CPed* pPed = CGameWorld::FindLocalPlayer();

	if (pPed)
	{
		if (pPed != m_blurData.m_pCameraTarget)
		{
			// TODO - deal with switching targets!
			// just reset for now
			m_blurData.m_blend=0.0f;
			m_blurData.m_pCameraTarget = pPed;
		}
	}

	if (!m_blurData.m_pCameraTarget)
	{
		// reset the blur data
		m_blurData.Clear();
		return;
	}

	// Get the motion blur tag.

	const crProperty* pProperty = m_blurData.m_pCameraTarget->GetAnimDirector()->FindPropertyInMoveNetworks(MotionBlur);

	if (pProperty!=m_blurData.m_pActiveTag)
	{
		if (pProperty)
		{
			if (m_blurData.m_pActiveTag)
			{
				// the tag has changed! TODO - deal with switching to a new tag whilst running
				// just reset for now.
				m_blurData.m_blend=0.0f;
				m_blurData.m_lastPosition.ZeroComponents();
			}

			m_blurData.m_pActiveTag = static_cast<const CMotionBlurEventTag*>(pProperty);
			m_blurData.m_blendOutDuration = m_blurData.m_pActiveTag->GetBlendOutDuration();
			m_blurData.m_totalBlurAmount = m_blurData.m_pActiveTag->GetBlurAmount();
			m_blurData.m_boneId = m_blurData.m_pActiveTag->GetBoneId();
			m_blurData.m_VelocityBlend = m_blurData.m_pActiveTag->GetVelocityBlend();
			m_blurData.m_Noisiness = m_blurData.m_pActiveTag->GetNoisiness();
			m_blurData.m_MaximumVelocity = m_blurData.m_pActiveTag->GetMaximumVelocity();
		}
		else
		{
			// start the blend out.
			m_blurData.m_pActiveTag = NULL;
		}
	}

	bool blendingIn = m_blurData.m_pActiveTag!=NULL;

	if (blendingIn)
	{
		float blendInDuration = m_blurData.m_pActiveTag->GetBlendInDuration();
		if (blendInDuration==0.0f)
		{
			m_blurData.m_blend = 1.0f;
		}
		else
		{
			m_blurData.m_blend = Clamp(m_blurData.m_blend + (fwTimer::GetTimeStep()*(1.0f/blendInDuration)), 0.0f, 1.0f);
		}
	}
	else
	{
		float blendOutDuration = m_blurData.m_blendOutDuration;
		if (blendOutDuration==0.0f)
		{
			m_blurData.m_blend = 0.0f;
		}
		else
		{
			m_blurData.m_blend = Clamp(m_blurData.m_blend - (fwTimer::GetTimeStep()*(1.0f/blendOutDuration)), 0.0f, 1.0f);
		}

		if (m_blurData.m_blend==0.0f)
		{
			// finished blending out. Clear the blur data
			m_blurData.Clear();
		}
	}
}

void CClipEventTags::CMotionBlurEventTag::MotionBlurData::Clear()
{
	m_lastPosition.ZeroComponents();
	m_timesLastPositionSet = 0;
	m_pActiveTag = NULL;
	m_blendOutDuration = 0.0f;
	m_totalBlurAmount = 0.0f;
	m_boneId = 0;
	m_pCameraTarget = NULL;
	m_VelocityBlend = 0.9f;
	m_Noisiness = 1.0f;
	m_MaximumVelocity = 50.0f;
}

const CClipEventTags::CMotionBlurEventTag::MotionBlurData* CClipEventTags::CMotionBlurEventTag::GetCurrentMotionBlurIfValid()
{
	if (m_blurData.IsValid())
	{
		return &m_blurData;
	}
	else
	{
		return NULL;
	}
}

const CClipEventTags::CMotionBlurEventTag::MotionBlurData* CClipEventTags::GetCurrentMotionBlur()
{
	return CClipEventTags::CMotionBlurEventTag::GetCurrentMotionBlurIfValid();
}

//////////////////////////////////////////////////////////////////////////
u32 CClipEventTags::CFirstPersonHelmetCullEventTag::GetVehicleNameHash() const
{
	animAssert(GetKey()==FirstPersonHelmetCull.GetHash());

	const crPropertyAttributeHashString* attrib = static_cast<const crPropertyAttributeHashString*>(GetAttribute(FirstPersonHelmetCullVehicleName.GetHash()));
	if (attrib)
	{
		return attrib->GetHashString().GetHash();
	}
	return 0;
}