/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by wavey@wavey.org
 * RTC config saving code (C) 2002 by hessu@hes.iki.fi
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include <stdio.h>
#include "config.h"
#include "kernel.h"
#include "thread.h"
#include "settings.h"
#include "disk.h"
#include "panic.h"
#include "debug.h"
#include "button.h"
#include "usb.h"
#include "backlight.h"
#include "lcd.h"
#include "mpeg.h"
#include "string.h"
#include "ata.h"
#include "fat.h"
#include "power.h"
#include "backlight.h"
#include "powermgmt.h"
#include "status.h"
#include "atoi.h"
#include "screens.h"
#include "ctype.h"
#include "file.h"
#include "errno.h"
#include "system.h"
#ifdef HAVE_LCD_BITMAP
#include "icons.h"
#include "font.h"
#include "peakmeter.h"
#endif
#include "lang.h"
#include "language.h"
#include "wps-display.h"
#include "powermgmt.h"
#include "sprintf.h"

struct user_settings global_settings;
char rockboxdir[] = ROCKBOX_DIR;       /* config/font/data file directory */

#define CONFIG_BLOCK_VERSION 4
#define CONFIG_BLOCK_SIZE 512
#define RTC_BLOCK_SIZE 44

#ifdef HAVE_LCD_BITMAP
#define MAX_LINES 10
#else
#define MAX_LINES 2
#endif

/********************************************

Config block as saved on the battery-packed RTC user RAM memory block
of 44 bytes, starting at offset 0x14 of the RTC memory space.

offset  abs
0x00    0x14    "Roc"   header signature: 0x52 0x6f 0x63
0x03    0x17    <version byte: 0x0>
0x04    0x18    <volume byte>
0x05    0x19    <balance byte>
0x06    0x1a    <bass byte>
0x07    0x1b    <treble byte>
0x08    0x1c    <loudness byte>
0x09    0x1d    <bass boost byte>
0x0a    0x1e    <contrast byte>
0x0b    0x1f    <backlight_on_when_charging, backlight_timeout>
0x0c    0x20    <poweroff timer byte>
0x0d    0x21    <resume settings byte>
0x0e    0x22    <shuffle,dirfilter,sort_case,discharge,statusbar,show_hidden,
                 scroll bar>
0x0f    0x23    <volume type, battery type, timeformat, scroll speed>
0x10    0x24    <ff/rewind min step, acceleration rate>
0x11    0x25    <AVC, channel config>
0x12    0x26    <(int) Resume playlist index, or -1 if no playlist resume>
0x16    0x2a    <(int) Byte offset into resume file>
0x1a    0x2e    <time until disk spindown>
0x1b    0x2f    <browse current, play selected, queue_resume>
0x1c    0x30    <peak meter hold timeout (bit 0-4)>, 
                 peak_meter_performance (bit 7)
0x1d    0x31    <(int) queue resume index>
0x21    0x35    <repeat mode (bit 0-1), rec. channels (bit 2),
                 mic gain (bit 4-7)>
0x22    0x36    <rec. quality (bit 0-2), source (bit 3-4), frequency (bit 5-7)>
0x23    0x37    <rec. left gain (bit 0-3)>
0x24    0x38    <rec. right gain (bit 0-3)>
0x25    0x39    <disk poweroff flag (bit 0), MP3 buffer margin (bit 1-3),
                 Trickle charge flag (bit 4), CPU sleep flag (bit 5)>
0x26    0x40    <runtime low byte>
0x27    0x41    <runtime high byte>
0x28    0x42    <topruntime low byte>
0x29    0x43    <topruntime high byte>

0x2a    <checksum 2 bytes: xor of 0x0-0x29>

Config memory is reset to 0xff and initialized with 'factory defaults' if
a valid header & checksum is not found. Config version number is only
increased when information is _relocated_ or space is _reused_ so that old
versions can read and modify configuration changed by new versions. New
versions should check for the value of '0xff' in each config memory
location used, and reset the setting in question with a factory default if
needed. Memory locations not used by a given version should not be
modified unless the header & checksum test fails.


Rest of config block, only saved to disk:
0xAE  fade on pause/unpause/stop setting (bit 0)
0xB0  peak meter clip hold timeout (bit 0-4)
0xB1  peak meter release step size, peak_meter_dbfs (bit 7)
0xB2  peak meter min either in -db or in percent
0xB3  peak meter max either in -db or in percent
0xB4  battery capacity
0xB5  scroll step in pixels
0xB6  scroll start and endpoint delay
0xB7  bidir scroll setting (bidi if 0-200% longer than screen width)
0xB8  (char[20]) WPS file
0xCC  (char[20]) Lang file
0xE0  (char[20]) Font file
0xF4  (int) Playlist first index
0xF8  (int) Playlist shuffle seed
0xFC  (char[260]) Resume playlist (path/to/dir or path/to/playlist.m3u)

*************************************/

#include "rtc.h"
static unsigned char config_block[CONFIG_BLOCK_SIZE];

/*
 * Calculates the checksum for the config block and returns it
 */

static unsigned short calculate_config_checksum(unsigned char* buf)
{
    unsigned int i;
    unsigned char cksum[2];
    cksum[0] = cksum[1] = 0;
    
    for (i=0; i < RTC_BLOCK_SIZE - 2; i+=2 ) {
        cksum[0] ^= buf[i];
        cksum[1] ^= buf[i+1];
    }

    return (cksum[0] << 8) | cksum[1];
}

/*
 * initialize the config block buffer
 */
static void init_config_buffer( void )
{
    DEBUGF( "init_config_buffer()\n" );
    
    /* reset to 0xff - all unused */
    memset(config_block, 0xff, CONFIG_BLOCK_SIZE);
    /* insert header */
    config_block[0] = 'R';
    config_block[1] = 'o';
    config_block[2] = 'c';
    config_block[3] = CONFIG_BLOCK_VERSION;
}

/*
 * save the config block buffer to disk or RTC RAM
 */
static int save_config_buffer( void )
{
    unsigned short chksum;
#ifdef HAVE_RTC
    unsigned int i;
#endif

    DEBUGF( "save_config_buffer()\n" );
    
    /* update the checksum in the end of the block before saving */
    chksum = calculate_config_checksum(config_block);
    config_block[ RTC_BLOCK_SIZE - 2 ] = chksum >> 8;
    config_block[ RTC_BLOCK_SIZE - 1 ] = chksum & 0xff;

#ifdef HAVE_RTC    
    /* FIXME: okay, it _would_ be cleaner and faster to implement rtc_write so
       that it would write a number of bytes at a time since the RTC chip
       supports that, but this will have to do for now 8-) */
    for (i=0; i < RTC_BLOCK_SIZE; i++ ) {
        int r = rtc_write(0x14+i, config_block[i]);
        if (r) {
            DEBUGF( "save_config_buffer: rtc_write failed at addr 0x%02x: %d\n",
                    14+i, r );
            return r;
        }
    }

#endif

    if (fat_startsector() != 0)
        ata_delayed_write( 61, config_block);
    else
        return -1;

    return 0;
}

