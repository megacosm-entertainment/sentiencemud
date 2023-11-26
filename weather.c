/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "olc.h"

/*

 TODO: MASSIVE REWORK NEEDED

*/

#if 0
/**
 * With storms we only get the most important storm. If a tornado and a rain storm are over a city,
 * then only the tornado will have affect. Simplifies having to worry about different
 * combinations of storms hitting an area.
 *
 *
 * Basically wilderness has a list of storms, you can check whether an area is in the wilds as it will
 * have x and y area coords set. The actual storms are added to wilderness area, and affected_by_storm and
 * storm_close are set on areas if their x and y coords fall in the radius of the storms.
 *
 * I have lots of code duplication with working out which is the most important storm. This is due to
 * being too lazy to come up with something better.
 */

// Creates a new storm
STORM_DATA* create_storm(AREA_DATA *pArea, int storm_type, int x, int y, int radius, float dx, float dy, int speed, int life) {
  STORM_DATA *storm = NULL;
  char buf[MSL];

  // create new storm structure
  storm = new_storm_data();

  // Add storm to area
  storm->next  = pArea->storm;
  pArea->storm = storm;

  // Set parameters
  storm->storm_type = storm_type;
  storm->x = x;
  storm->y = y;
  storm->radius = radius;

  // Direction and speed
  storm->dx = dx;
  storm->dy = dy;
  storm->counter = number_range(0, 360);
  storm->speed = speed;
  storm->life = life;

  sprintf(buf, "Create storm type %d at %d %d of radius %d in direction %f %f at speed %d and life %d",
     storm_type, x, y, radius, dx, dy, speed, life);
  log_string(buf);

  return storm;
}

// Remove the storm
void remove_storm(AREA_DATA *pArea, STORM_DATA *storm) {
	if (storm == pArea->storm)
	{
		pArea->storm = storm->next;
	}
	else
	{
		STORM_DATA *prev;

		for (prev = pArea->storm; prev != NULL;
				prev = prev->next)
		{
			if (prev->next == storm)
			{
				prev->next = storm->next;
				break;
			}
		}
	}
	storm->next     = NULL;
}

