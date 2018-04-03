#include "burro_canvas_audio.h"

#include <stdlib.h>
#include "x/xpulseaudio.h"
#define USE_GLIB_MAINLOOP

#ifdef USE_GLIB_MAINLOOP
#include <pulse/glib-mainloop.h>
#endif

#define BURRO_PROP_MEDIA_ROLE "game"
#define BURRO_PROP_APPLICATION_ID "com.lonelycactus.burroengine"
#define BURRO_PROP_APPLICATION_NAME "BurroEngine"

typedef struct pulse_priv_tag {
    pa_context_state_t state;
    pa_context *context;
#ifdef USE_GLIB_MAINLOOP
    pa_glib_mainloop *loop;
#else
    pa_mainloop *loop;
    //pa_threaded_mainloop *loop;
#endif
    gboolean finalize;
    unsigned samples_written;
} pulse_priv_t;
static pulse_priv_t pulse;

static void cb_audio_context_state(pa_context *c, void *userdata);
static void cb_audio_stream_started(pa_stream *p,
                                    void *userdata);
static void cb_audio_stream_write(pa_stream *p, size_t nbytes, void *userdata);


/*-----------------------------------------------------------------------------
  audio_model.c
  Copyright (C) 2014, 2015, 2018
  Michael L. Gran (spk121)

  GPL3+
  -----------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <math.h>
#include <stdalign.h>
#include <string.h>
// #include "../x.h"
// #include "loop.h"
// #include "rand.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wconversion"

/** A Guile SCM that holds the definition of the Guile instrument
    foreign object type. */
SCM G_instrument_tag;

/** Holds a list of sounds to be played.  Sounds are pairs where CAR
    is the start time and CDR is a native-endian, signed, 16-bit bytevector
    that holds a waveform. */
SCM_GLOBAL_VARIABLE_INIT (G_sound_playlist_var, "%sound-playlist", SCM_EOL);

/** Holds the information of the audio model */
static alignas(16) float am_working[AUDIO_BUFFER_SIZE];
static alignas(16) int16_t am_sum[AUDIO_BUFFER_SIZE];
static double am_timer_delta = 0.0;
static double am_timer = 0.0;

SCM_DEFINE (G_list_to_instrument, "list->instrument", 1, 0, 0,
            (SCM lst), "\
Given a 17-element list, this returns an instrument model.\n\
The contents of the list are\n\
attack duration, decay duration, release duration,\n\
initial freq adj, attack freq adj, sustain freq adj, release freq adj,\n\
attack amplitude adj, sustain ampl adj,\n\
noise flag, duty ratio,\n\
0th harmonic ampl adj through 6th harmonic ampl adj.")
{
    SCM_ASSERT_TYPE(scm_is_true (scm_list_p (lst)), lst, SCM_ARG1, "list->instrument",
                    "17 element list");
    instrument_t *inst = scm_gc_malloc (sizeof (instrument_t), "instrument");
#define DBL(n) scm_to_double(scm_list_ref(lst, scm_from_int(n)))
    inst->D_attack = CLAMP(DBL(0), 0.0, 5.0);
    inst->D_decay = CLAMP(DBL(1), 0.0, 5.0);
    inst->D_release = CLAMP(DBL(2), 0.0, 5.0);
    inst->F_initial = CLAMP(DBL(3), 0.0, 10.0);
    inst->F_attack = CLAMP(DBL(4), 0.0, 10.0);
    inst->F_sustain = CLAMP(DBL(5), 0.0, 10.0);
    inst->F_release = CLAMP(DBL(6), 0.0, 10.0);
    inst->A_attack = CLAMP(DBL(7), 0.0, 1.0);
    inst->A_sustain = CLAMP(DBL(8), 0.0, 1.0);
    inst->noise = scm_to_bool (scm_list_ref (lst, scm_from_int(9)));
    inst->duty = CLAMP(DBL(10), 0.01, 0.99);
    for (int i = 0; i < HARMONICS_NUM; i ++)
        inst->A_harmonics[i] = CLAMP(DBL(i+11), 0.0, 2.0);

    scm_remember_upto_here_1 (lst);
    return scm_make_foreign_object_1 (G_instrument_tag, inst);
}

