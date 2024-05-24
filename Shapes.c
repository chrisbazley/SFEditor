/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Shape rasterisation
 *  Copyright (C) 2019 Christopher Bazley
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
#include "stdlib.h"
#include "stdio.h"
#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <math.h>

#include "Macros.h"
#include "Debug.h"

#include "Shapes.h"
#include "MapCoord.h"

typedef struct
{
  MapCoord x;
  MapCoord end_x;
  MapCoord p;
  MapPoint d;
  MapCoord dir;
} Edge;

static void make_edge(Edge *const edge,
  MapPoint const *const start, MapPoint const *const end)
{
  assert(edge != NULL);
  assert(start != NULL);
  assert(end != NULL);
  assert(start != end);

  edge->x = start->x;
  edge->end_x = end->x;
  edge->d = MapPoint_abs_diff(*end, *start);
  edge->p = HIGHEST(edge->d.x, edge->d.y);
  edge->dir = end->x > start->x ? 1 : -1;
}

typedef enum {
  EdgeSide_Left,
  EdgeSide_Right
} EdgeSide;

static MapCoord advance_edge(Edge *const edge, EdgeSide side)
{
  assert(edge != NULL);
  MapCoord x = edge->x;
  MapCoord p = edge->p;
  MapCoord const dx = edge->d.x;
  assert(dx >= 0);
  MapCoord const dy = edge->d.y;
  assert(dy >= 0);
  MapCoord const dir = edge->dir;
  assert(dir == -1 || dir == 1);
  assert(side == EdgeSide_Left || side == EdgeSide_Right);

  MapCoord this_x = x;

  if (dy >= dx) {
    /* Steep edge: compute x for the next y */
    p -= 2 * dx;
    DEBUGF("Steep p=%" PRIMapCoord " x=%" PRIMapCoord "\n", p, x);
    if (p < 0) {
      p += 2 * dy;
      x += dir;
    }
  } else {
    /* Shallow edge: compute x for the current y */
    MapCoord const end_x = edge->end_x;
    if (dir > 0) {
      for (p -= 2 * dy; p >= 0 && x != end_x; p -= 2 * dy) {
        DEBUGF("Shallow p=%" PRIMapCoord " x=%" PRIMapCoord "\n", p, x);
        x += dir;
      }
      DEBUGF("Shallow p=%" PRIMapCoord " x=%" PRIMapCoord "\n", p, x);

      if (side == EdgeSide_Right) {
        this_x = x;
      }
    } else {
      for (p -= 2 * dy; p > 0 && x != end_x; p -= 2 * dy) {
        DEBUGF("Shallow p=%" PRIMapCoord " x=%" PRIMapCoord "\n", p, x);
        x += dir;
      }
      DEBUGF("Shallow p=%" PRIMapCoord " x=%" PRIMapCoord "\n", p, x);

      if (side == EdgeSide_Left) {
        this_x = x;
      }
    }

    /* x for the next y is one unit further */
    p += 2 * dx;
    if (x != end_x) {
      x += dir;
    }
  }

  DEBUGF("this_x=%" PRIMapCoord " p=%" PRIMapCoord
         " x=%" PRIMapCoord "\n", this_x, p, x);
  edge->x = x;
  edge->p = p;

  return this_x;
}

static void plot_trapezium(ShapesWriteFunction *const write, void *const arg,
    Edge *const left, Edge *const right, MapCoord const bot_y,
    MapCoord const top_y)
{
  assert(left != NULL);
  assert(right != NULL);
  assert(write != NULL);

  DEBUGF("Trapezium with base %" PRIMapCoord ",%" PRIMapCoord
         " and dir %" PRIMapCoord ",%" PRIMapCoord
         " and p %" PRIMapCoord ",%" PRIMapCoord
         " and gradients %" PRIMapCoord "/%" PRIMapCoord
         " %" PRIMapCoord "/%" PRIMapCoord
         " and vertical span %" PRIMapCoord ",%" PRIMapCoord "\n",
         left->x, right->x, left->dir, right->dir, left->p, right->p,
         left->d.y, left->d.x, right->d.y, right->d.x, bot_y, top_y);

  assert(left->x <= right->x);
  assert(bot_y <= top_y);

  MapArea map_area;

  for (MapCoord y = bot_y; y <= top_y; y++) {
    MapCoord const min_x = advance_edge(left, EdgeSide_Left);
    MapCoord const max_x = advance_edge(right, EdgeSide_Right);

    if (y == bot_y || min_x != map_area.min.x || max_x != map_area.max.x) {
      if (y != bot_y) {
        /* Draw the preceding rows */
        map_area.max.y = y - 1;
        write(&map_area, arg);
      }

      /* Start a new rectangle */
      map_area.min.x = min_x;
      map_area.max.x = max_x;
      map_area.min.y = y;
    }
  } /* next y */

  /* Draw the preceding rows */
  map_area.max.y = top_y;
  write(&map_area, arg);
}

