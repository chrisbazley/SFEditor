/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Global application configuration
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

#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include "stdlib.h"
#include <assert.h>

#include "kernel.h"

#include "View.h"
#include "msgtrans.h"
#include "err.h"
#include "Macros.h"
#include "FileUtils.h"
#include "Debug.h"
#include "StringBuff.h"
#include "SFError.h"
#include "Config.h"
#include "Utils.h"
#include "filescan.h"
#include "filepaths.h"
#include "PalEntry.h"

#define CUSTOMGAMELOC "CustomGameLocation = %d\n"
#define GAMELOCATION "GameLocation = %s\n"
#define EXTERNALDIR "UseExternalDir = %d\n"
#define LEVELSLOCATION "ExternalLocation = %s\n"
#define LAZYDIRSCAN "LazyDirScan = %d\n"
#define DEFAULT_EDIT_MODE "DefaultEditMode = %d\n"
#define DEFAULT_EDIT_TOOL "DefaultEditorTool = %d\n"
#define DEFAULT_SCALE "DefaultScaleLog2 = %d\n"
#define DEFAULT_ANGLE "DefaultOrientation = %d\n"
#define DEFAULT_GRID_COLOUR "DefaultGridColour = %x\n"
#define DEFAULT_BG_COLOUR "DefaultBackgroundColour = %x\n"
#define DEFAULT_SEL_COLOUR "DefaultSelectionColour = %x\n"
#define DEFAULT_GHOST_COLOUR "DefaultGhostColour = %x\n"
#define DEFAULT_SHOW_MAP "DefaultShowMap = %d\n"
#define DEFAULT_SHOW_MAP_OVERLAY "DefaultShowMapOverlay = %d\n"
#define DEFAULT_SHOW_MAP_ANIMS "DefaultShowMapAnims = %d\n"
#define DEFAULT_SHOW_OBJ "DefaultShowObj = %d\n"
#define DEFAULT_SHOW_OBJ_OVERLAY "DefaultShowObjOverlay = %d\n"
#define DEFAULT_SHOW_SHIPS "DefaultShowShips = %d\n"
#define DEFAULT_SHOW_INFO "DefaultShowInfo = %d\n"
#define DEFAULT_SHOW_GRID "DefaultShowGrid = %d\n"
#define DEFAULT_SHOW_NUMBERS "DefaultShowNumbers = %d\n"
#define DEFAULT_SHOW_STATUS_BAR "DefaultShowStatusBar = %d\n"
#define DEFAULT_SHOW_TOOL_BAR "DefaultShowToolBar = %d\n"
#define DEFAULT_SHOW_PALETTE "DefaultShowPalette = %d\n"
#define DEFAULT_ANIMATE_MAP "DefaultAnimateMap = %d\n"
#define TRANSFERSLOCATION "TransfersLocation = %s\n"
#define DEFAULT_FILL_IS_GLOBAL "DefaultFillIsGlobal = %d\n"
#define DEFAULT_BRUSH_SIZE "DefaultBrushSize = %d\n"
#define DEFAULT_WAND_SIZE "DefaultWandSize = %d\n"
#define DEFAULT_PLOT_SHAPE "DefaultPlotShape = %d\n"

#define STARTCONFIGMARK "StartConfig\n"
#define ENDCONFIGMARK "EndConfig\n"

enum {
  LineBufferSize = MaxPathSize +
                   HIGHEST(HIGHEST(sizeof(GAMELOCATION), sizeof(LEVELSLOCATION)),
                           sizeof(TRANSFERSLOCATION)),
  ErrBufferSize = 64,
};

static char const *game_dir; /* points to either <Star3000$Dir> or explicit path */

typedef struct {
  char custom_game_dir[MaxPathSize];
  char extern_levels_dir[MaxPathSize];
  char transfers_dir[MaxPathSize];
  bool use_custom_game_dir:1;
  bool use_extern_levels_dir:1; /* should we have one? */
  bool lazydirscan:1;           /* only rescan missions/maps when changed */
  bool default_animate_enabled:1;
  bool default_tool_bar_enabled:1;
  bool default_status_bar_enabled:1;
  bool default_palette_enabled:1;
  bool default_fill_is_global:1;
  EditMode default_edit_mode;
  EditorTool default_edit_tool;
  int default_brush_size;
  int default_wand_size;
  PlotShape default_plot_shape;
  ViewConfig default_view;
} Config;