/*
 * load the config block buffer from disk or RTC RAM
 */
static int load_config_buffer( void )
{
    unsigned short chksum;
    bool correct = false;

#ifdef HAVE_RTC
    unsigned int i;
    unsigned char rtc_block[RTC_BLOCK_SIZE];
#endif
    
    DEBUGF( "load_config_buffer()\n" );

    if (fat_startsector() != 0) {
        ata_read_sectors( 61, 1,  config_block);

        /* calculate the checksum, check it and the header */
        chksum = calculate_config_checksum(config_block);
        
        if (config_block[0] == 'R' &&
            config_block[1] == 'o' &&
            config_block[2] == 'c' &&
            config_block[3] == CONFIG_BLOCK_VERSION &&
            (chksum >> 8) == config_block[RTC_BLOCK_SIZE - 2] &&
            (chksum & 0xff) == config_block[RTC_BLOCK_SIZE - 1])
        {
            DEBUGF( "load_config_buffer: header & checksum test ok\n" );
            correct = true;
        }
    }

#ifdef HAVE_RTC    
    /* read rtc block */
    for (i=0; i < RTC_BLOCK_SIZE; i++ )
        rtc_block[i] = rtc_read(0x14+i);

    chksum = calculate_config_checksum(rtc_block);
    
    /* if rtc block is ok, use that */
    if (rtc_block[0] == 'R' &&
        rtc_block[1] == 'o' &&
        rtc_block[2] == 'c' &&
        rtc_block[3] == CONFIG_BLOCK_VERSION &&
        (chksum >> 8) == rtc_block[RTC_BLOCK_SIZE - 2] &&
        (chksum & 0xff) == rtc_block[RTC_BLOCK_SIZE - 1])
    {
        memcpy(config_block, rtc_block, RTC_BLOCK_SIZE);
        correct = true;
    }
#endif
    
    if ( !correct ) {
        /* if checksum is not valid, clear the config buffer */
        DEBUGF( "load_config_buffer: header & checksum test failed\n" );
        init_config_buffer();
        return -1;
    }

    return 0;
}

/*
 * persist all runtime user settings to disk or RTC RAM
 */
int settings_save( void )
{
    DEBUGF( "settings_save()\n" );
    
    /* update the config block buffer with current
       settings and save the block in the RTC */
    config_block[0x4] = (unsigned char)global_settings.volume;
    config_block[0x5] = (char)global_settings.balance;
    config_block[0x6] = (unsigned char)global_settings.bass;
    config_block[0x7] = (unsigned char)global_settings.treble;
    config_block[0x8] = (unsigned char)global_settings.loudness;
    config_block[0x9] = (unsigned char)global_settings.bass_boost;
    
    config_block[0xa] = (unsigned char)global_settings.contrast;

    config_block[0xb] = (unsigned char)
        ((global_settings.backlight_on_when_charging?0x40:0) |
         (global_settings.backlight_timeout & 0x3f));
    config_block[0xc] = (unsigned char)global_settings.poweroff;
    config_block[0xd] = (unsigned char)global_settings.resume;
    
    config_block[0xe] = (unsigned char)
        ((global_settings.playlist_shuffle & 1) |
         ((global_settings.dirfilter & 1) << 1) |
         ((global_settings.sort_case & 1) << 2) |
         ((global_settings.discharge & 1) << 3) |
         ((global_settings.statusbar & 1) << 4) |
         ((global_settings.dirfilter & 2) << 4) |
         ((global_settings.scrollbar & 1) << 6));
    
    config_block[0xf] = (unsigned char)
        ((global_settings.volume_type & 1) |
         ((global_settings.battery_type & 1) << 1) |
         ((global_settings.timeformat & 1)   << 2) |
         ( global_settings.scroll_speed      << 3));
    
    config_block[0x10] = (unsigned char)
        ((global_settings.ff_rewind_min_step & 15) << 4 |
         (global_settings.ff_rewind_accel & 15));
    config_block[0x11] = (unsigned char)
        ((global_settings.avc & 0x03) | 
         ((global_settings.channel_config & 0x03) << 2));

    memcpy(&config_block[0x12], &global_settings.resume_index, 4);
    memcpy(&config_block[0x16], &global_settings.resume_offset, 4);

    config_block[0x1a] = (unsigned char)global_settings.disk_spindown;
    config_block[0x1b] = (unsigned char)
        (((global_settings.browse_current & 1)) |
         ((global_settings.play_selected & 1) << 1) |
         ((global_settings.queue_resume & 3) << 2));
    
    config_block[0x1c] = (unsigned char)global_settings.peak_meter_hold;

    memcpy(&config_block[0x1d], &global_settings.queue_resume_index, 4);

    config_block[0x21] = (unsigned char)
        ((global_settings.repeat_mode & 3) |
         ((global_settings.rec_channels & 1) << 2) |
         ((global_settings.rec_mic_gain & 0x0f) << 4));
    config_block[0x22] = (unsigned char)
        ((global_settings.rec_quality & 7) |
         ((global_settings.rec_source & 1) << 3) |
         ((global_settings.rec_frequency & 7) << 5));
    config_block[0x23] = (unsigned char)global_settings.rec_left_gain;
    config_block[0x24] = (unsigned char)global_settings.rec_right_gain;
    config_block[0x25] = (unsigned char)
        ((global_settings.disk_poweroff & 1) |
         ((global_settings.buffer_margin & 7) << 1) |
         ((global_settings.trickle_charge & 1) << 4) |
         ((global_settings.cpu_sleep & 1) << 5));

    {
        static long lasttime = 0;

        global_settings.runtime += (current_tick - lasttime) / HZ;
        lasttime = current_tick;

        if ( global_settings.runtime > global_settings.topruntime )
            global_settings.topruntime = global_settings.runtime;

        config_block[0x26]=(unsigned char)(global_settings.runtime & 0xff);
        config_block[0x27]=(unsigned char)(global_settings.runtime >> 8);
        config_block[0x28]=(unsigned char)(global_settings.topruntime & 0xff);
        config_block[0x29]=(unsigned char)(global_settings.topruntime >> 8);
    }
    
    config_block[0xae] = (unsigned char)global_settings.fade_on_stop;
    config_block[0xb0] = (unsigned char)global_settings.peak_meter_clip_hold |
        (global_settings.peak_meter_performance ? 0x80 : 0);
    config_block[0xb1] = global_settings.peak_meter_release |
        (global_settings.peak_meter_dbfs ? 0x80 : 0);
    config_block[0xb2] = (unsigned char)global_settings.peak_meter_min;
    config_block[0xb3] = (unsigned char)global_settings.peak_meter_max;

    config_block[0xb4]=(global_settings.battery_capacity - 1000) / 50;
    config_block[0xb5]=(unsigned char)global_settings.scroll_step;
    config_block[0xb6]=(unsigned char)global_settings.scroll_delay;
    config_block[0xb7]=(unsigned char)global_settings.bidir_limit;

    strncpy(&config_block[0xb8], global_settings.wps_file, MAX_FILENAME);
    strncpy(&config_block[0xcc], global_settings.lang_file, MAX_FILENAME);
    strncpy(&config_block[0xe0], global_settings.font_file, MAX_FILENAME);
    memcpy(&config_block[0xF4], &global_settings.resume_first_index, 4);
    memcpy(&config_block[0xF8], &global_settings.resume_seed, 4);

    strncpy(&config_block[0xFC], global_settings.resume_file, MAX_PATH);
    
    DEBUGF( "+Resume file %s\n",global_settings.resume_file );
    DEBUGF( "+Resume index %X offset %X\n",
            global_settings.resume_index,
            global_settings.resume_offset );
    DEBUGF( "+Resume shuffle %s seed %X\n",
            global_settings.playlist_shuffle?"on":"off",
            global_settings.resume_seed );

    if(save_config_buffer())
    {
        lcd_clear_display();
#ifdef HAVE_LCD_CHARCELLS
        lcd_puts(0, 0, str(LANG_SETTINGS_SAVE_PLAYER));
        lcd_puts(0, 1, str(LANG_SETTINGS_BATTERY_PLAYER));
#else
        lcd_puts(4, 2, str(LANG_SETTINGS_SAVE_RECORDER));
        lcd_puts(2, 4, str(LANG_SETTINGS_BATTERY_RECORDER));
        lcd_update();
#endif
        sleep(HZ*2);
        return -1;
    }
    return 0;
}

