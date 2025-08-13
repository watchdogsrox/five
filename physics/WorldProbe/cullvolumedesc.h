#ifndef CULL_VOLUME_DESC_H
#define CULL_VOLUME_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/wpcommon.h"

namespace WorldProbe
{

	enum eCullVolumeType
	{
		INVALID_CULL_VOLUME_TYPE = -1,
		CULL_VOLUME_CAPSULE = 0,
		CULL_VOLUME_SPHERE,
		CULL_VOLUME_BOX
	};

	/////////////////////////////////////////////////////////////////////
	// Base descriptor class for the various types of cull volumes which
	// can be specified for a batch test.
	/////////////////////////////////////////////////////////////////////
	class CCullVolumeDesc
	{
	protected:
		CCullVolumeDesc()
			: m_eCullVolumeType(INVALID_CULL_VOLUME_TYPE)
		{
		}
		virtual ~CCullVolumeDesc()
		{
		}

	public:
		// Enum used to identify what type of shape test this is.
		eCullVolumeType GetTestType() const { return m_eCullVolumeType; }

	protected:
		eCullVolumeType m_eCullVolumeType;
	};

} // namespace WorldProbe

#endif // CULL_VOLUME_DESC_H
