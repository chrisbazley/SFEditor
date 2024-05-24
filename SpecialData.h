/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission data for special ship
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef PlayerData_h
#define PlayerData_h

struct PlayerData
{
  bool enabled;
  unsigned char laser_type;
  unsigned char engine;
  unsigned char control;
  unsigned char shields;
  unsigned char ata;
  unsigned char atg;
  unsigned char mines;
  unsigned char bombs;
  unsigned char mega_laser;
  unsigned char multi_ata;
};

#endif