// handles weather update for wilderness and netherworld
/*void update_weather() {
  AREA_DATA *pArea = NULL;
  AREA_DATA *pArea2 = NULL;
  STORM_DATA *storm = NULL;
  STORM_DATA *storm_next = NULL;
  char buf[MSL];
  int storms = 0;
  int min_storms = 30;

  log_string("Updating weather...");

  // get wilderness area
  if ((pArea = find_area("Wilderness")) == NULL)
      return;

  // check number of storms on map
  for (storm = pArea->storm; storm != NULL; storm = storm_next) {
    storm_next = storm->next;

    // increase count of storms
    storms++;

    // decrease life of storm
    storm->life--;

    // update the storm (direction etc)
    storm->x += storm->dx * storm->speed;
    storm->y += storm->dy * storm->speed;

    // increase counter
    if (number_percent() < 10) {
      storm->counter += number_range(-180, 180);
    }
    else {
      storm->counter++;
    }

    // Once it hits 360 back to 0 again
    storm->counter = storm->counter % 360;

    // update directions
    //storm->dx = cos( storm->counter * 3.1415926 / 180 );
    //storm->dy = sin( storm->counter * 3.1415926 / 180 );


    if (storm->x < 0 || storm->y < 0 || storm->life < 0) {
      remove_storm(pArea, storm);
    }


  //sprintf(buf, "Updated storm type %d at %d %d of radius %d in direction %f %f at speed %d and life %d",
   //  storm->storm_type, storm->x, storm->y, storm->radius, storm->dx, storm->dy, storm->speed, storm->life);
  //log_string(buf);

    // iterate through all areas, if they are in the wilderness show warning of storm
    for (pArea2 = area_first; pArea2 != NULL; pArea2 = pArea2->next) {
      pArea2->affected_by[0]_storm = NULL;
      pArea2->storm_close = NULL;

      if (pArea2->x > 0 && pArea2->y > 0) {
         long distance;

				 distance = (int) sqrt( 					\
									( storm->x - pArea2->x ) *	\
									( storm->x - pArea2->x ) +	\
									( storm->y - pArea2->y ) *	\
									( storm->y - pArea2->y ) );

         if (distance < storm->radius) {
             if ((storm->storm_type == WEATHER_TORNADO && pArea2->affected_by[0]_storm != NULL && pArea2->affected_by[0]_storm->storm_type == WEATHER_HURRICANE) ||
              (storm->storm_type == WEATHER_HURRICANE && pArea2->affected_by[0]_storm != NULL && pArea2->affected_by[0]_storm->storm_type == WEATHER_SNOW_STORM) ||
              (storm->storm_type == WEATHER_SNOW_STORM && pArea2->affected_by[0]_storm != NULL && pArea2->affected_by[0]_storm->storm_type == WEATHER_LIGHTNING_STORM) ||
              (storm->storm_type == WEATHER_LIGHTNING_STORM && pArea2->affected_by[0]_storm != NULL && pArea2->affected_by[0]_storm->storm_type == WEATHER_RAIN_STORM)) {
               pArea2->affected_by[0]_storm = storm;
						 }
             else {
               pArea2->affected_by[0]_storm = storm;
             }
         }
         else
         if (distance < storm->radius + 10) {
           if (pArea2->storm_close != NULL) {
             if ((storm->storm_type == WEATHER_TORNADO && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_HURRICANE) ||
              (storm->storm_type == WEATHER_HURRICANE && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_SNOW_STORM) ||
              (storm->storm_type == WEATHER_SNOW_STORM && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_LIGHTNING_STORM) ||
              (storm->storm_type == WEATHER_LIGHTNING_STORM && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_RAIN_STORM)) {
               pArea2->storm_close = storm;
						 }
             else {
               pArea2->storm_close = storm;
             }
           }
         }

         if (pArea2->affected_by[0]_storm != NULL) {
         sprintf(buf, "%s's is affected by type %d at %d, %d",
              pArea2->name, pArea2->affected_by[0]_storm->storm_type, pArea2->affected_by[0]_storm->x, pArea2->affected_by[0]_storm->y);
         log_string(buf);
				 }

         if (pArea2->storm_close != NULL) {
         sprintf(buf, "%s's closest storm is storm type %d at %d, %d.",
              pArea2->name,
              pArea2->storm_close->storm_type,
              pArea2->storm_close->x,
              pArea2->storm_close->y);
         log_string(buf);
         }
      }

    }
  }

  // if storms < 30 create some more
  while (storms < min_storms) {
    int x, y, radius, speed, life;
    float dx, dy;
    int angle = 0;
    int storm_type;

    // increase storms counter as we are making another
    storms++;

    x = number_range(0, pArea->map_size_x);
    y = number_range(0, pArea->map_size_y);

    angle = number_range(0, 360);
    dx = cos(angle * 3.1415926 / 180);
    dy = sin(angle * 3.1415926 / 180);

    life = number_range(20, 100);
    speed = number_range(2, 5);

    storm_type = number_range(1, 5);
    switch(storm_type) {
      case WEATHER_RAIN_STORM:

           radius = number_range(22, 55);

           if (number_percent() < 40) {
	   				 create_storm(pArea, WEATHER_LIGHTNING_STORM, x, y, radius-4, dx, dy, speed, life);
					 }
           if (number_percent() < 20) {
	   				 create_storm(pArea, WEATHER_SNOW_STORM, x, y, radius-7, dx, dy, speed, life);
           }
           if (number_percent() < 15) {
	   				 create_storm(pArea, WEATHER_HURRICANE, x, y, radius-9, dx, dy, speed, life);
           }
           break;
      case WEATHER_LIGHTNING_STORM:

           radius = number_range(20, 25);
           break;
      case WEATHER_SNOW_STORM:

           radius = number_range(15, 20 );
           break;
      case WEATHER_HURRICANE:

           radius = number_range(15, 20);
    			 speed = number_range(4, 7);
           break;
      case WEATHER_TORNADO:

           radius = number_range(10, 15);
    			 speed = number_range(4, 8);
           break;
      default:
           radius = 10;
    }

   // create storm
   create_storm(pArea, storm_type, x, y, radius, dx, dy, speed, life);
  }
}

// Commenting this out, since Whisp's weather system doesn't work yet -- Areo
void do_weather(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    STORM_DATA *storm = NULL;
    char buf[MAX_STRING_LENGTH];
    int x, y;
		int squares_to_show_x = 0;
		int squares_to_show_y = 0;
    int chx, chy;
    ROOM_INDEX_DATA *pRoom;

		squares_to_show_x = get_squares_to_show_x(4);
		squares_to_show_y = get_squares_to_show_y(4);

    // Get area
    pArea = ch->in_room->area;

    buf[0] = '\0';

    if (ON_SHIP(ch)) {
      pRoom = ch->in_room->ship->ship->in_room;
      pArea = pRoom->area;
      chx = pRoom->x;
      chy = pRoom->y;
    }
    else
    if (IN_WILDERNESS(ch)) {
      chx = ch->in_room->x;
      chy = ch->in_room->y;
    }
    else
    if (IS_OUTSIDE(ch) && pArea->x > 0 && pArea->y > 0) {
      chx = pArea->x;
      chy = pArea->y;
    }
    else {
			send_to_char("You can't see the weather here.\n\r", ch);
			return;
    }

    if (IN_NETHERWORLD(ch)) {
	send_to_char("Thick rolling clouds tumble and turn. Lightning crashes to the ground all around you.\n\r", ch);
	return;
    }

    // Run through storms in area
    for (y = 0; y < squares_to_show_y*2; y++) {
        for (x = 0; x < squares_to_show_x*2; x++) {
          int lx, ly;
					bool found_rain_storm = FALSE;
					bool found_lightning_storm = FALSE;
					bool found_snow_storm = FALSE;
					bool found_hurricane = FALSE;
					bool found_tornado = FALSE;

          lx = chx - squares_to_show_x + x;
          ly = chy - squares_to_show_y + y;

          for (storm = pArea->storm; storm != NULL; storm = storm->next) {
               double field = 0;
    					 double radius = 0;

    					 radius = (double)1 / (double)(2*storm->radius * storm->radius);

							 field =  (double) 1 /
												(double)( ( storm->x - lx ) *	\
												( storm->x - lx ) +	\
												( storm->y - ly ) *	\
												( storm->y - ly ) );

                if ( field >= radius ) {
                    switch(storm->storm_type) {
											case WEATHER_RAIN_STORM:
													 found_rain_storm = TRUE;
													 break;
											case WEATHER_LIGHTNING_STORM:
 													 found_lightning_storm = TRUE;
													 break;
											case WEATHER_SNOW_STORM:
 													 found_snow_storm = TRUE;
													 break;
											case WEATHER_HURRICANE:
 													 found_hurricane = TRUE;
													 break;
											case WEATHER_TORNADO:
 													 found_tornado = TRUE;
													 break;
 										}
                 }
                 //sprintf(buf2, "radius was %f, field was %f", radius, field);
                 //log_string(buf2);
						}

            if (x == squares_to_show_x && y == squares_to_show_y) {
                strcat(buf, "{M@{x");
            }
            else
            if (found_tornado) {
								strcat(buf, "{GT{x");
            }
            else
            if (found_hurricane) {
								strcat(buf, "{RH{x");
            }
            else
            if (found_snow_storm) {
								strcat(buf, "{WS{x");
            }
            else
            if (found_lightning_storm) {
								strcat(buf, "{YL{x");
            }
            else
            if (found_rain_storm) {
								strcat(buf, "{BR{x");
            }
            else {
               strcat(buf, ".");
            }
        }
        strcat(buf, "\n\r");
    }

    strcat(buf, "{GT{x - Tornado   {RH{x - Hurricane   {WS{x - Snow Storm   {YL{x - Lightning Storm\n\r"
                "{BR{x - Rain Storm\n\r");
    strcat(buf, "\0");

    send_to_char(buf, ch);
    return;
}*/

