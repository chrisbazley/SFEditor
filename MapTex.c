/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map tile set
 *  Copyright (C) 2007 Christopher Bazley
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
#include "stdlib.h"
#include <assert.h>

/* My headers */
#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "PathTail.h"
#include "StrDict.h"

#include "MSnakes.h"
#include "MapTexBitm.h"
#include "Smooth.h"
#include "MTransfers.h"
#include "MapTex.h"
#include "MapTexData.h"

#ifdef USE_OPTIONAL
#include "Optional.h"
#endif

static StrDict file_dict;

static void init_all(MapTex *const textures)
{
  assert(textures);

  MapTexBitmaps_init(&textures->tiles.obj);
  MapTexGroups_init(&textures->groups.obj);
  MapSnakes_init(&textures->snakes.obj);
  MapTransfers_init(&textures->transfers.obj);
}

static void destroy_all(MapTex *const textures)
{
  assert(textures);

  MapTexBitmaps_free(&textures->tiles.obj);
  MapTexGroups_free(&textures->groups.obj);
  MapSnakes_free(&textures->snakes.obj);
  MapTransfers_free(&textures->transfers.obj);
}

static SFError MapTex_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  MapTex *const textures = CONTAINER_OF(dfile, MapTex, dfile);

  destroy_all(textures);
  init_all(textures);

  return MapTexBitmaps_read(&textures->tiles.obj, reader);
}

static void MapTex_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  MapTex *const textures = CONTAINER_OF(dfile, MapTex, dfile);

  destroy_all(textures);
  dfile_destroy(&textures->dfile);
  free(textures);
}

static void MapTex_cleanup(void)
{
  strdict_destroy(&file_dict, (StrDictDestructorFn *)NULL, &file_dict);
}

void MapTex_init(void)
{
  strdict_init(&file_dict);
  atexit(MapTex_cleanup);
}

DFile *MapTex_get_dfile(MapTex *const textures)
{
  assert(textures);
  return &textures->dfile;
}

_Optional MapTex *MapTex_create(void)
{
  _Optional MapTex *const textures = malloc(sizeof(*textures));
  if (textures)
  {
    *textures = (MapTex){.dfile = {0}};

    dfile_init(&textures->dfile, MapTex_read_cb, (DFileWriteFn *)NULL,
               (DFileGetMinSizeFn *)NULL, MapTex_destroy_cb);

    init_all(&*textures);
  }
  return textures;
}

void MapTex_load_metadata(MapTex *const textures)
{
  assert(textures);

  _Optional char const *const filename = dfile_get_name(&textures->dfile);
  if (filename == NULL) {
    return;
  }

  char *const leaf_name = pathtail(&*filename, 1);

  MapTransfers_load_all(&textures->transfers.obj, leaf_name);

  MapTexGroups_load(&textures->groups.obj, leaf_name,
                    MapTexBitmaps_get_count(&textures->tiles.obj));

  MapSnakes_load(&textures->snakes.obj, leaf_name,
                    MapTexBitmaps_get_count(&textures->tiles.obj));
}

bool MapTex_share(MapTex *const textures)
{
  assert(textures);
  return dfile_set_shared(&textures->dfile, &file_dict);
}

_Optional MapTex *MapTex_get_shared(char const *const filename)
{
  _Optional DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, MapTex, dfile) : NULL;
}
