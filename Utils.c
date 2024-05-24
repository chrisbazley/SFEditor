/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  General utility functions
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

/* Relax... Don't do it! */
#include "stdlib.h"
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "stdio.h"
#include <stdbool.h>
#include <inttypes.h>

#include "kernel.h"
#include "swis.h"
#include "toolbox.h"
#include "event.h"
#include "window.h"
#include "wimp.h"
#include "wimplib.h"
#include "flex.h"
#include "menu.h"

#include "Platform.h"
#include "plot.h"
#include "PalEntry.h"
#include "StringBuff.h"
#include "OSSpriteOp.h"
#include "OSFSCntrl.h"
#include "msgtrans.h"
#include "err.h"
#include "hourglass.h"
#include "Macros.h"
#include "debug.h"
#include "DeIconise.h"
#include "FileUtils.h"
#include "nobudge.h"
#include "pathtail.h"
#include "strextra.h"
#include "sprformats.h"
#include "FilePaths.h"
#include "WimpExtra.h"
#include "Datestamp.h"

#include "Utils.h"
#include "SFinit.h"
#include "Session.h"
#include "DFile.h"

int string_lcount(char const *const string, int *const max_width)
{
  assert(string);
  assert(max_width);

  int line_count = 0;
  char const *start = string;
  bool finished = false;
  do {
    char const *end = strchr(start, '\n');
    if (end == NULL) {
      end = start + strlen(start);
      finished = true;
    }
    *max_width = HIGHEST(end - start, *max_width);
    line_count++;
    start = end + 1;
  } while (!finished);

  return line_count;
}

static void exec_cmd(char const *command, char const *file_name);

SpriteAreaHeader *get_sprite_area(void)
{
  static SpriteAreaHeader *sprite_area;
  if (!sprite_area)
  {
    _kernel_swi_regs regs;
    if (!E(toolbox_get_sys_info(Toolbox_GetSysInfo_SpriteArea, &regs))) {
      sprite_area = (SpriteAreaHeader *)regs.r[0];
    }
  }
  return sprite_area;
}

bool set_saved_with_stamp(DFile *const dfile, char const *const fname)
{
  assert(dfile);
  assert(fname);

  OS_DateAndTime date_stamp;
  if (E(get_date_stamp(fname, &date_stamp)))
  {
    return false;
  }

  int tmp[2] = {0};
  memcpy(tmp, &date_stamp, sizeof(date_stamp));
  dfile_set_saved(dfile, fname, tmp);
  return true;
}

void get_scrollbar_sizes(int *const width, int *const height)
{
  int sbar_width = 40, sbar_height = 40; /* standard size */

  if (wimp_version >= 400) {
    /* Use new Wimp_Extend reason code (see specification of Ursula Wimp) */
    int32_t info_block[25]; /* block must be 100 bytes */
    info_block[0] = 0; /* return generic values (no window handle) */

    if (!E(_swix(Wimp_Extend, _INR(0,1), 11, info_block))) {
      DEBUG("Wimp_Extend reports right border %"PRId32", bottom border %"PRId32,
            info_block[3], info_block[2]);
      sbar_width = info_block[3]; /* right border */
      sbar_height = info_block[2]; /* bottom border */
    }
  }

  if (width) {
    *width = sbar_width;
  }

  if (height) {
    *height = sbar_height;
  }
}

bool set_ptr_shape(char const *name, int active_x, int active_y)
{
  /* only allowed to use ptr 2 in the wimp? */
  return !E(os_sprite_op_set_pointer(tb_sprite_area, name, 2,
               active_x, active_y, NULL, NULL));
}

void redraw_gadget(ObjectId const window, ComponentId const gadget)
{
  DEBUG("Forcing gadget %d in window %d to be redrawn", gadget, window);

  BBox gadget_bbox;
  if (!E(gadget_get_bbox(0, window, gadget, &gadget_bbox))) {
    E(window_force_redraw(0, window, &gadget_bbox));
  }
}