void Shapes_tri(ShapesWriteFunction *const write, void *const arg,
  MapPoint const vertex_A, MapPoint const vertex_B, MapPoint const vertex_C)
{
  MapPoint const *left, *right, *mid, *bot;

  DEBUG("Triangle between %" PRIMapCoord ",%" PRIMapCoord ", %"
        PRIMapCoord ",%" PRIMapCoord " and %" PRIMapCoord ",%" PRIMapCoord,
        vertex_A.x, vertex_A.y,
        vertex_B.x, vertex_B.y,
        vertex_C.x, vertex_C.y);

  /* Swap vertices A and B, if B further down */
  if (vertex_B.y < vertex_A.y) {
    bot = &vertex_B;
    right = &vertex_A; /* speculative (could be left) */
  } else {
    bot = &vertex_A;
    right = &vertex_B; /* speculative (could be left) */
  }

  /* Swap vertices A and C, if C further down */
  if (vertex_C.y < bot->y) {
    left = bot; /* speculative (could be right) */
    bot = &vertex_C;
  } else {
    left = &vertex_C; /* speculative (could be right) */
  }

  /* Find 'left' and 'right' vertices
     (This refers to the sides connecting each one to the bot vertex, and
      does not necessary reflect their respective X coordinates) */
  if (right->y >= left->y)
    mid = left; /* 'East' vertex is most top */
  else
    mid = right; /* 'West' vertex is most top */

  if (MapPoint_clockwise(*left, *right, *bot)) {
    MapPoint const *const tmp = right;
    right = left;
    left = tmp;
  }
  DEBUGF("s: %" PRIMapCoord ",%" PRIMapCoord " "
         "w: %" PRIMapCoord ",%" PRIMapCoord " "
         "e: %" PRIMapCoord ",%" PRIMapCoord "\n",
         bot->x, bot->y, left->x, left->y, right->x, right->y);

  Edge left_edge, right_edge;
  make_edge(&left_edge, bot, left);
  make_edge(&right_edge, bot, right);

  /* Draw bottom half of triangle */
  plot_trapezium(write, arg, &left_edge, &right_edge, bot->y, mid->y);

  if (right->y != left->y) {
    /* Change the right or left gradient for the second (top) half. */
    MapCoord y_limit;
    if (mid == left) {
      /* Switch the left edge when the left vertex is further down.
         Advance the new left edge to mid->y + 1 to match the other side. */
      y_limit = right->y;
      make_edge(&left_edge, mid, right);
      (void)advance_edge(&left_edge, EdgeSide_Left);
      DEBUGF("Replaced left with x=%" PRIMapCoord " d=% "PRIMapCoord " g=%"
             PRIMapCoord "/%" PRIMapCoord "\n", left_edge.x, left_edge.dir,
             left_edge.d.y, left_edge.d.x);
    } else {
      /* Switch the right edge when the right vertex is further down. */
      y_limit = left->y;
      make_edge(&right_edge, mid, left);
      (void)advance_edge(&right_edge, EdgeSide_Right);
      DEBUGF("Replaced right with x=%" PRIMapCoord " d=%"PRIMapCoord " g=%"
             PRIMapCoord "/%" PRIMapCoord "\n", right_edge.x, right_edge.dir,
             right_edge.d.y, right_edge.d.x);
    }

    /* Draw top half of triangle. right_x and left_x were already
       updated to mid->y + 1 so begin from there. */
    plot_trapezium(write, arg, &left_edge, &right_edge, mid->y + 1, y_limit);
  }
}

