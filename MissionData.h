/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef MissionData_h
#define MissionData_h

#include <stdbool.h>
#include "BriefingData.h"
#include "FilenamesData.h"
#include "ShipsData.h"
#include "PathsData.h"
#include "InfosData.h"
#include "TriggersData.h"
#include "DefencData.h"
#include "PlayerData.h"
#include "FPerfData.h"
#include "BPerfData.h"
#include "PyramData.h"
#include "CloudsData.h"
#include "DFileData.h"
#include "Mission.h"

struct MissionData {
  MissionType type;
  struct PyramidData pyramid;
  struct PlayerData player;
  struct DefencesData defences;
  struct TriggersData triggers;
  struct TargetInfosData target_infos;
  struct FighterPerformData fighter_perform;
  struct BigPerformData big_perform;
  struct PathsData paths;
  struct ShipsData ships;
  struct FilenamesData filenames;
  struct BriefingData briefing;
  struct CloudColData clouds;
  bool dock_to_finish:1;
  bool scanners_down:1;
  bool impervious_map:1;
  int time_limit;
  DFile dfile;
};

#endif
