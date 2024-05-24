/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information transfers
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

#include "stdlib.h"
#include "stdio.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include "Err.h"
#include "Macros.h"
#include "Debug.h"

#include "Utils.h"
#include "ITransfers.h"
#include "WriterGKC.h"
#include "WriterGkey.h"
#include "ReaderGkey.h"
#include "InfosData.h"
#include "Infos.h"
#include "InfoEdit.h"
#include "InfoEditChg.h"
#include "Map.h"
#include "CoarseCoord.h"
#include "SelBitmask.h"

#define TRANSFER_TAG "YMMV"

enum
{
  TransferFormatVersion = 0,
  BytesPerTextOffset = 4,
};

/* Holds data on a single transfer (also used for clipboard) */
struct InfoTransfer
{
  struct DFile dfile;
  CoarsePoint2d offset, size_minus_one;
  TargetInfosData infos;
};

/* ---------------- Private functions ---------------- */

static void destroy_all(InfoTransfer *const transfer)
{
  /* Free the memory used for the transfer record */
  assert(transfer != NULL);
  target_infos_destroy(&transfer->infos);
}

static SFError read_offsets(size_t const n,
  long int offsets[n],
  Reader *const reader)
{
  assert(offsets);

  long int const index_start = reader_ftell(reader);
  if (index_start < 0)
  {
    return SFERROR(BadTell);
  }

  for (size_t t = 0; t < n; ++t)
  {
    int32_t offset;
    if (!reader_fread_int32(&offset, reader))
    {
      return SFERROR(ReadFail);
    }

    if ((offset < 0) ||
        ((uint32_t)offset < (n * BytesPerTextOffset)) ||
        (offset != WORD_ALIGN(offset)))
    {
      return SFERROR(BadStringOffset);
    }
    offsets[t] = index_start + offset;
  }
  return SFERROR(OK);
}

static SFError InfoTransfer_read_cb(DFile const *const dfile,
  Reader *const reader)
{
  assert(dfile);
  InfoTransfer *const transfer = CONTAINER_OF(dfile, InfoTransfer, dfile);

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

  int const flags = reader_fgetc(reader);
  if (flags == EOF)
  {
    return SFERROR(ReadFail);
  }

  if (TEST_BITS(flags, ~0))
  {
    return SFERROR(TransferFla);
  }

  if (!CoarsePoint2d_read(&transfer->offset, reader) ||
      !CoarsePoint2d_read(&transfer->size_minus_one, reader))
  {
    return SFERROR(ReadFail);
  }

  SFError err = target_infos_read(&transfer->infos, reader);
  if (SFError_fail(err)) {
    return err;
  }

  size_t const num_infos = target_infos_get_count(&transfer->infos);
  long int *const offsets = malloc(sizeof(*offsets) * TargetInfoTextIndex_Count * num_infos);
  if (!offsets)
  {
    return SFERROR(NoMem);
  }

  err = read_offsets(num_infos * TargetInfoTextIndex_Count, offsets, reader);
  if (!SFError_fail(err))
  {
     err = target_infos_read_texts(&transfer->infos,
                                   offsets, num_infos, reader);
  }
  free(offsets);
  return err;
}

static void InfoTransfer_write_cb(DFile const *const dfile,
  Writer *const writer)
{
  assert(dfile);
  InfoTransfer *const transfer = CONTAINER_OF(dfile, InfoTransfer, dfile);

  writer_fwrite(TRANSFER_TAG, sizeof(TRANSFER_TAG)-1, 1, writer);

  writer_fputc(TransferFormatVersion, writer);
  writer_fputc(0, writer); // flags

  CoarsePoint2d_write(transfer->offset, writer);
  CoarsePoint2d_write(transfer->size_minus_one, writer);

  target_infos_write(&transfer->infos, writer);
  size_t const num_infos = target_infos_get_count(&transfer->infos);
  assert(TargetInfoMax <= LONG_MAX);
  long int const offset = (long)num_infos * TargetInfoTextIndex_Count * BytesPerTextOffset;
  target_infos_write_text_offsets(&transfer->infos, writer, offset);
  target_infos_write_texts(&transfer->infos, writer);
}

static void InfoTransfer_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  InfoTransfer *const transfer = CONTAINER_OF(dfile, InfoTransfer, dfile);

  destroy_all(transfer);
  dfile_destroy(&transfer->dfile);
  free(transfer);
}

/* ----------------- Public functions ---------------- */

DFile *InfoTransfer_get_dfile(InfoTransfer *const transfer)
{
  assert(transfer);
  return &transfer->dfile;
}

InfoTransfer *InfoTransfer_create(void)
{
  InfoTransfer *const transfer = malloc(sizeof(*transfer));
  if (transfer == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }
  DEBUG ("New transfer list record is at %p", (void *)transfer);

  *transfer = (InfoTransfer){{0}};

  target_infos_init(&transfer->infos);

  dfile_init(&transfer->dfile, InfoTransfer_read_cb,
             InfoTransfer_write_cb, NULL, InfoTransfer_destroy_cb);

  return transfer;
}

