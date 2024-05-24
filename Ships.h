/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission ships
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Ships_h
#define Ships_h

struct Reader;
struct Writer;
#include "SFError.h"
#include "Paths.h"

#include "CoarseCoord.h"

enum {
  ShipsMax = 32
};

typedef struct ShipsData ShipsData;
typedef struct Ship Ship;

typedef enum {
  ShipDirection_S,
  ShipDirection_SE,
  ShipDirection_E,
  ShipDirection_NE,
  ShipDirection_N,
  ShipDirection_NW,
  ShipDirection_W,
  ShipDirection_SW,
} ShipDirection;

typedef enum
{
  ShipType_Player = 0,
  ShipType_Fighter1 = 1, /* Cobra / Blackbird / FIGHTER1 / FIGHTER3 */
  ShipType_Fighter2,     /* Avenger / FIGHTER1 / FIGHTER4 / TIE_F */
  ShipType_Fighter3,     /* Avenger / Phantom / Sharke W2 / TIE_F2 */
  ShipType_Fighter4,     /* Predator Mk IV / Sharke W2 / SkyHawk-A / Sabre /
                              NEW_ONE7 */
  ShipType_Big1 = 13,    /* Transporter / SkyTrain / TANKER / STAR_TREK /
                              MEGA_SHIP / WARRIOR */
  ShipType_Big2,         /* TriWing (Mothership) */
  ShipType_Big3,         /* Polar V / StarBase / Excalibur / DEATH_SHIP /
                              KLINGON2 */
  ShipType_Satellite = 21 /* Sentinel (Defence/Comms) */
}
ShipType;

/* Behaviour patterns of big ships */
typedef enum
{
  ShipBehaviour_Moving,
  ShipBehaviour_TurningWheel, /* for space stations */
  ShipBehaviour_SpinningTop,  /* like satellites */
  ShipBehaviour_Stationary
}
ShipBehaviour;

/* Significance of a ship to the player's mission */
typedef enum
{
  ShipMission_NotImportant,
  ShipMission_Target,
  ShipMission_Protect,
  ShipMission_ProtectUntilArrival,
  ShipMission_NoPlayerData,
  ShipMission_PreventLanding
}
ShipMission;

typedef struct {
  unsigned int is_friendly : 1;
  unsigned int is_cloaked : 1;
  unsigned int is_pacifist : 1;
  unsigned int is_alert : 1;
} ShipFlags;

typedef enum {
  ShipPilot_None,
  ShipPilot_CallumBlaze,
  ShipPilot_LukeForester,
  ShipPilot_JaneHollyDean,
  ShipPilot_HazelPhoenix,
  ShipPilot_DaveValiant,
  ShipPilot_StefanLancaster,
  ShipPilot_Crusher,
  ShipPilot_AlliedPilot,
  ShipPilot_LeslieJacobs,
  ShipPilot_JakePhillips,
  ShipPilot_Spyder,
  ShipPilot_JohnTBooker,
  ShipPilot_KellyForester,
  ShipPilot_TraineePilotI,
  ShipPilot_TraineePilotII
} ShipPilot;

void ships_init(ShipsData *ships);
void ships_destroy(ShipsData *ships);

SFError ships_add(ShipsData *ships,
  FinePoint3d coords,
  ShipDirection direction,
  ShipType type,
  ShipBehaviour behaviour,
  ShipMission importance,
  ShipFlags flags,
  ShipPilot pilot,
  Ship **new_ship);

void ship_delete(Ship *ship);

void ship_set_flightpath(Ship *ship, Waypoint *waypoint);
void ship_set_follow(Ship *ship, Ship *leader, FinePoint3d offset);
void ship_set_attack(Ship *ship, Ship *target);

SFError ships_read(ShipsData *ships, struct Reader *reader);
SFError ships_read_pad(ShipsData *ships, struct Reader *reader);

SFError ships_post_read(ShipsData *ships, PathsData *paths);

void ships_pre_write(ShipsData *ships);
void ships_write(ShipsData *ships, struct Writer *writer);
void ships_write_pad(ShipsData *ships, struct Writer *writer);

size_t ships_get_count(ShipsData const *ships);

Ship *ship_from_index(ShipsData *ships, int index);
int ship_get_index(Ship const *ship);

#endif
