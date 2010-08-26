/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 Lev Panov, Nick Petrov
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

#include "game.h"

#define _abs(x) ((x) >= 0 ? (x) : -(x))
#define _max(a, b) ((a) > (b) ? (a) : (b))

#define UNREAL_F 999

#define SQUARE_SIZE 16
#define XMAPOFFSET 25
#define YMAPOFFSET 30

#define PATH_OFFSET 1
#define MOVE_COST 10

typedef struct
{
  bool IsWalkable;
  bool IsOnOpen;
  bool IsOnClose;
  int G, H, F;
  int ParentX, ParentY;
} NODE;

typedef struct
{
  int X, Y;
} PATHELEM;

typedef struct
{
  PATHELEM Path[MAP_W * MAP_H];
  int Distance;
} PATH;

NODE Nodes[MAP_W][MAP_H]; 

bool GetNode(Field *field, int x, int y)
{  
	if (field->map[x][y] == SQUARE_FREE || field->map[x][y] == SQUARE_BOMB)
		return true;

	return false;
}

void InitNodes(Field *F)
{
  int x, y;

  for (x = 0; x < MAP_W; x++)
    for (y = 0; y < MAP_H; y++)
    {
	    Nodes[x][y].IsWalkable = GetNode(F, x, y);
	    Nodes[x][y].IsOnOpen = false;
	    Nodes[x][y].IsOnClose = false;
	    Nodes[x][y].G = 0;
	    Nodes[x][y].H = 0;
	    Nodes[x][y].F = 0;
	    Nodes[x][y].ParentX = 0;
	    Nodes[x][y].ParentY = 0;
	}
}

void FindPath(PATH *Path, int StartX, int StartY,
                int EndX, int EndY)
{
  int x = 0, y = 0; // for running through the nodes
  int dx, dy; // for the 8 (4) squares adjacent to each node
  int cx = StartX, cy = StartY;
  int lowestf = UNREAL_F; // start with the lowest being the highest
  
  // debug info
  /*
  int desc;
  char logStr[100] = "\n";
  static bool opened = false;
  
  if (!opened)
  {
    if ((desc = rb->open(PLUGIN_GAMES_DIR "/path.txt", O_WRONLY | O_CREAT, 0666)) < 0)
    { 
	  rb->splash(HZ, "Cant open");
      return;
    }
    else
	  opened = true;
  }
  rb->write(desc, logStr, rb->strlen(logStr));
  */
  
  // add starting node to open list
  Nodes[StartX][StartY].IsOnOpen = true;
  Nodes[StartX][StartY].IsOnClose = false;
  
  while (cx != EndX || cy != EndY)
  {
    // look for lowest F cost node on open list - this becomes the current node
	lowestf = UNREAL_F;
	for (x = 0; x < MAP_W; x++)
	{
	  for (y = 0; y < MAP_H; y++)
	  {
		Nodes[x][y].F = Nodes[x][y].G + Nodes[x][y].H;
		if (Nodes[x][y].IsOnOpen)
		{
			if (Nodes[x][y].F < lowestf)
			{
				cx = x;
				cy = y;
				lowestf = Nodes[x][y].F;
			}
		}
	  }
	}
	
	// we found it, so now put that node on the closed list
	Nodes[cx][cy].IsOnOpen = false;
	Nodes[cx][cy].IsOnClose = true;
	
	// for each of the 8 (4) adjacent node
	for (dx = -1; dx <= 1; dx++)
	{
	  for (dy = -1; dy <= 1; dy++)
	  {
		// we don't use diagonals in bomberman
		if (dx != -dy && dx != dy)
		{
		  if ((cx + dx) < MAP_W && (cx + dx) > -1 && 
		       (cy + dy) < MAP_H && (cy + dy) > -1)
		  {
			 // if its walkable and not on the closed list
			 if (Nodes[cx+dx][cy+dy].IsWalkable == true
				&& Nodes[cx+dx][cy+dy].IsOnClose == false)
			 {
			    // if its not on open list
				if (Nodes[cx+dx][cy+dy].IsOnOpen == false)
				{
				  // add it to open list
				  Nodes[cx+dx][cy+dy].IsOnOpen = true;
				  Nodes[cx+dx][cy+dy].IsOnClose = false;
				  // make the current node its parent
				  Nodes[cx+dx][cy+dy].ParentX = cx;
				  Nodes[cx+dx][cy+dy].ParentY = cy;
				  
				  // debug
				  /*
				  rb->memset(logStr, 0, 100);
	              rb->snprintf(logStr, 19 , "C: %i %i P: %i %i\n", cx+dx, cy+dy, cx, cy);
	              rb->write(desc, logStr, rb->strlen(logStr));
	              */
	              
				  // work out G
				  Nodes[cx+dx][cy+dy].G = MOVE_COST; // straights cost 10
				  // work out H
				  // MANHATTAN METHOD
				  Nodes[cx+dx][cy+dy].H =
							(_abs(EndX-(cx+dx)) + 
							 _abs(EndY-(cy+dy))) * MOVE_COST;
				  Nodes[cx+dx][cy+dy].F =
							Nodes[cx+dx][cy+dy].G + 
							Nodes[cx+dx][cy+dy].H;
				}
			} // end if walkable and not on closed list
	      }
		}
	  }
	}
  }
  
  // follow all the parents back to the start
  cx = EndX;
  cy = EndY;
  Path->Distance = 0;
  Path->Path[Path->Distance].X = cx;
  Path->Path[Path->Distance].Y = cy;
  Path->Distance++;
  
  while (cx != StartX || cy != StartY)
  {
	Path->Path[Path->Distance].X = Nodes[cx][cy].ParentX;
	Path->Path[Path->Distance].Y = Nodes[cx][cy].ParentY;
	cx = Path->Path[Path->Distance].X;
	cy = Path->Path[Path->Distance].Y;
	
	// debug
	/*
	rb->memset(logStr, 0, 100);
	rb->snprintf(logStr, 10 , "PP: %i %i\n", Path->Path[Path->Distance].X, 
	             Path->Path[Path->Distance].Y);
	rb->write(desc, logStr, rb->strlen(logStr));
	*/
	
	Path->Distance++;
	if (Path->Distance > 100)
		break;
  }
  
  // debug
  /*
   rb->close(desc);
   */
}

