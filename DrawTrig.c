/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Draw mission action triggers
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

#include "Debug.h"
#include "Macros.h"
#include "Err.h"
#include "PalEntry.h"
#include "OSSpriteOp.h"
#include "ClrTrans.h"
#include "OSVDU.h"

#include "Vertex.h"
#include "Obj.h"
#include "DrawTrig.h"
#include "DrawObjs.h"
#include "Triggers.h"
#include "Desktop.h"
#include "Utils.h"

typedef enum {
  DrawTrigSpr_First,
  DrawTrigSpr_MissionTarget = DrawTrigSpr_First,
  DrawTrigSpr_BonusMultiATA,
  DrawTrigSpr_BonusMegaLaser,
  DrawTrigSpr_BonusBombs,
  DrawTrigSpr_BonusMines,
  DrawTrigSpr_DefenceOff,
  DrawTrigSpr_DefenceOn,
  DrawTrigSpr_ChainReaction,
  DrawTrigSpr_CrippleShipType,
  DrawTrigSpr_CashBonus,
  DrawTrigSpr_MissionTimer,
  DrawTrigSpr_FriendlyDead,
  DrawTrigSpr_FixScanners,
  DrawTrigSpr_DefendGun,
  DrawTrigSpr_DefendSAM,
  DrawTrigSpr_DefendHangar,
  DrawTrigSpr_Count,
} DrawTrigSpr;

static DrawTrigSpr get_sprite(TriggerParam const param)
{
  static DrawTrigSpr const sprite_map[] = {
    [TriggerAction_MissionTarget] = DrawTrigSpr_MissionTarget,
    [TriggerAction_BonusMultiATA] = DrawTrigSpr_BonusMultiATA,
    [TriggerAction_BonusMegaLaser] = DrawTrigSpr_BonusMegaLaser,
    [TriggerAction_BonusBombs] = DrawTrigSpr_BonusBombs,
    [TriggerAction_BonusMines] = DrawTrigSpr_BonusMines,
    [TriggerAction_DefenceTimer] = DrawTrigSpr_DefenceOff,
    [TriggerAction_ChainReaction] = DrawTrigSpr_ChainReaction,
    [TriggerAction_CrippleShipType] = DrawTrigSpr_CrippleShipType,
    [TriggerAction_CashBonus] = DrawTrigSpr_CashBonus,
    [TriggerAction_MissionTimer] = DrawTrigSpr_MissionTimer,
    [TriggerAction_FriendlyDead] = DrawTrigSpr_FriendlyDead,
    [TriggerAction_MissionTarget2] = DrawTrigSpr_MissionTimer,
    [TriggerAction_FixScanners] = DrawTrigSpr_FixScanners,
  };

  assert(param.action >= 0);
  assert(param.action < ARRAY_SIZE(sprite_map));

  DrawTrigSpr sprite = sprite_map[param.action];
  if (param.action == TriggerAction_DefenceTimer &&
      param.value == TriggerActivateDefences) {
    sprite = DrawTrigSpr_DefenceOn;
  }
  return sprite;
}

static char *get_sprite_name(DrawTrigSpr const sprite)
{
  static char *const sprite_names[DrawTrigSpr_Count] = {
    [DrawTrigSpr_MissionTarget] = "objtarget",
    [DrawTrigSpr_BonusMultiATA] = "objmultiata",
    [DrawTrigSpr_BonusMegaLaser] = "objmegalaser",
    [DrawTrigSpr_BonusBombs] = "objbomb",
    [DrawTrigSpr_BonusMines] = "objmine",
    [DrawTrigSpr_DefenceOff] = "objdefendoff",
    [DrawTrigSpr_DefenceOn] = "objdefendon",
    [DrawTrigSpr_ChainReaction] = "objchain",
    [DrawTrigSpr_CrippleShipType] = "objshield",
    [DrawTrigSpr_CashBonus] = "objcash",
    [DrawTrigSpr_MissionTimer] = "objtimer",
    [DrawTrigSpr_FriendlyDead] = "objprotect",
    [DrawTrigSpr_FixScanners] = "objjammer",
    [DrawTrigSpr_DefendGun] = "defendgun",
    [DrawTrigSpr_DefendSAM] = "defendsam",
    [DrawTrigSpr_DefendHangar] = "defendhangar",
  };

  assert(sprite >= 0);
  assert(sprite < ARRAY_SIZE(sprite_names));
  return sprite_names[sprite];
}

static DrawTrigSpr get_def_sprite(ObjRef const obj_ref)
{
  DrawTrigSpr sprite = DrawTrigSpr_DefendHangar;
  if (objects_ref_is_gun(obj_ref)) {
    sprite = DrawTrigSpr_DefendGun;
  } else if (objects_ref_is_sam(obj_ref)) {
    sprite = DrawTrigSpr_DefendSAM;
  } else {
    assert(objects_ref_is_hangar(obj_ref));
  }
  return sprite;
}

