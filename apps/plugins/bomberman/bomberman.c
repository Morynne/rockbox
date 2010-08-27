/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 Lev Panov
 * 
 * Bomberman plugin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "plugin.h"

#ifdef HAVE_LCD_BITMAP
#include "lib/pluginlib_actions.h"
#include "lib/helper.h"
#include "lib/playback_control.h"

#include "game.h"
#include "draw.h"
#include "ai.h"

#define SLEEP_TIME 0

const struct button_mapping *plugin_contexts[] = {
	pla_main_ctx,
#if defined(HAVE_REMOTE_LCD)
    pla_remote_ctx,
#endif
};

#define NB_ACTION_CONTEXTS \
    (sizeof(plugin_contexts) / sizeof(struct button_mapping*))

void cleanup(void *parameter)
{
	(void)parameter;

	backlight_use_settings();
#ifdef HAVE_REMOTE_LCD
	remote_backlight_use_settings();
#endif
}

/* 
 * Main code 
 */

void InitGame(Game *game)
{
	int i, j;
	int DefaultMap[MAP_H][MAP_W] = {
		{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
		{2,0,0,1,1,1,0,1,0,1,0,1,0,1,0,0,2},
		{2,0,2,1,2,1,2,0,2,1,2,0,2,1,2,0,2},
		{2,0,0,1,1,1,1,1,0,1,0,1,0,1,0,0,2},
		{2,0,2,1,2,1,2,0,2,1,2,0,2,1,2,0,2},
		{2,0,0,1,1,1,1,1,0,1,0,1,0,1,0,0,2},
		{2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2},
		{2,0,0,1,1,1,1,1,0,1,0,1,0,1,0,0,2},
		{2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2},
		{2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
		{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2}
		};
	
	for (i = 0; i < MAP_W; i++)
		for (j = 0; j < MAP_H; j++)
		{
			game->field.map[i][j] = DefaultMap[j][i];//SQUARE_FREE;
			game->field.firemap[i][j].state = BOMB_NONE;
			game->field.boxes[i][j].state = HUNKY;
		}
			
	for (i = 0; i < BOMBS_MAX_NUM; i++)
		game->field.bombs[i].state = BOMB_NONE;
	
	game->nplayers = MAX_PLAYERS;
	
	game->bomb_rad[BOMB_PWR_SINGLE] = 1;
	game->bomb_rad[BOMB_PWR_DOUBLE] = 2;
	game->bomb_rad[BOMB_PWR_TRIPLE] = 4;
	game->bomb_rad[BOMB_PWR_QUAD] = 6;
	game->bomb_rad[BOMB_PWR_KILLER] = MAP_W;
}

void InitPlayer(Player *player, int x, int y)
{
	player->status.state = ALIVE;
	player->status.health = 100;
	player->xpos = x;
	player->ypos = y;
	player->look = LOOK_DOWN;
	player->speed = 1;
	player->bombs_max = -1;
	player->bombs_placed = 0;
	player->bomb_power = BOMB_PWR_KILLER;
	
	player->rxpos = 0;
	player->rypos = 0;
	player->ismove = false;
	player->move_phase = 0;
	
	player->IsAIPlayer = false;
}

void InitAI(Player *player, int x, int y)
{
	player->status.state = ALIVE;
	player->status.health = 100;
	player->xpos = x;
	player->ypos = y;
	player->look = LOOK_DOWN;
	player->speed = 1;
	player->bombs_max = -1;
	player->bombs_placed = 0;
	player->bomb_power = BOMB_PWR_KILLER;
	
	player->rxpos = 0;
	player->rypos = 0;
	player->ismove = false;
	player->move_phase = 0;
	
	player->IsAIPlayer = true;
}

void ToggleAudioPlayback(void)
{
	int audio_status = rb->audio_status();
	
    if (!audio_status && rb->global_status->resume_index != -1)
    {
        if (rb->playlist_resume() != -1)
        {
            rb->playlist_start(rb->global_status->resume_index,
                rb->global_status->resume_offset);
        }
    }
    else if (audio_status & AUDIO_STATUS_PAUSE)
        rb->audio_resume();
    else
        rb->audio_pause();
}

void PlayAudioPlaylist(int start_index)
{
	if (rb->playlist_resume() != -1)
		rb->playlist_start(start_index, 0);
}

int plugin_main(void)
{
    int action; /* Key action */
    int i;
    Game game;
    
    rb->srand(*rb->current_tick);
    
    InitGame(&game);

	InitPlayer(&game.players[0], 1, 5);
	//InitAI(&game.players[1], 3, 9);
	InitAI(&game.players[1], 10, 9);
	//InitAI(&game.players[2], 2, 1);
	//InitAI(&game.players[3], 15, 1);
	
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		game.draw_order[i] = &game.players[i];
		//game->draw_order[i].order = i;
	}
	
	PlayAudioPlaylist(0);
	//ToggleAudioPlayback();
	//rb->audio_next();
	//rb->audio_prev();

    /* Main loop */
    while (true)
    {
		Draw(&game);
	
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			int upd;
			
			upd = UpdatePlayer(&game, &game.players[i]);
			if (upd == DEAD)
			{
				game.nplayers--;
				if (game.nplayers == 1)
				{
					for (i = 0; i < MAX_PLAYERS; i++)
					{
						if (game.players[i].status.state == ALIVE)
						{
							//if (game.players[i].IsAIPlayer)
							//	rb->splash(HZ * 5, "You lose");
							//else
							//	rb->splash(HZ * 5, "You won");
						}
					}
				}
			}
		}
		
		UpdateBombs(&game);
		UpdateBoxes(&game);
		UpdateAI(&game, game.players);

		rb->sleep(SLEEP_TIME);
		
		action = pluginlib_getaction(TIMEOUT_NOBLOCK,
									 plugin_contexts,
									 NB_ACTION_CONTEXTS);
									 
		switch (action)
		{
			case PLA_EXIT:
				ToggleAudioPlayback();
				cleanup(NULL);
				return PLUGIN_OK;
				
			case PLA_UP:
			case PLA_UP_REPEAT:
				PlayerMoveUp(&game, &game.players[0]);
				break;
				
			case PLA_DOWN:
			case PLA_DOWN_REPEAT:
				PlayerMoveDown(&game, &game.players[0]);
				break;
			
			case PLA_RIGHT:
			case PLA_RIGHT_REPEAT:
				PlayerMoveRight(&game, &game.players[0]);
				break;
			
			case PLA_LEFT:
			case PLA_LEFT_REPEAT:
				PlayerMoveLeft(&game, &game.players[0]);
				break;
			
			case PLA_SELECT:
				PlayerPlaceBomb(&game, &game.players[0]);
				break;
				
			case PLA_CANCEL:
				break;
		}
	}
}

/* this is the plugin entry point */
enum plugin_status plugin_start(const void* parameter)
{
	int ret;
	
	/* avoid the compiler warning about unused parameter */
	(void)parameter;
    
#if LCD_DEPTH > 1
	rb->lcd_set_backdrop(NULL);
#endif
	backlight_force_on(); /* backlight control in lib/helper.c */
#ifdef HAVE_REMOTE_LCD
	remote_backlight_force_on(); /* remote backlight control in lib/helper.c */
#endif

	ret = plugin_main();

	return ret;
}

#endif /* #ifdef HAVE_LCD_BITMAP */
