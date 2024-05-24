/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map transfers
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

#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include "kernel.h"
#include "flex.h"

#include "SFformats.h" /* get Fednet filetype */
#include "Err.h"
#include "Msgtrans.h"
#include "Macros.h"
#include "StrExtra.h"
#include "hourglass.h"
#include "DateStamp.h"
#include "FileUtils.h"
#include "NoBudge.h"
#include "Debug.h"
#include "DirIter.h"
#include "OSSpriteOp.h"
#include "FOpenCount.h"
#include "PathTail.h"

#include "DFileUtils.h"
#include "SprMem.h"
#include "MTransfers.h"
#include "FilePaths.h"
#include "Utils.h"
#include "Session.h"
#include "Config.h"
#include "MapAnims.h"
#include "DrawTiles.h"
#include "MapEdit.h"
#include "MapEditChg.h"
#include "MapEditSel.h"
#include "MapCoord.h"
#include "MTransfersData.h"
#include "MapTexBDat.h"
#include "MapEditCtx.h"
#include "DataType.h"
#include "WriterGKC.h"
#include "WriterGkey.h"
#include "ReaderGkey.h"
#include "Map.h"
#include "CoarseCoord.h"
#include "StrDict.h"

#define TRANSFER_TAG "STMP"

enum
{
  PREALLOC_SIZE = 4096,
  HistoryLog2 = 9,
  TransferFormatWithAnims = 1,
  TransferFormatWithSizeMinus1 = 2,
  TransferFormatWithCompactAnims = 3,
  TransferFormatVersion = TransferFormatWithCompactAnims,
  TransferHasAnimations = 1,
  PreAnimPadding = 12,
  MapOffsetDivider = 4,
};

/* Holds data on a single transfer (also used for clipboard) */
struct MapTransfer
{
  struct DFile dfile;
  CoarsePoint2d  size_minus_one;
  void          *tiles, *anims; /* flex anchor */
  size_t         anim_count, anim_alloc;
};

typedef struct
{
  CoarsePoint2d coords;
  MapAnimParam param;
} MapTransferAnim;

/* ---------------- Private functions ---------------- */

static inline size_t uchar_offset(MapTransfer *const transfer,
  MapPoint const trans_pos)
{
  assert(transfer != NULL);
  assert(trans_pos.x >= 0);
  assert(trans_pos.x <= transfer->size_minus_one.x);
  assert(trans_pos.y >= 0);
  assert(trans_pos.y <= transfer->size_minus_one.y);

  size_t const offset = ((size_t)trans_pos.y * ((size_t)transfer->size_minus_one.x + 1)) +
                         (size_t)trans_pos.x;
  assert(offset < (size_t)flex_size(&transfer->tiles));
  return offset;
}

static inline int calc_map_size(CoarsePoint2d const size_minus_one)
{
  return (size_minus_one.x + 1) * (size_minus_one.y + 1);
}

static DrawTilesReadResult read_transfer_tile(void *const cb_arg, MapPoint const trans_pos)
{
  MapTransfer *const transfer = cb_arg;
  assert(transfer != NULL);

  DEBUG_VERBOSEF("Read %" PRIMapCoord ",%" PRIMapCoord
                 " in transfer %" PRIMapCoord ",%" PRIMapCoord "\n",
    trans_pos.x, trans_pos.y,
    MapTransfers_get_dims(transfer).x, MapTransfers_get_dims(transfer).y);

  return (DrawTilesReadResult){
    map_ref_from_num(((unsigned char *)transfer->tiles)[uchar_offset(transfer, trans_pos)])};
}

static void write_transfer_tile(MapTransfer *const transfer,
  MapPoint const trans_pos, MapRef const tile)
{
  assert(transfer != NULL);

  DEBUG_VERBOSEF("Write %" PRIMapCoord ",%" PRIMapCoord
                 " in transfer %" PRIMapCoord ",%" PRIMapCoord "\n",
    trans_pos.x, trans_pos.y,
    MapTransfers_get_dims(transfer).x, MapTransfers_get_dims(transfer).y);

  ((unsigned char *)transfer->tiles)[
    uchar_offset(transfer, trans_pos)] = map_ref_to_num(tile);
}

static int find_zoom(Vertex *const size, Vertex const target_size)
{
  /* Select zoom level to fit thumbnail size. Can't upscale textures here.
     (thumbnails are plotted as Wimp icons, so can't upscale at all actually.) */
  assert(size != NULL);
  assert(size->x >= 1);
  assert(size->y >= 1);
  assert(target_size.x >= 1);
  assert(target_size.y >= 1);
  DEBUGF("Trying to fit {%d,%d} px thumbnail into {%d,%d} px area\n",
         size->x, size->y, target_size.x, target_size.y);

  /* If plotted at the proposed zoom level, is the transfer too big? */
  int zoom_level;
  for (zoom_level = 0; /* first try a zoom level of 1:1 */
       zoom_level < 3 && (size->x > target_size.x || size->y > target_size.y);
       ++zoom_level)
  {
    /* Each zoom level increase halves the transfer size */
    *size = Vertex_div_log2(*size, 1);
  }

  DEBUGF("Size is {%d,%d} px at zoom level %d\n", size->x, size->y, zoom_level);
  return zoom_level;
}

static void delete_thumbnail(MapTransfers *const transfers_data,
  MapTransfer *const transfer)
{
  /* Delete the thumbnail sprite corresponding to a given transfer */
  assert(transfers_data != NULL);

  if (!transfers_data->have_thumbnails)
  {
    DEBUG("Can't delete transfer thumbnail - no sprites!");
    return;
  }

  assert(transfer != NULL);
  char const *const spr_name = get_leaf_name(&transfer->dfile);
  DEBUG("Deleting transfer thumbnail sprite '%s'", spr_name);
  SprMem_delete(&transfers_data->thumbnail_sprites, spr_name);
}

