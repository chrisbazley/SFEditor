/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Vector type for OS coordinates
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

#include "wimp.h" // For BBox
#include "macros.h"
#include "Vertex.h"

void BBox_expand(BBox *const bounding_box, Vertex const point)
{
  assert(bounding_box != NULL);
  DEBUG("Will expand bounding box %d,%d,%d,%d to include point %d,%d",
        bounding_box->xmin, bounding_box->ymin, bounding_box->xmax,
        bounding_box->ymax, point.x, point.y);

  if (point.x < bounding_box->xmin)
    bounding_box->xmin = point.x;

  if (point.y < bounding_box->ymin)
    bounding_box->ymin = point.y;

  if (point.x > bounding_box->xmax)
    bounding_box->xmax = point.x;

  if (point.y > bounding_box->ymax)
    bounding_box->ymax = point.y;

  DEBUG("Bounding box is now %d,%d,%d,%d", bounding_box->xmin,
        bounding_box->ymin, bounding_box->xmax, bounding_box->ymax);
}

void BBox_expand_for_area(BBox *const bounding_box, BBox const *const b)
{
  assert(bounding_box != NULL);
  DEBUG("Will expand bounding box %d,%d,%d,%d to include area %d,%d,%d,%d",
        bounding_box->xmin, bounding_box->ymin, bounding_box->xmax,
        bounding_box->ymax, b->xmin, b->ymin, b->xmax, b->ymax);

  assert(BBox_is_valid(b));

  if (b->xmin < bounding_box->xmin) {
    bounding_box->xmin = b->xmin;
  }
  if (b->ymin < bounding_box->ymin) {
    bounding_box->ymin = b->ymin;
  }
  if (b->xmax > bounding_box->xmax) {
    bounding_box->xmax = b->xmax;
  }
  if (b->ymax > bounding_box->ymax) {
    bounding_box->ymax = b->ymax;
  }

  DEBUG("Bounding box is now %d,%d,%d,%d", bounding_box->xmin,
        bounding_box->ymin, bounding_box->xmax, bounding_box->ymax);
}

void BBox_translate(BBox const *const bounding_box, Vertex const point,
  BBox *const result)
{
  assert(BBox_is_valid(bounding_box));
  DEBUG("Will translate bounding box %d,%d,%d,%d by %d,%d",
        bounding_box->xmin, bounding_box->ymin, bounding_box->xmax,
        bounding_box->ymax, point.x, point.y);

  result->xmin = bounding_box->xmin + point.x;
  result->ymin = bounding_box->ymin + point.y;
  result->xmax = bounding_box->xmax + point.x;
  result->ymax = bounding_box->ymax + point.y;

  DEBUG("Bounding box is now %d,%d,%d,%d", result->xmin,
        result->ymin, result->xmax, result->ymax);
}

bool BBox_contains(BBox const *const container, BBox const *const object)
{
  assert(BBox_is_valid(container));
  assert(BBox_is_valid(object));

  bool const contains = container->xmin <= object->xmin &&
         container->ymin <= object->ymin &&
         container->xmax >= object->xmax &&
         container->ymax >= object->ymax;

  DEBUGF("Bounding box %d,%d,%d,%d %s %d,%d,%d,%d\n",
         container->xmin, container->ymin, container->xmax, container->ymax,
         contains ? "contains" : "does not contain",
         object->xmin, object->ymin, object->xmax, object->ymax);
  return contains;
}

void BBox_intersection(BBox const *const a, BBox const *const b,
  BBox *const result)
{
  assert(BBox_is_valid(a));
  assert(BBox_is_valid(b));
  assert(result);

  DEBUG("Find intersection of bounding box %d,%d,%d,%d and %d,%d,%d,%d",
        a->xmin, a->ymin, a->xmax, a->ymax,
        b->xmin, b->ymin, b->xmax, b->ymax);

  result->xmin = HIGHEST(a->xmin, b->xmin);
  result->ymin = HIGHEST(a->ymin, b->ymin);
  result->xmax = LOWEST(a->xmax, b->xmax);
  result->ymax = LOWEST(a->ymax, b->ymax);

  DEBUG("Intersection is %d,%d,%d,%d (%s)",
        result->xmin, result->ymin, result->xmax, result->ymax,
        BBox_is_valid(result) ? "valid" : "invalid");
}
