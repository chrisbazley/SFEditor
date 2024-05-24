/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Selected ground objects properties dialogue box
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


#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"
#include "scrolllist.h"
#include "flex.h"

#include "msgtrans.h"
#include "PathTail.h"
#include "StringBuff.h"
#include "msgtrans.h"
#include "Macros.h"
#include "err.h"
#include "GadgetUtil.h"
#include "EventExtra.h"

#include "IntDict.h"
#include "Obj.h"
#include "SFError.h"
#include "Utils.h"
#include "filenames.h"
#include "graphicsdata.h"
#include "Triggers.h"
#include "Editor.h"
#include "Session.h"
#include "ObjEditCtx.h"
#include "OPropDbox.h"
#include "Ships.h"
#include "ObjectsMode.h"
#include "ObjectsEdit.h"

#define SINGLE_DBOX 1

enum {
  InitialArraySize = 4,
  ArrayGrowthFactor = 2,
  SetDefaultFocusInvisible = -2,
};

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_TypeSet = 0x6c,
  ComponentId_TriggerList  = 0x14,
  ComponentId_MissionTarget = 0,
  ComponentId_MultiATA   = 1,
  ComponentId_MultiATACount = 0x23,
  ComponentId_MegaLaser = 2,
  ComponentId_MegaLaserCount = 0x24,
  ComponentId_Bombs  = 3,
  ComponentId_BombsCount = 0x25,
  ComponentId_Mines  = 4,
  ComponentId_MinesCount  = 0x26,
  ComponentId_DefencesOn  = 6,
  ComponentId_DefencesOff  = 7,
  ComponentId_DefencesOffTime = 0x5c,
  ComponentId_Chain  = 12,
  ComponentId_ChainNext = 0x61,
  ComponentId_ChainNextLabel = 0x67,
  ComponentId_ChainNextX = 0x68,
  ComponentId_ChainNextY = 0x69,
  ComponentId_ChainDelay = 0x66,
  ComponentId_Shield  = 8,
  ComponentId_ShieldType = 0x58,
  ComponentId_Cash = 5,
  ComponentId_CashAmount = 0x5a,
  ComponentId_TimeLimit = 9,
  ComponentId_TimeLimitValue = 0x5e,
  ComponentId_Friendly = 10,
  ComponentId_RadarJammer = 11,
  ComponentId_DeleteButton = 0x65,
  ComponentId_ReplaceButton = 0x64,
  ComponentId_EditButton = 0x64,
  ComponentId_AddButton = 0x62,
  ComponentId_CancelButton = 0x6f,
  ComponentId_SetButton = 0x6e,
  ComponentId_SelectAllButton = 0x6a,
  ComponentId_ClearSelectionButton = 0x6b,
  ComponentId_CancelButton2 = 0x72,
  ComponentId_SetButton2 = 0x73,
  ComponentId_Location = 0x11,
};

static ComponentId const chain_components[] = {
  ComponentId_ChainNextLabel,
  ComponentId_ChainNextX,
  ComponentId_ChainNextY,
  ComponentId_ChainNext
};

typedef enum
{
  UITriggerAction_First,
  UITriggerAction_MissionTarget = UITriggerAction_First,
  UITriggerAction_BonusMultiATA,
  UITriggerAction_BonusMegaLaser,
  UITriggerAction_BonusBombs,
  UITriggerAction_BonusMines,
  UITriggerAction_DefencesOn,
  UITriggerAction_DefencesOff,
  UITriggerAction_ChainReaction,
  UITriggerAction_CrippleShipType,
  UITriggerAction_CashBonus,
  UITriggerAction_MissionTimer,
  UITriggerAction_FriendlyDead,
  UITriggerAction_FixScanners,
  UITriggerAction_Count,
}
UITriggerAction;

typedef struct
{
  ObjPropDboxes *prop_dboxes;
  ObjectId my_object;
  ObjectId my_add_object;
  int my_add_window;
  void *items;
  TriggerFullParam last_added;
  int nitems;
  int nalloc;
  int edited_item;
  MapPoint pos;
  bool keep; // don't allow deletion during setting of properties
} ObjPropDbox;

/* ---------------- Private functions ---------------- */

static Editor *get_editor(ObjPropDbox const *const prop)
{
  assert(prop != NULL);
  assert(prop->prop_dboxes != NULL);
  return prop->prop_dboxes->editor;
}

static EditSession *get_session(ObjPropDbox const *const prop)
{
  return Editor_get_session(get_editor(prop));
}

static ShipType const stypes[] = {
  ShipType_Player, ShipType_Fighter1, ShipType_Fighter2, ShipType_Fighter3,
  ShipType_Fighter4, ShipType_Big1, ShipType_Big2, ShipType_Big3, ShipType_Satellite};

static ShipType index_to_ship_type(int const index)
{
  DEBUGF("Index %d to ship type\n", index);
  if (index >= 0 && (size_t)index < ARRAY_SIZE(stypes))
  {
    return stypes[index];
  }
  assert("Bad stringset index" == NULL);
  return ShipType_Player;
}

static int ship_type_to_index(ShipType const ship_type)
{
  DEBUGF("Ship type %d to index\n", ship_type);

  for (size_t i = 0; i < ARRAY_SIZE(stypes); ++i) {
    if (stypes[i] == ship_type) {
      return stypes[i];
    }
  }
  assert("Bad ship type" == NULL);
  return 0;
}

static ObjRef index_to_obj_type(int const index)
{
  DEBUGF("Index %d to object type\n", index);
  if (index >= 0)
  {
    size_t uindex = (size_t)index;
//  if (uindex == 0) {
//    return Obj_RefNone;
//  }
//  uindex--;

    if (uindex < Obj_ObjectCount)
    {
      return objects_ref_object(uindex);
    }
    uindex -= Obj_ObjectCount;

    if (uindex < Obj_CloudCount)
    {
      return objects_ref_cloud(uindex);
    }
    uindex -= Obj_CloudCount;

    if (uindex == 0)
    {
      return objects_ref_hill();
    }
    uindex--;

//  if (uindex == 0)
//  {
//    return Obj_RefMask;
//  }
//  uindex--;
  }
  assert("Bad stringset index" == NULL);
  return objects_ref_from_num(Obj_RefMinObject);
}

