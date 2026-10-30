/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map texture bitmaps
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

/* ANSI headers */
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#include "flex.h"

#include "Reader.h"
#include "Writer.h"

/* My headers */
#include "PalEntry.h"
#include "Macros.h"
#include "Debug.h"
#include "SFSprConv.h"
#include "Utils.h"
#include "msgtrans.h"
#include "Hourglass.h"
#include "SFInit.h"
#include "MapTexBDat.h"
#include "MapTexBitm.h"
#include "SFError.h"
#include "MapCoord.h"

#ifdef USE_OPTIONAL
#include "Optional.h"
#endif

/* Constant numeric values */
enum
{
  SpriteNameSize = 13, /* including terminator */
  TransformFixedPointOne = 1 << 16,
  TranslateFixedPointOne = 1 << 8,
};

#include "MapTexBitmInternal.h"

static bool make_mip_level(MapTexBitmaps *const tiles, MapAngle const angle, int const level)
{
  assert(tiles);
  assert(level >= 0); // function doesn't upscale textures

  Vertex const size = {MapTexSize >> level, MapTexSize >> level};
  int const dst_stride = WORD_ALIGN(size.x);
  int const src_stride = WORD_ALIGN(MapTexSize);
  Vertex const pix_size = {1 << level, 1 << level};
  int const sample_count = 1 << (2 * level);

  SprMem *const sm = &tiles->sprites[angle][level].obj;

  if (!SprMem_init(sm, 0)) {
    return false;
  }

  hourglass_on();

  for (int tile_num = 0; tile_num < tiles->count; ++tile_num)
  {
    hourglass_percentage((int)((tile_num * 100) / tiles->count));

    char numstr[32];
    int nout = sprintf(numstr, "%d", tile_num);
    assert(nout >= 0); /* no formatting error */
    assert(nout < SpriteNameSize);
    NOT_USED(nout);

    if (!SprMem_create_sprite(sm, numstr, false, size, MapTexModeNumber))
    {
      hourglass_off();
      SprMem_destroy(sm);
      return false;
    }

    _Optional SpriteHeader *const src_spr = SprMem_get_sprite_address(&tiles->sprites[angle][0].obj, numstr);
    _Optional SpriteHeader *const dst_spr = SprMem_get_sprite_address(&tiles->sprites[angle][level].obj, numstr);

    if (!src_spr || !dst_spr) {
      if (src_spr) {
        SprMem_put_sprite_address(&tiles->sprites[angle][0].obj, &*src_spr);
      }
      hourglass_off();
      SprMem_destroy(sm);
      return false;
    }

    uint8_t const *const src = (uint8_t *)src_spr + src_spr->image;
    uint8_t *const dst = (uint8_t *)dst_spr + dst_spr->image;

    /* Total the R,G,B components across all pixels */
    int oy = 0;
    for (int y = 0; y < size.y; ++y) {
      int ox = 0;
      for (int x = 0; x < size.x; ++x) {
        int red_total = 0, green_total = 0, blue_total = 0;

        for (int py = 0; py < pix_size.y; ++py) {
          for (int px = 0; px < pix_size.x; ++px) {
            int const pix = src[((oy + py) * src_stride) + (ox + px)];

            PaletteEntry const palette_entry = (*palette)[pix];

            red_total += PALETTE_GET_RED(palette_entry);
            green_total += PALETTE_GET_GREEN(palette_entry);
            blue_total += PALETTE_GET_BLUE(palette_entry);
          }
        }

        red_total /= sample_count;
        green_total /= sample_count;
        blue_total /= sample_count;

        int const nearest = nearest_palette_entry_rgb(
          *palette, ARRAY_SIZE(*palette),
          red_total, green_total, blue_total);

        assert((unsigned char)nearest == nearest);
        dst[(y * dst_stride) + x] = (unsigned char)nearest;

        ox += pix_size.x;
      }
      oy += pix_size.y;
    }

    SprMem_put_sprite_address(&tiles->sprites[angle][level].obj, &*dst_spr);
    SprMem_put_sprite_address(&tiles->sprites[angle][0].obj, &*src_spr);
  }

  dump_sprites(tiles, angle, level);
  hourglass_off();

  return true;
}

