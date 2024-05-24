/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission flightpaths
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

#include "stdlib.h"
#include <inttypes.h>
#include <limits.h>

#include "Debug.h"
#include "Macros.h"
#include "LinkedList.h"
#include "Reader.h"
#include "Writer.h"

#include "CoarseCoord.h"
#include "Paths.h"
#include "PathsData.h"

enum {
  PathsMax = 8,
  PathMaxWaypoints = 64,
  BytesPerWaypoint = 4,
  WaypointPadding = 1,
  BytesPerPath = (PathMaxWaypoints * BytesPerWaypoint) + 4,
};

struct Waypoint
{
  Path *path;
  LinkedListItem link;
  CoarsePoint3d coords;
  LinkedList ref_list;
  LinkedListItem ref_link;
  unsigned char index;
};

struct Path
{
  PathsData *paths;
  LinkedListItem link;
  int count;
  LinkedList waypoints;
  unsigned char index;
};

void paths_init(PathsData *const paths)
{
  assert(paths);
  *paths = (PathsData){.count = 0, .state = PathsDataState_PreWrite};
  linkedlist_init(&paths->list);
}

void paths_destroy(PathsData *const paths)
{
  assert(paths);

  LINKEDLIST_FOR_EACH_SAFE(&paths->list, item, tmp)
  {
    Path *const path = CONTAINER_OF(item, Path, link);
    path_delete(path);
  }
}

Path *paths_add(PathsData *const paths)
{
  assert(paths);
  assert(paths->count >= 0);
  assert(paths->count <= PathsMax);

  if (paths->count == PathsMax)
  {
    return NULL;
  }

  Path *const path = malloc(sizeof(*path));
  if (path)
  {
    *path = (Path){.paths = paths, .count = 0};

    linkedlist_insert(&paths->list,
      linkedlist_get_tail(&paths->list),
      &path->link);

    paths->count++;
    path->paths->state = PathsDataState_PreWrite;
  }
  return path;
}

Waypoint *path_add_waypoint(Path *const path, CoarsePoint3d coords)
{
  assert(path);
  assert(path->paths);
  assert(path->count >= 0);
  assert(path->count <= PathMaxWaypoints);

  if (path->count == PathMaxWaypoints)
  {
    return NULL;
  }

  Waypoint *const waypoint = malloc(sizeof(*waypoint));
  if (waypoint)
  {
    *waypoint = (Waypoint){.path = path, .coords = coords};
    linkedlist_init(&waypoint->ref_list);

    linkedlist_insert(&path->waypoints,
      linkedlist_get_tail(&path->waypoints), &waypoint->link);

    path->count++;
    path->paths->state = PathsDataState_PreWrite;
  }
  return waypoint;
}

void waypoint_delete(Waypoint *const waypoint)
{
  assert(waypoint);
  assert(waypoint->path);
  assert(waypoint->path->paths);

  Path *const path = waypoint->path;
  linkedlist_remove(&path->waypoints, &waypoint->link);
  free(waypoint);

  assert(path->count > 0);
  --path->count;
  path->paths->state = PathsDataState_PreWrite;
}

void path_delete(Path *const path)
{
  assert(path);
  assert(path->paths);

  LINKEDLIST_FOR_EACH_SAFE(&path->waypoints, item, tmp)
  {
    Waypoint *const waypoint = CONTAINER_OF(item, Waypoint, link);
    waypoint_delete(waypoint);
  }

  PathsData *const paths = path->paths;
  linkedlist_remove(&paths->list, &path->link);
  free(path);

  assert(paths->count > 0);
  --paths->count;
  paths->state = PathsDataState_PreWrite;
}