static int obj_type_to_index(ObjRef const obj_type)
{
  DEBUGF("Object type %zu to index\n", objects_ref_to_num(obj_type));
  int low = 0;

//  if (objects_ref_is_none(obj_type)) {
//    return low;
//  }
//  low++;

  if (objects_ref_is_object(obj_type)) {
    return low + ((int)objects_ref_to_num(obj_type) - Obj_RefMinObject);
  }
  low += Obj_ObjectCount;

  if (objects_ref_is_cloud(obj_type)) {
    return low + ((int)objects_ref_to_num(obj_type) - Obj_RefMinCloud);
  }
  low += Obj_CloudCount;

  if (objects_ref_is_hill(obj_type)) {
    return low;
  }
  low++;

//  if (obj_type == Obj_RefMask) {
//    return low;
//  }
  assert("Bad object type" == NULL);
  return 0;
}


static UITriggerAction ui_from_action(TriggerParam const param)
{
  if (param.action == TriggerAction_DefenceTimer &&
      param.value == TriggerActivateDefences) {
    return UITriggerAction_DefencesOn;
  }

  static UITriggerAction const map[] = {
    [TriggerAction_MissionTarget] = UITriggerAction_MissionTarget,
    [TriggerAction_BonusMultiATA] = UITriggerAction_BonusMultiATA,
    [TriggerAction_BonusMegaLaser] = UITriggerAction_BonusMegaLaser,
    [TriggerAction_BonusBombs] = UITriggerAction_BonusBombs,
    [TriggerAction_BonusMines] = UITriggerAction_BonusMines,
    [TriggerAction_DefenceTimer] = UITriggerAction_DefencesOff,
    [TriggerAction_ChainReaction] = UITriggerAction_ChainReaction,
    [TriggerAction_CrippleShipType] = UITriggerAction_CrippleShipType,
    [TriggerAction_CashBonus] = UITriggerAction_CashBonus,
    [TriggerAction_MissionTimer] = UITriggerAction_MissionTimer,
    [TriggerAction_FriendlyDead] = UITriggerAction_FriendlyDead,
    [TriggerAction_MissionTarget2] = UITriggerAction_MissionTarget,
    [TriggerAction_FixScanners] = UITriggerAction_FixScanners,
  };
  assert(param.action >= 0);
  assert(param.action < ARRAY_SIZE(map));
  return map[param.action];
}

static TriggerParam action_from_ui(UITriggerAction const ui_act, int const value)
{
  if (ui_act == UITriggerAction_DefencesOn) {
    return (TriggerParam){.action = TriggerAction_DefenceTimer, .value = TriggerActivateDefences};
  }
  static TriggerAction const map[] = {
    [UITriggerAction_MissionTarget] = TriggerAction_MissionTarget,
    [UITriggerAction_BonusMultiATA] = TriggerAction_BonusMultiATA,
    [UITriggerAction_BonusMegaLaser] = TriggerAction_BonusMegaLaser,
    [UITriggerAction_BonusBombs] = TriggerAction_BonusBombs,
    [UITriggerAction_BonusMines] = TriggerAction_BonusMines,
    [UITriggerAction_DefencesOff] = TriggerAction_DefenceTimer,
    [UITriggerAction_ChainReaction] = TriggerAction_ChainReaction,
    [UITriggerAction_CrippleShipType] = TriggerAction_CrippleShipType,
    [UITriggerAction_CashBonus] = TriggerAction_CashBonus,
    [UITriggerAction_MissionTimer] = TriggerAction_MissionTimer,
    [UITriggerAction_FriendlyDead] = TriggerAction_FriendlyDead,
    [UITriggerAction_FixScanners] = TriggerAction_FixScanners,
  };
  assert(ui_act >= 0);
  assert(ui_act < ARRAY_SIZE(map));
  return (TriggerParam){.action = map[ui_act], .value = value};
}

static UITriggerAction ui_from_radio(ComponentId const radio_id)
{
 static UITriggerAction const map[] = {
    [ComponentId_MissionTarget] = UITriggerAction_MissionTarget,
    [ComponentId_MultiATA] = UITriggerAction_BonusMultiATA,
    [ComponentId_MegaLaser] = UITriggerAction_BonusMegaLaser,
    [ComponentId_Bombs] = UITriggerAction_BonusBombs,
    [ComponentId_Mines] = UITriggerAction_BonusMines,
    [ComponentId_DefencesOn] = UITriggerAction_DefencesOn,
    [ComponentId_DefencesOff] = UITriggerAction_DefencesOff,
    [ComponentId_Chain] = UITriggerAction_ChainReaction,
    [ComponentId_Shield] = UITriggerAction_CrippleShipType,
    [ComponentId_Cash] = UITriggerAction_CashBonus,
    [ComponentId_TimeLimit] = UITriggerAction_MissionTimer,
    [ComponentId_Friendly] = UITriggerAction_FriendlyDead,
    [ComponentId_RadarJammer] = UITriggerAction_FixScanners,
  };
  assert(radio_id >= 0);
  assert((size_t)radio_id < ARRAY_SIZE(map));
  return map[radio_id];
}

static ComponentId radio_from_ui(UITriggerAction const ui_act)
{
 static ComponentId const map[] = {
    [UITriggerAction_MissionTarget] = ComponentId_MissionTarget,
    [UITriggerAction_BonusMultiATA] = ComponentId_MultiATA,
    [UITriggerAction_BonusMegaLaser] = ComponentId_MegaLaser,
    [UITriggerAction_BonusBombs] = ComponentId_Bombs,
    [UITriggerAction_BonusMines] = ComponentId_Mines,
    [UITriggerAction_DefencesOn] = ComponentId_DefencesOn,
    [UITriggerAction_DefencesOff] = ComponentId_DefencesOff,
    [UITriggerAction_ChainReaction] = ComponentId_Chain,
    [UITriggerAction_CrippleShipType] = ComponentId_Shield,
    [UITriggerAction_CashBonus] = ComponentId_Cash,
    [UITriggerAction_MissionTimer] = ComponentId_TimeLimit,
    [UITriggerAction_FriendlyDead] = ComponentId_Friendly,
    [UITriggerAction_FixScanners] = ComponentId_RadarJammer,
  };
  assert(ui_act >= 0);
  assert(ui_act < ARRAY_SIZE(map));
  return map[ui_act];
}