SCM_DEFINE (G_instrument_generate_wave, "instrument-generate-wave",
            4, 0, 0, (SCM s_instrument, SCM s_frequency, SCM s_duration, SCM s_amplitude), "\
Renders a note into a Guile bytevector containing a native-endian,\n\
signed, 16-bit waveform sampled at AUDIO_SAMPLE_RATE_IN_HZ.  The\n\
rendering is according to an instrument model.")
{
    instrument_t *I = scm_foreign_object_ref (s_instrument, 0);
    double A = CLAMP(scm_to_double (s_amplitude), 0.0, 2.0);
    double D = CLAMP(scm_to_double (s_duration), 0.000001, 500.0);
    double F = CLAMP(scm_to_double (s_frequency), 5.0, 22050.0);

    double duration_in_seconds = MAX (D, I->D_attack + I->D_decay + I->D_release);
    double D_sustain = duration_in_seconds - (I->D_attack + I->D_decay + I->D_release);

    size_t duration_in_samples = ceil(duration_in_seconds * (double) AUDIO_SAMPLE_RATE_IN_HZ);
    SCM vec = scm_c_make_bytevector (duration_in_samples * sizeof (int16_t));

    double t = 0.0;
    double  start_of_period_in_seconds = 0.0;
    double amplitude, frequency;
    double period = 0.0;
    double end_of_period_in_seconds;
    size_t sample_cur = 0;
    int first = TRUE;
    int level_a, level_b;
    static int count = 0;
    count ++;

    while (sample_cur < duration_in_samples)
    {
        // We're staring a new period in the waveform, so we compute
        // the current amplitude and frequency values.
        if(first || t >= end_of_period_in_seconds)
        {
            if (first)
                first = FALSE;
            else
                while (t - start_of_period_in_seconds >= period)
                    start_of_period_in_seconds += period;
            if (t < I->D_attack)
            {
                amplitude = A * ((I->A_attack / I->D_attack) * t);
                frequency = F * ((I->F_attack - I->F_initial) * t / I->D_attack + I->F_initial);
            }
            else if (t < I->D_attack + I->D_decay)
            {
                amplitude = A * (((I->A_sustain - I->A_attack) / I->D_decay) * (t - I->D_attack) + I->A_attack);
                frequency = F * (((I->F_sustain - I->F_attack) / I->D_decay) * (t - I->D_attack) + I->F_attack);
            }
            else if (t < I->D_attack + I->D_decay + D_sustain)
            {
                amplitude = A * I->A_sustain;
                frequency = F * I->F_sustain;
            }
            else if (t < I->D_attack + I->D_decay + D_sustain + I->D_release)
            {
                amplitude = A * ((-I->A_sustain / I->D_release) * (t - I->D_attack - I->D_decay - D_sustain) + I->A_sustain);
                frequency = F * (((I->F_release - I->F_sustain) / I->D_release) * (t - I->D_attack - I->D_decay - D_sustain) + I->F_sustain);
            }
            else
            {
                amplitude = 0.0;
                frequency = F * I->F_release;
            }

            period = 1.0 / frequency;
            end_of_period_in_seconds = start_of_period_in_seconds + period;
            if (I->noise)
            {
                // Level A is the amplitude of the 1st half-period.
                // Level B is the second half-period.  For "tone",
                // obviously it will be positive then negative.  The
                // noise generator uses a random amplitude sign.
	      if(rand() % 2)
                    level_a = AUDIO_CHANNEL_AMPLITUDE_MAX_F * amplitude;
                else
                    level_a = -AUDIO_CHANNEL_AMPLITUDE_MAX_F * amplitude;
	      if(rand() % 2)
                    level_b = AUDIO_CHANNEL_AMPLITUDE_MAX_F * amplitude;
                else
                    level_b = -AUDIO_CHANNEL_AMPLITUDE_MAX_F * amplitude;
            }
            else
            {
                level_a = AUDIO_CHANNEL_AMPLITUDE_MAX_F * amplitude;
                level_b = -AUDIO_CHANNEL_AMPLITUDE_MAX_F * amplitude;
            }

        }

        double x = 0.0;
        if (t - start_of_period_in_seconds < period * I->duty)
        {
            // The first half-period of the waveform
            double theta = M_PI * (t - start_of_period_in_seconds) / (period * I->duty);
            for (int i = 0; i < HARMONICS_NUM; i ++)
                x += level_a * sin((1.0 + i) * theta) * I->A_harmonics[i];
        }
        else if (t < end_of_period_in_seconds)
        {
            // The second half-period of the waveform.
            double theta = M_PI * ((t - start_of_period_in_seconds) - period * I->duty) / (period * (1.0 - I->duty));
            for (int i = 0; i < HARMONICS_NUM; i ++)
                x += level_b * sin((1.0 + i) * theta) * I->A_harmonics[i];
        }
        else
            g_assert_not_reached();

        scm_bytevector_s16_set_x (vec,
                                  scm_from_size_t (sample_cur * sizeof(int16_t)),
                                  scm_from_int16 (CLAMP ((int16_t)round(x), -AUDIO_CHANNEL_AMPLITUDE_MAX_F, AUDIO_CHANNEL_AMPLITUDE_MAX_F)),
                                  scm_native_endianness());

        sample_cur ++;
        t += 1.0 / (double) AUDIO_SAMPLE_RATE_IN_HZ;
    }
#if 1
    {
        if (count % 1 == 0) {
            FILE *fp;
            if(I->noise)
                fp = fopen("noise.txt", "wt");
            else
                fp = fopen("wave.txt", "wt");
            for(size_t i2 = 0; i2 < duration_in_samples; i2++)
                fprintf(fp, "%zu %d\n", i2,
                        (int) scm_bytevector_s16_ref (vec,
                                                      scm_from_size_t (i2 * sizeof(int16_t)),
                                                      scm_native_endianness()));

            fclose(fp);
        }
    }
#endif
    scm_remember_upto_here_1 (s_instrument);
    return vec;
}


