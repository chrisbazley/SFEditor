/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission target information points
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Infos_h
#define Infos_h

struct Reader;
struct Writer;

#include <stddef.h>
#include "MapCoord.h"
#include "SFError.h"
#include "IntDict.h"

enum {
  TargetInfoMax = 6, // Never more than 4 in practice
  InfoMaxClickDist = 5, // any Manhattan distance less than 6 in the game
};

typedef enum {
  TargetInfoTextIndex_First,
  TargetInfoTextIndex_Type = TargetInfoTextIndex_First,
  TargetInfoTextIndex_Details,
  TargetInfoTextIndex_Count,
} TargetInfoTextIndex;

typedef struct TargetInfosData TargetInfosData;
typedef struct TargetInfo TargetInfo;

void target_infos_init(TargetInfosData *target_infos);
void target_infos_destroy(TargetInfosData *target_infos);

SFError target_infos_add(TargetInfosData *target_infos,
  MapPoint pos, size_t *index);

/* set_text not add_text because a fixed number of strings appear
   at predetermined screen coordinates */
SFError target_info_set_text(TargetInfo *info,
  TargetInfoTextIndex index, char const *string);

char const *target_info_get_text(TargetInfo const *info,
  TargetInfoTextIndex index);

MapPoint target_info_get_pos(TargetInfo const *info);
size_t target_info_set_pos(TargetInfo *info, MapPoint pos);

int target_info_get_id(TargetInfo const *const info);

size_t target_info_delete(TargetInfo *info);

size_t target_infos_get_count(TargetInfosData const *target_infos);
size_t target_infos_get_text_count(TargetInfosData const *target_infos);

TargetInfo *target_info_from_index(TargetInfosData const *target_infos, size_t index);

SFError target_infos_read(TargetInfosData *target_infos,
  struct Reader *reader);

SFError target_infos_read_pad(TargetInfosData *target_infos,
  struct Reader *reader);

void target_infos_write(TargetInfosData *target_infos,
  struct Writer *writer);

void target_infos_write_pad(TargetInfosData *target_infos,
  struct Writer *writer);

int target_infos_write_text_offsets(TargetInfosData *target_infos,
  struct Writer *writer, int offset);

void target_infos_write_texts(TargetInfosData *target_infos, struct Writer *writer);

SFError target_infos_read_texts(TargetInfosData *target_infos,
                                long int const *offsets, size_t count,
                                struct Reader *reader);

typedef struct {
  IntDict *dict;
  MapArea map_area;
  size_t next_index, end;
  bool done;
} TargetInfosIter;

size_t TargetInfosIter_get_first(TargetInfosIter *iter,
  TargetInfosData *infos, MapArea const *map_area);

size_t TargetInfosIter_get_next(TargetInfosIter *iter);

void TargetInfosIter_del_current(TargetInfosIter *iter);

static inline bool TargetInfosIter_done(TargetInfosIter const *const iter)
{
  assert(iter);
  assert(MapArea_is_valid(&iter->map_area));
  return iter->done;
}

#endif