static Config config;

/* ---------------- Private functions ---------------- */

static bool interpret_line(char const *const line)
{
  assert(line != NULL);

  int input;
  PaletteEntry colour;
  int num_inputs = sscanf(line, CUSTOMGAMELOC, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_use_custom_game_dir(input);
    return true;
  }

  char string[LineBufferSize];
  num_inputs = sscanf(line, GAMELOCATION, string);
  if (num_inputs == 1) {
    if (strlen(string) >= MaxPathSize) {
      DEBUGF("String too long (%s)\n", string);
      return false;
    }
    Config_set_custom_game_dir(string);
    return true;
  }

  num_inputs = sscanf(line, TRANSFERSLOCATION, string);
  if (num_inputs == 1) {
    if (strlen(string) >= MaxPathSize) {
      DEBUGF("String too long (%s)\n", string);
      return false;
    }
    Config_set_transfers_dir(string);
    return true;
  }

  num_inputs = sscanf(line, EXTERNALDIR, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_use_extern_levels_dir(input);
    return true;
  }

  num_inputs = sscanf(line, LEVELSLOCATION, string);
  if (num_inputs == 1) {
    if (strlen(string) >= MaxPathSize) {
      DEBUGF("String too long (%s)\n", string);
      return false;
    }
    Config_set_extern_levels_dir(string);
    return true;
  }

  num_inputs = sscanf(line, LAZYDIRSCAN, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_lazydirscan(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_EDIT_MODE, &input);
  if (num_inputs == 1) {
    if (input < EDITING_MODE_FIRST || input >= EDITING_MODE_COUNT) {
      DEBUGF("Bad editing mode (%d)\n", input);
      return false;
    }
    Config_set_default_edit_mode(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_EDIT_TOOL, &input);
  if (num_inputs == 1) {
    if (input < EDITORTOOL_FIRST || input >= EDITORTOOL_COUNT) {
      DEBUGF("Bad editor tool (%d)\n", input);
      return false;
    }
    Config_set_default_edit_tool(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SCALE, &input);
  if (num_inputs == 1) {
    if (input < EditWinZoomMin || input > EditWinZoomMax) {
      DEBUGF("Bad log2 scale (%d)\n", input);
      return false;
    }
    config.default_view.zoom_factor = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_ANGLE, &input);
  if (num_inputs == 1) {
    if (input < MapAngle_North || input > MapAngle_West) {
      DEBUGF("Bad view angle (%d)\n", input);
      return false;
    }
    config.default_view.angle = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_GRID_COLOUR, &colour);
  if (num_inputs == 1) {
    if (colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) {
      DEBUGF("Bad colour (0x%x)\n", colour);
      return false;
    }
    config.default_view.grid_colour = colour;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_BG_COLOUR, &colour);
  if (num_inputs == 1) {
    if (colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) {
      DEBUGF("Bad colour (0x%x)\n", colour);
      return false;
    }
    config.default_view.back_colour = colour;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SEL_COLOUR, &colour);
  if (num_inputs == 1) {
    if (colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) {
      DEBUGF("Bad colour (0x%x)\n", colour);
      return false;
    }
    config.default_view.sel_colour = colour;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_GHOST_COLOUR, &colour);
  if (num_inputs == 1) {
    if (colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) {
      DEBUGF("Bad colour (0x%x)\n", colour);
      return false;
    }
    config.default_view.ghost_colour = colour;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_MAP, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.MAP = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_MAP_OVERLAY, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.MAP_OVERLAY = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_MAP_ANIMS, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.MAP_ANIMS = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_OBJ, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.OBJECTS = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_OBJ_OVERLAY, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.OBJECTS_OVERLAY = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_SHIPS, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.SHIPS = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_INFO, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.INFO = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_GRID, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.GRID = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_NUMBERS, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.flags.NUMBERS = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_STATUS_BAR, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    config.default_view.show_status_bar = input;
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_TOOL_BAR, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_default_tool_bar_enabled(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_SHOW_PALETTE, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_default_palette_enabled(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_ANIMATE_MAP, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_default_animate_enabled(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_FILL_IS_GLOBAL, &input);
  if (num_inputs == 1) {
    if (input != 0 && input != 1) {
      DEBUGF("Bad Boolean value (%d)\n", input);
      return false;
    }
    Config_set_default_fill_is_global(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_BRUSH_SIZE, &input);
  if (num_inputs == 1) {
    if (input < 0) {
      DEBUGF("Bad brush size (%d)\n", input);
      return false;
    }
    Config_set_default_brush_size(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_WAND_SIZE, &input);
  if (num_inputs == 1) {
    if (input < 0) {
      DEBUGF("Bad wand size (%d)\n", input);
      return false;
    }
    Config_set_default_wand_size(input);
    return true;
  }

  num_inputs = sscanf(line, DEFAULT_PLOT_SHAPE, &input);
  if (num_inputs == 1) {
    if (input < PLOTSHAPE_FIRST || input >= PLOTSHAPE_COUNT) {
      DEBUGF("Bad plot shape (%d)\n", input);
      return false;
    }
    Config_set_default_plot_shape(input);
    return true;
  }

  DEBUGF("Unrecognized config line (%s)\n", line);
  return false; /* what-a-load-of-crap! */
}

static SFError read_from_file(FILE *const file, char *const err_buf)
{
  assert(file != NULL);
  assert(!ferror(file));
  assert(err_buf != NULL);

  *err_buf = '\0';

  int line = 0;
  bool block = false;
  char read_line[LineBufferSize];
  while (read_line_comm(read_line, sizeof(read_line), file, &line) != NULL)
  {
    if (strcmp(read_line, STARTCONFIGMARK) == 0) {
      if (block) {
        /* syntax error - already in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }
      block = true;
      continue;
    }

    if (strcmp(read_line, ENDCONFIGMARK) == 0) {
      if (!block) {
        /* syntax error - not in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }
      block = false;
      continue;
    }

    if (!block) {
      /* Unknown non-comment text outside block */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }

    if (!interpret_line(read_line)) {
      /* Report syntax error and line number */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }
  } /* endwhile */

  if (block) {
    /* syntax error - no ENDCONFIGMARK before EOF */
    strcpy(err_buf, ENDCONFIGMARK);
    return SFERROR(EOF);
  }

  return SFERROR(OK);
}

static bool loadfile(char const *const configfile)
{
  assert(configfile != NULL);
  SFError err = SFERROR(OK);
  char err_buf[ErrBufferSize] = "";
  FILE * const file = fopen(configfile, "r");
  if (file == NULL) {
    err = SFERROR(OpenInFail);
  } else {
    err = read_from_file(file, err_buf);
    fclose(file);
  }

  return !report_error(err, configfile, err_buf);
}

static bool write_to_file(FILE *const file)
{
  assert(file != NULL);
  assert(!ferror(file));

  int num_outputs = fprintf(file, "# %s\n", msgs_lookup("ConfigHeader"));
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, STARTCONFIGMARK);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, CUSTOMGAMELOC, config.use_custom_game_dir);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, GAMELOCATION, config.custom_game_dir);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, TRANSFERSLOCATION, config.transfers_dir);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, EXTERNALDIR, config.use_extern_levels_dir);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, LEVELSLOCATION, config.extern_levels_dir);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, LAZYDIRSCAN, config.lazydirscan);
  if (num_outputs < 0)
    return false;

  num_outputs = fprintf(file, DEFAULT_EDIT_MODE, config.default_edit_mode);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_EDIT_TOOL, config.default_edit_tool);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SCALE, config.default_view.zoom_factor);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_ANGLE, config.default_view.angle);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_GRID_COLOUR, config.default_view.grid_colour);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_BG_COLOUR, config.default_view.back_colour);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SEL_COLOUR, config.default_view.sel_colour);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_GHOST_COLOUR, config.default_view.ghost_colour);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_MAP, config.default_view.flags.MAP);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_MAP_OVERLAY, config.default_view.flags.MAP_OVERLAY);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_MAP_ANIMS, config.default_view.flags.MAP_ANIMS);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_OBJ, config.default_view.flags.OBJECTS);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_OBJ_OVERLAY, config.default_view.flags.OBJECTS_OVERLAY);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_SHIPS, config.default_view.flags.SHIPS);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_INFO, config.default_view.flags.INFO);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_GRID, config.default_view.flags.GRID);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_NUMBERS, config.default_view.flags.NUMBERS);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_STATUS_BAR, config.default_view.show_status_bar);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_TOOL_BAR, config.default_tool_bar_enabled);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_SHOW_PALETTE, config.default_palette_enabled);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_ANIMATE_MAP, config.default_animate_enabled);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_FILL_IS_GLOBAL, config.default_fill_is_global);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_BRUSH_SIZE, config.default_brush_size);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_WAND_SIZE, config.default_wand_size);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, DEFAULT_PLOT_SHAPE, config.default_plot_shape);
  if (num_outputs < 0) {
    return false;
  }

  num_outputs = fprintf(file, ENDCONFIGMARK);
  if (num_outputs < 0)
    return false;

  return true;
}