static bool make_transfer_thumbnail(MapTransfers *const transfers_data,
  MapTransfer *const transfer, MapTexBitmaps *const textures)
{
  assert(transfer != NULL);
  assert(transfers_data != NULL);
  assert(textures != NULL);

  DEBUG("About to create thumbnail for transfer '%s'", dfile_get_name(&transfer->dfile));

  /* Create a thumbnail sprite for a new transfer */
  MapPoint const size_in_tiles = MapTransfers_get_dims(transfer);

  static Vertex const tile_size = {MapTexSize, MapTexSize};
  static Vertex const target_size = {TransfersThumbWidth, TransfersThumbHeight};
  Vertex thumbnail_size = Vertex_mul(MapPoint_to_vertex(size_in_tiles), tile_size);
  int const thumb_zoom = find_zoom(&thumbnail_size, target_size);

  /* Create thumbnail sprite */
  char const *const spr_name = get_leaf_name(&transfer->dfile);
  if (!SprMem_create_sprite(&transfers_data->thumbnail_sprites,
          spr_name, false, thumbnail_size, DrawTilesModeNumber))
  {
    DEBUG("Failed to create sprite");
    return false; /* failure */
  }

  MapArea const scr_area = {{0,0}, {size_in_tiles.x-1, size_in_tiles.y-1}};

  /* Paint to thumbnail sprite */
  bool const needs_mask = DrawTiles_to_sprite(
    textures,
    &transfers_data->thumbnail_sprites,
    spr_name,
    MapAngle_North, &scr_area,
    read_transfer_tile,
    transfer,
    thumb_zoom,
    NULL /* no colour translation */);

  if (needs_mask) {
    /* Create thumbnail mask (with all pixels solid) */
    if (!SprMem_create_mask(&transfers_data->thumbnail_sprites, spr_name))
      return false;

    /* Paint to thumbnail mask */
    DrawTiles_to_mask(&transfers_data->thumbnail_sprites,
                      spr_name, MapAngle_North, &scr_area, read_transfer_tile,
                      transfer, thumb_zoom);
  }

  return true;
}

static bool make_thumbnails(MapTransfers *const transfers_data,
  MapTexBitmaps *const textures)
{
  assert(transfers_data);
  assert(textures);

  hourglass_on();
  size_t count = 0;
  bool success = true;

  StrDictVIter iter;
  for (MapTransfer *transfer = strdictviter_all_init(&iter, &transfers_data->dict);
       transfer != NULL;
       transfer = strdictviter_advance(&iter)) {
    if (count <= INT_MAX / 100)
    {
      hourglass_percentage((int)((count * 100) / transfers_data->count));
    }
    ++count;

    if (!make_transfer_thumbnail(transfers_data, transfer, textures))
    {
      success = false;
      break;
    }
  }
  hourglass_off();

  SprMem_minimize(&transfers_data->thumbnail_sprites);

#ifndef NDEBUG
  SprMem_verify(&transfers_data->thumbnail_sprites);

  if (transfers_data->count > 0)
    SprMem_save(&transfers_data->thumbnail_sprites, "transfers_thumbnails");
#endif

  return success;
}

static bool add_to_list(MapTransfers *const transfers_data,
  MapTransfer *const transfer, size_t *const index)
{
  assert(transfers_data != NULL);
  assert(transfer != NULL);
  DEBUG("Adding transfer '%s'", get_leaf_name(&transfer->dfile));
  // Careful! Key string isn't copied on insertion.
  size_t new_index;
  if (!strdict_insert(&transfers_data->dict, get_leaf_name(&transfer->dfile), transfer, &new_index)) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  transfers_data->count ++;
  DEBUG("MTransfers list now has %zu members", transfers_data->count);
  if (index) {
    assert(new_index <= INT_MAX);
    *index = new_index;
  }
  return true;
}

static void remove_from_list(MapTransfers *const transfers_data,
  MapTransfer *const transfer)
{
  assert(transfers_data != NULL);
  assert(transfer != NULL);
  MapTransfer *const removed = strdict_remove_value(&transfers_data->dict, get_leaf_name(&transfer->dfile), NULL);
  assert(removed == transfer);
  NOT_USED(removed);
  assert(transfers_data->count > 0);
  transfers_data->count --;
  DEBUG("Number of transfers in list is now %zu", transfers_data->count);
}

static bool transfer_pre_alloc(MapTransfer *const transfer, size_t const min_alloc)
{
  assert(transfer);
  assert(transfer->anim_count <= transfer->anim_alloc);

  if (transfer->anim_alloc < min_alloc) {
    size_t const nbytes = sizeof(MapTransferAnim) * min_alloc;
    if (nbytes > INT_MAX) {
      return false;
    }
    if (transfer->anims) {
      assert(transfer->anim_alloc > 0);
      if (!flex_extend(&transfer->anims, (int)nbytes)) {
        return false;
      }
    } else {
      assert(transfer->anim_alloc == 0);
      if (!flex_alloc(&transfer->anims, (int)nbytes)) {
        return false;
      }
    }
    transfer->anim_alloc = min_alloc;
  }
  return true;
}

static void transfer_add_anim(MapTransfer *const transfer,
  MapTransferAnim const *const anim)
{
  assert(transfer);
  assert(transfer->anims);
  assert(transfer->anim_count < transfer->anim_alloc);
  assert(anim);
  assert(anim->coords.x <= transfer->size_minus_one.x);
  assert(anim->coords.y <= transfer->size_minus_one.y);

  ((MapTransferAnim *)transfer->anims)[transfer->anim_count++] = *anim;
}

