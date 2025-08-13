#ifndef __MARKERINFO
#define __MARKERINFO

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "atl/array.h"
#include "math/amath.h"
#include "system/bit.h"
#include "audioengine/entity.h"

// framework
#include "fwlocalisation/textTypes.h"

// game
#include "replaycoordinator/ReplayAudioTrackProvider.h"

//! Type of controls - Used to restrict editing certain controls under certain clip conditions
enum eMarkerControls
{
	MARKER_CONTROL_SPEED,

	MARKER_CONTROL_EFFECT,
	MARKER_CONTROL_DOF,

	MARKER_CONTROL_SOUND,
	MARKER_CONTROL_SFXVOLUME,
	MARKER_CONTROL_MUSICVOLUME,
	MARKER_CONTROL_DIALOGVOLUME,
	MARKER_CONTROL_SCORE_INTENSITY,

	MARKER_CONTROL_CAMERA,
	MARKER_CONTROL_CAMERA_TYPE,

	MARKER_CONTROL_MAX,			
};

// Flags for control properties that have been edited
//! NOTE - We have shipped this so you can only add bitfields to the END now. Re-organization of these can only happen for unshipped products now.
enum eMarkerEditedProperties : rage::u64
{
	MARKER_EDITED_PROPERTIES_NONE			= 0,

	// Time not flagged here as it wastes a bitfield; time is always edited on a marker
	MARKER_EDITED_PROPERTIES_TYPE					= BIT_64(0),
	MARKER_EDITED_PROPERTIES_SPEED					= BIT_64(1),
	MARKER_EDITED_PROPERTIES_TRANS_TYPE				= BIT_64(2),
	MARKER_EDITED_PROPERTIES_TRANS_DURATION			= BIT_64(3),

	MARKER_EDITED_PROPERTIES_GENERAL_MASK			= MARKER_EDITED_PROPERTIES_TYPE | MARKER_EDITED_PROPERTIES_SPEED |
														MARKER_EDITED_PROPERTIES_TRANS_TYPE | MARKER_EDITED_PROPERTIES_TRANS_DURATION,

	MARKER_EDITED_PROPERTIES_EFFECT					= BIT_64(4),
	MARKER_EDITED_PROPERTIES_EFFECT_INTENSITY		= BIT_64(5),
	MARKER_EDITED_PROPERTIES_EFFECT_SATURATION		= BIT_64(6),
	MARKER_EDITED_PROPERTIES_EFFECT_CONTRAST		= BIT_64(7),
	MARKER_EDITED_PROPERTIES_EFFECT_VIGNETTE		= BIT_64(8),
	MARKER_EDITED_PROPERTIES_EFFECT_BRIGHTNESS		= BIT_64(9),

	MARKER_EDITED_PROPERTIES_EFFECTS_MASK			= MARKER_EDITED_PROPERTIES_EFFECT | MARKER_EDITED_PROPERTIES_EFFECT_INTENSITY |
														MARKER_EDITED_PROPERTIES_EFFECT_SATURATION | MARKER_EDITED_PROPERTIES_EFFECT_CONTRAST |
														 MARKER_EDITED_PROPERTIES_EFFECT_VIGNETTE | MARKER_EDITED_PROPERTIES_EFFECT_BRIGHTNESS,

	MARKER_EDITED_PROPERTIES_CAM_TYPE				= BIT_64(10),
	MARKER_EDITED_PROPERTIES_CAM_SHAKE_TYPE			= BIT_64(11),
	MARKER_EDITED_PROPERTIES_CAM_SHAKE_INTENSITY	= BIT_64(12),
	MARKER_EDITED_PROPERTIES_CAM_SHAKE_SPEED		= BIT_64(13),
	MARKER_EDITED_PROPERTIES_CAM_BLEND_EASING		= BIT_64(14),
	MARKER_EDITED_PROPERTIES_CAM_LOOKAT_TARGET		= BIT_64(15),
	MARKER_EDITED_PROPERTIES_LOOKAT_OFFSET			= BIT_64(16),
	MARKER_EDITED_PROPERTIES_LOOKAT_ROLL			= BIT_64(17),
	MARKER_EDITED_PROPERTIES_CAM_ATTACH_TARGET		= BIT_64(18),
	MARKER_EDITED_PROPERTIES_CAM_ATTACH_TYPE		= BIT_64(19),
	MARKER_EDITED_PROPERTIES_CAM_BLEND				= BIT_64(20),

	MARKER_EDITED_PROPERTIES_CAM_ZOOM				= BIT_64(21),
	MARKER_EDITED_PROPERTIES_CAM_INOUT				= BIT_64(22),
	MARKER_EDITED_PROPERTIES_CAM_ROLL				= BIT_64(23),
	MARKER_EDITED_PROPERTIES_FREECAM_POS			= BIT_64(24),
	MARKER_EDITED_PROPERTIES_FREECAM_ROTATE			= BIT_64(25),

	MARKER_EDITED_PROPERTIES_DOF_MODE				= BIT_64(26),
	MARKER_EDITED_PROPERTIES_DOF_FOCUS_MODE			= BIT_64(27),
	MARKER_EDITED_PROPERTIES_DOF_FOCUS_DISTANCE		= BIT_64(28),
	MARKER_EDITED_PROPERTIES_DOF_FOCUS_TARGET		= BIT_64(29),
	MARKER_EDITED_PROPERTIES_DOF_INTENSITY			= BIT_64(30),

	MARKER_EDITED_PROPERTIES_CAMERA_MASK			= MARKER_EDITED_PROPERTIES_CAM_TYPE | MARKER_EDITED_PROPERTIES_CAM_SHAKE_TYPE |
														MARKER_EDITED_PROPERTIES_CAM_SHAKE_INTENSITY | MARKER_EDITED_PROPERTIES_CAM_SHAKE_SPEED | MARKER_EDITED_PROPERTIES_CAM_LOOKAT_TARGET |
														 MARKER_EDITED_PROPERTIES_CAM_ATTACH_TARGET | MARKER_EDITED_PROPERTIES_CAM_BLEND | MARKER_EDITED_PROPERTIES_CAM_BLEND_EASING |
														  MARKER_EDITED_PROPERTIES_CAM_ZOOM | MARKER_EDITED_PROPERTIES_CAM_INOUT |
														  MARKER_EDITED_PROPERTIES_LOOKAT_OFFSET | MARKER_EDITED_PROPERTIES_LOOKAT_ROLL |
														   MARKER_EDITED_PROPERTIES_CAM_ROLL | MARKER_EDITED_PROPERTIES_FREECAM_POS |
														    MARKER_EDITED_PROPERTIES_FREECAM_ROTATE | MARKER_EDITED_PROPERTIES_DOF_MODE |
															 MARKER_EDITED_PROPERTIES_DOF_FOCUS_MODE | MARKER_EDITED_PROPERTIES_DOF_FOCUS_DISTANCE |
															  MARKER_EDITED_PROPERTIES_DOF_FOCUS_TARGET | MARKER_EDITED_PROPERTIES_DOF_INTENSITY,

	MARKER_EDITED_PROPERTIES_SFXVOLUME				= BIT_64(31),
	MARKER_EDITED_PROPERTIES_SPEECHVOLUME			= BIT_64(32),
	MARKER_EDITED_PROPERTIES_MUSICVOLUME			= BIT_64(33),
	MARKER_EDITED_PROPERTIES_SCORE_INTENSITY		= BIT_64(34),
    MARKER_EDITED_PROPERTIES_MIC_TYPE				= BIT_64(35),
    MARKER_EDITED_PROPERTIES_SFX_CATEGORY			= BIT_64(36),
    MARKER_EDITED_PROPERTIES_SFX_HASH				= BIT_64(37),
    MARKER_EDITED_PROPERTIES_SFX_OS_VOL				= BIT_64(38),
    MARKER_EDITED_PROPERTIES_AMB_VOL				= BIT_64(39),

	MARKER_EDITED_PROPERTIES_AUDIO_MASK				= MARKER_EDITED_PROPERTIES_SFXVOLUME | MARKER_EDITED_PROPERTIES_SPEECHVOLUME |
														MARKER_EDITED_PROPERTIES_MUSICVOLUME | MARKER_EDITED_PROPERTIES_SCORE_INTENSITY |
														 MARKER_EDITED_PROPERTIES_MIC_TYPE,

	MARKER_EDITED_PROPERTIES_ALL_MASK				= MARKER_EDITED_PROPERTIES_GENERAL_MASK | MARKER_EDITED_PROPERTIES_EFFECTS_MASK |
														MARKER_EDITED_PROPERTIES_CAMERA_MASK | MARKER_EDITED_PROPERTIES_AUDIO_MASK,

	MARKER_EDITED_PROPERTIES_COUNT					= 40
};

enum __DEPRICATED_eMarkerCameraBlendType
{
	MARKER_DEPRICATED_CAMERA_BLEND_FIRST = 0,

	MARKER_DEPRICATED_CAMERA_BLEND_NONE = MARKER_DEPRICATED_CAMERA_BLEND_FIRST,
	MARKER_DEPRICATED_CAMERA_BLEND_LINEAR,
	MARKER_DEPRICATED_CAMERA_BLEND_SLOW_IN,
	MARKER_DEPRICATED_CAMERA_BLEND_SLOW_OUT,
	MARKER_DEPRICATED_CAMERA_BLEND_SLOW_IN_OUT,

	MARKER_DEPRICATED_CAMERA_BLEND_MAX
};

enum __DEPRICATED_eMarkerCameraSmoothType
{
	MARKER_DEPRICATED_CAMERA_SMOOTH_FIRST = 0,

	MARKER_DEPRICATED_CAMERA_SMOOTH_OFF = MARKER_DEPRICATED_CAMERA_SMOOTH_FIRST,
	MARKER_DEPRICATED_CAMERA_SMOOTH_ON,

	MARKER_DEPRICATED_CAMERA_SMOOTH_MAX
};

enum eMarkerCameraBlendType
{
	MARKER_CAMERA_BLEND_FIRST = 0,

	MARKER_CAMERA_BLEND_NONE = MARKER_CAMERA_BLEND_FIRST,
	MARKER_CAMERA_BLEND_LINEAR,
	MARKER_CAMERA_BLEND_SMOOTH,

	MARKER_CAMERA_BLEND_MAX
};

enum eMarkerCameraAttachType
{
	MARKER_CAMERA_ATTACH_FIRST = 0,

	MARKER_CAMERA_ATTACH_FULL = MARKER_CAMERA_ATTACH_FIRST,
	MARKER_CAMERA_ATTACH_POSITION_AND_HEADING,
	MARKER_CAMERA_ATTACH_POSITION_ONLY,

	MARKER_CAMERA_ATTACH_MAX
};

enum ReplayCameraType
{
	RPLYCAM_FIRST = 0,

	RPLYCAM_FRONT_LOW = RPLYCAM_FIRST,
	RPLYCAM_REAR,
	RPLYCAM_RIGHT_SIDE,
	RPLYCAM_LEFT_SIDE,
	RPLYCAM_OVERHEAD,
	RPLYCAM_RECORDED,	
	RPLYCAM_FREE_CAMERA,
	RPLYCAM_TARGET = RPLYCAM_FREE_CAMERA,	

	RPLYCAM_MAX,

	// Special camera type to indicate "Ignore me, use the previous non-inherited camera type". 
	RPLYCAM_INERHITED,
};

enum __DEPRICATED_eReplayDofMode
{
	MARKER_DEPRICATED_DOF_MODE_FIRST = 0,

	MARKER_DEPRICATED_DOF_MODE_DEFAULT = MARKER_DEPRICATED_DOF_MODE_FIRST,
	MARKER_DEPRICATED_DOF_MODE_AUTOMATIC,
	MARKER_DEPRICATED_DOF_MODE_CUSTOM,
	MARKER_DEPRICATED_DOF_MODE_NONE,

	MARKER_DEPRICATED_DOF_MODE_MAX
};

enum eReplayDofMode
{
	MARKER_DOF_MODE_FIRST = 0,

	MARKER_DOF_MODE_DEFAULT = MARKER_DOF_MODE_FIRST,
	MARKER_DOF_MODE_CUSTOM,
	MARKER_DOF_MODE_NONE,

	MARKER_DOF_MODE_MAX
};

enum eReplayDofFocusMode
{
	MARKER_FOCUS_FIRST = 0,

	MARKER_FOCUS_AUTO = MARKER_FOCUS_FIRST,
	MARKER_FOCUS_MANUAL,
	MARKER_FOCUS_TARGET,

	MARKER_FOCUS_MAX
};

enum ReplayMarkerType
{
	MARKER_TYPE_FIRST = 0,

	MARKER_TYPE_EDIT = MARKER_TYPE_FIRST,
	MARKER_TYPE_EDIT_AND_ANCHOR,
	MARKER_TYPE_ANCHOR,

	MARKER_TYPE_TOGGLABLE_MAX,

	MARKER_TYPE_EDIT_AND_TRANSITION = MARKER_TYPE_TOGGLABLE_MAX,

	MARKER_TYPE_MAX
};

enum ReplayMarkerSpeed
{
	MARKER_SPEED_FIRST = 0,

	MARKER_SPEED_5		= MARKER_SPEED_FIRST,
	MARKER_SPEED_20,
	MARKER_SPEED_35,
	MARKER_SPEED_50,
	MARKER_SPEED_100,
	MARKER_SPEED_125,
	MARKER_SPEED_150,
	MARKER_SPEED_175,
	MARKER_SPEED_200,

	MARKER_SPEED_MAX
};

enum ReplayMarkerTransitionType
{
	MARKER_TRANSITIONTYPE_FIRST = 0,

	MARKER_TRANSITIONTYPE_NONE = MARKER_TRANSITIONTYPE_FIRST,

	MARKER_TRANSITIONTYPE_FADEIN,
	MARKER_TRANSITIONTYPE_FADEOUT,

	MARKER_TRANSITIONTYPE_MAX
};

enum eReplayMarkerAudioIntensity
{
	MARKER_AUDIO_INTENSITY_FIRST = 0,

