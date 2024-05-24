/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Snakes tool implementation
 *  Copyright (C) 2001 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* ANSI headers */
#include <assert.h>
#include "stdio.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "flex.h"
#include "macros.h"
#include "debug.h"

#include "Snakes.h"
#include "SnakesData.h"
#include "MapCoord.h"
#include "Utils.h"

#define UX_STARTSNAKEMARK "StartSnake"
#define STARTSNAKEMARK UX_STARTSNAKEMARK" '%11s'\n"
#define UX_ENDSNAKEMARK "EndSnake"
#define ENDSNAKEMARK UX_ENDSNAKEMARK"\n"

/*
  SNAKE_INSIDE is a fudge to allow the snakes tool to double as an edging tool:
Unlike simple road pieces, edging bits have 'sides' and therefore
directionality.
  We could have stored 'proper' left-handed and right-handedness for each
individual edge of a snake tile. This would have allowed things such as side-
swap pieces (and 16 distinct types of four-way junction) but would have required
8 bits of data per snake tile.
  Instead, we take advantage of the fact that edging tiles generally have only
2 exits, and that the change-overs between inside and outside are not random,
but follow a predictable pattern:
  A single flag serves to distinguish between the 'inside' and 'outside'
versions of a given tile, where 'inside' means edging on the right/bottom and
'outside' means edging on the left/top. Change-overs occur automatically at
West+South and North+East corner pieces. When deciding whether corner or
junction tiles are 'inside' or 'outside', the horizontal rule takes
precedence - tiles with bottom edging are deemed to be 'inside', regardless of
any lefthand edging.
  This reduces the total number of snake pieces to 2^5 (32) rather than 2^8
(256), and thus allows relatively succinct definitions and also speedy lookup
from a table of manageable size. Junctions are badly represented by this
scheme, but then junctions in edging are not very meaningful anyway.
  The table ordering goes as follows (binary 0-15): North, East, North+E,
South, S+N, S+E, S+E+N, West, W+N, W+E, W+E+N, W+S, W+S+N, W+E+S, W+S+E+N. The
equivalent 'Inside' tiles follow (binary 16-31): Inside+N, I+E, I+N+E... etc.
*/

#define SNAKE_NORTH  (1u << MapAngle_North)
#define SNAKE_EAST   (1u << MapAngle_East)
#define SNAKE_SOUTH  (1u << MapAngle_South)
#define SNAKE_WEST   (1u << MapAngle_West)
#define SNAKE_INSIDE (1u << MapAngle_Count) /* edging on right/bottom rather than left/top? */
#define SNAKE_ALL    ((1u << (MapAngle_Count + 1)) - 1)

enum {
  LineBufferSize = 255,
  InitSnakesArraySize = 8,
  SnakesArrayGrowthFactor = 2,
};

typedef struct {
  char name[16];
  /* Partial mapping from part specification to texture/object.
     Missing texture/object values are UCHAR_MAX.*/
  unsigned char read_parts[SNAKE_ALL + 1];
  /* As above but incorporating any necessary fallback substitutions. */
  unsigned char write_parts[SNAKE_ALL + 1];
} SnakeDefinition;

typedef enum {
  DrawState_Start,
  DrawState_Major,
  DrawState_Minor
} DrawState;

/* ---------------- Private functions --------------- */

static size_t get_snake_read_tile(Snakes const *const snakes_data, size_t const snake,
  unsigned int const part)
{
  assert(snakes_data != NULL);
  assert(snake >= 0);
  assert(snake < snakes_data->count);
  assert((part & ~SNAKE_ALL) == 0);

  SnakeDefinition const *const defs = snakes_data->data_anchor;
  size_t const tile = defs[snake].read_parts[part];

  DEBUG("%zu is read part %u (N:%u: E:%u S:%u W:%u I:%u) in snake definition %zu",
        tile, part, TEST_BITS(part, SNAKE_NORTH), TEST_BITS(part, SNAKE_EAST),
        TEST_BITS(part, SNAKE_SOUTH), TEST_BITS(part, SNAKE_WEST),
        TEST_BITS(part, SNAKE_INSIDE), snake);

  return tile;
}

static int get_snake_write_tile(Snakes const *const snakes_data, size_t const snake,
  unsigned int const part)
{
  assert(snakes_data != NULL);
  assert(snake >= 0);
  assert(snake < snakes_data->count);
  assert((part & ~SNAKE_ALL) == 0);

  SnakeDefinition const *const defs = snakes_data->data_anchor;
  int const tile = defs[snake].write_parts[part];

  DEBUG("%d is write part %u (N:%u: E:%u S:%u W:%u I:%u) in snake definition %zu",
        tile, part, TEST_BITS(part, SNAKE_NORTH), TEST_BITS(part, SNAKE_EAST),
        TEST_BITS(part, SNAKE_SOUTH), TEST_BITS(part, SNAKE_WEST),
        TEST_BITS(part, SNAKE_INSIDE), snake);

  return tile == UCHAR_MAX ? -1 : tile;
}

