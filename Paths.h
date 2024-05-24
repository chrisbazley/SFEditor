/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission flightpaths
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Paths_h
#define Paths_h

struct Reader;
struct Writer;
#include "SFError.h"

#include "CoarseCoord.h"

typedef struct PathsData PathsData;
typedef struct Path Path;
typedef struct Waypoint Waypoint;

void paths_init(PathsData *paths);
SFError paths_read(PathsData *paths, struct Reader *reader);
SFError paths_read_pad(PathsData *paths, struct Reader *reader);
void paths_pre_write(PathsData *paths);
void paths_write(PathsData *paths, struct Writer *writer);
void paths_write_pad(PathsData *paths, struct Writer *writer);
Path *paths_add(PathsData *paths);
void paths_destroy(PathsData *paths);

Path *path_from_index(PathsData *paths, int index);
int path_get_index(Path const *path);
Waypoint *path_add_waypoint(Path *path, CoarsePoint3d coords);
void path_delete(Path * path);

Path *waypoint_get_path(Waypoint const *waypoint);
Waypoint *waypoint_from_index(Path *path, int index);
int waypoint_get_index(Waypoint const *waypoint);
void waypoint_delete(Waypoint *waypoint);



#endif