flex_ptr memset_flex(flex_ptr ptr, int c, size_t n)
{
  /* Version of memset() that doesn't need to be called with flex budge
  disabled */
  assert(flex_size(ptr) > 0); /* also validates anchor */
  c = (char)c;

  if (n % 4) {
    /* Must write one byte at a time */
    char *write_ptr = (char *)*ptr; /* careful - flex block must stay put */
    size_t i = 0;
    while (i < n) { /* unrolled loop (8 writes per iteration) */
      write_ptr[i] = c;
      if (i++ >= n)
        break;

      write_ptr[i] = c;
      if (i++ >= n)
        break;

      write_ptr[i] = c;
      if (i++ >= n)
        break;

      write_ptr[i++] = c;
      if (i++ >= n)
        break;

      write_ptr[i] = c;
      if (i++ >= n)
        break;

      write_ptr[i] = c;
      if (i++ >= n)
        break;

      write_ptr[i] = c;
      if (i++ >= n)
        break;

      write_ptr[i++] = c;
    }
  } else {
    /* Optimisation - can write words instead of bytes */
    long *write_ptr = (long int *)*ptr; /* careful - flex block must stay put */
    long int wide = (long)c | (long)c<<8 | (long)c<<16 | (long)c<<24;
    size_t i = 0;
    while (i < n/sizeof(*write_ptr)) {
      /* unrolled loop (8 writes per iteration) */
      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i++] = wide;
    }
  }

  return ptr;
}

flex_ptr memcpy_flex(flex_ptr dst, flex_ptr src, size_t n)
{
  /* Version of memcpy() that doesn't need to be called with flex budge
  disabled */
  assert(flex_size(dst) > 0); /* also validates anchor */
  assert(flex_size(src) > 0);

  if (n % 4) {
    /* Must write one byte at a time */
    char *write_ptr = (char *)*dst; /* careful - flex block must stay put */
    char const *read_ptr = (char *)*src;
    size_t i = 0;
    while (i < n) { /* unrolled loop (8 writes per iteration) */
      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n)
        break;

      write_ptr[i] = read_ptr[i];
      i++;
    }
  } else {
    /* Optimisation - can write words instead of bytes */
    long *write_ptr = (long int *)*dst; /* careful - flex block must stay put */
    long const *read_ptr = (long int *)*src;
    size_t i = 0;
    while (i < n/sizeof(*write_ptr)) {
      /* unrolled loop (8 writes per iteration) */
      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      if (i++ >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = read_ptr[i];
      i++;
    }
  }

  return dst;
}

char *read_line_comm(char *const s, size_t const n, FILE *const stream,
                     int *const line_num)
{
  /* Read string from file into buffer s of length n,
     ignoring comments and blank lines.
     Returns s if successful, or NULL for read error/EOF */
  assert(s);
  assert(stream);
  assert(n <= INT_MAX);
  char *err;

  do {
    /* Read line */
    if (line_num != NULL)
      (*line_num)++;
    err = fgets(s, (int)n, stream);
  /* skip comments and blank lines */
  } while (err != NULL && (strncmp(s, "#", 1) == 0 || strncmp(s, "\n", 1) == 0));

  /* strip trailing spaces and add newline incase necessary */
  {
    size_t c = strlen(s); /* zero terminator of s */

    while (isspace(s[c-1]))
      c--; /* work backwards from end of string */
    /* c is final white-space character of s (if any), or the terminator */
    if (c+1 < n) {
      s[c] = '\n';
      s[c+1] = 0;
    }
  }
  return err;
}

unsigned int read_binary(char const *bin_string)
{
  unsigned int ch = 0, result = 0;

  while (bin_string[ch] != 0) {
    if (bin_string[ch] == '1')
      result |= 1u << ch;
    ch++;
  }
  return result;
}

bool object_is_showing(ObjectId const id)
{
  assert(id != NULL_ObjectId);

  unsigned int state;
  if (E(toolbox_get_object_state(0, id, &state))) {
    state = 0;
  }
  return TEST_BITS(state, Toolbox_GetObjectState_Showing);
}