SCM_DEFINE(G_play_wave, "play-wave", 1, 1, 0, (SCM wave, SCM start_time), "\
Play WAVE, a signed 16-bit bytevector of audio sampled at 44100 Hz.\n\
An optional start time may be provided, which is a Pulseaudio time in seconds\n\
If no start time is provided, the audio will play as soon as possible.")
{
    if (SCM_UNBNDP(start_time))
        start_time = scm_from_double (-1.0);
    SCM sound_playlist = scm_variable_ref (G_sound_playlist_var);
    sound_playlist = scm_append_x (scm_list_2 (sound_playlist,
                                               scm_list_1 (scm_list_3 (start_time, wave, scm_from_int(0)))));
    scm_variable_set_x (G_sound_playlist_var, sound_playlist);
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_audio_time, "audio-time", 0, 0, 0, (void), "\
Return the estimated Pulseaudio timer, in seconds.")
{
  return scm_from_double (am_timer_delta + g_get_monotonic_time());
}

SCM_DEFINE (G_audio_last_update_time, "audio-last-update-time", 0, 0, 0, (void), "\
Return the Pulseaudio timer val of the last time Pulseaudio\n\
requested an update.")
{
    return scm_from_double (am_timer);
}

double
wave_time_start (SCM sound)
{
    return scm_to_double (scm_car (sound));
}

double
wave_time_end (SCM sound)
{
    return wave_time_start (sound)
        + ((double) scm_c_bytevector_length (scm_cadr (sound)) / sizeof(int16_t)) / AUDIO_SAMPLE_RATE_IN_HZ;
}

int16_t *wave_data (SCM sound)
{
    return (int16_t *) SCM_BYTEVECTOR_CONTENTS (scm_cadr (sound));
}