	MARKER_AUDIO_INTENSITY_MIN = MARKER_AUDIO_INTENSITY_FIRST,
	MARKER_AUDIO_INTENSITY_LOW,
	MARKER_AUDIO_INTENSITY_MED,
	MARKER_AUDIO_INTENSITY_HIGH,
	MARKER_AUDIO_INTENSITY_MAXIMUM,

	MARKER_AUDIO_INTENSITY_MAX
};

enum eMarkerShakeType
{
	MARKER_SHAKE_FIRST = 0,

	MARKER_SHAKE_NONE = MARKER_SHAKE_FIRST,
	MARKER_SHAKE_HAND,
	MARKER_SHAKE_DRUNK,
	MARKER_SHAKE_GROUND_VIBRATION,
	MARKER_SHAKE_AIR_TURBULANCE,
	MARKER_SHAKE_EXPLOSION,

	MARKER_SHAKE_MAX
};

enum eMarkerMicrophoneType
{
	MARKER_MICROPHONE_FIRST = 0,

	MARKER_MICROPHONE_DEFAULT = MARKER_MICROPHONE_FIRST,
	MARKER_MICROPHONE_AT_CAMERA,
	MARKER_MICROPHONE_AT_TARGET,
	MARKER_MICROPHONE_CINEMATIC,

	MARKER_MICROPHONE_MAX
};

//! For limiting the range of data for a copy/paste operation
enum eMarkerCopyContext
{
    COPY_CONTEXT_NONE = 0,
    COPY_CONTEXT_EFFECTS_ONLY = BIT(0),
    COPY_CONTEXT_AUDIO_ONLY = BIT(1),

    COPY_CONTEXT_ALL = MAX_UINT32
};

#define DEFAULT_FREECAM_QUATERNION		(0)
#define DEFAULT_FREECAM_HEADING			(0.f)
#define DEFAULT_FREECAM_PITCH			(0.f)
#define DEFAULT_FREECAM_ROLL			(0.f)

#define DEFAULT_POSTFX_INTENSITY		(100.f)
#define DEFAULT_MARKER_SATURATION		(0.f)
#define DEFAULT_MARKER_CONTRAST			(0.f)
#define DEFAULT_MARKER_VIGNETTE			(0.f)
#define DEFAULT_MARKER_BRIGHTNESS		(0.f)
#define DEFAULT_MARKER_FOCAL_DISTANCE	(10.f)
#define DEFAULT_MARKER_DOF_INTENSITY	(50.f)

#define DEFAULT_MARKER_TARGETINDEX			(-1)
#define DEFAULT_MARKER_DOF_TARGETINDEX		(0)
#define DEFAULT_ATTACH_TARGET_ID			(DEFAULT_MARKER_TARGETINDEX)
#define DEFAULT_ATTACH_TARGET_INDEX			(DEFAULT_MARKER_TARGETINDEX)
#define DEFAULT_LOOKAT_TARGET_ID			(DEFAULT_MARKER_TARGETINDEX)
#define DEFAULT_LOOKAT_TARGET_INDEX			(DEFAULT_MARKER_TARGETINDEX)
#define DEFAULT_DOF_FOCUS_ID				(DEFAULT_MARKER_TARGETINDEX)
#define DEFAULT_DOF_FOCUS_INDEX				(DEFAULT_MARKER_DOF_TARGETINDEX)
#define DEFAULT_ATTACH_TYPE					(MARKER_CAMERA_ATTACH_POSITION_AND_HEADING)
#define DEFAULT_VERTICAL_LOOKAT_OFFSET		(0.0f)
#define DEFAULT_HORIZONTAL_LOOKAT_OFFSET	(0.0f)
#define DEFAULT_LOOKAT_ROLL					(0.0f)

#define DEFAULT_MARKER_SPEED			(MARKER_SPEED_100)
#define DEFAULT_MARKER_TIME_MS			(0)
#define DEFAULT_MARKER_TRANSDUR_MS		(0)

#define DEFAULT_MARKER_TYPE				MARKER_TYPE_EDIT
#define DEFAULT_MARKER_CAMERA			RPLYCAM_RECORDED
#define DEFAULT_MARKER_TRANS_TYPE		(MARKER_TRANSITIONTYPE_NONE)
#define DEFAULT_MARKER_FOV				(50)
#define DEFAULT_MARKER_CAMBLEND_TYPE	(MARKER_CAMERA_BLEND_NONE)
#define DEFAULT_MARKER_CAMSHAKE_TYPE	(MARKER_SHAKE_NONE)
#define DEFAULT_MARKER_SHAKE_INTENSITY	(5)
#define DEFAULT_MARKER_SHAKE_SPEED		(MARKER_SPEED_100)
#define DEFAULT_MARKER_POST_EFFECT		(0)
#define DEFAULT_MARKER_SFX_VOL			(8)
#define DEFAULT_MARKER_MUSIC_VOL		(8)
#define DEFAULT_MARKER_SPEECH_VOL		(8)
#define DEFAULT_MARKER_SFX_OS_VOL		(8)
#define DEFAULT_MARKER_AMB_VOL		    (8)
#define DEFAULT_MARKER_SFX_CAT  		(0)
#define DEFAULT_MARKER_SFX_HASH 		(g_NullSoundHash)
#define DEFAULT_MARKER_DOF_MODE			(MARKER_DOF_MODE_DEFAULT)
#define DEFAULT_MARKER_DOF_FOCUS		(MARKER_FOCUS_AUTO)
#define DEFAULT_MARKER_MICROPHONE_TYPE	(MARKER_MICROPHONE_DEFAULT)
#define DEFAULT_MARKER_AUDIO_INTENSITY  (MARKER_AUDIO_INTENSITY_MED)
#define DEFAULT_MARKER_CAMMATRIX_VALID	(false)
#define DEFAULT_MARKER_SPEECH_ON		(true)
#define DEFAULT_MARKER_CAMBLEND_EASE	(true)

#define INVALID_TIME					(-1.f)

//! +/- time around a marker we disallow placing other markers, in milliseconds
#define MARKER_FRAME_BOUNDARY_MS		(33.37f)
#define MARKER_MIN_BOUNDARY_MS			( MARKER_FRAME_BOUNDARY_MS * 0.05f )

// TODO4FIVE (int)CCamGame::ms_maxGameFOV;
#define MARKER_MAX_FOV				(95) 
#define MARKER_MIN_FOV				(15)

#define MARKER_MAX_SHAKE			(10) 
#define MARKER_MIN_SHAKE			(1)

#define MARKER_MAX_SMOOTH_LEVEL		(1) 
#define MARKER_MIN_SMOOTH_LEVEL		(0)

#define MARKER_MAX_FOCAL_DIST		(1000.f) 
#define MARKER_MIN_FOCAL_DIST		(0.5f)

#define MARKER_MAX_PFX_INTENSITY	(100.f) 
#define MARKER_MIN_PFX_INTENSITY	(0.f)

#define MARKER_MAX_DOF_INTENSITY	(100.f) 
#define MARKER_MIN_DOF_INTENSITY	(0.f)

#define MARKER_MAX_SATURATION		(100.f) 
#define MARKER_MIN_SATURATION		(-100.f)

#define MARKER_MAX_CONTRAST			(100.f) 
#define MARKER_MIN_CONTRAST			(-100.f)

#define MARKER_MAX_VIGNETTE			(100.f) 
#define MARKER_MIN_VIGNETTE			(-100.f)

#define MARKER_MAX_BRIGHTNESS		(100.f) 
#define MARKER_MIN_BRIGHTNESS		(-100.f)

struct sReplayMarkerInfo
{
public: // declarations and variables

	struct sMarkerData_V1
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;

		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;
		rage::u32	m_timeMs;				
		rage::u32   m_transitionDurationMs;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;

		bool m_camMatrixValid;
		bool m_SpeechOn;
		bool m_AttachTypePositionOnly;