int hand_back_caret(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(handle);
  NOT_USED(event_code);
  NOT_USED(event);

  if (!object_is_showing(id_block->ancestor_id))
    return 0; /* ancestor is hidden */

  /* Give the input focus to our ancestor (main editing window) */
  int window;
  if (!E(window_get_wimp_handle(0, id_block->ancestor_id, &window)))
    E(wimp_set_caret_position(window, -1, 0, 0, -1, -1));

  return 0;
}

int absdiff(int a, int b)
{
  int c = a - b;
  if (c < 0)
    return -c;
  return c;
}

void open_topleftofwin(unsigned int const flags, ObjectId const showobj,
  ObjectId const relativeto, ObjectId const parent, ComponentId const parent_component)
{
  WimpGetWindowStateBlock winstate;
  ON_ERR_RPT_RTN(window_get_wimp_handle(0, relativeto,
    &winstate.window_handle));

  WindowShowObjectBlock showblock;
  ON_ERR_RPT_RTN(wimp_get_window_state(&winstate));
    showblock.visible_area.xmin = winstate.visible_area.xmin+64;
    showblock.visible_area.ymin = winstate.visible_area.ymax-64;

  if (object_is_showing(showobj)) {
    /* Already open (may be iconised) */
    E(DeIconise_show_object(flags, showobj, Toolbox_ShowObject_Default,
      NULL, parent, parent_component));
  } else {
    /* Not open (can't very well be iconised!) */
    E(toolbox_show_object(flags, showobj, Toolbox_ShowObject_TopLeft,
      &showblock, parent, parent_component));
  }
}

void *get_ancestor_handle_if_showing(ObjectId const self_id)
{
  void *handle = NULL;
  assert(self_id != NULL_ObjectId);

  /* A bit tricky because the ancestor object ID may not be null but
     reference a dead object instead.
   */
  ObjectId ancestor_id;
  if (E(toolbox_get_ancestor(0, self_id, &ancestor_id, NULL)))
  {
    DEBUG("Failed to get ancestor of object 0x%x", self_id);
  }
  else if (ancestor_id == NULL_ObjectId)
  {
    DEBUG("No ancestor of object 0x%x", self_id);
  }
  else if (toolbox_get_client_handle(0, ancestor_id, &handle) != NULL)
  {
    DEBUG("Failed to get client handle of ancestor 0x%x", ancestor_id);
    handle = NULL;
  }

  return handle;
}

#if 0
void open_rhsofwin(unsigned int flags, ObjectId const showobj, ObjectId const relativeto, ObjectId const parent, ComponentId const parent_component)
{
  WimpGetWindowOutlineBlock wgwob;
  ON_ERR_RPT_RTN(window_get_wimp_handle(0, relativeto, &wgwob.window_handle));
  ON_ERR_RPT_RTN(wimp_get_window_outline(&wgwob));

  WimpGetWindowStateBlock winstate;
  winstate.window_handle = wgwob.window_handle;
  ON_ERR_RPT_RTN(wimp_get_window_state(&winstate));

  WindowShowObjectBlock showblock;
  showblock.visible_area.xmin = wgwob.outline.xmax;
  showblock.visible_area.ymin = winstate.visible_area.ymax;
  /* (actually ymax when using method Toolbox_ShowObject_TopLeft) */

  if (object_is_showing(showobj))
    /* Already open (may be iconised) */
    E(DeIconise_show_object(flags, showobj, Toolbox_ShowObject_TopLeft,
    &showblock, parent, parent_component));
  else
    /* Not open (can't very well be iconised!) */
    E(toolbox_show_object(flags, showobj, Toolbox_ShowObject_TopLeft,
    &showblock, parent, parent_component));
}
#endif

PaletteEntry opposite_col(PaletteEntry const real_col)
{
  PaletteEntry opp_colour = real_col;
  opp_colour ^= PAL_WHITE;
  DEBUGF("Opposite of colour &%X is &%X\n", real_col, opp_colour);
  return opp_colour;
}

bool show_win_if_open(ObjectId const win)
{
  /* Bring window to front, if already open somewhere */
  if (object_is_showing(win)) {
    WimpGetWindowStateBlock state;
    ON_ERR_RPT_RTN_V(window_get_wimp_handle(0, win, &state.window_handle),
    false);

    ON_ERR_RPT_RTN_V(wimp_get_window_state(&state), false);
    state.behind = -1; /* bring to front */
    ON_ERR_RPT_RTN_V(wimp_open_window((WimpOpenWindowBlock *)&state), false);
  }

  return true;
}

