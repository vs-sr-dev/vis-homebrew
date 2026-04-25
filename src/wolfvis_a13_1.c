/*
 * WolfVIS A.16a — static enemies (guard billboards over A.15 baseline).
 *
 * First milestone where the world contains living-targets-in-waiting:
 * Wolf3D guards rendered as static billboards at their map_plane1 tile
 * positions in E1L1. No AI, no HP, no movement, no animation, no rotation
 * (yet). Dry-run scope: validate the asset/scan/render pipeline for
 * enemies, isolating the rotation logic for a follow-up iteration.
 *
 * What changes vs A.15 (additive):
 *   - VSWAP loader extended to also load 8 chunks at sprite_start_idx +
 *     50..57. These are SPR_GRD_S_1..SPR_GRD_S_8 in the Wolf3D shareware
 *     sprite enum (WOLFSRC/WL_DEF.H lines 159-208: 0=DEMO, 1=DEATHCAM,
 *     2..49=SPR_STAT_0..47, 50..57=SPR_GRD_S_1..8). They live at slots
 *     18..25 in our sprites[] array (right after the 18 we already load).
 *   - NUM_SPRITES bumped 18 -> 26. Memory: 26 * 4096 = 104 KB, kept under
 *     __huge so Watcom places across multiple segments (the per-row 4 KB
 *     deref pattern from A.14 keeps the inner sample loop near-pointer
 *     cost).
 *   - MAX_OBJECTS bumped 64 -> 128. E1L1 has more enemy tiles than
 *     decoration tiles; we want headroom for both. Painter's sort is
 *     O(N^2) on the side-array but at 128 entries that's 16K cmp worst
 *     case, trivially cheap vs the cast workload.
 *   - ScanObjects extended with a second branch that recognizes the 24
 *     guard tile values across all three difficulty tiers (108..115 easy,
 *     144..151 medium, 180..187 hard — see WOLFSRC/WL_GAME.C SpawnStand /
 *     SpawnPatrol cases). For dry-run all enemies use sprite_idx = slot
 *     18 (SPR_GRD_S_1, front view) regardless of facing. A.16a iter 2
 *     adds the 8-direction CalcRotate logic from WL_DRAW.C:1024.
 *
 * Defer to A.16a iter 2 / A.16b:
 *   - 8-direction rotation: compute view angle (player -> enemy) minus
 *     enemy facing, pick sprite slot 18..25 by sector. Needs an atan2
 *     LUT (no <math.h> per the WIN87EM trap) and Object.facing_dir field.
 *   - Officer / SS / dog enemies (different sprite enum ranges, different
 *     spawn tile sets — see WL_GAME.C cases 116..213).
 *   - SpawnDeadGuard for tile 124 (corpse).
 *   - Walking animation (W1..W4 frames, A.16b with the AI ticker).
 *   - Damage / HP / death.
 *
 * Vanilla source map_plane1 -> enemy decoder (WL_GAME.C:330..477):
 *   tile  108..111 / 144..147 / 180..183 -> SpawnStand(en_guard, dir=tile-base)
 *   tile  112..115 / 148..151 / 184..187 -> SpawnPatrol(en_guard, dir=tile-base)
 *   (medium/hard tiers are gated on gamestate.difficulty in vanilla; we
 *   ignore the gate for PoC and spawn every tier so all guard tiles in
 *   E1L1 are visible regardless of the editor's difficulty intent.)
 * Direction encoding: tile-base = 0..3 = E/N/W/S. SpawnStand multiplies
 * by 2 -> dirtype value 0/2/4/6 in the WL_DEF.H dirtype enum
 * (E=0, NE=1, N=2, NW=3, W=4, SW=5, S=6, SE=7). We capture this in the
 * Object.enemy_dir field for use by the rotation code in iter 2; for
 * the dry-run only the position is rendered.
 *
 * --- Original A.15 header (kept for traceability) ---
 *
 * First milestone where the screen has chrome around the play area
 * instead of a black margin. Lifts the framing from "tech demo" to
 * "game" without changing the cast workload — HUD is a pixel-constant
 * blit baked into static_bg, so the per-frame DrawViewport pass is
 * untouched. Layout invariant kept: viewport 128x128, debug bar 30 px,
 * minimap 64x64, HUD now occupies y=163..199 (the previously-black
 * lower strip).
 *
 * Adds vs A.14.1:
 *   - digit_font[10][24] static const: 4x6 byte-per-pixel font for
 *     digits 0..9. ~240 B in code, no runtime heap.
 *   - DrawDigit / DrawNumber helpers (right-align with leading zeros).
 *   - face_placeholder: 24x24 stylized soldier-helmet drawn from
 *     primitives (FillRect) — no bitmap data needed. A.15.1 polish
 *     path: implement VGAGRAPH loader for the real BJ face frames
 *     (FACE1APIC etc., chunked Huffman in VGAGRAPH.WL1, separate
 *     from VSWAP).
 *   - DrawHUD: 5-panel chrome strip with LEVEL / SCORE / LIVES /
 *     FACE / HEALTH / AMMO / KEYS values dummied to constants for
 *     PoC. A.16+ enemies will introduce real damage/score/ammo
 *     dynamics; A.15 just establishes the chrome.
 *   - SetupStaticBg extended to bake HUD into static_bg (panel
 *     borders + dummy values + face placeholder all are constant).
 *     Net effect: A.15 adds zero per-frame cost vs A.14.1.
 *
 * Inherited from A.14.1 unchanged: door state machine, sliding slab
 * render, PRIMARY toggle, mid-plane interp cast logic, DOOR_TEX_IDX
 * sentinel, minimap door coloring.
 *
 * --- Original A.14.1 header (kept for traceability) ---
 *
 * Closes the cosmetic regression A.14 shipped with: the player visibly
 * walked through wall slabs at door tiles 90..101 because IsWall blocked
 * the cast (so they rendered as walls) while IsBlockingForMove let them
 * pass. A.14.1 makes doors first-class:
 *   - dedicated door texture (VSWAP chunk sprite_start_idx-8 = DOORWALL
 *     in vanilla Wolf3D) loaded into a 4 KB __far buffer.
 *   - per-tile state byte g_door_amt (0=closed, 64=fully open),
 *     advanced by WM_TIMER during opening/closing animation.
 *   - CastRay detects entry into a door tile, advances to the tile
 *     mid-plane (X=center for vertical doors, Y=center for horizontal),
 *     and treats the slab as a partial wall whose perp-axis extent is
 *     (1 - amt/64). A ray crossing the open portion passes through and
 *     keeps casting; a ray hitting the slab returns DOOR_TEX as tex_idx
 *     sentinel so DrawWallStripCol samples door_tex instead of walls[].
 *   - IsBlockingForMove returns true for doors with amt < 56 (mostly
 *     closed) so the player can walk through fully-open doors but
 *     bounces off closed/animating ones.
 *   - PRIMARY tap (VK_HC1_PRIMARY) scans the tile one step in front of
 *     the player along its heading; if it's a door tile, toggles its
 *     animation direction.
 *
 * Adds vs A.14:
 *   - door_tex[4096] __far (chunk sprite_start_idx-8).
 *   - g_door_amt[MAP_TILES] __far + g_door_dir[MAP_TILES] __far.
 *   - IsDoor helper (returns 1=vertical, 2=horizontal, 0=not door).
 *   - CastRay door branch + DrawWallStripCol DOOR_TEX path.
 *   - AdvanceDoors() called from WM_TIMER.
 *   - ToggleDoorInFront() called from VK_HC1_PRIMARY.
 *   - PRIMARY's old OPL click removed (door is the better use of the
 *     button; SECONDARY's click kept for now as audio sanity check).
 *
 * Layout 320x200:
 *   y=0..29       : debug bar (heartbeat + status, repainted on WM_TIMER)
 *   y=30..34      : black gutter
 *   y=35..162 x=0..127 : 3D viewport 128x128 (128 ray casts, ceil/wall/floor)
 *   y=35..98  x=140..203 : minimap 64x64 with player position + heading
 *   else          : black
 *
 * Controls (HC1 hand controller):
 *   d-pad UP/DOWN    : move forward / back along player heading
 *   d-pad LEFT/RIGHT : rotate counterclockwise / clockwise
 *   PRIMARY (A)      : OPL ch0 high click (legacy)
 *   SECONDARY (B)    : OPL ch0 mid click + key-off (legacy)
 *   F1               : OPL init + start music (idempotent)
 *   F3               : stop music
 *
 * Coordinate system:
 *   pos_x, pos_y in Q8.8 tile units. Map is 64x64 tiles, (0,0) is NW.
 *   Y+ is south (matches Wolf3D map storage). Angle 0 = east (+X), 256 =
 *   south (+Y), 512 = west, 768 = north. ANGLES=1024. sin_q15[a] = round
 *   (sin(2*pi*a/1024) * 32767), cos via shift by ANGLE_QUAD.
 *
 * Cast algorithm (PoC step-by-fraction):
 *   For each column, advance ray position by 1/16 tile per step. When the
 *   integer tile (tx,ty) changes and that tile is a wall, take it as the
 *   hit. Side X (vertical wall face) vs side Y (horizontal wall face) is
 *   detected by which axis crossed in this step. Returns Euclidean dist
 *   and tex_x (the 0..63 horizontal offset into the wall texture column).
 *   Fish-eye correction applied via per-column cos table.
 *
 * Cursor suppression (S8 fix for VIS native arrow showing through frames):
 *   1. WNDCLASS hCursor = NULL (no class default cursor).
 *   2. WM_SETCURSOR returns SetCursor(NULL); TRUE (suppress when entering).
 *   3. ShowCursor(FALSE) post-CreateWindow (decrement global counter).
 *
 * --- Inherited from A.12 (kept) ---
 *   Per-column post-walk pattern (the wall-strip is its texture-column twin).
 *   PIT-direct hi-res clock + skip-gap (A.10.1 polish).
 *   __far placement of large BSS to keep DGROUP under 64 KB.
 *   Channel-0 click separation from IMF ch1..8.
 */
#include <conio.h>
#include <windows.h>
#include "gamepal.h"
#include "wolfvis_a13_1_sintab.h"
#include "wolfvis_a13_1_atantab.h"

extern void FAR PASCAL hcGetCursorPos(LPPOINT lpp);

#define SCR_W        320
#define SCR_H        200

#define MAP_W        64
#define MAP_H        64
#define MAP_TILES    4096
#define NUMMAPS      100
#define CARMACK_SRC_MAX  4096
#define CARMACK_DST_MAX  8192

#define VK_HC1_DOWN      0x70
#define VK_HC1_F1        0x71
#define VK_HC1_PRIMARY   0x72
#define VK_HC1_F3        0x73
#define VK_HC1_F4        0x74
#define VK_HC1_SECONDARY 0x75
#define VK_HC1_TOOLBAR   0x76
#define VK_HC1_LEFT      0x77
#define VK_HC1_UP        0x78
#define VK_HC1_RIGHT     0x79

/* Scene layout */
#define DEBUG_BAR_H      30
#define VIEW_X0          0
#define VIEW_Y0          35
#define VIEW_W           128
#define VIEW_H           128
#define VIEW_CY          (VIEW_Y0 + VIEW_H/2)
#define MINIMAP_X0       140
#define MINIMAP_Y0       35
#define MINIMAP_TILE_PX  1
#define CEIL_COLOR       29     /* mid grey */
#define FLOOR_COLOR      24     /* darker grey */

/* HUD strip — occupies the previously-black y=163..199 lower 37 px.
 * Color choices match Wolf3D's status bar conventions: dark blue (1)
 * background, lighter blue (9) borders, white digits. Brown (60) /
 * peach (56) for the placeholder face. Indices verified against
 * gamepal6 RGB6 triplets. */
#define HUD_Y0           163
#define HUD_H            37
#define HUD_BG           1      /* dark blue panel fill (Wolf3D-style) */
#define HUD_BORDER       9      /* bright blue separator */
#define HUD_FG           15     /* bright white digit foreground */
#define HUD_FG_LOW       40     /* low-value warning (red) */
#define HUD_DIGIT_W      4
#define HUD_DIGIT_H      6
#define HUD_DIGIT_PITCH  5      /* 4 px digit + 1 px gap */
#define HUD_DIGIT_Y      178    /* baseline-ish y for the value row */

/* Panel x boundaries — laid out to center FACE on screen center (160).
 * LEVEL/SCORE/LIVES on the left occupy 144 px, FACE 32 px (centered
 * on 160), HEALTH/AMMO/KEYS on the right occupy 144 px. Symmetric. */
#define HUD_PX_LVL_END    36
#define HUD_PX_SCORE_END  108
#define HUD_PX_LIVES_END  144
#define HUD_PX_FACE_END   176
#define HUD_PX_HEALTH_END 224
#define HUD_PX_AMMO_END   272

/* VSWAP */
#define WALL_COUNT       32     /* 32 wall pages = 16 walls x {light, dark}
                                 * for full Wolf3D side-aware texturing
                                 * (vanilla horizwall[]=(i-1)*2 light Y-side,
                                 *  vertwall[]=(i-1)*2+1 dark X-side). */
#define DOOR_TEX_IDX     32     /* sentinel: tex_idx == WALL_COUNT means
                                 * "use door_tex"; out of [0..WALL_COUNT-1] */
#define CHUNKS_MAX       700

/* Door state machine constants. amt: 0..DOOR_AMT_OPEN, dir: idle/+1/-1.
 * One step per WM_TIMER (50 ms) = 64 steps over ~3.2 s — slightly slow
 * for muscle-memory but reads as a deliberate door swing on the small
 * viewport. Block threshold 56 means the player bounces off until the
 * door is mostly open, matching vanilla Wolf3D's behavior of doors
 * being non-traversable until ~7/8 open. */
