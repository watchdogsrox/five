/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorHelpers.cpp
// PURPOSE : Helper classes used to support the video editor
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoEditorHelpers.h"

#if GTA_REPLAY

// game
#include "frontend/ui_channel.h"
#include "replaycoordinator/storage/Montage.h"
#include "replaycoordinator/storage/MontageMusicClip.h"
#include "replaycoordinator/storage/MontageText.h"

FRONTEND_OPTIMISATIONS();

/////////////////////////////////////////////////////////////////////////////////
// Handle Base
void CMontageElementHandleBase::Delete()
{
	if( IsValid() )
	{
		m_target->DeleteText( m_index );
	}

	Release();
}

CMontageElementHandleBase::CMontageElementHandleBase()
	: m_target( NULL )
	, m_index( -1 )
{

}

CMontageElementHandleBase::CMontageElementHandleBase( CMontage* target, s32 const index )
	: m_target( target )
	, m_index( index )
{
}

void CMontageElementHandleBase::Initialize( CMontage* target, s32 const index )
{
	m_target = target;
	m_index = index;
}

bool CMontageElementHandleBase::Next()
{
	s32 nextIndex = m_index + 1;
	bool const shouldWrap = nextIndex >= GetElementCount();
	nextIndex = shouldWrap ? 0 : nextIndex;
	return shouldWrap;
}

bool CMontageElementHandleBase::Previous()
{
	s32 nextIndex = m_index - 1;
	bool const shouldWrap = nextIndex < 0;
	nextIndex = shouldWrap ? GetElementCount() - 1 : nextIndex;
	return shouldWrap;
}

bool CTextOverlayHandle::IsValid() const
{
	return CMontageElementHandleBase::IsValid() && m_target->IsValidTextIndex( m_index );
}

s32 CTextOverlayHandle::GetElementCount() const
{
	return IsValid() ? (s32)m_target->GetTextCount() : 0;
}

bool CTextOverlayHandle::SetLine( u32 const lineIndex, char const * const text, Vector2 const& position, Vector2 const& scale, 
								 CRGBA const& color, s32 const style )
{
	return IsValid() ? GetTargetText()->SetLine( lineIndex, text, position, scale, color, style ) : false;
}

void CTextOverlayHandle::ResetLine( u32 const index )
{
	if( IsValid() )
	{
		GetTargetText()->ResetLine( index );
	}
}

u32 CTextOverlayHandle::GetMaxLineCount() const
{
	return IsValid() ? GetTargetText()->GetMaxLineCount() : 0;
}

u32 CTextOverlayHandle::getStartTimeMs() const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getStartTimeMs - Invalid handle" ) ? GetTargetText()->getStartTimeMs() : 0;
}

void CTextOverlayHandle::setStartTimeMs( u32 const timeMs )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::getStartTimeMs - Invalid handle" ) )
	{
		GetTargetText()->setStartTimeMs( timeMs );	
	}
}

u32 CTextOverlayHandle::getDurationMs() const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getDurationMs - Invalid handle" ) ? GetTargetText()->getDurationMs() : 0;
}

void CTextOverlayHandle::setDurationMs( u32 const durationMs )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::setDurationMs - Invalid handle" ) )
	{
		GetTargetText()->setDurationMs( durationMs );	
	}
}

char const * CTextOverlayHandle::getText( u32 const lineIndex ) const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getText - Invalid handle" ) ? GetTargetText()->getText( lineIndex ) : NULL;
}

void CTextOverlayHandle::setText( u32 const lineIndex, char const * const text )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::setText - Invalid handle" ) )
	{
		GetTargetText()->setText( lineIndex, text );	
	}
}

u32 CTextOverlayHandle::getTextStyle( u32 const lineIndex ) const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getTextStyle - Invalid handle" ) ? GetTargetText()->getTextStyle( lineIndex ) : 0;
}

void CTextOverlayHandle::setTextStyle( u32 const lineIndex, u32 const style )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::setTextStyle - Invalid handle" ) )
	{
		GetTargetText()->setTextStyle( lineIndex, style );	
	}
}

rage::Vector2 CTextOverlayHandle::getTextPosition( u32 const lineIndex ) const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getTextPosition - Invalid handle" ) ? GetTargetText()->getTextPosition( lineIndex ) : rage::Vector2( 0.f, 0.f );
}

void CTextOverlayHandle::setTextPosition( u32 const lineIndex, Vector2 const& position )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::setTextPosition - Invalid handle" ) )
	{
		GetTargetText()->setTextPosition( lineIndex, position );	
	}
}

rage::Vector2 CTextOverlayHandle::getTextScale( u32 const lineIndex ) const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getTextScale - Invalid handle" ) ? GetTargetText()->getTextScale( lineIndex ) : rage::Vector2( 1.f, 1.f );
}

void CTextOverlayHandle::setTextScale( u32 const lineIndex, Vector2 const& scale )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::setTextScale - Invalid handle" ) )
	{
		GetTargetText()->setTextScale( lineIndex, scale );	
	}
}

CRGBA CTextOverlayHandle::getTextColor( u32 const lineIndex ) const
{
	return uiVerifyf( IsValid(), "CTextOverlayHandle::getTextColor - Invalid handle" ) ? GetTargetText()->getTextColor( lineIndex ) : CRGBA( 0, 0, 0, 255 );
}

void CTextOverlayHandle::setTextColor( u32 const lineIndex, CRGBA const& color )
{
	if( uiVerifyf( IsValid(), "CTextOverlayHandle::setTextColor - Invalid handle" ) )
	{
		GetTargetText()->setTextColor( lineIndex, color );	
	}
}

CMontageText* CTextOverlayHandle::GetTargetText()
{
	return IsValid() ? m_target->GetText( m_index ) : NULL;
}

