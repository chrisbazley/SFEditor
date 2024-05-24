/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Hill colours
 *  Copyright (C) 2021 Christopher Bazley
 */

/* ANSI headers */
#include <assert.h>
#include "stdlib.h"

#include "flex.h"

#include "Macros.h"
#include "debug.h"
#include "NoBudge.h"
#include "StrDict.h"

#include "DFileData.h"
#include "HillCol.h"
#include "utils.h"

enum {
  PREALLOC_SIZE = 4096,
};

static StrDict file_dict;

struct HillColData {
  DFile dfile;
  void *flex;
};

static SFError hillcol_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  HillColData *const hill_colours = CONTAINER_OF(dfile, HillColData, dfile);
  SFError err = SFERROR(OK);

  nobudge_register(PREALLOC_SIZE);
  if (reader_fread(hill_colours->flex, HillNumColours, 1, reader) != 1)
  {
    err = SFERROR(ReadFail);
  }
  nobudge_deregister();

  return check_trunc_or_ext(reader, err);
}

static void hillcol_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  HillColData *const hill_colours = CONTAINER_OF(dfile, HillColData, dfile);
  flex_free(&hill_colours->flex);
  dfile_destroy(&hill_colours->dfile);
  free(hill_colours);
}

static void hillcol_cleanup(void)
{
  strdict_destroy(&file_dict, NULL, NULL);
}

void hillcol_init(void)
{
  strdict_init(&file_dict);
  atexit(hillcol_cleanup);
}

HillColData *hillcol_create(void)
{
  HillColData *const hill_colours = malloc(sizeof(*hill_colours));
  if (hill_colours)
  {
    if (!flex_alloc(&hill_colours->flex, HillNumColours))
    {
      free(hill_colours);
      return NULL;
    }

    dfile_init(&hill_colours->dfile, hillcol_read_cb, NULL, NULL,
               hillcol_destroy_cb);
  }
  return hill_colours;
}

bool hillcol_share(HillColData *const hill_colours)
{
  assert(hill_colours);
  return dfile_set_shared(&hill_colours->dfile, &file_dict);
}

HillColData *hillcol_get_shared(char const *const filename)
{
  assert(filename);
  DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, HillColData, dfile) : NULL;
}

DFile *hillcol_get_dfile(HillColData *const hill_colours)
{
  assert(hill_colours);
  return &hill_colours->dfile;
}

size_t hillcol_get_colour(HillColData const *const hill_colours, int const index)
{
  assert(hill_colours);
  assert(index >= 0);
  assert(index < HillNumColours);
  return ((unsigned char const *)hill_colours->flex)[index];
}