void Shapes_rect(ShapesWriteFunction *const write, void *const arg,
  MapPoint const vertex_A, MapPoint const vertex_B)
{
  assert(write != NULL);
  DEBUG("Rectangle between %" PRIMapCoord ",%" PRIMapCoord " and %"
        PRIMapCoord ",%" PRIMapCoord, vertex_A.x,
        vertex_A.y, vertex_B.x, vertex_B.y);

  MapArea area;
  MapArea_from_points(&area, vertex_A, vertex_B);
  write(&area, arg);
}

static void write_circle(ShapesWriteFunction *const write, void *const arg,
  MapPoint const centre, MapArea const *const map_area)
{
  assert(write != NULL);
  assert(MapArea_is_valid(map_area));
  assert(map_area->min.y >= 0);

  MapArea trans_area;
  MapArea_translate(map_area, centre, &trans_area);
  DEBUGF("Painting area %" PRIMapCoord ",%" PRIMapCoord
         ",%" PRIMapCoord ",%" PRIMapCoord "\n",
    trans_area.min.x, trans_area.min.y, trans_area.max.x, trans_area.max.y);

  write(&trans_area, arg);

  MapArea_reflect_y(map_area, &trans_area);

  /* Don't write the row at y=0 twice */
  if (trans_area.max.y < 0 || --trans_area.max.y >= trans_area.min.y) {
    MapArea_translate(&trans_area, centre, &trans_area);
    DEBUGF("Painting area %" PRIMapCoord ",%" PRIMapCoord
           ",%" PRIMapCoord ",%" PRIMapCoord "\n",
      trans_area.min.x, trans_area.min.y, trans_area.max.x, trans_area.max.y);

    write(&trans_area, arg);
  }
}

void Shapes_circ(ShapesWriteFunction *const write, void *const arg,
  MapPoint const centre, MapCoord const radius)
{
  /*
    Expected drawing order:
       333      1 is the first block (and square root)
      55555     2 is a vertical reflection of 1
     1111111    3 reuses the square root used to draw 1
     1111111    4 is a vertical reflection of 3
     2222222    5 is drawn using the second square root
      66666     6 is a vertical reflection of 5
       444
   */
  MapCoord const radius_squared = radius * radius;
  MapArea map_area;

  DEBUG("Circle with centre %" PRIMapCoord ",%" PRIMapCoord
        " and radius %" PRIMapCoord, centre.x, centre.y, radius);

  MapCoord last_xoffset = -1;
  MapCoord yoffset;
  /* Increasing yoffset and decreasing last_xoffset converge until they overlap */
  for (yoffset = 0; last_xoffset == -1 || yoffset < last_xoffset; yoffset++) {
    /* Calculate the ends of the current row */
    MapCoord const xoffset = MapCoord_opp_to_adj(yoffset, radius_squared);
    assert(last_xoffset == -1 || xoffset <= last_xoffset);
    if (last_xoffset == xoffset) {
      continue;
    }

    if (last_xoffset >= 0) {
      /* Draw the preceding rows */
      assert(yoffset > 0);
      MapCoord const last_yoffset = yoffset - 1;
      map_area.max.y = last_yoffset;
      write_circle(write, arg, centre, &map_area);

      assert(last_yoffset < last_xoffset);
      MapArea const top_area = {
        .min = {.x = -last_yoffset, .y = last_xoffset},
        .max = {.x = last_yoffset, .y = last_xoffset},
      };
      write_circle(write, arg, centre, &top_area);
    }

    /* Start a new rectangle */
    map_area.min.x = -xoffset;
    map_area.max.x = +xoffset;
    map_area.min.y = yoffset;

    last_xoffset = xoffset;
  }

  DEBUGF("Draw the last rows\n");
  assert(yoffset > 0);
  MapCoord const last_yoffset = yoffset - 1;
  map_area.max.y = last_yoffset;
  write_circle(write, arg, centre, &map_area);

  assert(last_xoffset >= 0);
  if (last_yoffset < last_xoffset) {
    MapArea const top_area = {
      .min = {.x = -last_yoffset, .y = last_xoffset},
      .max = {.x = last_yoffset, .y = last_xoffset},
    };
    write_circle(write, arg, centre, &top_area);
  }
}

