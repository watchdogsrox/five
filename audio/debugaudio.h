#ifndef DEBUGAUDIO_H_INCLUDED
#define DEBUGAUDIO_H_INCLUDED
 

// Set the following to 1 to disable optimisations and enable various audio debug features across the code base
// NOTE: ensure this file is never checked in with debug turned on
#define AUD_DEBUG 0

#if AUD_DEBUG
OPTIMISATIONS_OFF()
#endif

#endif // DEBUGAUDIO_H_INCLUDED