static void fill_missing_snake_parts(SnakeDefinition *const snake)
{
  /* Copy snake parts from inside to outside (or vice-versa)
     to fill gaps in the table if the alternate tile exists. */
  assert(snake != NULL);
  for (unsigned int outside_part = 0; outside_part < SNAKE_INSIDE; outside_part++)
  {
    unsigned int const inside_part = outside_part | SNAKE_INSIDE;

    if (snake->read_parts[outside_part] == UCHAR_MAX &&
        snake->read_parts[inside_part] != UCHAR_MAX)
    {
      DEBUG("Copying missing outside part %u from inside %u", outside_part, inside_part);
      snake->read_parts[outside_part] = snake->read_parts[inside_part];
    }
    else if (snake->read_parts[inside_part] == UCHAR_MAX &&
             snake->read_parts[outside_part] != UCHAR_MAX)
    {
      DEBUG("Copying missing inside part %u from outside %u", inside_part, outside_part);
      snake->read_parts[inside_part] = snake->read_parts[outside_part];
    }
  }

  for (unsigned int part = 0; part <= SNAKE_ALL; part++) {
    unsigned char tile = snake->read_parts[part];
    if (tile == UCHAR_MAX) {
      DEBUG("Missing part %u (N:%u: E:%u S:%u W:%u I:%u",
            part, TEST_BITS(part, SNAKE_NORTH), TEST_BITS(part, SNAKE_EAST),
            TEST_BITS(part, SNAKE_SOUTH), TEST_BITS(part, SNAKE_WEST),
            TEST_BITS(part, SNAKE_INSIDE));
      unsigned int const basic_part = part & ~SNAKE_INSIDE; /* mask out part's inside/outside
                                                               status for convenience */
      unsigned int const inside_or_outside = part & SNAKE_INSIDE;

      switch (basic_part) {
        case SNAKE_NORTH | SNAKE_EAST | SNAKE_SOUTH:
        case SNAKE_NORTH | SNAKE_EAST | SNAKE_WEST:
        case SNAKE_NORTH | SNAKE_SOUTH | SNAKE_WEST:
        case SNAKE_EAST | SNAKE_SOUTH | SNAKE_WEST:
          DEBUG("No three-way junction so use a four-way junction");
          tile = snake->read_parts[SNAKE_NORTH | SNAKE_EAST | SNAKE_SOUTH | SNAKE_WEST | inside_or_outside];
          break;

        case SNAKE_NORTH | SNAKE_EAST:
          tile = snake->read_parts[basic_part | SNAKE_SOUTH | inside_or_outside];
          if (tile == UCHAR_MAX) {
            tile = snake->read_parts[basic_part | SNAKE_WEST | inside_or_outside];
          }
          if (tile != UCHAR_MAX) {
            DEBUG("No corner so use a three-way junction");
          } else {
            DEBUG("No corner so use a four-way junction");
            tile = snake->read_parts[basic_part | SNAKE_SOUTH | SNAKE_WEST | inside_or_outside];
          }
          break;

        case SNAKE_NORTH | SNAKE_WEST:
          tile = snake->read_parts[basic_part | SNAKE_SOUTH | inside_or_outside];
          if (tile == UCHAR_MAX) {
            tile = snake->read_parts[basic_part | SNAKE_EAST | inside_or_outside];
          }
          if (tile != UCHAR_MAX) {
            DEBUG("No corner so use a three-way junction");
          } else {
            DEBUG("No corner so use a four-way junction");
            tile = snake->read_parts[basic_part | SNAKE_SOUTH | SNAKE_EAST | inside_or_outside];
          }
          break;

        case SNAKE_SOUTH | SNAKE_WEST:
          tile = snake->read_parts[basic_part | SNAKE_NORTH | inside_or_outside];
          if (tile == UCHAR_MAX) {
            tile = snake->read_parts[basic_part | SNAKE_EAST | inside_or_outside];
          }
          if (tile != UCHAR_MAX) {
            DEBUG("No corner so use a three-way junction");
          } else {
            DEBUG("No corner so use a four-way junction");
            tile = snake->read_parts[basic_part | SNAKE_NORTH | SNAKE_EAST | inside_or_outside];
          }
          break;

        case SNAKE_EAST | SNAKE_SOUTH:
          tile = snake->read_parts[basic_part | SNAKE_NORTH | inside_or_outside];
          if (tile == UCHAR_MAX) {
            tile = snake->read_parts[basic_part | SNAKE_WEST | inside_or_outside];
          }
          if (tile != UCHAR_MAX) {
            DEBUG("No corner so use a three-way junction");
          } else {
            DEBUG("No corner so use a four-way junction");
            tile = snake->read_parts[basic_part | SNAKE_NORTH | SNAKE_WEST | inside_or_outside];
          }
          break;

        case 0:
          tile = snake->read_parts[SNAKE_NORTH | SNAKE_SOUTH | inside_or_outside];
          if (tile != UCHAR_MAX) {
            DEBUG("No blob so use a N/S straight");
          } else {
            DEBUG("No blob so use an E/W straight");
            tile = snake->read_parts[SNAKE_EAST | SNAKE_WEST | inside_or_outside];
          }
          break;

        case SNAKE_NORTH:
        case SNAKE_SOUTH:
          DEBUG("No vertical cap piece so use a N/S straight");
          tile = snake->read_parts[SNAKE_NORTH | SNAKE_SOUTH | inside_or_outside];
          break;

        case SNAKE_EAST:
        case SNAKE_WEST:
          DEBUG("No horizontal cap piece so use an E/W straight");
          tile = snake->read_parts[SNAKE_EAST | SNAKE_WEST | inside_or_outside];
          break;
      }
      DEBUG("Substitute tile is %u", tile);
    }
    snake->write_parts[part] = tile;
  }
}

static bool add_snake(Snakes *const snakes_data, SnakeDefinition *const snake)
{
  assert(snakes_data != NULL);
  assert(snake != NULL);
  assert(*snake->name != '\0');

  /*
    Extend (or create) flex block holding snake definitions
  */
  bool success = true;

  if (snakes_data->count == 0) {
    success = flex_alloc(&snakes_data->data_anchor,
              sizeof(SnakeDefinition) * InitSnakesArraySize);
  } else {
    int const size = flex_size(&snakes_data->data_anchor);
    if (snakes_data->count * sizeof(SnakeDefinition) >= (unsigned)size) {
      success = flex_extend(&snakes_data->data_anchor,
                            size * SnakesArrayGrowthFactor * (int)sizeof(SnakeDefinition));
    }
  }

  if (success) {
    /* Add snake to array */
    DEBUG("Adding snake '%s' to array at index %zu", snake->name, snakes_data->count);
    SnakeDefinition *const defs = snakes_data->data_anchor;
    defs[snakes_data->count++] = *snake;
  }

  return success;
}

