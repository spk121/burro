#include <glib.h>

typedef struct coreaudio_priv_tag {

	IMMDevice *device;
	wchar_t *device_name;
	IAudioClient *client;
	HANDLE shutdown_event;
	HANDLE audio_samples_ready_event;


	unsigned samples_written;
} coreaudio_priv_t;

coreaudio_priv_t coreaudio;

void canvas_audio_init()
{
	HRESULT hr;
	bool retValue = true;
	IMMDeviceEnumerator *device_enumerator = NULL;
	IPropertyStore *property_store = NULL;
	PROPVARIANT var_name;

	hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), NULL,
		CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator));
	if (FAILED(hr))
	{
		// g_debug("Unable to instantiate audio device enumerator: %x\n", hr);
		retValue = false;
		goto Exit;
	}
	// g_debug("Initiated audio device enumerator");

	hr = device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &coreaudio.device);
	if (FAILED(hr))
	{
		// g_debug("Unable to find an audio endpoint: %x\n", hr);
		retValue = false;
		goto Exit;
	}

	// Initialize container for property value.
	PropVariantInit(&var_name);

	hr = device->OpenPropertyStore(STGM_READ, &property_store);
	// Get the endpoint's friendly-name property.
	hr = property_store->GetValue(PKEY_Device_FriendlyName, &var_name);
	coreaudio.device_name = wcsdup(var_name.pwszVal);

	coreaudio.shutdown_event = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (coreaudio.shutdown_event == NULL)
	{
		printf("Unable to create shutdown event: %d.\n", GetLastError());
		return false;
	}

	coreaudio.audio_samples_ready_event = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (coreaudio.audio_samples_ready_event == NULL)
	{
		printf("Unable to create samples ready event: %d.\n", GetLastError());
		return false;
	}

	HRESULT hr = coreaudio.device->Activate(__uuidof(IAudioClient),
		CLSCTX_INPROC_SERVER, NULL,
		reinterpret_cast<void **>(&coreaudio.client));
	if (FAILED(hr))
	{
		printf("Unable to activate audio client: %x.\n", hr);
		return false;
	}

	stat st;
	int ret = stat("hello", &st);

	PropVariantInit(&varName);
	hr = props->GetValue(PKEY_AudioEngine_DeviceFormat, &varName);
	WAVEFORMATEXTENSIBLE *x = (WAVEFORMATEXTENSIBLE *)varName.blob.pBlobData;

	pa_mainloop_api *vtable = NULL;
	pa_proplist *main_proplist = NULL;

#ifdef USE_GLIB_MAINLOOP
	pulse.loop = xpa_glib_mainloop_new(g_main_context_default());
	vtable = pa_glib_mainloop_get_api(pulse.loop);
#else
	pulse.loop = pa_mainloop_new();
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
	xpa_proplist_free(main_proplist);
	xpa_context_set_state_callback(pulse.context,
		cb_audio_context_state, NULL);

	pulse.state = PA_CONTEXT_UNCONNECTED;

	/* Connect the context */
	xpa_context_connect_to_default_server(pulse.context);

	am.volume = 1.0;
	am.playing = TRUE;
	for (int i = 0; i < 4; i++)
		am.channels[i].volume = 1.0;

}



#if 0
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
#define AM_SAMPLE_RATE_IN_HZ (48000u)
#define AM_BUFFER_DURATION_IN_MILLISECONDS (2000u)
#define AM_BUFFER_SIZE ((AM_SAMPLE_RATE_IN_HZ * AM_BUFFER_DURATION_IN_MILLISECONDS) / 1000)

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

typedef struct pulse_priv_tag {
    pa_context_state_t state;
    pa_context *context;
#ifdef USE_GLIB_MAINLOOP
    pa_glib_mainloop *loop;
#else
    pa_mainloop *loop;
#endif
    unsigned samples_written;
} pulse_priv_t;

static audio_model_t am;
static pulse_priv_t pulse;

static void canvas_audio_init_step_2 (pa_context *context);
static void cb_audio_context_state(pa_context *c, void *userdata);
static void cb_audio_stream_started(pa_stream *p,
                                    void *userdata);
static void cb_audio_stream_write(pa_stream *p, size_t nbytes, void *userdata);

static gboolean
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

static void
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

/* This callback gets called when our context changes state.  We
 * really only care about when it's ready or if it has failed. */
static void cb_audio_context_state(pa_context *c,
                                   void *userdata __maybe_unused)
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
        canvas_audio_init_step_2 (c);
        break;
    default:
        g_debug("PulseAudio set context state to %d", pulse.state);
        break;
    }
}

/* This callback is called when the server starts playback after an
 * underrun or on initial startup. */
static void cb_audio_stream_started(pa_stream *p __maybe_unused,
                                    void *userdata __maybe_unused)
{
    g_debug("PulseAudio started playback");
}

/* This is called when new data may be written to the stream.  If we
   have data, we can ship it, otherwise we just note that the stream is
   waiting.  */
static void cb_audio_stream_write(pa_stream *p,
                                  size_t nbytes,
                                  void *userdata __maybe_unused)
{
    size_t n = nbytes / sizeof(float);

    g_return_if_fail (p != NULL);

#if 0
    pa_usec_t usec = xpa_stream_get_time (p);
    if (usec != 0)
        g_debug("Pulseaudio time is %"PRIu64", requests %zu samples", usec, n);
#endif
    
    if (n > AM_BUFFER_SIZE)
    {
        g_warning ("Pulseaudio buffer read overflow %zu > %u",
                   n, AM_BUFFER_SIZE);
        n = AM_BUFFER_SIZE;
        nbytes = n * sizeof (float);
    }

    am_update (n);
    xpa_stream_write (p, am.buffer, nbytes);
    pulse.samples_written += n;
}

void canvas_audio_init()
{
    pa_mainloop_api *vtable = NULL;
    pa_proplist *main_proplist = NULL;
    
#ifdef USE_GLIB_MAINLOOP
    pulse.loop = xpa_glib_mainloop_new (g_main_context_default());
    vtable = pa_glib_mainloop_get_api (pulse.loop);
#else
    pulse.loop = pa_mainloop_new ();
    vtable = xpa_mainloop_get_api (pulse.loop);
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

    am.volume = 1.0;
    am.playing = TRUE;
    for (int i = 0; i < 4; i ++)
        am.channels[i].volume = 1.0;
    
}

static void
canvas_audio_init_step_2(pa_context *context)
{
    pa_proplist *stream_proplist;
    pa_sample_spec sample_specification;
    pa_channel_map channel_map;
    pa_buffer_attr buffer_attributes;
    pa_stream *stream;

    /* Now we need to add our mono audio channel to the connection */

    /* SAMPLE_SPEC:  we describe the data going into our channel */
    sample_specification.format = PA_SAMPLE_FLOAT32NE;
    sample_specification.rate = AM_SAMPLE_RATE_IN_HZ;
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
void canvas_audio_fini()
{
    if (pulse.context)
    {
        pa_context_unref(pulse.context);
        pulse.context = NULL;
    }
#ifdef USE_GLIB_MAINLOOP
    if (pulse.loop)
    {
        pa_glib_mainloop_free (pulse.loop);
        pulse.loop = NULL;
    }
#else
    if (pulse.loop)
    {
        pa_mainloop_free (pulse.loop);
        pulse.loop = NULL;
    }
#endif
    g_debug("PulseAudio finalization complete");
}

int
canvas_audio_iterate()
{
    if (pulse.loop)
        return pa_mainloop_iterate (pulse.loop, 0, NULL);
    return -1;
}

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


