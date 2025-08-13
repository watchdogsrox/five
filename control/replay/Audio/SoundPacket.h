//=========================================================
// Name:		SoundPacket.h
// Description:	Sound replay packet.
//=========================================================

#ifndef INC_SOUNDPACKET_H_
#define INC_SOUNDPACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/packetbasics.h"
#include "control/replay/ReplayTrackingInfo.h"
#include "control/replay/misc/ReplayPacket.h"
#include "control/replay/effects/particlepacket.h"

#include "atl/map.h"
#include "audioengine/metadataref.h"
#include "scene/EntityTypes.h"
#include "audioengine/entity.h"


#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

//=========================================================
// Forward Declarations
//=========================================================
class audSpeechAudioEntity;
namespace rage
{
struct audSoundInitParams;
class audSound;
class audWaveSlot;
}
class CPed;
class CVehicle;
class audScene;
class naEnvironmentGroup;

//---------------------------------------------------------
// Namespace: ReplaySound
// Desc:      Keeps miscellaneous constants and datatypes
//             for reproducing sounds in replays out of the
//             global namespace.
//---------------------------------------------------------
class ReplaySound
{
public:
	//=========================================================
	// Constants
	//=========================================================
	static const u32 MAX_CONTEXT_NAME_LENGTH;	// See X:\audio\gta4_rel\Reports\CONTEXTLOOKUP.TXT

	//=========================================================
	// Enums
	//=========================================================

	// Context Where Sound Was Created and Played
	//  - Allows us to reproduce context specific settings.
	//  - Also useful for debugging.
	enum eContext
	{
		CONTEXT_INVALID		= 0,

		//Heli
		CONTEXT_HELI_GOING_DOWN,

		CONTEXT_AMBIENT_REGISTERED_EFFECTS,
	};

	// Type of Client Variable for a Sound's Requested Settings
	enum eClientVariableType
	{
		CLIENT_VAR_NONE	= 0,
		CLIENT_VAR_U32,
		CLIENT_VAR_F32,
		CLIENT_VAR_PTR
	};

	// TODO: Will we need this?
	/*enum eSoundStatus
	{
		SOUND_NEW = 0,
		SOUND_PLAYED
	};*/

	enum eSpeechAudEntityUpdateType
	{
		SPEECH_UPDATE_INVALID					= 0,
		SPEECH_UPDATE_FREE_SLOT_FUNC,
		SPEECH_UPDATE_ORPHAN_SPEECH_FUNC,
		SPEECH_UPDATE_UPDATE_FUNC,
		SPEECH_UPDATE_STOP_AMBIENT_SPEECH_FUNC,
		SPEECH_UPDATE_STOP_SCRIPTED_SPEECH_FUNC,
		SPEECH_UPDATE_STOP_ALL
	};

	//=========================================================
	// Init Params
	//=========================================================

	//---------------------------------------------------------
	// Class: CInitParams
	// Desc:  Holds settings to create sounds
	//---------------------------------------------------------
	struct CInitParams
	{
	public:
		void	Populate( const audSoundInitParams &originalInitParams, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID );
	
	public:
		CPacketVector3 Position;
		float	Volume;
		f32		PostSubmixAttenuation;
		s16		Pitch;
		s16		Pan;
		f32		VolumeCurveScale;
		s32		PrepareTimeLimit;

		u32		Context;
		u32		CategoryHashValue;

		s32		StartOffset;
		s32		Predelay;
		u32		AttackTime;

		union
		{
			u32		u32ClientVar;
// 			f32		f32ClientVar;
// 			void	*ptrClientVar; // for x64?
		};

		u32		TimerId;
		u32		LPFCutoff;
		u32		VirtualisationScoreOffset;
		s32		ShadowPcmSourceId;
		s32		PcmSourceChannelId;
		s16		SourceEffectSubmixId;
		u8		BucketId;
		u8		EffectRoute;
		u8		SpeakerMask;
	

		bool	PrimaryShadowSound		: 1;	// 1st bit
		bool	IsStartOffsetPercentage	: 1;
		bool	UpdateEntity			: 1;
		bool	AllowOrphaned			: 1;
		bool	AllowLoad				: 1;
		bool	RemoveHierarchy			: 1;
		bool	ShouldPlayPhysically	: 1;	
		bool	IsAuditioning			: 1;	// 8th bit
		bool	HasPosition				: 1;	// 1st bit
		bool	HasTracker				: 1;
		bool	HasEffectChain			: 1;	// unused?
		bool	HasWaveSlot				: 1;
		bool	HasCategory				: 1;
		bool	HasEnvironmentGroup		: 1;
		bool	HasShadowSound			: 1;	// unused?	
		bool	/* Padding */			: 1;	// 8th bit
	};

	//=========================================================
	// Sound Settings
	//=========================================================
	
	//---------------------------------------------------------
	// Class: CEnvelopeSoundUpdateSettings
	// Desc:  Holds settings to play back envelope sounds
	//---------------------------------------------------------
	class CEnvelopeSoundUpdateSettings
	{
	public:
		void		SetRequestedEnvelope( u32 attack, u32 decay, u32 sustain, s32 hold, s32 release );
		void		SetChildSoundReference( u32 childSoundHash )	{ m_ChildSoundHash = childSoundHash; }

		u32			GetAttack()			const	{ return m_Attack;			}
		u32			GetDecay()			const	{ return m_Decay;			}
		u32			GetSustain()		const	{ return m_Sustain;			}
		s32			GetHold()			const	{ return m_Hold;			}
		s32			GetRelease()		const	{ return m_Release;			}
		u32			GetChildSoundHash()	const	{ return m_ChildSoundHash;	}

	private:
		u32			m_Attack;
		u32			m_Decay;
		u32			m_Sustain;
		s32			m_Hold;
		s32			m_Release;
		u32			m_ChildSoundHash;
	};

	//=========================================================
	// Packet Params
	//=========================================================

	//---------------------------------------------------------
	// Class: CCreateParams
	// Desc:  Holds settings required for recreating a sound
	//---------------------------------------------------------
	class CCreateParams
	{
	public:
		CCreateParams( eContext context = CONTEXT_INVALID ) :
			m_CategoryHashValue( 0 ),	m_OwnerReplayId( -1 ), m_Context( context )
		{
		}