		sMarkerData_V1()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_AttachTypePositionOnly( false )
		{

		}
	};
	struct sMarkerData_V2
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;
		rage::u32	m_timeMs;				
		rage::u32   m_transitionDurationMs;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_cameraSmoothLevel;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_AttachTypePositionOnly;

		sMarkerData_V2()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_cameraSmoothLevel( 0 )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_AttachTypePositionOnly( false )
		{

		}
	};
	struct sMarkerData_V3
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;
		rage::u32	m_timeMs;				
		rage::u32   m_transitionDurationMs;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_cameraSmoothLevel;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_AttachTypePositionOnly;

		sMarkerData_V3()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_cameraSmoothLevel( 0 )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_AttachTypePositionOnly( false )
		{

		}
	};
	struct sMarkerData_V4
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;
		rage::u32	m_timeMs;				
		rage::u32   m_transitionDurationMs;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_cameraSmoothLevel;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_AttachTypePositionOnly;

		sMarkerData_V4()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_cameraSmoothLevel( 0 )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_AttachTypePositionOnly( false )
		{

		}
	};

	struct sMarkerData_V5
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;
		rage::u32	m_timeMs;				
		rage::u32   m_transitionDurationMs;
		rage::u32	m_shakeSpeed;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_cameraSmoothLevel;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_AttachTypePositionOnly;

		sMarkerData_V5()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_shakeSpeed( DEFAULT_MARKER_SPEED )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_cameraSmoothLevel( 0 )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_AttachTypePositionOnly( false )
		{

		}

	};

	struct sMarkerData_V6
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;
		rage::u32	m_timeMs;				
		rage::u32   m_transitionDurationMs;
		rage::u32	m_shakeSpeed;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_cameraSmoothLevel;
		rage::u8	m_attachType;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;

		sMarkerData_V6()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_shakeSpeed( DEFAULT_MARKER_SPEED )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_cameraSmoothLevel( 0 )
			, m_attachType( DEFAULT_ATTACH_TYPE )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
		{

		}

	};

	struct sMarkerData_V7
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 
		float		m_timeMs;				
		float		m_transitionDurationMs;

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;	
		rage::u32	m_shakeSpeed;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_CameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_cameraSmoothLevel;
		rage::u8	m_attachType;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;

		sMarkerData_V7()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_shakeSpeed( DEFAULT_MARKER_SPEED )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_CameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_cameraSmoothLevel( 0 )
			, m_attachType( DEFAULT_ATTACH_TYPE )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
		{

		}

	};
	struct sMarkerData_V8
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 
		float		m_timeMs;				
		float		m_transitionDurationMs;

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;	
		rage::u32	m_shakeSpeed;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_cameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_attachType;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_cameraEasing;

		sMarkerData_V8()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_shakeSpeed( DEFAULT_MARKER_SPEED )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_cameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_attachType( DEFAULT_ATTACH_TYPE )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_cameraEasing( DEFAULT_MARKER_CAMBLEND_EASE )
		{

		}

	};

	struct sMarkerData_V9
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 
		float		m_LookAtRoll;
		float		m_timeMs;				
		float		m_transitionDurationMs;

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;	
		rage::u32	m_shakeSpeed;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_cameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_attachType;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
		rage::u8	m_DialogVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_cameraEasing;

		sMarkerData_V9()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_LookAtRoll(DEFAULT_LOOKAT_ROLL)
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
			, m_shakeSpeed( DEFAULT_MARKER_SPEED )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_cameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_attachType( DEFAULT_ATTACH_TYPE )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_cameraEasing( DEFAULT_MARKER_CAMBLEND_EASE )
		{

		}
	};

    struct sMarkerData_V10
    {
        rage::u64	m_editedPropertyFlags;
        rage::u64	m_FreeCamQuaternion;
        rage::u64	m_AttachEntityQuaternion;

        float		m_AttachEntityPosition[3];
        float		m_FreeCamPosition[3];
        float		m_FreeCamPositionRelativeToAttachEntity[3];
        float		m_FreeCamRelativeHeading;
        float		m_FreeCamRelativePitch;
        float		m_FreeCamRelativeRoll;
        float		m_postFxIntensity;
        float		m_saturationIntensity;
        float		m_contrastIntensity;
        float		m_vignetteIntensity;
        float		m_brightness;
        float		m_dofFocalDistance;
        float		m_dofIntensity;
        float		m_VerticalLookAtOffset; 
        float		m_HorizontalLookAtOffset; 
        float		m_LookAtRoll;
        float		m_timeMs;				
        float		m_transitionDurationMs;

        rage::s32	m_AttachTargetId;
        rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
        rage::s32	m_LookAtTargetId;
        rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
        rage::s32	m_dofFocusTargetId;
        rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

        rage::u32	m_speed;	
        rage::u32	m_shakeSpeed;
        rage::u32	m_SFXHash;

        rage::u8	m_markerType;
        rage::u8	m_camType;
        rage::u8	m_transitionType;
        rage::u8	m_Fov;
        rage::u8	m_cameraBlendType;
        rage::u8	m_shakeType;
        rage::u8	m_shakeIntensity;
        rage::u8	m_attachType;
        rage::u8	m_activePostFx;
        rage::u8	m_SFXVolume;
        rage::u8	m_MusicVolume;	
        rage::u8	m_DialogVolume;	
        rage::u8	m_ScoreIntensity;
        rage::u8	m_dofMode;
        rage::u8	m_dofFocusMode;
        rage::u8	m_microphoneType;

        bool m_camMatrixValid;
        bool m_AttachEntityMatrixValid;
        bool m_SpeechOn;
        bool m_cameraEasing;

        sMarkerData_V10()
            : m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
            , m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
            , m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
            , m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
            , m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
            , m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
            , m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
            , m_saturationIntensity( DEFAULT_MARKER_SATURATION )
            , m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
            , m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
            , m_brightness( DEFAULT_MARKER_BRIGHTNESS )
            , m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
            , m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
            , m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
            , m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
            , m_LookAtRoll(DEFAULT_LOOKAT_ROLL)
            , m_timeMs( DEFAULT_MARKER_TIME_MS )
            , m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
            , m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
            , m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
            , m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
            , m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
            , m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
            , m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
            , m_speed( DEFAULT_MARKER_SPEED )
            , m_shakeSpeed( DEFAULT_MARKER_SPEED )
            , m_SFXHash( DEFAULT_MARKER_SFX_HASH )
            , m_markerType( DEFAULT_MARKER_TYPE ) 
            , m_camType( DEFAULT_MARKER_CAMERA )
            , m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
            , m_Fov( DEFAULT_MARKER_FOV )
            , m_cameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
            , m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
            , m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
            , m_attachType( DEFAULT_ATTACH_TYPE )
            , m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
            , m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
            , m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
            , m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
            , m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
            , m_dofMode( DEFAULT_MARKER_DOF_MODE )
            , m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
            , m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
            , m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
            , m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
            , m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
            , m_cameraEasing( DEFAULT_MARKER_CAMBLEND_EASE )
        {

        }

    };

    struct sMarkerData_V11
    {
        rage::u64	m_editedPropertyFlags;
        rage::u64	m_FreeCamQuaternion;
        rage::u64	m_AttachEntityQuaternion;

        float		m_AttachEntityPosition[3];
        float		m_FreeCamPosition[3];
        float		m_FreeCamPositionRelativeToAttachEntity[3];
        float		m_FreeCamRelativeHeading;
        float		m_FreeCamRelativePitch;
        float		m_FreeCamRelativeRoll;
        float		m_postFxIntensity;
        float		m_saturationIntensity;
        float		m_contrastIntensity;
        float		m_vignetteIntensity;
        float		m_brightness;
        float		m_dofFocalDistance;
        float		m_dofIntensity;
        float		m_VerticalLookAtOffset; 
        float		m_HorizontalLookAtOffset; 
        float		m_LookAtRoll;
        float		m_timeMs;				
        float		m_transitionDurationMs;

        rage::s32	m_AttachTargetId;
        rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
        rage::s32	m_LookAtTargetId;
        rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
        rage::s32	m_dofFocusTargetId;
        rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

        rage::u32	m_speed;	
        rage::u32	m_shakeSpeed;
        rage::u32	m_sfxHash;
        rage::u32   m_sfxCategory;

        rage::u8	m_markerType;
        rage::u8	m_camType;
        rage::u8	m_transitionType;
        rage::u8	m_Fov;
        rage::u8	m_cameraBlendType;
        rage::u8	m_shakeType;
        rage::u8	m_shakeIntensity;
        rage::u8	m_attachType;
        rage::u8	m_activePostFx;
        rage::u8	m_SFXVolume;
        rage::u8	m_MusicVolume;	
        rage::u8	m_DialogVolume;	
        rage::u8	m_ScoreIntensity;
        rage::u8	m_dofMode;
        rage::u8	m_dofFocusMode;
        rage::u8	m_microphoneType;

        bool m_camMatrixValid;
        bool m_AttachEntityMatrixValid;
        bool m_SpeechOn;
        bool m_cameraEasing;

        sMarkerData_V11()
            : m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
            , m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
            , m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
            , m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
            , m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
            , m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
            , m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
            , m_saturationIntensity( DEFAULT_MARKER_SATURATION )
            , m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
            , m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
            , m_brightness( DEFAULT_MARKER_BRIGHTNESS )
            , m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
            , m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
            , m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
            , m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
            , m_LookAtRoll(DEFAULT_LOOKAT_ROLL)
            , m_timeMs( DEFAULT_MARKER_TIME_MS )
            , m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
            , m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
            , m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
            , m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
            , m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
            , m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
            , m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
            , m_speed( DEFAULT_MARKER_SPEED )
            , m_shakeSpeed( DEFAULT_MARKER_SPEED )
            , m_sfxHash( DEFAULT_MARKER_SFX_HASH )
            , m_sfxCategory( DEFAULT_MARKER_SFX_CAT )
            , m_markerType( DEFAULT_MARKER_TYPE ) 
            , m_camType( DEFAULT_MARKER_CAMERA )
            , m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
            , m_Fov( DEFAULT_MARKER_FOV )
            , m_cameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
            , m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
            , m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
            , m_attachType( DEFAULT_ATTACH_TYPE )
            , m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
            , m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
            , m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
            , m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
            , m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
            , m_dofMode( DEFAULT_MARKER_DOF_MODE )
            , m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
            , m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
            , m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
            , m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
            , m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
            , m_cameraEasing( DEFAULT_MARKER_CAMBLEND_EASE )
        {

        }
    };
	
	//! Represents the current version of marker data.
	//! NOTE - Whenever you update, version the old style and update the loading code in CClip, 
	//!		   and add an appropriate assignment operator here.
	struct sMarkerData_Current
	{
		rage::u64	m_editedPropertyFlags;
		rage::u64	m_FreeCamQuaternion;
		rage::u64	m_AttachEntityQuaternion;

		float		m_AttachEntityPosition[3];
		float		m_FreeCamPosition[3];
		float		m_FreeCamPositionRelativeToAttachEntity[3];
		float		m_FreeCamRelativeHeading;
		float		m_FreeCamRelativePitch;
		float		m_FreeCamRelativeRoll;
		float		m_postFxIntensity;
		float		m_saturationIntensity;
		float		m_contrastIntensity;
		float		m_vignetteIntensity;
		float		m_brightness;
		float		m_dofFocalDistance;
		float		m_dofIntensity;
		float		m_VerticalLookAtOffset; 
		float		m_HorizontalLookAtOffset; 
		float		m_LookAtRoll;
		float		m_timeMs;				
		float		m_transitionDurationMs;

		rage::s32	m_AttachTargetId;
		rage::s32	m_AttachTargetIndex;	//ONLY used for UI to display an index associated with the attach target, anyone else should use m_AttachTargetReplayId.
		rage::s32	m_LookAtTargetId;
		rage::s32	m_LookAtTargetIndex;	//ONLY used for UI to display an index associated with the look at target, anyone else should use m_LookAtTargetReplayId.
		rage::s32	m_dofFocusTargetId;
		rage::s32	m_dofFocusTargetIndex;	//ONLY used for UI to display an index associated with the focus target, anyone else should use m_dofFocusTargetId.

		rage::u32	m_speed;	
		rage::u32	m_shakeSpeed;
        rage::u32	m_sfxHash;
        rage::u32   m_sfxCategory;

		rage::u8	m_markerType;
		rage::u8	m_camType;
		rage::u8	m_transitionType;
		rage::u8	m_Fov;
		rage::u8	m_cameraBlendType;
		rage::u8	m_shakeType;
		rage::u8	m_shakeIntensity;
		rage::u8	m_attachType;
		rage::u8	m_activePostFx;
		rage::u8	m_SFXVolume;
		rage::u8	m_MusicVolume;	
        rage::u8	m_DialogVolume;	
        rage::u8	m_AmbientVolume;
        rage::u8	m_oneshotVolume;	
		rage::u8	m_ScoreIntensity;
		rage::u8	m_dofMode;
		rage::u8	m_dofFocusMode;
		rage::u8	m_microphoneType;

		bool m_camMatrixValid;
		bool m_AttachEntityMatrixValid;
		bool m_SpeechOn;
		bool m_cameraEasing;

		sMarkerData_Current()
			: m_editedPropertyFlags( MARKER_EDITED_PROPERTIES_NONE )
			, m_FreeCamQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_AttachEntityQuaternion( DEFAULT_FREECAM_QUATERNION )
			, m_FreeCamRelativeHeading( DEFAULT_FREECAM_HEADING )
			, m_FreeCamRelativePitch( DEFAULT_FREECAM_PITCH )
			, m_FreeCamRelativeRoll( DEFAULT_FREECAM_ROLL )
			, m_postFxIntensity( DEFAULT_POSTFX_INTENSITY )
			, m_saturationIntensity( DEFAULT_MARKER_SATURATION )
			, m_contrastIntensity( DEFAULT_MARKER_CONTRAST )
			, m_vignetteIntensity( DEFAULT_MARKER_VIGNETTE )
			, m_brightness( DEFAULT_MARKER_BRIGHTNESS )
			, m_dofFocalDistance( DEFAULT_MARKER_FOCAL_DISTANCE )
			, m_dofIntensity( DEFAULT_MARKER_DOF_INTENSITY )
			, m_VerticalLookAtOffset(DEFAULT_VERTICAL_LOOKAT_OFFSET)
			, m_HorizontalLookAtOffset(DEFAULT_HORIZONTAL_LOOKAT_OFFSET)
			, m_LookAtRoll(DEFAULT_LOOKAT_ROLL)
			, m_timeMs( DEFAULT_MARKER_TIME_MS )
			, m_transitionDurationMs( DEFAULT_MARKER_TRANSDUR_MS )
			, m_AttachTargetId( DEFAULT_ATTACH_TARGET_ID )
			, m_AttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX )
			, m_LookAtTargetId( DEFAULT_LOOKAT_TARGET_ID )
			, m_LookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX )
			, m_dofFocusTargetId( DEFAULT_DOF_FOCUS_ID )
			, m_dofFocusTargetIndex( DEFAULT_DOF_FOCUS_INDEX )
			, m_speed( DEFAULT_MARKER_SPEED )
            , m_shakeSpeed( DEFAULT_MARKER_SPEED )
            , m_sfxHash( DEFAULT_MARKER_SFX_HASH )
            , m_sfxCategory( DEFAULT_MARKER_SFX_CAT )
			, m_markerType( DEFAULT_MARKER_TYPE ) 
			, m_camType( DEFAULT_MARKER_CAMERA )
			, m_transitionType( DEFAULT_MARKER_TRANS_TYPE )
			, m_Fov( DEFAULT_MARKER_FOV )
			, m_cameraBlendType( DEFAULT_MARKER_CAMBLEND_TYPE )
			, m_shakeType( DEFAULT_MARKER_CAMSHAKE_TYPE )
			, m_shakeIntensity( DEFAULT_MARKER_SHAKE_INTENSITY )
			, m_attachType( DEFAULT_ATTACH_TYPE )
			, m_activePostFx( DEFAULT_MARKER_POST_EFFECT )
			, m_SFXVolume( DEFAULT_MARKER_SFX_VOL )
			, m_MusicVolume( DEFAULT_MARKER_MUSIC_VOL )
			, m_DialogVolume( DEFAULT_MARKER_SPEECH_VOL )
            , m_AmbientVolume( DEFAULT_MARKER_AMB_VOL)
            , m_oneshotVolume( DEFAULT_MARKER_SFX_OS_VOL )
			, m_ScoreIntensity( DEFAULT_MARKER_AUDIO_INTENSITY )
			, m_dofMode( DEFAULT_MARKER_DOF_MODE )
			, m_dofFocusMode( DEFAULT_MARKER_DOF_FOCUS )
			, m_microphoneType( DEFAULT_MARKER_MICROPHONE_TYPE )
			, m_camMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_AttachEntityMatrixValid( DEFAULT_MARKER_CAMMATRIX_VALID )
			, m_SpeechOn( DEFAULT_MARKER_SPEECH_ON )
			, m_cameraEasing( DEFAULT_MARKER_CAMBLEND_EASE )
		{

		}

		//! Helper function to translate pre-version 8 blend data to version 8+ blend data
		void TranslateDepricatedBlendAndSmooting( u8 const oldBlendType, u8 const oldSmoothType )
		{
			m_cameraEasing = oldBlendType == MARKER_DEPRICATED_CAMERA_BLEND_SLOW_IN || oldBlendType == MARKER_DEPRICATED_CAMERA_BLEND_SLOW_OUT || 
				oldBlendType == MARKER_DEPRICATED_CAMERA_BLEND_SLOW_IN_OUT;

			m_cameraBlendType = u8(oldSmoothType == MARKER_DEPRICATED_CAMERA_SMOOTH_ON ? MARKER_CAMERA_BLEND_SMOOTH : 
				oldBlendType == MARKER_DEPRICATED_CAMERA_BLEND_LINEAR ? MARKER_CAMERA_BLEND_LINEAR : 
				m_cameraEasing ? MARKER_CAMERA_BLEND_LINEAR : MARKER_CAMERA_BLEND_NONE );
		}

		//! Helper function to translate pre-version 8 blend data to version 8+ blend data
		void TranslateDepricatedDofTypeAndMode( u8 const oldDofType, u8 const oldFocusMode )
		{
			m_dofMode = (u8)(oldDofType == MARKER_DEPRICATED_DOF_MODE_AUTOMATIC ? MARKER_DOF_MODE_CUSTOM : 
						oldDofType == MARKER_DEPRICATED_DOF_MODE_CUSTOM ? MARKER_DOF_MODE_CUSTOM : 
						oldDofType == MARKER_DEPRICATED_DOF_MODE_NONE ? MARKER_DOF_MODE_NONE : MARKER_DOF_MODE_DEFAULT);

			m_dofFocusMode = (u8)(oldDofType == MARKER_DEPRICATED_DOF_MODE_AUTOMATIC ? MARKER_FOCUS_AUTO : oldFocusMode);
		}

		sMarkerData_Current( sMarkerData_V1 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V1 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V1 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= DEFAULT_FREECAM_QUATERNION;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= DEFAULT_MARKER_BRIGHTNESS * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= (float)rhs.m_timeMs;
			m_transitionDurationMs		= (float)rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= DEFAULT_MARKER_SPEED;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;
			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= u8(rhs.m_AttachTypePositionOnly ? MARKER_CAMERA_ATTACH_POSITION_ONLY : MARKER_CAMERA_ATTACH_FULL);
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
			m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= DEFAULT_MARKER_MICROPHONE_TYPE;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= DEFAULT_MARKER_CAMMATRIX_VALID;
			m_SpeechOn					= rhs.m_SpeechOn;
			
			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, MARKER_DEPRICATED_CAMERA_SMOOTH_OFF );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V2 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V2 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V2 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= DEFAULT_MARKER_BRIGHTNESS * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= (float)rhs.m_timeMs;
			m_transitionDurationMs		= (float)rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= DEFAULT_MARKER_SPEED;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;
			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= u8(rhs.m_AttachTypePositionOnly ? MARKER_CAMERA_ATTACH_POSITION_ONLY : MARKER_CAMERA_ATTACH_FULL);
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= DEFAULT_MARKER_MICROPHONE_TYPE;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;

			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, rhs.m_cameraSmoothLevel );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V3 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V3 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V3 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= DEFAULT_MARKER_BRIGHTNESS * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= (float)rhs.m_timeMs;
			m_transitionDurationMs		= (float)rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= DEFAULT_MARKER_SPEED;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;
			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= u8(rhs.m_AttachTypePositionOnly ? MARKER_CAMERA_ATTACH_POSITION_ONLY : MARKER_CAMERA_ATTACH_FULL);
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;

			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, rhs.m_cameraSmoothLevel );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V4 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V4 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V4 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= rhs.m_brightness * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= (float)rhs.m_timeMs;
			m_transitionDurationMs		= (float)rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= DEFAULT_MARKER_SPEED;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;
			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= u8(rhs.m_AttachTypePositionOnly ? MARKER_CAMERA_ATTACH_POSITION_ONLY : MARKER_CAMERA_ATTACH_FULL);
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;
			
			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, rhs.m_cameraSmoothLevel );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V5 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V5 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V5 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= rhs.m_brightness * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= (float)rhs.m_timeMs;
			m_transitionDurationMs		= (float)rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;
			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= u8(rhs.m_AttachTypePositionOnly ? MARKER_CAMERA_ATTACH_POSITION_ONLY : MARKER_CAMERA_ATTACH_FULL);
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;

			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, rhs.m_cameraSmoothLevel );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V6 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V6 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V6 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= rhs.m_brightness * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= (float)rhs.m_timeMs;
			m_transitionDurationMs		= (float)rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;
			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= rhs.m_attachType;
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;

			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, rhs.m_cameraSmoothLevel );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V7 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V7 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V7 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= rhs.m_brightness * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_timeMs					= rhs.m_timeMs;
			m_transitionDurationMs		= rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
            m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			u8 actualMarkerType = MARKER_TYPE_EDIT;
			if( rhs.m_markerType == 1 )
			{
				actualMarkerType = 2;
			}
			else if( rhs.m_markerType == 2 )
			{
				actualMarkerType = 1;
			}

			m_markerType				= actualMarkerType;

			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= rhs.m_attachType;
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;

			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;

			TranslateDepricatedBlendAndSmooting( rhs.m_CameraBlendType, rhs.m_cameraSmoothLevel );
			TranslateDepricatedDofTypeAndMode( rhs.m_dofMode, rhs.m_dofFocusMode );
		}

		sMarkerData_Current( sMarkerData_V8 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V8 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V8 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= rhs.m_brightness * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_LookAtRoll				= DEFAULT_LOOKAT_ROLL;
			m_timeMs					= rhs.m_timeMs;
			m_transitionDurationMs		= rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
			m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			m_markerType				= rhs.m_markerType;

			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_cameraBlendType			= rhs.m_cameraBlendType;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= rhs.m_attachType;
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;
			m_dofMode					= rhs.m_dofMode;
			m_dofFocusMode				= rhs.m_dofFocusMode;
			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;
			m_cameraEasing				= rhs.m_cameraEasing;
		}

		sMarkerData_Current( sMarkerData_V9 const& other )
		{
			CopyData( other );
		}

		sMarkerData_Current& operator=( sMarkerData_V9 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( sMarkerData_V9 const& rhs )
		{
			m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
			m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
			m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

			m_AttachEntityPosition[0]	= 0.0f;
			m_AttachEntityPosition[1]	= 0.0f;
			m_AttachEntityPosition[2]	= 0.0f;
			sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
			sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_FreeCamPositionRelativeToAttachEntity) );

			m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
			m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
			m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
			m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
			m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
			m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
			m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
			m_brightness				= rhs.m_brightness * 100.f;
			m_dofFocalDistance			= rhs.m_dofFocalDistance;
			m_dofIntensity				= rhs.m_dofIntensity * 100.f;
			m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
			m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
			m_LookAtRoll				= rhs.m_LookAtRoll;
			m_timeMs					= rhs.m_timeMs;
			m_transitionDurationMs		= rhs.m_transitionDurationMs;

			m_AttachTargetId			= rhs.m_AttachTargetId;
			m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
			m_LookAtTargetId			= rhs.m_LookAtTargetId;
			m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
			m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
			m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

			m_speed						= rhs.m_speed;
			m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= DEFAULT_MARKER_SFX_HASH;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

			m_markerType				= rhs.m_markerType;

			m_camType					= rhs.m_camType;
			m_transitionType			= rhs.m_transitionType;
			m_Fov						= rhs.m_Fov;
			m_cameraBlendType			= rhs.m_cameraBlendType;
			m_shakeType					= rhs.m_shakeType;
			m_shakeIntensity			= rhs.m_shakeIntensity;
			m_attachType				= rhs.m_attachType;
			m_activePostFx				= rhs.m_activePostFx;
			m_SFXVolume					= rhs.m_SFXVolume;
			m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
			m_ScoreIntensity			= rhs.m_ScoreIntensity;
			m_dofMode					= rhs.m_dofMode;
			m_dofFocusMode				= rhs.m_dofFocusMode;
			m_microphoneType			= rhs.m_microphoneType;

			m_camMatrixValid			= rhs.m_camMatrixValid;
			m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
			m_SpeechOn					= rhs.m_SpeechOn;
			m_cameraEasing				= rhs.m_cameraEasing;
		}

        sMarkerData_Current( sMarkerData_V10 const& other )
        {
            CopyData( other );
        }

        sMarkerData_Current& operator=( sMarkerData_V10 const& rhs )
        {			
            CopyData( rhs );
            return *this;
        }

        void CopyData( sMarkerData_V10 const& rhs )
        {
            m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
            m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
            m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

            m_AttachEntityPosition[0]	= 0.0f;
            m_AttachEntityPosition[1]	= 0.0f;
            m_AttachEntityPosition[2]	= 0.0f;
            sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
            sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
                sizeof(m_FreeCamPositionRelativeToAttachEntity) );

            m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
            m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
            m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
            m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
            m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
            m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
            m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
            m_brightness				= rhs.m_brightness * 100.f;
            m_dofFocalDistance			= rhs.m_dofFocalDistance;
            m_dofIntensity				= rhs.m_dofIntensity * 100.f;
            m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
            m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
            m_LookAtRoll				= rhs.m_LookAtRoll;
            m_timeMs					= rhs.m_timeMs;
            m_transitionDurationMs		= rhs.m_transitionDurationMs;

            m_AttachTargetId			= rhs.m_AttachTargetId;
            m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
            m_LookAtTargetId			= rhs.m_LookAtTargetId;
            m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
            m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
            m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

            m_speed						= rhs.m_speed;
            m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= rhs.m_SFXHash;
            m_sfxCategory               = DEFAULT_MARKER_SFX_CAT;

            m_markerType				= rhs.m_markerType;

            m_camType					= rhs.m_camType;
            m_transitionType			= rhs.m_transitionType;
            m_Fov						= rhs.m_Fov;
            m_cameraBlendType			= rhs.m_cameraBlendType;
            m_shakeType					= rhs.m_shakeType;
            m_shakeIntensity			= rhs.m_shakeIntensity;
            m_attachType				= rhs.m_attachType;
            m_activePostFx				= rhs.m_activePostFx;
            m_SFXVolume					= rhs.m_SFXVolume;
            m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
            m_ScoreIntensity			= rhs.m_ScoreIntensity;
            m_dofMode					= rhs.m_dofMode;
            m_dofFocusMode				= rhs.m_dofFocusMode;
            m_microphoneType			= rhs.m_microphoneType;

            m_camMatrixValid			= rhs.m_camMatrixValid;
            m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
            m_SpeechOn					= rhs.m_SpeechOn;
            m_cameraEasing				= rhs.m_cameraEasing;
        }

        sMarkerData_Current( sMarkerData_V11 const& other )
        {
            CopyData( other );
        }

        sMarkerData_Current& operator=( sMarkerData_V11 const& rhs )
        {			
            CopyData( rhs );
            return *this;
        }

        void CopyData( sMarkerData_V11 const& rhs )
        {
            m_editedPropertyFlags		= rhs.m_editedPropertyFlags;
            m_FreeCamQuaternion			= rhs.m_FreeCamQuaternion;
            m_AttachEntityQuaternion	= rhs.m_AttachEntityQuaternion;

            m_AttachEntityPosition[0]	= 0.0f;
            m_AttachEntityPosition[1]	= 0.0f;
            m_AttachEntityPosition[2]	= 0.0f;
            sysMemCpy( m_FreeCamPosition, rhs.m_FreeCamPosition, sizeof(m_FreeCamPosition) );
            sysMemCpy( m_FreeCamPositionRelativeToAttachEntity, rhs.m_FreeCamPositionRelativeToAttachEntity, 
                sizeof(m_FreeCamPositionRelativeToAttachEntity) );

            m_FreeCamRelativeHeading	= rhs.m_FreeCamRelativeHeading;
            m_FreeCamRelativePitch		= rhs.m_FreeCamRelativePitch;
            m_FreeCamRelativeRoll		= rhs.m_FreeCamRelativeRoll;									  
            m_postFxIntensity			= rhs.m_postFxIntensity * 100.f;
            m_saturationIntensity		= rhs.m_saturationIntensity * 100.f;
            m_contrastIntensity			= rhs.m_contrastIntensity * 100.f;
            m_vignetteIntensity			= rhs.m_vignetteIntensity * 100.f;
            m_brightness				= rhs.m_brightness * 100.f;
            m_dofFocalDistance			= rhs.m_dofFocalDistance;
            m_dofIntensity				= rhs.m_dofIntensity * 100.f;
            m_VerticalLookAtOffset		= rhs.m_VerticalLookAtOffset;
            m_HorizontalLookAtOffset	= rhs.m_HorizontalLookAtOffset;
            m_LookAtRoll				= rhs.m_LookAtRoll;
            m_timeMs					= rhs.m_timeMs;
            m_transitionDurationMs		= rhs.m_transitionDurationMs;

            m_AttachTargetId			= rhs.m_AttachTargetId;
            m_AttachTargetIndex			= rhs.m_AttachTargetIndex;
            m_LookAtTargetId			= rhs.m_LookAtTargetId;
            m_LookAtTargetIndex			= rhs.m_LookAtTargetIndex;
            m_dofFocusTargetId			= rhs.m_dofFocusTargetId;
            m_dofFocusTargetIndex		= rhs.m_dofFocusTargetIndex;

            m_speed						= rhs.m_speed;
            m_shakeSpeed				= rhs.m_shakeSpeed;
            m_sfxHash					= rhs.m_sfxHash;
            m_sfxCategory               = rhs.m_sfxCategory;

            m_markerType				= rhs.m_markerType;

            m_camType					= rhs.m_camType;
            m_transitionType			= rhs.m_transitionType;
            m_Fov						= rhs.m_Fov;
            m_cameraBlendType			= rhs.m_cameraBlendType;
            m_shakeType					= rhs.m_shakeType;
            m_shakeIntensity			= rhs.m_shakeIntensity;
            m_attachType				= rhs.m_attachType;
            m_activePostFx				= rhs.m_activePostFx;
            m_SFXVolume					= rhs.m_SFXVolume;
            m_MusicVolume				= rhs.m_MusicVolume;
            m_DialogVolume				= rhs.m_DialogVolume;
            m_AmbientVolume             = DEFAULT_MARKER_AMB_VOL;
            m_oneshotVolume             = DEFAULT_MARKER_SFX_OS_VOL;
            m_ScoreIntensity			= rhs.m_ScoreIntensity;
            m_dofMode					= rhs.m_dofMode;
            m_dofFocusMode				= rhs.m_dofFocusMode;
            m_microphoneType			= rhs.m_microphoneType;

            m_camMatrixValid			= rhs.m_camMatrixValid;
            m_AttachEntityMatrixValid	= rhs.m_AttachEntityMatrixValid;
            m_SpeechOn					= rhs.m_SpeechOn;
            m_cameraEasing				= rhs.m_cameraEasing;
        }
	};

	//-------------------------------------------------------------------------------------------------
	// format: 1 based incrementing
	// NOTE: When you update the version, be sure to update the marker loading in Clip.cpp to support your changes
	static const u32 REPLAY_MARKER_VERSION = 12;