#ifdef HAVE_LCD_BITMAP
/**
 * Applies the range infos stored in global_settings to 
 * the peak meter. 
 */
void settings_apply_pm_range(void)
{
    int pm_min, pm_max;

    /* depending on the scale mode (dBfs or percent) the values
       of global_settings.peak_meter_dbfs have different meanings */
    if (global_settings.peak_meter_dbfs) 
    {
        /* convert to dBfs * 100          */
        pm_min = -(((int)global_settings.peak_meter_min) * 100);
        pm_max = -(((int)global_settings.peak_meter_max) * 100);
    }
    else 
    {
        /* percent is stored directly -> no conversion */
        pm_min = global_settings.peak_meter_min;
        pm_max = global_settings.peak_meter_max;
    }

    /* apply the range */
    peak_meter_init_range(global_settings.peak_meter_dbfs, pm_min, pm_max);
}
#endif /* HAVE_LCD_BITMAP */

void settings_apply(void)
{
    char buf[64];

    mpeg_sound_set(SOUND_BASS, global_settings.bass);
    mpeg_sound_set(SOUND_TREBLE, global_settings.treble);
    mpeg_sound_set(SOUND_BALANCE, global_settings.balance);
    mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
    mpeg_sound_set(SOUND_CHANNELS, global_settings.channel_config);
    
#ifdef HAVE_MAS3587F
    mpeg_sound_set(SOUND_LOUDNESS, global_settings.loudness);
    mpeg_sound_set(SOUND_SUPERBASS, global_settings.bass_boost);
    mpeg_sound_set(SOUND_AVC, global_settings.avc);
#endif

    lcd_set_contrast(global_settings.contrast);
    lcd_scroll_speed(global_settings.scroll_speed);
    backlight_set_timeout(global_settings.backlight_timeout);
    backlight_set_on_when_charging(global_settings.backlight_on_when_charging);
    ata_spindown(global_settings.disk_spindown);

#ifdef HAVE_ATA_POWER_OFF
    ata_poweroff(global_settings.disk_poweroff);
#endif

    set_poweroff_timeout(global_settings.poweroff);
#ifdef HAVE_CHARGE_CTRL
    charge_restart_level = global_settings.discharge ? 
        CHARGE_RESTART_LO : CHARGE_RESTART_HI;
    enable_trickle_charge(global_settings.trickle_charge);
#endif

    set_battery_capacity(global_settings.battery_capacity);

#ifdef HAVE_LCD_BITMAP
    settings_apply_pm_range();
    peak_meter_init_times(
        global_settings.peak_meter_release, global_settings.peak_meter_hold, 
        global_settings.peak_meter_clip_hold);
#endif

    if ( global_settings.wps_file[0] && 
         global_settings.wps_file[0] != 0xff ) {
        snprintf(buf, sizeof buf, ROCKBOX_DIR "/%s.wps",
                 global_settings.wps_file);
        wps_load(buf, false);
    }
    else
        wps_reset();

#ifdef HAVE_LCD_BITMAP
    if ( global_settings.font_file[0] &&
         global_settings.font_file[0] != 0xff ) {
        snprintf(buf, sizeof buf, ROCKBOX_DIR "/%s.fnt",
                 global_settings.font_file);
        font_load(buf);
    }
    else
        font_reset();

    lcd_scroll_step(global_settings.scroll_step);
#endif
    lcd_bidir_scroll(global_settings.bidir_limit);
    lcd_scroll_delay(global_settings.scroll_delay * (HZ/10));

    if ( global_settings.lang_file[0] &&
         global_settings.lang_file[0] != 0xff ) {
        snprintf(buf, sizeof buf, ROCKBOX_DIR "/%s.lng",
                 global_settings.lang_file);
        lang_load(buf);
    }

    cpu_sleep(global_settings.cpu_sleep);
}

/*
 * load settings from disk or RTC RAM
 */
