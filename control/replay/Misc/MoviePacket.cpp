#include "MoviePacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayMovieController.h"
#include "control/replay/ReplayMovieControllerNew.h"

//////////////////////////////////////////////////////////////////////////
// Expects an array of ReplayMovieInfo[MAX_ACTIVE_MOVIES]
CPacketMovie::CPacketMovie(	atFixedArray<ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE>, MAX_ACTIVE_MOVIES> &info )
	: CPacketEvent(PACKETID_MOVIE, sizeof(CPacketMovie), 1)
{
	m_MovieInfo = info;
}

//////////////////////////////////////////////////////////////////////////
void CPacketMovie::Extract(const CEventInfo<void>&) const
{
	ReplayMovieController::SubmitMoviesThisFrame(m_MovieInfo);
}


//////////////////////////////////////////////////////////////////////////
// New version that takes longer filenames
CPacketMovie2::CPacketMovie2(	atFixedArray<ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE_64>, MAX_ACTIVE_MOVIES> &info )
	: CPacketEvent(PACKETID_MOVIE2, sizeof(CPacketMovie2))
{
	m_MovieInfo = info;
}

//////////////////////////////////////////////////////////////////////////
void CPacketMovie2::Extract(const CEventInfo<void>&) const
{
	ReplayMovieControllerNew::SubmitMoviesThisFrame(m_MovieInfo);
}


//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketMovieEntity_V1 = 1;	// Handles movie name by hash
CPacketMovieEntity::CPacketMovieEntity(const char* movieName, bool frontendAudio, const CEntity *pEntity)
	: CPacketEvent(PACKETID_MOVIE_ENTITY, sizeof(CPacketMovieEntity), CPacketMovieEntity_V1)
{
	strncpy(m_MovieName, movieName, MAX_REPLAY_MOVIE_NAME_SIZE);
	m_FrontendAudio = frontendAudio;

	m_movieHash = atStringHash(m_MovieName);

	m_ModelNameHash = 0;
	if( pEntity )
	{
		CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
		if( pModelInfo )
		{
			m_ModelNameHash = pModelInfo->GetModelNameHash();
			Vec3V pos = pEntity->GetTransform().GetPosition();
			m_EntityPos.Store(pos);
		}
	}
}

void CPacketMovieEntity::Extract(const CEventInfo<void>&) const
{
	if( m_ModelNameHash != 0 )
	{
		Vector3 entityPos;
		m_EntityPos.Load(entityPos);
		if(GetPacketVersion() >= CPacketMovieEntity_V1)
			ReplayMovieControllerNew::RegisterMovieAudioLink(m_ModelNameHash, entityPos, m_movieHash, m_FrontendAudio );
		else
			ReplayMovieController::RegisterMovieAudioLink(m_ModelNameHash, entityPos, atStringHash(m_MovieName), m_FrontendAudio );
	}
}


#endif	//GTA_REPLAY