static MapTransferAnim transfer_get_anim(MapTransfer const *const transfer, size_t const index)
{
  assert(transfer);
  assert(transfer->anims);
  assert(transfer->anim_count <= transfer->anim_alloc);
  assert(index >= 0);
  assert(index < transfer->anim_count);

  MapTransferAnim const *const anim = ((MapTransferAnim *)transfer->anims) + index;
  assert(anim->coords.x <= transfer->size_minus_one.x);
  assert(anim->coords.y <= transfer->size_minus_one.y);

  return *anim;
}

static void write_anims(MapTransfer const *const transfer,
  Writer *const writer)
{
  assert(transfer);

  long int const pos = writer_ftell(writer);
  if (pos >= 0) {
    writer_fseek(writer, WORD_ALIGN(pos), SEEK_SET);
  }

  assert(transfer->anim_count <= INT32_MAX);
  writer_fwrite_int32((int32_t)transfer->anim_count, writer);

  for (size_t a = 0; a < transfer->anim_count; ++a)
  {
    MapTransferAnim const anim = transfer_get_anim(transfer, a);

    CoarsePoint2d_write(anim.coords, writer);
    writer_fwrite_uint16(anim.param.period, writer);

    for (size_t i = 0; i < AnimsNFrames; ++i)
    {
      writer_fputc((int)map_ref_to_num(anim.param.tiles[i]), writer);
    }
  }
}

static void destroy_all(MapTransfer *const transfer)
{
  assert(transfer);

  if (transfer->tiles)
  {
    flex_free(&transfer->tiles);
  }

  if (transfer->anims)
  {
    flex_free(&transfer->anims);
  }
}

static bool alloc_transfer(MapTransfer *const transfer,
  CoarsePoint2d const size_minus_one)
{
  assert(transfer);
  transfer->size_minus_one = size_minus_one;
  return flex_alloc(&transfer->tiles, calc_map_size(size_minus_one));
}

static SFError read_anims(MapTransfer *const transfer, Reader *const reader,
  int const version)
{
  assert(transfer);
  assert(version >= TransferFormatWithAnims);

  /* We can expect animations data at the end of the map data */
  long int const pos = reader_ftell(reader);
  if (pos < 0)
  {
    return SFERROR(BadTell);
  }
  DEBUGF("Reading animations at %ld\n", pos);

  if (reader_fseek(reader, WORD_ALIGN(pos), SEEK_SET))
  {
    DEBUGF("Failed to skip unaligned data before animations\n");
    return SFERROR(BadSeek);
  }

  int32_t anim_count;
  if (!reader_fread_int32(&anim_count, reader))
  {
    return SFERROR(ReadFail);
  }

  DEBUGF("Transfer animations count %" PRId32 "\n", anim_count);

  if (anim_count < 0 || anim_count > AnimsMax)
  {
    return SFERROR(BadNumAnims);
  }

  if (!transfer_pre_alloc(transfer, (size_t)anim_count))
  {
    return SFERROR(NoMem);
  }

  if ((version < TransferFormatWithCompactAnims) &&
      reader_fseek(reader, PreAnimPadding, SEEK_CUR))
  {
    DEBUGF("Failed to skip padding before old animations format\n");
    return SFERROR(BadSeek);
  }

  for (int32_t a = 0; a < anim_count; ++a)
  {
    MapTransferAnim anim = {{0}};

    if (version < TransferFormatWithCompactAnims)
    {
      /* Older versions stored animations in the same (inefficient) format
         as the game */
      DEBUGF("Reading old animations format\n");
      int32_t map_offset;
      if (!reader_fread_int32(&map_offset, reader))
      {
        return SFERROR(ReadFail);
      }

      if (map_offset % MapOffsetDivider)
      {
        return SFERROR(BadAnimCoord);
      }
      map_offset /= MapOffsetDivider;

      if (map_offset < 0 || map_offset >= Map_Area)
      {
        return SFERROR(BadAnimCoord);
      }

      anim.coords = (CoarsePoint2d){
        .y = map_offset / Map_Size,
        .x = map_offset % Map_Size,
      };

      int32_t timer_counter;
      if (!reader_fread_int32(&timer_counter, reader))
      {
        return SFERROR(ReadFail);
      }

      uint16_t period;
      if (!reader_fread_uint16(&period, reader))
      {
        return SFERROR(ReadFail);
      }
      anim.param.period = period;

      if (timer_counter != period)
      {
        DEBUGF("timer_counter %" PRId32 ", period %" PRIu16 "\n", timer_counter, period);
        //return SFERROR(BadAnimTime);
        timer_counter = period;
      }

      uint16_t frame_num;
      if (!reader_fread_uint16(&frame_num, reader))
      {
        return SFERROR(ReadFail);
      }

      if (frame_num != 0)
      {
        DEBUGF("frame_num %" PRIu16 "\n", frame_num);
        //return SFERROR(BadAnimState);
        frame_num = 0;
      }

      for (size_t i = 0; i < AnimsNFrames; ++i)
      {
        int32_t tile;
        if (!reader_fread_int32(&tile, reader))
        {
          return SFERROR(ReadFail);
        }

        if (tile < 0 || (tile > Map_RefMax && tile != Map_RefMask))
        {
          return SFERROR(BadAnimFrame);
        }
        anim.param.tiles[i] = map_ref_from_num((uint32_t)tile);
      }
    }
    else
    {
      DEBUGF("Reading new animations format\n");
      if (!CoarsePoint2d_read(&anim.coords, reader))
      {
        return SFERROR(ReadFail);
      }

      uint16_t period;
      if (!reader_fread_uint16(&period, reader))
      {
        return SFERROR(ReadFail);
      }
      anim.param.period = period;

      for (size_t i = 0; i < AnimsNFrames; ++i)
      {
        int const tile = reader_fgetc(reader);
        if (tile == EOF)
        {
          return SFERROR(ReadFail);
        }

        if (tile < 0 || (tile > Map_RefMax && tile != Map_RefMask))
        {
          return SFERROR(BadAnimFrame);
        }
        anim.param.tiles[i] = map_ref_from_num((unsigned)tile);
      }
    }

    if (anim.coords.x > transfer->size_minus_one.x ||
        anim.coords.y > transfer->size_minus_one.y)
    {
      return SFERROR(BadAnimCoord);
    }

    transfer_add_anim(transfer, &anim);
  }
  return SFERROR(OK);
}