const int16_t *audio_model_get_wave(double time_start, size_t n)
{
    double time_end = time_start + (double) n / AUDIO_SAMPLE_RATE_IN_HZ;
    SCM am_wave_list = scm_variable_ref(G_sound_playlist_var);

    am_timer = time_start;
    am_timer_delta = time_start - g_get_monotonic_time();

    /* Drop waves that have already finished. */
    /* FIXME: could be more efficient by using more schemey CDR operations. */
    for (int id = scm_to_int (scm_length (am_wave_list)) - 1; id >= 0; id --)
    {
        SCM s_wave = scm_list_ref (am_wave_list, scm_from_int (id));
        if (wave_time_start(s_wave) < 0.0)
        {
            // This wave had a start time of zero, meaning it is to
            // start as soon as possible.  We set its start time to
            // now.
            scm_list_set_x (am_wave_list,
                            scm_from_int(id),
                            scm_list_3(scm_from_double (time_start), scm_cadr(s_wave), scm_from_int(0)));
        }
        else if (wave_time_end(s_wave) < time_start)
        {
            // Wave has expired.  Dequeue it.
            if (id > 0)
                scm_delq_x (s_wave, am_wave_list);
            else
                am_wave_list = scm_cdr (am_wave_list);
        }
    }

    scm_variable_set_x (G_sound_playlist_var, am_wave_list);

    memset (am_working, 0, AUDIO_BUFFER_SIZE * sizeof(float));

    for (int id = scm_to_int (scm_length (am_wave_list)) - 1; id >= 0; id --)
    {
        SCM s_wave = scm_list_ref (am_wave_list, scm_from_int (id));
        double wave_start = wave_time_start(s_wave);
        double wave_end = wave_time_end(s_wave);

        if (time_end < wave_start)
        {
            // hasn't started yet
        }
        else if (time_start <= wave_start)
        {
            // wave starts during this sample
            size_t delta = (wave_start - time_start) * AUDIO_SAMPLE_RATE_IN_HZ;
            size_t len   = (wave_end - wave_start) * AUDIO_SAMPLE_RATE_IN_HZ;
            if (len > n - delta)
                len = n - delta;
            int16_t *wav = wave_data(s_wave);
            for (size_t i = 0; i < len; i ++)
                am_working[i + delta] += (float) wav[i];
            // Store the number of samples processed
            //am_wave_list = scm_list_set_x (am_wave_list, scm_from_int (id),
            //                               scm_list_3 (scm_car (s_wave), scm_cadr(s_wave), scm_from_size_t (len)));
            scm_list_set_x (s_wave, scm_from_int(2), scm_from_size_t (len));
        }
        else
        {
            // wave has already started
            // size_t delta = (time_start - wave_start) * AUDIO_SAMPLE_RATE_IN_HZ;
            size_t delta = scm_to_size_t (scm_list_ref (s_wave, scm_from_int(2)));
            size_t len   = scm_c_bytevector_length (scm_cadr(s_wave)) / 2 - delta;
            if (len > n)
                len = n;
            int16_t *wav = wave_data(s_wave);
            for (size_t i = 0; i < len; i ++)
                am_working[i] += (float) wav[i + delta];
            // Store the number of samples processed
            scm_list_set_x (s_wave, scm_from_int(2), scm_from_size_t (len + delta));
        }
    }

    for (int i = 0; i < n; i ++)
        am_sum[i] = (int16_t) round(CLAMP(am_working[i],
                                          AUDIO_CHANNEL_AMPLITUDE_MIN,
                                          AUDIO_CHANNEL_AMPLITUDE_MAX));

    return am_sum;
}