		void		SetCategoryHashValue( u32 value )	{ m_CategoryHashValue	= value;	}
		void		SetCategoryRolloff( f32 rolloff )	{ m_CategoryRolloff		= rolloff;	}
		void		SetOwnerReplayId( s32 id )			{ m_OwnerReplayId		= id;		}
		void		SetContext( eContext context )		{ m_Context				= context;	}

		u32			GetCategoryHashValue()	const		{ return m_CategoryHashValue;	}
		f32			GetCategoryRolloff()	const		{ return m_CategoryRolloff;		}
		s32			GetOwnerReplayId()		const		{ return m_OwnerReplayId;		}
		eContext	GetContext()			const		{ return m_Context;				}

	private:
		// Values to Help Obtain Pointers to Required Objects for Sound Creation
		union
		{
			u32		m_CategoryHashValue;
			f32		m_CategoryRolloff;
		};
		
		s32			m_OwnerReplayId;
		eContext	m_Context;
	};

	//---------------------------------------------------------
	// Class: CUpdateParams
	// Desc:  Holds settings required for playing a sound
	//---------------------------------------------------------
	class CUpdateParams
	{
	public:
		CUpdateParams( eContext context = CONTEXT_INVALID );

		// Miscellaneous Values
		void		SetClientVariableType( eClientVariableType clientVariableType )	{ m_ClientVariableType	= static_cast<u8> (clientVariableType);	}
		void		SetOwnerReplayId( s32 id )										{ m_OwnerReplayId		= id;		}
		void		SetContext( eContext context )									{ m_Context				= context;	}

		// Prepare and Play 
		void		SetPrepareAndPlay( bool wasWaveSlotProvided = false, bool allowLoad = false, s32 timeLimit = 0 );

		// Requested Settings
		void		SetPostSubmixVolumeAttenuation( f32 volumeAttenuation );
		void		SetVolume( f32 volume );
		void		SetPosition( Vector3 &position );
		void		SetPitch( s32 pitch );
		void		SetLowPassFilterCutoff( u32 lowPassFilterCutoff );
		void		SetShouldUpdateEntity( bool shouldUpdate );
		void		SetAllowOrphaned( bool allowOrphaned );
		void		SetShouldInvalidateEntity( bool shouldInvalidate )	{ m_ShouldInvalidateEntity = shouldInvalidate; }

		void		SetClientVariable( u32	value	);
		void		SetClientVariable( f32	value	);
		void		SetClientVariable( void *value	);

		// Miscellaneous Values
		eClientVariableType	GetClientVariableType()				const { return static_cast<eClientVariableType> (m_ClientVariableType);	}
		s32					GetOwnerReplayId()					const { return m_OwnerReplayId;			}
		eContext			GetContext()						const { return m_Context;				}

		// Prepare and Play 
		bool				GetWasWaveSlotProvided()			const { return m_WasWaveSlotProvided;	}
		bool				GetAllowLoad()						const { return m_AllowLoad;				}
		s32					GetTimeLimit()						const { return m_TimeLimit;				}

		// Requested Settings
		f32					GetPostSubmixVolumeAttenuation()	const { return m_PostSubmixVolumeAttenuation;	}
		f32					GetVolume()							const { return m_Volume;						}
		Vector3				GetPosition()						const;
		s32					GetPitch()							const { return m_Pitch;							}
		u32					GetLowPassFilterCutoff()			const { return m_LowPassFilterCutoff;			}
		bool				GetShouldUpdateEntity()				const { return m_ShouldUpdateEntity;			}
		bool				GetAllowOrphaned()					const { return m_AllowOrphaned;					}
		bool				GetShouldInvalidateEntity()			const { return m_ShouldInvalidateEntity;		}

		u32			GetClientVariableU32()						const { return m_ClientVariable.u32Val;	}
		f32			GetClientVariableF32()						const { return m_ClientVariable.f32Val;	}
		//void*		GetClientVariablePtr()						const { return m_ClientVariable.ptrVal;	}

		// Access Changed Requested Settings Bitfield
		bool		GetHasPostSubmixVolumeAttenuationChanged()	const { return m_HasPostSubmixVolumeAttenuationChanged;	}
		bool		GetHasVolumeChanged()						const { return m_HasVolumeChanged;						}
		bool		GetHasPositionChanged()						const { return m_HasPositionChanged;					}
		bool		GetHasPitchChanged()						const { return m_HasPitchChanged;						}
		bool		GetHasLowPassFilterCutoffChanged()			const { return m_HasLowPassFilterCutoffChanged;			}
		bool		GetHasShouldUpdateEntityChanged()			const { return m_HasShouldUpdateEntityChanged;			}
		bool		GetHasAllowOrphanedChanged()				const { return m_HasAllowOrphanedChanged;				}

		bool		GetHasOnlyLowPassFilterCutoffChanged()			const;
		bool		GetHasOnlyPitchChanged()						const;
		bool		GetHasOnlyPitchPositionChanged()				const;
		bool		GetHasOnlyPitchPositionVolumeChanged()			const;
		bool		GetHasOnlyPitchVolumeChanged()					const;
		bool		GetHasOnlyPositionChanged()						const;
		bool		GetHasOnlyPositionVolumeChanged()				const;
		bool		GetHasOnlyPostSubmixVolumeAttenuationChanged()	const;
		bool		GetHasOnlyVolumeChanged()						const;

	private:
		// Miscellaneous Values
		s32					m_OwnerReplayId;
		eContext			m_Context;

		// Requested Settings
		f32					m_PostSubmixVolumeAttenuation;
		f32					m_Volume;
		f32					m_Position[3];
		s32					m_Pitch;
		u32					m_LowPassFilterCutoff;

		union
		{
			u32				u32Val;
			f32				f32Val;
			/*void			*ptrVal;*/ // for x64?
		}					m_ClientVariable;

		// Prepare and Play Parameters
		s32					m_TimeLimit;
		bool				m_WasWaveSlotProvided					: 1;	// 1st bit
		bool				m_AllowLoad								: 1;

		// Requested Settings
		bool				m_ShouldUpdateEntity					: 1;
		bool				m_AllowOrphaned							: 1;
		bool				m_ShouldInvalidateEntity				: 1;
		bool				/* Padding */							: 3;	// 8th bit

		// Bit Field to Track Which Requested Settings were Changed
		bool				m_HasPostSubmixVolumeAttenuationChanged	: 1;	// 1st bit
		bool				m_HasVolumeChanged						: 1;
		bool				m_HasPositionChanged					: 1;
		bool				m_HasPitchChanged						: 1;
		bool				m_HasLowPassFilterCutoffChanged			: 1;
		bool				m_HasShouldUpdateEntityChanged			: 1;
		bool				m_HasAllowOrphanedChanged				: 1;
		bool				/* Padding */							: 1;	// 8th bit