//static bool part_is_bi_sided(const SnakeContext *const ctx,
//  unsigned int const snake_part)
//{
//  /* Examines a snake definition and returns true if both inside and outside
//     versions of the given part are the same tile */
//  assert(ctx != NULL);
//
//  unsigned int const outside_part = snake_part & ~SNAKE_INSIDE;
//  bool const dup =
//    get_snake_read_tile(ctx->snakes_data, ctx->snake, outside_part) ==
//    get_snake_read_tile(ctx->snakes_data, ctx->snake, SNAKE_INSIDE | outside_part);
//
//  DEBUG("Snake part %u with opp. sidedness is%s same tile",
//        snake_part, dup ? "" : " not");
//
//  return dup;
//}

static bool north_is_inside(unsigned int const part)
{
  /* Suitably modify the given snake part spec if it is a special part
  with north connectivity at odds with the setting of its SNAKE_INSIDE
  bit */
  assert((part & ~SNAKE_ALL) == 0);

  if ((part & ~SNAKE_INSIDE) == (SNAKE_NORTH | SNAKE_EAST)) {
    DEBUG("North exit from part %u swaps over inside and outside", part);
    return !TEST_BITS(part, SNAKE_INSIDE);
  } else {
    DEBUG("North exit from part %u keeps same inside/outside", part);
    return TEST_BITS(part, SNAKE_INSIDE);
  }
}

static bool south_is_inside(unsigned int const part)
{
  /* Suitably modify the given snake part spec if it is a special part
  with south connectivity at odds with the setting of its SNAKE_INSIDE
  bit */
  assert((part & ~SNAKE_ALL) == 0);

  if ((part & ~SNAKE_INSIDE) == (SNAKE_SOUTH | SNAKE_WEST)) {
    DEBUG("South exit from part %u swaps over inside and outside", part);
    return !TEST_BITS(part, SNAKE_INSIDE);
  } else {
    DEBUG("South exit from part %u keeps same inside/outside", part);
    return TEST_BITS(part, SNAKE_INSIDE);
  }
}

static bool add_to_connectivity(const SnakeContext *const ctx,
  size_t const tile, unsigned int const edge, bool const inside)
{
  /* Check whether a given tile number matches an edge and sidedness
     specification. Only one of the direction bits may be set in 'edge'.
     Copes with parts that have different sidedness depending on edge. */
  bool found = false, match = false;

  assert(ctx != NULL);
  assert(tile >= 0);
  assert(edge == SNAKE_NORTH || edge == SNAKE_EAST || edge == SNAKE_SOUTH || edge == SNAKE_WEST);

  DEBUG("Looking for tile %zu in snake %zu with connectivity (%s, %s)...",
        tile, ctx->snake,
        TEST_BITS(edge, SNAKE_NORTH) ? "north" :
        TEST_BITS(edge, SNAKE_EAST) ? "east" :
        TEST_BITS(edge, SNAKE_SOUTH) ? "south" :
        TEST_BITS(edge, SNAKE_WEST) ? "west" : "",
        inside ? "inside" : "outside");

  for (unsigned int part = 0; part <= SNAKE_ALL; part++) {
    /* Check whether tile number and edge matches specification
       (excluding sidedness) */
    if (part == SNAKE_INSIDE) {
      found = false; /* treat two halves of definition separately
                        (ambiguity over sidedness is very common) */
    }

    if (get_snake_read_tile(ctx->snakes_data, ctx->snake, part) != tile)
    {
      continue; /* this snake part is not the tile in question */
    }

    if (found) {
      /* Not a 1:1 relationship between tiles and snake parts */
      DEBUG("...tile is ambiguous - giving up");
      return false;
    }
    found = true;

    if (!TEST_BITS(part, edge)) {
      continue; /* this snake part hasn't got the edge we're interested in */
    }

    /* Check whether sidedness matches also (allowing for switchovers) */
    bool part_inside;
    switch (edge) {
    case SNAKE_NORTH:
      part_inside = north_is_inside(part);
      break;
    case SNAKE_SOUTH:
      part_inside = south_is_inside(part);
      break;
    default:
      part_inside = TEST_BITS(part, SNAKE_INSIDE);
      break;
    }

    if (part_inside != inside) {
      continue; /* sidedness does not match */
    }

    DEBUG("Tile found as part %u (N:%u: E:%u S:%u W:%u I:%u)", part,
          TEST_BITS(part, SNAKE_NORTH), TEST_BITS(part, SNAKE_EAST),
          TEST_BITS(part, SNAKE_SOUTH), TEST_BITS(part, SNAKE_WEST),
          TEST_BITS(part, SNAKE_INSIDE));
    match = true;

  } /* next part */

  if (!match) {
    DEBUG(" ...tile not found");
  } else {
    DEBUG("...tile matches part spec.");
 }

  return match;
}

static int count_exits(unsigned int const part)
{
  assert((part & ~SNAKE_ALL) == 0);
  return (TEST_BITS(part, SNAKE_NORTH) ? 1 : 0) +
         (TEST_BITS(part, SNAKE_EAST) ? 1 : 0) +
         (TEST_BITS(part, SNAKE_SOUTH) ? 1 : 0) +
         (TEST_BITS(part, SNAKE_WEST) ? 1 : 0);
}