#define DOOR_AMT_OPEN     64
#define DOOR_STEP         8     /* amt delta per WM_TIMER tick. Was 2 in
                                 * A.14.1 but with the ~5 FPS render rate
                                 * (each WM_TIMER blocked behind a paint)
                                 * full open took ~10s; bumped to 8 so the
                                 * full transition is ~8 ticks, a couple
                                 * seconds at typical render rate. */
#define DOOR_BLOCK_AMT    56    /* < this -> blocks movement */
#define DOOR_DIR_IDLE     0
#define DOOR_DIR_OPENING  1
#define DOOR_DIR_CLOSING  2
/* 26 sprite chunks total in our sprites[] array:
 *   slot 0      = SPR_DEMO       (VSWAP chunk sprite_start_idx + 0)
 *   slot 1      = SPR_DEATHCAM   (VSWAP chunk sprite_start_idx + 1)
 *   slot 2..17  = SPR_STAT_0..15 (VSWAP chunk sprite_start_idx + 2..17)
 *   slot 18..25 = SPR_GRD_S_1..8 (VSWAP chunk sprite_start_idx + 50..57)
 * The slots 18..25 are NOT contiguous with the underlying VSWAP indices
 * (we skip chunks 18..49 which are SPR_STAT_16..47, unused on E1L1). The
 * loader uses a sparse table sprite_chunk_offs[] mapping slot -> VSWAP
 * relative chunk index.
 *
 * Static decoration objects in E1L1 (lamps, columns, etc.) have plane1
 * obj_id 23..38 -> sprite_idx = obj_id - 23 + 2 (slots 2..17).
 * Guard enemy tiles (108..115 + 144..151 + 180..187 across difficulty
 * tiers) -> sprite_idx = GUARD_S_FIRST_SLOT (18) for the dry-run
 * (front-view only; rotation comes in iter 2). */
#define NUM_SPRITES           26
#define STAT_SPRITE_COUNT     18    /* slots 0..17 = legacy A.14 statics */
#define GUARD_S_FIRST_SLOT    18    /* slot 18 = SPR_GRD_S_1 (front view) */
#define GUARD_S_FRAME_COUNT   8     /* SPR_GRD_S_1..S_8 (8-direction front-still) */
#define GUARD_S_VSWAP_OFFSET  50    /* SPR_GRD_S_1 = sprite enum value 50 */

#define STAT_OBJ_FIRST        23
#define STAT_OBJ_LAST         38

/* Guard tile ranges in map_plane1 — three difficulty tiers, each with
 * stand/patrol pairs of 4 tiles (one per facing direction E/N/W/S).
 * Vanilla gates medium on gd_medium and hard on gd_hard, but we spawn
 * all so every guard the level designer placed is visible. */
#define GUARD_TILE_E_STAND_LO  108  /* 108..111 easy stand */
#define GUARD_TILE_E_STAND_HI  111
#define GUARD_TILE_E_PATROL_LO 112  /* 112..115 easy patrol */
#define GUARD_TILE_E_PATROL_HI 115
#define GUARD_TILE_M_STAND_LO  144  /* 144..147 medium stand */
#define GUARD_TILE_M_STAND_HI  147
#define GUARD_TILE_M_PATROL_LO 148  /* 148..151 medium patrol */
#define GUARD_TILE_M_PATROL_HI 151
#define GUARD_TILE_H_STAND_LO  180  /* 180..183 hard stand */
#define GUARD_TILE_H_STAND_HI  183
#define GUARD_TILE_H_PATROL_LO 184  /* 184..187 hard patrol */
#define GUARD_TILE_H_PATROL_HI 187

#define SPRITE_MAX       4096
#define MAX_OBJECTS      128
#define FOCAL_PIXELS     96L    /* (VIEW_W/2) / tan(FOV_ANGLES/2 in rad) */

/* Audio */
#define NUMAUDIOCHUNKS    288
#define MUSIC_SMOKE_CHUNK 261
#define MUSIC_BUF_BYTES   24000
#define MUSIC_TICK_HZ     700L

/* Trig + raycaster */
#define ANGLES        1024
#define ANGLE_QUAD    256
#define ANGLE_HALF    512
#define ANGLE_MASK    (ANGLES - 1)
#define FOV_ANGLES    192       /* ~67.5 deg total horizontal FOV */
#define ROT_STEP      32        /* ~11.25 deg per LEFT/RIGHT tap */
#define MOVE_STEP_Q88 24        /* ~3/32 tile per UP/DOWN tap */
#define MAX_CAST_STEPS 128      /* one step per tile crossed; map is 64x64 */

static BYTE  framebuf[64000];
static BYTE  static_bg[64000];

/* VSWAP buffers. walls[] = 32 * 4096 = 128 KB exceeds the 64 KB segment
 * cap that __far enforces; use __huge so Watcom places it across multiple
 * segments. Each 4 KB row sits inside a single segment so DrawWallStripCol
 * can deref by row to far ptr (mirror sprites[] pattern from A.14). */
static BYTE  __huge walls[WALL_COUNT][4096];
/* sprites[] = 26 * 4096 = 104 KB exceeds the 64 KB segment cap that
 * __far enforces; use __huge so Watcom places it across multiple
 * segments and synthesizes huge pointer arithmetic. The per-sprite
 * 4-KB rows still fit a single segment so DrawSprite/DrawSpriteWorld
 * never need to span — only sprite_idx selects which row's segment we
 * deref into. */
static BYTE  __huge sprites[NUM_SPRITES][SPRITE_MAX];
static WORD  sprite_len[NUM_SPRITES];
static DWORD __far pageoffs[CHUNKS_MAX];
static WORD  __far pagelens[CHUNKS_MAX];

/* Sparse VSWAP-chunk-offset table: sprite_chunk_offs[slot] = chunk index
 * relative to sprite_start_idx. Lets us skip the 32 unused SPR_STAT_16..47
 * chunks while still loading SPR_GRD_S_1..8 at slots 18..25. The legacy
 * A.14 contiguous load (slots 0..17 -> chunks 0..17) is preserved as the
 * identity prefix of this table. */
static const int sprite_chunk_offs[NUM_SPRITES] = {
    /* slots 0..17: SPR_DEMO, SPR_DEATHCAM, SPR_STAT_0..15 */
     0,  1,  2,  3,  4,  5,  6,  7,
     8,  9, 10, 11, 12, 13, 14, 15,
    16, 17,
    /* slots 18..25: SPR_GRD_S_1..S_8 (Wolf3D enum values 50..57) */
    50, 51, 52, 53, 54, 55, 56, 57
};

/* Door texture: single 64x64 page (4 KB) loaded from VSWAP at the
 * DOORWALL chunk = sprite_start_idx - 8 (vanilla Wolf3D convention,
 * see wolf3d/WOLFSRC/WL_DRAW.C HitVertDoor / HitHorizDoor). PoC only
 * loads the first door page; all door tiles 90..101 sample the same
 * texture regardless of lock type or orientation. */
static BYTE  __far door_tex[4096];
static int   gDoorTexErr = -1;

/* Per-tile door state. Indexed [ty * MAP_W + tx]. amt: open extent in
 * 1/64 tile (0=closed, 64=open). dir: idle/opening/closing. Only door
 * tiles (90..101) ever have non-zero values; non-door tiles stay 0. */
static BYTE  __far g_door_amt[MAP_TILES];
static BYTE  __far g_door_dir[MAP_TILES];

/* World-space billboards, populated at boot from plane1 scan.
 *   sprite_idx  : slot in sprites[] used as the base frame.
 *   enemy_dir   : 0..7 = WL_DEF.H dirtype value (E/NE/N/NW/W/SW/S/SE) for
 *                 enemy tiles; 0xFF = "not an enemy" (used by static
 *                 decoration entries). The dry-run renders all enemies
 *                 with sprite_idx alone (front view); iter 2 will use
 *                 enemy_dir + view-angle math to pick sprite_idx +
 *                 sector among slots 18..25. */
typedef struct {
    int  tile_x;
    int  tile_y;
    int  sprite_idx;
    BYTE enemy_dir;
} Object;
#define OBJ_DIR_NONE  0xFF
static Object __far g_objects[MAX_OBJECTS];
static int          g_num_objects = 0;

/* 1D z-buffer: perpendicular distance Q8.8 per viewport column. Walls
 * write it; sprites read it and skip columns where the wall is closer. */
static long g_zbuffer[VIEW_W];
static WORD  chunks_in_file, sprite_start_idx, sound_start_idx;
static int   gVSwapErr = -1;

/* Map */
static BYTE  carmack_src[CARMACK_SRC_MAX];
static WORD  carmack_dst[CARMACK_DST_MAX / 2];
static WORD  __far map_plane0[MAP_TILES];
static WORD  __far map_plane1[MAP_TILES];
static WORD  maphead_rlew_tag = 0;
static DWORD __far map_headeroffs[NUMMAPS];
static WORD  map_width        = 0;
static WORD  map_height       = 0;
static int   gMapErr  = -1;
static int   gLoadErr = -1;

/* Trig: sin_q15_lut[1024] is __far const (in wolfvis_a13_sintab.h). No
 * runtime sin() — pulling Watcom's math runtime would drag in the FP
 * emulation library that requires WIN87EM.DLL at load time, which VIS
 * does not ship → "Error loading <EXE>" loop reset. Static LUT keeps the
 * EXE self-contained. */
static int   __far fov_correct[VIEW_W];   /* Q1.15 cos of column angle offset */

/* Player state in Q8.8 tile units. Y+ is south. */
static long  g_px = (long)(32L << 8) | 0x80;   /* tile 32.5 */
static long  g_py = (long)(32L << 8) | 0x80;
static int   g_pa = 0;
static long  g_px_prev = (long)(32L << 8) | 0x80;
static long  g_py_prev = (long)(32L << 8) | 0x80;
static int   g_pa_prev = 0;
static int   g_player_inited = 0;

static struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bmi;
static struct { BITMAPINFOHEADER bmiHeader; WORD    bmiColors[256]; } bmiPal;

static HPALETTE gPal = NULL;
static BOOL     gPaletteRealized = FALSE;
static BYTE     gAudioOn = 0;

/* IMF audio state (port of A.10) */
static DWORD __far audio_offsets[NUMAUDIOCHUNKS + 1];
static int   gAudioHdrErr = -1;
static BYTE  __far music_buf[MUSIC_BUF_BYTES];
static UINT  gMusicLen   = 0;
static int   gMusicLoadErr = -1;
static int   gMusicChunk = -1;
static WORD *sqHack    = NULL;
static WORD *sqHackPtr = NULL;
static WORD  sqHackLen = 0;
static WORD  sqHackSeqLen = 0;
static DWORD sqHackTime = 0;
static DWORD alTimeCount = 0;
static BOOL  sqActive = FALSE;
static DWORD sqLastTick = 0;     /* unused with PIT-direct, kept for ABI */

/* PIT-direct timing state. See A.12 comment for full rationale. */
#define PIT_CYCLES_PER_IMF_TICK 852U
static WORD  prev_pit_count = 0;
static DWORD pit_accum      = 0;

/* Debug bar state */
static WORD  last_key_wparam = 0;
static WORD  key_count       = 0;
static WORD  last_msg_type   = 0;
static WORD  msg_count       = 0;
static WORD  tick_count      = 0;
static BOOL  has_focus       = FALSE;

/* Perf telemetry: ms spent in the most recent DrawViewport+sprites
 * pass, measured via PIT counter (596.4 cycles/ms — same divisor as
 * IMF clock, see reference_pit_596khz_vis). Displayed as a bit grid
 * in the debug bar so future perf-sweep work has a baseline to
 * regress against. */
static WORD  g_last_render_ms = 0;

/* ---- OPL3 routines ---- */

static void OplDelay(int cycles)
{
    while (cycles-- > 0) inp(0x388);
}

static void OplOut(BYTE reg, BYTE val)
{
    outp(0x388, reg);
    OplDelay(6);
    outp(0x389, val);
    OplDelay(35);
}

static void OplReset(void)
{
    int i;
    for (i = 0xB0; i <= 0xB8; i++) OplOut((BYTE)i, 0x00);
    for (i = 0x40; i <= 0x55; i++) OplOut((BYTE)i, 0x3F);
}

static void OplInit(void)
{
    OplReset();
    OplOut(0x20, 0x21);
    OplOut(0x40, 0x3F);
    OplOut(0x60, 0xF0);
    OplOut(0x80, 0x05);
    OplOut(0xE0, 0x00);
    OplOut(0x23, 0x21);
    OplOut(0x43, 0x00);
    OplOut(0x63, 0xF0);
    OplOut(0x83, 0x05);
    OplOut(0xE3, 0x00);
    OplOut(0xC0, 0x00);
}