static ComponentId gadget_from_ui(UITriggerAction const ui_act)
{
  static ComponentId const map[] = {
    [UITriggerAction_MissionTarget] = NULL_ComponentId,
    [UITriggerAction_BonusMultiATA] = ComponentId_MultiATACount,
    [UITriggerAction_BonusMegaLaser] = ComponentId_MegaLaserCount,
    [UITriggerAction_BonusBombs] = ComponentId_BombsCount,
    [UITriggerAction_BonusMines] = ComponentId_MinesCount,
    [UITriggerAction_DefencesOn] = NULL_ComponentId,
    [UITriggerAction_DefencesOff] = ComponentId_DefencesOffTime,
    [UITriggerAction_ChainReaction] = ComponentId_ChainDelay,
    [UITriggerAction_CrippleShipType] = ComponentId_ShieldType,
    [UITriggerAction_CashBonus] = ComponentId_CashAmount,
    [UITriggerAction_MissionTimer] = ComponentId_TimeLimitValue,
    [UITriggerAction_FriendlyDead] = NULL_ComponentId,
    [UITriggerAction_FixScanners] =  NULL_ComponentId,
  };
  assert(ui_act >= 0);
  assert(ui_act < ARRAY_SIZE(map));
  return map[ui_act];
}

static char const *get_list_text(ObjPropDbox const *const prop, TriggerFullParam const item)
{
  char const *text = "";
  char token[32];
  EditSession *const session = get_session(prop);

  UITriggerAction const ui_act = ui_from_action(item.param);
  sprintf(token, "Trig%d", (int)ui_act);

  switch (ui_act)
  {
    case UITriggerAction_BonusMultiATA:
    case UITriggerAction_BonusMegaLaser:
    case UITriggerAction_BonusBombs:
    case UITriggerAction_BonusMines:
      {
        char count[32];
        sprintf(count, "%d", item.param.value);
        text = msgs_lookup_subn(token, 1, count);
      }
      break;

    case UITriggerAction_DefencesOff:
      {
        char extra_time[32];
        sprintf(extra_time, "%d", item.param.value);
        text = msgs_lookup_subn(token, 1, extra_time);
      }
      break;

    case UITriggerAction_ChainReaction:
      {
        char delay[32];
        sprintf(delay, "%d", item.param.value * TriggerChainReactionMultipler);

        StringBuffer obj_name;
        stringbuffer_init(&obj_name);
        FilenamesData const *const filenames = Session_get_filenames(session);
        ObjEditContext const *const objects = Session_get_objects(session);
        if (!get_objname_from_type(&obj_name, filenames_get(filenames, DataType_PolygonMeshes),
                                   ObjectsEdit_read_ref(objects, item.next_coords)))
        {
          report_error(SFERROR(NoMem), "", "");
        }

        char loc[32] = "";
        sprintf(loc, "%03" PRIMapCoord ",%03" PRIMapCoord, item.next_coords.x, item.next_coords.y);

        text = msgs_lookup_subn(token, 3, delay, stringbuffer_get_pointer(&obj_name), loc);
        stringbuffer_destroy(&obj_name);
      }
      break;

    case UITriggerAction_CrippleShipType:
      {
        StringBuffer ship_name;
        stringbuffer_init(&ship_name);
        FilenamesData const *const filenames = Session_get_filenames(session);
        if (!get_shipname_from_type(&ship_name, filenames_get(filenames, DataType_PolygonMeshes),
                                   item.param.value))
        {
          report_error(SFERROR(NoMem), "", "");
        }
        text = msgs_lookup_subn(token, 1, stringbuffer_get_pointer(&ship_name));
        stringbuffer_destroy(&ship_name);
      }
      break;

    case UITriggerAction_CashBonus:
      {
        char credits[32] = "";
        sprintf(credits, "%d", item.param.value * TriggerCashMultipler);
        text = msgs_lookup_subn(token, 1, credits);
      }
      break;

    case UITriggerAction_MissionTimer:
      {
        char time[32] = "";
        sprintf(time, "%d", item.param.value);
        text = msgs_lookup_subn(token, 1, time);
      }
      break;

    default:
      text = msgs_lookup(token);
      break;
  }

  return text;
}

static inline TriggerFullParam get_trigger(ObjPropDbox *const prop, int const index)
{
  assert(prop);
  assert(prop->nitems <= prop->nalloc);
  assert(index >= 0);
  assert(index < prop->nitems);
  TriggerFullParam *const triggers = prop->items;
  return triggers[index];
}

static inline void set_trigger(ObjPropDbox *const prop, int const index, TriggerFullParam const item)
{
  assert(prop);
  assert(prop->nitems <= prop->nalloc);
  assert(index >= 0);
  assert(index < prop->nitems);
  TriggerFullParam *const triggers = prop->items;
  triggers[index] = item;
}

static bool add_to_list(ObjPropDbox *const prop, TriggerFullParam const item)
{
  int const limit = LOWEST(TriggersMax, UCHAR_MAX);
  assert(prop);
  assert(prop->nitems <= prop->nalloc);
  assert(prop->nalloc <= limit);

  if (prop->nitems == limit) {
    report_error(SFERROR(NumActions), "", "");
    return false;
  }

  int offset = -1;
  if (E(scrolllist_get_selected(0, prop->my_object, ComponentId_TriggerList, -1, &offset))) {
    return false;
  }
  assert(prop->nitems <= INT_MAX);
  offset = (offset < 0 || offset > prop->nitems) ? (int)prop->nitems : offset;
  DEBUGF("inserting at %d in list of length: %d\n", offset, prop->nitems);

  if (prop->nitems == prop->nalloc) {
    int new_size = InitialArraySize;
    int success = 0;
    if (prop->nalloc > 0) {
      if (prop->nalloc < limit / ArrayGrowthFactor) {
        new_size = prop->nalloc * ArrayGrowthFactor;
      } else {
        new_size = limit;
      }
      assert(new_size <= INT_MAX);
      success = flex_extend(&prop->items, new_size * (int)sizeof(TriggerFullParam));
    } else {
      success = flex_alloc(&prop->items, new_size * (int)sizeof(TriggerFullParam));
    }
    if (!success) {
      report_error(SFERROR(NoMem), "", "");
      return false;
    }
    prop->nalloc = new_size;
  }

  if (E(scrolllist_add_item(0, prop->my_object, ComponentId_TriggerList,
                get_list_text(prop, item), NULL, NULL, offset)))
  {
    return false;
  }

  if (prop->edited_item >= offset) {
    assert(prop->edited_item >= 0);
    ++prop->edited_item;
    DEBUGF("Edited item was %d, now %d\n", prop->edited_item - 1, prop->edited_item);
  }

  { /* flex must not budge */
    TriggerFullParam *const triggers = prop->items;
    for (int i = prop->nitems; i > offset; --i) {
      triggers[i] = triggers[i - 1];
    }
  }

  assert(prop->nitems < prop->nalloc);
  prop->nitems++;
  DEBUGF("length of list: %d\n", prop->nitems);

  set_trigger(prop, offset, item);

  prop->last_added = item;
  return true;
}

