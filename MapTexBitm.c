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

#include "Flex.h"

#include "Reader.h"
#include "Writer.h"
#include "StrExtra.h"

/* My headers */
#include "PalEntry.h"
#include "Macros.h"
#include "Debug.h"
#include "SFSprConv.h"
#include "Utils.h"
#include "msgtrans.h"
#include "hourglass.h"
#include "SFInit.h"
#include "MapTexBDat.h"
#include "MapTexBitm.h"
#include "SFError.h"
#include "MapCoord.h"

/* Constant numeric values */
enum
{
  SprAreaHdrSize = sizeof(int32_t) * 4,
  SprHdrSize = sizeof(int32_t) * 11,
  MapTileBitmapSize = WORD_ALIGN(MapTexSize) * MapTexSize,
  MapAnimFrameCount = 4,
  MapAnimTriggerCount = 4,
  SpriteNameSize = 13, /* including terminator */
  TransformFixedPointOne = 1 << 16,
  TranslateFixedPointOne = 1 << 8,
};

typedef struct
{
  int32_t last_tile_num;
  uint8_t splash_anim_1[MapAnimFrameCount];
  uint8_t splash_anim_2[MapAnimFrameCount];
  uint8_t splash_2_triggers[MapAnimTriggerCount];
}
MapTilesHeader;

typedef struct ConvertIter
{
  int32_t pos;
  int32_t count;
  Reader *reader;
  Writer *sprites;
  SFError (*convert)(struct ConvertIter *iter);
} ConvertIter;

typedef struct {
  ConvertIter super;
} TilesToSpritesIter;

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static SFError read_fail(Reader *const reader)
{
  return reader_feof(reader) ? SFERROR(Trunc) : SFERROR(ReadFail);
}

/* ----------------------------------------------------------------------- */

static inline SFError copy_n_flip(Reader *const reader, uint8_t *dst,
  int const width, int const height)
{
  DEBUGF("Copy and flip %d x %d bitmap\n", width, height);

  /* Append the raw bitmap to the output sprite, one row at a time
     (same pixel format etc).
     Note that the bitmap is flipped vertically during copying. */
  dst += height * WORD_ALIGN(width);

  for (int row = 0; row < height; ++row)
  {
    dst -= WORD_ALIGN(width);
    if (!reader_fread(dst, (size_t)WORD_ALIGN(width), 1, reader))
    {
      return read_fail(reader);
    }
  }

  return SFERROR(OK);
}

/* ----------------------------------------------------------------------- */

static bool read_tiles_anim(MapTilesHeader *const hdr, Reader *const reader)
{
  assert(hdr);
  return reader_fread(hdr->splash_anim_1, sizeof(hdr->splash_anim_1), 1, reader) &&
         reader_fread(hdr->splash_anim_2, sizeof(hdr->splash_anim_2), 1, reader) &&
         reader_fread(hdr->splash_2_triggers, sizeof(hdr->splash_2_triggers), 1, reader);
}

/* ----------------------------------------------------------------------- */

static SFError read_tiles_hdr(MapTilesHeader *const hdr, Reader *const reader)
{
  assert(hdr);
  if (!reader_fread_int32(&hdr->last_tile_num, reader))
  {
    return read_fail(reader);
  }

  /* Check that the no. of tiles claimed to be in the file is sensible */
  int32_t const last_tile_num = hdr->last_tile_num;
  DEBUG("File contains %" PRId32 " tiles", last_tile_num + 1);
  if ((last_tile_num < 0) || (last_tile_num >= MapTexMax))
  {
    return SFERROR(BadNumTiles);
  }

  if (!read_tiles_anim(hdr, reader))
  {
    return read_fail(reader);
  }

  for (size_t i = 0; i < ARRAY_SIZE(hdr->splash_anim_1); ++i)
  {
    if (hdr->splash_anim_1[i] > last_tile_num)
    {
      return SFERROR(BadTileAnims);
    }
  }

  for (size_t i = 0; i < ARRAY_SIZE(hdr->splash_anim_2); ++i)
  {
    if (hdr->splash_anim_2[i] > last_tile_num)
    {
      return SFERROR(BadTileAnims);
    }
  }

  return SFERROR(OK);
}

/* ----------------------------------------------------------------------- */