int get_storm_for_room(ROOM_INDEX_DATA *pRoom)
{
    AREA_DATA *pArea;
    AREA_DATA *pWilderness;
    STORM_DATA *storm = NULL;
    int lx, ly;
    int storm_type = WEATHER_NONE;
		bool found_rain_storm = FALSE;
		bool found_lightning_storm = FALSE;
		bool found_snow_storm = FALSE;
		bool found_hurricane = FALSE;
		bool found_tornado = FALSE;

    // Get area
    pArea = pRoom->area;
    pWilderness = find_area("Wilderness");

    lx = pRoom->x;
    ly = pRoom->y;

    // Check if room is in an area affected by a storm
    if (pWilderness != NULL && str_cmp(pArea->name, pWilderness->name)) {

	    if (pRoom->area->affected_by[0]_storm != NULL) {
		    storm_type = pRoom->area->affected_by[0]_storm->storm_type;
	    }
	    // Close storms
	    else
		    if (pRoom->area->storm_close != NULL) {
			    storm_type = pRoom->area->storm_close->storm_type;
		    }
    }
    else {

	    // Run through all storms in the wilderness and see which is affecting room
	    for (storm = pRoom->area->storm; storm != NULL; storm = storm->next) {
		    int distance;

		    distance = (int) sqrt( 					\
				    ( storm->x - lx ) *	\
				    ( storm->x - lx ) +	\
				    ( storm->y - ly ) *	\
				    ( storm->y - ly ) );

         if (distance < storm->radius) {
					switch(storm->storm_type) {
								case WEATHER_RAIN_STORM:
										 found_rain_storm = TRUE;
										 break;
								case WEATHER_LIGHTNING_STORM:
										 found_lightning_storm = TRUE;
										 break;
								case WEATHER_SNOW_STORM:
										 found_snow_storm = TRUE;
										 break;
								case WEATHER_HURRICANE:
										 found_hurricane = TRUE;
										 break;
								case WEATHER_TORNADO:
										 found_tornado = TRUE;
										 break;
							}
    		 }
      }

			if (found_tornado) {
				 storm_type = WEATHER_TORNADO;
			}
			else
			if (found_hurricane) {
				 storm_type = WEATHER_HURRICANE;
			}
			else
			if (found_snow_storm) {
				 storm_type = WEATHER_SNOW_STORM;
			}
			else
			if (found_lightning_storm) {
				 storm_type = WEATHER_LIGHTNING_STORM;
			}
			else
			if (found_rain_storm) {
				 storm_type = WEATHER_RAIN_STORM;
			}
			else {
          // Check if any storms are close
					for (storm = pRoom->area->storm; storm != NULL; storm = storm->next) {
						 int distance;

						 distance = (int) sqrt( 					\
											( storm->x - lx ) *	\
											( storm->x - lx ) +	\
											( storm->y - ly ) *	\
											( storm->y - ly ) );

						 if (distance < storm->radius + 10) {
							switch(storm->storm_type) {
										case WEATHER_RAIN_STORM:
												 found_rain_storm = TRUE;
												 break;
										case WEATHER_LIGHTNING_STORM:
												 found_lightning_storm = TRUE;
												 break;
										case WEATHER_SNOW_STORM:
												 found_snow_storm = TRUE;
												 break;
										case WEATHER_HURRICANE:
												 found_hurricane = TRUE;
												 break;
										case WEATHER_TORNADO:
												 found_tornado = TRUE;
												 break;
									}
						 }
         }
				if (found_tornado) {
					 storm_type = WEATHER_TORNADO;
				}
				else
				if (found_hurricane) {
					 storm_type = WEATHER_HURRICANE;
				}
				else
				if (found_snow_storm) {
					 storm_type = WEATHER_SNOW_STORM;
				}
				else
				if (found_lightning_storm) {
					 storm_type = WEATHER_LIGHTNING_STORM;
				}
				else
				if (found_rain_storm) {
					 storm_type = WEATHER_RAIN_STORM;
				}
			  else {
           storm_type = WEATHER_NONE;
        }
			}
		}
    return storm_type;
}