static void OplNoteOn(WORD fnum, BYTE block)
{
    BYTE b;
    OplOut(0xA0, (BYTE)(fnum & 0xFF));
    b = (BYTE)(0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
    OplOut(0xB0, b);
    gAudioOn = 1;
}

static void OplNoteOff(void)
{
    OplOut(0xB0, 0x00);
}

/* ---- PIT-direct hi-res clock ---- */

static WORD ReadPitCounter(void)
{
    BYTE lo, hi;
    outp(0x43, 0x00);
    lo = (BYTE)inp(0x40);
    hi = (BYTE)inp(0x40);
    return ((WORD)hi << 8) | (WORD)lo;
}

static void AdvanceAlTimeFromPit(void)
{
    WORD  now;
    DWORD diff;

    now = ReadPitCounter();
    if (now > prev_pit_count) {
        diff = (DWORD)prev_pit_count + (65536UL - (DWORD)now);
    } else {
        diff = (DWORD)(prev_pit_count - now);
    }
    prev_pit_count = now;
    pit_accum     += diff;

    while (pit_accum >= PIT_CYCLES_PER_IMF_TICK) {
        pit_accum   -= PIT_CYCLES_PER_IMF_TICK;
        alTimeCount += 1;
    }
}

/* ---- IMF loader + scheduler ---- */

static int LoadAudioHeader(void)
{
    HFILE f;
    OFSTRUCT of;
    UINT n;

    f = OpenFile("A:\\AUDIOHED.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)audio_offsets, sizeof(audio_offsets));
    _lclose(f);
    if (n != sizeof(audio_offsets)) return 2;
    return 0;
}

static int LoadMusicChunk(int chunk_idx)
{
    HFILE f;
    OFSTRUCT of;
    LONG  pos;
    UINT  n;
    DWORD chunk_off, chunk_len;

    if (chunk_idx < 0 || chunk_idx >= NUMAUDIOCHUNKS) return 1;
    chunk_off = audio_offsets[chunk_idx];
    chunk_len = audio_offsets[chunk_idx + 1] - chunk_off;
    if (chunk_len < 4 || chunk_len > MUSIC_BUF_BYTES) return 2;

    f = OpenFile("A:\\AUDIOT.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 3;
    pos = _llseek(f, (LONG)chunk_off, 0);
    if (pos == -1L) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)music_buf, (UINT)chunk_len);
    _lclose(f);
    if (n != chunk_len) return 5;

    gMusicLen   = (UINT)chunk_len;
    gMusicChunk = chunk_idx;
    return 0;
}

static void OplMusicReset(void)
{
    int i;
    for (i = 0xB0; i <= 0xB8; i++) OplOut((BYTE)i, 0x00);
    OplOut(0xBD, 0x00);
}

static void StartMusic(void)
{
    WORD imf_len;

    if (gMusicLen < 4) { sqActive = FALSE; return; }
    imf_len = music_buf[0] | ((WORD)music_buf[1] << 8);
    if (imf_len == 0 || imf_len > gMusicLen - 2 || (imf_len & 3)) {
        sqActive = FALSE;
        return;
    }

    OplMusicReset();
    sqHack         = (WORD *)(music_buf + 2);
    sqHackPtr      = sqHack;
    sqHackSeqLen   = imf_len;
    sqHackLen      = imf_len;
    sqHackTime     = 0;
    alTimeCount    = 0;
    prev_pit_count = ReadPitCounter();
    pit_accum      = 0;
    sqActive       = TRUE;
    gAudioOn       = 1;
}

static void StopMusic(void)
{
    sqActive = FALSE;
    OplMusicReset();
    gAudioOn = 0;
}

static void ServiceMusic(void)
{
    WORD reg_val;
    BYTE reg, val;
    WORD delay;

    if (!sqActive) return;

    AdvanceAlTimeFromPit();
    if (alTimeCount > sqHackTime + 4) alTimeCount = sqHackTime;

    while (sqHackLen >= 4 && sqHackTime <= alTimeCount) {
        reg_val   = *sqHackPtr++;
        delay     = *sqHackPtr++;
        reg       = (BYTE)(reg_val & 0xFF);
        val       = (BYTE)(reg_val >> 8);
        OplOut(reg, val);
        sqHackTime += delay;
        sqHackLen -= 4;
    }

    if (sqHackLen < 4) {
        sqHackPtr      = sqHack;
        sqHackLen      = sqHackSeqLen;
        alTimeCount    = 0;
        sqHackTime     = 0;
        prev_pit_count = ReadPitCounter();
        pit_accum      = 0;
    }
}

/* ---- VSWAP loader (walls only — A.13 dropped sprite gallery) ---- */

static int LoadVSwap(void)
{
    HFILE f;
    OFSTRUCT of;
    WORD hdr[3];
    UINT cbOffs, cbLens;
    UINT n;
    LONG pos;
    int  i;

    f = OpenFile("A:\\VSWAP.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;

    n = _lread(f, (LPVOID)hdr, 6);
    if (n != 6) { _lclose(f); return 2; }
    chunks_in_file   = hdr[0];
    sprite_start_idx = hdr[1];
    sound_start_idx  = hdr[2];
    if (chunks_in_file == 0 || chunks_in_file > CHUNKS_MAX) { _lclose(f); return 3; }

    cbOffs = (UINT)chunks_in_file * 4U;
    cbLens = (UINT)chunks_in_file * 2U;

    n = _lread(f, (LPVOID)pageoffs, cbOffs);
    if (n != cbOffs) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)pagelens, cbLens);
    if (n != cbLens) { _lclose(f); return 5; }

    for (i = 0; i < WALL_COUNT; i++) {
        pos = _llseek(f, (LONG)pageoffs[i], 0);
        if (pos == -1L) { _lclose(f); return 6; }
        n = _lread(f, (LPVOID)walls[i], 4096);
        if (n != 4096) { _lclose(f); return 7; }
    }

    /* Sprites: NUM_SPRITES chunks loaded sparsely via sprite_chunk_offs[].
     * Slots 0..17 map to contiguous chunks 0..17 (legacy A.14 layout);
     * slots 18..25 map to chunks 50..57 (SPR_GRD_S_1..S_8). Pages may
     * have variable length (t_compshape encoded), so we read pagelens[i]
     * bytes per chunk; empty pages (len==0) get sprite_len[i]=0 and are
     * silently skipped at draw time. */
    for (i = 0; i < NUM_SPRITES; i++) {
        WORD chunk = (WORD)(sprite_start_idx + sprite_chunk_offs[i]);
        WORD len;
        sprite_len[i] = 0;
        if (chunk >= chunks_in_file) continue;   /* sparse: chunk OOB == empty slot */
        len = pagelens[chunk];
        if (len == 0) continue;
        if (len > SPRITE_MAX) { _lclose(f); return 8; }
        pos = _llseek(f, (LONG)pageoffs[chunk], 0);
        if (pos == -1L) { _lclose(f); return 9; }
        n = _lread(f, (LPVOID)sprites[i], len);
        if (n != len) { _lclose(f); return 10; }
        sprite_len[i] = len;
    }

    /* Door texture: 4 KB raw page at chunk (sprite_start_idx - 8). This
     * is DOORWALL+0 in vanilla Wolf3D (the normal-door slab face). We
     * ignore +2/+3 (door-side walls), +4 (elevator), +6 (locked) for
     * PoC — all door tiles render with the same single-texture slab. */
    if (sprite_start_idx >= 8) {
        WORD door_chunk = (WORD)(sprite_start_idx - 8);
        if (door_chunk < chunks_in_file && pagelens[door_chunk] >= 4096) {
            pos = _llseek(f, (LONG)pageoffs[door_chunk], 0);
            if (pos != -1L) {
                n = _lread(f, (LPVOID)door_tex, 4096);
                if (n == 4096) gDoorTexErr = 0;
                else           gDoorTexErr = 12;
            } else {
                gDoorTexErr = 11;
            }
        } else {
            gDoorTexErr = 13;
        }
    } else {
        gDoorTexErr = 14;
    }

    _lclose(f);
    return 0;
}

static void InitPalette(void)
{
    int i;
    bmi.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth         = SCR_W;
    bmi.bmiHeader.biHeight        = SCR_H;
    bmi.bmiHeader.biPlanes        = 1;
    bmi.bmiHeader.biBitCount      = 8;
    bmi.bmiHeader.biCompression   = BI_RGB;
    bmi.bmiHeader.biClrUsed       = 256;
    bmi.bmiHeader.biClrImportant  = 256;
    for (i = 0; i < 256; i++) {
        bmi.bmiColors[i].rgbRed      = (BYTE)(gamepal6[i*3 + 0] << 2);
        bmi.bmiColors[i].rgbGreen    = (BYTE)(gamepal6[i*3 + 1] << 2);
        bmi.bmiColors[i].rgbBlue     = (BYTE)(gamepal6[i*3 + 2] << 2);
        bmi.bmiColors[i].rgbReserved = 0;
    }
}

static void BuildPalette(void)
{
    struct { WORD ver; WORD n; PALETTEENTRY p[256]; } lp;
    int i;
    lp.ver = 0x300;
    lp.n   = 256;
    for (i = 0; i < 256; i++) {
        lp.p[i].peRed   = bmi.bmiColors[i].rgbRed;
        lp.p[i].peGreen = bmi.bmiColors[i].rgbGreen;
        lp.p[i].peBlue  = bmi.bmiColors[i].rgbBlue;
        lp.p[i].peFlags = PC_NOCOLLAPSE;
    }
    gPal = CreatePalette((LOGPALETTE FAR *)&lp);
    bmiPal.bmiHeader = bmi.bmiHeader;
    for (i = 0; i < 256; i++) bmiPal.bmiColors[i] = (WORD)i;
}

static int CarmackExpand(const BYTE far *src, int src_len, WORD far *dst, int dst_words_max)
{
    const BYTE far *sp = src;
    const BYTE far *send = src + src_len;
    WORD far *dp = dst;
    WORD far *dend = dst + dst_words_max;
    WORD ch;
    BYTE lo, hi;
    int count, offset;
    WORD far *copyptr;

    while (dp < dend && sp + 2 <= send) {
        lo = *sp++;
        hi = *sp++;
        ch = ((WORD)hi << 8) | lo;
        if (hi == 0xA7) {
            count = lo;
            if (count == 0) {
                if (sp >= send) return -1;
                *dp++ = ((WORD)0xA7 << 8) | *sp++;
            } else {
                if (sp >= send) return -1;
                offset = *sp++;
                if (offset == 0) return -2;
                copyptr = dp - offset;
                if (copyptr < dst) return -3;
                while (count-- && dp < dend) *dp++ = *copyptr++;
            }
        } else if (hi == 0xA8) {
            count = lo;
            if (count == 0) {
                if (sp >= send) return -1;
                *dp++ = ((WORD)0xA8 << 8) | *sp++;
            } else {
                WORD off;
                if (sp + 2 > send) return -1;
                off = sp[0] | ((WORD)sp[1] << 8);
                sp += 2;
                if ((int)off >= dst_words_max) return -4;
                copyptr = dst + off;
                while (count-- && dp < dend) *dp++ = *copyptr++;
            }
        } else {
            *dp++ = ch;
        }
    }
    return (int)(dp - dst);
}

static int RLEWExpand(const WORD far *src, int src_words, WORD far *dst, int dst_words, WORD rlewtag)
{
    const WORD far *sp = src;
    const WORD far *send = src + src_words;
    WORD far *dp = dst;
    WORD far *dend = dst + dst_words;
    WORD w, count, value;

    while (sp < send && dp < dend) {
        w = *sp++;
        if (w == rlewtag) {
            if (sp + 2 > send) return -1;
            count = *sp++;
            value = *sp++;
            while (count-- && dp < dend) *dp++ = value;
        } else {
            *dp++ = w;
        }
    }
    return (int)(dp - dst);
}

static int LoadMapHead(void)
{
    HFILE f;
    OFSTRUCT of;
    WORD tag;
    UINT n;

    f = OpenFile("A:\\MAPHEAD.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)&tag, 2);
    if (n != 2) { _lclose(f); return 2; }
    maphead_rlew_tag = tag;
    n = _lread(f, (LPVOID)map_headeroffs, (UINT)(NUMMAPS * 4));
    if (n != NUMMAPS * 4) { _lclose(f); return 3; }
    _lclose(f);
    return 0;
}

static int LoadMapPlane(HFILE f, DWORD plane_start, UINT plane_len, WORD far *dst)
{
    LONG pos;
    UINT n;
    WORD carmack_expanded;
    int  carmack_words_out;
    WORD rlew_expanded;
    int  rlew_words_out;

    if (plane_len == 0 || plane_len > CARMACK_SRC_MAX) return 10;

    pos = _llseek(f, (LONG)plane_start, 0);
    if (pos == -1L) return 11;
    n = _lread(f, (LPVOID)carmack_src, plane_len);
    if (n != plane_len) return 12;

    carmack_expanded = carmack_src[0] | ((WORD)carmack_src[1] << 8);
    if (carmack_expanded == 0 || carmack_expanded > CARMACK_DST_MAX) return 13;

    carmack_words_out = CarmackExpand(
        carmack_src + 2, (int)(plane_len - 2),
        carmack_dst, carmack_expanded / 2);
    if (carmack_words_out < 2) return 14;

    rlew_expanded = carmack_dst[0];
    if (rlew_expanded != (WORD)(MAP_W * MAP_H * 2)) return 15;

    rlew_words_out = RLEWExpand(
        carmack_dst + 1, carmack_words_out - 1,
        dst, MAP_TILES, maphead_rlew_tag);
    if (rlew_words_out != MAP_TILES) return 16;

    return 0;
}

static int LoadMap(int mapnum)
{
    HFILE f;
    OFSTRUCT of;
    UINT n;
    LONG pos;
    DWORD hdr_off;
    struct {
        DWORD planestart[3];
        WORD  planelength[3];
        WORD  width, height;
        char  name[16];
    } maptype_buf;
    int rc;

    if (mapnum < 0 || mapnum >= NUMMAPS) return 1;
    hdr_off = map_headeroffs[mapnum];
    if (hdr_off == 0 || hdr_off == 0xFFFFFFFFUL) return 2;

    f = OpenFile("A:\\GAMEMAPS.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 3;

    pos = _llseek(f, (LONG)hdr_off, 0);
    if (pos == -1L) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)&maptype_buf, 38);
    if (n != 38) { _lclose(f); return 5; }

    map_width  = maptype_buf.width;
    map_height = maptype_buf.height;
    if (map_width != MAP_W || map_height != MAP_H) { _lclose(f); return 6; }

    rc = LoadMapPlane(f, maptype_buf.planestart[0], maptype_buf.planelength[0], map_plane0);
    if (rc) { _lclose(f); return 20 + rc; }
    rc = LoadMapPlane(f, maptype_buf.planestart[1], maptype_buf.planelength[1], map_plane1);
    if (rc) { _lclose(f); return 40 + rc; }

    _lclose(f);
    return 0;
}

