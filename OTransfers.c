/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object transfers
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

#include "OTransfers.h"
#include "FilePaths.h"
#include "Utils.h"
#include "Session.h"
#include "Config.h"
#include "Triggers.h"
#include "Palette.h"
#include "ObjectsEdit.h"
#include "ObjEditSel.h"
#include "MapCoord.h"
#include "OTransfersData.h"
#include "ObjEditCtx.h"
#include "DataType.h"
#include "WriterGKC.h"
#include "WriterGkey.h"
#include "ReaderGkey.h"
#include "DFileUtils.h"
#include "Triggers.h"
#include "Obj.h"
#include "CoarseCoord.h"

#define TRANSFER_TAG "LGTM"

enum
{
  PREALLOC_SIZE = 4096,
  TransferFormatVersion = 0,
  TransferHasTriggers = 1,
};

typedef struct
{
  CoarsePoint2d coords;
  TriggerFullParam fparam;
} ObjTransferTrigger;

/* Holds data on a single transfer (also used for clipboard) */
struct ObjTransfer
{
  struct DFile dfile;
  CoarsePoint2d  size_minus_one;
  void *refs, *triggers; /* flex anchor */
  size_t trigger_count, trigger_alloc;
};

/* ---------------- Private functions ---------------- */

static inline size_t uchar_offset(ObjTransfer *const transfer,
  MapPoint const trans_pos)
{
  assert(transfer != NULL);
  assert(trans_pos.x >= 0);
  assert(trans_pos.x <= transfer->size_minus_one.x);
  assert(trans_pos.y >= 0);
  assert(trans_pos.y <= transfer->size_minus_one.y);

  size_t const offset = ((size_t)trans_pos.y * ((size_t)transfer->size_minus_one.x + 1)) + (size_t)trans_pos.x;
  assert(offset < (size_t)flex_size(&transfer->refs));
  return offset;
}

static int calc_map_size(CoarsePoint2d const size_minus_one)
{
  return (size_minus_one.x + 1) * (size_minus_one.y + 1);
}

static void write_transfer_ref(ObjTransfer *const transfer,
  MapPoint const trans_pos, ObjRef const ref)
{
  assert(transfer != NULL);

  DEBUG_VERBOSEF("Write %" PRIMapCoord ",%" PRIMapCoord
                 " in transfer %" PRIMapCoord ",%" PRIMapCoord "\n",
    trans_pos.x, trans_pos.y,
    ObjTransfers_get_dims(transfer).x, ObjTransfers_get_dims(transfer).y);

  ((unsigned char *)transfer->refs)[
    uchar_offset(transfer, trans_pos)] = objects_ref_to_num(ref);
}

static bool add_to_list(ObjTransfers *const transfers_data,
  ObjTransfer *const transfer, int *const index)
{
  assert(transfers_data != NULL);
  assert(transfer != NULL);
  assert(index != NULL);
  DEBUG("Adding transfer '%s'", get_leaf_name(&transfer->dfile));
  // Careful! Key string isn't copied on insertion.
  size_t pos;
  if (!strdict_insert(&transfers_data->dict, get_leaf_name(&transfer->dfile), transfer, &pos)) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  transfers_data->count ++;
  DEBUG("OTransfers list now has %d members", transfers_data->count);
  if (index) {
    assert(pos <= (size_t)INT_MAX);
    *index = (int)pos;
  }
  return true;
}

static void remove_from_list(ObjTransfers *const transfers_data,
  ObjTransfer *const transfer)
{
  assert(transfers_data != NULL);
  assert(transfer != NULL);
  ObjTransfer *const removed = strdict_remove_value(
                                     &transfers_data->dict, get_leaf_name(&transfer->dfile), NULL);
  assert(removed == transfer);
  NOT_USED(removed);
  assert(transfers_data->count > 0);
  transfers_data->count --;
  DEBUG("Number of transfers in list is now %d", transfers_data->count);
}

static bool transfer_pre_alloc(ObjTransfer *const transfer, size_t const min_alloc)
{
  assert(transfer);
  assert(transfer->trigger_count <= transfer->trigger_alloc);

  if (transfer->trigger_alloc < min_alloc) {
    size_t const nbytes = (int)sizeof(ObjTransferTrigger) * min_alloc;
    if (nbytes > INT_MAX) {
      return false;
    }

    if (transfer->triggers) {
      assert(transfer->trigger_alloc > 0);
      if (!flex_extend(&transfer->triggers, (int)nbytes)) {
        return false;
      }
    } else {
      assert(transfer->trigger_alloc == 0);
      if (!flex_alloc(&transfer->triggers, (int)nbytes)) {
        return false;
      }
    }
    transfer->trigger_alloc = min_alloc;
  }
  return true;
}