static void set_buttons_faded(ObjPropDbox const *const prop)
{
  assert(prop);

  int offset = -1;
  int selection_count = 0;
  if (E(scrolllist_get_selected(0, prop->my_object, ComponentId_TriggerList, offset, &offset)) ||
      offset >= prop->nitems) {
    return;
  }

  DEBUGF("selected in list: %d\n", offset);
  selection_count += (offset >= 0 ? 1 : 0);

  E(set_gadget_faded(prop->my_object, ComponentId_DeleteButton, selection_count == 0));

#if SINGLE_DBOX
  E(set_gadget_faded(prop->my_object, ComponentId_ReplaceButton, selection_count == 0));
#else
  if (offset >= 0) {
    /* Check for multiple selection */
    if (E(scrolllist_get_selected(0, prop->my_object, ComponentId_TriggerList, offset, &offset)) ||
        offset >= prop->nitems) {
      return;
    }
    selection_count += (offset >= 0 ? 1 : 0);
  }
  E(set_gadget_faded(prop->my_object, ComponentId_EditButton, selection_count != 1));
#endif
}

static void set_sel_all_faded(ObjPropDbox const *const prop)
{
  assert(prop);
  E(set_gadget_faded(prop->my_object, ComponentId_SelectAllButton, prop->nitems == 0));
  E(set_gadget_faded(prop->my_object, ComponentId_ClearSelectionButton, prop->nitems == 0));
}

static void set_obj_type(ObjPropDbox const *const prop, ObjRef const obj_ref)
{
  assert(prop);
  E(stringset_set_selected(StringSet_IndexedSelection, prop->my_object,
         ComponentId_TypeSet, (char *)obj_type_to_index(obj_ref)));
}

static ObjRef get_obj_type(ObjPropDbox *const prop)
{
  assert(prop);
  ObjRef obj_ref = objects_ref_from_num(Obj_RefMinObject);
  int selected;
  if (!E(stringset_get_selected(StringSet_IndexedSelection, prop->my_object,
         ComponentId_TypeSet, &selected))) {
    obj_ref = index_to_obj_type(selected);
  }
  return obj_ref;
}

static void disp_pos(ObjPropDbox const *const prop)
{
  assert(prop);

  char string[24] = "";
  sprintf(string, "%3.3"PRIMapCoord",%3.3"PRIMapCoord, prop->pos.x, prop->pos.y);
  E(displayfield_set_value(0, prop->my_object, ComponentId_Location, string));
}

