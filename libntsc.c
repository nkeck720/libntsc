/*
 * libntsc - a library for generating NTSC B/W video with the sound card
 * under Linux
 */

/* Includes */
#include <alsa/asoundlib.h>

/* Predefs */
struct scanlineParams {
  unsigned int samplesScanline;
  unsigned int samplesVideo;
  unsigned int samplesHTip;
  unsigned int samplesHPorch;
  unsigned int samplesVideoPorch;
};


/* Globals and externs */
#define LIBNTSC_SAMPLE_RATE 192000
#define LIBNTSC_SAMPLE_FORMAT SND_PCM_FORMAT_S16_LE
#define LIBNTSC_HSYNC_TIP_US 4.7
#define LIBNTSC_HSYNC_PORCH_US 5.9
#define LIBNTSC_VIDEO_ACTIVE_US 51.5
#define LIBNTSC_VIDEO_PORCH_US 1.4
#define LIBNTSC_SCANLINE_US 63.5
#define LIBNTSC_BLANK_LEVEL 0
#define LIBNTSC_BLACK_LEVEL 9830
#define LIBNTSC_WHITE_LEVEL 32767

unsigned char** frameBuf;
signed short* sndBuf;
unsigned int curScanline;
enum scanMode {vsync, hsync, video} mode;
unsigned int curSample;
unsigned int samplesLeft;   // At end of ALSA frame
snd_pcm_t* cardHandle;
snd_pcm_hw_params_t* cardSettings;
int dir;
unsigned int rate = LIBNTSC_SAMPLE_RATE;
struct scanlineParams lineParams;
enum linesToSend {oddLines, evenLines} linesToScan;

/*
 * exponent() - uses integers to perform exponent calculations
 */

int exponent(int base, int exponent) {
  int result;
  if(exponent == 0) {
    return 1;
  }
  else if(exponent < 0) {
    for(int i = 0; i < -(exponent); i++) {
      result *= base;
    }
    result = 1 / result;
    return result;
  }
  for(int i = 0; i < exponent; i++) {
    result *= base;
  }
  return result;
}

/*
 * initNtsc() - initializes the soundcard and allocates framebuffer
 * NOTE: to avoid memory leaks, the lib must be closed with closeNtsc()
 * Returns: 0 on success, -1 on fail to find card, -2 on fail to set card parameters, -3 on fail to allocate framebuffer
 */

int initNtsc(void) {
  /* Init the soundcard under ALSA */
  if(snd_pcm_open(&cardHandle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    /* Error out now */
    return -1;
  }
  /* Allocate settings object */
  snd_pcm_hw_params_alloca(&cardSettings);
  snd_pcm_hw_params_any(cardHandle, cardSettings);
  /* Set samplerate and bit depth */
  snd_pcm_hw_params_set_access(cardHandle, cardSettings, SND_PCM_ACCESS_RW_INTERLEAVED); // Internal only; no effect on user
  snd_pcm_hw_params_set_format(cardHandle, cardSettings, LIBNTSC_SAMPLE_FORMAT);
  snd_pcm_hw_params_set_channels(cardHandle, cardSettings, 2); // Stereo, LEFT for audio, RIGHT for video
  snd_pcm_hw_params_set_rate_near(cardHandle, cardSettings, &rate, &dir);
  /* Allocate the framebuffer. This does calculations for samples per scanline, etc. also */
  float secondsPerLine = ((int)LIBNTSC_SCANLINE_US * exponent(10, -6));  // Gives seconds per line in integer form
  lineParams.samplesScanline = (int)(secondsPerLine * LIBNTSC_SAMPLE_RATE);
  secondsPerLine = ((int)LIBNTSC_HSYNC_TIP_US * exponent(10, -6));
  lineParams.samplesHTip = (int)(secondsPerLine * LIBNTSC_SAMPLE_RATE);
  secondsPerLine = ((int)LIBNTSC_HSYNC_PORCH_US * exponent(10, -6));
  lineParams.samplesHPorch = (int)(secondsPerLine * LIBNTSC_SAMPLE_RATE);
  secondsPerLine = ((int)LIBNTSC_VIDEO_ACTIVE_US * exponent(10, -6));
  lineParams.samplesVideo = (int)(secondsPerLine * LIBNTSC_SAMPLE_RATE);
  secondsPerLine = ((int)LIBNTSC_VIDEO_PORCH_US * exponent(10, -6));
  lineParams.samplesVideoPorch = (int)(secondsPerLine * LIBNTSC_SAMPLE_RATE);

  frameBuf = (unsigned char **)malloc(483 * sizeof(unsigned char *));
  if(!frameBuf) {
    return -3;
  }
  for(int i = 0; i < 483; i++) {
    frameBuf[i] = (unsigned char *)malloc(lineParams.samplesVideo * sizeof(unsigned char));
    if(!frameBuf[i]) {
      return -3;
    }
  }
  /* Set the soundcard's period length to that of one scanline */
  int perSize = (int)(lineparams.samplesScanline / 8);
  snd_pcm_set_period_size_near(cardHandle, cardSettings, &perSize, &dir);
  int fra
  /* Write the settings */
  if(snd_pcm_hw_params(cardHandle, cardSettings) < 0) {
    return -2;
  }
  /* Set initial values for the control variables */
  mode = vsync;
  linesToScan = oddLines;
  curScanline = 0;
  curSample = 0;
  /* We are done here */
  return 0;
}

/*
 * closeNtsc() - deacivates sound card and frees the framebuffer
 * Returns: 0 on success
 */

int closeNtsc(void) {
  snd_pcm_close(cardHandle);
  for(int i = 0; i < 483; i++) {
    free(frameBuf[i]);
  }
  free(frameBuf);
  free(sndBuf);
  return 0;
}

/*
 * sendScanline() - sends the next scanline to the soundcard
 * Returns: 0 on success
 */

int sendScanline(void) {
  if(mode == vsync) {
    for(int i = 0; i < lineparams.samplesHPorch * 2; i+2) {
      sndBuf[i] = (signed short)0;  // left (audio) channel
      sndBuf[i+1] = (signed short)LIBNTSC_BLANK_LEVEL; // right (video) channel
    }
    for(int i = 0; i < lineparams.samplesHTip * 2; i+2) {
      sndBuf[i] = (signed short)0;
      sndBuf[i+1] = (signed short)LIBNTSC_BLACK_LEVEL;
    }
    for(int i = 0; i < lineparams.samplesVideo * 2; i+2) {
      sndBuf[i] = (signed short)0;
      sndBuf[i+1] = (signed short)LIBNTSC_BLANK_LEVEL;
    }
    for(int i = 0; i < lineparams.samplesVideoPorch * 2; i+2) {
      sndBuf[i] = (signed short)0;
      sndBuf[i+1] = (signed short)LIBNTSC_BLANK_LEVEL;
    }
    curScanline++;
    
  }
  else if(mode == hsync) {
    
}
