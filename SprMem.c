/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Memory management for a sprite area
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

#include "stdlib.h"
#include <stdbool.h>
#include "kernel.h"

#include "Flex.h"
#include "nobudge.h"
#include "macros.h"
#include "err.h"
#include "msgtrans.h"

#include "debug.h"
#include "OSSpriteOp.h"
#include "OSVDU.h"

#include "Vertex.h"
#include "SprMem.h"
#include "Utils.h"

enum {
  GROWTH_FACTOR = 2,
  PREALLOC_SIZE = 512
};

static void *save_area = NULL;
static SpriteRestoreOutputBlock old_output_state;
static bool restore_state = false, on_exit = false;

typedef _kernel_oserror *switch_output_fn(SpriteAreaHeader *,
  char const *, void *, size_t, size_t *, SpriteRestoreOutputBlock *);

/* ---------------- Private functions --------------- */

static int estimate_sprite_size(Vertex const size, int const mode)
{
  assert(size.x >= 1);
  assert(size.y >= 1);

  int log2bpp;
  bool valid;
  if (E(os_read_mode_variable(mode, ModeVar_Log2BPP, &log2bpp, &valid)) ||
      !valid) {
    return -1;
  }

  int const bits_per_row = size.x << log2bpp;
  int const row_stride = WORD_ALIGN(bits_per_row / 8);
  return (int)sizeof(SpriteHeader) + (row_stride * size.y);
}

static bool ensure_free(SprMem *const sm, Vertex const size, int const mode)
{
  assert(sm != NULL);

  int const sprite_size = estimate_sprite_size(size, mode);
  if (sprite_size < 0) {
    return false; /* failed to estimate size */
  }

  SpriteAreaHeader hdr;
  if (E(os_sprite_op_read_header(sm->mem, &hdr))) {
    return false;
  }

  int const req_size = hdr.used + sprite_size;
  if (req_size > hdr.size) {
    int const new_size = HIGHEST(req_size, hdr.size * GROWTH_FACTOR);
    DEBUG("Extending sprite area %p from %d to %d bytes",
          sm->mem, hdr.size, new_size);

    if (!flex_extend(&sm->mem, new_size)) {
      DEBUG("Failed to extend sprite area");
      report_error(SFERROR(NoMem), "", "");
      return false;
    }

    /* update sprite area control block */
    ((SpriteAreaHeader *)sm->mem)->size = req_size;
  }

  return true;
}

static void restore_output(void)
{
  if (restore_state) {
    restore_state = false;

    E(os_sprite_op_restore_output(&old_output_state));
    nobudge_deregister();

    free(save_area);
    save_area = NULL;
  }
}