static bool add_north_exit(SnakeContext *const ctx, unsigned int const part)
{
  assert(ctx);
  assert((part & ~SNAKE_ALL) == 0);

  if (TEST_BITS(part, SNAKE_NORTH))
    return false;

  MapPoint const map_pos = {ctx->map_pos.x, ctx->map_pos.y + 1};
  size_t const north_tile = ctx->read(map_pos, ctx);
  if (north_tile == UINT8_MAX)
    return false;

  bool const n_inside = north_is_inside(part | SNAKE_NORTH);
  return add_to_connectivity(ctx, north_tile, SNAKE_SOUTH, n_inside);
}

static bool add_east_exit(SnakeContext *const ctx, unsigned int const part)
{
  assert(ctx);
  assert((part & ~SNAKE_ALL) == 0);

  if (TEST_BITS(part, SNAKE_EAST))
    return false;

  MapPoint const map_pos = {ctx->map_pos.x + 1, ctx->map_pos.y};
  size_t const east_tile = ctx->read(map_pos, ctx);
  if (east_tile == UINT8_MAX)
    return false;

  bool e_inside = TEST_BITS(part, SNAKE_INSIDE);
  if ((part & ~SNAKE_INSIDE) == SNAKE_NORTH)
    e_inside = !e_inside;

  return add_to_connectivity(ctx, east_tile, SNAKE_WEST, e_inside);
}

static bool add_south_exit(SnakeContext *const ctx, unsigned int const part)
{
  assert(ctx);
  assert((part & ~SNAKE_ALL) == 0);

  if (TEST_BITS(part, SNAKE_SOUTH))
    return false;

  MapPoint const map_pos = {ctx->map_pos.x, ctx->map_pos.y - 1};
  size_t const south_tile = ctx->read(map_pos, ctx);
  if (south_tile == UINT8_MAX)
    return false;

  bool const s_inside = south_is_inside(part | SNAKE_SOUTH);
  return add_to_connectivity(ctx, south_tile, SNAKE_NORTH, s_inside);
}

static bool add_west_exit(SnakeContext *const ctx, unsigned int const part)
{
  assert(ctx);
  assert((part & ~SNAKE_ALL) == 0);

  if (TEST_BITS(part, SNAKE_WEST))
    return false;

  MapPoint const map_pos = {ctx->map_pos.x - 1, ctx->map_pos.y};
  size_t const west_tile = ctx->read(map_pos, ctx);
  if (west_tile == UINT8_MAX)
    return false;

  bool w_inside = TEST_BITS(part, SNAKE_INSIDE);
  if ((part & ~SNAKE_INSIDE) == SNAKE_SOUTH)
    w_inside = !w_inside;

  return add_to_connectivity(ctx, west_tile, SNAKE_EAST, w_inside);
}

static int get_max_exits(const SnakeContext *const ctx)
{
  assert(ctx);
  if (get_snake_read_tile(ctx->snakes_data, ctx->snake, SNAKE_ALL) == UCHAR_MAX)
    return 2;

  return 4;
}

static unsigned int amend_part(SnakeContext *const ctx, unsigned int part)
{
  DEBUG("Initial snake part spec. %u (N:%u: E:%u S:%u W:%u I:%u)", part,
        TEST_BITS(part, SNAKE_NORTH), TEST_BITS(part, SNAKE_EAST),
        TEST_BITS(part, SNAKE_SOUTH), TEST_BITS(part, SNAKE_WEST),
        TEST_BITS(part, SNAKE_INSIDE));

  /* Calculate number of exits (initial connectivity) */
  int exit_count = count_exits(part);
  int const max_exit_count = get_max_exits(ctx);
  DEBUG("Initial number of exits is %d (limit %d)", exit_count, max_exit_count);

  if (add_north_exit(ctx, part)) {
    DEBUG("Adding north exit");
    SET_BITS(part, SNAKE_NORTH);

    if (++exit_count >= max_exit_count) {
      DEBUG("Reached exit quota");
      return part;
    }
  }

  if (add_east_exit(ctx, part)) {
    DEBUG("Adding east exit");
    SET_BITS(part, SNAKE_EAST);

    if ((part & ~SNAKE_INSIDE) == (SNAKE_NORTH | SNAKE_EAST)) {
      part ^= SNAKE_INSIDE; /* toggle sideness bit */
      DEBUG("Toggled sidedness bit for NE corner");
    }

    if (++exit_count >= max_exit_count) {
      DEBUG("Reached exit quota");
      return part;
    }

    /* Recheck northern neighbour (with new sidedness) */
    if (add_north_exit(ctx, part)) {
      DEBUG("Adding north exit");
      SET_BITS(part, SNAKE_NORTH);

      if (++exit_count >= max_exit_count) {
        DEBUG("Reached exit quota");
        return part;
      }
    }
  }

  /* We need both south and west neighbours because we may need to refer
     back to the former if we acquire connectivity that changes the sidedness of
     our southern edge. */

  if (add_south_exit(ctx, part)) {
    DEBUG("Adding south exit");
    SET_BITS(part, SNAKE_SOUTH);
    if (++exit_count >= max_exit_count)  {
      DEBUG("Reached exit quota");
      return part;
    }
  }

  if (add_west_exit(ctx, part)) {
    DEBUG("Adding west exit");
    SET_BITS(part, SNAKE_WEST);

    if ((part & ~SNAKE_INSIDE) == (SNAKE_SOUTH | SNAKE_WEST)) {
      part ^= SNAKE_INSIDE; /* toggle sideness bit */
      DEBUG("Toggled sidedness bit for SW corner");
    }

    if (++exit_count >= max_exit_count) {
      DEBUG("Reached exit quota");
      return part;
    }

    /* Recheck southern neighbour (with new sidedness) */
    if (add_south_exit(ctx, part)) {
      DEBUG("Adding south exit");
      SET_BITS(part, SNAKE_SOUTH);
      if (++exit_count >= max_exit_count) {
        DEBUG("Reached exit quota");
        return part;
      }
    }
  }

  return part;
}