		// Miscellaneous Values
		u8					m_ClientVariableType;
	};

	//=========================================================
	// Helper Functions
	//=========================================================

	/*TODO4FIVE audOcclusionGroupInterface* CreateOcclusionGroupForExplosion( CEntity *pEntity, const Vector3 *pPosition );*/
	
	static bool			ExtractSoundInitParams( audSoundInitParams &initParams, const CInitParams &storedInitParams, CEntity *pEntity, const CCreateParams *createParams, Vector3 *pPosition = NULL, u32 painVoiceType = 0, u8 soundEntity = 0, bool* createdEnvironmentGroup = nullptr);
	//static void			ExtractSoundSettings( audSound *pSound, const CUpdateParams *updateParams );
	static audWaveSlot*	ExtractWaveSlotPointer( eContext context, CEntity* pEntity );
	static naEnvironmentGroup*	ExtractEnvironmentGroupPointer(const CInitParams &storedInitParams, CEntity* pEntity, bool* createdEnvironmentGroup);
	static void			StoreSoundOwnerEntityType( s32 ownerReplayID, u8 &ownerEntityType );
	static audSound**	GetPersistentFireSoundPtr( u32 searchStartIdx = 0 );
	static bool			CreatePlayAndBindFireSound( s32 soundId, u32 fireSoundHash, audSoundInitParams &initParams );
	static CPed*		GetPedPtrFromReplayId( s32 replayId );


	enum
	{
		eHalfSpeedSound,
		eQuarterSpeedSound,
		eDoubleSpeedSound,
		eTripleSpeedSound,

	};

	class CSoundRegister
	{
	public:
		audMetadataRef	HalfSpeedSound;
		audMetadataRef	QuarterSpeedSound;
		audMetadataRef	DoubleSpeedSound;
		audMetadataRef	TripleSpeedSound;
	};

	static	atMap<int, CSoundRegister>	SoundsRegister;

	static void			RegisterSounds(int normalSound, audMetadataRef halfSound, audMetadataRef quarterSound, audMetadataRef doubleSound, audMetadataRef tripleSound);
	static void			GetSound(int speed, audMetadataRef& normalSound);


};



//---------------------------------------------------------
// Class: CPacketSound
// Desc:  Base history buffer sound packet to inherit from.
//        Note that inheriting from this will not allow us
//        to pack bytes between m_PacketID and m_GameTime.
//---------------------------------------------------------
enum eAudioSoundEntityType
{
	eNoGlobalSoundEntity = 0,
	eWeaponSoundEntity = 1,
	eVehicleSoundEntity, // Don't think we'll need this one
	eAmbientSoundEntity, 
	eCollisionAudioEntity,
	eFrontendSoundEntity,
	eScriptSoundEntity,
};


// template<int SNDCOUNT = 1>
// class CPacketSound : public CPacketEventTracked
// {
// public:
// 	CPacketSound(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version = InitialVersion)
// 		: CPacketEventTracked(packetID, size, version)
// 		REPLAY_GUARD_ONLY(, m_SoundGuard(REPLAY_GUARD_SOUND))
// 	{}
// 
// 	//int			GetNumTrackedSounds() const				{	return SNDCOUNT;			}
// 
// 	// By default we cancel this sound packet if the sound it is referring
// 	// to has not started tracking...we make no change to the tracking value
// // 	bool		Setup(bool tracked)
// // 	{
// // 		replayDebugf2("Continue tracking %d", GetTrackedID());
// // 		return tracked;
// // 	}
// 
// 	bool		ValidatePacket() const				
// 	{	
// #if REPLAY_GUARD_ENABLE
// 		replayAssertf(m_SoundGuard == REPLAY_GUARD_SOUND, "Validation of CPacketSound Failed!");
// 		return CPacketEventTracked::ValidatePacket() && m_SoundGuard == REPLAY_GUARD_SOUND;	
// #else
// 		return CPacketEventTracked::ValidatePacket();
// #endif
// 	}
// 
// 	void		PrintXML(FileHandle handle) const
// 	{
// 		CPacketEventTracked::PrintXML(handle);
// 	}
// 
// 	void		SetSoundPad16(s16 s)		{	m_pad = s;		}
// 	s16			GetSoundPad16() const		{	return m_pad;	}
// 
// protected:
// 	REPLAY_GUARD_ONLY(u32 m_SoundGuard;)
// 
// private:
// 	CPacketSound(){}
// };

//---------------------------------------------------------
// Class: CPacketSoundCreate
// Desc:  History buffer packet containing information to
//         recreate a sound in replays.
//---------------------------------------------------------
class CPacketSoundCreate : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketSoundCreate(audMetadataRef soundHash, const audSoundInitParams* initParams, u8 soundEntity/* = -1*/);

	void		SetOwnerEntityType( u32 type )	{ m_OwnerEntityType = static_cast<u8> (type); }
	s32			GetOwnerReplayID()		const	{ return m_CreateParams.GetOwnerReplayId();	}
	u8			GetOwnerEntityType()	const	{ return m_OwnerEntityType;					}
	bool		GetIsPersistent()		const	{ return m_IsPersistent != 0;				}
	audSound**	GetPersistentSoundPtrAddress();

	bool		ShouldInvalidate()		const	{	return GetReplayID() >= 0;	}
	void		Invalidate()					{ 	 }
	bool		Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const						{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const						{ return PREPLAY_SUCCESSFUL; }

	bool		ExtractAndPlay( audSound **ppSound );

	bool		Setup(bool ASSERT_ONLY(trackingIDisTracked))	{	REPLAY_ASSERTF(!trackingIDisTracked, "Should not already be set up");	return true;	}

	void		Print() const					{}
	void		PrintXML(FileHandle handle) const	{ (void)handle; }

	bool		ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CREATE_AND_PLAY, "Validation of CPacketSoundCreate Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CREATE_AND_PLAY;
	}

protected:
	u8								m_IsPersistent;
	u8								m_IsOffset;
	u8								m_OwnerEntityType;	
	audMetadataRef					m_SoundHashOrMeta;

	s8								m_SoundEntity;

	ReplaySound::CCreateParams		m_CreateParams;
	ReplaySound::CInitParams		m_StoredInitParams;
	bool		m_hasParams;
};