SFError paths_read_pad(PathsData *const paths, Reader *const reader)
{
  SFError err = paths_read(paths, reader);
  if (SFError_fail(err)) {
    return err;
  }

  assert(PathsMax <= LONG_MAX);
  long int const padding2 = PathsMax - (long)paths->count;
  if (reader_fseek(reader, padding2 * BytesPerPath, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading paths data at %ld\n", reader_ftell(reader));
  paths->state = PathsDataState_PreWrite;
  return SFERROR(OK);
}

SFError paths_read(PathsData *const paths, Reader *const reader)
{
  assert(paths);

  int32_t num_paths = 0;
  if (!reader_fread_int32(&num_paths, reader))
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Flightpaths count is %" PRId32 "\n", num_paths);

  if (num_paths < 0 || num_paths > PathsMax)
  {
    return SFERROR(BadNumPaths);
  }

  for (int32_t i = 0; i < num_paths; ++i)
  {
    DEBUGF("Reading flightpath %" PRId32 " data at %ld\n",
      i, reader_ftell(reader));
    int32_t num_waypoints = 0;
    if (!reader_fread_int32(&num_waypoints, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Flightpath %" PRId32 " waypoints count is %" PRId32 "\n",
           i, num_waypoints);

    if (num_waypoints < 0 || num_waypoints > PathMaxWaypoints)
    {
      return SFERROR(BadNumWaypoints);
    }

    Path *const path = paths_add(paths);
    if (!path)
    {
      return SFERROR(NoMem);
    }

    for (int32_t j = 0; j < num_waypoints; ++j)
    {
      DEBUGF("Reading flightpath %" PRId32 " waypoint %" PRId32 " data at %ld\n",
        i, j, reader_ftell(reader));
      CoarsePoint3d coords = {0};
      if (!CoarsePoint3d_read(&coords, reader))
      {
        return SFERROR(ReadFail);
      }
      if (reader_fseek(reader, WaypointPadding, SEEK_CUR))
      {
        return SFERROR(BadSeek);
      }
      if (!path_add_waypoint(path, coords))
      {
        return SFERROR(NoMem);
      }
    }

    long int const padding = PathMaxWaypoints - num_waypoints;
    if (reader_fseek(reader, padding * BytesPerWaypoint, SEEK_CUR))
    {
      return SFERROR(BadSeek);
    }
  }

  paths->state = PathsDataState_PreWrite;
  return SFERROR(OK);
}

static void write_waypoint(Waypoint const *const waypoint,
  Writer *const writer)
{
  assert(waypoint);
  CoarsePoint3d_write(waypoint->coords, writer);
  writer_fseek(writer, WaypointPadding, SEEK_CUR);
}

static void write_path(Path const *const path, Writer *const writer)
{
  assert(path);

  assert(path->count >= 0);
  assert(path->count < PathMaxWaypoints);
  writer_fwrite_int32(path->count, writer);

  LINKEDLIST_FOR_EACH(&path->waypoints, item)
  {
    Waypoint const *const waypoint = CONTAINER_OF(item, Waypoint, link);
    write_waypoint(waypoint, writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }

  long int const padding = PathMaxWaypoints - path->count;
  writer_fseek(writer, padding * BytesPerWaypoint, SEEK_CUR);
}

static inline void path_pre_write(Path *const path, int const pindex)
{
  assert(path);
  path->index = pindex;

  int index = 0;
  LINKEDLIST_FOR_EACH(&path->waypoints, item)
  {
    Waypoint *const waypoint = CONTAINER_OF(item, Waypoint, link);
    waypoint->index = index++;
  }
}

void paths_pre_write(PathsData *const paths)
{
  assert(paths);
  int index = 0;
  LINKEDLIST_FOR_EACH(&paths->list, item)
  {
    Path *const path = CONTAINER_OF(item, Path, link);
    path_pre_write(path, index++);
  }

  paths->state = PathsDataState_Write;
}

void paths_write_pad(PathsData *const paths, Writer *const writer)
{
  paths_write(paths, writer);
  if (writer_ferror(writer)) {
    return;
  }

  size_t const padding = PathsMax - paths->count;
  writer_fseek(writer, (long)padding * BytesPerPath, SEEK_CUR);
  DEBUGF("Finished writing paths data at %ld\n", writer_ftell(writer));
}

void paths_write(PathsData *const paths, Writer *const writer)
{
  assert(paths);
  assert(paths->state == PathsDataState_Write);

  assert(paths->count >= 0);
  assert(paths->count <= PathsMax);
  assert(paths->count <= INT32_MAX);
  writer_fwrite_int32((int32_t)paths->count, writer);

  LINKEDLIST_FOR_EACH(&paths->list, item)
  {
    Path const *const path = CONTAINER_OF(item, Path, link);
    write_path(path, writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }
}

Path *path_from_index(PathsData *const paths, int const index)
{
  /* Only expected to be used on mission load, otherwise we should
     substitute an array */
  assert(paths);
  assert(paths->state == PathsDataState_PreWrite);

  int i = 0;
  LINKEDLIST_FOR_EACH(&paths->list, item)
  {
    if (index == i++)
    {
      Path *const path = CONTAINER_OF(item, Path, link);
      DEBUGF("Decoded path index %d as %p\n", index, (void *)path);
      return path;
    }
  }
  DEBUGF("Failed to decode path index %d\n", index);
  return NULL;
}

Waypoint *waypoint_from_index(Path *const path, int const index)
{
  /* Only expected to be used on mission load, otherwise we should
     substitute an array */
  assert(path);
  assert(path->paths);
  assert(path->paths->state == PathsDataState_PreWrite);

  int i = 0;
  LINKEDLIST_FOR_EACH(&path->waypoints, item)
  {
    if (index == i++)
    {
      Waypoint *const wp = CONTAINER_OF(item, Waypoint, link);
      DEBUGF("Decoded waypoint index %d as %p\n", index, (void *)wp);
      return wp;
    }
  }
  DEBUGF("Failed to decode waypoint index %d\n", index);
  /* Hard mission 9 has invalid starting waypoint so allow that */
  return index == 1 ? waypoint_from_index(path, 0) : NULL;
}

int waypoint_get_index(Waypoint const *const waypoint)
{
  assert(waypoint);
  assert(waypoint->path);
  assert(waypoint->path->paths);
  assert(waypoint->path->paths->state == PathsDataState_Write);
  DEBUGF("Waypoint index is %d\n", waypoint->index);
  return waypoint->index;
}

int path_get_index(Path const *const path)
{
  assert(path);
  assert(path->paths);
  assert(path->paths->state == PathsDataState_Write);
  DEBUGF("Path index is %d\n", path->index);
  return path->index;
}

Path *waypoint_get_path(Waypoint const *const waypoint)
{
  assert(waypoint);
  return waypoint->path;
}