static size_t plot_tile(SnakeContext *const ctx, unsigned int const part_spec)
{
  /* Look at the surrounding tiles, and add to the connectivity of part_spec
     as appropriate. We are prepared to use a snake tile of different
     sidedness where this is a NW or SE bend (conceptually these have dual
     sidedness) */
  assert(ctx);
  assert(ctx->snakes_data);
  assert((part_spec & ~SNAKE_ALL) == 0);

  DEBUG("Tile from snake %zu requested at %" PRIMapCoord ",% " PRIMapCoord,
        ctx->snake, ctx->map_pos.x, ctx->map_pos.y);

  unsigned int part_to_plot = amend_part(ctx, part_spec);

  if (part_to_plot != part_spec) {
    DEBUG("Amended snake part spec. %u (N:%u: E:%u S:%u W:%u I:%u)",
          part_to_plot, TEST_BITS(part_to_plot, SNAKE_NORTH),
          TEST_BITS(part_to_plot, SNAKE_EAST),
          TEST_BITS(part_to_plot, SNAKE_SOUTH),
          TEST_BITS(part_to_plot, SNAKE_WEST),
          TEST_BITS(part_to_plot, SNAKE_INSIDE));
  } else {
    DEBUG("Snake part spec. unchanged");
  }

  /* Find a snake piece that matches the required connectivity */
  int tile_num = get_snake_write_tile(ctx->snakes_data, ctx->snake, part_to_plot);
  if (tile_num == -1) {
    /* No tile (no suitable junction defined?) - revert to plain route */
    part_to_plot = part_spec;
    tile_num = get_snake_write_tile(ctx->snakes_data, ctx->snake, part_to_plot);
  }
  if (tile_num != -1 && ctx->write) {
    DEBUG("Plotting snake tile %d", tile_num);
    ctx->write(ctx->map_pos, (size_t)tile_num, ctx);
  }
  return (size_t)tile_num;
}

static void steep_line_to_south(SnakeContext *const ctx, MapPoint end,
  MapPoint const d)
{
  /* Steep (y major)
  For lines with negative x gradients we need to swap between 'inside' and
  'outside' part sets at SW / NE bends. */
  DEBUG("Line is y major");
  assert(ctx != NULL);
  assert(ctx->map_pos.y >= end.y);

  /* MapPoint B is start point */
  DEBUG("Direction of line is south");
  if (ctx->major_direct == DIR_EAST || ctx->major_direct == DIR_NORTH) {
    DEBUG("Last line was easterly - will auto swap sides");
    ctx->default_piece ^= SNAKE_INSIDE;
  }
  ctx->major_direct = DIR_SOUTH;

  DrawState state = DrawState_Start;
  MapCoord p = d.y; /* decision parameter */
  int x_dir = (end.x >= ctx->map_pos.x ? 1 : -1);

  unsigned int snake_piece;

  do {
    snake_piece = ctx->default_piece |
      (ctx->map_pos.y > end.y ? SNAKE_SOUTH : 0);

    switch (state) {
      case DrawState_Major: /* coming from north */
        SET_BITS(snake_piece, SNAKE_NORTH);
        break;

      case DrawState_Minor: /* coming from east or west? */
        if (x_dir > 0) {
          /* Correct SW bend is not in expected set. */
          snake_piece ^= SNAKE_INSIDE;
          SET_BITS(snake_piece, SNAKE_WEST);
        } else {
          SET_BITS(snake_piece, SNAKE_EAST);
        }
        state = DrawState_Major;
        break;

      case DrawState_Start:
        ctx->default_piece &= SNAKE_INSIDE;
        /* Correct SW bend is not in expected set. */
        if ((snake_piece & ~SNAKE_INSIDE) == (SNAKE_SOUTH | SNAKE_WEST)) {
          snake_piece ^= SNAKE_INSIDE;
        }
        state = DrawState_Major;
        break;

      default:
        assert(false);
    }
    plot_tile(ctx, snake_piece);

    p -= 2 * d.x;
    ctx->map_pos.y --;

    if (p < 0) {
      /* overlap adjacent column (if any) by one tile, and turn corner */
      if (ctx->map_pos.y >= end.y) {
        DEBUG("Overlapping adjacent column %" PRIMapCoord, ctx->map_pos.y);
        snake_piece = ctx->default_piece | SNAKE_NORTH;

        /* heading east or west? */
        if (x_dir > 0) {
          /* Correct NE bend is not in expected set. */
          snake_piece ^= SNAKE_INSIDE;
          SET_BITS(snake_piece, SNAKE_EAST);
        } else {
          SET_BITS(snake_piece, SNAKE_WEST);
        }

        plot_tile(ctx, snake_piece);
        state = DrawState_Minor;
      }

      /* advance in x direction */
      ctx->map_pos.x += x_dir;
      p += 2 * d.y;
    }
  } while (ctx->map_pos.y >= end.y);

  unsigned int inside = ctx->default_piece & SNAKE_INSIDE;
  ctx->default_piece = (snake_piece & ~SNAKE_INSIDE) | inside;
}


