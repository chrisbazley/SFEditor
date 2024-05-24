/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal graphics set
 *  Copyright (C) 2021 Christopher Bazley
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
#include "debug.h"
#include "Macros.h"
#include "Reader.h"
#include "PathTail.h"
#include "StrDict.h"

#include "OSnakes.h"
#include "ObjGfxMesh.h"
#include "ObjGfx.h"
#include "ObjGfxData.h"

static StrDict file_dict;

static void init_all(ObjGfx *const graphics)
{
  assert(graphics);
  ObjGfxMeshes_init(&graphics->meshes);
  ObjSnakes_init(&graphics->snakes);
}

static void destroy_all(ObjGfx *const graphics)
{
  assert(graphics);
  ObjGfxMeshes_free(&graphics->meshes);
  ObjSnakes_free(&graphics->snakes);
}

static SFError ObjGfx_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  ObjGfx *const graphics = CONTAINER_OF(dfile, ObjGfx, dfile);

  destroy_all(graphics);
  init_all(graphics);

  return ObjGfxMeshes_read(&graphics->meshes, reader);
}

static void ObjGfx_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  ObjGfx *const graphics = CONTAINER_OF(dfile, ObjGfx, dfile);

  destroy_all(graphics);
  dfile_destroy(&graphics->dfile);
  free(graphics);
}

static void ObjGfx_cleanup(void)
{
  strdict_destroy(&file_dict, NULL, NULL);
}

void ObjGfx_init(void)
{
  strdict_init(&file_dict);
  atexit(ObjGfx_cleanup);
}

DFile *ObjGfx_get_dfile(ObjGfx *const graphics)
{
  assert(graphics);
  return &graphics->dfile;
}

ObjGfx *ObjGfx_create(void)
{
  ObjGfx *const graphics = malloc(sizeof(*graphics));
  if (graphics)
  {
    *graphics = (ObjGfx){{0}};

    dfile_init(&graphics->dfile, ObjGfx_read_cb, NULL, NULL,
               ObjGfx_destroy_cb);

    init_all(graphics);
  }
  return graphics;
}

void ObjGfx_load_metadata(ObjGfx *graphics)
{
  assert(graphics);

  char const *const filename = dfile_get_name(&graphics->dfile);
  if (filename == NULL) {
    return;
  }

  char *const leaf_name = pathtail(filename, 1);

  ObjSnakes_load(&graphics->snakes, leaf_name,
                    ObjGfxMeshes_get_ground_count(&graphics->meshes));
}


bool ObjGfx_share(ObjGfx *const graphics)
{
  assert(graphics);
  return dfile_set_shared(&graphics->dfile, &file_dict);
}

ObjGfx *ObjGfx_get_shared(char const *const filename)
{
  DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, ObjGfx, dfile) : NULL;
}