void MovePlayer(Game *G, Player *P, PATH *Path)
{
	if (P->xpos < Path->Path[PATH_OFFSET].X)
	  PlayerMoveRight(G, P);
	else if (P->xpos > Path->Path[PATH_OFFSET].X)
	  PlayerMoveLeft(G, P);
	else if (P->ypos < Path->Path[PATH_OFFSET].Y)
	  PlayerMoveDown(G, P);
	else if (P->ypos > Path->Path[PATH_OFFSET].Y) 
	  PlayerMoveUp(G, P);  
}

void CopyPaths(PATH *Dst, PATH *Src)
{
	int i;
	
	Dst->Distance = Src->Distance;
	
	for (i = Src->Distance - 1; i >= 0; i--)
	{
	  Dst->Path[(Src->Distance - 1) - i].X = Src->Path[i].X;
	  Dst->Path[(Src->Distance - 1) - i].Y = Src->Path[i].Y;
	}
}

void UpdateAI(Game *G, Player *Players)
{
  int i, j;
  PATH Path, CurPath;
  int MinDist = UNREAL_F;
  
  for (i = 0; i < MAX_PLAYERS; i++)
  {
		MinDist = UNREAL_F;
		
		if (Players[i].IsAIPlayer == true)
		{
			 for (j = 0; j < MAX_PLAYERS; j++)
			 {
				if (j == i)
				  continue;
				  
				FindPath(&Path, Players[i].xpos, Players[i].ypos, 
					Players[j].xpos, Players[j].ypos);
					
				if (Path.Distance < MinDist)
				{
				  MinDist = Path.Distance;
				  CopyPaths(&CurPath, &Path);
				}
			}
			
			if (CurPath.Distance > 1)
			  MovePlayer(G, &Players[i], &CurPath);
			  
			// debug: draw path
			/*
			FOR_NB_SCREENS(k)
            { 
		      struct screen *display = rb->screens[k];
		      int j;
		      
		      for (j = CurPath.Distance - 2; j >= 0; j--)
		      {
		        display->drawline(CurPath.Path[j].X * SQUARE_SIZE + XMAPOFFSET + SQUARE_SIZE / 2, 
		                          CurPath.Path[j].Y * SQUARE_SIZE + YMAPOFFSET + SQUARE_SIZE / 2,  
		                          CurPath.Path[j + 1].X * SQUARE_SIZE + XMAPOFFSET + SQUARE_SIZE / 2, 
		                          CurPath.Path[j + 1].Y * SQUARE_SIZE + YMAPOFFSET + SQUARE_SIZE / 2);
			  }
			  
		      display->update();
	        }
	        */
		}
	}
}