static void steep_line_to_north(SnakeContext *const ctx, MapPoint end,
  MapPoint const d)
{
  /* Steep (y major)
  For lines with negative x gradients we need to swap between 'inside' and
  'outside' part sets at SW / NE bends. */
  DEBUG("Line is y major");
  assert(ctx != NULL);
  assert(ctx->map_pos.y <= end.y);

  /* MapPoint A is start point */
  DEBUG("Direction of line is north");
  if (ctx->major_direct == DIR_WEST || ctx->major_direct == DIR_SOUTH) {
    DEBUG("Last line was westerly - will auto swap sides");
    ctx->default_piece ^= SNAKE_INSIDE;
  }
  ctx->major_direct = DIR_NORTH;

  DrawState state = DrawState_Start;
  MapCoord p = d.y; /* decision parameter */
  int x_dir = (end.x >= ctx->map_pos.x ? 1 : -1);

  unsigned int snake_piece;

  do {
    snake_piece = ctx->default_piece |
      (ctx->map_pos.y < end.y ? SNAKE_NORTH : 0);

    switch (state) {
      case DrawState_Major: /* coming from south */
        SET_BITS(snake_piece, SNAKE_SOUTH);
        break;

      case DrawState_Minor: /* coming from east or west? */
        if (x_dir > 0) {
          SET_BITS(snake_piece, SNAKE_WEST);
        } else {
          SET_BITS(snake_piece, SNAKE_EAST);
          /* Correct NE bend is not in expected set. This also caters for
             the case where we can't straighten out towards North (because at
             end of line) */
          snake_piece ^= SNAKE_INSIDE;
        }
        state = DrawState_Major;
        break;

      case DrawState_Start:
        ctx->default_piece &= SNAKE_INSIDE;
        /* Correct NE bend is not in expected set. */
        if ((snake_piece & ~SNAKE_INSIDE) == (SNAKE_NORTH | SNAKE_EAST)) {
          snake_piece ^= SNAKE_INSIDE;
        }
        state = DrawState_Major;
        break;

      default:
        assert(false);
    }
    plot_tile(ctx, snake_piece);

    p -= 2 * d.x;
    ctx->map_pos.y ++;

    if (p < 0) {
      /* overlap adjacent column (if any) by one tile, and turn corner */
      if (ctx->map_pos.y <= end.y) {
        DEBUG("Overlapping adjacent column %" PRIMapCoord, ctx->map_pos.y);
        snake_piece = ctx->default_piece | SNAKE_SOUTH;

        /* heading east or west? */
        if (x_dir > 0) {
          SET_BITS(snake_piece, SNAKE_EAST);
        } else {
          SET_BITS(snake_piece, SNAKE_WEST);
          /* Correct SW bend is not in expected set */
          snake_piece ^= SNAKE_INSIDE;
        }

        plot_tile(ctx, snake_piece);
        state = DrawState_Minor;
      }

      /* advance in x direction */
      ctx->map_pos.x += x_dir;
      p += 2 * d.y;
    }
  } while (ctx->map_pos.y <= end.y);

  unsigned int inside = ctx->default_piece & SNAKE_INSIDE;
  ctx->default_piece = (snake_piece & ~SNAKE_INSIDE) | inside;
}

static void steep_line(SnakeContext *const ctx, MapPoint end,
  MapPoint const d)
{
  /* Steep (y major)
  For lines with negative x gradients we need to swap between 'inside' and
  'outside' part sets at SW / NE bends. */
  DEBUG("Line is y major");
  assert(ctx != NULL);

  if (ctx->map_pos.y > end.y) {
    steep_line_to_south(ctx, end, d);
  } else {
    steep_line_to_north(ctx, end, d);
  }
}

static void shallow_to_west(SnakeContext *const ctx, MapPoint end,
  MapPoint const d)
{
  /* MapPoint B is start point */
  DEBUG("Direction of line is west");
  assert(ctx);
  assert(ctx->map_pos.x >= end.x);

  if (ctx->major_direct == DIR_NORTH || ctx->major_direct == DIR_EAST) {
    DEBUG("Last line was northerly - will auto swap sides");
    ctx->default_piece ^= SNAKE_INSIDE;
  }
  ctx->major_direct = DIR_WEST;

  DrawState state = DrawState_Start;
  MapCoord p = d.x; /* decision parameter */
  int y_dir = (end.y >= ctx->map_pos.y ? 1 : -1);

  unsigned int snake_piece;

  do {
    snake_piece = ctx->default_piece |
      (ctx->map_pos.x > end.x ? SNAKE_WEST : 0);

    switch (state) {
      case DrawState_Major: /* coming from east */
        SET_BITS(snake_piece, SNAKE_EAST);
        break;

      case DrawState_Minor: /* coming from south or north? */
        SET_BITS(snake_piece, y_dir > 0 ? SNAKE_SOUTH : SNAKE_NORTH);

        /* if cannot straighten out towards West (because at end of line)
        then we are left with a tile of the opposite side: */
        if (y_dir > 0 && ctx->map_pos.x <= end.x) {
          DEBUG("No room to straighten out towards West");
          snake_piece ^= SNAKE_INSIDE;
        }
        state = DrawState_Major;
        break;

      case DrawState_Start:
        ctx->default_piece &= SNAKE_INSIDE;
        state = DrawState_Major;
        break;

      default:
        assert(false);
    }

    /* Mechanism for joining lines of differing sidedness */
    plot_tile(ctx, snake_piece);

    p -= 2 * d.y;
    ctx->map_pos.x --;

    if (p < 0) {
      /* overlap adjacent row (if any) by one tile, and turn corner */
      if (ctx->map_pos.x >= end.x) {
        DEBUG("Overlapping adjacent row %" PRIMapCoord, ctx->map_pos.x);
        snake_piece = ctx->default_piece | SNAKE_EAST;

        /* heading south or north? */
        SET_BITS(snake_piece, y_dir > 0 ? SNAKE_NORTH : SNAKE_SOUTH);

        plot_tile(ctx, snake_piece);
        state = DrawState_Minor;
      }

      /* advance in y direction */
      ctx->map_pos.y += y_dir;
      p += 2 * d.x;
    }
  } while (ctx->map_pos.x >= end.x);

  unsigned int inside = ctx->default_piece & SNAKE_INSIDE;
  ctx->default_piece = (snake_piece & ~SNAKE_INSIDE) | inside;
}