// Affect chars caught in storm.
void storm_affect_char args((CHAR_DATA *ch, int storm_type)) {
  ROOM_INDEX_DATA *pRoom;
  long index = 0;
  AREA_DATA *pArea;
  int sector_type = ch->in_room->sector_type;
  AFFECT_DATA af;
  memset(&af,0,sizeof(af));

  switch(storm_type) {

    // Rain
    case WEATHER_RAIN_STORM:


      // Dont show too much information about it raining
      if (number_percent() < 33) {
      // Show that is is raining
			switch(number_range(0, 5)) {
				case 0:
						send_to_char("{CThe rain falls harder causing the puddles to splash everywhere.{x\n\r", ch);
						break;
				case 1:
						send_to_char("{CThe rain soften to a quiet eerie whisper.{x\n\r", ch);
						break;
				case 2:
						send_to_char("{CRain from the clouds above you saturate the surrounding area.{x\n\r", ch);
						break;
				case 3:
						send_to_char("{CYou are saturated with water from the rain.{x\n\r", ch);
						break;
				case 4:
						send_to_char("{CRain falls from above splashing as it hits the ground.{x\n\r", ch);
						break;
        default:
					switch(sector_type) {
						case SECT_CITY:
							send_to_char("{CRain makes a melodic drumbeat as it falls on the rooftops.{x\r\n", ch);
							break;
						case SECT_FOREST:
							send_to_char("{CThe raindrops falling on the forest around you makes a soothing sound.{x\r\n", ch);
							break;
						case SECT_FIELD:
							send_to_char("{CThe thirsty fields drink up the rain as it begins falling.{x\r\n", ch);
							break;
						case SECT_MOUNTAIN:
              send_to_char("{CThe rain trickles down into the mountain valleys.{x\r\n", ch);
							break;
						case SECT_DOCK:
              send_to_char("{CThe rain beats chaotically on the wooden boards holding the pier.{x\r\n", ch);
							break;
						case SECT_DESERT:
              send_to_char("{CThe desert enjoys its rare taste of rain as it spatters into the sand.{x\r\n", ch);
							break;
						case SECT_WATER_NOSWIM:
              send_to_char("{CThe rain beats chaotic on the surface.{x\r\n", ch);
							break;
						default:
              send_to_char("{CRain pours mercilessly down upon you.{x\r\n", ch);
					}
		  }
      }
      break;

    // Snow
    case WEATHER_SNOW_STORM:

      // Dont show too much information about it raining
      if (number_percent() < 33) {
      // Show that is is snowing
			switch(number_range(0, 8)) {
				case 0:
						send_to_char("{WSnow flakes fall gently to the ground.{x\n\r", ch);
						break;
				case 1:
						send_to_char("{WSnow litters the ground as it pours down from the clouds.{x\n\r", ch);
						break;
				case 2:
						send_to_char("{WThe snowfall quickens and the temperature drops.{x\n\r", ch);
						break;
				case 3:
						send_to_char("{WCold snow powers to the ground from the clouds above.{x\n\r", ch);
						break;
				case 4:
						send_to_char("{WThe temperature drops as snow falls from the sky.{x\n\r", ch);
						break;
        case 5:
        case 6:
        case 7:
	        cold_effect(ch, 1, dice(4,8), TARGET_CHAR);
          break;
        default:
					switch(sector_type) {
						case SECT_CITY:
							send_to_char("{WBig fluffy flakes of snow collect in the city streets.{x\r\n", ch);
							break;
						case SECT_FOREST:
							send_to_char("{WFlakes of snow fall softly, escaping the canopy of leaves above.{x\r\n", ch);
							break;
						case SECT_FIELD:
							send_to_char("{WSnowflakes collect in the field, freezing the undergrowth.{x\r\n", ch);
							break;
						case SECT_MOUNTAIN:
              send_to_char("{WThe thick snow obscures the nearby mountains.{x\r\n", ch);
							break;
						case SECT_DOCK:
              send_to_char("{WThe rain beats chaotically on the wooden boards holding the pier.{x\r\n", ch);
							break;
						case SECT_DESERT:
              send_to_char("{WThe desert enjoys its rare taste of rain as it spatters into the sand.{x\r\n", ch);
							break;
						case SECT_WATER_NOSWIM:
              send_to_char("{WThe snowflakes melt instantly as they touch the water.{x\r\n", ch);
							break;
						default:
              send_to_char("{WBig, fat, fluffy flakes fall from a heavy-laden sky.{x\n\r", ch);
					}
			}
			break;
		}

    // Lightning
    case WEATHER_LIGHTNING_STORM:

      // Dont show too much information about lightning storm
      if (number_percent() < 33) {

      // Show that there is a lightning storm
			switch(number_range(0, 7)) {
				case 0:
						send_to_char("A billow of thunder explodes around you.\n\r", ch);
						break;
				case 1:
							send_to_char("{YFlashes of lightning streak across the sky.{x\n\r", ch);
						break;
				case 2:
            if (ch->in_room->sector_type != SECT_WATER_SWIM &&
                ch->in_room->sector_type != SECT_WATER_NOSWIM) {
							send_to_char("The low rumble of thunder shakes the ground.\n\r", ch);
            }
						break;
				case 3:
						send_to_char("{YBrilliant flashes light up the clouds above.{x\n\r", ch);
						break;
				case 4:
            if (ch->in_room->sector_type != SECT_WATER_SWIM &&
                ch->in_room->sector_type != SECT_WATER_NOSWIM) {
							send_to_char("{YLightning crashes to the ground next to you!{x\n\r", ch);
            }
						break;
				case 5:
						if (number_percent() < 30 && !IS_SET(ch->in_room->room_flag[0], ROOM_SAFE) && ch->fighting == NULL)
						{
							act("{YZAAAAAAAAAAAAAAP! You are struck by a bolt from the sky...{x\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
							damage(ch, ch, number_range(500,30000), 0, DAM_LIGHTNING, FALSE);
						}
						break;
        default:
					switch(sector_type) {
						case SECT_CITY:
							send_to_char("{BThunder echos off the building walls.\r\n{x", ch);
							break;
						case SECT_FOREST:
							send_to_char("{YLightning flashes above the canopy of the forest.\r\n{x", ch);
							break;
						case SECT_FIELD:
							send_to_char("{YForks of lightning streaks down across the fields.\r\n{x", ch);
							break;
						case SECT_MOUNTAIN:
              send_to_char("{BThunder echos off the surrounding mountains.\r\n{x", ch);
              break;
						case SECT_HILLS:
              send_to_char("{BThunder echos off the surrounding hills.\r\n{x", ch);
              break;
						case SECT_TUNDRA:
              send_to_char("{YForks of lightning strike the tundra.\r\n{x", ch);
              break;
						case SECT_AIR:
              send_to_char("{YLightning forks in all directions!\r\n{x", ch);
              break;
						case SECT_DOCK:
              send_to_char("{CThe rain beats chaotically on the wooden boards holding the pier.\r\n{x", ch);
							break;
						case SECT_DESERT:
              send_to_char("{YLightning crackles across the open desert.\r\n{x", ch);
							break;
						case SECT_WATER_NOSWIM:
              send_to_char("Lightning sparks across the open water.\r\n", ch);
							break;
						case SECT_NETHERWORLD:
              send_to_char("{YLightning chaotically forks between the dead ground and toxic sky.\n\r", ch);
							break;
						default:
              send_to_char("{YLightning chaotically forks in the sky.\n\r", ch);
					}
			}
      }
      break;

    // Hurricane
    case WEATHER_HURRICANE:

      // Show that there is a hurricane
			switch(number_range(0, 3)) {
				case 0:
						send_to_char("The wind threatens to carry you away. Now would be an excellent time to get indoors!\n\r", ch);
						break;
				case 1:
						send_to_char("The wind whips around you angrily!\n\r", ch);
						break;
				case 2:
            send_to_char("The wind screams around you with incredible force!\n\r", ch);
						break;
				case 3:
            send_to_char("The wind howls fiercely as it torments all in its path!\n\r", ch);
						break;
				case 4:
            send_to_char("The hurricane relentlessly destroys everything around you!\n\r", ch);
						break;
				case 5:
			      send_to_char("Something caught in the wind hits you hard!{x\n\r", ch);
            send_to_char("Out of nowhere something slams solidly into you.\n\r", ch);
            act("{R$n is struck by an object caught in the storm!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					  damage(ch, ch, 30000, 0, DAM_NONE, FALSE);
						break;
        default:
					switch(sector_type) {
						case SECT_CITY:
              if (number_percent() < 50)
							send_to_char("A begger is caught by the wind and carried off!\r\n", ch);
							else
						  send_to_char("Some fencing and a small chicken flies past you!\n\r", ch);
							break;
						case SECT_FOREST:
              if (number_percent() < 50) {
								send_to_char("A tree is ripped from the grounds and carried off in the wind!\r\n", ch);
						  }
              else {
						  	send_to_char("Trees around you begin to snap from the powerful winds!\n\r", ch);
							}
							break;
						case SECT_FIELD:
              if (number_percent() < 50)
							send_to_char("A powerfull wind roars across the fields.\r\n", ch);
              else
						  send_to_char("A cow go's shooting past you, taken by the hurricane winds!\n\r", ch);
							break;
						case SECT_MOUNTAIN:
              send_to_char("A mountain goat is carried away by the winds!\r\n", ch);
							break;
						case SECT_DOCK:
              send_to_char("The rough waters smash up against the pier!\r\n", ch);
							break;
						case SECT_DESERT:
              send_to_char("A sand storm forms from the powerful winds!\r\n", ch);
							af.slot	= WEAR_NONE;
							af.where	= TO_AFFECTS;
							af.group	= AFFGROUP_PHYSICAL;
							af.type 	= gsn_blindness;
							af.level 	= 10;
							af.duration	= 3;
							af.location	= APPLY_HITROLL;
							af.modifier	= -4;
							af.bitvector 	= AFF_BLIND;
							af.bitvector2 = 0;

							affect_to_char(ch,&af);
              send_to_char("{DYou are blinded by the sand!{x\n\r", ch);
              act("$n is blinded by the blowing sand!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
							break;
						case SECT_WATER_NOSWIM:
              send_to_char("The seas swell turning the waves into mountains!\r\n", ch);
							break;
						default:
              send_to_char("The powerful winds of the hurricane threaten to carry you away!\n\r", ch);
					}
			}
      break;

    // Tornado
    case WEATHER_TORNADO:

      // Show that there is a tornado
			switch(number_range(0, 8)) {
        default:
					switch(sector_type) {
						case SECT_CITY:
              if (number_percent() < 20)
							send_to_char("An old man is caught by the wind and consumed by the tornado!\r\n", ch);
							else
              if (number_percent() < 20)
						  send_to_char("The buildings groan as they are slowly disected by the tornado!\n\r", ch);
							else
              if (number_percent() < 20)
              send_to_char("The litter in the city streets disappears into the tornado!\n\r", ch);
							else
              if (number_percent() < 20)
              send_to_char("The stray cat whimpers as a the tonado consumes it!\n\r", ch);
							break;
						case SECT_FOREST:
              if (number_percent() < 50) {
								send_to_char("A tree is ripped from the grounds and carried off in the wind!\r\n", ch);
						  }
              else {
						  	send_to_char("Trees around you begin to snap from the powerful winds!\n\r", ch);
							}
							break;
						case SECT_FIELD:
              if (number_percent() < 50)
							send_to_char("The giant spiralling tornado sucks plants and crops as it devestates the fields.\r\n", ch);
              else
						  send_to_char("A cow shoots past you, taken by the tornado!\n\r", ch);
							break;
						case SECT_MOUNTAIN:
              send_to_char("The stones and rocks of the mountain spiral into the tornado!\r\n", ch);
							break;
						case SECT_DOCK:
              send_to_char("The rough waters smash up against the pier!\r\n", ch);
							break;
						case SECT_DESERT:
              send_to_char("A sand storm forms from the powerful winds!\r\n", ch);
							af.slot	= WEAR_NONE;
							af.where	= TO_AFFECTS;
							af.type 	= skill_lookup("blindness");
							af.level 	= 10;
							af.duration	= 3;
							af.location	= APPLY_HITROLL;
							af.modifier	= -4;
							af.bitvector 	= AFF_BLIND;
							af.bitvector2 = 0;

							affect_to_char(ch,&af);
              send_to_char("{DYou are blinded by the sand!{x\n\r", ch);
              act("$n is blinded by the blowing sand!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
							break;
						case SECT_WATER_NOSWIM:
              send_to_char("The seas swell turning the waves into mountains!\r\n", ch);
							break;
				}
				case 0:
						send_to_char("The monstrous swirling storm brings devestation as it rips all that stand in front!\n\r", ch);
						break;
				case 1:
            if (ch->in_room->sector_type != SECT_WATER_SWIM &&
                ch->in_room->sector_type != SECT_WATER_NOSWIM) {
							send_to_char("The ground beneath you groans as the tornado tugs at its core.\n\r", ch);
						}
						break;
				case 2:
						send_to_char("You struggle to save yourself from the tornado!\n\r", ch);
						break;
				case 3:
            if (ch->in_room->sector_type != SECT_WATER_SWIM &&
                ch->in_room->sector_type != SECT_WATER_NOSWIM) {
						send_to_char("Objects around you are torn from the earth and fed to the mighty tornado!\n\r", ch);
						}
						break;
				case 4:
			      send_to_char("The sky groans loudly as the clouds above swirl chaotically.\n\r", ch);
						break;
				case 5:
			      send_to_char("{RYou are sucked up into the tornado!{x\n\r", ch);
            send_to_char("Out of nowhere something slams solidly into you.\n\r", ch);
            act("{R$n is struck by an object caught in the storm!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					  damage(ch, ch, 30000, 0, DAM_NONE, FALSE);
						break;
				case 6:
            pArea = find_area("Wilderness");
            index = (long)((long)number_range(0, pArea->map_size_y) * pArea->map_size_x + (long)number_range(0, pArea->map_size_x) + pArea->min_vnum + WILDERNESS_VNUM_OFFSET);

			      send_to_char("{RYou are sucked up into the tornado!{x\n\r", ch);

            act("{R$n is sucked up into the tornado!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

            if ((pRoom = get_room_index(NULL, index)) != NULL) {
                char_from_room(ch);
                char_to_room(ch, pRoom);
	    				  do_function(ch, &do_look, "auto");
						}

            if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING)) {
              send_to_char("You are thrown wildly around in circles and find yourself plummeting to the ground!\n\r", ch);
			        send_to_char("{RYou hit the ground with a loud thump!!!{x\n\r", ch);
					   	damage(ch, ch, 30000, 0, DAM_NONE, FALSE);
            }
            else {
              send_to_char("You are thrown ferociously out of the tornado.\n\r", ch);
              send_to_char("{RYou hit the ground hard, you manage to fly up enough to soften your landing.\n\r", ch);
					   	damage(ch, ch, number_range(10,ch->max_hit), 0, DAM_NONE, FALSE);
            }
						break;
			}
      break;
  }
}

// Affect chars who see storm in background.
void storm_affect_char_background args((CHAR_DATA *ch, int storm_type)) {

  switch(storm_type) {

    // None
    case WEATHER_NONE:
      switch(number_range(0,6)) {
         default:
            break;
         case 0:
            switch(weather_info.sunlight) {
              case SUN_RISE:
              case SUN_LIGHT:
                switch(number_range(0,3)) {
                  case 0:
                      send_to_char("The cloudless sky bathes you in sunshine.\n\r", ch);
                      break;
                  case 1:
                      send_to_char("The warm sunshine beams brightly around you.\n\r", ch);
                      break;
                  case 2:
                      send_to_char("The bright sunshine warms you.\n\r", ch);
                      break;
                  case 3:
                      send_to_char("The warm sun beams down through a cloudless blue sky.\n\r", ch);
                      break;
								}
                break;
              case SUN_SET:
              case SUN_DARK:
                switch(number_range(0,3)) {
                  case 0:
                      send_to_char("The stars twinkle in the cloudless sky.\n\r", ch);
                      break;
                  case 1:
                      send_to_char("A shooting star snakes across the cloudless sky.\n\r", ch);
                      break;
                  case 2:
                      send_to_char("The land is illuminated by the moon.\n\r", ch);
                      break;
                  case 3:
                      send_to_char("The stars above you twinkle in the clear sky.\n\r", ch);
                      break;
								}
                break;
						}
      }
    break;

    // Rain
    case WEATHER_RAIN_STORM:
      switch(number_range(0,3)) {
        case 0:
  	      send_to_char("Dark rain clouds gather in the distance.\n\r", ch);
          break;
        case 1:
  	      send_to_char("A hazy mist of rain clouds loom in the distance.\n\r", ch);
          break;
      }
      break;

    // Snow
    case WEATHER_SNOW_STORM:
      switch(number_range(0,3)) {
        case 0:
  	      send_to_char("The temperature begins to cool as snow clouds loom.\n\r", ch);
          break;
        case 1:
  	      send_to_char("Gusts of wind blow snowflakes in.\n\r", ch);
          break;
      }
      break;

    // Lightning
    case WEATHER_LIGHTNING_STORM:
      switch(number_range(0,3)) {
        case 0:
  	      send_to_char("Thunder rolls in from the distant storm.\n\r", ch);
          break;
        case 1:
  	      send_to_char("{YFlashes of lightning appear in the distance.{x\n\r", ch);
          break;
      }
      break;

    // Hurricane
    case WEATHER_HURRICANE:
      switch(number_range(0,3)) {
        case 0:
  	      send_to_char("Their is a sudden calming stillness in the air.\n\r", ch);
          break;
        case 1:
  	      send_to_char("In the distance you see the foreboding signs of a hurricane!\n\r", ch);
          break;
      }
      break;

    // Tornado
    case WEATHER_TORNADO:
      switch(number_range(0,3)) {
        case 0:
  	      send_to_char("You see a large column of swiling wind causing devestation nearby!\n\r", ch);
          break;
        case 1:
  	      send_to_char("The defeaning roar of a tornado can be heard nearby!\n\r", ch);
          break;
      }
      break;
  }
}

/*void update_weather_for_chars() {
	CHAR_DATA *ch = NULL;
	STORM_DATA *storm = NULL;
	DESCRIPTOR_DATA *d;

	for (d = descriptor_list; d != NULL; d = d->next)
	{
		if (d->character == NULL || d->character->in_room == NULL) {
			continue;
		}
		ch = d->character;

		if (!IS_AWAKE(d->character)) {
			continue;
		}

		// Netherworld is always a lightning storm
		if (IN_NETHERWORLD(ch)) {
			storm_affect_char(ch, WEATHER_LIGHTNING_STORM);
			return;
		}
		else
			// Edit is always clear
			if (IN_EDEN(ch)) {
				storm_affect_char(ch, WEATHER_NONE);
				return;
			}
			else {

				// Update weather
				if (IN_WILDERNESS(ch) || (IS_OUTSIDE(ch) && ch->in_room->area->x > 0 && ch->in_room->area->y > 0)) {
					int storm_type;

					// Close storms
					if (ch->in_room->area->storm_close != NULL) {
						storm_type = ch->in_room->area->storm_close->storm_type;

						storm_affect_char_background(ch, storm->storm_type);
					}

					// Handle affects on player from storm
					if (ch->in_room->area->affected_by[0]_storm != NULL) {
						storm_affect_char(ch, storm->storm_type);
					}

					//  Handle close storms in the wilderness
					if (IN_WILDERNESS(ch)) {
						STORM_DATA *storm;
						bool found_rain_storm = FALSE;
						bool found_lightning_storm = FALSE;
						bool found_snow_storm = FALSE;
						bool found_hurricane = FALSE;
						bool found_tornado = FALSE;
						int storm_type;

						// Run through all storms in the wilderness and see which is affecting player
						for (storm = ch->in_room->area->storm; storm != NULL; storm = storm->next) {
							int distance;

							distance = (int) sqrt( 					\
									( storm->x - ch->in_room->x ) *	\
									( storm->x - ch->in_room->x ) +	\
									( storm->y - ch->in_room->y ) *	\
									( storm->y - ch->in_room->y ) );

							if (distance < storm->radius) {
								switch(storm->storm_type) {
									case WEATHER_RAIN_STORM:
										found_rain_storm = TRUE;
										break;
									case WEATHER_LIGHTNING_STORM:
										found_lightning_storm = TRUE;
										break;
									case WEATHER_SNOW_STORM:
										found_snow_storm = TRUE;
										break;
									case WEATHER_HURRICANE:
										found_hurricane = TRUE;
										break;
									case WEATHER_TORNADO:
										found_tornado = TRUE;
										break;
								}
							}
						}

						if (found_tornado) {
							storm_type = WEATHER_TORNADO;
							storm_affect_char(ch, storm_type);
						}
						else
							if (found_hurricane) {
								storm_type = WEATHER_HURRICANE;
								storm_affect_char(ch, storm_type);
							}
							else
								if (found_snow_storm) {
									storm_type = WEATHER_SNOW_STORM;
									storm_affect_char(ch, storm_type);
								}
								else
									if (found_lightning_storm) {
										storm_type = WEATHER_LIGHTNING_STORM;
										storm_affect_char(ch, storm_type);
									}
									else
										if (found_rain_storm) {
											storm_type = WEATHER_RAIN_STORM;
											storm_affect_char(ch, storm_type);
										}
										else {
											// Check if any storms are close
											for (storm = ch->in_room->area->storm; storm != NULL; storm = storm->next) {
												int distance;

												distance = (int) sqrt( 					\
														( storm->x - ch->in_room->x ) *	\
														( storm->x - ch->in_room->x ) +	\
														( storm->y - ch->in_room->y ) *	\
														( storm->y - ch->in_room->y ) );

												if (distance < storm->radius + 5) {
													switch(storm->storm_type) {
														case WEATHER_RAIN_STORM:
															found_rain_storm = TRUE;
															break;
														case WEATHER_LIGHTNING_STORM:
															found_lightning_storm = TRUE;
															break;
														case WEATHER_SNOW_STORM:
															found_snow_storm = TRUE;
															break;
														case WEATHER_HURRICANE:
															found_hurricane = TRUE;
															break;
														case WEATHER_TORNADO:
															found_tornado = TRUE;
															break;
													}
												}
											}
											if (found_tornado) {
												storm_type = WEATHER_TORNADO;
											}
											else
												if (found_hurricane) {
													storm_type = WEATHER_HURRICANE;
												}
												else
													if (found_snow_storm) {
														storm_type = WEATHER_SNOW_STORM;
													}
													else
														if (found_lightning_storm) {
															storm_type = WEATHER_LIGHTNING_STORM;
														}
														else
															if (found_rain_storm) {
																storm_type = WEATHER_RAIN_STORM;
															}
															else {
																storm_type = WEATHER_NONE;
															}

											storm_affect_char_background(ch, storm_type);
										}
					}
				}
			}
	}
}*/
#endif