static void setup_win(ObjPropDbox *const prop)
{
  assert(prop);

  if (prop->nitems > 0) {
    E(scrolllist_delete_items(0, prop->my_object, ComponentId_TriggerList, 0, prop->nitems-1));
    prop->nitems = 0;
  }
  DEBUGF("length of list: %d\n", prop->nitems);

  set_buttons_faded(prop);

  TriggersIter iter;
  TriggerFullParam item;
  ObjEditContext const *const objects = Session_get_objects(get_session(prop));

  if (objects->triggers) {
    for (MapPoint p = TriggersIter_get_first(&iter, objects->triggers,
                                             &(MapArea){prop->pos, prop->pos}, &item);
         !TriggersIter_done(&iter);
         p = TriggersIter_get_next(&iter, &item))
    {
      DEBUGF("Trigger at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);
      if (!objects_coords_compare(prop->pos, p)) {
        continue;
      }

      if (!add_to_list(prop, item)) {
        break;
      }
    }
  }

  E(set_gadget_faded(prop->my_object, ComponentId_AddButton, objects->triggers == NULL));
  set_obj_type(prop, ObjectsEdit_read_ref(objects, prop->pos));
}

static MapPoint read_chain_coords(ObjPropDbox const *const prop)
{
  assert(prop);

  int x = 0, y = 0;
  E(numberrange_get_value(0, prop->my_add_object, ComponentId_ChainNextX, &x));
  E(numberrange_get_value(0, prop->my_add_object, ComponentId_ChainNextY, &y));
  return (MapPoint){x,y};
}

static void write_chain_coords(ObjPropDbox const *const prop, MapPoint const coords)
{
  assert(prop);
  E(numberrange_set_value(0, prop->my_add_object, ComponentId_ChainNextX, coords.x));
  E(numberrange_set_value(0, prop->my_add_object, ComponentId_ChainNextY, coords.y));
}

static void populate_type_set(ObjPropDbox const *const prop)
{
  assert(prop);

  StringBuffer objs_stringset;
  stringbuffer_init(&objs_stringset);
  FilenamesData const *const filenames = Session_get_filenames(get_session(prop));
  if (!build_objs_stringset(&objs_stringset, filenames_get(filenames, DataType_PolygonMeshes),
                             false, true, true, true, false)) {
    report_error(SFERROR(NoMem), "", "");
  } else {
    E(stringset_set_available(0, prop->my_object,
               ComponentId_TypeSet, stringbuffer_get_pointer(&objs_stringset)));
  }
  stringbuffer_destroy(&objs_stringset);
}

static void populate_ship_set(ObjPropDbox const *const prop)
{
  assert(prop);

  StringBuffer ships_stringset;
  stringbuffer_init(&ships_stringset);
  FilenamesData const *const filenames = Session_get_filenames(get_session(prop));
  if (!build_ships_stringset(&ships_stringset, filenames_get(filenames, DataType_PolygonMeshes),
                             true, true, true, true)) {
    report_error(SFERROR(NoMem), "", "");
  } else {
    E(stringset_set_available(0, prop->my_add_object,
               ComponentId_ShieldType, stringbuffer_get_pointer(&ships_stringset)));
  }
  stringbuffer_destroy(&ships_stringset);
}

static void display_chain_name(ObjPropDbox const *const prop)
{
  assert(prop);

  StringBuffer obj_name;
  stringbuffer_init(&obj_name);

  EditSession *const session = get_session(prop);
  FilenamesData const *const filenames = Session_get_filenames(session);
  ObjEditContext const *const objects = Session_get_objects(session);
  if (!get_objname_from_type(&obj_name, filenames_get(filenames, DataType_PolygonMeshes),
                             ObjectsEdit_read_ref(objects, read_chain_coords(prop))))
  {
    report_error(SFERROR(NoMem), "", "");
  }

  E(displayfield_set_value(0, prop->my_add_object, ComponentId_ChainNext,
                           stringbuffer_get_pointer(&obj_name)));

  stringbuffer_destroy(&obj_name);
}

static void maybe_set_faded(ObjPropDbox const *const prop, ComponentId const component_id,
                            bool const faded)
{
  assert(prop);

  if (component_id != NULL_ComponentId) {
    E(set_gadget_faded(prop->my_add_object, component_id, faded));
    if (component_id == ComponentId_ChainDelay) {
      for (size_t i = 0; i < ARRAY_SIZE(chain_components); ++i) {
        E(set_gadget_faded(prop->my_add_object, chain_components[i], faded));
      }
    }
  }
}

static void update_for_radio(ObjPropDbox const *const prop, ComponentId const old_on_button,
  ComponentId const new_on_button)
{
  assert(prop);
  assert(old_on_button != NULL_ComponentId);
  assert(new_on_button != NULL_ComponentId);

  if (old_on_button == new_on_button) {
    return;
  }

  ComponentId const old_gadget = gadget_from_ui(ui_from_radio(old_on_button));
  maybe_set_faded(prop, old_gadget, true);

  ComponentId new_gadget = gadget_from_ui(ui_from_radio(new_on_button));
  maybe_set_faded(prop, new_gadget, false);

  if (new_gadget == ComponentId_ShieldType) {
    new_gadget = NULL_ComponentId;
  }

  E(window_set_default_focus(0, prop->my_add_object,
    new_gadget == NULL_ComponentId ? SetDefaultFocusInvisible : new_gadget));

  unsigned int state;
  if (new_gadget != NULL_ComponentId &&
      !E(toolbox_get_object_state(0, prop->my_add_object, &state)) &&
      (state & Toolbox_GetObjectState_Showing)) {
    E(gadget_set_focus(0, prop->my_add_object, new_gadget));
  }
}

static void write_gadgets(ObjPropDbox const *const prop, TriggerFullParam const item)
{
  assert(prop);

  UITriggerAction const ui_act = ui_from_action(item.param);
  ComponentId const new_on_button = radio_from_ui(ui_act);

  ComponentId old_on_button;
  if (E(radiobutton_get_state(0, prop->my_add_object,
         ComponentId_MissionTarget, NULL, &old_on_button))) {
    return;
  }

  if (new_on_button != old_on_button) {
    E(radiobutton_set_state(0, prop->my_add_object, new_on_button, 1));
    update_for_radio(prop, old_on_button, new_on_button);
  }

  ComponentId const gadget = gadget_from_ui(ui_act);

  if (gadget != NULL_ComponentId)
  {
    if (ui_act == UITriggerAction_CrippleShipType) {
      E(stringset_set_selected(StringSet_IndexedSelection, prop->my_add_object, gadget,
        (char *)ship_type_to_index(item.param.value)));
    } else {
      E(numberrange_set_value(0, prop->my_add_object, gadget, item.param.value));
    }
  }

  if (ui_act == UITriggerAction_ChainReaction) {
    write_chain_coords(prop, item.next_coords);
    display_chain_name(prop);
  }
}

static TriggerFullParam read_gadgets(ObjPropDbox const *const prop)
{
  assert(prop);

  ComponentId radio_selected;
  if (E(radiobutton_get_state(0, prop->my_add_object,
         ComponentId_MissionTarget, NULL, &radio_selected))) {
    return (TriggerFullParam){{0}};
  }

  UITriggerAction const ui_act = ui_from_radio(radio_selected);
  ComponentId const gadget = gadget_from_ui(ui_act);
  int value = 0;

  if (gadget != NULL_ComponentId)
  {
    if (ui_act == UITriggerAction_CrippleShipType) {
      E(stringset_get_selected(StringSet_IndexedSelection, prop->my_add_object, gadget, &value));
      value = index_to_ship_type(value);
    } else {
      E(numberrange_get_value(0, prop->my_add_object, gadget, &value));
    }
  }

  MapPoint coords = {0};
  if (ui_act == UITriggerAction_ChainReaction) {
    coords = read_chain_coords(prop);
  }

  return (TriggerFullParam){.param = action_from_ui(ui_act, value), .next_coords = coords};
}

static void delete_sel_triggers(ObjPropDbox *const prop)
{
  assert(prop);
  assert(prop->nitems <= prop->nalloc);

  if (prop->nitems > 0) {
    int offset = -1;
    do {
      if (E(scrolllist_get_selected(0, prop->my_object, ComponentId_TriggerList, offset, &offset))) {
        return;
      }
      DEBUGF("selected in list: %d\n", offset);
      if (offset < 0 || offset >= prop->nitems) {
        break;
      }

      E(scrolllist_delete_items(ScrollList_DeleteItems_DoNotJumpToTop,
        prop->my_object, ComponentId_TriggerList, offset, offset));

      if (offset == prop->edited_item) {
        E(toolbox_hide_object(0, prop->my_add_object));
        assert(prop->edited_item >= 0);
        prop->edited_item = -1;
      } else if (offset < prop->edited_item) {
        assert(prop->edited_item >= 0);
        --prop->edited_item;
      }

      assert(prop->nitems > 0);
      prop->nitems--;
      DEBUGF("length of list: %d\n", prop->nitems);

      { /* flex must not budge */
        int const last = prop->nitems;
        TriggerFullParam *const triggers = prop->items;
        for (int i = offset; i < last; ++i) {
          triggers[i] = triggers[i + 1];
        }
      }

      offset--;
    } while (prop->nitems > 0);
  }
  set_buttons_faded(prop);
}

static void add_trigger(ObjPropDbox *const prop)
{
  add_to_list(prop, read_gadgets(prop));
}

#if !SINGLE_DBOX
static void edit_trigger(ObjPropDbox *const prop)
{
  assert(prop);
  assert(prop->nitems <= prop->nalloc);
  assert(prop->edited_item >= 0);
  assert(prop->edited_item < prop->nitems);
  DEBUGF("editing %d in list of length: %d\n", prop->edited_item, prop->nitems);

  TriggerFullParam const item = read_gadgets(prop);

  if (E(scrolllist_set_item_text(0, prop->my_object, ComponentId_TriggerList,
                get_list_text(prop, item), prop->edited_item)))
  {
    return;
  }

  set_trigger(prop, prop->edited_item, item);
}
#endif

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  ObjPropDbox *const prop = handle;

  switch (id_block->self_component) {
    case ComponentId_DeleteButton:
      delete_sel_triggers(prop);
      break;
#if SINGLE_DBOX
    case ComponentId_ReplaceButton:
      delete_sel_triggers(prop);
      add_trigger(prop);
      break;
    case ComponentId_AddButton:
      add_trigger(prop);
      break;
#else
    case ComponentId_EditButton:
      {
        int offset = -1;
        if (E(scrolllist_get_selected(0, prop->my_object, ComponentId_TriggerList, offset, &offset)) ||
            offset < 0 || offset >= prop->nitems) {
          return 1;
        }
        prop->edited_item = offset;
      }
      open_topleftofwin(0, prop->my_add_object, prop->my_object,
                        prop->my_object, id_block->self_component);
      break;
    case ComponentId_AddButton:
      prop->edited_item = -1;
      open_topleftofwin(0, prop->my_add_object, prop->my_object,
                        prop->my_object, id_block->self_component);
      break;
#endif
    case ComponentId_SelectAllButton:
      E(scrolllist_select_item(ScrollList_SelectionChangingMethod_OnAll, prop->my_object,
        ComponentId_TriggerList, 0));

      set_buttons_faded(prop);
      break;
    case ComponentId_ClearSelectionButton:
      E(scrolllist_deselect_item(ScrollList_SelectionChangingMethod_OnAll, prop->my_object,
        ComponentId_TriggerList, 0));

      set_buttons_faded(prop);
      break;
    case ComponentId_CancelButton:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        disp_pos(prop);
        setup_win(prop);
      }
      break;
    case ComponentId_SetButton:
      prop->keep = true;
      if (ObjectsMode_set_properties(get_editor(prop), prop->pos, get_obj_type(prop),
                                     prop->items, (size_t)prop->nitems))
      {
        if (!TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
          E(toolbox_hide_object(0, prop->my_object));
        }
      }
      prop->keep = false;
      break;
    default:
      return 0;
  }

  set_sel_all_faded(prop);
  return 1; /* event handled */
}

