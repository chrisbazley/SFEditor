/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Status bar of main editing window
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef StatusBar_h
#define StatusBar_h

#include "toolbox.h"
#include "MapCoord.h"

typedef struct {
  ObjectId my_object;
  int field_width;
  int window_width;
} StatusBarData;

void StatusBar_init(StatusBarData *status_bar_data, ObjectId id);
int StatusBar_get_height(void);
void StatusBar_show_mode(StatusBarData *status_bar_data, char const *mode);
void StatusBar_show_zoom(StatusBarData *status_bar_data, int zoom_factor);
void StatusBar_show_angle(StatusBarData *statusbar_data, MapAngle angle);
void StatusBar_show_pos(StatusBarData *status_bar_data, bool out, MapPoint map_pos);
void StatusBar_show_hint(StatusBarData *status_bar_data, char const *hint);
void StatusBar_reformat(StatusBarData *status_bar_data, int window_width, int field_width);
void StatusBar_show(StatusBarData *statusbar_data, ObjectId parent_id);
void StatusBar_hide(StatusBarData *statusbar_data);

#endif
