#if HAVE_CONFIG_H
#include "../config.h"
#endif

#include "canvas_audio.h"

#include <math.h>
#include <stdlib.h>
#include <vorbis/vorbisfile.h>
#include "canvas_lib.h"
#include "canvas_vram.h"

#define __maybe_unused __attribute__((unused))

#define BURRO_PROP_MEDIA_ROLE "game"
#define BURRO_PROP_APPLICATION_ID "com.lonelycactus.burroengine"
#define BURRO_PROP_APPLICATION_NAME "BurroEngine"

/* These functions are used to pass compressed audio from VRAM to Ogg Vorbis. */
static const ov_callbacks ov_cb = {
    vram_audio_read,
    vram_audio_seek,
    vram_audio_close,
    vram_audio_tell
};


/* These buffers hold uncompressed audio queued up to sent to
 * Pulseaudio. */
#define MICROSECONDS_PER_MILLISECOND (1000)

#define AUDIO_LATENCY_REQUESTED_IN_MILLISECONDS (100)

#define AM_CHANNELS_NUM 4
#define AM_OV_READ_BUFFER_SIZE 4096

#define AM_CHANNEL_TYPE_NONE (0)
#define AM_CHANNEL_TYPE_OGV (1)

typedef struct audio_channel_tag {
    int type;                   /* none or audio */
    int vram;                   /* if type = audio, VRAM_A etc */
    gboolean playing;                 /* play, pause */
    float volume;
    
    vram_io_context_t *io;
    OggVorbis_File vf;

    long size;
    float buffer[AM_BUFFER_SIZE];
} audio_channel_t;

typedef struct audio_model_tag {
    gboolean playing;
    float volume;
    
    long size;
    float buffer[AM_BUFFER_SIZE];
    
    char ov_read_buffer[AM_OV_READ_BUFFER_SIZE];
    
    audio_channel_t channels[AM_CHANNELS_NUM];
} audio_model_t;

static audio_model_t am;

float *
am_buffer()
{
    return am.buffer;
}

void
am_default_volume()
{
    am.volume = 1.0;
    am.playing = TRUE;
    for (int i = 0; i < 4; i ++)
        am.channels[i].volume = 1.0;
}
bool
am_is_playing()
{
    gboolean no_audio;
    int c;

    /* Are we playing any audio right now? */
    no_audio = TRUE;
    if (am.playing)
    {
        c = 0;
        while (c < AM_CHANNELS_NUM)
        {
            if (am.channels[c].playing)
            {
                no_audio = FALSE;
                break;
            }
            c++;
        }
    }
    return !no_audio;
}

static float fetch_audio_buf[AM_OV_READ_BUFFER_SIZE];

static long
resample (float *dest, int dest_len, float *src, int n_input, int rate)
{
    float time_seconds = (float) n_input / (float) rate;
    int n_output = time_seconds * AM_SAMPLE_RATE_IN_HZ;

    if (n_output < n_input / 2)
    {
        // If we have to do a lot of downsampling, we need to
        // low-pass-filter the input to lessen aliasing effects.
        float filter_weight = (float) (n_input - n_output) / (float) n_input;
        for (int j = 1; j < n_input; j ++)
        {
            int16_t cur = src[j];
            int16_t prev = src[j-1];
            src[j] = (filter_weight * prev
                      + (1.0 - filter_weight) * cur);
        }
    }
    
    if (n_output > dest_len)
        n_output = dest_len;
    for (int i = 0; i < n_output; i ++)
    {
        float time_cur;
        float input_sample;
        float frac_part, int_part;
        long j1, j2;

        time_cur = (float) i / AM_SAMPLE_RATE_IN_HZ;
        // Figure out if we are interpolating, or if we happen to be
        // right on an input sample.
        input_sample = time_cur * rate;
        frac_part = modff(input_sample, &int_part);
        j1 = lrintf(int_part);
        j2 = j1 + 1;
        if (j2 >= n_input)
            j2 --;
        
        if (frac_part < 0.001 || j1 == j2)
            dest[i] = src[j1];
        else if (frac_part > 0.999)
            dest[i] = src[j2];
        else
            dest[i] = src[j1] + frac_part * (src[j2] - src[j1]);
    }
    return n_output;
}