static void shallow_to_east(SnakeContext *const ctx, MapPoint end,
  MapPoint const d)
{
  /* MapPoint A is start point */
  DEBUG("Direction of line is east");
  assert(ctx);
  assert(ctx->map_pos.x <= end.x);

  if (ctx->major_direct == DIR_SOUTH || ctx->major_direct == DIR_WEST) {
    DEBUG("Last line was southerly - will auto swap sides");
    ctx->default_piece ^= SNAKE_INSIDE;
  }
  ctx->major_direct = DIR_EAST;

  DrawState state = DrawState_Start;
  MapCoord p = d.x; /* decision parameter */
  int y_dir = (end.y >= ctx->map_pos.y ? 1 : -1);

  unsigned int snake_piece = ctx->default_piece;

  do {
    snake_piece = ctx->default_piece |
      (ctx->map_pos.x < end.x ? SNAKE_EAST : 0);

    switch (state) {
      case DrawState_Major: /* coming from west */
        SET_BITS(snake_piece, SNAKE_WEST);
        break;

      case DrawState_Minor: /* coming from south or north? */
        SET_BITS(snake_piece, y_dir > 0 ? SNAKE_SOUTH : SNAKE_NORTH);

        /* if cannot straighten out towards East (because at end of line)
        then we are left with a tile of the opposite side: */
        if (y_dir <= 0 && ctx->map_pos.x >= end.x) {
          DEBUG("No room to straighten out towards east");
          snake_piece ^= SNAKE_INSIDE;
        }
        state = DrawState_Major;
        break;

      case DrawState_Start:
        ctx->default_piece &= SNAKE_INSIDE;
        state = DrawState_Major;
        break;

      default:
        assert(false);
    }

    /* Mechanism for joining lines of differing sidedness */
    plot_tile(ctx, snake_piece);

    p -= 2 * d.y;
    ctx->map_pos.x ++;

    if (p < 0) {
      /* overlap adjacent row (if any) by one tile, and turn corner */
      if (ctx->map_pos.x <= end.x) {
        DEBUG("Overlapping adjacent row %" PRIMapCoord, ctx->map_pos.x);
        snake_piece = ctx->default_piece | SNAKE_WEST;

        /* heading south or north? */
        SET_BITS(snake_piece, y_dir > 0 ? SNAKE_NORTH : SNAKE_SOUTH);

        plot_tile(ctx, snake_piece);
        state = DrawState_Minor;
      }

      /* advance in y direction */
      ctx->map_pos.y += y_dir;
      p += 2 * d.x;
    }
  } while (ctx->map_pos.x <= end.x);

  unsigned int inside = ctx->default_piece & SNAKE_INSIDE;
  ctx->default_piece = (snake_piece & ~SNAKE_INSIDE) | inside;
}

static void shallow_line(SnakeContext *const ctx, MapPoint end,
  MapPoint const d)
{
  /* Shallow (x major)
  Although we use SW / NE bends for lines with negative y gradients (E-S-E
  octant) there is no need to swap between 'inside' and 'outside' sets
  because the double-swap from 'inside' to 'outside' and back (or vice-
  versa) cancels out. */

  DEBUG("Line is x major");
  assert(ctx != NULL);

  if (ctx->map_pos.x > end.x) {
    shallow_to_west(ctx, end, d);
  } else {
    shallow_to_east(ctx, end, d);
  }
}

/* ---------------- Public functions ---------------- */

size_t Snakes_get_count(const Snakes *const snakes_data)
{
  assert(snakes_data != NULL);
  assert(snakes_data->count >= 0);
  DEBUG_VERBOSEF("No. of snakes is %zu\n", snakes_data->count);
  return snakes_data->count;
}

void Snakes_get_name(const Snakes *const snakes_data, size_t const snake,
  char *const snake_name, size_t const n)
{
  assert(snakes_data != NULL);
  assert(snake >= 0);
  assert(snake < snakes_data->count);
  assert(snake_name != NULL);

  if (n > 0) {
    size_t len = 0;
    char *out = snake_name;
    SnakeDefinition const *const defs = snakes_data->data_anchor;
    char const *const in = defs[snake].name;
    for (char c = in[len]; c != '\0'; c = in[len]) {
      if (len++ < n) {
        assert(out < snake_name + n);
        *(out++) = c;
      }
    }
    assert(out < snake_name + n);
    *out = '\0';
  }
}

size_t Snakes_begin_line(SnakeContext *const ctx,
  Snakes *const snakes_data, MapPoint const map_pos, size_t const snake,
  bool const inside, SnakesReadFunction *const read,
  SnakesWriteFunction *const write)
{
  assert(ctx != NULL);

  *ctx = (SnakeContext){
    .snakes_data = snakes_data,
    .map_pos = map_pos,
    .snake = snake,
    .default_piece = (inside ? SNAKE_INSIDE : 0),
    .major_direct = DIR_START,
    .read = read,
    .write = write,
  };

  DEBUG("Starting %sside snake %zu at %" PRIMapCoord ",%" PRIMapCoord,
        inside ? "in" : "out", snake, map_pos.x, map_pos.y);

  return plot_tile(ctx, ctx->default_piece);
}