static bool savefile(char const *const configfile)
{
  assert(configfile != NULL);
  SFError err = SFERROR(OK);
  FILE *const file = fopen(configfile, "w");
  if (file == NULL) {
    err = SFERROR(OpenOutFail);
  } else {
    bool const success = write_to_file(file);
    if (fclose(file) || !success)
    {
      err = SFERROR(WriteFail);
    }
  }
  return !report_error(err, configfile, "");
}

/* ---------------- Public functions ---------------- */

void Config_init(void)
{
  /* To be called at program start */

  /* We canonicalise the path so that any error messages are meaningful */
  {
    char *config_read_file;
    EF(canonicalise(&config_read_file, NULL, NULL, CHOICES_READ_PATH CONFIG_FILE));

    if (!loadfile(config_read_file) || !Config_setup_levels_path()) {
      exit(EXIT_FAILURE);
    }
    free(config_read_file);
  }

  /* Check that levels directory paths are still valid */
  if (config.use_extern_levels_dir && !file_exists(config.extern_levels_dir)) {
    /* External levels directory not found */
    WARN("ExternNotFoundLoad");
    config.use_extern_levels_dir = false; /* Configure it off */
    if (!Config_setup_levels_path()) /* update SFeditorLevels$Path */
      exit(EXIT_FAILURE);
  }
  if (!file_exists(game_dir)) {
    /* Main game directory not found */
    WARN("GameNotFoundLoad");
    game_dir = NULL;
  }

  if (!file_exists(config.transfers_dir)) {
    /* Transfers directory not found */
    WARN("TransfersNotFoundLoad");
  }
}