CMontageText const * CTextOverlayHandle::GetTargetText() const
{
	return IsValid() ? m_target->GetText( m_index ) : NULL;
}

bool CMusicClipHandle::IsValid() const
{
	return CMontageElementHandleBase::IsValid() && m_target->IsValidMusicClipIndex( m_index );
}

s32 CMusicClipHandle::GetElementCount() const
{
	return IsValid() ? m_target->GetMusicClipCount() : 0;
}

u32 CMusicClipHandle::getClipSoundHash() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getSoundHash() : 0;
}

void CMusicClipHandle::setClipSoundHash( u32 const soundHash )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setSoundHash( soundHash );
	}
}

s32 CMusicClipHandle::getStartTimeMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getStartTimeMs() : 0;
}

void CMusicClipHandle::setStartTimeMs( s32 const startTime )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setStartTimeMs( startTime );
	}
}

u32 CMusicClipHandle::getTrackTotalDurationMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getTrackTotalDurationMs() : 0;
}

void CMusicClipHandle::setTrimmedDurationMs( u32 const duration )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setTrimmedDurationMs( duration );
	}
}

u32 CMusicClipHandle::getTrimmedDurationMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getTrimmedDurationMs() : 0;
}

u32 CMusicClipHandle::getMaxDurationMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? CReplayAudioTrackProvider::GetMusicTrackDurationMs( clip->getSoundHash() ) : 0;
}

u32 CMusicClipHandle::getStartOffsetMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getStartOffsetMs() : 0;
}

void CMusicClipHandle::setStartOffsetMs( u32 const startOffset )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setStartOffsetMs( startOffset );
	}
}

u32 CMusicClipHandle::getStartTimeWithOffsetMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();

	if (clip)
	{
		s32 const c_startTimeWithOffsetMs = clip->getStartTimeMs() + clip->getStartOffsetMs();

		if (c_startTimeWithOffsetMs > 0)
		{
			return c_startTimeWithOffsetMs;
		}
	}

	return 0;
}

bool CMusicClipHandle::VerifyTimingValues() const
{
	const CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		if (Verifyf(clip->getTrimmedDurationMs() <= clip->getTrackTotalDurationMs()-clip->getStartOffsetMs() , "duration exceeds total track time! (%d + %d > %d)", clip->getStartOffsetMs(), clip->getTrimmedDurationMs(), clip->getTrackTotalDurationMs()))
		{
			return true;
		}
	}

	return false;
}

CMontageMusicClip* CMusicClipHandle::GetTargetMusicClip()
{
	return IsValid() ? m_target->GetMusicClip( m_index ) : NULL;
}

CMontageMusicClip const * CMusicClipHandle::GetTargetMusicClip() const
{
	return IsValid() ? m_target->GetMusicClip( m_index ) : NULL;
}

// Ambient Helpers

bool CAmbientClipHandle::IsValid() const
{
	return CMontageElementHandleBase::IsValid() && m_target->IsValidAmbientClipIndex( m_index );
}

s32 CAmbientClipHandle::GetElementCount() const
{
	return IsValid() ? m_target->GetAmbientClipCount() : 0;
}

u32 CAmbientClipHandle::getClipSoundHash() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getSoundHash() : 0;
}

void CAmbientClipHandle::setClipSoundHash( u32 const soundHash )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setSoundHash( soundHash );
	}
}

s32 CAmbientClipHandle::getStartTimeMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getStartTimeMs() : 0;
}

void CAmbientClipHandle::setStartTimeMs( s32 const startTime )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setStartTimeMs( startTime );
	}
}

u32 CAmbientClipHandle::getTrackTotalDurationMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getTrackTotalDurationMs() : 0;
}

void CAmbientClipHandle::setTrimmedDurationMs( u32 const duration )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setTrimmedDurationMs( duration );
	}
}

u32 CAmbientClipHandle::getTrimmedDurationMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getTrimmedDurationMs() : 0;
}

u32 CAmbientClipHandle::getMaxDurationMs() const
{
	return (u32)MAX_AMBIENT_DURATION_MS;
}

u32 CAmbientClipHandle::getStartOffsetMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();
	return clip ? clip->getStartOffsetMs() : 0;
}

void CAmbientClipHandle::setStartOffsetMs( u32 const startOffset )
{
	CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		clip->setStartOffsetMs( startOffset );
	}
}

u32 CAmbientClipHandle::getStartTimeWithOffsetMs() const
{
	CMontageMusicClip const* clip = GetTargetMusicClip();

	if (clip)
	{
		s32 const c_startTimeWithOffsetMs = clip->getStartTimeMs() + clip->getStartOffsetMs();

		if (c_startTimeWithOffsetMs > 0)
		{
			return c_startTimeWithOffsetMs;
		}
	}

	return 0;
}

bool CAmbientClipHandle::VerifyTimingValues() const
{
	const CMontageMusicClip* clip = GetTargetMusicClip();
	if( clip )
	{
		if (Verifyf(clip->getTrimmedDurationMs() <= clip->getTrackTotalDurationMs()-clip->getStartOffsetMs() , "duration exceeds total track time! (%d + %d > %d)", clip->getStartOffsetMs(), clip->getTrimmedDurationMs(), clip->getTrackTotalDurationMs()))
		{
			return true;
		}
	}

	return false;
}

CMontageMusicClip* CAmbientClipHandle::GetTargetMusicClip()
{
	return IsValid() ? m_target->GetAmbientClip( m_index ) : NULL;
}

CMontageMusicClip const * CAmbientClipHandle::GetTargetMusicClip() const
{
	return IsValid() ? m_target->GetAmbientClip( m_index ) : NULL;
}

#endif // GTA_REPLAY