static SFError tile_to_sprite(Reader *const reader, MapTexBitmaps *const tiles, MapRef const tile_num)
{
  assert(tiles);

  char name[SpriteNameSize] = {0};
  char numstr[16];
  int nout = sprintf(numstr, "%zu", map_ref_to_num(tile_num));
  assert(nout >= 0); /* no formatting error */
  NOT_USED(nout);
  strncat(name, numstr, sizeof(name) - 1);
  DEBUGF("Sprite name is %s\n", name);

  if (!SprMem_create_sprite(&tiles->sprites[MapAngle_North][0], name, false,
         (Vertex){MapTexSize, MapTexSize}, MapTexModeNumber))
  {
    return SFERROR(AlreadyReported);
  }

  SpriteHeader *const spr = SprMem_get_sprite_address(&tiles->sprites[MapAngle_North][0], name);
  if (!spr)
  {
    return SFERROR(AlreadyReported);
  }

  uint8_t *const dst = (uint8_t *)spr + spr->image;

  SFError const err = copy_n_flip(reader, dst, MapTexSize, MapTexSize);
  if (SFError_fail(err))
  {
    return err;
  }

  unsigned int red_total = 0, green_total = 0, blue_total = 0;

  /* Total the R,G,B components across all pixels */
  for (int y = 0; y < MapTexSize; ++y)
  {
    for (int x = 0; x < MapTexSize; ++x)
    {
      int const pix = dst[(y * MapTexSize) + x];
      PaletteEntry const palette_entry = (*palette)[pix];

      red_total += PALETTE_GET_RED(palette_entry);
      green_total += PALETTE_GET_GREEN(palette_entry);
      blue_total += PALETTE_GET_BLUE(palette_entry);
    }
  }

  SprMem_put_sprite_address(&tiles->sprites[MapAngle_North][0], spr);

  /* Divide by number of pixels to get averages for tile */
  red_total /= MapTexSize * MapTexSize;
  green_total /= MapTexSize * MapTexSize;
  blue_total /= MapTexSize * MapTexSize;

  unsigned int const bright =
    rgb_brightness(red_total, green_total, blue_total);

  DEBUG("Average colour for sprite %zu is %u,%u,%u (brightness %u)",
        map_ref_to_num(tile_num), red_total, green_total, blue_total, bright);

  size_t const index = map_ref_to_num(tile_num);
  size_t const bit = 1u << (index % CHAR_BIT);
  char *const bw_table = tiles->bw_table;
  if (bright > MaxBrightness/2)
  {
    SET_BITS(bw_table[index / CHAR_BIT], bit);
  }
  else
  {
    CLEAR_BITS(bw_table[index / CHAR_BIT], bit);
  }

  /* Find nearest colour and write to table */
  unsigned char *const avcols_table = tiles->avcols_table;
  assert(index < tiles->count);
  avcols_table[index] = nearest_palette_entry_rgb(
    *palette, ARRAY_SIZE(*palette),
    (unsigned int)red_total, (unsigned int)green_total, (unsigned int)blue_total);

  return SFERROR(OK);
}

#ifndef NDEBUG
static void dump_sprites(const MapTexBitmaps *const tiles, MapAngle const angle, int const level)
{
#define FILENAME_PREFIX "<Wimp$ScrapDir>.tile_sprites"
  SprMem_verify(&tiles->sprites[angle][level]);

  if (tiles->count > 0)
  {
    char filename[sizeof(FILENAME_PREFIX) + 16];
    sprintf(filename, FILENAME_PREFIX"%d%d", angle, level);
    SprMem_save(&tiles->sprites[angle][level], filename);
  }
}
#else
#define dump_sprites(tiles, angle, level)
#endif

/* ---------------- Public functions ---------------- */

size_t MapTexBitmaps_get_count(const MapTexBitmaps *const tiles)
{
  assert(tiles != NULL);
  assert(tiles->count > 0);
  DEBUG_VERBOSEF("No. of tiles is %zu\n", tiles->count);
  return tiles->count;
}

void MapTexBitmaps_init(MapTexBitmaps *const tiles)
{
  assert(tiles != NULL);
  *tiles = (MapTexBitmaps){0};
}

SFError MapTexBitmaps_read(MapTexBitmaps *const tiles, Reader *const reader)
{
  MapTexBitmaps_free(tiles);
  MapTexBitmaps_init(tiles);

  int const level = 0;
  if (!SprMem_init(&tiles->sprites[MapAngle_North][level], 0))
  {
    return SFERROR(AlreadyReported);
  }
  tiles->have_sprites[MapAngle_North][level] = true;

  MapTilesHeader hdr = {0};
  SFError err = read_tiles_hdr(&hdr, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  assert(hdr.last_tile_num >= 0);
  tiles->count = (size_t)hdr.last_tile_num + 1;

  if (!flex_alloc(&tiles->avcols_table, (int)tiles->count))
  {
    return SFERROR(NoMem);
  }

  if (!flex_alloc(&tiles->bw_table, ((int)tiles->count + CHAR_BIT - 1) / CHAR_BIT))
  {
    flex_free(&tiles->avcols_table);
    return SFERROR(NoMem);
  }

  hourglass_on();

  for (size_t tile_num = 0; tile_num < tiles->count && !SFError_fail(err); ++tile_num)
  {
    hourglass_percentage((int)((tile_num * 100) / tiles->count));
    err = tile_to_sprite(reader, tiles, map_ref_from_num(tile_num));
  }

  dump_sprites(tiles, MapAngle_North, 0);
  hourglass_off();

  return err;
}

void MapTexBitmaps_free(MapTexBitmaps *const tiles)
{
  assert(tiles != NULL);

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    for (int level = 0; level <= MapTexSizeLog2; ++level) {
      if (tiles->have_sprites[angle][level])
      {
        SprMem_destroy(&tiles->sprites[angle][level]);
      }
    }
  }

  if (tiles->avcols_table)
  {
    flex_free(&tiles->avcols_table);
  }

  if (tiles->bw_table)
  {
    flex_free(&tiles->bw_table);
  }
}