static SFError MapTransfer_read_cb(DFile const *const dfile,
  Reader *const reader)
{
  assert(dfile);
  MapTransfer *const transfer = CONTAINER_OF(dfile, MapTransfer, dfile);

  destroy_all(transfer);

  char tag[sizeof(TRANSFER_TAG)-1];
  if (!reader_fread(tag, sizeof(tag), 1, reader))
  {
    return SFERROR(ReadFail);
  }

  if (memcmp(TRANSFER_TAG, tag, sizeof(tag)))
  {
    return SFERROR(TransferNot);
  }

  int const version = reader_fgetc(reader);
  if (version == EOF)
  {
    return SFERROR(ReadFail);
  }

  if (version > TransferFormatVersion)
  {
    return SFERROR(TransferVer);
  }

  CoarsePoint2d size_minus_one = {0};
  if (!CoarsePoint2d_read(&size_minus_one, reader))
  {
    return SFERROR(ReadFail);
  }

  int flags = reader_fgetc(reader);
  if (flags == EOF)
  {
    return SFERROR(ReadFail);
  }

  /* Fix up differences between formats */
  if (version < TransferFormatWithAnims) {
    flags = 0;
    DEBUG("Clearing flags byte");
  }

  if (TEST_BITS(flags, ~TransferHasAnimations))
  {
    return SFERROR(TransferFla);
  }

  if (version < TransferFormatWithSizeMinus1)
  {
    if (size_minus_one.x == 0 || size_minus_one.y == 0) {
      return SFERROR(TransferSize); /* can't get this problem since version 2 */
    }

    DEBUG("Fixing up old-style dimensions");
    size_minus_one.x--;
    size_minus_one.y--;
  }

  DEBUGF("Transfer version %d, adjusted dimensions {%d,%d}, flags 0x%x\n",
         version, size_minus_one.x+1, size_minus_one.y+1, flags);

  if (!alloc_transfer(transfer, size_minus_one))
  {
    return SFERROR(NoMem);
  }

  SFError err = SFERROR(OK);

  nobudge_register(PREALLOC_SIZE);
  if (!reader_fread(transfer->tiles, (size_t)flex_size(&transfer->tiles), 1, reader))
  {
    err = SFERROR(ReadFail);
  }
  nobudge_deregister();

  if (!SFError_fail(err) && TEST_BITS(flags, TransferHasAnimations))
  {
    err = read_anims(transfer, reader, version);
  }

  return err;
}

static void MapTransfer_write_cb(DFile const *const dfile,
  Writer *const writer)
{
  assert(dfile);
  MapTransfer *const transfer = CONTAINER_OF(dfile, MapTransfer, dfile);

  writer_fwrite(TRANSFER_TAG, sizeof(TRANSFER_TAG)-1, 1, writer);
  writer_fputc(TransferFormatVersion, writer);
  CoarsePoint2d_write(transfer->size_minus_one, writer);
  writer_fputc(transfer->anim_count > 0 ? TransferHasAnimations : 0, writer);

  nobudge_register(PREALLOC_SIZE);
  writer_fwrite(transfer->tiles, (size_t)flex_size(&transfer->tiles), 1, writer);
  nobudge_deregister();

  if (transfer->anim_count > 0) {
    write_anims(transfer, writer);
  }
}

static void MapTransfer_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  MapTransfer *const transfer = CONTAINER_OF(dfile, MapTransfer, dfile);

  destroy_all(transfer);
  dfile_destroy(&transfer->dfile);
  free(transfer);
}

static void free_all_cb(char const *const key, void *const data, void *const arg)
{
  NOT_USED(key);
  MapTransfer *const transfer_to_delete = data;
  MapTransfers *const transfers_data = arg;
  delete_thumbnail(transfers_data, transfer_to_delete);
  dfile_release(&transfer_to_delete->dfile);
}

static void delete_transfer(MapTransfer *const transfer_to_delete,
  MapTransfers *const transfers_data)
{
  verbose_remove(dfile_get_name(&transfer_to_delete->dfile));
  delete_thumbnail(transfers_data, transfer_to_delete);
  dfile_release(&transfer_to_delete->dfile);
}

static void delete_all_cb(char const *const key, void *const data, void *const arg)
{
  NOT_USED(key);
  delete_transfer(data, arg);
}

/* ----------------- Public functions ---------------- */

DFile *MapTransfer_get_dfile(MapTransfer *const transfer)
{
  assert(transfer);
  return &transfer->dfile;
}

MapTransfer *MapTransfer_create(void)
{
  MapTransfer *const transfer = malloc(sizeof(*transfer));
  if (transfer == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }
  DEBUG ("New transfer list record is at %p", (void *)transfer);

  *transfer = (MapTransfer){{0}};

  dfile_init(&transfer->dfile, MapTransfer_read_cb,
             MapTransfer_write_cb, NULL, MapTransfer_destroy_cb);

  return transfer;
}

size_t MapTransfers_get_count(const MapTransfers *const transfers_data)
{
  assert(transfers_data != NULL);
  assert(transfers_data->count >= 0);
  DEBUG_VERBOSEF("No. of transfers is %zu\n", transfers_data->count);
  return transfers_data->count;
}

