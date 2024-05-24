/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects editing mode change
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef ObjEditChg_h
#define ObjEditChg_h

#include <stdbool.h>

typedef struct ObjEditChanges
{
  unsigned long int refs_changed;
  unsigned long int triggers_added;
  unsigned long int triggers_changed;
  unsigned long int triggers_deleted;
}
ObjEditChanges;

void ObjEditChanges_init(ObjEditChanges *change_info);

static inline bool ObjEditChanges_triggers_changed(const ObjEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->triggers_added ||
         change_info->triggers_changed ||
         change_info->triggers_deleted;
}

static inline bool ObjEditChanges_refs_changed(const ObjEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->refs_changed;
}

static inline bool ObjEditChanges_is_changed(const ObjEditChanges *const change_info)
{
  return ObjEditChanges_triggers_changed(change_info) ||
         ObjEditChanges_refs_changed(change_info);
}

static inline void ObjEditChanges_change_refs(ObjEditChanges *const change_info,
  unsigned long int const n)
{
  if (change_info) {
    change_info->refs_changed += n;
  }
}

static inline void ObjEditChanges_change_ref(ObjEditChanges *const change_info)
{
  ObjEditChanges_change_refs(change_info, 1);
}

static inline void ObjEditChanges_change_trig(ObjEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->triggers_changed;
  }
}

static inline void ObjEditChanges_add_trig(ObjEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->triggers_added;
  }
}

static inline void ObjEditChanges_delete_trig(ObjEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->triggers_deleted;
  }
}

char *ObjEditChanges_get_message(const ObjEditChanges *change_info);

#endif