bool MapTexBitmaps_is_bright(const MapTexBitmaps *const tiles, MapRef const tile_num)
{
  assert(tiles != NULL);
  size_t const index = map_ref_to_num(tile_num);
  assert(index < tiles->count);

  char *const bw_table = tiles->bw_table;
  return TEST_BITS(bw_table[index / CHAR_BIT],
                   1u << (index % CHAR_BIT));
}

int MapTexBitmaps_get_average_colour(const MapTexBitmaps *const tiles, MapRef const tile_num)
{
  assert(tiles != NULL);
  size_t const index = map_ref_to_num(tile_num);
  assert(index < tiles->count);

  unsigned char *const avcols_table = tiles->avcols_table;
  return avcols_table[index];
}

static bool make_mip_level(MapTexBitmaps *const tiles, MapAngle const angle, int const level)
{
  assert(tiles);
  assert(level >= 0); // function doesn't upscale textures

  Vertex const size = {MapTexSize >> level, MapTexSize >> level};
  int const dst_stride = WORD_ALIGN(size.x);
  int const src_stride = WORD_ALIGN(MapTexSize);
  Vertex const pix_size = {1 << level, 1 << level};
  unsigned int const sample_count = 1u << (2 * level);

  SprMem *const sm = &tiles->sprites[angle][level];

  if (!SprMem_init(sm, 0)) {
    return false;
  }

  hourglass_on();

  for (size_t tile_num = 0; tile_num < tiles->count; ++tile_num)
  {
    hourglass_percentage((int)((tile_num * 100) / tiles->count));

    char name[SpriteNameSize] = {0};
    char numstr[16];
    int nout = sprintf(numstr, "%zu", tile_num);
    assert(nout >= 0); /* no formatting error */
    NOT_USED(nout);
    strncat(name, numstr, sizeof(name) - 1);

    if (!SprMem_create_sprite(sm, name, false, size, MapTexModeNumber))
    {
      hourglass_off();
      SprMem_destroy(sm);
      return false;
    }

    SpriteHeader *const src_spr = SprMem_get_sprite_address(&tiles->sprites[angle][0], name);
    SpriteHeader *const dst_spr = SprMem_get_sprite_address(&tiles->sprites[angle][level], name);

    if (!src_spr || !dst_spr) {
      if (src_spr) {
        SprMem_put_sprite_address(&tiles->sprites[angle][0], src_spr);
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
        unsigned int red_total = 0, green_total = 0, blue_total = 0;

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

        dst[(y * dst_stride) + x] = nearest_palette_entry_rgb(
          *palette, ARRAY_SIZE(*palette),
          red_total, green_total, blue_total);

        ox += pix_size.x;
      }
      oy += pix_size.y;
    }

    SprMem_put_sprite_address(&tiles->sprites[angle][level], dst_spr);
    SprMem_put_sprite_address(&tiles->sprites[angle][0], src_spr);
  }

  dump_sprites(tiles, angle, level);
  hourglass_off();

  return true;
}

SprMem *MapTexBitmaps_get_sprites(MapTexBitmaps *const tiles, MapAngle angle, int const level)
{
  assert(tiles != NULL);
  assert(angle >= 0);
  assert(angle < ARRAY_SIZE(tiles->sprites));
  assert(level >= 0); // function doesn't upscale textures

  // All angles look the same at the highest MIP level (one pixel per tile)
  if (level == MapTexSizeLog2) {
    angle = MapAngle_North;
  }

  SprMem *const sm = &tiles->sprites[angle][level];

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

    for (size_t tile_num = 0; tile_num < tiles->count; ++tile_num)
    {
      hourglass_percentage((int)((tile_num * 100) / tiles->count));

      char name[SpriteNameSize] = {0};
      char numstr[16];
      int nout = sprintf(numstr, "%zu", tile_num);
      assert(nout >= 0); /* no formatting error */
      NOT_USED(nout);
      strncat(name, numstr, sizeof(name) - 1);

      if (!SprMem_create_sprite(sm, name, false, size, MapTexModeNumber) ||
          !SprMem_output_to_sprite(sm, name))
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
        SprMem_plot_trans_matrix_sprite(&tiles->sprites[MapAngle_North][level], name,
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
        SprMem_plot_trans_quad_sprite(&tiles->sprites[MapAngle_North][level], name,
                                      NULL,
                                      SPRITE_ACTION_OVERWRITE, &quad, NULL);
#endif
      } else {
        SprMem_plot_sprite(&tiles->sprites[MapAngle_North][level], name,
                           (Vertex){0,0}, SPRITE_ACTION_OVERWRITE);
      }
      SprMem_restore_output(sm);

      // SpriteExtend doesn't actually seem capable of rotation without distortion
      if (angle == MapAngle_South || angle == MapAngle_West) {
        SprMem_flip(sm, name);
      }
    }

    dump_sprites(tiles, angle, level);
    hourglass_off();

    tiles->have_sprites[angle][level] = true;
  }
  return tiles->have_sprites[angle][level] ? sm : NULL;
}
