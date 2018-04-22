#ifndef CANVAS_AUDIO_H
#define CANVAS_AUDIO_H

#include "visibility.h"
#include <stdbool.h>
#define AM_SAMPLE_RATE_IN_HZ (48000u)

#define AM_BUFFER_DURATION_IN_MILLISECONDS (2000u)
#define AM_BUFFER_SIZE ((AM_SAMPLE_RATE_IN_HZ * AM_BUFFER_DURATION_IN_MILLISECONDS) / 1000)

DLL_LOCAL void canvas_audio_init ();
DLL_LOCAL void canvas_audio_fini ();
DLL_PUBLIC int canvas_audio_iterate();
DLL_LOCAL void canvas_audio_init_guile_procedures ();
bool am_is_playing();
void am_update(int n);
float *am_buffer();
void am_default_volume();

#endif

/*
  Local Variables:
  mode:C
  c-file-style:"linux"
  tab-width:4
  c-basic-offset: 4
  indent-tabs-mode:nil
  End:
*/


