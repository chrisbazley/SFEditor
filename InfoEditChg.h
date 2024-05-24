/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information change
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef InfoEditChg_h
#define InfoEditChg_h

#include <stdbool.h>

typedef struct InfoEditChanges
{
  unsigned long int infos_added;
  unsigned long int infos_changed;
  unsigned long int infos_deleted;
}
InfoEditChanges;

void InfoEditChanges_init(InfoEditChanges *change_info);

static inline bool InfoEditChanges_is_changed(const InfoEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->infos_added ||
         change_info->infos_changed ||
         change_info->infos_deleted;
}

static inline void InfoEditChanges_change(InfoEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->infos_changed;
  }
}

static inline void InfoEditChanges_add(InfoEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->infos_added;
  }
}

static inline void InfoEditChanges_delete(InfoEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->infos_deleted;
  }
}

char *InfoEditChanges_get_message(const InfoEditChanges *change_info);

#endif