void MapTransfers_init(MapTransfers *const transfers_data)
{
  assert(transfers_data);

  *transfers_data = (MapTransfers){
    .count = 0,
    .have_thumbnails = false,
    .directory = NULL,
  };

  strdict_init(&transfers_data->dict);
}

void MapTransfers_load_all(MapTransfers *const transfers_data,
  char const *const tiles_set)
{
  DEBUG("Loading transfers for tiles set '%s'...", tiles_set);
  char *const dir = make_file_path_in_dir(Config_get_transfers_dir(), tiles_set);
  if (!dir) {
    return;
  }

  MapTransfers_free(transfers_data);
  MapTransfers_init(transfers_data);
  transfers_data->directory = dir;

  if (!file_exists(dir)) {
    return;
  }

  hourglass_on();

  DirIterator *iter = NULL;
  const _kernel_oserror *e = diriterator_make(&iter, 0, transfers_data->directory, NULL);
  int const expected_ftype = data_type_to_file_type(DataType_MapTransfer);
  for (;
       !E(e) && !diriterator_is_empty(iter);
       e = diriterator_advance(iter)) {

    DirIteratorObjectInfo info;
    int const object_type = diriterator_get_object_info(iter, &info);

    /* Check that file is of correct type */
    if ((object_type != ObjectType_File) || (info.file_type != expected_ftype)) {
      continue;
    }

    /* Check that filename is within length limit */
    Filename filename;
    if (diriterator_get_object_leaf_name(iter, filename, sizeof(filename)) >
        sizeof(Filename)-1) {
      DEBUGF("%s exceeds the character limit.\n", filename);
      continue;
    }
    DEBUG("File name is '%s'", filename);

    /* Load tiles transfer */
    size_t const n = diriterator_get_object_path_name(iter, NULL, 0);
    {
      char *const full_path = malloc(n + 1);
      if (!full_path) {
        report_error(SFERROR(NoMem), "", "");
        break;
      }
      (void)diriterator_get_object_path_name(iter, full_path, n + 1);

      MapTransfer *const transfer = MapTransfer_create();
      if (!transfer) {
        free(full_path);
        break;
      }

      if (report_error(load_compressed(&transfer->dfile, full_path), full_path, "")) {
        dfile_release(&transfer->dfile);
        free(full_path);
        break;
      }

      int tmp[2] = {0};
      memcpy(tmp, &info.date_stamp, sizeof(info.date_stamp));

      if (!dfile_set_saved(&transfer->dfile, full_path, tmp)) {
        report_error(SFERROR(NoMem), "", "");
        dfile_release(&transfer->dfile);
        free(full_path);
        break;
      }

      free(full_path);

      if (!add_to_list(transfers_data, transfer, NULL)) {
        dfile_release(&transfer->dfile);
        break;
      }
    }
  }

  DEBUG("Number of transfers in list is %zu", transfers_data->count);
  diriterator_destroy(iter);
  hourglass_off();
}

void MapTransfers_open_dir(MapTransfers const *const transfers_data)
{
  assert(transfers_data);
  if (transfers_data->directory) {
    open_dir(transfers_data->directory);
  }
}

void MapTransfers_free(MapTransfers *const transfers_data)
{
  DEBUG("Destroying transfers attached to tiles data %p",
        (void *)transfers_data);

  assert(transfers_data != NULL);

  strdict_destroy(&transfers_data->dict, free_all_cb, transfers_data);

  FREE_SAFE(transfers_data->directory);

  if (transfers_data->have_thumbnails) {
    SprMem_destroy(&transfers_data->thumbnail_sprites);
  }

}

bool MapTransfers_ensure_thumbnails(MapTransfers *const transfers_data,
  MapTexBitmaps *const textures)
{
  /* N.B. Although we are lazy about creating the thumbnail sprites, we still
     want to be able to share them with all other sessions using this tile set.
     Therefore they are part of the _MapTransfers structure */

  assert(transfers_data != NULL);
  if (transfers_data->have_thumbnails) {
    DEBUG("Transfer thumbnails already exist for tile set %p", (void *)transfers_data);
    return true; /* We already have thumbnail sprites */
  }

  DEBUG("Creating thumbnails of transfers for tile set %p", (void *)transfers_data);
  if (!SprMem_init(&transfers_data->thumbnail_sprites, 0)) {
    return false;
  }

  bool const success = make_thumbnails(transfers_data, textures);
  if (!success) {
    SprMem_destroy(&transfers_data->thumbnail_sprites);
  } else {
    transfers_data->have_thumbnails = true;
  }

  return success;
}

