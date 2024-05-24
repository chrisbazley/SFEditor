/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Editing session data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef SessionData_h
#define SessionData_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "toolbox.h"

#include "SprFormats.h"
#include "Scheduler.h"
#include "linkedlist.h"
#include "StringBuff.h"

#include "Session.h"
#include "BriefDbox.h"
#include "SpecialShip.h"
#include "EditWinData.h"
#include "ObjectsEdit.h"
#include "GfxConfig.h"
#include "EditMode.h"
#include "ObjEditCtx.h"
#include "MapEditCtx.h"
#include "InfoEditCtx.h"
#include "FPerfDbox.h"
#include "BPerfDbox.h"
#include "DFile.h"
#include "DataType.h"
#include "EditorData.h"
#include "IntDict.h"

typedef struct EditWinList
{
  EditWin edit_win;
#if PER_VIEW_SELECT
  bool edit_win_is_valid;
  Editor editor;
#endif
}
EditWinList;

struct EditSession
{
  LinkedListItem all_link;
  IntDict edit_wins_array;
#if !PER_VIEW_SELECT
  Editor editor;
#endif

  /* Mission filename ("E.E_01"), map filename ("Academy1") or path to oddball
file ("IDEFS::Tamzin.$.H.H_08") */
  StringBuffer filename;
  StringBuffer edit_win_titles;

  DFile *dfiles[DataType_SessionCount];

  struct MapEditContext map;
  struct ObjEditContext objects;
  struct InfoEditContext infos;

  /* Editable data areas - NULL means edit_win doesn't possess one */
  struct MissionData *mission;

  /* Read-only data areas */
  struct ObjGfx *graphics; /* (Map/Mission) */
  struct MapTex *textures; /* (Map/Mission) */
  struct HillColData *hill_colours; /* (Map/Mission) */
  struct PolyColData *poly_colours; /* (Map/Mission) */

  /* Filenames of graphics to use and cloud colours
     (copied from mission data if any loaded): */
  GfxConfig gfx_config;

  InterfaceType ui_type; /* see edit_win type codes above */

  unsigned char number_of_edit_wins;

  bool oddball_file:1, desired_animate_map:1, actual_animate_map:1,
       has_briefing:1, has_special_ship:1, untitled:1;
#if !PER_VIEW_SELECT
  bool has_editor:1;
#endif

  bool has_fperf[ShipType_Fighter4 - ShipType_Fighter1 + 1];
  bool has_bperf[ShipType_Big3 - ShipType_Big1 + 1];

  BriefDboxData briefing_data;
  FPerfDboxData fperf[ShipType_Fighter4 - ShipType_Fighter1 + 1];
  BPerfDboxData bperf[ShipType_Big3 - ShipType_Big1 + 1];
  SpecialShipData special_ship_data;

  SchedulerTime last_update_time;
};

#endif