enum {
  ScaleFactorNumerator = 1024,
  HalveFactorLog2 = 1,
};


static Vertex max_sprite_size_in_os, sprite_eig[DrawTrigSpr_Count];

static void get_sprite_info(void)
{
  static bool have_sprite_info = false;
  if (have_sprite_info) {
    return;
  }

  have_sprite_info = true;
  SpriteAreaHeader *const sprite_area = get_sprite_area();
  if (sprite_area) {
    int all_mode = OS_ReadModeVariable_CurrentMode;
    for (DrawTrigSpr sprite = DrawTrigSpr_First; sprite < DrawTrigSpr_Count; ++sprite) {
      Vertex sprite_size_in_px;
      int mode = 0;
      if (E(os_sprite_op_read_sprite_info(sprite_area,
             get_sprite_name(sprite), NULL, &sprite_size_in_px.x, &sprite_size_in_px.y, &mode))) {
        continue;
      }

      if (E(os_read_mode_variable(mode, ModeVar_XEigFactor, &sprite_eig[sprite].x, NULL)) ||
          E(os_read_mode_variable(mode, ModeVar_YEigFactor, &sprite_eig[sprite].y, NULL))) {
        continue;
      }

      Vertex const sprite_size_in_os = Vertex_mul_log2_pair(sprite_size_in_px, sprite_eig[sprite]);
      max_sprite_size_in_os = Vertex_max(max_sprite_size_in_os, sprite_size_in_os);
    }
  }
}

Vertex DrawTrig_get_max_size_os(void)
{
  get_sprite_info();
  DEBUGF("Max sprite size is %d,%d\n", max_sprite_size_in_os.x, max_sprite_size_in_os.y);
  return max_sprite_size_in_os;
}

bool DrawTrig_init(DrawTrigContext *const context,
  PaletteEntry (*const colours)[DrawTrigNumColours],
  PaletteEntry (*const sel_colours)[DrawTrigNumColours], int const zoom)
{
  /* Set up the workspace for the transfer function */
  assert(context);
  assert(palette);

  *context = (DrawTrigContext){
    .zoom = zoom,
  };

  get_sprite_info();

  for (size_t is_selected = 0; is_selected < ARRAY_SIZE(context->trans_table); ++is_selected) {
    PaletteEntry trig_colours[DrawTrigNumColours];

    for (int n = 0; n < DrawTrigNumColours; ++n) {
      trig_colours[n] = ((is_selected == DrawTrigSel_Yes) && sel_colours) ?
                            (*sel_colours)[n] : (*colours)[n];
    }

    /* The transfer function can't be written in C because it's not APCS-compliant */
    extern PaletteEntry transfer_func(PaletteEntry);

    ColourTransGenerateTableBlock block = {
      .source = {
        .type = ColourTransContextType_Sprite,
        .data = {
          .sprite = {
            .sprite_area = get_sprite_area(),
            .name_or_pointer = get_sprite_name(DrawTrigSpr_MissionTarget)
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
      .workspace = trig_colours,
    };

    if (E(colourtrans_generate_table(ColourTrans_GenerateTable_TransferFunction, &block,
           context->trans_table[is_selected],
           sizeof(context->trans_table[is_selected]),
           NULL))) {
      return false;
    }
  }
  return true;
}

static void plot_spr(DrawTrigContext const *const context, DrawTrigSpr const sprite,
  Vertex const scr_pos, bool const is_selected)
{
  Vertex const eigen_factors = Desktop_get_eigen_factors();

  ScaleFactors const scale_factors = {
      /* texels to OS units */
      .xmul = SIGNED_R_SHIFT(ScaleFactorNumerator, context->zoom - sprite_eig[sprite].x),
      .ymul = SIGNED_R_SHIFT(ScaleFactorNumerator, context->zoom - sprite_eig[sprite].y),
      /* OS units to screen pixels */
      .xdiv = ScaleFactorNumerator << eigen_factors.x,
      .ydiv = ScaleFactorNumerator << eigen_factors.y
  };

  E(os_sprite_op_plot_scaled_sprite(
     get_sprite_area(), get_sprite_name(sprite), scr_pos.x, scr_pos.y,
     GCOLAction_Overwrite | GCOLAction_TransparentBG,
     &scale_factors, context->trans_table[is_selected]));
}

void DrawTrig_plot(DrawTrigContext const *const context, TriggerParam const param,
  Vertex const scr_pos, bool const is_selected)
{
  plot_spr(context, get_sprite(param), scr_pos, is_selected);
}

void DrawTrig_plot_defence(DrawTrigContext const *const context,
  ObjRef const obj_ref, Vertex const scr_pos, bool const is_selected)
{
  plot_spr(context, get_def_sprite(obj_ref), scr_pos, is_selected);
}
