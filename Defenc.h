/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission ground defences
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Defenc_h
#define Defenc_h

#include "SFError.h"
#include "Ships.h"

struct Reader;
struct Writer;

typedef struct DefencesData DefencesData;

SFError defences_read(DefencesData *defences,
  struct Reader *reader);

void defences_write(DefencesData const *defences,
  struct Writer *writer);

void defences_set_timer(DefencesData *defences, int timer);
int defences_get_timer(DefencesData const *defences);

void defences_set_ship_type(DefencesData *defences, ShipType ship_type);
ShipType defences_get_ship_type(DefencesData const *defences);

void defences_set_fire_prob(DefencesData *defences, int fire_prob);
int defences_get_fire_prob(DefencesData const *defences);

void defences_set_laser_type(DefencesData *defences, int laser_type);
int defences_get_laser_type(DefencesData const *defences);

void defences_set_ships_per_hangar(DefencesData *defences, int ships_per_hangar);
int defences_get_ships_per_hangar(DefencesData const *defences);

void defences_set_ship_prob(DefencesData *defences, int ship_prob);
int defences_get_ship_prob(DefencesData const *defences);

#endif
