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
  size_t const width, size_t const height)
{
  DEBUGF("Copy and flip %zu x %zu bitmap\n", width, height);

  /* Append the raw bitmap to the output sprite, one row at a time
     (same pixel format etc).
     Note that the bitmap is flipped vertically during copying. */
  size_t const aligned_width = WORD_ALIGN_SZ(width);
  dst += height * aligned_width;

  for (size_t row = 0; row < height; ++row)
  {
    dst -= aligned_width;
    if (!reader_fread(dst, aligned_width, 1, reader))
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

  char numstr[32];
  int nout = sprintf(numstr, "%d", map_ref_to_num(tile_num));
  assert(nout >= 0); /* no formatting error */
  assert(nout < SpriteNameSize);
  NOT_USED(nout);

  if (!SprMem_create_sprite(&tiles->sprites[MapAngle_North][0].obj, numstr, false,
         (Vertex){MapTexSize, MapTexSize}, MapTexModeNumber))
  {
    return SFERROR(AlreadyReported);
  }

  _Optional SpriteHeader *const spr = SprMem_get_sprite_address(&tiles->sprites[MapAngle_North][0].obj, numstr);
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

  int red_total = 0, green_total = 0, blue_total = 0;

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

  SprMem_put_sprite_address(&tiles->sprites[MapAngle_North][0].obj, &*spr);

  /* Divide by number of pixels to get averages for tile */
  red_total /= MapTexSize * MapTexSize;
  green_total /= MapTexSize * MapTexSize;
  blue_total /= MapTexSize * MapTexSize;

  int const bright =
    rgb_brightness(red_total, green_total, blue_total);

  DEBUG("Average colour for sprite %d is %d,%d,%d (brightness %d)",
        map_ref_to_num(tile_num), red_total, green_total, blue_total, bright);

  unsigned char const index = map_ref_to_num(tile_num);
  unsigned int const bit = 1u << (index % CHAR_BIT);
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
  int const nearest = nearest_palette_entry_rgb(
    *palette, ARRAY_SIZE(*palette),
    red_total, green_total, blue_total);

  avcols_table[index] = (unsigned char)nearest;
  assert(nearest == avcols_table[index]);
  return SFERROR(OK);
}

#include "MapTexBitmInternal.h"

/* ---------------- Public functions ---------------- */

int MapTexBitmaps_get_count(const MapTexBitmaps *const tiles)
{
  assert(tiles != NULL);
  assert(tiles->count > 0);
  assert(tiles->count < MapTexMax);
  DEBUG_VERBOSEF("No. of tiles is %d\n", tiles->count);
  return tiles->count;
}

void MapTexBitmaps_init(MapTexBitmaps *const tiles)
{
  assert(tiles != NULL);
  *tiles = (MapTexBitmaps){
    .avcols_table = (void *)NULL,
    .bw_table = (void *)NULL,
  };
}

SFError MapTexBitmaps_read(MapTexBitmaps *const tiles, Reader *const reader)
{
  MapTexBitmaps_free(tiles);
  MapTexBitmaps_init(tiles);

  int const level = 0;
  if (!SprMem_init(&tiles->sprites[MapAngle_North][level].obj, 0))
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
  assert(hdr.last_tile_num <= INT_MAX - 1);
  tiles->count = hdr.last_tile_num + 1;

  if (!flex_alloc(&tiles->avcols_table, tiles->count))
  {
    return SFERROR(NoMem);
  }

  if (!flex_alloc(&tiles->bw_table, (tiles->count + CHAR_BIT - 1) / CHAR_BIT))
  {
    flex_free(&tiles->avcols_table);
    return SFERROR(NoMem);
  }

  hourglass_on();

  for (int tile_num = 0; tile_num < tiles->count && !SFError_fail(err); ++tile_num)
  {
    hourglass_percentage((tile_num * 100) / tiles->count);
    assert(tile_num <= UCHAR_MAX);
    err = tile_to_sprite(reader, tiles, map_ref_from_num((unsigned char)tile_num));
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
        SprMem_destroy(&tiles->sprites[angle][level].obj);
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
  unsigned char const index = map_ref_to_num(tile_num);
  assert(index < tiles->count);

  char *const bw_table = tiles->bw_table;
  return TEST_BITS(bw_table[index / CHAR_BIT],
                   1u << (index % CHAR_BIT));
}

int MapTexBitmaps_get_average_colour(const MapTexBitmaps *const tiles, MapRef const tile_num)
{
  assert(tiles != NULL);
  unsigned char const index = map_ref_to_num(tile_num);
  assert(index < tiles->count);

  unsigned char *const avcols_table = tiles->avcols_table;
  return avcols_table[index];
}
