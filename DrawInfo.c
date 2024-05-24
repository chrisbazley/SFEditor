/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Draw strategic target information
 *  Copyright (C) 2022 Christopher Bazley
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

#include "swis.h"
#include "toolbox.h"

#include "Debug.h"
#include "Macros.h"
#include "Err.h"
#include "PalEntry.h"
#include "OSSpriteOp.h"
#include "ClrTrans.h"
#include "OSVDU.h"
#include "MapTexBitm.h"
#include "Vertex.h"
#include "Clouds.h"
#include "Obj.h"
#include "DrawInfo.h"
#include "Desktop.h"
#include "Utils.h"
#include "EditWin.h"
#include "Plot.h"
#include "ObjLayout.h"

#define SPRITE_NAME "info"

enum {
  ScaleFactorNumerator = 1024,
  HalveFactorLog2 = 1,
};

static Vertex sprite_size_in_px[2], sprite_eig[2];
static char *sprite_names[] = {"info", "ginfo"};

static void get_sprite_info(void)
{
  static bool have_sprite_info = false;
  if (have_sprite_info) {
    return;
  }

  have_sprite_info = true;
  SpriteAreaHeader *const sprite_area = get_sprite_area();
  if (sprite_area) {
    for (int is_ghost = 0; is_ghost < 2; ++is_ghost) {
      int mode = 0;
      if (!E(os_sprite_op_read_sprite_info(sprite_area, sprite_names[is_ghost], NULL,
           &sprite_size_in_px[is_ghost].x, &sprite_size_in_px[is_ghost].y, &mode))) {
        E(os_read_mode_variable(mode, ModeVar_XEigFactor, &sprite_eig[is_ghost].x, NULL));
        E(os_read_mode_variable(mode, ModeVar_YEigFactor, &sprite_eig[is_ghost].y, NULL));
      }
    }
  }
}

Vertex DrawInfo_get_size_os(bool const is_ghost)
{
  get_sprite_info();
  return Vertex_mul_log2_pair(sprite_size_in_px[is_ghost], sprite_eig[is_ghost]);
}

bool DrawInfo_init(DrawInfoContext *const context,
  PaletteEntry (*const colours)[TargetInfoMax][DrawInfoPaletteSize],
  PaletteEntry (*const sel_colours)[TargetInfoMax][DrawInfoPaletteSize],
  int const zoom, bool const is_ghost)
{
  /* Set up the workspace for the transfer function */
  assert(context);

  Vertex const eigen_factors = Desktop_get_eigen_factors();
  get_sprite_info();

  Vertex const scaled_info_size = Vertex_div_log2(DrawInfo_get_size_os(is_ghost), zoom);
  DEBUGF("scaled_info_size %d,%d\n", scaled_info_size.x, scaled_info_size.y);

  *context = (DrawInfoContext){
    .plot_offset = Vertex_div_log2(scaled_info_size, HalveFactorLog2),
    .scale_factors = {
      /* Map units to OS units */
      .xmul = SIGNED_R_SHIFT(ScaleFactorNumerator, zoom - sprite_eig[is_ghost].x),
      .ymul = SIGNED_R_SHIFT(ScaleFactorNumerator, zoom - sprite_eig[is_ghost].y),
      /* OS units to screen pixels */
      .xdiv = ScaleFactorNumerator << eigen_factors.x,
      .ydiv = ScaleFactorNumerator << eigen_factors.y
    },
    .sprite_name = sprite_names[is_ghost],
  };

  int const maxp = is_ghost ? 1 : TargetInfoMax;
  for (int p = 0; p < maxp; ++p) {
    for (int is_selected = 0; is_selected < DrawInfoSel_Count; ++is_selected) {
      PaletteEntry info_colours[DrawInfoPaletteSize];

      for (int n = 0; n < DrawInfoPaletteSize; ++n) {
        info_colours[n] = ((is_selected == DrawInfoSel_Yes) && sel_colours) ?
                              (*sel_colours)[p][n] : (*colours)[p][n];
      }

      /* The transfer function can't be written in C because it's not APCS-compliant */
      extern PaletteEntry transfer_func(PaletteEntry);

      ColourTransGenerateTableBlock block = {
        .source = {
          .type = ColourTransContextType_Sprite,
          .data = {
            .sprite = {
              .sprite_area = get_sprite_area(),
              .name_or_pointer = sprite_names[is_ghost]
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
        },
        .transfer = transfer_func,
        .workspace = info_colours,
      };

      if (E(colourtrans_generate_table(ColourTrans_GenerateTable_TransferFunction, &block,
             context->trans_table[is_selected][p],
             sizeof(context->trans_table[is_selected][p]),
             NULL))) {
        return false;
      }
    }
  }
  return true;
}

void DrawInfo_plot(DrawInfoContext const *const context, Vertex scr_pos,
  bool const is_selected, int const id)
{
  scr_pos = Vertex_sub(scr_pos, context->plot_offset);
  E(os_sprite_op_plot_scaled_sprite(
     get_sprite_area(), context->sprite_name, scr_pos.x, scr_pos.y,
     GCOLAction_Overwrite | GCOLAction_TransparentBG,
     &context->scale_factors,
     context->trans_table[is_selected ? DrawInfoSel_Yes : DrawInfoSel_No][id % TargetInfoMax]));
}
