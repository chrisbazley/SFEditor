/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Procedural generation of hills
 *  Copyright (C) 2022 Christopher Bazley
 */

#include "flex.h"
#include "Macros.h"

#include "MapCoord.h"
#include "Map.h"
#include "Hill.h"
#include "HillCol.h"
#include "MapCoord.h"
#include "Obj.h"
#include "ObjGfxMesh.h"
#include "SFError.h"

#define HILL_HEIGHT_BUG 1
#define HILL_COLOUR_BUG 1
#define MIX_COLOURS 1
#define TRIG_BUG 1

enum {
  HillCoordPerQuarterTurn = 2,
  SineToHeightLog2 = 3, // ensure log2
  SineToHeight = 1 << SineToHeightLog2,
  FoothillBaseHeight = 5,
  HillBaseHeight = 10,
  MountainBaseHeight = 20,
  BaseHeightToWaveScaleNumerator = 10, // 15 .. 30
  WaveScaleDenominatorLog2 = 4,
  WaveScaleDenominator = 1 << WaveScaleDenominatorLog2, // 15/16 .. 30/16
  MinHeight = 1,
  MaxHeightNoiseLimit = 4, // ensure log2
  MaxNonSnowTotalHeight = 80,
  MaxNonCliffHeight = 20,
  ExcessHeight = Hill_MaxHeight - (HillNumColours - 1),
  HeightToColourFactor = (Hill_MaxHeight + ExcessHeight - 1) / ExcessHeight, // ensure log2
  ColoursPerGroup = HillNumColours / 3,
  FoothillColourStart = 0,
  CliffColourStart = FoothillColourStart + ColoursPerGroup,
  SnowColourStart = CliffColourStart + ColoursPerGroup,
  HillNeighbourDist = 1,
  MountainNeighbourDist = 2 * HillNeighbourDist,
  /* game generates heights from 2..124 (hills grid 1..62). Nevertheless, our minimum boundary in
     the editor is {0,0} because only the height at the NE corner is recalculated per map location. */
  GenerateHillAreaSize = Hill_Size - 2,
};

typedef struct {
  unsigned int height:6;
  unsigned int mixer:1;
  unsigned int type:3;
  unsigned char colours[Hill_MaxPolygons];
} Hill;

// This macro exists only because right-shifting negative numbers is implementation-defined
#define div_to_neg_inf(dividend, divisor) \
  ((dividend) >= 0 ? \
        (int)((unsigned)(dividend) / (divisor)) : \
        (((dividend) - ((divisor) - 1)) / (divisor)) \
  )

static int get_hill_height(HillsData const *const hills, MapPoint const pos)
{
  size_t const index = hill_coords_to_index(pos);
  Hill const *const data = (Hill *)hills->data;
  int const height = data[index].height;
  DEBUGF("Got hill height %d at %" PRIMapCoord ",%" PRIMapCoord "\n", height, pos.x, pos.y);
  return height;
}

static inline void set_hill_height(HillsData const *const hills, MapPoint const pos,
  int const height)
{
  DEBUGF("Set hill height %d at %" PRIMapCoord ",%" PRIMapCoord "\n", height, pos.x, pos.y);
  assert(hills);
  assert(hills->data);
  assert(height >= 0);
  assert(height <= Hill_MaxHeight);
  ((Hill *)hills->data)[hill_coords_to_index(pos)].height = height;
}

static inline bool change_mixer_for_type(HillType const type)
{
#if MIX_COLOURS
  return (type != HillType_None &&
          type != HillType_ABCA_ACDA &&
          type != HillType_ABDA_BCDB);
#else
  return false;
#endif
}

static inline int get_hill_mixer(HillsData const *const hills, MapPoint const pos)
{
  assert(hills);
  assert(hills->data);
  assert(hills_coords_in_range(pos));
  Hill *const hill = &((Hill *)hills->data)[hill_coords_to_index(pos)];
  int mixer = hill->mixer;
  assert(mixer == 0 || mixer == 1);
  DEBUGF("Get mixer %d at %"PRIMapCoord",%" PRIMapCoord"\n", mixer, pos.x, pos.y);
  return mixer;
}