/* ---- Trig table init ---- */

static void InitTrig(void)
{
    int i;
    for (i = 0; i < VIEW_W; i++) {
        int off = ((i - VIEW_W/2) * FOV_ANGLES) / VIEW_W;
        int idx = (off + ANGLES) & ANGLE_MASK;
        /* cos(off) = sin_q15_lut[off + ANGLE_QUAD] */
        idx = (idx + ANGLE_QUAD) & ANGLE_MASK;
        fov_correct[i] = sin_q15_lut[idx];
    }
}

#define SIN_Q15(a) (sin_q15_lut[(a) & ANGLE_MASK])
#define COS_Q15(a) (sin_q15_lut[((a) + ANGLE_QUAD) & ANGLE_MASK])

/* ---- Atan2 + dirtype table for enemy rotation (A.16a iter 2) ---- */

/* WL_DEF.H dirtype enum -> our Q10 angle space (0=E, 256=S, 512=W, 768=N,
 * CW because Y+ is south). dirtype CCW order: E/NE/N/NW/W/SW/S/SE = 0..7,
 * which maps to our Q10 as 0/896/768/640/512/384/256/128. NE in our
 * convention is between N (768) and E (1024=0), i.e. 896 — diagonals
 * sit at the half-sector centers. */
static const int __far dirangle_q10[8] = {
       0,    /* east       */
     896,    /* northeast  */
     768,    /* north      */
     640,    /* northwest  */
     512,    /* west       */
     384,    /* southwest  */
     256,    /* south      */
     128     /* southeast  */
};

/* atan2 in our Q10 angle space without dragging in <math.h> (which would
 * pull WIN87EM.DLL — see reference_win87em_trap.md). Uses the precomputed
 * Q10 atan LUT for t in [0, 1] then quadrant-fixes via signs of dx, dy.
 * Inputs are signed longs; only the ratio and signs matter, magnitude
 * is irrelevant (Q8.8 deltas as used by DrawSpriteWorld are fine). */
static int atan2_q10(long dy, long dx)
{
    long abs_dx = (dx < 0) ? -dx : dx;
    long abs_dy = (dy < 0) ? -dy : dy;
    int  t_idx, base;

    if (abs_dx == 0 && abs_dy == 0) return 0;

    if (abs_dx >= abs_dy) {
        /* |slope| <= 1: angle within +-45 deg of E or W axis. */
        t_idx = (int)((abs_dy * (long)ATAN_LUT_N) / abs_dx);
        if (t_idx < 0) t_idx = 0;
        if (t_idx > ATAN_LUT_N) t_idx = ATAN_LUT_N;
        base = atan_q10_lut[t_idx];          /* 0..128 in Q10 */
    } else {
        /* |slope| > 1: angle within +-45 deg of S or N axis. */
        t_idx = (int)((abs_dx * (long)ATAN_LUT_N) / abs_dy);
        if (t_idx < 0) t_idx = 0;
        if (t_idx > ATAN_LUT_N) t_idx = ATAN_LUT_N;
        base = 256 - atan_q10_lut[t_idx];    /* 128..256 in Q10 */
    }

    /* Quadrant fixup. Our Q10: 0=E, 256=S, 512=W, 768=N, CW. */
    if (dx >= 0 && dy >= 0) return base;            /* Q1 (E..S):    0..256 */
    if (dx <  0 && dy >= 0) return 512 - base;      /* Q2 (S..W):  256..512 */
    if (dx <  0 && dy <  0) return 512 + base;      /* Q3 (W..N):  512..768 */
    return 1024 - base;                              /* Q4 (N..E):  768..1024 */
}

/* ---- IsWall + InitPlayer ---- */

static int IsWall(int tx, int ty)
{
    WORD tile;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    tile = map_plane0[ty * MAP_W + tx];
    /* Wolf3D wall tiles: 1..63. Doors are NOT walls in A.14.1 — they
     * are handled separately by CastRay's door branch (mid-plane test
     * against g_door_amt) so closed doors block, open doors don't, and
     * partial states show a sliding slab. */
    if (tile >= 1 && tile <= 63) return 1;
    return 0;
}

/* Returns 1 if (tx,ty) is a vertical door (slab runs N-S, slides on Y),
 * 2 if horizontal (slab runs E-W, slides on X), 0 otherwise. From
 * vanilla Wolf3D's WL_GAME.C SpawnDoor: even tiles (90,92,94,96,98,100)
 * are vertical, odd tiles (91,93,95,97,99,101) are horizontal. */
static int IsDoor(int tx, int ty)
{
    WORD tile;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    tile = map_plane0[ty * MAP_W + tx];
    if (tile < 90 || tile > 101) return 0;
    return ((tile & 1) == 0) ? 1 : 2;   /* 90 even -> vertical */
}

/* Movement-only collision. Walls always block. Doors block only when
 * mostly closed (amt < DOOR_BLOCK_AMT). Vanilla Wolf3D requires the
 * door to be ~7/8 open before the player can pass; we use 56/64 ≈ 0.875
 * to match. Threshold also prevents the player from getting stuck in
 * a half-open door if PRIMARY is mashed during animation. */
static int IsBlockingForMove(int tx, int ty)
{
    WORD tile;
    int  ti;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    tile = map_plane0[ty * MAP_W + tx];
    if (tile >= 1 && tile <= 63) return 1;
    if (tile >= 90 && tile <= 101) {
        ti = ty * MAP_W + tx;
        return (g_door_amt[ti] < DOOR_BLOCK_AMT) ? 1 : 0;
    }
    return 0;
}

/* Pick wall texture for a map tile + side. Vanilla Wolf3D mapping
 * (WL_MAIN.C:712-713): horizwall[i] = (i-1)*2 (light, Y-side hit i.e. ray
 * crosses a Y-grid line and hits a horizontal wall face), vertwall[i] =
 * (i-1)*2+1 (dark, X-side hit). With WALL_COUNT=32 we cover tiles 1..16
 * directly with both light/dark faces; tiles 17..63 wrap modulo into the
 * same loaded bank, but every distinct tile_id still gets a distinct
 * (light, dark) pair — fixing the pre-A.4 Hitler-poster collapse. */
#define X_SIDE  0   /* ray crossed an X-grid (vertical wall face), DARK */
#define Y_SIDE  1   /* ray crossed a Y-grid (horizontal wall face), LIGHT */

static int TileToWallTex(WORD tile, int side)
{
    int idx;
    if (tile == 0) return 0;
    idx = (int)(tile - 1) * 2 + ((side == X_SIDE) ? 1 : 0);
    if (idx < 0)            idx = 0;
    idx = idx % WALL_COUNT;
    return idx;
}

/* Advance every door by elapsed wall-clock time since last call. A.13.1:
 * was advancing by fixed DOOR_STEP per WM_TIMER, but at low render rates
 * (5 FPS) the timer was throttled and full-open took ~10 s. Time-scaled
 * via GetTickCount makes the open/close duration independent of frame
 * rate: DOOR_MS_FULL_OPEN ms regardless of how often AdvanceDoors fires.
 *
 * Tick tracking: g_door_last_tick is the wall-clock at last call. First
 * call after boot init it from current; subsequent calls compute delta. */
#define DOOR_MS_FULL_OPEN  1200    /* full open/close in 1.2 s */
static DWORD g_door_last_tick = 0;

static BOOL AdvanceDoors(void)
{
    int   i;
    BOOL  changed = FALSE;
    DWORD now, elapsed;
    long  step;

    now = GetTickCount();
    if (g_door_last_tick == 0) {
        g_door_last_tick = now;
        return FALSE;
    }
    elapsed = now - g_door_last_tick;
    g_door_last_tick = now;
    if (elapsed == 0) return FALSE;
    /* step = elapsed * DOOR_AMT_OPEN / DOOR_MS_FULL_OPEN. Floor to >=1
     * so very fast frame rates still produce visible motion. Clamp to
     * DOOR_AMT_OPEN to handle long pauses (alt-tab) without wraparound. */
    step = ((long)elapsed * (long)DOOR_AMT_OPEN) / (long)DOOR_MS_FULL_OPEN;
    if (step < 1)               step = 1;
    if (step > DOOR_AMT_OPEN)   step = DOOR_AMT_OPEN;

    for (i = 0; i < MAP_TILES; i++) {
        if (g_door_dir[i] == DOOR_DIR_IDLE) continue;
        if (g_door_dir[i] == DOOR_DIR_OPENING) {
            if ((long)g_door_amt[i] + step >= DOOR_AMT_OPEN) {
                g_door_amt[i] = DOOR_AMT_OPEN;
                g_door_dir[i] = DOOR_DIR_IDLE;
            } else {
                g_door_amt[i] = (BYTE)(g_door_amt[i] + step);
            }
            changed = TRUE;
        } else if (g_door_dir[i] == DOOR_DIR_CLOSING) {
            if ((long)g_door_amt[i] <= step) {
                g_door_amt[i] = 0;
                g_door_dir[i] = DOOR_DIR_IDLE;
            } else {
                g_door_amt[i] = (BYTE)(g_door_amt[i] - step);
            }
            changed = TRUE;
        }
    }
    return changed;
}

/* Step one tile-unit forward along player heading and toggle the door
 * at that tile (if any). Forward step is COS_Q15(g_pa) along X, SIN
 * along Y. Returns TRUE if a toggle occurred (so the caller redraws). */
static BOOL ToggleDoorInFront(void)
{
    long fx_q88, fy_q88;
    int  ftx, fty, ti;

    fx_q88 = g_px + (((long)COS_Q15(g_pa) * (long)256L) >> 15);
    fy_q88 = g_py + (((long)SIN_Q15(g_pa) * (long)256L) >> 15);
    ftx = (int)(fx_q88 >> 8);
    fty = (int)(fy_q88 >> 8);
    if (!IsDoor(ftx, fty)) return FALSE;

    ti = fty * MAP_W + ftx;
    /* Toggle: idle-closed -> opening, idle-open -> closing, mid-anim
     * -> reverse direction so the door can be slammed/cancelled. */
    if (g_door_dir[ti] == DOOR_DIR_IDLE) {
        if (g_door_amt[ti] == 0)              g_door_dir[ti] = DOOR_DIR_OPENING;
        else if (g_door_amt[ti] == DOOR_AMT_OPEN)
                                              g_door_dir[ti] = DOOR_DIR_CLOSING;
        else                                  g_door_dir[ti] = DOOR_DIR_OPENING;
    } else if (g_door_dir[ti] == DOOR_DIR_OPENING) {
        g_door_dir[ti] = DOOR_DIR_CLOSING;
    } else {
        g_door_dir[ti] = DOOR_DIR_OPENING;
    }
    return TRUE;
}

static void InitPlayer(void)
{
    int tx, ty;
    WORD obj;
    int found = 0;

    if (gMapErr != 0) {
        /* No map — fallback to map center, angle 0. */
        g_px = (32L << 8) | 0x80;
        g_py = (32L << 8) | 0x80;
        g_pa = 0;
        return;
    }

    /* First pass: look for plane1 spawn markers 19=N, 20=E, 21=S, 22=W. */
    for (ty = 0; ty < MAP_H && !found; ty++) {
        for (tx = 0; tx < MAP_W && !found; tx++) {
            obj = map_plane1[ty * MAP_W + tx];
            if (obj >= 19 && obj <= 22 && !IsWall(tx, ty)) {
                g_px = ((long)tx << 8) | 0x80;
                g_py = ((long)ty << 8) | 0x80;
                /* 19=N(-Y), 20=E(+X), 21=S(+Y), 22=W(-X). Our angle conv:
                 * 0=E, 256=S, 512=W, 768=N. */
                switch (obj) {
                case 19: g_pa = 768; break;
                case 20: g_pa = 0;   break;
                case 21: g_pa = 256; break;
                case 22: g_pa = 512; break;
                }
                found = 1;
            }
        }
    }

    /* Fallback: first non-wall tile, facing east. */
    if (!found) {
        for (ty = 0; ty < MAP_H && !found; ty++) {
            for (tx = 0; tx < MAP_W && !found; tx++) {
                if (!IsWall(tx, ty)) {
                    g_px = ((long)tx << 8) | 0x80;
                    g_py = ((long)ty << 8) | 0x80;
                    g_pa = 0;
                    found = 1;
                }
            }
        }
    }

    g_px_prev = g_px;
    g_py_prev = g_py;
    g_pa_prev = g_pa;
    g_player_inited = 1;
}

/* Scan plane1 for static decoration objects (Wolf3D obj IDs 23..38).
 * Each match becomes a billboard at its tile center. obj_id - 23 = the
 * SPR_STAT_n index, our sprite_idx = (obj_id - 23) + 2 because chunks
 * 0/1 are Demo / DeathCam (not in-world). */
/* Map a guard-tile value (108..115 / 144..151 / 180..187) to the
 * E/N/W/S facing index 0..3, or -1 if not a guard tile. The vanilla
 * decoder is just (tile - base) where base is the low end of the
 * stand or patrol set; all six bases are 4 apart. */
static int GuardTileToDir(WORD tile)
{
    if (tile >= GUARD_TILE_E_STAND_LO  && tile <= GUARD_TILE_E_STAND_HI ) return (int)(tile - GUARD_TILE_E_STAND_LO);
    if (tile >= GUARD_TILE_E_PATROL_LO && tile <= GUARD_TILE_E_PATROL_HI) return (int)(tile - GUARD_TILE_E_PATROL_LO);
    if (tile >= GUARD_TILE_M_STAND_LO  && tile <= GUARD_TILE_M_STAND_HI ) return (int)(tile - GUARD_TILE_M_STAND_LO);
    if (tile >= GUARD_TILE_M_PATROL_LO && tile <= GUARD_TILE_M_PATROL_HI) return (int)(tile - GUARD_TILE_M_PATROL_LO);
    if (tile >= GUARD_TILE_H_STAND_LO  && tile <= GUARD_TILE_H_STAND_HI ) return (int)(tile - GUARD_TILE_H_STAND_LO);
    if (tile >= GUARD_TILE_H_PATROL_LO && tile <= GUARD_TILE_H_PATROL_HI) return (int)(tile - GUARD_TILE_H_PATROL_LO);
    return -1;
}

