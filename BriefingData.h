/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission briefing data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef BriefingData_h
#define BriefingData_h

struct BriefingData {
  size_t count;
  struct Text *texts;
  char prefix[12];
  int line_count;
  unsigned char title_colour;
  unsigned char cindex;
  unsigned char brief_colours[2];
};

#endif