MapTransfer *MapTransfers_grab_selection(const MapEditContext *const map,
  MapEditSelection *const selected)
{
  /* Find bounding box covering all selected tiles */
  MapArea bounds;
  if (!MapEditSelection_get_bounds(selected, &bounds)) {
    DEBUG ("Nothing selected!");
    return NULL; /* nothing selected! */
  }

  /* Create a new transfer record */
  MapPoint const size = MapPoint_sub(bounds.max, bounds.min);
  MapTransfer *const transfer = MapTransfer_create();
  if (transfer == NULL) {
    return NULL;
  }

  CoarsePoint2d const size_minus_one = {size.x, size.y};
  if (!alloc_transfer(transfer, size_minus_one))
  {
    report_error(SFERROR(NoMem), "", "");
    dfile_release(&transfer->dfile);
    return NULL;
  }

  /* Copy selected tiles to transfer. It's tempting to use
     MapEditSelection_for_each but we'd have to store the mask separately. */
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, &bounds);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    MapRef tile = map_ref_mask();
    if (MapEditSelection_is_selected(selected, p)) {
      tile = MapEdit_read_tile(map, p);
    }
    write_transfer_tile(transfer, MapPoint_sub(p, bounds.min), tile);
  }

  size_t sel_count = 0;
  if (map->anims != NULL) {
    MapArea bounds;
    if (!MapEditSelection_get_bounds(selected, &bounds)) {
      return 0; /* nothing selected! */
    }

    MapAnimsIter iter;
    for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &bounds, NULL);
         !MapAnimsIter_done(&iter);
         p = MapAnimsIter_get_next(&iter, NULL))
    {
      if (MapEditSelection_is_selected(selected, p)) {
        sel_count++;
      }
    }
  }

  if (sel_count > 0) {
    if (!transfer_pre_alloc(transfer, sel_count)) {
      report_error(SFERROR(NoMem), "", "");
      dfile_release(&transfer->dfile);
      return NULL;
    }

    MapTransferAnim anim = {{0}};
    MapAnimsIter iter;
    for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &bounds, &anim.param);
         !MapAnimsIter_done(&iter);
         p = MapAnimsIter_get_next(&iter, &anim.param))
    {
      if (MapEditSelection_is_selected(selected, p)) {
        /* The selection's wrapped bounding box may contain the coordinates of
           an animation even though those coordinates appear far outside the bounding box. */
        anim.coords = map_coords_to_coarse(map_coords_in_area(p, &bounds));
        transfer_add_anim(transfer, &anim);
      }
    }
  }

  return transfer;
}

static void for_each_area(MapTransfer *const transfer,
  void (*callback)(void *, MapArea const *), void *cb_arg)
{
  MapPoint const t_dims = MapTransfers_get_dims(transfer);

  MapArea area = {{0}};
  struct {
    bool pend_span_x:1;
    bool pend_span_xy:1;
    bool any_span_on_current_row:1;
  } state = {false, false, false};

  for (MapPoint trans_pos = {.y = 0}; trans_pos.y < t_dims.y; trans_pos.y++) {

    MapCoord start_x = -1; /* no non-mask tiles on this row yet */
    state.pend_span_x = false;
    state.any_span_on_current_row = false;

    for (trans_pos.x = 0; trans_pos.x <= t_dims.x; trans_pos.x++) {
      MapRef const tile_ref = trans_pos.x < t_dims.x ?
        read_transfer_tile(transfer, trans_pos).tile_ref : map_ref_mask();

      if (map_ref_is_mask(tile_ref)) {
        if (start_x >= 0) {
          /* Reached the first mask value beyond the end of a span of non-mask values */
          MapCoord const end_x = trans_pos.x - 1;
          DEBUGF("Span is x=%" PRIMapCoord ",%" PRIMapCoord "\n", start_x, end_x);
          if (state.pend_span_xy && area.min.x == start_x && area.max.x == end_x) {
            DEBUGF("Continuing block begun at y=%" PRIMapCoord "\n", area.min.y);
          } else {
            if (state.pend_span_xy) {
              DEBUGF("Emitting block begun at y=%" PRIMapCoord "\n", area.min.y);
              state.pend_span_xy = false;
              area.max.y = trans_pos.y - 1;
              callback(cb_arg, &area);
            }

            DEBUGF("Pending span {%" PRIMapCoord ",%" PRIMapCoord "} "
                   "begun at y=%" PRIMapCoord "\n", start_x, end_x, trans_pos.y);
            area.min.x = start_x;
            area.max.x = end_x;
            area.min.y = trans_pos.y;
            state.pend_span_x = true;
          }
          state.any_span_on_current_row = true;
          start_x = -1;
        }
      } else if (start_x < 0) {
        /* Found the start of a span of non-mask values */
        DEBUGF("Start of a span at x=%" PRIMapCoord "\n", trans_pos.x);
        if (state.pend_span_x) {
          DEBUGF("Emitting span {%" PRIMapCoord ",%" PRIMapCoord "} "
                 "begun at y=%" PRIMapCoord "\n", area.min.x, area.max.x, area.min.y);
          state.pend_span_x = false;
          area.max.y = trans_pos.y;
          callback(cb_arg, &area);
        } else if (state.any_span_on_current_row && state.pend_span_xy) {
          /* Blocks of non-mask values can't be pending across rows that contain
             other (non-contiguous) spans of non-mask values */
          DEBUGF("Emitting block {%" PRIMapCoord ",%" PRIMapCoord "} "
                 "begun at y=%" PRIMapCoord "\n", area.min.x, area.max.x, area.min.y);
          state.pend_span_xy = false;
          area.max.y = trans_pos.y;
          callback(cb_arg, &area);
        }
        start_x = trans_pos.x;
      }
    } /* next column */

    if (state.pend_span_x) {
      /* The last span on each line can be continued on the next */
      DEBUGF("Upgrading pending span to pending block {%" PRIMapCoord ",%" PRIMapCoord "} "
             "begun at y=%" PRIMapCoord "\n", area.min.x, area.max.x, area.min.y);
      assert(state.any_span_on_current_row);
      state.pend_span_xy = true;
    } else if (!state.any_span_on_current_row && state.pend_span_xy) {
      /* Blocks of non-mask values can't be pending across fully masked rows */
      DEBUGF("Empty row: emitting block {%" PRIMapCoord ",%" PRIMapCoord "} "
             "begun at y=%" PRIMapCoord "\n", area.min.x, area.max.x, area.min.y);
      state.pend_span_xy = false;
      area.max.y = trans_pos.y - 1;
      callback(cb_arg, &area);
    }
  } /* next row */

  if (state.pend_span_xy) {
    DEBUGF("Emitting last block begun at y=%" PRIMapCoord "\n", area.min.y);
    area.max.y = t_dims.y - 1;
    callback(cb_arg, &area);
  }
}