/* Counters reset each scan, exposed for the debug bar so we can confirm
 * "the level has N guards" at runtime without disassembling map data. */
static int g_num_static = 0;
static int g_num_enemies = 0;

static void ScanObjects(void)
{
    int  tx, ty;
    WORD obj;
    int  sprite_idx;
    int  dir4;
    BYTE dir8;

    g_num_objects = 0;
    g_num_static = 0;
    g_num_enemies = 0;
    if (gMapErr != 0) return;

    for (ty = 0; ty < MAP_H && g_num_objects < MAX_OBJECTS; ty++) {
        for (tx = 0; tx < MAP_W && g_num_objects < MAX_OBJECTS; tx++) {
            obj = map_plane1[ty * MAP_W + tx];

            /* Branch 1: static decoration (lamps, plants, etc.). */
            if (obj >= STAT_OBJ_FIRST && obj <= STAT_OBJ_LAST) {
                sprite_idx = (int)(obj - STAT_OBJ_FIRST) + 2;
                if (sprite_idx < 0 || sprite_idx >= NUM_SPRITES) continue;
                if (sprite_len[sprite_idx] == 0) continue;
                g_objects[g_num_objects].tile_x     = tx;
                g_objects[g_num_objects].tile_y     = ty;
                g_objects[g_num_objects].sprite_idx = sprite_idx;
                g_objects[g_num_objects].enemy_dir  = OBJ_DIR_NONE;
                g_num_objects++;
                g_num_static++;
                continue;
            }

            /* Branch 2: guard enemy tiles (stand or patrol, any tier).
             * Dry-run: all guards render with slot 18 = SPR_GRD_S_1
             * (front view). enemy_dir is captured for iter 2 rotation. */
            dir4 = GuardTileToDir(obj);
            if (dir4 >= 0) {
                if (sprite_len[GUARD_S_FIRST_SLOT] == 0) continue;
                dir8 = (BYTE)(dir4 * 2);    /* WL_DEF.H dirtype: E/N/W/S = 0/2/4/6 */
                g_objects[g_num_objects].tile_x     = tx;
                g_objects[g_num_objects].tile_y     = ty;
                g_objects[g_num_objects].sprite_idx = GUARD_S_FIRST_SLOT;
                g_objects[g_num_objects].enemy_dir  = dir8;
                g_num_objects++;
                g_num_enemies++;
            }
        }
    }
}

/* ---- Drawing primitives ---- */

static void ClearFrame(void)
{
    BYTE *p = framebuf;
    unsigned n;
    for (n = 0; n < 64000U; n++) *p++ = 0;
}

static void FB_Put(int sx, int sy, BYTE pix)
{
    int fb_y;
    if (sx < 0 || sx >= SCR_W || sy < 0 || sy >= SCR_H) return;
    fb_y = (SCR_H - 1) - sy;
    framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = pix;
}

static void FB_FillRect(int x, int y, int w, int h, BYTE pix)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            FB_Put(x + i, y + j, pix);
}

static BYTE TileToColor(WORD tile)
{
    if (tile == 0) return 31;
    if (tile == 21 || tile == 22) return 15;
    if (tile >= 90 && tile <= 101) return (BYTE)176;
    if (tile <= 63) return (BYTE)(64 + tile * 3);
    return (BYTE)(tile & 0xFF);
}

static BYTE ObjectToColor(WORD obj)
{
    if (obj == 0) return 0;
    if (obj >= 19 && obj <= 22) return 14;             /* player spawn markers */
    if (obj >= 23 && obj <= 74) return 135;            /* static decoration */
    /* Guards (stand + patrol, all 3 difficulty tiers) — colored bright
     * red so they stand out from decorations on the minimap. */
    if (obj >= GUARD_TILE_E_STAND_LO  && obj <= GUARD_TILE_E_PATROL_HI) return 40;
    if (obj >= GUARD_TILE_M_STAND_LO  && obj <= GUARD_TILE_M_PATROL_HI) return 40;
    if (obj >= GUARD_TILE_H_STAND_LO  && obj <= GUARD_TILE_H_PATROL_HI) return 40;
    /* Other enemy classes (officer/SS/dog) — colored differently for
     * visual differentiation, even though A.16a doesn't render them in
     * the world (only on the minimap as "things-not-yet-implemented"). */
    if (obj >= 116 && obj <= 213) return 44;
    return 127;
}

static void DrawMinimapWithPlayer(void)
{
    int tx, ty;
    WORD tile, obj;
    BYTE wc, oc;
    int  ppx, ppy, hx, hy;

    /* Background tiles + objects. Door tiles get a state-dependent
     * color so the minimap doubles as a "where can I go now" indicator
     * without a separate HUD: dark-orange = closed, light-orange =
     * animating, green = open enough to walk through. */
    for (ty = 0; ty < MAP_H; ty++) {
        for (tx = 0; tx < MAP_W; tx++) {
            tile = map_plane0[ty * MAP_W + tx];
            obj  = map_plane1[ty * MAP_W + tx];
            wc = TileToColor(tile);
            if (tile >= 90 && tile <= 101) {
                BYTE amt = g_door_amt[ty * MAP_W + tx];
                if (amt >= DOOR_BLOCK_AMT) wc = 105;   /* open: green */
                else if (amt > 0)          wc = 178;   /* animating */
                else                       wc = 176;   /* closed */
            }
            oc = ObjectToColor(obj);
            FB_Put(MINIMAP_X0 + tx, MINIMAP_Y0 + ty, oc ? oc : wc);
        }
    }

    /* Player position dot (3x3 cyan), heading line 4 px in cos/sin direction. */
    ppx = MINIMAP_X0 + (int)(g_px >> 8);
    ppy = MINIMAP_Y0 + (int)(g_py >> 8);
    {
        int dx, dy;
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
                FB_Put(ppx + dx, ppy + dy, 14);
    }
    /* Heading: 4 px line. Our angle 0=E (+X), 256=S (+Y). */
    hx = ppx + (int)(((long)COS_Q15(g_pa) * 4L) >> 15);
    hy = ppy + (int)(((long)SIN_Q15(g_pa) * 4L) >> 15);
    FB_Put(hx, hy, 15);
    /* Halfway dot to make the line readable on 1px-per-tile minimap. */
    FB_Put((ppx + hx) / 2, (ppy + hy) / 2, 15);
}

static void DrawCrosshair(void)
{
    int cx = VIEW_X0 + VIEW_W/2;
    int cy = VIEW_Y0 + VIEW_H/2;
    int i;
    for (i = -3; i <= 3; i++) {
        if (i < -1 || i > 1) {
            FB_Put(cx + i, cy, 15);
            FB_Put(cx, cy + i, 15);
        }
    }
}

/* ---- Raycaster ---- */

/* Cast a ray from g_px,g_py at angle ra using grid-line DDA (A.13.1).
 *
 * Each loop iteration steps the ray to the *next tile boundary* on the
 * axis with smaller side_dist, advancing exactly one tile per step. This
 * replaces the A.13/A.14 step-by-fraction (1/16 tile sub-step) DDA, which
 * paid ~16x in iteration count for an approximation that ALSO had texture
 * x errors at sub-pixel level. Grid-line DDA is both faster and exact.
 *
 * Fixed-point: distances are Q8.8 in tile units (one tile = 256). Player
 * pos g_px/g_py is Q8.8. Direction COS_Q15/SIN_Q15 is Q15 in [-32767..32767]
 * (one tile per ray-distance unit at full magnitude).
 *
 *   deltadist = 1/|d|  (distance ray travels per 1-tile step on that axis)
 *   side_dist = distance from origin to next grid line on that axis
 *
 * Returns perpendicular distance in Q8.8 (already partially fish-eye
 * corrected since it's a single-axis projection); out_tex_idx is the
 * WALL_COUNT index or DOOR_TEX_IDX; out_tex_x is 0..63.
 *
 * Door branch fires once per door-tile entry (vs every sub-step in old DDA)
 * and tests slab-plane crossing within the tile bounds, identical math.
 */
static long CastRay(int ra, int *out_tex_idx, int *out_tex_x)
{
    long dx_q15, dy_q15;
    long abs_dx, abs_dy;
    long deltadist_x_q88, deltadist_y_q88;
    long side_dist_x_q88, side_dist_y_q88;
    long perp_dist_q88;
    long frac_x_q8, frac_y_q8;
    long hit_pos_x_q88, hit_pos_y_q88;
    int  step_x, step_y;
    int  tx, ty;
    int  side;
    int  steps;
    int  orient;
    WORD hit_tile;

    *out_tex_idx = 0;
    *out_tex_x   = 0;

    dx_q15 = (long)COS_Q15(ra);
    dy_q15 = (long)SIN_Q15(ra);
    abs_dx = (dx_q15 < 0) ? -dx_q15 : dx_q15;
    abs_dy = (dy_q15 < 0) ? -dy_q15 : dy_q15;

    /* deltadist_q88 = (1<<23)/|d_q15|. abs_d=32767 -> 256 (1.0 tile),
     * abs_d=128 -> 65536 (256 tiles). Cap at LARGE for near-zero ray
     * components (ray almost axis-aligned -> the other axis dominates). */
    if (abs_dx < 32) deltadist_x_q88 = 0x00FFFFFFL;
    else             deltadist_x_q88 = (1L << 23) / abs_dx;
    if (abs_dy < 32) deltadist_y_q88 = 0x00FFFFFFL;
    else             deltadist_y_q88 = (1L << 23) / abs_dy;

    tx = (int)(g_px >> 8);
    ty = (int)(g_py >> 8);
    frac_x_q8 = (long)(g_px & 0xFF);
    frac_y_q8 = (long)(g_py & 0xFF);

    if (dx_q15 < 0) {
        step_x = -1;
        side_dist_x_q88 = (frac_x_q8 * deltadist_x_q88) >> 8;
    } else {
        step_x = 1;
        side_dist_x_q88 = ((256L - frac_x_q8) * deltadist_x_q88) >> 8;
    }
    if (dy_q15 < 0) {
        step_y = -1;
        side_dist_y_q88 = (frac_y_q8 * deltadist_y_q88) >> 8;
    } else {
        step_y = 1;
        side_dist_y_q88 = ((256L - frac_y_q8) * deltadist_y_q88) >> 8;
    }

    for (steps = 0; steps < MAX_CAST_STEPS; steps++) {
        /* Step the axis whose next grid line is closer. perp_dist is
         * captured BEFORE incrementing side_dist on that axis (it's the
         * distance to the grid line we're crossing right now). */
        if (side_dist_x_q88 < side_dist_y_q88) {
            perp_dist_q88     = side_dist_x_q88;
            side_dist_x_q88  += deltadist_x_q88;
            tx               += step_x;
            side              = X_SIDE;
        } else {
            perp_dist_q88     = side_dist_y_q88;
            side_dist_y_q88  += deltadist_y_q88;
            ty               += step_y;
            side              = Y_SIDE;
        }

        if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) break;

        /* Door: test slab plane crossing within this tile.
         *   orient==1 (vertical door): slab perpendicular to X at X=tx+0.5.
         *   orient==2 (horizontal door): slab at Y=ty+0.5.
         * Logic: compute distance along ray to slab axis target, verify
         * (a) slab is forward of origin AND inside this tile bound, (b) at
         * slab the perpendicular coord is still inside the tile, (c) perp
         * fraction is past the open extent (amt). If all three hold -> HIT.
         * Otherwise the ray passes through the door's open portion and the
         * loop continues to next tile. */
        orient = IsDoor(tx, ty);
        if (orient) {
            long slab_axis_target_q88, slab_perp_pos_q88;
            long axis_origin_q88, axis_dir;
            long axis_diff, abs_axis_diff, abs_dir;
            long slab_dist_q88;
            long next_perp_grid_dist;
            long perp_frac_q8, amt_q8;
            int  perp_tile_expected;
            int  amt;

            if (orient == 1) {
                slab_axis_target_q88 = ((long)tx << 8) + 128L;   /* tx + 0.5 */
                axis_origin_q88      = (long)g_px;
                axis_dir             = dx_q15;
                next_perp_grid_dist  = side_dist_y_q88;          /* exit via Y-grid */
                perp_tile_expected   = ty;
            } else {
                slab_axis_target_q88 = ((long)ty << 8) + 128L;
                axis_origin_q88      = (long)g_py;
                axis_dir             = dy_q15;
                next_perp_grid_dist  = side_dist_x_q88;
                perp_tile_expected   = tx;
            }

            if (axis_dir != 0) {
                axis_diff     = slab_axis_target_q88 - axis_origin_q88;
                abs_axis_diff = (axis_diff < 0) ? -axis_diff : axis_diff;
                abs_dir       = (axis_dir < 0) ? -axis_dir : axis_dir;

                /* Slab must be in ray's forward direction. */
                if ((axis_diff > 0 && axis_dir > 0) ||
                    (axis_diff < 0 && axis_dir < 0) ||
                    axis_diff == 0) {
                    slab_dist_q88 = (abs_axis_diff * 32767L) / abs_dir;

                    /* Slab must be reached BEFORE ray exits this tile via
                     * the perpendicular axis. (Exit via the slab axis is
                     * automatic: the slab is at mid-tile so it's always
                     * before the far axis-grid line.) */
                    if (slab_dist_q88 < next_perp_grid_dist) {
                        if (orient == 1) {
                            slab_perp_pos_q88 = (long)g_py +
                                ((slab_dist_q88 * dy_q15) / 32767L);
                        } else {
                            slab_perp_pos_q88 = (long)g_px +
                                ((slab_dist_q88 * dx_q15) / 32767L);
                        }

                        if ((int)(slab_perp_pos_q88 >> 8) == perp_tile_expected) {
                            perp_frac_q8 = slab_perp_pos_q88 & 0xFFL;
                            amt = g_door_amt[ty * MAP_W + tx];
                            amt_q8 = ((long)amt * 256L) / DOOR_AMT_OPEN;
                            if (perp_frac_q8 >= amt_q8) {
                                /* HIT slab. */
                                *out_tex_idx = DOOR_TEX_IDX;
                                *out_tex_x = (int)(perp_frac_q8 >> 2);
                                if (*out_tex_x < 0)  *out_tex_x = 0;
                                if (*out_tex_x > 63) *out_tex_x = 63;
                                if (slab_dist_q88 < 16) slab_dist_q88 = 16;
                                return slab_dist_q88;
                            }
                            /* perp_frac in open portion -> ray passes through. */
                        }
                    }
                }
            }
            /* No slab hit: ray transits door tile, continue DDA. */
            continue;
        }

        if (IsWall(tx, ty)) {
            hit_tile = map_plane0[ty * MAP_W + tx];
            *out_tex_idx = TileToWallTex(hit_tile, side);

            /* Texture x: fractional perp coord at hit point along the wall
             * face. For X-side hit (vertical face), perp coord is Y; for
             * Y-side, X. Compute hit position via projection along ray. */
            if (side == X_SIDE) {
                hit_pos_y_q88 = (long)g_py +
                    ((perp_dist_q88 * dy_q15) / 32767L);
                *out_tex_x = (int)((hit_pos_y_q88 & 0xFFL) >> 2);
                /* Mirror so texture orientation matches face normal. */
                if (dx_q15 > 0) *out_tex_x = 63 - *out_tex_x;
            } else {
                hit_pos_x_q88 = (long)g_px +
                    ((perp_dist_q88 * dx_q15) / 32767L);
                *out_tex_x = (int)((hit_pos_x_q88 & 0xFFL) >> 2);
                if (dy_q15 < 0) *out_tex_x = 63 - *out_tex_x;
            }
            if (*out_tex_x < 0)  *out_tex_x = 0;
            if (*out_tex_x > 63) *out_tex_x = 63;

            if (perp_dist_q88 < 16) perp_dist_q88 = 16;
            return perp_dist_q88;
        }
    }
    /* Ran out of steps or off map — far distance fallback. */
    return (long)(64L << 8);
}

