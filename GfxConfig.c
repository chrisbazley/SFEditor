/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Graphics files configuration
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

#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <stdbool.h>

#include "SFInit.h"
#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "Debug.h"
#include "FileUtils.h"
#include "StrExtra.h"
#include "SFError.h"
#include "utils.h"
#include "filepaths.h"
#include "GfxConfig.h"
#include "filenames.h"
#include "clouds.h"
#include "DataType.h"

static char const GF_STARTBASEGFXMARK[] = "StartBaseGfx\n";
static char const GF_ENDBASEGFXMARK[] = "EndBaseGfx\n";

static const struct {
  char const *name;
  DataType data_type;
} map[] = {
  { "MapTilesSet", DataType_MapTextures },
  { "PolyGfxSet", DataType_PolygonMeshes },
  { "Palette", DataType_PolygonColours },
  { "HillColours", DataType_HillColours },
};

static char const *colour_tags[Clouds_NumColours] = {
  "CloudColour1",
  "CloudColour2",
};

enum {
  LineBufferSize = 256,
  TagBufferSize = 16,
  ValueBufferSize = 256,
  ErrBufferSize = 64,
};

/* ---------------- Private functions ---------------- */

static bool interpret_line(GfxConfig *const graphics, char const *const line)
{
  assert(graphics != NULL);
  assert(line != NULL);

  char tag[TagBufferSize];
  char value[ValueBufferSize];
  int num_inputs = sscanf(line, "%15s = %255s\n", tag, value);
  if (num_inputs == 2)
  {
    for (size_t i = 0; i < ARRAY_SIZE(map); ++i)
    {
      if (!stricmp(tag, map[i].name))
      {
        filenames_set(&graphics->filenames, map[i].data_type, value);
        return true;
      }
    }

    for (size_t i = 0; i < Clouds_NumColours; ++i)
    {
      if (!stricmp(tag, colour_tags[i]))
      {
        unsigned long num = strtoul(value, NULL, 0);
        if (num >= NumColours)
        {
          return false;
        }
        clouds_set_colour(&graphics->clouds, i, (unsigned)num);
        return true;
      }
    }
  }

  return false; /* wha-loa-o-crap! */
}

static SFError read_from_file(FILE *const handle, GfxConfig *const graphics,
  char *const err_buf)
{
  assert(handle != NULL);
  assert(!ferror(handle));
  assert(graphics != NULL);
  assert(err_buf != NULL);

  *err_buf = '\0';

  int line = 0;
  bool block = false;
  char read_line[LineBufferSize];

  while (read_line_comm(read_line, sizeof(read_line), handle, &line) != NULL)
  {
    if (strcmp(read_line, GF_STARTBASEGFXMARK) == 0) {
      if (block) {
        /* syntax error - already in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }
      block = true;
      continue;
    }

    if (strcmp(read_line, GF_ENDBASEGFXMARK) == 0) {
      if (!block) {
        /* syntax error - not in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }
      block = false;
      continue;
    }

    if (block) {
      if (!interpret_line(graphics, read_line)) {
        /* Report syntax error and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(Mistake);
      }
    } else {
      /* Unknown non-comment text outside block */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }
  } /* endwhile */

  if (block) {
    /* syntax error - no GF_ENDBASEGFXMARK before EOF */
    strcpy(err_buf, GF_ENDBASEGFXMARK);
    return SFERROR(EOF);
  }

  return SFERROR(OK);
}

static bool write_to_file(FILE *const handle, const GfxConfig *const graphics)
{
  assert(handle != NULL);
  assert(!ferror(handle));
  assert(graphics != NULL);

  int num_outputs = fprintf(handle, "# %s\n", msgs_lookup("GfxPrefsHeader"));
  if (num_outputs < 0)
  {
    return false;
  }

  num_outputs = fprintf(handle, GF_STARTBASEGFXMARK);
  if (num_outputs < 0)
  {
    return false;
  }

  for (size_t i = 0; i < ARRAY_SIZE(map); ++i)
  {
    num_outputs = fprintf(handle, "%s = %s\n", map[i].name,
                          filenames_get(&graphics->filenames, map[i].data_type));
    if (num_outputs < 0)
    {
      return false;
    }
  }

  for (size_t i = 0; i < Clouds_NumColours; ++i)
  {
    num_outputs = fprintf(handle, "%s = %u\n", colour_tags[i],
                          clouds_get_colour(&graphics->clouds, i));
    if (num_outputs < 0)
    {
      return false;
    }
  }

  num_outputs = fprintf(handle, GF_ENDBASEGFXMARK);
  if (num_outputs < 0)
  {
    return false;
  }

  return true;
}

/* ---------------- Public functions ---------------- */

bool GfxConfig_load(GfxConfig *const graphics, char const *const basemap_filename)
{
  assert(graphics != NULL);
  assert(basemap_filename != NULL);

  char *path = make_file_path_in_dir(
                 CHOICES_READ_PATH MAPGFX_DIR, basemap_filename);

  if (path && !file_exists(path)) {
    /* Map unknown - fall back on default settings */
    free(path);
    path = make_file_path_in_dir(CHOICES_READ_PATH MAPGFX_DIR, UNKNOWN_FILE);
  }

  if (!path) {
    return false;
  }

  static DataType const data_types[] = {
    DataType_BaseMap, DataType_BaseObjects, DataType_BaseMapAnimations
  };
  for (size_t i = 0; i < ARRAY_SIZE(data_types); ++i)
  {
    filenames_set(&graphics->filenames, data_types[i], basemap_filename);
  }

  char err_buf[ErrBufferSize] = "";
  SFError err = SFERROR(OK);

  FILE *const file = fopen(path, "r");
  if (file == NULL) {
    err = SFERROR(OpenInFail);
  } else {
    err = read_from_file(file, graphics, err_buf);
    fclose(file);
  }

  bool const ok = !report_error(err, path, err_buf);
  free(path);
  return ok;
}

bool GfxConfig_save(const GfxConfig *const graphics, char const *const basemap_filename)
{
  assert(graphics != NULL);
  assert(basemap_filename != NULL);

  char *const full_path = make_file_path_in_dir(CHOICES_WRITE_PATH MAPGFX_DIR,
    basemap_filename);

  if (!full_path || !ensure_path_exists(full_path)) {
    return false;
  }

  SFError err = SFERROR(OK);
  FILE *const file = fopen(full_path, "w");
  if (file == NULL) {
    err = SFERROR(OpenOutFail);
  } else {
    bool success = write_to_file(file, graphics);
    if (fclose(file))
    {
      success = false;
    }
    if (!success)
    {
      err = SFERROR(WriteFail);
    }
  }

  bool const ok = !report_error(err, full_path, "");
  free(full_path);
  return ok;
}
