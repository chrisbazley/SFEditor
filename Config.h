/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Global application configuration
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef Config_h
#define Config_h

#include <stdbool.h>

#include "EditWin.h"
#include "PalEntry.h"
#include "MapMode.h"
#include "Editor.h"

enum {
  MaxPathSize = 256,
};

struct ViewConfig;

void Config_init(void);
char const *Config_get_write_dir(void);
char const *Config_get_read_dir(void);
void Config_save(void);

char const *Config_get_custom_game_dir(void);
char const *Config_get_extern_levels_dir(void);
char const *Config_get_transfers_dir(void);
bool Config_get_use_custom_game_dir(void);
bool Config_get_use_extern_levels_dir(void); /* should we have one? */
bool Config_get_lazydirscan(void);           /* only rescan missions/maps when changed */
bool Config_get_default_animate_enabled(void);
bool Config_get_default_tool_bar_enabled(void);
bool Config_get_default_palette_enabled(void);
EditMode Config_get_default_edit_mode(void);
EditorTool Config_get_default_edit_tool(void);
bool Config_get_default_fill_is_global(void);
PlotShape Config_get_default_plot_shape(void);
int Config_get_default_brush_size(void);
int Config_get_default_wand_size(void);
struct ViewConfig const *Config_get_default_view(void);

void Config_set_custom_game_dir(char const *path);
void Config_set_extern_levels_dir(char const *path);
void Config_set_transfers_dir(char const *path);
void Config_set_use_custom_game_dir(bool enable);
void Config_set_use_extern_levels_dir(bool enable);
void Config_set_lazydirscan(bool enable);
bool Config_setup_levels_path(void);
void Config_set_default_animate_enabled(bool enable);
void Config_set_default_tool_bar_enabled(bool enable);
void Config_set_default_palette_enabled(bool enable);
void Config_set_default_edit_mode(EditMode mode);
void Config_set_default_edit_tool(EditorTool tool);
void Config_set_default_fill_is_global(bool is_global);
void Config_set_default_plot_shape(PlotShape shape);
void Config_set_default_brush_size(int size);
void Config_set_default_wand_size(int size);
void Config_set_default_view(struct ViewConfig const *view);

#endif