/* Render TWO adjacent columns (col, col+1) with the same wall_h / tex /
 * perp_dist. Called by DrawViewport's half-col cast loop (step=2). The
 * cast cost halves and so does per-col bookkeeping (wall_h_long divide,
 * sy_step divide, texture pointer setup); the only cost that doubles is
 * the per-pixel framebuf write, which becomes a paired write that the
 * compiler/CPU can pipeline. Visual artifact: walls show 2-px horizontal
 * stairsteps, invisible at 128-wide viewport with 64-px texture source.
 *
 * Inner-loop micro-opt:
 *   - dy ranges pre-clipped once per call; no per-pixel bound checks.
 *   - framebuf access via two decrementing far pointers (col, col+1);
 *     avoids per-pixel `fb_y * SCR_W + sx` multiplication.
 *   - sy_src clamp inlined (no neg branch — sy_acc starts >=0).
 */
static void DrawWallStripCol(int col, long perp_dist_q88, int tex_idx, int tex_x)
{
    long wall_h_long;
    int  wall_h, dy_top, dy_bot, sx, dy, sy_src;
    BYTE __far *texcol;
    BYTE __far *fb1;
    BYTE __far *fb2;
    long sy_acc, sy_step;
    int  tex_idx_clamped;
    int  vy0, vy1;
    int  ceil_top, ceil_bot;
    int  wall_top, wall_bot;
    int  floor_top, floor_bot;
    BYTE c;

    if (perp_dist_q88 < 16) perp_dist_q88 = 16;
    /* Z-buffer: depth for both rendered cols (sprite occlusion). */
    if (col     >= 0 && col     < VIEW_W) g_zbuffer[col]     = perp_dist_q88;
    if (col + 1 >= 0 && col + 1 < VIEW_W) g_zbuffer[col + 1] = perp_dist_q88;

    wall_h_long = ((long)VIEW_H << 8) / perp_dist_q88;
    wall_h = (int)wall_h_long;
    if (wall_h < 1)  wall_h = 1;
    if (wall_h > 4 * VIEW_H) wall_h = 4 * VIEW_H;

    dy_top = VIEW_CY - wall_h / 2;
    dy_bot = dy_top + wall_h;

    sx = VIEW_X0 + col;
    if (sx < 0 || sx >= SCR_W) return;
    if (sx + 1 >= SCR_W) return;   /* paired write needs col+1 in screen */

    if (tex_idx == DOOR_TEX_IDX) {
        texcol = (BYTE __far *)door_tex + ((unsigned)tex_x * 64U);
    } else {
        tex_idx_clamped = tex_idx;
        if (tex_idx_clamped < 0)              tex_idx_clamped = 0;
        if (tex_idx_clamped >= WALL_COUNT)    tex_idx_clamped = WALL_COUNT - 1;
        texcol = (BYTE __far *)walls[tex_idx_clamped] + ((unsigned)tex_x * 64U);
    }

    /* Pre-clip dy ranges once. After this each fill loop is bound-check-free. */
    vy0 = VIEW_Y0;
    vy1 = VIEW_Y0 + VIEW_H;
    if (vy0 < 0)     vy0 = 0;
    if (vy1 > SCR_H) vy1 = SCR_H;

    ceil_top  = vy0;
    ceil_bot  = (dy_top  < vy0) ? vy0 : ((dy_top  > vy1) ? vy1 : dy_top);
    wall_top  = ceil_bot;
    wall_bot  = (dy_bot  < vy0) ? vy0 : ((dy_bot  > vy1) ? vy1 : dy_bot);
    floor_top = wall_bot;
    floor_bot = vy1;

    /* sy_acc pre-skip if wall is clipped at top of viewport. */
    sy_step = (64L << 16) / wall_h;
    sy_acc  = (dy_top < wall_top) ? (sy_step * (long)(wall_top - dy_top)) : 0;

    /* Ceiling fill: dy = ceil_top..ceil_bot-1, paired write. */
    fb1 = framebuf + (long)((SCR_H - 1) - ceil_top) * SCR_W + sx;
    fb2 = fb1 + 1;
    for (dy = ceil_top; dy < ceil_bot; dy++) {
        *fb1 = CEIL_COLOR;
        *fb2 = CEIL_COLOR;
        fb1 -= SCR_W;
        fb2 -= SCR_W;
    }

    /* Wall fill: dy = wall_top..wall_bot-1, sample texture, paired write. */
    fb1 = framebuf + (long)((SCR_H - 1) - wall_top) * SCR_W + sx;
    fb2 = fb1 + 1;
    for (dy = wall_top; dy < wall_bot; dy++) {
        sy_src = (int)(sy_acc >> 16);
        if (sy_src > 63) sy_src = 63;
        c = texcol[sy_src];
        *fb1 = c;
        *fb2 = c;
        fb1 -= SCR_W;
        fb2 -= SCR_W;
        sy_acc += sy_step;
    }

    /* Floor fill. */
    fb1 = framebuf + (long)((SCR_H - 1) - floor_top) * SCR_W + sx;
    fb2 = fb1 + 1;
    for (dy = floor_top; dy < floor_bot; dy++) {
        *fb1 = FLOOR_COLOR;
        *fb2 = FLOOR_COLOR;
        fb1 -= SCR_W;
        fb2 -= SCR_W;
    }
}

/* Render one billboard. World position is tile center (Q8.8). Transform
 * to camera space by translating by -player_pos and rotating by -g_pa
 * (so player heading aligns with +cam_y / depth axis). Cull if behind or
 * very near. Project cam_x onto screen via constant focal=96 px. Sprite
 * height in pixels uses the same inverse-depth formula as walls so a
 * 64-tile-unit sprite covers the same vertical range as a wall at the
 * same depth. Per-column z-test against g_zbuffer makes walls occlude. */
static void DrawSpriteWorld(int tile_x, int tile_y, int sprite_idx)
{
    long obj_x_q88, obj_y_q88, rx, ry, cam_x, cam_y;
    int  ca, sa;
    int  screen_x, dest_left, dest_right, dest_col;
    int  sprite_h, dy_top, dy_start, dy_end, dy, srcx, sy_src, fb_y, sx;
    long sprite_h_long;
    BYTE *sprite;
    WORD leftpix, rightpix, col_ofs, starty_src, endy_src, corr_top, src_idx;
    WORD far *dataofs;
    WORD far *post;

    if (sprite_idx < 0 || sprite_idx >= NUM_SPRITES) return;
    if (sprite_len[sprite_idx] == 0) return;

    obj_x_q88 = ((long)tile_x << 8) | 0x80;   /* tile center */
    obj_y_q88 = ((long)tile_y << 8) | 0x80;
    rx = obj_x_q88 - g_px;                     /* Q8.8 */
    ry = obj_y_q88 - g_py;

    ca = COS_Q15(g_pa);
    sa = SIN_Q15(g_pa);

    /* Camera space: forward = +cam_y, right = +cam_x.
     *   cam_y  =  rx*cos(pa) + ry*sin(pa)   [project onto forward dir]
     *   cam_x  = -rx*sin(pa) + ry*cos(pa)   [project onto right dir]
     * Inputs are Q8.8 * Q15 → Q23 → shift 15 → Q8.8. */
    cam_y = ( rx * (long)ca + ry * (long)sa) >> 15;
    if (cam_y < 32) return;                    /* behind player or too close */
    cam_x = (-rx * (long)sa + ry * (long)ca) >> 15;

    /* Screen X projection: pixel offset = cam_x * focal / cam_y. */
    screen_x = VIEW_W/2 + (int)((cam_x * FOCAL_PIXELS) / cam_y);

    /* Sprite height in pixels: same inverse-depth as walls. */
    sprite_h_long = ((long)VIEW_H << 8) / cam_y;
    sprite_h = (int)sprite_h_long;
    if (sprite_h < 1) return;
    if (sprite_h > 4 * VIEW_H) sprite_h = 4 * VIEW_H;

    dest_left  = screen_x - sprite_h / 2;
    dest_right = dest_left + sprite_h;
    if (dest_right <= 0 || dest_left >= VIEW_W) return;

    sprite   = sprites[sprite_idx];
    leftpix  = *(WORD far *)(sprite + 0);
    rightpix = *(WORD far *)(sprite + 2);
    if (leftpix > 63 || rightpix > 63 || leftpix > rightpix) return;
    dataofs  = (WORD far *)(sprite + 4);

    dy_top = VIEW_CY - sprite_h / 2;

    for (dest_col = dest_left; dest_col < dest_right; dest_col++) {
        if (dest_col < 0 || dest_col >= VIEW_W) continue;
        /* Wall occlusion: skip column if a wall is in front. */
        if (g_zbuffer[dest_col] <= cam_y) continue;

        srcx = (int)(((long)(dest_col - dest_left) * 64L) / (long)sprite_h);
        if (srcx < (int)leftpix || srcx > (int)rightpix) continue;
        col_ofs = dataofs[srcx - (int)leftpix];
        if (col_ofs >= SPRITE_MAX) continue;
        post = (WORD far *)(sprite + col_ofs);

        sx = VIEW_X0 + dest_col;
        if (sx < 0 || sx >= SCR_W) continue;

        while (post[0] != 0) {
            endy_src   = (WORD)(post[0] >> 1);
            corr_top   = post[1];
            starty_src = (WORD)(post[2] >> 1);
            if (endy_src > 64 || starty_src > endy_src) break;

            dy_start = dy_top + (int)(((long)starty_src * (long)sprite_h) / 64L);
            dy_end   = dy_top + (int)(((long)endy_src   * (long)sprite_h) / 64L);

            for (dy = dy_start; dy < dy_end; dy++) {
                if (dy < VIEW_Y0 || dy >= VIEW_Y0 + VIEW_H) continue;
                sy_src = (int)(((long)(dy - dy_top) * 64L) / (long)sprite_h);
                if (sy_src < (int)starty_src) sy_src = (int)starty_src;
                if (sy_src >= (int)endy_src)  sy_src = (int)endy_src - 1;
                src_idx = (WORD)(corr_top + (WORD)sy_src);
                if (src_idx >= SPRITE_MAX) continue;
                fb_y = (SCR_H - 1) - dy;
                framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = sprite[src_idx];
            }
            post += 3;
        }
    }
}

/* Painter's order: sort by descending cam_y (depth) so far sprites draw
 * first, near sprites paint over them. Insertion sort, MAX_OBJECTS=64,
 * trivial cost. Uses a side array of (cam_y_q88, obj_idx) pairs so we
 * don't reorder g_objects[] in place — the scan order is stable. */
