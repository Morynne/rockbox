/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Felix Arends
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include <windows.h>
#include "config.h"
#include "sh7034.h"
#include "button.h"

#define KEY(k)      HIBYTE(GetKeyState (k))

int last_key ;

void button_init(void) 
{ 
	last_key = 0 ;
}

int button_get(bool block)
{
    int btn = 0;
    if (KEY (VK_NUMPAD4) ||
        KEY (VK_LEFT)) // left button
        btn |= BUTTON_LEFT;

    if (KEY (VK_NUMPAD6) ||
        KEY (VK_RIGHT))
        btn |= BUTTON_RIGHT; // right button

    if (KEY (VK_NUMPAD8) ||
        KEY (VK_UP))
        btn |= BUTTON_UP; // up button

    if (KEY (VK_NUMPAD2) ||
        KEY (VK_DOWN))
        btn |= BUTTON_DOWN; // down button

    if (KEY (VK_NUMPAD5) ||
        KEY (VK_SPACE))
        btn |= BUTTON_PLAY; // play button

    if (KEY (VK_RETURN))
        btn |= BUTTON_OFF; // off button

    if (KEY (VK_ADD))
        btn |= BUTTON_ON; // on button

    if (KEY (VK_DIVIDE))
        btn |= BUTTON_F1; // F1 button

    if (KEY (VK_MULTIPLY))
        btn |= BUTTON_F2; // F2 button

    if (KEY (VK_SUBTRACT))
        btn |= BUTTON_F3; // F3 button

	if (btn != 0) {
		last_key = 0 ;
	}
    return btn;	
}