class CPacketSoundCreatePos : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CPacketSoundCreatePos(eAudioSoundEntityType type, u32 soundHash,const Vector3& position);

	void			Extract( const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CREATE_POSITION, "Validation of CPacketSoundCreatePos Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CREATE_POSITION;
	}

	static void CalculateValidMetadataOffsets();

protected:	
	u32 m_SoundHash;
	CPacketVector3 m_Position;
	u8 m_SoundEntityType;

private:
	static atArray<u32> sm_ValidMetadataRefs;
};


class CPacketSoundCreateMisc_Old : public CPacketEvent, public CPacketEntityInfo<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:

	CPacketSoundCreateMisc_Old() : CPacketEvent(PACKETID_SOUND_CREATE_MISC_OLD, sizeof(CPacketSoundCreateMisc_Old)){}

	void			Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const			{ return PRELOAD_SUCCESSFUL; } 
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CREATE_MISC_OLD, "Validation of CPacketSoundCreateMisc Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CREATE_MISC_OLD;
	}

protected:
	void Init(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash);

protected:	
	CPacketVector3 m_Position;
	s32 m_Predelay;
	s32 m_StartOffset;
	u32 m_SoundHash;
	u32 m_SlowMoSoundHash;
	u32 m_SoundSetHash;
	float m_Volume;					//convert to u8?
	s16 m_Pitch;	
	u8 m_SoundEntityType			: 4;
	u8 m_IsStartOffsetPercentage	: 1;	
	u8 m_TrackEntityPosition		: 1;
	u8 m_HasEnvironmentGroup		: 1;
	u8 m_useSoundHash				: 1;
};


class CPacketSoundCreateMisc : public CPacketEvent, public CPacketEntityInfoStaticAware<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:

	CPacketSoundCreateMisc(const u32 soundSetHash, const u32 soundHash,  const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash = g_NullSoundHash);
	CPacketSoundCreateMisc(const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash = g_NullSoundHash);

	void			Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const			{ return PRELOAD_SUCCESSFUL; } 
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CREATE_MISC, "Validation of CPacketSoundCreateMisc Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CREATE_MISC;
	}

protected:
	void Init(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash);

protected:	
	CPacketVector3 m_Position;
	s32 m_Predelay;
	s32 m_StartOffset;
	u32 m_SoundHash;
	u32 m_SlowMoSoundHash;
	u32 m_SoundSetHash;
	float m_Volume;					//convert to u8?
	s16 m_Pitch;	
	u8 m_SoundEntityType			: 4;
	u8 m_IsStartOffsetPercentage	: 1;	
	u8 m_TrackEntityPosition		: 1;
	u8 m_HasEnvironmentGroup		: 1;
	u8 m_useSoundHash				: 1;
};


class CPacketSoundCreateMiscWithVars : public CPacketEvent, public CPacketEntityInfoStaticAware<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:

	CPacketSoundCreateMiscWithVars(const u32 soundSetHash, const u32 soundHash,  const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash = g_NullSoundHash);
	CPacketSoundCreateMiscWithVars(const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash = g_NullSoundHash);

	void			Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const			{ return PRELOAD_SUCCESSFUL; } 
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CREATE_MISC_W_VAR, "Validation of CPacketSoundCreateMiscWithVars Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CREATE_MISC_W_VAR;
	}

protected:
	void Init(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash);

protected:	
	CPacketVector3 m_Position;
	s32 m_Predelay;
	s32 m_StartOffset;
	u32 m_SoundHash;
	u32 m_SlowMoSoundHash;
	u32 m_SoundSetHash;
	float m_Volume;					//convert to u8?
	s16 m_Pitch;	

	struct audVariableValue
	{
		audVariableValue()
			: nameHash(0)
		{
		}
		u32 nameHash;
		float value;
	};
	enum {kMaxInitParamVariables = 4};
	
	audVariableValue m_Variables[kMaxInitParamVariables];

	u8 m_SoundEntityType			: 4;
	u8 m_IsStartOffsetPercentage	: 1;	
	u8 m_TrackEntityPosition		: 1;
	u8 m_HasEnvironmentGroup		: 1;
	u8 m_useSoundHash				: 1;
};


struct replaytrackedSound
{
	replaytrackedSound() { m_pSound = NULL; m_slot = -1; }
	replaytrackedSound(audSound* sound) { m_pSound = sound; m_slot = -1;}
	replaytrackedSound(audSound* sound, s32 slot) { m_pSound = sound; m_slot = slot;}
	replaytrackedSound(const replaytrackedSound& other) { m_pSound = other.m_pSound; m_slot = other.m_slot; }

	bool operator !=(audSound* sound) const { return m_pSound != sound; }
	bool operator ==(const replaytrackedSound& other) const { return m_pSound == other.m_pSound; }


	audSound* m_pSound;
	s32		  m_slot;
};

typedef replaytrackedSound tTrackedSoundType;
class CPacketSoundCreatePersistant : public CPacketEventTracked, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketSoundCreatePersistant(u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity = eNoGlobalSoundEntity, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID);
	CPacketSoundCreatePersistant(u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity = eNoGlobalSoundEntity, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID);

	void		SetOwnerEntityType( u32 type )	{ m_OwnerEntityType = static_cast<u8> (type); }
	s32			GetOwnerReplayID()		const	{ return m_CreateParams.GetOwnerReplayId();	}
	u8			GetOwnerEntityType()	const	{ return m_OwnerEntityType;					}
	bool		GetIsPersistent()		const	{ return m_IsPersistent != 0;				}
	audSound**	GetPersistentSoundPtrAddress();
	void		Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const;
	bool		ExtractAndPlay( audSound **ppSound );

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{		return true;	}
	bool		ShouldStartTracking() const						{	return true;	}

	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void		PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CREATE_AND_PLAY_PERSIST, "Validation of CPacketSoundCreatePersistant Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CREATE_AND_PLAY_PERSIST;
	}

protected:
	void Init(const u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity, ReplaySound::eContext context);

protected:
	u8								m_IsPersistent;
	u8								m_IsOffset;
	u8								m_OwnerEntityType;	
	u32								m_SoundSetHash;
	u32								m_SoundHash;
	bool							m_useSoundHash;

	u8								m_SoundEntity;

	ReplaySound::CCreateParams		m_CreateParams;
	ReplaySound::CInitParams		m_StoredInitParams;
	bool							m_hasParams;

	bool							m_play;
};


