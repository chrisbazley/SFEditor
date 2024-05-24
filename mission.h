/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission file
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Mission_h
#define Mission_h

#include "Pyram.h"
#include "Player.h"
#include "Defenc.h"
#include "Triggers.h"
#include "Infos.h"
#include "FPerf.h"
#include "BPerf.h"
#include "Paths.h"
#include "Ships.h"
#include "Filenames.h"
#include "Briefing.h"
#include "Clouds.h"
#include "DFile.h"

typedef struct MissionData MissionData;

typedef enum {
  MissionType_Normal,
  MissionType_Space,
  MissionType_Cyber,
} MissionType;

MissionData *mission_create(void);

PyramidData *mission_get_pyramid(MissionData *mission);
PlayerData *mission_get_player(MissionData *mission);
DefencesData *mission_get_defences(MissionData *mission);
TriggersData *mission_get_triggers(MissionData *mission);
TargetInfosData *mission_get_target_infos(MissionData *mission);
FighterPerformData *mission_get_fighter_perform(MissionData *mission);
BigPerformData *mission_get_big_perform(MissionData *mission);
PathsData *mission_get_paths(MissionData *mission);
ShipsData *mission_get_ships(MissionData *mission);
FilenamesData *mission_get_filenames(MissionData *mission);
BriefingData *mission_get_briefing(MissionData *mission);
CloudColData *mission_get_cloud_colours(MissionData *mission);
DFile *mission_get_dfile(MissionData *mission);

MissionType mission_get_type(MissionData const *mission);
void mission_set_type(MissionData *mission, MissionType type);

bool mission_get_dock_to_finish(MissionData const *mission);
void mission_set_dock_to_finish(MissionData *mission, bool dock_to_finish);

bool mission_get_scanners_down(MissionData const *mission);
void mission_set_scanners_down(MissionData *mission, bool scanners_down);

bool mission_get_impervious_map(MissionData const *mission);
void mission_set_impervious_map(MissionData *mission, bool impervious_map);

int mission_get_time_limit(MissionData const *mission);
void mission_set_time_limit(MissionData *mission, int time_limit);
void mission_disable_time_limit(MissionData *mission);
bool mission_time_limit_is_disabled(MissionData const *mission);

#endif
