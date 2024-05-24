/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission big ships performance
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef BPerf_h
#define BPerf_h

#include "Ships.h"
#include "SFError.h"

struct Reader;
struct Writer;

typedef struct BigPerformData BigPerformData;
typedef struct BigPerform BigPerform;

SFError big_perform_read(BigPerformData *big,
  struct Reader *reader);

void big_perform_write(BigPerformData const *big,
  struct Writer *writer);

BigPerform *big_perform_get_ship(BigPerformData *big, ShipType type);

int big_perform_get_shields(BigPerform const *big);
bool big_perform_has_remote_shield(BigPerform const *big);
int big_perform_get_ship_prob(BigPerform const *big);
int big_perform_get_ship_count(BigPerform const *big);
int big_perform_get_laser_prob(BigPerform const *big);
int big_perform_get_missile_prob(BigPerform const *big);
int big_perform_get_laser_type(BigPerform const *big);

void big_perform_set_shields(BigPerform *big, int shields);
void big_perform_set_remote_shield(BigPerform *big);
void big_perform_set_ship_prob(BigPerform *big, int ship_prob);
void big_perform_set_ship_count(BigPerform *big, int ship_count);
void big_perform_set_laser_prob(BigPerform *big, int laser_prob);
void big_perform_set_missile_prob(BigPerform *big, int missile_prob);
void big_perform_set_laser_type(BigPerform *big, int laser_type);
#endif
