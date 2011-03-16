/*
 * This config file is for Rockbox as an application on Android
 */
#define TARGET_TREE /* this target is using the target tree system */

/* We don't run on hardware directly */
#define CONFIG_PLATFORM (PLATFORM_HOSTED|PLATFORM_ANDROID)

/* For Rolo and boot loader */
#define MODEL_NUMBER 100

#define MODEL_NAME   "Rockbox"

#define USB_NONE

/* define this if you have a bitmap LCD display */
#define HAVE_LCD_BITMAP

/* define this if you have a colour LCD */
#define HAVE_LCD_COLOR

/* define this if you want album art for this target */
#define HAVE_ALBUMART

/* define this to enable bitmap scaling */
#define HAVE_BMP_SCALING

/* define this to enable JPEG decoding */
#define HAVE_JPEG

/* define this if you have access to the quickscreen */
#define HAVE_QUICKSCREEN
/* define this if you have access to the pitchscreen */
#define HAVE_PITCHSCREEN

/* define this if you would like tagcache to build on this target */
#define HAVE_TAGCACHE

/* LCD dimensions
 *
 * overriden by configure for application builds */
#ifndef LCD_WIDTH
#define LCD_WIDTH  320
#endif

#ifndef LCD_HEIGHT
#define LCD_HEIGHT 480
#endif

#define LCD_DEPTH  16
#define LCD_PIXELFORMAT 565

#define HAVE_LCD_ENABLE

/* define this to indicate your device's keypad */
#define HAVE_TOUCHSCREEN
#define HAVE_BUTTON_DATA

/* define this if you have RTC RAM available for settings */
//#define HAVE_RTC_RAM

/* define this if you have a real-time clock */
#define CONFIG_RTC APPLICATION

/* The number of bytes reserved for loadable codecs */
#define CODEC_SIZE 0x100000

/* The number of bytes reserved for loadable plugins */
#define PLUGIN_BUFFER_SIZE 0x80000

#define AB_REPEAT_ENABLE

/* Define this if you do software codec */
#define CONFIG_CODEC SWCODEC

#define HAVE_MULTIMEDIA_KEYS
#define CONFIG_KEYPAD ANDROID_PAD

/* define this if the target has volume keys which can be used in the lists */
#define HAVE_VOLUME_IN_LIST

/* define this if the host platform can change volume outside of rockbox */
#define PLATFORM_HAS_VOLUME_CHANGE

#define HAVE_SW_TONE_CONTROLS 

/* Define current usage levels. */
#define CURRENT_NORMAL     88 /* 18 hours from a 1600 mAh battery */  
#define CURRENT_BACKLIGHT  30 /* TBD */ 
#define CURRENT_RECORD     0  /* no recording yet */ 

/* Define this to the CPU frequency */
/*
#define CPU_FREQ 48000000
*/

/* Offset ( in the firmware file's header ) to the file CRC */
#define FIRMWARE_OFFSET_FILE_CRC 0

/* Offset ( in the firmware file's header ) to the real data */
#define FIRMWARE_OFFSET_FILE_DATA 8

#define CONFIG_LCD LCD_COWOND2

/* Define this if a programmable hotkey is mapped */
#define HAVE_HOTKEY

#define BOOTDIR "/.rockbox"