void settings_load(void)
{
    
    DEBUGF( "reload_all_settings()\n" );

    /* populate settings with default values */
    settings_reset();
    
    /* load the buffer from the RTC (resets it to all-unused if the block
       is invalid) and decode the settings which are set in the block */
    if (!load_config_buffer()) {
        if (config_block[0x4] != 0xFF)
            global_settings.volume = config_block[0x4];
        if (config_block[0x5] != 0xFF)
            global_settings.balance = (char)config_block[0x5];
        if (config_block[0x6] != 0xFF)
            global_settings.bass = config_block[0x6];
        if (config_block[0x7] != 0xFF)
            global_settings.treble = config_block[0x7];
        if (config_block[0x8] != 0xFF)
            global_settings.loudness = config_block[0x8];
        if (config_block[0x9] != 0xFF)
            global_settings.bass_boost = config_block[0x9];
    
        if (config_block[0xa] != 0xFF) {
            global_settings.contrast = config_block[0xa];
            if ( global_settings.contrast < MIN_CONTRAST_SETTING )
                global_settings.contrast = DEFAULT_CONTRAST_SETTING;
        }

        if (config_block[0xb] != 0xFF) {
            /* Bit 7 is unused to be able to detect uninitialized entry */
            global_settings.backlight_timeout = config_block[0xb] & 0x3f;
            global_settings.backlight_on_when_charging =
                config_block[0xb] & 0x40 ? true : false;
        }

        if (config_block[0xc] != 0xFF)
            global_settings.poweroff = config_block[0xc];
        if (config_block[0xd] != 0xFF)
            global_settings.resume = config_block[0xd];
        if (config_block[0xe] != 0xFF) {
            global_settings.playlist_shuffle = config_block[0xe] & 1;
            global_settings.dirfilter = (config_block[0xe] >> 1) & 1;
            global_settings.sort_case = (config_block[0xe] >> 2) & 1;
            global_settings.discharge = (config_block[0xe] >> 3) & 1;
            global_settings.statusbar = (config_block[0xe] >> 4) & 1;
            global_settings.dirfilter |= ((config_block[0xe] >> 5) & 1) << 1;
            global_settings.scrollbar = (config_block[0xe] >> 6) & 1;
            /* Don't use the last bit, it must be unused to detect
               an uninitialized entry */
        }
        
        if (config_block[0xf] != 0xFF) {
            global_settings.volume_type = config_block[0xf] & 1;
            global_settings.battery_type = (config_block[0xf] >> 1) & 1;
            global_settings.timeformat  = (config_block[0xf] >> 2) & 1;
            global_settings.scroll_speed = config_block[0xf] >> 3;
        }

        if (config_block[0x10] != 0xFF) {
            global_settings.ff_rewind_min_step = (config_block[0x10] >> 4) & 15;
            global_settings.ff_rewind_accel = config_block[0x10] & 15;
        }

        if (config_block[0x11] != 0xFF)
        {
            global_settings.avc = config_block[0x11] & 0x03;
            global_settings.channel_config = (config_block[0x11] >> 2) & 0x03;
        }

        if (config_block[0x12] != 0xFF)
            memcpy(&global_settings.resume_index, &config_block[0x12], 4);

        if (config_block[0x16] != 0xFF)
            memcpy(&global_settings.resume_offset, &config_block[0x16], 4);

        if (config_block[0x1a] != 0xFF)
            global_settings.disk_spindown = config_block[0x1a];

        if (config_block[0x1b] != 0xFF) {
            global_settings.browse_current = (config_block[0x1b]) & 1;
            global_settings.play_selected = (config_block[0x1b] >> 1) & 1;
            global_settings.queue_resume = (config_block[0x1b] >> 2) & 3;
        }

        if (config_block[0x1c] != 0xFF)
            global_settings.peak_meter_hold = (config_block[0x1c]) & 0x1f;

        if (config_block[0x1d] != 0xFF)
            memcpy(&global_settings.queue_resume_index, &config_block[0x1d],
                4);

        if (config_block[0x21] != 0xFF)
        {
            global_settings.repeat_mode = config_block[0x21] & 3;
            global_settings.rec_channels = (config_block[0x21] >> 2) & 1;
            global_settings.rec_mic_gain = (config_block[0x21] >> 4) & 0x0f;
        }

        if (config_block[0x22] != 0xFF)
        {
            global_settings.rec_quality = config_block[0x22] & 7;
            global_settings.rec_source = (config_block[0x22] >> 3) & 3;
            global_settings.rec_frequency = (config_block[0x22] >> 5) & 7;
        }

        if (config_block[0x23] != 0xFF)
            global_settings.rec_left_gain = config_block[0x23] & 0x0f;

        if (config_block[0x24] != 0xFF)
            global_settings.rec_right_gain = config_block[0x24] & 0x0f;

        if (config_block[0x25] != 0xFF)
        {
            global_settings.disk_poweroff = config_block[0x25] & 1;
            global_settings.buffer_margin = (config_block[0x25] >> 1) & 7;
            global_settings.trickle_charge = (config_block[0x25] >> 4) & 1;
            global_settings.cpu_sleep = (config_block[0x25] >> 5) & 1;
        }

        if (config_block[0x27] != 0xff)
            global_settings.runtime = 
                config_block[0x26] | (config_block[0x27] << 8);

        if (config_block[0x29] != 0xff)
            global_settings.topruntime =
                config_block[0x28] | (config_block[0x29] << 8);

        global_settings.fade_on_stop=config_block[0xae];
	
        global_settings.peak_meter_clip_hold = (config_block[0xb0]) & 0x1f;
        global_settings.peak_meter_performance = 
            (config_block[0xb0] & 0x80) != 0;

        global_settings.peak_meter_release = config_block[0xb1] & 0x7f;
        global_settings.peak_meter_dbfs = (config_block[0xb1] & 0x80) != 0;

        global_settings.peak_meter_min = config_block[0xb2];
        global_settings.peak_meter_max = config_block[0xb3];

        global_settings.battery_capacity = config_block[0xb4]*50 + 1000;

        if (config_block[0xb5] != 0xff)
            global_settings.scroll_step = config_block[0xb5];

        if (config_block[0xb6] != 0xff)
            global_settings.scroll_delay = config_block[0xb6];

        if (config_block[0xb7] != 0xff)
            global_settings.bidir_limit = config_block[0xb7];

	if (config_block[0xae] != 0xff)
	    global_settings.fade_on_stop = config_block[0xae];
	
        memcpy(&global_settings.resume_first_index, &config_block[0xF4], 4);
        memcpy(&global_settings.resume_seed, &config_block[0xF8], 4);

        strncpy(global_settings.wps_file, &config_block[0xb8], MAX_FILENAME);
        strncpy(global_settings.lang_file, &config_block[0xcc], MAX_FILENAME);
        strncpy(global_settings.font_file, &config_block[0xe0], MAX_FILENAME);
        strncpy(global_settings.resume_file, &config_block[0xFC], MAX_PATH);
        global_settings.resume_file[MAX_PATH]=0;
    }

    settings_apply();
}

/* Read (up to) a line of text from fd into buffer and return number of bytes
 * read (which may be larger than the number of bytes stored in buffer). If 
 * an error occurs, -1 is returned (and buffer contains whatever could be 
 * read). A line is terminated by a LF char. Neither LF nor CR chars are 
 * stored in buffer.
 */
static int read_line(int fd, char* buffer, int buffer_size)
{
    int count = 0;
    int num_read = 0;
    
    errno = 0;

    while (count < buffer_size)
    {
        unsigned char c;

        if (1 != read(fd, &c, 1))
            break;
        
        num_read++;
            
        if ( c == '\n' )
            break;

        if ( c == '\r' )
            continue;

        buffer[count++] = c;
    }

    buffer[MIN(count, buffer_size - 1)] = 0;

    return errno ? -1 : num_read;
}

/* parse a line from a configuration file. the line format is: 

   setting name: setting value

   Any whitespace before setting name or value (after ':') is ignored.
   A # as first non-whitespace character discards the whole line.
   Function sets pointers to null-terminated setting name and value.
   Returns false if no valid config entry was found.
*/

static bool settings_parseline(char* line, char** name, char** value)
{
    char* ptr;

    while ( isspace(*line) )
        line++;

    if ( *line == '#' )
        return false;

    ptr = strchr(line, ':');
    if ( !ptr )
        return false;

    *name = line;
    *ptr = 0;
    ptr++;
    while (isspace(*ptr))
        ptr++;
    *value = ptr;
    return true;
}