static inline int swap_hill_mixer(HillsData *const hills, MapPoint const pos)
{
  assert(hills);
  assert(hills->data);
  assert(hills_coords_in_range(pos));
  Hill *const hill = &((Hill *)hills->data)[hill_coords_to_index(pos)];
  assert(hill->mixer >= 0);
  assert(hill->mixer <= 1);
  hill->mixer = 1 - hill->mixer;
  return hill->mixer;
}

static inline HillType get_hill_type(HillsData *const hills, MapPoint const pos)
{
  assert(hills);
  assert(hills->data);
  assert(hills_coords_in_range(pos));

  size_t const index = hill_coords_to_index(pos);
  Hill *const hill = &((Hill *)hills->data)[index];
  HillType type = hill->type;
  assert(type >= HillType_None);
  assert(type < HillType_Count);
  return type;
}

static inline void set_hill_metadata(HillsData *const hills, MapPoint const pos,
  HillType const type, int mixer, unsigned char (*const colours)[Hill_MaxPolygons])
{
  assert(hills);
  assert(hills->data);
  assert(hills_coords_in_range(pos));
  assert(type >= HillType_None);
  assert(type < HillType_Count);
  assert(mixer == 0 || mixer == 1);
  assert(colours);

  size_t const index = hill_coords_to_index(pos);
  Hill *const hill = &((Hill *)hills->data)[index];

  for (size_t i = 0; i < ARRAY_SIZE(*colours); ++i) {
    assert((*colours)[i] >= 0);
    assert((*colours)[i] < HillNumColours);
    hill->colours[i] = (*colours)[i];
  }
  hill->type = type;

  DEBUGF("Set hill type %d and mixer %d at %" PRIMapCoord ",%" PRIMapCoord "\n",
         type, mixer, pos.x, pos.y);
  hill->mixer = mixer;
}

static void clear_hill_metadata(HillsData *const hills)
{
  assert(hills);
  assert(hills->data);

  Hill *const data = hills->data;
  for (int i = 0; i < Hill_Size * Hill_Size; ++i) {
    data[i] = (Hill){.type = HillType_None, .height = 0, .mixer = 0};
  }
}

static bool hill_at_coord(HillsData const *const hills, MapPoint pos)
{
  assert(hills);
  if (!hills->read_hill_cb) {
    return false;
  }

  pos = MapPoint_mul_log2(pos, Hill_ObjPerHillLog2);

  // The game doesn't check for memory accesses out of bounds
  if (pos.x < 0) {
    pos.y--;
    pos.x = map_wrap_coord(pos.x);
  } else if (pos.x >= Map_Size) {
    pos.y++;
    pos.x = map_wrap_coord(pos.x);
  }

  assert(map_coords_in_range(pos));
  return hills->read_hill_cb(hills->edit_win, pos);
}

static void redraw_hill(HillsData const *const hills, MapPoint const pos,
  HillType const old_type, unsigned char (*const old_heights)[HillCorner_Count],
  HillType const new_type, unsigned char (*const new_heights)[HillCorner_Count])
{
  assert(hills);
  if (!hills->redraw_cb) {
    return;
  }
  if (old_type == HillType_None && new_type == HillType_None) {
    return;
  }
  hills->redraw_cb(hills->edit_win, pos, old_type, old_heights, new_type, new_heights);
}

#if TRIG_BUG
static inline int clamp(int f)
{
  if (f <= -SINE_TABLE_SCALE) {
    return 1 - SINE_TABLE_SCALE;
  }
  if (f >= SINE_TABLE_SCALE) {
    return SINE_TABLE_SCALE - 1;
  }
  return f;
}
#endif

