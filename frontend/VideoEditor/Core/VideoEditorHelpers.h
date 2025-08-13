/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorHelpers.h
// PURPOSE : Helper classes used to support the video editor
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#ifndef VIDEO_EDITOR_HELPERS_H_
#define VIDEO_EDITOR_HELPERS_H_

// rage
#include "file/limits.h"
#include "math/vecmath.h"

// game
#include "renderer/color.h"

class CMontage;
class CMontageMusicClip;
class CMontageText;
class CVideoEditorProject;

//! Base for other handles
class CMontageElementHandleBase
{
public: // methods
	virtual ~CMontageElementHandleBase() {};

	// Check if the handle is still valid
	virtual bool IsValid() const { return m_target != NULL; }

	//! Release this handle. Does not destroy underlying object.
	inline void Release() { m_target = NULL; m_index = -1; }

	virtual s32 GetElementCount() const = 0;

	//! Index of the underlying object we represent
	inline s32 GetIndex() const { return m_index; }

	//! Delete the underlying object and release the handle.
	void Delete();

	//! Increment to the next element. Returns true if we wrapped around
	bool Next();

	//! Decrement to the previous element. Returns true if we wrapped around
	bool Previous();

protected: // declarations & variables
	CMontage* 	m_target;
	s32 		m_index;

protected: // methods
	friend class CVideoEditorProject;

	CMontageElementHandleBase();

	CMontageElementHandleBase( CMontage* target, s32 const index );
	void Initialize( CMontage* target, s32 const index );
};

//! Helper to abstract editing overlay text
class CTextOverlayHandle : public CMontageElementHandleBase
{
public: // methods
	// Check if the handle is still valid
	virtual bool IsValid() const;
		
	virtual s32 GetElementCount() const;

	bool SetLine( u32 const lineIndex, char const * const text, 
		Vector2 const& position, Vector2 const& scale, CRGBA const& color, s32 const style );
	void ResetLine( u32 const lineIndex );

	u32 GetMaxLineCount() const;

	//! Accessors/Mutators for underlying functionality.
	u32 getStartTimeMs() const;
	void setStartTimeMs( u32 const timeMs );

	u32 getDurationMs() const;
	void setDurationMs( u32 const durationMs );

	char const * getText( u32 const lineIndex ) const;
	void setText( u32 const lineIndex, char const * const text );

	u32 getTextStyle( u32 const lineIndex ) const;
	void setTextStyle( u32 const lineIndex, u32 const style );

	Vector2 getTextPosition( u32 const lineIndex ) const;
	void setTextPosition( u32 const lineIndex, Vector2 const& position );

	Vector2 getTextScale( u32 const lineIndex ) const;
	void setTextScale( u32 const lineIndex, Vector2 const& scale );

	CRGBA getTextColor( u32 const lineIndex ) const;
	void setTextColor( u32 const lineIndex, CRGBA const& color );

private: // methods
	CMontageText* GetTargetText();
	CMontageText const * GetTargetText() const;
};

//! Helper to abstract editing music clips
class CMusicClipHandle : public CMontageElementHandleBase
{
public: // methods
	// Check if the handle is still valid
	virtual bool IsValid() const;

	virtual s32 GetElementCount() const;

	u32 getClipSoundHash() const;
	void setClipSoundHash( u32 const soundHash );

	s32 getStartTimeMs() const;
	void setStartTimeMs( s32 const startTime );

	u32 getTrackTotalDurationMs() const;
	
	void setTrimmedDurationMs( u32 const duration );
	u32 getTrimmedDurationMs() const;

	u32 getMaxDurationMs() const;

	u32 getStartOffsetMs() const;
	void setStartOffsetMs( u32 const startOffset );

	u32 getStartTimeWithOffsetMs() const;

	bool VerifyTimingValues() const;

private: // methods
	CMontageMusicClip* GetTargetMusicClip();
	CMontageMusicClip const * GetTargetMusicClip() const;
};

//! Helper to abstract editing music clips
class CAmbientClipHandle : public CMontageElementHandleBase
{
public: // methods
	// Check if the handle is still valid
	virtual bool IsValid() const;

	virtual s32 GetElementCount() const;

	u32 getClipSoundHash() const;
	void setClipSoundHash( u32 const soundHash );

	s32 getStartTimeMs() const;
	void setStartTimeMs( s32 const startTime );

	u32 getTrackTotalDurationMs() const;

	void setTrimmedDurationMs( u32 const duration );
	u32 getTrimmedDurationMs() const;

	u32 getMaxDurationMs() const;

	u32 getStartOffsetMs() const;
	void setStartOffsetMs( u32 const startOffset );

	u32 getStartTimeWithOffsetMs() const;

	bool VerifyTimingValues() const;

private: // methods
	CMontageMusicClip* GetTargetMusicClip();
	CMontageMusicClip const * GetTargetMusicClip() const;

};


#endif // VIDEO_EDITOR_HELPERS_H_

#endif // GTA_REPLAY
