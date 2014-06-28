#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <sys/ioctl.h>
#include <fftw3.h>
#define PI 3.14159265358979323846
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>



#define RED_PIN 0
#define GREEN_PIN 1
#define BLUE_PIN 2
#include <wiringPi.h>
#include <softPwm.h>


struct sigaction old_action;
int M=2048;
int shared[2048];
int debug=0;
int format=-1;
int rate=-1;


void sigint_handler(int sig_no)
{
    printf("\033[0m\n");
    system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz ");
    system("setterm -cursor on");
    system("setterm -blank 10");
    system("clear");        
    printf("CTRL-C pressed -- goodbye\n");
    sigaction(SIGINT, &old_action, NULL);
    kill(0, SIGINT);
}


void*
music(void* data)
{
signed char *buffer;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
unsigned int val;
snd_pcm_uframes_t frames;
char *device = ((char*)data);
val=44100;
int i, n, o, size, dir, err,lo;
int tempr,templ;
int radj,ladj;

//**init sound device***//

if ((err = snd_pcm_open(&handle, device,  SND_PCM_STREAM_CAPTURE , 0) < 0))
  printf("error opening stream:    %s\n",snd_strerror(err) );
else
 if(debug==1){ printf("open stream succes\n");  }    
snd_pcm_hw_params_alloca(&params);//assembling params
snd_pcm_hw_params_any (handle, params);//setting defaults or something
snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);//interleeaved mode right left right left
snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE); //trying to set 16bit
snd_pcm_hw_params_set_channels(handle, params, 2);//asuming stereo
val = 44100;
snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);//trying 44100 rate
frames = 256;
snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir); //number of frames pr read

err = snd_pcm_hw_params(handle, params); //atempting to set params
if (err < 0) {
    fprintf(stderr,
            "unable to set hw parameters: %s\n",
            snd_strerror(err));
    exit(1);
}

snd_pcm_hw_params_get_format(params, (snd_pcm_format_t * )&val); //getting actual format	
//convverting result to number of bits
if(val<6)format=16;
else if(val>5&&val<10)format=24;
else if(val>9)format=32;


snd_pcm_hw_params_get_rate( params, &rate, &dir); //getting rate 	

snd_pcm_hw_params_get_period_size(params,&frames, &dir);   
snd_pcm_hw_params_get_period_time(params,  &val, &dir);

size = frames * (format/8)*2; // frames * bits/8 * 2 channels 
buffer = (char *) malloc(size); 
radj=format/4;//adjustments for interleaved
ladj=format/8;
o=0;
while(1){

    err = snd_pcm_readi(handle, buffer, frames);
    if (err == -EPIPE) {
      /* EPIPE means overrun */
       if(debug==1){ fprintf(stderr, "overrun occurred\n");}
      snd_pcm_prepare(handle);
    } else if (err < 0) {
     if(debug==1){ fprintf(stderr, "error from read: %s\n",
              snd_strerror(err));}
    } else if (err != (int)frames) {
     if(debug==1){ fprintf(stderr, "short read, read %d %d frames\n", err,(int)frames);}
   }
   
       //sorting out one channel and only biggest octet
         n=0;//frame counter
        for (i=0; i<size ; i=i+(ladj)*2)
        {
                //              left                            right
                 //structuere [litte],[litte],[big],[big],[litte],[litte],[big],[big]... 
                  
                //first channel           
                tempr = ((buffer[i+(radj)-1 ] << 2));//using the 10 upper bits this whould give me a vert res of 1024, enough...
                lo=((buffer[i+(radj)-2] >> 6));
                if (lo<0)lo=lo+4;
                if(tempr>=0)tempr= tempr + lo;
                if(tempr<0)tempr= tempr - lo;

                //other channel
                templ=(buffer[i+(ladj)-1] << 2);
                lo=(buffer[i+(ladj)-2] >> 6);
                if(lo<0)lo=lo+4;
                if(templ>=0)templ=templ+lo;
                else templ=templ-lo;

                //adding channels and storing it int the buffer
                shared[o]=(tempr+templ)/2;
                o++;
                if(o==M-1)o=0;

                //shifing ringbuffer one to the left, this ended up using to much cpu..
                //for(o=0;o<M-1;o++) shared[o]=shared[o+1];
                n++;                
        }
    }
}