// no round function till C99
static inline MapCoord round_coord(double const x)
{
  return (MapCoord)(x >= 0 ? floor(x + 0.5) : ceil(x - 0.5));
}

static void steep_thick_line(ShapesWriteFunction *const write, void *const arg,
  MapPoint const start, MapPoint const end, MapCoord const thickness, MapPoint const d)
{
  assert(write != NULL);
  MapCoord const radius_squared = thickness * thickness;

  DEBUGF("Steep thick (y major)\n");
  MapCoord left_y_start, left_y_limit, right_y_start, right_y_limit;

  MapCoord const x_dir = (end.x >= start.x ? 1 : -1);
  MapCoord left_p = d.y, right_p = d.y; /* decision parameters */

  /* Calculate offsets (from end points) to where corners would be if line
  had square end caps. */
  MapPoint const corner_offset = {
    round_coord(sin(atan2(d.y, d.x)) * (double)thickness),
    round_coord(cos(atan2(d.y, d.x)) * (double)thickness)};

  DEBUG("Offsets to corners are %" PRIMapCoord ",%" PRIMapCoord,
    corner_offset.x, corner_offset.y);

  if (x_dir > 0) {
    left_y_start = start.y + corner_offset.y;
    left_y_limit = end.y + corner_offset.y;
    right_y_start = start.y - corner_offset.y;
    right_y_limit = end.y - corner_offset.y;
  } else {
    left_y_start = start.y - corner_offset.y;
    left_y_limit = end.y - corner_offset.y;
    right_y_start = start.y + corner_offset.y;
    right_y_limit = end.y + corner_offset.y;
  }

  MapCoord edge_min_x = start.x - corner_offset.x;
  MapCoord edge_max_x = start.x + corner_offset.x;

  MapCoord lowest_y = start.y - thickness, highest_y = end.y + thickness;
  DEBUG("Plot to rows %" PRIMapCoord " to %" PRIMapCoord "", lowest_y, highest_y);

  MapArea map_area;
  for (MapCoord y = lowest_y; y <= highest_y; y++)
  {
    MapCoord x_end_offset = -1, x_start_offset = -1; /* impossible sqr root */

    MapCoord min_x = edge_min_x;
    if (y > left_y_limit) {
      /* Calculate point on round end cap */
      x_end_offset = MapCoord_opp_to_adj(
        MapCoord_abs_diff(y, end.y), radius_squared);

      min_x = end.x - x_end_offset;
      DEBUG("Left x at %" PRIMapCoord " (on end cap)", min_x);
    } else if (y < left_y_start) {
      /* Calculate point on round start cap */
      x_start_offset = MapCoord_opp_to_adj(
        MapCoord_abs_diff(y, start.y), radius_squared);

      min_x = start.x - x_start_offset;
      DEBUG("Left x at %" PRIMapCoord " (on start cap)", min_x);
    } else {
      DEBUG("Left x at %" PRIMapCoord " (edge of line)", min_x);
      left_p -= 2 * d.x;
      if (left_p < 0) {
        left_p += 2 * d.y;
        edge_min_x += x_dir;
      }
    }

    MapCoord max_x = edge_max_x;
    if (y > right_y_limit) {
      /* Calculate point on round end cap */
      if (x_end_offset == -1) {
        x_end_offset = MapCoord_opp_to_adj(
          MapCoord_abs_diff(y, end.y), radius_squared);
      }
      max_x = end.x + x_end_offset;
      DEBUG("Right x at %" PRIMapCoord " (on end cap)", max_x);
    } else if (y < right_y_start) {
      /* Calculate point on round start cap */
      if (x_start_offset == -1) {
        x_start_offset = MapCoord_opp_to_adj(
          MapCoord_abs_diff(y, start.y), radius_squared);
      }
      max_x = start.x + x_start_offset;
      DEBUG("Right x at %" PRIMapCoord " (on start cap)", max_x);
    } else {
      DEBUG("Right x at %" PRIMapCoord " (edge of line)", max_x);
      right_p -= 2 * d.x;
      if (right_p < 0) {
        right_p += 2 * d.y;
        edge_max_x += x_dir;
      }
    }

    if (y == lowest_y || min_x != map_area.min.x || max_x != map_area.max.x) {
      if (y != lowest_y) {
        /* Draw the preceding rows */
        map_area.max.y = y - 1;
        write(&map_area, arg);
      }

      /* Start a new rectangle */
      map_area.min.x = min_x;
      map_area.max.x = max_x;
      map_area.min.y = y;
    }
  } /* next y */

  if (map_area.min.y <= highest_y) {
    /* Draw the final rectangle */
    map_area.max.y = highest_y;
    write(&map_area, arg);
  }
}