bool file_exists(char const *filepath)
{
  /* Read catalogue info for object without path */
  OS_File_CatalogueInfo catalogue_info;
  if (os_file_read_cat_no_path(filepath, &catalogue_info) != NULL) {
    DEBUG("...error");
    return false; /* if error then assume object doesn't exist */
  } else {
    DEBUG("...object %s",
          catalogue_info.object_type != ObjectType_NotFound ?
              "exists" : "does not exist");
    return (catalogue_info.object_type != ObjectType_NotFound);
  }
}

bool ensure_path_exists(char *const file_path)
{
  return !E(make_path(file_path, 0));
}

bool set_data_type(char *const file_path, DataType const data_type)
{
  return !E(set_file_type(file_path, data_type_to_file_type(data_type)));
}

bool verbose_copy(char const *old_name, char *new_name, bool const move)
{
  DEBUG("About to %s file(s) '%s' to '%s'", move ? "move" : "copy", old_name,
  new_name);
  if (!ensure_path_exists(new_name)) {
    return false;
  }
  /* No 'copy' function in standard library so call OS_FSControl directly */
  _kernel_oserror *const e = os_fscontrol_copy(old_name, new_name,
    OS_FSControl_Recurse | (move ? OS_FSControl_Delete : 0));

  if (e != NULL) {
    err_report(e->errnum, e->errmess);
  }

  return e == NULL;
}

bool verbose_rename(char const *old_name, char *new_name)
{
  DEBUG("About to rename file '%s' as '%s'", old_name, new_name);
  if (!ensure_path_exists(new_name)) {
    return false;
  }

  if (!rename(old_name, new_name)) {
    return true;
  }

  report_error(SFERROR(RenameFail), old_name, new_name);
  return false;
}

bool verbose_remove(char const *filename)
{
  DEBUG("About to remove file '%s'", filename);
  if (!remove(filename)) {
    return true;
  }

  report_error(SFERROR(RemoveFail), filename, "");
  return false;
}

void set_button(ObjectId const window, ComponentId const gadget,
  bool const condition)
{
  DEBUG("Ensuring button 0x%x in window 0x%x is %sselected", gadget, window,
        condition ? "" : "not ");

  int icon_flags; // WTF
  ON_ERR_RPT_RTN(button_get_flags(0, window, gadget, &icon_flags));

  if (condition) {
    if (!((unsigned)icon_flags & WimpIcon_Selected)) {
      E(button_set_flags(0, window, gadget, WimpIcon_Selected,
        WimpIcon_Selected));
    }
  } else if ((unsigned)icon_flags & WimpIcon_Selected) {
    E(button_set_flags(0, window, gadget, WimpIcon_Selected, 0));
  }
}

bool dialogue_confirm(char const *mess, char const *buttons_token)
{
  _kernel_oserror err_block = {DUMMY_ERRNO, ""};
  STRCPY_SAFE(err_block.errmess, mess);

  if (wimp_version >= 321) {
    /* Nice error box */
    return (wimp_report_error(&err_block, (1u << 8) | (1u << 11) |
           Wimp_ReportError_NoBeep, taskname, NULL, NULL,
           msgs_lookup(buttons_token)) == 3);
  }
  else {
    /* Backwards compatibility */
    return (wimp_report_error(&err_block, Wimp_ReportError_OK |
           Wimp_ReportError_Cancel | Wimp_ReportError_NoBeep, taskname) ==
           Wimp_ReportError_OK);
  }
}

bool update_menu_tick(IdBlock *id_block)
{
  int ticked;
  E(menu_get_tick(0, id_block->self_id, id_block->self_component,
                           &ticked));

  DEBUG("Setting entry %d of menu %d to %s", id_block->self_component,
        id_block->self_id, !ticked ? "ticked" : "unticked");

  E(menu_set_tick(0, id_block->self_id, id_block->self_component,
             !ticked));

  return !ticked;
}

