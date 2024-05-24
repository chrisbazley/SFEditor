/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Desktop screen mode variables
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

#include <stdbool.h>
#include "stdlib.h"
#include "kernel.h"
#include "swis.h"
#include "wimp.h"
#include "wimplib.h"
#include "event.h"
#include "Flex.h"

#include "osvdu.h"
#include "clrtrans.h"
#include "err.h"
#include "msgtrans.h"
#include "nobudge.h"

#include "Desktop.h"
#include "DrawTiles.h"

enum {
  PreAllocSize = 512,
  ReadModeNumber = 135,
};

static bool vars_are_valid;
static Vertex eigen_factors, desktop_size;
static void *trans_table;
static int log2bpp;

static WimpMessageHandler mode_change_handler, pal_change_handler;
static void read_mode_vars(void);
static void read_trans_table(int mode);

static void Desktop_destroy(void)
{
  if (trans_table)
  {
    flex_free(&trans_table);
  }
}

void Desktop_init(void)
{
  vars_are_valid = false;
  trans_table = NULL;

  EF(event_register_message_handler(Wimp_MModeChange, mode_change_handler, NULL));
  EF(event_register_message_handler(Wimp_MPaletteChange, pal_change_handler, NULL));

  atexit(Desktop_destroy);
}

void Desktop_invalidate(void)
{
  vars_are_valid = false;
  if (trans_table != NULL) {
    flex_free(&trans_table);
  }
}

Vertex Desktop_get_eigen_factors(void)
{
  read_mode_vars();
  return eigen_factors;
}

Vertex Desktop_get_size_px(void)
{
  read_mode_vars();
  return desktop_size;
}

Vertex Desktop_get_size_os(void)
{
  read_mode_vars();
  return Vertex_mul_log2_pair(desktop_size, eigen_factors);
}

void *Desktop_get_trans_table(void)
{
  if (trans_table == NULL) {
    read_trans_table(DrawTilesModeNumber);
  }
  if (trans_table != NULL) {
    nobudge_register(PreAllocSize);
  }
  return trans_table;
}

void Desktop_put_trans_table(void *const tt)
{
  assert(trans_table == tt);
  if (tt != NULL) {
    nobudge_deregister();
  }
}

int Desktop_get_screen_mode(void)
{
  int mode = DrawTilesModeNumber;
  _kernel_swi_regs regs = {{ReadModeNumber}};
  if (!E(_kernel_swi(OS_Byte, &regs, &regs)))
  {
    mode = regs.r[2];
  }
  return mode;
}

static void read_mode_vars(void)
{
  if (vars_are_valid) {
    return;
  }
  enum {
    VarIndex_XEig,
    VarIndex_YEig,
    VarIndex_XWindLimit,
    VarIndex_YWindLimit,
    VarIndex_Log2BPP,
    VarIndex_End
  };
  static VDUVar const mode_variables[] = {
   [VarIndex_XEig] = (VDUVar)ModeVar_XEigFactor,
   [VarIndex_YEig] = (VDUVar)ModeVar_YEigFactor,
   [VarIndex_XWindLimit] = (VDUVar)ModeVar_XWindLimit,
   [VarIndex_YWindLimit] = (VDUVar)ModeVar_YWindLimit,
   [VarIndex_Log2BPP] = (VDUVar)ModeVar_Log2BPP,
   [VarIndex_End] = VDUVar_EndOfList};

  int mode_var_val[VarIndex_End] = {0};

  if (!E(os_read_vdu_variables(mode_variables, mode_var_val))) {
    eigen_factors.x = mode_var_val[VarIndex_XEig];
    eigen_factors.y = mode_var_val[VarIndex_YEig];
    desktop_size.x = mode_var_val[VarIndex_XWindLimit];
    desktop_size.y = mode_var_val[VarIndex_YWindLimit];
    log2bpp = mode_var_val[VarIndex_Log2BPP];
    vars_are_valid = true;
  }
}

static void read_trans_table(int const mode)
{
  /* Shouldn't call this function if there is an existing
     colour translation table */
  assert(trans_table == NULL);

  /* Find required memory for colour translation table */
  const ColourTransGenerateTableBlock block = {
    .source = {
      .type = ColourTransContextType_Screen,
      .data = {
        .screen = {
          .mode = mode,
          .palette = ColourTrans_DefaultPalette
        }
      }
    },
    .destination = {
      .type = ColourTransContextType_Screen,
      .data = {
        .screen = {
          .mode = ColourTrans_CurrentMode,
          .palette = ColourTrans_CurrentPalette
        }
      }
    }
  };

  size_t size;
  if (E(colourtrans_generate_table(0, &block, NULL, 0, &size)))
    return;

  DEBUGF("%zu bytes are required for colour translation table\n", size);

  /* Allocate a buffer of the required size for the translation table */
  if (!flex_alloc(&trans_table, (int)size)) {
    WARN_RTN("ColTransMem");
  }

  /* Create colour translation table */
  nobudge_register(PreAllocSize);
  if (!E(colourtrans_generate_table(0, &block, trans_table, size, NULL))) {
    DEBUGF("Created colour translation table at %p\n", trans_table);

    /* Is the translation table really necessary? */
    if (log2bpp == DrawTilesModeLog2BPP &&
        size == 1u << (1u << DrawTilesModeLog2BPP)) {
      size_t i;
      for (i = 0; i < size; i++) {
        char * const ct = trans_table;
        if (ct[i] != i)
          break;
      }
      if (i >= size) {
        /* Translation table is a one-to-one mapping, so discard it */
        DEBUGF("Discarding superfluous colour translation table\n");
        flex_free(&trans_table);
      }
    }
  } else {
    flex_free(&trans_table);
  }
  nobudge_deregister();
}

static int mode_change_handler(WimpMessage *const message, void *const handle)
{
  NOT_USED(message);
  NOT_USED(handle);

  Desktop_invalidate();
  return 0; /* don't claim event */
}

static int pal_change_handler(WimpMessage *const message, void *const handle)
{
  NOT_USED(message);
  NOT_USED(handle);

  if (trans_table != NULL) {
    flex_free(&trans_table);
  }
  return 0; /* don't claim event */
}