static void transfer_add_trigger(ObjTransfer *const transfer,
  ObjTransferTrigger const *const trigger)
{
  assert(transfer);
  assert(transfer->triggers);
  assert(transfer->trigger_count < transfer->trigger_alloc);
  assert(trigger);
  assert(trigger->fparam.param.action != TriggerAction_Dummy);

  if (trigger->fparam.param.action == TriggerAction_ChainReaction ||
      trigger->fparam.param.action == TriggerAction_ChainReactionOut) {
    assert(trigger->coords.x <= transfer->size_minus_one.x);
    assert(trigger->coords.y <= transfer->size_minus_one.y);
  }

  if (trigger->fparam.param.action == TriggerAction_ChainReaction ||
      trigger->fparam.param.action == TriggerAction_ChainReactionIn) {
    assert(trigger->fparam.next_coords.x <= transfer->size_minus_one.x);
    assert(trigger->fparam.next_coords.y <= transfer->size_minus_one.y);
  }

  ((ObjTransferTrigger *)transfer->triggers)[transfer->trigger_count++] = *trigger;
}

static ObjTransferTrigger transfer_get_trigger(ObjTransfer const *const transfer, size_t const index)
{
  assert(transfer);
  assert(transfer->triggers);
  assert(transfer->trigger_count <= transfer->trigger_alloc);
  assert(index >= 0);
  assert(index < transfer->trigger_count);

  ObjTransferTrigger const *const trigger = ((ObjTransferTrigger *)transfer->triggers) + index;
  assert(trigger->fparam.param.action != TriggerAction_Dummy);

  if (trigger->fparam.param.action == TriggerAction_ChainReaction ||
      trigger->fparam.param.action == TriggerAction_ChainReactionOut) {
    assert(trigger->coords.x <= transfer->size_minus_one.x);
    assert(trigger->coords.y <= transfer->size_minus_one.y);
  }

  if (trigger->fparam.param.action == TriggerAction_ChainReaction ||
      trigger->fparam.param.action == TriggerAction_ChainReactionIn) {
    assert(trigger->fparam.next_coords.x <= transfer->size_minus_one.x);
    assert(trigger->fparam.next_coords.y <= transfer->size_minus_one.y);
  }

  DEBUGF("Got %s at %d,%d from index %zu in transfer %p\n",
    TriggerAction_to_string(trigger->fparam.param.action), trigger->coords.x, trigger->coords.y,
    index, (void *)transfer);

  return *trigger;
}

static void write_triggers(ObjTransfer const *const transfer,
  Writer *const writer)
{
  assert(transfer);

  long int const pos = writer_ftell(writer);
  if (pos >= 0) {
    writer_fseek(writer, WORD_ALIGN(pos), SEEK_SET);
  }

  assert(transfer->trigger_count <= INT32_MAX);
  writer_fwrite_int32((int32_t)transfer->trigger_count, writer);

  for (size_t a = 0; a < transfer->trigger_count; ++a)
  {
    ObjTransferTrigger const trigger = transfer_get_trigger(transfer, a);
    CoarsePoint2d_write(trigger.coords, writer);
    CoarsePoint2d_write(objects_coords_to_coarse(trigger.fparam.next_coords), writer);
    writer_fputc(trigger.fparam.param.action, writer);
    writer_fputc(trigger.fparam.param.value, writer);
  }
}

static void destroy_all(ObjTransfer *const transfer)
{
  /* Free the memory used for the transfer record */
  assert(transfer != NULL);

  if (transfer->refs)
  {
    flex_free(&transfer->refs);
  }

  if (transfer->triggers)
  {
    flex_free(&transfer->triggers);
  }
}

static bool alloc_transfer(ObjTransfer *const transfer,
  CoarsePoint2d const size_minus_one)
{
  assert(transfer);
  transfer->size_minus_one = size_minus_one;
  return flex_alloc(&transfer->refs, calc_map_size(size_minus_one));
}