bool wipe_menu(ObjectId const menu, ComponentId const last_entry)
{
  DEBUG("Wiping entries 0 to %d from menu %d", last_entry, menu);

  for (ComponentId i = 0; i <= last_entry; i++)
    ON_ERR_RPT_RTN_V(menu_remove_entry(0, menu, i), false);

  return true;
}

static void exec_cmd(char const *const prefix, char const *const file_name)
{
  assert(prefix != NULL);
  assert(file_name != NULL);

  StringBuffer cmd;
  stringbuffer_init(&cmd);
  if (!stringbuffer_append_all(&cmd, prefix) ||
      !stringbuffer_append_all(&cmd, file_name)) {
    report_error(SFERROR(NoMem), "", "");
  } else if (_kernel_oscli(stringbuffer_get_pointer(&cmd)) == _kernel_ERROR) {
    err_check_rep(_kernel_last_oserror());
  }
  stringbuffer_destroy(&cmd);
}

void open_file(char const *const file_name)
{
  exec_cmd("Filer_Run ", file_name);
}

void open_dir(char const *const file_name)
{
  exec_cmd("Filer_OpenDir ", file_name);
}

int truncate_string(char *string, int max_width)
{
  /* Truncates a text string (with ellipsis) to fit a width specified in
  OS coordinates. 'string' must point to an array at least 2 characters longer
  than the input string.
  Returns the width of the truncated string (in OS coordinates),
  or the maximum width (if shorter), or -1 if an error occurred. */
  _kernel_swi_regs args;
  int width;
  size_t num_chars = strlen(string);

  DEBUG("Will truncate string '%s' to fit width %d (in OS units)", string,
        max_width);
  do {
    if (wimp_version >= 321) {
      /* Variable size desktop font */
      args.r[0] = 1; /* calculate width of string */
      args.r[1] = (int)string;
      args.r[2] = 0; /* whole string */
      ON_ERR_RPT_RTN_V(wimp_text_op(&args), -1);
      width = args.r[0];
    } else {
      /* Fixed size system font */
      width = 8 * (int)strlen(string);
    }
    width += 8;
    if (width > max_width) {
      num_chars--; /* chop off a(nother) character */
      /* Add ellipsis and new terminator */
      strcpy(string + num_chars, "...");
    }
  } while (width > max_width && num_chars > 0);

  DEBUG("Truncated string is '%s' (width in OS units: %d)", string, width);
  return width > max_width ? max_width : width;
}

void hourglass_and_esc_on(void)
{
  /* Enable escape key & reset escape detection */
  if (_kernel_osbyte(229, 0, 0) == _kernel_ERROR)
    err_check_rep(_kernel_last_oserror());

  _kernel_escape_seen();

  hourglass_on();
}

void hourglass_and_esc_off(void)
{
  hourglass_off();

  /* Disable escape key & clear any escape condition */
  if (_kernel_osbyte(229, 1, 0) == _kernel_ERROR ||
  _kernel_osbyte(124, 0, 0) == _kernel_ERROR)
    err_check_rep(_kernel_last_oserror());
}

void edit_file(char const *const dir, char const *const tiles_set)
{
  char *const path = make_file_path_in_dir_on_path(
    CHOICES_WRITE_PATH, dir, tiles_set);
  if (path) {
    if (!file_exists(path)) {
      char *const defaults = make_file_path_in_dir_on_path(
                               CHOICES_DEFAULTS_PATH, dir, tiles_set);
      if (defaults && verbose_copy(defaults, path, false)) {
        open_file(path);
      }
      free(defaults);
    } else {
      open_file(path);
    }
    free(path);
  }
}

bool append_to_csv(StringBuffer *const csv, char const *const value)
{
  return (stringbuffer_get_length(csv) == 0 || stringbuffer_append_all(csv, ",")) &&
         stringbuffer_append_all(csv, value);
}