static void set_sound(char* value, int type, int* setting)
{
    int num = atoi(value);

    num = mpeg_phys2val(type, num);

    if ((num > mpeg_sound_max(type)) ||
        (num < mpeg_sound_min(type)))
    {
        num = mpeg_sound_default(type);
    }

    *setting = num;
    mpeg_sound_set(type, num);

#ifdef HAVE_MAS3507D
    /* This is required to actually apply balance */
    if (SOUND_BALANCE == type)
        mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
#endif
}

static void set_cfg_bool(bool* variable, char* value)
{
    /* look for the 'n' in 'on' */
    if ((value[1] & 0xdf) == 'N') 
        *variable = true;
    else
        *variable = false;
}

static void set_cfg_int(int* variable, char* value, int min, int max )
{
    *variable = atoi(value);

    if (*variable < min)
        *variable = min;
    else
        if (*variable > max)
            *variable = max;
}

static void set_cfg_option(int* variable, char* value,
                           char* options[], int numoptions )
{
    int i;

    for (i=0; i<numoptions; i++) {
        if (!strcasecmp(options[i], value)) {
            *variable = i;
            break;
        }
    }
}

bool settings_load_config(char* file)
{
    int fd;
    char line[128];

    fd = open(file, O_RDONLY);
    if (-1 == fd)
        return false;

    while (read_line(fd, line, sizeof line) > 0)
    {
        char* name;
        char* value;

        if (!settings_parseline(line, &name, &value))
            continue;

        if (!strcasecmp(name, "volume"))
            set_sound(value, SOUND_VOLUME, &global_settings.volume);
        else if (!strcasecmp(name, "bass"))
            set_sound(value, SOUND_BASS, &global_settings.bass);
        else if (!strcasecmp(name, "treble"))
            set_sound(value, SOUND_TREBLE, &global_settings.treble);
        else if (!strcasecmp(name, "balance"))
            set_sound(value, SOUND_BALANCE, &global_settings.balance);
        else if (!strcasecmp(name, "channels"))
            set_sound(value, SOUND_CHANNELS, &global_settings.channel_config);
        else if (!strcasecmp(name, "wps"))
            strncpy(global_settings.wps_file, value, MAX_FILENAME);
        else if (!strcasecmp(name, "lang"))
            strncpy(global_settings.lang_file, value, MAX_FILENAME);
#ifdef HAVE_LCD_BITMAP
        else if (!strcasecmp(name, "font"))
            strncpy(global_settings.font_file, value, MAX_FILENAME);
        else if (!strcasecmp(name, "scroll step"))
            set_cfg_int(&global_settings.scroll_step, value, 1, LCD_WIDTH);
        else if (!strcasecmp(name, "bidir limit"))
            set_cfg_int(&global_settings.bidir_limit, value, 0, 200);
        else if (!strcasecmp(name, "statusbar"))
            set_cfg_bool(&global_settings.statusbar, value);
        else if (!strcasecmp(name, "peak meter release"))
            set_cfg_int(&global_settings.peak_meter_release, value, 1, 0x7e);
        else if (!strcasecmp(name, "peak meter hold")) {
            static char* options[] = {
                "off","200ms","300ms","500ms",
                "1","2","3","4","5","6","7","8","9","10",
                "15","20","25","30","45","60","90",
                "2min","3min","5min","10min","20min","45min","90min"};
            set_cfg_option(&global_settings.peak_meter_hold, value,
                           options, 28);
        }
        else if (!strcasecmp(name, "peak meter clip hold")) {
            static char* options[] = {
                "on","1","2","3","4","5","6","7","8","9","10",
                "15","20","25","30","45","60","90",
                "2min","3min","5min","10min","20min","45min","90min"};
            set_cfg_option(&global_settings.peak_meter_clip_hold, value,
                           options, 25);
        }
        else if (!strcasecmp(name, "peak meter dbfs"))
            set_cfg_bool(&global_settings.peak_meter_dbfs, value);
        else if (!strcasecmp(name, "peak meter min"))
            set_cfg_int(&global_settings.peak_meter_min, value, 0, 100);
        else if (!strcasecmp(name, "peak meter max"))
            set_cfg_int(&global_settings.peak_meter_max, value, 0, 100);
        else if (!strcasecmp(name, "peak meter busy"))
            set_cfg_bool(&global_settings.peak_meter_performance, value);
#endif
        else if (!strcasecmp(name, "shuffle"))
            set_cfg_bool(&global_settings.playlist_shuffle, value);
        else if (!strcasecmp(name, "repeat")) {
            static char* options[] = {"off", "all", "one"};
            set_cfg_option(&global_settings.repeat_mode, value, options, 3);
        }
        else if (!strcasecmp(name, "resume"))
            set_cfg_int(&global_settings.resume, value, 0, 3);
        else if (!strcasecmp(name, "sort case"))
            set_cfg_bool(&global_settings.sort_case, value);
        else if (!strcasecmp(name, "show files")) {
            static char* options[] = {"all", "supported","music"};
            set_cfg_option(&global_settings.dirfilter, value, options, 3);
        }
        else if (!strcasecmp(name, "follow playlist"))
            set_cfg_bool(&global_settings.browse_current, value);
        else if (!strcasecmp(name, "play selected"))
            set_cfg_bool(&global_settings.play_selected, value);
        else if (!strcasecmp(name, "volume display")) {
            static char* options[] = {"graphic", "numeric"};
            set_cfg_option(&global_settings.volume_type, value, options, 2);
        }
        else if (!strcasecmp(name, "battery display")) {
            static char* options[] = {"graphic", "numeric"};
            set_cfg_option(&global_settings.battery_type, value, options, 2);
        }
        else if (!strcasecmp(name, "time format")) {
            static char* options[] = {"24hour", "12hour"};
            set_cfg_option(&global_settings.timeformat, value, options, 2);
        }
        else if (!strcasecmp(name, "contrast"))
            set_cfg_int(&global_settings.contrast, value,
                        0, MAX_CONTRAST_SETTING);
        else if (!strcasecmp(name, "scrollbar"))
            set_cfg_bool(&global_settings.scrollbar, value);
        else if (!strcasecmp(name, "scroll speed"))
            set_cfg_int(&global_settings.scroll_speed, value, 1, 10);
        else if (!strcasecmp(name, "scan min step")) {
            static char* options[] =
                {"1","2","3","4","5","6","8","10","15","20","25"};
            set_cfg_option(&global_settings.ff_rewind_min_step, value,
                           options, 11);
        }
        else if (!strcasecmp(name, "scan accel"))
            set_cfg_int(&global_settings.ff_rewind_min_step, value, 0, 15);
        else if (!strcasecmp(name, "scroll delay"))
            set_cfg_int(&global_settings.scroll_delay, value, 0, 250);
        else if (!strcasecmp(name, "backlight timeout")) {
            static char* options[] = {
                "off","on","1","2","3","4","5","6","7","8","9",
                "10","15","20","30","45","60","90"};
            set_cfg_option(&global_settings.backlight_timeout, value,
                           options, 18);
        }
        else if (!strcasecmp(name, "backlight when plugged"))
            set_cfg_bool(&global_settings.backlight_on_when_charging, value);
        else if (!strcasecmp(name, "antiskip"))
            set_cfg_int(&global_settings.buffer_margin, value, 0, 7);
        else if (!strcasecmp(name, "disk spindown"))
            set_cfg_int(&global_settings.disk_spindown, value, 3, 254);
#ifdef HAVE_ATA_POWER_OFF
        else if (!strcasecmp(name, "disk poweroff"))
            set_cfg_bool(&global_settings.disk_poweroff, value);
#endif
#ifdef HAVE_MAS3587F
        else if (!strcasecmp(name, "loudness"))
            set_sound(value, SOUND_LOUDNESS, &global_settings.loudness);
        else if (!strcasecmp(name, "bass boost"))
            set_sound(value, SOUND_SUPERBASS, &global_settings.bass_boost);
        else if (!strcasecmp(name, "auto volume"))
            set_sound(value, SOUND_AVC, &global_settings.avc);
        else if (!strcasecmp(name, "rec mic gain"))
            set_sound(value, SOUND_MIC_GAIN, &global_settings.rec_mic_gain);
        else if (!strcasecmp(name, "rec left gain"))
            set_sound(value, SOUND_LEFT_GAIN, &global_settings.rec_left_gain);
        else if (!strcasecmp(name, "rec right gain"))
            set_sound(value, SOUND_RIGHT_GAIN, &global_settings.rec_right_gain);
        else if (!strcasecmp(name, "rec quality"))
            set_cfg_int(&global_settings.rec_quality, value, 0, 7);
        else if (!strcasecmp(name, "rec source")) {
            static char* options[] = {"mic", "line", "spdif"};
            set_cfg_option(&global_settings.rec_source, value, options, 3);
        }
        else if (!strcasecmp(name, "rec frequency")) {
            static char* options[] = {"44", "48", "32", "22", "24", "16"};
            set_cfg_option(&global_settings.rec_frequency, value, options, 6);
        }
        else if (!strcasecmp(name, "rec channels")) {
            static char* options[] = {"stereo", "mono"};
            set_cfg_option(&global_settings.rec_channels, value, options, 2);
        }
#endif
        else if (!strcasecmp(name, "poweroff"))
            set_cfg_int(&global_settings.poweroff, value, 0, 15);
        else if (!strcasecmp(name, "battery capacity"))
            set_cfg_int(&global_settings.battery_capacity, value,
                        1500, BATTERY_CAPACITY_MAX);
#ifdef HAVE_CHARGE_CTRL
        else if (!strcasecmp(name, "deep discharge"))
            set_cfg_bool(&global_settings.discharge, value);
        else if (!strcasecmp(name, "trickle charge"))
            set_cfg_bool(&global_settings.trickle_charge, value);
#endif
        else if (!strcasecmp(name, "volume fade"))
            set_cfg_bool(&global_settings.fade_on_stop, value);
    }

    close(fd);
    settings_apply();
    settings_save();
    return true;
}