static SFError read_triggers(ObjTransfer *const transfer, Reader *const reader)
{
  assert(transfer);

  /* We can expect triggers data at the end of the map data */
  long int const pos = reader_ftell(reader);
  if (pos < 0)
  {
    return SFERROR(BadTell);
  }

  if (reader_fseek(reader, WORD_ALIGN(pos), SEEK_SET))
  {
    return SFERROR(BadSeek);
  }

  int32_t trigger_count;
  if (!reader_fread_int32(&trigger_count, reader))
  {
    return SFERROR(ReadFail);
  }

  if (trigger_count < 0 || trigger_count > TriggersMax)
  {
    return SFERROR(BadNumTriggers);
  }

  if (!transfer_pre_alloc(transfer, (size_t)trigger_count))
  {
    return SFERROR(NoMem);
  }

  for (int32_t a = 0; a < trigger_count; ++a)
  {
    ObjTransferTrigger trigger = {{0}};
    if (!CoarsePoint2d_read(&trigger.coords, reader))
    {
      return SFERROR(ReadFail);
    }

    CoarsePoint2d next_coords = {0};
    if (!CoarsePoint2d_read(&next_coords, reader))
    {
      return SFERROR(ReadFail);
    }

    trigger.fparam.next_coords = objects_coords_from_coarse(next_coords);

    int const a = reader_fgetc(reader);
    if (a == EOF)
    {
      return SFERROR(ReadFail);
    }
    if (a < TriggerAction_MissionTarget ||
        a > TriggerAction_ChainReactionIn)
    {
      return SFERROR(BadTriggerAction);
    }
    trigger.fparam.param.action = a;

    if (trigger.fparam.param.action != TriggerAction_ChainReactionIn)
    {
      if (trigger.coords.x > transfer->size_minus_one.x ||
          trigger.coords.y > transfer->size_minus_one.y)
      {
        return SFERROR(BadTriggerCoord);
      }
    }

    if (trigger.fparam.param.action != TriggerAction_ChainReactionOut) {
      if (next_coords.x > transfer->size_minus_one.x ||
          next_coords.y > transfer->size_minus_one.y)
      {
        return SFERROR(BadNextTriggerCoord);
      }
    }

    int const value = reader_fgetc(reader);
    if (value == EOF)
    {
      return SFERROR(ReadFail);
    }
    trigger.fparam.param.value = value;

    /* Allow special trigger types in the file to avoid upsetting the total count of
       triggers but ignore them when reading. */
    if (trigger.fparam.param.action != TriggerAction_ChainReactionOut &&
        trigger.fparam.param.action != TriggerAction_ChainReactionIn)
    {
      transfer_add_trigger(transfer, &trigger);
    }
  }
  return SFERROR(OK);
}

static SFError ObjTransfer_read_cb(DFile const *const dfile,
  Reader *const reader)
{
  assert(dfile);
  ObjTransfer *const transfer = CONTAINER_OF(dfile, ObjTransfer, dfile);

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

  if (TEST_BITS(flags, ~TransferHasTriggers))
  {
    return SFERROR(TransferFla);
  }

  if (!alloc_transfer(transfer, size_minus_one))
  {
    return SFERROR(NoMem);
  }

  SFError err = SFERROR(OK);

  nobudge_register(PREALLOC_SIZE);
  if (!reader_fread(transfer->refs, (size_t)flex_size(&transfer->refs), 1, reader))
  {
    err = SFERROR(ReadFail);
  }
  nobudge_deregister();

  if (!SFError_fail(err) && TEST_BITS(flags, TransferHasTriggers))
  {
    err = read_triggers(transfer, reader);
  }

  return err;
}

static void ObjTransfer_write_cb(DFile const *const dfile,
  Writer *const writer)
{
  assert(dfile);
  ObjTransfer *const transfer = CONTAINER_OF(dfile, ObjTransfer, dfile);

  writer_fwrite(TRANSFER_TAG, sizeof(TRANSFER_TAG)-1, 1, writer);
  writer_fputc(TransferFormatVersion, writer);
  CoarsePoint2d_write(transfer->size_minus_one, writer);
  writer_fputc(transfer->trigger_count > 0 ? TransferHasTriggers : 0, writer);

  nobudge_register(PREALLOC_SIZE);
  writer_fwrite(transfer->refs, (size_t)flex_size(&transfer->refs), 1, writer);
  nobudge_deregister();

  if (transfer->trigger_count > 0) {
    write_triggers(transfer, writer);
  }
}

static void ObjTransfer_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  ObjTransfer *const transfer = CONTAINER_OF(dfile, ObjTransfer, dfile);

  destroy_all(transfer);
  dfile_destroy(&transfer->dfile);
  free(transfer);
}

static void free_all_cb(char const *const key, void *const data, void *const arg)
{
  NOT_USED(key);
  ObjTransfer *const transfer_to_delete = data;
  NOT_USED(arg);
  dfile_release(&transfer_to_delete->dfile);
}

static void delete_transfer(ObjTransfer *const transfer_to_delete)
{
  assert(transfer_to_delete);
  verbose_remove(dfile_get_name(&transfer_to_delete->dfile));
  dfile_release(&transfer_to_delete->dfile);
}

static void delete_all_cb(char const *const key, void *const data, void *const arg)
{
  NOT_USED(key);
  NOT_USED(arg);
  delete_transfer(data);
}

/* ----------------- Public functions ---------------- */

DFile *ObjTransfer_get_dfile(ObjTransfer *const transfer)
{
  assert(transfer);
  return &transfer->dfile;
}

ObjTransfer *ObjTransfer_create(void)
{
  ObjTransfer *const transfer = malloc(sizeof(*transfer));
  if (transfer == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }
  DEBUG ("New transfer list record is at %p", (void *)transfer);

  *transfer = (ObjTransfer){{0}};

  dfile_init(&transfer->dfile, ObjTransfer_read_cb,
             ObjTransfer_write_cb, NULL, ObjTransfer_destroy_cb);

  return transfer;
}