static void DrawAllSprites(void)
{
    long  depth_q88[MAX_OBJECTS];
    int   order[MAX_OBJECTS];
    int   visible_count = 0;
    int   i, j, k;
    long  obj_x_q88, obj_y_q88, rx, ry, cam_y;
    int   ca, sa;
    long  d_tmp;
    int   o_tmp;

    if (g_num_objects == 0) return;

    ca = COS_Q15(g_pa);
    sa = SIN_Q15(g_pa);

    for (i = 0; i < g_num_objects; i++) {
        obj_x_q88 = ((long)g_objects[i].tile_x << 8) | 0x80;
        obj_y_q88 = ((long)g_objects[i].tile_y << 8) | 0x80;
        rx = obj_x_q88 - g_px;
        ry = obj_y_q88 - g_py;
        cam_y = ( rx * (long)ca + ry * (long)sa) >> 15;
        if (cam_y < 32) continue;          /* cull behind/too-close */
        depth_q88[visible_count] = cam_y;
        order[visible_count]     = i;
        visible_count++;
    }

    /* Insertion sort, descending by depth (back-to-front). */
    for (j = 1; j < visible_count; j++) {
        d_tmp = depth_q88[j];
        o_tmp = order[j];
        k = j - 1;
        while (k >= 0 && depth_q88[k] < d_tmp) {
            depth_q88[k+1] = depth_q88[k];
            order[k+1]     = order[k];
            k--;
        }
        depth_q88[k+1] = d_tmp;
        order[k+1]     = o_tmp;
    }

    for (i = 0; i < visible_count; i++) {
        int idx = order[i];
        int sprite_idx = g_objects[idx].sprite_idx;

        /* Enemy rotation: pick the SPR_GRD_S_n frame that matches the
         * player's view angle relative to the enemy's facing. Static
         * decorations (enemy_dir == OBJ_DIR_NONE) use sprite_idx as-is. */
        if (g_objects[idx].enemy_dir != OBJ_DIR_NONE) {
            long obj_x_q88 = ((long)g_objects[idx].tile_x << 8) | 0x80;
            long obj_y_q88 = ((long)g_objects[idx].tile_y << 8) | 0x80;
            long e2p_dx = g_px - obj_x_q88;        /* Q8.8 enemy-to-player */
            long e2p_dy = g_py - obj_y_q88;
            int  e2p_angle = atan2_q10(e2p_dy, e2p_dx);
            int  facing = dirangle_q10[g_objects[idx].enemy_dir & 7];
            /* Sign of (facing - e2p_angle) chosen so the SPR_GRD_S_n
             * frame indices (CCW around the enemy in vanilla Wolf3D's
             * art layout) match our CW-from-east Q10 angle convention.
             * Reversing this differs by reflection only (S_2/S_8 swap,
             * S_3/S_7 swap, etc.) — verified empirically vs snap 0017. */
            int  rel = (facing - e2p_angle + 1024 + 64) & ANGLE_MASK;
            int  sector = (rel >> 7) & 7;          /* 0..7 of 128 each */
            sprite_idx = GUARD_S_FIRST_SLOT + sector;
        }

        DrawSpriteWorld(g_objects[idx].tile_x,
                        g_objects[idx].tile_y,
                        sprite_idx);
    }
}

static void DrawViewport(void)
{
    int  col;
    int  ra, half_fov_a;
    int  tex_idx, tex_x;
    long dist_q88, perp_dist;

    /* Half-col cast: step=2, render col + col+1 with same data inside
     * DrawWallStripCol (paired writes). Halves cast cost + per-col setup. */
    for (col = 0; col < VIEW_W; col += 2) {
        half_fov_a = ((col - VIEW_W/2) * FOV_ANGLES) / VIEW_W;
        ra = (g_pa + half_fov_a) & ANGLE_MASK;
        dist_q88 = CastRay(ra, &tex_idx, &tex_x);
        /* Fish-eye correction: multiply by cos(half_fov_a). */
        perp_dist = (dist_q88 * (long)fov_correct[col]) >> 15;
        if (perp_dist < 16) perp_dist = 16;
        DrawWallStripCol(col, perp_dist, tex_idx, tex_x);
    }
    /* Sprites after walls: z-buffer is now populated, sprites read it. */
    DrawAllSprites();
}

static void DrawBitGrid(int sx, int sy, int h, WORD val, BYTE lit, BYTE unlit)
{
    int i;
    for (i = 0; i < 16; i++) {
        BYTE c = (val & ((WORD)1 << i)) ? lit : unlit;
        FB_FillRect(sx + i * 18, sy, 16, h, c);
    }
}

static void DrawDebugBar(void)
{
    BYTE hb = (BYTE)((tick_count & 1) ? 42 : 15);
    FB_FillRect(0, 0, SCR_W, 30, 8);
    FB_FillRect(2, 1, 28, 6, hb);
    FB_FillRect(34, 1, 10, 6, has_focus ? 105 : 31);
    FB_FillRect(50, 1, 14, 6, gMapErr == 0 ? 105 : (gMapErr > 0 ? 42 : 15));
    FB_FillRect(68, 1, 14, 6, gVSwapErr == 0 ? 105 : (gVSwapErr > 0 ? 42 : 15));
    FB_FillRect(86, 1, 14, 6, gAudioOn ? 14 : 31);
    FB_FillRect(220, 1, 40, 6, (BYTE)((msg_count & 0x07) ? 42 : 15));
    FB_FillRect(266, 1, 40, 6, (BYTE)((key_count & 0x07) ? 105 : 15));
    DrawBitGrid(16, 10, 7, last_msg_type, 42, 15);
    DrawBitGrid(16, 19, 7, last_key_wparam, 143, 15);

    /* Perf telemetry: 14×6 swatch + 100px horizontal bar. Color
     * encodes ms/frame tier for at-a-glance perf health.
     *   green  (105) : <  33 ms (≥30 FPS effective)
     *   yellow (14)  : 33-66 ms (15-30 FPS)
     *   orange (42)  : 66-200 ms (5-15 FPS)
     *   red    (40)  : > 200 ms (< 5 FPS)
     * Bar: 1 px per 2 ms, capped at 100 px (200 ms = full bar).
     *   - Empty portion: bright white track (color 15) so the boundary
     *     between filled / empty is unambiguous.
     *   - Tick: single-px black mark at x=120+16 (=33 ms / 30 FPS line)
     *     so the eye finds the "good enough" threshold without counting
     *     pixels.
     * Combined with the swatch, the eye reads "color = tier, length =
     * precise value, tick = reference threshold" without a font. */
    {
        BYTE tier_col;
        int  bar_w;
        if      (g_last_render_ms <  33)  tier_col = 105;
        else if (g_last_render_ms <  66)  tier_col =  14;
        else if (g_last_render_ms < 200)  tier_col =  42;
        else                              tier_col =  40;
        bar_w = (int)g_last_render_ms / 2;
        if (bar_w > 100) bar_w = 100;
        FB_FillRect(104, 1, 14, 6, tier_col);
        FB_FillRect(120, 1, 100, 6, 15);              /* white track */
        if (bar_w > 0) FB_FillRect(120, 1, bar_w, 6, tier_col);
        /* Reference tick at 33 ms (16 px from bar start). */
        FB_FillRect(136, 0, 1, 8, 0);
    }
}

/* ---- HUD ---- */

/* 4x6 byte-per-pixel digit font. Each row is 4 columns; values are 0
 * (background) or 1 (foreground). 240 B static const total. Authored
 * to be readable at 1:1 inside the 320x200 framebuf — at ~38 px wide
 * for a 6-digit score, fits comfortably in a 320 px strip alongside
 * the other panels. */
static const BYTE digit_font[10][24] = {
    /* 0 */ {0,1,1,0, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 0,1,1,0},
    /* 1 */ {0,1,1,0, 1,1,1,0, 0,1,1,0, 0,1,1,0, 0,1,1,0, 1,1,1,1},
    /* 2 */ {1,1,1,0, 0,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,0, 1,1,1,1},
    /* 3 */ {1,1,1,0, 0,0,0,1, 0,1,1,0, 0,0,0,1, 0,0,0,1, 1,1,1,0},
    /* 4 */ {0,0,1,0, 0,1,1,0, 1,0,1,0, 1,1,1,1, 0,0,1,0, 0,0,1,0},
    /* 5 */ {1,1,1,1, 1,0,0,0, 1,1,1,0, 0,0,0,1, 0,0,0,1, 1,1,1,0},
    /* 6 */ {0,1,1,1, 1,0,0,0, 1,1,1,0, 1,0,0,1, 1,0,0,1, 0,1,1,0},
    /* 7 */ {1,1,1,1, 0,0,0,1, 0,0,1,0, 0,1,0,0, 0,1,0,0, 0,1,0,0},
    /* 8 */ {0,1,1,0, 1,0,0,1, 0,1,1,0, 1,0,0,1, 1,0,0,1, 0,1,1,0},
    /* 9 */ {0,1,1,0, 1,0,0,1, 1,0,0,1, 0,1,1,1, 0,0,0,1, 1,1,1,0},
};

static void DrawDigit(int x, int y, int d, BYTE fg)
{
    int row, col;
    if (d < 0 || d > 9) return;
    for (row = 0; row < HUD_DIGIT_H; row++) {
        for (col = 0; col < HUD_DIGIT_W; col++) {
            if (digit_font[d][row * HUD_DIGIT_W + col]) {
                FB_Put(x + col, y + row, fg);
            }
        }
    }
}

/* Right-align: print exactly `width` digits at (x, y) with leading
 * zeros if `val` has fewer digits than `width`. Negative values clamp
 * to 0; values exceeding 10^width wrap modulo (no overflow indicator). */
static void DrawNumber(int x, int y, long val, int width, BYTE fg)
{
    int  i, d;
    long v = (val < 0) ? 0 : val;
    for (i = width - 1; i >= 0; i--) {
        d = (int)(v % 10L);
        DrawDigit(x + i * HUD_DIGIT_PITCH, y, d, fg);
        v /= 10L;
    }
}

/* Stylized 24x24 soldier-helmet face placeholder. PoC-only — A.15.1
 * polish path replaces with the real BJ face from VGAGRAPH (chunked
 * Huffman in VGAGRAPH.WL1, FACE1APIC at chunk 113). Drawn from
 * primitives so no bitmap data lives in the EXE. Colors verified
 * against gamepal6: 60=dark brown, 56=peach/skin, 8=dark grey. */
static void DrawFacePlaceholder(int x0, int y0)
{
    /* Helmet body: brown. */
    FB_FillRect(x0,      y0,     24, 24, 60);
    /* Helmet brim (darker top band). */
    FB_FillRect(x0,      y0,     24,  3, 8);
    /* Skin/face area inset. */
    FB_FillRect(x0 + 4,  y0 + 8, 16, 14, 56);  /* peach skin */
    /* Eyes (2 small dark squares). */
    FB_FillRect(x0 + 8,  y0 + 12, 2, 2, 0);
    FB_FillRect(x0 + 14, y0 + 12, 2, 2, 0);
    /* Mouth. */
    FB_FillRect(x0 + 10, y0 + 18, 4, 1, 0);
    /* Helmet outline (1-px frame). */
    FB_FillRect(x0,      y0,      24, 1, 0);
    FB_FillRect(x0,      y0 + 23, 24, 1, 0);
    FB_FillRect(x0,      y0,       1, 24, 0);
    FB_FillRect(x0 + 23, y0,       1, 24, 0);
}

/* Layout (320x37 strip at y=163..199), symmetric around screen
 * center 160 with FACE panel centered there:
 *
 *   x=0..35     LEVEL panel  (36 px, 1-digit value)
 *   x=36..107   SCORE panel  (72 px, 6-digit value)
 *   x=108..143  LIVES panel  (36 px, 1-digit value)
 *   x=144..175  FACE  panel  (32 px, 24x24 face centered at x=148)
 *   x=176..223  HEALTH panel (48 px, 3-digit value)
 *   x=224..271  AMMO  panel  (48 px, 2-digit value)
 *   x=272..319  KEYS  panel  (48 px, 2 key icons)
 *
 * Borders: 1-px line top (y=163), 1-px vertical separators between
 * panels at the panel boundaries. Bottom is screen edge. All values
 * dummied to constants for PoC — A.16+ enemies will introduce real
 * damage/score/ammo dynamics. */
static void DrawHUD(void)
{
    /* Background strip + top border. */
    FB_FillRect(0, HUD_Y0,     SCR_W, HUD_H, HUD_BG);
    FB_FillRect(0, HUD_Y0,     SCR_W, 1,     HUD_BORDER);

    /* Vertical separators between panels. */
    FB_FillRect(HUD_PX_LVL_END,    HUD_Y0, 1, HUD_H, HUD_BORDER);
    FB_FillRect(HUD_PX_SCORE_END,  HUD_Y0, 1, HUD_H, HUD_BORDER);
    FB_FillRect(HUD_PX_LIVES_END,  HUD_Y0, 1, HUD_H, HUD_BORDER);
    FB_FillRect(HUD_PX_FACE_END,   HUD_Y0, 1, HUD_H, HUD_BORDER);
    FB_FillRect(HUD_PX_HEALTH_END, HUD_Y0, 1, HUD_H, HUD_BORDER);
    FB_FillRect(HUD_PX_AMMO_END,   HUD_Y0, 1, HUD_H, HUD_BORDER);

    /* Values. PoC dummies — see header comment. */
    /* LEVEL=1, 1 digit centered in 36-px panel at x=16. */
    DrawNumber(16,  HUD_DIGIT_Y, 1L,    1, HUD_FG);
    /* SCORE=0, 6 digits (30 px) centered in 72-px panel at x=57. */
    DrawNumber(57,  HUD_DIGIT_Y, 0L,    6, HUD_FG);
    /* LIVES=3, 1 digit centered in 36-px panel x=108..143 at x=124. */
    DrawNumber(124, HUD_DIGIT_Y, 3L,    1, HUD_FG);
    /* FACE: 24x24 centered in 32-px panel x=144..175 → x0=148. */
    DrawFacePlaceholder(148, 170);
    /* HEALTH=100, 3 digits (14 px) centered in 48-px panel at x=193. */
    DrawNumber(193, HUD_DIGIT_Y, 100L,  3, HUD_FG);
    /* AMMO=8, 2 digits (9 px) centered in 48-px panel at x=243. */
    DrawNumber(243, HUD_DIGIT_Y, 8L,    2, HUD_FG);
    /* KEYS: 2 small key icons (gold + silver) at x=284, x=300. PoC
     * shows both as "owned" = bright; A.16+ will gate on key pickup.
     * silv=7 (light grey RGB(42,42,42), the (1,1,1) row entry). */
    {
        BYTE gold = 14;     /* yellow */
        BYTE silv = 7;      /* light grey */
        FB_FillRect(284, 174, 8, 12, gold);
        FB_FillRect(286, 178,  4,  4, 0);    /* notch */
        FB_FillRect(300, 174, 8, 12, silv);
        FB_FillRect(302, 178,  4,  4, 0);    /* notch */
    }
}