void Snakes_plot_line(SnakeContext *const ctx, MapPoint const end)
{
  assert(ctx != NULL);
  DEBUG("Continuing snake %zu from %" PRIMapCoord ",%" PRIMapCoord
        " to %" PRIMapCoord ",%" PRIMapCoord, ctx->snake,
        ctx->map_pos.x, ctx->map_pos.y, end.x, end.y);

  MapPoint const d = MapPoint_abs_diff(end, ctx->map_pos);

  if (d.y > d.x) {
    steep_line(ctx, end, d);
  } else {
    shallow_line(ctx, end, d);
  }

  DEBUG("Default snake part spec. %u (N:%u: E:%u S:%u W:%u I:%u)",
        ctx->default_piece, TEST_BITS(ctx->default_piece, SNAKE_NORTH),
        TEST_BITS(ctx->default_piece, SNAKE_EAST),
        TEST_BITS(ctx->default_piece, SNAKE_SOUTH),
        TEST_BITS(ctx->default_piece, SNAKE_WEST),
        TEST_BITS(ctx->default_piece, SNAKE_INSIDE));

  ctx->map_pos = end;
}

void Snakes_init(Snakes *const snakes_data)
{
  assert(snakes_data);
  *snakes_data = (Snakes){.count = 0};
}

SFError Snakes_load(FILE *const file, Snakes *const snakes_data,
  size_t const nobj, char *const err_buf)
{
  assert(file != NULL);
  assert(!ferror(file));
  assert(snakes_data != NULL);
  assert(err_buf);

  char read_line[LineBufferSize];
  int line = 0;
  bool in_snake = false;
  SnakeDefinition snake = {"", {0}};

  Snakes_free(snakes_data);
  Snakes_init(snakes_data);
  *err_buf = '\0';

  while (read_line_comm(read_line, sizeof(read_line), file, &line) != NULL)
  {
    if (strncmp(read_line, UX_STARTSNAKEMARK, sizeof(UX_STARTSNAKEMARK) - 1) == 0) {
      if (in_snake) {
        /* syntax error - already reading snake */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }

      /* Set all snake pieces to UCHAR_MAX (no tile) */
      for (unsigned int part = 0; part <= SNAKE_ALL; part++) {
        snake.read_parts[part] = UCHAR_MAX;
      }

      /*
        Extract snake name
      */
      {
        char *snake_name = strtok(read_line, "\"'"); /* start of line */
        if (snake_name != NULL)
          snake_name = strtok(NULL, "\"'"); /* string between quotes */

        if (snake_name == NULL || strtok(NULL, "\"'") == NULL) {
          /* Report syntax error and line number */
          sprintf(err_buf, "%d", line);
          return SFERROR(Mistake);
        }

        if (strlen(snake_name) >= sizeof(snake.name)) {
          /* Report name too long and line number */
          sprintf(err_buf, "%d", line);
          return SFERROR(StringTooLong);
        }
        DEBUG("Snake name %zu: %s", snakes_data->count, snake_name);
        strcpy(snake.name, snake_name);
      }

      in_snake = true;
      continue;
    }

    if (strcmp(read_line, ENDSNAKEMARK) == 0) {
      if (!in_snake) {
        /* syntax error - not reading snake */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }

      /* End of snake definition */
      fill_missing_snake_parts(&snake);
      if (!add_snake(snakes_data, &snake)) {
        sprintf(err_buf, "%d", line);
        return SFERROR(NoMem);
      }
      in_snake = false;
      continue;
    }

    if (!in_snake) {
      /* Unknown non-comment text outside snake definition */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }

    /* Now we expect a part definition in the form
       Tile no.: N junction?, E junction?, S junction?, W junction?, Inside? */
    int tile, n, e, s, w, i; /* Can't read directly into chars */
    int const num_inputs = sscanf(read_line, "%d:%d,%d,%d,%d,%d", &tile,
                                  &n, &e, &s, &w, &i);
    if (num_inputs != 6 || (n != 0 && n != 1) || (e != 0 && e != 1) ||
        (s != 0 && s != 1) || (w != 0 && w != 1) || (i != 0 && i != 1)) {
      /* Report syntax error and line number */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }
    if (tile < 0 || (size_t)tile >= nobj) {
      /* Tile number out of range */
      sprintf(err_buf, "%d", line);
      return SFERROR(NumRange);
    }
    unsigned int const part = (n ? SNAKE_NORTH : 0) | (e ? SNAKE_EAST : 0) |
                              (s ? SNAKE_SOUTH : 0) | (w ? SNAKE_WEST : 0) |
                              (i ? SNAKE_INSIDE : 0);
    snake.read_parts[part] = tile;

      DEBUG("From file: %d is part %u (N:%u: E:%u S:%u W:%u I:%u",
            tile, part, TEST_BITS(part, SNAKE_NORTH), TEST_BITS(part, SNAKE_EAST),
            TEST_BITS(part, SNAKE_SOUTH), TEST_BITS(part, SNAKE_WEST),
            TEST_BITS(part, SNAKE_INSIDE));
  } /* endwhile */

  if (in_snake) {
    /* syntax error - no endsnake before EOF */
    strcpy(err_buf, ENDSNAKEMARK);
    return SFERROR(EOF);
  }

  return SFERROR(OK);
}

void Snakes_free(Snakes *const snakes_data)
{
  assert(snakes_data != NULL);
  if (snakes_data->data_anchor != NULL)
  {
    flex_free(&snakes_data->data_anchor);
  }
}

bool Snakes_has_junctions(const Snakes *const snakes_data, size_t const snake)
{
  return get_snake_read_tile(snakes_data, snake, SNAKE_ALL) != UCHAR_MAX;
}

bool Snakes_has_bends(const Snakes *const snakes_data, size_t const snake)
{
  assert(snakes_data != NULL);
  return get_snake_read_tile(snakes_data, snake, SNAKE_NORTH|SNAKE_EAST) != UCHAR_MAX;
}
