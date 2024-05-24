/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Player's ship
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Player_h
#define Player_h

struct Reader;
struct Writer;

#include "Ships.h"
#include "SFError.h"

typedef struct PlayerData PlayerData;

SFError player_read(PlayerData *player, struct Reader *reader);

SFError player_read_docked(PlayerData *player,
  struct Reader *reader);

SFError player_post_read(PlayerData *player,
  ShipsData *ships);

void player_write(PlayerData const *player, struct Writer *writer);

void player_write_docked(PlayerData const *player,
  struct Writer *writer);

ShipType player_get_ship_type(PlayerData const *player);

void player_set_ship_type(PlayerData *player,
  ShipType ship_type);

bool player_get_equip_enabled(PlayerData const *player);
void player_set_equip_enabled(PlayerData *player, bool enabled);

int player_get_laser_type(PlayerData const *player);
void player_set_laser_type(PlayerData *player, int laser_type);

int player_get_engine(PlayerData const *player);
void player_set_engine(PlayerData *player, int engine);

int player_get_control(PlayerData const *player);
void player_set_control(PlayerData *player, int control);

int player_get_shields(PlayerData const *player);
void player_set_shields(PlayerData *player, int shields);

int player_get_ata(PlayerData const *player);
void player_set_ata(PlayerData *player, int ata);

int player_get_atg(PlayerData const *player);
void player_set_atg(PlayerData *player, int atg);

int player_get_mines(PlayerData const *player);
void player_set_mines(PlayerData *player, int mines);

int player_get_bombs(PlayerData const *player);
void player_set_bombs(PlayerData *player, int bombs);

int player_get_mega_laser(PlayerData const *player);
void player_set_mega_laser(PlayerData *player, int mega_laser);

int player_get_multi_ata(PlayerData const *player);
void player_set_multi_ata(PlayerData *player, int multi_ata);

#endif
