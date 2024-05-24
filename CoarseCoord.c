/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Coarse coordinates type definition
 *  Copyright (C) 2020 Christopher Bazley
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

#include "Macros.h"
#include "Debug.h"
#include "CoarseCoord.h"
#include "Reader.h"
#include "Writer.h"

CoarsePoint3d CoarsePoint3d_from_fine(FinePoint3d const point)
{
  return (CoarsePoint3d){
    CoarseCoord_from_fine(point.x),
    CoarseCoord_from_fine(point.y),
    CoarseCoord_from_fine(point.z),
  };
}

FinePoint3d FinePoint3d_from_coarse(CoarsePoint3d const point)
{
  return (FinePoint3d){
    FineCoord_from_coarse(point.x),
    FineCoord_from_coarse(point.y),
    FineCoord_from_coarse(point.z),
  };
}

bool CoarsePoint3d_read(CoarsePoint3d *const point,
  Reader *const reader)
{
  unsigned char coords[3] = {0};
  if (reader_fread(coords, sizeof(coords), 1, reader) != 1)
  {
    return false;
  }
  size_t i = 0;
  point->x = coords[i++];
  point->y = coords[i++];
  point->z = coords[i++];
  assert(i == ARRAY_SIZE(coords));
  DEBUGF("Finished reading coarse 3D coordinate data {%" PRICoarseCoord ",%" PRICoarseCoord ",%" PRICoarseCoord "} at %ld\n",
    point->x, point->y, point->z, reader_ftell(reader));
  return true;
}

void CoarsePoint3d_write(CoarsePoint3d const point,
  Writer *const writer)
{
  writer_fputc(point.x, writer);
  writer_fputc(point.y, writer);
  writer_fputc(point.z, writer);
  DEBUGF("Finished writing coarse 3D coordinate data {%" PRICoarseCoord ",%" PRICoarseCoord ",%" PRICoarseCoord "} at %ld\n",
    point.x, point.y, point.z, writer_ftell(writer));
}

bool CoarsePoint2d_read(CoarsePoint2d *const point,
  Reader *const reader)
{
  unsigned char coords[2] = {0};
  if (reader_fread(coords, sizeof(coords), 1, reader) != 1)
  {
    return false;
  }
  size_t i = 0;
  point->x = coords[i++];
  point->y = coords[i++];
  assert(i == ARRAY_SIZE(coords));
  DEBUGF("Finished reading coarse 2D coordinate data {%" PRICoarseCoord ",%" PRICoarseCoord "} at %ld\n",
    point->x, point->y, reader_ftell(reader));
  return true;
}

void CoarsePoint2d_write(CoarsePoint2d const point,
  Writer *const writer)
{
  writer_fputc(point.x, writer);
  writer_fputc(point.y, writer);
  DEBUGF("Finished writing coarse 2D coordinate data {%" PRICoarseCoord ",%" PRICoarseCoord "} at %ld\n",
    point.x, point.y, writer_ftell(writer));
}

bool FinePoint3d_read(FinePoint3d *const point, Reader *const reader)
{
  uint32_t coords[3];
  for (size_t j = 0; j < ARRAY_SIZE(coords); ++j) {
    if (!reader_fread_uint32(&coords[j], reader)) {
      return false;
    }
  }

  size_t i = 0;
  point->x = coords[i++];
  point->y = coords[i++];
  point->z = coords[i++];
  assert(i == ARRAY_SIZE(coords));
  DEBUGF("Finished reading fine 3D coordinate data {%" PRIFineCoord ",%" PRIFineCoord ",%" PRIFineCoord "} at %ld\n",
    point->x, point->y, point->z, reader_ftell(reader));
  return true;
}

void FinePoint3d_write(FinePoint3d const point, Writer *const writer)
{
  writer_fwrite_uint32(point.x, writer);
  writer_fwrite_uint32(point.y, writer);
  writer_fwrite_uint32(point.z, writer);
  DEBUGF("Finished writing fine 3D coordinate data {%" PRIFineCoord ",%" PRIFineCoord ",%" PRIFineCoord "} at %ld\n",
    point.x, point.y, point.z, writer_ftell(writer));
}