static void SetupStaticBg(void)
{
    unsigned i;
    ClearFrame();
    /* Debug bar area only — viewport and minimap are drawn dynamically. */
    DrawDebugBar();
    /* HUD: all values dummied so the entire strip is constant per
     * frame. Bake into static_bg below — zero per-frame cost. When
     * A.16+ wires real game state, only the variable values need a
     * per-frame redraw, not the chrome. */
    DrawHUD();
    for (i = 0; i < 64000U; i++) static_bg[i] = framebuf[i];
}

/* Try to move player by (dx, dy) in Q8.8 tile units. Reject if would
 * step into a wall (separately on each axis so we slide along walls). */
static void TryMovePlayer(long dx_q88, long dy_q88)
{
    long nx = g_px + dx_q88;
    long ny = g_py + dy_q88;
    int  tx_check, ty_check;

    /* X axis */
    tx_check = (int)(nx >> 8);
    ty_check = (int)(g_py >> 8);
    if (!IsBlockingForMove(tx_check, ty_check)) g_px = nx;

    /* Y axis */
    tx_check = (int)(g_px >> 8);
    ty_check = (int)(ny >> 8);
    if (!IsBlockingForMove(tx_check, ty_check)) g_py = ny;
}

/* ---- Held-key state (S9 input fix v2) ----
 *
 * Win16 on Modular Windows VIS HC does not auto-repeat WM_KEYDOWN AND
 * does not deliver WM_KEYUP at all. First fix attempt (TTL-based held
 * flags refreshed by WM_KEYDOWN) capped at 4 steps per tap (1 immediate
 * + 3 TTL polls), which is exactly what would happen if WM_KEYUP never
 * fires: nothing refreshes the TTL, so it bottoms out and the flag
 * auto-clears.
 *
 * Fix v2: poll GetAsyncKeyState directly each WM_TIMER. The async
 * keyboard buffer is independent of message-processing state — if the
 * HC driver maintains it correctly across the press/release transition,
 * the polled value tells us "still down" vs "released" without any
 * WM_KEYUP. WM_KEYDOWN is kept as a fast-path for the first step on
 * tap (responsiveness) but no longer drives the held-state tracking. */
#define MOVE_POLL_MS              50
#define DEBUG_BAR_TICKS_INTERVAL  10  /* every 10 polls = 500 ms */
static BYTE g_held_up      = 0;
static BYTE g_held_down    = 0;
static BYTE g_held_left    = 0;
static BYTE g_held_right   = 0;
static WORD g_poll_count   = 0;

/* Apply one tick of held-key movement. Returns TRUE if anything moved
 * or rotated. Forward/back are mutually exclusive (forward wins if
 * both flags somehow set). Same for left/right rotation. Movement and
 * rotation can stack in the same tick (e.g. circle-strafe-by-rotate). */
static BOOL ApplyHeldMovement(void)
{
    long dx_q88, dy_q88;
    BOOL changed = FALSE;

    if (g_held_up) {
        dx_q88 = ((long)COS_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15;
        dy_q88 = ((long)SIN_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15;
        TryMovePlayer(dx_q88, dy_q88);
        changed = TRUE;
    } else if (g_held_down) {
        dx_q88 = -(((long)COS_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15);
        dy_q88 = -(((long)SIN_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15);
        TryMovePlayer(dx_q88, dy_q88);
        changed = TRUE;
    }
    if (g_held_left) {
        g_pa = (g_pa - ROT_STEP) & ANGLE_MASK;
        changed = TRUE;
    } else if (g_held_right) {
        g_pa = (g_pa + ROT_STEP) & ANGLE_MASK;
        changed = TRUE;
    }
    return changed;
}

/* Refresh held flags from the async keyboard buffer. Bit 0x8000 = key
 * is currently down. If the HC driver keeps the buffer in sync with
 * physical state, this is the canonical Win16 way to detect held keys
 * without relying on WM_KEYUP. */
static void PollHeldKeysFromAsync(void)
{
    g_held_up    = (GetAsyncKeyState(VK_HC1_UP)    & 0x8000) ? 1 : 0;
    g_held_down  = (GetAsyncKeyState(VK_HC1_DOWN)  & 0x8000) ? 1 : 0;
    g_held_left  = (GetAsyncKeyState(VK_HC1_LEFT)  & 0x8000) ? 1 : 0;
    g_held_right = (GetAsyncKeyState(VK_HC1_RIGHT) & 0x8000) ? 1 : 0;
}

/* Centralized post-movement redraw — used by both WM_KEYDOWN (for the
 * immediate tap response) and WM_TIMER (for the held-key polling).
 * Sandwiches the actual render with PIT reads so we can display
 * ms/frame in the debug bar (telemetry for the future perf sweep). */
static void InvalidatePlayerView(HWND hWnd)
{
    RECT  dirty;
    WORD  pit_before, pit_after;
    DWORD pit_diff;

    pit_before = ReadPitCounter();
    DrawViewport();
    DrawCrosshair();
    DrawMinimapWithPlayer();
    pit_after = ReadPitCounter();
    /* PIT counter 0 decrements; wraps 0 -> 65535. */
    if (pit_after > pit_before) {
        pit_diff = (DWORD)pit_before + (65536UL - (DWORD)pit_after);
    } else {
        pit_diff = (DWORD)pit_before - (DWORD)pit_after;
    }
    /* 596 PIT cycles per ms (PIT @ 596.4 kHz on MAME-VIS). Cap at
     * 0xFFFF so a runaway diff doesn't garble the bit grid. */
    {
        DWORD ms = pit_diff / 596UL;
        if (ms > 0xFFFFUL) ms = 0xFFFFUL;
        g_last_render_ms = (WORD)ms;
    }

    dirty.left   = VIEW_X0;
    dirty.top    = VIEW_Y0;
    dirty.right  = MINIMAP_X0 + MAP_W;
    dirty.bottom = VIEW_Y0 + VIEW_H;
    InvalidateRect(hWnd, &dirty, FALSE);
    g_px_prev = g_px;
    g_py_prev = g_py;
    g_pa_prev = g_pa;
}

long FAR PASCAL _export WolfVisWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    switch (msg) {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        if (gPal) {
            SelectPalette(hdc, gPal, FALSE);
            if (!gPaletteRealized) { RealizePalette(hdc); gPaletteRealized = TRUE; }
        }
        /* Partial-src blit: GDI on MAME-VIS scans the FULL src DIB even
         * when dest is clipped, so a 320x200 src is paid every WM_PAINT.
         * Restrict src to ps.rcPaint to cut blit cost. Bottom-up DIB
         * (biHeight>0) means src y origin is at the BOTTOM of the DIB;
         * to mirror dst (dy, dh) (in top-down screen coords) into the
         * src rect, srcY = SCR_H - dy - dh. See
         * reference_stretchdibits_partial_src_gotcha.md for the formula
         * that A.9 first build got wrong (causing cursor scia). With
         * the right mirror this is safe AND faster. */
        {
            int dx = ps.rcPaint.left;
            int dy = ps.rcPaint.top;
            int dw = ps.rcPaint.right - ps.rcPaint.left;
            int dh = ps.rcPaint.bottom - ps.rcPaint.top;
            if (dw > 0 && dh > 0) {
                int sy = SCR_H - dy - dh;
                StretchDIBits(hdc, dx, dy, dw, dh,
                                  dx, sy, dw, dh,
                              framebuf, (BITMAPINFO FAR *)&bmiPal,
                              DIB_PAL_COLORS, SRCCOPY);
            }
        }
        EndPaint(hWnd, &ps);
        return 0;

    case WM_SETCURSOR:
        /* Suppress VIS native cursor over our client area. The arrow
         * the player would otherwise see blinking through our framebuf
         * is the system cursor MW renders for the hand controller. */
        SetCursor(NULL);
        return TRUE;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        long dx_q88, dy_q88;
        BOOL moved = FALSE, rotated = FALSE;

        last_key_wparam = (WORD)wp;
        last_msg_type   = (WORD)msg;
        key_count++;
        msg_count++;
        switch (wp) {
        case VK_HC1_UP:
            /* Forward along heading. dx = cos(g_pa) * MOVE_STEP, dy = sin.
             * Tap-fast-path: apply one immediate step on the WM_KEYDOWN
             * edge so first tap is reactive without waiting up to one
             * poll cycle. Subsequent steps come from the async poll. */
            dx_q88 = ((long)COS_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15;
            dy_q88 = ((long)SIN_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15;
            TryMovePlayer(dx_q88, dy_q88);
            moved = TRUE;
            break;
        case VK_HC1_DOWN:
            dx_q88 = -(((long)COS_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15);
            dy_q88 = -(((long)SIN_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15);
            TryMovePlayer(dx_q88, dy_q88);
            moved = TRUE;
            break;
        case VK_HC1_LEFT:
            g_pa = (g_pa - ROT_STEP) & ANGLE_MASK;
            rotated = TRUE;
            break;
        case VK_HC1_RIGHT:
            g_pa = (g_pa + ROT_STEP) & ANGLE_MASK;
            rotated = TRUE;
            break;
        case VK_HC1_PRIMARY:
            /* A.14.1: PRIMARY is now door toggle. The OPL click that
             * lived here through A.8..A.14 was a sanity check; doors
             * are the better use of the button now that we have walls
             * in front of us begging to be opened. */
            if (ToggleDoorInFront()) moved = TRUE;
            break;
        case VK_HC1_SECONDARY:
            OplNoteOn(0x244, 4);
            OplNoteOff();
            break;
        case VK_HC1_F1:
            if (!sqActive && gMusicLoadErr == 0) {
                OplInit();
                StartMusic();
            }
            break;
        case VK_HC1_F3:
            StopMusic();
            break;
        default: break;
        }

        if (moved || rotated) InvalidatePlayerView(hWnd);
        return 0;
    }

    case WM_TIMER: {
        RECT  dirty;
        POINT pt;
        BOOL  moved;

        g_poll_count++;
        /* Pump HC cursor pos every poll — keeps MW HC dispatcher
         * routing keys (A.8 gotcha pattern). */
        pt.x = 0; pt.y = 0;
        hcGetCursorPos((LPPOINT)&pt);

        /* Async-poll the keyboard for held d-pad state, then apply
         * one tick of movement per held key. Async poll is the canonical
         * Win16 substitute for WM_KEYUP-driven release tracking. */
        PollHeldKeysFromAsync();
        moved = ApplyHeldMovement();
        /* Advance any door animations one step. Either a held-key
         * movement OR a door change requires a viewport redraw. */
        if (AdvanceDoors()) moved = TRUE;
        if (moved) InvalidatePlayerView(hWnd);

        /* Debug bar update at lower cadence to avoid eating CPU on a
         * 50 ms timer (the bar barely needs to refresh — just heartbeat
         * + status indicators). Ticks every 500 ms as in A.13. */
        if ((g_poll_count % DEBUG_BAR_TICKS_INTERVAL) == 0) {
            tick_count++;
            has_focus = (GetFocus() == hWnd);
            DrawDebugBar();
            dirty.left   = 0;
            dirty.top    = 0;
            dirty.right  = SCR_W;
            dirty.bottom = DEBUG_BAR_H;
            InvalidateRect(hWnd, &dirty, FALSE);
        }
        return 0;
    }

    case WM_SETFOCUS:   has_focus = TRUE;  return 0;
    case WM_KILLFOCUS:  has_focus = FALSE; return 0;

    case WM_QUERYNEWPALETTE:
        gPaletteRealized = FALSE;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;

    case WM_PALETTECHANGED:
        if ((HWND)wp != hWnd) {
            gPaletteRealized = FALSE;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_CREATE: {
        HDC dc = GetDC(hWnd);
        if (gPal) { SelectPalette(dc, gPal, FALSE); RealizePalette(dc); }
        ReleaseDC(hWnd, dc);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND     hWnd;
    MSG      msg;
    (void)cmd;

    InitPalette();
    BuildPalette();
    InitTrig();

    gLoadErr = LoadMapHead();
    if (gLoadErr == 0) gMapErr = LoadMap(0);
    else               gMapErr = 100 + gLoadErr;

    gVSwapErr = LoadVSwap();

    gAudioHdrErr = LoadAudioHeader();
    if (gAudioHdrErr == 0) {
        gMusicLoadErr = LoadMusicChunk(MUSIC_SMOKE_CHUNK);
        if (gMusicLoadErr == 0) OplInit();
    }

    InitPlayer();
    ScanObjects();

    SetupStaticBg();
    /* Initial render of the dynamic layers so the first WM_PAINT shows
     * the viewport + minimap, not a blank panel. */
    DrawViewport();
    DrawCrosshair();
    DrawMinimapWithPlayer();

    if (!hPrev) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WolfVisWndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        /* Cursor suppression point #1: no class default cursor. */
        wc.hCursor       = NULL;
        wc.hbrBackground = GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = "WolfVISa15";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa15", "WolfVISa15",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    /* Cursor suppression point #3: backstop ShowCursor counter. */
    ShowCursor(FALSE);

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    /* 50 ms = 20 Hz — fast enough for held-key movement to feel
     * continuous. Debug bar throttles itself to 1 Hz inside WM_TIMER. */
    SetTimer(hWnd, 1, MOVE_POLL_MS, NULL);

    for (;;) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (sqActive) ServiceMusic();
        } else if (sqActive) {
            ServiceMusic();
        } else {
            WaitMessage();
        }
    }
}