class CPacketSoundUpdatePersistant : public CPacketEventTracked, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketSoundUpdatePersistant(u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity = eNoGlobalSoundEntity, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID, const u32 playTime = 0);
	CPacketSoundUpdatePersistant(u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity = eNoGlobalSoundEntity, ReplaySound::eContext context = ReplaySound::CONTEXT_INVALID, const u32 playTime = 0);

	void		SetOwnerEntityType( u32 type )	{ m_OwnerEntityType = static_cast<u8> (type); }
	s32			GetOwnerReplayID()		const	{ return m_CreateParams.GetOwnerReplayId();	}
	u8			GetOwnerEntityType()	const	{ return m_OwnerEntityType;					}
	bool		GetIsPersistent()		const	{ return m_IsPersistent != 0;				}
	audSound**	GetPersistentSoundPtrAddress();
	void		Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const;
	bool		ExtractAndPlay( audSound **ppSound );

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{		return true;	}
	bool		ShouldStartTracking() const						{	return true;	}

	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void		PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_UPDATE_PERSIST, "Validation of CPacketSoundUpdatePersistant Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_UPDATE_PERSIST;
	}

protected:
	void Init(const u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity, ReplaySound::eContext context, u32 playTime);

protected:
	u8								m_IsPersistent;
	u8								m_IsOffset;
	u8								m_OwnerEntityType;	
	u32								m_SoundSetHash;
	u32								m_SoundHash;
	bool							m_useSoundHash;

	u8								m_SoundEntity;

	ReplaySound::CCreateParams		m_CreateParams;
	ReplaySound::CInitParams		m_StoredInitParams;
	bool							m_hasParams;

	bool							m_play;
	u32								m_playTime;
};

//////////////////////////////////////////////////////////////////////////
class CPacketSoundPhoneRing : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:
	CPacketSoundPhoneRing(u32 soundSetHash, u32 soundHash, bool triggerRingtoneAsHUDSound, s32 m_RingToneTimeLimit, u32 startTime);

	void		Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const;

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{ return true; }
	bool		ShouldStartTracking() const							{ return true; }

	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void		PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_PHONE_RING, "Validation of CPacketSoundPhoneRing Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PHONE_RING;
	}

protected:
	u32				m_SoundSetHash;
	u32				m_SoundHash;
	bool			m_TriggerRingtoneAsHUDSound;
	s32				m_RingToneTimeLimit;
	
	//Version 1
	u32				m_StartTime;
};

//////////////////////////////////////////////////////////////////////////
class CPacketSoundStopPhoneRing : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:
	CPacketSoundStopPhoneRing();

	void		Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const;

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{ return true; }
	bool		ShouldEndTracking() const							{ return true; }


	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const		{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const		{ return PREPLAY_SUCCESSFUL; }

	void		PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_STOP_PHONE_RING, "Validation of CPacketSoundStopPhoneRing Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_STOP_PHONE_RING;
	}
};

//---------------------------------------------------------
// Class: CPacketSoundUpdateAndPlay
// Desc:  History buffer packet holding information to
//         play a particular sound in a replay.
//---------------------------------------------------------
class CPacketSoundUpdateAndPlay : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdateAndPlay(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void		PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_UPDATE_AND_PLAY, "Validation of CPacketSoundUpdateAndPlay Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_UPDATE_AND_PLAY;
	}

private:
	ReplaySound::CUpdateParams		m_UpdateParams;
	u8								m_OwnerEntityType;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdateSettings
// Desc:  History buffer packet holding information to
//         update a particular sound in a replay.
//---------------------------------------------------------
class CPacketSoundUpdateSettings : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdateSettings(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void	SetSoundID( s32 newId )		{ m_SoundID = newId;	}
	s32		GetSoundID()		  const	{ return m_SoundID;	}
	Vector3	GetPosition()		  const;

	void	Init();
	void	Store( s32 soundId, ReplaySound::CUpdateParams &updateParams );
	void	Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldInvalidate() const			{	return false;	}
	void	Invalidate()						{ 	}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_UPDATE_SETTINGS, "Validation of CPacketSoundUpdateSettings Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_UPDATE_SETTINGS;
	}

	void	Print() const						{}
	void	PrintXML(FileHandle) const	{ }
protected:
	
	// Bit Field to Track Which Requested Settings were Changed
	u8						m_HasPostSubmixVolumeAttenuationChanged	: 1;	// 1st bit
	u8						m_HasVolumeChanged						: 1;
	u8						m_HasPositionChanged					: 1;
	u8						m_HasPitchChanged						: 1;
	u8						m_HasLowPassFilterCutoffChanged			: 1;
	u8						m_Padding								: 3;	// 8th bit

	u16						m_Context;

	s32						m_SoundID;

	// Requested Settings
	f32						m_PostSubmixVolumeAttenuation;
	f32						m_Volume;
	f32						m_Position[3];
	s32						m_Pitch;
	u32						m_LowPassFilterCutoff;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdateLPFCutoff
// Desc:  History buffer packet holding information to
//         update the LPF cutoff of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdateLPFCutoff: public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdateLPFCutoff(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_LPF_CUTOFF, "Validation of CPacketSoundUpdateLPFCutoff Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_LPF_CUTOFF;
	}

private:
	u32		m_LowPassFilterCutoff;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePitch
// Desc:  History buffer packet holding information to
//         update the pitch of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePitch : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePitch(const s32 pitch);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_UPDATE_PITCH, "Validation of CPacketSoundUpdatePitch Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_UPDATE_PITCH;
	}

private:
	s16		m_pitch;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePosition
// Desc:  History buffer packet holding information to
//         update the position of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePosition : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePosition(Vec3V_In position);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_POS, "Validation of CPacketSoundUpdatePosition Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_POS;
	}

private:
	CPacketVector<3>	m_Position;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePostSubmix
// Desc:  History buffer packet holding information to
//         update the post submix volume attenuation of a 
//         particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePostSubmix : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePostSubmix(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_POST_SUBMIX, "Validation of CPacketSoundUpdatePostSubmix Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_POST_SUBMIX;
	}

private:
	f32		m_PostSubmixVolumeAttenuation;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdateVolume
// Desc:  History buffer packet holding information to
//         update the volume of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdateVolume : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdateVolume(const float volume);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_UPDATE_VOLUME, "Validation of CPacketSoundUpdateVolume Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_UPDATE_VOLUME;
	}

