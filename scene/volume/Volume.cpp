#include "Volume.h"

#if VOLUME_SUPPORT

#include "VolumeAggregate.h"

INSTANTIATE_RTTI_CLASS(CVolume,0xAA811A6A)

CVolume::~CVolume()
{
    if (m_Owner)
    {
        m_Owner->Remove(this);
    }
}

#endif // VOLUME_SUPPORT