static int calc_height_for_pos(HillsData const *const hills, MapPoint const p)
{
  if (!hill_at_coord(hills, p) ||
      !hill_at_coord(hills, (MapPoint){p.x - HillNeighbourDist, p.y}) ||
      !hill_at_coord(hills, (MapPoint){p.x, p.y - HillNeighbourDist})) {
    DEBUGF("No hill at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);
    return 0;
  }

  int min_height = MountainBaseHeight;

  if (!hill_at_coord(hills, (MapPoint){p.x + HillNeighbourDist, p.y}) ||
      !hill_at_coord(hills, (MapPoint){p.x, p.y + HillNeighbourDist})) {
    min_height = FoothillBaseHeight;
  } else if (!hill_at_coord(hills, (MapPoint){p.x + MountainNeighbourDist, p.y}) ||
             !hill_at_coord(hills, (MapPoint){p.x, p.y + MountainNeighbourDist})) {
    min_height = HillBaseHeight;
  }

  TrigTable const *const trig_table = ObjGfxMeshes_get_trig_table();
#if HILL_HEIGHT_BUG
  /* These coefficients were clearly meant to be cosine and sine in the original
     game code but they aren't (wrong magic address relocation number). */
  int f = TrigTable_look_up_sine(trig_table,
                        p.x * (OBJGFXMESH_ANGLE_QUART / HillCoordPerQuarterTurn));

  int g = TrigTable_look_up_sine(trig_table,
                        (OBJGFXMESH_ANGLE_QUART * 3) +
                        p.y * (OBJGFXMESH_ANGLE_QUART / HillCoordPerQuarterTurn));
#else
  int f = TrigTable_look_up_cosine(trig_table,
                        p.x * (OBJGFXMESH_ANGLE_QUART / HillCoordPerQuarterTurn));

  int g = TrigTable_look_up_sine(trig_table,
                        p.y * (OBJGFXMESH_ANGLE_QUART / HillCoordPerQuarterTurn));
#endif
#if TRIG_BUG
  f = clamp(f);
  g = clamp(g);
#endif

  int const combined_wave = f + g + SINE_TABLE_SCALE; // range -1.0 .. 3.0

  int const wave_height = div_to_neg_inf(combined_wave, SINE_TABLE_SCALE / SineToHeight);
  // range -8 .. 24

  int const wave_scale_numerator = min_height + BaseHeightToWaveScaleNumerator; // range 15 .. 30
  int const upscaled_wave_height = wave_height * wave_scale_numerator;

  int const scaled_wave_height = div_to_neg_inf(upscaled_wave_height, WaveScaleDenominator);
  // range -15..45 for mountains or -8..22 for foothills

  int height = min_height + scaled_wave_height;
  // range 5..65 for mountains or -3..27 for foothills

  if (height < MinHeight) {
    height = MinHeight;
  } else if (height > Hill_MaxHeight) {
    DEBUGF("Cap height %d at %" PRIMapCoord ",%" PRIMapCoord "\n", height, p.x, p.y);
    height = Hill_MaxHeight - (int)((unsigned)upscaled_wave_height % MaxHeightNoiseLimit);
  }
  DEBUGF("Calculated height %d at %" PRIMapCoord ",%" PRIMapCoord "\n", height, p.x, p.y);
  return height;
}

typedef struct {
 bool east:1;
 bool north:1;
} CornerFlags;

static inline int get_hill_colour(CornerFlags const flags, int const corner,
  int const left, int const right, int const mixer)
{
  assert(corner >= 0);
  assert(corner <= Hill_MaxHeight);
  assert(left >= 0);
  assert(left <= Hill_MaxHeight);
  assert(right >= 0);
  assert(right <= Hill_MaxHeight);
  assert(mixer == 0 || mixer == 1);

  int const total_height = corner + left + right; // range 0..141
  int left_diff = corner - left; // range -47..47
  if (flags.east) {
    DEBUGF("Inverting left height diff %d\n", left_diff);
    left_diff = -left_diff;
  }
  int right_diff = corner - right; // range -47..47
  if (flags.north) {
    DEBUGF("Inverting right height diff %d\n", right_diff);
    right_diff = -right_diff;
  }
  int const average_diff = div_to_neg_inf(left_diff + right_diff, 2); // range -47..47

  int const scaled_average = average_diff - div_to_neg_inf(average_diff, HeightToColourFactor);
  // range -59..36
  // I guess the original intention was to use a value between 0 and 36 as the final colour
  DEBUGF("Scaled average height diff %d from %d\n", scaled_average, average_diff);

  int colour = scaled_average + mixer; // range -59..37

  colour += 3; // range -56..40

  if (colour < 0) {
    DEBUGF("Fixing -ve colour %d\n", colour);
    colour = -colour; // range 1..56
    // Scaling the colour index again seems like a bug but that's what the game does
    colour -= (int)((unsigned)colour / HeightToColourFactor); // range 0..42
  } else {
    // range 0..40
  }

  // Select the same colour for values 11..42
  if (colour > ColoursPerGroup - 1) {
    DEBUGF("Clamping final colour %d\n", colour);
    colour = ColoursPerGroup - 1; // range 0..11
  }

  if (total_height > MaxNonSnowTotalHeight) {
    colour += SnowColourStart; // range 24..35
  } else if (abs(left_diff) > MaxNonCliffHeight ||
             abs(right_diff) > MaxNonCliffHeight) {
    colour += CliffColourStart; // range 12..23
  } else {
    colour += FoothillColourStart;
  }

  DEBUGF("Get hill colour %d from heights %d,%d,%d and mixer %d\n", colour, corner, left, right, mixer);
  assert(colour >= 0);
  assert(colour < HillNumColours);
  return colour;
}

static HillType get_hill_metadata_from_heights(unsigned char (*const heights)[HillCorner_Count], int mixer, unsigned char (*const colours)[Hill_MaxPolygons])
{
  HillType type = HillType_None;
  assert(heights);
  assert(colours);
  assert(mixer == 0 || mixer == 1);

  int const a = (*heights)[HillCorner_A],
            b = (*heights)[HillCorner_B],
            c = (*heights)[HillCorner_C],
            d = (*heights)[HillCorner_D];

  assert(a >= 0);
  assert(a <= Hill_MaxHeight);
  assert(b >= 0);
  assert(b <= Hill_MaxHeight);
  assert(c >= 0);
  assert(c <= Hill_MaxHeight);
  assert(d >= 0);
  assert(d <= Hill_MaxHeight);

  int i = 0;
  if (!a && !b && !c && !d) {
  } else if (a && !b && !c && !d) {
    // B
    // A D
    type = HillType_ABDA;
    mixer = 1 - mixer;
    (*colours)[i++] = get_hill_colour((CornerFlags){.east = false, .north = false}, a, d, b, mixer);
  } else if (!a && b && !c && !d) {
    // B C
    // A
    type = HillType_ABCA;
    mixer = 1 - mixer;
    (*colours)[i++] = get_hill_colour((CornerFlags){.east = false, .north = true}, b, c, a, mixer);
  } else if (!a && !b && c && !d) {
    // B C
    //   D
    type = HillType_BCDB;
    mixer = 1 - mixer;
    (*colours)[i++] = get_hill_colour((CornerFlags){.east = true, .north = true}, c, b, d, mixer);
  } else if (!a && !b && !c && d) {
    //   C
    // A D
    type = HillType_CDAC;
    mixer = 1 - mixer;
    (*colours)[i++] = get_hill_colour((CornerFlags){.east = true, .north = false}, d, a, c, mixer);
  } else {
    // At least two corners are higher than zero
    int const ac_slope = abs(a - c);
    int const bd_slope = abs(b - d);
    if (ac_slope > bd_slope) {
      DEBUGF("Diagonal A..C %d is steeper than diagonal B..D %d\n", ac_slope, bd_slope);
      // B C
      // A
      //   C
      // A D
      type = HillType_ABCA_ACDA; // type 1
    } else {
      DEBUGF("Diagonal B..D %d is steeper than diagonal A..C %d\n", bd_slope, ac_slope);
      // B C
      //   D
      // B
      // A D
      type = HillType_ABDA_BCDB; // type 2
    }

#if HILL_COLOUR_BUG
    // Bug: use type 2 colours if south-west (A) corner height <= 0 instead of ac_slope <= bd_slope
    if (a <= 0) {
#else
    if (ac_slope <= bd_slope) {
#endif
      // type 2 colours
      mixer = 1 - mixer;
      (*colours)[i++] = get_hill_colour((CornerFlags){.east = false, .north = false}, a, d, b, mixer);
#if MIX_COLOURS
      mixer = 1 - mixer;
#endif
      (*colours)[i++] = get_hill_colour((CornerFlags){.east = true, .north = true}, c, b, d, mixer);
    } else {
      // type 1 colours
      mixer = 1 - mixer;
      (*colours)[i++] = get_hill_colour((CornerFlags){.east = true, .north = false}, d, a, c, mixer);
#if MIX_COLOURS
      mixer = 1 - mixer;
#endif
      (*colours)[i++] = get_hill_colour((CornerFlags){.east = false, .north = true}, b, c, a, mixer);
    }
  }
  assert(i <= Hill_MaxPolygons);
  return type;
}

#ifndef NDEBUG
static void check_mixers(HillsData const *const hills)
{
  assert(hills);
  assert(hills->data);

  for (MapPoint pos = {.y = 1}; pos.y <= GenerateHillAreaSize; ++pos.y) {
    for (pos.x = 1; pos.x <= GenerateHillAreaSize; ++pos.x) {
      assert(calc_height_for_pos(hills, pos) == get_hill_height(hills, pos));
    }
  }

  int expected_mixer = 0;
  for (MapPoint pos = {.y = 0}; pos.y <= GenerateHillAreaSize; ++pos.y) {
    for (pos.x = 0; pos.x <= GenerateHillAreaSize; ++pos.x) {
      int const mixer = get_hill_mixer(hills, pos);
      DEBUGF("Mixer is %d, expected %d at %" PRIMapCoord ",%" PRIMapCoord "\n",
             mixer, expected_mixer, pos.x, pos.y);
      assert(mixer == expected_mixer);

      unsigned char colours[Hill_MaxPolygons] = {0};
      unsigned char heights[HillCorner_Count] = {0};
      HillType const type = hills_read(hills, pos, &colours, &heights);

      unsigned char expected_colours[Hill_MaxPolygons] = {0};
      HillType const expected_type = get_hill_metadata_from_heights(&heights, expected_mixer, &expected_colours);
      assert(type == expected_type);
      for (size_t i = 0; i < ARRAY_SIZE(colours); ++i) {
        assert(colours[i] == expected_colours[i]);
      }

      if (change_mixer_for_type(expected_type)) {
        expected_mixer = 1 - expected_mixer;
      }
    }
  }
}
#else
static inline void check_mixers(HillsData const *const hills)
{
  NOT_USED(hills);
}
#endif

static void generate_heights(HillsData *const hills, MapArea const *const update_area,
  bool const force)
{
  assert(hills);
  assert(MapArea_is_valid(update_area));
  DEBUGF("Generate hills %p at %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)hills, update_area->min.x, update_area->min.y, update_area->max.x, update_area->max.y);

  // Can't handle wrap-around in this function because of colour mixer bit
  assert(hills_coords_in_range(update_area->min));
  assert(hills_coords_in_range(update_area->max));
  MapPoint const max = {.x = LOWEST(update_area->max.x, GenerateHillAreaSize),
                        .y = LOWEST(update_area->max.y, GenerateHillAreaSize)};

  /* Polygon corners:
     B C
     A D
   */
  unsigned char heights[HillCorner_Count] = {0};
  int mixer = 0;
  bool colour_change = false;
  for (MapPoint p = {.y = update_area->min.y}; p.y <= max.y || (colour_change && p.y <= GenerateHillAreaSize); ++p.y) {
    p.x = colour_change ? 0 : update_area->min.x;

    heights[HillCorner_D] = get_hill_height(hills, p); // next A
    heights[HillCorner_C] = get_hill_height(hills, (MapPoint){p.x, p.y + 1}); // next B

    /* Mixer indirectly reflects the number of polygons earlier in the rasterised map.
       Get it at the start of each span to be updated to avoid counting from x = 0. */
    if (!colour_change) {
      mixer = get_hill_mixer(hills, p);
    }

    for (; p.x <= max.x || (colour_change && p.x <= GenerateHillAreaSize); ++p.x) {
      heights[HillCorner_B] = heights[HillCorner_C];
      heights[HillCorner_A] = heights[HillCorner_D];
      MapPoint const c_pos = (MapPoint){p.x + 1, p.y + 1};
      heights[HillCorner_C] = get_hill_height(hills, c_pos);
      heights[HillCorner_D] = get_hill_height(hills, (MapPoint){p.x + 1, p.y});

      int c = heights[HillCorner_C];
      if (p.x < max.x && p.y < max.y) {
        c = calc_height_for_pos(hills, c_pos);
      }

      HillType const old_type = get_hill_type(hills, p);
      HillType type = old_type;
      unsigned char colours[Hill_MaxPolygons] = {0};

      if (1/*c != heights[HillCorner_C]*/ || force) {

        unsigned char old_heights[HillCorner_Count];
        for (size_t i = 0; i < ARRAY_SIZE(heights); ++i) {
          old_heights[i] = heights[i];
        }

        set_hill_height(hills, c_pos, c);
        heights[HillCorner_C] = c;
        type = get_hill_metadata_from_heights(&heights, mixer, &colours);

        if (!force) {
          redraw_hill(hills, p, old_type, &old_heights, type, &heights);
        }

        // Set the initial mixer value to be used to colour any polygons which might in
        // future replace the polygons we are generating now.
        set_hill_metadata(hills, p, type, mixer, &colours);
      } else if (!force) {
        assert(old_type == get_hill_metadata_from_heights(&heights, mixer, &colours));
      }

      if (change_mixer_for_type(type)) {
        mixer = 1 - mixer;
      }

      DEBUGF("Hill at %" PRIMapCoord ",%" PRIMapCoord " has heights A=%d, B=%d, C=%d, D=%d\n",
             p.x, p.y, heights[HillCorner_A], heights[HillCorner_B], heights[HillCorner_C], heights[HillCorner_D]);

      if (change_mixer_for_type(type) != change_mixer_for_type(old_type)) {
        colour_change = !colour_change;
        DEBUGF("Colour change %s\n", colour_change ? "activated" : "deactivated");
      }
    }
  }
}

SFError hills_init(HillsData *const hills, HillReadFn *const read_hill_cb, HillRedrawFn *const redraw_cb,
  struct EditWin *const edit_win)
{
  assert(hills);
  *hills = (HillsData){.read_hill_cb = read_hill_cb, .redraw_cb = redraw_cb, .edit_win = edit_win};

  if (!flex_alloc(&hills->data, Hill_Size * Hill_Size * sizeof(Hill))) {
    return SFERROR(NoMem);
  }

  clear_hill_metadata(hills);
  return SFERROR(OK);
}

void hills_destroy(HillsData *const hills)
{
  assert(hills);
  flex_free(&hills->data);
}

void hills_make(HillsData *const hills)
{
  MapArea const update_area = {
    .min = {0, 0},
    .max = {Hill_Size - 1, Hill_Size - 1},
  };
  generate_heights(hills, &update_area, true);
  check_mixers(hills);
}

static bool hills_update_split_cb(MapArea const *const update_area, void *const cb_arg)
{
  generate_heights(cb_arg, update_area, false);

  return false;
}

void hills_update(HillsData *const hills, MapArea const *const changed_area)
{
  DEBUGF("Update hills %p at %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)hills, changed_area->min.x, changed_area->min.y, changed_area->max.x, changed_area->max.y);

  MapArea const update_area = {
    /* +1 because only the height at the NE corner is recalculated per map location
       and we need to recalculate the height of the most distant SW-edges of a mountain */
    .min = MapPoint_sub(changed_area->min, (MapPoint){MountainNeighbourDist + 1, MountainNeighbourDist + 1}),
    .max = MapPoint_add(changed_area->max, (MapPoint){HillNeighbourDist, HillNeighbourDist}),
  };

  hills_split_area(&update_area, hills_update_split_cb, hills);
  check_mixers(hills);
}

HillType hills_read(HillsData const *const hills, MapPoint const pos,
  unsigned char (*const colours)[Hill_MaxPolygons],
  unsigned char (*const heights)[HillCorner_Count])
{
  assert(hills);
  assert(hills->data);
  Hill const *const data = (Hill *)hills->data;
  size_t const index = hill_coords_to_index(pos);
  Hill const *const hill = &data[index];
  HillType const type = hill->type;
  if (type != HillType_None) {
    if (colours) {
      for (size_t i = 0; i < ARRAY_SIZE(*colours); ++i) {
        (*colours)[i] = hill->colours[i];
      }
    }
    if (heights) {
      (*heights)[HillCorner_A] = hill->height;
      bool const x_safe = hills_wrap_coord(pos.x) < Hill_Size - 1;
      bool const y_safe = hills_wrap_coord(pos.y) < Hill_Size - 1;
      (*heights)[HillCorner_B] = x_safe ? data[index + Hill_Size].height : 0;
      (*heights)[HillCorner_C] = (x_safe && y_safe) ? data[index + Hill_Size + 1].height : 0;
      (*heights)[HillCorner_D] = x_safe ? data[index + 1].height : 0;
    }
  }
  return type;
}