static void steep_line(ShapesWriteFunction *const write, void *const arg,
  MapPoint const start, MapPoint const end, MapPoint const d)
{
  assert(write != NULL);
  DEBUGF("Steep (y major)\n");
  MapCoord const x_dir = (end.x >= start.x ? 1 : -1);
  MapCoord p = d.y; /* decision parameter */
  MapArea map_area  = {start, start};

  MapCoord x = start.x;
  for (MapCoord y = start.y; y <= end.y; ++y) {
    /* Will the next point be in a different column? */
    p -= 2 * d.x;
    if (p < 0) {
      p += 2 * d.y;
      x += x_dir;

      /* Draw the current rectangle */
      DEBUGF("Draw the preceding rows at y %"PRIMapCoord "\n", y);
      map_area.max.y = y;
      write(&map_area, arg);

      /* Start a new rectangle */
      map_area.min.y = y + 1;
      map_area.min.x = map_area.max.x = x;
      DEBUGF("Start a new rectangle at y %"PRIMapCoord "\n", y);
    }
  } /* endwhile */

  if (map_area.min.y <= end.y) {
    /* Draw the final rectangle */
    DEBUGF("Draw the final rows,  map_area.min.x %" PRIMapCoord "\n",  map_area.min.x);
    map_area.max.y = end.y;
    write(&map_area, arg);
  }
}

