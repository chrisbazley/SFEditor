/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Palette window
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef Palette_h
#define Palette_h

#include <stdbool.h>
#include "Scheduler.h"

#include "toolbox.h"
#include "Vertex.h"

#define NULL_DATA_INDEX SIZE_MAX

typedef struct PaletteData PaletteData;
typedef struct PaletteClientFuncts PaletteClientFuncts;
struct Editor;

typedef bool PaletteInitialiseFunction(PaletteData *pal_data, struct Editor *editor, size_t *num_indices, bool reinit);
typedef void PaletteFinaliseFunction(PaletteData *pal_data, struct Editor *editor, bool reinit);
typedef void PaletteDragStartFunction(struct Editor *editor);
typedef void PaletteRedrawStartFunction(struct Editor *editor, bool labels);
typedef void PaletteRedrawObjectFunction(struct Editor *editor, Vertex origin, BBox const *bbox, size_t object_no, bool selected);
typedef void PaletteRedrawLabelFunction(struct Editor *editor, Vertex origin, BBox const *bbox, size_t object_no, bool selected);
typedef void PaletteRedrawEndFunction(struct Editor *editor, bool labels);
typedef size_t PaletteGridToIndex(struct Editor *editor, Vertex grid_pos, size_t num_columns);
typedef Vertex PaletteIndexToGrid(struct Editor *editor, size_t index, size_t num_columns);
typedef size_t PaletteGetMaxColumns(struct Editor *editor);
typedef size_t PaletteGetNumRows(struct Editor *editor, size_t num_columns);
typedef void PaletteReloadFunction(struct Editor *editor);
typedef void PaletteEditFunction(struct Editor *editor);
typedef void PaletteDeleteAllFunction(struct Editor *editor);
typedef void PaletteDeleteFunction(struct Editor *editor, size_t index);
typedef size_t PaletteIndexToObject(struct Editor *editor, size_t index);
typedef size_t PaletteObjectToIndex(struct Editor *editor, size_t object_no);
typedef SchedulerTime PaletteAnimateFunction(struct Editor *editor, SchedulerTime time_no);

typedef void PaletteUpdateMenusFunction(PaletteData *pal_data);

struct PaletteClientFuncts {
  Vertex object_size;
  char const *title_msg;
  bool selected_has_border:1;
  bool overlay_labels:1;
  bool menu_selects:1;
  unsigned char default_columns;
  PaletteInitialiseFunction *initialise;
  PaletteDragStartFunction *drag_start;
  PaletteRedrawStartFunction *start_redraw;
  PaletteRedrawObjectFunction *redraw_object;
  PaletteRedrawLabelFunction *redraw_label;
  PaletteRedrawEndFunction *end_redraw;
  PaletteFinaliseFunction *finalise;
  PaletteGridToIndex *grid_to_index;
  PaletteIndexToGrid *index_to_grid;
  PaletteGetMaxColumns *get_max_columns;
  PaletteGetNumRows *get_num_rows;
  PaletteReloadFunction *reload;
  PaletteEditFunction *edit;
  PaletteDeleteAllFunction *delete_all;
  PaletteDeleteFunction *delete;
  PaletteIndexToObject *index_to_object;
  PaletteObjectToIndex *object_to_index;
  PaletteUpdateMenusFunction *update_menus;
  PaletteAnimateFunction *animate;
};

struct EditWin;
struct Editor;

bool Palette_register_client(PaletteData *pal_data, const PaletteClientFuncts *client_functions);

bool Palette_init(PaletteData *pal_data, struct Editor *parent_editor);

void Palette_show(PaletteData *pal_data, struct EditWin *edit_win);
void Palette_reveal(PaletteData *pal_data);
void Palette_hide(PaletteData *pal_data);

void Palette_set_menu(PaletteData *pal_data, ObjectId menu_id);
void Palette_update_title(PaletteData *pal_data);
void Palette_destroy(PaletteData *pal_data);

void Palette_reinit(PaletteData *pal_data);

struct EditSession;
struct EditSession *Palette_get_session(PaletteData const *pal_data);

bool Palette_is_showing(PaletteData const *pal_data);
size_t Palette_get_selection(PaletteData const *pal_data);
void Palette_set_selection(PaletteData *pal_data, size_t object);
bool Palette_get_ordered_flag(PaletteData const *pal_data);
bool Palette_get_labels_flag(PaletteData const *pal_data);
void Palette_object_added(PaletteData *pal_data, size_t object);
void Palette_redraw_object(PaletteData *pal_data, size_t object);
void Palette_redraw_name(PaletteData *pal_data, size_t object);
void Palette_object_moved(PaletteData *pal_data, size_t old_object, size_t new_object);
void Palette_object_deleted(PaletteData *pal_data, size_t object);

#endif