char *make_file_path_in_dir_on_path(char const *const path,
  char const *const subdir, char const *const leaf)
{
  StringBuffer full_path;
  stringbuffer_init(&full_path);

  char *canonical = NULL;
  if (stringbuffer_append_all(&full_path, path) &&
      stringbuffer_append_all(&full_path, subdir) &&
      stringbuffer_append_separated(&full_path, PATH_SEPARATOR, leaf))
  {
    E(canonicalise(&canonical, NULL, NULL,
      stringbuffer_get_pointer(&full_path)));
  } else {
    report_error(SFERROR(NoMem), "", "");
  }

  stringbuffer_destroy(&full_path);
  return canonical;
}

char *make_file_path_in_subdir(char const *const dir,
  char const *const subdir, char const *const leaf)
{
  StringBuffer path;
  stringbuffer_init(&path);

  char *canonical = NULL;
  if (stringbuffer_append_all(&path, dir) &&
      stringbuffer_append_separated(&path, PATH_SEPARATOR, subdir) &&
      stringbuffer_append_separated(&path, PATH_SEPARATOR, leaf))
  {
    E(canonicalise(&canonical, NULL, NULL,
      stringbuffer_get_pointer(&path)));
  } else {
    report_error(SFERROR(NoMem), "", "");
  }

  stringbuffer_destroy(&path);
  return canonical;
}


char *make_file_path_in_dir(char const *const dir, char const *const leaf)
{
  return make_file_path_in_dir_on_path("", dir, leaf);
}

bool report_error(SFError const err, char const *const path, char const *const extra)
{
  bool is_err = true;
  static char const *const ms_to_token[] = {
#define DECLARE_ERROR(ms) [SFErrorType_ ## ms] = #ms,
#include "DeclErrors.h"
#undef DECLARE_ERROR
  };
  if (!SFError_fail(err))
  {
    is_err = false;
  }
  else if (err.type != SFErrorType_AlreadyReported)
  {
    DEBUGF("Reporting %s from %s\n", ms_to_token[err.type], err.loc);
    err_report(DUMMY_ERRNO, msgs_lookup_subn(ms_to_token[err.type], 2, path, extra));
  }
  return is_err;
}

void load_fail(CONST _kernel_oserror *const error,
  void *const client_handle)
{
  NOT_USED(client_handle);
  if (error != NULL)
  {
    err_check_rep(msgs_error_subn(error->errnum, "LoadFail", 1, error->errmess));
  }
}

SFError check_trunc_or_ext(Reader *const reader, SFError err)
{
  if (!SFError_fail(err))
  {
    if (reader_ferror(reader))
    {
      err = SFERROR(ReadFail);
    }
    else if (reader_fgetc(reader) != EOF)
    {
      err = SFERROR(TooLong);
    }
  }
  else if (reader_feof(reader))
  {
    err = SFERROR(Trunc);
  }

  return err;
}

bool claim_drag(const WimpMessage *const message, int const file_types[],
  unsigned int const flags, int *const my_ref)
{
  /* Claim a drag for ourselves */
  assert(message != NULL);
  assert(file_types != NULL);
  DEBUGF("Replying to message ref %d from task 0x%x with a DragClaim message\n",
        message->hdr.my_ref, message->hdr.sender);

  WimpMessage reply = {
    .hdr = {
      .your_ref = message->hdr.my_ref,
      .action_code = Wimp_MDragClaim,
    },
  };

  WimpDragClaimMessage *const dragclaim = (WimpDragClaimMessage *)&reply.data;
  dragclaim->flags = flags;

  size_t const array_len = copy_file_types(dragclaim->file_types, file_types,
    ARRAY_SIZE(dragclaim->file_types) - 1u) + 1u;

  reply.hdr.size = WORD_ALIGN((int)(sizeof(reply.hdr) +
    offsetof(WimpDragClaimMessage, file_types) +
    (sizeof(dragclaim->file_types[0]) * array_len)));

  bool success = false;

  if (!E(wimp_send_message(Wimp_EUserMessage, &reply, message->hdr.sender,
       0, NULL)))
  {
    success = true;
    DEBUGF("DragClaim message ref. is %d\n", reply.hdr.my_ref);
  }

  if (my_ref != NULL)
  {
    *my_ref = success ? reply.hdr.my_ref : 0;
  }

  return success;
}