static void shallow_thick_line(ShapesWriteFunction *const write,
  void *const arg, MapPoint const start, MapPoint const end,
  MapCoord const thickness, MapPoint const d)
{
  assert(write != NULL);
  MapCoord const radius_squared = thickness * thickness;

  DEBUGF("Shallow thick (x major)\n");
  MapCoord bot_x_start, bot_x_limit, top_x_start, top_x_limit;

  MapCoord const y_dir = (end.y >= start.y ? 1 : -1);
  MapCoord bot_p = d.x, top_p = d.x; /* decision parameters */

  /* Calculate offsets (from end points) to where corners would be if line
  had square end caps. */
  MapPoint const corner_offset = {
    round_coord(cos(atan2(d.x, d.y)) * (double)thickness),
    round_coord(sin(atan2(d.x, d.y)) * (double)thickness)};

  DEBUG("Offsets to corners are %" PRIMapCoord ",%" PRIMapCoord,
    corner_offset.x, corner_offset.y);

  if (y_dir > 0) {
    bot_x_start = start.x + corner_offset.x;
    bot_x_limit = end.x + corner_offset.x;
    top_x_start = start.x - corner_offset.x;
    top_x_limit = end.x - corner_offset.x;
  } else {
    bot_x_start = start.x - corner_offset.x;
    bot_x_limit = end.x - corner_offset.x;
    top_x_start = start.x + corner_offset.x;
    top_x_limit = end.x + corner_offset.x;
  }

  MapCoord edge_min_y = start.y - corner_offset.y;
  MapCoord edge_max_y = start.y + corner_offset.y;

  MapCoord lowest_x = start.x - thickness, highest_x = end.x + thickness;
  DEBUG("Plot to columns %" PRIMapCoord " to %" PRIMapCoord "",
    lowest_x, highest_x);

  MapArea map_area;
  for (MapCoord x = lowest_x; x <= highest_x; x++)
  {
    MapCoord y_end_offset = -1, y_start_offset = -1;

    MapCoord min_y = edge_min_y;
    if (x > bot_x_limit) {
      /* Calculate point on round end cap */
      y_end_offset = MapCoord_opp_to_adj(
        MapCoord_abs_diff(x, end.x), radius_squared);

      min_y = end.y - y_end_offset;
      DEBUG("Bottom y at %" PRIMapCoord " (on end cap)", min_y);
    } else if (x < bot_x_start) {
      /* Calculate point on round start cap */
      y_start_offset = MapCoord_opp_to_adj(
        MapCoord_abs_diff(x, start.x), radius_squared);

      min_y = start.y - y_start_offset;
      DEBUG("Bottom y at %" PRIMapCoord " (on start cap)", min_y);
    } else {
      DEBUG("Bottom y at %" PRIMapCoord " (edge of line)", min_y);
      bot_p -= 2 * d.y;
      if (bot_p < 0) {
        bot_p += 2 * d.x;
        edge_min_y += y_dir;
      }
    }

    MapCoord max_y = edge_max_y;
    if (x > top_x_limit) {
      /* Calculate point on round end cap */
      if (y_end_offset == -1) {
        y_end_offset = MapCoord_opp_to_adj(
          MapCoord_abs_diff(x, end.x), radius_squared);
      }
      max_y = end.y + y_end_offset;
      DEBUG("Top y at %" PRIMapCoord " (on end cap)", max_y);
    } else if (x < top_x_start) {
      /* Calculate point on round start cap */
      if (y_start_offset == -1) {
        y_start_offset = MapCoord_opp_to_adj(
          MapCoord_abs_diff(x, start.x), radius_squared);
      }
      max_y = start.y + y_start_offset;
      DEBUG("Top y at %" PRIMapCoord " (on start cap)", max_y);
    } else {
      DEBUG("Top y at %" PRIMapCoord " (edge of line)", max_y);
      top_p -= 2 * d.y;
      if (top_p < 0) {
        top_p += 2 * d.x;
        edge_max_y += y_dir;
      }
    }

    if (x == lowest_x || min_y != map_area.min.y || max_y != map_area.max.y) {
      if (x != lowest_x) {
        /* Draw the previous rectangle */
        map_area.max.x = x - 1;
        write(&map_area, arg);
      }

      /* Start a new rectangle */
      map_area.min.y = min_y;
      map_area.max.y = max_y;
      map_area.min.x = x;
    }
  } /* next x */

  if (map_area.min.x <= highest_x) {
    /* Draw the final rectangle */
    map_area.max.x = highest_x;
    write(&map_area, arg);
  }
}

static void shallow_line(ShapesWriteFunction *const write, void *const arg,
  MapPoint const start, MapPoint const end, MapPoint const d)
{
  assert(write != NULL);
  DEBUGF("Shallow (x major)\n");
  MapCoord const y_dir = (end.y >= start.y ? 1 : -1);
  MapCoord p = d.x; /* decision parameter */
  MapArea map_area = {start, start};

  MapCoord y = start.y;
  for (MapCoord x = start.x; x <= end.x; ++x) {
    /* Will the next point be in a different row? */
    p -= 2 * d.y;
    if (p < 0) {
      p += 2 * d.x;
      y += y_dir;

      /* Draw the current rectangle */
      map_area.max.x = x;
      write(&map_area, arg);

      /* Start a new rectangle */
      map_area.min.x = x + 1;
      map_area.min.y = map_area.max.y = y;
    }
  } /* endwhile */

  if (map_area.min.x <= end.x) {
    /* Draw the final rectangle */
    map_area.max.x = end.x;
    write(&map_area, arg);
  }
}

void Shapes_line(ShapesWriteFunction *const write, void *const arg,
  MapPoint start, MapPoint end,
  MapCoord const thickness)
{
  DEBUG("Line of thickness %" PRIMapCoord
        " from %" PRIMapCoord ",%" PRIMapCoord
        " to %" PRIMapCoord ",%" PRIMapCoord,
        thickness, start.x, start.y, end.x, end.y);

  MapPoint const d = MapPoint_abs_diff(end, start);

  if (d.y > d.x) {
    if (start.y > end.y) {
      MAP_POINT_SWAP(start, end);
    }
    if (thickness) {
      steep_thick_line(write, arg, start, end, thickness, d);
    } else {
      steep_line(write, arg, start, end, d);
    }
  } else {
    if (start.x > end.x) {
      MAP_POINT_SWAP(start, end);
    }
    if (thickness) {
      shallow_thick_line(write, arg, start, end, thickness, d);
    } else {
      shallow_line(write, arg, start, end, d);
    }
  }
}