_Optional SprMem *MapTexBitmaps_get_sprites(MapTexBitmaps *const tiles, MapAngle angle, int const level)
{
  assert(tiles != NULL);
  assert(angle >= 0);
  assert(angle < ARRAY_SIZE(tiles->sprites));
  assert(level >= 0); // function doesn't upscale textures

  // All angles look the same at the highest MIP level (one pixel per tile)
  if (level == MapTexSizeLog2) {
    angle = MapAngle_North;
  }

  SprMem *const sm = &tiles->sprites[angle][level].obj;

  if (!tiles->have_sprites[MapAngle_North][level]) {
    if (!make_mip_level(tiles, MapAngle_North, level)) {
      return NULL;
    }
    tiles->have_sprites[MapAngle_North][level] = true;
  }

  if (!tiles->have_sprites[angle][level]) {
    assert(angle != MapAngle_North || level != 0);

    if (!SprMem_init(sm, 0)) {
      return NULL;
    }

    Vertex const size = {MapTexSize >> level, MapTexSize >> level};

    hourglass_on();

    for (int tile_num = 0; tile_num < tiles->count; ++tile_num)
    {
      hourglass_percentage((int)((tile_num * 100) / tiles->count));

      char numstr[32];
      int nout = sprintf(numstr, "%d", tile_num);
      assert(nout >= 0); /* no formatting error */
      assert(nout < SpriteNameSize);
      NOT_USED(nout);

      if (!SprMem_create_sprite(sm, numstr, false, size, MapTexModeNumber) ||
          !SprMem_output_to_sprite(sm, numstr))
      {
        hourglass_off();
        SprMem_destroy(sm);
        return NULL;
      }

      if (angle == MapAngle_East || angle == MapAngle_West) {
#if 1
        TransformMatrix const matrix = {
          .xxmul = 0,
          .xymul = TransformFixedPointOne,
          .yxmul = -TransformFixedPointOne,
          .yymul = 0,
          .xadd = (TranslateFixedPointOne << MapTexModeXEig) * size.y,
        };
        SprMem_plot_trans_matrix_sprite(&tiles->sprites[MapAngle_North][level].obj, numstr,
                                        NULL,
                                        SPRITE_ACTION_OVERWRITE, &matrix, NULL);
#else
        TransformQuad const quad = {
          .coords = {
            {(TranslateFixedPointOne << MapTexModeXEig) * 0,
             (TranslateFixedPointOne << MapTexModeYEig) * 0},
            {(TranslateFixedPointOne << MapTexModeXEig) * 0,
             (TranslateFixedPointOne << MapTexModeYEig) * size.y},
            {(TranslateFixedPointOne << MapTexModeXEig) * size.x,
             (TranslateFixedPointOne << MapTexModeYEig) * size.y},
            {(TranslateFixedPointOne << MapTexModeXEig) * size.x,
             (TranslateFixedPointOne << MapTexModeYEig) * 0},
          }
        };
        SprMem_plot_trans_quad_sprite(&tiles->sprites[MapAngle_North][level].obj, name,
                                      NULL,
                                      SPRITE_ACTION_OVERWRITE, &quad, NULL);
#endif
      } else {
        SprMem_plot_sprite(&tiles->sprites[MapAngle_North][level].obj, numstr,
                           (Vertex){0,0}, SPRITE_ACTION_OVERWRITE);
      }
      SprMem_restore_output(sm);

      // SpriteExtend doesn't actually seem capable of rotation without distortion
      if (angle == MapAngle_South || angle == MapAngle_West) {
        SprMem_flip(sm, numstr);
      }
    }

    dump_sprites(tiles, angle, level);
    hourglass_off();

    tiles->have_sprites[angle][level] = true;
  }
  return tiles->have_sprites[angle][level] ? sm : NULL;
}