private:
	f32		m_Volume;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePitchPos
// Desc:  History buffer packet holding information to
//         update the pitch & position of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePitchPos : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePitchPos(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Store( s32 soundId, ReplaySound::CUpdateParams &updateParams );
	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_PITCH_POS, "Validation of CPacketSoundUpdatePitchPos Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PITCH_POS;
	}

private:
	s32		m_Pitch;
	f32		m_Position[3];
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePitchVol
// Desc:  History buffer packet holding information to
//         update the pitch & volume of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePitchVol : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePitchVol(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_PITCH_VOL, "Validation of CPacketSoundUpdatePitchVol Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PITCH_VOL;
	}

private:
	s32		m_Pitch;
	f32		m_Volume;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePosVol
// Desc:  History buffer packet holding information to
//         update the position & volume of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePosVol : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePosVol(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_POS_VOL, "Validation of CPacketSoundUpdatePosVol Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_POS_VOL;
	}

private:
	f32		m_Position[3];
	f32		m_Volume;
};

//---------------------------------------------------------
// Class: CPacketSoundUpdatePitchPosVol
// Desc:  History buffer packet holding information to update
//         the position, pitch & volume of a particular sound.
//---------------------------------------------------------
class CPacketSoundUpdatePitchPosVol : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdatePitchPosVol(s32 soundId, ReplaySound::CUpdateParams &updateParams);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_PITCH_POS_VOL, "Validation of CPacketSoundUpdatePitchPosVol Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PITCH_POS_VOL;
	}

private:
	s32		m_Pitch;
	f32		m_Position[3];
	f32		m_Volume;
};

//////////////////////////////////////////////////////////////////////////
class CPacketSoundUpdateDoppler : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundUpdateDoppler(const float doppler);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_UPDATE_DOPPLER, "Validation of CPacketSoundUpdateDoppler Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_UPDATE_DOPPLER;
	}

private:
	float m_doppler;
};

//////////////////////////////////////////////////////////////////////////
class CPacketSoundSetValueDH : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundSetValueDH(const u32 nameHash, const float value);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SET_VALUE_DH, "Validation of CPacketSoundSetValueDH Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SET_VALUE_DH;
	}

private:
	u32		m_nameHash;
	float	m_value;
};


//////////////////////////////////////////////////////////////////////////
const u8 IsFloatBit = 0;
class CPacketSoundSetClientVar : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundSetClientVar(u32 val);
	CPacketSoundSetClientVar(f32 val);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SET_CLIENT_VAR, "Validation of CPacketSoundSetClientVar Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SET_CLIENT_VAR;
	}

private:
	union
	{
		u32		m_valueU32;
		f32		m_valueF32;
	};
};


//////////////////////////////////////////////////////////////////////////
const u8 ForgetBit = 0;
class CPacketSoundStop : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundStop(bool forget);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool	Setup(bool tracked)
	{
		replayDebugf2("Stop tracking %d", GetTrackedID());
		if(!tracked)
		{
			replayDebugf2("Sound not started so cancelling stop");
			Cancel();
		}
		else
		{
			replayDebugf2("Stop is fine");
		}
		return false;
	}

	bool	ShouldEndTracking() const		{	return true;		}

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_STOP, "Validation of CPacketSoundStop Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_STOP;
	}
};


//////////////////////////////////////////////////////////////////////////
class CPacketSoundStart : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSoundStart();

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const					{ return PREPLAY_SUCCESSFUL; }

	bool	Setup(bool ASSERT_ONLY(tracked))

	{
		replayDebugf2("Start tracking %d", GetTrackedID());
		replayAssertf(!tracked, "Sound is already tracked?");
		return true;
	}

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_START, "Validation of CPacketSoundStart Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_START;
	}
};



//////////////////////////////////////////////////////////////////////////
class CPacketSoundWeaponEcho : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSoundWeaponEcho(const u32 echoHash, const Vector3& position);

	bool			Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_WEAPON_ECHO, "Validation of CPacketSoundWeaponEcho Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_WEAPON_ECHO;
	}

private:
	u32				m_echoSoundHash;
	CPacketVector3	m_position;		

};

//---------------------------------------------------------
// Class: CPacketBreathSound
// Desc:  History buffer packet holding information to
//         play ped breathing sounds
//---------------------------------------------------------
class CPacketBreathSound : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:
	CPacketBreathSound(f32 breathRate);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_BREATH, "Validation of CPacketBreathSound Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_BREATH;
	}

private:
	f32		m_BreathRate;
};

//---------------------------------------------------------
// Class: CPacketAutomaticSound
// Desc:  History buffer packet for sounds that can be
//         recreated easily with a simple function call
//---------------------------------------------------------
class CPacketAutomaticSound : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketAutomaticSound(u32 context, s32 ownerReplayId);

	void			Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_AUTOMATIC, "Validation of CPacketAutomaticSound Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_AUTOMATIC;
	}

private:
	u32		m_Context;
};

//---------------------------------------------------------
// Class: CPacketFireInitSound
// Desc:  Used to recreate fire initialization in a replay
//---------------------------------------------------------
const u8 StartOffsetU8	= 0;
class CPacketFireInitSound : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketFireInitSound(s32 mainFireReplayId, s32 fireStartReplayId, s32 entityOnFireReplayId, audSoundInitParams &initParams);
	
	s32		GetMainFireReplayId()	const	{ return m_MainFireReplayId;  }
	s32		GetFireStartReplayId()	const	{ return m_FireStartReplayId; }

	bool	ShouldInvalidate()		const	{	return GetMainFireReplayId() >= 0 || GetFireStartReplayId() >= 0;}
	void	Invalidate()
	{
		m_MainFireReplayId	= -1;
		m_FireStartReplayId	= -1;
	}

	void	Extract( bool shouldSkipStart = false ) const;

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_FIRE_INIT, "Validation of CPacketFireInitSound Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_FIRE_INIT;
	}

protected:
	
	s32		m_MainFireReplayId;
	s32		m_FireStartReplayId;
	s32		m_EntityOnFireReplayId;
	f32		m_Position[3];
};