/*
 * reset all settings to their default value 
 */
void settings_reset(void) {
        
    DEBUGF( "settings_reset()\n" );

    global_settings.volume      = mpeg_sound_default(SOUND_VOLUME);
    global_settings.balance     = mpeg_sound_default(SOUND_BALANCE);
    global_settings.bass        = mpeg_sound_default(SOUND_BASS);
    global_settings.treble      = mpeg_sound_default(SOUND_TREBLE);
    global_settings.loudness    = mpeg_sound_default(SOUND_LOUDNESS);
    global_settings.bass_boost  = mpeg_sound_default(SOUND_SUPERBASS);
    global_settings.avc         = mpeg_sound_default(SOUND_AVC);
    global_settings.channel_config = mpeg_sound_default(SOUND_CHANNELS);
    global_settings.rec_quality = 5;
    global_settings.rec_source = 0;    /* 0=mic */
    global_settings.rec_frequency = 0; /* 0=44.1kHz */
    global_settings.rec_channels = 0;  /* 0=Stereo */
    global_settings.rec_mic_gain = 8;
    global_settings.rec_left_gain = 2; /* 0dB */
    global_settings.rec_right_gain = 2; /* 0dB */
    global_settings.resume      = RESUME_ASK;
    global_settings.contrast    = DEFAULT_CONTRAST_SETTING;
    global_settings.poweroff    = DEFAULT_POWEROFF_SETTING;
    global_settings.backlight_timeout   = DEFAULT_BACKLIGHT_TIMEOUT_SETTING;
    global_settings.backlight_on_when_charging   = 
        DEFAULT_BACKLIGHT_ON_WHEN_CHARGING_SETTING;
    global_settings.battery_capacity = 1500; /* mAh */
    global_settings.trickle_charge = true;
    global_settings.dirfilter   = SHOW_MUSIC;
    global_settings.sort_case   = false;
    global_settings.statusbar   = true;
    global_settings.scrollbar   = true;
    global_settings.repeat_mode = REPEAT_ALL;
    global_settings.playlist_shuffle = false;
    global_settings.discharge    = 0;
    global_settings.timeformat   = 0;
    global_settings.volume_type  = 0;
    global_settings.battery_type = 0;
    global_settings.scroll_speed = 8;
    global_settings.bidir_limit  = 50;
    global_settings.scroll_delay = 100;
    global_settings.scroll_step  = 6;
    global_settings.ff_rewind_min_step = DEFAULT_FF_REWIND_MIN_STEP;
    global_settings.ff_rewind_accel = DEFAULT_FF_REWIND_ACCEL_SETTING;
    global_settings.resume_index = -1;
    global_settings.resume_offset = -1;
    global_settings.save_queue_resume = true;
    global_settings.queue_resume = 0;
    global_settings.queue_resume_index = -1;
    global_settings.disk_spindown = 5;
    global_settings.disk_poweroff = false;
    global_settings.buffer_margin = 0;
    global_settings.browse_current = false;
    global_settings.play_selected = true;
    global_settings.peak_meter_release = 8;
    global_settings.peak_meter_hold = 3;
    global_settings.peak_meter_clip_hold = 16;
    global_settings.peak_meter_dbfs = true;
    global_settings.peak_meter_min = 60;
    global_settings.peak_meter_max = 0;
    global_settings.peak_meter_performance = false;
    global_settings.wps_file[0] = 0;
    global_settings.font_file[0] = 0;
    global_settings.lang_file[0] = 0;
    global_settings.runtime = 0;
    global_settings.topruntime = 0;
    global_settings.cpu_sleep = true;
    global_settings.fade_on_stop = true;
}


/*
 * dump the list of current settings
 */
void settings_display(void)
{
#ifdef DEBUG
    DEBUGF( "\nsettings_display()\n" );

    DEBUGF( "\nvolume:\t\t%d\nbalance:\t%d\nbass:\t\t%d\ntreble:\t\t%d\n"
            "loudness:\t%d\nbass boost:\t%d\n",
            global_settings.volume,
            global_settings.balance,
            global_settings.bass,
            global_settings.treble,
            global_settings.loudness,
            global_settings.bass_boost );

    DEBUGF( "contrast:\t%d\npoweroff:\t%d\nbacklight_timeout:\t%d\n",
            global_settings.contrast,
            global_settings.poweroff,
            global_settings.backlight_timeout );
#endif
}