int main(int argc, char **argv)
{
pthread_t  p_thread; 
int        thr_id; 
char *device = "hw:1,1";
float fc[200];//={150.223,297.972,689.062,1470,3150,5512.5,11025,18000};
float fr[200];//={0.00340905,0.0067567,0.015625,0.0333,0.07142857,0.125,0.25,0.4};
int lcf[200], hcf[200];
float f[200];
int flast[200];
float peak[201];
int y[M/2+1];
long int lpeak,hpeak;
int bands=3;
int sleep=0;
int i, n, o, size, dir, err,bw,width,height,c,rest,virt;
int autoband=1;
//long int peakhist[bands][400];
float temp;
double sum=0;
struct winsize w;
double in[2*(M/2+1)];
fftw_complex out[M/2+1][2];
fftw_plan p;
char *color;
int col = 36;
int bgcol = 0;
int sens = 100;
int move=0;
int fall[200];
float fpeak[200];
float k[200];
float g;
int framerate=60;
char *usage="\nUsage : ./cava [options]\n\nOptions:\n\t-b 1..(console columns/2-1) or 200\t number of bars in the spectrum (default 25 + fills up the console), program wil auto adjust to maxsize if input is to high)\n\n\t-d 'alsa device'\t\t\t name of alsa capture device (default 'hw:1,1')\n\n\t-c color\t\t\t\t suported colors: red, green, yellow, magenta, cyan, white, blue, black (default: cyan)\n\n\t-C backround color\t\t\t supported colors: same as above (default: no change) \n\n\t-s sensitivity %\t\t\t sensitivity in percent, 0 means no respons 100 is normal 50 half 200 double and so forth\n\n\t-f framerate \t\t\t\t max frames per second to be drawn, if you are experiencing high CPU usage, try redcing this (default: 60)\n\n";
//**END INIT

for (i=0;i<200;i++)
{
flast[i]=0;
fall[i]=0;
fpeak[i]=0;
}
for (i=0;i<M;i++)shared[M]=0;

//setup for wiring pi
wiringPiSetup();
softPwmCreate(RED_PIN, 100, 100);
softPwmCreate(GREEN_PIN, 100, 100);
softPwmCreate(BLUE_PIN, 100, 100);

//**arg handler**//
while ((c = getopt (argc, argv, "b:d:s:f:c:C:h")) != -1)
switch (c) {
           case 'b':
             bands = atoi(optarg);
            autoband=0;  //dont automaticly add bands to fill frame
            if (bands>200)bands=200;
             break;
           case 'd':
             device = optarg;
             break;
           case 's':
             sens = atoi(optarg);
             break;
           case 'f':
             framerate = atoi(optarg);
             break;
           case 'c':
            col=0;
             color = optarg;
             if(strcmp(color,"black")==0) col=30;
             if(strcmp(color,"red")==0) col=31;
             if(strcmp(color,"green")==0) col=32;
             if(strcmp(color,"yellow")==0) col=33;
             if(strcmp(color,"blue")==0) col=34;
             if(strcmp(color,"magenta")==0) col=35;
             if(strcmp(color,"cyan")==0) col=36;
             if(strcmp(color,"white")==0) col=37;
             if(col==0)
                {
                printf("color %s not suprted\n",color);
                exit(1);
                }
             break;
           case 'C':
             bgcol = 0;
             color = optarg;
             if(strcmp(color,"black")==0) bgcol=40;
             if(strcmp(color,"red")==0) bgcol=41;
             if(strcmp(color,"green")==0) bgcol=42;
             if(strcmp(color,"yellow")==0) bgcol=43;
             if(strcmp(color,"blue")==0) bgcol=44;
             if(strcmp(color,"magenta")==0) bgcol=45;
             if(strcmp(color,"cyan")==0) bgcol=46;
             if(strcmp(color,"white")==0) bgcol=47;
             if(bgcol==0)
                {
                printf("color %s not suprted\n",color);
                exit(1);
                }
             break;
           case 'h':
             printf ("%s",usage);
            return 0; 
           case '?':
             printf ("%s",usage);
             return 1;
           default:
             abort ();
           }


//**ctrl c handler**//
struct sigaction action;
memset(&action, 0, sizeof(action));
action.sa_handler = &sigint_handler;
sigaction(SIGINT, &action, &old_action);



//**getting h*w of term**//
ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
if(bands>(int)w.ws_col/2-1)bands=(int)w.ws_col/2-1; //handle for user setting to many bars
height=(int)w.ws_row-1;
width=(int)w.ws_col-bands-1;
int matrix[width][height];
for(i=0;i<width;i++)
{
    for(n=0;n<height;n++)
    {
    matrix[i][n]=0;
    }
}
bw=width/bands;


g=((float)height/1000)*pow((60/(float)framerate),2.5);//calculating gravity

//if no bands are selected it tries to padd the default 20 if there is extra room
if(autoband==1) bands=bands+(((w.ws_col)-(bw*bands+bands-1))/(bw+1));

//checks if there is stil extra room, will use this to center
rest=(((w.ws_col)-(bw*bands+bands-1)));
if(rest<0)rest=0;


printf("hoyde: %d bredde: %d bands:%d bandbredde: %d rest: %d\n",(int)w.ws_row,(int)w.ws_col,bands,bw,rest);

//**watintg for audio to be ready**//
n=0;
thr_id = pthread_create(&p_thread, NULL, music,(void*)device); //starting music listner
while (format==-1||rate==-1)
    {
    usleep(1000);
    n++;
    if(n>2000)
        {
        if(debug==1)printf("could not get rate and or format, problems with audoi thread? quiting...\n");
        exit(1);
        }
    }
if(debug==1)printf("got format: %d and rate %d\n",format,rate);
debug=0;
//**calculating cutof frequencies**/
for(n=0;n<bands+1;n++)
{ 
    fc[n]=10000*pow(10,-2.37+((((float)n+1)/((float)bands+1))*2.37));//decided to cut it at 10k, little interesting to hear above
    fr[n]=fc[n]/(rate/2); //remember nyquist!, pr my calculations this should be rate/2 and  nyquist freq in M/2 but testing shows it is not... or maybe the nq freq is in M/4
    lcf[n]=fr[n]*(M/4);  //lfc stores the lower cut frequency fro each band in the fft out buffer

    if(n!=0)
        {
        hcf[n-1]=lcf[n]-1;        
        if(lcf[n]<=lcf[n-1])lcf[n]=lcf[n-1]+1;//pushing the spectrum up if the expe function gets "clumped"
        hcf[n-1]=lcf[n]-1;
        }

    if(debug==1&&n!=0){printf("%d: %f -> %f (%d -> %d) \n",n,fc[n-1],fc[n],lcf[n-1],hcf[n-1]);}
}
//exit(1);

//constants to wigh signal to frequency
for(n=0;n<bands;n++)k[n]=((float)height*pow(log(lcf[n]+1),1.3))/(1024*(M/6));// the log(lcf[n]+2) is because higher frequencys are usally lower ine effect in music



p =  fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE); //planning to rock


