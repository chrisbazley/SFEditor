/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygon colours
 *  Copyright (C) 2021 Christopher Bazley
 */

/* ANSI headers */
#include <assert.h>
#include "stdlib.h"
#include <stdint.h>

#include "flex.h"

#include "Macros.h"
#include "debug.h"
#include "NoBudge.h"
#include "StrDict.h"

#include "DFileData.h"
#include "PolyCol.h"
#include "utils.h"

enum {
  PREALLOC_SIZE = 4096,
};

static StrDict file_dict;

struct PolyColData {
  DFile dfile;
  void *flex;
};

static SFError polycol_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  PolyColData *const poly_colours = CONTAINER_OF(dfile, PolyColData, dfile);
  SFError err = SFERROR(OK);

  nobudge_register(PREALLOC_SIZE);
  if (reader_fread(poly_colours->flex, PolyColMax, 1, reader) != 1)
  {
    err = SFERROR(ReadFail);
  }
  nobudge_deregister();

  return check_trunc_or_ext(reader, err);
}

static void polycol_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  PolyColData *const poly_colours = CONTAINER_OF(dfile, PolyColData, dfile);
  flex_free(&poly_colours->flex);
  dfile_destroy(&poly_colours->dfile);
  free(poly_colours);
}

static void polycol_cleanup(void)
{
  strdict_destroy(&file_dict, NULL, NULL);
}

void polycol_init(void)
{
  strdict_init(&file_dict);
  atexit(polycol_cleanup);
}

PolyColData *polycol_create(void)
{
  PolyColData *const poly_colours = malloc(sizeof(*poly_colours));
  if (poly_colours)
  {
    if (!flex_alloc(&poly_colours->flex, PolyColMax))
    {
      free(poly_colours);
      return NULL;
    }

    dfile_init(&poly_colours->dfile, polycol_read_cb, NULL, NULL,
               polycol_destroy_cb);
  }
  return poly_colours;
}

bool polycol_share(PolyColData *const poly_colours)
{
  assert(poly_colours);
  return dfile_set_shared(&poly_colours->dfile, &file_dict);
}

PolyColData *polycol_get_shared(char const *const filename)
{
  assert(filename);
  DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, PolyColData, dfile) : NULL;
}

DFile *polycol_get_dfile(PolyColData *const poly_colours)
{
  assert(poly_colours);
  return &poly_colours->dfile;
}

size_t polycol_get_colour(PolyColData const *const poly_colours, size_t const index)
{
  assert(poly_colours);
  assert(index >= 0);
  assert(index < PolyColMax);
  return ((uint8_t const *)poly_colours->flex)[index];
}