//---------------------------------------------------------
// Class: CPacketFireUpdateSound
// Desc:  Updates fires for replays
//---------------------------------------------------------
class CPacketFireUpdateSound : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketFireUpdateSound() : CPacketEventTracked(PACKETID_SOUND_FIRE_UPDATE, sizeof(CPacketFireUpdateSound))	 {}
	~CPacketFireUpdateSound() {}

	s32		GetMainFireReplayId()	const	{ return m_MainFireReplayId;  }
	s32		GetWindFireReplayId()	const	{ return m_WindFireReplayId;  }
	s32		GetCloseFireReplayId()	const	{ return m_CloseFireReplayId; }

	bool	ShouldInvalidate()		const	{ return GetMainFireReplayId() >= 0 || GetWindFireReplayId() >= 0 || GetCloseFireReplayId() >= 0;}
	void	Invalidate()
	{
		m_MainFireReplayId	= -1;
		m_WindFireReplayId	= -1;
		m_CloseFireReplayId	= -1;
	}

	void	Store( s32 mainFireReplayId,	s32 windFireReplayId,	s32 closeFireReplayId,
				   f32 strengthVolume,		f32 windSpeedVolume,	Vector3 &position		);
	void	Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_FIRE_UPDATE, "Validation of CPacketFireUpdateSound Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_FIRE_UPDATE;
	}

protected:
	// NOTE: m_PacketID must start on the 1st byte and
	//       m_GameTime must start on the 5th byte
// 	u8		m_PacketID;
// 	u8		m_Pad[3];

	u32		m_GameTime;
	
	s32		m_MainFireReplayId;
	s32		m_WindFireReplayId;
	s32		m_CloseFireReplayId;

	f32		m_StrengthVolume;
	f32		m_WindSpeedVolume;
	f32		m_Position[3];
};

//---------------------------------------------------------
// Class: CPacketFireVehicleUpdateSound
// Desc:  Updates vehicle fires for replays
//---------------------------------------------------------
class CPacketFireVehicleUpdateSound : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketFireVehicleUpdateSound() : CPacketEventTracked(PACKETID_SOUND_FIRE_VEHICLE_UPDATE, sizeof(CPacketFireVehicleUpdateSound))		 {}
	~CPacketFireVehicleUpdateSound() {}

	s32		GetReplayID()	const	{ return m_VehicleReplayId;	 }
	u32		GetGameTime()			const	{ return m_GameTime;		 }

	bool	ShouldInvalidate()		const	{	return GetReplayID() >= 0;	}
	void	Invalidate()				{ m_VehicleReplayId = -1;	 }

	void	Store( s32 mainFireReplayId,	s32 windFireReplayId,	s32 closeFireReplayId,
				   s32 vehicleReplayId,		s32 startOffset									);
	void	Extract( bool shouldSkipStart = false ) const;

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_FIRE_VEHICLE_UPDATE, "Validation of CPacketFireVehicleUpdateSound Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_FIRE_VEHICLE_UPDATE;
	}

protected:
		
	u32		m_GameTime;
	s32		m_MainFireReplayId;
	s32		m_WindFireReplayId;
	s32		m_CloseFireReplayId;
	s32		m_VehicleReplayId;
};


//////////////////////////////////////////////////////////////////////////
class CPacketVehicleHorn : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleHorn(u32 hornType, s16 hornIndex, f32 holdTIme = 0.96f);

	void	Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_VEHICLE_HORN, "Validation of CPacketVehicleHorn Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_VEHICLE_HORN;
	}

private:

	u32		m_HornType;
	f32		m_HoldTime;
	s16		m_HornIndex;
};


//////////////////////////////////////////////////////////////////////////
class CPacketVehicleHornStop : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketVehicleHornStop();

	void			Extract(const CEventInfo<CVehicle>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CVehicle>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CVehicle>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_VEHICLE_HORN_STOP, "Validation of CPacketVehicleHornStop Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_VEHICLE_HORN_STOP;
	}
};

class CPacketPresuckSound : public CPacketEvent, public CPacketEntityInfoStaticAware<1>
{
public:

	CPacketPresuckSound(const u32 presuckSoundHash, const u32 presuckPreemptTime, const u32 dynamicSceneHash);

	void			Extract( const CEventInfo<CEntity>&) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{	return PRELOAD_SUCCESSFUL;	};
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const;

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const;
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_PRESUCK, "Validation of CPacketPresuckSound Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PRESUCK;
	}

	u32		GetPreplayTime(const CEventInfo<CEntity>&) const	{	return m_PresuckPreemptTime;	}

protected:	
	u32 m_PresuckSoundHash;
	u32 m_PresuckPreemptTime;
	u32 m_DynamicSceneHash;
	mutable audScene* m_AudioScene;
};

class CPacketPlayStreamFromEntity : public CPacketEventTracked, public CPacketEntityInfo<1>
{
public:

	CPacketPlayStreamFromEntity(const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName);

	void			Extract( const CTrackedEventInfo<tTrackedSoundType>&) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const;
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{		return true;	}
	bool		ShouldStartTracking() const						{	return true;	}

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SND_PLAYSTREAMFROMENTITY, "Validation of CPacketPlayStreamFromEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_PLAYSTREAMFROMENTITY;
	}

protected:	
	char m_CurrentStreamName[128];
	u32	m_StartOffset;
	char m_SetName[128];
};

class CPacketPlayStreamFromPosition : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:

	CPacketPlayStreamFromPosition(const Vector3 &pos, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName);

	void			Extract( const CTrackedEventInfo<tTrackedSoundType>&) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const;
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{		return true;	}
	bool		ShouldStartTracking() const						{	return true;	}


	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SND_PLAYSTREAMFROMPOSITION, "Validation of CPacketPlayStreamFromPosition Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_PLAYSTREAMFROMPOSITION;
	}

protected:	
	char m_CurrentStreamName[128];
	u32	m_StartOffset;
	char m_SetName[128];
	CPacketVector<3>	m_Pos;
};

class CPacketPlayStreamFrontend : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:

	CPacketPlayStreamFrontend(const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName);

	void			Extract( const CTrackedEventInfo<tTrackedSoundType>&) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const;
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool		Setup(bool ASSERT_ONLY(/*trackingIDisTracked*/))	{		return true;	}
	bool		ShouldStartTracking() const						{	return true;	}
/*	bool		HasExpired() const;*/

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SND_PLAYSTREAMFRONTED, "Validation of CPacketPlayStreamFrontend Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_PLAYSTREAMFRONTED;
	}

protected:	
	char m_CurrentStreamName[128];
	u32	m_StartOffset;
	char m_SetName[128];
};