bool set_bool(char* string, bool* variable )
{
    return set_bool_options(string, variable, str(LANG_SET_BOOL_YES), 
                            str(LANG_SET_BOOL_NO));
}

bool set_bool_options(char* string, bool* variable,
                      char* yes_str, char* no_str )
{
    char* names[] = { no_str, yes_str };
    int value = *variable;
    bool result;

    result = set_option(string, &value, names, 2, NULL);
    *variable = value;
    return result;
}

bool set_int(char* string, 
             char* unit,
             int* variable,
             void (*function)(int),
             int step,
             int min,
             int max )
{
    bool done = false;
    int button;
    int org_value=*variable;

#ifdef HAVE_LCD_BITMAP
    if(global_settings.statusbar)
        lcd_setmargins(0, STATUSBAR_HEIGHT);
    else
        lcd_setmargins(0, 0);
#endif

    lcd_clear_display();
    lcd_puts_scroll(0, 0, string);

    while (!done) {
        char str[32];
        snprintf(str,sizeof str,"%d %s  ", *variable, unit);
        lcd_puts(0, 1, str);
#ifdef HAVE_LCD_BITMAP
        status_draw();
#endif
        lcd_update();

        button = button_get_w_tmo(HZ/2);
        switch(button) {
#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_UP:
            case BUTTON_UP | BUTTON_REPEAT:
#else
            case BUTTON_RIGHT:
            case BUTTON_RIGHT | BUTTON_REPEAT:
#endif
                *variable += step;
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_DOWN:
            case BUTTON_DOWN | BUTTON_REPEAT:
#else
            case BUTTON_LEFT:
            case BUTTON_LEFT | BUTTON_REPEAT:
#endif
                *variable -= step;
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_LEFT:
            case BUTTON_PLAY:
#else
            case BUTTON_PLAY:
#endif
                done = true;
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_OFF:
#else
            case BUTTON_STOP:
            case BUTTON_MENU:
#endif
                if (*variable != org_value) {
                    *variable=org_value;
                    lcd_stop_scroll();
                    lcd_puts(0, 0, str(LANG_MENU_SETTING_CANCEL));
                    lcd_update();
                    sleep(HZ/2);
                }
                done = true;
                break;

            case SYS_USB_CONNECTED:
                usb_screen();
                return true;

        }
        if(*variable > max )
            *variable = max;

        if(*variable < min )
            *variable = min;

        if ( function && button != BUTTON_NONE)
            function(*variable);
    }
    lcd_stop_scroll();

    return false;
}

bool set_option(char* string, int* variable, char* options[],
                int numoptions, void (*function)(int))
{
    bool done = false;
    int button;
    int org_value=*variable;

#ifdef HAVE_LCD_BITMAP
    if(global_settings.statusbar)
        lcd_setmargins(0, STATUSBAR_HEIGHT);
    else
        lcd_setmargins(0, 0);
#endif

    lcd_clear_display();
    lcd_puts_scroll(0, 0, string);

    while ( !done ) {
        lcd_puts(0, 1, options[*variable]);
#ifdef HAVE_LCD_BITMAP
        status_draw();
#endif
        lcd_update();

        button = button_get_w_tmo(HZ/2);
        switch (button) {
#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_UP:
            case BUTTON_UP | BUTTON_REPEAT:
#else
            case BUTTON_RIGHT:
            case BUTTON_RIGHT | BUTTON_REPEAT:
#endif
                if ( *variable < (numoptions-1) )
                    (*variable)++;
				else
                    (*variable) -= (numoptions-1);
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_DOWN:
            case BUTTON_DOWN | BUTTON_REPEAT:
#else
            case BUTTON_LEFT:
            case BUTTON_LEFT | BUTTON_REPEAT:
#endif
                if ( *variable > 0 )
                    (*variable)--;
                else
                    (*variable) += (numoptions-1);
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_LEFT:
            case BUTTON_PLAY:
#else
            case BUTTON_PLAY:
#endif
                done = true;
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_OFF:
#else
            case BUTTON_STOP:
            case BUTTON_MENU:
#endif
                if (*variable != org_value) {
                    *variable=org_value;
                    lcd_stop_scroll();
                    lcd_puts(0, 0, str(LANG_MENU_SETTING_CANCEL));
                    lcd_update();
                    sleep(HZ/2);
                }
                done = true;
                break;

            case SYS_USB_CONNECTED:
                usb_screen();
                return true;
        }

        if ( function && button != BUTTON_NONE)
            function(*variable);
    }
    lcd_stop_scroll();
    return false;
}