enum {
  STACK_CHUNK_SIZE = 32,
};

typedef struct {
  MapCoord y;
  MapCoord min_x;
  MapCoord max_x;
  int dy;
} Segment;

typedef struct {
  int sp;
  int sl;
  Segment *mem;
} Stack;

static bool init_stack(Stack *const stack)
{
  assert(stack != NULL);

  *stack = (Stack){
    .mem = malloc(STACK_CHUNK_SIZE * sizeof(Segment)),
    .sp = 0,
    .sl = STACK_CHUNK_SIZE,
  };

  return stack->mem;
}

static void term_stack(Stack const *const stack)
{
  assert(stack != NULL);
  assert(stack->sp >= 0);
  assert(stack->sp <= stack->sl);
  free(stack->mem);
}

static bool push_segment(Stack *const stack, MapCoord const y, MapCoord const min_x,
  MapCoord const max_x, MapCoord const dy)
{
  assert(stack != NULL);
  assert(stack->sp >= 0);
  assert(stack->sp <= stack->sl);
  assert(min_x <= max_x);
  assert(dy == -1 || dy == +1);

  if (stack->sp >= stack->sl) {
    /* Attempt to extend fill stack */
    DEBUG("Extending stack from %d to %d",
          stack->sl, stack->sl + STACK_CHUNK_SIZE);

    void *const ext = realloc(stack->mem,
                        (size_t)(stack->sl + STACK_CHUNK_SIZE) * sizeof(Segment));
    if (!ext) {
      return false;
    }

    stack->sl += STACK_CHUNK_SIZE;
    stack->mem = ext;
  }

  DEBUG("Pushing item %d: span %" PRIMapCoord ",%" PRIMapCoord " on line %" PRIMapCoord
        " (parent was line %" PRIMapCoord ")", stack->sp, min_x, max_x, y, y - dy);

  stack->mem[stack->sp++] = (Segment){
    .y = y,
    .min_x = min_x,
    .max_x = max_x,
    .dy = dy,
  };

  return true;
}

static Segment pull_segment(Stack *const stack)
{
  assert(stack != NULL);
  assert(stack->sp >= 0);
  assert(stack->sp <= stack->sl);
  --stack->sp;
  DEBUG("Pulling item %d from stack", stack->sp);
  return stack->mem[stack->sp];
}

static bool stack_is_empty(Stack const *const stack)
{
  assert(stack != NULL);
  assert(stack->sp >= 0);
  assert(stack->sp <= stack->sl);
  return stack->sp == 0;
}

static MapCoord search_east_for_match(ShapesReadFunction *const read,
  void *const arg, size_t const find, MapPoint pos, MapCoord const x_limit)
{
  for (pos.x++; pos.x <= x_limit; pos.x++) {
    size_t const tile = read(pos, arg);
    if (tile == find) {
      break;
    }
  } /* next x */

  return pos.x;
}

