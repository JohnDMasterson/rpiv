#include <stdio.h> //library for simple input output
#include <stdlib.h>
#include <alsa/asoundlib.h> //library for intracting with alsa
#include <signal.h> //for kill
#include <stdbool.h> //for booleans
#include <pthread.h> //threading


struct sigaction old_action;
int format = -1;
int rate = -1;
int sharedBufferSize = 2048;
int frequencies[2048];


//quits program
void sigint_handler(int sig_no){
 printf("\033[0m\n");
 system("setterm -blank 10");
 system("clear");
 sigaction(SIGINT, &old_action, NULL);
 kill(0, SIGINT);
}

void* music(void* data){
	//buffer for audio
	signed char *buffer; 
	//handle for the PCM
	snd_pcm_t *handle; 
	snd_pcm_hw_params_t *params;
	//collections of channels?
	snd_pcm_uframes_t frames = 256;
	//audio playback rate, assumes 44100 Hz
	unsigned int val = 44100; 
	//channels for audio, assumes 2 channels
	int numChannels = 2; 
	 //name of device. ex is "hw:1,1"
	char *device = ((char*)data);
	//used in for loop later
	int i;
	//last frequency used in for loop
	int lastFrequency;
	int size, dir, err, lo;

	//storage for buffer channels
	int rChannel, lChannel;
	//offsets for left and right channels
	int radj, ladj;

	//Prepares the device for capture
	if((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0 ))
		printf("error opening stream:   %s\n", snd_strerror(err));

	//tries to set the device parameters
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params);
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(handle, params, numChannels);
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
 err = snd_pcm_hw_params(handle, params);
 if (err < 0) {
 	fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
 	exit(1);
 }

 //gets the actual settings for the device
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

 //sets up buffer 
 size = frames * (format/8)*2; //frames to collect * format in bits / 8 bits in a byte * 2 channels
 buffer = (char *) malloc(size);

 //I have no idea what these are
 radj=format/4;
 ladj=format/8;
 lastFrequency=0;

 while(true){
 	//reads pcm and writes it to the buffer
 	err = snd_pcm_readi(handle, buffer, frames);

 	//checks for underrun
 	if (err == -EPIPE)
 		snd_pcm_prepare(handle);

 	//processes audio from smallest to biggest
 	for (i=0; i<size; i=i+(ladj)*2) {
			//            left                       right
   //structure [litte],[litte],[big],[big],[litte],[litte],[big],[big]... 


 		//buffer at right shifted two bits left
   rChannel = ((buffer[i+(radj)-1 ] << 2));
   //buffer to the left of previous shifted 6 bits right
   lo=((buffer[i+(radj)-2] >> 6));
   if (lo<0)
   	lo=lo+4;
   if(rChannel>=0)
   	rChannel= rChannel + lo;
   else
   	rChannel= rChannel - lo;

   //same for left channel
   lChannel=(buffer[i+(ladj)-1] << 2);
   lo=(buffer[i+(ladj)-2] >> 6);
   if(lo<0)
   	lo=lo+4;
   if(lChannel>=0)
   	lChannel=lChannel+lo;
   else 
   	lChannel=lChannel-lo;

   //adding channels and storing it in the buffer
   frequencies[lastFrequency] = (rChannel+lChannel)/2;
   lastFrequency++;
   if(lastFrequency == sharedBufferSize-1)
   	lastFrequency=0; 
 	}

 }
}



int main(int argc, char **argv)
{
 // thread for music
 pthread_t musicThread; 
 // thread id for music
 int musicThreadId; 
 //defaults to hw device 2,1
 char *device = "hw:2,1";
	
 //setting up thread for music
 int timeout=0;
 musicThreadId = pthread_create(&musicThread, NULL, music,(void*)device);
 while (format==-1||rate==-1) {
  //sleeps for 1 second
  usleep(1000);
  timeout++;
  //times our after 1 minute 
  if(timeout>60)
   exit(1);
 }

 //setting up console
 system("setterm -cursor off");
 system("setterm -blank 0");
 //clearing console
 printf("\033[0m\n");
 system("clear");
 system("COLOR 45");


 //test for setting up console colors
 int test = 0;
 while(test < 20){

  #define ANSI_COLOR_RED "\x1b[31m"
  #define ANSI_COLOR_GREEN "\x1b[32m"
  #define ANSI_COLOR_BLUE "\x1b[34m"
  #define ANSI_COLOR_RESET "\x1b[0m"


  printf(ANSI_COLOR_RED "this is red    " ANSI_COLOR_RESET);
  printf(ANSI_COLOR_GREEN "this is green    " ANSI_COLOR_RESET);
  printf(ANSI_COLOR_BLUE "this is blue"  "\n" ANSI_COLOR_RESET);
  test++;
 }





	return 0;
}