int ObjTransfers_get_count(const ObjTransfers *const transfers_data)
{
  assert(transfers_data != NULL);
  assert(transfers_data->count >= 0);
  DEBUG_VERBOSEF("No. of transfers is %d\n", transfers_data->count);
  return transfers_data->count;
}

void ObjTransfers_init(ObjTransfers *const transfers_data)
{
  assert(transfers_data);

  *transfers_data = (ObjTransfers){
    .count = 0,
    .directory = NULL,
  };

  strdict_init(&transfers_data->dict);
}

void ObjTransfers_load_all(ObjTransfers *const transfers_data,
  char const *const refs_set)
{
  DEBUG("Loading transfers for refs set '%s'...", refs_set);
  char *const dir = make_file_path_in_dir(Config_get_transfers_dir(), refs_set);
  if (!dir) {
    return;
  }

  ObjTransfers_free(transfers_data);
  ObjTransfers_init(transfers_data);
  transfers_data->directory = dir;

  if (!file_exists(dir)) {
    return;
  }

  hourglass_on();

  DirIterator *iter = NULL;
  const _kernel_oserror *e = diriterator_make(&iter, 0, transfers_data->directory, NULL);
  int const expected_ftype = data_type_to_file_type(DataType_ObjectsTransfer);
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

    /* Load refs transfer */
    size_t const n = diriterator_get_object_path_name(iter, NULL, 0);
    {
      char *const full_path = malloc(n + 1);
      if (!full_path) {
        report_error(SFERROR(NoMem), "", "");
        break;
      }
      (void)diriterator_get_object_path_name(iter, full_path, n + 1);

      ObjTransfer *const transfer = ObjTransfer_create();
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

  DEBUG("Number of transfers in list is %d", transfers_data->count);
  diriterator_destroy(iter);
  hourglass_off();
}

void ObjTransfers_open_dir(ObjTransfers const *const transfers_data)
{
  assert(transfers_data);
  if (transfers_data->directory) {
    open_dir(transfers_data->directory);
  }
}

void ObjTransfers_free(ObjTransfers *const transfers_data)
{
  DEBUG("Destroying transfers list attached to refs data %p",
        (void *)transfers_data);

  assert(transfers_data != NULL);

  strdict_destroy(&transfers_data->dict, free_all_cb, transfers_data);

  FREE_SAFE(transfers_data->directory);
}

static size_t count_triggers(TriggersData *const triggers,
  ObjEditSelection *const selected)
{
  assert(triggers);
  DEBUGF("Counting how many triggers are selected\n");

  MapArea sel_area;
  if (!ObjEditSelection_get_bounds(selected, &sel_area)) {
    return 0;
  }

  size_t trig_count = 0;
  TriggersIter iter;
  for (MapPoint p = TriggersIter_get_first(&iter, triggers, &sel_area, NULL);
       !TriggersIter_done(&iter);
       p = TriggersIter_get_next(&iter, NULL))
  {
    DEBUGF("Trigger at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);
    if (ObjEditSelection_is_selected(selected, p)) {
      trig_count++;
      DEBUGF("Trigger is selected, count now %zu\n", trig_count);
    }
  }

  TriggersChainIter chain_iter;
  TriggerFullParam fparam = {{0}};
  for (MapPoint p = TriggersChainIter_get_first(&chain_iter, triggers, &sel_area, &fparam);
       !TriggersChainIter_done(&chain_iter);
       p = TriggersChainIter_get_next(&chain_iter, &fparam))
  {
    assert(fparam.param.action == TriggerAction_ChainReaction);
    DEBUGF("Chain reaction at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);

    if (ObjEditSelection_is_selected(selected, p)) {
      continue; // trigger already counted
    }

    if (ObjEditSelection_is_selected(selected, fparam.next_coords)) {
      trig_count++;
      DEBUGF("Chain's next object is selected, count now %zu\n", trig_count);
    }
  }

  return trig_count;
}

ObjTransfer *ObjTransfers_grab_selection(ObjEditContext const *const objects,
  ObjEditSelection *const selected)
{
  /* Find bounding box covering all selected refs */
  MapArea bounds;
  if (!ObjEditSelection_get_bounds(selected, &bounds)) {
    DEBUG ("Nothing selected!");
    return NULL; /* nothing selected! */
  }

  ObjTransfer *const transfer = ObjTransfer_create();
  if (transfer == NULL) {
    return NULL;
  }

  MapPoint const size_minus_one = MapPoint_sub(bounds.max, bounds.min);
  assert(objects_coords_in_range(size_minus_one));

  if (!alloc_transfer(transfer, objects_coords_to_coarse(size_minus_one)))
  {
    report_error(SFERROR(NoMem), "", "");
    dfile_release(&transfer->dfile);
    return NULL;
  }

  /* Copy selected refs to transfer. It's tempting to use
     ObjEditSelection_for_each but we'd have to store the mask separately. */
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, &bounds);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef ref = objects_ref_mask();
    if (ObjEditSelection_is_selected(selected, p)) {
      ref = ObjectsEdit_read_ref(objects, p);
    }
    write_transfer_ref(transfer, MapPoint_sub(p, bounds.min), ref);
  }

  size_t sel_count = 0;
  TriggersData *const triggers = objects->triggers;
  if (triggers != NULL) {
    sel_count = count_triggers(triggers, selected);
  }

  if (sel_count > 0) {
    if (!transfer_pre_alloc(transfer, sel_count)) {
      report_error(SFERROR(NoMem), "", "");
      dfile_release(&transfer->dfile);
      return NULL;
    }

    MapArea sel_area;
    if (!ObjEditSelection_get_bounds(selected, &sel_area)) {
      return 0;
    }

    /* Collect all triggers activated by destruction of selected objects, including chain
       reactions which destroy a (selected or unselected) object some time afterwards. */
    ObjTransferTrigger t_trig = {{0}};
    TriggersIter iter;
    for (MapPoint p = TriggersIter_get_first(&iter, triggers, &sel_area, &t_trig.fparam);
         !TriggersIter_done(&iter);
         p = TriggersIter_get_next(&iter, &t_trig.fparam))
    {
      assert(t_trig.fparam.param.action != TriggerAction_ChainReactionOut);
      assert(t_trig.fparam.param.action != TriggerAction_ChainReactionIn);

      if (ObjEditSelection_is_selected(selected, p)) {
        /* The selection's wrapped bounding box may contain the coordinates of
           a trigger even though those coordinates appear far outside the bounding box. */
        t_trig.coords = objects_coords_to_coarse(objects_coords_in_area(p, &bounds));

        if (t_trig.fparam.param.action == TriggerAction_ChainReaction) {
          if (ObjEditSelection_is_selected(selected, t_trig.fparam.next_coords)) {
            /* Selected object destroys another selected object some time afterwards.
               Convert absolute coordinates of next object in chain to relative coordinates. */
            t_trig.fparam.next_coords = objects_coords_in_area(t_trig.fparam.next_coords, &bounds);
          } else {
            /* Selected object destroys an unselected object some time afterwards.
               Keep absolute coordinates of next object in chain. */
            t_trig.fparam.param.action = TriggerAction_ChainReactionOut;
          }
        } else {
          t_trig.fparam.next_coords = (MapPoint){0,0};
        }

        transfer_add_trigger(transfer, &t_trig);
      }
    }

    /* Now collect any chain reactions which destroy a selected object some time after an
       unselected object is destroyed. */
    TriggersChainIter chain_iter;
    for (MapPoint p = TriggersChainIter_get_first(&chain_iter, triggers, &sel_area, &t_trig.fparam);
         !TriggersChainIter_done(&chain_iter);
         p = TriggersChainIter_get_next(&chain_iter, &t_trig.fparam))
    {
      assert(t_trig.fparam.param.action == TriggerAction_ChainReaction);
      DEBUGF("Chain reaction at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);

      if (ObjEditSelection_is_selected(selected, p)) {
        continue; // trigger already added
      }

      if (ObjEditSelection_is_selected(selected, t_trig.fparam.next_coords)) {
        /* Unselected object destroys a selected object some time afterwards.
           Convert absolute coordinates of next object in chain to relative coordinates. */
        t_trig.coords = objects_coords_to_coarse(p);
        t_trig.fparam.next_coords = objects_coords_in_area(t_trig.fparam.next_coords, &bounds);
        t_trig.fparam.param.action = TriggerAction_ChainReactionIn;
        transfer_add_trigger(transfer, &t_trig);
      }
    }
  }

  return transfer;
}

static bool for_each_area(ObjTransfer *const transfer,
  bool (*callback)(void *, MapArea const *), void *cb_arg)
{
  MapPoint const t_dims = ObjTransfers_get_dims(transfer);

  MapArea area = {{0}};
  struct {
    bool pend_span_x:1;
    bool pend_span_xy:1;
    bool any_span_on_current_row:1;
  } state = {false, false, false};

  for (MapPoint trans_pos = {.y = 0}; trans_pos.y < t_dims.y; trans_pos.y++) {

    MapCoord start_x = -1; /* no non-mask refs on this row yet */
    state.pend_span_x = false;
    state.any_span_on_current_row = false;

    for (trans_pos.x = 0; trans_pos.x <= t_dims.x; trans_pos.x++) {
      ObjRef const ref = trans_pos.x < t_dims.x ?
        ObjTransfers_read_ref(transfer, trans_pos) : objects_ref_mask();

      if (objects_ref_is_mask(ref)) {
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
              if (!callback(cb_arg, &area)) {
                return false;
              }
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
          if (!callback(cb_arg, &area)) {
            return false;
          }
        } else if (state.any_span_on_current_row && state.pend_span_xy) {
          /* Blocks of non-mask values can't be pending across rows that contain
             other (non-contiguous) spans of non-mask values */
          DEBUGF("Emitting block {%" PRIMapCoord ",%" PRIMapCoord "} "
                 "begun at y=%" PRIMapCoord "\n", area.min.x, area.max.x, area.min.y);
          state.pend_span_xy = false;
          area.max.y = trans_pos.y;
          if (!callback(cb_arg, &area)) {
            return false;
          }
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
      /* Blocks can't be pending across fully masked rows */
      DEBUGF("Empty row: emitting block {%" PRIMapCoord ",%" PRIMapCoord "} "
             "begun at y=%" PRIMapCoord "\n", area.min.x, area.max.x, area.min.y);
      state.pend_span_xy = false;
      area.max.y = trans_pos.y - 1;
      if (!callback(cb_arg, &area)) {
        return false;
      }
    }
  } /* next row */

  if (state.pend_span_xy) {
    DEBUGF("Emitting last block begun at y=%" PRIMapCoord "\n", area.min.y);
    area.max.y = t_dims.y - 1;
    if (!callback(cb_arg, &area)) {
      return false;
    }
  }
  return true;
}

typedef struct {
  ObjTransfer *transfer;
  MapPoint offset_in_trans; /* of plot area within transfer */
} ReadOffsetData;

static ObjRef read_offset_transfer_ref(void *const cb_arg, MapPoint const copy_area_pos)
{
  const ReadOffsetData *const data = cb_arg;
  assert(data != NULL);

  /* Translate coordinates within plot area to be relative to the transfer's origin instead */
  ObjRef const obj_ref = ObjTransfers_read_ref(data->transfer, MapPoint_add(copy_area_pos, data->offset_in_trans));
  assert(!objects_ref_is_mask(obj_ref));
  return obj_ref;
}

typedef struct {
  ObjEditContext const *objects;
  MapPoint const t_pos_on_map;
  ObjRef value;
  struct ObjEditChanges *change_info;
  struct ObjGfxMeshes *meshes;
} FillMapData;

static bool fill_map_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  FillMapData const *const data = cb_arg;

  MapArea m_subregion;
  MapArea_translate(t_subregion, data->t_pos_on_map, &m_subregion);

  ObjectsEdit_fill_area(data->objects, &m_subregion, data->value,
                        data->change_info, data->meshes);
  return true;
}

void ObjTransfers_fill_map(ObjEditContext const *const objects,
                                 MapPoint const bl,
                                 ObjTransfer *const transfer,
                                 ObjRef const value,
                                 struct ObjGfxMeshes *const meshes,
                                 struct ObjEditChanges *const change_info)
{
  assert(objects);
  assert(transfer);
  DEBUG("About to fill shape of transfer %p at %" PRIMapCoord ",%" PRIMapCoord " with %d",
        (void *)transfer, bl.x, bl.y, objects_ref_to_num(value));

  FillMapData data = {objects, bl, value, change_info, meshes};
  for_each_area(transfer, fill_map_cb, &data);
}

typedef struct {
  ObjEditContext const *objects;
  MapPoint const t_pos_on_map;
  ObjTransfer *transfer;
  struct ObjGfxMeshes *meshes;
  ObjEditSelection *occluded, *selection;
  struct ObjEditChanges *change_info;
} PlotToMapData;

static bool can_plot_to_map_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  PlotToMapData const *const data = cb_arg;

  /* Translate bbox of plot area within transfer to absolute map coordinates */
  MapArea m_subregion;
  MapArea_translate(t_subregion, data->t_pos_on_map, &m_subregion);

  ReadOffsetData read_data = {.transfer = data->transfer, .offset_in_trans = t_subregion->min};

  return ObjectsEdit_can_copy_to_area(data->objects, &m_subregion, read_offset_transfer_ref,
    &read_data, data->meshes, data->occluded);
}

bool ObjTransfers_can_plot_to_map(ObjEditContext const *const objects,
                                  MapPoint const bl,
                                  ObjTransfer *const transfer,
                                  struct ObjGfxMeshes *const meshes,
                                  ObjEditSelection *const occluded)
{
  assert(objects);
  assert(transfer);
  DEBUG("Checking whether we can paste transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  PlotToMapData data = {objects, bl, transfer, meshes, occluded};
  bool const can_plot = for_each_area(transfer, can_plot_to_map_cb, &data);
  DEBUGF("%s plot transfer\n", can_plot ? "Can" : "Can't");
  return can_plot;
}

static bool plot_to_map_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  PlotToMapData const *const data = cb_arg;

  /* Translate bbox of plot area within transfer to absolute map coordinates */
  MapArea m_subregion;
  MapArea_translate(t_subregion, data->t_pos_on_map, &m_subregion);

  ReadOffsetData read_data = {.transfer = data->transfer, .offset_in_trans = t_subregion->min};

  ObjectsEdit_copy_to_area(data->objects, &m_subregion, read_offset_transfer_ref,
    &read_data, data->change_info, data->meshes);

  if (data->selection) {
    ObjEditSelection_select_area(data->selection, &m_subregion);
  }

  return true;
}

bool ObjTransfers_plot_to_map(ObjEditContext const *const objects,
                                 MapPoint const bl,
                                 ObjTransfer *const transfer,
                                 struct ObjGfxMeshes *const meshes,
                                 ObjEditSelection *const selection,
                                 struct ObjEditChanges *const change_info)
{
  /* Paste transfer to refs map */
  assert(objects);
  assert(transfer);
  DEBUG("About to paste transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  PlotToMapData data = {objects, bl, transfer, meshes, NULL, selection, change_info};
  for_each_area(transfer, plot_to_map_cb, &data);

  /* Create new triggers from transfer (if any) */
  size_t const num_to_add = transfer->trigger_count;
  TriggersData *const triggers = objects->triggers;
  if (triggers == NULL)
    return num_to_add == 0; /* cannot paste new triggers */

  MapArea const transfer_area = {
    bl, MapPoint_add(bl, objects_coords_from_coarse(transfer->size_minus_one))};

  for (size_t a = 0; a < num_to_add; ++a) {
    ObjTransferTrigger trigger = transfer_get_trigger(transfer, a);
    MapPoint coords = {0,0};

    switch (trigger.fparam.param.action) {
      case TriggerAction_ChainReaction:
        /* Convert relative to absolute coordinates of current and next object in chain */
        coords = MapPoint_add(bl, objects_coords_from_coarse(trigger.coords));
        trigger.fparam.next_coords = MapPoint_add(transfer_area.min, trigger.fparam.next_coords);
        break;

      case TriggerAction_ChainReactionOut:
        /* Skip chain to absolute coordinates if they are overwritten by
           another part of the transfer */
        if (objects_bbox_contains(&transfer_area, trigger.fparam.next_coords)) {
          /* The transfer's wrapped bounding box may contain the destination
             coordinates of a chain reaction even though those coordinates
             appear far outside the bounding box. */
          ObjRef const obj_ref = ObjTransfers_read_ref(transfer,
                               objects_coords_in_area(trigger.fparam.next_coords, &transfer_area));
          if (!objects_ref_is_mask(obj_ref)) {
            continue;
          }
        }
        trigger.fparam.param.action = TriggerAction_ChainReaction;
        /* Convert relative to absolute coordinates of current object in chain */
        coords = MapPoint_add(bl, objects_coords_from_coarse(trigger.coords));
        break;

      case TriggerAction_ChainReactionIn:
        coords = objects_coords_from_coarse(trigger.coords);
        trigger.fparam.param.action = TriggerAction_ChainReaction;
        /* Convert relative to absolute coordinates of next object in chain */
        trigger.fparam.next_coords = MapPoint_add(transfer_area.min, trigger.fparam.next_coords);
        break;

      default:
        coords = MapPoint_add(bl, objects_coords_from_coarse(trigger.coords));
        break;
    }

    if (!ObjectsEdit_add_trigger(objects, coords, trigger.fparam, change_info)) {
      return false; /* error */
    }
  } /* next a */
  return true;
}

typedef struct {
  ObjEditSelection *selection;
  ObjEditContext const *objects;
  MapPoint t_pos_on_map;
  ObjTransfer *transfer;
} PlotToSelectData;

static bool plot_to_select_cb(void *const cb_arg, MapArea const *const t_subregion)
{
  assert(cb_arg != NULL);
  assert(MapArea_is_valid(t_subregion));
  PlotToSelectData const *const data = cb_arg;

  MapArea m_subregion;
  MapArea_translate(t_subregion, data->t_pos_on_map, &m_subregion);
  ObjEditSelection_select_area(data->selection, &m_subregion);
  return true;
}

void ObjTransfers_select(ObjEditSelection *const selection,
  MapPoint const bl, ObjTransfer *const transfer,
    ObjEditContext const *objects)
{
  DEBUG("About to select transfer %p at %" PRIMapCoord ",%" PRIMapCoord,
        (void *)transfer, bl.x, bl.y);

  PlotToSelectData data = {selection, objects, bl, transfer};
  for_each_area(transfer, plot_to_select_cb, &data);
}

ObjRef ObjTransfers_read_ref(ObjTransfer *const transfer, MapPoint const trans_pos)
{
  assert(transfer != NULL);

  ObjRef const obj_ref = objects_ref_from_num(((unsigned char *)transfer->refs)[uchar_offset(transfer, trans_pos)]);
  DEBUGF("Read %zu at %" PRIMapCoord ",%" PRIMapCoord
                 " in transfer of size %" PRIMapCoord ",%" PRIMapCoord "\n",
    objects_ref_to_num(obj_ref), trans_pos.x, trans_pos.y,
    ObjTransfers_get_dims(transfer).x, ObjTransfers_get_dims(transfer).y);
  return obj_ref;
}

ObjTransfer *ObjTransfers_find_by_name(ObjTransfers *const transfers_data,
  char const *const filename, int *const index_out)
{
  assert(filename != NULL);
  DEBUG("Find transfer named '%s' in tiles data %p", filename,
        (void *)transfers_data);

  assert(transfers_data != NULL);

  size_t index = SIZE_MAX;
  ObjTransfer *const transfer = strdict_find_value(&transfers_data->dict, filename, &index);

  if (!transfer) {
    DEBUG ("Reached end of transfers list without finding record!");
  } else {
    DEBUG ("Returning pointer to transfer record %p at index %zu", (void *)transfer, index);
  }

  if (index_out != NULL) {
    assert(index <= INT_MAX);
    *index_out = (int)index;
  }

  return transfer;
}

ObjTransfer *ObjTransfers_find_by_index(ObjTransfers *const transfers_data,
  int const transfer_index)
{
  DEBUG ("Find transfer at index %d in tiles data %p",
         transfer_index, (void *)transfers_data);

  assert(transfers_data != NULL);
  assert(transfer_index < transfers_data->count);
  return strdict_get_value_at(&transfers_data->dict, (size_t)transfer_index);
}

bool ObjTransfers_add(ObjTransfers *const transfers_data,
  ObjTransfer *const transfer, char const *const filename,
  int *const new_index_out)
{
  /* Find place in linked list to insert new transfer record */
  assert(transfers_data);
  assert(transfer);
  assert(filename);
  DEBUG("Will insert transfer '%s' into list attached to refs data %p",
        filename, (void *)transfers_data);

  if (!transfers_data->directory) {
    return false;
  }

  ObjTransfer *const existing_transfer = strdict_find_value(&transfers_data->dict, filename, NULL);
  if (existing_transfer) {
    ObjTransfers_remove_and_delete(transfers_data, existing_transfer);
  }

  int new_index = 0;
  bool success = false;
  char *const full_path = make_file_path_in_dir(transfers_data->directory, filename);

  if (full_path) {
    if (ensure_path_exists(full_path) &&
        !report_error(save_compressed(&transfer->dfile, full_path), full_path, "") &&
        set_data_type(full_path, DataType_ObjectsTransfer) &&
        set_saved_with_stamp(&transfer->dfile, full_path)) {
      success = add_to_list(transfers_data, transfer, &new_index);
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

bool ObjTransfers_rename(ObjTransfers *const transfers_data,
  ObjTransfer *const transfer_to_rename,
  const char *const new_name, int *const new_index_out)
{
  assert(transfers_data != NULL);
  assert(transfer_to_rename != NULL);
  assert(new_name != NULL);

  if (!transfers_data->directory) {
    return false;
  }

  assert(transfer_to_rename != NULL);
  assert(strdict_find_value(&transfers_data->dict, get_leaf_name(&transfer_to_rename->dfile), NULL) == transfer_to_rename);

  if (stricmp(get_leaf_name(&transfer_to_rename->dfile), new_name) != 0) {
    ObjTransfer *const dup = strdict_find_value(&transfers_data->dict, new_name, NULL);
    if (dup) {
      ObjTransfers_remove_and_delete(transfers_data, dup);
    }
  }

  /* Rename the corresponding file */
  bool success = false;
  char *const newpath = make_file_path_in_dir(transfers_data->directory, new_name);
  if (newpath) {
    success = verbose_rename(dfile_get_name(&transfer_to_rename->dfile), newpath);
    if (success) {
      ObjTransfer *const removed = strdict_remove_value(&transfers_data->dict,
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
      *new_index_out = (int)new_index;
    }
  }
  return success;
}

void ObjTransfers_remove_and_delete_all(ObjTransfers *const transfers_data)
{
  strdict_destroy(&transfers_data->dict, delete_all_cb, transfers_data);
  strdict_init(&transfers_data->dict);
  transfers_data->count = 0;
}

void ObjTransfers_remove_and_delete(ObjTransfers *const transfers_data,
   ObjTransfer *const transfer_to_delete)
{
  assert(transfer_to_delete != NULL);
  DEBUG ("Will delete transfer '%s' and delink record %p",
         dfile_get_name(&transfer_to_delete->dfile), (void *)transfer_to_delete);

  remove_from_list(transfers_data, transfer_to_delete);
  delete_transfer(transfer_to_delete);
}

MapPoint ObjTransfers_get_dims(ObjTransfer const *const transfer)
{
  assert(transfer != NULL);
  MapPoint const size_minus_one = objects_coords_from_coarse(transfer->size_minus_one);
  MapPoint const p = MapPoint_add(size_minus_one, (MapPoint){1,1});
  DEBUG("Dimensions of transfer: %" PRIMapCoord ",%" PRIMapCoord, p.x, p.y);
  return p;
}

size_t ObjTransfers_get_trigger_count(ObjTransfer const *const transfer)
{
  assert(transfer != NULL);
  return transfer->trigger_count;
}