#if !SINGLE_DBOX
static void setup_win_from_list(ObjPropDbox *const prop)
{
  assert(prop);
  char const *tok = "AddBut";
  if (prop->edited_item >= 0) {
    assert(prop->edited_item < prop->nitems);
    write_gadgets(prop, get_trigger(prop, prop->edited_item));
    tok = "EditBut";
  } else {
    write_gadgets(prop, prop->last_added);
  }

  E(actionbutton_set_text(0, prop->my_add_object, ComponentId_SetButton2, msgs_lookup(tok)));
}

static int actionbutton_selected_2(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  ObjPropDbox *const prop = handle;

  switch (id_block->self_component) {
    case ComponentId_CancelButton2:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        setup_win_from_list(prop);
      }
      break;
    case ComponentId_SetButton2:
      if (prop->edited_item >= 0) {
        edit_trigger(prop);
      } else {
        add_trigger(prop);
      }
      break;
    default:
      return 0;
  }

  set_sel_all_faded(prop);
  return 1; /* event handled */
}
#endif

static int scrolllist_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(id_block);
  ObjPropDbox *const prop = handle;
  ScrollListSelectionEvent const *const slse = (void *)event;

  set_buttons_faded(prop);

  if (slse->item >= 0 && slse->item < prop->nitems) {
    if ((slse->flags & ScrollList_Selection_Flags_DoubleClick) &&
        !(slse->flags & ScrollList_Selection_Flags_AdjustClick)) {
#if SINGLE_DBOX
      write_gadgets(prop, get_trigger(prop, slse->item));
#else
      prop->edited_item = slse->item;
      open_topleftofwin(0, prop->my_add_object, prop->my_object,
                        prop->my_object, id_block->self_component);
#endif
    }
  }

  return 1;
}

static int radiobutton_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  ObjPropDbox *const prop = handle;
  RadioButtonStateChangedEvent const *const rbsce = (void *)event;

  update_for_radio(prop, rbsce->old_on_button, id_block->self_component);
  return 1;
}

static int numberrange_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  ObjPropDbox *const prop = handle;

  if (id_block->self_component != ComponentId_ChainNextX &&
      id_block->self_component != ComponentId_ChainNextY) {
    return 0;
  }

  display_chain_name(prop);
  return 1;
}

static int ObjPropDboxes_about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  ObjPropDbox *const prop = handle;

  populate_type_set(prop);
  disp_pos(prop);
  setup_win(prop);
  set_sel_all_faded(prop);

  return 0; // two of these event handlers
}

static void delete_dbox(ObjPropDbox *const prop)
{
  assert(prop != NULL);
  E(remove_event_handlers_delete(prop->my_object));
#if !SINGLE_DBOX
  E(remove_event_handlers_delete(prop->my_add_object));
#endif
  if (prop->items) {
    flex_free(&prop->items);
  }
  ObjPropDbox *const removed = intdict_remove_value(&prop->prop_dboxes->sa_window, prop->my_add_window, NULL);
  assert(removed == prop);
  NOT_USED(removed);
  free(prop);
}

static int ObjPropDboxes_has_been_hidden(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  ObjPropDbox *const prop = handle;
  ObjPropDbox *const removed = intdict_remove_value(&prop->prop_dboxes->sa_coords, objects_coords_to_key(prop->pos), NULL);
  assert(removed == prop);
  NOT_USED(removed);
  delete_dbox(prop);
  return 1;
}

