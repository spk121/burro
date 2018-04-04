#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

char pcmout[4096];

FILE *fp_in, *fp_out;

void
resample_to_mono (float start_time, int channels, int rate, int16_t *buf, int n);


int main(int argc, char **argv)
{
  OggVorbis_File vf;
  int eof=0;
  int current_section;

  fp_in = fopen("in.txt", "wt");
  fp_out = fopen("out.txt", "wt");
  
  ov_fopen(argv[1], &vf);
  vorbis_info *vi=ov_info(&vf,-1);
  char **ptr=ov_comment(&vf,-1)->user_comments;
  while(*ptr)
    {
      printf("%s\n",*ptr);
      ++ptr;
    }
  printf("\nBitstream is %d channel, %ldHz\n",vi->channels,vi->rate);
  printf("There are %ld streams\n", ov_streams(&vf));
  printf("\nDecoded length: %ld samples\n",
	 (long)ov_pcm_total(&vf,0));
  printf("Encoded by: %s\n\n",ov_comment(&vf,-1)->vendor);
  printf("Time total: %f\n", ov_time_total(&vf, 0));
  printf("Bitrate: %0.2f kB\n", 0.125 * ov_bitrate(&vf, 0) / 1024.0);
  
#ifdef _WIN32
  _setmode( _fileno( stdin ), _O_BINARY );
  _setmode( _fileno( stdout ), _O_BINARY );
#endif

  #if 0
  if(ov_open_callbacks(stdin, &vf, NULL, 0, OV_CALLBACKS_NOCLOSE) < 0)
    {
      fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
      exit(1);
    }
  #endif

  int m = 0;
  float time_cur = 0.0;
  while(!eof){
    long ret=ov_read(&vf,pcmout,sizeof(pcmout),0,2,1,&current_section);
    if (ret == 0) {
      printf ("eof\n");
      
      /* EOF */
      eof=1;
    } else if (ret < 0) {
      printf ("error\n");
      /* error in the stream.  Not a problem, just reporting it in
	 case we (the app) cares.  In this case, we don't. */
    } else {
      // printf ("%ld ", ret);
      /* we don't bother dealing with sample rate changes, etc, but
	 you'll have to*/
      // fwrite(pcmout,1,ret,stdout);
      
      resample_to_mono (time_cur, vi->channels, vi->rate, (int16_t *) pcmout, ret / 2);
      if (m++ > 20)
	break;

      time_cur += (((ret / 2.0f) / vi->channels) / (float) vi->rate);
    }
  }
    
  
  ov_clear(&vf);
    
  fprintf(stderr,"Done.\n");
  fclose(fp_in);
  fclose(fp_out);
  return 0;
}

#define AUDIO_RATE 48000.0f
void
resample_to_mono (float start_time, int channels, int rate, int16_t *buf, int n)
{
  int n_input = n / channels;
  float time_seconds = (float) (n / channels ) / (float) rate;
  int n_output = time_seconds * AUDIO_RATE;

  if (n_output < n_input / 2)
    {
      // If we have to do a lot of downsampling, we need to
      // low-pass-filter the input to lessen aliasing effects.
      float filter_weight = (float) (n_input - n_output) / (float) n_input;
      for (int j = 1; j < n_input; j ++)
	{
	  fprintf (fp_in, "%f ", start_time + j  / (float) rate);
	  for (int c = 0; c < channels; c ++)
	    fprintf (fp_in, "%d ", buf[channels * j + c]);
	  for (int c = 0; c < channels; c ++)
	    {
	      int16_t cur = buf[channels * j + c];
	      int16_t prev = buf[channels * (j-1) + c];
	      buf[channels * j + c] = (filter_weight * prev
				       + (1.0 - filter_weight) * cur);
	      fprintf (fp_in, "%d ", buf[channels * j + c]);
	    }
	  fprintf(fp_in, "\n");
	}
    }
  else
    {
      for (int j = 1; j < n_input; j ++)
	{
	  fprintf (fp_in, "%f ", start_time + j  / (float) rate);
	  for (int c = 0; c < channels; c ++)
	    fprintf (fp_in, "%d ", buf[channels * j + c]);
	  fprintf(fp_in, "\n");
	}
    }
  
  float *output = malloc (n_output * sizeof (float));
  float time_cur = 0.0f;
  for (int i = 0; i < n_output; i ++)
    {
      time_cur = i / AUDIO_RATE;
      // Figure out if we are interpolating, or if we happen to be
      // right on an input sample.
      float input_sample = time_cur * rate;
      float frac_part, int_part;
      frac_part = modff(input_sample, &int_part);
      long j1 = lrintf(int_part);
      long j2 = j1 + 1;
      if (j2 >= n_input)
	j2 --;
      long val1 = 0.0;
      long val2 = 0.0;
      for (int c = 0; c < channels; c ++)
	{
	  val1 += (float) buf[channels * j1 + c];
	  val2 += (float) buf[channels * j2 + c];
	}
      val1 /= (float) channels;
      val2 /= (float) channels;
      
      if (frac_part < 0.001 || j1 == j2)
	output[i] = val1;
      else if (frac_part > 0.999)
	output[i] = val2;
      else
	output[i] = val1 + frac_part * (val2 - val1);
      fprintf (fp_out, "%f %f\n", start_time + time_cur, output[i]);
    }

  
  free(output);
}

