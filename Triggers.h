/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission action triggers
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Triggers_h
#define Triggers_h

struct Reader;
struct Writer;

#include <stdbool.h>
#include "SFError.h"
#include "MapCoord.h"
#include "LinkedList.h"
#include "IntDict.h"

enum {
  TriggersMax = 64,
};

typedef struct TriggersData TriggersData;
typedef struct Trigger Trigger;

typedef enum
{
#define DECLARE_TRIGGER(t) TriggerAction_ ## t,
#include "DeclTrig.h"
#undef DECLARE_TRIGGER
}
TriggerAction;

char const *TriggerAction_to_string(TriggerAction action);

enum {
  TriggerActivateDefences = 255,
  TriggerCashMultipler = 10,
  TriggerChainReactionMultipler = 2,
};

typedef struct {
  TriggerAction action;
  int value;
} TriggerParam;

typedef struct {
  TriggerParam param;
  MapPoint next_coords;
} TriggerFullParam;

SFError triggers_init(TriggersData *triggers);
size_t triggers_get_max_losses(TriggersData const *triggers);
void triggers_set_max_losses(TriggersData *triggers, size_t max);
size_t triggers_get_count(TriggersData *triggers);

void triggers_destroy(TriggersData *triggers);

SFError triggers_add(TriggersData *triggers,
  MapPoint coords, TriggerFullParam fparam);

bool triggers_check_locn(TriggersData *triggers, MapPoint map_pos);

size_t triggers_count_locn(TriggersData *triggers, MapPoint map_pos);
size_t triggers_count_bbox(TriggersData *triggers, MapArea const *map_area);

typedef struct
{
  TriggersData *triggers;
  IntDictVIter viter;
  Trigger *trigger;
  MapArea map_area;
  bool done;
}
TriggersIter;

MapPoint TriggersIter_get_first(TriggersIter *iter, TriggersData *triggers,
  MapArea const *map_area, TriggerFullParam *fparam);

MapPoint TriggersIter_get_next(TriggersIter *iter, TriggerFullParam *fparam);

static inline bool TriggersIter_done(TriggersIter const *iter)
{
  assert(iter);
  assert(iter->triggers);
  return iter->done;
}

void TriggersIter_del_current(TriggersIter *iter);

typedef struct
{
  TriggersData *triggers;
  IntDictVIter viter;
  Trigger *trigger;
  Trigger *next_chain;
  MapArea map_area;
  bool done;
}
TriggersChainIter;

MapPoint TriggersChainIter_get_first(TriggersChainIter *iter, TriggersData *triggers,
  MapArea const *map_area, TriggerFullParam *fparam);

MapPoint TriggersChainIter_get_next(TriggersChainIter *iter, TriggerFullParam *fparam);

static inline bool TriggersChainIter_done(TriggersChainIter const *iter)
{
  assert(iter);
  assert(iter->triggers);
  return iter->done;
}

void TriggersChainIter_del_current(TriggersChainIter *iter);

void triggers_write(TriggersData *triggers,
  struct Writer *writer);

void triggers_write_pad(TriggersData *triggers,
  struct Writer *writer);

SFError triggers_read(TriggersData *triggers,
  struct Reader *reader);

SFError triggers_read_pad(TriggersData *triggers,
  struct Reader *reader);

void triggers_write_max_losses(TriggersData *triggers,
  struct Writer *writer);

SFError triggers_read_max_losses(TriggersData *triggers,
  struct Reader *reader);

void triggers_cleanup(TriggersData *triggers);

#endif