public: // methods

	sReplayMarkerInfo()
		: m_data()
	{
		ResetEditedProperties();
	}

	sReplayMarkerInfo( sMarkerData_Current const& markerData )
		: m_data( markerData )
	{

	}

	sReplayMarkerInfo( sReplayMarkerInfo const& other )
	{
		CopyAll( other );
	}

	sReplayMarkerInfo& operator=( sReplayMarkerInfo const& rhs)
	{
		if( &rhs != this )
		{
			CopyAll( rhs );
		}

		return *this;
	}

    void CopyData( sReplayMarkerInfo const& other, eMarkerCopyContext const copyContext )
    {
        switch( copyContext )
        {
        case COPY_CONTEXT_EFFECTS_ONLY:
            {
                CopySubset_Effects( other );
                break;
            }

        case COPY_CONTEXT_AUDIO_ONLY:
            {
                CopySubset_Audio( other );
                break;
            }

        case COPY_CONTEXT_ALL:
            {
                CopyAll( other );
                break;
            }

        case COPY_CONTEXT_NONE:
        default:
            {
                // NOP
                break;
            }
        }
    }

	void CopyAll( sReplayMarkerInfo const& other )
	{
		m_data = other.m_data;
	}

    void CopySubset_Effects( sReplayMarkerInfo const& other )
    {
        m_data.m_postFxIntensity		= other.m_data.m_postFxIntensity;
        m_data.m_saturationIntensity	= other.m_data.m_saturationIntensity;
        m_data.m_contrastIntensity		= other.m_data.m_contrastIntensity;
        m_data.m_vignetteIntensity		= other.m_data.m_vignetteIntensity;
        m_data.m_brightness				= other.m_data.m_brightness;

        m_data.m_activePostFx		    = other.m_data.m_activePostFx;
    }

    void CopySubset_Audio( sReplayMarkerInfo const& other )
    {
        m_data.m_SFXVolume			= other.m_data.m_SFXVolume;
        m_data.m_MusicVolume		= other.m_data.m_MusicVolume;
        m_data.m_DialogVolume		= other.m_data.m_DialogVolume;
        m_data.m_AmbientVolume		= other.m_data.m_AmbientVolume;
        m_data.m_oneshotVolume		= other.m_data.m_oneshotVolume;
        m_data.m_ScoreIntensity		= other.m_data.m_ScoreIntensity;
        m_data.m_microphoneType		= other.m_data.m_microphoneType;
        m_data.m_sfxCategory        = other.m_data.m_sfxCategory;
        m_data.m_sfxHash            = other.m_data.m_sfxHash;

        m_data.m_SpeechOn           = other.m_data.m_SpeechOn;
    }

	//! Copies properties from given marker that we want to inherit
	void InterhitProperties( sReplayMarkerInfo const& other, sReplayMarkerInfo const* pNextMarker )
	{
		// Not currently used, but maintaining interface incase it's needed again for later validation
		(void)pNextMarker;

		if( other.IsEditableCamera() && !other.IsAnchor() )
		{
			m_data.m_FreeCamQuaternion = other.m_data.m_FreeCamQuaternion;
			m_data.m_AttachEntityQuaternion = other.m_data.m_AttachEntityQuaternion;

			sysMemCpy( m_data.m_AttachEntityPosition, other.m_data.m_AttachEntityPosition, sizeof(m_data.m_AttachEntityPosition) );
			sysMemCpy( m_data.m_FreeCamPosition, other.m_data.m_FreeCamPosition, sizeof(m_data.m_FreeCamPosition) );
			sysMemCpy( m_data.m_FreeCamPositionRelativeToAttachEntity, other.m_data.m_FreeCamPositionRelativeToAttachEntity, 
				sizeof(m_data.m_FreeCamPositionRelativeToAttachEntity) );
			
			m_data.m_FreeCamRelativeHeading = other.m_data.m_FreeCamRelativeHeading;
			m_data.m_FreeCamRelativePitch	= other.m_data.m_FreeCamRelativePitch;
			m_data.m_FreeCamRelativeRoll	= other.m_data.m_FreeCamRelativeRoll;

			m_data.m_AttachTargetId			= other.m_data.m_AttachTargetId;
			m_data.m_AttachTargetIndex		= other.m_data.m_AttachTargetIndex;
			m_data.m_LookAtTargetId			= other.m_data.m_LookAtTargetId;
			m_data.m_LookAtTargetIndex		= other.m_data.m_LookAtTargetIndex;

			m_data.m_VerticalLookAtOffset	= other.m_data.m_VerticalLookAtOffset;
			m_data.m_HorizontalLookAtOffset	= other.m_data.m_HorizontalLookAtOffset;
			m_data.m_LookAtRoll				= other.m_data.m_LookAtRoll;

			m_data.m_attachType				= other.m_data.m_attachType;

			m_data.m_Fov = other.m_data.m_Fov;

			//if we're cloning a free camera, we mark the camera matrix as invalid so we force the default frame, which is
			//a clone of what's rendered, rather than the marker's stored position/orientation used at the start of the marker
			if( other.IsFreeCamera() )
			{
				m_data.m_camMatrixValid = false;
				m_data.m_AttachEntityMatrixValid = false;
			}
			else
			{
				m_data.m_camMatrixValid = other.m_data.m_camMatrixValid;
				m_data.m_AttachEntityMatrixValid = other.m_data.m_AttachEntityMatrixValid;
			}
		}

		m_data.m_dofFocalDistance		= other.m_data.m_dofFocalDistance;
		m_data.m_dofIntensity			= other.m_data.m_dofIntensity;
		m_data.m_dofFocusTargetId		= other.m_data.m_dofFocusTargetId;
		m_data.m_dofFocusTargetIndex	= other.m_data.m_dofFocusTargetIndex;
		m_data.m_dofMode				= other.m_data.m_dofMode;
		m_data.m_dofFocusMode			= other.m_data.m_dofFocusMode;

		m_data.m_postFxIntensity		= other.m_data.m_postFxIntensity;
		m_data.m_saturationIntensity	= other.m_data.m_saturationIntensity;
		m_data.m_contrastIntensity		= other.m_data.m_contrastIntensity;
		m_data.m_vignetteIntensity		= other.m_data.m_vignetteIntensity;
		m_data.m_brightness				= other.m_data.m_brightness;

		m_data.m_speed					= other.m_data.m_speed;
		m_data.m_shakeSpeed				= other.m_data.m_shakeSpeed;

		m_data.m_markerType				= other.m_data.m_markerType;
		m_data.m_camType				= other.m_data.m_camType;
		m_data.m_transitionType			= other.m_data.m_transitionType;

		// only copy the blend behavior if the other marker camera is editable
		if ( other.IsFreeCamera() )
		{
			m_data.m_cameraBlendType	= other.m_data.m_cameraBlendType;
			m_data.m_cameraEasing	= other.m_data.m_cameraEasing;
		}
		else
		{
			m_data.m_cameraBlendType	= MARKER_CAMERA_BLEND_NONE;
			m_data.m_cameraEasing		= true; // Enable easing by default
		}

		m_data.m_shakeType			= other.m_data.m_shakeType;
		m_data.m_shakeIntensity		= other.m_data.m_shakeIntensity;
		m_data.m_activePostFx		= other.m_data.m_activePostFx;
		m_data.m_SFXVolume			= other.m_data.m_SFXVolume;
		m_data.m_MusicVolume		= other.m_data.m_MusicVolume;
        m_data.m_DialogVolume		= other.m_data.m_DialogVolume;
        m_data.m_AmbientVolume		= other.m_data.m_AmbientVolume;
        m_data.m_oneshotVolume		= other.m_data.m_oneshotVolume;
		m_data.m_ScoreIntensity		= other.m_data.m_ScoreIntensity;
        m_data.m_microphoneType		= other.m_data.m_microphoneType;
        m_data.m_sfxCategory		= other.m_data.m_sfxCategory;
        m_data.m_sfxHash            = DEFAULT_MARKER_SFX_HASH;

		m_data.m_SpeechOn = other.m_data.m_SpeechOn;

		// Reset edit state on inheritance, since we are now defaulting to an inherited state
		ResetEditedProperties();
	}

	inline rage::u64 GetFreeCamQuaternion() const { return m_data.m_FreeCamQuaternion; }
	inline void SetFreeCamQuaternion( rage::u64 const freeCamQuaternion ) { m_data.m_FreeCamQuaternion = freeCamQuaternion; }

	inline rage::u64 GetAttachEntityQuaternion() const { return m_data.m_AttachEntityQuaternion; }
	inline void SetAttachEntityQuaternion( rage::u64 const quaternion ) { m_data.m_AttachEntityQuaternion = quaternion; }

	inline float (&GetAttachEntityPosition())[3] { return m_data.m_AttachEntityPosition; }
	inline float const (&GetAttachEntityPosition() const)[3] { return m_data.m_AttachEntityPosition; }

	inline float (&GetFreeCamPosition())[3] { return m_data.m_FreeCamPosition; }
	inline float const (&GetFreeCamPosition() const)[3] { return m_data.m_FreeCamPosition; }

	inline float (&GetFreeCamPositionRelative())[3] { return m_data.m_FreeCamPositionRelativeToAttachEntity; }
	inline float const (&GetFreeCamPositionRelative() const)[3] { return m_data.m_FreeCamPositionRelativeToAttachEntity; }

	inline float GetFreeCamRelativeHeading() const { return m_data.m_FreeCamRelativeHeading; }
	inline void SetFreeCamRelativeHeading( float const relativeHeading ) { m_data.m_FreeCamRelativeHeading = relativeHeading; }

	inline float GetFreeCamRelativePitch() const { return m_data.m_FreeCamRelativePitch; }
	inline void SetFreeCamRelativePitch( float const relativePitch ) { m_data.m_FreeCamRelativePitch = relativePitch; }

	inline float GetFreeCamRelativeRoll() const { return m_data.m_FreeCamRelativeRoll; }
	inline void SetFreeCamRelativeRoll( float const relativeRoll ) { m_data.m_FreeCamRelativeRoll = relativeRoll; }

	inline rage::u8 GetFOV() const { return m_data.m_Fov; }
	inline void SetFOV( rage::u8 const fov ) { m_data.m_Fov = fov; }

	bool UpdateTransitionType( rage::s32 const delta, ReplayMarkerTransitionType const transitionToSkip )
	{
		rage::u8 const c_startingType = m_data.m_transitionType;

		rage::s32 const c_nextType = m_data.m_transitionType + delta;
		m_data.m_transitionType = ( c_nextType >= MARKER_TRANSITIONTYPE_MAX ) ? (u8)MARKER_TRANSITIONTYPE_FIRST :
			c_nextType < MARKER_TRANSITIONTYPE_FIRST ? (u8)(MARKER_TRANSITIONTYPE_MAX - 1) : (u8)c_nextType;

		//! Recurse for another step if we want to skip a transition type
		if( m_data.m_transitionType == transitionToSkip )
			UpdateTransitionType( delta, MARKER_TRANSITIONTYPE_MAX );

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_TRANS_TYPE );

		return c_startingType != m_data.m_transitionType;
	}

	static char const * GetTransitionTypeLngKey( ReplayMarkerTransitionType const transitionType )
	{
		static char const * const s_markerTransitionTypeStrings[ MARKER_TRANSITIONTYPE_MAX ] =
		{
			"VEUI_TRANS_NONE", "VEUI_FADEIN", "VEUI_FADEOUT"
		};

		char const * const lngKey = transitionType >= MARKER_TRANSITIONTYPE_FIRST && transitionType < MARKER_TRANSITIONTYPE_MAX ? 
			s_markerTransitionTypeStrings[ transitionType ] : "";

		return lngKey;
	}

	inline char const * GetTransitionTypeLngKey() const
	{
		return sReplayMarkerInfo::GetTransitionTypeLngKey( (ReplayMarkerTransitionType)m_data.m_transitionType );
	}

	static char const * GetTransitionTypeHelpLngKey( ReplayMarkerTransitionType const transitionType )
	{
		(void)transitionType;
		return "VEUI_ED_TNS_TYPE_HLP";
	}

	inline char const * GetTransitionTypeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetTransitionTypeHelpLngKey( (ReplayMarkerTransitionType)m_data.m_transitionType );
	}

	inline rage::u8 GetTransitionType() const { return m_data.m_transitionType; }
	inline void SetTransitionType( rage::u8 const transitionType ) 
	{ 
		m_data.m_transitionType = transitionType;
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_TRANS_TYPE );
	}

	inline bool IsAnchor() const
	{
		return m_data.m_markerType == MARKER_TYPE_ANCHOR;
	}

	inline bool IsAnchorOrEditableAnchor() const
	{
		return m_data.m_markerType == MARKER_TYPE_ANCHOR || m_data.m_markerType == MARKER_TYPE_EDIT_AND_ANCHOR;
	}

	inline bool IsEditable() const
	{
		return m_data.m_markerType == MARKER_TYPE_EDIT || m_data.m_markerType == MARKER_TYPE_EDIT_AND_ANCHOR ||
			m_data.m_markerType == MARKER_TYPE_EDIT_AND_TRANSITION;
	}

	static char const * GetMarkerTypeLngKey( ReplayMarkerType const markerType )
	{
		static char const * const s_markerTypeStrings[ MARKER_TYPE_MAX ] =
		{
			"MO_OFF", "MO_ON", "VEUI_MK_EXLC", "VEUI_MK_TRANS"
		};

		char const * const lngKey = markerType >= MARKER_TYPE_FIRST && markerType < MARKER_TYPE_MAX ? 
			s_markerTypeStrings[ markerType ] : "";

		return lngKey;
	}

	inline char const * GetMarkerTypeLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerTypeLngKey( (ReplayMarkerType)m_data.m_markerType );
	}

	static char const * GetMarkerTypeHelpLngKey( ReplayMarkerType const markerType )
	{
		static char const * const s_markerTypeHelpStrings[ MARKER_TYPE_MAX ] =
		{
			"VEUI_MK_TYPE_BA_HLP", "VEUI_MK_TYPE_BT_HLP", "VEUI_MK_TYPE_AN_HLP", ""
		};

		char const * const lngKey = markerType >= MARKER_TYPE_FIRST && markerType < MARKER_TYPE_MAX ? 
			s_markerTypeHelpStrings[ markerType ] : "";

		return lngKey;
	}

	inline char const * GetMarkerTypeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerTypeHelpLngKey( (ReplayMarkerType)m_data.m_markerType );
	}

	inline rage::u8 GetMarkerType() const { return m_data.m_markerType; }

	bool UpdateMarkerType( rage::s32 const delta )
	{
		rage::u8 const c_startingType = m_data.m_markerType;

		rage::s32 const c_nextType = m_data.m_markerType + delta;

		m_data.m_markerType = ( c_nextType >= MARKER_TYPE_TOGGLABLE_MAX ) ? (u8)MARKER_TYPE_FIRST :
			c_nextType < MARKER_TYPE_FIRST ? (u8)(MARKER_TYPE_TOGGLABLE_MAX - 1) : (u8)c_nextType;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_TYPE );

		return m_data.m_markerType != c_startingType;
	}

	bool UpdateMarkerSpeed( rage::s32 const delta )
	{
		rage::u32 const c_startingSpeed = m_data.m_speed;

		rage::s32 const c_nextSpeed = m_data.m_speed + delta;

		m_data.m_speed = ( c_nextSpeed >= MARKER_SPEED_MAX ) ? (u32)MARKER_SPEED_FIRST :
			c_nextSpeed < MARKER_SPEED_FIRST ? (u32)(MARKER_SPEED_MAX - 1) : (u32)c_nextSpeed;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SPEED );

		return m_data.m_speed != c_startingSpeed;
	}

	ReplayMarkerSpeed GetSpeedValue() const
	{
		return (ReplayMarkerSpeed)m_data.m_speed;
	}

	rage::u32 GetSpeedValueU32() const
	{
		switch( m_data.m_speed )
		{
		case MARKER_SPEED_5:
			return 5;
		case MARKER_SPEED_20:
			return 20;
		case MARKER_SPEED_35:
			return 35;
		case MARKER_SPEED_50:
			return 50;
		case MARKER_SPEED_125:
			return 125;
		case MARKER_SPEED_150:
			return 150;
		case MARKER_SPEED_175:
			return 175;
		case MARKER_SPEED_200:
			return 200;
		case MARKER_SPEED_100:
		default:
			return 100;
		}
	}

	float GetSpeedValueFloat() const
	{
		switch( m_data.m_speed )
		{
		case MARKER_SPEED_5:
			return 0.05f;
		case MARKER_SPEED_20:
			return 0.2f;
		case MARKER_SPEED_35:
			return 0.35f;
		case MARKER_SPEED_50:
			return 0.5f;
		case MARKER_SPEED_125:
			return 1.25f;
		case MARKER_SPEED_150:
			return 1.5f;
		case MARKER_SPEED_175:
			return 1.75f;
		case MARKER_SPEED_200:
			return 2.f;
		case MARKER_SPEED_100:
		default:
			return 1.f;
		}
	}

	float GetFrameBoundaryForThisMarker() const
	{
		return MARKER_FRAME_BOUNDARY_MS * GetSpeedValueFloat();
	}

	static char const * GetMarkerSpeedLngKey( ReplayMarkerSpeed const markerSpeed )
	{
		static char const * const s_markerSpeedStrings[ MARKER_SPEED_MAX ] =
		{
			"VEUI_SPEED_5", "VEUI_SPEED_20", "VEUI_SPEED_35", "VEUI_SPEED_50", "VEUI_SPEED_100",
			"VEUI_SPEED_125", "VEUI_SPEED_150", "VEUI_SPEED_175", "VEUI_SPEED_200"
		};

		char const * const lngKey = markerSpeed >= MARKER_SPEED_FIRST && markerSpeed < MARKER_SPEED_MAX ? 
			s_markerSpeedStrings[ markerSpeed ] : "";

		return lngKey;
	}

	inline char const * GetMarkerSpeedLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerSpeedLngKey( (ReplayMarkerSpeed)m_data.m_speed );
	}

	bool UpdateMarkerShakeSpeed( rage::s32 const delta )
	{
		rage::u32 const c_startingSpeed = m_data.m_shakeSpeed;

		rage::s32 const c_nextSpeed = m_data.m_shakeSpeed + delta;

		m_data.m_shakeSpeed = ( c_nextSpeed >= MARKER_SPEED_MAX ) ? (u32)MARKER_SPEED_FIRST :
			c_nextSpeed < MARKER_SPEED_FIRST ? (u32)(MARKER_SPEED_MAX - 1) : (u32)c_nextSpeed;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_SHAKE_SPEED );

		return m_data.m_shakeSpeed != c_startingSpeed;
	}

	rage::u32 GetShakeSpeedValue() const
	{
		switch( m_data.m_shakeSpeed )
		{
		case MARKER_SPEED_5:
			return 5;
		case MARKER_SPEED_20:
			return 20;
		case MARKER_SPEED_35:
			return 35;
		case MARKER_SPEED_50:
			return 50;
		case MARKER_SPEED_125:
			return 125;
		case MARKER_SPEED_150:
			return 150;
		case MARKER_SPEED_175:
			return 175;
		case MARKER_SPEED_200:
			return 200;
		case MARKER_SPEED_100:
		default:
			return 100;
		}
	}

	float GetShakeSpeedValueFloat() const
	{
		switch( m_data.m_shakeSpeed )
		{
		case MARKER_SPEED_5:
			return 0.05f;
		case MARKER_SPEED_20:
			return 0.2f;
		case MARKER_SPEED_35:
			return 0.35f;
		case MARKER_SPEED_50:
			return 0.5f;
		case MARKER_SPEED_125:
			return 1.25f;
		case MARKER_SPEED_150:
			return 1.5f;
		case MARKER_SPEED_175:
			return 1.75f;
		case MARKER_SPEED_200:
			return 2.f;
		case MARKER_SPEED_100:
		default:
			return 1.f;
		}
	}

	static char const * GetMarkerShakeSpeedLngKey( ReplayMarkerSpeed const markerSpeed )
	{
		return GetMarkerSpeedLngKey( markerSpeed );
	}

	inline char const * GetMarkerShakeSpeedLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerShakeSpeedLngKey( (ReplayMarkerSpeed)m_data.m_shakeSpeed );
	}

	static char const * GetMarkerBlendLngKey( eMarkerCameraBlendType const blendType )
	{
		static char const * const s_markerBlendStrings[ MARKER_CAMERA_BLEND_MAX ] =
		{
			"VEUI_BLEND_NONE", "VEUI_BLEND_LINEAR", "VEUI_BLEND_SMOOTH"
		};

		char const * const lngKey = blendType >= MARKER_CAMERA_BLEND_FIRST && blendType < MARKER_CAMERA_BLEND_MAX ? 
			s_markerBlendStrings[ blendType ] : "";

		return lngKey;
	}

	inline char const * GetMarkerBlendLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerBlendLngKey( (eMarkerCameraBlendType)m_data.m_cameraBlendType );
	}

	bool UpdateBlendType( rage::s32 const delta )
	{
		rage::u8 const c_startingBlend = m_data.m_cameraBlendType;
		rage::s32 const c_nextBlend = m_data.m_cameraBlendType + delta;

		m_data.m_cameraBlendType = ( c_nextBlend >= MARKER_CAMERA_BLEND_MAX ) ? (u8)MARKER_CAMERA_BLEND_FIRST :
			c_nextBlend < MARKER_CAMERA_BLEND_FIRST ? (u8)(MARKER_CAMERA_BLEND_MAX - 1) : (u8)c_nextBlend;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_BLEND );

		return m_data.m_cameraBlendType != c_startingBlend;
	}

	inline bool HasBlend() const { return GetCameraType() == RPLYCAM_FREE_CAMERA && GetBlendType() != MARKER_CAMERA_BLEND_NONE; }
	inline bool HasSmoothBlend() const { return GetCameraType() == RPLYCAM_FREE_CAMERA && GetBlendType() == MARKER_CAMERA_BLEND_SMOOTH; }
	inline u8 GetBlendType() const { return m_data.m_cameraBlendType; }

	inline char const * GetMarkerBlendEaseLngKey() const
	{
		return HasBlendEase() ? "MO_ON" : "MO_OFF";
	}

	bool UpdateBlendEaseType( rage::s32 const delta )
	{
		(void)delta;
		m_data.m_cameraEasing = !m_data.m_cameraEasing;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_BLEND_EASING );

		return true;
	}

	inline bool HasBlendEase() const { return GetCameraType() == RPLYCAM_FREE_CAMERA && m_data.m_cameraEasing; }

	static char const * GetMarkerShakeLngKey( eMarkerShakeType const shakeType )
	{
		static char const * const s_markerShakeStrings[ MARKER_SHAKE_MAX ] =
		{
			"VEUI_SHAKE_NONE", "VEUI_SHAKE_HAND", "VEUI_SHAKE_DRUNK", "VEUI_SHAKE_GROUND", "VEUI_SHAKE_AIR", "VEUI_SHAKE_EXPLOSION"
		};

		char const * const lngKey = shakeType >= MARKER_SHAKE_FIRST && shakeType < MARKER_SHAKE_MAX ? 
			s_markerShakeStrings[ shakeType ] : "";

		return lngKey;
	}

	inline char const * GetMarkerShakeLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerShakeLngKey( (eMarkerShakeType)m_data.m_shakeType );
	}

	bool UpdateShakeType( rage::s32 const delta )
	{
		rage::u8 const c_startShake = m_data.m_shakeType;
		rage::s32 const c_nextShake = m_data.m_shakeType + delta;

		m_data.m_shakeType = ( c_nextShake >= MARKER_SHAKE_MAX ) ? (u8)MARKER_SHAKE_FIRST :
			c_nextShake < MARKER_SHAKE_FIRST ? (u8)(MARKER_SHAKE_MAX - 1) : (u8)c_nextShake;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_SHAKE_TYPE );

		return m_data.m_shakeType != c_startShake;
	}

	inline bool HasShakeApplied() const { return GetShakeType() != MARKER_SHAKE_NONE; }
	inline u8 GetShakeType() const { return m_data.m_shakeType; }

	bool UpdateShakeIntensity( rage::s32 const delta )
	{
		rage::u8 const c_startingShake = m_data.m_shakeIntensity;
		rage::s32 const c_newShake = m_data.m_shakeIntensity + delta;
		m_data.m_shakeIntensity = (u8)rage::Clamp<rage::s32>( c_newShake, MARKER_MIN_SHAKE, MARKER_MAX_SHAKE );

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_SHAKE_INTENSITY );

		return m_data.m_shakeIntensity != c_startingShake;
	}

	inline u8 GetShakeIntensity() const { return m_data.m_shakeIntensity; }

	static char const * GetDofModeLngKey( eReplayDofMode const dofMode )
	{
		static char const * const s_dofModeStrings[ MARKER_DOF_MODE_MAX ] =
		{
			"VEUI_DOF_DEFAULT", "VEUI_DOF_CUSTOM", "VEUI_DOF_NONE"
		};

		char const * const lngKey = dofMode >= MARKER_DOF_MODE_FIRST && dofMode < MARKER_DOF_MODE_MAX ? 
			s_dofModeStrings[ dofMode ] : "";

		return lngKey;
	}

	static char const * GetDofModeHelpLngKey( eReplayDofMode const dofMode )
	{
		static char const * const s_dofModeHelpStrings[ MARKER_DOF_MODE_MAX ] =
		{
			"VEUI_DOF_DEFAULTH", "VEUI_DOF_CUSTOMH", "VEUI_DOF_NONEH"
		};

		char const * const lngKey = dofMode >= MARKER_DOF_MODE_FIRST && dofMode < MARKER_DOF_MODE_MAX ? 
			s_dofModeHelpStrings[ dofMode ] : "";

		return lngKey;
	}

	inline char const * GetDofModeLngKey() const
	{
		return sReplayMarkerInfo::GetDofModeLngKey( (eReplayDofMode)GetDofMode() );
	}

	inline char const * GetDofModeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetDofModeHelpLngKey( (eReplayDofMode)GetDofMode() );
	}

	bool UpdateDofMode( rage::s32 const delta )
	{
		rage::u8 const c_startingMode = GetDofMode();
		rage::s32 const c_nextMode = GetDofMode() + delta;

		m_data.m_dofMode = ( c_nextMode >= MARKER_DOF_MODE_MAX ) ? (u8)MARKER_DOF_MODE_FIRST :
			c_nextMode < MARKER_DOF_MODE_FIRST ? (u8)(MARKER_DOF_MODE_MAX - 1) : (u8)c_nextMode;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_DOF_MODE );

		return GetDofMode() != c_startingMode;
	}

	inline bool IsDofDefault() const { return GetDofMode() == MARKER_DOF_MODE_DEFAULT; }
	inline bool IsDofNone() const { return GetDofMode() == MARKER_DOF_MODE_NONE; }

	inline u8 GetDofMode() const { return m_data.m_dofMode >= MARKER_DOF_MODE_MAX ? MARKER_DOF_MODE_FIRST : m_data.m_dofMode; }

	static char const * GetFocusModeLngKey( eReplayDofFocusMode const focusMode )
	{
		static char const * const s_dofModeStrings[ MARKER_FOCUS_MAX ] =
		{
			"VEUI_FOCUS_AUTO", "VEUI_FOCUS_MAN", "VEUI_FOCUS_TARGET"
		};

		char const * const lngKey = focusMode >= MARKER_FOCUS_FIRST && focusMode < MARKER_FOCUS_MAX ? 
			s_dofModeStrings[ focusMode ] : "";

		return lngKey;
	}

	inline char const * GetFocusModeLngKey() const
	{
		return sReplayMarkerInfo::GetFocusModeLngKey( (eReplayDofFocusMode)m_data.m_dofFocusMode );
	}

	static char const * GetFocusModeHelpLngKey( eReplayDofFocusMode const focusMode )
	{
		static char const * const s_dofModeHelpStrings[ MARKER_FOCUS_MAX ] =
		{
			"VEUI_FOCUS_AUTOH", "VEUI_FOCUS_MANH", "VEUI_FOCUS_TARGETH"
		};

		char const * const lngKey = focusMode >= MARKER_FOCUS_FIRST && focusMode < MARKER_FOCUS_MAX ? 
			s_dofModeHelpStrings[ focusMode ] : "";

		return lngKey;
	}

	inline char const * GetFocusModeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetFocusModeHelpLngKey( (eReplayDofFocusMode)m_data.m_dofFocusMode );
	}

	bool UpdateFocusMode( rage::s32 const delta )
	{
		rage::u8 const c_startingFocus = m_data.m_dofFocusMode;
		rage::s32 const c_nextFocus = m_data.m_dofFocusMode + delta;

		m_data.m_dofFocusMode = ( c_nextFocus >= MARKER_FOCUS_MAX ) ? (u8)MARKER_FOCUS_FIRST :
			c_nextFocus < MARKER_FOCUS_FIRST ? (u8)(MARKER_FOCUS_MAX - 1) : (u8)c_nextFocus;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_DOF_FOCUS_MODE );

		return m_data.m_dofFocusMode != c_startingFocus;
	}

	inline u8 GetFocusMode() const { return m_data.m_dofFocusMode; }

	inline float GetFocalDistance() const { return m_data.m_dofFocalDistance; }
	inline s32 GetFocalDistanceWholeDecimetre() const { return s32( m_data.m_dofFocalDistance * 10.f ); }

	inline bool UpdateFocalDistance( float const delta )
	{
		float const c_startingDistance = m_data.m_dofFocalDistance;

		m_data.m_dofFocalDistance = rage::Clamp( m_data.m_dofFocalDistance + delta, MARKER_MIN_FOCAL_DIST, MARKER_MAX_FOCAL_DIST );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_DOF_FOCUS_DISTANCE );

		return c_startingDistance != m_data.m_dofFocalDistance;
	}

	inline float GetDofIntensity() const { return m_data.m_dofIntensity; }
	inline s32 GetDofIntensityWholePercent() const { return s32( m_data.m_dofIntensity ); }

	inline bool UpdateDofIntensity( float const delta )
	{
		float const c_oldIntensity = m_data.m_dofIntensity;
		m_data.m_dofIntensity = rage::Clamp( m_data.m_dofIntensity + delta, MARKER_MIN_DOF_INTENSITY, MARKER_MAX_DOF_INTENSITY );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_DOF_INTENSITY );

		return m_data.m_dofIntensity != c_oldIntensity;
	}

	inline rage::s32 GetFocusTargetId() const { return m_data.m_dofFocusTargetId; }
	inline void SetFocusTargetId( rage::s32 const dofFocusTargetId ) { m_data.m_dofFocusTargetId = dofFocusTargetId; }

	inline rage::s32 GetFocusTargetIndex() const { return Max(m_data.m_dofFocusTargetIndex, DEFAULT_MARKER_DOF_TARGETINDEX); }
	inline void SetFocusTargetIndex( rage::s32 const dofFocusTargetIndex ) { m_data.m_dofFocusTargetIndex = Max(dofFocusTargetIndex, DEFAULT_MARKER_DOF_TARGETINDEX); }

	inline bool CanChangeDofFocusDistance() const
	{
		return  m_data.m_dofFocusMode == MARKER_FOCUS_MANUAL;
	}

	inline bool CanChangeDofFocusTarget() const
	{
		return  m_data.m_dofFocusMode == MARKER_FOCUS_TARGET;
	}

	inline rage::s32 GetAttachTargetId() const { return m_data.m_AttachTargetId; }
	inline void SetAttachTargetId( rage::s32 const attachTargetId ) { m_data.m_AttachTargetId = attachTargetId; }

	inline rage::s32 GetAttachTargetIndex() const { return m_data.m_AttachTargetIndex; }
	inline void SetAttachTargetIndex( rage::s32 const attachTargetIndex ) 
    { 
        m_data.m_AttachTargetIndex = attachTargetIndex; 

        if( m_data.m_AttachTargetIndex != -1 && m_data.m_LookAtTargetId != m_data.m_AttachTargetId)
        {
            m_data.m_attachType = MARKER_CAMERA_ATTACH_POSITION_ONLY;
        }
    }

	inline rage::s32 GetLookAtTargetId() const { return m_data.m_LookAtTargetId; }
	inline void SetLookAtTargetId( rage::s32 const lookAtTargetId ) { m_data.m_LookAtTargetId = lookAtTargetId; }

	inline rage::s32 GetLookAtTargetIndex() const { return m_data.m_LookAtTargetIndex; }
	inline void SetLookAtTargetIndex( rage::s32 const lookAtTargetIndex )
	{
		m_data.m_LookAtTargetIndex = lookAtTargetIndex;

		if( m_data.m_LookAtTargetIndex != -1 && m_data.m_LookAtTargetId != m_data.m_AttachTargetId)
		{
			m_data.m_attachType = MARKER_CAMERA_ATTACH_POSITION_ONLY;
		}
	}

	inline float GetVerticalLookAtOffset() const { return m_data.m_VerticalLookAtOffset; }
	inline void SetVerticalLookAtOffset( float const verticalOffset ) { m_data.m_VerticalLookAtOffset = verticalOffset; }

	inline float GetHorizontalLookAtOffset() const { return m_data.m_HorizontalLookAtOffset; }
	inline void SetHorizontalLookAtOffset( float const horizontalOffset ) { m_data.m_HorizontalLookAtOffset = horizontalOffset; }

	inline float GetLookAtRoll() const { return m_data.m_LookAtRoll; }
	inline void SetLookAtRoll( float const roll ) { m_data.m_LookAtRoll = roll; }

	bool UpdateSfxVolume( rage::s32 const delta )
	{
		rage::u8 const c_startingVolume = m_data.m_SFXVolume;
		rage::s32 const c_newVolume = m_data.m_SFXVolume + delta;
		m_data.m_SFXVolume = (u8)rage::Clamp<rage::s32>( c_newVolume, 0, REPLAY_VOLUMES_COUNT - 1 );

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SFXVOLUME );

		return m_data.m_SFXVolume != c_startingVolume;
	}

	inline rage::u8 GetSfxVolume() const { return m_data.m_SFXVolume; }

    bool UpdateSfxOSVolume( rage::s32 const delta )
    {
        rage::u8 const c_startingVolume = m_data.m_oneshotVolume;
        rage::s32 const c_newVolume = m_data.m_oneshotVolume + delta;
        m_data.m_oneshotVolume = (u8)rage::Clamp<rage::s32>( c_newVolume, 0, REPLAY_VOLUMES_COUNT - 1 );

        SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SFX_OS_VOL );

        return m_data.m_oneshotVolume != c_startingVolume;
    }

    inline rage::u8 GetSfxOSVolume() const { return m_data.m_oneshotVolume; }

	bool UpdateMusicVolume( rage::s32 const delta )
	{
		rage::u8 const c_startingVolume = m_data.m_MusicVolume;
		rage::s32 const c_newVolume = m_data.m_MusicVolume + delta;
		m_data.m_MusicVolume = (u8)rage::Clamp<rage::s32>( c_newVolume, 0, REPLAY_VOLUMES_COUNT - 1 );

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_MUSICVOLUME );

		return m_data.m_MusicVolume != c_startingVolume;
	}

	inline rage::u8 GetMusicVolume() const { return m_data.m_MusicVolume; }

    bool UpdateAmbientVolume( rage::s32 const delta )
    {
        rage::u8 const c_startingVolume = m_data.m_AmbientVolume;
        rage::s32 const c_newVolume = m_data.m_AmbientVolume + delta;
        m_data.m_AmbientVolume = (u8)rage::Clamp<rage::s32>( c_newVolume, 0, REPLAY_VOLUMES_COUNT - 1 );

        SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_AMB_VOL );

        return m_data.m_AmbientVolume != c_startingVolume;
    }

    inline rage::u8 GetAmbientVolume() const { return m_data.m_AmbientVolume; }

	bool UpdateDialogVolume( rage::s32 const delta )
	{
		rage::u8 const c_startingVolume = m_data.m_DialogVolume;
		rage::s32 const c_newVolume = m_data.m_DialogVolume + delta;
		m_data.m_DialogVolume = (u8)rage::Clamp<rage::s32>( c_newVolume, 0, REPLAY_VOLUMES_COUNT - 1 );

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SPEECHVOLUME );

		return m_data.m_DialogVolume != c_startingVolume;
	}

	inline rage::u8 GetDialogVolume() const { return m_data.m_DialogVolume; }

	static char const * GetMarkerAudioIntensityLngKey( eReplayMarkerAudioIntensity const intensity )
	{
		static char const * const s_markerIntensityStrings[ MARKER_AUDIO_INTENSITY_MAX ] =
		{
			"VEUI_AUD_INT_MIN", "VEUI_AUD_INT_LOW", "VEUI_AUD_INT_MED", "VEUI_AUD_INT_HI", "VEUI_AUD_INT_MAX"
		};

		char const * const lngKey = intensity >= MARKER_AUDIO_INTENSITY_FIRST && intensity < MARKER_AUDIO_INTENSITY_MAX ? 
			s_markerIntensityStrings[ intensity ] : "";

		return lngKey;
	}

	inline char const * GetMarkerAudioIntensityLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerAudioIntensityLngKey( (eReplayMarkerAudioIntensity)m_data.m_ScoreIntensity );
	}

	bool UpdateScoreTrackIntensity( rage::s32 const delta )
	{
		rage::u8 const c_startingIntensity = m_data.m_ScoreIntensity;
		rage::s32 const c_nextIntensity = m_data.m_ScoreIntensity + delta;

		m_data.m_ScoreIntensity = ( c_nextIntensity >= MARKER_AUDIO_INTENSITY_MAX ) ? (u8)MARKER_AUDIO_INTENSITY_FIRST :
			c_nextIntensity < MARKER_AUDIO_INTENSITY_FIRST ? (u8)(MARKER_AUDIO_INTENSITY_MAX - 1) : (u8)c_nextIntensity;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SCORE_INTENSITY );

		return m_data.m_ScoreIntensity != c_startingIntensity;
	}

	static char const * GetMarkerMicrophoneTypeLngKey( eMarkerMicrophoneType const micType )
	{
		static char const * const s_markerMicrphoneStrings[ MARKER_AUDIO_INTENSITY_MAX ] =
		{
			"VEUI_AUD_MIC_DEF", "VEUI_AUD_MIC_CAM", "VEUI_AUD_MIC_TAR", "VEUI_AUD_MIC_CIN"
		};

		char const * const lngKey = micType >= MARKER_MICROPHONE_FIRST && micType < MARKER_MICROPHONE_MAX ? 
			s_markerMicrphoneStrings[ micType ] : "";

		return lngKey;
	}

	inline char const * GetMarkerMicrophoneTypeLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerMicrophoneTypeLngKey( (eMarkerMicrophoneType)m_data.m_microphoneType );
	}

	static char const * GetMarkerMicrophoneTypeHelpLngKey( eMarkerMicrophoneType const micType )
	{
		static char const * const s_markerMicrphoneHelpStrings[ MARKER_AUDIO_INTENSITY_MAX ] =
		{
			"VEUI_AUD_MIC_DEF_HLP", "VEUI_AUD_MIC_CAM_HLP", "VEUI_AUD_MIC_TAR_HLP", "VEUI_AUD_MIC_CIN_HLP"
		};

		char const * const lngKey = micType >= MARKER_MICROPHONE_FIRST && micType < MARKER_MICROPHONE_MAX ? 
			s_markerMicrphoneHelpStrings[ micType ] : "";

		return lngKey;
	}

	inline char const * GetMarkerMicrophoneTypeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetMarkerMicrophoneTypeHelpLngKey( (eMarkerMicrophoneType)m_data.m_microphoneType );
	}

	bool UpdateMarkerMicrophoneType( rage::s32 const delta )
	{
		rage::u8 const c_startingType = m_data.m_microphoneType;
		rage::s32 const c_nextType = m_data.m_microphoneType + delta;

		m_data.m_microphoneType = ( c_nextType >= MARKER_MICROPHONE_MAX ) ? (u8)MARKER_MICROPHONE_FIRST :
			c_nextType < MARKER_MICROPHONE_FIRST ? (u8)(MARKER_MICROPHONE_MAX - 1) : (u8)c_nextType;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_MIC_TYPE );

		return m_data.m_microphoneType != c_startingType;
	}

	inline u8 GetMicrophoneType() const { return m_data.m_microphoneType; }

	inline u8 GetScoreIntensity() const { return m_data.m_ScoreIntensity; }

    static char const * GetSfxCategoryLngKey( u32 const sfxCategory )
    {
        rage::u32 const c_maxCategories = CReplayAudioTrackProvider::GetCountOfCategoriesAvailable( REPLAY_SFX_ONESHOT );

        char const * const c_lngKey = sfxCategory < c_maxCategories ? 
            CReplayAudioTrackProvider::GetMusicCategoryNameKey( REPLAY_SFX_ONESHOT, sfxCategory ) : NULL;

        return c_lngKey;
    }

    inline char const * GetSfxCategoryLngKey() const
    {
        return sReplayMarkerInfo::GetSfxCategoryLngKey( m_data.m_sfxCategory );
    }

    bool UpdateMarkerSfxCategory( rage::s32 const delta )
    {
        rage::s32 const c_maxCategories = (s32)CReplayAudioTrackProvider::GetCountOfCategoriesAvailable( REPLAY_SFX_ONESHOT );
        rage::u32 const c_startingCat = m_data.m_sfxCategory;
        rage::s32 const c_nextCat = (s32)m_data.m_sfxCategory + delta;

        m_data.m_sfxCategory = u32( c_nextCat >= c_maxCategories ? 0 : c_nextCat < 0 ? c_maxCategories - 1 : c_nextCat );

        //! Get count of tracks for new category
        rage::u32 const c_maxTracks = CReplayAudioTrackProvider::GetTotalCountOfTracksForCategory( REPLAY_SFX_ONESHOT, m_data.m_sfxCategory );

        // Get details from previous hash
        rage::s32 categoryIndex( -1 );
        rage::s32 trackIndex( -1 );
        bool const c_foundIndices = CReplayAudioTrackProvider::GetCategoryAndIndexFromMusicTrackHash( REPLAY_SFX_ONESHOT, m_data.m_sfxHash, categoryIndex, trackIndex );

        //! Our new category has a track at the same index, so switch to that. Otherwise default to "None"
        m_data.m_sfxHash = c_foundIndices && trackIndex < c_maxTracks ? 
            CReplayAudioTrackProvider::GetMusicTrackSoundHash( REPLAY_SFX_ONESHOT, m_data.m_sfxCategory, trackIndex ) : g_NullSoundHash;

        SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SFX_CATEGORY );
        SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SFX_HASH );

        return m_data.m_sfxCategory != c_startingCat;
    }

    static bool GetSfxLngKey( u32 const sfxHash, char (&out_buffer)[ rage::LANG_KEY_SIZE ] )
    {
        bool success = false;

        if( sfxHash == g_NullSoundHash )
        {
            safecpy( out_buffer, "VEUI_SFX_NONE" );
            success = true;
        }
        else
        {
            success = CReplayAudioTrackProvider::GetMusicTrackNameKey( REPLAY_SFX_ONESHOT, sfxHash, out_buffer );
        }

        return success;
    }

    inline bool GetSfxLngKey( char (&out_buffer)[ rage::LANG_KEY_SIZE ] ) const
    {
        return sReplayMarkerInfo::GetSfxLngKey( m_data.m_sfxHash, out_buffer );
    }

    bool UpdateMarkerSfxHash( rage::s32 const delta )
    {
        rage::u32 const c_maxTracks = CReplayAudioTrackProvider::GetTotalCountOfTracksForCategory( REPLAY_SFX_ONESHOT, m_data.m_sfxCategory );
        rage::u32 const c_startingTrackHash = m_data.m_sfxHash;
        
        rage::s32 categoryIndex( -1 );
        rage::s32 trackIndex( -1 );

        bool const c_onNullHash = c_startingTrackHash == g_NullSoundHash;

        ASSERT_ONLY( bool const c_foundIndices = )CReplayAudioTrackProvider::GetCategoryAndIndexFromMusicTrackHash( REPLAY_SFX_ONESHOT, m_data.m_sfxHash, categoryIndex, trackIndex );
        Assertf( c_onNullHash || ( c_foundIndices && (u32)categoryIndex == m_data.m_sfxCategory ), "sReplayMarkerInfo::UpdateMarkerSfxHash - "
            "Data for this marker SFX is not valid. You may see strange behaviour on toggling..." );

        m_data.m_sfxCategory = c_onNullHash ? m_data.m_sfxCategory : categoryIndex;

        rage::s32 c_nextTrackIndex = c_onNullHash ? ( delta > 0 ? 0 : c_maxTracks - 1 ) : trackIndex + delta;
        trackIndex = ( c_nextTrackIndex >= c_maxTracks ) || ( c_nextTrackIndex < 0 ) ? -1 : c_nextTrackIndex;
        m_data.m_sfxHash = trackIndex == -1 ? g_NullSoundHash : CReplayAudioTrackProvider::GetMusicTrackSoundHash( REPLAY_SFX_ONESHOT, (u32)m_data.m_sfxCategory, (u32)trackIndex );

        SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_SFX_HASH );

        return m_data.m_sfxHash != c_startingTrackHash;
    }

    inline u32 GetSfxHash() const 
    { 
        return m_data.m_sfxHash; 
    }

	inline bool IsRecordedCamera() const
	{
		return m_data.m_camType == RPLYCAM_RECORDED;
	}

	inline bool IsTargetCamera() const
	{
		return m_data.m_camType == RPLYCAM_TARGET;
	}

	inline bool IsFreeCamera() const
	{
		return m_data.m_camType == RPLYCAM_FREE_CAMERA;
	}

	inline bool IsPresetCamera() const
	{
		return m_data.m_camType == RPLYCAM_FRONT_LOW || m_data.m_camType == RPLYCAM_REAR || m_data.m_camType == RPLYCAM_RIGHT_SIDE ||
			m_data.m_camType == RPLYCAM_LEFT_SIDE || m_data.m_camType == RPLYCAM_OVERHEAD;
	}

	inline bool IsEditableCamera() const
	{
		return  IsFreeCamera() || IsTargetCamera() || IsPresetCamera();
	}

	static char const * GetCameraTypeLngKey( ReplayCameraType const cameraType )
	{
		static char const * const s_replayCameraModeStrings[ RPLYCAM_MAX ] =
		{
			"VEUI_CAM_FRLOW", "VEUI_CAM_REAR", "VEUI_CAM_RHS", "VEUI_CAM_LHS", 
			"VEUI_CAM_ABOVE", "VEUI_CAM_RAW", "VEUI_CAM_FREE"
		};

		char const * const lngKey = cameraType >= RPLYCAM_FIRST && cameraType < RPLYCAM_MAX ? 
			s_replayCameraModeStrings[ cameraType ] : "";

		return lngKey;
	}

	inline char const * GetCameraTypeLngKey() const
	{
		return sReplayMarkerInfo::GetCameraTypeLngKey( (ReplayCameraType)m_data.m_camType );
	}

	static char const * GetCameraTypeHelpLngKey( ReplayCameraType const cameraType )
	{
		static char const * const s_replayCameraModeHelpStrings[ RPLYCAM_MAX ] =
		{
			"VEUI_CAM_FRLOWH", "VEUI_CAM_REARH", "VEUI_CAM_RHSH", "VEUI_CAM_LHSH", 
			"VEUI_CAM_ABOVEH", "VEUI_CAM_RAWH", "VEUI_CAM_FREEH"
		};

		char const * const lngKey = cameraType >= RPLYCAM_FIRST && cameraType < RPLYCAM_MAX ? 
			s_replayCameraModeHelpStrings[ cameraType ] : "";

		return lngKey;
	}

	inline char const * GetCameraTypeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetCameraTypeHelpLngKey( (ReplayCameraType)m_data.m_camType );
	}

	inline bool UpdateCameraType( rage::s32 const delta )
	{
		rage::u8 const c_startingType = m_data.m_camType;
		rage::s32 const c_nextType = m_data.m_camType + delta;

		m_data.m_camType = ( c_nextType >= RPLYCAM_MAX ) ? (u8)RPLYCAM_FIRST :
			c_nextType < RPLYCAM_FIRST ? (u8)(RPLYCAM_MAX - 1) : (u8)c_nextType;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_TYPE );

		return m_data.m_camType != c_startingType;
	}

	inline void SetCameraType( ReplayCameraType const cameraType ) { m_data.m_camType = (u8)cameraType; }
	inline rage::u8 GetCameraType() const { return m_data.m_camType; }

	inline bool UpdatePostFxIntensity( float const delta )
	{
		float const c_startingIntensity = m_data.m_postFxIntensity;
		m_data.m_postFxIntensity = rage::Clamp( m_data.m_postFxIntensity + delta, MARKER_MIN_PFX_INTENSITY, MARKER_MAX_PFX_INTENSITY );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_EFFECT_INTENSITY );
		return m_data.m_postFxIntensity != c_startingIntensity;
	}

	inline float GetPostFxIntensity() const { return m_data.m_postFxIntensity; }
	inline s32 GetPostFxIntensityWholePercent() const { return s32(m_data.m_postFxIntensity); }

	inline bool UpdateSaturationIntensity( float const delta )
	{
		float const c_startingIntensity = m_data.m_saturationIntensity;
		m_data.m_saturationIntensity = rage::Clamp( m_data.m_saturationIntensity + delta, MARKER_MIN_SATURATION, MARKER_MAX_SATURATION );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_EFFECT_SATURATION );
		return m_data.m_saturationIntensity != c_startingIntensity;
	}

	inline float GetSaturationIntensity() const { return m_data.m_saturationIntensity; }
	inline s32 GetSaturationIntensityWholePercent() const { return s32(m_data.m_saturationIntensity); }

	inline bool UpdateContrastIntensity( float const delta )
	{
		float const c_startingIntensity = m_data.m_contrastIntensity;
		m_data.m_contrastIntensity = rage::Clamp( m_data.m_contrastIntensity + delta, MARKER_MIN_CONTRAST, MARKER_MAX_CONTRAST );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_EFFECT_CONTRAST );
		return m_data.m_contrastIntensity != c_startingIntensity;
	}

	inline float GetContrastIntensity() const { return m_data.m_contrastIntensity; }
	inline s32 GetContrastIntensityWholePercent() const { return s32(m_data.m_contrastIntensity); }

	inline bool UpdateVignetteIntensity( float const delta )
	{
		float const c_startingIntensity = m_data.m_vignetteIntensity;
		m_data.m_vignetteIntensity = rage::Clamp( m_data.m_vignetteIntensity + delta, MARKER_MIN_VIGNETTE, MARKER_MAX_VIGNETTE );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_EFFECT_VIGNETTE );
		return m_data.m_vignetteIntensity != c_startingIntensity;
	}

	inline float GetVignetteIntensity() const { return m_data.m_vignetteIntensity; }
	inline s32 GetVignetteIntensityWholePercent() const { return s32(m_data.m_vignetteIntensity); }

	inline bool UpdateBrightness( float const delta )
	{
		float const c_startingBrightness = m_data.m_brightness;
		m_data.m_brightness = rage::Clamp( m_data.m_brightness + delta, MARKER_MIN_BRIGHTNESS, MARKER_MAX_BRIGHTNESS );
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_EFFECT_BRIGHTNESS );
		return m_data.m_brightness != c_startingBrightness;
	}

	inline float GetBrightness() const { return m_data.m_brightness; }
	inline s32 GetBrightnessWholePercent() const { return s32(m_data.m_brightness); }

	inline float GetNonDilatedTimeMs() const { return m_data.m_timeMs; }
	inline void SetNonDilatedTimeMs( float const newTimeMs ) { m_data.m_timeMs = newTimeMs; }

	inline float GetTransitionDurationMs() const { return m_data.m_transitionDurationMs; }
	inline void SetTransitionDurationMs( float const newDurationMs ) 
	{ 
		m_data.m_transitionDurationMs = newDurationMs; 
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_TRANS_DURATION );
	}

	inline rage::u8 GetActivePostFx() const { return m_data.m_activePostFx; }
	inline void SetActivePostFx( rage::u8 const newFx ) 
	{ 
		m_data.m_activePostFx = newFx; 
		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_EFFECT );
	}

	inline bool IsSpeechOn() const { return m_data.m_SpeechOn; }
	inline void SetSpeechOn( bool const isOn ) 
	{ 
		m_data.m_SpeechOn = isOn; 
	}

	inline bool IsAttachEntityMatrixValid() const { return m_data.m_AttachEntityMatrixValid; }
	inline void SetAttachEntityMatrixValid( bool const isValid ) { m_data.m_AttachEntityMatrixValid = isValid; }

	inline bool IsCamMatrixValid() const { return m_data.m_camMatrixValid; }
	inline void SetCamMatrixValid( bool const isValid ) { m_data.m_camMatrixValid = isValid; }

	char const * GetAttachTypeLngKey( eMarkerCameraAttachType const attachType ) const
	{
		static char const * const s_replayAttachTypeStrings[ MARKER_CAMERA_ATTACH_MAX ] =
		{
			"VEUI_CAM_ATTRI", "VEUI_CAM_ATTDE", "VEUI_CAM_ATTFL"
		};

		char const * const lngKey = attachType >= MARKER_CAMERA_ATTACH_FIRST && attachType < MARKER_CAMERA_ATTACH_MAX ? 
			s_replayAttachTypeStrings[ attachType ] : "";

		return lngKey;
	}

	inline char const * GetAttachTypeLngKey() const
	{
		return sReplayMarkerInfo::GetAttachTypeLngKey( (eMarkerCameraAttachType)m_data.m_attachType );
	}

	char const * GetAttachTypeHelpLngKey( eMarkerCameraAttachType const attachType ) const
	{
		static char const * const s_replayAttachTypeHelpStrings[ MARKER_CAMERA_ATTACH_MAX ] =
		{
			"VEUI_CAM_ATTRIH", "VEUI_CAM_ATTDEH", "VEUI_CAM_ATTFLH"
		};

		char const * const lngKey = attachType >= MARKER_CAMERA_ATTACH_FIRST && attachType < MARKER_CAMERA_ATTACH_MAX ? 
			s_replayAttachTypeHelpStrings[ attachType ] : "";

		return lngKey;
	}

	inline char const * GetAttachTypeHelpLngKey() const
	{
		return sReplayMarkerInfo::GetAttachTypeHelpLngKey( (eMarkerCameraAttachType)m_data.m_attachType );
	}

	inline bool IsAttachTypeFull() const { return m_data.m_attachType == MARKER_CAMERA_ATTACH_FULL; }
	inline bool IsAttachTypePositionAndHeading() const { return m_data.m_attachType == MARKER_CAMERA_ATTACH_POSITION_AND_HEADING; }
	inline bool IsAttachTypePositionOnly() const { return m_data.m_attachType == MARKER_CAMERA_ATTACH_POSITION_ONLY; }

	inline rage::u8 GetAttachType() const { return m_data.m_attachType; }

	inline bool UpdateAttachType( s32 const delta ) 
	{ 
		rage::s32 const c_startingType = m_data.m_attachType;
		rage::s32 const c_nextAttachType = m_data.m_attachType + delta;
		m_data.m_attachType = ( c_nextAttachType >= MARKER_CAMERA_ATTACH_MAX ) ? (u8)MARKER_CAMERA_ATTACH_FIRST :
			c_nextAttachType < MARKER_CAMERA_ATTACH_FIRST ? (u8)(MARKER_CAMERA_ATTACH_MAX - 1) : (u8)c_nextAttachType;

		SetPropertyAsEdited( MARKER_EDITED_PROPERTIES_CAM_ATTACH_TYPE );

		if (IsAttachTypeFull() && m_data.m_LookAtTargetIndex != -1 && m_data.m_LookAtTargetId != m_data.m_AttachTargetId)
		{
			m_data.m_LookAtTargetId		= DEFAULT_LOOKAT_TARGET_ID;
			m_data.m_LookAtTargetIndex	= DEFAULT_LOOKAT_TARGET_INDEX;
		}

		return m_data.m_attachType != c_startingType;
	}

	//! Functions for checking property edit flags
	inline void SetPropertyAsEdited( eMarkerEditedProperties const editedProperty )
	{
		m_data.m_editedPropertyFlags |= editedProperty;
	}

	inline void ClearPropertyEditState( eMarkerEditedProperties const editedProperty )
	{
		m_data.m_editedPropertyFlags &= ~editedProperty;
	}

	inline void ClearAllEditedProperty( rage::u64 const propertyMask )
	{
		m_data.m_editedPropertyFlags &= ~propertyMask;
	}

	inline bool IsPropertyEdited( eMarkerEditedProperties const editedProperty )
	{
		return ( m_data.m_editedPropertyFlags & editedProperty ) != 0;
	}

	inline bool IsAnyPropertyEdited( rage::u64 const propertyMask )
	{
		return ( m_data.m_editedPropertyFlags & propertyMask ) != 0;
	}

	inline void ResetEditedProperties()
	{
		m_data.m_editedPropertyFlags = MARKER_EDITED_PROPERTIES_NONE;
	}

private: // variables

	sMarkerData_Current	m_data;
};

// Typedefs for readability
typedef atArray<sReplayMarkerInfo*> ReplayMarkerInfoCollection;
typedef ReplayMarkerInfoCollection* ReplayMarkerInfoCollectionPointer;

inline static int ReplayMarkerCompareFunct( sReplayMarkerInfo* const* markerA, sReplayMarkerInfo* const* markerB )
{
	float const c_aTime = (*markerA)->GetNonDilatedTimeMs();
	float const c_bTime = (*markerB)->GetNonDilatedTimeMs();

	int const c_result = c_aTime < c_bTime ? -1 : c_aTime == c_bTime ? 0 : 1;
	return c_result;
}

#endif // GTA_REPLAY

#endif // __MARKERINFO