InfoTransfer *InfoTransfers_grab_selection(InfoEditContext const *const infos,
  SelectionBitmask *const selected)
{
  if (SelectionBitmask_is_none(selected)) {
    DEBUG ("Nothing selected!");
    return NULL; /* nothing selected! */
  }

  InfoTransfer *const transfer = InfoTransfer_create();
  if (transfer == NULL) {
    return NULL;
  }

  SelectionBitmaskIter iter;
  SFError err = SFERROR(OK);
  MapArea bounds = MapArea_make_invalid();

  for (size_t index = SelectionBitmaskIter_get_first(&iter, selected);
       !SelectionBitmaskIter_done(&iter);
       index = SelectionBitmaskIter_get_next(&iter))
  {
    TargetInfo const *const info = InfoEdit_get(infos, index);
    MapPoint const pos = target_info_get_pos(info);
    MapArea_expand(&bounds, pos);

    size_t t_index;
    err = target_infos_add(&transfer->infos, pos, &t_index);
    if (SFError_fail(err)) {
      break;
    }
    TargetInfo *const copy = target_info_from_index(&transfer->infos, t_index);

    for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
         k < TargetInfoTextIndex_Count;
         ++k) {
      char const *const text = target_info_get_text(info, k);
      err = target_info_set_text(copy, k, text);
      if (SFError_fail(err)) {
        break;
      }
    }
  }

  if (report_error(err, "", "")) {
    dfile_release(InfoTransfer_get_dfile(transfer));
    return NULL;
  }

  assert(MapArea_is_valid(&bounds));
  transfer->size_minus_one = map_coords_to_coarse(
                               MapPoint_sub(bounds.max, bounds.min));
  transfer->offset = map_coords_to_coarse(bounds.min);

  return transfer;
}

MapPoint InfoTransfers_get_origin(InfoTransfer const *const transfer)
{
  assert(transfer != NULL);
  MapPoint const p = map_coords_from_coarse(transfer->offset);
  DEBUG("Origin of transfer: %" PRIMapCoord ",%" PRIMapCoord, p.x, p.y);
  return p;
}

MapPoint InfoTransfers_get_dims(InfoTransfer const *const transfer)
{
  assert(transfer != NULL);
  MapPoint const size_minus_one = map_coords_from_coarse(transfer->size_minus_one);
  MapPoint const p = MapPoint_add(size_minus_one, (MapPoint){1,1});
  DEBUG("Dimensions of transfer: %" PRIMapCoord ",%" PRIMapCoord, p.x, p.y);
  return p;
}

size_t InfoTransfers_get_info_count(InfoTransfer const *const transfer)
{
  return target_infos_get_count(&transfer->infos);
}

MapPoint InfoTransfers_get_pos(InfoTransfer const *const transfer,
  size_t const index)
{
  TargetInfo const *const info = target_info_from_index(&transfer->infos, index);
  return MapPoint_sub(target_info_get_pos(info), map_coords_from_coarse(transfer->offset));
}

bool InfoTransfers_plot_to_map(InfoEditContext const *const infos,
                               MapPoint const bl,
                               InfoTransfer *const transfer,
                               SelectionBitmask *const selected,
                               InfoEditChanges *const change_info)
{
  assert(infos);
  assert(transfer);
  DEBUG("About to paste transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  size_t const count = target_infos_get_count(&transfer->infos);

  for (size_t t_index = 0; t_index < count; ++t_index) {
    TargetInfo const *const info = target_info_from_index(&transfer->infos, t_index);
    MapPoint const pos = MapPoint_sub(target_info_get_pos(info), map_coords_from_coarse(transfer->offset));

    char const *strings[TargetInfoTextIndex_Count];
    for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
         k < TargetInfoTextIndex_Count;
         ++k) {
      strings[k] = target_info_get_text(info, k);
    }

    size_t index;
    if (report_error(InfoEdit_add(infos, MapPoint_add(bl, pos), strings, change_info, &index), "", "")) {
      return false;
    }

    if (selected) {
      SelectionBitmask_select(selected, index);
    }
  }
  return true;
}

void InfoTransfers_find_occluded(InfoEditContext const *const infos,
                                   MapPoint const bl,
                                   InfoTransfer *const transfer,
                                   SelectionBitmask *const occluded)
{
  assert(infos);
  assert(transfer);
  DEBUG("Checking whether we can paste transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  size_t const count = target_infos_get_count(&transfer->infos);

  for (size_t t_index = 0; t_index < count; ++t_index) {
    TargetInfo const *const info = target_info_from_index(&transfer->infos, t_index);
    MapPoint const pos = MapPoint_sub(target_info_get_pos(info), map_coords_from_coarse(transfer->offset));
    InfoEdit_find_occluded(infos, MapPoint_add(bl, pos), occluded);
  }
}