static int ObjPropDboxes_about_to_be_shown_2(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  ObjPropDbox *const prop = handle;

  ObjEditContext const *const objects = Session_get_objects(get_session(prop));
  for (UITriggerAction ui_act = UITriggerAction_First; ui_act < UITriggerAction_Count; ++ui_act) {
    E(set_gadget_faded(prop->my_add_object, radio_from_ui(ui_act), !objects->triggers));
  }

  populate_ship_set(prop);
#if SINGLE_DBOX
  write_gadgets(prop, prop->last_added);
#else
  char *const tok = prop->edited_item >= 0 ? "EditTrigger" : "AddTrigger";
  E(window_set_title(0, prop->my_add_object, msgs_lookup(tok)));
  setup_win_from_list(prop);
#endif
  return 0; // two of these event handlers
}

static bool register_event_handlers(ObjPropDbox *const prop)
{
  assert(prop != NULL);
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } tbox_handlers[] = {
    { Window_AboutToBeShown, ObjPropDboxes_about_to_be_shown },
    { Window_HasBeenHidden, ObjPropDboxes_has_been_hidden },
    { ActionButton_Selected, actionbutton_selected },
    { ScrollList_Selection, scrolllist_selection },
  };

  for (size_t i = 0; i < ARRAY_SIZE(tbox_handlers); ++i) {
    if (E(event_register_toolbox_handler(prop->my_object,
                                      tbox_handlers[i].event_code,
                                      tbox_handlers[i].handler,
                                      prop)))
      return false;
  }
  return true;
}

static bool register_event_handlers2(ObjPropDbox *const prop)
{
  assert(prop != NULL);
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } tbox_handlers[] = {
    { Window_AboutToBeShown, ObjPropDboxes_about_to_be_shown_2 },
#if !SINGLE_DBOX
    { ActionButton_Selected, actionbutton_selected_2 },
#endif
    { RadioButton_StateChanged, radiobutton_changed },
    { NumberRange_ValueChanged, numberrange_changed },
  };

  for (size_t i = 0; i < ARRAY_SIZE(tbox_handlers); ++i) {
    if (E(event_register_toolbox_handler(prop->my_add_object,
                                      tbox_handlers[i].event_code,
                                      tbox_handlers[i].handler,
                                      prop)))
      return false;
  }
  return true;
}

static void update_title(ObjPropDbox *const prop)
{
  assert(prop != NULL);
  char const *const file_name = Session_get_filename(get_session(prop));
  E(window_set_title(0, prop->my_object,
                     msgs_lookup_subn("OPropTitle", 1, pathtail(file_name, 1))));
}

static ObjPropDbox *create_dbox(ObjPropDboxes *const prop_dboxes, MapPoint const pos)
{
  assert(prop_dboxes != NULL);
  DEBUGF("Creating properties dbox for %"PRIMapCoord",%"PRIMapCoord"\n", pos.x, pos.y);

  ObjPropDbox *const prop = malloc(sizeof(*prop));
  if (!prop) {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }

  *prop = (ObjPropDbox){
    .prop_dboxes = prop_dboxes,
    .last_added = {.param.action = TriggerAction_MissionTarget},
    .edited_item = -1,
    .pos = pos,
    .keep = false,
  };

  if (!E(toolbox_create_object(0, SINGLE_DBOX ? "ObjProp" : "ObjPropB", &prop->my_object))) {
    DEBUG("ObjProp object id is %d", prop->my_object);

    if (register_event_handlers(prop)) {
      E(scrolllist_set_state(0, prop->my_object, ComponentId_TriggerList,
                             ScrollList_MultipleSelections));
      update_title(prop);
#if SINGLE_DBOX
      prop->my_add_object = prop->my_object;
#else
      if (!E(toolbox_create_object(0, "ObjProp2", &prop->my_add_object)))
#endif
      {
        DEBUG("ObjProp2 object id is %d", prop->my_add_object);

        if (!E(window_get_wimp_handle(0, prop->my_add_object, &prop->my_add_window)) &&
             register_event_handlers2(prop))
        {
          if (!intdict_insert(&prop_dboxes->sa_window, prop->my_add_window, prop, NULL))
          {
            report_error(SFERROR(NoMem), "", "");
          }
          else
          {
            if (!intdict_insert(&prop_dboxes->sa_coords, objects_coords_to_key(pos), prop, NULL))
            {
              report_error(SFERROR(NoMem), "", "");
            }
            else
            {
              update_title(prop);
              return prop;
            }
            ObjPropDbox *const removed = intdict_remove_value(&prop_dboxes->sa_window, prop->my_add_window, NULL);
            assert(removed == prop);
            NOT_USED(removed);
          }
        }
#if !SINGLE_DBOX
        (void)remove_event_handlers_delete(prop->my_add_object);
#endif
      }
    }
    (void)remove_event_handlers_delete(prop->my_object);
  }
  free(prop);
  return NULL;
}

static void destroy_cb(IntDictKey key, void *value, void *arg)
{
  NOT_USED(key);
  NOT_USED(arg);
  delete_dbox(value);
}

/* ---------------- Public functions ---------------- */

void ObjPropDboxes_init(ObjPropDboxes *const prop_dboxes, Editor *editor)
{
  assert(prop_dboxes);
  assert(editor);
  *prop_dboxes = (ObjPropDboxes){.editor = editor};
  intdict_init(&prop_dboxes->sa_coords);
  intdict_init(&prop_dboxes->sa_window);
}

void ObjPropDboxes_destroy(ObjPropDboxes *const prop_dboxes)
{
  assert(prop_dboxes);
  intdict_destroy(&prop_dboxes->sa_coords, destroy_cb, NULL);
  intdict_destroy(&prop_dboxes->sa_window, NULL, NULL);
}

void ObjPropDboxes_update_title(ObjPropDboxes *const prop_dboxes)
{
  assert(prop_dboxes);
  IntDictVIter iter;
  for (ObjPropDbox *prop_dbox = intdictviter_all_init(&iter, &prop_dboxes->sa_coords);
       prop_dbox != NULL;
       prop_dbox = intdictviter_advance(&iter)) {
    update_title(prop_dbox);
  }
}