typedef struct {
  MapTransfer *transfer;
  MapPoint offset;
} ReadOffsetData;

static MapRef read_offset_transfer_tile(void *const cb_arg, MapPoint const trans_pos)
{
  const ReadOffsetData *const data = cb_arg;
  assert(data != NULL);
  DrawTilesReadResult const value = read_transfer_tile(data->transfer, MapPoint_add(trans_pos, data->offset));
  assert(!map_ref_is_mask(value.tile_ref));
  return value.tile_ref;
}

typedef struct {
  const MapEditContext *map;
  MapPoint const t_pos_on_map;
  MapTransfer *transfer;
  MapEditSelection *selection;
  MapEditChanges *change_info;
} PlotToMapData;

static void plot_to_map_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  PlotToMapData const *const data = cb_arg;

  MapArea map_area;
  MapArea_translate(t_subregion, data->t_pos_on_map, &map_area);

  ReadOffsetData read_data = {data->transfer, t_subregion->min};
  MapEdit_copy_to_area(data->map, &map_area, read_offset_transfer_tile,
    &read_data, data->change_info);

  if (data->selection) {
    MapEditSelection_select_area(data->selection, &map_area);
  }
}

bool MapTransfers_plot_to_map(const MapEditContext *const map,
                                   MapPoint const bl,
                                   MapTransfer *const transfer,
                                   MapEditSelection *const selection,
                                   MapEditChanges *const change_info)
{
  /* Paste transfer to tiles map */
  DEBUG("About to paste transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  // FIXME: check beforehand whether we can add the animations
  PlotToMapData data = {map, bl, transfer, selection, change_info};
  for_each_area(transfer, plot_to_map_cb, &data);

  /* Create new animations from transfer (if any) */
  size_t const num_to_add = transfer->anim_count;
  ConvAnimations *const anims = map->anims;
  if (anims == NULL)
    return num_to_add == 0; /* cannot paste new animations nor liquidate old ones */


  for (size_t a = 0; a < num_to_add; a++) {
    MapTransferAnim const anim = transfer_get_anim(transfer, a);
    if (!MapEdit_write_anim(map, MapPoint_add(bl, map_coords_from_coarse(anim.coords)),
                      anim.param, change_info)) {
      MapEdit_anims_to_map(map, NULL);
      return false; /* error */
    }
  } /* next a */

  MapEdit_anims_to_map(map, NULL /* don't want to double-count tiles changed */);
  return true;
}

MapArea MapTransfers_get_bbox(MapPoint const bl, MapTransfer *const transfer)
{
  MapPoint const size_minus_one = map_coords_from_coarse(transfer->size_minus_one);
  return (MapArea){bl, MapPoint_add(bl, size_minus_one)};
}

typedef struct {
  MapEditContext const *map;
  MapPoint const t_pos_on_map;
  MapRef value;
  MapEditChanges *change_info;
} FillMapData;

static void fill_map_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  FillMapData const *const data = cb_arg;

  MapArea map_area;
  MapArea_translate(t_subregion, data->t_pos_on_map, &map_area);

  MapEdit_fill_area(data->map, &map_area, data->value,
                    data->change_info);
}

void MapTransfers_fill_map(const MapEditContext *const map,
                              MapPoint const bl,
                              MapTransfer *const transfer,
                              MapRef const value,
                              MapEditChanges *const change_info)
{
  /* Paste transfer to tiles map */
  DEBUG("About to paste transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  FillMapData data = {map, bl, value, change_info};
  for_each_area(transfer, fill_map_cb, &data);
}

typedef struct {
  MapEditSelection *selection;
  MapPoint const t_pos_on_map;
  MapTransfer *transfer;
} PlotToSelectData;

static void plot_to_select_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  PlotToSelectData const *const data = cb_arg;

  MapArea map_area;
  MapArea_translate(t_subregion, data->t_pos_on_map, &map_area);
  MapEditSelection_select_area(data->selection, &map_area);
}