static void
am_fetch_audio (int chan_num, int samples_requested)
{
    audio_channel_t *ac = &am.channels[chan_num];
    while (ac->playing
           && (ac->type == AM_CHANNEL_TYPE_OGV)
           && ac->size < samples_requested)
    {
        float **pcm;
        int section;
        long samples_read = ov_read_float (&ac->vf,
                                           &pcm,
                                           AM_OV_READ_BUFFER_SIZE,
                                           &section);
        if (samples_read <= 0)
            return;

        /* Mix down all the channels into mono. */
        int channels = ov_info(&ac->vf, -1)->channels;
        memset (fetch_audio_buf, 0, AM_OV_READ_BUFFER_SIZE * sizeof (float));
        for (int c = 0; c < channels; c ++)
            for (int i = 0; i < samples_read; i ++)
                fetch_audio_buf[i] += pcm[c][i];

        /* Resample to the current rate */
        long samples_interpolated = resample (ac->buffer + ac->size,
                                          AM_BUFFER_SIZE - ac->size,
                                          fetch_audio_buf,
                                          samples_read,
                                          ov_info(&ac->vf, -1)->rate);
        ac->size += samples_interpolated;
    }
}


void
am_update(int n)
{
    int c;

    for (int i = 0; i < n; i ++)
        am.buffer[i] = 0.0f;
    am.size = n;

    /* Are we playing any audio right now? */
    if (!am_is_playing())
        return;

    /* Mix down, fetching new data if necessary. */
    for (c = 0; c < AM_CHANNELS_NUM; c ++)
    {
        if (am.channels[c].playing)
        {
            am_fetch_audio (c, n);
            int count = MIN(n, am.channels[c].size);
            for (int i = 0; i < count; i ++)
                am.buffer[i] += (am.channels[c].volume
                                 * am.channels[c].buffer[i]);
        }
    }
    for (int i = 0; i < n; i ++)
        am.buffer[i] *= am.volume;

    /* Dequeue audio from channels */
    for (c = 0; c < AM_CHANNELS_NUM; c ++)
    {
        if (am.channels[c].playing)
        {
            /* If we're out of data, we're done. */
            if (!am.channels[c].size)
            {
                if (am.channels[c].type == AM_CHANNEL_TYPE_OGV)
                {
                    ov_clear (&am.channels[c].vf);
                    g_free (am.channels[c].io);
                    am.channels[c].io = NULL;
                }
                am.channels[c].type = AM_CHANNEL_TYPE_NONE;
                am.channels[c].playing = FALSE;
            }
            
            if (am.channels[c].size < n)
            {
                memset (am.channels[c].buffer, 0, sizeof (am.channels[c].buffer));
                am.channels[c].size = 0;
            }
            else
            {
                for (int i = 0; i < am.channels[c].size - n; i ++)
                    am.channels[c].buffer[i] = am.channels[c].buffer[i + n];
                am.channels[c].size -= n;
                for (unsigned i = am.channels[c].size; i < AM_BUFFER_SIZE; i ++)
                    am.channels[c].buffer[i] = 0.0;
            }
        }
    }
}


#if !defined(HAVE_LIBPULSE) && !defined(HAVE_COREAUDIO)
void canvas_audio_init()
{
}

/* This finalizer is called if we are shutting down cleanly */
void canvas_audio_fini()
{
}

int
canvas_audio_iterate()
{
    return 0;
}

#endif 

