/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission data for player's ship
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef PlayerData_h
#define PlayerData_h

#include "Ships.h"
#include "CoarseCoord.h"

typedef enum {
  PlayerDataState_Write,
  PlayerDataState_PostRead,
} PlayerDataState;

struct PlayerData
{
  PlayerDataState state;
  CoarsePoint3d coords;
  ShipDirection direction;
  union {
    Ship *ship; /* valid for PlayerDataState_Write */
    int num; /* valid for PlayerDataState_PostRead */
  } docked_in;
  ShipType ship_type;
  bool equip_enabled;
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
