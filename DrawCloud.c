/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Draw clouds
 *  Copyright (C) 2021 Christopher Bazley
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
#include "DrawCloud.h"
#include "Desktop.h"
#include "DrawObjs.h"
#include "Utils.h"
#include "EditWin.h"
#include "Plot.h"
#include "ObjLayout.h"

#define SPRITE_NAME "cloud"

enum {
  ScaleFactorNumerator = 1024,
};

static Vertex sprite_size_in_px, sprite_eig;

static void get_sprite_info(void)
{
  static bool have_sprite_info = false;
  if (have_sprite_info) {
    return;
  }

  have_sprite_info = true;
  SpriteAreaHeader *const sprite_area = get_sprite_area();
  if (sprite_area) {
    int mode = 0;
    if (!E(os_sprite_op_read_sprite_info(sprite_area, SPRITE_NAME, NULL,
         &sprite_size_in_px.x, &sprite_size_in_px.y, &mode))) {
      E(os_read_mode_variable(mode, ModeVar_XEigFactor, &sprite_eig.x, NULL));
      E(os_read_mode_variable(mode, ModeVar_YEigFactor, &sprite_eig.y, NULL));
    }
  }
}

Vertex DrawCloud_get_size_os(void)
{
  get_sprite_info();
  return Vertex_mul_log2_pair(sprite_size_in_px, sprite_eig);
}

bool DrawCloud_init(DrawCloudContext *const context, CloudColData const *const clouds,
  PaletteEntry const (*const palette)[NumColours], PaletteEntry const (*const sel_palette)[NumColours],
  int const zoom, bool const is_ghost)
{
  /* Set up the workspace for the transfer function */
  assert(context);
  assert(palette);

  Vertex const eigen_factors = Desktop_get_eigen_factors();
  get_sprite_info();

  *context = (DrawCloudContext){
    .scale_factors = {
      /* texels to OS units */
      .xmul = SIGNED_R_SHIFT(ScaleFactorNumerator, zoom - sprite_eig.x),
      .ymul = SIGNED_R_SHIFT(ScaleFactorNumerator, zoom - sprite_eig.y),
      /* OS units to pixels */
      .xdiv = ScaleFactorNumerator << eigen_factors.x,
      .ydiv = ScaleFactorNumerator << eigen_factors.y
    },
    .is_ghost = is_ghost,
  };

  for (DrawCloudSel is_selected = DrawCloudSel_No; is_selected < DrawCloudSel_Count; ++is_selected) {
    for (size_t cloud_type = 0; cloud_type < Obj_NumCloudTints; ++cloud_type) {
      PaletteEntry cloud_colours[Clouds_NumColours];

      for (size_t n = 0; n < Clouds_NumColours; ++n) {
        unsigned int const cloud_colour = clouds_get_colour(clouds, n) + cloud_type;
        cloud_colours[n] = ((is_selected == DrawCloudSel_Yes) && sel_palette) ?
                              (*sel_palette)[cloud_colour] : (*palette)[cloud_colour];
      }

      /* The transfer function can't be written in C because it's not APCS-compliant */
      extern PaletteEntry transfer_func(PaletteEntry);

      ColourTransGenerateTableBlock block = {
        .source = {
          .type = ColourTransContextType_Sprite,
          .data = {
            .sprite = {
              .sprite_area = get_sprite_area(),
              .name_or_pointer = SPRITE_NAME
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
        .workspace = cloud_colours,
      };

      if (E(colourtrans_generate_table(ColourTrans_GenerateTable_TransferFunction, &block,
             context->trans_table[is_selected][cloud_type],
             sizeof(context->trans_table[is_selected][cloud_type]),
             NULL))) {
        return false;
      }
    }
  }
  return true;
}

void DrawCloud_plot(DrawCloudContext const *const context, Vertex const scr_pos,
  bool const is_selected, int const cloud_type)
{
  assert(context);
  if (!context->is_ghost) {
    E(os_sprite_op_plot_scaled_sprite(
       get_sprite_area(), SPRITE_NAME, scr_pos.x, scr_pos.y,
       GCOLAction_Overwrite | GCOLAction_TransparentBG,
       &context->scale_factors,
       context->trans_table[is_selected ? DrawCloudSel_Yes : DrawCloudSel_No][cloud_type]));
  }
}
