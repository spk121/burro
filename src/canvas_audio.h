#ifndef CANVAS_AUDIO_H
#define CANVAS_AUDIO_H

#include "visibility.h"
DLL_LOCAL void canvas_audio_init ();
DLL_LOCAL void canvas_audio_fini ();
DLL_PUBLIC int canvas_audio_iterate();
DLL_LOCAL void canvas_audio_init_guile_procedures ();

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