SCM_DEFINE(G_audio_channel_play, "audio-channel-play", 2, 0, 0, (SCM s_channel, SCM s_vram), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(s_channel, 1, 4) - 1;
    int vram = scm_to_signed_integer (s_vram, 1, 10);
    if (am.channels[c].playing)
    {
        ov_clear(&am.channels[c].vf);
        free(am.channels[c].io);
        am.channels[c].playing = FALSE;
        am.channels[c].type = AM_CHANNEL_TYPE_NONE;
        am.channels[c].size = 0;
        memset(am.channels[c].buffer, 0, AM_BUFFER_SIZE * sizeof(float));
    }
    if (vram_get_type(vram) != VRAM_TYPE_AUDIO)
    {
        g_warning ("VRAM is not an audio file");
        return SCM_BOOL_F;
    }
    am.channels[c].io = vram_audio_open (vram);
    ov_open_callbacks (am.channels[c].io, &(am.channels[c].vf), NULL, 0, ov_cb);
    am.channels[c].playing = TRUE;
    am.channels[c].type = AM_CHANNEL_TYPE_OGV;
    return SCM_BOOL_T;
}

SCM_DEFINE(G_audio_channel_pause, "audio-channel-pause", 1, 0, 0, (SCM channel), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(channel, 1, 4) - 1;
    am.channels[c].playing = FALSE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_audio_channel_unpause, "audio-channel-unpause", 1, 0, 0, (SCM channel), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(channel, 1, 4) - 1;
    if (am.channels[c].type == AM_CHANNEL_TYPE_OGV)
        am.channels[c].playing = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_audio_channel_stop, "audio-channel-stop", 1, 0, 0, (SCM channel), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(channel, 1, 4) - 1;
    if (am.channels[c].playing)
    {
        ov_clear(&am.channels[c].vf);
        free(am.channels[c].io);
        am.channels[c].playing = FALSE;
        am.channels[c].type = AM_CHANNEL_TYPE_NONE;
        am.channels[c].size = 0;
        memset(am.channels[c].buffer, 0, AM_BUFFER_SIZE * sizeof(float));
    }
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_audio_channel_playing_p, "audio-channel-playing?", 1, 0, 0, (SCM channel), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(channel, 1, 4) - 1;
    return scm_from_bool (am.channels[c].playing);
}


SCM_DEFINE(G_audio_channel_get_volume, "audio-channel-get-volume", 1, 0, 0, (SCM channel), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(channel, 1, 4) - 1;
    return scm_from_double (am.channels[c].volume);
}

SCM_DEFINE(G_audio_channel_set_volume_x, "audio-channel-set-volume", 2, 0, 0, (SCM channel, SCM volume), "\
Start playing audio from a given VRAM bank on a given channel")
{
    int c = scm_to_signed_integer(channel, 1, 4) - 1;
    float v = scm_to_double (volume);
    am.channels[c].volume = v;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_audio_pause, "audio-pause", 0, 0, 0, (void), "\
Pause all audio.")
{
    am.playing = FALSE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_audio_unpause, "audio-unpause", 0, 0, 0, (void), "\
Unause all audio.")
{
    am.playing = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_audio_set_volume, "audio-set-volume", 1, 0, 0, (SCM volume), "\
Set the main volume for the audio.\n")
{
    float v = scm_to_double(volume);
    am.volume = v;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_audio_get_volume, "audio-get-volume", 0, 0, 0, (), "\
Set the main volume for the audio.\n")
{
    return scm_from_double(am.volume);
}

void
canvas_audio_init_guile_procedures()
{
#ifndef SCM_MAGIC_SNARFER
#include "canvas_audio.x"
#endif
    scm_c_export ("audio-channel-play",
                  "audio-channel-pause",
                  "audio-channel-unpause",
                  "audio-channel-stop",
                  "audio-channel-playing?",
                  "audio-channel-set-volume",
                  "audio-channel-get-volume",
                  "audio-pause",
                  "audio-unpause",
                  "audio-set-volume",
                  "audio-get-volume",
                  NULL);
                  
}

/*
  Local Variables:
  mode:C
  c-file-style:"linux"
  tab-width:4
  c-basic-offset: 4
  indent-tabs-mode:nil
  End:
*/


