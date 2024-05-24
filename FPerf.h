/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission fighters performance
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef FPerf_h
#define FPerf_h

#include "Ships.h"
#include "SFError.h"

struct Reader;
struct Writer;

typedef struct FighterPerformData FighterPerformData;
typedef struct FighterPerform FighterPerform;

SFError fighter_perform_read(FighterPerformData *fighter,
  struct Reader *reader);

void fighter_perform_write(FighterPerformData const *fighter,
  struct Writer *writer);

FighterPerform *fighter_perform_get_ship(FighterPerformData *fighter, ShipType type);

int fighter_perform_get_shields(FighterPerform const *fighter);
int fighter_perform_get_engine(FighterPerform const *fighter);
int fighter_perform_get_laser_prob(FighterPerform const *fighter);
int fighter_perform_get_missile_prob(FighterPerform const *fighter);
int fighter_perform_get_laser_type(FighterPerform const *fighter);
int fighter_perform_get_control(FighterPerform const *fighter);

void fighter_perform_set_shields(FighterPerform *fighter, int shields);
void fighter_perform_set_engine(FighterPerform *fighter, int engine);
void fighter_perform_set_laser_prob(FighterPerform *fighter, int laser_prob);
void fighter_perform_set_missile_prob(FighterPerform *fighter, int missile_prob);
void fighter_perform_set_laser_type(FighterPerform *fighter, int laser_type);
void fighter_perform_set_control(FighterPerform *fighter, int control);

#endif
