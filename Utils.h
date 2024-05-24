/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  General utility functions
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef SFEUtils_h
#define SFEUtils_h

#include <stdio.h>
#include <stdbool.h>

#include "kernel.h"
#include "toolbox.h"
#include "event.h"
#include "flex.h"

#include "PalEntry.h"
#include "StringBuff.h"
#include "loader2.h"
#include "sprformats.h"
#include "sferror.h"
#include "reader.h"
#include "DFile.h"
#include "DataType.h"

int string_lcount(char const *string, int *max_width);

SpriteAreaHeader *get_sprite_area(void);

bool set_saved_with_stamp(DFile *dfile, char const *fname);

void get_scrollbar_sizes(int *width, int *height);

int truncate_string(char *string, int max_width);

bool set_ptr_shape(char const *name, int active_x, int active_y);

void redraw_gadget(ObjectId window, ComponentId gadget);

bool update_menu_tick(IdBlock *id_block);

/* Version of memset() that doesn't need to be called with flex budge disabled */
flex_ptr memset_flex(flex_ptr ptr, int c, size_t n);

/* Version of memcpy() that doesn't need to be called with flex budge disabled */
flex_ptr memcpy_flex(flex_ptr dst, flex_ptr src, size_t n);

ToolboxEventHandler hand_back_caret;

/* Interpret a string of 0s and 1s as binary */
unsigned int read_binary(char const *bin_string);

bool dialogue_confirm(char const *mess, char const *buttons_token);

/* Read line from file, skipping lines beginning with '#' */
char *read_line_comm(char *s, size_t n, FILE *stream, int *line_num);

void open_topleftofwin(unsigned int flags, ObjectId showobj,
  ObjectId relativeto, ObjectId parent, ComponentId parent_component);

char *truncate(char *string, int length);

char real_to_mode13col(PaletteEntry real_col);

PaletteEntry opposite_col(PaletteEntry real_col);

int absdiff(int a, int b);

bool file_exists(char const *filepath);

/* Bring window to front, if already open somewhere */
bool show_win_if_open(ObjectId win);

void *get_ancestor_handle_if_showing(ObjectId self_id);

bool ensure_path_exists(char *file_path);

bool set_data_type(char *file_path, DataType data_type);

void set_button(ObjectId window, ComponentId gadget, bool condition);

bool wipe_menu(ObjectId menu, ComponentId last_entry);

bool verbose_rename(char const */*old_name*/, char */*new_name*/);
bool verbose_remove(char const */*filename*/);
bool verbose_copy(char const */*old_name*/, char */*new_name*/, bool /*move*/);
#if 0
void open_rhsofwin(unsigned int flags, ObjectId showobj,
  ObjectId relativeto, ObjectId parent, ComponentId parent_component);
#endif

void open_file(char const *file_name);
void open_dir(char const *file_name);

void hourglass_and_esc_on(void);
void hourglass_and_esc_off(void);

bool object_is_showing(ObjectId id);

void edit_file(char const *dir, char const *tiles_set);
bool append_to_csv(StringBuffer *csv, char const *value);

char *make_file_path_in_dir_on_path(char const *path, char const *subdir,
  char const *leaf);

char *make_file_path_in_subdir(char const *dir, char const *subdir,
  char const *leaf);

char *make_file_path_in_dir(char const *dir, char const *leaf);

bool report_error(SFError err, char const *load_path, char const *extra);

void load_fail(CONST _kernel_oserror *error, void *client_handle);

SFError check_trunc_or_ext(struct Reader *reader, SFError err);

bool claim_drag(const WimpMessage *message, int const file_types[],
  unsigned int flags, int *my_ref);

#endif