void MapTransfers_select(MapEditSelection *const selection,
  MapPoint const bl, MapTransfer *const transfer)
{
  DEBUG("About to select transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  PlotToSelectData data = {selection, bl, transfer};
  for_each_area(transfer, plot_to_select_cb, &data);
}

MapRef MapTransfers_read_ref(MapTransfer *const transfer, MapPoint const trans_pos)
{
  assert(transfer != NULL);

  DEBUG_VERBOSEF("Read %" PRIMapCoord ",%" PRIMapCoord
                 " in transfer %" PRIMapCoord ",%" PRIMapCoord "\n",
    trans_pos.x, trans_pos.y,
    MapTransfers_get_dims(transfer).x, MapTransfers_get_dims(transfer).y);

  return map_ref_from_num(((unsigned char *)transfer->tiles)[uchar_offset(transfer, trans_pos)]);
}

MapTransfer *MapTransfers_find_by_name(MapTransfers *const transfers_data,
  char const *const filename, size_t *const index_out)
{
  assert(filename != NULL);
  DEBUG("Find transfer named '%s' in tiles data %p", filename,
        (void *)transfers_data);

  assert(transfers_data != NULL);

  size_t index = 0;;
  MapTransfer *const transfer = strdict_find_value(&transfers_data->dict, filename, &index);

  if (!transfer) {
    DEBUG ("Reached end of transfers list without finding record!");
  } else {
    DEBUG ("Returning pointer to transfer record %p at index %zu", (void *)transfer, index);
  }

  if (index_out != NULL) {
    assert(index <= INT_MAX);
    *index_out = index;
  }

  return transfer;
}

MapTransfer *MapTransfers_find_by_index(MapTransfers *const transfers_data,
  size_t const transfer_index)
{
  DEBUG ("Find transfer at index %zu in tiles data %p",
         transfer_index, (void *)transfers_data);

  assert(transfers_data != NULL);
  assert(transfer_index < transfers_data->count);
  return strdict_get_value_at(&transfers_data->dict, transfer_index);
}

bool MapTransfers_add(MapTransfers *const transfers_data,
  MapTransfer *const transfer, char const *const filename,
  size_t *const new_index_out, MapTexBitmaps *textures)
{
  assert(transfers_data);
  assert(transfer);
  assert(filename);
  assert(textures);
  DEBUG("Will insert transfer '%s' into tiles data %p",
        filename, (void *)transfers_data);

  if (!transfers_data->directory) {
    return false;
  }

  MapTransfer *const existing_transfer = strdict_find_value(&transfers_data->dict, filename, NULL);
  if (existing_transfer) {
    MapTransfers_remove_and_delete(transfers_data, existing_transfer, false);
  }

  size_t new_index = 0;
  bool success = false;
  char *const full_path = make_file_path_in_dir(transfers_data->directory, filename);

  if (full_path) {
    if (ensure_path_exists(full_path) &&
        !report_error(save_compressed(&transfer->dfile, full_path), full_path, "") &&
        set_data_type(full_path, DataType_MapTransfer) &&
        set_saved_with_stamp(&transfer->dfile, full_path)) {
      if (!transfers_data->have_thumbnails || make_transfer_thumbnail(transfers_data, transfer, textures)) {
        success = add_to_list(transfers_data, transfer, &new_index);
        if (!success) {
          delete_thumbnail(transfers_data, transfer);
        }
      }
    } else {
      remove(full_path);
    }
    free(full_path);
  }

  if (new_index_out != NULL) {
    *new_index_out = new_index;
  }
  return success;
}

bool MapTransfers_rename(MapTransfers *const transfers_data,
  MapTransfer *const transfer_to_rename,
  const char *const new_name, size_t *const new_index_out)
{
  assert(transfers_data != NULL);
  assert(transfer_to_rename != NULL);
  assert(new_name != NULL);

  DEBUGF("Rename transfer %p from '%s' to '%s'\n",
         (void*)transfer_to_rename, get_leaf_name(&transfer_to_rename->dfile), new_name);
  if (!transfers_data->directory) {
    return false;
  }

  assert(transfer_to_rename != NULL);
  assert(strdict_find_value(&transfers_data->dict, get_leaf_name(&transfer_to_rename->dfile), NULL) == transfer_to_rename);

  if (stricmp(get_leaf_name(&transfer_to_rename->dfile), new_name) != 0) {
    MapTransfer *const dup = strdict_find_value(&transfers_data->dict, new_name, NULL);
    if (dup) {
      MapTransfers_remove_and_delete(transfers_data, dup, false);
    }
  }

  char *const old_name = strdup(get_leaf_name(&transfer_to_rename->dfile));
  if (!old_name) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  /* Rename the corresponding file */
  bool success = false;
  char *const newpath = make_file_path_in_dir(transfers_data->directory, new_name);
  if (newpath) {
    success = verbose_rename(dfile_get_name(&transfer_to_rename->dfile), newpath);
    if (success) {
      MapTransfer *const removed = strdict_remove_value(&transfers_data->dict,
                                  get_leaf_name(&transfer_to_rename->dfile), NULL);
      assert(removed == transfer_to_rename);
      NOT_USED(removed);
      (void)set_saved_with_stamp(&transfer_to_rename->dfile, newpath);
    }
    free(newpath);
  }

  if (success) {
    // Careful! Key string isn't copied on insertion.
    // Should be impossible to fail to insert after removal
    size_t new_index;
    bool success = strdict_insert(&transfers_data->dict, get_leaf_name(&transfer_to_rename->dfile),
                                   transfer_to_rename, &new_index);
    assert(success);
    NOT_USED(success);

    if (new_index_out != NULL) {
      assert(new_index <= INT_MAX);
      *new_index_out = new_index;
    }

    if (transfers_data->have_thumbnails) {
      SprMem_rename(&transfers_data->thumbnail_sprites,
                    old_name, new_name);
    }
  }

  free(old_name);
  return success;
}

void MapTransfers_remove_and_delete_all(MapTransfers *const transfers_data)
{
  strdict_destroy(&transfers_data->dict, delete_all_cb, transfers_data);
  strdict_init(&transfers_data->dict);
  transfers_data->count = 0;
  SprMem_minimize(&transfers_data->thumbnail_sprites);
}

void MapTransfers_remove_and_delete(MapTransfers *const transfers_data,
   MapTransfer *const transfer_to_delete, bool const shrink_area)
{
  assert(transfer_to_delete != NULL);
  DEBUG ("Will delete transfer '%s' and delink record %p (%s shrink)",
         dfile_get_name(&transfer_to_delete->dfile), (void *)transfer_to_delete,
         shrink_area ? "do" : "don't");

  remove_from_list(transfers_data, transfer_to_delete);
  delete_transfer(transfer_to_delete, transfers_data);
  if (shrink_area) {
    SprMem_minimize(&transfers_data->thumbnail_sprites);
  }
}

MapPoint MapTransfers_get_dims(MapTransfer const *const transfer)
{
  assert(transfer != NULL);
  MapPoint const size_minus_one = map_coords_from_coarse(transfer->size_minus_one);
  MapPoint const p = MapPoint_add(size_minus_one, (MapPoint){1,1});
  DEBUG("Dimensions of transfer: %" PRIMapCoord ",%" PRIMapCoord, p.x, p.y);
  return p;
}

size_t MapTransfers_get_anim_count(MapTransfer const *const transfer)
{
  assert(transfer != NULL);
  return transfer->anim_count;
}