class CPacketStopStream : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CPacketStopStream();

	void			Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const								{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const								{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SND_STOPSTREAM, "Validation of CPacketStopStream Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_STOPSTREAM;
	}

protected:	
	char m_CurrentStreamName[128];
	u32	m_StartOffset;
	char m_SetName[128];
};


//=========================================================
// Inline Function Definitions
//=========================================================
inline void ReplaySound::CEnvelopeSoundUpdateSettings::SetRequestedEnvelope( u32 attack, u32 decay, u32 sustain, s32 hold, s32 release )
{
	m_Attack	= attack;
	m_Decay		= decay;
	m_Sustain	= sustain;
	m_Hold		= hold;
	m_Release	= release;
}

inline void ReplaySound::CUpdateParams::SetPrepareAndPlay( bool wasWaveSlotProvided, bool allowLoad, s32 timeLimit )
{
	m_WasWaveSlotProvided	= wasWaveSlotProvided;
	m_AllowLoad				= allowLoad;
	m_TimeLimit				= timeLimit;
}

inline void ReplaySound::CUpdateParams::SetPostSubmixVolumeAttenuation( f32 volumeAttenuation )
{
	m_PostSubmixVolumeAttenuation			= volumeAttenuation;
	m_HasPostSubmixVolumeAttenuationChanged	= true;
}

inline void ReplaySound::CUpdateParams::SetVolume( f32 volume )
{
	m_Volume			= volume;
	m_HasVolumeChanged	= true;
}

inline void ReplaySound::CUpdateParams::SetPosition( Vector3 &position )
{
	m_Position[0]			= position.x;
	m_Position[1]			= position.y;
	m_Position[2]			= position.z;
	m_HasPositionChanged	= true;
}

inline void ReplaySound::CUpdateParams::SetPitch( s32 pitch )
{
	m_Pitch				= pitch;
	m_HasPitchChanged	= true;
}

inline void ReplaySound::CUpdateParams::SetLowPassFilterCutoff( u32 lowPassFilterCutoff )
{
	m_LowPassFilterCutoff			= lowPassFilterCutoff;
	m_HasLowPassFilterCutoffChanged	= true;
}

inline void ReplaySound::CUpdateParams::SetShouldUpdateEntity( bool shouldUpdate )
{
	m_ShouldUpdateEntity			= shouldUpdate;
	m_HasShouldUpdateEntityChanged	= true;
}

inline void ReplaySound::CUpdateParams::SetAllowOrphaned( bool allowOrphaned )
{
	m_AllowOrphaned				= allowOrphaned;
	m_HasAllowOrphanedChanged	= true;
}

inline void	ReplaySound::CUpdateParams::SetClientVariable( u32	value )
{
	m_ClientVariable.u32Val		= value;
	m_ClientVariableType		= static_cast<u8> (CLIENT_VAR_U32);
}

inline void	ReplaySound::CUpdateParams::SetClientVariable( f32	value	)
{
	m_ClientVariable.f32Val		= value;
	m_ClientVariableType		= static_cast<u8> (CLIENT_VAR_F32);
}

// inline void	ReplaySound::CUpdateParams::SetClientVariable( void *value	)
// {
// 	m_ClientVariable.ptrVal		= value;
// 	m_ClientVariableType		= static_cast<u8> (CLIENT_VAR_PTR);
// }

inline Vector3 ReplaySound::CUpdateParams::GetPosition() const
{
	Vector3 pos;
	pos.Set( m_Position[0], m_Position[1], m_Position[2] );
	return pos;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyLowPassFilterCutoffChanged() const
{
	if( m_HasLowPassFilterCutoffChanged &&
		!m_HasPitchChanged				&& !m_HasPositionChanged					&& 
		!m_HasVolumeChanged				&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasShouldUpdateEntityChanged	&& !m_HasAllowOrphanedChanged					)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPitchChanged() const
{
	if( m_HasPitchChanged				&&
		!m_HasPositionChanged			&& !m_HasLowPassFilterCutoffChanged			&& 
		!m_HasVolumeChanged				&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasShouldUpdateEntityChanged	&& !m_HasAllowOrphanedChanged					)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPitchPositionChanged() const
{
	if( m_HasPitchChanged					&& m_HasPositionChanged						&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasVolumeChanged					&& !m_HasAllowOrphanedChanged				&&
		!m_HasShouldUpdateEntityChanged									 					)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPitchPositionVolumeChanged() const
{
	if( m_HasPitchChanged	&& m_HasPositionChanged	&& m_HasVolumeChanged				&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasShouldUpdateEntityChanged		&& !m_HasAllowOrphanedChanged					)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPitchVolumeChanged() const
{
	if( m_HasPitchChanged					&& m_HasVolumeChanged						&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasShouldUpdateEntityChanged		&& !m_HasAllowOrphanedChanged				&&
		!m_HasPositionChanged																)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPositionChanged() const
{
	if( m_HasPositionChanged				&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasShouldUpdateEntityChanged		&& !m_HasAllowOrphanedChanged				&&
		!m_HasPitchChanged					&& !m_HasVolumeChanged							)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPositionVolumeChanged() const
{
	if( m_HasPositionChanged				&& m_HasVolumeChanged						&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPostSubmixVolumeAttenuationChanged	&&
		!m_HasShouldUpdateEntityChanged		&& !m_HasAllowOrphanedChanged				&&
		!m_HasPitchChanged																	)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyPostSubmixVolumeAttenuationChanged() const
{
	if( m_HasPostSubmixVolumeAttenuationChanged								&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPositionChanged		&&
		!m_HasShouldUpdateEntityChanged		&& !m_HasAllowOrphanedChanged	&&
		!m_HasPitchChanged					&& !m_HasVolumeChanged				)
	{
		return true;
	}

	return false;
}

inline bool	ReplaySound::CUpdateParams::GetHasOnlyVolumeChanged() const
{
	if( m_HasVolumeChanged 					&&
		!m_HasLowPassFilterCutoffChanged	&& !m_HasPositionChanged					&&
		!m_HasShouldUpdateEntityChanged		&& !m_HasAllowOrphanedChanged				&&
		!m_HasPitchChanged					&& !m_HasPostSubmixVolumeAttenuationChanged		)
	{
		return true;
	}

	return false;
}

inline Vector3 CPacketSoundUpdateSettings::GetPosition() const
{
	Vector3 pos;
	pos.Set( m_Position[0], m_Position[1], m_Position[2] );
	return pos;
}


#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_SOUNDPACKET_H_