void
am_init_guile_procedures (void)
{
    G_instrument_tag = scm_make_foreign_object_type (scm_from_utf8_symbol ("instrument"),
                                                   scm_list_1 (scm_from_utf8_symbol ("data")),
                                                   NULL);

#include "burro_canvas_audio.x"
    scm_c_export ("%sound-playlist",
                  "list->instrument",
                  "instrument-generate-wave",
                  "play-wave",
                  "audio-time",
                  "audio-last-update-time",
                  NULL
        );
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

#pragma GCC diagnostic pop

/*  pulseaudio.c

    Copyright (C) 2018   Michael L. Gran
    This file is part of Burro Engine

    Burro Engine is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Burro Engine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Burro Engine.  If not, see <http://www.gnu.org/licenses/>.
*/


/* This callback gets called when our context changes state.  We
 * really only care about when it's ready or if it has failed. */
static void cb_audio_context_state(pa_context *c, void *userdata)
{
    pulse.state = (int) xpa_context_get_state(c);
    switch(pulse.state)
    {
    case PA_CONTEXT_UNCONNECTED:
        g_debug("PulseAudio set context state to UNCONNECTED");
        break;
    case PA_CONTEXT_CONNECTING:
        g_debug("PulseAudio set context state to CONNECTING");
        break;
    case PA_CONTEXT_AUTHORIZING:
        g_debug("PulseAudio set context state to AUTHORIZING");
        break;
    case PA_CONTEXT_SETTING_NAME:
        g_debug("PulseAudio set context state to SETTING NAME");
        break;
    case PA_CONTEXT_FAILED:
        g_debug("PulseAudio set context state to FAILED");
        break;
    case PA_CONTEXT_TERMINATED:
        g_debug("PulseAudio set context state to TERMINATED");
        break;
    case PA_CONTEXT_READY:
        g_debug("PulseAudio set context state to READY");
        pulse_initialize_audio_step_2 (c);
        break;
    default:
        g_debug("PulseAudio set context state to %d", pulse.state);
        break;
    }
}

/* This callback is called when the server starts playback after an
 * underrun or on initial startup. */
static void cb_audio_stream_started(pa_stream *p, void *userdata)
{
    g_debug("PulseAudio started playback");
}

/* This is called when new data may be written to the stream.  If we
   have data, we can ship it, otherwise we just note that the stream is
   waiting.  */
static void cb_audio_stream_write(pa_stream *p, size_t nbytes, void *userdata)
{
    size_t n = nbytes / sizeof(int16_t);

    g_return_if_fail (p != NULL);

    pa_usec_t usec = xpa_stream_get_time (p);
    if (usec != 0)
        g_debug("Pulseaudio time is %"PRIu64", requests %zu samples", usec, n);
    if (n > AUDIO_BUFFER_SIZE)
    {
        g_warning ("Pulseaudio buffer read overflow %zu > %u",
                   n, AUDIO_BUFFER_SIZE);
        n = AUDIO_BUFFER_SIZE;
        nbytes = n * sizeof (uint16_t);
    }
#if 0    
    const int16_t *wav = audio_model_get_wave(1.0e-6 * usec, n);
    xpa_stream_write(p, wav, nbytes);
#else
    int16_t *wav = malloc(nbytes);
    memset (wav, 0, nbytes);
    for (int i = 0; i < n; i ++)
      wav[i] = rand() % 600 - 300;
    xpa_stream_write (p, wav, nbytes);
    free(wav);
#endif    
    pulse.samples_written += n;
}

void pulse_initialize_audio_step_1()
{
    pa_mainloop_api *vtable = NULL;
    pa_proplist *main_proplist = NULL;

    pulse.loop = xpa_glib_mainloop_new (g_main_context_default());
#ifdef USE_GLIB_MAINLOOP
    vtable = pa_glib_mainloop_get_api (pulse.loop);
#else
    vtable = xpa_mainloop_get_api(pulse.loop);
#endif

    /* PROPLIST: Only the PA_PROP_MEDIA_ROLE is important.  */
    main_proplist = xpa_proplist_new();
    xpa_proplist_sets(main_proplist, PA_PROP_MEDIA_ROLE,
                      BURRO_PROP_MEDIA_ROLE);
    xpa_proplist_sets(main_proplist, PA_PROP_APPLICATION_ID,
                      BURRO_PROP_APPLICATION_ID);
    xpa_proplist_sets(main_proplist, PA_PROP_APPLICATION_NAME,
                      BURRO_PROP_APPLICATION_NAME);

    /* A context is the basic object for a connection to a PulseAudio
       server. It multiplexes commands, data streams and events
       through a single channel.  */
    pulse.context = xpa_context_new_with_proplist(vtable,
                                                  BURRO_PROP_APPLICATION_NAME,
                                                  main_proplist);
    xpa_proplist_free (main_proplist);
    xpa_context_set_state_callback(pulse.context,
                                   cb_audio_context_state, NULL);

    pulse.state = PA_CONTEXT_UNCONNECTED;

    /* Connect the context */
    xpa_context_connect_to_default_server(pulse.context);

}

void pulse_initialize_audio_step_2(pa_context *context)
{
    pa_proplist *stream_proplist;
    pa_sample_spec sample_specification;
    pa_channel_map channel_map;
    pa_buffer_attr buffer_attributes;
    pa_stream *stream;

    /* Now we need to add our mono audio channel to the connection */

    /* SAMPLE_SPEC:  we describe the data going into our channel */
    sample_specification.format = PA_SAMPLE_S16NE;
    sample_specification.rate = AUDIO_SAMPLE_RATE_IN_HZ;
    sample_specification.channels = 1;
    g_assert(pa_sample_spec_valid(&sample_specification));

    /* BUFFER_ATTRIBUTES: Here we set the buffering behavior of the
     * audio.  We want low latency.

     One wiki suggests that to set a specific latency,
     1. use pa_usec_to_bytes(&ss, ...) to convert the latency from a time unit to bytes
     2. use the PA_STREAM_ADJUST_LATENCY flag
     3. set pa_buffer_attr::tlength to latency in samples
     4. set rest of pa_buffer_attr to -1

     http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/LatencyControl
    */

    buffer_attributes.tlength = pa_usec_to_bytes (AUDIO_LATENCY_REQUESTED_IN_MILLISECONDS * MICROSECONDS_PER_MILLISECOND,
                                                  &sample_specification);
    buffer_attributes.maxlength = -1; /* -1 == default */
    buffer_attributes.prebuf = -1;
    buffer_attributes.minreq = -1;
    buffer_attributes.fragsize = -1;

    /* PROPERTY_LIST: Then we set up a structure to hold PulseAudio
     * properties for this. */
    stream_proplist = xpa_proplist_new();
    xpa_proplist_sets(stream_proplist, PA_PROP_MEDIA_NAME, "mono channel");
    xpa_proplist_sets(stream_proplist, PA_PROP_MEDIA_ROLE, "game");

    /* CHANNEL_MAP: Then we say which speakers we're using.  The game
       is mono, so - that makes it simple. */

    pa_channel_map_init_extend(&channel_map, 1, PA_CHANNEL_MAP_DEFAULT);

    if (!pa_channel_map_compatible(&channel_map, &sample_specification))
        g_error("Channel map doesn't match sample specification");

    {
        char tss[100], tcm[100];
        g_debug("Opening a stream with sample specification '%s' and channel map '%s'.",
                pa_sample_spec_snprint(tss, sizeof(tss), &sample_specification),
                pa_channel_map_snprint(tcm, sizeof(tcm), &channel_map));
    }

    /* STREAM: Group everything together as a stream */
    g_assert (pulse.context == context);
    stream = xpa_stream_new_with_proplist(pulse.context,
                                          "mono channel", /* Stream name */
                                          &sample_specification,
                                          &channel_map,
                                          stream_proplist);
    xpa_proplist_free (stream_proplist);

    xpa_stream_set_started_callback(stream, cb_audio_stream_started, NULL);
    xpa_stream_set_write_callback(stream, cb_audio_stream_write, NULL);

    pulse.samples_written = 0U;

    /* Connect the stream to the audio loop */
    xpa_stream_connect_playback_to_default_device (stream, context,
                                                   &buffer_attributes,
                                                   /* PA_STREAM_ADJUST_LATENCY | */
                                                   PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE);
    /* Finally done! */
    g_debug("PulseAudio initialization complete");
}

/* This finalizer is called if we are shutting down cleanly */
void pulse_finalize_audio()
{
#ifdef USE_GLIB_MAINLOOP
    pa_glib_mainloop_free (pulse.loop);
#else
    pa_threaded_mainloop_free(pulse.loop);
#endif
    pulse.finalize = TRUE;
    g_debug("PulseAudio finalization complete");
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


