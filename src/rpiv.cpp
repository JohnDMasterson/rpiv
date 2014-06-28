#include <iostream> //library for simple input output
#include <alsa/asoundlib.h> //library for intracting with alsa
#include <signal.h>


struct sigaction old_action;
int format = -1;
int rate = -1;


//handler for ctrl+c
void sigint_handler(int sig_no){
 printf("\033[0m\n");
 system("setterm -blank 10");
 system("clear");
 sigaction(SIGINT, &old_action, NULL);
 kill(0, SIGINT);
}

void* music(void* data){
	signed char *buffer;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	val = 44100;
	snd_pcm_uframes_t frames;
	frames = 256;
	char *device = ((char*)data);
	int i, n, o, size, dir, err, lo;
	int tempr, templ;
	int radj, ladj;

	if((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0 ))
		printf("error opening stream:   %s\n", snd_strerror(err));

	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params);
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(handle, params, 2);
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

 err = snd_pcm_hw_params(handle, params);
 if (err < 0) {
 	fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
 	exit(1);
 }

 snd_pcm_hw_params_get_format(params, (snd_pcm_format_t * )&val);
 if(val < 6)
 	format = 16;
 else if(val < 10)
 	format = 24;
 else
 	format = 32;

 snd_pcm_hw_params_get_rate(params, &rate, &dir);
 snd_pcm_hw_params_get_period_size(params, &frames, &dir);
 snd_pcm_hw_params_get_period_time(params, &val, &dir);

 size = frames * (format/8)*2;
 buffer = (char *) malloc(size);
 radj=format/4;
 ladj=format/8;
 o=0;
 while(true){
 	err = snd_pcm_readi(handle, buffer, frames);
 }
}


int main(){
	return 0;
}