void ObjPropDboxes_update_for_move(ObjPropDboxes *const prop_dboxes,
                                   MapPoint const old_pos, MapPoint const new_pos)
{
  if (objects_coords_compare(old_pos, new_pos)) {
    return;
  }

  ObjPropDbox *const prop_dbox = intdict_remove_value(&prop_dboxes->sa_coords,
                                   objects_coords_to_key(old_pos), NULL);
  if (!prop_dbox) {
    return;
  }

  assert(objects_coords_compare(prop_dbox->pos, old_pos));
  if (intdict_insert(&prop_dboxes->sa_coords, objects_coords_to_key(new_pos), prop_dbox, NULL)) {
    prop_dbox->pos = new_pos;
    disp_pos(prop_dbox);
  } else {
    report_error(SFERROR(NoMem), "", "");
    delete_dbox(prop_dbox);
  }
}

static bool split_callback(MapArea const *const bbox, void *const arg)
{
  ObjPropDboxes *const prop_dboxes = arg;

  IntDictKey const min_key = objects_coords_to_key(bbox->min);
  IntDictKey const max_key = objects_coords_to_key(bbox->max);
  assert(min_key <= max_key);

  IntDictVIter iter;
  for (ObjPropDbox *prop_dbox = intdictviter_init(&iter, &prop_dboxes->sa_coords, min_key, max_key);
       prop_dbox != NULL;
       prop_dbox = intdictviter_advance(&iter)) {
    if (objects_bbox_contains(bbox, prop_dbox->pos) && !prop_dbox->keep) {
      intdictviter_remove(&iter);
      delete_dbox(prop_dbox);
    }
  }
  return false;
}

void ObjPropDboxes_update_for_del(ObjPropDboxes *const prop_dboxes,
                                  MapArea const *const bbox)
{
  /* Split the map area first otherwise the min and max indices are nonsense */
  objects_split_area(bbox, split_callback, prop_dboxes);
}

void ObjPropDboxes_open(ObjPropDboxes *const prop_dboxes, MapPoint const pos,
                        EditWin *const edit_win)
{
  IntDictKey const key = objects_coords_to_key(pos);

  ObjPropDbox *prop_dbox = intdict_find_value(&prop_dboxes->sa_coords, key, NULL);
  if (!prop_dbox) {
    prop_dbox = create_dbox(prop_dboxes, pos);
  } else {
    assert(objects_coords_to_key(prop_dbox->pos) == key);
  }
  if (prop_dbox) {
    EditWin_show_dbox(edit_win, 0, prop_dbox->my_object);
  }
}

static bool dropped_on_icon_4(ObjPropDbox const *const prop, int const icon,
  ComponentId const component)
{
  assert(prop);

  int drop_icons[4];
  int nbytes;
  if (E(gadget_get_icon_list(0, prop->my_add_object, component, drop_icons,
           sizeof(drop_icons), &nbytes))) {
    return false;
  }

  assert(nbytes >= 0);
  assert((size_t)nbytes < sizeof(drop_icons));
  DEBUGF("get_icon_list reports nbytes = %d\n", nbytes);

  size_t const nicons = (size_t)nbytes / sizeof(drop_icons[0]);
  for (size_t j = 0; j < nicons; ++j) {
    DEBUGF("Component = 0x%x has icon[%zu] = %d\n",
           component, j, drop_icons[j]);
    if (icon == drop_icons[j]) {
      return true;
    }
  }
  return false;
}

static bool dropped_on_icon_3(ObjPropDbox const *const prop, int const icon,
  ComponentId const *const inner_comp, size_t const ncomp)
{
  for (size_t k = 0; k < ncomp; ++k) {
    if (dropped_on_icon_4(prop, icon, inner_comp[k])) {
      return true;
    }
  }
  return false;
}

static bool dropped_on_icon_2(ObjPropDbox const *const prop, int const icon,
  ComponentId const component)
{
  assert(prop);

  int type;
  if (E(gadget_get_type(0, prop->my_add_object, component, &type))) {
    return false;
  }

  ComponentId inner_comp[4];

  switch (type) {
  case NumberRange_Base:
    if (E(numberrange_get_components(
           NumberRange_GetComponents_ReturnNumericalField|
           NumberRange_GetComponents_ReturnLeftAdjuster|
           NumberRange_GetComponents_ReturnRightAdjuster,
           prop->my_add_object, component,
           &inner_comp[0], &inner_comp[1], &inner_comp[2], &inner_comp[3]))) {
      return false;
    }
    if (dropped_on_icon_3(prop, icon, inner_comp, ARRAY_SIZE(inner_comp))) {
      return true;
    }
    break;

  case StringSet_Base:
    if (E(stringset_get_components(
           StringSet_GetComponents_ReturnAlphaNumericField|
           StringSet_GetComponents_ReturnPopUpMenu,
           prop->my_add_object, component,
           &inner_comp[0], &inner_comp[1]))) {
      return false;
    }
    if (dropped_on_icon_3(prop, icon, inner_comp, 2)) {
      return true;
    }
    break;

  default:
    if (dropped_on_icon_4(prop, icon, component)) {
      return true;
    }
    break;
  }
  return false;
}

static bool dropped_on_icon(ObjPropDbox const *const prop, int const icon)
{
  if (dropped_on_icon_2(prop, icon, ComponentId_Chain)) {
    return true;
  }

  for (size_t i = 0; i < ARRAY_SIZE(chain_components); ++i) {
    if (dropped_on_icon_2(prop, icon, chain_components[i])) {
      return true;
    }
  }
  return false;
}

bool ObjPropDboxes_drag_obj_link(ObjPropDboxes *const prop_dboxes,
  int const window, int const icon, MapPoint const pos)
{
  DEBUGF("Drop in icon %d of window %d\n", icon, window);

  ObjPropDbox *const prop_dbox = intdict_find_value(&prop_dboxes->sa_window, window, NULL);
  if (!prop_dbox) {
    DEBUGF("Drop not in window %d\n", window);
    return false;
  }

  if (dropped_on_icon(prop_dbox, icon)) {
    TriggerFullParam item = {
      .param = {
        .action = TriggerAction_ChainReaction,
      },
      .next_coords = pos,
    };
    if (E(numberrange_get_value(0, prop_dbox->my_add_object,
           ComponentId_ChainDelay, &item.param.value))) {
      return false;
    }
    write_gadgets(prop_dbox, item);
    return true;
  }

  return false;
}