char const *Config_get_write_dir(void)
{
  char const *const write_dir = config.use_extern_levels_dir ?
                                  config.extern_levels_dir : game_dir;
  DEBUG("Write directory is '%s'", write_dir);
  return write_dir;
}

char const *Config_get_read_dir(void)
{
  return game_dir;
}

void Config_save(void)
{
  /* Success - save configuration to file and close window */
  char *config_write_file;
  if (!E(canonicalise(&config_write_file, NULL, NULL, CHOICES_WRITE_PATH CONFIG_FILE))) {
    /* (we canonicalise the path so that any error messages are meaningful) */
    if (ensure_path_exists(config_write_file)) {
      savefile(config_write_file);
    }

    free(config_write_file);
  }
}

bool Config_setup_levels_path(void)
{
  bool success = true;
  char const *const gd = config.use_custom_game_dir ? config.custom_game_dir : FIXED_GAME_DIR;

  /* Set up path for reading files */
  StringBuffer levels_path;
  stringbuffer_init(&levels_path);

  if (config.use_extern_levels_dir) {
    if (!stringbuffer_append_all(&levels_path, config.extern_levels_dir) ||
        !stringbuffer_append_all(&levels_path, ".,")) {
      report_error(SFERROR(NoMem), "", "");
      success = false;
    }
  }

  if (!stringbuffer_append_all(&levels_path, gd) ||
      !stringbuffer_append_all(&levels_path, ".")) {
    report_error(SFERROR(NoMem), "", "");
    success = false;
  }

  if (success && E(_kernel_setenv("SFeditorLevels$Path",
                                  stringbuffer_get_pointer(&levels_path)))) {
    success = false;
  }

  if (success) {
    /* Set pointer to game levels directory */
    game_dir = gd;

    /* Must rescan everything, as paths may have changed */
    for (int d = 0; d < FS_LAST; d++) {
      filescan_directory_updated((filescan_type)d);
    }
  }

  stringbuffer_destroy(&levels_path);
  return success;
}

char const *Config_get_custom_game_dir(void)
{
  return config.custom_game_dir;
}

char const *Config_get_extern_levels_dir(void)
{
  return config.extern_levels_dir;
}