#ifdef HAVE_LCD_BITMAP
#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_WIDTH 2
bool set_time(char* string, int timedate[])
{
    bool done = false;
    int button;
    int min = 0, steps = 0;
    int cursorpos = 0;
    int lastcursorpos = !cursorpos;
    unsigned char buffer[19];
    int realyear;
    int julianday;
    int i;
    unsigned char reffub[5];
    unsigned int width, height;
    unsigned int separator_width, weekday_width;
    unsigned int line_height, prev_line_height;
    char *dayname[] = {str(LANG_WEEKDAY_SUNDAY),
                       str(LANG_WEEKDAY_MONDAY),
                       str(LANG_WEEKDAY_TUESDAY),
                       str(LANG_WEEKDAY_WEDNESDAY),
                       str(LANF_WEEKDAY_THURSDAY),
                       str(LANG_WEEKDAY_FRIDAY),
                       str(LANG_WEEKDAY_SATURDAY)};
    char *monthname[] = {str(LANG_MONTH_JANUARY),
                         str(LANG_MONTH_FEBRUARY),
                         str(LANG_MONTH_MARCH),
                         str(LANG_MONTH_APRIL),
                         str(LANG_MONTH_MAY),
                         str(LANG_MONTH_JUNE),
                         str(LANG_MONTH_JULY),
                         str(LANG_MONTH_AUGUST),
                         str(LANG_MONTH_SEPTEMBER),
                         str(LANG_MONTH_OCTOBER),
                         str(LANG_MONTH_NOVEMBER),
                         str(LANG_MONTH_DECEMBER)};
    char cursor[][3] = {{ 0,  8, 12}, {18,  8, 12}, {36,  8, 12},
                        {24, 16, 24}, {54, 16, 18}, {78, 16, 12}};
    char daysinmonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    int monthname_len = 0, dayname_len = 0;


#ifdef HAVE_LCD_BITMAP
    if(global_settings.statusbar)
        lcd_setmargins(0, STATUSBAR_HEIGHT);
    else
        lcd_setmargins(0, 0);
#endif
    lcd_clear_display();
    lcd_puts_scroll(0, 0, string);

    while ( !done ) {
        /* calculate the number of days in febuary */
        realyear = timedate[3] + 2000;
        if((realyear % 4 == 0 && !(realyear % 100 == 0)) || realyear % 400 == 0)
            daysinmonth[1] = 29;
        else
            daysinmonth[1] = 28;

        /* fix day if month or year changed */
        if (timedate[5] > daysinmonth[timedate[4] - 1])
            timedate[5] = daysinmonth[timedate[4] - 1];

        /* calculate day of week */
        julianday = 0;
        for(i = 0; i < timedate[4] - 1; i++) {
           julianday += daysinmonth[i];
        }
        julianday += timedate[5];
        timedate[6] = (realyear + julianday + (realyear - 1) / 4 -
                       (realyear - 1) / 100 + (realyear - 1) / 400 + 7 - 1) % 7;

        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d ",
                 timedate[0], timedate[1], timedate[2]);
        lcd_puts(0, 1, buffer);

        /* recalculate the positions and offsets */
        lcd_getstringsize(string, &width, &prev_line_height);
        lcd_getstringsize(buffer, &width, &line_height);
        lcd_getstringsize(":", &separator_width, &height);

        /* hour */
        strncpy(reffub, buffer, 2);
        reffub[2] = '\0';
        lcd_getstringsize(reffub, &width, &height);
        cursor[0][INDEX_X] = 0;
        cursor[0][INDEX_Y] = prev_line_height;
        cursor[0][INDEX_WIDTH] = width;

        /* minute */
        strncpy(reffub, buffer + 3, 2);
        reffub[2] = '\0';
        lcd_getstringsize(reffub, &width, &height);
        cursor[1][INDEX_X] = cursor[0][INDEX_WIDTH] + separator_width;
        cursor[1][INDEX_Y] = prev_line_height;
        cursor[1][INDEX_WIDTH] = width;

        /* second */
        strncpy(reffub, buffer + 6, 2);
        reffub[2] = '\0';
        lcd_getstringsize(reffub, &width, &height);
        cursor[2][INDEX_X] = cursor[0][INDEX_WIDTH] + separator_width +
                             cursor[1][INDEX_WIDTH] + separator_width;
        cursor[2][INDEX_Y] = prev_line_height;
        cursor[2][INDEX_WIDTH] = width;

        lcd_getstringsize(buffer, &width, &prev_line_height);

        snprintf(buffer, sizeof(buffer), "%s 20%02d %s %02d ",
                 dayname[timedate[6]], timedate[3], monthname[timedate[4] - 1],
                 timedate[5]);
        lcd_puts(0, 2, buffer);

        /* recalculate the positions and offsets */
        lcd_getstringsize(buffer, &width, &line_height);

        /* store these 2 to prevent _repeated_ strlen calls */
        monthname_len = strlen(monthname[timedate[4] - 1]);
        dayname_len = strlen(dayname[timedate[6]]);

        /* weekday */
        strncpy(reffub, buffer, dayname_len);
        reffub[dayname_len] = '\0';
        lcd_getstringsize(reffub, &weekday_width, &height);
        lcd_getstringsize(" ", &separator_width, &height);

        /* year */
        strncpy(reffub, buffer + dayname_len + 1, 4);
        reffub[4] = '\0';
        lcd_getstringsize(reffub, &width, &height);
        cursor[3][INDEX_X] = weekday_width + separator_width;
        cursor[3][INDEX_Y] = cursor[0][INDEX_Y] + prev_line_height;
        cursor[3][INDEX_WIDTH] = width;

        /* month */
        strncpy(reffub, buffer + dayname_len + 6, monthname_len);
        reffub[monthname_len] = '\0';
        lcd_getstringsize(reffub, &width, &height);
        cursor[4][INDEX_X] = weekday_width + separator_width +
                             cursor[3][INDEX_WIDTH] + separator_width;
        cursor[4][INDEX_Y] = cursor[0][INDEX_Y] + prev_line_height;
        cursor[4][INDEX_WIDTH] = width;

        /* day */
        strncpy(reffub, buffer + dayname_len + monthname_len + 7, 2);
        reffub[2] = '\0';
        lcd_getstringsize(reffub, &width, &height);
        cursor[5][INDEX_X] = weekday_width + separator_width +
                             cursor[3][INDEX_WIDTH] + separator_width +
                             cursor[4][INDEX_WIDTH] + separator_width;
        cursor[5][INDEX_Y] = cursor[0][INDEX_Y] + prev_line_height;
        cursor[5][INDEX_WIDTH] = width;

        lcd_invertrect(cursor[cursorpos][INDEX_X],
                       cursor[cursorpos][INDEX_Y] + lcd_getymargin(),
                       cursor[cursorpos][INDEX_WIDTH],
                       line_height);

        lcd_puts(0, 4, str(LANG_TIME_SET));
        lcd_puts(0, 5, str(LANG_TIME_REVERT));
#ifdef HAVE_LCD_BITMAP
        status_draw();
#endif
        lcd_update();

        /* calculate the minimum and maximum for the number under cursor */
        if(cursorpos!=lastcursorpos) {
            lastcursorpos=cursorpos;
            switch(cursorpos) {
                case 0: /* hour */
                    min = 0;
                    steps = 24;
                    break;
                case 1: /* minute */
                case 2: /* second */
                    min = 0;
                    steps = 60;
                    break;
                case 3: /* year */
                    min = 0;
                    steps = 100;
                    break;
                case 4: /* month */
                    min = 1;
                    steps = 12;
                    break;
                case 5: /* day */
                    min = 1;
                    steps = daysinmonth[timedate[4] - 1];
                    break;
            }
        }

        button = button_get_w_tmo(HZ/2);
        switch ( button ) {
            case BUTTON_LEFT:
                cursorpos = (cursorpos + 6 - 1) % 6;
                break;
            case BUTTON_RIGHT:
                cursorpos = (cursorpos + 6 + 1) % 6;
                break;
            case BUTTON_UP:
            case BUTTON_UP | BUTTON_REPEAT:
                timedate[cursorpos] = (timedate[cursorpos] + steps - min + 1) %
                    steps + min;
                if(timedate[cursorpos] == 0)
                    timedate[cursorpos] += min;
                break;
            case BUTTON_DOWN:
            case BUTTON_DOWN | BUTTON_REPEAT:
                timedate[cursorpos]=(timedate[cursorpos]+steps - min - 1) % 
                    steps + min;
                if(timedate[cursorpos] == 0)
                    timedate[cursorpos] += min;
                break;
            case BUTTON_ON:
                done = true;
                if (timedate[6] == 0) /* rtc needs 1 .. 7 */
                    timedate[6] = 7;
                break;
            case BUTTON_OFF:
                done = true;
                timedate[0] = -1;
                break;
#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_F3:
#ifdef HAVE_LCD_BITMAP
                global_settings.statusbar = !global_settings.statusbar;
                settings_save();
                if(global_settings.statusbar)
                    lcd_setmargins(0, STATUSBAR_HEIGHT);
                else
                    lcd_setmargins(0, 0);
                lcd_clear_display();
                lcd_puts_scroll(0, 0, string);
#endif
                break;
#endif

            case SYS_USB_CONNECTED:
                usb_screen();
                return true;
        }
    }

    return false;
}
#endif