static bool switch_output(SprMem *const sm, char const *const name,
  switch_output_fn *const fn)
{
  assert(sm != NULL);
  assert(name != NULL);
  assert(fn != NULL);
  assert(!flex_set_budge(-1));

  restore_output();

  /* Get size of save area needed to preserve the VDU driver state */
  size_t save_area_size = 0;
  if (E(fn(sm->mem, name, NULL, 0, &save_area_size, NULL)))
    return false;

  /* Allocate the VDU driver state save area */
  free(save_area);
  save_area = malloc(save_area_size);
  if (!save_area)
  {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  /* Make the save area valid */
  *(int *)save_area = 0;

  /* Restore the VDU driver state at exit if not earlier  */
  if (!on_exit) {
    atexit(restore_output);
    on_exit = true;
  }

  /* Switch VDU output to the sprite or its mask */
  if (E(fn(sm->mem, name, save_area, save_area_size, NULL, &old_output_state)))
    return false;

  restore_state = true;
  return true;
}

/* ---------------- Public functions ---------------- */

bool SprMem_init(SprMem *const sm, int size)
{
  assert(sm != NULL);
  assert(size >= 0);

  size = HIGHEST(size, (int)sizeof(SpriteAreaHeader));
  if (!flex_alloc(&sm->mem, size))
  {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  nobudge_register(PREALLOC_SIZE);
  SpriteAreaHeader *const area = sm->mem;
  area->size = size;
  area->sprite_count = 0;
  area->first = sizeof(*area);
  area->used = sizeof(*area);
  nobudge_deregister();

  return true;
}

bool SprMem_create_sprite(SprMem *const sm, char const *const name,
  bool const has_palette, Vertex const size, int const mode)
{
  assert(sm != NULL);
  assert(name != NULL);
  assert(size.x >= 1);
  assert(size.y >= 1);

  nobudge_register(PREALLOC_SIZE);
  bool success = ensure_free(sm, size, mode);
  if (success)
    success = !E(os_sprite_op_create_sprite(sm->mem, name, has_palette,
                                            size.x, size.y, mode));
  nobudge_deregister();
  return success;
}

bool SprMem_create_mask(SprMem *const sm, char const *const name)
{
  assert(sm != NULL);
  assert(name != NULL);

  bool has_mask;
  Vertex size;
  int mode;

  nobudge_register(PREALLOC_SIZE);
  bool success = !E(os_sprite_op_read_sprite_info(sm->mem, name,
                     &has_mask, &size.x, &size.y, &mode));
  if (success && !has_mask) {
    success = ensure_free(sm, size, mode);
    if (success) {
      success = !E(os_sprite_op_create_mask(sm->mem, name));
    }
  }
  nobudge_deregister();
  return success;
}

void SprMem_rename(SprMem *const sm, char const *const old_name,
  char const *const new_name)
{
  assert(sm != NULL);
  assert(old_name != NULL);
  assert(new_name != NULL);

  nobudge_register(PREALLOC_SIZE);
  E(os_sprite_op_rename(sm->mem, old_name, new_name));
  nobudge_deregister();
}

SpriteHeader *SprMem_get_sprite_address(SprMem *const sm,
  char const *const name)
{
  assert(sm != NULL);
  assert(name != NULL);

  nobudge_register(PREALLOC_SIZE); /* protect sprite pointer */
  SpriteHeader *sprite = NULL;
  if (E(os_sprite_op_select(sm->mem, name, &sprite)))
    return NULL;

  return sprite;
}

void SprMem_put_sprite_address(SprMem *const sm,
  SpriteHeader *const sprite)
{
  NOT_USED(sm);
  NOT_USED(sprite);
  assert(sm != NULL);
  assert(sprite != NULL);
  nobudge_deregister();
}

SpriteAreaHeader *SprMem_get_area_address(SprMem *const sm)
{
  assert(sm != NULL);
  nobudge_register(PREALLOC_SIZE); /* protect sprite pointer */
  return sm->mem;
}

void SprMem_put_area_address(SprMem *const sm)
{
  NOT_USED(sm);
  assert(sm != NULL);
  nobudge_deregister();
}

bool SprMem_output_to_sprite(SprMem *const sm, char const *const name)
{
  nobudge_register(PREALLOC_SIZE); /* protect sprite pointer */
  bool const success = switch_output(sm, name,
    os_sprite_op_output_to_sprite);

  if (!success) {
    nobudge_deregister();
  }
  return success;
}

bool SprMem_output_to_mask(SprMem *const sm, char const *const name)
{
  nobudge_register(PREALLOC_SIZE); /* protect sprite pointer */
  bool const success = switch_output(sm, name,
    os_sprite_op_output_to_mask);

  if (!success) {
    nobudge_deregister();
  }
  return success;
}

void SprMem_restore_output(SprMem *const sm)
{
  NOT_USED(sm);
  assert(sm != NULL);
  restore_output();
}

void SprMem_delete(SprMem *const sm, char const *const name)
{
  assert(sm != NULL);
  nobudge_register(PREALLOC_SIZE);
  if (E(os_sprite_op_delete(sm->mem, name)))
  {
    DEBUGF("Failed to delete sprite '%s'\n", name);
  }
  nobudge_deregister();
}

void SprMem_flip(const SprMem *const sm, char const *const name)
{
  assert(sm != NULL);
  assert(name != NULL);

  /* If output was switched to a sprite or mask then this nobudge_register
     does nothing cheaply. */
  nobudge_register(PREALLOC_SIZE);

  if (E(os_sprite_op_flip_x(sm->mem, name)) ||
      E(os_sprite_op_flip_y(sm->mem, name)))
  {
    DEBUGF("Failed to flip sprite '%s'\n", name);
  }

  nobudge_deregister();
}

void SprMem_plot_sprite(const SprMem *const sm,
      char const *const name, Vertex const coords, int const action)
{
  assert(sm != NULL);
  assert(name != NULL);

  /* If output was switched to a sprite or mask then this nobudge_register
     does nothing cheaply. */
  nobudge_register(PREALLOC_SIZE);

  if (E(os_sprite_op_plot_sprite(sm->mem, name, coords.x, coords.y, action)))
  {
    DEBUGF("Failed to plot sprite '%s'\n", name);
  }

  nobudge_deregister();
}

void SprMem_plot_scaled_sprite(const SprMem *const sm, char const *const name,
      Vertex const coords, int const action, ScaleFactors *const scale,
      void const *const colours)
{
  assert(sm != NULL);
  assert(name != NULL);

  /* If output was switched to a sprite or mask then this nobudge_register
     does nothing cheaply. */
  nobudge_register(PREALLOC_SIZE);

  if (E(os_sprite_op_plot_scaled_sprite(sm->mem, name,
    coords.x, coords.y, action, scale, colours)))
  {
    DEBUGF("Failed to plot sprite '%s'\n", name);
  }

  nobudge_deregister();
}


void SprMem_plot_trans_quad_sprite(const SprMem *sm,
  const char *name, BBox const *src, int action,
  TransformQuad const *quad,
  void const *colours)
{
  assert(sm != NULL);
  assert(name != NULL);

  /* If output was switched to a sprite or mask then this nobudge_register
     does nothing cheaply. */
  nobudge_register(PREALLOC_SIZE);

  if (E(os_sprite_op_plot_trans_quad_sprite(sm->mem, name,
    src, action, quad, colours)))
  {
    DEBUGF("Failed to plot sprite '%s'\n", name);
  }

  nobudge_deregister();
}

void SprMem_plot_trans_matrix_sprite(const SprMem *sm,
  const char *name, BBox const *src, int action,
  TransformMatrix const *matrix,
  void const *colours)
{
  assert(sm != NULL);
  assert(name != NULL);

  /* If output was switched to a sprite or mask then this nobudge_register
     does nothing cheaply. */
  nobudge_register(PREALLOC_SIZE);

  if (E(os_sprite_op_plot_trans_matrix_sprite(sm->mem, name,
    src, action, matrix, colours)))
  {
    DEBUGF("Failed to plot sprite '%s'\n", name);
  }

  nobudge_deregister();
}

size_t SprMem_get_sprite_count(const SprMem *const sm)
{
  assert(sm != NULL);

  nobudge_register(PREALLOC_SIZE);
  SpriteAreaHeader hdr;
  size_t count = 0;
  if (!E(os_sprite_op_read_header(sm->mem, &hdr)) && hdr.sprite_count > 0) {
    count = (size_t)hdr.sprite_count;
  }
  nobudge_deregister();
  return count;
}

void SprMem_minimize(SprMem *const sm)
{
  assert(sm != NULL);

  nobudge_register(PREALLOC_SIZE);
  SpriteAreaHeader hdr;
  bool success = !E(os_sprite_op_read_header(sm->mem, &hdr));
  nobudge_deregister();

  if (success && hdr.used < hdr.size) {
    DEBUG("Trimming sprite area %p from %d to %d bytes",
          sm->mem, hdr.size, hdr.used);

    if (!flex_extend(&sm->mem, hdr.used)) {
      DEBUG("Failed to trim sprite area");
      report_error(SFERROR(NoMem), "", "");
    } else {
      /* update sprite area control block */
      ((SpriteAreaHeader *)sm->mem)->size = hdr.used;
    }
  }
}

bool SprMem_verify(const SprMem *const sm)
{
  nobudge_register(PREALLOC_SIZE);
  bool const success = !E(os_sprite_op_verify(sm->mem));
  nobudge_deregister();
  return success;
}

bool SprMem_save(const SprMem *const sm, char const *const filename)
{
  nobudge_register(PREALLOC_SIZE);
  bool const success = !E(os_sprite_op_save(sm->mem, filename));
  nobudge_deregister();
  return success;
}

void SprMem_destroy(SprMem *const sm)
{
  assert(sm != NULL);
  flex_free(&sm->mem);
}