static bool do_flood_fill(Stack *const stack, ShapesReadFunction *const read,
  ShapesWriteFunction *const write, void *const arg,
  size_t const find, MapPoint pos, MapCoord const limit)
{
  assert(read != NULL);
  assert(write != NULL);

  /*
   * A Seed Fill Algorithm
   * by Paul Heckbert
   * from "Graphics Gems", Academic Press, 1990
   */

  /*
   * Filled horizontal segment of scanline y for min_x <= x <= max_x.
   * Parent segment was on line y-dy.  dy=1 or -1
   */

  /* needed in some cases */
  if (!push_segment(stack, pos.y, pos.x, pos.x, 1)) {
    return false;
  }

  /* seed segment (popped 1st) */
  if (!push_segment(stack, pos.y + 1, pos.x, pos.x, -1)) {
    return false;
  }

  while (!stack_is_empty(stack))
  {
    /* pull segment off stack and fill a neighbouring scan line */
    Segment const seg = pull_segment(stack);

    pos = (MapPoint){seg.min_x, seg.y + seg.dy};

    /*
     * segment of scan line pos.y - seg.dy for seg.min_x <= pos.x <= seg.max_x was
     * previously filled, now explore adjacent values in scan line pos.y
     */

    MapArea west_fill = {pos, pos};

    /* Search westward until we find a mismatching value */
    MapCoord const west_limit = pos.x - limit;
    for (; pos.x > west_limit; pos.x--) {
      if (read(pos, arg) != find) {
        break;
      }
    } /* next x */

    west_fill.min.x = pos.x + 1;
    MapCoord next_seg_min_x;
    if (west_fill.min.x > west_fill.max.x) {
      /* Nothing found at start point or westward: search eastward until we find a matching value */
      next_seg_min_x = pos.x = search_east_for_match(read, arg, find, pos, seg.max_x);
      if (pos.x > seg.max_x) {
        continue; /* nothing found eastward either */
      }
    } else {
      /* Fill westward to the mismatching value */
      write(&west_fill, arg);

      if (west_fill.min.x < west_fill.max.x) {
        /* leak on west? (% can bleed backwards)
               /\     seg.min_x  seg.max_x
     Backwards ||     |          |
                      ############---seg.y
west_fill.min.y---%%%%#
                  |   |
    west_fill.min.x   west_fill.max.x
         */
        if (!push_segment(stack, pos.y, west_fill.min.x, west_fill.max.x - 1, -seg.dy)) {
          return false;
        }
      }

      /* Start point was already filled if required so skip it */
      pos.x = seg.min_x + 1;

      /* Defer adding the segment to bleed forwards from the west fill because
         it may be possible to combine it with that for the first east fill */
      next_seg_min_x = west_fill.min.x;
    }

    /* Search for matching segments adjacent to (directly above or below) the previously-filled segment,
       with the exception of the westernmost value, which was already scanned. */
    do {
      MapArea east_fill = {pos, pos};

      /* Search eastward until we find a mismatching value */
      MapCoord const east_limit = pos.x + limit;
      for (; pos.x < east_limit; pos.x++) {
        if (read(pos, arg) != find) {
          break;
        }
      }
      east_fill.max.x = pos.x - 1;
      if (east_fill.max.x >= east_fill.min.x) {
        /* Fill eastward to the mismatching value */
        write(&east_fill, arg);
      }

      /* leak on east? (% can bleed forwards)
         This illustrates a possible scenario on the first iteration of the loop, where only a
         single segment is pushed to allow both west and east fills to bleed southward.
         Any subsequent segments pushed will only be for southerly bleeding of east fills.

   Forwards ||        seg.min_x  seg.max_x
            \/        |          |
                      ############---seg.y
east_fill.min.y---%%%%%%%%%%
                  |    |   |
     next_seg_min_x    |   east_fill.max.x
                       east_fill.min.x
       */
      if (!push_segment(stack, pos.y, next_seg_min_x, east_fill.max.x, seg.dy)) {
        return false;
      }

      if (east_fill.max.x > seg.max_x) {
        /* leak on east? (% can bleed backwards)
                /\    seg.min_x  seg.max_x
      Backwards ||    |          |
                      ############---------seg.y
    east_fill.min.y----###########%%%%
                       |             |
         east_fill.min.x             east_fill.max.x */
        if (!push_segment(stack, pos.y, seg.max_x + 1, east_fill.max.x, -seg.dy)) {
          return false;
        }
      }

      /* Search eastwards until we find a matching value */
      next_seg_min_x = pos.x = search_east_for_match(read, arg, find, pos, seg.max_x);

    } while (pos.x <= seg.max_x);
  } /* endwhile (sp > 0) */

  return true;
}


bool Shapes_flood(ShapesReadFunction *const read, ShapesWriteFunction *const write,
  void *const arg, size_t const find, MapPoint const centre, MapCoord const limit)
{
  Stack stack;
  if (!init_stack(&stack))
  {
    return false;
  }

  bool const success = do_flood_fill(&stack, read, write, arg, find, centre, limit);
  term_stack(&stack);
  return success;
}