char const *Config_get_transfers_dir(void)
{
  return config.transfers_dir;
}

bool Config_get_use_custom_game_dir(void)
{
  return config.use_custom_game_dir;
}

bool Config_get_use_extern_levels_dir(void)
{
  return config.use_extern_levels_dir;
}

bool Config_get_lazydirscan(void)
{
  return config.lazydirscan;
}

bool Config_get_default_animate_enabled(void)
{
  return config.default_animate_enabled;
}

bool Config_get_default_tool_bar_enabled(void)
{
  return config.default_tool_bar_enabled;
}

bool Config_get_default_palette_enabled(void)
{
  return config.default_palette_enabled;
}

EditMode Config_get_default_edit_mode(void)
{
  return config.default_edit_mode;
}

EditorTool Config_get_default_edit_tool(void)
{
  return config.default_edit_tool;
}

bool Config_get_default_fill_is_global(void)
{
  return config.default_fill_is_global;
}

PlotShape Config_get_default_plot_shape(void)
{
  return config.default_plot_shape;
}

int Config_get_default_brush_size(void)
{
  return config.default_brush_size;
}

int Config_get_default_wand_size(void)
{
  return config.default_wand_size;
}

void Config_set_custom_game_dir(char const *const path)
{
  assert(path);
  assert(strlen(path) < MaxPathSize);
  *config.custom_game_dir = '\0';
  strncat(config.custom_game_dir, path, MaxPathSize-1);
}

void Config_set_extern_levels_dir(char const *const path)
{
  assert(path);
  assert(strlen(path) < MaxPathSize);
  *config.extern_levels_dir = '\0';
  strncat(config.extern_levels_dir, path, MaxPathSize-1);
}

void Config_set_transfers_dir(char const *const path)
{
  assert(path);
  assert(strlen(path) < MaxPathSize);
  *config.transfers_dir = '\0';
  strncat(config.transfers_dir, path, MaxPathSize-1);
}

void Config_set_use_custom_game_dir(bool const enable)
{
  config.use_custom_game_dir = enable;
}

void Config_set_use_extern_levels_dir(bool const enable)
{
  config.use_extern_levels_dir = enable;
}

void Config_set_lazydirscan(bool const enable)
{
  config.lazydirscan = enable;
}

void Config_set_default_animate_enabled(bool const enable)
{
  config.default_animate_enabled = enable;
}

void Config_set_default_tool_bar_enabled(bool const enable)
{
  config.default_tool_bar_enabled = enable;
}

void Config_set_default_palette_enabled(bool const enable)
{
  config.default_palette_enabled = enable;
}

void Config_set_default_edit_mode(EditMode const mode)
{
  assert(mode >= EDITING_MODE_FIRST);
  assert(mode < EDITING_MODE_COUNT);
  config.default_edit_mode = mode;
}

void Config_set_default_edit_tool(EditorTool const tool)
{
  assert(tool >= EDITORTOOL_FIRST);
  assert(tool < EDITORTOOL_COUNT);
  config.default_edit_tool = tool;
}

void Config_set_default_fill_is_global(bool const is_global)
{
  config.default_fill_is_global = is_global;
}

void Config_set_default_plot_shape(PlotShape const shape)
{
  assert(shape >= PLOTSHAPE_FIRST);
  assert(shape < PLOTSHAPE_COUNT);
  config.default_plot_shape = shape;
}

void Config_set_default_brush_size(int const size)
{
  assert(size >= 0);
  config.default_brush_size = size;
}

void Config_set_default_wand_size(int const size)
{
  assert(size >= 0);
  config.default_wand_size = size;
}

ViewConfig const *Config_get_default_view(void)
{
  return &config.default_view;
}

void Config_set_default_view(ViewConfig const *const view)
{
  assert(view->zoom_factor >= EditWinZoomMin);
  assert(view->zoom_factor <= EditWinZoomMax);
  assert(view->angle >= MapAngle_First);
  assert(view->angle < MapAngle_Count);
  assert((view->grid_colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) == 0);
  assert((view->back_colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) == 0);
  assert((view->ghost_colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) == 0);
  assert((view->sel_colour & ~(PaletteEntry_RedMask|PaletteEntry_GreenMask|PaletteEntry_BlueMask)) == 0);
  config.default_view = *view;
}