//**preparing screen**//
if(debug==0){
system("setterm -cursor off");
system("setterm -blank 0");
//resetting console
printf("\033[0m\n");
system("clear");
printf("\033[%dm",col);//setting color

printf("\033[1m"); //setting "bright" color mode, looks cooler... I think
if(bgcol!=0)
printf("\033[%dm",bgcol);
    {
    for (n=(height);n>=0;n--)
            {
                 for (i=0;i<width+bands;i++)
                {

                printf(" ",bgcol);//setting backround volor

                }
            printf("\n");//setting volor
            }
    printf("\033[%dA",height);//backup
    }
}



//**start main loop**//
while  (1)
{
  if(debug==1){ system("clear");}


//**populating input buffer & checking if there is sound**//
     lpeak=0;
     hpeak=0;
    for (i=0;i<(2*(M/2+1));i++)
    {
       if(i<M)
        {
             in[i]=shared[i];
             if(shared[i]>hpeak) hpeak=shared[i];
             if(shared[i]<lpeak) lpeak=shared[i];
        }   
       else in[i]=0;
    // if(debug==1) printf("%d %f\n",i,in[i]);
    }
    peak[bands]=(hpeak+abs(lpeak));

    //**if sound go ahead with fft**//
    if (peak[bands]!=0)    
    {
        sleep=0; //wake if was sleepy
        
        fftw_execute(p);         //applying FFT to signal

        //seperating freq bands
        for(o=0;o<bands;o++)
        {        
            peak[o]=0;

            //getting peaks 
             for (i=lcf[o];i<=hcf[o];i++)
                {
                y[i]=pow(pow(*out[i][0],2)+pow(*out[i][1],2),0.5); //getting r of compex
                if(y[i]>peak[o]) peak[o]=y[i];     
                }

            temp=peak[o]*k[o]*((float)sens/100); //multiplying with k and adjusting to sens settings
            if(temp>height)temp=height;//just in case
            
            //**falloff function**//
            if(temp<flast[o])   
                {
                f[o]=fpeak[o]-(g*fall[o]*fall[o]);
                fall[o]++;
                }
            else if (temp>=flast[o])
                {
                f[o]=temp;
                fpeak[o]=f[o];                
                fall[o]=0;
                }

            flast[o]=f[o]; //memmory for falloff func

            
            if(f[o]<0.125)f[o]=0.125;
            if(debug==1){ printf("%d: f:%f->%f (%d->%d)peak:%f adjpeak: %f \n",o,fc[o],fc[o+1],lcf[o],hcf[o],peak[o],f[o]);}
        }
     // if(debug==1){ printf("topp overall unfiltered:%f \n",peak[bands]); }
 
       
       //if(debug==1){ printf("topp overall alltime:%f \n",sum);}
    }
    else//**if no signal don't bother**//
    {
        if (sleep>(rate*3)/M)//if no signal for 3 sec, go to sleep mode
        {
            if(debug==1)printf("no sound detected for 5 sec, going to sleep mode\n");
            for (i=0;i<200;i++)flast[i]=0; //zeroing memory
            //pthread_cancel(thr_id);// this didnt work to well, killing sound thread
            usleep(1*1000000);//wait 1 sec, then check sound again. 
            continue;
        }
        if(debug==1)printf("no sound detected, trying again\n");  
        sleep++;
        continue;
    }

//**DRAWING**// -- put in function file maybe?
if (debug==0)
{
 int redpwm = 100-200*f[0]/height;
 if(redpwm < 0)
    redpwm = 0;
 printf( "\x1b[31m %d ", redpwm);

 int greenpwm = 100-200*f[1]/height;
 if(greenpwm < 0)
    greenpwm = 0;
 printf( "\x1b[32m %d ", greenpwm);

 int bluepwm = 100-200*f[2]/height;
 if(bluepwm < 0)
    bluepwm = 0;
 printf( "\x1b[34m %d ", bluepwm);

 softPwmWrite(RED_PIN, redpwm);
 softPwmWrite(GREEN_PIN, redpwm);
 softPwmWrite(BLUE_PIN, redpwm);




 printf("\n");//next line
 printf("\033[%dA",height);//backup
 usleep((1/(float)framerate)*1000000);//sleeping for set us

}


}
return 0;
}
    
