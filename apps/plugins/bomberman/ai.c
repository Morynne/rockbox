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

#define UNREAL_F 999

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
  //int F;
} PATHELEM;

typedef struct
{
  PATHELEM Path[MAP_W * MAP_H];
  int Distance;
} PATH;

/* PATHELEM Path[MAP_W][MAP_H]; */
NODE Nodes[MAP_W][MAP_H]; 

bool GetNode(Field  *field, int x, int y)
{  
	if (field->map[x][y] == SQUARE_FREE ||
		  field->map[x][y] == SQUARE_BOMB)
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
  int dx, dy; // for the 8 squares adjacent to each node
  int cx = StartX, cy = StartY;
  int lowestf = UNREAL_F; // start with the lowest being the highest
  
  // add starting node to open list
  Nodes[StartX][StartY].IsOnOpen = true;
  Nodes[StartX][StartY].IsOnClose = false;
//////////////////////LOOP BEGINS HERE/////////////////////////
  while (cx != EndX || cy != EndY)
  {
    //look for lowest F cost node on open list - this becomes the current node
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
	//rb->splash(HZ * 0.002, "One loop");
	// we found it, so now put that node on the closed list
	Nodes[cx][cy].IsOnOpen = false;
	Nodes[cx][cy].IsOnClose = true;
	// for each of the 8 adjacent node
	for (dx = -1; dx <= 1; dx++)
	{
	  for (dy = -1; dy <= 1; dy++)
	  {
		// We can`t use diagonals in bomberman
		if ((dx == dy) || (dy == -dx))
		{
		  Nodes[cx+dx][cy+dy].IsWalkable == false; 
		  continue;
		}
		if ((dx == 0) || (dy == 0))
		{
		  if ((cx + dx) < MAP_W && (cx + dx) > -1 && 
		       (cy + dy) < MAP_H && (cy + dy) > -1)
		  {
			 // if its walkable and not on the closed list
			 if (Nodes[cx+dx][cy+dy].IsWalkable == true 
			   && Nodes[cx+dx][cy+dy].IsOnClose == false)
			 {
			    //if its not on open list
				if (Nodes[cx+dx][cy+dy].IsOnOpen == false)
				{
				  //add it to open list
				  Nodes[cx+dx][cy+dy].IsOnOpen = true;
				  Nodes[cx+dx][cy+dy].IsOnClose = false;
				  //make the current node its parent
				  Nodes[cx+dx][cy+dy].ParentX = cx;
				  Nodes[cx+dx][cy+dy].ParentY = cy;
				  //work out G
				  // We can`t use diagonals in bomberman
				 /* if (dx != 0 && dy != 0) 
				    Nodes[cx+dx][cy+dy].G = 14; // diagonals cost 14
				  else*/ 
				    Nodes[cx+dx][cy+dy].G = 10; // straights cost 10
				  //work out H
				  //MANHATTAN METHOD
				  Nodes[cx+dx][cy+dy].H =
							(_abs(EndX-(cx+dx)) + 
							 _abs(EndY-(cy+dy)))*10;
				  Nodes[cx+dx][cy+dy].F =
							Nodes[cx+dx][cy+dy].G + 
							Nodes[cx+dx][cy+dy].H;

				}
				//otherwise it is on the open list
				// We can`t use diagonals in bomberman
				/*else if ( Nodes[cx+dx][cy+dy].IsOnClose == false 
						  && Nodes[cx+dx][cy+dy].IsOnOpen == true )
				{
				  if ( dx == 0 || dy == 0)   // if its not a diagonal
				  {
				     if ( Nodes[cx+dx][cy+dy].G == 14 )   //and it was previously
					 {
					   Nodes[cx+dx][cy+dy].G = 10; // straight score 10
					   //change its parent because its a shorter distance
					   Nodes[cx+dx][cy+dy].ParentX = cx;
					   Nodes[cx+dx][cy+dy].ParentY = cy;
					   //recalc H
					   Nodes[cx+dx][cy+dy].H = 
					  					 (_abs(EndX-(cx+dx)) + 
										    _abs(EndY-(cy+dy))) * 10;
					   //recalc F
					   Nodes[cx+dx][cy+dy].F = 
										 Nodes[cx+dx][cy+dy].G 
										 + Nodes[cx+dx][cy+dy].H;
					 }
				  }

				}//end else
				*/
			}// end if walkable and not on closed list
		  }
		}
	  }
	}
  }
  
  //follow all the parents back to the start
  
  cx = EndX;
  cy = EndY;
  Path->Distance = 0;
  
  while (cx != StartX || cy != StartY)
  {
	Path->Path[Path->Distance].X = Nodes[cx][cy].ParentX;
	Path->Path[Path->Distance].Y = Nodes[cx][cy].ParentY;
	cx = Nodes[cx][cy].ParentX;
	cy = Nodes[cx][cy].ParentY;
	Path->Distance++;
	if (Path->Distance > 100)
	{
		//rb->splash(HZ, "Too long path");
	  break;
	}
  }
  //rb->splash(HZ, "End");
  //return Path; //we're done, return a pointer to the final path;
}

void MovePlayer(Game *G, Player *P, PATH *Path)
{
	//rb->splash(HZ * 0.02, "move");
	if (P->xpos < Path->Path[Path->Distance - 2].X)
	  PlayerMoveRight(G, P);
	else if (P->xpos > Path->Path[Path->Distance - 2].X)
	  PlayerMoveLeft(G, P);
	else if (P->ypos < Path->Path[Path->Distance - 2].Y)
	  PlayerMoveDown(G, P);
	else if (P->ypos > Path->Path[Path->Distance - 2].Y) 
	  PlayerMoveUp(G, P);  
}

void CopyPaths(PATH *Dst, PATH *Src)
{
	int i;
	
	Dst->Distance = Src->Distance;
	for (i = 0; i < Src->Distance; i++)
	{
	  Dst->Path[i].X = Src->Path[i].X;
	  Dst->Path[i].Y = Src->Path[i].Y;
	}
}

void UpdateAI(Game *G, Player *Players)
{
  int i, j;
  PATH Path, CurPath;
  int MinDist = UNREAL_F;
  int dx, dy;
  
  //rb->splash(HZ, "Ai");
  
  for (i = 0; i < MAX_PLAYERS; i++)
  {
		MinDist = UNREAL_F;
		rb->memset(&CurPath, 0, sizeof(PATH));
		if (Players[i].IsAIPlayer == true)
		{
			//rb->splash(HZ, "Ai2");
			 for (j = 0; j < MAX_PLAYERS; j++)
			 {
				if (j == i)
				  continue;
				rb->memset(&Path, 0, sizeof(PATH));
				FindPath(&Path, Players[i].xpos, Players[i].ypos, 
				  Players[j].xpos, Players[j].ypos);
				if (Path.Distance < MinDist)
				{
				  MinDist = Path.Distance;
				  CopyPaths(&CurPath, &Path);
				  //CurPath = Path;
				}
			}
			//rb->splash(HZ, "GGG");
			MovePlayer(G, &Players[i], &CurPath);
			/*for(dx = -1; dx < 2; dx++)
			  for(dy = -1; dy < 2; dy++)
			    if(G->field.map[Players[i].xpos + dx][Players[i].ypos + dy] == SQUARE_BOX
			       && (dx == Players[i].rxpos && dy == Players[i].rypos))
			    {
						//rb->splash(HZ, "GGG");
			      PlayerPlaceBomb(G, &Players[i]);
			      break;
					}*/
		}
	}
}
